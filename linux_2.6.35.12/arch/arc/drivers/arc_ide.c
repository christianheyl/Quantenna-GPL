/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Rajeshwarr: Apr 2009
 *  -Removed the ARC specific code from generic ide layer
 *  -Support to build as a module
 *
 * TODO:
 *  Home grown/ugly Buffer bouncing code (shd use block layer / dma API bounce)
 *
 * Vineetg: Jan 2009
 *  -Removed explicit Cache Sync as that is done transparently by the
 *   New DMA Mapping API
 *  -Direct ref to sg_xx elements replaced with accessors
 *  -Removed globals to store bouncing info.
 *
 * Rajeshwarr: Dec 2008
 *  -Tons of cleanup in moving from 2.6.19 to 2.6.26
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ide.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>


/* drive's registers map in controller base address*/
#define DRIVE_REGISTER_OFFSET 0xA0
#define DRIVE_ALTSTAT_OFFSET 0xBC
#define DRIVE_STATCTRL_OFFSET 0xD8
#define DRIVE_STATCTRL_REL_OFFSET (DRIVE_STATCTRL_OFFSET - DRIVE_REGISTER_OFFSET)

/* statctrl bits */
#define IDE_STATCTRL_IS 0x20
#define IDE_STATCTRL_RS 4
#define IDE_STATCTRL_IC 2
#define IDE_STATCTRL_IE 1

/* DMA physical address reg */
#define IDE_PAR_PAGE(pageno) (pageno & 0x3ffff)

/* DMA command reg bits */
#define IDE_DCR_WRITE 2
#define IDE_DCR_READ ~IDE_DCR_WRITE
#define IDE_DCR_ENABLE 1
#define IDE_DMA_SIZE(size) ((size & 0xffff) << 8)

/* DMA Status reg */
#define IDE_DSR_ERROR     4
#define IDE_DSR_BUSY      2
#define IDE_DSR_COMPLETED 1

/* Two drive status bita */
#define IDE_DRIVE_STAT_BSY  0x80
#define IDE_DRIVE_STAT_DRDY 0x40

/* First version supporting DMA modes */
#define ARC_IDE_FIRST_DMA_VERSION 2

/* Revision is 3rd byte in ID register */
#define IDE_REV(x)  (((x) & 0xff0000) >> 15)

/* ARC IDE Controller */
typedef volatile struct {

    unsigned long ID;
    unsigned long timing;
    unsigned long statctrl;

    // Version 2 registers
    unsigned long dma_timing;
    unsigned long dma_address;
    unsigned long dma_command;
    unsigned long dma_status;
} ARC_IDE_if;

#ifdef DEBUG
#define DBG(fmt, args...) printk(KERN_INFO fmt , ## args)
#else
#define DBG(fmt, args...)
#endif

#ifdef CONFIG_ARC_BLK_DEV_IDEDMA
static void * bounce_buffer;
static int timeout_count;
#endif

/* ARC IDE controller supports only one sg entry */
#define ARC_IDE_NUM_SG  1

/* IDE DMA commands take buffer "page-number" and not pointer.
 * e.g. For a kernel buf @0x80A0_0000, dma map API gives bus addr 0x00A0_0000
 *      while IDE wants a 0x8000_0000 based pg-num, i.e. 0x80A0_0000 / 0x2000
 * Hence these conversion macros
 *
 * It has a further quirk that it only works with 8k pages.
 * In 4k page environement
 *      -We ensure that all DMA buffers are 8k aligned
 *      -the page-nos are also 8k based.
 */
#define BUS_ADDR_TO_CTRL(addr)    ((addr | PAGE_OFFSET) >> 13)
#define CTRL_TO_BUS_ADDR(addr)    ((addr << 13) - PAGE_OFFSET)

/* Convert controller addr back to kernel addr (not dma addr) */
#define CTRL_TO_KERNEL(addr)    ((addr) << 13)


/* Timing parameters. These are the figures taken from Joe's
 * document "Aurora IDE Interface Implementation Specification".
 * This is arranged as an array of 7 parameters for each of the
 * 5 modes that we support. The individual timing parameters are
 * (in turn):
 *  - T0    Minimum PIO cycle time
 *  - T1    Minimum addr setup before nDIOR/W
 *  - T2    Minimum nDIOR/W pulse width
 *  - T2L   Minimum nDIOR/W recovery time (modes 3&4 only)
 *  - T3    Minimum write data setup time
 *  - T4    Minimum write data hold time
 *  - T5    Minimum read data setup time
 *  - T6    Minimum read data hold data
 *  All figures are in nanoseconds.
 */

static unsigned long timings[5][8] = {
    { 600, 70, 165,  0, 60, 30, 50, 5 },
    { 383, 50, 125,  0, 45, 20, 35, 5 },
    { 240, 30, 100,  0, 30, 15, 20, 5 },
    { 180, 30,  80, 70, 30, 10, 20, 5 },
    { 120, 25,  70, 25, 20, 10, 20, 5 }
  //{ 120, 25, 100, 25, 20, 10, 20, 5 }
};

/* We're actually only interested in these. Why did I just type the rest in ? */
#define TIMING_T0 0
#define TIMING_T1 1
#define TIMING_T2 2
#define TIMING_T2L 3


/* Multi-word DMA timings
 * We rely on timing values that aren't in ide-timing.h
 */
static unsigned long dma_timings[3][5] = {
  { 480, 215,  50,  215,  50 },
  { 150,  50,  50,   80,  30 },
  { 120,  25,  25,   70,  25 }
};

#define DMA_TIMING_T0  0
#define DMA_TIMING_TKW 1
#define DMA_TIMING_TKR 2
#define DMA_TIMING_TD  3
#define DMA_TIMING_TM  4

/* Convert nanoseconds to clock cycles */
#define cycles_per_usec  (CONFIG_ARC700_CLK/1000000)

static __inline__ unsigned long ns_to_cycles(unsigned long ns)
{
    return ns * cycles_per_usec / 1000;
}

#define TIMING_REG_FORMAT(t1,t2,t2l)  \
                 ((((t2l) & 0xff) << 16) | (((t2) & 0xff) << 8) | ((t1) & 0xff))

/* IDE Controller */
static volatile ARC_IDE_if *controller =
                                (volatile ARC_IDE_if *)IDE_CONTROLLER_BASE;


/* PIO operations */

/* routine to tune PIO mode for drives */
static void arc_ide_set_pio_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
    unsigned long t0,t1,t2,t2l;
    const u8 pio_mode = drive->pio_mode - XFER_PIO_0;

    DBG("%s:\n",__FUNCTION__);
    DBG("drive %s, mode %d\n", drive->name,pio_mode);

    if (pio_mode > 4) {
        printk(KERN_ERR "%s: incorrect pio_mode\n", __FUNCTION__);
        return;
    }

    t0 = ns_to_cycles(timings[pio_mode][TIMING_T0]);
    t1 = ns_to_cycles(timings[pio_mode][TIMING_T1]);
    t2 = ns_to_cycles(timings[pio_mode][TIMING_T2]);
    t2l = ns_to_cycles(timings[pio_mode][TIMING_T2L]);

    DBG("t0 is %lu, t1 is %lu, t2 is %lu, t2l is %lu\n",t0,t1,t2,t2l);

    t0++; // Lose truncation errors
    while ((t1+t2+t2l) < t0) {
        DBG("Incrementing to match basic timing requirements\n");
        t2++;
        if ((t1+t2+t2l) < t0) t2l++;
        if ((t1+t2+t2l) < t0) t1++;
    }

    controller->timing = TIMING_REG_FORMAT(t1,t2,t2l);
    DBG("timing reg is set 0x%lx\n",controller->timing);

    return;
}


/* routine to reset controller after a disk reset */
static void arc_ide_reset_controller(ide_drive_t *drive)
{
    int i;
    unsigned char creg;

    DBG("%s\n", __FUNCTION__);

    controller->statctrl = IDE_STATCTRL_RS;
    udelay(250);
    controller->statctrl = 0;
    udelay(250);
    controller->statctrl = IDE_STATCTRL_RS;
    udelay(250);

    DBG("IDE alt status reg - 0x%02x statctrl - 0x%02x\n",
       *((volatile unsigned char*)(IDE_CONTROLLER_BASE+DRIVE_ALTSTAT_OFFSET)),
       *((volatile unsigned char*)(IDE_CONTROLLER_BASE+DRIVE_STATCTRL_OFFSET)));

    for (i = 0; i < 100000; i++) {
        creg =
        *((volatile unsigned char*)(IDE_CONTROLLER_BASE+DRIVE_ALTSTAT_OFFSET));

        if ((! (creg & IDE_DRIVE_STAT_BSY)) && (creg & IDE_DRIVE_STAT_DRDY))
            break;

        udelay(100);
    }
}

/* Sets the interrupt mask bit at the IDE Controller
 * Interrupt mask bit at the processor is set by api enable_irq
 */

int arc_ide_enable_irq(void)
{
    DBG("%s:\n",__FUNCTION__);
    controller->statctrl |= IDE_STATCTRL_IE;
    return 0;
}


int arc_ide_ack_irq(ide_hwif_t *hwif)
{
    DBG("%s:\n",__FUNCTION__);
    controller->statctrl |= IDE_STATCTRL_IC;
    return 1;
}

/* DMA mode operations */
void arc_ide_set_dma_mode(ide_hwif_t *hwif, ide_drive_t * drive)
{
    unsigned long t0,tkw,tkr,td,tm;
    int dma_mode;
    const u8 speed = drive->dma_mode;

    DBG("%s:\n", __FUNCTION__);

    switch(speed)
    {
        case XFER_MW_DMA_0:
            dma_mode = 0;
            break;

        case XFER_MW_DMA_1:
            dma_mode = 1;
            break;

        case XFER_MW_DMA_2:
            dma_mode = 2;
            break;

        default:
            dma_mode = 2;
            break;

    }

    DBG("mode: %d\n",dma_mode);

    t0 = ns_to_cycles(dma_timings[dma_mode][DMA_TIMING_T0]);
    tkw = ns_to_cycles(dma_timings[dma_mode][DMA_TIMING_TKW]);
    tkr = ns_to_cycles(dma_timings[dma_mode][DMA_TIMING_TKR]);
    td = ns_to_cycles(dma_timings[dma_mode][DMA_TIMING_TD]);
    tm = ns_to_cycles(dma_timings[dma_mode][DMA_TIMING_TM]);

    DBG("t0 is %lu, tkw is %lu, tkr is %lu, td is %lu, tm is %lu\n",
                                                            t0,tkw,tkr,td,tm);

    t0++; // Lose truncation errors

    while ((tkw + td) < t0) {
        DBG("Incrementing tkw++ to match basic write timing requirements\n");
        tkw++;
    }

    while ((tkr + td) < t0) {
        DBG("Incrementing tkr to match basic write timing requirements\n");
        tkr++;
    }

    controller->dma_timing = ((tkw & 0xff) << 24) | ((tkr & 0xff) << 16) |
                                  ((td & 0xff) << 8) | (tm & 0xff);

    DBG("DMA timing reg is now 0x%lx\n", controller->dma_timing);
}


#ifdef CONFIG_ARC_BLK_DEV_IDEDMA

/*    dma_timer_expiry    -    handle a DMA timeout
 *    @drive: Drive that timed out
 *
 *    An IDE DMA transfer timed out. In the event of an error we ask
 *    the driver to resolve the problem, if a DMA transfer is still
 *    in progress we continue to wait (arguably we need to add a
 *    secondary 'I dont care what the drive thinks' timeout here)
 *    Finally if we have an interrupt we let it complete the I/O.
 *    But only one time - we clear expiry and if it's still not
 *    completed after WAIT_CMD, we error and retry in PIO.
 *    This can occur if an interrupt is lost or due to hang or bugs.
 */

static int arc_ide_dma_timer_expiry(ide_drive_t *drive)
{
    unsigned long dma_stat = controller->dma_status;

    DBG("%s:\n", __FUNCTION__);

    DBG("%s: DSR 0x%08lx DCR 0x%08lx PAR 0x%08lx drive at 0x%p, name %s\n",
              drive->name, dma_stat, controller->dma_command,
              CTRL_TO_KERNEL(controller->dma_address), drive , drive->name);

    timeout_count++;

    /* BUSY Stupid Early Timer !! */
    if ((dma_stat & IDE_DSR_BUSY) && (timeout_count < 2))    {
        DBG("reset the timer dma_stat 0x%x, timeout_count %d\n", dma_stat, timeout_count);
        return WAIT_CMD;
    }

    do {
        controller->dma_command = 0;
        udelay(10);
        dma_stat = controller->dma_status;
    } while (dma_stat & IDE_DSR_BUSY);

    if (dma_stat & IDE_DSR_ERROR)  /* ERROR */
        return -1;

    return 0;    /* Unknown status -- reset the bus */
}

/* returns 1 if dma irq issued, 0 otherwise */
int arc_ide_dma_test_irq(ide_drive_t *drive)
{
    DBG("%s:\n", __FUNCTION__);

    DBG("statctrl %ld\n", controller->statctrl);

    /* return 1 if IRQ asserted */
    if (controller->statctrl & IDE_STATCTRL_IS)
        return 1;

    return 0;
}

void arc_ide_dma_host_off(ide_drive_t *drive)
{
    DBG("%s:\n", __FUNCTION__);

    controller->dma_command = 0;
}


void arc_ide_dma_host_on(ide_drive_t *drive)
{
    DBG("%s:\n", __FUNCTION__);

    if (drive->dma)
        controller->dma_command |= IDE_DCR_ENABLE;
}

void arc_ide_dma_host_set(ide_drive_t *drive, int on)
{
    if(on)
        arc_ide_dma_host_on(drive);
    else
        arc_ide_dma_host_off(drive);
}


static void arc_ide_dma_start(ide_drive_t *drive)
{
    DBG("%s:\n", __FUNCTION__);

    /* start DMA */
    controller->dma_command |= IDE_DCR_ENABLE;
    timeout_count = 0;
}


/*
 * Teardown mappings after DMA has completed.
 * returns 1 on error, 0 otherwise
 */
static int arc_ide_dma_end(ide_drive_t *drive)
{
    unsigned long dma_stat = 0;
    struct scatterlist *sg = drive->hwif->sg_table;

    DBG("DMA End %x %d\n", sg_virt(sg), sg_dma_len(sg));

    controller->dma_command = 0;        /* stop DMA */
    dma_stat = controller->dma_status;  /* get DMA status */

#ifdef DEBUG
    if (dma_stat & IDE_DSR_ERROR)
        printk("%s : DMA error DSR=0x%08lx", __FUNCTION__, dma_stat);
#endif

    timeout_count = 0;

    /* If driver bounced the DMA off a temp buffer,
     * need to move the contents back to orig block layer buffer
     * NOTE: This only needs to be done for DMA FROM DEVICE case
     *
     * To determine that this is a "bounced dma" instance, there are 3 ways
     *  (1) compare
     *      -addr of buf used for actual dma (controller->dma_address) with
     *      -addr of buf passed by block layer (sg_virt(sg))
     *
     *  (2) Check if orig buffer passed by block layer sg_virt(sg) was aligned
     *      in first place. If not implies bouncing.
     *
     * However both of above use sg_virt ( ) whcih is a bit heavy as it makes
     * a function call and also few branches to get to pointed page.
     * So defer its usage until abs must and instead use less costly measure
     * such as sg_dma_address which is a simple ptr access. And use the fact
     * that if orig buffer was not page aligned, it's dma addr (not contr addr)
     * won't either and hence would have required bouncing
     */

    if (sg_dma_address(sg) % 8192) {

        if (rq_data_dir(drive->hwif->rq) == READ) {

            DBG("Finalize bouncing FROM DEV: src %x Dst %x, sz %d\n",
                  bounce_buffer, sg_virt(sg), sg_dma_len(sg));

            memcpy(sg_virt(sg), bounce_buffer, sg_dma_len(sg));
        }

        /* Since this is bounced dma finalisation, to be good linux citizens
         * we need to call dma_unmap_single(bounce_buffer)
         * But we don't because
         * (i) It is a NOP on ARC
         * (ii) calling it causes a unncessary volatile read
         */
//        dma_unmap_single(HWIF(drive)->dev,
//                        CTRL_TO_BUS_ADDR(controller->dma_address),
//                        sg_dma_len(sg), DMA_FROM_DEVICE);
    }

    /* verify good DMA status */
    return (dma_stat & IDE_DSR_ERROR) ? 1 : 0;
}


/* Check if bounce buffer is large enough for request on hand and
 * try to grow it if need be
 */

static unsigned long grow_bounce_buffer(unsigned long size)
{
    /* allocated sz could be more than the sz requested */
    static unsigned long alloc_buf_sz;
    static void *alloc_buf;

    if (size > alloc_buf_sz) {
        free_pages((unsigned long)alloc_buf, get_order(alloc_buf_sz));

        /* IDE DMA wants 8k aligned bufs, which poses issue with 4k MMU pages.
         * So we allocate an extra page and align the buffer
         * The +1, originally for for gaurd page(s), helps ensure that the driver can
         * align 4k pages based buffer allocations to 8k.
         */
        alloc_buf = (void *)__get_dma_pages(GFP_ATOMIC,get_order(size) + 1);
        if (alloc_buf) {
            alloc_buf_sz = (get_order(size) + 1) * PAGE_SIZE;
            bounce_buffer = (void *)((unsigned int)(alloc_buf) & ~8191);
            DBG("DMA bounce buffer alloc [0x%lx] usable [0x%lx]\n",
                            alloc_buf, bounce_buffer);
        }
        else {
            alloc_buf_sz = 0;
            printk("DMA bounce buffer cannot grow to size of %lu\n", size);
        }
    }

    return (unsigned long) bounce_buffer;
}


static int arc_ide_setup_dma_from_dev(ide_drive_t *drive)
{
    ide_hwif_t *hwif    = drive->hwif;
    struct scatterlist *sg = hwif->sg_table;
    dma_addr_t real_dma_buf = sg_dma_address(sg);

    DBG("<- DMA FROM %x %d\n", sg_virt(sg), sg_dma_len(sg));

    if (real_dma_buf % 8192) {  // ARC IDE doesn't like non page aligned (8k) buf

        DBG("%s, non page-aligned FROM DEV\n", __FUNCTION__);

        if (grow_bounce_buffer(sg_dma_len(sg)) == 0) {
            return 1; // No bounce buffer, go to PIO
        }

        /* DMA into bounce buffer instead of block layer buffer
         * DMA Map this buffer so that proper cache synch can be done
         */
        real_dma_buf = dma_map_single(hwif->dev, bounce_buffer,
                                            sg_dma_len(sg),
                                            DMA_FROM_DEVICE);
    }

    controller->dma_address = BUS_ADDR_TO_CTRL(real_dma_buf);
    controller->dma_command = ((sg_dma_len(sg) >> 9) << 8);
                                  // 9 is the sector size shift (512B)

    return 0;
}

static int arc_ide_setup_dma_to_dev(ide_drive_t *drive)
{
    ide_hwif_t *hwif    = drive->hwif;
    struct scatterlist *sg = hwif->sg_table;
    dma_addr_t real_dma_buf = sg_dma_address(sg);

    DBG("-> DMA TO %x %d\n",sg_virt(sg), sg_dma_len(sg));

    if (real_dma_buf % 8192) {  // ARC IDE doesn't like non page aligned (8k) buf

        DBG("non aligned buffer TO DEV\n");
        if (grow_bounce_buffer(sg_dma_len(sg)) == 0) {
            return 1; // No bounce buffer, go to PIO
        }

        /* Copy block layer buffer into bounce buffer */
        memcpy(bounce_buffer, sg_virt(sg), sg_dma_len(sg));

        /* DMA From bounce buffer, but map it so that cache sync can be done */
        real_dma_buf = dma_map_single(hwif->dev, bounce_buffer,
                                            sg_dma_len(sg),
                                            DMA_TO_DEVICE);
    }

    controller->dma_address = BUS_ADDR_TO_CTRL(real_dma_buf);
    controller->dma_command = ((sg_dma_len(sg) >> 9) << 8) | IDE_DCR_WRITE;

    return 0;
}


static int arc_ide_dma_setup(ide_drive_t *drive, struct ide_cmd *cmd)
{
    struct request *rq = drive->hwif->rq;
    int result=1;    /* False */

    DBG("%s:\n",__FUNCTION__);

    if (rq_data_dir(rq) == WRITE)
        result = arc_ide_setup_dma_to_dev(drive);
    else
        result = arc_ide_setup_dma_from_dev(drive);

    return result;
}

int arc_ide_dma_init(ide_hwif_t *hwif, const struct ide_port_info *d)
{
    DBG("%s:\n", __FUNCTION__);

    // Check controller version and decide whether or not to enable DMA modes.
    if ( IDE_REV(controller->ID) >= ARC_IDE_FIRST_DMA_VERSION)
    {
        printk("Enabling DMA...\n");

        if((grow_bounce_buffer(PAGE_SIZE)) == 0)
            return -1;

        hwif->dmatable_cpu = dma_alloc_coherent(hwif->dev,
                                                PRD_ENTRIES * PRD_BYTES,
                                                & hwif->dmatable_dma,
                                                GFP_KERNEL);

        hwif->sg_table = kmalloc(sizeof(struct scatterlist) * ARC_IDE_NUM_SG,
                    GFP_KERNEL);

        hwif->sg_max_nents = ARC_IDE_NUM_SG;
    }

    return 0;
}

/*  Hooking arc_ide_ack_irq to the hwif->ack_intr will work but results in two
    ISR invocations for each hardware interrupt. IDE Controller Interrupt status
    is raised until the status is read on the drive. In the generic ide_intr, it
    invokes hwif->ack_intr in the beginning which will not clear the interrupt
    because the status on the drive is read later resulting another ISR call. To
    avoid this we are overwriting the generic ide_read_status.
*/

static const struct ide_dma_ops arc_ide_dma_ops = {
    .dma_host_set     = arc_ide_dma_host_set,
    .dma_setup        = arc_ide_dma_setup,
    .dma_start        = arc_ide_dma_start,
    .dma_end          = arc_ide_dma_end,
    .dma_test_irq     = arc_ide_dma_test_irq,
    .dma_timer_expiry = arc_ide_dma_timer_expiry,
};
#endif

static const struct ide_port_ops arc_ide_port_ops =
{
    .set_pio_mode   = arc_ide_set_pio_mode,
    .set_dma_mode   = arc_ide_set_dma_mode,
};


u8 arc_ide_read_status(struct hwif_s *hwif)
{
    u8 ret = ide_read_status(hwif);
    arc_ide_ack_irq(NULL);
    return ret;
}

const struct ide_tp_ops arc_ide_tp_ops = {
    .exec_command = ide_exec_command,
    .read_status = arc_ide_read_status,
    .read_altstatus = ide_read_altstatus,
    .write_devctl = ide_write_devctl,

    .dev_select = ide_dev_select,
    .tf_load = ide_tf_load,
    .tf_read = ide_tf_read,

    .input_data = ide_input_data,
    .output_data = ide_output_data,
};


static struct ide_port_info arc_ide_port_info = {
    .port_ops = &arc_ide_port_ops,
    .tp_ops = &arc_ide_tp_ops,
#ifdef CONFIG_ARC_BLK_DEV_IDEDMA
    .init_dma = arc_ide_dma_init,
    .dma_ops  = &arc_ide_dma_ops,
#endif
    .host_flags = 0,
    .pio_mask = ATA_PIO4,
    .mwdma_mask = ATA_MWDMA2,
    .chipset = ide_unknown
} ;

/* Set up a hw structure for a specified port
 */
static void arc_ide_setup_ports(struct ide_hw *hw, int data_port, int control_port,
                int irq)
{
    int i;

    memset(hw, 0, sizeof(*hw));

    for (i = 0; i < 8; i++)
        hw->io_ports_array[i] = data_port + i*4;

    hw->io_ports.ctl_addr = control_port;
    hw->irq = irq;
}

int __init arc_ide_init(void)
{
    struct ide_hw hw, *hws[] = {&hw};
    struct ide_host *host;
    unsigned int id_word;
    struct ID_REG {
	     unsigned int id:6,reserved:2,
				     nid:6,reserved2:2,
				     rev:8,
				     cfg:8;
    }
    * id_reg;

    DBG("%s:\n", __FUNCTION__);

	/* ID Register:
 	 * --------------------------------------------------------------------
 	 * | 31    	24  | 23      16 | 15 | 14 | 13       8 | 7 | 6 | 5      0|
 	 * | CFG = 0x00 | REV = 0x1  |  1 |  1 | NID = 0x36 | 0 | 0 | ID = 0x9|
 	 * --------------------------------------------------------------------
 	 */

    id_word = controller->ID;
    id_reg = (struct ID_REG *) &id_word;

    printk_init(KERN_INFO "ARC IDE interface driver, Controller ID[%d] REV[%d]\n",
                     id_reg->id, id_reg->rev);

    if ( ! ((id_reg->id == 0x9) && (id_reg->nid == 0x36 )) ) {
        printk_init("***ARC IDE [NOT] detected, skipping IDE init\n");
        return -1;
    }

    // Reset the IDE Controller
    arc_ide_reset_controller(0);

    arc_ide_setup_ports(&hw, IDE_CONTROLLER_BASE + DRIVE_REGISTER_OFFSET,
                   IDE_CONTROLLER_BASE + DRIVE_STATCTRL_OFFSET, IDE_IRQ);

    // Clear the Interrupt
    arc_ide_ack_irq(NULL);

    // Enable the irq at the IDE Controller
    arc_ide_enable_irq();

    ide_host_add(&arc_ide_port_info, hws, 1, &host);

    blk_queue_max_segments(host->ports[0]->devices[0]->queue, ARC_IDE_NUM_SG);

    return 0;
}

void __exit arc_ide_exit(void)
{
    //IDE to reset state
    controller->statctrl = 0;
}

late_initcall(arc_ide_init);    /* Call only after IDE init */
module_exit(arc_ide_exit);

MODULE_AUTHOR("ARC International");
MODULE_DESCRIPTION("ARC IDE interface");
MODULE_LICENSE("GPL");

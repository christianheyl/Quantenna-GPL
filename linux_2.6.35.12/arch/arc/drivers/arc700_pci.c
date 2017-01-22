/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Jan 2009
 *  -This sucker of code revisited probably first time in ages
 *  -Config Space accessors provided by GRPCI need not do local_irq_save( )
 *      etc since PCI Core invokes them under "pci_lock" spin_lock_irqsave( )
 */
/*
 * arc700_pci.c: GRPCI controller support
 *
 * The GRPCI provides two windows in the AMBA address space to which accesses
 * are translated into PCI cycles. One 256 MB big for generating PCI memory
 * cycles and one 64 KB for PCI IO cycles.
 *
 * This file provide low level functions for the generic PCI layer. The entry
 * point is pcic_init which detects the GRPCI core and configures the PCI
 * resources according to the AMBA plug and play information. When the host
 * controller is configured pcic_init calls pci_scan_bus() to detect all devices
 * and pci_assign_unassigned_resources() to allocate and assign all BARs.
 *
 * A 1:1 mapping between PCI addresses and AMBA addresses is provided. The
 * system RAM of the host is mapped at PCI address 0x40000000.
 *
 * Note that the GRPCI host controller does not generate type 1 configuration
 * cycles and thus cannot scan behind PCI<->PCI bridges.
 *
 * To preserve correct byte ordering the host controller should be configured to
 * do byte twisting. If, for some reason, this is not wanted uncomment
 * #define BT_ENABLED 1.
 *
 * The in/out and read/write macros defined in io.h will do the necessary byte
 * twisting for 16 and 32 bit PIO acccesses. The macros __raw_read* and
 * __raw_write* provide non twisting equivalents for memory cycles.
 *
 * The PCI IO area is remapped in pcic_init so the physical BAR values must be
 * passed directly to in/out and the translation from physical to virtual is
 * done in the in/out macros.
 *
 * All PCI interrupt lines are routed to the IRQ of the GRPCI apb slave.
 *
 *   (C) Copyright Gaisler Research, 2006
 *
 * Author Kristoffer Carlsson <kristoffer@gaisler.com>
 * Bits and pieces from MicroSPARC-IIep PCI controller support & MIPS PCI layer.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#include <asm/io.h>

#include <linux/ctype.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>

struct device *tmp_pci_dev=NULL;

//#define DEBUG 1

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

#define TO_KB(x)    (x * 1024)
#define TO_MB(x)    (TO_KB(x) * 1024)

/*********************************************************************
 *  GRPCI Params for mapping Accesses originating on AHB Side,
 *  mapped to PCI [IO | Mem | Cfg ] Transactions
 *********************************************************************/
#define GRPCI_MEM_WINDOW        TO_MB(256)
#define GRPCI_IO_WINDOW         TO_KB(64)

#define AHB_TO_PCI_MEM_START    0xD0000000
#define AHB_TO_PCI_IO_START     (AHB_TO_PCI_MEM_START + GRPCI_MEM_WINDOW)
#define AHB_TO_PCI_CFG_START    (AHB_TO_PCI_IO_START  + GRPCI_IO_WINDOW)

/* Uncomment if host controller byte twisting should be disabled */
//#define BT_ENABLED 1


typedef struct {
    volatile unsigned int cfg_stat;
    volatile unsigned int bar0;
    volatile unsigned int page0;
    volatile unsigned int bar1;
    volatile unsigned int page1;
    volatile unsigned int iomap;
    volatile unsigned int stat_cmd;
} ARC_GRPCI_Regs_Map;


struct arc_pci {

        ARC_GRPCI_Regs_Map      *regs;
        int                     irq;

        struct resource         mem_resource;
        struct resource         io_resource;

        struct pci_bus          *bus;

};

#define ARC_BYPASS_LOAD_PA(x)   (*(unsigned long*)(x))
#define ARC_BYPASS_STORE_PA(x,v) (*(unsigned long*)(x)=(unsigned long)(v))

struct arc_pci pcic0;


/* Generate a Configuration Read (type 0) cycle */
static int pcic_read_config_dword(unsigned int busno, unsigned int devfn,
                                  int where, u32 *value)
{
    struct arc_pci *pcic;


    pcic = &pcic0;

    *value = ARC_BYPASS_LOAD_PA(AHB_TO_PCI_CFG_START +
                                        ((devfn&0xff)<<8) + (where & ~3) );

#define GRPCI_CFG_CYCLE_TIMEOUT 0x100
    if (ARC_BYPASS_LOAD_PA(&pcic->regs->cfg_stat) & GRPCI_CFG_CYCLE_TIMEOUT) {
            *value = 0xFFFFFFFF;
    }

#ifdef BT_ENABLED
    *value = flip_dword(*value);
#endif

    DBG("ARC PCI READ : PCI Side=> [%2x:%2x.%d] %x - GRPCI addr: %x - val: %x\n",
        busno, PCI_SLOT(devfn), PCI_FUNC(devfn), where,
        AHB_TO_PCI_CFG_START + ((devfn&0xff)<<8) + (where & ~3), *value);

    return 0;
}

static int pcic_read_config(struct pci_bus *bus, unsigned int devfn,
                            int where, int size, u32 *val)
{
    unsigned int v;

    if (bus->number != 0) return -EINVAL;
    // For invalid slots return 0 (success) but value itself in invalid 0xff....
    if (PCI_SLOT(devfn) >= 21 || PCI_SLOT(devfn) == 0) {
        return 0;
    }

    switch (size) {
        case 1:
            pcic_read_config_dword(bus->number, devfn, where&~3, &v);
            *val = 0xff & (v >> (8*(where & 3)));
            return 0;
        case 2:
            if (where&1) return -EINVAL;
            pcic_read_config_dword(bus->number, devfn, where&~3, &v);
            *val = 0xffff & (v >> (8*(where & 3)));
            return 0;
        case 4:
            if (where&3) return -EINVAL;
            pcic_read_config_dword(bus->number, devfn, where&~3, val);
            return 0;
    }
    return -EINVAL;
}

/* Generate a Configuration Write (type 0) cycle */
static int pcic_write_config_dword(unsigned int busno, unsigned int devfn,
                                   int where, u32 value)
{
    struct arc_pci *pcic;
    u32 val;

    pcic = &pcic0;

#ifdef BT_ENABLED
    val = flip_dword(value);
#else
    val = value;
#endif

    DBG("ARC PCI WRITE: PCI Side=> [%2x:%2x.%d] %x - GRPCI addr: %x - val: %x\n",
        busno, PCI_SLOT(devfn), PCI_FUNC(devfn), where,
        AHB_TO_PCI_CFG_START + ((devfn&0xff)<<8) + (where & ~3), value);

    ARC_BYPASS_STORE_PA(AHB_TO_PCI_CFG_START + ((devfn&0xff)<<8) +
                                                            (where & ~3), val );


    return 0;
}

static int pcic_write_config(struct pci_bus *bus, unsigned int devfn,
   int where, int size, u32 val)
{
    unsigned int v;

    if (bus->number != 0) return -EINVAL;
    if (PCI_SLOT(devfn) >= 21) return 0;

    switch (size) {
        case 1:
            pcic_read_config_dword(bus->number, devfn, where&~3, &v);
            v = (v & ~(0xff << (8*(where&3)))) | ((0xff&val) << (8*(where&3)));
            return pcic_write_config_dword(bus->number, devfn, where&~3, v);
        case 2:
            if (where&1) return -EINVAL;
            pcic_read_config_dword(bus->number, devfn, where&~3, &v);
            v = (v & ~(0xffff << (8*(where&3)))) |
                                        ((0xffff&val) << (8*(where&3)));
            return pcic_write_config_dword(bus->number, devfn, where&~3, v);
        case 4:
            if (where&3) return -EINVAL;
            return pcic_write_config_dword(bus->number, devfn, where, val);
    }
    return -EINVAL;
}

static struct pci_ops pcic_ops = {
    .read =     pcic_read_config,
    .write =    pcic_write_config,
};

/*
 * Main entry point from the PCI subsystem.
 */
static int __init pcic_init(void)
{
    struct arc_pci *pcic;
    unsigned int data;

    pcic = &pcic0;

    /* configure pci controller */

    pcic->regs = (ARC_GRPCI_Regs_Map *) AHB_PCI_HOST_BRG_BASE;

    pcic->mem_resource.name  = "ARC PCI Memory space";
    pcic->mem_resource.start = AHB_TO_PCI_MEM_START;
    pcic->mem_resource.end   = AHB_TO_PCI_MEM_START + GRPCI_MEM_WINDOW -1; /* 256 MB */
    pcic->mem_resource.flags = IORESOURCE_MEM;

    pcic->io_resource.name  = "ARC PCI IO space";
    pcic->io_resource.start = AHB_TO_PCI_IO_START;
    pcic->io_resource.end   = AHB_TO_PCI_IO_START + GRPCI_IO_WINDOW  -1;  /* 64 KB */
    pcic->io_resource.flags = IORESOURCE_IO;

    pcic->irq = PCI_IRQ;

    printk("Init AHB-PCI Bridge (GRPCI controller)\n");

    /****************** AHB => PCI reachability *********************/

    /* AHB requests starting at this will be translated to PCI Mem access */
    ARC_BYPASS_STORE_PA(&pcic->regs->cfg_stat, AHB_TO_PCI_MEM_START);

    /* AHB requests starting at this will be translated to PCI I/O access
     * This also implicitly defines when PCI Configuration Cycles will be
     * generated. IO_ADDR + 64K
     */
    ARC_BYPASS_STORE_PA(&pcic->regs->iomap, AHB_TO_PCI_IO_START);

    /******************* PCI => AHB reachability *********************/

    /* Setup BAR1: for PCI -> AHB request
     * Addresses 0x8000_0000 onwards from PCI side will be honoured
     * by Bridge and ferried to AHB side
     */
    pcic_write_config_dword(0, 0xA8, PCI_BASE_ADDRESS_1, 0x80000000);

    /* AHB address computed as follows
     *  < upper 8 bits of page1 reg > << lower 24 bits of PCI addr
     * So 0x8AAA_0000 from PCI side will become 0x0AAA_0000 on AHB side
     */
    ARC_BYPASS_STORE_PA(&pcic->regs->page1 , 0x00000000);

    /* We dont user BAR0: still set it to non-zero
     * (grpci considers BAR==0 as invalid) */
    pcic_write_config_dword(0,0xA8,PCI_BASE_ADDRESS_0, 0x10000000);
#if 0
    {
    unsigned int for_page0;
    unsigned int *page0;

    pcic_write_config_dword(0,0xA8,PCI_BASE_ADDRESS_0, 0x80000000);
    pcic_read_config_dword(0, 0xA8, PCI_BASE_ADDRESS_0, &data);
    pcic_write_config_dword(0,0xA8,PCI_BASE_ADDRESS_0, 0xFFFFFFFF);
    pcic_read_config_dword(0, 0xA8, PCI_BASE_ADDRESS_0, &for_page0);
    pcic_write_config_dword(0,0xA8,PCI_BASE_ADDRESS_0, data);

    for_page0 = (~for_page0+1) >> 1;
    page0 = data + for_page0;
    *page0 = 0x0;
    }
#endif

    /* set as bus master and enable pci memory responses */
    pcic_read_config_dword(0, 0xA8, PCI_COMMAND, &data);
    pcic_write_config_dword(0, 0xA8, PCI_COMMAND,
                data | (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));

    if (request_resource(&iomem_resource, &pcic->mem_resource) < 0)
        goto out;
    if (request_resource(&ioport_resource, &pcic->io_resource) < 0)
        goto out_free_mem_resource;

    pcic->bus = pci_scan_bus(0, &pcic_ops, pcic);

    printk("Assigning PCI BARs.\n");
    pci_assign_unassigned_resources();

    return 0;

out_free_mem_resource:
    release_resource(&pcic->mem_resource);
    printk(KERN_WARNING "out_free_mem_resource\n");

out:
    printk(KERN_WARNING "Skipping PCI bus scan due to resource conflict\n");
    return 0;
}


/*
 * Normally called from {do_}pci_scan_bus...
 */
void __devinit pcibios_fixup_bus(struct pci_bus *bus)
{
    struct pci_dev *dev;
    int i, has_io, has_mem;
    unsigned int cmd;
    struct arc_pci *pcic;

    pcic = (struct arc_pci *) bus->sysdata;

    /* Generic PCI bus probing sets these to point at
     * &io{port,mem}_resouce which is wrong for us.
     */
    bus->resource[0] = &pcic->io_resource;
    bus->resource[1] = &pcic->mem_resource;

    if (bus->number != 0) {
        printk("pcibios_fixup_bus: nonzero bus 0x%x\n", bus->number);
        return;
    }

    list_for_each_entry(dev, &bus->devices, bus_list) {

    /*
     * Comment from i386 branch:
     *     There are buggy BIOSes that forget to enable I/O and memory
     *     access to PCI devices. We try to fix this, but we need to
     *     be sure that the BIOS didn't forget to assign an address
     *     to the device. [mj]
     * OBP is a case of such BIOS :-)
     */
     has_io = has_mem = 0;
     for(i=0; i<6; i++) {
        unsigned long f = dev->resource[i].flags;
        if (f & IORESOURCE_IO) {
            has_io = 1;
        } else if (f & IORESOURCE_MEM)
            has_mem = 1;
        }
        pcic_read_config(dev->bus, dev->devfn, PCI_COMMAND, 2, &cmd);
        if (has_io && !(cmd & PCI_COMMAND_IO)) {
            printk("PCIC: Enabling I/O for device [%2x:%2x.%d]\n",
                dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
            cmd |= PCI_COMMAND_IO;
            pcic_write_config(dev->bus, dev->devfn, PCI_COMMAND, 2, cmd);
        }
        if (has_mem && !(cmd & PCI_COMMAND_MEMORY)) {
            printk("PCIC: Enabling memory for device [%2x:%2x.%d]\n",
                dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
            cmd |= PCI_COMMAND_MEMORY;
            pcic_write_config(dev->bus, dev->devfn, PCI_COMMAND, 2, cmd);
        }

        /* All slots routed to one irq.
        * vineetg: We are only updating pci_dev->irq and not setting PCI_INT_LINE
        *           If the driver is using pci_dev->irq to request IRQ then ok
        *           if it tries to read the PCI_INT_LINE it's gonna fail
        */
        dev->irq = pcic->irq;
    }
}

/*
 * We need to avoid collisions with `mirrored' VGA ports
 * and other strange ISA hardware, so we always want the
 * addresses to be allocated in the 0x000-0x0ff region
 * modulo 0x400.
 *
 * Why? Because some silly external IO cards only decode
 * the low 10 bits of the IO address. The 0x00-0xff region
 * is reserved for motherboard devices that decode all 16
 * bits, so it's ok to allocate at, say, 0x2800-0x28ff,
 * but we want to try to avoid allocating at 0x2900-0x2bff
 * which might have be mirrored at 0x0100-0x03ff..
 */
void
pcibios_align_resource(void *data, struct resource *res,
               resource_size_t size, resource_size_t align)
{
    struct pci_dev *dev = data;
    struct arc_pci *pcic = dev->sysdata;
    unsigned long start = res->start;

    if (res->flags & IORESOURCE_IO) {
        /* Make sure we start at our min on all hoses */
        if (start < PCIBIOS_MIN_IO + pcic->io_resource.start)
            start = PCIBIOS_MIN_IO + pcic->io_resource.start;

        /*
         * Put everything into 0x00-0xff region modulo 0x400
         */
        if (start & 0x300)
            start = (start + 0x3ff) & ~0x3ff;
    } else if (res->flags & IORESOURCE_MEM) {
        /* Make sure we start at our min on all hoses */
        if (start < PCIBIOS_MIN_MEM + pcic->mem_resource.start)
            start = PCIBIOS_MIN_MEM + pcic->mem_resource.start;
    }

    res->start = start;
}


int pcibios_enable_device(struct pci_dev *dev, int mask)
{
    u16 cmd, old_cmd;
    int idx;
    struct resource *r;

    pci_read_config_word(dev, PCI_COMMAND, &cmd);
    old_cmd = cmd;
    for(idx=0; idx<6; idx++) {
        /* Only set up the requested stuff */
        if (!(mask & (1<<idx)))
            continue;

        r = &dev->resource[idx];
        if (!r->start && r->end) {
            printk(KERN_ERR "PCI: Device %s not available because of resource collisions\n", pci_name(dev));
            return -EINVAL;
        }
        if (r->flags & IORESOURCE_IO)
            cmd |= PCI_COMMAND_IO;
        if (r->flags & IORESOURCE_MEM)
            cmd |= PCI_COMMAND_MEMORY;
    }
    if (dev->resource[PCI_ROM_RESOURCE].start)
        cmd |= PCI_COMMAND_MEMORY;
    if (cmd != old_cmd) {
        printk("PCI: Enabling device %s (%04x -> %04x)\n", pci_name(dev), old_cmd, cmd);
        pci_write_config_word(dev, PCI_COMMAND, cmd);
    }
    return 0;
}

int pcibios_assign_resource(struct pci_dev *pdev, int resource)
{
    return -ENXIO;
}



void __init pcibios_update_irq(struct pci_dev *dev, int irq)
{
        pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

/*
 * Other archs parse arguments here.
 */
char * __devinit pcibios_setup(char *str)
{
    return str;
}

int pcic_present(void)
{
    return 1;
}

subsys_initcall(pcic_init);

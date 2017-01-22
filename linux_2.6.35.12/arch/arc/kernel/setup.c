/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Rajeshwarr: June 2009
 *    -Tidy up the Bootmem Allocator setup
 *    -We were skipping OVER "1" page in bootmem setup
 *
 *  Vineetg: June 2009
 *    -Tag parsing done ONLY if bootloader passes it, otherwise defaults
 *     for those params are already setup at compile time.
 *
 *  Vineetg: April 2009
 *    -NFS and IDE (hda2) root filesystem works now
 *
 *  Vineetg: Jan 30th 2008
 *    -setup_processor() is now CPU init API called by all CPUs
 *        It includes TLB init, Vect Tbl Iit, Clearing ALL IRQs etc
 *    -cpuinfo_arc700 is now an array of NR_CPUS; contains MP info as well
 *    -/proc/cpuinfo iterator fixed so that show_cpuinfo() gets
 *        correct CPU-id to display CPU specific info
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/mmzone.h>
#include <linux/bootmem.h>
#include <linux/magic.h>
#include <asm/pgtable.h>
#include <asm/serial.h>
#include <asm/arcregs.h>
#include <asm/tlb.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/cache.h>
#include <asm/irq.h>
#include <linux/socket.h>
#include <linux/root_dev.h>
#include <linux/initrd.h>
#include <linux/console.h>
#include <asm/thread_info.h>
#include <asm/unwind.h>
#include <linux/module.h>

#include <asm/board/board_config.h>

extern int _text, _etext, _edata, _end;
extern unsigned long end_kernel;
extern unsigned long ramd_start, ramd_end;
extern int root_mountflags;

extern void arc_irq_init(void);
extern void arc_cache_init(void);
extern char * arc_mmu_mumbojumbo(int cpu_id, char *buf);
extern char * arc_cache_mumbojumbo(int cpu_id, char *buf);
extern void __init arc_verify_sig_sz(void);
extern void __init read_decode_cache_bcr(void);

struct cpuinfo_arc __sram_data cpuinfo_arc700[NR_CPUS];
#define FIX_PTR(x)  __asm__ __volatile__(";":"+r"(x))

/* Important System variables
 * We start with compile time DEFAULTS and over-ride ONLY if bootloader
 * passes atag list
 */

unsigned long end_mem = CONFIG_ARC_KERNEL_MEM_BASE + CONFIG_ARC_KERNEL_MIN_SIZE + PHYS_SRAM_OFFSET;
unsigned long clk_speed = CONFIG_ARC700_CLK;
unsigned long serial_baudrate = BASE_BAUD;
int arc_console_baud = (CONFIG_ARC700_DEV_CLK/(BASE_BAUD * 4)) - 1;
struct sockaddr mac_addr = {0, {0x64,0x66,0x46,0x88,0x63,0x33 } };

#ifdef CONFIG_ROOT_NFS

// Example of NFS root booting.
char __initdata command_line[COMMAND_LINE_SIZE] = {"root=/dev/nfs nfsroot=10.0.0.2:/home/vineetg/ARC/arc_initramfs_nfs,nolock ip=dhcp console=ttyS0" };

#elif defined(CONFIG_ARC_SERIAL_CONSOLE)

/* with console=tty0, arc uart console will be prefered console and
 * registrations will be successful, otherwise dummy console will be
 * registered if CONFIG_VT_CONSOLE is enabled
 */
char __initdata command_line[COMMAND_LINE_SIZE] = {"console=ttyS0"};

#else

// Clean, no kernel command line.
// char __initdata command_line[COMMAND_LINE_SIZE];

// Use this next line to temporarily switch on "earlyprintk"
char __initdata command_line[COMMAND_LINE_SIZE] = {"earlyprintk=1 console=ttyS0,38400n8"};

#endif

struct task_struct *_current_task[NR_CPUS];  /* currently active task */

/* A7 does not autoswitch stacks on interrupt or exception ,so we do it
 * manually.But we need one register for each interrupt level to play with
 * which is saved and restored from the following variable.
 */

/* vineetg: Jan 10th 2008:
 * In SMP we use MMU_SCRATCH_DATA0 to save a temp reg to make the ISR
 * re-entrant. However this causes performance penalty in TLB Miss code
 * because SCRATCH_DATA0 no longer stores the PGD ptr, which now has to be
 * fetched with 3 mem accesse as follows: curr_task->active_mm->pgd
 * Thus in non SMP we retain the old scheme of using int1_saved_reg in ISR
 */
#ifndef CONFIG_SMP
volatile unsigned long int1_saved_reg;
#endif

volatile unsigned long int2_saved_reg;

/*
 * CPU info code now more organised. Instead of stupid if else,
 * added tables which can be run thru
 */
typedef struct {
    int id;
    const char *str;
    int up_range;
}
cpuinfo_data_t;


int __init read_arc_build_cfg_regs(void)
{
    int cpu = smp_processor_id();
    struct cpuinfo_arc *p_cpu = &cpuinfo_arc700[cpu];

    READ_BCR(AUX_IDENTITY, p_cpu->core);

    p_cpu->timers = read_new_aux_reg(ARC_REG_TIMERS_BCR);
    p_cpu->vec_base = read_new_aux_reg(AUX_INTR_VEC_BASE);
    p_cpu->perip_base = read_new_aux_reg(ARC_REG_PERIBASE_BCR);
    READ_BCR(ARC_REG_D_UNCACH_BCR, p_cpu->uncached_space);

    p_cpu->extn.mul = read_new_aux_reg(ARC_REG_MUL_BCR);
    p_cpu->extn.swap = read_new_aux_reg(ARC_REG_SWAP_BCR);
    p_cpu->extn.norm = read_new_aux_reg(ARC_REG_NORM_BCR);
    p_cpu->extn.minmax = read_new_aux_reg(ARC_REG_MIXMAX_BCR);
    p_cpu->extn.barrel = read_new_aux_reg(ARC_REG_BARREL_BCR);
    READ_BCR(ARC_REG_MAC_BCR, p_cpu->extn_mac_mul);

    p_cpu->extn.ext_arith = read_new_aux_reg(ARC_REG_EXTARITH_BCR);
    p_cpu->extn.crc = read_new_aux_reg(ARC_REG_CRC_BCR);

    /* Note that we read the CCM BCRs independent of kernel config
     * This is to catch the cases where user doesn't know that
     * CCMs are present in hardware build
     */
    {
        struct bcr_iccm iccm;
        struct bcr_dccm dccm;
        struct bcr_dccm_base dccm_base;
        unsigned int bcr_32bit_val;

        bcr_32bit_val = read_new_aux_reg(ARC_REG_ICCM_BCR);
        if (bcr_32bit_val) {
            iccm = *((struct bcr_iccm *)&bcr_32bit_val);
            p_cpu->iccm.base_addr = iccm.base << 16;
            p_cpu->iccm.sz = 0x2000 << (iccm.sz-1);
        }

        bcr_32bit_val = read_new_aux_reg(ARC_REG_DCCM_BCR);
        if (bcr_32bit_val) {
            dccm = *((struct bcr_dccm *)&bcr_32bit_val);
            p_cpu->dccm.sz = 0x800 << (dccm.sz);

            READ_BCR(ARC_REG_DCCMBASE_BCR, dccm_base);
            p_cpu->dccm.base_addr = dccm_base.addr << 8;
        }
    }

    READ_BCR(ARC_REG_XY_MEM_BCR, p_cpu->extn_xymem);

    read_decode_mmu_bcr();
    read_decode_cache_bcr();

    READ_BCR(ARC_REG_FP_BCR, p_cpu->fp);
    READ_BCR(ARC_REG_DPFP_BCR, p_cpu->dpfp);


#ifdef CONFIG_ARCH_ARC800
    READ_BCR(ARC_REG_MP_BCR, p_cpu->mp);
#endif

    return cpu;
}


char * arc_cpu_mumbojumbo(int cpu_id, char *buf)
{
    cpuinfo_data_t cpu_fam_nm [] = {
        { 0x10, "ARCTangent A5", 0x1F },
        { 0x20, "ARC 600", 0x2F },
        { 0x30, "ARC 700", 0x33 },
        { 0x34, "ARC 700 R4.10", 0x34 },
        { 0x0, NULL }
    };
    int i, num=0;
    struct cpuinfo_arc *p_cpu = & cpuinfo_arc700[cpu_id];
    FIX_PTR(p_cpu);


    for ( i = 0; cpu_fam_nm[i].id != 0; i++) {
        if ( (p_cpu->core.family >= cpu_fam_nm[i].id ) &&
             (p_cpu->core.family <= cpu_fam_nm[i].up_range) )
        {
            num += sprintf(buf, "\nProcessor Family: %s [0x%x]\n",
                    cpu_fam_nm[i].str, p_cpu->core.family);
            break;
        }
    }

    if ( cpu_fam_nm[i].id == 0 )
        num += sprintf(buf, "UNKNOWN ARC Processor\n");

    num += sprintf(buf+num, "CPU speed :\t%u.%02u Mhz\n",
                (unsigned int)(clk_speed / 1000000),
                (unsigned int)(clk_speed / 10000) % 100);

    num += sprintf(buf+num, "Timers: \t%s %s \n",
                ((p_cpu->timers & 0x200) ? "TIMER1":""),
                ((p_cpu->timers & 0x100) ? "TIMER0":""));

    num += sprintf(buf+num,"Interrupt Vect Base: \t0x%x \n", p_cpu->vec_base);
    if(p_cpu->perip_base==0){
        num += sprintf(buf+num,"Peripheral Base: NOT present; assuming 0xCOFC0000 \n");
    }
    else{
    num += sprintf(buf+num,"Peripheral Base: \t0x%x \n", p_cpu->perip_base);
    }
    num += sprintf(buf+num, "Data UNCACHED Base (I/O): start %#x Sz, %d MB \n",
            p_cpu->uncached_space.start, (0x10 << p_cpu->uncached_space.sz) );

    return buf;
}

char * arc_extn_mumbojumbo(int cpu_id, char *buf)
{
    const cpuinfo_data_t mul_type_nm [] = {
        { 0x0, "Not Present" },
        { 0x1, "32x32 with SPECIAL Result Reg" },
        { 0x2, "32x32 with ANY Result Reg" }
    };

    const cpuinfo_data_t mac_mul_nm[] = {
        { 0x0, "Not Present" },
        { 0x1, "Not Present" },
        { 0x2, "Dual 16 x 16" },
        { 0x3, "Not Present" },
        { 0x4, "32 x 16" },
        { 0x5, "Not Present" },
        { 0x6, "Dual 16 x 16 and 32 x 16" }
    };

    int num=0;
    struct cpuinfo_arc *p_cpu = & cpuinfo_arc700[cpu_id];
    FIX_PTR(p_cpu);

#define IS_AVAIL1(var)   ((var)? "Present" :"N/A")
#define IS_AVAIL3(var)   ((var)? "" :"N/A")

    num += sprintf(buf+num, "Extensions:\n");

    if (p_cpu->core.family == 0x34) {
        const char *inuse = "(in-use)";
        const char *notinuse = "(not used)";

        num += sprintf(buf+num,
            "   Insns: LLOCK/SCOND %s, SWAPE %s, RTSC %s\n",
            __CONFIG_ARC_HAS_LLSC_VAL ? inuse : notinuse,
            __CONFIG_ARC_HAS_SWAPE_VAL ? inuse : notinuse,
            __CONFIG_ARC_HAS_RTSC_VAL ? inuse : notinuse);

    }

    num += sprintf(buf+num, "   MPY: %s",
                        mul_type_nm[p_cpu->extn.mul].str);

    num += sprintf(buf+num, "   MAC MPY: %s\n",
                        mac_mul_nm[p_cpu->extn_mac_mul.type].str);

    num += sprintf(buf+num, "   DCCM: %s", IS_AVAIL3(p_cpu->dccm.sz));
    if (p_cpu->dccm.sz)
        num += sprintf(buf+num, "@ %x, %d KB ",
                        p_cpu->dccm.base_addr, TO_KB(p_cpu->dccm.sz));

    num += sprintf(buf+num, "  ICCM: %s", IS_AVAIL3(p_cpu->iccm.sz));
    if (p_cpu->iccm.sz)
        num += sprintf(buf+num, "@ %x, %d KB",
                        p_cpu->iccm.base_addr, TO_KB(p_cpu->iccm.sz));

    num += sprintf(buf+num, "\n   CRC: %s,", IS_AVAIL1(p_cpu->extn.crc));
    num += sprintf(buf+num, "   SWAP: %s", IS_AVAIL1(p_cpu->extn.swap));

#define IS_AVAIL2(var)   ((var == 0x2)? "Present" :"N/A")

    num += sprintf(buf+num, "   NORM: %s\n", IS_AVAIL2(p_cpu->extn.norm));
    num += sprintf(buf+num, "   Min-Max: %s,", IS_AVAIL2(p_cpu->extn.minmax));
    num += sprintf(buf+num, "   Barrel Shifter: %s\n",
                        IS_AVAIL2(p_cpu->extn.barrel));

    num += sprintf(buf+num, "   Ext Arith Insn: %s\n",
                        IS_AVAIL2(p_cpu->extn.ext_arith));

#ifdef CONFIG_ARCH_ARC800
    num += sprintf(buf+num, "MP Extensions: Ver (%d), Arch (%d)\n",
                    p_cpu->mp.ver, p_cpu->mp.mp_arch);

    num += sprintf(buf+num, "    SCU %s, IDU %s, SDU %s\n",
                    IS_AVAIL1(p_cpu->mp.scu),
                    IS_AVAIL1(p_cpu->mp.idu), IS_AVAIL1(p_cpu->mp.sdu));
#endif

    num += sprintf(buf+num, "Floating Point Extension: %s",
        (p_cpu->fp.ver || p_cpu->dpfp.ver)? "\n":"N/A\n");

    if (p_cpu->fp.ver) {
        num += sprintf(buf+num, "   Single Prec v[%d] %s\n",
                            (p_cpu->fp.ver), p_cpu->fp.fast ? "(fast)":"");
    }
    if (p_cpu->dpfp.ver) {
        num += sprintf(buf+num, "   Dbl Prec v[%d] %s\n",
                            (p_cpu->fp.ver), p_cpu->fp.fast ? "(fast)":"");
    }

    return buf;
}

void arc_chk_ccms(void)
{
#if defined(CONFIG_ARCH_ARC_DCCM) || defined(CONFIG_ARCH_ARC_ICCM)
    struct cpuinfo_arc *p_cpu = &cpuinfo_arc700[smp_processor_id()];
#endif

#ifdef CONFIG_ARCH_ARC_DCCM
    extern unsigned int __arc_dccm_base;

    /* DCCM can be arbit placed in hardware
     * Make sure it's placement/sz matches what Linux is built with
     */
    if ( (unsigned int)&__arc_dccm_base != p_cpu->dccm.base_addr)
        panic("Linux built with incorrect DCCM Base address\n");

    if (DCCM_COMPILE_SZ != p_cpu->dccm.sz)
        panic("Linux built with incorrect DCCM Size\n");
#endif

#ifdef CONFIG_ARCH_ARC_ICCM
    if (ICCM_COMPILE_SZ != p_cpu->iccm.sz)
        panic("Linux built with incorrect ICCM Size\n");
#endif
}

/* BVCI Bus Profiler: Latency Unit */

//#define CONFIG_ARC_BVCI_LAT_UNIT

#ifdef CONFIG_ARC_BVCI_LAT_UNIT

int mem_lat = 64;

static volatile int *ID = (volatile int *) BVCI_LAT_UNIT_BASE;

/* CTRL1 selects the Latency unit to program (0-8) */
static volatile int *LAT_CTRL1 = (volatile int *) BVCI_LAT_UNIT_BASE + 21;

/* CRTL2 provides the actual latency value to be programmed */
static volatile int *LAT_CTRL2 = (volatile int *) BVCI_LAT_UNIT_BASE + 22;

#endif


/* Ensure that FP hardware and kernel config match
 * -If hardware contains DPFP, kernel needs to save/restore FPU state
 *  across context switches
 * -If hardware lacks DPFP, but kernel configured to save FPU state then
 *  kernel trying to access non-existant DPFP regs will crash
 *
 * We only check for Dbl precision Floating Point, because only DPFP
 * hardware has dedicated regs which need to be saved/restored on ctx-sw
 * (Single Precision uses core regs), thus kernel is kind of oblivious to it
 */
void __init probe_fpu(void)
{
    struct cpuinfo_arc *p_cpu = &cpuinfo_arc700[smp_processor_id()];

    if (p_cpu->dpfp.ver) {
#ifndef CONFIG_ARCH_ARC_FPU
        printk("DPFP support broken in this kernel...\n");
#endif
    }
    else {
#ifdef CONFIG_ARCH_ARC_FPU
        panic("H/w lacks DPFP support, kernel won't work\n");
#endif
    }
}

void __init probe_lat_unit(void)
{
#ifdef CONFIG_ARC_BVCI_LAT_UNIT
    unsigned int id = *ID;

    printk("BVCI Profiler Ver %x\n",id);

    // *LAT_CTRL1 = 0; // Unit #0 : Adds latency to all mem accesses

    /* By default we want to simulate the delays
     * between (I$|D$) and memory
     */
    *LAT_CTRL1 = 1; // Unit #1 : I$ and system Bus
    *LAT_CTRL2 = mem_lat;

    *LAT_CTRL1 = 2; // Unit #2 : D$ and system Bus
    *LAT_CTRL2 = mem_lat;
#endif
}

/*
 * Initialize and setup the processor core
 * This is called by all the CPUs thus should not do special case stuff
 *    such as only for boot CPU etc
 */

void __init setup_processor(void)
{
    char str[512];
    int cpu_id = read_arc_build_cfg_regs();

    arc_irq_init();

    printk(arc_cpu_mumbojumbo(cpu_id, str));

    /* Enable MMU */
    arc_mmu_init();

    arc_cache_init();

    arc_chk_ccms();

    printk(arc_extn_mumbojumbo(cpu_id, str));

    probe_fpu();

    probe_lat_unit();
}

static int __init parse_tag_core(struct tag *tag)
{
    printk_init("ATAG_CORE: successful parsing\n");
    return 0;
}

__tagtable(ATAG_CORE, parse_tag_core);

static int __init parse_tag_mem32(struct tag *tag)
{
    printk_init("ATAG_MEM: size = 0x%x\n", tag->u.mem.size);

    end_mem = tag->u.mem.size + PHYS_SRAM_OFFSET;

    return 0;
}

__tagtable(ATAG_MEM, parse_tag_mem32);

static int __init parse_tag_clk_speed(struct tag *tag)
{
    printk_init("ATAG_CLK_SPEED: clock-speed = %d\n",
           tag->u.clk_speed.clk_speed_hz);
    clk_speed = tag->u.clk_speed.clk_speed_hz;
    return 0;
}

__tagtable(ATAG_CLK_SPEED, parse_tag_clk_speed);

/* not yet useful... */
static int __init parse_tag_ramdisk(struct tag *tag)
{
    printk_init("ATAG_RAMDISK: Flags for ramdisk = 0x%x\n",
           tag->u.ramdisk.flags);
    printk_init("ATAG_RAMDISK: ramdisk size = 0x%x\n", tag->u.ramdisk.size);
    printk_init("ATAG_RAMDISK: ramdisk start address = 0x%x\n",
           tag->u.ramdisk.start);
    return 0;

}

__tagtable(ATAG_RAMDISK, parse_tag_ramdisk);

/* not yet useful.... */
static int __init parse_tag_initrd2(struct tag *tag)
{
    printk_init("ATAG_INITRD2: initrd start = 0x%x\n", tag->u.initrd.start);
    printk_init("ATAG_INITRD2: initrd size = 0x%x\n", tag->u.initrd.size);
    return 0;

}

__tagtable(ATAG_INITRD2, parse_tag_initrd2);

static int __init parse_tag_cache(struct tag *tag)
{
    printk_init("ATAG_CACHE: ICACHE status = 0x%x\n", tag->u.cache.icache);
    printk_init("ATAG_CACHE: DCACHE status = 0x%x\n", tag->u.cache.dcache);
    return 0;

}

__tagtable(ATAG_CACHE, parse_tag_cache);

#ifdef CONFIG_ARC_SERIAL
static int __init parse_tag_serial(struct tag *tag)
{
    printk_init("ATAG_SERIAL: serial_nr = %d\n", tag->u.serial.serial_nr);
    /* when we have multiple uart's serial_nr should also be processed */
    printk_init("ATAG_SERIAL: serial baudrate = %d\n", tag->u.serial.baudrate);
    serial_baudrate = tag->u.serial.baudrate;
    return 0;
}
__tagtable(ATAG_SERIAL, parse_tag_serial);

#endif

static int __init parse_tag_vmac(struct tag *tag)
{
    int i;
    printk_init("ATAG_VMAC: vmac address = %d:%d:%d:%d:%d:%d\n",
           tag->u.vmac.addr[0], tag->u.vmac.addr[1], tag->u.vmac.addr[2],
           tag->u.vmac.addr[3], tag->u.vmac.addr[4], tag->u.vmac.addr[5]);

    mac_addr.sa_family = 0;

    for (i = 0; i < 6; i++)
        mac_addr.sa_data[i] = tag->u.vmac.addr[i];

    return 0;

}

__tagtable(ATAG_VMAC, parse_tag_vmac);

static int __init parse_tag_cmdline(struct tag *tag)
{
    printk_init("ATAG_CMDLINE: command line = %s\n", tag->u.cmdline.cmdline);
    strcpy(command_line, tag->u.cmdline.cmdline);
    parse_board_config(command_line);
    return 0;

}
__tagtable(ATAG_CMDLINE, parse_tag_cmdline);

static int __init parse_tag_hwid(struct tag *tag)
{
    printk_init("ATAG_HW_CONFIG_ID: hw_config_id = %u\n", tag->u.hwid.hwid);
    qtn_set_hw_config_id(tag->u.hwid.hwid);
    return 0;
}
__tagtable(ATAG_HW_CONFIG_ID, parse_tag_hwid);

static int __init parse_tag_spi_flash_protect_mode(struct tag *tag)
{
    printk_init("ATAG_SPI_FLASH_PROTECT_MODE: spi_flash_protect_mode = %u\n",
		tag->u.spi_flash_protect_mode.spi_flash_protect_mode);
    qtn_set_spi_protect_config(tag->u.spi_flash_protect_mode.spi_flash_protect_mode);
    return 0;
}
__tagtable(ATAG_SPI_FLASH_PROTECT_MODE, parse_tag_spi_flash_protect_mode);

static int __init parse_tag(struct tag *tag)
{
    extern struct tagtable __tagtable_begin, __tagtable_end;
    struct tagtable *t;

    for (t = &__tagtable_begin; t < &__tagtable_end; t++)
        if (tag->hdr.tag == t->tag) {
            t->parse(tag);
            break;
        }

    return t < &__tagtable_end;
}

static void __init parse_tags(struct tag *tags)
{
    while (tags->hdr.tag != ATAG_NONE) {

        if (!parse_tag(tags))
            printk_init("Ignoring unknown tag type\n");

        tags = tag_next(tags);
    }

}

static void __init arc_fill_poison(void)
{
    /* Fill beginning of DDR with poison bytes */
    unsigned long begin = RUBY_DRAM_BEGIN + CONFIG_ARC_NULL_BASE;
    unsigned long end = RUBY_DRAM_BEGIN + CONFIG_ARC_CONF_SIZE;
    /*
     * Poison the first page of memory with something likely to cause a noisy crash,
     * and disrupt the MAC if ever this region is mistakenly used.
     */
    memset((void*)begin, 0xEB, end - begin);
    flush_dcache_range(begin, end);
}

/* Sample of how atags must be prepared by the bootloader */
static struct init_tags {
    struct tag_header hdr1;
    struct tag_core core;
    struct tag_header hdr5;
    struct tag_clk_speed clk_speed;
    struct tag_header hdr6;
    struct tag_cache cache;
#ifdef CONFIG_ARC_SERIAL
    struct tag_header hdr7;
    struct tag_serial serial;
#endif
#ifdef CONFIG_ARCTANGENT_EMAC
    struct tag_header hdr8;
    struct tag_vmac vmac;
#endif
    struct tag_header hdr9;

} init_tags __initdata = {

    {tag_size(tag_core), ATAG_CORE},
    {},

    {tag_size(tag_clk_speed), ATAG_CLK_SPEED},
    {CONFIG_ARC700_CLK},

    {tag_size(tag_cache), ATAG_CACHE},

#ifdef CONFIG_ARC700_USE_ICACHE
    {1,
#else
    {0,

#endif
#ifdef CONFIG_ARC700_USE_DCACHE
     1},
#else
     0},

#endif

#ifdef CONFIG_ARC_SERIAL
    {tag_size(tag_serial), ATAG_SERIAL},
    {0, CONFIG_ARC_SERIAL_BAUD},
#endif

#ifdef CONFIG_ARCTANGENT_EMAC
    {tag_size(tag_vmac), ATAG_VMAC},
    {{0, 1, 2, 3, 4, 5}}, /* Never set the mac address starting with 1 */
#endif

    {0, ATAG_NONE}
};

void __init setup_arch(char **cmdline_p)
{
    int bootmap_size;
    unsigned long first_free_pfn, kernel_end_addr;
    extern unsigned long atag_head;
    struct tag *tags = (struct tag *)&init_tags;   /* to shut up gcc */

    /* stack overflow check */
    *end_of_stack(&init_task) = STACK_END_MAGIC;
#ifdef CONFIG_ARCH_RUBY_SRAM_IRQ_STACK
    extern unsigned long __irq_stack_begin;
    __irq_stack_begin = STACK_END_MAGIC;
#endif

    /* If parameters passed by u-boot, override compile-time parameters */
    if (atag_head) {
        tags = (struct tag *)atag_head;

        if (tags->hdr.tag == ATAG_CORE) {
            printk_init("Parsing ATAG parameters from bootloader\n");
            parse_tags(tags);
        } else
            printk_init("INVALID ATAG parameters from bootloader\n");
    }
    else {
        printk_init("SKIPPING ATAG parsing...\n");
    }

    /* Before probing MMU or caches, so any discrepancy printk( ) shows up
     * but after, tag parsing, as it could override the build-time baud
     */
    arc_early_serial_reg();

    setup_processor();

#ifdef CONFIG_SMP
    smp_init_cpus();
#endif

    init_mm.start_code = (unsigned long)&_text;
    init_mm.end_code = (unsigned long)&_etext;
    init_mm.end_data = (unsigned long)&_edata;
    init_mm.brk = (unsigned long)&_end;

    *cmdline_p = command_line;
    /*
     *  save a copy of he unparsed command line for the
     *  /proc/cmdline interface
     */
    strlcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);

    _current_task[0] = &init_task;

    /******* Setup bootmem allocator *************/

#define TO_PFN(addr) ((addr) >> PAGE_SHIFT)

    /* Make sure that "end_kernel" is page aligned in linker script
     * so that it points to first free page in system
     * Also being a linker script var, we need to do &end_kernel which
     * doesn't work with >> operator, hence helper "kernel_end_addr"
     */
    kernel_end_addr = (unsigned long) &end_kernel;

    /* First free page beyond kernel image */
    first_free_pfn = TO_PFN(kernel_end_addr);

    /* first page of system - kernel .vector starts here */
    min_low_pfn = TO_PFN(CONFIG_ARC_KERNEL_MEM_BASE + RUBY_DRAM_BEGIN);

    /* Last usable page of low mem (no HIGH_MEM yet for ARC port)
     * -must be BASE + SIZE
     */
    max_low_pfn = max_pfn = TO_PFN(end_mem);

    num_physpages = max_low_pfn - min_low_pfn;

    /* setup bootmem allocator */
    bootmap_size = init_bootmem_node(NODE_DATA(0),
                       first_free_pfn, /* place bootmem alloc bitmap here */
                       min_low_pfn,    /* First pg to track */
                       max_low_pfn);   /* Last pg to track */

    /* Make all mem tracked by bootmem alloc as usable,
     * except the bootmem bitmap itself
     */
    free_bootmem(kernel_end_addr,  end_mem - kernel_end_addr);
	free_bootmem(CONFIG_ARC_KERNEL_MEM_BASE + RUBY_DRAM_BEGIN, CONFIG_ARC_KERNEL_BASE - CONFIG_ARC_KERNEL_MEM_BASE);
    reserve_bootmem(kernel_end_addr, bootmap_size, BOOTMEM_DEFAULT);

    /* If no initramfs provided to kernel, and no NFS root, we fall back to
     * /dev/hda2 as ROOT device, assuming it has busybox and other
     * userland stuff
     */
#if !defined(CONFIG_BLK_DEV_INITRD) && !defined(CONFIG_ROOT_NFS)
    ROOT_DEV = Root_HDA2;
#endif

    /* Can be issue if someone passes cmd line arg "ro"
     * But that is unlikely so keeping it as it is
     */
    root_mountflags &= ~MS_RDONLY;

    console_verbose();

#ifdef CONFIG_VT
#if defined(CONFIG_ARC_PGU_CONSOLE)
    /* Arc PGU Console */
#error "FIXME: enable PGU Console"
#elif defined(CONFIG_DUMMY_CONSOLE)
    conswitchp = &dummy_con;
#endif
#endif

    paging_init();

    arc_verify_sig_sz();

    arc_unwind_init();
    arc_unwind_setup();

    arc_fill_poison();
}

/*
 *  Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
    char str[1024];
    int cpu_id = (0xFFFF & (int)v);

    seq_printf(m, arc_cpu_mumbojumbo(cpu_id, str));

    seq_printf(m, "Bogo MIPS : \t%lu.%02lu\n",
           loops_per_jiffy / (500000 / HZ),
           (loops_per_jiffy / (5000 / HZ)) % 100);

    seq_printf(m, arc_mmu_mumbojumbo(cpu_id, str));

    seq_printf(m, arc_cache_mumbojumbo(cpu_id, str));

    seq_printf(m, arc_extn_mumbojumbo(cpu_id, str));

    seq_printf(m, "\n\n");

    return 0;
}

static void *c_start(struct seq_file *m, loff_t * pos)
{
    /* this 0xFF xxxx business is a simple hack.
       We encode cpu-id as 0x 00FF <cpu-id> and return it as a ptr
       We Can't return cpu-id directly because 1st cpu-id is 0, which has
       special meaning in seq-file framework (iterator end).
       Otherwise we have to kmalloc in c_start() and do a free in c_stop()
       which is really not required for a such a simple case
       show_cpuinfo() extracts the cpu-id from it.
     */
    return *pos < NR_CPUS ? ((void *)(0xFF0000 | (int)(*pos))) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t * pos)
{
    ++*pos;
    return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
      start:c_start,
      next:c_next,
      stop:c_stop,
      show:show_cpuinfo,
};

/* vineetg, Dec 15th 2007
   CPU Topology Support
 */

#include <linux/cpu.h>

struct cpu cpu_topology[NR_CPUS];

static int __init topology_init(void)
{
    int cpu;

    for_each_possible_cpu(cpu)
        register_cpu(&cpu_topology[cpu], cpu);

    return 0;
}

subsys_initcall(topology_init);

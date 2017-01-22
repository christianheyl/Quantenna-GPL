/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * I-Cache and D-cache control functionality.
 *
 *  vineetg: May 2011: for Non-aliasing VIPT D-cache following can be NOPs
 *   -flush_cache_dup_mm (fork)
 *   -likewise for flush_cache_mm (exit/execve)
 *   -likewise for flush_cache_{range,page} (munmap, exit, COW-break)
 *
 * vineetg: Apr 2011
 *  -Now that MMU can support larger pg sz (16K), the determiniation of
 *   aliasing shd not be based on assumption of 8k pg
 *
 * vineetg: Mar 2011
 *  -optimised version of flush_icache_range( ) for making I/D coherent
 *   when vaddr is available (agnostic of num of aliases)
 *
 * vineetg: Mar 2011
 *  -Added documentation about I-cache aliasing on ARC700 and the way it
 *   was handled up until MMU V2.
 *  -Spotted a three year old bug when killing the 4 aliases, which needs
 *   bottom 2 bits, so we need to do paddr | {0x00, 0x01, 0x02, 0x03}
 *                        instead of paddr | {0x00, 0x01, 0x10, 0x11}
 *   (Rajesh you owe me one now)
 *
 * vineetg: Dec 2010
 *  -Off-by-one error when computing num_of_lines to flush
 *   This broke signal handling with bionic which uses synthetic sigret stub
 *
 * vineetg: Mar 2010
 *  -GCC can't generate ZOL for core cache flush loops.
 *   Conv them into iterations based as opposed to while (start < end) types
 *
 * Vineetg: July 2009
 *  -In I-cache flush routine we check for aliasing for every line INV.
 *   Instead now we setup routines per cache geometry and invoke them
 *   via function pointers.
 *
 * Vineetg: Mar 2009
 *  -D-Cache Invalidate mode set to INV ONLY as that is more common than
 *      FLUSH BEFORE INV (not called at all)
 *
 * Vineetg: Jan 2009
 *  -Cache Line flush routines used to flush an extra line beyond end addr
 *   because check was while (end >= start) instead of (end > start)
 *     =Some call sites had to work around by doing -1, -4 etc to end param
 *     ==Some callers didnt care. This was spec bad in case of INV routines
 *       which would discard valid data (cause of the horrible ext2 bug
 *       in ARC IDE driver)
 *
 * Vineetg: Oct 3rd 2008:
 *  -Got rid of un-necessary globals such as Cache line size,
 *      checks if cache enabled in entry of each cache routine etc
 *  -Cache Meta data (sz, alising etc) saved in a seperate structure rather than
 *      in the big cpu info struct
 *
 * vineetg: June 11th 2008:
 *  -Fixed flush_icache_range( )
 *   + As an API it is called by kernel while doing insmod after copying module
 *     code from user space to kernel space in load_module( ).
 *     Later kernel executes module out of this COPIED OVER kernel memory.
 *     Since ARC700 caches are not coherent (I$ doesnt snoop D$) both need
 *     to be flushed in flush_icache_range() which it was not doing.
 *   + load_module( ) passes vmalloc addr (Kernel Virtual Addr) to the API,
 *     however ARC cache maintenance OPs require PHY addr. Thus need to do
 *     vmalloc_to_phy.
 *   + Also added optimisation there, that for range > PAGE SIZE we flush the
 *     entire cache in one shot rather than line by line. For e.g. a module
 *     with Code sz 600k, old code flushed 600k worth of cache (line-by-line),
 *     while cache is only 16 or 32k.
 *
 * Amit Bhor, Rahul Trivedi: Codito Technologies 2004
 */

#include <linux/module.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <asm/mmu_context.h>
#include <asm/io.h>

extern int running_on_hw;

#ifdef CONFIG_ARC700_USE_ICACHE
static void __arc_icache_inv_lines_no_alias(unsigned long, int);
static void __arc_icache_inv_lines_2_alias(unsigned long, int);
static void __arc_icache_inv_lines_4_alias(unsigned long, int);

/* Holds the ptr to flush routine, dependign on size due to aliasing issues */
static void (* ___flush_icache_rtn)(unsigned long, int);
#endif


char * arc_cache_mumbojumbo(int cpu_id, char *buf)
{
    int num = 0;
    struct cpuinfo_arc_cache *p_cache = &cpuinfo_arc700[0].icache;

    num += sprintf(buf+num,"Detected I-cache : \n");
    num += sprintf(buf+num,"  Type=%d way set-assoc, Line length=%u, Size=%uK",
            p_cache->assoc, p_cache->line_len, TO_KB(p_cache->sz));

#ifdef CONFIG_ARC700_USE_ICACHE
    num += sprintf(buf+num," (enabled)\n");
#else
    num += sprintf(buf+num," (disabled)\n");
#endif

    p_cache = &cpuinfo_arc700[0].dcache;
    num += sprintf(buf+num,"Detected D-cache : \n");
    num += sprintf(buf+num,"  Type=%d way set-assoc, Line length=%u, Size=%uK",
            p_cache->assoc, p_cache->line_len, TO_KB(p_cache->sz));

#ifdef  CONFIG_ARC700_USE_DCACHE
    num += sprintf(buf+num," (enabled)\n");
#else
    num += sprintf(buf+num," (disabled)\n");
#endif

    return buf;
}

/* Read the Cache Build Confuration Registers, Decode them and save into
 * the cpuinfo structure for later use.
 * No Validation is done here, simply read/convert the BCRs
 */
void __init read_decode_cache_bcr(void)
{

#ifdef CONFIG_ARC700_USE_ICACHE
    {
        struct bcr_cache ibcr;

        struct cpuinfo_arc_cache *p_ic = &cpuinfo_arc700[0].icache;
        READ_BCR(ARC_REG_I_CACHE_BUILD_REG, ibcr);

        if (ibcr.config == 0x3)
            p_ic->assoc = 2;

        p_ic->line_len = 8 << ibcr.line_len;
        p_ic->sz = 0x200 << ibcr.sz;
        p_ic->ver = ibcr.ver;
    }
#endif

#ifdef CONFIG_ARC700_USE_DCACHE
    {
        struct bcr_cache dbcr;

        struct cpuinfo_arc_cache *p_dc = &cpuinfo_arc700[0].dcache;
        READ_BCR(ARC_REG_DC_BUILD_REG, dbcr);

        if (dbcr.config == 0x2)
            p_dc->assoc = 4; // 4 way set assoc

        p_dc->line_len = 16 << dbcr.line_len;
        p_dc->sz = 0x200 << dbcr.sz;
        p_dc->ver = dbcr.ver;
    }
#endif

}

/* 1. Validate the Cache Geomtery (compile time config matches hardware)
 * 2. If I-cache suffers from aliasing, setup work arounds (difft flush rtn)
 * 3. Enable the Caches, setup default flush mode for D-Cache
 * 3. Calculate the SHMLBA used by user space
 */
void __init arc_cache_init(void)
{
    struct cpuinfo_arc_cache *dc, *ic;
    unsigned int temp;
    int way_pg_ratio;
    char str[512];

    printk(arc_cache_mumbojumbo(0, str));

    ARC_shmlba = max(ARC_shmlba, (unsigned int)PAGE_SIZE);

    /***********************************************
     * I-Cache related init
     **********************************************/

    ic = &cpuinfo_arc700[0].icache;

#ifdef CONFIG_ARC700_USE_ICACHE
    /* 1. Confirm some of I-cache params which Linux assumes */
    if ( ( ic->assoc != ICACHE_COMPILE_WAY_NUM) ||
         ( ic->line_len != ICACHE_COMPILE_LINE_LEN ) ) {
        panic("Cache H/W doesn't match kernel Config");
    }

#if (CONFIG_ARC_MMU_VER > 2)
    if (ic->ver != 3) {
        if (!running_on_hw)
            printk("Use -prop=icache_version=3,-prop=dcache_version=3\n");

        panic("Cache ver doesn't match MMU ver\n");
    }
#endif

    /* if Cache way size is <= page size then no aliasing exhibited
     * otherwise ratio determines num of aliases.
     * e.g. 32K I$, 2 way set assoc, 8k pg size
     *       way-sz = 32k/2 = 16k
     *       way-pg-ratio = 16k/8k = 2, so 2 aliases possible
     *       (meaning 1 line could be in 2 possible locations).
     */
    way_pg_ratio = ic->sz/ICACHE_COMPILE_WAY_NUM/PAGE_SIZE;
    switch(way_pg_ratio) {
    case 0:
    case 1:
        ___flush_icache_rtn = __arc_icache_inv_lines_no_alias;
        break;
    case 2:
        ___flush_icache_rtn = __arc_icache_inv_lines_2_alias;
        break;
    case 4:
        ___flush_icache_rtn = __arc_icache_inv_lines_4_alias;
        break;
    default:
        panic("Unsupported I-Cache Sz\n");
    }

    if (way_pg_ratio > 1) {
        ic->has_aliasing = 1;
        ARC_shmlba = max(ARC_shmlba, ic->sz / ICACHE_COMPILE_WAY_NUM);
    }
#endif

    /* Enable/disable I-Cache */
    temp = read_new_aux_reg(ARC_REG_IC_CTRL) | BIT_IC_CTRL_ADDRESS_DEBUG_TYPE;

#ifdef TOPAZ_ICACHE_WORKAROUND
    temp |= (BIT_IC_CTRL_CACHE_DISABLE);
#elif defined(CONFIG_ARC700_USE_ICACHE)
    temp &= (~BIT_IC_CTRL_CACHE_DISABLE);
#else
    temp |= (BIT_IC_CTRL_CACHE_DISABLE);
#endif

     write_new_aux_reg(ARC_REG_IC_CTRL, temp);

    /***********************************************
     * D-Cache related init
     **********************************************/

    dc = &cpuinfo_arc700[0].dcache;

#ifdef CONFIG_ARC700_USE_DCACHE
    if ( ( dc->assoc != DCACHE_COMPILE_WAY_NUM) ||
         ( dc->line_len != DCACHE_COMPILE_LINE_LEN ) ) {
        panic("Cache H/W doesn't match kernel Config");
    }

    /* check for D-Cache aliasing */
    if ((dc->sz / DCACHE_COMPILE_WAY_NUM) > PAGE_SIZE) {
        panic("D$ aliasing not handled right now\n");
        dc->has_aliasing = 1;
        ARC_shmlba = max(ARC_shmlba, dc->sz / DCACHE_COMPILE_WAY_NUM);
    }
#endif

    /* Set the default Invalidate Mode to "simpy discard dirty lines"
     *  as this is more frequent then flush before invalidate
     * Ofcourse we toggle this default behviour when desired
     */
    temp = read_new_aux_reg(ARC_REG_DC_CTRL);
    temp &= ~BIT_DC_CTRL_INV_MODE_FLUSH;

#ifdef  CONFIG_ARC700_USE_DCACHE
    /* Enable D-Cache: Clear Bit 0 */
    if ((readl(RUBY_SYS_CTL_CSR) & 0xff) == TOPAZ_BOARD_REVA) {
        qtn_cache_topaz_war_dcache_lock(ARC_REG_DC_CTRL, temp & (~BIT_IC_CTRL_CACHE_DISABLE));
    } else {
        write_new_aux_reg(ARC_REG_DC_CTRL, temp & (~BIT_IC_CTRL_CACHE_DISABLE));
    }
#else
    /* Flush D cache */
    write_new_aux_reg(ARC_REG_DC_FLSH, 0x1);
    /* Disable D cache */
    write_new_aux_reg(ARC_REG_DC_CTRL, temp | BIT_IC_CTRL_CACHE_DISABLE);
#endif

    return;
}

#ifdef CONFIG_ARC700_USE_DCACHE

/***********************************************************
 * Machine specific helpers for Entire D-Cache or Per Line ops
 **********************************************************/

#define OP_INV          0x1
#define OP_FLUSH        0x2
#define OP_FLUSH_N_INV  0x3

/* Operation on Entire D-Cache
 * @cacheop = {OP_INV, OP_FLUSH, OP_FLUSH_N_INV}
 * @aux_reg: Relevant Register in Cache
 * Note that constant propagation ensures all the checks are gone
 *  in generated code, while havign a signle core routine to maintain
 */
static inline void __arc_dcache_entire_op(const int aux_reg, const int cacheop)
{
    unsigned long flags, tmp = tmp;

    local_irq_save(flags);

    if (cacheop == OP_FLUSH_N_INV) {
        /* Dcache provides 2 cmd: FLUSH or INV
         * INV inturn has sub-modes: DISCARD or FLUSH-BEFORE
         * flush-n-inv is achieved by INV cmd but with IM=1
         * Default INV sub-mode is DISCARD, which needs to be wiggled here
         */
        tmp = read_new_aux_reg(ARC_REG_DC_CTRL);
        write_new_aux_reg(ARC_REG_DC_CTRL, tmp|BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    if ((readl(RUBY_SYS_CTL_CSR) & 0xff) == TOPAZ_BOARD_REVA) {
        qtn_cache_topaz_war_dcache_lock(aux_reg, 0x1);
    } else {
        write_new_aux_reg(aux_reg, 0x1);
	}

    if (cacheop & OP_FLUSH) {
        /* wait for writeback to complete */
        while (read_new_aux_reg(ARC_REG_DC_CTRL) & BIT_DC_CTRL_FLUSH_STATUS);
    }

    if (cacheop == OP_FLUSH_N_INV) {
        /* Switch back the DISCARD ONLY Invalidate mode */
        write_new_aux_reg(ARC_REG_DC_CTRL, tmp & ~BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    local_irq_restore(flags);
}

/* Per Line Operation on D-Cache
 * Doesn't deal with type-of-op/IRQ-disabling/waiting-for-flush-to-complete
 * It's sole purpose is to help gcc generate ZOL
 */

static inline void __arc_dcache_per_line_op(unsigned long start, unsigned long sz, int aux_reg)
{
    int num_lines, slack;

    /* Ensure we properly floor/ceil the non-line aligned/sized requests
     * and have @start - aligned to cache line and integral @num_lines
     * However page sized flushes can be compile time optimised.
     *  -@start will be cache-line aligned already (being page aligned)
     *  -@sz will be integral multiple of line size (being page sized).
     */
    if (!(__builtin_constant_p(sz) && sz == PAGE_SIZE )) {
        slack = start & ~DCACHE_LINE_MASK;
        sz += slack;
        start -= slack;
    }

    num_lines = (sz + DCACHE_COMPILE_LINE_LEN - 1)/DCACHE_COMPILE_LINE_LEN;

    while (num_lines-- > 0) {
#if (CONFIG_ARC_MMU_VER > 2)
        /* Just as for I$, in MMU v3, D$ ops also require
         * "tag" bits in DC_PTAG, "index" bits in {FL|IV}DL
         * But we pass phy addr for both. This works for now since Linux
         * doesn't support aliasing configs for D$, yet. Thus paddr is
         * enough to provide both tag and index.
         */
        write_new_aux_reg(ARC_REG_DC_PTAG, start);
#endif
        write_new_aux_reg(aux_reg, start);
        start += DCACHE_COMPILE_LINE_LEN;
    }
}

/* D-Cache : Per Line FLUSH (wback)
 */

static inline void __arc_dcache_flush_lines(unsigned long start, unsigned long sz)
{
    unsigned long flags;

    local_irq_save(flags);

    __arc_dcache_per_line_op(start, sz, ARC_REG_DC_FLDL);

    /* wait for the flush to complete, poll on the FS Bit */
    while (read_new_aux_reg(ARC_REG_DC_CTRL) & BIT_DC_CTRL_FLUSH_STATUS) ;

    local_irq_restore(flags);
}

/* D-Cache : Per Line INV (discard or wback+discard)
 */

static inline void __sram_text __arc_dcache_inv_lines(unsigned long start, unsigned long sz,
                                    int flush_n_inv)
{
    unsigned long flags, orig_d_ctrl = orig_d_ctrl;

    local_irq_save(flags);

    if (flush_n_inv) {
        /* Dcache provides 2 cmd: FLUSH or INV
         * INV inturn has sub-modes: DISCARD or FLUSH-BEFORE
         * Default INV sub-mode is DISCARD, hence this check
         */
        orig_d_ctrl = read_new_aux_reg(ARC_REG_DC_CTRL);
	    write_new_aux_reg(ARC_REG_DC_CTRL, orig_d_ctrl | BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    __arc_dcache_per_line_op(start, sz, ARC_REG_DC_IVDL);


    if (flush_n_inv) {
        /* wait for the flush to complete, poll on the FS Bit */
        while (read_new_aux_reg(ARC_REG_DC_CTRL) & BIT_DC_CTRL_FLUSH_STATUS) ;

        /* Switch back the DISCARD ONLY Invalidate mode */
	    write_new_aux_reg(ARC_REG_DC_CTRL, orig_d_ctrl & ~BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    local_irq_restore(flags);
}

/***********************************************************
 * Exported APIs
 **********************************************************/

void inv_dcache_all()
{
    /* Throw away the contents of entrie Dcache */
    __arc_dcache_entire_op(ARC_REG_DC_IVDC, OP_INV);
}

void flush_and_inv_dcache_all(void)
{
    __arc_dcache_entire_op(ARC_REG_DC_IVDC, OP_FLUSH_N_INV);
}

void flush_dcache_all()
{
    __arc_dcache_entire_op(ARC_REG_DC_FLSH, OP_FLUSH);
}


void flush_dcache_range(unsigned long start, unsigned long end)
{
    __arc_dcache_flush_lines(start, end - start);
}

void flush_dcache_page(struct page *page)
{
    __arc_dcache_flush_lines((unsigned long)page_address(page), PAGE_SIZE);
}

/*
 * page is the kernel virtual addr and ARC_REG_DC_FLDL expects a phy addr
 * TODO: need to remove this
 */
void flush_dcache_page_virt(unsigned long *page)
{
    __arc_dcache_flush_lines((unsigned long)page,PAGE_SIZE);
}


void __sram_text flush_and_inv_dcache_range(unsigned long start, unsigned long end)
{
    __arc_dcache_inv_lines(start, end-start, 1);
}

__attribute__((section(".sram.text"))) void inv_dcache_range(unsigned long start, unsigned long end)
{
    __arc_dcache_inv_lines(start, end-start, 0);
}

EXPORT_SYMBOL(flush_dcache_range);
EXPORT_SYMBOL(inv_dcache_range);
EXPORT_SYMBOL(flush_dcache_all);
EXPORT_SYMBOL(flush_and_inv_dcache_range);
EXPORT_SYMBOL(flush_and_inv_dcache_all);
EXPORT_SYMBOL(flush_dcache_page);
#else
#define __arc_dcache_flush_lines(a,b)
#endif

#ifdef CONFIG_ARC700_USE_ICACHE

/***********************************************************
 * Machine specific helpers for per line I-Cache invalidate
 * We have 3 different routines to take care of I-cache aliasing
 * in specific configurations of ARC700
 *
 * For VIPT caches on ARC700, the internal logic of CPU, when fetching code,
 * needs to lookup I$. It uses vaddr (embedded in program code) to calc
 * the set-idx of cache-line and the corresponding paddr from MMU to match
 * the cache-line-tag. However the CDU iterface (to flush/inv) lines from
 * software, only takes paddr (partially to have simpler interface and moreso
 * because lot of times page is not yet mapped, so vaddr is not avail).
 * For simpler cases, using paddr alone suffices.
 * e.g. 2-way-set-assoc, 16K I$ (8k MMU pg sz, 32b cache line size):
 *      way_sz = cache_sz / num_ways = 16k/2 = 8k
 *      num_sets = way_sz / line_sz = 8k/32 = 256 => 8 bits
 *   Ignoring the bottom 5 bits corresp to the pffset within a 32b cacheline,
 *   bits req for calc set-index = bits 12:5 (0 based). Since this range fits
 *   inside the bottom 13 bits of paddr, which are same for vaddr and paddr
 *   (with 8k pg sz), paddr alone can be safely used by CDU to calc line's
 *   set-index
 *
 * However for a difft sized cache, say 32k I$, above math yields need
 * for 14 bits of vaddr to locate a cache line, which can't be provided by
 * paddr, since the bit 13 may differ between the two.
 *
 * This lack of extra bits needed for correct line addressing, defines the
 * classical problem of Cache aliasing with VIPT architectures
 * num_aliases = 1 << extra_bits
 * e.g. 2-way-set-assoc, 32K I$ with 8k MMU pg sz => 2 aliases
 *      2-way-set-assoc, 64K I$ with 8k MMU pg sz => 4 aliases
 *      2-way-set-assoc, 16K I$ with 8k MMU pg sz => NO aliases
 *
 * ------------------
 * MMU v2
 * ------------------
 * The solution was to provide CDU with these additonal vaddr bits. These
 * would be bits [x:13], x would depend on cache-geom.
 * H/w folks chose [17:13] to be a future safe range, and moreso these 5 bits
 * of vaddr could easily be "stuffed" in the paddr as bits [4:0] since the
 * orig 5 bits of paddr were anyways ignored by CDU line ops, since they
 * represent the offset within cache-line. The adv of using this "clumsy"
 * interace for additional info was no new reg was needed in CDU.
 *
 * 17:13 represented the max num of bits passable, actual bits needed were
 * fewer, based on the num-of-aliases possible.
 * -for 2 alias possibility, only bit 13 needed (32K cache)
 * -for 4 alias possibility, bits 14:13 needed (64K cache)
 *
 * Since vaddr was not available for all instances of I$ flush req by core
 * kernel, the only safe way (non-optimal though) was to kill all possible
 * lines which could represent an alias (even if they didnt represent one
 * in execution).
 * e.g. for 64K I$, 4 aliases possible, so we did
 *      flush start
 *      flush start | 0x01
 *      flush start | 0x2
 *      flush start | 0x3
 *
 * The penalty was invoking the operation itself, since tag match is anyways
 * paddr based, a line which didn't represent an alias would not match the
 * paddr, hence wont be killed
 *
 * Note that aliasing concerns are independent of line-sz for a given cache
 * geometry (size + set_assoc) because the extra bits required by line-sz are
 * reduced from the set calc.
 * e.g. 2-way-set-assoc, 32K I$ with 8k MMU pg sz and using match above
 *  32b line-sz: 9 bits set-index-calc, 5 bits offset-in-line => 1 extra bit
 *  64b line-sz: 8 bits set-index-calc, 6 bits offset-in-line => 1 extra bit
 *
 * ------------------
 * MMU v3
 * ------------------
 * This ver of MMU supports var page sizes (1k-16k) - Linux will support
 * 8k (default), 16k and 4k.
 * However from hardware perspective, smaller page sizes aggrevate aliasing
 * meaning more vaddr bits needed to disambiguate the cache-line-op ;
 * the existing scheme of piggybacking won't work for certain configurations.
 * Two new registers IC_PTAG and DC_PTAG inttoduced.
 * "tag" bits are provided in PTAG, index bits in existing IVIL/IVDL/FLDL regs
 *
 **********************************************************/

/*
 * start, end - PHY Address
 */

static void __arc_icache_inv_lines_no_alias(unsigned long start, int num_lines)
{
    while (num_lines-- > 0) {
#if (CONFIG_ARC_MMU_VER > 2)
        write_new_aux_reg(ARC_REG_IC_PTAG, start);
#endif
        write_new_aux_reg(ARC_REG_IC_IVIL, start);
        start += ICACHE_COMPILE_LINE_LEN;
    }
}

static void __arc_icache_inv_lines_2_alias(unsigned long start, int num_lines)
{
    while (num_lines-- > 0) {

#if (CONFIG_ARC_MMU_VER > 2)
        /* MMU v3, CDU prog model (for line ops) now uses a new IC_PTAG reg
         * to pass the "tag" bits and existing IVIL reg only looks at bits
         * relevant for "index" (details above)
         * Programming Notes:
         * -when writing tag to PTAG reg, bit chopping can be avoided,
         *  CDU ignores non-tag bits.
         * -Ideally "index" must be computed from vaddr, but it is not avail
         *  in these rtns. So to be safe, we kill the lines in all possible
         *  indexes corresp to num of aliases possible for given cache config.
         */
        write_new_aux_reg(ARC_REG_IC_PTAG, start);
        write_new_aux_reg(ARC_REG_IC_IVIL, start & ~(0x1 << PAGE_SHIFT));
        write_new_aux_reg(ARC_REG_IC_IVIL, start |  (0x1 << PAGE_SHIFT));
#else
        write_new_aux_reg(ARC_REG_IC_IVIL, start);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x01);
#endif
        start += ICACHE_COMPILE_LINE_LEN;
    }
}

static void __arc_icache_inv_lines_4_alias(unsigned long start, int num_lines)
{
    while (num_lines-- > 0) {

#if (CONFIG_ARC_MMU_VER > 2)
        write_new_aux_reg(ARC_REG_IC_PTAG, start);

        write_new_aux_reg(ARC_REG_IC_IVIL, start & ~(0x3 << PAGE_SHIFT));
        write_new_aux_reg(ARC_REG_IC_IVIL, start & ~(0x2 << PAGE_SHIFT));
        write_new_aux_reg(ARC_REG_IC_IVIL, start & ~(0x1 << PAGE_SHIFT));
        write_new_aux_reg(ARC_REG_IC_IVIL, start |  (0x3 << PAGE_SHIFT));
#else
        write_new_aux_reg(ARC_REG_IC_IVIL, start);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x01);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x02);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x03);
#endif
        start += ICACHE_COMPILE_LINE_LEN;
    }
}

static void __arc_icache_inv_lines(unsigned long start, unsigned long sz)
{
    unsigned long flags;
    int num_lines, slack;

    /* Ensure we properly floor/ceil the non-line aligned/sized requests
     * and have @start - aligned to cache line, and integral @num_lines
     * However page sized flushes can be compile time optimised.
     *  -@start will be cache-line aligned already (being page aligned)
     *  -@sz will be integral multiple of line size (being page sized).
     */
    if (!(__builtin_constant_p(sz) && sz == PAGE_SIZE )) {
        slack = start & ~ICACHE_LINE_MASK;
        sz += slack;
        start -= slack;
    }

    num_lines = (sz + ICACHE_COMPILE_LINE_LEN - 1)/ICACHE_COMPILE_LINE_LEN;

    local_irq_save(flags);
    (*___flush_icache_rtn)(start, num_lines);
    local_irq_restore(flags);
}

/* Unlike routines above, having vaddr for flush op (along with paddr),
 * prevents the need to speculatively kill the lines in multiple sets
 * based on ratio of way_sz : pg_sz
 */
static void __arc_icache_inv_lines_vaddr(unsigned long phy_start,
                unsigned long vaddr, unsigned long sz)
{
    unsigned long flags;
    int num_lines, slack;
    unsigned int addr;

    slack = phy_start & ~ICACHE_LINE_MASK;
    sz += slack;
    phy_start -= slack;
    num_lines = (sz + ICACHE_COMPILE_LINE_LEN - 1)/ICACHE_COMPILE_LINE_LEN;

#if (CONFIG_ARC_MMU_VER > 2)
    vaddr &= ~ICACHE_LINE_MASK;
    addr = phy_start;
#else
    // bits 17:13 of vaddr go as bits 4:0 of paddr
    addr = phy_start | (( vaddr >> 13 ) & 0x1F);
#endif

    local_irq_save(flags);
    while (num_lines-- > 0) {
#if (CONFIG_ARC_MMU_VER > 2)
        /* tag comes from phy addr */
        write_new_aux_reg(ARC_REG_IC_PTAG, addr);

        /* index bits come from vaddr */
        write_new_aux_reg(ARC_REG_IC_IVIL, vaddr);
        vaddr += ICACHE_COMPILE_LINE_LEN;
#else
        /* this paddr contains vaddrs bits as needed */
        write_new_aux_reg(ARC_REG_IC_IVIL, addr);
#endif
        addr += ICACHE_COMPILE_LINE_LEN;
    }
    local_irq_restore(flags);
}

/***********************************************************
 * Exported APIs
 **********************************************************/

/* This is API for making I/D Caches consistent when modifying code
 * (loadable modules, kprobes,  etc)
 * This is called on insmod, with kernel virtual address for CODE of
 * the module. ARC cache maintenance ops require PHY address thus we
 * need to convert vmalloc addr to PHY addr
 */
void flush_icache_range(unsigned long kstart, unsigned long kend)
{
    int tot_sz;
    unsigned long phy, pfn;
    unsigned long flags;

    //printk("Kernel Cache Cohenercy: %lx to %lx\n",kstart, kend);

    /* This is not the right API for user virtual address */
    if (kstart < TASK_SIZE) {
        BUG_ON("Flush icache range for user virtual addr space");
        return;
    }

    /* Shortcut for bigger flush ranges.
     * Here we dont care if this was kernel virtual or phy addr
     */
    tot_sz = kend - kstart + 1;
    if (tot_sz > PAGE_SIZE) {
        flush_cache_all();
        return;
    }

    /* Now it could be kernel virtual addr space (0x7000_0000 to 0x7FFF_FFFF)
     * or Kernel Physical addr space (0x8000_0000 onwards)
     * The caveat is ARC700 Cache flushes require PHY address, thus need
     * for seperate handling for virtual addr
     */

    /* Kernel Paddr */
    if (kstart > PAGE_OFFSET) {
        __arc_icache_inv_lines(kstart, kend-kstart);
        __arc_dcache_flush_lines(kstart, kend-kstart);
        return;
    }

    /* Kernel Vaddr */
    while (tot_sz > 0) {
        int sz;
        pfn = vmalloc_to_pfn((void *)kstart);
        phy = pfn << PAGE_SHIFT;
        sz = (tot_sz >= PAGE_SIZE)? PAGE_SIZE : tot_sz;
        //printk("Flushing for virt %lx Phy %lx PAGE %lx\n", kstart, phy, pfn);
        local_irq_save(flags);
        __arc_dcache_flush_lines(phy, sz);
        __arc_icache_inv_lines(phy, sz);
        local_irq_restore(flags);
        kstart += PAGE_SIZE;
        tot_sz -= PAGE_SIZE;
    }

}

/* Optimised ver of flush_icache_range() with spec callers: ptrace/signals
 * where vaddr is also available. This allows passing both vaddr and paddr
 * bits to CDU for cache flush, short-circuting the current pessimistic algo
 * which kills all possible aliases.
 * An added adv of knowing that vaddr is user-vaddr avoids various checks
 * and handling for k-vaddr, k-paddr as done in orig ver above
 */
void flush_icache_range_vaddr(unsigned long paddr, unsigned long u_vaddr,
                                int len)
{
    __arc_icache_inv_lines_vaddr(paddr, u_vaddr, len);
    __arc_dcache_flush_lines(paddr, len);
}

/*
 * This is called when a page-cache page is about to be mapped into a
 * user process' address space.  It offers an opportunity for a
 * port to ensure d-cache/i-cache coherency if necessary.
 */

void flush_icache_page(struct vm_area_struct *vma, struct page *page)
{
    if (!(vma->vm_flags & VM_EXEC))
        return;

    __arc_icache_inv_lines((unsigned long)page_address(page), PAGE_SIZE);
}

void flush_icache_all()
{
    unsigned long flags;

    local_irq_save(flags);

    write_new_aux_reg(ARC_REG_IC_IVIC, 1);

    /*
     * lr will not complete till the icache inv operation is not over
     */
    read_new_aux_reg(ARC_REG_IC_CTRL);
    local_irq_restore(flags);
}

void noinline flush_cache_all()
{
    unsigned long flags;

    local_irq_save(flags);

    flush_icache_all();

    flush_and_inv_dcache_all();

    local_irq_restore(flags);

}

#else
#define __arc_icache_inv_lines(a,b)
#endif


/* Explicit Cache flush request from user space via syscall
 * Needed for JITs which generate code on the fly
 * XXX: this code is a bit of overkill for the purpose it serves,
 * lean it down
 */
int sys_cacheflush(uint32_t start, uint32_t end, uint32_t flags)
{
#ifdef CONFIG_ARC700_CACHE
	struct vm_area_struct *vma;

	vma = find_vma(current->mm, start);
	while ((vma) && (vma->vm_start < end)) {
		uint32_t lstart, lend, laddr;
		lstart = (vma->vm_start < start) ? start : vma->vm_start;
		lend   = (vma->vm_end   > end  ) ? end   : vma->vm_end;
		lstart &= PAGE_MASK;

		for (laddr = lstart; laddr <= lend; laddr += PAGE_SIZE) {
			uint32_t pfn, sz, phy;
			unsigned long flags;

			pgd_t *pgd = pgd_offset(current->mm, laddr);
			pud_t *pud = pud_offset(pgd, laddr);
			pmd_t *pmd = pmd_offset(pud, laddr);
			pte_t *page_table = pte_offset_kernel(pmd, laddr);
			pte_t pte = *page_table;
			pfn = pte_pfn(pte);
			phy = pfn << PAGE_SHIFT;
			sz = (lend < (laddr + PAGE_SIZE)) ?
                             (lend - laddr) : PAGE_SIZE;
			if (start > laddr) {
				phy += start - laddr;
				sz -= start - laddr;
			}

			local_irq_save(flags);
			__arc_dcache_flush_lines(phy, sz);
			__arc_icache_inv_lines(phy, sz);
			local_irq_restore(flags);
		}
		start = vma->vm_end;
		vma = find_vma(current->mm, start);
	}

	return 0;
#else
    return -EINVAL;
#endif

}


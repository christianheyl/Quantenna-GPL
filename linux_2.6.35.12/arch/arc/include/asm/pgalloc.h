/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: June 2011
 *  -"/proc/meminfo | grep PageTables" kept on increasing
 *   Recently added pgtable dtor was not getting called.
 *
 * vineetg: May 2011
 *  -Variable pg-sz means that Page Tables could be variable sized themselves
 *    So calculate it based on addr traversal split [pgd-bits:pte-bits:xxx]
 *  -Page Table size capped to max 1 to save memory - hence verified.
 *  -Since these deal with constants, gcc compile-time optimizes them.
 *
 * vineetg: Nov 2010
 *  -Added pgtable ctor/dtor used for pgtable mem accounting
 *
 * vineetg: April 2010
 *  -Switched pgtable_t from being struct page * to unsigned long
 *      =Needed so that Page Table allocator (pte_alloc_one) is not forced to
 *       to deal with struct page. Thay way in future we can make it allocate
 *       multiple PG Tbls in one Page Frame
 *      =sweet side effect is avoiding calls to ugly page_address( ) from the
 *       pg-tlb allocator sub-sys (pte_alloc_one, ptr_free, pmd_populate
 *
 *  Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_PGALLOC_H
#define _ASM_ARC_PGALLOC_H

#include <linux/highmem.h>
#include <linux/log2.h>
#include <linux/hardirq.h>

#ifndef NONINLINE_MEMSET

/* @sz is bytes, but gauranteed to be multiple of 4
 * Similarly @ptr is alo word aligned
 */
static void inline memset_aligned(void *ptr, unsigned int sz)
{
    void *tmp = ptr;

    __asm__ __volatile__(
                  "mov     lp_count,%1\n"
                  "lp      1f\n"
                  "st.ab     0, [%0, 4]\n"
                  "st.ab     0, [%0, 4]\n"
                  "1:\n"
                  :"+r"(tmp)
                  :"ir"(sz/4/2) // 4: bytes to word
                                // 2: instances of st.ab in loop
                  :"lp_count");

}

#else
#define memset_aligned(p,s) memzero(p,s)
#endif

static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
    pmd_set(pmd, pte);
}

static inline void
pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t ptep)
{
    pmd_set(pmd, (pte_t *)ptep);
}

static inline int __get_order_pgd(void)
{
    const int num_pgs = (PTRS_PER_PGD * 4)/PAGE_SIZE;

    if (num_pgs)
        return order_base_2(num_pgs);

    return 0;  /* 1 Page */
}

static inline pgd_t *get_pgd_slow(void)
{
    int num, num2;
    pgd_t *ret = (pgd_t *)__get_free_pages(GFP_KERNEL, __get_order_pgd());

    if (ret) {
        num = USER_PTRS_PER_PGD + USER_KERNEL_GUTTER/PGDIR_SIZE;
        memset_aligned(ret, num * sizeof(pgd_t));

        num2 = VMALLOC_SIZE/PGDIR_SIZE;
        memcpy(ret + num, swapper_pg_dir + num, num2 * sizeof(pgd_t));

        memset_aligned(ret + num + num2,
               (PTRS_PER_PGD - num - num2) * sizeof(pgd_t));

    }
    return ret;
}

static inline void free_pgd_slow(pgd_t *pgd)
{
        free_pages((unsigned long)pgd,__get_order_pgd());
}

#define pgd_free(mm, pgd)      free_pgd_slow(pgd)
#define pgd_alloc(mm)          get_pgd_slow()

/* 1 Page whatever is PAGE_SIZE: 8k or 16k or 4k */
#define PTE_ORDER 0

/* We want to cap Page Table Size to 1 pg (although multiple tables can fit in
 * one page). This is obviously done to conserve resident-lockedup-memory and
 * also be able to use quicklists in future.
 * With software-only page-tables, aadr-split for traversal is tweakable and
 * that directly governs how big tables would be at each level. A wrong split
 * can overflow table size (complicated further by variable page size).
 * thus we need to programatically assert the size constraint
 *
 * All of this is const math, allowing gcc to do constant folding/propagation.
 * For good cases the entire fucntion is elimiated away.
 */
static inline void __verify_pte_order(void)
{
    /* SASID requires PTE to be two words - thus size of pg tbl is doubled */
#ifdef CONFIG_ARC_MMU_SASID
    const int multiplier = 2;
#else
    const int multiplier = 1;
#endif

    const int num_pgs = (PTRS_PER_PTE * 4 * multiplier)/PAGE_SIZE;

    if (num_pgs > 1)
        panic("PTE TBL too big\n");
}


static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm,
    unsigned long address)
{
    pte_t *pte;
    gfp_t gfp_mask = GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO;

    if (in_atomic())
	gfp_mask &= ~(__GFP_WAIT);

    __verify_pte_order();

    pte = (pte_t *) __get_free_pages(gfp_mask,
                                    PTE_ORDER);

    return pte;
}

static inline pgtable_t
pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
    pgtable_t pte_pg;

    __verify_pte_order();

    pte_pg = __get_free_pages(GFP_KERNEL | __GFP_REPEAT, PTE_ORDER);
    if (pte_pg)
    {
        memset_aligned((void *)pte_pg, PTRS_PER_PTE * 4);
		pgtable_page_ctor(virt_to_page(pte_pg));
    }

    return pte_pg;
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
    free_pages((unsigned long)pte, PTE_ORDER);  // takes phy addr
}

static inline void pte_free(struct mm_struct *mm, pgtable_t ptep)
{
	pgtable_page_dtor(virt_to_page(ptep));
    free_pages(ptep, PTE_ORDER);
}


#define __pte_free_tlb(tlb, pte, addr)  pte_free((tlb)->mm, pte)

extern void pgd_init(unsigned long page);

#define check_pgt_cache()   do { } while (0)

#define pmd_pgtable(pmd) pmd_page_vaddr(pmd)

#endif  /* _ASM_ARC_PGALLOC_H */

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: April 2010
 *  -PGD entry no longer contains any flags. If empty it is 0, otherwise has
 *   Pg-Tbl ptr. Thus pmd_present(), pmd_valid(), pmd_set( ) become simpler
 *
 * vineetg: April 2010
 *  -Switched form 8:11:13 split for page tabel lookup to 11:8:13
 *  -this speeds up page table allocation itself as we now have to memset 1K
 *    instead of 8k per page table.
 * -TODO: Right now page table alloc is 8K and rest 7K is unused
 *    need to optimise it
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_PGTABLE_H
#define _ASM_ARC_PGTABLE_H

#include <asm/page.h>
#include <asm/mmu.h>
#include <asm-generic/pgtable-nopmd.h>
#include <asm/mmapcode.h>

#include <asm/mem.h>

/* Page Table Lookup split */
#define BITS_IN_PAGE  PAGE_SHIFT

/* Optimal Sizing of Pg Tbl - based on page size */
#if defined(CONFIG_ARC_PAGE_SIZE_8K)
#define BITS_FOR_PTE  8
#elif defined(CONFIG_ARC_PAGE_SIZE_16K)
#define BITS_FOR_PTE  8
#elif defined(CONFIG_ARC_PAGE_SIZE_4K)
#define BITS_FOR_PTE  9
#endif

#define BITS_FOR_PGD  (32 - BITS_FOR_PTE - BITS_IN_PAGE)

#define PGDIR_SHIFT	(BITS_FOR_PTE + BITS_IN_PAGE)
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)    // Not TBL-sz, rather addr-space mapped
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/* These come automatically from the split above */
#define	PTRS_PER_PTE	(1 << BITS_FOR_PTE)
#define	PTRS_PER_PGD	(1 << BITS_FOR_PGD)

/* Number of entries a user land program use .
 * TASK_SIZE is the maximum Virtual address that can be used by a userland
 * program.
 */
#define	USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

/* Sameer: Need to recheck this value for ARC. */
#define FIRST_USER_ADDRESS      0

#ifndef __ASSEMBLY__

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

/* the zero page used for uninitialized and anonymous pages */
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))

#define pte_unmap(pte)		do { } while (0)
#define pte_unmap_nested(pte)		do { } while (0)

static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
  /* ARC doesn't keep page table caches in TLB */
}

#define set_pte(pteptr, pteval) ((*(pteptr)) = (pteval))
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)

/* find the page descriptor of the Page Tbl ref by PMD entry */
#define pmd_page(pmd) virt_to_page(pmd_val(pmd) & PAGE_MASK)

/* find the logical addr (phy for ARC) of the Page Tbl ref by PMD entry */
#define pmd_page_vaddr(pmd)	(pmd_val(pmd) & PAGE_MASK)

/* In a 2 level sys, setup the PGD entry with PTE value */
static inline void pmd_set(pmd_t * pmdp, pte_t * ptep)
{
    pmd_val(*pmdp) = (unsigned long) ptep;
}

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(mm,addr,ptep)	set_pte_at((mm),(addr),(ptep), __pte(0))
#define pmd_none(x)	    (!pmd_val(x))
#define	pmd_bad(x)      ((pmd_val(x) & ~PAGE_MASK))
#define pmd_present(x)	(pmd_val(x))
#define pmd_clear(xp)	do { pmd_val(*(xp)) = 0; } while (0)


#define pte_page(x) (mem_map + (unsigned long)(((pte_val(x)-PAGE_OFFSET) >> PAGE_SHIFT)))
/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte)	{ return pte_val(pte) & _PAGE_READ; }
static inline int pte_write(pte_t pte)	{ return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_dirty(pte_t pte)	{ return pte_val(pte) & _PAGE_MODIFIED; }
static inline int pte_young(pte_t pte)	{ return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_special(pte_t pte) { return 0;}

/* Sameer: A new macro for 2.6 We need a deeper look here. */
static inline int pte_file(pte_t pte)	{ return pte_val(pte) & _PAGE_FILE; }

/* Sameer: Temporary def */
#define PTE_FILE_MAX_BITS	30

/* Sameer: These are useful for non-linear memory mapped files. I am
           postponing the implementation till it actually becomes a blocker.
            We need arch-specific implementation here. Which I am not doing
	    right now. */

#define pgoff_to_pte(x)         __pte(x)

#define pte_to_pgoff(x)		(pte_val(x) >> 2)

#define pte_pfn(pte)		(pte_val(pte) >> PAGE_SHIFT)

#define pfn_pte(pfn,prot)	(__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot)))

#define __pte_index(addr)	(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

/* pte_offset gets a @ptr to PMD entry (PGD in our 2-tier paging system)
   and returns ptr to PTE entry corresponding to @addr
 */
#define pte_offset(dir, address) ((pte_t *) (pmd_page_vaddr(*dir)) + __pte_index(address))

/* No mapping of Page Tables in high mem etc, so following same as above */
#define pte_offset_kernel(dir,addr) pte_offset(dir, addr)
#define pte_offset_map(dir,addr) pte_offset(dir, addr)
#define pte_offset_map_nested(dir,addr) pte_offset(dir, addr)

#define PTE_BIT_FUNC(fn,op) \
static inline pte_t pte_##fn(pte_t pte) { pte_val(pte) op; return pte; }

PTE_BIT_FUNC(wrprotect, &= ~(_PAGE_WRITE));
PTE_BIT_FUNC(mkwrite,   |=  (_PAGE_WRITE));
PTE_BIT_FUNC(mkclean,   &= ~(_PAGE_MODIFIED));
PTE_BIT_FUNC(mkdirty,   |=  (_PAGE_MODIFIED));
PTE_BIT_FUNC(mkold,     &= ~(_PAGE_ACCESSED));
PTE_BIT_FUNC(mkyoung,   |=  (_PAGE_ACCESSED));
PTE_BIT_FUNC(exprotect, &= ~(_PAGE_EXECUTE));
PTE_BIT_FUNC(mkexec,    |=  (_PAGE_EXECUTE));

static inline pte_t pte_mkspecial(pte_t pte) { return pte; }

/* Macro to mark a page protection as uncacheable */
#define pgprot_noncached pgprot_noncached

static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	prot = (prot & ~_PAGE_CACHEABLE);

	return __pgprot(prot);
}

#define mk_pte(page, pgprot)  \
 ({ \
      pte_t pte;  \
      pte_val(pte) = __pa(page_address(page)) + pgprot_val(pgprot); \
      pte; \
 })

static inline pte_t mk_pte_phys(unsigned long physpage, pgprot_t pgprot)
{
	return __pte(physpage | pgprot_val(pgprot));
}

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}

static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
                                pte_t *ptep, pte_t pteval)
{
#ifdef CONFIG_ARC_MMU_SASID
    int sasid;

    /* For now, we are only interested in PTEs for code */
    if ( (pte_val(pteval) & (_PAGE_READ|_PAGE_WRITE|_PAGE_EXECUTE)) ==
                            (_PAGE_READ|_PAGE_EXECUTE)) {

        /* Does this code page belong to a mapped shared library, which has
         * been mapped such that vaddr is cmn across processes.
         * If yes, make a note in PTE that
         *  -it is shared
         *  -it's SASID - (S)hared (A)ddr (S)pace ID - a "handle" to this
         *   cmn-vadd-space instance)
         * That way TLB Miss handlers, which look at the PTE, will have all
         * the info to actually program a shared TLB entry into MMU.
         */
        sasid = mmapcode_find_mm_vaddr(mm, addr);

        if (sasid >= 0) {
            /* PTE now needs to save extra 5 bits SASID which won't fit in
             * orig 1 word-wide PTE (containing PFN and flags).
             * Using a long-long or pte[2] was generating horrible code.
             * So we split the info into 2 parts, each part in a seperate
             * page-table.
             *
             * When allocating page-frame for page table, we allocate double
             * the size, to fit 2 logical tables.
             * First one is the normal, containing normal PTE info, and is
             * the one hooked-up to upper level PGD.
             * The 2nd one, adjecent to first one in memory (by way of alloc),
             * is more-or-less hidden, containing only SASIDs (if at all).
             *
             * A full PTE entry correponds to one word each, in both, at same
             * relative indexes.
             * Given ptr to pte in main table, getting the corresp sibling
             * in other table is simply (ptep + PTRS_IN_PTE*4)
             */

            pte_val(pteval) |= _PAGE_SHARED_CODE;
            set_pte((ptep + PTRS_PER_PTE), sasid);
        }
    }
#endif
    set_pte(ptep, pteval);
}

/* to find an entry in a kernel page-table-directory.
 * All kernel related VM pages are in init's mm.
 */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* to find an entry in a page-table-directory */
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)

#define pgd_offset(mm, addr)	(((mm)->pgd)+pgd_index(addr))

/* Macro to quickly access the PGD entry, utlising the fact that some
 * arch may cache the pointer to Page Directory of "current" task
 * in a MMU register
 *
 * Thus task->mm->pgd (3 pointer dereferences, cache misses etc simply
 * becomes read a register
 *
 * ********CAUTION*******:
 * Kernel code might be dealing with some mm_struct of NON "current"
 * Thus use this macro only when you are certain that "current" is current
 * e.g. when dealing with signal frame setup code etc
 */
#define pgd_offset_fast(mm, addr)\
({ \
    pgd_t *pgd_base = (pgd_t *) read_new_aux_reg(ARC_REG_SCRATCH_DATA0);  \
    pgd_base + pgd_index(addr); \
})

extern int do_check_pgt_cache(int, int);
extern void paging_init(void);

extern char empty_zero_page[PAGE_SIZE];
extern pgd_t swapper_pg_dir[]__attribute__((aligned(PAGE_SIZE)));

void update_mmu_cache(struct vm_area_struct *vma,
				 unsigned long address, pte_t *ptep);

/* Encode swap {type,offset} tuple into PTE
 * We reserve 13 bits for 5-bit @type, keeping bits 12-5 zero, ensuring that
 * both PAGE_FILE and PAGE_PRESENT are zero in a PTE holding swap "identifier"
 */
#define __swp_entry(type,offset)	((swp_entry_t) { \
        ((type) & 0x1f) | ((offset) << 13) })

/* Decode a PTE containing swap "identifier "into constituents */
#define __swp_type(pte_lookalike)	(((pte_lookalike).val) & 0x1f)
#define __swp_offset(pte_lookalike)	((pte_lookalike).val << 13)

/* NOPs, to keep generic kernel happy */
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#include <asm-generic/pgtable.h>

/*
 * remap a physical page `pfn' of size `size' with page protection `prot'
 * into virtual address `from'
 */
#define io_remap_pfn_range(vma,from,pfn,size,prot) \
		remap_pfn_range(vma, from, pfn, size, prot)

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()   do { } while (0)

#endif /* __ASSEMBLY__ */
#endif /* _ARM_ARC_PGTABLE_H */

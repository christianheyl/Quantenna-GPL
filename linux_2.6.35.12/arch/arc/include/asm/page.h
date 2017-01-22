/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_PAGE_H
#define __ASM_ARC_PAGE_H

/* PAGE_SHIFT determines the page size */
#if defined(CONFIG_ARC_PAGE_SIZE_16K)
#define PAGE_SHIFT 14
#elif defined(CONFIG_ARC_PAGE_SIZE_4K)
#define PAGE_SHIFT 12
#else
/* Default 8k
 * done this way (instead of under CONFIG_ARC_PAGE_SIZE_8K) because adhoc
 * user code (busybox appletlib.h) expects PAGE_SHIFT to be defined w/o
 * using the correct uClibc header and in their build our autoconf.h is
 * not available
 */
#define PAGE_SHIFT 13
#endif

#ifdef __ASSEMBLY__
#define PAGE_SIZE       (1 << PAGE_SHIFT)
#define PAGE_OFFSET  	(0x80000000)
#else
#define PAGE_SIZE       (1UL << PAGE_SHIFT)	// 8K
#define PAGE_OFFSET  	(0x80000000UL)		// Kernel starts at 2G onwards
#endif

#define PAGE_MASK       (~(PAGE_SIZE-1))

#ifdef __KERNEL__

#include <asm/bug.h>

#define PHYS_SRAM_OFFSET 0x80000000

#define ARCH_PFN_OFFSET     (CONFIG_LINUX_LINK_BASE >> PAGE_SHIFT)
#define pfn_valid(pfn)      (((pfn) - ARCH_PFN_OFFSET) < max_mapnr)

/* Beware this looks cheap but it is pointer arithmatic
 * so becomes divide by sizeof which is not power of 2
 */
#define __pfn_to_page(pfn)  (mem_map + ((pfn) - ARCH_PFN_OFFSET))
#define __page_to_pfn(page) ((unsigned long)((page) - mem_map) + \
                                ARCH_PFN_OFFSET)

#define page_to_pfn __page_to_pfn
#define pfn_to_page __pfn_to_page

#ifndef __ASSEMBLY__

struct mm_struct;
struct vm_area_struct;
struct page;

extern void copy_page(void *to, void *from);
extern void clear_user_page(void *addr, unsigned long vaddr, struct page *page);
extern void copy_user_page(void *vto, void *vfrom, unsigned long vaddr, struct page *to);

#define get_user_page(vaddr)        __get_free_page(GFP_KERNEL)
#define free_user_page(page, addr)  free_page(addr)

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte_lo; } pte_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef unsigned long pgtable_t;

#define pte_val(x)      ((x).pte_lo)
#define pgd_val(x)      ((x).pgd)
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x)        ((pte_t) { (x) } )
#define __pgd(x)        ((pgd_t) { (x) } )
#define __pgprot(x)     ((pgprot_t) { (x) } )

#else

typedef unsigned long pte_t;
typedef unsigned long pgd_t;
typedef unsigned long pgprot_t;
typedef unsigned long pgtable_t;

#define pte_val(x)      (x)
#define pgd_val(x)	    (x)
#define pgprot_val(x)   (x)

#define __pte(x)        (x)
#define __pgprot(x)     (x)

#endif


/* __pa, __va, virt_to_page
 * ALERT: These macros are deprecated, dont use them
 *
 * These macros have historically been misnamed
 * virt here means link-address/program-address as embedded in object code.
 * So if kernel img is linked at 0x8000_0000 onwards, 0x8010_0000 will be
 * 128th page, and virt_to_page( ) will return the struct page corresp to it.
 * Note that mem_map[ ] is an array of struct page for each page frame in
 * the system.
 */

/* __pa, __va
 * Independent of where linux is linked at, link-addr = physical address
 * So the old macro  __pa = vaddr + PAGE_OFFSET - CONFIG_LINUX_LINK_BASE
 * would have been wrong in case kernel is not at 0x8zs
 */
#define __pa(vaddr)  ((unsigned long)vaddr)
#define __va(paddr)  ((void *)((unsigned long)(paddr)))

#define virt_to_page(kaddr) (mem_map + \
        ((__pa(kaddr) - CONFIG_LINUX_LINK_BASE) >> PAGE_SHIFT))

#define	virt_addr_valid(kaddr)	(((unsigned long)(kaddr) >= PAGE_OFFSET) && \
				((unsigned long)(kaddr) < (unsigned long)high_memory))

#define VALID_PAGE(page)    ((page - mem_map) < max_mapnr)

// Simon Spooner ARC
// Config option to make stack non-executable

#ifdef CONFIG_ARC_STACK_NONEXEC
#define VM_DATA_DEFAULT_FLAGS   (VM_READ | VM_WRITE |  \
                    VM_MAYREAD | VM_MAYWRITE )
#else
#define VM_DATA_DEFAULT_FLAGS   (VM_READ | VM_WRITE | VM_EXEC | \
                    VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)
#endif

#define PAGE_BUG(page) do { \
    BUG(); \
  } while (0)

#endif /* !__ASSEMBLY__ */

/* rtee: check if not using the page_address  macro helps */
#define WANT_PAGE_VIRTUAL   1

#define clear_page(paddr)  memset((unsigned int *)(paddr), 0, PAGE_SIZE)

#include <asm-generic/getorder.h>

#endif  /* __KERNEL__ */

#endif /* __ASM_ARC_PAGE_H */

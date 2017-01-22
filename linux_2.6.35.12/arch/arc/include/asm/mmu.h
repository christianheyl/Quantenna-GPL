/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: May 2011
 *  -Folded PAGE_PRESENT (used by VM) and PAGE_VALID (used by MMU) into 1.
 *     They are semantically the same although in different contexts
 *     VALID marks a TLB entry exists and it will only happen if PRESENT
 *  - Utilise some unused free bits to confine PTE flags to 12 bits
 *     This is a must for 4k pg-sz
 *
 * vineetg: Mar 2011 - changes to accomodate MMU TLB Page Descriptor mods
 *  -TLB Locking never really existed, except for initial specs
 *  -SILENT_xxx not needed for our port
 *  -Per my request, MMU V3 changes the layout of some of the bits
 *     to avoid a few shifts in TLB Miss handlers.
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_MMU_H
#define _ASM_ARC_MMU_H

/* Arch specific mmu context. We store the allocated ASID in here.
 */
#ifndef __ASSEMBLY__
#include <asm/unaligned.h>
typedef struct {
    unsigned long asid;         /* Pvt Addr-Space ID for mm */
#ifdef CONFIG_ARC_TLB_DBG
    struct task_struct *tsk;
#endif
#ifdef CONFIG_MMAP_CODE_CMN_VADDR
    unsigned long sasid;        /* bitmap of Shared Addr-space IDs */
#endif
} mm_context_t;

#endif

/* Page Table flags: some implemented by MMU (H), others emulated (S)
 * However not all (H) represent the exact bit value in hardware.
 * Since these are primarily for Linux vm, they need to be unique
 * e.g. MMU v2: K_READ bit is 8 and so is GLOBAL (possible becoz they live in
 *      seperate PD0 and PD1, which combined forms a translation entry)
 *      while for PTE perspective, they are 8 and 9 respectively
 * with MMU v3: Most bits (except SHARED) represent the exact hardware pos
 *      (saves some bit shift ops in TLB Miss hdlrs)
 */

#if (CONFIG_ARC_MMU_VER <= 2)

#define _PAGE_ACCESSED      (1<<1)  /* Page is accesses (S) */
#define _PAGE_CACHEABLE     (1<<2)  /* Page is cached (H) */
#define _PAGE_EXECUTE       (1<<3)  /* Page has user execute perm (H) */
#define _PAGE_WRITE         (1<<4)  /* Page has user write perm (H) */
#define _PAGE_READ          (1<<5)  /* Page has user read perm (H) */
#define _PAGE_K_EXECUTE     (1<<6)  /* Page has kernel execute perm (H) */
#define _PAGE_K_WRITE       (1<<7)  /* Page has kernel write perm (H) */
#define _PAGE_K_READ        (1<<8)  /* Page has kernel perm (H) */
#define _PAGE_GLOBAL        (1<<9)  /* Page is global (H) */
#define _PAGE_MODIFIED      (1<<10) /* Page modified (dirty) (S) */
#define _PAGE_FILE          (1<<10) /* page cache/ swap (S) */
#define _PAGE_VALID         (1<<11) /* Page is valid (H) */
#define _PAGE_PRESENT       _PAGE_VALID  /* Page present in memory (S)*/

#else

/* PD1 */
#define _PAGE_CACHEABLE     (1<<0)  /* Page is cached (H) */
#define _PAGE_EXECUTE       (1<<1)  /* Page has user execute perm (H) */
#define _PAGE_WRITE         (1<<2)  /* Page has user write perm (H) */
#define _PAGE_READ          (1<<3)  /* Page has user read perm (H) */
#define _PAGE_K_EXECUTE     (1<<4)  /* Page has kernel execute perm (H) */
#define _PAGE_K_WRITE       (1<<5)  /* Page has kernel write perm (H) */
#define _PAGE_K_READ        (1<<6)  /* Page has kernel perm (H) */
#define _PAGE_ACCESSED      (1<<7)  /* Page is accesses (S) */

/* PD0 */
#define _PAGE_GLOBAL        (1<<8)  /* Page is global (H) */
#define _PAGE_VALID         (1<<9)  /* Page is valid (H) */
#define _PAGE_PRESENT       _PAGE_VALID /* Page present in memory (S)*/
#define _PAGE_SHARED_CODE   (1<<10) /* Shared Code page with cmn vaddr
                                       usable for shared TLB entries (H) */

#define _PAGE_MODIFIED      (1<<11) /* Page modified (dirty) (S) */
#define _PAGE_FILE          (1<<12) /* page cache/ swap (S) */

#define _PAGE_SHARED_CODE_H (1<<31) /* Hardware counterpart of above */
#endif


/* Kernel allowed all permissions for all pages */
#define _KERNEL_PAGE_PERMS  (_PAGE_K_EXECUTE | _PAGE_K_WRITE | _PAGE_K_READ)

#ifdef  CONFIG_ARC700_CACHE_PAGES
#define _PAGE_DEF_CACHEABLE _PAGE_CACHEABLE
#else
#define _PAGE_DEF_CACHEABLE (0)
#endif

/* Helper for every "user" page
 * -kernel can R/W/X
 * -by default cached, unless config otherwise
 * -present in memory
 */
#define ___DEF (_PAGE_PRESENT | _KERNEL_PAGE_PERMS | _PAGE_DEF_CACHEABLE)

/* Set of bits not changed in pte_modify */
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_MODIFIED)

/* More Abbrevaited helpers */
#define PAGE_U_NONE     __pgprot(___DEF)
#define PAGE_U_R        __pgprot(___DEF | _PAGE_READ)
#define PAGE_U_W_R      __pgprot(___DEF | _PAGE_READ | _PAGE_WRITE)
#define PAGE_U_X_R      __pgprot(___DEF | _PAGE_READ | _PAGE_EXECUTE)
#define PAGE_U_X_W_R    __pgprot(___DEF | _PAGE_READ | _PAGE_WRITE | \
                                        _PAGE_EXECUTE)

/* While kernel runs out of unstrslated space, vmalloc/modules use a chunk of
 * kernel vaddr space - visible in all addr spaces, but kernel mode only
 * Thus Global, all-kernel-access, no-user-access, cached
 */
#define PAGE_KERNEL          __pgprot(___DEF | _PAGE_GLOBAL)

/* ioremap */
#define PAGE_KERNEL_NO_CACHE __pgprot(_PAGE_PRESENT| _KERNEL_PAGE_PERMS |\
                                         _PAGE_GLOBAL)

/****************************************************************
 * Mapping of vm_flags (Generic VM) to PTE flags (arch specific)
 *
 * Certain cases have 1:1 mapping
 *  e.g. __P101 means VM_READ, VM_EXEC and !VM_SHARED
 *       which directly corresponds to  PAGE_U_X_R
 *
 * Other rules which cause the divergence from 1:1 mapping
 *
 *  1. Although ARC700 can do exclusive execute/write protection (meaning R
 *     can be tracked independet of X/W unlike some other CPUs), still to
 *     keep things consistent with other archs:
 *      -Write implies Read:   W => R
 *      -Execute implies Read: X => R
 *
 *  2. Pvt Writable doesn't have Write Enabled initially: Pvt-W => !W
 *     This is to enable COW mechanism
 */
        /* xwr */
#define __P000  PAGE_U_NONE
#define __P001  PAGE_U_R
#define __P010  PAGE_U_R        // Pvt-W => !W
#define __P011  PAGE_U_R        // Pvt-W => !W
#define __P100  PAGE_U_X_R      // X => R
#define __P101  PAGE_U_X_R
#define __P110  PAGE_U_X_R      // Pvt-W => !W and X => R
#define __P111  PAGE_U_X_R      // Pvt-W => !W

#define __S000  PAGE_U_NONE
#define __S001  PAGE_U_R
#define __S010  PAGE_U_W_R      // W => R
#define __S011  PAGE_U_W_R
#define __S100  PAGE_U_X_R      // X => R
#define __S101  PAGE_U_X_R
#define __S110  PAGE_U_X_W_R    // X => R
#define __S111  PAGE_U_X_W_R

#endif  /* _ASM_ARC_MMU_H */


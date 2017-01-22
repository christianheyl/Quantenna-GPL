/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_TLB_H
#define _ASM_ARC_TLB_H

#ifdef __KERNEL__

/* Build option For chips with Metal Fix */
/* #define METAL_FIX  1 */
#define METAL_FIX  0

/*************************************
 * Basic TLB Commands
 ************************************/
#define TLBWrite    0x1
#define TLBRead     0x2
#define TLBGetIndex 0x3
#define TLBProbe    0x4

/*************************************
 * New Cmds because of MMU Changes
 *************************************/

#if (CONFIG_ARC_MMU_VER >=2)

#define TLBWriteNI  0x5     // JH special -- write JTLB without inv uTLBs
#define TLBIVUTLB   0x6     //JH special -- explicitly inv uTLBs

#elif (METAL_FIX==1)   /* Metal Fix: Old MMU but a new Cmd */

#define TLBWriteNI  TLBWrite    // WriteNI doesn't exist on this H/w
#define TLBIVUTLB   0x5         // This is the only additional cmd

#else /* MMU V1 */

#undef TLBWriteNI       // These cmds don't exist on older MMU
#undef TLBIVUTLB

#endif

#define PTE_BITS_IN_PD0    (_PAGE_GLOBAL | _PAGE_VALID)
#define PTE_BITS_IN_PD1    (PAGE_MASK | \
                             _PAGE_CACHEABLE | \
                             _PAGE_EXECUTE | _PAGE_WRITE | _PAGE_READ | \
                             _PAGE_K_EXECUTE | _PAGE_K_WRITE | _PAGE_K_READ)

#ifndef __ASSEMBLY__

void arc_mmu_init(void);
void tlb_find_asid(unsigned int asid);
void __init read_decode_mmu_bcr(void);

#define tlb_flush(tlb) local_flush_tlb_mm((tlb)->mm)
#define __tlb_remove_tlb_entry(tlb, ptep, address) do { } while (0)

/* This pair is called at time of munmap/exit to flush cache and TLB entries
 * for mappings bring torn down.
 * 1) cache-flush part -implemented via tlb_start_vma( ) can be NOP (for now)
 *    as we don't support aliasing configs in our VIPT D$.
 * 2) tlb-flush part - implemted via tlb_end-vma( ) can be NOP as well-
 *    albiet for difft reasons - its better handled by moving to new ASID
 *
 * Note, read http://lkml.org/lkml/2004/1/15/6
 */
#define tlb_start_vma(tlb, vma)
#define tlb_end_vma(tlb, vma)


static inline void enter_lazy_tlb(struct mm_struct *mm,
                    struct task_struct *tsk)
{
}

#include <linux/pagemap.h>
#include <asm-generic/tlb.h>

#else
#include <asm/tlb-mmu1.h>
#endif


#endif

#endif  /* _ASM_ARC_TLB_H */

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

extern void local_flush_tlb_page(struct vm_area_struct *vma,
    unsigned long page);


extern void local_flush_tlb_range(struct vm_area_struct *vma,
    unsigned long start, unsigned long end);

extern void local_flush_tlb_kernel_range(unsigned long start,
    unsigned long end);

extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *);


#define flush_tlb_range(vma,vmaddr,end)     local_flush_tlb_range(vma, vmaddr, end)
#define flush_tlb_page(vma,page)            local_flush_tlb_page(vma, page)
#define flush_tlb_kernel_range(vmaddr,end)  local_flush_tlb_kernel_range(vmaddr, end)
#define flush_tlb_all()                     local_flush_tlb_all()
#define flush_tlb_mm(mm)                    local_flush_tlb_mm(mm)



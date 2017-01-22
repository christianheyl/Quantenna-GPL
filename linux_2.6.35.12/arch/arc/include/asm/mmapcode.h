/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MMAP_CODE_H__
#define __MMAP_CODE_H__

#if defined(__KERNEL__) && !defined(__ASSEMBLY__)

#ifdef CONFIG_MMAP_CODE_CMN_VADDR

#include <linux/mm_types.h>
#include <linux/genalloc.h>
#include <linux/file.h>     /* fget */
#include <linux/fs.h>       /* inode->i_ino */
#include <asm/mmu.h>        /* mm_context_t */
#include <asm/mman.h>


void arc_unmap_area(struct mm_struct *mm, unsigned long start,
        unsigned long len);

void __init mmapcode_space_init(void);

int mmapcode_alloc_vaddr(struct file *filp, unsigned long pgoff,
        unsigned long len, unsigned long *vaddr);
int mmapcode_enab_vaddr(struct file *filp, unsigned long pgoff,
        unsigned long len, unsigned long vaddr);
int mmapcode_free(struct mm_struct *mm, unsigned long vaddr,
        unsigned long len);
int mmapcode_free_all(struct mm_struct *mm);
int mmapcode_find_mm_vaddr(struct mm_struct *mm, unsigned long vaddr);

static inline
void mmapcode_task_subscribe(struct mm_struct *mm, int mmapcode_id)
{
    mm->context.sasid |= (1 << (mmapcode_id & 0x1F));
}

static inline
void mmapcode_task_unsubscribe(struct mm_struct *mm, int mmapcode_id)
{
    mm->context.sasid &= ~(1 << (mmapcode_id & 0x1F));
}

static inline
int is_mmapcode_task_subscribed(struct mm_struct *mm, int mmapcode_id)
{
    return (mm->context.sasid & (1 << (mmapcode_id & 0x1F)))? 1:0;
}

static inline
int is_any_mmapcode_task_subscribed(struct mm_struct *mm)
{
    return (mm->context.sasid);
}

#else

#define mmapcode_space_init()    {}

#endif

#endif

#endif

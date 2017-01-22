/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: May 2011
 *  -Refactored get_new_mmu_context( ) to only handle live-mm.
 *   retiring-mm handled in other hooks
 *
 * vineetg: April 2011
 *  -CONFIG_ARC_MMU_SASID: support for ARC MMU Shared Address spaces
 *     activate_mm() and switch_mm() to setup MMU SASID reg with task's
 *     current sasid subscriptions
 *
 * Vineetg: March 25th, 2008: Bug #92690
 *  -Major rewrite of Core ASID allocation routine get_new_mmu_context
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_MMU_CONTEXT_H
#define _ASM_ARC_MMU_CONTEXT_H

#include <asm/arcregs.h>
#include <asm/tlb.h>

#ifndef CONFIG_MMAP_CODE_CMN_VADDR
#include <asm-generic/mm_hooks.h>
#else
extern void arch_dup_mmap(struct mm_struct *oldmm, struct mm_struct *mm);
extern void arch_exit_mmap(struct mm_struct *mm);
#endif


#define FIRST_ASID  0
#define MAX_ASID    255 /* ARC 700 8 bit PID field in PID Aux reg */
#define NUM_ASID    ((MAX_ASID - FIRST_ASID) + 1 )
/* We use this to indicate that no ASID has been allocated to a mmu context */
#define NO_ASID     (MAX_ASID + 1)

/* ASID to mm struct mapping */
extern struct mm_struct *asid_mm_map[ NUM_ASID + 1 ];

extern int asid_cache;

/* Get a new mmu context or a hardware PID/ASID to work with.
 * If PID rolls over flush cache and tlb.
 */
static inline void
get_new_mmu_context(struct mm_struct *mm)
{
    struct mm_struct *prev_owner;
    unsigned long flags;

    local_irq_save(flags);

    /* Relinquish the currently owned ASID (if any).
     * Doing unconditionally saves a cmp-n-branch;
     * for invalid asid, the array index is still valid
     */
    asid_mm_map[mm->context.asid] = (struct mm_struct *) NULL;

    /* move to new asid */
    if ( ++asid_cache > MAX_ASID) {  /* asid roll-over */
        asid_cache = FIRST_ASID;
        flush_tlb_all();
    }

    /* Is new ASID already owned by some-one else (we are stealing it).
     * If yes, let the orig owner be aware of this fact, so that when it runs,
     * it asks for a brand new ASID. This would only happen for a long-lived
     * task with asid from prev allocation cycle (before asid roll-over).
     *
     * This might look wrong - if we are re-using some other task's ASID,
     * won't we use it's stale TLB entries too. Actually switch_mm( ) takes
     * care of such a case: it ensures that a task with asid from prev alloc
     * cycle, when scheduled will refresh it's ASID - see switch_mm( ) below
     * The scenario of stealing here will only happen if that task didn't get
     * a chance to refresh it's asid.
     */
    if ( (prev_owner = asid_mm_map[asid_cache]) ) {
        prev_owner->context.asid = NO_ASID;
    }

    /* Assign new ASID to tsk */
    asid_mm_map[asid_cache] = mm;
    mm->context.asid = asid_cache;

#ifdef  CONFIG_ARC_TLB_DBG
    printk ("ARC_TLB_DBG: NewMM=0x%x OldMM=0x%x task_struct=0x%x Task: %s,"
            " pid:%u, assigned asid:%lu\n",
            (unsigned int)mm, (unsigned int)prev_owner,
            (unsigned int)(mm->context.tsk), (mm->context.tsk)->comm,
            (mm->context.tsk)->pid, mm->context.asid);

    /* This is to double check TLB entries already exist for
     *    this newly allocated ASID
     */
    tlb_find_asid(asid_cache);

#endif

    write_new_aux_reg(ARC_REG_PID, asid_cache|MMU_ENABLE);

    local_irq_restore(flags);
}

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */
static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
    mm->context.asid = NO_ASID;
#ifdef CONFIG_ARC_TLB_DBG
    mm->context.tsk = tsk;
#endif  /* CONFIG_ARC_TLB_DBG */
    return 0;
}

/* Switch the mm context */
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
                             struct task_struct *tsk)
{
    /* PGD cached in MMU reg to avoid 3 mem lookups: task->mm->pgd */
    write_new_aux_reg(ARC_REG_SCRATCH_DATA0, next->pgd);

    /* Allocate a new ASID if task doesn't have a valid one.
     * This could happen if this task never had an asid (fresh after fork) or
     * it's ASID was stolen - past an asid roll-over. Also in a special case,
     * even if the ASID was valid, we allocate a new ASID, if this task is
     * running for the first time afer an ASID rollover.
     * This is part of simplistic asid allocation algorithm - which allows
     * asid "stealing" - If asid-cache is at "x-1", a new req will allocate "x"
     * evenif "x" is already allocated. The *IMP* thing is stolen asid's TLB
     * entries are not flushed - which can cause stale TLB entry use.
     * The problem case is: A long-lived task has asid "x", asid_cache rolls
     * over from 255 to 0, and later this task gets scheduled - creating TLB
     * entries with "x". A little later, while asid_cache is "x-1" a task
     * needs a new ASID, and steals this "x" while TLB entries already exist.
     * To avoid this, we don't allow a ASID allocated in prev cycle to be used
     * past a roll-over.
     *
     * Both the non-alloc scenario and first-use-after-rollover can be
     * detected using the single condition below - since NO_ASID = 256
     * while asid_cache is always a valid asid value (0-255).
     */
    if (next->context.asid > asid_cache) {
        get_new_mmu_context(next);
    } else {
        // XXX: This will never happen given the chks above
        // BUG_ON(next->context.asid > MAX_ASID);
        write_new_aux_reg(ARC_REG_PID, next->context.asid|MMU_ENABLE);
    }

#ifdef CONFIG_ARC_MMU_SASID
    {
        unsigned int tsk_sasids;

        if ((tsk_sasids = is_any_mmapcode_task_subscribed(next))) {
            write_new_aux_reg(ARC_REG_SASID, tsk_sasids);
        }
    }
#endif

}

static inline void destroy_context(struct mm_struct *mm)
{
    unsigned long flags;

    local_irq_save(flags);

    asid_mm_map[mm->context.asid] = NULL;
    mm->context.asid = NO_ASID;

    local_irq_restore(flags);
}

/* it seemed that deactivate_mm( ) is a reasonable place to do book-keeping
 * for retiring-mm. However destroy_context( ) still needs to do that because
 * between mm_release( ) = >deactive_mm( ) and
 * mmput => .. => __mmdrop( ) => destroy_context( )
 * there is a good chance that task gets sched-out/in, making it's asid valid
 * again (this teased me for a whole day).
 */
#define deactivate_mm(tsk,mm)   do { } while (0)

static inline void
activate_mm (struct mm_struct *prev, struct mm_struct *next)
{
    write_new_aux_reg(ARC_REG_SCRATCH_DATA0, next->pgd);

    /* Unconditionally get a new ASID */
    get_new_mmu_context(next);

#ifdef CONFIG_ARC_MMU_SASID
    BUG_ON((is_any_mmapcode_task_subscribed(next)));
#endif
}

#endif  /* __ASM_ARC_MMU_CONTEXT_H */

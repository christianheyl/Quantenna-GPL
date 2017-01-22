/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Oct 2009
 *  No need for ARC specific thread_info allocator (kmalloc/free). This is
 *  anyways one page allocation, thus slab alloc can be short-circuited and
 *  the generic version (get_free_page) would be loads better.
 *
 * Sameer Dhavale: Codito Technologies 2004
 */

/*
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#include <asm/page.h>

#ifdef CONFIG_16KSTACKS
#define THREAD_SIZE_ORDER 2
#define THREAD_SIZE       (PAGE_SIZE << 1)
#else
#define THREAD_SIZE_ORDER 1
#define THREAD_SIZE       PAGE_SIZE
#endif

#ifndef __ASSEMBLY__

#include <linux/thread_info.h>


typedef unsigned long mm_segment_t;     /* domain register  */

/*
 * low level task data that entry.S needs immediate access to
 * - this struct should fit entirely inside of one cache line
 * - this struct shares the supervisor stack pages
 * - if the contents of this structure are changed, the assembly constants
 *   must also be changed
 */
struct thread_info {
    unsigned long           flags;      /* low level flags */
    int                     preempt_count;  /* 0 => preemptable, <0 => BUG */
    struct task_struct      *task;      /* main task structure */
    mm_segment_t            addr_limit; /* thread address space */
    struct exec_domain      *exec_domain;   /* execution domain */
    __u32                   cpu;        /* current CPU */
    unsigned long           thr_ptr;    /* TLS ptr */
    struct restart_block    restart_block;
};

/*
 * macros/functions for gaining access to the thread information structure
 *
 * preempt_count needs to be 1 initially, until the scheduler is functional.
 */
#define INIT_THREAD_INFO(tsk)           \
{                       \
    .task       = &tsk,         \
    .exec_domain    = &default_exec_domain, \
    .flags      = 0,            \
    .cpu        = 0,            \
    .preempt_count  = 1,            \
    .addr_limit = KERNEL_DS,        \
    .restart_block  = {         \
        .fn = do_no_restart_syscall,    \
    },                  \
}

#define init_thread_info    (init_thread_union.thread_info)
#define init_stack          (init_thread_union.stack)

#ifndef ASM_OFFSETS_C

#include <asm/asm-offsets.h>
#include <plat_memmap.h>    // Peripherals Memory Map

static inline __attribute_const__ struct thread_info *current_thread_info(void)
{
    register unsigned long sp asm ("sp");

#ifdef CONFIG_ARCH_RUBY_SRAM_IRQ_STACK
    if (unlikely((sp >= RUBY_SRAM_BEGIN) && (sp < RUBY_SRAM_BEGIN + RUBY_SRAM_SIZE))) {
        return *((struct thread_info **)((void*)curr_arc + TASK_THREAD_INFO));
    }
#endif

    return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}

#else

static inline __attribute_const__ struct thread_info *current_thread_info(void)
{
    return NULL;
}

#endif // #ifndef ASM_OFFSETS_C

#else /*  __ASSEMBLY__ */

#define GET_CURR_THR_INFO_FROM_SP(reg) \
	and reg, sp, ~(THREAD_SIZE - 1)

#endif  /* !__ASSEMBLY__ */


#define PREEMPT_ACTIVE      0x10000000

/*
 * thread information flags
 * - these are process state flags that various assembly files may need to
 *   access
 * - pending work-to-be-done flags are in LSW
 * - other flags in MSW
 */
#define TIF_NOTIFY_RESUME   1   /* resumption notification requested */
#define TIF_SIGPENDING      2   /* signal pending */
#define TIF_NEED_RESCHED    3   /* rescheduling necessary */
#define TIF_SYSCALL_AUDIT   4   /* syscall auditing active */
#define TIF_SECCOMP     5   /* secure computing */
#define TIF_RESTORE_SIGMASK 9   /* restore signal mask in do_signal() */
#define TIF_USEDFPU     16  /* FPU was used by this task this quantum (SMP) */
#define TIF_POLLING_NRFLAG  17  /* true if poll_idle() is polling TIF_NEED_RESCHED */
#define TIF_MEMDIE      18
#define TIF_FREEZE      19
#define TIF_STACK_REPLACED  30	/* replace_stack has been called; skip freeing thread pages */
#define TIF_SYSCALL_TRACE   31  /* syscall trace active */

#define _TIF_SYSCALL_TRACE  (1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME  (1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING     (1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED   (1<<TIF_NEED_RESCHED)
#define _TIF_SYSCALL_AUDIT  (1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP        (1<<TIF_SECCOMP)
#define _TIF_RESTORE_SIGMASK    (1<<TIF_RESTORE_SIGMASK)
#define _TIF_USEDFPU        (1<<TIF_USEDFPU)
#define _TIF_POLLING_NRFLAG (1<<TIF_POLLING_NRFLAG)
#define _TIF_FREEZE         (1<<TIF_FREEZE)
#define _TIF_STACK_REPLACED (1<<TIF_STACK_REPLACED)

/* work to do on interrupt/exception return */
#define _TIF_WORK_MASK      (0x0000ffef & ~_TIF_SECCOMP)
/* work to do on any return to u-space */
#define _TIF_ALLWORK_MASK   (0x8000ffff & ~_TIF_SECCOMP)

#endif /* __KERNEL__ */

#endif /* _ASM_THREAD_INFO_H */

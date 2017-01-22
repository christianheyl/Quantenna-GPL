/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: March 2010 : local_irq_save( ) optimisation
 *  -Remove explicit mov of current status32 into reg, that is not needed
 *  -Use BIC  insn instead of INVERTED + AND
 *  -Conditionally disable interrupts (if they are not enabled, don't disable)
 *
 * Vineetg: July 2009
 *  -Improved xchg implementation by tweaking reg contraints (+r) so that
 *   unnecessary glue code is not generated
 *
 * Vineetg: Mar 2009 (Critical look at file)
 *  -Improved local_irq_save() and friends to not use r20 (CALLLEE Saved)
 *      but let the compiler do register allocation
 *  -[mask|unmask]_interrupt( ) need not do inline IRQ disable as caller holds
 *      spin_lock_irqsave( ) before invoking them
 *
 * Vineetg: Mar 2009 (Supporting 2 levels of Interrupts)
 *  -irqs_enabled( ) need to consider both L1 and L2
 *  -local_irq_enable shd be cognizant of current IRQ state
 *    and must not re-enable lower priorty IRQs
 *   It is lot more involved now and needs to be written in "C"
 *
 * Vineetg: Feb 2009
 *  -schedular hook prepare_arch_switch( ) is now macro instead of inline func
 *   so that builtin_ret_addr( ) can correctly identify the caller of schdule()
 *
 * Vineetg: Oct 3rd 2008
 *  -Got rid of evil cli()/sti() from architecture code
 *
 * Amit Bhor, Sameer Dhavale:  Codito Technologies 2004
 */

#ifndef __ASM_ARC_SYSTEM_H
#define __ASM_ARC_SYSTEM_H

#include <asm/ptrace.h>

#ifndef __ASSEMBLY__

#ifdef __KERNEL__

/******************************************************************
 * Atomically Exchange memory with a register
 ******************************************************************/

static inline unsigned long __xchg_32(unsigned long with, volatile void *ptr)
{
    /* On ARC700, "ex" is inherently atomic so don't need IRQ disabling */

    __asm__ __volatile__ (
                " ex  %0, [%1]"
                  : "+r" (with)
                  : "r"(ptr)
                  : "memory" );

    return (with);
}

/* This function doesn't exist, so that for non supported xchg (64 bits)
 * we will get link errors.
 * shamelessly copied from MIPS
 */
extern void __xchg_bad_pointer(void);

static inline unsigned long __xchg(unsigned long val, volatile void *ptr,
                                        int size)
{
    switch (size) {
        case 4:
            return __xchg_32(val, ptr);
        // TODO-vineetg: 64 bit atomic xchg go here
    }
    __xchg_bad_pointer();
    return val;
}

#define xchg(ptr, with) \
  ((__typeof__ (*(ptr)))__xchg ((unsigned long)(with), (ptr), sizeof (*(ptr))))

/******************************************************************
 * Barriers
 ******************************************************************/

//TODO-vineetg: Need to see what this does, dont we need sync anywhere
#define mb() __asm__ __volatile__ ("" : : : "memory")
#define rmb() mb()
#define wmb() mb()
#define set_mb(var, value)  do { var = value; mb(); } while (0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

/* TODO-vineetg verify the correctness of macros here */
#ifdef CONFIG_SMP
#define smp_mb()        mb()
#define smp_rmb()       rmb()
#define smp_wmb()       wmb()
#else
#define smp_mb()        barrier()
#define smp_rmb()       barrier()
#define smp_wmb()       barrier()
#endif

#define smp_read_barrier_depends()      do { } while(0)

/******************************************************************
 * Arch Depenedent Context Switch Macro  called by sched
 * This in turn calls the regfile switching macro __switch_to ( )
 ******************************************************************/
struct task_struct; // to prevent cyclic dependencies

/* switch_to macro based on the ARM implementaion */
extern struct task_struct *__switch_to(struct task_struct *prev,
                                    struct task_struct *next);

#ifdef CONFIG_ARCH_ARC_FPU

#define HW_BUG_101581

#ifdef HW_BUG_101581

extern void fpu_save_restore(struct task_struct *prev, struct task_struct *next);

#define ARC_FPU_PREV(p,n)   fpu_save_restore(p,n)
#define ARC_FPU_NEXT(t)

#else  /* !HW_BUG_101581 */

extern void inline fpu_save(struct task_struct *tsk);
extern void inline fpu_restore(struct task_struct *tsk);

#define ARC_FPU_PREV(p,n)   fpu_save(p)
#define ARC_FPU_NEXT(t)     fpu_restore(t)

#endif  /* !HW_BUG_101581 */

#else  /* !CONFIG_ARCH_ARC_FPU */

#define ARC_FPU_PREV(p,n)
#define ARC_FPU_NEXT(t)

#endif   /* !CONFIG_ARCH_ARC_FPU */

#define switch_to(prev, next, last)         \
{                                           \
    do {                                    \
        ARC_FPU_PREV(prev, next);           \
        last = __switch_to( prev, next);    \
        ARC_FPU_NEXT(next);                 \
        mb();                               \
                                            \
    }while (0);                             \
}

/* Hook into Schedular to be invoked prior to Context Switch
 *  -If ARC H/W profiling enabled it does some stuff
 *  -If event logging enabled it takes a event snapshot
 *
 *  Having a funtion would have been cleaner but to get the correct caller
 *  (from __builtin_return_address) it needs to be inline
 */

/* Things to do for event logging prior to Context switch */
#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
#define PREP_ARCH_SWITCH_ACT1(next)                                 \
do {                                                                \
    if (next->mm)                                                   \
        take_snap(SNAP_PRE_CTXSW_2_U,                               \
                      (unsigned int) __builtin_return_address(0),   \
                      current_thread_info()->preempt_count);        \
    else                                                            \
        take_snap(SNAP_PRE_CTXSW_2_K,                               \
                      (unsigned int) __builtin_return_address(0),   \
                      current_thread_info()->preempt_count);        \
}                                                                   \
while(0)
#else
#define PREP_ARCH_SWITCH_ACT1(next)
#endif


/* Things to do for hardware based profiling prior to Context Switch */
#ifdef CONFIG_ARC_PROFILING
extern void arc_ctx_callout(struct task_struct *next);
#define PREP_ARCH_SWITCH_ACT2(next)    arc_ctx_callout(next)
#else
#define PREP_ARCH_SWITCH_ACT2(next)
#endif

/* This def is the one used by schedular */
#define prepare_arch_switch(next)   \
do {                                \
    PREP_ARCH_SWITCH_ACT1(next);     \
    PREP_ARCH_SWITCH_ACT2(next);    \
}                                   \
while(0)


/******************************************************************
 * Miscll stuff
 ******************************************************************/

/*
 * On SMP systems, when the scheduler does migration-cost autodetection,
 * it needs a way to flush as much of the CPU's caches as possible.
 *
 * TODO: fill this in!
 */
static inline void sched_cacheflush(void)
{
}

static inline unsigned long arch_align_stack(unsigned long sp)
{
    //unsigned long orig_sp = sp;

#ifdef CONFIG_ARCH_ARC_SPACE_RND
    /* ELF loader sets this flag way early.
     * So no need to check for multiple things like
     *   !(current->personality & ADDR_NO_RANDOMIZE)
     *   randomize_va_space
     */
    if (current->flags & PF_RANDOMIZE) {

        /* Stack grows down for ARC */
		sp -= get_random_int() & ~PAGE_MASK;
    }
#endif

    sp &= ~0xF;  // always align stack to 16 bytes

    //printk("RAND: SP orig %x rnd %x\n", orig_sp, sp);
    return sp;
}

/******************************************************************
 * Piggyback stuff
 * #defines/headers to be made avail to rest of code w/o explicit include
 * e.g. to call event log macros from any kernel file w/o including
 *     eventlog.h in that file
 ******************************************************************/


#include <plat_memmap.h>    // Peripherals Memory Map
#include <asm/event-log.h>  // event log from "C"

void show_stacktrace(struct task_struct *tsk, struct pt_regs *regs);
void raw_printk(const char *str, unsigned int num);
void raw_printk5(const char *str, unsigned int n1, unsigned int n2,
                    unsigned int n3, unsigned int n4);

/******************************************************************
 * printk calls in __init code, so that their literal strings go into
 * .init.rodata (which gets reclaimed) instead of in .rodata
 ******************************************************************/
#define INIT_PRINT 0

#if (INIT_PRINT == 2)
#define printk_init(fmt, args...)   printk(fmt, ## args)
#elif (INIT_PRINT == 1)
#define printk_init(fmt, args...)
#else
#define printk_init(fmt, args...)                   \
({                                                  \
    static const __initconst char __fmt[] = fmt;    \
    printk(__fmt, ## args);                         \
})
#endif

#endif /*__KERNEL__*/

#else  /* !__ASSEMBLY__ */

#include <asm/event-log.h>  // event log from Assembly


#endif /* __ASSEMBLY__ */


#endif /* ASM_ARC_SYSTEM_H */

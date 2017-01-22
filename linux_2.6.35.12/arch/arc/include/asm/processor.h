/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: March 2009
 *  -Implemented task_pt_regs( )
 *
 * Amit Bhor, Sameer Dhavale, Ashwin Chaugule: Codito Technologies 2004
 */

#ifndef __ASM_ARC_PROCESSOR_H
#define __ASM_ARC_PROCESSOR_H

#ifdef __KERNEL__

//#include <linux/linkage.h>
#include <asm/page.h>       /* for PAGE_OFFSET  */
#include <asm/arcregs.h>    /* for STATUS_E1_MASK et all */

/* Most of the architectures seem to be keeping some kind of padding between
 * userspace TASK_SIZE and PAGE_OFFSET. i.e TASK_SIZE != PAGE_OFFSET.
 * I'm not sure why it is so. We will keep this padding for now and remove it
 * later if not required.
 */
#define USER_KERNEL_GUTTER    0x01000000

/* User address space:
 * On ARC700, CPU allows the entire lower half of 32 bit address space to be
 * translated. Thus potentially 2G (0 - 0x7FFF_FFFF) could be user virt addr space.
 * However we steal some addr space for kernel virtual memory and others
 * and another 16M is gutter betn user/kernel spaces
 * Thus user virtual addr space is 0 - (VMALLOC_START - USER_KERNEL_GUTTER - 1)
 */
#define TASK_SIZE   (VMALLOC_START - USER_KERNEL_GUTTER)

#define STACK_TOP_MAX   TASK_SIZE
#define STACK_TOP       TASK_SIZE


/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE      (TASK_SIZE / 3)

#ifndef __ASSEMBLY__

/* For mmap randomisation and Page coloring for share code pages */
#define HAVE_ARCH_PICK_MMAP_LAYOUT

/* Arch specific stuff which needs to be saved per task.
 * However these items are not so important so as to earn a place in
 * struct thread_info
 */
struct thread_struct {
    unsigned long   ksp;            /* kernel mode stack pointer */
    unsigned long   callee_reg;     /* pointer to callee regs */
    unsigned long   fault_address;  /* dbls as brkpt holder as well */
    unsigned long   cause_code;     /* Exception Cause Code (ECR) */
#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    unsigned long   user_r25;
#endif
#ifdef CONFIG_ARCH_ARC_FPU
    struct arc_fpu  fpu;
#endif
};

#define INIT_THREAD  {                          \
    .ksp = sizeof(init_stack) + (unsigned long) init_stack, \
}

/* Forward declaration, a strange C thing */
struct task_struct;

/*
 * Return saved PC of a blocked thread.
 */
unsigned long thread_saved_pc(struct task_struct *t);

#define task_pt_regs(p) \
	((struct pt_regs *)(THREAD_SIZE - 4 + (void *)task_stack_page(p)) - 1)

/* Free all resources held by a thread. */
#define release_thread(thread) do { } while(0)

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk)    do { } while (0)

#define cpu_relax()    do { } while (0)

/*
 * Create a new kernel thread
 */


extern int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

#define copy_segments(tsk, mm)      do { } while (0)
#define release_segments(mm)        do { } while (0)

#define KSTK_EIP(tsk)   (task_pt_regs(tsk)->ret)

/* Where abouts of Task's sp, fp, blink when it was last seen in kernel mode.
 * So these can't be derived from pt_regs as that would give it's
 * sp, fp, blink of user mode
 */
#define KSTK_ESP(tsk)   (tsk->thread.ksp)
#define KSTK_BLINK(tsk) (*((unsigned int *)((KSTK_ESP(tsk)) + (13+1+1)*4)))
#define KSTK_FP(tsk)    (*((unsigned int *)((KSTK_ESP(tsk)) + (13+1)*4)))

/*
 * Do necessary setup to start up a newly executed thread.
 *
 * E1,E2 so that Interrupts are enabled in user mode
 * L set, so Loop inhibited to begin with
 * lp_start and lp_end seeded with bogus non-zero values so to easily catch
 * the ARC700 sr to lp_start hardware bug
 */
#define start_thread(_regs, _pc, _usp)          \
do {                            \
    set_fs(USER_DS); /* reads from user space */    \
    (_regs)->ret = (_pc);               \
    /* User mode, E1 and E2 enabled */      \
    (_regs)->status32 = STATUS_U_MASK       \
                 | STATUS_L_MASK  \
                 | STATUS_E1_MASK   \
                 | STATUS_E2_MASK;  \
    (_regs)->sp = (_usp);                   \
    (_regs)->lp_start = 0x10;                   \
    (_regs)->lp_end = 0x80;                   \
} while(0)

extern unsigned int get_wchan(struct task_struct *p);

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 * Should the PC register be read instead ? This macro does not seem to
 * be used in many places so this wont be all that bad.
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#endif  /* !__ASSEMBLY__ */
#endif  /* __KERNEL__ */
#endif  /* __ASM_ARC_PROCESSOR_H */

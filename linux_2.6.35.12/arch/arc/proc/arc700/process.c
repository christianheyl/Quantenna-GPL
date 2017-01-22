/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: March 2009
 *  -Cleaned up copy_thread( ), made it use task_pt_regs( )
 *  -It used to leave a margin of 8 bytes which was confusing
 *
 * Amit Bhor, Kanika Nema: Codito Technologies 2004
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/a.out.h>
#include <linux/reboot.h>
#include <linux/sys.h>      // for NR_SYSCALS
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/elf.h>
#include <asm/ptrace.h>
#include <asm/pgalloc.h>
#include <linux/vmalloc.h>
#include <asm/arcregs.h>    // For aux_regs
#include <linux/tick.h>     // for tick_nohz_xx
#include <asm/bug.h>
#include <linux/random.h>
#include <linux/delay.h>

/* Sameer: We don't have small-data support yet.
           I am trying to fix it by defining here. */
unsigned long volatile jiffies;

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */

#define ARC_SLEEP_WHEN_IDLE

#ifdef ARC_SLEEP_WHEN_IDLE
extern void arch_idle(void);
#else
void arch_idle(void)
{
}
#endif


void cpu_idle(void)
{

    /* vineetg May 23 2008: Merge from SMP branch to Mainline
     *
     * In the mainline kernel, To conserve power when idle, we "sleep"
     * in arch_idle and thus don't poll the need resched flag.
     * But the TIF_POLLING flag is enabled.
     *
     * This is fine in UNI, since nobody is looking at the TIF_POLLING flag
     * but in SMP, this is an indication to other CPU that "hey I'm polling,
     * don't waste time sending Inter Processor Interrupt (IPI) to
     * resched me. Conseq other CPU's sched sets my resched flag, which
     * I can't see immediately since I'm sleeping. Only when I get a local
     * local interrupt such as TIMER, I break out of sleep, check the
     * resched flag. This will work but not precisely what we want.
     *
     * Thus it is IMP to NOT set the TIF_POLLING flag in SMP and use
     * IPIs to resched.
     */

#if defined (CONFIG_SMP) && !defined(ARC_SLEEP_WHEN_IDLE)
    set_tsk_thread_flag(current, TIF_POLLING_NRFLAG);
#endif

    /* endless idle loop with no priority at all */
    while (1) {
        tick_nohz_stop_sched_tick(1);

        /* Test the need-resced "flag" of current task "idle"
           A local ISR or peer CPU may want resched
           If not set, go to low power friendly "sleep"
         */

        while(!need_resched())
            arch_idle();

        tick_nohz_restart_sched_tick();

        preempt_enable_no_resched();
        schedule();
        preempt_disable();
    }
}

void replace_stack(struct task_struct *task, void *new_stack)
{
	struct thread_info *old_ti = task_thread_info(task);
	struct thread_info *new_ti = new_stack;
	uint32_t stack_reloc = (uint32_t)((char*)new_ti - (char*)old_ti);

	if (unlikely(!task || !new_stack || ((unsigned long)new_stack & (THREAD_SIZE - 1)))) {
		BUG();
		return;
	}

	memcpy(new_ti, old_ti, THREAD_SIZE);
	free_thread_info(old_ti);

	task_stack_page(task) = new_ti;
	set_ti_thread_flag(new_ti, TIF_STACK_REPLACED);

	task->thread.ksp += stack_reloc;
	task->thread.callee_reg += stack_reloc;
}

void kernel_thread_helper(void)
{
    __asm__ __volatile__("mov   r0, r2 \n\t"
                 "mov   r1, r3 \n\t" "j     [r1] "::);
}

/* Sameer: Linux-2.6 has every arch specific process file defining
           its own version of kernel_thread instead of it being in
           fork.c. We need to define our own version of kernel_thread
           and call do_fork() Currently I am writing temporary definition
       which needs to be verified later. But this is an imp issue
       to be tackled */
int kernel_thread(int (*fn) (void *), void *arg, unsigned long flags)
{
    struct pt_regs regs;
    unsigned long status32;

    memset(&regs, 0, sizeof(regs));

    regs.r2 = (unsigned long)arg;
    regs.r3 = (unsigned long)fn;
    regs.blink = (unsigned long)do_exit;
    regs.ret = (unsigned long)kernel_thread_helper;
    __asm__ __volatile__("lr  r9,[status32] \n\t"
        "mov %0, r9":"=r"(status32)::"r9");

    regs.status32 = status32;


    /* Sameer: Fixme. Put values into pr_regs registers here */
    /* Ok, create the new process.. */
    return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs, 0, NULL,
               NULL);

}
EXPORT_SYMBOL(kernel_thread);

asmlinkage int sys_fork(struct pt_regs *regs)
{
    return do_fork(SIGCHLD, regs->sp, regs, 0, NULL, NULL);
}

asmlinkage int sys_vfork(struct pt_regs *regs)
{
    return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->sp, regs, 0,
               NULL, NULL);
}

/* Per man, C-lib clone( ) is as follows
 *
 * int clone(int (*fn)(void *), void *child_stack,
 *           int flags, void *arg, ...
 *           pid_t *ptid, struct user_desc *tls, pid_t *ctid);
 *
 * @fn and @arg are of userland thread-hnalder and thus of no use
 * in sys-call, hence excluded in sys_clone arg list.
 * The only addition is ptregs, needed by fork core, although now-a-days
 * task_pt_regs() can be called anywhere to get that.
 */
asmlinkage int sys_clone(unsigned long clone_flags, unsigned long newsp,
            int __user *parent_tidptr, void *tls, int __user *child_tidptr,
            struct pt_regs *regs)
{
    if (!newsp)
        newsp = regs->sp;
    return do_fork(clone_flags, newsp, regs, 0, parent_tidptr, child_tidptr);
}

asmlinkage void ret_from_fork(void);

/* Layout of Child kernel mode stack as setup at the end of this function is
 *
 * |     ...        |
 * |     ...        |
 * |    unused      |
 * |                |
 * ------------------  <==== top of Stack (thread.ksp)
 * |     r13        |
 * |                |
 * |    --to--.     |   (CALLEE Regs)
 * |                |
 * |     r25        |
 * ------------------
 * |     fp         |
 * |    blink       |   @ret_from_fork
 * ------------------
 * |     r0         |
 * |                |
 * |    --to--.     |   (CALLER Regs)
 * |                |
 * |     r12        |
 * ------------------
 * |   UNUSED 1 word|
 * ------------------  <===== END of PAGE
 */
int copy_thread(unsigned long clone_flags,
        unsigned long usp, unsigned long topstk,
        struct task_struct *p, struct pt_regs *regs)
{
    struct pt_regs *child_ptregs;
    struct callee_regs *child_cregs, *parent_cregs;
    unsigned long *childksp;

    /* Note that parent's pt_regs are passed to this function.
     * They may not be same as one returned by task_pt_regs(current)
     * _IF_ we are starting a kernel thread, because it doesn't have a
     * parent in true sense
     * BUG_ON(regs != task_pt_regs(current));
     */

    if (user_mode(regs))
        BUG_ON(regs != task_pt_regs(current));

    /****************************************************************
     * setup Child for its first ever return to user land
     * by making its pt_regs same as its parent
     ****************************************************************/

    /* Copy parents pt regs on child's kernel mode stack
     */
    child_ptregs = task_pt_regs(p);
    *child_ptregs = *regs;

    /* its kernel mode SP is now @ start of pt_regs */
    childksp = (unsigned long *)child_ptregs;

    /* fork return value 0 for the child */
    child_ptregs->r0 = 0;

    /* IF the parent is a kernel thread then we change the stack pointer
     * of the child its kernel stack
     */
    if (!user_mode(regs)) {
        /* Arc gcc stack adjustment */
        child_ptregs->sp = (unsigned long)task_thread_info(p) +(THREAD_SIZE - 4);
    } else
        child_ptregs->sp = usp;

    /****************************************************************
     * setup Child for its first ever execution in kernel mode, when
     * schedular( ) picks it up for the first time, in switch_to( )
     ****************************************************************/

    /* push frame pointer and return address (blink) as expected by
     * __switch_to on top of pt_regs
     */

    *(--childksp) = (unsigned long)ret_from_fork;   /* push blink */
    *(--childksp) = 0;  /* push fp */

    /* copy CALLEE regs now, on top of above 2 and pt_regs */
    child_cregs = ((struct callee_regs *)childksp) - 1;

    /* Don't copy for kernel threads because we didn't even save
     *  them in first place
     */
    if (user_mode(regs)) {
        parent_cregs = ((struct callee_regs *)regs) - 1;
        *child_cregs = *parent_cregs;
    }

    /* The kernel SP for child has grown further up, now it is
     * at the start of where CALLEE Regs were copied.
     * When child is passed to schedule( ) for the very first time,
     * it unwinds stack, loading CALLEE Regs from top and goes it's
     * merry way
     */
    p->thread.ksp = (unsigned long)child_cregs;  // THREAD_KSP

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    /* Replicate parent's user mode r25 for child */
    p->thread.user_r25 = current->thread.user_r25;
#endif

#ifdef CONFIG_ARC_TLS_REG_EMUL
    if (user_mode(regs)) {
        if (unlikely(clone_flags & CLONE_SETTLS)) {
            /* If we came here because of a clone and that too with XX_SETTLS,
            * set this task's userland tls data ptr from the 4th arg to clone
            * Note that clone C-lib call is difft from clone sys-call
            */
            task_thread_info(p)->thr_ptr = regs->r3;
        }
        else {
            /* Normal fork case: set parent's TLS ptr in child */
            task_thread_info(p)->thr_ptr = task_thread_info(current)->thr_ptr;
        }
    }
#endif

    return 0;
}

/* Sameer: Appended __user compiler attribs to arguements */
asmlinkage int sys_execve(char __user * filenamei, char __user * __user * argv,
              char __user * __user * envp, struct pt_regs *regs)
{
    int error;
    char __user *filename;

    filename = getname(filenamei);
    error = PTR_ERR(filename);
    if (IS_ERR(filename))
        goto out;
    error = do_execve(filename, argv, envp, regs);
    putname(filename);
      out:
    return error;
}

void flush_thread(void)
{
    /* DUMMY: just a dummy function to remove the undefined references */
}

void exit_thread(void)
{
    /* DUMMY: just a dummy function to remove the undefined references */
}

int dump_fpu(struct pt_regs *regs, elf_fpregset_t * fpu)
{
    return 0;
}

#ifndef CORE_DUMP_USE_REGSET

//#define CORE_DEBUG    1
/*
 * fill in the user structure for an elf core dump
 * NOTE: Before making any additions to the number of registers dumped,
 * please change the size of the array elf_gregset_t, defined in
 * include/asm/elf.h
 */
void arc_elf_core_copy_regs(elf_gregset_t *pr_reg, struct pt_regs *regs)
{
    void *elfregs = pr_reg;
    struct callee_regs *calleeregs;
    struct task_struct *tsk = current;

    printk("Dumping Task %x pid [%d] \n", tsk, tsk->pid);

    // setup by low level asm code as THREAD_CALLEE_REG
    calleeregs = (struct callee_regs *)tsk->thread.callee_reg;

    /* copy pt_regs */
    memcpy(elfregs, regs, sizeof(struct pt_regs));
    elfregs += sizeof(struct pt_regs);

    /* copy callee_regs */
    memcpy(elfregs, calleeregs, sizeof(struct callee_regs));
    elfregs += sizeof(struct callee_regs);

    if(in_brkpt_trap(regs)) {
#ifdef CORE_DEBUG
        printk("1 Writing stop_pc(efa) %08lx\n", tsk->thread.fault_address);
#endif
        memcpy(elfregs, &tsk->thread.fault_address, sizeof(unsigned long));
    }
    else {
#ifdef CORE_DEBUG
        printk("2 Writing stop_pc(ret) %08lx\n", regs->ret);
#endif
        memcpy(elfregs, &regs->ret, sizeof(unsigned long));

    }
}

int arc_elf_core_copy_tsk_regs(struct task_struct *tsk, elf_gregset_t *pr_regs)
{
    arc_elf_core_copy_regs(pr_regs, task_pt_regs(tsk));
    return 1;
}
#endif

/* Sameer: We don't have implementation yet */
void (*pm_power_off) (void) = NULL;

// Simon Spooner
// "prepare" for a context switch.
// This arch specific code detects if the next process is the process
// that is being monitored with the ARC profiling tools.
// If it is not, then it will optionally switch off the hardware profiling
// if desired.  If it is it will switch on the monitoring hardware

#ifdef CONFIG_ARC_PROFILING

unsigned long int arc_pid_to_monitor =0;
unsigned long int arc_profiling=0;
EXPORT_SYMBOL(arc_pid_to_monitor);
EXPORT_SYMBOL(arc_profiling);

void arc_ctx_callout(struct task_struct *next)
{
    unsigned int pct_control;
    volatile unsigned int *hwp_ctrl = (unsigned int *)ARC_HWP_CTRL;

    if(arc_profiling & 0x1)       // Is profiling context switch support enabled ?
    {
        if(arc_profiling &0x2) // PCT counters enabled ?
        {
            pct_control = read_new_aux_reg(ARC_PCT_CONTROL);  // Read hardware enable bit
            if( (arc_pid_to_monitor == next->pid)  \
                || arc_pid_to_monitor == 0)     // Are we interested (0 = system mode)?
                pct_control |= 0x1;     // switch on profiling
            else
                pct_control &= ~0x1;    // switch off profiling

            write_new_aux_reg(ARC_PCT_CONTROL, pct_control);
        }

        if(arc_profiling & 0x4)
        {
            if ( (arc_pid_to_monitor == next->pid) || \
                (arc_pid_to_monitor ==0) ) // Are we interested in this PID ?
            {
                // switch on hardware.
                *hwp_ctrl |= PR_CTRL_EN;
            }
            else
            {
                //switch off hardware
                *hwp_ctrl &= ~PR_CTRL_EN;
            }
        }
    }
}

#endif

/*
 * API: expected by schedular Code: If thread is sleeping where is that.
 * What is this good for? it will be always the scheduler or ret_from_fork.
 * So we hard code that anyways.
 */
unsigned long thread_saved_pc(struct task_struct *t)
{
    struct pt_regs *regs = task_pt_regs(t);
    unsigned long blink = 0;

    /* If the thread being queried for in not itself calling this, then it
     * implies it is not executing, which in turn implies it is sleeping,
     * which in turn implies it got switched OUT by the schedular.
     * In that case, it's kernel mode blink can reliably retrieved as per
     * the picture above (right above pt_regs).
     */
    if ( t != current && t->state != TASK_RUNNING ) {
        blink = *((unsigned int *)((unsigned long)regs - 4));
    }

    return blink;
}

/* Independent of kernel CONFIG option, we keep the syscall in
 * unistd.h and in sys-call jump table.
 * But return failure here if not supported
 */
int sys_arc_settls(void *user_tls_data_ptr)
{
#ifdef CONFIG_ARC_TLS_REG_EMUL
    task_thread_info(current)->thr_ptr = (unsigned int)user_tls_data_ptr;
    return 0;
#else
    return -EFAULT;
#endif
}

/* We return the user space TLS data ptr as sys-call return code
 * Ideally it should be copy to user.
 * However we can cheat by the fact that some sys-calls do return
 * absurdly high values
 * Since the tls dat aptr is not going to be in range of 0xFFFF_xxxx
 * it won't be considered a sys-call error
 * and it will be loads better than copy-to-user, which is a definite
 * D-TLB Miss
 */
int sys_arc_gettls(void)
{
#ifdef CONFIG_ARC_TLS_REG_EMUL
    return task_thread_info(current)->thr_ptr;
#else
    return -EFAULT;
#endif
}

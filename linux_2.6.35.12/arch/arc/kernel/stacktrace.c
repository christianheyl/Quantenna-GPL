/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *              Stack tracing for ARC Linux
 *
 *  vineetg: aug 2009
 *  -Implemented CONFIG_STACKTRACE APIs, primarily save_stack_trace_tsk( )
 *   for displaying task's kernel mode call stack in /proc/<pid>/stack
 *  -Iterator based approach to have single copy of unwinding core and APIs
 *   needing unwinding, implement the logic in iterator regarding:
 *      = which frame onwards to start capture
 *      = which frame to stop capturing (wchan)
 *      = specifics of data structs where trace is saved(CONFIG_STACKTRACE etc)
 *
 *  vineetg: March 2009
 *  -Implemented correct versions of thread_saved_pc() and get_wchan()
 *
 *  rajeshwarr: 2008
 *  -Initial implementation
 */

#include <linux/ptrace.h>
#include <linux/module.h>
#include <asm/arcregs.h>
#include <asm/unwind.h>
#include <linux/stacktrace.h>
#include <linux/kallsyms.h>

#define CRASH_MAX_ENTRIES	16
#define CRASH_SYM_LEN		64
struct crash_stack_trace {
	int num_entries;
	struct {
		unsigned int addr;
		char sym[KSYM_SYMBOL_LEN];
	} e [CRASH_MAX_ENTRIES];
};

struct crash_stack_trace crash_log;

/*-------------------------------------------------------------------------
 *              Unwinding Core
 *-------------------------------------------------------------------------
 */
#ifdef CONFIG_ARC_STACK_UNWIND

static void seed_unwind_frame_info(struct task_struct *tsk,
    struct pt_regs *regs, struct unwind_frame_info *frame_info)
{
    if (tsk == NULL && regs == NULL)
    {
        unsigned long fp, sp, blink, ret;
        frame_info->task = current;

        __asm__ __volatile__(
                  "1:mov %0,r27\n\t"
                    "mov %1,r28\n\t"
                    "mov %2,r31\n\t"
                    "mov %3,r63\n\t"
                   :"=r"(fp),"=r"(sp),"=r"(blink),"=r"(ret)
                   :
                   );

        frame_info->regs.r27 = fp;
        frame_info->regs.r28 = sp;
        frame_info->regs.r31 = blink;
        frame_info->regs.r63 = ret;
        frame_info->call_frame = 0;
    }
    else if (regs == NULL ) {

        frame_info->task = tsk;

        frame_info->regs.r27 = KSTK_FP(tsk);
        frame_info->regs.r28 = KSTK_ESP(tsk);
        frame_info->regs.r31 = KSTK_BLINK(tsk);
        frame_info->regs.r63 = (unsigned int )__switch_to;

        /* In the prologue of __switch_to, first fp is saved on the stack and
         * then sp is copied to fp, the dwarf assumes cfa as fp based but we
         * didn't save fp. The value retrieved above is  the fp's state in previous
         * frame. As a work around for this, we are unwinding from __switch_to
         * address and adjusting the sp accordingly. The other limitaion in
         * __switch_to macro is dwarf rules are not generated for inline
         * assembly code
         */
        frame_info->regs.r27 = 0;
        frame_info->regs.r28 += 64;
        frame_info->call_frame = 0;

    }
    else {
        frame_info->task = tsk;

        frame_info->regs.r27 = regs->fp;
        frame_info->regs.r28 = regs->sp;
        frame_info->regs.r31 = regs->blink;
        frame_info->regs.r63 = regs->ret;
        frame_info->call_frame = 0;
    }
}

static unsigned int noinline arc_unwind_core(struct task_struct *tsk,
    struct pt_regs *regs, int (*consumer_fn)(unsigned int, void *), void *arg)
{
    int ret = 0;
    unsigned int address;
    struct unwind_frame_info frame_info;

    seed_unwind_frame_info(tsk, regs, &frame_info);

    while(1)
    {
        address = UNW_PC(&frame_info);

        if (address && __kernel_text_address(address )) {
            if (consumer_fn(address, arg) == -1 )
                break;
        }

        ret = arc_unwind(&frame_info);

        if(ret == 0) {
            frame_info.regs.r63 = frame_info.regs.r31;
            continue;
        }
        else {
            break;
        }
    }

    return address; // return the last address it saw
}

#else
static unsigned int arc_unwind_core(struct task_struct *tsk,
    struct pt_regs *regs, int (*fn)(unsigned int, void *),void *arg)
{
    /* On ARC, only Dward based unwinder works. fp based backtracing is
     * not possible (-fno-omit-frame-pointer) because of the way function
     * prelogue is setup (callee regs saved and then fp set and not other
     * way around
     */
    printk("CONFIG_ARC_STACK_UNWIND needs to be enabled\n");
    return 0;
}
#endif

/*-------------------------------------------------------------------------
 *  iterators called by unwinding core to implement APIs expected by kernel
 *
 *  Return value protocol:
 *      (-1) to stop unwinding
 *      (xx) continue unwinding
 *-------------------------------------------------------------------------
 */

void save_crash_log(unsigned int address)
{
    unsigned long flags;
    int count;

    local_irq_save(flags);

    count = crash_log.num_entries;
    if (count < CRASH_MAX_ENTRIES-1) {
	crash_log.e[count].addr = address;
	sprint_symbol(crash_log.e[count].sym, address);
	crash_log.num_entries++;
    }

    local_irq_restore(flags);
}
/* Call-back which plugs into unwinding core to dump the stack in
 * case of panic/OOPs/BUG etc
 */
int verbose_dump_stack(unsigned int address, void *unused)
{
    printk("  0x%08x ", address);
    __print_symbol("  %s\n", address);
    save_crash_log(address);
    return 0;
}

#ifdef CONFIG_STACKTRACE

/* Call-back which plugs into unwinding core to capture the
 * traces needed by kernel on /proc/<pid>/stack
 */
int fill_backtrace(unsigned int address, void *arg)
{
    struct stack_trace *trace = arg;

    if (trace->skip > 0)
        trace->skip--;
    else
        trace->entries[trace->nr_entries++] = address;

    if (trace->nr_entries >= trace->max_entries)
        return -1;

    return 0;
}

int fill_backtrace_nosched(unsigned int address, void *arg)
{
    struct stack_trace *trace = arg;

    if (in_sched_functions(address))
        return 0;

    if (trace->skip > 0)
        trace->skip--;
    else
        trace->entries[trace->nr_entries++] = address;

    if (trace->nr_entries >= trace->max_entries)
        return -1;

    return 0;
}

#endif

int get_first_nonsched_frame(unsigned int address, void *unused)
{
    if (in_sched_functions(address))
        return 0;

    return -1;
}

/*-------------------------------------------------------------------------
 *              APIs expected by various kernel sub-systems
 *-------------------------------------------------------------------------
 */
void (*g_qtn_show_info) (void) = NULL;

void qtn_show_info_register(void *fn) {
    g_qtn_show_info = fn;
}

void qtn_show_info_unregister(void) {
    g_qtn_show_info = NULL;
}

EXPORT_SYMBOL(qtn_show_info_register);
EXPORT_SYMBOL(qtn_show_info_unregister);

void noinline show_stacktrace(struct task_struct *tsk, struct pt_regs *regs)
{
    if (g_qtn_show_info) {
        g_qtn_show_info();
    }
    printk("\nStack Trace:\n");
    arc_unwind_core(tsk, regs, verbose_dump_stack, NULL);
}

EXPORT_SYMBOL(show_stacktrace);

/* Expected by sched Code */
void show_stack(struct task_struct *tsk, unsigned long *sp)
{
    show_stacktrace(tsk, NULL);
}

/* Expected by Rest of kernel code */
void dump_stack(void)
{
    show_stacktrace(0, NULL);
}

EXPORT_SYMBOL(dump_stack);

/* Another API expected by schedular, shows up in "ps" as Wait Channel
 * Ofcourse just returning schedule( ) would be pointless so unwind until
 * the function is not in schedular code
 */
unsigned int get_wchan(struct task_struct *tsk)
{
    return arc_unwind_core(tsk, NULL, get_first_nonsched_frame, NULL);
}

#ifdef CONFIG_STACKTRACE

/* API required by CONFIG_STACKTRACE, CONFIG_LATENCYTOP.
 * A typical use is when /proc/<pid>/stack is queried by userland
 * and also by LatencyTop
 */
void save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
    arc_unwind_core(tsk, NULL, fill_backtrace_nosched, trace);
}

void save_stack_trace(struct stack_trace *trace)
{
    arc_unwind_core(current, NULL, fill_backtrace, trace);
}
#endif

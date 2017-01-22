/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Derived largely from ARM port
 */

#define ASM_OFFSETS_C

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <asm/hardirq.h>
#include <asm/thread_info.h>
#include <asm/page.h>

/* Use marker if you need to separate the values later */

#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

int main(void)
{
	DEFINE(TASK_STATE, offsetof(struct task_struct, state));
	DEFINE(TASK_FLAGS, offsetof(struct task_struct, flags));
	DEFINE(TASK_PTRACE, offsetof(struct task_struct, ptrace));
	DEFINE(TASK_THREAD, offsetof(struct task_struct, thread));
	DEFINE(TASK_MM, offsetof(struct task_struct, mm));
	DEFINE(TASK_PID, offsetof(struct task_struct, pid));
	DEFINE(TASK_THREAD_INFO, offsetof(struct task_struct, stack));

	BLANK();

	DEFINE(THREAD_KSP, offsetof(struct thread_struct, ksp));
	DEFINE(THREAD_CALLEE_REG, offsetof(struct thread_struct, callee_reg));
#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
	DEFINE(THREAD_USER_R25, offsetof(struct thread_struct, user_r25));
#endif
	DEFINE(THREAD_FAULT_ADDR, offsetof(struct thread_struct, fault_address));

	BLANK();

	DEFINE(THREAD_INFO_FLAGS, offsetof(struct thread_info, flags));
	DEFINE(THREAD_INFO_PREEMPT_COUNT, offsetof(struct thread_info, preempt_count));

	BLANK();

#ifdef CONFIG_SMP
	DEFINE(SECONDARY_BOOT_STACK, offsetof(secondary_boot_t ,stack));
	DEFINE(SECONDARY_BOOT_C_ENTRY, offsetof(secondary_boot_t ,c_entry));
	DEFINE(SECONDARY_BOOT_CPU_ID, offsetof(secondary_boot_t ,cpu_id));
	BLANK();
#endif

	DEFINE(TASK_ACT_MM, offsetof(struct task_struct, active_mm));
	DEFINE(TASK_TGID, offsetof(struct task_struct, tgid));

	DEFINE(MM_CTXT, offsetof(struct mm_struct, context));
	DEFINE(MM_PGD, offsetof(struct mm_struct, pgd));

	DEFINE(MM_CTXT_ASID, offsetof(mm_context_t, asid));

#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
	BLANK();
	DEFINE(EVLOG_FIELD_EXTRA, offsetof(timeline_log_t, extra));
	DEFINE(EVLOG_FIELD_EFA, offsetof(timeline_log_t, fault_addr));
	DEFINE(EVLOG_FIELD_CAUSE, offsetof(timeline_log_t, cause));
	DEFINE(EVLOG_FIELD_TASK, offsetof(timeline_log_t, task));
	DEFINE(EVLOG_FIELD_TIME, offsetof(timeline_log_t, time));
	DEFINE(EVLOG_FIELD_EVENT_ID, offsetof(timeline_log_t, event));
	DEFINE(EVLOG_FIELD_SP, offsetof(timeline_log_t, sp));
	DEFINE(EVLOG_RECORD_SZ, sizeof(timeline_log_t));
	DEFINE(EVLOG_FIELD_EXTRA2, offsetof(timeline_log_t, extra2));
	DEFINE(EVLOG_FIELD_EXTRA3, offsetof(timeline_log_t, extra3));
#endif

	return 0;
}

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *  vineetg: Feb 2008
 *
 *      System Event Logging APIs
 */

#ifndef __ASM_ARC_EVENT_LOG_ASM_H
#define __ASM_ARC_EVENT_LOG_ASM_H

#include <asm/event-log.h>

#ifdef __ASSEMBLY__

#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

.macro TAKE_SNAP_ASM reg_scratch, reg_ptr, type
.endm

.macro TAKE_SNAP_C_FROM_ASM type
.endm

#else

#include <asm/asm-offsets.h>

.macro TAKE_SNAP_ASM reg_scratch, reg_ptr, type

    /* Earlier we used to save only reg_scratch and clobber reg_ptr and rely
     * on caller to understand this. Too mcuh trouble.
     * Now we save both
     */
    st \reg_scratch, [tmp_save_reg]
    st \reg_ptr,     [tmp_save_reg2]

    ld \reg_ptr, [timeline_ctr]

    ; hack to detect if we are trying to overflow the circular log buffer
    brne \reg_ptr, MAX_SNAPS, 1f
    flag 1
    nop
    nop
    nop

1:
    mpyu  \reg_ptr, \reg_ptr, EVLOG_RECORD_SZ

;8001821c:	ld         r6,[0x8024ba98]
;80018228:	add1       r2,r6,r6
;80018230:	add2       r2,r6,r2
;80018234:	asl        r8,r2,2
;80018238:	add        r5,r8,0x8023ea64
;80018240:	st         r4,[r5,32]

;    add1 \reg_scratch, \reg_ptr, \reg_ptr
;    add2 \reg_scratch, \reg_ptr, \reg_scratch
;    asl  \reg_ptr, \reg_scratch, 2

    add \reg_ptr, timeline_log, \reg_ptr

    ;############ Common data ##########

    ; TIMER1 count in timeline_log[timeline_ctr].time
    lr  \reg_scratch, [ARC_REG_TIMER1_CNT]
    st  \reg_scratch, [\reg_ptr, EVLOG_FIELD_TIME]

    ; current task ptr in timeline_log[timeline_ctr].task
    ld \reg_scratch, [_current_task]
    ld \reg_scratch, [\reg_scratch, TASK_TGID]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_TASK]

    ; Type of event (Intr/Excp/Trap etc)
    mov \reg_scratch, \type
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EVENT_ID]

    ; save SP at time of exception
    st sp, [\reg_ptr, EVLOG_FIELD_SP]

    st 0, [\reg_ptr, EVLOG_FIELD_EXTRA]
    st 0, [\reg_ptr, EVLOG_FIELD_CAUSE]
    st 0, [\reg_ptr, EVLOG_FIELD_EXTRA3]

    lr \reg_scratch, [status32]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA2]



    ;############ Event specific data ##########

    ; save additional info depending on data point
    ;    in timeline_log[timeline_ctr].cause
    ;
    mov \reg_scratch, \type

    and.f 0, \reg_scratch, EVENT_CLASS_EXIT
    bz 1f

    // Stuff to do for all kernel exit events
    ld \reg_scratch, [sp, PT_status32]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA]

    // SP in log->sp
    // ld \reg_scratch, [sp, PT_sp]

    // preempt count in log->sp
    and  \reg_scratch, sp, ~(0x2000-1)
    ld \reg_scratch, [\reg_scratch, THREAD_INFO_PREEMPT_COUNT ]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_SP]


    ld \reg_scratch, [sp, PT_ret]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EFA]

    mov \reg_scratch, \type

1:

2:
    /* for Trap, Syscall number  */
    cmp \reg_scratch, SNAP_TRAP_IN
    bnz 3f
    st r8, [\reg_ptr, EVLOG_FIELD_CAUSE]
    lr \reg_scratch, [erstatus]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA]
    j 99f

3:

5:
    /* For Exceptions (TLB/ProtV etc)*/
    cmp \reg_scratch, SNAP_EXCP_IN
    bnz 6f

    lr \reg_scratch, [ecr]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_CAUSE]
    lr \reg_scratch, [eret]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EFA]
    lr \reg_scratch, [erstatus]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA]
    lr \reg_scratch, [efa]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA3]
    j 99f

6: /* for Interrupts, IRQ */

    cmp \reg_scratch, SNAP_INTR_OUT
    bnz 7f
    lr \reg_scratch, [icause1]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_CAUSE]
    j 99f

7:
    cmp \reg_scratch, SNAP_INTR_OUT2
    bnz 8f
    lr \reg_scratch, [icause2]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_CAUSE]
    j 99f

8:
    cmp \reg_scratch, SNAP_EXCP_OUT_FAST
    bnz 9f
    lr \reg_scratch, [erstatus]
    st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA]
    j 99f

9:
    // place holder for next

99:
    /* increment timeline_ctr  with mode on max */
    ld \reg_scratch, [timeline_ctr]
    add \reg_scratch, \reg_scratch, 1
    and \reg_scratch, \reg_scratch, MAX_SNAPS_MASK
    st \reg_scratch, [timeline_ctr]

    /* Restore back orig scratch reg */
    ld \reg_scratch, [tmp_save_reg]
    ld \reg_ptr, [tmp_save_reg2]
.endm

/* Macro to invoke a "C" routine from assmebler.
 * Makes the ISR/Event hanlder look so much clean
 */
.macro TAKE_SNAP_C_FROM_ASM type
    mov r0, \type
    bl take_snap2
.endm

#endif  /* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif	/* __ASSEMBLY__ */

#endif

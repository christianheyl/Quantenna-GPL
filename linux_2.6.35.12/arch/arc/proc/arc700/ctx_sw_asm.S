/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Aug 2009
 *  -Moved core context switch macro out of entry.S into this file.
 *  -This is the more "natural" hand written assembler
 */

#include <linux/linkage.h>   /* contains declarations of SYMBOL_NAME */
#include <asm/entry.h>       /* For the SAVE_* macros */
#include <asm/asm-offsets.h>

;################### Low Level Context Switch ##########################

.macro ARC_SCHED_FUNC x
    .section .sched.text,"ax",@progbits
    .align 4
    .global SYMBOL_NAME(\x)
    .type   SYMBOL_NAME(\x), @function
SYMBOL_NAME_LABEL(\x)
.endm

ARC_SCHED_FUNC __switch_to

    /* Save regs on kernel mode stack of task */
    st.a    blink, [sp, -4]
    st.a    fp, [sp, -4]
    SAVE_CALLEE_SAVED_KERNEL

    /* Save the now KSP in task->thread.ksp */
    st.as  sp, [r0, (TASK_THREAD + THREAD_KSP)/4]

    /* Return last task in r0 (return reg)
         On ARC, Return reg = First Arg reg = r0.
         Since we already have last task in r0,
         don't need to do anything special to return it
    */

    ; Ensure all pending memory operations have physically completed before
    ; switching contexts. This is to make sure that the right process is
    ; killed in case of a memory error
    sync

    /* switch to new task , second parameter to this function , ie r1*/
    // Temp reg r3 is required to get the ptr to store val
    PUT_CURR_TASK_ON_CPU  r1, r3

    /* reload SP with saved kernel mode stack pointer in task->thread.ksp */
    ld.as  sp, [r1, (TASK_THREAD + THREAD_KSP)/4]

    /* restore the registers */
    RESTORE_CALLEE_SAVED_KERNEL
    ld.ab   fp, [sp, 4]
    ld.ab   blink, [sp, 4]
    j       [blink]

ARC_EXIT __switch_to

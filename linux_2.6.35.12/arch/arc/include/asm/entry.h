/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: Feb/Mar 2011 (minor tweaks to reduce code in ISR/excpn hdlrs)
 *  -for ISR orig_r0 not used to keep it 0 instead of -1 (saves 4 bytes)
 *  -added pt_regs canary to verify they are not clobbered
 *
 * Vineetg: March 2009 (Supporting 2 levels of Interrupts)
 *  Stack switching code can no longer reliably rely on the fact that
 *  if we are NOT in user mode, stack is switched to kernel mode.
 *  e.g. L2 IRQ interrupted a L1 ISR which had not yet completed
 *  it's prologue including stack switching from user mode
 *
 * Vineetg: Feb 2009: Changes to stack Switching Macro
 *  The idea is to not have intermediate values in SP
 *    -Rather than using SP as scratcpad in switching code use R9
 *    -R25 safekeeping done before SP changes to kernel mode
 *
 * Vineetg: Aug 28th 2008: Bug #94984
 *  -Zero Overhead Loop Context shd be cleared when entering IRQ/EXcp/Trap
 *   Normally CPU does this automatically, however when doing FAKE rtie,
 *   we also need to explicitly do this. The problem in macros
 *   FAKE_RET_FROM_EXCPN and FAKE_RET_FROM_EXCPN_LOCK_IRQ was that this bit
 *   was being "CLEARED" rather then "SET". Actually "SET" clears ZOL context
 *
 * Vineetg: May 5th 2008
 *  -Modified CALLEE_REG save/restore macros to handle the fact that
 *      r25 contains the kernel current task ptr
 *  - Defined Stack Switching Macro to be reused in all intr/excp hdlrs
 *  - Shaved off 11 instructions from RESTORE_ALL_INT1 by using the
 *      address Write back load ld.ab instead of seperate ld/add instn
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef __ASM_ARC_ENTRY_H
#define __ASM_ARC_ENTRY_H

#ifdef __ASSEMBLY__
#include <asm/unistd.h> /* For NR_syscalls defination */
#include <asm/asm-offsets.h>
#include <asm/arcregs.h>
#include <asm/current.h>
#include <asm/ptrace.h>
#include <asm/pgtable.h>   // For VMALLOC_START
#include <asm/thread_info.h>   // For THREAD_SIZE

/* Context saving macros -
 *
 * Note
 *
 * ld.a    reg1, [reg2, x]  => Pre Increment
 * ld.aw   reg1, [reg2, x]
 *
 *      reg2 = reg2 + x
 *      Addr used to load reg1 = [reg2 + x]
 *
 * ld.ab   reg1, [reg2, x]  => Post Increment
 *      Addr used to load reg1 = [reg2]
 *      reg2 = reg2 + x
 *
 */

/*--------------------------------------------------------------
 * Save caller saved registers (scratch registers) ( r0 - r12 )
 * Registers are pushed / popped in the order defined in struct ptregs
 * in asm/ptrace.h
 *-------------------------------------------------------------*/
.macro  SAVE_CALLER_SAVED
    st.a    r0, [sp, -4]
    st.a    r1, [sp, -4]
    st.a    r2, [sp, -4]
    st.a    r3, [sp, -4]
    st.a    r4, [sp, -4]
    st.a    r5, [sp, -4]
    st.a    r6, [sp, -4]
    st.a    r7, [sp, -4]
    st.a    r8, [sp, -4]
    st.a    r9, [sp, -4]
    st.a    r10, [sp, -4]
    st.a    r11, [sp, -4]
    st.a    r12, [sp, -4]
.endm

/*--------------------------------------------------------------
 * Restore caller saved registers (scratch registers)
 *-------------------------------------------------------------*/
.macro RESTORE_CALLER_SAVED
    ld.ab   r12, [sp, 4]
    ld.ab   r11, [sp, 4]
    ld.ab   r10, [sp, 4]
    ld.ab   r9, [sp, 4]
    ld.ab   r8, [sp, 4]
    ld.ab   r7, [sp, 4]
    ld.ab   r6, [sp, 4]
    ld.ab   r5, [sp, 4]
    ld.ab   r4, [sp, 4]
    ld.ab   r3, [sp, 4]
    ld.ab   r2, [sp, 4]
    ld.ab   r1, [sp, 4]
    ld.ab   r0, [sp, 4]
.endm


/*--------------------------------------------------------------
 * Save callee saved registers (non scratch registers) ( r13 - r25 )
 *  on kernel stack.
 * User mode callee regs need to be saved in case of
 *    -fork and friends for replicating from parent to child
 *    -before going into do_signal( ) for ptrace/core-dump
 * Special case handling is required for r25 in case it is used by kernel
 *   for caching task ptr. Low level exception/ISR save user mode r25
 *   into task->thread.user_r25. So it needs to be retrieved from there and
 *   saved into kernel stack wit rest of callee reg-file
 *-------------------------------------------------------------*/
.macro SAVE_CALLEE_SAVED_USER
    st.a    r13, [sp, -4]
    st.a    r14, [sp, -4]
    st.a    r15, [sp, -4]
    st.a    r16, [sp, -4]
    st.a    r17, [sp, -4]
    st.a    r18, [sp, -4]
    st.a    r19, [sp, -4]
    st.a    r20, [sp, -4]
    st.a    r21, [sp, -4]
    st.a    r22, [sp, -4]
    st.a    r23, [sp, -4]
    st.a    r24, [sp, -4]

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    ; Retrieve orig r25 and save it on stack
    ld      r12, [r25, TASK_THREAD + THREAD_USER_R25]
    st.a    r12, [sp, -4]
#else
    st.a    r25, [sp, -4]
#endif

    /* move up by 1 word to "create" callee_regs->"stack_place_holder" */
    sub sp, sp, 4
.endm

/*--------------------------------------------------------------
 * Save callee saved registers (non scratch registers) ( r13 - r25 )
 * kernel mode callee regs needed to be saved in case of context switch
 * If r25 is used for caching task pointer then that need not be saved
 * as it can be re-created from current task global
 *-------------------------------------------------------------*/
.macro SAVE_CALLEE_SAVED_KERNEL
    st.a    r13, [sp, -4]
    st.a    r14, [sp, -4]
    st.a    r15, [sp, -4]
    st.a    r16, [sp, -4]
    st.a    r17, [sp, -4]
    st.a    r18, [sp, -4]
    st.a    r19, [sp, -4]
    st.a    r20, [sp, -4]
    st.a    r21, [sp, -4]
    st.a    r22, [sp, -4]
    st.a    r23, [sp, -4]
    st.a    r24, [sp, -4]
#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    sub     sp, sp, 8
#else
    st.a    r25, [sp, -4]
    sub     sp, sp, 4
#endif
.endm

/*--------------------------------------------------------------
 * RESTORE_CALLEE_SAVED:
 * Loads callee (non scratch) Reg File by popping from Kernel mode stack.
 * This is reverse of SAVE_CALLEE_SAVED,
 *
 * NOTE:
 * Ideally this shd only be called in switch_to for loading
 * switched-IN task's CALLEE Reg File.
 * For all other cases RESTORE_CALLEE_SAVED_FAST must be used
 * which simply pops the stack w/o touching regs.
 *-------------------------------------------------------------*/
.macro RESTORE_CALLEE_SAVED_KERNEL


#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    add     sp, sp, 8  /* skip callee_reg gutter and user r25 placeholder */
#else
    add     sp, sp, 4   /* skip "callee_regs->stack_place_holder" */
    ld.ab   r25, [sp, 4]
#endif

    ld.ab   r24, [sp, 4]
    ld.ab   r23, [sp, 4]
    ld.ab   r22, [sp, 4]
    ld.ab   r21, [sp, 4]
    ld.ab   r20, [sp, 4]
    ld.ab   r19, [sp, 4]
    ld.ab   r18, [sp, 4]
    ld.ab   r17, [sp, 4]
    ld.ab   r16, [sp, 4]
    ld.ab   r15, [sp, 4]
    ld.ab   r14, [sp, 4]
    ld.ab   r13, [sp, 4]

.endm

/*--------------------------------------------------------------
 * Super FAST Restore callee saved regs by simply re-adjusting SP
 *-------------------------------------------------------------*/
.macro DISCARD_CALLEE_SAVED_USER
    add     sp, sp, 14 * 4
.endm

/*--------------------------------------------------------------
 * Restore User mode r25 saved in task_struct->thread.user_r25
 *-------------------------------------------------------------*/
.macro RESTORE_USER_R25
    ld  r25, [r25, TASK_THREAD + THREAD_USER_R25]
.endm

/*--------------------------------------------------------------
 * Switch to Kernel Mode stack if SP points to User Mode stack
 *
 * This macro
 *  -if event happened in USER mode proceeds to switch stack.
 *    (in other words, would _NOT_ switch stack if event happened
 *      in kernel mode)
 *  -For that calculates the new value of SP (kernel mode stack)
 *
 * Note (Assumption on ENTRY)
 *   Requires r9 to be loaded with the pre-intr/pre-excp status32
 *
 * Note: Special case handling for L2 ISR
 *-------------------------------------------------------------*/

.macro SWITCH_TO_KERNEL_STK

    // User Mode when this happened ? Yes: Proceed to switch stack
    bbit1   r9, STATUS_U_BIT, 88f

    /* OK we were already in kernel mode when this event happened, thus can
     * assume SP is kernle mode SP. _NO_ need to do any stack switching */

#ifdef  CONFIG_ARCH_ARC_LV2_INTR
    /* However....
     * If Level 2 Interrupts enabled, we may end up with a corner case:
     * 1. User Task executing
     * 2. L1 IRQ taken, ISR starts (CPU auto-switched to KERNEL mode)
     * 3. But before it could switch SP from USER to KERNEL stack
     *      a L2 IRQ "Interrupts" L1
     * Thay way although L2 IRQ happened in Kernel mode, stack is still
     * not switched.
     * To handle this case we may need to switch stack even if in kernel mode
     * provided SP has values in range of USER mode stack ( < 0x7000_0000 )
     */
    brlo sp, VMALLOC_START, 88f

    // TODO: vineetg:
    // We need to be a bit more cautious here. What if a kernel bug in
    // L1 ISR, caused SP to go whaco (some small value which looks like USER stk)
    // and then we take L2 ISR.
    //
    // the above brlo alone would treat it as a valid L1-L2 sceanrio instead of
    // shouting alound
    // The only feasible way is to make sure this L2 happened in L1 prelogue ONLY
    // i.e. ilink2 is less than a pre-set marker in L1 ISR before it
    // switches stack

#endif

    /* Save Pre Intr/Exception Kernel SP on kernel stack
     *  This is not strictly required when in Kernel mode.
     *  We can skip this part here. However at the end, when restoring
     *  Regs back, we also need to make a check that for kernel mode
     *  SP is not restored. This however equires additional code, which
     *  in turn requires free Reg(s) to write the code itself, which we
     *  dont have since rest of the reg file has been restored to
     *  PRE-INTR/EXCP values
     */
    b.d 77f

    st.a    sp, [sp, -4]

88: /*------Intr/Ecxp happened in user mode, "switch" stack ------ */

    GET_CURR_TASK_ON_CPU   r9

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG

    /* If current task pointer cached in R25, time to
     *  -safekeep USER R25 in task->thread_struct->user_r25
     *  -load R25 with current task ptr
     */
    st.as      r25, [r9, (TASK_THREAD + THREAD_USER_R25)/4]
    mov     r25, r9
#endif

    /* Get task->thread_info (this is essentially start of a PAGE) */
    ld  r9, [r9, TASK_THREAD_INFO]

    /* Go to end of page pointed to by task->thread_info.
     *  This is start of THE kernel stack (grows upwards remember)
     */
    add2 r9, r9, (THREAD_SIZE - 4)/4   // 0ne word GUTTER

#ifdef PT_REGS_CANARY
    st    0xabcdabcd, [r9, 0]
#endif

    /* Save Pre Intr/Exception User SP on kernel stack */
    st.a    sp, [r9, -4]

    /* CAUTION:
     * SP should be set at the very end when we are done with everything
     * In case of 2 levels of iterrupt we depend on value of SP to assume
     * that everything else is done (loading R25 etc)
     */

    /* set SP to point to kernel mode stack */
    mov sp, r9

77: /* ----- Stack Switched to kernel Mode, Now save REG FILE ----- */

.endm

/*------------------------------------------------------------
 * "FAKE" a rtie to return from CPU Exception context
 * This is to re-enable Exceptions within exception
 * Look at EV_ProtV to see how this is actually used
 *-------------------------------------------------------------*/

.macro FAKE_RET_FROM_EXCPN  reg

    ld  \reg, [sp, PT_status32]
    bic  \reg, \reg, (STATUS_U_MASK|STATUS_DE_MASK)
    bset \reg, \reg, STATUS_L_BIT
    sr  \reg, [erstatus]
    mov \reg, 55f
    sr  \reg, [eret]

    rtie
55:
.endm

.macro GET_CURR_THR_INFO_FROM_SP  reg
    and \reg, sp, ~(THREAD_SIZE - 1)
.endm

/*--------------------------------------------------------------
 * Save all registers used by Exceptions (TLB Miss, Prot-V, Mem err etc)
 * Requires SP to be already switched to kernel mode Stack
 * sp points to the next free element on the stack at exit of this macro.
 * Registers are pushed / popped in the order defined in struct ptregs
 * in asm/ptrace.h
 * Note that syscalls are implemented via TRAP which is also a exception
 * from CPU's point of view
 *-------------------------------------------------------------*/
.macro SAVE_ALL_EXCEPTION   marker

    /* restore original r9 , saved in ex_saved_reg1
     * It will be saved on stack in macro: SAVE_CALLER_SAVED
     */
    ld  r9, [SYMBOL_NAME(ex_saved_reg1)]

    /* now we are ready to save the remaining context */
    st.a    \marker, [sp, -4]	/* orig_r8:
                                    syscalls   -> 1 to NR_SYSCALLS
                                    Exceptions -> NR_SYSCALLS + 1
                                    Break-point-> NR_SYSCALLS + 2
                                 */
    st.a    r0, [sp, -4]    /* orig_r0, needed only for sys calls */
    SAVE_CALLER_SAVED
    st.a    r26, [sp, -4]   /* gp */
    st.a    fp, [sp, - 4]
    st.a    blink, [sp, -4]
    lr  r9, [eret]
    st.a    r9, [sp, -4]
    lr  r9, [erstatus]
    st.a    r9, [sp, -4]
    st.a    lp_count, [sp, -4]
    lr  r9, [lp_end]
    st.a    r9, [sp, -4]
    lr  r9, [lp_start]
    st.a    r9, [sp, -4]
    lr  r9, [erbta]
    st.a    r9, [sp, -4]

#ifdef PT_REGS_CANARY
    mov   r9, 0xdeadbeef
    st    r9, [sp, -4]
#endif

    /* move up by 1 word to "create" pt_regs->"stack_place_holder" */
    sub sp, sp, 4
.endm

/*--------------------------------------------------------------
 * Save scratch regs for exceptions
 *-------------------------------------------------------------*/
.macro SAVE_ALL_SYS
    SAVE_ALL_EXCEPTION  (NR_syscalls + 1)
.endm

/*--------------------------------------------------------------
 * Save scratch regs for sys calls
 *-------------------------------------------------------------*/
.macro SAVE_ALL_TRAP
    SAVE_ALL_EXCEPTION  r8
.endm

/*--------------------------------------------------------------
 * Restore all registers used by system call or Exceptions
 * SP should always be pointing to the next free stack element
 * when entering this macro.
 *
 * NOTE:
 *
 * It is recommended that lp_count/ilink1/ilink2 not be used as a dest reg
 * for memory load operations. If used in that way interrupts are deffered
 * by hardware and that is not good.
 *-------------------------------------------------------------*/
.macro RESTORE_ALL_SYS

    add sp, sp, 4       /* hop over unused "pt_regs->stack_place_holder" */

    ld.ab   r9, [sp, 4]
    sr  r9, [erbta]
    ld.ab   r9, [sp, 4]
    sr  r9, [lp_start]
    ld.ab   r9, [sp, 4]
    sr  r9, [lp_end]
    ld.ab   r9, [sp, 4]
    mov lp_count, r9
    ld.ab   r9, [sp, 4]
    sr  r9, [erstatus]
    ld.ab   r9, [sp, 4]
    sr  r9, [eret]
    ld.ab   blink, [sp, 4]
    ld.ab   fp, [sp, 4]
    ld.ab   r26, [sp, 4]    /* gp */
    RESTORE_CALLER_SAVED

    /* ignoring orig_r0 and orig_r8 */
    ld  sp, [sp, 8] /* restore original sp */
.endm


/*--------------------------------------------------------------
 * Save all registers used by interrupt handlers.
 *-------------------------------------------------------------*/
.macro SAVE_ALL_INT1

    /* restore original r9 , saved in int1_saved_reg
     * It will be saved on stack in macro: SAVE_CALLER_SAVED
     */
#ifdef CONFIG_SMP
    lr  r9, [ARC_REG_SCRATCH_DATA0]
#else
    ld  r9, [SYMBOL_NAME(int1_saved_reg)]
#endif

    /* now we are ready to save the remaining context :) */
    st.a    -1, [sp, -4]    /* orig_r8, -1 for interuppt level one */
    st.a    0, [sp, -4]    /* orig_r0 , N/A for IRQ */
    SAVE_CALLER_SAVED
    st.a    r26, [sp, -4]   /* gp */
    st.a    fp, [sp, - 4]
    st.a    blink, [sp, -4]
    st.a    ilink1, [sp, -4]
    lr  r9, [status32_l1]
    st.a    r9, [sp, -4]
    st.a    lp_count, [sp, -4]
    lr  r9, [lp_end]
    st.a    r9, [sp, -4]
    lr  r9, [lp_start]
    st.a    r9, [sp, -4]
    lr  r9, [bta_l1]
    st.a    r9, [sp, -4]

#ifdef PT_REGS_CANARY
    mov   r9, 0xdeadbee1
    st    r9, [sp, -4]
#endif
    /* move up by 1 word to "create" pt_regs->"stack_place_holder" */
    sub sp, sp, 4
.endm

.macro SAVE_ALL_INT2

    /* TODO-vineetg: SMP we can't use global nor can we use
     *   SCRATCH0 as we do for int1 because while int1 is using
     *   it, int2 can come
     */
    /* retsore original r9 , saved in sys_saved_r9 */
    ld  r9, [SYMBOL_NAME(int2_saved_reg)]

    /* now we are ready to save the remaining context :) */
    st.a    -2, [sp, -4]    /* orig_r8, -2 for interrupt level 2 */
    st.a    0, [sp, -4]    /* orig_r0 , N/A for IRQ */
    SAVE_CALLER_SAVED
    st.a    r26, [sp, -4]   /* gp */
    st.a    fp, [sp, - 4]
    st.a    blink, [sp, -4]
    st.a    ilink2, [sp, -4]
    lr  r9, [status32_l2]
    st.a    r9, [sp, -4]
    st.a    lp_count, [sp, -4]
    lr  r9, [lp_end]
    st.a    r9, [sp, -4]
    lr  r9, [lp_start]
    st.a    r9, [sp, -4]
    lr  r9, [bta_l2]
    st.a    r9, [sp, -4]

#ifdef PT_REGS_CANARY
    mov   r9, 0xdeadbee2
    st    r9, [sp, -4]
#endif

    /* move up by 1 word to "create" pt_regs->"stack_place_holder" */
    sub sp, sp, 4
.endm

/*--------------------------------------------------------------
 * Restore all registers used by interrupt handlers.
 *
 * NOTE:
 *
 * It is recommended that lp_count/ilink1/ilink2 not be used as a dest reg
 * for memory load operations. If used in that way interrupts are deffered
 * by hardware and that is not good.
 *-------------------------------------------------------------*/

.macro RESTORE_ALL_INT1
    add sp, sp, 4       /* hop over unused "pt_regs->stack_place_holder" */

    ld.ab   r9, [sp, 4] /* Actual reg file */
    sr  r9, [bta_l1]
    ld.ab   r9, [sp, 4]
    sr  r9, [lp_start]
    ld.ab   r9, [sp, 4]
    sr  r9, [lp_end]
    ld.ab   r9, [sp, 4]
    mov lp_count, r9
    ld.ab   r9, [sp, 4]
    sr  r9, [status32_l1]
    ld.ab   r9, [sp, 4]
    mov ilink1, r9
    ld.ab   blink, [sp, 4]
    ld.ab   fp, [sp, 4]
    ld.ab   r26, [sp, 4]    /* gp */
    RESTORE_CALLER_SAVED

    /* ignoring orig_r0 and orig_r8 */
    ld  sp, [sp, 8] /* restore original sp */
.endm

.macro RESTORE_ALL_INT2
    add sp, sp, 4       /* hop over unused "pt_regs->stack_place_holder" */

    ld.ab   r9, [sp, 4]
    sr  r9, [bta_l2]
    ld.ab   r9, [sp, 4]
    sr  r9, [lp_start]
    ld.ab   r9, [sp, 4]
    sr  r9, [lp_end]
    ld.ab   r9, [sp, 4]
    mov lp_count, r9
    ld.ab   r9, [sp, 4]
    sr  r9, [status32_l2]
    ld.ab   r9, [sp, 4]
    mov ilink2, r9
    ld.ab   blink, [sp, 4]
    ld.ab   fp, [sp, 4]
    ld.ab   r26, [sp, 4]    /* gp */
    RESTORE_CALLER_SAVED

    /* ignoring orig_r0 and orig_r8 */
    ld  sp, [sp, 8] /* restore original sp */

.endm


/*----------------------------------------------------
    vineetg, Dec 30th 2008
    Helper Macros to access the current_task for
    switching to kernel mode stack
----------------------------------------------------*/

#ifdef CONFIG_SMP

/* Get CPU-ID of this core */
.macro  GET_CPU_ID  reg
    lr  \reg, [identity]
    lsr \reg, \reg, 8
    and \reg, \reg, 0xFF
.endm

/*-------------------------------------------------
 * Get current running task on this CPU
 * 1. Determine curr CPU id.
 * 2. Use it to index into _current_task[ ]

.macro  GET_CURR_TASK_ON_CPU_SLOW    reg
    GET_CPU_ID  \reg
    lsl \reg, \reg, 2
    ld  \reg, [_current_task, \reg]
.endm
 */

/* Using the Address Scaling mode to avoid extra instrn
 * Note this wont work with GCC 3.4 Assembler
 * REQuires src oper to be Reg
 */
.macro  GET_CURR_TASK_ON_CPU   reg
    GET_CPU_ID  \reg
    ld.as  \reg, [_current_task, \reg]
.endm

/*-------------------------------------------------
 * Save a new task as the "current" task on this CPU
 * 1. Determine curr CPU id.
 * 2. Use it to index into _current_task[ ]
 */

/* Ideal implementation I wanted, using Address Scaled Indexing
 * so offset in array calculated within st instn
 * GCC 4.2 Assembler doesnt allow this
 * This is because it is not allowed by ABI so no blame on Assmebler

.macro  PUT_CURR_TASK_ON_CPU_IDEAL    out_reg, tmp_reg
    GET_CPU_ID  \tmp_reg
    st.as  \out_reg, [_current_task, \tmp_reg]
.endm
 */

.macro  PUT_CURR_TASK_ON_CPU    out_reg, tmp_reg
    GET_CPU_ID  \tmp_reg
    lsl \tmp_reg, \tmp_reg, 2
    add \tmp_reg, \tmp_reg, _current_task
    st  \out_reg, [\tmp_reg]
#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    mov r25, \out_reg
#endif

.endm


#else   /* Uniprocessor implementation of macros */

.macro  GET_CURR_TASK_ON_CPU    reg
    ld  \reg, [_current_task]
.endm

.macro  PUT_CURR_TASK_ON_CPU    out_reg, tmp_reg
    st  \out_reg, [_current_task]
#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    mov r25, \out_reg
#endif
.endm

#endif /* SMP / UNI */

/* ------------------------------------------------------------------
 *   Get the Ptr to some field of Current Task at @off in task struct
 *       -Out Reg is specified by Caller
 *       -Uses r25 for Current task ptr if that is enabled
 */

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG

.macro GET_CURR_TASK_FIELD_PTR  off,  reg
    add \reg, r25, \off
.endm

#else

.macro GET_CURR_TASK_FIELD_PTR  off,  reg
    GET_CURR_TASK_ON_CPU  \reg
    add \reg, \reg, \off
.endm

#endif

#endif  /* __ASSEMBLY__ */

#endif  /* __ASM_ARC_ENTRY_H */

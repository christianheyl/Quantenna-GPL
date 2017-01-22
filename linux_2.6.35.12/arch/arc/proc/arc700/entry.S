/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TBD:
 *  -remove the low level wrappers for execve/rt_sigreturn/sigaltstack
 *   needed before task_pt_regs was invented. This saves a branch per call
 *   and more importantly allows clean invocation of post-syscall ptrace hook
 *
 * vineetg: May 2011
 *  -Userspace unaligned access emulation
 *
 * vineetg: Feb 2011 (ptrace low level code fixes)
 *  -traced syscall return code (r0) was not saved into pt_regs for restoring
 *   into user reg-file when traded task rets to user space.
 *  -syscalls needing arch-wrappers (mainly for passing sp as pt_regs)
 *   were not invoking post-syscall trace hook (jumping directly into
 *   ret_from_system_call)
 *
 * vineetg: Nov 2010:
 *  -Vector table jumps (@8 bytes) converted into branches (@4 bytes)
 *  -To maintain the slot size of 8 bytes/vector, added nop, which is
 *   not executed at runtime.
 *
 * vineetg: Nov 2009 (Everything needed for TIF_RESTORE_SIGMASK)
 *  -do_signal()invoked upon TIF_RESTORE_SIGMASK as well
 *  -Wrappers for sys_{,rt_}sigsuspend() nolonger needed as they don't
 *   need ptregs anymore
 *
 * Vineetg: Oct 2009
 *  -In a rare scenario, Process gets a Priv-V exception and gets scheduled
 *   out. Since we don't do FAKE RTIE for Priv-V, CPU excpetion state remains
 *   active (AE bit enabled).  This causes a double fault for a subseq valid
 *   exception. Thus FAKE RTIE needed in low level Priv-Violation handler.
 *   Instr Error could also cause similar scenario, so same there as well.
 *
 * Vineetg: July 2009
 *  -Removed CONFIG_ARC_USER_FAULTS_DEBUG (dumping regs in case of fatal
 *    signal in user faults return path) as it is already provided by
 *    /proc/sys/kernel/print-fatal-signals
 *
 * Vineetg: April 2009:
 *  -Check for Valid sys call number in EV_Trap was bogus
 *   sys calls > NR_syscalls was _NOT_ declared bad
 *   sys call  = NR_syscalls was _WRONGLY_ declared bad
 *
 * Vineetg: March 2009 : (Critical look at this file one more time)
 *  -Replaced and.f, nop, bz sequences with bbit0 type instruction
 *  -After returning from preempt_schedule_irq( ) we used to check for
 *   rescheuling again. That is already been done in the generic code
 *  -No need to maintain cpu_stat[].local_irq_count in ISR entry/exit
 *  -Removed the extra hops return_from_interrupt_level[1|2]
 *     while returning from Interrupts
 *  -waitpid no longer needed in syscall Table as uClibc implements it
 *   using wait4 syscall.
 *  -no need to save CALLEE Regs before execve
 *
 * Vineetg: March 2009 (Supporting 2 levels of Interrupts)
 *
 * Vineetg: Feb 2009
 *  -do_softirq( ) was being called twice on ISR return path
 *     First from irq_exit( ) in generic code
 *     Second in ret_from_interrupt_level[1 | 2]  => REMOVED THIS
 *
 * Vineetg: Sept 3rd 2008:
 *  -Added a INSPECT_ON_IRQ macro to be called from Low Level ISR to inspect
 *   a variable getting clobbered (during ISR or between 2 ISRs)
 *  -Don't need FAKE RTIE (re-enabling exceptions before entering Excpn hdlr)
 *   for Instruction Error and Priviledge Violation Exceptions
 *
 * Vineetg: Aug 28th 2008: Bug #94984
 *  -Zero Overhead Loop Context shd be cleared when entering IRQ/EXcp/Trap
 *   Normally CPU does this automatically, however when doing FAKE rtie,
 *   we need to explicitly do this. The problem in macros
 *   FAKE_RET_FROM_EXCPN and FAKE_RET_FROM_EXCPN_LOCK_IRQ was that this bit
 *   was being "CLEARED" rather then "SET". Since it is Loop INHIBIT Bit,
 *   setting it and not clearing it clears ZOL context
 *
 * Vineetg: June 10th 2008
 *   -We can now display all 26 Regs on SegV
 *    For that return from a critical User fault is handled differently from
 *    general return from exceptions (ret_from_user_faults)
 *   -Machine Check doesnt run recursively
 *
 * Vineetg: May 16th, 2008
 *  - r25 now contains the Current Task when in kernel
 *  - SysCall wrappers for exec/signal* needn't push/pop Callee saved Regs
 *  - SysCall wrappers for fork siblings neednot POP callee Regs
 *  - Kernel Stack Switching Code now reused as a macro
 *
 * Vineetg: Dec 22, 2007
 *    Minor Surgery of Low Level ISR to make it SMP safe
 *    - MMU_SCRATCH0 Reg used for freeing up r9 in Level 1 ISR
 *    - _current_task is made an array of NR_CPUS
 *    - Access of _current_task wrapped inside a macro so that if hardware
 *       team agrees for a dedicated reg, no other code is touched
 *
 * Amit Bhor, Rahul Trivedi, Kanika Nema, Sameer Dhavale : Codito Tech 2004
 */

/*
      ------------------------------------------------------------------
      Function                            ABI
      ------------------------------------------------------------------
      Arguments                           r0 - r7
   *  Caller Saved Registers              r0 - r12
   *  Callee Saved Registers              r13- r25
      Global Pointer (gp)                 r26
      Frame Pointer (fp)                  r27
      Stack Pointer (sp)                  r28
      Interrupt link register (ilink1)    r29
      Interrupt link register (ilink2)    r30
      Branch link register (blink)        r31
      ------------------------------------------------------------------
*/

    .cpu A7

;############################ Vector Table #################################

.macro VECTOR  lbl
#if 1   // Just in case, build breaks
    j   \lbl
#else
    b   \lbl
    nop
#endif
.endm

    .section .vector, "ax",@progbits
    .align 4

/* Each entry in the vector table must occupy 2 words. Since it is a jump
 * across sections (.vector to .text) we are gauranteed that 'j somewhere'
 * will use the 'j limm' form of the intrsuction as long as somewhere is in
 * a section other than .vector.
 */

; ********* Critical System Events **********************
VECTOR   res_service             ; 0x0, Restart Vector  (0x0)
VECTOR   mem_service             ; 0x8, Mem exception   (0x1)
VECTOR   instr_service           ; 0x10, Instrn Error   (0x2)

; ******************** Device ISRs **********************
#ifdef  CONFIG_TIMER_LV2
VECTOR   handle_interrupt_level2 ; Timer0 level2        (0x3)
#else
VECTOR   handle_interrupt_level1 ; Timer0 level1        (0x3)
#endif

VECTOR   handle_interrupt_level1 ;                      (0x4)

#ifdef  CONFIG_SERIAL_LV2
VECTOR   handle_interrupt_level2 ; UART level2          (0x5)
#else
VECTOR   handle_interrupt_level1 ; UART level1          (0x5)
#endif

#ifdef  CONFIG_EMAC_LV2
VECTOR   handle_interrupt_level2 ; EMAC level2          (0x6)
#else
VECTOR   handle_interrupt_level1 ; EMAC level1          (0x6)
#endif

.rept   25
VECTOR   handle_interrupt_level1 ; Other devices
.endr

/* FOR ARC600: timer = 0x3, uart = 0x8, emac = 0x10 */

; ******************** Exceptions **********************
VECTOR   EV_MachineCheck         ; 0x100, Fatal Machine check   (0x20)
VECTOR   EV_TLBMissI             ; 0x108, Intruction TLB miss   (0x21)
VECTOR   EV_TLBMissD             ; 0x110, Data TLB miss         (0x22)
VECTOR   EV_TLBProtV             ; 0x118, Protection Violation  (0x23)
                            ;         or Misaligned Access
VECTOR   EV_PrivilegeV           ; 0x120, Privilege Violation   (0x24)
VECTOR   EV_Trap                 ; 0x128, Trap exception        (0x25)
VECTOR   EV_Extension            ; 0x130, Extn Intruction Excp  (0x26)

.rept   24
VECTOR   reserved                ; Reserved Exceptions
.endr


#include <linux/linkage.h>   /* contains declarations of SYMBOL_NAME */
#include <asm/entry.h>       /* For the SAVE_* macros */
#include <asm/errno.h>
#include <asm/arcregs.h>

#include <asm/event-log-asm.h>
#include <asm/board/platform.h>

; ---------------------------------------------
/*    .section .text, "ax",@progbits*/

SYMBOL_NAME_LABEL(res_service)  ; processor restart
    flag    0x1     ; not implemented
    nop
    nop

SYMBOL_NAME_LABEL(reserved) ; processor restart
        rtie        ; jump to processor initializations

;##################### Interrupt Handling ##############################

#ifdef CONFIG_ARCH_ARC_LV2_INTR
; ---------------------------------------------
;  Level 2 ISR: Can interrupt a Level 1 ISR
; ---------------------------------------------
ARC_ENTRY handle_interrupt_level2

    ; TODO-vineetg for SMP this wont work
    ; free up r9 as scratchpad
    st  r9, [SYMBOL_NAME(int2_saved_reg)]

    ;Which mode (user/kernel) was the system in when intr occured
    lr  r9, [status32_l2]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_INT2

    ;------------------------------------------------------
    ; if L2 IRQ interrupted a L1 ISR, disable preemption
    ;------------------------------------------------------

    ld r9, [sp, PT_status32]        ; get statu32_l2 (saved in pt_regs)
    bbit0 r9, STATUS_A1_BIT, 1f     ; L1 not active when L2 IRQ, so normal

    ; A1 is set in status32_l2
    ; bump thread_info->preempt_count (Disable preemption)
    GET_CURR_THR_INFO_FROM_SP(r10)
    ld      r9, [r10, THREAD_INFO_PREEMPT_COUNT]
    add     r9, r9, 1
    st      r9, [r10, THREAD_INFO_PREEMPT_COUNT]

1:
    TAKE_SNAP_C_FROM_ASM  SNAP_INTR_IN2

    ;------------------------------------------------------
    ; setup params for Linux common ISR and invoke it
    ;------------------------------------------------------
    lr  r0, [icause2]
    and r0, r0, 0x1f
    mov r1, sp

    jl  SYMBOL_NAME(process_interrupt)

    mov r8,0x2
    sr r8, [AUX_IRQ_LV12]       ; clear bit in Sticky Status Reg

    b   ret_from_exception

ARC_EXIT handle_interrupt_level2

#endif

; ---------------------------------------------
;  Level 1 ISR
; ---------------------------------------------
ARC_ENTRY handle_interrupt_level1

    /* free up r9 as scratchpad */
#ifdef CONFIG_SMP
    sr  r9, [ARC_REG_SCRATCH_DATA0]
#else
    st   r9, [SYMBOL_NAME(int1_saved_reg)]
#endif

    ;Which mode (user/kernel) was the system in when intr occured
    lr  r9, [status32_l1]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_INT1

    ; snapshot routine takes care of disabling nested intr
    TAKE_SNAP_C_FROM_ASM  SNAP_INTR_IN

    lr  r0, [icause1]
    and r0, r0, 0x1f

    mov r1, sp

#ifdef CONFIG_ARCH_RUBY_SRAM_IRQ_STACK
    brlt sp, RUBY_SRAM_BEGIN, 1f
    brgt sp, RUBY_SRAM_BEGIN + RUBY_SRAM_SIZE, 1f
    b 2f
1:
    mov sp, __irq_stack_end
2:
    st.a r1, [sp, -4]
#endif

    jl  SYMBOL_NAME(process_interrupt)

#ifdef CONFIG_ARCH_RUBY_SRAM_IRQ_STACK
    ld.ab sp, [sp, 4]
#endif

    mov r8,0x1
    sr r8, [AUX_IRQ_LV12]       ; clear bit in Sticky Status Reg

    b   ret_from_exception
ARC_EXIT handle_interrupt_level1

;################### Non TLB Exception Handling #############################

; ---------------------------------------------
; Instruction Error Exception Handler
; ---------------------------------------------

ARC_ENTRY instr_service

    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    ; Get Instruction from ICache and save it for further analysis
    lr r9, [efa]
    sr r9, [ARC_REG_IC_RAM_ADDR]
    lr r9, [ARC_REG_IC_CTRL]
    st r9, [SYMBOL_NAME(ex_ic_ctrl)]
    lr r9, [ARC_REG_IC_DATA]
    st r9, [SYMBOL_NAME(ex_ic_data)]

    TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    lr  r0, [ecr]
    lr  r1, [efa]

    mov r2, sp

    FAKE_RET_FROM_EXCPN r9

    jl  do_insterror_or_kprobe
    b   ret_from_exception
ARC_EXIT instr_service

; ---------------------------------------------
; Memory Error Exception Handler
; ---------------------------------------------

ARC_ENTRY mem_service

    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    lr  r0, [ecr]
    lr  r1, [efa]
    mov r2, sp
    jl  do_memory_error
    b   ret_from_exception
ARC_EXIT mem_service

; ---------------------------------------------
; Machine Check Exception Handler
; ---------------------------------------------

ARC_ENTRY EV_MachineCheck

    st  r9, [SYMBOL_NAME(ex_saved_reg1)]
    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    lr  r0, [ecr]
    lr  r1, [efa]
    mov r2, sp

   cmp     r0, 0x200100 /* Is this an overlapping TLB ? */
   bne     not_tlb_overlap
   jl      do_tlb_overlap_fault
   b       ret_from_exception

not_tlb_overlap:

    ; DEAD END: can't do much, display Regs and HALT
    SAVE_CALLEE_SAVED_USER

    GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
    st  sp, [r10, THREAD_CALLEE_REG]

    j  do_machine_check_fault

ARC_EXIT EV_MachineCheck

; ---------------------------------------------
; Protection Violation Exception Handler
; ---------------------------------------------

ARC_ENTRY EV_TLBProtV

    ; free up r9 as scratchpad
    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

    ;Which mode (user/kernel) was the system in when Exception occured
    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    ;---------(3) Save some more regs-----------------
    ;  vineetg: Mar 6th: Random Seg Fault issue #1
    ;  ecr and efa were not saved in case an Intr sneaks in
    ;  after fake rtie
    ;
    lr  r3, [ecr]
    lr  r4, [efa]

    ; --------(4) Return from CPU Exception Mode ---------
    ;  Fake a rtie, but rtie to next label
    ;  That way, subsequently, do_page_fault ( ) executes in pure kernel
    ;  mode with further Exceptions enabled

    FAKE_RET_FROM_EXCPN r9

    ;------ (5) Type of Protection Violation? ----------
    ;
    ; ProtV Hardware Exception is triggered for Access Faults of 2 types
    ;   -Access Violaton (WRITE to READ ONLY Page)
    ;   -Unaligned Access (READ/WRITE on odd boundary)
    ;
    ; Handling for these 2 are completely difft in Linux
    ;   -Access Violation may be legitimate as Linux implements COW using it.
    ;   -Unaligned Access is wrong, but we'll try and recover from it
    ;
    cmp r3, 0x230400    ; Misaligned data access ?
    beq 4f

    ;========= (6a) Access Violation Processing ========
    cmp r3, 0x230100
    mov r1, 0x0              ; if LD exception ? write = 0
    mov.ne r1, 0x1           ; else write = 1

    mov r2, r4              ; faulting address
    mov r0, sp              ; pt_regs
    jl  do_page_fault
    b   ret_from_exception

    ;========== (6b) Non aligned access ============
4:
    mov r0, r3              ; cause code
    mov r1, r4              ; faulting address
    mov r2, sp              ; pt_regs

#ifdef  CONFIG_ARC_MISALIGNED_ACCESS
    SAVE_CALLEE_SAVED_USER
    mov r3, sp              ; callee_regs
#endif

    jl  do_misaligned_access

#ifdef  CONFIG_ARC_MISALIGNED_ACCESS
    RESTORE_CALLEE_SAVED_KERNEL
#endif

    b   ret_from_exception

ARC_EXIT EV_TLBProtV

; ---------------------------------------------
; Privilege Violation Exception Handler
; ---------------------------------------------
ARC_ENTRY EV_PrivilegeV
    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    lr  r0, [ecr]
    lr  r1, [efa]
    mov r2, sp

    FAKE_RET_FROM_EXCPN r9

    jl  do_privilege_fault
    b   ret_from_exception
ARC_EXIT EV_PrivilegeV

; ---------------------------------------------
; Extension Instruction Exception Handler
; ---------------------------------------------
ARC_ENTRY EV_Extension
    st  r9, [SYMBOL_NAME(ex_saved_reg1)]
    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    lr  r0, [ecr]
    lr  r1, [efa]
    mov r2, sp
    jl  do_extension_fault
    b   ret_from_exception
ARC_EXIT EV_Extension

;######################### System Call Tracing #########################

tracesys:
    ; save EFA in case tracer wants the PC of traced task
    ; using ERET won't work since next-PC has already committed
    lr  r12, [efa]
    GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r11
    st  r12, [r11, THREAD_FAULT_ADDR]

    ; PRE Sys Call Ptrace hook
    jl  SYMBOL_NAME(syscall_trace)

    ; Do the Sys Call as we normally would.
    ; Validate the Sys Call number
    ld  r8, [sp, PT_r8]
    cmp     r8,  NR_syscalls
    mov.hi  r0, -ENOSYS
    bhi     tracesys_exit

    ; Restore the sys-call args. Mere invocation of the hook abv could have
    ; clobbered them (since they are in scratch regs). The tracer could also
    ; have deliberately changed the args:  r0-r7 - Sys Calls Args
    ; Note that r8 - Sys Call Num has already been restored above
    ld  r0, [sp, PT_r0]
    ld  r1, [sp, PT_r1]
    ld  r2, [sp, PT_r2]
    ld  r3, [sp, PT_r3]
    ld  r4, [sp, PT_r4]
    ld  r5, [sp, PT_r5]
    ld  r6, [sp, PT_r6]
    ld  r7, [sp, PT_r7]
    ld.as   r9, [sys_call_table, r8]
    jl      [r9]        ; Entry into Sys Call Handler

tracesys_exit:
    st  r0, [sp, PT_r0]     ; sys call return value in pt_regs

    ;POST Sys Call Ptrace Hook
    jl  SYMBOL_NAME(syscall_trace)
    b   ret_from_exception ; NOT ret_from_system_call at is saves r0 which
                           ; we'd done before calling post hook above

;################### Break Point TRAP ##########################

    ; ======= (5b) Trap is due to Break-Point =========

trap_with_param:

    ;make sure orig_r8 is a positive value
    st  NR_syscalls + 2, [sp, PT_orig_r8]

    mov r0, r12
    lr  r1, [efa]
    mov r2, sp

    ; Now that we have read EFA, its safe to do "fake" rtie
    ;   and get out of CPU exception mode
    FAKE_RET_FROM_EXCPN r11

    ; Save callee regs in case gdb wants to have a look
    ; SP will grow up by size of CALLEE Reg-File
    ; NOTE: clobbers r12
    SAVE_CALLEE_SAVED_USER

    ; save location of saved Callee Regs @ thread_struct->pc
    GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
    st  sp, [r10, THREAD_CALLEE_REG]

    ; Call the trap handler
    jl  do_trap

    ; unwind stack to discard Callee saved Regs
    DISCARD_CALLEE_SAVED_USER

    b   ret_from_exception

;##################### Trap Handling ##############################
;
; EV_Trap caused by TRAP_S and TRAP0 instructions.
;------------------------------------------------------------------
;   (1) System Calls
;       :parameters in r0-r7.
;       :r8 has the system call number
;   (2) Break Points
;------------------------------------------------------------------

ARC_ENTRY EV_Trap

    ; free up r9 as scratchpad
    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    TAKE_SNAP_ASM r10, r9,  SNAP_TRAP_IN

    ;Which mode (user/kernel) was the system in when intr occured
    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_TRAP

    ;------- (4) What caused the Trap --------------
    lr     r12, [ecr]
    and.f  0, r12, ECAUSE_PARAMETER_MASK
    bnz    trap_with_param

    ; ======= (5a) Trap is due to System Call ========

    ; Before doing anything, return from CPU Exception Mode
    FAKE_RET_FROM_EXCPN r11

    ; Is this task being TRACED and Is Tracer interested in System Call tracing
    GET_CURR_THR_INFO_FROM_SP   r9
    ld  r10, [r9, THREAD_INFO_FLAGS]
    and.f   0, r10, _TIF_SYSCALL_TRACE

    ; If yes, call the system call with the pre and post tracing hooks
    bnz tracesys  ; this never comes back

    ;============ This is normal System Call case ==========
    ; Sys-call num shd not exceed the total system calls avail
    cmp     r8,  NR_syscalls
    mov.hi  r0, -ENOSYS
    bhi     ret_from_system_call

    ; Offset into the syscall_table and call handler
    ld.as   r9,[sys_call_table, r8]
    jl      [r9]        ; Entry into Sys Call Handler

    ; fall through to ret_from_system_call

ARC_EXIT EV_Trap
ARC_ENTRY ret_from_system_call

    st  r0, [sp, PT_r0]     ; sys call return value in pt_regs

    ; fall through yet again
    ; NOTE: ret_from_exception must immediately follow ret_from_system_call

;################### Return from Intr/Excp ##########################

ARC_ENTRY ret_from_exception

    ld  r8, [sp, PT_status32]   ; returning to User/Kernel Mode

#ifdef CONFIG_PREEMPT

    bbit0  r8, STATUS_U_BIT, resume_kernel_mode

#else

    bbit0  r8, STATUS_U_BIT, restore_regs

#endif

    ; else returning to user mode
resume_user_mode:

    ; TODO: Fast PAth chk: Can we rush back to user mode?


    ; --------- do we need to resched the current user task ----
    ; check Thread_info->flags for need sched flag
    GET_CURR_THR_INFO_FROM_SP(r10)
    ld  r9, [r10, THREAD_INFO_FLAGS]
    bbit0  r9, TIF_NEED_RESCHED, skip_user_resched

    ; User Task Preemption
    jl  schedule
    b   ret_from_exception

skip_user_resched:
    ; --------- check for signals/restore-sigmask ------------
    and.f  0, r9, (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK)
    bz  restore_regs

    ; setup args for "C" routine, do_signal ( )

    mov r0, sp      ;  pass ptr to pt_regs

    ; save CALLEE Regs.
    ; (i) For core dump: Incase this signal causes one
    ; (ii) If this is due to SIGTRAP/SIGSTOP, task is being traced thus
    ;      tracer might call PEEKUSR for a callee reg
    ;
    ; NOTE: SP will grow up by size of CALLEE Reg-File
    ; NOTE: macro clobbers r12
    SAVE_CALLEE_SAVED_USER

    ; save location of saved Callee Regs @ thread_struct->pc
    GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
    st  sp,[r10, THREAD_CALLEE_REG]

    jl  SYMBOL_NAME(do_signal)

    ; unwind stack to discard Callee saved Regs
    DISCARD_CALLEE_SAVED_USER

    ;--------- All things done, go back to Userland ------

    b restore_regs


#ifdef CONFIG_PREEMPT

resume_kernel_mode:

    ; check preempt_count
    GET_CURR_THR_INFO_FROM_SP(r10)
    ld  r8, [r10, THREAD_INFO_PREEMPT_COUNT]

    ; Preemption disabled, go back to whatever we were doing before
    brne  r8, 0, restore_regs

    ; check if this task's NEED_RESCHED flag set
    ld  r9, [r10, THREAD_INFO_FLAGS]
    bbit0  r9, TIF_NEED_RESCHED, restore_regs

    ; All set to invoke preemption.
    ; NEED to make sure that further IRQs dont happen.
    lr  r9, [status32]
    and r9, r9, ~(STATUS_E1_MASK | STATUS_E2_MASK)
    flag r9

    ; Invoke PREEMPTION
    jl      preempt_schedule_irq

#endif

    ; fall through

    ; Restore the saved sys context (common exit-path for EXCPN/IRQ/Trap)
    ; IRQ shd definitely not happen between now and rtie

restore_regs :

    ; Disable Interrupts first
    lr  r10, [status32]
    mov r9, r10 ;make a copy. Didn't they teach u to "save" things ?
    and r9, r9, ~(STATUS_E1_MASK | STATUS_E2_MASK)
    flag r9

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    ; Restore User R25
    ; Earlier this used to be only for returning to user mode
    ; However with 2 levels of IRQ this can also happen even if
    ; in kernel mode
    ld r9, [sp, PT_sp]
    brhs r9, VMALLOC_START, 8f
    RESTORE_USER_R25
8:
#endif

    ; Restore REG File. In case multiple Events outstanding,
    ; use the same priorty as rtie: EXCPN, L2 IRQ, L1 IRQ, None

    ; if Returning from Exception
    bbit0  r10, STATUS_AE_BIT, not_exception
    RESTORE_ALL_SYS
    rtie

    ; Not Exception so maybe Interrupts (Level 1 or 2)

not_exception:

#ifdef CONFIG_ARCH_ARC_LV2_INTR

    bbit0  r10, STATUS_A2_BIT, not_level2_interrupt

    ;------------------------------------------------------------------
    ; if L2 IRQ interrupted a L1 ISR,  we'd disbaled preemption earlier
    ; so that sched doesnt move to new task, causing L1 to be delayed
    ; undeterministically. Now that we've achieved that, lets reset
    ; things to what they were, before returning from L2 context
    ;----------------------------------------------------------------

    ld r9, [sp, PT_orig_r8]        ; get orig_r8 to make sure it is
    brne r9, -2, 149f              ; infact a L2 ISR ret path

    ld r9, [sp, PT_status32]        ; get statu32_l2 (saved in pt_regs)
    bbit0 r9, STATUS_A1_BIT, 149f     ; L1 not active when L2 IRQ, so normal

    ; A1 is set in status32_l2
    ; decrement thread_info->preempt_count (re-enable preemption)
    GET_CURR_THR_INFO_FROM_SP(r10)
    ld      r9, [r10, THREAD_INFO_PREEMPT_COUNT]

    ; paranoid check, given A1 was active when A2 happened, preempt count
    ; must not be 0 beccause we would have incremented it.
    ; If this does happen we simply HALT as it means a BUG !!!
    cmp     r9, 0
    bnz     2f
    flag 1

2:
    sub     r9, r9, 1
    st      r9, [r10, THREAD_INFO_PREEMPT_COUNT]

149:
    TAKE_SNAP_C_FROM_ASM SNAP_INTR_OUT2
    ;return from level 2
    RESTORE_ALL_INT2
debug_marker_l2:
    rtie

not_level2_interrupt:

#endif

    bbit0  r10, STATUS_A1_BIT, not_level1_interrupt

    ;return from level 1
    TAKE_SNAP_C_FROM_ASM SNAP_INTR_OUT

    RESTORE_ALL_INT1
debug_marker_l1:
    rtie

not_level1_interrupt:

    ;this case is for syscalls or Exceptions (with fake rtie)

#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
    ld r8, [sp, PT_orig_r8]
    cmp r8, NR_syscalls+1
    jeq  149f
    TAKE_SNAP_ASM r9, r10, SNAP_TRAP_OUT
    j 150f

149:
    TAKE_SNAP_ASM r9, r10, SNAP_EXCP_OUT
150:

#endif

    RESTORE_ALL_SYS
debug_marker_syscall:
    rtie

ARC_EXIT ret_from_exception

ARC_ENTRY ret_from_fork
    ; when the forked child comes here from the __switch_to function
    ; r0 has the last task pointer.
    ; put last task in scheduler queue
    jl  SYMBOL_NAME(schedule_tail)
    b SYMBOL_NAME(ret_from_exception)
ARC_EXIT ret_from_fork

;################### Special Sys Call Wrappers ##########################

ARC_ENTRY sys_execve_wrapper
    ; copy pointer to pt_regs as a parameter
    mov  r3, sp
    jl  SYMBOL_NAME(sys_execve)

    GET_CURR_THR_INFO_FROM_SP   r9
    ld  r10, [r9, THREAD_INFO_FLAGS]
    and.f   0, r10, _TIF_SYSCALL_TRACE

    bz ret_from_system_call
    b  tracesys_exit
ARC_EXIT sys_execve_wrapper

ARC_ENTRY sys_fork_wrapper
    ; copy pointer to pt_regs as a parameter
    mov  r0, sp
    SAVE_CALLEE_SAVED_USER
    jl  SYMBOL_NAME(sys_fork)
    DISCARD_CALLEE_SAVED_USER

    GET_CURR_THR_INFO_FROM_SP   r9
    ld  r10, [r9, THREAD_INFO_FLAGS]
    and.f   0, r10, _TIF_SYSCALL_TRACE

    bz ret_from_system_call
    b  tracesys_exit
ARC_EXIT sys_fork_wrapper

ARC_ENTRY sys_vfork_wrapper
    ; copy pointer to pt_regs as a parameter
    mov  r0, sp
    SAVE_CALLEE_SAVED_USER
    jl  SYMBOL_NAME(sys_vfork)
    DISCARD_CALLEE_SAVED_USER

    GET_CURR_THR_INFO_FROM_SP   r9
    ld  r10, [r9, THREAD_INFO_FLAGS]
    and.f   0, r10, _TIF_SYSCALL_TRACE

    bz ret_from_system_call
    b  tracesys_exit
ARC_EXIT sys_vfork_wrapper

ARC_ENTRY sys_clone_wrapper
    ; clone sys-call takes 2 mandatary args: @flags and @child-stack
    ; and it can take 3 var-args, depending on flags
    ; To keep sys_clone( ) signature constant, we assume all 5 args
    ; and set the helper @prtegs in next free reg
    ; this is cheap since our args are in regs, not on stack
    mov r5, sp  ; pt_regs
    SAVE_CALLEE_SAVED_USER
    jl  SYMBOL_NAME(sys_clone)
    DISCARD_CALLEE_SAVED_USER

    GET_CURR_THR_INFO_FROM_SP   r9
    ld  r10, [r9, THREAD_INFO_FLAGS]
    and.f   0, r10, _TIF_SYSCALL_TRACE

    bz ret_from_system_call
    b  tracesys_exit
ARC_EXIT sys_clone_wrapper

;################### Sys Call Table ##########################

; The syscall table starts here...
.data
ARC_ENTRY sys_call_table
__syscall_start:
  /* 0 */   .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_exit)
        .long   SYMBOL_NAME(sys_fork_wrapper)
        .long   SYMBOL_NAME(sys_read)
        .long   SYMBOL_NAME(sys_write)
  /* 5 */   .long   SYMBOL_NAME(sys_open)
        .long   SYMBOL_NAME(sys_close)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_waitpid */
        .long   SYMBOL_NAME(sys_creat)
        .long   SYMBOL_NAME(sys_link)
  /* 10 */  .long   SYMBOL_NAME(sys_unlink)
        .long   SYMBOL_NAME(sys_execve_wrapper)
        .long   SYMBOL_NAME(sys_chdir)
        .long   SYMBOL_NAME(sys_time) /* used by libc4 */
        .long   SYMBOL_NAME(sys_mknod)
  /* 15 */  .long   SYMBOL_NAME(sys_chmod)
        .long   SYMBOL_NAME(sys_lchown16)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_break */
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_stat */
        .long   SYMBOL_NAME(sys_lseek)
  /* 20 */  .long   SYMBOL_NAME(sys_getpid)
        .long   SYMBOL_NAME(sys_mount)
        .long   SYMBOL_NAME(sys_oldumount) /* used by libc4 */
        .long   SYMBOL_NAME(sys_setuid16)
        .long   SYMBOL_NAME(sys_getuid16)
  /* 25 */  .long   SYMBOL_NAME(sys_stime)
        .long   SYMBOL_NAME(sys_ptrace)
        .long   SYMBOL_NAME(sys_alarm) /* used by libc4 */
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_fstat */
        .long   SYMBOL_NAME(sys_pause)
  /* 30 */  .long   SYMBOL_NAME(sys_utime) /* used by libc4 */
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_stty */
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_getty */
        .long   SYMBOL_NAME(sys_access)
        .long   SYMBOL_NAME(sys_nice)
  /* 35 */  .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_ftime */
        .long   SYMBOL_NAME(sys_sync)
        .long   SYMBOL_NAME(sys_kill)
        .long   SYMBOL_NAME(sys_rename)
        .long   SYMBOL_NAME(sys_mkdir)
  /* 40 */  .long   SYMBOL_NAME(sys_rmdir)
        .long   SYMBOL_NAME(sys_dup)
        .long   SYMBOL_NAME(sys_pipe)
        .long   SYMBOL_NAME(sys_times)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_prof */
  /* 45 */  .long   SYMBOL_NAME(sys_brk)
        .long   SYMBOL_NAME(sys_setgid16)
        .long   SYMBOL_NAME(sys_getgid16)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_signal */
        .long   SYMBOL_NAME(sys_geteuid16)
  /* 50 */  .long   SYMBOL_NAME(sys_getegid16)
        .long   SYMBOL_NAME(sys_acct)
        .long   SYMBOL_NAME(sys_umount)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_lock */
        .long   SYMBOL_NAME(sys_ioctl)
  /* 55 */  .long   SYMBOL_NAME(sys_fcntl)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_mpx */
        .long   SYMBOL_NAME(sys_setpgid)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_ulimit */
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_olduname */
  /* 60 */  .long   SYMBOL_NAME(sys_umask)
        .long   SYMBOL_NAME(sys_chroot)
        .long   SYMBOL_NAME(sys_ustat)
        .long   SYMBOL_NAME(sys_dup2)
        .long   SYMBOL_NAME(sys_getppid)
  /* 65 */  .long   SYMBOL_NAME(sys_getpgrp)
        .long   SYMBOL_NAME(sys_setsid)
        .long   SYMBOL_NAME(sys_sigaction)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_sgetmask */
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_ssetmask */
  /* 70 */  .long   SYMBOL_NAME(sys_setreuid16)
        .long   SYMBOL_NAME(sys_setregid16)
        .long   SYMBOL_NAME(sys_sigsuspend)
        .long   SYMBOL_NAME(sys_sigpending)
        .long   SYMBOL_NAME(sys_sethostname)
  /* 75 */  .long   SYMBOL_NAME(sys_setrlimit)
        .long   SYMBOL_NAME(sys_old_getrlimit) /* used by libc4 */
        .long   SYMBOL_NAME(sys_getrusage)
        .long   SYMBOL_NAME(sys_gettimeofday)
        .long   SYMBOL_NAME(sys_settimeofday)
 /* 80 */   .long   SYMBOL_NAME(sys_getgroups16)
        .long   SYMBOL_NAME(sys_setgroups16)
        .long   SYMBOL_NAME(old_select) /* used by libc4 */
        .long   SYMBOL_NAME(sys_symlink)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_lstat */
  /* 85 */  .long   SYMBOL_NAME(sys_readlink)
        .long   SYMBOL_NAME(sys_uselib)
        .long   SYMBOL_NAME(sys_swapon)
        .long   SYMBOL_NAME(sys_reboot)
        .long   SYMBOL_NAME(sys_old_readdir) /* used by libc4 */
  /* 90 */  .long   SYMBOL_NAME(old_mmap) /* used by libc4 */
        .long   SYMBOL_NAME(sys_arc_munmap)
        .long   SYMBOL_NAME(sys_truncate)
        .long   SYMBOL_NAME(sys_ftruncate)
        .long   SYMBOL_NAME(sys_fchmod)
  /* 95 */  .long   SYMBOL_NAME(sys_fchown16)
        .long   SYMBOL_NAME(sys_getpriority)
        .long   SYMBOL_NAME(sys_setpriority)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_profil */
        .long   SYMBOL_NAME(sys_statfs)
  /* 100 */ .long   SYMBOL_NAME(sys_fstatfs)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_socketcall)
        .long   SYMBOL_NAME(sys_syslog)
        .long   SYMBOL_NAME(sys_setitimer)
  /* 105 */ .long   SYMBOL_NAME(sys_getitimer)
        .long   SYMBOL_NAME(sys_newstat)
        .long   SYMBOL_NAME(sys_newlstat)
        .long   SYMBOL_NAME(sys_newfstat)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_uname */
  /* 110 */ .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_iopl */
        .long   SYMBOL_NAME(sys_vhangup)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_ni_syscall) /* vm86old for i386 */
        .long   SYMBOL_NAME(sys_wait4)
  /* 115 */ .long   SYMBOL_NAME(sys_swapoff)
        .long   SYMBOL_NAME(sys_sysinfo)
        .long   SYMBOL_NAME(sys_ipc)
        .long   SYMBOL_NAME(sys_fsync)
        .long   SYMBOL_NAME(sys_sigreturn)
/* 120 */   .long   SYMBOL_NAME(sys_clone_wrapper)
        .long   SYMBOL_NAME(sys_setdomainname)
        .long   SYMBOL_NAME(sys_newuname)
        .long   SYMBOL_NAME(sys_cacheflush)
        .long   SYMBOL_NAME(sys_adjtimex)
 /* 125 */  .long   SYMBOL_NAME(sys_mprotect)
        .long   SYMBOL_NAME(sys_sigprocmask)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_init_module)
        .long   SYMBOL_NAME(sys_delete_module)
 /* 130 */  .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_quotactl)
        .long   SYMBOL_NAME(sys_getpgid)
        .long   SYMBOL_NAME(sys_fchdir)
        .long   SYMBOL_NAME(sys_bdflush)
 /* 135 */  .long   SYMBOL_NAME(sys_sysfs)
        .long   SYMBOL_NAME(sys_personality)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_setfsuid16)
        .long   SYMBOL_NAME(sys_setfsgid16)
 /* 140 */  .long   SYMBOL_NAME(sys_llseek)
        .long   SYMBOL_NAME(sys_getdents)
        .long   SYMBOL_NAME(sys_select)
        .long   SYMBOL_NAME(sys_flock)
        .long   SYMBOL_NAME(sys_msync)
 /* 145 */  .long   SYMBOL_NAME(sys_readv)
        .long   SYMBOL_NAME(sys_writev)
        .long   SYMBOL_NAME(sys_getsid)
        .long   SYMBOL_NAME(sys_fdatasync)
        .long   SYMBOL_NAME(sys_sysctl)
 /* 150 */  .long   SYMBOL_NAME(sys_mlock)
        .long   SYMBOL_NAME(sys_munlock)
        .long   SYMBOL_NAME(sys_mlockall)
        .long   SYMBOL_NAME(sys_munlockall)
        .long   SYMBOL_NAME(sys_sched_setparam)
 /* 155 */  .long   SYMBOL_NAME(sys_sched_getparam)
        .long   SYMBOL_NAME(sys_sched_setscheduler)
        .long   SYMBOL_NAME(sys_sched_getscheduler)
        .long   SYMBOL_NAME(sys_sched_yield)
        .long   SYMBOL_NAME(sys_sched_get_priority_max)
 /* 160 */  .long   SYMBOL_NAME(sys_sched_get_priority_min)
        .long   SYMBOL_NAME(sys_sched_rr_get_interval)
        .long   SYMBOL_NAME(sys_nanosleep)
        .long   SYMBOL_NAME(sys_mremap)
        .long   SYMBOL_NAME(sys_setresuid16)
 /* 165 */  .long   SYMBOL_NAME(sys_getresuid16)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_poll)
        .long   SYMBOL_NAME(sys_nfsservctl)
 /* 170 */  .long   SYMBOL_NAME(sys_setresgid16)
        .long   SYMBOL_NAME(sys_getresgid16)
        .long   SYMBOL_NAME(sys_prctl)
        .long   SYMBOL_NAME(sys_rt_sigreturn)
        .long   SYMBOL_NAME(sys_rt_sigaction)
 /* 175 */  .long   SYMBOL_NAME(sys_rt_sigprocmask)
        .long   SYMBOL_NAME(sys_rt_sigpending)
        .long   SYMBOL_NAME(sys_rt_sigtimedwait)
        .long   SYMBOL_NAME(sys_rt_sigqueueinfo)
        .long   SYMBOL_NAME(sys_rt_sigsuspend)
 /* 180 */  .long   SYMBOL_NAME(sys_pread64)
        .long   SYMBOL_NAME(sys_pwrite64)
        .long   SYMBOL_NAME(sys_chown16)
        .long   SYMBOL_NAME(sys_getcwd)
        .long   SYMBOL_NAME(sys_capget)
 /* 185 */  .long   SYMBOL_NAME(sys_capset)
        .long   SYMBOL_NAME(sys_sigaltstack)
        .long   SYMBOL_NAME(sys_sendfile)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_ni_syscall)
  /* 190 */ .long   SYMBOL_NAME(sys_vfork_wrapper)
        .long   SYMBOL_NAME(sys_getrlimit)
        .long   SYMBOL_NAME(sys_mmap2)
        .long   SYMBOL_NAME(sys_truncate64)
        .long   SYMBOL_NAME(sys_ftruncate64)
 /* 195 */  .long   SYMBOL_NAME(sys_stat64)
        .long   SYMBOL_NAME(sys_lstat64)
        .long   SYMBOL_NAME(sys_fstat64)
        .long   SYMBOL_NAME(sys_lchown)
        .long   SYMBOL_NAME(sys_getuid)
 /* 200 */  .long   SYMBOL_NAME(sys_getgid)
        .long   SYMBOL_NAME(sys_geteuid)
        .long   SYMBOL_NAME(sys_getegid)
        .long   SYMBOL_NAME(sys_setreuid)
        .long   SYMBOL_NAME(sys_setregid)
 /* 205 */  .long   SYMBOL_NAME(sys_getgroups)
        .long   SYMBOL_NAME(sys_setgroups)
        .long   SYMBOL_NAME(sys_fchown)
        .long   SYMBOL_NAME(sys_setresuid)
        .long   SYMBOL_NAME(sys_getresuid)
 /* 210 */  .long   SYMBOL_NAME(sys_setresgid)
        .long   SYMBOL_NAME(sys_getresgid)
        .long   SYMBOL_NAME(sys_chown)
        .long   SYMBOL_NAME(sys_setuid)
        .long   SYMBOL_NAME(sys_setgid)
 /* 215 */  .long   SYMBOL_NAME(sys_setfsuid)
        .long   SYMBOL_NAME(sys_setfsgid)
        .long   SYMBOL_NAME(sys_pivot_root)
        .long   SYMBOL_NAME(sys_mincore)
        .long   SYMBOL_NAME(sys_madvise)
 /* 220 */  .long   SYMBOL_NAME(sys_getdents64)
        .long   SYMBOL_NAME(sys_fcntl64)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_ni_syscall)
        .long   SYMBOL_NAME(sys_gettid)
/* 225 */   .long   SYMBOL_NAME(sys_lookup_dcookie)
        .long   SYMBOL_NAME(sys_statfs64)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_waitpid */
        .long   SYMBOL_NAME(sys_mq_open)
        .long   SYMBOL_NAME(sys_mq_unlink)
/* 230 */   .long   SYMBOL_NAME(sys_mq_timedreceive)
        .long   SYMBOL_NAME(sys_mq_notify)
        .long   SYMBOL_NAME(sys_mq_getsetattr)
        .long   SYMBOL_NAME(sys_mq_timedsend)
        .long   SYMBOL_NAME(sys_timer_create)
/* 235 */   .long   SYMBOL_NAME(sys_timer_settime)
        .long   SYMBOL_NAME(sys_timer_gettime)
        .long   SYMBOL_NAME(sys_timer_getoverrun)
        .long   SYMBOL_NAME(sys_timer_delete)
        .long   SYMBOL_NAME(sys_clock_settime)
/* 240 */   .long   SYMBOL_NAME(sys_clock_gettime)
        .long   SYMBOL_NAME(sys_clock_getres)
        .long   SYMBOL_NAME(sys_clock_nanosleep)
        .long   SYMBOL_NAME(sys_sched_setaffinity)
        .long   SYMBOL_NAME(sys_sched_getaffinity)
/* 245 */   .long   SYMBOL_NAME(sys_waitid)
        .long   SYMBOL_NAME(sys_restart_syscall)
        .long   SYMBOL_NAME(sys_pread64)
        .long   SYMBOL_NAME(sys_pwrite64)
        .long   SYMBOL_NAME(sys_mincore)
/* 250 */        .long   SYMBOL_NAME(sys_madvise)
        .long   SYMBOL_NAME(sys_readahead)
        .long   SYMBOL_NAME(sys_setxattr)
        .long   SYMBOL_NAME(sys_lsetxattr)
        .long   SYMBOL_NAME(sys_fsetxattr)
/* 255 */        .long   SYMBOL_NAME(sys_getxattr)
        .long   SYMBOL_NAME(sys_lgetxattr)
        .long   SYMBOL_NAME(sys_fgetxattr)
        .long   SYMBOL_NAME(sys_listxattr)
        .long   SYMBOL_NAME(sys_llistxattr)
/* 260 */        .long   SYMBOL_NAME(sys_flistxattr)
        .long   SYMBOL_NAME(sys_removexattr)
        .long   SYMBOL_NAME(sys_lremovexattr)
        .long   SYMBOL_NAME(sys_fremovexattr)
        .long   SYMBOL_NAME(sys_tkill)
/* 265 */        .long   SYMBOL_NAME(sys_sendfile64)
        .long   SYMBOL_NAME(sys_futex)
        .long   SYMBOL_NAME(sys_io_setup)
        .long   SYMBOL_NAME(sys_io_destroy)
        .long   SYMBOL_NAME(sys_io_getevents)
/* 270 */        .long   SYMBOL_NAME(sys_io_submit)
        .long   SYMBOL_NAME(sys_io_cancel)
        .long   SYMBOL_NAME(sys_ni_syscall) /* was sys_fadvise64 */
        .long   SYMBOL_NAME(sys_exit_group)
        .long   SYMBOL_NAME(sys_epoll_create)
/* 275 */        .long   SYMBOL_NAME(sys_epoll_ctl)
        .long   SYMBOL_NAME(sys_epoll_wait)
        .long   SYMBOL_NAME(sys_remap_file_pages)
        .long   SYMBOL_NAME(sys_set_tid_address)
        .long   SYMBOL_NAME(sys_fstatfs64)
/* 280 */        .long   SYMBOL_NAME(sys_tgkill)
        .long   SYMBOL_NAME(sys_utimes)
        .long   SYMBOL_NAME(sys_fadvise64_64)
        .long   SYMBOL_NAME(sys_mbind)
        .long   SYMBOL_NAME(sys_get_mempolicy)
/* 285 */        .long   SYMBOL_NAME(sys_set_mempolicy)
        .long   SYMBOL_NAME(sys_kexec_load)
        .long   SYMBOL_NAME(sys_add_key)
        .long   SYMBOL_NAME(sys_request_key)
        .long   SYMBOL_NAME(sys_keyctl)
/* 290 */        .long   SYMBOL_NAME(sys_ioprio_set)
        .long   SYMBOL_NAME(sys_ioprio_get)
        .long   SYMBOL_NAME(sys_inotify_init)
        .long   SYMBOL_NAME(sys_inotify_add_watch)
        .long   SYMBOL_NAME(sys_inotify_rm_watch)
/* 295 */        .long   SYMBOL_NAME(sys_migrate_pages)
        .long   SYMBOL_NAME(sys_openat)
        .long   SYMBOL_NAME(sys_mkdirat)
        .long   SYMBOL_NAME(sys_mknodat)
        .long   SYMBOL_NAME(sys_fchownat)
/* 300 */        .long   SYMBOL_NAME(sys_futimesat)
        .long   SYMBOL_NAME(sys_fstatat64)
        .long   SYMBOL_NAME(sys_unlinkat)
        .long   SYMBOL_NAME(sys_renameat)
        .long   SYMBOL_NAME(sys_linkat)
/* 305 */        .long   SYMBOL_NAME(sys_symlinkat)
        .long   SYMBOL_NAME(sys_readlinkat)
        .long   SYMBOL_NAME(sys_fchmodat)
        .long   SYMBOL_NAME(sys_faccessat)
        .long   SYMBOL_NAME(sys_pselect6)
/* 310 */        .long   SYMBOL_NAME(sys_ppoll)
        .long   SYMBOL_NAME(sys_unshare)
        .long   SYMBOL_NAME(sys_set_robust_list)
        .long   SYMBOL_NAME(sys_get_robust_list)
        .long   SYMBOL_NAME(sys_splice)
/* 315 */        .long   SYMBOL_NAME(sys_sync_file_range)
        .long   SYMBOL_NAME(sys_tee)
        .long   SYMBOL_NAME(sys_vmsplice)
        .long   SYMBOL_NAME(sys_move_pages)
        .long   SYMBOL_NAME(sys_getcpu)
/* 320 */        .long   SYMBOL_NAME(sys_epoll_pwait)
        .long   SYMBOL_NAME(sys_utimensat)
        .long   SYMBOL_NAME(sys_signalfd)
        .long   SYMBOL_NAME(sys_timerfd_create)
        .long   SYMBOL_NAME(sys_eventfd)
/* 325 */        .long   SYMBOL_NAME(sys_fallocate)
        .long   SYMBOL_NAME(sys_timerfd_settime)
        .long   SYMBOL_NAME(sys_timerfd_gettime)
        .long   SYMBOL_NAME(sys_signalfd4)
        .long   SYMBOL_NAME(sys_eventfd2)
/* 330 */        .long   SYMBOL_NAME(sys_epoll_create1)
        .long   SYMBOL_NAME(sys_dup3)
        .long   SYMBOL_NAME(sys_pipe2)
        .long   SYMBOL_NAME(sys_inotify_init1)
        .long   SYMBOL_NAME(sys_preadv)
/* 335 */        .long   SYMBOL_NAME(sys_pwritev)
        .long   SYMBOL_NAME(sys_socket)
        .long   SYMBOL_NAME(sys_bind)
        .long   SYMBOL_NAME(sys_connect)
        .long   SYMBOL_NAME(sys_listen)
/*340*/ .long   SYMBOL_NAME(sys_accept)
        .long   SYMBOL_NAME(sys_getsockname)
        .long   SYMBOL_NAME(sys_getpeername)
        .long   SYMBOL_NAME(sys_socketpair)
        .long   SYMBOL_NAME(sys_send)
/*345*/ .long   SYMBOL_NAME(sys_sendto)
        .long   SYMBOL_NAME(sys_recv)
        .long   SYMBOL_NAME(sys_recvfrom)
        .long   SYMBOL_NAME(sys_shutdown)
        .long   SYMBOL_NAME(sys_setsockopt)
/*350*/ .long   SYMBOL_NAME(sys_getsockopt)
        .long   SYMBOL_NAME(sys_sendmsg)
        .long   SYMBOL_NAME(sys_recvmsg)
        .long   SYMBOL_NAME(sys_arc_settls)
        .long   SYMBOL_NAME(sys_arc_gettls)



__syscall_end:

        .rept   NR_syscalls - (__syscall_end - __syscall_start) / 4
                .long   SYMBOL_NAME(sys_ni_syscall)
        .endr
ARC_EXIT sys_call_table


#ifdef CONFIG_ARC_STACK_UNWIND
; Workaround for bug 94179 (STAR ):
; Despite -fasynchronous-unwind-tables, linker is not making dwarf2 unwinder
; section (.debug_frame) as loadable. So we force it here.
; This also fixes STAR 9000487933 where the prev-workaround (objcopy --setflag)
; would not work after a clean build due to kernel build system dependencies.
.section .debug_frame, "wa",@progbits
#endif


/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Dec 2007
 *  -Check if we are running on Simulator or on real hardware
 *      to skip certain things during boot on simulator
 */

#include <asm/asm-offsets.h>
#include <asm/entry.h>
#include <linux/linkage.h>
#include <asm/arcregs.h>
#include <common/topaz_platform.h>

    .cpu A7
    .data
    .globl atag_head
    .align 4
atag_head:
        .long 0

    .section .init.text, "ax",@progbits
    .type stext, @function
    .globl stext
stext:

    ;adding support for atag parsing
    ;If kernel parameters are passed by u-boot, then
    ; r0 = 0
    ; r1 = magic number (board identity)
    ; r2 = address of ATAG parameter list

    cmp r0, 0x0
    jne no_params
    lr  r3, [identity]
    cmp r1, r3
    jne no_params
    st  r2, [atag_head]

no_params:
#ifdef CONFIG_SMP
    lr  r0,[identity]

    ; processor ID [ 3 2 <1> 0 ]
    ; For ARC700 it is 0. For ARC800 it is core-ID (0, 1, ...)
    asr r1, r0, 8
    and.f r1, r1, 0x000000ff

    ; Whether it be single core in ARC700 or Boot CPU of ARC800,
    ; both have processor-ID = 0.
    ; In either case, jump to master_proceed.
    ; Code same for SMP and Non-SMP

    jz master_proceed

    ; Secondary Halt self. Later master will set PC and clear halt bit
    flag 1

master_proceed:
#endif

#ifdef CONFIG_ARC_UBOOT_CMDLINE

; u-boot passes the command line variable at the beginning of BSS
; copy the first few bytes out of the BSS into the command_line variable.

    mov r5, [SYMBOL_NAME(command_line)]
    mov r6, __bss_start

__copy_commandline:
    ld.ab   r0, [r6,4]
    cmp     r0, 0
    beq     __clear_bss
    st.ab    r0, [r5,4]
    j       __copy_commandline
#endif

__clear_bss:

; Clear BSS before updating any globals
; If u-boot is configured, then only clean the beginning.

    mov r5, __bss_start
#ifdef CONFIG_ARC_UBOOT_CMDLINE
    mov r6, __bss_start + 256
#else
    mov r6, __bss_stop
#endif

__clear_bss_loop:

    st.ab   0, [r5,4]
    brlt    r5, r6, __clear_bss_loop

    ; chip-id [ <3 2> 1 0 ]
    ; useful for determining SIM vs Real H/w (SIM = 0xffff)
    lr  r0, [identity]
    lsr r3, r0, 16
    sub r3, 0xffff, r3
    st  r3, [SYMBOL_NAME(running_on_hw)] ; 0 for sim, non-zero for real hw

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    ; setup init_task as "current"
    mov r25, [SYMBOL_NAME(init_task)]
    st r25, [SYMBOL_NAME(_current_task)]
#endif


__clear_sram_bss:
    mov r5, __sram_bss_start
    mov r6, __sram_bss_end

__clear_sram_bss_loop:
    st.ab 0, [r5,4]
    brlt  r5, r6, __clear_sram_bss_loop


#include "head_fixup.S"


    ; setup stack (fp, sp)
    mov fp, 0
    mov sp,init_thread_union + THREAD_SIZE

    /* Jump to start of kernel initialization routine */
    jal start_kernel

#ifdef CONFIG_SMP
;----------------------------------------------------------
;     Routine called by Master to kick-start secondary
;       TODO-vineetg: secondary_boot_data is one entry wide,
;       so only 1 secondary CPU supported
;----------------------------------------------------------

    .section .init.text, "ax",@progbits
    .type wakeup_secondary, @function
    .globl wakeup_secondary

wakeup_secondary:

    ; (2)------- Set the PC of 2nd CPU  --------

    mov r1, SYMBOL_NAME(secondary_boot_data)    ; primary sets boot params here
    mov r2, ARC_XTL_CMD_WRITE_PC
    ld r3, [r1, SECONDARY_BOOT_CPU_ID]          ; Fetch cpu-id
    asl r3, r3, ARC_XTL_REG_SYNTAX_CMD_CPU_ID   ; shift left as per reg syntax
    or r2, r2, r3                               ; club cpu-id and write-pc cmd
    ld r0, [r1, SECONDARY_BOOT_C_ENTRY]         ; Fetch entry point

    ; Need to do these one-after-other
    sr r0, [ARC_AUX_XTL_REG_PARAM]              ; Params all set for WRITE_PC
    sr r2, [ARC_AUX_XTL_REG_CMD]                ; Trigger WRITE_PC

    ; (3)------- take the cpu out of Halt -----------

    mov r2, ARC_XTL_CMD_CLEAR_HALT
    or r2, r2, r3                   ; club cpu-id and clear-hlt cmd.
                                    ;   r3 already has cpu-id nicely shifted
    sr r2, [ARC_AUX_XTL_REG_CMD]    ; trigger CLEAR_HTL

    j [blink]   ; function epilogue, return to caller

;----------------------------------------------------------------
;     First lines of code run by secondary before jumping to 'C'
;----------------------------------------------------------------
    .section .init.text, "ax",@progbits
    .type first_lines_of_secondary, @function
    .globl first_lines_of_secondary

first_lines_of_secondary:

    ; extract processor id
    lr  r1,[identity]
    asr r1, r1, 8
    and r1, r1, 0x000000ff

    ; setup stack
    mov r2, SYMBOL_NAME(secondary_boot_data)
    ld r3, [r2, SECONDARY_BOOT_STACK]
    mov sp, r3

    mov fp, 0
    jal start_kernel_secondary

#endif

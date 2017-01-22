/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <asm/system.h>

#ifdef HW_BUG_101581

/* Becuase of a hardware bug, we can't do simple lr/sr combo to read/write
 * the DPFP aux regs. Apparently the bug doesn't prevent usage of DEXCLx isns
 * to get/set the regs.
 *
 * Store to 64bit dpfp1 reg:
 *   dexcl1 0, r1, r0  ; where r1:r0 is thw 64 bit val
 *
 * Read from dpfp1 into pair of core regs (w/o clobbering dpfp1)
 *   mov_s r3, 0
 *   daddh11 r1, r3, r3   ; get "hi" into r1 (dpfp1 unchanged)
 *   dexcl1 r0, r1, r3    ; get "low" into r0 (dpfp1 low clobbered)
 *   dexcl1 0, r1, r0     ; restore dpfp1 to orig value
 *
 * However we can tweak the read, so that read-out of old task's value
 * and settign with new task's value happen in one shot, hence
 * all work done before context switch, and nothing after context-switch
 */

void fpu_save_restore(struct task_struct *prev,
        struct task_struct *next)
{
    unsigned int *saveto = &prev->thread.fpu.aux_dpfp[0].l;
    unsigned int *readfrom = &next->thread.fpu.aux_dpfp[0].l;

    const unsigned int zero = 0;

    __asm__ __volatile__ (
        "daddh11  %0, %2, %2  \n"
        "dexcl1   %1, %3, %4  \n"
        :"=&r"(*(saveto+1)),              // early clobber must here
         "=&r"(*(saveto))
        :"r"(zero),
         "r"(*(readfrom+1)),"r"(*(readfrom))
    );

    __asm__ __volatile__ (
        "daddh22  %0, %2, %2  \n"
        "dexcl2   %1, %3, %4  \n"
        :"=&r"(*(saveto+3)),              // early clobber must here
         "=&r"(*(saveto+2))
        :"r"(zero),
         "r"(*(readfrom+3)),"r"(*(readfrom+2))
    );

}

#define ARC_FPU_PREV(p,n)   fpu_save_restore(p,n)
#define ARC_FPU_NEXT(t)

#else  /* !HW_BUG_101581 */

void fpu_save(struct task_struct *tsk)
{
    struct arc_fpu *fpu = &tsk->thread.fpu;
    fpu->aux_dpfp[0].l = read_new_aux_reg(ARC_AUX_DPFP_1L);
    fpu->aux_dpfp[0].h = read_new_aux_reg(ARC_AUX_DPFP_1H);
    fpu->aux_dpfp[1].l = read_new_aux_reg(ARC_AUX_DPFP_2L);
    fpu->aux_dpfp[1].h = read_new_aux_reg(ARC_AUX_DPFP_2H);
}

void fpu_restore(struct task_struct *tsk)
{
    struct arc_fpu *fpu = &tsk->thread.fpu;
    write_new_aux_reg(ARC_AUX_DPFP_1L, fpu->aux_dpfp[0].l);
    write_new_aux_reg(ARC_AUX_DPFP_1H, fpu->aux_dpfp[0].h);
    write_new_aux_reg(ARC_AUX_DPFP_2L, fpu->aux_dpfp[1].l);
    write_new_aux_reg(ARC_AUX_DPFP_2H, fpu->aux_dpfp[1].h);
}

#endif  /* !HW_BUG_101581 */

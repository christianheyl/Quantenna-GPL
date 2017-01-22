/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/kprobes.h>
#include <linux/slab.h>

enum
{
    op_BC = 0, op_BLC = 1, op_LD  = 2, op_ST = 3, op_MAJOR_4  = 4,
    op_MAJOR_5 = 5, op_LD_ADD = 12, op_ADD_SUB_SHIFT  = 13,
    op_ADD_MOV_CMP = 14, op_S = 15, op_LD_S = 16, op_LDB_S = 17,
    op_LDW_S = 18, op_LDWX_S  = 19, op_ST_S = 20, op_STB_S = 21,
    op_STW_S = 22, op_Su5     = 23, op_SP   = 24, op_GP    = 25, op_Pcl = 26,
    op_MOV_S = 27, op_ADD_CMP = 28, op_BR_S = 29, op_B_S   = 30, op_BL_S = 31
};

enum Flow
{
    noflow,
    direct_jump,
    direct_call,
    indirect_jump,
    indirect_call,
    invalid_instr
};

#define IS_BIT(word,n)     ((word) & (1<<n))
#define BITS(word,s,e)  (((word) << (31-e)) >> (s+(31-e)))

#define OPCODE(word)    (BITS((word),27,31))
#define FIELDA(word)    (BITS((word),0,5))
#define FIELDC(word)    (BITS((word),6,11))

#define FIELDS(word) ((BITS(word,0,5) << 6) | (BITS(word,6,11)))

#define STATUS32_L  0x00000100

struct arcDisState {
    unsigned long words[2];
    int instr_len;
    int opcode;
    int is_branch;
    int target;
    int reg_jump;
    int delay_slot;
    enum Flow flow;
};


static int __kprobes sign_extend(int value, int bits)
{
    if (IS_BIT(value, (bits-1)))
        value |= (0xffffffff << bits);

    return value;
}

static int __kprobes is_short_instr(unsigned long addr)
{
    u16 word = *((u16 *)addr);
    int opcode = (word >> 11) & 0x1F;

    if (opcode >= 0x0B)
        return 1;

    return 0;
}


static void __kprobes dsmArcInstr(unsigned long addr, struct arcDisState *state)
{
    int fieldA = 0, fieldB = 0;
    int fieldC = 0, fieldCisReg = 0;
    u16 word1 = 0, word0 = 0;
    int subopcode, is_linked, op_format;

    // Read the first instruction;
    // This fetches the upper part of the 32 bit instruction
    // in both the cases of Little Endian or Big Endian configurations.
    state->words[0] = *((u16 *)addr);
    word1 = *((u16 *)addr);

    // Get the Opcode
    state->opcode = (word1 >> 11) & 0x1F;

    // Check if the instruction is 32bit or 16 bit instruction
    if (state->opcode < 0x0B)
    {
        state->instr_len = 4;
        word0 = *((u16 *)(addr+2));
    }
    else
        state->instr_len = 2;

    state->words[0] = (word1 << 16) | word0;

    // Read the second word incase of limm
    word1 = *((u16 *)(addr + state->instr_len));
    word0 = *((u16 *)(addr + state->instr_len + 2));
    state->words[1] = (word1 << 16) | word0;

    state->is_branch = 0;

    switch (state->opcode)
    {
        case op_BC:
            state->is_branch = 1;

            /* unconditional branch s25, conditional branch s21*/
            if (IS_BIT(state->words[0], 16))
                fieldA = (BITS(state->words[0], 0, 3)) << 10;

            fieldA |= BITS(state->words[0], 6, 15);
            fieldA = fieldA << 10;
            fieldA |= BITS(state->words[0], 17, 26);
            /* target address is 16-bit aligned */
            fieldA = fieldA << 1;

            if (IS_BIT(state->words[0], 16))
                fieldA = sign_extend (fieldA, 25);
            else
                fieldA = sign_extend (fieldA, 21);

            fieldA += (addr & ~0x3);

            /* Check if the delay slot mode bit is set */
            state->delay_slot = IS_BIT(state->words[0],5);

            state->target = fieldA;
            state->flow = direct_jump;
            break;

        case op_BLC:
            /* Branch and Link*/
            if(IS_BIT(state->words[0],16))
            {
                /* unconditional branch s25, conditional branch s21*/
                if (IS_BIT(state->words[0], 17))
                    fieldA = (BITS(state->words[0], 0, 3)) << 10;

                fieldA |= BITS(state->words[0], 6, 15);
                fieldA = fieldA << 10;
                fieldA |= BITS(state->words[0], 18, 26);
                /* target address is 32-bit aligned */
                fieldA = fieldA << 2;

                if (IS_BIT(state->words[0], 17))
                    fieldA = sign_extend (fieldA, 25);
                else
                    fieldA = sign_extend (fieldA, 21);

                fieldA += (addr & ~0x3);

                /* Check if the delay slot mode bit is set */
                state->delay_slot = IS_BIT(state->words[0],5);
                state->flow = direct_call;
            }
            else /*Branch On Compare */
            {
                fieldA = (((BITS(state->words[0],15,15) << 7) | (BITS(state->words[0],17,23))) << 1);
                fieldA = sign_extend (fieldA, 9);
                fieldA += (addr & ~0x3);

                /* Check if the delay slot mode bit is set */
                state->delay_slot = IS_BIT(state->words[0],5);
                state->flow = direct_jump;
            }

            state->target = fieldA;
            state->is_branch = 1;
            break;

        case op_MAJOR_4:
            subopcode = BITS (state->words[0], 16, 21);
            switch (subopcode)
            {
                case 32:    //"Jcc"
                case 33:    //"Jcc.D"
                case 34:    //"JLcc"
                case 35:    //"JLcc.D"
                    is_linked = 0;

                    if (subopcode == 33 || subopcode == 35)
                        state->delay_slot = 1;

                    if (subopcode == 34 || subopcode == 35)
                        is_linked = 1;

                    fieldCisReg = 0;
                    op_format = BITS(state->words[0],22,23);
                    if(op_format == 0 || ((op_format == 3) && (!IS_BIT(state->words[0],5))))
                    {
                        fieldC = FIELDC(state->words[0]);
                        if (fieldC == 62)
                        {
                            fieldC = state->words[1];
                            state->instr_len +=4;
                        }
                        else
                            fieldCisReg = 1;
                    }
                    else if (op_format == 1 || ((op_format == 3) && (IS_BIT(state->words[0],5))))
                    {
                        fieldC = FIELDC(state->words[0]);
                    }
                    else  // op_format == 2
                    {
                        fieldC = FIELDS(state->words[0]);
                        fieldC = sign_extend(fieldC,11);
                    }

                    if (!fieldCisReg)
                    {
                        state->target = fieldC;
                        state->flow = is_linked ? direct_call : direct_jump;
                    }
                    else
                    {
                        state->reg_jump = fieldC;
                        state->flow = is_linked ? indirect_call : indirect_jump;
                    }
                    state->is_branch = 1;
                    break;

                case 40:
                    //"LPcc"
                    if (BITS(state->words[0],22,23) == 3)
                    {
                        // Conditional LPcc u7
                        fieldC = FIELDC(state->words[0]);

                        fieldC = fieldC << 1;
                        fieldC += (addr & ~0x03);
                        state->is_branch = 1;
                        state->flow = direct_jump;
                        state->target = fieldC;
                    }
                    /* For Unconditional lp, next pc is the fall through which
                     * is updated
                     */
                    break;

                default:
                    // Not a Jump or Loop instruction
                    break;
            }
            break;


        /* 16 Bit Branching Instructions */
        case op_S:
            subopcode = BITS(state->words[0],5,7);
            switch(subopcode)
            {
                case 0:  // "j_s"
                case 1:  // "j_s.d"
                case 2:  // "jl_s"
                case 3:  // "jl_s.d"
                    fieldB = (BITS(state->words[0],8,10));
                    // For 16-bit instructions, reduced register range
                    // from r0-r3 and r12-r15, get the actual register below.
                    if (fieldB > 3)
                        fieldB += 8;

                    state->reg_jump = fieldB;

                    if (subopcode == 1 || subopcode == 3)
                        state->delay_slot = 1;

                    if (subopcode == 2 || subopcode == 3)
                        state->flow = direct_call;
                    else
                        state->flow = indirect_jump;

                    break;

                case 7:
                    switch(BITS(state->words[0], 8, 10))
                    {
                        // Zero Operand Jump instruction
                        case 4: // "jeq_s [blink]"
                        case 5: // "jne_s [blink]"
                        case 6: // "j_s [blink]"
                        case 7: // "j_s.d [blink]"
                            if (subopcode == 7)
                                state->delay_slot = 1;

                            state->flow = indirect_jump;
                            state->reg_jump = 31;
                            break;
                        default:
                            break;
                     }
                     break;

                    default:
                    break;
            }
            break;

        // 16-bit Branch on Compare Register with Zero
        case op_BR_S:
                fieldA = (BITS(state->words[0],0,6)) << 1;
                fieldA = sign_extend(fieldA,7);
                fieldA += (addr & ~0x03);
                state->target = fieldA;
                state->flow = direct_jump;
                state->is_branch = 1;
                break;

        // 16-bit Branch Conditionally
        case op_B_S:
                subopcode = BITS(state->words[0],9,10);
                if (subopcode == 3)
                {
                    fieldA = BITS(state->words[0],0,5);
                    fieldA = sign_extend(fieldA,5);
                }
                else
                {
                    fieldA = BITS(state->words[0],0,8);
                    fieldA = sign_extend(fieldA,8);
                }
                fieldA = fieldA << 1;

                fieldA += (addr & ~0x03);
                state->target = fieldA;
                state->flow = direct_jump;
                state->is_branch = 1;
                break;

        //  16-bit  Branch and link unconditionally
        case op_BL_S:
                fieldA = (BITS(state->words[0],0,10)) << 2;
                fieldA = sign_extend(fieldA,12);
                fieldA += (addr & ~0x03);
                state->target = fieldA;
                state->flow = direct_call;
                state->is_branch = 1;
                break;

        default:
            break;
    }
}

unsigned long __kprobes get_reg(struct pt_regs *regs, unsigned long reg_no)
{
    switch(reg_no)
    {
        case 0: return regs->r0;
        case 1: return regs->r1;
        case 2: return regs->r2;
        case 3: return regs->r3;
        case 4: return regs->r4;
        case 5: return regs->r5;
        case 6: return regs->r6;
        case 7: return regs->r7;
        case 8: return regs->r8;
        case 9: return regs->r9;
        case 10:return regs->r10;
        case 11:return regs->r11;
        case 12:return regs->r12;
        case 26:return regs->r26;
        case 27:return regs->fp;
        case 28:return regs->sp;
        case 29:return regs->ret;
        case 30:return regs->ret;
        case 31:return regs->blink;
        default:
            printk("register %ld not part of pt regs\n", reg_no);
            return 0;
    }
}

static int __kprobes next_pc(unsigned long pc, struct pt_regs *regs,
                                unsigned long *fall_thru, unsigned long *target)
{
    struct arcDisState instr;

    memset(&instr, 0, sizeof(struct arcDisState));
    dsmArcInstr(pc, &instr);

    *fall_thru = pc + instr.instr_len;

    /* Instruction with possible two targets branch, jump and loop */
    if (instr.is_branch)
    {
        if (instr.flow == direct_jump || instr.flow == direct_call)
        {
            *target = instr.target;
        }
        else
        {
            // Get it from the register
            *target = get_reg(regs, instr.reg_jump);
        }
    }

    /* For the instructions with delay slots, the fall thru is the instruction
       following the instruction in delay slot.
    */
    if (instr.delay_slot)
    {
        struct arcDisState instr_d;

        dsmArcInstr(*fall_thru, &instr_d);

        *fall_thru += instr_d.instr_len;
        /* We cannot step into delay slot. If we put a trap in the delay slot,
         * we cannot identify that it is from delay slot and cannot set DE bit
         * when the actual instruction is painted and executed.
         */
    }

    /* Zero Overhead Loop - end of the loop */
    if (!(regs->status32 & STATUS32_L) && (*fall_thru == regs->lp_end)
                && (regs->lp_count > 1))
    {
        *fall_thru = regs->lp_start;
    }

    return instr.is_branch;
}



#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kdebug.h>
#include <asm/cacheflush.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

extern int fixup_exception(struct pt_regs *regs);

#define MIN_STACK_SIZE(addr)            \
    min((unsigned long)MAX_STACK_SIZE,  \
        (unsigned long)current_thread_info() + THREAD_SIZE - (addr))

DEFINE_PER_CPU(struct kprobe *, current_kprobe) = NULL;
DEFINE_PER_CPU(struct kprobe_ctlblk, kprobe_ctlblk);

void kretprobe_trampoline(void);

int __kprobes arch_prepare_kprobe(struct kprobe *p)
{

    // Attempt to probe at unaligned address
    if((unsigned long)p->addr & 0x01)
        return -EINVAL;

    /* Address should not be
     *  in exception handling code
     */

    p->ainsn.is_short = is_short_instr((unsigned long)p->addr);
    p->opcode = *p->addr;

    return 0;
}

void __kprobes arch_arm_kprobe(struct kprobe *p)
{
    *p->addr = UNIMP_S_INSTRUCTION;

    // Flush the icache
    flush_icache_range((unsigned long)p->addr,
            (unsigned long)p->addr + sizeof(kprobe_opcode_t));
}

void __kprobes arch_disarm_kprobe(struct kprobe *p)
{
    *p->addr = p->opcode;

    // Flush the icache
    flush_icache_range((unsigned long)p->addr,
            (unsigned long)p->addr + sizeof(kprobe_opcode_t));
}

void __kprobes arch_remove_kprobe(struct kprobe *p)
{
    arch_disarm_kprobe(p);

    // Can we remove the kprobe in the middle of kprobe handling?
    if(p->ainsn.t1_addr)
    {
        *(p->ainsn.t1_addr) = p->ainsn.t1_opcode;

        flush_icache_range((unsigned long)p->ainsn.t1_addr,
            (unsigned long)p->ainsn.t1_addr + sizeof(kprobe_opcode_t));

        p->ainsn.t1_addr = 0;
    }

    if(p->ainsn.t2_addr)
    {
        *(p->ainsn.t2_addr) = p->ainsn.t2_opcode;

        flush_icache_range((unsigned long)p->ainsn.t2_addr,
           (unsigned long)p->ainsn.t2_addr + sizeof(kprobe_opcode_t));

        p->ainsn.t2_addr = 0;
    }
}

static void __kprobes save_previous_kprobe(struct kprobe_ctlblk *kcb)
{
    kcb->prev_kprobe.kp = kprobe_running();
    kcb->prev_kprobe.status = kcb->kprobe_status;
}

static void __kprobes restore_previous_kprobe(struct kprobe_ctlblk *kcb)
{
    __get_cpu_var(current_kprobe) = kcb->prev_kprobe.kp;
    kcb->kprobe_status = kcb->prev_kprobe.status;
}

static inline void __kprobes set_current_kprobe(struct kprobe *p)
{
    __get_cpu_var(current_kprobe) = p;
}

static void __kprobes resume_execution(struct kprobe *p, unsigned long addr,
                                                            struct pt_regs *regs)
{
    /* Remove the trap instructions inserted for single step and restore the
     * original instructions
     */
    if(p->ainsn.t1_addr)
    {
        *(p->ainsn.t1_addr) = p->ainsn.t1_opcode;

        flush_icache_range((unsigned long)p->ainsn.t1_addr,
           (unsigned long)p->ainsn.t1_addr + sizeof(kprobe_opcode_t));

        p->ainsn.t1_addr = 0;
    }

    if(p->ainsn.t2_addr)
    {
        *(p->ainsn.t2_addr) = p->ainsn.t2_opcode;

        flush_icache_range((unsigned long)p->ainsn.t2_addr,
           (unsigned long)p->ainsn.t2_addr + sizeof(kprobe_opcode_t));

        p->ainsn.t2_addr = 0;
    }

    return;
}

static void __kprobes setup_singlestep(struct kprobe *p, struct pt_regs *regs)
{
    unsigned long fall_thru;
    unsigned long target=0;
    int is_branch;
    unsigned long bta;

    /* Copy the opcode back to the kprobe location and execute the instruction
     * Because of this we will not be able to get into the same kprobe until
     * this kprobe is done (eithe
     */
    *(p->addr) = p->opcode;

    // Flush the icache and dcache
    flush_icache_range((unsigned long)p->addr,
           (unsigned long)p->addr + sizeof(kprobe_opcode_t));


    /* Now we insert the trap at the next location after this instruction to
     * single step. If it is a branch we insert the trap at possible branch
     * targets
     */

    bta = regs->bta;

    if(regs->status32 & 0x40)
    {
        /* We are in a delay slot with the branch taken */

        fall_thru = bta & ~0x01;

        if (!p->ainsn.is_short)
        {
            if(bta & 0x01)
                regs->blink += 2;
            else
            {
                /* Branch not taken */
                fall_thru += 2;

                /* next pc is taken from bta after executing the delay
                 * slot instruction which was
                 */
                regs->bta += 2;
            }
        }

        is_branch = 0;
    }
    else
        is_branch = next_pc((unsigned long)p->addr, regs, &fall_thru, &target);

    p->ainsn.t1_addr = (kprobe_opcode_t *)fall_thru;
    p->ainsn.t1_opcode = *(p->ainsn.t1_addr);
    *(p->ainsn.t1_addr) = TRAP_S_2_INSTRUCTION;

    flush_icache_range((unsigned long)p->ainsn.t1_addr,
           (unsigned long)p->ainsn.t1_addr + sizeof(kprobe_opcode_t));

    if (is_branch)
    {
        p->ainsn.t2_addr = (kprobe_opcode_t *)target;
        p->ainsn.t2_opcode = *(p->ainsn.t2_addr);
        *(p->ainsn.t2_addr) = TRAP_S_2_INSTRUCTION;

        flush_icache_range((unsigned long)p->ainsn.t2_addr,
            (unsigned long)p->ainsn.t2_addr + sizeof(kprobe_opcode_t));
    }
}

int __kprobes kprobe_handler(unsigned long addr, struct pt_regs *regs)
{
    struct kprobe *p;
    struct kprobe_ctlblk *kcb;

    preempt_disable();

    kcb = get_kprobe_ctlblk();
    p = get_kprobe((unsigned long*)addr);

    if (p) {
        if (kprobe_running()) {
            /* We have rentered the kprobe_handler, since another kprobe was
             * hit while within the handler, we save the original kprobes and
             * single step on the instruction of the new probe without calling
             * any user handlers to avoid recursive kprobes.
             */
            save_previous_kprobe(kcb);
            set_current_kprobe(p);
            kprobes_inc_nmissed_count(p);
            setup_singlestep(p,regs);
            kcb->kprobe_status = KPROBE_REENTER;
            return 1;
        }

        set_current_kprobe(p);
        kcb->kprobe_status = KPROBE_HIT_ACTIVE;

        /* If we have no pre-handler or it returned 0, we continue with normal
         * processing. If we have a pre-handler and it returned non-zero -
         * which is expected from setjmp_pre_handler for jprobe, we return
         * without single stepping and leave that to the break-handler which is
         * invoked by a kprobe from jprobe_return
         */
        if(!p->pre_handler || !p->pre_handler(p, regs))
        {
            setup_singlestep(p,regs);
            kcb->kprobe_status = KPROBE_HIT_SS;
        }

        return 1;
    }
    else if (kprobe_running()) {
        p = __get_cpu_var(current_kprobe);
        if (p->break_handler && p->break_handler(p,regs)) {
            setup_singlestep(p,regs);
            kcb->kprobe_status = KPROBE_HIT_SS;
            return 1;
        }
    }

    // no_kprobe:
    preempt_enable_no_resched();
    return 0;
}

static int __kprobes post_kprobe_handler(unsigned long addr, struct pt_regs *regs)
{
    struct kprobe *cur = kprobe_running();
    struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

    if(!cur)
        return 0;

    resume_execution(cur, addr, regs);

    // Rearm the kprobe
    arch_arm_kprobe(cur);

    /* When we return from trap instruction we go to the next instruction
     * We restored the actual instruction in resume_exectuiont and we to
     * return to the same address and execute it
     */
    regs->ret = addr;

    if ((kcb->kprobe_status != KPROBE_REENTER) && cur->post_handler) {
        kcb->kprobe_status = KPROBE_HIT_SSDONE;
        cur->post_handler(cur,regs,0);
    }

    if (kcb->kprobe_status == KPROBE_REENTER) {
        restore_previous_kprobe(kcb);
        goto out;
    }

    reset_current_kprobe();

out:
    preempt_enable_no_resched();
    return 1;
}

/* Fault can be for the instruction being single stepped or for the
 * pre/post handlers in the module.
 * This is applicable for applications like user probes, where we have the
 * probe in user space and the handlers in the kernel
 */

int __kprobes kprobe_fault_handler(struct pt_regs *regs, unsigned long trapnr)
{
    struct kprobe *cur = kprobe_running();
    struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

    switch(kcb->kprobe_status) {
        case KPROBE_HIT_SS:
        case KPROBE_REENTER:
            /* We are here because the instruction being single stepped caused
             * the fault. We reset the current kprobe and allow the exception
             * handler as if it is regular exception.
             * In our case it doesn't matter because the system will be halted
             */
            resume_execution(cur, (unsigned long)cur->addr, regs);

            if (kcb->kprobe_status == KPROBE_REENTER)
                restore_previous_kprobe(kcb);
            else
                reset_current_kprobe();

            preempt_enable_no_resched();
            break;

        case KPROBE_HIT_ACTIVE:
        case KPROBE_HIT_SSDONE:
            /* We are here because the instructions in the pre/post handler
             * caused the fault.
             */

		    /* We increment the nmissed count for accounting,
		     * we can also use npre/npostfault count for accouting
		     * these specific fault cases.
		     */
		    kprobes_inc_nmissed_count(cur);

		    /*
		     * We come here because instructions in the pre/post
		     * handler caused the page_fault, this could happen
		     * if handler tries to access user space by
		     * copy_from_user(), get_user() etc. Let the
		     * user-specified handler try to fix it first.
		     */
            if (cur->fault_handler && cur->fault_handler(cur, regs, trapnr))
                return 1;

		    /* In case the user-specified fault handler returned
		     * zero, try to fix up.
		     */
            if (fixup_exception(regs))
                return 1;

		    /*
		     * fixup_exception() could not handle it,
		     * Let do_page_fault() fix it.
		     */
		    break;

        default:
            break;
    }
    return 0;
}

int __kprobes kprobe_exceptions_notify(struct notifier_block *self,
                unsigned long val, void *data)
{
    struct die_args *args = data;
    unsigned long addr = args->err;
    int ret =  NOTIFY_DONE;

    switch(val)
    {
        case DIE_IERR:
            if (kprobe_handler(addr, args->regs))
                return NOTIFY_STOP;
            break;

        case DIE_TRAP:
            if (post_kprobe_handler(addr, args->regs))
                return NOTIFY_STOP;
            break;

        default:
            break;
    }

    return ret;
}

int __kprobes setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct jprobe *jp = container_of(p, struct jprobe, kp);
    struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
    unsigned long sp_addr=regs->sp;

    kcb->jprobe_saved_regs = *regs;
    memcpy(kcb->jprobes_stack, (void *)sp_addr, MIN_STACK_SIZE(sp_addr));
    regs->ret = (unsigned long)(jp->entry);

    return 1;
}

void __kprobes jprobe_return(void)
{
    __asm__ __volatile__("unimp_s");
    return;
}

int __kprobes longjmp_break_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
    unsigned long sp_addr;

    *regs = kcb->jprobe_saved_regs;
    sp_addr = regs->sp;
    memcpy((void *)sp_addr, kcb->jprobes_stack, MIN_STACK_SIZE(sp_addr));
    preempt_enable_no_resched();

    return 1;
}


static void __used kretprobe_trampoline_holder(void)
{
    __asm__ __volatile__ (".global kretprobe_trampoline\n"
                          "kretprobe_trampoline:\n"
                          "nop\n");
}

void __kprobes arch_prepare_kretprobe(struct kretprobe_instance *ri,
                                struct pt_regs *regs)
{

    ri->ret_addr = (kprobe_opcode_t *)regs->blink;

    /* Replace the return addr with trampoline addr */
    regs->blink = (unsigned long)&kretprobe_trampoline;
}

static int __kprobes trampoline_probe_handler(struct kprobe *p,
                                struct pt_regs *regs)
{
	struct kretprobe_instance *ri = NULL;
	struct hlist_head *head, empty_rp;
	struct hlist_node *node, *tmp;
	unsigned long flags, orig_ret_address = 0;
	unsigned long trampoline_address = (unsigned long)&kretprobe_trampoline;

	INIT_HLIST_HEAD(&empty_rp);
	kretprobe_hash_lock(current, &head, &flags);

	/*
	 * It is possible to have multiple instances associated with a given
	 * task either because an multiple functions in the call path
	 * have a return probe installed on them, and/or more than one return
	 * return probe was registered for a target function.
	 *
	 * We can handle this because:
	 *     - instances are always inserted at the head of the list
	 *     - when multiple return probes are registered for the same
	 *	 function, the first instance's ret_addr will point to the
	 *	 real return address, and all the rest will point to
	 *	 kretprobe_trampoline
	 */
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			/* another task is sharing our hash bucket */
			continue;

		if (ri->rp && ri->rp->handler)
			ri->rp->handler(ri, regs);

		orig_ret_address = (unsigned long)ri->ret_addr;
		recycle_rp_inst(ri, &empty_rp);

		if (orig_ret_address != trampoline_address) {
			/*
			 * This is the real return address. Any other
			 * instances associated with this task are for
			 * other calls deeper on the call stack
			 */
			break;
		}
	}

	kretprobe_assert(ri, orig_ret_address, trampoline_address);
	regs->ret = orig_ret_address;

	reset_current_kprobe();
	kretprobe_hash_unlock(current, &flags);
	preempt_enable_no_resched();

	hlist_for_each_entry_safe(ri, node, tmp, &empty_rp, hlist) {
		hlist_del(&ri->hlist);
		kfree(ri);
	}


    /* By returning a non zero value, we are telling the kprobe handler that
     * we don't want the post_handler to run
     */
    return 1;
}

static struct kprobe trampoline_p = {
    .addr = (kprobe_opcode_t *) &kretprobe_trampoline,
    .pre_handler = trampoline_probe_handler
};

int __init arch_init_kprobes()
{
    // Registering the trampoline code for the kret probe
    return register_kprobe(&trampoline_p);
}

int __kprobes arch_trampoline_kprobe(struct kprobe *p)
{
    if (p->addr == (kprobe_opcode_t *)&kretprobe_trampoline)
        return 1;

    return 0;
}


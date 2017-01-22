/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  vineetg Jan 2009
 *      -Converted strcpy to strncpy
 *
 *  vineetg: Feb 2008
 *
 *  Event capturing Framework
 *
 *      -Captures the event-id and other iformation in circular log buffer
 *      -Event can be Interrupts, Traps, Sys-Calls, or arbitrary function call
 *
 *      -USAGE:
 *          Events are defined in API file, include/asm-arc/event-log.h
 *          To log the event, "C" code calls API
 *              take_snap(event-id, event-specific-info)
 *          To log the event, ASM caller calls a "asm" macho
 *              TAKE_SNAP_ASM reg-x, reg-y, event-id
 *          To stop the capture and sort the log buffer,
 *              sort_snaps(halt-after-sort)
 *
 *      -The reason for 2 APIs is that in low level handlers
 *          which we are interested in capturing, often dont have
 *          stack switched, thus a "C" cpi wont work. Also there
 *          is a very strict requirment of which registers are usable
 *          hence the 2 regs
 *
 *      -Done primarily to chase the Random Segmentation Faults
 *          when we switched from gcc3.4 to gcc4.2
 */

#include <linux/sort.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/current.h>
#include <asm/event-log.h>
#include <linux/module.h>
#include <linux/reboot.h>

#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE

/* current on ARC is a register variable "r25" setup on entry to kernel and
   restored back to user value on return.
   However if the event snap hostting return is called very late from
   ISR/EXception return code, r25 might already have been restored to user
   value, hence would no longer point to current task. This can cause weird
   de-referencing crashes. Safest option is to undef it and instead define
   it in terms of current_thread_info() which is derived from SP
*/
#undef current
#define current (current_thread_info()->task)

/* Log buffer which stores the event info
 *
 *  There is race condition when the counter goes 1 more than
 *  max-value (if intr sneaks in in the lgging routine. Since
 *  we dont want to do fancy intr-enable-disable etc,
 *  we keep 1 extar element in log buffer
 */
timeline_log_t  timeline_log[MAX_SNAPS+1];

/* counter in log bugger for next entry */
int timeline_ctr;

/* Used in the low level asm handler to free up a reg */
int tmp_save_reg, tmp_save_reg2;
int l2_ctr;

/* Event capture API */
void take_snap(int event, unsigned int arg1, unsigned int arg2)
{
    timeline_log[timeline_ctr].time = read_new_aux_reg(ARC_REG_TIMER1_CNT);
    timeline_log[timeline_ctr].task = current->pid;
    timeline_log[timeline_ctr].event = event;
    timeline_log[timeline_ctr].extra2 = read_new_aux_reg(0xa);  //status32

    timeline_log[timeline_ctr].cause = read_new_aux_reg(0x403);  //ecr
    timeline_log[timeline_ctr].fault_addr = read_new_aux_reg(0x404); // efa

    timeline_log[timeline_ctr].extra = arg1;
    timeline_log[timeline_ctr].sp = arg2;

    timeline_log[timeline_ctr].extra3 =
                    (unsigned int)__builtin_return_address(0);

    if ( timeline_ctr == (MAX_SNAPS - 1) ) timeline_ctr=0;
    else timeline_ctr++;

}

void take_snap2(int event)
{
    unsigned long x, flags=0, stat32;

    stat32 = read_new_aux_reg(0xa);  //status32

    /* In case this is for Level 1 ISR, disable further Interrupts
     * so that timeline_ctr is not clobbered
     */
    if (event == SNAP_INTR_IN )
        local_irq_save(flags);

    timeline_log[timeline_ctr].time = read_new_aux_reg(ARC_REG_TIMER1_CNT);
    timeline_log[timeline_ctr].task = current->pid;
    timeline_log[timeline_ctr].event = event;
    timeline_log[timeline_ctr].extra2 = stat32;

    timeline_log[timeline_ctr].sp = current_thread_info()->preempt_count;

    if (event == SNAP_INTR_IN2 ) {
        timeline_log[timeline_ctr].cause = read_new_aux_reg(0x40B); //icause2
        timeline_log[timeline_ctr].extra = read_new_aux_reg(0x0C); // statsu32_l2
        __asm__ __volatile__(
                "mov %0, ilink2   \r\n"
                :"=r" (x));
        timeline_log[timeline_ctr].fault_addr = x;
    }
    else if (event == SNAP_INTR_IN ) {
        timeline_log[timeline_ctr].cause = read_new_aux_reg(0x40A); //icause1
        timeline_log[timeline_ctr].extra = read_new_aux_reg(0x0B); // statsu32_l1
        __asm__ __volatile__(
                "mov %0, ilink1   \r\n"
                :"=r" (x));
        timeline_log[timeline_ctr].fault_addr = x;
    }

    if ( timeline_ctr == (MAX_SNAPS - 1) ) timeline_ctr=0;
    else timeline_ctr++;

    if (current_thread_info()->preempt_count == 0xFFFFFFFF)
        sort_snaps(1);

    if (event == SNAP_INTR_IN )
        local_irq_restore(flags);
}

/* CMP routine called by event sort
 * When comparing the click time entries of @a to @b:
 *  gt: returns 1
 *  lt:  -1
 *  eq: returns 0
 */
static int snap_cmp(const void *a, const void *b)
{
    timeline_log_t *click_a, *click_b;

    click_a = (timeline_log_t *)a;
    click_b = (timeline_log_t *)b;

    if ( click_a->time == click_b->time ) return 0;
    else if ( click_a->time < click_b->time ) return -1;

    return 1;
}

/* Event Sort API, so that counter Rollover is not visibel to user */
void sort_snaps(int halt_after_sort)
{
    int i;
    unsigned int flags, tmp;

    /* TODO SMP */
    local_irq_save(flags);

    take_snap(SNAP_SENTINEL, 0, 0);

    sort(timeline_log, MAX_SNAPS, sizeof(timeline_log_t), snap_cmp, NULL
                //click_swap
                );

    for ( i = 0; i < MAX_SNAPS; i++) {
        memset(timeline_log[i].nm, 0, 16);

        switch(timeline_log[i].event)
        {
        case SNAP_TLB_FLUSH_ALL:
                strcpy(timeline_log[i].nm, "TLB FLUSH ALL");
                break;
        case 85:
                strcpy(timeline_log[i].nm, "FORK");
                break;
        case 86:
                strcpy(timeline_log[i].nm, "EXEC");
                break;
        case 99:
                strcpy(timeline_log[i].nm, "Slow-TLB-Write");
                break;
        case SNAP_EXCP_IN:
            switch (timeline_log[i].cause>>16) {
            case 0x21:
                strcpy(timeline_log[i].nm, "I-TLB");
                break;
            case 0x22:
                strcpy(timeline_log[i].nm, "D-TLB");
                break;
            case 0x23:
                strcpy(timeline_log[i].nm, "PROT-V-TLB");
                break;
            default:
                strcpy(timeline_log[i].nm, "?#?");
                break;
            }
            break;
        case SNAP_EXCP_OUT_FAST:
            strcpy(timeline_log[i].nm, "TLB Refill");
            break;
        case SNAP_EXCP_OUT:
            strcpy(timeline_log[i].nm, "Excp-RET");
            break;
        case SNAP_TRAP_IN:
            strcpy(timeline_log[i].nm, "SyCall :");
            switch (timeline_log[i].cause) {
            case 1:
                strcat(timeline_log[i].nm, "Exit");
                break;
            case 2:
                strcat(timeline_log[i].nm, "fork");
                break;
            case 114:
                strcat(timeline_log[i].nm, "wait4");
                break;
            default:
                strcat(timeline_log[i].nm, "???");
            }
            break;
        case SNAP_TRAP_OUT:
            strcpy(timeline_log[i].nm, "SyCall-RET");
            break;
        case SNAP_PRE_CTXSW_2_U:
            strcpy(timeline_log[i].nm, "2-U-Ctx-sw");
            break;
        case SNAP_SENTINEL:
            memset(&timeline_log[i], 0, sizeof(timeline_log[i]));
            strcpy(timeline_log[i].nm, "----------");
            break;
        case SNAP_PRE_CTXSW_2_K:
            strcpy(timeline_log[i].nm, "2-K-Ctx-sw");
            break;
        case SNAP_INTR_OUT:
            strcpy(timeline_log[i].nm, "IRQ-OUT");
            break;
        case SNAP_INTR_OUT2:
            strcpy(timeline_log[i].nm, "IRQ(2)-OUT");
            break;
        case SNAP_INTR_IN:
            strcpy(timeline_log[i].nm, "IRQ-in");
            break;
        case SNAP_INTR_IN2:
            strcpy(timeline_log[i].nm, "IRQ(2)-in");
            break;
        case SNAP_DO_PF_EXIT:
            strcpy(timeline_log[i].nm, "PF-RET");
            break;
        case SNAP_PREEMPT_SCH_IRQ:
            strcpy(timeline_log[i].nm, "Prem-Sch IRQ");
            break;
        case SNAP_PREEMPT_SCH:
            strcpy(timeline_log[i].nm, "Prem-Sch");
            break;
        case SNAP_DO_PF_ENTER:
            tmp = timeline_log[i].cause >> 16;
            switch (tmp) {
            case 0x21:
                strcpy(timeline_log[i].nm, "PF-in:I-TLB");
                break;
            case 0x22:
                strcpy(timeline_log[i].nm, "PF-in:D-TLB");
                break;
            case 0x23:
                strcpy(timeline_log[i].nm, "PF-in:PROTV");
                break;
            default:
                strcpy(timeline_log[i].nm, "PF-in:???");
                break;
            }
            break;
        case SNAP_SIGRETURN:
            strcpy(timeline_log[i].nm, "sigreturn");
            break;
        case SNAP_BEFORE_SIG:
            strcpy(timeline_log[i].nm, "before sig");
            break;
        }

    }

#if 0
    printk("\n##### Last Few System Events ####\n");
    for ( i= MAX_SNAPS-10; i< MAX_SNAPS ;i++)
        printk("%s [%d], PID %d, ECR 0x%x, EFA 0x%x, Extra 0x%x SP 0x%x\n",
             timeline_log[i].nm, timeline_log[i].event, timeline_log[i].task,
             timeline_log[i].cause, timeline_log[i].fault_addr,
             timeline_log[i].extra, timeline_log[i].sp);
#endif

    if (halt_after_sort)
        __asm__("flag 1");
    else
        local_irq_restore(flags);

}

EXPORT_SYMBOL(take_snap);
EXPORT_SYMBOL(sort_snaps);

#endif

/*######################################################################
 *
 *
 *    Process fork/exec Profiling
 *
 *
 *#####################################################################*/


#ifdef CONFIG_DEBUG_ARC_PID

#define VINEET_MAX   1024

typedef struct {
    char type;
    int parentpid;
    int mypid;
    char nm[16];
    struct task_struct *task;
} my_pid_list_t;

static my_pid_list_t  my_pid_list[VINEET_MAX] =
{
    { .task = &init_task }
};

static atomic_t myctr;

void fork_exec_log(struct task_struct *p, char fork_or_exec)
{
        atomic_inc(&myctr);
        if ( myctr.counter == VINEET_MAX )
            myctr.counter= 64;

        memcpy( my_pid_list[myctr.counter].nm, current->comm, 16);
        my_pid_list[myctr.counter].mypid = p->tgid;
        my_pid_list[myctr.counter].task = p;
        my_pid_list[myctr.counter].type = fork_or_exec;

        if ( fork_or_exec == 'f')
            my_pid_list[myctr.counter].parentpid = current->pid;
        else
            my_pid_list[myctr.counter].parentpid = -1;

}

#endif

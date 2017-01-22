/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  vineetg: Dec 2009
 *      Reworked the numbering scheme into Event Classes for making it easier to
 *      do class specific things in the snapshot routines
 *
 *  vineetg: Feb 2008
 *      System Event Logging APIs
 */

#ifndef __ASM_ARC_EVENT_LOG_H
#define __ASM_ARC_EVENT_LOG_H

/*######################################################################
 *
 *    Event Logging API
 *
 *#####################################################################*/

/* Size of the log buffer */
#define MAX_SNAPS  1024
#define MAX_SNAPS_MASK (MAX_SNAPS-1)

/* Helpers to setup Event IDs:
 * 8 classes of events possible
 * 23 unique events for each Class
 * Right now we have only 3 classes:
 * Entry into kernel, exit from kernel and everything else is custom event
 *
 * Need for this fancy numbering scheme so that in event logger, class specific
 * things, common for all events in class, could be easily done
 */
#define EVENT_ID(x)             (0x100 << x)
#define EVENT_CLASS_ENTER       0x01	// Need to start from 1, not 0
#define EVENT_CLASS_EXIT        0x02
#define EVENT_CLASS_CUSTOM      0x80

#define KERNEL_ENTER_EVENT(x)   (EVENT_ID(x)|EVENT_CLASS_ENTER)
#define KERNEL_EXIT_EVENT(x)    (EVENT_ID(x)|EVENT_CLASS_EXIT)
#define CUSTOM_EVENT(x)         (EVENT_ID(x)|EVENT_CLASS_CUSTOM)

/* Actual Event IDs used in kernel code */
#define SNAP_INTR_IN            KERNEL_ENTER_EVENT(0)
#define SNAP_EXCP_IN            KERNEL_ENTER_EVENT(1)
#define SNAP_TRAP_IN            KERNEL_ENTER_EVENT(2)
#define SNAP_INTR_IN2           KERNEL_ENTER_EVENT(3)

#define SNAP_INTR_OUT           KERNEL_EXIT_EVENT(0)
#define SNAP_EXCP_OUT           KERNEL_EXIT_EVENT(1)
#define SNAP_TRAP_OUT           KERNEL_EXIT_EVENT(2)
#define SNAP_INTR_OUT2          KERNEL_EXIT_EVENT(3)
#define SNAP_EXCP_OUT_FAST      KERNEL_EXIT_EVENT(4)

#define SNAP_PRE_CTXSW_2_U      CUSTOM_EVENT(0)
#define SNAP_PRE_CTXSW_2_K      CUSTOM_EVENT(1)
#define SNAP_DO_PF_ENTER        CUSTOM_EVENT(2)
#define SNAP_DO_PF_EXIT         CUSTOM_EVENT(3)
#define SNAP_TLB_FLUSH_ALL      CUSTOM_EVENT(4)
#define SNAP_PREEMPT_SCH_IRQ    CUSTOM_EVENT(5)
#define SNAP_PREEMPT_SCH        CUSTOM_EVENT(6)
#define SNAP_SIGRETURN          CUSTOM_EVENT(7)
#define SNAP_BEFORE_SIG         CUSTOM_EVENT(8)

#define SNAP_SENTINEL           CUSTOM_EVENT(22)


#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

#define take_snap(type,extra,ptreg)
#define sort_snaps(halt_after_sort)

#else

#ifndef __ASSEMBLY__

typedef struct {

    /*  0 */ char nm[16];
    /* 16 */ unsigned int extra; /* Traps: Sys call num,
                                    Intr: IRQ, Excepn:
                                  */
    /* 20 */ unsigned int  fault_addr;
    /* 24 */ unsigned int  cause;
    /* 28 */ unsigned int task;
    /* 32 */ unsigned long time;
    /* 36 */ unsigned int  event;
    /* 40 */ unsigned int  sp;
    /* 44 */ unsigned int  extra2;
    /* 40 */ unsigned int  extra3;

}
timeline_log_t;

void take_snap(int type, unsigned int extra, unsigned int extra2);
void sort_snaps(int halt_after_sort);

#endif  /* __ASSEMBLY__ */

#endif /* CONFIG_ARC_DBG_EVENT_TIMELINE */

/*######################################################################
 *
 *    Process fork/exec Logging
 *
 *#####################################################################*/

#ifndef __ASSEMBLY__

//#define CONFIG_DEBUG_ARC_PID 1

#ifndef CONFIG_DEBUG_ARC_PID

#define fork_exec_log(p, c)

#endif /* CONFIG_DEBUG_ARC_PID */

#endif /* __ASSEMBLY__ */

#endif /* __ASM_ARC_EVENT_PROFILE_H */

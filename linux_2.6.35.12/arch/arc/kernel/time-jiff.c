/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  "Jiffies" based Time keeping with architecture specific
 *      -TOD
 *      -Tick processing
 *
 *  Both of these are now done by generic code.
 *  Please refer to arch/arc/kernel/time.c whic has the new cool stuff
 *  This file is there just in case some body needs our good old lean-thin
 *  timer infrastructure
 *
 * Mar 2009: Rajeshwar Ranga
 *  Added Timer1 as clock source.
 *
 * Mar 2009: Vineetg
 *  -forked off time.c into time.c (new) and time-jiff.c (old)
 *
 * Jan 22 2008: Vineet Gupta
 *  Removed the redundant scheduler_tick() from timer_handler()
 *   because update_process_times() was already calling it
 *   This shaved off about 25% cycles off of Timer ISR
 *
 * Dec 28 2007: Rajeshwar Ranga
 *  Split the Timer Interrupt Handler for Boot CPU and Others
 *  Boot CPU will do the global time keeping as well as scheduling
 *    ALL other CPU(s) onyl tick the scheduler
 *
 * Oct 30 2007: Simon Spooner
 *  Implemented the arch specific gettimeofday routines
 *
 * Amit Shah, Rahul Trivedi, Sameer Dhavale: Codito Technologies 2004
 */

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/timex.h>
#include <linux/profile.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arcregs.h>
#include <asm/event-log.h>
#include <linux/clocksource.h>

/* ARC700 has two 32bit independent prog Timers: TIMER0 and TIMER1
 * Each can programmed to go from @count to @limit and optionally
 * interrupt when that happens
 */

/******************************************************************
 * Hardware Interface routines to program the ARC TIMERs
 ******************************************************************/

/*
 * Arm the timer to interrupt after @limit cycles
 */
void arc_timer0_setup_event(unsigned int limit)
{
    /* setup start and end markers */
    write_new_aux_reg(ARC_REG_TIMER0_LIMIT, limit);
    write_new_aux_reg(ARC_REG_TIMER0_CNT, 0);     // start from 0

    /* IE: Interrupt on count = limit,
     * NH: Count cycles only when CPU running (NOT Halted)
     */
    write_new_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_IE|TIMER_CTRL_NH);
}

/*
 * Acknowledge the interrupt
 */
void arc_timer0_ack_event(void)
{
    /* Ack the interrupt by writing to CTRL reg.
       Any write will cause intr to be ack, however we only
       set the bits which sh be 1
     */
    write_new_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_IE | TIMER_CTRL_NH);
}

/*
 */
void arc_timer1_setup_free_flow(unsigned int limit)
{
    /* although for free flowing case, limit would alway be max 32 bits
     * still we've kept the interface open, just in case ...
     */
    write_new_aux_reg(ARC_REG_TIMER1_LIMIT, limit);     // caller specifies
    write_new_aux_reg(ARC_REG_TIMER1_CNT, 0);           // start from 0
    write_new_aux_reg(ARC_REG_TIMER1_CTRL, TIMER_CTRL_NH);
}


/******************************************************************
 * Old way of implementing gettimeofday backend
 * Now handled by architecture independent Generic TOD subsystem
 ******************************************************************/

#ifndef CONFIG_GENERIC_TIME

int do_settimeofday(struct timespec *tv)
{
     time_t wtm_sec, sec = tv->tv_sec;
     long wtm_nsec, nsec = tv->tv_nsec;

     if ((unsigned long)tv->tv_nsec >= NSEC_PER_SEC)
                return -EINVAL;

     write_seqlock_irq(&xtime_lock);
     {
        wtm_sec  = wall_to_monotonic.tv_sec + (xtime.tv_sec - sec);
        wtm_nsec = wall_to_monotonic.tv_nsec + (xtime.tv_nsec - nsec);

        set_normalized_timespec(&xtime, sec, nsec);
        set_normalized_timespec(&wall_to_monotonic, wtm_sec, wtm_nsec);

        time_adjust = 0;                /* stop active adjtime() */
        time_status |= STA_UNSYNC;
        time_maxerror = NTP_PHASE_LIMIT;
        time_esterror = NTP_PHASE_LIMIT;
     }
     write_sequnlock_irq(&xtime_lock);
     clock_was_set();
     return 0;
}

EXPORT_SYMBOL(do_settimeofday);

void do_gettimeofday(struct timeval *tv)
{
    unsigned long flags;
    unsigned long seq;
    unsigned long usec, sec;

    do {
        seq = read_seqbegin_irqsave(&xtime_lock, flags);
        sec = xtime.tv_sec;
        usec = xtime.tv_nsec / 1000;
    } while (read_seqretry_irqrestore(&xtime_lock, seq, flags));

    while (unlikely(usec >= USEC_PER_SEC)) {
        usec -= USEC_PER_SEC;
        ++sec;
    }
    tv->tv_sec = sec;
    tv->tv_usec = usec;
}

EXPORT_SYMBOL(do_gettimeofday);

#else

/********** Clock Source Device *********/

static cycle_t cycle_read_t1(void)
{
    return read_new_aux_reg(ARC_REG_TIMER1_CNT);
}

static struct clocksource clocksource_t1 = {
    .name   = "ARC Timer1",
    .rating = 300,
    .read   = cycle_read_t1,
    .mask   = CLOCKSOURCE_MASK(32),
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

void __init arc_clocksource_init(void)
{
    u64 temp;
    u32 shift;
    struct clocksource *cs = &clocksource_t1;

    /* Find a shift value */
    for (shift = 32; shift > 0; shift--) {
        temp = (u64) NSEC_PER_SEC << shift;
        do_div(temp, CONFIG_ARC700_CLK);
        if ((temp >> 32) == 0)
            break;
    }
    cs->shift = shift;
    cs->mult = (u32) temp;

    arc_timer1_setup_free_flow(0xFFFFFFFF);

    clocksource_register(&clocksource_t1);
}
#endif

/**************************************************************
 * Old way of counting ticks and deciding what to do each tick
 * Now handled by architecture independent code
 ************************************************************/

irqreturn_t timer_handler(int irq, void *dev_id);

static struct irqaction arc_timer_irq = {
	.name		= "ARC Timer0",
	.flags		= 0,
	.handler	= timer_handler,
};

void board_setup_timer(void)
{
    setup_irq(TIMER0_INT, &arc_timer_irq);

    /* Setup TIMER0 as the Linux heartbeat, ticking HZ times a sec */
    arc_timer0_setup_event(CONFIG_ARC700_CLK / HZ);

#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
    /* TIMER1 to provide free flowing cycle timestamping for event logger */
    arc_timer1_setup_free_flow(0xEFFFFFFF);
#endif
}

irqreturn_t timer_handler(int irq, void *dev_id)
{
    arc_timer0_ack_event();

    /* only Boot CPU, ID = 0, does wall time keeping */
    if (!smp_processor_id()) {
        write_seqlock(&xtime_lock);
        do_timer(1);
        write_sequnlock(&xtime_lock);
    }

    update_process_times(user_mode(get_irq_regs()));

#ifdef CONFIG_PROFILING
    profile_tick(CPU_PROFILING);
#endif

    return IRQ_HANDLED;
}



void __init time_init(void)
{
    extern unsigned long clk_speed;

    if (clk_speed != CONFIG_ARC700_CLK)
        panic("CONFIG_ARC700_CLK doesn't match corresp boot param\n");

    xtime.tv_sec = 0;
    xtime.tv_nsec = 0;
    set_normalized_timespec(&wall_to_monotonic, -xtime.tv_sec, -xtime.tv_nsec);

#ifdef CONFIG_GENERIC_TIME
    arc_clocksource_init();
#endif

    board_setup_timer();
}

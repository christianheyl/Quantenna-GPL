/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/
#include <linux/module.h>
#include <asm/board/pm.h>
#include "qdrv_sch_pm.h"
#include <qtn/qdrv_sch.h>

#if QDRV_SCH_PM

atomic_t qdrv_sch_pm_enqueued_counter = ATOMIC_INIT(0);

static unsigned int pm_slow_counter = 0;
static unsigned int pm_avg_enqueued = 0;
static atomic_t pm_slow_state = ATOMIC_INIT(0);
static atomic_t pm_init_counter = ATOMIC_INIT(0);
static struct timer_list pm_timer;
static struct delayed_work pm_work;

static inline void qdrv_sch_pm_queue_work(unsigned long delay)
{
	pm_slow_counter = 0;
	pm_avg_enqueued = 0;
	pm_queue_work(&pm_work, delay);
}

static void qdrv_sch_pm_wq_func(struct work_struct *work)
{
	pm_qos_update_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_QDISC,
		atomic_read(&pm_slow_state) ? BOARD_PM_LEVEL_SLOW_DOWN : PM_QOS_DEFAULT_VALUE);
}

static void qdrv_sch_pm_timer_func(unsigned long data)
{
	int enqueued = atomic_xchg(&qdrv_sch_pm_enqueued_counter, 0);

	/* calculate average enqueued packets number */
	if (pm_avg_enqueued == 0) {
		pm_avg_enqueued = (enqueued << 1);
	} else {
		pm_avg_enqueued = ((enqueued + pm_avg_enqueued) >> 1);
	}

	/* update counter of how long low level of traffic observed */
	if ((enqueued >= BOARD_PM_QDISC_SPEEDUP_THRESHOLD) ||
			(pm_avg_enqueued >= BOARD_PM_QDISC_SLOWDOWN_THRESHOLD)) {
		pm_slow_counter = 0;
	} else if (pm_slow_counter < BOARD_PM_QDISC_SLOWDOWN_COUNT) {
		++pm_slow_counter;
	}

	/* handle state transition */
	if (enqueued >= BOARD_PM_QDISC_SPEEDUP_THRESHOLD) {
		if (atomic_xchg(&pm_slow_state, 0)) {
			qdrv_sch_pm_queue_work(BOARD_PM_QDISC_DEFAULT_TIMEOUT);
		}
	} else if (pm_slow_counter >= BOARD_PM_QDISC_SLOWDOWN_COUNT) {
		if (!atomic_xchg(&pm_slow_state, 1)) {
			qdrv_sch_pm_queue_work(BOARD_PM_QDISC_SLOWDOWN_TIMEOUT);
		}
	}

	/* restart timer */
	mod_timer(&pm_timer, jiffies + BOARD_PM_QDISC_TIMER_TIMEOUT);
}

void qdrv_sch_pm_init(void)
{
	if (atomic_add_return(1, &pm_init_counter) != 1) {
		return;
	}

	pm_qos_add_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_QDISC, BOARD_PM_LEVEL_SLOW_DOWN);

	INIT_DELAYED_WORK(&pm_work, qdrv_sch_pm_wq_func);

	init_timer(&pm_timer);
	pm_timer.function = qdrv_sch_pm_timer_func;
	pm_timer.expires = jiffies + BOARD_PM_QDISC_TIMER_TIMEOUT;
	add_timer(&pm_timer);

	atomic_set(&pm_slow_state, 1);
}

void qdrv_sch_pm_exit(void)
{
	if (atomic_sub_return(1, &pm_init_counter) != 0) {
		return;
	}

	del_timer(&pm_timer);
	pm_flush_work(&pm_work);
	pm_qos_remove_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_QDISC);
}

#endif	/* QDRV_SCH_PM */

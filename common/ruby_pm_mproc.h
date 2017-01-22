/*
 * (C) Copyright 2012 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __RUBY_PM_MPROC_H
#define __RUBY_PM_MPROC_H

#if defined(__KERNEL__) || defined(MUC_BUILD)
	#include <qtn/mproc_sync.h>
	#include "ruby_pm.h"

#if QTN_SEM_TRACE
#define qtn_pm_duty_try_lock(_cpu) qtn_pm_duty_try_lock_dbg(_cpu, __FILE__, __LINE__)
	RUBY_WEAK(qtn_pm_duty_try_lock_dbg) int
	qtn_pm_duty_try_lock_dbg(QTN_SOC_CPU current_cpu, char *caller, int caller_line)
#else
	RUBY_WEAK(qtn_pm_duty_try_lock) int
	qtn_pm_duty_try_lock(QTN_SOC_CPU current_cpu)
#endif
	{
		int ret = 0;
		unsigned long flags;
		uint32_t fail_sem = 0;

		if (__qtn_mproc_sync_spin_try_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags, &fail_sem)) {
			u_int32_t lock = qtn_mproc_sync_addr(&qtn_mproc_sync_shared_params_get()->pm_duty_lock);
			if (!qtn_mproc_sync_mem_read(lock)) {
				qtn_mproc_sync_mem_write_wmb(lock, 1);
				ret = 1;
			}
			__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
		}

		return ret;
	}

#if QTN_SEM_TRACE
#define qtn_pm_duty_unlock(_cpu) qtn_pm_duty_unlock_dbg(_cpu, __FILE__, __LINE__)
	RUBY_WEAK(qtn_pm_duty_unlock_dbg) void
	qtn_pm_duty_unlock_dbg(QTN_SOC_CPU current_cpu, char *caller, int caller_line)
#else
	RUBY_WEAK(qtn_pm_duty_unlock) void
	qtn_pm_duty_unlock(QTN_SOC_CPU current_cpu)
#endif
	{
		u_int32_t lock = qtn_mproc_sync_addr(&qtn_mproc_sync_shared_params_get()->pm_duty_lock);
		unsigned long flags;

		__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
		qtn_mproc_sync_mem_write_wmb(lock, 0);
		__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
	}

#endif	// defined(__KERNEL__) || defined(MUC_BUILD)

#endif /* __RUBY_PM_MPROC_H */


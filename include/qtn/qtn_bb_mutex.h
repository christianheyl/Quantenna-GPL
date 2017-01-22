/*
 * (C) Copyright 2011 Quantenna Communications Inc.
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

#ifndef __QTN_BB_MUTEX_H
#define __QTN_BB_MUTEX_H

#include "mproc_sync.h"

#ifndef __ASSEMBLY__

#define QTN_BB_RESET_REGISTER_VAL		0x7A200
#define QTN_BB_HRESET_REGISTER_VAL		0x10000

struct qtn_bb_mutex
{
	volatile u_int32_t ref_counter;
	volatile u_int32_t collision_counter;
	volatile u_int32_t enter_counter;
};

RUBY_INLINE struct qtn_bb_mutex*
qtn_bb_mutex_get(void)
{
#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
	return (struct qtn_bb_mutex*)qtn_mproc_sync_nocache
		(qtn_mproc_sync_shared_params_get()->bb_mutex_bus);
#else
	/* Linux target */
	return qtn_mproc_sync_shared_params_get()->bb_mutex_lhost;
#endif
}

/*
 * Atomically increase reference counter, acquiring reader mutex if not previously held
 */
#if QTN_SEM_TRACE
#define qtn_bb_mutex_enter(_cpu)    qtn_bb_mutex_enter_dbg(_cpu, __FILE__, __LINE__)
RUBY_WEAK(qtn_bb_mutex_enter_dbg) int
qtn_bb_mutex_enter_dbg(QTN_SOC_CPU current_cpu, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_bb_mutex_enter) int
qtn_bb_mutex_enter(QTN_SOC_CPU current_cpu)
#endif
{
	struct qtn_bb_mutex *bb_mutex = qtn_bb_mutex_get();
	unsigned long flags;

	__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
	if (bb_mutex->ref_counter == 0) {
		++(bb_mutex->enter_counter);
	} else {
		++(bb_mutex->collision_counter);
	}
	__qtn_mproc_refcnt_inc(&bb_mutex->ref_counter);
	__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);

	return 0;
}

/*
 * Atomically decrease reference counter, releasing reader mutex if transitioning to 0
 */
#if QTN_SEM_TRACE
#define qtn_bb_mutex_leave(_cpu)    qtn_bb_mutex_leave_dbg(_cpu, __FILE__, __LINE__)
RUBY_WEAK(qtn_bb_mutex_leave_dbg) int
qtn_bb_mutex_leave_dbg(QTN_SOC_CPU current_cpu, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_bb_mutex_leave) int
qtn_bb_mutex_leave(QTN_SOC_CPU current_cpu)
#endif
{
	struct qtn_bb_mutex *bb_mutex = qtn_bb_mutex_get();
	unsigned long flags;

	__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
	__qtn_mproc_refcnt_dec(&bb_mutex->ref_counter);
	__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);

	return 0;
}

/*
 * Enable RIFS mode. Safe to call using any processor.
 * Make sure that SoC support this mode before calling.
 */
#if QTN_SEM_TRACE
#define qtn_rifs_mode_enable(_cpu)   qtn_rifs_mode_enable_dbg(_cpu, __FILE__, __LINE__)
RUBY_WEAK(qtn_rifs_mode_enable_dbg) void
qtn_rifs_mode_enable_dbg(QTN_SOC_CPU current_cpu, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_rifs_mode_enable) void
qtn_rifs_mode_enable(QTN_SOC_CPU current_cpu)
#endif
{
	unsigned long flags;

	__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
	qtn_mproc_sync_mem_write(RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE, RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE_ON);
	__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
}

/*
 * Disable RIFS mode. Safe to call using any processor.
 * Make sure that SoC support this mode before calling.
 */
#if QTN_SEM_TRACE
#define qtn_rifs_mode_disable(_cpu)   qtn_rifs_mode_disable_dbg(_cpu, __FILE__, __LINE__)
RUBY_WEAK(qtn_rifs_mode_disable_dbg) void
qtn_rifs_mode_disable_dbg(QTN_SOC_CPU current_cpu, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_rifs_mode_disable) void
qtn_rifs_mode_disable(QTN_SOC_CPU current_cpu)
#endif
{
	unsigned long flags;

	__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
	qtn_mproc_sync_mem_write(RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE, RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE_OFF);
	__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, &flags);
}

/*
 * Acquiring writer mutex
 */
#if QTN_SEM_TRACE
#define qtn_bb_mutex_reset_enter(_cpu, _flags)   qtn_bb_mutex_reset_enter_dbg(_cpu, _flags, __FILE__, __LINE__)
RUBY_WEAK(qtn_bb_mutex_reset_enter_dbg) void
qtn_bb_mutex_reset_enter_dbg(QTN_SOC_CPU current_cpu, unsigned long *flags, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_bb_mutex_reset_enter) void
qtn_bb_mutex_reset_enter(QTN_SOC_CPU current_cpu, unsigned long *flags)
#endif
{
	struct qtn_bb_mutex *bb_mutex = qtn_bb_mutex_get();

	while (1) {
		__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, flags);
		if (bb_mutex->ref_counter == 0) {
			break;
		}
		__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, flags);
	}
}

/*
 * Try to acquire writer mutex (to release qtn_bb_mutex_reset_leave() must be called)
 */
#if QTN_SEM_TRACE
#define qtn_bb_mutex_reset_try_enter(_cpu, _flags)   qtn_bb_mutex_reset_try_enter_dbg(_cpu, _flags, __FILE__, __LINE__)
RUBY_WEAK(qtn_bb_mutex_reset_try_enter_dbg) int
qtn_bb_mutex_reset_try_enter_dbg(QTN_SOC_CPU current_cpu, unsigned long *flags, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_bb_mutex_reset_try_enter) int
qtn_bb_mutex_reset_try_enter(QTN_SOC_CPU current_cpu, unsigned long *flags)
#endif
{
	int ret = 0;
	struct qtn_bb_mutex *bb_mutex = qtn_bb_mutex_get();

	__qtn_mproc_sync_spin_lock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, flags);
	if (bb_mutex->ref_counter == 0) {
		ret = 1;
	} else {
		__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, flags);
	}

	return ret;
}

/*
 * Release writer mutex
 */
#if QTN_SEM_TRACE
#define qtn_bb_mutex_reset_leave(_cpu, _flags)   qtn_bb_mutex_reset_leave_dbg(_cpu, _flags, __FILE__, __LINE__)
RUBY_WEAK(qtn_bb_mutex_reset_leave_dbg) void
qtn_bb_mutex_reset_leave_dbg(QTN_SOC_CPU current_cpu, unsigned long *flags, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_bb_mutex_reset_leave) void
qtn_bb_mutex_reset_leave(QTN_SOC_CPU current_cpu, unsigned long *flags)
#endif
{
	__qtn_mproc_sync_spin_unlock(current_cpu, QTN_ALL_SOC_CPU, QTN_SEM_BB_MUTEX_SEMNUM, flags);
}

/*
 * Return true if reset needed.
 */
RUBY_INLINE int
qtn_bb_mutex_is_reset(QTN_SOC_CPU current_cpu, u_int32_t status)
{
	return (status & QTN_BB_RESET_REGISTER_VAL);
}

RUBY_INLINE int
qtn_bb_mutex_is_hreset(QTN_SOC_CPU current_cpu, u_int32_t status)
{
	return (status & QTN_BB_HRESET_REGISTER_VAL);
}

#endif // #ifndef __ASSEMBLY__

#endif // #ifndef __QTN_BB_MUTEX_H


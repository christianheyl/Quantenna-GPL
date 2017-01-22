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

#ifndef __QTN_MPROC_SYNC_H
#define __QTN_MPROC_SYNC_H

#ifdef __KERNEL__
#include <linux/sched.h>
#endif

#include "mproc_sync_base.h"
#include "semaphores.h"
#include "shared_params.h"
#include "topaz_tqe_cpuif.h"

#ifndef __ASSEMBLY__

#define QTN_MPROC_TIMEOUT	(6 * HZ)

/*
 * NOTE: functions started from "__" are internal, and must not be used by client code.
 */

/* Enum represents each CPU in system */
typedef enum _QTN_SOC_CPU
{
	QTN_LHOST_SOC_CPU = (1 << 0),
	QTN_MUC_SOC_CPU   = (1 << 1),
	QTN_DSP_SOC_CPU   = (1 << 2)
} QTN_SOC_CPU;

#if QTN_SEM_TRACE
#define QTN_SEM_TRACE_NUM    12
#define QTN_SEM_TRACE_DEPTH  2

#define SEM_TRACE_CPU_LHOST     0
#define SEM_TRACE_CPU_MUC       1
#define SEM_TRACE_CPU_DSP       2
#define SEM_TRACE_CPU_AUC       3
#define SEM_TRACE_CPU_NUM       4

enum qtn_sem_state {
	QTN_SEM_STARTLOCK = 0,
	QTN_SEM_LOCKED = 1,
	QTN_SEM_UNLOCKED = 2,
};

struct qtn_sem_trace_entry {
	volatile uint32_t    pos;
	volatile uint64_t    jiffies;        /* per cpu jiffies: lhost: 32b jiffies; mus: 64b jiffies; dsp: no jiffies */
	volatile uint32_t    state;
	volatile uint32_t    caller_file[QTN_SEM_TRACE_DEPTH];
	volatile uint32_t    caller_line[QTN_SEM_TRACE_DEPTH];
};
struct qtn_sem_trace_log {
	volatile uint32_t trace_pos[SEM_TRACE_CPU_NUM];
	volatile uint32_t trace_idx[SEM_TRACE_CPU_NUM];
	volatile uint32_t last_dump_pos[SEM_TRACE_CPU_NUM];
	struct qtn_sem_trace_entry traces[SEM_TRACE_CPU_NUM][QTN_SEM_TRACE_NUM];
};

#if defined(DSP_BUILD)
#define PER_CPU_CLK    0
#else
#define PER_CPU_CLK    jiffies    /* Lhost, MuC and AuC has different HZ */
#endif

#endif /* QTN_SEM_TRACE */

#if defined(AUC_BUILD)
#define PER_CPU_PRINTK                auc_os_printf
#elif defined(MUC_BUILD)
#define PER_CPU_PRINTK                uc_printk
#elif !defined(DSP_BUILD)
#define PER_CPU_PRINTK                printk
#endif

#define	HAL_REG_READ_RAW(_r)	(uint32_t)(*((volatile uint32_t *)(_r)))

#if defined(CONFIG_RUBY_PCIE_TARGET)
/*
 * Trashed L2M_SEM_REG(0xE0000094) will lead to semaphore deadlock in Muc.
 * We still don't know who/where/why the register been trashed.
 * Proposal a workaround as following:
 * Record a software copy of the L2M and M2L semaphore register, and set/clear/update
 * the register(L2M and M2L) according to it's software copy.
 */
#define	CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND	(1)
#define	QTN_SYNC_MAX_RW_CHECK_NUM					(10000)
#endif

#define QTN_ALL_SOC_CPU	(QTN_LHOST_SOC_CPU | QTN_MUC_SOC_CPU | QTN_DSP_SOC_CPU)
#define QTN_MULTI_PROCESS_TQE_SEMA		0xf
#define QTN_MULTI_PROCESSOR_SEMA_KEY_SHIFT	28

/*
 * This multi-processor semaphore register supports up to 7 semaphores,
 * which is implemented by dedicated flops, not memory. Reading them is as slow or even slower
 * than reading SRAM.
 * Currently, the first semaphore is used for TQE and the other 6 semaphores are unused.
 * However enabling other semaphores could introduce more wait cycles to each other.
 * The semaphore lock process are:
 * 1. Try to lock with write CPUID | CPUID << 24 to semaphore register.
 * 2. Return immediately if successfully lock with passing read verify, otherwise step 3.
 * 3. Read semaphore and wait for free to lock, then step 1 or timeout with a failure.
 */
RUBY_INLINE int
_qtn_mproc_3way_tqe_sem_down(enum topaz_mproc_tqe_sem_id cpuid)
{
	const uint32_t  set_value = ((cpuid << QTN_MULTI_PROCESSOR_SEMA_KEY_SHIFT) | cpuid);
	uint32_t        sem_set_cnt = 0;

	do {
		/*
		 * The semaphore bits [3:0] can be set successfully only when it is unset or already
		 * owned by current cpuid, otherwise the write has no effect.
		 */
		qtn_mproc_sync_mem_write(TOPAZ_MPROC_SEMA, set_value);
		if ((qtn_mproc_sync_mem_read(TOPAZ_MPROC_SEMA) &
				QTN_MULTI_PROCESS_TQE_SEMA) == cpuid) {
			return 1;
		}
	} while (++sem_set_cnt < TQE_SEMA_GET_MAX);

	return 0;
}

/*
 * Returns 1 mean success.
 * Returns 0 if the processor did not hold the semaphore.
 */
RUBY_INLINE int
_qtn_mproc_3way_tqe_sem_up(enum topaz_mproc_tqe_sem_id cpuid)
{
	uint32_t	value;

	value = qtn_mproc_sync_mem_read(TOPAZ_MPROC_SEMA);
	value &= QTN_MULTI_PROCESS_TQE_SEMA;
	if (value != cpuid)
		return 0;
	/* Write current ID back to release HW semaphore */
	qtn_mproc_sync_mem_write(TOPAZ_MPROC_SEMA, value << QTN_MULTI_PROCESSOR_SEMA_KEY_SHIFT);

	return 1;
}

RUBY_INLINE void
__qtn_mproc_refcnt_inc(volatile u_int32_t *refcnt)
{
	*refcnt = *refcnt + 1;
	qtn_addr_wmb(qtn_mproc_sync_addr(refcnt));
}

RUBY_INLINE void
__qtn_mproc_refcnt_dec(volatile u_int32_t *refcnt)
{
	u_int32_t tmp = *refcnt;
	if (tmp == 0) {
		qtn_mproc_sync_log("ref counter dec broken");
	} else {
		*refcnt = tmp - 1;
		qtn_addr_wmb(qtn_mproc_sync_addr(refcnt));
	}
}

RUBY_INLINE u_int32_t
__qtn_mproc_sync_hw_sem1_addr(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set)
{
	switch (current_cpu)
	{
	case QTN_LHOST_SOC_CPU:
		if (QTN_DSP_SOC_CPU & peer_cpu_set) {
			return RUBY_SYS_CTL_L2D_SEM;
		}
		break;

	case QTN_MUC_SOC_CPU:
		if (QTN_DSP_SOC_CPU & peer_cpu_set) {
			return RUBY_SYS_CTL_M2D_SEM;
		}
		break;

	case QTN_DSP_SOC_CPU:
		if (QTN_MUC_SOC_CPU & peer_cpu_set) {
			return RUBY_SYS_CTL_D2M_SEM;
		}
		break;
	}

	return RUBY_BAD_BUS_ADDR;
}

RUBY_INLINE u_int32_t
__qtn_mproc_sync_hw_sem2_addr(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set)
{
	switch (current_cpu)
	{
	case QTN_LHOST_SOC_CPU:
		if (QTN_MUC_SOC_CPU & peer_cpu_set) {
			return RUBY_SYS_CTL_L2M_SEM;
		}
		break;

	case QTN_MUC_SOC_CPU:
		if (QTN_LHOST_SOC_CPU & peer_cpu_set) {
			return RUBY_SYS_CTL_M2L_SEM;
		}
		break;

	case QTN_DSP_SOC_CPU:
		if (QTN_LHOST_SOC_CPU & peer_cpu_set) {
			return RUBY_SYS_CTL_D2L_SEM;
		}
		break;
	}

	return RUBY_BAD_BUS_ADDR;
}

RUBY_INLINE u_int32_t
__qtn_mproc_sync_hw_sem_bit(u_int32_t which_sem)
{
	return (1 << which_sem);
}

RUBY_INLINE int
__qtn_mproc_sync_set_hw_sem(u_int32_t sem_addr, u_int32_t which_sem)
{
	int ret = 0;
#if defined(CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND)
	/* check counter */
	int check_counter = 0;
#endif

	if (sem_addr == RUBY_BAD_BUS_ADDR) {
		ret = 1;
	} else {
		u_int32_t sem_bit = __qtn_mproc_sync_hw_sem_bit(which_sem);
		u_int32_t sem_val = qtn_mproc_sync_mem_read(sem_addr);

#if defined(CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND)
		while (sem_val != qtn_mproc_sync_mem_read(sem_addr)) {
			if(++check_counter > QTN_SYNC_MAX_RW_CHECK_NUM) {
				qtn_mproc_sync_log("__qtn_mproc_sync_set_hw_sem: read semaphore mismatch...");
				return ret;
			} else {
				sem_val = qtn_mproc_sync_mem_read(sem_addr);
			}
		}
#endif	/* CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND */
		sem_val |= sem_bit;

		if (qtn_mproc_sync_mem_write_wmb(sem_addr, sem_val) & sem_bit) {
			ret = 1;
		}
	}
	return ret;
}

RUBY_INLINE void
__qtn_mproc_sync_clear_hw_sem(u_int32_t sem_addr, u_int32_t which_sem)
{
#if defined(CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND)
	/* check counter */
	int check_counter = 0;
#endif

	if (sem_addr != RUBY_BAD_BUS_ADDR) {
		u_int32_t sem_bit = __qtn_mproc_sync_hw_sem_bit(which_sem);
		u_int32_t sem_val = qtn_mproc_sync_mem_read(sem_addr);

#if defined(CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND)
		while (sem_val != qtn_mproc_sync_mem_read(sem_addr)) {
			if(++check_counter > QTN_SYNC_MAX_RW_CHECK_NUM) {
				check_counter = 0;
				qtn_mproc_sync_log("__qtn_mproc_sync_clear_hw_sem: read semaphore mismatch...");
			}
			sem_val = qtn_mproc_sync_mem_read(sem_addr);
		}
#endif	/* CONFIG_PCIE_TARGET_SEM_TRASHED_WORKAROUND */
		sem_val &= ~sem_bit;

		qtn_mproc_sync_mem_write_wmb(sem_addr, sem_val);
	}
}

RUBY_INLINE int
__qtn_mproc_sync_spin_try_lock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set,
	u_int32_t which_sem, unsigned long *flags, uint32_t *fail_sem)
{
	u_int32_t sem1_addr = __qtn_mproc_sync_hw_sem1_addr(current_cpu, peer_cpu_set);
	u_int32_t sem2_addr = __qtn_mproc_sync_hw_sem2_addr(current_cpu, peer_cpu_set);

	local_irq_save(*flags);

	if(!__qtn_mproc_sync_set_hw_sem(sem1_addr, which_sem)) {
		*fail_sem = sem1_addr;
		goto unlock1;
	}

	if(!__qtn_mproc_sync_set_hw_sem(sem2_addr, which_sem)) {
		*fail_sem = sem2_addr;
		goto unlock2;
	}

	return 1;

unlock2:
	__qtn_mproc_sync_clear_hw_sem(sem1_addr, which_sem);
unlock1:
	local_irq_restore(*flags);
	return 0;
}

RUBY_INLINE void
qtn_mproc_sync_spin_lock_reg_dump(void)
{
#if !defined(DSP_BUILD)
	uint32_t reg;

	PER_CPU_PRINTK("Dump semaphore registers:\n");
	for (reg = RUBY_SYS_CTL_L2M_SEM; reg <= RUBY_SYS_CTL_D2M_SEM; reg += 4) {
		PER_CPU_PRINTK("reg 0x%08x=0x%08x\n", reg, HAL_REG_READ_RAW(reg));
	}
#endif
}

#if QTN_SEM_TRACE
RUBY_INLINE struct qtn_sem_trace_log *qtn_mproc_sync_get_sem_trace_log(void)
{
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_sem_trace_log *log;

#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
	log = sp->sem_trace_log_bus;
#else
	log = sp->sem_trace_log_lhost;
#endif
	return log;
}

RUBY_INLINE void qtn_sem_trace_log(int state, char *caller0_file, int caller0_line, char *caller1_file, int caller1_line)
{
#if !defined(DSP_BUILD) || (QTN_SEM_TRACE_DSP)
	struct qtn_sem_trace_log *log = qtn_mproc_sync_get_sem_trace_log();
#if defined(MUC_BUILD)
	int cpu = SEM_TRACE_CPU_MUC;
#elif defined(DSP_BUILD)
	int cpu = SEM_TRACE_CPU_DSP;
#elif defined(AUC_BUILD)
	int cpu = SEM_TRACE_CPU_AUC;
#else
	int cpu = SEM_TRACE_CPU_LHOST;
#endif
	int idx;
	uint32_t flags;

	local_irq_save(flags);

	idx = log->trace_idx[cpu];

	log->traces[cpu][idx].pos = log->trace_pos[cpu];
	log->traces[cpu][idx].jiffies = PER_CPU_CLK;
	log->traces[cpu][idx].state = state;
	log->traces[cpu][idx].caller_file[0] = (unsigned int)caller0_file;
	log->traces[cpu][idx].caller_line[0] = caller0_line;
	log->traces[cpu][idx].caller_file[1] = (unsigned int)caller1_file;
	log->traces[cpu][idx].caller_line[1] = caller1_line;
	log->trace_pos[cpu]++;
	log->trace_idx[cpu] = (log->trace_pos[cpu]) % QTN_SEM_TRACE_NUM;

	local_irq_restore(flags);
#endif
}

RUBY_INLINE void
qtn_mproc_sync_spin_lock_log_dump(void)
{
#if !defined(DSP_BUILD)
	struct qtn_sem_trace_log *log = qtn_mproc_sync_get_sem_trace_log();
	int i, j, idx;
	struct qtn_sem_trace_entry *e;
	unsigned int file[QTN_SEM_TRACE_DEPTH];

	PER_CPU_PRINTK("Dump semaphore trace log at jiffies=%u\n", (unsigned int)jiffies);
	for (idx = 0; idx < SEM_TRACE_CPU_NUM; idx++) {
#if !QTN_SEM_TRACE_DSP
		if (idx == SEM_TRACE_CPU_DSP) {
			PER_CPU_PRINTK("CPU %d semaphore trace log is not available in this build\n", idx);
			continue;
		}
#endif
		PER_CPU_PRINTK("CPU %d semaphore trace log: pos=%u, last_dump_pos=%u\n",
				idx, log->trace_pos[idx], log->last_dump_pos[idx]);
		for (i = 0; i < QTN_SEM_TRACE_NUM; i++) {
			e = &log->traces[idx][i];
			for (j = 0; j < QTN_SEM_TRACE_DEPTH; j++) {
				file[j] = 0;
				if (e->caller_file[j]) {
					file[j] = e->caller_file[j];
#if defined(MUC_BUILD)
					if (idx != SEM_TRACE_CPU_MUC) {
						/* have no reliable way to convert lhost/dsp/auc string addr to muc */
						file[j] = 0;
					}
#elif defined(AUC_BUILD)
					if (idx != SEM_TRACE_CPU_AUC) {
						/* have no reliable way to convert lhost/dsp/muc string addr to auc */
						file[j] = 0;
					}
#else
					/* lhost */
					if (idx != SEM_TRACE_CPU_LHOST) {
						file[j] = (unsigned int)bus_to_virt(file[j]);
					}
#endif
				}
			}
			PER_CPU_PRINTK("%d pos=%u, jiffies=%u_%u, state=%u, "
					"caller0=0x%x %s %d, caller1=0x%x %s %d\n",
					i, e->pos, U64_HIGH32(e->jiffies), U64_LOW32(e->jiffies),
					e->state,
					(unsigned int)e->caller_file[0],
					file[0] ? (char*)file[0] : "N/A",
					e->caller_line[0],
					(unsigned int)e->caller_file[1],
					file[1] ? (char*)file[1] : "N/A",
					e->caller_line[1]
					);
		}
		log->last_dump_pos[idx] = log->trace_pos[idx];
	}
#endif
}
#endif /* QTN_SEM_TRACE */

RUBY_INLINE int
__qtn_mproc_sync_spin_lock_wait(QTN_SOC_CPU current_cpu)
{
	int wait_shift = 0;
	u_int32_t pm_lock_addr = qtn_mproc_sync_addr(&qtn_mproc_sync_shared_params_get()->pm_duty_lock);
	int i;

	if (unlikely(qtn_mproc_sync_mem_read(pm_lock_addr))) {
		wait_shift = 2;
	}

	for (i = 0; i < (10 << (wait_shift + current_cpu)); ++i) {
		qtn_pipeline_drain();
	}

	return wait_shift;
}

#if QTN_SEM_TRACE
#define __qtn_mproc_sync_spin_lock(_cpu, _peer, _sem, _flags)  \
	__qtn_mproc_sync_spin_lock_dbg(_cpu, _peer, _sem, _flags, __FILE__, __LINE__, caller, caller_line)

RUBY_INLINE void
__qtn_mproc_sync_spin_lock_dbg(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set,
	u_int32_t which_sem, unsigned long *flags,
	char *caller0_file, int caller0_line,
	char *caller1_file, int caller1_line)
#else
RUBY_INLINE void
__qtn_mproc_sync_spin_lock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set,
	u_int32_t which_sem, unsigned long *flags)
#endif
{
	/* Help to detect lockups */
	unsigned log_counter = 0;
	unsigned log_max_counter = 10000;
#define LOG_MAX_COUNTER_PANIC    320000
	int log_success = 0;
	uint32_t fail_sem = 0;
	int dumped =0;
#ifdef __KERNEL__
	unsigned long timeout_jiff;
#endif

#if QTN_SEM_TRACE
	qtn_sem_trace_log(QTN_SEM_STARTLOCK, caller0_file, caller0_line, caller1_file, caller1_line);
#endif

	/*
	 * We have 3 interlocked hw semaphores to be used for mutual exclusion in 3 CPU pairs.
	 * Doesn't matter which semaphore be locked first and which second,
	 * we can easily enters dead-lock state.
	 * To prevent dead-locking let's rollback if locking of whole set of 3 mutexes is failed
	 * at any stage.
	 * Also let's add per-processor delays after failed locking, so in case of collision
	 * it will be resolved faster.
	 *
	 * I think, that hw design of hw interlocked semaphores is not very lucky.
	 * It would be much better if we have 3 registers, 1 per CPU.
	 * And all 3 (not 2 as now) registers be interlocked.
	 */
	while (!__qtn_mproc_sync_spin_try_lock(current_cpu, peer_cpu_set, which_sem, flags, &fail_sem)) {
		unsigned int i;
		for (i = 0; i < 10 * (current_cpu + 1); ++i) {
			qtn_pipeline_drain();
		}
		if(unlikely(!__qtn_mproc_sync_spin_lock_wait(current_cpu) &&
				(++log_counter >= log_max_counter))) {
			log_counter = 0;
			log_max_counter = (log_max_counter << 1);
			if (unlikely(!log_max_counter)) {
				log_max_counter = 1;
			}
			qtn_mproc_sync_log("qtn_mproc_sync: waiting for semaphore ...");
			if ((log_max_counter >= LOG_MAX_COUNTER_PANIC) && (!dumped)) {
				/* Don't make false alert for PM/COC feature */
#if QTN_SEM_TRACE
				qtn_mproc_sync_spin_lock_log_dump();
#endif
				qtn_mproc_sync_spin_lock_reg_dump();
#ifdef __KERNEL__
				timeout_jiff = jiffies + QTN_MPROC_TIMEOUT;
				while (time_before(jiffies, timeout_jiff)) {
					schedule();
				}

				panic("Semaphore hang detected at clk %u: cpu=%x peer=%x sem=%x flags=%x fail_sem=%x\n",
					(unsigned int)jiffies, current_cpu, peer_cpu_set, which_sem,
					(unsigned int)*flags, fail_sem);
#endif
				dumped = 1;
			}
			log_success = 1;
		}
	}
#if QTN_SEM_TRACE
	qtn_sem_trace_log(QTN_SEM_LOCKED, caller0_file, caller0_line, caller1_file, caller1_line);
#endif
	if (unlikely(log_success)) {
		qtn_mproc_sync_log("qtn_mproc_sync: wait succeeded");
	}
}

#if QTN_SEM_TRACE
#define __qtn_mproc_sync_spin_unlock(_cpu, _peer, _sem, _flags)  \
	__qtn_mproc_sync_spin_unlock_dbg(_cpu, _peer, _sem, _flags, __FILE__, __LINE__, caller, caller_line)

RUBY_INLINE void
__qtn_mproc_sync_spin_unlock_dbg(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set,
	u_int32_t which_sem, unsigned long *flags,
	char *caller0_file, int caller0_line,
	char *caller1_file, int caller1_line)
#else
RUBY_INLINE void
__qtn_mproc_sync_spin_unlock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set,
	u_int32_t which_sem, unsigned long *flags)
#endif
{
	/* Caller must ensure that it hold spinlock. */

	__qtn_mproc_sync_clear_hw_sem(
		__qtn_mproc_sync_hw_sem2_addr(current_cpu, peer_cpu_set),
		which_sem);
	__qtn_mproc_sync_clear_hw_sem(
		__qtn_mproc_sync_hw_sem1_addr(current_cpu, peer_cpu_set),
		which_sem);

#if QTN_SEM_TRACE
	qtn_sem_trace_log(QTN_SEM_UNLOCKED, caller0_file, caller0_line, caller1_file, caller1_line);
#endif

	local_irq_restore(*flags);
}

RUBY_INLINE int
qtn_mproc_sync_set_hw_sem(u_int32_t sem_addr, u_int32_t which_sem)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __qtn_mproc_sync_set_hw_sem(sem_addr, which_sem);
	local_irq_restore(flags);

	return ret;
}

RUBY_INLINE void
qtn_mproc_sync_clear_hw_sem(u_int32_t sem_addr, u_int32_t which_sem)
{
	unsigned long flags;

	local_irq_save(flags);
	__qtn_mproc_sync_clear_hw_sem(sem_addr, which_sem);
	local_irq_restore(flags);
}


/*
 * Try lock interprocessor spinlock. Spinlock is not recoursive.
 */
RUBY_WEAK(qtn_mproc_sync_spin_try_lock) int
qtn_mproc_sync_spin_try_lock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set, u_int32_t which_sem)
{
	unsigned long flags;
	uint32_t fail_sem;
	if (__qtn_mproc_sync_spin_try_lock(current_cpu, peer_cpu_set, which_sem, &flags, &fail_sem)) {
		local_irq_restore(flags);
		return 1;
	}
	return 0;
}

/*
  * Lock interprocessor spinlock. Spinlock is not recoursive.
 */
#if QTN_SEM_TRACE
RUBY_WEAK(qtn_mproc_sync_spin_lock) void
qtn_mproc_sync_spin_lock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set, u_int32_t which_sem, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_mproc_sync_spin_lock) void
qtn_mproc_sync_spin_lock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set, u_int32_t which_sem)
#endif
{
	unsigned long flags;
	__qtn_mproc_sync_spin_lock(current_cpu, peer_cpu_set, which_sem, &flags);
	local_irq_restore(flags);
}

/*
 * Unlock interprocessor spinlock. Spinlock is not recoursive.
 */
#if QTN_SEM_TRACE
RUBY_WEAK(qtn_mproc_sync_spin_unlock) void
qtn_mproc_sync_spin_unlock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set, u_int32_t which_sem, char *caller, int caller_line)
#else
RUBY_WEAK(qtn_mproc_sync_spin_unlock) void
qtn_mproc_sync_spin_unlock(QTN_SOC_CPU current_cpu, QTN_SOC_CPU peer_cpu_set, u_int32_t which_sem)
#endif
{
	unsigned long flags;
	local_irq_save(flags);
	__qtn_mproc_sync_spin_unlock(current_cpu, peer_cpu_set, which_sem, &flags);
}

RUBY_INLINE volatile u_int32_t*
qtn_mproc_sync_irq_fixup_data(u_int32_t irq_reg)
{
#if CONFIG_RUBY_BROKEN_IPC_IRQS
	if (irq_reg == RUBY_SYS_CTL_M2L_INT) {
		return &qtn_mproc_sync_shared_params_get()->m2l_irq[0];
	}
#endif
	return RUBY_BAD_VIRT_ADDR;
}

RUBY_INLINE volatile u_int32_t*
qtn_mproc_sync_irq_fixup_data_ack(u_int32_t irq_reg)
{
#if !defined(MUC_BUILD) && !defined(DSP_BUILD)
	return qtn_mproc_sync_irq_fixup_data(irq_reg);
#else
	return RUBY_BAD_VIRT_ADDR;
#endif
}

RUBY_INLINE volatile u_int32_t*
qtn_mproc_sync_irq_fixup_data_trigger(u_int32_t irq_reg)
{
#if defined(MUC_BUILD)
	return qtn_mproc_sync_irq_fixup_data(irq_reg);
#else
	return RUBY_BAD_VIRT_ADDR;
#endif
}

RUBY_INLINE void
qtn_mproc_sync_irq_trigger(u_int32_t irq_reg, u_int32_t irqno)
{
	u_int32_t req = (1 << (irqno + 16)) | (1 << irqno);
	qtn_mproc_sync_mem_write_wmb(irq_reg, req);
}

RUBY_INLINE u_int32_t
qtn_mproc_sync_irq_ack_nolock(u_int32_t irq_reg, u_int32_t mask)
{
	u_int32_t status = qtn_mproc_sync_mem_read(irq_reg) & (mask << 16);
	u_int32_t ret = (status >> 16);
	if (likely(ret)) {
		qtn_mproc_sync_mem_write_wmb(irq_reg, status & 0xFFFF0000);
	}
	return ret;
}

RUBY_INLINE u_int32_t
qtn_mproc_sync_irq_ack(u_int32_t irq_reg, u_int32_t mask)
{
	return qtn_mproc_sync_irq_ack_nolock(irq_reg, mask);
}

RUBY_INLINE u_int32_t
qtn_mproc_sync_irq_ack_all(u_int32_t irq_reg)
{
	return qtn_mproc_sync_irq_ack(irq_reg, 0xFFFFFFFF);
}

#endif // #ifndef __ASSEMBLY__

#endif // #ifndef __QTN_MPROC_SYNC_H

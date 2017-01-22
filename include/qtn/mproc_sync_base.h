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

#ifndef __QTN_MPROC_SYNC_BASE_H
#define __QTN_MPROC_SYNC_BASE_H

#ifndef __ASSEMBLY__

#include "../common/ruby_mem.h"
#include "../common/topaz_platform.h"

/*
 * Functions from this module use local_irq_save()/local_irq_restore()
 * as synchronization primitives within CPU.
 * This works only for uniprocessor systems.
 * If we would ever use SMP inside our SoC, we must
 * switch to something like spin_lock_irqsave()/spin_lock_irqrestore().
 */
#if defined(MUC_BUILD) || defined(DSP_BUILD)
	#include "os/os_arch_arc.h"
	#define local_irq_save(_flags)		do { (_flags) = _save_disable_all(); } while(0)
	#define local_irq_restore(_flags)	do { _restore_enable((_flags)); } while(0)
#elif defined(AUC_BUILD)
	/* AuC now has no user-defined ISR. No need to synchronize. */
	#define local_irq_save(_flags)		do { (void)_flags; } while(0)
	#define local_irq_restore(_flags)	do { (void)_flags; } while(0)
#else
	/* Linux target. Functions defined here already. */
#endif // #if defined(MUC_BUILD) || defined(DSP_BUILD)


RUBY_INLINE void
qtn_mproc_sync_log(const char *msg)
{
#if defined(MUC_BUILD)
	extern int uc_printk(const char *fmt, ...);
	uc_printk("MuC: %s\n", msg);
#elif defined(DSP_BUILD)
	#ifdef DSP_DEBUG
	extern void dsp_serial_puts(const char *str);
	dsp_serial_puts("DSP: ");
	dsp_serial_puts(msg);
	dsp_serial_puts("\n");
	#endif
#elif defined(AUC_BUILD)
	extern int auc_os_printf(const char *fmt, ...);
	auc_os_printf("AuC: %s\n", msg);
#elif defined(ARCSHELL)
#elif defined(__KERNEL__) && !defined(UBOOT_BUILD)
	/* Linux target */
	printk(KERN_INFO"LHOST: %s : %s : %s\n", KBUILD_MODNAME, KBUILD_BASENAME, msg);
#else
	printf("LHOST: %s\n", msg);
#endif // #if defined(MUC_BUILD)
}

RUBY_INLINE void*
qtn_mproc_sync_nocache(void *ptr)
{
#if defined(MUC_BUILD)
	return muc_to_nocache(ptr);
#else
	return ptr;
#endif
}

RUBY_INLINE void
qtn_mproc_sync_mem_write_16(u_int32_t addr, u_int16_t val)
{
	/*
	 * Rely on fact that this operation is atomic,
	 * that single bus transaction handles write.
	 */
	*((volatile u_int16_t*)addr) = val;
}

RUBY_INLINE u_int16_t
qtn_mproc_sync_mem_read_16(u_int32_t addr)
{
	/*
	 * Rely on fact that this operation is atomic,
	 * that single bus transaction handles read.
	 */
	return *((volatile u_int16_t*)addr);
}

RUBY_INLINE void
qtn_mproc_sync_mem_write(u_int32_t addr, u_int32_t val)
{
	/*
	 * Rely on fact that this operation is atomic,
	 * that single bus transaction handles write.
	 */
	*((volatile u_int32_t*)addr) = val;
}

RUBY_INLINE u_int32_t
qtn_mproc_sync_mem_read(u_int32_t addr)
{
	/*
	 * Rely on fact that this operation is atomic,
	 * that single bus transaction handles read.
	 */
	return *((volatile u_int32_t*)addr);
}

RUBY_INLINE u_int32_t
qtn_mproc_sync_mem_write_wmb(u_int32_t addr, u_int32_t val)
{
	qtn_mproc_sync_mem_write(addr, val);
	return qtn_addr_wmb(addr);
}

RUBY_INLINE u_int32_t
qtn_mproc_sync_addr(volatile void *addr)
{
	return (u_int32_t)addr;
}

#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
	RUBY_INLINE struct shared_params*
	qtn_mproc_sync_shared_params_get(void)
	{
		return (struct shared_params*)qtn_mproc_sync_nocache((void*)
			qtn_mproc_sync_mem_read(RUBY_SYS_CTL_SPARE));
	}
#else
	extern struct shared_params *soc_shared_params;

	RUBY_INLINE struct shared_params*
	qtn_mproc_sync_shared_params_get(void)
	{
		return soc_shared_params;
	}

	/* Has to be used by Linux only */
	RUBY_INLINE void
	qtn_mproc_sync_shared_params_set(struct shared_params *params)
	{
		qtn_mproc_sync_mem_write_wmb(RUBY_SYS_CTL_SPARE, (u_int32_t)params);
	}
#endif // #if defined(MUC_BUILD) || defined(DSP_BUILD)

#endif // #ifndef __ASSEMBLY__

#endif // #ifndef __QTN_MPROC_SYNC_BASE_H



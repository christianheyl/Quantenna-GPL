/**
 * Copyright (c) 2011 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#ifndef __BOARD_RUBY_MEM_CHECK_H
#define __BOARD_RUBY_MEM_CHECK_H

#include <linux/module.h>
#include <linux/magic.h>
#include <linux/sched.h>

static inline int is_linux_ddr_mem_addr(uint32_t addr)
{
	const uint32_t linux_dram_start = CONFIG_ARC_KERNEL_MEM_BASE + RUBY_DRAM_BEGIN;
	const uint32_t linux_dram_end = linux_dram_start + CONFIG_ARC_KERNEL_MAX_SIZE;
	return ((addr >= linux_dram_start) && (addr < linux_dram_end));
}

static inline int is_linux_sram_mem_addr(uint32_t addr)
{
	const uint32_t linux_sram_b1_start = CONFIG_ARC_KERNEL_SRAM_B1_BASE + RUBY_SRAM_BEGIN;
	const uint32_t linux_sram_b1_end = CONFIG_ARC_KERNEL_SRAM_B1_END + RUBY_SRAM_BEGIN;
	const uint32_t linux_sram_b2_start = CONFIG_ARC_KERNEL_SRAM_B2_BASE + RUBY_SRAM_BEGIN;
	const uint32_t linux_sram_b2_end = CONFIG_ARC_KERNEL_SRAM_B2_END + RUBY_SRAM_BEGIN;
	if ((addr >= linux_sram_b1_start) && (addr < linux_sram_b1_end)) {
		return 1;
	} else if ((addr >= linux_sram_b2_start) && (addr < linux_sram_b2_end)) {
		return 1;
	} else {
		return 0;
	}
}

static inline int is_linux_mem_addr(uint32_t addr)
{
	if (is_linux_ddr_mem_addr(addr)) {
		return 1;
	} else if (is_linux_sram_mem_addr(addr)) {
		return 1;
	} else {
		return 0;
	}
}

static inline int is_sram_irq_stack_good(void)
{
#ifdef CONFIG_ARCH_RUBY_SRAM_IRQ_STACK
	extern unsigned long __irq_stack_begin;
	if (unlikely(__irq_stack_begin != STACK_END_MAGIC)) {
		return 0;
	}
#endif
	return 1;
}

static inline int is_kernel_stack_good(void)
{
	struct task_struct *task = current;
	if (likely(task)) {
		unsigned long *stack = end_of_stack(task);
		if (unlikely(!is_linux_mem_addr((uint32_t)stack) ||
				(*stack != STACK_END_MAGIC))) {
			return 0;
		}
	}
	return 1;
}

static inline void check_stack_consistency_panic(const char *s1, const char *s2)
{
	register unsigned long sp asm ("sp");
	register unsigned long ilink1 asm ("ilink1");
	dump_stack();
	printk(KERN_ERR"ilink1=0x%lx sp=0x%lx\n", ilink1, sp);
	panic("%s overran stack, or stack corrupted: %s\n", s1, s2);
}

static inline void check_stack_consistency(const char *func)
{
	if (unlikely(!is_sram_irq_stack_good())) {
		check_stack_consistency_panic("IRQ", func);
	} else if (unlikely(!is_kernel_stack_good())) {
		check_stack_consistency_panic("Thread", func);
	}
}

#endif // #ifndef __BOARD_RUBY_MEM_CHECK_H


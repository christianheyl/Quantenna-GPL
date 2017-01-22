/**
 * Copyright (c) 2010 Quantenna Communications, Inc.
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

#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <asm/board/platform.h>

#ifdef TOPAZ_AMBER_IP
#include <qtn/amber.h>
#endif

static inline void machine_restart_watchdog(unsigned long delay)
{
	writel(RUBY_WDT_ENABLE, RUBY_WDT_CTL);
	writel(delay, RUBY_WDT_TIMEOUT_RANGE);
	writel(RUBY_WDT_MAGIC_NUMBER, RUBY_WDT_COUNTER_RESTART);
}

static inline void machine_restart_sysctrl(unsigned long mask)
{
	writel(mask, RUBY_SYS_CTL_CPU_VEC_MASK);
	writel(0x0, RUBY_SYS_CTL_CPU_VEC);
	writel(0x0, RUBY_SYS_CTL_CPU_VEC_MASK);
}

void machine_restart(char *__unused)
{
#ifndef TOPAZ_AMBER_IP
	/* Be paranoid - use both watchdog and sysctl to reset */
	machine_restart_watchdog(RUBY_WDT_RESET_TIMEOUT);
	machine_restart_sysctrl(RUBY_SYS_CTL_RESET_ALL);

	/* Board must be resetted before this point! */
	while(1);
#else
	machine_halt();
#endif
}

void machine_halt(void)
{
#ifdef TOPAZ_AMPER_IP
	local_irq_disable();
	/* Tell ST HOST we have been halted */
	amber_shutdown();
#endif
	/* Halt the processor */
	__asm__ __volatile__("flag  %0"::"i"(STATUS_H_MASK));
}

void machine_power_off(void)
{
	/* FIXME ::  power off ??? */
	machine_halt();
}


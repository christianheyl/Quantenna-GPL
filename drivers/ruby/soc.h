/*
 * (C) Copyright 2010 Quantenna Communications Inc.
 *
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


#ifndef __BOARD_RUBY_SOC_H
#define __BOARD_RUBY_SOC_H

#include <common/topaz_platform.h>
#include <asm/hardware.h>
#include <linux/init.h>

int soc_id(void);
u32 chip_id(void);

const char *get_board_id(void);
const unsigned char* get_ethernet_addr(void);
extern int global_disable_wd;
extern unsigned global_auc_config;

/* Following macro sets the watchdog timer for 5 Seconds */
static inline void init_watchdog_timer(void)
{
	if (!global_disable_wd) {
		writel(0xC, IO_ADDRESS(RUBY_WDT_TIMEOUT_RANGE));
		writel((RUBY_WDT_ENABLE_IRQ_WARN | RUBY_WDT_ENABLE), IO_ADDRESS(RUBY_WDT_CTL));
		writel(RUBY_WDT_MAGIC_NUMBER, IO_ADDRESS(RUBY_WDT_COUNTER_RESTART));
	}
}
static inline void pet_watchdog_timer(void)
{
	if (!global_disable_wd) {
		writel(RUBY_WDT_MAGIC_NUMBER, IO_ADDRESS(RUBY_WDT_COUNTER_RESTART));
	}
}

#endif // #ifndef __BOARD_RUBY_SOC_H


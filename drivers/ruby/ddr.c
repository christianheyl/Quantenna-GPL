/*
 * Copyright (c) Quantenna Communications Incorporated 2012.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/pm_qos_params.h>

#include <asm/board/platform.h>
#include <asm/hardware.h>

#include <common/ruby_pm.h>

static void ruby_ddr_powerdown_enable(int enable)
{
	printk(KERN_INFO"ddr sleep %sabled\n", enable ? "en" : "dis");

	if (enable) {
		writel(readl(RUBY_DDR_CONTROL) | RUBY_DDR_CONTROL_POWERDOWN_EN,
			RUBY_DDR_CONTROL);
	} else {
		writel(readl(RUBY_DDR_CONTROL) & ~RUBY_DDR_CONTROL_POWERDOWN_EN,
			RUBY_DDR_CONTROL);
	}
}

static int ruby_ddr_pm_notify(struct notifier_block *b, unsigned long level, void *v)
{
	static unsigned long pm_prev_level = BOARD_PM_LEVEL_NO;
	const unsigned long threshold = BOARD_PM_LEVEL_SLOW_DOWN;

	if ((pm_prev_level < threshold) && (level >= threshold)) {
		ruby_ddr_powerdown_enable(1);
	} else if ((pm_prev_level >= threshold) && (level < threshold)) {
		ruby_ddr_powerdown_enable(0);
	}

	pm_prev_level = level;

	return NOTIFY_OK;
}

static struct notifier_block pm_notifier = {
	.notifier_call = ruby_ddr_pm_notify,
};

static int __init ruby_ddr_init(void)
{
	pm_qos_add_notifier(PM_QOS_POWER_SAVE, &pm_notifier);
	return 0;
}
arch_initcall(ruby_ddr_init);

MODULE_DESCRIPTION("Ruby DDR");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");

/*
 * (C) Copyright 2010 Quantenna Communications Inc.
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

#include <linux/module.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/err.h>
#include <asm/hardware.h>
#include <asm/board/platform.h>

struct clk {
	unsigned long rate;	/* Clock rate in HZ */
};

struct qtn_clk_lookup_entry {
	const char *name;
	struct clk *clk_p;
};

static struct clk qtn_clk_bus = {
	.rate	= 0,
};

static struct qtn_clk_lookup_entry qtn_clk_list[] = {
	{ .name = "qtn-i2c.0", .clk_p = &qtn_clk_bus },
	{ .name = "bus-clk", .clk_p = &qtn_clk_bus },
};

static unsigned long qtn_clk_get_bus_freq(void)
{
	unsigned long freq = 0;

#ifdef CONFIG_ARC700_FPGA
	freq = CONFIG_ARC700_FPGA_CLK;
#else
	switch(readl(IO_ADDRESS(RUBY_SYS_CTL_CTRL)) & RUBY_SYS_CTL_MASK_CLKSEL) {
	case RUBY_SYS_CTL_CLKSEL(0x0):
		freq = RUBY_SYS_CTL_CLKSEL_00_BUS_FREQ;
		break;
	case RUBY_SYS_CTL_CLKSEL(0x1):
		freq = RUBY_SYS_CTL_CLKSEL_01_BUS_FREQ;
		break;
	case RUBY_SYS_CTL_CLKSEL(0x2):
		freq = RUBY_SYS_CTL_CLKSEL_10_BUS_FREQ;
		break;
	case RUBY_SYS_CTL_CLKSEL(0x3):
		freq = RUBY_SYS_CTL_CLKSEL_11_BUS_FREQ;
		break;
	default:
		panic("Logic error!\n");
		break;
	}
#endif
	return freq;
}

static int __init qtn_clk_init_rates(void)
{
	/* Clock source bits are latched on power-on */
	qtn_clk_bus.rate = qtn_clk_get_bus_freq();
	return 0;
}

arch_initcall(qtn_clk_init_rates);


struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *ret_clk = NULL;
	const char *dev_id = NULL;
	int i;

	if (dev) {
		dev_id = dev_name(dev);

		for (i = 0; i < ARRAY_SIZE(qtn_clk_list); ++i) {
			if (!strcmp(dev_id, qtn_clk_list[i].name)) {
				ret_clk = qtn_clk_list[i].clk_p;
				break;
			}
		}
	}

	return ret_clk ? ret_clk : ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL(clk_get);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk ? clk->rate : 0;
}
EXPORT_SYMBOL(clk_get_rate);

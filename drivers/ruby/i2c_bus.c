/**
 * Copyright (c) 2008-2014 Quantenna Communications, Inc.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/board/platform.h>

static struct resource qtn_i2c_resources[] = {
	{
		.start	= RUBY_I2C_BASE_ADDR,
		.end	= RUBY_I2C_BASE_ADDR + RUBY_I2C_MEM_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start = RUBY_IRQ_MISC_I2C,
		.end   = RUBY_IRQ_MISC_I2C,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device qtn_i2c_device = {
	.name = "qtn-i2c",
	.id = RUBY_I2C_ADAPTER_NUM,
	.resource = qtn_i2c_resources,
	.num_resources = ARRAY_SIZE(qtn_i2c_resources),
};

static int __init qtn_i2c_setup_bus(void)
{
	pr_debug("Quantenna I2C device register\n");
	return platform_device_register(&qtn_i2c_device);
}

arch_initcall(qtn_i2c_setup_bus);

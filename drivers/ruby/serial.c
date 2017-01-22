/*
 * (C) Copyright 2010 Quantenna Communications Inc.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/serial_8250.h>

#include <asm/board/platform.h>
#include <asm/board/gpio.h>
#include <asm/board/board_config.h>

static struct plat_serial8250_port ruby_data[] =
{
	{
		.iotype		= UPIO_DWAPB,
		.private_data	= (void*)RUBY_UART0_USR,
		.flags		= (UPF_BOOT_AUTOCONF | UPF_SPD_FLAG),
		.mapbase	= RUBY_UART0_BASE_ADDR,
		.membase	= (void*)RUBY_UART0_BASE_ADDR,
		.irq		= RUBY_IRQ_UART0,
		.uartclk	= CONFIG_ARC700_DEV_CLK,
		.regshift	= 2,
	},
	{
		.iotype		= UPIO_DWAPB,
		.private_data	= (void*)RUBY_UART1_USR,
		.flags		= (UPF_BOOT_AUTOCONF | UPF_SPD_FLAG),
		.mapbase	= RUBY_UART1_BASE_ADDR,
		.membase	= (void*)RUBY_UART1_BASE_ADDR,
		.irq		= RUBY_IRQ_UART1,
		.uartclk	= CONFIG_ARC700_DEV_CLK,
		.regshift	= 2,
	},
	{}
};

static struct platform_device ruby_uart =
{
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= ruby_data,
	},
};

static int __init setup_console(void)
{
	int use_uart1 = 0;
	int ret;
	
	if (get_board_config(BOARD_CFG_UART1, &use_uart1) != 0) {
		printk(KERN_ERR "get_board_config returned error status for UART1\n");
	}

	/* if uart1 is not requested, remove it from the device list so ttyS1 doesn't appear */
	if (!use_uart1) {
		memset(&ruby_data[1], 0, sizeof(struct plat_serial8250_port));
	}

	/* register device */
	ret = platform_device_register(&ruby_uart);

	/* configure GPIOs */
	if(!ret) {
		gpio_uart0_config();
		if (use_uart1) {
			gpio_uart1_config();
		}
	}

	return ret;
}
arch_initcall(setup_console);


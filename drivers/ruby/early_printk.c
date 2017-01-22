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

#include <linux/console.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <asm/serial.h>
#include <asm/hardware.h>

#include <asm/board/platform.h>
#include <asm/board/gpio.h>

inline static int early_serial_tx_ready(void)
{
	return (readb(IO_ADDRESS(RUBY_UART0_LSR)) & RUBY_LSR_TX_Empty);
}

inline static void early_serial_wait_tx(void)
{
	while (!early_serial_tx_ready());
}

inline static void early_serial_putc_simple(const char c)
{
	early_serial_wait_tx();
	writeb(c, IO_ADDRESS(RUBY_UART0_RBR_THR_DLL));
}

inline static void early_serial_putc(const char c)
{
	if (c == '\n') {
		early_serial_putc_simple('\r');
	}
	early_serial_putc_simple(c);
}

inline static void early_serial_setbrg(void)
{
	u32 baud_val = (BASE_BAUD * 16);
	u32 div_val = (CONFIG_ARC700_DEV_CLK + baud_val / 2) / baud_val;
	u8 lcr_val =
		RUBY_LCR_Data_Word_Length_8 |
		RUBY_LCR_Stop_Bit_1 |
		RUBY_LCR_No_Parity |
		RUBY_LCR_Break_Disable;

	if (!(readb(IO_ADDRESS(RUBY_UART0_USR)) & RUBY_USR_Busy)) {
		writeb(lcr_val | RUBY_LCR_DLAB, IO_ADDRESS(RUBY_UART0_LCR));
		writeb((div_val & 0xff), IO_ADDRESS(RUBY_UART0_RBR_THR_DLL));
		writeb((div_val >> 8) & 0xff, IO_ADDRESS(RUBY_UART0_DLH_IER));
		writeb(lcr_val, IO_ADDRESS(RUBY_UART0_LCR));
	}
}

static void early_console_write(struct console *co, const char *s, unsigned count)
{
	while(count--) {
		early_serial_putc(*s);
		++s;
	}
}

static int __init early_console_setup(struct console *co, char *options)
{
	/* Options are ignored.
	* User hard-coded in early_serial_setbrg() mode.
	*/
	early_serial_setbrg();
	return 0;
}

static struct console __initdata early_console_struct = {
	.name  = "ruby_early",
	.write = early_console_write,
	.setup = early_console_setup,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.index = -1.
};

static struct console *early_console = &early_console_struct;

static int __init setup_early_printk(char *buf)
{
	gpio_uart0_config();
	register_console(early_console);
	return 0;
}
early_param("earlyprintk", setup_early_printk);


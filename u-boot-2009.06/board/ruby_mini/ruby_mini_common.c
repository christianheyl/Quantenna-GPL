/*
 * (C) Copyright 2013 Quantenna Communications Inc.
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

#include <common.h>
#include <config.h>
#include <command.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <asm/types.h>
#include <asm/cache.h>
#include <net.h>
#include <timestamp.h>
#include <asm/io.h>

#include "ruby.h"
#include "board_cfg.h"
#include <shared_defs_common.h>
#include "ruby_board_cfg.h"
#include "ruby_board_db.h"
#include "ruby_version.h"
#include "ruby_mini_common.h"

gd_t *gd;
gd_t *global_data;

int console_assign(int file, char *devname)
{
	return 0;
}

int console_init_f(void)
{
	gd->have_console = 0;
	return 0;
}

int getc(void)
{
	return 0;
}

void putc(char c)
{
#ifdef CONFIG_SHOW_BOOT_PROGRESS
	serial_putc(c);
#endif
}

void puts(const char *s)
{
#ifdef CONFIG_SHOW_BOOT_PROGRESS
	serial_puts(s);
#endif
}

void printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[256];

	va_start (args, fmt);

	/*
	 * For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf (printbuffer, fmt, args);
	va_end (args);

	/* Print the string */
	puts (printbuffer);
}

void vprintf(const char *fmt, va_list args)
{
	uint i;
	char printbuffer[256];

	/*
	 * For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf (printbuffer, fmt, args);

	/* Print the string */
	puts (printbuffer);
}

int ctrlc(void)
{
	return 0;
}

int had_ctrlc(void)
{
	return 0;
}

void print_size (phys_size_t s, const char * c) {}

unsigned long load_addr = RUBY_SRAM_BEGIN;

#if !defined(TOPAZ_EP_MINI_UBOOT) && !defined(TOPAZ_TINY_UBOOT)
/*
 * tftp environment variables (ipaddr, serverip, bootfile) are set dynamically
 * before attempting tftp. They are cleared before attempting bootp. Bootp will
 * not work if ipaddr is set, as the udp receive routines reject packets whose
 * destination address does not match ipaddr
 */
uchar __env[CONFIG_ENV_SIZE] = {
	"ethaddr="	MKSTR(CONFIG_ETHADDR)	"\0"
	"ipaddr="	MKSTR(CONFIG_IPADDR)	"\0"
	"serverip="	MKSTR(CONFIG_SERVERIP)	"\0"
	"bootfile="	MKSTR(CONFIG_BOOTFILE)	"\0"
	"\0"
};

static const board_cfg_t mini_board_conf = {
	.bc_emac0       = EMAC0_CONFIG,
	.bc_emac1       = EMAC1_CONFIG,
	.bc_phy0_addr   = EMAC0_PHY_ADDR,
	.bc_phy1_addr   = EMAC1_PHY_ADDR,
	.bc_rgmii_timing = 0x1f821f82,//EMAC_RGMII_TIMING,
};

uchar env_get_char (int index)
{
	return __env[index];
}

uchar *env_get_addr (int index)
{
	return &__env[index];
}

void *env_get_file_body(int fileoffset)
{
	return NULL; // there are no config files in minimal environment.
}


void env_crc_update(void) {}
#endif /* TOPAZ_EP_MINI_UBOOT */

void show_boot_progress(int status)
{
}

int cpu_init(void)
{
	/*
	 * disable interrupts. Smaller version with no
	 * return code compared to u-boot full
	 */
	unsigned int status;
	status = read_new_aux_reg(ARC_REG_STATUS32);
	status &= STATUS_DISABLE_INTERRUPTS;
	__asm__ __volatile__ ( "flag %0" : : "r"(status) );

	return 0;
}

static void board_global_data_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	gd->cpu_clk = RUBY_FIXED_CPU_CLK;
	gd->bus_clk = RUBY_FIXED_DEV_CLK;
	gd->baudrate = RUBY_SERIAL_BAUD;
	gd->bd->bi_boot_params = 0x0;
}

int board_init(void)
{
	/*
	 * Enable i-cache/d-cache immediately upon jump from start.S
	 */
	#ifndef TOPAZ_ICACHE_WORKAROUND
	icache_enable();
	#endif

	dcache_enable();
	board_global_data_init();
	board_serial_init();
	board_timer_init();
#if defined(TOPAZ_EP_MINI_UBOOT) || defined(TOPAZ_TINY_UBOOT)
	extern void board_spi_flash_init(void);
	board_spi_flash_init();
#endif
	return 0;
}

#if !defined(TOPAZ_EP_MINI_UBOOT)
int env_init(void)
{
#if !defined(TOPAZ_TINY_UBOOT)
	gd->env_addr  = (ulong)&__env[0];
	gd->env_valid = 1;
#endif
	return 0;
}
#endif

typedef int (init_fnc_t) (void);
static init_fnc_t *init_sequence[] = {
	cpu_init,		/* cpu specific initialisations*/
	board_init,
	serial_init,            /* serial communications setup */
	env_init,		/* intialise environment */
	console_init_f,         /* stage 1 init of console */
	NULL,
};

void ruby_mini_init(void)
{
	static gd_t gd_data;
	static bd_t bd_data;
	init_fnc_t **init_fnc_ptr;

	gd = global_data = &gd_data;
	gd->bd = &bd_data;
	gd->bd->bi_board_id = 0;

	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr)() != 0) {
			hang();
		}
	}
}

void __attribute__ ((noreturn)) hang(void)
{
	board_reset("hang called\n");
}

void cmd_usage(cmd_tbl_t *cmdtp)
{
}

#if !defined(TOPAZ_EP_MINI_UBOOT) && !defined(TOPAZ_TINY_UBOOT)
int board_config(int board_id, int parameter)
{
	int* ptr = (int *)&mini_board_conf;
	return ptr[parameter];
}
#endif


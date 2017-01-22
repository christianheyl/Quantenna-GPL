/*
 * (C) Copyright 2011 - 2013 Quantenna Communications Inc.
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

#include "ruby.h"
#include "board_cfg.h"
#include <shared_defs_common.h>
#include "ruby_board_cfg.h"
#include "ruby_board_db.h"
#include "ruby_version.h"
#include "ruby_mini_common.h"
#include "rtl8367b/rtl8367b_init.h"

void *ar8236_init(unsigned long baseAddr, unsigned long phy_addr)
{
	return NULL;
}

void *ar8237_init(unsigned long baseAddr, unsigned long phy_addr)
{
	return NULL;
}

ret_t rtl8367b_poll_linkup(struct emac_private *priv)
{
	return 0;
}

ret_t rtl8367b_init(struct emac_private *priv, uint32_t emac_cfg)
{
	return 0;
}

void do_bootm(void){}

static void get_stage2(void)
{
	uint32_t method_used;
	int ret = get_stage2_image(get_early_flash_config(TEXT_BASE)->method, &method_used);

	if (ret == 0) {
		void (*jump_stage2)(void) = (void*)load_addr;
		struct early_flash_config *stage2ef = get_early_flash_config(load_addr);

		/* provide a boot method, ip and server to 2nd stage u-boot */
		stage2ef->method = method_used;
		stage2ef->ipaddr = NetOurIP;
		stage2ef->serverip = NetServerIP;

		eth_halt();

		flush_and_inv_dcache_all();
		invalidate_icache_all();

		jump_stage2();
	}
}


void start_arcboot(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	ruby_mini_init();

	printf("Quantenna Mini U-Boot\n");
	printf("Version: %s Built: %s at %s\n",
			RUBY_UBOOT_VERSION, U_BOOT_DATE, U_BOOT_TIME);

#if defined(CONFIG_CMD_NET)
	/*IP Address */
	gd->bd->bi_ip_addr = getenv_IPaddr ("ipaddr");
	/* MAC Address */
	{
		int i;
		ulong reg;
		char *s, *e;
		char tmp[64];
		i = getenv_r ("ethaddr", tmp, sizeof(tmp));
		s = (i > 0) ? tmp : NULL;
		for (reg = 0; reg < 6; ++reg) {
			gd->bd->bi_enetaddr[reg] = s ? simple_strtoul (s, &e, 16) : 0;
			if (s) {
				s = (*e) ? e + 1 : e;
			}
		}
	}
	/* Boot file */
	{
		const char *s;
		if ((s = getenv("bootfile")) != NULL) {
			copy_filename(BootFile, s, sizeof(BootFile));
		}
	}
#endif

#ifdef CONFIG_CMD_NET
	eth_initialize(gd->bd);
#endif
	qtn_parse_early_flash_config(1);

	get_stage2();

	board_reset("get_stage2 returned");
}



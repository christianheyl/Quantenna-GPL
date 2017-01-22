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

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <qtn/shared_defs.h>
#include <asm/board/board_config.h>
#include <common/ruby_board_db.h>
#include <common/ruby_partitions.h>

static int			global_board_id = -1;
static int			global_spi_protect_mode = 0;
static int			g_slow_ethernet = 0;
static const board_cfg_t	g_board_cfg_table[] = QTN_BOARD_DB;
static ruby_bda_t bda_copy;

#define MEM_SIZE_128MB (128 * 1024 * 1024)

static int __init
setup_slow_ethernet(char *buf)
{
	g_slow_ethernet = (buf != NULL);
	return 0;
}
early_param("slow_ethernet", setup_slow_ethernet);

static void __init
update_ddr_size(void)
{
	/* update DDR size */
	extern unsigned long end_mem;
	int ddr_size = 0;
	if (get_board_config(BOARD_CFG_DDR_SIZE, &ddr_size) < 0) {
		printk_init("UNKNOWN DDR SIZE !!!\n");
	} else {
#if !TOPAZ_MMAP_UNIFIED
		if (ddr_size > MEM_SIZE_128MB)
			ddr_size = MEM_SIZE_128MB;
#endif
		end_mem = RUBY_DRAM_BEGIN + ddr_size;
	}
}

void __init
qtn_set_hw_config_id(int id)
{
	global_board_id = id;
	update_ddr_size();
}

int qtn_get_hw_config_id(void)
{
	return global_board_id;
}
EXPORT_SYMBOL(qtn_get_hw_config_id);

void __init
qtn_set_spi_protect_config(u32 mode)
{
	global_spi_protect_mode = mode;
}

int qtn_get_spi_protect_config(void)
{
	return global_spi_protect_mode;
}
EXPORT_SYMBOL(qtn_get_spi_protect_config);

static const ruby_bda_t *get_bda(void)
{
	return &bda_copy;
}

static int
lookup_config( const board_cfg_t *p_board_config_entry, int board_config_param, int *p_board_config_value )
{
	int	retval = 0;

	switch (board_config_param)
	{
	  case BOARD_CFG_WIFI_HW:
		*p_board_config_value = p_board_config_entry->bc_wifi_hw;
		break;

	  case BOARD_CFG_EMAC0:
		*p_board_config_value = p_board_config_entry->bc_emac0;
		break;

	  case BOARD_CFG_EMAC1:
		*p_board_config_value = p_board_config_entry->bc_emac1;
		break;

	  case BOARD_CFG_PHY1_ADDR:
		*p_board_config_value = p_board_config_entry->bc_phy1_addr;
		break;

	  case BOARD_CFG_PHY0_ADDR:
		*p_board_config_value = p_board_config_entry->bc_phy0_addr;
		break;

	  case BOARD_CFG_UART1:
		*p_board_config_value = p_board_config_entry->bc_uart1;
		break;

	  case BOARD_CFG_PCIE:
		*p_board_config_value = p_board_config_entry->bc_pcie;
		break;

	  case BOARD_CFG_RGMII_TIMING:
		*p_board_config_value = p_board_config_entry->bc_rgmii_timing;
		break;

	  case BOARD_CFG_DDR_SIZE:
		*p_board_config_value = p_board_config_entry->bc_ddr_size;
		break;

	  case BOARD_CFG_FLASH_SIZE:
		{
			if (global_board_id == QTN_RUBY_AUTOCONFIG_ID ||
					global_board_id == QTN_RUBY_UNIVERSAL_BOARD_ID) {
				*p_board_config_value = get_bda()->bda_flashsz;
			} else {
				*p_board_config_value = 0;
			}
		}
		break;
	  default:
		retval = -1;
	}

	return( retval );
}

int
get_board_config( int board_config_param, int *p_board_config_value )
{
	int	iter;
	int	retval = -1;
	int	found_entry = 0;

	static int	get_board_config_error_is_reported = 0;

	if (p_board_config_value == NULL) {
		return( -1 );
	}

	if (global_board_id == QTN_RUBY_AUTOCONFIG_ID ||
			global_board_id == QTN_RUBY_UNIVERSAL_BOARD_ID) {
		return lookup_config(&get_bda()->bda_boardcfg, board_config_param,
					p_board_config_value);
	}
	//printk("get_board_config:board ID %d param %d\n",global_board_id,board_config_param);
	for (iter = 0; iter < sizeof( g_board_cfg_table ) / sizeof( g_board_cfg_table[ 0 ] ) && (found_entry == 0); iter++) {
		if (global_board_id == g_board_cfg_table[ iter ].bc_board_id) {
			found_entry = 1;
			retval = lookup_config( &g_board_cfg_table[ iter ], board_config_param, p_board_config_value );
		}
	}
	/*
	 * Default to the first entry in the table if not found.
	 * Likely will not work for customer boards, but Q bringup baords might be OK.
	 */
	if (found_entry == 0) {
		retval = lookup_config( &g_board_cfg_table[ 0 ], board_config_param, p_board_config_value );
		if (get_board_config_error_is_reported == 0) {
			printk(KERN_ERR "get board config: HW config ID %d not recognized, defaulting to %d\n",
					 global_board_id, g_board_cfg_table[ 0 ].bc_board_id);
			get_board_config_error_is_reported = 1;
		}
	}

	return( retval );
}
EXPORT_SYMBOL(get_board_config);

int
board_slow_ethernet(void)
{
	int emac0_cfg = 0, emac1_cfg = 0;

	if (g_slow_ethernet) {
			return 1;
	}

#ifdef DETECT_SLOW_ETHERNET
	get_board_config(BOARD_CFG_EMAC0, &emac0_cfg);
	get_board_config(BOARD_CFG_EMAC1, &emac1_cfg);
#endif

	return ( (emac0_cfg | emac1_cfg) & EMAC_SLOW_PHY);
}
EXPORT_SYMBOL(board_slow_ethernet);

int
board_napi_budget(void)
{
	return 128;
}
EXPORT_SYMBOL(board_napi_budget);

int
get_all_board_params(char *p)
{
	int iter;
	int ch_printed;
	const board_cfg_t *board = NULL;

	if (global_board_id == QTN_RUBY_AUTOCONFIG_ID ||
			global_board_id == QTN_RUBY_UNIVERSAL_BOARD_ID) {
		board = &get_bda()->bda_boardcfg;
	} else {
		for (iter = 0; iter < ARRAY_SIZE(g_board_cfg_table); iter++) {
			if (global_board_id == g_board_cfg_table[iter].bc_board_id) {
				board = &g_board_cfg_table[iter];
				break;
			}
		}
	}

	if (!board)
		return 0;

	ch_printed = snprintf(p, PAGE_SIZE,
				"board_id\t%d\n"
				"name\t%s\n"
				"ddr_type\t%d\n"
				"ddr_speed\t%d\n"
				"ddr_size\t%d\n"
				"emac0\t%d\n"
				"emac1\t%d\n"
				"phy0_addr\t%d\n"
				"phy1_addr\t%d\n"
				"spi1\t%d\n"
				"wifi_hw\t%d\n"
				"uart1\t%d\n"
				"bd_pcie\t%d\n"
				"rgmii_timing\t%d\n",
				board->bc_board_id, board->bc_name,
				board->bc_ddr_type, board->bc_ddr_speed, board->bc_ddr_size,
				board->bc_emac0, board->bc_emac1, board->bc_phy0_addr,
				board->bc_phy1_addr, board->bc_spi1, board->bc_wifi_hw,
				board->bc_uart1, board->bc_pcie, board->bc_rgmii_timing);
	if (ch_printed < 0)
		ch_printed = 0;

	return ch_printed;
}
EXPORT_SYMBOL(get_all_board_params);

void __init
parse_board_config(char *cmdline)
{
	/* parse command line */
	int cmdline_len = strlen(cmdline);
	const char *var = "hw_config_id=";
	int var_len = strlen(var);
	while (cmdline_len > var_len) {
		if (!strncmp(cmdline, var, var_len)) {
			sscanf(cmdline + var_len, "%d", &global_board_id);
			printk_init("Board id: %d\n", global_board_id);
			break;
		}
		++cmdline;
		--cmdline_len;
	}

	/* we should already know board id */
	if (global_board_id < 0) {
		printk_init("UNKNOWN BOARD ID !!!\n");
	}

	/* update DDR size */
	update_ddr_size();

	/*
	 * Copy bda structure so that it doesn't get overwritten by an A-MSDU with
	 * a size greater than CONFIG_ARC_CONF_SIZE
	 */
	memcpy(&bda_copy, (void *)CONFIG_ARC_CONF_BASE, sizeof(bda_copy));
	bda_copy.bda_boardcfg.bc_name = bda_copy.bda_boardname;
}


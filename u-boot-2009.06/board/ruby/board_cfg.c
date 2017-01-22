/*
 * Copyright (c) 2010 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  file purpose
 *    Map board_id to coard configuration definitions
 */

////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////
#include "ruby.h"
#include "board_cfg.h"
#include "ddr.h"
#include "malloc.h"
#include <shared_defs_common.h>

/* Enable auto configured board ID based on uboot environment vars */
#define UBOOT_BOARD_AUTOCONFIG

#include "ruby_board_cfg.h"
#include "ruby_board_db.h"
#include "spi_flash.h"

#define FLASH_SIZE_INTERPRET_MB_THRESHOLD	1024

static const board_cfg_t	g_board_cfg_table[] = QTN_BOARD_DB;
static board_cfg_t		*custom_board = NULL;
extern uint32_t			flash_size;
static char			board_name[RUBY_BDA_NAMELEN];

const char * const board_cfg_field_names[] = BOARD_CFG_FIELD_NAMES;

extern board_cfg_t board_hw_config;

extern int mdc_clk_divisor;

/***************************************************************************
   Function:
   Purpose:
   Returns:
   Note: Code assumes parameter is within range
 **************************************************************************/

const board_cfg_t *board_locate(int board_id)
{
	int i;

	if (board_id == QTN_RUBY_UNIVERSAL_BOARD_ID) {
		return &board_hw_config;
	}

	if (board_id == QTN_RUBY_AUTOCONFIG_ID) {
		return custom_board;
	}

	for (i = 0; i < ARRAY_SIZE(g_board_cfg_table); i++) {
		if (board_id == g_board_cfg_table[i].bc_board_id){
			return &g_board_cfg_table[i];
		}
	}

	return NULL;
}


int board_config(int board_id, int parameter)
{

	const int *ptr;
	const board_cfg_t *board = board_locate(board_id);

	if (board) {
		ptr = (int *)board;
		return ptr[parameter];
	}

	return -1;
}

static int board_parse_tag_value(const struct ruby_cfgstr_map *map, const char *token, int *val)
{
	while (map && map->name) {
		if (!strcmp(map->name, token)) {
			*val = map->val;
			return 0;
		}
		map++;
	}

	return -1;
}

#ifndef TOPAZ_EP_MINI_UBOOT
static const char *board_find_tag_name(const struct ruby_board_param *params, uint32_t index, int val)
{
	while (params) {
		if (params->p_index == index) {
			const struct ruby_cfgstr_map *map = params->p_map;
			while (map) {
				if (map->val == val) {
					return map->name;
				}
				map++;
			}
			return NULL;
		}
		params++;
	}
	return NULL;
}
#endif /* TOPAZ_EP_MINI_UBOOT */

int board_parse_tag(const char *bc_param, const char *valstr, int *val)
{
	const struct ruby_board_param *params = g_custom_board_params;

	while (params && params->p_map) {
		if (strcmp(bc_param, board_cfg_field_names[params->p_index]) == 0 &&
				board_parse_tag_value(params->p_map, valstr, val) == 0) {
			return 0;
		}
		params++;
	}

	return -1;
}

int board_parse_custom_cfg(void)
{
	const struct ruby_board_param *params = g_custom_board_params;
	int *ptr;
	char *token = NULL;
	int val = 0;

	custom_board = &g_custom_board_cfg;
	ptr = (int *) custom_board;

	while (params && params->p_map) {
		val = 0;
		token = getenv(board_cfg_field_names[params->p_index]);
		if (!token) {
			continue;
		}

		if (params->p_index == BOARD_CFG_NAME) {
			uint32_t len = strlen(token);

			/* Copy the boardname string as we don't know if the buffer space from getenv() will be reused */
			memset(board_name, 0, RUBY_BDA_NAMELEN);
			memcpy(board_name, token, min(len, RUBY_BDA_NAMELEN - 1));
		} else if (!board_parse_tag_value(params->p_map, token, &val)) {
			if (params->p_index == BOARD_CFG_FLASH_SIZE) {
				flash_size = val;
			} else if (params->p_index < BOARD_CFG_STRUCT_NUM_FIELDS) {
				ptr[params->p_index] = val;
			}
		} else {
			printf("%s: token '%s' invalid\n", __FUNCTION__, token);
		}
		params++;
	}
	return 0;
}

void board_setup_bda(void *addr, int board_id)
{
	ruby_bda_t *bda = (ruby_bda_t *)addr;
	const board_cfg_t *board = board_locate(board_id);
	char *name = (board_id == QTN_RUBY_AUTOCONFIG_ID) ? board_name : board->bc_name;

	uint32_t len = strlen(name);

	bda->bda_len = sizeof(ruby_bda_t);
	bda->bda_version = RUBY_BDA_VERSION;

	printf("BDA at 0x%x\n", (int)addr);

	memcpy(&bda->bda_boardcfg, board, sizeof(board_cfg_t));
	memset(bda->bda_boardname, 0 , RUBY_BDA_NAMELEN);
	memcpy(bda->bda_boardname, name, (len > (RUBY_BDA_NAMELEN-1)) ? (RUBY_BDA_NAMELEN -1) : len);

	if (flash_size == 0) {
		bda->bda_flashsz = spi_flash_size();
	} else if (flash_size < FLASH_SIZE_INTERPRET_MB_THRESHOLD) {
		/*
		 * Older Universal hw_config_id implementations
		 * specify flash size in megabytes. Assume very small
		 * values were intended as megabytes
		 */
		bda->bda_flashsz = flash_size << 20;
	} else {
		bda->bda_flashsz = flash_size;
	}
}

#ifndef TOPAZ_EP_MINI_UBOOT
int do_list_board_options(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	const struct ruby_board_param *params = g_custom_board_params;

	printf("HW config Board options:\n");
	while (params && params->p_map) {
		const struct ruby_cfgstr_map *map = params->p_map;

		printf("%s=(", board_cfg_field_names[params->p_index]);
		while(map && map->name) {
			printf("%s,",  map->name);
			map++;
		}

		printf(")\n");
		params++;
	}
	return 0;

}

int do_list_board_cfg(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	const struct ruby_board_param *params = g_custom_board_params;
	const board_cfg_t *board = NULL;
	int board_id = 0;
	char *token = (argc == 2) ? argv[1] : getenv("hw_config_id");;

	if(!token) {
		printf("\"hw_config_id\" not set\n");
		return -1;
	}

	board_id = simple_strtoul (token, NULL, 10);
	if (board_id < 0) {
		printf("\"Invalid board: %s\n", token);
		return -1;
	}
	board = board_locate(board_id);

	if(!board) {
		printf("Board ID:%d not valid.\n", board_id);
		return -1;
	}
	if (argc == 2) {
		printf("Board config for ID:%d\n", board_id);
	} else {
		printf("Current board config:\n");
	}
	printf("\tID:\t%d\n", board->bc_board_id);
	printf("\tName:\t%s\n", board->bc_name);
	printf("\tDDR:\t%s %dMHz %dM\n",
		board_find_tag_name(params, BOARD_CFG_DDR_TYPE, board->bc_ddr_type), 
						board->bc_ddr_speed, board->bc_ddr_size >> 20);
	printf("\tEMAC0:\t%s\n", board_find_tag_name(params, BOARD_CFG_EMAC0, board->bc_emac0));	
	printf("\tEMAC1:\t%s\n", board_find_tag_name(params, BOARD_CFG_EMAC1, board->bc_emac1));
	printf("\tRFPA:\t%s\n", board_find_tag_name(params, BOARD_CFG_WIFI_HW, board->bc_wifi_hw));
	printf("\tRGMII:\t0x%x\n", board->bc_rgmii_timing);

	printf("\tSPI1:\t%s\n", board->bc_spi1 ? "Enabled" : "Disabled");
	printf("\tUART1:\t%s\n", board->bc_uart1 ? "Enabled" : "Disabled");
	printf("\tPCIe:\t%s\n", board->bc_pcie ? "Enabled" : "Disabled");

	return 0;

}

int do_list_board(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int i;
	printf("HW config  Board Name\n");
	printf("--------  ------------------------------\n");
	for (i=0;i<sizeof(g_board_cfg_table)/sizeof(board_cfg_t);i++) {
		printf("%8d  %s\n",g_board_cfg_table[i].bc_board_id,
			g_board_cfg_table[i].bc_name);
	}
	printf("\n");

	return 0;
}

U_BOOT_CMD(boardcfg, 2, 0, do_list_board_cfg,"list current board config",NULL);

U_BOOT_CMD(boardopts, 1, 0, do_list_board_options,"list board config options",NULL);

U_BOOT_CMD(lsid, 1, 0, do_list_board,"list board IDs",NULL);
#endif /* TOPAZ_EP_MINI_UBOOT */


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


#ifndef __BOARD_RUBY_TYPE_H
#define __BOARD_RUBY_TYPE_H

#include <common/ruby_board_cfg.h>

void __init qtn_set_hw_config_id(int id);
void __init qtn_set_spi_protect_config(u32 mode);

int get_board_config( int board_config_param, int *p_board_config_value );
int get_all_board_params( char * p );

int board_slow_ethernet(void);

int board_napi_budget(void);

void parse_board_config(char *cmdline);

void parse_spi_config(char *cmdline);

void parse_pcie_intr_config(char *cmdline);

#endif // #ifndef __BOARD_RUBY_TYPE_H

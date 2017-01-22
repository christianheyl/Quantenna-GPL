#ifndef __FILE__H__
#define __FILE__H__
/*
 * Copyright (c) 2010 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  Board configuration definitions that only apply to the boot loader.
 *
 *  Most definitions have been moved to common/ruby_config.h
 *
 */

int board_config(int board_id, int parameter);
int board_parse_custom_cfg(void);
void board_setup_bda(void *addr, int board_id);
int board_parse_tag(const char *bc_param, const char *valstr, int *val);

#endif // __BOARD_CFG_H__


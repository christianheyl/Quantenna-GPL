/*
 * (C) Copyright 2013 Quantenna Communications Inc.
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

#ifndef __BOARD_QTN_TROUBLESHOOT_H
#define __BOARD_QTN_TROUBLESHOOT_H

#include <linux/types.h>
#include <common/topaz_platform.h>

#define HEADER_CORE_DUMP	(0xDEAD)

typedef int (*arc_troubleshoot_start_hook_cbk)(void *in_ctx);

void arc_set_sram_safe_area(unsigned long sram_start, unsigned long sram_end);

void arc_save_to_sram_safe_area(int compress_ratio);

void arc_set_troubleshoot_start_hook(arc_troubleshoot_start_hook_cbk in_troubleshoot_start, void *in_ctx);

/* Inside printk.c - not the ideal header for this, but since it's Quantenna added, is
 * OK in here.
 */
void *get_log_buf(int *, int *, char **);

#endif // #ifndef __BOARD_QTN_TROUBLESHOOT_H


/**
  Copyright (c) 2014 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef _QDRV_SHOW_H
#define _QDRV_SHOW_H

#include <linux/seq_file.h>
#include "qdrv_mac.h"

enum qdrv_show_assoc_group {
	QDRV_SHOW_ASSOC_STATE,
	QDRV_SHOW_ASSOC_VER,
	QDRV_SHOW_ASSOC_PHY,
	QDRV_SHOW_ASSOC_PHY_ALL,
	QDRV_SHOW_ASSOC_TX,
	QDRV_SHOW_ASSOC_RX,
	QDRV_SHOW_ASSOC_ALL
};

/* arguments for "show_assoc" command */
struct qdrv_show_assoc_params {
	struct qdrv_mac *mac;
	enum qdrv_show_assoc_group show_group;
	uint8_t filter_macaddr[IEEE80211_ADDR_LEN];
	uint16_t filter_idx;
};

void qdrv_show_assoc_init_params(struct qdrv_show_assoc_params *params, struct qdrv_mac *mac);
int qdrv_show_assoc_parse_params(struct qdrv_show_assoc_params *params, int argc, char *argv[]);
void qdrv_show_assoc_print_usage(struct seq_file *s, void *data, uint32_t num);
void qdrv_show_assoc_print_stats(struct seq_file *s, void *data, uint32_t num);


#endif /* _QDRV_SHOW_H */

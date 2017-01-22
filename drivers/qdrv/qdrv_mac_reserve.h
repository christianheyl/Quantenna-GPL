/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
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

#ifndef _QDRV_MAC_RESERVE_H
#define _QDRV_MAC_RESERVE_H

#include "qdrv_wlan.h"

int __sram_text qdrv_mac_reserved(const uint8_t *addr);
void qdrv_mac_reserve_clear(void);
int qdrv_mac_reserve_set(const uint8_t *addr, const uint8_t *mask);
void qdrv_mac_reserve_show(struct seq_file *s, void *data, u32 num);
void qdrv_mac_reserve_init(struct qdrv_wlan *qw);

#endif /* _QDRV_MAC_RESERVE_H */

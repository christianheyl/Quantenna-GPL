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

#ifndef _HAPR_L2_PACKET_H_
#define _HAPR_L2_PACKET_H_

#include "hapr_types.h"
#include "hapr_eloop.h"

struct hapr_l2_packet;

struct hapr_l2_packet *hapr_l2_packet_create(const char *ifname, const uint8_t *own_addr,
	unsigned short protocol,
	void (*rx_callback)(void *ctx, const uint8_t *src_addr,
		const uint8_t *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr);

void hapr_l2_packet_destroy(struct hapr_l2_packet *l2);

int hapr_l2_packet_register(struct hapr_l2_packet *l2, struct hapr_eloop *eloop);

void hapr_l2_packet_unregister(struct hapr_l2_packet *l2, struct hapr_eloop *eloop);

int hapr_l2_packet_get_own_addr(struct hapr_l2_packet *l2, uint8_t *addr);

int hapr_l2_packet_send(struct hapr_l2_packet *l2, const uint8_t *dst_addr, uint16_t proto,
	const uint8_t *buf, size_t len);

#endif

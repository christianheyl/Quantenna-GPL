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

#ifndef _HAPR_NETLINK_H_
#define _HAPR_NETLINK_H_

#include "hapr_types.h"
#include "hapr_eloop.h"

struct hapr_netlink;
struct ifinfomsg;

struct hapr_netlink_config
{
	void *ctx;
	void (*newlink_cb)(void *ctx, struct ifinfomsg *ifi, uint8_t *buf, size_t len);
	void (*dellink_cb)(void *ctx, struct ifinfomsg *ifi, uint8_t *buf, size_t len);
};

struct hapr_netlink *hapr_netlink_create(struct hapr_netlink_config *cfg);

void hapr_netlink_destroy(struct hapr_netlink *netlink);

int hapr_netlink_register(struct hapr_netlink *netlink, struct hapr_eloop *eloop);

void hapr_netlink_unregister(struct hapr_netlink *netlink, struct hapr_eloop *eloop);

#endif

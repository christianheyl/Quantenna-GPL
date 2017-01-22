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

#ifndef _HAPR_CLIENT_H_
#define _HAPR_CLIENT_H_

#include "hapr_types.h"
#include "hapr_list.h"
#include "qlink_server.h"
#include <sys/un.h>

struct hapr_bss;

struct hapr_client
{
	struct qlink_server *qs;

	uint32_t ping_timeout_ms;
	uint32_t flags;
	uint32_t ctrl_client_check_timeout_ms;

	struct hapr_list bss_list;

	/*
	 * ctrl iface related.
	 */

	int ctrl_sock;
	struct sockaddr_un ctrl_addr;
	struct hapr_list ctrl_clients;
};

struct hapr_client *hapr_client_create(struct qlink_server *qs, const uint8_t *addr,
	uint32_t ping_timeout_ms, uint32_t flags, uint32_t ctrl_client_check_timeout_ms);

void hapr_client_destroy(struct hapr_client *client);

struct hapr_bss *hapr_client_get_bss(struct hapr_client *client, uint32_t qid);

int hapr_client_ctrl_init(struct hapr_client *client, const char *ctrl_dir, const char *ctrl_name);

int hapr_client_ctrl_send(struct hapr_client *client, const char *addr, const char *data, int len);

#endif

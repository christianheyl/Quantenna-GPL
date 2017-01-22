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

#ifndef _HAPR_BSS_H_
#define _HAPR_BSS_H_

#include "hapr_types.h"
#include "hapr_list.h"
#include "hapr_l2_packet.h"
#include "hapr_netlink.h"
#include <net/if.h>

struct hapr_client;

struct hapr_bss
{
	struct hapr_client *client;
	struct hapr_list list;
	uint32_t qid;
	char ifname[IFNAMSIZ];
	int ifindex;
	char brname[IFNAMSIZ];
	uint8_t own_addr[ETH_ALEN];
	int is_vap;
	int we_version;

	int ioctl_sock;	/* socket for ioctl() use */
	struct hapr_l2_packet *sock_xmit; /* raw packet xmit socket */
	struct hapr_l2_packet *sock_recv; /* raw packet recv socket */
	struct hapr_l2_packet *sock_raw; /* raw 802.11 management frames */
	struct hapr_netlink *netlink;
	int added_if_into_bridge;
};

struct hapr_bss *hapr_bss_create(struct hapr_client *client, uint32_t qid, const char *ifname,
	const char *brname, int is_vap);

void hapr_bss_destroy(struct hapr_bss *bss);

int hapr_bss_set80211param(struct hapr_bss *bss, int op, int arg);

int hapr_bss_set80211priv(struct hapr_bss *bss, int op, void *data, int len);

int hapr_bss_set_privacy(struct hapr_bss *bss, int enabled);

int hapr_bss_handle_bridge_eapol(struct hapr_bss *bss);

int hapr_bss_get_extended_capa(struct hapr_bss *bss, uint8_t *extended_capa_len,
	uint8_t **extended_capa, uint8_t **extended_capa_mask);

int hapr_bss_set_ssid(struct hapr_bss *bss, const uint8_t *buf, int len);

int hapr_bss_commit(struct hapr_bss *bss);

int hapr_bss_set_interworking(struct hapr_bss *bss, uint8_t eid, int interworking,
	uint8_t access_network_type, const uint8_t *hessid);

int hapr_bss_brcm(struct hapr_bss *bss, uint8_t *data, uint32_t len);

int hapr_bss_check_if(struct hapr_bss *bss);

int hapr_bss_set_channel(struct hapr_bss *bss, int channel);

int hapr_bss_get_ssid(struct hapr_bss *bss, uint8_t *buf, int *len);

int hapr_bss_send_action(struct hapr_bss *bss, const void *data, int len);

#endif

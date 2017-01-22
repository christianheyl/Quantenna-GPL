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

#include "hapr_netlink.h"
#include "hapr_log.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/rtnetlink.h>

struct hapr_netlink
{
	struct hapr_netlink_config *cfg;
	int sock;
};

static void hapr_netlink_receive_link(struct hapr_netlink *netlink,
	void (*cb)(void *ctx, struct ifinfomsg *ifi, uint8_t *buf, size_t len),
	struct nlmsghdr *h)
{
	if (cb == NULL || NLMSG_PAYLOAD(h, 0) < sizeof(struct ifinfomsg))
		return;

	cb(netlink->cfg->ctx, NLMSG_DATA(h),
		(uint8_t *)NLMSG_DATA(h) + NLMSG_ALIGN(sizeof(struct ifinfomsg)),
		NLMSG_PAYLOAD(h, sizeof(struct ifinfomsg)));
}

static void hapr_netlink_receive(int sock, void *user_data)
{
	struct hapr_netlink *netlink = user_data;
	char buf[8192];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	int max_events = 10;

try_again:
	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *)&from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			HAPR_LOG(ERROR, "netlink: recvfrom failed: %s", strerror(errno));
		return;
	}

	h = (struct nlmsghdr *)buf;
	while (NLMSG_OK(h, left)) {
		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			hapr_netlink_receive_link(netlink, netlink->cfg->newlink_cb, h);
			break;
		case RTM_DELLINK:
			hapr_netlink_receive_link(netlink, netlink->cfg->dellink_cb, h);
			break;
		}

		h = NLMSG_NEXT(h, left);
	}

	if (left > 0) {
		HAPR_LOG(DEBUG, "netlink: %d extra bytes in the end of netlink message", left);
	}

	if (--max_events > 0) {
		/*
		 * Try to receive all events in one eloop call in order to
		 * limit race condition on cases where AssocInfo event, Assoc
		 * event, and EAPOL frames are received more or less at the
		 * same time. We want to process the event messages first
		 * before starting EAPOL processing.
		 */
		goto try_again;
	}
}

struct hapr_netlink *hapr_netlink_create(struct hapr_netlink_config *cfg)
{
	struct hapr_netlink *netlink;
	struct sockaddr_nl local;

	netlink = malloc(sizeof(*netlink));
	if (netlink == NULL) {
		HAPR_LOG(ERROR, "cannot allocate netlink, no mem");
		return NULL;
	}

	memset(netlink, 0, sizeof(*netlink));

	netlink->sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (netlink->sock < 0) {
		HAPR_LOG(ERROR, "netlink: Failed to open netlink socket: %s", strerror(errno));
		free(netlink);
		return NULL;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(netlink->sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
		HAPR_LOG(ERROR, "netlink: Failed to bind netlink socket: %s", strerror(errno));
		close(netlink->sock);
		free(netlink);
		return NULL;
	}

	netlink->cfg = cfg;

	return netlink;
}

void hapr_netlink_destroy(struct hapr_netlink *netlink)
{
	close(netlink->sock);
	free(netlink->cfg);
	free(netlink);
}

int hapr_netlink_register(struct hapr_netlink *netlink, struct hapr_eloop *eloop)
{
	return hapr_eloop_register_read_sock(eloop, netlink->sock, hapr_netlink_receive, netlink);
}

void hapr_netlink_unregister(struct hapr_netlink *netlink, struct hapr_eloop *eloop)
{
	hapr_eloop_unregister_read_sock(eloop, netlink->sock);
}

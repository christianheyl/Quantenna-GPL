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

#include "hapr_l2_packet.h"
#include "hapr_log.h"
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

struct hapr_l2_packet
{
	struct hapr_eloop *eloop;
	int fd;
	char ifname[IFNAMSIZ + 1];
	int ifindex;
	uint8_t own_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, const uint8_t *src_addr,
		const uint8_t *buf, size_t len);
	void *rx_callback_ctx;
	int l2_hdr; /* whether to include layer 2 (Ethernet) header data buffers */
};

static void hapr_l2_packet_receive(int sock, void *user_data)
{
	struct hapr_l2_packet *l2 = user_data;
	uint8_t buf[2300];
	int res;
	struct sockaddr_ll ll;
	socklen_t fromlen;

	memset(&ll, 0, sizeof(ll));
	fromlen = sizeof(ll);
	res = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&ll, &fromlen);
	if (res < 0) {
		HAPR_LOG(DEBUG, "recvfrom: %s", strerror(errno));
		return;
	}

	l2->rx_callback(l2->rx_callback_ctx, ll.sll_addr, buf, res);
}

struct hapr_l2_packet *hapr_l2_packet_create(const char *ifname, const uint8_t *own_addr,
	unsigned short protocol,
	void (*rx_callback)(void *ctx, const uint8_t *src_addr,
		const uint8_t *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr)
{
	struct hapr_l2_packet *l2;
	struct ifreq ifr;
	struct sockaddr_ll ll;

	l2 = malloc(sizeof(*l2));
	if (l2 == NULL) {
		HAPR_LOG(ERROR, "cannot allocate l2_packet, no mem");
		return NULL;
	}

	memset(l2, 0, sizeof(*l2));

	strncpy(l2->ifname, ifname, sizeof(l2->ifname) - 1);
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;
	l2->l2_hdr = l2_hdr;

	l2->fd = socket(PF_PACKET, l2_hdr ? SOCK_RAW : SOCK_DGRAM,
		htons(protocol));
	if (l2->fd < 0) {
		HAPR_LOG(ERROR, "socket(PF_PACKET): %s", strerror(errno));
		free(l2);
		return NULL;
	}
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, l2->ifname, sizeof(ifr.ifr_name) - 1);
	if (ioctl(l2->fd, SIOCGIFINDEX, &ifr) < 0) {
		HAPR_LOG(ERROR, "ioctl[SIOCGIFINDEX]: %s", strerror(errno));
		close(l2->fd);
		free(l2);
		return NULL;
	}
	l2->ifindex = ifr.ifr_ifindex;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(protocol);
	if (bind(l2->fd, (struct sockaddr *)&ll, sizeof(ll)) < 0) {
		HAPR_LOG(ERROR, "bind[PF_PACKET]: %s", strerror(errno));
		close(l2->fd);
		free(l2);
		return NULL;
	}

	if (ioctl(l2->fd, SIOCGIFHWADDR, &ifr) < 0) {
		HAPR_LOG(ERROR, "ioctl[SIOCGIFHWADDR]: %s", strerror(errno));
		close(l2->fd);
		free(l2);
		return NULL;
	}
	memcpy(l2->own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return l2;
}

void hapr_l2_packet_destroy(struct hapr_l2_packet *l2)
{
	close(l2->fd);
	free(l2);
}

int hapr_l2_packet_register(struct hapr_l2_packet *l2, struct hapr_eloop *eloop)
{
	return hapr_eloop_register_read_sock(eloop, l2->fd, hapr_l2_packet_receive, l2);
}

void hapr_l2_packet_unregister(struct hapr_l2_packet *l2, struct hapr_eloop *eloop)
{
	hapr_eloop_unregister_read_sock(eloop, l2->fd);
}

int hapr_l2_packet_get_own_addr(struct hapr_l2_packet *l2, uint8_t *addr)
{
	memcpy(addr, l2->own_addr, ETH_ALEN);
	return 0;
}

int hapr_l2_packet_send(struct hapr_l2_packet *l2, const uint8_t *dst_addr, uint16_t proto,
	const uint8_t *buf, size_t len)
{
	int ret;

	if (l2->l2_hdr) {
		ret = send(l2->fd, buf, len, 0);
		if (ret < 0)
			HAPR_LOG(ERROR, "send: %s", strerror(errno));
	} else {
		struct sockaddr_ll ll;
		memset(&ll, 0, sizeof(ll));
		ll.sll_family = AF_PACKET;
		ll.sll_ifindex = l2->ifindex;
		ll.sll_protocol = htons(proto);
		ll.sll_halen = ETH_ALEN;
		memcpy(ll.sll_addr, dst_addr, ETH_ALEN);
		ret = sendto(l2->fd, buf, len, 0, (struct sockaddr *)&ll,
			     sizeof(ll));
		if (ret < 0) {
			HAPR_LOG(ERROR, "sendto: %s", strerror(errno));
		}
	}

	return ret;
}

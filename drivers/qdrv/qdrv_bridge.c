/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2010 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Quantenna Communications Inc                               **
**  File        : qdrv_bridge.c                                              **
**  Description : 3-address mode bridging                                    **
**                                                                           **
**  This module maintains two tables.                                        **
**                                                                           **
**  1. A Unicast table containing the MAC address and IP address of each     **
**     downstream client. This is used to determine the destination Ethernet **
**     MAC address for dowstream frames.                                     **
**                                                                           **
**  2. A Multicast table for keeping track of multicast group registrations  **
**     made by downstream clients.                                           **
**     Each multicast table entry contains an multicast IP address and a     **
**     list of one or more downstream clients that have joined the           **
**     multicast group.                                                      **
**     Downstream clients are not visible to the AP in 3-address mode, so    **
**     the AP 'sees' only that the station is joined to the multicast group. **
**     This table is used to ensure that an upstream  multicast LEAVE        **
**     message is only sent when the last downstream client leaves a         **
**     group.                                                                **
**                                                                           **
*******************************************************************************/
/**
  Copyright (c) 2010 Quantenna Communications Inc
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

#include <linux/ip.h>
#include <linux/in.h>
#include <linux/spinlock.h>
#include <linux/jhash.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include "linux/udp.h"
#include "linux/igmp.h"
#include "qdrv_debug.h"
#include "qdrv_bridge.h"

#include <qtn/iputil.h>
#include <qtn/topaz_fwt_sw.h>

#if defined(CONFIG_IPV6)
#include <net/ipv6.h>
#include <linux/icmpv6.h>
#endif

struct udp_dhcp_packet {
	struct iphdr iphdr_p;
	struct udphdr udphdr_p;
	struct dhcp_message dhcp_msg;
}__attribute__ ((packed));

#define BOOTREQUEST	1
#define DHCPSERVER_PORT	67
#define DHCPREQUEST	3
#define ARPHRD_ETHER	1
#define DHCP_BROADCAST_FLAG	0x8000

#define QDRV_IGMP_OP_NONE	0x00
#define QDRV_IGMP_OP_JOIN	0x01
#define QDRV_IGMP_OP_LEAVE	0x02
#define QDRV_IP_MCAST_PREF	0xE

#define QDRV_BR_PRINT_BUF_SIZE 2048
#define QDRV_BR_PRINT_LINE_MAX 48

static struct kmem_cache *br_uc_cache __read_mostly = NULL;
static struct kmem_cache *br_mc_cache __read_mostly = NULL;
static struct kmem_cache *br_mc_client_cache __read_mostly = NULL;
#if defined(CONFIG_IPV6)
static struct kmem_cache *br_ipv6uc_cache __read_mostly = NULL;
#endif

/*
 * Create a hash of a MAC address
 */
static int
qdrv_br_mac_hash(unsigned char *mac_addr)
{
	return jhash(mac_addr, IEEE80211_ADDR_LEN, 0) & (QDRV_BR_MAC_HASH_SIZE - 1);
}

/*
 * Create a hash of an IP address
 */
static int
qdrv_br_ip_hash(__be32 ip_addr)
{
	return jhash_1word(ip_addr, 0) & (QDRV_BR_IP_HASH_SIZE - 1);
}

#if defined(CONFIG_IPV6)
/*
 * Create a hash of an IPv6 address
 */
static int
qdrv_br_ipv6_hash(const struct in6_addr *ipv6_addr)
{
	uint32_t aligned32[2];

	aligned32[0] = get_unaligned((uint32_t *)(&ipv6_addr->s6_addr32[2]));
	aligned32[1] = get_unaligned((uint32_t *)(&ipv6_addr->s6_addr32[3]));

	return jhash_2words(aligned32[0], aligned32[1], 0) & (QDRV_BR_IP_HASH_SIZE - 1);
}
#endif

/*
 * Lock the unicast table for write access
 */
static void
qdrv_br_uc_lock(struct qdrv_br *br)
{
	spin_lock_irqsave(&br->uc_lock, br->uc_lock_flags);
}

/*
 * Unlock the unicast table
 */
static void
qdrv_br_uc_unlock(struct qdrv_br *br)
{
	spin_unlock_irqrestore(&br->uc_lock, br->uc_lock_flags);
}

/*
 * Lock the multicast table for write access
 */
static void
qdrv_br_lock_mc(struct qdrv_br *br)
{
	spin_lock_irqsave(&br->mc_lock, br->mc_lock_flags);
}

/*
 * Unlock the multicast table
 */
static void
qdrv_br_unlock_mc(struct qdrv_br *br)
{
	spin_unlock_irqrestore(&br->mc_lock, br->mc_lock_flags);
}

static int
qdrv_br_ip_is_unicast(u32 ip_addr)
{
	if ((ip_addr != INADDR_ANY) &&
			(ip_addr != INADDR_BROADCAST) &&
			(!IN_MULTICAST(ntohl(ip_addr)))) {
		return 1;
	}

	return 0;
}

/*
 * Free a unicast entry
 */
static void
qdrv_br_uc_free(struct rcu_head *head)
{
	struct qdrv_br_uc *br_uc;

	br_uc = container_of(head, struct qdrv_br_uc, rcu);

	kmem_cache_free(br_uc_cache, br_uc);
}

#if defined(CONFIG_IPV6)
static void
qdrv_br_ipv6uc_free(struct rcu_head *head)
{
	struct qdrv_br_ipv6_uc *br_ipv6_uc;

	br_ipv6_uc = container_of(head, struct qdrv_br_ipv6_uc, rcu);

	kmem_cache_free(br_ipv6uc_cache, br_ipv6_uc);
}
#endif

/*
 * Free a multicast entry
 */
static void
qdrv_br_mc_free(struct rcu_head *head)
{
	struct qdrv_br_mc *br_mc;

	br_mc = container_of(head, struct qdrv_br_mc, rcu);

	kmem_cache_free(br_mc_cache, br_mc);
}

/*
 * Free a multicast client entry
 */
static void
qdrv_br_mc_client_free(struct rcu_head *head)
{
	struct qdrv_br_mc_client *br_mc_client;

	br_mc_client = container_of(head, struct qdrv_br_mc_client, rcu);

	kmem_cache_free(br_mc_client_cache, br_mc_client);
}

/*
 * Remove a multicast client entry from a multicast entry
 * Assumes the multicast table is locked for write.
 */
static void
qdrv_br_mc_client_delete(struct qdrv_br *br, struct qdrv_br_mc *br_mc,
				struct qdrv_br_mc_client *br_mc_client)
{
	hlist_del_rcu(&br_mc_client->mc_client_hlist);
	call_rcu(&br_mc_client->rcu, qdrv_br_mc_client_free);
	atomic_dec(&br_mc->mc_client_tot);
	atomic_dec(&br->mc_tot);
}

/*
 * Remove a multicast address entry
 * Assumes the multicast table is locked for write.
 */
static void
qdrv_br_mc_delete(struct qdrv_br *br, struct qdrv_br_mc *br_mc)
{
	hlist_del_rcu(&br_mc->mc_hlist);
	call_rcu(&br_mc->rcu, qdrv_br_mc_free);
	atomic_dec(&br->mc_tot);
}

/*
 * Remove a unicast entry
 * Assumes the unicast table is locked for write.
 */
static void
qdrv_br_uc_delete(struct qdrv_br *br, struct qdrv_br_uc *br_uc)
{
	hlist_del_rcu(&br_uc->mac_hlist);
	hlist_del_rcu(&br_uc->ip_hlist);
	call_rcu(&br_uc->rcu, qdrv_br_uc_free);
	atomic_dec(&br->uc_tot);

	fwt_sw_remove_uc_ipmac((uint8_t *)&br_uc->ip_addr, __constant_htons(ETH_P_IP));
}

#if defined(CONFIG_IPV6)
/*
 * Remove an IPv6 unicast entry
 */
static void
qdrv_br_ipv6uc_delete(struct qdrv_br *br, struct qdrv_br_ipv6_uc *br_ipv6_uc)
{
	hlist_del_rcu(&br_ipv6_uc->ipv6_hlist);
	call_rcu(&br_ipv6_uc->rcu, qdrv_br_ipv6uc_free);
	atomic_dec(&br->uc_ipv6_tot);

	fwt_sw_remove_uc_ipmac((uint8_t *)&br_ipv6_uc->ipv6_addr, __constant_htons(ETH_P_IPV6));
}
#endif

/*
 * Find a multicast client entry
 * Assumes the multicast table is locked for read or write.
 */
static struct qdrv_br_mc_client *
qdrv_br_mc_client_find(struct qdrv_br_mc *br_mc, unsigned char *mac_addr)
{
	struct hlist_head *head = &br_mc->mc_client_hash[qdrv_br_mac_hash(mac_addr)];
	struct hlist_node *h;
	struct qdrv_br_mc_client *br_mc_client;

	hlist_for_each_entry_rcu(br_mc_client, h, head, mc_client_hlist) {
		if (IEEE80211_ADDR_EQ(br_mc_client->mac_addr, mac_addr)) {
			return br_mc_client;
		}
	}

	return NULL;
}

/*
 * Find a multicast entry
 * Assumes the multicast table is locked for read or write.
 */
static struct qdrv_br_mc *
qdrv_br_mc_find(struct qdrv_br *br, __be32 mc_ip_addr)
{
	struct hlist_head *head = &br->mc_ip_hash[qdrv_br_ip_hash(mc_ip_addr)];
	struct hlist_node *h;
	struct qdrv_br_mc *br_mc;

	hlist_for_each_entry_rcu(br_mc, h, head, mc_hlist) {
		if (br_mc->mc_ip_addr == mc_ip_addr) {
			return br_mc;
		}
	}

	return NULL;
}

/*
 * Find a unicast entry by IP address
 * Assumes the unicast table is locked for read or write.
 */
static struct qdrv_br_uc *
qdrv_br_uc_find_by_ip(struct qdrv_br *br, u32 ip_addr)
{
	struct hlist_head *head = &br->uc_ip_hash[qdrv_br_ip_hash(ip_addr)];
	struct hlist_node *h;
	struct qdrv_br_uc *br_uc;

	hlist_for_each_entry_rcu(br_uc, h, head, ip_hlist) {
		if (br_uc->ip_addr == ip_addr) {
			return br_uc;
		}
	}

	return NULL;
}

/* Currently unused static function qdrv_br_uc_find_by_mac - remove */
#if 0
/*
 * Find a unicast entry by MAC address
 * Assumes the unicast table is locked for read or write.
 */
static struct qdrv_br_uc *
qdrv_br_uc_find_by_mac(struct qdrv_br *br, unsigned char *mac_addr)
{
	struct hlist_head *head = &br->uc_mac_hash[qdrv_br_mac_hash(mac_addr)];
	struct hlist_node *h;
	struct qdrv_br_uc *br_uc;

	hlist_for_each_entry_rcu(br_uc, h, head, mac_hlist) {
		if (IEEE80211_ADDR_EQ(br_uc->mac_addr, mac_addr)) {
			return br_uc;
		}
	}

	return NULL;
}
#endif

#if defined(CONFIG_IPV6)
/*
 * Find a unicast entry by IPv6 address
 * Assumes the unicast table is locked for read or write
 */
static struct qdrv_br_ipv6_uc *
qdrv_br_ipv6uc_find_by_ip(struct qdrv_br *br, const struct in6_addr *ipv6_addr)
{
	struct hlist_head *head = &br->uc_ipv6_hash[qdrv_br_ipv6_hash(ipv6_addr)];
	struct hlist_node *h;
	struct qdrv_br_ipv6_uc *br_ipv6_uc;

	hlist_for_each_entry_rcu(br_ipv6_uc, h, head, ipv6_hlist) {
		if (memcmp(&br_ipv6_uc->ipv6_addr, ipv6_addr, sizeof(struct in6_addr)) == 0) {
			return br_ipv6_uc;
		}
	}

	return NULL;
}
#endif

/*
 * Add a multicast client entry to a multicast entry
 * Assumes the multicast table is locked for write.
 */
static void
qdrv_br_mc_client_add(struct qdrv_br *br, struct qdrv_br_mc *br_mc,
			unsigned char *mac_addr)
{
	struct qdrv_br_mc_client *br_mc_client;

	if (atomic_read(&br->mc_tot) >= QDRV_BR_MCAST_MAX) {
		DBGPRINTF_E("multicast table is full, can't add %pM\n",
			mac_addr);
		return;
	}

	br_mc_client = kmem_cache_alloc(br_mc_client_cache, GFP_ATOMIC);
	if (br_mc_client == NULL) {
		DBGPRINTF_E("failed to allocate multicast client entry %pM\n",
			mac_addr);
		return;
	}

	memset(br_mc_client, 0, sizeof(*br_mc_client));
	atomic_inc(&br->mc_tot);
	atomic_inc(&br_mc->mc_client_tot);
	IEEE80211_ADDR_COPY(br_mc_client->mac_addr, mac_addr);
	hlist_add_head_rcu(&br_mc_client->mc_client_hlist,
			&br_mc->mc_client_hash[qdrv_br_mac_hash(mac_addr)]);
}

/*
 * Add multicast entry
 * Assumes the multicast table is locked for write.
 */
static struct qdrv_br_mc *
qdrv_br_mc_add(struct qdrv_br *br, __be32 mc_ip_addr)
{
	struct qdrv_br_mc *br_mc;

	if (atomic_read(&br->mc_tot) >= QDRV_BR_MCAST_MAX) {
		DBGPRINTF_E("multicast table is full, cant add "
			NIPQUAD_FMT "\n",
			NIPQUAD(mc_ip_addr));
		return NULL;
	}

	br_mc = kmem_cache_alloc(br_mc_cache, GFP_ATOMIC);
	if (br_mc == NULL) {
		DBGPRINTF_E("failed to allocate multicast entry "
			NIPQUAD_FMT "\n",
			NIPQUAD(mc_ip_addr));
		return NULL;
	}

	memset(br_mc, 0, sizeof(*br_mc));

	atomic_inc(&br->mc_tot);
	br_mc->mc_ip_addr = mc_ip_addr;
	hlist_add_head_rcu(&br_mc->mc_hlist, &br->mc_ip_hash[qdrv_br_ip_hash(mc_ip_addr)]);

	return br_mc;
}

/*
 * Add a unicast entry
 * Assumes the unicast table is locked for write.
 */
static void
qdrv_br_uc_add(struct qdrv_br *br, unsigned char *mac_addr, __be32 ip_addr)
{
	struct qdrv_br_uc *br_uc;

	/* IP address must be unique.  Remove any stale entry. */
	br_uc = qdrv_br_uc_find_by_ip(br, ip_addr);
	if (br_uc != NULL) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_BRIDGE,
			"delete old entry mac=%pM ip=" NIPQUAD_FMT "\n",
			br_uc->mac_addr, NIPQUAD(ip_addr));
		qdrv_br_uc_delete(br, br_uc);
	} else {
		if (atomic_read(&br->uc_tot) >= QDRV_BR_ENT_MAX) {
			DBGPRINTF_LIMIT_E("unicast table is full, can't add %pM\n",
				mac_addr);
			return;
		}
	}

	br_uc = kmem_cache_alloc(br_uc_cache, GFP_ATOMIC);
	if (br_uc == NULL) {
		DBGPRINTF_E("failed to allocate unicast entry %pM\n",
			mac_addr);
		return;
	}

	memset(br_uc, 0, sizeof(*br_uc));

	atomic_inc(&br->uc_tot);
	IEEE80211_ADDR_COPY(br_uc->mac_addr, mac_addr);
	br_uc->ip_addr = ip_addr;
	hlist_add_head_rcu(&br_uc->mac_hlist, &br->uc_mac_hash[qdrv_br_mac_hash(mac_addr)]);

	hlist_add_head_rcu(&br_uc->ip_hlist, &br->uc_ip_hash[qdrv_br_ip_hash(ip_addr)]);
}

#if defined(CONFIG_IPV6)
static void
qdrv_br_ipv6uc_add(struct qdrv_br *br, const unsigned char *mac_addr, const struct in6_addr *ipv6_addr)
{
	struct qdrv_br_ipv6_uc *br_ipv6_uc;

	/* IP address must be unique.  Remove any stale entry. */
	br_ipv6_uc = qdrv_br_ipv6uc_find_by_ip(br, ipv6_addr);
	if (br_ipv6_uc != NULL) {
		qdrv_br_ipv6uc_delete(br, br_ipv6_uc);
	} else {
		if (atomic_read(&br->uc_ipv6_tot) >= QDRV_BR_ENT_MAX) {
			DBGPRINTF_LIMIT_E("unicast table is full, can't add %pM\n",
				mac_addr);
			return;
		}
	}

	br_ipv6_uc = kmem_cache_alloc(br_ipv6uc_cache, GFP_ATOMIC);
	if (br_ipv6_uc == NULL) {
		DBGPRINTF_E("failed to allocate unicast entry %pM\n",
			mac_addr);
		return;
	}

	memset(br_ipv6_uc, 0, sizeof(*br_ipv6_uc));

	atomic_inc(&br->uc_ipv6_tot);
	IEEE80211_ADDR_COPY(br_ipv6_uc->mac_addr, mac_addr);
	memcpy(&br_ipv6_uc->ipv6_addr, ipv6_addr, sizeof(br_ipv6_uc->ipv6_addr));

	hlist_add_head_rcu(&br_ipv6_uc->ipv6_hlist, &br->uc_ipv6_hash[qdrv_br_ipv6_hash(ipv6_addr)]);

	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
		"mapping %pI6 to %pM\n", &br_ipv6_uc->ipv6_addr, br_ipv6_uc->mac_addr);
}
#endif

/*
 * Update a multicast entry from an upstream IGMP packet
 *
 * For an IGMP JOIN, create an entry for the multicast address if it is not
 * already present, then add the client to the multicast address entry.
 *
 * For an IGMP LEAVE, delete the client from the multicast entry.  If no clients
 * are left under the multicast entry, delete the multicast entry.  If other clients
 * remain under the multicast entry, notify the caller so that the LEAVE message
 * is dropped.
 *
 * Returns 1 if leaving and other clients are registered with the multicast address,
 * otherwise 0.
 */
static int
qdrv_br_mc_update(struct qdrv_br *br, int op, __be32 mc_ip_addr, unsigned char *client_addr)
{
	struct qdrv_br_mc *br_mc;
	struct qdrv_br_mc_client *br_mc_client;
	int rc = 0;

	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
		"ip=" NIPQUAD_FMT " client=%pM op=%s\n",
		NIPQUAD(mc_ip_addr), client_addr,
		(op == QDRV_IGMP_OP_JOIN) ? "join" : "leave");

	qdrv_br_lock_mc(br);

	br_mc = qdrv_br_mc_find(br, mc_ip_addr);
	if ((br_mc == NULL) &&
			(op == QDRV_IGMP_OP_JOIN)) {
		br_mc = qdrv_br_mc_add(br, mc_ip_addr);
	}
	if (br_mc == NULL) {
		/* Either malloc failed or leaving and there is no mc entry to leave */
		if (op == QDRV_IGMP_OP_LEAVE) {
			DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
				"client=%pM leaving mc " NIPQUAD_FMT " but mc not in table\n",
				client_addr, NIPQUAD(mc_ip_addr));
		}
		qdrv_br_unlock_mc(br);
		return 0;
	}

	br_mc_client = qdrv_br_mc_client_find(br_mc, client_addr);
	switch (op) {
	case QDRV_IGMP_OP_JOIN:
		if (br_mc_client == NULL) {
			qdrv_br_mc_client_add(br, br_mc, client_addr);
		}
		break;
	case QDRV_IGMP_OP_LEAVE:
		if (br_mc_client == NULL) {
			/* This can happen, for example, if the STA rebooted after the JOIN */
			DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
				"client=%pM leaving mc " NIPQUAD_FMT " but not in table\n",
				client_addr, NIPQUAD(mc_ip_addr));
			if (br_mc != NULL) {
				rc = 1;
			}
		} else {
			qdrv_br_mc_client_delete(br, br_mc, br_mc_client);
			if (atomic_read(&br_mc->mc_client_tot) < 1) {
				qdrv_br_mc_delete(br, br_mc);
			} else {
				rc = 1;
			}
		}
	}

	qdrv_br_unlock_mc(br);

	return rc;
}

/*
 * Create or update an IPv4 unicast entry
 */
static void
qdrv_br_uc_update(struct qdrv_br *br, __be32 ip_addr, unsigned char *mac_addr)
{
	struct qdrv_br_uc *br_uc;

	qdrv_br_uc_lock(br);

	br_uc = qdrv_br_uc_find_by_ip(br, ip_addr);
	if (br_uc == NULL) {
		qdrv_br_uc_add(br, mac_addr, ip_addr);
	} else if (!IEEE80211_ADDR_EQ(br_uc->mac_addr, mac_addr)) {
		/* Update the entry if its MAC address has changed */
		hlist_del_rcu(&br_uc->mac_hlist);
		IEEE80211_ADDR_COPY(br_uc->mac_addr, mac_addr);
		hlist_add_head_rcu(&br_uc->mac_hlist,
				   &br->uc_mac_hash[qdrv_br_mac_hash(mac_addr)]);
	}

	fwt_sw_update_uc_ipmac(mac_addr, (uint8_t *)&ip_addr, __constant_htons(ETH_P_IP));

	qdrv_br_uc_unlock(br);
}

#if defined(CONFIG_IPV6)
/*
 * Create or update an IPv6 unicast entry
 */
static void
qdrv_br_ipv6uc_update(struct qdrv_br *br, const struct in6_addr *ipv6_addr, const unsigned char *mac_addr)
{
	struct qdrv_br_ipv6_uc *br_ipv6_uc;

	qdrv_br_uc_lock(br);

	br_ipv6_uc = qdrv_br_ipv6uc_find_by_ip(br, ipv6_addr);
	if (!br_ipv6_uc) {
		qdrv_br_ipv6uc_add(br, mac_addr, ipv6_addr);
	} else {
		/* Update the entry if its MAC address has changed */
		if (compare_ether_addr(br_ipv6_uc->mac_addr, mac_addr) != 0) {
			hlist_del_rcu(&br_ipv6_uc->ipv6_hlist);
			memcpy(br_ipv6_uc->mac_addr, mac_addr, ETH_ALEN);
			hlist_add_head_rcu(&br_ipv6_uc->ipv6_hlist,
				&br->uc_ipv6_hash[qdrv_br_ipv6_hash(ipv6_addr)]);
		}
	}

	fwt_sw_update_uc_ipmac(mac_addr, (uint8_t *)ipv6_addr, __constant_htons(ETH_P_IPV6));

	qdrv_br_uc_unlock(br);
}
#endif

/*
 * Update a unicast entry from an upstream ARP packet
 */
void
qdrv_br_uc_update_from_arp(struct qdrv_br *br, struct ether_arp *arp)
{
	__be32 ip_addr;

	ip_addr = get_unaligned((u32 *)&arp->arp_spa);

	if (!qdrv_br_ip_is_unicast(ip_addr)) {
		return;
	}

	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
		"mac=%pM ip=" NIPQUAD_FMT "\n",
		arp->arp_sha, NIPQUAD(ip_addr));

	qdrv_br_uc_update(br, ip_addr, arp->arp_sha);
}

/*
 * Update a multicast entry from an upstream IGMP packet
 *
 * Returns 0 if OK, or 1 if the packet should be dropped.
 *
 * A client station in 3-address mode forwards multicast subscriptions to the
 * AP with source address set to the station's Wifi MAC address, so the AP sees
 * only one subscription even if when multiple clients subscribe.  This
 * function's sole purpose is to keep track of the downstream clients that
 * subscribe to a given multicast address, and to only forward a delete
 * request when the last client leaves.
 *
 * Note: A combination of join and leave requests in a single message (probably
 * never done in practice?) may not be handled correctly, since the decision to
 * forward or drop the message can only be made once.
 */
int
qdrv_br_mc_update_from_igmp(struct qdrv_br *br, struct sk_buff *skb,
				struct ether_header *eh, struct iphdr *iphdr_p)
{
	const struct igmphdr *igmp_p = (struct igmphdr *)
		((unsigned int *) iphdr_p + iphdr_p->ihl);
	const struct igmpv3_report *igmpv3_p = (struct igmpv3_report *)
		((unsigned int *) iphdr_p + iphdr_p->ihl);

	__be32 mc_ip_addr = 0;
	int num = -1;
	int n = 0;
	int op;
	int rc = 0;

	if ((skb->data + skb->len + 1) < (unsigned char *)(igmp_p + 1)) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
			"IGMP packet is too small (%p/%p)\n",
			skb->data + skb->len, igmp_p + 1);
		return 0;
	}

	do {
		op = QDRV_IGMP_OP_NONE;

		switch(igmp_p->type) {
		case IGMP_HOST_MEMBERSHIP_REPORT:
			op = QDRV_IGMP_OP_JOIN;
			mc_ip_addr = get_unaligned((u32 *)&igmp_p->group);
			break;
		case IGMPV2_HOST_MEMBERSHIP_REPORT:
			op = QDRV_IGMP_OP_JOIN;
			mc_ip_addr = get_unaligned((u32 *)&igmp_p->group);
			break;
		case IGMP_HOST_LEAVE_MESSAGE:
			op = QDRV_IGMP_OP_LEAVE;
			mc_ip_addr = get_unaligned((u32 *)&igmp_p->group);
			break;
		case IGMPV3_HOST_MEMBERSHIP_REPORT:
			mc_ip_addr = get_unaligned((u32 *)&igmpv3_p->grec[n].grec_mca);
			if (num < 0) {
				num = ntohs(igmpv3_p->ngrec);
			}
			if ((igmpv3_p->grec[n].grec_type == IGMPV3_CHANGE_TO_EXCLUDE) ||
					(igmpv3_p->grec[n].grec_type == IGMPV3_MODE_IS_EXCLUDE)) {
				op = QDRV_IGMP_OP_JOIN;
			} else if ((igmpv3_p->grec[n].grec_type == IGMPV3_CHANGE_TO_INCLUDE) ||
					(igmpv3_p->grec[n].grec_type == IGMPV3_MODE_IS_INCLUDE)) {
				op = QDRV_IGMP_OP_LEAVE;
			}
			n++;
			break;
		default:
			break;
		}

		if (op > QDRV_IGMP_OP_NONE) {
			/* rc will be 1 if leaving and the multicast entry still has clients */
			rc = qdrv_br_mc_update(br, op, mc_ip_addr, eh->ether_shost);
		}
	} while (--num > 0);

	/* Last operation in packet determines whether it will be dropped */
	return rc;
}

/*
 * Update a unicast entry from an upstream DHCP packet
 */
void
qdrv_br_uc_update_from_dhcp(struct qdrv_br *br, struct sk_buff *skb, struct iphdr *iphdr_p)
{
	struct udp_dhcp_packet *dhcpmsg = (struct udp_dhcp_packet *)iphdr_p;
	struct udphdr *uh = &dhcpmsg->udphdr_p;
	__be32 ip_addr;
	__wsum csum;

	/* Is this a DHCP packet? */
	if ((skb->len < ((u8 *)iphdr_p - skb->data) + sizeof(*dhcpmsg)) ||
			(uh->dest != __constant_htons(DHCPSERVER_PORT)) ||
			(dhcpmsg->dhcp_msg.op != BOOTREQUEST) ||
			(dhcpmsg->dhcp_msg.htype != ARPHRD_ETHER)) {
		return;
	}

	/*
	 * 3rd party APs may not forward unicast DHCP responses to us, so set the
	 * broadcast flag and recompute the UDP checksum.
	 */
	if (!(dhcpmsg->dhcp_msg.flags & __constant_htons(DHCP_BROADCAST_FLAG))) {

		dhcpmsg->dhcp_msg.flags |= __constant_htons(DHCP_BROADCAST_FLAG);

		/* Recalculate the UDP checksum */
		if (uh->check != 0) {
			uh->check = 0;
			csum = csum_partial(uh, ntohs(uh->len), 0);

			/* Add psuedo IP header checksum */
			uh->check = csum_tcpudp_magic(iphdr_p->saddr, iphdr_p->daddr,
					ntohs(uh->len), iphdr_p->protocol, csum);

			/* 0 is converted to -1 */
			if (uh->check == 0) {
				uh->check = CSUM_MANGLED_0;
			}
		}
	}

	/*
	 * Assume that any record containing a valid client IP address in the bootp structure
	 * is valid.  Ideally we should parse the DHCP structure that follows
	 * the BOOTP structure for message type 3, but this should suffice.
	 */
	ip_addr = get_unaligned((u32 *)&dhcpmsg->dhcp_msg.ciaddr);
	if (qdrv_br_ip_is_unicast(ip_addr)) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
			"source=%d dest=%d op=%02x ip=" NIPQUAD_FMT " mac=%pM\n",
			dhcpmsg->udphdr_p.source, dhcpmsg->udphdr_p.dest,
			dhcpmsg->dhcp_msg.op, NIPQUAD(ip_addr),
			dhcpmsg->dhcp_msg.chaddr);
		qdrv_br_uc_update(br, ip_addr, dhcpmsg->dhcp_msg.chaddr);
	}
}

#if defined(CONFIG_IPV6)
void qdrv_br_ipv6uc_update_from_icmpv6(struct qdrv_br *br, const struct ethhdr *eth,
			const struct ipv6hdr *ipv6h, const struct icmp6hdr *icmpv6h)
{
	static const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;

	if (icmpv6h->icmp6_type != NDISC_NEIGHBOUR_SOLICITATION
			&& icmpv6h->icmp6_type != NDISC_NEIGHBOUR_ADVERTISEMENT)
		return;

	/*
	 * RFC4861 section 4.3: source address field of an ICMPv6 neighbor solicitation
	 * can be "unspecified" if Duplicate Address Detection is in progress
	 */
	if (memcmp(&in6addr_any, &ipv6h->saddr, sizeof(struct in6_addr)) == 0)
		return;

	qdrv_br_ipv6uc_update(br, &ipv6h->saddr, eth->h_source);
}
#endif

static int
qdrv_br_ipv4_set_dest_mac(struct qdrv_br *br, struct ether_header *eh, __be32 ip_addr)
{
	struct qdrv_br_uc *br_uc;

	if (!qdrv_br_ip_is_unicast(ip_addr))
		return 1;

	rcu_read_lock();

	br_uc = qdrv_br_uc_find_by_ip(br, ip_addr);
	if (!br_uc) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
			"IP address %pI4 not found in bridge table\n", &ip_addr);
		rcu_read_unlock();
		return 1;
	}

	IEEE80211_ADDR_COPY(eh->ether_dhost, br_uc->mac_addr);

	rcu_read_unlock();

	return 0;
}

#if defined(CONFIG_IPV6)
static int
qdrv_br_ipv6_set_dest_mac(struct qdrv_br *br, struct ether_header *eh, const struct in6_addr *ipv6_addr)
{
	struct qdrv_br_ipv6_uc *br_ipv6_uc;

	rcu_read_lock();

	br_ipv6_uc = qdrv_br_ipv6uc_find_by_ip(br, ipv6_addr);
	if (!br_ipv6_uc) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
			"IP address %pI6 not found in bridge table\n", ipv6_addr);
		rcu_read_unlock();
		return 1;
	}

	IEEE80211_ADDR_COPY(eh->ether_dhost, br_ipv6_uc->mac_addr);

	rcu_read_unlock();

	return 0;
}
#endif

/*
 * Replace the destination MAC address in a downstream packet
 *
 * For multicast IP (224.0.0.0 to 239.0.0.0), the MAC address may have been
 * changed to the station's unicast MAC address by the AP.  Convert it back to
 * an IPv4 Ethernet MAC address as per RFC 1112, section 6.4: place the
 * low-order 23-bits of the IP address into the low-order 23 bits of the
 * Ethernet multicast address 01-00-5E-00-00-00.

 * For unicast IP, use the qdrv bridge table.
 *
 * Returns 0 if OK, or 1 if the MAC was not updated.
 */
int
qdrv_br_set_dest_mac(struct qdrv_br *br, struct ether_header *eh, const struct sk_buff *skb)
{
	struct iphdr *iphdr_p;
	struct ether_arp *arp_p = NULL;
	__be32 ip_addr = INADDR_ANY;
	unsigned char *ip_addr_p = (unsigned char *)&ip_addr;
	char mc_pref[] = {0x01, 0x00, 0x5e};
#ifdef CONFIG_IPV6
	struct ipv6hdr *ip6hdr_p;
	struct in6_addr *ip6addr_p = NULL;
	char mc6_pref[] = {0x33, 0x33};
#endif
	void *l3hdr;
	uint16_t ether_type;

	if (eh->ether_type == __constant_htons(ETH_P_8021Q)) {
		ether_type = *(&eh->ether_type + 2);
		l3hdr = &eh->ether_type + 3;
	} else {
		ether_type = eh->ether_type;
		l3hdr = eh + 1;
	}

	if (ether_type == __constant_htons(ETH_P_IP)) {
		iphdr_p = (struct iphdr *)l3hdr;
		if ((skb->data + skb->len) < (unsigned char *)(iphdr_p + 1)) {
			DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
				"IP packet is too small (%p/%p)\n",
				skb->data + skb->len, iphdr_p + 1);
			return 1;
		}
		ip_addr = get_unaligned((u32 *)&iphdr_p->daddr);
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_BRIDGE,
			"ip proto=%u smac=%pM sip=" NIPQUAD_FMT " dip=" NIPQUAD_FMT "\n",
			iphdr_p->protocol, eh->ether_shost,
			NIPQUAD(iphdr_p->saddr), NIPQUAD(ip_addr));

		if ((ip_addr_p[0] >> 4) == QDRV_IP_MCAST_PREF) {
			eh->ether_dhost[0] = mc_pref[0];
			eh->ether_dhost[1] = mc_pref[1];
			eh->ether_dhost[2] = mc_pref[2];
			eh->ether_dhost[3] = ip_addr_p[1] & 0x7F;
			eh->ether_dhost[4] = ip_addr_p[2];
			eh->ether_dhost[5] = ip_addr_p[3];
			return 0;
		}

		return qdrv_br_ipv4_set_dest_mac(br, eh, ip_addr);
#if defined(CONFIG_IPV6)
	} else if (ether_type == __constant_htons(ETH_P_IPV6)) {
		ip6hdr_p = (struct ipv6hdr *)l3hdr;
		if ((skb->data + skb->len) < (unsigned char *)(ip6hdr_p + 1)) {
			DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
				"IP packet is too small (%p/%p)\n",
				skb->data + skb->len, ip6hdr_p + 1);
			return 1;
		}

		/*
		 * IPv6 address map to MAC address
		 * First two octets are the value 0x3333 and
		 * last four octets are the last four octets of ip.
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 * |0 0 1 1 0 0 1 1|0 0 1 1 0 0 1 1|
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 * |    IP6[12]    |    IP6[13]    |
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 * |    IP6[14]    |    IP6[15]    |
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		*/
		ip6addr_p = &(ip6hdr_p->daddr);
		if (ip6addr_p->s6_addr[0] == 0xFF) {
			eh->ether_dhost[0] = mc6_pref[0];
			eh->ether_dhost[1] = mc6_pref[1];
			eh->ether_dhost[2] = ip6addr_p->s6_addr[12];
			eh->ether_dhost[3] = ip6addr_p->s6_addr[13];
			eh->ether_dhost[4] = ip6addr_p->s6_addr[14];
			eh->ether_dhost[5] = ip6addr_p->s6_addr[15];
			return 0;
		}

		return qdrv_br_ipv6_set_dest_mac(br, eh, ip6addr_p);
#endif
	} else if (ether_type == __constant_htons(ETH_P_ARP)) {
		arp_p = (struct ether_arp *)l3hdr;
		if ((skb->data + skb->len + 1) < (unsigned char *)(arp_p + 1)) {
			DBGPRINTF(DBG_LL_CRIT, QDRV_LF_BRIDGE,
				"ARP packet is too small (%p/%p)\n",
				skb->data + skb->len, arp_p + 1);
			return 1;
		}
		ip_addr = get_unaligned((u32 *)&arp_p->arp_tpa);
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_BRIDGE,
			"ARP proto=%04x op=%04x sha=%pM tha=%pM "
			"sip=" NIPQUAD_FMT " dip=" NIPQUAD_FMT "\n",
			arp_p->ea_hdr.ar_pro, arp_p->ea_hdr.ar_op,
			arp_p->arp_sha, arp_p->arp_tha,
			NIPQUAD(arp_p->arp_spa), NIPQUAD(ip_addr));

		if (qdrv_br_ipv4_set_dest_mac(br, eh, ip_addr) == 0) {
			IEEE80211_ADDR_COPY(arp_p->arp_tha, eh->ether_dhost);
			return 0;
		}

		return 1;
	} else {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_BRIDGE,
			"Ethertype 0x%04x not supported\n", ntohs(ether_type));
		return 1;
	}
}

/*
 * Display all entries in a vap's bridge table
 */
void
qdrv_br_show(struct qdrv_br *br)
{
	struct hlist_node *h;
	struct hlist_node *h1;
	struct qdrv_br_uc *br_uc;
	struct qdrv_br_ipv6_uc *br_ipv6_uc;
	struct qdrv_br_mc *br_mc;
	struct qdrv_br_mc_client *br_mc_client;
	int i;
	int j;

	printk("Client MAC          IPv4 Address\n");

	rcu_read_lock();
	for (i = 0; i < QDRV_BR_IP_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(br_uc, h, &br->uc_ip_hash[i], ip_hlist) {
			printk("%pM   %pI4\n",
				br_uc->mac_addr, &br_uc->ip_addr);
		}
	}
	rcu_read_unlock();

	printk("\n");
	printk("Client MAC          IPv6 Address\n");

	rcu_read_lock();
	for (i = 0; i < QDRV_BR_IP_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(br_ipv6_uc, h, &br->uc_ipv6_hash[i], ipv6_hlist) {
			printk("%pM  %pI6\n",
				br_ipv6_uc->mac_addr, &br_ipv6_uc->ipv6_addr);
		}
	}
	rcu_read_unlock();

	printk("\n");
	printk("Multicast IP        Client MAC\n");

	rcu_read_lock();
	for (i = 0; i < QDRV_BR_IP_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(br_mc, h, &br->mc_ip_hash[i], mc_hlist) {

			printk(NIPQUAD_FMT "\n", NIPQUAD(br_mc->mc_ip_addr));

			for (j = 0; j < QDRV_BR_MAC_HASH_SIZE; j++) {
				hlist_for_each_entry_rcu(br_mc_client, h1,
							 &br_mc->mc_client_hash[j],
							 mc_client_hlist) {
					printk("                    %pM\n",
						br_mc_client->mac_addr);

				}
			}
		}
	}
	rcu_read_unlock();

	printk("\n");
}

/*
 * Clear the qdrv bridge table for a vap
 */
void
qdrv_br_clear(struct qdrv_br *br)
{
	struct hlist_node *h;
	struct hlist_node *h1;
	struct hlist_node *h2;
	struct hlist_node *h3;
	struct qdrv_br_uc *br_uc;
	struct qdrv_br_mc *br_mc;
	struct qdrv_br_mc_client *br_mc_client;
	int i;
	int j;

	qdrv_br_uc_lock(br);

	for (i = 0; i < QDRV_BR_MAC_HASH_SIZE; i++) {
		hlist_for_each_entry_safe(br_uc, h, h1, &br->uc_mac_hash[i], mac_hlist) {
			qdrv_br_uc_delete(br, br_uc);
		}
	}
	atomic_set(&br->uc_tot, 0);

	qdrv_br_uc_unlock(br);

	qdrv_br_lock_mc(br);

	for (i = 0; i < QDRV_BR_IP_HASH_SIZE; i++) {
		hlist_for_each_entry_safe(br_mc, h, h1, &br->mc_ip_hash[i], mc_hlist) {
			for (j = 0; j < QDRV_BR_MAC_HASH_SIZE; j++) {
				hlist_for_each_entry_safe(br_mc_client, h2, h3,
						&br_mc->mc_client_hash[j], mc_client_hlist) {
					qdrv_br_mc_client_delete(br, br_mc, br_mc_client);
				}
			}
			qdrv_br_mc_delete(br, br_mc);
		}
	}
	atomic_set(&br->mc_tot, 0);

	qdrv_br_unlock_mc(br);
}

/*
 * Delete the bridge table for a vap
 */
void
qdrv_br_delete(struct qdrv_br *br)
{
	qdrv_br_clear(br);
}

/*
 * Create the bridge table for a vap
 */
void
qdrv_br_create(struct qdrv_br *br)
{
	spin_lock_init(&br->uc_lock);
	spin_lock_init(&br->mc_lock);

	/* Cache tables are global and are never deleted */
	if (br_uc_cache != NULL) {
		return;
	}

	br_uc_cache = kmem_cache_create("qdrv_br_uc_cache",
					 sizeof(struct qdrv_br_uc),
					 0, 0, NULL);
	br_mc_cache = kmem_cache_create("qdrv_br_mc_cache",
					sizeof(struct qdrv_br_mc),
					0, 0, NULL);
	br_mc_client_cache = kmem_cache_create("qdrv_br_mc_client_cache",
					sizeof(struct qdrv_br_mc_client),
					0, 0, NULL);
	br_ipv6uc_cache = kmem_cache_create("qdrv_br_ipv6_uc_cache",
					sizeof(struct qdrv_br_ipv6_uc),
					0, 0, NULL);

	KASSERT(((br_uc_cache != NULL) && (br_mc_cache != NULL) &&
			(br_mc_client_cache != NULL) && (br_ipv6uc_cache != NULL)),
		(DBGEFMT "Could not allocate qdrv bridge cache", DBGARG));
}

void qdrv_br_exit(struct qdrv_br *br)
{
	kmem_cache_destroy(br_uc_cache);
	kmem_cache_destroy(br_mc_cache);
	kmem_cache_destroy(br_mc_client_cache);
	kmem_cache_destroy(br_ipv6uc_cache);
}


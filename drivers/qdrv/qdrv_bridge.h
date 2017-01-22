/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
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

#ifndef _QDRV_BRIDGE_H_
#define _QDRV_BRIDGE_H_

#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#if defined(CONFIG_IPV6)
#include <net/ipv6.h>
#include <linux/in6.h>
#endif
#include <net80211/if_ethersubr.h>
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_comm.h"

#define QDRV_BR_ENT_MAX			128	/* unicast clients */
#define QDRV_BR_MCAST_MAX		128	/* multicast addresses & clients */
#define QDRV_BR_MAC_HASH_SIZE		256
#define QDRV_BR_IP_HASH_SIZE		256

struct	ether_arp {
	struct	arphdr ea_hdr;		/* fixed-size header */
	u_int8_t arp_sha[ETH_ALEN];	/* sender hardware address */
	u_int8_t arp_spa[4];		/* sender protocol address */
	u_int8_t arp_tha[ETH_ALEN];	/* target hardware address */
	u_int8_t arp_tpa[4];		/* target protocol address */
};

/*
 * Each bridge table (one per vap) contains:
 * - a list of downstream (unicast) clients, keyed by MAC address and IP address hash
 * - a list of multicast addresses, keyed by IP address, containing a list of
 *   subscribed downstream client MAC addresses
 * All keys are implemented as hash tables.
 */
struct qdrv_br_uc {
	__be32				ip_addr;
	unsigned char			mac_addr[IEEE80211_ADDR_LEN];
	struct hlist_node		mac_hlist;
	struct hlist_node		ip_hlist;
	struct rcu_head			rcu;
};

struct qdrv_br_mc {
	__be32				mc_ip_addr;
	atomic_t			mc_client_tot;
	struct hlist_node		mc_hlist;
	struct hlist_head		mc_client_hash[QDRV_BR_IP_HASH_SIZE];
	struct rcu_head			rcu;
};

struct qdrv_br_mc_client {
	unsigned char			mac_addr[IEEE80211_ADDR_LEN];
	struct hlist_node		mc_client_hlist;
	struct rcu_head			rcu;
};

#if defined(CONFIG_IPV6)
struct qdrv_br_ipv6_uc {
	struct in6_addr			ipv6_addr;
	unsigned char			mac_addr[ETH_ALEN];
	struct hlist_node		mac_hlist;
	struct hlist_node		ipv6_hlist;
	struct rcu_head			rcu;
};
#endif

struct qdrv_br {
	spinlock_t			uc_lock;
	spinlock_t			mc_lock;
	unsigned long			uc_lock_flags;
	unsigned long			mc_lock_flags;
	atomic_t			uc_tot;
	atomic_t			mc_tot;	// Total multicast and client entries
	struct hlist_head		uc_mac_hash[QDRV_BR_MAC_HASH_SIZE];
	struct hlist_head		uc_ip_hash[QDRV_BR_IP_HASH_SIZE];
	struct hlist_head		mc_ip_hash[QDRV_BR_IP_HASH_SIZE];
#if defined(CONFIG_IPV6)
	struct hlist_head		uc_ipv6_hash[QDRV_BR_IP_HASH_SIZE];
	atomic_t			uc_ipv6_tot;
	spinlock_t			uc_ipv6_lock;
	unsigned long			uc_ipv6_lock_flags;
#endif
};

void qdrv_br_create(struct qdrv_br *br);
void qdrv_br_exit(struct qdrv_br *br);
void qdrv_br_delete(struct qdrv_br *br);
void qdrv_br_show(struct qdrv_br *br);
void qdrv_br_clear(struct qdrv_br *br);
void qdrv_br_uc_update_from_dhcp(struct qdrv_br *br, struct sk_buff *skb,
				  struct iphdr *iphdr_p);
void qdrv_br_uc_update_from_arp(struct qdrv_br *br, struct ether_arp *arp);
int qdrv_br_mc_update_from_igmp(struct qdrv_br *br, struct sk_buff *skb,
				struct ether_header *eh, struct iphdr *iphdr_p);

#if defined(CONFIG_IPV6)
void qdrv_br_ipv6uc_update_from_icmpv6(struct qdrv_br *br,
				const struct ethhdr *eth,
				const struct ipv6hdr *ipv6h,
				const struct icmp6hdr *icmpv6h);
#endif

int qdrv_br_set_dest_mac(struct qdrv_br *br, struct ether_header *eh, const struct sk_buff *skb);

#endif

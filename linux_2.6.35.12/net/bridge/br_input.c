/*
 *	Handle incoming frames
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include "br_private.h"

#include <linux/igmp.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/if_ether.h>

#include <qtn/iputil.h>

#ifdef CONFIG_IPV6
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/icmpv6.h>
#endif

#ifndef ETH_P_80211_RAW
# define ETH_P_80211_RAW 0x0019
#endif

static int br_vlan_promisc = 0;

void br_vlan_set_promisc(int enable)
{
	printk("%s vlan promiscuous mode\n", enable ? "Enabling" : "Disabling");
	br_vlan_promisc = enable;
}

EXPORT_SYMBOL(br_vlan_set_promisc);

/* Bridge group multicast address 802.1d (pg 51). */
const u8 br_group_address[ETH_ALEN] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

static int br_pass_frame_up(struct sk_buff *skb)
{
	struct net_device *indev, *brdev = BR_INPUT_SKB_CB(skb)->brdev;
	struct net_bridge *br = netdev_priv(brdev);
	struct br_cpu_netstats *brstats = this_cpu_ptr(br->stats);

	brstats->rx_packets++;
	brstats->rx_bytes += skb->len;

	indev = skb->dev;
	skb->dev = brdev;

	return NF_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN, skb, indev, NULL,
		       netif_receive_skb);
}

static int br_handle_to_protoclstack(const unsigned char *dest, void *data)
{
	int retval;

	retval = !iputil_mac_is_v4_multicast(dest) ||
		iputil_is_v4_ssdp(dest, data) ||
		iputil_is_lncb(dest, data);

#ifdef CONFIG_IPV6
	if (retval)
		retval = (!iputil_mac_is_v6_multicast(dest) ||
			iputil_mac_is_v6_local((struct ipv6hdr *)data));
#endif

	return retval;
}

static int br_handle_mcast_mgmt_frame(struct sk_buff *skb, struct net_bridge *br)
{
	return br->igmp_snoop_enabled && (!BR_INPUT_SKB_CB(skb)->igmp ||
			BR_INPUT_SKB_CB_UCAST_FWD(skb) ||
			BR_INPUT_SKB_CB_MROUTERS_ONLY(skb));
}

static int br_handle_mcast_exception(struct net_bridge *br,
		const unsigned char *dest, void *data)
{
	struct iphdr *iph;
	int retval;
	int ssdp_check;
#ifdef CONFIG_IPV6
	struct ipv6hdr *ip6hdr;
#endif

	iph = data;
	ssdp_check = (br->ssdp_flood_state == BR_SSDP_FLOOD_DISABLED);

	retval = iputil_mac_is_v4_multicast(dest) &&
			(!iputil_is_v4_ssdp(dest, iph) || ssdp_check);
#ifdef CONFIG_IPV6
	if (!retval) {
		ip6hdr = data;
		retval = iputil_mac_is_v6_multicast(dest) &&
				(!iputil_is_v6_ssdp(dest, ip6hdr) || ssdp_check) &&
				!iputil_mac_is_v6_local(ip6hdr);
	}
#endif

	return retval;
}

int br_handle_frame_finish_multicast(struct sk_buff *skb,
		struct net_bridge *br, const unsigned char *dest)
{
	struct net_bridge_mdb_entry *mdst;
	struct sk_buff *skb2 = NULL;
	struct ethhdr *eth = eth_hdr(skb);
	void *data;

	if (eth->h_proto != __constant_htons(ETH_P_8021Q))
		data = (void *)(eth + 1);
	else
		data = (void *)((struct vlan_ethhdr *)eth + 1);

	br->dev->stats.multicast++;

	if (br_handle_to_protoclstack(dest, data))
		skb2 = skb;

	if (!iputil_is_lncb(dest, data) &&
			br_handle_mcast_mgmt_frame(skb, br) &&
			br_handle_mcast_exception(br, dest, data)) {
		if (BR_INPUT_SKB_CB_MROUTERS_ONLY(skb)) {
			mdst = br_mdb_get_ext(br, skb);
			br_report_flood(br, mdst, skb);
			br_multicast_forward(NULL, skb, skb2);
			skb = NULL;
		} else if(BR_INPUT_SKB_CB_UCAST_FWD(skb)) {
			mdst = br_mdb_get_ext(br, skb);
			if (mdst) {
				br_multicast_copy_to_sub_ports(br, mdst, skb);

				if (skb2 == NULL)
					kfree_skb(skb);
				skb = NULL;
			} else if (br->igmp_snoop_enabled) {
				goto drop;
			}
		} else {
			mdst = br_mdb_get(br, skb);
			if (mdst) {
				br_multicast_forward(mdst, skb, skb2);
				skb = NULL;
			} else if (br->igmp_snoop_enabled) {
				goto drop;
			}
		}
	}

	if (skb) {
		skb->dest_port = 0;
		br_flood_forward(br, skb, skb2);
	}

	if (skb2)
		return br_pass_frame_up(skb2);

out:
	return 0;
drop:
	kfree_skb(skb);
	goto out;
}

/* note: already called with rcu_read_lock (preempt_disabled) */
int __sram_text br_handle_frame_finish(struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge_port *p = rcu_dereference(skb->dev->br_port);
	struct net_bridge *br;
	struct net_bridge_fdb_entry *dst;
	struct sk_buff *skb2;

	if (!p || p->state == BR_STATE_DISABLED)
		goto drop;

	br = p->br;

	/* ETH_P_80211_RAW */
	/* This type of frame should be passed up the stack, not forwarded or flooded.
	 * Note that due to the nature of the skb with this protocol, the header is an
	 * 802.11 header, not an Ethernet header.
	 */
	if (skb->protocol == __constant_htons(ETH_P_80211_RAW))
	{
		/* Pass it up, do not forward or add entries to the bridge table */
		BR_INPUT_SKB_CB(skb)->brdev = br->dev;
		br_pass_frame_up(skb);
		goto out;
	}

	/* insert into forwarding database after filtering to avoid spoofing */
	if (!M_FLAG_ISSET(skb, M_NO_L2_LRN))
	br_fdb_update(br, p, eth_hdr(skb)->h_source, skb->src_port);
	br_handle_fwt_capacity(br);

	if (is_multicast_ether_addr(dest) &&
		br_multicast_rcv(br, p, skb))
		goto drop;

	if (p->state == BR_STATE_LEARNING)
		goto drop;

	BR_INPUT_SKB_CB(skb)->brdev = br->dev;

	/* The packet skb2 goes to the local host (NULL to skip). */
	skb2 = NULL;

	if (br->dev->flags & IFF_PROMISC)
		skb2 = skb;
	else if (unlikely(br_vlan_promisc && skb->protocol == __constant_htons(ETH_P_8021Q)))
		skb2 = skb;

	dst = NULL;

#ifdef CONFIG_IPV6
	if ((eth_hdr(skb)->h_proto == __constant_htons(ETH_P_IPV6)) &&
		(skb->len < sizeof(struct ipv6hdr))) {
		goto drop;
	}
#endif

	if (is_multicast_ether_addr(dest)) {
		return br_handle_frame_finish_multicast(skb, br, dest);
	} else if ((dst = __br_fdb_get(br, dest)) && dst->is_local) {
		skb2 = skb;
		/* Do not forward the packet since it's local. */
		skb = NULL;
	}

	if (skb) {
		if (dst) {
			skb->dest_port = dst->sub_port;
			br_forward(dst->dst, skb, skb2);
		} else {
			skb->dest_port = 0;
			br_flood_forward(br, skb, skb2);
		}
	}

	if (skb2)
		return br_pass_frame_up(skb2);

out:
	return 0;
drop:
	kfree_skb(skb);
	goto out;
}

/* note: already called with rcu_read_lock */
static int br_handle_local_finish(struct sk_buff *skb)
{
	struct net_bridge_port *p = rcu_dereference(skb->dev->br_port);

	if (p)
		br_fdb_update(p->br, p, eth_hdr(skb)->h_source, 0);
	return 0;	 /* process further */
}

/* Does address match the link local multicast address.
 * 01:80:c2:00:00:0X
 */
static inline int is_link_local(const unsigned char *dest)
{
	__be16 *a = (__be16 *)dest;
	static const __be16 *b = (const __be16 *)br_group_address;
	static const __be16 m = cpu_to_be16(0xfff0);

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | ((a[2] ^ b[2]) & m)) == 0;
}

/*
 * Called via br_handle_frame_hook.
 * Return NULL if skb is handled
 * note: already called with rcu_read_lock
 */
struct sk_buff * __sram_text br_handle_frame(struct net_bridge_port *p, struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	int (*rhook)(struct sk_buff *skb);

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return NULL;

	/* For ETH_P_80211_RAW, any pointers to addresses are invalid, so
	 * simply pass the packet onto the br_handle_frame_finish to pass
	 * it up the stack.
	 */
	if (skb->protocol == __constant_htons(ETH_P_80211_RAW)) {
		if (p->state == BR_STATE_FORWARDING || p->state == BR_STATE_LEARNING) {
			NF_HOOK(PF_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
					br_handle_frame_finish);
		} else {
			kfree_skb( skb );
		}
		return NULL;
	}

	if (!is_valid_ether_addr(eth_hdr(skb)->h_source))
		goto drop;

	if (unlikely(is_link_local(dest))) {
		/* Pause frames shouldn't be passed up by driver anyway */
		if (skb->protocol == htons(ETH_P_PAUSE))
			goto drop;

		/* If STP is turned off, then forward */
		if (p->br->stp_enabled == BR_NO_STP && dest[5] == 0)
			goto forward;

		if (NF_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN, skb, skb->dev,
			    NULL, br_handle_local_finish))
			return NULL;	/* frame consumed by filter */
		else
			return skb;	/* continue processing */
	}

forward:
	switch (p->state) {
	case BR_STATE_FORWARDING:
		rhook = rcu_dereference(br_should_route_hook);
		if (rhook != NULL) {
			if (rhook(skb))
				return skb;
			dest = eth_hdr(skb)->h_dest;
		}
		/* fall through */
	case BR_STATE_LEARNING:
		if (!compare_ether_addr(p->br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		NF_HOOK(NFPROTO_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
			br_handle_frame_finish);
		break;
	default:
drop:
		kfree_skb(skb);
	}
	return NULL;
}

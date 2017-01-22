/*
 * Bridge multicast support.
 *
 * Copyright (c) 2010 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#include <linux/err.h>
#include <linux/if_ether.h>
#include <linux/igmp.h>
#include <linux/jhash.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/netfilter_bridge.h>
#include <linux/random.h>
#include <linux/rculist.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <net/ip.h>
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
#include <net/ipv6.h>
#include <net/mld.h>
#include <net/addrconf.h>
#include <net/ip6_checksum.h>
#endif

#include "br_private.h"

/**
 * Added by Quantenna
 * Set cbks for handling multicast scenarios.
 */
static br_join_multicast_cbk g_fwt_mult_join_hook = NULL;
static br_leave_multicast_cbk g_fwt_mult_leave_hook = NULL;

static void br_ip4_multicast_leave_group(struct net_bridge *br,
					 struct net_bridge_port *port,
					 const uint8_t *src_mac,
					 u32 sub_port,
					 __be32 group);

unsigned int g_br_igmp_snoop_dbg = 0;

/*
 * IGMPv3 Group Record Type
 */
static const char *igmpv3_grec_type[] =
{
	"mode_is_include",
	"mode_is_exclude",
	"change_to_include_mode",
	"change_to_exclude_mode",
	"allow_new_sources",
	"block_old_sources",
};

/*
 * This function assumes that igmp packet is valid
 */
static void br_igmp_snoop_trace_v3_report(struct net_device *dev, __be32 saddr,
	const struct igmphdr *igmp, const char *vlan_str, uint8_t snoop_enabled)
{
	struct igmpv3_report *igmpv3_rpt = (struct igmpv3_report *)igmp;
	struct igmpv3_grec *igmpv3_grec = &igmpv3_rpt->grec[0];

	u8 ngrec;	/* number of Group Records */
	u8 grec_idx;

	BR_SNOOP_DBG("IGMPv3 Membership Report from %pI4, vlan %s%s\n",
		&saddr, vlan_str, snoop_enabled ? "" : " [ignored]");

	ngrec = ntohs(igmpv3_rpt->ngrec);
	/* impossible */
	if (ngrec == 0) {
		BR_SNOOP_DBG("  Malformed Report with empty Group Records!\n");
		return;
	}

	for (grec_idx = 0; grec_idx < ngrec; grec_idx++) {
		u16 grec_nsrcs;	/* number of sources in current Group Record */
		u16 grec_len;	/* length of current Group Record */
		u16 nsrc_idx;
		u16 is_first = 1;

		if ((igmpv3_grec->grec_type < IGMPV3_MODE_IS_INCLUDE) ||
				(igmpv3_grec->grec_type > IGMPV3_BLOCK_OLD_SOURCES))
			continue;

		grec_nsrcs = ntohs(igmpv3_grec->grec_nsrcs);
		grec_len = sizeof(struct igmpv3_grec) + grec_nsrcs * sizeof(__be32);

		BR_SNOOP_DBG("  %u %pI4 %s ",
			grec_idx,
			&igmpv3_grec->grec_mca,
			igmpv3_grec_type[igmpv3_grec->grec_type - 1]);

		/* dump source list */
		for (nsrc_idx = 0; nsrc_idx < grec_nsrcs; nsrc_idx++) {
			BR_SNOOP_DBG("%s %pI4", is_first ? "" : ", ",
				&igmpv3_grec->grec_src[nsrc_idx]);
			is_first = 0;
		}
		BR_SNOOP_DBG("\n");

		/* Go to the next record */
		igmpv3_grec = (struct igmpv3_grec *)((u8 *)igmpv3_grec + grec_len);
	}
}

#define BR_VLANID_STR_MAX_LEN	8
static void br_igmp_snoop_trace(struct net_device *dev, __be32 saddr,
	const struct igmphdr *igmp, struct sk_buff *skb, uint8_t snoop_enabled)
{
	struct vlan_ethhdr *veth = vlan_eth_hdr(skb);
	char vlan_str[BR_VLANID_STR_MAX_LEN] = { 0 };

	if (g_br_igmp_snoop_dbg) {
		if (veth->h_vlan_proto == __constant_htons(ETH_P_8021Q))
			snprintf(vlan_str, BR_VLANID_STR_MAX_LEN, "%u", ntohs(veth->h_vlan_TCI) & VLAN_VID_MASK);
		else
			snprintf(vlan_str, BR_VLANID_STR_MAX_LEN, "%s", "none");
	}

	switch(igmp->type) {
	case IGMP_HOST_MEMBERSHIP_QUERY:
		BR_SNOOP_DBG("IGMP Membership Query, vlan %s", vlan_str);
		break;
	case IGMP_HOST_MEMBERSHIP_REPORT:
		BR_SNOOP_DBG("IGMPv1 Membership Report, vlan %s", vlan_str);
		break;
	case IGMPV2_HOST_MEMBERSHIP_REPORT:
		BR_SNOOP_DBG("IGMPv2 Membership Report, vlan %s", vlan_str);
		break;
	case IGMP_HOST_LEAVE_MESSAGE:
		BR_SNOOP_DBG("IGMP Leave Group, vlan %s", vlan_str);
		break;
	case IGMPV3_HOST_MEMBERSHIP_REPORT:
		br_igmp_snoop_trace_v3_report(dev, saddr, igmp, vlan_str, snoop_enabled);
		return;
	default:
		BR_SNOOP_DBG("Unsupported type %u, vlan %s\n", igmp->type, vlan_str);
		return;
	}

	BR_SNOOP_DBG(" from %pI4 group %pI4%s\n",
		&saddr, &igmp->group, snoop_enabled ? "" : " [ignored]");

}

void br_multicast_mdb_dump(struct net_bridge_mdb_entry *me)
{
	struct net_bridge_port_group *pg;
	char mac_addr[ETH_ALEN];

	if (me->addr.proto == htons(ETH_P_IP))
		ip_eth_mc_map(me->addr.u.ip4, mac_addr);
	else if (me->addr.proto == htons(ETH_P_IPV6))
		ipv6_eth_mc_map(&me->addr.u.ip6, (uint8_t *)mac_addr);
	else
		return;

	printk("%pM\n", mac_addr);

	/* dump group members */
	for (pg = me->ports; pg != NULL; pg = pg->next) {
		/* skip empty port */
		if (pg->subport_reqs.count == 0)
			continue;

		printk("  %-12s 0x%x\n", pg->port->dev->name, pg->sub_port);
	}
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static inline int ipv6_is_local_multicast(const struct in6_addr *addr)
{
	if (ipv6_addr_is_multicast(addr) &&
	    IPV6_ADDR_MC_SCOPE(addr) <= IPV6_ADDR_SCOPE_LINKLOCAL)
		return 1;
	return 0;
}

static void br_ip6_multicast_leave_group(struct net_bridge *br,
					 struct net_bridge_port *port,
					 const uint8_t *src_mac,
					 u32 sub_port,
					 const struct in6_addr *group);
#endif

static inline int br_ip_equal(const struct br_ip *a, const struct br_ip *b)
{
	if (a->proto != b->proto)
		return 0;
	switch (a->proto) {
	case htons(ETH_P_IP):
		return a->u.ip4 == b->u.ip4;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case htons(ETH_P_IPV6):
		return ipv6_addr_equal(&a->u.ip6, &b->u.ip6);
#endif
	}
	return 0;
}

static inline int __br_ip4_hash(struct net_bridge_mdb_htable *mdb, __be32 ip)
{
	return jhash_1word(mdb->secret, (__force u32)ip) & (mdb->max - 1);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static inline int __br_ip6_hash(struct net_bridge_mdb_htable *mdb,
				const struct in6_addr *ip)
{
	return jhash2((__force u32 *)ip->s6_addr32, 4, mdb->secret) & (mdb->max - 1);
}
#endif

static inline int br_ip_hash(struct net_bridge_mdb_htable *mdb,
			     struct br_ip *ip)
{
	switch (ip->proto) {
	case htons(ETH_P_IP):
		return __br_ip4_hash(mdb, ip->u.ip4);
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case htons(ETH_P_IPV6):
		return __br_ip6_hash(mdb, &ip->u.ip6);
#endif
	}
	return 0;
}

#if 0
/* for debug only */
static int br_show_group(struct net_bridge_mdb_entry *mp);
static int br_show_all(struct net_bridge *br);
#endif

static struct net_bridge_mdb_entry *__br_mdb_ip_get(
	struct net_bridge_mdb_htable *mdb, struct br_ip *dst, int hash)
{
	struct net_bridge_mdb_entry *mp;
	struct hlist_node *p;

	hlist_for_each_entry_rcu(mp, p, &mdb->mhash[hash], hlist[mdb->ver]) {
		if (br_ip_equal(&mp->addr, dst)) {
			return mp;
		}
	}

	return NULL;
}

static struct net_bridge_mdb_entry *br_mdb_ip_get(
	struct net_bridge_mdb_htable *mdb, struct br_ip *dst)
{
	if (!mdb)
		return NULL;

	return __br_mdb_ip_get(mdb, dst, br_ip_hash(mdb, dst));
}

static struct net_bridge_mdb_entry *br_mdb_ip4_get(
	struct net_bridge_mdb_htable *mdb, __be32 dst)
{
	struct br_ip br_dst;

	br_dst.u.ip4 = dst;
	br_dst.proto = htons(ETH_P_IP);

	return br_mdb_ip_get(mdb, &br_dst);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static struct net_bridge_mdb_entry *br_mdb_ip6_get(
	struct net_bridge_mdb_htable *mdb, const struct in6_addr *dst)
{
	struct br_ip br_dst;

	ipv6_addr_copy(&br_dst.u.ip6, dst);
	br_dst.proto = htons(ETH_P_IPV6);

	return br_mdb_ip_get(mdb, &br_dst);
}
#endif

struct net_bridge_mdb_entry *br_mdb_get_ext(struct net_bridge *br,
					struct sk_buff *skb)
{
	struct net_bridge_mdb_htable *mdb = br->mdb;
	struct br_ip ip;

	if (skb->protocol == __constant_htons(ETH_P_8021Q)) {
		ip.proto = vlan_eth_hdr(skb)->h_vlan_encapsulated_proto;
	} else {
		ip.proto = skb->protocol;
	}

	switch (ip.proto) {
	case htons(ETH_P_IP):
		ip.u.ip4 = ip_hdr(skb)->daddr;
		break;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case htons(ETH_P_IPV6):
		ipv6_addr_copy(&ip.u.ip6, &ipv6_hdr(skb)->daddr);
		break;
#endif
	default:
		return NULL;
	}

	return br_mdb_ip_get(mdb, &ip);
}

struct net_bridge_mdb_entry *br_mdb_get(struct net_bridge *br,
					struct sk_buff *skb)
{
	if (br->multicast_disabled)
		return NULL;

	if (BR_INPUT_SKB_CB(skb)->igmp)
		return NULL;

	return br_mdb_get_ext(br, skb);
}

static void br_mdb_free(struct rcu_head *head)
{
	struct net_bridge_mdb_htable *mdb =
		container_of(head, struct net_bridge_mdb_htable, rcu);
	struct net_bridge_mdb_htable *old = mdb->old;

	mdb->old = NULL;
	kfree(old->mhash);
	kfree(old);
}

static int br_mdb_copy(struct net_bridge_mdb_htable *new,
		       struct net_bridge_mdb_htable *old,
		       int elasticity)
{
	struct net_bridge_mdb_entry *mp;
	struct hlist_node *p;
	int maxlen;
	int len;
	int i;

	for (i = 0; i < old->max; i++)
		hlist_for_each_entry(mp, p, &old->mhash[i], hlist[old->ver])
			hlist_add_head(&mp->hlist[new->ver],
				       &new->mhash[br_ip_hash(new, &mp->addr)]);

	if (!elasticity)
		return 0;

	maxlen = 0;
	for (i = 0; i < new->max; i++) {
		len = 0;
		hlist_for_each_entry(mp, p, &new->mhash[i], hlist[new->ver])
			len++;
		if (len > maxlen)
			maxlen = len;
	}

	return maxlen > elasticity ? -EINVAL : 0;
}

static void br_multicast_free_pg(struct rcu_head *head)
{
	struct net_bridge_port_group *p =
		container_of(head, struct net_bridge_port_group, rcu);

	struct net_bridge_subport_req *sr;
	struct net_bridge_subport_req *sr_tmp;

	for (sr = p->subport_reqs.head; sr; sr = sr_tmp) {
		sr_tmp = sr->next;
		--p->subport_reqs.count;
		kfree(sr);
	}
	WARN_ON(p->subport_reqs.count != 0);
	kfree(p);
}

static void br_multicast_free_group(struct rcu_head *head)
{
	struct net_bridge_mdb_entry *mp =
		container_of(head, struct net_bridge_mdb_entry, rcu);

	kfree(mp);
}

static void br_multicast_group_expired(unsigned long data)
{
	struct net_bridge_mdb_entry *mp = (void *)data;
	struct net_bridge *br = mp->br;
	struct net_bridge_mdb_htable *mdb;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev) || timer_pending(&mp->timer))
		goto out;

	if (!hlist_unhashed(&mp->mglist))
		hlist_del_init(&mp->mglist);

	if (mp->ports)
		goto out;

	mdb = br->mdb;
	hlist_del_rcu(&mp->hlist[mdb->ver]);
	mdb->size--;

	call_rcu_bh(&mp->rcu, br_multicast_free_group);

out:
	spin_unlock(&br->multicast_lock);
}

static void br_multicast_remove_pg(struct net_bridge *br,
		struct net_bridge_mdb_entry *mp,
		struct net_bridge_port_group *p,
		struct net_bridge_port_group **pp)
{
	rcu_assign_pointer(*pp, p->next);

	hlist_del_init(&p->mglist);
	del_timer(&p->timer);

	if (g_fwt_mult_leave_hook) {
		g_fwt_mult_leave_hook(p->sub_port,
				p->port->dev->if_port, &p->addr);
	}

	call_rcu_bh(&p->rcu, br_multicast_free_pg);

	if (!mp->ports && hlist_unhashed(&mp->mglist) &&
		netif_running(br->dev))
		mod_timer(&mp->timer, jiffies);
}

static void br_multicast_del_pg(struct net_bridge *br,
				struct net_bridge_port_group *pg)
{
	struct net_bridge_mdb_htable *mdb = br->mdb;
	struct net_bridge_mdb_entry *mp;
	struct net_bridge_port_group *p;
	struct net_bridge_port_group **pp;

	mp = br_mdb_ip_get(mdb, &pg->addr);
	if (WARN_ON(!mp))
		return;

	for (pp = &mp->ports; (p = *pp); pp = &p->next) {
		if (p != pg || p->sub_port != pg->sub_port)
			continue;

		br_multicast_remove_pg(br, mp, p, pp);
		return;
	}

	WARN_ON(1);
}

static void br_multicast_port_group_expired(unsigned long data)
{
	struct net_bridge_port_group *pg = (void *)data;
	struct net_bridge *br = pg->port->br;

	spin_lock(&br->multicast_lock);

	if (pg->addr.proto == htons(ETH_P_IP))
		BR_SNOOP_DBG("Member port timeout: group %pI4, subport 0x%x\n",
			&pg->addr.u.ip4, pg->sub_port);

	if (!netif_running(br->dev) || timer_pending(&pg->timer) ||
	    hlist_unhashed(&pg->mglist))
		goto out;

	br_multicast_del_pg(br, pg);

out:
	spin_unlock(&br->multicast_lock);
}

static int br_mdb_rehash(struct net_bridge_mdb_htable **mdbp, int max,
			 int elasticity)
{
	struct net_bridge_mdb_htable *old = *mdbp;
	struct net_bridge_mdb_htable *mdb;
	int err;

	mdb = kmalloc(sizeof(*mdb), GFP_ATOMIC);
	if (!mdb)
		return -ENOMEM;

	mdb->max = max;
	mdb->old = old;

	mdb->mhash = kzalloc(max * sizeof(*mdb->mhash), GFP_ATOMIC);
	if (!mdb->mhash) {
		kfree(mdb);
		return -ENOMEM;
	}

	mdb->size = old ? old->size : 0;
	mdb->ver = old ? old->ver ^ 1 : 0;

	if (!old || elasticity)
		get_random_bytes(&mdb->secret, sizeof(mdb->secret));
	else
		mdb->secret = old->secret;

	if (!old)
		goto out;

	err = br_mdb_copy(mdb, old, elasticity);
	if (err) {
		kfree(mdb->mhash);
		kfree(mdb);
		return err;
	}

	call_rcu_bh(&mdb->rcu, br_mdb_free);

out:
	rcu_assign_pointer(*mdbp, mdb);

	return 0;
}

static struct sk_buff *br_ip4_multicast_alloc_query(struct net_bridge *br,
						    __be32 group)
{
	struct sk_buff *skb;
	struct igmphdr *ih;
	struct ethhdr *eth;
	struct iphdr *iph;

	skb = netdev_alloc_skb_ip_align(br->dev, sizeof(*eth) + sizeof(*iph) +
						 sizeof(*ih) + 4);
	if (!skb)
		goto out;

	skb->protocol = htons(ETH_P_IP);

	skb_reset_mac_header(skb);
	eth = eth_hdr(skb);

	memcpy(eth->h_source, br->dev->dev_addr, 6);
	eth->h_dest[0] = 1;
	eth->h_dest[1] = 0;
	eth->h_dest[2] = 0x5e;
	eth->h_dest[3] = 0;
	eth->h_dest[4] = 0;
	eth->h_dest[5] = 1;
	eth->h_proto = htons(ETH_P_IP);
	skb_put(skb, sizeof(*eth));

	skb_set_network_header(skb, skb->len);
	iph = ip_hdr(skb);

	iph->version = 4;
	iph->ihl = 6;
	iph->tos = 0xc0;
	iph->tot_len = htons(sizeof(*iph) + sizeof(*ih) + 4);
	iph->id = 0;
	iph->frag_off = htons(IP_DF);
	iph->ttl = 1;
	iph->protocol = IPPROTO_IGMP;
	iph->saddr = inet_select_addr(br->dev, 0, RT_SCOPE_LINK);
	iph->daddr = htonl(INADDR_ALLHOSTS_GROUP);
	((u8 *)&iph[1])[0] = IPOPT_RA;
	((u8 *)&iph[1])[1] = 4;
	((u8 *)&iph[1])[2] = 0;
	((u8 *)&iph[1])[3] = 0;
	ip_send_check(iph);
	skb_put(skb, 24);

	skb_set_transport_header(skb, skb->len);
	ih = igmp_hdr(skb);
	ih->type = IGMP_HOST_MEMBERSHIP_QUERY;
	ih->code = (group ? br->multicast_last_member_interval :
			    br->multicast_query_response_interval) /
		   (HZ / IGMP_TIMER_SCALE);
	ih->group = group;
	ih->csum = 0;
	ih->csum = ip_compute_csum((void *)ih, sizeof(struct igmphdr));
	skb_put(skb, sizeof(*ih));

	__skb_pull(skb, sizeof(*eth));

out:
	return skb;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static struct sk_buff *br_ip6_multicast_alloc_query(struct net_bridge *br,
						    struct in6_addr *group)
{
	struct sk_buff *skb;
	struct ipv6hdr *ip6h;
	struct mld_msg *mldq;
	struct ethhdr *eth;
	u8 *hopopt;
	unsigned long interval;

	skb = netdev_alloc_skb_ip_align(br->dev, sizeof(*eth) + sizeof(*ip6h) +
						 8 + sizeof(*mldq));
	if (!skb)
		goto out;

	skb->protocol = htons(ETH_P_IPV6);

	/* Ethernet header */
	skb_reset_mac_header(skb);
	eth = eth_hdr(skb);

	memcpy(eth->h_source, br->dev->dev_addr, 6);
	ipv6_eth_mc_map(group, eth->h_dest);
	eth->h_proto = htons(ETH_P_IPV6);
	skb_put(skb, sizeof(*eth));

	/* IPv6 header + HbH option */
	skb_set_network_header(skb, skb->len);
	ip6h = ipv6_hdr(skb);

	*(__force __be32 *)ip6h = htonl(0x60000000);
	ip6h->payload_len = htons(8 + sizeof(*mldq));
	ip6h->nexthdr = IPPROTO_HOPOPTS;
	ip6h->hop_limit = 1;
	ipv6_addr_set(&ip6h->daddr, htonl(0xff020000), 0, 0, htonl(1));
	if (ipv6_dev_get_saddr(dev_net(br->dev), br->dev, &ip6h->daddr, 0,
			       &ip6h->saddr)) {
		kfree_skb(skb);
		return NULL;
	}

	hopopt = (u8 *)(ip6h + 1);
	hopopt[0] = IPPROTO_ICMPV6;		/* next hdr */
	hopopt[1] = 0;				/* length of HbH */
	hopopt[2] = IPV6_TLV_ROUTERALERT;	/* Router Alert */
	hopopt[3] = 2;				/* Length of RA Option */
	hopopt[4] = 0;				/* Type = 0x0000 (MLD) */
	hopopt[5] = 0;
	hopopt[6] = IPV6_TLV_PAD0;		/* Pad0 */
	hopopt[7] = IPV6_TLV_PAD0;		/* Pad0 */

	skb_put(skb, sizeof(*ip6h) + 8);

	/* ICMPv6 */
	skb_set_transport_header(skb, skb->len);
	mldq = (struct mld_msg *) icmp6_hdr(skb);

	interval = ipv6_addr_any(group) ? br->multicast_last_member_interval :
					  br->multicast_query_response_interval;

	mldq->mld_type = ICMPV6_MGM_QUERY;
	mldq->mld_code = 0;
	mldq->mld_cksum = 0;
	mldq->mld_maxdelay = htons((u16)jiffies_to_msecs(interval));
	mldq->mld_reserved = 0;
	ipv6_addr_copy(&mldq->mld_mca, group);

	/* checksum */
	mldq->mld_cksum = csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
					  sizeof(*mldq), IPPROTO_ICMPV6,
					  csum_partial(mldq,
						       sizeof(*mldq), 0));
	skb_put(skb, sizeof(*mldq));

	__skb_pull(skb, sizeof(*eth));

out:
	return skb;
}
#endif

static struct sk_buff *br_multicast_alloc_query(struct net_bridge *br,
						struct br_ip *addr)
{
	switch (addr->proto) {
	case htons(ETH_P_IP):
		return br_ip4_multicast_alloc_query(br, addr->u.ip4);
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case htons(ETH_P_IPV6):
		return br_ip6_multicast_alloc_query(br, &addr->u.ip6);
#endif
	}
	return NULL;
}

static struct net_bridge_mdb_entry *br_multicast_get_group(
	struct net_bridge *br, struct net_bridge_port *port,
	struct br_ip *group, int hash)
{
	struct net_bridge_mdb_htable *mdb = br->mdb;
	struct net_bridge_mdb_entry *mp;
	struct hlist_node *p;
	unsigned count = 0;
	unsigned max;
	int elasticity;
	int err;

	hlist_for_each_entry(mp, p, &mdb->mhash[hash], hlist[mdb->ver]) {
		count++;
		if (unlikely(br_ip_equal(group, &mp->addr)))
			return mp;
	}

	elasticity = 0;
	max = mdb->max;

	if (unlikely(count > br->hash_elasticity && count)) {
		if (net_ratelimit())
			br_info(br, "Multicast hash table "
				"chain limit reached: %s\n",
				port ? port->dev->name : br->dev->name);

		elasticity = br->hash_elasticity;
	}

	if (mdb->size >= max) {
		max *= 2;
		if (unlikely(max >= br->hash_max)) {
			br_warn(br, "Multicast hash table maximum "
				"reached, disabling snooping: %s, %d\n",
				port ? port->dev->name : br->dev->name, max);
			err = -E2BIG;
disable:
			br->multicast_disabled = 1;
			goto err;
		}
	}

	if (max > mdb->max || elasticity) {
		if (mdb->old) {
			if (net_ratelimit())
				br_info(br, "Multicast hash table "
					"on fire: %s\n",
					port ? port->dev->name : br->dev->name);
			err = -EEXIST;
			goto err;
		}

		err = br_mdb_rehash(&br->mdb, max, elasticity);
		if (err) {
			br_warn(br, "Cannot rehash multicast "
				"hash table, disabling snooping: %s, %d, %d\n",
				port ? port->dev->name : br->dev->name,
				mdb->size, err);
			goto disable;
		}

		err = -EAGAIN;
		goto err;
	}

	return NULL;

err:
	mp = ERR_PTR(err);
	return mp;
}

static struct net_bridge_mdb_entry *br_multicast_new_group(
	struct net_bridge *br, struct net_bridge_port *port,
	struct br_ip *group)
{
	struct net_bridge_mdb_htable *mdb = br->mdb;
	struct net_bridge_mdb_entry *mp;
	int hash;

	if (!mdb) {
		if (br_mdb_rehash(&br->mdb, BR_HASH_SIZE, 0))
			return NULL;
		goto rehash;
	}

	hash = br_ip_hash(mdb, group);
	mp = br_multicast_get_group(br, port, group, hash);
	switch (PTR_ERR(mp)) {
	case 0:
		break;

	case -EAGAIN:
rehash:
		mdb = br->mdb;
		hash = br_ip_hash(mdb, group);
		break;

	default:
		goto out;
	}

	mp = kzalloc(sizeof(*mp), GFP_ATOMIC);
	if (unlikely(!mp))
		goto out;

	mp->br = br;
	mp->addr = *group;
	setup_timer(&mp->timer, br_multicast_group_expired,
		    (unsigned long)mp);

	hlist_add_head_rcu(&mp->hlist[mdb->ver], &mdb->mhash[hash]);
	mdb->size++;

out:
	return mp;
}

#if 0
/* for debug only */
static int br_show_group(struct net_bridge_mdb_entry *mp)
{
	struct net_bridge_port_group *p;
	struct net_bridge_port_group **pp;

	if (mp->addr.proto == htons(ETH_P_IP)) {
		printk("MCGroup[0x%x]: %d.%d.%d.%d\n", ntohs(mp->addr.proto),
				((unsigned char*)(&mp->addr.u.ip4))[0], ((unsigned char*)(&mp->addr.u.ip4))[1],
				((unsigned char*)(&mp->addr.u.ip4))[2], ((unsigned char*)(&mp->addr.u.ip4))[3]);
	} else if (mp->addr.proto == htons(ETH_P_IPV6)) {
		printk("MCGroup[0x%x]: %x:%x:%x:%x:%x:%x:%x:%x\n", ntohs(mp->addr.proto),
				mp->addr.u.ip6.s6_addr16[0], mp->addr.u.ip6.s6_addr16[1],
				mp->addr.u.ip6.s6_addr16[2], mp->addr.u.ip6.s6_addr16[3],
				mp->addr.u.ip6.s6_addr16[4], mp->addr.u.ip6.s6_addr16[5],
				mp->addr.u.ip6.s6_addr16[6], mp->addr.u.ip6.s6_addr16[7]);
	} else {
		printk("Unkown MCGroup protocol: [0x%x]", ntohs(mp->addr.proto));
	}

	for (pp = &mp->ports; (p = *pp); pp = &p->next) {
		printk("\tport[%s]sub[%d]expire[%ul]\n",
				p->port->dev->name,
				p->sub_port,
				p->timer.expires - jiffies);
	}

	return 0;
}

static int br_show_all(struct net_bridge *br)
{
	struct net_bridge_mdb_htable *mdb = br->mdb;
	struct net_bridge_mdb_entry *mp;
	struct hlist_node *p;
	int hash;

	for (hash = 0; hash < BR_HASH_SIZE; hash++) {
		hlist_for_each_entry(mp, p, &mdb->mhash[hash], hlist[mdb->ver]) {
			br_show_group(mp);
		}
	}

	return 0;
}
#endif

static void br_multicast_add_group_subport_req(struct net_bridge_port_group *p,
					       const uint8_t *src_mac)
{
	struct net_bridge_subport_req *sr;
	struct net_bridge_subport_req *first;
	struct net_bridge_subport_req **srp;

	for (srp = &p->subport_reqs.head; (sr = *srp); srp = &sr->next) {
		if (memcmp(sr->src_mac, src_mac, sizeof(sr->src_mac)) == 0) {
			/* entry exists, don't duplicate */
			return;
		}
	}

	sr = kmalloc(sizeof(*sr), GFP_ATOMIC);
	if (sr) {
		if (p->subport_reqs.count < BR_MCAST_SUBPORT_REQ_LIMIT) {
			++p->subport_reqs.count;
		} else {
			/* remove oldest entry when full */
			first = p->subport_reqs.head;
			BUG_ON(!first || !first->next);
			p->subport_reqs.head = first->next;
			kfree(first);
		}

		memcpy(sr->src_mac, src_mac, sizeof(sr->src_mac));
		sr->next = NULL;
		*srp = sr;
	}
}

static void br_multicast_leave_subport_mac(struct net_bridge_port_group *p,
					   const uint8_t *src_mac)
{
	struct net_bridge_subport_req *sr;
	struct net_bridge_subport_req **srp;

	for (srp = &p->subport_reqs.head; (sr = *srp); srp = &sr->next) {
		if (memcmp(sr->src_mac, src_mac, sizeof(sr->src_mac)) == 0) {
			*srp = sr->next;
			kfree(sr);
			--p->subport_reqs.count;
			break;
		}
	}
}

static int br_multicast_subport_empty(const struct net_bridge_port_group *p)
{
	return p->subport_reqs.head == NULL;
}

static int br_multicast_add_group(struct net_bridge *br,
				  struct net_bridge_port *port,
				  const uint8_t *src_mac,
				  u32 sub_port,
				  struct br_ip *group)
{
	struct net_bridge_mdb_entry *mp;
	struct net_bridge_port_group *p;
	struct net_bridge_port_group **pp;
	unsigned long now = jiffies;
	int err;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev) ||
	    (port && port->state == BR_STATE_DISABLED))
		goto out;

	if (port && br_is_wlan_dev(port->dev) &&
			br_fdb_check_active_sub_port_hook &&
			!br_fdb_check_active_sub_port_hook(port, sub_port))
		goto out;

	mp = br_multicast_new_group(br, port, group);
	err = PTR_ERR(mp);
	if (unlikely(IS_ERR(mp) || !mp))
		goto err;

	if (!port) {
		hlist_add_head(&mp->mglist, &br->mglist);
		mod_timer(&mp->timer, now + br->multicast_membership_interval);
		goto out;
	}

	/* hook up an update for the FWT entries */
	if (g_fwt_mult_join_hook) {
		g_fwt_mult_join_hook (sub_port, port->dev->if_port, group);
	}

	for (pp = &mp->ports; (p = *pp); pp = &p->next) {
		if (p->port == port && p->sub_port == sub_port)
			goto found;
		if ((unsigned long)p->port < (unsigned long)port)
			break;
	}

	p = kzalloc(sizeof(*p), GFP_ATOMIC);
	err = -ENOMEM;
	if (unlikely(!p))
		goto err;

	p->addr = *group;
	p->port = port;
	p->sub_port = sub_port;
	p->subport_reqs.head = NULL;
	p->subport_reqs.count = 0;
	p->next = *pp;
	hlist_add_head(&p->mglist, &port->mglist);
	setup_timer(&p->timer, br_multicast_port_group_expired,
		    (unsigned long)p);

	rcu_assign_pointer(*pp, p);

found:
	br_multicast_add_group_subport_req(p, src_mac);
	mod_timer(&p->timer, now + br->multicast_membership_interval);
	mod_timer(&mp->timer, now + br->multicast_membership_interval);
out:
	err = 0;

err:
	spin_unlock(&br->multicast_lock);
	return err;
}

static int br_ip4_multicast_add_group(struct net_bridge *br,
				      struct net_bridge_port *port,
				      const uint8_t *src_mac,
				      u32 sub_port,
				      __be32 group)
{
	struct br_ip br_group;

	if (ipv4_is_local_multicast(group))
		return 0;

	br_group.u.ip4 = group;
	br_group.proto = htons(ETH_P_IP);

	return br_multicast_add_group(br, port, src_mac, sub_port, &br_group);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static int br_ip6_multicast_add_group(struct net_bridge *br,
				      struct net_bridge_port *port,
				      const uint8_t *src_mac,
				      u32 sub_port,
				      const struct in6_addr *group)
{
	struct br_ip br_group;

	if (ipv6_is_local_multicast(group))
		return 0;

	ipv6_addr_copy(&br_group.u.ip6, group);
	br_group.proto = htons(ETH_P_IPV6);

	return br_multicast_add_group(br, port, src_mac, sub_port, &br_group);
}
#endif

static void br_multicast_router_expired(unsigned long data)
{
	struct net_bridge_port *port = (void *)data;
	struct net_bridge *br = port->br;

	spin_lock(&br->multicast_lock);
	BR_SNOOP_DBG("Router port %s(0x%x) timeout\n", port->dev->name, port->port_no);
	if (port->multicast_router != 1 ||
	    timer_pending(&port->multicast_router_timer) ||
	    hlist_unhashed(&port->rlist))
		goto out;

	memset(port->router_port_bitmap, 0, sizeof(port->router_port_bitmap));
	hlist_del_init_rcu(&port->rlist);

out:
	spin_unlock(&br->multicast_lock);
}

static void br_multicast_local_router_expired(unsigned long data)
{
}

static void __br_multicast_send_query(struct net_bridge *br,
				      struct net_bridge_port *port,
				      struct br_ip *ip)
{
	struct sk_buff *skb;

	skb = br_multicast_alloc_query(br, ip);
	if (!skb)
		return;

	if (port) {
		__skb_push(skb, sizeof(struct ethhdr));
		skb->dev = port->dev;
		NF_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
			dev_queue_xmit);
	} else
		netif_rx(skb);
}

static void br_multicast_send_query(struct net_bridge *br,
				    struct net_bridge_port *port, u32 sent)
{
	unsigned long time;
	struct br_ip br_group;

	if (!netif_running(br->dev) || br->multicast_disabled ||
	    timer_pending(&br->multicast_querier_timer))
		return;

	memset(&br_group.u, 0, sizeof(br_group.u));

	br_group.proto = htons(ETH_P_IP);
	__br_multicast_send_query(br, port, &br_group);

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	br_group.proto = htons(ETH_P_IPV6);
	__br_multicast_send_query(br, port, &br_group);
#endif

	time = jiffies;
	time += sent < br->multicast_startup_query_count ?
		br->multicast_startup_query_interval :
		br->multicast_query_interval;
	mod_timer(port ? &port->multicast_query_timer :
			 &br->multicast_query_timer, time);
}

static void br_multicast_port_query_expired(unsigned long data)
{
	struct net_bridge_port *port = (void *)data;
	struct net_bridge *br = port->br;

	if (br->multicast_router == 0 || port->multicast_router == 0)
		return;

	spin_lock(&br->multicast_lock);
	if (port->state == BR_STATE_DISABLED ||
	    port->state == BR_STATE_BLOCKING)
		goto out;

	if (port->multicast_startup_queries_sent <
	    br->multicast_startup_query_count)
		port->multicast_startup_queries_sent++;

	br_multicast_send_query(port->br, port,
				port->multicast_startup_queries_sent);

out:
	spin_unlock(&br->multicast_lock);
}

void br_multicast_add_port(struct net_bridge_port *port)
{
	port->multicast_router = 1;

	setup_timer(&port->multicast_router_timer, br_multicast_router_expired,
		    (unsigned long)port);
	setup_timer(&port->multicast_query_timer,
		    br_multicast_port_query_expired, (unsigned long)port);
}

void br_multicast_del_port(struct net_bridge_port *port)
{
	del_timer_sync(&port->multicast_router_timer);
}

static void __br_multicast_enable_port(struct net_bridge_port *port)
{
	port->multicast_startup_queries_sent = 0;

	if (try_to_del_timer_sync(&port->multicast_query_timer) >= 0 ||
	    del_timer(&port->multicast_query_timer))
		mod_timer(&port->multicast_query_timer, jiffies);
}

void br_multicast_enable_port(struct net_bridge_port *port)
{
	struct net_bridge *br = port->br;

	spin_lock(&br->multicast_lock);
	if (br->multicast_disabled || !netif_running(br->dev))
		goto out;

	__br_multicast_enable_port(port);

out:
	spin_unlock(&br->multicast_lock);
}

void br_multicast_disable_port(struct net_bridge_port *port)
{
	struct net_bridge *br = port->br;
	struct net_bridge_port_group *pg;
	struct hlist_node *p, *n;

	spin_lock(&br->multicast_lock);
	hlist_for_each_entry_safe(pg, p, n, &port->mglist, mglist)
		br_multicast_del_pg(br, pg);

	if (!hlist_unhashed(&port->rlist))
		hlist_del_init_rcu(&port->rlist);
	del_timer(&port->multicast_router_timer);
	del_timer(&port->multicast_query_timer);
	spin_unlock(&br->multicast_lock);
}

static const uint8_t *br_ether_srcmac(const struct sk_buff *skb)
{
	const struct ethhdr *eth = (const void *) skb_mac_header(skb);
	return eth->h_source;
}

static int br_ip4_multicast_igmp3_report(struct net_bridge *br,
					 struct net_bridge_port *port,
					 struct sk_buff *skb)
{
	const uint8_t *src_mac = br_ether_srcmac(skb);
	struct igmpv3_report *ih;
	struct igmpv3_grec *grec;
	int i;
	int len;
	int num;
	int type;
	int err = 0;
	__be32 group;

	if (!pskb_may_pull(skb, sizeof(*ih)))
		return -EINVAL;

	ih = igmpv3_report_hdr(skb);
	num = ntohs(ih->ngrec);
	len = sizeof(*ih);

	for (i = 0; i < num; i++) {
		len += sizeof(*grec);
		if (!pskb_may_pull(skb, len))
			return -EINVAL;

		grec = (void *)(skb->data + len - sizeof(*grec));
		group = grec->grec_mca;
		type = grec->grec_type;

		len += ntohs(grec->grec_nsrcs) * 4;
		if (!pskb_may_pull(skb, len))
			return -EINVAL;

		/* We treat this as an IGMPv2 report for now. */
		switch (type) {
		case IGMPV3_MODE_IS_INCLUDE:
		case IGMPV3_MODE_IS_EXCLUDE:
		case IGMPV3_CHANGE_TO_INCLUDE:
		case IGMPV3_CHANGE_TO_EXCLUDE:
		case IGMPV3_ALLOW_NEW_SOURCES:
		case IGMPV3_BLOCK_OLD_SOURCES:
			break;
		default:
			continue;
		}

		if ((type == IGMPV3_CHANGE_TO_INCLUDE || type == IGMPV3_MODE_IS_INCLUDE) &&
				ntohs(grec->grec_nsrcs) == 0) {
			br_ip4_multicast_leave_group(br, port, src_mac, skb->src_port, group);
		} else {
			err = br_ip4_multicast_add_group(br, port, src_mac, skb->src_port, group);
		}

		if (err)
			break;
	}

	return err;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static int br_ip6_multicast_mld2_report(struct net_bridge *br,
					struct net_bridge_port *port,
					struct sk_buff *skb)
{
	const uint8_t *src_mac = br_ether_srcmac(skb);
	struct icmp6hdr *icmp6h;
	struct mld2_grec *grec;
	int i;
	int len;
	int num;
	int err = 0;

/*
 * For some reason, we has to disable the following source code to pass MLDv2 test.
 */
#if 0
	if (!pskb_may_pull(skb, sizeof(*icmp6h)))
		return -EINVAL;
#endif

	icmp6h = icmp6_hdr(skb);
	num = ntohs(icmp6h->icmp6_dataun.un_data16[1]);
	len = sizeof(*icmp6h);

	for (i = 0; i < num; i++) {
		__be16 *nsrcs, _nsrcs;

		nsrcs = skb_header_pointer(skb,
					   len + offsetof(struct mld2_grec,
							  grec_mca),
					   sizeof(_nsrcs), &_nsrcs);

		if (!nsrcs)
			return -EINVAL;
/*
 * For some reason, we has to disable the following source code to pass MLDv2 test.
 */
#if 0
		if (!pskb_may_pull(skb,
				   len + sizeof(*grec) +
				   sizeof(struct in6_addr) * (*nsrcs)))
			return -EINVAL;
#endif
		grec = (struct mld2_grec *)(skb->data + len);
		len += sizeof(*grec) + sizeof(struct in6_addr) * (*nsrcs);

		/* Add MLDv2 support as following */
		if (__constant_ntohs(grec->grec_nsrcs)) {
			err = br_ip6_multicast_add_group(br, port,
					src_mac, skb->src_port,	&grec->grec_mca);
			break;
		} else {
			switch (grec->grec_type) {
			case MLD2_MODE_IS_EXCLUDE:
			case MLD2_CHANGE_TO_EXCLUDE:
			case MLD2_ALLOW_NEW_SOURCES:
			case MLD2_BLOCK_OLD_SOURCES:
				err = br_ip6_multicast_add_group(br, port,
						src_mac, skb->src_port,	&grec->grec_mca);
				break;
			case MLD2_MODE_IS_INCLUDE:
			case MLD2_CHANGE_TO_INCLUDE:
				br_ip6_multicast_leave_group(br, port,
						src_mac, skb->src_port, &grec->grec_mca);
				break;

			default:
				continue;
			}
		}

		if (!err)
			break;
	}

	return err;
}
#endif

/*
 * Add port to rotuer_list
 *  list is maintained ordered by pointer value
 *  and locked by br->multicast_lock and RCU
 */
static void br_multicast_add_router(struct net_bridge *br,
				    struct net_bridge_port *port)
{
	struct net_bridge_port *p;
	struct hlist_node *n, *slot = NULL;

	hlist_for_each_entry(p, n, &br->router_list, rlist) {
		if ((unsigned long) port >= (unsigned long) p)
			break;
		slot = n;
	}

	if (slot)
		hlist_add_after_rcu(slot, &port->rlist);
	else
		hlist_add_head_rcu(&port->rlist, &br->router_list);
}

static void br_multicast_mark_router(struct net_bridge *br,
		struct net_bridge_port *port, port_id src_port)
{
	unsigned long now = jiffies;

	if (!port) {
		if (br->multicast_router == 1)
			mod_timer(&br->multicast_router_timer,
				  now + br->multicast_querier_interval);
		return;
	}

	if (port->multicast_router != 1)
		return;

	if (src_port)
		br_set_sub_port_bitmap(port->router_port_bitmap, src_port);

	if (!hlist_unhashed(&port->rlist))
		goto timer;

	br_multicast_add_router(br, port);

timer:
	mod_timer(&port->multicast_router_timer,
		  now + br->multicast_querier_interval);
}

static void br_multicast_query_received(struct net_bridge *br,
		struct net_bridge_port *port, port_id src_port, int saddr)
{
	if (saddr)
		mod_timer(&br->multicast_querier_timer,
			  jiffies + br->multicast_querier_interval);
	else if (timer_pending(&br->multicast_querier_timer))
		return;

	br_multicast_mark_router(br, port, src_port);
}

static void _br_multicast_query_refresh(struct net_bridge *br,
		struct net_bridge_mdb_entry *mp, unsigned long max_delay, int group_specific)
{
	if (group_specific) {
		mp->rx_specific_query = group_specific;
		goto out;
	}

	mp->report_flood_indicator++;
	if (mp->report_flood_indicator > br->report_flood_interval)
		mp->report_flood_indicator = 0;

	/*
	 * General Query: rx_specific_query refreshed only when
	 * 1. previous Query == General Query
	 * 2. previous Query != General Query, but its MRT has expired
	 */
	if (!mp->rx_specific_query || time_after(jiffies, mp->report_target_jiffies))
		mp->rx_specific_query = group_specific;
out:
	mp->report_target_jiffies = jiffies + max_delay;
}

static void br_multicast_query_refresh(struct net_bridge *br,
		struct net_bridge_mdb_entry *mp, unsigned long max_delay)
{
	struct net_bridge_mdb_htable *mdb = br->mdb;
	struct net_bridge_mdb_entry *mp2;
	struct hlist_node *p;
	int hash;

	if (mp) {
		_br_multicast_query_refresh(br,mp, max_delay, 1);
	} else {
		if (!mdb)
			return;
		for (hash = 0; hash < mdb->max; hash++) {
			hlist_for_each_entry_rcu(mp2, p, &mdb->mhash[hash], hlist[mdb->ver])
				_br_multicast_query_refresh(br, mp2, max_delay, 0);
		}
	}
}

static int br_ip4_multicast_query(struct net_bridge *br,
				  struct net_bridge_port *port,
				  struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	struct igmphdr *ih = igmp_hdr(skb);
	struct net_bridge_mdb_entry *mp = NULL;
	struct igmpv3_query *ih3;
	struct net_bridge_port_group *p;
	struct net_bridge_port_group **pp;
	unsigned long max_delay;
	unsigned long mrt_delay;
	unsigned long now = jiffies;
	__be32 group;
	int err = 0;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev) ||
	    (port && port->state == BR_STATE_DISABLED))
		goto out;

	br_multicast_query_received(br, port, skb->src_port, !!iph->saddr);

	group = ih->group;
	if (skb->len == sizeof(*ih)) {
		max_delay = ih->code * (HZ / IGMP_TIMER_SCALE);

		if (!max_delay) {
			max_delay = 10 * HZ;
			group = 0;
		}
	} else {
		if (!pskb_may_pull(skb, sizeof(struct igmpv3_query))) {
			err = -EINVAL;
			goto out;
		}

		ih3 = igmpv3_query_hdr(skb);
		if (ih3->nsrcs)
			goto out;

		max_delay = ih3->code ?
			    IGMPV3_MRC(ih3->code) * (HZ / IGMP_TIMER_SCALE) : 1;
	}

	mrt_delay = max_delay;

	if (!group)
		goto query_refresh;

	/*
	 * IGMP Group-specific Membership Query must be
	 * copied to each member in target multicast group
	 */
	BR_INPUT_SKB_CB(skb)->ucast_fwd = 1;

	mp = br_mdb_ip4_get(br->mdb, group);
	if (!mp)
		goto out;

	max_delay *= br->multicast_last_member_count;

	if (!hlist_unhashed(&mp->mglist) &&
	    (timer_pending(&mp->timer) ?
	     time_after(mp->timer.expires, now + max_delay) :
	     try_to_del_timer_sync(&mp->timer) >= 0))
		mod_timer(&mp->timer, now + max_delay);

	for (pp = &mp->ports; (p = *pp); pp = &p->next) {
		if (timer_pending(&p->timer) ?
		    time_after(p->timer.expires, now + max_delay) :
		    try_to_del_timer_sync(&p->timer) >= 0)
			mod_timer(&p->timer, now + max_delay);
	}

query_refresh:
	/*
	 * use Max Response Time derived from packet
	 */
	br_multicast_query_refresh(br, mp, mrt_delay);
out:
	spin_unlock(&br->multicast_lock);
	return err;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static int br_ip6_multicast_query(struct net_bridge *br,
				  struct net_bridge_port *port,
				  struct sk_buff *skb)
{
	struct ipv6hdr *ip6h = ipv6_hdr(skb);
	struct mld_msg *mld = (struct mld_msg *) icmp6_hdr(skb);
	struct net_bridge_mdb_entry *mp = NULL;
	struct mld2_query *mld2q;
	struct net_bridge_port_group *p, **pp;
	unsigned long max_delay = 10 * HZ;
	unsigned long mrt_delay;
	unsigned long now = jiffies;
	struct in6_addr *group = NULL;
	int err = 0;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev) ||
	    (port && port->state == BR_STATE_DISABLED))
		goto out;

	br_multicast_query_received(br, port, skb->src_port, !ipv6_addr_any(&ip6h->saddr));

	if (skb->len == sizeof(*mld)) {
		if (!pskb_may_pull(skb, sizeof(*mld))) {
			err = -EINVAL;
			goto out;
		}
		mld = (struct mld_msg *) icmp6_hdr(skb);
		max_delay = msecs_to_jiffies(htons(mld->mld_maxdelay));
		if (max_delay)
			group = &mld->mld_mca;
	} else if (skb->len >= sizeof(*mld2q)) {
		if (!pskb_may_pull(skb, sizeof(*mld2q))) {
			err = -EINVAL;
			goto out;
		}
		mld2q = (struct mld2_query *)icmp6_hdr(skb);
		if (!mld2q->mld2q_nsrcs)
			group = &mld2q->mld2q_mca;
		max_delay = mld2q->mld2q_mrc ? MLDV2_MRC(mld2q->mld2q_mrc) : 1;
	}

	mrt_delay = max_delay;

	if (!group)
		goto query_refresh;

	mp = br_mdb_ip6_get(br->mdb, group);
	if (!mp)
		goto out;

	max_delay *= br->multicast_last_member_count;
	if (!hlist_unhashed(&mp->mglist) &&
	    (timer_pending(&mp->timer) ?
	     time_after(mp->timer.expires, now + max_delay) :
	     try_to_del_timer_sync(&mp->timer) >= 0))
		mod_timer(&mp->timer, now + max_delay);

	for (pp = &mp->ports; (p = *pp); pp = &p->next) {
		if (timer_pending(&p->timer) ?
		    time_after(p->timer.expires, now + max_delay) :
		    try_to_del_timer_sync(&p->timer) >= 0)
			mod_timer(&p->timer, now + max_delay);
	}

query_refresh:
	/*
	 * use Max Response Time derived from packet
	 */
	br_multicast_query_refresh(br, mp, mrt_delay);
out:
	spin_unlock(&br->multicast_lock);
	return err;
}
#endif

/* called with multicast lock held */
static void br_multicast_leave_subport(struct net_bridge *br,
		struct net_bridge_port *port,
		const uint8_t *src_mac,
		u32 sub_port,
		struct br_ip *group)
{
	struct net_bridge_mdb_entry *me;
	struct net_bridge_port_group *p;

	if (port == NULL)
		return;

	me = br_mdb_ip_get(br->mdb, group);
	if (me == NULL)
		return;

	for (p = me->ports; p != NULL; p = p->next) {
		if ((p->port != port) || (p->sub_port != sub_port))
			continue;

		br_multicast_leave_subport_mac(p, src_mac);

		if (!br_multicast_subport_empty(p))
			break;

		if (g_fwt_mult_leave_hook)
			g_fwt_mult_leave_hook(sub_port, port->dev->if_port, group);

		br_multicast_del_pg(br, p);
	}
}

static void br_multicast_leave_group(struct net_bridge *br,
				     struct net_bridge_port *port,
				     const uint8_t *src_mac,
				     u32 sub_port,
				     struct br_ip *group)
{
	struct net_bridge_mdb_htable *mdb;
	struct net_bridge_mdb_entry *mp;
	struct net_bridge_port_group *p;
	unsigned long now;
	unsigned long time;

	spin_lock(&br->multicast_lock);
	br_multicast_leave_subport(br, port, src_mac, sub_port, group);
	if (!netif_running(br->dev) ||
	    (port && port->state == BR_STATE_DISABLED) ||
	    timer_pending(&br->multicast_querier_timer))
		goto out;

	mdb = br->mdb;
	mp = br_mdb_ip_get(mdb, group);
	if (!mp)
		goto out;

	now = jiffies;
	time = now + br->multicast_last_member_count *
		     br->multicast_last_member_interval;

	if (!port) {
		if (!hlist_unhashed(&mp->mglist) &&
		    (timer_pending(&mp->timer) ?
		     time_after(mp->timer.expires, time) :
		     try_to_del_timer_sync(&mp->timer) >= 0)) {
			mod_timer(&mp->timer, time);
		}
		goto out;
	}

	for (p = mp->ports; p; p = p->next) {
		if (p->port != port || p->sub_port != sub_port)
			continue;

		if (!br_multicast_subport_empty(p)) {
			break;
		}

		if (!hlist_unhashed(&p->mglist) &&
		    (timer_pending(&p->timer) ?
		     time_after(p->timer.expires, time) :
		     try_to_del_timer_sync(&p->timer) >= 0)) {
			mod_timer(&p->timer, time);
		}

		break;
	}

out:
	spin_unlock(&br->multicast_lock);
}

static void br_ip4_multicast_leave_group(struct net_bridge *br,
					 struct net_bridge_port *port,
					 const uint8_t *src_mac,
					 u32 sub_port,
					 __be32 group)
{
	struct br_ip br_group;

	if (ipv4_is_local_multicast(group))
		return;

	br_group.u.ip4 = group;
	br_group.proto = htons(ETH_P_IP);

	br_multicast_leave_group(br, port, src_mac, sub_port, &br_group);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static void br_ip6_multicast_leave_group(struct net_bridge *br,
					 struct net_bridge_port *port,
					 const uint8_t *src_mac,
					 u32 sub_port,
					 const struct in6_addr *group)
{
	struct br_ip br_group;

	if (ipv6_is_local_multicast(group))
		return;

	ipv6_addr_copy(&br_group.u.ip6, group);
	br_group.proto = htons(ETH_P_IPV6);

	br_multicast_leave_group(br, port, src_mac, sub_port, &br_group);
}
#endif

/* Added by Quantenna */
void br_register_mult_cbk_t(br_leave_multicast_cbk leave_func, br_join_multicast_cbk join_func)
{
	g_fwt_mult_leave_hook = leave_func;
	g_fwt_mult_join_hook = join_func;
}
EXPORT_SYMBOL(br_register_mult_cbk_t);

static int br_multicast_ipv4_rcv(struct net_bridge *br,
				 struct net_bridge_port *port,
				 struct sk_buff *skb)
{
	const uint8_t *src_mac = br_ether_srcmac(skb);
	struct sk_buff *skb2 = skb;
	struct iphdr *iph;
	struct igmphdr *ih;
	unsigned len;
	unsigned offset;
	int err;

	/* We treat OOM as packet loss for now. */
	if (!pskb_may_pull(skb, sizeof(*iph)))
		return -EINVAL;

	iph = ip_hdr(skb);

	if (iph->ihl < 5 || iph->version != 4)
		return -EINVAL;

	if (!pskb_may_pull(skb, ip_hdrlen(skb)))
		return -EINVAL;

	iph = ip_hdr(skb);

	if (unlikely(ip_fast_csum((u8 *)iph, iph->ihl)))
		return -EINVAL;

	if (iph->protocol != IPPROTO_IGMP)
		return 0;

	len = ntohs(iph->tot_len);
	if (skb->len < len || len < ip_hdrlen(skb))
		return -EINVAL;

	if (skb->len > len) {
		skb2 = skb_clone(skb, GFP_ATOMIC);
		if (!skb2)
			return -ENOMEM;

		err = pskb_trim_rcsum(skb2, len);
		if (err)
			goto err_out;
	}

	len -= ip_hdrlen(skb2);
	offset = skb_network_offset(skb2) + ip_hdrlen(skb2);
	__skb_pull(skb2, offset);
	skb_reset_transport_header(skb2);

	err = -EINVAL;
	if (!pskb_may_pull(skb2, sizeof(*ih)))
		goto out;

	switch (skb2->ip_summed) {
	case CHECKSUM_COMPLETE:
		if (!csum_fold(skb2->csum))
			break;
		/* fall through */
	case CHECKSUM_NONE:
		skb2->csum = 0;
		if (skb_checksum_complete(skb2))
			goto out;
	}

	err = 0;

	BR_INPUT_SKB_CB(skb)->igmp = 1;
	ih = igmp_hdr(skb2);

/*
 * Allow IGMP snooping on PCIe RC ever if it is disabled
 * This is a WAR to allow IP multicast traffic to go on HDP
 */
#ifndef CONFIG_TOPAZ_PCIE_HOST
	if (br->igmp_snoop_enabled)
#endif
	{
		switch (ih->type) {
		case IGMP_HOST_MEMBERSHIP_REPORT:
		case IGMPV2_HOST_MEMBERSHIP_REPORT:
			BR_INPUT_SKB_CB(skb)->mrouters_only = 1;
			err = br_ip4_multicast_add_group(br, port,
					src_mac, skb->src_port, ih->group);
			break;
		case IGMPV3_HOST_MEMBERSHIP_REPORT:
			err = br_ip4_multicast_igmp3_report(br, port, skb2);
			break;
		case IGMP_HOST_MEMBERSHIP_QUERY:
			err = br_ip4_multicast_query(br, port, skb2);
			break;
		case IGMP_HOST_LEAVE_MESSAGE:
			br_ip4_multicast_leave_group(br, port,
					src_mac, skb->src_port, ih->group);
			break;
		}
	}

	if (!err)
		br_igmp_snoop_trace(skb->dev, iph->saddr, ih, skb2, br->igmp_snoop_enabled);
	else
		BR_SNOOP_DBG("IGMP packet is damaged\n");

	if ((skb2 != skb) && BR_INPUT_SKB_CB(skb2)->ucast_fwd)
		BR_INPUT_SKB_CB(skb)->ucast_fwd = 1;
out:
	__skb_push(skb2, offset);
err_out:
	if (skb2 != skb)
		kfree_skb(skb2);
	return err;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static int br_multicast_ipv6_rcv(struct net_bridge *br,
				 struct net_bridge_port *port,
				 struct sk_buff *skb)
{
	const uint8_t *src_mac = br_ether_srcmac(skb);
	struct sk_buff *skb2;
	struct ipv6hdr *ip6h;
	struct in6_addr *saddr, *daddr;
	struct icmp6hdr *icmp6h;
	u8 nexthdr;
	unsigned len;
	unsigned offset;
	int err = -EINVAL;

	if (!pskb_may_pull(skb, sizeof(*ip6h)))
		return -EINVAL;

	ip6h = ipv6_hdr(skb);

	/*
	 * We're interested in MLD messages only.
	 *  - Version is 6
	 *  - MLD has always Router Alert hop-by-hop option
	 *  - But we do not support jumbrograms.
	 */
	if (ip6h->version != 6 ||
	    ip6h->nexthdr != IPPROTO_HOPOPTS ||
	    ip6h->payload_len == 0)
		return 0;

	len = ntohs(ip6h->payload_len);
	if (skb->len < len)
		return -EINVAL;

	nexthdr = ip6h->nexthdr;
	offset = ipv6_skip_exthdr(skb, sizeof(*ip6h), &nexthdr);

	if (offset < 0 || nexthdr != IPPROTO_ICMPV6)
		return 0;

	/* Okay, we found ICMPv6 header */
	skb2 = skb_clone(skb, GFP_ATOMIC);
	if (!skb2)
		return -ENOMEM;

	len -= offset - sizeof(struct ipv6hdr);

	__skb_pull(skb2, offset);
	skb_reset_transport_header(skb2);

/*
 * For some reason, we has to disable the following source code to pass MLDv2 test.
 */
#if 0
	err = -EINVAL;
	if (!pskb_may_pull(skb2, sizeof(*icmp6h)))
		goto out;
#endif
	icmp6h = icmp6_hdr(skb2);

	switch (icmp6h->icmp6_type) {
	case ICMPV6_MGM_QUERY:
	case ICMPV6_MGM_REPORT:
	case ICMPV6_MGM_REDUCTION:
	case ICMPV6_MLD2_REPORT:
		break;
	default:
		err = 0;
		goto out;
	}

	/* Okay, we found MLD message. Check further. */
	if (skb2->len > len) {
		err = pskb_trim_rcsum(skb2, len);
		if (err)
			goto out;
	}

	saddr = &ip6h->saddr;
	daddr = &ip6h->daddr;
	switch (skb2->ip_summed) {
	case CHECKSUM_COMPLETE:
		if (!csum_ipv6_magic(saddr, daddr, skb2->len, IPPROTO_ICMPV6,skb2->csum))
			break;
		/*FALLTHROUGH*/
	case CHECKSUM_NONE:
		skb2->csum = ~csum_unfold(csum_ipv6_magic(saddr, daddr, skb2->len, IPPROTO_ICMPV6, 0));
		if (skb_checksum_complete(skb2)) {
			goto out;
		}
	}

	err = 0;
	BR_INPUT_SKB_CB(skb)->igmp = 1;
	switch (icmp6h->icmp6_type) {
	case ICMPV6_MGM_REPORT:
	    {
		struct mld_msg *mld = (struct mld_msg *)icmp6h;
		BR_INPUT_SKB_CB(skb)->mrouters_only = 1;
		err = br_ip6_multicast_add_group(br, port,
				src_mac, skb->src_port, &mld->mld_mca);
		break;
	    }
	case ICMPV6_MLD2_REPORT:
		err = br_ip6_multicast_mld2_report(br, port, skb2);
		break;
	case ICMPV6_MGM_QUERY:
		err = br_ip6_multicast_query(br, port, skb2);
		break;
	case ICMPV6_MGM_REDUCTION:
	    {
		struct mld_msg *mld = (struct mld_msg *)icmp6h;
		br_ip6_multicast_leave_group(br, port,
				src_mac, skb->src_port, &mld->mld_mca);
	    }
	}

out:
	__skb_push(skb2, offset);
	if (skb2 != skb)
		kfree_skb(skb2);
	return err;
}
#endif

int br_multicast_rcv(struct net_bridge *br, struct net_bridge_port *port,
		     struct sk_buff *skb)
{
	uint16_t protocol = skb->protocol;
	int ret = 0;

	BR_INPUT_SKB_CB(skb)->igmp = 0;
	BR_INPUT_SKB_CB(skb)->mrouters_only = 0;
	BR_INPUT_SKB_CB(skb)->ucast_fwd = 0;

	if (br->multicast_disabled)
		return 0;

	if (skb->protocol == htons(ETH_P_8021Q)) {
		/*
		* Need to strip VLAN tag for IGMP snooping
		* Will restore it later
		* Only consider one layer of VLAN tag
		*/
		protocol = *(uint16_t *)(skb->data + 2);
		skb_pull(skb, sizeof(struct vlan_hdr));
		skb_reset_network_header(skb);
	}

	switch (protocol) {
	case htons(ETH_P_IP):
		ret = br_multicast_ipv4_rcv(br, port, skb);
		break;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case htons(ETH_P_IPV6):
		ret = br_multicast_ipv6_rcv(br, port, skb);
		break;
#endif
	}

	if (skb->protocol == htons(ETH_P_8021Q)) {
		skb_push(skb, sizeof(struct vlan_hdr));
	}

	return ret;
}

static void br_multicast_query_expired(unsigned long data)
{
	struct net_bridge *br = (void *)data;

	if (br->multicast_router == 0)
		return;

	spin_lock(&br->multicast_lock);
	if (br->multicast_startup_queries_sent <
	    br->multicast_startup_query_count)
		br->multicast_startup_queries_sent++;

	br_multicast_send_query(br, NULL, br->multicast_startup_queries_sent);

	spin_unlock(&br->multicast_lock);
}

void br_multicast_init(struct net_bridge *br)
{
	br->hash_elasticity = 4;
	br->hash_max = 512;

	br->multicast_router = 1;
	br->multicast_last_member_count = 2;
	br->multicast_startup_query_count = 2;

	br->multicast_last_member_interval = HZ;
	br->multicast_query_response_interval = 10 * HZ;
	br->multicast_startup_query_interval = 125 * HZ / 4;
	br->multicast_query_interval = 125 * HZ;
	br->multicast_querier_interval = 255 * HZ;
	/*
	* Some types of STB don't send out IGMP report even on receiving one IGMP query.
	* The worst case is 384 seconds. We enlarge this value to enhance system robust.
	*/
	br->multicast_membership_interval = 760 * HZ;

	spin_lock_init(&br->multicast_lock);
	setup_timer(&br->multicast_router_timer,
		    br_multicast_local_router_expired, 0);
	setup_timer(&br->multicast_querier_timer,
		    br_multicast_local_router_expired, 0);
	setup_timer(&br->multicast_query_timer, br_multicast_query_expired,
		    (unsigned long)br);
}

void br_multicast_open(struct net_bridge *br)
{
	br->multicast_startup_queries_sent = 0;

	if (br->multicast_disabled)
		return;

	mod_timer(&br->multicast_query_timer, jiffies);
}

void br_multicast_stop(struct net_bridge *br)
{
	struct net_bridge_mdb_htable *mdb;
	struct net_bridge_mdb_entry *mp;
	struct hlist_node *p, *n;
	u32 ver;
	int i;

	del_timer_sync(&br->multicast_router_timer);
	del_timer_sync(&br->multicast_querier_timer);
	del_timer_sync(&br->multicast_query_timer);

	spin_lock_bh(&br->multicast_lock);
	mdb = br->mdb;
	if (!mdb)
		goto out;

	br->mdb = NULL;

	ver = mdb->ver;
	for (i = 0; i < mdb->max; i++) {
		hlist_for_each_entry_safe(mp, p, n, &mdb->mhash[i],
					  hlist[ver]) {
			del_timer(&mp->timer);
			call_rcu_bh(&mp->rcu, br_multicast_free_group);
		}
	}

	if (mdb->old) {
		spin_unlock_bh(&br->multicast_lock);
		rcu_barrier_bh();
		spin_lock_bh(&br->multicast_lock);
		WARN_ON(mdb->old);
	}

	mdb->old = mdb;
	call_rcu_bh(&mdb->rcu, br_mdb_free);

out:
	spin_unlock_bh(&br->multicast_lock);
}

int br_multicast_set_router(struct net_bridge *br, unsigned long val)
{
	int err = -ENOENT;

	spin_lock_bh(&br->multicast_lock);
	if (!netif_running(br->dev))
		goto unlock;

	switch (val) {
	case 0:
	case 2:
		del_timer(&br->multicast_router_timer);
		/* fall through */
	case 1:
		br->multicast_router = val;
		err = 0;
		break;

	default:
		err = -EINVAL;
		break;
	}

unlock:
	spin_unlock_bh(&br->multicast_lock);

	return err;
}

int br_multicast_set_port_router(struct net_bridge_port *p, unsigned long val)
{
	struct net_bridge *br = p->br;
	int err = -ENOENT;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev) || p->state == BR_STATE_DISABLED)
		goto unlock;

	switch (val) {
	case 0:
	case 1:
	case 2:
		p->multicast_router = val;
		err = 0;

		if (val < 2 && !hlist_unhashed(&p->rlist))
			hlist_del_init_rcu(&p->rlist);

		if (val == 1)
			break;

		del_timer(&p->multicast_router_timer);

		if (val == 0)
			break;

		br_multicast_add_router(br, p);
		break;

	default:
		err = -EINVAL;
		break;
	}

unlock:
	spin_unlock(&br->multicast_lock);

	return err;
}

int br_multicast_toggle(struct net_bridge *br, unsigned long val)
{
	struct net_bridge_port *port;
	int err = -ENOENT;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev))
		goto unlock;

	err = 0;
	if (br->multicast_disabled == !val)
		goto unlock;

	br->multicast_disabled = !val;
	if (br->multicast_disabled)
		goto unlock;

	if (br->mdb) {
		if (br->mdb->old) {
			err = -EEXIST;
rollback:
			br->multicast_disabled = !!val;
			goto unlock;
		}

		err = br_mdb_rehash(&br->mdb, br->mdb->max,
				    br->hash_elasticity);
		if (err)
			goto rollback;
	}

	br_multicast_open(br);
	list_for_each_entry(port, &br->port_list, list) {
		if (port->state == BR_STATE_DISABLED ||
		    port->state == BR_STATE_BLOCKING)
			continue;

		__br_multicast_enable_port(port);
	}

unlock:
	spin_unlock(&br->multicast_lock);

	return err;
}

int br_multicast_set_hash_max(struct net_bridge *br, unsigned long val)
{
	int err = -ENOENT;
	u32 old;

	spin_lock(&br->multicast_lock);
	if (!netif_running(br->dev))
		goto unlock;

	err = -EINVAL;
	if (!is_power_of_2(val))
		goto unlock;
	if (br->mdb && val < br->mdb->size)
		goto unlock;

	err = 0;

	old = br->hash_max;
	br->hash_max = val;

	if (br->mdb) {
		if (br->mdb->old) {
			err = -EEXIST;
rollback:
			br->hash_max = old;
			goto unlock;
		}

		err = br_mdb_rehash(&br->mdb, br->hash_max,
				    br->hash_elasticity);
		if (err)
			goto rollback;
	}

unlock:
	spin_unlock(&br->multicast_lock);

	return err;
}

/* Added by Quantenna */
void br_mdb_delete_by_sub_port(struct net_bridge *br,
			       const struct net_bridge_port *port,
			       port_id sub_port)
{
	struct net_bridge_mdb_htable *mdb;
	struct net_bridge_mdb_entry *mp;
	struct net_bridge_port_group *p;
	struct net_bridge_port_group **pp;
	struct hlist_node *node;
	struct hlist_node *node2;

	unsigned long flags;
	int i;

	spin_lock_irqsave(&br->multicast_lock, flags);

	mdb = br->mdb;
	if (mdb == NULL) {
		spin_unlock_irqrestore(&br->multicast_lock, flags);
		return;
	}

	for (i = 0; i < mdb->max; i++) {
		hlist_for_each_entry_safe(mp, node, node2, &mdb->mhash[i], hlist[mdb->ver]) {
			for (pp = &mp->ports; (p = *pp); pp = &p->next) {
				if (p->port != port || p->sub_port != sub_port)
					continue;
				br_multicast_remove_pg(br, mp, p, pp);
				break;
			}
		}
	}

	spin_unlock_irqrestore(&br->multicast_lock, flags);
}

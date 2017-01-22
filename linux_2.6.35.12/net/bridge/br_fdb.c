/*
 *	Forwarding database
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <asm/unaligned.h>
#include "br_private.h"
#include <net/ip.h>
#include <net/if_inet6.h>
#include <qtn/iputil.h>

static struct kmem_cache *br_fdb_cache __read_mostly;

/**
 * Added by Quantenna
 * Set cbk for each new bridge entry.
 */
#define BR_FWT_THRESH_HIGH		(3 * TOPAZ_FWT_HW_TOTAL_ENTRIES / 4)
#define BR_FWT_THRESH_CRITICAL		(TOPAZ_FWT_HW_TOTAL_ENTRIES - 400)

static br_add_entry_cbk g_add_fwt_entry_hook = NULL;
static br_delete_entry_cbk g_delete_fwt_entry_hook = NULL;
static br_fwt_ageing_time_cbk g_fwt_ageing_jiffies_hook = NULL;
static br_fwt_get_ent_cnt g_fwt_get_ent_cnt_hook = NULL;


static int fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
		      const unsigned char *addr);

static u32 fdb_salt __read_mostly;

int __init br_fdb_init(void)
{
	br_fdb_cache = kmem_cache_create("bridge_fdb_cache",
					 sizeof(struct net_bridge_fdb_entry),
					 0,
					 SLAB_HWCACHE_ALIGN, NULL);
	if (!br_fdb_cache)
		return -ENOMEM;

	get_random_bytes(&fdb_salt, sizeof(fdb_salt));
	return 0;
}

void br_fdb_fini(void)
{
	kmem_cache_destroy(br_fdb_cache);
}


/* if topology_changing then use forward_delay (default 15 sec)
 * otherwise keep longer (default 5 minutes)
 */
static inline unsigned long hold_time(const struct net_bridge *br)
{
	return br->topology_change ? br->forward_delay : br->ageing_time;
}

static int br_get_timestamp_update_fwt(struct net_bridge_fdb_entry *fdb)
{
	int fwt_ageing_jiffies;

	if (!fdb->is_static && !fdb->is_local && g_fwt_ageing_jiffies_hook) {
		fwt_ageing_jiffies = g_fwt_ageing_jiffies_hook(fdb->addr.addr);
		if (fwt_ageing_jiffies >= 0) {
			fdb->ageing_timer = jiffies - fwt_ageing_jiffies;
			return 1;
		}
	}

	return 0;
}

static inline int has_expired(const struct net_bridge *br,
				const struct net_bridge_fdb_entry *fdb)
{
	return !fdb->is_static &&
			time_before_eq(fdb->ageing_timer + hold_time(br), jiffies);
}

static inline int br_mac_hash(const unsigned char *mac)
{
	/* use 1 byte of OUI cnd 3 bytes of NIC */
	u32 key = *(u16 *)(mac + 2) + (*(u16 *)(mac + 4) << 16);

	return jhash_1word(key, fdb_salt) & (BR_HASH_SIZE - 1);
}

static void fdb_rcu_free(struct rcu_head *head)
{
	struct net_bridge_fdb_entry *ent
		= container_of(head, struct net_bridge_fdb_entry, rcu);
	kmem_cache_free(br_fdb_cache, ent);
}

static inline void fdb_delete(struct net_bridge * br, struct net_bridge_fdb_entry *f)
{
	/* Added by Quantenna */
	if (f == br->br_fdb_attached) {
		br->br_fdb_attached = NULL;
	}

	/* Added by Quantenna */
	/* delete from fwt entry */
	if (f && g_delete_fwt_entry_hook && !f->is_local){
		g_delete_fwt_entry_hook(f->addr.addr);
	}

	hlist_del_rcu(&f->hlist);
	call_rcu(&f->rcu, fdb_rcu_free);
}

void br_handle_fwt_capacity(struct net_bridge *br)
{
	uint16_t fwt_entries;

	if (!g_fwt_get_ent_cnt_hook) {
		return;
	}
	fwt_entries = g_fwt_get_ent_cnt_hook();
	if (fwt_entries < BR_FWT_THRESH_CRITICAL) {
		br->ageing_time = BR_AGE_MAX_SEC * HZ;
	} else {
		br->ageing_time = max(BR_AGE_MIN_SEC * HZ, br->ageing_time / 2);
		mod_timer(&br->gc_timer, jiffies);
	}
}

void br_fdb_changeaddr(struct net_bridge_port *p, const unsigned char *newaddr)
{
	struct net_bridge *br = p->br;
	int i;

	spin_lock_bh(&br->hash_lock);

	/* Search all chains since old address/hash is unknown */
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h;
		hlist_for_each(h, &br->hash[i]) {
			struct net_bridge_fdb_entry *f;

			f = hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (f->dst == p && f->is_local) {
				/* maybe another port has same hw addr? */
				struct net_bridge_port *op;
				list_for_each_entry(op, &br->port_list, list) {
					if (op != p &&
					    !compare_ether_addr(op->dev->dev_addr,
								f->addr.addr)) {
						f->dst = op;
						goto insert;
					}
				}

				/* delete old one */
				fdb_delete(br, f);
				goto insert;
			}
		}
	}
insert:
	/* insert new address,  may fail if invalid address or dup. */
	fdb_insert(br, p, newaddr);

	spin_unlock_bh(&br->hash_lock);
}

void br_fdb_cleanup(unsigned long _data)
{
	struct net_bridge *br = (struct net_bridge *)_data;
	int i;
	uint16_t fwt_entries;
	unsigned long delay = hold_time(br);

	if (g_fwt_get_ent_cnt_hook && !br->topology_change) {
		fwt_entries = g_fwt_get_ent_cnt_hook();
		if (fwt_entries < BR_FWT_THRESH_HIGH) {
			mod_timer(&br->gc_timer, jiffies + delay);
			return;
		}
	}

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;

		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			unsigned long this_timer;
			if (f->is_static)
				continue;
			br_get_timestamp_update_fwt(f);
			this_timer = f->ageing_timer + delay;
			if (time_before_eq(this_timer, jiffies)) {
				fdb_delete(br, f);
			}
		}
	}
	spin_unlock_bh(&br->hash_lock);

	mod_timer(&br->gc_timer, round_jiffies_up(jiffies + delay));
}

/* Completely flush all dynamic entries in forwarding database.*/
void br_fdb_flush(struct net_bridge *br)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;
		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			if (!f->is_static)
				fdb_delete(br, f);
		}
	}
	spin_unlock_bh(&br->hash_lock);
}

/* Added by Quantenna */
unsigned char br_fdb_delete_by_sub_port(struct net_bridge *br,
			       const struct net_bridge_port *p,
			       port_id sub_port)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&br->hash_lock, flags);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h, *g;

		hlist_for_each_safe(h, g, &br->hash[i]) {
			struct net_bridge_fdb_entry *f
				= hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if ((f->dst == p) &&
			    (f->sub_port == sub_port) &&
			    !f->is_static) {
				fdb_delete(br, f);
			}
		}
	}

	spin_unlock_irqrestore(&br->hash_lock, flags);

	br_mdb_delete_by_sub_port(br, p, sub_port);

	return 0;
}

/* Flush all entries refering to a specific port.
 * if do_all is set also flush static entries
 */
void br_fdb_delete_by_port(struct net_bridge *br,
			   const struct net_bridge_port *p,
			   int do_all)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h, *g;

		hlist_for_each_safe(h, g, &br->hash[i]) {
			struct net_bridge_fdb_entry *f
				= hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (f->dst != p)
				continue;

			if (f->is_static && !do_all)
				continue;
			/*
			 * if multiple ports all have the same device address
			 * then when one port is deleted, assign
			 * the local entry to other port
			 */
			if (f->is_local) {
				struct net_bridge_port *op;
				list_for_each_entry(op, &br->port_list, list) {
					if (op != p &&
					    !compare_ether_addr(op->dev->dev_addr,
								f->addr.addr)) {
						f->dst = op;
						goto skip_delete;
					}
				}
			}

			fdb_delete(br, f);
		skip_delete: ;
		}
	}
	spin_unlock_bh(&br->hash_lock);
}

/* No locking or refcounting, assumes caller has rcu_read_lock */
struct net_bridge_fdb_entry * __sram_text __br_fdb_get(struct net_bridge *br,
					  const unsigned char *addr)
{
	struct hlist_node *h;
	struct net_bridge_fdb_entry *fdb;

	hlist_for_each_entry_rcu(fdb, h, &br->hash[br_mac_hash(addr)], hlist) {
		if (!compare_ether_addr(fdb->addr.addr, addr)) {
			if (unlikely(has_expired(br, fdb))) {
				br_get_timestamp_update_fwt(fdb);
				if (time_before_eq(fdb->ageing_timer + hold_time(br), jiffies)) {
					break;
				}
			}
			return fdb;
		}
	}

	return NULL;
}

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
/* Interface used by ATM LANE hook to test
 * if an addr is on some other bridge port */
int br_fdb_test_addr(struct net_device *dev, unsigned char *addr)
{
	struct net_bridge_fdb_entry *fdb;
	int ret;

	if (!dev->br_port)
		return 0;

	rcu_read_lock();
	fdb = __br_fdb_get(dev->br_port->br, addr);
	ret = fdb && fdb->dst->dev != dev &&
		fdb->dst->state == BR_STATE_FORWARDING;
	rcu_read_unlock();

	return ret;
}
#endif /* CONFIG_ATM_LANE */

/* Interface used by ATM hook that keeps a ref count */
struct net_bridge_fdb_entry *br_fdb_get(struct net_bridge *br,
		unsigned char *addr)
{
	struct net_bridge_fdb_entry *fdb = NULL;

	rcu_read_lock();
	fdb = __br_fdb_get(br, addr);
	rcu_read_unlock();
	return fdb;
}

struct net_bridge_fdb_entry *br_fdb_get_ext(struct net_bridge *br,
			struct sk_buff *skb,
			unsigned char *addr)
{
	if (iputil_mac_is_v4_multicast(addr)
#if defined(CONFIG_IPV6)
		|| iputil_mac_is_v6_multicast(addr)
#endif
			) {
		return (struct net_bridge_fdb_entry *)br_mdb_get(br, skb);
	}

	return br_fdb_get(br, addr);
}

/* Set entry up for deletion with RCU  */
void br_fdb_put(struct net_bridge_fdb_entry *ent)
{
}

/*
 * Fill buffer with forwarding table records in
 * the API format.
 */
int br_fdb_fillbuf(struct net_bridge *br, void *buf,
		   unsigned long maxnum, unsigned long skip)
{
	struct __fdb_entry *fe = buf;
	int i, num = 0;
	struct hlist_node *h;
	struct net_bridge_fdb_entry *f;
	struct net_bridge_mdb_entry *mp;
	struct net_bridge_port_group *port_group;
	char mac_addr[ETH_ALEN];

	memset(buf, 0, maxnum*sizeof(struct __fdb_entry));

	rcu_read_lock();
	for (i = 0; i < BR_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(f, h, &br->hash[i], hlist) {
			if (num >= maxnum)
				goto out;
			br_get_timestamp_update_fwt(f);
			if (has_expired(br, f)) {
				continue;
			}

			if (skip) {
				--skip;
				continue;
			}

			/* convert from internal format to API */
			memcpy(fe->mac_addr, f->addr.addr, ETH_ALEN);

			/* due to ABI compat need to split into hi/lo */
			fe->port_no = (f->dst->port_no & 0xF) | ((f->sub_port & 0xF)<<4);
			fe->port_hi = f->dst->port_no >> 8;
			fe->is_wlan = br_is_wlan_dev(f->dst->dev);

			fe->is_local = f->is_local;
			if (!f->is_static)
				fe->ageing_timer_value = jiffies_to_clock_t(jiffies - f->ageing_timer);
			++fe;
			++num;
		}
	}

	/* Print our mcast entries also */
	if (br->mdb == 0)
		goto out;

	for (i = 0; i < BR_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(mp, h, &br->mdb->mhash[i], hlist[br->mdb->ver]) {

			if (mp->addr.proto == htons(ETH_P_IP)) {
				ip_eth_mc_map(mp->addr.u.ip4, mac_addr);
			} else if (mp->addr.proto == htons(ETH_P_IPV6)) {
				ipv6_eth_mc_map(&mp->addr.u.ip6, mac_addr);
			} else {
				printk("Unknown protocol\n");
				continue;
			}

			for (port_group = mp->ports; port_group; port_group = port_group->next) {
				if (num >= maxnum)
					goto out;

				if (skip) {
					--skip;
					continue;
				}

				memcpy(fe->mac_addr, mac_addr, ETH_ALEN);
				fe->port_no = (port_group->port->port_no & 0xF) | ((port_group->sub_port & 0xF) << 4);
				fe->is_local = 0;
				fe->ageing_timer_value = port_group->timer.expires - jiffies;
				++fe;
				++num;
			}
		}
	}

out:
	rcu_read_unlock();

	return num;
}

static inline struct net_bridge_fdb_entry *fdb_find(struct hlist_head *head,
						    const unsigned char *addr)
{
	struct hlist_node *h;
	struct net_bridge_fdb_entry *fdb;

	hlist_for_each_entry_rcu(fdb, h, head, hlist) {
		if (!compare_ether_addr(fdb->addr.addr, addr))
			return fdb;
	}
	return NULL;
}

/*
 * Added by Quantenna
 * Return the address of the attached device.
 */
unsigned char br_fdb_get_attached(struct net_bridge *br, unsigned char *addr)
{
	u_char rc = 0;

	spin_lock_bh(&br->hash_lock);

	if (br->br_fdb_attached) {
		memcpy(addr, br->br_fdb_attached->addr.addr, ETH_ALEN);
		rc = 1;
	}

	spin_unlock_bh(&br->hash_lock);

	return rc;
}

static inline fdb_is_invalid_source(struct net_bridge_port *source, port_id sub_port)
{
	/*
	 * Don't create/update the FDB if the source is a wlan device and
	 * the node does not exist.
	 */
	return (br_is_wlan_dev(source->dev) &&
			br_fdb_check_active_sub_port_hook &&
			!br_fdb_check_active_sub_port_hook(source, sub_port));
}

static struct net_bridge_fdb_entry *fdb_create(struct net_bridge *br,
					       struct hlist_head *head,
					       struct net_bridge_port *source,
					       const unsigned char *addr,
					       int is_local, port_id sub_port)
{
	struct net_bridge_fdb_entry *fdb;

	if (!is_local && fdb_is_invalid_source(source, sub_port))
	      return NULL;

	fdb = kmem_cache_alloc(br_fdb_cache, GFP_ATOMIC);
	if (fdb) {
		memcpy(fdb->addr.addr, addr, ETH_ALEN);
		hlist_add_head_rcu(&fdb->hlist, head);

		fdb->dst = source;
		fdb->is_local = is_local;
		fdb->is_static = is_local;
		fdb->ageing_timer = jiffies;
		fdb->sub_port = sub_port;
		fdb->is_constant = 0;

		/*
		 * Added by Quantenna
		 * The most recently discovered address that was not heard from a
		 * wireless interface is treated as the 'attached' device for
		 * non-bridge mode.
		 */
		if ((!fdb->is_local) &&
			(strncmp(source->dev->name, "wifi", 4) != 0)) {

			if (br->br_fdb_attached != fdb) {
#if 0
				printk(KERN_WARNING
					   "Using source %s addr %02x:%02x:%02x:%02x:%02x:%02x as the attached device\n",
					   source->dev->name,
					   addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
#endif
				br->br_fdb_attached = fdb;
			}
		}
		/* Entries that created from none traffic procedure should be inserted to FWT as well */
		if ((!fdb->is_local) && g_add_fwt_entry_hook) {
			g_add_fwt_entry_hook(addr, source->dev->if_port, sub_port, NULL);
		}
	}
	return fdb;
}

static int fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
		  const unsigned char *addr)
{
	struct hlist_head *head = &br->hash[br_mac_hash(addr)];
	struct net_bridge_fdb_entry *fdb;

	if (!is_valid_ether_addr(addr))
		return -EINVAL;

	fdb = fdb_find(head, addr);
	if (fdb) {
		/* it is okay to have multiple ports with same
		 * address, just use the first one.
		 */
		if (fdb->is_local)
			return 0;
		br_warn(br, "adding interface %s with same address "
		       "as a received packet\n",
		       source->dev->name);
		fdb_delete(br, fdb);
	}

	if (!fdb_create(br, head, source, addr, 1, 0))
		return -ENOMEM;

	return 0;
}

/* Added by Quantenna */
void br_register_hooks_cbk_t(br_add_entry_cbk add_func, br_delete_entry_cbk delete_func,
		br_fwt_ageing_time_cbk ageing_func, br_fwt_get_ent_cnt entries_num)
{
	g_add_fwt_entry_hook = add_func;
	g_delete_fwt_entry_hook = delete_func;
	g_fwt_ageing_jiffies_hook = ageing_func;
	g_fwt_get_ent_cnt_hook = entries_num;
}
EXPORT_SYMBOL(br_register_hooks_cbk_t);

int br_fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
		  const unsigned char *addr)
{
	int ret;

	spin_lock_bh(&br->hash_lock);
	ret = fdb_insert(br, source, addr);
	spin_unlock_bh(&br->hash_lock);
	return ret;
}

/* TDLS-TODO This has been largely copied from br_fdb_update, refactor */
void br_fdb_update_const(struct net_bridge *br, struct net_bridge_port *source,
		   const unsigned char *addr, port_id sub_port)
{
	struct hlist_head *head = &br->hash[br_mac_hash(addr)];
	struct net_bridge_fdb_entry *fdb;

	spin_lock(&br->hash_lock);

	fdb = fdb_find(head, addr);
	if (fdb) {
		fdb->is_constant = 0;
		br_fdb_update(br, source, addr, sub_port);
	} else {
		fdb = fdb_create(br, head, source, addr, 0, sub_port);
	}

	if (likely(fdb))
		fdb->is_constant = 1;

	spin_unlock(&br->hash_lock);
}

void __sram_text br_fdb_update(struct net_bridge *br, struct net_bridge_port *source,
		   const unsigned char *addr, port_id sub_port)
{
	struct hlist_head *head = &br->hash[br_mac_hash(addr)];
	struct net_bridge_fdb_entry *fdb = NULL;

	/* some users want to always flood. */
	if (hold_time(br) == 0)
		return;

	/* ignore packets unless we are using this port */
	if (!(source->state == BR_STATE_LEARNING ||
	      source->state == BR_STATE_FORWARDING))
		return;

	if (fdb_is_invalid_source(source, sub_port))
		return;

	fdb = fdb_find(head, addr);
	if (likely(fdb)) {
		/* attempt to update an entry for a local interface */
		if (unlikely(fdb->is_local)) {
			if (net_ratelimit())
				br_warn(br, "received packet on %s with "
					"own address as source address\n",
					source->dev->name);
		} else {
			/* fastpath: update of existing entry */
			fdb->dst = source;
			fdb->ageing_timer = jiffies;
			if (likely(!fdb->is_constant))
				fdb->sub_port = sub_port;
		}
	} else {
		spin_lock(&br->hash_lock);
		fdb = fdb_create(br, head, source, addr, 0, sub_port);
		/* else  we lose race and someone else inserts
		 * it first, don't bother updating
		 */
		spin_unlock(&br->hash_lock);
	}

	if (fdb && !fdb->is_local && !fdb->is_constant) {
		if (g_add_fwt_entry_hook) {
			g_add_fwt_entry_hook(addr, source->dev->if_port, sub_port, NULL);
		}
	}
}

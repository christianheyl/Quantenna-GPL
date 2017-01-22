/*
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

#ifndef _BR_PRIVATE_H
#define _BR_PRIVATE_H

#include <linux/netdevice.h>
#include <linux/if_bridge.h>
#include <net/route.h>
#include "br_public.h"
#include <qtn/br_types.h>

#define BR_HOLD_TIME (1*HZ)

#define BR_PORT_BITS	10
#define BR_MAX_PORTS	(1<<BR_PORT_BITS)

#define BR_VERSION	"2.3"

/* Path to usermode spanning tree program */
#define BR_STP_PROG	"/sbin/bridge-stp"

#define BR_AGE_MAX_SEC (60 * 60 * 2)
#define BR_AGE_MIN_SEC (30 * 60)
enum
{
	MCAST_FWD_BLOCK = 0,
	MCAST_FWD_ALLOW = 1,
};

struct net_bridge_mcast_reqs
{
	mac_addr ucast_req_addr;
	unsigned char forward;
	struct list_head list;
};

struct net_bridge_subport_req {
	uint8_t src_mac[ETH_ALEN];
	struct net_bridge_subport_req *next;
};

struct net_bridge_port_group {
	struct net_bridge_port		*port;
	struct net_bridge_port_group	*next;
	struct hlist_node		mglist;
	struct rcu_head			rcu;
	struct timer_list		timer;
	struct timer_list		query_timer;
	struct br_ip			addr;
	u32				queries_sent;
	u32				sub_port;
	struct {
		struct net_bridge_subport_req	*head;
		unsigned int		count;
	} subport_reqs;
};

#define BR_MCAST_SUBPORT_REQ_LIMIT	32

struct net_bridge_mdb_entry
{
	struct hlist_node		hlist[2];
	struct hlist_node		mglist;
	struct net_bridge		*br;
	struct net_bridge_port_group	*ports;
	struct rcu_head			rcu;
	struct timer_list		timer;
	struct timer_list		query_timer;
	struct br_ip			addr;
	u32				queries_sent;
	u32				report_flood_indicator;
	unsigned long			report_target_jiffies;
	u32				rx_specific_query;
};

struct net_bridge_mdb_htable
{
	struct hlist_head		*mhash;
	struct rcu_head			rcu;
	struct net_bridge_mdb_htable	*old;
	u32				size;
	u32				max;
	u32				secret;
	u32				ver;
};

struct br_input_skb_cb {
	struct net_device *brdev;
#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
	int igmp;
	int mrouters_only;
	int ucast_fwd;
#endif
};

#define BR_INPUT_SKB_CB(__skb)	((struct br_input_skb_cb *)(__skb)->cb)

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
# define BR_INPUT_SKB_CB_MROUTERS_ONLY(__skb)	(BR_INPUT_SKB_CB(__skb)->mrouters_only)
#define BR_INPUT_SKB_CB_UCAST_FWD(__skb)	(BR_INPUT_SKB_CB(__skb)->ucast_fwd)
#else
# define BR_INPUT_SKB_CB_MROUTERS_ONLY(__skb)	(0)
#define BR_INPUT_SKB_CB_UCAST_FWD(__skb)	(0)
#endif

#define br_printk(level, br, format, args...)	\
	printk(level "%s: " format, (br)->dev->name, ##args)

#define br_err(__br, format, args...)			\
	br_printk(KERN_ERR, __br, format, ##args)
#define br_warn(__br, format, args...)			\
	br_printk(KERN_WARNING, __br, format, ##args)
#define br_notice(__br, format, args...)		\
	br_printk(KERN_NOTICE, __br, format, ##args)
#define br_info(__br, format, args...)			\
	br_printk(KERN_INFO, __br, format, ##args)

#define br_debug(br, format, args...)			\
	pr_debug("%s: " format,  (br)->dev->name, ##args)

extern struct notifier_block br_device_notifier;
extern const u8 br_group_address[ETH_ALEN];

/* called under bridge lock */
static inline int br_is_root_bridge(const struct net_bridge *br)
{
	return !memcmp(&br->bridge_id, &br->designated_root, 8);
}

/* br_device.c */
extern void br_dev_setup(struct net_device *dev);
extern netdev_tx_t br_dev_xmit(struct sk_buff *skb,
			       struct net_device *dev);
#ifdef CONFIG_NET_POLL_CONTROLLER
extern void br_netpoll_cleanup(struct net_device *dev);
extern void br_netpoll_enable(struct net_bridge *br,
			      struct net_device *dev);
extern void br_netpoll_disable(struct net_bridge *br,
			       struct net_device *dev);
#else
#define br_netpoll_cleanup(br)
#define br_netpoll_enable(br, dev)
#define br_netpoll_disable(br, dev)

#endif

/* br_fdb.c */
extern int br_fdb_init(void);
extern void br_fdb_fini(void);
extern void br_fdb_flush(struct net_bridge *br);
extern void br_fdb_changeaddr(struct net_bridge_port *p,
			      const unsigned char *newaddr);
extern void br_fdb_cleanup(unsigned long arg);
extern void br_handle_fwt_capacity(struct net_bridge *br);
extern void br_fdb_delete_by_port(struct net_bridge *br,
				  const struct net_bridge_port *p, int do_all);
extern struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,
						 const unsigned char *addr);
extern struct net_bridge_fdb_entry *br_fdb_get(struct net_bridge *br,
					       unsigned char *addr);
extern struct net_bridge_fdb_entry *br_fdb_get_ext(struct net_bridge *br,
						struct sk_buff *skb,
					       unsigned char *addr);
extern void br_fdb_put(struct net_bridge_fdb_entry *ent);
extern int br_fdb_test_addr(struct net_device *dev, unsigned char *addr);
extern unsigned char br_fdb_get_attached(struct net_bridge *br, unsigned char *addr);
extern unsigned char br_fdb_delete_by_sub_port(struct net_bridge *br,
				      const struct net_bridge_port *p,
				      port_id sub_port);
extern int br_fdb_fillbuf(struct net_bridge *br, void *buf,
			  unsigned long count, unsigned long off);
extern int br_fdb_insert(struct net_bridge *br,
			 struct net_bridge_port *source,
			 const unsigned char *addr);
extern void br_fdb_update_const(struct net_bridge *br,
			  struct net_bridge_port *source,
			  const unsigned char *addr, port_id sub_port);
extern void br_fdb_update(struct net_bridge *br,
			  struct net_bridge_port *source,
			  const unsigned char *addr, port_id sub_port);
extern void br_mcast_fdb_update(struct net_bridge *br, struct net_bridge_port *source,
		   const unsigned char *mcast_addr, const unsigned char *ucast_addr,port_id sub_port,
			int op_join);
/* br_forward.c */
extern void br_deliver(const struct net_bridge_port *to,
		struct sk_buff *skb);
extern int br_dev_queue_push_xmit(struct sk_buff *skb);
extern void br_forward(const struct net_bridge_port *to,
		struct sk_buff *skb, struct sk_buff *skb0);
extern int br_forward_finish(struct sk_buff *skb);
extern void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb);
extern void br_flood_forward(struct net_bridge *br, struct sk_buff *skb,
			     struct sk_buff *skb2);
			
/* br_if.c */
extern void br_port_carrier_check(struct net_bridge_port *p);
extern int br_add_bridge(struct net *net, const char *name);
extern int br_del_bridge(struct net *net, const char *name);
extern void br_net_exit(struct net *net);
extern int br_add_if(struct net_bridge *br,
	      struct net_device *dev);
extern int br_del_if(struct net_bridge *br,
	      struct net_device *dev);
extern int br_min_mtu(const struct net_bridge *br);
extern void br_features_recompute(struct net_bridge *br);

/* br_input.c */
extern int br_handle_frame_finish(struct sk_buff *skb);
extern struct sk_buff *br_handle_frame(struct net_bridge_port *p,
				       struct sk_buff *skb);

/* br_ioctl.c */
extern int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
extern int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *arg);

/* br_multicast.c */
#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
extern int br_multicast_rcv(struct net_bridge *br,
			    struct net_bridge_port *port,
			    struct sk_buff *skb);
extern struct net_bridge_mdb_entry *br_mdb_get(struct net_bridge *br,
					       struct sk_buff *skb);
extern void br_multicast_add_port(struct net_bridge_port *port);
extern void br_multicast_del_port(struct net_bridge_port *port);
extern void br_multicast_enable_port(struct net_bridge_port *port);
extern void br_multicast_disable_port(struct net_bridge_port *port);
extern void br_multicast_init(struct net_bridge *br);
extern void br_multicast_open(struct net_bridge *br);
extern void br_multicast_stop(struct net_bridge *br);
extern void br_multicast_deliver(struct net_bridge_mdb_entry *mdst,
				 struct sk_buff *skb);
extern void br_multicast_forward(struct net_bridge_mdb_entry *mdst,
				 struct sk_buff *skb, struct sk_buff *skb2);
extern int br_multicast_set_router(struct net_bridge *br, unsigned long val);
extern int br_multicast_set_port_router(struct net_bridge_port *p,
					unsigned long val);
extern int br_multicast_toggle(struct net_bridge *br, unsigned long val);
extern int br_multicast_set_hash_max(struct net_bridge *br, unsigned long val);
extern void br_mdb_delete_by_sub_port(struct net_bridge *br, const struct net_bridge_port *port,
					port_id sub_port);
static inline bool br_multicast_is_router(struct net_bridge *br)
{
	return br->multicast_router == 2 ||
	       (br->multicast_router == 1 &&
		timer_pending(&br->multicast_router_timer));
}
static inline void br_reset_sub_port_bitmap(uint32_t *sub_port_map, port_id sub_port)
{
	port_id unmap_sub_port;

	unmap_sub_port = BR_SUBPORT_UNMAP(sub_port);
	sub_port_map[BR_SUBPORT_IDX(unmap_sub_port)] &= ~BR_SUBPORT_BITMAP(unmap_sub_port);
}

extern void br_report_flood(struct net_bridge *br,
		struct net_bridge_mdb_entry *mp, struct sk_buff *skb);
extern struct net_bridge_mdb_entry *br_mdb_get_ext(struct net_bridge *br, struct sk_buff *skb);
extern void br_multicast_copy_to_sub_ports(struct net_bridge *br,
		struct net_bridge_mdb_entry *mdst, struct sk_buff *skb);
#else
static inline int br_multicast_rcv(struct net_bridge *br,
				   struct net_bridge_port *port,
				   struct sk_buff *skb)
{
	return 0;
}

static inline struct net_bridge_mdb_entry *br_mdb_get(struct net_bridge *br,
						      struct sk_buff *skb)
{
	return NULL;
}

static inline void br_multicast_add_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_del_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_enable_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_disable_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_init(struct net_bridge *br)
{
}

static inline void br_multicast_open(struct net_bridge *br)
{
}

static inline void br_multicast_stop(struct net_bridge *br)
{
}

static inline void br_multicast_deliver(struct net_bridge_mdb_entry *mdst,
					struct sk_buff *skb)
{
}

static inline void br_multicast_forward(struct net_bridge_mdb_entry *mdst,
					struct sk_buff *skb,
					struct sk_buff *skb2)
{
}

static inline void br_report_flood(struct net_bridge *br,
		struct net_bridge_mdb_entry *mp, struct sk_buff *skb)
{
}

static inline bool br_multicast_is_router(struct net_bridge *br)
{
	return 0;
}

static inline void br_reset_sub_port_bitmap(uint32_t *sub_port_map, port_id sub_port)
{
}

static inline struct net_bridge_mdb_entry *br_mdb_get_ext(struct net_bridge *br,
		struct sk_buff *skb)
{
	return NULL;
}
#endif

/* br_netfilter.c */
#ifdef CONFIG_BRIDGE_NETFILTER
extern int br_netfilter_init(void);
extern void br_netfilter_fini(void);
extern void br_netfilter_rtable_init(struct net_bridge *);
#else
#define br_netfilter_init()	(0)
#define br_netfilter_fini()	do { } while(0)
#define br_netfilter_rtable_init(x)
#endif

/* br_stp.c */
extern void br_log_state(const struct net_bridge_port *p);
extern struct net_bridge_port *br_get_port(struct net_bridge *br,
					   u16 port_no);
extern void br_init_port(struct net_bridge_port *p);
extern void br_become_designated_port(struct net_bridge_port *p);

/* br_stp_if.c */
extern void br_stp_enable_bridge(struct net_bridge *br);
extern void br_stp_disable_bridge(struct net_bridge *br);
extern void br_stp_set_enabled(struct net_bridge *br, unsigned long val);
extern void br_stp_enable_port(struct net_bridge_port *p);
extern void br_stp_disable_port(struct net_bridge_port *p);
extern void br_stp_recalculate_bridge_id(struct net_bridge *br);
extern void br_stp_change_bridge_id(struct net_bridge *br, const unsigned char *a);
extern void br_stp_set_bridge_priority(struct net_bridge *br,
				       u16 newprio);
extern void br_stp_set_port_priority(struct net_bridge_port *p,
				     u8 newprio);
extern void br_stp_set_path_cost(struct net_bridge_port *p,
				 u32 path_cost);
extern ssize_t br_show_bridge_id(char *buf, const struct bridge_id *id);

/* br_stp_bpdu.c */
struct stp_proto;
extern void br_stp_rcv(const struct stp_proto *proto, struct sk_buff *skb,
		       struct net_device *dev);

/* br_stp_timer.c */
extern void br_stp_timer_init(struct net_bridge *br);
extern void br_stp_port_timer_init(struct net_bridge_port *p);
extern unsigned long br_timer_value(const struct timer_list *timer);

/* br.c */
#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
extern int (*br_fdb_test_addr_hook)(struct net_device *dev, unsigned char *addr);
#endif
extern int (*br_fdb_fillbuf_hook)(struct net_bridge *br,
				  void *buf,
				  unsigned long maxnum,
				  unsigned long skip);

/* br_netlink.c */
extern int br_netlink_init(void);
extern void br_netlink_fini(void);
extern void br_ifinfo_notify(int event, struct net_bridge_port *port);

#ifdef CONFIG_SYSFS
/* br_sysfs_if.c */
extern const struct sysfs_ops brport_sysfs_ops;
extern int br_sysfs_addif(struct net_bridge_port *p);
extern int br_sysfs_renameif(struct net_bridge_port *p);

/* br_sysfs_br.c */
extern int br_sysfs_addbr(struct net_device *dev);
extern void br_sysfs_delbr(struct net_device *dev);

#else

#define br_sysfs_addif(p)	(0)
#define br_sysfs_renameif(p)	(0)
#define br_sysfs_addbr(dev)	(0)
#define br_sysfs_delbr(dev)	do { } while(0)
#endif /* CONFIG_SYSFS */

extern unsigned int g_br_igmp_snoop_dbg;

#define BR_SNOOP_DBG(fmt, ...)	do {			\
	if (g_br_igmp_snoop_dbg)			\
		if (net_ratelimit())			\
			printk(fmt, ##__VA_ARGS__);	\
} while(0)

extern void br_multicast_mdb_dump(struct net_bridge_mdb_entry *me);

#endif

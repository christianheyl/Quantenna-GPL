/*
 *	Public APIs declaration.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _BR_PUBLIC_H
#define _BR_PUBLIC_H

#include <qtn/br_types.h>

#define BR_HASH_BITS 8
#define BR_HASH_SIZE (1 << BR_HASH_BITS)

typedef struct bridge_id bridge_id;
typedef struct mac_addr mac_addr;
typedef __u16 port_id;

struct bridge_id
{
	unsigned char	prio[2];
	unsigned char	addr[6];
};

struct mac_addr
{
	unsigned char	addr[6];
};

struct net_bridge_fdb_entry
{
	struct hlist_node		hlist;
	struct net_bridge_port		*dst;

	port_id sub_port;

	struct rcu_head			rcu;
	unsigned long			ageing_timer;
	mac_addr			addr;
	unsigned char			is_local;
	unsigned char			is_static;
	unsigned char			is_constant;
};

struct net_bridge_port
{
	struct net_bridge		*br;
	struct net_device		*dev;
	struct list_head		list;

	/* STP */
	u8				priority;
	u8				state;
	u16				port_no;
	unsigned char			topology_change_ack;
	unsigned char			config_pending;
	port_id				port_id;
	port_id				designated_port;
	bridge_id			designated_root;
	bridge_id			designated_bridge;
	u32				path_cost;
	u32				designated_cost;

	struct timer_list		forward_delay_timer;
	struct timer_list		hold_timer;
	struct timer_list		message_age_timer;
	struct kobject			kobj;
	struct rcu_head			rcu;

	unsigned long			flags;
#define BR_HAIRPIN_MODE		0x00000001

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
#define	BR_SUB_PORT_BITMAP_SIZE		(4)
	u32				multicast_startup_queries_sent;
	unsigned char			multicast_router;
	struct timer_list		multicast_router_timer;
	struct timer_list		multicast_query_timer;
	struct hlist_head		mglist;
	struct hlist_node		rlist;
	u32				router_port_bitmap[BR_SUB_PORT_BITMAP_SIZE];
#endif

#ifdef CONFIG_SYSFS
	char				sysfs_name[IFNAMSIZ];
#endif
};

#define	BR_SUBPORT_UNMAP(__SUB_PORT__)		((__SUB_PORT__) &~ 0x8000)
#define	BR_SUBPORT_MAP(__SUB_PORT__)		((__SUB_PORT__) | 0x8000)

#define	BR_SUBPORT_IDX(__SUB_PORT__)		(((__SUB_PORT__) >> 5) & 0x7)
#define	BR_SUBPORT_BITMAP(__SUB_PORT__)		(1U << ((__SUB_PORT__) & 0x1F))
#define	BR_SUBPORT(__IDX__, __BIT__)		(((__IDX__) << 5) + (__BIT__))

struct br_cpu_netstats {
	unsigned long	rx_packets;
	unsigned long	rx_bytes;
	unsigned long	tx_packets;
	unsigned long	tx_bytes;
};

struct net_bridge
{
	spinlock_t			lock;
	struct list_head		port_list;
	struct net_device		*dev;
	struct net_device_stats		statistics;

	struct br_cpu_netstats __percpu *stats;
	spinlock_t			hash_lock;
	struct hlist_head		hash[BR_HASH_SIZE];
	struct hlist_head		mcast_hash[BR_HASH_SIZE];
	unsigned long			feature_mask;
#ifdef CONFIG_BRIDGE_NETFILTER
	struct rtable			fake_rtable;
#endif
	unsigned long			flags;
#define BR_SET_MAC_ADDR		0x00000001

	/* STP */
	bridge_id			designated_root;
	bridge_id			bridge_id;
	u32				root_path_cost;
	unsigned long			max_age;
	unsigned long			hello_time;
	unsigned long			forward_delay;
	unsigned long			bridge_max_age;
	unsigned long			ageing_time;
	unsigned long			bridge_hello_time;
	unsigned long			bridge_forward_delay;

	u8				group_addr[ETH_ALEN];
	u16				root_port;

	enum {
		BR_NO_STP,		/* no spanning tree */
		BR_KERNEL_STP,		/* old STP in kernel */
		BR_USER_STP,		/* new RSTP in userspace */
	} stp_enabled;

	unsigned char			topology_change;
	unsigned char			topology_change_detected;

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
	unsigned char			multicast_router;

	u8				multicast_disabled:1;

	u32				hash_elasticity;
	u32				hash_max;
#define BR_ALWAYS_FLOOD_REPORT		(-1U)
	u32				report_flood_interval;

	u32				multicast_last_member_count;
	u32				multicast_startup_queries_sent;
	u32				multicast_startup_query_count;

	unsigned long			multicast_last_member_interval;
	unsigned long			multicast_membership_interval;
	unsigned long			multicast_querier_interval;
	unsigned long			multicast_query_interval;
	unsigned long			multicast_query_response_interval;
	unsigned long			multicast_startup_query_interval;

	spinlock_t			multicast_lock;
	struct net_bridge_mdb_htable	*mdb;
	struct hlist_head		router_list;
	struct hlist_head		mglist;

	struct timer_list		multicast_router_timer;
	struct timer_list		multicast_querier_timer;
	struct timer_list		multicast_query_timer;
#endif

	struct timer_list		hello_timer;
	struct timer_list		tcn_timer;
	struct timer_list		topology_change_timer;
	struct timer_list		gc_timer;
	struct kobject			*ifobj;
		/* address of attached device when in 3-addr mode */
	struct net_bridge_fdb_entry *br_fdb_attached;
	enum {
		BR_IGMP_SNOOP_DISABLED,
		BR_IGMP_SNOOP_ENABLED,
	} igmp_snoop_enabled;
	enum {
		BR_SSDP_FLOOD_DISABLED,
		BR_SSDP_FLOOD_ENABLED
	} ssdp_flood_state;
};

/* br.c */
extern unsigned char (*br_fdb_get_attached_hook)(struct net_bridge *br,
                                                 unsigned char *addr);
extern unsigned char (*br_fdb_update_const_hook)(struct net_bridge *br,
							struct net_bridge_port *source,
							const unsigned char *addr, uint32_t sub_port);
extern unsigned char (*br_fdb_delete_by_sub_port_hook)(struct net_bridge *br,
						       const struct net_bridge_port *p,
						       port_id sub_port);
extern int (*br_fdb_get_active_sub_port_hook)(const struct net_bridge_port *p,
						       uint32_t *sub_port_bitmap, int size);
extern int (*br_fdb_check_active_sub_port_hook)(const struct net_bridge_port *p,
						       const uint32_t sub_port);
extern struct net_bridge_fdb_entry *(*br_fdb_get_hook)(struct net_bridge *br,
							struct sk_buff *skb,
						       unsigned char *addr);
extern void (*br_fdb_put_hook)(struct net_bridge_fdb_entry *ent);

/* new added APIs */
void br_vlan_set_promisc(int enable);
void br_set_ap_isolate(int enable);
int br_get_ap_isolate(void);

typedef int (*br_add_entry_cbk)(const uint8_t *mac_be, uint8_t port_id,
		uint8_t sub_port, const struct br_ip *group);
typedef int (*br_delete_entry_cbk)(const uint8_t *mac_be);
typedef int (*br_fwt_ageing_time_cbk)(const uint8_t *mac_be);
typedef int (*br_mult_get_fwt_ts_hook)(const struct br_ip *group);
typedef int (*br_acquire_time_res_cbk)(void);
typedef int (*br_join_multicast_cbk)(uint8_t sub_port, uint8_t port_id, const struct br_ip *group);
typedef int (*br_leave_multicast_cbk)(uint8_t sub_port, uint8_t port_id, const struct br_ip *group);
typedef uint16_t(*br_fwt_get_ent_cnt)(void);

/* register CBK for copying bridge database to fwt interface */
void br_register_hooks_cbk_t(br_add_entry_cbk add_func, br_delete_entry_cbk delete_func,
		br_fwt_ageing_time_cbk ageing_func, br_fwt_get_ent_cnt entries_cnt);
void br_register_mult_cbk_t(br_leave_multicast_cbk leave_func, br_join_multicast_cbk join_func);

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
static inline void br_set_sub_port_bitmap(uint32_t *sub_port_map, port_id sub_port)
{
	port_id unmap_sub_port;

	unmap_sub_port = BR_SUBPORT_UNMAP(sub_port);
	BUG_ON(BR_SUB_PORT_BITMAP_SIZE < BR_SUBPORT_IDX(unmap_sub_port));
	sub_port_map[BR_SUBPORT_IDX(unmap_sub_port)] |= BR_SUBPORT_BITMAP(unmap_sub_port);
}
#else
static inline void br_set_sub_port_bitmap(uint32_t *sub_port_map, port_id sub_port)
{
}
#endif
static inline int br_is_wlan_dev(struct net_device *dev)
{
	return !!(dev->qtn_flags & QTN_FLAG_WIFI_DEVICE);
}
#endif


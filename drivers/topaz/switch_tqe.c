/**
 * Copyright (c) 2011-2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/kernel.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/system.h>
#include <qtn/dmautil.h>
#include <drivers/ruby/dma_cache_ops.h>

#include "topaz_test.h"
#include <qtn/topaz_fwt_sw.h>
#include <qtn/topaz_fwt_db.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_tqe.h>
#include <qtn/topaz_hbm_cpuif.h>
#include <qtn/topaz_hbm.h>
#include <qtn/topaz_fwt.h>
#include <qtn/topaz_vlan_cpuif.h>
#include "net80211/ieee80211.h"
#include "net80211/if_ethersubr.h"
#include <qtn/qtn_net_packet.h>
#include <qtn/qdrv_sch.h>
#include <qtn/topaz_congest_queue.h>
#include <qtn/qtn_wowlan.h>
#include <qtn/iputil.h>
#include <qtn/mproc_sync.h>
#include <qtn/qtn_vlan.h>

int g_dscp_flag = 0;
int g_dscp_value[2];
uint16_t g_wowlan_host_state = 0;
uint16_t g_wowlan_match_type = 0;
uint16_t g_wowlan_l2_ether_type = 0x0842;
uint16_t g_wowlan_l3_udp_port = 0xffff;
uint8_t g_l2_ext_filter = 0;
uint8_t g_l2_ext_filter_port = TOPAZ_TQE_NUM_PORTS;
EXPORT_SYMBOL(g_l2_ext_filter);
EXPORT_SYMBOL(g_l2_ext_filter_port);
EXPORT_SYMBOL(g_wowlan_host_state);
EXPORT_SYMBOL(g_wowlan_match_type);
EXPORT_SYMBOL(g_wowlan_l2_ether_type);
EXPORT_SYMBOL(g_wowlan_l3_udp_port);
EXPORT_SYMBOL(g_dscp_flag);
EXPORT_SYMBOL(g_dscp_value);

int tqe_sem_en = 0;
module_param(tqe_sem_en, int, S_IRWXU);

struct tqe_netdev_priv {
	struct napi_struct napi;
	struct net_device_stats stats;

	struct topaz_congest_queue *congest_queue;

	ALIGNED_DMA_DESC(union, topaz_tqe_cpuif_descr) rx;
};

static tqe_fwt_get_mcast_hook g_tqe_fwt_get_mcast_hook = NULL;
static tqe_fwt_get_mcast_ff_hook g_tqe_fwt_get_mcast_ff_hook = NULL;
static tqe_fwt_get_ucast_hook g_tqe_fwt_get_ucast_hook = NULL;
static tqe_fwt_false_miss_hook g_tqe_fwt_false_miss_hook = NULL;
static tqe_fwt_get_from_mac_hook g_tqe_fwt_get_from_mac_hook = NULL;

static tqe_mac_reserved_hook g_tqe_mac_reserved_hook = NULL;
static inline void __sram_text tqe_rx_pkt_drop(const union topaz_tqe_cpuif_descr *desc);

static struct {
	tqe_port_handler handler;
	void *token;
	int32_t group;
} tqe_port_handlers[TOPAZ_TQE_NUM_PORTS];

int tqe_port_add_handler(enum topaz_tqe_port port, tqe_port_handler handler, void *token)
{
	if (port >= TOPAZ_TQE_NUM_PORTS || !handler) {
		return -EINVAL;
	}

	tqe_port_handlers[port].handler = handler;
	tqe_port_handlers[port].token = token;

	return 0;
}
EXPORT_SYMBOL(tqe_port_add_handler);

void tqe_port_remove_handler(enum topaz_tqe_port port)
{
	if (port >= TOPAZ_TQE_NUM_PORTS || !tqe_port_handlers[port].handler) {
		printk(KERN_ERR "%s: invalid port %u\n", __FUNCTION__, port);
		return;
	}

	tqe_port_handlers[port].handler = NULL;
	tqe_port_handlers[port].token = NULL;
}
EXPORT_SYMBOL(tqe_port_remove_handler);

static void tqe_port_set(const enum topaz_tqe_port port, const uint8_t enable)
{
	struct topaz_fwt_sw_mcast_entry *mcast_ent;

	if (!g_tqe_fwt_get_mcast_ff_hook) {
		return;
	}

	mcast_ent = g_tqe_fwt_get_mcast_ff_hook();
	if (unlikely(!mcast_ent)) {
		return;
	}
	if (enable) {
		topaz_fwt_sw_mcast_port_set(mcast_ent, port);
	} else {
		topaz_fwt_sw_mcast_port_clear(mcast_ent, port);
	}
	topaz_fwt_sw_mcast_flush(mcast_ent);
}

void tqe_port_set_group(const enum topaz_tqe_port port, int32_t group)
{
	if ((port < TOPAZ_TQE_NUM_PORTS) && (group > 0))
		tqe_port_handlers[port].group = group;
}
EXPORT_SYMBOL(tqe_port_set_group);

void tqe_port_clear_group(const enum topaz_tqe_port port)
{
	if (port < TOPAZ_TQE_NUM_PORTS)
		tqe_port_handlers[port].group = 0;
}
EXPORT_SYMBOL(tqe_port_clear_group);

void tqe_port_register(const enum topaz_tqe_port port)
{
	tqe_port_set(port, 1);
}
EXPORT_SYMBOL(tqe_port_register);

void tqe_port_unregister(const enum topaz_tqe_port port)
{
	tqe_port_set(port, 0);
}
EXPORT_SYMBOL(tqe_port_unregister);

struct update_multicast_tx_stats {
	void (*fn)(void *ctx, uint8_t node);
	void *ctx;
};

struct update_multicast_tx_stats update_multicast;

void tqe_reg_multicast_tx_stats(void (*fn)(void *ctx, uint8_t), void *ctx)
{
	update_multicast.fn = fn;
	update_multicast.ctx = ctx;
}
EXPORT_SYMBOL(tqe_reg_multicast_tx_stats);

#if defined(CONFIG_ARCH_TOPAZ_SWITCH_TEST) || defined(CONFIG_ARCH_TOPAZ_SWITCH_TEST_MODULE)
static void topaz_tqe_test_ctrl(const uint8_t *buff_virt_rx)
{
	const uint8_t ctrl_dstmac[ETH_ALEN] = TOPAZ_TEST_CTRL_DSTMAC;
	const uint8_t ctrl_srcmac[ETH_ALEN] = TOPAZ_TEST_CTRL_SRCMAC;

	if (memcmp(&buff_virt_rx[ETH_ALEN * 0], ctrl_dstmac, ETH_ALEN) == 0 &&
		memcmp(&buff_virt_rx[ETH_ALEN * 1], ctrl_srcmac, ETH_ALEN) == 0) {

		const char *test_str = (const char *)&buff_virt_rx[128];
		unsigned long len;
		char *cmd = NULL;
		char **words = NULL;
		int rc;
		int word_count;
		int (*parse)(int, char**) = NULL;

		len = strlen(test_str);
		cmd = kmalloc(len + 1, GFP_KERNEL);
		words = kmalloc(len * sizeof(char *) / 2, GFP_KERNEL);
		if (!cmd || !words) {
			rc = -ENOMEM;
			goto out;
		}

		strcpy(cmd, test_str);
		word_count = topaz_test_split_words(words, cmd);

		if (strcmp(words[0], "dpi_test") == 0) {
			parse = &topaz_dpi_test_parse;
		} else if (strcmp(words[0], "fwt_test") == 0) {
			parse = &topaz_fwt_test_parse;
		} else if (strcmp(words[0], "ipprt_emac0") == 0) {
			parse = &topaz_ipprt_emac0_test_parse;
		} else if (strcmp(words[0], "ipprt_emac1") == 0) {
			parse = &topaz_ipprt_emac1_test_parse;
		} else if (strcmp(words[0], "vlan_test") == 0) {
			parse = &topaz_vlan_test_parse;
		} else {
			printk("%s: invalid ctrl packet\n", __FUNCTION__);
		}

		if (parse) {
			rc = parse(word_count - 1, words + 1);
			printk("%s: rc %d '%s'\n", __FUNCTION__, rc, test_str);
		}
out:
		if (cmd)
			kfree(cmd);
		if (words)
			kfree(words);
	}
}
#endif

uint32_t
switch_tqe_multi_proc_sem_down(char * funcname, int linenum)
{
#ifdef CONFIG_TOPAZ_PCIE_TARGET
	uint32_t prtcnt;

	if (tqe_sem_en == 0)
		return 1;

	prtcnt = 0;
	while (_qtn_mproc_3way_tqe_sem_down(TOPAZ_MPROC_TQE_SEM_LHOST) == 0) {
		if ((prtcnt & 0xff) == 0)
			printk("%s line %d fail to get tqe semaphore\n", funcname, linenum);
		prtcnt++;
	}
#endif
	return 1;
}

EXPORT_SYMBOL(switch_tqe_multi_proc_sem_down);

uint32_t
switch_tqe_multi_proc_sem_up(void)
{
#ifdef CONFIG_TOPAZ_PCIE_TARGET
	if (tqe_sem_en == 0)
		return 1;

	if (_qtn_mproc_3way_tqe_sem_up(TOPAZ_MPROC_TQE_SEM_LHOST)) {
		return 1;
	} else {
		WARN_ONCE(1, "%s failed to relese HW semaphore\n", __func__);
		return 0;
	}
#else
	return 1;
#endif
}

EXPORT_SYMBOL(switch_tqe_multi_proc_sem_up);

static void tqe_buf_set_refcounts(void *buf_start, int32_t enqueue, int32_t free)
{
	uint32_t *p = buf_start;
	uint32_t *_m = topaz_hbm_buf_get_meta(p);
	uint32_t *enqueuep = _m - HBM_HR_OFFSET_ENQ_CNT;
	uint32_t *freep = _m - HBM_HR_OFFSET_FREE_CNT;

	if (enqueue >= 0)
		arc_write_uncached_32(enqueuep, enqueue);
	if (free >= 0)
		arc_write_uncached_32(freep, free);
}

int topaz_tqe_xmit(union topaz_tqe_cpuif_ppctl *pp_cntl)
{
	int num = topaz_tqe_cpuif_port_to_num(TOPAZ_TQE_LOCAL_CPU);

	topaz_tqe_wait();
	switch_tqe_multi_proc_sem_down("topaz_tqe_xmit",__LINE__);
	topaz_tqe_cpuif_tx_start(pp_cntl);
	switch_tqe_multi_proc_sem_up();

	wmb();

	topaz_tqe_wait();
	if ((qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_TXSTART(num)) &
			TOPAZ_TQE_CPUIF_TX_START_NOT_SUCCESS))
		return NET_XMIT_CN;
	else
		return NET_XMIT_SUCCESS;

}

void topaz_tqe_congest_queue_process(const union topaz_tqe_cpuif_descr *desc,
		void *queue, uint8_t node, uint8_t tqe_tid,
		union topaz_tqe_cpuif_ppctl *ppctl, uint8_t is_unicast)
{
	struct topaz_congest_queue *congest_queue = (struct topaz_congest_queue *)queue;
	struct topaz_congest_q_desc *q_desc;
	int8_t re_sched = 0;
	int8_t ret = 0;

	if (topaz_queue_congested(congest_queue, node, tqe_tid)) {
		q_desc = topaz_get_congest_queue(congest_queue, node, tqe_tid);
		ret = topaz_congest_enqueue(q_desc, ppctl);
		if (ret == NET_XMIT_CN) {
			topaz_hbm_congest_queue_put_buf(ppctl);
		}

		re_sched = topaz_congest_queue_xmit(q_desc, TOPAZ_SOFTIRQ_BUDGET);
		if (re_sched)
			tasklet_schedule(&congest_queue->congest_tx);

	} else {
		ret = congest_queue->xmit_func(ppctl);

		if (unlikely(ret != NET_XMIT_SUCCESS)) {
			if (is_unicast)
				q_desc = topaz_congest_alloc_unicast_queue(congest_queue,
										node, tqe_tid);
			else
				q_desc = topaz_congest_alloc_queue(congest_queue, node, tqe_tid);

			if (!q_desc) {
				topaz_hbm_congest_queue_put_buf(ppctl);
			} else {
				ret = topaz_congest_enqueue(q_desc, ppctl);

				if (ret == NET_XMIT_CN) {
					topaz_hbm_congest_queue_put_buf(ppctl);
				} else {
					tasklet_schedule(&congest_queue->congest_tx);
				}
			}
		}
	}
}

/*
 * Push a packet to the TQE
 */
static void __sram_text tqe_push_mcast(const void *token1, void *token2,
					uint8_t port, uint8_t node, uint8_t tid)
{
	const union topaz_tqe_cpuif_descr *desc = token1;
	union topaz_tqe_cpuif_ppctl ppctl;
	const uint8_t portal = 1;
	const uint16_t misc_user = 0;
	void *queue = token2;
	uint8_t tqe_free = queue ? 0 : 1;
	struct qtn_vlan_dev *vdev;

	/*
	 * VLAN egress check -- multicast forwarding
	 * for out_port==WMAC, AuC handles egress
	 */
	if (vlan_enabled && TOPAZ_TQE_PORT_IS_EMAC(port)) {
		vdev = vport_tbl_lhost[port];
		if (!qtn_vlan_egress(vdev, 0, bus_to_virt((uintptr_t)desc->data.pkt))) {
			tqe_rx_pkt_drop(desc);
			return;
		}
	}

	topaz_tqe_cpuif_ppctl_init(&ppctl,
			port, &node, 1, tid,
			portal, 1, TOPAZ_HBM_EMAC_TX_DONE_POOL, tqe_free, misc_user);

	ppctl.data.pkt = desc->data.pkt;
	ppctl.data.length = desc->data.length;
	ppctl.data.buff_ptr_offset = desc->data.buff_ptr_offset;

	if (queue) {
		topaz_tqe_congest_queue_process(desc, queue, node, tid, &ppctl, 0);
	} else {
		topaz_tqe_wait();
		switch_tqe_multi_proc_sem_down("tqe_push_mcast",__LINE__);
		topaz_tqe_cpuif_tx_start(&ppctl);
		switch_tqe_multi_proc_sem_up();
	}

	if (port == TOPAZ_TQE_WMAC_PORT && update_multicast.fn)
		update_multicast.fn(update_multicast.ctx, node);
}

/*
 * returns the number of TQE pushes; 0 means buffer is not consumed here
 */
static uint32_t __sram_text tqe_push_mc_ports(void *queue,
		const struct topaz_fwt_sw_mcast_entry *mcast_ent_shared,
		const union topaz_tqe_cpuif_descr *desc, uint8_t tid, uint8_t in_node,
		uint32_t header_access_bytes)
{
	struct topaz_fwt_sw_mcast_entry mcast_ent;
	enum topaz_tqe_port in_port = desc->data.in_port;
	uint32_t push_count;
	uint32_t pushes = 0;
	uint8_t port = TOPAZ_TQE_FIRST_PORT;
	void *buf_bus_rx = desc->data.pkt;
	void *buf_virt_rx = bus_to_virt((unsigned long) buf_bus_rx);
	const struct ether_header *eh = buf_virt_rx;
	int offset = desc->data.buff_ptr_offset;

	mcast_ent = *mcast_ent_shared;

	/* The MuC handles snooped multicast directly */
	if (in_port == TOPAZ_TQE_WMAC_PORT || in_port == TOPAZ_TQE_MUC_PORT) {
		if (printk_ratelimit())
			printk("%s: mcast pkt from mac t=%04x d=%pM s=%pM\n", __FUNCTION__,
				eh->ether_type, eh->ether_dhost, eh->ether_shost);
		return 0;
	}

	/* find the expected enqueue count and set the HBM buffer reference count */
	push_count = topaz_fwt_sw_mcast_enqueues(&mcast_ent, mcast_ent.port_bitmap,
						in_port, in_node);
	if (unlikely(!push_count)) {
		return 0;
	}

	tqe_buf_set_refcounts((uint8_t *)buf_virt_rx + offset, push_count, 0);

	flush_and_inv_dcache_range((unsigned long)buf_virt_rx, (unsigned long)(buf_virt_rx + header_access_bytes));

	/* push this packet to the tqe for each port/node */
	while (mcast_ent.port_bitmap) {
		if (mcast_ent.port_bitmap & 0x1) {
			if (topaz_fwt_sw_mcast_port_has_nodes(port)) {
				pushes += topaz_fwt_sw_mcast_do_per_node(tqe_push_mcast,
							&mcast_ent, desc, queue, in_node, port, tid);
			} else {
				if (port != in_port)  {
					tqe_push_mcast(desc, NULL, port, 0, 0);
					++pushes;
				}
			}
		}
		mcast_ent.port_bitmap >>= 1;
		port++;
	}

	if (unlikely(pushes != push_count)) {
		printk(KERN_CRIT "%s: pushes %u push_count %u, buffer leak imminent\n",
				__FUNCTION__, pushes, push_count);
	}

	return push_count;
}

int __sram_text tqe_rx_get_node_id(const struct ether_header *eh)
{
	const struct fwt_db_entry *fwt_ent = NULL;

	if (likely(g_tqe_fwt_get_ucast_hook)) {
		fwt_ent = g_tqe_fwt_get_ucast_hook(eh->ether_shost, eh->ether_shost);
		if (likely(fwt_ent) && fwt_ent->valid)
			return fwt_ent->out_node;
	}

	return 0;
}

inline
const uint16_t *
tqe_rx_ether_type_skip_vlan(const struct ether_header *eh, uint32_t len)
{
	const uint16_t *ether_type = &eh->ether_type;

	if (len < sizeof(struct ether_header)) {
		return NULL;
	}

	while(qtn_ether_type_is_vlan(*ether_type)) {
		if (len < sizeof(struct ether_header) + VLAN_HLEN) {
			return NULL;
		}
		ether_type += VLAN_HLEN / sizeof(*ether_type);
		len -= VLAN_HLEN;
	}

	return ether_type;
}

int __sram_text tqe_rx_multicast(void *congest_queue, const union topaz_tqe_cpuif_descr *desc)
{
	int timeout;
	union topaz_fwt_lookup fwt_lu;
	const struct topaz_fwt_sw_mcast_entry *mcast_ent = NULL;
	const struct ether_header *eh = bus_to_virt((uintptr_t) desc->data.pkt);
	const enum topaz_tqe_port in_port = desc->data.in_port;
	const void *ipaddr = NULL;
	uint8_t tid = 0;
	const uint16_t *ether_type = tqe_rx_ether_type_skip_vlan(eh, desc->data.length);
	const void *iphdr = NULL;
	uint8_t vlan_index;
	uint8_t in_node = 0;
	uint32_t header_access_bytes = 0;
	uint32_t ether_payload_length = 0;
	uint8_t false_miss = 0;

	if (unlikely(!ether_type)) {
		printk(KERN_WARNING "%s: malformed packet in_port %u\n", __FUNCTION__, in_port);
		return 0;
	}

	iphdr = ether_type + 1;
	ether_payload_length = desc->data.length - ((char *)iphdr - (char *)eh);
	header_access_bytes = iphdr - (const void *)eh;

	/* FIXME: this won't work for 802.3 frames */
	if (*ether_type == __constant_htons(ETH_P_IP)
			&& iputil_mac_is_v4_multicast(eh->ether_dhost)
			&& (ether_payload_length >= sizeof(struct qtn_ipv4))) {
		const struct qtn_ipv4 *ipv4 = (const struct qtn_ipv4 *)iphdr;
		/* do not accelerate IGMP */
		if (ipv4->proto == QTN_IP_PROTO_IGMP) {
			return 0;
		}
		ipaddr = &ipv4->dstip;

		/* Option field doesn't take into account because they are not accessed */
		header_access_bytes += sizeof (struct qtn_ipv4);
	} else if (*ether_type == __constant_htons(ETH_P_IPV6)
			&& iputil_mac_is_v6_multicast(eh->ether_dhost)
			&& (ether_payload_length >= sizeof(struct qtn_ipv6))) {
		const struct qtn_ipv6 *ipv6 = (const struct qtn_ipv6 *)iphdr;
		ipaddr = &ipv6->dstip;

		header_access_bytes += sizeof (struct qtn_ipv6);
	}

	if (ipaddr) {
		topaz_tqe_vlan_gettid(desc->data.pkt, &tid, &vlan_index);
		fwt_lu = topaz_fwt_hw_lookup_wait_be(eh->ether_dhost, &timeout, &false_miss);
		if (fwt_lu.data.valid && !timeout) {
#ifndef TOPAZ_DISABLE_FWT_WAR
			if (unlikely(false_miss && g_tqe_fwt_false_miss_hook))
				g_tqe_fwt_false_miss_hook(fwt_lu.data.entry_addr, false_miss);
#endif

			if (g_tqe_fwt_get_mcast_hook)
				mcast_ent = g_tqe_fwt_get_mcast_hook(fwt_lu.data.entry_addr,
						ipaddr, *ether_type);

			if (mcast_ent) {
				if ((mcast_ent->flood_forward) && (in_port == TOPAZ_TQE_MUC_PORT)) {
					in_node = tqe_rx_get_node_id(eh);
					if (in_node == 0)
						return 0;
				}
				return tqe_push_mc_ports(congest_queue, mcast_ent, desc, tid,
								in_node, header_access_bytes);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(tqe_rx_multicast);

static inline void __sram_text tqe_rx_pkt_drop(const union topaz_tqe_cpuif_descr *desc)
{
	void *buf_virt_rx = bus_to_virt((unsigned long) desc->data.pkt);
	uint16_t buflen = desc->data.length;
	const int8_t dest_pool = topaz_hbm_payload_get_pool_bus(desc->data.pkt);
	void *buf_bus = topaz_hbm_payload_store_align_bus(desc->data.pkt, dest_pool, 0);
	unsigned long flags;

	cache_op_before_rx(buf_virt_rx, buflen, 0);

	local_irq_save(flags);
	topaz_hbm_filter_txdone_buf(buf_bus);
	local_irq_restore(flags);
}

void tqe_register_mac_reserved_cbk(tqe_mac_reserved_hook cbk_func)
{
	g_tqe_mac_reserved_hook = cbk_func;
}
EXPORT_SYMBOL(tqe_register_mac_reserved_cbk);

void tqe_register_ucastfwt_cbk(tqe_fwt_get_ucast_hook cbk_func)
{
	g_tqe_fwt_get_ucast_hook = cbk_func;
}
EXPORT_SYMBOL(tqe_register_ucastfwt_cbk);

void tqe_register_macfwt_cbk(tqe_fwt_get_from_mac_hook cbk_func)
{
	 g_tqe_fwt_get_from_mac_hook = cbk_func;
}
EXPORT_SYMBOL(tqe_register_macfwt_cbk);

static int topaz_swfwd_tqe_xmit(const fwt_db_entry *fwt_ent,
				const union topaz_tqe_cpuif_descr *desc,
				void *queue)
{
	uint8_t port;
	uint8_t node;
	uint16_t misc_user;
	uint8_t tid = 0;
	union topaz_tqe_cpuif_ppctl ctl;
	uint8_t portal;
	uint8_t vlan_index;
	uint8_t tqe_free = queue ? 0 : 1;
	struct qtn_vlan_dev *vdev;

	if (fwt_ent->out_port == TOPAZ_TQE_LHOST_PORT)
		return 0;

	/*
	 * VLAN egress check -- unicast forwarding
	 * for out_port==WMAC, AuC handles egress
	 */
	if (vlan_enabled && TOPAZ_TQE_PORT_IS_EMAC(fwt_ent->out_port)) {
		vdev = vport_tbl_lhost[fwt_ent->out_port];
		if (!qtn_vlan_egress(vdev, 0, bus_to_virt((uintptr_t)desc->data.pkt))) {
			tqe_rx_pkt_drop(desc);
			return 1;
		}
	}

	if (TOPAZ_TQE_PORT_IS_EMAC(desc->data.in_port)) {
		if(g_dscp_flag){
			tid = (g_dscp_value[desc->data.in_port] & 0xFF);
		} else {
			topaz_tqe_vlan_gettid(desc->data.pkt, &tid, &vlan_index);
		}
	} else {
		topaz_tqe_vlan_gettid(desc->data.pkt, &tid, &vlan_index);
	}

	port = fwt_ent->out_port;
	node = fwt_ent->out_node;
	portal = fwt_ent->portal;
	misc_user = 0;
	topaz_tqe_cpuif_ppctl_init(&ctl,
			port, &node, 1, tid,
			portal, 1, 0, tqe_free, misc_user);

	ctl.data.pkt = (void *)desc->data.pkt;
	ctl.data.buff_ptr_offset = desc->data.buff_ptr_offset;
	ctl.data.length = desc->data.length;
	ctl.data.buff_pool_num = TOPAZ_HBM_EMAC_TX_DONE_POOL;

	if (queue) {
		topaz_tqe_congest_queue_process(desc, queue, node, tid, &ctl, 1);
	} else {
		while (topaz_tqe_cpuif_tx_nready());
		switch_tqe_multi_proc_sem_down("topaz_swfwd_tqe_xmit",__LINE__);
		topaz_tqe_cpuif_tx_start(&ctl);
		switch_tqe_multi_proc_sem_up();
	}

	return 1;
}

static inline int topaz_is_dhcp(const struct iphdr *ipv4h)
{
	const struct udphdr *udph;

	if (ipv4h->protocol != IPPROTO_UDP)
		return 0;

	udph = (const struct udphdr *)((const uint8_t *)ipv4h + (ipv4h->ihl << 2));
	if (udph->source == __constant_htons(DHCPSERVER_PORT)
			&& udph->dest == __constant_htons(DHCPCLIENT_PORT))
		return 1;

        return 0;
}

#ifdef CONFIG_IPV6
static inline int topaz_ipv6_not_accel(const struct ipv6hdr *ipv6h, int seg_len)
{
	uint8_t nexthdr;
	const struct udphdr *udph;
	const struct icmp6hdr *icmph;
	int nhdr_off;

	nhdr_off = iputil_v6_skip_exthdr(ipv6h, sizeof(struct ipv6hdr),
			&nexthdr, seg_len, NULL, NULL);

	if (nexthdr == IPPROTO_UDP) {
		udph = (const struct udphdr *)((const uint8_t *)ipv6h + nhdr_off);
		if (udph->source == __constant_htons(DHCPV6SERVER_PORT)
				&& udph->dest == __constant_htons(DHCPV6CLIENT_PORT))
			return 1;
	} else if (nexthdr == IPPROTO_ICMPV6) {
		icmph = (const struct icmp6hdr *)((const uint8_t *)ipv6h + nhdr_off);
		if (icmph->icmp6_type == NDISC_NEIGHBOUR_SOLICITATION
				|| icmph->icmp6_type == NDISC_NEIGHBOUR_ADVERTISEMENT)
			return 1;
	}

	return 0;
}
#endif

static int __sram_text tqe_rx_pktfwd(void *queue, const union topaz_tqe_cpuif_descr *desc)
{
	enum topaz_tqe_port in_port = desc->data.in_port;
	const struct fwt_db_entry *fwt_ent;
	const struct ether_header *eh = bus_to_virt((uintptr_t) desc->data.pkt);
	const struct vlan_ethhdr *vlan_hdr = (struct vlan_ethhdr *)eh;
	const struct iphdr *ipv4h;
	const struct ipv6hdr *ipv6h;
	uint16_t ether_type;
	uint16_t ether_hdrlen;

	if (!TOPAZ_TQE_PORT_IS_EMAC(in_port))
		return 0;

	if (unlikely(iputil_eth_is_multicast(eh)))
		return 0;

	ether_type = eh->ether_type;
	if (ether_type == __constant_htons(ETH_P_8021Q)) {
		ether_type = vlan_hdr->h_vlan_encapsulated_proto;
		ipv4h = (const struct iphdr *)(vlan_hdr + 1);
		ipv6h = (const struct ipv6hdr *)(vlan_hdr + 1);
		ether_hdrlen = sizeof(struct vlan_ethhdr);
	} else {
		ipv4h = (const struct iphdr *)(eh + 1);
		ipv6h = (const struct ipv6hdr *)(eh + 1);
		ether_hdrlen = sizeof(struct ether_header);
	}

	if (ether_type == __constant_htons(ETH_P_ARP))
		return 0;

	if (ether_type == __constant_htons(ETH_P_IP)
			&& topaz_is_dhcp(ipv4h))
		return 0;

#ifdef CONFIG_IPV6
	if (ether_type == __constant_htons(ETH_P_IPV6)
			&& topaz_ipv6_not_accel(ipv6h, desc->data.length - ether_hdrlen))
		return 0;
#endif

	if (unlikely(!g_tqe_fwt_get_ucast_hook))
		return 0;

	fwt_ent = g_tqe_fwt_get_ucast_hook(eh->ether_shost, eh->ether_dhost);
	if (unlikely(!fwt_ent))
		return 0;

	/* Don't return to sender */
	if (unlikely((in_port == fwt_ent->out_port) ||
		((tqe_port_handlers[in_port].group > 0) &&
			tqe_port_handlers[in_port].group ==
				tqe_port_handlers[fwt_ent->out_port].group))) {
		tqe_rx_pkt_drop(desc);
		return 1;
	}

	return topaz_swfwd_tqe_xmit(fwt_ent, desc, queue);
}

int wowlan_magic_packet_check(const union topaz_tqe_cpuif_descr *desc)
{
	const struct ether_header *eh = bus_to_virt((uintptr_t) desc->data.pkt);
	const uint16_t *ether_type = NULL;
	const void *iphdr = NULL;
	uint32_t ether_payload_length = 0;

	if (likely(!g_wowlan_host_state) ||
			(desc->data.in_port != TOPAZ_TQE_MUC_PORT))
		return 0;

	ether_type = tqe_rx_ether_type_skip_vlan(eh, desc->data.length);
	if (unlikely(!ether_type)) {
		return 0;
	}

	iphdr = (void *)(ether_type + 1);
	ether_payload_length = desc->data.length - ((char *)iphdr - (char *)eh);
	if ((*ether_type == __constant_htons(ETH_P_IP))
			&& (ether_payload_length < sizeof(struct iphdr))) {
		return 0;
	}

	return wowlan_is_magic_packet(*ether_type, eh, iphdr,
			g_wowlan_match_type,
			g_wowlan_l2_ether_type,
			g_wowlan_l3_udp_port);
}

static int tqe_rx_l2_ext_filter_handler(union topaz_tqe_cpuif_descr *desc, struct sk_buff *skb)
{
	enum topaz_tqe_port in_port = desc->data.in_port;
	const struct fwt_db_entry *fwt_ent;
	const struct ether_header *eh = bus_to_virt((uintptr_t) desc->data.pkt);

	if (in_port != g_l2_ext_filter_port)
		return 0;

	if (unlikely(!g_tqe_fwt_get_from_mac_hook))
		return 0;

	fwt_ent = g_tqe_fwt_get_from_mac_hook(eh->ether_shost);
	if (unlikely(!fwt_ent))
		return 0;

	if (TOPAZ_TQE_PORT_IS_WMAC(fwt_ent->out_port)) {
		/* Change the in port to prevent FWT updates */
		desc->data.in_port = TOPAZ_TQE_MUC_PORT;
		desc->data.misc_user = fwt_ent->out_node;
		skb->ext_l2_filter = 1;
		return 1;
	}

	return 0;
}

int __sram_text tqe_rx_l2_ext_filter(union topaz_tqe_cpuif_descr *desc, struct sk_buff *skb)
{
	if (unlikely(g_l2_ext_filter))
		return tqe_rx_l2_ext_filter_handler(desc, skb);

	return 0;
}
EXPORT_SYMBOL(tqe_rx_l2_ext_filter);

void __sram_text tqe_rx_call_port_handler(union topaz_tqe_cpuif_descr *desc,
		struct sk_buff *skb, uint8_t *whole_frm_hdr)
{
	enum topaz_tqe_port in_port = desc->data.in_port;

	tqe_port_handlers[in_port].handler(tqe_port_handlers[in_port].token,
						desc, skb, whole_frm_hdr);
}
EXPORT_SYMBOL(tqe_rx_call_port_handler);

static int __sram_text tqe_rx_desc_handler(const struct tqe_netdev_priv *priv, union topaz_tqe_cpuif_descr *desc)
{
	enum topaz_tqe_port in_port = desc->data.in_port;
	void *buf_bus_rx = desc->data.pkt;
	void *buf_virt_rx = bus_to_virt((unsigned long) buf_bus_rx);
	uint16_t buflen = desc->data.length;
	const int8_t pool = topaz_hbm_payload_get_pool_bus(buf_bus_rx);
	const struct ether_header *eh = bus_to_virt((uintptr_t) desc->data.pkt);
	uint8_t vinfo_hdr = 0;

	if (unlikely(buf_bus_rx == NULL)) {
		if (printk_ratelimit()) {
			printk(KERN_CRIT "%s: NULL buffer from TQE, len %u, in port %u",
					__FUNCTION__, buflen, in_port);
		}
		return -1;
	}

	if (unlikely(buflen < ETH_HLEN)) {
		printk(KERN_WARNING
			"%s: buffer from TQE smaller than ethernet header, len %u, in port %u",
							__FUNCTION__, buflen, in_port);
		return -1;
	}

	if (unlikely(!topaz_hbm_pool_valid(pool))) {
		printk(KERN_CRIT "%s: invalid pool buffer from TQE: 0x%p", __FUNCTION__, buf_bus_rx);
		return -1;
	}

	if (likely((in_port < TOPAZ_TQE_NUM_PORTS) && tqe_port_handlers[in_port].handler)) {
		struct sk_buff *skb;
		uint8_t *whole_frm_hdr;

		topaz_hbm_debug_stamp(topaz_hbm_payload_store_align_virt(buf_virt_rx, pool, 0),
				TOPAZ_HBM_OWNER_LH_RX_TQE, buflen);

		/* invalidate enough for l3 packet inspection for multicast frames */
		inv_dcache_sizerange_safe(buf_virt_rx, 64);

#if defined(CONFIG_ARCH_TOPAZ_SWITCH_TEST) || defined(CONFIG_ARCH_TOPAZ_SWITCH_TEST_MODULE)
		topaz_tqe_test_ctrl(buf_virt_rx);
#endif
		if (TOPAZ_TQE_PORT_IS_EMAC(in_port)) {
			if (vlan_enabled) {
				struct qtn_vlan_dev *vdev = vport_tbl_lhost[in_port];
				BUG_ON(vdev == NULL);

				if (!qtn_vlan_ingress(vdev, 0,
						buf_virt_rx, 0, 1)) {
					tqe_rx_pkt_drop(desc);
					return 0;
				}
			}
		} else if (unlikely(((in_port == TOPAZ_TQE_WMAC_PORT) || (in_port == TOPAZ_TQE_MUC_PORT)))) {
			if (g_tqe_mac_reserved_hook && g_tqe_mac_reserved_hook(eh->ether_shost)) {
				tqe_rx_pkt_drop(desc);
				return 0;
			}
		} else {
			BUG_ON(1);
		}

		if (vlan_enabled)
			vinfo_hdr = QVLAN_PKTCTRL_LEN;

		if (likely(!wowlan_magic_packet_check(desc))) {
#ifdef CONFIG_TOPAZ_PCIE_HOST
			if (tqe_rx_multicast(NULL, desc))
#else
			if (tqe_rx_multicast(priv->congest_queue, desc))
#endif
				return 0;

			if (tqe_rx_pktfwd(priv->congest_queue, desc))
				return 0;
		}

#if TOPAZ_HBM_BUF_WMAC_RX_QUARANTINE
		if (pool == TOPAZ_HBM_BUF_WMAC_RX_POOL)
		{
			skb = topaz_hbm_attach_skb_quarantine(buf_virt_rx, pool, buflen, &whole_frm_hdr);
			/* now desc doesn't link to the new skb data buffer */
			if (skb) {
				/* new buf is used, no need for original one */
				tqe_rx_pkt_drop(desc);
			}
		}
		else
#endif
		{
			skb = topaz_hbm_attach_skb((uint8_t *)buf_virt_rx, pool, vinfo_hdr);
			whole_frm_hdr = skb->head;
		}
		if (skb) {
			/* attach VLAN information to skb */
			skb_put(skb, buflen);
			if (vlan_enabled) {
				struct qtn_vlan_pkt *pkt = qtn_vlan_get_info(skb->data);
				if (unlikely(pkt->magic != QVLAN_PKT_MAGIC)) {
					printk(KERN_WARNING "WARNING: pkt(%p) magic not right. 0x%04x\n", skb->data, pkt->magic);
				} else {
					skb->vlan_tci = pkt->vlan_info & QVLAN_MASK_VID;
				}
				M_FLAG_SET(skb, M_VLAN_TAGGED);
			}

			/* Frame received from external L2 filter will not have MAC header */
			if (tqe_rx_l2_ext_filter(desc, skb))
				whole_frm_hdr = NULL;
			tqe_rx_call_port_handler(desc, skb, whole_frm_hdr);
			return 0;
		}

	} else {
		if (printk_ratelimit()) {
			printk(KERN_ERR "%s: input from unhandled port %u misc %u\n",
					__FUNCTION__, in_port, (unsigned)desc->data.misc_user);
		}
	}

	tqe_rx_pkt_drop(desc);

	return 0;
}

static void tqe_irq_enable(void)
{
	topaz_tqe_cpuif_setup_irq(1, 0);
}

static void tqe_irq_disable(void)
{
	topaz_tqe_cpuif_setup_irq(0, 0);
}

static int __sram_text tqe_rx_napi_handler(struct napi_struct *napi, int budget)
{
	int processed = 0;
	struct tqe_netdev_priv *priv = container_of(napi, struct tqe_netdev_priv, napi);

	while (processed < budget) {
		union topaz_tqe_cpuif_status status;
		union topaz_tqe_cpuif_descr __iomem *desc_bus;
		union topaz_tqe_cpuif_descr *desc_virt;
		union topaz_tqe_cpuif_descr desc_local;
		uintptr_t inv_start;
		size_t inv_size;

		status = topaz_tqe_cpuif_get_status();
		if (status.data.empty) {
			break;
		}

		desc_bus = topaz_tqe_cpuif_get_curr();
		desc_virt = bus_to_virt((uintptr_t) desc_bus);

		/* invalidate descriptor and copy to the stack */
		inv_start = (uintptr_t) align_buf_cache(desc_virt);
		inv_size = align_buf_cache_size(desc_virt, sizeof(*desc_virt));
		inv_dcache_range(inv_start, inv_start + inv_size);
		memcpy(&desc_local, desc_virt, sizeof(*desc_virt));

		if (likely(desc_local.data.own)) {
			topaz_tqe_cpuif_put_back(desc_bus);
			tqe_rx_desc_handler(priv, &desc_local);
			++processed;
		} else {
			printk("%s unowned descriptor? desc_bus 0x%p 0x%08x 0x%08x 0x%08x 0x%08x\n",
					__FUNCTION__, desc_bus,
					desc_local.raw.dw0, desc_local.raw.dw1,
					desc_local.raw.dw2, desc_local.raw.dw3);
			break;
		}
	}

	if (processed < budget) {
		napi_complete(napi);
		tqe_irq_enable();
	}

	return processed;
}

static irqreturn_t __sram_text tqe_irqh(int irq, void *_dev)
{
	struct net_device *dev = _dev;
	struct tqe_netdev_priv *priv = netdev_priv(dev);

	napi_schedule(&priv->napi);
	tqe_irq_disable();

	return IRQ_HANDLED;
}

/*
 * TQE network device ops
 */
static int tqe_ndo_open(struct net_device *dev)
{
	return -ENODEV;
}

static int tqe_ndo_stop(struct net_device *dev)
{
	return -ENODEV;
}

static int tqe_tx_buf(union topaz_tqe_cpuif_ppctl *ppctl,
		void __iomem *virt_buf, unsigned long data_len, int8_t pool)
{
	const uintptr_t bus_data_start = virt_to_bus(virt_buf);
	const long buff_ptr_offset = topaz_hbm_payload_buff_ptr_offset_bus((void *)bus_data_start, pool, NULL);

	ppctl->data.pkt = (void *) bus_data_start;
	ppctl->data.buff_ptr_offset = buff_ptr_offset;
	ppctl->data.length = data_len;
	/* always free to txdone pool */
	ppctl->data.buff_pool_num = TOPAZ_HBM_EMAC_TX_DONE_POOL;

	while (topaz_tqe_cpuif_tx_nready());
	
	switch_tqe_multi_proc_sem_down("tqe_tx_buf",__LINE__);
	topaz_tqe_cpuif_tx_start(ppctl);
	switch_tqe_multi_proc_sem_up();

	return 0;
}

void tqe_register_fwt_cbk(tqe_fwt_get_mcast_hook get_mcast_cbk_func,
				tqe_fwt_get_mcast_ff_hook get_mcast_ff_cbk_func,
				tqe_fwt_false_miss_hook false_miss_func)
{
	g_tqe_fwt_get_mcast_hook = get_mcast_cbk_func;
	g_tqe_fwt_get_mcast_ff_hook = get_mcast_ff_cbk_func;
	g_tqe_fwt_false_miss_hook = false_miss_func;
}
EXPORT_SYMBOL(tqe_register_fwt_cbk);

int tqe_tx(union topaz_tqe_cpuif_ppctl *ppctl, struct sk_buff *skb)
{
	unsigned int data_len = skb->len;
	void *buf_virt = skb->data;
	void *buf_bus = (void *) virt_to_bus(buf_virt);
	void *buf_virt_vlan;
	int8_t pool = topaz_hbm_payload_get_pool_bus(buf_bus);
	const bool hbm_can_use = !vlan_enabled &&
		topaz_hbm_pool_valid(pool) &&
		(atomic_read(&skb->users) == 1) &&
		(atomic_read(&skb_shinfo(skb)->dataref) == 1);

	if (hbm_can_use) {
		/*
		 * skb is otherwise unused; clear to send out tqe.
		 * Set flag such that payload isn't returned to the hbm on free
		 */
		skb->hbm_no_free = 1;

		topaz_hbm_flush_skb_cache(skb);
	} else {
		void *hbm_buf_virt;
		uintptr_t flush_start;
		size_t flush_size;

		if (data_len < TOPAZ_HBM_BUF_EMAC_RX_SIZE) {
			pool = TOPAZ_HBM_BUF_EMAC_RX_POOL;
		} else {
			/*
			 * requested impossibly large transmission
			 */
			if (printk_ratelimit()) {
				printk(KERN_ERR "%s: requested oversize transmission"
						" (%u bytes) to port %d\n",
						__FUNCTION__, data_len, ppctl->data.out_port);
			}
			kfree_skb(skb);
			return NETDEV_TX_OK;
		}

		hbm_buf_virt = topaz_hbm_get_payload_virt(pool);
		if (unlikely(!hbm_buf_virt)) {
			/* buffer will be stored in gso_skb and re-attempted for xmit */
			return NETDEV_TX_BUSY;
		}

		topaz_hbm_debug_stamp(hbm_buf_virt, TOPAZ_HBM_OWNER_LH_TX_TQE, data_len);

		memcpy(hbm_buf_virt, buf_virt, data_len);
		buf_virt = hbm_buf_virt;

		if (M_FLAG_ISSET(skb, M_VLAN_TAGGED)) {
			buf_virt_vlan = qtn_vlan_get_info(buf_virt);
			memcpy(buf_virt_vlan, (uint8_t *)qtn_vlan_get_info(skb->data),
					QVLAN_PKTCTRL_LEN);
			flush_start = (uintptr_t) align_buf_cache(buf_virt_vlan);
			flush_size = align_buf_cache_size(buf_virt_vlan, data_len + QVLAN_PKTCTRL_LEN);
		} else {
			flush_start = (uintptr_t) align_buf_cache(buf_virt);
			flush_size = align_buf_cache_size(buf_virt, data_len);
		}

		flush_and_inv_dcache_range(flush_start, flush_start + flush_size);
	}
	dev_kfree_skb(skb);

	tqe_tx_buf(ppctl, buf_virt, data_len, pool);

	return NETDEV_TX_OK;
}
EXPORT_SYMBOL(tqe_tx);

static int tqe_ndo_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	return NETDEV_TX_BUSY;
}

static const struct net_device_ops tqe_ndo = {
	.ndo_open = tqe_ndo_open,
	.ndo_stop = tqe_ndo_stop,
	.ndo_start_xmit = tqe_ndo_start_xmit,
	.ndo_set_mac_address = eth_mac_addr,
};

static int tqe_descs_alloc(struct tqe_netdev_priv *priv)
{
	int i;
	union topaz_tqe_cpuif_descr __iomem *bus_descs;

	if (ALIGNED_DMA_DESC_ALLOC(&priv->rx, QTN_BUFS_LHOST_TQE_RX_RING, TOPAZ_TQE_CPUIF_RXDESC_ALIGN, 1)) {
		return -ENOMEM;
	}

	bus_descs = (void *)priv->rx.descs_dma_addr;
	for (i = 0; i < QTN_BUFS_LHOST_TQE_RX_RING; i++) {
		priv->rx.descs[i].data.next = &bus_descs[(i + 1) % QTN_BUFS_LHOST_TQE_RX_RING];
	}

	printk(KERN_INFO "%s: %u tqe_rx_descriptors at kern uncached 0x%p bus 0x%p\n",
			__FUNCTION__, priv->rx.desc_count, priv->rx.descs, bus_descs);

	topaz_tqe_cpuif_setup_ring((void *)priv->rx.descs_dma_addr, priv->rx.desc_count);

	return 0;
}

static void tqe_descs_free(struct tqe_netdev_priv *priv)
{
	if (priv->rx.descs) {
		ALIGNED_DMA_DESC_FREE(&priv->rx);
	}
}

void print_tqe_counters(struct tqe_netdev_priv *priv)
{
	int i;

	if (priv->congest_queue == NULL)
		return;

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++)
		printk("rx_congest_fwd %d:\t%08x \t%d\n",
			i, priv->congest_queue->queues[i].congest_xmit,
			priv->congest_queue->queues[i].qlen);

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++)
		printk("rx_congest_drop %d:\t%08x\n",
			i, priv->congest_queue->queues[i].congest_drop);

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++)
		printk("rx_congest_enq_fail %d:\t%08x\n",
			i, priv->congest_queue->queues[i].congest_enq_fail);

	/* Congest Queue */
	printk("rx_congest_entry:\t%08x\n", priv->congest_queue->func_entry);
	printk("rx_congest_retry:\t%08x\n", priv->congest_queue->cnt_retries);
	printk("total len:\t%08x \tunicast count:%d\n",
			priv->congest_queue->total_qlen,
			priv->congest_queue->unicast_qcount);
	printk("active tid num:\t%08x\n", qtn_mproc_sync_shared_params_get()->active_tid_num);
}

static ssize_t tqe_dbg_show(struct device *dev, struct device_attribute *attr,
						char *buff)
{
	return 0;
}

static void tqe_init_port_handler(void)
{
	memset(tqe_port_handlers, 0, sizeof(tqe_port_handlers));
	return;
}

static ssize_t tqe_dbg_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = container_of(dev, struct net_device, dev);
	struct tqe_netdev_priv *priv = netdev_priv(ndev);
	char buffer[128];
	char *str = buffer;
	char *token;
	uint32_t cmd;

	strncpy(str, buf, sizeof(buffer) - 1);

	token = strsep(&str, " ,\n");
	cmd = (uint32_t)simple_strtoul(token, NULL, 10);
	switch (cmd) {
	case 0:
		print_tqe_counters(priv);
		break;
	case 1:
		topaz_congest_dump(priv->congest_queue);
		break;
	default:
		break;
	}

	return count;
}
DEVICE_ATTR(dbg, S_IWUSR | S_IRUSR, tqe_dbg_show, tqe_dbg_set); /* dev_attr_dbg */

static struct net_device * __init tqe_netdev_init(void)
{
	int rc = 0;
	struct net_device *dev = NULL;
	struct tqe_netdev_priv *priv;
	static const int tqe_netdev_irq = TOPAZ_IRQ_TQE;

	tqe_init_port_handler();

	dev = alloc_netdev(sizeof(struct tqe_netdev_priv), "tqe", &ether_setup);
	if (!dev) {
		printk(KERN_ERR "%s: unable to allocate dev\n", __FUNCTION__);
		goto netdev_alloc_error;
	}
	priv = netdev_priv(dev);

	dev->base_addr = 0;
	dev->irq = tqe_netdev_irq;
	dev->watchdog_timeo = 60 * HZ;
	dev->tx_queue_len = 1;
	dev->netdev_ops = &tqe_ndo;

	/* Initialise TQE */
	topaz_tqe_cpuif_setup_reset(1);
	topaz_tqe_cpuif_setup_reset(0);

	if (tqe_descs_alloc(priv)) {
		goto desc_alloc_error;
	}

	rc = request_irq(dev->irq, &tqe_irqh, 0, dev->name, dev);
	if (rc) {
		printk(KERN_ERR "%s: unable to get %s IRQ %d\n",
				__FUNCTION__, dev->name, tqe_netdev_irq);
		goto irq_request_error;
	}
#ifndef CONFIG_TOPAZ_PCIE_HOST
	/* Initialize congestion queue */
	priv->congest_queue = topaz_congest_queue_init();
	if (priv->congest_queue == NULL){
		printk(KERN_ERR "LHOST TQE: Can't allocate congest queue\n");
		goto congest_queue_alloc_error;
	}
	priv->congest_queue->xmit_func = topaz_tqe_xmit;
#endif
	rc = register_netdev(dev);
	if (rc) {
		printk(KERN_ERR "%s: Cannot register net device '%s', error %d\n",
				__FUNCTION__, dev->name, rc);
		goto netdev_register_error;
	}

	netif_napi_add(dev, &priv->napi, &tqe_rx_napi_handler, 8);
	napi_enable(&priv->napi);

	tqe_irq_enable();

	device_create_file(&dev->dev, &dev_attr_dbg);

	return dev;

netdev_register_error:
	topaz_congest_queue_exit(priv->congest_queue);
#ifndef CONFIG_TOPAZ_PCIE_HOST
congest_queue_alloc_error:
	free_irq(dev->irq, dev);
#endif
irq_request_error:
	tqe_descs_free(priv);
desc_alloc_error:
	free_netdev(dev);
netdev_alloc_error:
	return NULL;
}


static void __exit tqe_netdev_exit(struct net_device *dev)
{
	struct tqe_netdev_priv *priv  = netdev_priv(dev);

	device_remove_file(&dev->dev, &dev_attr_dbg);
	tqe_irq_disable();
	free_irq(dev->irq, dev);
	free_netdev(dev);
	topaz_congest_queue_exit(priv->congest_queue);
}

static struct net_device *tqe_netdev;

static int __init tqe_module_init(void)
{
	tqe_netdev = tqe_netdev_init();

	return tqe_netdev ? 0 : -EFAULT;
}

static void __exit tqe_module_exit(void)
{
	tqe_netdev_exit(tqe_netdev);
}

module_init(tqe_module_init);
module_exit(tqe_module_exit);

MODULE_LICENSE("GPL");


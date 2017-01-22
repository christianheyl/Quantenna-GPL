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
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#ifdef CONFIG_IPV6
#include <net/ipv6.h>
#endif
#include <linux/udp.h>
#include <linux/etherdevice.h>
#include <linux/udp.h>
#include <linux/igmp.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <trace/skb.h>
#include <trace/ippkt.h>
#include <asm/hardware.h>
#include <asm/board/dma_cache_ops.h>
#include <asm/board/gpio.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_comm.h"
#include "qdrv_wlan.h"
#include "qdrv_vap.h"
#include "qdrv_bridge.h"
#include "qdrv_mac_reserve.h"
#include <qtn/registers.h>
#include <net80211/if_llc.h>
#include <net80211/if_ethersubr.h>
#include <qtn/skb_recycle.h>
#include <net80211/ieee80211_proto.h>
#include <qtn/qtn_global.h>
#include <qtn/iputil.h>
#ifdef CONFIG_QVSP
#include "qtn/qvsp.h"
#endif

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
#include "qtn/qdrv_sch.h"
#include <linux/if_bridge.h>
#include <linux/net/bridge/br_public.h>
#endif

#include <qtn/qtn_decap.h>
#include <qtn/qtn_vlan.h>
#include <qtn/topaz_hbm.h>
#include <qtn/topaz_tqe.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_fwt_db.h>
#include <qtn/topaz_fwt_sw.h>
#include <qtn/qtn_wowlan.h>

#ifdef TOPAZ_AMBER_IP
#include <qtn/topaz_amber.h>
#endif

#define QDRV_SKIP_ETH_HDR(_eh) ((_eh) + 1)

typedef enum {
	REPLACE_IP_MAC = 0,
	SAVE_IP_MAC = 1
} ip_mac_flag;

struct arp_message {
	uint16_t hw_type;
	uint16_t pro_type;
	uint8_t hw_size;
	uint8_t pro_size;
	uint16_t opcode;
	uint8_t shost[ETHER_ADDR_LEN];
	uint32_t sipaddr;
	uint8_t thost[ETHER_ADDR_LEN];
	uint32_t tipaddr;
	uint8_t others[0];
}__attribute__ ((packed));

static struct host_rxdesc *prev_rxdesc = NULL;

#if !(TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT)
static struct sk_buff* __sram_text rx_alloc_skb(struct qdrv_wlan *qw, u8** skb_phy_addr, int trace)
{
	struct sk_buff *skb;
	int alignment;
	int cache_alignment = dma_get_cache_alignment();
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();

	if (!skb_phy_addr) {
		return NULL;
	}

	skb = qtn_skb_recycle_list_pop(recycle_list, &recycle_list->stats_qdrv);
	if (skb) {
		qw->tx_stats.tx_min_cl_cnt = skb_queue_len(&recycle_list->list);
	} else {
		skb = dev_alloc_skb(qtn_rx_buf_size());
	}

	if (skb == NULL) {
		*skb_phy_addr = NULL;
		return NULL;
	}

	skb->recycle_list = recycle_list;

	/* skb->data should be cache aligned - do calculation here to be sure. */
	alignment = (unsigned int)(skb->data) & (cache_alignment - 1);
	if (alignment) {
		skb_reserve(skb, cache_alignment - alignment);
	}

	*skb_phy_addr = (u8 *) cache_op_before_rx(skb->data,
					rx_buf_map_size(), skb->cache_is_cleaned);
	skb->cache_is_cleaned = 0;
	if (!*skb_phy_addr) {
		dev_kfree_skb(skb);
		return NULL;
	}

	trace_skb_perf_start(skb, trace);
	trace_skb_perf_stamp_call(skb);

	return skb;
}
#endif

static int rxdesc_alloc_buffer(struct qdrv_wlan *qw, struct host_rxdesc *rxdesc)
{
#if TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT
	rxdesc->rd_buffer = NULL;	/* buffer is set by MuC or AuC form hbm pool */
	return 0;
#else
	void *skb = rx_alloc_skb(qw, &rxdesc->rd_buffer, 1);
	rxdesc->skbuff = skb;
	return (skb == NULL);
#endif
}

static struct host_rxdesc *rx_alloc_desc(struct qdrv_wlan *qw,
	struct dma_pool *rxdesc_cache, struct host_rxdesc **rd_phys)
{
	struct host_rxdesc *rd = (struct host_rxdesc *)
		dma_pool_alloc(rxdesc_cache, GFP_KERNEL | GFP_DMA, (dma_addr_t*)rd_phys);

	if (!rd) {
		return NULL;
	}

	memset(rd, 0, sizeof(*rd));

	rd->rd_va = rd;
	rd->rd_pa = *rd_phys;

	if (rxdesc_alloc_buffer(qw, rd) != 0) {
		dma_pool_free(rxdesc_cache, rd, (u32)(*rd_phys));
		rd = NULL;
	}

	return rd;
}

static void rx_fifo_destroy(struct host_fifo_if *hfif)
{
	int i;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	prev_rxdesc = NULL;

	for(i = 0; i < hfif->ring_size; i++ ) {

		struct host_rxdesc *rxdesc = hfif->descp[i];
		if (rxdesc == NULL) {
			continue;
		}

		DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_PKT_RX,
			"i=%d rdesc=%p, skb=%p, %p, stat=%x\n",
			i, rxdesc, rxdesc->rd_buffer, rxdesc->skbuff,
			rxdesc->rs_statword);

		dev_kfree_skb((struct sk_buff *)rxdesc->skbuff);
		dma_pool_free(hfif->df_rxdesc_cache, rxdesc,
			(dma_addr_t)rxdesc->rd_pa);
	}
	dma_pool_destroy(hfif->df_rxdesc_cache);
	kfree(hfif->descp);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

static void destroy_rx_ring(struct host_rxif *rxif)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	rx_fifo_destroy(&rxif->rx);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

static __sram_text void qdrv_rx_irq(void *arg1, void *arg2)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) arg1;
	struct qdrv_mac *mac = (struct qdrv_mac *) arg2;
	struct qdrv_vap *qv;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	RXSTAT(qw, rx_irq);

	/* FIXME Figure out which VAP device has the traffic */
	/* Use the first VAP for now */
	if (mac->vnet[0] != NULL) {
		qdrv_mac_disable_irq(mac, qw->rxirq);
		DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_TRACE,
			"Schedule \"%s\" for RX\n", mac->vnet[0]->name);
		RXSTAT(qw, rx_irq_schedule);
		RXSTAT_SET(qw, rx_poll_stopped, 0);

		qv = netdev_priv(mac->vnet[0]);
		napi_schedule(&qv->napi);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

static __sram_text int qdrv_rx_decap_data_ratelimit(struct qdrv_wlan *qw)
{
	/*
	 * Rate limit checks - ensure we don't blindly process all of the
	 * possibly malicious data frames
	 */
	if (!qw->unknown_dp_jiffies) {
		qw->unknown_dp_jiffies = jiffies;
	}
	qw->unknown_dp_count++;
	if (qw->unknown_dp_count > MAX_UNKNOWN_DP_PER_SECOND) {
		if (time_after(jiffies, qw->unknown_dp_jiffies) &&
				time_before(jiffies, (qw->unknown_dp_jiffies + HZ))) {
			return 0;
		}
		qw->unknown_dp_jiffies = jiffies;
		qw->unknown_dp_count = 1;
	}

	return 1;
}

static int qdrv_rx_is_br_isolate(struct qdrv_wlan *qw, struct sk_buff *skb)
{
	struct vlan_ethhdr *veth = (struct vlan_ethhdr *)(skb->data);
	uint16_t vlanid;

	if ((qw->br_isolate & QDRV_BR_ISOLATE_VLAN)
			&& (veth->h_vlan_proto == __constant_htons(ETH_P_8021Q))) {
		if (qw->br_isolate_vid == QVLAN_VID_ALL)
			return 1;

		vlanid = ntohs(veth->h_vlan_TCI) & VLAN_VID_MASK;
		if (qw->br_isolate_vid == vlanid)
			return 1;
	}

	return 0;
}

/*
 * Determines whether a frame should be accepted, based on information
 * about the frame's origin and encryption, and policy for this vap.
 */
static __sram_text int qdrv_accept_data_frame(struct qdrv_wlan *qw, struct ieee80211vap *vap,
	struct ieee80211_node *ni, const struct ieee80211_qosframe_addr4 *wh,
	struct sk_buff *skb, __be16 ether_type)
{
	char *err = NULL;
	struct ether_header *eh = (struct ether_header *)skb->data;
	int key = (wh != NULL) ? (wh->i_fc[1] & IEEE80211_FC1_PROT) : 0;

	/*
	 * Data frames from unknown nodes should not make it here, but just in case...
	 */
	if (!ni || ((vap->iv_opmode == IEEE80211_M_HOSTAP) && (skb->src_port == 0))) {
		vap->iv_stats.is_rx_unauth++;
		vap->iv_devstats.rx_errors++;
		err = "unknown node";
	} else if (qdrv_mac_reserved(eh->ether_shost)) {
		err = "reserved mac";
	} else if (ether_type == __constant_htons(ETH_P_PAE)) {
		/* encrypted eapol is always OK */
		if (key)
			return 1;
		/* cleartext eapol is OK if we don't have pairwise keys yet */
		if (vap->iv_nw_keys[0].wk_cipher == &ieee80211_cipher_none)
			return 1;
		/* cleartext eapol is OK if configured to allow it */
		if (!IEEE80211_VAP_DROPUNENC_EAPOL(vap))
			return 1;
		/* cleartext eapol is OK if other unencrypted is OK */
		if (!(vap->iv_flags & IEEE80211_F_DROPUNENC))
			return 1;

		/* not OK */
		vap->iv_stats.is_rx_unauth++;
		vap->iv_devstats.rx_errors++;
		IEEE80211_NODE_STAT(ni, rx_unauth);
		IEEE80211_NODE_STAT(ni, rx_errors);
		err = "invalid EAP message";
	} else if (!ieee80211_node_is_authorized(ni)) {
		/*
		 * Deny non-PAE frames received prior to authorization.  For
		 * open/shared-key authentication the port is mark authorized after
		 * authentication completes.  For 802.1X the port is not marked
		 * authorized by the authenticator until the handshake has completed.
		 */
		vap->iv_stats.is_rx_unauth++;
		vap->iv_devstats.rx_errors++;
		IEEE80211_NODE_STAT(ni, rx_unauth);
		IEEE80211_NODE_STAT(ni, rx_errors);
		err = "node not authorized";
	} else if (!key &&
		(vap->iv_flags & IEEE80211_F_PRIVACY) &&
		(vap->iv_flags & IEEE80211_F_DROPUNENC)) {

		/*
		 * Frame received from external L2 filter will not have
		 * MAC header. So protection bit will be zero.
		 */
		if (g_l2_ext_filter && skb->ext_l2_filter)
			return 1;

		/* Deny non-PAE frames received without encryption */
		IEEE80211_NODE_STAT(ni, rx_unencrypted);
		err = "not encrypted";
	} else if (qdrv_rx_is_br_isolate(qw, skb)) {
		err = "br isolate";
	}

	if (err) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
			"dropping frame - %s n=%u s=%pM d=%pM t=%04x\n",
			err, IEEE80211_NODE_IDX_UNMAP(skb->src_port),
			eh->ether_shost, eh->ether_dhost, ether_type);
		return 0;
	}

	return 1;
}

static inline int __sram_text qdrv_rx_lncb_should_drop(struct net_device *dev,
		struct ieee80211vap *vap, const struct ether_header *eh, void *p_iphdr)
{
	/*
	 * AP will send mc/bc in 4 addr format packets, so drop this one if
	 * it is to be sent reliably.
	 */
	if (vap->iv_bss && vap->iv_bss->ni_lncb_4addr &&
			qdrv_wlan_is_4addr_mc(eh, p_iphdr, vap, 1) ) {
		return 1;
	}
	return 0;
}

/*
* This function returns:
* 0 if DA remains untouched
* 1 if DA is changed by the qdrv bridge
*/
static int __sram_text qdrv_rx_set_dest_mac(struct qdrv_wlan *qw, struct qdrv_vap *qv,
		struct ether_header *eh, const struct sk_buff *skb)
{
	if ((qv->iv.iv_flags_ext & IEEE80211_FEXT_WDS) ||
			(IEEE80211_IS_MULTICAST(eh->ether_dhost))) {
		return 0;
	} else if (QDRV_FLAG_3ADDR_BRIDGE_ENABLED()) {
		return !qdrv_br_set_dest_mac(&qw->bridge_table, eh, skb);
	} else if (!br_fdb_get_attached_hook ||
			!skb->dev->br_port ||
			!br_fdb_get_attached_hook(skb->dev->br_port->br, eh->ether_dhost)) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_BRIDGE,
			"destination mac %pM not updated from bridge\n",
			eh->ether_dhost);
		return 0;
	}

	return 0;
}

static void qdrv_bridge_set_dest_addr(struct sk_buff *skb, void *eh1)
{
	struct net_device *dev = skb->dev;
	struct qdrv_vap *qv = netdev_priv(dev);
	struct qdrv_wlan *qw = (struct qdrv_wlan *) qv->parent;

	qdrv_rx_set_dest_mac(qw, qv, (struct ether_header *)eh1, skb);
}

#ifdef CONFIG_QVSP
static __always_inline int
qdrv_rx_strm_check(struct sk_buff *skb, struct qdrv_vap *qv, struct ieee80211_node *ni,
	struct ether_header *eh, u8 *data_start, int ac, int32_t tid)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) qv->parent;
	struct iphdr *p_iphdr = (struct iphdr *)data_start;
	uint32_t l2_header_len = data_start - skb->data;

	if (qvsp_is_active(qw->qvsp) && ni &&
			iputil_eth_is_ipv4or6(eh->ether_type) &&
			(skb->len >= (l2_header_len + sizeof(struct udphdr)
				+ iputil_hdrlen(p_iphdr, skb->len - l2_header_len))) &&
			(!IEEE80211_IS_MULTICAST(eh->ether_dhost) ||
				iputil_is_mc_data(eh, p_iphdr))) {
		if (qvsp_strm_check_add(qw->qvsp, QVSP_IF_QDRV_RX, ni, skb, eh, p_iphdr,
				skb->len - (data_start - skb->data), ac, tid)) {
			return 1;
		}
	}

	return 0;
}
#endif

static void qdrv_rx_data_unauthed(struct ieee80211vap *vap, struct ieee80211_node *ni, unsigned char *mac)
{
	uint8_t reason;

	if (vap->iv_state <= IEEE80211_S_AUTH) {
		reason = IEEE80211_REASON_NOT_AUTHED;
	} else {
		reason = IEEE80211_REASON_NOT_ASSOCED;
	}
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH,
			"send deauth to node "MACSTR" for rxing data when state=%d\n",
			MAC2STR(mac), vap->iv_state);
	IEEE80211_SEND_MGMT(ni,	IEEE80211_FC0_SUBTYPE_DEAUTH, reason);

	/*
	 * In case the current AP we are trying to associate didn't clean up for last session,
	 * don't wait for wpa_supplicant's next trying command which is 10s later.
	 * Give AP and STA some time to clean up and then try again quickly.
	 */
	if ((vap->iv_state >= IEEE80211_S_AUTH) &&
		IEEE80211_ADDR_EQ(vap->iv_bss->ni_bssid, mac)) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH,
				"schedule fast rejoin bssid "MACSTR"\n",
				MAC2STR(vap->iv_bss->ni_bssid));
		IEEE80211_ADDR_COPY(vap->iv_sta_fast_rejoin_bssid, vap->iv_bss->ni_bssid);
		mod_timer(&vap->iv_sta_fast_rejoin, jiffies + HZ);
		ieee80211_new_state(vap, IEEE80211_S_INIT, IEEE80211_FC0_SUBTYPE_DEAUTH);
	}
}

static  int __sram_text qdrv_rx_is_tdls_action_frame(struct sk_buff *skb, int hdrlen)
{
	const uint8_t snap_e_header_pref[] = {LLC_SNAP_LSAP, LLC_SNAP_LSAP, LLC_UI, 0x00, 0x00};
	uint8_t *data = &skb->data[hdrlen];
	uint16_t ether_type = get_unaligned((uint16_t*)&data[6]);
	int32_t snap_encap_pref = !memcmp(data, snap_e_header_pref, sizeof(snap_e_header_pref));

	return (snap_encap_pref && (ether_type == htons(ETHERTYPE_80211MGT)));
}

struct qdrv_rx_skb_list
{
	struct sk_buff_head skb_list;
	struct sk_buff *skb_prev;
	int budget;
};

static inline __sram_text void qdrv_rx_skb_list_init(struct qdrv_rx_skb_list *skb_rcv_list, int budget)
{
	__skb_queue_head_init(&skb_rcv_list->skb_list);
	skb_rcv_list->skb_prev = NULL;
	skb_rcv_list->budget = budget;
}

static inline __sram_text void qdrv_rx_skb_list_indicate(struct qdrv_rx_skb_list *skb_rcv_list, int last)
{
	if (last || (skb_queue_len(&skb_rcv_list->skb_list) >=
			skb_rcv_list->budget)) {
		struct sk_buff *skb, *skb_tmp;
		skb_queue_walk_safe(&skb_rcv_list->skb_list, skb, skb_tmp) {
			skb->next = NULL;
			skb = switch_vlan_to_proto_stack(skb, 0);
			if (skb)
				netif_receive_skb(skb);
		}
		if (!last) {
			qdrv_rx_skb_list_init(skb_rcv_list, skb_rcv_list->budget);
		}
	}
}

static inline __sram_text void qdrv_rx_skb_list_append(struct qdrv_rx_skb_list *skb_rcv_list, struct sk_buff *skb)
{
	if (unlikely(!skb_rcv_list->skb_prev)) {
		__skb_queue_head(&skb_rcv_list->skb_list, skb);
	} else {
		__skb_append(skb_rcv_list->skb_prev, skb, &skb_rcv_list->skb_list);
	}
	skb_rcv_list->skb_prev = skb;
}

static int __sram_text handle_rx_msdu(struct qdrv_wlan *qw,
		struct qdrv_vap *qv,
		struct ieee80211_node *ni,
		const struct ieee80211_qosframe_addr4 *wh,
		struct sk_buff *skb,
		bool check_3addr_br);

struct qdrv_rx_decap_context {
	struct qdrv_vap *qv;
	struct ieee80211_node *ni;
	struct sk_buff *skb;
	uint32_t pseudo_rssi;
	struct qdrv_rx_skb_list *skb_rcv_list;
	struct ieee80211_qosframe_addr4 *wh_copy;
	uint32_t skb_done;
};

#define STD_MAGIC_PAYLOAD_REPETITION	16
#define STD_MAGIC_PAYLOAD_LEN		102
void wowlan_encap_std_magic_pattern(uint8_t *match_pattern, uint8_t *addr)
{
	uint8_t br_addr[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t i;
	uint8_t pos = 0;

	IEEE80211_ADDR_COPY(match_pattern, br_addr);
	pos += IEEE80211_ADDR_LEN;

	for (i = 0; i < STD_MAGIC_PAYLOAD_REPETITION; i++) {
		IEEE80211_ADDR_COPY(&match_pattern[pos], addr);
		pos += IEEE80211_ADDR_LEN;
	}
}

static int wowlan_magic_match(const void *data, const uint16_t length,
		const void *magic_pattern, const uint8_t magic_len)
{
	uint8_t *recv;
	const uint8_t *match = magic_pattern;
	const struct ether_header *eh = data;
	const uint16_t *ether_type = &eh->ether_type;
	int i = 0;
	uint16_t len = length;

	if (len < sizeof(struct ether_header)) {
		return 0;
	}

	while(qtn_ether_type_is_vlan(*ether_type)) {
		if (len < sizeof(struct ether_header) + VLAN_HLEN) {
			return 0;
		}
		ether_type += VLAN_HLEN / sizeof(*ether_type);
		len -= VLAN_HLEN;
	}

	recv = (void *)(ether_type + 1);
	len -= sizeof(struct ether_header);

	while (len >= magic_len) {
		while (recv[i] == match[i]) {
			if (++i == magic_len)
				break;
		}

		if (i == magic_len) {
			return 1;
		}

		i = 0;
		len--;
		recv++;
	}

	return 0;
}

void wowlan_wakeup_host(void)
{
#ifndef TOPAZ_AMBER_IP
	gpio_wowlan_output(WOWLAN_GPIO_OUTPUT_PIN, 1);
	udelay(10000);
	gpio_wowlan_output(WOWLAN_GPIO_OUTPUT_PIN, 0);
#else
	/*
	 * In Amber WOWLAN is handled by WIFI2SOC interrupt.
	 */
	amber_trigger_wifi2soc_interrupt(TOPAZ_AMBER_WIFI2SOC_WAKE_ON_WLAN);
#endif
}

int wowlan_magic_process(struct sk_buff *skb, struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ether_header *eh = (struct ether_header *) skb->data;
	void *l3hdr = (eh + 1);
	uint16_t ether_type = QTN_SKB_CB_ETHERTYPE(skb);
	uint8_t match_pattern[MAX_USER_DEFINED_MAGIC_LEN];
	uint8_t wake_flag = 0;

	if (!wowlan_is_magic_packet(ether_type, eh, l3hdr,
		ic->ic_wowlan.wowlan_match,
		ic->ic_wowlan.L2_ether_type,
		ic->ic_wowlan.L3_udp_port)) {
		return 0;
	}

	memset(match_pattern, 0, sizeof(match_pattern));

	if (ic->ic_wowlan.pattern.len != 0)
		wake_flag = wowlan_magic_match(skb->data, skb->len, ic->ic_wowlan.pattern.magic_pattern, ic->ic_wowlan.pattern.len);
	else {
		wowlan_encap_std_magic_pattern(match_pattern, vap->iv_ic->soc_addr);
		wake_flag = wowlan_magic_match(skb->data, skb->len, match_pattern, STD_MAGIC_PAYLOAD_LEN);
		wowlan_encap_std_magic_pattern(match_pattern, vap->iv_bss->ni_bssid);
		wake_flag |= wowlan_magic_match(skb->data, skb->len, match_pattern, STD_MAGIC_PAYLOAD_LEN);
	}

	if (wake_flag) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,
				"%s WoWLAN: Wake up host\n", __func__);
		wowlan_wakeup_host();
		return 1;
	}

	return 0;
}

static inline int qdrv_rx_should_check_3addr_br(struct qdrv_rx_decap_context *ctx)
{
	struct ieee80211_node *ni = ctx->ni;
	struct ieee80211vap *vap = &ctx->qv->iv;

	return (vap->iv_opmode == IEEE80211_M_STA &&
			(!ieee80211_node_is_qtn(ni) ||
			!(vap->iv_flags_ext & IEEE80211_FEXT_WDS)));
}

static inline int qdrv_rx_vlan_ingress(struct qdrv_vap *qv, struct qdrv_node *qn, struct qtn_rx_decap_info *di,
		struct sk_buff *skb)
{
	struct qtn_vlan_dev *vdev = vdev_tbl_lhost[QDRV_WLANID_FROM_DEVID(qv->devid)];

	if (!vlan_enabled)
		return 1;

	M_FLAG_SET(skb, M_VLAN_TAGGED);

	return qtn_vlan_ingress(vdev, IEEE80211_NODE_IDX_UNMAP(qn->qn_node.ni_node_idx),
		di->start, di->vlanid, 1);
}

static int __sram_text qdrv_rx_decap_callback(struct qtn_rx_decap_info *di, void *_ctx)
{
	struct qdrv_rx_decap_context *ctx = _ctx;
	struct qdrv_vap *qv = ctx->qv;
	struct qdrv_wlan *qw = qv->parent;
	struct ieee80211_node *ni = ctx->ni;
	struct qdrv_node *qn = container_of(ni, struct qdrv_node, qn_node);
	struct sk_buff *skb;

	if (unlikely(!di->decapped)) {
		printk(KERN_ERR "%s: not decapped\n", __FUNCTION__);
		return -1;
	}

	if (di->l3_ether_type == htons(ETHERTYPE_8021Q))
		ni->ni_stats.ns_rx_vlan_pkts++;

	di->check_3addr_br = qdrv_rx_should_check_3addr_br(ctx);

	memcpy(di->start, &di->eh, qtn_rx_decap_newhdr_size(di));

	if (di->last_msdu) {
		skb = ctx->skb;
		ctx->skb_done = 1;
	} else {
		skb = skb_clone(ctx->skb, GFP_ATOMIC);
		if (unlikely(skb == NULL)) {
			printk(KERN_ERR "%s: null skb\n", __FUNCTION__);
			return 0;
		}
	}

	skb->data = di->start;
	skb->len = 0;
	skb_reset_network_header(skb);
	skb_reset_tail_pointer(skb);
	skb_put(skb, di->len);
	skb->dev = qv->ndev;
	skb->is_recyclable = 1;
	skb->vlan_tci = di->vlanid;
	QTN_SKB_CB_ETHERTYPE(skb) = di->l3_ether_type;

	if (!qdrv_rx_vlan_ingress(qv, qn, di, skb)) {
		dev_kfree_skb(skb);
		return 0;
	}

	if (handle_rx_msdu(qw, qv, ni, ctx->wh_copy, skb, di->check_3addr_br) == 0) {
		qdrv_rx_skb_list_append(ctx->skb_rcv_list, skb);
		qdrv_rx_skb_list_indicate(ctx->skb_rcv_list, 0);
	}

	return 0;
}

/*
 * check the data frame is valid or not.
 */
static int __sram_text qdrv_rx_data_frm_check(struct ieee80211vap *vap, uint8_t dir,
	struct ieee80211_qosframe_addr4 *pwh)
{
	int ret = 1;
	uint8_t *bssid;

	switch (vap->iv_opmode) {
	case IEEE80211_M_HOSTAP:
		if (unlikely((dir == IEEE80211_FC1_DIR_FROMDS) ||
			(dir == IEEE80211_FC1_DIR_NODS))) {
			ret = 0;
		} else if (dir == IEEE80211_FC1_DIR_TODS) {
			bssid = pwh->i_addr1;

			if (!IEEE80211_ADDR_EQ(bssid, vap->iv_bss->ni_bssid) &&
					!ieee80211_is_bcst(bssid))
				ret = 0;
		}
		break;
	case IEEE80211_M_STA:
		if (unlikely(dir == IEEE80211_FC1_DIR_TODS))
			ret = 0;
		break;
	case IEEE80211_M_WDS:
		if (unlikely(dir != IEEE80211_FC1_DIR_DSTODS))
			ret = 0;
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
	default:
		break;
	}

	return ret;
}

static void qdrv_rx_reject(struct qdrv_wlan *qw, struct ieee80211vap *vap,
				struct ieee80211_qosframe_addr4 *wh_copy)
{
	struct ieee80211_node *ni;
	uint8_t dir = wh_copy->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	if ((dir != IEEE80211_FC1_DIR_NODS) && qdrv_rx_decap_data_ratelimit(qw)) {
		ni = _ieee80211_tmp_node(vap, wh_copy->i_addr2, wh_copy->i_addr2);
		if (ni) {
			IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
						IEEE80211_REASON_NOT_AUTHED);
			ieee80211_free_node(ni);
		}
	}

	RXSTAT(qw, rx_poll_vap_err);
	vap->iv_stats.is_rx_unauth++;
	vap->iv_devstats.rx_errors++;
}

/*
 * Decap the incoming frame.
 * Returns 0 if the frame has been consumed, else 1.
 */
static int __sram_text qdrv_rx_decap(struct qdrv_vap *qv, struct sk_buff *skb,
		struct qdrv_rx_skb_list *skb_rcv_list, uint32_t pseudo_rssi)
{
	uint8_t dir;
	uint8_t type;
	uint8_t subtype;
	uint8_t frag;
	struct qdrv_wlan *qw = qv->parent;
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211vap *vap = &qv->iv;
	struct ieee80211_node *ni = NULL;
	int node_reference_held = 0;
	int more_data = 0;
	struct qdrv_rx_decap_context ctx;
	struct ieee80211_qosframe_addr4 wh_copy;
#if !TOPAZ_RX_ACCELERATE
	int hdrlen;
#endif
	struct qtn_vlan_dev *vdev;
	uint16_t def_vlanid;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	memcpy(&wh_copy, skb->data, sizeof(wh_copy));
	dir = wh_copy.i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh_copy.i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh_copy.i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
	    (ic->ic_flags & IEEE80211_F_SCAN) &&
#ifdef QTN_BG_SCAN
	    !(ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) &&
#endif /* QTN_BG_SCAN */
	    (subtype != IEEE80211_FC0_SUBTYPE_BEACON)) {
		dev_kfree_skb(skb);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return 0;
	}

	if ((type != IEEE80211_FC0_TYPE_MGT) ||
		(subtype != IEEE80211_FC0_SUBTYPE_BEACON)) {
		RXSTAT(qw, rx_non_beacon);
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_PKT_RX,
			"a1=%pM a2=%pM a3=%pM"
			" dir=0x%01x type=0x%01x subtype=0x%01x\n",
			wh_copy.i_addr1, wh_copy.i_addr2, wh_copy.i_addr3,
			dir, type >> 2, subtype >> 4);
	} else {
		RXSTAT(qw, rx_beacon);
	}

	if (IFF_DUMPPKTS_RECV(&wh_copy, DBG_LOG_FUNC)) {
		ieee80211_dump_pkt(ic, skb->data,
			(skb->len > g_dbg_dump_pkt_len) ? g_dbg_dump_pkt_len : skb->len,
			-1, pseudo_rssi);
	}

	if (skb->src_port > 0) {
		ni = ieee80211_find_node_by_node_idx(vap, skb->src_port);
		if (ni) {
			node_reference_held = 1;
			/*
			 * FIXME
			 * skb->src_port may be wrong.
			 * For WLAN_OVER_WDS_P2P test, AP may think that Beacon frames sent by its
			 * WDS peers to be from associated STAs by mistake.
			 * Dig into this issue deeply later.
			 */
			if (!IEEE80211_ADDR_EQ(ni->ni_macaddr, wh_copy.i_addr2) &&
					(type != IEEE80211_FC0_TYPE_CTL)) {
				ieee80211_check_free_node(node_reference_held, ni);
				node_reference_held = 0;
				ni = NULL;
			}
		}
	}

	if (unlikely(ni && (type == IEEE80211_FC0_TYPE_DATA) &&
			!qdrv_rx_data_frm_check(vap, dir, &wh_copy))) {
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_PKT_RX, "ignoring a1=%pM "
			"a2=%pM a3=%pM opmode=0x%01x dir=0x%01x type=0x%01x"
			"subtype=0x%01x\n", wh_copy.i_addr1, wh_copy.i_addr2,
			wh_copy.i_addr3,vap->iv_opmode, dir, type >> 2, subtype >> 4);
		dev_kfree_skb(skb);
		ieee80211_check_free_node(node_reference_held, ni);
		return 0;
	}

	if (!ni) {
		ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *) &wh_copy);
		if (ni) {
			node_reference_held = 1;
		} else if (vap->iv_opmode == IEEE80211_M_STA) {
			if (type == IEEE80211_FC0_TYPE_DATA) {
				qdrv_rx_reject(qw, vap, &wh_copy);
				dev_kfree_skb(skb);
				return 0;
			} else if (type == IEEE80211_FC0_TYPE_MGT) {
				if (ic->ic_flags_qtn & IEEE80211_QTN_MONITOR)
					ni = vap->iv_bss;
			}
		}
	}

	/* Pass up some data packets from unknown stations, so a deauth can be sent */
	if (!ni && (type == IEEE80211_FC0_TYPE_DATA) &&
	    (qv->iv.iv_opmode == IEEE80211_M_HOSTAP)) {
		RXSTAT(qw, rx_data_no_node);

		if (qdrv_rx_decap_data_ratelimit(qw)) {
			RXSTAT(qw, rx_input_all);
			type = ieee80211_input_all(ic, skb, 0, 0);
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return 0;
		}
		dev_kfree_skb(skb);
		return 0;
	}

	if (ni) {
		ni->ni_last_rx = jiffies;

		/* Silently drop frames from blacklisted stations */
		if (ni->ni_blacklist_timeout > 0) {
			vap->iv_devstats.rx_dropped++;
			IEEE80211_NODE_STAT(ni, rx_dropped);
			dev_kfree_skb(skb);
			RXSTAT(qw, rx_blacklist);
			ieee80211_check_free_node(node_reference_held, ni);
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return 0;
		}

		/*
		 * Store the RSSI value previously calculated.
		 */
		if (!((vap->iv_opmode == IEEE80211_M_HOSTAP)
			&& (type == IEEE80211_FC0_TYPE_MGT)
			&& ((subtype == IEEE80211_FC0_SUBTYPE_BEACON) || (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ)))) {
			ni->ni_rssi = (u_int8_t)pseudo_rssi;
		}

		if (type == IEEE80211_FC0_TYPE_DATA) {
			/* if the node is unauthorized, a deauth should be sent by the sta*/
			if (unlikely(vap->iv_state < IEEE80211_S_RUN) &&
			    vap->iv_opmode == IEEE80211_M_STA) {

				if (qdrv_rx_decap_data_ratelimit(qw)) {
					qdrv_rx_data_unauthed(vap, ni, wh_copy.i_addr2);
				}
				dev_kfree_skb(skb);
				RXSTAT(qw, rx_poll_vap_err);
				IEEE80211_NODE_STAT(ni, rx_unauth);
				IEEE80211_NODE_STAT(ni, rx_errors);
				vap->iv_stats.is_rx_unauth++;
				vap->iv_devstats.rx_errors++;
				ieee80211_check_free_node(node_reference_held, ni);
				return 0;
			}

			ni->ni_stats.ns_rx_data++;
			ni->ni_stats.ns_rx_bytes += skb->len;
		}
	}

	frag = ((type != IEEE80211_FC0_TYPE_CTL) &&
		((wh_copy.i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
		(le16_to_cpu(*(__le16 *) wh_copy.i_seq) & IEEE80211_SEQ_FRAG_MASK)));

	/* Pass up non-data, fragmented data for reassembly, or data from unknown nodes */
	if ((type != IEEE80211_FC0_TYPE_DATA) || frag ||
			((type == IEEE80211_FC0_TYPE_DATA) && !ni)) {
		if (frag) {
			RXSTAT(qw, rx_frag);
		}
		if ((type == IEEE80211_FC0_TYPE_DATA) && !ni) {
			/* Rate limit these */
			if (!qdrv_rx_decap_data_ratelimit(qw)) {
				ieee80211_check_free_node(node_reference_held, ni);
				dev_kfree_skb(skb);
				return 0;
			}
		}

		if (ni == NULL) {
			RXSTAT(qw, rx_input_all);
			type = ieee80211_input_all(ic, skb, pseudo_rssi, 0);
		} else {
			RXSTAT(qw, rx_input_node);
			type = ieee80211_input(ni, skb, pseudo_rssi, 0);
			ieee80211_check_free_node(node_reference_held, ni);
		}
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return 0;
	}

	/* check tdls */
	if ((vap->iv_opmode == IEEE80211_M_STA) &&
			qdrv_rx_is_tdls_action_frame(skb, ieee80211_hdrspace(ic, &wh_copy))) {
		RXSTAT(qw, rx_input_node);
		ieee80211_input(ni, skb, pseudo_rssi, 0);
		ieee80211_check_free_node(node_reference_held, ni);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return 0;
	}

	KASSERT(ni != NULL, ("Node must exist here"));

	trace_ippkt_check(skb->data, skb->len, TRACE_IPPKT_LOC_WLAN_RX);
	RXSTAT(qw, rx_packets);

	if ((vap->iv_opmode == IEEE80211_M_STA) &&
			(vap->iv_state == IEEE80211_S_RUN) && (ni == vap->iv_bss)) {
		more_data = wh_copy.i_fc[1] & IEEE80211_FC1_MORE_DATA;
		if (unlikely((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
				vap->iv_ap_buffered && more_data)) {
			ni->ni_flags |= IEEE80211_NODE_PS_DELIVERING;
			ieee80211_send_pspoll(ni);
		} else {
			ni->ni_flags &= ~IEEE80211_NODE_PS_DELIVERING;
		}
	}

	vdev = vdev_tbl_lhost[QDRV_WLANID_FROM_DEVID(qv->devid)];
	if (QVLAN_IS_DYNAMIC(vdev)) {
		def_vlanid = vdev->u.node_vlan[IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx)];
	} else {
		def_vlanid = vdev->pvid;
	}

	ctx.qv = qv;
	ctx.ni = ni;
	ctx.skb = skb;
	ctx.pseudo_rssi = pseudo_rssi;
	ctx.skb_rcv_list = skb_rcv_list;
	ctx.wh_copy = &wh_copy;
	ctx.skb_done = 0;

	if (qtn_rx_decap(&wh_copy, skb->data, skb->len, def_vlanid, &qtn_vlan_info, vlan_enabled,
				&qdrv_rx_decap_callback, &ctx, NULL) == QTN_RX_DECAP_TRAINING) {
		RXSTAT(qw, rx_rate_train_invalid);
	}
	if (ctx.skb_done == 0) {
		dev_kfree_skb(skb);
	}

	ieee80211_check_free_node(node_reference_held, ni);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 1;
}

static int rx_fifo_init(struct qdrv_wlan *qw, struct host_fifo_if *hfif)
{
	struct host_descfifo *fifo_desc;
	int i;

	if (hfif == NULL) {
		DBGPRINTF_E("FIFO input is empty\n");
		return -ENOMEM;
	}

	if (hfif->fifo == NULL) {
		DBGPRINTF_E("FIFO information is empty\n");
		return -ENOMEM;
	}

	fifo_desc = hfif->fifo;
	fifo_desc->df_numelems = fifo_desc->df_size;

	hfif->ring_size = fifo_desc->df_size;
	hfif->descp = kzalloc(sizeof(hfif->descp[0]) * hfif->ring_size, GFP_KERNEL);
	if (hfif->descp == NULL) {
		return -ENOMEM;
	}
	hfif->pending = NULL;
	hfif->df_rxdesc_cache = dma_pool_create("rxdesc", NULL,
					sizeof(struct host_rxdesc), 8, 0);
	if (hfif->df_rxdesc_cache == NULL) {
		kfree(hfif->descp);
		printk("create rxdesc pool error!\n");
		return -ENOMEM;
	}

	/*
	 * Set pointers in pointer array to the descriptors in the
	 * descriptor array
	 */
	for (i = 0; i < hfif->ring_size; i++ ) {
		struct host_rxdesc *rxdesc, *rd_dma;

		rxdesc = rx_alloc_desc(qw, hfif->df_rxdesc_cache, &rd_dma);
		if (rxdesc == NULL) {
			DBGPRINTF_E("Unable to allocate descriptor\n");
			return -ENOMEM;
		}

		if (prev_rxdesc) {
			prev_rxdesc->rd_next = rd_dma;
		} else {
			fifo_desc->df_fifo = rd_dma;
		}
		prev_rxdesc = rxdesc;

		hfif->descp[i] = rxdesc;
	}

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_PKT_RX,
		"unit %d hfif %p ringsize %d\n",
		qw->unit, hfif, hfif->ring_size);

	return 0;
}

static inline int qdrv_rx_check_bridge(struct net_device *dev, struct sk_buff *skb,
					uint8_t *addr, struct iphdr *p_iphdr)
{
	unsigned char igmp_snoop;

	if (IEEE80211_ADDR_BCAST(addr))
		return 1;

	if (likely(dev->br_port && dev->br_port->br))
		igmp_snoop = dev->br_port->br->igmp_snoop_enabled;
	else
		igmp_snoop = BR_IGMP_SNOOP_DISABLED;

	if (igmp_snoop == BR_IGMP_SNOOP_DISABLED && IEEE80211_IS_MULTICAST(addr))
		return 1;

	return 0;
}

static inline int qdrv_rx_should_send_to_bss(struct net_device *dev, struct ieee80211vap *vap,
		struct sk_buff *skb)
{
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	u16 ether_type;
	struct ether_header *eh = (struct ether_header *) skb->data;
	u8 *data_start = qdrv_sch_find_data_start(skb, eh, &ether_type);
	struct iphdr *p_iphdr = (struct iphdr *)data_start;
	struct igmphdr *igmp_p;
	struct qdrv_vap *qv = netdev_priv(dev);
	struct qdrv_wlan *qw = (struct qdrv_wlan *)qv->parent;
	uint8_t *addr = eh->ether_dhost;
	uint32_t data_len;
#ifdef CONFIG_IPV6
	struct ipv6hdr *ip6hdr_p;
	uint8_t nexthdr;
	struct icmp6hdr *icmp6hdr;
	int nhdr_off;
#endif

	/* drop if Intra BSS isolation enabled */
	if (br_get_ap_isolate() || QTN_FLAG_IS_INTRA_BSS(skb->dev->qtn_flags))
		return 0;

	/* If enabled, always send back LNCB and SSDP packets to the BSS */
	if (vap->iv_ap_fwd_lncb) {
		if (iputil_is_lncb((uint8_t *)eh, p_iphdr) ||
				iputil_is_ssdp(eh->ether_dhost, p_iphdr)) {
			return 1;
		}
	}

	/*
	 * Deliver the IGMP frames to linux bridge module
	 * Send non-snooped multicast back to the BSS
	 */
	if (iputil_eth_is_v4_multicast(eh)) {
		if (p_iphdr->protocol == IPPROTO_IGMP) {
			RXSTAT(qw, rx_igmp);
			igmp_p = iputil_igmp_hdr(p_iphdr);

			DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_PKT_RX,
					"RX IGMP: type 0x%x src=%pM dst=%pM\n",
					igmp_p->type, eh->ether_shost, eh->ether_dhost);
			if (igmp_p->type == IGMP_HOST_MEMBERSHIP_REPORT ||
					igmp_p->type == IGMPV2_HOST_MEMBERSHIP_REPORT) {
				return 0;
			}

			return 1;
		}

		return qdrv_rx_check_bridge(dev, skb, addr, p_iphdr);
#ifdef CONFIG_IPV6
	} else if (iputil_eth_is_v6_multicast(eh)) {
		data_len = skb->len - (data_start - skb->data);
		ip6hdr_p = (struct ipv6hdr *)data_start;
		nhdr_off = iputil_v6_skip_exthdr(ip6hdr_p, sizeof(struct ipv6hdr),
			&nexthdr, (skb->len - ((uint8_t *)ip6hdr_p - skb->data)), NULL, NULL);
		 if (nexthdr == IPPROTO_ICMPV6) {
			 icmp6hdr = (struct icmp6hdr*)(data_start + nhdr_off);
			 if (icmp6hdr->icmp6_type == ICMPV6_MGM_REPORT) {
				 RXSTAT(qw, rx_igmp);
				 DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_PKT_RX,
						 "RX MLD: type 0x%x src=%pM dst=%pM\n",
						 icmp6hdr->icmp6_type, eh->ether_shost, eh->ether_dhost);
				 return 0;
			 } else if (icmp6hdr->icmp6_type == ICMPV6_MGM_QUERY ||
					 icmp6hdr->icmp6_type == ICMPV6_MLD2_REPORT ||
					 icmp6hdr->icmp6_type == ICMPV6_MGM_REDUCTION) {
				 DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_PKT_RX,
						 "RX MLD: type 0x%x src=%pM dst=%pM\n",
						 icmp6hdr->icmp6_type, eh->ether_shost, eh->ether_dhost);
				 RXSTAT(qw, rx_igmp);
			 }

			 return 1;
		}

		return qdrv_rx_check_bridge(dev, skb, addr, p_iphdr);
#endif
	}

#endif
	return 1;
}

static inline int __sram_text qdrv_rx_mcast_should_drop(struct net_device *dev,
		struct qdrv_vap *qv, struct sk_buff *skb, struct ieee80211_node *ni)
{
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	struct net_bridge_fdb_entry *f = NULL;
	struct ether_header *eh = (struct ether_header *) skb->data;
#endif
	struct ieee80211vap *vap = ni->ni_vap;

	if ((vap->iv_opmode == IEEE80211_M_STA) && !IEEE80211_NODE_IS_NONE_TDLS(ni)) {
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_PKT_RX,
			"drop multicast frame from tdls peer [%pM]\n", ni->ni_macaddr);
		return 1;
	}

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	f = br_fdb_get_hook(dev->br_port->br, skb, eh->ether_shost);

	if ((f != NULL) && (f->dst->dev != dev) &&
			(f->dst->state == BR_STATE_FORWARDING) &&
			time_after(f->ageing_timer, jiffies - (5 * HZ))) {
		/* hit from bridge table, drop it since it is probably a multicast echo */
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_BRIDGE,
			"src=%pM dst=%pM multicast echo\n",
			eh->ether_shost, eh->ether_dhost);
		br_fdb_put_hook(f);
		return 1;
	}
	if ((f != NULL) && (f->is_local)) {
		/* hit from bridge table, drop it since it is probably a multicast echo */
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_BRIDGE,
			"src=%pM dst=%pM multicast echo\n",
			eh->ether_shost, eh->ether_dhost);
		br_fdb_put_hook(f);
		return 1;
	}
	if (f) {
		br_fdb_put_hook(f);
	}
#endif //CONFIG_BRIDGE...
	return 0;
}

/*
 * 1. Save IP corresponding MAC Address to database
 * 2. Replace MAC for corresponding IP in packets
 * 3. Replace MAC for existing IP in database
 *
 * Fixme: when should we remove these nodes?
 */

void qdrv_dump_replace_db(struct qdrv_wlan *qw)
{
	struct ieee80211com *ic = &qw->ic;
	struct ip_mac_mapping *q = ic->ic_ip_mac_mapping;

	printk("Current replace db:\n");
	while (q) {
		printk("entry: IP " NIPQUAD_FMT ": %pM\n",
			NIPQUAD(q->ip_addr), q->mac);
		q = q->next;
	}
	printk("End of replace db\n");
}

static void
qdrv_replace_handle_ip(struct qdrv_wlan *qw, struct sk_buff *skb, u_int8_t *client_mac,
		u_int32_t client_ip, ip_mac_flag save_replace_flag)
{
	struct ether_header *eh = (struct ether_header *) skb->data;
	struct ieee80211com *ic = &qw->ic;
	struct ip_mac_mapping *p;
	struct ip_mac_mapping *q = ic->ic_ip_mac_mapping;

	if (client_ip == 0)
		return;

	/* First IP to store in database */
	if (save_replace_flag == SAVE_IP_MAC && !ic->ic_ip_mac_mapping) {
		ic->ic_ip_mac_mapping = (struct ip_mac_mapping *)kmalloc(sizeof(struct ip_mac_mapping), GFP_KERNEL);
		if (!ic->ic_ip_mac_mapping) {
			printk("***CRITICAL*** Cannot allocate memory for head in %s\n",
				__FUNCTION__);
			return;
		}
		memset(ic->ic_ip_mac_mapping, 0, sizeof(struct ip_mac_mapping));
		ic->ic_ip_mac_mapping->ip_addr = client_ip;
		memcpy(ic->ic_ip_mac_mapping->mac, client_mac, ETHER_ADDR_LEN);
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
			"(head) insert to database for IP " NIPQUAD_FMT ": %pM\n",
			NIPQUAD(ic->ic_ip_mac_mapping->ip_addr),
			ic->ic_ip_mac_mapping->mac);
		return;
	}

	/* Replace current packets */
	if (save_replace_flag == REPLACE_IP_MAC) {
		while (q) {
			if (q->ip_addr == client_ip) {
				if (compare_ether_addr(eh->ether_shost, q->mac)) {
					DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
						"replacing packets for IP " NIPQUAD_FMT ": %pM->%pM\n",
						NIPQUAD(client_ip), eh->ether_shost, q->mac);
					memcpy(eh->ether_shost, q->mac, ETHER_ADDR_LEN);
				}
				return;
			}
			q = q->next;
		}

		return;
	}

	while (q) {
		/* IP Already here but MAC may be changed */
		if (q->ip_addr == client_ip) {
			if (compare_ether_addr(q->mac, client_mac)) {
				DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
					"database change for IP " NIPQUAD_FMT ": %pM->%pM\n",
					NIPQUAD(client_ip), q->mac, client_mac);
				memcpy(q->mac, client_mac, ETHER_ADDR_LEN);
			}
			return;
		}
		if (q->next == NULL)
			break;
		q = q->next;
	}

	/* Save corresponding IP and MAC to database as a new entry */
	p = (struct ip_mac_mapping *)kmalloc(sizeof(struct ip_mac_mapping), GFP_KERNEL);
	if (p == NULL) {
		printk("****CRITICAL**** memory allocation failed in %s\n", __FUNCTION__);
		return;
	}

	memset(p, 0, sizeof(struct ip_mac_mapping));
	p->ip_addr = client_ip;
	memcpy(p->mac, client_mac, ETHER_ADDR_LEN);

	q->next = p;
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
			"insert to database for IP " NIPQUAD_FMT ": %pM\n",
			NIPQUAD(p->ip_addr), p->mac);

	return;
}

static const uint16_t *
get_ether_type_skip_vlan(const struct ether_header *eh, uint32_t len)
{
	const uint16_t *ether_type_p = &eh->ether_type;

	if (len < sizeof(struct ether_header))
		return NULL;

	while(qtn_ether_type_is_vlan(*ether_type_p)) {
		if (len < sizeof(struct ether_header) + VLAN_HLEN)
			return NULL;
		ether_type_p += VLAN_HLEN / sizeof(*ether_type_p);
		len -= VLAN_HLEN;
	}

	return ether_type_p;
}

static void qdrv_replace_handle_arp(struct qdrv_wlan *qw, struct sk_buff *skb)
{
	struct ether_header *eh = (struct ether_header *) skb->data;
	struct arp_message *arp_p = NULL;
	const uint16_t *ether_type_p = NULL;

	ether_type_p = get_ether_type_skip_vlan(eh, skb->len);
	if (unlikely(!ether_type_p))
		return;
	arp_p = (void *)(ether_type_p + 1);

	qdrv_replace_handle_ip(qw, skb, arp_p->shost, arp_p->sipaddr, SAVE_IP_MAC);

	if (unlikely(compare_ether_addr(eh->ether_shost, arp_p->shost))) {
		memcpy(eh->ether_shost, arp_p->shost, ETHER_ADDR_LEN);
	}

	return;
}

static uint32_t qdrv_find_src_vendor(struct qdrv_vap *qv, struct sk_buff *skb)
{
	struct ieee80211vap *vap = &qv->iv;
	struct ieee80211_node *ni = NULL;
	uint32_t vendor = 0;

	ni = ieee80211_find_node_by_node_idx(vap, skb->src_port);
	if (ni) {
		if (ni->ni_brcm_flags) {
			vendor |= PEER_VENDOR_BRCM;
		}
		ieee80211_free_node(ni);
	}

	return vendor;
}

/*
 * Replace Source MAC Address and BOOTP Client Address with Client Identifier
 */
static inline void __sram_text qdrv_replace_dhcp_packets_header(struct sk_buff *skb, struct iphdr *iphdr_p)
{
	struct ether_header *eh = (struct ether_header *) skb->data;
	struct udphdr *uh = (struct udphdr*)(iphdr_p + 1);
	struct dhcp_message *dhcp_msg = (struct dhcp_message *)((u8 *)uh + sizeof(struct udphdr));

	u8 *frm;
	u8 *efrm;
	int chsum = 0;
	__wsum csum;


	if (dhcp_msg->op != BOOTREQUEST || dhcp_msg->htype != ARPHRD_ETHER)
		return;

	frm = (u8 *)(dhcp_msg->options);
	efrm = skb->data + skb->len;

	while (frm < efrm) {
		if (*frm == 0x3d && *(frm + 1) == 0x07 && *(frm + 2) == 0x01) {
			if (memcmp(dhcp_msg->chaddr, frm + 3, ETHER_ADDR_LEN)) {
				memcpy(dhcp_msg->chaddr, frm + 3, ETHER_ADDR_LEN);
			}
			if (memcmp(eh->ether_shost, frm + 3, ETHER_ADDR_LEN)) {
				memcpy(eh->ether_shost, frm + 3, ETHER_ADDR_LEN);
			}
			chsum = 1;
			break;
		}
		frm += *(frm+1) + 2;
	}

	/* Recalculate the UDP checksum */
	if (chsum && uh->check != 0) {
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

	return;
}

void qdrv_tqe_send_l2_ext_filter(struct qdrv_wlan *qw, struct sk_buff *skb)
{
	union topaz_tqe_cpuif_ppctl ppctl;
	uint8_t port = g_l2_ext_filter_port;
	uint8_t node = 0;

	topaz_tqe_cpuif_ppctl_init(&ppctl, port, &node, 1,
			0, 1, 1, 0, 1, 0);

	if (unlikely(tqe_tx(&ppctl, skb) == NETDEV_TX_BUSY)) {
		dev_kfree_skb(skb);
		TXSTAT(qw, tx_drop_l2_ext_filter);
	} else {
		TXSTAT(qw, tx_l2_ext_filter);
	}
}

static inline int qdrv_restrict_wlan_ip(struct qdrv_wlan *qw, struct iphdr *iphdr_p)
{
	if (qw->restrict_wlan_ip && qdrv_is_bridge_ipaddr(qw, iphdr_p->daddr))
		return 1;
	return 0;
}

static int __sram_text handle_rx_msdu(struct qdrv_wlan *qw,
		struct qdrv_vap *qv,
		struct ieee80211_node *ni,
		const struct ieee80211_qosframe_addr4 *wh,
		struct sk_buff *skb,
		bool check_3addr_br)
{
#define CHECK_VENDOR	do {						\
		if (!vendor_checked) {					\
			vendor = qdrv_find_src_vendor(qv, skb);		\
			vendor_checked = 1;				\
		}							\
	} while (0)

	struct ieee80211com *ic = &qw->ic;
	struct ieee80211vap *vap = &qv->iv;
	struct net_device *dev = vap->iv_dev;
	struct ether_header *eh = (struct ether_header *) skb->data;
	void *l3hdr = NULL;
	struct iphdr *iphdr_p = NULL;
	const uint16_t *ether_type_p = NULL;
	uint8_t ip_proto = 0;
	uint32_t vendor = 0;
	int vendor_checked = 0;
	int rc;
	uint16_t ether_type = QTN_SKB_CB_ETHERTYPE(skb);
	int mu = STATS_SU;
	void *proto_data = NULL;

	if (unlikely(ic->ic_wowlan.host_state)) {
		if (wowlan_magic_process(skb, vap)) {
			dev_kfree_skb(skb);
			return 1;
		}
	}

	ether_type_p = get_ether_type_skip_vlan(eh, skb->len);
	if (unlikely(!ether_type_p)) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,
			"Fail to get Ether type %pM\n", eh->ether_shost);
		dev_kfree_skb(skb);
		return 1;
	}

	l3hdr = (void *)(ether_type_p + 1);

	if (check_3addr_br) {
		/*
		 * Set the destination MAC address
		 * - if in 4-address mode or the frame is multicast, use the supplied DA
		 * - if 3-address bridge mode is enabled, get the DA from the qdrv bridge
		 * - in standard 3-address mode, use the bridge device's MAC
		 *   address as the destination (if found), otherwise use the supplied DA
		 */
		qdrv_rx_set_dest_mac(qw, qv, eh, skb);
		if (qdrv_rx_lncb_should_drop(dev, vap, eh, l3hdr)) {
			int igmp_type = qdrv_igmp_type(l3hdr, skb->len - sizeof(*eh));
			if (igmp_type != 0) {
				RXSTAT(qw, rx_igmp_3_drop);
			}
			dev_kfree_skb(skb);
			RXSTAT(qw, rx_mc_3_drop);
			return 1;
		}
	}

	/* Check if node is authorized to receive */
	if (!qdrv_accept_data_frame(qw, vap, ni, wh, skb, ether_type)) {
		dev_kfree_skb(skb);
		RXSTAT(qw, rx_poll_vap_err);
		return 1;
	}

	if ((ether_type == __constant_htons(ETH_P_IP)) ||
			(ether_type == __constant_htons(ETH_P_IPV6))) {
		iphdr_p = l3hdr;
		ip_proto = iputil_proto_info(iphdr_p, skb, &proto_data, NULL, NULL);
	}

	if (bcast_pps_should_drop(eh->ether_dhost, &vap->bcast_pps, *ether_type_p,
				ip_proto, proto_data, 1)) {
		dev_kfree_skb(skb);
		return 1;
	}

#if TOPAZ_RX_ACCELERATE
	if (ieee80211_tdls_tqe_path_check(ni, skb,
				ni->ni_shared_stats->tx[mu].avg_rssi_dbm,
				ether_type)) {
		dev_kfree_skb(skb);
		return 1;
	}
#endif

	/* FIXME If EAPOL need passing externally, this needs fixing up */
	if (unlikely(g_l2_ext_filter)) {
		if (!skb->ext_l2_filter &&
				vap->iv_opmode == IEEE80211_M_HOSTAP &&
				!(ether_type == __constant_htons(ETH_P_PAE) &&
				IEEE80211_ADDR_EQ(eh->ether_dhost, vap->iv_myaddr))) {
			qdrv_tqe_send_l2_ext_filter(qw, skb);
			return 1;
		}
	}

	qdrv_wlan_stats_prot(qw, 0, ether_type, ip_proto);

	if (vap->proxy_arp) {
		if (ether_type == __constant_htons(ETH_P_ARP)) {
			if (qdrv_proxy_arp(vap, qw, ni, (uint8_t*)l3hdr)) {
				dev_kfree_skb(skb);
				return 1;
			}
#ifdef CONFIG_IPV6
		} else if (ether_type == __constant_htons(ETH_P_IPV6)) {
			if (qdrv_wlan_handle_neigh_msg(vap, qw, (uint8_t*)l3hdr, 0,
					skb, ip_proto, proto_data)) {
				dev_kfree_skb(skb);
				return 1;
			}
#endif
		}
	}

	if (unlikely((ic->ic_vendor_fix & (VENDOR_FIX_BRCM_DHCP |
						VENDOR_FIX_BRCM_DROP_STA_IGMPQUERY |
						VENDOR_FIX_BRCM_REPLACE_IGMP_SRCMAC)) &&
			(qv->iv.iv_opmode == IEEE80211_M_HOSTAP))) {

		if (unlikely(ether_type == __constant_htons(ETH_P_ARP))) {
			CHECK_VENDOR;
			if (vendor & PEER_VENDOR_BRCM) {
				qdrv_replace_handle_arp(qw, skb);
			}
		}

		if (unlikely(iphdr_p)) {
			struct igmphdr *igmp_p = (struct igmphdr *)((unsigned int*)iphdr_p + iphdr_p->ihl);

			if ((ic->ic_vendor_fix & VENDOR_FIX_BRCM_DROP_STA_IGMPQUERY) &&
					(ip_proto == IPPROTO_IGMP) &&
					(igmp_p->type == IGMP_HOST_MEMBERSHIP_QUERY)) {
				DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
					"suspicious IGMP query received from %pM " NIPQUAD_FMT "\n",
					eh->ether_shost, NIPQUAD(iphdr_p->saddr));
				CHECK_VENDOR;
				if (vendor & PEER_VENDOR_BRCM) {
					DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
						"drop IGMP query from Broadcom STA\n");
					dev_kfree_skb(skb);
					return 1;
				}
			}

			if ((ic->ic_vendor_fix & VENDOR_FIX_BRCM_REPLACE_IGMP_SRCMAC) &&
					(ip_proto == IPPROTO_IGMP)) {
				DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
					"IGMP msg received from %pM " NIPQUAD_FMT ", type=0x%x\n",
					eh->ether_shost, NIPQUAD(iphdr_p->saddr), igmp_p->type);
				switch (igmp_p->type) {
				case IGMP_HOST_MEMBERSHIP_REPORT:
				case IGMPV2_HOST_MEMBERSHIP_REPORT:
				case IGMPV3_HOST_MEMBERSHIP_REPORT:
				case IGMP_HOST_LEAVE_MESSAGE:
					break;
				default:
					DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
						"suspicious IGMP type 0x%x for group "NIPQUAD_FMT"\n",
						igmp_p->type, NIPQUAD(igmp_p->group));
					break;
				}
				CHECK_VENDOR;
				if (vendor & PEER_VENDOR_BRCM) {
					qdrv_replace_handle_ip(qw, skb, NULL, iphdr_p->saddr, REPLACE_IP_MAC);
				}
			}

			if ((ic->ic_vendor_fix & VENDOR_FIX_BRCM_DHCP) &&
					IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
					(ip_proto == IPPROTO_UDP)) {
				CHECK_VENDOR;
				if (vendor & PEER_VENDOR_BRCM) {
					qdrv_replace_dhcp_packets_header(skb, iphdr_p);
				}
			}

			if ((ic->ic_vendor_fix & VENDOR_FIX_BRCM_REPLACE_IP_SRCMAC) &&
					((ip_proto != IPPROTO_IGMP))) {
				DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
					"pkt received from %pM " NIPQUAD_FMT ", ip_protocol=0x%x\n",
					eh->ether_shost, NIPQUAD(iphdr_p->saddr), ip_proto);
				CHECK_VENDOR;
				if (vendor & PEER_VENDOR_BRCM) {
					qdrv_replace_handle_ip(qw, skb, NULL, iphdr_p->saddr, REPLACE_IP_MAC);
				}
			}
		}
	}

	/*
	 * If the destination is multicast, LNCB, or another node on the BSS, send the packet back
	 * to the BSS.
	 */
	if ((qv->iv.iv_opmode == IEEE80211_M_HOSTAP) &&
			IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
			qdrv_rx_should_send_to_bss(dev, vap, skb)) {
		struct sk_buff *skb2;
		skb->is_recyclable = 0;
		skb2 = skb_copy(skb, GFP_ATOMIC);
		if (skb2 != NULL) {
			DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX, "send Rx pkt back to BSS\n");
			skb2->is_recyclable = 0;
			M_FLAG_SET(skb2, M_ORIG_OUTSIDE);
			QDRV_TX_DBG(2, NULL, "back to BSS - skb=%p\n", skb);
			rc = dev_queue_xmit(skb2);
			if (rc != NET_XMIT_SUCCESS) {
				QDRV_TX_DBG(2, NULL, "back to BSS failed rc=%u\n", rc);
			}
		}
	}

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	if (br_fdb_get_hook != NULL && dev->br_port != NULL) {
		if (unlikely( (!compare_ether_addr(eh->ether_dhost, skb->dev->dev_addr)) ||
				(!compare_ether_addr(eh->ether_dhost, dev->br_port->dev->dev_addr)))) {
			skb->is_recyclable = 0;
		}

		if ((qv->iv.iv_opmode != IEEE80211_M_HOSTAP) &&
				IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
				qdrv_rx_mcast_should_drop(dev, qv, skb, ni)) {
			dev_kfree_skb(skb);
			return 1;
		}
	}
#endif /* defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE) */

	if (*ether_type_p == __constant_htons(ETH_P_IP)) {
		if (qdrv_restrict_wlan_ip(qw, (struct iphdr *)l3hdr)) {
			dev_kfree_skb(skb);
			return 1;
		}
	}

	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		if (!compare_ether_addr(eh->ether_dhost, vap->iv_dev->broadcast)) {
			vap->iv_devstats.rx_broadcast_packets++;
			ni->ni_stats.ns_rx_bcast++;
		} else {
			vap->iv_devstats.multicast++;
			ni->ni_stats.ns_rx_mcast++;
		}
	} else {
		vap->iv_devstats.rx_unicast_packets++;
		ni->ni_stats.ns_rx_ucast++;
	}

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_PKT_RX,
		"send to stack src=%pM dst=%pM\n",
		 eh->ether_shost, eh->ether_dhost);
	skb->protocol = eth_type_trans(skb, skb->dev);
	trace_skb_perf_stamp_call(skb);

	return 0;
}

static struct sk_buff *qdrv_rx_rxdesc_get_skb(struct host_rxdesc *rxdesc, int pktlen)
{
#if TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT
	void *buf_bus = rxdesc->rd_buffer;
	struct sk_buff *skb = NULL;

	if (likely(buf_bus)) {
#if TOPAZ_HBM_BUF_WMAC_RX_QUARANTINE
		skb = topaz_hbm_attach_skb_quarantine(bus_to_virt((unsigned int)buf_bus),
				TOPAZ_HBM_BUF_WMAC_RX_POOL, pktlen, NULL);
		/* no matter if new buf is used, no need for original one */
		topaz_hbm_release_buf_safe(buf_bus);
#else
		skb = topaz_hbm_attach_skb_bus(buf_bus,	TOPAZ_HBM_BUF_WMAC_RX_POOL);
		if (unlikely(skb == NULL)) {
			topaz_hbm_put_payload_aligned_bus(buf_bus, TOPAZ_HBM_BUF_WMAC_RX_POOL);
		}
#endif
	}
	return skb;
#else
	return (struct sk_buff *) rxdesc->skbuff;
#endif
}

int __sram_text qdrv_rx_poll(struct napi_struct *napi, int budget)
{
	struct qdrv_vap *qv = container_of(napi, struct qdrv_vap, napi);
	struct qdrv_vap *qv1 = qv;
	struct qdrv_wlan *qw = (struct qdrv_wlan *) qv->parent;
	struct host_rxdesc *rxdesc = NULL;
	int processed = 0;
	u_int32_t df_numelems;
	struct qdrv_rx_skb_list skb_rcv_list;

	DBGPRINTF_LIMIT(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	RXSTAT(qw, rx_poll);

	/* Init pending list of skb to be indicated */
	qdrv_rx_skb_list_init(&skb_rcv_list, budget);

	/* Process left over from last poll */
	if (qw->rx_if.rx.pending != NULL) {
		rxdesc = qw->rx_if.rx.pending;
		qw->rx_if.rx.pending = NULL;
		RXSTAT(qw, rx_poll_pending);
	}

	DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_PKT_RX,
			"Limit %d sem @ 0x%08x bit %d\n",
			budget, (unsigned int) qw->host_sem, qw->rx_if.rx_sem_bit);

	while (processed < budget) {
		struct sk_buff *skb;
		struct ieee80211_node *ni;
		int pktlen;
		int node_idx_unmapped;
		uint32_t pseudo_rssi;

		if (rxdesc == NULL) {
			/* No previous data. */
			while (!sem_take(qw->host_sem, qw->scan_if.scan_sem_bit));
			rxdesc = qw->rx_if.rx.fifo->hrdstart;
			if (rxdesc) {
				writel_wmb(NULL, &qw->rx_if.rx.fifo->hrdstart);
			}
			sem_give(qw->host_sem, qw->scan_if.scan_sem_bit);

			if (rxdesc == NULL) {
				/* Nothing more to process */
				RXSTAT(qw, rx_poll_empty);
				break;
			}

			DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_PKT_RX, "Retrieving\n");
			RXSTAT(qw, rx_poll_retrieving);
		}

		DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_PKT_RX,
				"rxdesc 0x%08x buf 0x%08x stat 0x%08x\n",
				(unsigned int) rxdesc,
				(unsigned int) rxdesc->rd_buffer,
				rxdesc->rs_statword);

		pktlen = (rxdesc->rs_statword >> 16) & 0xffff;
		node_idx_unmapped = (rxdesc->rs_statword >> 2) & 0x3ff;
		pseudo_rssi = 70 - (rxdesc->gain_db & 0xFF);

		/* Check descriptor */
		if (pktlen == 0xFFFF) {
			skb = NULL; /* This skb has been given to DSP for action frame */
			goto alloc_desc_buff;
		} else if (!pktlen) {
			if (likely(rxdesc->rd_buffer)) {
				topaz_hbm_put_payload_aligned_bus((void *)rxdesc->rd_buffer,
						TOPAZ_HBM_BUF_WMAC_RX_POOL);
			}
			goto alloc_desc_buff;
		} else if (!(rxdesc->rs_statword & MUC_RXSTATUS_DONE)) {
			DBGPRINTF_E("Done bit not set for descriptor 0x%08x\n",
					(unsigned int) rxdesc);
			if (likely(rxdesc->rd_buffer)) {
				topaz_hbm_put_payload_aligned_bus((void *)rxdesc->rd_buffer,
						TOPAZ_HBM_BUF_WMAC_RX_POOL);
			}
			goto alloc_desc_buff;
		} else if ((skb = qdrv_rx_rxdesc_get_skb(rxdesc, pktlen)) == NULL) {
			/*
			DBGPRINTF_E("No buffer for descriptor 0x%08x\n",
					(unsigned int) rxdesc);
			*/
			RXSTAT(qw, rx_poll_buffer_err);
			goto alloc_desc_buff;
		}

		trace_skb_perf_stamp_call(skb);

		ni = qw->ic.ic_node_idx_ni[node_idx_unmapped];
		if (ni) {
			qv = container_of(ni->ni_vap, struct qdrv_vap, iv);
		} else {
			qv = netdev_priv(qw->mac->vnet[0]);
		}

		DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_PKT_RX,
				"Processed %d pktlen %d node_idx 0x%x ni %p\n",
				processed, pktlen, node_idx_unmapped, ni);

		if (unlikely(!qv)) {
			RXSTAT(qw, rx_poll_vap_err);
			kfree_skb(skb);
			goto alloc_desc_buff;
		}

		/* Prepare the skb */
		skb->dev = qv->ndev;
		skb->src_port = IEEE80211_NODE_IDX_MAP(node_idx_unmapped);
		/*
		 * CCMP MIC error frames may be decapsulated into garbage packets
		 * with invalid MAC DA, messing up the forwarding table.
		 */
		if (unlikely(MS(rxdesc->rs_statword, MUC_RXSTATUS_MIC_ERR))) {
			M_FLAG_SET(skb, M_NO_L2_LRN);
		}
		skb_put(skb, pktlen);
		topaz_hbm_debug_stamp(skb->head, TOPAZ_HBM_OWNER_LH_RX_MBOX, pktlen);
		/* Decap the 802.11 frame into one or more (A-MSDU) Ethernet frames */
		processed += qdrv_rx_decap(qv, skb, &skb_rcv_list, pseudo_rssi);

alloc_desc_buff:
		if (rxdesc_alloc_buffer(qw, rxdesc)) {
			/*
			 * Let's break loop, current descriptor will be added to pending.
			 * Next time we will try to allocate again.
			 */
			RXSTAT(qw, rx_poll_skballoc_err);
			DBGPRINTF_E("Failed to allocate skb for descriptor 0x%08x\n",
				(unsigned int) rxdesc);
			break;
		}
		/*
		 * Get the physical ring pointer we are going to process now
		 * and pass the previous pointer to the MuC. There is 1 descriptor
		 * as buffer zone to prevent overwrites.
		 */
		if (prev_rxdesc) {
			writel_wmb(rxdesc->rd_pa, &prev_rxdesc->rd_next);
		}
		prev_rxdesc = rxdesc;
		prev_rxdesc->rs_statword = (0xABBAABBA & ~MUC_RXSTATUS_DONE);

		/*
		 * Get next descriptor.
		 */
		rxdesc = rxdesc->rd_next;
		if (rxdesc) {
			prev_rxdesc->rd_next = NULL;
			RXSTAT(qw, rx_poll_next);
		}

		/* Tell MuC that descriptor is returned */
		df_numelems = qw->rx_if.rx.fifo->df_numelems + 1;
		writel_wmb(df_numelems, &qw->rx_if.rx.fifo->df_numelems);
		RXSTAT_SET(qw, rx_df_numelems, df_numelems);
	}

	qw->rx_if.rx.pending = rxdesc;

	qdrv_rx_skb_list_indicate(&skb_rcv_list, 1);

	if (processed < budget) {
		DBGPRINTF(DBG_LL_TRIAL, QDRV_LF_PKT_RX, "Complete\n");
		RXSTAT(qw, rx_poll_complete);
		RXSTAT_SET(qw, rx_poll_stopped, 1);

		/* MBSS - Napi is scheduled for vnet[0] so napi_complete should map to correct VAP */
		/* qv1 = qv at start of this function stores correct VAP for this purpose */
		napi_complete(&qv1->napi);

		qdrv_mac_enable_irq(qw->mac, qw->rxirq);
	} else {
		RXSTAT(qw, rx_poll_continue);
	}

	DBGPRINTF_LIMIT(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return min(processed, budget);
}

int qdrv_rx_start(struct qdrv_mac *mac)
{
	struct int_handler int_handler;
	struct qdrv_wlan *qw = (struct qdrv_wlan *) mac->data;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	int_handler.handler = qdrv_rx_irq;
	int_handler.arg1 = (void *) qw;
	int_handler.arg2 = (void *) mac;

	if (qdrv_mac_set_handler(mac, qw->rxirq, &int_handler) != 0) {
		DBGPRINTF_E("Failed to register IRQ handler for %d\n", qw->rxirq);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	qdrv_mac_enable_irq(mac, qw->rxirq);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

int qdrv_rx_stop(struct qdrv_mac *mac)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) mac->data;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	qdrv_mac_disable_irq(mac, qw->rxirq);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

/*
 * we can't get how many layers MUC has applied on this packet
 * through tqe interface.
 * but current vlan code indicate only one layer, so just remove it.
 * */
static void qdrv_rx_remove_eapol_vlan(struct ieee80211vap *vap, struct sk_buff *skb, uint16_t ether_type)
{
	struct ether_header eth, *peth;

	peth = (struct ether_header *)skb->data;
	if (ether_type == __constant_htons(ETH_P_PAE) &&
			peth->ether_type == __constant_htons(ETH_P_8021Q)) {
		memcpy(&eth, peth, sizeof(eth));
		eth.ether_type = __constant_htons(ETH_P_PAE);
		skb_pull(skb, sizeof(struct vlan_ethhdr));
		memcpy(skb_push(skb, sizeof(eth)), &eth, sizeof(eth));
	}
}

/*
 * Frame received from external L2 filter will not have MAC header
 * So whole_frm_hdr will be NULL for those frames.
 */
static void qdrv_rx_tqe_rx_handler(void *token,
		const union topaz_tqe_cpuif_descr *descr,
		struct sk_buff *skb, uint8_t *whole_frm_hdr)
{
	struct qdrv_wlan *qw = token;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	struct qdrv_vap *qv;
	const uint16_t misc_user = descr->data.misc_user;
	const uint16_t node_idx_unmapped = MS(misc_user, TQE_MISCUSER_M2L_DATA_NODE_IDX);
	const bool check_3addr_br = MS(misc_user, TQE_MISCUSER_M2L_DATA_3ADDR_BR);
	const struct ieee80211_qosframe_addr4 *wh = (void *) whole_frm_hdr;
	uint16_t ether_type;

	ni = qw->ic.ic_node_idx_ni[node_idx_unmapped];
	if (likely(ni)) {
		ieee80211_ref_node(ni);
		vap = ni->ni_vap;
		qv = container_of(vap, struct qdrv_vap, iv);

		qdrv_sch_find_data_start(skb, (struct ether_header *)skb->data, &ether_type);
		skb->src_port = IEEE80211_NODE_IDX_MAP(node_idx_unmapped);
		skb->dev = vap->iv_dev;
		QTN_SKB_CB_ETHERTYPE(skb) = ether_type;
		qdrv_rx_remove_eapol_vlan(vap, skb, ether_type);

		if (handle_rx_msdu(qw, qv, ni, wh, skb, check_3addr_br) == 0) {
			skb = switch_vlan_to_proto_stack(skb, 0);
			if (skb)
				netif_receive_skb(skb);
		}
		ieee80211_free_node(ni);
	} else {
		RXSTAT(qw, rx_data_no_node);
		kfree_skb(skb);
	}
}

int qdrv_rx_init(struct qdrv_wlan *qw, struct host_ioctl_hifinfo *hifinfo)
{
	struct host_rxfifo *fifos;

	fifos = ioremap_nocache(muc_to_lhost(IO_ADDRESS(hifinfo->hi_rxfifo)),
			sizeof(*fifos));

	qw->rxirq = hifinfo->hi_rxdoneirq & IOCTL_DEVATTACH_IRQNUM;

	qw->rx_if.rx_sem_bit = fifos->rf_sem;

	qw->ic.ic_bridge_set_dest_addr = qdrv_bridge_set_dest_addr;

	qw->rx_if.rx.fifo = ioremap_nocache(
			muc_to_lhost(IO_ADDRESS((u32)fifos->rf_fifo)),
			sizeof(*qw->rx_if.rx.fifo));

	iounmap(fifos);

	if (rx_fifo_init(qw, &qw->rx_if.rx) < 0) {
		DBGPRINTF_E("Failed to setup RX FIFO buffers\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -ENOMEM;
	}

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_PKT_RX,
			"hi_rxfifo %p %p (virt)\n",
			(void *)hifinfo->hi_rxfifo,
			(void *)IO_ADDRESS((u32)hifinfo->hi_rxfifo));

	tqe_port_add_handler(TOPAZ_TQE_MUC_PORT, &qdrv_rx_tqe_rx_handler, qw);
	tqe_port_register(TOPAZ_TQE_WMAC_PORT);

	qdrv_mac_reserve_init(qw);

	return 0;
}

int qdrv_rx_exit(struct qdrv_wlan *qw)
{
	qdrv_mac_reserve_clear();

	tqe_port_unregister(TOPAZ_TQE_WMAC_PORT);
	tqe_port_remove_handler(TOPAZ_TQE_MUC_PORT);

	destroy_rx_ring(&qw->rx_if);

	return 0;
}

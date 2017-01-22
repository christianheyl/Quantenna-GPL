/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ieee80211_node.c 2366 2007-05-23 08:43:05Z mrenzmann $
 */
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 node handling support.
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#include <qtn/qdrv_sch.h>

#include "net80211/if_media.h"

#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_node.h"
#include "net80211/ieee80211_dot11_msg.h"
#include "net80211/if_ethersubr.h"
#include "net80211/ieee80211_tpc.h"
#include "net80211/ieee80211_tdls.h"

#include <qtn/qtn_net_packet.h>
#include <qtn/skb_recycle.h>
#include "qdrv_sch_const.h"
#include <qtn/qtn_pcap.h>
#include <qtn/shared_params.h>
#include <qtn/qtn_vlan.h>
#include "qtn_logging.h"

#include <asm/board/pm.h>
#include <asm/board/kdump.h>

#define IEEE80211_OBSS_AP_SCAN_INT	25
#define IEEE80211_BSS_DELETE_DELAY 5
#define FREQ_2_4_GHZ                    0
#define FREQ_5_GHZ                      1

#define DBGMAC "%02X:%02X:%02X:%02X:%02X:%02X"
#define ETHERFMT(a) \
	        (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

static void ieee80211_aid_remove(struct ieee80211_node *);
static void ieee80211_idx_remove(struct ieee80211_node *);
static void ieee80211_node_cleanup(struct ieee80211_node *);
static void ieee80211_node_exit(struct ieee80211_node *);
static u_int8_t ieee80211_node_getrssi(const struct ieee80211_node *);

static void _ieee80211_free_node(struct ieee80211_node *);

static void ieee80211_node_timeout(unsigned long);

static void ieee80211_node_table_init(struct ieee80211com *,
	struct ieee80211_node_table *, const char *, int);
static void ieee80211_node_table_cleanup(struct ieee80211com *, struct ieee80211_node_table *);
static void ieee80211_node_table_reset(struct ieee80211com *, struct ieee80211_node_table *,
	struct ieee80211vap *);
static void ieee80211_node_wds_ageout(unsigned long);
static void ieee80211_timeout_station_work(struct work_struct *work);

#define IEEE80211_REAUTH_ENABLE 1
MALLOC_DEFINE(M_80211_NODE, "80211node", "802.11 node state");

#ifdef IEEE80211_DEBUG_REFCNT

static struct ieee80211_node_table *nt_refdebug = NULL;

static void ieee80211_node_show_refdebug_info(struct ieee80211_node *ni)
{
	struct node_refdebug_info *info = ni->ni_refdebug_info_p;
	struct node_refdebug_info_entry *entry;
	int i;

	if (info) {
		printk("\nnode %p, MAC %pM, ref count %d, entry count %d:\n",
				ni,
				ni->ni_macaddr,
				ieee80211_node_refcnt(ni),
				info->entry_count);
		for (i = 0; i< info->entry_count; i++) {
			entry = &info->entry[i];
			/* INC	count	line	filename */
			printk("%4s %10d %9d %s\n",
					(entry->line & 0xffff0000) ? "dec" : "inc",
					entry->count,
					(entry->line & 0x0000ffff),
					entry->fname);
		}

		if (info->entry_count == REFDEBUG_ENTRY_MAX) {
			printk("%4s %10d %9s %s\n",
					"inc", info->inc_count, "unknown", "unknown");
			printk("%4s %10d %9s %s\n",
					"dec", info->dec_count, "unknown", "unknown");
		}
	}
}

#endif

void ieee80211_node_dbgref_history_dump(void)
{
#ifdef IEEE80211_DEBUG_REFCNT
	struct ieee80211_node *ni, *next;

	if (!nt_refdebug)
	      return;

	IEEE80211_NODE_LOCK_IRQ(nt_refdebug);
	TAILQ_FOREACH_SAFE(ni, &nt_refdebug->nt_node, ni_list, next) {
		ieee80211_node_show_refdebug_info(ni);
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt_refdebug);
#else
	printk("%s: not enabled\n", __func__);
#endif
}

#ifdef IEEE80211_DEBUG_REFCNT
static __sram_text void
ieee80211_node_dbgref_history(const struct ieee80211_node *ni,
			const char *filename, int line, int is_increased)
{
	unsigned long flags;
	struct node_refdebug_info *info;
	struct node_refdebug_info_entry *entry;
	int i;

	info = ni->ni_refdebug_info_p;
	if (!info)
	      return;

	line = is_increased ? line : (0xffff0000 | line);

	local_irq_save(flags);
	for (i = 0; i < REFDEBUG_ENTRY_MAX; i++) {
		entry = &info->entry[i];
		if (entry->fname == NULL) {
			entry->fname = filename;
			entry->line = line;
			info->entry_count++;
			break;
		}
		if (entry->line == line &&
				strcmp(filename, entry->fname) == 0)
			break;
	}

	if (unlikely(i == REFDEBUG_ENTRY_MAX)) {
		printk_once("%s: Table is full\n", __FUNCTION__);
		if (is_increased)
			info->inc_count++;
		else
			info->dec_count++;
	} else {
		entry->count++;
	}
	local_irq_restore(flags);
}

void __sram_text
ieee80211_node_dbgref(const struct ieee80211_node *ni, const char *filename,
			const int line, int is_increased)
{
	ieee80211_node_dbgref_history(ni, filename, line, is_increased);
}
EXPORT_SYMBOL(ieee80211_node_dbgref);
#endif

/*
 * Caller must lock the IEEE80211_NODE_LOCK
 * Context: hwIRQ, softIRQ and process context
 */
static void __sram_text
_ieee80211_remove_node(struct ieee80211_node *ni)
{
	struct ieee80211_node_table *nt = ni->ni_table;

	if (nt != NULL) {
		TAILQ_REMOVE(&nt->nt_node, ni, ni_list);
		LIST_REMOVE(ni, ni_hash);
		ni->ni_table = NULL;
		ieee80211_aid_remove(ni);
		ieee80211_idx_remove(ni);
	}
}

/*
 * Reclaim a node.  If this is the last reference count then
 * do the normal free work.  Otherwise remove it from the node
 * table and mark it gone by clearing the back-reference.
 */
static void
#ifdef IEEE80211_DEBUG_REFCNT
#define ieee80211_node_reclaim(_nt, _ni) \
        ieee80211_node_reclaim_debug(_nt, _ni, __FILE__, __LINE__)
ieee80211_node_reclaim_debug(struct ieee80211_node_table *nt, struct ieee80211_node *ni,
			const char *filename, int line)
#else
ieee80211_node_reclaim(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
#endif
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_LOCK_IRQ(ic);
	if (!ieee80211_node_dectestref(ni)) {
		ieee80211_node_dbgref(ni, filename, line, IEEE80211_NODEREF_DECR);
		/*
		 * Other references are present, just remove the
		 * node from the table so it cannot be found.  When
		 * the references are dropped storage will be
		 * reclaimed.
		 */
		_ieee80211_remove_node(ni);
	} else {
		ieee80211_node_dbgref(ni, filename, line, IEEE80211_NODEREF_DECR);
		_ieee80211_free_node(ni);
	}
	IEEE80211_UNLOCK_IRQ(ic);
}

void
indicate_association(void)
{
}
EXPORT_SYMBOL(indicate_association);

void
indicate_disassociation(void)
{
}
EXPORT_SYMBOL(indicate_disassociation);

void
ieee80211_node_attach(struct ieee80211com *ic)
{
	ieee80211_node_table_init(ic, &ic->ic_sta, "station",
		IEEE80211_INACT_INIT);
	init_timer(&ic->ic_inact);
	ic->ic_inact.function = ieee80211_node_timeout;
	ic->ic_inact.data = (unsigned long) ic;
	ic->ic_inact.expires = jiffies + IEEE80211_INACT_WAIT * HZ;
#if IEEE80211_REAUTH_ENABLE
	add_timer(&ic->ic_inact);
#endif

	/* copy default check interval from inactivity timer */
	ic->ic_scan_results_check = IEEE80211_INACT_WAIT;
	init_timer(&ic->ic_scan_results_expire);
	ic->ic_scan_results_expire.function = ieee80211_scan_timeout;
	ic->ic_scan_results_expire.data = (unsigned long) ic;
	ic->ic_scan_results_expire.expires = jiffies + ic->ic_scan_results_check * HZ;
	add_timer(&ic->ic_scan_results_expire);

	ic->ic_node_free = ieee80211_node_exit;
	ic->ic_node_cleanup = ieee80211_node_cleanup;
	ic->ic_node_getrssi = ieee80211_node_getrssi;
	ic->ic_iterate_nodes = ieee80211_iterate_nodes;
	ic->ic_iterate_dev_nodes = ieee80211_iterate_dev_nodes;

#ifdef IEEE80211_DEBUG_REFCNT
	kdump_add_troubleshooter(&ieee80211_node_dbgref_history_dump);
	nt_refdebug = &ic->ic_sta;
#endif
}

void
ieee80211_node_detach(struct ieee80211com *ic)
{
#if IEEE80211_REAUTH_ENABLE
	del_timer(&ic->ic_inact);
#endif
	del_timer(&ic->ic_scan_results_expire);
	ieee80211_node_table_cleanup(ic, &ic->ic_sta);
}

void
ieee80211_node_vattach(struct ieee80211vap *vap)
{
	/* default station inactivity timer setings */
	vap->iv_inact_init = IEEE80211_INACT_INIT;
	vap->iv_inact_auth = IEEE80211_INACT_AUTH;
	vap->iv_inact_run = IEEE80211_INACT_RUN;
	vap->iv_inact_probe = IEEE80211_INACT_PROBE;
}

void
ieee80211_node_latevattach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_rsnparms *rsn;

	/*
	 * Allocate these only if needed. On STA, only needed for TDLS
	 * connections. Beware that adhoc mode doesn't support ATIM yet
	 */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
			vap->iv_opmode == IEEE80211_M_STA ||
			vap->iv_opmode == IEEE80211_M_WDS) {
		if (vap->iv_max_aid == 0)
			vap->iv_max_aid = IEEE80211_AID_DEF;
		else if (vap->iv_max_aid > IEEE80211_AID_MAX)
			vap->iv_max_aid = IEEE80211_AID_MAX;
	}

	ieee80211_reset_bss(vap);

	if (vap->iv_opmode == IEEE80211_M_WDS) {
		vap->iv_bss = NULL;
		vap->iv_auth = ieee80211_authenticator_get(IEEE80211_AUTH_AUTO);
		return;
	}

	/*
	 * Set up "global settings" in the bss node so that
	 * each new station automatically inherits them.
	 */
	rsn = &vap->iv_bss->ni_rsn;
	rsn->rsn_ucastcipherset |= 1 << IEEE80211_CIPHER_AES_CCM;
	rsn->rsn_ucastcipherset |= 1 << IEEE80211_CIPHER_WEP;
	if (WPA_TKIP_SUPPORT)
		rsn->rsn_ucastcipherset |= 1 << IEEE80211_CIPHER_TKIP;
	if (ic->ic_caps & IEEE80211_C_AES)
		rsn->rsn_ucastcipherset |= 1 << IEEE80211_CIPHER_AES_OCB;
	if (ic->ic_caps & IEEE80211_C_CKIP)
		rsn->rsn_ucastcipherset |= 1 << IEEE80211_CIPHER_CKIP;
	/*
	 * Default unicast cipher to WEP for 802.1x use.  If
	 * WPA is enabled the management code will set these
	 * values to reflect.
	 */
	rsn->rsn_ucastcipher = IEEE80211_CIPHER_WEP;
	rsn->rsn_ucastkeylen = 104 / NBBY;
	/* Initialise with the lowest allowed mcast cipher */
	if (!WPA_TKIP_SUPPORT)
		rsn->rsn_mcastcipher = IEEE80211_CIPHER_AES_CCM;
	else
		rsn->rsn_mcastcipher = IEEE80211_CIPHER_TKIP;
	rsn->rsn_mcastkeylen = 128 / NBBY;
	/*
	 * We support both WPA-PSK and 802.1x; the one used
	 * is determined by the authentication mode and the
	 * setting of the PSK state.
	 */
	rsn->rsn_keymgmtset = WPA_ASE_8021X_UNSPEC | WPA_ASE_8021X_PSK;
	rsn->rsn_keymgmt = WPA_ASE_8021X_PSK;

	vap->iv_auth = ieee80211_authenticator_get(vap->iv_bss->ni_authmode);
}

void
ieee80211_node_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (vap->iv_bss != NULL)
		ieee80211_ref_node(vap->iv_bss);

	ieee80211_node_table_reset(ic, &ic->ic_sta, vap);
	if (vap->iv_bss != NULL) {
		ieee80211_free_node(vap->iv_bss);
		vap->iv_bss = NULL;
	}
}

/*
 * Empty data frame used to do rate training. Goes through the normal
 * data path, to trigger the rate adaptation algorithm to find the optimal rate
 * before we send traffic.
 *
 * OUI extended Ethertype frame for Quantenna OUI is used for rate training
 * packets. The bridge at the other end of the link will discard these frames
 * as they are destined for the address of the WiFi interface, and there is
 * no handler installed for this frame type.
 */
static void ieee80211_send_dummy_data(struct ieee80211_node *ni,
		struct ieee80211vap *vap, int skb_flags)
{
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();
	struct sk_buff *skb = qtn_skb_recycle_list_pop(recycle_list, &recycle_list->stats_qdrv);
	struct qtn_dummy_frame *df;
	int total_len = QTN_RATE_TRAIN_DATA_LEN + sizeof(*df) - sizeof(df->eh);
	uint32_t *payload;
	uint32_t *data;

	if (!skb) {
		/* If the recycle list is empty, use a smaller buffer to reduce memory pressure */
		skb = dev_alloc_skb(QTN_RATE_TRAIN_DATA_LEN * 3);
		if (!skb) {
			return;
		}
	}

	df = (struct qtn_dummy_frame *)skb_put(skb, sizeof(df->eh));
	payload = (uint32_t *)skb_put(skb, total_len);

	skb_reset_network_header(skb);

	memcpy(df->eh.ether_shost, vap->iv_myaddr, sizeof(df->eh.ether_shost));
	memcpy(df->eh.ether_dhost, ni->ni_macaddr, sizeof(df->eh.ether_dhost));
	memset(payload, QTN_RATE_TRAIN_BYTE, total_len);
	df->llc.llc_dsap = LLC_SNAP_LSAP;
	df->llc.llc_ssap = LLC_SNAP_LSAP;
	df->llc.llc_un.type_snap.control = LLC_UI;
	df->llc.llc_un.type_snap.org_code[0] = 0;
	df->llc.llc_un.type_snap.org_code[1] = 0;
	df->llc.llc_un.type_snap.org_code[2] = 0;
	df->llc.llc_un.type_snap.ether_type = htons(ETHERTYPE_802A);
	ieee80211_oui_add_qtn(df->ouie.oui);
	put_unaligned(htons(QTN_OUIE_TYPE_TRAINING), &df->ouie.type);
	df->eh.ether_type = htons(total_len);

	if (ni->ni_rate_train_hash) {
		data = (uint32_t *)(df + 1);
		put_unaligned(htonl(ni->ni_rate_train_hash), data + 1);
	}

	skb->dev = vap->iv_dev;

	qdrv_sch_classify_bk(skb);

	ieee80211_ref_node(ni);
	QTN_SKB_CB_NI(skb) = ni;

	M_FLAG_SET(skb, M_RATE_TRAINING);
	M_FLAG_SET(skb, M_NO_AMSDU);
	M_FLAG_SET(skb, M_VSP_CHK);
	if (skb_flags != 0)
		M_FLAG_SET(skb, (skb_flags));

	dev_queue_xmit(skb);
}

/*
 * Rate training - send a bunch of NULL data packets to get the rate
 * retry algorithm to converge quickly.
 */
static void ieee80211_sta_add_training(unsigned long arg)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)arg;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	int i;

	spin_lock(&ni->ni_lock);
	if (ni->ni_training_count < vap->iv_rate_training_count) {
		ni->ni_training_count++;
		spin_unlock(&ni->ni_lock);
		mod_timer(&ni->ni_training_timer, jiffies + 1);
		for (i = 0; i < vap->iv_rate_training_burst_count; ++i) {
			ieee80211_send_dummy_data(ni, vap, 0);
		}
	} else {
		ni->ni_training_flag = NI_TRAINING_END;
		spin_unlock(&ni->ni_lock);
		printk("%s: [%pM] %s ends\n",
			vap->iv_dev->name, ni->ni_macaddr,
			ic->ic_ocac.ocac_running ? "T process" : "training");
		ieee80211_free_node(ni);
	}
}

/*
 * Rate detecting - send a bunch of NULL data packets to get
 * the current Tx rate.
 */
static void ieee80211_tdls_rate_detection(unsigned long arg)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)arg;
	struct ieee80211vap *vap = ni->ni_vap;
	int i;

	if (ni->ni_training_count < vap->tdls_training_pkt_cnt) {
		for (i = 0; i < DEFAULT_TDLS_RATE_DETECTION_BURST_CNT; ++i) {
			ieee80211_send_dummy_data(ni, vap, 0);
		}
		ni->ni_training_count++;
		mod_timer(&ni->ni_training_timer, jiffies + 2);
	} else {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"[%pM] rate detecting ends\n", ni->ni_macaddr);
		spin_lock(&ni->ni_lock);
		ni->ni_training_flag = NI_TRAINING_END;
		spin_unlock(&ni->ni_lock);
		ieee80211_free_node(ni);
	}
}

static void ieee80211_add_assoc_record(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_assoc_history	*ah = &ic->ic_assoc_history;
	struct timeval time;
	time_t oldest_time = ah->ah_timestamp[0];
	int i, index = 0;

	if (ni->ni_associd == 0) {
		return;
	}

	do_gettimeofday(&time);

	for (i = 0; i < IEEE80211_MAX_ASSOC_HISTORY; i++) {
		if (IEEE80211_ADDR_EQ(ah->ah_macaddr_table[i], ni->ni_macaddr)) {
			ah->ah_timestamp[i] = time.tv_sec;

			return;
		}
	}

	for (i = 0; i < IEEE80211_MAX_ASSOC_HISTORY; i++) {
		if (ah->ah_timestamp[i] <= 0) {
			IEEE80211_ADDR_COPY(ah->ah_macaddr_table[i], ni->ni_macaddr);
			ah->ah_timestamp[i] = time.tv_sec;

			return;
		}
	}

	for (i = 1; i < IEEE80211_MAX_ASSOC_HISTORY; i++) {
		if (ah->ah_timestamp[i] < oldest_time) {
			oldest_time = ah->ah_timestamp[i];
			index = i;
		}
	}

	IEEE80211_ADDR_COPY(ah->ah_macaddr_table[index], ni->ni_macaddr);
	ah->ah_timestamp[index] = time.tv_sec;
}

void ieee80211_active_training_timer(struct ieee80211_node *ni,
		void (*call_back)(unsigned long), unsigned int interval)
{
	if ((!ni) || (!call_back)) {
		printk("%s: Get invalid arguments\n", __func__);
		return;
	}

	/* Reference for the timer below */
	spin_lock_bh(&ni->ni_lock);
	/* If training already started, don't start new training. */
	if (ni->ni_training_flag == NI_TRAINING_RUNNING) {
		printk("%s: Rate training is running\n", __func__);
		spin_unlock_bh(&ni->ni_lock);
		return;
	} else {
		ieee80211_ref_node(ni);
	}
	ni->ni_training_count = 0;
	ni->ni_training_flag = NI_TRAINING_RUNNING;
	ni->ni_training_start = jiffies + interval;
	spin_unlock_bh(&ni->ni_lock);

	ni->ni_training_timer.function = call_back;
	ni->ni_training_timer.data = (unsigned long)ni;

	/*
	 * We stagger the start of the training for the node to prevent situations
	 * where all nodes associate immediately, causing out of resource due to
	 * allocating too many training packets.
	 */
	mod_timer(&ni->ni_training_timer, jiffies + interval);
}

static int ieee80211_node_training_required(const struct ieee80211_node *ni)
{
	const int skip_training = TOPAZ_FPGA_PLATFORM || QTN_GENPCAP;
	struct ieee80211vap *vap = ni->ni_vap;
	uint32_t ret = 0;
	/* We only do rate training for Q->Q links */
	if (!skip_training && (ni->ni_qtn_assoc_ie != NULL)
		&& (vap->iv_opmode != IEEE80211_M_WDS)) {
		if (((vap->iv_opmode == IEEE80211_M_HOSTAP) && (ni != vap->iv_bss))
			|| ((vap->iv_opmode == IEEE80211_M_STA) && (ni == vap->iv_bss))) {
			ret = IEEE80211_NODE_TRAINING_NORMAL_MODE;
		} else if (((vap->iv_opmode == IEEE80211_M_STA)
				&& (vap->tdls_discovery_interval == 0)
				&& IEEE80211_NODE_IS_TDLS_ACTIVE(ni))) {
			ret = IEEE80211_NODE_TRAINING_TDLS_MODE;
		}
	}
	return ret;
}

void ieee80211_node_training_start(struct ieee80211_node *ni, int immediate)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint32_t training_delay_secs;

	/*
	 * We stagger the start of the training for the node to prevent situations
	 * where all nodes associate immediately, causing out of resource due to
	 * allocating too many training packets.
	 */
	training_delay_secs = 1 + (immediate ? 0 : (IEEE80211_NODE_AID(ni) % 10));

	/* Reference for the timer below */
	spin_lock_bh(&ni->ni_lock);

	/*
	 * If training already started, don't reference the node again or
	 * we could leak the node if it's discarded.
	 *
	 * This happens on the STA side as the BSS node is retained longer
	 * than on the AP side.
	 */
	if (ni->ni_training_flag != NI_TRAINING_RUNNING) {
		ieee80211_ref_node(ni);
	}
	ni->ni_training_count = 0;
	ni->ni_training_flag = NI_TRAINING_RUNNING;
	spin_unlock_bh(&ni->ni_lock);

	ni->ni_training_timer.function = ieee80211_sta_add_training;
	ni->ni_training_timer.data = (unsigned long)ni;

	mod_timer(&ni->ni_training_timer, jiffies + (training_delay_secs * HZ));
	printk("%s: [%pM] %s starts in %d seconds, idx %u\n",
			ni->ni_vap->iv_dev->name, ni->ni_macaddr,
			ic->ic_ocac.ocac_running ? "T process" : "training",
			training_delay_secs,
			IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx));
}

void
_ieee80211_node_authorize(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap= ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	uint32_t ret;

	ieee80211_add_assoc_record(ic, ni);

	ni->ni_flags |= IEEE80211_NODE_AUTH;
	ni->ni_inact_reload = vap->iv_inact_run;
	ic->ic_node_auth_state_change(ni, 1);

	ret = ieee80211_node_training_required(ni);
	if (ret == IEEE80211_NODE_TRAINING_NORMAL_MODE) {
		ieee80211_node_training_start(ni, 0);
	} else if (ret == IEEE80211_NODE_TRAINING_TDLS_MODE) {
		ieee80211_tdls_add_rate_detection(ni);
	}
}

void
ieee80211_tdls_add_rate_detection(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap= ni->ni_vap;
	struct ieee80211_node *ap_ni = vap->iv_bss;
	unsigned int interval;
	int smthd_rssi;

	if (ap_ni == NULL)
		return;

	smthd_rssi = ieee80211_tdls_get_smoothed_rssi(vap, ni);
	if (smthd_rssi < vap->tdls_min_valid_rssi)
		return;

	interval = HZ + (HZ * IEEE80211_NODE_AID(ap_ni) % 5) +
		(HZ / 2 * (IEEE80211_NODE_AID(ni) % 10));

	ieee80211_active_training_timer(ni, ieee80211_tdls_rate_detection, interval);
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
		"TDLS %s: Rate detection starts for %s AID %d at %lu\n", __func__,
		ether_sprintf(ni->ni_macaddr), IEEE80211_NODE_AID(ni), jiffies + interval);
}
EXPORT_SYMBOL(ieee80211_tdls_add_rate_detection);

/*
 * Port authorize/unauthorize interfaces for use by an authenticator.
 */
void
ieee80211_node_authorize(struct ieee80211_node *ni)
{
	int msg = IEEE80211_DOT11_MSG_CLIENT_CONNECTED;

	_ieee80211_node_authorize(ni);

	if (ni->ni_vap->iv_opmode == IEEE80211_M_STA) {
		msg = IEEE80211_DOT11_MSG_AP_CONNECTED;
	}

	if (ni->ni_authmode <= IEEE80211_AUTH_SHARED) {
		ieee80211_eventf(ni->ni_vap->iv_dev, QEVT_COMMON_PREFIX" %s ["DBGMAC"] [%s/%s] SSID %s",
					d11_m[msg],
					ETHERFMT(ni->ni_bssid),
					ni->ni_authmode <= IEEE80211_AUTH_OPEN ? "OPEN" : "SHARED",
					"NONE",
					ni->ni_essid);
	}
}
EXPORT_SYMBOL(ieee80211_node_authorize);

void
ieee80211_node_unauthorize(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;

	if (ieee80211_node_is_authorized(ni) &&
			!IEEE80211_ADDR_EQ(vap->iv_myaddr, ni->ni_macaddr))
		vap->iv_disconn_cnt++;

	ni->ni_flags &= ~IEEE80211_NODE_AUTH;
	ic->ic_node_auth_state_change(ni, 0);
}
EXPORT_SYMBOL(ieee80211_node_unauthorize);

/*
 * Set/change the channel.  The rate set is also updated
 * to ensure a consistent view by drivers.
 */
void
ieee80211_node_set_chan(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_channel *chan = ic->ic_bsschan;

	KASSERT(chan != IEEE80211_CHAN_ANYC, ("BSS channel not set up"));
	ni->ni_chan = chan;

	if (IEEE80211_IS_CHAN_HALF(ic->ic_curchan)) {
		ni->ni_rates = ic->ic_sup_half_rates;
	} else if (IEEE80211_IS_CHAN_QUARTER(ic->ic_curchan)) {
		ni->ni_rates = ic->ic_sup_quarter_rates;
	} else {
		ni->ni_rates = ic->ic_sup_rates[ic->ic_curmode];
	}
}

static __inline void
copy_bss(struct ieee80211_node *nbss, const struct ieee80211_node *obss)
{
	/* propagate useful state */
	nbss->ni_authmode = obss->ni_authmode;
	nbss->ni_ath_flags = obss->ni_ath_flags;
	nbss->ni_txpower = obss->ni_txpower;
	nbss->ni_vlan = obss->ni_vlan;
	nbss->ni_rsn = obss->ni_rsn;
	/* XXX statistics? */
}

/*
 * Create an IBSS or BSS.
 */
void
ieee80211_create_bss(struct ieee80211vap* vap, struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	struct ieee80211_channel *previous_chann = NULL;

	if (vap->iv_opmode == IEEE80211_M_WDS) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
			"%s: skipping for WDS mode\n",
			__func__);

		if (ic->ic_bsschan == IEEE80211_CHAN_ANYC) {
			ni = ieee80211_find_node(&ic->ic_sta, vap->iv_myaddr);
			if (!ni)
				return;

			/* Ensure ic_bsschan is set if WDS has been created first */
			ic->ic_bsschan = chan;
			ieee80211_node_set_chan(ic, ni);
			ieee80211_free_node(ni);
		}
		return;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		"%s: creating on channel %u\n", __func__,
		ieee80211_chan2ieee(ic, chan));

	/* Check to see if we already have a node for this mac */
	ni = ieee80211_find_node(&ic->ic_sta, vap->iv_myaddr);
	if (ni == NULL) {
		ni = ieee80211_alloc_node(&ic->ic_sta, vap, vap->iv_myaddr, "bss create");
	}

	KASSERT(ni != NULL, ("unable to find or create BSS node"));

	ni->ni_node_idx = vap->iv_vapnode_idx;

	ni->ni_vhtop.chanwidth = ic->ic_vhtop.chanwidth;

	IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_myaddr);
	ni->ni_esslen = vap->iv_des_ssid[0].len;
	memcpy(ni->ni_essid, vap->iv_des_ssid[0].ssid, ni->ni_esslen);
	if (vap->iv_bss != NULL)
		copy_bss(ni, vap->iv_bss);
	ni->ni_intval = ic->ic_lintval;

	if (vap->iv_flags & IEEE80211_F_PRIVACY)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if (ic->ic_phytype == IEEE80211_T_FH) {
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		vap->iv_flags |= IEEE80211_F_SIBSS;
		ni->ni_capinfo |= IEEE80211_CAPINFO_IBSS;	/* XXX */
		if (vap->iv_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_des_bssid);
		else
			ni->ni_bssid[0] |= 0x02;	/* local bit for IBSS */
	} else if (vap->iv_opmode == IEEE80211_M_AHDEMO) {
		if (vap->iv_flags & IEEE80211_F_DESBSSID)
		    IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_des_bssid);
		else {
		    ni->ni_bssid[0] = 0x00;
		    ni->ni_bssid[1] = 0x00;
		    ni->ni_bssid[2] = 0x00;
		    ni->ni_bssid[3] = 0x00;
		    ni->ni_bssid[4] = 0x00;
		    ni->ni_bssid[5] = 0x00;
		}
	}

	ni->ni_vendor = PEER_VENDOR_QTN;
	if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		ni->ni_node_type = IEEE80211_NODE_TYPE_VAP;
	else if (vap->iv_opmode == IEEE80211_M_STA)
		ni->ni_node_type = IEEE80211_NODE_TYPE_STA;

	/* clear DFS CAC state on previous channel */
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
			ic->ic_bsschan->ic_freq != chan->ic_freq &&
			IEEE80211_IS_CHAN_CACDONE(ic->ic_bsschan)) {

		/*
		 * IEEE80211_CHAN_DFS_CAC_DONE indicates whether or not to do CAC afresh.
		 * US   : IEEE80211_CHAN_DFS_CAC_DONE shall be cleared whenver we move to
		 *        a different channel
		 * ETSI : IEEE80211_CHAN_DFS_CAC_DONE shall be retained; Only event which
		 *        would mark the channel as unusable is the radar indication
		 */
		if (ic->ic_dfs_is_eu_region() == false) {
			previous_chann = ic->ic_bsschan;
			previous_chann->ic_flags &= ~IEEE80211_CHAN_DFS_CAC_DONE;
			if (ic->ic_mark_channel_dfs_cac_status) {
				ic->ic_mark_channel_dfs_cac_status(ic, previous_chann, IEEE80211_CHAN_DFS_CAC_DONE, false);
				ic->ic_mark_channel_dfs_cac_status(ic, previous_chann, IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, false);
			}
			/* Mark the channel as not_available and ready for cac */
			if (ic->ic_mark_channel_availability_status) {
				ic->ic_mark_channel_availability_status(ic, previous_chann,
						IEEE80211_CHANNEL_STATUS_NOT_AVAILABLE_CAC_REQUIRED);
			}
			printk(KERN_DEBUG "ieee80211_create_bss:"
					"Clearing CAC_DONE Status for chan %d\n",
					previous_chann->ic_ieee);
		}
	}

	/*
	 * Fix the channel and related attributes.
	 * If a channel change was initiated, ieee80211_channel_switch_post will handle it
	 */
	ic->ic_bsschan = chan;
	ieee80211_node_set_chan(ic, ni);

	ieee80211_sta_join1(vap, ni, 0);

	ieee80211_free_node(ni);
}
EXPORT_SYMBOL(ieee80211_create_bss);

/*
 * Reset bss state on transition to the INIT state.
 * Clear any stations from the table (they have been
 * deauth'd) and reset the bss node (clears key, rate,
 * etc. state).
 */
void
ieee80211_reset_bss(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *obss = vap->iv_bss;
	struct ieee80211_node *ni;

	if (vap->iv_opmode == IEEE80211_M_WDS) {
		vap->iv_bss = NULL;
		return;
	}

	if (obss != NULL) {
		ieee80211_ref_node(obss);
	}

	ieee80211_node_table_reset(ic, &ic->ic_sta, vap);
	ieee80211_reset_erp(ic, ic->ic_curmode);

	ni = ieee80211_alloc_node(&ic->ic_sta, vap, vap->iv_myaddr, "bss reset");
	KASSERT(ni != NULL, ("unable to set up BSS node"));

	ni->ni_vendor = PEER_VENDOR_QTN;
	if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		ni->ni_node_type = IEEE80211_NODE_TYPE_VAP;
	else if (vap->iv_opmode == IEEE80211_M_STA)
		ni->ni_node_type = IEEE80211_NODE_TYPE_STA;

	vap->iv_bss = ni;
	IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_myaddr);

	if (obss != NULL) {
		copy_bss(ni, obss);
		ni->ni_intval = ic->ic_lintval;
		ieee80211_free_node(obss);
	}

	ieee80211_free_node(ni);
}

static int
match_ssid(const struct ieee80211_node *ni,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	int i;

	if (!ni)
		return 0;

	for (i = 0; i < nssid; i++) {
		if (ni->ni_esslen == ssids[i].len &&
		    memcmp(ni->ni_essid, ssids[i].ssid, ni->ni_esslen) == 0)
			return 1;
	}
	return 0;
}

/*
 * Test a node for suitability/compatibility.
 */
static int
check_bss(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint8_t rate;
	unsigned int chan;

	chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (unlikely(chan == IEEE80211_CHAN_ANY))
		return 0;

	if (isclr(ic->ic_chan_active, chan))
		return 0;
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			return 0;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			return 0;
	}
	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			return 0;
	} else {
		/* Reference: IEEE802.11 7.3.1.4
		 * This means that the data confidentiality service is required
		 * for all frames exchanged with this STA  in IBSS and for all
		 * frames exchanged within the entire BSS otherwise
		 */

		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			return 0;
	}
	rate = ieee80211_fix_rate(ni, IEEE80211_F_DONEGO | IEEE80211_F_DOFRATE);
	if (rate & IEEE80211_RATE_BASIC)
		return 0;
	if (vap->iv_des_nssid != 0 &&
	    !match_ssid(ni, vap->iv_des_nssid, vap->iv_des_ssid))
		return 0;
	if ((vap->iv_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(vap->iv_des_bssid, ni->ni_bssid))
		return 0;
	return 1;
}

#ifdef IEEE80211_DEBUG
/*
 * Display node suitability/compatibility.
 */
static void
check_bss_debug(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint8_t rate;
	unsigned int chan;
	int fail;

	chan = ieee80211_chan2ieee(ic, ni->ni_chan);

	fail = 0;
	if (chan == IEEE80211_CHAN_ANY || isclr(ic->ic_chan_active, chan))
		fail |= 0x01;
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		/* This means that the data confidentiality service is required
		 * for all frames exchanged within this BSS. (IEEE802.11 7.3.1.4)
		 */
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
	rate = ieee80211_fix_rate(ni, IEEE80211_F_DONEGO | IEEE80211_F_DOFRATE);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if (vap->iv_des_nssid != 0 &&
	    !match_ssid(ni, vap->iv_des_nssid, vap->iv_des_ssid))
		fail |= 0x10;
	if ((vap->iv_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(vap->iv_des_bssid, ni->ni_bssid))
		fail |= 0x20;

	printk(" %c %s", fail ? '-' : '+', ether_sprintf(ni->ni_macaddr));
	printk(" %s%c", ether_sprintf(ni->ni_bssid), fail & 0x20 ? '!' : ' ');
	printk(" %3d%c",
		chan, fail & 0x01 ? '!' : ' ');
	printk(" %+4d", ni->ni_rssi);
	printk(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		fail & 0x08 ? '!' : ' ');
	printk(" %4s%c",
		(ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
			(ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
				"????",
			fail & 0x02 ? '!' : ' ');
	printk(" %3s%c ",
		(ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?  "wep" : "no",
		fail & 0x04 ? '!' : ' ');
	ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
	printk("%s\n", fail & 0x10 ? "!" : "");
}
#endif /* IEEE80211_DEBUG */

/*
 * Handle 802.11 ad hoc network merge.  The
 * convention, set by the Wireless Ethernet Compatibility Alliance
 * (WECA), is that an 802.11 station will change its BSSID to match
 * the "oldest" 802.11 ad hoc network, on the same channel, that
 * has the station's desired SSID.  The "oldest" 802.11 network
 * sends beacons with the greatest TSF timestamp.
 *
 * The caller is assumed to validate TSF's before attempting a merge.
 *
 * Return !0 if the BSSID changed, 0 otherwise.
 */
int
ieee80211_ibss_merge(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;

	if (ni == vap->iv_bss ||
	    IEEE80211_ADDR_EQ(ni->ni_bssid, vap->iv_bss->ni_bssid)) {
		/* unchanged, nothing to do */
		return 0;
	}
	if (!check_bss(vap, ni)) {
		/* capabilities mismatch */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
		    "%s: merge failed, capabilities mismatch\n", __func__);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_assoc(vap))
			check_bss_debug(vap, ni);
#endif
		vap->iv_stats.is_ibss_capmismatch++;
		return 0;
	}
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
		"%s: new bssid %s: %s preamble, %s slot time%s\n", __func__,
		ether_sprintf(ni->ni_bssid),
		ic->ic_flags & IEEE80211_F_SHPREAMBLE ? "short" : "long",
		ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long",
		ic->ic_flags & IEEE80211_F_USEPROT ? ", protection" : "");

	ieee80211_ref_node(ni);

	ieee80211_sta_join1(vap, ni, 0);

	return 1;
}
EXPORT_SYMBOL(ieee80211_ibss_merge);

static __inline int
ssid_equal(const struct ieee80211_node *a, const struct ieee80211_node *b)
{
	if (!a || !b)
		return 0;

	return (a->ni_esslen == b->ni_esslen &&
		memcmp(a->ni_essid, b->ni_essid, a->ni_esslen) == 0);
}

static void
ieee80211_sta_set_chanlist(struct ieee80211com *ic, int bw)
{
	COMPILE_TIME_ASSERT(sizeof(ic->ic_chan_active) == sizeof(ic->ic_chan_active_80));
	COMPILE_TIME_ASSERT(sizeof(ic->ic_chan_active) == sizeof(ic->ic_chan_active_40));
	COMPILE_TIME_ASSERT(sizeof(ic->ic_chan_active) == sizeof(ic->ic_chan_active_20));

	switch(bw) {
	case BW_HT80:
		memcpy(ic->ic_chan_active, ic->ic_chan_active_80, sizeof(ic->ic_chan_active));
		break;
	case BW_HT40:
		memcpy(ic->ic_chan_active, ic->ic_chan_active_40, sizeof(ic->ic_chan_active));
		break;
	default:
		memcpy(ic->ic_chan_active, ic->ic_chan_active_20, sizeof(ic->ic_chan_active));
		break;
	}
}

void ieee80211_nonqtn_sta_join(struct ieee80211vap *vap,
		struct ieee80211_node *ni, const char *caller)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (!ieee80211_node_is_qtn(ni)) {
		vap->iv_non_qtn_sta_assoc++;
		ic->ic_nonqtn_sta++;
		ieee80211_set_recv_ctrlpkts(vap);

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
			"%s - increases counter ic_nonqtn_sta %u, iv_non_qtn_sta_assoc %u\n",
			caller, ic->ic_nonqtn_sta, vap->iv_non_qtn_sta_assoc);
	}
}

void ieee80211_nonqtn_sta_leave(struct ieee80211vap *vap,
		struct ieee80211_node *ni, const char *caller)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (!ieee80211_node_is_qtn(ni)) {
		WARN_ON(ic->ic_nonqtn_sta == 0);
		WARN_ON(vap->iv_non_qtn_sta_assoc == 0);

		if (ic->ic_nonqtn_sta > 0)
			ic->ic_nonqtn_sta--;
		if (vap->iv_non_qtn_sta_assoc > 0)
			vap->iv_non_qtn_sta_assoc--;
		ieee80211_set_recv_ctrlpkts(vap);

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
			"%s - reduces counter ic_nonqtn_sta %u, iv_non_qtn_sta_assoc %u\n",
			caller, ic->ic_nonqtn_sta, vap->iv_non_qtn_sta_assoc);
	}
}

static void
ieee80211_reset_bw(struct ieee80211vap *vap, struct ieee80211com *ic, int channel, int delay_ch_switch)
{
	int bw;
	int max_channel_bw;
	int i;
	int idx_bw;
	struct ieee80211_channel *chan;
	int cur_bw = ieee80211_get_cap_bw(ic);

	if (vap->iv_opmode != IEEE80211_M_STA) {
		return;
	}

	max_channel_bw = ieee80211_get_max_channel_bw(ic, channel);
	bw = MIN(MIN(ic->ic_max_system_bw, max_channel_bw), ic->ic_bss_bw);

	if (bw >= BW_HT80 && !IS_IEEE80211_VHT_ENABLED(ic)) {
		bw = BW_HT40;
	}

	if ((bw > BW_HT40) && !ieee80211_swfeat_is_supported(SWFEAT_ID_VHT, 1)) {
		printk("BW %d is not supported on this device\n", bw);
		bw = BW_HT40;
	}

	if ((bw >= BW_HT40) && (ic->ic_curmode == IEEE80211_MODE_11A)) {
		bw = BW_HT20;
	}

	if ((ic->ic_curmode == IEEE80211_MODE_11B) ||
			(ic->ic_curmode == IEEE80211_MODE_11G) ||
				(ic->ic_curmode == IEEE80211_MODE_11NG)) {
		bw = BW_HT20;
	}

	if (ic->ic_curmode == IEEE80211_MODE_11NG_HT40PM) {
		bw = BW_HT40;
	}

	if (bw == cur_bw)
		return;

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, bw, NULL, 0);

	/* Reset the active channel list as per the bw selected */
	ieee80211_sta_set_chanlist(ic, bw);

	/*
         * If region = none do not reconfigure the tx_power for
         * the bw set
         */
	if (ic->ic_country_code == CTRY_DEFAULT)
	return;

	/* Update ic_maxpower since bw is changed */
	switch(bw) {
	case BW_HT80:
		idx_bw = PWR_IDX_80M;
		break;
	case BW_HT40:
		idx_bw = PWR_IDX_40M;
		break;
	case BW_HT20:
		idx_bw = PWR_IDX_20M;
		break;
	default:
		printk("unsupported bw: %u\n", bw);
		return;
	}
	for (i = 0; i < ic->ic_nchans; i++) {
		chan = &ic->ic_channels[i];
		if (!isset(ic->ic_chan_active, chan->ic_ieee)) {
			continue;
		}
		chan->ic_maxpower = chan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][idx_bw];
		chan->ic_maxpower_normal = chan->ic_maxpower;
	}

	if (!delay_ch_switch) {
		/* Reset channel to apply the new bandwidth and power configuration */
		ic->ic_set_channel(ic);
	}
}

/*
 * Restore bandwidth to initial state.
 */
void
ieee80211_restore_bw(struct ieee80211vap *vap, struct ieee80211com *ic)
{
	int bw = ic->ic_max_system_bw;
	int cur_bw = ieee80211_get_cap_bw(ic);

	if (vap->iv_opmode != IEEE80211_M_STA)
		return;

	if (bw >= BW_HT80 && !IS_IEEE80211_VHT_ENABLED(ic))
		bw = BW_HT40;

	if ((bw > BW_HT40) && !ieee80211_swfeat_is_supported(SWFEAT_ID_VHT, 1))
		bw = BW_HT40;

	if ((bw >= BW_HT40) && (ic->ic_curmode == IEEE80211_MODE_11A))
		bw = BW_HT20;

	if ((ic->ic_curmode == IEEE80211_MODE_11B) ||
			(ic->ic_curmode == IEEE80211_MODE_11G))
		bw = BW_HT20;

	if (bw == cur_bw)
		return;

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, bw, NULL, 0);

	/* Reset the active channel list as per the bw selected */
	ieee80211_sta_set_chanlist(ic, bw);
}

/*
 * Set phymode, bw and vht params specific to band
 */
void
configure_phy_mode(struct ieee80211vap *vap, struct ieee80211_node *selbs)
{
	struct ieee80211com *ic = selbs->ni_ic;
	int aggr = 1;

	if (selbs->ni_chan->ic_ieee > QTN_2G_LAST_OPERATING_CHAN) {
		if (vap->iv_5ghz_prof.phy_mode == IEEE80211_MODE_11NA) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11NA;
			vap->iv_5ghz_prof.bw = BW_HT20;
			vap->iv_5ghz_prof.vht = 0;
		} else if (vap->iv_5ghz_prof.phy_mode == IEEE80211_MODE_11NA_HT40PM) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11NA_HT40PM;
			vap->iv_5ghz_prof.bw = BW_HT40;
			vap->iv_5ghz_prof.vht = 0;
		} else if (vap->iv_5ghz_prof.phy_mode == IEEE80211_MODE_11AC_VHT20PM) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11AC_VHT20PM;
			vap->iv_5ghz_prof.bw = BW_HT20;
			vap->iv_5ghz_prof.vht = 1;
		} else if (vap->iv_5ghz_prof.phy_mode == IEEE80211_MODE_11AC_VHT40PM) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11AC_VHT40PM;
			vap->iv_5ghz_prof.bw = BW_HT40;
			vap->iv_5ghz_prof.vht = 1;
		} else {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11AC_VHT80PM;
			vap->iv_5ghz_prof.bw = BW_HT80;
			vap->iv_5ghz_prof.vht = 1;
		}

		ieee80211_setmode(ic, ic->ic_des_mode);
		ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, vap->iv_5ghz_prof.bw, NULL, 0);
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_MODE, vap->iv_5ghz_prof.vht, NULL, 0);
	} else {
		if (vap->iv_2_4ghz_prof.phy_mode == IEEE80211_MODE_11B) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11B;
			vap->iv_2_4ghz_prof.bw = BW_HT20;
		} else if (vap->iv_2_4ghz_prof.phy_mode == IEEE80211_MODE_11G) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11G;
			vap->iv_2_4ghz_prof.bw = BW_HT20;
		} else  if (vap->iv_2_4ghz_prof.phy_mode == IEEE80211_MODE_11NG) {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11NG;
			vap->iv_2_4ghz_prof.bw = BW_HT20;
		} else {
			ic->ic_des_mode = ic->ic_phymode = IEEE80211_MODE_11NG_HT40PM;
			vap->iv_2_4ghz_prof.bw = BW_HT40;
		}

		ieee80211_setmode(ic, ic->ic_des_mode);
		ieee80211_start_obss_scan_timer(vap);

		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, vap->iv_2_4ghz_prof.bw, NULL, 0);
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_PHY_MODE, ic->ic_phymode, NULL, 0);
	}

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_TX_AMSDU, aggr, NULL, 0);
	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_AGGREGATION, aggr, NULL, 0);
}

static void ieee80211_repeater_csa_finish(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	KASSERT((vap->iv_opmode == IEEE80211_M_STA
		&& (ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)), ("Should only be called for repeater STA interface"));

	ieee80211_finish_csa(arg);

	/* finish the undone job left by ieee80211_sta_join1 */
	IEEE80211_SCHEDULE_TQUEUE(&vap->iv_stajoin1tq);
}

/*
 * Join the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a reference already held for use in assigning
 * to iv_bss.
 */
void
ieee80211_sta_join1(struct ieee80211vap *vap, struct ieee80211_node *selbs, int reauth)
{
	struct ieee80211com *ic = selbs->ni_ic;
	struct ieee80211_node *obss = vap->iv_bss;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211com *ic_sta = vap->iv_ic;
	int canreassoc;
	int cur_band, new_band;
	u_int8_t bridge_mode_changed;
	int start_csa = (vap->iv_opmode == IEEE80211_M_STA
			&& (ic->ic_flags_ext & IEEE80211_FEXT_REPEATER));

	cur_band = new_band = 0;
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		/*
		 * Delete unusable rates; we've already checked
		 * that the negotiated rate set is acceptable.
		 */
		ieee80211_fix_rate(selbs, IEEE80211_F_DODEL);
		ieee80211_fix_ht_rate(selbs, IEEE80211_F_DODEL);
	}

	/* Set bridge mode according to AP's capabilities and local config */
	bridge_mode_changed = ieee80211_bridgemode_set(vap, 0);

	/*
	 * Check if old+new node have the same ssid in which
	 * case we can reassociate when operating in sta mode.
	 */
	canreassoc = (obss != NULL &&
			vap->iv_state >= IEEE80211_S_ASSOC &&
			ssid_equal(obss, selbs) &&
			!bridge_mode_changed);

	if ((obss != NULL) && (obss != selbs) &&
			(vap->iv_state >= IEEE80211_S_ASSOC)) {
#if defined(QBMPS_ENABLE)
		if ((ic->ic_flags_qtn & IEEE80211_QTN_BMPS) &&
		    (vap->iv_opmode == IEEE80211_M_STA)) {
			/* exit power-saving */
			ieee80211_pm_queue_work(ic);
		}
#endif
		IEEE80211_SEND_MGMT(obss,
				IEEE80211_FC0_SUBTYPE_DISASSOC,
				IEEE80211_REASON_ASSOC_LEAVE);
		/*
		 * Ensure that the deauth/disassoc frame is sent
		 * before the node is deleted.
		 */
		ieee80211_safe_wait_ms(50, !in_interrupt());
	}

	vap->iv_bss = selbs;
	if ((obss != NULL) && (obss != selbs)) {
		if (vap->iv_opmode == IEEE80211_M_STA) {
			ic->ic_new_assoc(selbs);
		}
		ic->ic_disassoc(obss);
		IEEE80211_NODE_LOCK_IRQ(nt);
		ieee80211_node_reclaim(nt, obss);
		IEEE80211_NODE_UNLOCK_IRQ(nt);
	}

	/* Update secondary channel offset base on htop */
	if (selbs->ni_chan->ic_ieee <= QTN_2G_LAST_OPERATING_CHAN)
		ieee80211_update_sec_chan_offset(selbs->ni_chan, selbs->ni_htinfo.choffset);

	/* check if required to update phymode and bandwidth */
	if (ic->ic_rf_chipid == CHIPID_DUAL) {
		if (selbs->ni_chan->ic_ieee > QTN_2G_LAST_OPERATING_CHAN)
			new_band = FREQ_5_GHZ;
		else
			new_band = FREQ_2_4_GHZ;
		if (ic_sta->ic_curchan->ic_ieee > QTN_2G_LAST_OPERATING_CHAN)
			cur_band = FREQ_5_GHZ;
		else
			cur_band = FREQ_2_4_GHZ;

		if (cur_band != new_band)
			configure_phy_mode(vap, selbs);
	}

	ic->ic_bsschan = selbs->ni_chan;
	ic->ic_curchan = selbs->ni_chan;

	ic->ic_bss_bw = ic->ic_max_system_bw;
	if (vap->iv_opmode == IEEE80211_M_STA) {
		if (IEEE80211_VHTOP_GET_CHANWIDTH(&selbs->ni_ie_vhtop)
				>= IEEE80211_VHTOP_CHAN_WIDTH_80MHZ) {
			ic->ic_bss_bw = MIN(BW_HT80, ic->ic_max_system_bw);
		} else if (selbs->ni_ie_htinfo.hi_byte1 & IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40) {
			ic->ic_bss_bw = MIN(BW_HT40, ic->ic_max_system_bw);
		} else {
			ic->ic_bss_bw = MIN(BW_HT20, ic->ic_max_system_bw);
		}
	}

	ic->ic_fast_reass_chan = selbs->ni_chan;
	ic->ic_fast_reass_scan_cnt = 0;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE | IEEE80211_MSG_SCAN,
		"Fast reassoc chan=%u fast=%u state=%u\n",
		(ic->ic_fast_reass_chan && (ic->ic_fast_reass_chan != IEEE80211_CHAN_ANYC)) ?
			ic->ic_fast_reass_chan->ic_ieee : 0,
			canreassoc,
			vap->iv_state);

	if (start_csa) {
		ieee80211_enter_csa(ic, ic->ic_curchan,
			ieee80211_repeater_csa_finish,
			IEEE80211_CSW_REASON_SCAN,
			IEEE80211_DEFAULT_CHANCHANGE_TBTT_COUNT,
			IEEE80211_CSA_MUST_STOP_TX,
			IEEE80211_CSA_F_BEACON | IEEE80211_CSA_F_ACTION);
	} else {
		ic->ic_set_channel(ic);
	}

	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) ||
			(vap->iv_opmode == IEEE80211_M_STA))
		ieee80211_build_countryie(ic);

	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */
	if (ic->ic_curmode != IEEE80211_MODE_11B) {
		ieee80211_reset_erp(ic, ic->ic_curmode);
	}

	if (reauth && vap->iv_opmode == IEEE80211_M_STA) {
		/*
		 * Reset the bw before association to minimum of
		 * max_system_bw and max_channel_bw.
		 */
		ieee80211_reset_bw(vap, ic, selbs->ni_chan->ic_ieee, start_csa);

		/*
		 * Act as if we received a DEAUTH frame in case we are
		 * invoked from the RUN state.  This will cause us to try
		 * to re-authenticate if we are operating as a station.
		 */
		if (canreassoc) {
			vap->iv_nsparams.newstate = IEEE80211_S_ASSOC;
			vap->iv_nsparams.arg = 0;
		} else {
			vap->iv_nsparams.newstate = IEEE80211_S_AUTH;
			vap->iv_nsparams.arg = IEEE80211_FC0_SUBTYPE_DEAUTH;
		}
	} else {
		vap->iv_nsparams.newstate = IEEE80211_S_RUN;
		vap->iv_nsparams.arg = -1;
	}

	if (!start_csa)
		IEEE80211_SCHEDULE_TQUEUE(&vap->iv_stajoin1tq);
}

void
ieee80211_sta_join1_tasklet(IEEE80211_TQUEUE_ARG data)
{
	struct ieee80211vap *vap= (struct ieee80211vap *) data;
	int rc;

	rc = ieee80211_new_state(vap, vap->iv_nsparams.newstate, vap->iv_nsparams.arg);
	vap->iv_nsparams.result = rc;
	vap->iv_nsdone = 1;

	/* Enable system xmit for DFS slave */
	vap->iv_ic->ic_sta_set_xmit(true);
}
EXPORT_SYMBOL(ieee80211_sta_join1_tasklet);

extern int
ieee80211_get_rsn_from_ie(struct ieee80211vap *vap, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn_parm);


int
ieee80211_sta_join(struct ieee80211vap *vap,
	const struct ieee80211_scan_entry *se)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	struct ieee80211_node *bss_ni = vap->iv_bss;

	if (!bss_ni) {
		bss_ni = TAILQ_FIRST(&ic->ic_vaps)->iv_bss;
		if (!bss_ni) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
				"%s: no bss node\n", __func__);
			return 0;
		}
	}

	ni = ieee80211_find_node(&ic->ic_sta, se->se_macaddr);
	if (ni == NULL) {
		ni = ieee80211_alloc_node(&ic->ic_sta, vap, se->se_macaddr, "join");
		if (ni == NULL) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
				"%s: failed to alloc node\n", __func__);
			return 0;
		}
	}

	/* Inherit some params from BSS */
	ni->ni_authmode = bss_ni->ni_authmode;
	ni->ni_rsn = bss_ni->ni_rsn;

	/* Expand scan state into node's format */
	IEEE80211_ADDR_COPY(ni->ni_bssid, se->se_bssid);
	ni->ni_esslen = se->se_ssid[1];
	memcpy(ni->ni_essid, se->se_ssid + 2, ni->ni_esslen);

	ni->ni_rstamp = se->se_rstamp;
	ni->ni_tstamp.tsf = se->se_tstamp.tsf;
	ni->ni_intval = IEEE80211_BINTVAL_SANITISE(se->se_intval);
	ni->ni_capinfo = se->se_capinfo;
	ni->ni_chan = se->se_chan;
	ni->ni_timoff = se->se_timoff;
	ni->ni_fhdwell = se->se_fhdwell;
	ni->ni_fhindex = se->se_fhindex;
	ni->ni_erp = se->se_erp;
	ni->ni_rssi = se->se_rssi;
	ni->ni_node_type = IEEE80211_NODE_TYPE_VAP;
	ni->tdls_status = IEEE80211_TDLS_NODE_STATUS_NONE;
	if (se->se_htcap_ie != NULL) {
		ieee80211_parse_htcap(ni, se->se_htcap_ie);
		ni->ni_flags |= IEEE80211_NODE_HT;
	} else {
		ni->ni_flags &= ~IEEE80211_NODE_HT;
	}
	if (se->se_htinfo_ie != NULL)
		ieee80211_parse_htinfo(ni, se->se_htinfo_ie);
	if (se->se_vhtcap_ie != NULL)
		ieee80211_parse_vhtcap(ni, se->se_vhtcap_ie);
	if (se->se_vhtop_ie != NULL)
		ieee80211_parse_vhtop(ni, se->se_vhtop_ie);
	if (se->se_wpa_ie != NULL)
		ieee80211_saveie(&ni->ni_wpa_ie, se->se_wpa_ie);
	if (se->se_rsn_ie != NULL)
		ieee80211_saveie(&ni->ni_rsn_ie, se->se_rsn_ie);
	if (se->se_wme_ie != NULL)
		ieee80211_saveie(&ni->ni_wme_ie, se->se_wme_ie);
	if (se->se_qtn_ie != NULL)
		ieee80211_saveie(&ni->ni_qtn_assoc_ie, se->se_qtn_ie);
	if (se->se_ath_ie != NULL)
		ieee80211_saveath(ni, se->se_ath_ie);
	ni->ni_ext_role = se->se_ext_role;
	if (se->se_ext_bssid_ie != NULL)
		ieee80211_saveie(&ni->ni_ext_bssid_ie, se->se_ext_bssid_ie);

	/*
	 * Parse and fill the rsn_cap here only. The remaining of rsn params is gotten from bss above
	 */
	if(ni->ni_rsn_ie) {
		struct ieee80211_rsnparms rsn_param;
		u_int8_t res = 0;
		memset((u_int8_t*)&rsn_param, 0, sizeof(rsn_param));
		res = ieee80211_get_rsn_from_ie(vap, ni->ni_rsn_ie, &rsn_param);
		if(res == 0) {
			ni->ni_rsn.rsn_caps = rsn_param.rsn_caps;
			if (!vap->iv_pmf) {
				ni->ni_rsn.rsn_caps &= ~(RSN_CAP_MFP_REQ | RSN_CAP_MFP_CAP);
			}
		}
	}

	vap->iv_qtn_ap_cap = se->se_qtn_ie_flags;
	vap->iv_is_qtn_dev = se->se_is_qtn_dev;
	vap->iv_dtim_period = se->se_dtimperiod;
	vap->iv_dtim_count = 0;
	vap->iv_local_max_txpow = se->local_max_txpwr;

	/* NB: must be after ni_chan is set up */
	ieee80211_setup_rates(ni, se->se_rates, se->se_xrates,
		IEEE80211_F_DOSORT | IEEE80211_F_DONEGO | IEEE80211_F_DODEL);

	if (ic->sta_dfs_info.sta_dfs_strict_mode) {
		if (ieee80211_is_chan_not_available(ni->ni_chan)) {
			if (ic->ic_mark_channel_availability_status) {
				ic->ic_mark_channel_availability_status(ic, ni->ni_chan,
						IEEE80211_CHANNEL_STATUS_AVAILABLE);
			}
		}
	}

	ieee80211_sta_join1(vap, ni, 1);

	ieee80211_free_node(ni);

	return 1;
}
EXPORT_SYMBOL(ieee80211_sta_join);

static __inline void
ieee80211_clear_aid_bitmap(struct ieee80211com *ic, uint16_t aid)
{
	if (IEEE80211_AID(aid) < QTN_NODE_TBL_SIZE_LHOST)
		IEEE80211_AID_CLR(ic, aid);
}

static void
ieee80211_aid_remove(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;

	if (ni->ni_associd) {
		ieee80211_clear_aid_bitmap(ic, ni->ni_associd);

		ic->ic_node_auth_state_change(ni, 0);

		ni->ni_associd = 0;
	}
}

static void
ieee80211_idx_remove(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;

	if (ni->ni_node_idx) {
		if (ic->ic_node_idx_ni[IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx)] == ni) {
			ic->ic_node_idx_ni[IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx)] = NULL;
		}
		ic->ic_unregister_node(ni);
		ni->ni_node_idx = 0;
	}
}

/*
 * Leave the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
void
ieee80211_sta_leave(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	ieee80211_ref_node(ni);

	ieee80211_ppqueue_remove_node_leave(&vap->iv_ppqueue, ni);
	if ((ic->ic_measure_info.ni == ni) &&
			(ic->ic_measure_info.status == MEAS_STATUS_RUNNING)) {
		 ic->ic_measure_info.status = MEAS_STATUS_DISCRAD;
	}

#if defined(QBMPS_ENABLE)
	if (ic->ic_flags_qtn & IEEE80211_QTN_BMPS) {
		/* exit power-saving */
		ieee80211_pm_queue_work(ic);
	}
#endif
	/* WDS/Repeater: Stop software beacon timer for STA */
	if (vap->iv_opmode == IEEE80211_M_STA &&
	    vap->iv_flags_ext & IEEE80211_FEXT_SWBMISS) {
		del_timer(&vap->iv_swbmiss);
	}
	/* Clear up the training timer */
	if (vap->iv_opmode != IEEE80211_M_WDS) {
		if (ni) {
			int free_node = 0;
			del_timer_sync(&ni->ni_training_timer);
			spin_lock(&ni->ni_lock);
			if (ni->ni_training_flag == NI_TRAINING_RUNNING) {
				free_node = 1;
				ni->ni_training_flag = NI_TRAINING_END;
			}
			spin_unlock(&ni->ni_lock);
			if (free_node) {
				printk("%s: [%pM] %s stopped\n",
						vap->iv_dev->name, ni->ni_macaddr,
						ic->ic_ocac.ocac_running ? "T process" : "training");
				ieee80211_free_node(ni);
			}
		}
	}

	if (vap->iv_opmode == IEEE80211_M_STA)
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_PWR_ADJUST_AUTO, 0, NULL, 0);

	/*
	 * Make sure the node index is removed after the state change, which is below. Otherwise, fdb will not be cleared.
	 */
	ic->ic_node_auth_state_change(ni, 0);

	/*
	 * Make sure the node index is removed before deleting the MuC/AuC node structure, else
	 * packets will be dropped.
	 */
	ieee80211_idx_remove(ni);

	ic->ic_node_cleanup(ni);
	ic->ic_disassoc(ni);

	ieee80211_notify_node_leave(ni);
	ieee80211_free_node(ni);
}

/*
 * Node table support.
 */
static void
ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt,	const char *name, int inact)
{
	nt->nt_ic = ic;
	IEEE80211_NODE_LOCK_INIT(nt, "");
	IEEE80211_SCAN_LOCK_INIT(nt, "");

	TAILQ_INIT(&nt->nt_node);
	nt->nt_name = name;
	nt->nt_scangen = 1;
	nt->nt_inact_init = inact;
	init_timer(&nt->nt_wds_aging_timer);
	nt->nt_wds_aging_timer.function = ieee80211_node_wds_ageout;
	nt->nt_wds_aging_timer.data = (unsigned long) nt;
	mod_timer(&nt->nt_wds_aging_timer, jiffies + HZ * WDS_AGING_TIMER_VAL);
}

/*
 * Reclaim any resources in a node and reset any critical
 * state.  Typically nodes are free'd immediately after,
 * but in some cases the storage may be reused so we need
 * to ensure consistent state (should probably fix that).
 *
 * Context: hwIRQ, softIRQ and process context
 */
static void
ieee80211_node_cleanup(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	IEEE80211_LOCK_IRQ(ni->ni_ic);
	/* NB: preserve ni_table */
	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT) {
		if (vap->iv_opmode != IEEE80211_M_STA)
			vap->iv_ps_sta--;

		if (WMM_UAPSD_NODE_IS_PWR_MGT(ni)) {
			ni->ni_ic->ic_uapsdmaxtriggers--;
		}

		ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
		IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
			"power save mode off, %u sta's in ps mode",
			vap->iv_ps_sta);
	}

	ni->ni_flags &= ~IEEE80211_NODE_HT;

	ieee80211_node_saveq_drain(ni);

	if (vap->iv_set_tim != NULL) {
		vap->iv_set_tim(ni, 0);
	}

	ieee80211_aid_remove(ni);

	memset(&ni->ni_stats, 0, sizeof(ni->ni_stats));

	if (ni->ni_challenge != NULL) {
		FREE(ni->ni_challenge, M_DEVBUF);
		ni->ni_challenge = NULL;
	}

	if (ni->ni_qtn_brmacs != NULL) {
		FREE(ni->ni_qtn_brmacs, M_DEVBUF);
		ni->ni_qtn_brmacs = NULL;
	}
	/*
	 * Preserve SSID, WPA, and WME ie's so the bss node is
	 * reusable during a re-auth/re-assoc state transition.
	 * If we remove these data they will not be recreated
	 * because they come from a probe-response or beacon frame
	 * which cannot be expected prior to the association-response.
	 * This should not be an issue when operating in other modes
	 * as stations leaving always go through a full state transition
	 * which will rebuild this state.
	 *
	 * XXX does this leave us open to inheriting old state?
	 */

	if (ni->ni_rxfrag != NULL) {
		dev_kfree_skb_any(ni->ni_rxfrag);
		ni->ni_rxfrag = NULL;
	}

	/*
	 * If there are related frames have been pushed to HW, clear the security bit in Node Cache.
         * Don't delete the key here, disassoc will release the node cache later.
         */
#if 0
	ieee80211_crypto_delkey(vap, &ni->ni_ucastkey, ni);
	ni->ni_rxkeyoff = 0;
#endif
	/* Deauthorize - remove from various linked lists. */
	ieee80211_node_unauthorize(ni);
	IEEE80211_UNLOCK_IRQ(ni->ni_ic);
}

static void
ieee80211_node_exit(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	ic->ic_node_count--;

	ic->ic_node_cleanup(ni);

	if (ni->ni_wpa_ie != NULL) {
		FREE(ni->ni_wpa_ie, M_DEVBUF);
		ni->ni_wpa_ie = NULL;
	}
	if (ni->ni_rsn_ie != NULL) {
		FREE(ni->ni_rsn_ie, M_DEVBUF);
		ni->ni_rsn_ie = NULL;
	}
	if (ni->ni_osen_ie != NULL) {
		FREE(ni->ni_osen_ie, M_DEVBUF);
		ni->ni_osen_ie = NULL;
	}
	if (ni->ni_wme_ie != NULL) {
		FREE(ni->ni_wme_ie, M_DEVBUF);
		ni->ni_wme_ie = NULL;
	}
	if (ni->ni_wsc_ie != NULL) {
		FREE(ni->ni_wsc_ie, M_DEVBUF);
		ni->ni_wsc_ie = NULL;
	}
	if (ni->ni_ath_ie != NULL) {
		FREE(ni->ni_ath_ie, M_DEVBUF);
		ni->ni_ath_ie = NULL;
	}
	if (ni->ni_qtn_assoc_ie != NULL) {
		FREE(ni->ni_qtn_assoc_ie, M_DEVBUF);
		ni->ni_qtn_assoc_ie = NULL;
	}
	if (ni->ni_qtn_pairing_ie != NULL) {
		FREE(ni->ni_qtn_pairing_ie, M_DEVBUF);
		ni->ni_qtn_pairing_ie = NULL;
	}
	if (ni->ni_qtn_brmacs != NULL) {
		FREE(ni->ni_qtn_brmacs, M_DEVBUF);
		ni->ni_qtn_brmacs = NULL;
	}
	if (ni->ni_ext_bssid_ie != NULL) {
		FREE(ni->ni_ext_bssid_ie, M_DEVBUF);
		ni->ni_ext_bssid_ie = NULL;
	}

	ieee80211_scs_free_node_tdls_stats(ic, ni);

	ic->ic_qdrv_node_free(ni);

	IEEE80211_NODE_SAVEQ_DESTROY(ni);
	FREE(ni, M_80211_NODE);
}

static u_int8_t
ieee80211_node_getrssi(const struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

/*
 * Configure block ack agreements for a node.
 */
static void ieee80211_tx_addba(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	int tid;

	if (ni->ni_associd == 0) {
		ieee80211_free_node(ni);
		return;
	}

	for (tid = 0; tid < 7; tid++) {
		if (ni->ni_ba_tx[tid].state == IEEE80211_BA_REQUESTED) {
			ic->ic_setparam(ni, IEEE80211_PARAM_HTBA_SEQ_CTRL,
					tid << 16, 0, 0);
			if (ni->ni_vap->iv_opmode == IEEE80211_M_WDS &&
					tid == IEEE80211_WDS_LINK_MAINTAIN_BA_TID &&
					ni->ni_qtn_assoc_ie) {
				/* For Q WDS peer, use larger buffer size for TID 0 to get more throughput */
				ic->ic_setparam(ni, IEEE80211_PARAM_HTBA_SIZE_CTRL,
					tid << 16 | IEEE80211_DEFAULT_BA_WINSIZE_H, 0, 0);
			} else {
				ic->ic_setparam(ni, IEEE80211_PARAM_HTBA_SIZE_CTRL,
						tid << 16 | vap->iv_max_ba_win_size, 0, 0);
			}
			ic->ic_setparam(ni, IEEE80211_PARAM_HTBA_TIME_CTRL,
					tid << 16 | 0x0, 0, 0);
			ic->ic_setparam(ni, IEEE80211_PARAM_HT_ADDBA,
					tid, 0, 0);
		}
	}

	ieee80211_free_node(ni);
}

/*
 * Configure block agreements for a node, workqueue version.
 * The node structure must be locked before scheduling this workqueue.
 */
static void ieee80211_tx_addba_work(struct work_struct *work)
{
	struct ieee80211_node *ni =
		container_of(work, struct ieee80211_node, ni_tx_addba_task);

	ieee80211_tx_addba(ni);
}

/*
 * Create an entry in the specified node table.  The node
 * is set up with the mac address, an initial reference count,
 * and some basic parameters obtained from global state.
 * This interface is not intended for general use, it is
 * used by the routines below to create entries with a
 * specific purpose.
 */
struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211_node_table *nt,
	struct ieee80211vap *vap, const u_int8_t *macaddr, const char *caller)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;
	int hash;
	int i;

	ni = ic->ic_node_alloc(nt, vap, macaddr, 0);
	if (ni == NULL) {
		/*
		 * Free the expired tdls node first and then try to
		 * allocate new node again
		 */
		if (vap->iv_opmode == IEEE80211_M_STA) {
			ieee80211_tdls_node_expire((unsigned long)vap);
			ni = ic->ic_node_alloc(nt, vap, macaddr, 0);
		}
		if (ni == NULL) {
			printk(KERN_WARNING "Failed to allocate node for %s\n", caller);
			vap->iv_stats.is_rx_nodealloc++;
			return NULL;
		}
	}

	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	hash = IEEE80211_NODE_HASH(macaddr);
	ni->ni_vap = vap;
	ni->ni_ic = ic;
#ifdef IEEE80211_DEBUG_REFCNT
	ni->ni_refdebug_info_p = kmalloc(sizeof(*ni->ni_refdebug_info_p), GFP_ATOMIC);
	if (ni->ni_refdebug_info_p)
		memset(ni->ni_refdebug_info_p, 0, sizeof(*ni->ni_refdebug_info_p));
#endif
	ieee80211_node_initref(ni);
	ieee80211_ref_node(ni);

	ni->ni_tbtt = 0;
	ni->ni_dtim_tbtt = 0;
	ni->ni_flags |= (IS_IEEE80211_VHT_ENABLED(ic) ? IEEE80211_NODE_VHT:IEEE80211_NODE_HT);
	ni->ni_htcap = ic->ic_htcap;
	ni->ni_vhtcap = ic->ic_vhtcap;
	ni->ni_chan = IEEE80211_CHAN_ANYC;
	ni->ni_authmode = IEEE80211_AUTH_OPEN;
	ni->ni_txpower = ic->ic_txpowlimit;	/* max power */
	ieee80211_crypto_resetkey(vap, &ni->ni_ucastkey, IEEE80211_KEYIX_NONE);
	ni->ni_inact_reload = nt->nt_inact_init;
	ni->ni_inact = ni->ni_inact_reload;
	ni->ni_ath_defkeyindex = IEEE80211_INVAL_DEFKEY;
	ni->ni_rxkeyoff = 0;
	INIT_WORK(&ni->ni_tx_addba_task, ieee80211_tx_addba_work);
	INIT_WORK(&ni->ni_inact_work, ieee80211_timeout_station_work);

	init_timer(&ni->ni_training_timer);
	ni->ni_training_flag = NI_TRAINING_INIT;
	ni->ni_node_type = IEEE80211_NODE_TYPE_NONE;
	spin_lock_init(&ni->ni_lock);
	ni->ni_training_count = 0;
	ni->ni_training_start = 0;
	ni->tdls_initiator = 0;
	ni->last_tx_phy_rate = -1;
	ni->tdls_path_sel_num = 0;
	ni->tdls_last_path_sel = 0;
	ni->tdls_no_send_cs_resp = 0;
	ni->tdls_send_cs_req = 0;
	ni->ni_chan_num = 0;
	memset(ni->ni_supp_chans, 0, sizeof(ni->ni_supp_chans));
	memset(&ni->ni_obss_ie, 0, sizeof(struct ieee80211_obss_scan_ie));
	ni->ni_ext_role = IEEE80211_EXTENDER_ROLE_NONE;
	ni->ni_ext_bssid_ie = NULL;
	get_random_bytes(&ni->ni_rate_train, sizeof(ni->ni_rate_train));

	ni->tdls_peer_associd = 0;
	ni->tdls_status = IEEE80211_TDLS_NODE_STATUS_NONE;

	IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");
#if defined(CONFIG_QTN_80211K_SUPPORT)
	init_waitqueue_head(&ni->ni_dotk_waitq);
#endif
	init_waitqueue_head(&ni->ni_meas_info.meas_waitq);
	init_waitqueue_head(&ni->ni_tpc_info.tpc_wait_info.tpc_waitq);

	for (i = 0; i < WME_NUM_TID; ++i) {
		ni->ni_ba_rx[i].state = IEEE80211_BA_NOT_ESTABLISHED;
		seqlock_init(&ni->ni_ba_rx[i].state_lock);
		ni->ni_ba_tx[i].state = IEEE80211_BA_NOT_ESTABLISHED;
		seqlock_init(&ni->ni_ba_tx[i].state_lock);
	}

	IEEE80211_NODE_LOCK_IRQ(nt);
	ni->ni_table = nt;
	TAILQ_INSERT_TAIL(&nt->nt_node, ni, ni_list);
	LIST_INSERT_HEAD(&nt->nt_hash[hash], ni, ni_hash);
	ni->ni_rxfrag = NULL;
	ni->ni_challenge = NULL;
	ni->ni_implicit_ba_size = IEEE80211_DEFAULT_BA_WINSIZE;
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	WME_UAPSD_NODE_TRIGSEQINIT(ni);

	return ni;
}
EXPORT_SYMBOL(ieee80211_alloc_node);

/* Add wds address to the node table */
int
ieee80211_add_wds_addr(struct ieee80211_node_table *nt,
	struct ieee80211_node *ni, const u_int8_t *macaddr, u_int8_t wds_static)
{
	int hash;
	struct ieee80211_wds_addr *wds;

	MALLOC(wds, struct ieee80211_wds_addr *, sizeof(struct ieee80211_wds_addr),
	       M_80211_WDS, M_NOWAIT | M_ZERO);
	if (wds == NULL) {
		/* XXX msg */
		return 1;
	}
	if (wds_static)
		wds->wds_agingcount = WDS_AGING_STATIC;
	else
		wds->wds_agingcount = WDS_AGING_COUNT;
	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_ADDR_COPY(wds->wds_macaddr, macaddr);
	wds->wds_ni = ni;
	IEEE80211_NODE_LOCK_IRQ(nt);
	LIST_INSERT_HEAD(&nt->nt_wds_hash[hash], wds, wds_hash);
	IEEE80211_NODE_UNLOCK_IRQ(nt);
	return 0;
}
EXPORT_SYMBOL(ieee80211_add_wds_addr);

/* remove wds address from the wds hash table */
void
ieee80211_remove_wds_addr(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	int hash;
	struct ieee80211_wds_addr *wds;

	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_NODE_LOCK_IRQ(nt);
	LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
		if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {
			LIST_REMOVE(wds, wds_hash);
			FREE(wds, M_80211_WDS);
			break;
		}
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);
}
EXPORT_SYMBOL(ieee80211_remove_wds_addr);


/* Remove node references from wds table */
void
ieee80211_del_wds_node(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
	int hash;
	struct ieee80211_wds_addr *wds;

	IEEE80211_NODE_LOCK_IRQ(nt);
	for (hash = 0; hash < IEEE80211_NODE_HASHSIZE; hash++) {
		LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
			if (wds->wds_ni == ni) {
				LIST_REMOVE(wds, wds_hash);
				FREE(wds, M_80211_WDS);
			}
		}
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);
}
EXPORT_SYMBOL(ieee80211_del_wds_node);

static void
ieee80211_node_wds_ageout(unsigned long data)
{
	struct ieee80211_node_table *nt = (struct ieee80211_node_table *)data;
	int hash;
	struct ieee80211_wds_addr *wds;

	IEEE80211_NODE_LOCK_IRQ(nt);
	for (hash = 0; hash < IEEE80211_NODE_HASHSIZE; hash++) {
		LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
			if (wds->wds_agingcount != WDS_AGING_STATIC) {
				if (!wds->wds_agingcount) {
					LIST_REMOVE(wds, wds_hash);
					FREE(wds, M_80211_WDS);
				} else
					wds->wds_agingcount--;
			}
		}
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);
	mod_timer(&nt->nt_wds_aging_timer, jiffies + HZ * WDS_AGING_TIMER_VAL);
}

/*
 * Craft a temporary node suitable for sending a management frame
 * to the specified station.  We craft only as much state as we
 * need to do the work since the node will be immediately reclaimed
 * once the send completes.
 */
struct ieee80211_node *
_ieee80211_tmp_node(struct ieee80211vap *vap, const u_int8_t *macaddr,
		const u_int8_t *bssid)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(&ic->ic_sta, vap, macaddr, 1);
	if (ni == NULL) {
		vap->iv_stats.is_rx_nodealloc++;
		return NULL;
	}

#ifdef IEEE80211_DEBUG_REFCNT
	ni->ni_refdebug_info_p = NULL;
#endif

	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	IEEE80211_ADDR_COPY(ni->ni_bssid, bssid);


	ni->ni_txpower = vap->iv_bss->ni_txpower;
	ni->ni_vap = vap;
	ni->ni_node_idx = vap->iv_vapnode_idx;

	ieee80211_node_initref(ni);

	/* NB: required by ieee80211_fix_rate */
	if (vap->iv_opmode == IEEE80211_M_STA) {
		ni->ni_rates = ic->ic_sup_rates[ic->ic_curmode];
	} else {
		ieee80211_node_set_chan(ic, ni);
	}
	ieee80211_crypto_resetkey(vap, &ni->ni_ucastkey,
		IEEE80211_KEYIX_NONE);
	/* XXX optimize away */
	IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");

	ni->ni_table = NULL;
	ni->ni_ic = ic;
	ni->ni_rxfrag = NULL;
	ni->ni_challenge = NULL;

	return ni;
}
EXPORT_SYMBOL(_ieee80211_tmp_node);

struct ieee80211_node *
ieee80211_tmp_node(struct ieee80211vap *vap, const u_int8_t *macaddr)
{
	return _ieee80211_tmp_node(vap, macaddr, vap->iv_bss->ni_bssid);
}
EXPORT_SYMBOL(ieee80211_tmp_node);

/*
 * Add the specified station to the station table.
 * The node is locked and must be freed by the caller.
 */
struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211vap *vap, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	ni = ieee80211_alloc_node(&ic->ic_sta, vap, macaddr, "bss dup");
	if (ni != NULL) {
		/*
		 * Inherit from iv_bss.
		 */
		ni->ni_authmode = vap->iv_bss->ni_authmode;
		ni->ni_txpower = vap->iv_bss->ni_txpower;
		ni->ni_vlan = vap->iv_bss->ni_vlan;	/* XXX?? */
		IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_bss->ni_bssid);
		ieee80211_node_set_chan(ic, ni);
		ni->ni_rsn = vap->iv_bss->ni_rsn;
		ni->ni_rxfrag = NULL;
		ni->ni_associd = vap->iv_bss->ni_associd;

		ni->ni_node_idx = vap->iv_bss->ni_node_idx;
	}
	return ni;
}

static struct ieee80211_node *
_ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	struct ieee80211_wds_addr *wds;
	int hash;
	IEEE80211_NODE_LOCK_ASSERT(nt);

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
		if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {
			ni = wds->wds_ni;
			if (wds->wds_agingcount != WDS_AGING_STATIC)
				wds->wds_agingcount = WDS_AGING_COUNT; /* reset the aging count */
			ieee80211_ref_node(ni);
			return ni;
		}
	}
	return NULL;
}

static struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
_ieee80211_find_node_debug(struct ieee80211_node_table *nt, const u_int8_t *macaddr,
				const char *filename,
				int line)
#else
_ieee80211_find_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
#endif
{
	struct ieee80211_node *ni;
	int hash;
	struct ieee80211_wds_addr *wds;

	IEEE80211_NODE_LOCK_ASSERT(nt);

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
			ieee80211_ref_node_debug(ni, filename, line);
			return ni;
		}
	}

	/* Now, we look for the desired mac address in the 4 address nodes */
	LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
		if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {
			ni = wds->wds_ni;
			ieee80211_ref_node_debug(ni, filename, line);
			return ni;
		}
	}
	return NULL;
}
#ifdef IEEE80211_DEBUG_REFCNT
#define	_ieee80211_find_node(nt, mac) \
	_ieee80211_find_node_debug(nt, mac, filename, line)
#endif

struct ieee80211_node *
ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK_IRQ(nt);
	ni = _ieee80211_find_wds_node(nt, macaddr);
	IEEE80211_NODE_UNLOCK_IRQ(nt);
	return ni;
}
EXPORT_SYMBOL(ieee80211_find_wds_node);

struct ieee80211_node * __sram_text
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_debug(struct ieee80211_node_table *nt, const u_int8_t *macaddr,
				const char *filename, int line)
#else
ieee80211_find_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
#endif
{
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK_IRQ(nt);
	ni = _ieee80211_find_node(nt, macaddr);
	IEEE80211_NODE_UNLOCK_IRQ(nt);
	return ni;
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_node_debug);
#else
EXPORT_SYMBOL(ieee80211_find_node);
#endif

#ifdef IEEE80211_DEBUG_REFCNT
#define	ieee80211_find_node_by_ip_addr \
	ieee80211_find_node_by_ip_addr_debug
#endif

struct ieee80211_node * __sram_text
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_by_ip_addr_debug(struct ieee80211_node_table *nt, uint32_t ip_addr,
			const char *filename,
			int line)
#else
ieee80211_find_node_by_ip_addr(struct ieee80211vap *vap, uint32_t ip_addr)
#endif
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni = NULL;

	IEEE80211_NODE_LOCK(nt);

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_vap->iv_dev != vap->iv_dev) {
			continue;
		}
		if (ni->ni_ip_addr && (ip_addr == ni->ni_ip_addr)) {
			ieee80211_ref_node_debug(ni, filename, line);
			break;
		}
	}

	IEEE80211_NODE_UNLOCK(nt);

	return ni;
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_node_by_ip_addr_debug);
#else
EXPORT_SYMBOL(ieee80211_find_node_by_ip_addr);
#endif

#ifdef CONFIG_IPV6
#ifdef IEEE80211_DEBUG_REFCNT
#define	ieee80211_find_node_by_ipv6_addr \
	ieee80211_find_node_by_ipv6_addr_debug
#endif

struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_by_ipv6_addr_debug(struct ieee80211_node_table *nt, struct in6_addr *ipv6_addr,
			const char *filename,
			int line)
#else
ieee80211_find_node_by_ipv6_addr(struct ieee80211vap *vap, struct in6_addr *ipv6_addr)
#endif
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni = NULL;

	IEEE80211_NODE_LOCK(nt);

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_vap->iv_dev != vap->iv_dev) {
			continue;
		}
		if (!memcmp(ni->ipv6_llocal.s6_addr, ipv6_addr, sizeof(struct in6_addr))) {
			ieee80211_ref_node_debug(ni, filename, line);
			break;
		}
	}

	IEEE80211_NODE_UNLOCK(nt);

	return ni;
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_node_by_ipv6_addr_debug);
#else
EXPORT_SYMBOL(ieee80211_find_node_by_ipv6_addr);
#endif
#endif

/**
 * Look up the AID by MAC address.
 * refcnt is not incremented.
 */
u_int16_t /*__sram_text*/
ieee80211_find_aid_by_mac_addr(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int hash;
	u_int16_t aid = 0;

	IEEE80211_NODE_LOCK_IRQ(nt);

	IEEE80211_NODE_LOCK_ASSERT(nt);

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
			aid = IEEE80211_AID(ni->ni_associd);
			break;
		}
	}

	IEEE80211_NODE_UNLOCK_IRQ(nt);
	return aid;
}
EXPORT_SYMBOL(ieee80211_find_aid_by_mac_addr);

struct ieee80211_node * __sram_text
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_by_node_idx_debug(struct ieee80211vap *vap, uint16_t node_idx,
			const char *filename,
			int line)
#else
ieee80211_find_node_by_node_idx(struct ieee80211vap *vap, uint16_t node_idx)
#endif
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK_IRQ(nt);
	ni = vap->iv_ic->ic_node_idx_ni[IEEE80211_NODE_IDX_UNMAP(node_idx)];
	if (ni) {
		ieee80211_ref_node_debug(ni, filename, line);
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	return ni;
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_node_by_node_idx_debug);
#else
EXPORT_SYMBOL(ieee80211_find_node_by_node_idx);
#endif

struct ieee80211_node * __sram_text
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_by_idx_debug(struct ieee80211com *ic,
			struct ieee80211vap *vap,
			uint16_t node_idx,
			const char *filename,
			int line)
#else
ieee80211_find_node_by_idx(struct ieee80211com *ic, struct ieee80211vap *vap, uint16_t node_idx)
#endif
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni = NULL;

	if (IEEE80211_NODE_IDX_UNMAP(node_idx) < QTN_NCIDX_MAX) {
		IEEE80211_NODE_LOCK_IRQ(nt);
		ni = ic->ic_node_idx_ni[IEEE80211_NODE_IDX_UNMAP(node_idx)];
		if (ni) {
			if (!vap || (ni->ni_vap == vap)) {
				ieee80211_ref_node_debug(ni, filename, line);
			} else {
				ni = NULL;
			}
		}
		IEEE80211_NODE_UNLOCK_IRQ(nt);
	}

	return ni;
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_node_by_idx_debug);
#else
EXPORT_SYMBOL(ieee80211_find_node_by_idx);
#endif

/*
 * Fake up a node; this handles node discovery in adhoc mode.
 * Note that for the driver's benefit we treat this like an association so the driver has an
 * opportunity to set up its private state.
 * The node is locked and must be freed by the caller.
 */
struct ieee80211_node *
ieee80211_fakeup_adhoc_node(struct ieee80211vap *vap, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;

	ni = ieee80211_dup_bss(vap, macaddr);
	if (ni != NULL) {
		/* XXX no rate negotiation; just dup */
		ni->ni_rates = vap->iv_bss->ni_rates;
		if (vap->iv_ic->ic_newassoc != NULL)
			vap->iv_ic->ic_newassoc(ni, 1);
		/* XXX not right for 802.1x/WPA */
		ieee80211_node_authorize(ni);
	}
	return ni;
}

/*
 * Do node discovery in adhoc mode on receipt of a beacon or probe response frame.
 * Note that for the driver's benefit we treat this like an association so the driver has an
 * opportunity to set up its private state.
 * The node is locked and must be freed by the caller.
 */
struct ieee80211_node *
ieee80211_add_neighbor(struct ieee80211vap *vap,	const struct ieee80211_frame *wh,
	const struct ieee80211_scanparams *sp)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	ni = ieee80211_dup_bss(vap, wh->i_addr2);	/* XXX alloc_node? */
	/* TODO: not really putting itself in a table */
	if (ni != NULL) {
		ni->ni_esslen = sp->ssid[1];
		memcpy(ni->ni_essid, sp->ssid + 2, sp->ssid[1]);

		IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
		memcpy(ni->ni_tstamp.data, sp->tstamp, sizeof(ni->ni_tstamp));
		ni->ni_intval = IEEE80211_BINTVAL_SANITISE(sp->bintval);
		ni->ni_capinfo = sp->capinfo;
		ni->ni_chan = ic->ic_curchan;
		ni->ni_fhdwell = sp->fhdwell;
		ni->ni_fhindex = sp->fhindex;
		ni->ni_erp = sp->erp;
		ni->ni_timoff = sp->timoff;
		if (sp->wme != NULL)
			ieee80211_saveie(&ni->ni_wme_ie, sp->wme);
		if (sp->wpa != NULL)
			ieee80211_saveie(&ni->ni_wpa_ie, sp->wpa);
		if (sp->rsn != NULL)
			ieee80211_saveie(&ni->ni_rsn_ie, sp->rsn);
		if (sp->ath != NULL)
			ieee80211_saveath(ni, sp->ath);

		/* NB: must be after ni_chan is set up */
		ieee80211_setup_rates(ni, sp->rates, sp->xrates, IEEE80211_F_DOSORT);

		if (ic->ic_newassoc != NULL)
			ic->ic_newassoc(ni, 1);
		/* XXX not right for 802.1x/WPA */
		ieee80211_node_authorize(ni);
		if (vap->iv_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * Blindly propagate capabilities based on the
			 * local configuration.  In particular this permits
			 * us to use QoS to disable ACK's and to use short
			 * preamble on 2.4G channels.
			 */
			if (vap->iv_flags & IEEE80211_F_WME)
				ni->ni_flags |= IEEE80211_NODE_QOS;
			if (vap->iv_flags & IEEE80211_F_SHPREAMBLE)
				ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		}
	}

	return ni;
}

/*
 * Locate the node for sender, track state, and then pass the
 * (referenced) node up to the 802.11 layer for its use.  We
 * return NULL when the sender is unknown; the driver is required
 * locate the appropriate virtual ap in that case; possibly
 * sending it to all (using ieee80211_input_all).
 */
struct ieee80211_node * __sram_text
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_rxnode_debug(struct ieee80211com *ic,
			const struct ieee80211_frame_min *wh,
			const char *filename,
			int line)
#else
ieee80211_find_rxnode(struct ieee80211com *ic, const struct ieee80211_frame_min *wh)
#endif
{
#define	IS_CTL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
#define	IS_PSPOLL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	/* XXX check ic_bss first in station mode */
	/* XXX 4-address frames? */
	nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK_IRQ(nt);
	if (IS_CTL(wh) && !IS_PSPOLL(wh) /*&& !IS_RTS(ah)*/)
		ni = _ieee80211_find_node(nt, wh->i_addr1);
	else
		ni = _ieee80211_find_node(nt, wh->i_addr2);
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	return ni;
#undef IS_PSPOLL
#undef IS_CTL
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_rxnode_debug);
#else
EXPORT_SYMBOL(ieee80211_find_rxnode);
#endif

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_txnode_debug(struct ieee80211vap *vap,
			const u_int8_t *mac,
			const char *filename,
			int line)
#else
ieee80211_find_txnode(struct ieee80211vap *vap, const u_int8_t *mac)
#endif
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	/*
	 * The destination address should be in the node table
	 * unless we are operating in station mode or this is a
	 * multicast/broadcast frame.
	 */
	if (vap->iv_opmode == IEEE80211_M_STA || IEEE80211_IS_MULTICAST(mac)) {
		if (vap->iv_bss)
			ieee80211_ref_node_debug(vap->iv_bss, filename, line);
		return vap->iv_bss;
	}

	/* XXX can't hold lock across dup_bss due to recursive locking */
	nt = &vap->iv_ic->ic_sta;
	IEEE80211_NODE_LOCK_IRQ(nt);
	ni = _ieee80211_find_node(nt, mac);
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	if (ni == NULL) {
		if (vap->iv_opmode == IEEE80211_M_IBSS ||
		    vap->iv_opmode == IEEE80211_M_AHDEMO) {

			ni = ieee80211_fakeup_adhoc_node(vap, mac);
		} else {
//			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_OUTPUT, mac,
//				"no node, discard frame (%s)", __func__);
//			vap->iv_stats.is_tx_nonode++;
		}
	}
	return ni;
}

#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_txnode_debug);
#else
EXPORT_SYMBOL(ieee80211_find_txnode);
#endif

void
ieee80211_idx_add(struct ieee80211_node *ni, uint16_t new_idx)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;

	if (new_idx == IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx)) {
		return;
	}

	IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
		"aid=%u change idx from %u to %u\n",
		IEEE80211_NODE_AID(ni), IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx), new_idx);

	ieee80211_idx_remove(ni);

	if ((new_idx == 0) || (new_idx >= QTN_NCIDX_MAX)) {
		printk("%s: [%pM] invalid idx %u\n", __func__, ni->ni_macaddr, new_idx);
		return;
	}

	ni->ni_node_idx = IEEE80211_NODE_IDX_MAP(new_idx);
	ic->ic_register_node(ni);
	ic->ic_node_idx_ni[new_idx] = ni;
	if (vap->iv_opmode == IEEE80211_M_WDS) {
		vap->iv_vapnode_idx = ni->ni_node_idx;
	}
}
EXPORT_SYMBOL(ieee80211_idx_add);

/* Caller must lock the IEEE80211_NODE_LOCK
 *
 * Context: hwIRQ, softIRQ and process context
 */
static void __sram_text
_ieee80211_free_node(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

#ifdef IEEE80211_DEBUG_REFCNT
	if (ni->ni_refdebug_info_p) {
		ieee80211_node_show_refdebug_info(ni);
		kfree(ni->ni_refdebug_info_p);
		ni->ni_refdebug_info_p = NULL;
	} else if (ni->ni_table != NULL) {
		printk("%s:%d: freeing node %p\n", __func__, __LINE__, ni);
	}
#endif
	_ieee80211_remove_node(ni);

	vap->iv_ic->ic_node_free(ni);
}

void __sram_text
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_free_node_debug(struct ieee80211_node *ni, const char *filename, int line)
#else
ieee80211_free_node(struct ieee80211_node *ni)
#endif
{
	struct ieee80211_node_table *nt = ni->ni_table;
	struct ieee80211com *ic = ni->ni_ic;

	/*
	 * XXX: may need to lock out the following race. we dectestref
	 *      and determine it's time to free the node. between the if()
	 *      and lock, we take an rx intr to receive a frame from this
	 *      node. the rx path (tasklet or intr) bumps this node's
	 *      refcnt and xmits a response frame. eventually that response
	 *      will get reaped, and the reaping code will attempt to use
	 *      the node. the code below will delete the node prior
	 *      to the reap and we could get a crash.
	 *
	 *      as a stopgap before delving deeper, lock intrs to
	 *      prevent this case.
	 */
	IEEE80211_LOCK_IRQ(ic);
	if (ieee80211_node_dectestref(ni)) {
		ieee80211_node_dbgref(ni, filename, line, IEEE80211_NODEREF_DECR);
		/*
		 * Beware; if the node is marked gone then it's already
		 * been removed from the table and we cannot assume the
		 * table still exists.  Regardless, there's no need to lock
		 * the table.
		 */
		if (ni->ni_table != NULL) {
			IEEE80211_NODE_LOCK(nt);
			_ieee80211_free_node(ni);
			IEEE80211_NODE_UNLOCK(nt);
		} else
			_ieee80211_free_node(ni);
	} else {
		ieee80211_node_dbgref(ni, filename, line, IEEE80211_NODEREF_DECR);
	}
	IEEE80211_UNLOCK_IRQ(ic);
}
#ifdef IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_free_node_debug);
#else
EXPORT_SYMBOL(ieee80211_free_node);
#endif

void
ieee80211_sta_assocs_inc(struct ieee80211vap *vap, const char *caller)
{
	struct ieee80211com *ic = vap->iv_ic;

	ic->ic_sta_assoc++;
	vap->iv_sta_assoc++;
	ic->ic_ssid_grp[vap->iv_ssid_group].assocs++;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
		"%s - increases counter ic_sta_assoc %u, iv_sta_assoc %u\n",
		caller, ic->ic_sta_assoc, vap->iv_sta_assoc);
}

void
ieee80211_sta_assocs_dec(struct ieee80211vap *vap, const char *caller)
{
	struct ieee80211com *ic = vap->iv_ic;

	WARN_ON(ic->ic_sta_assoc == 0);
	WARN_ON(vap->iv_sta_assoc == 0);
	WARN_ON(ic->ic_ssid_grp[vap->iv_ssid_group].assocs == 0);

	if (ic->ic_sta_assoc > 0)
		ic->ic_sta_assoc--;
	if (vap->iv_sta_assoc > 0)
		vap->iv_sta_assoc--;
	if (ic->ic_ssid_grp[vap->iv_ssid_group].assocs > 0)
		ic->ic_ssid_grp[vap->iv_ssid_group].assocs--;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
		"%s - reduces counter ic_sta_assoc %u, iv_sta_assoc %u\n",
		caller, ic->ic_sta_assoc, vap->iv_sta_assoc);
}

static void
ieee80211_node_table_reset(struct ieee80211com *ic, struct ieee80211_node_table *nt,
	struct ieee80211vap *match)
{
	struct ieee80211_node *ni, *next;

	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
		if (match != NULL && ni->ni_vap != match)
			continue;
		if (ni->ni_associd != 0) {
			struct ieee80211vap *vap = ni->ni_vap;

			if (vap->iv_auth->ia_node_leave != NULL)
				vap->iv_auth->ia_node_leave(ni);
			ieee80211_clear_aid_bitmap(ic, ni->ni_associd);
			ieee80211_sta_assocs_dec(vap, __func__);
			ieee80211_nonqtn_sta_leave(vap, ni, __func__);
		}
		ieee80211_node_reclaim(nt, ni);
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);
}

static void
ieee80211_node_table_cleanup(struct ieee80211com *ic, struct ieee80211_node_table *nt)
{
	struct ieee80211_node *ni, *next;

	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
		if (ni->ni_associd != 0) {
			struct ieee80211vap *vap = ni->ni_vap;

			if (vap->iv_auth->ia_node_leave != NULL)
				vap->iv_auth->ia_node_leave(ni);
			ieee80211_clear_aid_bitmap(ic, ni->ni_associd);
		}
		ieee80211_node_reclaim(nt, ni);
	}
	del_timer(&nt->nt_wds_aging_timer);
	IEEE80211_SCAN_LOCK_DESTROY(nt);
	IEEE80211_NODE_LOCK_DESTROY(nt);
}

void
ieee80211_node_ba_del(struct ieee80211_node *ni, uint8_t tid, uint8_t is_tx, uint16_t reason)
{
	enum ieee80211_ba_state	new_state;
	int state_changed = 0;

	if (is_tx) {
		if (ni->ni_vap->iv_ba_control & (1 << tid)) {
			new_state = IEEE80211_BA_NOT_ESTABLISHED;
		} else {
			new_state = IEEE80211_BA_BLOCKED;
		}
		if (ni->ni_ba_tx[tid].state != new_state) {
			state_changed = 1;
			memset(&ni->ni_ba_tx[tid], 0, sizeof(ni->ni_ba_tx[tid]));
			ieee80211_node_tx_ba_set_state(ni, tid, new_state,
				reason == IEEE80211_REASON_STA_NOT_USE ?
					IEEE80211_TX_BA_REQUEST_NEW_ATTEMPT_TIMEOUT : 0);
		}
	} else {
		if (ni->ni_ba_rx[tid].state != IEEE80211_BA_NOT_ESTABLISHED) {
#ifdef CONFIG_QVSP
			struct ieee80211_ba_throt ba_throt;
			/* backup and restore BA throt state across BA delete */
			memcpy(&ba_throt, &ni->ni_ba_rx[tid].ba_throt, sizeof(struct ieee80211_ba_throt));
#endif
			state_changed = 1;
			memset(&ni->ni_ba_rx[tid], 0, sizeof(ni->ni_ba_rx[tid]));
			ni->ni_ba_rx[tid].state = IEEE80211_BA_NOT_ESTABLISHED;
#ifdef CONFIG_QVSP
			memcpy(&ni->ni_ba_rx[tid].ba_throt, &ba_throt, sizeof(struct ieee80211_ba_throt));
#endif
		}
	}

	if (state_changed && ni->ni_ic->ic_htdelba) {
		IEEE80211_NOTE(ni->ni_vap,
				IEEE80211_MSG_NODE | IEEE80211_MSG_INPUT | IEEE80211_MSG_OUTPUT, ni,
			       "block ack del tid=%d is_tx=%d reason=%d",
			       (int)tid, (int)is_tx, (int)reason);
		(*ni->ni_ic->ic_htdelba)(ni, tid, is_tx);
	}
}
EXPORT_SYMBOL(ieee80211_node_ba_del);

#define IEEE80211_MAX_WDS_BA_ATTEMPTS 3

static void
ieee80211_timeout_station_work_wds(struct work_struct *work)
{
	struct ieee80211_node *ni = container_of(work, struct ieee80211_node, ni_inact_work);
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct qtn_wds_ext_event_data event_data;
	struct ieee80211vap *primary_vap = NULL;

	if (ni->ni_inact == 0) {
		/* Nothing received (including beacons) */
		IEEE80211_NOTE(ni->ni_vap,
			IEEE80211_MSG_INACT | IEEE80211_MSG_NODE, ni,
			"%s", "no beacons received from WDS peer");

		ieee80211_node_ba_state_clear(ni);
		ni->ni_inact = ni->ni_inact_reload;
		ieee80211_pm_queue_work(ic);

		if ((ic->ic_extender_role != IEEE80211_EXTENDER_ROLE_NONE) &&
				!IEEE80211_VAP_WDS_BASIC(vap)) {
			primary_vap = TAILQ_FIRST(&ic->ic_vaps);
			extender_event_data_prepare(ic, NULL,
					&event_data,
					WDS_EXT_LINK_STATUS_UPDATE,
					ni->ni_macaddr);
			ieee80211_extender_send_event(primary_vap, &event_data, NULL);
			ieee80211_extender_remove_peer_wds_info(ic, ni->ni_macaddr);
		}
	}

	if (!IEEE80211_BA_IS_COMPLETE(ni->ni_ba_rx[IEEE80211_WDS_LINK_MAINTAIN_BA_TID].state) ||
		!IEEE80211_BA_IS_COMPLETE(ni->ni_ba_tx[IEEE80211_WDS_LINK_MAINTAIN_BA_TID].state)) {

		if (ni->ni_wds_ba_attempts++ >= IEEE80211_MAX_WDS_BA_ATTEMPTS) {
			/* Still not fully established, send DELBA to peer */
			ni->ni_wds_ba_attempts = 0;
			ieee80211_node_ba_state_clear(ni);
			ic->ic_setparam(ni, IEEE80211_PARAM_HT_DELBA,
					IEEE80211_WDS_LINK_MAINTAIN_BA_TID, 0, 0); }
	} else {
		ni->ni_wds_ba_attempts = 0;
	}

	if (ni->ni_ba_tx[IEEE80211_WDS_LINK_MAINTAIN_BA_TID].state == IEEE80211_BA_NOT_ESTABLISHED) {
		ieee80211_node_tx_ba_set_state(ni, IEEE80211_WDS_LINK_MAINTAIN_BA_TID,
						IEEE80211_BA_REQUESTED, 0);
	}

	ieee80211_tx_addba(ni);
}

/*
 * This workqueue is called for any station that requires timeout processing.
 * A node reference is taken before calling to prevent premature deletion.
 * If the inact timer has not yet expired the station is probed with a null
 * data probe (this can happen multiple times).  If the inact timer has expired, the node
 * is removed.
 */
static void
ieee80211_timeout_station_work(struct work_struct *work)
{
	struct ieee80211_node *ni = container_of(work, struct ieee80211_node, ni_inact_work);
	struct ieee80211vap *vap = ni->ni_vap;

	if (qtn_vlan_is_group_addr(ni->ni_macaddr)) {
		ieee80211_free_node(ni);
		return;
	}

	/* Send dummy data packets to check if ACK can be received */
	if ((IEEE80211_NODE_IS_TDLS_ACTIVE(ni) || IEEE80211_NODE_IS_TDLS_IDLE(ni)) &&
				(ni->ni_inact <= IEEE80211_INACT_SEND_PKT_THRSH))
		ieee80211_send_dummy_data(ni, vap, M_TX_DONE_IMM_INT);
	if (vap->iv_opmode == IEEE80211_M_WDS) {
		ieee80211_timeout_station_work_wds(work);
		return;
	}


	if (ni->ni_inact > 0) {
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			IEEE80211_NOTE(vap, IEEE80211_MSG_INACT | IEEE80211_MSG_NODE | IEEE80211_MSG_POWER,
			ni, "%s", "probe station due to inactivity");
			/* Either of these frees the node reference */
			if (ni->ni_flags & IEEE80211_NODE_QOS) {
				/* non-QoS frames to 3rd party QoS node (Intel) can cause a BA teardown */
				ieee80211_send_qosnulldata(ni, WMM_AC_BK);
			} else {
				ieee80211_send_nulldata(ni);
			}
		} else {
			ieee80211_free_node(ni);
		}
		return;
	}

	IEEE80211_NOTE(ni->ni_vap,
		IEEE80211_MSG_INACT | IEEE80211_MSG_NODE, ni,
		"station timed out due to inactivity (refcnt %u)",
		ieee80211_node_refcnt(ni));

	ni->ni_vap->iv_stats.is_node_timeout++;

	if (ni->ni_associd != 0 && (vap->iv_opmode != IEEE80211_M_STA ||
			IEEE80211_NODE_IS_NONE_TDLS(ni))) {
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DISASSOC, IEEE80211_REASON_ASSOC_EXPIRE);
		ieee80211_dot11_msg_send(ni->ni_vap, (char *)ni->ni_macaddr,
			d11_m[IEEE80211_DOT11_MSG_CLIENT_REMOVED],
			d11_c[IEEE80211_DOT11_MSG_REASON_CLIENT_TIMEOUT],
			-1, NULL, NULL, NULL);
	}

	if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
			enum ieee80211_tdls_operation operation = IEEE80211_TDLS_TEARDOWN;
			if (ieee80211_tdls_send_event(ni, IEEE80211_EVENT_STATION_LOW_ACK, &operation))
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS %s: Send event %d failed\n", __func__, operation);
	} else if (IEEE80211_NODE_IS_TDLS_IDLE(ni)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: TDLS peer timed out due to inactivity (tdls_status %d)\n",
				__func__, ni->tdls_status);
		ieee80211_tdls_node_leave(vap, ni);
	} else {
		ieee80211_node_leave(ni);
	}
	ieee80211_free_node(ni);
}

static void
ieee80211_timeout_station_sched(struct ieee80211_node *ni)
{
	ieee80211_ref_node(ni);
	if (unlikely(schedule_work(&ni->ni_inact_work) == 0))
		ieee80211_free_node(ni);
}

static int __inline ieee80211_wds_node_cac_check(struct ieee80211com *ic)
{
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
			(ic->ic_bsschan->ic_flags & IEEE80211_CHAN_DFS) &&
			IEEE80211_IS_CHAN_CAC_IN_PROGRESS(ic->ic_bsschan))
		return 1;

	return 0;
}

static void
ieee80211_timeout_station(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	int is_adhoc;
	int su = STATS_SU;

	if ((ni == ni->ni_vap->iv_bss) ||
			IEEE80211_ADDR_EQ(ni->ni_vap->iv_myaddr, ni->ni_macaddr) ||
			ieee80211_blacklist_check(ni)) {
		return;
	}

	/*
	 * Free fragment if not needed anymore (last fragment older than 1s).
	 * XXX doesn't belong here
	 */
	if (ni->ni_rxfrag != NULL &&
			time_after(jiffies, ni->ni_rxfragstamp + HZ)) {
		dev_kfree_skb(ni->ni_rxfrag);
		ni->ni_rxfrag = NULL;
	}

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_INACT | IEEE80211_MSG_NODE, ni,
		"sta ni_inact=%u rx=%u/%u/%u ack=%u/%u/%u",
		ni->ni_inact,
		ni->ni_shared_stats->rx[su].pkts_cum, ni->rx_pkts,
		(ni->ni_shared_stats->rx[su].pkts_cum - ni->rx_pkts),
		ni->ni_shared_stats->tx[su].acks, ni->tx_acks,
		(ni->ni_shared_stats->tx[su].acks - ni->tx_acks));

	/*
	 * WDS nodes are never timed out or removed, but this timeout station work mechanism is used
	 * to establish a block ack if needed.
	 */
	if (ni->ni_vap->iv_opmode == IEEE80211_M_WDS &&
			IEEE80211_ADDR_EQ(ni->ni_macaddr, ni->ni_vap->wds_mac)) {
		if ((ni->ni_inact > 0) && !ieee80211_wds_node_cac_check(ic))
			ni->ni_inact--;
		ieee80211_timeout_station_sched(ni);
		return;
	}

	if ((ni->ni_shared_stats->rx[su].pkts_cum != ni->rx_pkts) ||
			(ni->ni_shared_stats->tx[su].acks != ni->tx_acks)) {
		ni->ni_inact = ni->ni_inact_reload;
	} else {
		if (ni->ni_inact > 0)
			ni->ni_inact--;
	}
	/* always reset the local counters in case the shared counters wrap */
	ni->rx_pkts = ni->ni_shared_stats->rx[su].pkts_cum;
	ni->tx_acks = ni->ni_shared_stats->tx[su].acks;

	if (ni->ni_inact > 0) {
		is_adhoc = (ic->ic_opmode == IEEE80211_M_IBSS ||
				ic->ic_opmode == IEEE80211_M_AHDEMO);
		if ((ni->ni_associd != 0 || is_adhoc) &&
				(ni->ni_inact <= ni->ni_vap->iv_inact_probe ||
					ic->ic_ocac.ocac_running)) {
			ieee80211_timeout_station_sched(ni);
		}
	} else {
		ieee80211_timeout_station_sched(ni);
	}
}

/*
 * Process inactive stations and do related housekeeping.
 * Actual processing is offloaded to a workqueue because many of the called fuctions
 * cannot run at interrupt level.  This also keeps table parsing simple because nodes
 * are not removed during the loop.
 */
static void
ieee80211_timeout_stations(struct ieee80211_node_table *nt)
{
	struct ieee80211_node *ni;
	struct ieee80211com *ic = nt->nt_ic;

	IEEE80211_SCAN_LOCK_IRQ(nt);
	IEEE80211_NODE_LOCK_IRQ(nt);

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		ieee80211_timeout_station(ic, ni);
	}

	IEEE80211_NODE_UNLOCK_IRQ(nt);
	IEEE80211_SCAN_UNLOCK_IRQ(nt);
}

/*
 * Send out null frame from stations periodically
 * to check the validity of the connection.
 */
static void
ieee80211_timeout_ap(struct ieee80211com *ic)
{
	struct ieee80211vap *vap = NULL;
	struct ieee80211_node *ni;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		KASSERT(vap != NULL, ("timeout ap, vap is invalid.\n"));
		if ((vap->iv_opmode == IEEE80211_M_STA) &&
		    (vap->iv_state == IEEE80211_S_RUN)) {
			ni = vap->iv_bss;
			ieee80211_ref_node(ni);

			IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_INACT | IEEE80211_MSG_NODE, ni,
				"ap ni_inact=%u ack=%u/%u/%u",
				ni->ni_inact,
				ni->ni_shared_stats->tx[STATS_SU].acks, ni->tx_acks,
				(ni->ni_shared_stats->tx[STATS_SU].acks - ni->tx_acks));

			if (ni->ni_shared_stats->tx[STATS_SU].acks != ni->tx_acks) {
				ni->ni_inact = ni->ni_inact_reload;
			} else if (ni->ni_inact > 0) {
				ni->ni_inact--;
			}
			ni->tx_acks = ni->ni_shared_stats->tx[STATS_SU].acks;

			ieee80211_send_nulldata(ni);
		}
	}
}

/*
 * Per-ieee80211com inactivity timer callback.
 */
static void
ieee80211_node_timeout(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *) arg;

	ieee80211_timeout_stations(&ic->ic_sta);
	ieee80211_timeout_ap(ic);

	ic->ic_inact.expires = jiffies + IEEE80211_INACT_WAIT * HZ;
	add_timer(&ic->ic_inact);
}

void
ieee80211_iterate_nodes(struct ieee80211_node_table *nt, ieee80211_iter_func *f,
			void *arg, int ignore_blacklisted)
{
	ieee80211_iterate_dev_nodes(NULL, nt, f, arg, ignore_blacklisted);
}
EXPORT_SYMBOL(ieee80211_iterate_nodes);

void
ieee80211_iterate_dev_nodes(struct net_device *dev, struct ieee80211_node_table *nt,
			    ieee80211_iter_func *f, void *arg, int ignore_blacklisted)
{
	struct ieee80211_node *ni;
	u_int gen;

	IEEE80211_SCAN_LOCK_BH(nt);
	gen = ++nt->nt_scangen;
restart:
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if ((dev != NULL && ni->ni_vap->iv_dev != dev) ||
		    (ignore_blacklisted && (ni->ni_blacklist_timeout > 0))) {
			continue;
		}

		if (ni->ni_scangen != gen) {
			ni->ni_scangen = gen;
			ieee80211_ref_node(ni);
			IEEE80211_NODE_UNLOCK(nt);
			(*f)(arg, ni);
			ieee80211_free_node(ni);
			goto restart;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_SCAN_UNLOCK_BH(nt);
}
EXPORT_SYMBOL(ieee80211_iterate_dev_nodes);

void
ieee80211_dump_node(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
	int i;

	printf("Node 0x%p: mac %s refcnt %d\n", ni,
		ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));
	printf("    scangen %u authmode %u flags 0x%x\n",
		ni->ni_scangen, ni->ni_authmode, ni->ni_flags);
	printf("    associd 0x%x txpower %u vlan %u\n",
		ni->ni_associd, ni->ni_txpower, ni->ni_vlan);
	printf ("    rxfragstamp %lu\n", ni->ni_rxfragstamp);
	for (i = 0; i < 17; i++) {
		printf("    %d: txseq %u rxseq %u fragno %u\n", i,
		       ni->ni_txseqs[i],
		       ni->ni_rxseqs[i] >> IEEE80211_SEQ_SEQ_SHIFT,
		       ni->ni_rxseqs[i] & IEEE80211_SEQ_FRAG_MASK);
	}
	printf("    rstamp %u rssi %u intval %u capinfo 0x%x\n",
		ni->ni_rstamp, ni->ni_rssi, ni->ni_intval, ni->ni_capinfo);
	printf("    bssid %s essid \"%.*s\" channel %u:0x%x\n",
		ether_sprintf(ni->ni_bssid),
		ni->ni_esslen, ni->ni_essid,
		ni->ni_chan != IEEE80211_CHAN_ANYC ?
			ni->ni_chan->ic_freq : IEEE80211_CHAN_ANY,
		ni->ni_chan != IEEE80211_CHAN_ANYC ? ni->ni_chan->ic_flags : 0);
	printf("    inact %u txrate %u tdls %d\n",
		ni->ni_inact, ni->ni_txrate, ni->tdls_status);

}

void
ieee80211_dump_nodes(struct ieee80211_node_table *nt)
{
	ieee80211_iterate_nodes(nt,
		(ieee80211_iter_func *) ieee80211_dump_node, nt, 1);
}
EXPORT_SYMBOL(ieee80211_dump_nodes);

#ifdef CONFIG_QVSP
void
ieee80211_node_vsp_send_action(void *arg, struct ieee80211_node *ni)
{
	if ((ni->ni_associd != 0) && ni->ni_qtn_assoc_ie) {
		ni->ni_ic->ic_send_mgmt(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)arg);
	}
}
EXPORT_SYMBOL(ieee80211_node_vsp_send_action);
#endif

/*
 * Handle a station joining an 11g network.
 */
static void
ieee80211_node_join_11g(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;

	IEEE80211_LOCK_ASSERT(ic);

	KASSERT(IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan),
	     ("not in 11g, bss %u:0x%x, curmode %u", ic->ic_bsschan->ic_freq,
	      ic->ic_bsschan->ic_flags, ic->ic_curmode));

	/*
	 * If STA isn't capable of short slot time, bump
	 * the count of long slot time stations.
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0) {
		ic->ic_longslotsta++;
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
			"station needs long slot time, count %d",
			ic->ic_longslotsta);
	}

	/*
	 * If the new station is not an ERP station
	 * then bump the counter and enable protection
	 * if configured.
	 */
	if (!ieee80211_iserp_rateset(ic, &ni->ni_rates)) {
		ic->ic_nonerpsta++;
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
			"station is !ERP, %d non-ERP stations associated",
			ic->ic_nonerpsta);
		/*
		 * If protection is configured, enable it.
		 */
		if (IEEE80211_BG_PROTECT_ENABLED(ic)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
				"%s: enable use of protection\n", __func__);
			ic->ic_flags |= IEEE80211_F_USEPROT;
		}
		/*
		 * If station does not support short preamble
		 * then we must enable use of Barker preamble.
		 */
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) == 0) {
			IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
				"%s", "station needs long preamble");
			ic->ic_flags |= IEEE80211_F_USEBARKER;
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		}

		/* Update ERP element if this is first non ERP station */
		if (IEEE80211_BG_PROTECT_ENABLED(ic) && ic->ic_nonerpsta == 1) {
			ic->ic_flags_ext |= IEEE80211_FEXT_ERPUPDATE;

			/* tell Muc to use cts-to-self mechanism */
			ic->ic_set_11g_erp(vap, 1);
		}
	} else {
		ni->ni_flags |= IEEE80211_NODE_ERP;
	}

	/*
	 * Disable use of short slot time in following condition.
	 * Note that the actual switch over to long slot time use may not
	 * occur until the next beacon transmission (per sec. 7.3.1.4 of 11g).
	 */
	if ((ic->ic_longslotsta > 0) || (ic->ic_nonerpsta > 0)) {
		/* XXX vap's w/ conflicting needs won't work */
		if (!IEEE80211_IS_CHAN_108G(ic->ic_bsschan)) {
			/*
			 * Don't force slot time when switched to turbo
			 * mode as non-ERP stations won't be present; this
			 * need only be done when on the normal G channel.
			 */
			ieee80211_set_shortslottime(ic, 0);
		}
	}
}

void
ieee80211_node_ba_state_clear(struct ieee80211_node *ni)
{
	uint8_t i = 0;
	for (i = 0; i < 7; i++) {
		ieee80211_node_ba_del(ni, i, 0, IEEE80211_REASON_UNSPECIFIED);
		ieee80211_node_ba_del(ni, i, 1, IEEE80211_REASON_UNSPECIFIED);
	}
}

void
ieee80211_node_implicit_ba_setup(struct ieee80211_node *ni)
{
	int i = 0;
	u_int8_t our_ba = ni->ni_vap->iv_implicit_ba;
	u_int8_t their_ba = ni->ni_implicit_ba;
	u_int16_t size = ni->ni_implicit_ba_size;

	for (i = 0; i < 7; i++) {
		if ((our_ba >> i) & 0x1) {
			/* Add the BA for TX */
			ni->ni_ba_tx[i].state = IEEE80211_BA_ESTABLISHED;
			ni->ni_ba_tx[i].type = 1;
			ni->ni_ba_tx[i].buff_size = size;
			ni->ni_ba_tx[i].timeout = 0;
			ni->ni_ba_tx[i].frag = 0;
			ni->ni_ba_tx[i].seq = 0;
			printk(KERN_WARNING "%s: [%s] implicit TX BA for TID %d, win_size=%d\n",
				ni->ni_vap->iv_dev->name, ether_sprintf(ni->ni_macaddr), i, (int)size);
		}
		if ((their_ba >> i) & 0x1) {
			/* Add the BA for RX */
			ni->ni_ba_rx[i].state = IEEE80211_BA_ESTABLISHED;
			ni->ni_ba_rx[i].type = 1;
			ni->ni_ba_rx[i].buff_size = size;
			ni->ni_ba_rx[i].timeout = 0;
			ni->ni_ba_rx[i].frag = 0;
			ni->ni_ba_rx[i].seq = 0;
			printk(KERN_WARNING "%s: [%s] implicit RX BA for TID %d, win_size=%d\n",
				ni->ni_vap->iv_dev->name, ether_sprintf(ni->ni_macaddr), i, (int)size);
		}
	}
}

/*
 * Decide wheter to turn off/on reception of control packets.
 * If third party peers are present: Allow control frames
 */
void ieee80211_set_recv_ctrlpkts(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (ic->ic_nonqtn_sta) {
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_RX_CTRL_FILTER,
					1, NULL, 0);
	} else {
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_RX_CTRL_FILTER,
					0, NULL, 0);
	}
}

int ieee80211_aid_acquire(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	static uint16_t last_used_aid = 0;
	int aid;
	int ret = 0;

	IEEE80211_LOCK_IRQ(ic);

	for (aid = (last_used_aid + 1) % QTN_NODE_TBL_SIZE_LHOST;
			aid != last_used_aid;
			aid = ((aid + 1) % QTN_NODE_TBL_SIZE_LHOST)) {

		/* AID 0 is reserved for the BSS */
		if (aid == 0) {
			continue;
		}

		if ((ni->ni_vap->iv_opmode == IEEE80211_M_STA) &&
			(aid == IEEE80211_AID(ni->ni_vap->iv_bss->ni_associd)))
			continue;

		if (!IEEE80211_AID_ISSET(ic, aid)) {
			break;
		}
	}

	if (aid == last_used_aid) {
		ret = -1;
	} else {
		last_used_aid = aid;
		ni->ni_associd = aid | 0xc000;
		IEEE80211_AID_SET(ic, ni->ni_associd);
	}

	IEEE80211_UNLOCK_IRQ(ic);

	return ret;
}
EXPORT_SYMBOL(ieee80211_aid_acquire);

void
ieee80211_node_join(struct ieee80211_node *ni, int resp)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap, *tmp_vap;
	int newassoc;
	u_int8_t beacon_update_required = 0;

	if (ni->ni_associd == 0) {
		if (ieee80211_aid_acquire(ic, ni)) {
			IEEE80211_SEND_MGMT(ni, resp,
				IEEE80211_REASON_ASSOC_TOOMANY);
			ieee80211_node_leave(ni);
			ieee80211_off_channel_resume(vap);
			return;
		}

		IEEE80211_LOCK_IRQ(ic);
		ieee80211_sta_assocs_inc(vap, __func__);
		ieee80211_nonqtn_sta_join(vap, ni, __func__);

		if (IEEE80211_ATH_CAP(vap, ni, IEEE80211_ATHC_TURBOP))
			ic->ic_dt_sta_assoc++;

		if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan))
			ieee80211_node_join_11g(ni);
		IEEE80211_UNLOCK_IRQ(ic);

		newassoc = 1;
	} else {
		newassoc = 0;
	}

	printk(KERN_WARNING "%s: %s %s, tot=%u/%u\n",
		vap->iv_dev->name, ether_sprintf(ni->ni_macaddr),
		ic->ic_ocac.ocac_running ? "connected" : "associated",
		ic->ic_sta_assoc, ic->ic_nonqtn_sta);

	if (ni->ni_authmode != IEEE80211_AUTH_8021X)
		ieee80211_node_authorize(ni);

	IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG, ni,
		"station %sassociated at aid %d: %s preamble, %s slot time"
		"%s%s%s%s%s%s%s",
		newassoc ? "" : "re",
		IEEE80211_NODE_AID(ni),
		(ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) ? "short" : "long",
		ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long",
		ic->ic_flags & IEEE80211_F_USEPROT ? ", protection" : "",
		ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : "",
		IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_TURBOP) ?
			", turbo" : "",
		IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_COMP) ?
			", compression" : "",
		IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_FF) ?
			", fast-frames" : "",
		IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_XR) ? ", XR" : "",
		IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_AR) ? ", AR" : ""
	);

	if(vap->iv_pmf) {
		ni->ni_sa_query_timeout = 0;
	} else {
		/* vap is not PMF capable, reset any flags */
		ni->ni_rsn.rsn_caps &= ~(RSN_CAP_MFP_REQ | RSN_CAP_MFP_CAP);
	}
	/* give driver a chance to set up state like ni_txrate */
	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(ni, newassoc);
	ni->ni_inact_reload = vap->iv_inact_auth;
	ni->ni_inact = ni->ni_inact_reload;

	IEEE80211_SEND_MGMT(ni, resp, IEEE80211_STATUS_SUCCESS);
	/* tell the authenticator about new station */
	if (vap->iv_auth->ia_node_join != NULL)
		vap->iv_auth->ia_node_join(ni);
	indicate_association();

	ieee80211_notify_node_join(ni, newassoc);

	/*For AP mode, record the strat time of association with STA*/
	ni->ni_start_time_assoc = get_jiffies_64();

	/* Update beacon if required */
	if (newassoc) {
		if (vap->interworking)
			beacon_update_required = 1;

		if ((ni->ni_flags & IEEE80211_NODE_HT) != IEEE80211_NODE_HT) {
			if (ic->ic_non_ht_sta == 0 ||
				(ic->ic_flags_ext & IEEE80211_FEXT_ERPUPDATE))
				beacon_update_required = 1;

			ic->ic_non_ht_sta++;
		} else {
			if ((ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) &&
				!(ni->ni_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40)) {

				if(ic->ic_ht_20mhz_only_sta == 0)
					beacon_update_required = 1;

				ic->ic_ht_20mhz_only_sta++;
			}
		}

		if ((ic->ic_peer_rts_mode == IEEE80211_PEER_RTS_PMP) &&
			((ic->ic_sta_assoc - ic->ic_nonqtn_sta) >= IEEE80211_MAX_STA_CCA_ENABLED)) {

			ic->ic_peer_rts = 1;
			TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
				if (tmp_vap->iv_opmode != IEEE80211_M_HOSTAP)
					continue;

				if (tmp_vap->iv_state != IEEE80211_S_RUN)
					continue;

				if (tmp_vap == vap) {
					beacon_update_required = 1;
					continue;
				}

				ic->ic_beacon_update(tmp_vap);
			}
		}
		/* Update beacon */
		if(beacon_update_required) {
			ic->ic_beacon_update(vap);
		}

		ieee80211_pm_queue_work(ic);

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
		if (br_fdb_update_const_hook && vap->iv_dev->br_port) {
			br_fdb_update_const_hook(vap->iv_dev->br_port->br, vap->iv_dev->br_port,
				ni->ni_macaddr, IEEE80211_NODE_IDX_MAP(ni->ni_node_idx));
		}
#endif
	}
}

/*
 * Handle a station leaving an 11g network.
 */
static void
ieee80211_node_leave_11g(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;

	IEEE80211_LOCK_ASSERT(ic);

	KASSERT(IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan),
		("not in 11g, bss %u:0x%x, curmode %u",
		ic->ic_bsschan->ic_freq, ic->ic_bsschan->ic_flags,
		ic->ic_curmode));

	/*
	 * If a long slot station do the slot time bookkeeping.
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0) {
		/* this can be 0 on mode changes from B -> G */
		if (ic->ic_longslotsta > 0)
			ic->ic_longslotsta--;
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
			"long slot time station leaves, count now %d",
			ic->ic_longslotsta);
	}
	/*
	 * If a non-ERP station do the protection-related bookkeeping.
	 */
	if ((ni->ni_flags & IEEE80211_NODE_ERP) == 0) {
		/* this can be 0 on mode changes from B -> G */
		if (ic->ic_nonerpsta > 0)
			ic->ic_nonerpsta--;
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
			"non-ERP station leaves, count now %d", ic->ic_nonerpsta);
		if (IEEE80211_BG_PROTECT_ENABLED(ic) && ic->ic_nonerpsta == 0) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
				"%s: disable use of protection\n", __func__);
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
			/* XXX verify mode? */
			if (ic->ic_caps & IEEE80211_C_SHPREAMBLE) {
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
					"%s: re-enable use of short preamble\n",
					__func__);
				ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
				ic->ic_flags &= ~IEEE80211_F_USEBARKER;
			}
			ic->ic_set_11g_erp(vap, 0);
			ic->ic_flags_ext |= IEEE80211_FEXT_ERPUPDATE;
		}
	}

	if (!ic->ic_longslotsta && !ic->ic_nonerpsta) {
		/*
		 * Re-enable use of short slot time if supported
		 * and not operating in IBSS mode (per spec).
		 */
		if ((ic->ic_caps & IEEE80211_C_SHSLOT) &&
		    vap->iv_opmode != IEEE80211_M_IBSS) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
				"%s: re-enable use of short slot time\n",
				__func__);
			ieee80211_set_shortslottime(ic, 1);
		}
	}
}

static u_int8_t
ieee80211_node_assoc_cleanup(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	u_int8_t beacon_update_required = 0;
	struct ieee80211vap *tmp_vap;

	/*
	 * Tell the authenticator the station is leaving.
	 * Note that we must do this before yanking the
	 * association id as the authenticator uses the
	 * associd to locate its state block.
	 */
	if (vap->iv_auth->ia_node_leave != NULL) {
		vap->iv_auth->ia_node_leave(ni);
	}

	ieee80211_notify_sta_stats(ni);

	IEEE80211_LOCK_IRQ(ic);
	if (IEEE80211_AID(ni->ni_associd) != 0) {
		ieee80211_aid_remove(ni);
		ieee80211_idx_remove(ni);
		if (IEEE80211_NODE_IS_NONE_TDLS(ni)) {
			ieee80211_sta_assocs_dec(vap, __func__);
			if (ic->ic_sta_assoc == 0)
				indicate_disassociation();
		}
		if (vap->iv_opmode == IEEE80211_M_WDS) {
			vap->iv_vapnode_idx = 0;
			ic->ic_wds_links--;
		}
	}

	if ((vap->iv_opmode != IEEE80211_M_WDS) &&
			IEEE80211_NODE_IS_NONE_TDLS(ni)) {
		ieee80211_nonqtn_sta_leave(vap, ni, __func__);
	}

	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) ||
			(vap->iv_opmode == IEEE80211_M_IBSS)) {
		if (vap->interworking)
			beacon_update_required = 1;

		if ((ni->ni_flags & IEEE80211_NODE_HT) != IEEE80211_NODE_HT) {
			ic->ic_non_ht_sta--;
			if (ic->ic_non_ht_sta == 0) {
				beacon_update_required = 1;
			}
		} else {
			if ((ic->ic_htcap.cap  & IEEE80211_HTCAP_C_CHWIDTH40) &&
				!(ni->ni_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40)) {
				ic->ic_ht_20mhz_only_sta--;

				if (ic->ic_ht_20mhz_only_sta == 0) {
					beacon_update_required = 1;
				}
			}
		}
	}

	if ((ic->ic_peer_rts_mode == IEEE80211_PEER_RTS_PMP) &&
		((ic->ic_sta_assoc - ic->ic_nonqtn_sta) < IEEE80211_MAX_STA_CCA_ENABLED)) {

		ic->ic_peer_rts = 0;
		TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
			if (tmp_vap->iv_opmode != IEEE80211_M_HOSTAP)
				continue;

			if (tmp_vap->iv_state != IEEE80211_S_RUN)
				continue;

			if (tmp_vap == vap) {
				beacon_update_required = 1;
				continue;
			}

			ic->ic_beacon_update(tmp_vap);
		}
	}
	if (IEEE80211_ATH_CAP(vap, ni, IEEE80211_ATHC_TURBOP))
		ic->ic_dt_sta_assoc--;

	if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan)) {
		ieee80211_node_leave_11g(ni);
	}

	/* Non-ERP station leaving the network is updated in
	   ieee80211_node_leave_11g, So need to do beacon update here */
	if (ic->ic_flags_ext & IEEE80211_FEXT_ERPUPDATE) {
		beacon_update_required = 1;
	}


	IEEE80211_UNLOCK_IRQ(ic);
	/*
	 * Cleanup station state.  In particular clear various
	 * state that might otherwise be reused if the node
	 * is reused before the reference count goes to zero
	 * (and memory is reclaimed).
	 */
	ieee80211_sta_leave(ni);

	return beacon_update_required;
}

/*
 * Handle bookkeeping for a station/neighbor leaving
 * the bss when operating in ap, wds or adhoc modes.
 */
void
ieee80211_node_leave(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node_table *nt = ni->ni_table;
	u_int8_t beacon_update_required = 0;

	/* remove pending entry in queue */
	ieee80211_ppqueue_remove_node_leave(&vap->iv_ppqueue, ni);
	if ((ic->ic_measure_info.ni == ni) && (ic->ic_measure_info.status == MEAS_STATUS_RUNNING)) {
		 ic->ic_measure_info.status = MEAS_STATUS_DISCRAD;
	}

	/* Tell the qdrv */
	ic->ic_node_auth_state_change(ni, 0);

	ieee80211_scs_clean_stats(ic, IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN, 0);

	IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG, ni,
		"station with aid %d leaves (refcnt %u)",
		IEEE80211_NODE_AID(ni), ieee80211_node_refcnt(ni));

	if (ni->ni_associd != 0 && ((vap->iv_opmode != IEEE80211_M_STA) ||
			!IEEE80211_NODE_IS_NONE_TDLS(ni))) {
		beacon_update_required = ieee80211_node_assoc_cleanup(ni);
	}

	printk(KERN_WARNING "%s: %s %s, tot=%u/%u\n",
		vap->iv_dev->name, ether_sprintf(ni->ni_macaddr),
		ic->ic_ocac.ocac_running ? "disconnected" : "disassociated",
		ic->ic_sta_assoc, ic->ic_nonqtn_sta);

	if (ni == vap->iv_mgmt_retry_ni) {
		vap->iv_mgmt_retry_ni = NULL;
		vap->iv_mgmt_retry_cnt = 0;
	}

	/*
	 * Remove the node from any table it's recorded in and
	 * drop the caller's reference.  Removal from the table
	 * is important to ensure the node is not reprocessed
	 * for inactivity.
	 */
	if (ni->ni_blacklist_timeout == 0) {
		if (nt != NULL) {
			ieee80211_remove_wds_addr(nt,ni->ni_macaddr);
			IEEE80211_NODE_LOCK_IRQ(nt);
			ieee80211_node_reclaim(nt, ni);
			IEEE80211_NODE_UNLOCK_IRQ(nt);
		} else {
			ieee80211_free_node(ni);
		}
	}

	/*
	 * Update beacon
	 * Anyway, check mode before beacon update is good
	 */
	if ((vap->iv_opmode == IEEE80211_M_HOSTAP || vap->iv_opmode == IEEE80211_M_IBSS) &&
		beacon_update_required &&
		(vap->iv_state == IEEE80211_S_RUN)) {
		ic->ic_beacon_update(vap);
	}

	ieee80211_pm_queue_work(ic);
}
EXPORT_SYMBOL(ieee80211_node_leave);

u_int8_t
ieee80211_getrssi(struct ieee80211com *ic)
{
#define	NZ(x)	((x) == 0 ? 1 : (x))
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211vap *vap;
	u_int32_t rssi_samples, rssi_total;
	struct ieee80211_node *ni;

	rssi_total = 0;
	rssi_samples = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:		/* average of all ibss neighbors */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			if (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) {
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(ni);
			}
		break;
	case IEEE80211_M_AHDEMO:	/* average of all neighbors */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
			if (memcmp(ni->ni_vap->iv_myaddr, ni->ni_macaddr,
						IEEE80211_ADDR_LEN)!=0) {
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(ni);
			}
		}
		break;
	case IEEE80211_M_HOSTAP:	/* average of all associated stations */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			if (IEEE80211_AID(ni->ni_associd) != 0) {
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(ni);
			}
		break;
	case IEEE80211_M_MONITOR:	/* XXX */
	case IEEE80211_M_STA:		/* use stats from associated ap */
	default:
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
			if (vap->iv_bss != NULL) {
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(vap->iv_bss);
			}
		break;
	}
	return rssi_total / NZ(rssi_samples);
#undef NZ
}
EXPORT_SYMBOL(ieee80211_getrssi);

void ieee80211_node_tx_ba_set_state(struct ieee80211_node *ni, uint8_t tid, enum ieee80211_ba_state state, unsigned delay)
{
	struct ieee80211_ba_tid *ba = &ni->ni_ba_tx[tid];

	write_seqlock_bh(&ba->state_lock);

	ba->state = state;
	ba->state_deadline = 0;
	if (delay) {
		ba->state_deadline = jiffies + delay;
		if (ba->state_deadline == 0) {
			++ba->state_deadline;
		}
	}

	write_sequnlock_bh(&ba->state_lock);
}
EXPORT_SYMBOL(ieee80211_node_tx_ba_set_state);

int ieee80211_node_is_intel(struct ieee80211_node *ni)
{
	int i;

	if (ni->ni_qtn_flags & QTN_IS_INTEL_NODE) {
		return (1);
	}

	/* check the established TID TX BA timeout, if any one is none zero, it is Intel */
	for (i = 0; i < WME_NUM_TID; i++) {
		if ((ni->ni_ba_tx[i].state == IEEE80211_BA_ESTABLISHED) &&
		    (ni->ni_ba_tx[i].timeout != 0)) {
			ni->ni_qtn_flags |= QTN_IS_INTEL_NODE;
			return (1);
		}
	}

	/* Try to identify problem peers - may cause other peer stations to be blacklisted */
	uint16_t peer_cap = IEEE80211_HTCAP_CAPABILITIES(&ni->ni_ie_htcap);
	if ((ni->ni_qtn_assoc_ie == NULL) && !(ni->ni_qtn_flags & QTN_IS_BCM_NODE) &&
			!(IEEE80211_NODE_IS_VHT(ni)) &&
			(ni->ni_htcap.mpduspacing == 5) &&
			!(peer_cap & IEEE80211_HTCAP_C_TXSTBC) &&
			(peer_cap & IEEE80211_HTCAP_C_SHORTGI20) &&
			((ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM1] & 0x1) == 0x01)) {
		ni->ni_qtn_flags |= QTN_IS_INTEL_NODE;
		return (1);
	}

	struct ieee80211_ie_vhtcap *vhtcap = &ni->ni_ie_vhtcap;
	if ((ni->ni_qtn_assoc_ie == NULL) && (IEEE80211_NODE_IS_VHT(ni)) &&
	    (ni->ni_vendor != PEER_VENDOR_BRCM) && (ni->ni_vendor != PEER_VENDOR_ATH) &&
	    (ni->ni_vendor != PEER_VENDOR_RLNK) && !(IEEE80211_VHTCAP_GET_RXLDPC(vhtcap)) &&
	    !(IEEE80211_VHTCAP_GET_TXSTBC(vhtcap)) && 
	    !(IEEE80211_VHTCAP_GET_HTC_VHT(vhtcap)) &&
	    !(IEEE80211_VHTCAP_GET_RXANTPAT(vhtcap))) {

		ni->ni_qtn_flags |= QTN_IS_INTEL_NODE;
		return (1);
	}

	return (0);
}
EXPORT_SYMBOL(ieee80211_node_is_intel);

int ieee80211_node_is_realtek(struct ieee80211_node *ni)
{
	int ret = 0;

	if ((ni->ni_vendor == PEER_VENDOR_RTK) ||
			(ni->ni_qtn_flags & QTN_IS_REALTEK_NODE)) {
		ret = 1;
	} else if (ni->ni_vendor == PEER_VENDOR_NONE) {
		/*
		 * It is likely that the association request frame from Realtek station
		 * doesn't have vendor IE, so we have to detect Realtek station by its
		 * some properties.
		 * The below judgement logic is for Edimax AC-1200, which is Realtek
		 * 2*2 11ac chip.
		 */
		struct ieee80211_ie_vhtcap *vhtcap = &ni->ni_ie_vhtcap;
		uint32_t ni_oui = (ni->ni_macaddr[2] << 16) | (ni->ni_macaddr[1] << 8) | ni->ni_macaddr[0];

		/* Edimax AC-1200 detection */
		if ((ni_oui == EDIMAX_OUI) &&
				IEEE80211_NODE_IS_VHT(ni) &&
				(ni->ni_htcap.mpduspacing == 0) &&
				(ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_NSS2]) &&
				(ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_NSS3] == 0) &&
				!IEEE80211_VHTCAP_GET_CHANWIDTH(vhtcap) &&
				IEEE80211_VHTCAP_GET_SGI_80MHZ(vhtcap) &&
				IEEE80211_VHTCAP_GET_HTC_VHT(vhtcap) &&
				!IEEE80211_VHTCAP_GET_RXANTPAT(vhtcap) &&
				!IEEE80211_VHTCAP_GET_TXANTPAT(vhtcap)) {
			ret = 1;
		}
	}

	return ret;
}
EXPORT_SYMBOL(ieee80211_node_is_realtek);

int ieee80211_node_is_opti_node(struct ieee80211_node *ni)
{
	struct ieee80211_ie_vhtcap *vhtcap = &ni->ni_ie_vhtcap;
	int ret = 0;

	if ((ni->ni_qtn_assoc_ie == NULL) && (IEEE80211_NODE_IS_VHT(ni)) &&
			(ni->ni_vendor != PEER_VENDOR_BRCM) && (ni->ni_vendor != PEER_VENDOR_ATH) &&
			(ni->ni_vendor != PEER_VENDOR_RLNK) &&
			!(ni->ni_qtn_flags & QTN_IS_REALTEK_NODE) &&
			!(IEEE80211_VHTCAP_GET_RXLDPC(vhtcap)) &&
			!(IEEE80211_VHTCAP_GET_RXSTBC(vhtcap)) &&
			!(IEEE80211_VHTCAP_GET_SU_BEAMFORMER(vhtcap)) &&
			!(IEEE80211_VHTCAP_GET_SU_BEAMFORMEE(vhtcap)))
		ret = 1;

#ifdef WLAN_VW_11N_DETECTION
	if ((ni->ni_qtn_assoc_ie == NULL) && (IEEE80211_NODE_IS_HT(ni)) &&
			!IEEE80211_NODE_IS_VHT(ni) &&
			!(ni->ni_vendor & PEER_VENDOR_MASK) &&
			!(ni->ni_qtn_flags & QTN_IS_REALTEK_NODE) &&
			(ni->ni_raw_bintval == 0xa))
		ret = 1;
#endif

	return ret;
}
EXPORT_SYMBOL(ieee80211_node_is_opti_node);

struct ieee80211_node *ieee80211_find_node_by_aid(struct ieee80211com *ic, uint8_t aid)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	int found = 0;

	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if ((ni->ni_associd & 0x3FFF) == aid) {
			found = 1;
			break;
		}
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	if (found) {
		ieee80211_ref_node(ni);
		return (ni);
	}
	return (NULL);
}
EXPORT_SYMBOL(ieee80211_find_node_by_aid);


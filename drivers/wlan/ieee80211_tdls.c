 /*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2010-2013 Quantenna Communications, Inc.            **
**                            All Rights Reserved                            **
**                                                                           **
**  File        : ieee80211_tdls.c                                           **
**  Description : Tunnelled Direct-Link Setup                                **
**                                                                           **
**  This module implements the IEEE Std 802.11z specification as well as a   **
**  proprietary discovery mechanism.                                         **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************

EH0*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <net/iw_handler.h>

#include "net80211/if_llc.h"
#include "net80211/if_ethersubr.h"
#include "net80211/if_media.h"

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_tdls.h"
#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_dot11_msg.h"
#include "net80211/ieee80211_linux.h"

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
#include <linux/if_bridge.h>
#include "linux/net/bridge/br_private.h"
#endif

#define IEEE80211_TDLS_FRAME_MAX 512
#define IEEE80211_TDLS_MAX_BRIDGE_CLIENTS 256
#define IEEE80211_TDLS_BR_SUB_PORT_SHIFT 4

/*
 * FIXME: Make bridge table extraction more efficient.  Possible improvements:
 * - Support a filter on the fillbuf call so that only the entries we want are
 * returned.  Or perhaps a new, specialised bridge function.
 * - Build an ordered linked list of pointers to bridge entries instead of
 * sorting after the fact. This would be faster and save stack space (only 4
 * bytes per bridge entry), but we'd have to lock out bridge updates while
 * building the IE from the pointers.
 * - Maintain the linked list in the bridge instead of creating it during each
 * call.
 *
 * FIXME: This bridge_entries pointer doesn't belong here, fix with above.
 */
static struct __fdb_entry *bridge_entries;

static const char *ieee80211_tdls_action_name[] = {
	"setup request",
	"setup response",
	"setup confirm",
	"teardown",
	"peer traffic indication",
	"channel switch request",
	"channel switch response",
	"peer PSM request",
	"peer PSM response",
	"peer traffic response",
	"discovery request"
};

static const char *ieee80211_tdls_stats_string[] = {
	"none",
	"inactive",
	"starting",
	"active",
	"idle"
};

static __inline int
bit_num(int32_t val)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (val == 0)
			return i;
		val = val >> 1;
	}

	return i;
}

const char *
ieee80211_tdls_action_name_get(uint8_t action)
{
	if (action >= ARRAY_SIZE(ieee80211_tdls_action_name))
		return "unknown";

	return ieee80211_tdls_action_name[action];
}

const char *
ieee80211_tdls_status_string_get(uint8_t stats)
{
	if (stats >= ARRAY_SIZE(ieee80211_tdls_stats_string))
		return "unknown";

	return ieee80211_tdls_stats_string[stats];
}

/*
 * TDLS is not allowed when using TKIP
 * Returns 0 if the security config is valid, else 1.
 */
static int
ieee80211_tdls_sec_mode_valid(struct ieee80211vap *vap)
{
	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
		if (vap->iv_bss->ni_ucastkey.wk_ciphertype == IEEE80211_CIPHER_TKIP)
			return 0;
	}

	return 1;
}

static int
ieee80211_tdls_get_privacy(struct ieee80211vap *vap)
{
	return vap->iv_bss->ni_ucastkey.wk_ciphertype != IEEE80211_CIPHER_NONE;
}

int
ieee80211_tdls_get_smoothed_rssi(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = vap->iv_ic;
	int32_t rssi = ic->ic_rssi(ni);
	int32_t cur_rssi = 0;
	int32_t cur_smthd_rssi = 0;

	if (rssi < -1 && rssi > -1200)
		cur_rssi = rssi;
	else if (ni->ni_rssi > 0)
		/* Correct pseudo RSSIs that apparently still get into the node table */
		cur_rssi = (ni->ni_rssi * 10) - 900;

	cur_smthd_rssi = ic->ic_smoothed_rssi(ni);
	if ((cur_smthd_rssi > -1) || (cur_smthd_rssi < -1200))
		cur_smthd_rssi = cur_rssi;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Peer: %pM, rssi=%d\n", __func__,
			ni->ni_macaddr, cur_smthd_rssi);

	return cur_smthd_rssi;
}

static void
ieee80211_tdls_update_peer_assoc_bw(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_params *tdls)
{
	struct ieee80211com *ic = peer_ni->ni_vap->iv_ic;
	struct ieee80211_ie_htcap *htcap = (struct ieee80211_ie_htcap *)tdls->htcap;
	struct ieee80211_ie_vhtcap *vhtcap = (struct ieee80211_ie_vhtcap *)tdls->vhtcap;
	enum ieee80211_vhtop_chanwidth assoc_vhtop_bw;
	enum ieee80211_vhtop_chanwidth bss_bw;

	if (htcap && (ic->ic_bss_bw < BW_HT40))
		peer_ni->ni_htcap.cap &= ~(IEEE80211_HTCAP_C_CHWIDTH40 |
				IEEE80211_HTCAP_C_SHORTGI40);

	if (vhtcap) {
		switch (IEEE80211_VHTCAP_GET_CHANWIDTH(vhtcap)) {
		case IEEE80211_VHTCAP_CW_160M:
			assoc_vhtop_bw = IEEE80211_VHTOP_CHAN_WIDTH_160MHZ;
			break;

		case IEEE80211_VHTCAP_CW_160_AND_80P80M:
			assoc_vhtop_bw = IEEE80211_VHTOP_CHAN_WIDTH_80PLUS80MHZ;
			break;

		case IEEE80211_VHTCAP_CW_80M_ONLY:
		default:
			assoc_vhtop_bw = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
			break;
		}

		switch (ic->ic_bss_bw) {
		case BW_HT160:
			bss_bw = IEEE80211_VHTOP_CHAN_WIDTH_160MHZ;
			break;
		case BW_HT80:
			bss_bw = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
			break;
		case BW_HT40:
		case BW_HT20:
		default:
			bss_bw = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
			break;
		}

		assoc_vhtop_bw = min(bss_bw, assoc_vhtop_bw);
		peer_ni->ni_vhtop.chanwidth = min(ic->ic_vhtop.chanwidth, assoc_vhtop_bw);

		if (IS_IEEE80211_11NG_VHT_ENABLED(ic)) {
			assoc_vhtop_bw = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
			assoc_vhtop_bw = min(bss_bw, assoc_vhtop_bw);
			peer_ni->ni_vhtop.chanwidth =
					min(ic->ic_vhtop_24g.chanwidth, assoc_vhtop_bw);
		}
	}
}

static void
ieee80211_tdls_update_rates(struct ieee80211_node *peer_ni,
	struct ieee80211_tdls_params *tdls)
{
	struct ieee80211com *ic;

	if (!peer_ni || !tdls)
		return;

	ic = peer_ni->ni_vap->iv_ic;

	if (tdls->rates)
		ieee80211_parse_rates(peer_ni, tdls->rates, tdls->xrates);
	if (tdls->htcap) {
		peer_ni->ni_flags |= IEEE80211_NODE_HT;
		ieee80211_parse_htcap(peer_ni, tdls->htcap);
		if (tdls->htinfo)
		      ieee80211_parse_htinfo(peer_ni, tdls->htinfo);

		if (IS_IEEE80211_VHT_ENABLED(ic) && tdls->vhtcap) {
			peer_ni->ni_flags |= IEEE80211_NODE_VHT;
			ieee80211_parse_vhtcap(peer_ni, tdls->vhtcap);
			if (tdls->vhtop)
				ieee80211_parse_vhtop(peer_ni, tdls->vhtop);
			else
				ieee80211_tdls_update_peer_assoc_bw(peer_ni, tdls);
		} else {
			peer_ni->ni_flags &= ~IEEE80211_NODE_VHT;
			memset(&peer_ni->ni_vhtcap, 0, sizeof(peer_ni->ni_vhtcap));
			memset(&peer_ni->ni_vhtop, 0, sizeof(peer_ni->ni_vhtop));
		}
	} else {
		memset(&peer_ni->ni_htcap, 0, sizeof(peer_ni->ni_htcap));
		memset(&peer_ni->ni_htinfo, 0, sizeof(peer_ni->ni_htinfo));
		memset(&peer_ni->ni_vhtcap, 0, sizeof(peer_ni->ni_vhtcap));
		memset(&peer_ni->ni_vhtop, 0, sizeof(peer_ni->ni_vhtop));

		peer_ni->ni_flags &= ~IEEE80211_NODE_HT;
		peer_ni->ni_flags &= ~IEEE80211_NODE_VHT;
	}

	ieee80211_fix_rate(peer_ni, IEEE80211_F_DONEGO |
		IEEE80211_F_DOXSECT | IEEE80211_F_DODEL);
	ieee80211_fix_ht_rate(peer_ni, IEEE80211_F_DONEGO |
		IEEE80211_F_DOXSECT | IEEE80211_F_DODEL);
}

int
ieee80211_tdls_set_key(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211_key *wk;
	const uint8_t *mac;
	int error = 0;

	if (ni == NULL)
		return -1;

	mac = ni->ni_macaddr;
	wk = &ni->ni_ucastkey;
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"[%s] Setting key bcast=%u\n", ether_sprintf(mac),
		IEEE80211_IS_MULTICAST(mac));

	if (wk->wk_keylen != 0) {
		ieee80211_key_update_begin(vap);
		error = vap->iv_key_set(vap, wk, mac);
		ieee80211_key_update_end(vap);
	}

	return error;
}

int
ieee80211_tdls_del_key(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211_key *wk;
	const uint8_t *mac;
	int error = 0;

	if (ni == NULL)
		return -1;

	mac = ni->ni_macaddr;
	wk = &ni->ni_ucastkey;
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"[%s] deleting key bcast=%u\n", ether_sprintf(mac),
		IEEE80211_IS_MULTICAST(mac));

	/* wk must be set to ni->ni_ucastkey for sw crypto */
	wk->wk_ciphertype = 0;
	wk->wk_keytsc = 0;
	wk->wk_keylen = sizeof(wk->wk_key);
	memset(wk->wk_key, 0, sizeof(wk->wk_key));

	ieee80211_key_update_begin(vap);
	error = vap->iv_key_delete(vap, wk, mac);
	ieee80211_key_update_end(vap);

	return error;
}

static void
ieee80211_create_tdls_peer(struct ieee80211vap *vap, struct ieee80211_node *peer_ni,
	struct ieee80211_tdls_params *tdls)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = TAILQ_FIRST(&ic->ic_vaps)->iv_bss;

	ieee80211_node_set_chan(ic, peer_ni);

	peer_ni->ni_capinfo = ni->ni_capinfo;
	peer_ni->ni_txpower = ni->ni_txpower;
	peer_ni->ni_ath_flags = vap->iv_ath_cap;
	peer_ni->ni_flags |= (IEEE80211_NODE_AUTH | IEEE80211_NODE_QOS);
	peer_ni->ni_node_type = IEEE80211_NODE_TYPE_TDLS;

	if (tdls && tdls->supp_chan)
		ieee80211_parse_supp_chan(peer_ni, tdls->supp_chan);

	ieee80211_tdls_update_rates(peer_ni, tdls);
	ieee80211_update_current_mode(peer_ni);
	peer_ni->ni_start_time_assoc = get_jiffies_64();
	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(peer_ni, 1);
}

void
ieee80211_tdls_update_uapsd_indicication_windows(struct ieee80211vap *vap)
{
	if (vap->iv_opmode == IEEE80211_M_STA && vap->iv_ic->ic_set_tdls_param)
		vap->iv_ic->ic_set_tdls_param(vap->iv_bss, IOCTL_TDLS_UAPSD_IND_WND,
					(int)vap->tdls_uapsd_indicat_wnd);
}

static void
ieee80211_tdls_update_peer(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = peer_ni->ni_vap;
	int ni_update_required = 0;

	if (tdls == NULL)
		return;

	if ((tdls->act != IEEE80211_ACTION_TDLS_SETUP_REQ) &&
		(tdls->act != IEEE80211_ACTION_TDLS_SETUP_RESP))
		return;

	if (tdls->supp_chan)
		ieee80211_parse_supp_chan(peer_ni, tdls->supp_chan);

	if (tdls->qtn_info) {
		if (peer_ni->ni_qtn_assoc_ie == NULL)
			ni_update_required = 1;
		else if (memcmp(tdls->qtn_info, peer_ni->ni_qtn_assoc_ie,
				sizeof(struct ieee80211_ie_qtn)))
			ni_update_required = 1;
	}

	if (tdls->htcap && memcmp(tdls->htcap, &peer_ni->ni_ie_htcap,
				sizeof(peer_ni->ni_ie_htcap)))
		ni_update_required = 1;
	if (tdls->vhtcap && memcmp(tdls->vhtcap, &peer_ni->ni_ie_vhtcap,
				sizeof(peer_ni->ni_ie_vhtcap)))
		ni_update_required = 1;

	if (ni_update_required) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: update peer %pM, tdls_status: %d, ref_cnt = %d\n", __func__,
			peer_ni->ni_macaddr, peer_ni->tdls_status, ieee80211_node_refcnt(peer_ni));

		ieee80211_tdls_update_rates(peer_ni, tdls);
		ieee80211_input_tdls_qtnie(peer_ni, vap, (struct ieee80211_ie_qtn *)tdls->qtn_info);
		ieee80211_update_current_mode(peer_ni);
	}
}


/*
 * Find or create a peer node
 * The returned node structure must be freed after use
 * Returns a pointer to a node structure if successful, else NULL
 */
static struct ieee80211_node *
ieee80211_tdls_find_or_create_peer(struct ieee80211_node *ni, uint8_t *peer_mac,
		struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *peer_ni = NULL;

	peer_ni = ieee80211_find_node(&ic->ic_sta, peer_mac);
	if (peer_ni == NULL) {
		peer_ni = ieee80211_alloc_node(&ic->ic_sta, vap, peer_mac, "TDLS peer");
		if (peer_ni == NULL) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: could not create peer node %pM\n",
				__func__, peer_ni->ni_macaddr);
			return NULL;
		}

		peer_ni->tdls_status = IEEE80211_TDLS_NODE_STATUS_INACTIVE;
		if (ieee80211_aid_acquire(ic, peer_ni)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: could not create peer node %pM"
				" - too many nodes\n",
				__func__, peer_ni->ni_macaddr);
			ieee80211_node_decref(peer_ni);
			ieee80211_free_node(peer_ni);
			return NULL;
		}

		IEEE80211_ADDR_COPY(peer_ni->ni_bssid, ni->ni_bssid);

		if (tdls && tdls->qtn_info != NULL)
			ieee80211_input_tdls_qtnie(peer_ni, vap,
						(struct ieee80211_ie_qtn *)tdls->qtn_info);

		/* Add node to macfw iv_aid_ni table */
		ieee80211_create_tdls_peer(vap, peer_ni, tdls);

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: created peer node %pM aid = %d, ref_cnt = %d\n", __func__,
			peer_ni->ni_macaddr, IEEE80211_NODE_AID(peer_ni), ieee80211_node_refcnt(peer_ni));
	} else {
		ieee80211_tdls_update_peer(peer_ni, tdls);
	}

	return peer_ni;
}

int
ieee80211_tdls_send_event(struct ieee80211_node *peer_ni,
		enum ieee80211_tdls_event event, void *data)
{
	struct ieee80211vap *vap = peer_ni->ni_vap;
	struct ieee80211_tdls_event_data event_data;
	union iwreq_data wreq;
	char *event_name = NULL;

	switch (event) {
	case IEEE80211_EVENT_TDLS:
		event_name = "EVENT_TDLS";
		break;
	case IEEE80211_EVENT_STATION_LOW_ACK:
		event_name = "EVENT_STA_LOW_ACK";
		break;
	default:
		break;
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS:%s Send event %s, run operation %d\n", __func__,
		event_name, *((enum ieee80211_tdls_operation *)data));

	memset(&event_data, 0, sizeof(event_data));
	strncpy(event_data.name, event_name, sizeof(event_data.name));
	event_data.index = event;
	event_data.sub_index = *((enum ieee80211_tdls_operation *)data);
	memcpy(event_data.peer_mac, peer_ni->ni_macaddr, IEEE80211_ADDR_LEN);

	memset(&wreq, 0, sizeof(wreq));
	wreq.data.length = sizeof(event_data);
	wireless_send_event(vap->iv_dev, IWEVCUSTOM, &wreq, (char *)&event_data);

	return 0;
}
EXPORT_SYMBOL(ieee80211_tdls_send_event);

static struct tdls_peer_ps_info *
ieee80211_tdls_find_or_create_peer_ps_info(struct ieee80211vap *vap,
	struct ieee80211_node *peer_ni)
{
	int found = 0;
	unsigned long flags;
	struct tdls_peer_ps_info *peer_ps_info = NULL;
	int hash = IEEE80211_NODE_HASH(peer_ni->ni_macaddr);

	spin_lock_irqsave(&vap->tdls_ps_lock, flags);
	LIST_FOREACH(peer_ps_info, &vap->tdls_ps_hash[hash], peer_hash) {
		if (IEEE80211_ADDR_EQ(peer_ps_info->peer_addr, peer_ni->ni_macaddr)) {
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&vap->tdls_ps_lock, flags);

	if (found == 1)
		return peer_ps_info;

	MALLOC(peer_ps_info, struct tdls_peer_ps_info *,
				sizeof(*peer_ps_info), M_DEVBUF, M_WAITOK);
	if (peer_ps_info != NULL) {
		memcpy(peer_ps_info->peer_addr, peer_ni->ni_macaddr, IEEE80211_ADDR_LEN);
		peer_ps_info->tdls_path_down_cnt = 0;
		peer_ps_info->tdls_link_disabled_ints = 0;
		spin_lock_irqsave(&vap->tdls_ps_lock, flags);
		LIST_INSERT_HEAD(&vap->tdls_ps_hash[hash], peer_ps_info, peer_hash);
		spin_unlock_irqrestore(&vap->tdls_ps_lock, flags);
	}

	return peer_ps_info;
}

static void
ieee80211_tdls_peer_ps_info_decre(struct ieee80211vap *vap)
{
	int i;
	unsigned long flags;
	struct tdls_peer_ps_info *peer_ps_info = NULL;

	spin_lock_irqsave(&vap->tdls_ps_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH(peer_ps_info, &vap->tdls_ps_hash[i], peer_hash) {
			if ((peer_ps_info != NULL) &&
					(peer_ps_info->tdls_link_disabled_ints > 0))
				peer_ps_info->tdls_link_disabled_ints--;
		}
	}
	spin_unlock_irqrestore(&vap->tdls_ps_lock, flags);
}

void
ieee80211_tdls_free_peer_ps_info(struct ieee80211vap *vap)
{
	int i;
	unsigned long flags;
	struct tdls_peer_ps_info *peer_ps_info = NULL;

	spin_lock_irqsave(&vap->tdls_ps_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH(peer_ps_info, &vap->tdls_ps_hash[i], peer_hash) {
			if (peer_ps_info != NULL) {
				LIST_REMOVE(peer_ps_info, peer_hash);
				FREE(peer_ps_info, M_DEVBUF);
			}
		}
	}
	spin_unlock_irqrestore(&vap->tdls_ps_lock, flags);
}

static int
ieee80211_tdls_state_should_move(struct ieee80211vap *vap,
	struct ieee80211_node *peer_ni)
{
	int mu = STATS_SU;
#define QTN_TDLS_RATE_CHANGE_THRSH	100
	struct tdls_peer_ps_info *peer_ps_info = NULL;
	int32_t cur_ap_tx_rate = vap->iv_bss->ni_shared_stats->tx[mu].avg_tx_phy_rate;
	int32_t last_ap_tx_rate = vap->iv_bss->last_tx_phy_rate;
	int32_t ap_rate_diff;
	int should_move = 1;

	peer_ps_info = ieee80211_tdls_find_or_create_peer_ps_info(vap, peer_ni);
	if ((peer_ps_info) && (peer_ps_info->tdls_link_disabled_ints > 0)) {
		if (cur_ap_tx_rate > last_ap_tx_rate)
			ap_rate_diff = cur_ap_tx_rate - last_ap_tx_rate;
		else
			ap_rate_diff = last_ap_tx_rate - cur_ap_tx_rate;

		if (ap_rate_diff > QTN_TDLS_RATE_CHANGE_THRSH) {
			peer_ps_info->tdls_path_down_cnt = 0;
			peer_ps_info->tdls_link_disabled_ints = 0;
		} else {
			should_move = 0;
		}
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: Peer: %pM link_disabled_ints = %d, "
		"should %s move to next state\n", __func__, peer_ni->ni_macaddr,
		(peer_ps_info == NULL) ? 0 : peer_ps_info->tdls_link_disabled_ints,
		should_move ? "" : "not");

	return should_move;
}

/*
 * Decide if should try to setup TDLS link
 * Return 0: shouldn't setup
 * Return 1: setup TDLS link
 */
int
ieee80211_tdls_link_should_setup(struct ieee80211vap *vap,
	struct ieee80211_node *peer_ni)
{
	int should_setup = 1;
	int32_t smthd_rssi = ieee80211_tdls_get_smoothed_rssi(vap, peer_ni);

	if (IEEE80211_NODE_IS_TDLS_ACTIVE(peer_ni)) {
		should_setup = 0;
		goto OUT;
	}

	if (vap->tdls_path_sel_prohibited == 1)
		goto OUT;
	/*
	 * if peer is 3rd-party STA, we will establish TDLS Link first, then send trainning packet,
	 * otherwise, training packets will be treated as attacking packets by 3rd-party STA,
	 * and it send deauth frame to QTN-STA, the result is not what we exspect.
	 */
	if ((vap->tdls_discovery_interval > 0) && peer_ni->ni_qtn_assoc_ie) {
		should_setup = 0;
		goto OUT;
	}

	if (smthd_rssi < vap->tdls_min_valid_rssi) {
		should_setup = 0;
		goto OUT;
	}

	if (timer_pending(&vap->tdls_rate_detect_timer))
		should_setup = ieee80211_tdls_state_should_move(vap, peer_ni);

OUT:
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: %s peer: %pM RSSI = %d, should %s setup TDLS link\n",
		__func__, peer_ni->ni_qtn_assoc_ie ? "qtn" : "non-qtn",
		peer_ni->ni_macaddr, smthd_rssi, should_setup ? "" : "not");

	return should_setup;
}

int
ieee80211_tdls_link_should_response(struct ieee80211vap *vap,
	struct ieee80211_node *peer_ni)
{
	int should_resp = 1;
	int32_t smthd_rssi = ieee80211_tdls_get_smoothed_rssi(vap, peer_ni);

	if (vap->tdls_path_sel_prohibited == 1)
		goto OUT;

	if (smthd_rssi < vap->tdls_min_valid_rssi) {
		should_resp = 0;
		goto OUT;
	}

	if (timer_pending(&vap->tdls_rate_detect_timer))
		should_resp = ieee80211_tdls_state_should_move(vap, peer_ni);

OUT:
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: Peer: %pM RSSI = %d, should %s send response frame\n",
		__func__, peer_ni->ni_macaddr, smthd_rssi, should_resp ? "" : "not");

	return should_resp;
}

/*
 * Decide if TDLS link should be torn down or established
 * Return 0: teardown TDLS link
 * Return 1: establish TDLS link
 */
static int
ieee80211_tdls_data_path_selection(struct ieee80211vap *vap,
	struct ieee80211_node *peer_ni)
{
	int mu = STATS_SU;
	int32_t last_ap_tx_rate = vap->iv_bss->last_tx_phy_rate;
	int32_t last_peer_tx_rate = peer_ni->last_tx_phy_rate;
	int32_t avg_ap_tx_rate = 0;
	int32_t avg_peer_tx_rate = 0;
	int32_t smthd_rssi = 0;
	int is_tdls_path = -1;
	int use_tdls_path = 0;
	int32_t cur_ap_tx_rate = vap->iv_bss->ni_shared_stats->tx[mu].avg_tx_phy_rate;
	int32_t cur_peer_tx_rate = peer_ni->ni_shared_stats->tx[mu].avg_tx_phy_rate;

	if (vap->tdls_path_sel_prohibited == 1) {
		is_tdls_path = 1;
		goto out;
	}

	if (last_ap_tx_rate <= 0)
		last_ap_tx_rate = cur_ap_tx_rate;
	avg_ap_tx_rate = last_ap_tx_rate * vap->tdls_phy_rate_wgt / 10 +
				cur_ap_tx_rate * (10 - vap->tdls_phy_rate_wgt) / 10;
	if (last_peer_tx_rate <= 0)
		last_peer_tx_rate = cur_peer_tx_rate;
	avg_peer_tx_rate = last_peer_tx_rate * vap->tdls_phy_rate_wgt / 10 +
				cur_peer_tx_rate * (10 - vap->tdls_phy_rate_wgt) / 10;

	smthd_rssi = ieee80211_tdls_get_smoothed_rssi(vap, peer_ni);
	if (smthd_rssi >= vap->tdls_min_valid_rssi) {
		if ((vap->iv_bss->ni_training_flag == NI_TRAINING_END) &&
				(peer_ni->ni_training_flag == NI_TRAINING_END)) {
			/*
			 * Use the TDLS path if the Tx rate is better than a predefined proportion
			 * of the Tx rate via the AP.  E.g. if the weighting is 8, then the direct
			 * rate must be at least 80% of the rate via the AP.
			 */
			if ((avg_peer_tx_rate > vap->tdls_path_sel_rate_thrshld) &&
					(avg_peer_tx_rate >= avg_ap_tx_rate * vap->tdls_path_sel_weight / 10))
				use_tdls_path = 1;
			else
				use_tdls_path = 0;
		} else {
			use_tdls_path = 1;
		}
	} else {
		use_tdls_path = 0;
	}

	if (use_tdls_path != peer_ni->tdls_last_path_sel)
		peer_ni->tdls_path_sel_num = 0;

	if (use_tdls_path == 0)
		peer_ni->tdls_path_sel_num--;
	else
		peer_ni->tdls_path_sel_num++;

	if (peer_ni->tdls_path_sel_num >= vap->tdls_switch_ints)
		is_tdls_path = 1;
	else if (peer_ni->tdls_path_sel_num <= (0 - vap->tdls_switch_ints))
		is_tdls_path = 0;

	vap->iv_bss->last_tx_phy_rate = avg_ap_tx_rate;
	peer_ni->last_tx_phy_rate = avg_peer_tx_rate;
	peer_ni->tdls_last_path_sel =  use_tdls_path;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: peer %pM rssi=%d, path_sel_num=%d, avg_ap_rate=%d, avg_peer_rate=%d\n",
		__func__, peer_ni->ni_macaddr, smthd_rssi, peer_ni->tdls_path_sel_num,
		avg_ap_tx_rate, avg_peer_tx_rate);

out:
	if (is_tdls_path == 1)
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Creating TDLS data link with %pM\n",
			__func__, peer_ni->ni_macaddr);
	else if (is_tdls_path == 0)
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Tearing down TDLS data link with %pM\n",
			__func__, peer_ni->ni_macaddr);

	return is_tdls_path;
}

int ieee80211_tdls_remain_on_channel(struct ieee80211vap *vap,
		struct ieee80211_node *ni, uint8_t chan, uint8_t bandwidth,
		uint64_t start_tsf, uint32_t duration)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *newchan = NULL;
	int ret = 0;

	if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Channel switch already in progress, owner=%d\n",
			__func__, ic->ic_csa_reason);
		return -1;
	}

	newchan = ic->ic_findchannel(ic, chan, ic->ic_des_mode);
	if (newchan == NULL) {
		newchan = ic->ic_findchannel(ic, chan, IEEE80211_MODE_AUTO);
		if (newchan == NULL) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS,
				IEEE80211_TDLS_MSG_DBG, "TDLS %s: Fail to find target channel\n", __func__);
			return -1;
		}
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Start to switch channel %d (bw:%d), start_tsf: %llu, duration: %u\n",
			__func__, chan, bandwidth, start_tsf, duration);

	ret = ic->ic_remain_on_channel(ic, ni, newchan, bandwidth, start_tsf, duration, 0);
	if (!ret)
		vap->tdls_cs_node = ni;

	return ret;
}

/*
 * Initialise an action frame
 */
static struct sk_buff *
ieee80211_tdls_init_frame(struct ieee80211_node *ni, uint8_t **frm_p,
		uint8_t action, uint8_t direct)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ether_header *eh;

	struct sk_buff *skb;
	uint8_t *frm = *frm_p;
	uint8_t payload_type = IEEE80211_SNAP_TYPE_TDLS;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_NODE, IEEE80211_TDLS_MSG_DBG,
		"%s: Sending %s\n", __func__, ieee80211_tdls_action_name_get(action));

	skb = dev_alloc_skb(frm_len);
	if (skb == NULL) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_NODE, IEEE80211_TDLS_MSG_WARN,
			"%s: cannot get buf; size %u", __func__, frm_len);
		vap->iv_stats.is_tx_nobuf++;
		return NULL;
	}
	frm = skb_put(skb, frm_len);

	skb->priority = WME_AC_VI;	/* "unless specified otherwise" 11.21.2 */
	M_FLAG_SET(skb, M_CLASSIFY);

	eh = (struct ether_header *)frm;
	if (direct) {
		IEEE80211_ADDR_COPY(eh->ether_dhost, ni->ni_macaddr);
		IEEE80211_ADDR_COPY(eh->ether_shost, vap->iv_myaddr);
	} else {
		IEEE80211_ADDR_COPY(eh->ether_dhost, vap->iv_dev->broadcast);
		IEEE80211_ADDR_COPY(eh->ether_shost, vap->iv_myaddr);
	}
	eh->ether_type = htons(ETHERTYPE_80211MGT);
	frm += ETHER_HDR_LEN;

	*frm++ = payload_type;
	*frm++ = IEEE80211_ACTION_CAT_TDLS;
	*frm++ = action;

	*frm_p = frm;

	return skb;
}

static int
ieee80211_tdls_over_qhop_enabled(struct ieee80211vap *vap)
{
	uint8_t ext_role = vap->iv_bss->ni_ext_role;

	return (vap->tdls_over_qhop_en &&
			(ext_role != IEEE80211_EXTENDER_ROLE_NONE));
}

static int
ieee80211_tdls_ext_bssid_allowed(struct ieee80211vap *vap, u8 *bssid)
{
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211_qtn_ext_bssid *ext_bssid;
	int i;

	if (!ni || !ni->ni_ext_bssid_ie)
		return 0;

	ext_bssid = (struct ieee80211_qtn_ext_bssid *)ni->ni_ext_bssid_ie;

	if (!is_zero_ether_addr(ext_bssid->mbs_bssid) &&
			IEEE80211_ADDR_EQ(ext_bssid->mbs_bssid, bssid)) {
		return 1;
	} else {
		for (i = 0; i < QTN_MAX_RBS_NUM; i++) {
			if (!is_zero_ether_addr(ext_bssid->rbs_bssid[i]) &&
					IEEE80211_ADDR_EQ(ext_bssid->rbs_bssid[i], bssid))
				return 1;
		}
	}

	return 0;
}

static int
ieee80211_tdls_copy_link_id(struct ieee80211_node *ni, uint8_t **frm,
			struct ieee80211_tdls_action_data *data)
{
	uint8_t *ie;
	uint8_t *ie_end;

	if (!ni || !frm || !data)
		return 0;

	ie = data->ie_buf;
	ie_end = ie + data->ie_buflen;
	while (ie < ie_end) {
		switch (*ie) {
		case IEEE80211_ELEMID_TDLS_LINK_ID:
			memcpy(*frm, ie, ie[1] + 2);
			*frm += ie[1] + 2;
			return 1;
		default:
			break;
		}
		ie += ie[1] + 2;
	}

	return 0;
}

static int
ieee80211_tdls_add_tlv_link_id(struct ieee80211_node *ni,
	struct sk_buff *skb, uint8_t action, uint8_t **frm, uint8_t *da,
	struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_tdls_link_id *link_id;

	if ((*frm + sizeof(*link_id)) > (skb->data + skb->len)) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ELEMID,
			"[%s] TDLS %s frame is too big\n", vap->iv_dev->name,
			ieee80211_tdls_action_name_get(action));
		vap->iv_stats.is_rx_elem_toosmall++;
		return 1;
	}

	if (!ieee80211_tdls_copy_link_id(ni, frm, data)) {
		link_id = (struct ieee80211_tdls_link_id *)*frm;
		link_id->id = IEEE80211_ELEMID_TDLS_LINK_ID;
		link_id->len = sizeof(*link_id) - 2;
		IEEE80211_ADDR_COPY(link_id->bssid, ni->ni_bssid);
		switch (action) {
		case IEEE80211_ACTION_TDLS_DISC_REQ:
			IEEE80211_ADDR_COPY(link_id->init_sa, vap->iv_myaddr);
			IEEE80211_ADDR_COPY(link_id->resp_sa, da);
			break;
		case IEEE80211_ACTION_PUB_TDLS_DISC_RESP:
			IEEE80211_ADDR_COPY(link_id->init_sa, da);
			IEEE80211_ADDR_COPY(link_id->resp_sa, vap->iv_myaddr);
			break;
		case IEEE80211_ACTION_TDLS_SETUP_REQ:
		case IEEE80211_ACTION_TDLS_SETUP_RESP:
		case IEEE80211_ACTION_TDLS_SETUP_CONFIRM:
		case IEEE80211_ACTION_TDLS_TEARDOWN:
		case IEEE80211_ACTION_TDLS_PTI:
		case IEEE80211_ACTION_TDLS_PEER_TRAF_RESP:
		case IEEE80211_ACTION_TDLS_CS_REQ:
		case IEEE80211_ACTION_TDLS_CS_RESP:
			/*
			 * tdls_initiator means who starts to setup TDLS link firstly.
			 * 1 indicates our own peer setups TDLS link.
			 * 0 indicates the other peer setups TDLS link.
			 */
			if (ni->tdls_initiator) {
				IEEE80211_ADDR_COPY(link_id->init_sa, vap->iv_myaddr);
				IEEE80211_ADDR_COPY(link_id->resp_sa, da);
			} else {
				IEEE80211_ADDR_COPY(link_id->init_sa, da);
				IEEE80211_ADDR_COPY(link_id->resp_sa, vap->iv_myaddr);
			}
			break;
		}

		*frm += sizeof(*link_id);
	}

	return 0;
}

static int
ieee80211_tdls_add_cap(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *cap_p = *frm;
	uint16_t capinfo = 0;

	if (vap->iv_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (vap->iv_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		IEEE80211_IS_CHAN_2GHZ(ic->ic_bsschan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	if (ic->ic_flags & IEEE80211_F_DOTH)
		capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
	*(__le16 *)cap_p = htole16(capinfo);
	*frm += 2;

	return 0;
}

static int
ieee80211_tdls_add_tlv_rates(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t mode = ic->ic_curmode;
	struct ieee80211_rateset *rs = &ic->ic_sup_rates[mode];
	uint8_t *ie = *frm;
	int nrates;

	*ie++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_legacy_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*ie++ = nrates;
	memcpy(ie, rs->rs_rates, nrates);
	*frm += nrates + 2;

	return 0;
}

static int
ieee80211_tdls_add_tlv_country(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *ie = *frm;

	if (ic->ic_country_ie.country_len > 0) {
		memcpy(ie, (uint8_t *)&ic->ic_country_ie,
			ic->ic_country_ie.country_len + 2);
		*frm += ic->ic_country_ie.country_len + 2;
	}

	return 0;
}

static int
ieee80211_tdls_add_tlv_xrates(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t mode = ic->ic_curmode;
	struct ieee80211_rateset *rs = &ic->ic_sup_rates[mode];
	uint8_t *ie = *frm;
	int nrates = 0;

	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		nrates = rs->rs_legacy_nrates - IEEE80211_RATE_SIZE;
		if (nrates) {
			*ie++ = IEEE80211_ELEMID_XRATES;
			*ie++ = nrates;
			memcpy(ie, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
			*frm += nrates + 2;
		}
	}

	return 0;
}

static int
ieee80211_tdls_add_tlv_rsn(struct ieee80211_node *ni, uint8_t **frm,
			struct ieee80211_tdls_action_data *data)
{
	uint8_t *ie;
	uint8_t *ie_end;

	if ((!ni) || (!frm) || (!data))
		return 1;

	ie = data->ie_buf;
	ie_end = ie + data->ie_buflen;
	while (ie < ie_end) {
		switch (*ie) {
		case IEEE80211_ELEMID_RSN:
			memcpy(*frm, ie, ie[1] + 2);
			*frm += ie[1] + 2;
			break;
		default:
			break;
		}
		ie += ie[1] + 2;
	}

	return 0;
}

static int
ieee80211_tdls_add_tlv_ext_cap(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	uint8_t *excap_p = *frm;
	uint32_t excapinfo[2] = {0, 0};

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_PROHIB)
		excapinfo[1] |= IEEE80211_EXTCAP2_TDLS_PROHIB;
	else
		excapinfo[1] |= IEEE80211_EXTCAP2_TDLS;
	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB)
		excapinfo[1] |= IEEE80211_EXTCAP2_TDLS_CS_PROHIB;
	else
		excapinfo[0] |= IEEE80211_EXTCAP1_TDLS_CS;

	excapinfo[0] |= IEEE80211_EXTCAP1_TDLS_UAPSD;
	*excap_p++ = IEEE80211_ELEMID_EXTCAP;
	*excap_p++ = IEEE8211_EXTCAP_LENGTH;
	*(__le32 *)excap_p = htole32(excapinfo[0]);
	excap_p += 4;
	*(__le32 *)excap_p = htole32(excapinfo[1]);

	*frm += IEEE8211_EXTCAP_LENGTH + 2;

	return 0;
}

static int
ieee80211_tdls_add_tlv_qos_cap(struct ieee80211_node *ni, uint8_t **frm)
{
	static const u_int8_t oui[3] = {0x00, 0x50, 0xf2};
	struct ieee80211_ie_wme *ie = (struct ieee80211_ie_wme *) *frm;

	ie->wme_id = IEEE80211_ELEMID_VENDOR;
	ie->wme_len = sizeof(*ie) - 2;
	memcpy(ie->wme_oui,oui,sizeof(oui));
	ie->wme_type = WME_OUI_TYPE;
	ie->wme_subtype = WME_INFO_OUI_SUBTYPE;
	ie->wme_version = WME_VERSION;
	ie->wme_info = 0;
	ie->wme_info |= WME_UAPSD_MASK;

	*frm += sizeof(*ie);

	return 0;
}

static int
ieee80211_tdls_add_tlv_edca_param(struct ieee80211_node *ni, uint8_t **frm)
{
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
	} while (0)
	static const u_int8_t oui[3] = {0x00, 0x50, 0xf2};
	struct ieee80211_wme_param *ie = (struct ieee80211_wme_param *) *frm;
	struct ieee80211_wme_state *wme = &ni->ni_ic->ic_wme;
	struct ieee80211vap *vap = ni->ni_vap;
	u_int8_t *frame_p = NULL;
	int i;

	ie->param_id = IEEE80211_ELEMID_VENDOR;
	ie->param_len = sizeof(*ie) - 2;
	memcpy(ie->param_oui,oui,sizeof(oui));
	ie->param_oui_type = WME_OUI_TYPE;
	ie->param_oui_sybtype = WME_PARAM_OUI_SUBTYPE;
	ie->param_version = WME_VERSION;
	ie->param_qosInfo = 0;
	ie->param_qosInfo |= WME_UAPSD_MASK;
	ie->param_reserved = 0;

	frame_p = *frm + 10;
	for(i = 0; i < WME_NUM_AC; i++) {
		const struct wmm_params *ac;

		ac = &wme->wme_bssChanParams.cap_wmeParams[i];

		*frame_p++ = SM(i, WME_PARAM_ACI) |
			SM(ac->wmm_acm, WME_PARAM_ACM) |
			SM(ac->wmm_aifsn, WME_PARAM_AIFSN);
		*frame_p++ = SM(ac->wmm_logcwmax, WME_PARAM_LOGCWMAX) |
			SM(ac->wmm_logcwmin, WME_PARAM_LOGCWMIN);
		ADDSHORT(frame_p, ac->wmm_txopLimit);
	}

	*frm += sizeof(*ie);
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"ieee80211_tdls_add_tlv_edca_param: add edca parameter qos info"
			" is [%02x] reserve is [%02x]\n", ie->param_qosInfo, ie->param_reserved);
	return 0;
#undef ADDSHORT
}

static int
ieee80211_tdls_add_tlv_ftie(struct ieee80211_node *ni, uint8_t **frm,
			struct ieee80211_tdls_action_data *data)
{
	uint8_t *ie;
	uint8_t *ie_end;

	if ((!ni) || (!frm) || (!data))
		return 1;

	ie = data->ie_buf;
	ie_end = ie + data->ie_buflen;
	while (ie < ie_end) {
		switch (*ie) {
		case IEEE80211_ELEMID_FTIE:
			memcpy(*frm, ie, ie[1] + 2);
			*frm += ie[1] + 2;
			break;
		default:
			break;
		}
		ie += ie[1] + 2;
	}

	return 0;
}

static int
ieee80211_tdls_add_tlv_tpk_timeout(struct ieee80211_node *ni, uint8_t **frm,
			struct ieee80211_tdls_action_data *data)
{
	uint8_t *ie;
	uint8_t *ie_end;

	if ((!ni) || (!frm) || (!data))
		return 1;

	ie = data->ie_buf;
	ie_end = ie + data->ie_buflen;
	while (ie < ie_end) {
		switch (*ie) {
		case IEEE80211_ELEMID_TIMEOUT_INT:
			memcpy(*frm, ie, ie[1] + 2);
			*frm += ie[1] + 2;
			break;
		default:
			break;
		}
		ie += ie[1] + 2;
	}

	return 0;

}

static int
ieee80211_tdls_add_tlv_sup_reg_class(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *ie = *frm;
	uint8_t *ie_len = *frm + 1;
	uint8_t cur_reg_class;
	int bandwidth;
	int i;

	bandwidth = ieee80211_get_bw(ic);
	cur_reg_class = ieee80211_get_current_operating_class(ic->ic_country_code,
			ic->ic_bsschan->ic_ieee, bandwidth);

	*ie++ = IEEE80211_ELEMID_REG_CLASSES;
	*ie++ = 1;
	*ie++ = cur_reg_class;

	for (i = 0; i < IEEE80211_OPER_CLASS_MAX; i++) {
		if (isset(ic->ic_oper_class, i)) {
			*ie++ = i;
			(*ie_len)++;
		}
	}

	*frm += *ie_len + 2;

	return 0;
}

static int
ieee80211_tdls_add_tlv_ht_cap(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_htcap htcap;

	if (!IS_IEEE80211_11NA(ic) && !IS_IEEE80211_11NG(ic) &&
			!IS_IEEE80211_VHT_ENABLED(ic))
		return 0;

	memcpy(&htcap, &ic->ic_htcap, sizeof(htcap));
	if (ic->ic_bss_bw < BW_HT40)
		htcap.cap &= ~(IEEE80211_HTCAP_C_CHWIDTH40 |
					IEEE80211_HTCAP_C_SHORTGI40);

	*frm = ieee80211_add_htcap(ni, *frm, &htcap,
				IEEE80211_FC0_SUBTYPE_ACTION);

	return 0;
}

static int
ieee80211_tdls_add_tlv_ht_oper(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211com *ic = ni->ni_vap->iv_ic;
	struct ieee80211_htinfo htinfo;
	int16_t htinfo_channel_width = 0;
	int16_t htinfo_2nd_channel_offset = 0;

	if (!IS_IEEE80211_11NA(ic) && !IS_IEEE80211_11NG(ic) &&
			!IS_IEEE80211_VHT_ENABLED(ic))
		return 0;

	memcpy(&htinfo, &ic->ic_htinfo, sizeof(htinfo));
	ieee80211_get_channel_bw_offset(ic, &htinfo_channel_width,
			&htinfo_2nd_channel_offset);

	if (IEEE80211_IS_CHAN_ANYN(ic->ic_bsschan) &&
			(ic->ic_curmode >= IEEE80211_MODE_11NA)) {
		htinfo.ctrlchannel = ieee80211_chan2ieee(ic, ic->ic_bsschan);
		if (ic->ic_bss_bw >= BW_HT40) {
			htinfo.byte1 |= (htinfo_channel_width ?
				IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40 : 0x0);
			htinfo.choffset = htinfo_2nd_channel_offset;
		} else {
			htinfo.byte1 &= ~IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40;
			htinfo.choffset = IEEE80211_HTINFO_CHOFF_SCN;
		}
	}

	*frm = ieee80211_add_htinfo(ni, *frm, &htinfo);

	return 0;
}

static int
ieee80211_tdls_add_tlv_bss_2040_coex(struct ieee80211_node *ni, u_int8_t **frm)
{
	uint8_t *ie = (uint8_t *)(*frm);

	ie[0] = IEEE80211_ELEMID_20_40_BSS_COEX;
	ie[1] = 1;
	ie[2] = IEEE80211_2040BSSCOEX_INFO_REQ;

	*frm += 3;

	return 0;
}

static void
ieee80211_tdls_add_sec_chan_off(uint8_t **frm,
		struct ieee80211vap *vap, uint8_t csa_chan)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	uint8_t sec_position = IEEE80211_HTINFO_EXTOFFSET_NA;
	struct ieee80211_ie_sec_chan_off *sco = (struct ieee80211_ie_sec_chan_off *)(*frm);
	chan = ieee80211_find_channel_by_ieee(ic, csa_chan);

	if (!chan) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: failed to find the target channel %u\n", __func__, csa_chan);
		return;
	}

	if (chan->ic_flags & IEEE80211_CHAN_HT40D)
		sec_position = IEEE80211_HTINFO_EXTOFFSET_BELOW;
	else if (chan->ic_flags & IEEE80211_CHAN_HT40U)
		sec_position = IEEE80211_HTINFO_EXTOFFSET_ABOVE;

	sco->sco_id = IEEE80211_ELEMID_SEC_CHAN_OFF;
	sco->sco_len = 1;
	sco->sco_off = sec_position;

	*frm += sizeof(struct ieee80211_ie_sec_chan_off);

	return;
}

static int
ieee80211_tdls_add_tlv_2nd_chan_off(struct ieee80211_node *ni,
                u_int8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *newchan = NULL;

	if (vap->tdls_off_chan_bw < BW_HT40)
		return 0;

	newchan = ic->ic_findchannel(ic, vap->tdls_target_chan, IEEE80211_MODE_AUTO);
	if (newchan == NULL) {
	    IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
	            "TDLS %s: failed to find the target channel %u\n",
	            __func__, vap->tdls_target_chan);
	    return -1;
	}

	ieee80211_tdls_add_sec_chan_off(frm, vap, newchan->ic_ieee);

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
	                "%s: Add second channel offset IE\n", __func__);

	return 0;
}

static int
ieee80211_tdls_add_tlv_vhtcap(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211com *ic = ni->ni_vap->iv_ic;

	if (!IS_IEEE80211_VHT_ENABLED(ic))
		return 0;

	*frm = ieee80211_add_vhtcap(ni, *frm, &ic->ic_vhtcap,
				IEEE80211_FC0_SUBTYPE_ACTION);

	return 0;
}

static int
ieee80211_tdls_add_tlv_vhtop(struct ieee80211_node *ni, uint8_t **frm)
{
	struct ieee80211com *ic = ni->ni_vap->iv_ic;
	struct ieee80211_vhtop vhtop;
	uint8_t bw = BW_HT20;

	if (!IS_IEEE80211_VHT_ENABLED(ic))
		return 0;

	memcpy(&vhtop, &ic->ic_vhtop, sizeof(vhtop));

	if (IEEE80211_IS_VHT_20(ic))
		bw = MIN(ic->ic_bss_bw, BW_HT20);
	else if (IEEE80211_IS_VHT_40(ic))
		bw = MIN(ic->ic_bss_bw, BW_HT40);
	else if (IEEE80211_IS_VHT_80(ic))
		bw = MIN(ic->ic_bss_bw, BW_HT80);
	else if (IEEE80211_IS_VHT_160(ic))
		bw = MIN(ic->ic_bss_bw, BW_HT160);

	if ((bw == BW_HT20) || (bw == BW_HT40)) {
		vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
	} else if (bw == BW_HT80) {
		vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
		vhtop.centerfreq0 = ic->ic_bsschan->ic_center_f_80MHz;
	} else if (bw == BW_HT160) {
		vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_160MHZ;
		vhtop.centerfreq0 = ic->ic_bsschan->ic_center_f_160MHz;
	}
	memcpy(&ni->ni_vhtop, &vhtop, sizeof(ni->ni_vhtop));

	*frm = ieee80211_add_vhtop(ni, *frm, &vhtop);

	return 0;
}

static int
ieee80211_tdls_add_tlv_aid(struct ieee80211_node *ni, u_int8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_ie_aid *aid = (struct ieee80211_ie_aid *)(*frm);

	if (!IS_IEEE80211_VHT_ENABLED(ic))
		return 0;

	aid->aid_id= IEEE80211_ELEMID_AID;
	aid->aid_len= 2;
	aid->aid = htole16(vap->iv_bss->ni_associd);

	*frm += sizeof(struct ieee80211_ie_aid);

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"%s: Add AID IE, AID = %d\n", __func__, aid->aid);

	return 0;
}

static int
ieee80211_tdls_add_tlv_wide_bw_cs(struct ieee80211_node *ni,
		u_int8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_ie_wbchansw *ie = (struct ieee80211_ie_wbchansw *)(*frm);
	struct ieee80211_channel *des_chan = NULL;
	u_int32_t chwidth = 0;

	if (vap->tdls_off_chan_bw <= BW_HT40)
		return 0;

	des_chan = ic->ic_findchannel(ic, vap->tdls_target_chan, IEEE80211_MODE_AUTO);
	if (des_chan == NULL) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Fail to find the target channel\n", __func__);
		return -1;
	}

	ie->wbcs_id = IEEE80211_ELEMID_WBWCHANSWITCH;
	ie->wbcs_len = sizeof(struct ieee80211_ie_wbchansw) - 2;
	switch (vap->tdls_off_chan_bw) {
		case BW_HT20:
		case BW_HT40:
			chwidth = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
			break;
		case BW_HT80:
			chwidth = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
			break;
		default:
			chwidth = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
	}

	ie->wbcs_newchanw = chwidth;
	if (vap->tdls_off_chan_bw == BW_HT40) {
		ie->wbcs_newchancf0 = des_chan->ic_center_f_40MHz;
		ie->wbcs_newchancf1 = 0;
	} else if (vap->tdls_off_chan_bw == BW_HT80) {
		ie->wbcs_newchancf0 = des_chan->ic_center_f_80MHz;
		ie->wbcs_newchancf1 = 0;
	} else {
		ie->wbcs_newchancf0 = 0;
		ie->wbcs_newchancf1 = 0;
	}

	*frm += sizeof(struct ieee80211_ie_wbchansw);

	return 0;
}

static int
ieee80211_tdls_add_tlv_vht_tx_power_evlope(struct ieee80211_node *ni,
		u_int8_t **frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_ie_vtxpwren *ie = (struct ieee80211_ie_vtxpwren *)(*frm);
	u_int8_t local_max_tx_pwrcnt = 0;
	struct ieee80211_channel *des_chan = NULL;

	if (!IS_IEEE80211_VHT_ENABLED(ic) || !(ic->ic_flags & IEEE80211_F_DOTH))
		return 0;

	des_chan = ic->ic_findchannel(ic, vap->tdls_target_chan, IEEE80211_MODE_AUTO);
	if (des_chan == NULL) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Fail to find the target channel\n", __func__);
		return -1;
	}

	switch (vap->tdls_off_chan_bw) {
		case BW_HT20:
			local_max_tx_pwrcnt = IEEE80211_TX_POW_FOR_20MHZ;
			break;
		case BW_HT40:
			local_max_tx_pwrcnt = IEEE80211_TX_POW_FOR_40MHZ;
			break;
		case BW_HT80:
			local_max_tx_pwrcnt = IEEE80211_TX_POW_FOR_80MHZ;
			break;
		default:
			local_max_tx_pwrcnt = IEEE80211_TX_POW_FOR_80MHZ;
	}

	ie->vtxpwren_id = IEEE80211_ELEMID_VHTXMTPWRENVLP;
	ie->vtxpwren_len = sizeof(struct ieee80211_ie_vtxpwren) - 2;

	ie->vtxpwren_txpwr_info = local_max_tx_pwrcnt;
	ie->vtxpwren_tp20 = des_chan->ic_maxregpower - ic->ic_pwr_constraint;
	ie->vtxpwren_tp40 = des_chan->ic_maxregpower - ic->ic_pwr_constraint;
	ie->vtxpwren_tp80 = des_chan->ic_maxregpower - ic->ic_pwr_constraint;
	ie->vtxpwren_tp160 = 0;

	*frm += sizeof(struct ieee80211_ie_vtxpwren);

	return 0;
}

static uint8_t *
ieee80211_tdls_find_cs_timing(uint8_t *buf, uint32_t buf_len)
{
	uint8_t *ie = buf;
	uint8_t *ie_end = ie + buf_len;
	while (ie < ie_end) {
		switch (*ie) {
		case IEEE80211_ELEMID_TDLS_CS_TIMING:
			return ie;
		default:
			break;
		}
		ie += ie[1] + 2;
	}

	return NULL;
}

static int
ieee80211_tdls_copy_cs_timing(struct ieee80211_node *ni, uint8_t **frm,
			struct ieee80211_tdls_action_data *data)
{
	uint8_t *ie;
	uint8_t *ie_end;

	if (!ni || !frm || !data)
		return 1;

	ie = data->ie_buf;
	ie_end = ie + data->ie_buflen;
	while (ie < ie_end) {
		switch (*ie) {
		case IEEE80211_ELEMID_TDLS_CS_TIMING:
			memcpy(*frm, ie, ie[1] + 2);
			*frm += ie[1] + 2;
			return 0;
		default:
			break;
		}
		ie += ie[1] + 2;
	}

	return 1;
}

static int
ieee80211_tdls_add_tlv_cs_timimg(struct ieee80211_node *ni, u_int8_t **frm,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_tdls_cs_timing *cs_timing = (struct ieee80211_tdls_cs_timing *)(*frm);
	uint16_t sw_time;
	uint16_t sw_timeout;

	sw_time = DEFAULT_TDLS_CH_SW_NEGO_TIME;
	sw_timeout = DEFAULT_TDLS_CH_SW_NEGO_TIME + DEFAULT_TDLS_CH_SW_PROC_TIME;

	cs_timing->id = IEEE80211_ELEMID_TDLS_CS_TIMING;
	cs_timing->len = 4;
	cs_timing->switch_time = cpu_to_le16(sw_time);
	cs_timing->switch_timeout = cpu_to_le16(sw_timeout);

	*frm += sizeof(struct ieee80211_tdls_cs_timing);

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"%s: Add cs timing IE, cs_time = %d us, cs_timeout = %d us \n",
			__func__, sw_time, sw_timeout);

	return 0;
}


static int
ieee80211_tdls_compare_fdb_entry(const void *a, const void *b)
{
	struct __fdb_entry * fdb_a = (struct __fdb_entry *) a;
	struct __fdb_entry * fdb_b = (struct __fdb_entry *) b;

	if (fdb_a->ageing_timer_value > fdb_b->ageing_timer_value)
		return 1;
	if (fdb_a->ageing_timer_value < fdb_b->ageing_timer_value)
		return -1;
	return 0;
}

static int
ieee80211_tdls_add_tlv_downstream_clients(struct ieee80211_node *ni, uint8_t **frm, size_t buf_size)
{
	struct ieee80211vap *vap = ni->ni_vap;
	int bridge_entry_cnt = 0;
	int max_clients = 0;
	int client_list_cnt = 0;
	struct ieee80211_ie_qtn_tdls_clients *clients;
	int i;

	if (buf_size < sizeof(*clients)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: Not enough space for TDLS downstream "
			"client TLV, increase TDLS frame size\n", __func__);
		return 1;
	}

	/* First, extract all bridge entries */
	if (!br_fdb_fillbuf_hook || !vap->iv_dev->br_port) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: Not a bridge port or bridge (%p) or "
			"callback func was not initialized (%p)",
			__func__, vap->iv_dev->br_port, br_fdb_fillbuf_hook);
		return 1;
	}

	if (!bridge_entries) {
		bridge_entries = (struct __fdb_entry *) kmalloc(
				sizeof(struct __fdb_entry) *
				IEEE80211_TDLS_MAX_BRIDGE_CLIENTS, GFP_KERNEL);
		if (bridge_entries == NULL) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"%s: can't alloc space for "
				"downstream bridge entries", __func__);
			return 1;
		}
	}

	bridge_entry_cnt = br_fdb_fillbuf_hook(vap->iv_dev->br_port->br,
			bridge_entries, IEEE80211_TDLS_MAX_BRIDGE_CLIENTS, 0);

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS: bridge_entry_cnt=%d\n", bridge_entry_cnt);

	if (bridge_entry_cnt >= IEEE80211_TDLS_MAX_BRIDGE_CLIENTS)
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: at maximum # of TDLS bridge entries "
			"(%d)\n", __func__, bridge_entry_cnt);

	/* Sort the bridge entries by age */
	sort(bridge_entries, bridge_entry_cnt, sizeof(struct __fdb_entry),
			ieee80211_tdls_compare_fdb_entry, NULL);

	/* Calculate the space we have for downstream entries in the frame */
	max_clients = (buf_size - sizeof(*clients)) / IEEE80211_ADDR_LEN;
	if (max_clients > IEEE80211_QTN_IE_DOWNSTREAM_MAC_MAX)
		max_clients = IEEE80211_QTN_IE_DOWNSTREAM_MAC_MAX;

	/* Fill out the frame */
	clients = (struct ieee80211_ie_qtn_tdls_clients *)*frm;
	clients->qtn_ie_id = IEEE80211_ELEMID_VENDOR;
	clients->qtn_ie_oui[0] = QTN_OUI & 0xff;
	clients->qtn_ie_oui[1] = (QTN_OUI >> 8) & 0xff;
	clients->qtn_ie_oui[2] = (QTN_OUI >> 16) & 0xff;
	clients->qtn_ie_type = QTN_OUI_TDLS_BRMACS;

	for (i = 0; i < bridge_entry_cnt; i++) {
		if (bridge_entries[i].is_local || !bridge_entries[i].is_wlan) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS: Bridge table macs %pM"
				" %s a local address and %s a wlan device\n",
				bridge_entries[i].mac_addr,
				bridge_entries[i].is_local ? "is" : "is not",
				bridge_entries[i].is_wlan ? "is" : "is not");

			IEEE80211_ADDR_COPY(clients->qtn_ie_mac +
					client_list_cnt * IEEE80211_ADDR_LEN,
					bridge_entries[i].mac_addr);

			client_list_cnt++;

			if (client_list_cnt >= max_clients)
				break;
		}

	}

	clients->qtn_ie_mac_cnt = client_list_cnt;
	clients->qtn_ie_len = sizeof(*clients) - 2 +
			(client_list_cnt * IEEE80211_ADDR_LEN);
	*frm += sizeof(*clients) + (client_list_cnt * IEEE80211_ADDR_LEN);

	return 0;
}

static void
ieee80211_tdls_add_bridge_entry(struct ieee80211_node *ni, uint8_t *addr,
		__u16 sub_port)
{
	struct ieee80211vap *vap = ni->ni_vap;

	rcu_read_lock();
	if (br_fdb_update_const_hook && vap->iv_dev->br_port) {
		br_fdb_update_const_hook(vap->iv_dev->br_port->br,
				vap->iv_dev->br_port, addr, IEEE80211_NODE_IDX_MAP(sub_port));
	}
	rcu_read_unlock();
}

int
ieee80211_tdls_add_bridge_entry_for_peer(struct ieee80211_node *peer_ni)
{
	IEEE80211_TDLS_DPRINTF(peer_ni->ni_vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: for mac %pM ncidx = 0x%x\n", __func__,
		peer_ni->ni_macaddr, peer_ni->ni_node_idx);

	ieee80211_tdls_add_bridge_entry(peer_ni, peer_ni->ni_macaddr,
			peer_ni->ni_node_idx);

	return 0;
}

int
ieee80211_tdls_disable_peer_link(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (!IEEE80211_NODE_IS_TDLS_ACTIVE(ni) &&
			!IEEE80211_NODE_IS_TDLS_STARTING(ni))
		return 0;

	if (ni->ni_node_idx) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"Purge subport [0x%x] since link disabled\n", ni->ni_node_idx);
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
		/*
		 * Delete all bridge table entries for the AID.  They would eventually
		 * age out, but in the mean time data will be directed to the wrong
		 * sub_port (AID) until the bridge entries get updated by upstream
		 * traffic from the endpoint.
		 * Multicast port entries for the AID (sub_port) are not aged and would
		 * hang around for ever, so they are also deleted
		 */
		if (br_fdb_delete_by_sub_port_hook &&
				vap->iv_dev->br_port) {
			br_fdb_delete_by_sub_port_hook(vap->iv_dev->br_port->br,
					vap->iv_dev->br_port,
					ni->ni_node_idx);
		}
#endif
	}

	ni->tdls_initiator = 0;

	if (ni->ni_ext_flags & IEEE80211_NODE_TDLS_AUTH) {
		ieee80211_sta_assocs_dec(vap, __func__);
		ieee80211_nonqtn_sta_leave(vap, ni, __func__);
	}

	/* Restore ni_bssid to the local AP BSSID */
	if (!IEEE80211_ADDR_EQ(ni->ni_bssid, vap->iv_bss->ni_bssid))
		IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_bss->ni_bssid);

	if (vap->tdls_cs_node && (vap->tdls_cs_node == ni))
		ieee80211_tdls_return_to_base_channel(vap, 0);
	ieee80211_tdls_update_node_status(ni, IEEE80211_TDLS_NODE_STATUS_IDLE);

	ni->ni_ext_flags &= ~IEEE80211_NODE_TDLS_AUTH;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS peer %pM teared down\n", ni->ni_macaddr);

	return 0;
}

static int
ieee80211_tdls_check_target_chan(struct ieee80211_node *ni,
			struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	int tar_chan =  tdls->target_chan;
	struct ieee80211_channel *chan = NULL;

	chan = ic->ic_findchannel(ic, tar_chan, IEEE80211_MODE_AUTO);
	if (isclr(ic->ic_chan_active, tar_chan) || !chan ||
			(chan->ic_flags & IEEE80211_CHAN_DFS))
		return 1;
	else
		return 0;
}

static int
ieee80211_tdls_check_2nd_chan_off(struct ieee80211_node *ni,
			struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	struct ieee80211_ie_sec_chan_off *sco =
		(struct ieee80211_ie_sec_chan_off *)tdls->sec_chan_off;
	int sec_chan;
	int tar_chan;

	if (!sco) {
		vap->tdls_off_chan_bw = BW_HT20;
		return 0;
	}

	sec_chan = sco->sco_off;
	tar_chan = tdls->target_chan;
	chan = ic->ic_findchannel(ic, tar_chan, IEEE80211_MODE_AUTO);
	if (!chan)
		return 1;

	if ((sec_chan == IEEE80211_HTINFO_EXTOFFSET_BELOW) &&
			(chan->ic_flags & IEEE80211_CHAN_HT40D)) {
		vap->tdls_off_chan_bw = BW_HT40;
		return 0;
	}

	if ((sec_chan == IEEE80211_HTINFO_EXTOFFSET_ABOVE) &&
			(chan->ic_flags & IEEE80211_CHAN_HT40U)) {
		vap->tdls_off_chan_bw = BW_HT40;
		return 0;
	}

	if ((sec_chan == IEEE80211_HTINFO_EXTOFFSET_NA) &&
			(chan->ic_flags & IEEE80211_CHAN_HT20)) {
		vap->tdls_off_chan_bw = BW_HT20;
		return 0;
	}

	return 0;
}

static int
ieee80211_tdls_check_wide_bw_cs(struct ieee80211_node *ni,
			struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	struct ieee80211_ie_wbchansw *bw_cs =
		(struct ieee80211_ie_wbchansw *)tdls->wide_bw_cs;

	if (!bw_cs)
		return 1;

	if (bw_cs->wbcs_newchanw == 0)
		return 1;

	chan = ic->ic_findchannel(ic, tdls->target_chan, IEEE80211_MODE_AUTO);
	if (!chan)
		return 1;

	if ((bw_cs->wbcs_newchanw == 1) &&
			(chan->ic_center_f_80MHz == bw_cs->wbcs_newchancf0)) {
		vap->tdls_off_chan_bw = BW_HT80;
		return 0;
	}

	if ((bw_cs->wbcs_newchanw == 2) &&
				(chan->ic_center_f_160MHz== bw_cs->wbcs_newchancf0)) {
		vap->tdls_off_chan_bw = BW_HT160;
		return 0;
	}

	return 1;
}

static int
ieee80211_tdls_check_reg_class(struct ieee80211_node *ni,
			struct ieee80211_tdls_params *tdls)
{
	return 0;
}

static int
ieee80211_tdls_check_link_id(struct ieee80211_node *ni,
			struct ieee80211_tdls_params *tdls, uint8_t action)
{
	struct ieee80211vap *vap = ni->ni_vap;
	int ret = 1;

	if (!tdls->link_id)
		return ret;

	if (ieee80211_tdls_over_qhop_enabled(vap))
		ret = !ieee80211_tdls_ext_bssid_allowed(vap, tdls->link_id->bssid);
	else
		ret = !IEEE80211_ADDR_EQ(ni->ni_bssid, tdls->link_id->bssid);

	return ret;
}

static int
ieee80211_tdls_check_chan_switch_timing(struct ieee80211_node *ni,
			struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_tdls_cs_timing *cs_timing =
			(struct ieee80211_tdls_cs_timing *)(tdls->cs_timing);
	uint32_t sw_time;
	uint32_t sw_timeout;
	uint64_t cur_tsf;
	uint64_t tbtt;
	uint32_t duration;

	if (!cs_timing)
		return 1;

	sw_time = cs_timing->switch_time;
	sw_timeout = cs_timing->switch_timeout;

	ic->ic_get_tsf(&cur_tsf);
	tbtt = vap->iv_bss->ni_shared_stats->dtim_tbtt;
	duration = (uint32_t)(tbtt - cur_tsf);

	if ((sw_time < DEFAULT_TDLS_CH_SW_PROC_TIME) ||	(sw_time > duration) ||
			((sw_timeout - sw_time) < DEFAULT_TDLS_CH_SW_MIN_TIME) ||
			((sw_timeout - sw_time) > (duration - DEFAULT_TDLS_CH_SW_PROC_TIME)))
		return 1;

	return 0;
}

static uint8_t
ieee80211_tdls_select_target_channel(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t tar_chan = 0;
	int pick_flags = IEEE80211_SCS_PICK_NON_DFS_ONLY | IEEE80211_SCS_PICK_ANYWAY;

	tar_chan = ieee80211_scs_pick_channel(ic, pick_flags, IEEE80211_SCS_NA_CC);

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: Target channel %d\n", __func__, tar_chan);

	return tar_chan;
}

int
ieee80211_tdls_channel_switch_allowed(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	struct ieee80211_node *tmp;

	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, tmp) {
		if (ni->ni_flags & IEEE80211_NODE_PS_DELIVERING)
			return 0;
	}

	return 1;
}
extern int dev_queue_xmit(struct sk_buff *skb);

static void
ieee80211_tdls_send_frame(struct ieee80211_node *ni, struct sk_buff *skb)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node *bss;
	int err;

	if ((ni->ni_vap)->iv_debug & IEEE80211_MSG_OUTPUT) {
		int i;
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"Xmit:dev[%s]len[%d]bcast[%d]pri[%d]\n",
			ni->ni_vap->iv_dev->name, skb->len,
			ni->ni_stats.ns_tx_bcast, skb->priority);
		for(i = 0; i < 64; i += 16) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"Data[%p]: %02x %02x %02x %02x", &skb->data[i<<4],
				skb->data[i << 4], skb->data[(i << 4) + 1],
				skb->data[(i << 4) + 2], skb->data[(i << 4) + 3]);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"  %02x %02x %02x %02x\t",
				skb->data[(i << 4) + 4], skb->data[(i << 4) + 5],
				skb->data[(i << 4) + 6], skb->data[(i << 4) + 7]);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"%02x %02x %02x %02x",
				skb->data[(i << 4) + 8], skb->data[(i << 4) + 9],
				skb->data[(i << 4) + 10], skb->data[(i << 4) + 11]);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"  %02x %02x %02x %02x\n",
				skb->data[(i << 4) + 12], skb->data[(i << 4) + 13],
				skb->data[(i << 4) + 14], skb->data[(i << 4) + 15]);
		}
	}

	bss = vap->iv_bss;
	ieee80211_ref_node(bss);

	skb->dev = bss->ni_vap->iv_dev;
	QTN_SKB_CB_NI(skb) = bss;
	M_FLAG_SET(skb, M_NO_AMSDU);

	vap->iv_stats.is_tx_tdls++;
	IEEE80211_NODE_STAT(ni, tx_tdls_action);

	err = dev_queue_xmit(skb);
	if (err < 0) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Sending failed\n", __func__);
		kfree_skb(skb);
		ieee80211_free_node(bss);
	}
}

static void
ieee80211_tdls_send_frame_over_tdls(struct ieee80211_node *peer_ni, struct sk_buff *skb)
{
	struct ieee80211vap *vap = peer_ni->ni_vap;
	int err;

	if (!IEEE80211_NODE_IS_TDLS_ACTIVE(peer_ni))
		return;

	if (vap->iv_debug & IEEE80211_MSG_OUTPUT) {
		int i;
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"Xmit:dev[%s]len[%d]bcast[%d]pri[%d]\n",
			peer_ni->ni_vap->iv_dev->name, skb->len,
			peer_ni->ni_stats.ns_tx_bcast, skb->priority);
		for(i = 0; i < 64; i += 16) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"Data[%p]: %02x %02x %02x %02x", &skb->data[i<<4],
				skb->data[i << 4], skb->data[(i << 4) + 1],
				skb->data[(i << 4) + 2], skb->data[(i << 4) + 3]);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"  %02x %02x %02x %02x\t",
				skb->data[(i << 4) + 4], skb->data[(i << 4) + 5],
				skb->data[(i << 4) + 6], skb->data[(i << 4) + 7]);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"%02x %02x %02x %02x",
				skb->data[(i << 4) + 8], skb->data[(i << 4) + 9],
				skb->data[(i << 4) + 10], skb->data[(i << 4) + 11]);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"  %02x %02x %02x %02x\n",
				skb->data[(i << 4) + 12], skb->data[(i << 4) + 13],
				skb->data[(i << 4) + 14], skb->data[(i << 4) + 15]);
		}
	}

	ieee80211_ref_node(peer_ni);

	skb->dev = peer_ni->ni_vap->iv_dev;
	QTN_SKB_CB_NI(skb) = peer_ni;
	M_FLAG_SET(skb, M_NO_AMSDU);

	vap->iv_stats.is_tx_tdls++;
	IEEE80211_NODE_STAT(peer_ni, tx_tdls_action);

	err = dev_queue_xmit(skb);
	if (err < 0) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Sending failed\n", __func__);
		kfree_skb(skb);
		ieee80211_free_node(peer_ni);
	}
}

/*
 * Send a Setup Confirm
 */
static int
ieee80211_tdls_send_setup_confirm(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	uint8_t action = IEEE80211_ACTION_TDLS_SETUP_CONFIRM;
	struct sk_buff *skb;
	uint8_t *frm = NULL;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;

	if (!peer_ni || !data)
		return 1;

	vap = peer_ni->ni_vap;
	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: tdls function prohibited, don't send setup confirm\n", __func__);
		return 1;
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: send setup confirm frame to %pM\n", __func__, peer_ni->ni_macaddr);

	peer_ni->tdls_initiator = 1;

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb)
		goto error;

	memcpy(frm, &data->status, sizeof(data->status));
	frm += sizeof(data->status);
	*frm = data->dtoken;
	frm += sizeof(data->dtoken);

	if (le16toh(data->status) == IEEE80211_STATUS_SUCCESS) {
		if (ieee80211_tdls_get_privacy(vap)) {
			if (ieee80211_tdls_add_tlv_rsn(peer_ni, &frm, data))
				goto error;
		}
		if (ieee80211_tdls_add_tlv_edca_param(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_get_privacy(vap)) {
			if (ieee80211_tdls_add_tlv_ftie(peer_ni, &frm, data))
				goto error;
			if (ieee80211_tdls_add_tlv_tpk_timeout(peer_ni, &frm, data))
				goto error;
		}
		if (!IEEE80211_NODE_IS_HT(vap->iv_bss) &&
				IEEE80211_NODE_IS_HT(peer_ni))
			ieee80211_tdls_add_tlv_ht_oper(peer_ni, &frm);

		if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
				peer_ni->ni_macaddr, data))
			goto error;

		if (!IEEE80211_NODE_IS_VHT(vap->iv_bss) &&
				IEEE80211_NODE_IS_VHT(peer_ni))
			ieee80211_tdls_add_tlv_vhtop(peer_ni, &frm);
	}

	frm = ieee80211_add_qtn_ie(frm, peer_ni->ni_ic,
			IEEE80211_QTN_BRIDGEMODE, IEEE80211_QTN_BRIDGEMODE,
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H, 0);

	if (ieee80211_tdls_add_tlv_downstream_clients(peer_ni, &frm,
			(frm_len - (frm - skb->data))))
		goto error;

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame(peer_ni, skb);

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);

	return 1;
}

/*
 * Send a Setup Response
 * A reference to the peer_ni structure must be held before calling this function, and
 * must be freed on return if not sent.
 * Returns 0 if successful, else 1.
 * Note: Link ID is always present, not just if status code is 0 (correction to 7.4.11.2).
 */
static int
ieee80211_tdls_send_setup_resp(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	uint8_t action = IEEE80211_ACTION_TDLS_SETUP_RESP;
	struct sk_buff *skb;
	uint8_t *frm = NULL;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;
	struct ieee80211com *ic;
	if (!peer_ni || !data)
		return 1;

	vap = peer_ni->ni_vap;
	ic = vap->iv_ic;
	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: tdls function prohibited, don't send setup response\n", __func__);
		return 1;
	}

	if (!ieee80211_tdls_link_should_response(vap, peer_ni)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: Don't send setup response to Peer %pM due to bad link\n",
				__func__, peer_ni->ni_macaddr);
		return 1;
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: send setup response frame to %pM\n", __func__, peer_ni->ni_macaddr);

	peer_ni->tdls_initiator = 0;

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb)
		goto error;

	memcpy(frm, &data->status, sizeof(data->status));
	frm += sizeof(data->status);
	*frm = data->dtoken;
	frm += sizeof(data->dtoken);
	if (le16toh(data->status) == IEEE80211_STATUS_SUCCESS) {
		if (ieee80211_tdls_add_cap(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_add_tlv_rates(peer_ni, &frm))
			goto error;

		if ((ic->ic_flags_ext & IEEE80211_FEXT_COUNTRYIE)
				|| ((ic->ic_flags & IEEE80211_F_DOTH) &&
					(ic->ic_flags_ext & IEEE80211_FEXT_TPC))) {
			if (ieee80211_tdls_add_tlv_country(peer_ni, &frm))
				goto error;
		}

		if (ieee80211_tdls_add_tlv_xrates(peer_ni, &frm))
			goto error;

		if (!(vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB))
			frm = ieee80211_add_supported_chans(frm, ic);

		if (ieee80211_tdls_get_privacy(vap)) {
			if (ieee80211_tdls_add_tlv_rsn(peer_ni, &frm, data))
				goto error;
		}
		if (ieee80211_tdls_add_tlv_ext_cap(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_add_tlv_qos_cap(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_get_privacy(vap)) {
			if (ieee80211_tdls_add_tlv_ftie(peer_ni, &frm, data))
				goto error;
			if (ieee80211_tdls_add_tlv_tpk_timeout(peer_ni, &frm, data))
				goto error;
		}
		if (!(vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB)) {
			if (ieee80211_tdls_add_tlv_sup_reg_class(peer_ni, &frm))
				goto error;
		}
		if (ieee80211_tdls_add_tlv_ht_cap(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_add_tlv_bss_2040_coex(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
						   peer_ni->ni_macaddr, data))
			goto error;
		if (ieee80211_tdls_add_tlv_aid(peer_ni, &frm))
			goto error;
		if (ieee80211_tdls_add_tlv_vhtcap(peer_ni, &frm))
			goto error;
	}

	frm = ieee80211_add_qtn_ie(frm, peer_ni->ni_ic,
			IEEE80211_QTN_BRIDGEMODE, IEEE80211_QTN_BRIDGEMODE,
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H, 0);

	if (ieee80211_tdls_add_tlv_downstream_clients(peer_ni, &frm,
			(frm_len - (frm - skb->data))))
		goto error;

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame(peer_ni, skb);

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);

	return 1;
}

/*
 * Send a Setup Request
 * A reference to the peer_ni structure must be held before calling this function, and
 * must be freed on return if not sent.
 * Returns 0 if sent successfully, else 1.
 */
static int
ieee80211_tdls_send_setup_req(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	uint8_t action = IEEE80211_ACTION_TDLS_SETUP_REQ;
	struct sk_buff *skb;
	uint8_t *frm = NULL;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;

	if (!peer_ni || !data)
		return 1;

	vap = peer_ni->ni_vap;
	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: tdls function prohibited, don't send setup request\n", __func__);
		return 1;
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: send SETUP REQUEST frame to %pM\n", __func__, peer_ni->ni_macaddr);

	peer_ni->tdls_initiator = 1;

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb)
		goto error;

	*frm = data->dtoken;
	frm += sizeof(data->dtoken);

	if (ieee80211_tdls_add_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_rates(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_country(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_xrates(peer_ni, &frm))
		goto error;

	if (!(vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB))
		frm = ieee80211_add_supported_chans(frm, vap->iv_ic);

	if (ieee80211_tdls_get_privacy(vap)) {
		if (ieee80211_tdls_add_tlv_rsn(peer_ni, &frm, data))
			goto error;
	}
	if (ieee80211_tdls_add_tlv_ext_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_qos_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_get_privacy(vap)) {
		if (ieee80211_tdls_add_tlv_ftie(peer_ni, &frm, data))
			goto error;
		if (ieee80211_tdls_add_tlv_tpk_timeout(peer_ni, &frm, data))
			goto error;
	}
	if (!(vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB)) {
		if (ieee80211_tdls_add_tlv_sup_reg_class(peer_ni, &frm))
			goto error;
	}
	if (ieee80211_tdls_add_tlv_ht_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_bss_2040_coex(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
					   peer_ni->ni_macaddr, data))
		goto error;
	if (ieee80211_tdls_add_tlv_aid(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_vhtcap(peer_ni, &frm))
		goto error;

	frm = ieee80211_add_qtn_ie(frm, peer_ni->ni_ic,
			IEEE80211_QTN_BRIDGEMODE, IEEE80211_QTN_BRIDGEMODE,
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H, 0);

	if (ieee80211_tdls_add_tlv_downstream_clients(peer_ni, &frm,
			(frm_len - (frm - skb->data))))
		goto error;

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame(peer_ni, skb);

	peer_ni->tdls_setup_start = jiffies;
	ieee80211_tdls_update_node_status(peer_ni,
		IEEE80211_TDLS_NODE_STATUS_STARTING);

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);

	return 1;
}

/*
 * Send a Discovery Response
 * Returns 0 if sent successfully, else 1.
 * Note: Discovery response is a public action frame.  All other TDLS frames are
 * management over data.
 */
static int
ieee80211_tdls_send_disc_resp(struct ieee80211_node *peer_ni,
	struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	struct sk_buff *skb = NULL;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;
	uint8_t *frm;
	uint8_t action = IEEE80211_ACTION_PUB_TDLS_DISC_RESP;
	struct ieee80211_action *ia;
	struct ieee80211_tdls_link_id *link_id = NULL;
	uint8_t *bssid;

	if (!peer_ni) {
		printk(KERN_WARNING "%s: Invalid peer node\n", __func__);
		return 1;
	}

	vap = peer_ni->ni_vap;

	if (!data) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Invalid argument data\n", __func__);
		goto error;
	}

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: tdls function prohibited, don't send discover response\n", __func__);
		goto error;
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: send DISC RESPONSE frame to %pM\n", __func__, peer_ni->ni_macaddr);


	skb = ieee80211_getmgtframe(&frm, frm_len);
	if (skb == NULL) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_NODE, IEEE80211_TDLS_MSG_WARN,
			"%s: cannot get buf; size %u", __func__, frm_len);
		vap->iv_stats.is_tx_nobuf++;
		goto error;
	}

	ia = (struct ieee80211_action *)frm;
	ia->ia_category = IEEE80211_ACTION_CAT_PUBLIC;
	ia->ia_action = IEEE80211_ACTION_PUB_TDLS_DISC_RESP;
	frm += sizeof(*ia);

	/* Fixed Length Fields */
	*frm = data->dtoken;
	frm += sizeof(data->dtoken);
	if (ieee80211_tdls_add_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_rates(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_xrates(peer_ni, &frm))
		goto error;

	frm = ieee80211_add_supported_chans(frm, vap->iv_ic);

	if (ieee80211_tdls_get_privacy(vap)) {
		if (ieee80211_tdls_add_tlv_rsn(peer_ni, &frm, data))
			goto error;
	}
	if (ieee80211_tdls_add_tlv_ext_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_get_privacy(vap)) {
		if (ieee80211_tdls_add_tlv_ftie(peer_ni, &frm, data))
			goto error;
		if (ieee80211_tdls_add_tlv_tpk_timeout(peer_ni, &frm, data))
			goto error;
	}
	if (!(vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB)) {
		if (ieee80211_tdls_add_tlv_sup_reg_class(peer_ni, &frm))
			goto error;
	}
	if (ieee80211_tdls_add_tlv_ht_cap(peer_ni, &frm))
		goto error;
	if (ieee80211_tdls_add_tlv_bss_2040_coex(peer_ni, &frm))
		goto error;

	link_id = (struct ieee80211_tdls_link_id *)frm;
	if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
					   peer_ni->ni_macaddr, data))
		goto error;
	if (ieee80211_tdls_add_tlv_vhtcap(peer_ni, &frm))
		goto error;

	frm = ieee80211_add_qtn_ie(frm, peer_ni->ni_ic,
			IEEE80211_QTN_BRIDGEMODE, IEEE80211_QTN_BRIDGEMODE,
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H, 0);

	if (ieee80211_tdls_add_tlv_downstream_clients(peer_ni, &frm, frm_len -
			(frm - skb->data))) {
		goto error;
	}

	skb_trim(skb, frm - skb->data);

	vap->iv_stats.is_tx_tdls++;
	IEEE80211_NODE_STAT(peer_ni, tx_tdls_action);

	bssid = peer_ni->ni_bssid;
	if (ieee80211_tdls_over_qhop_enabled(vap) && link_id)
		bssid = link_id->bssid;

	ieee80211_tdls_mgmt_output(peer_ni, skb,
		IEEE80211_FC0_TYPE_MGT, IEEE80211_FC0_SUBTYPE_ACTION, peer_ni->ni_macaddr, bssid);

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);
	ieee80211_free_node(peer_ni);

	return 1;
}

/*
 * Send a Discovery Request
 * Returns 0 if sent successfully, else 1.
 */
static int
ieee80211_tdls_send_disc_req(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	static uint8_t dtoken = 0;
	uint8_t action = IEEE80211_ACTION_TDLS_DISC_REQ;
	struct sk_buff *skb;
	uint8_t *frm = NULL;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;

	if (!peer_ni) {
		printk(KERN_WARNING "%s: Invalid peer node\n", __func__);
		return 1;
	}

	vap = peer_ni->ni_vap;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: tdls function prohibited, don't send discover request\n", __func__);
		return 1;
	}

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: send DISC RESQUEST frame to %pM\n", __func__, peer_ni->ni_macaddr);

	if (peer_ni == vap->iv_bss)
		skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 0);
	else
		skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: can't alloc space for disc request frame", __func__);
		goto error;
	}

	if (!data) {
		*frm = dtoken++;
		frm += sizeof(dtoken);

		if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action,
				&frm, vap->iv_dev->broadcast, data)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"%s: Failed to add link id for bcast", __func__);
			goto error;
		}
	} else {
		*frm = data->dtoken;
		frm += sizeof(data->dtoken);

		memcpy(frm, data->ie_buf, le32toh(data->ie_buflen));
		frm += le32toh(data->ie_buflen);

		if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action,
				&frm, peer_ni->ni_macaddr, data)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"%s: Failed to add link id", __func__);
			goto error;
		}
	}

	if (ieee80211_tdls_add_tlv_rates(peer_ni, &frm))
		goto error;

	if (ieee80211_tdls_add_tlv_xrates(peer_ni, &frm))
		goto error;

	if (ieee80211_tdls_add_tlv_ht_cap(peer_ni, &frm))
		goto error;

	frm = ieee80211_add_qtn_ie(frm, peer_ni->ni_ic,
			IEEE80211_QTN_BRIDGEMODE, IEEE80211_QTN_BRIDGEMODE,
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H, 0);

	if (ieee80211_tdls_add_tlv_downstream_clients(peer_ni, &frm, frm_len -
			(frm - skb->data))) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: Failed to add DS clients", __func__);
		goto error;
	}

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame(peer_ni, skb);

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);
	return 1;
}

static int
ieee80211_tdls_send_teardown(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	uint8_t action = IEEE80211_ACTION_TDLS_TEARDOWN;
	struct sk_buff *skb = NULL;
	struct sk_buff *skb2 = NULL;
	uint8_t *frm = NULL;
	uint16_t frm_len = IEEE80211_TDLS_FRAME_MAX;

	if (!peer_ni || !data)
		return 1;

	vap = peer_ni->ni_vap;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: send teardown frame to %pM\n", __func__, peer_ni->ni_macaddr);

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb)
		goto error;

	memcpy(frm, &data->status, sizeof(data->status));
	frm += sizeof(data->status);

	if (ieee80211_tdls_get_privacy(vap)) {
		if (ieee80211_tdls_add_tlv_ftie(peer_ni, &frm, data))
			goto error;
	}

	if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
					   peer_ni->ni_macaddr, data))
		goto error;

	frm = ieee80211_add_qtn_ie(frm, peer_ni->ni_ic,
			IEEE80211_QTN_BRIDGEMODE, IEEE80211_QTN_BRIDGEMODE,
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H, 0);

	if (ieee80211_tdls_add_tlv_downstream_clients(peer_ni, &frm, frm_len -
			(frm - skb->data))) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: Failed to add DS clients", __func__);
		goto error;
	}

	/*
	 * Send teardown frame via AP and TDLS link simultaneously to
	 * avoid peer fails to receive it.
	 */
	skb_trim(skb, frm - skb->data);
	skb2 = skb_copy(skb, GFP_ATOMIC);

	ieee80211_tdls_send_frame_over_tdls(peer_ni, skb);
	if (skb2)
		ieee80211_tdls_send_frame(peer_ni, skb2);

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);

	return 1;
}

/*
 * Send a Peer Traffic Indication Request
 * Returns 0 if sent successfully, else 1.
 */
static int
ieee80211_tdls_send_pti_req(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap= peer_ni->ni_vap;
	static uint8_t dtoken = 0;
	uint8_t action = IEEE80211_ACTION_TDLS_PTI;
	struct sk_buff *skb = NULL;
	uint8_t *frm = NULL;
	uint32_t pti = 0;
	uint32_t pti_ctrl = 0;

	/* Although it's unlikely, pti maybe 0 */
	pti = vap->iv_ic->ic_get_tdls_param(peer_ni, IOCTL_TDLS_PTI);
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: ni=%p ni_ref=%d vap=%p iv_myaddr=%pM\n ni_bssid=%pM"
		" traffic indicator 0x%x\n", __func__, peer_ni,
		ieee80211_node_refcnt(peer_ni), vap,
		vap->iv_myaddr, peer_ni->ni_bssid, pti);

	if (pti == 0)
		goto error;

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: can't alloc space for pti request frame", __func__);
		goto error;
	}

	if (!data) {
		*frm = dtoken++;
		frm += sizeof(dtoken);
	} else {
		*frm = data->dtoken;
		frm += sizeof(data->dtoken);
	}

	if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action,
			&frm, peer_ni->ni_macaddr, data)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"%s: Failed to add link id", __func__);
		goto error;
	}

	pti_ctrl = vap->iv_ic->ic_get_tdls_param(peer_ni, IOCTL_TDLS_PTI_CTRL);

	*frm++ = IEEE80211_ELEMID_TDLS_PTI_CTRL;
	*frm++ = 3;
	*frm++ = (pti_ctrl >> 16) & 0xFF;
	*((uint16_t*)frm) = htole16(pti_ctrl) & 0xFFFF;
	frm += 2;

	*frm++ = IEEE80211_ELEMID_TDLS_PU_BUF_STAT;
	*frm++ = 1;
	*frm++ = (pti & 0xF);

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame(peer_ni, skb);
	vap->iv_ic->ic_set_tdls_param(peer_ni, IOCTL_TDLS_PTI_PENDING, 1);

	return 0;
error:
	if (skb)
		dev_kfree_skb(skb);
	return 1;
}

static int
ieee80211_tdls_get_off_chan_and_bw(struct ieee80211vap *vap,
		struct ieee80211_node *ni, uint8_t *tar_chan, uint8_t *tar_bw)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	int bw_tmp = BW_INVALID;
	int chan_tmp = 0;

	if (vap->tdls_fixed_off_chan == TDLS_INVALID_CHANNEL_NUM)
		chan_tmp = ieee80211_tdls_select_target_channel(vap);
	else
		chan_tmp = vap->tdls_fixed_off_chan;

	chan = ic->ic_findchannel(ic, chan_tmp, IEEE80211_MODE_AUTO);
	if (!chan)
		return 1;

	bw_tmp = ieee80211_get_max_bw(vap, ni, chan_tmp);
	if (vap->tdls_fixed_off_chan_bw != BW_INVALID)
		bw_tmp = MIN(vap->tdls_fixed_off_chan_bw, bw_tmp);

	*tar_chan = chan_tmp;
	*tar_bw = bw_tmp;

	return 0;
}

int
ieee80211_tdls_send_chan_switch_req(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	uint8_t action = IEEE80211_ACTION_TDLS_CS_REQ;
	struct sk_buff *skb;
	uint8_t *frm = NULL;
	uint8_t reg_class = 0;

	if (!peer_ni)
		return 1;

	vap = peer_ni->ni_vap;
	ic = vap->iv_ic;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch req to peer %pM since tdls prohibited\n",
			peer_ni->ni_macaddr);
		return 1;
	}

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch req to peer %pM since channel switch"
			" prohibited\n", peer_ni->ni_macaddr);
		return 1;
	}

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PASSIVE) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch req to peer %pM since passive mode\n",
			peer_ni->ni_macaddr);
		return 1;
	}

	if (vap->tdls_chan_switching == 1) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch req to peer %pM since channel switch"
			" in progress\n", peer_ni->ni_macaddr);
		return 1;
	}

	if (peer_ni->tdls_no_send_cs_resp == 1) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch req to peer %pM since channel switch"
			" request processing\n", peer_ni->ni_macaddr);
		return 1;
	}

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to alloc skb\n", __func__);
		goto error;
	}

	if (ieee80211_tdls_get_off_chan_and_bw(vap, peer_ni,
				&vap->tdls_target_chan, &vap->tdls_off_chan_bw)) {
		if (vap->tdls_target_chan == 0)
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: Skip channel switch since current channel is best\n", __func__);
		else
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: Fail to get target channel and bandwidth\n", __func__);
		goto error;
	}

	if (vap->tdls_target_chan == ic->ic_curchan->ic_ieee) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Skip channel switch since off channel is equal with"
			" current channel\n", __func__);
		goto error;
	}

	reg_class = ieee80211_get_current_operating_class(ic->ic_country_code,
				vap->tdls_target_chan, vap->tdls_off_chan_bw);

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS: send channel switch frame to %pM, tar_chan: %d, BW: %d, reg_class:%d\n",
		peer_ni->ni_macaddr, vap->tdls_target_chan, vap->tdls_off_chan_bw, reg_class);

	*frm = vap->tdls_target_chan;
	frm += sizeof(vap->tdls_target_chan);
	*frm = reg_class;
	frm += sizeof(reg_class);
	if (ieee80211_tdls_add_tlv_2nd_chan_off(peer_ni, &frm)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add second channel offset IE for CS request\n", __func__);
		goto error;
	}
	if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
					   peer_ni->ni_macaddr, data)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add link ID IE for CS request\n", __func__);
		goto error;
	}
	if (ieee80211_tdls_add_tlv_cs_timimg(peer_ni, &frm, data)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add cs_timing IE for CS request\n", __func__);
		goto error;
	}
	if (ieee80211_tdls_add_tlv_wide_bw_cs(peer_ni, &frm)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add wide_bw_cs IE for CS request\n", __func__);
		goto error;
	}
	if (ieee80211_tdls_add_tlv_vht_tx_power_evlope(peer_ni, &frm)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add vht_tx_power_envlope IE for CS request\n", __func__);
		goto error;
	}

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame_over_tdls(peer_ni, skb);

	peer_ni->tdls_send_cs_req = 1;

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);

	return 1;
}

int
ieee80211_tdls_send_chan_switch_resp(struct ieee80211_node *peer_ni,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	uint8_t action = IEEE80211_ACTION_TDLS_CS_RESP;
	struct ieee80211_tdls_cs_timing *cs_timing = NULL;
	struct sk_buff *skb;
	uint8_t *frm = NULL;
	uint16_t status;
	uint8_t tar_chan;
	uint64_t cur_tsf;
	uint64_t start_tsf;
	uint64_t tbtt;
	uint32_t duration = 0;
	int chan_switch;

	if ((!peer_ni) || (!data))
		return 1;

	vap = peer_ni->ni_vap;
	ic = vap->iv_ic;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch resp to peer %pM since tdls prohibited\n",
			peer_ni->ni_macaddr);
		return 1;
	}

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS: Don't send channel switch resp to peer %pM since channel switch"
			" prohibited\n", peer_ni->ni_macaddr);
		return 1;
	}

	tar_chan = data->dtoken;
	status = data->status;
	cs_timing = (struct ieee80211_tdls_cs_timing *)
			ieee80211_tdls_find_cs_timing(data->ie_buf, data->ie_buflen);
	if (!cs_timing)
		return 1;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS: send channel switch response frame to %pM\n", peer_ni->ni_macaddr);

	skb = ieee80211_tdls_init_frame(peer_ni, &frm, action, 1);
	if (!skb) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to alloc skb\n", __func__);
		goto error;
	}

	*((uint16_t *)frm) = cpu_to_le16(status);
	frm += sizeof(status);
	if (ieee80211_tdls_add_tlv_link_id(peer_ni, skb, action, &frm,
						   peer_ni->ni_macaddr, data)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add link ID IE for channel switch resp\n", __func__);
			goto error;
	}
	if (ieee80211_tdls_copy_cs_timing(peer_ni, &frm, data)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Fail to add cs timing IE for channel switch resp\n", __func__);
			goto error;
	}

	chan_switch = (!status && !vap->tdls_chan_switching);

	if (chan_switch)
		ieee80211_sta_pwrsave(vap, 1);

	skb_trim(skb, frm - skb->data);
	ieee80211_tdls_send_frame_over_tdls(peer_ni, skb);

	peer_ni->tdls_no_send_cs_resp = 0;

	if (chan_switch) {
		ic->ic_get_tsf(&cur_tsf);
		vap->tdls_target_chan = tar_chan;
		vap->tdls_cs_time = cs_timing->switch_time;
		vap->tdls_cs_timeout = cs_timing->switch_timeout;

		tbtt = vap->iv_bss->ni_shared_stats->dtim_tbtt;
		start_tsf = cur_tsf + cs_timing->switch_time + DEFAULT_TDLS_CH_SW_TX_TIME;
		if (tbtt > (start_tsf + DEFAULT_TDLS_CH_SW_PROC_TIME))
			duration = (uint32_t)(tbtt - start_tsf - DEFAULT_TDLS_CH_SW_PROC_TIME);
		duration = MAX(duration, cs_timing->switch_timeout - cs_timing->switch_time);
		vap->tdls_cs_duration = duration;

		if (ieee80211_tdls_remain_on_channel(vap, peer_ni, tar_chan,
				vap->tdls_off_chan_bw, start_tsf, vap->tdls_cs_duration) != 0) {
			ieee80211_sta_pwrsave(vap, 0);
		}
	}

	return 0;

error:
	if (skb)
		dev_kfree_skb(skb);

	return 1;
}

int
ieee80211_tdls_validate_vap_state(struct ieee80211vap *vap)
{
	if (vap->iv_state != IEEE80211_S_RUN) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: not in run state\n", __func__);
		return 1;
	}

	/* TDLS is not allowed when disabled by the AP */
	if (!ieee80211_tdls_sec_mode_valid(vap)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: ignore TDLS because of TKIP\n", __func__);
		return 1;
	}

	return 0;
}

int
ieee80211_tdls_validate_params(struct ieee80211_node *ni, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (ieee80211_tdls_validate_vap_state(vap) != 0)
		return 1;

	return 0;
}

static void
ieee80211_tdls_add_clients_to_bridge(struct ieee80211_node *ni,
					struct ieee80211_node *peer_ni, uint8_t *ie_buf)
{
	struct ieee80211_ie_qtn_tdls_clients *clients =
			(struct ieee80211_ie_qtn_tdls_clients *)ie_buf;
	struct ieee80211vap *vap = ni->ni_vap;

	if (clients != NULL) {
		uint8_t i;

		/* Validate qtn_ie_mac_cnt */
		if ((clients->qtn_ie_mac_cnt * IEEE80211_ADDR_LEN) !=
				(clients->qtn_ie_len - 5)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ELEMID,
					"[%s] Bad qtn_ie_mac_cnt in TDLS client list\n",
					vap->iv_dev->name);
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS error %s: Received TDLS clients "
					"with bad qtn_ie_mac_cnt\n", __func__);
			return;
		}

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS number of clients = %d\n", clients->qtn_ie_mac_cnt);
		for (i = 0; i < clients->qtn_ie_mac_cnt; i++) {
			uint8_t *m = &clients->qtn_ie_mac[i * IEEE80211_ADDR_LEN];
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS client: %pM ncidx 0x%x\n", m, peer_ni->ni_node_idx);

			if (!is_multicast_ether_addr(m)) {
				ieee80211_tdls_add_bridge_entry(peer_ni, m,
							peer_ni->ni_node_idx);
			}
		}
	}
}

/*
 * Process a Discovery Response (Public Action frame)
 */
void
ieee80211_tdls_recv_disc_resp(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node *peer_ni;
	enum ieee80211_tdls_operation operation;

	if (!tdls || (ieee80211_tdls_validate_params(ni, tdls) != 0))
		return;

	tdls->act = IEEE80211_ACTION_PUB_TDLS_DISC_RESP;

	/*
	 * A discovery response may be unsolicited.  Find or create the peer node.
	 */
	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni == NULL)
		return;

	peer_ni->ni_rssi = rssi;
	peer_ni->tdls_last_seen = jiffies;
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS: received DISC RESP from peer %pM at %u, rssi=%d\n",
		peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

	if (tdls->qtn_brmacs != NULL) {
		if (peer_ni->ni_qtn_brmacs == NULL)
			MALLOC(peer_ni->ni_qtn_brmacs, uint8_t *,
				IEEE80211_MAX_IE_LEN, M_DEVBUF, M_WAITOK);
		if (peer_ni->ni_qtn_brmacs != NULL)
			memcpy(peer_ni->ni_qtn_brmacs, tdls->qtn_brmacs,
				tdls->qtn_brmacs[1] + 2);
	}

	/* FIXME What if this node thinks we are active but the other node doesn't think so? */
	if (IEEE80211_NODE_IS_TDLS_ACTIVE(peer_ni)) {
		/* Extract downstream mac addresses */
		ieee80211_tdls_add_clients_to_bridge(ni, peer_ni, tdls->qtn_brmacs);
	} else if (ieee80211_tdls_link_should_setup(vap, peer_ni)) {
		operation = IEEE80211_TDLS_SETUP;
		if (ieee80211_tdls_send_event(peer_ni, IEEE80211_EVENT_TDLS, &operation))
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: Send event %d failed\n", __func__, operation);
	}

	ieee80211_free_node(peer_ni);
}

/*
 * Process a Setup Request
 */
void
ieee80211_tdls_recv_setup_req(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node *peer_ni;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,"%s",
			"TDLS: received SETUP REQUEST, but tdls function prohibited, drop it\n");
		return;
	}

	if (!tdls || (ieee80211_tdls_validate_params(ni, tdls) != 0))
		return;

	/* We need to do this in the absence of explicit discovery message. */
	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni) {
		peer_ni->tdls_initiator = 0;
		peer_ni->tdls_last_seen = jiffies;
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS: received SETUP REQUEST from peer %pM at %u, rssi=%d\n",
			peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

		peer_ni->tdls_setup_start = jiffies;
		ieee80211_tdls_update_node_status(peer_ni,
				IEEE80211_TDLS_NODE_STATUS_STARTING);

		peer_ni->ni_vendor = PEER_VENDOR_NONE;
		if (tdls->qtn_info != NULL)
			peer_ni->ni_vendor = PEER_VENDOR_QTN;

		if (tdls->aid != NULL)
			peer_ni->tdls_peer_associd = le16toh(tdls->aid->aid);

		if (tdls->qtn_brmacs != NULL) {
			if (peer_ni->ni_qtn_brmacs == NULL)
				MALLOC(peer_ni->ni_qtn_brmacs, uint8_t *,
					IEEE80211_MAX_IE_LEN, M_DEVBUF, M_WAITOK);
			if (peer_ni->ni_qtn_brmacs != NULL)
				memcpy(peer_ni->ni_qtn_brmacs, tdls->qtn_brmacs,
					tdls->qtn_brmacs[1] + 2);
		}

		if (ieee80211_tdls_over_qhop_enabled(vap)) {
			if (ieee80211_tdls_ext_bssid_allowed(vap, tdls->link_id->bssid) &&
					!IEEE80211_ADDR_EQ(peer_ni->ni_bssid, tdls->link_id->bssid))
				IEEE80211_ADDR_COPY(peer_ni->ni_bssid, tdls->link_id->bssid);
		} else {
			if (!IEEE80211_ADDR_EQ(peer_ni->ni_bssid, vap->iv_bss->ni_bssid))
				IEEE80211_ADDR_COPY(peer_ni->ni_bssid, vap->iv_bss->ni_bssid);
		}

		ieee80211_free_node(peer_ni);
	}
}

/*
 * Process a Setup Response
 */
void
ieee80211_tdls_recv_setup_resp(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211_node *peer_ni;
	struct ieee80211vap *vap = ni->ni_vap;

	if (!tdls || (ieee80211_tdls_validate_params(ni, tdls) != 0))
		return;

	/* A setup response may be unsolicited.  Find or create the peer node. */
	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni != NULL) {
		peer_ni->ni_rssi = rssi;
		peer_ni->tdls_last_seen = jiffies;
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS: received SETUP RESP from peer %pM at %u, rssi=%d\n",
			peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

		peer_ni->ni_vendor = PEER_VENDOR_NONE;
		if (tdls->qtn_info != NULL)
			peer_ni->ni_vendor = PEER_VENDOR_QTN;

		peer_ni->tdls_initiator = 1;

		if (tdls->aid != NULL)
			peer_ni->tdls_peer_associd = le16toh(tdls->aid->aid);

		if (tdls->qtn_brmacs != NULL) {
			if (peer_ni->ni_qtn_brmacs == NULL)
				MALLOC(peer_ni->ni_qtn_brmacs, uint8_t *,
					IEEE80211_MAX_IE_LEN, M_DEVBUF, M_WAITOK);
			if (peer_ni->ni_qtn_brmacs != NULL)
				memcpy(peer_ni->ni_qtn_brmacs, tdls->qtn_brmacs,
					tdls->qtn_brmacs[1] + 2);
		}

		ieee80211_free_node(peer_ni);
	}
}

/*
 * Process a Discovery Request
 */
void
ieee80211_tdls_recv_disc_req(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node *peer_ni;

	if (tdls == NULL) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Required TLV is missing\n", __func__);
		return;
	}

	if (ieee80211_tdls_validate_params(ni, tdls) != 0)
		return;

	if (memcmp(ni->ni_vap->iv_myaddr, tdls->sa, IEEE80211_ADDR_LEN) == 0)
		return;

	/* when tdls function is prohibited, ignore discovery request */
	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: receive discover request, but tdls function prohibited, drop it\n", __func__);
		return;
	}

	/* Find or create the peer node */
	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni) {
		peer_ni->tdls_last_seen = jiffies;
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS: received DISC REQUEST from peer %pM at %u, rssi=%d\n",
			peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

		if (IEEE80211_NODE_IS_TDLS_ACTIVE(peer_ni)) {
			ieee80211_tdls_add_clients_to_bridge(ni, peer_ni, tdls->qtn_brmacs);
		} else if (tdls->qtn_brmacs != NULL) {
			if (peer_ni->ni_qtn_brmacs == NULL)
				MALLOC(peer_ni->ni_qtn_brmacs, uint8_t *,
					IEEE80211_MAX_IE_LEN, M_DEVBUF, M_WAITOK);
			if (peer_ni->ni_qtn_brmacs != NULL)
				memcpy(peer_ni->ni_qtn_brmacs, tdls->qtn_brmacs,
					tdls->qtn_brmacs[1] + 2);
		}

		ieee80211_free_node(peer_ni);
	}
}

/*
 * Process a channel switch Request
 */
void
ieee80211_tdls_recv_chan_switch_req(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node *peer_ni;
	struct ieee80211_tdls_action_data *data;
	uint16_t status = 0;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: drop channel switch req since tdls prohibited\n", __func__);
		return;
	}

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: drop channel switch req since tdls chan switch prohibited\n", __func__);
		return;
	}

	if (ieee80211_tdls_validate_params(ni, tdls) != 0)
		return;

	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni) {
		peer_ni->ni_rssi = rssi;
		peer_ni->tdls_last_seen = jiffies;
		peer_ni->tdls_no_send_cs_resp = 1;

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS: received CHAN SWITCH REQUEST from peer %pM at %u, rssi=%d\n",
			peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

		MALLOC(data, struct ieee80211_tdls_action_data *,
					sizeof(struct ieee80211_tdls_action_data) +
					sizeof(struct ieee80211_tdls_cs_timing),
					M_DEVBUF, M_WAITOK);

		if (ieee80211_tdls_check_target_chan(peer_ni, tdls)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS: Reject channel switch req from peer: %pM due to invalid"
					" target channel: %u\n", peer_ni->ni_macaddr, tdls->target_chan);
			status = IEEE80211_STATUS_PEER_MECHANISM_REJECT;
		}

		if (ieee80211_tdls_check_reg_class(peer_ni, tdls)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS: Reject channel switch reqt from peer: %pM due to invalid"
					" reglatory class: %d \n", peer_ni->ni_macaddr, tdls->reg_class);
			status = IEEE80211_STATUS_PEER_MECHANISM_REJECT;
		}

		if (ieee80211_tdls_check_wide_bw_cs(peer_ni, tdls)) {
			if (ieee80211_tdls_check_2nd_chan_off(peer_ni, tdls)) {
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS: Reject channel switch requset from peer: %pM due to invalid"
					" second channel offset: %d \n", peer_ni->ni_macaddr, tdls->sec_chan_off[2]);
				status = IEEE80211_STATUS_PEER_MECHANISM_REJECT;
			}
		}

		if (ieee80211_tdls_check_link_id(peer_ni, tdls, IEEE80211_ACTION_TDLS_CS_REQ)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS: Reject channel switch requset from peer: %pM due to invalid"
					" link id\n", peer_ni->ni_macaddr);
			status = IEEE80211_STATUS_PEER_MECHANISM_REJECT;
		}

		if (ieee80211_tdls_check_chan_switch_timing(peer_ni, tdls)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS: Reject channel switch requset from peer: %pM due to invalid"
					" switch timing parameter\n", peer_ni->ni_macaddr);
			status = IEEE80211_STATUS_PEER_MECHANISM_REJECT;
		}

		memcpy(data->dest_mac, peer_ni->ni_macaddr, sizeof(data->dest_mac));
		data->status = cpu_to_le16(status);
		data->dtoken = tdls->target_chan;
		data->ie_buflen = sizeof(struct ieee80211_tdls_cs_timing);
		memcpy(data->ie_buf, tdls->cs_timing, data->ie_buflen);
		ieee80211_tdls_send_chan_switch_resp(peer_ni, data);

		FREE(data, M_DEVBUF);

		ieee80211_free_node(peer_ni);
	}
}

/*
 * Process a channel switch Response
 */
void
ieee80211_tdls_recv_chan_switch_resp(struct ieee80211_node *ni,
	struct sk_buff *skb, int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_tdls_cs_timing *cs_timing =
		(struct ieee80211_tdls_cs_timing *)tdls->cs_timing;
	struct ieee80211_node *peer_ni;
	int tar_chan;
	int chan_bw;
	uint64_t cur_tsf;
	uint64_t start_tsf;
	uint64_t tbtt;
	uint32_t duration = 0;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: drop channel switch resp since tdls prohibited\n", __func__);
		return;
	}

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: drop channel switch resp since tdls chan switch"
			" prohibited\n", __func__);
		return;
	}

	if (ieee80211_tdls_validate_params(ni, tdls) != 0) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: TDLS parameters verification fails\n", __func__);
		return;
	}

	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni != NULL) {
		peer_ni->ni_rssi = rssi;
		peer_ni->tdls_last_seen = jiffies;

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS: received CHAN SWITCH RESP from peer %pM at %u, rssi=%d\n",
			peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

		if (tdls->status == 0) {
			ic->ic_get_tsf(&cur_tsf);
			/*
			 * TDLS link should be returned back base channel
			 * on reception unsolicited TDLS channel switch response
			 */
			if ((ni->tdls_send_cs_req == 0) &&
				(vap->tdls_chan_switching == 1) &&
				(vap->tdls_cs_time == cs_timing->switch_time) &&
				(vap->tdls_cs_timeout == cs_timing->switch_timeout)) {
				tar_chan = ic->ic_bsschan->ic_ieee;
				chan_bw = ieee80211_get_bw(ic);
			} else {
				tar_chan = vap->tdls_target_chan;
				chan_bw = vap->tdls_off_chan_bw;
			}

			vap->tdls_cs_time = cs_timing->switch_time;
			vap->tdls_cs_timeout = cs_timing->switch_timeout;

			tbtt = vap->iv_bss->ni_shared_stats->dtim_tbtt;
			start_tsf = cur_tsf + cs_timing->switch_time + DEFAULT_TDLS_CH_SW_TX_TIME;
			if (tbtt > (start_tsf + DEFAULT_TDLS_CH_SW_PROC_TIME))
				duration = (uint32_t)(tbtt - start_tsf - DEFAULT_TDLS_CH_SW_PROC_TIME);
			duration = MAX(duration, cs_timing->switch_timeout - cs_timing->switch_time);
			vap->tdls_cs_duration = duration;

			ieee80211_sta_pwrsave(vap, 1);
			if (ieee80211_tdls_remain_on_channel(vap, peer_ni, tar_chan,
					chan_bw, start_tsf, vap->tdls_cs_duration) != 0) {
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS %s: Peer %pM channel switch fails\n", __func__, peer_ni->ni_macaddr);
				ieee80211_sta_pwrsave(vap, 0);
			}
		} else {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS %s: Peer %pM rejects channel switch requset\n",
					__func__, peer_ni->ni_macaddr);
		}

		peer_ni->tdls_send_cs_req = 0;
		ieee80211_free_node(peer_ni);
	}
}

/*
 * Make TDLS link switch to base channel
 * Returns:
 *   1 - returned to base channel
 *   0 - failed to return to base channel
 */
int
ieee80211_tdls_return_to_base_channel(struct ieee80211vap *vap, int ap_disassoc)
{
#define	IEEE80211_TDLS_RET_BASE_CHAN_WAIT_TIME	5
#define	IEEE80211_TDLS_RET_BASE_CHAN_WAIT_CYCL	10
	struct ieee80211com *ic = vap->iv_ic;
	int tar_chan = ic->ic_bsschan->ic_ieee;
	int chan_bw = 0;
	uint8_t count = 0;
	uint64_t cur_tsf;
	uint64_t start_tsf;

	if ((vap->tdls_chan_switching == 0) ||
			(vap->tdls_cs_node == NULL))
		return 1;

	if (ap_disassoc)
		vap->tdls_cs_disassoc_pending = 1;

	IEEE80211_TDLS_DPRINTF(vap,
		IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
		"TDLS %s: TDLS link with peer %pM needs to return base channel,"
		" disassoc_peeding = %d\n", __func__,
		vap->tdls_cs_node->ni_macaddr, vap->tdls_cs_disassoc_pending);

	ic->ic_get_tsf(&cur_tsf);
	start_tsf = cur_tsf + DEFAULT_TDLS_CH_SW_PROC_TIME;
	ieee80211_tdls_remain_on_channel(vap, vap->tdls_cs_node, tar_chan,
			chan_bw, start_tsf, vap->tdls_cs_duration);

	if (!in_interrupt()) {
		while (vap->tdls_chan_switching == 1) {
			msleep(IEEE80211_TDLS_RET_BASE_CHAN_WAIT_TIME);
			if (count++ > IEEE80211_TDLS_RET_BASE_CHAN_WAIT_CYCL) {
				IEEE80211_TDLS_DPRINTF(vap,
					IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS %s: TDLS link with peer %pM failed to return"
					" to base channel\n", __func__,
					vap->tdls_cs_node->ni_macaddr);
				break;
			}
		}
	}

	if (vap->tdls_chan_switching == 0) {
		vap->tdls_cs_disassoc_pending = 0;
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(ieee80211_tdls_return_to_base_channel);

/*
 * Process a Setup Confirm
 */
void
ieee80211_tdls_recv_setup_confirm(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211_node *peer_ni;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;

	if (!tdls || (ieee80211_tdls_validate_params(ni, tdls) != 0)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: TDLS parameters verification fails\n", __func__);
		return;
	}

	peer_ni = ieee80211_tdls_find_or_create_peer(ni, tdls->sa, tdls);
	if (peer_ni == NULL)
		return;

	peer_ni->tdls_initiator = 0;
	peer_ni->tdls_last_seen = jiffies;
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS: received SETUP CONFIRM from peer %pM at %u, rssi=%d\n",
		peer_ni->ni_macaddr, peer_ni->tdls_last_seen, rssi);

	if (tdls->htinfo)
		ieee80211_parse_htinfo(peer_ni, tdls->htinfo);

	if (IS_IEEE80211_VHT_ENABLED(ic) && tdls->vhtop)
		ieee80211_parse_vhtop(peer_ni, tdls->vhtop);

	if (tdls->qtn_brmacs != NULL) {
		if (peer_ni->ni_qtn_brmacs == NULL)
			MALLOC(peer_ni->ni_qtn_brmacs, uint8_t *,
				IEEE80211_MAX_IE_LEN, M_DEVBUF, M_WAITOK);
		if (peer_ni->ni_qtn_brmacs != NULL)
			memcpy(peer_ni->ni_qtn_brmacs, tdls->qtn_brmacs,
			tdls->qtn_brmacs[1] + 2);
	}

	ieee80211_free_node(peer_ni);
}

void
ieee80211_tdls_recv_teardown(struct ieee80211_node *ni, struct sk_buff *skb,
	int rssi, struct ieee80211_tdls_params *tdls)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (ieee80211_tdls_validate_params(ni, tdls) != 0) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: TDLS parameters verification fails\n", __func__);
		return;
	}

	ni->tdls_last_seen = jiffies;
	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS: received TEARDOWN from peer %pM at %u, rssi=%d\n", tdls->sa,
		ni->tdls_last_seen, rssi);
}

int
ieee80211_tdls_send_action_frame(struct net_device *ndev,
		struct ieee80211_tdls_action_data *data)
{
	struct ieee80211vap *vap = netdev_priv(ndev);
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *peer_ni;
	int ret = 0;

	if (data == NULL) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Action data is NULL\n", __func__);
		return -1;
	}

	if (vap->iv_opmode != IEEE80211_M_STA) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: vap is not in STA mode\n", __func__);
		return -1;
	}

	if (ieee80211_tdls_validate_vap_state(vap) != 0) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: vap is not in correct state\n", __func__);
		return -1;
	}

	if (IEEE80211_ADDR_EQ(vap->iv_bss->ni_macaddr, data->dest_mac)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Should not send to BSS\n", __func__);
		return -1;
	}

	if (is_multicast_ether_addr(data->dest_mac)) {
		if (data->action == IEEE80211_ACTION_TDLS_DISC_REQ) {
			peer_ni = vap->iv_bss;
			ieee80211_ref_node(peer_ni);
		} else {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: Dest address of %s action must be unicast\n",
				__func__, ieee80211_tdls_action_name_get(data->action));
			return -1;
		}
	} else {
		if ((data->action == IEEE80211_ACTION_TDLS_TEARDOWN) ||
				(data->action == IEEE80211_ACTION_TDLS_PTI))
			peer_ni = ieee80211_find_node(&ic->ic_sta, data->dest_mac);
		else
			peer_ni = ieee80211_tdls_find_or_create_peer(ni, data->dest_mac, NULL);
		if (peer_ni == NULL) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: Peer node is not found\n", __func__);
			return -1;
		}
	}

	switch (data->action) {
	case IEEE80211_ACTION_TDLS_SETUP_REQ:
		ret = ieee80211_tdls_send_setup_req(peer_ni, data);
		ieee80211_free_node(peer_ni);
		break;
	case IEEE80211_ACTION_TDLS_SETUP_RESP:
		ret = ieee80211_tdls_send_setup_resp(peer_ni, data);
		ieee80211_free_node(peer_ni);
		break;
	case IEEE80211_ACTION_TDLS_SETUP_CONFIRM:
		ret = ieee80211_tdls_send_setup_confirm(peer_ni, data);
		ieee80211_free_node(peer_ni);
		break;
	case IEEE80211_ACTION_TDLS_TEARDOWN:
		ret = ieee80211_tdls_send_teardown(peer_ni, data);
		ieee80211_free_node(peer_ni);
		break;
	case IEEE80211_ACTION_TDLS_DISC_REQ:
		ret = ieee80211_tdls_send_disc_req(peer_ni, data);
		ieee80211_free_node(peer_ni);
		break;
	case IEEE80211_ACTION_PUB_TDLS_DISC_RESP:
		ret = ieee80211_tdls_send_disc_resp(peer_ni, data);
		break;
	case IEEE80211_ACTION_TDLS_PTI:
		ret = ieee80211_tdls_send_pti_req(peer_ni, data);
		ieee80211_free_node(peer_ni);
		break;
	default:
		ieee80211_free_node(peer_ni);
		break;
	}

	return ret;
}

/*
 * Periodically send TDLS discovery requests
 */
void
ieee80211_tdls_trigger_rate_detection(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: discovery timeout\n", __func__);

	if (ieee80211_tdls_validate_vap_state(vap) != 0) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: not sending disc req while not associated\n", __func__);
	} else {
		ieee80211_tdls_send_disc_req(vap->iv_bss, NULL);
		schedule_delayed_work(&vap->tdls_rate_detect_work,
				DEFAULT_TDLS_RATE_DETECTION_WAITING_T * HZ);
		mod_timer(&vap->tdls_rate_detect_timer,
				jiffies + vap->tdls_discovery_interval * HZ);
	}
}

static void
ieee80211_tdls_bottom_half_rate_detetion(struct work_struct *work)
{
	struct delayed_work *dwork = (struct delayed_work *)work;
	struct ieee80211vap *vap =
			container_of(dwork, struct ieee80211vap, tdls_rate_detect_work);
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	struct ieee80211_node *next;
	uint8_t random;
	int mu = STATS_SU;

	get_random_bytes(&random, sizeof(random));

	if (vap->tdls_path_sel_prohibited == 0) {
		if (vap->iv_bss->ni_shared_stats->tx[mu].pkts_per_sec <
					vap->tdls_path_sel_pps_thrshld)
			ieee80211_tdls_add_rate_detection(vap->iv_bss);

		TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
			/*
			 * Don't send training packets to 3rd part TDLS peer before
			 * TDLS peer is established since it could cause 3rd part TDLS
			 * peer send deauth frame.
			 */
			if (!ni->ni_qtn_assoc_ie &&
					!IEEE80211_NODE_IS_TDLS_ACTIVE(ni))
				continue;

			if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
				if ((ni->tdls_initiator == 1) &&
					(ni->ni_shared_stats->tx[mu].pkts_per_sec <
						vap->tdls_path_sel_pps_thrshld))
					ieee80211_tdls_add_rate_detection(ni);
			} else if (!IEEE80211_NODE_IS_NONE_TDLS(ni)) {
				ieee80211_tdls_add_rate_detection(ni);
			}
		}

		ieee80211_tdls_peer_ps_info_decre(vap);
	}

	schedule_delayed_work(&vap->tdls_link_switch_work,
			(DEFAULT_TDLS_RATE_DETECTION_WAITING_T + (random % 10)) * HZ);
}

static void
ieee80211_tdls_data_link_switch(struct work_struct *work)
{
	struct delayed_work *dwork = (struct delayed_work *)work;
	struct ieee80211vap *vap =
			container_of(dwork, struct ieee80211vap, tdls_link_switch_work);
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	enum ieee80211_tdls_operation operation;
	struct tdls_peer_ps_info *peer_ps_info = NULL;
	int link_switch = -1;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Check if need to switch data link\n", __func__);

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (IEEE80211_NODE_IS_TDLS_STARTING(ni) &&
				(time_after(jiffies, ni->tdls_setup_start +
					DEFAULT_TDLS_SETUP_EXPIRE_DURATION * HZ)))
			ieee80211_tdls_disable_peer_link(ni);

		if (IEEE80211_NODE_IS_NONE_TDLS(ni) ||
				IEEE80211_NODE_IS_TDLS_STARTING(ni))
			continue;

		if (vap->tdls_path_sel_prohibited == 0) {
			peer_ps_info = ieee80211_tdls_find_or_create_peer_ps_info(vap, ni);
			if ((peer_ps_info) && (peer_ps_info->tdls_link_disabled_ints > 0)) {
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
					"TDLS %s: Peer %pM status: %d, disabled_ints: %d\n",
					__func__, ni->ni_macaddr, ni->tdls_status,
					peer_ps_info->tdls_link_disabled_ints);
				continue;
			}

			if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni) &&
					(ni->tdls_initiator == 0)) {
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
					"TDLS %s: Peer %pM status: %d, initiator: %d\n", __func__,
					ni->ni_macaddr, ni->tdls_status, ni->tdls_initiator);
				continue;
			}
		}

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Peer %pM status: %d, rssi: %d, disabled_ints: %d\n", __func__,
			ni->ni_macaddr, ni->tdls_status, ieee80211_tdls_get_smoothed_rssi(vap, ni),
			(peer_ps_info == NULL) ? 0 : peer_ps_info->tdls_link_disabled_ints);

		link_switch = ieee80211_tdls_data_path_selection(vap, ni);
		if (link_switch == 1) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: Setting up TDLS link with peer %pM\n",
				__func__, ni->ni_macaddr);

			if (peer_ps_info != NULL) {
				peer_ps_info->tdls_path_down_cnt = 0;
				peer_ps_info->tdls_link_disabled_ints = 0;

				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS %s: Clear path selection info, path_down_cnt = %d,"
					" link_disabled_ints = %d\n", __func__,
					peer_ps_info->tdls_path_down_cnt,
					peer_ps_info->tdls_link_disabled_ints);
			}

			if (!IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
				operation = IEEE80211_TDLS_SETUP;
				ieee80211_tdls_send_event(ni, IEEE80211_EVENT_TDLS, &operation);
			}
		} else if (link_switch == 0) {
			if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
					"TDLS %s: tearing down TDLS link with peer %pM\n",
					__func__, ni->ni_macaddr);

				if (peer_ps_info != NULL) {
					peer_ps_info->tdls_path_down_cnt++;
					peer_ps_info->tdls_link_disabled_ints = DEFAULT_TDLS_LINK_DISABLE_SCALE *
									peer_ps_info->tdls_path_down_cnt;

					IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
							"TDLS %s: Set path selection info, path_down_cnt = %d,"
							" link_disabled_ints = %d\n", __func__,
							peer_ps_info->tdls_path_down_cnt,
							peer_ps_info->tdls_link_disabled_ints);
				}

				operation = IEEE80211_TDLS_TEARDOWN;
				ieee80211_tdls_send_event(ni, IEEE80211_EVENT_TDLS, &operation);
			}
		}
	}
	IEEE80211_NODE_UNLOCK(nt);
}

/*
 * Start or stop periodic TDLS discovery
 *   Start broadcasting TDLS discovery frames every <value> seconds.
 *   A value of 0 stops TDLS discovery.
 *   Returns 0 if config applied, else 1.
 */
int
ieee80211_tdls_cfg_disc_int(struct ieee80211vap *vap, int value)
{
	struct net_device *dev = vap->iv_dev;
	unsigned int pre_disc_interval = 0;

	if (vap->iv_opmode != IEEE80211_M_STA) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"%s: TDLS: discovery is only supported on STA nodes\n",
					dev->name);
			return -1;
	}

	/* TDLS recheck after assoc in case security mode changes */
	if (!ieee80211_tdls_sec_mode_valid(vap)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"%s: TDLS: not allowed when using TKIP\n",
					dev->name);
			return -1;
	}

	if (value <= 0)
		value = 0;

	pre_disc_interval = vap->tdls_discovery_interval;
	vap->tdls_discovery_interval = value;

	if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0) {
		if ((pre_disc_interval == 0) && (vap->tdls_discovery_interval > 0))
			mod_timer(&vap->tdls_rate_detect_timer, jiffies + HZ);

		if ((vap->tdls_discovery_interval == 0) && timer_pending(&vap->tdls_rate_detect_timer)) {
			del_timer(&vap->tdls_rate_detect_timer);
			cancel_rearming_delayed_work(&vap->tdls_rate_detect_work);
			cancel_rearming_delayed_work(&vap->tdls_link_switch_work);

			ieee80211_tdls_free_peer_ps_info(vap);
		}
	}

	return 0;
}

int
ieee80211_tdls_enable_peer_link(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(ni, 0);

	if (!(ni->ni_ext_flags & IEEE80211_NODE_TDLS_AUTH)) {
		ieee80211_sta_assocs_inc(vap, __func__);
		ieee80211_nonqtn_sta_join(vap, ni, __func__);
	}

	if (ni->ni_qtn_assoc_ie && ni->ni_implicit_ba_valid)
		ieee80211_node_implicit_ba_setup(ni);

	ieee80211_tdls_set_key(vap, ni);

	ieee80211_tdls_update_node_status(ni, IEEE80211_TDLS_NODE_STATUS_ACTIVE);
	_ieee80211_node_authorize(ni);
	ni->ni_ext_flags |= IEEE80211_NODE_TDLS_AUTH;
	ieee80211_tdls_set_link_timeout(vap, ni);

	ieee80211_tdls_add_bridge_entry_for_peer(ni);
	ieee80211_tdls_add_clients_to_bridge(vap->iv_bss, ni, ni->ni_qtn_brmacs);

	printk(KERN_INFO "%s: TDLS peer %s associated, tot=%u/%u\n",
		vap->iv_dev->name, ether_sprintf(ni->ni_macaddr),
		ic->ic_sta_assoc, ic->ic_nonqtn_sta);

	return 0;
}

int
ieee80211_tdls_node_leave(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	ni->ni_chan_num = 0;
	memset(ni->ni_supp_chans, 0, sizeof(ni->ni_supp_chans));
	ieee80211_tdls_del_key(vap, ni);
	ieee80211_tdls_update_node_status(ni, IEEE80211_TDLS_NODE_STATUS_INACTIVE);

	ieee80211_node_leave(ni);

	return 0;
}

int
ieee80211_tdls_teardown_all_link(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	enum ieee80211_tdls_operation operation;
	if (vap->iv_opmode != IEEE80211_M_STA)
		return -1;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (!IEEE80211_NODE_IS_NONE_TDLS(ni)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: tearing down TDLS link with peer %pM\n",__func__, ni->ni_macaddr);
			operation = IEEE80211_TDLS_TEARDOWN;
			ieee80211_tdls_send_event(ni, IEEE80211_EVENT_TDLS, &operation);
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	return 0;
}

int
ieee80211_tdls_free_all_inactive_peers(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni, *next;
	if (vap->iv_opmode != IEEE80211_M_STA)
		return -1;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
		if (!ieee80211_node_is_running(ni)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
					"TDLS %s: free TDLS peer %pM\n", __func__, ni->ni_macaddr);
			ieee80211_tdls_node_leave(vap, ni);
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	return 0;
}

int
ieee80211_tdls_free_all_peers(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni, *next;
	if (vap->iv_opmode != IEEE80211_M_STA)
		return -1;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
		if (!IEEE80211_NODE_IS_NONE_TDLS(ni)) {
			ieee80211_tdls_disable_peer_link(ni);

			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
					"TDLS %s: free TDLS peer %pM\n", __func__, ni->ni_macaddr);
			ieee80211_tdls_node_leave(vap, ni);
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	return 0;
}

int
ieee80211_tdls_init_disc_timer(struct ieee80211vap *vap)
{
	init_timer(&vap->tdls_rate_detect_timer);
	vap->tdls_rate_detect_timer.function = ieee80211_tdls_trigger_rate_detection;
	vap->tdls_rate_detect_timer.data = (unsigned long) vap;
	INIT_DELAYED_WORK(&vap->tdls_rate_detect_work, ieee80211_tdls_bottom_half_rate_detetion);
	INIT_DELAYED_WORK(&vap->tdls_link_switch_work, ieee80211_tdls_data_link_switch);

	return 0;
}

int
ieee80211_tdls_clear_disc_timer(struct ieee80211vap *vap)
{
	if (vap == NULL)
		return -1;

	if (vap->tdls_discovery_interval > 0) {
		del_timer(&vap->tdls_rate_detect_timer);
		cancel_rearming_delayed_work(&vap->tdls_rate_detect_work);
		cancel_rearming_delayed_work(&vap->tdls_link_switch_work);

		ieee80211_tdls_free_peer_ps_info(vap);
	}

	return 0;
}

int
ieee80211_tdls_start_disc_timer(struct ieee80211vap *vap)
{
	if (vap == NULL)
		return -1;

	if (vap->iv_opmode != IEEE80211_M_STA)
		return -1;

	/* TDLS recheck after assoc in case security mode changes */
	if (!ieee80211_tdls_sec_mode_valid(vap))
		return -1;

	if (vap->tdls_discovery_interval > 0)
		mod_timer(&vap->tdls_rate_detect_timer, jiffies + HZ);

	return 0;
}

void
ieee80211_tdls_node_expire(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	int ni_expired;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (IEEE80211_NODE_IS_NONE_TDLS(ni))
		      continue;

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			 "TDLS: peer %pM, last_seen: %u, Now: %u\n",
			 ni->ni_macaddr, ni->tdls_last_seen, jiffies);

		if (IEEE80211_NODE_IS_TDLS_STARTING(ni) &&
				(time_after(jiffies, ni->tdls_setup_start +
					DEFAULT_TDLS_SETUP_EXPIRE_DURATION * HZ)))
			ieee80211_tdls_disable_peer_link(ni);

		if (IEEE80211_NODE_IS_TDLS_INACTIVE(ni) ||
				IEEE80211_NODE_IS_TDLS_IDLE(ni)) {
			ni_expired = time_after(jiffies,
					ni->tdls_last_seen + vap->tdls_node_life_cycle * HZ);
			if (ni_expired)
				ieee80211_tdls_node_leave(vap, ni);
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	mod_timer(&vap->tdls_node_expire_timer,
			jiffies + vap->tdls_node_life_cycle * HZ);
}

int
ieee80211_tdls_start_node_expire_timer(struct ieee80211vap *vap)
{
	if (vap->iv_opmode != IEEE80211_M_STA)
		return -1;

	if (vap->tdls_node_life_cycle > 0) {
		if (timer_pending(&vap->tdls_node_expire_timer))
			del_timer(&vap->tdls_node_expire_timer);

		mod_timer(&vap->tdls_node_expire_timer,
			jiffies + vap->tdls_node_life_cycle * HZ);
	}

	return 0;
}

int
ieee80211_tdls_init_node_expire_timer(struct ieee80211vap *vap)
{
	if (vap == NULL)
		return -1;

	init_timer(&vap->tdls_node_expire_timer);
	vap->tdls_node_expire_timer.function = ieee80211_tdls_node_expire;
	vap->tdls_node_expire_timer.data = (unsigned long) vap;

	if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0)
		ieee80211_tdls_start_node_expire_timer(vap);

	return 0;
}

int
ieee80211_tdls_clear_node_expire_timer(struct ieee80211vap *vap)
{
	if (vap)
		del_timer(&vap->tdls_node_expire_timer);

	return 0;
}

void
ieee80211_tdls_all_peer_disabled(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	int all_disabled = 1;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
			all_disabled = 0;
			break;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	if (all_disabled) {
		del_timer(&vap->tdls_disassoc_timer);
		ieee80211_new_state(vap, vap->tdls_pending_state,
					vap->tdls_pending_arg);
	} else {
		mod_timer(&vap->tdls_disassoc_timer, jiffies + HZ / 2);
	}
}

int
ieee80211_tdls_init_disassoc_pending_timer(struct ieee80211vap *vap)
{
	if (vap == NULL)
		return -1;

	init_timer(&vap->tdls_disassoc_timer);
	vap->tdls_disassoc_timer.function = ieee80211_tdls_all_peer_disabled;
	vap->tdls_disassoc_timer.data = (unsigned long)vap;

	return 0;
}

int
ieee80211_tdls_pend_disassociation(struct ieee80211vap *vap,
	enum ieee80211_state nstate, int arg)
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	int need_pending = 0;

	if ((vap->iv_opmode == IEEE80211_M_STA) && (vap->iv_state == IEEE80211_S_RUN) &&
			((nstate == IEEE80211_S_INIT) || (nstate == IEEE80211_S_AUTH) ||
				(nstate == IEEE80211_S_ASSOC))) {
		IEEE80211_NODE_LOCK(nt);
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
			if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
				need_pending = 1;
				break;
			}
		}
		IEEE80211_NODE_UNLOCK(nt);

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: pend disassoication with AP, nstate: %d\n", __func__, nstate);

		if (need_pending) {
			ieee80211_tdls_teardown_all_link(vap);

			vap->tdls_pending_state = nstate;
			vap->tdls_pending_arg = arg;
			mod_timer(&vap->tdls_disassoc_timer, jiffies + HZ / 2);
		}
	}

	return need_pending;
}
EXPORT_SYMBOL(ieee80211_tdls_pend_disassociation);

int
ieee80211_tdls_set_link_timeout(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	uint16_t elapsed_count = 0;

	if ((vap == NULL) || (ni == NULL))
		return -1;

	if (vap->iv_opmode != IEEE80211_M_STA)
		return -1;

	elapsed_count = ni->ni_inact_reload - ni->ni_inact;
	if ((vap->tdls_timeout_time % IEEE80211_INACT_WAIT) != 0)
		ni->ni_inact_reload = vap->tdls_timeout_time / IEEE80211_INACT_WAIT + 1;
	else
		ni->ni_inact_reload = vap->tdls_timeout_time / IEEE80211_INACT_WAIT;

	if (ni->ni_inact_reload > elapsed_count)
		ni->ni_inact = ni->ni_inact_reload - elapsed_count;
	else
		ni->ni_inact = IEEE80211_INACT_SEND_PKT_THRSH;

	return 0;
}

void
ieee80211_tdls_update_node_status(struct ieee80211_node *ni, enum ni_tdls_status stats)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (ni->tdls_status != stats) {
		ni->tdls_status = stats;
		vap->iv_ic->ic_set_tdls_param(ni, IOCTL_TDLS_STATUS, (int)ni->tdls_status);
	}
}

int
ieee80211_tdls_start_channel_switch(struct ieee80211vap *vap,
		struct ieee80211_node *peer_ni)
{
	int ret = 0;

	if ((!vap) || (vap->iv_opmode != IEEE80211_M_STA))
		return 1;

	if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: Channel switch function has been prohibited\n", __func__);
		return 1;
	}

	if (!ieee80211_tdls_channel_switch_allowed(vap)) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: Don't start channel switch due to "
			"fail to enter power save state\n",__func__);
		return 1;
	}

	ret = ieee80211_tdls_send_chan_switch_req(peer_ni, NULL);

	return ret;
}

void
ieee80211_tdls_vattach(struct ieee80211vap *vap)
{
	ieee80211_tdls_init_disc_timer(vap);
	ieee80211_tdls_init_node_expire_timer(vap);
	ieee80211_tdls_init_disassoc_pending_timer(vap);
	ieee80211_tdls_update_uapsd_indicication_windows(vap);
}

void
ieee80211_tdls_vdetach(struct ieee80211vap *vap)
{
	del_timer(&vap->tdls_rate_detect_timer);
	del_timer(&vap->tdls_node_expire_timer);
	cancel_rearming_delayed_work(&vap->tdls_rate_detect_work);
	cancel_rearming_delayed_work(&vap->tdls_link_switch_work);
	del_timer(&vap->tdls_disassoc_timer);

	ieee80211_tdls_free_peer_ps_info(vap);
	ieee80211_tdls_free_all_peers(vap);
}

/* update tdls link timeout time for the peers who has established tdls link with station */
int ieee80211_tdls_update_link_timeout(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt = NULL;
	struct ieee80211_node *ni = NULL;
	uint16_t elapsed_count = 0;

	if ((NULL == vap) || (vap->iv_opmode != IEEE80211_M_STA))
		return -1;

	nt = &vap->iv_ic->ic_sta;

	IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
		"TDLS %s: update link timeout time [%u]\n", __func__, vap->tdls_timeout_time);

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni) ||
				IEEE80211_NODE_IS_TDLS_IDLE(ni)) {
			elapsed_count = ni->ni_inact_reload - ni->ni_inact;
			if ((vap->tdls_timeout_time % IEEE80211_INACT_WAIT) != 0)
				ni->ni_inact_reload = vap->tdls_timeout_time / IEEE80211_INACT_WAIT + 1;
			else
				ni->ni_inact_reload = vap->tdls_timeout_time / IEEE80211_INACT_WAIT;

			if (ni->ni_inact_reload > elapsed_count)
				ni->ni_inact = ni->ni_inact_reload - elapsed_count;
			else
				ni->ni_inact = IEEE80211_INACT_SEND_PKT_THRSH;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	return 0;
}


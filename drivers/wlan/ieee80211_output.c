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
 * $Id: ieee80211_output.c 2606 2007-07-25 15:14:52Z mrenzmann $
 */
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 output handling.
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <linux/if_arp.h>		/* for ARP proxy */
#include <linux/ip.h>			/* XXX for TOS */
#include <net/iw_handler.h>		/* wireless_send_event(..) */

#include "qtn/qtn_global.h"
#include "qtn/shared_params.h"
#include "qtn/hardware_revision.h"

#include "net80211/if_llc.h"
#include "net80211/if_ethersubr.h"
#include "net80211/if_media.h"
#include "net80211/ieee80211.h"

#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_dot11_msg.h"
#include "net80211/ieee80211_monitor.h"
#include "net80211/ieee80211_tdls.h"

#include "qtn_logging.h"

#ifdef IEEE80211_DEBUG
/*
 * Decide if an outbound management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static __inline int
doprint(struct ieee80211vap *vap, int subtype)
{
	if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
		return (vap->iv_opmode == IEEE80211_M_IBSS);
	return 1;
}
#endif

#define	senderr(_x, _v)	do { vap->iv_stats._v++; ret = (_x); goto bad; } while (0)

/*
 * Add the Quantenna OUI to a frame
 */
uint8_t
ieee80211_oui_add_qtn(uint8_t *oui)
{
	oui[0] = QTN_OUI & 0xff;
	oui[1] = (QTN_OUI >> 8) & 0xff;
	oui[2] = (QTN_OUI >> 16) & 0xff;

	return IEEE80211_OUI_LEN;
}
EXPORT_SYMBOL(ieee80211_oui_add_qtn);

void ieee80211_parent_queue_xmit(struct sk_buff *skb)
{
	struct ieee80211vap *vap = netdev_priv(skb->dev);

	skb->dev = vap->iv_dev;

	dev_queue_xmit(skb);
}

/*
 * Initialise an 802.11 header.
 * This should be called early on in constructing a frame as it sets i_fc[1]. Other bits can then be
 * OR'd in.
 */
static void
ieee80211_send_setup(struct ieee80211vap *vap, struct ieee80211_node *ni,
			struct ieee80211_frame *wh,
			const uint8_t type, const uint8_t subtype,
			const uint8_t *sa, const uint8_t *da, const uint8_t *bssid)
{
#define	WH4(wh)	((struct ieee80211_frame_addr4 *)wh)

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | type | subtype;
	if (type == IEEE80211_FC0_TYPE_DATA) {
		switch (vap->iv_opmode) {
		case IEEE80211_M_STA:
			wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr2, sa);
			IEEE80211_ADDR_COPY(wh->i_addr3, da);
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, da);
			IEEE80211_ADDR_COPY(wh->i_addr2, sa);
			IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
			break;
		case IEEE80211_M_HOSTAP:
			wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
			IEEE80211_ADDR_COPY(wh->i_addr1, da);
			IEEE80211_ADDR_COPY(wh->i_addr2, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr3, sa);
			break;
		case IEEE80211_M_WDS:
			wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, bssid); /* bssid holds RA */
			IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
			IEEE80211_ADDR_COPY(wh->i_addr3, da);
			IEEE80211_ADDR_COPY(WH4(wh)->i_addr4, sa);
			break;
		case IEEE80211_M_MONITOR:
			break;
		}
	} else {
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, da);
		IEEE80211_ADDR_COPY(wh->i_addr2, sa);
		IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
	}
	wh->i_dur[0] = 0;
	wh->i_dur[1] = 0;

	if (!(subtype & IEEE80211_FC0_SUBTYPE_QOS)) {
		*(__le16 *)&wh->i_seq[0] = htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[0]++;
	}
#undef WH4
}

/*
 * Send an EAPOL frame to the specified node.
 * Use the MGMT frame path to ensure that EAPOL frames are high priority.
 */
void
ieee80211_eap_output(struct net_device *dev, const void *const eap_msg, const int eap_msg_len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct ieee80211_qosframe *qwh;
	const struct ether_header *const eh = eap_msg;
	uint8_t *frm;
	struct sk_buff *skb;
	struct llc *llc;
	int headerlen;
	uint8_t subtype;

	if (eap_msg_len <= sizeof(*eh)) {
		return;
	}

	if (vap->iv_opmode == IEEE80211_M_WDS) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DOT1X,
			"[%pM] eap send failed - WDS not supported\n",
			eh->ether_dhost);
		return;
	}

	ni = ieee80211_find_node(&ic->ic_sta, eh->ether_dhost);
	if (!ni) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DOT1X,
			"[%pM] eap send failed - node %s not found\n",
			eh->ether_dhost);
		return;
	}

	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		headerlen = sizeof(*qwh);
		subtype = IEEE80211_FC0_SUBTYPE_QOS;
	} else {
		headerlen = sizeof(*wh);
		subtype = IEEE80211_FC0_SUBTYPE_DATA;
	}

	skb = ieee80211_getdataframe(vap, &frm, 1, eap_msg_len + LLC_SNAPFRAMELEN);
	if (!skb) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DOT1X,
			"[%s] eap send failed - sbk alloc\n",
			ni->ni_macaddr);
		ieee80211_free_node(ni);
		return;
	}

	memcpy(frm, eap_msg, eap_msg_len);

	/* Replace the ethernet header with SNAP and 802.11 headers */
	skb_pull(skb, sizeof(*eh));

	llc = (struct llc *) skb_push(skb, LLC_SNAPFRAMELEN);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = htons(ETH_P_PAE);

	wh = (struct ieee80211_frame *) skb_push(skb, headerlen);

	ieee80211_send_setup(vap, ni, wh,
		IEEE80211_FC0_TYPE_DATA, subtype,
		vap->iv_myaddr, ni->ni_macaddr, ni->ni_bssid);

	skb_trim(skb, eap_msg_len - sizeof(*eh) + headerlen + LLC_SNAPFRAMELEN);

	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		qwh = (struct ieee80211_qosframe *) wh;
		qwh->i_qos[0] = QTN_TID_WLAN;
		qwh->i_qos[1] = 0;
	}

	if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap))
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DOT1X,
		"[%pM] send eapol frame on channel %u\n",
		ni->ni_macaddr, ieee80211_chan2ieee(ic, ic->ic_curchan));

	IEEE80211_NODE_STAT(ni, tx_data);

	ieee80211_off_channel_suspend(vap, IEEE80211_OFFCHAN_TIMEOUT_EAPOL);

	ic->ic_send_80211(ic, ni, skb, WME_AC_VO, 0);
}
EXPORT_SYMBOL(ieee80211_eap_output);

/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially freeing up any associated storage).
 */
void
ieee80211_mgmt_output(struct ieee80211_node *ni, struct sk_buff *skb, int subtype,
			const u_int8_t da[IEEE80211_ADDR_LEN])
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame *wh;

	KASSERT(ni != NULL, ("null node"));

	wh = (struct ieee80211_frame *) skb_push(skb, sizeof(struct ieee80211_frame));
	ieee80211_send_setup(vap, ni, wh, IEEE80211_FC0_TYPE_MGT, subtype,
		vap->iv_myaddr, da, ni->ni_bssid);

	/* FIXME power management */

	if (M_FLAG_ISSET(skb, M_LINK0) && ni->ni_challenge != NULL) {
		M_FLAG_CLR(skb, M_LINK0);
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_AUTH, wh->i_addr1,
			"encrypting frame (%s)", __func__);
		wh->i_fc[1] |= IEEE80211_FC1_PROT;
	}

	if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap))
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

#ifdef IEEE80211_DEBUG
	if ((ieee80211_msg_debug(vap) && doprint(vap, subtype)) ||
	    ieee80211_msg_dumppkts(vap)) {
		printf("[%pM] send %s on channel %u\n",
			wh->i_addr1,
			ieee80211_mgt_subtype_name[
				(subtype & IEEE80211_FC0_SUBTYPE_MASK) >>
					IEEE80211_FC0_SUBTYPE_SHIFT],
			ieee80211_chan2ieee(ic, ic->ic_curchan));
	}
#endif
	IEEE80211_NODE_STAT(ni, tx_mgmt);

	ic->ic_send_80211(ic, ni, skb, WME_AC_VO, 1);
}
EXPORT_SYMBOL(ieee80211_mgmt_output);

void
ieee80211_tdls_mgmt_output(struct ieee80211_node *ni,
	struct sk_buff *skb, const uint8_t type,
	const uint8_t subtype, const uint8_t *da,
	const uint8_t *bssid)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame *wh;

	KASSERT(ni != NULL, ("null node"));

	wh = (struct ieee80211_frame *)
		skb_push(skb, sizeof(struct ieee80211_frame));
	ieee80211_send_setup(vap, ni, wh,
		type, subtype, vap->iv_myaddr, da, bssid);

	if (M_FLAG_ISSET(skb, M_LINK0) && ni->ni_challenge != NULL) {
		M_FLAG_CLR(skb, M_LINK0);
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_AUTH, wh->i_addr1,
			"encrypting frame (%s)", __func__);
		wh->i_fc[1] |= IEEE80211_FC1_PROT;
	}

	/* XXX power management */
	if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap))
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

#ifdef IEEE80211_DEBUG
	if ((ieee80211_msg_debug(vap) && doprint(vap, subtype)) ||
	    ieee80211_msg_dumppkts(vap)) {
		printf("[%s] send %s on channel %u\n",
			ether_sprintf(wh->i_addr1),
			ieee80211_mgt_subtype_name[
				(subtype & IEEE80211_FC0_SUBTYPE_MASK) >>
					IEEE80211_FC0_SUBTYPE_SHIFT],
			ieee80211_chan2ieee(ic, ic->ic_curchan));
	}
#endif
	IEEE80211_NODE_STAT(ni, tx_mgmt);

	ic->ic_send_80211(ic, ni, skb, WME_AC_VO, 1);
}
EXPORT_SYMBOL(ieee80211_tdls_mgmt_output);

/*
 * Send a null data frame to the specified node.
 *
 * NB: the caller is assumed to have setup a node reference
 *     for use; this is necessary to deal with a race condition
 *     when probing for inactive stations.
 */
int
ieee80211_send_nulldata(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb;
	struct ieee80211_frame *wh;
	uint8_t *frm;

	skb = ieee80211_getdataframe(vap, &frm, 0, 0);
	if (skb == NULL) {
		ieee80211_free_node(ni);
		return -ENOMEM;
	}

	wh = (struct ieee80211_frame *) skb_push(skb, sizeof(struct ieee80211_frame));
	ieee80211_send_setup(vap, ni, wh,
		IEEE80211_FC0_TYPE_DATA,
		IEEE80211_FC0_SUBTYPE_NODATA,
		vap->iv_myaddr, ni->ni_macaddr, ni->ni_bssid);

	/* NB: power management bit is never sent by an AP */
	if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
			vap->iv_opmode != IEEE80211_M_HOSTAP &&
			vap->iv_opmode != IEEE80211_M_WDS)
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

	IEEE80211_NODE_STAT(ni, tx_data);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
		"[%s] send null data frame on channel %u, pwr mgt %s\n",
		ether_sprintf(ni->ni_macaddr),
		ieee80211_chan2ieee(ic, ic->ic_curchan),
		wh->i_fc[1] & IEEE80211_FC1_PWR_MGT ? "ena" : "dis");

	ic->ic_send_80211(ic, ni, skb, WME_AC_VO, 0);

	return 0;
}

/*
 * Send some tuning data packets for low level to do
 * power adjustment.
 */
int
ieee80211_send_tuning_data(struct ieee80211_node *ni)
{
	struct ieee80211com	*ic = ni->ni_ic;
	struct ieee80211vap	*vap = ni->ni_vap;
	struct sk_buff		*skb;
	struct ieee80211_frame	*wh;
	uint8_t			*frm;
	const uint8_t		da[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	skb = ieee80211_getdataframe(vap, &frm, 0, 0);
	if (skb == NULL)
		return -ENOMEM;

	/* Fill up the frame header */
	wh = (struct ieee80211_frame *)skb_push(skb, sizeof(struct ieee80211_frame));

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_NODATA;
	wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;

	IEEE80211_ADDR_COPY(wh->i_addr1, da);
	IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_macaddr);

	wh->i_dur[0] = 0;
	wh->i_dur[1] = 0;

	ieee80211_ref_node(ni);

	/* No need to send these at high priority */
	(void)ic->ic_send_80211(ic, ni, skb, WME_AC_BE, 0);

	return 0;
}
EXPORT_SYMBOL(ieee80211_send_nulldata);

/*
 * Get null data on a particular AC to a node.
 *
 * The caller is assumed to have taken a node reference.
 */
struct sk_buff *
ieee80211_get_nulldata(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct sk_buff *skb;
	struct ieee80211_frame *wh;
	uint8_t *frm;

	skb = ieee80211_getdataframe(vap, &frm, 0, 0);
	if (skb == NULL)
		return NULL;

	wh = (struct ieee80211_frame *)skb_push(skb, sizeof(struct ieee80211_frame));

	ieee80211_send_setup(vap, ni, (struct ieee80211_frame *)wh,
		IEEE80211_FC0_TYPE_DATA,
		IEEE80211_FC0_SUBTYPE_NODATA,
		vap->iv_myaddr,
		ni->ni_macaddr,
		ni->ni_bssid);

	if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap))
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

	IEEE80211_NODE_STAT(ni, tx_data);

	return skb;
}
EXPORT_SYMBOL(ieee80211_get_nulldata);

/*
 * Get QoS null data on a particular AC to a node.
 *
 * The caller is assumed to have taken a node reference.
 */
struct sk_buff *
ieee80211_get_qosnulldata(struct ieee80211_node *ni, int ac)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb;
	struct ieee80211_qosframe *qwh;
	uint8_t *frm;
	int tid;

	skb = ieee80211_getdataframe(vap, &frm, 1, 0);
	if (skb == NULL)
		return NULL;

	skb->priority = ac;
	qwh = (struct ieee80211_qosframe *)skb_push(skb, sizeof(struct ieee80211_qosframe));

	ieee80211_send_setup(vap, ni, (struct ieee80211_frame *)qwh,
		IEEE80211_FC0_TYPE_DATA,
		IEEE80211_FC0_SUBTYPE_QOS_NULL,
		vap->iv_myaddr,
		ni->ni_macaddr,
		ni->ni_bssid);

	if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap))
		qwh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

	tid = QTN_TID_WLAN;
	qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
	if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmm_noackPolicy)
		qwh->i_qos[0] |= (1 << IEEE80211_QOS_ACKPOLICY_S) & IEEE80211_QOS_ACKPOLICY;
	qwh->i_qos[1] = 0;

	IEEE80211_NODE_STAT(ni, tx_data);

	if (WME_UAPSD_AC_CAN_TRIGGER(skb->priority, ni)) {
		/* U-APSD power save queue */
		/* XXXAPSD: assuming triggerable means deliverable */
		M_FLAG_SET(skb, M_UAPSD);
	}

	return skb;
}
EXPORT_SYMBOL(ieee80211_get_qosnulldata);

/*
 * Send QoS null data on a particular AC to a node.
 *
 * The caller is assumed to have taken a node reference.
 */
int
ieee80211_send_qosnulldata(struct ieee80211_node *ni, int ac)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb;

	skb = ieee80211_get_qosnulldata(ni, ac);
	if (skb == NULL) {
		ieee80211_free_node(ni);
		return -ENOMEM;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
			"[%s] send qos null data frame on channel %u\n",
			ether_sprintf(ni->ni_macaddr),
			ieee80211_chan2ieee(ic, ic->ic_curchan));
	ic->ic_send_80211(ic, ni, skb, ac, 0);

	return 0;
}
EXPORT_SYMBOL(ieee80211_send_qosnulldata);

int
ieee80211_send_qosnulldata_ext(struct ieee80211com *ic, uint8_t *mac_addr, int pwr_mgt)
{
#define	WH4(wh)	((struct ieee80211_frame_addr4 *)wh)
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;
	struct sk_buff *skb;
	uint8_t *frm;
	struct ieee80211_qosframe *qwh;
	int ac;
	int tid;

	ni = ieee80211_find_node(&ic->ic_sta, mac_addr);
	if (!ni)
	      return -EINVAL;

	ac = WMM_AC_BK;
	vap = ni->ni_vap;
	skb = ieee80211_getdataframe(vap, &frm, 1, 0);
	if (skb == NULL) {
		ieee80211_free_node(ni);
		return -ENOMEM;
	}

	skb->priority = ac;
	qwh = (struct ieee80211_qosframe *)skb_push(skb, sizeof(struct ieee80211_qosframe_addr4));

	ieee80211_send_setup(vap, ni, (struct ieee80211_frame *)qwh,
		IEEE80211_FC0_TYPE_MGT,
		IEEE80211_FC0_SUBTYPE_QOS_NULL,
		vap->iv_myaddr,
		ni->ni_macaddr,
		ni->ni_bssid);
	qwh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS_NULL;

	if (pwr_mgt)
		qwh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

	tid = QTN_TID_WLAN;
	qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
	if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmm_noackPolicy)
		qwh->i_qos[0] |= (1 << IEEE80211_QOS_ACKPOLICY_S) & IEEE80211_QOS_ACKPOLICY;
	qwh->i_qos[1] = 0;

	IEEE80211_NODE_STAT(ni, tx_data);

	if (WME_UAPSD_AC_CAN_TRIGGER(skb->priority, ni)) {
		/* U-APSD power save queue */
		/* XXXAPSD: assuming triggerable means deliverable */
		M_FLAG_SET(skb, M_UAPSD);
	}
	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
			"[%s] send pwr_mgt(%d) data frame on channel %u\n",
			ether_sprintf(ni->ni_macaddr), pwr_mgt,
			ieee80211_chan2ieee(ic, ic->ic_curchan));

	ic->ic_send_80211(ic, ni, skb, ac, 0);

	return 0;
}
EXPORT_SYMBOL(ieee80211_send_qosnulldata_ext);

/*
 * Add transmit power envelope information element
 */
u_int8_t *
ieee80211_add_vhttxpwr_envelope(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int32_t bw = ieee80211_get_bw(ic);
	struct ieee80211_ie_vtxpwren *ie = (struct ieee80211_ie_vtxpwren *)frm;
	u_int8_t local_max_tx_pwrcnt = 0;
	struct ieee80211_channel *des_chan = ic->ic_des_chan;

	if (des_chan == IEEE80211_CHAN_ANYC)
		return frm;

	switch (bw) {
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

	frm += sizeof(struct ieee80211_ie_vtxpwren);
	return frm;
}

/*
 * Add wide-bandwidth Channel switch wrapper information element
 */
u_int8_t *
ieee80211_add_wband_chanswitch(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int32_t bw = ieee80211_get_bw(ic);
	struct ieee80211_ie_wbchansw *ie = (struct ieee80211_ie_wbchansw *)frm;
	struct ieee80211_channel *des_chan = ic->ic_des_chan;
	u_int32_t chwidth = 0;

	if (!des_chan || (des_chan == IEEE80211_CHAN_ANYC))
		return frm;

	ie->wbcs_id = IEEE80211_ELEMID_WBWCHANSWITCH;
	ie->wbcs_len = sizeof(struct ieee80211_ie_wbchansw) - 2;
	switch (bw) {
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
	if (bw == BW_HT40) {
		ie->wbcs_newchancf0 = des_chan->ic_center_f_40MHz;
		ie->wbcs_newchancf1 = 0;
	} else if (bw == BW_HT80) {
		ie->wbcs_newchancf0 = des_chan->ic_center_f_80MHz;
		ie->wbcs_newchancf1 = 0;
	} else {
		ie->wbcs_newchancf0 = 0;
		ie->wbcs_newchancf1 = 0;
	}

	frm += sizeof(struct ieee80211_ie_wbchansw);
	return frm;
}

/*
 * Add Channel switch wrapper information element
 */
u_int8_t *
ieee80211_add_chansw_wrap(u_int8_t *frm, struct ieee80211com *ic)
{
	struct ieee80211_ie_chsw_wrapper *ie = (struct ieee80211_ie_chsw_wrapper *) frm;
	u_int32_t bw = ieee80211_get_bw(ic);
	ie->chsw_id = IEEE80211_ELEMID_CHANSWITCHWRP;
	ie->chsw_len = 0;
	frm += sizeof(struct ieee80211_ie_chsw_wrapper);

	/* Wide bandwidth channel switch element */
	if (bw > BW_HT20) {
		ie->chsw_len += sizeof(struct ieee80211_ie_wbchansw);
		frm = ieee80211_add_wband_chanswitch(frm, ic);
	}
	/* VHT transmit power envelope */
	if ((ic->ic_flags & IEEE80211_F_DOTH) &&
	    (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
		ie->chsw_len += sizeof(struct ieee80211_ie_vtxpwren);
		frm = ieee80211_add_vhttxpwr_envelope(frm, ic);
	}
	return frm;
}

/*
 * Add a supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add a Supported Channels element id to a frame.
 */
uint8_t*
ieee80211_add_supported_chans(uint8_t *frm, struct ieee80211com *ic)
{
	int band_idx;
	int first_chan;
	int temp_chan;
	int chan_cnt;
	int active_chan_cnt;
	uint8_t *ie_len;
	int cur_bw;
	uint8_t *chan_active;
	struct ieee80211_band_info *band;


	*frm++ = IEEE80211_ELEMID_SUPPCHAN;
	ie_len = frm++;
	*ie_len = 0;

	cur_bw = ieee80211_get_bw(ic);
	if (cur_bw == BW_HT20)
		chan_active = &ic->ic_chan_active_20[0];
	else if (cur_bw == BW_HT40)
		chan_active = &ic->ic_chan_active_40[0];
	else if (cur_bw == BW_HT80)
		chan_active = &ic->ic_chan_active_80[0];
	else
		chan_active = &ic->ic_chan_active[0];

	for (band_idx = 0; band_idx < IEEE80211_BAND_IDX_MAX; band_idx++) {
		band = ieee80211_get_band_info(band_idx);
		if (band == NULL)
			continue;

		first_chan = band->band_first_chan;
		chan_cnt = band->band_chan_cnt;
		active_chan_cnt = 0;
		for (temp_chan = first_chan; chan_cnt >= 0; chan_cnt--){
			if (isset(chan_active, temp_chan) && chan_cnt > 0) {
				active_chan_cnt++;
			} else if (active_chan_cnt) {
				*frm++ = first_chan;
				*frm++ = active_chan_cnt;
				*ie_len += 2;
				active_chan_cnt = 0;
			}

			if (active_chan_cnt == 1)
				first_chan = temp_chan;

			temp_chan += band->band_chan_step;
		}
	}

	return frm;
}


/*
 * Add an extended capabilities element id to a frame
 */
u_int8_t *
ieee80211_add_extcap(u_int8_t *frm)
{
	struct ieee80211_extcap_param *ie  = (struct ieee80211_extcap_param *)frm;

	memset(ie, 0, sizeof(struct ieee80211_extcap_param));

	ie->param_id = IEEE80211_ELEMID_EXTCAP;
	ie->param_len =	sizeof(struct ieee80211_extcap_param) - 2;
	/* max msdu in amsdu 0 = unlimited */
	ie->ext_cap[7] = IEEE80211_EXTCAP_OPMODE_NOTIFICATION;

	return frm + sizeof(struct ieee80211_extcap_param);
}

/*
 * Add an extended supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
//FIXME
#if 1
	/*
	 * Add an extended supported rates element if operating in 11g/n mode.
	 * Only 11g rates are added. 11n Rates are published via ht cap */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		int nrates = rs->rs_legacy_nrates - IEEE80211_RATE_SIZE;
		if(nrates)
		{
			*frm++ = IEEE80211_ELEMID_XRATES;
			*frm++ = nrates;
			memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
			frm += nrates;
		}
	}
#else
	/* Add BSS membership selector (HT == 0x7F)*/
	*frm++ = IEEE80211_ELEMID_XRATES;
	*frm++ = 1;
	*frm++ = 0x7F;
#endif
	return frm;
}

/*
 * Add an ssid elemet to a frame.
 */
static u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add an csa element to a frame.
 */
u_int8_t *
ieee80211_add_csa(u_int8_t *frm,
		u_int8_t csa_mode,
		u_int8_t csa_chan,
		u_int8_t csa_count)
{
	*frm++ = IEEE80211_ELEMID_CHANSWITCHANN;
	*frm++ = 3;
	*frm++ = csa_mode;
	*frm++ = csa_chan;
	*frm++ = csa_count;

	return frm;
}

/*
 * Add secondary channel offset element to a frame.
 */
void ieee80211_add_sec_chan_off(u_int8_t **frm,
		struct ieee80211com *ic,
		uint8_t csa_chan)
{
	struct ieee80211_channel *chan = NULL;
	uint8_t sec_position = IEEE80211_HTINFO_EXTOFFSET_NA;
        struct ieee80211_ie_sec_chan_off *sco = (struct ieee80211_ie_sec_chan_off *)(*frm);
	uint32_t curr_bw = ieee80211_get_bw(ic);

	chan = ieee80211_find_channel_by_ieee(ic, csa_chan);

	if (chan && (curr_bw >= BW_HT40)) {
		if (chan->ic_flags & IEEE80211_CHAN_HT40D) {
			sec_position = IEEE80211_HTINFO_EXTOFFSET_BELOW;
		} else if (chan->ic_flags & IEEE80211_CHAN_HT40U) {
			sec_position = IEEE80211_HTINFO_EXTOFFSET_ABOVE;
		}
	}

        sco->sco_id = IEEE80211_ELEMID_SEC_CHAN_OFF;
        sco->sco_len = 1;
        sco->sco_off = sec_position;

        *frm += sizeof(struct ieee80211_ie_sec_chan_off);

	return;
}

/*
 * Add an erp element to a frame.
 */
u_int8_t *
ieee80211_add_erp(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int8_t erp;

	*frm++ = IEEE80211_ELEMID_ERP;
	*frm++ = 1;
	erp = 0;
	if (ic->ic_nonerpsta != 0)
		erp |= IEEE80211_ERP_NON_ERP_PRESENT;
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		erp |= IEEE80211_ERP_USE_PROTECTION;
	if (ic->ic_flags & IEEE80211_F_USEBARKER)
		erp |= IEEE80211_ERP_LONG_PREAMBLE;
	*frm++ = erp;
	return frm;
}

/*
 * Add a country information element to a frame.
 */
u_int8_t *
ieee80211_add_country(u_int8_t *frm, struct ieee80211com *ic)
{
	/* add country code */
	memcpy(frm, (u_int8_t *)&ic->ic_country_ie,
		ic->ic_country_ie.country_len + 2);
	frm +=  ic->ic_country_ie.country_len + 2;
	return frm;
}

/*
 * Add BSS load element to frame
 */
u_int8_t *
ieee80211_add_bss_load(u_int8_t *frm, struct ieee80211vap *vap)
{
	*frm++ = IEEE80211_ELEMID_BSS_LOAD;
	*frm++ = 5;
	*(__le16 *)frm = htole16(vap->iv_sta_assoc);
	frm += 2;
	/* TODO Channel Utilization and Available Admission Capacity
	 * parameters need to be updated with correct values */
	*frm++ = 0;
	*(__le16 *)frm = 0;
	frm += 2;

	return frm;
}

static u_int8_t *
ieee80211_setup_wpa_ie(struct ieee80211vap *vap, u_int8_t *ie)
{
#define	WPA_OUI_BYTES		0x00, 0x50, 0xf2
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)
	static const u_int8_t oui[4] = { WPA_OUI_BYTES, WPA_OUI_TYPE };
	static const u_int8_t cipher_suite[][4] = {
		{ WPA_OUI_BYTES, WPA_CSE_WEP40 },	/* NB: 40-bit */
		{ WPA_OUI_BYTES, WPA_CSE_TKIP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX WRAP */
		{ WPA_OUI_BYTES, WPA_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ WPA_OUI_BYTES, WPA_CSE_NULL },
	};
	static const u_int8_t wep104_suite[4] =
		{ WPA_OUI_BYTES, WPA_CSE_WEP104 };
	static const u_int8_t key_mgt_unspec[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* WPA OUI */
	frm += sizeof(oui);
	ADDSHORT(frm, WPA_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else if ((!WPA_TKIP_SUPPORT) && (rsn->rsn_mcastcipher == IEEE80211_CIPHER_TKIP))	/* remove TKIP functionality */
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1 << IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (WPA_TKIP_SUPPORT) {
		if (rsn->rsn_ucastcipherset & (1 << IEEE80211_CIPHER_TKIP)) {
			selcnt[0]++;
			ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
		}
	}
	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	if ((rsn->rsn_caps != 0) && (rsn->rsn_caps != RSN_CAP_PREAUTH))
		ADDSHORT(frm, rsn->rsn_caps);

	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1] + 2 <= sizeof(struct ieee80211_ie_wpa),
		("WPA IE too big, %u > %u",
		ie[1] + 2, (int)sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSHORT
#undef ADDSELECTOR
#undef WPA_OUI_BYTES
}

static u_int8_t *
ieee80211_setup_rsn_ie(struct ieee80211vap *vap, u_int8_t *ie)
{
#define	RSN_OUI_BYTES		0x00, 0x0f, 0xac
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)
	static const u_int8_t cipher_suite[][4] = {
		{ RSN_OUI_BYTES, RSN_CSE_WEP40 },	/* NB: 40-bit */
		{ RSN_OUI_BYTES, RSN_CSE_TKIP },
		{ RSN_OUI_BYTES, RSN_CSE_WRAP },
		{ RSN_OUI_BYTES, RSN_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ RSN_OUI_BYTES, RSN_CSE_NULL },
	};
	static const u_int8_t wep104_suite[4] =
		{ RSN_OUI_BYTES, RSN_CSE_WEP104 };
	static const u_int8_t key_mgt_unspec[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_PSK };
	static const u_int8_t key_mgt_dot1x_sha256[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_SHA256 };
	static const u_int8_t key_mgt_psk_sha256[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_PSK_SHA256 };
	static const u_int8_t key_mgt_bip[4] =
		{ RSN_OUI_BYTES, RSN_CSE_BIP };
	const struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_RSN;
	*frm++ = 0;				/* length filled in below */
	ADDSHORT(frm, RSN_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else if ((!WPA_TKIP_SUPPORT) && (rsn->rsn_mcastcipher == IEEE80211_CIPHER_TKIP))	/* remove TKIP functionality */
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1 << IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (WPA_TKIP_SUPPORT) {
		if (rsn->rsn_ucastcipherset & (1 << IEEE80211_CIPHER_TKIP)) {
			selcnt[0]++;
			ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
		}
	}
	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset == WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	if (vap->iv_pmf) {
		if ((rsn->rsn_keymgmtset == RSN_ASE_8021X_PSK_SHA256)) {
			selcnt[0]++;
			ADDSELECTOR(frm, key_mgt_psk_sha256);
		} else if ((rsn->rsn_keymgmtset == RSN_ASE_8021X_SHA256)) {
			selcnt[0]++;
			ADDSELECTOR(frm, key_mgt_dot1x_sha256);
		}
	}

	/* capabilities */
	ADDSHORT(frm, (rsn->rsn_caps | (vap->iv_pmf << 6)));
	/* XXX PMKID */
	if (vap->iv_pmf) {
		/* PMKID here: We dont support PMKID list  */
		ADDSHORT(frm, 0);

		/* 802.11w Group Management Cipher suite */
		selcnt = frm;
		if (rsn->rsn_ucastcipherset & (1 << IEEE80211_CIPHER_AES_CCM)) {
			selcnt[0]++;
			ADDSELECTOR(frm, key_mgt_bip);
		}
	}
	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1] + 2 <= sizeof(struct ieee80211_ie_wpa),
		("RSN IE too big, %u > %u",
		ie[1] + 2, (int)sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSELECTOR
#undef ADDSHORT
#undef RSN_OUI_BYTES
}

/*
 * Add a WPA/RSN element to a frame.
 */
u_int8_t *
ieee80211_add_wpa(u_int8_t *frm, struct ieee80211vap *vap)
{

	KASSERT(vap->iv_flags & IEEE80211_F_WPA, ("no WPA/RSN!"));
	if (vap->iv_flags & IEEE80211_F_WPA2)
		frm = ieee80211_setup_rsn_ie(vap, frm);
	if (vap->iv_flags & IEEE80211_F_WPA1)
		frm = ieee80211_setup_wpa_ie(vap, frm);
	return frm;
}

#define	WME_OUI_BYTES		0x00, 0x50, 0xf2
/*
 * Add a WME Info element to a frame.
 */
static u_int8_t *
ieee80211_add_wme(u_int8_t *frm, struct ieee80211_node *ni)
{
	static const u_int8_t oui[4] = { WME_OUI_BYTES, WME_OUI_TYPE };
	struct ieee80211_ie_wme *ie = (struct ieee80211_ie_wme *) frm;
	struct ieee80211_wme_state *wme = &ni->ni_ic->ic_wme;
	struct ieee80211vap *vap = ni->ni_vap;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* WME OUI */
	frm += sizeof(oui);
	*frm++ = WME_INFO_OUI_SUBTYPE;		/* OUI subtype */
	*frm++ = WME_VERSION;			/* protocol version */
	/* QoS Info field depends on operating mode */
	switch (vap->iv_opmode) {
	case IEEE80211_M_HOSTAP:
		*frm = wme->wme_bssChanParams.cap_info_count;
		if (IEEE80211_VAP_UAPSD_ENABLED(vap))
			*frm |= WME_CAPINFO_UAPSD_EN;
		frm++;
		break;
	case IEEE80211_M_STA:
		*frm++ = vap->iv_uapsdinfo;
		break;
	default:
		*frm++ = 0;
	}

	ie->wme_len = frm - &ie->wme_oui[0];

	return frm;
}

/*
 * Add a WME Parameter element to a frame.
 */
u_int8_t *
ieee80211_add_wme_param(u_int8_t *frm, struct ieee80211_wme_state *wme,
	int uapsd_enable, int is_qtn_wme)
{
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
	static const u_int8_t oui[4] = { WME_OUI_BYTES, WME_OUI_TYPE };
	struct ieee80211_wme_param *ie = (struct ieee80211_wme_param *) frm;
	int i;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* WME OUI */
	frm += sizeof(oui);
	*frm++ = WME_PARAM_OUI_SUBTYPE;		/* OUI subtype */
	*frm++ = WME_VERSION;			/* protocol version */
	*frm = wme->wme_bssChanParams.cap_info_count;
	if (uapsd_enable)
		*frm |= WME_CAPINFO_UAPSD_EN;
	frm++;
	*frm++ = 0;                             /* reserved field */
	for (i = 0; i < WME_NUM_AC; i++) {
		const struct wmm_params *ac;
#ifdef CONFIG_QVSP
		if (!is_qtn_wme && (wme->wme_throt_bm & BIT(i))) {
			ac = &wme->wme_throt_bssChanParams.cap_wmeParams[i];
		} else
#endif
		{
			ac = &wme->wme_bssChanParams.cap_wmeParams[i];
		}
		*frm++ = SM(i, WME_PARAM_ACI) |
			SM(ac->wmm_acm, WME_PARAM_ACM) |
			SM(ac->wmm_aifsn, WME_PARAM_AIFSN);
		*frm++ = SM(ac->wmm_logcwmax, WME_PARAM_LOGCWMAX) |
			SM(ac->wmm_logcwmin, WME_PARAM_LOGCWMIN);
		ADDSHORT(frm, ac->wmm_txopLimit);
	}

	ie->param_len = frm - &ie->param_oui[0];

	return frm;
#undef ADDSHORT
}
#undef WME_OUI_BYTES

/*
 * Add an Atheros Advanaced Capability element to a frame
 */
u_int8_t *
ieee80211_add_athAdvCap(u_int8_t *frm, u_int8_t capability, u_int16_t defaultKey)
{
	static const u_int8_t oui[6] = {(ATH_OUI & 0xff), ((ATH_OUI >>8) & 0xff),
		((ATH_OUI >> 16) & 0xff), ATH_OUI_TYPE,
		ATH_OUI_SUBTYPE, ATH_OUI_VERSION};
	struct ieee80211_ie_athAdvCap *ie = (struct ieee80211_ie_athAdvCap *) frm;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* Length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* Atheros OUI, type, subtype, and version for adv capabilities */
	frm += sizeof(oui);
	*frm++ = capability;

	/* Setup default key index in little endian byte order */
	*frm++ = (defaultKey & 0xff);
	*frm++ = ((defaultKey >> 8) & 0xff);
	ie->athAdvCap_len = frm - &ie->athAdvCap_oui[0];

	return frm;
}

/*
 * Add the Quantenna IE to a frame
 * - all existing fields must be backwards compatible with previous verions.
 */
uint8_t *
ieee80211_add_qtn_ie(uint8_t *frm, struct ieee80211com *ic, uint8_t flags, uint8_t my_flags,
			uint8_t implicit_ba, uint16_t implicit_ba_size, uint32_t rate_train)
{
	struct ieee80211_ie_qtn *ie = (struct ieee80211_ie_qtn *)frm;

	ie->qtn_ie_id = IEEE80211_ELEMID_VENDOR;
	ie->qtn_ie_len = (uint8_t)sizeof(*ie) - IEEE80211_IE_ID_LEN_SIZE;
	ieee80211_oui_add_qtn(ie->qtn_ie_oui);
	ie->qtn_ie_type = QTN_OUI_CFG;
	ie->qtn_ie_flags = (flags | IEEE80211_QTN_FLAGS_ENVY_DFLT) & IEEE80211_QTN_FLAGS_ENVY;
	ie->qtn_ie_implicit_ba_tid = implicit_ba;
	ie->qtn_ie_my_flags = (my_flags | IEEE80211_QTN_CAPS_DFLT) & ~IEEE80211_QTN_BF_VER1;
	ie->qtn_ie_implicit_ba_tid_h = implicit_ba;
	ie->qtn_ie_implicit_ba_size = (implicit_ba_size >> IEEE80211_QTN_IE_BA_SIZE_SH);
	ie->qtn_ie_vsp_version = IEEE80211_QTN_VSP_VERSION;

	put_unaligned(htonl(ic->ic_ver_sw), &ie->qtn_ie_ver_sw);
	put_unaligned(htons(ic->ic_ver_hw), &ie->qtn_ie_ver_hw);
	put_unaligned(htons(ic->ic_ver_platform_id), &ie->qtn_ie_ver_platform_id);
	put_unaligned(htonl(ic->ic_ver_timestamp), &ie->qtn_ie_ver_timestamp);
	put_unaligned(htonl(rate_train), &ie->qtn_ie_rate_train);
	put_unaligned(htonl(ic->ic_ver_flags), &ie->qtn_ie_ver_flags);

	return frm + sizeof(*ie);
}

#ifdef CONFIG_QVSP
static __inline int
ieee80211_vsp_ie_max_len(struct ieee80211com *ic)
{
	return sizeof(struct ieee80211_ie_vsp) +
		(sizeof(struct ieee80211_ie_vsp_item) * ARRAY_SIZE(ic->vsp_cfg));
}

/*
 * Add the Quantenna VSP configuration IE to a frame
 */
static uint8_t *
ieee80211_add_vsp_ie(struct ieee80211vap *vap, void *start, void *end)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_ie_vsp *vsp_ie = start;
	struct ieee80211_ie_vsp_item *item_p = &vsp_ie->item[0];
	int i;

	vsp_ie->id = IEEE80211_ELEMID_VENDOR;
	ieee80211_oui_add_qtn(vsp_ie->oui);
	vsp_ie->type = QTN_OUI_VSP_CTRL;
	vsp_ie->item_cnt = 0;

	for (i = 0; i < ARRAY_SIZE(ic->vsp_cfg); i++) {
		if (ic->vsp_cfg[i].set != 0) {
			item_p->index = i;
			put_unaligned(htonl(ic->vsp_cfg[i].value), &item_p->value);
			item_p++;
			vsp_ie->item_cnt++;
			if ((void *)item_p > end) {
				printk(KERN_INFO "VSP: not adding IE to assoc resp - too long\n");
				return start;
			}
		}
	}
	vsp_ie->len = (uint8_t *)item_p - &vsp_ie->oui[0];

	return (uint8_t *)item_p;
}

static uint8_t *
ieee80211_add_timeout_ie(u_int8_t *frm)
{
		struct ieee80211_timout_int_ie *tie = (struct ieee80211_timout_int_ie *) frm;

		tie->timout_int_ie = IEEE80211_ELEMID_TIMEOUT_INT;
		tie->timout_int_len = 5;
		tie->timout_int_type = IEEE80211_TIMEOUT_ASSOC_COMEBACK;	/* timeout value type */
		tie->timout_int_value = htole32(IEEE80211_W_ASSOC_COMEBACK_TO);	/* default value is 1000tus */
		frm += sizeof(struct ieee80211_timout_int_ie);
		return frm;
}

uint8_t *
ieee80211_add_qtn_wme_param(struct ieee80211vap *vap, u_int8_t *frm)
{
	struct ieee80211_ie_qtn_wme *qwme_ie = (struct ieee80211_ie_qtn_wme *)frm;
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);

	qwme_ie->qtn_ie_id = IEEE80211_ELEMID_VENDOR;
	qwme_ie->qtn_ie_len = sizeof(struct ieee80211_ie_qtn_wme) - 2;
	ieee80211_oui_add_qtn(qwme_ie->qtn_ie_oui);
	qwme_ie->qtn_ie_type = QTN_OUI_QWME;
	qwme_ie->qtn_wme_ie_version = QTN_QWME_IE_VERSION;

	return ieee80211_add_wme_param((uint8_t*)&qwme_ie->qtn_wme_ie, wme, IEEE80211_VAP_UAPSD_ENABLED(vap), 1);
}
#endif

/*
 * Add Quantenna pairing hash to a frame
 */
u_int8_t *
ieee80211_add_qtn_pairing_ie(u_int8_t *frm, struct ieee80211_app_ie_t *pairing_ie)
{
	struct ieee80211_ie_qtn_pairing_tlv tlv_ie;
	struct ieee80211_ie_qtn_pairing *ie = (struct ieee80211_ie_qtn_pairing *) frm;

	tlv_ie.qtn_pairing_tlv_type = QTN_OUI_PAIRING;
	memcpy(tlv_ie.qtn_pairing_tlv_hash, pairing_ie->ie, QTN_PAIRING_TLV_HASH_LEN);
	tlv_ie.qtn_pairing_tlv_len = htole16(sizeof(struct ieee80211_ie_qtn_pairing_tlv));

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;
	frm += ieee80211_oui_add_qtn(frm);
	memcpy(frm, &tlv_ie, sizeof(struct ieee80211_ie_qtn_pairing_tlv));
	frm += sizeof(struct ieee80211_ie_qtn_pairing_tlv);
	ie->qtn_pairing_ie_len = frm - &ie->qtn_pairing_ie_oui[0];

	return frm;
}
/*
 * Add Quantenna specific 802.11h information elements to a frame.
 */
u_int8_t *
ieee80211_add_qtn_csatsf_ie(u_int8_t *frm, u_int64_t tsf)
{
	struct ieee80211_ie_qtn_csa_tsf *ie = (struct ieee80211_ie_qtn_csa_tsf *)frm;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;		/* Length is filled in below */
	frm += ieee80211_oui_add_qtn(frm);
	*frm++ = QTN_OUI_CFG;
	ie->tsf = htonll(tsf);
	frm += sizeof(tsf);
	ie->len = frm - &ie->qtn_ie_oui[0];

	return frm;
}

/*
 * Add 802.11h information elements to a frame.
 */
static u_int8_t *
ieee80211_add_doth(u_int8_t *frm, struct ieee80211com *ic)
{
	/* XXX ie structures */
	/*
	 * Power Capability IE
	 */
	if (ic->ic_flags_ext & IEEE80211_FEXT_TPC) {
		*frm++ = IEEE80211_ELEMID_PWRCAP;
		*frm++ = 2;
		*frm++ = ic->ic_bsschan->ic_minpower;
		*frm++ = ic->ic_bsschan->ic_maxpower;
	}

	return frm;
}

/*
 * Add 802.11n HT MCS
 */
static void
ieee80211_mcs_populate(struct ieee80211_node *ni, struct ieee80211_ie_htcap *ie, struct ieee80211_htcap *htcap, int subtype) {

	/* Update the supported MCS on Assoc response based on intersection of AP and client capability */
	if ((ni->ni_vap->iv_opmode == IEEE80211_M_HOSTAP) &&
		(subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP || subtype == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)) {
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS1,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS1] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_NSS1]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS2,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS2] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_NSS2]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS3,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS3] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_NSS3]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS4,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS4] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_NSS4]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM1,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM1] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM1]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM2,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM2] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM2]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM3,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM3] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM3]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM4,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM4] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM4]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM5,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM5] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM5]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM6,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM6] & ni->ni_htcap.mcsset[IEEE80211_HT_MCSSET_20_40_UEQM6]);
	} else {
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS1,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS1]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS2,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS2]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS3,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS3]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_NSS4,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_NSS4]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM1,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM1]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM2,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM2]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM3,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM3]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM4,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM4]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM5,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM5]);
		IEEE80211_HTCAP_SET_MCS_VALUE(ie, IEEE80211_HT_MCSSET_20_40_UEQM6,
						  htcap->mcsset[IEEE80211_HT_MCSSET_20_40_UEQM6]);
	}

}

/*
 * Add 802.11n HT Capabilities IE
 */
u_int8_t *
ieee80211_add_htcap(struct ieee80211_node *ni, u_int8_t *frm, struct ieee80211_htcap *htcap, int subtype)
{
	struct ieee80211_ie_htcap *ie = (struct ieee80211_ie_htcap *)(void*) frm;

	memset(ie, 0, sizeof(struct ieee80211_ie_htcap));

	ie->hc_id = IEEE80211_ELEMID_HTCAP;
	ie->hc_len = sizeof(struct ieee80211_ie_htcap) - 2;

	/* Update the LDPC capability based on the setting */
	if (ni->ni_vap->iv_ht_flags & IEEE80211_HTF_LDPC_ENABLED) {
		htcap->cap |= IEEE80211_HTCAP_C_LDPCCODING;
	} else {
		htcap->cap &= ~IEEE80211_HTCAP_C_LDPCCODING;
	}

	/* Update the STBC capability based on the setting */
	if (ni->ni_vap->iv_ht_flags & IEEE80211_HTF_STBC_ENABLED) {
		htcap->cap |= (IEEE80211_HTCAP_C_TXSTBC | IEEE80211_HTCAP_C_RXSTBC);
	} else {
		htcap->cap &= ~(IEEE80211_HTCAP_C_TXSTBC | IEEE80211_HTCAP_C_RXSTBC);
	}

	IEEE80211_HTCAP_SET_CAPABILITIES(ie,htcap->cap);

	if (ni->ni_vap->iv_smps_force & 0x8000) {
		IEEE80211_HTCAP_SET_PWRSAVE_MODE(ie, ni->ni_vap->iv_smps_force & 0xF);
	} else {
		IEEE80211_HTCAP_SET_PWRSAVE_MODE(ie,htcap->pwrsave);
	}

	IEEE80211_HTCAP_SET_AMPDU_LEN(ie,htcap->maxampdu);
	IEEE80211_HTCAP_SET_AMPDU_SPACING(ie,htcap->mpduspacing);

	ieee80211_mcs_populate(ni, ie, htcap, subtype);

	IEEE80211_HTCAP_SET_HIGHEST_DATA_RATE(ie,htcap->maxdatarate);
	IEEE80211_HTCAP_SET_MCS_PARAMS(ie,htcap->mcsparams);
	IEEE80211_HTCAP_SET_MCS_STREAMS(ie,htcap->numtxspstr);

	ie->hc_txbf[0] = htcap->hc_txbf[0];
	ie->hc_txbf[1] = htcap->hc_txbf[1];
	ie->hc_txbf[2] = htcap->hc_txbf[2];
	ie->hc_txbf[3] = htcap->hc_txbf[3];

	return frm + sizeof(struct ieee80211_ie_htcap);
}


/*
 * Add 802.11n HT Information IE
 */
u_int8_t *
ieee80211_add_htinfo(struct ieee80211_node *ni, u_int8_t *frm, struct ieee80211_htinfo *htinfo)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic  = vap->iv_ic;		/* back ptr to common state */
	u_int8_t byteval = 0;
	struct ieee80211_ie_htinfo *ie = (struct ieee80211_ie_htinfo *)(void*) frm;
	memset(ie, 0, sizeof(struct ieee80211_ie_htinfo));
	ie->hi_id = IEEE80211_ELEMID_HTINFO;
	ie->hi_len = sizeof(struct ieee80211_ie_htinfo) - 2;
	IEEE80211_HTINFO_SET_PRIMARY_CHANNEL(ie,htinfo->ctrlchannel);

	/* set byte 1 */
	byteval = 0;

	/* set channel width */
	byteval |= (htinfo->byte1 & IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40);

	/*
	 * Std 802.11ac-2013, 10.39.1 'Basic VHT BSS functionality': A VHT
	 * AP shall set the RIFS Mode field in the HT Operation element to 0.
	 */
	if (!IS_IEEE80211_VHT_ENABLED(ic) ||
			(vap->iv_opmode != IEEE80211_M_HOSTAP)) {
		/* Rx RIFS is supported */
		byteval |= IEEE80211_HTINFO_B1_RIFS_MODE;
	}

	/* set S-PSMP support */
	/* Deprecated in current draft 11.0 */
// 	byteval |= (htinfo->byte1 & IEEE80211_HTINFO_B1_CONTROLLED_ACCESS);

	IEEE80211_HTINFO_SET_BYTE_ONE(ie,byteval);

	/* set service level granularity and secondary channel offset */
	/* Deprecated in current draft 11.0 */
	//IEEE80211_HTINFO_B1_SET_SIGRANULARITY(ie,htinfo->sigranularity);
	//
	IEEE80211_HTINFO_B1_SET_EXT_CHOFFSET(ie,htinfo->choffset);

	/* set byte 2 */
	byteval = 0;

	/* set op mode */
	if (IEEE80211_11N_PROTECT_ENABLED(ic) &&
			(vap->iv_opmode != IEEE80211_M_IBSS)) {
		if (vap->iv_non_gf_sta_present)
			byteval |= IEEE80211_HTINFO_B2_NON_GF_PRESENT;

		if (ic->ic_non_ht_non_member || ic->ic_non_ht_sta)
			byteval |= IEEE80211_HTINFO_B2_OBSS_PROT;
	}

	/* set OBSS */
	IEEE80211_HTINFO_SET_BYTE_TWO(ie,byteval);

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		u_int8_t opmode = 0;
		if (ic->ic_non_ht_sta != 0) {
			opmode = IEEE80211_HTINFO_OPMODE_HT_PROT_MIXED;
		} else {
			if (ic->ic_non_ht_non_member != 0) {
				opmode = IEEE80211_HTINFO_OPMODE_HT_PROT_NON_MEM;
			} else {
				if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) { /* 20/40 MHZ mode */
					if (ic->ic_ht_20mhz_only_sta != 0) /* 20 MHZ only HT STA is present */
						opmode = IEEE80211_HTINFO_OPMODE_HT_PROT_20_ONLY;
					else
						opmode = IEEE80211_HTINFO_OPMODE_NO_PROT;
				} else {
					opmode = IEEE80211_HTINFO_OPMODE_NO_PROT;
				}
			}
		}

		/*
		 * If nonHT, 20MHz, 'nonHT in other BSS' stations counts are all 0, then we have
		 * a QTN specific usage of HT protection field. If any of those counters are non-zero,
		 * then ht protection field is set as per standard. Otherwise WFA test cases fail.
		*/
		if (!ic->ic_non_ht_sta && !ic->ic_ht_20mhz_only_sta && !ic->ic_non_ht_non_member) {
			/* QTN specific settings */
			if ((!IEEE80211_COM_WDS_IS_RBS(ic) || !ic->ic_extender_mbs_ocac) && !ic->ic_peer_rts) {
				opmode = IEEE80211_HTINFO_OPMODE_NO_PROT;
			} else {
				opmode = IEEE80211_HTINFO_OPMODE_HT_PROT_NON_MEM;
			}

		}

		if (!IEEE80211_11N_PROTECT_ENABLED(ic))
			opmode = IEEE80211_HTINFO_OPMODE_NO_PROT;

		htinfo->opmode = opmode;
		IEEE80211_HTINFO_B2_SET_OP_MODE(ie, htinfo->opmode);
	}

	/* set byte 3 */
	IEEE80211_HTINFO_SET_BYTE_THREE(ie,0);

	/* set byte 4 */
	byteval = 0;

	if (vap->iv_opmode != IEEE80211_M_IBSS)
	{
		/* set dual beacon */
		byteval |= (htinfo->byte4 & IEEE80211_HTINFO_B4_DUAL_BEACON);

		/* set DUAL CTS requirement */
		if (vap->iv_dual_cts_required)
				byteval |= (IEEE80211_HTINFO_B4_DUAL_CTS);
	}

	IEEE80211_HTINFO_SET_BYTE_FOUR(ie,byteval);

	/* set byte 5 */
	byteval = 0;
	if (vap->iv_opmode != IEEE80211_M_IBSS) {
		/* set STBC beacon */
		if (vap->iv_stbc_beacon)
			byteval |= (IEEE80211_HTINFO_B5_STBC_BEACON);

		/* set LSIG TXOP support */
		if (vap->iv_lsig_txop_ok)
			byteval |= (IEEE80211_HTINFO_B5_LSIGTXOPPROT);
	}

	IEEE80211_HTINFO_SET_BYTE_FIVE(ie,byteval);

	IEEE80211_HTINFO_SET_BASIC_MCS_VALUE(ie,IEEE80211_HT_MCSSET_20_40_NSS1,htinfo->basicmcsset[0]);
	IEEE80211_HTINFO_SET_BASIC_MCS_VALUE(ie,IEEE80211_HT_MCSSET_20_40_NSS2,htinfo->basicmcsset[1]);

	return frm + sizeof(struct ieee80211_ie_htinfo);
}


/*
 * Add 802.11ac VHT Capabilities IE
 */
u_int8_t *
ieee80211_add_vhtcap(struct ieee80211_node *ni, u_int8_t *frm, struct ieee80211_vhtcap *vhtcap, uint8_t subtype)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_ie_vhtcap *ie = (struct ieee80211_ie_vhtcap *)(void *) frm;
	u_int32_t vhtcap_flags = vhtcap->cap_flags;
	uint32_t bfstscap;

	memset(ie, 0, sizeof(struct ieee80211_ie_vhtcap));

	ie->vht_id = IEEE80211_ELEMID_VHTCAP;
	ie->vht_len = sizeof(struct ieee80211_ie_vhtcap) - 2;

	if (vap->iv_vht_flags & IEEE80211_VHTCAP_C_RX_LDPC) {
		vhtcap_flags |= IEEE80211_VHTCAP_C_RX_LDPC;
	} else {
		vhtcap_flags &= ~IEEE80211_VHTCAP_C_RX_LDPC;
	}
	if (vap->iv_vht_flags & IEEE80211_VHTCAP_C_TX_STBC) {
		vhtcap_flags |= IEEE80211_VHTCAP_C_TX_STBC;
	} else {
		vhtcap_flags &= ~IEEE80211_VHTCAP_C_TX_STBC;
	}

	if (ic->ic_vhtcap.cap_flags & IEEE80211_VHTCAP_C_MU_BEAM_FORMER_CAP) {
		vhtcap_flags |= IEEE80211_VHTCAP_C_MU_BEAM_FORMER_CAP;
	} else {
		vhtcap_flags &= ~IEEE80211_VHTCAP_C_MU_BEAM_FORMER_CAP;
	}

	if (ic->ic_vhtcap.cap_flags & IEEE80211_VHTCAP_C_MU_BEAM_FORMEE_CAP) {
		vhtcap_flags |= IEEE80211_VHTCAP_C_MU_BEAM_FORMEE_CAP;
	} else {
		vhtcap_flags &= ~IEEE80211_VHTCAP_C_MU_BEAM_FORMEE_CAP;
	}

	IEEE80211_VHTCAP_SET_CAPFLAGS(ie, vhtcap_flags);

	IEEE80211_VHTCAP_SET_MAXMPDU(ie, vhtcap->maxmpdu);
	IEEE80211_VHTCAP_SET_CHANWIDTH(ie, vhtcap->chanwidth);
	if (ni->ni_vap->iv_vht_flags & IEEE80211_VHTCAP_C_TX_STBC) {
		IEEE80211_VHTCAP_SET_RXSTBC(ie, vhtcap->rxstbc);
	}

	/* FIXME: IOT Workaround for BRCM to set beamformee STS to 3 in beacons
	 * BRCM sound with us assuming 2 antenna  when STS is set to our max value which is 4
	 * However when beacons advertise it as 3, the sounding is done in 3 antenna.
	 * Intel, QCA, MRVL takes the STS value from probe resp / assoc resp.
	 * So having STS 4 in those frames can make use of 4 antenna in V matrix.
	 */
	bfstscap = (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
				? MIN(vhtcap->bfstscap, IEEE80211_VHTCAP_RX_STS_3)
				: vhtcap->bfstscap;

	IEEE80211_VHTCAP_SET_BFSTSCAP(ie, bfstscap);
	IEEE80211_VHTCAP_SET_NUMSOUND(ie, vhtcap->numsounding);
	IEEE80211_VHTCAP_SET_MAXAMPDUEXP(ie, vhtcap->maxampduexp);
	IEEE80211_VHTCAP_SET_LNKADPTCAP(ie, vhtcap->lnkadptcap);

	IEEE80211_VHTCAP_SET_RX_MCS_NSS(ie, vhtcap->rxmcsmap);

	IEEE80211_VHTCAP_SET_TX_MCS_NSS(ie, vhtcap->txmcsmap);

	IEEE80211_VHTCAP_SET_RX_LGIMAXRATE(ie, vhtcap->rxlgimaxrate);
	IEEE80211_VHTCAP_SET_TX_LGIMAXRATE(ie, vhtcap->txlgimaxrate);

	return frm + sizeof(struct ieee80211_ie_vhtcap);
}

/*
 * 802.11ac VHT Operation IE
 */
uint8_t *ieee80211_add_vhtop(struct ieee80211_node *ni, uint8_t *frm, struct ieee80211_vhtop *vhtop)
{
	struct ieee80211_ie_vhtop *ie = (struct ieee80211_ie_vhtop *)frm;

	memset (ie, 0, sizeof(struct ieee80211_ie_vhtop));

	ie->vhtop_id = IEEE80211_ELEMID_VHTOP;
	ie->vhtop_len = sizeof(struct ieee80211_ie_vhtop) - 2;

	IEEE80211_VHTOP_SET_CHANWIDTH(ie, vhtop->chanwidth);
	IEEE80211_VHTOP_SET_CENTERFREQ0(ie, vhtop->centerfreq0);
	IEEE80211_VHTOP_SET_CENTERFREQ1(ie, vhtop->centerfreq1);

	IEEE80211_VHTOP_SET_BASIC_MCS_NSS(ie, vhtop->basicvhtmcsnssset);

	return frm + sizeof(struct ieee80211_ie_vhtop);
}

u_int8_t ieee80211_get_peer_nss(struct ieee80211_node *ni)
{
	u_int8_t nss = 0;

	if (IEEE80211_VHT_HAS_4SS(ni->ni_vhtcap.txmcsmap)) {
		nss = 3;
	} else if (IEEE80211_VHT_HAS_3SS(ni->ni_vhtcap.txmcsmap)) {
		nss = 2;
	} else if (IEEE80211_VHT_HAS_2SS(ni->ni_vhtcap.txmcsmap)) {
		nss = 1;
	}

	return nss;
}

/*
 * Add 802.11ac VHT Operating Mode Notification IE
 */
uint8_t *ieee80211_add_vhtop_notif(struct ieee80211_node *ni, uint8_t *frm, struct ieee80211com *ic, int band_24g)
{
	struct ieee80211_ie_vhtop_notif *ie = (struct ieee80211_ie_vhtop_notif *)frm;
	uint8_t chwidth;
	uint8_t vht_nss_cap = (band_24g ? ic->ic_vht_nss_cap_24g : ic->ic_vht_nss_cap);
	uint8_t rxnss = min(vht_nss_cap - 1, QTN_GLOBAL_RATE_NSS_MAX - 1);
	uint8_t rxnss_type = 0;
	struct ieee80211vap *vap = ni->ni_vap;

	switch (ic->ic_max_system_bw) {
	case BW_HT40:
		chwidth = IEEE80211_CWM_WIDTH40;
		break;
	case BW_HT20:
		chwidth = IEEE80211_CWM_WIDTH20;
		break;
	case BW_HT80:
	default:
		chwidth = IEEE80211_CWM_WIDTH80;
		break;
	}

	if (band_24g && chwidth == IEEE80211_CWM_WIDTH80)
		chwidth = IEEE80211_CWM_WIDTH40;

	memset(ie, 0, sizeof(struct ieee80211_ie_vhtop_notif));

	ie->id = IEEE80211_ELEMID_OPMOD_NOTIF;
	ie->len = sizeof(*ie) - 2;

	if (vap->iv_opmode == IEEE80211_M_STA) {
		rxnss = min((uint8_t)(vht_nss_cap - 1), (uint8_t)ieee80211_get_peer_nss(ni));
	}

	if (ic->ic_vht_opmode_notif == IEEE80211_VHT_OPMODE_NOTIF_DEFAULT) {
		/* by default, use current configured channel width and nss */
		ie->vhtop_notif_mode = SM(chwidth, IEEE80211_VHT_OPMODE_CHWIDTH) |
					SM(rxnss, IEEE80211_VHT_OPMODE_RXNSS) |
					SM(rxnss_type, IEEE80211_VHT_OPMODE_RXNSS_TYPE);
	} else {
		ie->vhtop_notif_mode = (uint8_t) (ic->ic_vht_opmode_notif & 0x00ff);
	}

	return frm + sizeof(*ie);
}

/*
 * Add 20/40 coexistence IE
 */
u_int8_t *
ieee80211_add_20_40_bss_coex_ie(u_int8_t *frm, u_int8_t coex)
{
	*frm++ = IEEE80211_ELEMID_20_40_BSS_COEX;
	*frm++ = 1;
	*frm++ = coex;

	return frm;
}

void
ieee80211_get_20_40_bss_into_chan_list(struct ieee80211com *ic,
		struct ieee80211vap *vap, u_int16_t *pp_ch_list)
{
	struct sta_table *st = ic->ic_scan->ss_priv;
	struct sta_entry *se, *next;
	uint16_t ch_list = 0;
	uint8_t se_pri_chan = 0;
	uint8_t se_sec_chan = 0;
	uint8_t unallowed = 0;

	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		if (!IEEE80211_ADDR_EQ(se->base.se_macaddr, vap->iv_bss->ni_macaddr) &&
				(se->base.se_chan->ic_ieee <= QTN_2G_LAST_OPERATING_CHAN)) {
			ieee80211_find_ht_pri_sec_chan(vap, &se->base,
						&se_pri_chan, &se_sec_chan);
			unallowed = !ieee80211_20_40_operation_permitted(vap,
						se_pri_chan, se_sec_chan);
		} else {
			unallowed = 0;
		}

		if (unallowed)
			ch_list |= 1 << se->base.se_chan->ic_ieee;
	}
	*pp_ch_list = ch_list;
}

static uint8_t
ieee80211_count_channels(uint16_t ch_list)
{
	uint8_t chan_count = 0;

	for ( ; ch_list; ch_list &= (ch_list - 1))
		chan_count++;

	return chan_count;
}

/*
 * Add 20/40 BSS channel report
 */
u_int8_t *
ieee80211_add_20_40_bss_into_ch_rep(u_int8_t *frm, struct ieee80211com *ic, u_int16_t ch_list)
{
#define IEEE80211_24GHZ_BAND 25
#define IEEE80211_GLOBAL_24GHZ_OPER_CLASS 81
#define bitsz_var(var) (sizeof(var) * 8)
	int i;
	uint8_t cur_reg_class = 0;

	for (i = 1; i < bitsz_var(ch_list); i++) {
		if (ch_list & (1 << i)) {
			cur_reg_class = ieee80211_get_current_operating_class(ic->ic_country_code,
						ic->ic_bsschan->ic_ieee,
						IEEE80211_24GHZ_BAND);
			if (!cur_reg_class)
				cur_reg_class = IEEE80211_GLOBAL_24GHZ_OPER_CLASS;
			break;
		}
	}

	*frm++ = IEEE80211_ELEMID_20_40_IT_CH_REP;
	*frm++ = ieee80211_count_channels(ch_list) + 1;
	*frm++ = cur_reg_class;
	for (i = 1; i < bitsz_var(ch_list); i++) {
		if (ch_list & (1 << i)) {
			*frm++ = i;
		}
	}

	return frm;
}

u_int8_t *
ieee80211_add_obss_scan_ie(u_int8_t *frm, struct ieee80211_obss_scan_ie *obss_ie)
{
	*frm++ = IEEE80211_ELEMID_OBSS_SCAN;
	*frm++ = sizeof(struct ieee80211_obss_scan_ie) - 2;
	memcpy(frm, &obss_ie->obss_passive_dwell, sizeof(struct ieee80211_obss_scan_ie) - 2);

	return (frm + sizeof(struct ieee80211_obss_scan_ie) - 2);
}

/*
* Add Extender Role IE
*/
u_int8_t *
ieee80211_add_qtn_extender_role_ie(uint8_t *frm, uint8_t role)
{
	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = sizeof(struct ieee80211_qtn_ext_role) - 2;
	frm += ieee80211_oui_add_qtn(frm);
	*frm++ = QTN_OUI_EXTENDER_ROLE;
	*frm++ = role;
	return frm;
}

u_int8_t *
ieee80211_add_qtn_extender_bssid_ie(struct ieee80211vap *vap, uint8_t *frm)
{
	struct ieee80211com *ic = vap->iv_ic;
	int i;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = sizeof(struct ieee80211_qtn_ext_bssid) - 2;
	frm += ieee80211_oui_add_qtn(frm);
	*frm++ = QTN_OUI_EXTENDER_BSSID;

	memcpy(frm, ic->ic_extender_mbs_bssid, IEEE80211_ADDR_LEN);
	frm = frm + IEEE80211_ADDR_LEN;
	*frm++ = ic->ic_extender_rbs_num;
	for (i = 0; i < QTN_MAX_RBS_NUM; i++) {
		memcpy(frm, ic->ic_extender_rbs_bssid[i], IEEE80211_ADDR_LEN);
		frm = frm + IEEE80211_ADDR_LEN;
	}

	return frm;
}

u_int8_t *
ieee80211_add_qtn_extender_state_ie(uint8_t *frm, uint8_t ocac)
{
	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = sizeof(struct ieee80211_qtn_ext_state) - 2;
	frm += ieee80211_oui_add_qtn(frm);
	*frm++ = QTN_OUI_EXTENDER_STATE;
	*frm++ = (ocac ? QTN_EXT_MBS_OCAC : 0);
	*frm++ = 0;
	*frm++ = 0;
	*frm++ = 0;

	return frm;
}

/**
 * Find the next IE in the buffer, return pointer to the next IE,
 * and the length of the current IE.
 */
static void *
ieee80211_smash_ie(const void *p_buf, int buf_len, int *p_this_ie_len)
{
	struct ieee80211_ie *p_ie = (struct ieee80211_ie *)p_buf;
	struct ieee80211_ie *p_next_ie;
	if (p_buf == NULL || buf_len < IEEE80211_IE_ID_LEN_SIZE) {
		return NULL;
	}
	if ((p_ie->len + IEEE80211_IE_ID_LEN_SIZE) > buf_len) {
		return NULL;
	}
	*p_this_ie_len = p_ie->len;
	p_next_ie = (struct ieee80211_ie *)(p_buf + p_ie->len + IEEE80211_IE_ID_LEN_SIZE);
	if ((u32)p_next_ie < (u32)(p_buf + buf_len)) {
		return p_next_ie;
	}
	return NULL;
}

/*
 * Create a probe request frame with the specified ssid
 * and any optional information element data.
 */
struct sk_buff *
ieee80211_get_probereq(struct ieee80211_node *ni,
	const u_int8_t sa[IEEE80211_ADDR_LEN],
	const u_int8_t da[IEEE80211_ADDR_LEN],
	const u_int8_t bssid[IEEE80211_ADDR_LEN],
	const u_int8_t *ssid, size_t ssidlen,
	const void *optie, size_t optielen)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	enum ieee80211_phymode mode;
	struct ieee80211_frame *wh;
	struct sk_buff *skb;
	u_int8_t *frm;

	mode = ic->ic_curmode;
	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 *	[tlv] HT capabilities
	 *	[tlv] user-specified ie's
	 */
	skb = ieee80211_getmgtframe(&frm, 2 + IEEE80211_NWID_LEN +
	       2 + IEEE80211_RATE_SIZE +
	       2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE) +
	       ((ic->ic_curmode >= IEEE80211_MODE_11NA) ?
			(sizeof(struct ieee80211_ie_htcap) +
			 sizeof(struct ieee80211_extcap_param)) : 0) +
	       (optie != NULL ? optielen : 0) +
	       vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_REQ].length +
	       (IS_IEEE80211_DUALBAND_VHT_ENABLED(ic) ? sizeof(struct ieee80211_ie_vhtcap): 0)
	       );

	if (skb == NULL) {
		vap->iv_stats.is_tx_nobuf++;
		return NULL;
	}

	frm = ieee80211_add_ssid(frm, ssid, ssidlen);
	frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);

	if (ic->ic_curmode >= IEEE80211_MODE_11NA) {
		frm = ieee80211_add_htcap(ni, frm, &ic->ic_htcap, IEEE80211_FC0_SUBTYPE_PROBE_REQ);
		/* Ext. Capabilities - For AP mode hostapd adds the extended cap */
		if (vap->iv_opmode == IEEE80211_M_STA)
			frm = ieee80211_add_extcap(frm);
	}

	frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);

	if (IS_IEEE80211_VHT_ENABLED(ic)) {
		frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap, IEEE80211_FC0_SUBTYPE_PROBE_REQ);
	} else if (IS_IEEE80211_11NG_VHT_ENABLED(ic)) {
		/* QTN 2.4G VHT IE */
		frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap_24g, IEEE80211_FC0_SUBTYPE_PROBE_REQ);
	}

	/* Only add in vendor IEs to the probe request frame. */
	if (optie != NULL) {
		struct ieee80211_ie *p_ie = (struct ieee80211_ie *)optie;
		struct ieee80211_ie *p_next_ie;
		int this_len = 0;
		do {
			p_next_ie = ieee80211_smash_ie(p_ie, optielen, &this_len);
			if (p_ie && this_len) {
				/**
				 * Rules for probe request is that vendor elements can be appended,
				 * and only a few of the standard IEs. optie as passed in can
				 * contain any one of a number of elements.
				 */
				if (p_ie->id == IEEE80211_ELEMID_VENDOR) {
					if (((struct ieee80211_ie_wme *)p_ie)->wme_type == WPA_OUI_TYPE) {
						continue;
					}
					memcpy(frm, p_ie, this_len + IEEE80211_IE_ID_LEN_SIZE);
					frm += this_len + IEEE80211_IE_ID_LEN_SIZE;
				}
				p_ie = p_next_ie;
				optielen -= this_len + IEEE80211_IE_ID_LEN_SIZE;
			}
		} while (p_next_ie != NULL);
	}

	if (vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_REQ].ie) {
		memcpy(frm, vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_REQ].ie,
			vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_REQ].length);
		frm += vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_REQ].length;
	}

	skb_trim(skb, frm - skb->data);

	wh = (struct ieee80211_frame *)
		skb_push(skb, sizeof(struct ieee80211_frame));
	ieee80211_send_setup(vap, ni, wh,
		IEEE80211_FC0_TYPE_MGT,
		IEEE80211_FC0_SUBTYPE_PROBE_REQ,
		sa, da, bssid);

	/* FIXME power management? */

	IEEE80211_NODE_STAT(ni, tx_probereq);
	IEEE80211_NODE_STAT(ni, tx_mgmt);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
		"[%s] send probe req on channel %u\n",
		ether_sprintf(wh->i_addr1),
		ieee80211_chan2ieee(ic, ic->ic_curchan));

	return skb;
}
EXPORT_SYMBOL(ieee80211_get_probereq);

/*
 * Send a probe request frame with the specified ssid
 * and any optional information element data.
 */
int
ieee80211_send_probereq(struct ieee80211_node *ni,
	const u_int8_t sa[IEEE80211_ADDR_LEN],
	const u_int8_t da[IEEE80211_ADDR_LEN],
	const u_int8_t bssid[IEEE80211_ADDR_LEN],
	const u_int8_t *ssid, size_t ssidlen,
	const void *optie, size_t optielen)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb;

	ieee80211_ref_node(ni);

	skb = ieee80211_get_probereq(ni, sa, da, bssid,
			ssid, ssidlen, optie, optielen);
	if (skb == NULL) {
		ieee80211_free_node(ni);
		return -ENOMEM;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
			"[%s] send probe req frame on channel %u\n",
			ether_sprintf(ni->ni_macaddr),
			ieee80211_chan2ieee(ic, ic->ic_curchan));
	ic->ic_send_80211(ic, ni, skb, WME_AC_BE, 0);

	return 0;
}

/* Send a broadcast CSA frame, announcing the new channel. References are from
 * IEEE 802.11h-2003. CSA frame format is an "Action" frame (Type: 00, Subtype:
 * 1101, see 7.1.3.1.2)
 *
 * [1] Category : 0, Spectrum Management, 7.3.1.11
 * [1] Action : 4, Channel Switch Announcement, 7.4.1 and 7.4.1.5
 * [1] Element ID : 37, Channel Switch Announcement, 7.3.2
 * [1] Length : 3, 7.3.2.20
 * [1] Channel Switch Mode : 1, stop transmission immediately
 * [1] New Channel Number
 * [1] Channel Switch Count in TBTT : 0, immediate channel switch
 *
 * csa_mode : IEEE80211_CSA_CAN_STOP_TX / IEEE80211_CSA_MUST_STOP_TX
 * csa_chan : new IEEE channel number
 * csa_tbtt : TBTT until Channel Switch happens
*/
void
ieee80211_send_csa_frame(struct ieee80211vap *vap,
				u_int8_t csa_mode,
				u_int8_t csa_chan,
				u_int8_t csa_count,
				u_int64_t tsf)
{
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211com *ic = ni->ni_ic;
	uint32_t bw = ieee80211_get_bw(ic);
	uint8_t wband_chanswitch_ie_len = ieee80211_wband_chanswitch_ie_len(bw);
	struct sk_buff *skb;
	int frm_len;
	u_int8_t *frm;

	frm_len = IEEE80211_CSA_LEN + ieee80211_sec_chan_off_ie_len() +
			wband_chanswitch_ie_len;

	if (tsf != 0) {
		frm_len += sizeof(struct ieee80211_ie_qtn_csa_tsf);
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
			"%s: Sending action frame with CSA IE: %u/%u/%u\n",
			__func__, csa_mode, csa_chan, csa_count);

	skb = ieee80211_getmgtframe(&frm, frm_len);
	if (skb == NULL) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
			"%s: cannot get buf; size %u", __func__, frm_len);
		vap->iv_stats.is_tx_nobuf++;
		return;
	}

	*frm++ = IEEE80211_ACTION_CAT_SPEC_MGMT;  /* Category */
	*frm++ = IEEE80211_ACTION_S_CHANSWITCHANN;      /* Spectrum Management */
	frm = ieee80211_add_csa(frm, csa_mode, csa_chan, csa_count);
	ieee80211_add_sec_chan_off(&frm, ic, csa_chan);

	if (wband_chanswitch_ie_len) {
		frm = ieee80211_add_wband_chanswitch(frm, ic);
	}

	if (tsf != 0) {
		ieee80211_add_qtn_csatsf_ie(frm, tsf);
	}

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		ieee80211_ref_node(ni);
		ieee80211_mgmt_output(ni, skb, IEEE80211_FC0_SUBTYPE_ACTION,
					vap->iv_dev->broadcast);
	} else {
		/* STA mode - tell AP to change channel */
		ieee80211_ref_node(ni);
		ieee80211_mgmt_output(ni, skb, IEEE80211_FC0_SUBTYPE_ACTION,
					ni->ni_bssid);
	}
}
EXPORT_SYMBOL(ieee80211_send_csa_frame);

#ifdef CONFIG_QVSP
static int
ieee80211_compile_action_qvsp_frame(struct ieee80211vap *vap, struct ieee80211_qvsp_act *qvsp_a,
				struct sk_buff **pp_skb)
{
	struct sk_buff *skb = NULL;

	switch (qvsp_a->type) {
	case QVSP_ACTION_STRM_CTRL: {
		struct ieee80211_qvsp_act_strm_ctrl *qvsp_asc =
			(struct ieee80211_qvsp_act_strm_ctrl *)qvsp_a;
		int total_len;
		struct ieee80211_qvsp_act_strm_ctrl_s *qa;
		struct ieee80211_qvsp_strm_id *qai;
		struct ieee80211_qvsp_strm_id *qvsp_asc_i = &qvsp_asc->strm_items[0];
		int i;
		u_int8_t *frm;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,
				"VSP: constructing stream ctrl frame\n", 0);

		if (qvsp_asc->count > IEEE8021_QVSP_MAX_ACT_ITEMS) {
			printk(KERN_INFO "VSP: truncating strm ctrl frame - too long\n");
			qvsp_asc->count = IEEE8021_QVSP_MAX_ACT_ITEMS;
		}

		total_len = sizeof(*qa) + (qvsp_asc->count * sizeof(*qai));
		KASSERT(total_len <= IEEE80211_MTU_MAX, ("VSP: strm ctrl frame is too large"));
		skb = ieee80211_getmgtframe(&frm, total_len);
		if (!skb) {
			return -ENOMEM;
		}
		/* Common header */
		qa = (struct ieee80211_qvsp_act_strm_ctrl_s *)frm;
		qai = &qa->strm_items[0];
		qa->header.category = IEEE80211_ACTION_CAT_VENDOR;
		ieee80211_oui_add_qtn(qa->header.oui);
		qa->header.type = QVSP_ACTION_TYPE_VSP;
		qa->header.action = qvsp_a->type;
		qa->strm_state = qvsp_asc->strm_state;
		qa->dis_attr.throt_policy = qvsp_asc->dis_attr.throt_policy;
		qa->dis_attr.throt_rate = qvsp_asc->dis_attr.throt_rate;
		qa->dis_attr.demote_rule = qvsp_asc->dis_attr.demote_rule;
		qa->dis_attr.demote_state = qvsp_asc->dis_attr.demote_state;
		qa->count = qvsp_asc->count;
		/* Set state for one or more streams */
		for (i = 0; i < qa->count; i++) {
			*qai++ = *qvsp_asc_i++;
		}
		break;
	}
	case QVSP_ACTION_VSP_CTRL: {
		struct ieee80211_qvsp_act_cfg *qvsp_ac = (struct ieee80211_qvsp_act_cfg *)qvsp_a;
		int total_len;
		struct ieee80211_qvsp_act_vsp_ctrl_s *qa;
		struct ieee80211_qvsp_act_vsp_ctrl_item_s *qai;
		struct ieee80211_qvsp_act_cfg_item *qvsp_ac_i = &qvsp_ac->cfg_items[0];
		int i;
		u_int8_t *frm;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,
				"VSP: constructing cfg frame\n", 0);

		if (qvsp_ac->count > IEEE8021_QVSP_MAX_ACT_ITEMS) {
			printk(KERN_INFO "VSP: truncating cfg frame - too long\n");
			qvsp_ac->count = IEEE8021_QVSP_MAX_ACT_ITEMS;
		}
		total_len = sizeof(*qa) + (qvsp_ac->count * sizeof(*qai));

		KASSERT(total_len <= IEEE80211_MTU_MAX, ("VSP: cfg frame is too large"));
		skb = ieee80211_getmgtframe(&frm, total_len);
		if (!skb) {
			return -ENOMEM;
		}
		/* Common header */
		qa = (struct ieee80211_qvsp_act_vsp_ctrl_s *)frm;
		qai = &qa->ctrl_items[0];
		qa->header.category = IEEE80211_ACTION_CAT_VENDOR;
		ieee80211_oui_add_qtn(qa->header.oui);
		qa->header.type = QVSP_ACTION_TYPE_VSP;
		qa->header.action = qvsp_a->type;
		qa->count = qvsp_ac->count;
		/* Zero or more config index/value pairs. */
		for (i = 0; i < qa->count; i++) {
			qai->index = htonl(qvsp_ac_i->index);
			qai->value = htonl(qvsp_ac_i->value);
			qai++;
			qvsp_ac_i++;
		}
		break;
	}
	default:
		break;
	}

	if (skb) {
		*pp_skb = skb;
		return 0;
	}

	return -EINVAL;
}
#endif

static int
ieee80211_compile_action_20_40_coex_frame(struct ieee80211vap *vap, struct ieee80211_action_data *action_data,
				struct sk_buff **pp_skb, struct ieee80211_node *ni)
{
	struct sk_buff *skb;
	int32_t frame_len = 0;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *frm;
	uint8_t *coex_value = (uint8_t *)action_data->params;
	uint8_t coex = vap->iv_coex;
	uint16_t ch_list = 0;

	if (coex_value)
		coex = *coex_value;

	frame_len = sizeof(struct ieee80211_action) + sizeof(struct ieee80211_20_40_coex_param);

	if (ic->ic_opmode == IEEE80211_M_STA) {
		uint8_t chan_count;
		ieee80211_get_20_40_bss_into_chan_list(ic, vap, &ch_list);
		chan_count = ieee80211_count_channels(ch_list);
		if (chan_count) {
			frame_len +=  sizeof(struct ieee80211_20_40_in_ch_rep) + chan_count;
			coex |=  WLAN_20_40_BSS_COEX_20MHZ_WIDTH_REQ;
		}
	}

	skb = ieee80211_getmgtframe(&frm, frame_len);
	if (skb == NULL)
		return -1;

	*frm++ = action_data->cat;
	*frm++ = action_data->action;

	frm = ieee80211_add_20_40_bss_coex_ie(frm, coex);

	if (ic->ic_opmode == IEEE80211_M_STA && ch_list) {
		frm = ieee80211_add_20_40_bss_into_ch_rep(frm, ic, ch_list);
	}

	ni->ni_coex = 0;
	skb_trim(skb, frm - skb->data);

	if (skb) {
		*pp_skb = skb;
	}

	return 0;

}

static int
ieee80211_compile_action_sa_query_frame(struct ieee80211vap *vap, struct ieee80211_action_data *action_data,
				struct sk_buff **pp_skb){

	struct sk_buff *skb;
	int32_t frame_len = 0;
	u_int8_t *frm;
	uint16_t *tid = (uint16_t *)action_data->params;
	frame_len = sizeof(struct ieee80211_action_sa_query);

	skb = ieee80211_getmgtframe(&frm, frame_len);
	if (skb == NULL)
		return -1;

	*frm++ = IEEE80211_ACTION_CAT_SA_QUERY;
	*frm++ = action_data->action;
	ADDINT16(frm, *tid);

	skb_trim(skb, frm - skb->data);
	*pp_skb = skb;

	return 0;
}
int32_t ieee80211_measure_request_ie_len(struct ieee80211_meas_request_ctrl *mrequest_ctrl)
{
	int32_t meas_ie_len;

	switch (mrequest_ctrl->meas_type) {
	case IEEE80211_CCA_MEASTYPE_BASIC:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq);
		break;
	case IEEE80211_CCA_MEASTYPE_CCA:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq);
		break;
	case IEEE80211_CCA_MEASTYPE_RPI:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq);
		break;
	case IEEE80211_RM_MEASTYPE_STA:
	{
		int32_t cnt;
		ieee80211_11k_sub_element *p_se;
		ieee80211_11k_sub_element_head *se_head;

		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_sta_stat);

		if (mrequest_ctrl->u.sta_stats.sub_item == NULL) {
			break;
		}

		se_head = (ieee80211_11k_sub_element_head *)mrequest_ctrl->u.sta_stats.sub_item;
		SLIST_FOREACH(p_se, se_head, next) {
			switch (p_se->sub_id) {
			case IEEE80211_ELEMID_VENDOR:
			{
				struct stastats_subele_vendor *vendor;
				u_int32_t flags;

				meas_ie_len += sizeof(struct ieee80211_ie_qtn_rm_measure_sta);
				vendor = (struct stastats_subele_vendor *)p_se->data;
				flags = vendor->flags;

				if (!IEEE80211_IS_ALL_SET(flags, RM_QTN_MAX)) {
					meas_ie_len += 1;

					for (cnt = RM_QTN_TX_STATS; cnt <= RM_QTN_MAX; cnt++) {
						if (flags & (BIT(cnt)))
							meas_ie_len += 2;
					}

					for (cnt = RM_QTN_CTRL_START; cnt <= RM_QTN_CTRL_END; cnt++) {
						if (flags & (BIT(cnt)))
							meas_ie_len += 2;
					}
				}

				break;
			}
			default:
				break;
			}
		}
		break;
	}
	case IEEE80211_RM_MEASTYPE_QTN_CCA:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq);
		break;
	case IEEE80211_RM_MEASTYPE_CH_LOAD:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_chan_load);
		break;
	case IEEE80211_RM_MEASTYPE_NOISE:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_noise_his);
		break;
	case IEEE80211_RM_MEASTYPE_BEACON:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_beacon);
		break;
	case IEEE80211_RM_MEASTYPE_FRAME:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_frame);
		break;
	case IEEE80211_RM_MEASTYPE_CATEGORY:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_trans_stream_cat);
		break;
	case IEEE80211_RM_MEASTYPE_MUL_DIAG:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ sizeof(struct ieee80211_ie_measreq_multicast_diag);
		break;
	default:
		meas_ie_len = -1;
		break;
	}

	return meas_ie_len;
}

int32_t ieee80211_measure_report_ie_len(struct ieee80211_meas_report_ctrl *mreport_ctrl)
{
	int32_t meas_ie_len;

	/* measurement report filed would not exist if any bit of measurement report is set */
	switch (mreport_ctrl->meas_type) {
	case IEEE80211_CCA_MEASTYPE_BASIC:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? sizeof(struct ieee80211_ie_measrep_basic) : 0);
		break;
	case IEEE80211_CCA_MEASTYPE_CCA:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? sizeof(struct ieee80211_ie_measrep_cca) : 0);
		break;
	case IEEE80211_CCA_MEASTYPE_RPI:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? sizeof(struct ieee80211_ie_measrep_rpi) : 0);
		break;
	case IEEE80211_RM_MEASTYPE_STA:
	{
		int32_t cnt;

		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm);
		if (mreport_ctrl->report_mode == 0) {
			ieee80211_11k_sub_element *p_se;
			ieee80211_11k_sub_element_head *se_head;

			meas_ie_len += sizeof(struct ieee80211_ie_measrep_sta_stat);

			if (0 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group0);
			else if (1 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group1);
			else if (1 < mreport_ctrl->u.sta_stats.group_id && mreport_ctrl->u.sta_stats.group_id < 10)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group2to9);
			else if (10 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group10);
			else if (11 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group11);
			else if (12 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group12);
			else if (13 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group13);
			else if (14 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group14);
			else if (15 == mreport_ctrl->u.sta_stats.group_id)
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group15);
			else
				meas_ie_len += sizeof(struct ieee80211_rm_sta_stats_group16);

			if (mreport_ctrl->u.sta_stats.sub_item == NULL) {
				break;
			}

			se_head = (ieee80211_11k_sub_element_head *)mreport_ctrl->u.sta_stats.sub_item;
			/* optional sub element length */
			SLIST_FOREACH(p_se, se_head, next) {
				switch (p_se->sub_id) {
				case IEEE80211_ELEMID_VENDOR:
				{
					struct stastats_subele_vendor *vendor = (struct stastats_subele_vendor *)p_se->data;
					u_int32_t vendor_flags = vendor->flags;

					meas_ie_len += sizeof(struct ieee80211_ie_qtn_rm_measure_sta);
					if (IEEE80211_IS_ALL_SET(vendor_flags, RM_QTN_MAX)) {
						meas_ie_len += sizeof(struct ieee80211_ie_qtn_rm_sta_all);
					} else {
						meas_ie_len++;

						for (cnt = RM_QTN_TX_STATS; cnt <= RM_QTN_MAX; cnt++) {
							if (vendor_flags & (BIT(cnt))) {
								meas_ie_len += 2;
								meas_ie_len += ieee80211_meas_sta_qtn_report_subtype_len[cnt];
							}
						}

						for (cnt = RM_QTN_CTRL_START; cnt <= RM_QTN_CTRL_END; cnt++) {
							if (vendor_flags & (BIT(cnt))) {
								meas_ie_len += 2;
								meas_ie_len += ieee80211_meas_sta_qtn_report_subtype_len[cnt];
							}
						}
					}
					break;
				}
				default:
					break;
				}
			}
		}
		break;
	}
	case IEEE80211_RM_MEASTYPE_QTN_CCA:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ?
				sizeof(struct cca_rm_rep_data) + sizeof(struct ieee80211_ie_qtn_scs) : 0) +
			mreport_ctrl->u.qtn_cca.extra_ie_len;
		break;
	case IEEE80211_RM_MEASTYPE_CH_LOAD:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? (sizeof(struct ieee80211_ie_measrep_chan_load)) : (0));
		break;
	case IEEE80211_RM_MEASTYPE_NOISE:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? (sizeof(struct ieee80211_ie_measrep_noise_his)) : (0));
		break;
	case IEEE80211_RM_MEASTYPE_BEACON:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? (sizeof(struct ieee80211_ie_measrep_beacon)) : (0));
		break;
	case IEEE80211_RM_MEASTYPE_FRAME:
	{
		ieee80211_11k_sub_element *p_se;
		ieee80211_11k_sub_element_head *se_head;

		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm);

		if (mreport_ctrl->report_mode == 0) {
			meas_ie_len += sizeof(struct ieee80211_ie_measrep_frame);

			se_head = (ieee80211_11k_sub_element_head *)mreport_ctrl->u.frame.sub_item;
			SLIST_FOREACH(p_se, se_head, next) {
				switch (p_se->sub_id) {
				case IEEE80211_FRAME_REPORT_SUBELE_FRAME_COUNT_REPORT:
					meas_ie_len += sizeof(struct ieee80211_subie_section_frame_entry);
					break;
				default:
					break;
				}
			}
		}
		break;
	}
	case IEEE80211_RM_MEASTYPE_CATEGORY:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? (sizeof(struct ieee80211_ie_measrep_trans_stream_cat)) : (0));
		break;
	case IEEE80211_RM_MEASTYPE_MUL_DIAG:
		meas_ie_len = sizeof(struct ieee80211_ie_measure_comm)
			+ ((mreport_ctrl->report_mode == 0) ? (sizeof(struct ieee80211_ie_measrep_multicast_diag)) : (0));
		break;
	default:
		meas_ie_len = -1;
		break;
	}

	return meas_ie_len;
}

u_int8_t *ieee80211_measure_request_ie_generate(struct ieee80211_node *ni,
		u_int8_t *frm,
		struct ieee80211_meas_request_ctrl *mrequest_ctrl)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	u_int8_t *ele_len;


	*frm++ = IEEE80211_ELEMID_MEASREQ;
	ele_len = frm;
	*frm++ = 0;	/* will be filled when finished */
	*frm++ = 1;	/* measurement token */
	*frm++ = 0;	/* mode */
	*frm++ = mrequest_ctrl->meas_type;

	switch (mrequest_ctrl->meas_type) {
	case IEEE80211_CCA_MEASTYPE_BASIC:
	{
		*frm++ = mrequest_ctrl->u.basic.channel;
		ADDINT32(frm, *((u_int32_t *)&mrequest_ctrl->u.basic.start_tsf));
		ADDINT32(frm, *((u_int32_t *)&mrequest_ctrl->u.basic.start_tsf + 1));
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.basic.duration_ms));

		break;
	}
	case IEEE80211_CCA_MEASTYPE_CCA:
	{
		*frm++ = mrequest_ctrl->u.cca.channel;
		ADDINT32(frm, *((u_int32_t *)&mrequest_ctrl->u.cca.start_tsf));
		ADDINT32(frm, *((u_int32_t *)&mrequest_ctrl->u.cca.start_tsf + 1));
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.cca.duration_ms));

		break;
	}
	case IEEE80211_CCA_MEASTYPE_RPI:
	{
		*frm++ = mrequest_ctrl->u.rpi.channel;
		ADDINT32(frm, *((u_int32_t *)&mrequest_ctrl->u.rpi.start_tsf));
		ADDINT32(frm, *((u_int32_t *)&mrequest_ctrl->u.cca.start_tsf + 1));
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.rpi.duration_ms));

		break;
	}
	case IEEE80211_RM_MEASTYPE_STA:
	{
		u_int16_t random_interval;
		ieee80211_11k_sub_element *p_se;
		ieee80211_11k_sub_element_head *se_head;

		memcpy(frm, ni->ni_macaddr, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
		get_random_bytes(&random_interval, 1);
		ADDINT16(frm, random_interval);
		ADDINT16(frm, mrequest_ctrl->u.sta_stats.duration_tu);
		*frm++ = mrequest_ctrl->u.sta_stats.group_id;

		/* optional sub element */
		if (mrequest_ctrl->u.sta_stats.sub_item != NULL) {
			se_head = (ieee80211_11k_sub_element_head *)mrequest_ctrl->u.sta_stats.sub_item;
			while (!SLIST_EMPTY(se_head)) {
				p_se = SLIST_FIRST(se_head);
				switch (p_se->sub_id) {
				case IEEE80211_ELEMID_VENDOR:
				{
					struct stastats_subele_vendor *vendor = (struct stastats_subele_vendor *)p_se->data;
					u_int8_t *vendor_ie_len;

					if (vendor->flags & RM_QTN_MEASURE_MASK) {
						*frm++ = IEEE80211_ELEMID_VENDOR;
						vendor_ie_len = frm;
						*frm++ = 0;
						frm += ieee80211_oui_add_qtn(frm);
						*frm++ = ni->ni_rm_sta_seq++;

						if (IEEE80211_IS_ALL_SET(vendor->flags, RM_QTN_MAX)) {
							*frm++ = QTN_OUI_RM_ALL;
						} else {
							u_int8_t cnt, *p_tlv_cnt;

							*frm++ =  QTN_OUI_RM_SPCIAL;
							p_tlv_cnt = frm;
							*frm++ = 0;

							for (cnt = RM_QTN_TX_STATS; cnt <= RM_QTN_MAX; cnt++) {
								if (vendor->flags & (BIT(cnt))) {
									*frm++ = cnt;
									*frm++ = 0;
									*p_tlv_cnt += 1;
								}
							}

							for (cnt = RM_QTN_CTRL_START; cnt <= RM_QTN_CTRL_END; cnt++) {
								if (vendor->flags & (BIT(cnt))) {
									*frm++ = cnt;
									*frm++ = 0;
									*p_tlv_cnt += 1;
								}
							}
						}
						*vendor_ie_len = frm - vendor_ie_len - 1;
					}
					break;
				}
				default:
					break;
				}
				SLIST_REMOVE_HEAD(se_head, next);
				kfree(p_se);
			}
		}

		break;
	}
	case IEEE80211_RM_MEASTYPE_QTN_CCA:
	{
		/* replace with real type */
		*(frm - 1) = IEEE80211_CCA_MEASTYPE_CCA;

		*frm++ = ic->ic_curchan->ic_ieee;
		ADDINT32(frm, 0);
		ADDINT32(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.qtn_cca.duration_tu));
		break;
	}
	case IEEE80211_RM_MEASTYPE_CH_LOAD:
	{
		*frm++ = 0;	/* TODO: operating class, figure out a correct mapping */
		*frm++ = mrequest_ctrl->u.chan_load.channel;
		ADDINT16(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.chan_load.duration_ms));

		break;
	}
	case IEEE80211_RM_MEASTYPE_NOISE:
	{
		*frm++ = 0;	/* TODO: operating class, figure out a correct mapping */
		*frm++ = mrequest_ctrl->u.noise_his.channel;
		ADDINT16(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.noise_his.duration_ms));

		break;
	}
	case IEEE80211_RM_MEASTYPE_BEACON:
	{
		*frm++ = mrequest_ctrl->u.beacon.op_class;
		*frm++ = mrequest_ctrl->u.beacon.channel;
		ADDINT16(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.beacon.duration_ms));
		*frm++ = mrequest_ctrl->u.beacon.mode;
		memcpy(frm, mrequest_ctrl->u.beacon.bssid, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;

		break;
	}
	case IEEE80211_RM_MEASTYPE_FRAME:
	{
		*frm++ = mrequest_ctrl->u.frame.op_class;
		*frm++ = mrequest_ctrl->u.frame.channel;
		ADDINT16(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.frame.duration_ms));
		*frm++ = mrequest_ctrl->u.frame.type;
		memcpy(frm, mrequest_ctrl->u.frame.mac_address, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;

		break;
	}
	case IEEE80211_RM_MEASTYPE_CATEGORY:
	{
		ADDINT16(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.tran_stream_cat.duration_ms));
		memcpy(frm, mrequest_ctrl->u.tran_stream_cat.peer_sta, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
		*frm++ = mrequest_ctrl->u.tran_stream_cat.tid;
		*frm++ = mrequest_ctrl->u.tran_stream_cat.bin0;

		break;
	}
	case IEEE80211_RM_MEASTYPE_MUL_DIAG:
		ADDINT16(frm, 0);
		ADDINT16(frm, IEEE80211_MS_TO_TU(mrequest_ctrl->u.multicast_diag.duration_ms));
		memcpy(frm, mrequest_ctrl->u.multicast_diag.group_mac, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;

		break;
	default:
		break;
	}
	*ele_len = (frm - ele_len) - 1;

	return frm;
}

u_int8_t *ieee80211_measure_report_ie_generate(struct ieee80211_node *ni,
		u_int8_t *frm,
		struct ieee80211_meas_report_ctrl *mreport_ctrl)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	u_int8_t *ele_len;

	/* common part */
	*frm++ = IEEE80211_ELEMID_MEASREP;
	ele_len = frm;
	*frm++ = 0;
	if (mreport_ctrl->autonomous)
		*frm++ = 0;
	else
		*frm++ = mreport_ctrl->meas_token;
	*frm++ = mreport_ctrl->report_mode;
	*frm++ = mreport_ctrl->meas_type;

	if (mreport_ctrl->report_mode == 0) {
		switch (mreport_ctrl->meas_type) {
		case IEEE80211_CCA_MEASTYPE_BASIC:
		{
			*frm++ = mreport_ctrl->u.basic.channel;
			ADDINT32(frm, *((u_int32_t *)&mreport_ctrl->u.basic.start_tsf));
			ADDINT32(frm, *((u_int32_t *)&mreport_ctrl->u.basic.start_tsf) + 1);
			ADDINT16(frm, mreport_ctrl->u.basic.duration_tu);
			*frm++ = mreport_ctrl->u.basic.basic_report;

			break;
		}
		case IEEE80211_CCA_MEASTYPE_CCA:
		{
			*frm++ = mreport_ctrl->u.cca.channel;
			ADDINT32(frm, *((u_int32_t *)&mreport_ctrl->u.cca.start_tsf));
			ADDINT32(frm, *((u_int32_t *)&mreport_ctrl->u.cca.start_tsf) + 1);
			ADDINT16(frm, mreport_ctrl->u.cca.duration_tu);
			*frm++ = mreport_ctrl->u.cca.cca_report;

			break;
		}
		case IEEE80211_CCA_MEASTYPE_RPI:
		{
			*frm++ = mreport_ctrl->u.rpi.channel;
			ADDINT32(frm, *((u_int32_t *)&mreport_ctrl->u.rpi.start_tsf));
			ADDINT32(frm, *((u_int32_t *)&mreport_ctrl->u.rpi.start_tsf) + 1);
			ADDINT16(frm, mreport_ctrl->u.rpi.duration_tu);
			memcpy(frm, mreport_ctrl->u.rpi.rpi_report, sizeof(mreport_ctrl->u.rpi.rpi_report));
			frm += sizeof(mreport_ctrl->u.rpi.rpi_report);

			break;
		}
		case IEEE80211_RM_MEASTYPE_STA:
		{
			u_int8_t group_len;
			ieee80211_11k_sub_element *p_se;
			ieee80211_11k_sub_element_head *se_head;
			struct ieee80211_nodestats *stats;
			u_int8_t assoc_bw;

			ADDINT16(frm, mreport_ctrl->u.sta_stats.duration_tu);
			*frm++ = mreport_ctrl->u.sta_stats.group_id;

			switch (mreport_ctrl->u.sta_stats.group_id) {
			case 0:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group0);
				break;
			case 1:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group0);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group2to9);
				break;
			case 10:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group10);
				break;
			case 11:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group11);
				break;
			case 12:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group12);
				break;
			case 13:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group13);
				break;
			case 14:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group14);
				break;
			case 15:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group15);
				break;
			case 16:
				group_len = sizeof(struct ieee80211_rm_sta_stats_group16);
				break;
			default:
				group_len = sizeof(struct ieee80211_ie_qtn_rm_sta_all);
				break;
			}
			frm += group_len;

			if (mreport_ctrl->u.sta_stats.sub_item != NULL) {
				se_head = (ieee80211_11k_sub_element_head *)mreport_ctrl->u.sta_stats.sub_item;
				while (!SLIST_EMPTY(se_head)) {
					p_se = SLIST_FIRST(se_head);
					switch (p_se->sub_id) {
					case IEEE80211_ELEMID_VENDOR:
					{
						struct stastats_subele_vendor *vendor = (struct stastats_subele_vendor *)p_se->data;
						u_int32_t vendor_flags = vendor->flags;
						u_int8_t sequence = vendor->sequence;

						if (vendor_flags & RM_QTN_MEASURE_MASK) {
							u_int8_t *vendor_ie_len;

							*frm++ = IEEE80211_ELEMID_VENDOR;
							vendor_ie_len = frm;
							*frm++ = 0;
							frm += ieee80211_oui_add_qtn(frm);
							*frm++ = sequence;

							stats = &ni->ni_stats;

							if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT)) {
								 switch (ni->ni_vhtcap.chanwidth) {
									case IEEE80211_VHTCAP_CW_160M:
									case IEEE80211_VHTCAP_CW_160_AND_80P80M:
										assoc_bw = 160;
										break;
									case IEEE80211_VHTCAP_CW_80M_ONLY:
									default:
										assoc_bw = 80;
								}
							} else {
								if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40 &&
									ni->ni_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) {
									assoc_bw = 40;
								} else {
									assoc_bw = 20;
								}
							}

							ic->ic_iterate_nodes(&ic->ic_sta, get_node_info, (void *)NULL, 1);

							if (IEEE80211_IS_ALL_SET(vendor_flags, RM_QTN_MAX)) {
								*frm++ = QTN_OUI_RM_ALL;

								/* fill the content here */
								/* fill the sta tx statistics */
								ADDINT32TO64(frm, stats->ns_tx_bytes);
								ADDINT32(frm, stats->ns_tx_data);
								ADDINT32(frm, stats->ns_tx_dropped);
								ADDINT32(frm, stats->ns_tx_errors);
								ADDINT32(frm, stats->ns_tx_ucast);
								ADDINT32(frm, stats->ns_tx_mcast);
								ADDINT32(frm, stats->ns_tx_bcast);

								/* fill the sta rx statistics */
								ADDINT32TO64(frm, stats->ns_rx_bytes);
								ADDINT32(frm, stats->ns_rx_data);
								ADDINT32(frm, stats->ns_rx_dropped);
								ADDINT32(frm, stats->ns_rx_errors);
								ADDINT32(frm, stats->ns_rx_ucast);
								ADDINT32(frm, stats->ns_rx_mcast);
								ADDINT32(frm, stats->ns_rx_bcast);

								/* fill the sta parameters */
								ADDINT32(frm, ni->ni_max_queue);
								ADDINT16TO32(frm, ni->ni_linkqual);
								ADDINT32(frm, ni->ni_smthd_rssi);
								ADDINT8TO32(frm, assoc_bw);
								ADDINT32(frm, ni->ni_snr);
								ADDINT8TO32(frm, ni->ni_rates.rs_rates[ni->ni_txrate]);
								ADDINT16TO32(frm, ni->ni_rx_phy_rate);
							} else {
								u_int8_t *p_tlv_cnt;
								u_int8_t i;

								*frm++ = QTN_OUI_RM_SPCIAL;
								p_tlv_cnt = frm;
								*frm++ = 0;

								for (i = 0; i <= RM_QTN_MAX; i++) {
									if (vendor_flags & (BIT(i))) {
										*p_tlv_cnt += 1;
										*frm++ = i;
										switch (i) {
										case RM_QTN_TX_STATS:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32TO64(frm, stats->ns_tx_bytes);
											ADDINT32(frm, stats->ns_tx_data);
											ADDINT32(frm, stats->ns_tx_dropped);
											ADDINT32(frm, stats->ns_tx_errors);
											ADDINT32(frm, stats->ns_tx_ucast);
											ADDINT32(frm, stats->ns_tx_mcast);
											ADDINT32(frm, stats->ns_tx_bcast);
											break;
										}
										case RM_QTN_RX_STATS:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32TO64(frm, stats->ns_rx_bytes);
											ADDINT32(frm, stats->ns_rx_data);
											ADDINT32(frm, stats->ns_rx_dropped);
											ADDINT32(frm, stats->ns_rx_errors);
											ADDINT32(frm, stats->ns_rx_ucast);
											ADDINT32(frm, stats->ns_rx_mcast);
											ADDINT32(frm, stats->ns_rx_bcast);
											break;
										}
										case RM_QTN_MAX_QUEUED:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, ni->ni_max_queue);
											break;
										}
										case RM_QTN_LINK_QUALITY:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, ni->ni_linkqual);
											break;
										}
										case RM_QTN_RSSI_DBM:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, ni->ni_smthd_rssi);
											break;
										}
										case RM_QTN_BANDWIDTH:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, assoc_bw);
											break;
										}
										case RM_QTN_SNR:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, ni->ni_snr);
											break;
										}
										case RM_QTN_TX_PHY_RATE:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, ni->ni_linkqual);
											break;
										}
										case RM_QTN_RX_PHY_RATE:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											ADDINT32(frm, ni->ni_rx_phy_rate);
											break;
										}
										case RM_QTN_CCA:
										{
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											/* Reserved for cca */
											ADDINT32(frm, 0);
											break;
										}
										case RM_QTN_BR_IP:
										{
											/* Vendor specific content: tlv length field */
											__be32 br_ip = 0;
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											if (ic->ic_getparam != NULL) {
												(*ic->ic_getparam)(ni, IEEE80211_PARAM_BR_IP_ADDR,
														(int *)&br_ip, NULL, NULL);
											}
											ADDINT32(frm, br_ip);
											break;
										}
										case RM_QTN_RSSI:
										{
											int32_t local_rssi = 0;
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];

											if (ic->ic_rssi) {
												local_rssi = ic->ic_rssi(ni);
											}


											if (local_rssi < -1 && local_rssi > -1200) {
												local_rssi += 900;
											}

											if (local_rssi < 0) {
												local_rssi = 0;
											}

											ADDINT32(frm, local_rssi);
											break;
										}
										case RM_QTN_HW_NOISE:
										{
											int32_t local_noise = 0;
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];

											local_noise = ic->ic_hw_noise(ni);

											ADDINT32(frm, local_noise);

											break;
										}
										case RM_QTN_SOC_MACADDR:
										{
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];

											memcpy(frm, ic->soc_addr, IEEE80211_ADDR_LEN);
											frm += IEEE80211_ADDR_LEN;

											break;
										}
										case RM_QTN_SOC_IPADDR:
										{
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];

											ADDINT32(frm, ic->ic_soc_ipaddr);

											break;
										}
										default:
											/* Vendor specific content: tlv length field */
											*frm++ = sizeof(u_int32_t);
											/* unkown type report 0 */
											ADDINT32(frm, 0);
											break;
										}
									}
								}

								for (i = RM_QTN_CTRL_START; i <= RM_QTN_CTRL_END; i++) {
									if (vendor_flags & (BIT(i))) {
										*frm++ = i;
										switch(i) {
										case RM_QTN_RESET_CNTS:
										{
											int32_t		ret;
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											/* reset all counter */
											ret = ieee80211_rst_dev_stats(vap);
											ADDINT32(frm, ret);
											break;
										}
										case RM_QTN_RESET_QUEUED:
											/* Vendor specific content: tlv length field */
											*frm++ = ieee80211_meas_sta_qtn_report_subtype_len[i];
											/* reset all counter */
											ic->ic_queue_reset(ni);
											ADDINT32(frm, 0);
											break;
										default:
											/* Vendor specific content: tlv length field */
											*frm++ = sizeof(int32_t);
											ADDINT32(frm, -1);
											break;
										}
									}
								}
							}
							*vendor_ie_len = frm - vendor_ie_len - 1;
						}
						break;
					}
					default:
						printk("unknown STA Statistics sub element, ID = %d\n", p_se->sub_id);
						break;
					}
					SLIST_REMOVE_HEAD(se_head, next);
					kfree(p_se);
				}
			}

			break;
		}
		case IEEE80211_RM_MEASTYPE_QTN_CCA:
		{
			u_int8_t *vendor_ie_len;

			/* replace with real type */
			*(frm - 1) = IEEE80211_CCA_MEASTYPE_CCA;

			*frm++ = mreport_ctrl->u.qtn_cca.channel;
			ADDINT32LE(frm, *((u_int32_t *)&mreport_ctrl->u.qtn_cca.start_tsf));
			ADDINT32LE(frm, *((u_int32_t *)&mreport_ctrl->u.qtn_cca.start_tsf) + 1);
			ADDINT16LE(frm, IEEE80211_MS_TO_TU(mreport_ctrl->u.qtn_cca.duration_ms));
			*frm++ = mreport_ctrl->u.qtn_cca.qtn_cca_report;

			/* qtn SCS IE */
			*frm++ = IEEE80211_ELEMID_VENDOR;
			vendor_ie_len = frm;
			*frm++ = 0;
			frm += ieee80211_oui_add_qtn(frm);
			*frm++ = QTN_OUI_SCS;
			*frm++ = QTN_SCS_IE_TYPE_STA_INTF_RPT;
			ADDINT32LE(frm, mreport_ctrl->u.qtn_cca.sp_fail);
			ADDINT32LE(frm, mreport_ctrl->u.qtn_cca.lp_fail);
			ADDINT16LE(frm, mreport_ctrl->u.qtn_cca.others_time);
			ADDINT16LE(frm, mreport_ctrl->u.qtn_cca.extra_ie_len);
			memcpy(frm, mreport_ctrl->u.qtn_cca.extra_ie,
						mreport_ctrl->u.qtn_cca.extra_ie_len);
			frm += mreport_ctrl->u.qtn_cca.extra_ie_len;
			*vendor_ie_len = frm - vendor_ie_len - 1;

			break;
		}
		case IEEE80211_RM_MEASTYPE_CH_LOAD:
		{
			u_int64_t tsf;

			ic->ic_get_tsf(&tsf);

			*frm++ = mreport_ctrl->u.chan_load.op_class;
			*frm++ = mreport_ctrl->u.chan_load.channel;
			ADDINT32(frm, *((u_int32_t *)&tsf));
			ADDINT32(frm, *((u_int32_t *)&tsf + 1));
			ADDINT16(frm, mreport_ctrl->u.chan_load.duration_tu);
			*frm++ = mreport_ctrl->u.chan_load.channel_load;

			break;
		}
		case IEEE80211_RM_MEASTYPE_NOISE:
		{
			u_int64_t tsf;

			ic->ic_get_tsf(&tsf);

			*frm++ = mreport_ctrl->u.noise_his.op_class;
			*frm++ = mreport_ctrl->u.noise_his.channel;
			ADDINT32(frm, *((u_int32_t *)&tsf));
			ADDINT32(frm, *((u_int32_t *)&tsf + 1));
			ADDINT16(frm, mreport_ctrl->u.noise_his.duration_tu);
			*frm++ = mreport_ctrl->u.noise_his.antenna_id;
			*frm++ = mreport_ctrl->u.noise_his.anpi;
			memcpy(frm, mreport_ctrl->u.noise_his.ipi, sizeof(mreport_ctrl->u.noise_his.ipi));
			frm += sizeof(mreport_ctrl->u.noise_his.ipi);

			break;
		}
		case IEEE80211_RM_MEASTYPE_BEACON:
		{
			u_int64_t tsf;

			ic->ic_get_tsf(&tsf);

			*frm++ = mreport_ctrl->u.beacon.op_class;
			*frm++ = mreport_ctrl->u.beacon.channel;
			ADDINT32(frm, *((u_int32_t *)&tsf));
			ADDINT32(frm, *((u_int32_t *)&tsf + 1));
			ADDINT16(frm, mreport_ctrl->u.beacon.duration_tu);
			*frm++ = mreport_ctrl->u.beacon.reported_frame_info;
			*frm++ = mreport_ctrl->u.beacon.rcpi;
			*frm++ = mreport_ctrl->u.beacon.rsni;
			memcpy(frm, mreport_ctrl->u.beacon.bssid, IEEE80211_ADDR_LEN);
			frm += IEEE80211_ADDR_LEN;
			*frm++ = mreport_ctrl->u.beacon.antenna_id;
			memcpy(frm, mreport_ctrl->u.beacon.parent_tsf, 4);
			frm += sizeof(mreport_ctrl->u.beacon.parent_tsf);

			break;
		}
		case IEEE80211_RM_MEASTYPE_FRAME:
		{
			u_int64_t tsf;
			ieee80211_11k_sub_element *p_se;
			ieee80211_11k_sub_element_head *se_head;

			ic->ic_get_tsf(&tsf);

			*frm++ = mreport_ctrl->u.frame.op_class;
			*frm++ = mreport_ctrl->u.frame.channel;
			ADDINT32(frm, *((u_int32_t *)&tsf));
			ADDINT32(frm, *((u_int32_t *)&tsf + 1));
			ADDINT16(frm, mreport_ctrl->u.frame.duration_tu);

			se_head = (ieee80211_11k_sub_element_head *)mreport_ctrl->u.frame.sub_item;
			while (!SLIST_EMPTY(se_head)) {
				p_se = SLIST_FIRST(se_head);
				switch (p_se->sub_id) {
				case IEEE80211_FRAME_REPORT_SUBELE_FRAME_COUNT_REPORT:
				{
					u_int8_t *sub_ele_len;
					struct frame_report_subele_frame_count *sub_ele;

					sub_ele = (struct frame_report_subele_frame_count *)p_se->data;
					*frm++ = p_se->sub_id;
					sub_ele_len = frm;
					*frm++ = 0;
					memcpy(frm, sub_ele->ta, IEEE80211_ADDR_LEN);
					frm += IEEE80211_ADDR_LEN;
					memcpy(frm, sub_ele->bssid, IEEE80211_ADDR_LEN);
					frm += IEEE80211_ADDR_LEN;
					*frm++ = sub_ele->phy_type;
					*frm++ = sub_ele->avg_rcpi;
					*frm++ = sub_ele->last_rsni;
					*frm++ = sub_ele->last_rcpi;
					*frm++ = sub_ele->antenna_id;
					ADDINT16(frm, sub_ele->frame_count);

					*sub_ele_len = frm - sub_ele_len - 1;

					break;
				}
				default:
					break;
				}
				SLIST_REMOVE_HEAD(se_head, next);
				kfree(p_se);
			}

			break;
		}
		case IEEE80211_RM_MEASTYPE_CATEGORY:
		{
			u_int64_t tsf;

			ic->ic_get_tsf(&tsf);

			ADDINT32(frm, *((u_int32_t *)&tsf));
			ADDINT32(frm, *((u_int32_t *)&tsf + 1));
			ADDINT16(frm, mreport_ctrl->u.tran_stream_cat.duration_tu);
			memcpy(frm, mreport_ctrl->u.tran_stream_cat.peer_sta, IEEE80211_ADDR_LEN);
			frm += IEEE80211_ADDR_LEN;
			*frm++ = mreport_ctrl->u.tran_stream_cat.tid;
			*frm++ = mreport_ctrl->u.tran_stream_cat.reason;
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.tran_msdu_cnt);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.msdu_discard_cnt);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.msdu_fail_cnt);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.msdu_mul_retry_cnt);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.qos_lost_cnt);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.avg_queue_delay);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.avg_tran_delay);
			*frm++ = mreport_ctrl->u.tran_stream_cat.bin0_range;
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.bins[0]);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.bins[1]);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.bins[2]);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.bins[3]);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.bins[4]);
			ADDINT32(frm, mreport_ctrl->u.tran_stream_cat.bins[5]);

			break;
		}
		case IEEE80211_RM_MEASTYPE_MUL_DIAG:
			ADDINT32(frm, 0);
			ADDINT32(frm, 0);
			ADDINT16(frm, mreport_ctrl->u.multicast_diag.duration_tu);
			memcpy(frm, mreport_ctrl->u.multicast_diag.group_mac, IEEE80211_ADDR_LEN);
			frm += IEEE80211_ADDR_LEN;
			*frm++ = mreport_ctrl->u.multicast_diag.reason;
			ADDINT32(frm, mreport_ctrl->u.multicast_diag.mul_rec_msdu_cnt);
			ADDINT16(frm, mreport_ctrl->u.multicast_diag.first_seq_num);
			ADDINT16(frm, mreport_ctrl->u.multicast_diag.last_seq_num);
			ADDINT16(frm, mreport_ctrl->u.multicast_diag.mul_rate);

			break;
		default:
			break;
		}
	}
	*ele_len = frm - ele_len - 1;

	return frm;
}

int32_t ieee80211_compile_action_measurement_11h(struct ieee80211_node *ni,
		void *ctrl,
		u_int8_t action,
		struct sk_buff **p_skb)
{
	struct sk_buff *skb;
	u_int8_t *frm;
	int32_t meas_frame_len = 0;
	int32_t meas_ie_len = 0;
	struct ieee80211_meas_request_ctrl *mrequest_ctrl = NULL;
	struct ieee80211_meas_report_ctrl *mreport_ctrl = NULL;
	u_int8_t tx_token;

	if ((action != IEEE80211_ACTION_S_MEASUREMENT_REQUEST) && (action != IEEE80211_ACTION_S_MEASUREMENT_REPORT))
		return -1;

	if (action == IEEE80211_ACTION_S_MEASUREMENT_REQUEST) {
		mrequest_ctrl = (struct ieee80211_meas_request_ctrl *)ctrl;
		meas_frame_len = sizeof(struct ieee80211_action_sm_measurement_header);

		meas_ie_len = ieee80211_measure_request_ie_len(mrequest_ctrl);
		if (meas_ie_len <= 0)
			return -1;
		meas_frame_len += meas_ie_len;

		skb = ieee80211_getmgtframe(&frm, meas_frame_len);
		if (NULL == skb)
			return -1;

		*frm++ = IEEE80211_ACTION_CAT_SPEC_MGMT;
		*frm++ = action;
		if (ni->ni_action_token == 0)
			ni->ni_action_token++;
		tx_token = ni->ni_action_token++;
		*frm++ = tx_token;
		frm = ieee80211_measure_request_ie_generate(ni, frm, mrequest_ctrl);
		if (mrequest_ctrl->expire != 0) {
			skb = ieee80211_ppqueue_pre_tx(ni,
					skb,
					IEEE80211_ACTION_CAT_SPEC_MGMT,
					IEEE80211_ACTION_S_MEASUREMENT_REPORT,
					tx_token,
					mrequest_ctrl->expire,
					mrequest_ctrl->fn_success,
					mrequest_ctrl->fn_fail);
			if (skb == NULL)
				return -1;
		}
	} else {
		mreport_ctrl = (struct ieee80211_meas_report_ctrl *)ctrl;
		meas_frame_len = sizeof(struct ieee80211_action_sm_measurement_header);

		meas_ie_len = ieee80211_measure_report_ie_len(mreport_ctrl);
		if (meas_ie_len <= 0)
			return -1;
		meas_frame_len += meas_ie_len;

		skb = ieee80211_getmgtframe(&frm, meas_frame_len);
		if (NULL == skb)
			return -1;
		memset(frm, 0, meas_frame_len);

		*frm++ = IEEE80211_ACTION_CAT_SPEC_MGMT;
		*frm++ = action;
		if (mreport_ctrl->autonomous)
			*frm++ = 0;
		else
			*frm++ = mreport_ctrl->token;
		frm = ieee80211_measure_report_ie_generate(ni, frm, mreport_ctrl);
	}

	KASSERT(((frm - skb->data) <= meas_frame_len),
			("ERROR: 11h measure frame gen fail\n"
			"expected len = %d\n"
			"start address(0x%x), end address(0x%x), len = %d\n",
			meas_frame_len,
			(uint32_t)skb->data,
			(uint32_t)frm,
			(uint32_t)(frm - skb->data)));

	skb_trim(skb, frm - skb->data);
	*p_skb = skb;
	return 0;
}

int32_t ieee80211_compile_action_measurement_11k(struct ieee80211_node *ni,
		void *ctrl,
		u_int8_t action,
		struct sk_buff **p_skb)
{
	struct sk_buff *skb;
	u_int8_t *frm;
	int32_t meas_frame_len = 0;
	int32_t meas_ie_len = 0;
	struct ieee80211_meas_request_ctrl *mrequest_ctrl = NULL;
	struct ieee80211_meas_report_ctrl *mreport_ctrl = NULL;
	u_int8_t tx_token;

	if ((action != IEEE80211_ACTION_R_MEASUREMENT_REQUEST) && (action != IEEE80211_ACTION_R_MEASUREMENT_REPORT))
		return -1;

	if (action == IEEE80211_ACTION_R_MEASUREMENT_REQUEST) {
		mrequest_ctrl = (struct ieee80211_meas_request_ctrl *)ctrl;
		meas_frame_len = sizeof(struct ieee80211_action_radio_measure_request);

		meas_ie_len = ieee80211_measure_request_ie_len(mrequest_ctrl);
		if (meas_ie_len <= 0)
			return -1;
		meas_frame_len += meas_ie_len;

		skb = ieee80211_getmgtframe(&frm, meas_frame_len);
		if (NULL == skb)
			return -1;

		*frm++ = IEEE80211_ACTION_CAT_RM;
		*frm++ = action;
		if (ni->ni_action_token == 0)
			ni->ni_action_token++;
		tx_token = ni->ni_action_token++;
		*frm++ = tx_token;
		ADDINT16(frm, 0);	/* set number of repetitions to 0 */
		frm = ieee80211_measure_request_ie_generate(ni, frm, mrequest_ctrl);
		if (mrequest_ctrl->expire != 0) {
			skb = ieee80211_ppqueue_pre_tx(ni,
					skb,
					IEEE80211_ACTION_CAT_RM,
					IEEE80211_ACTION_R_MEASUREMENT_REPORT,
					tx_token,
					mrequest_ctrl->expire,
					mrequest_ctrl->fn_success,
					mrequest_ctrl->fn_fail);
			if (skb == NULL)
				return -1;
		}
	} else {
		mreport_ctrl = (struct ieee80211_meas_report_ctrl *)ctrl;
		meas_frame_len = sizeof(struct ieee80211_action_radio_measure_report);

		meas_ie_len = ieee80211_measure_report_ie_len(mreport_ctrl);
		if (meas_ie_len <= 0)
			return -1;
		meas_frame_len += meas_ie_len;

		skb = ieee80211_getmgtframe(&frm, meas_frame_len);
		if (NULL == skb)
			return -1;

		*frm++ = IEEE80211_ACTION_CAT_RM;
		*frm++ = action;
		if (mreport_ctrl->autonomous)
			*frm++ = 0;
		else
			*frm++ = mreport_ctrl->token;
		frm = ieee80211_measure_report_ie_generate(ni, frm, mreport_ctrl);
	}

	KASSERT(((frm - skb->data) <= meas_frame_len),
			("ERROR: 11k measure frame gen fail\n"
			"expected len = %d\n"
			"start address(0x%x), end address(0x%x), len = %d\n",
			meas_frame_len,
			(uint32_t)skb->data,
			(uint32_t)frm,
			(uint32_t)(frm - skb->data)));

	skb_trim(skb, frm - skb->data);
	*p_skb = skb;
	return 0;
}

#if defined(CONFIG_QTN_80211K_SUPPORT)
void ieee80211_send_action_cca_report(struct ieee80211_node *ni, uint8_t token,
		uint16_t cca_intf, uint64_t tsf, uint16_t duration, uint32_t sp_fail,
		uint32_t lp_fail, uint16_t others_time, uint8_t *extra_ie, uint16_t ie_len)
{
	struct ieee80211_meas_report_ctrl mreport_ctrl;
	struct ieee80211_action_data action_data;
	u_int16_t frac_busy;

	frac_busy = cca_intf * IEEE80211_11K_CCA_INTF_SCALE / IEEE80211_SCS_CCA_INTF_SCALE;
	mreport_ctrl.meas_type = IEEE80211_RM_MEASTYPE_QTN_CCA;
	mreport_ctrl.report_mode = 0;
	mreport_ctrl.autonomous = 1;
	mreport_ctrl.u.qtn_cca.channel = ni->ni_chan->ic_ieee;
	mreport_ctrl.u.qtn_cca.start_tsf = tsf;
	mreport_ctrl.u.qtn_cca.duration_ms = duration;
	mreport_ctrl.u.qtn_cca.qtn_cca_report = (u_int8_t)frac_busy;
	mreport_ctrl.u.qtn_cca.sp_fail = sp_fail;
	mreport_ctrl.u.qtn_cca.lp_fail = lp_fail;
	mreport_ctrl.u.qtn_cca.others_time = others_time;
	mreport_ctrl.u.qtn_cca.extra_ie = extra_ie;
	mreport_ctrl.u.qtn_cca.extra_ie_len = ie_len;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &mreport_ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
#endif

__inline void ieee80211_ppqueue_release_entry(struct ieee80211_pairing_pending_entry *entry)
{
	if (entry != NULL) {
		dev_kfree_skb_any(entry->skb);
		kfree(entry);
	}
}

void ieee80211_ppqueue_insert_entry(struct ieee80211_pairing_pending_queue *queue,
				struct ieee80211_pairing_pending_entry *entry)
{
	int32_t timer_refresh = 0;
	unsigned long flags;
	struct ieee80211_pairing_pending_entry *prev, *cur;

	spin_lock_irqsave(&queue->lock, flags);
	if (queue->next == NULL) {
		queue->next = entry;
		queue->next_expire_jiffies = entry->next_expire_jiffies;
		timer_refresh = 1;
	} else {
		if (time_before(entry->next_expire_jiffies, queue->next->next_expire_jiffies)) {
			entry->next = queue->next;
			queue->next = entry;
			queue->next_expire_jiffies = entry->next_expire_jiffies;
			timer_refresh = 1;
		} else {
			prev = queue->next;
			cur = prev->next;
			while (cur != NULL) {
				if (time_before(entry->next_expire_jiffies, cur->next_expire_jiffies)) {
					entry->next = cur;
					prev->next = entry;
					break;
				}
				prev = cur;
				cur = prev->next;
			}
		}
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	if (timer_refresh) {
		mod_timer(&queue->timer, queue->next_expire_jiffies);
	}
}

void ieee80211_ppqueue_remove_with_response(struct ieee80211_pairing_pending_queue *queue,
					struct ieee80211_node *ni,
					u_int8_t category,
					u_int8_t action,
					u_int8_t token)
{
	struct ieee80211_pairing_pending_entry **prev, *cur, *to_free;
	unsigned long flags;

	prev = &queue->next;
	cur = queue->next;
	to_free = NULL;

	spin_lock_irqsave(&queue->lock, flags);
	while (cur != NULL) {
		if ((cur->ni == ni) &&
			(cur->expected_category == category) &&
			(cur->expected_action == action) &&
			(cur->expected_token == token)) {
			to_free = cur;
			*prev = cur->next;
			break;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	if (to_free != NULL) {
		if (to_free->fn_success != NULL)
			to_free->fn_success(to_free->ni);

		ieee80211_ppqueue_release_entry(to_free);
	}
}

void ieee80211_ppqueue_remove_node_leave(struct ieee80211_pairing_pending_queue *queue,
				struct ieee80211_node *ni)
{
	struct ieee80211_pairing_pending_entry **prev, *cur;
	struct ieee80211_pairing_pending_entry *ni_drop_list, *to_free;
	unsigned long flags;

	prev = &queue->next;
	cur = queue->next;
	ni_drop_list = NULL;
	to_free = NULL;

	spin_lock_irqsave(&queue->lock, flags);
	while (cur != NULL) {
		if (cur->ni == ni) {
			to_free = cur;
			*prev = cur->next;
			cur = cur->next;
			REPLACE_PPQ_ENTRY_HEAD(ni_drop_list, to_free);
			continue;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	while (ni_drop_list != NULL) {
		to_free = ni_drop_list;
		ni_drop_list = ni_drop_list->next;

		if (to_free->fn_fail)
			to_free->fn_fail(to_free->ni, PPQ_FAIL_NODELEAVE);

		ieee80211_ppqueue_release_entry(to_free);
	}
}

void ieee80211_ppqueue_remove_with_cat_action(struct ieee80211_pairing_pending_queue *queue,
				u_int8_t category,
				u_int8_t action)
{
	struct ieee80211_pairing_pending_entry **prev, *cur;
	struct ieee80211_pairing_pending_entry *ni_drop_list, *to_free;
	unsigned long flags;

	prev = &queue->next;
	cur = queue->next;
	ni_drop_list = NULL;
	to_free = NULL;

	spin_lock_irqsave(&queue->lock, flags);
	while (cur != NULL) {
		if (cur->expected_category == category &&
			cur->expected_action == action) {
			to_free = cur;
			*prev = cur->next;
			cur = cur->next;
			REPLACE_PPQ_ENTRY_HEAD(ni_drop_list, to_free);
			continue;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	while (ni_drop_list != NULL) {
		to_free = ni_drop_list;
		ni_drop_list = ni_drop_list->next;

		if (to_free->fn_fail)
			to_free->fn_fail(to_free->ni, PPQ_FAIL_STOP);

		ieee80211_ppqueue_release_entry(to_free);
	}
}

void ieee80211_ppqueue_flush(struct ieee80211_pairing_pending_queue *queue)
{
	struct ieee80211_pairing_pending_entry *flush_list, *to_free;
	unsigned long flags;

	spin_lock_irqsave(&queue->lock, flags);
	flush_list = queue->next;
	queue->next = NULL;
	spin_unlock_irqrestore(&queue->lock, flags);

	while (flush_list != NULL) {
		to_free = flush_list;
		flush_list = flush_list->next;

		if (to_free->fn_fail)
			to_free->fn_fail(to_free->ni, PPQ_FAIL_STOP);

		ieee80211_ppqueue_release_entry(to_free);
	}
}

void ieee80211_ppqueue_timeout(unsigned long ctx)
{
	struct ieee80211_pairing_pending_queue *queue = (struct ieee80211_pairing_pending_queue *)ctx;
	struct ieee80211_pairing_pending_entry **prev, *cur, *to_do, *timeout_retry, *timeout_fail;
	struct sk_buff *skb;
	unsigned long flags;

	prev = &queue->next;
	cur = queue->next;
	to_do = NULL;
	timeout_retry = NULL;
	timeout_fail = NULL;

	spin_lock_irqsave(&queue->lock, flags);
	while (cur != NULL) {
		if (time_before_eq(cur->next_expire_jiffies, jiffies)) {
			to_do = cur;
			*prev = cur->next;
			cur = cur->next;
			if (to_do->retry_cnt < to_do->max_retry)
				REPLACE_PPQ_ENTRY_HEAD(timeout_retry, to_do);
			else
				REPLACE_PPQ_ENTRY_HEAD(timeout_fail, to_do);
			continue;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	while (timeout_retry != NULL) {
		to_do = timeout_retry;
		timeout_retry = timeout_retry->next;

		to_do->retry_cnt++;
		to_do->next_expire_jiffies = jiffies + to_do->expire;
		skb = skb_clone(to_do->skb, GFP_ATOMIC);
		if (skb) {
			ieee80211_ref_node(to_do->ni);
			ieee80211_mgmt_output(to_do->ni, skb, IEEE80211_FC0_SUBTYPE_ACTION, to_do->ni->ni_macaddr);
		}
		ieee80211_ppqueue_insert_entry(queue, to_do);
	}

	while (timeout_fail != NULL) {
		to_do = timeout_fail;
		timeout_fail = timeout_fail->next;

		if (to_do->fn_fail)
			to_do->fn_fail(to_do->ni, PPQ_FAIL_TIMEOUT);

		ieee80211_ppqueue_release_entry(to_do);
	}
}

void ieee80211_ppqueue_init(struct ieee80211vap *vap)
{
	struct ieee80211_pairing_pending_queue *queue = (struct ieee80211_pairing_pending_queue *)&vap->iv_ppqueue;

	spin_lock_init(&queue->lock);
	init_timer(&queue->timer);
	queue->timer.data = (unsigned long)queue;
	queue->timer.function = ieee80211_ppqueue_timeout;
	queue->next = NULL;
	queue->next_expire_jiffies = 0;
}

void ieee80211_ppqueue_deinit(struct ieee80211vap *vap)
{
	struct ieee80211_pairing_pending_queue *queue = (struct ieee80211_pairing_pending_queue *)&vap->iv_ppqueue;

	del_timer(&queue->timer);
	ieee80211_ppqueue_flush(queue);
}

struct sk_buff *ieee80211_ppqueue_pre_tx(struct ieee80211_node *ni,
				struct sk_buff *skb,
				u_int8_t category,
				u_int8_t action,
				u_int8_t token,
				unsigned long expire,
				ppq_callback_success fn_success,
				ppq_callback_fail fn_fail)
{
	struct sk_buff *cloned_skb = NULL;
	struct ieee80211_pairing_pending_queue *queue = &ni->ni_vap->iv_ppqueue;
	struct ieee80211_pairing_pending_entry *entry = NULL;

	entry = (struct ieee80211_pairing_pending_entry *)kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (NULL == entry) {
		dev_kfree_skb_any(skb);
		return NULL;
	}

	cloned_skb = skb_clone(skb, GFP_ATOMIC);
	if (cloned_skb == NULL) {
		dev_kfree_skb_any(skb);
		kfree(entry);
		return NULL;
	}

	memset(entry, 0, sizeof(*entry));
	entry->skb = skb;
	entry->ni = ni;
	entry->expected_category = category;
	entry->expected_action = action;
	entry->expected_token = token;
	entry->expire = expire;
	entry->next_expire_jiffies = jiffies + expire;
	entry->max_retry = IEEE80211_PPQ_DEF_MAX_RETRY;
	entry->retry_cnt = 0;
	entry->fn_success = fn_success;
	entry->fn_fail = fn_fail;

	ieee80211_ppqueue_insert_entry(queue, entry);
	return cloned_skb;
}

int32_t ieee80211_compile_action_link_measure_request(struct ieee80211_node *ni,
		void *ctrl,
		struct sk_buff **p_skb)
{
	struct sk_buff *skb;
	int32_t frame_len = 0;
	struct ieee80211_link_measure_request *request;
	u_int8_t *frm;
	u_int8_t tx_token;

	request = (struct ieee80211_link_measure_request *)ctrl;
	frame_len = sizeof(struct ieee80211_action_rm_link_measure_request);

	skb = ieee80211_getmgtframe(&frm, frame_len);
	if (skb == NULL)
		return -1;

	*frm++ = IEEE80211_ACTION_CAT_RM;
	*frm++ = IEEE80211_ACTION_R_LINKMEASURE_REQUEST;

	if (ni->ni_action_token == 0)
		ni->ni_action_token++;
	tx_token = ni->ni_action_token++;
	*frm++ = tx_token;
	*frm++ = ni->ni_ic->ic_get_local_txpow(ni->ni_ic);
	*frm++ = ni->ni_ic->ic_curchan->ic_maxpower_normal + 6;	/* 4 anntenna, add 6 db */

	if (request->ppq.expire != 0) {
		skb = ieee80211_ppqueue_pre_tx(ni, skb, IEEE80211_ACTION_CAT_RM,
				IEEE80211_ACTION_R_LINKMEASURE_REPORT,
				tx_token, request->ppq.expire,
				request->ppq.fn_success, request->ppq.fn_fail);
		if (skb == NULL)
			return -1;
	}

	skb_trim(skb, frm - skb->data);
	*p_skb = skb;
	return 0;
}

int32_t ieee80211_compile_action_link_measure_report(struct ieee80211_node *ni,
		void *ctrl,
		struct sk_buff **p_skb)
{
	struct sk_buff *skb;
	int32_t frame_len = 0;
	struct ieee80211_link_measure_report *report;
	u_int8_t *frm;

	report = (struct ieee80211_link_measure_report *)ctrl;
	frame_len = sizeof(struct ieee80211_action_rm_link_measure_report);

	skb = ieee80211_getmgtframe(&frm, frame_len);
	if (skb == NULL)
		return -1;

	*frm++ = IEEE80211_ACTION_CAT_RM;
	*frm++ = IEEE80211_ACTION_R_LINKMEASURE_REPORT;
	*frm++ = report->token;
	*frm++ = IEEE80211_ELEMID_TPCREP;
	*frm++ = 2;
	*frm++ = report->tpc_report.tx_power;
	*frm++ = report->tpc_report.link_margin;
	*frm++ = report->recv_antenna_id;
	*frm++ = report->tran_antenna_id;
	*frm++ = report->rcpi;
	*frm++ = report->rsni;

	skb_trim(skb, frm - skb->data);
	*p_skb = skb;
	return 0;
}

int32_t ieee80211_compile_action_neighbor_report_request(struct ieee80211_node *ni,
		void *ctrl,
		struct sk_buff **p_skb)
{
	struct sk_buff *skb;
	int32_t frame_len = 0;
	u_int8_t *frm;
	u_int8_t tx_token;
	struct ieee80211_neighbor_report_request *request;

	request = (struct ieee80211_neighbor_report_request *)ctrl;
	frame_len = sizeof(struct ieee80211_action_rm_neighbor_report_request);
	skb = ieee80211_getmgtframe(&frm, frame_len);
	if (skb == NULL)
		return -1;

	*frm++ = IEEE80211_ACTION_CAT_RM;
	*frm++ = IEEE80211_ACTION_R_NEIGHBOR_REQUEST;
	if (ni->ni_action_token == 0)
		ni->ni_action_token++;
	tx_token = ni->ni_action_token++;
	*frm++ = tx_token;

	if (request->ppq.expire != 0) {
		skb = ieee80211_ppqueue_pre_tx(ni, skb, IEEE80211_ACTION_CAT_RM,
				IEEE80211_ACTION_R_NEIGHBOR_REPORT,
				tx_token, request->ppq.expire,
				request->ppq.fn_success, request->ppq.fn_fail);
		if (skb == NULL)
			return -1;
	}

	skb_trim(skb, frm - skb->data);
	*p_skb = skb;
	return 0;
}

int32_t ieee80211_compile_action_neighbor_report_response(struct ieee80211_node *ni,
		void *ctrl,
		struct sk_buff **p_skb)
{
	struct sk_buff *skb;
	int32_t frame_len = 0;
	u_int8_t *frm;
	struct ieee80211_neighbor_report_response *response;
	u_int8_t i;
	u_int8_t bss_num = 0;

	response = (struct ieee80211_neighbor_report_response *)ctrl;
	frame_len = sizeof(struct ieee80211_action_rm_neighbor_report_response);
	if (response->bss_num > 0) {
		bss_num = (response->bss_num > 32 ? 32 : response->bss_num);
		frame_len += sizeof(struct ieee80211_ie_neighbor_report) * bss_num;
	}
	skb = ieee80211_getmgtframe(&frm, frame_len);
	if (skb == NULL)
		return -1;

	*frm++ = IEEE80211_ACTION_CAT_RM;
	*frm++ = IEEE80211_ACTION_R_NEIGHBOR_REPORT;
	*frm++ = response->token;

	for (i = 0; i < bss_num; i++) {
		*frm++ = IEEE80211_ELEMID_NEIGHBOR_REP;
		*frm++ = sizeof(struct ieee80211_ie_neighbor_report) - 2;
		memcpy(frm, response->neighbor_report_ptr[i]->bssid, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
		ADDINT32(frm, response->neighbor_report_ptr[i]->bssid_info);
		*frm++ = response->neighbor_report_ptr[i]->operating_class;
		*frm++ = response->neighbor_report_ptr[i]->channel;
		*frm++ = response->neighbor_report_ptr[i]->phy_type;
	}

	skb_trim(skb, frm - skb->data);
	*p_skb = skb;
	return 0;
}

/*
 * To check whether to enable RX AMSDU or not.
 * Return 1: RX AMSDU can be enabled, 0: should be disabled
 */
static int ieee80211_rx_amsdu_allowed(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;

	if (vap->iv_rx_amsdu_enable == QTN_RX_AMSDU_DISABLE) {
		return 0;
	}

	if ((vap->iv_rx_amsdu_enable == QTN_RX_AMSDU_DYNAMIC) && !ieee80211_node_is_qtn(ni)) {
		if (ic->ic_scs.scs_stats_on) {
			if (vap->iv_rx_amsdu_threshold_cca && (ic->ic_opmode == IEEE80211_M_HOSTAP)) {
				struct ap_state *as = ic->ic_scan->ss_scs_priv;
				uint32_t cca_intf = as->as_cca_intf[ic->ic_curchan->ic_ieee];
				if ((cca_intf != SCS_CCA_INTF_INVALID) &&
						(cca_intf > vap->iv_rx_amsdu_threshold_cca)) {
					return 0;
				}
			}
			if (vap->iv_rx_amsdu_threshold_pmbl) {
				uint32_t pmbl_err = (vap->iv_rx_amsdu_pmbl_wf_sp * ic->ic_scs.scs_sp_err_smthed +
						vap->iv_rx_amsdu_pmbl_wf_lp * ic->ic_scs.scs_lp_err_smthed) / 100;

				if (pmbl_err > vap->iv_rx_amsdu_threshold_pmbl) {
					return 0;
				}
			}
		}
	}

	return 1;
}

void
ieee80211_get_channel_bw_offset(struct ieee80211com *ic, int16_t *is_40, int16_t *offset)
{
	*is_40 = 0;
	*offset = IEEE80211_HTINFO_CHOFF_SCN;

	if ((ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) &&
			(ic->ic_bsschan->ic_flags & IEEE80211_CHAN_HT40_DUAL_EXT)) {
		*offset = (ic->ic_bsschan->ic_flags & IEEE80211_CHAN_HT40U) ?
				IEEE80211_HTINFO_CHOFF_SCA : IEEE80211_HTINFO_CHOFF_SCB;
		*is_40 = 1;
	}
}

static int ieee80211_check_11b_ap(const struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_rateset *b_rates = &ic->ic_sup_rates[IEEE80211_MODE_11B];
	int i, j;

	for (i = 0; i < ni->ni_rates.rs_nrates; i++) {
		for (j = 0; j < b_rates->rs_nrates; j++) {
			if ((ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) ==
					(b_rates->rs_rates[j] & IEEE80211_RATE_VAL)) {
				break;
			}
		}

		if (j == b_rates->rs_nrates)
			return 0;
	}

	return 1;
}

/*
 * Send a management frame.  The node is for the destination (or ic_bss
 * when in station mode).  Nodes other than ic_bss have their reference
 * count bumped to reflect our use for an indeterminate time.
 */
int
ieee80211_send_mgmt(struct ieee80211_node *ni, int type, int arg)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb = NULL;
	u_int8_t *frm;
	int16_t htinfo_channel_width = 0;
	int16_t htinfo_2nd_channel_offset = 0;
	u_int16_t capinfo, def_keyindex;
	int has_challenge, is_shared_key, ret, timer, status, is_bcast_probe, expired_timer;
	enum ieee80211_phymode mode;
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
	int ap_pure_tkip = 0;
	int sta_pure_tkip = 0;
	int is_11b_ap;

	KASSERT(ni != NULL, ("null node"));

	if (vap->iv_opmode == IEEE80211_M_STA && ni != vap->iv_bss) {
		/*
		 * In Roaming cases, STA may receive null data frames from old AP
		 * if it's not disassociated properly.
		 * We add a exception here so that STA can send De-auth to old AP.
		 */
		if (type != IEEE80211_FC0_SUBTYPE_ACTION && type != IEEE80211_FC0_SUBTYPE_DEAUTH)
			return 0;
	}

	ieee80211_ref_node(ni);

	mode = ic->ic_curmode;
	timer = 0;

	if (vap->iv_bss && !vap->allow_tkip_for_vht) {
	      ap_pure_tkip = (vap->iv_bss->ni_rsn.rsn_ucastcipherset == IEEE80211_C_TKIP);
	      sta_pure_tkip = (vap->iv_bss->ni_rsn.rsn_ucastcipher == IEEE80211_CIPHER_TKIP);
	}

	switch (type) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		/*
		 * probe response frame format
	 	 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] capability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[7] FH/DS parameter set
		 *	[tlv] IBSS parameter set
		 *	[tlv] country code
		 *	[3] power constraint
		 *	[4] tpc report
		 *	[tlv] Channel Switch Announcement
		 *	[3] extended rate phy (ERP)
		 *	[tlv] extended supported rates
		 *	[tlv] WME parameters
		 *	[tlv] WPA/RSN parameters
		 *	[tlv] Atheros Advanced Capabilities
		 *	[tlv] AtherosXR parameters
		 *	[tlv] Quantenna parameters (probe resp)
		 *      [tlv] QTN IE
		 */
		skb = ieee80211_getmgtframe(&frm,
			  8	/* time stamp */
			+ sizeof(u_int16_t)	/* beacon interval */
			+ sizeof(u_int16_t)	/* capability information */
			+ 2 + IEEE80211_NWID_LEN	/* ssid */
			+ 2 + IEEE80211_RATE_SIZE	/* supported rates */
			+ 7	/* FH/DS parameters max(7,3) */
			/* XXX allocate max size */
			+ 4	/* IBSS parameter set*/
			+ 2 + ic->ic_country_ie.country_len	/* country code */
			+ ((vap->interworking) ? 7 : 0)	/* BSS load */
			+ 3	/* power constraint */
			+ 4	/* tpc report */
			+ IEEE80211_CHANSWITCHANN_BYTES  /*	CSA	*/
			+ 3	/* ERP */
			+ 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
			+ sizeof(struct ieee80211_wme_param)
			/* XXX !WPA1+WPA2 fits w/o a cluster */
			+ (vap->iv_flags & IEEE80211_F_WPA ?
				2 * sizeof(struct ieee80211_ie_wpa) : 0)
			+ ((ic->ic_curmode >= IEEE80211_MODE_11NA) ?
					(sizeof(struct ieee80211_ie_htcap) +
					 sizeof(struct ieee80211_ie_htinfo)) : 0)
			+ sizeof(struct ieee80211_ie_athAdvCap)
			+ vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_RESP].length
			+ sizeof(struct ieee80211_ie_qtn)
			+ sizeof(struct ieee80211_qtn_ext_role)
			+ sizeof(struct ieee80211_qtn_ext_bssid)
			+ (IS_IEEE80211_DUALBAND_VHT_ENABLED(ic) ?
				(sizeof(struct ieee80211_ie_vhtcap) +
				 sizeof(struct ieee80211_ie_vhtop) +
				 sizeof(struct ieee80211_ie_vtxpwren)) : 0)
			+ ((IS_IEEE80211_11NG(ic)) ?
				(sizeof(struct ieee80211_20_40_coex_param) +
				sizeof(struct ieee80211_obss_scan_ie)) : 0)
			);
		if (skb == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		/* timestamp should be filled later */
		memset(frm, 0, 8);
		frm += 8;

		/* beacon interval */
		*(__le16 *)frm = htole16(vap->iv_bss->ni_intval);
		frm += 2;

		/* cap. info */
		if (vap->iv_opmode == IEEE80211_M_IBSS)
			capinfo = IEEE80211_CAPINFO_IBSS;
		else
			capinfo = IEEE80211_CAPINFO_ESS;
		if (vap->iv_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		if (ic->ic_flags & IEEE80211_F_DOTH)
			capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
		*(__le16 *)frm = htole16(capinfo);
		frm += 2;

		/* ssid */
		is_bcast_probe = arg;
		if ((vap->iv_flags & IEEE80211_F_HIDESSID) && is_bcast_probe) {
			frm = ieee80211_add_ssid(frm, (u_int8_t *)"", 0);
		} else {
			frm = ieee80211_add_ssid(frm, vap->iv_bss->ni_essid,
				vap->iv_bss->ni_esslen);
		}

		/* supported rates */
		frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);

		/* XXX: FH/DS parameter set, correct ? */
		if (ic->ic_phytype == IEEE80211_T_FH) {
			*frm++ = IEEE80211_ELEMID_FHPARMS;
			*frm++ = 5;
			*frm++ = ni->ni_fhdwell & 0x00ff;
			*frm++ = (ni->ni_fhdwell >> 8) & 0x00ff;
			*frm++ = IEEE80211_FH_CHANSET(
				ieee80211_chan2ieee(ic, ic->ic_curchan));
			*frm++ = IEEE80211_FH_CHANPAT(
				ieee80211_chan2ieee(ic, ic->ic_curchan));
			*frm++ = ni->ni_fhindex;
		} else {
			*frm++ = IEEE80211_ELEMID_DSPARMS;
			*frm++ = 1;
			*frm++ = ieee80211_chan2ieee(ic, ic->ic_bsschan);
		}

		if (vap->iv_opmode == IEEE80211_M_IBSS) {
			*frm++ = IEEE80211_ELEMID_IBSSPARMS;
			*frm++ = 2;
			*frm++ = 0;
			*frm++ = 0;		/* TODO: ATIM window */
		}

		/*
		 * Tight coupling between Country IE and Power Constraint IE
		 * Both using IEEE80211_FEXT_COUNTRYIE to optional enable them.
		 */
		/* country code */
		if ((ic->ic_flags_ext & IEEE80211_FEXT_COUNTRYIE) ||
				((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)))
			frm = ieee80211_add_country(frm, ic);

		if (vap->interworking)
			frm = ieee80211_add_bss_load(frm, vap);

		/* power constraint */
		if (((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_COUNTRYIE)) ||
				((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext && IEEE80211_FEXT_TPC))) {
			*frm++ = IEEE80211_ELEMID_PWRCNSTR;
			*frm++ = 1;
			*frm++ = IEEE80211_PWRCONSTRAINT_VAL(ic);
		}

		if (IS_IEEE80211_11NG(ic)) {
			frm = ieee80211_add_20_40_bss_coex_ie(frm, vap->iv_coex);
			frm = ieee80211_add_obss_scan_ie(frm, &ic->ic_obss_ie);
		}

		/* Transmit power envelope */
		if (IS_IEEE80211_VHT_ENABLED(ic) && (ic->ic_flags & IEEE80211_F_DOTH)) {
			frm = ieee80211_add_vhttxpwr_envelope(frm, ic);
		}

		/*TPC Report*/
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			*frm++ = IEEE80211_ELEMID_TPCREP;
			*frm++ = 2;
			*frm++ = ic->ic_get_local_txpow(ic);	/* tx power would be updated in macfw */
			*frm++ = 0;	/* link margin is 0 */
		}

		/*	CSA	*/
		if (ic->ic_csa_count) {
			frm = ieee80211_add_csa(frm, ic->ic_csa_mode,
					ic->ic_csa_chan->ic_ieee, ic->ic_csa_count);
			ieee80211_add_sec_chan_off(&frm, ic, ic->ic_csa_chan->ic_ieee);
		}

		/* ERP */
		if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
			((ic->ic_curmode == IEEE80211_MODE_11A) ||
			(ic->ic_curmode == IEEE80211_MODE_11B))) {
			frm = ieee80211_add_erp(frm, ic);
		}

		ieee80211_get_channel_bw_offset(ic, &htinfo_channel_width, &htinfo_2nd_channel_offset);

		/* 802.11n specific IEs */
		if (IEEE80211_IS_CHAN_ANYN(ic->ic_bsschan) &&
			(ic->ic_curmode >= IEEE80211_MODE_11NA) && !ap_pure_tkip) {
			frm = ieee80211_add_htcap(ni, frm, &ic->ic_htcap, type);
			ic->ic_htinfo.ctrlchannel = ieee80211_chan2ieee(ic, ic->ic_bsschan);
			ic->ic_htinfo.byte1 |= (htinfo_channel_width ? IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40 : 0x0);
			ic->ic_htinfo.choffset = htinfo_2nd_channel_offset;
			frm = ieee80211_add_htinfo(ni, frm, &ic->ic_htinfo);
		}

		/* Ext. Supp. Rates */
		frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);

		/* WME */
		if (vap->iv_flags & IEEE80211_F_WME)
			frm = ieee80211_add_wme_param(frm, wme,
					IEEE80211_VAP_UAPSD_ENABLED(vap), 0);

		/* WPA */
		if (!vap->iv_osen && (vap->iv_flags & IEEE80211_F_WPA))
			frm = ieee80211_add_wpa(frm, vap);

		/* AthAdvCaps */
		if (vap->iv_bss && vap->iv_bss->ni_ath_flags)
			frm = ieee80211_add_athAdvCap(frm, vap->iv_bss->ni_ath_flags,
					vap->iv_bss->ni_ath_defkeyindex);

		if (vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_RESP].ie) {
			memcpy(frm, vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_RESP].ie,
					vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_RESP].length);
			frm += vap->app_ie[IEEE80211_APPIE_FRAME_PROBE_RESP].length;
		}

		if (IS_IEEE80211_VHT_ENABLED(ic) && !ap_pure_tkip) {
			frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap, type);

			/* VHT Operation element */
			if ((IEEE80211_IS_VHT_40(ic)) || (IEEE80211_IS_VHT_20(ic))) {
				ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
				ic->ic_vhtop.centerfreq0 = 0;
			} else if (IEEE80211_IS_VHT_80(ic)) {
				ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
				ic->ic_vhtop.centerfreq0 = ic->ic_bsschan->ic_center_f_80MHz;
			} else {
				ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_160MHZ;
				ic->ic_vhtop.centerfreq0 = ic->ic_bsschan->ic_center_f_160MHz;
			}
			frm = ieee80211_add_vhtop(ni, frm, &ic->ic_vhtop);
		} else if (IS_IEEE80211_11NG_VHT_ENABLED(ic) && !ap_pure_tkip) {
			/* QTN 2.4G VHT IE */
			frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap_24g, type);
			frm = ieee80211_add_vhtop(ni, frm, &ic->ic_vhtop_24g);
		}

		frm = ieee80211_add_qtn_ie(frm, ic,
			(vap->iv_flags_ext & IEEE80211_FEXT_WDS ? (IEEE80211_QTN_BRIDGEMODE) : 0),
			(vap->iv_flags_ext & IEEE80211_FEXT_WDS ?
				(IEEE80211_QTN_BRIDGEMODE | IEEE80211_QTN_LNCB) : 0),
			0, 0, 0);

		/* Add Quantenna extender IE */
		if (!IEEE80211_COM_WDS_IS_NONE(ic) && (vap == TAILQ_FIRST(&ic->ic_vaps))) {
			frm = ieee80211_add_qtn_extender_role_ie(frm, ic->ic_extender_role);
			frm = ieee80211_add_qtn_extender_bssid_ie(vap, frm);
			frm = ieee80211_add_qtn_extender_state_ie(frm, !!ic->ic_ocac.ocac_cfg.ocac_enable);
		}

#ifdef CONFIG_QVSP
		/* QTN WME IE */
		if (ic->ic_wme.wme_throt_bm && ic->ic_wme.wme_throt_add_qwme_ie &&
				(vap->iv_flags & IEEE80211_F_WME)) {
			frm = ieee80211_add_qtn_wme_param(vap, frm);
		}
#endif
		skb_trim(skb, frm - skb->data);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH:
		status = arg >> 16;
		arg &= 0xffff;
		has_challenge = ((arg == IEEE80211_AUTH_SHARED_CHALLENGE ||
			arg == IEEE80211_AUTH_SHARED_RESPONSE) &&
			ni->ni_challenge != NULL);

		/*
		 * Deduce whether we're doing open authentication or
		 * shared key authentication.  We do the latter if
		 * we're in the middle of a shared key authentication
		 * handshake or if we're initiating an authentication
		 * request and configured to use shared key.
		 */
		is_shared_key = has_challenge ||
			arg >= IEEE80211_AUTH_SHARED_RESPONSE ||
			(arg == IEEE80211_AUTH_SHARED_REQUEST &&
			vap->iv_bss->ni_authmode == IEEE80211_AUTH_SHARED);

		skb = ieee80211_getmgtframe(&frm,
			3 * sizeof(u_int16_t)
			+ (has_challenge && status == IEEE80211_STATUS_SUCCESS ?
				sizeof(u_int16_t)+IEEE80211_CHALLENGE_LEN : 0));
		if (skb == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		((__le16 *)frm)[0] =
			(is_shared_key) ? htole16(IEEE80211_AUTH_ALG_SHARED)
				: htole16(IEEE80211_AUTH_ALG_OPEN);
		((__le16 *)frm)[1] = htole16(arg);	/* sequence number */
		((__le16 *)frm)[2] = htole16(status);	/* status */

		if (has_challenge && status == IEEE80211_STATUS_SUCCESS) {
			((__le16 *)frm)[3] =
				htole16((IEEE80211_CHALLENGE_LEN << 8) |
					IEEE80211_ELEMID_CHALLENGE);
			memcpy(&((__le16 *)frm)[4], ni->ni_challenge,
				IEEE80211_CHALLENGE_LEN);
			if (arg == IEEE80211_AUTH_SHARED_RESPONSE) {
				IEEE80211_NOTE(vap, IEEE80211_MSG_AUTH, ni,
					"request encrypt frame (%s)", __func__);
				M_FLAG_SET(skb, M_LINK0);
			}
		}

		/* XXX not right for shared key */
		if (status == IEEE80211_STATUS_SUCCESS) {
			IEEE80211_NODE_STAT(ni, tx_auth);
			if (arg == IEEE80211_AUTH_OPEN_RESPONSE && vap->iv_opmode == IEEE80211_M_HOSTAP) {
				char event_string[IW_CUSTOM_MAX]; /* Buffer for IWEVENT message */
				union iwreq_data wreq;
				memset(&wreq, 0, sizeof(wreq));
				snprintf(event_string,IW_CUSTOM_MAX,"%sClient authenticated [%pM]",
							QEVT_COMMON_PREFIX, ni->ni_macaddr);
				wreq.data.length = strlen(event_string);
				wireless_send_event(vap->iv_dev, IWEVCUSTOM, &wreq, event_string);
			}
		} else {
			IEEE80211_NODE_STAT(ni, tx_auth_fail);
			if (arg == IEEE80211_AUTH_OPEN_RESPONSE && vap->iv_opmode == IEEE80211_M_HOSTAP) {
				char event_string[IW_CUSTOM_MAX]; /* Buffer for IWEVENT message */
				union iwreq_data wreq;
				memset(&wreq, 0, sizeof(wreq));
				snprintf(event_string,IW_CUSTOM_MAX,"%sClient failed to authenticate [%pM]",
							QEVT_COMMON_PREFIX, ni->ni_macaddr);
				wreq.data.length = strlen(event_string);
				wireless_send_event(vap->iv_dev, IWEVCUSTOM, &wreq, event_string);
			}
		}

		if (vap->iv_opmode == IEEE80211_M_STA)
			timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		IEEE80211_NOTE(vap, IEEE80211_MSG_AUTH, ni,
			"send station deauthenticate (reason %d)", arg);
		skb = ieee80211_getmgtframe(&frm, sizeof(u_int16_t));
		if (skb == NULL)
			senderr(ENOMEM, is_tx_nobuf);
		*(__le16 *)frm = htole16(arg);	/* reason */

		IEEE80211_NODE_STAT(ni, tx_deauth);
		IEEE80211_NODE_STAT_SET(ni, tx_deauth_code, arg);
		{
			int msg = IEEE80211_DOT11_MSG_CLIENT_REMOVED;
			if (vap->iv_opmode == IEEE80211_M_STA) {
				msg = IEEE80211_DOT11_MSG_AP_DISCONNECTED;
			}
			if (arg == IEEE80211_REASON_AUTH_EXPIRE) {
				ieee80211_eventf(vap->iv_dev,"%s[WLAN access rejected: incorrect "
						 "security] from MAC address %pM", QEVT_ACL_PREFIX,
						 ni->ni_macaddr);
			}
			ieee80211_dot11_msg_send(ni->ni_vap,
					(char *)ni->ni_macaddr,
					d11_m[msg],
					d11_c[IEEE80211_DOT11_MSG_REASON_DEAUTHENTICATED],
					arg,
					(arg < DOT11_MAX_REASON_CODE) ? d11_r[arg] : "Reserved",
					NULL,
					NULL);
		}

		ieee80211_node_unauthorize(ni);		/* port closed */
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] listen interval
		 *	[6*] current AP address (reassoc only)
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[4] power capability (802.11h)
		 *	[54] supported channels element (802.11h)
		 *	[tlv] extended supported rates
		 *	[tlv] WME [if enabled and AP capable]
		 *      [tlv] Atheros advanced capabilities
		 *	[tlv] user-specified ie's
		 *      [tlv] QTN IE
		 */
		skb = ieee80211_getmgtframe(&frm,
			sizeof(u_int16_t) +
			sizeof(u_int16_t) +
			IEEE80211_ADDR_LEN +
			2 + IEEE80211_NWID_LEN +
			2 + IEEE80211_RATE_SIZE +
			4 + (2 + IEEE80211_SUPPCHAN_LEN) +
			2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE) +
			((ic->ic_curmode >= IEEE80211_MODE_11NA) ?
					(sizeof(struct ieee80211_ie_htcap) +
					 sizeof(struct ieee80211_extcap_param)) : 0) +
			sizeof(struct ieee80211_ie_wme) +
			sizeof(struct ieee80211_ie_athAdvCap) +
			(vap->iv_opt_ie != NULL ? vap->iv_opt_ie_len : 0) +
			vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_REQ].length +
			sizeof(struct ieee80211_ie_qtn) +
			(vap->qtn_pairing_ie.ie ? sizeof(struct ieee80211_ie_qtn_pairing) : 0) +
			(IS_IEEE80211_DUALBAND_VHT_ENABLED(ic) ?
				sizeof(struct ieee80211_ie_vhtcap) +
				sizeof(struct ieee80211_ie_vhtop_notif) : 0)
			);
		if (skb == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		capinfo = 0;
		if (vap->iv_opmode == IEEE80211_M_IBSS)
			capinfo |= IEEE80211_CAPINFO_IBSS;
		else		/* IEEE80211_M_STA */
			capinfo |= IEEE80211_CAPINFO_ESS;
		if (vap->iv_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		/*
		 * NB: Some 11a AP's reject the request when
		 *     short premable is set.
		 */
		/* Capability information */
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) &&
		    (ic->ic_caps & IEEE80211_C_SHSLOT))
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		if ((ic->ic_flags & IEEE80211_F_DOTH) &&
			(vap->iv_bss->ni_flags & IEEE80211_NODE_HT)) {
			capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
		}
		*(__le16 *)frm = htole16(capinfo);
		frm += 2;

		/* listen interval */
		*(__le16 *)frm = htole16(ic->ic_lintval / ni->ni_intval);
		frm += 2;

		/* Current AP address */
		if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
			IEEE80211_ADDR_COPY(frm, vap->iv_bss->ni_bssid);
			frm += IEEE80211_ADDR_LEN;
		}
		/* ssid */
		frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);

		is_11b_ap = ieee80211_check_11b_ap(ni);
		/* supported rates */
		if (!is_11b_ap) {
			frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);
		} else {
			frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[IEEE80211_MODE_11B]);
		}

		if ((ic->ic_curmode >= IEEE80211_MODE_11NA) &&
			(vap->iv_bss->ni_flags & IEEE80211_NODE_HT) && !sta_pure_tkip) {
			frm = ieee80211_add_htcap(ni, frm, &ic->ic_htcap, type);
			/* Ext. Capabilities - For AP mode hostapd adds the extended cap */
			if (vap->iv_opmode == IEEE80211_M_STA)
				frm = ieee80211_add_extcap(frm);
		}

		/* ext. supp. rates */
		if (!is_11b_ap) {
			frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);
		}

		/* power capability/supported channels
		 * in chapter 8.3.3.5, power capability IE is right after extended supported rates
		 * and before supported channels
		 * */
		if (ic->ic_flags & IEEE80211_F_DOTH)
			frm = ieee80211_add_doth(frm, ic);

		/* Supported Channels */
		frm = ieee80211_add_supported_chans(frm, ic);

		/* WME */
		if ((vap->iv_flags & IEEE80211_F_WME) && (ni->ni_wme_ie != NULL))
			frm = ieee80211_add_wme(frm, ni);

		/* ath adv. cap */
		if (ni->ni_ath_flags & vap->iv_ath_cap) {
			IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
				"Adding ath adv cap ie: ni_ath_flags = %02x, "
				"iv_ath_cap = %02x", ni->ni_ath_flags,
				vap->iv_ath_cap);

			/* Setup default key index for static wep case */
			def_keyindex = IEEE80211_INVAL_DEFKEY;
			if (((vap->iv_flags & IEEE80211_F_WPA) == 0) &&
			    (ni->ni_authmode != IEEE80211_AUTH_8021X) &&
			    (vap->iv_def_txkey != IEEE80211_KEYIX_NONE))
				def_keyindex = vap->iv_def_txkey;

			frm = ieee80211_add_athAdvCap(frm,
				ni->ni_ath_flags & vap->iv_ath_cap,
				def_keyindex);
		}

		/* 802.11ac vht capability */
		if (IS_IEEE80211_VHT_ENABLED(ic) && !sta_pure_tkip) {
			frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap, type);
			frm = ieee80211_add_vhtop_notif(ni, frm, ic, 0);
		} else if (IS_IEEE80211_11NG_VHT_ENABLED(ic) && !sta_pure_tkip) {
			/* QTN 2.4G VHT IE */
			frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap_24g, type);
			frm = ieee80211_add_vhtop_notif(ni, frm, ic, 1);
		}

		/* User-spec */
		if (vap->iv_opt_ie != NULL) {
			memcpy(frm, vap->iv_opt_ie, vap->iv_opt_ie_len);
			frm += vap->iv_opt_ie_len;
		}

		if (vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_REQ].ie) {
			memcpy(frm, vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_REQ].ie,
				vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_REQ].length);
			frm += vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_REQ].length;
		}

		frm = ieee80211_add_qtn_ie(frm, ic,
			(vap->iv_flags_ext & IEEE80211_FEXT_WDS ? (IEEE80211_QTN_BRIDGEMODE) : 0),
			(vap->iv_flags_ext & IEEE80211_FEXT_WDS ?
				(IEEE80211_QTN_BRIDGEMODE | IEEE80211_QTN_LNCB) : 0),
			vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H,
			ni->ni_rate_train);

		/* Add QTN Pairing IE */
		if (vap->qtn_pairing_ie.ie) {
			frm = ieee80211_add_qtn_pairing_ie(frm, &vap->qtn_pairing_ie);
		}

		skb_trim(skb, frm - skb->data);

		timer = IEEE80211_TRANS_WAIT;
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
			       "send station %s",
			       (type == IEEE80211_FC0_SUBTYPE_ASSOC_REQ) ? "assoc_req" : "reassoc_req");
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *      [tlv] WME (if enabled and STA enabled)
		 *      [tlv] Atheros Advanced Capabilities
		 *      [tlv] QTN IE
		 *      [tlv] VSP IE
		 */
		skb = ieee80211_getmgtframe(&frm,
			3 * sizeof(u_int16_t) +
			2 + IEEE80211_RATE_SIZE +
			2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE) +
			((IEEE80211_IS_CHAN_ANYN(ic->ic_bsschan) &&
			  (ic->ic_curmode >= IEEE80211_MODE_11NA)) ?
					(sizeof(struct ieee80211_ie_htcap) +
					 sizeof(struct ieee80211_ie_htinfo)) : 0) +
			sizeof(struct ieee80211_wme_param) +
			(vap->iv_ath_cap ? sizeof(struct ieee80211_ie_athAdvCap) : 0) +
			vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_RESP].length +
			sizeof(struct ieee80211_ie_qtn) +
			(vap->qtn_pairing_ie.ie ? sizeof(struct ieee80211_ie_qtn_pairing) : 0)
#ifdef CONFIG_QVSP
			+ ieee80211_vsp_ie_max_len(ic)
#endif
			+ (IS_IEEE80211_DUALBAND_VHT_ENABLED(ic) ?
				(sizeof(struct ieee80211_ie_vhtcap) +
				 sizeof(struct ieee80211_ie_vhtop)) : 0)
			+ ((IS_IEEE80211_11NG(ic)) ?
				(sizeof(struct ieee80211_20_40_coex_param) +
				sizeof(struct ieee80211_obss_scan_ie)) : 0)
			);
		if (skb == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		/* Capability Information */
		capinfo = IEEE80211_CAPINFO_ESS;
		if (vap->iv_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		if (ic->ic_flags & IEEE80211_F_DOTH)
			capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
		*(__le16 *)frm = htole16(capinfo);
		frm += 2;

		/* status */
		*(__le16 *)frm = htole16(arg);
		frm += 2;

		/* Assoc ID */
		if (arg == IEEE80211_STATUS_SUCCESS) {
			*(__le16 *)frm = htole16(ni->ni_associd);
			IEEE80211_NODE_STAT(ni, tx_assoc);
		} else
			IEEE80211_NODE_STAT(ni, tx_assoc_fail);
		frm += 2;

		/* supported rates */
		frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);

		if (IS_IEEE80211_11NG(ic)) {
			frm = ieee80211_add_20_40_bss_coex_ie(frm, vap->iv_coex);
			frm = ieee80211_add_obss_scan_ie(frm, &ic->ic_obss_ie);
		}

		/* 802.11w / PMF Timeout element */
		if(vap->iv_pmf && arg == IEEE80211_STATUS_PMF_REJECT_RETRY) {
			frm = ieee80211_add_timeout_ie(frm);
		}

		if (IEEE80211_IS_CHAN_ANYN(ic->ic_bsschan) &&
			(ic->ic_curmode >= IEEE80211_MODE_11NA)) {
			if (arg == IEEE80211_STATUS_SUCCESS) {
				if ((ni->ni_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) == 0) {
					vap->iv_ht_anomaly_40MHz_present = 1;
				}

				if ((ni->ni_htcap.cap & IEEE80211_HTCAP_C_GREENFIELD) == 0)
					vap->iv_non_gf_sta_present = 1;

				if ((ni->ni_htcap.cap & IEEE80211_HTCAP_C_LSIGTXOPPROT) == 0)
					vap->iv_lsig_txop_ok = 0;

				vap->iv_ht_flags |= IEEE80211_HTF_HTINFOUPDATE;

			}
			if (!ap_pure_tkip &&
				(ni->ni_rsn.rsn_ucastcipher != IEEE80211_CIPHER_TKIP) &&
					(ni->ni_flags >= IEEE80211_NODE_HT)) {
				frm = ieee80211_add_htcap(ni, frm, &ic->ic_htcap, type);
				frm = ieee80211_add_htinfo(ni, frm, &ic->ic_htinfo);
			}
		}

		/* ext. suppo. rates */
		frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);

		/* WME */
		if ((vap->iv_flags & IEEE80211_F_WME) && (ni->ni_wme_ie != NULL))
			frm = ieee80211_add_wme_param(frm, wme,
						IEEE80211_VAP_UAPSD_ENABLED(vap), 0);

		/* athAdvCap */
		if (vap->iv_ath_cap)
			frm = ieee80211_add_athAdvCap(frm,
				vap->iv_ath_cap & ni->ni_ath_flags,
				ni->ni_ath_defkeyindex);

		if (vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_RESP].ie) {
			memcpy(frm, vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_RESP].ie,
				vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_RESP].length);
			frm += vap->app_ie[IEEE80211_APPIE_FRAME_ASSOC_RESP].length;
		}

		if (ni->ni_qtn_assoc_ie) {
			frm = ieee80211_add_qtn_ie(frm, ic,
				((struct ieee80211_ie_qtn *)ni->ni_qtn_assoc_ie)->qtn_ie_flags,
				(vap->iv_flags_ext & IEEE80211_FEXT_WDS ?
					(IEEE80211_QTN_BRIDGEMODE | IEEE80211_QTN_LNCB) : 0),
				vap->iv_implicit_ba, IEEE80211_DEFAULT_BA_WINSIZE_H,
				ni->ni_rate_train);
#ifdef CONFIG_QVSP
			frm = ieee80211_add_vsp_ie(vap, frm, skb->end);
			/* QTN WME IE */
			if (ic->ic_wme.wme_throt_bm && ic->ic_wme.wme_throt_add_qwme_ie &&
					(vap->iv_flags & IEEE80211_F_WME)) {
				frm = ieee80211_add_qtn_wme_param(vap, frm);
			}
#endif
		}
		if (!ap_pure_tkip && (ni->ni_flags & IEEE80211_NODE_VHT)) {
			if (IS_IEEE80211_VHT_ENABLED(ic)) {
				frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap, type);

				/* VHT Operation element */
				if ((IEEE80211_IS_VHT_40(ic)) || (IEEE80211_IS_VHT_20(ic))) {
					ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
					ic->ic_vhtop.centerfreq0 = 0;
				} else if (IEEE80211_IS_VHT_80(ic)) {
					ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
					ic->ic_vhtop.centerfreq0 = ic->ic_bsschan->ic_center_f_80MHz;
				} else {
					ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_160MHZ;
					ic->ic_vhtop.centerfreq0 = ic->ic_bsschan->ic_center_f_160MHz;
				}
				frm = ieee80211_add_vhtop(ni, frm, &ic->ic_vhtop);
			} else if (IS_IEEE80211_11NG_VHT_ENABLED(ic)) {
				/* QTN 2.4G VHT IE */
				frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap_24g, type);
				frm = ieee80211_add_vhtop(ni, frm, &ic->ic_vhtop_24g);
			}
		}

		/* Add QTN Pairing IE. */
		if (vap->qtn_pairing_ie.ie) {
			frm = ieee80211_add_qtn_pairing_ie(frm, &vap->qtn_pairing_ie);
		}

		skb_trim(skb, frm - skb->data);
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
			       "send station %s (reason %d)",
			       (type == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) ? "assoc_resp" : "reassoc_resp", arg);

		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			ieee80211_eventf(vap->iv_dev, "%sClient associated [%pM]",
					 QEVT_COMMON_PREFIX, ni->ni_macaddr);
		}
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
		    "send station disassociate (reason %d)", arg);
		skb = ieee80211_getmgtframe(&frm, sizeof(u_int16_t));
		if (skb == NULL)
			senderr(ENOMEM, is_tx_nobuf);
		*(__le16 *)frm = htole16(arg);	/* reason */

		{
			int msg = IEEE80211_DOT11_MSG_CLIENT_REMOVED;
			if (vap->iv_opmode == IEEE80211_M_STA) {
				msg = IEEE80211_DOT11_MSG_AP_DISCONNECTED;
			}
			ieee80211_dot11_msg_send(ni->ni_vap,
					(char *)ni->ni_macaddr,
					d11_m[msg],
					d11_c[IEEE80211_DOT11_MSG_REASON_DISASSOCIATED],
					arg,
					(arg < DOT11_MAX_REASON_CODE) ? d11_r[arg] : "Reserved",
					NULL,
					NULL);
		}

		if (ic->ic_opmode == IEEE80211_M_STA)
			del_timer_sync(&ic->ic_obss_timer);

		IEEE80211_NODE_STAT(ni, tx_disassoc);
		IEEE80211_NODE_STAT_SET(ni, tx_disassoc_code, arg);
		break;

	case IEEE80211_FC0_SUBTYPE_ACTION: {
		u_int16_t temp16;
		struct ieee80211_action_data *action_data = (struct ieee80211_action_data *)arg;
		u_int8_t cat = action_data->cat;

		IEEE80211_NODE_STAT(ni, tx_action);

		switch (cat) {
		case IEEE80211_ACTION_CAT_SPEC_MGMT:
			switch (action_data->action) {
			case IEEE80211_ACTION_S_TPC_REQUEST:
			{
				struct ieee80211_action_tpc_request *request;
				u_int8_t tx_token;

				request = (struct ieee80211_action_tpc_request *)action_data->params;
				skb = ieee80211_getmgtframe(&frm, sizeof(u_int16_t) +	/* action header */
									1 +		/* dialog token */
									2);		/* tpc request ie */
				if (skb == NULL)
					senderr(ENOMEM, is_tx_nobuf);
				*frm++ = IEEE80211_ACTION_CAT_SPEC_MGMT;
				*frm++ = IEEE80211_ACTION_S_TPC_REQUEST;
				tx_token = ni->ni_action_token++;
				*frm++ = tx_token;
				*frm++ = IEEE80211_ELEMID_TPCREQ;
				*frm++ = 0;

				if (request->expire != 0) {
					skb = ieee80211_ppqueue_pre_tx(ni,
							skb,
							IEEE80211_ACTION_CAT_SPEC_MGMT,
							IEEE80211_ACTION_S_TPC_REPORT,
							tx_token,
							request->expire,
							request->fn_success,
							request->fn_fail);

					if (skb == NULL) {
						ret = -ENOMEM;
						goto bad;
					}
				}
				break;
			}
			case IEEE80211_ACTION_S_TPC_REPORT: {
				struct ieee80211_action_tpc_report *tpc_report = (struct ieee80211_action_tpc_report *)action_data->params;
				skb = ieee80211_getmgtframe(&frm, sizeof(u_int16_t) +	/* action header */
									1 +		/* dialog token */
									4);		/* tpc report ie */
				if (skb == NULL)
					senderr(ENOMEM, is_tx_nobuf);
				*frm++ = IEEE80211_ACTION_CAT_SPEC_MGMT;
				*frm++ = IEEE80211_ACTION_S_TPC_REPORT;
				*frm++ = tpc_report->rx_token;
				*frm++ = IEEE80211_ELEMID_TPCREP;
				*frm++ = 2;
				*frm++ = tpc_report->tx_power;
				*frm++ = tpc_report->link_margin;
				break;
			}
			case IEEE80211_ACTION_S_MEASUREMENT_REQUEST:
			case IEEE80211_ACTION_S_MEASUREMENT_REPORT:
				ret = ieee80211_compile_action_measurement_11h(ni, action_data->params, action_data->action, &skb);
				if (ret != 0)
					goto bad;
				break;
			default:
				break;
			}
			break;
		case IEEE80211_ACTION_CAT_HT: {
			switch (action_data->action) {
			case IEEE80211_ACTION_HT_NCBEAMFORMING:
				IEEE80211_DPRINTF(vap,IEEE80211_MSG_OUTPUT,
					"Err: Action frame construction not suppported\n",0);
				break;
			case IEEE80211_ACTION_HT_MIMOPWRSAVE:
				{
				/* Form the HT SM PS frame - change of mode for this client. */
				/* Single byte argument, which is formatted as per 802.11n d11.0 section 7.3.1.22 */
				u_int8_t *p_byte = (u_int8_t *)&action_data->params;
				skb = ieee80211_getmgtframe(&frm, sizeof(u_int16_t) + /* action header */
								1 /* SMPS state change */ );
				if (skb == NULL) {
					senderr(ENOMEM, is_tx_nobuf);
				}

				*(u_int8_t *)frm = IEEE80211_ACTION_CAT_HT;
				frm += 1;

				*(u_int8_t *)frm = IEEE80211_ACTION_HT_MIMOPWRSAVE;
				frm += 1;

				*(u_int8_t *)frm = *p_byte; /* New power save mode */
				frm += 1;
				}
				break;
			default:
				break;
			}

			if(skb != NULL) {
				skb_trim(skb, frm - skb->data);
			}
			break;
		}
		case IEEE80211_ACTION_CAT_BA: {
			switch (action_data->action) {
			case IEEE80211_ACTION_BA_ADDBA_REQ: {
				struct ba_action_req *ba = (struct ba_action_req *)action_data->params;

				skb = ieee80211_getmgtframe(&frm,
							sizeof(u_int16_t) + /* action header */
							sizeof(u_int8_t) + /* dialog */
							sizeof(u_int16_t) + /* BA params */
							sizeof(u_int16_t) + /* BA timeout */
							sizeof(u_int16_t) /* BA sequence control */
							);
				if (skb == NULL) {
					senderr(ENOMEM, is_tx_nobuf);
				}

				*(u_int8_t *)frm = IEEE80211_ACTION_CAT_BA;
				frm += 1;

				*(u_int8_t *)frm = IEEE80211_ACTION_BA_ADDBA_REQ;
				frm += 1;

				/* fill ba dialog */
				ni->ni_ba_tx[ba->tid].dlg_out = (ni->ni_ba_tx[ba->tid].dlg_out + 1) % 0xFF;
				*(u_int8_t *)frm = ni->ni_ba_tx[ba->tid].dlg_out;
				frm += 1;

				/* fill ba params (non half word aligned) */
				temp16 = 0;
				temp16 |= ((ba->type == IEEE80211_BA_DELAYED) ?
						IEEE80211_A_BA_DELAYED : IEEE80211_A_BA_IMMEDIATE);
				temp16 |= ((ba->tid) << IEEE80211_A_BA_TID_S);
				temp16 |= ((ba->buff_size) << IEEE80211_A_BA_BUFF_SIZE_S);
				if (vap->iv_tx_amsdu)
					temp16 |= IEEE80211_A_BA_AMSDU_SUPPORTED;

				*(u_int8_t *)frm = temp16 & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = temp16 >> 8;
				frm += 1;

				/* fill ba timeout (non half word aligned) */
				*(u_int8_t *)frm = ba->timeout & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = ba->timeout >> 8;
				frm += 1;

				/* fill sequence control (non half word aligned) */
				*(u_int8_t *)frm = ba->frag | ((ba->seq & 0xF) << 4);
				frm += 1;

				*(u_int8_t *)frm = (ba->seq  & 0xFF0) >> 4;
				frm += 1;
				break;
			}
			case IEEE80211_ACTION_BA_ADDBA_RESP: {
				struct ba_action_resp *ba = (struct ba_action_resp *)action_data->params;
				skb = ieee80211_getmgtframe(&frm,
							sizeof(u_int16_t) + /* action header */
							sizeof(u_int8_t) + /* dialog */
							sizeof(u_int16_t) + /* status */
							sizeof(u_int16_t) + /* BA params */
							sizeof(u_int16_t) /* BA timeout */
							);
				if (skb == NULL)
					senderr(ENOMEM, is_tx_nobuf);

				*(u_int8_t *)frm = IEEE80211_ACTION_CAT_BA;
				frm += 1;

				*(u_int8_t *)frm = IEEE80211_ACTION_BA_ADDBA_RESP;
				frm += 1;

				/* fill ba dialog */
				*(u_int8_t *)frm = ni->ni_ba_rx[ba->tid].dlg_in;
				frm += 1;

				/* fill ba status (non half word aligned) */
				*(u_int8_t *)frm = ba->reason & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = ba->reason >> 8;
				frm += 1;

				/* fill ba params (non half word aligned) */
				temp16 = 0;
				temp16 |= ((ba->type == IEEE80211_BA_DELAYED) ?
						IEEE80211_A_BA_DELAYED : IEEE80211_A_BA_IMMEDIATE);
				temp16 |= ((ba->tid) << IEEE80211_A_BA_TID_S);
				temp16 |= ((ba->buff_size) << IEEE80211_A_BA_BUFF_SIZE_S);

				if (!ieee80211_rx_amsdu_allowed(ni)) {
					IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,
							"receive AMSDU within AMPDU is not permitted from \
							station %pM\n",	ni->ni_macaddr);
					temp16 &= ~IEEE80211_A_BA_AMSDU_SUPPORTED;
				} else {
					temp16 |= IEEE80211_A_BA_AMSDU_SUPPORTED;
				}

				*(u_int8_t *)frm = temp16 & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = temp16 >> 8;
				frm += 1;

				/* fill ba timeout (non half word aligned) */
				*(u_int8_t *)frm = ba->timeout & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = ba->timeout >> 8;
				frm += 1;
				break;
			}
			case IEEE80211_ACTION_BA_DELBA: {
				struct ba_action_del *ba = (struct ba_action_del *)action_data->params;
				skb = ieee80211_getmgtframe(&frm,
							sizeof(u_int16_t) + /* action header */
							sizeof(u_int16_t) + /* DELBA params */
							sizeof(u_int16_t) /* DELBA reason */
							);
				if (skb == NULL)
					senderr(ENOMEM, is_tx_nobuf);

				*(u_int8_t *)frm = IEEE80211_ACTION_CAT_BA;
				frm += 1;

				*(u_int8_t *)frm = IEEE80211_ACTION_BA_DELBA;
				frm += 1;

				/* fill ba params (non half word aligned) */
				temp16 = 0;
				temp16 |= SM(ba->tid, IEEE80211_A_BA_DELBA_TID);
				temp16 |= SM(ba->initiator, IEEE80211_A_BA_INITIATOR);

				*(u_int8_t *)frm = temp16 & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = temp16 >> 8;
				frm += 1;

				/* fill ba reason (non half word aligned) */
				*(u_int8_t *)frm = ba->reason & 0xFF;
				frm += 1;

				*(u_int8_t *)frm = ba->reason >> 8;
				frm += 1;
				break;
			}
			default:
				break;
			}

			if(skb != NULL) {
				skb_trim(skb, frm - skb->data);
			}
			break;
		}
		case IEEE80211_ACTION_CAT_RM:
		{
			switch (action_data->action) {
			case IEEE80211_ACTION_R_MEASUREMENT_REQUEST:
			case IEEE80211_ACTION_R_MEASUREMENT_REPORT:
				ret = ieee80211_compile_action_measurement_11k(ni, action_data->params, action_data->action, &skb);
				break;
			case IEEE80211_ACTION_R_LINKMEASURE_REQUEST:
				ret = ieee80211_compile_action_link_measure_request(ni, action_data->params, &skb);
				break;
			case IEEE80211_ACTION_R_LINKMEASURE_REPORT:
				ret = ieee80211_compile_action_link_measure_report(ni, action_data->params, &skb);
				break;
			case IEEE80211_ACTION_R_NEIGHBOR_REQUEST:
				ret = ieee80211_compile_action_neighbor_report_request(ni, action_data->params, &skb);
				break;
			case IEEE80211_ACTION_R_NEIGHBOR_REPORT:
				ret = ieee80211_compile_action_neighbor_report_response(ni, action_data->params, &skb);
				break;
			default:
				ret = -1;
				break;
			}

			if (ret != 0)
				goto bad;

			break;
		}
#ifdef CONFIG_QVSP
		case IEEE80211_ACTION_CAT_VENDOR: {
			/* FIXME: Work out which vendor specific type to output. */
			struct ieee80211_qvsp_act *qvsp_a = (struct ieee80211_qvsp_act *)action_data->params;
			ret = ieee80211_compile_action_qvsp_frame(vap, qvsp_a, &skb);
			if (ret != 0)
				goto bad;
			break;
		}
#endif
		case IEEE80211_ACTION_CAT_SA_QUERY: {
			ret = ieee80211_compile_action_sa_query_frame(vap, action_data, &skb);
			if (ret != 0)
				goto bad;
			break;
		}
		case IEEE80211_ACTION_CAT_PUBLIC:
			if (action_data->action == IEEE80211_ACTION_PUB_20_40_COEX) {
				ret = ieee80211_compile_action_20_40_coex_frame(vap,
							action_data, &skb, ni);
				if (ret != 0)
					goto bad;
				break;
			}
		/* fall through if condition is not true */
		case IEEE80211_ACTION_CAT_QOS:
		case IEEE80211_ACTION_CAT_WNM: {
			struct action_frame_payload *frm_payload =
						(struct action_frame_payload *)action_data->params;

			skb = ieee80211_getmgtframe(&frm, frm_payload->length);

			if (skb == NULL) {
				senderr(ENOMEM, is_tx_nobuf);
			} else {
				memcpy(frm, frm_payload->data, frm_payload->length);
				frm += frm_payload->length;
			}
			break;
		}
		default:
			break;
		}
		break;
	}

	default:
		IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
			"invalid mgmt frame type %u", type);
		senderr(EINVAL, is_tx_unknownmgt);
		/* NOTREACHED */
	}

	if (skb != NULL) {
		if (timer) {
			if (ni != vap->iv_mgmt_retry_ni || type != vap->iv_mgmt_retry_type ||
					arg != vap->iv_mgmt_retry_arg) {
				vap->iv_mgmt_retry_ni = ni;
				vap->iv_mgmt_retry_type = type;
				vap->iv_mgmt_retry_arg = arg;
				vap->iv_mgmt_retry_cnt = 0;
			}
			expired_timer = (timer * HZ) + (random32() % HZ);
			mod_timer(&vap->iv_mgtsend, jiffies + expired_timer);
		}

		ieee80211_mgmt_output(ni, skb, type, ni->ni_macaddr);
	} else {
		ieee80211_free_node(ni);
	}

	return 0;

bad:
	ieee80211_free_node(ni);
	return ret;
#undef senderr
}

/*
 * Send PS-POLL from to bss. Should only be called when as STA.
 */
void
ieee80211_send_pspoll(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct sk_buff *skb;
	struct ieee80211_ctlframe_addr2 *wh;

	skb = dev_alloc_skb(sizeof(struct ieee80211_ctlframe_addr2));
	if (skb == NULL) {
		return;
	}
	ieee80211_ref_node(ni);

	wh = (struct ieee80211_ctlframe_addr2 *) skb_put(skb, sizeof(struct ieee80211_ctlframe_addr2));

	wh->i_aidordur = htole16(0xc000 | IEEE80211_NODE_AID(ni));
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
	IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
	wh->i_fc[0] = 0;
	wh->i_fc[1] = 0;
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
		IEEE80211_FC0_SUBTYPE_PS_POLL;
	if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap))
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
			"[%s] send ps poll frame on channel %u\n",
			ether_sprintf(ni->ni_macaddr),
			ieee80211_chan2ieee(ic, ic->ic_curchan));
	ic->ic_send_80211(ic, ni, skb, WME_AC_VO, 0);
}
EXPORT_SYMBOL(ieee80211_send_pspoll);


/*
 * Send DELBA management frame.
 */
void ieee80211_send_delba(struct ieee80211_node *ni, int tid, int tx, int reason)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_action_data act;
	struct ba_action_del ba_del;

	memset(&ba_del, 0, sizeof(ba_del));
	ba_del.tid = tid;
	ba_del.reason = reason;
	//	ba_del.initiator = (tx) ? 1 : 0;
	ba_del.initiator = !tx;

	memset(&act, 0, sizeof(act));
	act.cat = IEEE80211_ACTION_CAT_BA;
	act.action = IEEE80211_ACTION_BA_DELBA;
	act.params = (void *)&ba_del;

	ic->ic_send_mgmt(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&act);
}
EXPORT_SYMBOL(ieee80211_send_delba);

void ieee80211_rm_req_callback_success(void *ctx)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	if (ni->ni_dotk_meas_state.meas_state_sta.pending) {
		ni->ni_dotk_meas_state.meas_state_sta.pending = 0;
		wake_up_interruptible(&ni->ni_dotk_waitq);
	}
}

void ieee80211_rm_req_callback_fail(void *ctx, int32_t reason)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	if (ni->ni_dotk_meas_state.meas_state_sta.pending) {
		ni->ni_dotk_meas_state.meas_state_sta.status = -ETIMEDOUT;
		ni->ni_dotk_meas_state.meas_state_sta.pending = 0;
		wake_up_interruptible(&ni->ni_dotk_waitq);
	}
}

/*
 * Send radio measurement request - STA Statistics to associated STA. Should only be called when as AP.
 */
void
ieee80211_send_rm_req_stastats(struct ieee80211_node *ni, u_int32_t flags)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;
	ieee80211_11k_sub_element_head se_head;
	ieee80211_11k_sub_element *p_se;
	struct stastats_subele_vendor *vendor = NULL;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_STA;
	ctrl.u.sta_stats.duration_tu = 0;
	if (flags & RM_QTN_MEASURE_MASK) {
		ctrl.u.sta_stats.group_id = 0;
		SLIST_INIT(&se_head);
		p_se = (ieee80211_11k_sub_element *)kmalloc(sizeof(*p_se) + sizeof(struct stastats_subele_vendor), GFP_KERNEL);
		if (p_se != NULL) {
			p_se->sub_id = IEEE80211_ELEMID_VENDOR;
			vendor = (struct stastats_subele_vendor *)p_se->data;
			vendor->flags = flags;
			SLIST_INSERT_HEAD(&se_head, p_se, next);
		}
		ctrl.u.sta_stats.sub_item = &se_head;
	} else {
		ctrl.u.sta_stats.group_id = 221;
	}
	ctrl.expire = IEEE80211K_RM_MEASURE_STA_TIMEOUT;
	ctrl.fn_success = ieee80211_rm_req_callback_success;
	ctrl.fn_fail = ieee80211_rm_req_callback_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_stastats);

int32_t
ieee80211_send_rm_rep_stastats(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t group_id,
		u_int16_t duration_tu,
		void *sub_item)
{
	struct ieee80211_meas_report_ctrl mreport_ctrl;
	struct ieee80211_action_data action_data;

	mreport_ctrl.meas_type = IEEE80211_RM_MEASTYPE_STA;
	mreport_ctrl.token = token;
	mreport_ctrl.meas_token = meas_token;
	mreport_ctrl.report_mode = report_mode;
	mreport_ctrl.autonomous = 0;
	if (report_mode == 0) {
		mreport_ctrl.u.sta_stats.group_id = group_id;
		mreport_ctrl.u.sta_stats.duration_tu = duration_tu;
		mreport_ctrl.u.sta_stats.sub_item = sub_item;
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &mreport_ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_rep_stastats);

/*
 * Send radio measurement request - CCA. Should only be called when as AP.
 */
void
ieee80211_send_rm_req_cca(struct ieee80211_node *ni)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_QTN_CCA;
	ctrl.u.qtn_cca.duration_tu = 1;
	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_cca);

void
ieee80211_send_rm_req_stastats_all(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	/* Fixed structure STA statistics */
	u_int32_t flags = BIT(RM_QTN_MAX + 1) - 1;

	IEEE80211_NODE_LOCK_BH(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;
		if (ni->ni_associd == 0)
			continue;
		ieee80211_send_rm_req_stastats(ni, flags);
	}
	IEEE80211_NODE_UNLOCK_BH(nt);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_stastats_all);

void
ieee80211_send_rm_req_chan_load(struct ieee80211_node *ni,
				u_int8_t channel,
				u_int16_t duration_ms,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_CH_LOAD;
	ctrl.u.chan_load.channel = (channel == 0 ? ni->ni_ic->ic_curchan->ic_ieee : channel);
	ctrl.u.chan_load.duration_ms = duration_ms;

	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_chan_load);

void
ieee80211_send_rm_rep_chan_load(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t op_class,
		u_int8_t channel,
		u_int16_t duration_tu,
		u_int8_t channel_load)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_CH_LOAD;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.chan_load.op_class = op_class;
		ctrl.u.chan_load.channel = channel;
		ctrl.u.chan_load.duration_tu = duration_tu;
		ctrl.u.chan_load.channel_load = channel_load;
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_rep_chan_load);

void
ieee80211_send_rm_req_noise_his(struct ieee80211_node *ni,
				u_int8_t channel,
				u_int16_t duration_ms,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_NOISE;
	ctrl.u.noise_his.channel = (channel == 0 ? ni->ni_ic->ic_curchan->ic_ieee : channel);
	ctrl.u.noise_his.duration_ms = duration_ms;

	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_noise_his);

void
ieee80211_send_rm_rep_noise_his(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t op_class,
		u_int8_t channel,
		u_int16_t duration_tu,
		u_int8_t antenna_id,
		u_int8_t anpi,
		u_int8_t *ipi)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_NOISE;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.noise_his.op_class = op_class;
		ctrl.u.noise_his.channel = channel;
		ctrl.u.noise_his.duration_tu = duration_tu;
		ctrl.u.noise_his.antenna_id = antenna_id;
		ctrl.u.noise_his.anpi = anpi;
		if (ipi != NULL)
			memcpy(ctrl.u.noise_his.ipi, ipi, sizeof(ctrl.u.noise_his.ipi));
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_rep_noise_his);

void
ieee80211_send_rm_req_beacon(struct ieee80211_node *ni,
				u_int8_t op_class,
				u_int8_t channel,
				u_int16_t duration_ms,
				u_int8_t mode,
				u_int8_t *bssid,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_BEACON;
	ctrl.u.beacon.op_class = op_class;
	ctrl.u.beacon.channel = (channel == 0 ? ni->ni_ic->ic_curchan->ic_ieee : channel);
	ctrl.u.beacon.duration_ms = duration_ms;
	ctrl.u.beacon.mode = mode;
	if (bssid != NULL)
		memcpy(ctrl.u.beacon.bssid, bssid, IEEE80211_ADDR_LEN);
	else
		memset(ctrl.u.beacon.bssid, 0xFF, IEEE80211_ADDR_LEN);

	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_beacon);

void
ieee80211_send_rm_rep_beacon(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t op_class,
		u_int8_t channel,
		u_int16_t duration_tu,
		u_int8_t reported_frame_info,
		u_int8_t rcpi,
		u_int8_t rsni,
		u_int8_t *bssid,
		u_int8_t antenna_id,
		u_int8_t *parent_tsf)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_BEACON;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.beacon.op_class = op_class;
		ctrl.u.beacon.channel = channel;
		ctrl.u.beacon.duration_tu = duration_tu;
		ctrl.u.beacon.reported_frame_info = reported_frame_info;
		ctrl.u.beacon.rcpi = rcpi;
		ctrl.u.beacon.rsni = rsni;
		if (bssid != NULL)
			memcpy(ctrl.u.beacon.bssid, bssid, IEEE80211_ADDR_LEN);
		ctrl.u.beacon.antenna_id = antenna_id;
		if (parent_tsf != NULL)
			memcpy(ctrl.u.beacon.parent_tsf, parent_tsf, sizeof(ctrl.u.beacon.parent_tsf));
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_rep_beacon);

void
ieee80211_send_rm_req_frame(struct ieee80211_node *ni,
				u_int8_t op_class,
				u_int8_t channel,
				u_int16_t duration_ms,
				u_int8_t type,
				u_int8_t *mac_address,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_FRAME;
	ctrl.u.frame.op_class = op_class;
	ctrl.u.frame.channel = (channel == 0 ? ni->ni_ic->ic_curchan->ic_ieee : channel);
	ctrl.u.frame.duration_ms = duration_ms;
	ctrl.u.frame.type = type;
	if (mac_address != NULL)
		memcpy(ctrl.u.frame.mac_address, mac_address, IEEE80211_ADDR_LEN);
	else
		memset(ctrl.u.frame.mac_address, 0xFF, IEEE80211_ADDR_LEN);

	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_frame);

void
ieee80211_send_rm_rep_frame(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t op_class,
		u_int8_t channel,
		u_int16_t duration_tu,
		void *sub_ele)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_FRAME;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.frame.op_class = op_class;
		ctrl.u.frame.channel = channel;
		ctrl.u.frame.duration_tu = duration_tu;
		ctrl.u.frame.sub_item = sub_ele;
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_rep_frame);

void
ieee80211_send_rm_req_tran_stream_cat(struct ieee80211_node *ni,
				u_int16_t duration_ms,
				u_int8_t *peer_sta,
				u_int8_t tid,
				u_int8_t bin0,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;
	u_int8_t null_mac[IEEE80211_ADDR_LEN] = {0};

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_CATEGORY;
	ctrl.u.tran_stream_cat.duration_ms = duration_ms;
	if (peer_sta != NULL && memcmp(null_mac, ctrl.u.tran_stream_cat.peer_sta, IEEE80211_ADDR_LEN) != 0)
		memcpy(ctrl.u.tran_stream_cat.peer_sta, peer_sta, IEEE80211_ADDR_LEN);
	else
		memcpy(ctrl.u.tran_stream_cat.peer_sta, ni->ni_macaddr, IEEE80211_ADDR_LEN);
	ctrl.u.tran_stream_cat.tid = tid;
	ctrl.u.tran_stream_cat.bin0 = bin0;

	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_tran_stream_cat);

void
ieee80211_send_rm_req_multicast_diag(struct ieee80211_node *ni,
				u_int16_t duration_ms,
				u_int8_t *group_mac,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_MUL_DIAG;
	ctrl.u.multicast_diag.duration_ms = duration_ms;
	if (group_mac != NULL)
		memcpy(ctrl.u.multicast_diag.group_mac, group_mac, IEEE80211_ADDR_LEN);
	else
		memset(ctrl.u.multicast_diag.group_mac, 0, IEEE80211_ADDR_LEN);

	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_req_multicast_diag);

void
ieee80211_send_rm_rep_multicast_diag(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int16_t duration_tu,
		u_int8_t *group_mac,
		u_int8_t reason,
		u_int32_t mul_rec_msdu_cnt,
		u_int16_t first_seq_num,
		u_int16_t last_seq_num,
		u_int16_t mul_rate)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_RM_MEASTYPE_MUL_DIAG;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.multicast_diag.duration_tu = duration_tu;
		if (group_mac != NULL)
			memcpy(ctrl.u.multicast_diag.group_mac, group_mac, IEEE80211_ADDR_LEN);
		else
			memset(ctrl.u.multicast_diag.group_mac, 0, IEEE80211_ADDR_LEN);
		ctrl.u.multicast_diag.reason = reason;
		ctrl.u.multicast_diag.mul_rec_msdu_cnt = mul_rec_msdu_cnt;
		ctrl.u.multicast_diag.first_seq_num = first_seq_num;
		ctrl.u.multicast_diag.last_seq_num = last_seq_num;
		ctrl.u.multicast_diag.mul_rate = mul_rate;
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_rm_rep_multicast_diag);

int32_t ieee80211_send_meas_request_basic(struct ieee80211_node *ni,
		u_int8_t channel,
		u_int16_t tsf_offset,
		u_int16_t duration,
		unsigned long expire,
		void *fn_success,
		void *fn_fail)
{
	struct ieee80211com *ic = ni->ni_vap->iv_ic;
	u_int64_t tsf;
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	ctrl.meas_type = IEEE80211_CCA_MEASTYPE_BASIC;
	if (channel == 0)
		ctrl.u.basic.channel = ic->ic_curchan->ic_ieee;
	else
		ctrl.u.basic.channel = channel;
	ic->ic_get_tsf(&tsf);
	if (tsf_offset == 0)
		ctrl.u.basic.start_tsf = 0;
	else
		ctrl.u.basic.start_tsf = tsf + tsf_offset * 1000;
	ctrl.u.basic.duration_ms = duration;
	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_S_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_meas_request_basic);

int32_t ieee80211_send_meas_request_cca(struct ieee80211_node *ni,
		u_int8_t channel,
		u_int16_t tsf_offset,
		u_int16_t duration,
		unsigned long expire,
		void *fn_success,
		void *fn_fail)
{
	struct ieee80211com *ic = ni->ni_vap->iv_ic;
	u_int64_t tsf;
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	ctrl.meas_type = IEEE80211_CCA_MEASTYPE_CCA;
	if (channel == 0)
		ctrl.u.cca.channel = ic->ic_curchan->ic_ieee;
	else
		ctrl.u.cca.channel = channel;
	ic->ic_get_tsf(&tsf);
	if (tsf_offset == 0)
		ctrl.u.cca.start_tsf = 0;
	else
		ctrl.u.cca.start_tsf = tsf + tsf_offset * 1000;
	ctrl.u.cca.duration_ms = duration;
	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_S_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_meas_request_cca);

int32_t ieee80211_send_meas_request_rpi(struct ieee80211_node *ni,
		u_int8_t channel,
		u_int16_t tsf_offset,
		u_int16_t duration,
		unsigned long expire,
		void *fn_success,
		void *fn_fail)
{
	struct ieee80211com *ic = ni->ni_vap->iv_ic;
	u_int64_t tsf;
	struct ieee80211_meas_request_ctrl ctrl;
	struct ieee80211_action_data action_data;

	ctrl.meas_type = IEEE80211_CCA_MEASTYPE_RPI;
	if (channel == 0)
		ctrl.u.rpi.channel = ic->ic_curchan->ic_ieee;
	else
		ctrl.u.rpi.channel = channel;
	ic->ic_get_tsf(&tsf);
	if (tsf_offset == 0)
		ctrl.u.rpi.start_tsf = 0;
	else
		ctrl.u.rpi.start_tsf = tsf + tsf_offset * 1000;
	ctrl.u.rpi.duration_ms = duration;
	ctrl.expire = expire;
	ctrl.fn_success = (ppq_callback_success)fn_success;
	ctrl.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_S_MEASUREMENT_REQUEST;
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_meas_request_rpi);

int32_t ieee80211_send_meas_report_basic(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t channel,
		u_int64_t start_tsf,
		u_int16_t duration,
		u_int8_t basic_report)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_CCA_MEASTYPE_BASIC;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.basic.channel = channel;
		ctrl.u.basic.start_tsf = start_tsf;
		ctrl.u.basic.duration_tu = duration;
		ctrl.u.basic.basic_report = basic_report;
	}

	action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_meas_report_basic);

int32_t ieee80211_send_meas_report_cca(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t channel,
		u_int64_t start_tsf,
		u_int16_t duration,
		u_int8_t cca_report)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_CCA_MEASTYPE_CCA;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.cca.channel = channel;
		ctrl.u.cca.start_tsf = start_tsf;
		ctrl.u.cca.duration_tu = duration;
		ctrl.u.cca.cca_report = cca_report;
	}

	action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_meas_report_cca);

int32_t ieee80211_send_meas_report_rpi(struct ieee80211_node *ni,
		u_int8_t report_mode,
		u_int8_t token,
		u_int8_t meas_token,
		u_int8_t channel,
		u_int64_t start_tsf,
		u_int16_t duration,
		u_int8_t *rpi_report)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = IEEE80211_CCA_MEASTYPE_RPI;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;
	if (report_mode == 0) {
		ctrl.u.rpi.channel = channel;
		ctrl.u.rpi.start_tsf = start_tsf;
		ctrl.u.rpi.duration_tu = duration;
		memcpy(ctrl.u.rpi.rpi_report, rpi_report, sizeof(ctrl.u.rpi.rpi_report));
	}

	action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_R_MEASUREMENT_REPORT;
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_meas_report_rpi);

void ieee80211_send_link_measure_request(struct ieee80211_node *ni,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_link_measure_request request;
	struct ieee80211_action_data action_data;

	memset(&request, 0, sizeof(request));
	request.ppq.expire = expire;
	request.ppq.fn_success = (ppq_callback_success)fn_success;
	request.ppq.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_LINKMEASURE_REQUEST;
	action_data.params = &request;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_link_measure_request);

void ieee80211_send_neighbor_report_request(struct ieee80211_node *ni,
				unsigned long expire,
				void *fn_success,
				void *fn_fail)
{
	struct ieee80211_neighbor_report_request request;
	struct ieee80211_action_data action_data;

	memset(&request, 0, sizeof(request));
	request.ppq.expire = expire;
	request.ppq.fn_success = (ppq_callback_success)fn_success;
	request.ppq.fn_fail = (ppq_callback_fail)fn_fail;

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_NEIGHBOR_REQUEST;
	action_data.params = &request;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}
EXPORT_SYMBOL(ieee80211_send_neighbor_report_request);

void ieee80211_send_neighbor_report_response(struct ieee80211_node *ni,
					u_int8_t token,
					u_int8_t bss_num,
					void *table)
{
	struct ieee80211_neighbor_report_response response;
	struct ieee80211_action_data action_data;
	struct ieee80211_neighbor_report_request_item** bss_table;
	u_int8_t i;

	memset(&response, 0, sizeof(response));
	response.token = token;
	response.bss_num = bss_num;
	bss_table = (struct ieee80211_neighbor_report_request_item**)table;
	for (i = 0; i < bss_num; i++) {
		response.neighbor_report_ptr[i] = bss_table[i];
	}

	action_data.cat = IEEE80211_ACTION_CAT_RM;
	action_data.action = IEEE80211_ACTION_R_NEIGHBOR_REPORT;
	action_data.params = &response;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}

void
ieee80211_send_vht_opmode_action(struct ieee80211vap *vap,
					struct ieee80211_node *ni,
					uint8_t bw, uint8_t rx_nss)
{
	struct sk_buff *skb;
	int frm_len;
	u_int8_t *frm;

	frm_len = IEEE80211_NCW_ACT_LEN;

	skb = ieee80211_getmgtframe(&frm, frm_len);
	if (skb == NULL) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
			"%s: cannot get buf; size %u", __func__, frm_len);
		vap->iv_stats.is_tx_nobuf++;
		return;
	}

	*frm++ = IEEE80211_ACTION_CAT_VHT;
	*frm++ = IEEE80211_ACTION_VHT_OPMODE_NOTIFICATION;
	*frm++ = bw | (rx_nss << 4);

	ieee80211_ref_node(ni);
	ieee80211_mgmt_output(ni, skb, IEEE80211_FC0_SUBTYPE_ACTION,
					ni->ni_macaddr);
}

/* sending Notify Channel Width Action
 * if the ni is NULL, then it sends it as broadcast
 * otherwise, unicast it to the targeted node
 *     width == 0:  use HT20
 *     width != 0   use any channel width the node support
 *
 * Notify Channel Width Action frame fromat:
 *     |category: 1byte, value(7): CAT_HT | action: 1byte, value(0): NCW | Channel_width: 1byte, value: 0/HT20, or 1/HT40 |
 */
void
ieee80211_send_notify_chan_width_action(struct ieee80211vap *vap,
					struct ieee80211_node *ni,
					u_int32_t width)
{
	// struct ieee80211com *ic = vap->iv_ic;
	struct sk_buff *skb;
	int frm_len;
	u_int8_t *frm;
	int is_bcst = (ni == NULL) ? 1 : 0;

	frm_len = IEEE80211_NCW_ACT_LEN;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
			  "%s: Sending Notify Chan Width %s action to %s\n",
			  __func__,
			  (width) ? "HT40/20" : "HT20",
			  (is_bcst) ? "bcast" : ether_sprintf(ni->ni_macaddr));

	skb = ieee80211_getmgtframe(&frm, frm_len);
	if (skb == NULL) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
			"%s: cannot get buf; size %u", __func__, frm_len);
		vap->iv_stats.is_tx_nobuf++;
		return;
	}

	*frm++ = IEEE80211_ACTION_CAT_HT;            /* Category */
	*frm++ = IEEE80211_ACTION_HT_TXCHWIDTH;      /* notify channel width action */
	if (width) {
		*frm++ = IEEE80211_CWM_WIDTH40;
	} else {
		*frm++ = IEEE80211_CWM_WIDTH20;
	}

	if (is_bcst) {
		ieee80211_ref_node(vap->iv_bss);
		ieee80211_mgmt_output(vap->iv_bss, skb, IEEE80211_FC0_SUBTYPE_ACTION,
					vap->iv_dev->broadcast);
	} else {
		ieee80211_ref_node(ni);
		ieee80211_mgmt_output(ni, skb, IEEE80211_FC0_SUBTYPE_ACTION,
					ni->ni_macaddr);
	}
}
EXPORT_SYMBOL(ieee80211_send_notify_chan_width_action);

/* sending 11ac(VHT) Group ID mgmt Action
 * node group membership and the positions are stored inside the node struct
 * VHT group ID mgmt action frame format:
 * |category: 1byte, value(21): CAT_VHT | action: 1byte, value(1): GRP ID MGMT | membership status array: 8 bytes | user position array: 16 bytes|
 */
void
ieee80211_send_vht_grp_id_mgmt_action(struct ieee80211vap *vap,
				      struct ieee80211_node *ni)
{
	struct sk_buff *skb;
	int frm_len;
	u_int8_t *frm;

	frm_len = IEEE80211_MU_GRP_ID_ACT_LEN;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_VHT,
			  "%s: Sending MU GRP ID action to %s\n",
			  __func__, ether_sprintf(ni->ni_macaddr));

	skb = ieee80211_getmgtframe(&frm, frm_len);
	if (skb == NULL) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
			"%s: cannot get buf; size %u", __func__, frm_len);
		vap->iv_stats.is_tx_nobuf++;
		return;
	}

	/* Category VHT */
	*frm++ = IEEE80211_ACTION_CAT_VHT;

	/* MU GRP ID action */
	*frm++ = IEEE80211_ACTION_VHT_MU_GRP_ID;

	memcpy(frm, &ni->ni_mu_grp, sizeof(ni->ni_mu_grp));
	frm += sizeof(ni->ni_mu_grp);

	ieee80211_ref_node(ni);
	ieee80211_mgmt_output(ni, skb, IEEE80211_FC0_SUBTYPE_ACTION,
			      ni->ni_macaddr);

}

EXPORT_SYMBOL(ieee80211_send_vht_grp_id_mgmt_action);

int
ieee80211_send_20_40_bss_coex(struct ieee80211vap *vap)
{

	struct ieee80211_action_data action_data;
	struct ieee80211_node *ni = vap->iv_bss;
	uint8_t coex = vap->iv_coex;

	if ((!ni) || (!IEEE80211_AID(ni->ni_associd)))
		return -EFAULT;

	action_data.cat = IEEE80211_ACTION_CAT_PUBLIC;
	action_data.action = IEEE80211_ACTION_PUB_20_40_COEX;
	action_data.params = &coex;

	IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
	return 0;
}

void ieee80211_send_sa_query (struct ieee80211_node *ni, u_int8_t action,
					u_int16_t tid)
{
	struct ieee80211_action_data action_data;

	if(RSN_IS_MFP(ni->ni_rsn.rsn_caps)) {
		action_data.cat = IEEE80211_ACTION_CAT_SA_QUERY;
		action_data.action = action;
		action_data.params = &tid;
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
		ni->ni_sa_query_timeout = jiffies;
	}
}

/**
  Copyright (c) 2015 Quantenna Communications Inc
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_beacon_desc.h"
#include "qdrv/qdrv_vap.h"

/* This fucntion allocates a beacon ie and associate the beacon ie buffer. */
static void ieee80211_beacon_associate_ie(struct beacon_shared_ie_t *ie, uint8_t *frm, uint8_t size)
{
	if (ie == NULL || frm == NULL)
		return;
	ie->size = size;
	/* convert to bus address for MuC access only */
	ie->buf = plat_kernel_addr_to_dma(NULL, frm);

	/* to avoid memory remap, keep lhost original buffer address for debug purpose */
	ie->lhost_buf = frm;
	ie->next = NULL;
	ie->next_muc_addr = 0;
}

static struct beacon_shared_ie_t *ieee80211_beacon_alloc_ie(struct ieee80211_beacon_param_t *param)
{
	struct beacon_shared_ie_t *ie;

	if (param == NULL)
		return NULL;

	if ((param->curr + sizeof(*ie) - (uint32_t)param->buf) >
			param->size) {
		panic("%s: allocated space %d is not enough for adding more ie descriptors\n",
				__func__, param->size);
	}
	ie = (struct beacon_shared_ie_t *)param->curr;
	param->curr += sizeof(*ie);
	return ie;
}

/*
 * This function allocates an ie descriptor, construct it with ie payload buffer and
 * queue it to the list
 */
static void ieee80211_add_beacon_ie_desc(struct ieee80211_beacon_param_t *param, uint8_t *frm,
		uint16_t size)
{
	struct beacon_shared_ie_t *new_ie;

	if (param == NULL || frm == NULL || size == 0)
		return;
	new_ie = ieee80211_beacon_alloc_ie(param);
	if (new_ie == NULL)
		return;
	ieee80211_beacon_associate_ie(new_ie, frm, size);

	if (param->head == NULL) {
		param->head = new_ie;
		param->tail = new_ie;
	} else {
		param->tail->next = new_ie;
		param->tail->next_muc_addr = plat_kernel_addr_to_dma(NULL, new_ie);
		param->tail = new_ie;
	}
}

/* This function is same as above but always append the descriptor as header. */
static void ieee80211_add_beacon_ie_desc_head(struct ieee80211_beacon_param_t *param, uint8_t *frm,
		uint16_t size)
{
	struct beacon_shared_ie_t *new_ie, *temp;

	if (param == NULL || frm == NULL || size == 0)
		return;
	new_ie = ieee80211_beacon_alloc_ie(param);
	if (new_ie == NULL)
		return;
	ieee80211_beacon_associate_ie(new_ie, frm, size);

	temp = param->head;
	new_ie->next = temp;
	new_ie->next_muc_addr = plat_kernel_addr_to_dma(NULL, temp);
	param->head = new_ie;
	/* if header wasn't there, then we need setup tail as same as header */
	if (temp == NULL)
		param->tail = new_ie;
}

int ieee80211_beacon_create_param(struct ieee80211vap *vap)
{
	if (vap == NULL)
		return -EINVAL;
	vap->param = kmalloc(sizeof(struct ieee80211_beacon_param_t), GFP_KERNEL);
	if (vap->param == NULL) {
		printk("Error, %s failed to allocate beacon param %ld bytes\n", __func__,
				sizeof(struct ieee80211_beacon_param_t));
		return -ENOMEM;
	}

	vap->param->size = BEACON_PARAM_SIZE;
	vap->param->curr = (uint32_t)&vap->param->buf[0];
	vap->param->head = NULL;
	vap->param->tail = NULL;
	return 0;
}
EXPORT_SYMBOL(ieee80211_beacon_create_param);

void ieee80211_beacon_flush_param(struct ieee80211_beacon_param_t *param)
{
	if (param == NULL)
		return;
	/* Flush the liner memory so that MuC can rebuild the beacon ie via link list */
	flush_dcache_range((uint32_t)param,
			(uint32_t)param + sizeof(struct ieee80211_beacon_param_t));
	return;
}
EXPORT_SYMBOL(ieee80211_beacon_flush_param);

/* This free the descriptor memory and reset the parameters */
void ieee80211_beacon_destroy_param(struct ieee80211vap *vap)
{
	kfree(vap->param);
	vap->param = NULL;
}
EXPORT_SYMBOL(ieee80211_beacon_destroy_param);

uint8_t *ieee80211_add_beacon_desc_header(struct ieee80211_node *ni, uint8_t *frm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_beacon_param_t *param = vap->param;
	uint8_t *post_frm;

	post_frm = ieee80211_add_beacon_header(ni, frm);
	/*
	 * Always queue the beacon frame header on the list header position so that MuC
	 * can compose the frame easily.
	 */
	ieee80211_add_beacon_ie_desc_head(param, frm, post_frm - frm);
	return post_frm;
}
EXPORT_SYMBOL(ieee80211_add_beacon_desc_header);

uint8_t *ieee80211_add_beacon_desc_mandatory_fields(struct ieee80211_node *ni, uint8_t *frm,
		struct ieee80211_beacon_offsets *bo)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_beacon_param_t *param = vap->param;
	uint8_t *post_frm;

	post_frm = ieee80211_add_mandatory_field(ni, frm, bo);
	ieee80211_add_beacon_ie_desc(param, frm, post_frm - frm);
	return post_frm;
}
EXPORT_SYMBOL(ieee80211_add_beacon_desc_mandatory_fields);

/*
 * This function provides a common interface to add ie field, ext_ie_id is extended to
 * support same IE id in different descriptors.
 */
uint8_t *ieee80211_add_beacon_desc_ie(struct ieee80211_node *ni, uint16_t ext_ie_id, uint8_t *frm)
{
	uint8_t *pre_frm = frm, *post_frm = frm;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_beacon_param_t *param = vap->param;
	struct ieee80211_rateset *rs = &ic->ic_sup_rates[ic->ic_curmode];
	struct ieee80211_tim_ie *tie;
	int ap_pure_tkip = 0;

	if (vap->iv_bss && !vap->allow_tkip_for_vht)
		ap_pure_tkip = (vap->iv_bss->ni_rsn.rsn_ucastcipherset == IEEE80211_C_TKIP);


	if (param == NULL)
		return 0;

	switch (ext_ie_id) {
	case IEEE80211_ELEMID_RATES:
		/* supported rates */
		frm = ieee80211_add_rates(frm, rs);
		break;
	case IEEE80211_ELEMID_DSPARMS:
		/* XXX: better way to check this? */
		/* XXX: how about DS ? */
		if (!IEEE80211_IS_CHAN_FHSS(ic->ic_bsschan)) {
			*frm++ = IEEE80211_ELEMID_DSPARMS;
			*frm++ = 1;
			*frm++ = ieee80211_chan2ieee(ic, ic->ic_bsschan);
		}
		break;
	case IEEE80211_ELEMID_IBSSPARMS:
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0;
		*frm++ = 0;		/* TODO: ATIM window */
		break;
	case IEEE80211_ELEMID_TIM:
			/* IBSS/TIM */
		tie = (struct ieee80211_tim_ie *) frm;

		tie->tim_ie = IEEE80211_ELEMID_TIM;
		/* tim length */
		tie->tim_len = sizeof(*tie) - sizeof(tie->tim_len) - sizeof(tie->tim_ie);
		tie->tim_count = 0;	/* DTIM count */
		tie->tim_period = vap->iv_dtim_period;	/* DTIM period */
		tie->tim_bitctl = 0;	/* bitmap control */
		/* Partial virtual bitmap */
		memset(&tie->tim_bitmap[0], 0, sizeof(tie->tim_bitmap));
		frm += sizeof(struct ieee80211_tim_ie);
		break;
	case IEEE80211_ELEMID_COUNTRY:
		frm = ieee80211_add_country(frm, ic);
		break;
	case IEEE80211_ELEMID_20_40_BSS_COEX:
		frm = ieee80211_add_20_40_bss_coex_ie(frm, vap->iv_coex);
		break;
	case IEEE80211_ELEMID_OBSS_SCAN:
		frm = ieee80211_add_obss_scan_ie(frm, &ic->ic_obss_ie);
		break;
	case IEEE80211_ELEMID_BSS_LOAD:
		frm = ieee80211_add_bss_load(frm, vap);
		break;
	case IEEE80211_ELEMID_PWRCNSTR:
		*frm++ = IEEE80211_ELEMID_PWRCNSTR;
		*frm++ = 1;
		*frm++ = IEEE80211_PWRCONSTRAINT_VAL(ic);
		break;
	case IEEE80211_ELEMID_VHTXMTPWRENVLP:
		frm = ieee80211_add_vhttxpwr_envelope(frm, ic);
		break;
	case IEEE80211_ELEMID_TPCREP:
		*frm++ = IEEE80211_ELEMID_TPCREP;
		*frm++ = 2;
		*frm++ = 0;	/* tx power would be updated in macfw */
		*frm++ = 0;	/* link margin is always 0 in beacon*/
		break;
	case IEEE80211_ELEMID_CHANSWITCHANN:
		frm = ieee80211_add_csa(frm, ic->ic_csa_mode,
				ic->ic_csa_chan->ic_ieee, ic->ic_csa_count);
		break;
	case IEEE80211_ELEMID_ERP:
		frm = ieee80211_add_erp(frm, ic);
		break;
	case IEEE80211_ELEMID_HTCAP:
		frm = ieee80211_add_htcap(ni, frm, &ic->ic_htcap, IEEE80211_FC0_SUBTYPE_BEACON);
		break;
	case IEEE80211_ELEMID_HTINFO:
		frm = ieee80211_add_htinfo(ni, frm, &ic->ic_htinfo);
		break;
	case IEEE80211_ELEMID_SEC_CHAN_OFF:
		ieee80211_add_sec_chan_off(&frm, ic, ic->ic_csa_chan->ic_ieee);
		break;
	case IEEE80211_ELEMID_XRATES:
		frm = ieee80211_add_xrates(frm, rs);
		break;
	case IEEE80211_ELEMID_VENDOR_WME:
		if (vap->iv_flags & IEEE80211_F_WME) {
			struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
			frm = ieee80211_add_wme_param(frm, wme, IEEE80211_VAP_UAPSD_ENABLED(vap), 0);
		}
		break;
	case IEEE80211_ELEMID_VENDOR_WPA:
		if (!vap->iv_osen && (vap->iv_flags & IEEE80211_F_WPA))
			frm = ieee80211_add_wpa(frm, vap);
		break;
	case IEEE80211_ELEMID_VHTCAP:
		if (IS_IEEE80211_VHT_ENABLED(ic) && !ap_pure_tkip) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
					"%s: VHT is Enabled in network\n", __func__);
			/* VHT capability */
			frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap, IEEE80211_FC0_SUBTYPE_BEACON);

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

			if (SIGMA_TESTBED_SUPPORT &&
					ic->ic_vht_opmode_notif != IEEE80211_VHT_OPMODE_NOTIF_DEFAULT) {
				frm = ieee80211_add_vhtop_notif(ni, frm, ic, 0);
			}
		} else if (IS_IEEE80211_11NG_VHT_ENABLED(ic) && !ap_pure_tkip) {
			/* QTN 2.4G band VHT IE */
			frm = ieee80211_add_vhtcap(ni, frm, &ic->ic_vhtcap_24g, IEEE80211_FC0_SUBTYPE_BEACON);
			frm = ieee80211_add_vhtop(ni, frm, &ic->ic_vhtop_24g);
		} else {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
					"%s: VHT is disabled in network\n", __func__);
		}
		break;
	case IEEE80211_ELEMID_WBWCHANSWITCH:
		frm = ieee80211_add_wband_chanswitch(frm, ic);
		break;
	case IEEE80211_ELEMID_CHANSWITCHWRP:
		frm = ieee80211_add_chansw_wrap(frm, ic);
		break;
	case IEEE80211_ELEMID_VENDOR_ATH:
		frm = ieee80211_add_athAdvCap(frm, vap->iv_bss->ni_ath_flags,
				vap->iv_bss->ni_ath_defkeyindex);
		break;
	case IEEE80211_ELEMID_VENDOR_QTN:
		frm = ieee80211_add_qtn_ie(frm, ic,
				(vap->iv_flags_ext & IEEE80211_FEXT_WDS ? IEEE80211_QTN_BRIDGEMODE : 0),
				(vap->iv_flags_ext & IEEE80211_FEXT_WDS ?
					(IEEE80211_QTN_BRIDGEMODE | IEEE80211_QTN_LNCB) : 0),
				0, 0, 0);
		break;

	case IEEE80211_ELEMID_VENDOR_EXT_ROLE:
		frm = ieee80211_add_qtn_extender_role_ie(frm, ic->ic_extender_role);
		break;
	case IEEE80211_ELEMID_VENDOR_EXT_BSSID:
		frm = ieee80211_add_qtn_extender_bssid_ie(vap, frm);
		break;
	case IEEE80211_ELEMID_VENDOR_EXT_STATE:
		frm = ieee80211_add_qtn_extender_state_ie(frm, !!ic->ic_ocac.ocac_cfg.ocac_enable);
		break;
	case IEEE80211_ELEMID_VENDOR_QTN_WME:
		if (ic->ic_wme.wme_throt_add_qwme_ie
				&& (vap->iv_flags & IEEE80211_F_WME))
			frm = ieee80211_add_qtn_wme_param(vap, frm);
		break;
	case IEEE80211_ELEMID_VENDOR_EPIGRAM:
		if (ic->ic_vendor_fix & VENDOR_FIX_BRCM_DHCP)
			frm = ieee80211_add_epigram_ie(frm);
		break;
	case IEEE80211_ELEMID_VENDOR_APP:
		memcpy(frm, vap->app_ie[IEEE80211_APPIE_FRAME_BEACON].ie,
			vap->app_ie[IEEE80211_APPIE_FRAME_BEACON].length);
		frm += vap->app_ie[IEEE80211_APPIE_FRAME_BEACON].length;
		break;
	case IEEE80211_ELEMID_MEASREQ:
		{
#if defined(CONFIG_QTN_80211K_SUPPORT)
		size_t chan_cca_ie_bytes = sizeof(struct ieee80211_ie_measreq) + sizeof(struct ieee80211_ie_measure_comm);
		struct ieee80211_ie_measure_comm *ie_comm = (struct ieee80211_ie_measure_comm *)frm;
		struct ieee80211_ie_measreq *ie = (struct ieee80211_ie_measreq *) ie_comm->data;

		ie_comm->id = IEEE80211_ELEMID_MEASREQ;
		ie_comm->len = chan_cca_ie_bytes - 2;
		ie_comm->token = ic->ic_cca_token;
		ie_comm->mode = IEEE80211_CCA_REQMODE_ENABLE | IEEE80211_CCA_REQMODE_REQUEST;
		ie_comm->type = IEEE80211_CCA_MEASTYPE_CCA;
#else
		size_t chan_cca_ie_bytes = sizeof(struct ieee80211_ie_measreq);
		struct ieee80211_ie_measreq *ie = (struct ieee80211_ie_measreq *) frm;

		ie->id = IEEE80211_ELEMID_MEASREQ;
		ie->len = sizeof(struct ieee80211_ie_measreq) - 2;
		ie->meas_token = ic->ic_cca_token;
		ie->req_mode = IEEE80211_CCA_REQMODE_ENABLE | IEEE80211_CCA_REQMODE_REQUEST;
		ie->meas_type = IEEE80211_CCA_MEASTYPE_CCA;
#endif
		ie->chan_num = ic->ic_cca_chan;
		ie->start_tsf = htonll(ic->ic_cca_start_tsf);
		ie->duration_tu = htons(ic->ic_cca_duration_tu);

		frm += chan_cca_ie_bytes;
		break;
		}
	default:
		break;
	}
	post_frm = frm;
	if (post_frm != pre_frm)
		ieee80211_add_beacon_ie_desc(param, pre_frm, post_frm - pre_frm);

	return post_frm;
}
EXPORT_SYMBOL(ieee80211_add_beacon_desc_ie);

void ieee80211_dump_beacon_desc_ie(struct ieee80211_beacon_param_t *param)
{
	struct beacon_shared_ie_t  *desc;
	int index = 0;

	desc = param->head;
	while (desc) {
		printk("LHOST Dump Beacon IE %d\n", ++index);
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_ADDRESS,
			16, 1, desc->lhost_buf, desc->size, false);
		desc = desc->next;
	}

}
EXPORT_SYMBOL(ieee80211_dump_beacon_desc_ie);

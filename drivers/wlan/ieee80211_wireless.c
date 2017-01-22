/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: ieee80211_wireless.c 2614 2007-07-26 12:58:47Z mrenzmann $
 */

/*
 * Wireless extensions support for 802.11 common code.
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif

#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>		/* XXX for ARPHRD_ETHER */
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/watch64.h>

#include <qtn/muc_phy_stats.h>
#include <qtn/shared_defs.h>
#include <qtn/skb_recycle.h>
#include <qtn/lhost_muc_comm.h>
#include <qtn/qtn_global.h>

#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/pm_qos_params.h>

#include <asm/uaccess.h>
#include "net80211/if_media.h"

#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_linux.h"
#include "net80211/_ieee80211.h"
#include "qdrv_sch_const.h"
#include "net80211/ieee80211_tpc.h"
#include "net80211/ieee80211_tdls.h"
#include "qtn_logging.h"

#include <qdrv/qdrv_vap.h>
#include <qtn/qtn_debug.h>
#include <qtn/shared_params.h>
#include <qtn/qtn_bb_mutex.h>
#include <qtn/qtn_vlan.h>
#include <linux/net/bridge/br_public.h>

#include "qtn/wlan_ioctl.h"

#include "soc.h"

#include <qdrv/qdrv_mac.h>
#include <qtn/txbf_mbox.h>

#include <qtn/topaz_tqe_cpuif.h>

#include <qtn/hardware_revision.h>

#define	IS_UP(_dev) \
	(((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define	IS_UP_AUTO(_vap) \
	(IS_UP((_vap)->iv_dev) && \
	 (_vap)->iv_ic->ic_roaming == IEEE80211_ROAMING_AUTO)
#define	RESCAN	1

#define DBGMAC "%02X:%02X:%02X:%02X:%02X:%02X"
#define ETHERFMT(a) \
	        (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

#define STRUCT_MEMBER_SIZEOF(stype, member)		sizeof(((stype *)0)->member)
#define RSSI_SWING_RANGE 30
#define RSSI_MAX_SMTHING_FCTR 100
#define RSSI_HIGH_SMTHING_FCTR 99
#define RSSI_MED_SMTHING_FCTR 95
#define QTN_AMPDU_DETECT_PERIOD 1
#define QTN_RSSI_SAMPLE_TH	4

#define SCS_CHAN_POWER_DIFF_SAFE		2
#define SCS_CHAN_POWER_DIFF_MAX			16
#define SCS_MAX_RAW_CHAN_METRIC			0x7FFFFFFF
#define SCS_MAX_RATE_RATIO_CAP			0x7FFFFFFF
#define SCS_PICK_CHAN_MIN_SCALED_TRAFFIC	100 /* ms */
#define MIN_CAC_PERIOD				70 /* seconds */

#ifndef SYSTEM_BUILD
#define ic2dev(ic)	((struct ieee80211vap *)(TAILQ_FIRST(&(ic)->ic_vaps)) ? \
			((struct ieee80211vap *)(TAILQ_FIRST(&(ic)->ic_vaps)))->iv_dev : NULL)
#else
#define ic2dev(ic)	NULL
#endif
#define DFS_S_DBG_QEVT(qevtdev, ...)	do {\
						printk(__VA_ARGS__);\
						ieee80211_eventf(qevtdev, __VA_ARGS__);\
					} while (0)

#define	IEEE80211_OBSS_AP_SCAN_INT 25
int wlan_11ac_20M_mcs_nss_tbl[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, -1,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, -1,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, -1
};

#define DM_DEFAULT_TX_POWER_FACTOR	2
#define DM_DEFAULT_DFS_FACTOR		8

#if defined(CONFIG_QTN_80211K_SUPPORT)
const uint8_t ieee80211_meas_sta_qtn_report_subtype_len[RM_QTN_CTRL_END + 1] = {
	[RM_QTN_TX_STATS] = sizeof(struct ieee80211_ie_qtn_rm_txstats),
	[RM_QTN_RX_STATS] = sizeof(struct ieee80211_ie_qtn_rm_rxstats),
	[RM_QTN_MAX_QUEUED] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, max_queued),
	[RM_QTN_LINK_QUALITY] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, link_quality),
	[RM_QTN_RSSI_DBM] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, rssi_dbm),
	[RM_QTN_BANDWIDTH] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, bandwidth),
	[RM_QTN_SNR] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, snr),
	[RM_QTN_TX_PHY_RATE] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, tx_phy_rate),
	[RM_QTN_RX_PHY_RATE] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, rx_phy_rate),
	[RM_QTN_CCA] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, cca),
	[RM_QTN_BR_IP] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, br_ip),
	[RM_QTN_RSSI] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, rssi),
	[RM_QTN_HW_NOISE] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, hw_noise),
	[RM_QTN_SOC_MACADDR] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, soc_macaddr),
	[RM_QTN_SOC_IPADDR] = STRUCT_MEMBER_SIZEOF(struct ieee80211_ie_qtn_rm_sta_all, soc_ipaddr),
	[RM_QTN_UNKNOWN] = sizeof(u_int32_t),
	[RM_QTN_RESET_CNTS] = sizeof(u_int32_t),
	[RM_QTN_RESET_QUEUED] = sizeof(u_int32_t),
};
#endif

struct assoc_info_report {
	uint64_t	ai_rx_bytes;
	uint64_t	ai_tx_bytes;
	uint32_t	ai_rx_packets;
	uint32_t	ai_tx_packets;
	uint32_t	ai_rx_errors;
	uint32_t	ai_tx_errors;
	uint32_t	ai_rx_dropped;
	uint32_t	ai_tx_dropped;
	uint32_t	ai_tx_wifi_drop[WME_AC_NUM];
	uint32_t	ai_tx_ucast;
	uint32_t	ai_rx_ucast;
	uint32_t	ai_tx_mcast;
	uint32_t	ai_rx_mcast;
	uint32_t	ai_tx_bcast;
	uint32_t	ai_rx_bcast;
	uint32_t	ai_tx_failed;
	uint32_t	ai_time_associated;	/*Unit: seconds*/
	uint16_t	ai_assoc_id;
	uint16_t	ai_link_quality;
	uint16_t	ai_tx_phy_rate;
	uint16_t	ai_rx_phy_rate;
	uint32_t	ai_achievable_tx_phy_rate;
	uint32_t	ai_achievable_rx_phy_rate;
	u_int32_t	ai_rx_fragment_pkts;
	u_int32_t	ai_rx_vlan_pkts;
	uint8_t		ai_mac_addr[IEEE80211_ADDR_LEN];
	int32_t		ai_rssi;
	int32_t		ai_smthd_rssi;
	int32_t		ai_snr;
	int32_t		ai_max_queued;
	uint8_t		ai_bw;
	uint8_t		ai_tx_mcs;
	uint8_t		ai_rx_mcs;
	uint8_t		ai_auth;
	char		ai_ifname[IFNAMSIZ];
	uint32_t	ai_ip_addr;
	int32_t		ai_hw_noise;
	uint32_t	ai_is_qtn_node;
};

struct assoc_info_table {
	uint16_t	unit_size;	/* Size of structure assoc_info_table */
	uint16_t	cnt;		/* Record the number of valid entries */
	struct assoc_info_report array[QTN_ASSOC_LIMIT];
};

struct sample_assoc_data {
	uint8_t mac_addr[IEEE80211_ADDR_LEN];
	uint8_t assoc_id;
	uint8_t bw;
	uint8_t tx_stream;
	uint8_t rx_stream;
	uint32_t time_associated;	/*Unit: seconds*/
	uint32_t achievable_tx_phy_rate;
	uint32_t achievable_rx_phy_rate;
	uint32_t rx_packets;
	uint32_t tx_packets;
	uint32_t rx_errors;
	uint32_t tx_errors;
	uint32_t rx_dropped;
	uint32_t tx_dropped;
	uint32_t tx_wifi_drop[WME_AC_NUM];
	uint32_t rx_ucast;
	uint32_t tx_ucast;
	uint32_t rx_mcast;
	uint32_t tx_mcast;
	uint32_t rx_bcast;
	uint32_t tx_bcast;
	uint16_t link_quality;
	uint32_t ip_addr;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint32_t last_rssi_dbm[NUM_ANT + 1];
	uint32_t last_rcpi_dbm[NUM_ANT + 1];
	uint32_t last_evm_dbm[NUM_ANT + 1];
	uint32_t last_hw_noise[NUM_ANT + 1];
	uint8_t protocol;
	uint8_t vendor;
}__packed;

struct sample_assoc_user_data {
	int num_entry;
	int offset;
	struct sample_assoc_data *data;
};

struct node_client_data {
	struct list_head node_list;
	struct sample_assoc_data data;
};

struct node_txrx_airtime {
	uint16_t node_index;
	uint8_t  macaddr[IEEE80211_ADDR_LEN];
	uint32_t tx_airtime;
	uint32_t tx_airtime_accum;
	uint32_t rx_airtime;
	uint32_t rx_airtime_accum;
};

struct txrx_airtime {
	uint16_t               nr_nodes;     /* number of nodes */
	uint16_t               free_airtime; /* in ms */
	uint32_t               total_tx_airtime;/* total tx airtime of clients */
	uint32_t               total_rx_airtime; /* total rx airtime of clients */
	struct node_txrx_airtime nodes[QTN_ASSOC_LIMIT];
};

#ifdef WLAN_MALLOC_FREE_TOT_DEBUG
int g_wlan_tot_alloc = 0;
int g_wlan_tot_alloc_cnt = 0;
int g_wlan_tot_free = 0;
int g_wlan_tot_free_cnt = 0;
int g_wlan_balance = 0;
int g_wlan_tot_node_alloc = 0;
int g_wlan_tot_node_alloc_tmp = 0;
int g_wlan_tot_node_free = 0;
int g_wlan_tot_node_free_tmp = 0;

EXPORT_SYMBOL(g_wlan_tot_alloc);
EXPORT_SYMBOL(g_wlan_tot_alloc_cnt);
EXPORT_SYMBOL(g_wlan_tot_free);
EXPORT_SYMBOL(g_wlan_tot_free_cnt);
EXPORT_SYMBOL(g_wlan_balance);
EXPORT_SYMBOL(g_wlan_tot_node_alloc);
EXPORT_SYMBOL(g_wlan_tot_node_alloc_tmp);
EXPORT_SYMBOL(g_wlan_tot_node_free);
EXPORT_SYMBOL(g_wlan_tot_node_free_tmp);
#endif

extern uint16_t g_wowlan_host_state;
extern uint16_t g_wowlan_match_type;
extern uint16_t g_wowlan_l2_ether_type;
extern uint16_t g_wowlan_l3_udp_port;

extern int fwt_db_get_macs_behind_node(const uint8_t index, uint32_t *num_entries, uint32_t max_req,
					uint32_t *flags, uint8_t *buf);

int ieee80211_send_tuning_data(struct ieee80211_node *);
void topaz_congest_set_unicast_queue_count(uint32_t qnum);
static void get_node_max_rssi (void *arg, struct ieee80211_node *ni);
static void ieee80211_pco_timer_func ( unsigned long arg );
static int ieee80211_ba_setup_detect_set(struct ieee80211vap *vap, int enable);
static int ieee80211_wds_vap_exists(struct ieee80211com *ic);

extern u_int8_t g_channel_fixed;
/*
 * The RSSI values reported in the TX/RX descriptors in the driver are the SNR
 * expressed in dBm. Thus 'rssi' is signal level above the noise floor in dBm.
 *
 * Noise is measured in dBm and is negative unless there is an unimaginable
 * level of RF noise.
 *
 * The signal level is noise + rssi.
 *
 * Note that the iw_quality values are 1 byte, and can be signed, unsigned or
 * negative depending on context.
 *
 */
void
set_quality(struct iw_quality *iq, u_int rssi, int noise)
{
	iq->qual = rssi;
	iq->noise = noise;
	iq->level = ((((int)rssi + noise) <= 0) ? ((int)rssi + noise) : 0);
	iq->updated = IW_QUAL_ALL_UPDATED;
	iq->updated |= IW_QUAL_DBM;
}
static void
pre_announced_chanswitch(struct net_device *dev, u_int32_t channel, u_int32_t tbtt);

static void
preempt_scan(struct net_device *dev, int max_grace, int max_wait)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	int total_delay = 0;
	int canceled = 0, ready = 0;
	while (!ready && total_delay < max_grace + max_wait) {
	  if ((ic->ic_flags & IEEE80211_F_SCAN) == 0
#ifdef QTN_BG_SCAN
		&& (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0
#endif /* QTN_BG_SCAN */
	  ) {
	    ready = 1;
	  } else {
	    if (!canceled && total_delay > max_grace) {
	      /*
		 Cancel any existing active scan, so that any new parameters
		 in this scan ioctl (or the defaults) can be honored, then
		 wait around a while to see if the scan cancels properly.
	      */
	      IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: cancel pending scan request\n", __func__);
	      (void) ieee80211_cancel_scan(vap);
	      canceled = 1;
	    }
	    mdelay (1);
	    total_delay += 1;
	  }
	}
	if (!ready) {
	  IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			    "%s: Timeout canceling current scan.\n",
			    __func__);
	}
}

static struct iw_statistics *
ieee80211_iw_getstats(struct net_device *dev)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

#ifdef USE_LINUX_FRAMEWORK
	struct iw_statistics *is = &vap->iv_iwstats;
	set_quality(&is->qual, ieee80211_getrssi(vap->iv_ic),
			ic->ic_channoise);
	is->status = vap->iv_state;
	is->discard.nwid = vap->iv_stats.is_rx_wrongbss +
		vap->iv_stats.is_rx_ssidmismatch;
	is->discard.code = vap->iv_stats.is_rx_wepfail +
		vap->iv_stats.is_rx_decryptcrc;
	is->discard.fragment = 0;
	is->discard.retries = 0;
	is->discard.misc = 0;

	is->miss.beacon = 0;
	return is;
#else

	struct iw_statistics *is = &ic->ic_iwstats;
	ic->ic_get_wlanstats(ic, is);

	return is;
#endif

}

static int
ieee80211_ioctl_giwname(struct net_device *dev, struct iw_request_info *info,
	char *name, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211_channel *c = vap->iv_ic->ic_curchan;

	if (vap->iv_ic->ic_des_mode == IEEE80211_MODE_AUTO &&
		vap->iv_ic->ic_rf_chipid == CHIPID_DUAL)
		/* Display all the supported modes for RFIC5 */
		strncpy(name, "IEEE 802.11gnac", IFNAMSIZ);
	else if ((IEEE80211_IS_CHAN_11AC(c) ) &&
		(vap->iv_ic->ic_phymode >= IEEE80211_MODE_11AC_VHT20PM))
                strncpy(name, "IEEE 802.11ac", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_108G(c))
		strncpy(name, "IEEE 802.11Tg", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_108A(c))
		strncpy(name, "IEEE 802.11Ta", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_TURBO(c))
		strncpy(name, "IEEE 802.11T", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_11NG(c) &&
		(vap->iv_ic->ic_phymode == IEEE80211_MODE_11NG_HT40PM))
		strncpy(name, "IEEE 802.11ng40", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_11NG(c) &&
		(vap->iv_ic->ic_phymode == IEEE80211_MODE_11NG))
		strncpy(name, "IEEE 802.11ng", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_11NA(c) &&
		(vap->iv_ic->ic_phymode == IEEE80211_MODE_11NA_HT40PM))
		strncpy(name, "IEEE 802.11na40", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_11NA(c) &&
		(vap->iv_ic->ic_phymode == IEEE80211_MODE_11NA))
		strncpy(name, "IEEE 802.11na", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_ANYG(c) &&
		(vap->iv_ic->ic_phymode == IEEE80211_MODE_11G))
		strncpy(name, "IEEE 802.11g", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_A(c))
		strncpy(name, "IEEE 802.11a", IFNAMSIZ);
	else if (IEEE80211_IS_CHAN_B(c))
		strncpy(name, "IEEE 802.11b", IFNAMSIZ);
	else
		strncpy(name, "IEEE 802.11", IFNAMSIZ);
	/* XXX FHSS */
	return 0;
}

/*
 * Get a key index from a request.  If nothing is
 * specified in the request we use the current xmit
 * key index.  Otherwise we just convert the index
 * to be base zero.
 */
static int
getiwkeyix(struct ieee80211vap *vap, const struct iw_point* erq, int *kix)
{
	int kid;

	kid = erq->flags & IW_ENCODE_INDEX;
	if (kid < 1 || kid > IEEE80211_WEP_NKID) {
		kid = vap->iv_def_txkey;
		if (kid == IEEE80211_KEYIX_NONE)
			kid = 0;
	} else {
		--kid;
	}
	if (0 <= kid && kid < IEEE80211_WEP_NKID) {
		*kix = kid;
		return 0;
	} else
		return -EINVAL;
}

static int
ieee80211_ioctl_siwencode(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *erq, char *keybuf)
{
#ifndef IEEE80211_UNUSED_CRYPTO_COMMANDS
	return -EOPNOTSUPP;
#else
	struct ieee80211vap *vap = netdev_priv(dev);
	int kid = 0;
	int error = -EINVAL;
	int wepchange = 0;

	if ((erq->flags & IW_ENCODE_DISABLED) == 0) {
		/*
		 * Enable crypto, set key contents, and
		 * set the default transmit key.
		 */
		error = getiwkeyix(vap, erq, &kid);
		if (error < 0)
			return error;
		if (erq->length > IEEE80211_KEYBUF_SIZE)
			return -EINVAL;
		/* XXX no way to install 0-length key */
		if (erq->length > 0) {
			struct ieee80211_key *k = &vap->iv_nw_keys[kid];

			/*
			 * Set key contents.  This interface only supports WEP.
			 * Indicate intended key index.
			 */
			k->wk_keyix = kid;
			k->wk_keylen = erq->length;
			k->wk_ciphertype = IEEE80211_CIPHER_WEP;
			memcpy(k->wk_key, keybuf, erq->length);
			memset(k->wk_key + erq->length, 0,
					IEEE80211_KEYBUF_SIZE - erq->length);
			error = vap->iv_key_set(vap, k, vap->iv_myaddr);

		} else
			error = -EINVAL;
	} else {
		/*
		 * When the length is zero the request only changes
		 * the default transmit key.  Verify the new key has
		 * a non-zero length.
		 */
		if (vap->iv_nw_keys[kid].wk_keylen == 0)
			error = -EINVAL;
	}
	if (error == 0) {
		/*
		 * The default transmit key is only changed when:
		 * 1. Privacy is enabled and no key matter is
		 *    specified.
		 * 2. Privacy is currently disabled.
		 * This is deduced from the iwconfig man page.
		 */
		if (erq->length == 0 ||
				(vap->iv_flags & IEEE80211_F_PRIVACY) == 0)
			vap->iv_def_txkey = kid;
		wepchange = (vap->iv_flags & IEEE80211_F_PRIVACY) == 0;
		vap->iv_flags |= IEEE80211_F_PRIVACY;
	} else {
		if ((vap->iv_flags & IEEE80211_F_PRIVACY) == 0)
			return 0;
		vap->iv_flags &= ~IEEE80211_F_PRIVACY;
		wepchange = 1;
		error = 0;
	}
	if (error == 0) {
		/* Set policy for unencrypted frames */
		if ((erq->flags & IW_ENCODE_OPEN) &&
				(!(erq->flags & IW_ENCODE_RESTRICTED))) {
			vap->iv_flags &= ~IEEE80211_F_DROPUNENC;
		} else if (!(erq->flags & IW_ENCODE_OPEN) &&
				(erq->flags & IW_ENCODE_RESTRICTED)) {
			vap->iv_flags |= IEEE80211_F_DROPUNENC;
		} else {
			/* Default policy */
			if (vap->iv_flags & IEEE80211_F_PRIVACY)
				vap->iv_flags |= IEEE80211_F_DROPUNENC;
			else
				vap->iv_flags &= ~IEEE80211_F_DROPUNENC;
		}
	}
	if (error == 0 && IS_UP(vap->iv_dev)) {
		/*
		 * Device is up and running; we must kick it to
		 * effect the change.  If we're enabling/disabling
		 * crypto use then we must re-initialize the device
		 * so the 802.11 state machine is reset.  Otherwise
		 * the key state should have been updated above.
		 */
		if (wepchange && IS_UP_AUTO(vap))
			ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	}
	return error;
#endif /* IEEE80211_UNUSED_CRYPTO_COMMANDS */
}

static int
ieee80211_ioctl_giwencode(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *erq, char *key)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211_key *k;
	int error, kid;

	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
		error = getiwkeyix(vap, erq, &kid);
		if (error < 0)
			return error;
		k = &vap->iv_nw_keys[kid];
		/* XXX no way to return cipher/key type */

		erq->flags = kid + 1;			/* NB: base 1 */
		if (erq->length > k->wk_keylen)
			erq->length = k->wk_keylen;
		memcpy(key, k->wk_key, erq->length);
		erq->flags |= IW_ENCODE_ENABLED;
	} else {
		erq->length = 0;
		erq->flags = IW_ENCODE_DISABLED;
	}
	if (vap->iv_flags & IEEE80211_F_DROPUNENC)
		erq->flags |= IW_ENCODE_RESTRICTED;
	else
		erq->flags |= IW_ENCODE_OPEN;
	return 0;
}

#ifndef ifr_media
#define	ifr_media	ifr_ifru.ifru_ivalue
#endif

static int
ieee80211_ioctl_siwrate(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	static const u_int mopts[] = {
		IFM_AUTO,
		IFM_IEEE80211_11A,
		IFM_IEEE80211_11B,
		IFM_IEEE80211_11G,
		IFM_IEEE80211_FH,
		IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
		IFM_IEEE80211_11G | IFM_IEEE80211_TURBO,
		IFM_IEEE80211_11NA,
		IFM_IEEE80211_11NG,
		IFM_IEEE80211_11NG_HT40PM,
		IFM_IEEE80211_11NA_HT40PM,
		IFM_IEEE80211_11AC_VHT20PM,
		IFM_IEEE80211_11AC_VHT40PM,
		IFM_IEEE80211_11AC_VHT80PM,
		IFM_IEEE80211_11AC_VHT160PM,
	};
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ifreq ifr;
	int rate, retv;
	u_int16_t mode = ic->ic_des_mode;
	u_int16_t chan_mode = 0;
	uint8_t sgi = 0;

	if (mode == IEEE80211_MODE_AUTO)
		return -EINVAL;

	if (vap->iv_media.ifm_cur == NULL)
		return -EINVAL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_media = vap->iv_media.ifm_cur->ifm_media &~ (IFM_MMASK|IFM_TMASK);
	ifr.ifr_media |= mopts[ic->ic_des_mode];


	if (rrq->fixed) {
		/* XXX fudge checking rates */
		if (mode < IEEE80211_MODE_11NA) {
			rate = ieee80211_rate2media(ic, 2 * rrq->value / 1000000,
				ic->ic_des_mode);
		} else {
			if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) {
				chan_mode = 1;
				sgi = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;
			} else {
				sgi = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
			}

			rate = ieee80211_rate2mcs(2 * rrq->value / 1000000, chan_mode, sgi);
			/* No mcs match found. It can be a legacy rate */
			if (rate < 0) {
				rate = ieee80211_mcs2media(ic,
					(2 * rrq->value / 1000000),
					ic->ic_des_mode);
			} else {
				rate = ieee80211_mcs2media(ic, rate, ic->ic_des_mode);
			}
		}
		if (rate == IFM_AUTO) {		/* NB: unknown rate */
			return -EINVAL;
		}
	} else {
		rate = IFM_AUTO;
		vap->iv_mcs_config = IEEE80211_MCS_AUTO_RATE_ENABLE;
	}
	ifr.ifr_media |= IFM_SUBTYPE(rate);

	/* refresh media capabilities based on channel */
	ifmedia_removeall(&vap->iv_media);
	(void) ieee80211_media_setup(ic, &vap->iv_media,
		vap->iv_caps, vap->iv_media.ifm_change, vap->iv_media.ifm_status);

	retv = ifmedia_ioctl(vap->iv_dev, &ifr, &vap->iv_media, SIOCSIFMEDIA);
	if (retv == -ENETRESET)
	{
#if 0 //No need to restart network after rate change
		retv = IS_UP_AUTO(vap) ? ieee80211_open(vap->iv_dev) : 0;
#endif
		return 0;
	}
	return retv;
}

static int
ieee80211_ioctl_giwrate(struct net_device *dev,	struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ifmediareq imr;
	int rate, mcs;
	u_int16_t mode, chan_mode = 0;
	uint8_t sgi = 0;
	mode = ic->ic_des_mode;

	memset(&imr, 0, sizeof(imr));
	vap->iv_media.ifm_status((void *) vap, &imr);

	rrq->fixed = IFM_SUBTYPE(vap->iv_media.ifm_media) != IFM_AUTO;
	/* media status will have the current xmit rate if available */

	if(mode < IEEE80211_MODE_11NA)
	{
		rate = ieee80211_media2rate(imr.ifm_active);

		if (rate == -1)		/* IFM_AUTO */
			rate = 0;
		rrq->value = 1000000 * (rate / 2);
	}
	else
	{
		if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) {
			chan_mode = 1;
			sgi = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;
		} else {
			sgi = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
		}
		rate = ieee80211_media2mcs(imr.ifm_active);
		if(rate > 0) //Fixed rate
		{
			if(rate & 0x80 ) /* if 11n rate is an mcs index */
			{
				mcs = rate & 0xf;
				rate = ieee80211_mcs2rate(mcs, chan_mode, sgi, 0);
			}
		}
		else		/* IFM_AUTO */
		{
			rate = 0;
		}
		rrq->value = 1000000 * (rate / 2);
	}
	return 0;
}

static int
ieee80211_ioctl_siwsens(struct net_device *dev,	struct iw_request_info *info,
	struct iw_param *sens, char *extra)
{
	return -EOPNOTSUPP;
}

static int
ieee80211_ioctl_giwsens(struct net_device *dev,	struct iw_request_info *info,
	struct iw_param *sens, char *extra)
{
	sens->value = 1;
	sens->fixed = 1;

	return 0;
}

static int
ieee80211_ioctl_siwrts(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rts, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	u32 val;

	if (rts->disabled)
		val = IEEE80211_RTS_THRESH_OFF;
	else if (IEEE80211_RTS_MIN <= rts->value &&
	    rts->value <= IEEE80211_RTS_MAX)
		val = rts->value;
	else
		return -EINVAL;
	if (val != vap->iv_rtsthreshold) {
		vap->iv_rtsthreshold = val;
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_RTSTHRESHOLD, val, NULL, 0);
	}
	return 0;
}

static int
ieee80211_ioctl_giwrts(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rts, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	rts->value = vap->iv_rtsthreshold;
	rts->disabled = (rts->value == IEEE80211_RTS_THRESH_OFF);
	rts->fixed = 1;

	return 0;
}

static int
ieee80211_ioctl_siwfrag(struct net_device *dev,	struct iw_request_info *info,
	struct iw_param *rts, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	u16 val;

	if (rts->disabled)
		val = 2346;
	else if (rts->value < 256 || rts->value > 2346)
		return -EINVAL;
	else
		val = (rts->value & ~0x1);

	if (val != vap->iv_fragthreshold) {
		vap->iv_fragthreshold = val;
		return ic->ic_reset(ic);
	}

	return 0;
}

static int
ieee80211_ioctl_giwfrag(struct net_device *dev,	struct iw_request_info *info,
	struct iw_param *rts, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	rts->value = vap->iv_fragthreshold;
	rts->disabled = (rts->value == 2346);
	rts->fixed = 1;

	return 0;
}

static int
ieee80211_ioctl_siwap(struct net_device *dev, struct iw_request_info *info,
	struct sockaddr *ap_addr, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	/* NB: should not be set when in AP mode */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		return -EINVAL;

	if (vap->iv_opmode == IEEE80211_M_WDS)
		IEEE80211_ADDR_COPY(vap->wds_mac, &ap_addr->sa_data);

	/*
	 * zero address corresponds to 'iwconfig ath0 ap off', which means
	 * enable automatic choice of AP without actually forcing a
	 * reassociation.
	 *
	 * broadcast address corresponds to 'iwconfig ath0 ap any', which
	 * means scan for the current best AP.
	 *
	 * anything else specifies a particular AP.
	 */
	vap->iv_flags &= ~IEEE80211_F_DESBSSID;
	if (!IEEE80211_ADDR_NULL(&ap_addr->sa_data)) {
		if (!IEEE80211_ADDR_EQ(vap->iv_des_bssid, (u_int8_t*) "\xff\xff\xff\xff\xff\xff"))
			vap->iv_flags |= IEEE80211_F_DESBSSID;

		IEEE80211_ADDR_COPY(vap->iv_des_bssid, &ap_addr->sa_data);
		if (IS_UP_AUTO(vap))
			ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	}
	return 0;
}

static int
ieee80211_ioctl_giwap(struct net_device *dev, struct iw_request_info *info,
	struct sockaddr *ap_addr, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (vap->iv_flags & IEEE80211_F_DESBSSID) {
		IEEE80211_ADDR_COPY(&ap_addr->sa_data, vap->iv_des_bssid);
	} else if (vap->iv_opmode == IEEE80211_M_WDS) {
		IEEE80211_ADDR_COPY(&ap_addr->sa_data, vap->wds_mac);
	} else if ((vap->iv_opmode == IEEE80211_M_HOSTAP && vap->iv_state == IEEE80211_S_SCAN) ||
		    vap->iv_state == IEEE80211_S_RUN) {
			IEEE80211_ADDR_COPY(&ap_addr->sa_data, vap->iv_bss->ni_bssid);
	} else {
		IEEE80211_ADDR_SET_NULL(&ap_addr->sa_data);
	}

	ap_addr->sa_family = ARPHRD_ETHER;
	return 0;
}

static int
ieee80211_ioctl_siwnickn(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *nickname)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (data->length > IEEE80211_NWID_LEN)
		return -EINVAL;

	memset(vap->iv_nickname, 0, IEEE80211_NWID_LEN);
	memcpy(vap->iv_nickname, nickname, data->length);
	vap->iv_nicknamelen = data->length;

	return 0;
}

static int
ieee80211_ioctl_giwnickn(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *nickname)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (data->length > vap->iv_nicknamelen + 1)
		data->length = vap->iv_nicknamelen + 1;
	if (data->length > 0) {
		memcpy(nickname, vap->iv_nickname, data->length - 1); /* XXX: strcpy? */
		nickname[data->length-1] = '\0';
	}
	return 0;
}

static int
find11gchannel(struct ieee80211com *ic, int i, int freq)
{
	for (; i < ic->ic_nchans; i++) {
		const struct ieee80211_channel *c = &ic->ic_channels[i];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return 1;
	}
	return 0;
}

struct ieee80211_channel *
findchannel(struct ieee80211com *ic, int ieee, int mode)
{
	u_int modeflags;
	int i;

	modeflags = ieee80211_get_chanflags(mode);
	for (i = 0; i < ic->ic_nchans; i++) {
		struct ieee80211_channel *c = &ic->ic_channels[i];

		if (c->ic_ieee != ieee)
			continue;
		if (mode == IEEE80211_MODE_AUTO) {
			/*
			 * XXX special-case 11b/g channels so we
			 *     always select the g channel if both
			 *     are present.
			 */
			if (!IEEE80211_IS_CHAN_B(c) ||
			    !find11gchannel(ic, i + 1, c->ic_freq))
				return c;
		} else {
			if ((c->ic_flags & modeflags) == modeflags)
				return c;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(findchannel);

struct ieee80211_channel *
findchannel_any(struct ieee80211com *ic, int ieee, int prefer_mode)
{
	struct ieee80211_channel *c;

	c = findchannel(ic, ieee, prefer_mode);
	if (c == NULL) {
		c = findchannel(ic, ieee, IEEE80211_MODE_AUTO);
		if (c == NULL) {
			printk("Channel %d does not exist\n", ieee);
			c = IEEE80211_CHAN_ANYC;
		}
	}

	return c;
}

struct ieee80211_channel *ieee80211_find_channel_by_ieee(struct ieee80211com *ic, int chan_ieee)
{
	struct ieee80211_channel *chan;

	if (chan_ieee > IEEE80211_CHAN_MAX) {
		return NULL;
	}

	if (isclr(ic->ic_chan_active, chan_ieee)) {
		return NULL;
	}

	chan = findchannel(ic, chan_ieee, ic->ic_des_mode);
	if (chan == NULL) {
		chan = findchannel(ic, chan_ieee, IEEE80211_MODE_AUTO);
	}

	return chan;
}
EXPORT_SYMBOL(ieee80211_find_channel_by_ieee);

static char *
ieee80211_wireless_swfeat_desc(const enum swfeat feat)
{
	char *desc = "Invalid feature";

	switch (feat) {
	case SWFEAT_ID_MODE_AP:
		desc = "Access Point";
		break;
	case SWFEAT_ID_MODE_STA:
		desc = "Non-AP station";
		break;
	case SWFEAT_ID_MODE_REPEATER:
		desc = "Repeater";
		break;
	case SWFEAT_ID_PCIE_RC:
		desc = "PCIe RC mode";
		break;
	case SWFEAT_ID_VHT:
		desc = "VHT (802.11ac)";
		break;
	case SWFEAT_ID_2X2:
		desc = "802.11ac 2x2";
		break;
	case SWFEAT_ID_2X4:
		desc = "802.11ac 2x4";
		break;
	case SWFEAT_ID_3X3:
		desc = "802.11ac 3x3";
		break;
	case SWFEAT_ID_4X4:
		desc = "802.11ac 4x4";
		break;
	case SWFEAT_ID_HS20:
		desc = "Hotspot 2.0 (802.11u)";
		break;
	case SWFEAT_ID_WPA2_ENT:
		desc = "WPA2 Enterprise";
		break;
	case SWFEAT_ID_MESH:
		desc = "Mesh (802.11s)";
		break;
	case SWFEAT_ID_TDLS:
		desc = "TDLS (802.11z)";
		break;
	case SWFEAT_ID_OCAC:
		desc = "Zero-Second DFS (OCAC)";
		break;
	case SWFEAT_ID_QHOP:
		desc = "QHOP (WDS Extender)";
		break;
	case SWFEAT_ID_QSV:
		desc = "Spectrum View (QSV)";
		break;
	case SWFEAT_ID_QSV_NEIGH:
		desc = "Neighbour Report";
		break;
	case SWFEAT_ID_MU_MIMO:
		desc = "MU-MIMO";
		break;
	case SWFEAT_ID_DUAL_CHAN_VIRT:
		desc = "Dual Channel Virtual Concurrent";
		break;
	case SWFEAT_ID_DUAL_CHAN:
		desc = "Dual Channel Dual Concurrent";
		break;
	case SWFEAT_ID_DUAL_BAND_VIRT:
		desc = "Dual Band Virtual Concurrent";
		break;
	case SWFEAT_ID_DUAL_BAND:
		desc = "Dual Band Dual Concurrent";
		break;
	case SWFEAT_ID_QTM_PRIO:
		desc = "QTM - Per SSID Prioritisation ";
		break;
	case SWFEAT_ID_QTM:
		desc = "QTM - Network Aware";
		break;
	case SWFEAT_ID_SPEC_ANALYZER:
		desc = "Spectrum Analyzer";
		break;
	case SWFEAT_ID_MAX:
		break;
	}

	return desc;
}

static int
ieee80211_subioctl_print_swfeat_map(struct net_device *dev,
				void __user *outbuf, int len)
{
	char *buf;
	char *bufp;
	int i;
	int j;
	int rem = len;
	int rc = 0;

	if (!outbuf) {
		printk("%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}

	buf = kzalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		printk("%s: buffer alloc failed\n", __FUNCTION__);
		return -EFAULT;
	}

	bufp = buf;
	for (i = 0; i < SWFEAT_ID_MAX; i++) {
		if (isset(soc_shared_params->swfeat_map, i)) {
			j = snprintf(bufp, rem, "%s\n", ieee80211_wireless_swfeat_desc(i));
			if (j <= 0)
				break;
			bufp += j;
			rem -= j;
			if (rem <= 0)
				break;
		}
	}

	if (copy_to_user(outbuf, buf, len) != 0) {
		printk("%s: copy_to_user failed\n", __FUNCTION__);
		rc = -EIO;
	}

	kfree(buf);

	return rc;
}

static int
ieee80211_subioctl_get_swfeat_map(struct net_device *dev,
				void __user *swfeat_map, int len)
{
	if (!swfeat_map) {
		printk("%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}

	if (len != sizeof(soc_shared_params->swfeat_map)) {
		printk("%s: invalid size\n", __FUNCTION__);
		return -EINVAL;
	}

	if (copy_to_user(swfeat_map, &soc_shared_params->swfeat_map, len) != 0) {
		printk("%s: copy_to_user failed\n", __FUNCTION__);
		return -EIO;
	}

	return 0;
}

/*
 * Feature restrictions are enforced in the MuC firmware. Bypassing this check will
 * cause the system to continually reboot.
 */
int ieee80211_swfeat_is_supported(uint16_t feat, uint8_t print_msg)
{
	if ((feat < SWFEAT_ID_MAX) && isset(soc_shared_params->swfeat_map, feat))
		return 1;

	if (print_msg)
		printk("%s is not supported on this device\n",
			ieee80211_wireless_swfeat_desc(feat));

	return 0;
}
EXPORT_SYMBOL(ieee80211_swfeat_is_supported);

static inline int
ieee80211_vht_tx_mcs_is_valid(uint32_t mcs_val, uint32_t mcs_nss)
{
	if (mcs_val >= IEEE80211_AC_MCS_MAX || mcs_nss >= IEEE80211_AC_MCS_NSS_MAX)
		return 0;

	if (ieee80211_swfeat_is_supported(SWFEAT_ID_2X2, 0) ||
			ieee80211_swfeat_is_supported(SWFEAT_ID_2X4, 0)) {
		if (mcs_nss >= IEEE80211_VHT_NSS2)
			return 0;
	} else if (ieee80211_swfeat_is_supported(SWFEAT_ID_3X3, 0)) {
		if (mcs_nss >= IEEE80211_VHT_NSS3)
			return 0;
	}

	return 1;
}

static inline int
ieee80211_ht_tx_mcs_is_valid(uint32_t mcs)
{
	if (mcs < IEEE80211_HT_EQUAL_MCS_START ||
			mcs > IEEE80211_UNEQUAL_MCS_MAX ||
			mcs == IEEE80211_EQUAL_MCS_32)
		return 0;

	if (ieee80211_swfeat_is_supported(SWFEAT_ID_2X2, 0) ||
			ieee80211_swfeat_is_supported(SWFEAT_ID_2X4, 0)) {
		if ((mcs > IEEE80211_HT_EQUAL_MCS_2SS_MAX &&
					mcs < IEEE80211_UNEQUAL_MCS_START) ||
				mcs > IEEE80211_HT_UNEQUAL_MCS_2SS_MAX) {
			return 0;
		}
	} else if (ieee80211_swfeat_is_supported(SWFEAT_ID_3X3, 0)) {
		if ((mcs > IEEE80211_HT_EQUAL_MCS_3SS_MAX &&
					mcs < IEEE80211_UNEQUAL_MCS_START) ||
				mcs > IEEE80211_HT_UNEQUAL_MCS_3SS_MAX) {
			return 0;
		}
	}

	return 1;
}

#define	IEEE80211_MODE_TURBO_STATIC_A	IEEE80211_MODE_MAX
static int
ieee80211_check_mode_consistency(struct ieee80211com *ic, int mode,
	struct ieee80211_channel *c)
{
	if (c == IEEE80211_CHAN_ANYC)
		return 0;
	switch (mode) {
	case IEEE80211_MODE_11B:
		if (IEEE80211_IS_CHAN_B(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_11G:
		if (IEEE80211_IS_CHAN_ANYG(c)) {
			return 0;
		} else {
			return 1;
		}
		break;
	case IEEE80211_MODE_11NG:
		if (IEEE80211_IS_CHAN_11NG(c)) {
			return 0;
		} else {
			return 1;
		}
		break;
	case IEEE80211_MODE_11NG_HT40PM:
		if (IEEE80211_IS_CHAN_11NG_HT40PLUS(c) || IEEE80211_IS_CHAN_11NG_HT40MINUS(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_11NA:
		if (IEEE80211_IS_CHAN_11NA(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_11A:
		if (IEEE80211_IS_CHAN_A(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_11NA_HT40PM:
		if (IEEE80211_IS_CHAN_11NA_HT40PLUS(c) || IEEE80211_IS_CHAN_11NA_HT40MINUS(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_TURBO_STATIC_A:
		if (IEEE80211_IS_CHAN_A(c) && IEEE80211_IS_CHAN_STURBO(c))
			return -1; /* Mode not supported */
		else
			return 1;
		break;
	case IEEE80211_MODE_11AC_VHT20PM:
		if (IEEE80211_IS_CHAN_11AC(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_11AC_VHT40PM:
		if (IEEE80211_IS_CHAN_11AC_VHT40PLUS(c) || IEEE80211_IS_CHAN_11AC_VHT40MINUS(c))
			return 0;
		else
			return 1;
		break;
	case IEEE80211_MODE_11AC_VHT80PM:
		if (IEEE80211_IS_CHAN_11AC_VHT80_EDGEPLUS(c) ||
			IEEE80211_IS_CHAN_11AC_VHT80_CNTRPLUS(c) ||
			IEEE80211_IS_CHAN_11AC_VHT80_CNTRMINUS(c) ||
			IEEE80211_IS_CHAN_11AC_VHT80_EDGEMINUS(c))
			return 0;
		else
			return 1;
	case IEEE80211_MODE_AUTO:
		return 0;
		break;
	}
	return -1;
}
#undef	IEEE80211_MODE_TURBO_STATIC_A

void
ieee80211_initiate_scan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	ic->ic_des_chan = IEEE80211_CHAN_ANYC;

	if (IS_UP(vap->iv_dev) && (vap->iv_opmode == IEEE80211_M_HOSTAP)) {
		pre_announced_chanswitch(vap->iv_dev,
					 ieee80211_chan2ieee(ic, ic->ic_des_chan),
					 IEEE80211_DEFAULT_CHANCHANGE_TBTT_COUNT);
		ic->ic_curchan = ic->ic_des_chan;
		ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	}
}
EXPORT_SYMBOL(ieee80211_initiate_scan);

static int
ieee80211_ioctl_siwfreq(struct net_device *dev, struct iw_request_info *info,
	struct iw_freq *freq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211vap *canceled_scan_vap = NULL;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *c, *c2;
	int i;

	if (freq->e > 1) {
		return -EINVAL;
	}
	if (freq->e == 1) {
		i = (ic->ic_mhz2ieee)(ic, freq->m / 100000, 0);
	} else {
		i = freq->m;
	}

	if (i != 0) {
		if (i > IEEE80211_CHAN_MAX) {
			printk("Channel %d is invalid\n", i);
			return -EINVAL;
		}

		c = findchannel(ic, i, ic->ic_des_mode);
		if (c == NULL) {
			printk("Channel %d does not exist\n", i);
			return -EINVAL;
		}

		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if ((c->ic_freq == ic->ic_curchan->ic_freq) && ic->ic_chan_is_set) {
				if (ic->ic_get_init_cac_duration(ic) > 0) {
					ic->ic_stop_icac_procedure(ic);
					printk(KERN_DEBUG "ICAC: Aborted ICAC due to set channel request\n");
				}
				return 0;
			}

			if (!ic->ic_check_channel(ic, c, 0, 1)) {
				printk("Channel %d (%d MHz) cannot be selected\n", i, c->ic_freq);
				return -EINVAL;
			}
		}

		if ((!ieee80211_is_chan_available(c)) && ieee80211_is_on_weather_channel(ic, c)) {
			printk("Weather channel %d (%d MHz) cannot be selected\n", i, c->ic_freq);
			return -EINVAL;
		}

		c = ieee80211_chk_update_pri_chan(ic, c, 0, "iwconfig", 1);
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
				isset(ic->ic_chan_pri_inactive, c->ic_ieee) &&
				isclr(ic->ic_is_inactive_autochan_only, c->ic_ieee)) {
			return -EINVAL;
		}

		i = c->ic_ieee;

		/*
		 * Fine tune channel selection based on desired mode:
		 *   if 11b is requested, find the 11b version of any
		 *      11g channel returned,
		 *   if static turbo, find the turbo version of any
		 *	11a channel return,
		 *   otherwise we should be ok with what we've got.
		 */
		switch (ic->ic_des_mode) {
		case IEEE80211_MODE_11B:
			if (IEEE80211_IS_CHAN_ANYG(c)) {
				c2 = findchannel(ic, i, IEEE80211_MODE_11B);
				/* NB: should not happen, =>'s 11g w/o 11b */
				if (c2 != NULL)
					c = c2;
			}
			break;
		case IEEE80211_MODE_TURBO_A:
			if (IEEE80211_IS_CHAN_A(c)) {
				c2 = findchannel(ic, i, IEEE80211_MODE_TURBO_A);
				if (c2 != NULL)
					c = c2;
			}
			break;
		default:		/* NB: no static turboG */
			break;
		}
		if (ieee80211_check_mode_consistency(ic, ic->ic_des_mode, c)) {
			if (vap->iv_opmode == IEEE80211_M_HOSTAP)
				return -EINVAL;
		}

		/*
		 * Cancel scan before setting desired channel or before return when the channel
		 * is same as bss channel
		 */
		if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && ((ic->ic_flags & IEEE80211_F_SCAN)
#ifdef QTN_BG_SCAN
				|| (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN)
#endif /* QTN_BG_SCAN */
				)) {
			/*
			 * Find which vap is in SCAN state(Only one can be in SCAN state at the same time, other
			 * is pending for scan done on this vap)
			 * For MBSS, it may be the primary vap, or the last vap whose mode is IEEE80211_M_HOSTAP
			 */
			TAILQ_FOREACH(canceled_scan_vap, &ic->ic_vaps, iv_next) {
				if (canceled_scan_vap->iv_state == IEEE80211_S_SCAN) {
					break;
				}
			}
			if (canceled_scan_vap != NULL) {
				if (canceled_scan_vap->iv_state != IEEE80211_S_SCAN) {
					canceled_scan_vap = NULL;
				}
			}

			if (canceled_scan_vap) {
				/*
				 * Cancel channel scan on vap which is SCAN state
				 * For example: scan triggered by freq ioctl or channel auto when boot up
				 */
				ieee80211_cancel_scan_no_wait(canceled_scan_vap);
			} else {
				/*
				 * Cancel channel scan(vap is not in SCAN state)
				 * For example: scan triggered by scan ioctl
				 */
				ieee80211_cancel_scan_no_wait(vap);
			}
		}

		if (vap->iv_state == IEEE80211_S_RUN && c == ic->ic_bsschan)
			return 0;	/* no change, return */

		ic->ic_des_chan = c;
	} else {
		/*
		 * Intepret channel 0 to mean "no desired channel";
		 * otherwise there's no way to undo fixing the desired
		 * channel.
		 */
		if (ic->ic_des_chan == IEEE80211_CHAN_ANYC)
			return 0;
		ic->ic_des_chan = IEEE80211_CHAN_ANYC;
	}

	/* Go out of idle state and delay idle state check.*/
	if (ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] >= BOARD_PM_LEVEL_IDLE) {
		pm_qos_update_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_WLAN, BOARD_PM_LEVEL_NO);
		ieee80211_pm_queue_work(ic);
	}

	if (ic->ic_des_chan == IEEE80211_CHAN_ANYC) {
		ic->ic_csw_reason = IEEE80211_CSW_REASON_SCAN;
	} else {
		ic->ic_csw_reason = IEEE80211_CSW_REASON_MANUAL;
	}

	if ((vap->iv_opmode == IEEE80211_M_MONITOR ||
	    vap->iv_opmode == IEEE80211_M_WDS) &&
	    ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
		/* Monitor and wds modes can switch directly. */
		ic->ic_curchan = ic->ic_des_chan;
		if (vap->iv_state == IEEE80211_S_RUN) {
			ic->ic_set_channel(ic);
		}
	} else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		/*
		 * Use channel switch announcement on beacon if possible.
		 * Otherwise, ic_des_chan will take  effect when we are transitioned
		 * to RUN state later.
		 * We use ic_set_channel directly if we are "running" but not "up".
		 */

		if (IS_UP(vap->iv_dev)) {
			if ((ic->ic_des_chan != IEEE80211_CHAN_ANYC) &&
				(vap->iv_state == IEEE80211_S_RUN)) {
				ieee80211_enter_csa(ic, ic->ic_des_chan, NULL,
					IEEE80211_CSW_REASON_MANUAL,
					IEEE80211_DEFAULT_CHANCHANGE_TBTT_COUNT,
					IEEE80211_CSA_MUST_STOP_TX,
					IEEE80211_CSA_F_BEACON | IEEE80211_CSA_F_ACTION);
			} else {
				if (canceled_scan_vap) {
					/*
					 * Scan is canceled on vap which is in SCAN state,
					 * do SCAN -> SCAN on vap of scan canceled
					 */
					ieee80211_new_state(canceled_scan_vap, IEEE80211_S_SCAN, 0);
				} else {
					ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
				}
			}
		} else if (ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
			ic->ic_curchan = ic->ic_des_chan;
			ic->ic_set_channel(ic);
		}
	} else {
		/* Need to go through the state machine in case we need
		 * to reassociate or the like.  The state machine will
		 * pickup the desired channel and avoid scanning. */
		if (IS_UP_AUTO(vap)) {
			ic->ic_curchan = ic->ic_des_chan;
			ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
			/* In case of no channel change, Don't Scan. Only VCO cal is required */
			ic->ic_set_channel(ic);
		} else {
			/* STA doesn't support auto channel */
			if ((vap->iv_opmode == IEEE80211_M_STA) && (ic->ic_des_chan == IEEE80211_CHAN_ANYC)) {
				ic->ic_des_chan = ic->ic_curchan;
				return -EINVAL;
			} else {
				ic->ic_curchan = ic->ic_des_chan;
				ic->ic_set_channel(ic);
			}
		}
	}

	if (ic->ic_get_init_cac_duration(ic) > 0) {
		ic->ic_stop_icac_procedure(ic);
		printk(KERN_DEBUG "ICAC: Aborted ICAC due to set channel request\n");
	}

	ic->ic_chan_switch_reason_record(ic, IEEE80211_CSW_REASON_MANUAL);
	return 0;
}

static int
ieee80211_ioctl_giwfreq(struct net_device *dev, struct iw_request_info *info,
	struct iw_freq *freq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	if (vap->iv_state == IEEE80211_S_RUN &&
	    vap->iv_opmode != IEEE80211_M_MONITOR) {
		/*
		 * NB: use curchan for monitor mode so you can see
		 *     manual scanning by apps like kismet.
		 */
		KASSERT(ic->ic_bsschan != IEEE80211_CHAN_ANYC,
			("bss channel not set"));
		freq->m = ic->ic_curchan->ic_freq;
	} else if (vap->iv_state != IEEE80211_S_INIT) {	/* e.g. when scanning */
		if (ic->ic_curchan != IEEE80211_CHAN_ANYC)
			freq->m = ic->ic_curchan->ic_freq;
		else
			freq->m = 0;
	} else if (ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
		freq->m = ic->ic_des_chan->ic_freq;
	} else {
		freq->m = 0;
	}

	freq->m *= 100000;
	freq->e = 1;

	return 0;
}

static int
ieee80211_ioctl_siwessid(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *ssid)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (vap->iv_opmode == IEEE80211_M_WDS)
		return -EOPNOTSUPP;

	if (data->flags == 0)		/* ANY */
		vap->iv_des_nssid = 0;
	else {
		if (data->length > IEEE80211_NWID_LEN)
			data->length = IEEE80211_NWID_LEN;
		/* NB: always use entry 0 */
		memcpy(vap->iv_des_ssid[0].ssid, ssid, data->length);
		vap->iv_des_ssid[0].len = data->length;
		vap->iv_des_nssid = 1;
		/*
		 * Deduct a trailing \0 since iwconfig passes a string
		 * length that includes this.  Unfortunately this means
		 * that specifying a string with multiple trailing \0's
		 * won't be handled correctly.  Not sure there's a good
		 * solution; the API is botched (the length should be
		 * exactly those bytes that are meaningful and not include
		 * extraneous stuff).
		 */
		if (data->length > 0 &&
		    vap->iv_des_ssid[0].ssid[data->length - 1] == '\0')
			vap->iv_des_ssid[0].len--;
	}

	if (vap->iv_opmode == IEEE80211_M_STA)
		return IS_UP_AUTO(vap) ? ieee80211_init(vap->iv_dev, RESCAN) : 0;
	else
		return IS_UP(vap->iv_dev) ? ieee80211_init(vap->iv_dev, RESCAN) : 0;
}

static int
ieee80211_ioctl_giwessid(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *essid)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (vap->iv_opmode == IEEE80211_M_WDS)
		return -EOPNOTSUPP;

	data->flags = 1;		/* active */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		if (vap->iv_des_nssid > 0) {
			if (data->length > vap->iv_des_ssid[0].len)
				data->length = vap->iv_des_ssid[0].len;
			memcpy(essid, vap->iv_des_ssid[0].ssid, data->length);
		} else
			data->length = 0;
	} else {
		if (vap->iv_des_nssid == 0 && vap->iv_bss) {
			if (data->length > vap->iv_bss->ni_esslen)
				data->length = vap->iv_bss->ni_esslen;
			memcpy(essid, vap->iv_bss->ni_essid, data->length);
		} else {
			if (data->length > vap->iv_des_ssid[0].len)
				data->length = vap->iv_des_ssid[0].len;
			memcpy(essid, vap->iv_des_ssid[0].ssid, data->length);
		}
	}
	return 0;
}

static int
ieee80211_ioctl_giwrange(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	//struct ieee80211_node *ni = vap->iv_bss;
	struct iw_range *range = (struct iw_range *) extra;
	struct ieee80211_rateset *rs;
	u_int8_t reported[IEEE80211_CHAN_BYTES];	/* XXX stack usage? */
	int i, r, chan_mode = 0;
	int step = 0;
	uint8_t sgi = 0;

	data->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	/* txpower (128 values, but will print out only IW_MAX_TXPOWER) */
	range->num_txpower = (ic->ic_txpowlimit >= 8) ? IW_MAX_TXPOWER : ic->ic_txpowlimit;
	step = ic->ic_txpowlimit / (2 * (IW_MAX_TXPOWER - 1));

	range->txpower[0] = 0;
	for (i = 1; i < IW_MAX_TXPOWER; i++)
		range->txpower[i] = (ic->ic_txpowlimit/2)
			- (IW_MAX_TXPOWER - i - 1) * step;

	range->txpower_capa = IW_TXPOW_DBM;

	if (vap->iv_opmode == IEEE80211_M_STA ||
	    vap->iv_opmode == IEEE80211_M_IBSS) {
		range->min_pmp = 1 * 1024;
		range->max_pmp = 65535 * 1024;
		range->min_pmt = 1 * 1024;
		range->max_pmt = 1000 * 1024;
		range->pmp_flags = IW_POWER_PERIOD;
		range->pmt_flags = IW_POWER_TIMEOUT;
		range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT |
			IW_POWER_UNICAST_R | IW_POWER_ALL_R;
	}

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 13;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT;
	range->min_retry = 0;
	range->max_retry = 255;

	range->num_frequency = 0;
	memset(reported, 0, sizeof(reported));
	for (i = 0; i < ic->ic_nchans; i++) {
		const struct ieee80211_channel *c = &ic->ic_channels[i];

		/* discard if previously reported (e.g. b/g) */
		if (isclr(reported, c->ic_ieee) &&
				isset(ic->ic_chan_active, c->ic_ieee)) {
			setbit(reported, c->ic_ieee);
			range->freq[range->num_frequency].i = c->ic_ieee;
			range->freq[range->num_frequency].m =
				ic->ic_channels[i].ic_freq * 100000;
			range->freq[range->num_frequency].e = 1;
			if (++range->num_frequency == IW_MAX_FREQUENCIES)
				break;
		}
	}

	/* Supported channels count */
	range->num_channels = range->num_frequency;

	/* Atheros' RSSI value is SNR: 0 -> 60 for old chipsets. Range
	 * for newer chipsets is unknown. This value is arbitarily chosen
	 * to give an indication that full rate will be available and to be
	 * a practicable maximum. */
	range->max_qual.qual  = 70;

	/* XXX: This should be updated to use the current noise floor. */
	/* These are negative full bytes.
	 * Min. quality is noise + 1 */
#define QNT_DEFAULT_NOISE 0
	range->max_qual.updated |= IW_QUAL_DBM;
	range->max_qual.level = QNT_DEFAULT_NOISE + 1;
	range->max_qual.noise = QNT_DEFAULT_NOISE;

	range->sensitivity = 1;

	range->max_encoding_tokens = IEEE80211_WEP_NKID;
	/* XXX query driver to find out supported key sizes */
	range->num_encoding_sizes = 3;
	range->encoding_size[0] = 5;		/* 40-bit */
	range->encoding_size[1] = 13;		/* 104-bit */
	range->encoding_size[2] = 16;		/* 128-bit */

	if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) {
		chan_mode = 1;
		sgi = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;
	} else {
		sgi = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
	}
	rs = &ic->ic_sup_rates[ic->ic_des_mode];
	range->num_bitrates = rs->rs_nrates;
	if (range->num_bitrates > MIN(IEEE80211_RATE_MAXSIZE, IW_MAX_BITRATES))
		range->num_bitrates = MIN(IEEE80211_RATE_MAXSIZE, IW_MAX_BITRATES);
	for (i = 0; i < range->num_bitrates; i++) {
			r = rs->rs_rates[i] & IEEE80211_RATE_VAL;

			/* Skip legacy rates */
			if(i >= (rs->rs_legacy_nrates))
			{
				r = ieee80211_mcs2rate(r, chan_mode, sgi, 0);
			}
			range->bitrate[i] = (r * 1000000) / 2;
	}

	/* estimated maximum TCP throughput values (bps) */
	range->throughput = 5500000;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

	/* Event capability (kernel) */
	IW_EVENT_CAPA_SET_KERNEL(range->event_capa);

	/* Event capability (driver) */
	if (vap->iv_opmode == IEEE80211_M_STA ||
		 vap->iv_opmode == IEEE80211_M_IBSS ||
		 vap->iv_opmode == IEEE80211_M_AHDEMO) {
		/* for now, only ibss, ahdemo, sta has this cap */
		IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWSCAN);
	}

	if (vap->iv_opmode == IEEE80211_M_STA) {
		/* for sta only */
		IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWAP);
		IW_EVENT_CAPA_SET(range->event_capa, IWEVREGISTERED);
		IW_EVENT_CAPA_SET(range->event_capa, IWEVEXPIRED);
	}

	/* this is used for reporting replay failure, which is used by the different encoding schemes */
	IW_EVENT_CAPA_SET(range->event_capa, IWEVCUSTOM);

	/* report supported WPA/WPA2 capabilities to userspace */
	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
			       IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;

	return 0;
}

static int
ieee80211_ioctl_setspy(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	/* save the list of node addresses */
	struct ieee80211vap *vap = netdev_priv(dev);
	struct sockaddr address[IW_MAX_SPY];
	unsigned int number = data->length;
	int i;

	if (number > IW_MAX_SPY)
		return -E2BIG;

	/* get the addresses into the driver */
	if (data->pointer) {
		if (copy_from_user(address, data->pointer,
		    sizeof(struct sockaddr) * number))
			return -EFAULT;
	} else {
		return -EFAULT;
	}

	/* copy the MAC addresses into a list */
	if (number > 0) {
		/* extract the MAC addresses */
		for (i = 0; i < number; i++)
			memcpy(&vap->iv_spy.mac[i * IEEE80211_ADDR_LEN],
				address[i].sa_data, IEEE80211_ADDR_LEN);
		/* init rssi timestamps */
		memset(vap->iv_spy.ts_rssi, 0, IW_MAX_SPY * sizeof(u_int32_t));
	}
	vap->iv_spy.num = number;

	return 0;
}

static int
ieee80211_ioctl_getspy(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	/*
	 * locate nodes by mac (ieee80211_find_node()),
	 * copy out rssi, set updated flag appropriately
	 */
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;
	struct ieee80211com *ic = vap->iv_ic;
	struct sockaddr *address;
	struct iw_quality *spy_stat;
	unsigned int number = vap->iv_spy.num;
	int i;

	address = (struct sockaddr *) extra;
	spy_stat = (struct iw_quality *) (extra + number * sizeof(struct sockaddr));

	for (i = 0; i < number; i++) {
		memcpy(address[i].sa_data, &vap->iv_spy.mac[i * IEEE80211_ADDR_LEN],
			IEEE80211_ADDR_LEN);
		address[i].sa_family = AF_PACKET;
	}

	/* locate a node, read its rssi, check if updated, convert to dBm */
	for (i = 0; i < number; i++) {
		ni = ieee80211_find_node(nt, &vap->iv_spy.mac[i * IEEE80211_ADDR_LEN]);
		/* check we are associated w/ this vap */
		if (ni) {
			if (ni->ni_vap == vap) {
				set_quality(&spy_stat[i], ni->ni_rssi, ic->ic_channoise);
				if (ni->ni_rstamp != vap->iv_spy.ts_rssi[i]) {
					vap->iv_spy.ts_rssi[i] = ni->ni_rstamp;
				} else {
					spy_stat[i].updated = 0;
				}
			}
			ieee80211_free_node(ni);
		} else {
			spy_stat[i].updated = IW_QUAL_ALL_INVALID;
		}
	}

	/* copy results to userspace */
	data->length = number;
	return 0;
}

/* Enhanced iwspy support */
static int
ieee80211_ioctl_setthrspy(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct iw_thrspy threshold;

	if (data->length != 1)
		return -EINVAL;

	/* get the threshold values into the driver */
	if (data->pointer) {
		if (copy_from_user(&threshold, data->pointer,
		    sizeof(struct iw_thrspy)))
			return -EFAULT;
        } else
		return -EINVAL;

	if (threshold.low.level == 0) {
		/* disable threshold */
		vap->iv_spy.thr_low = 0;
		vap->iv_spy.thr_high = 0;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
			"%s: disabled iw_spy threshold\n", __func__);
	} else {
		/* We are passed a signal level/strength - calculate
		 * corresponding RSSI values */
		/* XXX: We should use current noise value. */
		vap->iv_spy.thr_low = threshold.low.level + QNT_DEFAULT_NOISE;
		vap->iv_spy.thr_high = threshold.high.level + QNT_DEFAULT_NOISE;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
			"%s: enabled iw_spy threshold\n", __func__);
	}

	return 0;
}

static int
ieee80211_ioctl_getthrspy(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct iw_thrspy *threshold;

	threshold = (struct iw_thrspy *) extra;

	/* set threshold values */
	set_quality(&(threshold->low), vap->iv_spy.thr_low, ic->ic_channoise);
	set_quality(&(threshold->high), vap->iv_spy.thr_high, ic->ic_channoise);

	/* copy results to userspace */
	data->length = 1;

	return 0;
}

static int
ieee80211_ioctl_siwmode(struct net_device *dev, struct iw_request_info *info,
	__u32 *mode, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ifmediareq imr;
	int valid = 0;

	memset(&imr, 0, sizeof(imr));
	vap->iv_media.ifm_status((void *) vap, &imr);

	if (imr.ifm_active & IFM_IEEE80211_HOSTAP)
		valid = (*mode == IW_MODE_MASTER);
	else if (imr.ifm_active & IFM_IEEE80211_MONITOR)
		valid = (*mode == IW_MODE_MONITOR);
	else if (imr.ifm_active & IFM_IEEE80211_ADHOC)
		valid = (*mode == IW_MODE_ADHOC);
	else if (imr.ifm_active & IFM_IEEE80211_WDS)
		valid = (*mode == IW_MODE_REPEAT);
	else
		valid = (*mode == IW_MODE_INFRA);

	return valid ? 0 : -EINVAL;
}

static int
ieee80211_ioctl_giwmode(struct net_device *dev,	struct iw_request_info *info,
	__u32 *mode, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ifmediareq imr;

	memset(&imr, 0, sizeof(imr));
	vap->iv_media.ifm_status((void *) vap, &imr);

	if (imr.ifm_active & IFM_IEEE80211_HOSTAP)
		*mode = IW_MODE_MASTER;
	else if (imr.ifm_active & IFM_IEEE80211_MONITOR)
		*mode = IW_MODE_MONITOR;
	else if (imr.ifm_active & IFM_IEEE80211_ADHOC)
		*mode = IW_MODE_ADHOC;
	else if (imr.ifm_active & IFM_IEEE80211_WDS)
		*mode = IW_MODE_REPEAT;
	else
		*mode = IW_MODE_INFRA;
	return 0;
}

static int
ieee80211_ioctl_siwpower(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *wrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	/* XXX: These values, flags, and caps do not seem to be used elsewhere
	 * at all? */

	if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
		return -EOPNOTSUPP;

	if (wrq->disabled) {
		if (ic->ic_flags & IEEE80211_F_PMGTON)
			ic->ic_flags &= ~IEEE80211_F_PMGTON;
	} else {
		switch (wrq->flags & IW_POWER_MODE) {
		case IW_POWER_UNICAST_R:
		case IW_POWER_ALL_R:
		case IW_POWER_ON:
			if (wrq->flags & IW_POWER_PERIOD) {
				if (IEEE80211_BINTVAL_VALID(wrq->value))
					ic->ic_lintval = IEEE80211_MS_TO_TU(wrq->value);
				else
					return -EINVAL;
			}
			if (wrq->flags & IW_POWER_TIMEOUT)
				ic->ic_holdover = IEEE80211_MS_TO_TU(wrq->value);

				ic->ic_flags |= IEEE80211_F_PMGTON;
			break;
		default:
			return -EINVAL;
		}
	}

	return ic->ic_reset(ic);
}

static int
ieee80211_ioctl_giwpower(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	rrq->disabled = (ic->ic_flags & IEEE80211_F_PMGTON) == 0;
	if (!rrq->disabled) {
		switch (rrq->flags & IW_POWER_TYPE) {
		case IW_POWER_TIMEOUT:
			rrq->flags = IW_POWER_TIMEOUT;
			rrq->value = IEEE80211_TU_TO_MS(ic->ic_holdover);
			break;
		case IW_POWER_PERIOD:
			rrq->flags = IW_POWER_PERIOD;
			rrq->value = IEEE80211_TU_TO_MS(ic->ic_lintval);
			break;
		}
		rrq->flags |= IW_POWER_ALL_R;
	}
	return 0;
}

static int
ieee80211_ioctl_siwretry(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	if (rrq->disabled) {
		if (vap->iv_flags & IEEE80211_F_SWRETRY) {
			vap->iv_flags &= ~IEEE80211_F_SWRETRY;
			goto done;
		}
		return 0;
	}

	if ((vap->iv_caps & IEEE80211_C_SWRETRY) == 0)
		return -EOPNOTSUPP;
	if (rrq->flags == IW_RETRY_LIMIT) {
		if (rrq->value >= 0) {
			vap->iv_txmin = rrq->value;
			vap->iv_txmax = rrq->value;	/* XXX */
			vap->iv_txlifetime = 0;		/* XXX */
			vap->iv_flags |= IEEE80211_F_SWRETRY;
		} else {
			vap->iv_flags &= ~IEEE80211_F_SWRETRY;
		}
		return 0;
	}
done:
	return IS_UP(vap->iv_dev) ? ic->ic_reset(ic) : 0;
}

static int
ieee80211_ioctl_giwretry(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	rrq->disabled = (vap->iv_flags & IEEE80211_F_SWRETRY) == 0;
	if (!rrq->disabled) {
		switch (rrq->flags & IW_RETRY_TYPE) {
		case IW_RETRY_LIFETIME:
			rrq->flags = IW_RETRY_LIFETIME;
			rrq->value = IEEE80211_TU_TO_MS(vap->iv_txlifetime);
			break;
		case IW_RETRY_LIMIT:
			rrq->flags = IW_RETRY_LIMIT;
			switch (rrq->flags & IW_RETRY_MODIFIER) {
			case IW_RETRY_MIN:
				rrq->flags |= IW_RETRY_MAX;
				rrq->value = vap->iv_txmin;
				break;
			case IW_RETRY_MAX:
				rrq->flags |= IW_RETRY_MAX;
				rrq->value = vap->iv_txmax;
				break;
			}
			break;
		}
	}
	return 0;
}

static int
ieee80211_ioctl_siwtxpow(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	int fixed, disabled;

	fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED);
	if (!vap->iv_bss) {
		return 0;
	}

	disabled = (fixed && vap->iv_bss->ni_txpower == 0);
	if (rrq->disabled) {
		if (!disabled) {
			if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
				return -EOPNOTSUPP;
			ic->ic_flags |= IEEE80211_F_TXPOW_FIXED;
			vap->iv_bss->ni_txpower = 0;
			goto done;
		}
		return 0;
	}

	if (rrq->fixed) {
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			return -EOPNOTSUPP;
		if (rrq->flags != IW_TXPOW_DBM)
			return -EOPNOTSUPP;
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC) {
			if (ic->ic_bsschan->ic_maxregpower >= rrq->value &&
			    ic->ic_txpowlimit/2 >= rrq->value) {
 			        vap->iv_bss->ni_txpower = 2 * rrq->value;
				ic->ic_newtxpowlimit = 2 * rrq->value;
 				ic->ic_flags |= IEEE80211_F_TXPOW_FIXED;
 			} else
				return -EINVAL;
		} else {
			/*
			 * No channel set yet
			 */
			if (ic->ic_txpowlimit/2 >= rrq->value) {
				vap->iv_bss->ni_txpower = 2 * rrq->value;
				ic->ic_newtxpowlimit = 2 * rrq->value;
				ic->ic_flags |= IEEE80211_F_TXPOW_FIXED;
			}
			else
				return -EINVAL;
		}
	} else {
		if (!fixed)		/* no change */
			return 0;
		ic->ic_flags &= ~IEEE80211_F_TXPOW_FIXED;
	}
done:
	return ic->ic_reset(ic);
}

static int
ieee80211_ioctl_giwtxpow(struct net_device *dev, struct iw_request_info *info,
	struct iw_param *rrq, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	rrq->fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;
	rrq->disabled = (rrq->fixed && rrq->value == 0);
	rrq->flags = IW_TXPOW_DBM;

	if (vap->iv_bss) {
		/* ni_txpower is stored in 0.5dBm units */
		rrq->value = vap->iv_bss->ni_txpower >> 1;
	} else {
		rrq->value = 0;
	}

	return 0;
}

struct waplistreq {	/* XXX: not the right place for declaration? */
	struct ieee80211vap *vap;
	struct sockaddr addr[IW_MAX_AP];
	struct iw_quality qual[IW_MAX_AP];
	int i;
};

static int
waplist_cb(void *arg, const struct ieee80211_scan_entry *se)
{
	struct waplistreq *req = arg;
	int i = req->i;

	if (i >= IW_MAX_AP)
		return 0;
	req->addr[i].sa_family = ARPHRD_ETHER;
	if (req->vap->iv_opmode == IEEE80211_M_HOSTAP)
		IEEE80211_ADDR_COPY(req->addr[i].sa_data, se->se_macaddr);
	else
		IEEE80211_ADDR_COPY(req->addr[i].sa_data, se->se_bssid);
	set_quality(&req->qual[i], se->se_rssi, QNT_DEFAULT_NOISE);
	req->i = i + 1;

	return 0;
}

static int
ieee80211_ioctl_iwaplist(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct waplistreq req;		/* XXX off stack */

	req.vap = vap;
	req.i = 0;
	ieee80211_scan_iterate(ic, waplist_cb, &req);

	data->length = req.i;
	memcpy(extra, &req.addr, req.i * sizeof(req.addr[0]));
	data->flags = 1;		/* signal quality present (sort of) */
	memcpy(extra + req.i * sizeof(req.addr[0]), &req.qual,
		req.i * sizeof(req.qual[0]));

	return 0;
}

#ifdef SIOCGIWSCAN
static int is_dfs_channel_available(struct ieee80211com *ic)
{
	struct ieee80211_channel *c;
	struct ieee80211_channel *secondary_chan = NULL;
	int dfs_channel_available = 0;
	int i;

	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c == NULL || isclr(ic->ic_chan_active, c->ic_ieee))
			continue;
		if (c->ic_flags & IEEE80211_CHAN_DFS) {
			if (c->ic_flags & IEEE80211_CHAN_RADAR) {
				continue;
			} else {
				if (c->ic_flags & IEEE80211_CHAN_HT40) {
					secondary_chan = NULL;
					if (c->ic_flags & IEEE80211_CHAN_HT40D) {
						secondary_chan = c - 1;
					} else if (c->ic_flags & IEEE80211_CHAN_HT40U) {
						secondary_chan = c + 1;
					}

					if (secondary_chan && (secondary_chan->ic_flags & IEEE80211_CHAN_RADAR))
						continue;
				}
				dfs_channel_available = 1;
				break;
			}
		}
	}

	return dfs_channel_available;
}

static int
ieee80211_ioctl_siwscan(struct net_device *dev,	struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	uint32_t scan_flags = 0;
	uint16_t pick_flags = 0;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	int dfs_channel_available;

	/*
	 * XXX don't permit a scan to be started unless we
	 * know the device is ready.  For the moment this means
	 * the device is marked up as this is the required to
	 * initialize the hardware.  It would be better to permit
	 * scanning prior to being up but that'll require some
	 * changes to the infrastructure.
	 */
	if (!IS_UP(vap->iv_dev))
		return -ENETDOWN;	/* XXX */

	if ((ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
			IEEE80211_IS_CHAN_CAC_IN_PROGRESS(ic->ic_bsschan)) ||
			ic->ic_ocac.ocac_running)
		return -EBUSY;

	if (!ieee80211_should_scan(vap))
		return -EAGAIN;


	if (ic->ic_get_init_cac_duration(ic) > 0) {
		return -EAGAIN;
	}

	/* XXX always manual... */
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		"%s: active scan request\n", __func__);
	preempt_scan(dev, 100, 100);
	ss->is_scan_valid = 1;
	ic->ic_csw_reason = IEEE80211_CSW_REASON_SCAN;

	if (data && (data->flags & IW_SCAN_THIS_ESSID)) {
		struct iw_scan_req req;
		struct ieee80211_scan_ssid ssid;
		int copyLength;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: SCAN_THIS_ESSID requested\n", __func__);
		if (data->length > sizeof req) {
			copyLength = sizeof req;
		} else {
			copyLength = data->length;
		}
		memset(&req, 0, sizeof req);
		if (copy_from_user(&req, data->pointer, copyLength))
			return -EFAULT;
		memcpy(&ssid.ssid, req.essid, sizeof ssid.ssid);
		ssid.len = req.essid_len;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				  "%s: requesting scan of essid '%s'\n", __func__, ssid.ssid);
		(void) ieee80211_start_scan(vap,
			IEEE80211_SCAN_ACTIVE |
			IEEE80211_SCAN_NOPICK |
			IEEE80211_SCAN_ONCE |
			(IEEE80211_USE_QTN_BGSCAN(vap) ? IEEE80211_SCAN_QTN_BGSCAN: 0) |
			((vap->iv_opmode == IEEE80211_M_HOSTAP) ? IEEE80211_SCAN_FLUSH : 0),
			IEEE80211_SCAN_FOREVER,
			1, &ssid);
		return 0;
	}

	if (data && data->pointer) {
		if (copy_from_user(&pick_flags, data->pointer, sizeof(pick_flags)))
			return -EFAULT;

		u_int16_t flags_tmp = pick_flags & IEEE80211_PICK_ALGORITHM_MASK;
		u_int16_t flags_bg_scan_mode = 0;

		/*
		 * For DFS reentry, check if any DFS channel is available.
		 * If not, skip channel scan and return directly.
		 */
		if (pick_flags & (IEEE80211_PICK_REENTRY | IEEE80211_PICK_DFS)) {
			dfs_channel_available = is_dfs_channel_available(ic);
			if (dfs_channel_available == 0) {
				printk(KERN_WARNING "%s: All DFS channels are in non-occupy list, skip DFS reentry!\n",
						vap->iv_dev->name);
				return -EAGAIN;
			}
		}

		if (flags_tmp == IEEE80211_PICK_REENTRY || flags_tmp == IEEE80211_PICK_CLEAREST) {
			scan_flags = IEEE80211_SCAN_FLUSH;
			/* Go out of idle state and delay idle state check.*/
			if (ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] >= BOARD_PM_LEVEL_IDLE) {
				pm_qos_update_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_WLAN, BOARD_PM_LEVEL_NO);
				ieee80211_pm_queue_work(ic);
			}
		}
		else if (flags_tmp == IEEE80211_PICK_NOPICK) {
			scan_flags = IEEE80211_SCAN_NOPICK;
		}
#ifdef QTN_BG_SCAN
		else if (flags_tmp == IEEE80211_PICK_NOPICK_BG) {
			scan_flags = IEEE80211_SCAN_NOPICK;
			if ((vap->iv_opmode == IEEE80211_M_HOSTAP && ic->ic_sta_assoc > 0) ||
					(vap->iv_opmode == IEEE80211_M_STA && vap->iv_state == IEEE80211_S_RUN)) {
				scan_flags |= IEEE80211_SCAN_QTN_BGSCAN;
				flags_bg_scan_mode = pick_flags & IEEE80211_PICK_BG_MODE_MASK;
				if (IS_MULTIPLE_BITS_SET(flags_bg_scan_mode)) {
					/* use auto mode if multiple modes are set */
					flags_bg_scan_mode = 0;
				}
			}
		}

#endif /* QTN_BG_SCAN */
		if (pick_flags & IEEE80211_PICK_SCAN_FLUSH) {
			scan_flags |= IEEE80211_SCAN_FLUSH;
		}

		/*
		 * set pick flags before start scanning, and remember to clean it when selection channel done
		 * only for AP mode
		 */
		ss->ss_pick_flags = (pick_flags & (~IEEE80211_PICK_CONTROL_MASK)) | flags_bg_scan_mode;
	}

	if ((vap->iv_opmode == IEEE80211_M_STA) && (vap->iv_state < IEEE80211_S_RUN))
		scan_flags |= IEEE80211_SCAN_FLUSH;

	if (IEEE80211_USE_QTN_BGSCAN(vap))
		scan_flags |= IEEE80211_SCAN_QTN_BGSCAN;

	(void) ieee80211_start_scan(vap, IEEE80211_SCAN_ACTIVE |
			scan_flags | IEEE80211_SCAN_ONCE,
		IEEE80211_SCAN_FOREVER,
		/* XXX use ioctl params */
		vap->iv_des_nssid, vap->iv_des_ssid);
	return 0;
}

/*
 * Encode a WPA or RSN information element as a custom
 * element using the hostap format.
 */
static u_int
encode_ie(void *buf, size_t bufsize, const u_int8_t *ie, size_t ielen,
	const char *leader, size_t leader_len)
{
	char *p;
	int i;

	if (bufsize < leader_len)
		return 0;
	p = buf;
	memcpy(p, leader, leader_len);
	bufsize -= leader_len;
	p += leader_len;
	for (i = 0; i < ielen && bufsize > 2; i++) {
		p += sprintf(p, "%02x", ie[i]);
		bufsize -= 2;
	}
	return (i == ielen ? p - (char *)buf : 0);
}

struct iwscanreq {		/* XXX: right place for this declaration? */
	struct ieee80211vap *vap;
	char *current_ev;
	char *end_buf;
	int mode;
	struct iw_request_info *info;
};

/*
 * Recalculate the RSSI of MBS/RBS
 * Make sure the RSSI has the following priority.
 *	MBS in best rate range has the highest level RSSI.
 *	RBS in best rate range has the second high level RSSI.
 *	MBS not in best rate range has the third high level RSSI.
 *	RBS not in best rate range has the fourth high level RSSI.
 */
static int8_t
ieee80211_calcu_extwds_node_rssi(struct ieee80211com *ic,
	const struct ieee80211_scan_entry *se)
{
	int8_t rssi = se->se_rssi;
	if (se->se_ext_role == IEEE80211_EXTENDER_ROLE_NONE)
		return rssi;

	if (se->se_ext_role == IEEE80211_EXTENDER_ROLE_MBS) {
		if (rssi >= ic->ic_extender_mbs_best_rssi) {
			rssi = IEEE80211_EXTWDS_MBS_BEST_RATE_RSSI;
		} else {
			rssi = (rssi - IEEE80211_EXTWDS_MIN_PSEUDO_RSSI) *
				IEEE80211_EXTWDS_BEST_RATE_BDRY_RSSI /
				(ic->ic_extender_mbs_best_rssi -
					IEEE80211_EXTWDS_MIN_PSEUDO_RSSI) *
				ic->ic_extender_mbs_wgt / 10;
		}
	} else if (se->se_ext_role == IEEE80211_EXTENDER_ROLE_RBS) {
		if (rssi >= ic->ic_extender_rbs_best_rssi) {
			rssi = (rssi - ic->ic_extender_rbs_best_rssi) *
				(IEEE80211_EXTWDS_MAX_PSEUDO_RSSI -
					IEEE80211_EXTWDS_BEST_RATE_BDRY_RSSI) /
				(IEEE80211_EXTWDS_MAX_PSEUDO_RSSI -
					ic->ic_extender_rbs_best_rssi) +
				IEEE80211_EXTWDS_BEST_RATE_BDRY_RSSI;
		} else {
			rssi = (rssi - IEEE80211_EXTWDS_MIN_PSEUDO_RSSI) *
				IEEE80211_EXTWDS_BEST_RATE_BDRY_RSSI /
				(ic->ic_extender_rbs_best_rssi -
					IEEE80211_EXTWDS_MIN_PSEUDO_RSSI) *
				ic->ic_extender_rbs_wgt / 10;
		}
	}

	return rssi;
}

static int
giwscan_cb(void *arg, const struct ieee80211_scan_entry *se)
{
	struct iwscanreq *req = arg;
	struct ieee80211vap *vap = req->vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct iw_request_info *info = req->info;
	char *current_ev = req->current_ev;
	char *end_buf = req->end_buf;
	char *last_ev;
#define MAX_IE_LENGTH 257
	char buf[MAX_IE_LENGTH];
#ifndef IWEVGENIE
	static const char rsn_leader[] = IEEE80211_IE_LEADER_STR_RSN;
	static const char wpa_leader[] = IEEE80211_IE_LEADER_STR_WPA;
#endif
	struct iw_event iwe;
	char *current_val;
	int j;
	u_int8_t chan_mode = 0;
	uint8_t sgi = 0;
	u_int8_t k, r;
	u_int16_t mask;
	struct ieee80211_ie_htcap *htcap;
	struct ieee80211_ie_vhtcap *vhtcap;
	int rate_ie_exist = 0;

	if (current_ev >= end_buf)
		return E2BIG;

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		IEEE80211_ADDR_COPY(iwe.u.ap_addr.sa_data, se->se_macaddr);
	else
		IEEE80211_ADDR_COPY(iwe.u.ap_addr.sa_data, se->se_bssid);
	current_ev = iwe_stream_add_event(info, current_ev, end_buf, &iwe, IW_EV_ADDR_LEN);

	/* We ran out of space in the buffer. */
	if (last_ev == current_ev)
	  return E2BIG;

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.flags = 1;
	iwe.u.data.length = se->se_ssid[1];
	current_ev = iwe_stream_add_point(info, current_ev,
					  end_buf, &iwe, (char *)se->se_ssid + 2);

	/* We ran out of space in the buffer. */
	if (last_ev == current_ev)
	  return E2BIG;

	if (se->se_capinfo & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) {
		memset(&iwe, 0, sizeof(iwe));
		last_ev = current_ev;
		iwe.cmd = SIOCGIWMODE;
		iwe.u.mode = se->se_capinfo & IEEE80211_CAPINFO_ESS ?
			IW_MODE_MASTER : IW_MODE_ADHOC;
		current_ev = iwe_stream_add_event(info, current_ev,
			end_buf, &iwe, IW_EV_UINT_LEN);

		/* We ran out of space in the buffer. */
		if (last_ev == current_ev)
		  return E2BIG;
	}

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = se->se_chan->ic_freq * 100000;
	iwe.u.freq.e = 1;
	current_ev = iwe_stream_add_event(info, current_ev,
		end_buf, &iwe, IW_EV_FREQ_LEN);

	/* We ran out of space in the buffer. */
	if (last_ev == current_ev)
	  return E2BIG;

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = IWEVQUAL;
	set_quality(&iwe.u.qual, se->se_rssi, QNT_DEFAULT_NOISE);
	/*
	 * Assign the real RSSI to 'level' for MBS/RBS, so wpa_supplicant
	 * can run roaming between MBS and RBS base on the 'level' value.
	 */
	if (se->se_ext_role != IEEE80211_EXTENDER_ROLE_NONE) {
		iwe.u.qual.qual = ieee80211_calcu_extwds_node_rssi(ic, se);
		iwe.u.qual.level = iwe.u.qual.qual - IEEE80211_EXTWDS_RSSI_TRANSITON_FACTOR;
	}
	current_ev = iwe_stream_add_event(info, current_ev,
		end_buf, &iwe, IW_EV_QUAL_LEN);

	/* We ran out of space in the buffer */
	if (last_ev == current_ev)
	  return E2BIG;

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = SIOCGIWENCODE;
	if (se->se_capinfo & IEEE80211_CAPINFO_PRIVACY)
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	iwe.u.data.length = 0;
	current_ev = iwe_stream_add_point(info, current_ev, end_buf, &iwe, "");

	/* We ran out of space in the buffer. */
	if (last_ev == current_ev)
	  return E2BIG;

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = SIOCGIWRATE;
	current_val = current_ev + IW_EV_LCP_LEN;
	/* NB: not sorted, does it matter? */
	for (j = 0; j < se->se_rates[1]; j++) {
		int r = se->se_rates[2 + j] & IEEE80211_RATE_VAL;
		if (r != 0) {
			iwe.u.bitrate.value = r * (1000000 / 2);
			current_val = iwe_stream_add_value(info, current_ev,
				current_val, end_buf, &iwe,
				IW_EV_PARAM_LEN);
			rate_ie_exist++;
		}
	}
	for (j = 0; j < se->se_xrates[1]; j++) {
		int r = se->se_xrates[2+j] & IEEE80211_RATE_VAL;
		if (r != 0) {
			iwe.u.bitrate.value = r * (1000000 / 2);
			current_val = iwe_stream_add_value(info, current_ev,
				current_val, end_buf, &iwe,
				IW_EV_PARAM_LEN);
			rate_ie_exist++;
		}
	}

	htcap = (struct ieee80211_ie_htcap *)se->se_htcap_ie;
	if (htcap) {
		r = 0;
		if (htcap->hc_cap[0] & IEEE80211_HTCAP_C_CHWIDTH40) {
			chan_mode = 1;
			sgi = htcap->hc_cap[0] & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;
		} else {
			chan_mode = 0;
			sgi = htcap->hc_cap[0] & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
		}
		for (j = IEEE80211_HT_MCSSET_20_40_NSS1; j <= IEEE80211_HT_MCSSET_20_40_NSS4; j++) {
			mask = 1;
			for (k = 0; k < 8; k++, r++) {
				if (htcap->hc_mcsset[j] & mask) {
					/* Copy HT rates */
					iwe.u.bitrate.value = ieee80211_mcs2rate(r, chan_mode, sgi, 0) * (1000000 / 2);
					current_val = iwe_stream_add_value(info,
							current_ev,
							current_val,
							end_buf,
							&iwe,
							IW_EV_PARAM_LEN);
					rate_ie_exist++;
				}
				mask = mask << 1;
			}
		}
	}

	vhtcap = (struct ieee80211_ie_vhtcap *)se->se_vhtcap_ie;
	if (vhtcap) {
		u_int16_t mcsmap = 0;
		r = 0;
		/* 80+80 or 160 Mhz */
		if (IEEE80211_VHTCAP_GET_CHANWIDTH(vhtcap)) {
			chan_mode = 1;
			sgi = IEEE80211_VHTCAP_GET_SGI_160MHZ(vhtcap);
		} else {
			chan_mode = 0;
			sgi = IEEE80211_VHTCAP_GET_SGI_80MHZ(vhtcap);
		}
		mask = 0x3;
		mcsmap = (u_int16_t)IEEE80211_VHTCAP_GET_TX_MCS_NSS(vhtcap);
		for (k = 0; k < 8; k++) {
			if ((mcsmap & mask) != mask) {
				int m;
				int val = (mcsmap & mask)>>(k * 2);
				r = (val == 2) ? 9: (val == 1) ? 8 : 7;
				/* Copy HT rates */
				for (m = 0; m <= r; m++) {
					iwe.u.bitrate.value =
						(ieee80211_mcs2rate(m, chan_mode, sgi, 1)
						* (1000000 / 2)) * (k+1);
					current_val = iwe_stream_add_value(info,
						current_ev,
						current_val,
						end_buf,
						&iwe,
						IW_EV_PARAM_LEN);
					rate_ie_exist++;
				}
				mask = mask << 2;
			} else {
				break;
			}
		}
	}

	/* remove fixed header if no rates were added */
	if ((current_val - current_ev) > IW_EV_LCP_LEN) {
		current_ev = current_val;
	} else {
	  /* We ran out of space in the buffer. */
	  if (last_ev == current_ev && rate_ie_exist)
	    return E2BIG;
	}

	memset(&iwe, 0, sizeof(iwe));
	last_ev = current_ev;
	iwe.cmd = IWEVCUSTOM;
	snprintf(buf, sizeof(buf), "bcn_int=%d", se->se_intval);
	iwe.u.data.length = strlen(buf);
	current_ev = iwe_stream_add_point(info, current_ev, end_buf, &iwe, buf);

	/* We ran out of space in the buffer. */
	if (last_ev == current_ev)
	  return E2BIG;

	memset(&iwe, 0, sizeof(iwe));
	memset(buf, 0, sizeof(buf));
	last_ev = current_ev;
	iwe.cmd = IWEVCUSTOM;
	snprintf(buf, sizeof(buf) - 1, IEEE80211_IE_LEADER_STR_EXT_ROLE"%d", se->se_ext_role);
	iwe.u.data.length = strlen(buf) + 1;
	current_ev = iwe_stream_add_point(info, current_ev, end_buf, &iwe, buf);

	/* We ran out of space in the buffer. */
	if (last_ev == current_ev)
		return E2BIG;

	if (se->se_rsn_ie != NULL) {
	  last_ev = current_ev;
#ifdef IWEVGENIE
		memset(&iwe, 0, sizeof(iwe));
		if ((se->se_rsn_ie[1] + 2) > MAX_IE_LENGTH)
			return E2BIG;
		memcpy(buf, se->se_rsn_ie, se->se_rsn_ie[1] + 2);
		iwe.cmd = IWEVGENIE;
		iwe.u.data.length = se->se_rsn_ie[1] + 2;
#else
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = IWEVCUSTOM;
		if (se->se_rsn_ie[0] == IEEE80211_ELEMID_RSN)
			iwe.u.data.length = encode_ie(buf, sizeof(buf),
				se->se_rsn_ie, se->se_rsn_ie[1] + 2,
				rsn_leader, sizeof(rsn_leader) - 1);
#endif
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info,
					current_ev, end_buf, &iwe, buf);

			/* We ran out of space in the buffer */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}

	if (se->se_wpa_ie != NULL) {
	  last_ev = current_ev;
#ifdef IWEVGENIE
		memset(&iwe, 0, sizeof(iwe));
		if ((se->se_wpa_ie[1] + 2) > MAX_IE_LENGTH)
			return E2BIG;
		memcpy(buf, se->se_wpa_ie, se->se_wpa_ie[1] + 2);
		iwe.cmd = IWEVGENIE;
		iwe.u.data.length = se->se_wpa_ie[1] + 2;
#else
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = encode_ie(buf, sizeof(buf),
			se->se_wpa_ie, se->se_wpa_ie[1] + 2,
			wpa_leader, sizeof(wpa_leader) - 1);
#endif
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info, current_ev,
					end_buf, &iwe, buf);

			/* We ran out of space in the buffer. */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}

	if (se->se_wme_ie != NULL) {
		static const char wme_leader[] = IEEE80211_IE_LEADER_STR_WME;

		memset(&iwe, 0, sizeof(iwe));
		last_ev = current_ev;
		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = encode_ie(buf, sizeof(buf),
			se->se_wme_ie, se->se_wme_ie[1] + 2,
			wme_leader, sizeof(wme_leader) - 1);
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info, current_ev,
					end_buf, &iwe, buf);

			/* We ran out of space in the buffer. */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}
	if (se->se_wsc_ie != NULL) {
		last_ev = current_ev;
#ifdef IWEVGENIE
		memset(&iwe, 0, sizeof(iwe));
		if ((se->se_wsc_ie[1] + 2) > sizeof(buf))
			return E2BIG;
		memcpy(buf, se->se_wsc_ie, se->se_wsc_ie[1] + 2);
		iwe.cmd = IWEVGENIE;
		iwe.u.data.length = se->se_wsc_ie[1] + 2;
#endif
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info, current_ev,
					end_buf, &iwe, buf);

			/* We ran out of space in the buffer. */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}
	if (se->se_ath_ie != NULL) {
		static const char ath_leader[] = IEEE80211_IE_LEADER_STR_ATH;

		memset(&iwe, 0, sizeof(iwe));
		last_ev = current_ev;
		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = encode_ie(buf, sizeof(buf),
			se->se_ath_ie, se->se_ath_ie[1] + 2,
			ath_leader, sizeof(ath_leader) - 1);
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info, current_ev,
					end_buf, &iwe, buf);

			/* We ran out of space in the buffer. */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}

	if (se->se_htcap_ie != NULL) {
		static const char htcap_leader[] = IEEE80211_IE_LEADER_STR_HTCAP;

		memset(&iwe, 0, sizeof(iwe));
		last_ev = current_ev;
		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = encode_ie(buf, sizeof(buf),
			se->se_htcap_ie, se->se_htcap_ie[1] + 2,
			htcap_leader, sizeof(htcap_leader) - 1);
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info, current_ev,
					end_buf, &iwe, buf);

			/* We ran out of space in the buffer. */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}

	if (se->se_vhtcap_ie != NULL) {
		static const char vhtcap_leader[] = IEEE80211_IE_LEADER_STR_VHTCAP;

		memset(&iwe, 0, sizeof(iwe));
		last_ev = current_ev;
		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = encode_ie(buf, sizeof(buf),
			se->se_vhtcap_ie, se->se_vhtcap_ie[1] + 2,
			vhtcap_leader, sizeof(vhtcap_leader) - 1);
		if (iwe.u.data.length != 0) {
			current_ev = iwe_stream_add_point(info, current_ev,
					end_buf, &iwe, buf);

			/* We ran out of space in the buffer. */
			if (last_ev == current_ev)
			  return E2BIG;
		}
	}

	req->current_ev = current_ev;

	return 0;
}

static int
ieee80211_ioctl_giwscan(struct net_device *dev,	struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct iwscanreq req;
	int res = 0;

	ieee80211_dump_scan_res(ic->ic_scan);

	req.vap = vap;
	req.current_ev = extra;
	if (data->length == 0) {
	  req.end_buf = extra + IW_SCAN_MAX_DATA;
	} else {
	  req.end_buf = extra + data->length;
	}

	/*
	 * NB: This is no longer needed, as long as the caller supports
	 * large scan results.
	 *
	 * Don't need do WPA/RSN sort any more since the original scan list
	 * has been sorted.
	 */
	req.info = info;
	res = ieee80211_scan_iterate(ic, giwscan_cb, &req);
	data->length = req.current_ev - extra;

	if (res != 0) {
	  return -res;
	}

	return res;
}
#endif /* SIOCGIWSCAN */

static int
cipher2cap(int cipher)
{
	switch (cipher) {
	case IEEE80211_CIPHER_WEP:	return IEEE80211_C_WEP;
	case IEEE80211_CIPHER_AES_OCB:	return IEEE80211_C_AES;
	case IEEE80211_CIPHER_AES_CCM:	return IEEE80211_C_AES_CCM;
	case IEEE80211_CIPHER_CKIP:	return IEEE80211_C_CKIP;
	case IEEE80211_CIPHER_TKIP:	return IEEE80211_C_TKIP;
	}
	return 0;
}

#define	IEEE80211_MODE_TURBO_STATIC_A	IEEE80211_MODE_MAX

static int
ieee80211_convert_mode(const char *mode)
{
#define TOUPPER(c) ((((c) > 0x60) && ((c) < 0x7b)) ? ((c) - 0x20) : (c))
	static const struct {
		char *name;
		int mode;
	} mappings[] = {
		/* NB: need to order longest strings first for overlaps */
		{ "11AST" , IEEE80211_MODE_TURBO_STATIC_A },
		{ "AUTO"  , IEEE80211_MODE_AUTO },
		{ "11A"   , IEEE80211_MODE_11A },
		{ "11B"   , IEEE80211_MODE_11B },
		{ "11G"   , IEEE80211_MODE_11G },
		{ "11NG"   , IEEE80211_MODE_11NG },
		{ "11NG40"   , IEEE80211_MODE_11NG_HT40PM},
		{ "11NA"   , IEEE80211_MODE_11NA },
		{ "11NA40"   , IEEE80211_MODE_11NA_HT40PM},
		{ "11AC"   , IEEE80211_MODE_11AC_VHT20PM},
		{ "11AC40"   , IEEE80211_MODE_11AC_VHT40PM},
		{ "11AC80"   , IEEE80211_MODE_11AC_VHT80PM},
		{ "11AC160"   , IEEE80211_MODE_11AC_VHT160PM},
		{ "FH"    , IEEE80211_MODE_FH },
		{ "0"     , IEEE80211_MODE_AUTO },
		{ "1"     , IEEE80211_MODE_11A },
		{ "2"     , IEEE80211_MODE_11B },
		{ "3"     , IEEE80211_MODE_11G },
		{ "4"     , IEEE80211_MODE_FH },
		{ "5"     , IEEE80211_MODE_TURBO_STATIC_A },
		{ "11AC80EDGE+", IEEE80211_MODE_11AC_VHT80PM},
		{ "11AC80CNTR+", IEEE80211_MODE_11AC_VHT80PM},
		{ "11AC80CNTR-", IEEE80211_MODE_11AC_VHT80PM},
		{ "11AC80EDGE-", IEEE80211_MODE_11AC_VHT80PM},
		{ "11ACONLY",	IEEE80211_MODE_11AC_VHT20PM},
		{ "11ACONLY40",	IEEE80211_MODE_11AC_VHT40PM},
		{ "11ACONLY80",	IEEE80211_MODE_11AC_VHT80PM},
		{ "11NONLY",	IEEE80211_MODE_11NA},
		{ "11NONLY40",	IEEE80211_MODE_11NA_HT40PM},
		{ NULL }
	};
	int i, j;
	const char *cp;

	for (i = 0; mappings[i].name != NULL; i++) {
		cp = mappings[i].name;
		for (j = 0; j < strlen(mode) + 1; j++) {
			/* convert user-specified string to upper case */
			if (TOUPPER(mode[j]) != cp[j])
				break;
			if (cp[j] == '\0')
				return mappings[i].mode;
		}
	}
	return -1;
#undef TOUPPER
}

static int
ieee80211_ioctl_postevent(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *wri, char *extra)
{
	char s[256];
	static const char *tag = QEVT_COMMON_PREFIX;
	memset(s, 0, sizeof(s));
	if (wri->length > sizeof(s))		/* silently truncate */
		wri->length = sizeof(s);
	if (copy_from_user(s, wri->pointer, wri->length))
		return -EINVAL;
	s[sizeof(s)-1] = '\0';			/* ensure null termination */

	/* We demux - one message is "WPA-PORT-ENABLE", the rest should be pushed
	 * via wireless_send_event.
	 */
	if (!strncmp(s, "WPA-PORT-ENABLE", sizeof("WPA-PORT-ENABLE")-1)) {
		struct ieee80211vap *vap = netdev_priv(dev);
		struct ieee80211_node *ni = vap->iv_bss;
		if (ni) {
			_ieee80211_node_authorize(ni);
		}
	} else {
		/* In this case, we assume the message from userspace has the correct format */
		ieee80211_eventf(dev, "%s%s", tag, s);
	}
	return 0;
}

static int
ieee80211_ioctl_txeapol(struct net_device *dev, struct iw_request_info *info,
        struct iw_point *wri, char *extra)
{
	char *buf;

	buf = ieee80211_malloc(wri->length, M_NOWAIT);
	if (buf == NULL)
		return -EINVAL;

	if (copy_from_user(buf, wri->pointer, wri->length)) {
		ieee80211_free(buf);
		return -EINVAL;
	}

	ieee80211_eap_output(dev, buf, wri->length);

	ieee80211_free(buf);

	return 0;
}

/*
 * Blacklist a station that is in the hostapd MAC filtering list.
 */
static void
ieee80211_blacklist_add(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (vap->iv_blacklist_timeout > 0) {
		ni->ni_blacklist_timeout = jiffies + vap->iv_blacklist_timeout;
		/* Corner case - can't use zero! */
		if (ni->ni_blacklist_timeout == 0) {
		    ni->ni_blacklist_timeout = 1;
		}
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
			"[%s] blacklisted\n",
			ether_sprintf(ni->ni_macaddr));
	}
}

static void
ieee80211_domlme(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211req_mlme *mlme = arg;

	if ((mlme->im_op != IEEE80211_MLME_DEBUG_CLEAR) &&
	    (ni->ni_associd != 0)) {
		/* This status is only used internally, for blacklisting */
		if (mlme->im_reason == IEEE80211_STATUS_DENIED) {
			ieee80211_blacklist_add(ni);
			mlme->im_reason = IEEE80211_REASON_UNSPECIFIED;
		}

		IEEE80211_SEND_MGMT(ni,
			mlme->im_op == IEEE80211_MLME_DEAUTH ?
				IEEE80211_FC0_SUBTYPE_DEAUTH :
				IEEE80211_FC0_SUBTYPE_DISASSOC,
			mlme->im_reason);

		/*
		 * Ensure that the deauth/disassoc frame is sent
		 * before the node is deleted.
		 */
		if (mlme->im_reason == IEEE80211_REASON_MIC_FAILURE)
			ieee80211_safe_wait_ms(150, !in_interrupt());
	}
	if (!(IEEE80211_ADDR_EQ(ni->ni_macaddr, ni->ni_bssid))) {
		ieee80211_node_leave(ni);
	}
}

/**
 * Common routine to force reassociation due to fundamental changes in config.
 */
static void
ieee80211_wireless_reassoc(struct ieee80211vap *vap, int debug, int rescan)
{
	struct net_device *dev = vap->iv_dev;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211req_mlme mlme;

	if (vap->iv_state == IEEE80211_S_INIT)
	{
		return;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"%s Forcing reassociation\n", __func__);
	switch (vap->iv_opmode) {
		case IEEE80211_M_STA:
			if (rescan)
			{
				ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
			}
			else
			{
				ieee80211_new_state(vap, IEEE80211_S_ASSOC, 0);
			}
			break;
		case IEEE80211_M_HOSTAP:
			if (vap->iv_state == IEEE80211_S_RUN) {
				ic->ic_beacon_update(vap);
				if (debug)
				{
					mlme.im_op = IEEE80211_MLME_DEBUG_CLEAR;
					mlme.im_reason = IEEE80211_REASON_UNSPECIFIED;
				}
				else
				{
					mlme.im_op = IEEE80211_MLME_DISASSOC;
					mlme.im_reason = IEEE80211_REASON_UNSPECIFIED;
				}
				ieee80211_iterate_dev_nodes(dev, &ic->ic_sta, ieee80211_domlme, &mlme, 1);
			}
			break;
		default:
			break;
	}
}

static struct ieee80211_channel *
find_alt_primary_chan_11ac80(struct ieee80211com *ic, char *mode)
{
	int chan_offset = 0, chan;
	struct ieee80211_channel *c = NULL;

	if (!strncasecmp(mode ,"11ac80Edge+", strlen(mode))) {
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LL)
			chan_offset = 0;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LU)
			chan_offset = -1;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UL)
			chan_offset = -2;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU)
			chan_offset = -3;
	} else if (!strncasecmp(mode ,"11ac80Cntr+", strlen(mode))) {
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LL)
			chan_offset = 1;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LU)
			chan_offset = 0;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UL)
			chan_offset = -1;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU)
			chan_offset = -2;
	} else if (!strncasecmp(mode ,"11ac80Cntr-", strlen(mode))) {
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LL)
			chan_offset = 2;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LU)
			chan_offset = 1;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UL)
			chan_offset = 0;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU)
			chan_offset = -1;
	} else if (!strncasecmp(mode ,"11ac80Edge-", strlen(mode))) {
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LL)
			chan_offset = 3;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_LU)
			chan_offset = 2;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UL)
			chan_offset = 1;
		if (ic->ic_curchan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU)
			chan_offset = 0;
	}

	chan = ic->ic_curchan->ic_ieee + (4 * chan_offset);
	if (chan >= IEEE80211_DEFAULT_5_GHZ_CHANNEL) {
		c = findchannel_any(ic, chan, ic->ic_curmode);
	}
	return c;
}

static void
update_sta_profile(char *s, struct ieee80211vap *vap)
{
	if (strcasecmp(s, "11b") == 0) {
		vap->iv_2_4ghz_prof.phy_mode = IEEE80211_MODE_11B;
	} else if (strcasecmp(s, "11a") == 0) {
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11A;
	} else if (strcasecmp(s, "11g") == 0) {
		vap->iv_2_4ghz_prof.phy_mode = IEEE80211_MODE_11G;
	} else if (strcasecmp(s, "11ng") == 0) {
		vap->iv_2_4ghz_prof.phy_mode = IEEE80211_MODE_11NG;
	} else if (strcasecmp(s, "11ng40") == 0) {
		vap->iv_2_4ghz_prof.phy_mode = IEEE80211_MODE_11NG_HT40PM;
	} else if (strcasecmp(s, "11na") == 0) {
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11NA;
	} else if (strcasecmp(s, "11na40") == 0) {
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11NA_HT40PM;
	} else if ((strcasecmp(s, "11ac") == 0 ) || (strcasecmp(s, "11acOnly") == 0)) {
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11AC_VHT20PM;
	} else if ((strcasecmp(s, "11ac40") == 0) || (strcasecmp(s, "11acOnly40") == 0)) {
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11AC_VHT40PM;
	} else if ((strcasecmp(s, "11ac80") == 0) || (strcasecmp(s, "11acOnly80") == 0 )) {
		vap->iv_5ghz_prof.phy_mode = IEEE80211_MODE_11AC_VHT80PM;
	} else {
		printk(KERN_INFO "In DBS mode - %s is not correct\n", s);
	}
}

static int
ieee80211_get_bw_from_phymode(int mode)
{
	int bw;

	if (mode == IEEE80211_MODE_11AC_VHT160PM)
		bw = BW_HT160;
	else if (mode == IEEE80211_MODE_11AC_VHT80PM)
		bw = BW_HT80;
	else if ((mode == IEEE80211_MODE_11AC_VHT40PM) ||
			(mode == IEEE80211_MODE_11NG_HT40PM) ||
			(mode == IEEE80211_MODE_11NA_HT40PM))
		bw = BW_HT40;
	else
		bw = BW_HT20;

	return bw;
}

static void
ieee80211_update_bw_from_phymode(struct ieee80211vap *vap, int mode)
{
	struct ieee80211com *ic = vap->iv_ic;
	int bw = ieee80211_get_bw_from_phymode(mode);

	ic->ic_max_system_bw = bw;
	ieee80211_update_bw_capa(vap, bw);
	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_SET_MUC_BW, bw, NULL, 0);
}


static int
ieee80211_ioctl_setmode(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *wri, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ifreq ifr;
	char s[12];		/* big enough for "11ac80Edge+ */
	int retv;
	int mode;
	int ifr_mode;
	int aggr = 1;

	if (ic->ic_media.ifm_cur == NULL)
		return -EINVAL;
	if (wri->length > sizeof(s))		/* silently truncate */
		wri->length = sizeof(s);
	if (copy_from_user(s, wri->pointer, wri->length))
		return -EINVAL;

	/* ensure null termination */
	s[sizeof(s)-1] = '\0';
	mode = ieee80211_convert_mode(s);
	if (mode < 0)
		return -EINVAL;

	/* update station profile */
	if ((ic->ic_rf_chipid == CHIPID_DUAL) && (ic->ic_opmode == IEEE80211_M_STA))
		update_sta_profile(s, vap);

	if (((strcasecmp(s, "11ac") == 0) || (strcasecmp(s, "11acOnly") == 0) ||
		(strcasecmp(s, "11acOnly40") == 0) ||(strcasecmp(s, "11acOnly80") == 0))
		&& !ieee80211_swfeat_is_supported(SWFEAT_ID_VHT, 1))
		return -EOPNOTSUPP;

	if ((strcasecmp(s, "11acOnly") == 0) || (strcasecmp(s, "11acOnly40") == 0) ||
		(strcasecmp(s, "11acOnly80") == 0)) {
		vap->iv_11ac_and_11n_flag = IEEE80211_11AC_ONLY;
	} else if ((strcasecmp(s, "11nOnly") == 0) || (strcasecmp(s, "11nOnly40") == 0)) {
		vap->iv_11ac_and_11n_flag = IEEE80211_11N_ONLY;
	} else {
		vap->iv_11ac_and_11n_flag = 0;
	}

	/* In AP mode, redefining AUTO mode */
	if (mode == IEEE80211_MODE_AUTO && ic->ic_opmode == IEEE80211_M_HOSTAP) {
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			mode = IEEE80211_MODE_11NG;
		else
			mode = IEEE80211_MODE_11AC_VHT80PM;
	}

	retv = ieee80211_check_mode_consistency(ic, mode, ic->ic_curchan);
	if (retv == -1) {
		return -EINVAL;
	} else if (retv == 1) {
		/*
		* Reset current channel with default channel when phy mode
		* is not consistent with current channel for dual bands chip
		*/
		if (ic->ic_rf_chipid != CHIPID_DUAL) {
			printk(KERN_INFO "mode - %s is not consistent with channel %d\n",
						s, ic->ic_curchan->ic_ieee);
			return -EOPNOTSUPP;
		}

		/* Send deauth frame before switching channel */
		ieee80211_wireless_reassoc(vap, 0, 1);

		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
				ic->ic_des_chan = findchannel_any(ic, IEEE80211_DEFAULT_2_4_GHZ_CHANNEL, mode);
			else
				ic->ic_des_chan = findchannel_any(ic, IEEE80211_DEFAULT_5_GHZ_CHANNEL, mode);


			ic->ic_curchan = ic->ic_des_chan;
			ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
			ic->ic_set_channel(ic);

			if (IS_UP_AUTO(vap))
				ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
		} else {
			ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		}
	}

	ifr_mode = mode;
	memset(&ifr, 0, sizeof(ifr));

	if(vap->iv_media.ifm_cur == NULL)
		return -EINVAL;

	ifr.ifr_media = vap->iv_media.ifm_cur->ifm_media &~ IFM_MMASK;
	if (mode == IEEE80211_MODE_TURBO_STATIC_A)
		ifr_mode = IEEE80211_MODE_11A;
	ifr.ifr_media |= IFM_MAKEMODE(ifr_mode);
	/* We cannot call with the parent device, needs to be the VAP device */
	retv = ifmedia_ioctl(vap->iv_dev, &ifr, &vap->iv_media, SIOCSIFMEDIA);

	if (retv == -ENETRESET) {
		/* Updating bandwidth base on mode */
		ieee80211_update_bw_from_phymode(vap, mode);

		/* Updating all mode related flags */
		ic->ic_des_mode = ic->ic_phymode = mode;
		ieee80211_setmode(ic, ic->ic_des_mode);
		if (ic->ic_curmode < IEEE80211_MODE_11NA)
			aggr = 0;

		/* Switch channel according to Edge+/- Cntr +/- */
		if (ic->ic_phymode == IEEE80211_MODE_11AC_VHT80PM) {
			ic->ic_des_chan = find_alt_primary_chan_11ac80(ic, s);
			if (ic->ic_des_chan == NULL) {
				ic->ic_des_chan = ic->ic_curchan;
				return -EINVAL;
			}

			ic->ic_curchan = ic->ic_des_chan;
			ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
			ic->ic_set_channel(ic);
		}

		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_PHY_MODE, ic->ic_phymode, NULL, 0);
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_TX_AMSDU, aggr, NULL, 0);
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_AGGREGATION, aggr, NULL, 0);

		ieee80211_start_obss_scan_timer(vap);

		ieee80211_wireless_reassoc(vap, 0, 1);
		retv = 0;
	}
	return -retv;
}

void ieee80211_param_to_qdrv(struct ieee80211vap *vap,
	int param, int value, unsigned char *data, int len)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;

	if (vap->iv_opmode == IEEE80211_M_WDS) {
		ni = ieee80211_find_wds_node(&ic->ic_sta, vap->wds_mac);
		if (ni == NULL)
			return;
	}

	if (ni == NULL) {
		if (ic && TAILQ_FIRST(&ic->ic_vaps))
			ni = TAILQ_FIRST(&ic->ic_vaps)->iv_bss;

		if (ni == NULL)
			return;
	}

	KASSERT(ni, ("no bss node"));
#if 1
	if (vap->iv_opmode != IEEE80211_M_WDS)
		ieee80211_ref_node(ni);
#else
	ieee80211_ref_node(ni);
#endif
	if (ic->ic_setparam != NULL) {
		(*ic->ic_setparam)(ni, param, value, data, len);
	}

	ieee80211_free_node(ni);

	return;
}
EXPORT_SYMBOL(ieee80211_param_to_qdrv);

static void ieee80211_param_from_qdrv(struct ieee80211vap *vap,
	int param, int *value, unsigned char *data, int *len)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;

	if (ni == NULL) {
		ni = TAILQ_FIRST(&ic->ic_vaps)->iv_bss;
	}

	KASSERT(ni, ("no bss node"));
	ieee80211_ref_node(ni);

	if (ic->ic_getparam != NULL) {
		(*ic->ic_getparam)(ni, param, value, data, len);
	}

	ieee80211_free_node(ni);

	return;
}

/* Function attached to the iwpriv wifi0 forcesmps call. */
static int
ieee80211_forcesmps(struct ieee80211vap *vap, int value)
{
	/* Ensure we only apply valid values to the local vap state */
	short smps_mode = (short)value;
	if (value == -1)
	{
		struct ieee80211_node *ni;
		ni = ieee80211_find_node(&vap->iv_ic->ic_sta, vap->iv_myaddr);
		if (ni == NULL) {
			return 0;
		}
		printk("Clearing force SMPS mode in driver\n");
		smps_mode = ni->ni_htcap.pwrsave;
		ieee80211_free_node(ni);
	}
	if (smps_mode != IEEE80211_HTCAP_C_MIMOPWRSAVE_NA)
	{
		/* If we're a STA, send out an ACTION frame to change our SMPS mode */
		if (vap->iv_opmode == IEEE80211_M_STA)
		{
			struct ieee80211_action_data act;
			int action_byte = -1;
			memset(&act, 0, sizeof(act));
			act.cat = IEEE80211_ACTION_CAT_HT;
			act.action = IEEE80211_ACTION_HT_MIMOPWRSAVE;
			switch (smps_mode)
			{
				/* See 802.11n d11.0 section 7.3.1.22 for the formatting of the HT ACTION SMPS byte. */
				case IEEE80211_HTCAP_C_MIMOPWRSAVE_STATIC:
					action_byte = 0x1;
					break;
				case IEEE80211_HTCAP_C_MIMOPWRSAVE_DYNAMIC:
					action_byte = 0x3;
					break;
				case IEEE80211_HTCAP_C_MIMOPWRSAVE_NONE:
					action_byte = 0x0;
					break;
				default:
					printf("Not sending ACTION frame\n");
					break;
			}
			if (action_byte >= 0)
			{
				act.params = (int *)action_byte; //Contain the single byte for the SMPS action frame in the param field
				printk("STA Sending HT Action frame to change PS mode (%02X->%02X)\n", vap->iv_smps_force & 0xFF, smps_mode);
				if (value == -1)
				{
					vap->iv_smps_force &= ~0x8000;
				}
				else
				{
					vap->iv_smps_force = 0x8000 | smps_mode;
				}
				IEEE80211_SEND_MGMT(vap->iv_bss, IEEE80211_FC0_SUBTYPE_ACTION, (int)&act);
			}
		}
	}
	else
	{
		printk("Ignoring invalid SMPS mode (%04X) at WLAN driver\n", smps_mode);
	}
	return 1;
}

/*
 * Check if a node is blacklisted.
 * Returns 1 if true, else 0.
 */
int
ieee80211_blacklist_check(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (ni->ni_blacklist_timeout == 0) {
		return 0;
	}

	if (time_after(jiffies, ni->ni_blacklist_timeout)) {
		ni->ni_blacklist_timeout = 0;
		/* Remove blacklist entry from node table */
		ieee80211_node_leave(ni);
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
			"[%s] removed from blacklist\n",
			ether_sprintf(ni->ni_macaddr));
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL(ieee80211_blacklist_check);

/* Routine to clear existing BA agreements if necessary - used when the global BA control
 * changes
 */
static void
ieee80211_wireless_ba_change(struct ieee80211vap *vap)
{
	ieee80211_wireless_reassoc(vap, 0, 1);
	/* FIXME: re-enable this code once DELBA is working properly. */
#if 0
	int i;
	for (i = 0; i < 8; i++)
	{
		if ((vap->iv_ba_old_control & (1 << i)) && (!(vap->iv_ba_control & (1 << i))))
		{
			printk("Deleting block acks on TID %d\n", i);
			switch (vap->iv_opmode)
			{
				case IEEE80211_M_HOSTAP:
					struct ieee80211com *ic = vap->iv_ic;
					struct net_device *dev = vap->iv_dev;
					int tid_del = i;
					/* Iterate through all STAs - if BA is established, delete it. */
					ieee80211_iterate_dev_nodes(dev, &ic->ic_sta,
						ieee80211_wireless_ba_del, &tid_del, 1);
					break;

				case IEEE80211_M_STA:
					ieee80211_wireless_ba_del((void *)&i, vap->iv_bss);
					break;

				default:
					break;
			}
		}
	}
#endif
}

void ieee80211_obss_scan_timer(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *)arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;

	if (!IEEE80211_IS_11NG_40(ic) || !ic->ic_obss_scan_enable)
		return;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		if (!ic->ic_obss_scan_count) {
			mod_timer(&ic->ic_obss_timer, jiffies + IEEE80211_OBSS_AP_SCAN_INT * HZ);
			(void) ieee80211_start_scan(vap,
					IEEE80211_SCAN_NOPICK|IEEE80211_SCAN_ONCE|IEEE80211_SCAN_ACTIVE,
					IEEE80211_SCAN_FOREVER, 0, NULL);
		}
	} else if (ni && IEEE80211_AID(ni->ni_associd) &&
			(ni->ni_obss_ie.param_id == IEEE80211_ELEMID_OBSS_SCAN)) {
		mod_timer(&ic->ic_obss_timer, jiffies + ni->ni_obss_ie.obss_trigger_interval * HZ);
		(void) ieee80211_start_scan(vap,
			IEEE80211_SCAN_NOPICK|IEEE80211_SCAN_ACTIVE|IEEE80211_SCAN_ONCE|IEEE80211_SCAN_OBSS,
				IEEE80211_SCAN_FOREVER, 0, NULL);
	}
}

void ieee80211_start_obss_scan_timer(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (!IEEE80211_IS_11NG_40(ic) || !ic->ic_obss_scan_enable)
		return;

	ic->ic_obss_scan_count = 0;
	ic->ic_obss_timer.function = ieee80211_obss_scan_timer;
	ic->ic_obss_timer.data = (unsigned long)vap;
	mod_timer(&ic->ic_obss_timer, jiffies + IEEE80211_OBSS_AP_SCAN_INT * HZ);
}

void ieee80211_finish_csa(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;
	struct ieee80211vap *vap;

	/* clear DFS CAC state on previous channel */
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		ic->ic_bsschan->ic_freq != ic->ic_csa_chan->ic_freq &&
		IEEE80211_IS_CHAN_CACDONE(ic->ic_bsschan)) {
		/*
		 * IEEE80211_CHAN_DFS_CAC_DONE indicates whether or not to do CAC afresh.
		 * US   : IEEE80211_CHAN_DFS_CAC_DONE shall be cleared whenver we move to
		 *        a different channel
		 * ETSI : IEEE80211_CHAN_DFS_CAC_DONE shall be retained; Only event which
		 *        would mark the channel as unusable is the radar indication
		 */
		if ((ic->ic_dfs_is_eu_region() == false) &&
		   (ic->ic_chan_compare_equality(ic, ic->ic_bsschan, ic->ic_csa_chan) == false)) {
			ic->ic_bsschan->ic_flags &= ~IEEE80211_CHAN_DFS_CAC_DONE;
			if (ic->ic_mark_channel_dfs_cac_status) {
				ic->ic_mark_channel_dfs_cac_status(ic, ic->ic_bsschan, IEEE80211_CHAN_DFS_CAC_DONE, false);
				ic->ic_mark_channel_dfs_cac_status(ic, ic->ic_bsschan, IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, false);
			}
			/* Mark the channel as not_available and ready for cac */
			if (ic->ic_mark_channel_availability_status) {
				ic->ic_mark_channel_availability_status(ic, ic->ic_bsschan,
						IEEE80211_CHANNEL_STATUS_NOT_AVAILABLE_CAC_REQUIRED);
			}
			printk(KERN_DEBUG"ieee80211_finish_csa:"
					"Clearing CAC_DONE Status for chan %d\n",
					ic->ic_bsschan->ic_ieee);
		}
	}

	ic->ic_prevchan = ic->ic_curchan;
	ic->ic_curchan = ic->ic_csa_chan;
	ic->ic_bsschan = ic->ic_csa_chan;
	ic->ic_des_chan = ic->ic_csa_chan;
	ic->ic_csa_count = 0;

	/* Remove the CSA IE from beacons and cause other field in beacon updated */
	ic->ic_flags &= ~IEEE80211_F_CHANSWITCH;
	ic->ic_set_channel(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;

		if ((vap->iv_state != IEEE80211_S_RUN) && (vap->iv_state != IEEE80211_S_SCAN))
			continue;
		ic->ic_beacon_update(vap);
	}

	/* record channel change event */
	ic->ic_chan_switch_record(ic, ic->ic_csa_chan, ic->ic_csa_reason);
	ic->ic_chan_switch_reason_record(ic, ic->ic_csa_reason);
	return;
}

/*
 * Start a CSA process: CSA beacon/action will be sent to STA to notify the CS.
 * @finish_csa: At the CS time, finish_csa() will be called to do the actual CS. If not provided,
 * ieee80211_finish_csa() will be called as default action.
 * @flag: specify whether to use CSA beacon or CSA action or both.
 */
int ieee80211_enter_csa(struct ieee80211com *ic, struct ieee80211_channel *chan,
		void (*finish_csa)(unsigned long arg), uint32_t reason,
		uint8_t csa_count, uint8_t csa_mode, uint32_t flag)
{
	struct ieee80211vap *vap;
	uint32_t csa_flag;

	if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
		IEEE80211_DPRINTF(TAILQ_FIRST(&ic->ic_vaps), IEEE80211_MSG_DOTH,
				"%s: CSA already in progress, owner=%d, pre-"
				"owner=%d\n", __func__, reason, ic->ic_csa_reason);
		return -1;
	}

	ic->ic_csa_chan = chan;
	csa_flag = ic->ic_csa_flag ? ic->ic_csa_flag : flag;

	/* now flag the beacon update to include the channel switch IE */
	ic->ic_flags |= IEEE80211_F_CHANSWITCH;
	ic->ic_csa_count = csa_count;
	ic->ic_csa_mode = csa_mode;
	ic->ic_csa_reason = reason;
	ic->ic_csw_reason = reason;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;

		if (vap->iv_state != IEEE80211_S_RUN)
			continue;

		/* send broadcast csa action */
		if (csa_flag & IEEE80211_CSA_F_ACTION)
			ic->ic_send_csa_frame(vap, ic->ic_csa_mode,
				ic->ic_csa_chan->ic_ieee, ic->ic_csa_count, 0);
		/* Update beacon to include CSA IE */
		if (csa_flag & IEEE80211_CSA_F_BEACON)
			ic->ic_beacon_update(vap);
	}

	ic->ic_timer_csa.function = finish_csa ? finish_csa : ieee80211_finish_csa;
	mod_timer(&ic->ic_timer_csa, jiffies +
			IEEE80211_TU_TO_JIFFIES(ic->ic_csa_count * ic->ic_lintval + 10));

	/*
	 * Store original attenuation to handle the following case:
	 * We switched to low power channel when attenuation is small. But then in low
	 * power channel attenuation increases but interference is low. We need to detect
	 * such rate ratio drop to trigger channel ranking.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
			ic->ic_curchan != IEEE80211_CHAN_ANYC &&
			ic->ic_scan &&
			ic->ic_scan->ss_scs_priv) {
		struct ap_state *as = ic->ic_scan->ss_scs_priv;

		if (chan->ic_maxpower < (ic->ic_curchan->ic_maxpower - SCS_CHAN_POWER_DIFF_SAFE)) {
			as->as_sta_atten_expect = as->as_sta_atten_max;
		} else if (chan->ic_maxpower > (ic->ic_curchan->ic_maxpower + SCS_CHAN_POWER_DIFF_SAFE)) {
			as->as_sta_atten_expect = SCS_ATTEN_UNINITED;
		}
		SCSDBG(SCSLOG_NOTICE, "atten expect set to %d\n", as->as_sta_atten_expect);
	}

	/* for dfs reentry demon */
	vap = TAILQ_FIRST(&ic->ic_vaps);
	if (ieee80211_is_repeater(ic) && !ieee80211_is_repeater_associated(ic))
		vap = TAILQ_NEXT(vap, iv_next);

	ic->ic_dfs_chan_switch_notify(vap->iv_dev, ic->ic_csa_chan);

	return 0;
}
EXPORT_SYMBOL(ieee80211_enter_csa);

int ieee80211_get_cap_bw(struct ieee80211com *ic)
{
	int bw = BW_INVALID;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	ieee80211_param_from_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, &bw, NULL, 0);

	return bw;
}
EXPORT_SYMBOL(ieee80211_get_cap_bw);

int ieee80211_get_bw(struct ieee80211com *ic)
{
        int bw = ieee80211_get_cap_bw(ic);

        if ((ic->ic_opmode == IEEE80211_M_STA) && (bw != BW_INVALID) && ic->ic_bss_bw) {
                bw = MIN(bw, ic->ic_bss_bw);
        }
        return bw;
}
EXPORT_SYMBOL(ieee80211_get_bw);

void ieee80211_update_bw_capa(struct ieee80211vap *vap, int bw)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *bss = vap->iv_bss;

	if (bw == BW_HT20) {
		ic->ic_htcap.cap &= ~(IEEE80211_HTCAP_C_CHWIDTH40 |
					IEEE80211_HTCAP_C_SHORTGI40);
		if (vap->iv_ht_flags & IEEE80211_HTF_SHORTGI_ENABLED)
		      ic->ic_htcap.cap |= IEEE80211_HTCAP_C_SHORTGI20;
		ic->ic_htinfo.byte1 &= ~IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40;
		ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
	} else if (bw == BW_HT40) {
		ic->ic_htcap.cap |= IEEE80211_HTCAP_C_CHWIDTH40;
		if (vap->iv_ht_flags & IEEE80211_HTF_SHORTGI_ENABLED)
			ic->ic_htcap.cap |= (IEEE80211_HTCAP_C_SHORTGI40 |
						IEEE80211_HTCAP_C_SHORTGI20);
		ic->ic_htinfo.byte1 |= IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40;
		ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ;
	} else if (bw == BW_HT80) {
		ic->ic_htcap.cap |= IEEE80211_HTCAP_C_CHWIDTH40;
		if (vap->iv_ht_flags & IEEE80211_HTF_SHORTGI_ENABLED)
			ic->ic_htcap.cap |= (IEEE80211_HTCAP_C_SHORTGI40 |
						IEEE80211_HTCAP_C_SHORTGI20);
		ic->ic_htinfo.byte1 |= IEEE80211_HTINFO_B1_REC_TXCHWIDTH_40;
		if (vap->iv_vht_flags & IEEE80211_VHTCAP_C_SHORT_GI_80)
			ic->ic_vhtcap.cap_flags |= IEEE80211_VHTCAP_C_SHORT_GI_80;
		ic->ic_vhtop.chanwidth = IEEE80211_VHTOP_CHAN_WIDTH_80MHZ;
	} else {
		printk(KERN_INFO "%s: error - invalid bw %u\n", __func__, bw);
		return;
	}

	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && bss) {
		memcpy(&bss->ni_htcap, &ic->ic_htcap, sizeof(bss->ni_htcap));
		memcpy(&bss->ni_htinfo, &ic->ic_htinfo, sizeof(bss->ni_htinfo));
		memcpy(&bss->ni_vhtcap, &ic->ic_vhtcap, sizeof(bss->ni_vhtcap));
		memcpy(&bss->ni_vhtop, &ic->ic_vhtop, sizeof(bss->ni_vhtop));
	}
}
EXPORT_SYMBOL(ieee80211_update_bw_capa);

int ieee80211_get_mu_grp(struct ieee80211com *ic,
	struct qtn_mu_grp_args *mu_grp_tbl)
{
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int len = sizeof(*mu_grp_tbl)*IEEE80211_MU_GRP_NUM_MAX;

	ieee80211_param_from_qdrv(vap, IEEE80211_PARAM_GET_MU_GRP, NULL, (void*)mu_grp_tbl, &len);

	return len;
}
EXPORT_SYMBOL(ieee80211_get_mu_grp);

int ieee80211_find_sec_chan(struct ieee80211_channel *chan)
{
	int chan_sec = 0;

	if (chan->ic_flags & IEEE80211_CHAN_HT40D) {
		chan_sec = chan->ic_ieee - IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_flags & IEEE80211_CHAN_HT40U) {
		chan_sec = chan->ic_ieee + IEEE80211_CHAN_SEC_SHIFT;
	}

	return chan_sec;
}
EXPORT_SYMBOL(ieee80211_find_sec_chan);

int ieee80211_find_sec40u_chan(struct ieee80211_channel *chan)
{
	int chan_sec40u = 0;

	if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_LL) {
		chan_sec40u = chan->ic_ieee + 3 * IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_LU) {
		chan_sec40u = chan->ic_ieee + 2 * IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_UL) {
		chan_sec40u = chan->ic_ieee - IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU) {
		chan_sec40u = chan->ic_ieee - 2 * IEEE80211_CHAN_SEC_SHIFT;
	}

	return chan_sec40u;
}
EXPORT_SYMBOL(ieee80211_find_sec40u_chan);

int ieee80211_find_sec40l_chan(struct ieee80211_channel *chan)
{
	int chan_sec40l = 0;

	if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_LL) {
		chan_sec40l = chan->ic_ieee + 2 * IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_LU) {
		chan_sec40l = chan->ic_ieee + IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_UL) {
		chan_sec40l = chan->ic_ieee - 2 * IEEE80211_CHAN_SEC_SHIFT;
	} else if (chan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU) {
		chan_sec40l = chan->ic_ieee - 3 * IEEE80211_CHAN_SEC_SHIFT;
	}

	return chan_sec40l;
}
EXPORT_SYMBOL(ieee80211_find_sec40l_chan);

int ieee80211_find_sec_chan_by_operating_class(struct ieee80211com *ic, int chan, uint32_t preference)
{
	uint8_t *chan_list;
	int chan_sec = 0;

	chan_list = kzalloc(howmany(IEEE80211_CHAN_MAX, NBBY), GFP_ATOMIC);
	if (chan_list == NULL) {
		printk(KERN_ERR "%s: buffer alloc failed\n", __FUNCTION__);
		return 0;
	}

	ieee80211_get_prichan_list_by_operating_class(ic,
				BW_HT40,
				(uint8_t *)chan_list,
				preference);
	if (isset(chan_list, chan)) {
		if (IEEE80211_OC_BEHAV_CHAN_UPPER == preference)
			chan_sec = chan - IEEE80211_CHAN_SEC_SHIFT;
		else if (IEEE80211_OC_BEHAV_CHAN_LOWWER == preference)
			chan_sec = chan + IEEE80211_CHAN_SEC_SHIFT;
	}

	if (chan_list)
		kfree(chan_list);

	return chan_sec;
}
EXPORT_SYMBOL(ieee80211_find_sec_chan_by_operating_class);

/*
 * Generate an interference mitigation event
 */
#ifdef QSCS_ENABLED
struct brcm_rxglitch_thrshld_pair brcm_rxglitch_thrshlds[BRCM_RXGLITH_THRSHLD_PWR_NUM][BRCM_RXGLITH_THRSHLD_STEP] = {
	{
		{-49, BRCM_RXGLITCH_TOP},
		{-58, 32000},
		{-65, 20000},
		{-73, 12000},
		{BRCM_RSSI_MIN, 10000},
	},
	{
		{-59, BRCM_RXGLITCH_TOP},
		{-68, 40000},
		{-74, 20000},
		{BRCM_RSSI_MIN, 10000},
		{0, 0},
	},
};

static struct qtn_scs_vsp_node_stats *ieee80211_scs_find_node_stats(struct ieee80211com *ic, struct qtn_scs_info *scs_info_read, uint16_t aid);
int ieee80211_scs_clean_stats(struct ieee80211com *ic, uint32_t level, int clear_dfs_reentry);
static uint32_t ieee80211_scs_fix_cca_intf(struct ieee80211com *ic, struct ieee80211_node *ni, uint32_t cca_intf, uint32_t sp_fail, uint32_t lp_fail);

static int ieee80211_prichan_is_newchan_better(struct ieee80211com *ic,
		int newchan_ieee, int oldchan_ieee, int random_select)
{
	int cur_bw;
	struct ieee80211_channel *newchan;
	struct ieee80211_channel *oldchan;

	if (!newchan_ieee || isclr(ic->ic_chan_active, newchan_ieee) ||
			isset(ic->ic_chan_pri_inactive, newchan_ieee)) {
		return 0;
	}
	newchan = findchannel_any(ic, newchan_ieee, ic->ic_des_mode);
	if (newchan == NULL) {
		return 0;
	}

	if (!oldchan_ieee || isclr(ic->ic_chan_active, oldchan_ieee) ||
			isset(ic->ic_chan_pri_inactive, oldchan_ieee)) {
		return 1;
	}
	oldchan = findchannel_any(ic, oldchan_ieee, ic->ic_des_mode);
	if (oldchan == NULL) {
		return 1;
	}

	/* Choose the channel with maximal power setting */
	cur_bw = ieee80211_get_bw(ic);
	if (cur_bw >= BW_HT80) {
		if (newchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_80M] >
				oldchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_80M]) {
			return 1;
		} else if (newchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_80M] <
				oldchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_80M]) {
			return 0;
		}
	}

	if (cur_bw >= BW_HT40) {
		if (newchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_40M] >
				oldchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_40M]) {
			return 1;
		} else if (newchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_40M] <
				oldchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_40M]) {
			return 0;
		}
	}

	if (newchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_20M] >
			oldchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_20M]) {
		return 1;
	} else if (newchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_20M] <
			oldchan->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][PWR_IDX_20M]) {
		return 0;
	}

	/* All powers are same, run random selection per request */
	if (random_select) {
		uint8_t rndbuf;
		get_random_bytes(&rndbuf, 1);
		return (rndbuf > 127);
	}

	return 0;
}

struct ieee80211_channel* ieee80211_chk_update_pri_chan(struct ieee80211com *ic,
		struct ieee80211_channel *chan, uint32_t rank_by_pwr, const char* caller, int print_warning)
{
	struct ieee80211_channel *prichan;
	int newchan_ieee;
	int prichan_ieee = 0;
	int cur_bw = ieee80211_get_bw(ic);
	int is_manual_cfg;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		return chan;
	}

	is_manual_cfg = !strcmp(caller, "iwconfig");

	if (cur_bw <= BW_HT20) {
		goto done;
	}

	if (isclr(ic->ic_chan_pri_inactive, chan->ic_ieee) ||
			(is_manual_cfg && isset(ic->ic_is_inactive_autochan_only, chan->ic_ieee))) {
		if (!rank_by_pwr) {
			return chan;
		} else {
			prichan_ieee = chan->ic_ieee;
		}
	}

	newchan_ieee = ieee80211_find_sec_chan(chan);
	if (ieee80211_prichan_is_newchan_better(ic, newchan_ieee, prichan_ieee, 0)) {
		prichan_ieee = newchan_ieee;
	}

	if (cur_bw > BW_HT40) {
		newchan_ieee = ieee80211_find_sec40u_chan(chan);
		if (ieee80211_prichan_is_newchan_better(ic, newchan_ieee, prichan_ieee,
				prichan_ieee && (prichan_ieee != chan->ic_ieee))) {
			prichan_ieee = newchan_ieee;
		}

		newchan_ieee = ieee80211_find_sec40l_chan(chan);
		if (ieee80211_prichan_is_newchan_better(ic, newchan_ieee, prichan_ieee,
				prichan_ieee && (prichan_ieee != chan->ic_ieee))) {
			prichan_ieee = newchan_ieee;
		}
	}

	if (prichan_ieee && prichan_ieee != chan->ic_ieee) {
		prichan = findchannel_any(ic, prichan_ieee, ic->ic_des_mode);
		if (prichan) {
			if (isset(ic->ic_chan_pri_inactive, chan->ic_ieee)) {
				if (print_warning) {
					printk("%s: channel %d can't be used as primary channel,"
							" use %d instead within current bandwidth\n",
							caller, chan->ic_ieee, prichan_ieee);
				}
			}
			return prichan;
		}
	}

done:
	if (isset(ic->ic_chan_pri_inactive, chan->ic_ieee) &&
			(!is_manual_cfg || isclr(ic->ic_is_inactive_autochan_only, chan->ic_ieee))) {
		if (print_warning) {
			printk("%s: channel %d can't be used as primary channel,"
					" and no alternative channel within current bandwidth\n",
					caller, chan->ic_ieee);
		}
	}

	return chan;
}
EXPORT_SYMBOL(ieee80211_chk_update_pri_chan);

static void
ieee80211_wireless_scs_msg_send(struct ieee80211vap *vap, char *msg_buf)
{
	ieee80211_eventf(vap->iv_dev, "%s",  msg_buf);
};

static __inline int
ieee80211_is_cac_in_progress(struct ieee80211com *ic)
{
	return ic->ic_curchan->ic_flags & IEEE80211_CHAN_DFS_CAC_IN_PROGRESS;
}

void ieee80211_off_channel_timeout(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *)arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct offchan_protect *offchan_prt = &ic->ic_offchan_protect;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "%s: suspending counter %u\n",
				__func__, offchan_prt->offchan_suspend_cnt);

	offchan_prt->offchan_suspend_cnt = 0;
	offchan_prt->offchan_timeout = 0;
	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_OFF_CHAN_SUSPEND, 0, NULL, 0);
}

void ieee80211_off_channel_suspend(struct ieee80211vap *vap, uint32_t timeout)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct offchan_protect *offchan_prt = &ic->ic_offchan_protect;

	if (offchan_prt->offchan_timeout == 0) {
		offchan_prt->offchan_stop_expire.data = (unsigned long)vap;
		offchan_prt->offchan_stop_expire.expires = jiffies + IEEE80211_OFFCHAN_TIMEOUT_DEFAULT * HZ;
		offchan_prt->offchan_timeout = jiffies + IEEE80211_OFFCHAN_TIMEOUT_DEFAULT * HZ;
		add_timer(&offchan_prt->offchan_stop_expire);
	}

	if (time_after(jiffies + timeout * HZ, offchan_prt->offchan_timeout)) {
		offchan_prt->offchan_stop_expire.data = (unsigned long)vap;
		offchan_prt->offchan_timeout = jiffies + timeout * HZ;
		mod_timer(&offchan_prt->offchan_stop_expire, offchan_prt->offchan_timeout);
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "%s: suspending counter %u, "
				"timeout %lus later\n",
				__func__, offchan_prt->offchan_suspend_cnt,
				(offchan_prt->offchan_timeout - jiffies) / HZ);

	offchan_prt->offchan_suspend_cnt++;
	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_OFF_CHAN_SUSPEND, 1, NULL, 0);
}

void ieee80211_off_channel_resume(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct offchan_protect *offchan_prt = &ic->ic_offchan_protect;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "%s: suspending counter %u, "
				"timeout %lus later\n",
				__func__, offchan_prt->offchan_suspend_cnt,
				offchan_prt->offchan_timeout ?
					(offchan_prt->offchan_timeout - jiffies) / HZ :
					0);

	/* There is a potential race condition here, but the timer will kick in and recover it.
	 * Currently we don't plan to protect against it */
	if (offchan_prt->offchan_suspend_cnt)
		offchan_prt->offchan_suspend_cnt--;
	else
		return;

	if (offchan_prt->offchan_suspend_cnt == 0) {
		offchan_prt->offchan_timeout = 0;
		del_timer(&offchan_prt->offchan_stop_expire);
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_OFF_CHAN_SUSPEND, 0, NULL, 0);
	}
}

/*
 * Interference mitigation sampling task
 * Periodically go off-channel to sample the quality of another channel.
 */
static void
ieee80211_wireless_scs_sampling_task(struct work_struct *work)
{
	struct delayed_work *dwork = (struct delayed_work *)work;
	struct ieee80211com *ic =
		container_of(dwork, struct ieee80211com, ic_scs_sample_work);
	struct ieee80211vap *vap = NULL;
	struct ieee80211vap *vap_first = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211vap *vap_next;

	if (ieee80211_is_cac_in_progress(ic)) {
		SCSDBG(SCSLOG_NOTICE, "%s: not sampling - CAC in progress\n", __func__);
		goto next_work;
	}

	//FIXME check threshold

	/* Only sample if at least one VAP is in run state and none are scanning */
	vap_next = vap_first;
	while ((vap_next != NULL) &&
	       (vap_next->iv_state != IEEE80211_S_SCAN)) {

		if ((vap == NULL) && (vap_next->iv_opmode == IEEE80211_M_HOSTAP) &&
				(vap_next->iv_state == IEEE80211_S_RUN)) {
			vap = vap_next;
		}
		vap_next = TAILQ_NEXT(vap_next, iv_next);
	}

	if (vap) {
		if (vap_next == NULL) {
			IEEE80211_SCS_CNT_INC(&ic->ic_scs, IEEE80211_SCS_CNT_TRIGGER);
			ieee80211_scan_scs_sample(vap);
		} else {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: not sampling - scan in progress\n", __func__);
		}
	} else {
		SCSDBG(SCSLOG_NOTICE, "%s: not sampling - no VAPs in RUN state\n", __func__);
	}

next_work:
	schedule_delayed_work(&ic->ic_scs_sample_work, ic->ic_scs.scs_sample_intv * HZ);
}

static void ieee80211_scs_trigger_channel_switch(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;

	ieee80211_finish_csa((unsigned long)ic);

	ieee80211_scs_clean_stats(ic, IEEE80211_SCS_STATE_CHANNEL_SWITCHING, 0);

	return;
}

static __inline int
ieee80211_scs_get_cca_intf_thrshld(struct ieee80211com *ic, uint8_t is_high)
{
	uint32_t thrshld = is_high ? ic->ic_scs.scs_cca_intf_hi_thrshld :
			ic->ic_scs.scs_cca_intf_lo_thrshld;

	if (ic->ic_curchan && (ic->ic_curchan->ic_flags & IEEE80211_CHAN_DFS)) {
		thrshld += ic->ic_scs.scs_cca_intf_dfs_margin;
	}

	thrshld = MIN(thrshld, 100);
	thrshld = thrshld * IEEE80211_SCS_CCA_INTF_SCALE / 100;

	return thrshld;
}

static __inline int
ieee80211_scs_get_cca_idle_thrshld(struct ieee80211com *ic)
{
	int thrshld = ic->ic_scs.scs_cca_idle_thrshld * IEEE80211_SCS_CCA_INTF_SCALE / 100;

	return thrshld;
}

static int
ieee80211_scs_is_interference_over_thresholds(struct ieee80211com *ic,
		uint32_t cca_intf, uint32_t cca_idle, uint32_t pmbl_err)
{
	uint32_t cca_intf_high_thrshld = ieee80211_scs_get_cca_intf_thrshld(ic, 1);
	uint32_t cca_intf_low_thrshld = ieee80211_scs_get_cca_intf_thrshld(ic, 0);
	uint32_t cca_idle_thrshld = ieee80211_scs_get_cca_idle_thrshld(ic);
	uint32_t pmbl_err_thrshld = ic->ic_scs.scs_pmbl_err_thrshld;

	/* Currently we don't apply the thresholds of FAT and preamble error to QHop case */
	if (ieee80211_wds_vap_exists(ic)) {
		if (cca_intf >= cca_intf_low_thrshld) {
			SCSDBG(SCSLOG_VERBOSE, "%s: [QHOP case:cca_intf > low thrshld] Trigger channel change\n", __func__);
			return 1;
		}
	} else if (cca_idle < cca_idle_thrshld) {
		if ((cca_intf > cca_intf_high_thrshld)
			|| ((cca_intf > cca_intf_low_thrshld)
			&& (pmbl_err > pmbl_err_thrshld))) {
			SCSDBG(SCSLOG_VERBOSE, "%s: [cca_idle < thrshld, %s] - Trigger channel change\n", __func__,
				((cca_intf > cca_intf_high_thrshld) ? "cca_intf > high thrshld" :
				"cca_intf > low thrshld, pmbl_err > thrshld"));
			return 1;
		}
	}
	return 0;
}

static int
ieee80211_is_cc_required(struct ieee80211com *ic, uint32_t compound_cca_intf,
	uint32_t cca_idle_smthed, uint32_t pmbl_err)
{
	int res = 0;
	struct ap_state *as;

	if (ic->ic_sta_cc) {
		SCSDBG(SCSLOG_NOTICE, "STA reported SCS measurements\n");
		res |= IEEE80211_SCS_STA_CCA_REQ_CC;
	}

	if (ic->ic_sta_cc_brcm) {
		SCSDBG(SCSLOG_NOTICE, "brcm STA info need channel change\n");
		res |= IEEE80211_SCS_BRCM_STA_TRIGGER_CC;
	}

	if (ic->ic_opmode == IEEE80211_M_STA) {
		/* For STA mode, always send cca report to AP */
		res |= IEEE80211_SCS_SELF_CCA_CC;
	} else if (ieee80211_scs_is_interference_over_thresholds(ic,
			compound_cca_intf, cca_idle_smthed, pmbl_err)) {
		SCSDBG(SCSLOG_NOTICE, "Self CCA requested channel change,"
				" compound_cca_intf=%u, cca_idle_smth=%u, pmbl_err=%u\n",
				compound_cca_intf, cca_idle_smthed, pmbl_err);
		res |= IEEE80211_SCS_SELF_CCA_CC;
	}

	if (ic->ic_opmode == IEEE80211_M_HOSTAP && ic->ic_scs.scs_atten_sw_enable) {
		as = ic->ic_scan->ss_scs_priv;
		if (SCS_ATTEN_VALID(as->as_sta_atten_expect) &&
			SCS_ATTEN_VALID(as->as_sta_atten_max) &&
			(as->as_sta_atten_max >= (as->as_sta_atten_expect + ic->ic_scs.scs_thrshld_atten_inc))) {
			SCSDBG(SCSLOG_NOTICE, "raw attenuation increased, need channel change, curr=%d, expect=%d\n",
					      as->as_sta_atten_max, as->as_sta_atten_expect);
			res |= IEEE80211_SCS_ATTEN_INC_CC;
		}
	}

	if ((res) && (ic->ic_radar_test_mode_enabled != NULL) && ic->ic_radar_test_mode_enabled()) {
		SCSDBG(SCSLOG_NOTICE, "channel change is disabled under radar test mode\n");
		res = 0;
	}

	/* Don't switch channel under basic WDS mode */
	/* But channel switch is now possible on the WDS link if it is an MBS */
	if (res) {
		struct ieee80211vap *vap;
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if (IEEE80211_VAP_WDS_BASIC(vap)) {
				SCSDBG(SCSLOG_NOTICE, "channel change is disabled under basic WDS mode\n");
				res = 0;
				break;
			}
		}
	}

	if (res && (ic->ic_opmode == IEEE80211_M_HOSTAP) &&
			(ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] >= BOARD_PM_LEVEL_DUTY)) {
		SCSDBG(SCSLOG_NOTICE, "channel change is disabled in CoC idle state\n");
		res = 0;
	}

	if (res && !ic->ic_scs.scs_enable) {
		SCSDBG(SCSLOG_NOTICE, "channel change is disabled since SCS is disabled\n");
		res = 0;
	}

	return res;
}

void ieee80211_scs_show_ranking_stats(struct ieee80211com *ic, int show_input, int show_result)
{
	struct ap_state *as;
	int i;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		printk("SCS ranking state is only available in AP mode\n");
		return;
	}

	as = ic->ic_scan->ss_scs_priv;

	if (show_input) {
		printk("SCS: ranking parameters\n");
		for (i = 0; i < IEEE80211_CHAN_MAX; i++) {
			if (isclr(ic->ic_chan_active, i)) {
				continue;
			}
			if (as->as_cca_intf[i] != SCS_CCA_INTF_INVALID) {
				printk("chan %d: cca_intf=%u, pmbl=%u %u\n",
					i, as->as_cca_intf[i], as->as_pmbl_err_ap[i],
					as->as_pmbl_err_sta[i]);
			}
		}

		printk("SCS: atten info: num=%d, sum=%d, min=%d, max=%d, avg=%d, expect=%d\n",
			as->as_sta_atten_num,
			as->as_sta_atten_sum,
			as->as_sta_atten_min,
			as->as_sta_atten_max,
			as->as_sta_atten_num ? (as->as_sta_atten_sum / as->as_sta_atten_num) : 0,
			as->as_sta_atten_expect);

		printk("SCS: tx_ms=%u, rx_ms=%u\n", as->as_tx_ms, as->as_rx_ms);
	}

	if (show_result) {
		int isdfs;
		int txpower;
		struct ieee80211_channel *chan;

		printk("SCS: ranking table, ranking_cnt=%u\n", as->as_scs_ranking_cnt);
		printk("chan dfs xped txpower cca_intf     metric    pmbl_ap   pmbl_sta\n");
		for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
			chan = ieee80211_find_channel_by_ieee(ic, i);
			if (chan == NULL) {
				continue;
			}

			isdfs = !!(chan->ic_flags & IEEE80211_CHAN_DFS);
			txpower = chan->ic_maxpower;

			printk("%4d %3d %4d %7d %8u %10d %10d %10d\n",
					i,
					isdfs,
					!!isset(as->as_chan_xped, i),
					txpower,
					((as->as_cca_intf[i] == SCS_CCA_INTF_INVALID) ? 0 : as->as_cca_intf[i]),
					as->as_chanmetric[i],
					as->as_pmbl_err_ap[i], as->as_pmbl_err_sta[i]);
		}
	}
}
EXPORT_SYMBOL(ieee80211_scs_show_ranking_stats);

void ieee80211_show_initial_ranking_stats(struct ieee80211com *ic)
{
	struct ap_state *as;
	int i;
	int isdfs;
	int txpower;
	struct ieee80211_channel *chan;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		printk("Initial scan ranking state is only available in AP mode\n");
		return;
	}

	as = ic->ic_scan->ss_priv;
	if (as == NULL) {
		printk("Initial scan ranking state is not available because auto channel is disabled\n");
		return;
	}

	printk("AP: initial ranking table\n");
	printk("chan dfs txpower  numbeacon        cci        aci   cca_intf   pmbl_err     metric\n");
	for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
		chan = ieee80211_find_channel_by_ieee(ic, i);
		if (chan == NULL) {
			continue;
		}

		isdfs = !!(chan->ic_flags & IEEE80211_CHAN_DFS);
		txpower = chan->ic_maxpower;

		printk("%4d %3d %7d %10u %10d %10d %10d %10d %10d\n",
				i,
				isdfs,
				txpower,
				as->as_numbeacons[i],
				as->as_cci[i],
				as->as_aci[i],
				as->as_cca_intf[i],
				as->as_pmbl_err_ap[i],
				as->as_chanmetric[i]);
	}
}
EXPORT_SYMBOL(ieee80211_show_initial_ranking_stats);

static __inline int
ieee80211_scs_node_is_valid(struct ieee80211_node *ni)
{
	return (ni->ni_associd &&
		ieee80211_node_is_authorized(ni) &&
		!ieee80211_blacklist_check(ni));
}

void ieee80211_scs_node_clean_stats(void *s, struct ieee80211_node *ni)
{
	int level = (uint32_t)s;
	struct ieee80211com *ic = ni->ni_ic;

	if (!ieee80211_scs_node_is_valid(ni) && (level != IEEE80211_SCS_STATE_INIT)) {
		return;
	}

	SCSDBG(SCSLOG_VERBOSE, "node 0x%x state clean with level %d\n", ni->ni_associd, level);

	if (level <= IEEE80211_SCS_STATE_PERIOD_CLEAN) {
		ni->ni_recent_cca_intf = SCS_CCA_INTF_INVALID;
		ni->ni_recent_sp_fail = 0;
		ni->ni_recent_lp_fail = 0;
		ni->ni_recent_tdls_tx_time = 0;
		ni->ni_recent_tdls_rx_time = 0;
		ni->ni_recent_others_time = 0;
	}

	if (level <= IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN) {
		/* set to -1 helps to discard first report after assoc or channel switch */
		ni->ni_recent_rxglitch_trig_consecut = -1;
		ni->ni_recent_rxglitch = 0;
		ni->ni_recent_cca_intf_smthed = 0;
		ni->ni_others_rx_time_smthed = 0;
		ni->ni_others_tx_time_smthed = 0;
		ni->ni_recent_others_time_smth = 0;
		ni->ni_tdls_tx_time_smthed = 0;
		ni->ni_tdls_rx_time_smthed = 0;
	}

	if (level <= IEEE80211_SCS_STATE_RESET) {
		ni->ni_atten_smoothed = SCS_ATTEN_UNINITED;
	}
}

void ieee80211_scs_clean_tdls_stats_list(struct ieee80211com *ic)
{
	int i;
	unsigned long flags;
	struct ieee80211_tdls_scs_entry *scs_entry = NULL;

	SCSDBG(SCSLOG_NOTICE, "SCS: clean all of tdls stats\n");
	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH(scs_entry, &ic->ic_scs.scs_tdls_list[i], entry) {
			if (scs_entry != NULL) {
				scs_entry->stats.is_latest = 0;
				scs_entry->stats.tx_time = 0;
			}
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);
}

void
ieee80211_scs_metric_update_timestamps(struct ap_state *as)
{
	int i;
	for (i = 0; i < IEEE80211_CHAN_MAX; ++i) {
		as->as_chanmetric_timestamp[i] = jiffies;
	}
}
EXPORT_SYMBOL(ieee80211_scs_metric_update_timestamps);

/*
 * Clean SCS state with different clean levels.
 * Valid levels are IEEE80211_SCS_STATE_XXXX.
 * @clear_dfs_reentry: only effective at level IEEE80211_SCS_STATE_PERIOD_CLEAN.
 */
int ieee80211_scs_clean_stats(struct ieee80211com *ic, uint32_t level, int clear_dfs_reentry)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	int i;

	SCSDBG(SCSLOG_INFO, "clean stats with level %u\n", level);

	if (level <= IEEE80211_SCS_STATE_PERIOD_CLEAN) {
		ic->ic_sta_cc = 0;
		ic->ic_sta_cc_brcm = 0;
		if (clear_dfs_reentry) {
			as->as_dfs_reentry_cnt = 0;
			as->as_dfs_reentry_level = 0;
			SCSDBG(SCSLOG_INFO, "dfs reentry state cleared\n");
		}
		ieee80211_scs_clean_tdls_stats_list(ic);
	}

	if (level <= IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN) {
		as->as_tx_ms_smth = 0;
		as->as_rx_ms_smth = 0;
		as->as_cca_intf_smth = 0;
	}

	if (level <= IEEE80211_SCS_STATE_CHANNEL_SWITCHING) {
		ic->ic_scs.scs_cca_intf_smthed = 0;
		ic->ic_scs.scs_sp_err_smthed = 0;
		ic->ic_scs.scs_lp_err_smthed = 0;
		ic->ic_scs.scs_cca_idle_smthed = 0;
	}

	if (level <= IEEE80211_SCS_STATE_RESET) {
		SCSDBG(SCSLOG_NOTICE, "reset ranking stats\n");
		ic->ic_scs.scs_brcm_rxglitch_thrshlds = (struct brcm_rxglitch_thrshld_pair*)brcm_rxglitch_thrshlds;
		as->as_scs_ranking_cnt = 0;

		for (i = 0; i < IEEE80211_CHAN_MAX; i++) {
			as->as_cca_intf[i] = SCS_CCA_INTF_INVALID;
			as->as_cca_intf_jiffies[i] = 0;
			as->as_pmbl_err_ap[i] = 0;
			as->as_pmbl_err_sta[i] = 0;
		}

		as->as_sta_atten_num = 0;
		as->as_sta_atten_sum = 0;
		as->as_sta_atten_min = SCS_ATTEN_UNINITED;
		as->as_sta_atten_max = SCS_ATTEN_UNINITED;
		as->as_sta_atten_expect = SCS_ATTEN_UNINITED;

		as->as_dfs_reentry_cnt = 0;
		as->as_dfs_reentry_level = 0;

		as->as_tx_ms = 0;
		as->as_rx_ms = 0;

		memset(as->as_chan_xped, 0, sizeof(as->as_chan_xped));

		memset(as->as_chanmetric, 0, sizeof(as->as_chanmetric));
		ieee80211_scs_metric_update_timestamps(as);
		memset(as->as_chanmetric_pref, 0, sizeof(as->as_chanmetric_pref));
	}

	/*
	 * No need to do clean node in level IEEE80211_SCS_STATE_PERIOD_CLEAN
	 * because it is associated with jiffies. So that we don't need to iterate
	 * all node every scs interval.
	 */
	if ((level <= IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN) &&
		(ic->ic_opmode == IEEE80211_M_HOSTAP)) {
			ic->ic_iterate_nodes(&ic->ic_sta, ieee80211_scs_node_clean_stats,
				(void *)level, 1);
	}

	return 0;
}
EXPORT_SYMBOL(ieee80211_scs_clean_stats);

static void ieee80211_send_usr_l2_pkt(struct ieee80211vap *vap, uint8_t *pkt, uint32_t pkt_len)
{
	struct sk_buff *skb = dev_alloc_skb(qtn_rx_buf_size());

	if (pkt_len > qtn_rx_buf_size())
		pkt_len = qtn_rx_buf_size();

	if (skb) {
		if (copy_from_user(skb->data, pkt, pkt_len)) {
			dev_kfree_skb(skb);
			return;
		}
		skb->len = pkt_len;
		skb->dest_port = 0;
		skb->dev = vap->iv_dev;
		skb->priority = QDRV_SCH_MODULE_ID | QDRV_BAND_AC_BK;
		dev_queue_xmit(skb);
	}
}

void ieee80211_scs_brcm_info_report(struct ieee80211com *ic, struct ieee80211_node *ni, int32_t rssi, uint32_t rxglitch)
{
	int i;
	uint32_t glitch_thrshld = 0;
	int ratio;
	int pwr;
	uint32_t cca_intf;
	uint32_t trig_rxglitch;

	/* Currently we don't support BRCM 11AC STA's CC request*/
	if (IEEE80211_NODE_IS_VHT(ni)) {
		SCSDBG(SCSLOG_NOTICE, "Ignore BRCM 11AC STA's report\n");
		return;
	}

	if (!ic->ic_curchan)
		return;

	if (!ic->ic_scs.scs_stats_on)
		return;

	if (rxglitch >= BRCM_RXGLITCH_MAX_PER_INTVL) {
		SCSDBG(SCSLOG_NOTICE, "brcm node 0x%x "MACSTR" rssi=%d, rxglitch=%u(discard)\n",
			       ni->ni_associd, MAC2STR(ni->ni_macaddr), rssi, rxglitch);
		return;
	} else {
		SCSDBG(SCSLOG_INFO, "brcm node 0x%x "MACSTR" rssi=%d, rxglitch=%u\n",
			       ni->ni_associd, MAC2STR(ni->ni_macaddr), rssi, rxglitch);
	}

	if (ic->ic_curchan->ic_maxpower >= IEEE80211_SCS_CHAN_POWER_CUTPOINT) {
		pwr = BRCM_RXGLITH_THRSHLD_HIPWR;
	} else {
		pwr = BRCM_RXGLITH_THRSHLD_LOWPWR;
	}

	for (i = 0; i < BRCM_RXGLITH_THRSHLD_STEP; i++) {
		if (rssi > brcm_rxglitch_thrshlds[pwr][i].rssi) {
			glitch_thrshld = brcm_rxglitch_thrshlds[pwr][i].rxglitch;
			glitch_thrshld = glitch_thrshld * ic->ic_scs.scs_brcm_rxglitch_thrshlds_scale / 100;
			break;
		}
	}
	if (!glitch_thrshld)
		return;

	trig_rxglitch = 0;
	if (rxglitch >= glitch_thrshld){
		if (ni->ni_recent_rxglitch_trig_consecut > 0) {
			trig_rxglitch = rxglitch;
			SCSDBG(SCSLOG_NOTICE, "brcm node 0x%x is triggered consecutively\n", ni->ni_associd);
		} else {
			SCSDBG(SCSLOG_NOTICE, "brcm node 0x%x is not triggered in last report, wait for next\n", ni->ni_associd);
		}
		ni->ni_recent_rxglitch_trig_consecut++;
	} else if ((ni->ni_recent_rxglitch_trig_consecut > 0) &&
		(rxglitch >= (BRCM_RXGLITCH_NEXT_TRIG_THRSHLD * glitch_thrshld / 100))) {
		SCSDBG(SCSLOG_NOTICE, "brcm node 0x%x is triggered in last report, and validated\n",
				ni->ni_associd);
		trig_rxglitch = ni->ni_recent_rxglitch;
		ni->ni_recent_rxglitch_trig_consecut = 0;
	} else {
		ni->ni_recent_rxglitch_trig_consecut = 0;
	}
	ni->ni_recent_rxglitch = rxglitch;

	if (trig_rxglitch) {
		ratio = trig_rxglitch * 100 / glitch_thrshld;
		cca_intf = ratio * ic->ic_scs.scs_cca_intf_lo_thrshld * IEEE80211_SCS_CCA_INTF_SCALE / 10000;
		cca_intf = MIN(cca_intf, IEEE80211_SCS_CCA_INTF_SCALE);
		SCSDBG(SCSLOG_NOTICE, "brcm node 0x%x report high rxglitch %u > %u, "
					"with consecutive count %u, mapped to cca_intf %u\n",
					ni->ni_associd, trig_rxglitch, glitch_thrshld,
					ni->ni_recent_rxglitch_trig_consecut,
					cca_intf);
		ni->ni_recent_cca_intf = cca_intf;
		ni->ni_recent_cca_intf_jiffies = jiffies;
		ic->ic_sta_cc_brcm = 1;
	}
}

void ieee80211_scs_update_cca_intf(struct ieee80211com *ic, uint32_t chan, uint8_t iscochan,
				uint16_t cca_intf, uint16_t cca_dur, struct ieee80211_node *ni,
				uint32_t pmbl_err)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	uint16_t cca_intf_scaled;
	uint16_t old_cca_intf = 0;
	uint8_t smth_fctr = 0;
	uint32_t old_pmbl_ap = 0;
	uint32_t old_pmbl_sta = 0;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		return;
	}

	if ((chan >= IEEE80211_CHAN_MAX) || (chan == 0)) {
		return;
	}

	/* reset entry */
	if (cca_intf == SCS_CCA_INTF_INVALID) {
		SCSDBG(SCSLOG_INFO, "clean chan %u cca_intf in ranking table\n", chan);
		as->as_cca_intf[chan] = SCS_CCA_INTF_INVALID;
		as->as_cca_intf_jiffies[chan] = 0;
		as->as_pmbl_err_ap[chan] = 0;
		as->as_pmbl_err_sta[chan] = 0;
		return;
	}

	/* always scale cca_intf so that it is adaptive to cca duration change and off-chan sample */
	cca_intf_scaled = cca_intf * IEEE80211_SCS_CCA_INTF_SCALE / cca_dur;
	if (cca_dur != IEEE80211_SCS_CCA_INTF_SCALE)
		SCSDBG(SCSLOG_NOTICE, "scale cca_intf from %u to %u\n", cca_intf, cca_intf_scaled);

	if (ni) {
		SCSDBG(SCSLOG_NOTICE, "%s - sta %x add cca intf %u to chan %u, sta max pmbl=%u\n",
				iscochan ? "cochan" : "offchan",
				ni->ni_associd, cca_intf_scaled, chan, pmbl_err);
	} else {
		SCSDBG(SCSLOG_NOTICE, "%s - self add cca intf %u to chan %u, pmbl=%u\n",
				iscochan ? "cochan" : "offchan",
				cca_intf_scaled, chan, pmbl_err);
	}

	if (iscochan) {
		/* current channel's cca_intf use maximum of AP and STAs */
		if ((cca_intf_scaled > as->as_cca_intf[chan]) ||
				(as->as_cca_intf[chan] == SCS_CCA_INTF_INVALID)) {
			as->as_cca_intf[chan] = cca_intf_scaled;
			as->as_cca_intf_jiffies[chan] = jiffies;
		}
		if (ni != NULL)
			as->as_pmbl_err_sta[chan] = MAX(as->as_pmbl_err_sta[chan], pmbl_err);
		else
			as->as_pmbl_err_ap[chan] = pmbl_err;
	} else {
		/* update off-channel sampling stats with exponential smoothing */
		if (as->as_cca_intf[chan] == SCS_CCA_INTF_INVALID) {
			as->as_cca_intf[chan] = cca_intf_scaled;
			/* only have AP side off-channel sampling now */
			as->as_pmbl_err_ap[chan] = pmbl_err;
		} else {
			smth_fctr = (isclr(as->as_chan_xped, chan)) ?
				ic->ic_scs.scs_cca_intf_smth_fctr[SCS_CCA_INTF_SMTH_FCTR_NOXP] :
				ic->ic_scs.scs_cca_intf_smth_fctr[SCS_CCA_INTF_SMTH_FCTR_XPED];
			old_cca_intf = as->as_cca_intf[chan];
			as->as_cca_intf[chan] =  IEEE80211_SCS_SMOOTH(old_cca_intf, cca_intf_scaled,
					smth_fctr);
			/* only have AP side off-channel sampling now */
			old_pmbl_ap = as->as_pmbl_err_ap[chan];
			as->as_pmbl_err_ap[chan] = IEEE80211_SCS_SMOOTH(old_pmbl_ap, pmbl_err,
					smth_fctr);
			/* let sta side pmbl smoothing out */
			old_pmbl_sta = as->as_pmbl_err_sta[chan];
			as->as_pmbl_err_sta[chan] = IEEE80211_SCS_SMOOTH(old_pmbl_sta, 0,
					smth_fctr);
		}
		/* mark channel entry updated so it won't be aged out */
		as->as_cca_intf_jiffies[chan] = jiffies;
		SCSDBG(SCSLOG_INFO, "OC: chan=%u, smth_fctr=%u, cca_intf: prev=%u, "
				"curr=%u, smthed=%u pmbl: prev=%u,%u, curr=%u, smthed=%u,%u\n",
				chan, smth_fctr,
				old_cca_intf, cca_intf_scaled, as->as_cca_intf[chan],
				old_pmbl_ap, old_pmbl_sta, pmbl_err,
				as->as_pmbl_err_ap[chan], as->as_pmbl_err_sta[chan]);
	}
}

void ieee80211_scs_update_chans_cca_intf(struct ieee80211com *ic, struct ieee80211_channel *chan,
		uint32_t chan_bw, uint32_t update_mode, uint16_t cca_intf, uint16_t cca_dur,
		struct ieee80211_node *ni, uint32_t pmbl_err)
{
	uint32_t chan_pri = chan->ic_ieee;
	uint32_t chan_sec = 0;
	uint32_t chan_sec40u = 0;
	uint32_t chan_sec40l = 0;
	uint8_t isoffchan = (update_mode == IEEE80211_SCS_OFFCHAN);
	uint8_t iscochan = (update_mode == IEEE80211_SCS_COCHAN);

	SCSDBG(SCSLOG_INFO, "Update chans cca intf -- chan:%u bw:%u intf:%u dur:%u pmbl:%u\n",
			chan_pri, chan_bw, cca_intf, cca_dur, pmbl_err);

	if (isoffchan && chan_pri == ic->ic_curchan->ic_ieee) {
		return;
	}
	if (chan_bw >= BW_HT40) {
		chan_sec = ieee80211_find_sec_chan(chan);
		if (isoffchan && chan_sec == ic->ic_curchan->ic_ieee) {
			return;
		}
		if (chan_bw >= BW_HT80) {
			chan_sec40u = ieee80211_find_sec40u_chan(chan);
			chan_sec40l = ieee80211_find_sec40l_chan(chan);
			if (isoffchan && (chan_sec40u == ic->ic_curchan->ic_ieee ||
					chan_sec40l == ic->ic_curchan->ic_ieee)) {
				return;
			}
		}
	}

	ieee80211_scs_update_cca_intf(ic, chan_pri, iscochan, cca_intf, cca_dur, ni, pmbl_err);
	if (chan_bw >= BW_HT40) {
		if (chan_sec) {
			ieee80211_scs_update_cca_intf(ic, chan_sec,
					iscochan, cca_intf, cca_dur, ni, pmbl_err);
		}
		if (chan_bw >= BW_HT80) {
			if (chan_sec40u) {
				ieee80211_scs_update_cca_intf(ic, chan_sec40u,
						iscochan, cca_intf, cca_dur, ni, pmbl_err);
			}
			if (chan_sec40l) {
				ieee80211_scs_update_cca_intf(ic, chan_sec40l,
						iscochan, cca_intf, cca_dur, ni, pmbl_err);
			}
		}
	}
}

/*
 * This function should NOT be called right after the txpower change because the rssi may come
 * from the original txpower. Make sure txpower and rssi is match.
 */
void ieee80211_scs_node_update_rssi(void *s, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = s;
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	int32_t rssi = SCS_RSSI_UNINITED;
	int32_t prev_atten;
	uint8_t smth_fctr;
	int32_t txpower = ic->ic_curchan->ic_maxpower;
	int32_t atten = SCS_ATTEN_UNINITED;

	if (!ieee80211_scs_node_is_valid(ni)) {
		return;
	}

	/* get recent and smooth */
	rssi = ic->ic_rssi(ni);
	if (SCS_RSSI_VALID(rssi)) {
		atten = txpower - rssi / SCS_RSSI_PRECISION_RECIP;
		if (ni->ni_atten_smoothed == SCS_ATTEN_UNINITED) {
			ni->ni_atten_smoothed = atten;
			SCSDBG(SCSLOG_INFO, "node 0x%x init rssi=%d, atten=%d\n", ni->ni_associd,
				       rssi, ni->ni_atten_smoothed);
		} else {
			/* exponential smooth */
			prev_atten = ni->ni_atten_smoothed;
			smth_fctr = (atten <= prev_atten) ? ic->ic_scs.scs_rssi_smth_fctr[SCS_RSSI_SMTH_FCTR_UP]
							: ic->ic_scs.scs_rssi_smth_fctr[SCS_RSSI_SMTH_FCTR_DOWN];
			ni->ni_atten_smoothed = IEEE80211_SCS_SMOOTH(prev_atten, atten, smth_fctr);
			SCSDBG(SCSLOG_VERBOSE, "node 0x%x prev_atten=%d, curr_rssi=%d, curr_atten=%d, "
					"smooth_factor=%u, smoothed atten=%d\n",
					ni->ni_associd, prev_atten, rssi, atten,
					smth_fctr, ni->ni_atten_smoothed);
		}
	}

	/* report smoothed one to ranking database */
	atten = ni->ni_atten_smoothed;
	if (SCS_ATTEN_VALID(atten)) {
		as->as_sta_atten_num++;
		as->as_sta_atten_sum += atten;
		if ((as->as_sta_atten_min == SCS_ATTEN_UNINITED) || (atten < as->as_sta_atten_min)) {
			as->as_sta_atten_min = atten;
		}
		if ((as->as_sta_atten_max == SCS_ATTEN_UNINITED) || (atten > as->as_sta_atten_max)) {
			as->as_sta_atten_max = atten;
		}
	}
}

void ieee80211_scs_collect_node_atten(struct ieee80211com *ic)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;

	/* clear the attenuation summary stats before collecting from nodes */
	as->as_sta_atten_num = 0;
	as->as_sta_atten_sum = 0;
	as->as_sta_atten_min = SCS_ATTEN_UNINITED;
	as->as_sta_atten_max = SCS_ATTEN_UNINITED;

	/* let all node update their own information into the ranking stats database */
	ic->ic_iterate_nodes(&ic->ic_sta, ieee80211_scs_node_update_rssi, (void *)ic, 1);
	SCSDBG(SCSLOG_INFO, "atten info: num=%d, sum=%d, min=%d, max=%d, avg=%d\n",
			as->as_sta_atten_num, as->as_sta_atten_sum,
			as->as_sta_atten_min, as->as_sta_atten_max,
			as->as_sta_atten_num ? (as->as_sta_atten_sum / as->as_sta_atten_num) : 0);
}

#define SCS_MIN_TX_TIME_FOR_COMP	10 /* ms */
#define SCS_MIN_RX_TIME_FOR_COMP	10 /* ms */
#define SCS_MIN_TDLS_TIME_FOR_COMP	10 /* ms */
#define SCS_TX_TIME_COMP_STEP		50 /* ms */
#define SCS_RX_TIME_COMP_STEP		50 /* ms */
#define SCS_TDLS_TIME_COMP_STEP		50 /* ms */

#define SCS_RX_COMPENSTATION		0
#define SCS_TX_COMPENSTATION		1
#define SCS_TDLS_COMPENSTATION		2

static uint32_t tx_time_compenstation[SCS_MAX_TXTIME_COMP_INDEX] = {30, 35, 40, 45, 50, 50, 50, 50};
static uint32_t rx_time_compenstation[SCS_MAX_RXTIME_COMP_INDEX] = {30, 50, 70, 90, 100, 110, 120, 130};
static uint32_t tdls_time_compenstation[SCS_MAX_RXTIME_COMP_INDEX] = {40, 70, 70, 80, 80, 90, 90, 90};

static void ieee80211_scs_set_time_compensation(uint32_t type, uint32_t index, uint32_t comp)
{
	int i;

	if (type == SCS_RX_COMPENSTATION) {
		if (index >= SCS_MAX_RXTIME_COMP_INDEX) {
			printk("SCS: The index(%u) for rxtime compensation is not correct!\n", index);
		} else {
			rx_time_compenstation[index] = comp;
		}

		printk("Current rx time compensation:\n");
		for (i = 0; i < SCS_MAX_RXTIME_COMP_INDEX; i++) {
			printk("  %u", rx_time_compenstation[i]);
		}
		printk("\n");

	} else if (type == SCS_TX_COMPENSTATION) {
		if (index >= SCS_MAX_TXTIME_COMP_INDEX) {
			printk("SCS: The index(%u) for txtime compensation is not correct!\n", index);
		} else {
			tx_time_compenstation[index] = comp;
		}

		printk("Current tx time compensation:\n");
		for (i = 0; i < SCS_MAX_TXTIME_COMP_INDEX; i++) {
			printk("  %u", tx_time_compenstation[i]);
		}
		printk("\n");
	} else if (type == SCS_TDLS_COMPENSTATION) {
		if (index >= SCS_MAX_TDLSTIME_COMP_INDEX) {
			printk("SCS: The index(%u) for tdlstime compensation is not correct!\n", index);
		} else {
			tdls_time_compenstation[index] = comp;
		}

		printk("Current tdls time compensation:\n");
		for (i = 0; i < SCS_MAX_TDLSTIME_COMP_INDEX; i++) {
			printk("  %u", tdls_time_compenstation[i]);
		}
		printk("\n");
	}
}

/* Add some compensation because (tx_time+rx_time) always is less than the caused cca interference */
static uint32_t ieee80211_scs_get_time_compensation(uint32_t tx_time, uint32_t rx_time)
{
	uint32_t rx_comp, tx_comp;
	uint32_t index;

	if (rx_time > SCS_MIN_RX_TIME_FOR_COMP) {
		index = rx_time / SCS_RX_TIME_COMP_STEP;
		index = (index >= SCS_MAX_RXTIME_COMP_INDEX) ? (SCS_MAX_RXTIME_COMP_INDEX - 1) : index;
		rx_comp = rx_time_compenstation[index];
	} else {
		/* if only downstream traffic or no traffic, don't add compensation */
		return 0;
	}

	if (tx_time > SCS_MIN_TX_TIME_FOR_COMP) {
		index = tx_time / SCS_TX_TIME_COMP_STEP;
		index = (index >= SCS_MAX_TXTIME_COMP_INDEX) ? (SCS_MAX_TXTIME_COMP_INDEX - 1) : index;
		tx_comp = tx_time_compenstation[index];
	} else {
		tx_comp = 0;
	}

	return (tx_comp + rx_comp);
}

#define SCS_MIN_STATS_FOR_STABLE_CHECK		30
static uint32_t ieee80211_scs_is_stats_unstable(struct ieee80211com *ic,
			uint32_t last_stats, uint32_t new_stats)
{
	uint32_t diff, sum;

	if (last_stats < SCS_MIN_STATS_FOR_STABLE_CHECK && new_stats < SCS_MIN_STATS_FOR_STABLE_CHECK) {
		return 0;
	}

	diff = (last_stats > new_stats) ? (last_stats - new_stats) : (new_stats - last_stats);
	sum = last_stats + new_stats;

	if (diff > ic->ic_scs.scs_pmp_stats_stable_range) {
		return 1;
	}

	if ((diff * 100 / sum) > ic->ic_scs.scs_pmp_stats_stable_percent) {
		return 1;
	}

	return 0;
}

static void ieee80211_scs_clear_node_smooth_data(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		return;

	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (!ieee80211_scs_node_is_valid(ni))
			continue;

		ni->ni_recent_cca_intf_smthed = 0;
		ni->ni_others_rx_time_smthed = 0;
		ni->ni_others_tx_time_smthed = 0;
		ni->ni_tdls_tx_time_smthed = 0;
		ni->ni_tdls_rx_time_smthed = 0;
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);
}

static uint16_t ieee80211_scs_ap_get_tdls_link_time(struct ieee80211com *ic)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	struct ieee80211_node *ni_tmp;
	uint16_t ap_tdls_time = 0;
	uint16_t tdls_comp;
	uint16_t index;

	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, ni_tmp)
		ap_tdls_time += ni->ni_tdls_tx_time_smthed;

	/*  calculate compensation */
	if (ap_tdls_time > SCS_MIN_TDLS_TIME_FOR_COMP) {
		index = ap_tdls_time / SCS_TDLS_TIME_COMP_STEP;
		index = (index >= SCS_MAX_TDLSTIME_COMP_INDEX) ?
					(SCS_MAX_TDLSTIME_COMP_INDEX - 1) : index;
		tdls_comp = tdls_time_compenstation[index];
	} else {
		tdls_comp = 0;
	}

	return (ap_tdls_time + tdls_comp);
}

static uint32_t ieee80211_scs_smooth_ap_cca_intf_time(struct ieee80211com *ic,
		uint32_t raw_cca_intf, uint32_t *stats_unstable)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	uint32_t ap_tdls_intf;
	uint32_t corrective_cca_intf;
	uint32_t compound_cca_intf;

	if ((jiffies - ic->ic_scs.scs_cca_intf_smthed_jiffies) >
			(ic->ic_scs.scs_pmp_stats_clear_interval * HZ)) {
		as->as_tx_ms_smth = 0;
		as->as_rx_ms_smth = 0;
		ic->ic_scs.scs_cca_intf_smthed = 0;
		ieee80211_scs_clear_node_smooth_data(ic);
	}

	*stats_unstable = ieee80211_scs_is_stats_unstable(ic,
			ic->ic_scs.scs_cca_intf_smthed, raw_cca_intf);
	*stats_unstable |= ((jiffies - ic->ic_scs.scs_cca_intf_smthed_jiffies) >
			(5 * ic->ic_scs.scs_cca_sample_dur * HZ));

	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		ni->ni_tdls_rx_time_smthed = IEEE80211_SCS_SMOOTH(ni->ni_tdls_rx_time_smthed,
				ni->ni_recent_tdls_rx_time, ic->ic_scs.scs_pmp_rx_time_smth_fctr);
		ni->ni_tdls_tx_time_smthed = IEEE80211_SCS_SMOOTH(ni->ni_tdls_tx_time_smthed,
				ni->ni_recent_tdls_tx_time, ic->ic_scs.scs_pmp_tx_time_smth_fctr);
		ni->ni_tdls_time_smth_jiffies = jiffies;
		SCSDBG(SCSLOG_INFO, "STA 0x%x - tdls_tx_time_smth: %u, tdls_rx_time_smth=%u\n",
				ni->ni_associd, ni->ni_tdls_tx_time_smthed, ni->ni_tdls_rx_time_smthed);
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	ic->ic_scs.scs_cca_intf_smthed = IEEE80211_SCS_SMOOTH(ic->ic_scs.scs_cca_intf_smthed,
			raw_cca_intf, ic->ic_scs.scs_pmp_rpt_cca_smth_fctr);
	ic->ic_scs.scs_cca_intf_smthed_jiffies = jiffies;

	ap_tdls_intf = ieee80211_scs_ap_get_tdls_link_time(ic);
	if (ic->ic_scs.scs_cca_intf_smthed > ap_tdls_intf)
		corrective_cca_intf = ic->ic_scs.scs_cca_intf_smthed - ap_tdls_intf;
	else
		corrective_cca_intf = 0;

	compound_cca_intf = ieee80211_scs_fix_cca_intf(ic, NULL, corrective_cca_intf,
				ic->ic_scs.scs_sp_err_smthed, ic->ic_scs.scs_lp_err_smthed);

	SCSDBG(SCSLOG_INFO, "raw_cca_int %u, cca_int_smthed %u, ap_tdls_intf %u,"
			" corrective_cca_intf %u, compound_cca_intf %u, stats_unstable %u\n",
			raw_cca_intf, ic->ic_scs.scs_cca_intf_smthed, ap_tdls_intf,
			corrective_cca_intf, compound_cca_intf, *stats_unstable);

	return compound_cca_intf;
}

static uint32_t ieee80211_scs_smooth_sta_cca_intf_time(struct ieee80211com *ic,
		struct ieee80211_node *ni, uint32_t total_tx_time, uint32_t total_rx_time,
		uint32_t node_tx_time, uint32_t node_rx_time)
{
	uint32_t others_tx_time, others_rx_time;
	uint32_t others_time, others_time_smth;
	uint32_t is_stats_unstable;
	int32_t node_cca_intf;

	/* get other nodes' tx/rx time and smooth them */
	others_tx_time = (total_tx_time > node_tx_time) ? (total_tx_time - node_tx_time) : 0;
	others_rx_time = (total_rx_time > node_rx_time) ? (total_rx_time - node_rx_time) : 0;
	others_time = others_rx_time + others_tx_time;
	others_time_smth = ni->ni_others_rx_time_smthed + ni->ni_others_tx_time_smthed; /* old smooth value */
	is_stats_unstable = ieee80211_scs_is_stats_unstable(ic, others_time_smth, others_time);
	is_stats_unstable |= ((jiffies - ni->ni_others_time_smth_jiffies) > (5 * ic->ic_scs.scs_cca_sample_dur * HZ));

	SCSDBG(SCSLOG_NOTICE, "node 0x%x time before smth -- self:(tx:%u, rx:%u), others:%u(tx:%u, rx:%u),"
			"others_smth:%u(tx:%u, rx:%u), tdls:(tx:%u, rx:%u), tdls_smth:(tx:%u, rx:%u)\n",
			ni->ni_associd, node_tx_time, node_rx_time, others_time, others_tx_time, others_rx_time,
			others_time_smth, ni->ni_others_tx_time_smthed, ni->ni_others_rx_time_smthed,
			ni->ni_recent_tdls_tx_time, ni->ni_recent_tdls_rx_time,
			ni->ni_tdls_tx_time_smthed, ni->ni_tdls_rx_time_smthed);

	ni->ni_others_rx_time_smthed = IEEE80211_SCS_SMOOTH(ni->ni_others_rx_time_smthed, others_rx_time,
			ic->ic_scs.scs_pmp_rx_time_smth_fctr);
	ni->ni_others_tx_time_smthed = IEEE80211_SCS_SMOOTH(ni->ni_others_tx_time_smthed, others_tx_time,
			ic->ic_scs.scs_pmp_tx_time_smth_fctr);
	ni->ni_others_time_smth_jiffies = jiffies;

	is_stats_unstable |= ieee80211_scs_is_stats_unstable(ic, ni->ni_tdls_rx_time_smthed, ni->ni_recent_tdls_rx_time);
	is_stats_unstable |= ieee80211_scs_is_stats_unstable(ic, ni->ni_tdls_tx_time_smthed, ni->ni_recent_tdls_tx_time);
	is_stats_unstable |= ((jiffies - ni->ni_tdls_time_smth_jiffies) > (5 * ic->ic_scs.scs_cca_sample_dur * HZ));

	/* smooth cca interference */
	node_cca_intf = ni->ni_recent_cca_intf;
	if ((jiffies - ni->ni_recent_cca_intf_jiffies) < (ic->ic_scs.scs_cca_sample_dur * HZ) &&
			node_cca_intf != SCS_CCA_INTF_INVALID) {
		SCSDBG(SCSLOG_NOTICE, "node 0x%x cca intf before smth -- smthed:%u, recent_cca_intf:%u\n",
				ni->ni_associd, ni->ni_recent_cca_intf_smthed, node_cca_intf);

		is_stats_unstable |= ieee80211_scs_is_stats_unstable(ic,
				ni->ni_recent_cca_intf_smthed, node_cca_intf);
		is_stats_unstable |= ((jiffies - ni->ni_cca_intf_smth_jiffies) > (5 * ic->ic_scs.scs_cca_sample_dur * HZ));

		ni->ni_recent_cca_intf_smthed = IEEE80211_SCS_SMOOTH(ni->ni_recent_cca_intf_smthed, node_cca_intf,
				ic->ic_scs.scs_pmp_rpt_cca_smth_fctr);
		ni->ni_cca_intf_smth_jiffies = jiffies;
	} else {
		is_stats_unstable = 1;
	}

	return is_stats_unstable;
}

#define ADD_SCS_TDLS_STATS(frm, s_addr, r_addr, tx_time, is_latest)	\
do {	\
	IEEE80211_ADDR_COPY(frm, s_addr);	\
	frm += IEEE80211_ADDR_LEN;	\
	IEEE80211_ADDR_COPY(frm, r_addr);	\
	frm += IEEE80211_ADDR_LEN;	\
	ADDINT16LE(frm, tx_time);	\
	ADDINT16LE(frm, is_latest);	\
} while(0)

static int ieee80211_scs_add_tdls_stats_ie(struct ieee80211vap *vap,
	struct qtn_scs_info *scs_info_read, uint8_t *frm, uint16_t frm_len)
{
#define	IEEE80211_SCS_TDLS_TRAINING_DUATION		4
#define IEEE80211_SCS_TDLS_TRAINING_COMPANSATION	20
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	struct qtn_scs_vsp_node_stats *stats;
	uint8_t *end = frm + frm_len;
	int tdls_tx_time = 0;
	int ie_len = 0;

	if (vap->iv_opmode != IEEE80211_M_STA || (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED))
		return ie_len;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (!ieee80211_scs_node_is_valid(ni))
			continue;

		if (IEEE80211_NODE_IS_NONE_TDLS(ni) || IEEE80211_NODE_IS_TDLS_INACTIVE(ni))
			continue;

		stats = ieee80211_scs_find_node_stats(ic, scs_info_read, ni->ni_associd);
		if (stats)
			tdls_tx_time = stats->tx_usecs / 1000;
		else
			tdls_tx_time = 0;

		/*
		 * Add some compansation since training packets could
		 * cause high interference in bad environment
		 */
		if (time_after(jiffies, ni->ni_training_start) &&
				time_before(jiffies, ni->ni_training_start +
					IEEE80211_SCS_TDLS_TRAINING_DUATION * HZ))
			tdls_tx_time += IEEE80211_SCS_TDLS_TRAINING_COMPANSATION;

		if (frm < end) {
			ADD_SCS_TDLS_STATS(frm, vap->iv_myaddr, ni->ni_macaddr, tdls_tx_time, 1);
			ie_len += sizeof(struct ieee80211_tdls_scs_stats);
			SCSDBG(SCSLOG_NOTICE, "Add SCS TDLS status: sender_mac %pM "
					"receiver_mac %pM tx_time %u\n",vap->iv_myaddr,
					ni->ni_macaddr, tdls_tx_time);
		} else {
			SCSDBG(SCSLOG_NOTICE, "Failed to add tdls stats IE\n");
		}
	}

	return ie_len;
}

void ieee80211_scs_update_tdls_stats(struct ieee80211com *ic,
		struct ieee80211_tdls_scs_stats *scs_stats)
{
	int found = 0;
	unsigned long flags;
	struct ieee80211_tdls_scs_entry *scs_entry = NULL;
	int hash = IEEE80211_NODE_HASH(scs_stats->s_addr);

	SCSDBG(SCSLOG_INFO, "Update SCS TDLS status: sender_mac %pM "
					"receiver_mac %pM tx_time %u\n", scs_stats->s_addr,
					scs_stats->r_addr, le16toh(scs_stats->tx_time));

	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	LIST_FOREACH(scs_entry, &ic->ic_scs.scs_tdls_list[hash], entry) {
		if (IEEE80211_ADDR_EQ(scs_entry->stats.s_addr, scs_stats->s_addr) &&
				IEEE80211_ADDR_EQ(scs_entry->stats.r_addr, scs_stats->r_addr)) {
			scs_entry->stats.is_latest = 1;
			scs_entry->stats.tx_time = le16toh(scs_stats->tx_time);
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);

	if (found == 0) {
		MALLOC(scs_entry, struct ieee80211_tdls_scs_entry *,
					sizeof(*scs_entry), M_DEVBUF, M_WAITOK);
		if (scs_entry != NULL) {
			IEEE80211_ADDR_COPY(scs_entry->stats.s_addr, scs_stats->s_addr);
			IEEE80211_ADDR_COPY(scs_entry->stats.r_addr, scs_stats->r_addr);
			scs_entry->stats.is_latest = 1;
			scs_entry->stats.tx_time = le16toh(scs_stats->tx_time);
			spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
			LIST_INSERT_HEAD(&ic->ic_scs.scs_tdls_list[hash], scs_entry, entry);
			spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);
		} else {
			SCSDBG(SCSLOG_NOTICE, "SCS TDLS entry allocation failed\n");
		}
	}
}

void ieee80211_scs_free_node_tdls_stats(struct ieee80211com *ic,
		struct ieee80211_node *ni)
{
	int i;
	int freed = 0;
	unsigned long flags;
	struct ieee80211_tdls_scs_entry *scs_entry;
	struct ieee80211_tdls_scs_entry *temp_entry;

	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH_SAFE(scs_entry, &ic->ic_scs.scs_tdls_list[i], entry, temp_entry) {
			if ((IEEE80211_ADDR_EQ(scs_entry->stats.s_addr, ni->ni_macaddr) ||
					IEEE80211_ADDR_EQ(scs_entry->stats.r_addr, ni->ni_macaddr))) {
				LIST_REMOVE(scs_entry, entry);
				FREE(scs_entry, M_DEVBUF);
				freed = 1;
			}
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);
	if (freed)
		SCSDBG(SCSLOG_NOTICE, "free node %pM tdls stats\n", ni->ni_macaddr);
}

void ieee80211_scs_free_tdls_stats_list(struct ieee80211com *ic)
{
	int i;
	unsigned long flags;
	struct ieee80211_tdls_scs_entry *scs_entry;
	struct ieee80211_tdls_scs_entry *temp_entry;

	SCSDBG(SCSLOG_NOTICE, "SCS: free all of tdls stats\n");
	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH_SAFE(scs_entry, &ic->ic_scs.scs_tdls_list[i], entry, temp_entry) {
			LIST_REMOVE(scs_entry, entry);
			FREE(scs_entry, M_DEVBUF);
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);
}

static void ieee80211_scs_dump_tdls_stats(struct ieee80211com *ic)
{
	int i;
	unsigned long flags;
	struct ieee80211_tdls_scs_entry *scs_entry = NULL;

	SCSDBG(SCSLOG_INFO, "Dump SCS tdls stats:\n");
	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH(scs_entry, &ic->ic_scs.scs_tdls_list[i], entry) {
			if (scs_entry != NULL) {
				SCSDBG(SCSLOG_VERBOSE, "sender_mac %pM, receiver_mac %pM,"
					" tx_time: %u\n", scs_entry->stats.s_addr,
					scs_entry->stats.r_addr, scs_entry->stats.tx_time);
			}
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);
}

static void ieee80211_scs_update_current_tdls_time(struct ieee80211com *ic,
			struct ieee80211_node *ni)
{
	struct ieee80211_tdls_scs_entry *scs_entry = NULL;
	int hash = IEEE80211_NODE_HASH(ni->ni_macaddr);
	unsigned long flags;
	int i;

	ni->ni_recent_tdls_tx_time = 0;
	ni->ni_recent_tdls_rx_time = 0;

	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	LIST_FOREACH(scs_entry, &ic->ic_scs.scs_tdls_list[hash], entry) {
		if (IEEE80211_ADDR_EQ(scs_entry->stats.s_addr, ni->ni_macaddr)) {
			ni->ni_recent_tdls_tx_time += scs_entry->stats.tx_time;
			SCSDBG(SCSLOG_VERBOSE, "Node %pM tdls_tx_time: sender_mac %pM,"
				" receiver_mac %pM, tx_time: %u\n", ni->ni_macaddr,
				scs_entry->stats.s_addr, scs_entry->stats.r_addr,
				scs_entry->stats.tx_time);
		}
	}

	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH(scs_entry, &ic->ic_scs.scs_tdls_list[i], entry) {
			if (IEEE80211_ADDR_EQ(scs_entry->stats.r_addr, ni->ni_macaddr)) {
				ni->ni_recent_tdls_rx_time += scs_entry->stats.tx_time;
				SCSDBG(SCSLOG_VERBOSE, "Node %pM tdls_rx_time: sender_mac %pM,"
					" receiver_mac %pM, rx_time: %u\n", ni->ni_macaddr,
					scs_entry->stats.s_addr, scs_entry->stats.r_addr,
					scs_entry->stats.tx_time);
			}
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);

	SCSDBG(SCSLOG_INFO, "Node %pM, recent_tdls_tx_time %u, recent_tdls_rx_time %u\n",
			ni->ni_macaddr, ni->ni_recent_tdls_tx_time, ni->ni_recent_tdls_rx_time);
}

static int ieee80211_scs_update_tdls_link_time(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	int failed = 0;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		return failed;

	ieee80211_scs_dump_tdls_stats(ic);

	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_vap->iv_opmode == IEEE80211_M_WDS)
			continue;

		if (!ieee80211_scs_node_is_valid(ni))
			continue;

		/*
		 * We must confirm all of CCA actions are received and all of CCA info are correct,
		 * or we will get wrong total TDLS link time and calculate wrong CCA interference
		 */
		if (ni->ni_recent_cca_intf == SCS_CCA_INTF_INVALID) {
			failed = 1;
			SCSDBG(SCSLOG_NOTICE, "Get wrong CCA info from STA 0x%x\n", ni->ni_associd);
			break;
		}

		ieee80211_scs_update_current_tdls_time(ic, ni);
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	return failed;
}

static uint16_t ieee80211_scs_sta_get_tdls_link_time(struct ieee80211com *ic,
		struct ieee80211_node *cur_ni)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;;
	struct ieee80211_node *ni;
	uint16_t sta_tdls_time = 0;
	uint16_t tdls_comp;
	uint16_t index;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		return 0;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni != cur_ni)
			sta_tdls_time += ni->ni_tdls_tx_time_smthed;
	}

	sta_tdls_time = (sta_tdls_time > cur_ni->ni_tdls_rx_time_smthed) ?
			(sta_tdls_time - cur_ni->ni_tdls_rx_time_smthed) : 0;

	/* Add some compensation */
	if (sta_tdls_time > SCS_MIN_TDLS_TIME_FOR_COMP) {
		index = sta_tdls_time / SCS_TDLS_TIME_COMP_STEP;
		index = (index >= SCS_MAX_TDLSTIME_COMP_INDEX) ?
					(SCS_MAX_TDLSTIME_COMP_INDEX - 1) : index;
		tdls_comp = tdls_time_compenstation[index];
	} else {
		tdls_comp = 0;
	}

	return (sta_tdls_time + tdls_comp);
}

static uint32_t ieee80211_scs_tdls_link_is_existing(struct ieee80211com *ic)
{
	int i;
	unsigned long flags;
	struct ieee80211_tdls_scs_entry *scs_entry = NULL;
	uint32_t is_existing = 0;

	spin_lock_irqsave(&ic->ic_scs.scs_tdls_lock, flags);
	for (i = 0; i < IEEE80211_NODE_HASHSIZE; i++) {
		LIST_FOREACH(scs_entry, &ic->ic_scs.scs_tdls_list[i], entry) {
			if (scs_entry != NULL && scs_entry->stats.is_latest) {
				is_existing = 1;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&ic->ic_scs.scs_tdls_lock, flags);
	return is_existing;
}

int ieee80211_scs_is_wds_rbs_node(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	int is_wds_rbs = 0;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (IEEE80211_VAP_WDS_IS_RBS(vap)) {
			SCSDBG(SCSLOG_INFO, "channel change is disabled under RBS WDS mode\n");
			is_wds_rbs = 1;
			break;
		}
	}
	return is_wds_rbs;
}
EXPORT_SYMBOL(ieee80211_scs_is_wds_rbs_node);

static int
ieee80211_scs_is_wds_mbs_node(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	int is_wds_mbs = 0;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (IEEE80211_VAP_WDS_IS_MBS(vap)) {
			is_wds_mbs = 1;
			break;
		}
	}
	return is_wds_mbs;
}

static int ieee80211_wds_vap_exists(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if ((vap->iv_opmode == IEEE80211_M_WDS) && (vap->iv_state == IEEE80211_S_RUN)) {
			return 1;
		}
	}

	return 0;
}

static uint32_t
ieee80211_adjust_others_time_on_mbs(struct ieee80211com *ic, uint32_t compound_cca_intf)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ap_state *as = ic->ic_scan->ss_scs_priv;

	SCSDBG(SCSLOG_INFO, "compound_cca_intf before %u\n", compound_cca_intf);
	as->as_cca_intf_smth = IEEE80211_SCS_SMOOTH(as->as_cca_intf_smth, compound_cca_intf, IEEE80211_SCS_SMTH_RBS_TIME);
	if (as->as_cca_intf_smth > compound_cca_intf) {
		/* Smooth rising up; but sharp drop */
		as->as_cca_intf_smth = compound_cca_intf;
	}
	SCSDBG(SCSLOG_INFO, "compound_cca_intf smoothed %u\n", as->as_cca_intf_smth);
	compound_cca_intf = as->as_cca_intf_smth;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (!ieee80211_scs_node_is_valid(ni)) {
			continue;
		}
		compound_cca_intf = (compound_cca_intf > ni->ni_recent_others_time_smth) ?
					(compound_cca_intf - ni->ni_recent_others_time_smth) : 0;

		SCSDBG(SCSLOG_INFO, "compound_cca_intf %u others_time %u\n", compound_cca_intf, ni->ni_recent_others_time_smth);
	}
	SCSDBG(SCSLOG_INFO, "compound_cca_intf after %u\n", compound_cca_intf);
	return compound_cca_intf;
}

static uint32_t
ieee80211_adjust_node_rbs_others_time(struct ieee80211com *ic, uint32_t node_cca_intf)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;

	SCSDBG(SCSLOG_INFO, "node_cca_intf before %u\n", node_cca_intf);

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (!ieee80211_scs_node_is_valid(ni)) {
			continue;
		}
		node_cca_intf = (node_cca_intf > ni->ni_recent_others_time_smth) ?
					(node_cca_intf - ni->ni_recent_others_time_smth) : 0;
	}
	SCSDBG(SCSLOG_INFO, "node_cca_intf after %u\n", node_cca_intf);
	return node_cca_intf;
}

uint32_t ieee80211_scs_collect_ranking_stats(struct ieee80211com *ic, struct qtn_scs_info *scs_info_read, uint32_t cc_flag, uint32_t compound_cca_intf)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt;
	int cca_intf_is_recent = 0;
	uint32_t node_num[SCS_NODE_TRAFFIC_TYPE_NUM][SCS_NODE_INTF_TYPE_NUM];
	uint32_t traffic_idx, intf_idx;
	uint32_t intf_max;
	struct ieee80211_node *intf_max_ni;
	struct qtn_scs_vsp_node_stats *stats;
	uint32_t node_time, node_tx_time, node_rx_time;
	uint32_t others_time_smth, others_time_comp;
	uint32_t total_tx_time, total_rx_time;
	uint32_t is_stats_unstable;
	uint32_t new_cc_flag = cc_flag;
	int32_t node_cca_intf, ap_cca_intf, node_compound_cca_intf;
	uint32_t pmbl_max, node_pmbl, ap_pmbl;
	int cur_bw = ieee80211_get_bw(ic);
	int is_wds_mbs = ieee80211_scs_is_wds_mbs_node(ic);
	uint16_t ap_tdls_intf;
	uint16_t sta_tdls_intf;

	ap_tdls_intf = ieee80211_scs_ap_get_tdls_link_time(ic);
	ap_cca_intf = (ic->ic_scs.scs_cca_intf_smthed > ap_tdls_intf) ?
			(ic->ic_scs.scs_cca_intf_smthed - ap_tdls_intf) : 0;
	ap_pmbl = (ic->ic_scs.scs_sp_err_smthed * ic->ic_scs.scs_sp_wf +
				ic->ic_scs.scs_lp_err_smthed * ic->ic_scs.scs_lp_wf) / 100;

	/*
	 * Currently only reset current channel's cca_intf, because other channel's cca_intf
	 * are updated more slowly.
	 */
	ieee80211_scs_update_chans_cca_intf(ic, ic->ic_curchan, cur_bw, IEEE80211_SCS_COCHAN,
				SCS_CCA_INTF_INVALID, IEEE80211_SCS_CCA_INTF_SCALE, NULL, 0);

	/* collect interference info from all stas */
	intf_max = SCS_CCA_INTF_INVALID;
	pmbl_max = 0;
	intf_max_ni = NULL;
	memset(node_num, 0x0, sizeof(node_num));
	nt = &ic->ic_sta;
	IEEE80211_SCAN_LOCK_IRQ(nt);
	IEEE80211_NODE_LOCK_IRQ(nt);

	total_tx_time = scs_info_read->tx_usecs / 1000;
	total_rx_time = scs_info_read->rx_usecs / 1000;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (!ieee80211_scs_node_is_valid(ni)) {
			continue;
		}

		/* traffic condition */
		stats = ieee80211_scs_find_node_stats(ic, scs_info_read, ni->ni_associd);
		if (stats) {
			node_tx_time = stats->tx_usecs / 1000;
			node_rx_time = stats->rx_usecs / 1000;
			node_time = node_tx_time + node_rx_time;
		} else {
			SCSDBG(SCSLOG_NOTICE, "no stats available for node 0x%x\n", ni->ni_associd);
			node_tx_time = 0;
			node_rx_time = 0;
			node_time = 0;
		}
		node_time += ni->ni_recent_tdls_tx_time;

		is_stats_unstable = ieee80211_scs_smooth_sta_cca_intf_time(ic, ni, total_tx_time, total_rx_time,
				node_tx_time, node_rx_time);
		others_time_smth = ni->ni_others_rx_time_smthed + ni->ni_others_tx_time_smthed; /* new smooth value */
		others_time_comp = ieee80211_scs_get_time_compensation(ni->ni_others_tx_time_smthed,
				ni->ni_others_rx_time_smthed);
		others_time_smth = others_time_smth + others_time_comp;
		ni->ni_others_time = others_time_smth;	/* This is used to compensate for RBS */
		sta_tdls_intf = ieee80211_scs_sta_get_tdls_link_time(ic, ni);

		SCSDBG(SCSLOG_NOTICE, "node 0x%x time after smth and compensation -- others_smth:%u "
				"(tx:%u, rx:%u, comp:%u), sta_tdls_intf: %u\n", ni->ni_associd,	others_time_smth,
				ni->ni_others_tx_time_smthed, ni->ni_others_rx_time_smthed, others_time_comp, sta_tdls_intf);

		traffic_idx = (node_time <= (ic->ic_scs.scs_thrshld_loaded * scs_info_read->cca_try / 1000)) ?
				SCS_NODE_TRAFFIC_IDLE : SCS_NODE_TRAFFIC_LOADED;
		/* inteference condition */
		node_cca_intf = (int32_t)ni->ni_recent_cca_intf;
		node_pmbl = 0;
		cca_intf_is_recent = (jiffies - ni->ni_recent_cca_intf_jiffies) < (ic->ic_scs.scs_cca_sample_dur * HZ);
		intf_idx = ((node_cca_intf != SCS_CCA_INTF_INVALID) && (cca_intf_is_recent)) ?
				SCS_NODE_INTFED : SCS_NODE_NOTINTFED;

		if (intf_idx == SCS_NODE_INTFED) {
			/* vendor specific handle */
			if ((ni->ni_vendor == PEER_VENDOR_QTN)
					|| IEEE80211_VAP_WDS_IS_MBS(ni->ni_vap)) {
				if (ic->ic_sta_assoc >= IEEE80211_MAX_STA_CCA_ENABLED) {
					/* PMP case */
					if (is_stats_unstable) {
						/* stats is unstable, ignore it */
						node_cca_intf = 0;
					} else {
						/* remove the interference that come from other associated station */
						node_cca_intf = (ni->ni_recent_cca_intf_smthed > others_time_smth) ?
								(ni->ni_recent_cca_intf_smthed - others_time_smth) : 0;
						node_cca_intf = (node_cca_intf > sta_tdls_intf) ? (node_cca_intf - sta_tdls_intf) : 0;
						node_cca_intf = MIN(node_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE);
					}
					SCSDBG(SCSLOG_NOTICE, "node 0x%x -- cca_smth: %u, others_time_smth: %u,"
							"sta_tdls_inf: %u, diff: %d, node_cca_intf: %u, is_stable: %u\n",
							ni->ni_associd,	ni->ni_recent_cca_intf_smthed, others_time_smth, sta_tdls_intf,
							(int)(ni->ni_recent_cca_intf_smthed - others_time_smth - sta_tdls_intf),
							node_cca_intf, !is_stats_unstable);
				}
				if (is_wds_mbs) {
					/* Is this node a non-WDS node ? */
					if (!(IEEE80211_VAP_WDS_ANY(ni->ni_vap))) {
						node_cca_intf = ieee80211_adjust_node_rbs_others_time(ic, node_cca_intf);
					} else {
						SCSDBG(SCSLOG_NOTICE, "WDS Node; No adjustment needed\n");
					}
				}
				node_compound_cca_intf = ieee80211_scs_fix_cca_intf(ic, ni, node_cca_intf,
						ni->ni_recent_sp_fail, ni->ni_recent_lp_fail);
				node_pmbl = (ni->ni_recent_sp_fail * ic->ic_scs.scs_sp_wf +
						ni->ni_recent_lp_fail * ic->ic_scs.scs_lp_wf) / 100;

				if (ieee80211_scs_is_interference_over_thresholds(ic,
						node_compound_cca_intf, ic->ic_scs.scs_cca_idle_smthed, node_pmbl)) {
					if (!(cc_flag & IEEE80211_SCS_SELF_CCA_CC) && (node_cca_intf > ap_cca_intf)) {
						SCSDBG(SCSLOG_NOTICE, "increase cca_intf for sta side ACI, sta cca_intf=%u,"
								" self cca_intf=%u\n", node_cca_intf, ap_cca_intf);
						node_cca_intf = node_cca_intf * 2 - ap_cca_intf;
						node_cca_intf = MIN(node_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE);
						node_compound_cca_intf = ieee80211_scs_fix_cca_intf(ic, ni, node_cca_intf,
								ni->ni_recent_sp_fail, ni->ni_recent_lp_fail);
					}
					node_cca_intf = node_compound_cca_intf;
				} else {
					intf_idx = SCS_NODE_NOTINTFED;
					node_pmbl = 0; /* don't record it */
				}
			} else if (!(cc_flag & IEEE80211_SCS_SELF_CCA_CC) && (node_cca_intf > ap_cca_intf)) {
				SCSDBG(SCSLOG_NOTICE, "increase cca_intf for sta side ACI, sta cca_intf=%u,"
						" self cca_intf=%u\n", node_cca_intf, ap_cca_intf);
				node_cca_intf = node_cca_intf * 2 - ap_cca_intf;
				node_cca_intf = MIN(node_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE);
			}
		}
		SCSDBG(SCSLOG_NOTICE, "node 0x%x: vendor=%d, traffic=%d(time=%u), intfed=%d, cca_intf=%d\n",
					ni->ni_associd, ni->ni_vendor, traffic_idx, node_time, intf_idx,
					(intf_idx == SCS_NODE_INTFED) ? node_cca_intf : -1);
		node_num[traffic_idx][intf_idx]++;
		if (intf_idx == SCS_NODE_INTFED) {
			if ((node_cca_intf > intf_max)
				|| (intf_max == SCS_CCA_INTF_INVALID)) {
				intf_max = node_cca_intf;
				intf_max_ni = ni;
			}
		}
		pmbl_max = MAX(node_pmbl, pmbl_max);

		ni->ni_recent_cca_intf = SCS_CCA_INTF_INVALID;
	}

	/* Handle loaded and idle cases */
	SCSDBG(SCSLOG_NOTICE, "traffic-intf matrix: loaded_intfed=%d, loaded_notintfed=%d, "
				"idle_intfed=%d, idle_notintfed=%d\n",
				node_num[SCS_NODE_TRAFFIC_LOADED][SCS_NODE_INTFED],
				node_num[SCS_NODE_TRAFFIC_LOADED][SCS_NODE_NOTINTFED],
				node_num[SCS_NODE_TRAFFIC_IDLE][SCS_NODE_INTFED],
				node_num[SCS_NODE_TRAFFIC_IDLE][SCS_NODE_NOTINTFED]);
	if (node_num[SCS_NODE_TRAFFIC_IDLE][SCS_NODE_INTFED] &&
		!node_num[SCS_NODE_TRAFFIC_LOADED][SCS_NODE_INTFED] &&
		node_num[SCS_NODE_TRAFFIC_LOADED][SCS_NODE_NOTINTFED]) {
		SCSDBG(SCSLOG_NOTICE, "discard intfed idle sta report because loaded sta is not intfed\n");

		new_cc_flag &= ~(IEEE80211_SCS_STA_CCA_REQ_CC | IEEE80211_SCS_BRCM_STA_TRIGGER_CC);
	} else if (node_num[SCS_NODE_TRAFFIC_IDLE][SCS_NODE_INTFED] +
			node_num[SCS_NODE_TRAFFIC_LOADED][SCS_NODE_INTFED]) {
		SCSDBG(SCSLOG_NOTICE, "stas are under interference\n");
		if (intf_max_ni)
			memcpy(ic->ic_csw_mac, intf_max_ni->ni_macaddr, IEEE80211_ADDR_LEN);
		ieee80211_scs_update_chans_cca_intf(ic, ic->ic_curchan, cur_bw, IEEE80211_SCS_COCHAN,
				intf_max, IEEE80211_SCS_CCA_INTF_SCALE, intf_max_ni, pmbl_max);
		if (!(new_cc_flag & (IEEE80211_SCS_STA_CCA_REQ_CC | IEEE80211_SCS_BRCM_STA_TRIGGER_CC))) {
			new_cc_flag |= IEEE80211_SCS_STA_CCA_REQ_CC;
		}
	} else {
		new_cc_flag &= ~(IEEE80211_SCS_STA_CCA_REQ_CC | IEEE80211_SCS_BRCM_STA_TRIGGER_CC);

		SCSDBG(SCSLOG_NOTICE, "sta are free of interference\n");
	}

	IEEE80211_NODE_UNLOCK_IRQ(nt);
	IEEE80211_SCAN_UNLOCK_IRQ(nt);

	/* Adjust self intf on mbs-wds */
	if (is_wds_mbs) {
		compound_cca_intf = ieee80211_adjust_others_time_on_mbs(ic, compound_cca_intf);
	}

	/* update self's */
	ieee80211_scs_update_chans_cca_intf(ic, ic->ic_curchan, cur_bw, IEEE80211_SCS_COCHAN,
			compound_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE, NULL, ap_pmbl);

	/* Recheck the interference detected by AP self */
	if (ieee80211_scs_is_interference_over_thresholds(ic,
			compound_cca_intf, ic->ic_scs.scs_cca_idle_smthed, ap_pmbl)) {
		new_cc_flag |= IEEE80211_SCS_SELF_CCA_CC;
		SCSDBG(SCSLOG_NOTICE, "Recheck Self CCA intf, need to change channel"
				" compound_cca_intf=%u, cca_idle_smth=%u, pmbl_err=%u\n",
				compound_cca_intf, ic->ic_scs.scs_cca_idle_smthed, ap_pmbl);
	} else {
		new_cc_flag &= ~IEEE80211_SCS_SELF_CCA_CC;
		SCSDBG(SCSLOG_NOTICE, "Recheck Self CCA intf, needn't to change channel"
				" compound_cca_intf=%u, cca_idle_smth=%u, pmbl_err=%u\n",
				compound_cca_intf, ic->ic_scs.scs_cca_idle_smthed, ap_pmbl);
	}

	return new_cc_flag;
}

/*
 * Calculate the rate ratio based on attenuation
 * return unit: percent
 * If no attenuation available, return 100
 * For domains other than us, return 100
 * Otherwise, calculate rate ratio according to attenuation.
 */
#define SCS_RATE_RATIO_QTN		0
#define SCS_RATE_RATIO_NONQTN		1
#define SCS_RATE_RATIO_NUM		2
#define SCS_RATE_RATIO_ENTRY_MAX	9
#define SCS_RATE_RATIO_DEFAULT		100
#define SCS_RATE_RATIO_MIN		0
struct scs_rate_ratio_table {
	int atten_min;
	int atten_max;
	int atten_intvl;
	int entry_num;
	uint8_t rate_ratios[SCS_RATE_RATIO_ENTRY_MAX];	// percent
} scs_rate_ratio_set[SCS_RATE_RATIO_NUM] = {
	/* qtn */
	{
	76,
	104,
	4,
	9,
	{95, 90, 80, 70, 60, 45, 30, 20, 0},
	},
	/* non-qtn, based on test of brcm sta */
	{
	74,
	98,
	6,
	6,
	{95, 90, 80, 50, 30, 0},
	}
};
uint8_t ieee80211_scs_calc_rate_ratio(struct ieee80211com *ic)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	int32_t txpower = ic->ic_curchan->ic_maxpower;
	int32_t attenuation;
	int32_t idx = 0;
	uint8_t rate_ratio;
	int type = ic->ic_nonqtn_sta ? SCS_RATE_RATIO_NONQTN : SCS_RATE_RATIO_QTN;

	/* currently only use worst case */
	if (!SCS_ATTEN_VALID(as->as_sta_atten_max)) {
		return SCS_RATE_RATIO_MIN;
	}
	attenuation = as->as_sta_atten_max;
	SCSDBG(SCSLOG_NOTICE, "txpower=%d, raw attenuation=%d, attenuation adjust=%d, non_qtn_sta=%d\n",
			txpower, attenuation, ic->ic_scs.scs_atten_adjust, ic->ic_nonqtn_sta);
	attenuation += ic->ic_scs.scs_atten_adjust;

	if (attenuation <= scs_rate_ratio_set[type].atten_min) {
		idx = 0;
	} else if (attenuation > scs_rate_ratio_set[type].atten_max) {
		idx = scs_rate_ratio_set[type].entry_num - 1;
	} else {
		idx = (attenuation - scs_rate_ratio_set[type].atten_min +
			scs_rate_ratio_set[type].atten_intvl - 1) / scs_rate_ratio_set[type].atten_intvl;
	}
	rate_ratio = scs_rate_ratio_set[type].rate_ratios[idx];

	SCSDBG(SCSLOG_INFO, "txpower=%d, attenuation=%d, rate ratio = %d\n",
			txpower, attenuation, rate_ratio);

	return rate_ratio;
}

int ieee80211_scs_aging(struct ieee80211com *ic, uint32_t thrshld)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	int i;
	int aged_num = 0;
	uint32_t jiffies_diff = thrshld * 60 * HZ;

	if (thrshld == 0)
		return 0;

	for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
		if (as->as_cca_intf[i] == SCS_CCA_INTF_INVALID)
			continue;

		if ((jiffies - as->as_cca_intf_jiffies[i]) > jiffies_diff) {
			SCSDBG(SCSLOG_NOTICE, "chan %d cca intf aged out(%u - %u > %u)\n", i,
					(unsigned int)jiffies, as->as_cca_intf_jiffies[i],
					jiffies_diff);
			as->as_cca_intf[i] = SCS_CCA_INTF_INVALID;
			as->as_cca_intf_jiffies[i] = 0;
			as->as_pmbl_err_ap[i] = 0;
			as->as_pmbl_err_sta[i] = 0;
			aged_num++;
		}
	}

	return aged_num;
}

static void
ieee80211_scs_get_chan_metric(struct ieee80211com *ic, struct ieee80211_channel *chan,
		uint8_t rate_ratio, int32_t *metric, uint32_t *metric_pref, uint32_t cc_flag)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	int32_t chan_metric;
	int32_t chan_metric_pref = 0;
	int32_t chan_rate_ratio;
	int32_t chan_rate_ratio_cap;
	int32_t txpower;
	int32_t cur_txpower = 0;
	uint32_t traffic_ms;
	uint16_t cca_intf;
	char rndbuf[2];
	uint8_t chan_ieee = chan->ic_ieee;
	int isdfs = !!(chan->ic_flags & IEEE80211_CHAN_DFS);

	traffic_ms = MAX((as->as_tx_ms + as->as_rx_ms), SCS_PICK_CHAN_MIN_SCALED_TRAFFIC);
	if (ic->ic_curchan != IEEE80211_CHAN_ANYC) {
		cur_txpower = ic->ic_curchan->ic_maxpower;
	}
	txpower = chan->ic_maxpower;
	cca_intf = (as->as_cca_intf[chan_ieee] == SCS_CCA_INTF_INVALID) ? 0 : as->as_cca_intf[chan_ieee];

	chan_rate_ratio = 100 - ((100 - rate_ratio) * ABS(txpower - cur_txpower)) / SCS_CHAN_POWER_DIFF_MAX;
	chan_rate_ratio = MAX(chan_rate_ratio, 0);
	if ((IEEE80211_SCS_ATTEN_INC_CC & cc_flag) && txpower < cur_txpower) {
		chan_metric = SCS_MAX_RAW_CHAN_METRIC;
		chan_rate_ratio_cap = 0;
	} else if ((txpower - cur_txpower) < -SCS_CHAN_POWER_DIFF_SAFE) {
		if (chan_rate_ratio > 0) {
			chan_metric = traffic_ms * 100 / chan_rate_ratio + cca_intf;
			chan_rate_ratio_cap = chan_rate_ratio;
		} else {
			chan_metric = SCS_MAX_RAW_CHAN_METRIC;
			chan_rate_ratio_cap = 0;
		}
	} else if ((txpower - cur_txpower) > SCS_CHAN_POWER_DIFF_SAFE) {
		chan_metric = traffic_ms * chan_rate_ratio / 100 + cca_intf;
		if (chan_rate_ratio > 0) {
			chan_rate_ratio_cap = 100 * 100 / chan_rate_ratio;
		} else {
			chan_rate_ratio_cap = SCS_MAX_RATE_RATIO_CAP;
		}
	} else {
		chan_metric = traffic_ms + cca_intf;
		chan_rate_ratio_cap = 100;
	}

	/* Correct channel metric to account for different channel switch margins */
	chan_metric = MAX(0, (chan_metric -
			(isdfs * (ic->ic_scs.scs_leavedfs_chan_mtrc_mrgn - ic->ic_scs.scs_chan_mtrc_mrgn))));

	if (metric) {
		*metric = chan_metric;
	}

	if (metric_pref) {
		int tx_power_factor = DM_DEFAULT_TX_POWER_FACTOR;
		int dfs_factor = DM_DEFAULT_DFS_FACTOR;

		if (ic->ic_dm_factor.flags) {
			if (ic->ic_dm_factor.flags & DM_FLAG_TXPOWER_FACTOR_PRESENT) {
				tx_power_factor = ic->ic_dm_factor.txpower_factor;
			}
			if (ic->ic_dm_factor.flags & DM_FLAG_DFS_FACTOR_PRESENT) {
				dfs_factor = ic->ic_dm_factor.dfs_factor;
			}
		}

		chan_metric_pref = (tx_power_factor * txpower) + (isdfs * dfs_factor);
		/* metric preference: power.random */
		chan_metric_pref = (chan_metric_pref << 16);
		/* Add a little noise to equally choose best ones */
		get_random_bytes(rndbuf, sizeof(rndbuf));
		chan_metric_pref += (rndbuf[0] << 8) | rndbuf[1];

		*metric_pref = chan_metric_pref;
	}

	SCSDBG(SCSLOG_NOTICE, "chan %d: txpower=%d, dfs=%d, radar=%d, rate_ratio_cap=%d, "
				"margin_correction=-%d, metric=%d, pref=0x%x\n",
				chan_ieee, txpower,
				!!(chan->ic_flags & IEEE80211_CHAN_DFS),
				!!(chan->ic_flags & IEEE80211_CHAN_RADAR),
				chan_rate_ratio_cap,
				(isdfs * (ic->ic_scs.scs_leavedfs_chan_mtrc_mrgn - ic->ic_scs.scs_chan_mtrc_mrgn)),
				chan_metric, chan_metric_pref);
}

/*
 * Channel ranking and selection of SCS
 * Called when SCS decided that channel change is required.
 * Return the selected channel number. 0 means no valid better channel.
 */
int
ieee80211_scs_pick_channel(struct ieee80211com *ic, int pick_flags, uint32_t cc_flag)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	uint8_t rate_ratio;
	struct ieee80211_channel *chan;
	struct ieee80211_channel *chan2;
	struct ieee80211_channel *selected_chan = NULL;
	struct ieee80211_channel *fs_bestchan = NULL;
	struct ieee80211_channel *fs_secbestchan = NULL;
	int i;
	int chan_sec_ieee;
	int chan_sec40u_ieee;
	int chan_sec40l_ieee;
	int selected_subchan;
	int best_chan_ieee;
	int chan_metric;
	int best_metric;
	int curr_metric = SCS_MAX_RAW_CHAN_METRIC;
	uint32_t chan_metric_pref;
	uint32_t best_metric_pref;
	int isdfs;
	int is_match_dfs_pickflags;
	int pick_anyway = (pick_flags & IEEE80211_SCS_PICK_ANYWAY);
	int curchan;
	int cur_bw;
	int bestchan_is_curchan = 0;
	int is_curchan;
	int is_weather_chan;
	int bestchan_is_weather_chan = 0;
	struct ieee80211_channel *best_chan = NULL;

	/* Prepare the information we need to do the channel ranking */
	curchan = ic->ic_curchan->ic_ieee;
	cur_bw = ieee80211_get_bw(ic);
	rate_ratio = ieee80211_scs_calc_rate_ratio(ic);
	SCSDBG(SCSLOG_NOTICE, "curchan=%d cur_bw=%d cur_txpower=%d,"
			" rate_ratio=%d, tx_ms=%d, rx_ms=%d\n",
			curchan, cur_bw, ic->ic_curchan->ic_maxpower,
			rate_ratio, as->as_tx_ms, as->as_rx_ms);

	ieee80211_scs_aging(ic, ic->ic_scs.scs_thrshld_aging_nor);

	/* ranking */
	memset(as->as_chanmetric, 0, sizeof(as->as_chanmetric));
	ieee80211_scs_metric_update_timestamps(as);

	best_chan_ieee = SCS_BEST_CHAN_INVALID;
	best_metric = 0;
	best_metric_pref = 0;

	for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
		chan = ieee80211_find_channel_by_ieee(ic, i);
		if (chan == NULL) {
			continue;
		}

		isdfs = (chan->ic_flags & IEEE80211_CHAN_DFS);
		is_match_dfs_pickflags = (isdfs && (pick_flags & IEEE80211_SCS_PICK_DFS_ONLY))
			|| (isdfs && (pick_flags & IEEE80211_SCS_PICK_AVAILABLE_DFS_ONLY))
			|| (!isdfs && (pick_flags & IEEE80211_SCS_PICK_NON_DFS_ONLY))
			|| !(pick_flags & (IEEE80211_SCS_PICK_DFS_ONLY | IEEE80211_SCS_PICK_NON_DFS_ONLY |
							IEEE80211_SCS_PICK_AVAILABLE_DFS_ONLY));

		ieee80211_scs_get_chan_metric(ic, chan, rate_ratio,
				&as->as_chanmetric[i], &as->as_chanmetric_pref[i], cc_flag);
		/*
		 * (i) When scs_pick_channel is being called from scs comparison task, scs manual,
		 * we should choose from channels which are available
		 * (ii) When scs_pick_channel is being called from OCAC task
		 * we should choose from DFS channels which are not available, cac not done, and radar not detected.
		 */

		/* This is called only from SCS Manual and SCS comparison task context */
		if ((pick_flags & IEEE80211_SCS_PICK_AVAILABLE_ANY_CHANNEL)
			&& (!ieee80211_is_chan_available(chan))) {
			SCSDBG(SCSLOG_INFO, "chan %d skipped as not available\n", chan->ic_ieee);
			continue;
		}

		/* OCAC needs a DFS channel for which CAC is not already done */
		if (pick_flags & IEEE80211_SCS_PICK_NOT_AVAILABLE_DFS_ONLY) {
			if ((chan) && (ic->ic_dfs_chans_available_for_cac(ic, chan) == false))
			{
				SCSDBG(SCSLOG_INFO, "chan %d skipped (flag: %d)\n", chan->ic_ieee,
						ic->ic_chan_availability_status[chan->ic_ieee]);
				continue;
			}
		}

		if (cur_bw == BW_HT20) {
			chan_sec_ieee = -1;
			chan_sec40u_ieee = -1;
			chan_sec40l_ieee = -1;

			chan_metric = as->as_chanmetric[i];
			chan_metric_pref = as->as_chanmetric_pref[i];
			selected_subchan = i;
		} else if (cur_bw == BW_HT40) {
			/* only calculate channel pair when low number 20M channel is already calculated */
			if (!(chan->ic_flags & IEEE80211_CHAN_HT40D)) {
				continue;
			}
			chan_sec_ieee = i - IEEE80211_CHAN_SEC_SHIFT;
			chan_sec40u_ieee = -1;
			chan_sec40l_ieee = -1;

			/* select worse channel as primary channel within the chan pair */
			if ((as->as_chanmetric[i] < as->as_chanmetric[chan_sec_ieee]) ||
					((as->as_chanmetric[i] == as->as_chanmetric[chan_sec_ieee]) &&
					as->as_chanmetric_pref[i] > as->as_chanmetric_pref[chan_sec_ieee])) {
				selected_subchan = chan_sec_ieee;
			} else {
				selected_subchan = i;
			}

			chan_metric = as->as_chanmetric[selected_subchan];
			chan_metric_pref = as->as_chanmetric_pref[selected_subchan];
		} else if (cur_bw >= BW_HT80) {
			/* only calculate channel set when all 20M channels are already calculated */
			if (!(chan->ic_ext_flags & IEEE80211_CHAN_VHT80_UU)) {
				continue;
			}
			chan_sec_ieee = i - IEEE80211_CHAN_SEC_SHIFT;
			chan_sec40u_ieee = i - 2 * IEEE80211_CHAN_SEC_SHIFT;
			chan_sec40l_ieee = i - 3 * IEEE80211_CHAN_SEC_SHIFT;

			/* select worst channel as primary channel within the best chan set */
			if ((as->as_chanmetric[i] < as->as_chanmetric[chan_sec_ieee]) ||
					((as->as_chanmetric[i] == as->as_chanmetric[chan_sec_ieee]) &&
					as->as_chanmetric_pref[i] > as->as_chanmetric_pref[chan_sec_ieee])) {
				selected_subchan = chan_sec_ieee;
			} else {
				selected_subchan = i;
			}
			if ((as->as_chanmetric[selected_subchan] < as->as_chanmetric[chan_sec40u_ieee]) ||
					((as->as_chanmetric[selected_subchan] == as->as_chanmetric[chan_sec40u_ieee]) &&
					as->as_chanmetric_pref[selected_subchan] > as->as_chanmetric_pref[chan_sec40u_ieee])) {
				selected_subchan = chan_sec40u_ieee;
			}
			if ((as->as_chanmetric[selected_subchan] < as->as_chanmetric[chan_sec40l_ieee]) ||
					((as->as_chanmetric[selected_subchan] == as->as_chanmetric[chan_sec40l_ieee]) &&
					as->as_chanmetric_pref[selected_subchan] > as->as_chanmetric_pref[chan_sec40l_ieee])) {
				selected_subchan = chan_sec40l_ieee;
			}

			chan_metric = as->as_chanmetric[selected_subchan];
			chan_metric_pref = as->as_chanmetric_pref[selected_subchan];
		} else {
			printk("SCS: unknown bandwidth: %u\n", cur_bw);
			continue;
		}

		/* Need to calculate DFS channel metric in case current channel is DFS channel */
		if ((curchan == i) ||
				(curchan == chan_sec_ieee) ||
				(curchan == chan_sec40u_ieee) ||
				(curchan == chan_sec40l_ieee)) {
			is_curchan = 1;
			curr_metric = chan_metric;
			if (pick_anyway) {
				continue;
			}
		} else {
			is_curchan = 0;
		}

		if ((!(pick_flags & IEEE80211_SCS_PICK_AVAILABLE_ANY_CHANNEL)) &&
				(!(pick_flags & IEEE80211_SCS_PICK_NOT_AVAILABLE_DFS_ONLY))) {
			if (!is_match_dfs_pickflags) {
				continue;
			} else if (pick_flags & IEEE80211_SCS_PICK_AVAILABLE_DFS_ONLY) {
				if (!ieee80211_is_chan_available(chan)) {
					SCSDBG(SCSLOG_INFO, "chan %d skipped as not available\n", chan->ic_ieee);
					continue;
				}
			}
		}

		if (isset(ic->ic_chan_pri_inactive, i) &&
				((chan_sec_ieee == -1) || isset(ic->ic_chan_pri_inactive, chan_sec_ieee)) &&
				((chan_sec40u_ieee == -1) || isset(ic->ic_chan_pri_inactive, chan_sec40u_ieee)) &&
				((chan_sec40l_ieee == -1) || isset(ic->ic_chan_pri_inactive, chan_sec40l_ieee))) {
			/* All the sub-channel can't be primary channel */
			continue;
		}

		is_weather_chan = ieee80211_is_on_weather_channel(ic, chan);

		/* radar detected on this channel, secondary channel or secondary 40MHz channels*/
		if (chan->ic_flags & IEEE80211_CHAN_DFS) {
			if (chan->ic_flags & IEEE80211_CHAN_RADAR)
				continue;

			if (cur_bw >= BW_HT40) {
				chan2 = ieee80211_find_channel_by_ieee(ic, chan_sec_ieee);
				if (chan2 && (chan2->ic_flags & IEEE80211_CHAN_RADAR))
					continue;

				if (cur_bw >= BW_HT80) {
					chan2 = ieee80211_find_channel_by_ieee(ic, chan_sec40u_ieee);
					if (chan2 && (chan2->ic_flags & IEEE80211_CHAN_RADAR))
						continue;

					chan2 = ieee80211_find_channel_by_ieee(ic, chan_sec40l_ieee);
					if (chan2 && (chan2->ic_flags & IEEE80211_CHAN_RADAR))
						continue;
				}
			}
		}

		if (best_chan_ieee != SCS_BEST_CHAN_INVALID &&
				!bestchan_is_weather_chan &&
				is_weather_chan &&
				!is_curchan &&
				(pick_flags & IEEE80211_SCS_PICK_NOT_AVAILABLE_DFS_ONLY) &&
				(ic->ic_ocac.ocac_cfg.ocac_params.wea_duration_secs >
					ic->ic_ocac.ocac_cfg.ocac_params.duration_secs)) {
			/* Weather channel has low priority since it need too long CAC time */
			continue;
		}

		/* Select best channel */
		if (best_chan_ieee == SCS_BEST_CHAN_INVALID ||
			chan_metric < best_metric ||
			((chan_metric == best_metric)
				&& (chan_metric_pref > best_metric_pref))) {
			best_chan_ieee = selected_subchan;
			best_metric = chan_metric;
			best_metric_pref = chan_metric_pref;
			bestchan_is_curchan = is_curchan;
			bestchan_is_weather_chan = is_weather_chan;
		}

		/* Update best alternate channel information for fast switch */
		selected_chan = ieee80211_find_channel_by_ieee(ic, selected_subchan);
		if (!is_curchan && selected_chan && ieee80211_is_chan_available(selected_chan)) {
			if ((fs_bestchan == NULL) || (!ieee80211_is_chan_available(fs_bestchan))
				|| (chan_metric < as->as_chanmetric[fs_bestchan->ic_ieee])) {
				if (fs_bestchan && ieee80211_is_chan_available(fs_bestchan)
					&& (ic->ic_chan_compare_equality(ic, fs_bestchan, selected_chan) == false)) {
					fs_secbestchan = fs_bestchan;
				}
				fs_bestchan = selected_chan;
			}
			else if (fs_bestchan && (ic->ic_chan_compare_equality(ic, fs_bestchan, selected_chan) == false)) {
				if ((fs_secbestchan == NULL) || (!ieee80211_is_chan_available(fs_secbestchan))
					|| (chan_metric < as->as_chanmetric[fs_secbestchan->ic_ieee])) {
					fs_secbestchan = selected_chan;
				}
			}
		}
	}

	if (fs_secbestchan) {
		ic->ic_ieee_best_alt_chan = fs_secbestchan->ic_ieee;
	}

	if (!pick_anyway && best_chan_ieee != SCS_BEST_CHAN_INVALID) {
		if (bestchan_is_curchan) {
			best_chan_ieee = SCS_BEST_CHAN_INVALID;
			SCSDBG(SCSLOG_NOTICE, "current chan is best channel\n");
		} else {
			best_chan = ieee80211_find_channel_by_ieee(ic, best_chan_ieee);
			bool dfs_to_non_dfs = false;

			if (curr_metric == SCS_MAX_RAW_CHAN_METRIC) {
				/* If the IEEE80211_CHAN_HT40D channel in 40MHz or IEEE80211_CHAN_VHT80_UU
				 * channel in 80MHz is disabled, curr_metric may be SCS_MAX_RAW_CHAN_METRIC
				 * per above metric calculation, so need to read it from as_chanmetric table.
				 */
				curr_metric = as->as_chanmetric[curchan];
			}

			if (best_chan) {
				dfs_to_non_dfs = (ic->ic_curchan->ic_flags & IEEE80211_CHAN_DFS) &&
						 (!(best_chan->ic_flags & IEEE80211_CHAN_DFS));

				if (best_metric == SCS_MAX_RAW_CHAN_METRIC ||
						((curr_metric - best_metric) * 100) < ((dfs_to_non_dfs ?
								(ic->ic_scs.scs_leavedfs_chan_mtrc_mrgn) :
								(ic->ic_scs.scs_chan_mtrc_mrgn))  * IEEE80211_SCS_CCA_INTF_SCALE)) {
					/* Avoid unnecessary channel change */
					SCSDBG(SCSLOG_NOTICE, "best chan %u is not better enough (%u - %u) < %u%%\n",
							best_chan_ieee, curr_metric, best_metric,
							dfs_to_non_dfs ? ic->ic_scs.scs_leavedfs_chan_mtrc_mrgn : ic->ic_scs.scs_chan_mtrc_mrgn);

					best_chan_ieee = SCS_BEST_CHAN_INVALID;
				}
			} else {
				best_chan_ieee = SCS_BEST_CHAN_INVALID;
			}
		}
	}
	as->as_scs_ranking_cnt++;

	if (best_chan_ieee != SCS_BEST_CHAN_INVALID) {
		struct ieee80211_channel *best_chan = ieee80211_find_channel_by_ieee(ic, best_chan_ieee);
		if (best_chan) {
			best_chan = ieee80211_chk_update_pri_chan(ic, best_chan, 1, "SCS", 0);
			if (best_chan) {
				best_chan_ieee = best_chan->ic_ieee;
			}
		}
		SCSDBG(SCSLOG_NOTICE, "chan %d selected as best chan\n", best_chan_ieee);
	} else {
		ic->ic_ieee_best_alt_chan = best_chan ? best_chan->ic_ieee :
			(fs_bestchan ? fs_bestchan->ic_ieee : ic->ic_ieee_best_alt_chan);
	}
	SCSDBG(SCSLOG_NOTICE, "%s: Fast-switch best alt channel updated to %d\n",
		__func__, ic->ic_ieee_best_alt_chan);

	return best_chan_ieee;
}

void ieee80211_scs_contribute_randomness(uint32_t cca_intf, uint32_t lpre_err, uint32_t spre_err)
{
	unsigned random_buf[] = {cca_intf, lpre_err, spre_err};

	add_qtn_randomness(random_buf, ARRAY_SIZE(random_buf));
}

static int ieee80211_scs_get_scaled_scan_info(struct ieee80211com *ic, int chan_ieee,
		struct qtn_scs_scan_info *p_scan_info)
{
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_scs_info_set *scs_info_lh = sp->scs_info_lhost;
	int ret = -1;

	p_scan_info->cca_try = 0;
	if (chan_ieee < IEEE80211_CHAN_MAX) {
		memcpy(p_scan_info, &scs_info_lh->scan_info[chan_ieee], sizeof(struct qtn_scs_scan_info));
	}

	if (p_scan_info->cca_try) {
		ieee80211_scs_contribute_randomness(p_scan_info->cca_intf, p_scan_info->lpre_err,
				p_scan_info->spre_err);
		p_scan_info->cca_intf = IEEE80211_SCS_NORMALIZE(p_scan_info->cca_intf, p_scan_info->cca_try);
		p_scan_info->cca_busy = IEEE80211_SCS_NORMALIZE(p_scan_info->cca_busy, p_scan_info->cca_try);
		p_scan_info->cca_idle = IEEE80211_SCS_NORMALIZE(p_scan_info->cca_idle, p_scan_info->cca_try);
		p_scan_info->cca_tx = IEEE80211_SCS_NORMALIZE(p_scan_info->cca_tx, p_scan_info->cca_try);
		p_scan_info->bcn_rcvd = IEEE80211_SCS_NORMALIZE(p_scan_info->bcn_rcvd, p_scan_info->cca_try);
		p_scan_info->crc_err = IEEE80211_SCS_NORMALIZE(p_scan_info->crc_err, p_scan_info->cca_try);
		p_scan_info->spre_err = IEEE80211_SCS_NORMALIZE(p_scan_info->spre_err, p_scan_info->cca_try);
		p_scan_info->lpre_err = IEEE80211_SCS_NORMALIZE(p_scan_info->lpre_err, p_scan_info->cca_try);
		p_scan_info->cca_try = IEEE80211_SCS_CCA_INTF_SCALE;
		ret = 0;
	}

	return ret;
}

void ieee80211_scs_update_ranking_table_by_scan(struct ieee80211com *ic)
{
	struct qtn_scs_scan_info scan_info;
	struct ap_state *as, *scs_as;
	struct ieee80211_channel *chan;
	int chansec_ieee;
	uint32_t pmbl_err;
	int i, ret;
	uint32_t update_mode = IEEE80211_SCS_OFFCHAN;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		return;
	}

	/* if we didn't return to bss channel then stats for the last channel were not updated */
	if (ic->ic_bsschan == IEEE80211_CHAN_ANYC) {
		ic->ic_scs_update_scan_stats(ic);
		update_mode = IEEE80211_SCS_INIT_SCAN;
	}

	as = ic->ic_scan->ss_priv;
	scs_as = ic->ic_scan->ss_scs_priv;
	for (i = 0; i < IEEE80211_CHAN_MAX; i++) {
		chan = ieee80211_find_channel_by_ieee(ic, i);
		if (chan == NULL) {
			continue;
		}
		ret = ieee80211_scs_get_scaled_scan_info(ic, i, &scan_info);
		if (ret != 0) {
			/* use secondary channel's scan info */
			chansec_ieee = ieee80211_find_sec_chan(chan);
			if (chansec_ieee) {
				ret = ieee80211_scs_get_scaled_scan_info(ic, chansec_ieee, &scan_info);
			}
			SCSDBG(SCSLOG_INFO, "Didn't find scan_info of channel %u, use the scan_info of channel %u\n",
					i, chansec_ieee);
		}
		if (ret == 0) {
			pmbl_err = (scan_info.spre_err * ic->ic_scs.scs_sp_wf +
					scan_info.lpre_err * ic->ic_scs.scs_lp_wf) / 100;
			/* update initial channel ranking table */
			if (as) {
				as->as_pmbl_err_ap[i] = pmbl_err;
				as->as_cca_intf[i] = scan_info.cca_intf;
				as->as_cca_intf_jiffies[i] = jiffies;
			}

			/* update SCS channel ranking table */
			if (scs_as) {
				uint32_t compound_cca_intf = scan_info.cca_intf;

				/*
				 * don't add preamble failure counts to compound_cca_intf because
				 * the preamble failure counts got by scanning are not reliable
				 */
				/*
				compound_cca_intf = ieee80211_scs_fix_cca_intf(ic, NULL, scan_info.cca_intf,
						scan_info.spre_err, scan_info.lpre_err);
				*/
				ieee80211_scs_update_chans_cca_intf(ic, chan, scan_info.bw_sel,
						update_mode, compound_cca_intf,
						IEEE80211_SCS_CCA_INTF_SCALE,
						NULL, pmbl_err);
			}
		} else {
			SCSDBG(SCSLOG_INFO, "No available scan_info for channel %u\n", i);
			/* clear initial channel ranking table */
			if (as) {
				as->as_pmbl_err_ap[i] = 0;
				as->as_cca_intf[i] = SCS_CCA_INTF_INVALID;
				as->as_cca_intf_jiffies[i] = jiffies;
			}
		}
	}
}
EXPORT_SYMBOL(ieee80211_scs_update_ranking_table_by_scan);

void ieee80211_scs_scale_cochan_data(struct ieee80211com *ic, struct qtn_scs_info *scs_info_read)
{
	int i;
	struct qtn_scs_vsp_node_stats *stats;

	if (scs_info_read->cca_try == 0)
		return;

	scs_info_read->cca_idle = IEEE80211_SCS_NORMALIZE(scs_info_read->cca_idle, scs_info_read->cca_try);
	scs_info_read->cca_busy = IEEE80211_SCS_NORMALIZE(scs_info_read->cca_busy, scs_info_read->cca_try);
	scs_info_read->cca_interference = IEEE80211_SCS_NORMALIZE(scs_info_read->cca_interference, scs_info_read->cca_try);
	scs_info_read->cca_tx = IEEE80211_SCS_NORMALIZE(scs_info_read->cca_tx, scs_info_read->cca_try);
	scs_info_read->tx_usecs = IEEE80211_SCS_NORMALIZE(scs_info_read->tx_usecs, scs_info_read->cca_try);
	scs_info_read->rx_usecs = IEEE80211_SCS_NORMALIZE(scs_info_read->rx_usecs, scs_info_read->cca_try);
	scs_info_read->beacon_recvd = IEEE80211_SCS_NORMALIZE(scs_info_read->beacon_recvd, scs_info_read->cca_try);

	for (i = 0; i < scs_info_read->scs_vsp_info.num_of_assoc; i++) {
		stats = &scs_info_read->scs_vsp_info.scs_vsp_node_stats[i];
		stats->tx_usecs = IEEE80211_SCS_NORMALIZE(stats->tx_usecs, scs_info_read->cca_try);
		stats->rx_usecs = IEEE80211_SCS_NORMALIZE(stats->rx_usecs, scs_info_read->cca_try);
	}

	scs_info_read->cca_try = IEEE80211_SCS_CCA_INTF_SCALE;
}

void ieee80211_scs_scale_offchan_data(struct ieee80211com *ic, struct qtn_scs_oc_info *scs_oc_info)
{
	if (scs_oc_info->off_chan_cca_try_cnt == 0)
		return;

	scs_oc_info->off_chan_cca_busy = IEEE80211_SCS_NORMALIZE(scs_oc_info->off_chan_cca_busy,
			scs_oc_info->off_chan_cca_try_cnt);
	scs_oc_info->off_chan_cca_sample_cnt = IEEE80211_SCS_NORMALIZE(scs_oc_info->off_chan_cca_sample_cnt,
			scs_oc_info->off_chan_cca_try_cnt);
	scs_oc_info->off_chan_beacon_recvd = IEEE80211_SCS_NORMALIZE(scs_oc_info->off_chan_beacon_recvd,
			scs_oc_info->off_chan_cca_try_cnt);
	scs_oc_info->off_chan_crc_errs = IEEE80211_SCS_NORMALIZE(scs_oc_info->off_chan_crc_errs,
			scs_oc_info->off_chan_cca_try_cnt);
	scs_oc_info->off_chan_sp_errs = IEEE80211_SCS_NORMALIZE(scs_oc_info->off_chan_sp_errs,
			scs_oc_info->off_chan_cca_try_cnt);
	scs_oc_info->off_chan_lp_errs = IEEE80211_SCS_NORMALIZE(scs_oc_info->off_chan_lp_errs,
			scs_oc_info->off_chan_cca_try_cnt);

	scs_oc_info->off_chan_cca_try_cnt = IEEE80211_SCS_CCA_INTF_SCALE;
}

static struct qtn_scs_vsp_node_stats *ieee80211_scs_find_node_stats(struct ieee80211com *ic, struct qtn_scs_info *scs_info_read, uint16_t aid)
{
	int i;
	struct qtn_scs_vsp_node_stats *stats;

	for (i = 0; i < scs_info_read->scs_vsp_info.num_of_assoc; i++) {
		stats = &scs_info_read->scs_vsp_info.scs_vsp_node_stats[i];
		if (stats->ni_associd == IEEE80211_AID(aid))
			return stats;
	}

	return NULL;
}

uint32_t local_scs_median_parition(uint32_t *buf, uint32_t l, uint32_t r)
{
	uint32_t t;
	uint32_t i;
	uint32_t p;

	if (l == r)
		return l;

	for (i = l, p = l; i < r; i++) {
		if (buf[i] < buf[r]) {
			t = buf[i];
			buf[i] = buf[p];
			buf[p] = t;
			p++;
		}
	}

	t = buf[r];
	buf[r] = buf[p];
	buf[p] = t;

	return p;
}

static uint32_t local_scs_get_median(uint32_t *buf, uint32_t l, uint32_t r)
{
	uint32_t idx;

	idx = local_scs_median_parition(buf, l, r);

	if (idx == QTN_SCS_FILTER_MEDIAN_IDX)
		return buf[idx];

	if (idx > QTN_SCS_FILTER_MEDIAN_IDX)
		return local_scs_get_median(buf, l, idx - 1);
	else
		return local_scs_get_median(buf, idx + 1, r);
}

static uint32_t ieee80211_scs_get_median(struct ieee80211com *ic,
			struct qtn_scs_data_history *history)
{
	static uint32_t buf[QTN_SCS_FILTER_WINDOW_SZ];

	memcpy(buf, history->buffer, sizeof(history->buffer));

	return local_scs_get_median(buf, 0, QTN_SCS_FILTER_WINDOW_SZ - 1);
}

static int32_t ieee80211_scs_get_channel_index(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	uint32_t i;

	for (i = 0; i < ic->ic_nchans; i++) {
		if (ic->ic_channels[i].ic_ieee == chan->ic_ieee)
			break;
	}

	if (i == ic->ic_nchans)
		return -1;

	return i;
}

static void ieee80211_scs_update_cochan_filter_history(struct ieee80211com *ic,
			struct ieee80211_phy_stats  *p_phy_stats,
			struct qtn_scs_stats_history *history,
			int32_t idx)
{
	if (idx < 0)
		return;

	history->sp_errs[idx].buffer[history->sp_errs[idx].idx++] = p_phy_stats->cnt_sp_fail;
	if (history->sp_errs[idx].idx == QTN_SCS_FILTER_WINDOW_SZ)
		history->sp_errs[idx].idx = 0;

	history->lp_errs[idx].buffer[history->lp_errs[idx].idx++] = p_phy_stats->cnt_lp_fail;
	if (history->lp_errs[idx].idx == QTN_SCS_FILTER_WINDOW_SZ)
		history->lp_errs[idx].idx = 0;
}

static uint32_t ieee80211_scs_fix_cca_intf(struct ieee80211com *ic, struct ieee80211_node *ni, uint32_t cca_intf, uint32_t sp_fail, uint32_t lp_fail)
{
	uint32_t pmbl_fail;
	int pmbl_level;
	int mapped_intf_max;
	uint32_t compound_cca_intf;

	pmbl_fail = (sp_fail * ic->ic_scs.scs_sp_wf + lp_fail * ic->ic_scs.scs_lp_wf) / 100;
	pmbl_level = pmbl_fail * 100 / ic->ic_scs.scs_pmbl_err_range;
	pmbl_level = MIN(pmbl_level, 100);
	mapped_intf_max = ic->ic_scs.scs_pmbl_err_mapped_intf_range * IEEE80211_SCS_CCA_INTF_SCALE / 100;
	compound_cca_intf = cca_intf + pmbl_level * mapped_intf_max / 100;
	compound_cca_intf = MIN(compound_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE);
	SCSDBG(SCSLOG_INFO, "node 0x%x: node_cca_intf=%u, pmbl_smthed_weighted=%u, compound_cca_intf=%u\n",
			(ni ? ni->ni_associd : 0),
			cca_intf, pmbl_fail, compound_cca_intf);

	return compound_cca_intf;
}

static int
ieee80211_scs_change_channel(struct ieee80211com *ic, int newchan_ieee)
{
	struct ieee80211_channel *newchan;
	struct ieee80211_channel *curchan;
	int chan2_ieee;
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	int ret = -1;
	int cur_bw;

	newchan = ieee80211_find_channel_by_ieee(ic, newchan_ieee);
	if (newchan == NULL) {
		return ret;
	}

	curchan = ic->ic_curchan;
	ret = ieee80211_enter_csa(ic, newchan, ieee80211_scs_trigger_channel_switch,
			ic->ic_csw_reason,
			IEEE80211_DEFAULT_CHANCHANGE_TBTT_COUNT,
			IEEE80211_CSA_MUST_STOP_TX,
			IEEE80211_CSA_F_BEACON | IEEE80211_CSA_F_ACTION);
	if (ret == 0) {
		ic->ic_aci_cci_cce.cce_previous = curchan->ic_ieee;
		ic->ic_aci_cci_cce.cce_current = newchan->ic_ieee;

		setbit(as->as_chan_xped, curchan->ic_ieee);
		cur_bw = ieee80211_get_bw(ic);
		if (cur_bw >= BW_HT40) {
			chan2_ieee = ieee80211_find_sec_chan(curchan);
			if (chan2_ieee)
				setbit(as->as_chan_xped, chan2_ieee);
			if (cur_bw >= BW_HT80) {
				chan2_ieee = ieee80211_find_sec40u_chan(curchan);
				if (chan2_ieee)
					setbit(as->as_chan_xped, chan2_ieee);
				chan2_ieee = ieee80211_find_sec40l_chan(curchan);
				if (chan2_ieee)
					setbit(as->as_chan_xped, chan2_ieee);
			}
		}
	}

	return ret;
}

static void
ieee80211_scs_update_dfs_reentry(struct ieee80211com *ic, uint32_t cc_flag, uint32_t *dfs_reentry_clear)
{
	int curr_chan_cca_intf;
	int aged_num;
	int curchan = ic->ic_curchan->ic_ieee;
	struct ap_state *as = ic->ic_scan->ss_scs_priv;

	if (!ic->ic_scs.scs_thrshld_dfs_reentry)
		return;

	curr_chan_cca_intf = (as->as_cca_intf[curchan] == SCS_CCA_INTF_INVALID) ? 0 : as->as_cca_intf[curchan];

	/* When request is because of interference */
	if ((cc_flag & IEEE80211_SCS_INTF_CC) &&
		((curr_chan_cca_intf * 100) >
		 (ic->ic_scs.scs_thrshld_dfs_reentry_intf * IEEE80211_SCS_CCA_INTF_SCALE))) {
		as->as_dfs_reentry_cnt++;
		SCSDBG(SCSLOG_NOTICE, "channel picking discard counter %d\n",
					as->as_dfs_reentry_cnt);
		*dfs_reentry_clear = 0;
		if ((as->as_dfs_reentry_cnt * ic->ic_scs.scs_cca_sample_dur) >=
			ic->ic_scs.scs_thrshld_dfs_reentry) {
			aged_num = ieee80211_scs_aging(ic, ic->ic_scs.scs_thrshld_aging_dfsreent);
			if (aged_num > 0) {
				/* don't clear dfs reentry and wait for next interval result */
				SCSDBG(SCSLOG_NOTICE, "%u channel entry aged out, "
					"postpone DFS re-entry and re-try other channel\n",
					aged_num);
			} else {
				as->as_dfs_reentry_level = 1;
				SCSDBG(SCSLOG_NOTICE, "immediately DFS re-entry triggered with "
						"channel picking counter %d\n",
						as->as_dfs_reentry_cnt);
			}
		}
	}
}

static void
ieee80211_scs_start_compare(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *) arg;
	struct ieee80211vap *vap;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	int new_chan;
	struct qtn_scs_info_set *scs_info_lh = sp->scs_info_lhost;
	struct qtn_scs_info scs_info_read;
	uint32_t cc_flag;
	uint32_t pmbl_err;
	uint32_t dfs_reentry_clear = 1;
	uint32_t compound_cca_intf, raw_cca_intf;
	uint32_t clean_level = IEEE80211_SCS_STATE_PERIOD_CLEAN;
	struct ieee80211_phy_stats phy_stats;
	struct ap_state *as = ic->ic_scan->ss_scs_priv;
	uint8_t *tdls_stats_buf = NULL;
	uint16_t tdls_stats_buf_len = IEEE80211_MAX_TDLS_NODES *
			sizeof(struct ieee80211_tdls_scs_stats);
	uint16_t extra_ie_len = 0;
	int tdls_update_failed = 0;
	uint32_t stats_unstable = 0;

	if (ic->ic_get_phy_stats
			&& !ic->ic_get_phy_stats(ic->ic_dev, ic, &phy_stats, 0)) {
		uint32_t sp_errs;
		uint32_t lp_errs;
		int32_t idx = ieee80211_scs_get_channel_index(ic, ic->ic_curchan);
		if (idx < 0)
			goto compare_end;
		ieee80211_scs_update_cochan_filter_history(ic, &phy_stats,
					&scs_info_lh->stats_history, idx);
		sp_errs = ieee80211_scs_get_median(ic, &scs_info_lh->stats_history.sp_errs[idx]);
		lp_errs = ieee80211_scs_get_median(ic, &scs_info_lh->stats_history.lp_errs[idx]);
		ic->ic_scs.scs_sp_err_smthed = IEEE80211_SCS_SMOOTH(ic->ic_scs.scs_sp_err_smthed,
				sp_errs, ic->ic_scs.scs_pmbl_err_smth_fctr);
		ic->ic_scs.scs_lp_err_smthed = IEEE80211_SCS_SMOOTH(ic->ic_scs.scs_lp_err_smthed,
				lp_errs, ic->ic_scs.scs_pmbl_err_smth_fctr);
		ieee80211_scs_contribute_randomness(phy_stats.cca_int, phy_stats.cnt_lp_fail,
			phy_stats.cnt_sp_fail);
	}

	/* Copy scs info into a local structure so MuC can continue fill it */
	memcpy((void *)&scs_info_read, (void *)&scs_info_lh->scs_info[scs_info_lh->valid_index],
			sizeof(struct qtn_scs_info));
	if (scs_info_read.cca_try == 0) {
		/* make sure cross channel switching stats are cleared */
		clean_level = IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN;
		goto compare_end;
	}

	/* Read and process off channel stats */
	if (scs_info_read.oc_info_count) {
		int i;
		uint32_t pmbl;
		struct qtn_scs_oc_info * p_oc_info;
		struct ieee80211_channel *off_chan;

		for (i = 0; i < scs_info_read.oc_info_count; i++) {
			p_oc_info = &scs_info_read.oc_info[i];
			if (p_oc_info->off_chan_cca_try_cnt) {
				SCSDBG(SCSLOG_INFO, "OC: Channel=%d, bw=%d cca_busy=%d, "
						"cca_smpl=%d, cca_try=%d, bcn_rcvd=%d, "
						"crc_err=%d, sp_err=%d, lp_err=%d\n",
						p_oc_info->off_channel,
						p_oc_info->off_chan_bw_sel,
						p_oc_info->off_chan_cca_busy,
						p_oc_info->off_chan_cca_sample_cnt,
						p_oc_info->off_chan_cca_try_cnt,
						p_oc_info->off_chan_beacon_recvd,
						p_oc_info->off_chan_crc_errs,
						p_oc_info->off_chan_sp_errs,
						p_oc_info->off_chan_lp_errs);
				off_chan = ieee80211_find_channel_by_ieee(ic, p_oc_info->off_channel);
				if (off_chan == NULL) {
					continue;
				}
				ieee80211_scs_scale_offchan_data(ic, p_oc_info);
				raw_cca_intf = p_oc_info->off_chan_cca_busy;
				compound_cca_intf = ieee80211_scs_fix_cca_intf(ic, NULL, raw_cca_intf,
						p_oc_info->off_chan_sp_errs, p_oc_info->off_chan_lp_errs);
				pmbl = (p_oc_info->off_chan_sp_errs * ic->ic_scs.scs_sp_wf +
						p_oc_info->off_chan_lp_errs * ic->ic_scs.scs_lp_wf) / 100;
				ieee80211_scs_update_chans_cca_intf(ic, off_chan,
						p_oc_info->off_chan_bw_sel, IEEE80211_SCS_OFFCHAN,
						compound_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE,
						NULL, pmbl);
			}
		}
	}

	/* Scale co-channel data */
	ieee80211_scs_scale_cochan_data(ic, &scs_info_read);

	SCSDBG(SCSLOG_INFO, "cca_try=%u, cca_idle=%u cca_busy=%u cca_intf=%u cca_tx=%u tx_ms=%u rx_ms=%u\n",
			scs_info_read.cca_try,
			scs_info_read.cca_idle,
			scs_info_read.cca_busy,
			scs_info_read.cca_interference,
			scs_info_read.cca_tx,
			scs_info_read.tx_usecs / 1000,
			scs_info_read.rx_usecs / 1000);

	if (ic->ic_scs.scs_debug_enable >= SCSLOG_VERBOSE) {
		int node_index;

		for (node_index = 0; node_index < scs_info_read.scs_vsp_info.num_of_assoc; node_index++) {
			printk("SCS: AssocID = 0x%04X, tx_time = %u, rx_time =%u\n",
				scs_info_read.scs_vsp_info.scs_vsp_node_stats[node_index].ni_associd,
				scs_info_read.scs_vsp_info.scs_vsp_node_stats[node_index].tx_usecs / 1000,
				scs_info_read.scs_vsp_info.scs_vsp_node_stats[node_index].rx_usecs / 1000);
		}
	}

	raw_cca_intf = scs_info_read.cca_interference;
	if ((ic->ic_opmode == IEEE80211_M_HOSTAP) && ieee80211_scs_tdls_link_is_existing(ic)) {
		tdls_update_failed = ieee80211_scs_update_tdls_link_time(ic);
		compound_cca_intf = ieee80211_scs_smooth_ap_cca_intf_time(ic, raw_cca_intf,
					&stats_unstable);
	} else {
		compound_cca_intf = ieee80211_scs_fix_cca_intf(ic, NULL, raw_cca_intf,
				ic->ic_scs.scs_sp_err_smthed, ic->ic_scs.scs_lp_err_smthed);
	}
	pmbl_err = (ic->ic_scs.scs_sp_err_smthed * ic->ic_scs.scs_sp_wf +
			ic->ic_scs.scs_lp_err_smthed * ic->ic_scs.scs_lp_wf) / 100;

	SCSDBG(SCSLOG_INFO, "current pmbl error = %u %u, smoothed = %u %u,"
			" raw_cca_intf = %u, comp_cca_intf = %u\n",
			phy_stats.cnt_sp_fail, phy_stats.cnt_lp_fail,
			ic->ic_scs.scs_sp_err_smthed, ic->ic_scs.scs_lp_err_smthed,
			raw_cca_intf, compound_cca_intf);

	/* update smoothed free airtime */
	if (ic->ic_scs.scs_cca_idle_smthed) {
		ic->ic_scs.scs_cca_idle_smthed = IEEE80211_SCS_SMOOTH(ic->ic_scs.scs_cca_idle_smthed,
				scs_info_read.cca_idle, ic->ic_scs.scs_cca_idle_smth_fctr);
	} else {
		ic->ic_scs.scs_cca_idle_smthed = scs_info_read.cca_idle;
	}
	SCSDBG(SCSLOG_INFO, "cca_idle_smthed %u\n", ic->ic_scs.scs_cca_idle_smthed);

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		ieee80211_scs_collect_node_atten(ic);
		int cur_bw = ieee80211_get_bw(ic);

		/* clear current cca intf at first */
		ieee80211_scs_update_chans_cca_intf(ic, ic->ic_curchan, cur_bw, IEEE80211_SCS_COCHAN,
				SCS_CCA_INTF_INVALID, IEEE80211_SCS_CCA_INTF_SCALE, NULL, 0);
		/* update cca intf stats */
		ieee80211_scs_update_chans_cca_intf(ic, ic->ic_curchan, cur_bw, IEEE80211_SCS_COCHAN,
				compound_cca_intf, IEEE80211_SCS_CCA_INTF_SCALE, NULL, pmbl_err);

		/* update tx/rx time stats */
		as->as_tx_ms = scs_info_read.tx_usecs / 1000;
		as->as_rx_ms = scs_info_read.rx_usecs / 1000;
		as->as_tx_ms_smth = IEEE80211_SCS_SMOOTH(as->as_tx_ms_smth, as->as_tx_ms,
				ic->ic_scs.scs_as_tx_time_smth_fctr);
		as->as_rx_ms_smth = IEEE80211_SCS_SMOOTH(as->as_rx_ms_smth, as->as_rx_ms,
				ic->ic_scs.scs_as_rx_time_smth_fctr);
		SCSDBG(SCSLOG_INFO, "AS tx rx = %u %u, smoothed = %u %u\n",
				as->as_tx_ms, as->as_rx_ms, as->as_tx_ms_smth, as->as_rx_ms_smth);
	}

	cc_flag = ieee80211_is_cc_required(ic, compound_cca_intf,
			ic->ic_scs.scs_cca_idle_smthed, pmbl_err);
	if (cc_flag) {
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			cc_flag = ieee80211_scs_collect_ranking_stats(ic, &scs_info_read, cc_flag,
								compound_cca_intf);
			if (ic->ic_scs.scs_debug_enable)
				ieee80211_scs_show_ranking_stats(ic, 1, 0);

			int is_wds_rbs = ieee80211_scs_is_wds_rbs_node(ic);

			/*
			 * Pick a channel from DFS and Non-DFS set of channels;
			 * Consider channel margins as well while we pick the channel
			 */
			new_chan = ieee80211_scs_pick_channel(ic, IEEE80211_SCS_PICK_AVAILABLE_ANY_CHANNEL, cc_flag);

			if (!is_wds_rbs) {
				if (!cc_flag) {
					SCSDBG(SCSLOG_NOTICE, "all channel change request conditions are cleared\n");
					goto compare_end;
				}

				if (new_chan == SCS_BEST_CHAN_INVALID) {
					ieee80211_scs_update_dfs_reentry(ic, cc_flag, &dfs_reentry_clear);
					goto compare_end;
				}

				if (ic->ic_scs.scs_report_only) {
					SCSDBG(SCSLOG_NOTICE, "channel change is disabled under report only mode\n");
					goto compare_end;
				}

				if (ieee80211_is_cac_in_progress(ic)) {
					SCSDBG(SCSLOG_NOTICE, "Channel change is disabled during CAC\n");
					goto compare_end;
				}

				if ((cc_flag == IEEE80211_SCS_SELF_CCA_CC) &&
						ic->ic_get_cca_adjusting_status()) {
					SCSDBG(SCSLOG_NOTICE, "Self channel change is paused while adjusting cca threshold\n");
					goto compare_end;
				}

				if (tdls_update_failed || stats_unstable)
					goto compare_end;
				ic->ic_csw_reason = CSW_REASON_SET_SCS_FLAG(cc_flag, IEEE80211_CSW_REASON_SCS);
				if (!ieee80211_scs_change_channel(ic, new_chan)) {
					int curchan_ieee = ic->ic_curchan->ic_ieee;

					printk("SCS: Switching to chan %d, reason %x,"
							" cca_intf %u %u, pmbl_ap %u %u, pmbl_sta %u %u, cca_idle %u\n",
							new_chan, cc_flag, as->as_cca_intf[curchan_ieee], as->as_cca_intf[new_chan],
							as->as_pmbl_err_ap[curchan_ieee], as->as_pmbl_err_ap[new_chan],
							as->as_pmbl_err_sta[curchan_ieee], as->as_pmbl_err_sta[new_chan],
							ic->ic_scs.scs_cca_idle_smthed);
				}
			} else {
				/* In RBS mode - inform the MBS AP to change the channel */
				/* WDS Link to MBS? */
				if (new_chan == SCS_BEST_CHAN_INVALID) {
					SCSDBG(SCSLOG_NOTICE, "new channel recommendation %d\n", new_chan);
				}

				SCSDBG(SCSLOG_NOTICE, "SCS: send busy_fraction %u with cca_intf %u to AP, pmbl_error=%u %u\n",
					(raw_cca_intf * IEEE80211_11K_CCA_INTF_SCALE
					/ IEEE80211_SCS_CCA_INTF_SCALE),
					raw_cca_intf, ic->ic_scs.scs_sp_err_smthed, ic->ic_scs.scs_lp_err_smthed);

				vap = TAILQ_FIRST(&ic->ic_vaps);
				TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
					if (IEEE80211_VAP_WDS_IS_RBS(vap)) {
						struct ieee80211_node *ni = NULL;
						uint64_t tsf = 0;
						uint16_t others_time;

						ni = ieee80211_get_wds_peer_node_ref(vap);
						if (ni) {
							others_time = ni->ni_others_time;
							SCSDBG(SCSLOG_NOTICE,
								"RBS: allnodes(tx+rx)- wds %u\n", others_time);
							/* here replace the old csa mgmt packet sending*/
							ic->ic_get_tsf(&tsf);
							ieee80211_send_action_cca_report(ni, 0,
								(uint16_t)raw_cca_intf,
								tsf, (uint16_t)scs_info_read.cca_try,
								ic->ic_scs.scs_sp_err_smthed,
								ic->ic_scs.scs_lp_err_smthed,
								others_time, NULL, 0);
							ieee80211_free_node(ni);
						} else {
							printk("%s: RBS: WDS Peer Node is NULL\n",__func__);
						}
					}
				}
			}
		} else {
			/* In STA mode - inform AP to change the channel */
			SCSDBG(SCSLOG_NOTICE, "channel change is required on STA\n");
			if (!ic->ic_nonqtn_sta) {
				tdls_stats_buf = kmalloc(tdls_stats_buf_len, GFP_ATOMIC);
				if (!tdls_stats_buf) {
					SCSDBG(SCSLOG_NOTICE, "TDLS stats buffer allocation failed\n");
					goto compare_end;
				}

				TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
					memset(tdls_stats_buf, 0, tdls_stats_buf_len);
					extra_ie_len = ieee80211_scs_add_tdls_stats_ie(vap, &scs_info_read,
							tdls_stats_buf, tdls_stats_buf_len);
					/* STA should be associated */
					if (vap->iv_state == IEEE80211_S_RUN) {
						uint64_t tsf = 0;
						struct ieee80211_node *ni = vap->iv_bss;

						ic->ic_get_tsf(&tsf);
						SCSDBG(SCSLOG_NOTICE, "send busy_fraction %u with cca_intf %u to AP, pmbl_error=%u %u\n",
							(raw_cca_intf * IEEE80211_11K_CCA_INTF_SCALE / IEEE80211_SCS_CCA_INTF_SCALE),
							raw_cca_intf, ic->ic_scs.scs_sp_err_smthed, ic->ic_scs.scs_lp_err_smthed);
						/* here replace the old csa mgmt packet sending*/
						ieee80211_send_action_cca_report(ni, 0, (uint16_t)raw_cca_intf, tsf,
							       (uint16_t)scs_info_read.cca_try, ic->ic_scs.scs_sp_err_smthed,
							       ic->ic_scs.scs_lp_err_smthed, 0, tdls_stats_buf, extra_ie_len);
					}
				}

				if(tdls_stats_buf)
					kfree(tdls_stats_buf);
			}
		}
	}

compare_end:
	ieee80211_scs_clean_stats(ic, clean_level, dfs_reentry_clear);

	mod_timer(&ic->ic_scs.scs_compare_timer,
		  jiffies + (ic->ic_scs.scs_cca_sample_dur * HZ));
}

static void
ieee80211_scs_switch_channel_manually(struct ieee80211com *ic, int pick_flags)
{
	int	new_chan;

	if (!ic->ic_scs.scs_enable) {
		SCSDBG(SCSLOG_CRIT, "Stop switching channel because SCS is disabled!\n");
		return;
	}

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		ieee80211_scs_collect_node_atten(ic);

		if (ic->ic_scs.scs_debug_enable) {
			ieee80211_scs_show_ranking_stats(ic, 1, 0);
		}

		/*
		 * Pick a channel from DFS and Non-DFS sets;
		 * If the picked channel is non-DFS, OCAC performs off-channel CAC for DFS channel;
		 * If the picked channel is DFS, OCAC will not kickin
		 */
		new_chan = ieee80211_scs_pick_channel(ic,
				(pick_flags| IEEE80211_SCS_PICK_ANYWAY),
				IEEE80211_SCS_NA_CC);
		if (new_chan == SCS_BEST_CHAN_INVALID) {
			goto sc_err;
		}

		ic->ic_csw_reason = IEEE80211_CSW_REASON_SCS;
		if (!ieee80211_scs_change_channel(ic, new_chan)) {
			SCSDBG(SCSLOG_CRIT, "Switching to channel %d manually\n", new_chan);
		}
	} else {
		SCSDBG(SCSLOG_CRIT, "Support switch channel manually on AP side only now!\n");
	}

	return;

sc_err:
	SCSDBG(SCSLOG_CRIT, "Switch channel manually error!\n");
}

void ieee80211_scs_start_comparing_timer(struct ieee80211com *ic)
{
	init_timer(&ic->ic_scs.scs_compare_timer);
	ic->ic_scs.scs_compare_timer.function = ieee80211_scs_start_compare;
	ic->ic_scs.scs_compare_timer.data = (unsigned long) ic;
	ic->ic_scs.scs_compare_timer.expires = jiffies + IEEE80211_SCS_COMPARE_INIT_TIMER * HZ;
	add_timer(&ic->ic_scs.scs_compare_timer);
}

static int
ieee80211_wireless_scs_stats_task_start(struct ieee80211vap *vap, uint8_t start)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (start && !ic->ic_scs.scs_stats_on) {
		ieee80211_scs_clean_stats(ic, IEEE80211_SCS_STATE_RESET, 0);
		ieee80211_scs_start_comparing_timer(ic);
		ic->ic_scs.scs_stats_on = 1;
		ieee80211_wireless_scs_msg_send(vap, "SCS: stats task is started");
	} else if (!start && ic->ic_scs.scs_stats_on) {
		del_timer(&ic->ic_scs.scs_compare_timer);
		ic->ic_scs.scs_stats_on = 0;
		ieee80211_wireless_scs_msg_send(vap, "SCS: stats task is stopped");
	}

	return 0;
}

static int
ieee80211_wireless_scs_smpl_task_start(struct ieee80211vap *vap, uint8_t start)
{
#define IEEE80211_SCS_BUF_LEN	256
	char msg_buf[IEEE80211_SCS_BUF_LEN];
	struct ieee80211com *ic = vap->iv_ic;

	if (start && ((ic->ic_scs.scs_sample_intv < IEEE80211_SCS_SMPL_INTV_MIN) ||
		(ic->ic_scs.scs_sample_intv > IEEE80211_SCS_SMPL_INTV_MAX))) {
		return -1;
	}

	if (start) {
		cancel_delayed_work_sync(&ic->ic_scs_sample_work);
		snprintf(msg_buf, sizeof(msg_buf),
			"SCS: channel sampling started - interval is %u seconds",
			ic->ic_scs.scs_sample_intv);
		ieee80211_wireless_scs_msg_send(vap, msg_buf);
		INIT_DELAYED_WORK(&ic->ic_scs_sample_work,
				  ieee80211_wireless_scs_sampling_task);
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			schedule_delayed_work(&ic->ic_scs_sample_work, (HZ / 2));
		}
	} else {
		ieee80211_wireless_scs_msg_send(vap, "SCS: channel sampling disabled");
		cancel_delayed_work_sync(&ic->ic_scs_sample_work);
	}

	return 0;
}

static void
ieee80211_wireless_scs_report_show(struct ieee80211com *ic, uint16_t param)
{
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_scs_info scs_info_read;
	struct ieee80211vap *vap;
	uint32_t pmbl_fail;

	/* CCA information cannot match a certain channel under scan state */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state == IEEE80211_S_RUN) {
			break;
		}
	}
	if (vap == NULL) {
		printk("SCS: No VAP in running state, no report available\n");
		return;
	}

	if (param == IEEE80211_SCS_CHAN_ALL) {
		ieee80211_show_initial_ranking_stats(ic);
		ieee80211_scs_show_ranking_stats(ic, 0, 1);
	} else if (param == IEEE80211_SCS_CHAN_CURRENT) {
		if (ic->ic_scs.scs_stats_on) {
			/* Copy scs info into a local structure so MuC can continue to fill it in */
			memcpy(&scs_info_read, &sp->scs_info_lhost->scs_info[sp->scs_info_lhost->valid_index],
					sizeof(scs_info_read));
			if (scs_info_read.cca_try) {
				ieee80211_scs_scale_cochan_data(ic, &scs_info_read);
				pmbl_fail = (ic->ic_scs.scs_sp_err_smthed * ic->ic_scs.scs_sp_wf +
						ic->ic_scs.scs_lp_err_smthed * ic->ic_scs.scs_lp_wf) / 100;
				printk("SCS: current channel %d, cca_try=%u, cca_idle=%u cca_busy=%u cca_intf=%u cca_tx=%u tx_ms=%u rx_ms=%u"
					" pmbl_cnt=%u\n",
					ic->ic_curchan->ic_ieee,
					scs_info_read.cca_try,
					scs_info_read.cca_idle,
					scs_info_read.cca_busy,
					scs_info_read.cca_interference,
					scs_info_read.cca_tx,
					scs_info_read.tx_usecs / 1000,
					scs_info_read.rx_usecs / 1000,
					pmbl_fail);
			} else {
				printk("Current channel report is temporarily not available, please try later\n");
			}
		} else {
			printk("SCS is disabled, no report available for current channel\n");
		}
	}
}

static void
ieee80211_wireless_scs_get_internal_stats(struct ieee80211com *ic, uint16_t param)
{
	int i;

	if (!ic->ic_scs.scs_stats_on) {
		printk("SCS stats is off, no stats available\n");
		return;
	}

	printk("SCS lhost stats: off-channel sample counter:\n");
	for (i = 0; i < IEEE80211_SCS_CNT_MAX; i++) {
		printk("NO.%d=%u\n", i, ic->ic_scs.scs_cnt[i]);
	}
}

int
ieee80211_param_scs_set(struct net_device *dev, struct ieee80211vap *vap, u_int32_t value)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint16_t cmd =	value >> IEEE80211_SCS_COMMAND_S;
	uint16_t arg = value & IEEE80211_SCS_VALUE_M;
	uint8_t u8_arg0 = arg >> 8;
	uint8_t u8_arg1 = arg & 0xFF;
#if TOPAZ_FPGA_PLATFORM
	printk("SCS is not supported yet on Topaz\n");
	return -1;
#endif

	if (cmd >= IEEE80211_SCS_SET_MAX) {
		printk(KERN_WARNING "%s: SCS: invalid config cmd %u, arg=%u\n",
				dev->name, cmd, arg);
		return -1;
	}

	SCSDBG(SCSLOG_INFO, "set param %u to value 0x%x\n", cmd, arg);

	switch (cmd) {
	case IEEE80211_SCS_SET_ENABLE:
		if (arg > 1) {
			return -1;
		}

		if (ic->ic_scs.scs_enable != arg) {
			printk("%sabling SCS\n", arg ? "En" : "Dis");
			ic->ic_scs.scs_enable = arg;
			if (ic->ic_scs.scs_enable) {
				if (ieee80211_wireless_scs_stats_task_start(vap, 1) < 0) {
					return -1;
				}
			}
			/* SCS off channel sampling follows SCS */
			if (!ic->ic_scs.scs_smpl_enable != !arg) {
				if (ieee80211_wireless_scs_smpl_task_start(vap, arg) < 0) {
					return -1;
				} else {
					ic->ic_scs.scs_smpl_enable = arg;
				}
			}
		} else {
			return 0;
		}
		break;
	case IEEE80211_SCS_SET_DEBUG_ENABLE:
		if (arg > 3) {
			return -1;
		}
		if( ic->ic_scs.scs_debug_enable != arg) {
			ic->ic_scs.scs_debug_enable = arg;
		}
		break;
	case IEEE80211_SCS_SET_SAMPLE_ENABLE:
		if (arg > 1) {
			return -1;
		}

		if (ic->ic_scs.scs_smpl_enable != arg) {
			if (ieee80211_wireless_scs_smpl_task_start(vap, arg) < 0) {
				return -1;
			} else {
				ic->ic_scs.scs_smpl_enable = arg;
			}
		}
		break;
	case IEEE80211_SCS_SET_SAMPLE_DWELL_TIME:
		if (arg < IEEE80211_SCS_SMPL_DWELL_TIME_MIN ||
			arg > IEEE80211_SCS_SMPL_DWELL_TIME_MAX) {
			return -1;
		}
		if (ic->ic_scs.scs_smpl_dwell_time != arg) {
			ic->ic_scs.scs_smpl_dwell_time = arg;
		}
		break;
	case IEEE80211_SCS_SET_SAMPLE_INTERVAL:
		if (arg < IEEE80211_SCS_SMPL_INTV_MIN ||
			arg > IEEE80211_SCS_SMPL_INTV_MAX) {
			return -1;
		}
		ic->ic_scs.scs_sample_intv = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_SMPL_PKTNUM:
		ic->ic_scs.scs_thrshld_smpl_pktnum = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_SMPL_AIRTIME:
		ic->ic_scs.scs_thrshld_smpl_airtime = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_ATTEN_INC:
		if (arg > IEEE80211_SCS_THRSHLD_ATTEN_INC_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "attenuation increase threshold change from %u to %u\n",
				ic->ic_scs.scs_thrshld_atten_inc, arg);
		ic->ic_scs.scs_thrshld_atten_inc = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_DFS_REENTRY:
		SCSDBG(SCSLOG_NOTICE, "DFS reentry threshold change from %u to %u\n",
				ic->ic_scs.scs_thrshld_dfs_reentry, arg);
		ic->ic_scs.scs_thrshld_dfs_reentry = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_DFS_REENTRY_INTF:
		printk("DFS reentry cca intf threshold change from %u to %u\n",
				ic->ic_scs.scs_thrshld_dfs_reentry_intf, arg);
		ic->ic_scs.scs_thrshld_dfs_reentry_intf = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_DFS_REENTRY_MINRATE:
		SCSDBG(SCSLOG_NOTICE, "DFS reentry minrate threshold change from %u to %u, unit 100kbps\n",
				ic->ic_scs.scs_thrshld_dfs_reentry_minrate, arg);
		ic->ic_scs.scs_thrshld_dfs_reentry_minrate = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_LOAD:
		if (arg > IEEE80211_SCS_THRSHLD_LOADED_MAX)
			return -1;
		SCSDBG(SCSLOG_NOTICE, "traffic load threshold change from %u to %u\n",
					ic->ic_scs.scs_thrshld_loaded, arg);
		ic->ic_scs.scs_thrshld_loaded = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_AGING_NOR:
		SCSDBG(SCSLOG_NOTICE, "normal aging threshold change from %u to %u minutes\n",
					ic->ic_scs.scs_thrshld_aging_nor, arg);
		ic->ic_scs.scs_thrshld_aging_nor = arg;
		break;
	case IEEE80211_SCS_SET_THRSHLD_AGING_DFSREENT:
		SCSDBG(SCSLOG_NOTICE, "dfs re-entry aging threshold change from %u to %u minutes\n",
					ic->ic_scs.scs_thrshld_aging_dfsreent, arg);
		ic->ic_scs.scs_thrshld_aging_dfsreent = arg;
		break;
	case IEEE80211_SCS_SET_CCA_IDLE_THRSHLD:
		SCSDBG(SCSLOG_NOTICE, "cca idle threshold change from %u to %u\n",
				ic->ic_scs.scs_cca_idle_thrshld, arg);
		ic->ic_scs.scs_cca_idle_thrshld = arg;
		break;
	case IEEE80211_SCS_SET_PMBL_ERR_THRSHLD:
		SCSDBG(SCSLOG_NOTICE, "pmbl error threshold change from %u to %u\n",
				ic->ic_scs.scs_pmbl_err_thrshld, arg);
		ic->ic_scs.scs_pmbl_err_thrshld = arg;
		break;
	case IEEE80211_SCS_SET_CCA_INTF_LO_THR:
		SCSDBG(SCSLOG_NOTICE, "cca intf low threshold change from %u to %u\n",
				ic->ic_scs.scs_cca_intf_lo_thrshld, arg);
		ic->ic_scs.scs_cca_intf_lo_thrshld = arg;
		break;
	case IEEE80211_SCS_SET_CCA_INTF_HI_THR:
		SCSDBG(SCSLOG_NOTICE, "cca intf high threshold change from %u to %u\n",
				ic->ic_scs.scs_cca_intf_hi_thrshld, arg);
		ic->ic_scs.scs_cca_intf_hi_thrshld = arg;
		break;
	case IEEE80211_SCS_SET_CCA_INTF_RATIO:
		SCSDBG(SCSLOG_NOTICE, "cca intf ratio threshold change from %u to %u\n",
				ic->ic_scs.scs_cca_intf_ratio, arg);
		ic->ic_scs.scs_cca_intf_ratio = arg;
		break;
	case IEEE80211_SCS_SET_CCA_INTF_DFS_MARGIN:
		SCSDBG(SCSLOG_NOTICE, "cca intf dfs margin change from %u to %u\n",
				ic->ic_scs.scs_cca_intf_dfs_margin, arg);
		ic->ic_scs.scs_cca_intf_dfs_margin = arg;
		break;
	case IEEE80211_SCS_SET_CCA_SMPL_DUR:
		if (arg < IEEE80211_SCS_CCA_DUR_MIN ||
			arg > IEEE80211_SCS_CCA_DUR_MAX) {
			return -1;
		}
		ic->ic_scs.scs_cca_sample_dur = arg;
		ieee80211_scs_clean_stats(ic, IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN, 0);
		break;
	case IEEE80211_SCS_SET_REPORT_ONLY:
		ic->ic_scs.scs_report_only = arg;
		break;
	case IEEE80211_SCS_GET_REPORT:
		ieee80211_wireless_scs_report_show(ic, arg);
		break;
	case IEEE80211_SCS_GET_INTERNAL_STATS:
		ieee80211_wireless_scs_get_internal_stats(ic, arg);
		break;
	case IEEE80211_SCS_SET_CCA_INTF_SMTH_FCTR:
		if ((u8_arg0 > IEEE80211_CCA_INTF_SMTH_FCTR_MAX) ||
			(u8_arg1 > IEEE80211_CCA_INTF_SMTH_FCTR_MAX)) {
			return -1;
		}
		ic->ic_scs.scs_cca_intf_smth_fctr[SCS_CCA_INTF_SMTH_FCTR_NOXP] = u8_arg0;
		ic->ic_scs.scs_cca_intf_smth_fctr[SCS_CCA_INTF_SMTH_FCTR_XPED] = u8_arg1;
		break;
	case IEEE80211_SCS_RESET_RANKING_TABLE:
		ieee80211_scs_clean_stats(ic, IEEE80211_SCS_STATE_RESET, 0);
		break;
	case IEEE80211_SCS_SET_CHAN_MTRC_MRGN:
		if (arg > IEEE80211_SCS_CHAN_MTRC_MRGN_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "chan metric margin change from %u to %u\n",
				ic->ic_scs.scs_chan_mtrc_mrgn, arg);
		ic->ic_scs.scs_chan_mtrc_mrgn = arg;
		break;
	case IEEE80211_SCS_SET_LEAVE_DFS_CHAN_MTRC_MRGN:
		if (arg > IEEE80211_SCS_CHAN_MTRC_MRGN_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "Leave DFS chan metric margin change from %u to %u\n",
				ic->ic_scs.scs_leavedfs_chan_mtrc_mrgn, arg);
		ic->ic_scs.scs_leavedfs_chan_mtrc_mrgn = arg;
		break;
	case IEEE80211_SCS_SET_RSSI_SMTH_FCTR:
		if ((u8_arg0 > IEEE80211_SCS_RSSI_SMTH_FCTR_MAX) ||
			(u8_arg1 > IEEE80211_SCS_RSSI_SMTH_FCTR_MAX))	{
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "rssi smoothing factor(up/down) change from %u/%u to %u/%u\n",
				ic->ic_scs.scs_rssi_smth_fctr[SCS_RSSI_SMTH_FCTR_UP],
				ic->ic_scs.scs_rssi_smth_fctr[SCS_RSSI_SMTH_FCTR_DOWN],
				u8_arg0, u8_arg1);
		ic->ic_scs.scs_rssi_smth_fctr[SCS_RSSI_SMTH_FCTR_UP] = u8_arg0;
		ic->ic_scs.scs_rssi_smth_fctr[SCS_RSSI_SMTH_FCTR_DOWN] = u8_arg1;
		break;
	case IEEE80211_SCS_SET_ATTEN_ADJUST:
		if (((int8_t)arg < IEEE80211_SCS_ATTEN_ADJUST_MIN) ||
			((int8_t)arg > IEEE80211_SCS_ATTEN_ADJUST_MAX)) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "attenuation adjust change from %d to %d\n",
				ic->ic_scs.scs_atten_adjust, (int8_t)arg);
		ic->ic_scs.scs_atten_adjust = (int8_t)arg;
		break;
	case IEEE80211_SCS_SET_ATTEN_SWITCH_ENABLE:
		if (arg > 1) {
			return -1;
		}

		if (ic->ic_scs.scs_atten_sw_enable != arg) {
			SCSDBG(SCSLOG_NOTICE, "attenuation channel change logic is %s\n",
						arg ? "enabled" : "disabled");
			ic->ic_scs.scs_atten_sw_enable = (uint16_t)arg;
		}
		break;
	case IEEE80211_SCS_SET_PMBL_ERR_SMTH_FCTR:
		if (arg > IEEE80211_SCS_PMBL_ERR_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "preamble error smoothing factor change from %u to %u\n",
				ic->ic_scs.scs_pmbl_err_smth_fctr, arg);
		ic->ic_scs.scs_pmbl_err_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_PMP_RPT_CCA_SMTH_FCTR:
		if (arg > IEEE80211_SCS_PMP_RPT_CCA_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_pmp_rpt_cca_smth_fctr change from %u to %u\n",
				ic->ic_scs.scs_pmp_rpt_cca_smth_fctr, arg);
		ic->ic_scs.scs_pmp_rpt_cca_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_PMP_RX_TIME_SMTH_FCTR:
		if (arg > IEEE80211_SCS_PMP_RX_TIME_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_pmp_rx_time_smth_fctr change from %u to %u\n",
				ic->ic_scs.scs_pmp_rx_time_smth_fctr, arg);
		ic->ic_scs.scs_pmp_rx_time_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_PMP_TX_TIME_SMTH_FCTR:
		if (arg > IEEE80211_SCS_PMP_TX_TIME_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_pmp_tx_time_smth_fctr change from %u to %u\n",
				ic->ic_scs.scs_pmp_tx_time_smth_fctr, arg);
		ic->ic_scs.scs_pmp_tx_time_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_PMP_STATS_STABLE_PERCENT:
		if (arg > IEEE80211_SCS_PMP_STATS_STABLE_PERCENT_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_pmp_stats_stable_percent change from %u to %u\n",
				ic->ic_scs.scs_pmp_stats_stable_percent, arg);
		ic->ic_scs.scs_pmp_stats_stable_percent = arg;
		break;
	case IEEE80211_SCS_SET_PMP_STATS_STABLE_RANGE:
		if (arg > IEEE80211_SCS_PMP_STATS_STABLE_RANGE_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_pmp_stats_stable_range change from %u to %u\n",
				ic->ic_scs.scs_pmp_stats_stable_range, arg);
		ic->ic_scs.scs_pmp_stats_stable_range = arg;
		break;
	case IEEE80211_SCS_SET_PMP_STATS_CLEAR_INTERVAL:
		if (arg > IEEE80211_SCS_PMP_STATS_CLEAR_INTERVAL_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_pmp_stats_clear_interval change from %u to %u\n",
				ic->ic_scs.scs_pmp_stats_clear_interval, arg);
		ic->ic_scs.scs_pmp_stats_clear_interval = arg;
		break;
	case IEEE80211_SCS_SET_PMP_TXTIME_COMPENSATION:
		ieee80211_scs_set_time_compensation(SCS_TX_COMPENSTATION, u8_arg0, u8_arg1);
		break;
	case IEEE80211_SCS_SET_PMP_RXTIME_COMPENSATION:
		ieee80211_scs_set_time_compensation(SCS_RX_COMPENSTATION, u8_arg0, u8_arg1);
		break;
	case IEEE80211_SCS_SET_PMP_TDLSTIME_COMPENSATION:
		ieee80211_scs_set_time_compensation(SCS_TDLS_COMPENSTATION, u8_arg0, u8_arg1);
		break;
	case IEEE80211_SCS_SET_SWITCH_CHANNEL_MANUALLY:
		ieee80211_scs_switch_channel_manually(ic, arg);
		break;
	case IEEE80211_SCS_SET_AS_RX_TIME_SMTH_FCTR:
		if (arg > IEEE80211_SCS_AS_RX_TIME_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_as_rx_time_smth_fctr change from %u to %u\n",
				ic->ic_scs.scs_as_rx_time_smth_fctr, arg);
		ic->ic_scs.scs_as_rx_time_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_AS_TX_TIME_SMTH_FCTR:
		if (arg > IEEE80211_SCS_AS_TX_TIME_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "scs_as_tx_time_smth_fctr change from %u to %u\n",
				ic->ic_scs.scs_as_tx_time_smth_fctr, arg);
		ic->ic_scs.scs_as_tx_time_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_PMBL_ERR_RANGE:
		if (arg < IEEE80211_SCS_PMBL_ERR_RANGE_MIN) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "preamble error range change from %u to %u\n",
				ic->ic_scs.scs_pmbl_err_range, arg);
		ic->ic_scs.scs_pmbl_err_range = arg;
		break;
	case IEEE80211_SCS_SET_PMBL_ERR_MAPPED_INTF_RANGE:
		if (arg > IEEE80211_SCS_PMBL_ERR_MAPPED_INTF_RANGE_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "preamble error mapped cca_intf range change from %u to %u\n",
				ic->ic_scs.scs_pmbl_err_mapped_intf_range, arg);
		ic->ic_scs.scs_pmbl_err_mapped_intf_range = arg;
		break;
	case IEEE80211_SCS_SET_PMBL_ERR_WF:
		if ((u8_arg0 > IEEE80211_SCS_PMBL_ERR_WF_MAX) ||
			(u8_arg1 > IEEE80211_SCS_PMBL_ERR_WF_MAX))	{
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "preamble error weighting factor(sp/lp) change from %u/%u to %u/%u\n",
					ic->ic_scs.scs_sp_wf,
					ic->ic_scs.scs_lp_wf,
					u8_arg0, u8_arg1);
		ic->ic_scs.scs_sp_wf = u8_arg0;
		ic->ic_scs.scs_lp_wf = u8_arg1;
		break;
	case IEEE80211_SCS_SET_STATS_START:
		SCSDBG(SCSLOG_NOTICE, "%sing scs stats\n", arg ? "start" : "stopp");
		if (ieee80211_wireless_scs_stats_task_start(vap, !!arg) < 0) {
			return -1;
		}
		break;
	case IEEE80211_SCS_SET_CCA_IDLE_SMTH_FCTR:
		if (arg > IEEE80211_SCS_CCA_IDLE_SMTH_FCTR_MAX) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "cca idle smoothing factor change from %u to %u\n",
				ic->ic_scs.scs_cca_idle_smth_fctr, arg);
		ic->ic_scs.scs_cca_idle_smth_fctr = arg;
		break;
	case IEEE80211_SCS_SET_CCA_THRESHOLD_TYPE:
		if (arg > 1) {
			return -1;
		}
		SCSDBG(SCSLOG_NOTICE, "cca thresholds switched to %s sensitive ones\n",
				arg ? "more" : "less");
		ic->ic_scs.scs_cca_threshold_type = arg + 1;
		break;
	default:
		break;
	}

	SCSDBG(SCSLOG_INFO, "set param %u to value 0x%x completed successfully\n",
			cmd, arg);

	return 0;
}
EXPORT_SYMBOL(ieee80211_param_scs_set);

static int
ieee80211_scs_get_currchan_rpt(struct ieee80211com *ic, struct ieee80211req_scs *req, uint32_t *reason)
{
	struct ieee80211req_scs_currchan_rpt rpt;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_scs_info scs_info_read;
	struct ieee80211vap *vap;

	if (!ic->ic_scs.scs_stats_on) {
		*reason = IEEE80211REQ_SCS_RESULT_SCS_DISABLED;
		return -EINVAL;
	}

	/* CCA information cannot match a certain channel under scan state */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state == IEEE80211_S_RUN) {
			break;
		}
	}
	if (vap == NULL) {
		*reason = IEEE80211REQ_SCS_RESULT_NO_VAP_RUNNING;
		return -EINVAL;
	}

	/* Copy scs info into a local structure so MuC can continue to fill it in */
	memcpy(&scs_info_read, &sp->scs_info_lhost->scs_info[sp->scs_info_lhost->valid_index],
			sizeof(scs_info_read));
	if (!scs_info_read.cca_try) {
		*reason = IEEE80211REQ_SCS_RESULT_TMP_UNAVAILABLE;
		return -EAGAIN;
	}

	ieee80211_scs_scale_cochan_data(ic, &scs_info_read);

	memset(&rpt, 0x0, sizeof(struct ieee80211req_scs_currchan_rpt));

	rpt.iscr_curchan = ic->ic_curchan->ic_ieee;
	rpt.iscr_cca_try = scs_info_read.cca_try;
	rpt.iscr_cca_idle = scs_info_read.cca_idle;
	rpt.iscr_cca_busy = scs_info_read.cca_busy;
	rpt.iscr_cca_intf = scs_info_read.cca_interference;
	rpt.iscr_cca_tx = scs_info_read.cca_tx;
	rpt.iscr_tx_ms = scs_info_read.tx_usecs / 1000;
	rpt.iscr_rx_ms = scs_info_read.rx_usecs / 1000;
	rpt.iscr_pmbl = (ic->ic_scs.scs_sp_err_smthed * ic->ic_scs.scs_sp_wf +
			ic->ic_scs.scs_lp_err_smthed * ic->ic_scs.scs_lp_wf) / 100;

	if (copy_to_user(req->is_data, &rpt, req->is_data_len)) {
		SCSDBG(SCSLOG_NOTICE, "copy_to_user data failed with op=0x%x\n", req->is_op);
		return -EIO;
	}

	return 0;
}

static int
ieee80211_scs_get_ranking_rpt(struct ieee80211com *ic, struct ieee80211req_scs *req, uint32_t *reason)
{
	static struct ieee80211req_scs_ranking_rpt rpt;
	struct ieee80211req_scs_ranking_rpt_chan *chan_rpt;
	int i;
	int num = 0;
	struct ieee80211_channel *chan;
	struct ap_state *as;

	if (!ic->ic_scs.scs_stats_on) {
		*reason = IEEE80211REQ_SCS_RESULT_SCS_DISABLED;
		return -EINVAL;
	}

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		*reason = IEEE80211REQ_SCS_RESULT_APMODE_ONLY;
		return -EINVAL;
	}

	as = ic->ic_scan->ss_scs_priv;

	memset(&rpt, 0x0, sizeof(struct ieee80211req_scs_ranking_rpt));

	/* the ranking table */
	for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
		chan = ieee80211_find_channel_by_ieee(ic, i);
		if (chan == NULL) {
			continue;
		}

		chan_rpt = &rpt.isr_chans[num];
		chan_rpt->isrc_chan = i;
		chan_rpt->isrc_dfs = !!(chan->ic_flags & IEEE80211_CHAN_DFS);
		chan_rpt->isrc_txpwr = chan->ic_maxpower;
		chan_rpt->isrc_cca_intf = (as->as_cca_intf[i] == SCS_CCA_INTF_INVALID) ? 0 : as->as_cca_intf[i];
		chan_rpt->isrc_metric = as->as_chanmetric[i];
		chan_rpt->isrc_metric_age = (jiffies - as->as_chanmetric_timestamp[i]) / HZ;
		chan_rpt->isrc_pmbl_ap = as->as_pmbl_err_ap[i];
		chan_rpt->isrc_pmbl_sta = as->as_pmbl_err_sta[i];
		chan_rpt->isrc_times = ic->ic_chan_occupy_record.times[i];
		chan_rpt->isrc_duration = ic->ic_chan_occupy_record.duration[i];
		chan_rpt->isrc_chan_avail_status = ic->ic_chan_availability_status[chan->ic_ieee];
		if (i == ic->ic_chan_occupy_record.cur_chan) {
			chan_rpt->isrc_duration += (jiffies - INITIAL_JIFFIES) / HZ -
					ic->ic_chan_occupy_record.occupy_start;
		}

		num++;
		if (num >= IEEE80211REQ_SCS_REPORT_CHAN_NUM)
			break;
	}
	rpt.isr_num = num;

	if (copy_to_user(req->is_data, &rpt, req->is_data_len)) {
		SCSDBG(SCSLOG_NOTICE, "copy_to_user data failed with op=0x%x\n", req->is_op);
		return -EIO;
	}

	return 0;
}

static int
ieee80211_scs_get_score_rpt(struct ieee80211com *ic, struct ieee80211req_scs *req, uint32_t *reason)
{
	static struct ieee80211req_scs_score_rpt rpt;
	struct ieee80211req_scs_score_rpt_chan *chan_rpt;
	struct ieee80211_channel *chan;
	int32_t chan_metric[IEEE80211REQ_SCS_REPORT_CHAN_NUM];
	int32_t max_metric = -1;
	int32_t min_metric = -1;
	int32_t max_diff;
	int iter;
	int num = 0;
	uint8_t rate_ratio;

	if (!ic->ic_scs.scs_stats_on) {
		*reason = IEEE80211REQ_SCS_RESULT_SCS_DISABLED;
		return -EINVAL;
	}

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		*reason = IEEE80211REQ_SCS_RESULT_APMODE_ONLY;
		return -EINVAL;
	}

	if (ic->ic_scan == NULL ||
			ic->ic_scan->ss_scs_priv == NULL ||
			ic->ic_curchan == IEEE80211_CHAN_ANYC) {
		*reason = IEEE80211REQ_SCS_RESULT_TMP_UNAVAILABLE;
		return -EINVAL;
	}

	memset(&rpt, 0x0, sizeof(struct ieee80211req_scs_score_rpt));

	ieee80211_scs_aging(ic, ic->ic_scs.scs_thrshld_aging_nor);
	rate_ratio = ieee80211_scs_calc_rate_ratio(ic);

	for (iter = 0; iter < ic->ic_nchans; iter++) {
		chan = &ic->ic_channels[iter];
		if (!isset(ic->ic_chan_active, chan->ic_ieee)) {
			continue;
		}
		chan_rpt = &rpt.isr_chans[num];
		chan_rpt->isrc_chan = chan->ic_ieee;
		chan_rpt->isrc_score = 0;

		ieee80211_scs_get_chan_metric(ic, chan, rate_ratio,
				&chan_metric[num], NULL, IEEE80211_SCS_NA_CC);
		if (chan_metric[num] >= 0 &&
				chan_metric[num] < SCS_MAX_RAW_CHAN_METRIC) {
			if (max_metric == -1 ||
					chan_metric[num] > max_metric) {
				max_metric = chan_metric[num];
			}
			if (min_metric == -1 ||
					chan_metric[num] < min_metric) {
				min_metric = chan_metric[num];
			}
		}

		num++;
		if (num >= IEEE80211REQ_SCS_REPORT_CHAN_NUM) {
			break;
		}
	}
	if (num == 0) {
		*reason = IEEE80211REQ_SCS_RESULT_TMP_UNAVAILABLE;
		return -EINVAL;
	}

	rpt.isr_num = num;

	/* For the scoring, the algorithm as below:
	 *   1. For the channel which has the minimum valid metric, the score is 100
	 *   2. For the channel which has the maximum valid metric, the score is 0
	 *   3. For other channels that have valid metrics, the score is
	 *         100 * (max_valid_metric - metric) / (max_valid_metric - min_valid_metric)
	 *   4. For invalid metric, if it is larger than maximum valid metric, the score is 0;
	 *      and if it is less than minimum valid metric - it may result from too large
	 *      power difference from the current channel, the score is 100.
	 */
	if (max_metric != -1) {
		max_diff = max_metric - min_metric;
		for (iter = 0; iter < num; iter++) {
			chan_rpt = &rpt.isr_chans[iter];
			if (chan_metric[iter] > max_metric) {
				chan_rpt->isrc_score = 0;
			} else if (chan_metric[iter] < min_metric){
				chan_rpt->isrc_score = 100;
			} else {
				if (max_diff == 0) {
					chan_rpt->isrc_score = 100;
				} else {
					chan_rpt->isrc_score =
							100 * (max_metric - chan_metric[iter]) / max_diff;
				}
			}
		}
	}


	if (copy_to_user(req->is_data, &rpt, req->is_data_len)) {
		SCSDBG(SCSLOG_NOTICE, "copy_to_user data failed with op=0x%x\n", req->is_op);
		return -EIO;
	}

	return 0;
}

static int
ieee80211_scs_get_init_ranking_rpt(struct ieee80211com *ic, struct ieee80211req_scs *req, uint32_t *reason)
{
	static struct ieee80211req_scs_ranking_rpt rpt;
	struct ieee80211req_scs_ranking_rpt_chan *chan_rpt;
	int i;
	int num = 0;
	struct ieee80211_channel *chan;
	struct ap_state *as;
	struct ieee80211vap *vap;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		*reason = IEEE80211REQ_SCS_RESULT_APMODE_ONLY;
		return -EINVAL;
	}

	as = ic->ic_scan->ss_priv;
	if (as == NULL) {
		*reason = IEEE80211REQ_SCS_RESULT_AUTOCHAN_DISABLED;
		return -EINVAL;
	}

	/* when in auto channel scanning */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state == IEEE80211_S_RUN) {
			break;
		}
	}
	if (vap == NULL) {
		*reason = IEEE80211REQ_SCS_RESULT_TMP_UNAVAILABLE;
		return -EAGAIN;
	}

	memset(&rpt, 0x0, sizeof(struct ieee80211req_scs_ranking_rpt));

	/* the ranking table */
	for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
		chan = ieee80211_find_channel_by_ieee(ic, i);
		if (chan == NULL) {
			continue;
		}

		chan_rpt = &rpt.isr_chans[num];
		chan_rpt->isrc_chan = i;
		chan_rpt->isrc_dfs = !!(chan->ic_flags & IEEE80211_CHAN_DFS);
		chan_rpt->isrc_txpwr = chan->ic_maxpower;
		chan_rpt->isrc_numbeacons = as->as_numbeacons[i];
		chan_rpt->isrc_metric = as->as_chanmetric[i];
		chan_rpt->isrc_cci = as->as_cci[i];
		chan_rpt->isrc_aci = as->as_aci[i];

		num++;
		if (num >= IEEE80211REQ_SCS_REPORT_CHAN_NUM)
			break;
	}
	rpt.isr_num = num;

	if (copy_to_user(req->is_data, &rpt, req->is_data_len)) {
		SCSDBG(SCSLOG_NOTICE, "copy_to_user data failed with op=0x%x\n", req->is_op);
		return -EIO;
	}

	return 0;
}

static int
ieee80211_scs_get_param_rpt(struct ieee80211com *ic, struct ieee80211req_scs *req, uint32_t *reason)
{
	int retval = 0;
	uint32_t i, len;
	struct ieee80211req_scs_param_rpt *rpt;
	struct ieee80211_scs *scs = &(ic->ic_scs);

	len = sizeof(*rpt)*SCS_PARAM_MAX;
	rpt = (struct ieee80211req_scs_param_rpt *)kmalloc(len, GFP_KERNEL);
	if (rpt == NULL) {
		retval = -EIO;
		goto ready_to_return;
	}

	memset(rpt, 0, len);

	rpt[SCS_SMPL_DWELL_TIME].cfg_param = scs->scs_smpl_dwell_time;
	rpt[SCS_SAMPLE_INTV].cfg_param = scs->scs_sample_intv;
	rpt[SCS_THRSHLD_SMPL_PKTNUM].cfg_param = scs->scs_thrshld_smpl_pktnum;
	rpt[SCS_THRSHLD_SMPL_AIRTIME].cfg_param = scs->scs_thrshld_smpl_airtime;

	rpt[SCS_THRSHLD_ATTEN_INC].cfg_param = scs->scs_thrshld_atten_inc;
	rpt[SCS_THRSHLD_DFS_REENTRY].cfg_param = scs->scs_thrshld_dfs_reentry;
	rpt[SCS_THRSHLD_DFS_REENTRY_MINRATE].cfg_param = scs->scs_thrshld_dfs_reentry_minrate;
	rpt[SCS_THRSHLD_DFS_REENTRY_INTF].cfg_param = scs->scs_thrshld_dfs_reentry_intf;
	rpt[SCS_THRSHLD_LOADED].cfg_param = scs->scs_thrshld_loaded;
	rpt[SCS_THRSHLD_AGING_NOR].cfg_param = scs->scs_thrshld_aging_nor;
	rpt[SCS_THRSHLD_AGING_DFSREENT].cfg_param = scs->scs_thrshld_aging_dfsreent;

	rpt[SCS_ENABLE].cfg_param = (uint32_t)(scs->scs_enable);
	rpt[SCS_DEBUG_ENABLE].cfg_param = (uint32_t)(scs->scs_debug_enable);
	rpt[SCS_SMPL_ENABLE].cfg_param = (uint32_t)(scs->scs_smpl_enable);
	rpt[SCS_REPORT_ONLY].cfg_param = (uint32_t)(scs->scs_report_only);

	rpt[SCS_CCA_IDLE_THRSHLD].cfg_param = scs->scs_cca_idle_thrshld;
	rpt[SCS_CCA_INTF_HI_THRSHLD].cfg_param = scs->scs_cca_intf_hi_thrshld;
	rpt[SCS_CCA_INTF_LO_THRSHLD].cfg_param = scs->scs_cca_intf_lo_thrshld;
	rpt[SCS_CCA_INTF_RATIO].cfg_param = scs->scs_cca_intf_ratio;
	rpt[SCS_CCA_INTF_DFS_MARGIN].cfg_param = scs->scs_cca_intf_dfs_margin;
	rpt[SCS_PMBL_ERR_THRSHLD].cfg_param = scs->scs_pmbl_err_thrshld;
	rpt[SCS_CCA_SAMPLE_DUR].cfg_param = scs->scs_cca_sample_dur;
	rpt[SCS_CCA_INTF_SMTH_NOXP].cfg_param = (uint32_t)(scs->scs_cca_intf_smth_fctr[0]);
	rpt[SCS_CCA_INTF_SMTH_XPED].cfg_param = (uint32_t)(scs->scs_cca_intf_smth_fctr[1]);
	rpt[SCS_RSSI_SMTH_UP].cfg_param = (uint32_t)(scs->scs_rssi_smth_fctr[0]);
	rpt[SCS_RSSI_SMTH_DOWN].cfg_param = (uint32_t)(scs->scs_rssi_smth_fctr[1]);

	rpt[SCS_CHAN_MTRC_MRGN].cfg_param = (uint32_t)(scs->scs_chan_mtrc_mrgn);
	rpt[SCS_LEAVE_DFS_CHAN_MTRC_MRGN].cfg_param = (uint32_t)(scs->scs_leavedfs_chan_mtrc_mrgn);

	rpt[SCS_ATTEN_ADJUST].signed_param_flag  = 1;
	rpt[SCS_ATTEN_ADJUST].cfg_param = (uint32_t)(scs->scs_atten_adjust);
	rpt[SCS_ATTEN_SW_ENABLE].cfg_param = (uint32_t)(scs->scs_atten_sw_enable);

	rpt[SCS_PMBL_ERR_SMTH_FCTR].cfg_param = scs->scs_pmbl_err_smth_fctr;
	rpt[SCS_PMBL_ERR_RANGE].cfg_param = scs->scs_pmbl_err_range;
	rpt[SCS_PMBL_ERR_MAPPED_INTF_RANGE].cfg_param = scs->scs_pmbl_err_mapped_intf_range;
	rpt[SCS_SP_WF].cfg_param = scs->scs_sp_wf;
	rpt[SCS_LP_WF].cfg_param = scs->scs_lp_wf;
	rpt[SCS_PMP_RPT_CCA_SMTH_FCTR].cfg_param = (uint32_t)(scs->scs_pmp_rpt_cca_smth_fctr);
	rpt[SCS_PMP_RX_TIME_SMTH_FCTR].cfg_param = (uint32_t)(scs->scs_pmp_rx_time_smth_fctr);
	rpt[SCS_PMP_TX_TIME_SMTH_FCTR].cfg_param = (uint32_t)(scs->scs_pmp_tx_time_smth_fctr);
	rpt[SCS_PMP_STATS_STABLE_PERCENT].cfg_param = (uint32_t)(scs->scs_pmp_stats_stable_percent);
	rpt[SCS_PMP_STATS_STABLE_RANGE].cfg_param = (uint32_t)(scs->scs_pmp_stats_stable_range);
	rpt[SCS_PMP_STATS_CLEAR_INTERVAL].cfg_param = (uint32_t)(scs->scs_pmp_stats_clear_interval);
	rpt[SCS_AS_RX_TIME_SMTH_FCTR].cfg_param = (uint32_t)(scs->scs_as_rx_time_smth_fctr);
	rpt[SCS_AS_TX_TIME_SMTH_FCTR].cfg_param = (uint32_t)(scs->scs_as_tx_time_smth_fctr);
	rpt[SCS_CCA_IDLE_SMTH_FCTR].cfg_param = (uint32_t)(scs->scs_cca_idle_smth_fctr);
	rpt[SCS_CCA_THRESHOD_TYPE].cfg_param = (uint32_t)(scs->scs_cca_threshold_type);

	for (i = 0; i < SCS_MAX_TXTIME_COMP_INDEX; i++) {
		rpt[SCS_TX_TIME_COMPENSTATION_START+i].cfg_param = tx_time_compenstation[i];
	}
	for (i = 0; i < SCS_MAX_RXTIME_COMP_INDEX; i++) {
		rpt[SCS_RX_TIME_COMPENSTATION_START+i].cfg_param = rx_time_compenstation[i];
	}
	for (i = 0; i < SCS_MAX_TDLSTIME_COMP_INDEX; i++) {
		rpt[SCS_TDLS_TIME_COMPENSTATION_START+i].cfg_param = tdls_time_compenstation[i];
	}

	if (copy_to_user(req->is_data, rpt, MIN(req->is_data_len, len))) {
		SCSDBG(SCSLOG_NOTICE, "copy_to_user data failed with op=0x%x\n", req->is_op);
		retval = -EIO;
		goto ready_to_return;
	}

ready_to_return:
	if (rpt != NULL) {
		kfree(rpt);
		rpt = NULL;
	}
	return retval;
}

/* WAR for bug16636, supposed to be removed after cca threshold re-tuned */
void ieee80211_scs_adjust_cca_threshold(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;
	uint32_t value = IEEE80211_SCS_SET_CCA_THRESHOLD_TYPE << IEEE80211_SCS_COMMAND_S;

	if (ieee80211_get_type_of_neighborhood(ic) == IEEE80211_NEIGHBORHOOD_TYPE_VERY_DENSE &&
				ic->ic_ver_hw == HARDWARE_REVISION_TOPAZ_A2) {
		IEEE80211_DPRINTF(ss->ss_vap, IEEE80211_MSG_SCAN, "%s: neighborhood is very dense, "
					"switch cca thresholds to less sensitive ones\n", __func__);
		ieee80211_param_to_qdrv(ss->ss_vap, IEEE80211_PARAM_CCA_FIXED, 0, NULL, 0);
		if (ieee80211_param_scs_set(ss->ss_vap->iv_dev, ss->ss_vap, value) == 0) {
			ieee80211_param_to_qdrv(ss->ss_vap, IEEE80211_PARAM_SCS, value, NULL, 0);
			ieee80211_param_to_qdrv(ss->ss_vap, IEEE80211_PARAM_CCA_FIXED, 1, NULL, 0);
		}
	}
}

static int
ieee80211_subioctl_scs(struct net_device *dev, struct ieee80211req_scs __user* ps)
{
	int retval = 0;
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	struct ieee80211req_scs req;
	uint32_t reason = IEEE80211REQ_SCS_RESULT_OK;

	if (!ps) {
		SCSDBG(SCSLOG_NOTICE, "%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}

	if (copy_from_user(&req, ps, sizeof(struct ieee80211req_scs))) {
		SCSDBG(SCSLOG_NOTICE, "%s: copy_from_user failed\n", __FUNCTION__);
		return -EIO;
	}

	if ((req.is_op & IEEE80211REQ_SCS_FLAG_GET) && (!req.is_data)) {
		SCSDBG(SCSLOG_NOTICE, "%s: NULL pointer for GET operation\n", __FUNCTION__);
		return -EFAULT;
	}

	if (!req.is_status) {
		SCSDBG(SCSLOG_NOTICE, "%s: NULL pointer for reason field\n", __FUNCTION__);
		return -EFAULT;
	}

	SCSDBG(SCSLOG_INFO, "%s: op=0x%x\n", __FUNCTION__, req.is_op);
	switch (req.is_op) {
	case IEEE80211REQ_SCS_GET_CURRCHAN_RPT:
		retval = ieee80211_scs_get_currchan_rpt(ic, &req, &reason);
		break;
	case IEEE80211REQ_SCS_GET_RANKING_RPT:
		retval = ieee80211_scs_get_ranking_rpt(ic, &req, &reason);
		break;
	case IEEE80211REQ_SCS_GET_INIT_RANKING_RPT:
		retval = ieee80211_scs_get_init_ranking_rpt(ic, &req, &reason);
		break;
	case IEEE80211REQ_SCS_GET_PARAM_RPT:
		retval = ieee80211_scs_get_param_rpt(ic, &req, &reason);
		break;
	case IEEE80211REQ_SCS_GET_SCORE_RPT:
		retval = ieee80211_scs_get_score_rpt(ic, &req, &reason);
		break;
	default:
		SCSDBG(SCSLOG_NOTICE, "unknown ioctl op=0x%x\n", req.is_op);
		return -EINVAL;
	}

	if (copy_to_user(req.is_status, &reason, sizeof(*req.is_status))) {
		SCSDBG(SCSLOG_NOTICE, "copy_to_user reason failed with op=0x%x\n", req.is_op);
		return -EIO;
	}

	return retval;
}

#endif /* QSCS_ENABLED */

struct ap_scan_iter {
	struct ieee80211vap *vap;
	char *current_env;
	char *end_buf;
	int32_t	ap_counts;
	int mode;
};

static int
ieee80211_subioctl_wait_scan_complete(struct net_device *dev, char __user* p_timeout)
{
	int retval = 0;
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	uint32_t timeout;

	if (copy_from_user(&timeout, p_timeout, sizeof(timeout))) {
		return -EIO;
	};

	if (((ic->ic_flags & IEEE80211_F_SCAN) == 0)
#ifdef QTN_BG_SCAN
			&& ((ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0)
#endif /* QTN_BG_SCAN */
			) {
		return -1;
	}

	retval = wait_event_interruptible_timeout(ic->ic_scan_comp,
			(((ic->ic_flags & IEEE80211_F_SCAN) == 0)
#ifdef QTN_BG_SCAN
					&& ((ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0)
#endif /* QTN_BG_SCAN */
					), timeout * HZ);

	return retval;
}

static uint32_t
maxrate(const struct ieee80211_scan_entry *se)
{
	int j, chan_mode = 0;
	uint32_t max = 0;
	uint8_t sgi = 0;
	int k, r;
	u_int16_t mask;
	struct ieee80211_ie_htcap *htcap;
	struct ieee80211_ie_vhtcap *vhtcap;

	htcap = (struct ieee80211_ie_htcap *)se->se_htcap_ie;
	vhtcap = (struct ieee80211_ie_vhtcap *)se->se_vhtcap_ie;
	if (vhtcap) {
		u_int16_t mcsmap = 0;
		r = 0;
		/* 80+80 or 160 Mhz */
		if (IEEE80211_VHTCAP_GET_CHANWIDTH(vhtcap)) {
			chan_mode = 1;
			sgi = IEEE80211_VHTCAP_GET_SGI_160MHZ(vhtcap);
		} else {
			chan_mode = 0;
			sgi = IEEE80211_VHTCAP_GET_SGI_80MHZ(vhtcap);
		}
		mask = 0xc000;
		mcsmap = (u_int16_t)IEEE80211_VHTCAP_GET_TX_MCS_NSS(vhtcap);
		for (k = 8; k > 0; k--) {
			if ((mcsmap & mask) != mask) {
				uint32_t rate = 0;
				int val = ((mcsmap & mask)>>((k-1) * 2));
				r = (val == 2) ? 9: (val == 1) ? 8 : 7;
				rate = ((uint32_t)ieee80211_mcs2rate(r, chan_mode, sgi, 1) * (1000000 / 2)) * k;
				if (max < rate)
					max = rate;
				break;
			}
			mask = mask >> 2;
		}
		return max;
	} else if (htcap) {
		r = 0;
		if (htcap->hc_cap[0] & IEEE80211_HTCAP_C_CHWIDTH40) {
			chan_mode = 1;
			sgi = htcap->hc_cap[0] & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;
		} else {
			sgi = htcap->hc_cap[0] & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
		}
		for (j = IEEE80211_HT_MCSSET_20_40_NSS1; j <= IEEE80211_HT_MCSSET_20_40_NSS4; j++) {
			mask = 1;
			for (k = 0; k < 8; k++, r++) {
				if (htcap->hc_mcsset[j] & mask) {
					/* Copy HT rates */
					int rate = ieee80211_mcs2rate(r, chan_mode, sgi, 0) * (1000000 / 2);
					if (max < rate) max = rate;
				}
				mask = mask << 1;
			}
		}
		return max;
	}

	for (j = 0; j < se->se_rates[1]; j++) {
		int r = se->se_rates[2 + j] & IEEE80211_RATE_VAL;
		if (r != 0) {
			r = r * (1000000 / 2);
			if (max < r) max = r;
		}
	}
	for (j = 0; j < se->se_xrates[1]; j++) {
		int r = se->se_xrates[2+j] & IEEE80211_RATE_VAL;
		if (r != 0) {
			r = r * (1000000 / 2);
			if (max < r) max = r;
		}
	}
	return max;
}

static int
push_scan_results(void *arg, const struct ieee80211_scan_entry *se)
{
	struct ap_scan_iter			*piter = (struct ap_scan_iter*)arg;
	struct ieee80211_per_ap_scan_result	*pap;

	pap = (struct ieee80211_per_ap_scan_result *)piter->current_env;

	if (piter->current_env >= piter->end_buf)
		return E2BIG;

	/* check length, set macaddr, ssid, channel, rssi, flags,htcap*/
	if (piter->current_env + sizeof(*pap) >= piter->end_buf)
		return E2BIG;

	/* ap mac addr */
	if (piter->vap->iv_opmode == IEEE80211_M_HOSTAP)
		IEEE80211_ADDR_COPY(pap->ap_addr_mac, se->se_macaddr);
	else
		IEEE80211_ADDR_COPY(pap->ap_addr_mac, se->se_bssid);

	/* ssid */
	memset(pap->ap_name_ssid, 0, sizeof(pap->ap_name_ssid));
	memcpy(pap->ap_name_ssid, se->se_ssid + 2, se->se_ssid[1]);

	/* channel ieee */
	pap->ap_channel_ieee = se->se_chan->ic_ieee;

	/* max bandwidth */
	pap->ap_max_bw = ieee80211_get_max_ap_bw(se);

	/* rssi */
	pap->ap_rssi = se->se_rssi;

	/* flags:privacy */
	pap->ap_flags = !!(se->se_capinfo & IEEE80211_CAPINFO_PRIVACY);

	/* htcap available */
	pap->ap_htcap = (se->se_htcap_ie != NULL);

	/* vhtcap available */
	pap->ap_vhtcap = (se->se_vhtcap_ie != NULL);

	pap->ap_qhop_role = se->se_ext_role;

	piter->current_env += sizeof(*pap);
	pap->ap_bestrate = maxrate(se);

	/* check length, copy wpa_ie, wsc_ie and rsn_ie to buffer if exist */
	pap->ap_num_genies = 0;
	if (se->se_rsn_ie != NULL) {
		if (piter->current_env + se->se_rsn_ie[1] + 2 >= piter->end_buf)
			return E2BIG;
		memcpy(piter->current_env, se->se_rsn_ie, se->se_rsn_ie[1] + 2);
		piter->current_env += se->se_rsn_ie[1] + 2;
		pap->ap_num_genies++;
	}

	if (se->se_wpa_ie != NULL) {
		if (piter->current_env + se->se_wpa_ie[1] + 2 >= piter->end_buf)
			return E2BIG;
		memcpy(piter->current_env, se->se_wpa_ie, se->se_wpa_ie[1] + 2);
		piter->current_env += se->se_wpa_ie[1] + 2;
		pap->ap_num_genies++;
	}

	if (se->se_wsc_ie != NULL) {
		if (piter->current_env + se->se_wsc_ie[1] + 2 >= piter->end_buf)
			return E2BIG;
		memcpy(piter->current_env, se->se_wsc_ie, se->se_wsc_ie[1] + 2);
		piter->current_env += se->se_wsc_ie[1] + 2;
		pap->ap_num_genies++;
	}

	pap->ap_beacon_intval = se->se_intval;
	pap->ap_dtim_intval = se->se_dtimperiod;
	pap->ap_is_ess = se->se_capinfo & IEEE80211_CAPINFO_ESS;

	/* keep address 4-byte aligned*/
	piter->current_env = (char *)(((int)piter->current_env + 3) & (~3));

	piter->ap_counts++;
	return 0;
}

static inline int
ieee80211_set_threshold_of_neighborhood_type(struct ieee80211com *ic, uint32_t type, uint32_t value)
{
	if (IEEE80211_NEIGHBORHOOD_TYPE_SPARSE == type)
		ic->ic_neighbor_cnt_sparse = value;
	else if (IEEE80211_NEIGHBORHOOD_TYPE_DENSE == type)
		ic->ic_neighbor_cnt_dense = value;
	else
		return 1;

	return 0;
}

static inline uint32_t
ieee80211_get_threshold_of_neighborhood_type(struct ieee80211com *ic, uint32_t type)
{
	if (IEEE80211_NEIGHBORHOOD_TYPE_SPARSE == type)
		return ic->ic_neighbor_cnt_sparse;
	else if (IEEE80211_NEIGHBORHOOD_TYPE_DENSE == type)
		return ic->ic_neighbor_cnt_dense;

	return 0;
}

static int
ieee80211_subioctl_ap_scan_results(struct net_device *dev, char __user* data, int32_t len)
{
	int retval;
	int i, r, chan_mode = 0;;
	uint8_t sgi = 0;
	char *kdata;
	struct ieee80211vap			*vap = netdev_priv(dev);
	struct ieee80211com			*ic  = vap->iv_ic;
	struct ieee80211_rateset		*rs;
	struct ap_scan_iter			iter;
	struct ieee80211_general_ap_scan_result *ge_ap_scan_result;

	kdata = kmalloc(len, GFP_KERNEL);
	if (NULL == kdata)
		return -ENOMEM;

	/* get bit rates from ic->ic_sup_rates[ic->ic_des_mode] */
	ge_ap_scan_result = (struct ieee80211_general_ap_scan_result *)kdata;
	ge_ap_scan_result->num_ap_results = 0;
	rs = &ic->ic_sup_rates[ic->ic_des_mode];
	ge_ap_scan_result->num_bitrates = rs->rs_nrates;

	if (ge_ap_scan_result->num_bitrates > MIN(IEEE80211_RATE_MAXSIZE, AP_SCAN_MAX_NUM_RATES)) {
		ge_ap_scan_result->num_bitrates = MIN(IEEE80211_RATE_MAXSIZE, AP_SCAN_MAX_NUM_RATES);
	}

	if (vap->iv_ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) {
		chan_mode = 1;
		sgi = vap->iv_ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;
	} else {
		sgi = vap->iv_ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
	}
	for (i = 0; i < ge_ap_scan_result->num_bitrates; i++) {
		r = rs->rs_rates[i] & IEEE80211_RATE_VAL;

		/* Skip legacy rates */
		if(i >= (rs->rs_legacy_nrates))
		{
			r = ieee80211_mcs2rate(r, chan_mode, sgi, 0);
		}
		ge_ap_scan_result->bitrates[i] = (r * 1000000) / 2;
	}

	/* initialize ap_scan_iter */
	iter.ap_counts = 0;
	iter.current_env = kdata + sizeof(*ge_ap_scan_result);
	iter.end_buf = kdata + len;
	iter.vap = vap;

	/*
	 * iterate scan results to push per-ap data into buffer
	 *
	 * Don't need do WPA/RSN sort any more since the original scan list
	 * has been sorted.
	 */
	retval = ieee80211_scan_iterate(ic, push_scan_results, &iter);

	ge_ap_scan_result->num_ap_results = iter.ap_counts;
	if (copy_to_user(data, kdata, iter.current_env - kdata))
		retval = -EIO;

	if (retval > 0)
		retval = -retval;

	kfree(kdata);
	return retval;
}

static int
ieee80211_ocac_clean_stats(struct ieee80211com *ic, int clean_level)
{
	switch (clean_level) {
	case IEEE80211_OCAC_CLEAN_STATS_STOP:
		ic->ic_ocac.ocac_counts.clean_stats_stop++;
		break;
	case IEEE80211_OCAC_CLEAN_STATS_START:
		ic->ic_ocac.ocac_counts.clean_stats_start++;
		break;
	case IEEE80211_OCAC_CLEAN_STATS_RESET:
		ic->ic_ocac.ocac_counts.clean_stats_reset++;
		break;
	}

	if (clean_level <= IEEE80211_OCAC_CLEAN_STATS_RESET) {
		ic->ic_ocac.ocac_accum_duration_secs = 0;
		ic->ic_ocac.ocac_accum_cac_time_ms = 0;
	}

	return 0;
}

static struct ieee80211_channel *
ieee80211_ocac_pick_dfs_channel(struct ieee80211com *ic, int chan_dfs)
{
	int chan_ieee;
	struct ieee80211_channel *chan = NULL;

	ic->ic_ocac.ocac_counts.pick_offchan++;
	if (chan_dfs) {
		chan_ieee = chan_dfs;
	} else {
		/* Pick a DFS channel on which OCAC will be performed */
		chan_ieee = ieee80211_scs_pick_channel(ic,
				(IEEE80211_SCS_PICK_NOT_AVAILABLE_DFS_ONLY | IEEE80211_SCS_PICK_ANYWAY),
				IEEE80211_SCS_NA_CC);
	}
	chan = ieee80211_find_channel_by_ieee(ic, chan_ieee);

	/*
	 * Select a DFS channel for OCAC, only if CAC is not already done;
	 * Initial CAC might have cleared the channel already;
	 */
	if ((chan && (ic->ic_dfs_chans_available_for_cac(ic, chan) == false))) {
		ic->ic_ocac.ocac_counts.invalid_offchan++;
		return NULL;
	}

	return chan;
}

static void ieee80211_ocac_set_beacon_interval(struct ieee80211com *ic)
{
	if (ic->ic_lintval != ic->ic_ocac.ocac_cfg.ocac_params.beacon_interval &&
			ic->ic_lintval == ic->ic_lintval_backup) {
		ieee80211_beacon_interval_set(ic,
				ic->ic_ocac.ocac_cfg.ocac_params.beacon_interval);
		ic->ic_ocac.ocac_bcn_intval_set = 1;
		ic->ic_ocac.ocac_counts.set_bcn_intval++;
		OCACDBG(OCACLOG_NOTICE, "set beacon interval to %u\n",
				ic->ic_ocac.ocac_cfg.ocac_params.beacon_interval);
	}
}

static void ieee80211_ocac_restore_beacon_interval(struct ieee80211com *ic)
{
	if (ic->ic_lintval == ic->ic_ocac.ocac_cfg.ocac_params.beacon_interval &&
			ic->ic_lintval != ic->ic_lintval_backup &&
			ic->ic_ocac.ocac_bcn_intval_set) {
		ieee80211_beacon_interval_set(ic, ic->ic_lintval_backup);
		ic->ic_ocac.ocac_bcn_intval_set = 0;
		ic->ic_ocac.ocac_counts.restore_bcn_intval++;
		OCACDBG(OCACLOG_NOTICE, "restore beacon interval to %u\n",
				ic->ic_lintval_backup);
	}
}

static void ieee80211_ocac_trigger_channel_switch(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *)arg;
	struct ieee80211_channel *chan = ic->ic_csa_chan;

	/* don't run CAC on new channel */
	chan->ic_flags |= IEEE80211_CHAN_DFS_OCAC_DONE;
	ieee80211_finish_csa((unsigned long)ic);
	chan->ic_flags &= ~IEEE80211_CHAN_DFS_OCAC_DONE;

	ieee80211_ocac_restore_beacon_interval(ic);

	return;
}

static int
ieee80211_ocac_check_radar_by_chan_ieee(struct ieee80211com *ic, int chan_ieee)
{
	struct ieee80211_channel *chan;

	if (chan_ieee) {
		chan = ieee80211_find_channel_by_ieee(ic, chan_ieee);
		if (chan && (chan->ic_flags & IEEE80211_CHAN_RADAR)) {
			ic->ic_ocac.ocac_counts.csw_fail_radar++;
			OCACDBG(OCACLOG_NOTICE, "switch channel failed "
					"because radar detected on channel: %u\n",
					chan_ieee);
			return -1;
		}
	}

	return 0;
}

/*
 * Change channel to DFS channel after off-channel CAC is completed
 * return:
 * -1: channel switch failed
 * 0 : channel switch succeeded.
 */
static int
ieee80211_ocac_change_channel(struct ieee80211com *ic, struct ieee80211_channel *newchan)
{
	int ret;
	uint32_t cur_cca_intf = 0;
	uint32_t new_cca_intf = 0;
	int chan2_ieee;

	struct ap_state *as;

	if (ic->ic_ocac.ocac_cfg.ocac_report_only) {
		ic->ic_ocac.ocac_counts.csw_rpt_only++;
		OCACDBG(OCACLOG_NOTICE, "didn't switch channel for report only\n");
		return -1;
	}

	as = ic->ic_scan->ss_scs_priv;
	if (as->as_cca_intf[ic->ic_curchan->ic_ieee] != SCS_CCA_INTF_INVALID) {
		cur_cca_intf = 100 * as->as_cca_intf[ic->ic_curchan->ic_ieee]
						     / IEEE80211_SCS_CCA_INTF_SCALE;
	}
	if (as->as_cca_intf[newchan->ic_ieee] != SCS_CCA_INTF_INVALID) {
		new_cca_intf = 100 * as->as_cca_intf[newchan->ic_ieee]
						     / IEEE80211_SCS_CCA_INTF_SCALE;
	}

	if ((new_cca_intf > ic->ic_ocac.ocac_cfg.ocac_params.thresh_cca_intf)
			&& (new_cca_intf > cur_cca_intf)) {
		ic->ic_ocac.ocac_counts.csw_fail_intf++;
		OCACDBG(OCACLOG_NOTICE, "can't switch to channel %u, "
				"cur_intf: %u, new_intf: %u\n",
				newchan->ic_ieee, cur_cca_intf, new_cca_intf);
		return -1;
	}

	chan2_ieee = ieee80211_find_sec_chan(newchan);
	if (ieee80211_ocac_check_radar_by_chan_ieee(ic, chan2_ieee)) {
		return -1;
	}
	if (ieee80211_get_bw(ic) >= BW_HT80) {
		chan2_ieee = ieee80211_find_sec40u_chan(newchan);
		if (ieee80211_ocac_check_radar_by_chan_ieee(ic, chan2_ieee)) {
			return -1;
		}
		chan2_ieee = ieee80211_find_sec40l_chan(newchan);
		if (ieee80211_ocac_check_radar_by_chan_ieee(ic, chan2_ieee)) {
			return -1;
		}
	}

	ret = ieee80211_enter_csa(ic, newchan, ieee80211_ocac_trigger_channel_switch,
			IEEE80211_CSW_REASON_OCAC,
			IEEE80211_DEFAULT_CHANCHANGE_TBTT_COUNT,
			IEEE80211_CSA_MUST_STOP_TX,
			IEEE80211_CSA_F_BEACON | IEEE80211_CSA_F_ACTION);
	if (ret == 0) {
		ic->ic_ocac.ocac_counts.csw_success++;
		DFS_S_DBG_QEVT(ic2dev(ic), "DFS_s_radio: CAC completed and start working on "
			       "channel %u\n", newchan->ic_ieee);
	} else {
		ic->ic_ocac.ocac_counts.csw_fail_csa++;
		OCACDBG(OCACLOG_NOTICE, "switch to channel %u failed\n", newchan->ic_ieee);
	}

	return ret;
}

/*
 *  * Stop off-channel CAC
 *   */
static int
ieee80211_wireless_stop_ocac(struct ieee80211vap *vap)
{
        struct ieee80211com *ic = vap->iv_ic;

        if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
                printk("DFS seamless radio is only supported on APs");
                return -1;
        }

        del_timer(&ic->ic_ocac.ocac_timer);

        if (ic->ic_set_ocac(vap, NULL)) {
                return -1;
        }

        if (ic->ic_ocac.ocac_running) {
                ic->ic_ocac.ocac_running = 0;
                ieee80211_pm_queue_work(ic);
                ic->ic_ocac.ocac_counts.pm_update++;
        }
        ieee80211_ocac_restore_beacon_interval(ic);
        ieee80211_ocac_clean_stats(ic, IEEE80211_OCAC_CLEAN_STATS_STOP);

        printk("DFS seamless radio is stopped\n");

        return 0;
}

static __inline__ uint32_t
ieee80211_ocac_get_param_duration(struct ieee80211com *ic, struct ieee80211_channel *dfs_chan)
{
	return (ieee80211_is_on_weather_channel(ic, dfs_chan) ?
			ic->ic_ocac.ocac_cfg.ocac_params.wea_duration_secs :
			ic->ic_ocac.ocac_cfg.ocac_params.duration_secs);
}

static __inline__ uint32_t
ieee80211_ocac_get_param_cac_time(struct ieee80211com *ic, struct ieee80211_channel *dfs_chan)
{
	return (ieee80211_is_on_weather_channel(ic, dfs_chan) ?
			ic->ic_ocac.ocac_cfg.ocac_params.wea_cac_time_secs :
			ic->ic_ocac.ocac_cfg.ocac_params.cac_time_secs);
}

static void
ieee80211_ocac_timer_func(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *)arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	struct ieee80211_channel *dfs_chan = NULL;
	struct ieee80211vap *tmp_vap;
	uint32_t num_vap = 0;
	uint32_t vap_idx;
	uint32_t radar_detected;
	uint32_t reset_duration = 0;
	uint8_t prev_ocac_running;

	if (vap->iv_state != IEEE80211_S_RUN) {
		OCACDBG(OCACLOG_NOTICE, "DFS seamless radio is pending"
				" because the AP is not in running state\n");
		ic->ic_ocac.ocac_counts.ap_not_running++;
		goto set_ocac;
	}

	if (ic->ic_flags & IEEE80211_F_SCAN) {
		OCACDBG(OCACLOG_NOTICE, "DFS seamless radio is pending"
				" because channel scanning is in progress\n");
		ic->ic_ocac.ocac_counts.chan_scanning++;
		goto set_ocac;
	}

	if ((ic->ic_curchan != IEEE80211_CHAN_ANYC) &&
			(ic->ic_curchan->ic_flags & IEEE80211_CHAN_DFS)) {
		OCACDBG(OCACLOG_NOTICE, "DFS seamless radio is pending"
				" because the current channel is a DFS channel\n");
		ic->ic_ocac.ocac_counts.curchan_dfs++;
		ieee80211_ocac_restore_beacon_interval(ic);
		reset_duration = 1;
		goto set_ocac;
	}

	TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
		if (tmp_vap->iv_opmode == IEEE80211_M_WDS &&
				!IEEE80211_VAP_WDS_IS_MBS(tmp_vap)) {
			OCACDBG(OCACLOG_NOTICE, "DFS seamless radio is pending"
					" because a WDS interface exists\n");
			ic->ic_ocac.ocac_counts.wds_exist++;
			ieee80211_ocac_restore_beacon_interval(ic);
			reset_duration = 1;
			goto set_ocac;
		}
		if (tmp_vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;
		if ((tmp_vap->iv_state != IEEE80211_S_RUN)
				&& (tmp_vap->iv_state != IEEE80211_S_SCAN))
			continue;
		num_vap++;
		vap_idx = ic->ic_get_vap_idx(tmp_vap);
		if (vap_idx > 1) {
			OCACDBG(OCACLOG_NOTICE, "DFS seamless radio is pending because"
					" unsupported MBSSID(wifi%d) is configured\n", vap_idx);
			/* Support OCAC with two VAPs only - wifi0 and wifi1 */
			ic->ic_ocac.ocac_counts.unsupported_mbssid++;
			ieee80211_ocac_restore_beacon_interval(ic);
			reset_duration = 1;
			goto set_ocac;
		}
	}

	if (num_vap > 1 && ic->ic_beaconing_scheme == QTN_BEACONING_SCHEME_0) {
		OCACDBG(OCACLOG_NOTICE, "DFS seamless radio with two MBSSIDs is pending because"
				" of the beaconing scheme initiated when booting up\n");
		OCACDBG(OCACLOG_NOTICE, "To enable DFS seamless radio with two MBSSIDs, please"
				" save DFS seamless radio configuration and reboot the board\n");
		/* If two VAPs are configured, the beaconing scheme should be shceme 1 */
		ic->ic_ocac.ocac_counts.beacon_scheme0++;
		ieee80211_ocac_restore_beacon_interval(ic);
		reset_duration = 1;
		goto set_ocac;
	}

	if (ic->ic_ocac.ocac_chan == NULL ||
			ic->ic_ocac.ocac_repick_dfs_chan) {
		/* initial off channel selection */
		ic->ic_ocac.ocac_counts.init_offchan++;
		ic->ic_ocac.ocac_chan = ieee80211_ocac_pick_dfs_channel(ic,
				ic->ic_ocac.ocac_cfg.ocac_chan_ieee);
		if (ic->ic_ocac.ocac_chan == NULL) {
			OCACDBG(OCACLOG_NOTICE, "Init DFS channel selection (%d) error\n",
						ic->ic_ocac.ocac_cfg.ocac_chan_ieee);
			ic->ic_ocac.ocac_counts.no_offchan++;
			reset_duration = 1;
			goto set_ocac;
		}
		ic->ic_ocac.ocac_repick_dfs_chan = 0;
		ieee80211_ocac_clean_stats(ic, IEEE80211_OCAC_CLEAN_STATS_RESET);
		DFS_S_DBG_QEVT(ic2dev(ic), "DFS_s_radio: CAC started for channel %u\n",
				ic->ic_ocac.ocac_chan->ic_ieee);
		OCACDBG(OCACLOG_NOTICE, "CAC duration: %u secs, minimal valid CAC time: %u secs\n",
				ieee80211_ocac_get_param_duration(ic, ic->ic_ocac.ocac_chan),
				ieee80211_ocac_get_param_cac_time(ic, ic->ic_ocac.ocac_chan));
	}

	ieee80211_ocac_set_beacon_interval(ic);

	radar_detected = ic->ic_ocac.ocac_chan->ic_flags & IEEE80211_CHAN_RADAR;
	if (radar_detected) {
		ic->ic_ocac.ocac_counts.radar_detected++;
		OCACDBG(OCACLOG_NOTICE, "Radar was detected on channel %u\n",
				ic->ic_ocac.ocac_chan->ic_ieee);
		if (ic->ic_ocac.ocac_cfg.ocac_chan_ieee) {
			OCACDBG(OCACLOG_NOTICE, "CAC stops when radar detected for test mode\n");
			goto stop_ocac;
		}
		chan = ieee80211_ocac_pick_dfs_channel(ic,
				ic->ic_ocac.ocac_cfg.ocac_chan_ieee);
		if (chan && ic->ic_ocac.ocac_chan != chan) {
			ic->ic_ocac.ocac_chan = chan;
			DFS_S_DBG_QEVT(ic2dev(ic), "DFS_s_radio: CAC restarted for channel %u\n",
					     chan->ic_ieee);
			OCACDBG(OCACLOG_NOTICE, "CAC duration: %u secs, minimal valid CAC time: %u secs\n",
					ieee80211_ocac_get_param_duration(ic, ic->ic_ocac.ocac_chan),
					ieee80211_ocac_get_param_cac_time(ic, ic->ic_ocac.ocac_chan));
		}
		ieee80211_ocac_clean_stats(ic, IEEE80211_OCAC_CLEAN_STATS_RESET);
		if (chan == NULL) {
			reset_duration = 1;
			goto set_ocac;
		}
	} else {
		/* off-channel CAC time out for this DFS channel */
		if (ic->ic_ocac.ocac_accum_duration_secs >
				ieee80211_ocac_get_param_duration(ic, ic->ic_ocac.ocac_chan)) {
			if ((ic->ic_ocac.ocac_accum_cac_time_ms / 1000) >
					ieee80211_ocac_get_param_cac_time(ic, ic->ic_ocac.ocac_chan)) {
				ic->ic_ocac.ocac_counts.cac_success++;
				OCACDBG(OCACLOG_NOTICE, "CAC succeed and no radar\n");
				if (ieee80211_ocac_change_channel(ic, ic->ic_ocac.ocac_chan)) {
					chan = ieee80211_ocac_pick_dfs_channel(ic,
							ic->ic_ocac.ocac_cfg.ocac_chan_ieee);
					if (chan && ic->ic_ocac.ocac_chan != chan) {
						ic->ic_ocac.ocac_chan = chan;
						DFS_S_DBG_QEVT(ic2dev(ic), "DFS_s_radio: CAC "
							       "restarted for channel %u\n",
								chan->ic_ieee);
						OCACDBG(OCACLOG_NOTICE, "CAC duration: %u secs,"
								" minimal valid CAC time: %u secs\n",
								ieee80211_ocac_get_param_duration(ic,
										ic->ic_ocac.ocac_chan),
								ieee80211_ocac_get_param_cac_time(ic,
										ic->ic_ocac.ocac_chan));
					}
				} else if (ic->ic_ocac.ocac_cfg.ocac_chan_ieee) {
					OCACDBG(OCACLOG_NOTICE, "CAC stops after switching"
							" to dfs channel for test mode\n");
					goto stop_ocac;
				} else {
					ic->ic_ocac.ocac_repick_dfs_chan = 1;
				}
			} else {
				ic->ic_ocac.ocac_counts.cac_failed++;
				OCACDBG(OCACLOG_NOTICE, "CAC failed and restarted, CAC accumulated %u, CAC desired %u\n",
						ic->ic_ocac.ocac_accum_cac_time_ms / 1000,
						ieee80211_ocac_get_param_cac_time(ic, ic->ic_ocac.ocac_chan));
			}
			ieee80211_ocac_clean_stats(ic, IEEE80211_OCAC_CLEAN_STATS_RESET);
		}
	}

	dfs_chan = ic->ic_ocac.ocac_chan;

set_ocac:
	ic->ic_set_ocac(vap, dfs_chan);

	prev_ocac_running = ic->ic_ocac.ocac_running;
	ic->ic_ocac.ocac_running = dfs_chan ? 1 : 0;

	if (prev_ocac_running != ic->ic_ocac.ocac_running) {
		ieee80211_pm_queue_work(ic);
		ic->ic_ocac.ocac_counts.pm_update++;
	}

	if (reset_duration) {
		ieee80211_ocac_clean_stats(ic, IEEE80211_OCAC_CLEAN_STATS_RESET);
	} else {
		ic->ic_ocac.ocac_accum_duration_secs +=
				ic->ic_ocac.ocac_cfg.ocac_params.timer_interval;
	}

	mod_timer(&ic->ic_ocac.ocac_timer,
			jiffies + (ic->ic_ocac.ocac_cfg.ocac_params.timer_interval * HZ));

	return;

stop_ocac:
	ieee80211_wireless_stop_ocac(vap);
	ic->ic_ocac.ocac_cfg.ocac_enable = 0;
	ic->ic_beacon_update(vap);
}

struct ieee80211_ocac_params_dflt {
	char			region[4];
	struct ieee80211_ocac_params	dflt_params;
} ocac_params_dflt[] = {
	{
		"EU",
		{
			1,	/* traffic control */
			23,	/* secure dwell */
			40,	/* dwell time */
			720,	/* duration */
			240,	/* cac time */
			20,	/* dwell time for weather channel */
			11520,	/* duration for weather channel */
			1920,	/* cac time for weather channel */
			90,	/* thresh fat */
			30,	/* thresh traffic */
			10,	/* thresh fat dec */
			20,	/* thresh cca intf */
			10,	/* offset txhalt */
			7,	/* offset offchan */
			2,	/* timer interval */
			100	/* beacon interval */
		}
	},
	{
		"US",
		{
			0,	/* traffic control */
			23,	/* secure dwell */
			80,	/* dwell time */
			70,	/* duration */
			50,	/* cac time */
			80,	/* dwell time for weather channel*/
			70,	/* duration for weather channel */
			50,	/* cac time for weather channel */
			75,	/* thresh fat */
			3,	/* thresh traffic */
			10,	/* thresh fat dec */
			20,	/* thresh cca intf */
			5,	/* offset txhalt */
			5,	/* offset offchan */
			2,	/* timer interval */
			100	/* beacon interval */
		}
	}
};

static int
ieee80211_ocac_is_region_supported(const char *region)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ocac_params_dflt); i++) {
		if (!strcasecmp(region, ocac_params_dflt[i].region)) {
			return 1;
		}
	}

	return 0;
}

void
ieee80211_ocac_update_params(struct ieee80211com *ic, const char *region)
{
	int i;

	if (ic->ic_ocac.ocac_cfg.ocac_enable) {
		return;
	}

	if (!strcasecmp(ic->ic_ocac.ocac_cfg.ocac_region, region)) {
		return;
	}

	strncpy(ic->ic_ocac.ocac_cfg.ocac_region, region,
			sizeof(ic->ic_ocac.ocac_cfg.ocac_region));

	for (i = 0; i < ARRAY_SIZE(ocac_params_dflt); i++) {
		if (!strcasecmp(ic->ic_ocac.ocac_cfg.ocac_region, ocac_params_dflt[i].region)) {
			memcpy(&ic->ic_ocac.ocac_cfg.ocac_params, &ocac_params_dflt[i].dflt_params,
					sizeof(ic->ic_ocac.ocac_cfg.ocac_params));
			printk("DFS_s_radio: parameters updated, region: %s\n",
					ic->ic_ocac.ocac_cfg.ocac_region);
			break;
		}
	}
}
EXPORT_SYMBOL(ieee80211_ocac_update_params);

enum qtn_ocac_unsupported_reason {
	QTN_OCAC_REASON_AP_MODE = 1,
	QTN_OCAC_REASON_REGION,
	QTN_OCAC_REASON_WDS,
	QTN_OCAC_REASON_MBSSID,
	QTN_OCAC_REASON_NO_DFS_CHAN,
	QTN_OCAC_REASON_NO_NON_DFS_CHAN,
	QTN_OCAC_REASON_MAX = QTN_OCAC_REASON_NO_NON_DFS_CHAN
};

static char *qtn_ocac_reason_str[QTN_OCAC_REASON_MAX] = {
	"supported in AP mode only",
	"not supported for current region",
	"not supported in the case of WDS interface exist",
	"not supported in the case of unsupported MBSSID(except wifi0 and wifi1) is configured",
	"not supported because no DFS channel",
	"not supported because no non-DFS channel"
};

static unsigned int
ieee80211_wireless_is_ocac_unsupported(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *tmp_vap;
	struct ieee80211_channel *c;
	uint32_t num_dfs_chan = 0;
	uint32_t num_non_dfs_chan = 0;
	unsigned int ret = 0;
	int i;

	if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
		ret = QTN_OCAC_REASON_AP_MODE;
		goto done;
	}

	if (!ieee80211_ocac_is_region_supported(ic->ic_ocac.ocac_cfg.ocac_region)) {
		ret = QTN_OCAC_REASON_REGION;
		goto done;
	}

	TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
		if (tmp_vap->iv_opmode == IEEE80211_M_WDS &&
				!IEEE80211_VAP_WDS_IS_MBS(tmp_vap)) {
			ic->ic_ocac.ocac_counts.wds_exist++;
			ret = QTN_OCAC_REASON_WDS;
			goto done;
		}
		if (tmp_vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;
		if ((tmp_vap->iv_state != IEEE80211_S_RUN)
				&& (tmp_vap->iv_state != IEEE80211_S_SCAN))
			continue;
		if (ic->ic_get_vap_idx(tmp_vap) > 1) {
			/* Only support OCAC with two VAPs - wifi0 and wifi1 */
			ic->ic_ocac.ocac_counts.unsupported_mbssid++;
			ret = QTN_OCAC_REASON_MBSSID;
			goto done;
		}
	}

	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c == NULL || isclr(ic->ic_chan_active, c->ic_ieee))
			continue;
		if (c->ic_flags & IEEE80211_CHAN_DFS) {
			num_dfs_chan++;
		} else {
			num_non_dfs_chan++;
		}
	}
	if (num_dfs_chan == 0) {
		ret = QTN_OCAC_REASON_NO_DFS_CHAN;
		goto done;
	}
	if (num_non_dfs_chan == 0) {
		ret = QTN_OCAC_REASON_NO_NON_DFS_CHAN;
		goto done;
	}

done:
	return ret;
}

/*
 * Start off-channel CAC
 */
static int
ieee80211_wireless_start_ocac(struct ieee80211vap *vap, int chan_ieee)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan;
	unsigned int reason;

	reason = ieee80211_wireless_is_ocac_unsupported(vap);
	if (reason) {
		if (reason <= QTN_OCAC_REASON_MAX) {
			printk("DFS seamless radio is %s\n", qtn_ocac_reason_str[reason - 1]);
		}
		return -1;
	}

	if (chan_ieee) {
		chan = ieee80211_find_channel_by_ieee(ic, chan_ieee);
		if (chan == NULL || !(chan->ic_flags & IEEE80211_CHAN_DFS)) {
			DFS_S_DBG_QEVT(ic2dev(ic), "DFS_s_radio: channel %u is not a valid DFS channel\n",
					chan_ieee);
			return -1;
		} else if (chan->ic_flags & IEEE80211_CHAN_RADAR) {
			DFS_S_DBG_QEVT(ic2dev(ic), "DFS_s_radio: radar detected on channel %u\n",
					     chan_ieee);
			return -1;
		}
	}

	ic->ic_ocac.ocac_cfg.ocac_chan_ieee = chan_ieee;
	ic->ic_ocac.ocac_chan = NULL;	/* reset the dfs channel */

	ieee80211_ocac_clean_stats(ic, IEEE80211_OCAC_CLEAN_STATS_START);

	init_timer(&ic->ic_ocac.ocac_timer);
	ic->ic_ocac.ocac_timer.function = ieee80211_ocac_timer_func;
	ic->ic_ocac.ocac_timer.data = (unsigned long) vap;
	ic->ic_ocac.ocac_timer.expires = jiffies +
			ic->ic_ocac.ocac_cfg.ocac_timer_expire_init * HZ;
	add_timer(&ic->ic_ocac.ocac_timer);

	printk("Starting DFS seamless radio...\n");

	return 0;
}

static void
ieee80211_ocac_set_dump_counts(struct ieee80211com *ic, int value)
{
	struct ieee80211_ocac_counts *ocac_counts = &ic->ic_ocac.ocac_counts;

	if (value) {
		printk("DFS_s_radio counts:\n");
		printk("  ap_not_running:       %u\n", ocac_counts->ap_not_running);
		printk("  chan_scanning:        %u\n", ocac_counts->chan_scanning);
		printk("  curchan_dfs:          %u\n", ocac_counts->curchan_dfs);
		printk("  init_offchan:         %u\n", ocac_counts->init_offchan);
		printk("  no_offchan:           %u\n", ocac_counts->no_offchan);
		printk("  pick_offchan:         %u\n", ocac_counts->pick_offchan);
		printk("  invalid_offchan:      %u\n", ocac_counts->invalid_offchan);
		printk("  set_bcn_intval:       %u\n", ocac_counts->set_bcn_intval);
		printk("  restore_bcn_intval:   %u\n", ocac_counts->restore_bcn_intval);
		printk("  pm_update:            %u\n", ocac_counts->pm_update);
		printk("  unsupported_mbssid:   %u\n", ocac_counts->unsupported_mbssid);
		printk("  beacon_scheme0:       %u\n", ocac_counts->beacon_scheme0);
		printk("  wds_exist:            %u\n", ocac_counts->wds_exist);
		printk("  set_run:              %u\n", ocac_counts->set_run);
		printk("  set_pend:             %u\n", ocac_counts->set_pend);
		printk("  skip_set_run:         %u\n", ocac_counts->skip_set_run);
		printk("  skip_set_pend:        %u\n", ocac_counts->skip_set_pend);
		printk("  alloc_skb_error:      %u\n", ocac_counts->alloc_skb_error);
		printk("  set_frame_error:      %u\n", ocac_counts->set_frame_error);
		printk("  hostlink_err:         %u\n", ocac_counts->hostlink_err);
		printk("  hostlink_ok:          %u\n", ocac_counts->hostlink_ok);
		printk("  radar_detected:       %u\n", ocac_counts->radar_detected);
		printk("  cac_failed:           %u\n", ocac_counts->cac_failed);
		printk("  cac_success:          %u\n", ocac_counts->cac_success);
		printk("  csw_rpt_only:         %u\n", ocac_counts->csw_rpt_only);
		printk("  csw_fail_intf:        %u\n", ocac_counts->csw_fail_intf);
		printk("  csw_fail_radar:       %u\n", ocac_counts->csw_fail_radar);
		printk("  csw_fail_csa:         %u\n", ocac_counts->csw_fail_csa);
		printk("  csw_success:          %u\n", ocac_counts->csw_success);
		printk("  clean_stats_reset:    %u\n", ocac_counts->clean_stats_reset);
		printk("  clean_stats_start:    %u\n", ocac_counts->clean_stats_start);
		printk("  clean_stats_stop:     %u\n", ocac_counts->clean_stats_stop);
		printk("  tasklet_off_chan:     %u\n", ocac_counts->tasklet_off_chan);
		printk("  tasklet_data_chan:    %u\n", ocac_counts->tasklet_data_chan);
		printk("  intr_off_chan:        %u\n", ocac_counts->intr_off_chan);
		printk("  intr_data_chan:       %u\n", ocac_counts->intr_data_chan);
	} else {
		/* clear ocac counts */
		memset(ocac_counts, 0, sizeof(ic->ic_ocac.ocac_counts));
	}
}

static void
ieee80211_ocac_dump_tsflog(struct ieee80211com *ic)
{
	int i;
	int cur_index;
	int next_index;
	uint32_t time_sw_offchan;
	uint32_t time_sw_datachan;
	uint32_t time_on_offchan;
	uint32_t time_on_datachan;
	struct ieee80211_ocac_tsflog tsflog;

	IEEE80211_LOCK_BH(ic);
	memcpy(&tsflog, &ic->ic_ocac.ocac_tsflog, sizeof(struct ieee80211_ocac_tsflog));
	IEEE80211_UNLOCK_BH(ic);

	printk("  sw_offchan  on_offchan  sw_datachan  on_datachan\n");
	cur_index = tsflog.log_index;
	for (i = 0; i < QTN_OCAC_TSF_LOG_DEPTH; i++)
	{
		next_index = (cur_index + 1) % QTN_OCAC_TSF_LOG_DEPTH;

		time_sw_offchan = (uint32_t)(tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_OFF_CHAN_DONE] -
				tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_OFF_CHAN]);
		time_on_offchan = (uint32_t)(tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_DATA_CHAN] -
				tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_OFF_CHAN_DONE]);
		time_on_datachan = (uint32_t)(tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_OFF_CHAN] -
				tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_DATA_CHAN_DONE]);
		time_sw_datachan = (uint32_t)(tsflog.tsf_log[next_index][OCAC_TSF_LOG_GOTO_DATA_CHAN_DONE] -
				tsflog.tsf_log[cur_index][OCAC_TSF_LOG_GOTO_DATA_CHAN]);
		cur_index = next_index;
		printk("  %10u  %10u  %10u  %10u\n", time_sw_offchan, time_on_offchan,
				time_sw_datachan, time_on_datachan);
	}
}

static void
ieee80211_ocac_dump_cfg(struct ieee80211com *ic)
{
	printk("DFS_s_radio cfg:\n");
	printk("  region:            %s\n", ic->ic_ocac.ocac_cfg.ocac_region);
	printk("  started:           %u\n", ic->ic_ocac.ocac_cfg.ocac_enable);
	printk("  debug_level:       %u\n", ic->ic_ocac.ocac_cfg.ocac_debug_level);
	printk("  report_only:       %u\n", ic->ic_ocac.ocac_cfg.ocac_report_only);
	printk("  off_channel:       %u\n", ic->ic_ocac.ocac_cfg.ocac_chan_ieee);
	printk("  timer_expire_init: %u\n", ic->ic_ocac.ocac_cfg.ocac_timer_expire_init);
	printk("  secure_dwell_ms:   %u\n", ic->ic_ocac.ocac_cfg.ocac_params.secure_dwell_ms);
	printk("  dwell_time_ms:     %u\n", ic->ic_ocac.ocac_cfg.ocac_params.dwell_time_ms);
	printk("  duration_secs:     %u\n", ic->ic_ocac.ocac_cfg.ocac_params.duration_secs);
	printk("  cac_time_secs:     %u\n", ic->ic_ocac.ocac_cfg.ocac_params.cac_time_secs);
	printk("  wea_dwell_time_ms: %u\n", ic->ic_ocac.ocac_cfg.ocac_params.wea_dwell_time_ms);
	printk("  wea_duration_secs: %u\n", ic->ic_ocac.ocac_cfg.ocac_params.wea_duration_secs);
	printk("  wea_cac_time_secs: %u\n", ic->ic_ocac.ocac_cfg.ocac_params.wea_cac_time_secs);
	printk("  thrshld_fat:       %u\n", ic->ic_ocac.ocac_cfg.ocac_params.thresh_fat);
	printk("  thrshld_traffic:   %u\n", ic->ic_ocac.ocac_cfg.ocac_params.thresh_traffic);
	printk("  thrshld_cca_intf:  %u\n", ic->ic_ocac.ocac_cfg.ocac_params.thresh_cca_intf);
	printk("  thrshld_fat_dec:   %u\n", ic->ic_ocac.ocac_cfg.ocac_params.thresh_fat_dec);
	printk("  timer_interval:    %u\n", ic->ic_ocac.ocac_cfg.ocac_params.timer_interval);
	printk("  traffic_ctrl:      %u\n", ic->ic_ocac.ocac_cfg.ocac_params.traffic_ctrl);
	printk("  offset_txhalt:     %u\n", ic->ic_ocac.ocac_cfg.ocac_params.offset_txhalt);
	printk("  offset_offchan:    %u\n", ic->ic_ocac.ocac_cfg.ocac_params.offset_offchan);
	printk("  beacon_interval:   %u\n", ic->ic_ocac.ocac_cfg.ocac_params.beacon_interval);
}

static int
ieee80211_param_ocac_set(struct net_device *dev, struct ieee80211vap *vap, u_int32_t value)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t cmd =	value >> IEEE80211_OCAC_COMMAND_S;
	uint32_t arg = value & IEEE80211_OCAC_VALUE_M;

	if (!ieee80211_swfeat_is_supported(SWFEAT_ID_OCAC, 1))
		return -1;

	if (cmd >= IEEE80211_OCAC_SET_MAX) {
		printk("%s: invalid DFS_s_radio setparam cmd %u, arg=%u\n",
				dev->name, cmd, arg);
		return -1;
	}

	OCACDBG(OCACLOG_NOTICE, "setparam command: %u, value: 0x%x\n", cmd, arg);

	switch (cmd) {
	case IEEE80211_OCAC_SET_ENABLE:
		if (ic->ic_ocac.ocac_cfg.ocac_enable) {
			printk("DFS seamless radio is already running\n");
			return -1;
		}
		if (ieee80211_wireless_start_ocac(vap, arg)) {
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_enable = 1;
		ic->ic_beacon_update(vap);
		break;
	case IEEE80211_OCAC_SET_DISABLE:
		if (ic->ic_ocac.ocac_cfg.ocac_enable) {
			ieee80211_wireless_stop_ocac(vap);
			ic->ic_ocac.ocac_cfg.ocac_enable = 0;
			ic->ic_beacon_update(vap);
		}
		break;
	case IEEE80211_OCAC_SET_DEBUG_LEVEL:
		ic->ic_ocac.ocac_cfg.ocac_debug_level = arg;
		break;
	case IEEE80211_OCAC_SET_DWELL_TIME:
		if (arg < IEEE80211_OCAC_DWELL_TIME_MIN ||
				arg > IEEE80211_OCAC_DWELL_TIME_MAX) {
			printk("Invalid DFS_s_radio dwell time: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.dwell_time_ms = arg;
		break;
	case IEEE80211_OCAC_SET_SECURE_DWELL_TIME:
		if (arg < IEEE80211_OCAC_SECURE_DWELL_TIME_MIN ||
				arg > IEEE80211_OCAC_SECURE_DWELL_TIME_MAX) {
			printk("Invalid DFS_s_radio secure dwell time: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.secure_dwell_ms = arg;
		break;
	case IEEE80211_OCAC_SET_DURATION:
		if (arg < IEEE80211_OCAC_DURATION_MIN ||
				arg > IEEE80211_OCAC_DURATION_MAX) {
			printk("Invalid DFS_s_radio duration: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.duration_secs = arg;
		break;
	case IEEE80211_OCAC_SET_CAC_TIME:
		if (arg < IEEE80211_OCAC_CAC_TIME_MIN ||
				arg > IEEE80211_OCAC_CAC_TIME_MAX) {
			printk("Invalid DFS_s_radio cac time: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.cac_time_secs = arg;
		break;
	case IEEE80211_OCAC_SET_WEATHER_DWELL_TIME:
		if (arg < IEEE80211_OCAC_DWELL_TIME_MIN ||
				arg > IEEE80211_OCAC_DWELL_TIME_MAX) {
			printk("Invalid DFS_s_radio dwell time: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.wea_dwell_time_ms = arg;
		break;
	case IEEE80211_OCAC_SET_WEATHER_DURATION:
		if (arg & IEEE80211_OCAC_COMPRESS_VALUE_F) {
			arg = (arg & IEEE80211_OCAC_COMPRESS_VALUE_M) << 2;
		}
		if (arg < IEEE80211_OCAC_WEA_DURATION_MIN ||
				arg > IEEE80211_OCAC_WEA_DURATION_MAX) {
			printk("Invalid DFS_s_radio duration for weather channel: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.wea_duration_secs = arg;
		break;
	case IEEE80211_OCAC_SET_WEATHER_CAC_TIME:
		if (arg & IEEE80211_OCAC_COMPRESS_VALUE_F) {
			arg = (arg & IEEE80211_OCAC_COMPRESS_VALUE_M) << 2;
		}
		if (arg < IEEE80211_OCAC_WEA_CAC_TIME_MIN ||
				arg > IEEE80211_OCAC_WEA_CAC_TIME_MAX) {
			printk("Invalid DFS_s_radio cac time for weather channel: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.wea_cac_time_secs = arg;
		break;
	case IEEE80211_OCAC_SET_THRESHOLD_FAT:
		if (arg < IEEE80211_OCAC_THRESHOLD_FAT_MIN ||
				arg > IEEE80211_OCAC_THRESHOLD_FAT_MAX) {
			printk("Invalid DFS_s_radio fat threshold: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.thresh_fat = arg;
		break;
	case IEEE80211_OCAC_SET_THRESHOLD_TRAFFIC:
		if (arg < IEEE80211_OCAC_THRESHOLD_TRAFFIC_MIN ||
				arg > IEEE80211_OCAC_THRESHOLD_TRAFFIC_MAX) {
			printk("Invalid DFS_s_radio traffic threshold: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.thresh_traffic = arg;
		break;
	case IEEE80211_OCAC_SET_THRESHOLD_CCA_INTF:
		if (arg < IEEE80211_OCAC_THRESHOLD_CCA_INTF_MIN ||
				arg > IEEE80211_OCAC_THRESHOLD_CCA_INTF_MAX) {
			printk("Invalid DFS_s_radio cca_intf threshold: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.thresh_cca_intf = arg;
		break;
	case IEEE80211_OCAC_SET_THRESHOLD_FAT_DEC:
		if (arg < IEEE80211_OCAC_THRESHOLD_FAT_DEC_MIN ||
				arg > IEEE80211_OCAC_THRESHOLD_FAT_DEC_MAX) {
			printk("Invalid DFS_s_radio fat_dec threshold: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.thresh_fat_dec = arg;
		break;
	case IEEE80211_OCAC_SET_TIMER_INTERVAL:
		if (arg < IEEE80211_OCAC_TIMER_INTERVAL_MIN ||
				arg > IEEE80211_OCAC_TIMER_INTERVAL_MAX) {
			printk("Invalid DFS_s_radio timer interval: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.timer_interval = arg;
		break;
	case IEEE80211_OCAC_SET_TIMER_EXPIRE_INIT:
		if (arg < IEEE80211_OCAC_TIMER_EXPIRE_INIT_MIN ||
				arg > IEEE80211_OCAC_TIMER_EXPIRE_INIT_MAX) {
			printk("Invalid DFS_s_radio timer expire init: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_timer_expire_init = arg;
		break;
	case IEEE80211_OCAC_SET_OFFSET_TXHALT:
		if (arg < IEEE80211_OCAC_OFFSET_TXHALT_MIN ||
				arg > IEEE80211_OCAC_OFFSET_TXHALT_MAX) {
			printk("Invalid DFS_s_radio offset for txhalt: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.offset_txhalt = arg;
		break;
	case IEEE80211_OCAC_SET_OFFSET_OFFCHAN:
		if (arg < IEEE80211_OCAC_OFFSET_OFFCHAN_MIN ||
				arg > IEEE80211_OCAC_OFFSET_OFFCHAN_MAX) {
			printk("Invalid DFS_s_radio offset for switch off channel: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.offset_offchan = arg;
		break;
	case IEEE80211_OCAC_SET_BEACON_INTERVAL:
		if (arg < IEEE80211_OCAC_BEACON_INTERVAL_MIN ||
				arg > IEEE80211_OCAC_BEACON_INTERVAL_MAX) {
			printk("Invalid DFS_s_radio beacon interval: %u\n", arg);
			return -1;
		}
		ic->ic_ocac.ocac_cfg.ocac_params.beacon_interval = arg;
		break;
	case IEEE80211_OCAC_SET_DUMP_COUNTS:
		ieee80211_ocac_set_dump_counts(ic, arg);
		break;
	case IEEE80211_OCAC_SET_DUMP_TSFLOG:
		ieee80211_ocac_dump_tsflog(ic);
		break;
	case IEEE80211_OCAC_SET_DUMP_CFG:
		ieee80211_ocac_dump_cfg(ic);
		break;
	case IEEE80211_OCAC_SET_TRAFFIC_CONTROL:
		ic->ic_ocac.ocac_cfg.ocac_params.traffic_ctrl = arg ? 1 : 0;
		break;
	case IEEE80211_OCAC_SET_REPORT_ONLY:
		ic->ic_ocac.ocac_cfg.ocac_report_only = arg ? 1 : 0;
		break;
	case IEEE80211_OCAC_SET_DUMP_CCA_COUNTS:
		break;
	default:
		break;
	}

	return 0;
}

static int
ieee80211_ioctl_set_dfs_fast_switch(struct ieee80211com *ic)
{
	if (ic->ic_ieee_alt_chan != 0) {
		struct ieee80211_channel *chan = findchannel(ic, ic->ic_ieee_alt_chan, ic->ic_des_mode);

		if ((chan != NULL) && !(ieee80211_is_chan_available(chan))) {
			return EINVAL;
		}
	}

	ic->ic_flags_ext |= IEEE80211_FEXT_DFS_FAST_SWITCH;
	return 0;
}

static int
ieee80211_ioctl_set_alt_chan(struct ieee80211com *ic, uint8_t ieee_alt_chan)
{
	struct ieee80211_channel *chan = NULL;

	if (ieee_alt_chan == 0) {
		ic->ic_ieee_alt_chan = ieee_alt_chan;
		return 0;
	}

	if (ic->ic_curchan->ic_ieee == ieee_alt_chan) {
		return EINVAL;
	}

	chan = findchannel(ic, ieee_alt_chan, ic->ic_des_mode);

	if (chan == NULL) {
		return EINVAL;
	}

	if ((ic->ic_flags_ext & IEEE80211_FEXT_DFS_FAST_SWITCH)
		&& !(ieee80211_is_chan_available(chan))) {
		return EINVAL;
	}

	ic->ic_ieee_alt_chan = ieee_alt_chan;

	return 0;
}


static int
apply_tx_power(struct ieee80211vap *vap, int enc_val, int flag)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *c;
	int8_t *bw_powers;
	uint8_t start_chan = (enc_val & 0xFF000000) >> 24;
	uint8_t stop_chan = (enc_val & 0x00FF0000) >> 16;
	uint8_t max_power = (enc_val & 0x0000FF00) >> 8;
	uint8_t min_power = (enc_val & 0x000000FF);
	int iter;
	int cur_bw;
	int idx_bf;
	int idx_ss;
	int idx_bw;

	if (start_chan > stop_chan)
		return -EINVAL;

	for (iter = 0; iter < ic->ic_nchans; iter++) {
		c = ic->ic_channels + iter;
		if (start_chan <= c->ic_ieee && c->ic_ieee <= stop_chan) {
			switch (flag) {
			case IEEE80211_BKUP_TXPOWER_NORMAL:
				c->ic_maxpower = max_power;
				c->ic_minpower = min_power;
				c->ic_maxpower_normal = max_power;
				c->ic_minpower_normal = min_power;

				cur_bw = ieee80211_get_bw(ic);
				switch (cur_bw) {
				case BW_HT20:
					idx_bw = PWR_IDX_20M;
					break;
				case BW_HT40:
					idx_bw = PWR_IDX_40M;
					break;
				case BW_HT80:
					idx_bw = PWR_IDX_80M;
					break;
				default:
					idx_bw = PWR_IDX_BW_MAX; /* Invalid case */
					break;
				}
				if (idx_bw < PWR_IDX_BW_MAX) {
					c->ic_maxpower_table[PWR_IDX_BF_OFF][PWR_IDX_1SS][idx_bw] = max_power;
					if (ic->ic_power_table_update) {
						ic->ic_power_table_update(vap, c);
					}
				}
				break;
			case IEEE80211_APPLY_LOWGAIN_TXPOWER:
				if (c->ic_maxpower_normal) {
					c->ic_maxpower = IEEE80211_LOWGAIN_TXPOW_MAX;
					c->ic_minpower = IEEE80211_LOWGAIN_TXPOW_MIN;
				}
				break;
			case IEEE80211_APPLY_TXPOWER_NORMAL:
				if (c->ic_maxpower_normal) {
					c->ic_maxpower = c->ic_maxpower_normal;
					c->ic_minpower = c->ic_minpower_normal;
				}
				break;
			case IEEE80211_INIT_TXPOWER_TABLE:
				c->ic_maxpower = max_power;
				c->ic_minpower = min_power;
				c->ic_maxpower_normal = max_power;
				c->ic_minpower_normal = min_power;
				for (idx_bf = PWR_IDX_BF_OFF; idx_bf < PWR_IDX_BF_MAX; idx_bf++) {
					for (idx_ss = PWR_IDX_1SS; idx_ss < PWR_IDX_SS_MAX; idx_ss++) {
						bw_powers = c->ic_maxpower_table[idx_bf][idx_ss];
						bw_powers[PWR_IDX_20M] = max_power;
						bw_powers[PWR_IDX_40M] = max_power;
						bw_powers[PWR_IDX_80M] = max_power;
					}
				}
				if (ic->ic_power_table_update) {
					ic->ic_power_table_update(vap, c);
				}
				break;
			default:
				printk("%s: Invalid flag", __func__);
			}
		}
	}
	return 0;
}

static int ieee80211_set_bw_txpower(struct ieee80211vap *vap, unsigned int enc_val)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t channel = (enc_val >> 24) & 0xFF;
	uint8_t bf_on = (enc_val >> 20) & 0xF;
	uint8_t num_ss = (enc_val >> 16) & 0xF;
	uint8_t bandwidth = (enc_val >> 8) & 0xF;
	uint8_t power = enc_val & 0xFF;
	uint8_t idx_bf = PWR_IDX_BF_OFF + bf_on;
	uint8_t idx_ss = PWR_IDX_1SS + num_ss - 1;
	uint8_t idx_bw = PWR_IDX_20M + bandwidth - QTN_BW_20M;
	int iter;
	int retval = -EINVAL;

	if (idx_bf >= PWR_IDX_BF_MAX ||
			idx_ss >= PWR_IDX_SS_MAX ||
			idx_bw >= PWR_IDX_BW_MAX) {
		return retval;
	}

	for (iter = 0; iter < ic->ic_nchans; iter++) {
		if (ic->ic_channels[iter].ic_ieee == channel) {
			ic->ic_channels[iter].ic_maxpower_table[idx_bf][idx_ss][idx_bw] = power;
			/*
			 * Update the maxpower of current bandwidth if it is for bfoff and 1ss
			 */
			if (idx_bf == PWR_IDX_BF_OFF &&	idx_ss == PWR_IDX_1SS) {
				int cur_bw = ieee80211_get_bw(ic);

				if ((cur_bw == BW_HT20 && bandwidth == QTN_BW_20M) ||
						(cur_bw == BW_HT40 && bandwidth == QTN_BW_40M) ||
						(cur_bw == BW_HT80 && bandwidth == QTN_BW_80M)) {
					ic->ic_channels[iter].ic_maxpower = power;
					ic->ic_channels[iter].ic_maxpower_normal = power;
				}
			}
			retval = 0;

			break;
		}
	}

	return retval;
}

static int ieee80211_dump_tx_power(struct ieee80211com *ic)
{
	struct ieee80211_channel *chan;
	int iter;
	int idx_bf;
	int idx_ss;

	printk("channel   max_pwr   min_pwr   pwr_80M   pwr_40M   pwr_20M\n");
	for (iter = 0; iter < ic->ic_nchans; iter++) {
		chan = &ic->ic_channels[iter];
		if (!isset(ic->ic_chan_active, chan->ic_ieee)) {
			continue;
		}
		for (idx_bf = PWR_IDX_BF_OFF; idx_bf < PWR_IDX_BF_MAX; idx_bf++) {
			for (idx_ss = PWR_IDX_1SS; idx_ss < PWR_IDX_SS_MAX; idx_ss++) {
				printk("%7d   %7d   %7d   %7d   %7d   %7d\n",
					chan->ic_ieee,
					chan->ic_maxpower,
					chan->ic_minpower,
					chan->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_80M],
					chan->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_40M],
					chan->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_20M]);
			}
		}
	}
	return 0;
}

static int set_regulatory_tx_power(struct ieee80211com *ic, int enc_val)
{
	u_int8_t start_chan = (enc_val & 0x00FF0000) >> 16;
	u_int8_t stop_chan = (enc_val & 0x0000FF00) >> 8;
	u_int8_t reg_power = (enc_val & 0x000000FF);
	int iter;

	if (start_chan > stop_chan)
		return -EINVAL;

	for (iter = 0; iter < ic->ic_nchans; iter++) {
		if (start_chan <= ic->ic_channels[iter].ic_ieee &&
		    ic->ic_channels[iter].ic_ieee <= stop_chan) {
			ic->ic_channels[iter].ic_maxregpower = reg_power;
		}
	}

	return 0;
}

void ieee80211_doth_meas_callback_success(void *ctx)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	if (ni->ni_meas_info.pending) {
		ni->ni_meas_info.pending = 0;
		ni->ni_meas_info.reason = 0;
		wake_up_interruptible(&ni->ni_meas_info.meas_waitq);
	}
}

void ieee80211_doth_meas_callback_fail(void *ctx, int32_t reason)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	if (ni->ni_meas_info.pending) {
		ni->ni_meas_info.pending = 0;
		ni->ni_meas_info.reason = reason;
		wake_up_interruptible(&ni->ni_meas_info.meas_waitq);
	}
}

void ioctl_tpc_report_callback_success(void *ctx)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	if (ni->ni_tpc_info.tpc_wait_info.tpc_pending) {
		ni->ni_tpc_info.tpc_wait_info.tpc_pending = 0;
		ni->ni_tpc_info.tpc_wait_info.reason = 0;
		wake_up_interruptible(&ni->ni_tpc_info.tpc_wait_info.tpc_waitq);
	}
}

void ioctl_tpc_report_callback_fail(void *ctx, int32_t reason)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	if (ni->ni_tpc_info.tpc_wait_info.tpc_pending) {
		ni->ni_tpc_info.tpc_wait_info.tpc_pending = 0;
		ni->ni_tpc_info.tpc_wait_info.reason = reason;
		wake_up_interruptible(&ni->ni_tpc_info.tpc_wait_info.tpc_waitq);
	}
}

static int
ieee80211_subioctl_get_doth_dotk_report(struct net_device *dev, char __user *user_pointer)
{
	int ret = 0;
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211req_node_info	req_info;
	union ieee80211rep_node_info	rep_info;
	union ieee80211rep_node_info	*p_rep_info;

	if (copy_from_user(&req_info, user_pointer, sizeof(struct ieee80211req_node_info)))
		return -EFAULT;

	p_rep_info = &rep_info;
	switch (req_info.req_type) {
	case IOCTL_REQ_MEASUREMENT:
	{
		struct ieee80211_node *ni;

		ni = ieee80211_find_node(&ic->ic_sta, req_info.u_req_info.req_node_meas.mac_addr);
		if (NULL == ni)
			return -EINVAL;

		switch (req_info.u_req_info.req_node_meas.type) {
		case IOCTL_MEAS_TYPE_BASIC:
			ieee80211_send_meas_request_basic(ni,
					req_info.u_req_info.req_node_meas.ioctl_basic.channel,
					req_info.u_req_info.req_node_meas.ioctl_basic.start_offset_ms,
					req_info.u_req_info.req_node_meas.ioctl_basic.duration_ms,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(req_info.u_req_info.req_node_meas.ioctl_basic.start_offset_ms,
						req_info.u_req_info.req_node_meas.ioctl_basic.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_CCA:
			ieee80211_send_meas_request_cca(ni,
					req_info.u_req_info.req_node_meas.ioctl_cca.channel,
					req_info.u_req_info.req_node_meas.ioctl_cca.start_offset_ms,
					req_info.u_req_info.req_node_meas.ioctl_cca.duration_ms,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(req_info.u_req_info.req_node_meas.ioctl_cca.start_offset_ms,
						req_info.u_req_info.req_node_meas.ioctl_cca.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_RPI:
			ieee80211_send_meas_request_rpi(ni,
					req_info.u_req_info.req_node_meas.ioctl_rpi.channel,
					req_info.u_req_info.req_node_meas.ioctl_rpi.start_offset_ms,
					req_info.u_req_info.req_node_meas.ioctl_rpi.duration_ms,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(req_info.u_req_info.req_node_meas.ioctl_rpi.start_offset_ms,
						req_info.u_req_info.req_node_meas.ioctl_rpi.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_CHAN_LOAD:
			ieee80211_send_rm_req_chan_load(ni,
					req_info.u_req_info.req_node_meas.ioctl_chan_load.channel,
					req_info.u_req_info.req_node_meas.ioctl_chan_load.duration_ms,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(0,
						req_info.u_req_info.req_node_meas.ioctl_chan_load.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_NOISE_HIS:
			ieee80211_send_rm_req_noise_his(ni,
					req_info.u_req_info.req_node_meas.ioctl_noise_his.channel,
					req_info.u_req_info.req_node_meas.ioctl_noise_his.duration_ms,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(0,
						req_info.u_req_info.req_node_meas.ioctl_noise_his.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_BEACON:
			ieee80211_send_rm_req_beacon(ni,
					req_info.u_req_info.req_node_meas.ioctl_beacon.op_class,
					req_info.u_req_info.req_node_meas.ioctl_beacon.channel,
					req_info.u_req_info.req_node_meas.ioctl_beacon.duration_ms,
					req_info.u_req_info.req_node_meas.ioctl_beacon.mode,
					req_info.u_req_info.req_node_meas.ioctl_beacon.bssid,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(0,
						req_info.u_req_info.req_node_meas.ioctl_beacon.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_FRAME:
			ieee80211_send_rm_req_frame(ni,
					req_info.u_req_info.req_node_meas.ioctl_frame.op_class,
					req_info.u_req_info.req_node_meas.ioctl_frame.channel,
					req_info.u_req_info.req_node_meas.ioctl_frame.duration_ms,
					req_info.u_req_info.req_node_meas.ioctl_frame.type,
					req_info.u_req_info.req_node_meas.ioctl_frame.mac_address,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(0,
						req_info.u_req_info.req_node_meas.ioctl_frame.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case  IOCTL_MEAS_TYPE_CAT:
			ieee80211_send_rm_req_tran_stream_cat(ni,
					req_info.u_req_info.req_node_meas.ioctl_tran_stream_cat.duration_ms,
					req_info.u_req_info.req_node_meas.ioctl_tran_stream_cat.peer_sta,
					req_info.u_req_info.req_node_meas.ioctl_tran_stream_cat.tid,
					req_info.u_req_info.req_node_meas.ioctl_tran_stream_cat.bin0,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(0,
						req_info.u_req_info.req_node_meas.ioctl_tran_stream_cat.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_MUL_DIAG:
			ieee80211_send_rm_req_multicast_diag(ni,
					req_info.u_req_info.req_node_meas.ioctl_multicast_diag.duration_ms,
					req_info.u_req_info.req_node_meas.ioctl_multicast_diag.group_mac,
					IEEE80211_MEASUREMENT_REQ_TIMEOUT(0,
						req_info.u_req_info.req_node_meas.ioctl_multicast_diag.duration_ms),
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_LINK:
			ieee80211_send_link_measure_request(ni,
					HZ / 10,
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		case IOCTL_MEAS_TYPE_NEIGHBOR:
			ieee80211_send_neighbor_report_request(ni,
					HZ / 10,
					(void *)ieee80211_doth_meas_callback_success,
					(void *)ieee80211_doth_meas_callback_fail);
			break;
		default:
			break;
		}

		ni->ni_meas_info.pending = 1;
		ret = wait_event_interruptible(ni->ni_meas_info.meas_waitq,
				ni->ni_meas_info.pending == 0);

		if (ret ==  0) {
			if (ni->ni_meas_info.reason != 0) {
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
					"[%s]Measurement Request Fail:timeout waiting for response\n",
					__func__);
				switch (ni->ni_meas_info.reason) {
				case PPQ_FAIL_TIMEOUT:
					p_rep_info->meas_result.status = IOCTL_MEAS_STATUS_TIMEOUT;
					break;
				case PPQ_FAIL_NODELEAVE:
					p_rep_info->meas_result.status = IOCTL_MEAS_STATUS_NODELEAVE;
					break;
				case PPQ_FAIL_STOP:
					p_rep_info->meas_result.status = IOCTL_MEAS_STATUS_STOP;
				default:
					break;
				}
			} else {
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
					"[%s]Measurement Request SUCC ret = %d\n",
					__func__, ret);

				if (req_info.u_req_info.req_node_meas.type == IOCTL_MEAS_TYPE_LINK) {
					p_rep_info->meas_result.status = IOCTL_MEAS_STATUS_SUCC;
					p_rep_info->meas_result.report_mode = 0;
					p_rep_info->meas_result.u_data.link_measure.tpc_report.link_margin = ni->ni_lm.tpc_report.link_margin;
					p_rep_info->meas_result.u_data.link_measure.tpc_report.tx_power = ni->ni_lm.tpc_report.tx_power;
					p_rep_info->meas_result.u_data.link_measure.recv_antenna_id = ni->ni_lm.recv_antenna_id;
					p_rep_info->meas_result.u_data.link_measure.tran_antenna_id = ni->ni_lm.tran_antenna_id;
					p_rep_info->meas_result.u_data.link_measure.rcpi = ni->ni_lm.rcpi;
					p_rep_info->meas_result.u_data.link_measure.rsni = ni->ni_lm.rsni;

					break;
				}

				if (req_info.u_req_info.req_node_meas.type == IOCTL_MEAS_TYPE_NEIGHBOR) {
					uint8_t i;

					p_rep_info->meas_result.status = IOCTL_MEAS_STATUS_SUCC;
					p_rep_info->meas_result.report_mode = 0;
					p_rep_info->meas_result.u_data.neighbor_report.item_num = 0;
					for (i = 0; i < ni->ni_neighbor.report_count && i < IEEE80211_MAX_NEIGHBOR_REPORT_ITEM; i++) {
						memcpy(&p_rep_info->meas_result.u_data.neighbor_report.item[i],
								ni->ni_neighbor.item_table[i],
								sizeof(p_rep_info->meas_result.u_data.neighbor_report.item[i]));
						kfree(ni->ni_neighbor.item_table[i]);
						ni->ni_neighbor.item_table[i] = NULL;
					}
					p_rep_info->meas_result.u_data.neighbor_report.item_num = i;
					ni->ni_neighbor.report_count = 0;
					break;
				}

				p_rep_info->meas_result.status = IOCTL_MEAS_STATUS_SUCC;
				p_rep_info->meas_result.report_mode = ni->ni_meas_info.ni_meas_rep_mode;
				if (ni->ni_meas_info.ni_meas_rep_mode == 0) {
					switch (req_info.u_req_info.req_node_meas.type) {
					case IOCTL_MEAS_TYPE_BASIC:
						p_rep_info->meas_result.u_data.basic = ni->ni_meas_info.rep.basic;
						break;
					case IOCTL_MEAS_TYPE_CCA:
						p_rep_info->meas_result.u_data.cca = ni->ni_meas_info.rep.cca;
						break;
					case IOCTL_MEAS_TYPE_RPI:
						memcpy(p_rep_info->meas_result.u_data.rpi,
								ni->ni_meas_info.rep.rpi,
								sizeof(p_rep_info->meas_result.u_data.rpi));
						break;
					case IOCTL_MEAS_TYPE_CHAN_LOAD:
						p_rep_info->meas_result.u_data.chan_load = ni->ni_meas_info.rep.chan_load;
						break;
					case IOCTL_MEAS_TYPE_NOISE_HIS:
						p_rep_info->meas_result.u_data.noise_his.antenna_id = ni->ni_meas_info.rep.noise_his.antenna_id;
						p_rep_info->meas_result.u_data.noise_his.anpi = ni->ni_meas_info.rep.noise_his.anpi;
						memcpy(p_rep_info->meas_result.u_data.noise_his.ipi,
								ni->ni_meas_info.rep.noise_his.ipi,
								sizeof(p_rep_info->meas_result.u_data.noise_his.ipi));
						break;
					case IOCTL_MEAS_TYPE_BEACON:
						p_rep_info->meas_result.u_data.beacon.reported_frame_info = ni->ni_meas_info.rep.beacon.reported_frame_info;
						p_rep_info->meas_result.u_data.beacon.rcpi = ni->ni_meas_info.rep.beacon.rcpi;
						p_rep_info->meas_result.u_data.beacon.rsni = ni->ni_meas_info.rep.beacon.rsni;
						memcpy(p_rep_info->meas_result.u_data.beacon.bssid,
								ni->ni_meas_info.rep.beacon.bssid,
								sizeof(p_rep_info->meas_result.u_data.beacon.bssid));
						p_rep_info->meas_result.u_data.beacon.antenna_id = ni->ni_meas_info.rep.beacon.antenna_id;
						p_rep_info->meas_result.u_data.beacon.parent_tsf = ni->ni_meas_info.rep.beacon.parent_tsf;
						break;
					case IOCTL_MEAS_TYPE_FRAME:
						p_rep_info->meas_result.u_data.frame.sub_ele_report = ni->ni_meas_info.rep.frame_count.sub_ele_flag;
						memcpy(p_rep_info->meas_result.u_data.frame.ta, ni->ni_meas_info.rep.frame_count.ta, IEEE80211_ADDR_LEN);
						memcpy(p_rep_info->meas_result.u_data.frame.bssid, ni->ni_meas_info.rep.frame_count.bssid, IEEE80211_ADDR_LEN);
						p_rep_info->meas_result.u_data.frame.phy_type = ni->ni_meas_info.rep.frame_count.phy_type;
						p_rep_info->meas_result.u_data.frame.avg_rcpi = ni->ni_meas_info.rep.frame_count.avg_rcpi;
						p_rep_info->meas_result.u_data.frame.last_rsni = ni->ni_meas_info.rep.frame_count.last_rsni;
						p_rep_info->meas_result.u_data.frame.last_rcpi = ni->ni_meas_info.rep.frame_count.last_rcpi;
						p_rep_info->meas_result.u_data.frame.antenna_id = ni->ni_meas_info.rep.frame_count.antenna_id;
						p_rep_info->meas_result.u_data.frame.frame_count = ni->ni_meas_info.rep.frame_count.frame_count;
						break;
					case IOCTL_MEAS_TYPE_CAT:
						memcpy(&p_rep_info->meas_result.u_data.tran_stream_cat,
								&ni->ni_meas_info.rep.tran_stream_cat,
								sizeof(p_rep_info->meas_result.u_data.tran_stream_cat));
						break;
					case IOCTL_MEAS_TYPE_MUL_DIAG:
						p_rep_info->meas_result.u_data.multicast_diag.reason = ni->ni_meas_info.rep.multicast_diag.reason;
						p_rep_info->meas_result.u_data.multicast_diag.mul_rec_msdu_cnt = ni->ni_meas_info.rep.multicast_diag.mul_rec_msdu_cnt;
						p_rep_info->meas_result.u_data.multicast_diag.first_seq_num = ni->ni_meas_info.rep.multicast_diag.first_seq_num;
						p_rep_info->meas_result.u_data.multicast_diag.last_seq_num = ni->ni_meas_info.rep.multicast_diag.last_seq_num;
						p_rep_info->meas_result.u_data.multicast_diag.mul_rate = ni->ni_meas_info.rep.multicast_diag.mul_rate;

						break;
					default:
						break;
					}
				}
			}
		} else {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
				"[%s]Measurement Request Fail:waiting for response cancelled\n",
				__func__);
			ni->ni_meas_info.pending = 0;
			ret = -ECANCELED;
		}
		ieee80211_free_node(ni);
		break;
	}
	case IOCTL_REQ_TPC:
	{
		struct ieee80211_action_tpc_request request;
		struct ieee80211_action_data	action_data;
		struct ieee80211_node		*ni;

		if (((ic->ic_flags & IEEE80211_F_DOTH) == 0) ||
				((ic->ic_flags_ext & IEEE80211_FEXT_TPC) == 0)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
				"[%s]TPC Request fail:802.11 disabled\n",
				__func__);
			return -EOPNOTSUPP;
		}

		ni = ieee80211_find_node(&ic->ic_sta, req_info.u_req_info.req_node_tpc.mac_addr);
		if (NULL == ni) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
				"[%s]TPC Request Fail:no such node %s\n",
				__func__,
				ether_sprintf(req_info.u_req_info.req_node_tpc.mac_addr));
			return -EINVAL;
		}
		if (((IEEE80211_CAPINFO_SPECTRUM_MGMT & ni->ni_capinfo) == 0) ||
				((ni->ni_flags & IEEE80211_NODE_TPC) == 0)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
				"[%s]TPC Request Fail:node don't support 802.11h\n",
				__func__);
			ieee80211_free_node(ni);
			return -EOPNOTSUPP;
		}

		request.expire = HZ / 10;
		request.fn_success = ioctl_tpc_report_callback_success;
		request.fn_fail = ioctl_tpc_report_callback_fail;
		action_data.cat = IEEE80211_ACTION_CAT_SPEC_MGMT;
		action_data.action = IEEE80211_ACTION_S_TPC_REQUEST;
		action_data.params = &request;
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);

		ni->ni_tpc_info.tpc_wait_info.tpc_pending = 1;
		ni->ni_tpc_info.tpc_wait_info.reason = 0;
		ret = wait_event_interruptible(ni->ni_tpc_info.tpc_wait_info.tpc_waitq,
				ni->ni_tpc_info.tpc_wait_info.tpc_pending == 0);

		if (ret == 0) {
			if (ni->ni_tpc_info.tpc_wait_info.reason != 0) {
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
					"[%s]TPC Request Fail:timeout waiting for response\n",
					__func__);
				p_rep_info->tpc_result.status = 1;
			} else {
				p_rep_info->tpc_result.status = 0;
				p_rep_info->tpc_result.link_margin = ni->ni_tpc_info.tpc_report.node_link_margin;
				p_rep_info->tpc_result.tx_power	= ni->ni_tpc_info.tpc_report.node_txpow;
			}
		} else {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
				"[%s]TPC Request Fail:waiting for response cancelled\n",
				__func__);
			ni->ni_tpc_info.tpc_wait_info.tpc_pending = 0;
			ret = -ECANCELED;
		}
		ieee80211_free_node(ni);
		break;
	}
	default:
		ret = -EOPNOTSUPP;
		break;
	}
	if (ret == 0)
		ret = copy_to_user(user_pointer, p_rep_info, sizeof(union ieee80211rep_node_info));

	return ret;
}

void ieee80211_beacon_interval_set(struct ieee80211com *ic, int value)
{
	struct ieee80211vap *vap_each;

	ic->ic_lintval = value;

	TAILQ_FOREACH(vap_each, &ic->ic_vaps, iv_next) {
		if ((vap_each->iv_opmode == IEEE80211_M_HOSTAP) ||
				(vap_each->iv_opmode == IEEE80211_M_IBSS) ){
			vap_each->iv_bss->ni_intval = value;
			ic->ic_beacon_update(vap_each);
		}
	}
}
EXPORT_SYMBOL(ieee80211_beacon_interval_set);

static int ieee80211_11ac_mcs_format(int mcs, int bw)
{
	int retval = IEEE80211_11AC_MCS_VAL_ERR;
	int mcs_val, mcs_nss;

	/* Check for unequal MCS */
	if (mcs & 0x100) {
		mcs &=0xFF;
		if ((mcs >= IEEE80211_UNEQUAL_MCS_START) && (mcs <= IEEE80211_UNEQUAL_MCS_MAX)) {
			if (ieee80211_swfeat_is_supported(SWFEAT_ID_2X2, 0) ||
					ieee80211_swfeat_is_supported(SWFEAT_ID_2X4, 0)) {
				if (mcs > IEEE80211_HT_UNEQUAL_MCS_2SS_MAX) {
					return retval;
				}
			} else if (ieee80211_swfeat_is_supported(SWFEAT_ID_3X3, 0)) {
				if (mcs > IEEE80211_HT_UNEQUAL_MCS_3SS_MAX) {
					return retval;
				}
			}
			retval = (mcs - IEEE80211_UNEQUAL_MCS_START) | IEEE80211_UNEQUAL_MCS_BIT;
		}
	} else {
		mcs_val = mcs & IEEE80211_AC_MCS_VAL_MASK;
		mcs_nss = (mcs & IEEE80211_AC_MCS_NSS_MASK) >> IEEE80211_11AC_MCS_NSS_SHIFT;

		if (!ieee80211_vht_tx_mcs_is_valid(mcs_val, mcs_nss)) {
			return retval;
		}

		retval = (bw == 20) ? wlan_11ac_20M_mcs_nss_tbl[(mcs_nss * IEEE80211_AC_MCS_MAX) + mcs_val] : mcs;
	}
	return retval;
}

static int
ieee80211_ioctl_setchan_inactive_pri(struct ieee80211com *ic, struct ieee80211vap *vap, uint32_t value)
{
	uint16_t chan = value & 0xFFFF;
	uint8_t active = (value >> 16) & 0xFF;
	uint8_t flags = (value >> 24) & 0xFF;

	int reselect = 0;
	struct ieee80211_channel *c;
	int cur_bw;
	int set_inactive = 0;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		return 0;
	}

	c = findchannel_any(ic, chan, ic->ic_des_mode);
	if (c == NULL) {
		return -EINVAL;
	}

	if (active) {
		if (isset(ic->ic_chan_pri_inactive, chan)) {
			/*
			 * clrbit if flag indicates user configurtion.
			 * And save the bit to is_inactive_usercfg to honor user configurtion
			 * so as to override regulatory db.
			 *
			 * Otherwise clrbit only if user has not cfg-ed.
			 */
			if (flags & CHAN_PRI_INACTIVE_CFG_USER_OVERRIDE) {
				clrbit(ic->ic_chan_pri_inactive, chan);
				printk("channel %u is removed from non-primary channel list\n", chan);
				setbit(ic->ic_is_inactive_usercfg, chan);
			} else if (!isset(ic->ic_is_inactive_usercfg, chan)) {
				clrbit(ic->ic_chan_pri_inactive, chan);
				printk("channel %u is removed from non-primary channel list\n", chan);
			}
		}
		return 0;
	} else {
		if (isclr(ic->ic_chan_pri_inactive, chan) ||
				!!(flags & CHAN_PRI_INACTIVE_CFG_AUTOCHAN_ONLY) !=
						!!isset(ic->ic_is_inactive_autochan_only, chan)) {
			/*
			 * setbit if flag indicates user configurtion.
			 * And save the bit to is_inactive_usercfg to honor user configurtion
			 * so as to override regulatory db.
			 *
			 * Otherwise setbit only if user has not cfg-ed.
			 */
			if (flags & CHAN_PRI_INACTIVE_CFG_USER_OVERRIDE) {
				setbit(ic->ic_chan_pri_inactive, chan);
				printk("channel %u is added into non-primary channel list\n", chan);
				setbit(ic->ic_is_inactive_usercfg, chan);
				set_inactive = 1;
			} else if (!isset(ic->ic_is_inactive_usercfg, chan)) {
				setbit(ic->ic_chan_pri_inactive, chan);
				printk("channel %u is added into non-primary channel list\n", chan);
				set_inactive = 1;
			}
			if (set_inactive) {
				if (flags & CHAN_PRI_INACTIVE_CFG_AUTOCHAN_ONLY) {
					setbit(ic->ic_is_inactive_autochan_only, chan);
					printk("channel %u is non-primary for auto channel selection only\n", chan);
				} else {
					clrbit(ic->ic_is_inactive_autochan_only, chan);
				}
			}
		} else {
			return 0;
		}
	}

	cur_bw = ieee80211_get_bw(ic);
	if (cur_bw >= BW_HT40) {
		int no_pri_chan = 1;
		int c_ieee = ieee80211_find_sec_chan(c);

		if (c_ieee && isclr(ic->ic_chan_pri_inactive, c_ieee)) {
			no_pri_chan = 0;
		}
		if (cur_bw >= BW_HT80) {
			c_ieee = ieee80211_find_sec40u_chan(c);
			if (c_ieee && isclr(ic->ic_chan_pri_inactive, c_ieee)) {
				no_pri_chan = 0;
			}
			c_ieee = ieee80211_find_sec40l_chan(c);
			if (c_ieee && isclr(ic->ic_chan_pri_inactive, c_ieee)) {
				no_pri_chan = 0;
			}
		}
		if (no_pri_chan) {
			printk("Warning: all the sub channels are not in primary channel list!\n");
		}
	}

	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
			isset(ic->ic_chan_pri_inactive, ic->ic_bsschan->ic_ieee)) {
		printk("current channel is %d, cannot be used as primary channel\n", ic->ic_bsschan->ic_ieee);
		reselect = 1;
	}
	if(ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
			isset(ic->ic_chan_pri_inactive, ic->ic_des_chan->ic_ieee)) {
		printk("Channel %d cannot be used as desired channel\n", ic->ic_des_chan->ic_ieee);
		ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		reselect = 1;
	}

	if (reselect && IS_UP_AUTO(vap)) {
		ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	}

	return 0;
}

static int
local_get_inactive_primary_chan_num(struct ieee80211com *ic, struct ieee80211_inactive_chanlist *chanlist)
{
	int i;
	int num = 0;

	for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
		if (isset(ic->ic_chan_pri_inactive, i)) {
			if (chanlist)
				chanlist->channels[i] = CHAN_PRI_INACTIVE_CFG_USER_OVERRIDE;
			if (chanlist && isset(ic->ic_is_inactive_autochan_only, i))
				chanlist->channels[i] |= CHAN_PRI_INACTIVE_CFG_AUTOCHAN_ONLY;
			num++;
		}
	}

	return num;
}

static int
ieee80211_get_inactive_primary_chan_num(struct ieee80211com *ic)
{
	return local_get_inactive_primary_chan_num(ic, NULL);
}

static void
ieee80211_training_restart_by_node_idx(struct ieee80211vap *vap, uint16_t node_idx)
{
	struct ieee80211_node *ni;

	ni = ieee80211_find_node_by_node_idx(vap, node_idx);
	if (ni) {
		ieee80211_node_training_start(ni, 1);
		ieee80211_free_node(ni);
	}
}

static int
ieee80211_subioctl_set_chan_dfs_required(struct net_device *dev, void __user *pointer, int cnt)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	int channels[IEEE80211_CHAN_MAX];
	int i;

	if (cnt > ARRAY_SIZE(channels)) {
		printk("%s: max number of supported channels is %ld\n", __func__,
			ARRAY_SIZE(channels));
		cnt = ARRAY_SIZE(channels);
	}

	if (copy_from_user(channels, pointer, cnt*sizeof(int)))
		return -EFAULT;

	for (i = 0; i < cnt; i++) {
		if (channels[i] < IEEE80211_CHAN_MAX && channels[i] > 0) {
			setbit(ic->ic_chan_dfs_required, channels[i]);
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
				"Mark DFS channel %d\n", channels[i]);
		}
	}

	ic->ic_mark_dfs_channels(ic, ic->ic_nchans, ic->ic_channels);

	return 0;
}

static int
ieee80211_subioctl_set_chan_weather_radar(struct net_device *dev, void __user *pointer, int cnt)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	int channels[IEEE80211_CHAN_MAX];
	int i;

	if (cnt > ARRAY_SIZE(channels)) {
		printk("%s: max number of supported channels is %ld\n", __func__,
			ARRAY_SIZE(channels));
		cnt = ARRAY_SIZE(channels);
	}

	if (copy_from_user(channels, pointer, cnt*sizeof(int)))
		return -EFAULT;

	for (i = 0; i < cnt; i++) {
		if (channels[i] < IEEE80211_CHAN_MAX && channels[i] > 0) {
			setbit(ic->ic_chan_weather_radar, channels[i]);
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
				"Mark weather channel %d\n", channels[i]);
		}
	}

	ic->ic_mark_weather_radar_chans(ic, ic->ic_nchans, ic->ic_channels);

	return 0;
}

static int
ieee80211_subioctl_setget_chan_disabled(struct net_device *dev, void __user *pointer, int len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieeee80211_disabled_chanlist channels;
	int i, j = 0, cnt;

	if (copy_from_user(&channels, pointer, len)) {
		printk("%s: copy_from_user failed\n", __FUNCTION__);
		return -EFAULT;
	}

	if (channels.dir == SET_CHAN_DISABLED) {
		if ((vap->iv_opmode == IEEE80211_M_STA && vap->iv_state == IEEE80211_S_RUN) ||
				(vap->iv_opmode == IEEE80211_M_HOSTAP && ic->ic_sta_assoc != 0)) {
			printk("%s: channel disable settings is not allowed during associated\n", __FUNCTION__);
			return -EPERM;
		}

		cnt = channels.list_len;
		if (cnt > ARRAY_SIZE(channels.chan)) {
			printk("%s: max number of supported channels is %ld\n", __func__,
				ARRAY_SIZE(channels.chan));
			return -EFAULT;
		}

		for (i = 0; i < cnt; i++) {
			if (channels.chan[i] >= IEEE80211_CHAN_MAX)
				return -EFAULT;
			if (!isset(ic->ic_chan_avail, channels.chan[i]))
				return -EFAULT;
		}

		for (i = 0; i < cnt; i++) {
			if (!channels.flag) {
				clrbit(ic->ic_chan_disabled, channels.chan[i]);
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
					"Mark channel %d enabled\n", channels.chan[i]);
			} else {
				setbit(ic->ic_chan_disabled, channels.chan[i]);
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
					"Mark channel %d disabled\n", channels.chan[i]);
			}
		}
	} else if (channels.dir == GET_CHAN_DISABLED) {
		for (i = 1; i <= IEEE80211_CHAN_MAX; i++) {
			if (isset(ic->ic_chan_disabled, i)) {
				channels.chan[j++] = i;
			}
		}
		channels.list_len = j;

		if (copy_to_user(pointer, &channels, sizeof(channels))) {
			printk("%s: copy_to_user failed\n", __FUNCTION__);
			return -EIO;
		}
	}

	return 0;
}

static int
ieee80211_subioctl_wowlan_setget(struct net_device *dev, struct ieee80211req_wowlan __user* ps, int len)
{
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	struct ieee80211req_wowlan req;
	int ret = 0;

	if (!ps) {
		printk("%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}

	if ((sizeof(req) > len)) {
		printk("%s: low memory for request's result\n", __FUNCTION__);
		return -EFAULT;
	}

	if (copy_from_user(&req, ps, sizeof(struct ieee80211req_wowlan))) {
		printk("%s: copy_from_user failed\n", __FUNCTION__);
		return -EIO;
	}

	if (!req.is_data) {
		printk("%s: user space buffer invalid\n", __FUNCTION__);
		return -EFAULT;
	}

	switch (req.is_op) {
	case IEEE80211_WOWLAN_MAGIC_PATTERN:
		if (req.is_data) {
			uint32_t len = MIN(MAX_USER_DEFINED_MAGIC_LEN, req.is_data_len);
			memset(ic->ic_wowlan.pattern.magic_pattern, 0, MAX_USER_DEFINED_MAGIC_LEN);
			if (copy_from_user(ic->ic_wowlan.pattern.magic_pattern, req.is_data, len)) {
				printk("%s: copy_from_user pattern copy failed\n", __FUNCTION__);
				return -EIO;
			}
			ic->ic_wowlan.pattern.len = len;
		}
		return 0;
	case IEEE80211_WOWLAN_HOST_POWER_SAVE:
		ret = copy_to_user(req.is_data, &(ic->ic_wowlan.host_state),
				MIN(req.is_data_len, sizeof(ic->ic_wowlan.host_state)));
		break;
	case IEEE80211_WOWLAN_MATCH_TYPE:
		ret = copy_to_user(req.is_data, &(ic->ic_wowlan.wowlan_match),
				MIN(req.is_data_len, sizeof(ic->ic_wowlan.wowlan_match)));
		break;
	case IEEE80211_WOWLAN_L2_ETHER_TYPE:
		ret = copy_to_user(req.is_data, &(ic->ic_wowlan.L2_ether_type),
				MIN(req.is_data_len, sizeof(ic->ic_wowlan.L2_ether_type)));
		break;
	case IEEE80211_WOWLAN_L3_UDP_PORT:
		ret = copy_to_user(req.is_data, &(ic->ic_wowlan.L3_udp_port),
				MIN(req.is_data_len, sizeof(ic->ic_wowlan.L3_udp_port)));
		break;
	case IEEE80211_WOWLAN_MAGIC_PATTERN_GET:
		req.is_data_len = MIN(req.is_data_len, ic->ic_wowlan.pattern.len);
		ret = copy_to_user(req.is_data, &(ic->ic_wowlan.pattern.magic_pattern), req.is_data_len);
		break;
	default:
		break;
	}

	if (ret) {
		printk("%s: buffer content: copy_to_user failed\n", __FUNCTION__);
		return -EIO;
	}

	if (copy_to_user(ps, &req, sizeof(req))) {
		printk("%s: copy_to_user failed\n", __FUNCTION__);
		return -EIO;
	}
	return 0;
}

static int
ieee80211_subioctl_get_sta_auth(struct net_device *dev, struct ieee80211req_auth_description __user* ps, int cnt)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	struct ieee80211req_auth_description auth_description;
	uint8_t *casted_ptr = (uint8_t*)&auth_description.description;

	if (!ps) {
		printk("%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}

	if (sizeof(auth_description) > cnt) {
		printk("%s: low memory for request's result\n", __FUNCTION__);
		return -EFAULT;
	}

	if (copy_from_user(&auth_description, ps, sizeof(auth_description))) {
		printk("%s: copy_from_user failed\n", __FUNCTION__);
		return -EIO;
	}

	ni = ieee80211_find_node(&ic->ic_sta, auth_description.macaddr);
	if (NULL == ni) {
		printk("%s: client not found\n", __FUNCTION__);
		return -EINVAL;
	}

	casted_ptr[IEEE80211_AUTHDESCR_ALGO_POS] = ni->ni_used_auth_algo;

	if (ni->ni_rsn_ie != NULL || ni->ni_wpa_ie != NULL) {
		casted_ptr[IEEE80211_AUTHDESCR_KEYMGMT_POS] = ni->ni_rsn.rsn_keymgmt;
		casted_ptr[IEEE80211_AUTHDESCR_KEYPROTO_POS] = ni->ni_rsn_ie != NULL ?
								(uint8_t)IEEE80211_AUTHDESCR_KEYPROTO_RSN :
								(uint8_t)IEEE80211_AUTHDESCR_KEYPROTO_WPA;
		casted_ptr[IEEE80211_AUTHDESCR_CIPHER_POS] = (uint8_t)ni->ni_rsn.rsn_ucastcipher;
	} else {
		if (vap->iv_flags & IEEE80211_F_PRIVACY)
			casted_ptr[IEEE80211_AUTHDESCR_KEYMGMT_POS] = (uint8_t)IEEE80211_AUTHDESCR_KEYMGMT_WEP;
	}

	ieee80211_free_node(ni);

	if (copy_to_user(ps, &auth_description, sizeof(auth_description))) {
		printk("%s: copy_to_user failed\n", __FUNCTION__);
		return -EIO;
	}
	return 0;
}

static int
ieee80211_subioctl_get_sta_vendor(struct net_device *dev, uint8_t __user* ps)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	uint8_t macaddr[IEEE80211_ADDR_LEN];

	if (!ps) {
		printk("%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}
	if (copy_from_user(macaddr, ps, sizeof(macaddr))) {
		printk("%s: copy_from_user failed\n", __FUNCTION__);
		return -EIO;
	}

	ni = ieee80211_find_node(&ic->ic_sta, macaddr);
	if (NULL == ni) {
		printk("%s: client not found\n", __FUNCTION__);
		return -EINVAL;
	}

	if (copy_to_user(ps, &ni->ni_vendor, sizeof(ni->ni_vendor))) {
		printk("%s: copy_to_user failed\n", __FUNCTION__);
		ieee80211_free_node(ni);
		return -EIO;
	}

	ieee80211_free_node(ni);
	return 0;
}
static int local_is_channel_disabled(struct ieee80211com *ic, int channel, int bw)
{
	if (isset(ic->ic_chan_disabled, channel)) {
		return 1;
	}

	/* based on BW, need to check if more channels need to be disabled*/
	if (bw >= BW_HT40) {
		uint32_t chan_sec = 0;
		uint32_t chan_sec40u = 0;
		uint32_t chan_sec40l = 0;
		struct ieee80211_channel *chan;
		chan = findchannel(ic, channel, ic->ic_des_mode);
		if (chan == NULL) {
			chan = findchannel(ic, channel, IEEE80211_MODE_AUTO);
		}

		if (chan) {
			chan_sec = ieee80211_find_sec_chan(chan);
			if (unlikely(chan_sec && isset(ic->ic_chan_disabled, chan_sec))) {
				return 1;
			}
			if (bw >= BW_HT80) {
				chan_sec40u = ieee80211_find_sec40u_chan(chan);
				chan_sec40l = ieee80211_find_sec40l_chan(chan);
				if (unlikely(chan_sec40u && isset(ic->ic_chan_disabled, chan_sec40u)) ||
						(chan_sec40l && isset(ic->ic_chan_disabled, chan_sec40l))) {
					return 1;
				}
			}
		}
	}
	return 0;
}
static int
ieee80211_subioctl_set_active_chanlist_by_bw(struct net_device *dev, void __user *pointer)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_active_chanlist list;
	uint8_t chanlist[IEEE80211_CHAN_BYTES];
	int i;
	int j;
	int nchan = 0;

	memset(chanlist, 0, sizeof(chanlist));

	if (copy_from_user(&list, pointer, sizeof(list)))
		return -EFAULT;

	if ((ic->ic_phytype == IEEE80211_T_DS) || (ic->ic_phytype == IEEE80211_T_OFDM))
		i = 1;
	else
		i = 0;

	for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
		if (isset(list.channels, j) && isset(ic->ic_chan_avail, i) &&
				!local_is_channel_disabled(ic, i, list.bw)) {
			setbit(chanlist, i);
			nchan++;
		}
	}

	if (nchan == 0)
		return -EINVAL;

	switch (list.bw) {
	case BW_HT80:
		memcpy(ic->ic_chan_active_80, chanlist, sizeof(ic->ic_chan_active_80));
		break;
	case BW_HT40:
		memcpy(ic->ic_chan_active_40, chanlist, sizeof(ic->ic_chan_active_40));
		break;
	case BW_HT20:
		memcpy(ic->ic_chan_active_20, chanlist, sizeof(ic->ic_chan_active_20));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
ieee80211_subioctl_get_sta_tput_caps(struct net_device *dev,
				struct ieee8011req_sta_tput_caps __user* ps, int cnt)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = NULL;
	struct ieee8011req_sta_tput_caps tput_caps;

	if (!ps) {
		printk("%s: NULL pointer for user request\n", __FUNCTION__);
		return -EFAULT;
	}

	if (sizeof(tput_caps) > cnt) {
		printk("%s: return buffer is too small\n", __FUNCTION__);
		return -EFAULT;
	}

	if (copy_from_user(&tput_caps, ps, sizeof(tput_caps))) {
		printk("%s: copy_from_user failed\n", __FUNCTION__);
		return -EIO;
	}

	ni = ieee80211_find_node(&ic->ic_sta, tput_caps.macaddr);
	if (!ni) {
		tput_caps.mode = IEEE80211_WIFI_MODE_NONE;
		printk("%s: station %pM not found\n", __FUNCTION__, tput_caps.macaddr);
		goto copy_and_exit;
	}

	COMPILE_TIME_ASSERT(sizeof(ni->ni_ie_htcap) == sizeof(tput_caps.htcap_ie) &&
			sizeof(ni->ni_ie_vhtcap) == sizeof(tput_caps.vhtcap_ie));

	tput_caps.mode = ni->ni_wifi_mode;
	if (IEEE80211_NODE_IS_VHT(ni)) {
		memcpy(tput_caps.htcap_ie, &ni->ni_ie_htcap, sizeof(tput_caps.htcap_ie));
		memcpy(tput_caps.vhtcap_ie, &ni->ni_ie_vhtcap, sizeof(tput_caps.vhtcap_ie));
	} else if (IEEE80211_NODE_IS_HT(ni)) {
		memcpy(tput_caps.htcap_ie, &ni->ni_ie_htcap, sizeof(tput_caps.htcap_ie));
	}
	ieee80211_free_node(ni);
copy_and_exit:
	if (copy_to_user(ps, &tput_caps, sizeof(tput_caps))) {
		printk("%s: copy_to_user failed\n", __FUNCTION__);
		return -EIO;
	}
	return 0;
}

static int
ieee80211_subioctl_get_chan_power_table(struct net_device *dev, void __user *pointer, uint32_t len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *c = NULL;
	struct ieee80211_chan_power_table power_table;
	int iter;
	int idx_bf;
	int idx_ss;

	if (copy_from_user(&power_table, pointer, sizeof(power_table)))
		return -EFAULT;

	for (iter = 0; iter < ic->ic_nchans; iter++) {
		c = ic->ic_channels + iter;
		if (c->ic_ieee == power_table.chan_ieee) {
			break;
		}
	}

	if (iter >= ic->ic_nchans || c == NULL) {
		/*
		 * Didn't find this channel, set the channel as
		 * 0 to indicate there is no such channel
		 */
		power_table.chan_ieee = 0;
	} else {
		for (idx_bf = PWR_IDX_BF_OFF; idx_bf < PWR_IDX_BF_MAX; idx_bf++) {
			for (idx_ss = PWR_IDX_1SS; idx_ss < PWR_IDX_SS_MAX; idx_ss++) {
				power_table.maxpower_table[idx_bf][idx_ss][PWR_IDX_20M] =
						c->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_20M];
				power_table.maxpower_table[idx_bf][idx_ss][PWR_IDX_40M] =
						c->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_40M];
				power_table.maxpower_table[idx_bf][idx_ss][PWR_IDX_80M] =
						c->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_80M];
			}
		}
	}

	if (copy_to_user(pointer, &power_table, len))
		return -EINVAL;

	return 0;
}

static int
ieee80211_subioctl_set_chan_power_table(struct net_device *dev, void __user *pointer)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *c;
	struct ieee80211_chan_power_table power_table;
	int8_t *s_pwrs;
	int8_t *d_pwrs;
	int iter;
	int cur_bw;
	int idx_bf;
	int idx_ss;

	if (copy_from_user(&power_table, pointer, sizeof(power_table)))
		return -EFAULT;

	for (iter = 0; iter < ic->ic_nchans; iter++) {
		c = ic->ic_channels + iter;
		if (c->ic_ieee == power_table.chan_ieee) {
			for (idx_bf = PWR_IDX_BF_OFF; idx_bf < PWR_IDX_BF_MAX; idx_bf++) {
				for (idx_ss = PWR_IDX_1SS; idx_ss < PWR_IDX_SS_MAX; idx_ss++) {
					s_pwrs = power_table.maxpower_table[idx_bf][idx_ss];
					d_pwrs = c->ic_maxpower_table[idx_bf][idx_ss];
					d_pwrs[PWR_IDX_20M] = s_pwrs[PWR_IDX_20M];
					d_pwrs[PWR_IDX_40M] = s_pwrs[PWR_IDX_40M];
					d_pwrs[PWR_IDX_80M] = s_pwrs[PWR_IDX_80M];
					/*
					 * Update the maxpower of current bandwidth
					 */
					if (idx_bf == PWR_IDX_BF_OFF && idx_ss == PWR_IDX_1SS) {
						cur_bw = ieee80211_get_bw(ic);
						switch (cur_bw) {
						case BW_HT20:
							c->ic_maxpower = s_pwrs[PWR_IDX_20M];
							break;
						case BW_HT40:
							c->ic_maxpower = s_pwrs[PWR_IDX_40M];
							break;
						case BW_HT80:
							c->ic_maxpower = s_pwrs[PWR_IDX_80M];
							break;
						}
					}
				}
			}
			if (ic->ic_power_table_update) {
				ic->ic_power_table_update(vap, c);
			}
			break;
		}
	}

	return 0;
}

static void
get_txrx_airtime_ioctl(void *s, struct ieee80211_node *ni)
{
	struct node_txrx_airtime __user *u_airtime;
	struct iwreq *iwr = (struct iwreq *)s;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct node_txrx_airtime airtime;
	uint16_t __user *u_nr_nodes;
	struct txrx_airtime *txrxat;
	uint16_t nr_nodes;

	airtime.node_index = IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx);
	memcpy(airtime.macaddr, ni->ni_macaddr, IEEE80211_ADDR_LEN);
	airtime.tx_airtime = ic->ic_tx_airtime(ni);
	airtime.tx_airtime_accum = ic->ic_tx_accum_airtime(ni);
	airtime.rx_airtime = ic->ic_rx_airtime(ni);
	airtime.rx_airtime_accum = ic->ic_rx_accum_airtime(ni);

	txrxat = (struct txrx_airtime *)iwr->u.data.pointer;

	txrxat->total_tx_airtime += airtime.tx_airtime_accum;
	txrxat->total_rx_airtime += airtime.rx_airtime_accum;

        txrxat = (struct txrx_airtime *)iwr->u.data.pointer;
        u_nr_nodes = &txrxat->nr_nodes;

	if (copy_from_user(&nr_nodes, u_nr_nodes, sizeof(nr_nodes)))
		return;

	u_airtime = txrxat->nodes + nr_nodes;

	if (nr_nodes++ > QTN_ASSOC_LIMIT - 1)
		return;

	if (copy_to_user(u_airtime, &airtime, sizeof(airtime)))
		return;

	if (copy_to_user(u_nr_nodes, &nr_nodes, sizeof(nr_nodes)))
		return;
}

static int
ieee80211_subioctl_set_sec_chan(struct net_device *dev,
		void __user *pointer, uint32_t len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	int chan_off_pair[2];
	int chan_ieee;
	int offset;

	if (vap->iv_opmode != IEEE80211_M_HOSTAP)
		return -EOPNOTSUPP;

	if (copy_from_user(chan_off_pair, pointer, sizeof(chan_off_pair)))
		return -EFAULT;

	chan_ieee = chan_off_pair[0];
	offset = chan_off_pair[1];
	if (!ieee80211_dual_sec_chan_supported(vap, chan_ieee))
		return -EINVAL;

	chan = findchannel(ic, chan_ieee, ic->ic_des_mode);
	if (!chan)
		return -EINVAL;

	offset = (offset == 0) ? IEEE80211_HTINFO_CHOFF_SCA : IEEE80211_HTINFO_CHOFF_SCB;
	ieee80211_update_sec_chan_offset(chan, offset);

	if (chan_ieee == ic->ic_curchan->ic_ieee) {
		if (vap->iv_state == IEEE80211_S_RUN)
			ic->ic_set_channel(ic);
		ieee80211_wireless_reassoc(vap, 0, 1);
	}

	return 0;
}

static int
ieee80211_subioctl_get_sec_chan(struct net_device *dev,
		void __user *pointer, uint32_t len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	int cur_bw = ieee80211_get_bw(ic);
	int chan_off_pair[2];
	int chan_ieee;
	int offset;

	if (copy_from_user(chan_off_pair, pointer, sizeof(chan_off_pair)))
		return -EFAULT;

	chan_ieee = chan_off_pair[0];
	offset = chan_ieee;
	chan = findchannel(ic, chan_ieee, ic->ic_des_mode);
	if (!chan)
		return -EINVAL;

	if (cur_bw >= BW_HT40) {
		if (chan->ic_flags & IEEE80211_CHAN_HT40U)
			offset = chan_ieee + IEEE80211_SEC_CHAN_OFFSET;
		else if (chan->ic_flags & IEEE80211_CHAN_HT40D)
			offset = chan_ieee - IEEE80211_SEC_CHAN_OFFSET;
	}
	chan_off_pair[1] = offset;

	if (copy_to_user(pointer, chan_off_pair, sizeof(chan_off_pair)))
		return -EFAULT;

	return 0;
}

static int
ieee80211_subioctl_get_txrx_airtime(struct net_device *dev, struct iwreq *iwr)
{
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic  = vap->iv_ic;
	struct txrx_airtime *txrxat;
	uint16_t __user *u_fat;
	uint16_t fat;

	txrxat = (struct txrx_airtime *)iwr->u.data.pointer;
	u_fat = &txrxat->free_airtime;

	if (copy_from_user(&fat, u_fat, sizeof(fat)))
		return -EFAULT;
	fat = sp->free_airtime;
	copy_to_user(u_fat, &fat, sizeof(fat));

	ic->ic_iterate_nodes(&ic->ic_sta, get_txrx_airtime_ioctl, iwr, 1);

	return 0;
}

static int
ieee80211_subioctl_get_chan_pri_inact(struct net_device *dev,
		void __user *pointer, uint32_t len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_inactive_chanlist chanlist;

	if (sizeof(chanlist) > len)
		return -ENOMEM;

	memset(&chanlist, 0, sizeof(chanlist));

	local_get_inactive_primary_chan_num(ic, &chanlist);

	if (copy_to_user(pointer, &chanlist, sizeof(chanlist)) != 0)
		return -EFAULT;

	return 0;
}

static int
ieee80211_ioctl_di_dfs_channels(struct net_device *dev, void __user *pointer, int cnt)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	int flag_deactive;

	if (copy_from_user(&flag_deactive, pointer, sizeof(int)))
		return -EFAULT;

	ic->ic_dfs_channels_deactive = !!flag_deactive;

	return 0;
}

#define MAX_CLIENT_LIST 200
static int
ieee80211_subioctl_get_client_mac_list(struct net_device *dev,
		void __user *pointer, uint32_t len)
{
	int rval = 0;
	uint32_t num_of_entries = 0;
	struct ieee80211_mac_list mlist;
	uint32_t flags = 0;

	if (copy_from_user(&mlist, pointer, sizeof(mlist)) != 0)
		return -EFAULT;

	rval = fwt_db_get_macs_behind_node(mlist.num_entries, &num_of_entries, MAX_CLIENT_LIST,
					&flags,	(uint8_t *)&mlist.macaddr[0]);
	mlist.num_entries = num_of_entries;
	mlist.flags = flags;

	if (copy_to_user(pointer, &mlist, sizeof(mlist)) != 0)
		return -EFAULT;

	return rval;
}

static int
ieee80211_set_nss_cap(struct ieee80211vap *vap, const int param, const int nss)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (nss < 1 || nss > QTN_GLOBAL_RATE_NSS_MAX)
		return EINVAL;

	if (ieee80211_swfeat_is_supported(SWFEAT_ID_2X2, 0) ||
			ieee80211_swfeat_is_supported(SWFEAT_ID_2X4, 0)) {
		if (nss > QTN_2X2_GLOBAL_RATE_NSS_MAX) {
			return EINVAL;
		}
	} else if (ieee80211_swfeat_is_supported(SWFEAT_ID_3X3, 0)) {
		if (nss > QTN_3X3_GLOBAL_RATE_NSS_MAX) {
			return EINVAL;
		}
	}

	if (param == IEEE80211_PARAM_HT_NSS_CAP)
		ic->ic_ht_nss_cap = nss;
	else if (param == IEEE80211_PARAM_VHT_NSS_CAP) {
		ic->ic_vht_nss_cap = nss;
		ic->ic_vhtcap.numsounding = nss -1;
		ic->ic_vhtcap_24g.numsounding = nss -1;
	} else
		return EINVAL;

	ieee80211_param_to_qdrv(vap, param, nss, NULL, 0);
	ieee80211_wireless_reassoc(vap, 0, 1);

	return 0;
}

#if defined(QBMPS_ENABLE)
int ieee80211_wireless_set_sta_bmps(struct ieee80211vap *vap, struct ieee80211com *ic,
						int value)
{
	struct qdrv_vap *qv = container_of(vap, struct qdrv_vap, iv);

	/* valid setting is: 0 - disable */
	/*                   1 - manual mode */
	/*                   2 - auto mode */
	if ((unsigned)value > BMPS_MODE_AUTO)
		return EINVAL;

	/* BMPS power-saving only works in STA mode */
	if (vap->iv_opmode != IEEE80211_M_STA)
		return EINVAL;

	if (qv->qv_bmps_mode == value)
		return 0;

	if (value == 0) {
		/* disable power-saving */
		ic->ic_flags_qtn &= ~IEEE80211_QTN_BMPS;
	} else {
		/* enable power-saving */
		ic->ic_flags_qtn |= IEEE80211_QTN_BMPS;
	}

	if (qv->qv_bmps_mode == BMPS_MODE_AUTO) {
		/* stop tput measurement if previously in auto mode */
		del_timer(&ic->ic_bmps_tput_check.tput_timer);
	}

	/* exit power-saving first if previously power-saving is enabled */
	if (qv->qv_bmps_mode != BMPS_MODE_OFF)
		pm_qos_update_requirement(PM_QOS_POWER_SAVE,
						BOARD_PM_GOVERNOR_WLAN,
						BOARD_PM_LEVEL_NO);

	qv->qv_bmps_mode = value;

	if (vap->iv_state == IEEE80211_S_RUN) {

		/* update null frame */
		ieee80211_sta_bmps_update(vap);

		if (value == BMPS_MODE_MANUAL) {
			/* manual mode, start power-saving immediately */
			/* only when there is only one STA VAP and no SWBMISS */
			if (ieee80211_is_idle_state(ic))
				pm_qos_update_requirement(PM_QOS_POWER_SAVE,
							BOARD_PM_GOVERNOR_WLAN,
							BOARD_PM_LEVEL_IDLE);
		} else if (value == BMPS_MODE_AUTO){
			/* auto mode, start tput measurement */
			vap->iv_bmps_tput_high = -1;
			ic->ic_bmps_tput_check.tput_timer.expires = jiffies +
						(BMPS_TPUT_MEASURE_PERIOD_MS / 1000) * HZ;
			add_timer(&ic->ic_bmps_tput_check.tput_timer);
		}
	}

	return 0;

}
EXPORT_SYMBOL(ieee80211_wireless_set_sta_bmps);
#endif

static int
ieee80211_subioctl_set_dscp2tid_map(struct net_device *dev, void __user *pointer, uint16_t len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct qdrv_vap *qv = container_of(vap, struct qdrv_vap, iv);
	uint8_t dscp2tid[IP_DSCP_NUM];

	if (len != IP_DSCP_NUM)
		return -EINVAL;

	if (copy_from_user(dscp2tid, pointer, sizeof(dscp2tid)))
		return -EFAULT;

	if (!ic->ic_set_dscp2tid_map)
		return -EINVAL;

	ic->ic_set_dscp2tid_map(qv->qv_vap_idx, dscp2tid);

	return 0;
}

static int
ieee80211_set_vht_mcs_cap(struct ieee80211vap *vap, const int mcs)
{
	struct ieee80211com *ic = vap->iv_ic;

	switch(mcs) {
	case IEEE80211_VHT_MCS_0_7:
	case IEEE80211_VHT_MCS_0_8:
	case IEEE80211_VHT_MCS_0_9:
		ic->ic_vht_mcs_cap = mcs;
		break;
	default:
		return EINVAL;
	}

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_VHT_MCS_CAP, mcs, NULL, 0);
	ieee80211_wireless_reassoc(vap, 0, 1);

	return 0;
}

static int
ieee80211_subioctl_get_dscp2tid_map(struct net_device *dev, void __user *pointer, uint16_t len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct qdrv_vap *qv = container_of(vap, struct qdrv_vap, iv);
	uint8_t dscp2tid[IP_DSCP_NUM];

	if (len != IP_DSCP_NUM)
		return -EINVAL;

	if (!ic->ic_get_dscp2tid_map)
		return -EINVAL;

	ic->ic_get_dscp2tid_map(qv->qv_vap_idx, dscp2tid);

	if(copy_to_user(pointer, dscp2tid, len))
		return -EFAULT;

	return 0;
}

static int
ieee80211_param_wowlan_set(struct net_device *dev, struct ieee80211vap *vap, u_int32_t value)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint16_t cmd =	value >> 16;
	uint16_t arg = value & 0xffff;

	if (cmd >= IEEE80211_WOWLAN_SET_MAX) {
		printk(KERN_WARNING "%s: WOWLAN set: invalid config cmd %u, arg=%u\n",
				dev->name, cmd, arg);
		return -1;
	}

	switch (cmd) {
	case IEEE80211_WOWLAN_HOST_POWER_SAVE:
		if (arg > 1)
			return -1;
		if (ic->ic_wowlan.host_state != arg) {
			ic->ic_wowlan.host_state = arg;
			g_wowlan_host_state = arg;

			/* trigger WLAN manual mode STA power-saving */
#if defined(QBMPS_ENABLE)
			if (ic->ic_wowlan.host_state)
				ieee80211_wireless_set_sta_bmps(vap, ic, 1);
			else
				ieee80211_wireless_set_sta_bmps(vap, ic, 0);
#endif
		}
		break;
	case IEEE80211_WOWLAN_MATCH_TYPE:
		if (arg > 2)
			return -1;
		if (ic->ic_wowlan.wowlan_match != arg) {
			ic->ic_wowlan.wowlan_match = arg;
			g_wowlan_match_type = arg;
		}
		break;
	case IEEE80211_WOWLAN_L2_ETHER_TYPE:
		if (arg < 0x600)
			return -1;
		if (ic->ic_wowlan.L2_ether_type != arg) {
			ic->ic_wowlan.L2_ether_type = arg;
			g_wowlan_l2_ether_type = arg;
		}
		break;
	case IEEE80211_WOWLAN_L3_UDP_PORT:
		if (ic->ic_wowlan.L3_udp_port != arg) {
			ic->ic_wowlan.L3_udp_port = arg;
			g_wowlan_l3_udp_port = arg;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void
ieee80211_ioctl_swfeat_disable(struct ieee80211vap *vap, const int param, int feat)
{
	if (feat >= SWFEAT_ID_MAX)
		printk("%s: feature id %d is invalid\n", __func__, feat);
	else
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_SWFEAT_DISABLE, feat, NULL, 0);
}

void ieee80211_send_vht_opmode_to_all(struct ieee80211com *ic, uint8_t bw)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni && (ni != ni->ni_vap->iv_bss) && IEEE80211_NODE_IS_VHT(ni)) {
			ieee80211_send_vht_opmode_action(ni->ni_vap, ni, bw, 3);
		}
	}
}

static int
ieee80211_ioctl_set_assoc_limit(struct ieee80211com *ic,
				struct ieee80211vap *vap, int value)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	int i;

	if ((vap->iv_opmode != IEEE80211_M_HOSTAP) &&
			!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
		return EINVAL;
	}

	if (value < 0  || value > QTN_ASSOC_LIMIT) {
		printk("%s: Max configurable limit is %d\n",
					__func__, QTN_ASSOC_LIMIT);
		return EINVAL;
	}

	ic->ic_sta_assoc_limit = value;

	for (i = 0; i < IEEE80211_MAX_BSS_GROUP; i++) {
		ic->ic_ssid_grp[i].limit = ic->ic_sta_assoc_limit;
		ic->ic_ssid_grp[i].reserve = 0;
	}

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		ieee80211_wireless_reassoc(ni->ni_vap, 0, 0);
	}

	return 0;
}

static int
ieee80211_ioctl_set_bss_grp_assoc_limit(struct ieee80211com *ic,
					struct ieee80211vap *vap, int value)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	int limit, grp;

	if ((vap->iv_opmode != IEEE80211_M_HOSTAP) &&
			!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
		return EINVAL;
	}

	limit = (value & 0xffff);
	grp = ((value >> 16) & 0xffff);

	if (grp < IEEE80211_MIN_BSS_GROUP || grp >= IEEE80211_MAX_BSS_GROUP) {
		return EINVAL;
	}

	if (limit < 0 || limit > ic->ic_sta_assoc_limit
			|| limit < ic->ic_ssid_grp[grp].reserve) {
		return EINVAL;
	}

	ic->ic_ssid_grp[grp].limit = limit;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_vap->iv_ssid_group == grp) {
			ieee80211_wireless_reassoc(ni->ni_vap, 0, 0);
		}
	}

	return 0;
}

static int
ieee80211_ioctl_set_bss_grpid(struct ieee80211com *ic,
				struct ieee80211vap *vap, int value)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;

	if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
		return EINVAL;
	}

	if (value < IEEE80211_MIN_BSS_GROUP || value >= IEEE80211_MAX_BSS_GROUP) {
		return EINVAL;
	}

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		ieee80211_wireless_reassoc(ni->ni_vap, 0, 0);
	}

	vap->iv_ssid_group = value;

	return 0;
}

static int
ieee80211_ioctl_set_bss_grp_assoc_reserve(struct ieee80211com *ic,
					struct ieee80211vap *vap, int value)
{
	struct ieee80211_node *ni;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	int grp;
	int reserve;
	int tot_reserve = 0;
	int i;

	if ((vap->iv_opmode != IEEE80211_M_HOSTAP) &&
			!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
		return EINVAL;
	}

	reserve = (value & 0xffff);
	grp = ((value >> 16) & 0xffff);

	if (grp < IEEE80211_MIN_BSS_GROUP || grp >= IEEE80211_MAX_BSS_GROUP) {
		return EINVAL;
	}

	tot_reserve = reserve;
	for (i = IEEE80211_MIN_BSS_GROUP; i < IEEE80211_MAX_BSS_GROUP; i++) {
		if (i == grp)
			continue;
		tot_reserve += ic->ic_ssid_grp[i].reserve;
	}

	if (reserve > ic->ic_ssid_grp[grp].limit
			|| tot_reserve > ic->ic_sta_assoc_limit) {
		return EINVAL;
	}

	ic->ic_ssid_grp[grp].reserve = reserve;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		ieee80211_wireless_reassoc(ni->ni_vap, 0, 0);
	}

	return 0;
}

static int
ieee80211_ioctl_extender_role(struct ieee80211com *ic, struct ieee80211vap *vap, int value)
{
	struct ieee80211vap *vap_tmp;

	if (!ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
		return EOPNOTSUPP;

	if (ic->ic_extender_role == value)
		return 0;

	ic->ic_extender_role = value;
	switch(ic->ic_extender_role) {
	case IEEE80211_EXTENDER_ROLE_RBS:
	case IEEE80211_EXTENDER_ROLE_MBS:
	case IEEE80211_EXTENDER_ROLE_NONE:
		if (ic->ic_extender_role == IEEE80211_EXTENDER_ROLE_MBS)
			IEEE80211_ADDR_COPY(ic->ic_extender_mbs_bssid, ic->ic_myaddr);
		else
			IEEE80211_ADDR_SET_NULL(ic->ic_extender_mbs_bssid);
		ic->ic_extender_rbs_num = 0;
		memset(ic->ic_extender_rbs_bssid[0], 0, sizeof(ic->ic_extender_rbs_bssid));

		if (vap->iv_opmode == IEEE80211_M_HOSTAP)
			ic->ic_beacon_update(vap);
		break;
	default:
		return EINVAL;
		break;
	}

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_EXTENDER_ROLE, value, NULL, 0);

	/* change mode of existing WDS links */
	TAILQ_FOREACH(vap_tmp, &ic->ic_vaps, iv_next) {
		if (!IEEE80211_VAP_WDS_ANY(vap_tmp))
			continue;
		ieee80211_vap_wds_mode_change(vap_tmp);
	}

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		ieee80211_extender_cleanup_wds_link(vap);
		if (ic->ic_extender_role == IEEE80211_EXTENDER_ROLE_RBS)
			mod_timer(&ic->ic_extender_scan_timer, jiffies);
	}

	return 0;
}

static int ieee80211_ioctl_set_vap_pri(struct ieee80211com *ic, struct ieee80211vap *vap, int value)
{
	if (!ieee80211_swfeat_is_supported(SWFEAT_ID_QTM_PRIO, 1))
		return EOPNOTSUPP;

	if (value >= QTN_VAP_PRIORITY_NUM)
		return EINVAL;

	vap->iv_pri = value;

	IEEE80211_LOCK_IRQ(ic);

	ieee80211_adjust_wme_by_vappri(ic);

	IEEE80211_UNLOCK_IRQ(ic);

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_VAP_PRI, value, NULL, 0);

	return 0;
}

static int ieee80211_wireless_set_mu_tx_rate(struct ieee80211vap *vap, struct ieee80211com *ic,
						const int value)
{
	int mcs1 = value & IEEE80211_AC_MCS_MASK;
	int mcs2 = (value >> IEEE80211_AC_MCS_SHIFT) & IEEE80211_AC_MCS_MASK;
	int mcs_to_muc1 = ieee80211_11ac_mcs_format(mcs1, 80);
	int mcs_to_muc2 = ieee80211_11ac_mcs_format(mcs2, 80);
	int mcs_to_muc;

	if (!ieee80211_swfeat_is_supported(SWFEAT_ID_MU_MIMO, 1))
		return EOPNOTSUPP;

	if (!IS_IEEE80211_VHT_ENABLED(ic))
		return EINVAL;

	if ((mcs_to_muc1 < 0) || (mcs_to_muc2 < 0))
		return EINVAL;

	mcs_to_muc = (mcs_to_muc2 << IEEE80211_AC_MCS_SHIFT) | mcs_to_muc1;

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_FIXED_11AC_MU_TX_RATE, mcs_to_muc, NULL, 0);

	return 0;
}

static void ieee80211_wireless_set_40mhz_intolerant(struct ieee80211vap *vap,
		                                    int intol)
{
	struct ieee80211com *ic = vap->iv_ic;

	/*
	 * 80211 specifies 5G STA shall set 40MHz intolerant to 0
	 */
	if (!ic->ic_curchan->ic_flags & IEEE80211_CHAN_2GHZ)
		return;

	if (!!intol == !!(vap->iv_coex & WLAN_20_40_BSS_COEX_40MHZ_INTOL) &&
	    !!intol == !!(ic->ic_htcap.cap & IEEE80211_HTCAP_C_40_INTOLERANT))
		return;

	if (intol) {
		vap->iv_coex |= WLAN_20_40_BSS_COEX_40MHZ_INTOL;
		ic->ic_htcap.cap |= IEEE80211_HTCAP_C_40_INTOLERANT;
	} else {
		vap->iv_coex &= ~WLAN_20_40_BSS_COEX_40MHZ_INTOL;
		ic->ic_htcap.cap &= ~IEEE80211_HTCAP_C_40_INTOLERANT;
	}

	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, BW_HT20, NULL, 0);

	if (vap->iv_opmode == IEEE80211_M_STA)
		ieee80211_send_20_40_bss_coex(vap);
}

static void
local_weather_channel_check(struct net_device *dev)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan;

	if (vap->iv_opmode != IEEE80211_M_HOSTAP)
		return;

	if (ic->ic_curchan == NULL || ic->ic_curchan == IEEE80211_CHAN_ANYC)
		return;

	if (ieee80211_is_on_weather_channel(ic, ic->ic_curchan)) {
		printk("Weather channel %d is not allowed during boot time, switching to a new one\n",
					ic->ic_curchan->ic_ieee);
		chan = ieee80211_find_channel_by_ieee(ic, IEEE80211_DEFAULT_5_GHZ_CHANNEL);
		if (!chan)
			return;
		ic->ic_curchan = chan;
		ic->ic_des_chan = chan;
		ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
		ic->ic_set_channel(ic);
		ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	}
}

static int
ieee80211_ioctl_setparam(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_rsnparms *rsn = NULL;
	int *i = (int *) extra;
	int param = i[0];		/* parameter id is 1st */
	int value = i[1];		/* NB: most values are TYPE_INT */
	int temp_value;
	int retv = 0;
	int j, caps;
	const struct ieee80211_authenticator *auth;
	const struct ieee80211_aclator *acl;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct ieee80211vap *vap_each;
	struct ieee80211vap *vap_tmp;
	struct ieee80211vap *first_vap;

	if (vap->iv_bss)
		rsn = &vap->iv_bss->ni_rsn;

	switch (param) {
	case IEEE80211_PARAM_AP_ISOLATE:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP)
			br_set_ap_isolate(value);
		else
			return -EINVAL;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_AUTHMODE:
		if (!vap->iv_bss)
			return -EFAULT;

		switch (value) {
		case IEEE80211_AUTH_WPA:	/* WPA */
		case IEEE80211_AUTH_8021X:	/* 802.1x */
		case IEEE80211_AUTH_OPEN:	/* open */
		case IEEE80211_AUTH_SHARED:	/* shared-key */
		case IEEE80211_AUTH_AUTO:	/* auto */
			auth = ieee80211_authenticator_get(value);
			if (auth == NULL)
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}
		switch (value) {
		case IEEE80211_AUTH_WPA:	/* WPA w/ 802.1x */
			value = IEEE80211_AUTH_8021X;
			break;
		case IEEE80211_AUTH_OPEN:	/* open */
			vap->iv_flags &= ~(IEEE80211_F_WPA);
			break;
		case IEEE80211_AUTH_SHARED:	/* shared-key */
		case IEEE80211_AUTH_AUTO:	/* auto */
		case IEEE80211_AUTH_8021X:	/* 802.1x */
			vap->iv_flags &= ~IEEE80211_F_WPA;
			break;
		}
		/* NB: authenticator attach/detach happens on state change */
		vap->iv_bss->ni_authmode = value;
		/* XXX mixed/mode/usage? */
		vap->iv_auth = auth;
		vap->iv_osen = 0;
		retv = ENETRESET;
		break;
	case IEEE80211_PARAM_PROTMODE:
		if (value > IEEE80211_PROT_RTSCTS)
			return -EINVAL;
		ic->ic_protmode = value;
		/* NB: if not operating in 11g this can wait */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan))
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_MCASTCIPHER:
		if ((vap->iv_caps & cipher2cap(value)) == 0 &&
		    !ieee80211_crypto_available(value))
			return -EINVAL;
		if (!rsn)
			return -EFAULT;
		if (!WPA_TKIP_SUPPORT) {
			if (value == IEEE80211_CIPHER_AES_CCM)
				rsn->rsn_mcastcipher = value;
			else
				printk("%s: invalid cipher %d ignored\n", __FUNCTION__, value);
		} else {
			rsn->rsn_mcastcipher = value;
		}
		if (vap->iv_flags & IEEE80211_F_WPA)
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_MCASTKEYLEN:
		if (!(0 < value && value <= IEEE80211_KEYBUF_SIZE))
			return -EINVAL;
		if (!rsn)
			return -EFAULT;
		/* XXX no way to verify driver capability */
		rsn->rsn_mcastkeylen = value;
		if (vap->iv_flags & IEEE80211_F_WPA)
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_UCASTCIPHERS:
		if (!rsn)
			return -EFAULT;
		/*
		 * NB: this logic intentionally ignores unknown and
		 * unsupported ciphers so folks can specify 0xff or
		 * similar and get all available ciphers.
		 */
		/* caps are really ciphers */
		caps = 0;
		for (j = 1; j < 32; j++)	/* NB: skip WEP */
			if ((value & (1 << j)) &&
			    ((vap->iv_caps & cipher2cap(j)) ||
			     ieee80211_crypto_available(j)))
				caps |= 1 << j;
		if (caps == 0)			/* nothing available */
			return -EINVAL;
		/* XXX verify ciphers ok for unicast use? */
		/* XXX disallow if running as it'll have no effect */
		rsn->rsn_ucastcipherset = caps;
		if (vap->iv_flags & IEEE80211_F_WPA)
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_UCASTCIPHER:
		if ((vap->iv_caps & cipher2cap(value)) == 0 &&
		    !ieee80211_crypto_available(value))
			return -EINVAL;
		if (!rsn)
			return -EFAULT;

		rsn->rsn_ucastcipher = value;
		if (vap->iv_flags & IEEE80211_F_WPA)
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_UCASTKEYLEN:
		if (!(0 < value && value <= IEEE80211_KEYBUF_SIZE))
			return -EINVAL;
		if (!rsn)
			return -EFAULT;
		/* XXX no way to verify driver capability */
		rsn->rsn_ucastkeylen = value;
		break;
	case IEEE80211_PARAM_KEYMGTALGS:
		if (!rsn)
			return -EFAULT;
		/*
		 * Map supplcant values to RSN values. Included only the currently
		 * used mappings. But, need to increases the cases as we support more.
		 * */
		switch (value) {
			case WPA_KEY_MGMT_PSK:
				rsn->rsn_keymgmtset = RSN_ASE_8021X_PSK;
				break;
			case WPA_KEY_MGMT_NONE:
				rsn->rsn_keymgmtset = RSN_ASE_NONE;
				break;
			case WPA_KEY_MGMT_IEEE8021X_SHA256:
				rsn->rsn_keymgmtset = RSN_ASE_8021X_SHA256;
				break;
			case WPA_KEY_MGMT_PSK_SHA256:
				rsn->rsn_keymgmtset = RSN_ASE_8021X_PSK_SHA256;
				break;
			default:
				rsn->rsn_keymgmtset = value;
				break;
		}
		if (vap->iv_flags & IEEE80211_F_WPA)
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_RSNCAPS:
		if (!rsn)
			return -EFAULT;
		/* XXX check */
		rsn->rsn_caps = value;
		if (vap->iv_flags & IEEE80211_F_WPA)
			retv = ENETRESET;
		break;
	case IEEE80211_PARAM_WPA:
		if (value > 3)
			return -EINVAL;
		/* XXX verify ciphers available */
		vap->iv_flags &= ~IEEE80211_F_WPA;
		switch (value) {
		case 1:
			vap->iv_flags |= IEEE80211_F_WPA1;
			break;
		case 2:
			vap->iv_flags |= IEEE80211_F_WPA2;
			break;
		case 3:
			vap->iv_flags |= IEEE80211_F_WPA1 | IEEE80211_F_WPA2;
			break;
		}
		retv = ENETRESET;		/* XXX? */
		break;
	case IEEE80211_PARAM_ROAMING:
		if (!(IEEE80211_ROAMING_DEVICE <= value &&
		    value <= IEEE80211_ROAMING_MANUAL))
			return -EINVAL;
		ic->ic_roaming = value;
		break;
	case IEEE80211_PARAM_PRIVACY:
		if (value) {
			/* XXX check for key state? */
			vap->iv_flags |= IEEE80211_F_PRIVACY;
		} else
			vap->iv_flags &= ~IEEE80211_F_PRIVACY;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_DROPUNENCRYPTED:
		if (value)
			vap->iv_flags |= IEEE80211_F_DROPUNENC;
		else
			vap->iv_flags &= ~IEEE80211_F_DROPUNENC;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_DROPUNENC_EAPOL:
		if (value)
			IEEE80211_VAP_DROPUNENC_EAPOL_ENABLE(vap);
		else
			IEEE80211_VAP_DROPUNENC_EAPOL_DISABLE(vap);
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_COUNTERMEASURES:
		{
			int invoked = 0;
			int cleared = 0;
			static const char *tag = QEVT_COMMON_PREFIX;

			if (value) {
				if ((vap->iv_flags & IEEE80211_F_WPA) == 0)
					return -EINVAL;
				vap->iv_flags |= IEEE80211_F_COUNTERM;
				invoked = 1;
			} else {
				if (vap->iv_flags & IEEE80211_F_COUNTERM) {
					cleared = 1;
				}
				vap->iv_flags &= ~IEEE80211_F_COUNTERM;
			}
			if (invoked || cleared)
			{
				ieee80211_eventf(dev, "%sTKIP countermeasures %s", tag,
						 invoked ? "invoked" : "cleared");
			}
		}
		break;
	case IEEE80211_PARAM_DRIVER_CAPS:
		vap->iv_caps = value;		/* NB: for testing */
		break;
	case IEEE80211_PARAM_MACCMD:
		acl = vap->iv_acl;
		switch (value) {
		case IEEE80211_MACCMD_POLICY_OPEN:
		case IEEE80211_MACCMD_POLICY_ALLOW:
		case IEEE80211_MACCMD_POLICY_DENY:
			if (acl == NULL) {
				acl = ieee80211_aclator_get("mac");
				if (acl == NULL || !acl->iac_attach(vap))
					return -EINVAL;
				vap->iv_acl = acl;
			}
			acl->iac_setpolicy(vap, value);
			break;
		case IEEE80211_MACCMD_FLUSH:
			if (acl != NULL)
				acl->iac_flush(vap);
			/* NB: silently ignore when not in use */
			break;
		case IEEE80211_MACCMD_DETACH:
			if (acl != NULL) {
				vap->iv_acl = NULL;
				acl->iac_detach(vap);
			}
			break;
		}
		break;
	case IEEE80211_PARAM_WMM:
		if (ic->ic_caps & IEEE80211_C_WME){
			if (value) {
				vap->iv_flags |= IEEE80211_F_WME;
				vap->iv_ic->ic_flags |= IEEE80211_F_WME; /* XXX needed by ic_reset */
			} else {
				vap->iv_flags &= ~IEEE80211_F_WME;
				vap->iv_ic->ic_flags &= ~IEEE80211_F_WME; /* XXX needed by ic_reset */
			}
			retv = ENETRESET;	/* Renegotiate for capabilities */
		}
		break;
	case IEEE80211_PARAM_HIDESSID:
	{
		int	beacon_update_required= 0;

		if ((!!(vap->iv_flags & IEEE80211_F_HIDESSID)) ^ (!!value))
			beacon_update_required = 1;

		if (value)
			vap->iv_flags |= IEEE80211_F_HIDESSID;
		else
			vap->iv_flags &= ~IEEE80211_F_HIDESSID;

		if (beacon_update_required && (vap->iv_state & IEEE80211_S_RUN))
			ic->ic_beacon_update(vap);

	}
		break;
	case IEEE80211_PARAM_APBRIDGE:
		if (value == 0)
			vap->iv_flags |= IEEE80211_F_NOBRIDGE;
		else
			vap->iv_flags &= ~IEEE80211_F_NOBRIDGE;
		break;
	case IEEE80211_PARAM_INACT:
		vap->iv_inact_run = value / IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PARAM_INACT_AUTH:
		vap->iv_inact_auth = value / IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PARAM_INACT_INIT:
		vap->iv_inact_init = value / IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PARAM_DTIM_PERIOD:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP &&
		    vap->iv_opmode != IEEE80211_M_IBSS)
			return -EINVAL;
		if (IEEE80211_DTIM_MIN <= value &&
		    value <= IEEE80211_DTIM_MAX) {
			vap->iv_dtim_period = value;
			ic->ic_beacon_update(vap);
			retv = -EINVAL;		/* requires restart */
		} else
			retv = EINVAL;
		break;
	case IEEE80211_PARAM_BEACON_INTERVAL:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP &&
		    vap->iv_opmode != IEEE80211_M_IBSS)
			return -EINVAL;

		if (IEEE80211_BINTVAL_VALID(value)) {
			vap->iv_ic->ic_lintval_backup = value;
			ieee80211_beacon_interval_set(vap->iv_ic, value);
			retv = -EINVAL;
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_DOTH:
		if (value)
			ic->ic_flags |= IEEE80211_F_DOTH;
		else
			ic->ic_flags &= ~IEEE80211_F_DOTH;
		break;
	case IEEE80211_PARAM_SHPREAMBLE:
		if (value) {
			ic->ic_caps |= IEEE80211_C_SHPREAMBLE;
			ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
			ic->ic_flags &= ~IEEE80211_F_USEBARKER;
		} else {
			ic->ic_caps &= ~IEEE80211_C_SHPREAMBLE;
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
			ic->ic_flags |= IEEE80211_F_USEBARKER;
		}
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_PWRCONSTRAINT:
		{
			uint16_t pwr_constraint = (value & 0xffff);
			struct pwr_info_per_vap pwr;
			pwr.vap = vap;
			pwr.max_in_minpwr = -100; /* a value that never be real */

			if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
				if (vap->iv_opmode == IEEE80211_M_HOSTAP && ic->ic_bsschan != IEEE80211_CHAN_ANYC) {
					ieee80211_iterate_nodes(&ic->ic_sta, get_max_in_minpwr, &pwr, 1);
					if (pwr_constraint >= ic->ic_bsschan->ic_maxregpower) {
						printk("power constraint(%d) >= current channel max regulatory power(%d)\n", pwr_constraint, ic->ic_bsschan->ic_maxregpower);
						retv = EINVAL;
					}
					else if ((pwr.max_in_minpwr != -100) &&
							((ic->ic_bsschan->ic_maxregpower - pwr_constraint) < pwr.max_in_minpwr)) {
						printk("power constraint(%d) make local max transmit power(%d) less than the max value(%d) of min power in associated STAs\n",
								pwr_constraint,
								(ic->ic_bsschan->ic_maxregpower - pwr_constraint),
								pwr.max_in_minpwr);
						retv = EINVAL;
					}
					else {
						ic->ic_pwr_constraint = pwr_constraint;
						if (vap->iv_state == IEEE80211_S_RUN)
							ic->ic_beacon_update(vap);
					}
				} else {
					retv = EOPNOTSUPP;
				}
			} else {
					retv = EOPNOTSUPP;
			}
		}
		break;
	case IEEE80211_PARAM_GENREASSOC:
		if (!vap->iv_bss)
			return -EFAULT;

		IEEE80211_SEND_MGMT(vap->iv_bss, IEEE80211_FC0_SUBTYPE_REASSOC_REQ, 0);
		break;
	case IEEE80211_PARAM_REPEATER:
		if (IEEE80211_VAP_WDS_IS_RBS(vap) || IEEE80211_VAP_WDS_IS_MBS(vap)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
					"%s can't config repeater since "
					"it's a extender\n", vap->iv_dev->name);
			return -EINVAL;
		}

		first_vap = TAILQ_FIRST(&ic->ic_vaps);
		if (first_vap->iv_opmode != IEEE80211_M_STA)
			return -EINVAL;

		if (value) {
			ic->ic_flags_ext |= IEEE80211_FEXT_REPEATER;
			ieee80211_new_state(first_vap, IEEE80211_S_INIT, 0);
		} else {
			ic->ic_flags_ext &= ~IEEE80211_FEXT_REPEATER;
		}
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_REPEATER, value, NULL, 0);

		break;
	case IEEE80211_PARAM_WDS:
		if (value) {
			vap->iv_qtn_flags &= ~IEEE80211_QTN_BRIDGEMODE_DISABLED;
		} else {
			vap->iv_qtn_flags |= IEEE80211_QTN_BRIDGEMODE_DISABLED;
		}
		ieee80211_bridgemode_set(vap, 1);
		break;
	case IEEE80211_PARAM_BGSCAN:
		if (value) {
			if ((vap->iv_caps & IEEE80211_C_BGSCAN) == 0)
				return -EINVAL;
			vap->iv_flags |= IEEE80211_F_BGSCAN;
		} else {
			/* XXX racey? */
			vap->iv_flags &= ~IEEE80211_F_BGSCAN;
			ieee80211_cancel_scan(vap);	/* anything current */
		}
		break;
	case IEEE80211_PARAM_BGSCAN_IDLE:
		if (value >= IEEE80211_BGSCAN_IDLE_MIN)
			vap->iv_bgscanidle = msecs_to_jiffies(value);
		else
			retv = EINVAL;
		break;
	case IEEE80211_PARAM_BGSCAN_INTERVAL:
		if (value >= IEEE80211_BGSCAN_INTVAL_MIN) {
			vap->iv_bgscanintvl = value * HZ;
			ic->ic_extender_bgscanintvl = vap->iv_bgscanintvl;
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_SCAN_OPCHAN:
		ic->ic_scan_opchan_enable = !!(value);
		break;
	case IEEE80211_PARAM_EXTENDER_MBS_RSSI_MARGIN:
		if (ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
			ic->ic_extender_mbs_rssi_margin = value;
		break;
	case IEEE80211_PARAM_MCAST_RATE:
		/* units are in KILObits per second */
		if (value >= 256 && value <= 54000)
			vap->iv_mcast_rate = value;
		else
			retv = EINVAL;
		break;
	case IEEE80211_PARAM_COVERAGE_CLASS:
		if (value >= 0 && value <= IEEE80211_COVERAGE_CLASS_MAX) {
			ic->ic_coverageclass = value;
			if (IS_UP_AUTO(vap))
				ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
			retv = 0;
		} else
			retv = EINVAL;
		break;
	case IEEE80211_PARAM_COUNTRY_IE:
		if (value)
			ic->ic_flags_ext |= IEEE80211_FEXT_COUNTRYIE;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_COUNTRYIE;
		retv = ENETRESET;
		break;
	case IEEE80211_PARAM_REGCLASS:
		if (value)
			ic->ic_flags_ext |= IEEE80211_FEXT_REGCLASS;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_REGCLASS;
		retv = ENETRESET;
		break;
	case IEEE80211_PARAM_SCANVALID:
		vap->iv_scanvalid = value * HZ;
		break;
	case IEEE80211_PARAM_ROAM_RSSI_11A:
		vap->iv_roam.rssi11a = value;
		break;
	case IEEE80211_PARAM_ROAM_RSSI_11B:
		vap->iv_roam.rssi11bOnly = value;
		break;
	case IEEE80211_PARAM_ROAM_RSSI_11G:
		vap->iv_roam.rssi11b = value;
		break;
	case IEEE80211_PARAM_ROAM_RATE_11A:
		vap->iv_roam.rate11a = value;
		break;
	case IEEE80211_PARAM_ROAM_RATE_11B:
		vap->iv_roam.rate11bOnly = value;
		break;
	case IEEE80211_PARAM_ROAM_RATE_11G:
		vap->iv_roam.rate11b = value;
		break;
	case IEEE80211_PARAM_UAPSDINFO:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (ic->ic_caps & IEEE80211_C_UAPSD) {
				if (value)
					IEEE80211_VAP_UAPSD_ENABLE(vap);
				else
					IEEE80211_VAP_UAPSD_DISABLE(vap);
				retv = ENETRESET;
			}
		} else if (vap->iv_opmode == IEEE80211_M_STA) {
			vap->iv_uapsdinfo = value;
			IEEE80211_VAP_UAPSD_ENABLE(vap);
			retv = ENETRESET;
		}
		break;
	case IEEE80211_PARAM_SLEEP:
		if (!vap->iv_bss)
			return -EFAULT;

		/* XXX: Forced sleep for testing. Does not actually place the
		 *      HW in sleep mode yet. this only makes sense for STAs.
		 */
		if (value) {
			/* goto sleep */
			IEEE80211_VAP_GOTOSLEEP(vap);
		} else {
			/* wakeup */
			IEEE80211_VAP_WAKEUP(vap);
		}
		ieee80211_ref_node(vap->iv_bss);
		ieee80211_send_nulldata(vap->iv_bss);
		break;
	case IEEE80211_PARAM_TUNEPD:
		if (!vap->iv_bss)
			return -EFAULT;

		/* Send specified number of frames */
		for (j = 0; j < value; j++)
			ieee80211_send_tuning_data(vap->iv_bss);
		break;
	case IEEE80211_PARAM_QOSNULL:
		{
			struct ieee80211_node *ni_sta;
			if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
				/* Tx a QoS null frame to an STA by passing node_index */
				ni_sta = ieee80211_find_node_by_idx(ic, NULL, value);
				if (ni_sta) {
					ieee80211_send_qosnulldata(ni_sta, WMM_AC_BK);
				}
			} else {
				if (!vap->iv_bss)
					return -EFAULT;

				/* Force a QoS Null for testing. */
				ieee80211_ref_node(vap->iv_bss);
				ieee80211_send_qosnulldata(vap->iv_bss, value);
			}
		}
		break;
	case IEEE80211_PARAM_PSPOLL:
		if (!vap->iv_bss)
			return -EFAULT;

		/* Force a PS-POLL for testing. */
		ieee80211_send_pspoll(vap->iv_bss);
		break;
	case IEEE80211_PARAM_EOSPDROP:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (value)
				IEEE80211_VAP_EOSPDROP_ENABLE(vap);
			else
				IEEE80211_VAP_EOSPDROP_DISABLE(vap);
		}
		break;
	case IEEE80211_PARAM_MARKDFS:
		if (value)
			ic->ic_flags_ext |= IEEE80211_FEXT_MARKDFS;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_MARKDFS;

		/* set radar mode for qdrv */
		ic->ic_set_radar(value);
		break;
	case IEEE80211_PARAM_RADAR_BW:
		ic->ic_radar_bw = value;
		break;
	case IEEE80211_PARAM_STA_DFS:
		if (vap->iv_opmode == IEEE80211_M_STA) {
			if (value) {
				ic->ic_flags_ext |= IEEE80211_FEXT_MARKDFS;
			} else {
				ic->ic_flags_ext &= ~IEEE80211_FEXT_MARKDFS;
			}
			ic->ic_enable_sta_dfs(value);
		}
		break;
	case IEEE80211_PARAM_1BIT_PKT_DETECT:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_DYNAMIC_AC:
		if (ic->ic_rf_chipid == CHIPID_DUAL)
			return -EOPNOTSUPP;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_AMPDU_DENSITY:
		if (value < IEEE80211_AMPDU_MIN_DENSITY ||
		    value > IEEE80211_AMPDU_MAX_DENSITY) {
			return -EINVAL;
		} else {
			if (ic->ic_htcap.mpduspacing != value) {
				ic->ic_htcap.mpduspacing = value;
				ieee80211_wireless_reassoc(vap, 0, 1);
			}
		}
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_BA_SETUP_ENABLE:
		ieee80211_ba_setup_detect_set(vap, value);
		break;
	case IEEE80211_PARAM_AGGREGATION:
		if (value == 0 && IS_IEEE80211_VHT_ENABLED(ic))
			return -EOPNOTSUPP;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_MCS_CAP:
		/*
		 * Updating iv_mcs_config to have sync with other MCS
		 * commands, call_qcsapi and iwpriv
		 */
		if (!ieee80211_ht_tx_mcs_is_valid(value)) {
			printk("Invalid MCS in 11n mode\n");
			return -EINVAL;
		}

		vap->iv_mcs_config = IEEE80211_N_RATE_PREFIX | ((value << 16) & 0xff0000) |
				((value << 8) & 0xff00) | (value & 0xff);

		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_MU_ENABLE:
		if (!ieee80211_swfeat_is_supported(SWFEAT_ID_MU_MIMO, 1) ||
			(vap->iv_opmode == IEEE80211_M_STA &&
			get_hardware_revision() < HARDWARE_REVISION_TOPAZ_A2)) {
			return -EOPNOTSUPP;
		} else {
			uint32_t mu_bf_cap_flag = ic->ic_vhtcap.cap_flags &
						IEEE80211_VHTCAP_C_MU_BEAM_FORMXX_CAP_MASK;

			ic->ic_vhtcap.cap_flags &= ~IEEE80211_VHTCAP_C_MU_BEAM_FORMXX_CAP_MASK;
			ic->ic_mu_enable = value;

			/* when MU is enabled, make sure txBF STS cap is advertising 4SS */
			if (ic->ic_mu_enable) {
				if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
					mu_bf_cap_flag = IEEE80211_VHTCAP_C_MU_BEAM_FORMER_CAP;
				} else if (vap->iv_opmode == IEEE80211_M_STA) {
					mu_bf_cap_flag = IEEE80211_VHTCAP_C_MU_BEAM_FORMEE_CAP;
				}

				ic->ic_vhtcap.bfstscap_save = ic->ic_vhtcap.bfstscap;
				ic->ic_vhtcap.bfstscap = IEEE80211_VHTCAP_RX_STS_4;
				ic->ic_vhtcap.cap_flags |= mu_bf_cap_flag;

				ic->ic_vhtcap_24g.bfstscap_save = ic->ic_vhtcap_24g.bfstscap;
				ic->ic_vhtcap_24g.bfstscap = IEEE80211_VHTCAP_RX_STS_4;
			} else {
				if (ic->ic_vhtcap.bfstscap_save != IEEE80211_VHTCAP_RX_STS_INVALID) {
					ic->ic_vhtcap.bfstscap = ic->ic_vhtcap.bfstscap_save;
				}

				if (ic->ic_vhtcap_24g.bfstscap_save != IEEE80211_VHTCAP_RX_STS_INVALID) {
					ic->ic_vhtcap_24g.bfstscap = ic->ic_vhtcap_24g.bfstscap_save;
				}
			}

			ieee80211_param_to_qdrv(vap, param, ic->ic_mu_enable, NULL, 0);

			/* Force a reassociation to use the new BEAM FORMEE/ER settings */
			TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
				ieee80211_wireless_reassoc(vap_each, 0, 1);
			}
		}
		break;
	case IEEE80211_PARAM_MIMOMODE:
	case IEEE80211_PARAM_RETRY_COUNT:
	case IEEE80211_PARAM_RG:
	case IEEE80211_PARAM_ACK_POLICY:
	case IEEE80211_PARAM_EXP_MAT_SEL:
	case IEEE80211_PARAM_LEGACY_MODE:
	case IEEE80211_PARAM_MAX_AGG_SUBFRM:
	case IEEE80211_PARAM_MAX_AGG_SIZE:
	case IEEE80211_PARAM_TXBF_PERIOD:
	case IEEE80211_PARAM_TXBF_CTRL:
	case IEEE80211_PARAM_HTBA_SEQ_CTRL:
	case IEEE80211_PARAM_HTBA_SIZE_CTRL:
	case IEEE80211_PARAM_HTBA_TIME_CTRL:
	case IEEE80211_PARAM_HT_ADDBA:
	case IEEE80211_PARAM_HT_DELBA:
	case IEEE80211_PARAM_MUC_PROFILE:
	case IEEE80211_PARAM_MUC_PHY_STATS:
	case IEEE80211_PARAM_MUC_SET_PARTNUM:
	case IEEE80211_PARAM_ENABLE_GAIN_ADAPT:
	case IEEE80211_PARAM_FORCEMICERROR:
	case IEEE80211_PARAM_ENABLECOUNTERMEASURES:
	case IEEE80211_PARAM_RATE_CTRL_FLAGS:
	case IEEE80211_PARAM_CONFIG_BB_INTR_DO_SRESET:
	case IEEE80211_PARAM_CONFIG_MAC_INTR_DO_SRESET:
	case IEEE80211_PARAM_CONFIG_WDG_DO_SRESET:
	case IEEE80211_PARAM_TRIGGER_RESET:
	case IEEE80211_PARAM_INJECT_INVALID_FCS:
	case IEEE80211_PARAM_CONFIG_WDG_SENSITIVITY:
	case IEEE80211_PARAM_MAX_MGMT_FRAMES:
	case IEEE80211_PARAM_MCS_ODD_EVEN:
	case IEEE80211_PARAM_RESTRICTED_MODE:
	case IEEE80211_PARAM_RESTRICT_RTS:
	case IEEE80211_PARAM_RESTRICT_LIMIT:
	case IEEE80211_PARAM_RESTRICT_RATE:
	case IEEE80211_PARAM_SWRETRY_AGG_MAX:
	case IEEE80211_PARAM_SWRETRY_NOAGG_MAX:
	case IEEE80211_PARAM_SWRETRY_SUSPEND_XMIT:
	case IEEE80211_PARAM_BB_MAC_RESET_MSGS:
	case IEEE80211_PARAM_BB_MAC_RESET_DONE_WAIT:
	case IEEE80211_PARAM_TX_AGG_TIMEOUT:
	case IEEE80211_PARAM_LEGACY_RETRY_LIMIT:
	case IEEE80211_PARAM_RX_CTRL_FILTER:
	case IEEE80211_PARAM_DUMP_TCM_FD:
	case IEEE80211_PARAM_RXCSR_ERR_ALLOW:
	case IEEE80211_PARAM_DUMP_TRIGGER:
	case IEEE80211_PARAM_STOP_FLAGS:
	case IEEE80211_PARAM_CHECK_FLAGS:
	case IEEE80211_PARAM_PWR_ADJUST:
	case IEEE80211_PARAM_PWR_ADJUST_AUTO:
	case IEEE80211_PARAM_RTS_CTS:
	case IEEE80211_PARAM_TX_QOS_SCHED:
	case IEEE80211_PARAM_PEER_RTS_MODE:
	case IEEE80211_PARAM_DYN_WMM:
	case IEEE80211_PARAM_GET_CH_INUSE:
	case IEEE80211_PARAM_RX_AGG_TIMEOUT:
	case IEEE80211_PARAM_FORCE_MUC_HALT:
	case IEEE80211_PARAM_FORCE_ENABLE_TRIGGERS:
	case IEEE80211_PARAM_FORCE_MUC_TRACE:
	case IEEE80211_PARAM_BK_BITMAP_MODE:
	case IEEE80211_PARAM_PROBE_RES_RETRIES:
	case IEEE80211_PARAM_TEST_LNCB:
	case IEEE80211_PARAM_UNKNOWN_DEST_ARP:
	case IEEE80211_PARAM_UNKNOWN_DEST_FWD:
	case IEEE80211_PARAM_DBG_MODE_FLAGS:
	case IEEE80211_PARAM_PWR_SAVE:
	case IEEE80211_PARAM_DBG_FD:
	case IEEE80211_PARAM_CCA_PRI:
	case IEEE80211_PARAM_CCA_SEC:
	case IEEE80211_PARAM_CCA_SEC40:
	case IEEE80211_PARAM_CCA_FIXED:
	case IEEE80211_PARAM_PS_CMD:
	case IEEE80211_PARAM_DYN_AGG_TIMEOUT:
	case IEEE80211_PARAM_SIFS_TIMING:
	case IEEE80211_PARAM_TEST_TRAFFIC:
	case IEEE80211_PARAM_TX_AMSDU:
	case IEEE80211_PARAM_QCAT_STATE:
	case IEEE80211_PARAM_RALG_DBG:
	case IEEE80211_PARAM_SINGLE_AGG_QUEUING:
	case IEEE80211_PARAM_BA_THROT:
	case IEEE80211_PARAM_TX_QUEUING_ALG:
	case IEEE80211_PARAM_DAC_DBG:
	case IEEE80211_PARAM_CARRIER_ID:
	case IEEE80211_PARAM_WME_THROT:
	case IEEE80211_PARAM_GENPCAP:
	case IEEE80211_PARAM_CCA_DEBUG:
	case IEEE80211_PARAM_CCA_STATS_PERIOD:
	case IEEE80211_PARAM_AUC_TX:
	case IEEE80211_PARAM_TUNEPD_DONE:
	case IEEE80211_PARAM_AUC_RX_DBG:
	case IEEE80211_PARAM_AUC_TX_DBG:
	case IEEE80211_PARAM_RX_ACCELERATE:
	case IEEE80211_PARAM_RX_ACCEL_LOOKUP_SA:
	case IEEE80211_PARAM_BR_IP_ADDR:
	case IEEE80211_PARAM_AC_INHERITANCE:
	case IEEE80211_PARAM_AC_Q2Q_INHERITANCE:
	case IEEE80211_PARAM_1SS_AMSDU_SUPPORT:
	case IEEE80211_PARAM_TACMAP:
	case IEEE80211_PARAM_AUC_QOS_SCH:
	case IEEE80211_PARAM_TXBF_IOT:
	case IEEE80211_PARAM_AGGRESSIVE_AGG:
	case IEEE80211_PARAM_MU_DEBUG_FLAG:
	case IEEE80211_PARAM_INST_MU_GRP_QMAT:
	case IEEE80211_PARAM_DELE_MU_GRP_QMAT:
	case IEEE80211_PARAM_EN_MU_GRP_QMAT:
	case IEEE80211_PARAM_DIS_MU_GRP_QMAT:
	case IEEE80211_PARAM_SET_CRC_ERR:
	case IEEE80211_PARAM_MU_SWITCH_USR_POS:
	case IEEE80211_PARAM_SET_GRP_SND_PERIOD:
	case IEEE80211_PARAM_SET_PREC_SND_PERIOD:
	case IEEE80211_PARAM_SET_MU_RANK_TOLERANCE:
	case IEEE80211_PARAM_AUTO_CCA_ENABLE:
	case IEEE80211_PARAM_AUTO_CCA_PARAMS:
	case IEEE80211_PARAM_AUTO_CCA_DEBUG:
	case IEEE80211_PARAM_AUTO_CS_ENABLE:
	case IEEE80211_PARAM_AUTO_CS_PARAMS:
	case IEEE80211_PARAM_CS_THRESHOLD:
	case IEEE80211_PARAM_CS_THRESHOLD_DBM:
	case IEEE80211_PARAM_DUMP_PPPC_TX_SCALE_BASES:
	case IEEE80211_PARAM_GLOBAL_FIXED_TX_SCALE_INDEX:
	case IEEE80211_PARAM_NDPA_LEGACY_FORMAT:
	case IEEE80211_PARAM_SFS:
	case IEEE80211_PARAM_INST_1SS_DEF_MAT_ENABLE:
	case IEEE80211_PARAM_INST_1SS_DEF_MAT_THRESHOLD:
	case IEEE80211_PARAM_RATE_TRAIN_DBG:
	case IEEE80211_PARAM_MU_USE_EQ:
	case IEEE80211_PARAM_MU_AIRTIME_PADDING:
	case IEEE80211_PARAM_MU_AMSDU_SIZE:
	case IEEE80211_PARAM_RESTRICT_WLAN_IP:
	case IEEE80211_PARAM_MUC_SYS_DEBUG:
	case IEEE80211_PARAM_AUC_TX_AGG_DURATION:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_OFF_CHAN_SUSPEND:
		if (value & IEEE80211_OFFCHAN_SUSPEND_MASK) {
			value = value & IEEE80211_OFFCHAN_TIMEOUT_MASK;
			if (value > IEEE80211_OFFCHAN_TIMEOUT_MAX)
				value = IEEE80211_OFFCHAN_TIMEOUT_MAX;
			else if (value < IEEE80211_OFFCHAN_TIMEOUT_MIN)
				value = IEEE80211_OFFCHAN_TIMEOUT_MIN;
			ieee80211_off_channel_suspend(vap, value);
		} else {
			ieee80211_off_channel_resume(vap);
		}
		break;
	case IEEE80211_PARAM_SET_RTS_BW_DYN:
		ic->ic_rts_bw_dyn = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_SET_CTS_BW:
		ic->ic_cts_bw = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_NDPA_DUR:
		ic->ic_ndpa_dur = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_SU_TXBF_PKT_CNT:
		ic->ic_su_txbf_pkt_cnt = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_MU_TXBF_PKT_CNT:
		ic->ic_mu_txbf_pkt_cnt = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_MU_DEBUG_LEVEL:
		ic->ic_mu_debug_level = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_FIXED_SGI:
		ic->ic_gi_fixed = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_FIXED_BW:
		if (value & QTN_BW_FIXED_EN) {
			int bw = ieee80211_get_bw(ic);
			int qtn_bw = MS(value, QTN_BW_FIXED_BW);
			if (bw == BW_INVALID) {
				printk("current bw is invalid!\n");
				break;
			}
			if ((bw == BW_HT20 && qtn_bw > QTN_BW_20M) ||
					(bw == BW_HT40 && qtn_bw > QTN_BW_40M) ||
					(bw == BW_HT80 && qtn_bw > QTN_BW_80M)) {
				printk("Can't set fixed qtn_bw %u because current bw is %u\n",
						qtn_bw, bw);
				break;
			}
		}
		ic->ic_bw_fixed = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_RTSTHRESHOLD:
		if (value == vap->iv_rtsthreshold) // Nothing to do
			break;

		if (IEEE80211_RTS_MIN <= value && value <= IEEE80211_RTS_MAX) {
			vap->iv_rtsthreshold = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_PWR_ADJUST_SCANCNT:
		ic->ic_pwr_adjust_scancnt = value;
		break;
	case IEEE80211_PARAM_MUC_FLAGS:
		if (value & QTN_FLAG_MCS_UEQM_DISABLE) {
			ic->ic_caps &= ~IEEE80211_C_UEQM;
		} else {
			ic->ic_caps |= IEEE80211_C_UEQM;
		}
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_HT_NSS_CAP:
	case IEEE80211_PARAM_VHT_NSS_CAP:
		retv = ieee80211_set_nss_cap(vap, param, value);
		break;
	case IEEE80211_PARAM_ANTENNA_USAGE:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_VHT_MCS_CAP:
		retv = ieee80211_set_vht_mcs_cap(vap, value);
		break;
	case IEEE80211_PARAM_LDPC:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);

		/* Update the HT capabilities */
		TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
			if (value) {
				vap_each->iv_ht_flags |= IEEE80211_HTF_LDPC_ENABLED;
				vap_each->iv_vht_flags |= IEEE80211_VHTCAP_C_RX_LDPC;
				ic->ic_vhtcap.cap_flags |= IEEE80211_VHTCAP_C_RX_LDPC;
				ic->ic_vhtcap_24g.cap_flags |= IEEE80211_VHTCAP_C_RX_LDPC;
			} else {
				vap_each->iv_ht_flags &= ~IEEE80211_HTF_LDPC_ENABLED;
				vap_each->iv_vht_flags &= ~IEEE80211_VHTCAP_C_RX_LDPC;
				ic->ic_vhtcap.cap_flags &= ~IEEE80211_VHTCAP_C_RX_LDPC;
				ic->ic_vhtcap_24g.cap_flags &= ~IEEE80211_VHTCAP_C_RX_LDPC;
			}
			if ((vap_each->iv_opmode == IEEE80211_M_HOSTAP) &&
				(vap_each->iv_state == IEEE80211_S_RUN)) {
				ic->ic_beacon_update(vap_each);
			}
			/* Force a reassociation to use the new LDPC setting */
			ieee80211_wireless_reassoc(vap_each, 0, 1);
		}
		break;
	case IEEE80211_PARAM_STBC:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);

		/* Update the HT capabilities */
		TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
			if (value) {
				vap_each->iv_ht_flags |= IEEE80211_HTF_STBC_ENABLED;
				vap_each->iv_vht_flags |= IEEE80211_VHTCAP_C_TX_STBC;
			} else {
				vap_each->iv_ht_flags &= ~IEEE80211_HTF_STBC_ENABLED;
				vap_each->iv_vht_flags &= ~IEEE80211_VHTCAP_C_TX_STBC;
			}
			if ((vap_each->iv_opmode == IEEE80211_M_HOSTAP) &&
				(vap_each->iv_state == IEEE80211_S_RUN)) {
				ic->ic_beacon_update(vap_each);
			}
			/* Force a reassociation to use the new STBC setting */
			ieee80211_wireless_reassoc(vap_each, 0, 1);
		}
		break;
	case IEEE80211_PARAM_LDPC_ALLOW_NON_QTN:
		if (value) {
			vap->iv_ht_flags |= IEEE80211_HTF_LDPC_ALLOW_NON_QTN;
		} else {
			vap->iv_ht_flags &= ~IEEE80211_HTF_LDPC_ALLOW_NON_QTN;
		}
		/* Force a reassociation to use the new LDPC setting */
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_TRAINING_COUNT:
		vap->iv_rate_training_count = value;
		break;
	case IEEE80211_PARAM_FIXED_TX_RATE:
		{
			int mcs_val, mcs_nss;

			if ((value & IEEE80211_RATE_PREFIX_MASK) == IEEE80211_AC_RATE_PREFIX) {
				if (!IS_IEEE80211_VHT_ENABLED(ic))
					return -EINVAL;

				mcs_val = value & IEEE80211_AC_MCS_VAL_MASK;
				mcs_nss = (value & IEEE80211_AC_MCS_NSS_MASK) >> IEEE80211_11AC_MCS_NSS_SHIFT;

				if (!ieee80211_vht_tx_mcs_is_valid(mcs_val, mcs_nss)) {
					return -EINVAL;
				}
			} else if ((value & IEEE80211_RATE_PREFIX_MASK) == IEEE80211_N_RATE_PREFIX) {
				mcs_val = (value & 0xFF) & 0x7f;
				if (!ieee80211_ht_tx_mcs_is_valid(mcs_val)) {
					return -EINVAL;
				}
			}
			printk("Warning: %s MCS rate is fixed at 0x%08x\n", __func__, value);
			/* Forward fixed MCS rate configuration to the driver and MuC */
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			/* Remember current configuration in the VAP */
			vap->iv_mcs_config = value;
		}
		break;
	case IEEE80211_PARAM_SHORT_GI:
		/* Forward short GI configuration to the driver and MuC */
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);

		TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
			if (value) {
				vap_each->iv_ht_flags |= IEEE80211_HTF_SHORTGI_ENABLED;
				vap_each->iv_vht_flags |= IEEE80211_VHTCAP_C_SHORT_GI_80;
			} else {
				vap_each->iv_ht_flags &= ~IEEE80211_HTF_SHORTGI_ENABLED;
				vap_each->iv_vht_flags &= ~IEEE80211_VHTCAP_C_SHORT_GI_80;
			}

			if ((vap_each->iv_opmode == IEEE80211_M_HOSTAP) &&
				(vap_each->iv_state == IEEE80211_S_RUN)) {
				ic->ic_beacon_update(vap_each);
			}
			/* Force a reassociation to use the new SGI setting */
			ieee80211_wireless_reassoc(vap_each, 0, 1);
		}
		break;
	case IEEE80211_PARAM_BW_SEL_MUC:
	case IEEE80211_PARAM_BW_SEL:
		if ((value != BW_HT160) && (value != BW_HT80) &&
				(value != BW_HT40) && (value != BW_HT20))
			return -EINVAL;

		/* update station profile */
		if ((ic->ic_rf_chipid == CHIPID_DUAL) &&
				(ic->ic_opmode == IEEE80211_M_STA)) {
			if (IS_IEEE80211_5G_BAND(ic))
				vap->iv_5ghz_prof.bw = value;
			else
				vap->iv_2_4ghz_prof.bw = value;
		}

		if ((value > BW_HT40) &&
				!ieee80211_swfeat_is_supported(SWFEAT_ID_VHT, 1)) {
			retv = EOPNOTSUPP;
			break;
		}
		/*
		 * Blocking 40MHZ and 80MHZ in 2.4 band and legacy 11A
		 * only mode
		 */
		if ((value >= BW_HT40) && ((ic->ic_curmode == IEEE80211_MODE_11A) ||
				(!IS_IEEE80211_11NG(ic) && IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))))
			return -EOPNOTSUPP;

		ic->ic_max_system_bw = value;

		/* Forward bandwidth configuration (enabling 40 MHz BW) to the driver and MuC */
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);

		ieee80211_start_obss_scan_timer(vap);
		ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
		if (IS_UP_AUTO(vap)) {
			ieee80211_wireless_reassoc(vap, 0, 1);
			ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
		} else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			ic->ic_set_channel(ic);
		}
		break;
	case IEEE80211_PARAM_PHY_STATS_MODE:
		if (value != MUC_PHY_STATS_ALTERNATE &&
		    value != MUC_PHY_STATS_RSSI_RCPI_ONLY &&
		    value != MUC_PHY_STATS_ERROR_SUM_ONLY) {
			retv = EINVAL;
		} else {
			/* Forward Phy Stats configuration to the driver and MuC */
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			ic->ic_mode_get_phy_stats = value;
		}
		break;
	case IEEE80211_PARAM_FORCE_SMPS:
		/* Reflect the forced value in the local structures */
		ieee80211_forcesmps(vap, value);
		break;
	case IEEE80211_PARAM_CHANNEL_NOSCAN:
		g_channel_fixed = value;
		break;
	case IEEE80211_PARAM_LINK_LOSS:
		vap->iv_link_loss_enabled = value;
		break;
	case IEEE80211_PARAM_BCN_MISS_THR:
		if (!vap->iv_bss)
			return -EFAULT;

		if (vap->iv_opmode == IEEE80211_M_STA) {
			if (value < 1) {
				printk(KERN_ERR "%s: bad value %d, "
						"beacon miss threshold must be >= 1\n",
						dev->name, value);
				break;
			}
			printk(KERN_INFO "%s: set beacon miss threshold to %d\n",
					dev->name, value);
			vap->iv_bcn_miss_thr = value;

			/* Recalculate swbmiss period with new value */
			vap->iv_swbmiss_period =
					IEEE80211_TU_TO_JIFFIES(vap->iv_bss->ni_intval *
					vap->iv_bcn_miss_thr);

			if (vap->iv_swbmiss_warnings)
				vap->iv_swbmiss_period /= (vap->iv_swbmiss_warnings + 1);
		} else {
			printk(KERN_ERR "%s: can't set beacon miss threshold for non STA\n",
					dev->name);
		}
		break;
	case IEEE80211_PARAM_IMPLICITBA:
		if (vap->iv_implicit_ba != (value & 0xFFFF))
		{
			printk("New implicit BA value (%04X) - remove all associations\n", (value & 0xFFFF));
			vap->iv_implicit_ba = value & 0xFFFF;
			ieee80211_wireless_reassoc(vap, 0, 1);
		}
		break;
	case IEEE80211_PARAM_SHOWMEM:
		if (value == 0x1) {
#ifdef WLAN_MALLOC_FREE_TOT_DEBUG
			printk("WLAN bytes: allocated=%d freed=%d balance=%d\n"
				"     times: allocated=%d freed=%d\n"
				"     nodes: allocated=%d freed=%d current=%d refs=%u\n"
				"     tmp:   allocated=%d freed=%d\n",
				g_wlan_tot_alloc, g_wlan_tot_free, g_wlan_balance,
				g_wlan_tot_alloc_cnt, g_wlan_tot_free_cnt,
				g_wlan_tot_node_alloc, g_wlan_tot_node_free, ic->ic_node_count,
				atomic_read(&vap->iv_dev->refcnt),
				g_wlan_tot_node_alloc_tmp, g_wlan_tot_node_free_tmp);
#else
			printk("Memory debug statistics disabled\n");
#endif
		}
		break;
	case IEEE80211_PARAM_RX_AMSDU_ENABLE:
		vap->iv_rx_amsdu_enable = value;
		ieee80211_wireless_reassoc(vap, 1, 0);
		break;
	case IEEE80211_PARAM_RX_AMSDU_THRESHOLD_CCA:
		vap->iv_rx_amsdu_threshold_cca = value;
		break;
	case IEEE80211_PARAM_RX_AMSDU_THRESHOLD_PMBL:
		vap->iv_rx_amsdu_threshold_pmbl = value;
		break;
	case IEEE80211_PARAM_RX_AMSDU_PMBL_WF_SP:
		vap->iv_rx_amsdu_pmbl_wf_sp = value;
		break;
	case IEEE80211_PARAM_RX_AMSDU_PMBL_WF_LP:
		vap->iv_rx_amsdu_pmbl_wf_lp = value;
		break;
	case IEEE80211_PARAM_CLIENT_REMOVE:
		printk("Removing clients, not forcing deauth\n");
		ieee80211_wireless_reassoc(vap, 1, 0);
		break;
	case IEEE80211_PARAM_VAP_DBG:
		vap->iv_debug = value;
		break;
	case IEEE80211_PARAM_NODEREF_DBG:
		ieee80211_node_dbgref_history_dump();
		break;
	case IEEE80211_PARAM_GLOBAL_BA_CONTROL:
		if (vap->iv_ba_control != (value & 0xFFFF))
		{
			vap->iv_ba_old_control = vap->iv_ba_control;
			vap->iv_ba_control = value & 0xFFFF;
			ieee80211_wireless_ba_change(vap);
		}
		break;
	case IEEE80211_PARAM_NO_SSID_ASSOC:
		if (value) {
			vap->iv_qtn_options &= ~IEEE80211_QTN_NO_SSID_ASSOC_DISABLED;
			printk("Enabling associations with no SSID\n");
		} else {
			vap->iv_qtn_options |= IEEE80211_QTN_NO_SSID_ASSOC_DISABLED;
			printk("Disabling associations with no SSID\n");
		}
		break;
	case IEEE80211_PARAM_CONFIG_TXPOWER:
		retv = apply_tx_power(vap, value, IEEE80211_BKUP_TXPOWER_NORMAL);
		if (retv) {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_INITIATE_TXPOWER_TABLE:
		retv = apply_tx_power(vap, value, IEEE80211_INIT_TXPOWER_TABLE);
		if (retv) {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_CONFIG_BW_TXPOWER:
		retv = ieee80211_set_bw_txpower(vap, value);
		if (!retv) {
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_DUMP_CONFIG_TXPOWER:
		if (value == 1) {
			ieee80211_dump_tx_power(ic);
		} else {
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		}
		break;
	case IEEE80211_PARAM_TPC:
		if (ic->ic_flags & IEEE80211_F_DOTH) {
			int			last_tpc_state;
			struct ieee80211vap	*each_vap;

			value = !!value;
			last_tpc_state = !!(ic->ic_flags_ext & IEEE80211_FEXT_TPC);
			if (value != last_tpc_state) {
				if (value) {
					printk("Enable tpc feature\n");
					ic->ic_flags_ext |= IEEE80211_FEXT_TPC;
					ic->ic_pppc_select_enable_backup = ic->ic_pppc_select_enable;
					if (ic->ic_pppc_select_enable) {
						ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_PPPC_SELECT, 0, NULL, 0);
						ic->ic_pppc_select_enable = 0;
					}
				} else {
					printk("Disable tpc feature\n");
					ic->ic_flags_ext &= ~IEEE80211_FEXT_TPC;
					ieee80211_tpc_query_stop(&ic->ic_tpc_query_info);
					ic->ic_pppc_select_enable = ic->ic_pppc_select_enable_backup;
					if (ic->ic_pppc_select_enable)
						ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_PPPC_SELECT, 1, NULL, 0);
				}
				TAILQ_FOREACH(each_vap, &ic->ic_vaps, iv_next) {
					if ((each_vap->iv_opmode == IEEE80211_M_HOSTAP) && (each_vap->iv_state == IEEE80211_S_RUN)) {
						if (!value) {
							ieee80211_ppqueue_remove_with_cat_action(&each_vap->iv_ppqueue,
									IEEE80211_ACTION_CAT_SPEC_MGMT,
									IEEE80211_ACTION_S_TPC_REPORT);
						}
						ic->ic_beacon_update(vap);
					}
				}
			}
		} else {
			retv = EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_CONFIG_TPC_INTERVAL:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC))
			retv = ieee80211_tpc_query_config_interval(&ic->ic_tpc_query_info, value);
		else
			retv = EOPNOTSUPP;
		break;
	case IEEE80211_PARAM_TPC_QUERY:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			if (value == 0) {
				ieee80211_tpc_query_stop(&ic->ic_tpc_query_info);
			} else {
				ieee80211_tpc_query_start(&ic->ic_tpc_query_info);
			}
		}
		else
			retv = EOPNOTSUPP;
		break;
	case IEEE80211_PARAM_CONFIG_REGULATORY_TXPOWER:
		retv = set_regulatory_tx_power(ic, value);
		break;
	case IEEE80211_PARAM_SKB_LIST_MAX:
		{
			struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();
			recycle_list->max = (int)value;
		}
		break;
	case IEEE80211_PARAM_DFS_FAST_SWITCH:
		if (value) {
			retv = ieee80211_ioctl_set_dfs_fast_switch(ic);
		} else {
			ic->ic_flags_ext &= ~IEEE80211_FEXT_DFS_FAST_SWITCH;
		}
		break;
	case IEEE80211_PARAM_SCAN_NO_DFS:
		if (value) {
			ic->ic_flags_ext |= IEEE80211_FEXT_SCAN_NO_DFS;
		} else {
			ic->ic_flags_ext &= ~IEEE80211_FEXT_SCAN_NO_DFS;
		}
		break;
	case IEEE80211_PARAM_11N_40_ONLY_MODE:
		if (value && !(ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40)) {
			retv = EINVAL;
			break;
		}
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
			(vap->iv_state == IEEE80211_S_RUN)) {
			ic->ic_beacon_update(vap);
		}
		break;
	case IEEE80211_PARAM_REGULATORY_REGION:
		{
			u_int16_t iso_code = CTRY_DEFAULT;
			union {
				char		as_chars[ 4 ];
				u_int32_t	as_u32;
			} region;

			region.as_u32 = (u_int32_t) value;
			region.as_chars[ 3 ] = '\0';

			retv = ieee80211_country_string_to_countryid( region.as_chars, &iso_code );
			if (retv == 0) {
				ic->ic_country_code = iso_code;
				ic->ic_mark_dfs_channels(ic, ic->ic_nchans, ic->ic_channels);
				ic->ic_mark_weather_radar_chans(ic, ic->ic_nchans, ic->ic_channels);
				retv = ieee80211_region_to_operating_class(ic, region.as_chars);
				if (retv < 0)
					vap->iv_flags_ext |= IEEE80211_FEXT_TDLS_CS_PROHIB;
				/* TODO: Should be removed after weather channel CAC bug fixed */
				local_weather_channel_check(dev);
			}
		}
		break;
	case IEEE80211_PARAM_SAMPLE_RATE:
		ic->ic_sample_rate = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_BA_MAX_WIN_SIZE:
		if (value < IEEE80211_MAX_BA_WINSIZE) {
			vap->iv_max_ba_win_size = value;
		} else {
			vap->iv_max_ba_win_size = IEEE80211_MAX_BA_WINSIZE;
		}
		ieee80211_wireless_reassoc(vap, 0, 0);
		break;
#ifdef QSCS_ENABLED
	case IEEE80211_PARAM_SCS:
		/* Disable SCS on DUAL band board only for 2.4GHZ band (RFIC6)*/
		if (ic->ic_rf_chipid == CHIPID_DUAL) {
			if ((ic->ic_curmode == IEEE80211_MODE_11B) ||
					(ic->ic_curmode == IEEE80211_MODE_11G) ||
					(ic->ic_curmode == IEEE80211_MODE_FH) ||
					(ic->ic_curmode == IEEE80211_MODE_TURBO_G) ||
					(ic->ic_curmode == IEEE80211_MODE_11NG) ||
					(ic->ic_curmode == IEEE80211_MODE_11NG_HT40PM)) {
				return -EOPNOTSUPP;
			}
		}

		if (ieee80211_param_scs_set(dev, vap, value) == 0) {
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
#endif /* QSCS_ENABLED */
	case IEEE80211_PARAM_MIN_DWELL_TIME_ACTIVE:
		ic->ic_mindwell_active = (u_int16_t) value;
		break;
	case IEEE80211_PARAM_MIN_DWELL_TIME_PASSIVE:
		ic->ic_mindwell_passive = (u_int16_t) value;
		break;
	case IEEE80211_PARAM_MAX_DWELL_TIME_ACTIVE:
		ic->ic_maxdwell_active = (u_int16_t) value;
		break;
	case IEEE80211_PARAM_MAX_DWELL_TIME_PASSIVE:
		ic->ic_maxdwell_passive = (u_int16_t) value;
		break;
#ifdef QTN_BG_SCAN
	case IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_ACTIVE:
		if (value) {
			ic->ic_qtn_bgscan.dwell_msecs_active = (u_int16_t) value;
		}
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_PASSIVE:
		if (value) {
			ic->ic_qtn_bgscan.dwell_msecs_passive = (u_int16_t) value;
		}
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_ACTIVE:
		if (value) {
			ic->ic_qtn_bgscan.duration_msecs_active = (u_int16_t) value;
		}
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_FAST:
		if (value) {
			ic->ic_qtn_bgscan.duration_msecs_passive_fast = (u_int16_t) value;
		}
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_NORMAL:
		if (value) {
			ic->ic_qtn_bgscan.duration_msecs_passive_normal = (u_int16_t) value;
		}
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_SLOW:
		if (value) {
			ic->ic_qtn_bgscan.duration_msecs_passive_slow = (u_int16_t) value;
		}
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_FAST:
		ic->ic_qtn_bgscan.thrshld_fat_passive_fast = (u_int16_t) value;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_NORMAL:
		ic->ic_qtn_bgscan.thrshld_fat_passive_normal = (u_int16_t) value;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DEBUG:
		ic->ic_qtn_bgscan.debug_flags = (u_int16_t) value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
#endif /*QTN_BG_SCAN */
	case IEEE80211_PARAM_QTN_BCM_WAR:
		if (value)
			ic->ic_flags_qtn |= IEEE80211_QTN_BCM_WAR;
		else
			ic->ic_flags_qtn &= ~IEEE80211_QTN_BCM_WAR;
		break;
	case IEEE80211_PARAM_ALT_CHAN:
		retv = ieee80211_ioctl_set_alt_chan(ic, (uint8_t) value);
		break;
	case IEEE80211_PARAM_GI_SELECT:
		ic->ic_gi_select_enable = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_RADAR_NONOCCUPY_PERIOD:
		if ((value >= IEEE80211_MIN_NON_OCCUPANCY_PERIOD) &&
		    (value <= IEEE80211_MAX_NON_OCCUPANCY_PERIOD)) {
			ic->ic_non_occupancy_period = value * HZ;
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_MC_LEGACY_RATE:
		{
			u_int8_t mc_rate = 0;
			static const char *idx2lr[] = {"6", "9", "12", "18", "24", "36", "48", "54", "1", "2", "5.5", "11"};
			int i;
			for (i = 0; i < 4; i++) {
				u_int8_t rate = (value >> i*8) & 0xFF;
				if (rate > 11) {
					retv = EINVAL;
					break;
				}
			}
			if (retv != EINVAL) {
				mc_rate = (value >> 24) & 0xFF;
				printk("Forcing multicast rate to %sMbps\n", idx2lr[mc_rate]);
				vap->iv_mc_legacy_rate = value;
				ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
				if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
					ic->ic_beacon_update(vap);
				}
			}
		}
		break;
	case IEEE80211_PARAM_RADAR_NONOCCUPY_ACT_SCAN:
		if (value)
			ic->ic_flags_qtn |= IEEE80211_QTN_RADAR_SCAN_START;
		else
			ic->ic_flags_qtn &= ~IEEE80211_QTN_RADAR_SCAN_START;
		break;
	case IEEE80211_PARAM_FWD_UNKNOWN_MC:
		vap->iv_forward_unknown_mc = !!value;
		break;
	case IEEE80211_PARAM_MC_TO_UC:
		if (value < IEEE80211_QTN_MC_TO_UC_LEGACY || value > IEEE80211_QTN_MC_TO_UC_ALWAYS)
			return -EINVAL;
		vap->iv_mc_to_uc = value;
		break;
	case IEEE80211_PARAM_BCST_4:
		vap->iv_reliable_bcst = !!value;
		break;
	case IEEE80211_PARAM_AP_FWD_LNCB:
		vap->iv_ap_fwd_lncb = !!value;
		break;
	case IEEE80211_PARAM_PPPC_SELECT:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			retv = EOPNOTSUPP;
		} else {
			ic->ic_pppc_select_enable = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		}
		break;
	case IEEE80211_PARAM_PPPC_STEP:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			retv = EOPNOTSUPP;
		} else {
			if ((value >= QTN_SEL_PPPC_MAX_STEPS) || (value < 0)) {
				return -EINVAL;
			}
			ic->ic_pppc_step_db = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			ieee80211_wireless_reassoc(vap, 0, 1);
		}
		break;
	case IEEE80211_PARAM_EMI_POWER_SWITCHING:
		ic->ic_emi_power_switch_enable = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_ASSOC_LIMIT:
		retv = ieee80211_ioctl_set_assoc_limit(ic, vap, value);
		break;
	case IEEE80211_PARAM_BSS_ASSOC_LIMIT:
		retv = ieee80211_ioctl_set_bss_grp_assoc_limit(ic, vap, value);
		break;
	case IEEE80211_PARAM_ASSOC_HISTORY:
		{
			int i;

			memset(&ic->ic_assoc_history.ah_macaddr_table[0][0],
				0, sizeof(ic->ic_assoc_history.ah_macaddr_table));
			for(i = 0; i < IEEE80211_MAX_ASSOC_HISTORY; i++) {
				ic->ic_assoc_history.ah_timestamp[i] = 0;
			}
		}
		break;
	case IEEE80211_PARAM_CSW_RECORD:
		memset(&ic->ic_csw_record, 0, sizeof(ic->ic_csw_record));
		break;
	case IEEE80211_PARAM_IOT_TWEAKS:
		qtn_mproc_sync_shared_params_get()->iot_tweaks = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_VSP_NOD_DEBUG:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_FAST_REASSOC:
		if (value) {
			ic->ic_flags_ext |= IEEE80211_FEXT_SCAN_FAST_REASS;
		} else {
			ic->ic_flags_ext &= ~IEEE80211_FEXT_SCAN_FAST_REASS;
		}
		break;
	case IEEE80211_PARAM_CSA_FLAG:
		ic->ic_csa_flag = value;
		break;
	case IEEE80211_PARAM_DEF_MATRIX:
		ic->ic_def_matrix = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_MODE:
		/* update 5ghz station profile */
		if ((ic->ic_rf_chipid == CHIPID_DUAL) &&
				(ic->ic_opmode == IEEE80211_M_STA))
			vap->iv_5ghz_prof.vht = value;

		if (chip_id() >= QTN_BBIC_11AC) {
			ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			ieee80211_wireless_reassoc(vap, 0, 1);

			/* On AP, change channel to take new bw into effect by BB */
			/* On STA, reassociation would result into re-scannning so not required */
			if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
				ic->ic_set_channel(ic);
			}
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_ENABLE_11AC:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		vap->iv_11ac_enabled = value;
		break;
	case IEEE80211_PARAM_FIXED_11AC_MU_TX_RATE:
		retv = ieee80211_wireless_set_mu_tx_rate(vap, ic, value);
		break;
	case IEEE80211_PARAM_FIXED_11AC_TX_RATE:
		if (IS_IEEE80211_DUALBAND_VHT_ENABLED(ic)) {
			int mcs_to_muc = ieee80211_11ac_mcs_format(value, 40);
			if (mcs_to_muc < 0) {
				retv = EINVAL;
				break;
			}
			/* Forward fixed MCS rate configuration to the driver and MuC */
			ieee80211_param_to_qdrv(vap, param, mcs_to_muc, NULL, 0);
			/* Remember current configuration in the VAP */
			vap->iv_mcs_config = (value & 0x0FFFFFF) | IEEE80211_AC_RATE_PREFIX;
		} else {
			retv = EINVAL;
		}

		break;
	case IEEE80211_PARAM_VAP_PRI:
		retv = ieee80211_ioctl_set_vap_pri(ic, vap, value);
		break;
	case IEEE80211_PARAM_AIRFAIR:
		if (value <= 1) {
			ic->ic_airfair = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_VAP_PRI_WME:
		printk("Change auto WME param based on VAP priority from %u to %u\n",
				ic->ic_vap_pri_wme, value);
		ic->ic_vap_pri_wme = value;
		IEEE80211_LOCK_IRQ(ic);
		ieee80211_adjust_wme_by_vappri(ic);
		IEEE80211_UNLOCK_IRQ(ic);
		break;
	case IEEE80211_PARAM_TDLS_DISC_INT:
		if (ieee80211_tdls_cfg_disc_int(vap, value) != 0) {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_TDLS_PATH_SEL_WEIGHT:
		vap->tdls_path_sel_weight = value;
		break;
	case IEEE80211_PARAM_TDLS_MODE:
		vap->tdls_path_sel_prohibited = !!value;
		if (vap->tdls_path_sel_prohibited == 1)
			ieee80211_tdls_free_peer_ps_info(vap);
		break;
	case IEEE80211_PARAM_TDLS_TIMEOUT_TIME:
		vap->tdls_timeout_time = value;
		if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0)
			ieee80211_tdls_update_link_timeout(vap);
		break;
	case IEEE80211_PARAM_TDLS_TRAINING_PKT_CNT:
		vap->tdls_training_pkt_cnt = value;
		break;
	case IEEE80211_PARAM_TDLS_PATH_SEL_PPS_THRSHLD:
		vap->tdls_path_sel_pps_thrshld = value;
		break;
	case IEEE80211_PARAM_TDLS_PATH_SEL_RATE_THRSHLD:
		vap->tdls_path_sel_rate_thrshld = value;
		break;
	case IEEE80211_PARAM_TDLS_VERBOSE:
		vap->tdls_verbose = value;
		ic->ic_set_tdls_param(vap->iv_bss, IOCTL_TDLS_DBG_LEVEL, value);
		break;
	case IEEE80211_PARAM_TDLS_MIN_RSSI:
		vap->tdls_min_valid_rssi = value;
		break;
	case IEEE80211_PARAM_TDLS_UAPSD_INDICAT_WND:
		if (vap->tdls_uapsd_indicat_wnd != value) {
			vap->tdls_uapsd_indicat_wnd = value;
			ieee80211_tdls_update_uapsd_indicication_windows(vap);
		}
		break;
	case IEEE80211_PARAM_TDLS_SWITCH_INTS:
		vap->tdls_switch_ints = value;
		break;
	case IEEE80211_PARAM_TDLS_RATE_WEIGHT:
		vap->tdls_phy_rate_wgt = value;
		break;
	case IEEE80211_PARAM_TDLS_CS_MODE:
		if (value == 2) {
			vap->iv_flags_ext |= IEEE80211_FEXT_TDLS_CS_PASSIVE;
			vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_CS_PROHIB;
		} else if (value == 1){
			vap->iv_flags_ext |= IEEE80211_FEXT_TDLS_CS_PROHIB;
			vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_CS_PASSIVE;
		} else {
			vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_CS_PROHIB;
			vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_CS_PASSIVE;
		}
		break;
	case IEEE80211_PARAM_TDLS_OFF_CHAN:
		vap->tdls_fixed_off_chan = value;
		break;
	case IEEE80211_PARAM_TDLS_OFF_CHAN_BW:
		if ((value == BW_INVALID) || (value == BW_HT20) ||
				(value == BW_HT40) || (value == BW_HT80) ||
					(value == BW_HT160))
			vap->tdls_fixed_off_chan_bw = value;
		else
			retv = EINVAL;
		break;
	case IEEE80211_PARAM_TDLS_NODE_LIFE_CYCLE:
		vap->tdls_node_life_cycle = value;
		if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0)
			ieee80211_tdls_start_node_expire_timer(vap);
		break;
	case IEEE80211_PARAM_TDLS_OVER_QHOP_ENABLE:
		vap->tdls_over_qhop_en = value;
		break;
	case IEEE80211_PARAM_OCAC:
	case IEEE80211_PARAM_SDFS:
		if (ieee80211_param_ocac_set(dev, vap, value) == 0) {
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_DEACTIVE_CHAN_PRI:
		retv = ieee80211_ioctl_setchan_inactive_pri(ic, vap, value);
		break;
	case IEEE80211_PARAM_SPECIFIC_SCAN:
		if (!!value)
			vap->iv_flags_ext |= IEEE80211_FEXT_SPECIFIC_SCAN;
		else
			vap->iv_flags_ext &= ~IEEE80211_FEXT_SPECIFIC_SCAN;
		break;
	case IEEE80211_PARAM_SCAN_TBL_LEN_MAX:
		if (value != ic->ic_scan_tbl_len_max) {
			ic->ic_scan_tbl_len_max = value;
			ieee80211_scan_flush(ic);
		}
		break;
	case IEEE80211_PARAM_TRAINING_START:
		ieee80211_training_restart_by_node_idx(vap, value);
		break;
	case IEEE80211_PARAM_SPEC_COUNTRY_CODE:
		{
			uint16_t iso_code = CTRY_DEFAULT;
			union {
				char as_chars[4];
				uint32_t as_u32;
			} region;

			region.as_u32 = (uint32_t)value;
			region.as_chars[3] = '\0';

			retv = ieee80211_country_string_to_countryid(region.as_chars, &iso_code);
			if (retv == 0) {
				ic->ic_spec_country_code = iso_code;
				ieee80211_build_countryie(ic);
			} else {
				retv = -retv;
			}
		}
		break;
        case IEEE80211_PARAM_VCO_LOCK_DETECT_MODE:
        {
            sp->vco_lock_detect_mode = value;
        }
        break;
	case IEEE80211_PARAM_CONFIG_PMF:
			/* enable/disable PMF per VAP */
			if( value <= 3) {
				vap->iv_pmf = value;
				ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			} else {
				retv = EINVAL;
			}
		break;
	case IEEE80211_PARAM_SCAN_CANCEL:
		if (value) {
			/* Force canceling immediately */
			ieee80211_cancel_scan_no_wait(vap);
		} else {
			ieee80211_cancel_scan(vap);
		}
		break;

	case IEEE80211_PARAM_DSP_DEBUG_LEVEL:
		DSP_PARAM_SET(debug_level, value);
		break;

	case IEEE80211_PARAM_DSP_DEBUG_FLAG:
		DSP_PARAM_SET(debug_flag, value);
		break;

	case IEEE80211_PARAM_DSP_MU_RANK_CRITERIA:
		DSP_PARAM_SET(rank_criteria_to_use, value);
		break;

	case IEEE80211_PARAM_DSP_PRECODING_ALGORITHM:
		if ( MU_ALLOWED_ALG(value) ) {
			DSP_PARAM_SET(precoding_algorithm_to_use, value);
		} else {
			return -EINVAL;
		}
		break;

	case IEEE80211_PARAM_DSP_RANKING_ALGORITHM:
		if ( MU_ALLOWED_ALG(value) ) {
			DSP_PARAM_SET(ranking_algorithm_to_use, value);
		} else {
			return -EINVAL;
		}
		break;

	case IEEE80211_PARAM_INTRA_BSS_ISOLATE:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (value)
				dev->qtn_flags |= QTN_FLAG_INTRA_BSS_ISOLATE;
			else
				dev->qtn_flags &= ~QTN_FLAG_INTRA_BSS_ISOLATE;
		} else {
			return -EINVAL;
		}

		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_BSS_ISOLATE:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (value)
				dev->qtn_flags |= QTN_FLAG_BSS_ISOLATE;
			else
				dev->qtn_flags &= ~QTN_FLAG_BSS_ISOLATE;
		} else {
			return -EINVAL;
		}

		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_BF_RX_STS:
		if ((value > 0) && (value <= (IEEE80211_VHTCAP_RX_STS_4 + 1))) {
			ic->ic_vhtcap.bfstscap = value - 1;
			ic->ic_vhtcap_24g.bfstscap = value - 1;
			TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
				if ((vap->iv_opmode != IEEE80211_M_HOSTAP) || (vap->iv_state != IEEE80211_S_RUN))
					continue;
				ic->ic_beacon_update(vap);
			}
			retv = 0;
		} else {
			retv = -EINVAL;
		}
		break;
	case IEEE80211_PARAM_PC_OVERRIDE:
	{
		uint16_t pwr_constraint = (value & 0xffff);
		uint8_t rssi_threshold = ((value >> 16) & 0xff);
		uint8_t sec_offset = ((value >> 24) & 0xff);

		KASSERT(ic->ic_bsschan != IEEE80211_CHAN_ANYC, ("bss channel not set"));

		/*
		 * Default mode when value = 1
		 **/
		if (pwr_constraint && !rssi_threshold && !sec_offset) {
			pwr_constraint = PWR_CONSTRAINT_PC_DEF;
			rssi_threshold = PWR_CONSTRAINT_RSSI_DEF;
			sec_offset = PWR_CONSTRAINT_OFFSET;
		}
		/*
		 * Hack for ASUS/Broadcomm 3ss client to turn down power
		 * Made sure this does not affect any other code like tpc/pppc
		 * by placing code here.
		 */
		if (((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_COUNTRYIE)) &&
			!((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC))) {
				if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
					if(value && ic->ic_pco.pco_pwr_constraint_save == PWR_CONSTRAINT_SAVE_INIT) {
						printk("pwr-cons=%d rssi-thr=%d sec-off=%d\n", pwr_constraint, -rssi_threshold, sec_offset);
						if(pwr_constraint < ic->ic_bsschan->ic_maxregpower) {
							ic->ic_pco.pco_pwr_constraint_save = ic->ic_pwr_constraint;
							ic->ic_pco.pco_rssi_threshold = rssi_threshold;
							ic->ic_pco.pco_pwr_constraint = pwr_constraint;
							ic->ic_pco.pco_sec_offset = sec_offset;
							init_timer(&ic->ic_pco.pco_timer);
							ic->ic_pco.pco_timer.function = ieee80211_pco_timer_func;
							ic->ic_pco.pco_timer.data = (unsigned long) vap;
							ic->ic_pco.pco_timer.expires = jiffies + (5 * HZ);
							add_timer(&ic->ic_pco.pco_timer);
						} else {
							printk("power constraint(%d) >= current channel max regulatory power(%d)\n", pwr_constraint, ic->ic_bsschan->ic_maxregpower);
							retv = EINVAL;
						}
					} else {
						if (!value && ic->ic_pco.pco_pwr_constraint_save != PWR_CONSTRAINT_SAVE_INIT) {
							printk("PCO Disabled\n");
							ic->ic_pco.pco_pwr_constraint = 0;
							ic->ic_pco.pco_rssi_threshold = 0;
							ic->ic_pco.pco_sec_offset = 0;
							ieee80211_pco_timer_func((unsigned long)vap);
							del_timer(&ic->ic_pco.pco_timer);
							ic->ic_pwr_constraint = ic->ic_pco.pco_pwr_constraint_save;
							ic->ic_pco.pco_pwr_constraint_save = PWR_CONSTRAINT_SAVE_INIT;
						} else {
							retv = EINVAL;
						}
					}
				} else {
					retv = EOPNOTSUPP;
				}
		} else {
				printk("power constraint override needs to have TPC disabled\n");
				retv = EOPNOTSUPP;
		}
		break;
	}
	case IEEE80211_PARAM_WOWLAN:
		if (ieee80211_param_wowlan_set(dev, vap, value) == 0) {
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_WDS_MODE:
		if (ieee80211_is_repeater(ic)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
					"%s can't config WDS mode since it's a"
					" repeater\n", vap->iv_dev->name);
			retv = EPERM;
			break;
		}
#ifdef CONFIG_QVSP
		temp_value = !!IEEE80211_VAP_WDS_IS_RBS(vap);

		ieee80211_vap_set_extdr_flags(vap, value);

		value = !!IEEE80211_VAP_WDS_IS_RBS(vap);
		if (vap->iv_opmode == IEEE80211_M_WDS && temp_value ^ value)
			ic->ic_vsp_change_stamode(ic, value);
#else
		ieee80211_vap_set_extdr_flags(vap, value);

#endif
		break;
	case IEEE80211_PARAM_EXTENDER_ROLE:
		if (ieee80211_is_repeater(ic)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
					"%s can't config extender role since "
					"it's a repeater\n", vap->iv_dev->name);
			retv = EPERM;
			break;
		}
		retv = ieee80211_ioctl_extender_role(ic, vap, value);
		break;
	case IEEE80211_PARAM_EXTENDER_MBS_BEST_RSSI:
		if (ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
			ic->ic_extender_mbs_best_rssi = value;
		break;
	case IEEE80211_PARAM_EXTENDER_RBS_BEST_RSSI:
		if (ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
			ic->ic_extender_rbs_best_rssi = value;
		break;
	case IEEE80211_PARAM_EXTENDER_MBS_WGT:
		if (ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
			ic->ic_extender_mbs_wgt = value;
		break;
	case IEEE80211_PARAM_VAP_TX_AMSDU:
		if (vap->iv_tx_amsdu != value) {
			vap->iv_tx_amsdu = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			ieee80211_wireless_reassoc(vap, 0, 1);
		}
		break;
	case IEEE80211_PARAM_TX_MAXMPDU:
		if ((value < IEEE80211_VHTCAP_MAX_MPDU_3895) ||
			(value > IEEE80211_VHTCAP_MAX_MPDU_11454)) {
			retv = EINVAL;
		} else if (vap->iv_tx_max_amsdu != value) {
			vap->iv_tx_max_amsdu = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		}
		break;
	case IEEE80211_PARAM_EXTENDER_RBS_WGT:
		if (ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
			ic->ic_extender_rbs_wgt = value;
		break;
	case IEEE80211_PARAM_EXTENDER_VERBOSE:
		if (ieee80211_swfeat_is_supported(SWFEAT_ID_QHOP, 1))
			ic->ic_extender_verbose = value;
		break;
        case IEEE80211_PARAM_BB_PARAM:
                {
			/* Channel will check bb_param and change default max gain for high band channel */
                        sp->bb_param = value;
                }
                break;
	 case IEEE80211_PARAM_TQEW_DESCR_LIMIT:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if ((value > 0) && (value <= 100)) {
				ic->ic_tqew_descr_limit = value;
				ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
			} else {
				retv = EINVAL;
			}
		} else {
			retv = EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_SWFEAT_DISABLE:
		ieee80211_ioctl_swfeat_disable(vap, param, value);
		break;
	case IEEE80211_PARAM_HS2:
		if (!ieee80211_swfeat_is_supported(SWFEAT_ID_HS20, 1)) {
			retv = EOPNOTSUPP;
			break;
		}
		if (vap->iv_opmode == IEEE80211_M_HOSTAP)
			vap->hs20_enable = value;
		break;
	case IEEE80211_PARAM_DGAF_CONTROL:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			vap->disable_dgaf = !!value;
		} else {
			return -EINVAL;
		}
		break;
        case IEEE80211_PARAM_11N_AMSDU_CTRL:
		if (value)
			ic->ic_flags_qtn |= QTN_NODE_11N_TXAMSDU_OFF;
		else
			ic->ic_flags_qtn &= ~QTN_NODE_11N_TXAMSDU_OFF;
                break;
	case IEEE80211_PARAM_SCAN_RESULTS_CHECK_INV :
		if (value > 0 && value != ic->ic_scan_results_check) {
			ic->ic_scan_results_check = value;
			mod_timer(&ic->ic_scan_results_expire,
					jiffies + ic->ic_scan_results_check * HZ);
		}
		break;
	case IEEE80211_PARAM_FLUSH_SCAN_ENTRY:
		if (value)
			ieee80211_scan_flush(ic);
		break;
	case IEEE80211_PARAM_VHT_OPMODE_BW:
		ieee80211_send_vht_opmode_to_all(ic, (uint8_t)value);
		break;
	case IEEE80211_PARAM_PROXY_ARP:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			vap->proxy_arp = !!value;
		} else {
			return -EINVAL;
		}
		break;
	case IEEE80211_PARAM_QTN_HAL_PM_CORRUPT_DEBUG:
#ifdef QTN_HAL_PM_CORRUPT_DEBUG
		if (value)
			sp->qtn_hal_pm_corrupt_debug = 1;
		else
			sp->qtn_hal_pm_corrupt_debug = 0;
#else
		sp->qtn_hal_pm_corrupt_debug = 0;
		printk("pm_corrupt_debug can't be activated when QTN_HAL_PM_CORRUPT_DEBUG "
				"is undefined\n");
#endif
		break;
	case IEEE80211_PARAM_L2_EXT_FILTER:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			return ic->ic_set_l2_ext_filter(vap, value);
		} else {
			return -EINVAL;
		}
		break;
	case IEEE80211_PARAM_L2_EXT_FILTER_PORT:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP)
			return ic->ic_set_l2_ext_filter_port(vap, value);
		else
			return -EINVAL;
		break;
	case IEEE80211_PARAM_ENABLE_RX_OPTIM_STATS:
		if (value >= 0) {
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_PARAM_SET_UNICAST_QUEUE_NUM:
		topaz_congest_set_unicast_queue_count(value);
                break;
	case IEEE80211_PARAM_MRC_ENABLE:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_OBSS_EXEMPT_REQ:
		if (ic->ic_opmode == IEEE80211_M_STA) {
			if (value) {
				vap->iv_coex |= WLAN_20_40_BSS_COEX_OBSS_EXEMPT_REQ;
				ieee80211_send_20_40_bss_coex(vap);
			} else {
				vap->iv_coex &= ~WLAN_20_40_BSS_COEX_OBSS_EXEMPT_REQ;
			}
		} else
			retv = EOPNOTSUPP;
		break;
	case IEEE80211_PARAM_OBSS_TRIGG_SCAN_INT:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			ic->ic_obss_ie.obss_trigger_interval = value;
			ic->ic_beacon_update(vap);
		} else
			retv = EOPNOTSUPP;
		break;
	case IEEE80211_PARAM_PREF_BAND:
		vap->iv_pref_band = value;
		break;
	case IEEE80211_PARAM_BW_2_4GHZ:
		if ((value != BW_HT160) && (value != BW_HT80) &&
				(value != BW_HT40) && (value != BW_HT20))
			return -EINVAL;

		/* update 2.4ghz station profile */
		if ((ic->ic_rf_chipid == CHIPID_DUAL) &&
				(ic->ic_opmode == IEEE80211_M_STA))
			vap->iv_2_4ghz_prof.bw = value;

		/* check if phymode is also in 2.4ghz mode then set bw */
		if (IS_IEEE80211_24G_BAND(ic)) {
			/* Blocking 40MHZ in 2.4 11G only mode */
			if ((value >= BW_HT40) && IEEE80211_IS_11G(ic))
				return -EOPNOTSUPP;

			ic->ic_max_system_bw = value;

			/* Forward bandwidth configuration (enabling 40 MHz BW) to the driver and MuC */
			ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BW_SEL_MUC, value, NULL, 0);

			ieee80211_start_obss_scan_timer(vap);
			ic->ic_csw_reason = IEEE80211_CSW_REASON_CONFIG;
			if (IS_UP_AUTO(vap)) {
				ieee80211_wireless_reassoc(vap, 0, 1);
				ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
			}
		}
		break;
	case IEEE80211_PARAM_ALLOW_VHT_TKIP:
		if (!WPA_TKIP_SUPPORT)
			return -EOPNOTSUPP;

		TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
			vap_each->allow_tkip_for_vht = value;
			if ((vap_each->iv_opmode == IEEE80211_M_HOSTAP) &&
				(vap_each->iv_state == IEEE80211_S_RUN)) {
				ic->ic_beacon_update(vap_each);
			}

			ieee80211_wireless_reassoc(vap_each, 0, 1);
		}
		break;
	case IEEE80211_PARAM_VHT_OPMODE_NOTIF:
		ic->ic_vht_opmode_notif = (uint16_t)value;
		TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
			if ((vap_each->iv_opmode == IEEE80211_M_HOSTAP) &&
				(vap_each->iv_state == IEEE80211_S_RUN)) {
				ic->ic_beacon_update(vap_each);
			}
		}
		break;
	case IEEE80211_PARAM_USE_NON_HT_DUPLICATE_MU:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		if (ic->use_non_ht_duplicate_for_mu != value) {
			ieee80211_wireless_reassoc(vap, 0, 1);
		}

		ic->use_non_ht_duplicate_for_mu = value;
		break;
	case IEEE80211_PARAM_QTN_BLOCK_BSS:
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "%s block state is from %d to %d\n",
			vap->iv_dev->name, vap->is_block_all_assoc, value);

		vap->is_block_all_assoc = !!value;
		break;
	case IEEE80211_PARAM_VHT_2_4GHZ:
		if (value)
			ic->ic_flags_ext |= IEEE80211_FEXT_24GVHT;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_24GVHT;
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_VHT_2_4GHZ, value, NULL, 0);

		TAILQ_FOREACH(vap_each, &ic->ic_vaps, iv_next)
			ieee80211_wireless_reassoc(vap_each, 0, 0);
		break;
	case IEEE80211_PARAM_BEACONING_SCHEME:
		value = value ? QTN_BEACONING_SCHEME_1 : QTN_BEACONING_SCHEME_0;
		if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
				ic->ic_beaconing_scheme != value) {
			ic->ic_beaconing_scheme = value;
			ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		}
		break;
#if defined(QBMPS_ENABLE)
	case IEEE80211_PARAM_STA_BMPS:
		retv = ieee80211_wireless_set_sta_bmps(vap, ic, value);
		break;
#endif
	case IEEE80211_PARAM_40MHZ_INTOLERANT:
		ieee80211_wireless_set_40mhz_intolerant(vap, value);
		break;
	case IEEE80211_PARAM_DISABLE_TX_BA:
		vap->tx_ba_disable = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		printk("%s: TX Block Ack establishment disable configured to %u\n",dev->name, value);
		ieee80211_wireless_reassoc(vap, 0, 0);
		break;
	case IEEE80211_PARAM_DECLINE_RX_BA:
		vap->rx_ba_decline = value;
		printk("%s: RX Block Ack decline configured to %u\n",dev->name, value);
		ieee80211_wireless_reassoc(vap, 0, 0);
		break;
	case IEEE80211_PARAM_VAP_STATE:
		vap->iv_vap_state = !!value;
		break;
	case IEEE80211_PARAM_TX_AIRTIME_CONTROL:
		ic->ic_tx_airtime_control(vap, value);
		break;
	case IEEE80211_PARAM_OSEN:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			vap->iv_osen = value;
			if (vap->iv_flags & IEEE80211_F_WPA)
				retv = ENETRESET;
		} else {
			retv = EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_OBSS_SCAN:
		if (ic->ic_obss_scan_enable != !!value) {
			ic->ic_obss_scan_enable = !!value;
			if (ic->ic_obss_scan_enable)
				ieee80211_start_obss_scan_timer(vap);
			else
				del_timer_sync(&ic->ic_obss_timer);
		}
		break;
	case IEEE80211_PARAM_SHORT_SLOT:
		if (value) {
			ic->ic_caps |= IEEE80211_C_SHSLOT;
			ic->ic_flags |= IEEE80211_F_SHSLOT;
		} else {
			ic->ic_caps &= ~IEEE80211_C_SHSLOT;
			ic->ic_flags &= ~IEEE80211_F_SHSLOT;
		}
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_BG_PROTECT:
		if (ic->ic_rf_chipid == CHIPID_DUAL
				&& IS_IEEE80211_24G_BAND(ic)) {
			if (value) {
				ic->ic_flags_ext |= IEEE80211_FEXT_BG_PROTECT;
			} else {
				ic->ic_flags_ext &= ~IEEE80211_FEXT_BG_PROTECT;
				ic->ic_set_11g_erp(vap, 0);
			}
			ieee80211_wireless_reassoc(vap, 0, 1);
		} else {
			retv = EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_11N_PROTECT:
		if (value) {
			ic->ic_flags_ext |= IEEE80211_FEXT_11N_PROTECT;
		} else {
			ic->ic_flags_ext &= ~IEEE80211_FEXT_11N_PROTECT;
			if (ic->ic_local_rts &&
				(vap->iv_opmode == IEEE80211_M_STA ||
				vap->iv_opmode == IEEE80211_M_WDS)) {
				ic->ic_local_rts = 0;
				ic->ic_use_rtscts(ic);
			}
		}
		ieee80211_wireless_reassoc(vap, 0, 1);
		break;
	case IEEE80211_PARAM_MU_NDPA_BW_SIGNALING_SUPPORT:
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		ic->rx_bws_support_for_mu_ndpa = value;
		break;
	case IEEE80211_PARAM_WPA_STARTED:
	case IEEE80211_PARAM_HOSTAP_STARTED:
		ic->hostap_wpa_state = value;
		break;
	case IEEE80211_PARAM_BSS_GROUP_ID:
		retv = ieee80211_ioctl_set_bss_grpid(ic, vap, value);
		break;
	case IEEE80211_PARAM_BSS_ASSOC_RESERVE:
		retv = ieee80211_ioctl_set_bss_grp_assoc_reserve(ic, vap, value);
		break;
	case IEEE80211_PARAM_MAX_BCAST_PPS:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
			return -EINVAL;
		}
		vap->bcast_pps.max_bcast_pps = value;
		if (vap->bcast_pps.max_bcast_pps) {
			vap->bcast_pps.rx_bcast_counter = 0;
			vap->bcast_pps.rx_bcast_pps_start_time = jiffies + HZ;
			vap->bcast_pps.tx_bcast_counter = 0;
			vap->bcast_pps.tx_bcast_pps_start_time = jiffies + HZ;
		}
		break;
	case IEEE80211_PARAM_MAX_BOOT_CAC_DURATION:
		if ((vap->iv_opmode != IEEE80211_M_HOSTAP)
			|| ((value > 0) && (value < MIN_CAC_PERIOD))
			|| ((value > 0) && (!(ic->ic_dfs_is_eu_region())))) {
			return -EINVAL;
		}
		if (ic->ic_set_init_cac_duration) {
			ic->ic_set_init_cac_duration(ic, value);
		}
		/* Start ICAC procedures */
		if (ic->ic_start_icac_procedure) {
			ic->ic_start_icac_procedure(ic);
		}

		break;
	case IEEE80211_PARAM_RX_BAR_SYNC:
		ic->ic_rx_bar_sync = value;
		ieee80211_param_to_qdrv(vap, param, value, NULL, 0);
		break;
	case IEEE80211_PARAM_STOP_ICAC:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (ic->ic_get_init_cac_duration(ic) > 0) {
				ic->ic_stop_icac_procedure(ic);
				printk(KERN_DEBUG "ICAC: Aborted ICAC due to set channel request\n");
			}
		}
		break;
	case IEEE80211_PARAM_STA_DFS_STRICT_MODE:
		if ((vap->iv_opmode != IEEE80211_M_STA)	|| (!(ic->ic_dfs_is_eu_region()))) {
			return -EINVAL;
		}
		if (value) {
			/* sta_dfs must be enabled to support sta_dfs_strict mode */
			ic->ic_flags_ext |= IEEE80211_FEXT_MARKDFS;
		} else {
			ic->ic_flags_ext &= ~IEEE80211_FEXT_MARKDFS;
		}
		sp->csa_lhost->sta_dfs_strict_mode = !!value;
		ic->ic_enable_sta_dfs(!!value);
		ic->ic_set_radar(!!value);
		ic->sta_dfs_info.sta_dfs_strict_mode = !!value;
		break;
	case IEEE80211_PARAM_STA_DFS_STRICT_MEASUREMENT_IN_CAC:
		if ((vap->iv_opmode != IEEE80211_M_STA) || (!(ic->ic_dfs_is_eu_region()))) {
			return -EINVAL;
		}
		ic->sta_dfs_info.sta_dfs_strict_msr_cac = !!value;
		break;
	case IEEE80211_PARAM_STA_DFS_STRICT_TX_CHAN_CLOSE_TIME:
		if ((value >= STA_DFS_STRICT_TX_CHAN_CLOSE_TIME_MIN)
			&& (value <= STA_DFS_STRICT_TX_CHAN_CLOSE_TIME_MAX)) {
			ic->sta_dfs_info.sta_dfs_tx_chan_close_time = value;
		} else {
			return -EINVAL;
		}
		break;
	case IEEE80211_PARAM_NEIGHBORHOOD_THRSHD:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;

		ieee80211_set_threshold_of_neighborhood_type(ic, value >> 16, value & 0xFFFF);
		break;
	case IEEE80211_PARAM_DFS_CSA_CNT:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;

		if (value <= 0)
			return -EINVAL;

		/* Should not longer than Channel Closing Transmission Time (1s) */
		TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
			temp_value = IEEE80211_TU_TO_JIFFIES(vap_each->iv_bss->ni_intval);
			temp_value *= value;
			if (temp_value >= HZ)
				return -EINVAL;
		}

		ic->ic_dfs_csa_cnt = value;
		break;
	default:
		retv = EOPNOTSUPP;
		break;
	}

	if (retv == ENETRESET)
		retv = IS_UP_AUTO(vap) ? ieee80211_open(vap->iv_dev) : 0;

	return -retv;
}

/*
 * Issue two commands to overcome the short range association issue:
 * 1) Change the transmit power level
 * 2) Change the rx gain and agc
 */
int ieee80211_pwr_adjust(struct ieee80211vap *vap,  int rxgain_state)
{
	int args[2];
	int retval = 0;
	int anychan = 0;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s: Enabling %s gain Settings\n",
			  __func__, rxgain_state ? "Low" : "High");

	if (rxgain_state)
		retval = apply_tx_power(vap, IEEE80211_TXPOW_ENCODE(anychan),
					IEEE80211_APPLY_LOWGAIN_TXPOWER);
	else
		retval = apply_tx_power(vap, IEEE80211_TXPOW_ENCODE(anychan),
					IEEE80211_APPLY_TXPOWER_NORMAL);

	if (retval >= 0) {
		args[0] = IEEE80211_PARAM_PWR_ADJUST;
		args[1] = rxgain_state;
		retval = ieee80211_ioctl_setparam(vap->iv_dev, NULL, NULL, (char*)args);
	}

	return retval;
}
EXPORT_SYMBOL(ieee80211_pwr_adjust);

static int
ieee80211_ioctl_getparam_txpower(struct ieee80211vap *vap, int *param)
{
	struct ieee80211com *ic = vap->iv_ic;
	int chan = (param[0] >> 16);
	int retval = -EINVAL;

	/* Retrieve and see if we are in low gain state and return power accordingly */
	ieee80211_param_from_qdrv(vap, param[0] & 0xffff, &param[0], NULL, 0);

	if (chan <= IEEE80211_CHAN_MAX && chan > 0 && isset(ic->ic_chan_active, chan)) {
		const struct ieee80211_channel *c = findchannel(ic, chan, IEEE80211_MODE_AUTO);

		if (c != NULL) {
			if (param[0] == 1)
				param[0] = IEEE80211_LOWGAIN_TXPOW_MAX;
			else
				param[0] = c->ic_maxpower_normal;
			retval = 0;
		}
	}

	return retval;
}
static int
ieee80211_ioctl_getparam_bw_txpower(struct ieee80211vap *vap, int *param)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t chan = (((uint32_t)param[0]) >> 24) & 0xff;
	uint32_t bf_on = (param[0] >> 20) & 0xf;
	uint32_t num_ss = (param[0] >> 16) & 0xf;
	int retval = -EINVAL;

	if (chan <= IEEE80211_CHAN_MAX &&
			isset(ic->ic_chan_active, chan) &&
			num_ss <= IEEE80211_QTN_NUM_RF_STREAMS) {
		const struct ieee80211_channel *c = findchannel(ic, chan, IEEE80211_MODE_AUTO);
		uint32_t idx_bf = PWR_IDX_BF_OFF + bf_on;
		uint32_t idx_ss = PWR_IDX_1SS + num_ss - 1;

		if (c != NULL && idx_bf < PWR_IDX_BF_MAX && idx_ss < PWR_IDX_SS_MAX) {
			param[0] = ((c->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_80M] & 0xff) << 16) |
					((c->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_40M] & 0xff) << 8) |
					(c->ic_maxpower_table[idx_bf][idx_ss][PWR_IDX_20M] & 0xff);
			retval = 0;
		}
	}

	return retval;
}

static int
ieee80211_param_ocac_get(struct ieee80211vap *vap, int *param)
{
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t param_id = (((uint32_t)param[0]) >> 16) & 0xffff;
	int retval = 0;

	switch(param_id) {
	case IEEE80211_OCAC_GET_STATUS:
		param[0] = ic->ic_ocac.ocac_cfg.ocac_enable;
		break;
	case IEEE80211_OCAC_GET_AVAILABILITY:
		param[0] = !ieee80211_wireless_is_ocac_unsupported(vap);
		break;
	default:
		retval = -EINVAL;
		break;
	}

	return retval;
}

static int
ieee80211_ioctl_getmode(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *wri, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	switch (vap->iv_ic->ic_phymode ) {
		case IEEE80211_MODE_11A:
			strcpy(extra, "11a");
			break;
		case IEEE80211_MODE_11B:
			strcpy(extra, "11b");
			break;
		case IEEE80211_MODE_11NG_HT40PM:
			strcpy(extra, "11ng40");
			break;
		case IEEE80211_MODE_11NA_HT40PM:
			if (vap->iv_11ac_and_11n_flag & IEEE80211_11N_ONLY) {
				strcpy(extra, "11nOnly40");
			} else {
				strcpy(extra, "11na40");
			}
			break;
		case IEEE80211_MODE_11NG:
			strcpy(extra, "11ng");
			break;
		case IEEE80211_MODE_11G:
			strcpy(extra, "11g");
			break;
		case IEEE80211_MODE_11NA:
			if (vap->iv_11ac_and_11n_flag & IEEE80211_11N_ONLY) {
				strcpy(extra, "11nOnly20");
			} else {
				strcpy(extra, "11na20");
			}
			break;
		case IEEE80211_MODE_FH:
			strcpy(extra, "FH");
			break;
		case IEEE80211_MODE_11AC_VHT20PM:
			if (vap->iv_11ac_and_11n_flag & IEEE80211_11AC_ONLY) {
				strcpy(extra, "11acOnly20");
			} else {
				strcpy(extra, "11ac20");
			}
			break;
		case IEEE80211_MODE_11AC_VHT40PM:
			if (vap->iv_11ac_and_11n_flag & IEEE80211_11AC_ONLY) {
				strcpy(extra, "11acOnly40");
			} else {
				strcpy(extra, "11ac40");
			}
			break;
		case IEEE80211_MODE_11AC_VHT80PM:
			if (vap->iv_11ac_and_11n_flag & IEEE80211_11AC_ONLY) {
				strcpy(extra, "11acOnly80");
			} else {
				if (IEEE80211_IS_CHAN_VHT80_EDGEPLUS(vap->iv_ic->ic_curchan))
					strcpy(extra, "11ac80Edge+");
				else if (IEEE80211_IS_CHAN_VHT80_CNTRPLUS(vap->iv_ic->ic_curchan))
					strcpy(extra, "11ac80Cntr+");
				else if (IEEE80211_IS_CHAN_VHT80_CNTRMINUS(vap->iv_ic->ic_curchan))
					strcpy(extra, "11ac80Cntr-");
				else if (IEEE80211_IS_CHAN_VHT80_EDGEMINUS(vap->iv_ic->ic_curchan))
					strcpy(extra, "11ac80Edge-");
			}
			break;
		case IFM_AUTO:
			strcpy(extra, "auto");
			break;
		default:
			return -EINVAL;
	}
	wri->length = strlen(extra);
	strncpy(wri->pointer, extra, wri->length);
	return 0;
}

static void
ieee80211_blacklist_node_print(void *arg, struct ieee80211_node *ni)
{
	if (ni->ni_blacklist_timeout > 0) {
		printf("%s\n", ether_sprintf(ni->ni_macaddr));
	}
}

static uint32_t
ieee80211_param_wowlan_get(struct ieee80211com *ic)
{
	return ((ic->ic_wowlan.host_state << 31) |
			(ic->ic_wowlan.wowlan_match << 29) |
			((ic->ic_wowlan.L3_udp_port&0x1fff) << 16) |
			ic->ic_wowlan.L2_ether_type);
}

static void
ieee80211_extdr_dump_flags(struct ieee80211vap *vap)
{
	const char *wds_mode;

	if (IEEE80211_VAP_WDS_IS_MBS(vap))
		wds_mode = "MBS";
	else if (IEEE80211_VAP_WDS_IS_RBS(vap))
		wds_mode = "RBS";
	else
		wds_mode = "WDS";

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_EXTDR,
			"Legay VAP QHOP mode: %s\n", wds_mode);
}

static int
ieee80211_ioctl_getparam(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
	int *param = (int *) extra;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
#if defined(QBMPS_ENABLE)
	struct qdrv_vap *qv = container_of(vap, struct qdrv_vap, iv);
#endif

	switch (param[0] & 0xffff) {
	case IEEE80211_PARAM_AP_ISOLATE:
		param[0] = br_get_ap_isolate();
		break;
	case IEEE80211_PARAM_AUTHMODE:
		if (vap->iv_flags & IEEE80211_F_WPA)
			param[0] = IEEE80211_AUTH_WPA;
		else
			param[0] = vap->iv_bss->ni_authmode;
		break;
	case IEEE80211_PARAM_PROTMODE:
		param[0] = ic->ic_protmode;
		break;
	case IEEE80211_PARAM_MCASTCIPHER:
		param[0] = rsn->rsn_mcastcipher;
		break;
	case IEEE80211_PARAM_MCASTKEYLEN:
		param[0] = rsn->rsn_mcastkeylen;
		break;
	case IEEE80211_PARAM_UCASTCIPHERS:
		param[0] = rsn->rsn_ucastcipherset;
		break;
	case IEEE80211_PARAM_UCASTCIPHER:
		param[0] = rsn->rsn_ucastcipher;
		break;
	case IEEE80211_PARAM_UCASTKEYLEN:
		param[0] = rsn->rsn_ucastkeylen;
		break;
	case IEEE80211_PARAM_KEYMGTALGS:
		param[0] = rsn->rsn_keymgmtset;
		break;
	case IEEE80211_PARAM_RSNCAPS:
		param[0] = rsn->rsn_caps;
		break;
	case IEEE80211_PARAM_WPA:
		switch (vap->iv_flags & IEEE80211_F_WPA) {
		case IEEE80211_F_WPA1:
			param[0] = 1;
			break;
		case IEEE80211_F_WPA2:
			param[0] = 2;
			break;
		case IEEE80211_F_WPA1 | IEEE80211_F_WPA2:
			param[0] = 3;
			break;
		default:
			param[0] = 0;
			break;
		}
		break;
	case IEEE80211_PARAM_ROAMING:
		param[0] = ic->ic_roaming;
		break;
	case IEEE80211_PARAM_PRIVACY:
		param[0] = (vap->iv_flags & IEEE80211_F_PRIVACY) != 0;
		break;
	case IEEE80211_PARAM_DROPUNENCRYPTED:
		param[0] = (vap->iv_flags & IEEE80211_F_DROPUNENC) != 0;
		break;
	case IEEE80211_PARAM_DROPUNENC_EAPOL:
		param[0] = IEEE80211_VAP_DROPUNENC_EAPOL(vap);
		break;
	case IEEE80211_PARAM_COUNTERMEASURES:
		param[0] = (vap->iv_flags & IEEE80211_F_COUNTERM) != 0;
		break;
	case IEEE80211_PARAM_DRIVER_CAPS:
		param[0] = vap->iv_caps;
		break;
	case IEEE80211_PARAM_WMM:
		param[0] = (vap->iv_flags & IEEE80211_F_WME) != 0;
		break;
	case IEEE80211_PARAM_HIDESSID:
		param[0] = (vap->iv_flags & IEEE80211_F_HIDESSID) != 0;
		break;
	case IEEE80211_PARAM_APBRIDGE:
		param[0] = (vap->iv_flags & IEEE80211_F_NOBRIDGE) == 0;
		break;
	case IEEE80211_PARAM_INACT:
		param[0] = vap->iv_inact_run * IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PARAM_INACT_AUTH:
		param[0] = vap->iv_inact_auth * IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PARAM_INACT_INIT:
		param[0] = vap->iv_inact_init * IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PARAM_DTIM_PERIOD:
		param[0] = vap->iv_dtim_period;
		break;
	case IEEE80211_PARAM_BEACON_INTERVAL:
		/* NB: get from ic_bss for station mode */
		param[0] = ic->ic_lintval_backup;
		break;
	case IEEE80211_PARAM_DOTH:
		param[0] = (ic->ic_flags & IEEE80211_F_DOTH) != 0;
		break;
	case IEEE80211_PARAM_SHPREAMBLE:
		param[0] = (ic->ic_caps & IEEE80211_C_SHPREAMBLE) != 0;
		break;
	case IEEE80211_PARAM_PWRCONSTRAINT:
		if (ic->ic_flags & IEEE80211_F_DOTH && ic->ic_flags_ext & IEEE80211_FEXT_TPC)
			param[0] = ic->ic_pwr_constraint;
		else
			return -EOPNOTSUPP;
		break;
	case IEEE80211_PARAM_PUREG:
		param[0] = (vap->iv_flags & IEEE80211_F_PUREG) != 0;
		break;
	case IEEE80211_PARAM_WDS:
		param[0] = ((vap->iv_flags_ext & IEEE80211_FEXT_WDS) == IEEE80211_FEXT_WDS);
		break;
	case IEEE80211_PARAM_REPEATER:
		param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER);
		break;
	case IEEE80211_PARAM_BGSCAN:
		param[0] = (vap->iv_flags & IEEE80211_F_BGSCAN) != 0;
		break;
	case IEEE80211_PARAM_BGSCAN_IDLE:
		param[0] = jiffies_to_msecs(vap->iv_bgscanidle); /* ms */
		break;
	case IEEE80211_PARAM_BGSCAN_INTERVAL:
		param[0] = vap->iv_bgscanintvl / HZ;	/* seconds */
		break;
	case IEEE80211_PARAM_SCAN_OPCHAN:
		param[0] = ic->ic_scan_opchan_enable;
		break;
	case IEEE80211_PARAM_EXTENDER_MBS_RSSI_MARGIN:
		param[0] = ic->ic_extender_mbs_rssi_margin;
		break;
	case IEEE80211_PARAM_MCAST_RATE:
		param[0] = vap->iv_mcast_rate;	/* seconds */
		break;
	case IEEE80211_PARAM_COVERAGE_CLASS:
		param[0] = ic->ic_coverageclass;
		break;
	case IEEE80211_PARAM_COUNTRY_IE:
		param[0] = (ic->ic_flags_ext & IEEE80211_FEXT_COUNTRYIE) != 0;
		break;
	case IEEE80211_PARAM_REGCLASS:
		param[0] = (ic->ic_flags_ext & IEEE80211_FEXT_REGCLASS) != 0;
		break;
	case IEEE80211_PARAM_SCANVALID:
		param[0] = vap->iv_scanvalid / HZ;	/* seconds */
		break;
	case IEEE80211_PARAM_ROAM_RSSI_11A:
		param[0] = vap->iv_roam.rssi11a;
		break;
	case IEEE80211_PARAM_ROAM_RSSI_11B:
		param[0] = vap->iv_roam.rssi11bOnly;
		break;
	case IEEE80211_PARAM_ROAM_RSSI_11G:
		param[0] = vap->iv_roam.rssi11b;
		break;
	case IEEE80211_PARAM_ROAM_RATE_11A:
		param[0] = vap->iv_roam.rate11a;
		break;
	case IEEE80211_PARAM_ROAM_RATE_11B:
		param[0] = vap->iv_roam.rate11bOnly;
		break;
	case IEEE80211_PARAM_ROAM_RATE_11G:
		param[0] = vap->iv_roam.rate11b;
		break;
	case IEEE80211_PARAM_UAPSDINFO:
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			if (IEEE80211_VAP_UAPSD_ENABLED(vap))
				param[0] = 1;
			else
				param[0] = 0;
		} else if (vap->iv_opmode == IEEE80211_M_STA)
			param[0] = vap->iv_uapsdinfo;
		break;
	case IEEE80211_PARAM_SLEEP:
		param[0] = vap->iv_bss->ni_flags & IEEE80211_NODE_PWR_MGT;
		break;
	case IEEE80211_PARAM_EOSPDROP:
		param[0] = IEEE80211_VAP_EOSPDROP_ENABLED(vap);
		break;
	case IEEE80211_PARAM_STA_DFS:
	case IEEE80211_PARAM_MARKDFS:
		if (ic->ic_flags_ext & IEEE80211_FEXT_MARKDFS)
			param[0] = 1;
		else
			param[0] = 0;
		break;
	case IEEE80211_PARAM_SHORT_GI:
		/* Checking both SGI flags, if SGI is enabled/disabled */
		if (IS_IEEE80211_VHT_ENABLED(ic)) {
			param[0] = (ic->ic_vhtcap.cap_flags & IEEE80211_VHTCAP_C_SHORT_GI_80) ? 1 : 0;
		} else {
			param[0] = (vap->iv_ht_flags & IEEE80211_HTF_SHORTGI_ENABLED &&
				    (ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 ||
				     ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40)) ? 1 : 0;
		}
		break;
	case IEEE80211_PARAM_MCS_CAP:
		if ((vap->iv_mcs_config & IEEE80211_RATE_PREFIX_MASK) == IEEE80211_N_RATE_PREFIX) {
			param[0] = (vap->iv_mcs_config & 0xFF);
		} else {
			printk("11N MCS is not set\n");
			return -EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_LEGACY_RETRY_LIMIT:
	case IEEE80211_PARAM_RETRY_COUNT:
	case IEEE80211_PARAM_TXBF_PERIOD:
	case IEEE80211_PARAM_TXBF_CTRL:
	case IEEE80211_PARAM_GET_RFCHIP_ID:
	case IEEE80211_PARAM_GET_RFCHIP_VERID:
	case IEEE80211_PARAM_LDPC:
	case IEEE80211_PARAM_STBC:
	case IEEE80211_PARAM_RTS_CTS:
	case IEEE80211_PARAM_TX_QOS_SCHED:
	case IEEE80211_PARAM_PEER_RTS_MODE:
	case IEEE80211_PARAM_DYN_WMM:
	case IEEE80211_PARAM_11N_40_ONLY_MODE:
	case IEEE80211_PARAM_MAX_MGMT_FRAMES:
	case IEEE80211_PARAM_MCS_ODD_EVEN:
	case IEEE80211_PARAM_RESTRICTED_MODE:
	case IEEE80211_PARAM_RESTRICT_RTS:
	case IEEE80211_PARAM_RESTRICT_LIMIT:
	case IEEE80211_PARAM_RESTRICT_RATE:
	case IEEE80211_PARAM_SWRETRY_AGG_MAX:
	case IEEE80211_PARAM_SWRETRY_NOAGG_MAX:
	case IEEE80211_PARAM_SWRETRY_SUSPEND_XMIT:
	case IEEE80211_PARAM_RX_AGG_TIMEOUT:
	case IEEE80211_PARAM_CARRIER_ID:
	case IEEE80211_PARAM_TX_QUEUING_ALG:
	case IEEE80211_PARAM_CONGEST_IDX:
	case IEEE80211_PARAM_MICHAEL_ERR_CNT:
	case IEEE80211_PARAM_MAX_AGG_SIZE:
	case IEEE80211_PARAM_MU_ENABLE:
	case IEEE80211_PARAM_MU_USE_EQ:
	case IEEE80211_PARAM_RESTRICT_WLAN_IP:
		ieee80211_param_from_qdrv(vap, param[0], &param[0], NULL, 0);
		break;
	case IEEE80211_PARAM_GET_MU_GRP_QMAT:
		ieee80211_param_from_qdrv(vap, param[0] & 0xffff, &param[0], NULL, 0);
		break;
	case IEEE80211_PARAM_BW_SEL_MUC:
		param[0] = ieee80211_get_bw(ic);
		break;
	case IEEE80211_PARAM_MODE:
		if (IS_IEEE80211_5G_BAND(ic))
			ieee80211_param_from_qdrv(vap, param[0], &param[0], NULL, 0);
		else
			param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_24GVHT);
		break;
	case IEEE80211_PARAM_HT_NSS_CAP:
		param[0] = ic->ic_ht_nss_cap;
		break;
	case IEEE80211_PARAM_VHT_NSS_CAP:
		param[0] = ic->ic_vht_nss_cap;
		break;
	case IEEE80211_PARAM_VHT_MCS_CAP:
		param[0] = ic->ic_vht_mcs_cap;
		break;
	case IEEE80211_PARAM_RTSTHRESHOLD:
		param[0] = vap->iv_rtsthreshold;
		break;
	case IEEE80211_PARAM_AMPDU_DENSITY:
		param[0] = ic->ic_htcap.mpduspacing;
		break;
	case IEEE80211_PARAM_SCANSTATUS:
		if (ic->ic_flags & IEEE80211_F_SCAN) {
			param[0] = 1;
		} else {
			param[0] = 0;
		}
		break;
	case IEEE80211_PARAM_CACSTATUS:
		ieee80211_param_from_qdrv(vap, param[0], &param[0], NULL, 0);
		break;
	case IEEE80211_PARAM_IMPLICITBA:
		param[0] = vap->iv_implicit_ba;
		break;
	case IEEE80211_PARAM_GLOBAL_BA_CONTROL:
		param[0] = vap->iv_ba_control;
		break;
	case IEEE80211_PARAM_VAP_STATS:
		{
			param[0] = 0; // no meaning

			printk("RX stats (delta)\n");
			printk("  dup:\t%u\n", vap->iv_stats.is_rx_dup);
			printk("  beacon:\t%u\n", vap->iv_stats.is_rx_beacon);
			printk("  elem_missing:\t%u\n", vap->iv_stats.is_rx_elem_missing);
			printk("  badchan:\t%u\n", vap->iv_stats.is_rx_badchan);
			printk("  chanmismatch:\t%u\n", vap->iv_stats.is_rx_chanmismatch);

			// clear
			memset(&vap->iv_stats, 0, sizeof(vap->iv_stats));
		}
		break;
	case IEEE80211_PARAM_DFS_FAST_SWITCH:
		param[0] = ((ic->ic_flags_ext & IEEE80211_FEXT_DFS_FAST_SWITCH) != 0);
		break;
	case IEEE80211_PARAM_SCAN_NO_DFS:
		param[0] = ((ic->ic_flags_ext & IEEE80211_FEXT_SCAN_NO_DFS) != 0);
		break;
	case IEEE80211_PARAM_BLACKLIST_GET:
		ieee80211_iterate_dev_nodes(dev,
			&ic->ic_sta, ieee80211_blacklist_node_print, NULL, 0);
		param[0] = 0;
		break;
	case IEEE80211_PARAM_FIXED_TX_RATE:
		param[0] = vap->iv_mcs_config;
		break;
	case IEEE80211_PARAM_REGULATORY_REGION:
		{
			union {
				char		as_chars[ 4 ];
				u_int32_t	as_u32;
			} region;

			if (ieee80211_countryid_to_country_string( ic->ic_country_code, region.as_chars ) != 0) {
				/*
				 * If we can't get a country string from the current code,
				 * return the NULL string as the region.
				 */
				region.as_u32 = 0;
			}

			param[0] = (int) region.as_u32;
		}
		break;
	case IEEE80211_PARAM_SAMPLE_RATE:
		param[0] = ic->ic_sample_rate;
		break;
	case IEEE80211_PARAM_CONFIG_TXPOWER:
		{
			int retval = ieee80211_ioctl_getparam_txpower(vap, param);
			if (retval != 0) {
				return retval;
			}
		}
		break;
	case IEEE80211_PARAM_CONFIG_BW_TXPOWER:
		{
			int retval = ieee80211_ioctl_getparam_bw_txpower(vap, param);
			if (retval != 0) {
				return retval;
			}
		}
		break;
	case IEEE80211_PARAM_TPC:
		if (ic->ic_flags & IEEE80211_F_DOTH) {
			param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_TPC);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_CONFIG_TPC_INTERVAL:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			param[0] = ieee80211_tpc_query_get_interval(&ic->ic_tpc_query_info);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_TPC_QUERY:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			param[0] = ieee80211_tpc_query_state(&ic->ic_tpc_query_info);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_CONFIG_REGULATORY_TXPOWER:
		{
			int chan = (param[0] >> 16);

			if (chan <= IEEE80211_CHAN_MAX && chan > 0 && isset(ic->ic_chan_active, chan)) {
				const struct ieee80211_channel *c = findchannel(ic, chan, IEEE80211_MODE_AUTO);

				if (c != NULL) {
					param[0] = c->ic_maxregpower;
				} else {
					return -EINVAL;
				}
			}
		}
		break;
	case IEEE80211_PARAM_BA_MAX_WIN_SIZE:
		param[0] = vap->iv_max_ba_win_size;
		break;
	case IEEE80211_PARAM_MIN_DWELL_TIME_ACTIVE:
		param[0] = ic->ic_mindwell_active;
		break;
	case IEEE80211_PARAM_MIN_DWELL_TIME_PASSIVE:
		param[0] = ic->ic_mindwell_passive;
		break;
	case IEEE80211_PARAM_MAX_DWELL_TIME_ACTIVE:
		param[0] = ic->ic_maxdwell_active;
		break;
#ifdef QTN_BG_SCAN
	case IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_ACTIVE:
		param[0] = ic->ic_qtn_bgscan.dwell_msecs_active;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_PASSIVE:
		param[0] = ic->ic_qtn_bgscan.dwell_msecs_passive;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_ACTIVE:
		param[0] = ic->ic_qtn_bgscan.duration_msecs_active;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_FAST:
		param[0] = ic->ic_qtn_bgscan.duration_msecs_passive_fast;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_NORMAL:
		param[0] = ic->ic_qtn_bgscan.duration_msecs_passive_normal;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_SLOW:
		param[0] = ic->ic_qtn_bgscan.duration_msecs_passive_slow;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_FAST:
		param[0] = ic->ic_qtn_bgscan.thrshld_fat_passive_fast;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_NORMAL:
		param[0] = ic->ic_qtn_bgscan.thrshld_fat_passive_normal;
		break;
	case IEEE80211_PARAM_QTN_BGSCAN_DEBUG:
		param[0] = ic->ic_qtn_bgscan.debug_flags;
		break;
#endif /*QTN_BG_SCAN */
	case IEEE80211_PARAM_MAX_DWELL_TIME_PASSIVE:
		param[0] = ic->ic_maxdwell_passive;
		break;
#ifdef QSCS_ENABLED
	case IEEE80211_PARAM_SCS:
		param[0] = ic->ic_scs.scs_enable;
		break;
	case IEEE80211_PARAM_SCS_DFS_REENTRY_REQUEST:
		param[0] = ((struct ap_state *)(ic->ic_scan->ss_scs_priv))->as_dfs_reentry_level;
		break;
	case IEEE80211_PARAM_SCS_CCA_INTF:
		{
			int chan = (param[0] >> 16);
			struct ap_state *as = ic->ic_scan->ss_scs_priv;

			if (as && chan < IEEE80211_CHAN_MAX && chan > 0 && isset(ic->ic_chan_active, chan)) {
				if (as->as_cca_intf[chan] == SCS_CCA_INTF_INVALID)
					param[0] = -1;
				else
					param[0] = as->as_cca_intf[chan];
			} else {
				return -EINVAL;
			}
		}
		break;
#endif /* QSCS_ENABLED */
	case IEEE80211_PARAM_ALT_CHAN:
		param[0] = ic->ic_ieee_alt_chan;
		break;
	case IEEE80211_PARAM_LDPC_ALLOW_NON_QTN:
		param[0] = (vap->iv_ht_flags & IEEE80211_HTF_LDPC_ALLOW_NON_QTN) ? 1 : 0;
		break;
	case IEEE80211_PARAM_FWD_UNKNOWN_MC:
		param[0] = vap->iv_forward_unknown_mc;
		break;
	case IEEE80211_PARAM_MC_TO_UC:
		param[0] = vap->iv_mc_to_uc;
		break;
	case IEEE80211_PARAM_BCST_4:
		param[0] = vap->iv_reliable_bcst;
		break;
	case IEEE80211_PARAM_AP_FWD_LNCB:
		param[0] = vap->iv_ap_fwd_lncb;
		break;
	case IEEE80211_PARAM_GI_SELECT:
		param[0] = ic->ic_gi_select_enable;
		break;
	case IEEE80211_PARAM_PPPC_SELECT:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			return -EOPNOTSUPP;
		} else {
			param[0] = ic->ic_pppc_select_enable;
		}
		break;
	case IEEE80211_PARAM_PPPC_STEP:
		if ((ic->ic_flags & IEEE80211_F_DOTH) && (ic->ic_flags_ext & IEEE80211_FEXT_TPC)) {
			return -EOPNOTSUPP;
		} else {
			param[0] = ic->ic_pppc_step_db;
		}
		break;
	case IEEE80211_PARAM_EMI_POWER_SWITCHING:
		param[0] = ic->ic_emi_power_switch_enable;
		break;
	case IEEE80211_PARAM_GET_DFS_CCE:
		param[0] = (ic->ic_dfs_cce.cce_previous << IEEE80211_CCE_PREV_CHAN_SHIFT) |
			    ic->ic_dfs_cce.cce_current;
		break;
	case IEEE80211_PARAM_GET_SCS_CCE:
		param[0] = (ic->ic_aci_cci_cce.cce_previous << IEEE80211_CCE_PREV_CHAN_SHIFT) |
			    ic->ic_aci_cci_cce.cce_current;
		break;
	case IEEE80211_PARAM_ASSOC_LIMIT:
		param[0] = ic->ic_sta_assoc_limit;
		break;
	case IEEE80211_PARAM_HW_BONDING:
		param[0] = soc_shared_params->hardware_options;
		break;
	case IEEE80211_PARAM_BSS_ASSOC_LIMIT:
		{
			uint32_t group = (((uint32_t)param[0]) >> 16) & 0xffff;

			if (group < IEEE80211_MIN_BSS_GROUP
					|| group >= IEEE80211_MAX_BSS_GROUP) {
				return -EINVAL;
			}

			param[0] = ic->ic_ssid_grp[group].limit;
		}
		break;
	case IEEE80211_PARAM_BSS_GROUP_ID:
		param[0] = vap->iv_ssid_group;
		break;
	case IEEE80211_PARAM_BSS_ASSOC_RESERVE:
		{
			uint32_t group = (((uint32_t)param[0]) >> 16) & 0xffff;

			if (group < IEEE80211_MIN_BSS_GROUP
					|| group >= IEEE80211_MAX_BSS_GROUP) {
				return -EINVAL;
			}

			param[0] = ic->ic_ssid_grp[group].reserve;
		}
		break;
	case IEEE80211_PARAM_IOT_TWEAKS:
		param[0] = qtn_mproc_sync_shared_params_get()->iot_tweaks;
		break;
        case IEEE80211_PARAM_FAST_REASSOC:
                param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_SCAN_FAST_REASS);
                break;
	case IEEE80211_PARAM_CSA_FLAG:
		param[0] = ic->ic_csa_flag;
		break;
	case IEEE80211_PARAM_DEF_MATRIX:
		param[0] = ic->ic_def_matrix;
		break;
	case IEEE80211_PARAM_ENABLE_11AC:
		param[0] = vap->iv_11ac_enabled;
		break;
	case IEEE80211_PARAM_FIXED_11AC_TX_RATE:
		if ((vap->iv_mcs_config & IEEE80211_RATE_PREFIX_MASK) ==
				IEEE80211_AC_RATE_PREFIX) {
			param[0] = vap->iv_mcs_config & 0x0F;
		} else {
			printk("VHT rate is not set\n");
			return -EOPNOTSUPP;
		}
		break;
	case IEEE80211_PARAM_VAP_PRI:
		param[0] = vap->iv_pri;
		break;
	case IEEE80211_PARAM_AIRFAIR:
		param[0] = ic->ic_airfair;
		break;
	case IEEE80211_PARAM_TDLS_STATUS:
		if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_PROHIB) == IEEE80211_FEXT_TDLS_PROHIB)
			param[0] = 0;
		else
			param[0] = 1;
		break;
	case IEEE80211_PARAM_TDLS_MODE:
		param[0] = vap->tdls_path_sel_prohibited;
		break;
	case IEEE80211_PARAM_TDLS_TIMEOUT_TIME:
		param[0] = vap->tdls_timeout_time;
		break;
	case IEEE80211_PARAM_TDLS_PATH_SEL_WEIGHT:
		param[0] = vap->tdls_path_sel_weight;
		break;
	case IEEE80211_PARAM_TDLS_TRAINING_PKT_CNT:
		param[0] = vap->tdls_training_pkt_cnt;
		break;
	case IEEE80211_PARAM_TDLS_DISC_INT:
		param[0] = vap->tdls_discovery_interval;
		break;
	case IEEE80211_PARAM_TDLS_PATH_SEL_PPS_THRSHLD:
		param[0] = vap->tdls_path_sel_pps_thrshld;
		break;
	case IEEE80211_PARAM_TDLS_PATH_SEL_RATE_THRSHLD:
		param[0] = vap->tdls_path_sel_rate_thrshld;
		break;
	case IEEE80211_PARAM_TDLS_VERBOSE:
		param[0] = vap->tdls_verbose;
		break;
	case IEEE80211_PARAM_TDLS_MIN_RSSI:
		param[0] = vap->tdls_min_valid_rssi;
		break;
	case IEEE80211_PARAM_TDLS_SWITCH_INTS:
		param[0] = vap->tdls_switch_ints;
		break;
	case IEEE80211_PARAM_TDLS_RATE_WEIGHT:
		param[0] = vap->tdls_phy_rate_wgt;
		break;
	case IEEE80211_PARAM_TDLS_UAPSD_INDICAT_WND:
		param[0] = vap->tdls_uapsd_indicat_wnd;
		break;
	case IEEE80211_PARAM_TDLS_CS_MODE:
		if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PASSIVE)
			param[0] = 2;
		else if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_CS_PROHIB)
			param[0] = 1;
		else
			param[0] = 0;
		break;
	case IEEE80211_PARAM_TDLS_OFF_CHAN:
		param[0] = vap->tdls_fixed_off_chan;
		break;
	case IEEE80211_PARAM_TDLS_OFF_CHAN_BW:
		param[0] = vap->tdls_fixed_off_chan_bw;
		break;
	case IEEE80211_PARAM_TDLS_NODE_LIFE_CYCLE:
		param[0] = vap->tdls_node_life_cycle;
		break;
	case IEEE80211_PARAM_TDLS_OVER_QHOP_ENABLE:
		param[0] = vap->tdls_over_qhop_en;
		break;
	case IEEE80211_PARAM_OCAC:
	case IEEE80211_PARAM_SDFS:
		{
			int retval = ieee80211_param_ocac_get(vap, param);
			if (retval != 0) {
				return retval;
			}
		}
		break;
	case IEEE80211_PARAM_DEACTIVE_CHAN_PRI:
		param[0] = ieee80211_get_inactive_primary_chan_num(ic);
		break;
	case IEEE80211_PARAM_SPECIFIC_SCAN:
		param[0] = (vap->iv_flags_ext & IEEE80211_FEXT_SPECIFIC_SCAN) ? 1 : 0;
		break;
	case IEEE80211_PARAM_FIXED_SGI:
		param[0] = ic->ic_gi_fixed;
		break;
	case IEEE80211_PARAM_FIXED_BW:
		param[0] = ic->ic_bw_fixed;
		break;
	case IEEE80211_PARAM_SPEC_COUNTRY_CODE:
		{
			union {
				char as_chars[4];
				uint32_t as_u32;
			} region;

			if (ieee80211_countryid_to_country_string(ic->ic_spec_country_code,
					region.as_chars) != 0) {
				/*
				 * If we can't get a country string from the current code,
				 * return the NULL string as the region.
				 */
				region.as_u32 = 0;
			}

			param[0] = (int)region.as_u32;
		}
		break;
        case IEEE80211_PARAM_VCO_LOCK_DETECT_MODE:
        {
            param[0] = sp->vco_lock_detect_mode;
        }
        break;
	case IEEE80211_PARAM_CONFIG_PMF:
		param[0] = vap->iv_pmf;
		break;
	case IEEE80211_PARAM_RX_AMSDU_ENABLE:
		param[0] = vap->iv_rx_amsdu_enable;
		break;
	case IEEE80211_PARAM_RX_AMSDU_THRESHOLD_CCA:
		param[0] = vap->iv_rx_amsdu_threshold_cca;
		break;
	case IEEE80211_PARAM_RX_AMSDU_THRESHOLD_PMBL:
		param[0] = vap->iv_rx_amsdu_threshold_pmbl;
		break;
	case IEEE80211_PARAM_RX_AMSDU_PMBL_WF_SP:
		param[0] = vap->iv_rx_amsdu_pmbl_wf_sp;
		break;
	case IEEE80211_PARAM_RX_AMSDU_PMBL_WF_LP:
		param[0] = vap->iv_rx_amsdu_pmbl_wf_lp;
		break;
	case IEEE80211_PARAM_INTRA_BSS_ISOLATE:
		param[0] = !!(dev->qtn_flags & QTN_FLAG_INTRA_BSS_ISOLATE);
		break;
	case IEEE80211_PARAM_BSS_ISOLATE:
		param[0] = !!(dev->qtn_flags & QTN_FLAG_BSS_ISOLATE);
		break;
	case IEEE80211_PARAM_BF_RX_STS:
		param[0] = ic->ic_vhtcap.bfstscap + 1;
		break;
	case IEEE80211_PARAM_PC_OVERRIDE:
		param[0] = ((ic->ic_pwr_constraint)|(ic->ic_pco.pco_set<<8));
		break;
	case IEEE80211_PARAM_WOWLAN:
		param[0] = ieee80211_param_wowlan_get(ic);
		break;
	case IEEE80211_PARAM_SCAN_TBL_LEN_MAX:
		param[0] = ic->ic_scan_tbl_len_max;
		break;
	case IEEE80211_PARAM_WDS_MODE:
		param[0] = IEEE80211_VAP_WDS_IS_RBS(vap) ? 1 : IEEE80211_VAP_WDS_IS_MBS(vap) ? 0 : 2;
		ieee80211_extdr_dump_flags(vap);
		break;
	case IEEE80211_PARAM_EXTENDER_ROLE:
		param[0] = ic->ic_extender_role;
		break;
	case IEEE80211_PARAM_EXTENDER_MBS_BEST_RSSI:
		param[0] = ic->ic_extender_mbs_best_rssi;
		break;
	case IEEE80211_PARAM_EXTENDER_RBS_BEST_RSSI:
		param[0] = ic->ic_extender_rbs_best_rssi;
		break;
	case IEEE80211_PARAM_EXTENDER_MBS_WGT:
		param[0] = ic->ic_extender_mbs_wgt;
		break;
	case IEEE80211_PARAM_EXTENDER_RBS_WGT:
		param[0] = ic->ic_extender_rbs_wgt;
		break;
	case IEEE80211_PARAM_EXTENDER_VERBOSE:
		param[0] = ic->ic_extender_verbose;
		break;
	case IEEE80211_PARAM_VAP_TX_AMSDU:
		param[0] = vap->iv_tx_amsdu;
		break;
	case IEEE80211_PARAM_TX_MAXMPDU:
		param[0] = vap->iv_tx_max_amsdu;
		break;
	case IEEE80211_PARAM_DISASSOC_REASON:
		param[0] = vap->iv_disassoc_reason;
		break;
        case IEEE80211_PARAM_BB_PARAM:
                {
                        param[0] = sp->bb_param;
                }
                break;
	case IEEE80211_PARAM_NDPA_DUR:
		param[0] = ic->ic_ndpa_dur;
		break;
	case IEEE80211_PARAM_SU_TXBF_PKT_CNT:
		param[0] = ic->ic_su_txbf_pkt_cnt;
		break;
	case IEEE80211_PARAM_MU_TXBF_PKT_CNT:
		param[0] = ic->ic_mu_txbf_pkt_cnt;
		break;
	case IEEE80211_PARAM_SCAN_RESULTS_CHECK_INV:
		param[0] = ic->ic_scan_results_check;
		break;
	case IEEE80211_PARAM_TQEW_DESCR_LIMIT:
		param[0] = ic->ic_tqew_descr_limit;
		break;
	case IEEE80211_PARAM_CS_THRESHOLD:
		param[0] = sp->cs_thresh_base_val;
		break;
	case IEEE80211_PARAM_L2_EXT_FILTER:
		param[0] = g_l2_ext_filter;
		break;
	case IEEE80211_PARAM_L2_EXT_FILTER_PORT:
		param[0] = ic->ic_get_l2_ext_filter_port();
		break;
	case IEEE80211_PARAM_OBSS_TRIGG_SCAN_INT:
		if ((ic->ic_opmode == IEEE80211_M_STA) &&
				(vap->iv_bss) &&
				(IEEE80211_AID(vap->iv_bss->ni_associd)))
			param[0] = vap->iv_bss->ni_obss_ie.obss_trigger_interval;
		else
			param[0] = ic->ic_obss_ie.obss_trigger_interval;
		break;
	case IEEE80211_PARAM_PREF_BAND:
		param[0] = vap->iv_pref_band;
		break;
	case IEEE80211_PARAM_BW_2_4GHZ:
		if (IS_IEEE80211_24G_BAND(ic))
			param[0] = ieee80211_get_bw(ic);
		else
			return -EINVAL;
		break;
	case IEEE80211_PARAM_ALLOW_VHT_TKIP:
		param[0] = vap->allow_tkip_for_vht;
		break;
	case IEEE80211_PARAM_VHT_OPMODE_NOTIF:
		param[0] = ic->ic_vht_opmode_notif;
		break;
	case IEEE80211_PARAM_QTN_BLOCK_BSS:
		param[0] = !!(vap->is_block_all_assoc);
		break;
	case IEEE80211_PARAM_VHT_2_4GHZ:
		param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_24GVHT);
		break;
	case IEEE80211_PARAM_BEACONING_SCHEME:
		param[0] = ic->ic_beaconing_scheme;
		break;
#if defined(QBMPS_ENABLE)
	case IEEE80211_PARAM_STA_BMPS:
		param[0] = qv->qv_bmps_mode;
		break;
#endif
	case IEEE80211_PARAM_40MHZ_INTOLERANT:
		if (IEEE80211_IS_11B(ic) || IEEE80211_IS_11G(ic))
			param[0] = !!(vap->iv_coex & WLAN_20_40_BSS_COEX_40MHZ_INTOL);
		else if (IS_IEEE80211_11NG(ic))
			param[0] = !!(ic->ic_htcap.cap & IEEE80211_HTCAP_C_40_INTOLERANT);
		else
			param[0] = 0;
		break;
	case IEEE80211_PARAM_SET_RTS_BW_DYN:
		param[0] = ic->ic_rts_bw_dyn;
		break;
	case IEEE80211_PARAM_SET_CTS_BW:
		param[0] = ic->ic_cts_bw;
		break;
	case IEEE80211_PARAM_USE_NON_HT_DUPLICATE_MU:
		param[0] = ic->use_non_ht_duplicate_for_mu;
		break;
	case IEEE80211_PARAM_DISABLE_TX_BA:
		param[0] = vap->tx_ba_disable;
		break;
	case IEEE80211_PARAM_DECLINE_RX_BA:
		param[0] = vap->rx_ba_decline;
		break;
	case IEEE80211_PARAM_VAP_STATE:
		param[0] = vap->iv_vap_state;
		break;
	case IEEE80211_PARAM_OBSS_SCAN:
		param[0] = ic->ic_obss_scan_enable;
		break;
	case IEEE80211_PARAM_SHORT_SLOT:
		param[0] = !!(ic->ic_caps & IEEE80211_C_SHSLOT);
		break;
	case IEEE80211_PARAM_BG_PROTECT:
		param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_BG_PROTECT);
		break;
	case IEEE80211_PARAM_11N_PROTECT:
		param[0] = !!(ic->ic_flags_ext & IEEE80211_FEXT_11N_PROTECT);
		break;
	case IEEE80211_PARAM_MU_NDPA_BW_SIGNALING_SUPPORT:
		param[0] = ic->rx_bws_support_for_mu_ndpa;
		break;
	case IEEE80211_PARAM_WPA_STARTED:
	case IEEE80211_PARAM_HOSTAP_STARTED:
		param[0] = ic->hostap_wpa_state;
		break;
	case IEEE80211_PARAM_EP_STATUS:
		ieee80211_param_from_qdrv(vap, param[0], &param[0], NULL, 0);
		break;
	case IEEE80211_PARAM_RX_BAR_SYNC:
		param[0] = ic->ic_rx_bar_sync;
		break;
	case IEEE80211_PARAM_GET_REG_DOMAIN_IS_EU:
		param[0] = ic->ic_dfs_is_eu_region();
		break;
	case IEEE80211_PARAM_GET_CHAN_AVAILABILITY_STATUS:
		param[0] = 0; //No param
		ic->ic_dump_available_channels(ic);
		break;
	case IEEE80211_PARAM_NEIGHBORHOOD_THRSHD:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;

		param[0] = ieee80211_get_threshold_of_neighborhood_type(ic, (((uint32_t)param[0]) >> 16) & 0xFFFF);
		break;
	case IEEE80211_PARAM_NEIGHBORHOOD_TYPE:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP && vap->iv_opmode != IEEE80211_M_STA)
			return -EINVAL;

		param[0] = ieee80211_get_type_of_neighborhood(ic);
		break;
	case IEEE80211_PARAM_NEIGHBORHOOD_COUNT:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP && vap->iv_opmode != IEEE80211_M_STA)
			return -EINVAL;

		param[0] = ic->ic_neighbor_count;
		break;
	case IEEE80211_PARAM_STA_DFS_STRICT_MODE:
		if ((vap->iv_opmode != IEEE80211_M_STA)	|| (!(ic->ic_dfs_is_eu_region()))) {
			return -EINVAL;
		}
		param[0] = ic->sta_dfs_info.sta_dfs_strict_mode;
		break;
	case IEEE80211_PARAM_STA_DFS_STRICT_MEASUREMENT_IN_CAC:
		if ((vap->iv_opmode != IEEE80211_M_STA)	|| (!(ic->ic_dfs_is_eu_region()))) {
			return -EINVAL;
		}
		param[0] = ic->sta_dfs_info.sta_dfs_strict_msr_cac;
		break;
	case IEEE80211_PARAM_STA_DFS_STRICT_TX_CHAN_CLOSE_TIME:
		if ((vap->iv_opmode != IEEE80211_M_STA)	|| (!(ic->ic_dfs_is_eu_region()))) {
			return -EINVAL;
		}
		param[0] = ic->sta_dfs_info.sta_dfs_tx_chan_close_time;
		break;
	case IEEE80211_PARAM_RADAR_NONOCCUPY_PERIOD:
		param[0] = (ic->ic_non_occupancy_period / HZ);
		break;
	case IEEE80211_PARAM_DFS_CSA_CNT:
		param[0] = ic->ic_dfs_csa_cnt;
		break;
	case IEEE80211_PARAM_IS_WEATHER_CHANNEL:
		{
			struct ieee80211_channel *chan = ieee80211_find_channel_by_ieee(ic,
						(((uint32_t)param[0]) >> 16) & 0xFFFF);
			if (chan)
				param[0] = ieee80211_is_on_weather_channel(ic, chan);
			else
				return -EINVAL;
			break;
		}
	case IEEE80211_PARAM_VAP_DBG:
		param[0] = vap->iv_debug;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int
ieee80211_ioctl_getblockdata(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{

	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct iw_point *iwp = (struct iw_point *)w;
	int subcmd = iwp->flags;

	(void) ic;

	switch (subcmd) {
	case IEEE80211_PARAM_ASSOC_HISTORY:
		{
			struct ieee80211_assoc_history	*ah = &ic->ic_assoc_history;

			iwp->length = sizeof(*ah);
			memcpy(extra, ah, iwp->length);
		}
		break;
	case IEEE80211_PARAM_CSW_RECORD:
		{
			struct ieee80211req_csw_record * record = &ic->ic_csw_record;

			iwp->length = sizeof(struct ieee80211req_csw_record);
			memcpy(extra, record, iwp->length);
		}
		break;
	case IEEE80211_PARAM_PWR_SAVE:
		{
			iwp->length = sizeof(ic->ic_pm_state);
			memcpy(extra, ic->ic_pm_state, sizeof(ic->ic_pm_state));
		}
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

/* returns non-zero if ID is for a system IE (not for app use) */
static int
is_sys_ie(u_int8_t ie_id)
{
	/* XXX review this list */
	switch (ie_id) {
	case IEEE80211_ELEMID_SSID:
	case IEEE80211_ELEMID_RATES:
	case IEEE80211_ELEMID_FHPARMS:
	case IEEE80211_ELEMID_DSPARMS:
	case IEEE80211_ELEMID_CFPARMS:
	case IEEE80211_ELEMID_TIM:
	case IEEE80211_ELEMID_IBSSPARMS:
	case IEEE80211_ELEMID_COUNTRY:
	case IEEE80211_ELEMID_REQINFO:
	case IEEE80211_ELEMID_CHALLENGE:
	case IEEE80211_ELEMID_PWRCNSTR:
	case IEEE80211_ELEMID_PWRCAP:
	case IEEE80211_ELEMID_TPCREQ:
	case IEEE80211_ELEMID_TPCREP:
	case IEEE80211_ELEMID_SUPPCHAN:
	case IEEE80211_ELEMID_CHANSWITCHANN:
	case IEEE80211_ELEMID_MEASREQ:
	case IEEE80211_ELEMID_MEASREP:
	case IEEE80211_ELEMID_QUIET:
	case IEEE80211_ELEMID_IBSSDFS:
	case IEEE80211_ELEMID_ERP:
	case IEEE80211_ELEMID_RSN:
	case IEEE80211_ELEMID_XRATES:
	case IEEE80211_ELEMID_TPC:
	case IEEE80211_ELEMID_CCKM:
		return 1;
	default:
		return 0;
	}
}

/* returns non-zero if the buffer appears to contain a valid IE list */
static int
is_valid_ie_list(u_int32_t buf_len, void *buf, int exclude_sys_ies)
{
	struct ieee80211_ie *ie = (struct ieee80211_ie *)buf;

	while (buf_len >= sizeof(*ie)) {
		int ie_elem_len = sizeof(*ie) + ie->len;
		if (buf_len < ie_elem_len)
			break;
		if (exclude_sys_ies && is_sys_ie(ie->id))
			break;
		buf_len -= ie_elem_len;
		ie = (struct ieee80211_ie *)(ie->info + ie->len);
	}

	return (buf_len == 0) ? 1 : 0;
}

static int
ieee80211_ioctl_setoptie(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *wri, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	void *ie;

	/*
	 * NB: Doing this for ap operation could be useful (e.g. for
	 *     WPA and/or WME) except that it typically is worthless
	 *     without being able to intervene when processing
	 *     association response frames--so disallow it for now.
	 */
	if (vap->iv_opmode != IEEE80211_M_STA)
		return -EINVAL;
	if (! is_valid_ie_list(wri->length, extra, 0))
		return -EINVAL;
	/* NB: wri->length is validated by the wireless extensions code */
	MALLOC(ie, void *, wri->length, M_DEVBUF, M_WAITOK);
	if (ie == NULL)
		return -ENOMEM;
	memcpy(ie, extra, wri->length);
	if (vap->iv_opt_ie != NULL)
		FREE(vap->iv_opt_ie, M_DEVBUF);
	vap->iv_opt_ie = ie;
	vap->iv_opt_ie_len = wri->length;

	ieee80211_parse_cipher_key(vap, vap->iv_opt_ie, vap->iv_opt_ie_len);

	return 0;
}

static int
ieee80211_ioctl_getoptie(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *wri, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (vap->iv_opt_ie == NULL) {
		wri->length = 0;
		return 0;
	}
	wri->length = vap->iv_opt_ie_len;
	memcpy(extra, vap->iv_opt_ie, vap->iv_opt_ie_len);
	return 0;
}

/* the following functions are used by the set/get appiebuf functions */
static int
add_app_ie(unsigned int frame_type_index, struct ieee80211vap *vap,
	struct ieee80211req_getset_appiebuf *iebuf)
{
	struct ieee80211_ie *ie;

	if (! is_valid_ie_list(iebuf->app_buflen, iebuf->app_buf, 1))
		return -EINVAL;
	/* NB: data.length is validated by the wireless extensions code */
	MALLOC(ie, struct ieee80211_ie *, iebuf->app_buflen, M_DEVBUF, M_WAITOK);
	if (ie == NULL)
		return -ENOMEM;

	memcpy(ie, iebuf->app_buf, iebuf->app_buflen);
	if (vap->app_ie[frame_type_index].ie != NULL)
		FREE(vap->app_ie[frame_type_index].ie, M_DEVBUF);
	vap->app_ie[frame_type_index].ie = ie;
	vap->app_ie[frame_type_index].length = iebuf->app_buflen;

	return 0;
}

static int
remove_app_ie(unsigned int frame_type_index, struct ieee80211vap *vap)
{
	struct ieee80211_app_ie_t *app_ie = &vap->app_ie[frame_type_index];
	if (app_ie->ie != NULL) {
		FREE(app_ie->ie, M_DEVBUF);
		app_ie->ie = NULL;
		app_ie->length = 0;
	}
	return 0;
}

static int
get_app_ie(unsigned int frame_type_index, struct ieee80211vap *vap,
	struct ieee80211req_getset_appiebuf *iebuf)
{
	struct ieee80211_app_ie_t *app_ie = &vap->app_ie[frame_type_index];
	if (iebuf->app_buflen < app_ie->length)
		return -EINVAL;

	iebuf->app_buflen = app_ie->length;
	memcpy(iebuf->app_buf, app_ie->ie, app_ie->length);
	return 0;
}

static int
ieee80211_ioctl_setappiebuf(struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211req_getset_appiebuf *iebuf =
		(struct ieee80211req_getset_appiebuf *)extra;
	struct ieee80211_ie *ie;
	enum ieee80211_opmode chk_opmode;
	int iebuf_len;
	int rc = 0;

	iebuf_len = data->length - sizeof(struct ieee80211req_getset_appiebuf);
	if ( iebuf_len < 0 || iebuf_len != iebuf->app_buflen ||
		 iebuf->app_buflen > IEEE80211_APPIE_MAX )
		return -EINVAL;

	switch (iebuf->app_frmtype) {
	case IEEE80211_APPIE_FRAME_BEACON:
	case IEEE80211_APPIE_FRAME_PROBE_RESP:
	case IEEE80211_APPIE_FRAME_ASSOC_RESP:
		chk_opmode = IEEE80211_M_HOSTAP;
		break;
	case IEEE80211_APPIE_FRAME_PROBE_REQ:
	case IEEE80211_APPIE_FRAME_ASSOC_REQ:
	case IEEE80211_APPIE_FRAME_TDLS_ACT:
		chk_opmode = IEEE80211_M_STA;
		break;
	default:
		return -EINVAL;
	}
	if (vap->iv_opmode != chk_opmode)
		return -EINVAL;

	if (iebuf->app_frmtype == IEEE80211_APPIE_FRAME_TDLS_ACT) {
		rc = ieee80211_tdls_send_action_frame(dev,
			(struct ieee80211_tdls_action_data *)iebuf->app_buf);
		return rc;
	}

	if (iebuf->app_buflen) {

		if ((iebuf->app_frmtype == IEEE80211_APPIE_FRAME_ASSOC_REQ ||
				iebuf->app_frmtype == IEEE80211_APPIE_FRAME_ASSOC_RESP) &&
				iebuf->flags == F_QTN_IEEE80211_PAIRING_IE) {

			MALLOC(ie, struct ieee80211_ie *, iebuf->app_buflen, M_DEVBUF, M_WAITOK);

			if (ie == NULL)
				return -ENOMEM;

			memcpy(ie, iebuf->app_buf, iebuf->app_buflen);

			if (vap->qtn_pairing_ie.ie != NULL)
				FREE(vap->qtn_pairing_ie.ie, M_DEVBUF);
			vap->qtn_pairing_ie.ie = ie;
			vap->qtn_pairing_ie.length = iebuf->app_buflen;

			return 0;

		}

		rc = add_app_ie(iebuf->app_frmtype, vap, iebuf);
	} else {
		if ((iebuf->app_frmtype == IEEE80211_APPIE_FRAME_ASSOC_REQ ||
				iebuf->app_frmtype == IEEE80211_APPIE_FRAME_ASSOC_RESP) &&
				iebuf->flags == F_QTN_IEEE80211_PAIRING_IE) {
			if (vap->qtn_pairing_ie.ie != NULL) {
				FREE(vap->qtn_pairing_ie.ie, M_DEVBUF);
				vap->qtn_pairing_ie.ie = NULL;
				vap->qtn_pairing_ie.length = 0;
			}

			return 0;
		}

		rc = remove_app_ie(iebuf->app_frmtype, vap);
	}
	if ((iebuf->app_frmtype == IEEE80211_APPIE_FRAME_BEACON) && (rc == 0)) {
		struct ieee80211com *ic = vap->iv_ic;

		vap->iv_flags_ext |= IEEE80211_FEXT_APPIE_UPDATE;
		if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
			(vap->iv_state == IEEE80211_S_RUN)) {

			ic->ic_beacon_update(vap);
		}
	}

	return rc;
}

static int
ieee80211_ioctl_startcca(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *wri, char *extra)

{
	struct qtn_cca_args *ccaval = (struct qtn_cca_args *)extra;
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan = NULL;
	uint64_t tsf = 0;

	if (copy_from_user(ccaval, wri->pointer, sizeof(struct qtn_cca_args))) {
		return -EINVAL;
	}

	chan = findchannel(ic, ccaval->cca_channel, ic->ic_des_mode);
	if (chan == NULL) {
		printk(KERN_ERR "Invalid channel %d ? \n", ccaval->cca_channel);
		return -EINVAL;
	}

	ic->ic_get_tsf(&tsf);
	tsf = tsf + IEEE80211_SEC_TO_USEC(1); /* after 1 second */

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_ROAM,
		"CCA channel change scheduled at tsf %016llX \n", tsf);

	ic->ic_cca_start_tsf = tsf;
	ic->ic_cca_duration_tu = IEEE80211_MS_TO_TU(ccaval->duration);
	ic->ic_cca_chan = chan->ic_ieee;

	ic->ic_flags |= IEEE80211_F_CCA;
	ic->ic_cca_token++;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;

		if ((vap->iv_state != IEEE80211_S_RUN) && (vap->iv_state != IEEE80211_S_SCAN))
			continue;
		ic->ic_beacon_update(vap);
	}

	ic->ic_set_start_cca_measurement(ic, chan, tsf, ccaval->duration);
	wri->length = sizeof(struct qtn_cca_args);

	return 0;
}

static int
ieee80211_ioctl_getappiebuf(struct net_device *dev, struct iw_request_info *info,
	struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211req_getset_appiebuf *iebuf =
		(struct ieee80211req_getset_appiebuf *)extra;
	int max_iebuf_len;
	int rc = 0;

	max_iebuf_len = data->length - sizeof(struct ieee80211req_getset_appiebuf);
	if (max_iebuf_len < 0)
		return -EINVAL;
	if (copy_from_user(iebuf, data->pointer, sizeof(struct ieee80211req_getset_appiebuf)))
		return -EFAULT;
	if (iebuf->app_buflen > max_iebuf_len)
		iebuf->app_buflen = max_iebuf_len;

	switch (iebuf->app_frmtype) {
	case IEEE80211_APPIE_FRAME_BEACON:
	case IEEE80211_APPIE_FRAME_PROBE_RESP:
	case IEEE80211_APPIE_FRAME_ASSOC_RESP:
		if (vap->iv_opmode == IEEE80211_M_STA)
			return -EINVAL;
		break;
	case IEEE80211_APPIE_FRAME_PROBE_REQ:
	case IEEE80211_APPIE_FRAME_ASSOC_REQ:
		if (vap->iv_opmode != IEEE80211_M_STA)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	rc = get_app_ie(iebuf->app_frmtype, vap, iebuf);

	data->length = sizeof(struct ieee80211req_getset_appiebuf) + iebuf->app_buflen;

	return rc;
}


static void
wpa_hexdump_key(struct ieee80211vap *vap, int level,
	const char *title, const u8 *buf, size_t len)
{
#ifdef IEEE80211_DEBUG
	int show = level;
	size_t i;

	if (!ieee80211_msg_debug(vap)) {
		return;
	}

	printk("\n%s - hexdump(len=%lu):", title, (unsigned long) len);
	if (buf == NULL) {
		printk(" [NULL]");
	} else if (show) {
        	for (i = 0; i < len; i++)
			printk("%s%02x", i%4==0? "\n":" ", buf[i]);
	} else {
        	printk(" [REMOVED]");
	}
	printk("\n");
#endif
}

static int
ieee80211_ioctl_setfilter(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211req_set_filter *app_filter = (struct ieee80211req_set_filter *)extra;

	if ((extra == NULL) || (app_filter->app_filterype & ~IEEE80211_FILTER_TYPE_ALL))
		return -EINVAL;

	vap->app_filter = app_filter->app_filterype;

	return 0;
}

static int
ieee80211_ioctl_setkey(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211req_key *ik = (struct ieee80211req_key *)extra;
	struct ieee80211_node *ni;
	struct ieee80211_key *wk;
	u_int8_t kid = ik->ik_keyix;
	int i;
	int error;

	if ((ik->ik_keylen > sizeof(ik->ik_keydata)) ||
			(ik->ik_keylen > sizeof(wk->wk_key))) {
		return -E2BIG;
	}

	if(ik->ik_type == IEEE80211_CIPHER_AES_CMAC) {
		// 802.11w CMAC / IGTK ignore for now.
		return 0;
	}

	if (kid == IEEE80211_KEYIX_NONE) {
		/* Unicast key */
		kid = 0;
	} else if (kid >= IEEE80211_WEP_NKID) {
		return -EINVAL;
	}

	/* Group keys */
	if (((ik->ik_flags & IEEE80211_KEY_XMIT) == 0) ||
			(ik->ik_flags & IEEE80211_KEY_GROUP) ||
			IEEE80211_IS_MULTICAST(ik->ik_macaddr)) {

		ik->ik_flags |= IEEE80211_KEY_GROUP;
		if (vap->iv_opmode == IEEE80211_M_STA) {
			memset(ik->ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		} else if(ik->ik_flags & IEEE80211_KEY_VLANGROUP) {
			qtn_vlan_gen_group_addr(ik->ik_macaddr, ik->ik_vlan, vap->iv_dev->dev_id);
		} else {
			IEEE80211_ADDR_COPY(ik->ik_macaddr, vap->iv_bss->ni_macaddr);
		}
	}

	/* wk must be set to ni->ni_ucastkey for sw crypto */
	wk = &vap->iv_nw_keys[kid];
	wk->wk_ciphertype = ik->ik_type;
	wk->wk_keylen = ik->ik_keylen;
	wk->wk_flags = ik->ik_flags;
	wk->wk_keyix = kid;
	for (i = 0; i < WME_NUM_TID; i++) {
		wk->wk_keyrsc[i] = ik->ik_keyrsc;
	}
	wk->wk_keytsc = 0;
	memset(wk->wk_key, 0, sizeof(wk->wk_key));
	memcpy(wk->wk_key, ik->ik_keydata, ik->ik_keylen);

	if (!(ik->ik_flags & IEEE80211_KEY_GROUP)) {
		ni = ieee80211_find_node(&vap->iv_ic->ic_sta, ik->ik_macaddr);
		if (ni) {
			memcpy(&ni->ni_ucastkey, wk, sizeof(ni->ni_ucastkey));
			ieee80211_free_node(ni);
		}
		if (vap->iv_opmode == IEEE80211_M_WDS)
			memcpy(&vap->iv_wds_peer_key, wk, sizeof(vap->iv_wds_peer_key));
	}

	wpa_hexdump_key(vap, 0,
			(ik->ik_flags & IEEE80211_KEY_GROUP) ? "GTK" : "PTK",
			ik->ik_keydata, ik->ik_keylen);

	ieee80211_key_update_begin(vap);
	error = vap->iv_key_set(vap, wk, ik->ik_macaddr);
	ieee80211_key_update_end(vap);

	return error;
}

static int
ieee80211_ioctl_getkey(struct net_device *dev, struct iwreq *iwr)
{
#ifndef IEEE80211_UNUSED_CRYPTO_COMMANDS
	return -EOPNOTSUPP;
#else
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	struct ieee80211req_key ik;
	struct ieee80211_key *wk;
	const struct ieee80211_cipher *cip;
	u_int8_t kid;

	if (iwr->u.data.length != sizeof(ik))
		return -EINVAL;
	if (copy_from_user(&ik, iwr->u.data.pointer, sizeof(ik)))
		return -EFAULT;
	kid = ik.ik_keyix;
	if (kid == IEEE80211_KEYIX_NONE) {
		ni = ieee80211_find_node(&ic->ic_sta, ik.ik_macaddr);
		if (ni == NULL)
			return -EINVAL;
		wk = &ni->ni_ucastkey;
	} else {
		if (kid >= IEEE80211_WEP_NKID)
			return -EINVAL;
		wk = &vap->iv_nw_keys[kid];
		IEEE80211_ADDR_COPY(&ik.ik_macaddr, vap->iv_bss->ni_macaddr);
		ni = NULL;
	}
	cip = wk->wk_cipher;
	ik.ik_type = cip->ic_cipher;
	ik.ik_keylen = wk->wk_keylen;
	ik.ik_flags = wk->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV);
	if (wk->wk_keyix == vap->iv_def_txkey)
		ik.ik_flags |= IEEE80211_KEY_DEFAULT;
	if (capable(CAP_NET_ADMIN)) {
		/* NB: only root can read key data */
		ik.ik_keyrsc = wk->wk_keyrsc[0];
		ik.ik_keytsc = wk->wk_keytsc;
		memcpy(ik.ik_keydata, wk->wk_key, wk->wk_keylen);
		if (cip->ic_cipher == IEEE80211_CIPHER_TKIP) {
			memcpy(ik.ik_keydata+wk->wk_keylen,
				wk->wk_key + IEEE80211_KEYBUF_SIZE,
				IEEE80211_MICBUF_SIZE);
			ik.ik_keylen += IEEE80211_MICBUF_SIZE;
		}
	} else {
		ik.ik_keyrsc = 0;
		ik.ik_keytsc = 0;
		memset(ik.ik_keydata, 0, sizeof(ik.ik_keydata));
	}
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (copy_to_user(iwr->u.data.pointer, &ik, sizeof(ik)) ? -EFAULT : 0);
#endif /* IEEE80211_UNUSED_CRYPTO_COMMANDS */
}

static int
ieee80211_ioctl_delkey(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211req_del_key *dk = (struct ieee80211req_del_key *)extra;
	struct ieee80211_node *ni;
	struct ieee80211_key *wk;
	uint8_t kid = dk->idk_keyix;
	uint32_t is_group_key = 0;
	int error;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WPA | IEEE80211_MSG_DEBUG,
		"[%s] deleting key wepkid=%d bcast=%u\n",
		ether_sprintf(dk->idk_macaddr),
		dk->idk_keyix,
		IEEE80211_IS_MULTICAST(dk->idk_macaddr));

	if (kid == IEEE80211_KEYIX_NONE) {
		/* Unicast key */
		kid = 0;
	} else if (kid >= IEEE80211_WEP_NKID) {
		return -EINVAL;
	}

	/*
	 * hostapd sends a delete request for each of the four WEP global keys
	 * during initialisation.  WEP is not supported and each vap node entry has its
	 * own global key, so the same key will be deleted four times.
	 */
	if (IEEE80211_IS_MULTICAST(dk->idk_macaddr)) {
		is_group_key = 1;
		IEEE80211_ADDR_COPY(dk->idk_macaddr, vap->iv_bss->ni_macaddr);
	}

	/* wk must be set to ni->ni_ucastkey for sw crypto */
	wk = &vap->iv_nw_keys[kid];
	wk->wk_ciphertype = 0;
	wk->wk_keytsc = 0;
	wk->wk_keylen = sizeof(wk->wk_key);
	memset(wk->wk_key, 0, sizeof(wk->wk_key));

	if (!is_group_key) {
		ni = ieee80211_find_node(&vap->iv_ic->ic_sta, dk->idk_macaddr);
		if (ni) {
			memcpy(&ni->ni_ucastkey, wk, sizeof(ni->ni_ucastkey));
			ieee80211_free_node(ni);
		}
		if (vap->iv_opmode == IEEE80211_M_WDS) {
			vap->iv_wds_peer_key.wk_ciphertype = 0;
			vap->iv_wds_peer_key.wk_keytsc = 0;
			vap->iv_wds_peer_key.wk_keylen = 0;
			ieee80211_crypto_resetkey(vap, &vap->iv_wds_peer_key,
				IEEE80211_KEYIX_NONE);
		}
	}

	ieee80211_key_update_begin(vap);
	error = vap->iv_key_delete(vap, wk, dk->idk_macaddr);
	ieee80211_key_update_end(vap);

	return error;
}

struct scanlookup {		/* XXX: right place for declaration? */
	const u_int8_t *mac;
	int esslen;
	const char *essid;
	const struct ieee80211_scan_entry *se;
};

/*
 * Match mac address and any ssid.
 */
static int
mlmelookup(void *arg, const struct ieee80211_scan_entry *se)
{
	struct scanlookup *look = arg;

	if (!IEEE80211_ADDR_EQ(look->mac, se->se_macaddr))
		return 0;
	if (look->esslen != 0) {
		if (se->se_ssid[1] != look->esslen)
			return 0;
		if (memcmp(look->essid, se->se_ssid + 2, look->esslen))
			return 0;
	}
	look->se = se;

	return 0;
}

/*
 * Set operational Bridge Mode for:
 * - an AP when the config is changed
 * - a station when the config is changed or when associating with a new AP
 * In Bridge Mode, eligible frames (non-1X, unicast data frames) are
 * transmitted using the 4-address header format.
 */
u_int8_t ieee80211_bridgemode_set(struct ieee80211vap *vap, u_int8_t config_change)
{
	u_int8_t op_bridgemode;

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		op_bridgemode = !(vap->iv_qtn_flags & IEEE80211_QTN_BRIDGEMODE_DISABLED);
	} else {
		op_bridgemode = !(vap->iv_qtn_flags & IEEE80211_QTN_BRIDGEMODE_DISABLED) &&
						(vap->iv_qtn_ap_cap & IEEE80211_QTN_BRIDGEMODE);
	}

	/* Has bridge mode changed? */
	if ((op_bridgemode && !(vap->iv_flags_ext & IEEE80211_FEXT_WDS)) ||
		(!op_bridgemode && (vap->iv_flags_ext & IEEE80211_FEXT_WDS))) {

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
			"%s: %s Bridge Mode\n", __func__, op_bridgemode ? "Enabling" : "Disabling");

		if (op_bridgemode) {
			vap->iv_flags_ext |= IEEE80211_FEXT_WDS;
		} else {
			vap->iv_flags_ext &= ~IEEE80211_FEXT_WDS;
		}

		/* Notify the MuC */
		ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_WDS, op_bridgemode, NULL, 0);

		/*
		 * If the change was caused by a configuration change, force
		 * reassociation to ensure that everyone is in sync.
		 */
		if (config_change) {
			ieee80211_wireless_reassoc(vap, 0, 0);
		}
		return 1;
	}

	return 0;
}

void ieee80211_sta_fast_rejoin(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct scanlookup lookup;

	if (vap->iv_state >= IEEE80211_S_AUTH) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "state(%d) not expected\n",
				vap->iv_state);
		return;
	}

	lookup.se = NULL;
	lookup.mac = vap->iv_sta_fast_rejoin_bssid;
	if (vap->iv_des_nssid != 0) {
		lookup.esslen = vap->iv_des_ssid[0].len;
		lookup.essid = (char *)vap->iv_des_ssid[0].ssid;
	} else {
		lookup.esslen = 0;
		lookup.essid = (char *)"";
	}
	ieee80211_scan_iterate(ic, mlmelookup, &lookup);
	if (lookup.se != NULL) {
		vap->iv_nsdone = 0;
		vap->iv_nsparams.result = 0;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "fast rejoin bssid "MACSTR"\n",
				MAC2STR(vap->iv_sta_fast_rejoin_bssid));
		if (!ieee80211_sta_join(vap, lookup.se)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "fast rejoin bssid "MACSTR" failed\n",
					MAC2STR(vap->iv_sta_fast_rejoin_bssid));
		}
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "fast rejoin bssid "MACSTR" not found\n",
				MAC2STR(vap->iv_sta_fast_rejoin_bssid));
	}
}

void ieee80211_ba_setup_detect_rssi(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *) arg;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	int32_t rssi;

	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni && (ni->ni_qtn_flags & QTN_IS_INTEL_NODE)
				&& !IEEE80211_NODE_IS_VHT(ni)) {
			rssi = ni->ni_shared_stats->rx[STATS_SU].last_rssi_dbm[NUM_ANT];
			if (ni->rssi_avg_dbm) {
				ni->rssi_avg_dbm = (ni->rssi_avg_dbm *
							(QTN_RSSI_SAMPLE_TH - 1) + rssi)
									/ QTN_RSSI_SAMPLE_TH;
			} else {
				ni->rssi_avg_dbm = rssi;
			}
		}
	}

	mod_timer(&ic->ic_ba_setup_detect, jiffies + HZ * QTN_AMPDU_DETECT_PERIOD);
}

static int ieee80211_ba_setup_detect_set(struct ieee80211vap *vap, int enable)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (enable) {
		init_timer(&ic->ic_ba_setup_detect);
		ic->ic_ba_setup_detect.function = ieee80211_ba_setup_detect_rssi;
		ic->ic_ba_setup_detect.data = (unsigned long) ic;
		ic->ic_ba_setup_detect.expires = jiffies;
		add_timer(&ic->ic_ba_setup_detect);
	} else {
		del_timer(&ic->ic_ba_setup_detect);
	}

	return 0;
}

static int
ieee80211_ioctl_setmlme(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211req_mlme *mlme = (struct ieee80211req_mlme *)extra;
	struct ieee80211_node *ni;

	if (!IS_UP(dev))
		return -EINVAL;

	if (ic->ic_flags_qtn & IEEE80211_QTN_MONITOR)
		return 0;

	switch (mlme->im_op) {
	case IEEE80211_MLME_ASSOC:
		if (vap->iv_opmode == IEEE80211_M_STA) {
			struct scanlookup lookup;

			lookup.se = NULL;
			lookup.mac = mlme->im_macaddr;
			/* XXX use revised api w/ explicit ssid */
			if (vap->iv_des_nssid != 0) {
				lookup.esslen = vap->iv_des_ssid[0].len;
				lookup.essid = (char *)vap->iv_des_ssid[0].ssid;
			} else {
				lookup.esslen = 0;
				lookup.essid = (char *)"";
			}
			ieee80211_scan_iterate(ic, mlmelookup, &lookup);
			if (lookup.se != NULL) {
				vap->iv_nsdone = 0;
				vap->iv_nsparams.result = 0;
				if (ieee80211_sta_join(vap, lookup.se))
					while (!vap->iv_nsdone)
						IEEE80211_RESCHEDULE();
				if (vap->iv_nsparams.result == 0)
					return 0;
			}
		}
		return -EINVAL;
	case IEEE80211_MLME_DEBUG_CLEAR:
	case IEEE80211_MLME_DISASSOC:
	case IEEE80211_MLME_DEAUTH:
		switch (vap->iv_opmode) {
		case IEEE80211_M_STA:
			/* XXX not quite right */
			ieee80211_new_state(vap, IEEE80211_S_INIT,
				mlme->im_reason);
			break;
		case IEEE80211_M_HOSTAP:
			/* NB: the broadcast address means do 'em all */
			IEEE80211_NODE_LOCK_BH(&ic->ic_sta);
			if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, vap->iv_dev->broadcast)) {
				ni = ieee80211_find_node(&ic->ic_sta,
					mlme->im_macaddr);
				if (ni == NULL) {
					IEEE80211_NODE_UNLOCK_BH(&ic->ic_sta);
					return -EINVAL;
				}
				if (dev == ni->ni_vap->iv_dev) {
					ieee80211_domlme(mlme, ni);
				}
				ieee80211_free_node(ni);
			} else {
				ieee80211_iterate_dev_nodes(dev, &ic->ic_sta, ieee80211_domlme, mlme, 0);
			}
			IEEE80211_NODE_UNLOCK_BH(&ic->ic_sta);
			break;
		default:
			return -EINVAL;
		}
		break;
	case IEEE80211_MLME_AUTHORIZE:
	case IEEE80211_MLME_UNAUTHORIZE:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;
		ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr);
		if (ni == NULL)
			return -ENOENT;
		if (mlme->im_op == IEEE80211_MLME_AUTHORIZE)
			ieee80211_node_authorize(ni);
		else
			ieee80211_node_unauthorize(ni);
		ieee80211_free_node(ni);
		break;
	case IEEE80211_MLME_CLEAR_STATS:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;
		ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr);
		if (ni == NULL)
			return -ENOENT;

		/* clear statistics */
		memset(&ni->ni_stats, 0, sizeof(struct ieee80211_nodestats));
		ieee80211_free_node(ni);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* 'iwpriv wifi0 doth_radar X' simulates a radar detection on current channel
 * triggers a channel switch to a random if X is 0 or to the IEEE channel X */
static int
ieee80211_ioctl_radar(struct net_device *dev, struct iw_request_info *info,
        void *w, char *extra)
{
	int *params = (int *) extra;
	struct ieee80211vap *vap = netdev_priv(dev);
        struct ieee80211com *ic = vap->iv_ic;
	u_int8_t new_ieee = params[0];

	if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_DFS) {
		ic->ic_radar_detected(ic, new_ieee);
		return 0;
	} else {
		return -EINVAL;
	}
}

/* 'iwpriv wifi0 dfsactscan 1' will have STA do active scan on DFS channels,
 * and 'iwpriv wifi0 dfsactscan 0' revert it to the default
 * behavior (passive scan on DFS channels) */
static int
ieee80211_ioctl_dfsactscan(struct net_device *dev, struct iw_request_info *info,
        void *w, char *extra)
{
        int* params = (int *) extra;
        struct ieee80211vap *vap = netdev_priv(dev);
        struct ieee80211com *ic = vap->iv_ic;
        u_int8_t dfsactscan = params[0];
	int i;

	if (ic->ic_opmode != IEEE80211_M_STA)
		printk("%s: this command can be used only for STA\n", __FUNCTION__);

/* Note: this logic should be same with qdrv_radar_is_dfs_required(), but wlan-to-qdrv
 * dependency is considered not desirable, so the logic is duplicated here
 */
#define IS_DFS_CHAN(chan) ((5250 <= (chan)->ic_freq) && ((chan)->ic_freq <= 5725))

	for (i = 0; i < ic->ic_nchans; i++) {
		if (IS_DFS_CHAN(&ic->ic_channels[i])) {
			if (dfsactscan) {
				ic->ic_channels[i].ic_flags &= ~IEEE80211_CHAN_PASSIVE;
			} else {
				ic->ic_channels[i].ic_flags |= IEEE80211_CHAN_PASSIVE;
			}
		}
	}

	if (dfsactscan)
		printk("STA is now configured to do an active scan on DFS channels\n");
	else
		printk("STA is now configured to do a passive scan on DFS channels (default)\n");

        return 0;

#undef IS_DFS_CHAN
}

static int
ieee80211_ioctl_wdsmac(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct sockaddr *sa = (struct sockaddr *)extra;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *vap_tmp;
	struct ieee80211vap *vap_found = NULL;

	if (vap->iv_opmode != IEEE80211_M_WDS)
		return -EOPNOTSUPP;

	if (!IEEE80211_ADDR_NULL(vap->wds_mac)) {
		printk("%s: Failed to add WDS MAC: %s\n", dev->name,
			ether_sprintf((u_int8_t *)sa->sa_data));
		printk("%s: Device already has WDS mac address attached,"
			" remove first\n", dev->name);
		return -1;
	}

	TAILQ_FOREACH(vap_tmp, &ic->ic_vaps, iv_next) {
		if (IEEE80211_ADDR_EQ(vap_tmp->iv_myaddr, sa->sa_data)) {
			vap_found = vap_tmp;
			break;
		}

		if ((vap_tmp->iv_opmode == IEEE80211_M_WDS) &&
				(IEEE80211_ADDR_EQ(vap_tmp->wds_mac, sa->sa_data)) ) {
			vap_found = vap_tmp;
			break;
		}
	}

	if (vap_found) {
		printk("%s: The mac address(%s) has been used by device(%s)\n",
				dev->name,
				ether_sprintf((u_int8_t *)sa->sa_data),
				vap_found->iv_dev->name);
		return -EINVAL;
	}

	memcpy(vap->wds_mac, sa->sa_data, IEEE80211_ADDR_LEN);

	printk("%s: Added WDS MAC: %s\n", dev->name,
		ether_sprintf(vap->wds_mac));

	return 0;
}

static int
ieee80211_ioctl_wdsdelmac(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct sockaddr *sa = (struct sockaddr *)extra;
	struct ieee80211com *ic = vap->iv_ic;

	if (IEEE80211_ADDR_NULL(vap->wds_mac))
		return 0;

	if (vap->iv_opmode != IEEE80211_M_WDS)
		return -EOPNOTSUPP;

	/*
	 * Compare supplied MAC address with WDS MAC of this interface
	 * remove when mac address is known
	 */
	if (IEEE80211_ADDR_EQ(vap->wds_mac, sa->sa_data)) {
		ieee80211_extender_remove_peer_wds_info(ic, vap->wds_mac);

		IEEE80211_ADDR_SET_NULL(vap->wds_mac);
		return 0;
	}

	printk("%s: WDS MAC address %s is not known by this interface\n",
	       dev->name, ether_sprintf((u_int8_t *)sa->sa_data));

	return -1;
}

/*
 * kick associated station with the given MAC address.
 */
static int
ieee80211_ioctl_kickmac(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct sockaddr *sa = (struct sockaddr *)extra;
	struct ieee80211req_mlme mlme;

	if (sa->sa_family != ARPHRD_ETHER)
		return -EINVAL;

	/* Setup a MLME request for disassociation of the given MAC */
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = IEEE80211_REASON_UNSPECIFIED;
	IEEE80211_ADDR_COPY(&(mlme.im_macaddr), sa->sa_data);

	/* Send the MLME request and return the result. */
	return ieee80211_ioctl_setmlme(dev, info, w, (char *)&mlme);
}

/* Currently this function is used to associate with an AP with given bssid */
static int
ieee80211_ioctl_addmac(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct sockaddr *sa = (struct sockaddr *)extra;

#ifdef DEMO_CONTROL
	struct ieee80211com *ic = vap->iv_ic;
	memcpy(vap->iv_des_bssid, sa->sa_data, IEEE80211_ADDR_LEN);


	ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_BSSID,
		0, vap->iv_des_bssid, IEEE80211_ADDR_LEN);

	if (IS_UP(vap->iv_dev))
		return ic->ic_reset(ic);
#else
	const struct ieee80211_aclator *acl = vap->iv_acl;
	if (acl == NULL) {
		acl = ieee80211_aclator_get("mac");
		if (acl == NULL || !acl->iac_attach(vap))
			return -EINVAL;
		vap->iv_acl = acl;
	}
	acl->iac_add(vap, sa->sa_data);
#endif
	return 0;
}

static int
ieee80211_ioctl_delmac(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct sockaddr *sa = (struct sockaddr *)extra;
	const struct ieee80211_aclator *acl = vap->iv_acl;

	if (acl == NULL) {
		acl = ieee80211_aclator_get("mac");
		if (acl == NULL || !acl->iac_attach(vap))
			return -EINVAL;
		vap->iv_acl = acl;
	}
	acl->iac_remove(vap, (u_int8_t *)sa->sa_data);
	return 0;
}

static int
ieee80211_ioctl_setchanlist(struct net_device *dev,
	struct iw_request_info *info, void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211req_chanlist *list =
		(struct ieee80211req_chanlist *)extra;
	u_char chanlist[IEEE80211_CHAN_BYTES];
	int i, j, k = 0, nchan;
	struct ieee80211_channel *ch;

	memset(chanlist, 0, sizeof(chanlist));
	/*
	 * Since channel 0 is not available for DS, channel 1
	 * is assigned to LSB on WaveLAN.
	 */
	if ((ic->ic_phytype == IEEE80211_T_DS) || (ic->ic_phytype == IEEE80211_T_OFDM))
		i = 1;
	else
		i = 0;
	nchan = 0;
	for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
		/*
		 * NB: silently discard unavailable channels so users
		 *     can specify 1-255 to get all available channels.
		 */
		if (isset(list->ic_channels, j) && isset(ic->ic_chan_avail, i)) {
			if (ic->ic_dfs_channels_deactive && isset(ic->ic_chan_dfs_required, i)) {
				continue;
			}
			if (local_is_channel_disabled(ic, i, ieee80211_get_bw(ic))) {
				continue;
			}
			setbit(chanlist, i);
			nchan++;
		}
	}

	if (nchan == 0)			/* no valid channels, disallow */
		return -EINVAL;
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&	/* XXX */
	    isclr(chanlist, ic->ic_bsschan->ic_ieee)) {
		ic->ic_bsschan = IEEE80211_CHAN_ANYC;	/* invalidate */
		k = 1;
	}

	memset(ic->ic_chan_active_80, 0, sizeof(ic->ic_chan_active_80));
	memset(ic->ic_chan_active_40, 0, sizeof(ic->ic_chan_active_40));
	memset(ic->ic_chan_active_20, 0, sizeof(ic->ic_chan_active_20));

	for (i = 0; i < ic->ic_nchans; i++) {
		ch = &ic->ic_channels[i];
		if (isset(chanlist, ch->ic_ieee)) {
			if (IEEE80211_IS_CHAN_HT40(ch))
				setbit(ic->ic_chan_active_40, ch->ic_ieee);

			if (IEEE80211_IS_CHAN_VHT80(ch))
				setbit(ic->ic_chan_active_80, ch->ic_ieee);

			setbit(ic->ic_chan_active_20, ch->ic_ieee);
		}
	}

	memcpy(ic->ic_chan_active, chanlist, sizeof(ic->ic_chan_active));
	if (IS_UP_AUTO(vap)) {
		if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
				isclr(chanlist, ic->ic_des_chan->ic_ieee)) {
			ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		}
		if (ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
			int bw = ieee80211_get_bw(ic);
			if (((bw >= BW_HT80) && !(ic->ic_des_chan->ic_flags & IEEE80211_CHAN_VHT80)) ||
					((bw >= BW_HT40) && !(ic->ic_des_chan->ic_flags & IEEE80211_CHAN_HT40))) {
				ic->ic_des_chan = IEEE80211_CHAN_ANYC;
			}
		}
		ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	/* send disassoc to ap when BSS channel is invalid. */
	} else if (k && vap->iv_state == IEEE80211_S_RUN &&
			vap->iv_opmode != IEEE80211_M_MONITOR) {
		ieee80211_new_state(vap, IEEE80211_S_INIT, 0);
	}

	return 0;
}

static int
ieee80211_ioctl_getchanlist(struct net_device *dev,
	struct iw_request_info *info, void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	union iwreq_data *iwr = (union iwreq_data *)w;

	memcpy(extra, ic->ic_chan_active, sizeof(ic->ic_chan_active));
	iwr->data.length = sizeof(ic->ic_chan_active);
	return 0;
}

static int
ieee80211_ioctl_getchaninfo(struct net_device *dev,
	struct iw_request_info *info, void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	union iwreq_data *iwr = (union iwreq_data *)w;
	struct ieee80211req_chaninfo *chans =
		(struct ieee80211req_chaninfo *) extra;
	u_int8_t reported[IEEE80211_CHAN_BYTES];	/* XXX stack usage? */
	int i;

	memset(chans, 0, sizeof(*chans));
	memset(reported, 0, sizeof(reported));
	for (i = 0; i < ic->ic_nchans; i++) {
		const struct ieee80211_channel *c = &ic->ic_channels[i];
		const struct ieee80211_channel *c1 = c;

		if (isclr(reported, c->ic_ieee)) {
			setbit(reported, c->ic_ieee);

			/* pick turbo channel over non-turbo channel, and
			 * 11g channel over 11b channel */
			if (IEEE80211_IS_CHAN_A(c))
				c1 = findchannel(ic, c->ic_ieee, IEEE80211_MODE_TURBO_A);
			if (IEEE80211_IS_CHAN_ANYG(c))
				c1 = findchannel(ic, c->ic_ieee, IEEE80211_MODE_TURBO_G);
			else if (IEEE80211_IS_CHAN_B(c)) {
				c1 = findchannel(ic, c->ic_ieee, IEEE80211_MODE_TURBO_G);
				if (!c1)
					c1 = findchannel(ic, c->ic_ieee, IEEE80211_MODE_11G);
			}

			if (c1)
				c = c1;
			chans->ic_chans[chans->ic_nchans].ic_ieee = c->ic_ieee;
			chans->ic_chans[chans->ic_nchans].ic_freq = c->ic_freq;
			chans->ic_chans[chans->ic_nchans].ic_flags = c->ic_flags;
			if (++chans->ic_nchans >= IEEE80211_CHAN_MAX)
				break;
		}
	}

	iwr->data.length = chans->ic_nchans * sizeof(struct ieee80211_chan);
	return 0;
}

static int
ieee80211_ioctl_setwmmparams(struct net_device *dev,
	struct iw_request_info *info, void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	int *param = (int *) extra;
	int ac = (param[1] >= 0 && param[1] < WME_NUM_AC) ?
		param[1] : WME_AC_BE;
	int bss = param[2];
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
#ifdef CONFIG_QVSP
	struct ieee80211com *ic = vap->iv_ic;
#endif

	switch (param[0]) {
	case IEEE80211_WMMPARAMS_CWMIN:
		if (param[3] < 0 || param[3] > 15)
			return -EINVAL;
		if (bss) {
			wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmm_logcwmin = param[3];
			wme->wme_wmeBssChanParams.cap_info_count++;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0) {
				wme->wme_bssChanParams.cap_wmeParams[ac].wmm_logcwmin = param[3];
			}

		} else {
			wme->wme_wmeChanParams.cap_wmeParams[ac].wmm_logcwmin = param[3];
			wme->wme_wmeChanParams.cap_info_count++;
			wme->wme_chanParams.cap_wmeParams[ac].wmm_logcwmin = param[3];
		}
		ieee80211_wme_updateparams(vap, !bss);
		break;
	case IEEE80211_WMMPARAMS_CWMAX:
		if (param[3] < 0 || param[3] > 15)
			return -EINVAL;
		if (bss) {
			wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmm_logcwmax = param[3];
			wme->wme_wmeBssChanParams.cap_info_count++;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0) {
				wme->wme_bssChanParams.cap_wmeParams[ac].wmm_logcwmax = param[3];
			}
		} else {
			wme->wme_wmeChanParams.cap_wmeParams[ac].wmm_logcwmax = param[3];
			wme->wme_wmeChanParams.cap_info_count++;
			wme->wme_chanParams.cap_wmeParams[ac].wmm_logcwmax = param[3];
		}
		ieee80211_wme_updateparams(vap, !bss);
		break;
	case IEEE80211_WMMPARAMS_AIFS:
		if (param[3] < 0 || param[3] > 15)
			return -EINVAL;
		if (bss) {
			wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmm_aifsn = param[3];
			wme->wme_wmeBssChanParams.cap_info_count++;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
				wme->wme_bssChanParams.cap_wmeParams[ac].wmm_aifsn = param[3];
		} else {
			wme->wme_wmeChanParams.cap_wmeParams[ac].wmm_aifsn = param[3];
			wme->wme_wmeChanParams.cap_info_count++;
			wme->wme_chanParams.cap_wmeParams[ac].wmm_aifsn = param[3];
		}
		ieee80211_wme_updateparams(vap, !bss);
		break;
	case IEEE80211_WMMPARAMS_TXOPLIMIT:
		if (param[3] < 0 || param[3] > 8192)
			return -EINVAL;
		if (bss) {
			wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmm_txopLimit
				= IEEE80211_US_TO_TXOP(param[3]);
			wme->wme_wmeBssChanParams.cap_info_count++;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
				wme->wme_bssChanParams.cap_wmeParams[ac].wmm_txopLimit =
					IEEE80211_US_TO_TXOP(param[3]);
		} else {
			wme->wme_wmeChanParams.cap_wmeParams[ac].wmm_txopLimit
				= IEEE80211_US_TO_TXOP(param[3]);
			wme->wme_wmeChanParams.cap_info_count++;
			wme->wme_chanParams.cap_wmeParams[ac].wmm_txopLimit
				= IEEE80211_US_TO_TXOP(param[3]);
		}
		ieee80211_wme_updateparams(vap, !bss);
		break;
	case IEEE80211_WMMPARAMS_ACM:
		if (!bss || param[3] < 0 || param[3] > 1)
			return -EINVAL;
		/* ACM bit applies to BSS case only */
		wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmm_acm = param[3];
		wme->wme_wmeBssChanParams.cap_info_count++;
		if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
			wme->wme_bssChanParams.cap_wmeParams[ac].wmm_acm = param[3];
		ieee80211_wme_updateparams(vap, 0);
		break;
	case IEEE80211_WMMPARAMS_NOACKPOLICY:
		if (bss || param[3] < 0 || param[3] > 1)
			return -EINVAL;
		/* ack policy applies to non-BSS case only */
		wme->wme_wmeChanParams.cap_wmeParams[ac].wmm_noackPolicy = param[3];
		wme->wme_wmeChanParams.cap_info_count++;
		wme->wme_chanParams.cap_wmeParams[ac].wmm_noackPolicy = param[3];
		ieee80211_vap_sync_chan_wmestate(vap);
		break;
	default:
		break;
	}

#ifdef CONFIG_QVSP
	if (ic->ic_vsp_reset) {
		ic->ic_vsp_reset(ic);
	}
#endif

	return 0;
}

static int
ieee80211_ioctl_getwmmparams(struct net_device *dev,
	struct iw_request_info *info, void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	int *param = (int *) extra;
	int ac = (param[1] >= 0 && param[1] < WME_NUM_AC) ?
		param[1] : WME_AC_BE;
	int bss = param[2];
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
	struct chanAccParams *chanParams = (!bss) ?
		&(wme->wme_chanParams) : &(wme->wme_bssChanParams);

	switch (param[0]) {
        case IEEE80211_WMMPARAMS_CWMIN:
		param[0] = chanParams->cap_wmeParams[ac].wmm_logcwmin;
		break;
        case IEEE80211_WMMPARAMS_CWMAX:
		param[0] = chanParams->cap_wmeParams[ac].wmm_logcwmax;
		break;
        case IEEE80211_WMMPARAMS_AIFS:
		param[0] = chanParams->cap_wmeParams[ac].wmm_aifsn;
		break;
        case IEEE80211_WMMPARAMS_TXOPLIMIT:
		param[0] = IEEE80211_TXOP_TO_US(chanParams->cap_wmeParams[ac].wmm_txopLimit);
		break;
        case IEEE80211_WMMPARAMS_ACM:
		param[0] = wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmm_acm;
		break;
        case IEEE80211_WMMPARAMS_NOACKPOLICY:
		param[0] = wme->wme_wmeChanParams.cap_wmeParams[ac].wmm_noackPolicy;
		break;
	default:
		break;
	}
	return 0;
}

static int
ieee80211_ioctl_getwpaie(struct net_device *dev, struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	struct ieee80211req_wpaie wpaie = {{0}};

	if (iwr->u.data.length != sizeof(wpaie))
		return -EINVAL;
	if (copy_from_user(&wpaie, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
		return -EFAULT;
	ni = ieee80211_find_node(&ic->ic_sta, wpaie.wpa_macaddr);
	if (ni == NULL)
		return -EINVAL;		/* XXX */
	if (ni->ni_wpa_ie != NULL) {
		int ielen = ni->ni_wpa_ie[1] + 2;
		if (ielen > sizeof(wpaie.wpa_ie))
			ielen = sizeof(wpaie.wpa_ie);
		memcpy(wpaie.wpa_ie, ni->ni_wpa_ie, ielen);
	}
	if (ni->ni_rsn_ie != NULL) {
		int ielen = ni->ni_rsn_ie[1] + 2;
		if (ielen > sizeof(wpaie.rsn_ie))
			ielen = sizeof(wpaie.rsn_ie);
		memcpy(wpaie.rsn_ie, ni->ni_rsn_ie, ielen);
	}
	if (ni->ni_osen_ie != NULL) {
		int ielen = ni->ni_osen_ie[1] + 2;
		if (ielen > sizeof(wpaie.osen_ie))
			ielen = sizeof(wpaie.osen_ie);
		memcpy(wpaie.osen_ie, ni->ni_osen_ie, ielen);
	}
	if (ni->ni_wsc_ie != NULL) {
		int ielen = ni->ni_wsc_ie[1] + 2;

		if (ielen > sizeof(wpaie.wps_ie)) {
			ielen = sizeof(wpaie.wps_ie);
		}
		memcpy(wpaie.wps_ie, ni->ni_wsc_ie, ielen);
	}
	if (ni->ni_qtn_pairing_ie != NULL) {
		struct ieee80211_ie_qtn_pairing *hash_ie = (struct ieee80211_ie_qtn_pairing *)ni->ni_qtn_pairing_ie;

		memcpy(wpaie.qtn_pairing_ie, hash_ie->qtn_pairing_tlv.qtn_pairing_tlv_hash, QTN_PAIRING_TLV_HASH_LEN);
		wpaie.has_pairing_ie = QTN_PAIRING_IE_EXIST;
	} else {
		wpaie.has_pairing_ie = QTN_PAIRING_IE_ABSENT;
	}

	ieee80211_free_node(ni);
	return (copy_to_user(iwr->u.data.pointer, &wpaie, sizeof(wpaie)) ?
		-EFAULT : 0);
}

#if defined(CONFIG_QTN_80211K_SUPPORT)
static int
ieee80211_ioctl_getstastatistic(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *data, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni = NULL;
	struct ieee80211req_qtn_rmt_sta_stats *sta_statistic = NULL;
	struct ieee80211req_qtn_rmt_sta_stats_setpara setpara;
	int ret = 0;

	if (copy_from_user(&setpara, data->pointer, sizeof(struct ieee80211req_qtn_rmt_sta_stats_setpara))) {
		return -EFAULT;
	}

	ni = ieee80211_find_node(nt, setpara.macaddr);

	if (ni) {
		if (setpara.flags == RM_STANDARD_CCA) {
			ieee80211_send_rm_req_cca(ni);
		} else {
			if (ni->ni_qtn_assoc_ie == NULL) {
				if (setpara.flags == BIT(RM_QTN_RSSI_DBM)) {
					setpara.flags = BIT(RM_GRP221_RSSI);
				} else if (setpara.flags == BIT(RM_QTN_HW_NOISE)) {
					setpara.flags = BIT(RM_GRP221_PHY_NOISE);
				} else if (setpara.flags == BIT(RM_QTN_SOC_MACADDR)) {
					setpara.flags = BIT(RM_GRP221_SOC_MAC);
				}
			}

			sta_statistic = (struct ieee80211req_qtn_rmt_sta_stats *)extra;

			/* Set pending flag and clear status */
			ni->ni_dotk_meas_state.meas_state_sta.pending = 1;
			ni->ni_dotk_meas_state.meas_state_sta.status = 0;

			ieee80211_send_rm_req_stastats(ni, setpara.flags);

			/*
			 * ret = 0, thread is waked up
			 * ret < 0, interrupted by SIGNAL
			 * */
			ret = wait_event_interruptible(ni->ni_dotk_waitq,
					ni->ni_dotk_meas_state.meas_state_sta.pending == 0);

			data->length = sizeof(struct ieee80211req_qtn_rmt_sta_stats);
			if (ret == 0) {
				if (ni->ni_dotk_meas_state.meas_state_sta.status == 0) {
					memset(sta_statistic, 0, sizeof(struct ieee80211req_qtn_rmt_sta_stats));

					if (setpara.flags & RM_QTN_MEASURE_MASK) {
						memcpy(&(sta_statistic->rmt_sta_stats),
								&(ni->ni_qtn_rm_sta_all),
								sizeof(struct ieee80211_ie_qtn_rm_sta_all));
					} else {
						if (setpara.flags & BIT(RM_GRP221_RSSI)) {
							int rm_rssi = *(int8_t *)&ni->ni_rm_sta_grp221.rssi;
							if (rm_rssi < 0) {
								sta_statistic->rmt_sta_stats.rssi_dbm = rm_rssi * 10 + 5;
							} else {
								sta_statistic->rmt_sta_stats.rssi_dbm = rm_rssi * 10 - 5;
							}
						}

						if (setpara.flags & BIT(RM_GRP221_PHY_NOISE)) {
							int rm_noise = *(int8_t *)&ni->ni_rm_sta_grp221.phy_noise;
							sta_statistic->rmt_sta_stats.hw_noise = rm_noise * 10;
						}

						if (setpara.flags & BIT(RM_GRP221_SOC_MAC)) {
							memcpy(sta_statistic->rmt_sta_stats.soc_macaddr,
									ni->ni_rm_sta_grp221.soc_macaddr,
									IEEE80211_ADDR_LEN);
						}
					}

					/* Success */
					sta_statistic->status = 0;
				} else {
					/* Timer expired / peer don't support */
					sta_statistic->status = ni->ni_dotk_meas_state.meas_state_sta.status;
				}
			} else {
				/* Canceled by signal */
				sta_statistic->status = -ECANCELED;
			}
			ret = 0;
		}
		ieee80211_free_node(ni);
	} else {
		ret = -EINVAL;
	}

	return ret;
}
#endif

static int
ieee80211_ioctl_getstastats(struct net_device *dev, struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	u_int8_t macaddr[IEEE80211_ADDR_LEN];
	const int off = __offsetof(struct ieee80211req_sta_stats, is_stats);
	int error;

	if (iwr->u.data.length < off)
		return -EINVAL;
	if (copy_from_user(macaddr, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
		return -EFAULT;
	ni = ieee80211_find_node(&ic->ic_sta, macaddr);
	if (ni == NULL)
		return -EINVAL;		/* XXX */
	if (ic->ic_get_shared_node_stats)
		ic->ic_get_shared_node_stats(ni);
	if (iwr->u.data.length > sizeof(struct ieee80211req_sta_stats))
		iwr->u.data.length = sizeof(struct ieee80211req_sta_stats);
	/* NB: copy out only the statistics */
	error = copy_to_user(iwr->u.data.pointer + off, &ni->ni_stats,
		iwr->u.data.length - off);
	ieee80211_free_node(ni);
	return (error ? -EFAULT : 0);
}

struct scanreq {			/* XXX: right place for declaration? */
	struct ieee80211req_scan_result *sr;
	size_t space;
};

static size_t
scan_space(const struct ieee80211_scan_entry *se, int *ielen)
{
	*ielen = 0;
	if (se->se_rsn_ie != NULL)
		*ielen += 2 + se->se_rsn_ie[1];
	if (se->se_wpa_ie != NULL)
		*ielen += 2 + se->se_wpa_ie[1];
	if (se->se_wme_ie != NULL)
		*ielen += 2 + se->se_wme_ie[1];
	if (se->se_ath_ie != NULL)
		*ielen += 2 + se->se_ath_ie[1];
	if (se->se_htcap_ie != NULL)
		*ielen += 2 + se->se_htcap_ie[1];

	return roundup(sizeof(struct ieee80211req_scan_result) +
		se->se_ssid[1] + *ielen, sizeof(u_int32_t));
}

static int
get_scan_space(void *arg, const struct ieee80211_scan_entry *se)
{
	struct scanreq *req = arg;
	int ielen;

	req->space += scan_space(se, &ielen);

	return 0;
}

static int
get_scan_result(void *arg, const struct ieee80211_scan_entry *se)
{
	struct scanreq *req = arg;
	struct ieee80211req_scan_result *sr;
	int ielen, len, nr, nxr;
	u_int8_t *cp;

	len = scan_space(se, &ielen);
	if (len > req->space) {
	  printk("[madwifi] %s() : Not enough space.\n", __FUNCTION__);
		return 0;
	}

	sr = req->sr;
	memset(sr, 0, sizeof(*sr));
	sr->isr_ssid_len = se->se_ssid[1];
	/* XXX watch for overflow */
	sr->isr_ie_len = ielen;
	sr->isr_len = len;
	sr->isr_freq = se->se_chan->ic_freq;
	sr->isr_flags = se->se_chan->ic_flags;
	sr->isr_rssi = se->se_rssi;
	sr->isr_intval = se->se_intval;
	sr->isr_capinfo = se->se_capinfo;
	sr->isr_erp = se->se_erp;
	IEEE80211_ADDR_COPY(sr->isr_bssid, se->se_bssid);
	/* XXX bounds check */
	nr = se->se_rates[1];
	memcpy(sr->isr_rates, se->se_rates + 2, nr);
	nxr = se->se_xrates[1];
	memcpy(sr->isr_rates+nr, se->se_xrates + 2, nxr);
	sr->isr_nrates = nr + nxr;

	cp = (u_int8_t *)(sr + 1);
	memcpy(cp, se->se_ssid + 2, sr->isr_ssid_len);
	cp += sr->isr_ssid_len;
	if (se->se_rsn_ie != NULL) {
		memcpy(cp, se->se_rsn_ie, 2 + se->se_rsn_ie[1]);
		cp += 2 + se->se_rsn_ie[1];
	}
	if (se->se_wpa_ie != NULL) {
		memcpy(cp, se->se_wpa_ie, 2 + se->se_wpa_ie[1]);
		cp += 2 + se->se_wpa_ie[1];
	}
	if (se->se_wme_ie != NULL) {
		memcpy(cp, se->se_wme_ie, 2 + se->se_wme_ie[1]);
		cp += 2 + se->se_wme_ie[1];
	}
	if (se->se_ath_ie != NULL) {
		memcpy(cp, se->se_ath_ie, 2 + se->se_ath_ie[1]);
		cp += 2 + se->se_ath_ie[1];
	}
	if (se->se_htcap_ie != NULL) {
		memcpy(cp, se->se_htcap_ie, 2 + se->se_htcap_ie[1]);
		cp += 2 + se->se_htcap_ie[1];
	}

	req->space -= len;
	req->sr = (struct ieee80211req_scan_result *)(((u_int8_t *)sr) + len);

	return 0;
}

static int
ieee80211_ioctl_getscanresults(struct net_device *dev, struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct scanreq req;
	int error;

	if (iwr->u.data.length < sizeof(struct scanreq))
		return -EFAULT;

	error = 0;
	req.space = 0;
	ieee80211_scan_iterate(ic, get_scan_space, &req);
	if (req.space > iwr->u.data.length)
		req.space = iwr->u.data.length;
	if (req.space > 0) {
		size_t space;
		void *p;

		space = req.space;
		MALLOC(p, void *, space, M_TEMP, M_WAITOK);
		if (p == NULL)
			return -ENOMEM;
		req.sr = p;
		ieee80211_scan_iterate(ic, get_scan_result, &req);
		iwr->u.data.length = space - req.space;
		error = copy_to_user(iwr->u.data.pointer, p, iwr->u.data.length);
		FREE(p, M_TEMP);
	} else
		iwr->u.data.length = 0;

	return (error ? -EFAULT : 0);
}

struct stainforeq {		/* XXX: right place for declaration? */
	struct ieee80211vap *vap;
	struct ieee80211req_sta_info *si;
	size_t	space;
};

static size_t
sta_space(const struct ieee80211_node *ni, size_t *ielen)
{
	*ielen = 0;
	if (ni->ni_rsn_ie != NULL)
		*ielen += 2+ni->ni_rsn_ie[1];
	if (ni->ni_wpa_ie != NULL)
		*ielen += 2+ni->ni_wpa_ie[1];
	if (ni->ni_wme_ie != NULL)
		*ielen += 2+ni->ni_wme_ie[1];
	if (ni->ni_ath_ie != NULL)
		*ielen += 2+ni->ni_ath_ie[1];
	return roundup(sizeof(struct ieee80211req_sta_info) + *ielen,
		      sizeof(u_int32_t));
}

static void
get_sta_space(void *arg, struct ieee80211_node *ni)
{
	struct stainforeq *req = arg;
	struct ieee80211vap *vap = ni->ni_vap;
	size_t ielen;

	if (vap != req->vap && vap != req->vap->iv_xrvap)	/* only entries for this vap */
		return;
	if ((vap->iv_opmode == IEEE80211_M_HOSTAP ||
	     vap->iv_opmode == IEEE80211_M_WDS) &&
	    ni->ni_associd == 0)				/* only associated stations or a WDS peer */
		return;
	req->space += sta_space(ni, &ielen);
}

static void
get_sta_info(void *arg, struct ieee80211_node *ni)
{
	struct stainforeq *req = arg;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211req_sta_info *si;
	size_t ielen, len;
	u_int8_t *cp;

	if (vap != req->vap && vap != req->vap->iv_xrvap)	/* only entries for this vap (or) xrvap */
		return;
	if ((vap->iv_opmode == IEEE80211_M_HOSTAP ||
	     vap->iv_opmode == IEEE80211_M_WDS) &&
	    ni->ni_associd == 0)				/* only associated stations or a WDS peer */
		return;
	if (ni->ni_chan == IEEE80211_CHAN_ANYC)			/* XXX bogus entry */
		return;
	len = sta_space(ni, &ielen);
	if (len > req->space)
		return;
	si = req->si;
	si->isi_len = len;
	si->isi_ie_len = ielen;
	si->isi_freq = ni->ni_chan->ic_freq;
	si->isi_flags = ni->ni_chan->ic_flags;
	si->isi_state = ni->ni_flags;
	si->isi_authmode = ni->ni_authmode;
	si->isi_rssi = ic->ic_node_getrssi(ni);
	si->isi_capinfo = ni->ni_capinfo;
	si->isi_athflags = ni->ni_ath_flags;
	si->isi_erp = ni->ni_erp;
	IEEE80211_ADDR_COPY(si->isi_macaddr, ni->ni_macaddr);
	si->isi_nrates = ni->ni_rates.rs_nrates;
	if (si->isi_nrates > 15)
		si->isi_nrates = 15;
	memcpy(si->isi_rates, ni->ni_rates.rs_rates, si->isi_nrates);
	si->isi_txrate = ni->ni_txrate;
	si->isi_ie_len = ielen;
	si->isi_associd = ni->ni_associd;
	si->isi_txpower = ni->ni_txpower;
	si->isi_vlan = ni->ni_vlan;
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		memcpy(si->isi_txseqs, ni->ni_txseqs, sizeof(ni->ni_txseqs));
		memcpy(si->isi_rxseqs, ni->ni_rxseqs, sizeof(ni->ni_rxseqs));
	} else {
		si->isi_txseqs[0] = ni->ni_txseqs[0];
		si->isi_rxseqs[0] = ni->ni_rxseqs[0];
	}
	si->isi_uapsd = ni->ni_uapsd;
	if ( vap == req->vap->iv_xrvap)
		si->isi_opmode = IEEE80211_STA_OPMODE_XR;
	else
		si->isi_opmode = IEEE80211_STA_OPMODE_NORMAL;
	/* NB: leave all cases in case we relax ni_associd == 0 check */
	if (ieee80211_node_is_authorized(ni))
		si->isi_inact = vap->iv_inact_run;
	else if (ni->ni_associd != 0)
		si->isi_inact = vap->iv_inact_auth;
	else
		si->isi_inact = vap->iv_inact_init;
	si->isi_inact = (si->isi_inact - ni->ni_inact) * IEEE80211_INACT_WAIT;

	cp = (u_int8_t *)(si+1);
	if (ni->ni_rsn_ie != NULL) {
		memcpy(cp, ni->ni_rsn_ie, 2 + ni->ni_rsn_ie[1]);
		cp += 2 + ni->ni_rsn_ie[1];
        }
	if (ni->ni_wpa_ie != NULL) {
		memcpy(cp, ni->ni_wpa_ie, 2 + ni->ni_wpa_ie[1]);
		cp += 2 + ni->ni_wpa_ie[1];
	}
	if (ni->ni_wme_ie != NULL) {
		memcpy(cp, ni->ni_wme_ie, 2 + ni->ni_wme_ie[1]);
		cp += 2 + ni->ni_wme_ie[1];
	}
	if (ni->ni_ath_ie != NULL) {
		memcpy(cp, ni->ni_ath_ie, 2 + ni->ni_ath_ie[1]);
		cp += 2 + ni->ni_ath_ie[1];
	}

	req->si = (struct ieee80211req_sta_info *)(((u_int8_t *)si) + len);
	req->space -= len;
}

static int
ieee80211_ioctl_getstainfo(struct net_device *dev, struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct stainforeq req;
	int error;

	if (iwr->u.data.length < sizeof(struct stainforeq))
		return -EFAULT;

	/* estimate space required for station info */
	error = 0;
	req.space = sizeof(struct stainforeq);
	req.vap = vap;
	ieee80211_iterate_nodes(&ic->ic_sta, get_sta_space, &req, 1);
	if (req.space > iwr->u.data.length)
		req.space = iwr->u.data.length;
	if (req.space > 0) {
		size_t space;
		void *p;

		space = req.space;
		MALLOC(p, void *, space, M_TEMP, M_WAITOK);
		req.si = (struct ieee80211req_sta_info *)p;
		ieee80211_iterate_nodes(&ic->ic_sta, get_sta_info, &req, 1);
		iwr->u.data.length = space - req.space;
		error = copy_to_user(iwr->u.data.pointer, p, iwr->u.data.length);
		FREE(p, M_TEMP);
	} else
		iwr->u.data.length = 0;

	return (error ? -EFAULT : 0);
}

static __inline const char *
ieee80211_get_vendor_str(uint8_t vendor)
{
	const char *vendor_str = "unknown";

	switch (vendor) {
	case PEER_VENDOR_QTN:
		vendor_str = "qtn";
		break;
	case PEER_VENDOR_BRCM:
		vendor_str = "brcm";
		break;
	case PEER_VENDOR_ATH:
		vendor_str = "ath";
		break;
	case PEER_VENDOR_RLNK:
		vendor_str = "rlnk";
		break;
	case PEER_VENDOR_RTK:
		vendor_str = "rtk";
		break;
	case PEER_VENDOR_INTEL:
		vendor_str = "intel";
		break;
	default:
		break;
	}

	return vendor_str;
}

/* This array must be kept in sync with the IEEE80211_NODE_TYPE_xxx values */
static const char *
ieee80211_node_type_str[] = {
	"none",
	"vap",
	"sta",
	"wds",
	"tdls"
};

static __inline const char *
ieee80211_get_node_type_str(uint8_t type)
{
	if (type >= ARRAY_SIZE(ieee80211_node_type_str))
		type = IEEE80211_NODE_TYPE_NONE;

	return ieee80211_node_type_str[type];
}

static void
get_node_ht_bw_and_sgi(struct ieee80211com *ic, struct ieee80211_node *ni,
		uint8_t *bw, uint8_t *assoc_bw, uint8_t *sgi)
{
	if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40 &&
			ni->ni_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40) {
		*assoc_bw = 40;
		*bw = IEEE80211_CWM_WIDTH40;
		if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40 &&
				ni->ni_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40)
			*sgi = 1;
		else
			*sgi = 0;
	} else {
		*assoc_bw = 20;
		*bw = IEEE80211_CWM_WIDTH20;
		if (ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 &&
				ni->ni_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20)
			*sgi = 1;
		else
			*sgi = 0;
	}
}

static void
get_node_vht_bw_and_sgi(struct ieee80211_node *ni, uint8_t *bw,
		uint8_t *assoc_bw, uint8_t *sgi)
{
	switch (ni->ni_vhtop.chanwidth) {
	case IEEE80211_VHTOP_CHAN_WIDTH_160MHZ:
	case IEEE80211_VHTOP_CHAN_WIDTH_80PLUS80MHZ:
		*bw = IEEE80211_CWM_WIDTH160;
		*assoc_bw = 160;
		if (ni->ni_vhtcap.cap_flags & IEEE80211_VHTCAP_C_SHORT_GI_160)
			*sgi = 1;
		else
			*sgi = 0;
		break;
	case IEEE80211_VHTOP_CHAN_WIDTH_80MHZ:
		*bw = IEEE80211_CWM_WIDTH80;
		*assoc_bw = 80;
		if (ni->ni_vhtcap.cap_flags & IEEE80211_VHTCAP_C_SHORT_GI_80)
			*sgi = 1;
		else
			*sgi = 0;
		break;
	case IEEE80211_VHTOP_CHAN_WIDTH_20_40MHZ:
		get_node_ht_bw_and_sgi(ni->ni_ic, ni, bw, assoc_bw, sgi);
		break;
	default:
		break;
	}
}

static void
get_node_ht_max_mcs(struct ieee80211com *ic, struct ieee80211_node *ni,
		uint8_t *max_mcs)
{
	int i;

	for (i = IEEE80211_HT_MCSSET_20_40_NSS1; i <= IEEE80211_HT_MCSSET_20_40_NSS4; i++) {
		if ((ic->ic_htcap.mcsset[i] & ni->ni_htcap.mcsset[i]) == 0xff)
			*max_mcs = 8 * (i - IEEE80211_HT_MCSSET_20_40_NSS1 + 1) - 1;
	}
}

static void
get_node_vht_max_nss_mcs(struct ieee80211_node *ni, uint8_t *max_nss, uint8_t *max_mcs)
{
	uint8_t mcs = 0;
	uint8_t nss = 0;
	uint16_t tx_mcsnss_map;
	int i;

	tx_mcsnss_map = ni->ni_vhtcap.txmcsmap;

	for (i = IEEE80211_VHT_NSS1; i <= IEEE80211_VHT_NSS8; i++) {
		switch (tx_mcsnss_map & 0x03) {
		case IEEE80211_VHT_MCS_0_7:
			mcs = 7;
			nss++;
			break;
		case IEEE80211_VHT_MCS_0_8:
			mcs = 8;
			nss++;
			break;
		case IEEE80211_VHT_MCS_0_9:
			mcs = 9;
			nss++;
			break;
		case IEEE80211_VHT_MCS_NA:
		default:
			/* do nothing */
			break;
		}
		tx_mcsnss_map >>= 2;
	}

	if (mcs != 0) {
		*max_mcs = mcs;
		*max_nss = nss - 1; /* Nss index starts from 0 */
	}
}

static void get_node_max_mimo(struct ieee80211_node *ni, uint8_t *tx, uint8_t *rx)
{
	uint8_t tx_max = 0;
	uint8_t rx_max = 0;
	uint16_t vht_mcsmap = 0;
        struct ieee80211_ie_htcap *htcap = (struct ieee80211_ie_htcap *)&ni->ni_ie_htcap;
        struct ieee80211_ie_vhtcap *vhtcap =
				(struct ieee80211_ie_vhtcap *)&ni->ni_ie_vhtcap;

	switch (ni->ni_wifi_mode) {
	case IEEE80211_WIFI_MODE_AC:
		vht_mcsmap = IEEE80211_VHTCAP_GET_RX_MCS_NSS(vhtcap);
		for (rx_max = 0; rx_max < IEEE80211_VHTCAP_MCS_MAX; ++rx_max) {
			if (IEEE80211_VHTCAP_GET_MCS_MAP_ENTRY(vht_mcsmap,
					rx_max) == IEEE80211_VHTCAP_MCS_DISABLED)
				break;
		}
		vht_mcsmap = IEEE80211_VHTCAP_GET_TX_MCS_NSS(vhtcap);
		for (tx_max = 0; tx_max < IEEE80211_VHTCAP_MCS_MAX; ++tx_max) {
			if (IEEE80211_VHTCAP_GET_MCS_MAP_ENTRY(vht_mcsmap,
					tx_max) == IEEE80211_VHTCAP_MCS_DISABLED)
				break;
		}
		break;
	case IEEE80211_WIFI_MODE_NA:
	case IEEE80211_WIFI_MODE_NG:
		if (IEEE80211_HT_IS_4SS_NODE(htcap->hc_mcsset)) {
			rx_max = 4;
		} else if (IEEE80211_HT_IS_3SS_NODE(htcap->hc_mcsset)) {
			rx_max = 3;
		} else if (IEEE80211_HT_IS_2SS_NODE(htcap->hc_mcsset)) {
			rx_max = 2;
		}
		if ((IEEE80211_HTCAP_MCS_PARAMS(htcap) &
				IEEE80211_HTCAP_MCS_TX_SET_DEFINED) &&
				(IEEE80211_HTCAP_MCS_PARAMS(htcap) &
				IEEE80211_HTCAP_MCS_TX_RX_SET_NEQ)) {
			tx_max = IEEE80211_HTCAP_MCS_STREAMS(htcap) + 1;
		} else if (IEEE80211_HTCAP_MCS_PARAMS(htcap) &
				IEEE80211_HTCAP_MCS_TX_RX_SET_NEQ) {
			tx_max = 0;
		} else {
			tx_max = rx_max;
		}
		break;
	default:
		/* Non ht mode */
		tx_max = 1;
		rx_max = 1;
		break;
	}

	*tx = tx_max;
	*rx = rx_max;
}

static void get_node_achievable_phyrate_and_bw(struct ieee80211_node *ni,
						uint32_t *tx_rate,
						uint32_t *rx_rate,
						uint8_t *node_bw)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t bw = 0;
	uint8_t assoc_bw = 0;
	uint8_t sgi = 0;
	uint8_t max_mcs = 0;
	uint8_t max_nss = 0;
	uint8_t vht = 0;

	if (ni->ni_wifi_mode != IEEE80211_WIFI_MODE_NG &&
		ni->ni_wifi_mode != IEEE80211_WIFI_MODE_NA &&
		ni->ni_wifi_mode != IEEE80211_WIFI_MODE_AC) {
			*tx_rate =
				(ni->ni_rates.rs_rates[ni->ni_rates.rs_nrates - 1] &
					IEEE80211_RATE_VAL) / 2;
			*rx_rate = *tx_rate;
			*node_bw = 20;
			return;
	}

	if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT)) {
		vht = 1;
		get_node_vht_bw_and_sgi(ni, &bw, &assoc_bw, &sgi);
		get_node_vht_max_nss_mcs(ni, &max_nss, &max_mcs);
	} else {
		get_node_ht_bw_and_sgi(ic, ni, &bw, &assoc_bw, &sgi);
		get_node_ht_max_mcs(ic, ni, &max_mcs);
	}

	*tx_rate = ic->ic_mcs_to_phyrate(bw, sgi, max_mcs, max_nss, vht);
	*rx_rate = *tx_rate;
	*node_bw = assoc_bw;
}

void sample_rel_client_data(struct ieee80211vap *vap)
{
	struct node_client_data *ncd, *ncd_tmp;

	spin_lock(&vap->sample_sta_lock);
	list_for_each_entry_safe(ncd, ncd_tmp, &vap->sample_sta_list, node_list) {
		list_del(&ncd->node_list);
		kfree(ncd);
	}
	vap->sample_sta_count = 0;
	spin_unlock(&vap->sample_sta_lock);
}

static void sample_iterate_client_data(void *s, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = (struct ieee80211vap *)s;
	struct node_client_data *clt = NULL;
	int i;
	struct qtn_node_shared_stats_rx *rx = &ni->ni_shared_stats->rx[STATS_SU];

	/* Skipping other interface node and self vap node */
	if ((ni->ni_vap != vap) ||
		(memcmp(ni->ni_macaddr, vap->iv_myaddr, IEEE80211_ADDR_LEN) == 0)) {
		return;
	}

	MALLOC(clt, struct node_client_data *, sizeof(struct node_client_data),
							M_TEMP, M_WAITOK | M_ZERO);

	if (clt == NULL) {
		printk("Failed to alloc client data\n");
		return;
	}

	IEEE80211_ADDR_COPY(clt->data.mac_addr, ni->ni_macaddr);

	clt->data.assoc_id = IEEE80211_AID(ni->ni_associd);
	clt->data.protocol = ni->ni_wifi_mode;
	clt->data.time_associated = (u_int32_t)div_u64(get_jiffies_64() -
							ni->ni_start_time_assoc, HZ);

	get_node_max_mimo(ni, &clt->data.tx_stream, &clt->data.rx_stream);
	get_node_achievable_phyrate_and_bw(ni, &clt->data.achievable_tx_phy_rate,
						&clt->data.achievable_rx_phy_rate,
						&clt->data.bw);

	clt->data.rx_bytes = ni->ni_stats.ns_rx_bytes;
	clt->data.tx_bytes = ni->ni_stats.ns_tx_bytes;
	clt->data.rx_packets = ni->ni_stats.ns_rx_data;
	clt->data.tx_packets = ni->ni_stats.ns_tx_data;
	clt->data.rx_errors = ni->ni_stats.ns_rx_errors;
	clt->data.tx_errors = ni->ni_stats.ns_tx_errors;
	clt->data.rx_dropped = ni->ni_stats.ns_rx_dropped;
	clt->data.tx_dropped = ni->ni_stats.ns_tx_dropped;
	clt->data.rx_ucast = ni->ni_stats.ns_rx_ucast;
	clt->data.tx_ucast = ni->ni_stats.ns_tx_ucast;
	clt->data.rx_mcast = ni->ni_stats.ns_rx_mcast;
	clt->data.tx_mcast = ni->ni_stats.ns_tx_mcast;
	clt->data.rx_bcast = ni->ni_stats.ns_rx_bcast;
	clt->data.tx_bcast = ni->ni_stats.ns_tx_bcast;
	clt->data.link_quality = (uint16_t) ni->ni_linkqual;
	clt->data.ip_addr = ni->ni_ip_addr;
	clt->data.vendor = ni->ni_vendor;

	for (i = 0; i < WME_AC_NUM; i++)
		clt->data.tx_wifi_drop[i] = ni->ni_stats.ns_tx_wifi_drop[i];

	for (i = 0; i <= NUM_ANT; i++) {
		clt->data.last_rssi_dbm[i] = rx->last_rssi_dbm[i];
		clt->data.last_rcpi_dbm[i] = rx->last_rcpi_dbm[i];
		clt->data.last_evm_dbm[i] = rx->last_evm_dbm[i];
		clt->data.last_hw_noise[i] = rx->last_hw_noise[i];
	}

	spin_lock(&vap->sample_sta_lock);
	list_add(&clt->node_list, &vap->sample_sta_list);
	vap->sample_sta_count++;
	spin_unlock(&vap->sample_sta_lock);
}

static int
ieee80211_subioctl_sample_all_clients(struct ieee80211vap *vap, struct iwreq *iwr)
{
	struct ieee80211com *ic  = vap->iv_ic;
	uint8_t __user *sta_count = iwr->u.data.pointer;

	sample_rel_client_data(vap);
	ic->ic_iterate_nodes(&ic->ic_sta, sample_iterate_client_data, vap, 1);

	if (copy_to_user(sta_count, &vap->sample_sta_count, sizeof(*sta_count)) != 0)
		return -EFAULT;

	return 0;
}

static int
ieee80211_subioctl_get_assoc_data(struct ieee80211vap *vap, void __user *pointer, uint32_t len)
{
	struct node_client_data *ncd;
	int count = 0;
	int u_count = 0;
	int offset;
	int num_entry;
	struct sample_assoc_user_data __user *u_pointer =
					(struct sample_assoc_user_data *)pointer;

	if (copy_from_user(&num_entry, &u_pointer->num_entry, sizeof(num_entry)) != 0)
		return -EFAULT;

	if (copy_from_user(&offset, &u_pointer->offset, sizeof(offset)) != 0)
		return -EFAULT;

	if (vap->sample_sta_count < (num_entry + offset))
		return -EINVAL;

	if (len < (num_entry * sizeof(struct sample_assoc_data)
				+ sizeof(u_pointer->num_entry)
				+ sizeof(u_pointer->offset)))
		return -EINVAL;

	spin_lock(&vap->sample_sta_lock);
	list_for_each_entry(ncd, &vap->sample_sta_list, node_list) {
		if ((count >= offset) && (count < (num_entry + offset))) {
			if (copy_to_user(((u_pointer->data) + (u_count)),
				&(ncd->data), sizeof(struct sample_assoc_data)) != 0) {
				spin_unlock(&vap->sample_sta_lock);
				return -EFAULT;
			}
			u_count++;
		}
		count++;
	}

	spin_unlock(&vap->sample_sta_lock);

	return 0;
}

static void
get_interface_wmmac_stats(void *s, struct ieee80211_node *ni)
{
	struct ieee80211req_interface_wmmac_stats wmmac_stats;
	struct iwreq *iwr = (struct iwreq *)s;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	void __user *pointer;
	int i;

	pointer	= iwr->u.data.pointer;
	if (copy_from_user(&wmmac_stats, pointer, sizeof(wmmac_stats)))
		return;

	if (ic->ic_get_shared_node_stats)
		ic->ic_get_shared_node_stats(ni);

	for (i = 0; i < WMM_AC_NUM; i++) {
		wmmac_stats.tx_wifi_drop[i] += ni->ni_stats.ns_tx_wifi_drop[i];
		wmmac_stats.tx_wifi_sent[i] += ni->ni_shared_stats->tx[STATS_SU].tx_sent_data_msdu[i];
	}

	copy_to_user(pointer, &wmmac_stats, sizeof(wmmac_stats));
}

static int
ieee80211_subioctl_get_interface_wmmac_stats(struct net_device *dev,
					     struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	ic->ic_iterate_dev_nodes(vap->iv_dev, &ic->ic_sta, get_interface_wmmac_stats, iwr, 1);

	return 0;
}

/*
 * This function will be called when processes write following command:
 *  get <unit> assoc_info.
 * into file in sysfs.
 *
 * This function is to report all nodes in the table. User can do further filtering.
 * E.g. user only need MAC address on designated WiFi interface.
 *
 * This function works for both AP and STA.
 */
void
get_node_info(void *s, struct ieee80211_node *ni)
{
	struct seq_file *sq = (struct seq_file *)s;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;

	uint8_t *mac = ni->ni_macaddr;
	uint8_t assoc_bw = 20;
	uint8_t bw = 0;
	uint8_t sgi = 0;
	uint8_t max_mcs = 0;
	uint8_t max_nss = 0;
	uint8_t vht;
	uint32_t achievable_tx_phy_rate;
	uint32_t achievable_rx_phy_rate;  /* Unit: in Kbps */
	uint32_t time_associated = 0;		/* Unit: second, 32bits should be up to 136 years */
	uint32_t combined_ba_state = 0;
	int32_t i;
	uint32_t current_tx_phy_rate = 0;
	uint32_t current_rx_phy_rate = 0;
	int32_t current_rssi = ic->ic_rssi(ni);
	int32_t current_snr = ic->ic_snr(ni);
	int32_t current_max_queue = ic->ic_max_queue(ni);
	uint32_t tx_failed = ic->ic_tx_failed(ni);

	ic->ic_rxtx_phy_rate(ni, 0, NULL, NULL, &current_tx_phy_rate);
	ic->ic_rxtx_phy_rate(ni, 1, NULL, NULL, &current_rx_phy_rate);

	if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT)) {
		vht = 1;
		get_node_vht_bw_and_sgi(ni, &bw, &assoc_bw, &sgi);
	} else {
		vht = 0;
		get_node_ht_bw_and_sgi(ic, ni, &bw, &assoc_bw, &sgi);
	}

	if (ieee80211_node_is_authorized(ni)) {
		if (vht == 0)
			get_node_ht_max_mcs(ic, ni, &max_mcs);
		else
			get_node_vht_max_nss_mcs(ni, &max_nss, &max_mcs);

		achievable_tx_phy_rate = ic->ic_mcs_to_phyrate(bw, sgi, max_mcs, max_nss, vht);
		achievable_rx_phy_rate = achievable_tx_phy_rate;
	} else {
		achievable_tx_phy_rate = 0;
		achievable_rx_phy_rate = 0;
	}

	if (current_rssi < -1 && current_rssi > -1200) {
		ni->ni_rssi = current_rssi;
	} else if (ni->ni_rssi > 0) {
		/* correct pseudo RSSIs that apparently still get into the node table */
		ni->ni_rssi = (ni->ni_rssi * 10) - 900;
	}

	ni->ni_smthd_rssi = ic->ic_smoothed_rssi(ni);
	if ((ni->ni_smthd_rssi > -1) || (ni->ni_smthd_rssi < -1200)) {
		ni->ni_smthd_rssi = ni->ni_rssi;
	}

	if (current_tx_phy_rate > 0) {
		ni->ni_linkqual = (uint16_t) current_tx_phy_rate;
	}

	if (current_rx_phy_rate > 0) {
		ni->ni_rx_phy_rate = (uint16_t) current_rx_phy_rate;
	}

	if (current_snr < -1) {
		ni->ni_snr = current_snr;
	}

	if (current_max_queue > -1) {
		ni->ni_max_queue = current_max_queue;
	}

	if(vap->iv_opmode == IEEE80211_M_STA) {
		time_associated = (vap->iv_state == IEEE80211_S_RUN) ?
				(u_int32_t)div_u64(get_jiffies_64() - ni->ni_start_time_assoc, HZ) : 0;
	} else {
		time_associated = (vap->iv_bss == ni) ?
				0 : (u_int32_t)div_u64(get_jiffies_64() - ni->ni_start_time_assoc, HZ);
	}

	COMPILE_TIME_ASSERT(WME_NUM_TID <= 16);

	for (i = 0; i < WME_NUM_TID; i++) {
		combined_ba_state |= (int) (ni->ni_ba_rx[i].state == IEEE80211_BA_ESTABLISHED)
				<< (WME_NUM_TID + i);
		combined_ba_state |= (int) (ni->ni_ba_tx[i].state == IEEE80211_BA_ESTABLISHED) << i;
	}

	if (ic->ic_get_shared_node_stats)
		ic->ic_get_shared_node_stats(ni);

	if (sq != NULL) {
		/* NOTE: if this output format changes, there are flow-on effects to qcsapi. */
		seq_printf(sq,
			"%02X:%02X:%02X:%02X:%02X:%02X "
			"%4u %3u %10s %5u %5u %6u %6u %5d %5d %10u %12llu %10u %12llu "
			"%5u %5u %5u %5u %7u %7u %7u %7u %7u %7u %5d %6u %2u %8u %4u %08x %s\n",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
			IEEE80211_AID(ni->ni_associd),
			IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx),
			ieee80211_tdls_status_string_get(ni->tdls_status),
			ni->ni_linkqual,
			ni->ni_rx_phy_rate,
			achievable_tx_phy_rate,
			achievable_rx_phy_rate,
			ni->ni_smthd_rssi,
			ni->ni_snr,
			ni->ni_stats.ns_rx_data,
			ni->ni_stats.ns_rx_bytes,
			ni->ni_stats.ns_tx_data,
			ni->ni_stats.ns_tx_bytes,
			ni->ni_stats.ns_rx_errors,
			ni->ni_stats.ns_rx_dropped,
			ni->ni_stats.ns_tx_errors,
			ni->ni_stats.ns_tx_dropped,
			ni->ni_stats.ns_tx_ucast,
			ni->ni_stats.ns_rx_ucast,
			ni->ni_stats.ns_tx_mcast,
			ni->ni_stats.ns_rx_mcast,
			ni->ni_stats.ns_tx_bcast,
			ni->ni_stats.ns_rx_bcast,
			ni->ni_max_queue,
			tx_failed,
			assoc_bw,
			time_associated,
			ieee80211_node_is_authorized(ni),
			combined_ba_state,
			vap->iv_dev->name
		);
	}
}
EXPORT_SYMBOL(get_node_info);

void get_node_assoc_state(void *s, struct ieee80211_node *ni)
{
	struct seq_file *sq = (struct seq_file *)s;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *mac = ni->ni_macaddr;
	uint8_t assoc_bw = 20;
	uint8_t bw = 0;
	uint8_t sgi = 0;
	uint32_t time_associated = 0;	/* Unit: second, 32bits should be up to 136 years */
	uint32_t combined_ba_state = 0;
	uint32_t i;

	const char *wifi_modes_strings[] = WLAN_WIFI_MODES_STRINGS;
	COMPILE_TIME_ASSERT(ARRAY_SIZE(wifi_modes_strings) == IEEE80211_WIFI_MODE_MAX);

	if (!sq)
		return;

	if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT))
		get_node_vht_bw_and_sgi(ni, &bw, &assoc_bw, &sgi);
	else
		get_node_ht_bw_and_sgi(ic, ni, &bw, &assoc_bw, &sgi);

	if (vap->iv_opmode == IEEE80211_M_STA)
		time_associated = (vap->iv_state == IEEE80211_S_RUN) ?
			(u_int32_t)div_u64(get_jiffies_64()-ni->ni_start_time_assoc, HZ) : 0;
	else
		time_associated = (vap->iv_bss == ni) ?
			0 : (u_int32_t)div_u64(get_jiffies_64()-ni->ni_start_time_assoc, HZ);

	COMPILE_TIME_ASSERT(WME_NUM_TID <= 16);

	for (i = 0; i < WME_NUM_TID; i++) {
		combined_ba_state |= (int) (ni->ni_ba_rx[i].state == IEEE80211_BA_ESTABLISHED)
				<< (WME_NUM_TID + i);
		combined_ba_state |= (int) (ni->ni_ba_tx[i].state == IEEE80211_BA_ESTABLISHED) << i;
	}

	seq_printf(sq,
		"%pM %4u %4u %6s %4s %8s %4u %7u %6u   %08x %12s %10s %16u\n",
		mac,
		IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx),
		IEEE80211_AID(ni->ni_associd),
		ieee80211_get_node_type_str(ni->ni_node_type),
		wifi_modes_strings[ni->ni_wifi_mode],
		ieee80211_get_vendor_str(ni->ni_vendor),
		assoc_bw,
		time_associated,
		ieee80211_node_is_authorized(ni),
		combined_ba_state,
		ieee80211_tdls_status_string_get(ni->tdls_status),
		vap->iv_dev->name,
		ieee80211_node_power_save_scheme(ni));
}
EXPORT_SYMBOL(get_node_assoc_state);

void get_node_ver(void *s, struct ieee80211_node *ni)
{
	struct seq_file *sq = (struct seq_file *)s;
#define IEEE80211_VER_SW_STR_LEN 15
	char sw_str[IEEE80211_VER_SW_STR_LEN];
	struct ieee80211com *ic = ni->ni_ic;
	uint32_t *ver_sw;
	uint16_t ver_platform_id;
	uint16_t ver_hw;
	uint32_t timestamp;
	uint32_t flags;

	if (sq == NULL) {
		return;
	}

	if (IEEE80211_AID(ni->ni_associd) == 0) {
		ver_sw = &ic->ic_ver_sw;
		ver_platform_id = ic->ic_ver_platform_id;
		ver_hw = ic->ic_ver_hw;
		timestamp = ic->ic_ver_timestamp;
		flags = ic->ic_ver_flags;
	} else {
		ver_sw = &ni->ni_ver_sw;
		ver_platform_id = ni->ni_ver_platform_id;
		ver_hw = ni->ni_ver_hw;
		timestamp = ni->ni_ver_timestamp;
		flags = ni->ni_ver_flags;
	}

	snprintf(sw_str, sizeof(sw_str),
		DBGFMT_BYTEFLD4_P,
		DBGFMT_BYTEFLD4_V(*ver_sw));

	seq_printf(sq, "%pM %4u %-*s %-8u %-6u %-10u 0x%08x\n",
		ni->ni_macaddr,
		IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx),
		IEEE80211_VER_SW_STR_LEN, *ver_sw ? sw_str : "-",
		ver_platform_id,
		ver_hw,
		timestamp,
		flags);
}
EXPORT_SYMBOL(get_node_ver);

void
ieee80211_update_node_assoc_qual(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t current_tx_phy_rate = 0;
	int32_t current_rssi = ic->ic_rssi(ni);
	int32_t current_snr = ic->ic_snr(ni);

	ic->ic_rxtx_phy_rate(ni, 0, NULL, NULL, &current_tx_phy_rate);

	if (current_rssi < -1 && current_rssi > -1200)
		ni->ni_rssi = current_rssi;
	else if (ni->ni_rssi > 0)
		/* correct pseudo RSSIs that apparently still get into the node table */
		ni->ni_rssi = (ni->ni_rssi * 10) - 900;

	ni->ni_smthd_rssi = ic->ic_smoothed_rssi(ni);
	if ((ni->ni_smthd_rssi > -1) || (ni->ni_smthd_rssi < -1200))
		ni->ni_smthd_rssi = ni->ni_rssi;

	if (current_tx_phy_rate > 0)
		ni->ni_linkqual = (uint16_t) current_tx_phy_rate;

	if (current_snr < -1)
		ni->ni_snr = current_snr;
}
EXPORT_SYMBOL(ieee80211_update_node_assoc_qual);

void
get_node_tx_stats(void *s, struct ieee80211_node *ni)
{
	struct seq_file *sq = (struct seq_file *)s;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *mac = ni->ni_macaddr;
	int32_t current_max_queue = ic->ic_max_queue(ni);
	uint32_t tx_failed = ic->ic_tx_failed(ni);
	uint32_t achievable_tx_phy_rate; /* Kbps */
	uint8_t assoc_bw = 20;
	uint8_t bw = 0;
	uint8_t sgi = 0;
	uint8_t max_mcs = 0;
	uint8_t max_nss = 0;
	uint8_t vht;

	if (!sq)
		return;

	if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT)) {
		vht = 1;
		get_node_vht_bw_and_sgi(ni, &bw, &assoc_bw, &sgi);
	} else {
		vht = 0;
		get_node_ht_bw_and_sgi(ic, ni, &bw, &assoc_bw, &sgi);
	}

	if (ieee80211_node_is_authorized(ni)) {
		if (vht == 0)
			get_node_ht_max_mcs(ic, ni, &max_mcs);
		else
			get_node_vht_max_nss_mcs(ni, &max_nss, &max_mcs);

		achievable_tx_phy_rate = ic->ic_mcs_to_phyrate(bw, sgi, max_mcs, max_nss, vht);
	} else {
		achievable_tx_phy_rate = 0;
	}

	if (current_max_queue > -1)
		ni->ni_max_queue = current_max_queue;

	seq_printf(sq,
		"%pM %4u %8u %10u %12llu %8u %8u %10u %10u %10u %5u %7u\n",
		mac,
		IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx),
		achievable_tx_phy_rate,
		ni->ni_stats.ns_tx_data,
		ni->ni_stats.ns_tx_bytes,
		ni->ni_stats.ns_tx_errors,
		ni->ni_stats.ns_tx_dropped,
		ni->ni_stats.ns_tx_ucast,
		ni->ni_stats.ns_tx_mcast,
		ni->ni_stats.ns_tx_bcast,
		ni->ni_max_queue,
		tx_failed);
}
EXPORT_SYMBOL(get_node_tx_stats);

void
get_node_rx_stats(void *s, struct ieee80211_node *ni)
{
	struct seq_file *sq = (struct seq_file *)s;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint8_t *mac = ni->ni_macaddr;
	uint32_t achievable_rx_phy_rate; /* Kbps */
	uint32_t current_rx_phy_rate = 0;
	uint8_t assoc_bw = 20;
	uint8_t bw = 0;
	uint8_t sgi = 0;
	uint8_t max_mcs = 0;
	uint8_t max_nss = 0;
	uint8_t vht;

	if (!sq)
		return;

	ic->ic_rxtx_phy_rate(ni, 1, NULL, NULL, &current_rx_phy_rate);

	if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT)) {
		vht = 1;
		get_node_vht_bw_and_sgi(ni, &bw, &assoc_bw, &sgi);
	} else {
		vht = 0;
		get_node_ht_bw_and_sgi(ic, ni, &bw, &assoc_bw, &sgi);
	}

	if (ieee80211_node_is_authorized(ni)) {
		if (vht == 0)
			get_node_ht_max_mcs(ic, ni, &max_mcs);
		else
			get_node_vht_max_nss_mcs(ni, &max_nss, &max_mcs);

		achievable_rx_phy_rate = ic->ic_mcs_to_phyrate(bw, sgi, max_mcs, max_nss, vht);
	} else {
		achievable_rx_phy_rate = 0;
	}

	if (current_rx_phy_rate > 0)
		ni->ni_rx_phy_rate = (uint16_t) current_rx_phy_rate;

	seq_printf(sq,
		"%pM %4u %8d %8d %10u %12llu %8u %8u %10u %10u %10u\n",
		mac,
		IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx),
		achievable_rx_phy_rate,
		ni->ni_rx_phy_rate,
		ni->ni_stats.ns_rx_data,
		ni->ni_stats.ns_rx_bytes,
		ni->ni_stats.ns_rx_errors,
		ni->ni_stats.ns_rx_dropped,
		ni->ni_stats.ns_rx_ucast,
		ni->ni_stats.ns_rx_mcast,
		ni->ni_stats.ns_rx_bcast);
}
EXPORT_SYMBOL(get_node_rx_stats);

static void
get_node_max_rssi (void *arg, struct ieee80211_node *ni) {

	int32_t *max_rssi = (int32_t *)arg;
	int32_t cur_rssi = (ni->ni_ic->ic_rssi(ni)/IEEE80211_RSSI_FACTOR);

	if (cur_rssi != 0 &&
		(ni->ni_associd != 0) &&
		(ni->ni_vendor == PEER_VENDOR_BRCM) &&
		(cur_rssi > *max_rssi)) {
		*max_rssi = cur_rssi;
	}
}

static void
ieee80211_pco_timer_func ( unsigned long arg ) {
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;
	struct ieee80211com *ic = vap->iv_ic;
	uint16_t pwr_constraint = ic->ic_pco.pco_pwr_constraint;
	uint8_t rssi_threshold = ic->ic_pco.pco_rssi_threshold;
	uint16_t pwr_constraint_sec = (ic->ic_pco.pco_pwr_constraint > ic->ic_pco.pco_sec_offset) ? (ic->ic_pco.pco_pwr_constraint-ic->ic_pco.pco_sec_offset):0;
	uint8_t rssi_threshold_sec = ic->ic_pco.pco_rssi_threshold+ic->ic_pco.pco_sec_offset;
	uint8_t next_update = 1;
	int32_t max_rssi = -100;

	// Apply WAR for single STA only
	if( ic->ic_sta_assoc == 1 ) {
		// check for brcm node with RSSI > rssi_threshold
		ieee80211_iterate_nodes(&ic->ic_sta, get_node_max_rssi, &max_rssi, 1);
	}

	/*
	 * Apply contraint WAR for higher channels only.
	 * The backoff dB value comparing with max reg tx power is a safety check
	 * to make sure the STAs local_tx_power won't be less than 0dBm
	 */
	if ((max_rssi > -rssi_threshold) && !(ic->ic_pco.pco_set) &&
		(pwr_constraint < ic->ic_bsschan->ic_maxregpower) &&
		(ic->ic_curchan->ic_ieee > QTN_5G_LAST_UNII2_OPERATING_CHAN)) {
		/* Back down the STA power using Power constraint */
		ic->ic_pwr_constraint = pwr_constraint;
		if (vap->iv_state == IEEE80211_S_RUN)
			ic->ic_beacon_update(vap);
		ic->ic_pco.pco_set = 1;
		next_update = 60;
	} else if ((max_rssi > -rssi_threshold_sec) && !(ic->ic_pco.pco_set) &&
		(pwr_constraint_sec < ic->ic_bsschan->ic_maxregpower) &&
		(ic->ic_curchan->ic_ieee > QTN_5G_LAST_UNII2_OPERATING_CHAN)) {
		/* Back down the STA power using Secondary Power constraint */
		ic->ic_pwr_constraint = pwr_constraint_sec;
		if (vap->iv_state == IEEE80211_S_RUN)
			ic->ic_beacon_update(vap);
		ic->ic_pco.pco_set = 1;
		next_update = 60;
	} else if ((ic->ic_pco.pco_set) && (max_rssi <= -rssi_threshold_sec)) {
		ic->ic_pwr_constraint = ic->ic_pco.pco_pwr_constraint_save;
		if (vap->iv_state == IEEE80211_S_RUN)
			ic->ic_beacon_update(vap);
		ic->ic_pco.pco_set = 0;
		next_update = 1;
	}

	mod_timer(&ic->ic_pco.pco_timer,
		jiffies + (next_update * HZ));
}
/*
 * Implementation of ioctl command: IEEE80211_IOCTL_GET_ASSOC_TBL
 * This command is to report all nodes in the table. User can do further filtering nodes.
 * E.g. user only need MAC address on designated WiFi interface.
 *
 * This function works for both AP and STA.
 */
static void get_node_info_ioctl(void *s, struct ieee80211_node *ni)
{
	struct iwreq *iwr = (struct iwreq *)s;
	struct assoc_info_report __user	*u_record;
	struct assoc_info_report record;
	uint16_t __user	*u_cnt;
	uint16_t cnt;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic	= vap->iv_ic;
	uint8_t *mac = ni->ni_macaddr;
	uint8_t assoc_bw = 20;
	uint8_t bw = 0;
	uint8_t sgi = 0;
	uint8_t max_mcs = 0;
	uint8_t max_nss = 0;
	uint8_t tx_mcs = 0;
	uint8_t rx_mcs = 0;
	uint8_t vht;
	uint32_t max_tx_rate;
	uint32_t max_rx_rate;		/* Unit: Kbps */
	uint32_t assoc_time = 0;		/* Unit: second, 32bits should be up to 136 years */
	uint32_t cur_tx_rate = 0;
	uint32_t cur_rx_rate = 0;
	int32_t rssi = ic->ic_rssi(ni);
	int32_t snr = ic->ic_snr(ni);
	int32_t max_queue = ic->ic_max_queue(ni);
	uint32_t tx_failed = ic->ic_tx_failed(ni);
	int i;

	ni->ni_hw_noise = ic->ic_hw_noise(ni);
	ic->ic_rxtx_phy_rate(ni, 0, NULL, &tx_mcs, &cur_tx_rate);
	ic->ic_rxtx_phy_rate(ni, 1, NULL, &rx_mcs, &cur_rx_rate);

	if (IS_IEEE80211_VHT_ENABLED(ic) && (ni->ni_flags & IEEE80211_NODE_VHT)) {
		vht = 1;
		get_node_vht_bw_and_sgi(ni, &bw, &assoc_bw, &sgi);
	} else {
		vht = 0;
		get_node_ht_bw_and_sgi(ic, ni, &bw, &assoc_bw, &sgi);
	}

	if (ieee80211_node_is_authorized(ni)) {
		if (vht == 0)
			get_node_ht_max_mcs(ic, ni, &max_mcs);
		else
			get_node_vht_max_nss_mcs(ni, &max_nss, &max_mcs);

		max_tx_rate = ic->ic_mcs_to_phyrate(bw, sgi, max_mcs, max_nss, vht);
		max_rx_rate = max_tx_rate;
	} else {
		max_tx_rate = 0;
		max_rx_rate = 0;
	}

	if (rssi < -1 && rssi > -1200)
		ni->ni_rssi = rssi;
	else if (ni->ni_rssi > 0) {
		/* Correct pseudo RSSIs that apparently still get into the node table */
		ni->ni_rssi = (ni->ni_rssi * 10) - 900;
	}

	ni->ni_smthd_rssi = ic->ic_smoothed_rssi(ni);
	if ((ni->ni_smthd_rssi > -1) || (ni->ni_smthd_rssi < -1200)) {
		ni->ni_smthd_rssi = ni->ni_rssi;
	}

	if (cur_tx_rate > 0)
		ni->ni_linkqual    = (uint16_t) cur_tx_rate;

	if (cur_rx_rate > 0)
		ni->ni_rx_phy_rate = (uint16_t) cur_rx_rate;

	if (snr < -1)
		ni->ni_snr	   = snr;

	if (max_queue > -1)
		ni->ni_max_queue   = max_queue;

	if (vap->iv_opmode == IEEE80211_M_STA) {
		assoc_time = (vap->iv_state == IEEE80211_S_RUN) ?
			(u_int32_t)div_u64(get_jiffies_64() - ni->ni_start_time_assoc, HZ) : 0;
	} else {
		assoc_time = (vap->iv_bss == ni) ?
			0 : (u_int32_t)div_u64(get_jiffies_64() - ni->ni_start_time_assoc, HZ);
	}

	if (ic->ic_get_shared_node_stats)
		ic->ic_get_shared_node_stats(ni);

	memcpy(record.ai_mac_addr, mac, IEEE80211_ADDR_LEN);

	record.ai_assoc_id		= IEEE80211_AID(ni->ni_associd);
	record.ai_link_quality		= (uint16_t) ni->ni_linkqual;
	record.ai_rx_phy_rate		= ni->ni_rx_phy_rate;
	record.ai_tx_phy_rate		= (uint16_t) ni->ni_linkqual;
	record.ai_achievable_tx_phy_rate = max_tx_rate;
	record.ai_achievable_rx_phy_rate = max_rx_rate;
	record.ai_rssi			= ni->ni_rssi;
	record.ai_smthd_rssi		= ni->ni_smthd_rssi;
	record.ai_snr			= ni->ni_snr;
	record.ai_rx_packets		= ni->ni_stats.ns_rx_data;
	record.ai_rx_bytes		= ni->ni_stats.ns_rx_bytes;
	record.ai_tx_packets		= ni->ni_stats.ns_tx_data;
	record.ai_tx_bytes		= ni->ni_stats.ns_tx_bytes;
	record.ai_rx_errors		= ni->ni_stats.ns_rx_errors;
	record.ai_rx_dropped		= ni->ni_stats.ns_rx_dropped;
	record.ai_tx_ucast		= ni->ni_stats.ns_tx_ucast;
	record.ai_rx_ucast		= ni->ni_stats.ns_rx_ucast;
	record.ai_tx_mcast		= ni->ni_stats.ns_tx_mcast;
	record.ai_rx_mcast		= ni->ni_stats.ns_rx_mcast;
	record.ai_tx_bcast		= ni->ni_stats.ns_tx_bcast;
	record.ai_rx_bcast		= ni->ni_stats.ns_rx_bcast;
	record.ai_tx_errors		= ni->ni_stats.ns_tx_errors;
	record.ai_tx_dropped		= ni->ni_stats.ns_tx_dropped;
	for (i = 0; i < WME_AC_NUM; i++)
		record.ai_tx_wifi_drop[i] = ni->ni_stats.ns_tx_wifi_drop[i];
	record.ai_rx_fragment_pkts	= ni->ni_stats.ns_rx_fragment_pkts;
	record.ai_rx_vlan_pkts		= ni->ni_stats.ns_rx_vlan_pkts;
	record.ai_max_queued		= ni->ni_max_queue;
	record.ai_tx_failed		= tx_failed;
	record.ai_bw			= assoc_bw;
	record.ai_tx_mcs		= tx_mcs;
	record.ai_rx_mcs		= rx_mcs;
	record.ai_time_associated	= assoc_time;
	record.ai_auth			= ieee80211_node_is_authorized(ni);
	record.ai_ip_addr		= ni->ni_ip_addr;
	record.ai_hw_noise		= ni->ni_hw_noise;
	record.ai_is_qtn_node		= (ni->ni_qtn_assoc_ie) ? 1 : 0;

	strncpy(record.ai_ifname, vap->iv_dev->name, strlen(vap->iv_dev->name) + 1);

	/* Update record to user space  */
	u_cnt = &(( (struct assoc_info_table *)iwr->u.data.pointer )->cnt);
	if (copy_from_user(&cnt, u_cnt, sizeof(cnt)))
		return;

	u_record = ( (struct assoc_info_table *)iwr->u.data.pointer )->array + cnt;

	if (cnt++ > QTN_ASSOC_LIMIT - 1)
		return;

	if (copy_to_user(u_record, &record, sizeof(record)))
		return;

	copy_to_user( u_cnt, &cnt, sizeof(cnt) );
}

static int
ieee80211_ioctl_getassoctbl(struct net_device *dev, struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic  = vap->iv_ic;
	uint16_t	   unit_size = ( (struct assoc_info_table *)iwr->u.data.pointer )->unit_size;

	if (unit_size != sizeof( struct assoc_info_report )) {
		printk(KERN_ERR "The size of structure assoc_info_report doesn't match\n");
		return -EPERM;
	}

	ic->ic_iterate_nodes(&ic->ic_sta, get_node_info_ioctl, iwr, 1);

	return 0;
}

static int
ieee80211_subioctl_rst_queue(struct net_device *dev, char __user* mac)
{
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	struct ieee80211_node	*ni  = NULL;
	uint8_t		mac_addr[IEEE80211_ADDR_LEN];

	if (copy_from_user(mac_addr, mac, IEEE80211_ADDR_LEN))
		return -EFAULT;

	ni = ieee80211_find_node(&ic->ic_sta, mac_addr);
	if (ni == NULL)
		return -ENOENT;

	ic->ic_queue_reset(ni);

	ieee80211_free_node(ni);

	return 0;
}

static int
ieee80211_subioctl_radar_status(struct net_device *dev, struct ieee80211req_radar_status __user* status)
{
	int retval = 0;
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211req_radar_status rdstatus;
	int chan_idx=0;

	if (copy_from_user(&rdstatus, status, sizeof(rdstatus)) != 0)
		return -EFAULT;

	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		if (ic->ic_channels[chan_idx].ic_ieee == rdstatus.channel)
			break;
	}

	if (chan_idx >= ic->ic_nchans) {
		retval = -EINVAL;
	} else {
		if (!(ic->ic_channels[chan_idx].ic_flags & IEEE80211_CHAN_DFS))
			retval = -EINVAL;
	}

	if (retval >= 0) {
		rdstatus.flags = ((ic->ic_channels[chan_idx].ic_flags) & (IEEE80211_CHAN_RADAR)) ? 1 : 0;
		rdstatus.ic_radardetected = ic->ic_channels[chan_idx].ic_radardetected;
		if (copy_to_user(status, &rdstatus, sizeof(rdstatus)) != 0)
			retval = -EFAULT;
	}

	return retval;
}

static int
ieee80211_subioctl_get_phy_stats(struct net_device *dev, struct ieee80211_phy_stats __user* ps)
{
	int retval = 0;
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	struct ieee80211_phy_stats phy_stats;

	if (ic->ic_get_phy_stats) {
		retval = ic->ic_get_phy_stats(dev, ic, &phy_stats, 1);
	} else
		return -EINVAL;

	if (retval >= 0) {
		if (copy_to_user(ps, &phy_stats, sizeof(struct ieee80211_phy_stats)))
			retval = -EFAULT;
	}

	return retval;
}

static int
ieee80211_subioctl_get_dscp2ac_map(struct net_device *dev, uint8_t __user* ps)
{
	int retval = 0;
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic  = vap->iv_ic;
	uint8_t dscp2ac_map[IP_DSCP_NUM] = {0};
	uint8_t vap_idx = ic->ic_get_vap_idx(vap);

	if (!ic->ic_get_dscp2ac_map)
		return -EINVAL;

	ic->ic_get_dscp2ac_map(vap_idx, dscp2ac_map);

	if (copy_to_user(ps, dscp2ac_map, IP_DSCP_NUM) != 0)
		retval = -EIO;

	return retval;
}

static int
ieee80211_subioctl_set_dscp2ac_map(struct net_device *dev, struct ieeee80211_dscp2ac __user* ps)
{
	int retval = 0;
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic  = vap->iv_ic;
	struct ieeee80211_dscp2ac dscp2ac;
	uint8_t vap_idx = ic->ic_get_vap_idx(vap);

	if (!ps) {
		return -EFAULT;
	}

	if (copy_from_user(&dscp2ac, ps, sizeof(struct ieeee80211_dscp2ac))) {
		return -EIO;
	}

	if (dscp2ac.list_len > IP_DSCP_NUM) {
		printk(KERN_WARNING "%s: DSCP list size %u larger then max allowed %d\n",
			 dev->name, dscp2ac.list_len, IP_DSCP_NUM);
		return -EINVAL;
	}

	if (ic->ic_set_dscp2ac_map) {
		ic->ic_set_dscp2ac_map(vap_idx, dscp2ac.dscp, dscp2ac.list_len, dscp2ac.ac);
	} else
		return -EINVAL;

	return retval;
}


static int
ieee80211_subioctl_brcm(struct net_device *dev, struct ieee80211req_brcm __user* ps)
{
	int retval = 0;
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	struct ieee80211req_brcm req;
	struct ieee80211_node *ni;

	if (copy_from_user(&req, ps, sizeof(struct ieee80211req_brcm))) {
		return -EINVAL;
	};

	switch (req.ib_op) {
	case IEEE80211REQ_BRCM_INFO:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;
		ni = ieee80211_find_node(&ic->ic_sta, req.ib_macaddr);
		if (ni == NULL)
			return -ENOENT;
		ieee80211_scs_brcm_info_report(ic, ni, req.ib_rssi, req.ib_rxglitch);
		ieee80211_free_node(ni);
		break;
	case IEEE80211REQ_BRCM_PKT:
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			return -EINVAL;
		ieee80211_send_usr_l2_pkt(vap, req.ib_pkt, req.ib_pkt_len);
		SCSDBG(SCSLOG_INFO, "send brcm info pkt in vap %u\n", vap->iv_unit);
		break;
	default:
		return -EINVAL;
	}

	return retval;
}

static int
ieee80211_subioctl_disconn_info(struct net_device *dev, struct ieee80211req_disconn_info __user* disconn_info)
{
	int retval = 0;
	struct ieee80211req_disconn_info info;
	struct ieee80211vap	*vap = netdev_priv(dev);

	if (copy_from_user(&info, disconn_info, sizeof(info)) != 0)
		return -EFAULT;

	if (info.resetflag) {
		vap->iv_disconn_cnt = 0;
		vap->iv_disconn_seq = 0;
	} else {
		vap->iv_disconn_seq++;
		if (vap->iv_opmode == IEEE80211_M_STA) {
			info.asso_sta_count = (vap->iv_state == IEEE80211_S_RUN) ? 1 : 0;
		} else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			info.asso_sta_count = vap->iv_sta_assoc;
		}

		info.disconn_count = vap->iv_disconn_cnt;
		info.sequence = vap->iv_disconn_seq;
		info.up_time = (jiffies - INITIAL_JIFFIES) / HZ;

		if (copy_to_user(disconn_info, &info, sizeof(info)) != 0)
			retval = -EFAULT;
	}

	return retval;
}

static int
ieee80211_subioctl_tdls_operation(struct net_device *dev, char __user* data)
{
	struct ieee80211vap	*vap = netdev_priv(dev);
	struct ieee80211com	*ic  = vap->iv_ic;
	struct ieee80211_node *peer_ni  = NULL;
	struct ieee80211_tdls_oper_data oper_data;
	uint64_t tbtt;
	uint64_t cur_tsf;
	unsigned long duration;
	unsigned long cur_jiffies;

	if (copy_from_user(&oper_data.dest_mac, data, IEEE80211_ADDR_LEN))
		return -EFAULT;
	if (copy_from_user(&oper_data.oper, data + IEEE80211_ADDR_LEN, sizeof(oper_data.oper)))
		return -EFAULT;

	if((oper_data.oper != IEEE80211_TDLS_ENABLE) && (oper_data.oper != IEEE80211_TDLS_DISABLE)) {
		peer_ni = ieee80211_find_node(&ic->ic_sta, oper_data.dest_mac);
		if (peer_ni == NULL || IEEE80211_NODE_IS_NONE_TDLS(peer_ni)) {
			if (peer_ni)
			      ieee80211_free_node(peer_ni);
			return -ENOENT;
		}
	}

	switch (oper_data.oper) {
	case IEEE80211_TDLS_SETUP:
		if (IEEE80211_NODE_IS_TDLS_INACTIVE(peer_ni) ||
				IEEE80211_NODE_IS_TDLS_IDLE(peer_ni)) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: setting up data link with peer %pM\n",
				__func__, peer_ni->ni_macaddr);
		}
		break;
	case IEEE80211_TDLS_TEARDOWN:
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: tearing down data link with peer %pM\n",
			__func__, peer_ni->ni_macaddr);
		/*
		 * Just remove the fdb entry form bridge module,
		 * delay the node free in later node_expire timer callback.
		 */
		ieee80211_tdls_disable_peer_link(peer_ni);

		if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED)
			ieee80211_tdls_node_leave(vap, peer_ni);
		break;
	case IEEE80211_TDLS_ENABLE_LINK:
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: data link established successfully with peer %pM\n",
			__func__, peer_ni->ni_macaddr);

		ieee80211_tdls_enable_peer_link(vap, peer_ni);
		break;
	case IEEE80211_TDLS_DISABLE_LINK:
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
			"TDLS %s: data link removed with peer %pM\n",
			__func__, peer_ni->ni_macaddr);

		/*
		 * Just remove the fdb entry form bridge module,
		 * delay the node free in later node_expire timer callback.
		 */
		ieee80211_tdls_disable_peer_link(peer_ni);

		if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED)
			ieee80211_tdls_node_leave(vap, peer_ni);
		break;
	case IEEE80211_TDLS_ENABLE:
		if (!ieee80211_swfeat_is_supported(SWFEAT_ID_TDLS, 1))
			return -EPERM;
		if (vap->iv_flags_ext & IEEE80211_FEXT_TDLS_PROHIB) {
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: TDLS function is enabled\n", __func__);

			/* attention: must set the flag firstly */
			vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_PROHIB;
			if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0) {
				ieee80211_tdls_start_disc_timer(vap);
				ieee80211_tdls_start_node_expire_timer(vap);
			}
		}
		break;
	case IEEE80211_TDLS_DISABLE:
		if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_PROHIB) == 0) {
			if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0) {
				IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
					"TDLS %s: TDLS function is disabled\n", __func__);

				/* teardown the link and clear timer */
				ieee80211_tdls_teardown_all_link(vap);
				ieee80211_tdls_clear_disc_timer(vap);
				ieee80211_tdls_clear_node_expire_timer(vap);
				ieee80211_tdls_free_all_inactive_peers(vap);
			}
			vap->iv_flags_ext |= IEEE80211_FEXT_TDLS_PROHIB;
		}
		break;
	case IEEE80211_TDLS_SWITCH_CHAN:
		ic->ic_get_tsf(&cur_tsf);
		cur_jiffies = jiffies;
		tbtt = vap->iv_bss->ni_shared_stats->beacon_tbtt;

		if (tbtt > cur_tsf) {
			duration = IEEE80211_USEC_TO_MS((uint32_t)(tbtt - cur_tsf));
		} else {
			duration = 0;
			IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_WARN,
				"TDLS %s: Get wrong TBTT\n", __func__);
		}

		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
				"TDLS %s: cur_tsf = %016llx, tbtt = %016llx, duration = %08lx\n",
				__func__, cur_tsf, tbtt, duration);

		while (time_before(jiffies, cur_jiffies + duration * HZ / 1000))
			msleep(5);
		ieee80211_tdls_start_channel_switch(vap, peer_ni);
		break;
	default:
		break;
	}

	if (peer_ni) {
		IEEE80211_TDLS_DPRINTF(vap, IEEE80211_MSG_TDLS, IEEE80211_TDLS_MSG_DBG,
			"TDLS %s: TDLS operation: %d, TDLS status: %d\n",
			__func__, oper_data.oper, peer_ni->tdls_status);
		ieee80211_free_node(peer_ni);
	}

	return 0;
}

/*  app_buf = [struct app_action_frm_buf] + [Action Frame Payload]
*   Action Frame Payload = IEEE80211_ACTION_CAT_* (u8) + action (u8) + dialog token (u8) +
*                  status code (u8) + Info
*   Action Frame Payload is constructed in hostapd
*/
static int
ieee80211_subioctl_send_action_frame(struct net_device *dev, u8 *app_buf,
					u32 app_buf_len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	struct app_action_frame_buf *app_action_frm_buf;
	struct ieee80211_node *ni = NULL;
	struct ieee80211_action_data action_data;
	int retval = 0;

	/* kzalloc() alloctes memory to copy data from user. */
	app_action_frm_buf = (struct app_action_frame_buf *) kzalloc(app_buf_len, GFP_KERNEL);
	if (!app_action_frm_buf)
		return -ENOMEM;

	if (copy_from_user(app_action_frm_buf, app_buf, app_buf_len) != 0) {
		retval = -EFAULT;
		goto buf_free;
	}

	memset(&action_data, 0, sizeof(action_data));
	action_data.cat = app_action_frm_buf->cat;
	action_data.action = app_action_frm_buf->action;

	/* Pointer to action frm payload passed to next function */
	action_data.params = &app_action_frm_buf->frm_payload;

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		ni = ieee80211_find_node(&ic->ic_sta, app_action_frm_buf->dst_mac_addr);

		/* Public action frames are handled before association */
		if (ni == NULL && action_data.cat == IEEE80211_ACTION_CAT_PUBLIC) {
			ni = ieee80211_tmp_node(vap, app_action_frm_buf->dst_mac_addr);
		}

		if (ni == NULL) {
			retval = -ENOENT;
			goto buf_free;
		}

		if (action_data.cat == IEEE80211_ACTION_CAT_PUBLIC ||
				(ni->ni_associd && ieee80211_node_is_authorized(ni)))
			IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
		else
			retval = -ENOENT;

		ieee80211_free_node(ni);
	} else {
		/* Currently sending action frames for AP mode only */
		retval = -EINVAL;
	}

buf_free:
	kfree(app_action_frm_buf);

	return retval;
}

/*
 * Function to get the driver capabilities. Currently extended
 * capabilities IE is sent here
 **/
static int
ieee80211_subioctl_get_driver_capa(struct net_device *dev,
					uint8_t __user *app_buf,
					uint32_t app_buf_len)
{
#define DRV_EXT_CAPABILITY_LEN 8
	u_int8_t ext_capability[DRV_EXT_CAPABILITY_LEN] = {0};
	u_int8_t *buf = NULL;
	u_int8_t *pos;
	u_int32_t len = 0;
	int retval = 0;

	MALLOC(buf, u_int8_t *, app_buf_len, M_DEVBUF, M_WAITOK);
	if(!buf)
		return -ENOMEM;

	/*Keep buffer in the form of Type(4 bytes):Length:Value format */

	pos = buf;
	pos += sizeof(u_int32_t); /* total data len in buf */

	*pos++ = IEEE80211_ELEMID_EXTCAP;
	*pos++ = DRV_EXT_CAPABILITY_LEN;

	ext_capability[0] = IEEE80211_EXTCAP_20_40_COEXISTENCE;
	/* max msdu in amsdu 0 = unlimited */
	ext_capability[7] = IEEE80211_EXTCAP_OPMODE_NOTIFICATION;

	/* extended capability supported by driver */
	memcpy(pos, ext_capability, DRV_EXT_CAPABILITY_LEN);
	pos +=  DRV_EXT_CAPABILITY_LEN;

	/* extended capability mask */
	memcpy(pos, ext_capability, DRV_EXT_CAPABILITY_LEN);
	pos +=  DRV_EXT_CAPABILITY_LEN;

	/* TODO Can send more data to application from here */

	len = (u_int32_t)(pos - buf);
	*(u_int32_t *)buf = len;

	if (len > app_buf_len) {
		printk (KERN_WARNING "length is more than app_buf_len\n");
		retval = -1;
		goto done;
	}

	if (copy_to_user(app_buf, buf, len)) {
		retval = -EIO;
	}

done:
	FREE(buf, M_DEVBUF);

	return retval;

#undef DRV_EXT_CAPABILITY_LEN
}

struct get_links_max_quality_request {
	uint32_t max_quality;
	uint8_t mac_addr[IEEE80211_ADDR_LEN];
};

static void get_link_quality_max_callback(void *s, struct ieee80211_node *ni)
{
	struct get_links_max_quality_request* cmd
		= (struct get_links_max_quality_request*)s;

	if (ieee80211_node_is_authorized(ni) != 0 &&
			IEEE80211_AID(ni->ni_associd) != 0 &&
			!IEEE80211_ADDR_EQ(ni->ni_macaddr, cmd->mac_addr)) {
		cmd->max_quality = MAX(cmd->max_quality, (uint32_t)ni->ni_linkqual);
	}
}

static int
ieee80211_subioctl_get_link_quality_max(struct net_device *dev, uint8_t __user *app_buf,
	uint32_t app_buf_len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic  = vap->iv_ic;
	struct get_links_max_quality_request request;

	if (app_buf_len != sizeof(request.max_quality))
		return -EINVAL;

	IEEE80211_ADDR_COPY(request.mac_addr, dev->dev_addr);
	request.max_quality = 0;

	ic->ic_iterate_dev_nodes(dev, &ic->ic_sta, get_link_quality_max_callback, &request, 1);

	if (copy_to_user(app_buf, &request.max_quality, sizeof(request.max_quality)))
		return -EINVAL;

	return 0;
}

static int
ieee80211_subioctl_set_ap_info(struct net_device *dev, uint8_t __user* app_buf,
						uint32_t app_buf_len)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct app_ie *ie = NULL;
	int retval = 0;

	if (app_buf_len < 4)
		return -EINVAL;

	MALLOC(ie, struct app_ie *, app_buf_len, M_DEVBUF, M_WAITOK);
	if (!ie)
		return -ENOMEM;

	if (copy_from_user(ie, app_buf, app_buf_len) != 0) {
		retval = -EFAULT;
		goto buf_free;
	}

	/*TODO: This function can be used to handle other Elements from hostapd */
	switch (ie->id) {
	case IEEE80211_ELEMID_INTERWORKING:
		memset(&vap->interw_info, 0, sizeof(struct interworking_info));
		vap->interworking = ie->u.interw.interworking;
		if (vap->interworking) {
			vap->interw_info.an_type = ie->u.interw.an_type;
			if (ie->len > 2) {
				IEEE80211_ADDR_COPY(vap->interw_info.hessid,
							ie->u.interw.hessid);
			}
		}
		break;
	default:
		retval = -EOPNOTSUPP;
		break;
	}

buf_free:
	FREE(ie, M_DEVBUF);
	return retval;
}

static int
ieee80211_get_supp_chans(struct ieee80211vap *vap, struct iwreq *iwr)
{
	int8_t mac_addr[IEEE80211_ADDR_LEN];
	struct ieee80211_node *ni;
	const char *errmsg = " [buffer overflow]";
	void __user* pointer;
	char *buffer;
	char *tmp_buf;
	int buf_len;
	int chan;
	int len;
	int retval;

	pointer = iwr->u.data.pointer;
	if (copy_from_user(mac_addr, pointer, IEEE80211_ADDR_LEN) != 0)
		return -EINVAL;

	ni = ieee80211_find_node(&vap->iv_ic->ic_sta, (const uint8_t*)mac_addr);
	if (!ni)
		return -EINVAL;

	buf_len = iwr->u.data.length;
	buffer = kmalloc(buf_len + 1, GFP_KERNEL);
	if (!buffer) {
		ieee80211_free_node(ni);
		return -EINVAL;
	}
	memset(buffer, 0, buf_len);

	len = 0;
	for (chan = 0; chan < IEEE80211_CHAN_MAX; chan++) {
		if (isset(ni->ni_supp_chans, chan)) {
			tmp_buf = buffer + len;
			len += snprintf(tmp_buf, buf_len - len, "%d,", chan);
		}
	}

	ieee80211_free_node(ni);
	if (len > buf_len) {
		len = strlen(errmsg);
		tmp_buf = buffer + buf_len - len;
		while (len < buf_len) {
			if (tmp_buf[0] == ',')
				break;
			tmp_buf--;
			len++;
		}

		memset(tmp_buf, 0, len);
		strcpy(tmp_buf, errmsg);
		len = strlen(buffer);
	} else if (len > 0) {
		buffer[len - 1] = '\0';
	} else {
		len = snprintf(buffer, buf_len, "Not available");
	}

	retval = copy_to_user(pointer, buffer, len);
	kfree(buffer);

	if (retval)
		return -EINVAL;

	return 0;
}

/**
 *  ieee80211_ioctl_ext - dispatch function for sub-ioctl commands
 *  dev:	network device descriptor
 *  iwr:	request information
 *
 *	Three parameters are used to support sub-ioctls:
 *		iwr->u.data.flags  : the command of sub-ioctl operation
 *		iwr->u.data.pointer: void* pointer to generic object
 *		iwr->u.data.length : the size of generic object
 */
static int
ieee80211_ioctl_ext(struct net_device *dev, struct iwreq *iwr)
{
	int retval = 0;
	int16_t sub_io_cmd = iwr->u.data.flags;
	struct ieee80211vap *vap = netdev_priv(dev);

	switch (sub_io_cmd) {
	case SIOCDEV_SUBIO_RST_QUEUE:
		retval = ieee80211_subioctl_rst_queue(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_RADAR_STATUS:
		retval = ieee80211_subioctl_radar_status(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_GET_PHY_STATS:
		retval = ieee80211_subioctl_get_phy_stats(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_DISCONN_INFO:
		retval = ieee80211_subioctl_disconn_info(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_SET_BRCM_IOCTL:
		retval = ieee80211_subioctl_brcm(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_SCS:
		retval = ieee80211_subioctl_scs(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_WAIT_SCAN_TIMEOUT:
		retval = ieee80211_subioctl_wait_scan_complete(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_AP_SCAN_RESULTS:
		retval = ieee80211_subioctl_ap_scan_results(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
#if defined(CONFIG_QTN_80211K_SUPPORT)
	case SIOCDEV_SUBIO_SET_SOC_ADDR_IOCTL:
	{
		u_int8_t addr_from_user[IEEE80211_ADDR_LEN];
		if (copy_from_user(addr_from_user, iwr->u.data.pointer, IEEE80211_ADDR_LEN)) {
			retval = -EFAULT;
		} else {
			memcpy(vap->iv_ic->soc_addr, addr_from_user, IEEE80211_ADDR_LEN);
		}
		break;
	}
#endif
	case SIOCDEV_SUBIO_SET_TDLS_OPER:
		retval = ieee80211_subioctl_tdls_operation(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_GET_11H_11K_NODE_INFO:
		retval = ieee80211_subioctl_get_doth_dotk_report(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_GET_DSCP2AC_MAP:
		retval = ieee80211_subioctl_get_dscp2ac_map(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_SET_DSCP2AC_MAP:
		retval = ieee80211_subioctl_set_dscp2ac_map(dev, iwr->u.data.pointer);
		break;

	case SIOCDEV_SUBIO_SET_MARK_DFS_CHAN:
		retval = ieee80211_subioctl_set_chan_dfs_required(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SET_WEATHER_CHAN:
		retval = ieee80211_subioctl_set_chan_weather_radar(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SETGET_CHAN_DISABLED:
		retval = ieee80211_subioctl_setget_chan_disabled(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_WOWLAN:
		retval = ieee80211_subioctl_wowlan_setget(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;

	case SIOCDEV_SUBIO_GET_STA_AUTH:
		retval = ieee80211_subioctl_get_sta_auth(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_STA_VENDOR:
		retval = ieee80211_subioctl_get_sta_vendor(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_GET_STA_TPUT_CAPS:
		retval = ieee80211_subioctl_get_sta_tput_caps(dev, iwr->u.data.pointer,
				iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_SWFEAT_MAP:
		retval = ieee80211_subioctl_get_swfeat_map(dev, iwr->u.data.pointer,
				iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_PRINT_SWFEAT_MAP:
		retval = ieee80211_subioctl_print_swfeat_map(dev, iwr->u.data.pointer,
				iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_DI_DFS_CHANNELS:
		retval = ieee80211_ioctl_di_dfs_channels(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SEND_ACTION_FRAME:
		ieee80211_subioctl_send_action_frame(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_DRIVER_CAPABILITY:
		ieee80211_subioctl_get_driver_capa(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SET_AP_INFO:
		ieee80211_subioctl_set_ap_info(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SET_ACTIVE_CHANNEL_LIST:
		retval = ieee80211_subioctl_set_active_chanlist_by_bw(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_GET_LINK_QUALITY_MAX:
		retval = ieee80211_subioctl_get_link_quality_max(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_CHANNEL_POWER_TABLE:
		retval = ieee80211_subioctl_get_chan_power_table(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SET_CHANNEL_POWER_TABLE:
		retval = ieee80211_subioctl_set_chan_power_table(dev, iwr->u.data.pointer);
		break;
	case SIOCDEV_SUBIO_SET_SEC_CHAN:
		retval = ieee80211_subioctl_set_sec_chan(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_SEC_CHAN:
		retval = ieee80211_subioctl_get_sec_chan(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_DSCP2TID_MAP:
		retval = ieee80211_subioctl_get_dscp2tid_map(dev, iwr->u.data.pointer,
				iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SET_DSCP2TID_MAP:
		retval = ieee80211_subioctl_set_dscp2tid_map(dev, iwr->u.data.pointer,
				iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_TX_AIRTIME:
		retval = ieee80211_subioctl_get_txrx_airtime(dev, iwr);
		break;
	case SIOCDEV_SUBIO_GET_CHAN_PRI_INACT:
		retval = ieee80211_subioctl_get_chan_pri_inact(dev, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_SUPP_CHAN:
		retval = ieee80211_get_supp_chans(vap, iwr);
		break;
	case SIOCDEV_SUBIO_GET_CLIENT_MACS:
		retval = ieee80211_subioctl_get_client_mac_list(dev, iwr->u.data.pointer,
							iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_SAMPLE_ALL_DATA:
		retval = ieee80211_subioctl_sample_all_clients(vap, iwr);
		break;
	case SIOCDEV_SUBIO_GET_ASSOC_DATA:
		retval = ieee80211_subioctl_get_assoc_data(vap, iwr->u.data.pointer, iwr->u.data.length);
		break;
	case SIOCDEV_SUBIO_GET_INTERFACE_WMMAC_STATS:
		retval = ieee80211_subioctl_get_interface_wmmac_stats(dev, iwr);
		break;
	default:
		retval = -EOPNOTSUPP;
	}

	return retval;
}

#define MAX_NUM_SUPPORTED_RATES		512
#define GET_SET_BASIC_RATE		1
#define GET_SET_OPERATIONAL_RATE	2
#define GET_SET_MCS_RATE		3
#define MAX_MCS_SIZE			77
#define USEC_PER_SECOND			1000000

static int
is_duplicate_rate(const uint32_t *rate_set, int num, uint32_t rate)
{
	int i;

	for (i = 0; i < num; i++) {
		if (rate_set[i] == rate) {
			return 1;
		}
	}

	return 0;
}

static int ieee80211_ioctl_get_cap_ht_rates(struct ieee80211com *ic,
						uint32_t *ht_rates, int pos)
{
        int i = 0;
        int k = 0, r = 0;
	u_int16_t chan_20 = 0;
	u_int16_t mask;
	u_int16_t chan_40 = 0;
	int sgi_40 = 0;
	int sgi_20 = 0;
	uint32_t rate;

	/* HT 20 MHz band LGI and SGI rates */
	sgi_20 = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI20 ? 1 : 0;
	chan_40 = ic->ic_htcap.cap & IEEE80211_HTCAP_C_CHWIDTH40 ? 1 : 0;
	sgi_40 = ic->ic_htcap.cap & IEEE80211_HTCAP_C_SHORTGI40 ? 1 : 0;

	for (r = 0, i = IEEE80211_HT_MCSSET_20_40_NSS1;
			i <= IEEE80211_HT_MCSSET_20_40_UEQM6; i++) {
		mask = 1;
		for (k = 0; k < 8; k++, r++) {
			if (ic->ic_htcap.mcsset[i] & mask) {
				rate = ieee80211_mcs2rate(r,
					chan_20, 0, 0) * (USEC_PER_SECOND / 2);
				if (!is_duplicate_rate(ht_rates, pos, rate)) {
					ht_rates[pos++] = rate;
				}
				if (sgi_20) {
					rate = ieee80211_mcs2rate(r,
						chan_20, sgi_20, 0) * (USEC_PER_SECOND / 2);
					if (!is_duplicate_rate(ht_rates, pos, rate)) {
						ht_rates[pos++] = rate;
					}
				}
				if (chan_40) {
					rate = ieee80211_mcs2rate(r,
						chan_40, 0, 0) * (USEC_PER_SECOND / 2);
					if (!is_duplicate_rate(ht_rates, pos, rate)) {
						ht_rates[pos++] = rate;
					}
					if (sgi_40) {
						rate = ieee80211_mcs2rate(r,
							chan_40, sgi_40, 0) * (USEC_PER_SECOND / 2);
						if (!is_duplicate_rate(ht_rates, pos, rate)) {
							ht_rates[pos++] = rate;
						}
					}
				}
			}
			mask = mask << 1;
		}
	}

	return pos;
}

static int ieee80211_ioctl_get_cap_vht_rates(struct ieee80211com *ic,
						uint32_t *vht_rates, int pos)
{
        int k = 0, r = 0;
	u_int16_t mcsmap = 0;
	int sgi_80;
	int sgi_160;
	u_int16_t mask;
	u_int16_t chan_80 = 0;
	u_int16_t chan_160 = 0;
	uint32_t rate;

	if (ic->ic_vhtcap.cap_flags & IEEE80211_VHTCAP_C_CHWIDTH) {
		chan_160 = 1;
	}
	sgi_80 = (ic->ic_vhtcap.cap_flags & IEEE80211_VHTCAP_C_SHORT_GI_80) ? 1 : 0;
	sgi_160 = (ic->ic_vhtcap.cap_flags & IEEE80211_VHTCAP_C_SHORT_GI_160) ? 1 : 0;
	mask = 0x3;
	mcsmap = ic->ic_vhtcap.txmcsmap;
	for (k = 0; k < 8; k++) {
		if ((mcsmap & mask) != mask) {
			int m;
			int val = (mcsmap & mask)>>(k * 2);
			r = (val == 2) ? 9: (val == 1) ? 8 : 7;
			for (m = 0; m <= r; m++) {
				rate = (ieee80211_mcs2rate(m, chan_80, 0, 1) * (USEC_PER_SECOND / 2)) * (k+1);
				if (!is_duplicate_rate(vht_rates, pos, rate)) {
					vht_rates[pos++] = rate;
				}
				if (sgi_80) {
					rate = (ieee80211_mcs2rate(m, chan_80,
						sgi_80, 1) * (USEC_PER_SECOND / 2)) * (k+1);
					if (!is_duplicate_rate(vht_rates, pos, rate)) {
						vht_rates[pos++] = rate;
					}
				}

				/* 160/80+80 MHz rates */
				if (chan_160) {
					rate  = (ieee80211_mcs2rate(m,
						chan_160, 0, 1) * (USEC_PER_SECOND / 2)) * (k+1);
					if (!is_duplicate_rate(vht_rates, pos, rate)) {
						vht_rates[pos++] = rate;
					}
					if (sgi_160) {
						rate = (ieee80211_mcs2rate(m,
							chan_160, sgi_160, 1) * (USEC_PER_SECOND / 2)) * (k+1);
						if (!is_duplicate_rate(vht_rates, pos, rate)) {
							vht_rates[pos++] = rate;
						}
					}
				}
			}
			mask = mask << 2;
		} else {
			break;
		}
	}

	return pos;
}

/**
 *  ieee80211_ioctl_get_rates - function to get the rates
 */
static int
ieee80211_ioctl_get_rates(struct net_device *dev, struct iwreq *iwr)
{
        struct ieee80211vap *vap = netdev_priv(dev);
        struct ieee80211com *ic  = vap->iv_ic;
        uint32_t achievable_tx_phy_rates[MAX_NUM_SUPPORTED_RATES];
        uint32_t achievable_rates_num = iwr->u.data.length / sizeof(uint32_t);
        int mode, nrates, i = 0, j = 0;
        int flags = iwr->u.data.flags;
        struct ieee80211_rateset *rs;

        mode = ic->ic_curmode; // Get the current mode
        rs = &ic->ic_sup_rates[mode]; // Get the supported rates depending on the mode
        nrates = rs->rs_legacy_nrates;

        if (flags == GET_SET_MCS_RATE) {
		/* NB: not sorted */
		/* Basic and extended rates */
                for (i = 0; i < nrates ; i++) {
                        achievable_tx_phy_rates[j] = rs->rs_rates[i] & IEEE80211_RATE_VAL;
			// Keep the rates in Mbps. Multiply the rate by 1M
			achievable_tx_phy_rates[j] *= (USEC_PER_SECOND / 2);
                        j++;
                }

		/* Possible HT rates */
		j = ieee80211_ioctl_get_cap_ht_rates(ic, &achievable_tx_phy_rates[0], j);

		/* Possible VHT rates */
		j = ieee80211_ioctl_get_cap_vht_rates(ic, &achievable_tx_phy_rates[0], j);

                if (achievable_rates_num > j) {
			achievable_rates_num = j;
		}

		iwr->u.data.length = achievable_rates_num * sizeof(achievable_rates_num);

        } else { // Basic or operational rates
                for (i = 0; i < nrates ; i++) {
                        if ( (flags == GET_SET_BASIC_RATE) && !(rs->rs_rates[i] & IEEE80211_RATE_BASIC) ) {
                                continue;
                        }

                        achievable_tx_phy_rates[j] = rs->rs_rates[i] & IEEE80211_RATE_VAL;
			// Keep the rates in Mbps. Multiply the rate by 1M
			achievable_tx_phy_rates[j] *= USEC_PER_SECOND;
                        j++;
                }

                iwr->u.data.length = j * sizeof(int32_t);
        }

	if (copy_to_user(iwr->u.data.pointer, achievable_tx_phy_rates,
			MIN(sizeof(achievable_tx_phy_rates), iwr->u.data.length)))
		return -EFAULT;

        return 0;
}

/**
 * ieee80211_ioctl_set_rates - function to set the rates. This can be used for setting the
 * rates either to basic or operational rates.
 */
static int
ieee80211_ioctl_set_rates(struct net_device *dev, struct iwreq *iwr)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic  = vap->iv_ic;
	struct ieee80211_rateset *rs;
	char *ptr = ((char *)iwr->u.data.pointer);
	uint32_t num_rates = iwr->u.data.length;
	uint32_t flags = iwr->u.data.flags;
	unsigned long rate;
	enum ieee80211_phymode mode;
	int retval = 0;
	int i = 0;

	mode = ic->ic_curmode;
	rs = &ic->ic_sup_rates[mode];

	/* Set rates is allowed for only Basic and operational rates (non 11n) */
	if (num_rates > IEEE80211_AG_RATE_MAXSIZE) {
		num_rates = IEEE80211_AG_RATE_MAXSIZE;
	}

	while ( num_rates-- ) {
		if (strict_strtoul(ptr, 0, &rate)) {
			printk(KERN_WARNING "Invalide input string\n");
			retval = -EINVAL;
			break;
		}

		for (i = 0; i < rs->rs_legacy_nrates; i++) {
			if (rate == (rs->rs_rates[i] & IEEE80211_RATE_VAL)) {

				if (flags == GET_SET_BASIC_RATE) {
					rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
				} else if (flags == GET_SET_OPERATIONAL_RATE ) {
					rs->rs_rates[i] &= ~IEEE80211_RATE_BASIC;

				} else {
					printk(KERN_WARNING "Not supported to change the MCS rates\n");
					retval = -EINVAL;
					break;
				}
			}
		}
		ptr += strlen(ptr) + 1;
	}

	/* Update the beacon. This will dynamically change the rates
	 * in probe response and beacons */
	if (!retval) {
		ic->ic_beacon_update(vap);
	}

	return retval;
}

static void
pre_announced_chanswitch(struct net_device *dev, u_int32_t channel, u_int32_t tbtt) {
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	/* now flag the beacon update to include the channel switch IE */
	ic->ic_flags |= IEEE80211_F_CHANSWITCH;
	ic->ic_chanchange_chan = channel;
	ic->ic_chanchange_tbtt = tbtt;
}

static int
ieee80211_ioctl_chanswitch(struct net_device *dev, struct iw_request_info *info,
	void *w, char *extra)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;
	int *param = (int *) extra;

	if (!(ic->ic_flags & IEEE80211_F_DOTH))
		return 0;

	pre_announced_chanswitch(dev, param[0], param[1]);

	return 0;
}

static int
ieee80211_ioctl_siwmlme(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *erq, char *data)
{
	struct ieee80211req_mlme mlme;
	struct iw_mlme *wextmlme = (struct iw_mlme *)data;

	memset(&mlme, 0, sizeof(mlme));

	switch(wextmlme->cmd) {
	case IW_MLME_DEAUTH:
		mlme.im_op = IEEE80211_MLME_DEAUTH;
		break;
	case IW_MLME_DISASSOC:
		mlme.im_op = IEEE80211_MLME_DISASSOC;
		break;
	default:
		return -EINVAL;
	}

	mlme.im_reason = wextmlme->reason_code;

	memcpy(mlme.im_macaddr, wextmlme->addr.sa_data, IEEE80211_ADDR_LEN);

	return ieee80211_ioctl_setmlme(dev, NULL, NULL, (char*)&mlme);
}


static int
ieee80211_ioctl_giwgenie(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *out, char *buf)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	if (out->length < vap->iv_opt_ie_len)
		return -E2BIG;

	return ieee80211_ioctl_getoptie(dev, info, out, buf);
}

static int
ieee80211_ioctl_siwgenie(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *erq, char *data)
{
	return ieee80211_ioctl_setoptie(dev, info, erq, data);
}


static int
siwauth_wpa_version(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int ver = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_WPA;

	if ((ver & IW_AUTH_WPA_VERSION_WPA) && (ver & IW_AUTH_WPA_VERSION_WPA2))
		args[1] = 3;
	else if (ver & IW_AUTH_WPA_VERSION_WPA2)
		args[1] = 2;
	else if (ver & IW_AUTH_WPA_VERSION_WPA)
		args[1] = 1;
	else
		args[1] = 0;

	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
iwcipher2ieee80211cipher(int iwciph)
{
	switch(iwciph) {
	case IW_AUTH_CIPHER_NONE:
		return IEEE80211_CIPHER_NONE;
	case IW_AUTH_CIPHER_WEP40:
	case IW_AUTH_CIPHER_WEP104:
		return IEEE80211_CIPHER_WEP;
	case IW_AUTH_CIPHER_TKIP:
		return IEEE80211_CIPHER_TKIP;
	case IW_AUTH_CIPHER_CCMP:
		return IEEE80211_CIPHER_AES_CCM;
	}
	return -1;
}

static int
ieee80211cipher2iwcipher(int ieee80211ciph)
{
	switch(ieee80211ciph) {
	case IEEE80211_CIPHER_NONE:
		return IW_AUTH_CIPHER_NONE;
	case IEEE80211_CIPHER_WEP:
		return IW_AUTH_CIPHER_WEP104;
	case IEEE80211_CIPHER_TKIP:
		return IW_AUTH_CIPHER_TKIP;
	case IEEE80211_CIPHER_AES_CCM:
		return IW_AUTH_CIPHER_CCMP;
	}
	return -1;
}

/* TODO We don't enforce wep key lengths. */
static int
siwauth_cipher_pairwise(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int iwciph = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_UCASTCIPHER;
	args[1] = iwcipher2ieee80211cipher(iwciph);
	if (args[1] < 0) {
		printk(KERN_WARNING "%s: unknown pairwise cipher %d\n",
		       dev->name, iwciph);
		return -EINVAL;
	}
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

/* TODO We don't enforce wep key lengths. */
static int
siwauth_cipher_group(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int iwciph = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_MCASTCIPHER;
	args[1] = iwcipher2ieee80211cipher(iwciph);
	if (args[1] < 0) {
		printk(KERN_WARNING "%s: unknown group cipher %d\n",
		       dev->name, iwciph);
		return -EINVAL;
	}
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_key_mgmt(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int iwkm = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_KEYMGTALGS;
	args[1] = WPA_ASE_NONE;
	if (iwkm & IW_AUTH_KEY_MGMT_802_1X)
		args[1] |= WPA_ASE_8021X_UNSPEC;
	if (iwkm & IW_AUTH_KEY_MGMT_PSK)
		args[1] |= WPA_ASE_8021X_PSK;

	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_tkip_countermeasures(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int args[2];
	args[0] = IEEE80211_PARAM_COUNTERMEASURES;
	args[1] = erq->value;
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_drop_unencrypted(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int args[2];
	args[0] = IEEE80211_PARAM_DROPUNENCRYPTED;
	args[1] = erq->value;
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}


static int
siwauth_80211_auth_alg(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
#define VALID_ALGS_MASK (IW_AUTH_ALG_OPEN_SYSTEM|IW_AUTH_ALG_SHARED_KEY|IW_AUTH_ALG_LEAP)
	int mode = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_AUTHMODE;

	if (mode & ~VALID_ALGS_MASK) {
		return -EINVAL;
	}
	if (mode & IW_AUTH_ALG_LEAP) {
		args[1] = IEEE80211_AUTH_8021X;
	} else if ((mode & IW_AUTH_ALG_SHARED_KEY) &&
		  (mode & IW_AUTH_ALG_OPEN_SYSTEM)) {
		args[1] = IEEE80211_AUTH_AUTO;
	} else if (mode & IW_AUTH_ALG_SHARED_KEY) {
		args[1] = IEEE80211_AUTH_SHARED;
	} else {
		args[1] = IEEE80211_AUTH_OPEN;
	}
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_wpa_enabled(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int enabled = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_WPA;
	if (enabled)
		args[1] = 3; /* enable WPA1 and WPA2 */
	else
		args[1] = 0; /* disable WPA1 and WPA2 */

	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_rx_unencrypted_eapol(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rxunenc = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_DROPUNENC_EAPOL;
	if (rxunenc)
		args[1] = 1;
	else
		args[1] = 0;

	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_roaming_control(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int roam = erq->value;
	int args[2];

	args[0] = IEEE80211_PARAM_ROAMING;
	switch(roam) {
	case IW_AUTH_ROAMING_ENABLE:
		args[1] = IEEE80211_ROAMING_AUTO;
		break;
	case IW_AUTH_ROAMING_DISABLE:
		args[1] = IEEE80211_ROAMING_MANUAL;
		break;
	default:
		return -EINVAL;
	}
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_privacy_invoked(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int args[2];
	args[0] = IEEE80211_PARAM_PRIVACY;
	args[1] = erq->value;
	return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

/*
 * If this function is invoked it means someone is using the wireless extensions
 * API instead of the private madwifi ioctls.  That's fine.  We translate their
 * request into the format used by the private ioctls.  Note that the
 * iw_request_info and iw_param structures are not the same ones as the
 * private ioctl handler expects.  Luckily, the private ioctl handler doesn't
 * do anything with those at the moment.  We pass NULL for those, because in
 * case someone does modify the ioctl handler to use those values, a null
 * pointer will be easier to debug than other bad behavior.
 */
static int
ieee80211_ioctl_siwauth(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc = -EINVAL;

	switch(erq->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
		rc = siwauth_wpa_version(dev, info, erq, buf);
		break;
	case IW_AUTH_CIPHER_PAIRWISE:
		rc = siwauth_cipher_pairwise(dev, info, erq, buf);
		break;
	case IW_AUTH_CIPHER_GROUP:
		rc = siwauth_cipher_group(dev, info, erq, buf);
		break;
	case IW_AUTH_KEY_MGMT:
		rc = siwauth_key_mgmt(dev, info, erq, buf);
		break;
	case IW_AUTH_TKIP_COUNTERMEASURES:
		rc = siwauth_tkip_countermeasures(dev, info, erq, buf);
		break;
	case IW_AUTH_DROP_UNENCRYPTED:
		rc = siwauth_drop_unencrypted(dev, info, erq, buf);
		break;
	case IW_AUTH_80211_AUTH_ALG:
		rc = siwauth_80211_auth_alg(dev, info, erq, buf);
		break;
	case IW_AUTH_WPA_ENABLED:
		rc = siwauth_wpa_enabled(dev, info, erq, buf);
		break;
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		rc = siwauth_rx_unencrypted_eapol(dev, info, erq, buf);
		break;
	case IW_AUTH_ROAMING_CONTROL:
		rc = siwauth_roaming_control(dev, info, erq, buf);
		break;
	case IW_AUTH_PRIVACY_INVOKED:
		rc = siwauth_privacy_invoked(dev, info, erq, buf);
		break;
	default:
		printk(KERN_WARNING "%s: unknown SIOCSIWAUTH flag %d\n",
			dev->name, erq->flags);
		break;
	}

	return rc;
}

static int
giwauth_wpa_version(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int ver;
	int rc;
	int arg = IEEE80211_PARAM_WPA;

	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;

	switch(arg) {
	case 1:
	    	ver = IW_AUTH_WPA_VERSION_WPA;
		break;
	case 2:
	    	ver = IW_AUTH_WPA_VERSION_WPA2;
		break;
	case 3:
	    	ver = IW_AUTH_WPA_VERSION|IW_AUTH_WPA_VERSION_WPA2;
		break;
	default:
		ver = IW_AUTH_WPA_VERSION_DISABLED;
		break;
	}

	erq->value = ver;
	return rc;
}

static int
giwauth_cipher_pairwise(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc;
	int arg = IEEE80211_PARAM_UCASTCIPHER;

	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;

	erq->value = ieee80211cipher2iwcipher(arg);
	if (erq->value < 0)
		return -EINVAL;
	return 0;
}


static int
giwauth_cipher_group(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc;
	int arg = IEEE80211_PARAM_MCASTCIPHER;

	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;

	erq->value = ieee80211cipher2iwcipher(arg);
	if (erq->value < 0)
		return -EINVAL;
	return 0;
}

static int
giwauth_key_mgmt(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int arg;
	int rc;

	arg = IEEE80211_PARAM_KEYMGTALGS;
	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;
	erq->value = 0;
	if (arg & WPA_ASE_8021X_UNSPEC)
		erq->value |= IW_AUTH_KEY_MGMT_802_1X;
	if (arg & WPA_ASE_8021X_PSK)
		erq->value |= IW_AUTH_KEY_MGMT_PSK;
	return 0;
}

static int
giwauth_tkip_countermeasures(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int arg;
	int rc;

	arg = IEEE80211_PARAM_COUNTERMEASURES;
	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;
	erq->value = arg;
	return 0;
}

static int
giwauth_drop_unencrypted(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int arg;
	int rc;
	arg = IEEE80211_PARAM_DROPUNENCRYPTED;
	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;
	erq->value = arg;
	return 0;
}

static int
giwauth_80211_auth_alg(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	return -EOPNOTSUPP;
}

static int
giwauth_wpa_enabled(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc;
	int arg = IEEE80211_PARAM_WPA;

	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;

	erq->value = arg;
	return 0;

}

static int
giwauth_rx_unencrypted_eapol(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	return -EOPNOTSUPP;
}

static int
giwauth_roaming_control(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc;
	int arg;

	arg = IEEE80211_PARAM_ROAMING;
	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;

	switch(arg) {
	case IEEE80211_ROAMING_DEVICE:
	case IEEE80211_ROAMING_AUTO:
		erq->value = IW_AUTH_ROAMING_ENABLE;
		break;
	default:
		erq->value = IW_AUTH_ROAMING_DISABLE;
		break;
	}

	return 0;
}

static int
giwauth_privacy_invoked(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc;
	int arg;
	arg = IEEE80211_PARAM_PRIVACY;
	rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
	if (rc)
		return rc;
	erq->value = arg;
	return 0;
}

static int
ieee80211_ioctl_giwauth(struct net_device *dev,
	struct iw_request_info *info, struct iw_param *erq, char *buf)
{
	int rc = -EOPNOTSUPP;

	switch(erq->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
		rc = giwauth_wpa_version(dev, info, erq, buf);
		break;
	case IW_AUTH_CIPHER_PAIRWISE:
		rc = giwauth_cipher_pairwise(dev, info, erq, buf);
		break;
	case IW_AUTH_CIPHER_GROUP:
		rc = giwauth_cipher_group(dev, info, erq, buf);
		break;
	case IW_AUTH_KEY_MGMT:
		rc = giwauth_key_mgmt(dev, info, erq, buf);
		break;
	case IW_AUTH_TKIP_COUNTERMEASURES:
		rc = giwauth_tkip_countermeasures(dev, info, erq, buf);
		break;
	case IW_AUTH_DROP_UNENCRYPTED:
		rc = giwauth_drop_unencrypted(dev, info, erq, buf);
		break;
	case IW_AUTH_80211_AUTH_ALG:
		rc = giwauth_80211_auth_alg(dev, info, erq, buf);
		break;
	case IW_AUTH_WPA_ENABLED:
		rc = giwauth_wpa_enabled(dev, info, erq, buf);
		break;
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		rc = giwauth_rx_unencrypted_eapol(dev, info, erq, buf);
		break;
	case IW_AUTH_ROAMING_CONTROL:
		rc = giwauth_roaming_control(dev, info, erq, buf);
		break;
	case IW_AUTH_PRIVACY_INVOKED:
		rc = giwauth_privacy_invoked(dev, info, erq, buf);
		break;
	default:
		printk(KERN_WARNING "%s: unknown SIOCGIWAUTH flag %d\n",
			dev->name, erq->flags);
		break;
	}

	return rc;
}

/*
 * Retrieve information about a key.  Open question: should we allow
 * callers to retrieve unicast keys based on a supplied MAC address?
 * The ipw2200 reference implementation doesn't, so we don't either.
 *
 * Not currently used
 */
static int
ieee80211_ioctl_giwencodeext(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *erq, char *extra)
{
#ifndef IEEE80211_UNUSED_CRYPTO_COMMANDS
	return -EOPNOTSUPP;
#else
	struct ieee80211vap *vap = netdev_priv(dev);
	struct iw_encode_ext *ext;
	struct ieee80211_key *wk;
	int error;
	int kid;
	int max_key_len;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	max_key_len = erq->length - sizeof(*ext);
	if (max_key_len < 0)
		return -EINVAL;
	ext = (struct iw_encode_ext *)extra;

	error = getiwkeyix(vap, erq, &kid);
	if (error < 0)
		return error;

	wk = &vap->iv_nw_keys[kid];
	if (wk->wk_keylen > max_key_len)
		return -E2BIG;

	erq->flags = kid+1;
	memset(ext, 0, sizeof(*ext));

	ext->key_len = wk->wk_keylen;
	memcpy(ext->key, wk->wk_key, wk->wk_keylen);

	/* flags */
	if (wk->wk_flags & IEEE80211_KEY_GROUP)
		ext->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;

	/* algorithm */
	switch(wk->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_NONE:
		ext->alg = IW_ENCODE_ALG_NONE;
		erq->flags |= IW_ENCODE_DISABLED;
		break;
	case IEEE80211_CIPHER_WEP:
		ext->alg = IW_ENCODE_ALG_WEP;
		break;
	case IEEE80211_CIPHER_TKIP:
		ext->alg = IW_ENCODE_ALG_TKIP;
		break;
	case IEEE80211_CIPHER_AES_OCB:
	case IEEE80211_CIPHER_AES_CCM:
	case IEEE80211_CIPHER_CKIP:
		ext->alg = IW_ENCODE_ALG_CCMP;
		break;
	default:
		return -EINVAL;
	}
	return 0;
#endif /* IEEE80211_UNUSED_CRYPTO_COMMANDS */
}

static int
ieee80211_ioctl_siwencodeext(struct net_device *dev,
	struct iw_request_info *info, struct iw_point *erq, char *extra)
{
#ifndef IEEE80211_UNUSED_CRYPTO_COMMANDS
	return -EOPNOTSUPP;
#else
	struct ieee80211vap *vap = netdev_priv(dev);
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	struct ieee80211req_key kr;
	int error;
	int kid;

	error = getiwkeyix(vap, erq, &kid);
	if (error < 0)
		return error;

	if (ext->key_len > (erq->length - sizeof(struct iw_encode_ext)))
		return -EINVAL;

	if (ext->alg == IW_ENCODE_ALG_NONE) {
		/* convert to the format used by IEEE_80211_IOCTL_DELKEY */
		struct ieee80211req_del_key dk;

		memset(&dk, 0, sizeof(dk));
		dk.idk_keyix = kid;
		memcpy(&dk.idk_macaddr, ext->addr.sa_data, IEEE80211_ADDR_LEN);

		return ieee80211_ioctl_delkey(dev, NULL, NULL, (char*)&dk);
	}

	/* TODO This memcmp for the broadcast address seems hackish, but
	 * mimics what wpa supplicant was doing.  The wpa supplicant comments
	 * make it sound like they were having trouble with
	 * IEEE80211_IOCTL_SETKEY and static WEP keys.  It might be worth
	 * figuring out what their trouble was so the rest of this function
	 * can be implemented in terms of ieee80211_ioctl_setkey */
	if (ext->alg == IW_ENCODE_ALG_WEP &&
	    memcmp(ext->addr.sa_data, "\xff\xff\xff\xff\xff\xff",
		   IEEE80211_ADDR_LEN) == 0) {
		/* convert to the format used by SIOCSIWENCODE.  The old
		 * format just had the key in the extra buf, whereas the
		 * new format has the key tacked on to the end of the
		 * iw_encode_ext structure */
		struct iw_request_info oldinfo;
		struct iw_point olderq;
		char *key;

		memset(&oldinfo, 0, sizeof(oldinfo));
		oldinfo.cmd = SIOCSIWENCODE;
		oldinfo.flags = info->flags;

		memset(&olderq, 0, sizeof(olderq));
		olderq.flags = erq->flags;
		olderq.pointer = erq->pointer;
		olderq.length = ext->key_len;

		key = (char *)ext->key;

		return ieee80211_ioctl_siwencode(dev, &oldinfo, &olderq, key);
	}

	/* convert to the format used by IEEE_80211_IOCTL_SETKEY */
	memset(&kr, 0, sizeof(kr));

	switch(ext->alg) {
	case IW_ENCODE_ALG_WEP:
		kr.ik_type = IEEE80211_CIPHER_WEP;
		break;
	case IW_ENCODE_ALG_TKIP:
		kr.ik_type = IEEE80211_CIPHER_TKIP;
		break;
	case IW_ENCODE_ALG_CCMP:
		kr.ik_type = IEEE80211_CIPHER_AES_CCM;
		break;
	default:
		printk(KERN_WARNING "%s: unknown algorithm %d\n",
		       dev->name, ext->alg);
		return -EINVAL;
	}

	kr.ik_keyix = kid;

	if (ext->key_len > sizeof(kr.ik_keydata)) {
		printk(KERN_WARNING "%s: key size %d is too large\n",
		       dev->name, ext->key_len);
		return -E2BIG;
	}
	memcpy(kr.ik_keydata, ext->key, ext->key_len);
	kr.ik_keylen = ext->key_len;

	kr.ik_flags = IEEE80211_KEY_RECV;

	if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
		kr.ik_flags |= IEEE80211_KEY_GROUP;

	if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
		kr.ik_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_DEFAULT;
		memcpy(kr.ik_macaddr, ext->addr.sa_data, IEEE80211_ADDR_LEN);
	}

	if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
		memcpy(&kr.ik_keyrsc, ext->rx_seq, sizeof(kr.ik_keyrsc));
	}

	return ieee80211_ioctl_setkey(dev, NULL, NULL, (char*)&kr);
#endif /* IEEE80211_UNUSED_CRYPTO_COMMANDS */
}

#define	IW_PRIV_TYPE_OPTIE	IW_PRIV_TYPE_BYTE | IEEE80211_MAX_OPT_IE
#define	IW_PRIV_TYPE_KEY \
	IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_key)
#define	IW_PRIV_TYPE_DELKEY \
	IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_del_key)
#define	IW_PRIV_TYPE_MLME \
	IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_mlme)
#define	IW_PRIV_TYPE_CHANLIST \
	IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_chanlist)
#define	IW_PRIV_TYPE_CHANINFO \
	IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_chaninfo)
#define IW_PRIV_TYPE_APPIEBUF \
	(IW_PRIV_TYPE_BYTE | (sizeof(struct ieee80211req_getset_appiebuf) + IEEE80211_APPIE_MAX))
#define IW_PRIV_TYPE_FILTER \
	IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_set_filter)
#define IW_PRIV_TYPE_CCA \
	(IW_PRIV_TYPE_BYTE | sizeof(struct qtn_cca_args))
#if defined(CONFIG_QTN_80211K_SUPPORT)
#define IW_PRIV_TYPE_STASTATISTIC_SETPARA \
	(IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_qtn_rmt_sta_stats_setpara))
#define IW_PRIV_TYPE_STASTATISTIC_GETPARA \
	(IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_qtn_rmt_sta_stats))
#endif

/* make sure the size of each block_data member is large then IFNAMSIZ */
union block_data {
	struct ieee80211req_csw_record record;
	struct ieee80211_assoc_history assoc_history;
	uint32_t pm_state[QTN_PM_IOCTL_MAX];
};
#define IW_PRIV_BLOCK_DATASIZE (sizeof(union block_data))

static const struct iw_priv_args ieee80211_priv_args[] =
{
	/* NB: setoptie & getoptie are !IW_PRIV_SIZE_FIXED */
	{ IEEE80211_IOCTL_SETOPTIE,
	  IW_PRIV_TYPE_OPTIE, 0,			"setoptie" },
	{ IEEE80211_IOCTL_GETOPTIE,
	  0, IW_PRIV_TYPE_OPTIE,			"getoptie" },
	{ IEEE80211_IOCTL_SETKEY,
	  IW_PRIV_TYPE_KEY | IW_PRIV_SIZE_FIXED, 0,	"setkey" },
	{ IEEE80211_IOCTL_DELKEY,
	  IW_PRIV_TYPE_DELKEY | IW_PRIV_SIZE_FIXED, 0,	"delkey" },
	{ IEEE80211_IOCTL_SETMLME,
	  IW_PRIV_TYPE_MLME | IW_PRIV_SIZE_FIXED, 0,	"setmlme" },
	{ IEEE80211_IOCTL_ADDMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"setbssid" },
	{ IEEE80211_IOCTL_DELMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"delmac" },
	{ IEEE80211_IOCTL_KICKMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "kickmac"},
	{ IEEE80211_IOCTL_WDSADDMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"wds_add" },
	{ IEEE80211_IOCTL_WDSDELMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"wds_del" },
	{ IEEE80211_IOCTL_SETCHANLIST,
	  IW_PRIV_TYPE_CHANLIST | IW_PRIV_SIZE_FIXED, 0,"setchanlist" },
	{ IEEE80211_IOCTL_GETCHANLIST,
	  0, IW_PRIV_TYPE_CHANLIST | IW_PRIV_SIZE_FIXED,"getchanlist" },
	{ IEEE80211_IOCTL_STARTCCA,
	  IW_PRIV_TYPE_CCA | IW_PRIV_SIZE_FIXED | 1, 0, "startcca" },
	{ IEEE80211_IOCTL_GETCHANINFO,
	  0, IW_PRIV_TYPE_CHANINFO | IW_PRIV_SIZE_FIXED,"getchaninfo" },
	{ IEEE80211_IOCTL_SETMODE,
	  IW_PRIV_TYPE_CHAR |  12, 0, "mode" },
	{ IEEE80211_IOCTL_GETMODE,
	  0, IW_PRIV_TYPE_CHAR | 6, "get_mode" },
	{ IEEE80211_IOCTL_POSTEVENT,
	  IW_PRIV_TYPE_CHAR | 256, 0,			"postevent" },
	{ IEEE80211_IOCTL_TXEAPOL,
	  IW_PRIV_TYPE_BYTE | 2047, 0,			"txeapol" },
	{ IEEE80211_IOCTL_SETWMMPARAMS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0,"setwmmparams" },
	{ IEEE80211_IOCTL_GETWMMPARAMS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "getwmmparams" },
	{ IEEE80211_IOCTL_RADAR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth_radar" },
	{ IEEE80211_IOCTL_DFSACTSCAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dfsactscan" },
#if defined(CONFIG_QTN_80211K_SUPPORT)
	{ IEEE80211_IOCTL_GETSTASTATISTIC,
	  IW_PRIV_TYPE_STASTATISTIC_SETPARA,
	  IW_PRIV_TYPE_STASTATISTIC_GETPARA | IW_PRIV_SIZE_FIXED, "getstastatistic" },
#endif
	/*
	 * These depends on sub-ioctl support which added in version 12.
	 */
	{ IEEE80211_IOCTL_SETWMMPARAMS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"" },
	{ IEEE80211_IOCTL_GETWMMPARAMS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "" },
	/* sub-ioctl handlers */
	{ IEEE80211_WMMPARAMS_CWMIN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"cwmin" },
	{ IEEE80211_WMMPARAMS_CWMIN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_cwmin" },
	{ IEEE80211_WMMPARAMS_CWMAX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"cwmax" },
	{ IEEE80211_WMMPARAMS_CWMAX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_cwmax" },
	{ IEEE80211_WMMPARAMS_AIFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"aifs" },
	{ IEEE80211_WMMPARAMS_AIFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_aifs" },
	{ IEEE80211_WMMPARAMS_TXOPLIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"txoplimit" },
	{ IEEE80211_WMMPARAMS_TXOPLIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_txoplimit" },
	{ IEEE80211_WMMPARAMS_ACM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"acm" },
	{ IEEE80211_WMMPARAMS_ACM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_acm" },
	{ IEEE80211_WMMPARAMS_NOACKPOLICY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"noackpolicy" },
	{ IEEE80211_WMMPARAMS_NOACKPOLICY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_noackpolicy" },

	{ IEEE80211_IOCTL_SETPARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam" },
	/*
	 * These depends on sub-ioctl support which added in version 12.
	 */
	{ IEEE80211_IOCTL_GETPARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,	"getparam" },

	/* sub-ioctl handlers */
	{ IEEE80211_IOCTL_SETPARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
	{ IEEE80211_IOCTL_GETPARAM,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "" },

	/* sub-ioctl definitions */
	{ IEEE80211_PARAM_AUTHMODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "authmode" },
	{ IEEE80211_PARAM_AUTHMODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_authmode" },
	{ IEEE80211_PARAM_PROTMODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "protmode" },
	{ IEEE80211_PARAM_PROTMODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_protmode" },
	{ IEEE80211_PARAM_MCASTCIPHER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcastcipher" },
	{ IEEE80211_PARAM_MCASTCIPHER,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcastcipher" },
	{ IEEE80211_PARAM_MCASTKEYLEN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcastkeylen" },
	{ IEEE80211_PARAM_MCASTKEYLEN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcastkeylen" },
	{ IEEE80211_PARAM_UCASTCIPHERS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ucastciphers" },
	{ IEEE80211_PARAM_UCASTCIPHERS,
	/*
	 * NB: can't use "get_ucastciphers" due to iwpriv command names
	 *     must be <IFNAMESIZ which is 16.
	 */
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_uciphers" },
	{ IEEE80211_PARAM_UCASTCIPHER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ucastcipher" },
	{ IEEE80211_PARAM_UCASTCIPHER,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ucastcipher" },
	{ IEEE80211_PARAM_UCASTKEYLEN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ucastkeylen" },
	{ IEEE80211_PARAM_UCASTKEYLEN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ucastkeylen" },
	{ IEEE80211_PARAM_KEYMGTALGS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "keymgtalgs" },
	{ IEEE80211_PARAM_KEYMGTALGS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_keymgtalgs" },
	{ IEEE80211_PARAM_RSNCAPS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rsncaps" },
	{ IEEE80211_PARAM_RSNCAPS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rsncaps" },
	{ IEEE80211_PARAM_ROAMING,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hostroaming" },
	{ IEEE80211_PARAM_ROAMING,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_hostroaming" },
	{ IEEE80211_PARAM_PRIVACY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "privacy" },
	{ IEEE80211_PARAM_PRIVACY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_privacy" },
	{ IEEE80211_PARAM_COUNTERMEASURES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "countermeasures" },
	{ IEEE80211_PARAM_COUNTERMEASURES,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_countermeas" },
	{ IEEE80211_PARAM_DROPUNENCRYPTED,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dropunencrypted" },
	{ IEEE80211_PARAM_DROPUNENCRYPTED,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dropunencry" },
	{ IEEE80211_PARAM_WPA,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wpa" },
	{ IEEE80211_PARAM_WPA,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wpa" },
	{ IEEE80211_PARAM_DRIVER_CAPS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "driver_caps" },
	{ IEEE80211_PARAM_DRIVER_CAPS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_driver_caps" },
	{ IEEE80211_PARAM_MACCMD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "maccmd" },
	{ IEEE80211_PARAM_WMM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wmm" },
	{ IEEE80211_PARAM_WMM,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wmm" },
	{ IEEE80211_PARAM_HIDESSID,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hide_ssid" },
	{ IEEE80211_PARAM_HIDESSID,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_hide_ssid" },
	{ IEEE80211_PARAM_APBRIDGE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ap_bridge" },
	{ IEEE80211_PARAM_APBRIDGE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ap_bridge" },
	{ IEEE80211_PARAM_INACT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inact" },
	{ IEEE80211_PARAM_INACT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_inact" },
	{ IEEE80211_PARAM_INACT_AUTH,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inact_auth" },
	{ IEEE80211_PARAM_INACT_AUTH,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_inact_auth" },
	{ IEEE80211_PARAM_INACT_INIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inact_init" },
	{ IEEE80211_PARAM_INACT_INIT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_inact_init" },
	{ IEEE80211_PARAM_ABOLT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "abolt" },
	{ IEEE80211_PARAM_ABOLT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_abolt" },
	{ IEEE80211_PARAM_DTIM_PERIOD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dtim_period" },
	{ IEEE80211_PARAM_DTIM_PERIOD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dtim_period" },
	{ IEEE80211_PARAM_ASSOC_LIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "assoc_limit" },
	{ IEEE80211_PARAM_ASSOC_LIMIT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_assoc_limit" },
	{ IEEE80211_PARAM_BSS_ASSOC_LIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bss_assoc_limit" },
	{ IEEE80211_PARAM_BSS_ASSOC_LIMIT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bss_assolmt" },
	/* XXX bintval chosen to avoid 16-char limit */
	{ IEEE80211_PARAM_BEACON_INTERVAL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bintval" },
	{ IEEE80211_PARAM_BEACON_INTERVAL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bintval" },
	{ IEEE80211_PARAM_DOTH,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth" },
	{ IEEE80211_PARAM_DOTH,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_doth" },
	{ IEEE80211_PARAM_PWRCONSTRAINT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth_pwrcst" },
	{ IEEE80211_PARAM_PWRCONSTRAINT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_doth_pwrcst" },
	{ IEEE80211_PARAM_GENREASSOC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth_reassoc" },
#ifdef MATS
	{ IEEE80211_PARAM_COMPRESSION,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "compression" },
	{ IEEE80211_PARAM_COMPRESSION,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_compression" },
	{ IEEE80211_PARAM_FF,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ff" },
	{ IEEE80211_PARAM_FF,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ff" },
	{ IEEE80211_PARAM_TURBO,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "turbo" },
	{ IEEE80211_PARAM_TURBO,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_turbo" },
	{ IEEE80211_PARAM_XR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "xr" },
	{ IEEE80211_PARAM_XR,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_xr" },
	{ IEEE80211_PARAM_BURST,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "burst" },
	{ IEEE80211_PARAM_BURST,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_burst" },
#endif
	{ IEEE80211_IOCTL_CHANSWITCH,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0,	"doth_chanswitch" },
	{ IEEE80211_PARAM_PUREG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pureg" },
	{ IEEE80211_PARAM_PUREG,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_pureg" },
	{ IEEE80211_PARAM_REPEATER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "repeater" },
	{ IEEE80211_PARAM_REPEATER,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_repeater" },
	{ IEEE80211_PARAM_WDS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wds" },
	{ IEEE80211_PARAM_WDS,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wds" },
	{ IEEE80211_PARAM_BGSCAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bgscan" },
	{ IEEE80211_PARAM_BGSCAN,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bgscan" },
	{ IEEE80211_PARAM_BGSCAN_IDLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bgscanidle" },
	{ IEEE80211_PARAM_BGSCAN_IDLE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bgscanidle" },
	{ IEEE80211_PARAM_BGSCAN_INTERVAL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bgscanintvl" },
	{ IEEE80211_PARAM_BGSCAN_INTERVAL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bgscanintvl" },
	{ IEEE80211_PARAM_MCAST_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcast_rate" },
	{ IEEE80211_PARAM_MCAST_RATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcast_rate" },
	{ IEEE80211_PARAM_COVERAGE_CLASS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "coverageclass" },
	{ IEEE80211_PARAM_COVERAGE_CLASS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_coveragecls" },
	{ IEEE80211_PARAM_COUNTRY_IE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "countryie" },
	{ IEEE80211_PARAM_COUNTRY_IE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_countryie" },
	{ IEEE80211_PARAM_SCANVALID,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scanvalid" },
	{ IEEE80211_PARAM_SCANVALID,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scanvalid" },
	{ IEEE80211_PARAM_REGCLASS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "regclass" },
	{ IEEE80211_PARAM_REGCLASS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_regclass" },
	{ IEEE80211_PARAM_DROPUNENC_EAPOL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dropunenceapol" },
	{ IEEE80211_PARAM_DROPUNENC_EAPOL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dropunencea" },
	{ IEEE80211_PARAM_SHPREAMBLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "shpreamble" },
	{ IEEE80211_PARAM_SHPREAMBLE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_shpreamble" },
	/*
	 * NB: these should be roamrssi* etc, but iwpriv usurps all
	 *     strings that start with roam!
	 */
	{ IEEE80211_PARAM_ROAM_RSSI_11A,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi11a" },
	{ IEEE80211_PARAM_ROAM_RSSI_11A,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rssi11a" },
	{ IEEE80211_PARAM_ROAM_RSSI_11B,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi11b" },
	{ IEEE80211_PARAM_ROAM_RSSI_11B,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rssi11b" },
	{ IEEE80211_PARAM_ROAM_RSSI_11G,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi11g" },
	{ IEEE80211_PARAM_ROAM_RSSI_11G,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rssi11g" },
	{ IEEE80211_PARAM_ROAM_RATE_11A,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate11a" },
	{ IEEE80211_PARAM_ROAM_RATE_11A,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate11a" },
	{ IEEE80211_PARAM_ROAM_RATE_11B,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate11b" },
	{ IEEE80211_PARAM_ROAM_RATE_11B,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate11b" },
	{ IEEE80211_PARAM_ROAM_RATE_11G,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate11g" },
	{ IEEE80211_PARAM_ROAM_RATE_11G,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate11g" },
	{ IEEE80211_PARAM_UAPSDINFO,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "uapsd" },
	{ IEEE80211_PARAM_UAPSDINFO,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_uapsd" },
	{ IEEE80211_PARAM_SLEEP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sleep" },
	{ IEEE80211_PARAM_SLEEP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sleep" },
	{ IEEE80211_PARAM_QOSNULL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "qosnull" },
	{ IEEE80211_PARAM_PSPOLL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pspoll" },
	{ IEEE80211_PARAM_EOSPDROP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "eospdrop" },
	{ IEEE80211_PARAM_EOSPDROP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_eospdrop" },
	{ IEEE80211_PARAM_STA_DFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sta_dfs"},
	{ IEEE80211_PARAM_STA_DFS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sta_dfs"},
	{ IEEE80211_PARAM_MARKDFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "markdfs" },
	{ IEEE80211_PARAM_MARKDFS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_markdfs" },
	{ IEEE80211_PARAM_RADAR_BW,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "radar_bw" },
	{ IEEE80211_IOCTL_SET_APPIEBUF,
	  IW_PRIV_TYPE_APPIEBUF, 0, "setiebuf" },
	{ IEEE80211_IOCTL_GET_APPIEBUF,
	  0, IW_PRIV_TYPE_APPIEBUF, "getiebuf" },
	{ IEEE80211_IOCTL_FILTERFRAME,
	  IW_PRIV_TYPE_FILTER , 0, "setfilter" },
	{ IEEE80211_PARAM_FIXED_TX_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "fixedtxrate" },
	{ IEEE80211_PARAM_FIXED_TX_RATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_fixedtxrate" },
	{ IEEE80211_PARAM_MIMOMODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mimomode" },
	{ IEEE80211_PARAM_MIMOMODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mimomode" },
	{ IEEE80211_PARAM_AGGREGATION,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aggregation" },
	{ IEEE80211_PARAM_AGGREGATION,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aggregation" },
	{ IEEE80211_PARAM_RETRY_COUNT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "retrycount" },
	{ IEEE80211_PARAM_RETRY_COUNT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_retrycount" },
	{ IEEE80211_PARAM_VAP_DBG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vapdebug" },
	{ IEEE80211_PARAM_VAP_DBG,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vapdebug" },
	{ IEEE80211_PARAM_NODEREF_DBG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "noderef_dbg" },
	{ IEEE80211_PARAM_EXP_MAT_SEL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "expmattype" },
	{ IEEE80211_PARAM_EXP_MAT_SEL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_expmattype" },
	{ IEEE80211_PARAM_BW_SEL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bwselect" },
	{ IEEE80211_PARAM_BW_SEL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bwselect" },
	{ IEEE80211_PARAM_RG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rgselect" },
	{ IEEE80211_PARAM_RG,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rgselect" },
	{ IEEE80211_PARAM_BW_SEL_MUC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bwselect_muc" },
	{ IEEE80211_PARAM_BW_SEL_MUC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bwselect_muc" },
	{ IEEE80211_PARAM_ACK_POLICY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ackpolicy" },
	{ IEEE80211_PARAM_ACK_POLICY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ackpolicy" },
	{ IEEE80211_PARAM_LEGACY_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "legacyselect" },
	{ IEEE80211_PARAM_LEGACY_MODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_legacyselect" },
	{ IEEE80211_PARAM_MAX_AGG_SIZE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "max_aggsize" },
	{ IEEE80211_PARAM_MAX_AGG_SIZE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_maxaggsize" },
	{ IEEE80211_PARAM_TXBF_CTRL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "txbf_ctrl" },
	{ IEEE80211_PARAM_TXBF_CTRL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_txbfctrl" },
	{ IEEE80211_PARAM_TXBF_PERIOD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "txbf_period" },
	{ IEEE80211_PARAM_TXBF_PERIOD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_txbfperiod" },
	{ IEEE80211_PARAM_HTBA_SEQ_CTRL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "htba_seq" },
	{ IEEE80211_PARAM_HTBA_SEQ_CTRL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_htba_seq" },
	{ IEEE80211_PARAM_HTBA_SIZE_CTRL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "htba_size" },
	{ IEEE80211_PARAM_HTBA_SIZE_CTRL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_htba_size" },
	{ IEEE80211_PARAM_HTBA_TIME_CTRL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "htba_time" },
	{ IEEE80211_PARAM_HTBA_TIME_CTRL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_htba_time" },
	{ IEEE80211_PARAM_HT_ADDBA,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "htba_addba" },
	{ IEEE80211_PARAM_HT_ADDBA,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_htba_addba" },
	{ IEEE80211_PARAM_HT_DELBA,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "htba_delba" },
	{ IEEE80211_PARAM_HT_DELBA,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_htba_delba" },
	{ IEEE80211_PARAM_CHANNEL_NOSCAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "disablescan" },
	{ IEEE80211_PARAM_CHANNEL_NOSCAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_disablescan" },
	{ IEEE80211_PARAM_MUC_PROFILE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "muc_profile" },
	{ IEEE80211_PARAM_MUC_PROFILE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "muc_profile" },
	{ IEEE80211_PARAM_MUC_PHY_STATS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "muc_set_phystat" },
	{ IEEE80211_PARAM_MUC_PHY_STATS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "muc_get_phystat" },
	{ IEEE80211_PARAM_MUC_SET_PARTNUM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "muc_set_partnum" },
	{ IEEE80211_PARAM_MUC_SET_PARTNUM,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "muc_get_partnum" },
	{ IEEE80211_PARAM_ENABLE_GAIN_ADAPT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ena_gain_adapt" },
	{ IEEE80211_PARAM_ENABLE_GAIN_ADAPT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_gain_adapt" },
	{ IEEE80211_PARAM_GET_RFCHIP_ID,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rfchipid" },
	{ IEEE80211_PARAM_GET_RFCHIP_ID,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rfchipid" },
	{ IEEE80211_PARAM_GET_RFCHIP_VERID,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rfchip_verid" },
	{ IEEE80211_PARAM_GET_RFCHIP_VERID,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rfchip_verid" },
	{ IEEE80211_PARAM_SHORT_GI,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "short_gi" },
	{ IEEE80211_PARAM_SHORT_GI,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_short_gi" },
	{ IEEE80211_PARAM_FORCE_SMPS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "forcesmps" },
	{ IEEE80211_PARAM_FORCEMICERROR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "forcemicerror" },
	{ IEEE80211_PARAM_ENABLECOUNTERMEASURES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "en_cmsures" },
	{ IEEE80211_PARAM_IMPLICITBA,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "implicit_ba" },
	{ IEEE80211_PARAM_IMPLICITBA,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_implicit_ba" },
	{ IEEE80211_PARAM_CLIENT_REMOVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cl_remove" },
	{ IEEE80211_PARAM_SHOWMEM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "showmem" },
	{ IEEE80211_PARAM_SCANSTATUS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scanstatus" },
	{ IEEE80211_PARAM_SCANSTATUS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scanstatus" },
	{ IEEE80211_PARAM_CACSTATUS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cacstatus" },
	{ IEEE80211_PARAM_GLOBAL_BA_CONTROL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ba_control" },
	{ IEEE80211_PARAM_GLOBAL_BA_CONTROL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ba_control" },
	{ IEEE80211_PARAM_NO_SSID_ASSOC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "no_ssid_assoc" },
	{ IEEE80211_PARAM_CONFIG_TXPOWER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cfg_txpower" },
	{ IEEE80211_PARAM_INITIATE_TXPOWER_TABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "init_txpower" },
	{ IEEE80211_PARAM_CONFIG_BW_TXPOWER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cfg_bw_power" },
	{ IEEE80211_PARAM_CONFIG_TPC_INTERVAL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tpc_intvl" },
	{ IEEE80211_PARAM_CONFIG_TPC_INTERVAL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tpc_interval" },
	{ IEEE80211_PARAM_TPC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tpc" },
	{ IEEE80211_PARAM_TPC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tpc" },
	{ IEEE80211_PARAM_TPC_QUERY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tpc_query" },
	{ IEEE80211_PARAM_TPC_QUERY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tpc_query" },
	{ IEEE80211_PARAM_CONFIG_REGULATORY_TXPOWER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cfg_reg_txpower" },
	{ IEEE80211_PARAM_SKB_LIST_MAX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "skb_list_max" },
	{ IEEE80211_PARAM_VAP_STATS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "vapstats" },
	{ IEEE80211_PARAM_RATE_CTRL_FLAGS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate_ctrl_flags" },
	{ IEEE80211_PARAM_LDPC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ldpc" },
	{ IEEE80211_PARAM_LDPC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ldpc" },
	{ IEEE80211_PARAM_DFS_FAST_SWITCH,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dfs_fast_switch" },
	{ IEEE80211_PARAM_DFS_FAST_SWITCH,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dfs_switch" },
	{ IEEE80211_PARAM_BLACKLIST_GET,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_blacklist" },
	{ IEEE80211_PARAM_SCAN_NO_DFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scan_no_dfs" },
	{ IEEE80211_PARAM_SCAN_NO_DFS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scan_dfs" },
	{ IEEE80211_PARAM_SAMPLE_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sample_rate" },
	{ IEEE80211_PARAM_SAMPLE_RATE, 0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sample_rate" },
	{ IEEE80211_PARAM_11N_40_ONLY_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_40only_mode" },
	{ IEEE80211_PARAM_11N_40_ONLY_MODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_40only_mode" },
	{ IEEE80211_PARAM_AMPDU_DENSITY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ampdu_dens" },
	{ IEEE80211_PARAM_AMPDU_DENSITY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ampdu_dens" },
	{ IEEE80211_PARAM_REGULATORY_REGION,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "region" },
	{ IEEE80211_PARAM_REGULATORY_REGION,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_region" },
	{ IEEE80211_PARAM_SPEC_COUNTRY_CODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "country_code" },
	{ IEEE80211_PARAM_SPEC_COUNTRY_CODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_country_code" },
	{ IEEE80211_PARAM_MCS_CAP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcs_cap" },
	{ IEEE80211_PARAM_MCS_CAP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcs_cap" },
	{ IEEE80211_PARAM_MAX_MGMT_FRAMES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "max_mgmtfrms" },
	{ IEEE80211_PARAM_MAX_MGMT_FRAMES,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_max_mgtfrms" },
	{ IEEE80211_PARAM_MCS_ODD_EVEN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcs_odd_even" },
	{ IEEE80211_PARAM_MCS_ODD_EVEN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcs_oddeven" },
	{ IEEE80211_PARAM_BA_MAX_WIN_SIZE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ba_max" },
	{ IEEE80211_PARAM_BA_MAX_WIN_SIZE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ba_max" },
	{ IEEE80211_PARAM_RESTRICTED_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tx_restrict" },
	{ IEEE80211_PARAM_RESTRICTED_MODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tx_restrict" },
	{ IEEE80211_PARAM_PHY_STATS_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mode_phy_stats" },
	{ IEEE80211_PARAM_MIN_DWELL_TIME_ACTIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "min_dt_act" },
	{ IEEE80211_PARAM_MIN_DWELL_TIME_ACTIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_min_dt_act" },
	{ IEEE80211_PARAM_MIN_DWELL_TIME_PASSIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "min_dt_pas" },
	{ IEEE80211_PARAM_MIN_DWELL_TIME_PASSIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_min_dt_pas" },
	{ IEEE80211_PARAM_MAX_DWELL_TIME_ACTIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "max_dt_act" },
	{ IEEE80211_PARAM_MAX_DWELL_TIME_ACTIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_max_dt_act" },
	{ IEEE80211_PARAM_MAX_DWELL_TIME_PASSIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "max_dt_pas" },
	{ IEEE80211_PARAM_MAX_DWELL_TIME_PASSIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_max_dt_pas" },
#ifdef QTN_BG_SCAN
	{ IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_ACTIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_dt_act" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_ACTIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_dt_act" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_PASSIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_dt_pas" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_PASSIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_dt_pas" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_ACTIVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_dur_act" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_ACTIVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_dur_act" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_FAST,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_dur_pf" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_FAST,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_dur_pf" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_NORMAL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_dur_pn" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_NORMAL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_dur_pn" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_SLOW,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_dur_ps" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DURATION_PASSIVE_SLOW,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_dur_ps" },
	{ IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_FAST,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_thr_fst" },
	{ IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_FAST,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_thr_fst" },
	{ IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_NORMAL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_thr_nor" },
	{ IEEE80211_PARAM_QTN_BGSCAN_THRSHLD_PASSIVE_NORMAL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_thr_nor" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DEBUG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bg_debug" },
	{ IEEE80211_PARAM_QTN_BGSCAN_DEBUG,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bg_debug" },
#endif /* QTN_BG_SCAN */
	{ IEEE80211_PARAM_LEGACY_RETRY_LIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "legacy_retry" },
	{ IEEE80211_PARAM_LEGACY_RETRY_LIMIT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_legacyretry" },
#ifdef QSCS_ENABLED
	{ IEEE80211_PARAM_SCS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scs_set" },
	{ IEEE80211_PARAM_SCS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "scs_get" },
	{ IEEE80211_PARAM_SCS_DFS_REENTRY_REQUEST,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "scs_get_reent" },
#endif /* QSCS_ENABLED */
	{ IEEE80211_PARAM_TRAINING_COUNT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "training_count" },
	{ IEEE80211_PARAM_DYNAMIC_AC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dynamic_ac" },
	{ IEEE80211_PARAM_DUMP_TRIGGER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbg_dump" },
	{ IEEE80211_PARAM_DUMP_TCM_FD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbg_dump_tcm_fd" },
	{ IEEE80211_PARAM_RXCSR_ERR_ALLOW,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxcsr_err_allow" },
	{ IEEE80211_PARAM_STOP_FLAGS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbg_stop" },
	{ IEEE80211_PARAM_CHECK_FLAGS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbg_check" },
	{ IEEE80211_PARAM_RX_CTRL_FILTER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ctrl_filter" },
	{ IEEE80211_PARAM_ALT_CHAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_alt_chan" },
	{ IEEE80211_PARAM_ALT_CHAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_alt_chan" },
	{ IEEE80211_PARAM_QTN_BCM_WAR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bcm_fixup" },
	{ IEEE80211_PARAM_GI_SELECT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gi_select" },
	{ IEEE80211_PARAM_GI_SELECT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_gi_select" },
	{ IEEE80211_PARAM_FIXED_SGI,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gi_fixed" },
	{ IEEE80211_PARAM_FIXED_SGI,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_gi_fixed" },
	{ IEEE80211_PARAM_FIXED_BW,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bw_fixed" },
	{ IEEE80211_PARAM_FIXED_BW,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bw_fixed" },
	{ IEEE80211_PARAM_LDPC_ALLOW_NON_QTN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ldpc_nonqtn" },
	{ IEEE80211_PARAM_LDPC_ALLOW_NON_QTN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ldpc_nonqtn" },
	{ IEEE80211_PARAM_FWD_UNKNOWN_MC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "fwd_unknown_mc" },
	{ IEEE80211_PARAM_FWD_UNKNOWN_MC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_unknown_mc" },
	{ IEEE80211_PARAM_BCST_4,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "reliable_bc" },
	{ IEEE80211_PARAM_BCST_4,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_reliable_bc" },
	{ IEEE80211_PARAM_AP_FWD_LNCB,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ap_fwd_lncb" },
	{ IEEE80211_PARAM_AP_FWD_LNCB,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ap_fwd_lncb" },
	{ IEEE80211_PARAM_PPPC_SELECT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pppc" },
	{ IEEE80211_PARAM_PPPC_SELECT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_pppc" },
	{ IEEE80211_PARAM_PPPC_STEP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pppc_step" },
	{ IEEE80211_PARAM_PPPC_STEP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_pppc_step" },
	{ IEEE80211_PARAM_TEST_LNCB,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "test_lncb" },
	{ IEEE80211_PARAM_STBC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_stbc" },
	{ IEEE80211_PARAM_STBC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_stbc" },
	{ IEEE80211_PARAM_RTS_CTS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rtscts" },
	{ IEEE80211_PARAM_RTS_CTS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rtscts" },
	{ IEEE80211_PARAM_TX_QOS_SCHED,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_txqos_sched" },
	{ IEEE80211_PARAM_TX_QOS_SCHED,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_txqos_sched" },
	{ IEEE80211_PARAM_GET_DFS_CCE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dfs_cce" },
#ifdef QSCS_ENABLED
	{ IEEE80211_PARAM_GET_SCS_CCE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scs_cce" },
#endif /* QSCS_ENABLED */
	{ IEEE80211_PARAM_RX_AGG_TIMEOUT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rx_agg_to" },
	{ IEEE80211_PARAM_RX_AGG_TIMEOUT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rx_agg_to" },
	{ IEEE80211_PARAM_FORCE_MUC_HALT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "muc_halt" },
	{ IEEE80211_PARAM_FORCE_ENABLE_TRIGGERS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tr_trig" },
	{ IEEE80211_PARAM_FORCE_MUC_TRACE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "muc_tb" },
	{ IEEE80211_PARAM_BK_BITMAP_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_bkbitmap" },
	{ IEEE80211_PARAM_PROBE_RES_RETRIES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "probe_rt" },
	{ IEEE80211_PARAM_MUC_FLAGS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "muc_flags" },
	{ IEEE80211_PARAM_HT_NSS_CAP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ht_nss_cap" },
	{ IEEE80211_PARAM_HT_NSS_CAP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ht_nss_cap" },
	{ IEEE80211_PARAM_VHT_NSS_CAP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vht_nss_cap" },
	{ IEEE80211_PARAM_VHT_NSS_CAP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vht_nss_cap" },
	{ IEEE80211_PARAM_UNKNOWN_DEST_ARP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "unknown_dst_arp" },
	{ IEEE80211_PARAM_UNKNOWN_DEST_FWD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "unknown_dst_fwd" },
	{ IEEE80211_PARAM_ASSOC_HISTORY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "reset_assoc_his" },
	{ IEEE80211_PARAM_CSW_RECORD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "clean_csw" },

	{ IEEE80211_PARAM_UPDATE_MU_GRP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_grp_upd" },
	{ IEEE80211_PARAM_FIXED_11AC_MU_TX_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_tx_rate_set" },
	{ IEEE80211_PARAM_MU_DEBUG_LEVEL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_dbg_lvl_set" },
	{ IEEE80211_PARAM_MU_ENABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_enable_set" },
	{ IEEE80211_PARAM_MU_ENABLE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "mu_enable_get" },
	{ IEEE80211_PARAM_INST_MU_GRP_QMAT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_grp_qmt_inst" },
	{ IEEE80211_PARAM_DELE_MU_GRP_QMAT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_grp_qmt_del" },
	{ IEEE80211_PARAM_GET_MU_GRP_QMAT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "mu_grp_qmt_get" },
	{ IEEE80211_PARAM_EN_MU_GRP_QMAT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_grp_qmt_ena" },
	{ IEEE80211_PARAM_DIS_MU_GRP_QMAT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_grp_qmt_dis" },
	{ IEEE80211_PARAM_MU_DEBUG_FLAG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_dbg_flg_set" },
	{ IEEE80211_PARAM_MU_AIRTIME_PADDING,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_airtime_pad" },
	{ IEEE80211_PARAM_DSP_DEBUG_LEVEL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dsp_dbg_lvl_set" },
	{ IEEE80211_PARAM_DSP_DEBUG_FLAG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dsp_dbg_flg_set" },
	{ IEEE80211_PARAM_SET_CRC_ERR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_crc_error" },
	{ IEEE80211_PARAM_MU_SWITCH_USR_POS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_sw_usr_pos" },
	{ IEEE80211_PARAM_SET_GRP_SND_PERIOD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_grp_snd_per" },
	{ IEEE80211_PARAM_SET_PREC_SND_PERIOD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_prec_snd_per" },
	{ IEEE80211_PARAM_SET_MU_RANK_TOLERANCE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_rank_toleran" },
	{ IEEE80211_PARAM_DSP_PRECODING_ALGORITHM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dsp_prc_alg_set" },
	{ IEEE80211_PARAM_DSP_RANKING_ALGORITHM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dsp_rnk_alg_set" },
	{ IEEE80211_PARAM_MU_USE_EQ,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_set_use_eq" },
	{ IEEE80211_PARAM_MU_USE_EQ,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "mu_get_use_eq" },
	{ IEEE80211_PARAM_MU_AMSDU_SIZE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mu_amsdu_size" },
#if defined(QBMPS_ENABLE)
	{ IEEE80211_PARAM_STA_BMPS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_bmps" },
	{ IEEE80211_PARAM_STA_BMPS,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bmps" },
#endif

	{ IEEE80211_IOCTL_GETBLOCK,
	  0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IW_PRIV_BLOCK_DATASIZE, "" },
	/* sub-ioctl */
	{ IEEE80211_PARAM_ASSOC_HISTORY,
	  0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IW_PRIV_BLOCK_DATASIZE, "assoc_history" },
	{ IEEE80211_PARAM_CSW_RECORD,
	  0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IW_PRIV_BLOCK_DATASIZE, "get_csw_record" },
	{ IEEE80211_PARAM_RESTRICT_RTS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "restrict_rts" },
	{ IEEE80211_PARAM_RESTRICT_RTS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rstrict_rts" },
	{ IEEE80211_PARAM_RESTRICT_LIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "restrict_max" },
	{ IEEE80211_PARAM_RESTRICT_LIMIT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rstrict_max" },
	{ IEEE80211_PARAM_SWRETRY_AGG_MAX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "swret_agg" },
	{ IEEE80211_PARAM_SWRETRY_AGG_MAX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_swret_agg" },
	{ IEEE80211_PARAM_SWRETRY_NOAGG_MAX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "swret_noagg" },
	{ IEEE80211_PARAM_SWRETRY_NOAGG_MAX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_swret_noagg" },
	{ IEEE80211_PARAM_CCA_PRI,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_cca_pri" },
	{ IEEE80211_PARAM_CCA_SEC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_cca_sec" },
	{ IEEE80211_PARAM_CCA_SEC40,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_cca_sec40" },
	{ IEEE80211_PARAM_CCA_FIXED,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_cca_fixed" },
	{ IEEE80211_PARAM_PWR_SAVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pm" },
	{ IEEE80211_PARAM_PWR_SAVE,
	  0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IW_PRIV_BLOCK_DATASIZE, "get_pm" },
	{ IEEE80211_PARAM_PS_CMD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ps_cmd" },
	{ IEEE80211_PARAM_FAST_REASSOC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "fast_reassoc" },
	{ IEEE80211_PARAM_TEST_TRAFFIC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "test_traffic" },
	{ IEEE80211_PARAM_QCAT_STATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "qcat_state" },
	{ IEEE80211_PARAM_RALG_DBG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ralg_dbg" },
	{ IEEE80211_PARAM_CSA_FLAG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "csa_flag" },
	{ IEEE80211_PARAM_CSA_FLAG,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_csa_flag" },
	{ IEEE80211_PARAM_DEF_MATRIX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "def_matrix" },
	{ IEEE80211_PARAM_DEF_MATRIX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_def_matrix" },
	{ IEEE80211_PARAM_TUNEPD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tunepd" },
	{ IEEE80211_PARAM_TUNEPD_DONE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tunepd_done" },
	{ IEEE80211_PARAM_RTSTHRESHOLD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rtshold" },
	{ IEEE80211_PARAM_CARRIER_ID,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "carrier_id" },
	{ IEEE80211_PARAM_CARRIER_ID,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_carrier_id" },
	{ IEEE80211_PARAM_BA_THROT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ba_throt" },
	{ IEEE80211_PARAM_TX_QUEUING_ALG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "queuing_alg" },
	{ IEEE80211_PARAM_TX_QUEUING_ALG,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_queuing_alg" },
	{ IEEE80211_PARAM_WME_THROT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wme_throt" },
	{ IEEE80211_PARAM_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vht" },
	{ IEEE80211_PARAM_MODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vht" },
	{ IEEE80211_PARAM_ENABLE_11AC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "enable_11ac" },
	{ IEEE80211_PARAM_ENABLE_11AC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_11ac_status" },
	{ IEEE80211_PARAM_FIXED_11AC_TX_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_11ac_mcs" },
	{ IEEE80211_PARAM_FIXED_11AC_TX_RATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_11ac_mcs" },
	{ IEEE80211_PARAM_AUC_TX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auc_tx" },
	{ IEEE80211_PARAM_FIXED_11AC_MU_TX_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_mu_mcs" },
	{ IEEE80211_PARAM_AUC_RX_DBG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auc_rx_dbg" },
	{ IEEE80211_PARAM_AUC_TX_DBG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auc_tx_dbg" },
	{ IEEE80211_PARAM_RX_ACCELERATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rx_accel" },
	{ IEEE80211_PARAM_RX_ACCEL_LOOKUP_SA,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rx_accel_lu_sa" },
	{ IEEE80211_PARAM_TACMAP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tacmap" },
	{ IEEE80211_PARAM_VAP_PRI,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_pri" },
	{ IEEE80211_PARAM_VAP_PRI,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vap_pri" },
	{ IEEE80211_PARAM_AIRFAIR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "airfair" },
	{ IEEE80211_PARAM_AIRFAIR,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_airfair" },
	{ IEEE80211_PARAM_AUC_QOS_SCH,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auc_qos_sch" },
	{ IEEE80211_PARAM_VAP_PRI_WME,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_pri_wme" },
	{ IEEE80211_PARAM_EMI_POWER_SWITCHING,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "emi_pwr_sw" },
	{ IEEE80211_PARAM_EMI_POWER_SWITCHING,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_emi_pwr" },
	{ IEEE80211_PARAM_AGGRESSIVE_AGG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aggressive_agg" },
	{ IEEE80211_PARAM_TQEW_DESCR_LIMIT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tqew_descrs" },
	{ IEEE80211_PARAM_TQEW_DESCR_LIMIT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tqew_descrs" },
	{ IEEE80211_PARAM_BR_IP_ADDR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "br_ip" },
	{ IEEE80211_PARAM_GENPCAP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "genpcap" },
	{ IEEE80211_PARAM_TDLS_DISC_INT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_disc_int" },
	{ IEEE80211_PARAM_TDLS_PATH_SEL_WEIGHT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_path_wgt" },
	{ IEEE80211_PARAM_TDLS_MIN_RSSI,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_min_rssi" },
	{ IEEE80211_PARAM_TDLS_SWITCH_INTS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_swit_ints" },
	{ IEEE80211_PARAM_TDLS_RATE_WEIGHT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_rate_wgt" },
	{ IEEE80211_PARAM_TDLS_OFF_CHAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_offchan" },
	{ IEEE80211_PARAM_TDLS_OFF_CHAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tdls_offchan" },
	{ IEEE80211_PARAM_TDLS_OFF_CHAN_BW,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls_offchbw" },
	{ IEEE80211_PARAM_TDLS_OFF_CHAN_BW,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tdls_offchbw" },
	{ IEEE80211_PARAM_OCAC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ocac_set" },
	{ IEEE80211_PARAM_OCAC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "ocac_get" },
	{ IEEE80211_PARAM_SDFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sdfs_set" },
	{ IEEE80211_PARAM_SDFS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "sdfs_get" },
	{ IEEE80211_PARAM_DEACTIVE_CHAN_PRI,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "deact_chan_pri" },
	{ IEEE80211_PARAM_RESTRICT_RATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "restrict_rt" },
	{ IEEE80211_PARAM_RESTRICT_RATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_restrict_rt" },
	{ IEEE80211_PARAM_TRAINING_START,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "training_restart" },
    { IEEE80211_PARAM_VCO_LOCK_DETECT_MODE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vco_lock" },
    { IEEE80211_PARAM_VCO_LOCK_DETECT_MODE,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vco_lock" },
	{ IEEE80211_PARAM_CONFIG_PMF,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pmf_set" },
	{ IEEE80211_PARAM_CONFIG_PMF,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "pmf_get" },
	{ IEEE80211_PARAM_SCAN_CANCEL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scan_cancel" },
	{ IEEE80211_PARAM_AUTO_CCA_ENABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auto_cca_enable" },
	{ IEEE80211_PARAM_AUTO_CCA_PARAMS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auto_cca_param" },
	{ IEEE80211_PARAM_AUTO_CCA_DEBUG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auto_cca_dbg" },
	{ IEEE80211_PARAM_AUTO_CS_ENABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "autocs_enable" },
	{ IEEE80211_PARAM_AUTO_CS_PARAMS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "autocs_param" },
	{ IEEE80211_PARAM_INTRA_BSS_ISOLATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "intra_bss" },
	{ IEEE80211_PARAM_INTRA_BSS_ISOLATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_intra_bss" },
	{ IEEE80211_PARAM_BSS_ISOLATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bss_isolate" },
	{ IEEE80211_PARAM_BSS_ISOLATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bss_iso" },
	{ IEEE80211_PARAM_BF_RX_STS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bf_rxsts" },
	{ IEEE80211_PARAM_BF_RX_STS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bf_rxsts" },
	{ IEEE80211_PARAM_PC_OVERRIDE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pc_override" },
	{ IEEE80211_PARAM_PC_OVERRIDE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_pc_override" },
	{ IEEE80211_PARAM_WOWLAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wowlan_set" },
	{ IEEE80211_PARAM_WOWLAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "wowlan_get" },
	{ IEEE80211_PARAM_RX_AMSDU_ENABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rx_amsdu" },
	{ IEEE80211_PARAM_RX_AMSDU_ENABLE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rx_amsdu" },
	{ IEEE80211_PARAM_DISASSOC_REASON,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "disassoc_reason" },
	{ IEEE80211_PARAM_PEER_RTS_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "peer_rts" },
	{ IEEE80211_PARAM_PEER_RTS_MODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_peer_rts" },
	{ IEEE80211_PARAM_DYN_WMM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dyn_wmm" },
	{ IEEE80211_PARAM_DYN_WMM,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dyn_wmm" },
	{ IEEE80211_PARAM_BA_SETUP_ENABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi_for_ba_set" },
        { IEEE80211_PARAM_BB_PARAM,
          IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_bb_param" },
        { IEEE80211_PARAM_BB_PARAM,
          0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bb_param" },
	{ IEEE80211_PARAM_VAP_TX_AMSDU,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_txamsdu" },
	{ IEEE80211_PARAM_VAP_TX_AMSDU,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vap_txamsdu" },
	{ IEEE80211_PARAM_CS_THRESHOLD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cs_thres" },
	{ IEEE80211_PARAM_CS_THRESHOLD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cs_thres" },
	{ IEEE80211_PARAM_CS_THRESHOLD_DBM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cs_thres_dbm" },
	{ IEEE80211_PARAM_SCAN_RESULTS_CHECK_INV,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scan_inv" },
	{ IEEE80211_PARAM_SCAN_RESULTS_CHECK_INV,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_scan_inv" },
	{ IEEE80211_PARAM_SFS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sfs" },
	{ IEEE80211_PARAM_INST_1SS_DEF_MAT_ENABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "def_1ss_mat_en" },
	{ IEEE80211_PARAM_INST_1SS_DEF_MAT_THRESHOLD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "def_1ss_mat_th" },
	{ IEEE80211_PARAM_SWFEAT_DISABLE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "swfeat_disable"},
	{ IEEE80211_PARAM_FLUSH_SCAN_ENTRY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "flush_scan" },
	{ IEEE80211_PARAM_SCAN_OPCHAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_scan_opch" },
	{ IEEE80211_PARAM_SCAN_OPCHAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scan_opch" },
	{ IEEE80211_PARAM_DUMP_PPPC_TX_SCALE_BASES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dump_scale_base" },
	{ IEEE80211_PARAM_GLOBAL_FIXED_TX_SCALE_INDEX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tx_scale_index" },
	{ IEEE80211_PARAM_QTN_HAL_PM_CORRUPT_DEBUG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pm_corrupt_debug"},
	{ IEEE80211_PARAM_L2_EXT_FILTER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "l2_ext_filter" },
	{ IEEE80211_PARAM_L2_EXT_FILTER,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_l2_ext_filt" },
        { IEEE80211_PARAM_ENABLE_RX_OPTIM_STATS,
          IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_optim_stats" },
	{ IEEE80211_PARAM_SET_UNICAST_QUEUE_NUM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "unicast_qcnt" },
	{ IEEE80211_PARAM_OBSS_EXEMPT_REQ,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "obss_exe_req" },
	{ IEEE80211_PARAM_OBSS_TRIGG_SCAN_INT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_obss_int" },
	{ IEEE80211_PARAM_OBSS_TRIGG_SCAN_INT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_obss_int" },
	{ IEEE80211_PARAM_ALLOW_VHT_TKIP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vht_tkip" },
	{ IEEE80211_PARAM_ALLOW_VHT_TKIP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vht_tkip" },
	{ IEEE80211_PARAM_VHT_2_4GHZ,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vht_24g" },
	{ IEEE80211_PARAM_VHT_2_4GHZ,0,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vht_24g" },
	{ IEEE80211_PARAM_BEACONING_SCHEME,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bcn_scheme" },
	{ IEEE80211_PARAM_BEACONING_SCHEME,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bcn_scheme" },
	{ IEEE80211_PARAM_40MHZ_INTOLERANT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "40mhz_intol" },
	{ IEEE80211_PARAM_40MHZ_INTOLERANT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_40mhz_intol" },
	{ IEEE80211_PARAM_VAP_STATE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_state" },
	{ IEEE80211_PARAM_VAP_STATE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vap_state" },
	{ IEEE80211_PARAM_ANTENNA_USAGE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_antenna_usg" },
	{ IEEE80211_PARAM_ANTENNA_USAGE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_antenna_usg" },
	{ IEEE80211_PARAM_SET_RTS_BW_DYN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rts_bw" },
	{ IEEE80211_PARAM_SET_RTS_BW_DYN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rts_bw" },
	{ IEEE80211_PARAM_SET_CTS_BW,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_cts_bw" },
	{ IEEE80211_PARAM_SET_CTS_BW,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cts_bw" },
	{ IEEE80211_PARAM_VHT_MCS_CAP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vht_mcs_cap" },
	{ IEEE80211_PARAM_VHT_MCS_CAP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vht_mcs_cap" },
	{ IEEE80211_PARAM_VHT_OPMODE_NOTIF,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vht_opmntf" },
	{ IEEE80211_PARAM_VHT_OPMODE_NOTIF,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vht_opmntf" },
	{ IEEE80211_PARAM_USE_NON_HT_DUPLICATE_MU,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_mu_non_ht" },
	{ IEEE80211_PARAM_USE_NON_HT_DUPLICATE_MU,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mu_non_ht" },
	{ IEEE80211_PARAM_BG_PROTECT,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "11g_protect" },
	{ IEEE80211_PARAM_BG_PROTECT, 0,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_11g_protect" },
	{ IEEE80211_PARAM_MU_NDPA_BW_SIGNALING_SUPPORT,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rx_bws_ndpa" },
	{ IEEE80211_PARAM_MU_NDPA_BW_SIGNALING_SUPPORT,
	 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rx_bws_ndpa" },
	{ IEEE80211_PARAM_RESTRICT_WLAN_IP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rstrict_wip" },
	{ IEEE80211_PARAM_RESTRICT_WLAN_IP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rstrict_wip" },
	{ IEEE80211_PARAM_MC_TO_UC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_mc_to_uc" },
	{ IEEE80211_PARAM_MC_TO_UC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mc_to_uc" },
	{ IEEE80211_PARAM_HOSTAP_STARTED,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hostap_state" },
	{ IEEE80211_PARAM_HOSTAP_STARTED,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_hostap_state" },
	{ IEEE80211_PARAM_WPA_STARTED,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wpa_state" },
	{ IEEE80211_PARAM_WPA_STARTED,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wpa_state" },
	{ IEEE80211_PARAM_EP_STATUS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ep_status" },
	{ IEEE80211_PARAM_MAX_BCAST_PPS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_bcast_pps" },
	{ IEEE80211_PARAM_BSS_GROUP_ID,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_ssid_grpid" },
	{ IEEE80211_PARAM_BSS_GROUP_ID,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ssid_grpid" },
	{ IEEE80211_PARAM_BSS_ASSOC_RESERVE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_assoc_rsrv" },
	{ IEEE80211_PARAM_BSS_ASSOC_RESERVE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_assoc_rsrv" },
	{ IEEE80211_PARAM_MAX_BOOT_CAC_DURATION,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_boot_cac" },
	{ IEEE80211_PARAM_RX_BAR_SYNC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rx_bar_sync" },
	{ IEEE80211_PARAM_RX_BAR_SYNC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rx_bar_sync" },
	{ IEEE80211_PARAM_GET_REG_DOMAIN_IS_EU,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_reg_is_eu" },
	{ IEEE80211_PARAM_AUC_TX_AGG_DURATION,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "auc_tx_agg_dur" },
	{ IEEE80211_PARAM_GET_CHAN_AVAILABILITY_STATUS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "chan_avail_stat" },
	{ IEEE80211_PARAM_STOP_ICAC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1 , 0, "stop_icac" },
	{ IEEE80211_PARAM_STA_DFS_STRICT_MODE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sta_dfs_strict" },
	{ IEEE80211_PARAM_STA_DFS_STRICT_MEASUREMENT_IN_CAC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sta_dfs_msr_cac" },
	{ IEEE80211_PARAM_STA_DFS_STRICT_TX_CHAN_CLOSE_TIME,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sta_dfs_end_tx" },
	{ IEEE80211_PARAM_NEIGHBORHOOD_THRSHD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,	"set_nbr_thrshd" },
	{ IEEE80211_PARAM_NEIGHBORHOOD_TYPE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,	"get_nbr_type" },
	{ IEEE80211_PARAM_NEIGHBORHOOD_COUNT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,	"get_nbr_cnt" },
	{ IEEE80211_PARAM_RADAR_NONOCCUPY_PERIOD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "radar_nop_time" },
	{ IEEE80211_PARAM_RADAR_NONOCCUPY_PERIOD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gt_radar_nop" },
	{ IEEE80211_PARAM_DFS_CSA_CNT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dfs_csa_cnt" },
	{ IEEE80211_PARAM_DFS_CSA_CNT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dfs_csa_cnt" },
	{ IEEE80211_PARAM_STA_DFS_STRICT_MODE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gt_stdfs_strict" },
	{ IEEE80211_PARAM_STA_DFS_STRICT_MEASUREMENT_IN_CAC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gt_stdfs_msrcac" },
	{ IEEE80211_PARAM_STA_DFS_STRICT_TX_CHAN_CLOSE_TIME,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gt_stdfs_end_tx" },
};

static const iw_handler ieee80211_handlers[] = {
	(iw_handler) NULL,				/* SIOCSIWCOMMIT */
	(iw_handler) ieee80211_ioctl_giwname,		/* SIOCGIWNAME */
	(iw_handler) NULL,				/* SIOCSIWNWID */
	(iw_handler) NULL,				/* SIOCGIWNWID */
	(iw_handler) ieee80211_ioctl_siwfreq,		/* SIOCSIWFREQ */
	(iw_handler) ieee80211_ioctl_giwfreq,		/* SIOCGIWFREQ */
	(iw_handler) ieee80211_ioctl_siwmode,		/* SIOCSIWMODE */
	(iw_handler) ieee80211_ioctl_giwmode,		/* SIOCGIWMODE */
	(iw_handler) ieee80211_ioctl_siwsens,		/* SIOCSIWSENS */
	(iw_handler) ieee80211_ioctl_giwsens,		/* SIOCGIWSENS */
	(iw_handler) NULL /* not used */,		/* SIOCSIWRANGE */
	(iw_handler) ieee80211_ioctl_giwrange,		/* SIOCGIWRANGE */
	(iw_handler) NULL /* not used */,		/* SIOCSIWPRIV */
	(iw_handler) NULL /* kernel code */,		/* SIOCGIWPRIV */
	(iw_handler) NULL /* not used */,		/* SIOCSIWSTATS */
	(iw_handler) NULL /* kernel code */,		/* SIOCGIWSTATS */
	(iw_handler) ieee80211_ioctl_setspy,		/* SIOCSIWSPY */
	(iw_handler) ieee80211_ioctl_getspy,		/* SIOCGIWSPY */
	(iw_handler) ieee80211_ioctl_setthrspy,		/* SIOCSIWTHRSPY */
	(iw_handler) ieee80211_ioctl_getthrspy,		/* SIOCGIWTHRSPY */
	(iw_handler) ieee80211_ioctl_siwap,		/* SIOCSIWAP */
	(iw_handler) ieee80211_ioctl_giwap,		/* SIOCGIWAP */
#ifdef SIOCSIWMLME
	(iw_handler) ieee80211_ioctl_siwmlme,		/* SIOCSIWMLME */
#else
	(iw_handler) NULL,				/* -- hole -- */
#endif
	(iw_handler) ieee80211_ioctl_iwaplist,		/* SIOCGIWAPLIST */
#ifdef SIOCGIWSCAN
	(iw_handler) ieee80211_ioctl_siwscan,		/* SIOCSIWSCAN */
	(iw_handler) ieee80211_ioctl_giwscan,		/* SIOCGIWSCAN */
#else
	(iw_handler) NULL,				/* SIOCSIWSCAN */
	(iw_handler) NULL,				/* SIOCGIWSCAN */
#endif /* SIOCGIWSCAN */
	(iw_handler) ieee80211_ioctl_siwessid,		/* SIOCSIWESSID */
	(iw_handler) ieee80211_ioctl_giwessid,		/* SIOCGIWESSID */
	(iw_handler) ieee80211_ioctl_siwnickn,		/* SIOCSIWNICKN */
	(iw_handler) ieee80211_ioctl_giwnickn,		/* SIOCGIWNICKN */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) ieee80211_ioctl_siwrate,		/* SIOCSIWRATE */
	(iw_handler) ieee80211_ioctl_giwrate,		/* SIOCGIWRATE */
	(iw_handler) ieee80211_ioctl_siwrts,		/* SIOCSIWRTS */
	(iw_handler) ieee80211_ioctl_giwrts,		/* SIOCGIWRTS */
	(iw_handler) ieee80211_ioctl_siwfrag,		/* SIOCSIWFRAG */
	(iw_handler) ieee80211_ioctl_giwfrag,		/* SIOCGIWFRAG */
	(iw_handler) ieee80211_ioctl_siwtxpow,		/* SIOCSIWTXPOW */
	(iw_handler) ieee80211_ioctl_giwtxpow,		/* SIOCGIWTXPOW */
	(iw_handler) ieee80211_ioctl_siwretry,		/* SIOCSIWRETRY */
	(iw_handler) ieee80211_ioctl_giwretry,		/* SIOCGIWRETRY */
	(iw_handler) ieee80211_ioctl_siwencode,		/* SIOCSIWENCODE */
	(iw_handler) ieee80211_ioctl_giwencode,		/* SIOCGIWENCODE */
	(iw_handler) ieee80211_ioctl_siwpower,		/* SIOCSIWPOWER */
	(iw_handler) ieee80211_ioctl_giwpower,		/* SIOCGIWPOWER */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) ieee80211_ioctl_siwgenie,		/* SIOCSIWGENIE */
	(iw_handler) ieee80211_ioctl_giwgenie,		/* SIOCGIWGENIE */
	(iw_handler) ieee80211_ioctl_siwauth,		/* SIOCSIWAUTH */
	(iw_handler) ieee80211_ioctl_giwauth,		/* SIOCGIWAUTH */
	(iw_handler) ieee80211_ioctl_siwencodeext,	/* SIOCSIWENCODEEXT */
	(iw_handler) ieee80211_ioctl_giwencodeext,	/* SIOCGIWENCODEEXT */
};

static const iw_handler ieee80211_priv_handlers[] = {
	(iw_handler) ieee80211_ioctl_setparam,		/* SIOCIWFIRSTPRIV+0 */
	(iw_handler) ieee80211_ioctl_getparam,		/* SIOCIWFIRSTPRIV+1 */
	(iw_handler) ieee80211_ioctl_setmode,		/* SIOCIWFIRSTPRIV+2 */
	(iw_handler) ieee80211_ioctl_getmode,		/* SIOCIWFIRSTPRIV+3 */
	(iw_handler) ieee80211_ioctl_setwmmparams,	/* SIOCIWFIRSTPRIV+4 */
	(iw_handler) ieee80211_ioctl_getwmmparams,	/* SIOCIWFIRSTPRIV+5 */
	(iw_handler) ieee80211_ioctl_setchanlist,	/* SIOCIWFIRSTPRIV+6 */
	(iw_handler) ieee80211_ioctl_getchanlist,	/* SIOCIWFIRSTPRIV+7 */
	(iw_handler) ieee80211_ioctl_chanswitch,	/* SIOCIWFIRSTPRIV+8 */
	(iw_handler) ieee80211_ioctl_getappiebuf,	/* SIOCIWFIRSTPRIV+9 */
	(iw_handler) ieee80211_ioctl_setappiebuf,	/* SIOCIWFIRSTPRIV+10 */
	(iw_handler) ieee80211_ioctl_getscanresults,	/* SIOCIWFIRSTPRIV+11 */
	(iw_handler) ieee80211_ioctl_setfilter,		/* SIOCIWFIRSTPRIV+12 */
	(iw_handler) ieee80211_ioctl_getchaninfo,	/* SIOCIWFIRSTPRIV+13 */
	(iw_handler) ieee80211_ioctl_setoptie,		/* SIOCIWFIRSTPRIV+14 */
	(iw_handler) ieee80211_ioctl_getoptie,		/* SIOCIWFIRSTPRIV+15 */
	(iw_handler) ieee80211_ioctl_setmlme,		/* SIOCIWFIRSTPRIV+16 */
	(iw_handler) ieee80211_ioctl_radar,		/* SIOCIWFIRSTPRIV+17 */
	(iw_handler) ieee80211_ioctl_setkey,		/* SIOCIWFIRSTPRIV+18 */
	(iw_handler) ieee80211_ioctl_postevent,		/* SIOCIWFIRSTPRIV+19 */
	(iw_handler) ieee80211_ioctl_delkey,		/* SIOCIWFIRSTPRIV+20 */
	(iw_handler) ieee80211_ioctl_txeapol,		/* SIOCIWFIRSTPRIV+21 */
	(iw_handler) ieee80211_ioctl_addmac,		/* SIOCIWFIRSTPRIV+22 */
	(iw_handler) ieee80211_ioctl_startcca,		/* SIOCIWFIRSTPRIV+23 */
	(iw_handler) ieee80211_ioctl_delmac,		/* SIOCIWFIRSTPRIV+24 */
#if defined(CONFIG_QTN_80211K_SUPPORT)
	(iw_handler) ieee80211_ioctl_getstastatistic,	/* SIOCIWFIRSTPRIV+25 */
#else
	(iw_handler) NULL,				/* SIOCIWFIRSTPRIV+25 */
#endif
	(iw_handler) ieee80211_ioctl_wdsmac,		/* SIOCIWFIRSTPRIV+26 */
	(iw_handler) NULL,				/* SIOCIWFIRSTPRIV+27 */
	(iw_handler) ieee80211_ioctl_wdsdelmac,		/* SIOCIWFIRSTPRIV+28 */
	(iw_handler) ieee80211_ioctl_getblockdata,	/* SIOCIWFIRSTPRIV+29 */
	(iw_handler) ieee80211_ioctl_kickmac,		/* SIOCIWFIRSTPRIV+30 */
	(iw_handler) ieee80211_ioctl_dfsactscan,	/* SIOCIWFIRSTPRIV+31 */
};
static struct iw_handler_def ieee80211_iw_handler_def = {
#define	N(a)	(sizeof (a) / sizeof (a[0]))
	.standard		= (iw_handler *) ieee80211_handlers,
	.num_standard		= N(ieee80211_handlers),
	.private		= (iw_handler *) ieee80211_priv_handlers,
	.num_private		= N(ieee80211_priv_handlers),
	.private_args		= (struct iw_priv_args *) ieee80211_priv_args,
	.num_private_args	= N(ieee80211_priv_args),
#if IW_HANDLER_VERSION >= 7
	.get_wireless_stats	= ieee80211_iw_getstats,
#endif
#undef N
};

static	void ieee80211_delete_wlanunit(u_int);

/**
 * The interface stats and local node's stats are reset here
 * */
int ieee80211_rst_dev_stats(struct ieee80211vap *vap)
{
	struct net_device_stats *stats = &vap->iv_devstats;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni, *next;
	struct ieee80211_node_table *nt = &ic->ic_sta;

	/*The interface's statics will be cleared*/
	stats->rx_packets = 0;
	watch64_reset(&stats->rx_packets, NULL);

	stats->tx_packets = 0;
	watch64_reset(&stats->tx_packets, NULL);

	stats->rx_bytes = 0;
	watch64_reset(&stats->rx_bytes, NULL);

	stats->tx_bytes = 0;
	watch64_reset(&stats->tx_bytes, NULL);

	stats->rx_unicast_packets = 0;
	watch64_reset(&stats->rx_unicast_packets, NULL);

	stats->tx_unicast_packets = 0;
	watch64_reset(&stats->tx_unicast_packets, NULL);

	stats->tx_multicast_packets = 0;
	stats->multicast = 0;
	stats->rx_broadcast_packets = 0;
	stats->tx_broadcast_packets = 0;
	stats->rx_unknown_packets = 0;
	stats->rx_dropped = 0;

	vap->iv_stats.is_tx_nodefkey = 0;
	vap->iv_stats.is_tx_noheadroom = 0;
	vap->iv_stats.is_crypto_enmicfail = 0;
	stats->tx_errors = 0;

	vap->iv_stats.is_tx_nobuf = 0;
	vap->iv_stats.is_tx_nonode = 0;
	vap->iv_stats.is_tx_unknownmgt = 0;
	vap->iv_stats.is_tx_badcipher = 0;
	vap->iv_stats.is_tx_nodefkey = 0;
	stats->tx_dropped = 0;

	vap->iv_stats.is_rx_tooshort = 0;
	vap->iv_stats.is_rx_wepfail = 0;
	vap->iv_stats.is_rx_decap = 0;
	vap->iv_stats.is_rx_nobuf = 0;
	vap->iv_stats.is_rx_decryptcrc = 0;
	vap->iv_stats.is_rx_ccmpmic = 0;
	vap->iv_stats.is_rx_tkipmic = 0;
	vap->iv_stats.is_rx_tkipicv = 0;
	stats->rx_errors = 0;

	ic->ic_reset_shared_vap_stats(vap);

	/* The statics of local nodes in the node table will be cleared */
	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
		if (!IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr)){
			memset(&ni->ni_stats, 0, sizeof(struct ieee80211_nodestats));
			ic->ic_reset_shared_node_stats(ni);
		}
	}
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	return 0;
}

/*
 * Handle private ioctl requests.
 */
static int
ieee80211_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct ieee80211vap *vap = netdev_priv(dev);
	u_int unit;

	switch (cmd) {
	case SIOCG80211STATS:
		return copy_to_user(ifr->ifr_data, &vap->iv_stats,
			sizeof (vap->iv_stats)) ? -EFAULT : 0;
	case SIOCR80211STATS:
		return ieee80211_rst_dev_stats(vap);
	case SIOC80211IFDESTROY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		ieee80211_stop(vap->iv_dev);	/* force state before cleanup */
		unit = vap->iv_unit;
		vap->iv_ic->ic_vap_delete(vap);
		return 0;
	case IEEE80211_IOCTL_GETKEY:
		return ieee80211_ioctl_getkey(dev, (struct iwreq *) ifr);
	case IEEE80211_IOCTL_GETWPAIE:
		return ieee80211_ioctl_getwpaie(dev, (struct iwreq *) ifr);
	case IEEE80211_IOCTL_STA_STATS:
		return ieee80211_ioctl_getstastats(dev, (struct iwreq *) ifr);
	case IEEE80211_IOCTL_STA_INFO:
		return ieee80211_ioctl_getstainfo(dev, (struct iwreq *) ifr);
	case IEEE80211_IOCTL_SCAN_RESULTS:
		return ieee80211_ioctl_getscanresults(dev, (struct iwreq *)ifr);
	case IEEE80211_IOCTL_GET_ASSOC_TBL:
		return ieee80211_ioctl_getassoctbl(dev, (struct iwreq *)ifr);
	case IEEE80211_IOCTL_GET_RATES:
		return ieee80211_ioctl_get_rates(dev, (struct iwreq *)ifr);
	case IEEE80211_IOCTL_SET_RATES:
		return ieee80211_ioctl_set_rates(dev, (struct iwreq *)ifr);
	case IEEE80211_IOCTL_EXT:
		return ieee80211_ioctl_ext(dev, (struct iwreq *)ifr);
	}
	return -EOPNOTSUPP;
}

static u_int8_t wlan_units[32];		/* enough for 256 */

/*
 * Allocate a new unit number.  If the map is full return -1;
 * otherwise the allocate unit number is returned.
 */
static int
ieee80211_new_wlanunit(void)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	u_int unit;
	u_int8_t b;
	int i;

	/* NB: covered by rtnl_lock */
	unit = 0;
	for (i = 0; i < N(wlan_units) && wlan_units[i] == 0xff; i++)
		unit += NBBY;
	if (i == N(wlan_units))
		return -1;
	for (b = wlan_units[i]; b & 1; b >>= 1)
		unit++;
	setbit(wlan_units, unit);

	return unit;
#undef N
}

/*
 * Reclaim the specified unit number.
 */
static void
ieee80211_delete_wlanunit(u_int unit)
{
	/* NB: covered by rtnl_lock */
	KASSERT(unit < sizeof(wlan_units) * NBBY, ("invalid wlan unit %u", unit));
	KASSERT(isset(wlan_units, unit), ("wlan unit %u not allocated", unit));
	clrbit(wlan_units, unit);
}

/*
 * Create a virtual ap.  This is public as it must be implemented
 * outside our control (e.g. in the driver).
 */
int
ieee80211_ioctl_create_vap(struct ieee80211com *ic, struct ifreq *ifr, struct net_device *mdev)
{
	struct ieee80211_clone_params cp;
	struct ieee80211vap *vap;
	char name[IFNAMSIZ];
	int unit;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	if (copy_from_user(&cp, ifr->ifr_data, sizeof(cp)))
		return -EFAULT;

	unit = ieee80211_new_wlanunit();
	if (unit == -1)
		return -EIO;		/* XXX */
	memcpy(name, cp.icp_name, IFNAMSIZ - 1);
	name[IFNAMSIZ - 1] = '\0';

	vap = ic->ic_vap_create(ic, name, unit, cp.icp_opmode, cp.icp_flags, mdev);
	if (vap == NULL) {
		ieee80211_delete_wlanunit(unit);
		return -EIO;
	}
	/* return final device name */
	memcpy(ifr->ifr_name, vap->iv_dev->name, IFNAMSIZ - 1);
	ifr->ifr_name[IFNAMSIZ - 1] = '\0';
	return 0;
}
EXPORT_SYMBOL(ieee80211_ioctl_create_vap);

/*
 * Create a virtual ap.  This is public as it must be implemented
 * outside our control (e.g. in the driver).
 * Must be called with rtnl_lock held
 */
int
ieee80211_create_vap(struct ieee80211com *ic, char *name,
	struct net_device *mdev, int opmode, int opflags)
{
	int unit;

	if ((unit = ieee80211_new_wlanunit()) == -1)
		return -EIO;		/* XXX */

	if (!ic->ic_vap_create(ic, name, unit, opmode, opflags, mdev)) {
		ieee80211_delete_wlanunit(unit);
		return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL(ieee80211_create_vap);


void
ieee80211_ioctl_vattach(struct ieee80211vap *vap)
{
	struct net_device *dev = vap->iv_dev;
	struct net_device_ops *pndo = (struct net_device_ops *)dev->netdev_ops;

	pndo->ndo_do_ioctl = ieee80211_ioctl;
#if IW_HANDLER_VERSION < 7
	dev->get_wireless_stats = ieee80211_iw_getstats;
#endif
	dev->wireless_handlers = &ieee80211_iw_handler_def;
}

void
ieee80211_ioctl_vdetach(struct ieee80211vap *vap)
{
	if ((vap->iv_unit != -1) && isset(wlan_units, vap->iv_unit))
		ieee80211_delete_wlanunit(vap->iv_unit);
}

/* Input function to send a message via the wireless_event kernel mechanism. */
void
ieee80211_dot11_msg_send(struct ieee80211vap *vap,
			const char *mac_bssid,
			const char *message,
			const char *message_code,
			int message_reason,
			const char *message_description,
			const char *auth,
			const char *crypto)
{
	char buf[384];
	char mac[6] = {'0', '0', '0', '0', '0', '0'};
	const char *tag = QEVT_COMMON_PREFIX;
	buf[sizeof(buf)-1] = '\0';

	/* Ensure we always have a valid address */
	if (mac_bssid)
	{
		IEEE80211_ADDR_COPY(mac, mac_bssid);
	}

	/* Message includes a reason code - disassoc, deauth to/from AP/STA */
	if (message_reason >= 0) {
		ieee80211_eventf(vap->iv_dev, "%s%s [" DBGMAC "] [%s - %d - %s]",
			tag,
			message,
			ETHERFMT(mac),
			message_code ? message_code : "no code",
			message_reason,
			message_description ? message_description : "no description"
			);

	}
	/* STA/AP connected including auth details */
	else if (auth) {
		ieee80211_eventf(vap->iv_dev, "%s%s [" DBGMAC "] [%s/%s]",
			tag,
			message,
			ETHERFMT(mac),
			auth,
			crypto ? crypto : "unspecified"
			);
	}
	/* Further details included in the message */
	else if (message_description) {
		ieee80211_eventf(vap->iv_dev, "%s%s [" DBGMAC "] [%s - %s]",
			tag,
			message,
			ETHERFMT(mac),
			message_code ? message_code : "no code",
			message_description
			);
	}
	/* Single descriptive text */
	else {
		ieee80211_eventf(vap->iv_dev, "%s%s [" DBGMAC "] [%s]",
			tag,
			message,
			ETHERFMT(mac),
			message_code ? message_code : "no code"
			);
	}
}
EXPORT_SYMBOL(ieee80211_dot11_msg_send);

/* Dot11Msg messages */
char *d11_m[] = {
	"Client connected",
	"Client disconnected",
	"Client authentication failed",
	"Client removed",
	"Connected to AP",
	"Connection to AP failed",
	"Disconnected from AP",
};
EXPORT_SYMBOL(d11_m);

/* Dot11Msg details */
char *d11_c[] = {
	"Disassociated",
	"Deauthenticated",
	"TKIP countermeasures invoked",
	"Client timeout",
	"WPA password failure",
	"WPA timeout",
	"Beacon loss",
	"Client sent deauthentication",
	"Client sent disassociation"
};
EXPORT_SYMBOL(d11_c);

/* Dot11Msg reason fields - directly taken from the Reason field in the 802.11 spec(s). */
char *d11_r[] = {
	"Reserved",
	"Unspecified reason",
	"Previous authentication no longer valid",
	"Deauthenticated because sending STA is leaving (or has left) IBSS or ESS",
	"Disassociated due to inactivity",
	"Disassociated because AP is unable to handle all currently associated STAs",
	"Class 2 frame received from nonauthenticated STA",
	"Class 3 frame received from nonassociated STA",
	"Disassociated because sending STA is leaving (or has left) BSS",
	"STA requesting (re)association is not authenticated with responding STA",
	"Disassociated because the information in the Power Capability element is unacceptable",
	"Disassociated because the information in the Supported Channels element is unacceptable",
	"Reserved",
	"Invalid information element",
	"Message integrity code (MIC) failure",
	"4-Way Handshake timeout",
	"Group Key Handshake timeout",
	"Information element in 4-Way Handshake different from (Re)Association Request/Probe Response/Beacon frame",
	"Invalid group cipher",
	"Invalid pairwise cipher",
	"Invalid AKMP",
	"Unsupported RSN information element version",
	"Invalid RSN information element capabilities",
	"IEEE 802.1X authentication failed",
	"Cipher suite rejected because of the security policy",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"TS deleted because QoS AP lacks sufficient bandwidth due to change in BSS or operational mode",
	"Disassociated for unspecified, QoS-related reason",
	"Disassociated because QoS AP lacks sufficient bandwidth for this QoS STA",
	"Disassociated because excessive number of frames need to be acknowledged, but are not acknowledged"
	"Disassociated because STA is transmitting outside the limits of its TXOPs",
	"Requested from peer STA as the STA is leaving hte BSS (or resetting)",
	"Requested from peer STA as it does not want to use the mechanism",
	"Requested from peer STA as the STA received frames using the mechanism for which a setup is required",
	"Requested from peer STA due to timeout",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Peer STA does not support the requested cipher suite"
};
EXPORT_SYMBOL(d11_r);

/*
 * hostapd - driver interaction with MADWIFI 802.11 driver
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, Video54 Technologies
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This driver wrapper is only for hostapd AP mode functionality. Station
 * (wpa_supplicant) operations with madwifi are supported by the driver_wext.c
 * wrapper.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "wireless_copy.h"

#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ieee802_11_auth.h"
#include "ap/wps_hostapd.h"
#include "qtn_hapd/qtn_hapd_bss.h"
#include "qtn_hapd/qtn_hapd_pp.h"
/*
 * Avoid conflicts with wpa_supplicant definitions by undefining a definition.
 */
#undef WME_OUI_TYPE
#undef WPA_OUI_TYPE

#include <include/compat.h>
#include <net80211/ieee80211.h>
#ifdef WME_NUM_AC
/* Assume this is built against BSD branch of madwifi driver. */
#define MADWIFI_BSD
#include <net80211/_ieee80211.h>
#endif /* WME_NUM_AC */
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
#include <netpacket/packet.h>

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW 0x0019
#endif
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */

/*
 * Avoid conflicts with hostapd definitions by undefining couple of defines
 * from madwifi header files.
 */
#undef RSN_VERSION
#undef WPA_VERSION
#undef WPA_OUI_TYPE
#undef WME_OUI_TYPE


#ifdef IEEE80211_IOCTL_SETWMMPARAMS
/* Assume this is built against madwifi-ng */
#define MADWIFI_NG
#endif /* IEEE80211_IOCTL_SETWMMPARAMS */

#define WPA_KEY_RSC_LEN 8

#include "priv_netlink.h"
#include "netlink.h"
#include "linux_ioctl.h"
#include "l2_packet/l2_packet.h"
#include "utils/list.h"

#define MADWIFI_CMD_BUF_SIZE		128
#define IEEE80211_IOCTL_POSTEVENT	(SIOCIWFIRSTPRIV+19)
#define IEEE80211_IOCTL_TXEAPOL		(SIOCIWFIRSTPRIV+21)
#define MADWIFI_CMD_WDS_EXT_LEN		256

struct madwifi_bss {
	struct madwifi_driver_data *drv;
	struct dl_list list;
	void *bss_ctx;
	char ifname[IFNAMSIZ];
	int ifindex;
	u8 bssid[ETH_ALEN];
	char brname[IFNAMSIZ];
	u8 acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;
	int ioctl_sock;	/* socket for ioctl() use */
	struct l2_packet_data *sock_xmit; /* raw packet xmit socket */
	struct l2_packet_data *sock_recv; /* raw packet recv socket */
	struct l2_packet_data *sock_raw; /* raw 802.11 management frames */
	struct netlink_data *netlink;
	int added_if_into_bridge;
};

struct madwifi_driver_data {
	struct dl_list bss;
	int	we_version;
	u8 *extended_capa;
	u8 *extended_capa_mask;
	u8 extended_capa_len;
};

static int madwifi_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
			      int reason_code);

static int
set80211priv(struct madwifi_bss *bss, int op, void *data, int len)
{
	struct iwreq iwr;
	int do_inline = len < IFNAMSIZ;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
#ifdef IEEE80211_IOCTL_FILTERFRAME
	/* FILTERFRAME must be NOT inline, regardless of size. */
	if (op == IEEE80211_IOCTL_FILTERFRAME)
		do_inline = 0;
#endif /* IEEE80211_IOCTL_FILTERFRAME */
	if ((op == IEEE80211_IOCTL_SET_APPIEBUF) ||
	    (op == IEEE80211_IOCTL_POSTEVENT) ||
	    (op == IEEE80211_IOCTL_TXEAPOL))
		do_inline = 0;
	if (do_inline) {
		/*
		 * Argument data fits inline; put it there.
		 */
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(bss->ioctl_sock, op, &iwr) < 0) {
#ifdef MADWIFI_NG
		int first = IEEE80211_IOCTL_SETPARAM;
		static const char *opnames[] = {
			"ioctl[IEEE80211_IOCTL_SETPARAM]",
			"ioctl[IEEE80211_IOCTL_GETPARAM]",
			"ioctl[IEEE80211_IOCTL_SETMODE]",
			"ioctl[IEEE80211_IOCTL_GETMODE]",
			"ioctl[IEEE80211_IOCTL_SETWMMPARAMS]",
			"ioctl[IEEE80211_IOCTL_GETWMMPARAMS]",
			"ioctl[IEEE80211_IOCTL_SETCHANLIST]",
			"ioctl[IEEE80211_IOCTL_GETCHANLIST]",
			"ioctl[IEEE80211_IOCTL_CHANSWITCH]",
			"ioctl[IEEE80211_IOCTL_GET_APPIEBUF]",
			"ioctl[IEEE80211_IOCTL_SET_APPIEBUF]",
			"ioctl[IEEE80211_IOCTL_GETSCANRESULTS]",
			"ioctl[IEEE80211_IOCTL_FILTERFRAME]",
			"ioctl[IEEE80211_IOCTL_GETCHANINFO]",
			"ioctl[IEEE80211_IOCTL_SETOPTIE]",
			"ioctl[IEEE80211_IOCTL_GETOPTIE]",
			"ioctl[IEEE80211_IOCTL_SETMLME]",
			"ioctl[IEEE80211_IOCTL_RADAR]",
			"ioctl[IEEE80211_IOCTL_SETKEY]",
			"ioctl[IEEE80211_IOCTL_POSTEVENT]",
			"ioctl[IEEE80211_IOCTL_DELKEY]",
			"ioctl[IEEE80211_IOCTL_TXEAPOL]",
			"ioctl[IEEE80211_IOCTL_ADDMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_DELMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_WDSMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_WDSDELMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_KICKMAC]",
		};
#else /* MADWIFI_NG */
		int first = IEEE80211_IOCTL_SETPARAM;
		static const char *opnames[] = {
			"ioctl[IEEE80211_IOCTL_SETPARAM]",
			"ioctl[IEEE80211_IOCTL_GETPARAM]",
			"ioctl[IEEE80211_IOCTL_SETKEY]",
			"ioctl[SIOCIWFIRSTPRIV+3]",
			"ioctl[IEEE80211_IOCTL_DELKEY]",
			"ioctl[SIOCIWFIRSTPRIV+5]",
			"ioctl[IEEE80211_IOCTL_SETMLME]",
			"ioctl[SIOCIWFIRSTPRIV+7]",
			"ioctl[IEEE80211_IOCTL_SETOPTIE]",
			"ioctl[IEEE80211_IOCTL_GETOPTIE]",
			"ioctl[IEEE80211_IOCTL_ADDMAC]",
			"ioctl[SIOCIWFIRSTPRIV+11]",
			"ioctl[IEEE80211_IOCTL_DELMAC]",
			"ioctl[SIOCIWFIRSTPRIV+13]",
			"ioctl[IEEE80211_IOCTL_CHANLIST]",
			"ioctl[SIOCIWFIRSTPRIV+15]",
			"ioctl[IEEE80211_IOCTL_GETRSN]",
			"ioctl[SIOCIWFIRSTPRIV+17]",
			"ioctl[IEEE80211_IOCTL_GETKEY]",
		};
#endif /* MADWIFI_NG */
		int idx = op - first;
		const char *ioctl_name = "ioctl[unknown???]";
		if (first <= op &&
		    idx < (int) ARRAY_SIZE(opnames) &&
		    opnames[idx]) {
			ioctl_name = opnames[idx];
		}
		wpa_printf(MSG_DEBUG, "%s: %s returned error %d: %s\n",
				__func__, ioctl_name, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static int
set80211param(struct madwifi_bss *bss, int op, int arg)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.mode = op;
	memcpy(iwr.u.name+sizeof(__u32), &arg, sizeof(arg));

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		perror("ioctl[IEEE80211_IOCTL_SETPARAM]");
		wpa_printf(MSG_DEBUG, "%s: Failed to set parameter (op %d "
			   "arg %d)", __func__, op, arg);
		return -1;
	}
	return 0;
}

#ifndef CONFIG_NO_STDOUT_DEBUG
static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}
#endif /* CONFIG_NO_STDOUT_DEBUG */

/*
 * Configure WPA parameters.
 */
static int
madwifi_configure_wpa(struct madwifi_bss *bss,
		      struct wpa_bss_params *params)
{
	int v;
	switch (params->wpa_group) {
	case WPA_CIPHER_CCMP:
		v = IEEE80211_CIPHER_AES_CCM;
		break;
	case WPA_CIPHER_TKIP:
		v = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_CIPHER_WEP104:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_WEP40:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_NONE:
		v = IEEE80211_CIPHER_NONE;
		break;
	default:
		wpa_printf(MSG_ERROR, "Unknown group key cipher %u",
			   params->wpa_group);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: group key cipher=%d", __func__, v);
	if (set80211param(bss, IEEE80211_PARAM_MCASTCIPHER, v)) {
		printf("Unable to set group key cipher to %u\n", v);
		return -1;
	}
	if (v == IEEE80211_CIPHER_WEP) {
		/* key length is done only for specific ciphers */
		v = (params->wpa_group == WPA_CIPHER_WEP104 ? 13 : 5);
		if (set80211param(bss, IEEE80211_PARAM_MCASTKEYLEN, v)) {
			printf("Unable to set group key length to %u\n", v);
			return -1;
		}
	}

	v = 0;
	if (params->wpa_pairwise & WPA_CIPHER_CCMP)
		v |= 1<<IEEE80211_CIPHER_AES_CCM;
	if (params->wpa_pairwise & WPA_CIPHER_TKIP)
		v |= 1<<IEEE80211_CIPHER_TKIP;
	if (params->wpa_pairwise & WPA_CIPHER_NONE)
		v |= 1<<IEEE80211_CIPHER_NONE;
	wpa_printf(MSG_DEBUG, "%s: pairwise key ciphers=0x%x", __func__, v);
	if (set80211param(bss, IEEE80211_PARAM_UCASTCIPHERS, v)) {
		printf("Unable to set pairwise key ciphers to 0x%x\n", v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: key management algorithms=0x%x",
		   __func__, params->wpa_key_mgmt);
	if (set80211param(bss, IEEE80211_PARAM_KEYMGTALGS,
			  params->wpa_key_mgmt)) {
		printf("Unable to set key management algorithms to 0x%x\n",
			params->wpa_key_mgmt);
		return -1;
	}

	v = 0;
	if (params->rsn_preauth)
		v |= BIT(0);
	wpa_printf(MSG_DEBUG, "%s: rsn capabilities=0x%x",
		   __func__, params->rsn_preauth);
	if (set80211param(bss, IEEE80211_PARAM_RSNCAPS, v)) {
		printf("Unable to set RSN capabilities to 0x%x\n", v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: enable WPA=0x%x", __func__, params->wpa);
	if (set80211param(bss, IEEE80211_PARAM_WPA, params->wpa)) {
		printf("Unable to set WPA to %u\n", params->wpa);
		return -1;
	}
	return 0;
}

static int
madwifi_set_ieee8021x(void *priv, struct wpa_bss_params *params)
{
	struct madwifi_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, params->enabled);

	if (!params->enabled) {
		/* XXX restore state */
		return set80211param(bss, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_AUTO);
	}
	if (!params->wpa && !params->ieee802_1x) {
		wpa_printf(MSG_WARNING, "No 802.1X or WPA enabled!");
		return -1;
	}
	if (params->wpa && madwifi_configure_wpa(bss, params) != 0) {
		wpa_printf(MSG_WARNING, "Error configuring WPA state!");
		return -1;
	}
	if (set80211param(bss, IEEE80211_PARAM_AUTHMODE,
		(params->wpa ? IEEE80211_AUTH_WPA : IEEE80211_AUTH_8021X))) {
		wpa_printf(MSG_WARNING, "Error enabling WPA/802.1X!");
		return -1;
	}
	if (set80211param(bss, IEEE80211_PARAM_CONFIG_PMF,
			(params->ieee80211w ? params->ieee80211w + 1 : 0))) {
		wpa_printf(MSG_WARNING, "Error enabling PMF");
		return -1;
	}

	return 0;
}

static int
madwifi_set_privacy(void *priv, int enabled)
{
	if (priv == NULL) {
		return 0;
	}

	struct madwifi_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	return set80211param(bss, IEEE80211_PARAM_PRIVACY, enabled);
}

static int
madwifi_set_sta_authorized(void *priv, const u8 *addr, int authorized)
{
	struct madwifi_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s authorized=%d",
		   __func__, ether_sprintf(addr), authorized);

	if (authorized)
		mlme.im_op = IEEE80211_MLME_AUTHORIZE;
	else
		mlme.im_op = IEEE80211_MLME_UNAUTHORIZE;
	mlme.im_reason = 0;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(bss, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to %sauthorize STA " MACSTR,
			   __func__, authorized ? "" : "un", MAC2STR(addr));
	}

	return ret;
}

static int
madwifi_brcm_info_ioctl(struct madwifi_bss *bss, void *data, int len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.data.flags = SIOCDEV_SUBIO_SET_BRCM_IOCTL;
	iwr.u.data.pointer = data;
	iwr.u.data.length = len;

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to do brcm info ioctl", __func__);
		return -1;
	}
	return 0;
}

static int
madwifi_sta_set_flags(void *priv, const u8 *addr,
		      int total_flags, int flags_or, int flags_and)
{
	/* For now, only support setting Authorized flag */
	if (flags_or & WPA_STA_AUTHORIZED)
		return madwifi_set_sta_authorized(priv, addr, 1);
	if (!(flags_and & WPA_STA_AUTHORIZED))
		return madwifi_set_sta_authorized(priv, addr, 0);
	return 0;
}

static int
madwifi_del_key(void *priv, const u8 *addr, int key_idx)
{
	struct madwifi_bss *bss = priv;
	struct ieee80211req_del_key wk;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s key_idx=%d",
		   __func__, ether_sprintf(addr), key_idx);

	memset(&wk, 0, sizeof(wk));
	if (addr != NULL) {
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (u8) IEEE80211_KEYIX_NONE;
	} else {
		wk.idk_keyix = key_idx;
		memset(wk.idk_macaddr, 0xff, IEEE80211_ADDR_LEN);
	}

	ret = set80211priv(bss, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to delete key (addr %s"
			   " key_idx %d)", __func__, ether_sprintf(addr),
			   key_idx);
	}

	return ret;
}

static int
wpa_driver_madwifi_set_key(const char *ifname, void *priv, enum wpa_alg alg,
			   const u8 *addr, int key_idx, int set_tx,
			   const u8 *seq, size_t seq_len,
			   const u8 *key, size_t key_len)
{
	struct madwifi_bss *bss = priv;
	struct ieee80211req_key wk;
	u_int8_t cipher;
	int ret;
	const char *vlanif;
	int vlanid;

	if (alg == WPA_ALG_NONE)
		return madwifi_del_key(bss, addr, key_idx);

	wpa_printf(MSG_DEBUG, "%s: alg=%d addr=%s key_idx=%d",
		   __func__, alg, ether_sprintf(addr), key_idx);
	wpa_hexdump_key(MSG_DEBUG, "set-key", key, key_len);

	if (alg == WPA_ALG_WEP)
		cipher = IEEE80211_CIPHER_WEP;
	else if (alg == WPA_ALG_TKIP)
		cipher = IEEE80211_CIPHER_TKIP;
	else if (alg == WPA_ALG_CCMP)
		cipher = IEEE80211_CIPHER_AES_CCM;
	else if (alg == WPA_ALG_IGTK)
		cipher = IEEE80211_CIPHER_AES_CMAC;
	else {
		printf("%s: unknown/unsupported algorithm %d\n",
			__func__, alg);
		return -1;
	}

	if (key_len > sizeof(wk.ik_keydata)) {
		printf("%s: key length %lu too big\n", __func__,
		       (unsigned long) key_len);
		return -3;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT;
	if (addr == NULL || is_broadcast_ether_addr(addr)) {
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_keyix = key_idx;
		wk.ik_flags |= IEEE80211_KEY_DEFAULT;
	} else {
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.ik_keyix = IEEE80211_KEYIX_NONE;
	}

	vlanif = strstr(ifname, "vlan");
	if (vlanif) {
		sscanf(ifname, "vlan%d", &vlanid);
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_vlan = (uint16_t)vlanid;
		wk.ik_flags |= (IEEE80211_KEY_VLANGROUP | IEEE80211_KEY_GROUP);
	}

	wk.ik_keylen = key_len;
	memcpy(wk.ik_keydata, key, key_len);

	ret = set80211priv(bss, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to set key (addr %s"
			   " key_idx %d alg %d key_len %lu set_tx %d)",
			   __func__, ether_sprintf(wk.ik_macaddr), key_idx,
			   alg, (unsigned long) key_len, set_tx);
	}

	return ret;
}


static int
madwifi_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx,
		   u8 *seq)
{
	struct madwifi_bss *bss = priv;
	struct ieee80211req_key wk;

	wpa_printf(MSG_DEBUG, "%s: addr=%s idx=%d",
		   __func__, ether_sprintf(addr), idx);

	memset(&wk, 0, sizeof(wk));
	if (addr == NULL)
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
	else
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	wk.ik_keyix = idx;

	if (set80211priv(bss, IEEE80211_IOCTL_GETKEY, &wk, sizeof(wk))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get encryption data "
			   "(addr " MACSTR " key_idx %d)",
			   __func__, MAC2STR(wk.ik_macaddr), idx);
		return -1;
	}

#ifdef WORDS_BIGENDIAN
	{
		/*
		 * wk.ik_keytsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
		u8 tmp[WPA_KEY_RSC_LEN];
		memcpy(tmp, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
		for (i = 0; i < WPA_KEY_RSC_LEN; i++) {
			seq[i] = tmp[WPA_KEY_RSC_LEN - i - 1];
		}
	}
#else /* WORDS_BIGENDIAN */
	memcpy(seq, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
#endif /* WORDS_BIGENDIAN */
	return 0;
}


static int 
madwifi_flush(void *priv)
{
#ifdef MADWIFI_BSD
	u8 allsta[IEEE80211_ADDR_LEN];
	memset(allsta, 0xff, IEEE80211_ADDR_LEN);
	return madwifi_sta_deauth(priv, NULL, allsta,
				  IEEE80211_REASON_AUTH_LEAVE);
#else /* MADWIFI_BSD */
	return 0;		/* XXX */
#endif /* MADWIFI_BSD */
}


static int
madwifi_read_sta_driver_data(void *priv, struct hostap_sta_driver_data *data,
			     const u8 *addr)
{
	struct madwifi_bss *bss = priv;

#ifdef MADWIFI_BSD
	struct ieee80211req_sta_stats stats;

	memset(data, 0, sizeof(*data));

	/*
	 * Fetch statistics for station from the system.
	 */
	memset(&stats, 0, sizeof(stats));
	memcpy(stats.is_u.macaddr, addr, IEEE80211_ADDR_LEN);
	if (set80211priv(bss,
#ifdef MADWIFI_NG
			 IEEE80211_IOCTL_STA_STATS,
#else /* MADWIFI_NG */
			 IEEE80211_IOCTL_GETSTASTATS,
#endif /* MADWIFI_NG */
			 &stats, sizeof(stats))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to fetch STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
		if (memcmp(addr, bss->acct_mac, ETH_ALEN) == 0) {
			memcpy(data, &bss->acct_data, sizeof(*data));
			return 0;
		}

		printf("Failed to get station stats information element.\n");
		return -1;
	}

	data->rx_packets = stats.is_stats.ns_rx_data;
	data->rx_bytes = stats.is_stats.ns_rx_bytes;
	data->tx_packets = stats.is_stats.ns_tx_data;
	data->tx_bytes = stats.is_stats.ns_tx_bytes;
	return 0;

#else /* MADWIFI_BSD */

	char buf[1024], line[128], *pos;
	FILE *f;
	unsigned long val;

	memset(data, 0, sizeof(*data));
	snprintf(buf, sizeof(buf), "/proc/net/madwifi/%s/" MACSTR,
		 bss->ifname, MAC2STR(addr));

	f = fopen(buf, "r");
	if (!f) {
		if (memcmp(addr, bss->acct_mac, ETH_ALEN) != 0)
			return -1;
		memcpy(data, &bss->acct_data, sizeof(*data));
		return 0;
	}
	/* Need to read proc file with in one piece, so use large enough
	 * buffer. */
	setbuffer(f, buf, sizeof(buf));

	while (fgets(line, sizeof(line), f)) {
		pos = strchr(line, '=');
		if (!pos)
			continue;
		*pos++ = '\0';
		val = strtoul(pos, NULL, 10);
		if (strcmp(line, "rx_packets") == 0)
			data->rx_packets = val;
		else if (strcmp(line, "tx_packets") == 0)
			data->tx_packets = val;
		else if (strcmp(line, "rx_bytes") == 0)
			data->rx_bytes = val;
		else if (strcmp(line, "tx_bytes") == 0)
			data->tx_bytes = val;
	}

	fclose(f);

	return 0;
#endif /* MADWIFI_BSD */
}


static int
madwifi_sta_clear_stats(void *priv, const u8 *addr)
{
#if defined(MADWIFI_BSD) && defined(IEEE80211_MLME_CLEAR_STATS)
	struct madwifi_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s", __func__, ether_sprintf(addr));

	mlme.im_op = IEEE80211_MLME_CLEAR_STATS;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(bss, IEEE80211_IOCTL_SETMLME, &mlme,
			   sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to clear STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
	}

	return ret;
#else /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
	return 0; /* FIX */
#endif /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
}


static int
madwifi_set_opt_ie(void *priv, const u8 *ie, size_t ie_len)
{
	/*
	 * Do nothing; we setup parameters at startup that define the
	 * contents of the beacon information element.
	 */
	return 0;
}

static int
madwifi_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
		   int reason_code)
{
	if (priv == NULL) {
		return 0;
	}

	struct madwifi_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	if (bss->ioctl_sock < 0)
		return -1;

	if (!linux_iface_up(bss->ioctl_sock, bss->ifname)) {
		wpa_printf(MSG_DEBUG, "%s: Interface is not up.", __func__);
		return 0;
	}

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(bss, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to deauth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int
madwifi_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
		     int reason_code)
{
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(priv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disassoc STA (addr "
			   MACSTR " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

#ifdef IEEE80211_IOCTL_FILTERFRAME
#ifdef CONFIG_WPS
/**
* return 0 if Probe request packet is received and handled
* return -1 if frame is not a probe request frame
*/
static int madwifi_raw_recv_probe_req(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
	struct madwifi_bss *bss = ctx;
	const struct ieee80211_mgmt *mgmt;
	u16 fc;
	union wpa_event_data event;

	/* Send Probe Request information to WPS processing */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req))
		return -1;
	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_PROBE_REQ)
		return -1;

	os_memset(&event, 0, sizeof(event));
	event.rx_probe_req.sa = mgmt->sa;
	event.rx_probe_req.da = mgmt->da;
	event.rx_probe_req.bssid = mgmt->bssid;
	event.rx_probe_req.ie = mgmt->u.probe_req.variable;
	event.rx_probe_req.ie_len =
		len - (IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req));
	wpa_supplicant_event(bss->bss_ctx, EVENT_RX_PROBE_REQ, &event);

	return 0;
}
#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20
static int madwifi_raw_recv_hs20(void *ctx, const u8 *src_addr, const u8 *buf,
                               size_t len)
{
	struct madwifi_bss *bss = ctx;
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	u16 fc;

	/* Send the Action frame for HS20 processing */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.action.category) +
			sizeof(mgmt->u.action.u.public_action))
		return -1;

	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
			WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_ACTION ||
			mgmt->u.action.category != WLAN_ACTION_PUBLIC)
		return -1;

	/*TODO We are handling Public Action Frames now, Dropping other types
	 * of action frames */
	mgmt = (const struct ieee80211_mgmt *) buf;
	wpa_printf(MSG_DEBUG, "%s:Received Public Action frame\n", __func__);

	os_memset(&event, 0, sizeof(event));
	event.rx_mgmt.frame = (const u8 *) mgmt;
	event.rx_mgmt.frame_len = len;
	wpa_supplicant_event(bss->bss_ctx, EVENT_RX_MGMT, &event);

	return 0;
}
#endif

static void madwifi_raw_receive(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
#ifdef CONFIG_WPS
	if (madwifi_raw_recv_probe_req(ctx, src_addr, buf, len) == 0)
		return;
#endif
#ifdef CONFIG_HS20
	madwifi_raw_recv_hs20(ctx, src_addr, buf, len);
#endif
}
#endif /* IEEE80211_IOCTL_FILTERFRAME */

static int madwifi_receive_pkt(struct madwifi_bss *bss)
{
	int ret = 0;
#ifdef IEEE80211_IOCTL_FILTERFRAME
	struct ieee80211req_set_filter filt;
	struct hostapd_data *hapd = bss->bss_ctx;

	wpa_printf(MSG_DEBUG, "%s Enter", __func__);

	filt.app_filterype = 0;
#ifdef CONFIG_WPS
	filt.app_filterype = IEEE80211_FILTER_TYPE_PROBE_REQ;
#endif /* CONFIG_WPS */
#ifdef CONFIG_HS20
	filt.app_filterype |= IEEE80211_FILTER_TYPE_ACTION;
#endif

	ret = set80211priv(bss, IEEE80211_IOCTL_FILTERFRAME, &filt,
			   sizeof(struct ieee80211req_set_filter));
	if (ret)
		return ret;

	bss->sock_raw = l2_packet_init(hapd->conf->bridge, NULL, ETH_P_80211_RAW,
				       madwifi_raw_receive, bss, 1);
	if (bss->sock_raw == NULL)
		return -1;
#endif /* IEEE80211_IOCTL_FILTERFRAME */
	return ret;
}

#ifdef CONFIG_WPS
static int
madwifi_set_wps_ie(void *priv, const u8 *ie, size_t len, u32 frametype)
{
	struct madwifi_bss *bss = priv;
	u8 buf[1024];
	struct ieee80211req_getset_appiebuf *beac_ie;

	if ((len + sizeof(*beac_ie)) > sizeof(buf)) {
		wpa_printf(MSG_ERROR, "%s WPS IE length %lu exceeds the buffer size %lu",
			__func__, (unsigned long)len, sizeof(buf));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s WPS IE length = %lu", __func__, (unsigned long)len);

	os_memset(buf, 0, sizeof(buf));
	beac_ie = (struct ieee80211req_getset_appiebuf *) buf;
	beac_ie->app_frmtype = frametype;
	beac_ie->app_buflen = len;
	os_memcpy(&(beac_ie->app_buf[0]), ie, len);

	return set80211priv(bss, IEEE80211_IOCTL_SET_APPIEBUF, beac_ie,
			    sizeof(struct ieee80211req_getset_appiebuf) + len);
}

static int
madwifi_set_ap_wps_ie(void *priv, const struct wpabuf *beacon,
		      const struct wpabuf *proberesp,
		      const struct wpabuf *assocresp)
{
	if (madwifi_set_wps_ie(priv, beacon ? wpabuf_head(beacon) : NULL,
			       beacon ? wpabuf_len(beacon) : 0,
			       IEEE80211_APPIE_FRAME_BEACON) < 0) {
		return -1;
	}
	if (madwifi_set_wps_ie(priv,
				  proberesp ? wpabuf_head(proberesp) : NULL,
				  proberesp ? wpabuf_len(proberesp) : 0,
				  IEEE80211_APPIE_FRAME_PROBE_RESP) < 0) {
		return -1;
	}
	return madwifi_set_wps_ie(priv,
				  assocresp ? wpabuf_head(assocresp) : NULL,
				  assocresp ? wpabuf_len(assocresp) : 0,
				  IEEE80211_APPIE_FRAME_ASSOC_RESP);
}
#else /* CONFIG_WPS */
#define madwifi_set_ap_wps_ie NULL
#endif /* CONFIG_WPS */

static int madwifi_write2_qdrv_control(const char *buf)
{
	static const char *const file = "/sys/devices/qdrv/control";

	FILE *qdrv_control;
	int ret;

	qdrv_control = fopen(file, "w");
	if (!qdrv_control)
		return -1;

	ret = fwrite(buf, strlen(buf), 1, qdrv_control);
	fclose(qdrv_control);

	if (!ret)
		return -1;

	return 0;
}

#ifndef CONFIG_NO_VLAN
static int madwifi_set_sta_vlan(void *priv, const u8 *addr,
			const char *ifname, int vlan_id)
{
	char mac[20];
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	wpa_printf(MSG_DEBUG, "%s: Bind %s with VLAN %d\n",
			ifname,
			mac,
			vlan_id);

	snprintf(buf, sizeof(buf), "set dyn-vlan %s %d\n", mac, vlan_id);

	ret = madwifi_write2_qdrv_control(buf);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "%s: Bind STA VLAN failed\n", ifname);
		return -1;
	}

	return 0;
}

static int madwifi_set_dyn_vlan(void *priv, const char *ifname, int enable)
{
	char buf[MADWIFI_CMD_BUF_SIZE];
	const char *cmd;
	int ret;

	cmd = (enable ? "enable" : "disable");
	wpa_printf(MSG_DEBUG, "%s: %s dynamic VLAN ", ifname, cmd);

	snprintf(buf, sizeof(buf), "set vlan %s %s 1", ifname, enable ? "dynamic" : "undynamic");

	ret = madwifi_write2_qdrv_control(buf);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "%s: %s dynamic VLAN failed\n", ifname, cmd);
		return -1;
	}

        return 0;
}

static int madwifi_vlan_group_add(void *priv, const char *ifname, int vlan_id)
{
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	snprintf(buf, sizeof(buf), "set vlan-group %s %d 1", ifname, vlan_id);

	ret = madwifi_write2_qdrv_control(buf);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "set vlan group %d failed\n", vlan_id);
		return -1;
	}

	return 0;
}

static int madwifi_vlan_group_remove(void *priv, const char *ifname, int vlan_id)
{
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	snprintf(buf, sizeof(buf), "set vlan-group %s %d 0", ifname, vlan_id);

	ret = madwifi_write2_qdrv_control(buf);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "set vlan group %d failed\n", vlan_id);
		return -1;
	}

	return 0;
}

#else /* CONFIG_NO_VLAN */
#define madwifi_set_sta_vlan		NULL
#define madwifi_set_dyn_vlan		NULL
#define madwifi_vlan_group_add		NULL
#define madwifi_vlan_group_remove	NULL
#endif /* CONFIG_NO_VLAN */

static int madwifi_set_freq(void *priv, struct hostapd_freq_params *freq)
{
	struct madwifi_bss *bss = priv;
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.freq.m = freq->channel;
	iwr.u.freq.e = 0;

	if (ioctl(bss->ioctl_sock, SIOCSIWFREQ, &iwr) < 0) {
		perror("ioctl[SIOCSIWFREQ]");
		return -1;
	}

	return 0;
}

static void
madwifi_new_sta(struct madwifi_bss *bss, u8 addr[IEEE80211_ADDR_LEN])
{
	struct ieee80211req_wpaie ie;
	int ielen = 0;
	u8 *iebuf = NULL;
	int res;
	struct hostapd_data *hapd = bss->bss_ctx;

	if (qtn_hapd_acl_reject(hapd, addr)){
		/* This reason code is used only by the driver, for blacklisting */
		res = madwifi_sta_disassoc(bss, hapd->own_addr, addr, WLAN_REASON_DENIED);
		if (res < 0) {
			wpa_printf(MSG_ERROR, "%s: Failed to disassociate STA (addr " MACSTR
				   " that is denied by MAC address ACLs)",
				   __func__, MAC2STR(addr));
		}
		return;
	}

	/*
	 * Fetch negotiated WPA/RSN parameters from the system.
	 */
	memset(&ie, 0, sizeof(ie));
	memcpy(ie.wpa_macaddr, addr, IEEE80211_ADDR_LEN);
	if (set80211priv(bss, IEEE80211_IOCTL_GETWPAIE, &ie, sizeof(ie))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get WPA/RSN IE",
			   __func__);
		goto no_ie;
	}

	/*
	 * Handling Pairing hash IE here.
	 */
	if (qtn_hapd_pairingie_handle(bss, hapd, addr, &ie) < 0){
		madwifi_sta_disassoc(bss, hapd->own_addr,
					addr, IEEE80211_REASON_IE_INVALID);
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "madwifi req WPA IE",
		    ie.wpa_ie, IEEE80211_MAX_OPT_IE);
	iebuf = ie.wpa_ie;
	/* madwifi seems to return some random data if WPA/RSN IE is not set.
	 * Assume the IE was not included if the IE type is unknown. */
	if (iebuf[0] != WLAN_EID_VENDOR_SPECIFIC)
		iebuf[1] = 0;
#ifdef MADWIFI_NG
	wpa_hexdump(MSG_MSGDUMP, "madwifi req RSN IE",
		    ie.rsn_ie, IEEE80211_MAX_OPT_IE);
	if (iebuf[1] == 0 && ie.rsn_ie[1] > 0) {
		/* madwifi-ng svn #1453 added rsn_ie. Use it, if wpa_ie was not
		 * set. This is needed for WPA2. */
		iebuf = ie.rsn_ie;
		if (iebuf[0] != WLAN_EID_RSN)
			iebuf[1] = 0;
	}
#endif /* MADWIFI_NG */

	ielen = iebuf[1];

	if (ie.wps_ie &&
	    ((ie.wps_ie[1] > 0) &&
	     (ie.wps_ie[0] == WLAN_EID_VENDOR_SPECIFIC))) {
		iebuf = ie.wps_ie;
		ielen = ie.wps_ie[1];
	}

#ifdef CONFIG_HS20
	wpa_hexdump(MSG_MSGDUMP, "madwifi req OSEN IE",
			    ie.osen_ie, IEEE80211_MAX_OPT_IE);
	if (ielen == 0 && ie.osen_ie[1] > 0) {
		iebuf = ie.osen_ie;
		ielen = ie.osen_ie[1];
	}
#endif

	if (ielen == 0)
		iebuf = NULL;
	else
		ielen += 2;

no_ie:
	drv_event_assoc(bss->bss_ctx, addr, iebuf, ielen, 0);

	if (memcmp(addr, bss->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		memset(bss->acct_mac, 0, ETH_ALEN);
		memset(&bss->acct_data, 0, sizeof(bss->acct_data));
	}
}

static void
madwifi_generate_wds_key(const u8 *mbs_addr, const u8 *rbs_addr, const u8 *pmk, u8 *wds_key)
{
	const u8 *addr[3];
	size_t len[3];

	addr[0] = mbs_addr;
	len[0] = ETH_ALEN;
	addr[1] = rbs_addr;
	len[1] = ETH_ALEN;
	addr[2] = pmk;
	len[2] = PMK_LEN;

	sha256_vector(3, addr, len, wds_key);

	wpa_hexdump(MSG_ERROR, "mbs_mac:", mbs_addr, ETH_ALEN);
	wpa_hexdump(MSG_ERROR, "rbs_mac:", rbs_addr, ETH_ALEN);
	wpa_hexdump(MSG_ERROR, "pmk:", pmk, PMK_LEN);
	wpa_hexdump(MSG_ERROR, "wds_key:", wds_key, PMK_LEN);
}


static void
madwifi_wireless_event_wireless_custom(struct madwifi_bss *bss,
				       char *custom)
{
	wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'", custom);

	if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		/* Local - default to 'yes' */
		int local = 1;
		u8 addr[ETH_ALEN];
		pos = strstr(custom, "addr=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "without sender address ignored");
			return;
		}
		pos += 5;
		/* Quantenna - go into countermeasures regardless */
		if (strstr(custom, "qtn=1"))
		{
			/* Ensure for Quantenna devices we don't check the MAC address */
			local = 0;
		}
		if (hwaddr_aton(pos, addr) == 0) {
			union wpa_event_data data;
			os_memset(&data, 0, sizeof(data));
			data.michael_mic_failure.unicast = 1;
			data.michael_mic_failure.local = local;
			data.michael_mic_failure.src = addr;
			wpa_supplicant_event(bss->bss_ctx,
					     EVENT_MICHAEL_MIC_FAILURE, &data);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u8 addr[ETH_ALEN];
		u32 val;
		key = custom;
		while ((key = strchr(key, '\n')) != NULL) {
			key++;
			value = strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (strcmp(key, "mac") == 0) {
				if (hwaddr_aton(value, addr) == 0) {
					memcpy(bss->acct_mac, addr, ETH_ALEN);
				} else {
					wpa_printf(MSG_DEBUG,
						   "STA-TRAFFIC-STAT "
					           "with invalid MAC address");
				}
			} else if (strcmp(key, "rx_packets") == 0)
				bss->acct_data.rx_packets = val;
			else if (strcmp(key, "tx_packets") == 0)
				bss->acct_data.tx_packets = val;
			else if (strcmp(key, "rx_bytes") == 0)
				bss->acct_data.rx_bytes = val;
			else if (strcmp(key, "tx_bytes") == 0)
				bss->acct_data.tx_bytes = val;
			key = value;
		}
	} else if (os_strncmp(custom, "WPS-BUTTON.indication", 21) == 0) {
		hostapd_wps_button_pushed(bss->bss_ctx, NULL);
	} else if (os_strncmp(custom, "STA-REQUIRE-LEAVE", 17) == 0) {
		u8 addr[ETH_ALEN];
		char *addr_str;
		addr_str = os_strchr(custom, '=');
		if (addr_str != NULL) {
			addr_str++;
			if (hwaddr_aton(addr_str, addr) == 0) {
				hostapd_sta_require_leave(bss->bss_ctx, addr);
			} else {
				wpa_printf(MSG_DEBUG, "STA-REQUIRE-LEAVE "
					   "with invalid MAC address");
			}
		}
	}
}

static void
madwifi_wireless_event_wireless(struct madwifi_bss *bss,
					    char *data, int len)
{
	struct madwifi_driver_data *drv = bss->drv;
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_MSGDUMP, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (drv->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVEXPIRED:
			drv_event_disassoc(bss->bss_ctx,
					   (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			madwifi_new_sta(bss, (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			madwifi_wireless_event_wireless_custom(bss, buf);
			free(buf);
			break;
		}

		pos += iwe->len;
	}
}


static void
madwifi_wireless_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi,
				   u8 *buf, size_t len)
{
	struct madwifi_bss *bss = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	if (ifi->ifi_index != bss->ifindex)
		return;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			madwifi_wireless_event_wireless(
				bss, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static int
madwifi_get_we_version(struct madwifi_driver_data *drv)
{
	struct madwifi_bss *bss;
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	bss = dl_list_first(&drv->bss, struct madwifi_bss, list);
	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = os_zalloc(buflen);
	if (range == NULL)
		return -1;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(bss->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}


static int
madwifi_wireless_event_init(struct madwifi_bss *bss)
{
	struct netlink_config *cfg;

	madwifi_get_we_version(bss->drv);

	cfg = os_zalloc(sizeof(*cfg));
	if (cfg == NULL)
		return -1;
	cfg->ctx = bss;
	cfg->newlink_cb = madwifi_wireless_event_rtm_newlink;
	bss->netlink = netlink_init(cfg);
	if (bss->netlink == NULL) {
		os_free(cfg);
		return -1;
	}

	return 0;
}


static int
madwifi_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
		   int encrypt, const u8 *own_addr, u32 flags)
{
	struct madwifi_bss *bss = priv;
	unsigned char buf[3000];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = malloc(len);
		if (bp == NULL) {
			printf("EAPOL frame discarded, cannot malloc temp "
			       "buffer of size %lu!\n", (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	memcpy(eth->h_dest, addr, ETH_ALEN);
	memcpy(eth->h_source, own_addr, ETH_ALEN);
	eth->h_proto = host_to_be16(ETH_P_EAPOL);
	memcpy(eth+1, data, data_len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", bp, len);

	/* FIXME this currently only supports EAPOL frames up to 2047 bytes */
	if (data_len > 2047) {
		if (bp != buf) {
			free(bp);
		}
		return -1;
	}

#define QTN_WPA_FAST_PATH	/* Send directly to the wlan driver */
#ifdef QTN_WPA_FAST_PATH
		status = set80211priv(bss, IEEE80211_IOCTL_TXEAPOL, bp, len);
#else
		status = l2_packet_send(bss->sock_xmit, addr, ETH_P_EAPOL, bp, len);
#endif

	if (bp != buf)
		free(bp);
	return status;
}


static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct madwifi_bss *bss = ctx;
	drv_event_eapol_rx(bss->bss_ctx, src_addr, buf + sizeof(struct l2_ethhdr),
			   len - sizeof(struct l2_ethhdr));
}


static int madwifi_init_bss_bridge(struct madwifi_bss *bss, const char *ifname)
{
	char in_br[IFNAMSIZ + 1];
	const char* brname = bss->brname;
	int add_bridge_required = 1;

	if (brname[0] == 0) {
		add_bridge_required = 0;
	}

	if (linux_br_get(in_br, ifname) == 0) {
		/* it is in a bridge already */
		if (os_strcmp(in_br, brname) == 0) {
			add_bridge_required = 0;
		} else {
			/* but not the desired bridge; remove */
			wpa_printf(MSG_DEBUG, "%s: Removing interface %s from bridge %s",
					__FUNCTION__, ifname, in_br);
			if (linux_br_del_if(bss->ioctl_sock, in_br, ifname) < 0) {
				wpa_printf(MSG_ERROR, "%s: Failed to "
						"remove interface %s from bridge %s: %s",
						__FUNCTION__, ifname, brname, strerror(errno));
				return -1;
			}
		}
	}

	if (add_bridge_required) {
		wpa_printf(MSG_DEBUG, "%s: Adding interface %s into bridge %s",
				__FUNCTION__, ifname, brname);
		if (linux_br_add_if(bss->ioctl_sock, brname, ifname) < 0) {
			wpa_printf(MSG_ERROR, "%s: Failed to add interface %s "
					"into bridge %s: %s",
					__FUNCTION__, ifname, brname, strerror(errno));
			return -1;
		}
		bss->added_if_into_bridge = 1;
	}

	return 0;
}


static int madwifi_deinit_bss_bridge(struct madwifi_bss *bss, const char *ifname)
{
	const char* brname = bss->brname;

	if (bss->added_if_into_bridge) {
		if (linux_br_del_if(bss->ioctl_sock, brname, ifname) < 0) {
			wpa_printf(MSG_ERROR, "%s: Failed to "
					"remove interface %s from bridge %s: %s",
					__FUNCTION__, ifname, brname, strerror(errno));
			return -1;
		}
		bss->added_if_into_bridge = 0;
	}

	return 0;
}


static void *
madwifi_init_bss(struct madwifi_driver_data *drv, struct hostapd_data *hapd, const char *name, const char *brname)
{
	struct madwifi_bss *bss;
	struct ifreq ifr;

	bss = os_zalloc(sizeof(struct madwifi_bss));
	if (bss == NULL) {
		return NULL;
	}

	dl_list_add(&drv->bss, &bss->list);
	bss->bss_ctx = hapd;
	bss->drv = drv;

	bss->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (bss->ioctl_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		goto bad;
	}
	memcpy(bss->ifname, name, sizeof(bss->ifname));

	memset(bss->brname, 0, sizeof(bss->brname));
	if (brname) {
		strncpy(bss->brname, brname, sizeof(bss->brname));
	}

	memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, bss->ifname, sizeof(ifr.ifr_name));
	if (ioctl(bss->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		goto bad;
	}
	bss->ifindex = ifr.ifr_ifindex;

	bss->sock_xmit = l2_packet_init(bss->ifname, NULL, ETH_P_EAPOL,
					handle_read, bss, 1);
	if (bss->sock_xmit == NULL)
		goto bad;

	bss->sock_recv = bss->sock_xmit;

	/* mark down during setup */
	linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 0);
	madwifi_set_privacy(bss, 0); /* default to no privacy */

	madwifi_receive_pkt(bss);

	if (madwifi_wireless_event_init(bss))
		goto bad;

	if (madwifi_init_bss_bridge(bss, name))
		goto bad;

	return bss;
bad:
	if (bss->sock_xmit != NULL)
		l2_packet_deinit(bss->sock_xmit);
	if (bss->ioctl_sock >= 0)
		close(bss->ioctl_sock);
	dl_list_del(&bss->list);
	if (bss)
		os_free(bss);

	return NULL;
}


static int madwifi_bss_add(void *priv, const char *ifname, const u8 *bssid,
			       void *bss_ctx, void **drv_priv, u8 *ifaddr, const char *bridge)
{
	struct madwifi_bss *primary_bss = priv;
	struct madwifi_driver_data *drv = primary_bss->drv;
	struct madwifi_bss *new_bss = NULL;
	char bssid_str[20] = {0};
	FILE *qdrv_control;
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	/* TODO: replace with proper QCSAPI set mode call */
	qdrv_control = fopen("/sys/devices/qdrv/control", "w");
	if (!qdrv_control) {
		return 1;
	}

	if (hostapd_mac_comp_empty(bssid) == 0) {
		snprintf(buf, sizeof(buf), "start 0 ap %s\n", ifname);
	} else {
		snprintf(bssid_str, sizeof(bssid_str), "%02x:%02x:%02x:%02x:%02x:%02x",
			bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
		snprintf(buf, sizeof(buf), "start 0 ap %s %s\n", ifname, bssid_str);
	}
	ret = fwrite(buf, strlen(buf), 1, qdrv_control);
	fclose(qdrv_control);

	if (!ret) {
		wpa_printf(MSG_DEBUG, "%s: VAP create failed, "
				"couldn't write to qdrv control",
				__func__);
		return 1;
	}

	snprintf(buf, sizeof(buf),
			"echo 1 > /proc/sys/net/ipv6/conf/%s/disable_ipv6", ifname);
	system(buf);

	new_bss = madwifi_init_bss(drv, bss_ctx, ifname, bridge);
	if (new_bss == NULL) {
		wpa_printf(MSG_ERROR, "%s: new BSS is null", __func__);
		return 1;
	}

	if (linux_get_ifhwaddr(new_bss->ioctl_sock, new_bss->ifname, ifaddr)) {
		wpa_printf(MSG_ERROR, "%s: failed to get iface hw address",
				__func__);
		return 1;
	}

	os_memcpy(new_bss->bssid, ifaddr, ETH_ALEN);

	if (drv_priv)
		*drv_priv = new_bss;

	snprintf(buf, sizeof(buf), "/scripts/tc_prio -dev %s -join > /dev/null", ifname);
	system(buf);

	return 0;
}


static int madwifi_bss_remove(void *priv, const char *ifname)
{
	struct madwifi_bss *bss = priv;
	FILE *qdrv_control;
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	madwifi_deinit_bss_bridge(bss, ifname);
	madwifi_set_privacy(bss, 0);
	netlink_deinit(bss->netlink);
	bss->netlink = NULL;
	(void) linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 0);
	if (bss->ioctl_sock >= 0) {
		close(bss->ioctl_sock);
		bss->ioctl_sock = -1;
	}
	if (bss->sock_recv != NULL && bss->sock_recv != bss->sock_xmit) {
		l2_packet_deinit(bss->sock_recv);
		bss->sock_recv = NULL;
	}
	if (bss->sock_xmit != NULL) {
		l2_packet_deinit(bss->sock_xmit);
		bss->sock_xmit = NULL;
	}
	if (bss->sock_raw) {
		l2_packet_deinit(bss->sock_raw);
		bss->sock_raw = NULL;
	}
	dl_list_del(&bss->list);
	os_free(bss);

	/* TODO: replace with proper QCSAPI set mode call */
	qdrv_control = fopen("/sys/devices/qdrv/control", "w");
	if (!qdrv_control) {
		return 1;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf) - 1, "stop 0 %s\n", ifname);
	ret = fwrite(buf, strlen(buf), 1, qdrv_control);
	fclose(qdrv_control);

	if (!ret) {
		wpa_printf(MSG_DEBUG, "%s: VAP remove failed, "
				"couldn't write to qdrv control",
				__func__);
		return 1;
	}

	return 0;
}

static int madwifi_if_add(void *priv, enum wpa_driver_if_type type,
			      const char *ifname, const u8 *addr,
			      void *bss_ctx, void **drv_priv,
			      char *force_ifname, u8 *if_addr,
			      const char *bridge, int use_existing)
{
	wpa_printf(MSG_DEBUG, "%s(type=%d ifname=%s bss_ctx=%p)\n",
		   __func__, type, ifname, bss_ctx);

	if (type == WPA_IF_AP_BSS) {
		return madwifi_bss_add(priv, ifname, addr, bss_ctx,
				drv_priv, if_addr, bridge);
	}

	return 0;
}


static int madwifi_if_remove(void *priv, enum wpa_driver_if_type type,
				 const char *ifname)
{
	wpa_printf(MSG_DEBUG, "%s(type=%d ifname=%s)", __func__, type, ifname);

	if (priv == NULL)
		return 0;

	if (type == WPA_IF_AP_BSS)
		return madwifi_bss_remove(priv, ifname);

	return 0;
}

static int
madwifi_get_driver_capa_info(struct madwifi_bss *bss)
{
	struct madwifi_driver_data *drv = bss->drv;
	struct iwreq iwr;
	u8 buf[MADWIFI_CMD_BUF_SIZE];
	u8 *pos = buf;
	u8 *end;
	u32 data_len;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);

	iwr.u.data.flags = SIOCDEV_SUBIO_GET_DRIVER_CAPABILITY;
	iwr.u.data.pointer = &buf;
	iwr.u.data.length = sizeof(buf);

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get Ext capability"
			" from driver ", __func__);
		return -1;
	}

	data_len = (u32)*pos;
	end = pos + data_len;
	pos += sizeof(u32);

	while (pos < end) {
		switch (*pos) {
		case IEEE80211_ELEMID_EXTCAP:
			pos++;
			drv->extended_capa_len = *pos++;
			drv->extended_capa = os_zalloc(drv->extended_capa_len);

			if (drv->extended_capa) {
				os_memcpy(drv->extended_capa, pos,
							drv->extended_capa_len);
				pos += drv->extended_capa_len;
			}

			drv->extended_capa_mask = os_zalloc(drv->extended_capa_len);

			if (drv->extended_capa_mask) {
				os_memcpy(drv->extended_capa_mask, pos,
					drv->extended_capa_len);
				pos += drv->extended_capa_len;
			} else {
				os_free(drv->extended_capa);
				drv->extended_capa = NULL;
				drv->extended_capa_len = 0;
			}
			break;
		default:
			wpa_printf(MSG_DEBUG, "Not handling other data %d\n", *pos);
			pos = end; /* Exit here */
			break;
		}
	}

	return 0;
}

static void *
madwifi_init(struct hostapd_data *hapd, struct wpa_init_params *params)
{
	struct madwifi_driver_data *drv;
	struct madwifi_bss *bss;
	char brname[IFNAMSIZ];

	drv = os_zalloc(sizeof(struct madwifi_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for madwifi driver data\n");
		return NULL;
	}

	dl_list_init(&drv->bss);

	bss = madwifi_init_bss(drv, hapd, params->ifname, hapd->conf->bridge);
	if (bss == NULL) {
		os_free(drv);
		return NULL;
	}

	/* FIXME: handle this for the new BSS case too */
	if (l2_packet_get_own_addr(bss->sock_xmit, params->own_addr))
		goto bad;
	if (params->bridge[0]) {
		wpa_printf(MSG_DEBUG, "Configure bridge %s for EAPOL traffic.",
			   params->bridge[0]);
		bss->sock_recv = l2_packet_init(params->bridge[0], NULL,
						ETH_P_EAPOL, handle_read, bss,
						1);
		if (bss->sock_recv == NULL)
			goto bad;
	} else if (linux_br_get(brname, bss->ifname) == 0) {
		wpa_printf(MSG_DEBUG, "Interface in bridge %s; configure for "
			   "EAPOL receive", brname);
		bss->sock_recv = l2_packet_init(brname, NULL, ETH_P_EAPOL,
						handle_read, bss, 1);
		if (bss->sock_recv == NULL)
			goto bad;
	} else {
		bss->sock_recv = bss->sock_xmit;
	}

	if (madwifi_get_driver_capa_info(bss))
		goto bad;

	set80211param(bss, IEEE80211_PARAM_HOSTAP_STARTED, 1);

	return bss;
bad:
	madwifi_bss_remove(bss, params->ifname);
	os_free(drv);
	return NULL;
}


static void
madwifi_deinit(void *priv)
{
	/* Secondary BSSes will be cleaned up in madwifi_if_remove, this function cleans up the primary BSS */
	struct madwifi_bss *bss = priv;
	struct madwifi_driver_data *drv = bss->drv;

	set80211param(bss, IEEE80211_PARAM_HOSTAP_STARTED, 0);

	netlink_deinit(bss->netlink);
	bss->netlink = NULL;
	(void) linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 0);
	if (bss->ioctl_sock >= 0) {
		close(bss->ioctl_sock);
		bss->ioctl_sock = -1;
	}

	if (bss->sock_recv != NULL && bss->sock_recv != bss->sock_xmit) {
		l2_packet_deinit(bss->sock_recv);
		bss->sock_recv = NULL;
	}

	if (bss->sock_xmit != NULL) {
		l2_packet_deinit(bss->sock_xmit);
		bss->sock_xmit = NULL;
	}

	if (bss->sock_raw) {
		l2_packet_deinit(bss->sock_raw);
		bss->sock_raw = NULL;
	}

	if (drv->extended_capa)
		os_free(drv->extended_capa);

	if (drv->extended_capa_mask)
		os_free(drv->extended_capa_mask);

	dl_list_del(&bss->list);
	os_free(bss);
	free(drv);
}

static int
madwifi_set_ssid(void *priv, const u8 *buf, int len)
{
	struct madwifi_bss *bss = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(bss->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}
	return 0;
}

static int
madwifi_get_ssid(void *priv, u8 *buf, int len)
{
	struct madwifi_bss *bss = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len;

	if (ioctl(bss->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else
		ret = iwr.u.essid.length;

	return ret;
}

static int
madwifi_set_countermeasures(void *priv, int enabled)
{
	struct madwifi_bss *bss = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(bss, IEEE80211_PARAM_COUNTERMEASURES, enabled);
}

static int
madwifi_commit(void *priv)
{
	struct madwifi_bss *bss = priv;

	linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 1);

	return 0;
}

static int
madwifi_set_intra_bss(void *priv, int enabled)
{
        struct madwifi_bss *bss = priv;
        return set80211param(bss, IEEE80211_PARAM_AP_ISOLATE, enabled);
}

static int
madwifi_set_intra_per_bss(void *priv, int enabled)
{
	struct madwifi_bss *bss = priv;

	set80211param(bss, IEEE80211_PARAM_INTRA_BSS_ISOLATE, enabled);

	return 0;
}

static int
madwifi_set_bss_isolate(void *priv, int enabled)
{
	struct madwifi_bss *bss = priv;

	set80211param(bss, IEEE80211_PARAM_BSS_ISOLATE, enabled);

	return 0;
}

static int
madwifi_set_bss_assoc_limit(void *priv, int limit)
{
        struct madwifi_bss *bss = priv;

        return set80211param(bss, IEEE80211_PARAM_BSS_ASSOC_LIMIT, limit);
}

static int
madwifi_set_total_assoc_limit(void *priv, int limit)
{
        struct madwifi_bss *bss = priv;

        return set80211param(bss, IEEE80211_PARAM_ASSOC_LIMIT, limit);
}

static int
madwifi_set_broadcast_ssid(void *priv, int value)
{
	struct madwifi_bss *bss = priv;
	return set80211param(bss, IEEE80211_PARAM_HIDESSID, value);
}

static void madwifi_send_log(void *priv, const char *msg)
{
	struct madwifi_bss *bss = priv;
	set80211priv(bss, IEEE80211_IOCTL_POSTEVENT, (void *) msg,
			strnlen(msg, MAX_WLAN_MSG_LEN));
}

/**
* app_buf = [struct app_action_frm_buf] + [Action Frame Payload]
* Action Frame Payload = WLAN_ACTION_PUBLIC (u8) + action (u8) + dialog token (u8) +
* status code (u8) + Info
*/
static int
madwifi_send_action (void *priv, unsigned int freq,
                                       unsigned int wait_time,
                                       const u8 *dst_mac, const u8 *src_mac,
                                       const u8 *bssid,
                                       const u8 *data, size_t data_len,
                                       int no_cck)
{
	struct madwifi_bss *bss = priv;
	struct iwreq iwr;
	struct app_action_frame_buf *app_action_frm_buf;
	int ret = 0;

	app_action_frm_buf = os_malloc(data_len  + sizeof(struct app_action_frame_buf));
	if (!app_action_frm_buf) {
		wpa_printf(MSG_DEBUG, "Memory allocation failed in "
			"func : %s line : %d \n", __func__, __LINE__);
		return -1;
	}

	/* data is Action frame payload. First byte of the data is action frame category
	 * and the second byte is action */
	app_action_frm_buf->cat = *data;
	app_action_frm_buf->action = *(data + 1);
	os_memcpy(app_action_frm_buf->dst_mac_addr, dst_mac, ETH_ALEN);

	app_action_frm_buf->frm_payload.length = (u16)data_len;
	os_memcpy(app_action_frm_buf->frm_payload.data, data, data_len);

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.data.flags = SIOCDEV_SUBIO_SEND_ACTION_FRAME;
	iwr.u.data.pointer = app_action_frm_buf;
	iwr.u.data.length = data_len + sizeof(struct app_action_frame_buf);

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to send action "
				"frame ioctl", __func__);
		ret = -1;
	}

	os_free(app_action_frm_buf);

	return ret;
}

static int madwifi_send_mlme(void *priv, const void *msg, size_t len, int noack)
{
	struct madwifi_bss *bss = priv;
	struct ieee80211_mgmt *mgmt = msg;
	uint16_t fc;

	if (!priv || !msg)
		return -1;

	if (len < offsetof(struct ieee80211_mgmt, u))
		return -1;

	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
			WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION) {
		size_t data_len = len - offsetof(struct ieee80211_mgmt, u);
		struct hostapd_data *hapd = bss->bss_ctx;

		return madwifi_send_action(priv, hapd->iface->freq, 0,
					mgmt->da, mgmt->sa,
					mgmt->bssid, &mgmt->u.action,
					data_len, 0);
	} else {
		return -1;
	}
}

static int madwifi_set_pairing_hash_ie(void *priv, const u8 *pairing_hash,
							size_t ies_len)
{
	struct ieee80211req_getset_appiebuf *pairing_hash_ie;
	int ret;

	pairing_hash_ie = os_malloc(sizeof(*pairing_hash_ie) + ies_len);
	if (pairing_hash_ie == NULL)
		return -1;

	os_memset(pairing_hash_ie, 0, sizeof(*pairing_hash_ie) + ies_len);
	pairing_hash_ie->app_frmtype = IEEE80211_APPIE_FRAME_ASSOC_RESP;
	pairing_hash_ie->app_buflen = ies_len;
	pairing_hash_ie->flags = F_QTN_IEEE80211_PAIRING_IE;
	os_memcpy(pairing_hash_ie->app_buf, pairing_hash, ies_len);

	ret = set80211priv(priv, IEEE80211_IOCTL_SET_APPIEBUF, pairing_hash_ie,
			   sizeof(struct ieee80211req_getset_appiebuf) +
			   ies_len);

	os_free(pairing_hash_ie);

	return ret;
}

static int
madwifi_get_driver_capa(void *priv, struct wpa_driver_capa *capa)
{
	struct madwifi_bss *bss = priv;
	struct madwifi_driver_data *drv = bss->drv;

	if (drv->extended_capa) {
		capa->extended_capa_len = drv->extended_capa_len;
		capa->extended_capa = drv->extended_capa;
		capa->extended_capa_mask = drv->extended_capa_mask;
	} else {
		return -1;
	}

	return 0;
}

static int
madwifi_set_interworking(struct madwifi_bss *bss,
			struct wpa_driver_ap_params *params)
{
	struct iwreq iwr;
	struct app_ie ie;

	memset(&ie, 0, sizeof(struct app_ie));

	ie.id = WLAN_EID_INTERWORKING;		/* IE ID */
	ie.u.interw.interworking = params->interworking;
	ie.len++;

	if (params->interworking) {
		ie.u.interw.an_type = params->access_network_type;
		ie.len++;

		if (params->hessid) {
			os_memcpy(ie.u.interw.hessid, params->hessid, ETH_ALEN);
			ie.len += ETH_ALEN;
		}
	}

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.data.flags = SIOCDEV_SUBIO_SET_AP_INFO;
	iwr.u.data.pointer = &ie;
	iwr.u.data.length = ie.len + 1 + 2;	/* IE data len + IE ID + IE len */

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to set interworking"
				"info ioctl", __func__);
		return -1;
	}

	return 0;
}

static int
madwifi_set_ap(void *priv, struct wpa_driver_ap_params *params)
{
	struct madwifi_bss *bss = priv;

	/* TODO As of now this function is used for setting HESSID and
	* Access Network Type. It can be used for other elements in future
	* like set_authmode, set_privacy, set_ieee8021x, set_generic_elem
	* and hapd_set_ssid */

	if (madwifi_set_interworking(bss, params))
		return -1;

	if (set80211param(bss, IEEE80211_PARAM_HS2, params->hs20_enable)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set hs20 enabled to %d\n",
					__func__, params->hs20_enable);
		return -1;
	}

	if (set80211param(bss, IEEE80211_PARAM_DGAF_CONTROL,
					params->disable_dgaf)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set disable dgaf to %u\n",
					__func__, params->disable_dgaf);
		return -1;
	}

	if (set80211param(bss, IEEE80211_PARAM_PROXY_ARP,
					params->proxy_arp)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set proxy ARP to %u\n",
					__func__, params->proxy_arp);
		return -1;
	}

	if (set80211param(bss, IEEE80211_PARAM_DTIM_PERIOD,
					params->dtim_period)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set DTIM period to %u\n",
					__func__, params->dtim_period);
		return -1;
	}

#ifdef CONFIG_HS20
	if (params->osen) {
		struct wpa_bss_params wpa_params;
		memset(&wpa_params, 0, sizeof(wpa_params));

		wpa_params.enabled = 1;
		wpa_params.wpa = 2;
		wpa_params.ieee802_1x = 1;
		wpa_params.wpa_group = WPA_CIPHER_CCMP;
		wpa_params.wpa_pairwise = WPA_CIPHER_CCMP;
		wpa_params.wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		if (madwifi_set_privacy(priv, 1)) {
			wpa_printf(MSG_DEBUG, "%s: Unable to enable privacy\n", __func__);
			return -1;
		}
		if (madwifi_set_ieee8021x(priv, &wpa_params)) {
			wpa_printf(MSG_DEBUG, "%s: Unable to set 802.1X params\n", __func__);
			return -1;
		}
		if (set80211param(bss, IEEE80211_PARAM_OSEN, 1)) {
			wpa_printf(MSG_DEBUG, "%s: Unable to set OSEN\n", __func__);
			return -1;
		}
	}
#endif

	return 0;
}

int madwifi_set_qos_map(void *priv, const u8 *qos_map_set,
			   u8 qos_map_set_len)
{
	struct madwifi_bss *bss = priv;
	u8 dscp2up[IP_DSCP_NUM];
	struct iwreq iwr;
	int up_start_idx;
	u8 *up_map;
	u8 up;
	u8 dscp;
	u8 dscp_low;
	u8 dscp_high;
	int i;

	if (qos_map_set_len < (IEEE8021P_PRIORITY_NUM * 2) ||
	    qos_map_set_len > ((IEEE8021P_PRIORITY_NUM + IEEE80211_DSCP_MAX_EXCEPTIONS) * 2) ||
	    (qos_map_set_len & 1)) {
		wpa_printf(MSG_ERROR, "%s invalid QoS Map length\n", __func__);
		return -1;
	}

	up_start_idx = qos_map_set_len - IEEE8021P_PRIORITY_NUM * 2;
	up_map = &qos_map_set[up_start_idx];
	os_memset(dscp2up, IEEE8021P_PRIORITY_NUM, sizeof(dscp2up));

	for (up = 0; up < IEEE8021P_PRIORITY_NUM; up++) {
		dscp_low = up_map[up * 2];
		dscp_high = up_map[up * 2 + 1];
		if (dscp_low < IP_DSCP_NUM &&
		    dscp_high < IP_DSCP_NUM &&
		    dscp_low <= dscp_high) {
			os_memset(&dscp2up[dscp_low], up, dscp_high - dscp_low + 1);
		}
	}
	for (i = 0; i < up_start_idx; i += 2) {
		dscp = qos_map_set[i];
		up = qos_map_set[i + 1];
		if (dscp < IP_DSCP_NUM) {
			dscp2up[dscp] = up;
		}
	}

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, bss->ifname, IFNAMSIZ);
	iwr.u.data.flags = SIOCDEV_SUBIO_SET_DSCP2TID_MAP;
	iwr.u.data.pointer = dscp2up;
	iwr.u.data.length = IP_DSCP_NUM;

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to call SIOCDEV_SUBIO_SET_DSCP2TID_MAP", __func__);
		return -1;
	}

	return 0;
}

const struct wpa_driver_ops wpa_driver_madwifi_ops = {
	.name			= "madwifi",
	.desc			= "MADWIFI 802.11 support (Atheros, etc.)",
	.set_key		= wpa_driver_madwifi_set_key,
	.hapd_init		= madwifi_init,
	.hapd_deinit		= madwifi_deinit,
	.set_ieee8021x		= madwifi_set_ieee8021x,
	.set_privacy		= madwifi_set_privacy,
	.get_seqnum		= madwifi_get_seqnum,
	.if_add			= madwifi_if_add,
	.if_remove		= madwifi_if_remove,
	.flush			= madwifi_flush,
	.set_generic_elem	= madwifi_set_opt_ie,
	.sta_set_flags		= madwifi_sta_set_flags,
	.read_sta_data		= madwifi_read_sta_driver_data,
	.hapd_send_eapol	= madwifi_send_eapol,
	.sta_disassoc		= madwifi_sta_disassoc,
	.sta_deauth		= madwifi_sta_deauth,
	.hapd_set_ssid		= madwifi_set_ssid,
	.hapd_get_ssid		= madwifi_get_ssid,
	.hapd_set_countermeasures	= madwifi_set_countermeasures,
	.sta_clear_stats        = madwifi_sta_clear_stats,
	.commit			= madwifi_commit,
	.set_ap_wps_ie		= madwifi_set_ap_wps_ie,
	.set_freq		= madwifi_set_freq,
	.set_intra_bss          = madwifi_set_intra_bss,
	.set_intra_per_bss	= madwifi_set_intra_per_bss,
	.set_bss_isolate	= madwifi_set_bss_isolate,
	.set_bss_assoc_limit    = madwifi_set_bss_assoc_limit,
	.set_total_assoc_limit  = madwifi_set_total_assoc_limit,
	.set_brcm_ioctl         = madwifi_brcm_info_ioctl,
	.set_sta_vlan		= madwifi_set_sta_vlan,
	.set_dyn_vlan		= madwifi_set_dyn_vlan,
	.vlan_group_add		= madwifi_vlan_group_add,
	.vlan_group_remove	= madwifi_vlan_group_remove,
	.set_broadcast_ssid	= madwifi_set_broadcast_ssid,
	.set_pairing_hash_ie	= madwifi_set_pairing_hash_ie,
	.send_log		= madwifi_send_log,
	.send_action		= madwifi_send_action,
	.get_capa		= madwifi_get_driver_capa,
	.set_ap                 = madwifi_set_ap,
	.set_qos_map            = madwifi_set_qos_map,
	.send_mlme		= madwifi_send_mlme,
};

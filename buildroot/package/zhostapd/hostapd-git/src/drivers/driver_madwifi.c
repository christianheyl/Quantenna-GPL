/*
 * WPA Supplicant - driver interaction with MADWIFI 802.11 driver
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, Video54 Technologies
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * While this driver wrapper supports both AP (hostapd) and station
 * (wpa_supplicant) operations, the station side is deprecated and
 * driver_wext.c should be used instead. This driver wrapper should only be
 * used with hostapd for AP mode functionality.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "wireless_copy.h"
#include "crypto/crypto.h"

#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ieee802_11_auth.h"
#include "ap/wps_hostapd.h"
/*
 * Avoid conflicts with wpa_supplicant definitions
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

#include "utils/list.h"
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

#define QTN_WDS_EXT_SCRIPT "qtn_wds_ext.sh"

#ifdef HOSTAPD

#include "priv_netlink.h"
#include "netlink.h"
#include "linux_ioctl.h"
#include "l2_packet/l2_packet.h"

#define IEEE80211_IOCTL_POSTEVENT	(SIOCIWFIRSTPRIV+19)
#define IEEE80211_IOCTL_TXEAPOL		(SIOCIWFIRSTPRIV+21)

#define MADWIFI_CMD_BUF_SIZE		128
#define MADWIFI_CMD_WDS_EXT_LEN		256
#define WDS_KEY_LEN			32

struct madwifi_bss {
	struct madwifi_driver_data *drv;
	struct dl_list list;
	void *bss_ctx;
	char ifname[IFNAMSIZ];
	int ifindex;
	u8 bssid[ETH_ALEN];
	char brname[IFNAMSIZ];
	int ioctl_sock;	/* socket for ioctl() use */
	struct l2_packet_data *sock_xmit; /* raw packet xmit socket */
	struct l2_packet_data *sock_recv; /* raw packet recv socket */
	struct l2_packet_data *sock_raw; /* raw 802.11 management frames */
	struct netlink_data *netlink;
	int added_if_into_bridge;
};

struct madwifi_driver_data {
	/*
	 * back pointer to data for the primary BSS.
	 *
	 * Will be initialized when madwifi_init was called.
	 * Should not be used for per bss usage.
	 */
	struct hostapd_data *hapd;

	char	iface[IFNAMSIZ + 1];
	int	we_version;
	u8	acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;

	struct dl_list bss;
};



static int
madwifi_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
		int reason_code);

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len);

static int
madwifi_set_privacy(void *priv, int enabled);

static int
madwifi_wireless_event_init(struct madwifi_bss *bss);

static int
madwifi_receive_probe_req(struct madwifi_bss *bss);

static int madwifi_init_bss_bridge(struct madwifi_bss *bss, const char *ifname)
{
	char in_br[IFNAMSIZ + 1];
	struct hostapd_data *hapd = bss->bss_ctx;
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
madwifi_init_bss(struct madwifi_driver_data *drv, const char *name, const char *brname)
{
	struct madwifi_bss *bss;
	struct ifreq ifr;

	bss = os_zalloc(sizeof(struct madwifi_bss));
	if (bss == NULL) {
		return NULL;
	}

	dl_list_add(&drv->bss, &bss->list);
	/* init to hostapd_data of the primary bss, should be set to per bss hostapd_data later */
	bss->bss_ctx = drv->hapd;
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

	madwifi_receive_probe_req(bss);

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

	new_bss = madwifi_init_bss(drv, ifname, bridge);
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
	/* Set hostapd_data per bss here */
	new_bss->bss_ctx = bss_ctx;

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
			      const char *bridge)
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
		    idx < (int) (sizeof(opnames) / sizeof(opnames[0])) &&
		    opnames[idx]) {
			ioctl_name = opnames[idx];
		}
		wpa_printf(MSG_DEBUG, "%s: %s returned error %d: %s\n",
				__func__, ioctl_name, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static void madwifi_send_log(void *priv, const char *msg)
{
	struct madwifi_bss *bss = priv;
	set80211priv(bss, IEEE80211_IOCTL_POSTEVENT, (void *) msg,
			strnlen(msg, MAX_WLAN_MSG_LEN));
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

#include "utils/list.h"
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
	if (priv == NULL) {
		return 0;
	}

	struct madwifi_bss *bss = priv;
	struct hostapd_data *hapd = bss->bss_ctx;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, params->enabled);

	if (!params->enabled) {
		/* XXX restore state */
		return set80211param(bss, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_AUTO);
	}
	if (!params->wpa && !params->ieee802_1x) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_DRIVER,
			HOSTAPD_LEVEL_WARNING, "No 802.1X or WPA enabled!");
		return -1;
	}
	if (params->wpa && madwifi_configure_wpa(bss, params) != 0) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_DRIVER,
			HOSTAPD_LEVEL_WARNING, "Error configuring WPA state!");
		return -1;
	}
	if (set80211param(bss, IEEE80211_PARAM_AUTHMODE,
		(params->wpa ? IEEE80211_AUTH_WPA : IEEE80211_AUTH_8021X))) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_DRIVER,
			HOSTAPD_LEVEL_WARNING, "Error enabling WPA/802.1X!");
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

/* N.B this is the hostapd verison of madwifi_set_key */
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
	struct madwifi_driver_data *drv = bss->drv;

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
		if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
			memcpy(data, &drv->acct_data, sizeof(*data));
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
		 drv->iface, MAC2STR(addr));

	f = fopen(buf, "r");
	if (!f) {
		if (memcmp(addr, drv->acct_mac, ETH_ALEN) != 0)
			return -1;
		memcpy(data, &drv->acct_data, sizeof(*data));
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
	struct ifreq ifr;
	int ret;

	if (bss->ioctl_sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, bss->ifname, IFNAMSIZ);

	if (ioctl(bss->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		printf("ioctl failed for ifr name %s\n", ifr.ifr_name);
		perror("ioctl[SIOCGIFFLAGS]");
		return -1;
	}
	if (!(ifr.ifr_flags & IFF_UP)) {
		wpa_printf(MSG_DEBUG, "%s: Interface is not up.\n", __func__);
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
	struct madwifi_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(bss, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disassoc STA (addr "
			   MACSTR " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
static void madwifi_raw_receive(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
	struct madwifi_bss *bss = ctx;
	const struct ieee80211_mgmt *mgmt;
	u16 fc;
	union wpa_event_data event;

	/* Send Probe Request information to WPS processing */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req))
		return;
	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_PROBE_REQ)
		return;

	os_memset(&event, 0, sizeof(event));
	event.rx_probe_req.sa = mgmt->sa;
	event.rx_probe_req.da = mgmt->da;
	event.rx_probe_req.bssid = mgmt->bssid;
	event.rx_probe_req.ie = mgmt->u.probe_req.variable;
	event.rx_probe_req.ie_len =
		len - (IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req));
	wpa_supplicant_event(bss->bss_ctx, EVENT_RX_PROBE_REQ, &event);
}
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */

static int madwifi_receive_probe_req(struct madwifi_bss *bss)
{
	int ret = 0;
#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
	struct ieee80211req_set_filter filt;
	struct hostapd_data *hapd = bss->bss_ctx;

	wpa_printf(MSG_DEBUG, "%s Enter", __func__);
	filt.app_filterype = IEEE80211_FILTER_TYPE_PROBE_REQ;

	ret = set80211priv(bss, IEEE80211_IOCTL_FILTERFRAME, &filt,
			   sizeof(struct ieee80211req_set_filter));
	if (ret)
		return ret;

	bss->sock_raw = l2_packet_init(hapd->conf->bridge, NULL, ETH_P_80211_RAW,
				       madwifi_raw_receive, bss, 1);
	if (bss->sock_raw == NULL)
		return -1;
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */
	return ret;
}

#ifdef CONFIG_WPS
static int
madwifi_set_wps_ie(void *priv, const u8 *ie, size_t len, u32 frametype)
{
	struct madwifi_bss *bss = priv;
	u8 buf[1024];
	struct ieee80211req_getset_appiebuf *beac_ie;

	wpa_printf(MSG_DEBUG, "%s buflen = %lu", __func__,
		   (unsigned long) len);

	if (len > sizeof(buf)) {
		wpa_printf(MSG_ERROR, "%s Transmitted WPS data exceeds max length (%lu) - "
			"reduce description lengths", __func__,
			sizeof(buf));
		return -1;
	}

	os_memset(buf, 0, sizeof(buf));
	beac_ie = (struct ieee80211req_getset_appiebuf *) buf;
	memset(beac_ie, 0, sizeof(struct ieee80211req_getset_appiebuf));
	beac_ie->app_frmtype = frametype;
	beac_ie->app_buflen = len;
	memcpy(&(beac_ie->app_buf[0]), ie, len);

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

	snprintf(buf, sizeof(buf), "set vlan %s dynamic %d", ifname, !!enable);

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
	struct madwifi_driver_data *drv = bss->drv;
	struct hostapd_data *hapd = bss->bss_ctx;
	struct ieee80211req_wpaie ie;
	int ielen = 0;
	int allowed, res;
	u8 *iebuf = NULL;
	u8 *pairing_iebuf = NULL;
	u8 peering_hash[PAIRING_HASH_LEN];
	u8 peering_ie_exist;

	/* Is this station allowed according to MAC address ACLs? */
	allowed = hostapd_allowed_address(hapd, addr, NULL, 0, NULL, NULL, NULL);
	if (allowed == HOSTAPD_ACL_REJECT) {
		hostapd_notif_disassoc(hapd, addr);

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
	if (hapd->conf->pairing_id && hapd->conf->pairing_enable == PPS2_MODE_DENY) {
		pairing_iebuf = ie.qtn_pairing_ie;
		peering_ie_exist = ie.has_pairing_ie;

		if (peering_ie_exist) {
			hostapd_setup_pairing_hash(hapd->conf->pairing_id, addr, peering_hash);

			if (memcmp(pairing_iebuf, peering_hash, PAIRING_HASH_LEN) == 0) {
				hostapd_notif_disassoc(hapd, addr);
				madwifi_sta_deauth(bss, hapd->own_addr, addr, WLAN_REASON_DENIED);
				wpa_printf(MSG_DEBUG, "%s: PPS2, STA denied", __func__);
			}
		}

	} else if (hapd->conf->pairing_id && hapd->conf->pairing_enable == PPS2_MODE_ACCEPT) {
		pairing_iebuf = ie.qtn_pairing_ie;
		peering_ie_exist = ie.has_pairing_ie;

		if (peering_ie_exist)
			hostapd_setup_pairing_hash(hapd->conf->pairing_id, addr, peering_hash);

		if (!peering_ie_exist || memcmp(pairing_iebuf, peering_hash, PAIRING_HASH_LEN) != 0) {
			hostapd_notif_disassoc(hapd, addr);
			madwifi_sta_disassoc(bss, hapd->own_addr, addr, IEEE80211_REASON_IE_INVALID);
			wpa_printf(MSG_ERROR, "%s: Pairing Hash IE checking - not matched", __func__);

			return;
		}

		wpa_printf(MSG_DEBUG, "%s: pairing IE checking pass", __func__);
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

	if (ielen == 0)
		iebuf = NULL;
	else
		ielen += 2;

no_ie:
	drv_event_assoc(hapd, addr, iebuf, ielen, 0);

	if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		memset(drv->acct_mac, 0, ETH_ALEN);
		memset(&drv->acct_data, 0, sizeof(drv->acct_data));
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
madwifi_qtn_extender_event(struct hostapd_data *hapd, void *custom)
{
	int ok_to_run = 1;
	char cmd[MADWIFI_CMD_WDS_EXT_LEN] = {'\0'};
	u8 wds_key[WDS_KEY_LEN];
	char wds_key_hex[WDS_KEY_LEN * 2 + 1] = {'\0'};

	struct qtn_wds_ext_event_data *event_data =
		(struct qtn_wds_ext_event_data*)custom;

	wpa_printf(MSG_DEBUG, "Madwifi: received QTN-WDS-EXT message, "
			"cmd = %d, mac = " MACSTR " peer role=%d",
			event_data->cmd, MAC2STR(event_data->mac),
			event_data->extender_role);

	switch (event_data->cmd) {
	case WDS_EXT_RECEIVED_MBS_IE:
		if (hapd->conf->wpa && hapd->conf->ssid.wpa_psk) {
			madwifi_generate_wds_key(event_data->mac, hapd->own_addr,
				hapd->conf->ssid.wpa_psk->psk_md5, &wds_key);
			wpa_snprintf_hex(wds_key_hex, sizeof(wds_key_hex),
						wds_key, sizeof(wds_key));
		} else {
			snprintf(wds_key_hex, sizeof(wds_key_hex), "NULL");
		}
		snprintf(cmd, sizeof(cmd), "%s %s peer=" MACSTR " channel=%d wds_key=%s &",
				QTN_WDS_EXT_SCRIPT,
				"RBS-CREATE-WDS-LINK",
				MAC2STR(event_data->mac),
				event_data->channel,
				wds_key_hex);
		break;
	case WDS_EXT_RECEIVED_RBS_IE:
		if (hapd->conf->wpa && hapd->conf->ssid.wpa_psk) {
			madwifi_generate_wds_key(hapd->own_addr, event_data->mac,
				hapd->conf->ssid.wpa_psk->psk_md5, &wds_key);
			wpa_snprintf_hex(wds_key_hex, sizeof(wds_key_hex),
						wds_key, sizeof(wds_key));
		} else {
			snprintf(wds_key_hex, sizeof(wds_key_hex), "NULL");
		}
		snprintf(cmd, sizeof(cmd), "%s %s peer=" MACSTR " wds_key=%s &",
				QTN_WDS_EXT_SCRIPT,
				"MBS-CREATE-WDS-LINK",
				MAC2STR(event_data->mac),
				wds_key_hex);
		break;
	case WDS_EXT_LINK_STATUS_UPDATE:
		if (event_data->extender_role == IEEE80211_EXTENDER_ROLE_MBS) {
			snprintf(cmd, sizeof(cmd), "%s %s peer=" MACSTR " &",
					QTN_WDS_EXT_SCRIPT, "MBS-REMOVE-WDS-LINK",
					MAC2STR(event_data->mac));
		} else if (event_data->extender_role == IEEE80211_EXTENDER_ROLE_RBS) {
			snprintf(cmd, sizeof(cmd), "%s %s peer=" MACSTR " &",
					QTN_WDS_EXT_SCRIPT, "RBS-REMOVE-WDS-LINK",
					MAC2STR(event_data->mac));
		}
		break;
	case WDS_EXT_RBS_OUT_OF_BRR:
		snprintf(cmd, sizeof(cmd), "%s %s peer=" MACSTR " &",
				QTN_WDS_EXT_SCRIPT, "START-STA-RBS",
				MAC2STR(event_data->mac));
		break;
	case WDS_EXT_RBS_SET_CHANNEL:
		snprintf(cmd, sizeof(cmd), "%s %s channel=%d &",
				QTN_WDS_EXT_SCRIPT, "RBS-SET-CHANNEL",
				event_data->channel);
		break;
	case WDS_EXT_CLEANUP_WDS_LINK:
		snprintf(cmd, sizeof(cmd), "%s %s peer=" MACSTR " &",
				QTN_WDS_EXT_SCRIPT, "REMOVE-WDS-LINK",
				MAC2STR(event_data->mac));
		break;
	default:
		ok_to_run = 0;
		break;
	}

	if (ok_to_run) {
		wpa_printf(MSG_DEBUG, "Madwifi: call command - %s", cmd);
		system(cmd);
	}
}

static void
madwifi_wireless_event_wireless_custom(struct madwifi_bss *bss,
				       char *custom)
{
	struct madwifi_driver_data *drv = bss->drv;
	struct hostapd_data *hapd = bss->bss_ctx;

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
			wpa_supplicant_event(hapd,
					     EVENT_MICHAEL_MIC_FAILURE, &data);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u32 val;
		key = custom;
		while ((key = strchr(key, '\n')) != NULL) {
			key++;
			value = strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (strcmp(key, "mac") == 0)
				hwaddr_aton(value, drv->acct_mac);
			else if (strcmp(key, "rx_packets") == 0)
				drv->acct_data.rx_packets = val;
			else if (strcmp(key, "tx_packets") == 0)
				drv->acct_data.tx_packets = val;
			else if (strcmp(key, "rx_bytes") == 0)
				drv->acct_data.rx_bytes = val;
			else if (strcmp(key, "tx_bytes") == 0)
				drv->acct_data.tx_bytes = val;
			key = value;
		}
	} else if (os_strncmp(custom, "WPS-BUTTON.indication", 21) == 0) {
		hostapd_wps_button_pushed(hapd, NULL);
	} else if (os_strncmp(custom, "STA-REQUIRE-LEAVE", 17) == 0) {
		u8 addr[ETH_ALEN];
		char *addr_str;
		addr_str = os_strchr(custom, '=');
		if (addr_str != NULL) {
			addr_str++;
			hwaddr_aton(addr_str, addr);
			hostapd_sta_require_leave(hapd, addr);
		}
	} else if (os_strncmp(custom, "QTN-WDS-EXT", 11) == 0) {
		madwifi_qtn_extender_event(hapd, custom);
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
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
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
	struct madwifi_driver_data *drv = bss->drv;
	struct netlink_config *cfg;

	madwifi_get_we_version(drv);

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
	drv->hapd = hapd;
	memcpy(drv->iface, params->ifname, sizeof(drv->iface));

	bss = madwifi_init_bss(drv, params->ifname, hapd->conf->bridge);
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
madwifi_set_broadcast_ssid(void *priv, int value)
{
	struct madwifi_bss *bss = priv;
	return set80211param(bss, IEEE80211_PARAM_HIDESSID, value);
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

	set80211param(bss, IEEE80211_PARAM_AP_ISOLATE, enabled);

	return 0;
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

	set80211param(bss, IEEE80211_PARAM_BSS_ASSOC_LIMIT, limit);

	return 0;
}

static int
madwifi_set_total_assoc_limit(void *priv, int limit)
{
	struct madwifi_bss *bss = priv;

	set80211param(bss, IEEE80211_PARAM_ASSOC_LIMIT, limit);

	return 0;
}

static int madwifi_set_pairing_hash_ie(void *priv, const u8 *pairing_hash,
							size_t ies_len)
{
	struct ieee80211req_getset_appiebuf *pairing_hash_ie;
	int ret;

	pairing_hash_ie = os_zalloc(sizeof(*pairing_hash_ie) + ies_len);
	if (pairing_hash_ie == NULL)
		return -1;

	memset(pairing_hash_ie, 0, sizeof(*pairing_hash_ie));
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

static int madwifi_set_ap(void *priv, struct wpa_driver_ap_params *params)
{
	struct madwifi_bss *bss = priv;

	return set80211param(bss, IEEE80211_PARAM_DTIM_PERIOD, params->dtim_period);
}
#else /* HOSTAPD */

struct wpa_driver_madwifi_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
};

static int wpa_driver_madwifi_set_auth_alg(void *priv, int auth_alg);
static int wpa_driver_madwifi_set_probe_req_ie(void *priv, const u8 *ies,
					       size_t ies_len);


static int
set80211priv(struct wpa_driver_madwifi_data *drv, int op, void *data, int len,
	     int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if (len < IFNAMSIZ &&
	    op != IEEE80211_IOCTL_SET_APPIEBUF) {
		/*
		 * Argument data fits inline; put it there.
		 */
		os_memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->sock, op, &iwr) < 0) {
		if (show_err) {
#ifdef MADWIFI_NG
			int first = IEEE80211_IOCTL_SETPARAM;
			int last = IEEE80211_IOCTL_KICKMAC;
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
				NULL,
				"ioctl[IEEE80211_IOCTL_SET_APPIEBUF]",
				"ioctl[IEEE80211_IOCTL_GETSCANRESULTS]",
				NULL,
				"ioctl[IEEE80211_IOCTL_GETCHANINFO]",
				"ioctl[IEEE80211_IOCTL_SETOPTIE]",
				"ioctl[IEEE80211_IOCTL_GETOPTIE]",
				"ioctl[IEEE80211_IOCTL_SETMLME]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
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
			int last = IEEE80211_IOCTL_CHANLIST;
			static const char *opnames[] = {
				"ioctl[IEEE80211_IOCTL_SETPARAM]",
				"ioctl[IEEE80211_IOCTL_GETPARAM]",
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				"ioctl[IEEE80211_IOCTL_GETKEY]",
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETMLME]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETOPTIE]",
				"ioctl[IEEE80211_IOCTL_GETOPTIE]",
				"ioctl[IEEE80211_IOCTL_ADDMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELMAC]",
				NULL,
				"ioctl[IEEE80211_IOCTL_CHANLIST]",
			};
#endif /* MADWIFI_NG */
			int idx = op - first;
			if (first <= op && op <= last &&
			    idx < (int) (sizeof(opnames) / sizeof(opnames[0]))
			    && opnames[idx])
				perror(opnames[idx]);
			else
				perror("ioctl[unknown???]");
		}
		return -1;
	}
	return 0;
}

static int
set80211param(struct wpa_driver_madwifi_data *drv, int op, int arg,
	      int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.mode = op;
	os_memcpy(iwr.u.name+sizeof(u32), &arg, sizeof(arg));

	if (ioctl(drv->sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		if (show_err) 
			perror("ioctl[IEEE80211_IOCTL_SETPARAM]");
		return -1;
	}
	return 0;
}

static int
wpa_driver_madwifi_set_wpa_ie(struct wpa_driver_madwifi_data *drv,
			      const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* NB: SETOPTIE is not fixed-size so must not be inlined */
	iwr.u.data.pointer = (void *) wpa_ie;
	iwr.u.data.length = wpa_ie_len;

	if (ioctl(drv->sock, IEEE80211_IOCTL_SETOPTIE, &iwr) < 0) {
		perror("ioctl[IEEE80211_IOCTL_SETOPTIE]");
		return -1;
	}
	return 0;
}

static int
wpa_driver_madwifi_del_key(struct wpa_driver_madwifi_data *drv, int key_idx,
			   const u8 *addr)
{
	struct ieee80211req_del_key wk;

	wpa_printf(MSG_DEBUG, "%s: keyidx=%d", __FUNCTION__, key_idx);
	os_memset(&wk, 0, sizeof(wk));
	wk.idk_keyix = key_idx;
	if (addr != NULL)
		os_memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);

	return set80211priv(drv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk), 1);
}

static int
wpa_driver_madwifi_set_key(const char *ifname, void *priv, enum wpa_alg alg,
			   const u8 *addr, int key_idx, int set_tx,
			   const u8 *seq, size_t seq_len,
			   const u8 *key, size_t key_len)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_key wk;
	char *alg_name;
	u_int8_t cipher;

	if (alg == WPA_ALG_NONE)
		return wpa_driver_madwifi_del_key(drv, key_idx, addr);

	switch (alg) {
	case WPA_ALG_WEP:
		if (addr == NULL || os_memcmp(addr, "\xff\xff\xff\xff\xff\xff",
					      ETH_ALEN) == 0) {
			/*
			 * madwifi did not seem to like static WEP key
			 * configuration with IEEE80211_IOCTL_SETKEY, so use
			 * Linux wireless extensions ioctl for this.
			 */
			return wpa_driver_wext_set_key(ifname, drv->wext, alg,
						       addr, key_idx, set_tx,
						       seq, seq_len,
						       key, key_len);
		}
		alg_name = "WEP";
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
	case WPA_ALG_IGTK:
		alg_name = "IGTK";
		cipher  = IEEE80211_CIPHER_AES_CMAC;
		break;
	default:
		wpa_printf(MSG_DEBUG, "%s: unknown/unsupported algorithm %d",
			__FUNCTION__, alg);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: alg=%s key_idx=%d set_tx=%d seq_len=%lu "
		   "key_len=%lu", __FUNCTION__, alg_name, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	if (seq_len > sizeof(u_int64_t)) {
		wpa_printf(MSG_DEBUG, "%s: seq_len %lu too big",
			   __FUNCTION__, (unsigned long) seq_len);
		return -2;
	}
	if (key_len > sizeof(wk.ik_keydata)) {
		wpa_printf(MSG_DEBUG, "%s: key length %lu too big",
			   __FUNCTION__, (unsigned long) key_len);
		return -3;
	}

	os_memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV;
	if (addr == NULL ||
	    os_memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		wk.ik_flags |= IEEE80211_KEY_GROUP;
	if (set_tx) {
		wk.ik_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_DEFAULT;
		os_memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	} else
		os_memset(wk.ik_macaddr, 0, IEEE80211_ADDR_LEN);
	wk.ik_keyix = key_idx;
	wk.ik_keylen = key_len;
#ifdef WORDS_BIGENDIAN
	if (seq) {
		size_t i;
		u8 tmp[WPA_KEY_RSC_LEN];
		os_memset(tmp, 0, sizeof(tmp));
		for (i = 0; i < seq_len; i++)
			tmp[WPA_KEY_RSC_LEN - i - 1] = seq[i];
		os_memcpy(&wk.ik_keyrsc, tmp, WPA_KEY_RSC_LEN);
	}
#else /* WORDS_BIGENDIAN */
	if (seq)
		os_memcpy(&wk.ik_keyrsc, seq, seq_len);
#endif /* WORDS_BIGENDIAN */
	os_memcpy(wk.ik_keydata, key, key_len);

	return set80211priv(drv, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk), 1);
}

static int
wpa_driver_madwifi_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_madwifi_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_COUNTERMEASURES, enabled, 1);
}

static int
wpa_driver_madwifi_deauthenticate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme), 1);
}

static int
wpa_driver_madwifi_disassociate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme), 1);
}

static int
wpa_driver_madwifi_associate(void *priv,
			     struct wpa_driver_associate_params *params)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret = 0, privacy = 1;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

	if (set80211param(drv, IEEE80211_PARAM_DROPUNENCRYPTED,
			  params->drop_unencrypted, 1) < 0)
		ret = -1;
	if (wpa_driver_madwifi_set_auth_alg(drv, params->auth_alg) < 0)
		ret = -1;

	/*
	 * NB: Don't need to set the freq or cipher-related state as
	 *     this is implied by the bssid which is used to locate
	 *     the scanned node state which holds it.  The ssid is
	 *     needed to disambiguate an AP that broadcasts multiple
	 *     ssid's but uses the same bssid.
	 */
	/* XXX error handling is wrong but unclear what to do... */
	if (wpa_driver_madwifi_set_wpa_ie(drv, params->wpa_ie,
					  params->wpa_ie_len) < 0)
		ret = -1;

	if (params->pairwise_suite == CIPHER_NONE &&
	    params->group_suite == CIPHER_NONE &&
	    params->key_mgmt_suite == KEY_MGMT_NONE &&
	    params->wpa_ie_len == 0)
		privacy = 0;

	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, privacy, 1) < 0)
		ret = -1;

	if (params->wpa_ie_len &&
	    set80211param(drv, IEEE80211_PARAM_WPA,
			  params->wpa_ie[0] == WLAN_EID_RSN ? 2 : 1, 1) < 0)
		ret = -1;

	if (params->bssid == NULL) {
		/* ap_scan=2 mode - driver takes care of AP selection and
		 * roaming */
		/* FIX: this does not seem to work; would probably need to
		 * change something in the driver */
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 0, 1) < 0)
			ret = -1;

		if (wpa_driver_wext_set_ssid(drv->wext, params->ssid,
					     params->ssid_len) < 0)
			ret = -1;
	} else {
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0)
			ret = -1;
		if (wpa_driver_wext_set_ssid(drv->wext, params->ssid,
					     params->ssid_len) < 0)
			ret = -1;
		os_memset(&mlme, 0, sizeof(mlme));
		mlme.im_op = IEEE80211_MLME_ASSOC;
		os_memcpy(mlme.im_macaddr, params->bssid, IEEE80211_ADDR_LEN);
		if (set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme,
				 sizeof(mlme), 1) < 0) {
			wpa_printf(MSG_DEBUG, "%s: SETMLME[ASSOC] failed",
				   __func__);
			ret = -1;
		}
	}

	return ret;
}

static int
wpa_driver_madwifi_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_madwifi_data *drv = priv;
	int authmode;

	if ((auth_alg & WPA_AUTH_ALG_OPEN) &&
	    (auth_alg & WPA_AUTH_ALG_SHARED))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_alg & WPA_AUTH_ALG_SHARED)
		authmode = IEEE80211_AUTH_SHARED;
	else
		authmode = IEEE80211_AUTH_OPEN;

	return set80211param(drv, IEEE80211_PARAM_AUTHMODE, authmode, 1);
}

static int
wpa_driver_madwifi_scan(void *priv, struct wpa_driver_scan_params *params)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;
	const u8 *ssid = params->ssids[0].ssid;
	size_t ssid_len = params->ssids[0].ssid_len;

	wpa_driver_madwifi_set_probe_req_ie(drv, params->extra_ies,
					    params->extra_ies_len);

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	/* set desired ssid before scan */
	/* FIX: scan should not break the current association, so using
	 * set_ssid may not be the best way of doing this.. */
	if (wpa_driver_wext_set_ssid(drv->wext, ssid, ssid_len) < 0)
		ret = -1;

	if (ioctl(drv->sock, SIOCSIWSCAN, &iwr) < 0) {
		perror("ioctl[SIOCSIWSCAN]");
		ret = -1;
	}

	/*
	 * madwifi delivers a scan complete event so no need to poll, but
	 * register a backup timeout anyway to make sure that we recover even
	 * if the driver does not send this event for any reason. This timeout
	 * will only be used if the event is not delivered (event handler will
	 * cancel the timeout).
	 */
	eloop_cancel_timeout(wpa_driver_wext_scan_timeout, drv->wext,
			     drv->ctx);
	eloop_register_timeout(30, 0, wpa_driver_wext_scan_timeout, drv->wext,
			       drv->ctx);

	return ret;
}

static int wpa_driver_madwifi_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_driver_madwifi_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static struct wpa_scan_results *
wpa_driver_madwifi_get_scan_results(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext);
}


static int wpa_driver_madwifi_set_operstate(void *priv, int state)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_set_operstate(drv->wext, state);
}


static int wpa_driver_madwifi_set_probe_req_ie(void *priv, const u8 *ies,
					       size_t ies_len)
{
	struct ieee80211req_getset_appiebuf *probe_req_ie;
	int ret;

	probe_req_ie = os_zalloc(sizeof(*probe_req_ie) + ies_len);
	if (probe_req_ie == NULL)
		return -1;

	memset(probe_req_ie, 0, sizeof(*probe_req_ie));
	probe_req_ie->app_frmtype = IEEE80211_APPIE_FRAME_PROBE_REQ;
	probe_req_ie->app_buflen = ies_len;
	os_memcpy(probe_req_ie->app_buf, ies, ies_len);

	ret = set80211priv(priv, IEEE80211_IOCTL_SET_APPIEBUF, probe_req_ie,
			   sizeof(struct ieee80211req_getset_appiebuf) +
			   ies_len, 1);

	os_free(probe_req_ie);

	return ret;
}


static void * wpa_driver_madwifi_init(void *ctx, const char *ifname)
{
	struct wpa_driver_madwifi_data *drv;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->wext = wpa_driver_wext_init(ctx, ifname);
	if (drv->wext == NULL)
		goto fail;

	drv->ctx = ctx;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail2;

	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to set wpa_supplicant-based "
			   "roaming", __FUNCTION__);
		goto fail3;
	}

	if (set80211param(drv, IEEE80211_PARAM_WPA, 3, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable WPA support",
			   __FUNCTION__);
		goto fail3;
	}

	return drv;

fail3:
	close(drv->sock);
fail2:
	wpa_driver_wext_deinit(drv->wext);
fail:
	os_free(drv);
	return NULL;
}


static void wpa_driver_madwifi_deinit(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;

	if (wpa_driver_madwifi_set_wpa_ie(drv, NULL, 0) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to clear WPA IE",
			   __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable driver-based "
			   "roaming", __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to disable forced Privacy "
			   "flag", __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_WPA, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to disable WPA",
			   __FUNCTION__);
	}

	wpa_driver_wext_deinit(drv->wext);

	close(drv->sock);
	os_free(drv);
}

#endif /* HOSTAPD */


const struct wpa_driver_ops wpa_driver_madwifi_ops = {
	.name			= "madwifi",
	.desc			= "MADWIFI 802.11 support (Atheros, etc.)",
	.set_key		= wpa_driver_madwifi_set_key,
#ifdef HOSTAPD
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
	.set_broadcast_ssid	= madwifi_set_broadcast_ssid,
	.sta_clear_stats        = madwifi_sta_clear_stats,
	.commit			= madwifi_commit,
	.send_log		= madwifi_send_log,
	.set_ap_wps_ie		= madwifi_set_ap_wps_ie,
	.set_freq		= madwifi_set_freq,
	.set_intra_bss		= madwifi_set_intra_bss,
	.set_intra_per_bss	= madwifi_set_intra_per_bss,
	.set_bss_isolate	= madwifi_set_bss_isolate,
	.set_bss_assoc_limit	= madwifi_set_bss_assoc_limit,
	.set_total_assoc_limit	= madwifi_set_total_assoc_limit,
	.set_pairing_hash_ie	= madwifi_set_pairing_hash_ie,
	.set_brcm_ioctl         = madwifi_brcm_info_ioctl,
	.set_sta_vlan		= madwifi_set_sta_vlan,
	.set_dyn_vlan		= madwifi_set_dyn_vlan,
	.vlan_group_add		= madwifi_vlan_group_add,
	.vlan_group_remove	= madwifi_vlan_group_remove,
	.set_ap                 = madwifi_set_ap,
#else /* HOSTAPD */
	.get_bssid		= wpa_driver_madwifi_get_bssid,
	.get_ssid		= wpa_driver_madwifi_get_ssid,
	.init			= wpa_driver_madwifi_init,
	.deinit			= wpa_driver_madwifi_deinit,
	.set_countermeasures	= wpa_driver_madwifi_set_countermeasures,
	.scan2			= wpa_driver_madwifi_scan,
	.get_scan_results2	= wpa_driver_madwifi_get_scan_results,
	.deauthenticate		= wpa_driver_madwifi_deauthenticate,
	.disassociate		= wpa_driver_madwifi_disassociate,
	.associate		= wpa_driver_madwifi_associate,
	.set_operstate		= wpa_driver_madwifi_set_operstate,
#endif /* HOSTAPD */
};

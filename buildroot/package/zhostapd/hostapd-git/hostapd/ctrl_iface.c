/*
 * hostapd / UNIX domain socket -based control interface
 * Copyright (c) 2004-2010, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include <sys/un.h>
#include <sys/stat.h>
#include <stddef.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/version.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "radius/radius_client.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ieee802_1x.h"
#include "ap/wpa_auth.h"
#include "ap/ieee802_11.h"
#include "ap/sta_info.h"
#include "ap/accounting.h"
#include "ap/wps_hostapd.h"
#include "ap/ctrl_iface_ap.h"
#include "ap/ap_drv_ops.h"
#include "wps/wps_defs.h"
#include "wps/wps.h"
#include "ctrl_iface.h"
#include "ap/wpa_auth_i.h"


struct wpa_ctrl_dst {
	struct wpa_ctrl_dst *next;
	struct sockaddr_un addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    const char *buf, size_t len);

static struct hostapd_data *hostapd_find_bss(struct hostapd_iface *iface, const char *bss_name)
{
	int i;

	if ((iface == NULL) || (bss_name == NULL))
		return NULL;

	for (i = 0; i < iface->num_bss; i++) {
		if (strncmp(iface->bss[i]->conf->iface, bss_name, IFNAMSIZ) == 0)
			return iface->bss[i];
	}

	return NULL;
}

static int hostapd_ctrl_iface_attach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dst = os_zalloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	os_memcpy(&dst->addr, from, sizeof(struct sockaddr_un));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dst->next = hapd->ctrl_dst;
	hapd->ctrl_dst = dst;
	wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor attached",
		    (u8 *) from->sun_path,
		    fromlen - offsetof(struct sockaddr_un, sun_path));
	return 0;
}


static int hostapd_ctrl_iface_detach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst, *prev = NULL;

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			if (prev == NULL)
				hapd->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			os_free(dst);
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor detached",
				    (u8 *) from->sun_path,
				    fromlen -
				    offsetof(struct sockaddr_un, sun_path));
			return 0;
		}
		prev = dst;
		dst = dst->next;
	}
	return -1;
}


static int hostapd_ctrl_iface_level(struct hostapd_data *hapd,
				    struct sockaddr_un *from,
				    socklen_t fromlen,
				    char *level)
{
	struct wpa_ctrl_dst *dst;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE LEVEL %s", level);

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE changed monitor "
				    "level", (u8 *) from->sun_path, fromlen -
				    offsetof(struct sockaddr_un, sun_path));
			dst->debug_level = atoi(level);
			return 0;
		}
		dst = dst->next;
	}

	return -1;
}


static int hostapd_ctrl_iface_new_sta(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE NEW_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return 0;

	wpa_printf(MSG_DEBUG, "Add new STA " MACSTR " based on ctrl_iface "
		   "notification", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta == NULL)
		return -1;

	hostapd_new_assoc_sta(hapd, sta, 0);
	return 0;
}


#ifdef CONFIG_P2P_MANAGER
static int p2p_manager_disconnect(struct hostapd_data *hapd, u16 stype,
				  u8 minor_reason_code, const u8 *addr)
{
	struct ieee80211_mgmt *mgmt;
	int ret;
	u8 *pos;

	if (hapd->driver->send_frame == NULL)
		return -1;

	mgmt = os_zalloc(sizeof(*mgmt) + 100);
	if (mgmt == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "P2P: Disconnect STA " MACSTR " with minor "
		   "reason code %u (stype=%u)",
		   MAC2STR(addr), minor_reason_code, stype);

	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, stype);
	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	if (stype == WLAN_FC_STYPE_DEAUTH) {
		mgmt->u.deauth.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = (u8 *) (&mgmt->u.deauth.reason_code + 1);
	} else {
		mgmt->u.disassoc.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = (u8 *) (&mgmt->u.disassoc.reason_code + 1);
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = 4 + 3 + 1;
	WPA_PUT_BE24(pos, OUI_WFA);
	pos += 3;
	*pos++ = P2P_OUI_TYPE;

	*pos++ = P2P_ATTR_MINOR_REASON_CODE;
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	*pos++ = minor_reason_code;

	ret = hapd->driver->send_frame(hapd->drv_priv, (u8 *) mgmt,
				       pos - (u8 *) mgmt, 1);
	os_free(mgmt);

	return ret < 0 ? -1 : 0;
}
#endif /* CONFIG_P2P_MANAGER */


static int hostapd_ctrl_iface_deauthenticate(struct hostapd_data *hapd,
					     const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE DEAUTHENTICATE %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " test=");
	if (pos) {
		struct ieee80211_mgmt mgmt;
		int encrypt;
		if (hapd->driver->send_frame == NULL)
			return -1;
		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DEAUTH);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.deauth.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		if (hapd->driver->send_frame(hapd->drv_priv, (u8 *) &mgmt,
					     IEEE80211_HDRLEN +
					     sizeof(mgmt.u.deauth),
					     encrypt) < 0)
			return -1;
		return 0;
	}

#ifdef CONFIG_P2P_MANAGER
	pos = os_strstr(txtaddr, " p2p=");
	if (pos) {
		return p2p_manager_disconnect(hapd, WLAN_FC_STYPE_DEAUTH,
					      atoi(pos + 5), addr);
	}
#endif /* CONFIG_P2P_MANAGER */

	hostapd_drv_sta_deauth(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_deauthenticate(hapd, sta,
				      WLAN_REASON_PREV_AUTH_NOT_VALID);
	else if (addr[0] == 0xff)
		hostapd_free_stas(hapd);

	return 0;
}


static int hostapd_ctrl_iface_disassociate(struct hostapd_data *hapd,
					   const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE DISASSOCIATE %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " test=");
	if (pos) {
		struct ieee80211_mgmt mgmt;
		int encrypt;
		if (hapd->driver->send_frame == NULL)
			return -1;
		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DISASSOC);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.disassoc.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		if (hapd->driver->send_frame(hapd->drv_priv, (u8 *) &mgmt,
					     IEEE80211_HDRLEN +
					     sizeof(mgmt.u.deauth),
					     encrypt) < 0)
			return -1;
		return 0;
	}

#ifdef CONFIG_P2P_MANAGER
	pos = os_strstr(txtaddr, " p2p=");
	if (pos) {
		return p2p_manager_disconnect(hapd, WLAN_FC_STYPE_DISASSOC,
					      atoi(pos + 5), addr);
	}
#endif /* CONFIG_P2P_MANAGER */

	hostapd_drv_sta_disassoc(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_disassociate(hapd, sta,
				    WLAN_REASON_PREV_AUTH_NOT_VALID);
	else if (addr[0] == 0xff)
		hostapd_free_stas(hapd);

	return 0;
}


#ifdef CONFIG_IEEE80211W
#ifdef NEED_AP_MLME
static int hostapd_ctrl_iface_sa_query(struct hostapd_data *hapd,
				       const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	u8 trans_id[WLAN_SA_QUERY_TR_ID_LEN];

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SA_QUERY %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr) ||
	    os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0)
		return -1;

	ieee802_11_send_sa_query_req(hapd, addr, trans_id);

	return 0;
}
#endif /* NEED_AP_MLME */
#endif /* CONFIG_IEEE80211W */


#ifdef CONFIG_WPS
static int hostapd_ctrl_iface_wps_pin(struct hostapd_data *hapd, char *txt)
{
	char *pin = os_strchr(txt, ' ');
	char *timeout_txt;
	int timeout;
	u8 addr_buf[ETH_ALEN], *addr = NULL;
	char *pos;

	if (pin == NULL)
		return -1;
	*pin++ = '\0';

	timeout_txt = os_strchr(pin, ' ');
	if (timeout_txt) {
		*timeout_txt++ = '\0';
		timeout = atoi(timeout_txt);
		pos = os_strchr(timeout_txt, ' ');
		if (pos) {
			*pos++ = '\0';
			if (hwaddr_aton(pos, addr_buf) == 0)
				addr = addr_buf;
		}
	} else {
		timeout = WPS_PIN_DEFAULT_TIMEOUT;
	}

	return hostapd_wps_add_pin(hapd, 0, addr, txt, pin, timeout);
}

static int hostapd_ctrl_iface_wps_pin_bss(struct hostapd_data *hapd, char *txt)
{
	char *uuid_txt = os_strchr(txt, ' ');
	char *pin;
	char *timeout_txt;
	int timeout;
	u8 addr_buf[ETH_ALEN], *addr = NULL;
	char *pos;
	struct hostapd_data *sel_bss;

	if (uuid_txt == NULL)
		return -1;
	*uuid_txt++ = '\0';

	sel_bss = hostapd_find_bss(hapd->iface, txt);
	if (sel_bss == NULL)
		return -1;

	pin = os_strchr(uuid_txt, ' ');
	if (pin == NULL)
		return -1;
	*pin++ = '\0';

	timeout_txt = os_strchr(pin, ' ');
	if (timeout_txt) {
		*timeout_txt++ = '\0';
		timeout = atoi(timeout_txt);
		pos = os_strchr(timeout_txt, ' ');
		if (pos) {
			*pos++ = '\0';
			if (hwaddr_aton(pos, addr_buf) == 0)
				addr = addr_buf;
		}
	} else {
		timeout = WPS_PIN_DEFAULT_TIMEOUT;
	}

	return hostapd_wps_add_pin(sel_bss, 1, addr, uuid_txt, pin, timeout);
}

static int hostapd_ctrl_iface_wps_check_pin(
	struct hostapd_data *hapd, char *cmd, char *buf, size_t buflen)
{
	char pin[9];
	size_t len;
	char *pos;
	int ret;

	wpa_hexdump_ascii_key(MSG_DEBUG, "WPS_CHECK_PIN",
			      (u8 *) cmd, os_strlen(cmd));
	for (pos = cmd, len = 0; *pos != '\0'; pos++) {
		if (*pos < '0' || *pos > '9')
			continue;
		pin[len++] = *pos;
		if (len == 9) {
			wpa_printf(MSG_DEBUG, "WPS: Too long PIN");
			return -1;
		}
	}
	if (len != 4 && len != 8) {
		wpa_printf(MSG_DEBUG, "WPS: Invalid PIN length %d", (int) len);
		return -1;
	}
	pin[len] = '\0';

	if (len == 8) {
		unsigned int pin_val;
		pin_val = atoi(pin);
		if (!wps_pin_valid(pin_val)) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid checksum digit");
			ret = os_snprintf(buf, buflen, "FAIL-CHECKSUM\n");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}
	}

	ret = os_snprintf(buf, buflen, "%s", pin);
	if (ret < 0 || (size_t) ret >= buflen)
		return -1;

	return ret;
}


#ifdef CONFIG_WPS_OOB
static int hostapd_ctrl_iface_wps_oob(struct hostapd_data *hapd, char *txt)
{
	char *path, *method, *name;

	path = os_strchr(txt, ' ');
	if (path == NULL)
		return -1;
	*path++ = '\0';

	method = os_strchr(path, ' ');
	if (method == NULL)
		return -1;
	*method++ = '\0';

	name = os_strchr(method, ' ');
	if (name != NULL)
		*name++ = '\0';

	return hostapd_wps_start_oob(hapd, txt, path, method, name);
}
#endif /* CONFIG_WPS_OOB */


static int hostapd_ctrl_iface_wps_ap_pin(struct hostapd_data *hapd, char *txt,
					 char *buf, size_t buflen)
{
	int timeout = WPS_AP_PIN_DEFAULT_TIMEOUT;
	char *pos;
	const char *pin_txt;

	pos = os_strchr(txt, ' ');
	if (pos)
		*pos++ = '\0';

	if (os_strcmp(txt, "disable") == 0) {
		hostapd_wps_ap_pin_disable(hapd);
		return os_snprintf(buf, buflen, "OK\n");
	}

	if (os_strcmp(txt, "enable") == 0) {
		hostapd_wps_ap_pin_enable(hapd, 0);
		return os_snprintf(buf, buflen, "OK\n");
	}

	if (os_strcmp(txt, "random") == 0) {
		if (pos)
			timeout = atoi(pos);
		pin_txt = hostapd_wps_ap_pin_random(hapd, timeout);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "get") == 0) {
		pin_txt = hostapd_wps_ap_pin_get(hapd);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "set") == 0) {
		char *pin;
		if (pos == NULL)
			return -1;
		pin = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			timeout = atoi(pos);
		}
		if (os_strlen(pin) > buflen)
			return -1;
		if (hostapd_wps_ap_pin_set(hapd, pin, timeout) < 0)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin);
	}

	return -1;
}


static int hostapd_ctrl_iface_wps_config(struct hostapd_data *hapd, char *txt)
{
	char *pos;
	char *ssid, *auth, *encr = NULL, *key = NULL;

	ssid = txt;
	pos = os_strchr(txt, ' ');
	if (!pos)
		return -1;
	*pos++ = '\0';

	auth = pos;
	pos = os_strchr(pos, ' ');
	if (pos) {
		*pos++ = '\0';
		encr = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			key = pos;
		}
	}

	return hostapd_wps_config_ap(hapd, ssid, auth, encr, key);
}
#endif /* CONFIG_WPS */

#ifdef CONFIG_WPS
static int hostapd_ctrl_iface_get_wps_configured_state(struct hostapd_data *hapd,
					 char *buf, size_t buflen) {
	int ret;
	char *pos, *end;
	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "%s\n",
			  hapd->conf->wps_state == 0 ? "disabled" :
			  (hapd->conf->wps_state == 1 ? "not configured" :
			   "configured"));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	return pos - buf;
}
#endif

static int hostapd_ctrl_iface_get_config(struct hostapd_data *hapd,
					 char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "bssid=" MACSTR "\n"
			  "ssid=%s\n",
			  MAC2STR(hapd->own_addr),
			  hapd->conf->ssid.ssid);
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

#ifdef CONFIG_WPS
	ret = os_snprintf(pos, end - pos, "wps_state=%s\n",
			  hapd->conf->wps_state == 0 ? "disabled" :
			  (hapd->conf->wps_state == 1 ? "not configured" :
			   "configured"));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_passphrase) {
		ret = os_snprintf(pos, end - pos, "passphrase=%s\n",
				  hapd->conf->ssid.wpa_passphrase);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_psk &&
	    hapd->conf->ssid.wpa_psk->group) {
		char hex[PMK_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 hapd->conf->ssid.wpa_psk->psk, PMK_LEN);
		ret = os_snprintf(pos, end - pos, "psk=%s\n", hex);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_WPS */

	if (hapd->conf->wpa && hapd->conf->wpa_key_mgmt) {
		ret = os_snprintf(pos, end - pos, "key_mgmt=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
			ret = os_snprintf(pos, end - pos, "WPA-PSK ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
			ret = os_snprintf(pos, end - pos, "WPA-EAP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
#ifdef CONFIG_IEEE80211R
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_PSK) {
			ret = os_snprintf(pos, end - pos, "FT-PSK ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X) {
			ret = os_snprintf(pos, end - pos, "FT-EAP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK_SHA256) {
			ret = os_snprintf(pos, end - pos, "WPA-PSK-SHA256 ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256) {
			ret = os_snprintf(pos, end - pos, "WPA-EAP-SHA256 ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
#endif /* CONFIG_IEEE80211W */

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa && hapd->conf->wpa_group == WPA_CIPHER_CCMP) {
		ret = os_snprintf(pos, end - pos, "group_cipher=CCMP\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	} else if (hapd->conf->wpa &&
		   hapd->conf->wpa_group == WPA_CIPHER_TKIP) {
		ret = os_snprintf(pos, end - pos, "group_cipher=TKIP\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && hapd->conf->rsn_pairwise) {
		ret = os_snprintf(pos, end - pos, "rsn_pairwise_cipher=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		if (hapd->conf->rsn_pairwise & WPA_CIPHER_CCMP) {
			ret = os_snprintf(pos, end - pos, "CCMP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->rsn_pairwise & WPA_CIPHER_TKIP) {
			ret = os_snprintf(pos, end - pos, "TKIP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_WPA) && hapd->conf->wpa_pairwise) {
		ret = os_snprintf(pos, end - pos, "wpa_pairwise_cipher=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		if (hapd->conf->wpa_pairwise & WPA_CIPHER_CCMP) {
			ret = os_snprintf(pos, end - pos, "CCMP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_pairwise & WPA_CIPHER_TKIP) {
			ret = os_snprintf(pos, end - pos, "TKIP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_set(struct hostapd_data *hapd, char *cmd)
{
	char *value;
	int ret = 0;

	value = os_strchr(cmd, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET '%s'='%s'", cmd, value);
	if (0) {
#ifdef CONFIG_WPS_TESTING
	} else if (os_strcasecmp(cmd, "wps_version_number") == 0) {
		long int val;
		val = strtol(value, NULL, 0);
		if (val < 0 || val > 0xff) {
			ret = -1;
			wpa_printf(MSG_DEBUG, "WPS: Invalid "
				   "wps_version_number %ld", val);
		} else {
			wps_version_number = val;
			wpa_printf(MSG_DEBUG, "WPS: Testing - force WPS "
				   "version %u.%u",
				   (wps_version_number & 0xf0) >> 4,
				   wps_version_number & 0x0f);
			hostapd_wps_update_ie(hapd);
		}
	} else if (os_strcasecmp(cmd, "wps_testing_dummy_cred") == 0) {
		wps_testing_dummy_cred = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - dummy_cred=%d",
			   wps_testing_dummy_cred);
#endif /* CONFIG_WPS_TESTING */
	} else if (os_strcasecmp(cmd, "default_pbc_bss") == 0) {
		struct hostapd_iface *iface = hapd->iface;
		int i;

		if (os_strcasecmp(value, "null") == 0) {
			iface->default_bss_for_pbc = NULL;
		} else {
			for (i = 0; i < iface->num_bss; i++) {
				if (os_strncmp(iface->bss[i]->conf->iface, value, IFNAMSIZ) == 0) {
					iface->default_bss_for_pbc = iface->bss[i];
					break;
				}
			}

			if (i == iface->num_bss)
				ret = -1;
		}
	} else {
		ret = -1;
	}

	return ret;
}

static int hostapd_ctrl_iface_get(struct hostapd_data *hapd, char *cmd,
				  char *buf, size_t buflen)
{
	int res;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET '%s'", cmd);

	if (os_strcmp(cmd, "version") == 0) {
		res = os_snprintf(buf, buflen, "%s", VERSION_STR);
		if (res < 0 || (unsigned int) res >= buflen)
			return -1;
		return res;
	} else if (hapd->wps && os_strcmp(cmd, "uuid") == 0) {
		/* This is a 16-size hexs array, here we convert them to string in UUID format*/
		res = uuid_bin2str(hapd->wps->uuid, buf, 41);
		if (!res)
			res = strlen(buf); /*get out the uuid's length*/

		if (res < 0 || (unsigned int) res >= buflen)
			return -1;

		return res;

	} else if (hapd->wps && os_strcmp(cmd, "device_name") == 0) {
		if (hapd->wps->dev.device_name) {
			res = os_snprintf(buf, buflen, "%s", hapd->wps->dev.device_name);
		} else {
			res = -1;
		}

		if (res < 0 || (unsigned int) res >= buflen)
			return -1;

		return res;

	} else if (hapd->wps && os_strcmp(cmd, "os_version") == 0) {
		char os_buf[4] = {0};

		WPA_PUT_BE32(os_buf, hapd->wps->dev.os_version);
		res = sprintf(buf, "%02x%02x%02x%02x", os_buf[0], os_buf[1], os_buf[2], os_buf[3]);

		if (res < 0 || (unsigned int) res > buflen)
			return -1;

		return res;

	} else if (hapd->wps && os_strcmp(cmd, "config_methods") == 0) {

		res = os_snprintf(buf, buflen,
#ifdef CONFIG_WPS2
				"%s%s%s%s"
#else
				"%s%s"
#endif
				"%s%s%s%s%s%s%s",

#ifdef CONFIG_WPS2
			    ((hapd->wps->config_methods & WPS_CONFIG_VIRT_DISPLAY)
					== WPS_CONFIG_VIRT_DISPLAY) ? " virtual_display" : "",
			    ((hapd->wps->config_methods & WPS_CONFIG_PHY_DISPLAY)
					== WPS_CONFIG_PHY_DISPLAY) ? " physical_display" : "",
			    ((hapd->wps->config_methods & WPS_CONFIG_VIRT_PUSHBUTTON)
					== WPS_CONFIG_VIRT_PUSHBUTTON) ? " virtual_push_button" : "",
			    ((hapd->wps->config_methods & WPS_CONFIG_PHY_PUSHBUTTON)
					== WPS_CONFIG_PHY_PUSHBUTTON) ? " physical_push_button" : "",
#else
			    hapd->wps->config_methods & WPS_CONFIG_DISPLAY ? " display" : "",
			    hapd->wps->config_methods & WPS_CONFIG_PUSHBUTTON ? " push_button" : "",
#endif
			    hapd->wps->config_methods & WPS_CONFIG_USBA ? " usba" : "",
			    hapd->wps->config_methods & WPS_CONFIG_ETHERNET ? " ethernet" : "",
			    hapd->wps->config_methods & WPS_CONFIG_LABEL ? " label" : "",
			    hapd->wps->config_methods& WPS_CONFIG_EXT_NFC_TOKEN ? " ext_nfc_token" : "",
			    hapd->wps->config_methods & WPS_CONFIG_INT_NFC_TOKEN ? " int_nfc_token" : "",
			    hapd->wps->config_methods & WPS_CONFIG_NFC_INTERFACE ? " nfc_interface" : "",
			    hapd->wps->config_methods & WPS_CONFIG_KEYPAD ? " keypad" : ""

		);

		if (res < 0 || (unsigned int) res >= buflen)
			return -1;
		return res;

	} else if (hapd->wps && os_strcmp(cmd, "ap_setup_locked") == 0) {

		res = sprintf(buf, "%d", hapd->wps->ap_setup_locked);

		if (res < 0 || (unsigned int) res >= buflen)
			return -1;
		return res;
	} else if (os_strcmp(cmd, "default_pbc_bss") == 0) {
		struct hostapd_iface *iface = hapd->iface;

		if (iface->default_bss_for_pbc != NULL) {
			res = snprintf(buf, buflen, "%s", iface->default_bss_for_pbc->conf->iface);
		} else {
			res = snprintf(buf, buflen, "null");
		}

		if (res < 0 || (unsigned int) res >= buflen)
			return -1;
		else
			return res;
	}

	return -1;
}

static int hostapd_ctrl_iface_bss_set(struct hostapd_data *hapd, char *cmd)
{
	char *bss_name;
	char *value;
	struct hostapd_data *sel_bss;
	int ret = 0;

	bss_name = os_strchr(cmd, ' ');
	if (NULL == bss_name)
		return -1;
	*bss_name++ = '\0';

	value = os_strchr(bss_name, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	if ((sel_bss = hostapd_find_bss(hapd->iface, bss_name)) == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET BSS interface[%s] '%s'='%s'", bss_name, cmd, value);
	if (os_strcasecmp(cmd, "wps_on_hidden_ssid") == 0) {
		int enable = !!atoi(value);

		if (sel_bss->qtn_wps_on_hidden_ssid != enable) {
			sel_bss->qtn_wps_on_hidden_ssid = enable;
			ret = hostapd_update_bss_config(sel_bss->iface, sel_bss->conf->iface);
		}
	} else if (os_strcasecmp(cmd, "force_broadcast_uuid") == 0) {
		int flag = !!atoi(value);

		if (sel_bss->force_broadcast_uuid != flag) {
			sel_bss->force_broadcast_uuid = flag;
			ret = hostapd_update_bss_config(sel_bss->iface, sel_bss->conf->iface);
		}
	} else if (os_strcasecmp(cmd, "ap_setup_locked") == 0) {
		int val = !!atoi(value);

		if (sel_bss->wps) {
			sel_bss->wps->ap_setup_locked = val;
			sel_bss->ap_setup_locked_runtime = val;
			ret = wps_registrar_update_ie(sel_bss->wps->registrar);
		}
	} else if (os_strcasecmp(cmd, "auto_lockdown_fail_count") == 0) {
		int val = atoi(value);

		if (sel_bss->wps) {
			if (strcmp(hostapd_wps_get_ap_pin_fail_method(sel_bss), "auto_lockdown") == 0) {
				sel_bss->ap_pin_fail_data.auto_lockdown.accu_fail = val;
			} else
				ret = -1;
		}
	} else {
		ret = -1;
	}

	return ret;
}

static int hostapd_ctrl_iface_bss_get(struct hostapd_data *hapd, char *cmd, char *reply, int reply_size)
{
	char *bss_name;
	struct hostapd_data *sel_bss;
	int ret = 0;

	bss_name = os_strchr(cmd, ' ');
	if (bss_name == NULL)
		return -1;
	*bss_name++ = '\0';

	if ((sel_bss = hostapd_find_bss(hapd->iface, bss_name)) == NULL)
		return -1;

	if (os_strcasecmp(cmd, "wps_on_hidden_ssid") == 0) {
		ret = snprintf(reply, reply_size, "%s",
				sel_bss->qtn_wps_on_hidden_ssid ? "on" : "off");
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (sel_bss->wps && os_strcmp(cmd, "uuid") == 0) {
		/* This is a 16-size hexs array, here we convert them to string in UUID format*/
		ret = uuid_bin2str(sel_bss->wps->uuid, reply, 41);
		if (!ret)
			ret = strlen(reply); /*get out the uuid's length*/
		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;

		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "device_name") == 0) {
		if (sel_bss->wps->dev.device_name) {
			ret = os_snprintf(reply, reply_size, "%s", sel_bss->wps->dev.device_name);
		} else {
			ret = -1;
		}
		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;

		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "os_version") == 0) {
		char os_buf[4] = {0};

		WPA_PUT_BE32(os_buf, sel_bss->wps->dev.os_version);
		ret = sprintf(reply, "%02x%02x%02x%02x", os_buf[0], os_buf[1], os_buf[2], os_buf[3]);

		if (ret < 0 || (unsigned int) ret > reply_size)
			return -1;

		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "config_methods") == 0) {
		ret = os_snprintf(reply, reply_size,
#ifdef CONFIG_WPS2
				"%s%s%s%s"
#else
				"%s%s"
#endif
				"%s%s%s%s%s%s%s",

#ifdef CONFIG_WPS2
			    ((sel_bss->wps->config_methods & WPS_CONFIG_VIRT_DISPLAY)
					== WPS_CONFIG_VIRT_DISPLAY) ? " virtual_display" : "",
			    ((sel_bss->wps->config_methods & WPS_CONFIG_PHY_DISPLAY)
					== WPS_CONFIG_PHY_DISPLAY) ? " physical_display" : "",
			    ((sel_bss->wps->config_methods & WPS_CONFIG_VIRT_PUSHBUTTON)
					== WPS_CONFIG_VIRT_PUSHBUTTON) ? " virtual_push_button" : "",
			    ((sel_bss->wps->config_methods & WPS_CONFIG_PHY_PUSHBUTTON)
					== WPS_CONFIG_PHY_PUSHBUTTON) ? " physical_push_button" : "",
#else
			    sel_bss->wps->config_methods & WPS_CONFIG_DISPLAY ? " display" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_PUSHBUTTON ? " push_button" : "",
#endif
			    sel_bss->wps->config_methods & WPS_CONFIG_USBA ? " usba" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_ETHERNET ? " ethernet" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_LABEL ? " label" : "",
			    sel_bss->wps->config_methods& WPS_CONFIG_EXT_NFC_TOKEN ? " ext_nfc_token" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_INT_NFC_TOKEN ? " int_nfc_token" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_NFC_INTERFACE ? " nfc_interface" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_KEYPAD ? " keypad" : ""

		);

		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;
		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "ap_setup_locked") == 0) {
		ret = sprintf(reply, "%d", sel_bss->wps->ap_setup_locked);
		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;
		return ret;
	} else if (os_strcasecmp(cmd, "force_broadcast_uuid") == 0) {
		ret = snprintf(reply, reply_size, "%s",
				sel_bss->force_broadcast_uuid ? "on" : "off");
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "ap_pin_fail_method") == 0) {
		ret = snprintf(reply, reply_size, "%s", hostapd_wps_get_ap_pin_fail_method(sel_bss));
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (sel_bss->wps && os_strcasecmp(cmd, "wps_vendor_spec") == 0) {
		switch (sel_bss->wps->vendor) {
		case WPS_VENDOR_NONE:
			ret = snprintf(reply, reply_size, "NO Vendor");
			break;
		case WPS_VENDOR_NETGEAR:
			ret = snprintf(reply, reply_size, "Netgear");
			break;
		default:
			ret = snprintf(reply, reply_size, "Unknown Vendor");
			break;
		}
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd,"auto_lockdown_max_retry") == 0) {
		if (strcmp(hostapd_wps_get_ap_pin_fail_method(sel_bss), "auto_lockdown") == 0) {
			ret = snprintf(reply, reply_size, "%d",
					sel_bss->ap_pin_fail_data.auto_lockdown.fail_threshold);
			if (ret < 0 || ret >= reply_size)
				ret = -1;
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd, "last_wps_client") == 0) {
		if (sel_bss->wps != NULL) {
			ret = snprintf(reply, reply_size,  MACSTR "-%s",
				MAC2STR(sel_bss->wps->last_wps_client),
				sel_bss->wps->last_wps_client_wps_type);
			if (ret < 0 || ret >= reply_size)
				ret = -1;
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd,"auto_lockdown_fail_count") == 0) {
		if (strcmp(hostapd_wps_get_ap_pin_fail_method(sel_bss), "auto_lockdown") == 0) {
			ret = snprintf(reply, reply_size, "%d",
					sel_bss->ap_pin_fail_data.auto_lockdown.accu_fail);
			if (ret < 0 || ret >= reply_size)
				ret = -1;
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd, "last_wps_client_devname") == 0) {
		if (sel_bss->wps != NULL &&
			!is_zero_ether_addr(sel_bss->wps->last_wps_client)) {
			ret = wps_registrar_get_peer_devname(sel_bss->wps->registrar,
					sel_bss->wps->last_wps_client, reply, reply_size);
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	return ret;
}

#define WPA_HANDSHAKING_STR		"WPA_HANDSHAKING"
#define NO_WPA_HANDSHAKING_STR	"NO_WPA_HANDSHAKING"
#define WPA_SUCCESS_STR			"WPA_SUCCESS"
#define WPA_INITIAL_STR			"WPA_INITIAL"
#define MAC_ADDR_STR_LEN		17
static int hostapd_ctrl_iface_wpa_status(struct hostapd_data *hapd, char *mac_addr,
		  char *buf, size_t buflen)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN], zero_addr[ETH_ALEN] = {0};
	int ap_wpa_handshaking = 0;
	int sta_exist = 0;
	int res = -1;

	if (strlen(mac_addr) != MAC_ADDR_STR_LEN || hwaddr_aton(mac_addr, addr))
		return res;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (os_memcmp(sta->addr, addr, sizeof(addr)) == 0) {
			if ((sta->flags & WLAN_STA_AUTH) && !(sta->flags & WLAN_STA_AUTHORIZED)) {
				res = sprintf(buf, "%s", WPA_HANDSHAKING_STR);
			} else if (sta->flags & WLAN_STA_AUTHORIZED) {
				res = sprintf(buf, "%s", WPA_SUCCESS_STR);
			} else if (!(sta->flags & WLAN_STA_AUTH)) {
				res = sprintf(buf, "%s", WPA_INITIAL_STR);
			}
			sta_exist = 1;
			break;
		} else if (os_memcmp(addr, zero_addr, sizeof(addr)) ==  0) {
			if ((sta->flags & WLAN_STA_AUTH) && !(sta->flags & WLAN_STA_AUTHORIZED)) {
				ap_wpa_handshaking++;
				wpa_printf(MSG_DEBUG, "STA " MACSTR " handshaking with the AP",
					   MAC2STR(sta->addr));
			}
		}
	}

	/*
	 * Mac addr 00:00:00:00:00:00 here is used to check whether a WPA handshaking exists
	 * in the AP without specific address
	 */
	if (os_memcmp(addr, zero_addr, sizeof(addr)) ==  0) {
		if (ap_wpa_handshaking) {
			res = sprintf(buf, "%s", WPA_HANDSHAKING_STR);
		} else {
			res = sprintf(buf, "%s", NO_WPA_HANDSHAKING_STR);
		}

		sta_exist = 1;

	}

	if (sta_exist == 0) {
		res = sprintf(buf, "%s", "station not found");
	}

	return res;
}

static int hostapd_ctrl_iface_wps_timeout(struct hostapd_data *hapd, char *txt, char *reply, int size)
{
#define WPS_TIMEOUT_MAX		600
#define WPS_TIMEOUT_MIN		(WPS_PBC_WALK_TIME)
	int timeout_val;
	char tmp_buf[256];

	timeout_val = atoi(txt);
	if ((timeout_val < WPS_TIMEOUT_MIN) || (timeout_val > WPS_TIMEOUT_MAX)) {
		os_snprintf(tmp_buf, sizeof(tmp_buf), "FAIL: wps timeout should be set to [%d,%d]\n",
				WPS_TIMEOUT_MIN, WPS_TIMEOUT_MAX);
	} else {
		qtn_wps_pbc_timeout = timeout_val;
		os_snprintf(tmp_buf, sizeof(tmp_buf), "OK\n");
	}
	os_strlcpy(reply, tmp_buf, size);

	return (strlen(tmp_buf) + 1);
#undef WPS_TIMEOUT_MAX
#undef WPS_TIMEOUT_MIN
}

static int hostapd_ctrl_iface_get_psk_auth_failure(struct hostapd_data *hapd,
		char *ifname, char *buf, size_t buflen)
{
	int res;
	int i;
	struct hostapd_data *hapd2 = NULL;
	struct wpa_authenticator *wpa_auth = NULL;
	struct hostapd_bss_config *bss_config = NULL;
	struct hostapd_iface *hapd_iface = hapd->iface;

	if (ifname == NULL || buf == NULL)
		return -1;

	for (i = 0; hapd_iface->num_bss; i++) {
		bss_config = hapd_iface->bss[i]->conf;
		if (os_strcmp(ifname, bss_config->iface) == 0) {
			hapd2 = hapd_iface->bss[i];
			break;
		}
	}

	if (hapd2 == NULL)
		return -1;
	if ((wpa_auth = hapd2->wpa_auth) == NULL)
		return -1;

	res = os_snprintf(buf, buflen, "%u", wpa_auth->dot11RSNA4WayHandshakeFailures);
	if (res < 0 || (unsigned int)res >= buflen)
		return -1;

	return res;
}

static int hostapd_ctrl_iface_pbc_in_srcm(struct hostapd_data *hapd, char *txt)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_data * hapd_data;
	int flag = !!atoi(txt);
	int ret_val = 0;
	int i;

	if (flag != hapd->iconf->pbc_in_srcm) {
		hapd->iconf->pbc_in_srcm = flag;

		for (i = 0; i < iface->num_bss; i++) {
			hapd_data = iface->bss[i];
			if (hapd_data && (hapd_data->wps))
				hostapd_update_wps(hapd_data);
		}
	}

	return ret_val;
}

static int hostapd_ctrl_iface_wps_upnp_enable(struct hostapd_data *hapd, char *option,
		  char *buf, size_t buflen)
{
	int enable;

	if (hapd->wps == NULL)
		return -1;

	enable = atoi(option);
	if ((strlen(option) != 1) || (isdigit(option[0]) == 0) || ((enable != 0) && (enable != 1))) {
		return -1;
	}

	if (enable && hapd->wps_upnp == NULL) {
		if (hapd->conf->upnp_iface == NULL) {
			hapd->conf->upnp_iface = os_strdup("br0");
		}
		hostapd_wps_upnp_init(hapd, hapd->wps);
	}

	if (!enable && hapd->wps_upnp != NULL) {
		hostapd_wps_upnp_deinit(hapd);
	}

	return 0;
}

static int hostapd_ctrl_iface_wps_upnp_status(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	char *response[] = {"0\n", "1\n"};
	int select_resp;

	if (hapd->wps_upnp != NULL)
		select_resp = 1;
	else
		select_resp = 0;
	os_memcpy(buf, response[select_resp], strlen(response[select_resp]) + 1);

	return strlen(response[select_resp]) + 1;
}

static int hostapd_ctrl_iface_non_wps_pp_enable(struct hostapd_data *hapd, char *buf)
{
	char *bss_name;
	char *enable_str;
	int enable;
	struct hostapd_data *hapd_bss;

	bss_name = buf;
	enable_str = os_strchr(bss_name, ' ');
	if (enable_str == NULL)
		return -1;
	*enable_str++ = '\0';

	hapd_bss = hostapd_find_bss(hapd->iface, bss_name);
	if (hapd_bss == NULL)
		return -1;

	enable = atoi(enable_str);
	if (enable == 0)
		hapd_bss->non_wps_pp_enable = 0;
	else
		hapd_bss->non_wps_pp_enable = 1;

	return hostapd_update_bss_config(hapd->iface, bss_name);
}

static int hostapd_ctrl_iface_non_wps_pp_status(struct hostapd_data *hapd, char *buf,
		char *reply, int reply_len)
{
	char *bss_name;
	struct hostapd_data *hapd_bss;

	bss_name = buf;
	hapd_bss = hostapd_find_bss(hapd->iface, bss_name);
	if (hapd_bss == NULL)
		return -1;

	if (hapd_bss->non_wps_pp_enable)
		os_snprintf(reply, reply_len, "%d", 1);
	else
		os_snprintf(reply, reply_len, "%d", 0);

	return 2;
}

static int hostapd_wps_sta_cancel(struct hostapd_data *hapd,
					    struct sta_info *sta, void *ctx)
{
	if (sta && (sta->flags & WLAN_STA_WPS)) {
		ap_sta_deauthenticate(hapd, sta,
				      WLAN_REASON_PREV_AUTH_NOT_VALID);
		wpa_printf(MSG_DEBUG, "WPS: %s: Deauth sta=" MACSTR "due to user cancel",
				__func__, MAC2STR(sta->addr));

		return 1;
	}

	return 0;
}

static int hostapd_ctrl_iface_wps_cancel(struct hostapd_data *hapd, char *txt)
{
	struct hostapd_data *sel_bss;
	int reg_sel = 0, wps_sta = 0;

	sel_bss = hostapd_find_bss(hapd->iface, txt);
	if (!sel_bss || !sel_bss->wps) {
		return -1;
	}

	reg_sel = wps_registrar_wps_cancel(sel_bss->wps->registrar);

	wps_sta = ap_for_each_sta(sel_bss,
			hostapd_wps_sta_cancel, NULL);

	if (!reg_sel && !wps_sta) {
		wpa_printf(MSG_DEBUG, "No WPS operation in progress at this time");
		return -1;
	}

	/*
	 * There are 2 cases to return wps cancel as success:
	 * 1. When wps cancel was initiated but no connection has been
	 *    established with client yet.
	 * 2. Client is in the middle of exchanging WPS messages.
	 */
	return 0;
}

static void hostapd_ctrl_iface_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	const int reply_size = 4096;
	int reply_len;
	int level = MSG_DEBUG;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
	if (os_strcmp(buf, "PING") == 0)
		level = MSG_EXCESSIVE;
	wpa_hexdump_ascii(level, "RX ctrl_iface", (u8 *) buf, res);

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
		return;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "MIB") == 0) {
		reply_len = ieee802_11_get_mib(hapd, reply, reply_size);
		if (reply_len >= 0) {
			res = wpa_get_mib(hapd->wpa_auth, reply + reply_len,
					  reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = ieee802_1x_get_mib(hapd, reply + reply_len,
						 reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#ifndef CONFIG_NO_RADIUS
		if (reply_len >= 0) {
			res = radius_client_get_mib(hapd->radius,
						    reply + reply_len,
						    reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#endif /* CONFIG_NO_RADIUS */
	} else if (os_strcmp(buf, "STA-FIRST") == 0) {
		reply_len = hostapd_ctrl_iface_sta_first(hapd, reply,
							 reply_size);
	} else if (os_strncmp(buf, "STA ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_sta(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strncmp(buf, "STA-NEXT ", 9) == 0) {
		reply_len = hostapd_ctrl_iface_sta_next(hapd, buf + 9, reply,
							reply_size);
	} else if (os_strcmp(buf, "ATTACH") == 0) {
		if (hostapd_ctrl_iface_attach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (hostapd_ctrl_iface_detach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (os_strncmp(buf, "LEVEL ", 6) == 0) {
		if (hostapd_ctrl_iface_level(hapd, &from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "NEW_STA ", 8) == 0) {
		if (hostapd_ctrl_iface_new_sta(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "DEAUTHENTICATE ", 15) == 0) {
		if (hostapd_ctrl_iface_deauthenticate(hapd, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISASSOCIATE ", 13) == 0) {
		if (hostapd_ctrl_iface_disassociate(hapd, buf + 13))
			reply_len = -1;
#ifdef CONFIG_IEEE80211W
#ifdef NEED_AP_MLME
	} else if (os_strncmp(buf, "SA_QUERY ", 9) == 0) {
		if (hostapd_ctrl_iface_sa_query(hapd, buf + 9))
			reply_len = -1;
#endif /* NEED_AP_MLME */
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WPS
	} else if (os_strncmp(buf, "WPS_PIN ", 8) == 0) {
		if (hostapd_ctrl_iface_wps_pin(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_PIN_BSS ", 12) == 0) {
		if (hostapd_ctrl_iface_wps_pin_bss(hapd, buf + 12))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_CHECK_PIN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_check_pin(
			hapd, buf + 14, reply, reply_size);
	} else if (os_memcmp(buf, "WPS_PBC", 7) == 0) {
		int ret;

		if (strlen(buf) <= 8) {
			ret = hostapd_wps_button_pushed(hapd, NULL);
		} else {
			ret = hostapd_wps_button_pushed_interface(hapd, buf + 8);
		}

		if (ret == -2) {
			reply_len = os_snprintf(reply, reply_size, "WPS overlap");
			reply_len++;
		} else if (ret != 0) {
			reply_len = -1;
		}
	} else if (os_strcmp(buf, "WPS_STATUS") == 0) {
		reply_len = hostapd_ctrl_iface_wps_get_status(hapd, reply, reply_size);
	} else if (os_strncmp(buf, "WPS_STATUS ", 11) == 0) {
		struct hostapd_data *sel_bss;

		sel_bss = hostapd_find_bss(hapd->iface, buf + strlen("WPS_STATUS "));
		if (sel_bss == NULL)
			reply_len = -1;
		else
			reply_len = hostapd_ctrl_iface_wps_get_status(sel_bss,
							reply, reply_size);

#ifdef CONFIG_WPS_OOB
	} else if (os_strncmp(buf, "WPS_OOB ", 8) == 0) {
		if (hostapd_ctrl_iface_wps_oob(hapd, buf + 8))
			reply_len = -1;
#endif /* CONFIG_WPS_OOB */
	} else if (os_strncmp(buf, "WPS_AP_PIN_BSS ", 15) == 0) {
		char *bss_name = buf + 15;
		char *param;
		struct hostapd_data *sel_bss;

		param = os_strchr(bss_name, ' ');
		if (param == NULL) {
			reply_len = -1;
		} else {
			*param++ = '\0';
			sel_bss = hostapd_find_bss(hapd->iface, bss_name);
			if (sel_bss == NULL) {
				reply_len = -1;
			} else {
				reply_len = hostapd_ctrl_iface_wps_ap_pin(sel_bss,
						param, reply, reply_size);
			}
		}
	} else if (os_strncmp(buf, "WPS_AP_PIN ", 11) == 0) {
		reply_len = hostapd_ctrl_iface_wps_ap_pin(hapd, buf + 11,
							  reply, reply_size);
	} else if (os_strncmp(buf, "WPS_CONFIG ", 11) == 0) {
		if (hostapd_ctrl_iface_wps_config(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "WPS_CONFIGURED_STATE") == 0) {
		reply_len = hostapd_ctrl_iface_get_wps_configured_state(hapd,
							reply, reply_size);
#endif /* CONFIG_WPS */
	} else if (os_strncmp(buf, "WPS_CONFIGURED_STATE ", strlen("WPS_CONFIGURED_STATE ")) == 0) {
		struct hostapd_data *sel_bss;

		sel_bss = hostapd_find_bss(hapd->iface, buf + strlen("WPS_CONFIGURED_STATE "));
		if (sel_bss == NULL)
			reply_len = -1;
		else
			reply_len = hostapd_ctrl_iface_get_wps_configured_state(sel_bss,
							reply, reply_size);
	} else if (os_strcmp(buf, "GET_CONFIG") == 0) {
		reply_len = hostapd_ctrl_iface_get_config(hapd, reply,
							  reply_size);
	} else if (os_strncmp(buf, "SET ", 4) == 0) {
		if (hostapd_ctrl_iface_set(hapd, buf + 4))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_get(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strncmp(buf, "BSS_SET ", 8) == 0) {
		if (hostapd_ctrl_iface_bss_set(hapd, buf + 8) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "BSS_GET ", 8) == 0) {
		reply_len = hostapd_ctrl_iface_bss_get(hapd, buf + 8, reply,
						reply_size);
	} else if (os_strcmp(buf, "RECONFIGURE") == 0) {
		if (hostapd_reload_config(hapd->iface))
			reply_len = -1;
	} else if (os_strncmp(buf, "UPDATE_BSSCONFIG ", 17) == 0) {
		if (hostapd_update_bss_config(hapd->iface, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "CREATE_BSSCONFIG ", 17) == 0) {
		if (hostapd_create_bss_config(hapd->iface, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "REMOVE_BSSCONFIG ", 17) == 0) {
		if (hostapd_remove_bss_config(hapd->iface, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "STATUS ", strlen("STATUS ")) == 0) {
		reply_len = hostapd_ctrl_iface_wpa_status(hapd, buf + strlen("STATUS "), reply,
								reply_size);
	} else if (os_strncmp(buf, "WPS_TIMEOUT ", strlen("WPS_TIMEOUT ")) == 0) {
		reply_len = hostapd_ctrl_iface_wps_timeout(hapd, buf + strlen("WPS_TIMEOUT "),
								reply, reply_size);
	} else if (os_strncmp(buf, "GET_PSK_AUTH_FAILURE ",
		strlen("GET_PSK_AUTH_FAILURE ")) == 0) {
		reply_len = hostapd_ctrl_iface_get_psk_auth_failure(hapd,
				buf + strlen("GET_PSK_AUTH_FAILURE "), reply, reply_size);
	} else if (os_strncmp(buf, "WPS_PBC_IN_SRCM ", strlen("WPS_PBC_IN_SRCM ")) == 0) {
		if (hostapd_ctrl_iface_pbc_in_srcm(hapd, buf + strlen("WPS_PBC_IN_SRCM ")) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_PBC_IN_SRCM", strlen("GET_PBC_IN_SRCM")) == 0) {
		reply_len = os_snprintf(reply, reply_size, "%d", hapd->iconf->pbc_in_srcm);
		if (reply_len < 0 || (unsigned int) reply_len >= reply_size)
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_UPNP_ENABLE ", strlen("WPS_UPNP_ENABLE ")) == 0) {
		reply_len = hostapd_ctrl_iface_wps_upnp_enable(hapd, buf + strlen("WPS_UPNP_ENABLE "),
								reply, reply_size);
	} else if (os_strncmp(buf, "WPS_UPNP_STATUS", strlen("WPS_UPNP_STATUS")) == 0) {
		reply_len = hostapd_ctrl_iface_wps_upnp_status(hapd, reply, reply_size);
	} else if (os_strncmp(buf, "NON_WPS_PP_ENABLE ", strlen("NON_WPS_PP_ENABLE ")) == 0) {
		if (hostapd_ctrl_iface_non_wps_pp_enable(hapd, buf + strlen("NON_WPS_PP_ENABLE ")) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NON_WPS_PP_STATUS ", strlen("NON_WPS_PP_STATUS ")) == 0) {
		reply_len = hostapd_ctrl_iface_non_wps_pp_status(hapd, buf + strlen("NON_WPS_PP_STATUS "), reply, reply_size);
	} else if (os_strncmp(buf, "WPS_CANCEL ", strlen("WPS_CANCEL ")) == 0) {
		if (hostapd_ctrl_iface_wps_cancel(hapd, buf + strlen("WPS_CANCEL ")) < 0)
			reply_len = -1;
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}
	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
	os_free(reply);
}


static char * hostapd_ctrl_iface_path(struct hostapd_data *hapd)
{
	char *buf;
	size_t len;

	if (hapd->conf->ctrl_interface == NULL)
		return NULL;

	len = os_strlen(hapd->conf->ctrl_interface) +
		os_strlen(hapd->conf->iface) + 2;
	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s",
		    hapd->conf->ctrl_interface, hapd->conf->iface);
	buf[len - 1] = '\0';
	return buf;
}


static void hostapd_ctrl_iface_msg_cb(void *ctx, int level,
				      const char *txt, size_t len)
{
	struct hostapd_data *hapd = ctx;
	if (hapd == NULL)
		return;
	hostapd_ctrl_iface_send(hapd, level, txt, len);
}


int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	hapd->ctrl_sock = -1;

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	if (!hapd->primary_interface)
		return 0;

	if (mkdir(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    chown(hapd->conf->ctrl_interface, 0,
		  hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface]");
		return -1;
	}

	if (os_strlen(hapd->conf->ctrl_interface) + 1 +
	    os_strlen(hapd->conf->iface) >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_ctrl_iface_path(hapd);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   fname);
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				perror("bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    chown(fname, 0, hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	os_free(fname);

	hapd->ctrl_sock = s;
	eloop_register_read_sock(s, hostapd_ctrl_iface_receive, hapd,
				 NULL);
	hapd->msg_ctx = hapd;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
}


void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (!hapd->primary_interface)
		return;

	if (hapd->ctrl_sock > -1) {
		char *fname;
		eloop_unregister_read_sock(hapd->ctrl_sock);
		close(hapd->ctrl_sock);
		hapd->ctrl_sock = -1;
		fname = hostapd_ctrl_iface_path(hapd);
		if (fname)
			unlink(fname);
		os_free(fname);

		if (hapd->conf->ctrl_interface &&
		    rmdir(hapd->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				perror("rmdir[ctrl_interface]");
			}
		}
	}

	dst = hapd->ctrl_dst;
	while (dst) {
		prev = dst;
		dst = dst->next;
		os_free(prev);
	}
}


static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    const char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	struct msghdr msg;
	int idx;
	struct iovec io[2];
	char levelstr[10];

	dst = hapd->ctrl_dst;
	if (hapd->ctrl_sock < 0 || dst == NULL)
		return;

	os_snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	io[0].iov_base = levelstr;
	io[0].iov_len = os_strlen(levelstr);
	io[1].iov_base = (char *) buf;
	io[1].iov_len = len;
	os_memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = 2;

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor send",
				    (u8 *) dst->addr.sun_path, dst->addrlen -
				    offsetof(struct sockaddr_un, sun_path));
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(hapd->ctrl_sock, &msg, 0) < 0) {
				int _errno = errno;
				wpa_printf(MSG_INFO, "CTRL_IFACE monitor[%d]: "
					   "%d - %s",
					   idx, errno, strerror(errno));
				dst->errors++;
				if (dst->errors > 10 || _errno == ENOENT) {
					hostapd_ctrl_iface_detach(
						hapd, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
}

#endif /* CONFIG_NATIVE_WINDOWS */

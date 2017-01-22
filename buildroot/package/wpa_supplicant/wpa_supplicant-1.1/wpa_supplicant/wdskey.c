/*
 * WDS peer encryption key set tool
 *
 * Largely based on:
 * WPA Supplicant - ASCII passphrase to WPA PSK tool
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
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

#include "includes.h"

#include "common.h"
#include "drivers/driver.h"
#include "sha1.h"
#include <sys/ioctl.h>
#include <string.h>
#include "ieee802_11_defs.h"
#include "wireless_copy.h"
#include <stdio.h>

#undef WME_OUI_TYPE

#include <include/compat.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

#define IEEE80211_IOCTL_POSTEVENT	(SIOCIWFIRSTPRIV+19)
#define MADWIFI_NG

struct wpa_driver_madwifi_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
};

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
set80211priv(struct wpa_driver_madwifi_data *drv, int op, void *data, int len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if (len < IFNAMSIZ &&
	    ((op != IEEE80211_IOCTL_SET_APPIEBUF) && (op != IEEE80211_IOCTL_POSTEVENT))) {
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

	if (ioctl(drv->sock, op, &iwr) < 0) {
		printf("ioctl failed\n");
		return -1;
	}

	printf("set80211 success\n");
	return 0;
}

static int
madwifi_del_key(struct wpa_driver_madwifi_data *drv, int key_idx,
			   const u8 *addr)
{
	struct ieee80211req_del_key wk;

	printf("%s: keyidx=%d", __FUNCTION__, key_idx);
	memset(&wk, 0, sizeof(wk));
	wk.idk_keyix = key_idx;
	if (addr != NULL)
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);

	return set80211priv(drv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk));
}

static int
wpa_driver_madwifi_set_key(void *priv, int alg,
			   const u8 *addr, int key_idx, int set_tx,
			   const u8 *seq, size_t seq_len,
			   const u8 *key, size_t key_len)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_key wk;
	u_int8_t cipher;
	int ret;

	if (alg == WPA_ALG_NONE)
		return madwifi_del_key(drv, key_idx, addr);

	if (alg == WPA_ALG_WEP)
		cipher = IEEE80211_CIPHER_WEP;
	else if (alg == WPA_ALG_TKIP)
		cipher = IEEE80211_CIPHER_TKIP;
	else if (alg == WPA_ALG_CCMP)
		cipher = IEEE80211_CIPHER_AES_CCM;
	else {
		printf("%s: unknown/unsupported algorithm %d\n",
			__func__, alg);
		return -1;
	}

	printf("%s: cipher is %d\n", __func__, cipher);
	if (key_len > sizeof(wk.ik_keydata)) {
		printf("%s: key length %lu too big\n", __func__,
		       (unsigned long) key_len);
		return -3;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT;
	wk.ik_keyix = key_idx;
	if (addr == NULL) {
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
	} else {
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	}
	wk.ik_keylen = key_len;
	memcpy(wk.ik_keydata, key, key_len);

	ret = set80211param(drv, IEEE80211_PARAM_PRIVACY, 1, 1);
	printf("set privacy returned %d\n", ret);

	ret = set80211priv(drv, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk));
	if (ret < 0) {
		printf("%s: Failed to set key"
			   " key_idx %d alg %d key_len %lu set_tx %d)",
			   __func__, key_idx,
			   alg, (unsigned long) key_len, set_tx);
	}

	return ret;
}


static void *wdskey_init(const char *ifname)
{
	struct wpa_driver_madwifi_data *drv;
	drv = calloc(sizeof(*drv), 1);
	if (drv == NULL) {
		printf("alloc failed\n");
		return NULL;
	}

	strlcpy(drv->ifname, ifname, sizeof(drv->ifname));

	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail;

	return drv;

fail:
	printf("failed to create socket\n");
	free(drv);
	return NULL;

}

static void wdskey_exit(void *wpa_drv)
{
	struct wpa_driver_madwifi_data *drv = wpa_drv;
	if (drv == NULL)
		return;
	if (drv->sock >= 0)
		close(drv->sock);
	free(drv);
}

static int parse_mac(char *str, unsigned char *addr)
{
	int len = 0;

	while (*str) {
		int tmp;
		if (str[0] == ':' || str[0] == '.') {
			str++;
			continue;
		}
		if (str[1] == 0)
			return -1;
		if (sscanf(str, "%02x", &tmp) != 1)
			return -1;
		addr[len] = tmp;
		len++;
		str += 2;
	}
	return len;
}

int main(int argc, char *argv[])
{
	unsigned char psk[32];
	int i;
	int ret;
	char *interface, *ssid, *passphrase;
	struct wpa_driver_madwifi_data *drv;
	u8 peeraddr[6];

	if (argc < 4) {
		printf("usage: wdskey <interface> <ssid> <passphrase> <macaddr>\n");
		return 1;
	}

	interface = argv[1];
	ssid = argv[2];
	passphrase = argv[3];

	ret = parse_mac(argv[4], peeraddr);
	if (ret != 6) {
		printf("failed to parse peer mac\n");
		return 1;
	}

	if (strlen(passphrase) < 8 || strlen(passphrase) > 63) {
		printf("Passphrase must be 8..63 characters\n");
		return 1;
	}

	pbkdf2_sha1(passphrase, ssid, strlen(ssid), 4096, psk, 32);

	printf("psk=");
	for (i = 0; i < 32; i++)
		printf("%02x", psk[i]);
	printf("\n");

	drv = wdskey_init(interface);
	if (!drv) {
		printf("interface init for %s failed\n", interface);
		return 1;
	}

	printf("using peer addr %02x:%02x:%02x:%02x:%02x:%02x\n",
		peeraddr[0], peeraddr[1], peeraddr[2],
		peeraddr[3], peeraddr[4], peeraddr[5]);

	ret = wpa_driver_madwifi_set_key(drv, WPA_ALG_CCMP,
			&peeraddr, 0, 1,
			NULL, 0, psk, 32);

	printf("set key returned %d\n", ret);

	wdskey_exit(drv);

	return ret;
}

/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2014 Quantenna Communications, Inc.          **
**                                                                           **
**  File        : qtn_hapd_pp.c                                              **
**  Description :                                                            **
**                                                                           **
*******************************************************************************
**  Copyright 1992-2014 The FreeBSD Project. All rights reserved.            **
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
**  Alternatively, this software may also be distributed under the terms of  **
**  the GNU General Public License ("GPL") version 2, or (at your option)    **
**  any later version as published by the Free Software Foundation.          **
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
EH1*/
#include "utils/includes.h"
#include <net/if.h>
#include <sys/ioctl.h>
#include <net80211/ieee80211_ioctl.h>
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "radius/radius_client.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "ap/authsrv.h"
#include "ap/sta_info.h"
#include "ap/accounting.h"
#include "ap/ap_list.h"
#include "ap/beacon.h"
#include "ap/iapp.h"
#include "ap/ieee802_1x.h"
#include "ap/ieee802_11_auth.h"
#include "ap/vlan_init.h"
#include "ap/wpa_auth.h"
#include "ap/wps_hostapd.h"
#include "ap/hw_features.h"
#include "ap/wpa_auth_glue.h"
#include "ap/ap_drv_ops.h"
#include "ap/ap_config.h"
#include "ap/p2p_hostapd.h"
#include "crypto/md5.h"

static char *wps_extract_first_word(const char *buf, char delimeter, char *word, int len)
{
	char *next, *ret;
	char delim;
	int i;

	if (buf == NULL) {
		return NULL;
	}

	ret = next = NULL;
	next = os_strchr(buf, delimeter);
	if (next == NULL)
		delim = '\0';
	else
		delim = delimeter;
	i = 0;
	while(*buf != delim && i < (len - 1)) {
		word[i++] = *buf++;
	}
	word[i] = '\0';
	ret = (next == NULL ? NULL : next + 1);

	return ret;
}

int wps_verify_device_name(const struct wps_context *wps,
				  const u8 * dev_name,
				  const size_t dev_name_len)
{
	int ret = WPS_PP_NAME_CHK_PASS;
	char extract_devname[MAX_WPS_STA_DEVICE_NAME_LEN];
	char *next;

	if (wps == NULL)
		return WPS_PP_NAME_CHK_REJECT;

	if (dev_name == NULL || dev_name_len == 0)
		return WPS_PP_NAME_CHK_PASS;

	/* while-list has higher priority than black-list */
	next = ((wps->dev.pp_device_name != NULL) ? (wps->dev.pp_device_name) : (wps->dev.pp_device_name_bl));

	if (wps->dev.pp_enable && next != NULL) {
		ret = ((next == wps->dev.pp_device_name) ? (WPS_PP_NAME_CHK_REJECT) : (WPS_PP_NAME_CHK_PASS));
		do {
			next = wps_extract_first_word(next, ',', extract_devname, sizeof(extract_devname));
			if (os_strlen(extract_devname) == dev_name_len &&
					strncmp((char *)dev_name, extract_devname, dev_name_len) == 0) {
				ret = !ret;
				break;
			}
		} while (next != NULL);
	}

	if (ret == WPS_PP_NAME_CHK_REJECT) {
		if (wps->dev.pp_device_name != NULL) {
			wpa_printf(MSG_WARNING,
				"WPS: Pair Protection check reject, device name is excluded from white list: \"%s\"",
				wps->dev.pp_device_name);
		} else {
			wpa_printf(MSG_WARNING,
				"WPS: Pair Protection check reject, device name is include within black list: \"%s\"",
				wps->dev.pp_device_name_bl);
		}
		wpa_hexdump_ascii(MSG_WARNING,
			"WPS: Pair Protection received dev_name",
			dev_name, dev_name_len);
	}

	return ret;
}

int hostapd_setup_pairing_hash(const char *pairing_id, const u8 *own_addr, u8 *pairing_hash)
{
	int ret = 0;
	const u8 *addr[2];
	size_t len[2];

	addr[0] = pairing_id;
	len[0] = os_strlen(pairing_id) + 1;
	addr[1] = own_addr;
	len[1] = ETH_ALEN;

	/*
	 * Pairing hash = SHA256(Pairing_id || peering_MAC)
	 */
	sha256_vector(2, addr, len, pairing_hash);

	wpa_printf(MSG_DEBUG, "Pairing ID- %s, own_mac - "MACSTR,
				pairing_id, MAC2STR(own_addr));
	wpa_hexdump(MSG_DEBUG, "Pairing-hash:", pairing_hash, PAIRING_HASH_LEN);

	return ret;
}

int hostapd_drv_set_pairing_hash_ie(struct hostapd_data *hapd,
				const u8 *hash_ie, size_t ies_len)
{
	if (hapd->driver == NULL || hapd->driver->set_pairing_hash_ie == NULL)
		return 0;
	return hapd->driver->set_pairing_hash_ie(hapd->drv_priv, hash_ie, ies_len);
}

int qtn_hapd_pp2_setup(struct hostapd_data *hapd)
{
	if (hapd->conf && hapd->conf->pairing_id &&
			hostapd_setup_pairing_hash(hapd->conf->pairing_id,
					hapd->own_addr, hapd->conf->pairing_hash) < 0) {
		wpa_printf(MSG_ERROR, "Pairing hash initializing failed");
		return -1;
	}

	if (hapd->conf->pairing_id && hapd->conf->pairing_enable == PPS2_MODE_ACCEPT) {
		if (hostapd_drv_set_pairing_hash_ie(hapd,
				hapd->conf->pairing_hash, PAIRING_HASH_LEN) < 0) {
			wpa_printf(MSG_ERROR, "Setting pairing hash IE failed");
			return -1;
		}
	} else {
		if (hostapd_drv_set_pairing_hash_ie(hapd,
						hapd->conf->pairing_hash, 0) < 0) {
			wpa_printf(MSG_ERROR, "Clearing pairing hash IE failed");
			return -1;
		}
	}

	return 0;
}

int qtn_hapd_pairingie_handle(void *bss, struct hostapd_data *hapd, u8 *addr, struct ieee80211req_wpaie *ie)
{
	u8 *pairing_iebuf = NULL;
	u8 peering_hash[PAIRING_HASH_LEN];
	u8 peering_ie_exist;

	if (hapd->conf->pairing_id && hapd->conf->pairing_enable == PPS2_MODE_DENY) {
		pairing_iebuf = ie->qtn_pairing_ie;
		peering_ie_exist = ie->has_pairing_ie;

		if (peering_ie_exist) {
			hostapd_setup_pairing_hash(hapd->conf->pairing_id, addr, peering_hash);

			if (memcmp(pairing_iebuf, peering_hash, PAIRING_HASH_LEN) == 0) {
				hostapd_notif_disassoc(hapd, addr);
				wpa_printf(MSG_DEBUG, "%s: PPS2, STA denied", __func__);

				return -1;
			}
		}

	} else if (hapd->conf->pairing_id && hapd->conf->pairing_enable == PPS2_MODE_ACCEPT) {
		pairing_iebuf = ie->qtn_pairing_ie;
		peering_ie_exist = ie->has_pairing_ie;

		if (peering_ie_exist)
			hostapd_setup_pairing_hash(hapd->conf->pairing_id, addr, peering_hash);

		if (!peering_ie_exist || memcmp(pairing_iebuf, peering_hash, PAIRING_HASH_LEN) != 0) {
			hostapd_notif_disassoc(hapd, addr);
			wpa_printf(MSG_DEBUG, "%s: Pairing Hash IE checking - not matched", __func__);

			return -1;
		}

		wpa_printf(MSG_DEBUG, "%s: pairing IE checking pass", __func__);
	}

	return 0;
}

static int md5_sum(char *passphrase, int len, char *res_buf)
{
        int i;
        char *psk_use;
        unsigned char hex_buf[MD5_MAC_LEN];
        u8 *passphrase_vec[2];
        size_t     len_vec[2];
        char buf_use[MD5_STR_BUF_LEN] = {0};
        char *pos, *end;
        int ret;

        psk_use = (char *)os_malloc(len+1);
        if (!psk_use) {
                wpa_printf(MSG_DEBUG, "md5_sum: psk_use malloc fail");
                return -1;
        }

        memcpy(psk_use, passphrase, len);
        psk_use[len] = '\n';
        passphrase_vec[0] = (u8 *)psk_use;
        len_vec[0] = len + 1;

        if(md5_vector(1, passphrase_vec, len_vec, hex_buf) < 0) {
                wpa_printf(MSG_DEBUG, "md5_sum: md5_vector fail");
                os_free(psk_use);
                return -1;
        }

        pos = buf_use;
        end = pos + sizeof(buf_use);
        for(i=0; i<MD5_MAC_LEN; i++) {
                ret = os_snprintf(pos, (end - pos), "%02x", hex_buf[i]);
                if (ret < 0 || ret > (end - pos)) {
                        wpa_printf(MSG_DEBUG, "md5_sum: hex to str error");
                        os_free(psk_use);
                        return -1;
                }
                pos += ret;
        }
        memcpy(res_buf, buf_use, (MD5_STR_BUF_LEN - 1));
        os_free(psk_use);

        return 0;
}

static int local_md5_convert_passphrase(const string_64 psk_web, string_64 pre_shared_key)
{
        int key_size;
        char passphrase_md5_res[MD5_STR_BUF_LEN] = {0};

        if (!psk_web || !pre_shared_key)
                return -1;

        key_size = os_strlen(psk_web);
        if (key_size < 8 || key_size > 64)
                return -1;

        memset(pre_shared_key, 0, sizeof(string_64));
        if (md5_sum(psk_web, key_size, passphrase_md5_res) < 0) {
                wpa_printf(MSG_DEBUG, "%s: md5_sum fail", __func__);
                return -1;
        }
        wpa_printf(MSG_DEBUG, "passphrase after md5 function is %s", passphrase_md5_res);

        if (key_size <= (MD5_STR_BUF_LEN - 1)) {
                memcpy(pre_shared_key, passphrase_md5_res, key_size);
        } else {
                memcpy(pre_shared_key, passphrase_md5_res, (MD5_STR_BUF_LEN - 1));
                strncpy(pre_shared_key + (MD5_STR_BUF_LEN - 1), psk_web + (MD5_STR_BUF_LEN - 1),
                                                key_size - (MD5_STR_BUF_LEN - 1));
        }

        return 0;
}

int qtn_hapd_cfg_wpa_passphrase(void *bss, const char *pos, int errors, int line)
{
	struct hostapd_bss_config *pbss = bss;
	int len = os_strlen(pos);
	string_64 wpa_passphrase_md5 = {0};
	if (len < 8 || len > 63) {
		wpa_printf(MSG_ERROR, "Line %d: invalid WPA "
                   "passphrase length %d (expected "
                   "8..63)", line, len);
		errors++;
	} else {
		pbss->ssid.wpa_passphrase = os_strdup(pos);
		memset(wpa_passphrase_md5, 0, sizeof(wpa_passphrase_md5));
		local_md5_convert_passphrase(pbss->ssid.wpa_passphrase, wpa_passphrase_md5);
		pbss->ssid.wpa_passphrase_md5 = os_strdup(wpa_passphrase_md5);
                if (pbss->ssid.wpa_passphrase || pbss->ssid.wpa_passphrase_md5) {
                        os_free(pbss->ssid.wpa_psk);
                        pbss->ssid.wpa_psk = NULL;
                        pbss->ssid.wpa_passphrase_set = 1;
                }
	}

	return errors;
}

int qtn_hapd_cfg_wpa_psk (void *bss, const char *pos, int errors, int line)
{
	struct hostapd_bss_config *pbss = bss;
        string_64 wpa_psk_md5 = {0};
        char temp[65] = {0};
        os_free(pbss->ssid.wpa_psk);
        pbss->ssid.wpa_psk =
                os_zalloc(sizeof(struct hostapd_wpa_psk));

        strncpy(temp, pos, 64);
        local_md5_convert_passphrase(temp, wpa_psk_md5);

        if (pbss->ssid.wpa_psk == NULL)
                errors++;
        else if (hexstr2bin(pos, pbss->ssid.wpa_psk->psk, PMK_LEN) ||
                        hexstr2bin(wpa_psk_md5, pbss->ssid.wpa_psk->psk_md5, PMK_LEN) ||
                        pos[PMK_LEN * 2] != '\0') {
                wpa_printf(MSG_ERROR, "Line %d: Invalid PSK "
                           "'%s'.", line, pos);
                errors++;
        } else {
                pbss->ssid.wpa_psk->group = 1;
		os_free(pbss->ssid.wpa_passphrase);
		pbss->ssid.wpa_passphrase = NULL;
		pbss->ssid.wpa_psk_set = 1;
        }

	return errors;
}

struct hostapd_data *qtn_hostapd_find_bss(void *iface, const char *bss_name)
{
	int i;
	struct hostapd_iface *piface = iface;

	if ((piface == NULL) || (bss_name == NULL))
		return NULL;

	for (i = 0; i < piface->num_bss; i++) {
		if (strncmp(piface->bss[i]->conf->iface, bss_name, IFNAMSIZ) == 0)
			return piface->bss[i];
	}

	return NULL;
}

int qtn_non_wps_pp_enable(struct hostapd_data *hapd, char *buf)
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

        hapd_bss = qtn_hostapd_find_bss(hapd->iface, bss_name);
        if (hapd_bss == NULL)
	       return -1;

        enable = atoi(enable_str);
        if (enable == 0)
                hapd_bss->non_wps_pp_enable = 0;
        else
                hapd_bss->non_wps_pp_enable = 1;

	return hostapd_update_bss(hapd->iface, bss_name);
}

int qtn_non_wps_pp_status(struct hostapd_data *hapd, char *buf,
                char *reply, int reply_len)
{
        char *bss_name;
        struct hostapd_data *hapd_bss;

        bss_name = buf;
        hapd_bss = qtn_hostapd_find_bss(hapd->iface, bss_name);
        if (hapd_bss == NULL)
                return -1;

        if (hapd_bss->non_wps_pp_enable)
                os_snprintf(reply, reply_len, "%d", 1);
        else
                os_snprintf(reply, reply_len, "%d", 0);

        return 2;
}

const u8 *hostapd_get_psk_md5(const void *bss,
                        const u8 *addr, const u8 *p2p_dev_addr,
			const u8 *prev_psk_md5)
{
	struct hostapd_bss_config *pbss = bss;
	struct hostapd_wpa_psk *psk;
        int next_ok = prev_psk_md5 == NULL;

        if (p2p_dev_addr) {
                wpa_printf(MSG_DEBUG, "Searching a PSK for " MACSTR
                           " p2p_dev_addr=" MACSTR " prev_psk_md5=%p",
                           MAC2STR(addr), MAC2STR(p2p_dev_addr), prev_psk_md5);
                if (!is_zero_ether_addr(p2p_dev_addr))
                        addr = NULL; /* Use P2P Device Address for matching */
        } else {
                wpa_printf(MSG_DEBUG, "Searching a PSK for " MACSTR
                           " prev_psk=%p",
                           MAC2STR(addr), prev_psk_md5);
        }

        for (psk = pbss->ssid.wpa_psk; psk != NULL; psk = psk->next) {
                if (next_ok &&
                    (psk->group || os_memcmp(psk->addr, addr, ETH_ALEN) == 0))
                        return psk->psk_md5;

                if (psk->psk_md5 == prev_psk_md5)
                        next_ok = 1;
        }

        return NULL;
}


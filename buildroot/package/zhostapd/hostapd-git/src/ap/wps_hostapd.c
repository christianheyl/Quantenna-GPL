/*
 * hostapd / WPS integration
 * Copyright (c) 2008-2010, Jouni Malinen <j@w1.fi>
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

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/uuid.h"
#include "crypto/dh_groups.h"
#include "common/wpa_ctrl.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "eapol_auth/eapol_auth_sm_i.h"
#include "wps/wps.h"
#include "wps/wps_defs.h"
#include "wps/wps_dev_attr.h"
#include "hostapd.h"
#include "ap_config.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "sta_info.h"
#include "wps_hostapd.h"
#include "drivers/driver.h"


#ifdef CONFIG_WPS_UPNP
#include "wps/wps_upnp.h"
int hostapd_wps_upnp_init(struct hostapd_data *hapd,
				 struct wps_context *wps);
void hostapd_wps_upnp_deinit(struct hostapd_data *hapd);
#endif /* CONFIG_WPS_UPNP */

static int hostapd_wps_probe_req_rx(void *ctx, const u8 *addr, const u8 *da,
				    const u8 *bssid,
				    const u8 *ie, size_t ie_len);
static void hostapd_wps_ap_pin_timeout(void *eloop_data, void *user_ctx);


struct wps_for_each_data {
	int (*func)(struct hostapd_data *h, void *ctx);
	void *ctx;
};


static int wps_for_each(struct hostapd_iface *iface, void *ctx)
{
	struct wps_for_each_data *data = ctx;
	size_t j;

	if (iface == NULL)
		return 0;
	for (j = 0; j < iface->num_bss; j++) {
		struct hostapd_data *hapd = iface->bss[j];
		int ret = data->func(hapd, data->ctx);
		if (ret)
			return ret;
	}

	return 0;
}


static int hostapd_wps_for_each(struct hostapd_data *hapd,
				int (*func)(struct hostapd_data *h, void *ctx),
				void *ctx)
{
	struct hostapd_iface *iface = hapd->iface;
	struct wps_for_each_data data;
	data.func = func;
	data.ctx = ctx;
	if (iface->for_each_interface == NULL)
		return wps_for_each(iface, &data);
	return iface->for_each_interface(iface->interfaces, wps_for_each,
					 &data);
}


static const char *wps_ext_state_to_str(enum wps_external_state external_state)
{
	static const struct {
		enum wps_external_state wps_external_state;
		char *state_as_str;
	} state_to_str[] = {
		{ WPS_EXT_STATE_INI, "WPS_INITIAL" },
		{ WPS_EXT_STATE_PROCESS_START, "WPS_START" },
		{ WPS_EXT_STATE_PROCESS_SUCCESS, "WPS_SUCCESS" },
		{ WPS_EXT_STATE_MSG_EX_ERROR, "WPS_ERROR" },
		{ WPS_EXT_STATE_TIMEOUT_ERROR, "WPS_TIMEOUT" },
		{ WPS_EXT_STATE_MSG_EX_OVERLAP, "WPS_OVERLAP" },
		{ WPS_EXT_STATE_EAP_M2_SEND, "WPS_M2_SEND" },
		{ WPS_EXT_STATE_EAP_M8_SEND, "WPS_M8_SEND" },
		{ WPS_EXT_STATE_STA_CANCEL, "WPS_STA_CANCEL" },
		{ WPS_EXT_STATE_STA_PIN_FAIL, "WPS_STA_PIN_ERR" },
		{ WPS_EXT_STATE_AP_PIN_SUCCESS, "WPS_AP_PIN_SUC"},
		{ WPS_EXT_STATE_AP_PIN_FAIL, "WPS_AP_PIN_ERR" },
		{ WPS_EXT_STATE_PIN_TIMEOUT, "WPS_PIN_TIMEOUT"},
		{ WPS_EXT_STATE_UNKNOWN, NULL }
	};

	int		 i;
	const char	*retaddr = NULL;

	for (i = 0; state_to_str[i].wps_external_state !=
			WPS_EXT_STATE_UNKNOWN && retaddr == NULL; i++) {
		if (external_state == state_to_str[i].wps_external_state) {
			retaddr = state_to_str[i].state_as_str;
		}
	}

	return retaddr;
}

static int hostapd_wps_new_psk_cb(void *ctx, const u8 *mac_addr, const u8 *psk,
				  size_t psk_len)
{
	struct hostapd_data *hapd = ctx;
	struct hostapd_wpa_psk *p;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	wpa_printf(MSG_DEBUG, "Received new WPA/WPA2-PSK from WPS for STA "
		   MACSTR, MAC2STR(mac_addr));
	wpa_hexdump_key(MSG_DEBUG, "Per-device PSK", psk, psk_len);

	if (psk_len != PMK_LEN) {
		wpa_printf(MSG_DEBUG, "Unexpected PSK length %lu",
			   (unsigned long) psk_len);
		return -1;
	}

	/* Add the new PSK to runtime PSK list */
	p = os_zalloc(sizeof(*p));
	if (p == NULL)
		return -1;
	os_memcpy(p->addr, mac_addr, ETH_ALEN);
	os_memcpy(p->psk, psk, PMK_LEN);

	p->next = ssid->wpa_psk;
	ssid->wpa_psk = p;

	if (ssid->wpa_psk_file) {
		FILE *f;
		char hex[PMK_LEN * 2 + 1];
		/* Add the new PSK to PSK list file */
		f = fopen(ssid->wpa_psk_file, "a");
		if (f == NULL) {
			wpa_printf(MSG_DEBUG, "Failed to add the PSK to "
				   "'%s'", ssid->wpa_psk_file);
			return -1;
		}

		wpa_snprintf_hex(hex, sizeof(hex), psk, psk_len);
		fprintf(f, MACSTR " %s\n", MAC2STR(mac_addr), hex);
		fclose(f);
	}

	return 0;
}


static int hostapd_wps_set_ie_cb(void *ctx, struct wpabuf *beacon_ie,
				 struct wpabuf *probe_resp_ie)
{
	struct hostapd_data *hapd = ctx;
	wpabuf_free(hapd->wps_beacon_ie);
	hapd->wps_beacon_ie = beacon_ie;
	wpabuf_free(hapd->wps_probe_resp_ie);
	hapd->wps_probe_resp_ie = probe_resp_ie;
	if (hapd->beacon_set_done)
		ieee802_11_set_beacon(hapd);
	return hostapd_set_ap_wps_ie(hapd);
}


static void hostapd_wps_pin_needed_cb(void *ctx, const u8 *uuid_e,
				      const struct wps_device_data *dev)
{
	struct hostapd_data *hapd = ctx;
	char uuid[40], txt[400];
	int len;
	char devtype[WPS_DEV_TYPE_BUFSIZE];
	if (uuid_bin2str(uuid_e, uuid, sizeof(uuid)))
		return;
	wpa_printf(MSG_DEBUG, "WPS: PIN needed for E-UUID %s", uuid);
	len = os_snprintf(txt, sizeof(txt), WPS_EVENT_PIN_NEEDED
			  "%s " MACSTR " [%s|%s|%s|%s|%s|%s]",
			  uuid, MAC2STR(dev->mac_addr), dev->device_name,
			  dev->manufacturer, dev->model_name,
			  dev->model_number, dev->serial_number,
			  wps_dev_type_bin2str(dev->pri_dev_type, devtype,
					       sizeof(devtype)));
	if (len > 0 && len < (int) sizeof(txt))
		wpa_msg(hapd->msg_ctx, MSG_INFO, "%s", txt);

	if (hapd->conf->wps_pin_requests) {
		FILE *f;
		struct os_time t;
		f = fopen(hapd->conf->wps_pin_requests, "a");
		if (f == NULL)
			return;
		os_get_time(&t);
		fprintf(f, "%ld\t%s\t" MACSTR "\t%s\t%s\t%s\t%s\t%s"
			"\t%s\n",
			t.sec, uuid, MAC2STR(dev->mac_addr), dev->device_name,
			dev->manufacturer, dev->model_name, dev->model_number,
			dev->serial_number,
			wps_dev_type_bin2str(dev->pri_dev_type, devtype,
					     sizeof(devtype)));
		fclose(f);
	}
}


static void hostapd_wps_reg_success_cb(void *ctx, const u8 *mac_addr,
				       const u8 *uuid_e)
{
	struct hostapd_data *hapd = ctx;
	char uuid[40];
	if (uuid_bin2str(uuid_e, uuid, sizeof(uuid)))
		return;
	wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_REG_SUCCESS MACSTR " %s",
		MAC2STR(mac_addr), uuid);
	if (hapd->wps_reg_success_cb)
		hapd->wps_reg_success_cb(hapd->wps_reg_success_cb_ctx,
					 mac_addr, uuid_e);
}


static void hostapd_wps_enrollee_seen_cb(void *ctx, const u8 *addr,
					 const u8 *uuid_e,
					 const u8 *pri_dev_type,
					 u16 config_methods,
					 u16 dev_password_id, u8 request_type,
					 const char *dev_name)
{
	struct hostapd_data *hapd = ctx;
	char uuid[40];
	char devtype[WPS_DEV_TYPE_BUFSIZE];
	if (uuid_bin2str(uuid_e, uuid, sizeof(uuid)))
		return;
	if (dev_name == NULL)
		dev_name = "";
	wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ENROLLEE_SEEN MACSTR
		     " %s %s 0x%x %u %u [%s]",
		     MAC2STR(addr), uuid,
		     wps_dev_type_bin2str(pri_dev_type, devtype,
					  sizeof(devtype)),
		     config_methods, dev_password_id, request_type, dev_name);
}

static int hostapd_wps_ssid_set_cb(void *ctx, int value)
{
	struct hostapd_data *hapd = ctx;

	if (value != 0)
		return hostapd_set_broadcast_ssid(hapd, 1);
	else
		return hostapd_set_broadcast_ssid(hapd, 0);
}

static void hostapd_send_wlan_msg(struct hostapd_data *hapd, const char *msg)
{
	if (hapd->driver->send_log) {
		hapd->driver->send_log(hapd->drv_priv, (char *)msg);
	}
}

static void hostapd_wps_reg_event_cb(void *ctx, const char *msg)
{
	struct hostapd_data *hapd = ctx;

	if (msg)
		hostapd_send_wlan_msg(hapd, msg);
}

static int str_starts(const char *str, const char *start)
{
	return os_strncmp(str, start, os_strlen(start)) == 0;
}


static void wps_reload_config(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;

	wpa_printf(MSG_DEBUG, "WPS: Reload configuration data");
	if (iface->reload_config(iface) < 0) {
		wpa_printf(MSG_WARNING, "WPS: Failed to reload the updated "
			   "configuration");
	}
}


static void wps_update_bss_config(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;
	char *bss = user_ctx;

	if (bss == NULL) {
		wpa_printf(MSG_ERROR, "WPS: BSS name NULL"
				"Fail to update configuration");
		return;
	}

	wpa_printf(MSG_DEBUG, "WPS: Update bss %s configuration data", bss);
	if (iface->update_bss_config(iface, bss) < 0) {
		wpa_printf(MSG_WARNING, "WPS: Failed to update"
				"bss configuration");
	}
	os_free(bss);
}


static void hapd_wps_write_new_config(FILE *file, const struct wps_credential *cred)
{
	int wpa;
	int i;

	fprintf(file, "# WPS configuration - START\n");
	fprintf(file, "wps_state=2\n");

	fprintf(file, "ssid=");
	for (i = 0; i < cred->ssid_len; i++)
		fputc(cred->ssid[i], file);
	fprintf(file, "\n");

	if ((cred->auth_type & (WPS_AUTH_WPA2 | WPS_AUTH_WPA2PSK)) &&
	    (cred->auth_type & (WPS_AUTH_WPA | WPS_AUTH_WPAPSK)))
		wpa = 3;
	else if (cred->auth_type & (WPS_AUTH_WPA2 | WPS_AUTH_WPA2PSK))
		wpa = 2;
	else if (cred->auth_type & (WPS_AUTH_WPA | WPS_AUTH_WPAPSK))
		wpa = 1;
	else
		wpa = 0;

	fprintf(file, "wpa=%d\n", wpa);

	if (wpa) {
		char *prefix;

		fprintf(file, "wpa_key_mgmt=");
		prefix = "";
		if (cred->auth_type & (WPS_AUTH_WPA2 | WPS_AUTH_WPA)) {
			fprintf(file, "WPA-EAP");
			prefix = " ";
		}
		if (cred->auth_type & (WPS_AUTH_WPA2PSK | WPS_AUTH_WPAPSK))
			fprintf(file, "%sWPA-PSK", prefix);
		fprintf(file, "\n");

		fprintf(file, "wpa_pairwise=");
		prefix = "";
		if (cred->encr_type & WPS_ENCR_AES) {
			fprintf(file, "CCMP");
			prefix = " ";
		}
		if (cred->encr_type & WPS_ENCR_TKIP) {
			fprintf(file, "%sTKIP", prefix);
		}
		fprintf(file, "\n");

		if (cred->key_len >= 8 && cred->key_len < 64) {
			fprintf(file, "wpa_passphrase=");
			for (i = 0; i < cred->key_len; i++)
				fputc(cred->key[i], file);
			fprintf(file, "\n");
		} else if (cred->key_len == 64) {
			fprintf(file, "wpa_psk=");
			for (i = 0; i < cred->key_len; i++)
				fputc(cred->key[i], file);
			fprintf(file, "\n");
		} else {
			wpa_printf(MSG_WARNING, "WPS: Invalid key length %lu "
				   "for WPA/WPA2",
				   (unsigned long) cred->key_len);
		}

		fprintf(file, "auth_algs=1\n");
	} else {
		if ((cred->auth_type & WPS_AUTH_OPEN) &&
		    (cred->auth_type & WPS_AUTH_SHARED))
			fprintf(file, "auth_algs=3\n");
		else if (cred->auth_type & WPS_AUTH_SHARED)
			fprintf(file, "auth_algs=2\n");
		else
			fprintf(file, "auth_algs=1\n");

		if (cred->encr_type & WPS_ENCR_WEP && cred->key_idx <= 4) {
			int key_idx = cred->key_idx;
			if (key_idx)
				key_idx--;
			fprintf(file, "wep_default_key=%d\n", key_idx);
			fprintf(file, "wep_key%d=", key_idx);
			if (cred->key_len == 10 || cred->key_len == 26) {
				/* WEP key as a hex string */
				for (i = 0; i < cred->key_len; i++)
					fputc(cred->key[i], file);
			} else {
				/* Raw WEP key; convert to hex */
				for (i = 0; i < cred->key_len; i++)
					fprintf(file, "%02x", cred->key[i]);
			}
			fprintf(file, "\n");
		} else {
			fprintf(file, "wpa_key_mgmt=WPA-PSK\n");
			fprintf(file, "wpa_pairwise=CCMP\n");
			fprintf(file, "wpa_passphrase=qtn01234\n");
		}
	}
	fprintf(file, "# WPS configuration - END\n");
}


static enum wps_cred_write_back_state {
	WPS_CRED_SEARCH = 0,
	WPS_CRED_FOUND,
	WPS_CRED_DONE,
};


static int hapd_wps_cred_cb(struct hostapd_data *hapd, void *ctx)
{
	const struct wps_credential *cred = ctx;
	FILE *oconf, *nconf;
	size_t len;
	char *tmp_fname;
	char buf[1024];
	char bss[32];
	static enum wps_cred_write_back_state state;
	char *bss_name;

	if (hapd->wps == NULL)
		return 0;

	wpa_hexdump_key(MSG_DEBUG, "WPS: Received Credential attribute",
			cred->cred_attr, cred->cred_attr_len);

	wpa_printf(MSG_DEBUG, "WPS: Received new AP Settings");
	wpa_hexdump_ascii(MSG_DEBUG, "WPS: SSID", cred->ssid, cred->ssid_len);
	wpa_printf(MSG_DEBUG, "WPS: Authentication Type 0x%x",
		   cred->auth_type);
	wpa_printf(MSG_DEBUG, "WPS: Encryption Type 0x%x", cred->encr_type);
	wpa_printf(MSG_DEBUG, "WPS: Network Key Index %d", cred->key_idx);
	wpa_hexdump_key(MSG_DEBUG, "WPS: Network Key",
			cred->key, cred->key_len);
	wpa_printf(MSG_DEBUG, "WPS: MAC Address " MACSTR,
		   MAC2STR(cred->mac_addr));

	if ((hapd->conf->wps_cred_processing == 1 ||
	     hapd->conf->wps_cred_processing == 2) && cred->cred_attr) {
		size_t blen = cred->cred_attr_len * 2 + 1;
		char *_buf = os_malloc(blen);
		if (_buf) {
			wpa_snprintf_hex(_buf, blen,
					 cred->cred_attr, cred->cred_attr_len);
			wpa_msg(hapd->msg_ctx, MSG_INFO, "%s%s",
				WPS_EVENT_NEW_AP_SETTINGS, _buf);
			os_free(_buf);
		}
	} else
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_NEW_AP_SETTINGS);

	if (hapd->conf->wps_cred_processing == 1)
		return 0;

	os_memcpy(hapd->wps->ssid, cred->ssid, cred->ssid_len);
	hapd->wps->ssid_len = cred->ssid_len;
	hapd->wps->encr_types = cred->encr_type;
	hapd->wps->auth_types = cred->auth_type;
	if (cred->key_len == 0) {
		os_free(hapd->wps->network_key);
		hapd->wps->network_key = NULL;
		hapd->wps->network_key_len = 0;
	} else {
		if (hapd->wps->network_key == NULL ||
		    hapd->wps->network_key_len < cred->key_len) {
			hapd->wps->network_key_len = 0;
			os_free(hapd->wps->network_key);
			hapd->wps->network_key = os_malloc(cred->key_len);
			if (hapd->wps->network_key == NULL)
				return -1;
		}
		hapd->wps->network_key_len = cred->key_len;
		os_memcpy(hapd->wps->network_key, cred->key, cred->key_len);
	}
	hapd->wps->wps_state = WPS_STATE_CONFIGURED;

	len = os_strlen(hapd->iface->config_fname) + 5;
	tmp_fname = os_malloc(len);
	if (tmp_fname == NULL)
		return -1;
	os_snprintf(tmp_fname, len, "%s-new", hapd->iface->config_fname);

	oconf = fopen(hapd->iface->config_fname, "r");
	if (oconf == NULL) {
		wpa_printf(MSG_WARNING, "WPS: Could not open current "
			   "configuration file");
		os_free(tmp_fname);
		return -1;
	}

	nconf = fopen(tmp_fname, "w");
	if (nconf == NULL) {
		wpa_printf(MSG_WARNING, "WPS: Could not write updated "
			   "configuration file");
		os_free(tmp_fname);
		fclose(oconf);
		return -1;
	}

	state = WPS_CRED_SEARCH;
	snprintf(bss, sizeof(bss), "bss=%s", hapd->conf->iface);
	if (hapd->primary_interface) {
		wpa_printf(MSG_DEBUG, "WPS: [cred] found primary interface (%s)",
				hapd->conf->iface);
		hapd_wps_write_new_config(nconf, cred);
		state = WPS_CRED_FOUND;
	}

	while (fgets(buf, sizeof(buf), oconf)) {
		switch(state) {
		case WPS_CRED_SEARCH:
			if (str_starts(buf, bss)) {
				wpa_printf(MSG_DEBUG, "WPS: [cred] found bss--%s",
						buf);
				fprintf(nconf, "%s", buf);
				hapd_wps_write_new_config(nconf, cred);
				state = WPS_CRED_FOUND;
				wpa_printf(MSG_DEBUG, "WPS: [cred] mode switch, search->found");
			} else {
				fprintf(nconf, "%s", buf);
			}
			break;
		case WPS_CRED_FOUND:
			if (str_starts(buf, "bss=")) {
				wpa_printf(MSG_DEBUG, "WPS: [cred] found another bss--%s", buf);
				fprintf(nconf, "%s", buf);
				state = WPS_CRED_DONE;
				wpa_printf(MSG_DEBUG, "WPS: [cred] mode switch, found->done");
			} else if (str_starts(buf, "ssid=") ||
				str_starts(buf, "auth_algs=") ||
				str_starts(buf, "wep_default_key=") ||
				str_starts(buf, "wep_key") ||
				str_starts(buf, "wps_state=") ||
				str_starts(buf, "wpa=") ||
				str_starts(buf, "wpa_psk=") ||
				str_starts(buf, "wpa_pairwise=") ||
				str_starts(buf, "rsn_pairwise=") ||
				str_starts(buf, "wpa_key_mgmt=") ||
				str_starts(buf, "wpa_passphrase=")) {
				/* Discard old config */
				wpa_printf(MSG_DEBUG, "WPS: [cred] skip field--%s", buf);
			} else {
				fprintf(nconf, "%s", buf);
			}
			break;
		case WPS_CRED_DONE:
			fprintf(nconf, "%s", buf);
			break;
		default:
			break;
		}
	}

	fclose(nconf);
	fclose(oconf);

	if (rename(tmp_fname, hapd->iface->config_fname) < 0) {
		wpa_printf(MSG_WARNING, "WPS: Failed to rename the updated "
			   "configuration file: %s", strerror(errno));
		os_free(tmp_fname);
		return -1;
	}

	os_free(tmp_fname);

	/* Schedule configuration reload after short period of time to allow
	 * EAP-WSC to be finished.
	 */
	bss_name = os_strdup(hapd->conf->iface);
	eloop_register_timeout(0, 1000000, wps_update_bss_config, hapd->iface,
			       bss_name);

	wpa_printf(MSG_DEBUG, "WPS: AP configuration updated");

	return 0;
}


static int hostapd_wps_cred_cb(void *ctx, const struct wps_credential *cred)
{
	struct hostapd_data *hapd = ctx;
	return hapd_wps_cred_cb(hapd, (void *)cred);
}


struct wps_ap_pin_fail_method {
	char *name;
	int (*cfg_init)(struct hostapd_data *hapd);
	int (*pwd_auth_fail)(struct hostapd_data *hapd);
	int (*pwd_auth_succ)(struct hostapd_data *hapd);
	int (*reset)(struct hostapd_data *hapd, int keep_data);
};


void wps_update_ap_setup_locked(struct hostapd_data *hapd, int flag)
{
	if(hapd->wps == NULL)
		return;

	flag = !!flag;
	if (flag ^ (!!hapd->wps->ap_setup_locked)) {
		wpa_printf(MSG_DEBUG, "WPS: [ap_setup_locked] updating value from %d to %d",
				hapd->wps->ap_setup_locked, flag);
		wpa_msg(hapd->msg_ctx, MSG_INFO, flag == 1 ?
				WPS_EVENT_AP_SETUP_LOCKED : WPS_EVENT_AP_SETUP_UNLOCKED);
		hapd->wps->ap_setup_locked = flag;
		wps_registrar_update_ie(hapd->wps->registrar);
	}
}


static int wps_ap_pin_fail_default_cfg_init(struct hostapd_data *hapd)
{
	struct ap_pin_fail_dfl_data *data;

	wpa_printf(MSG_DEBUG, "WPS: [default] init data");
	data = &hapd->ap_pin_fail_data.dfl;

	return 0;
}


static void hostapd_wps_reenable_ap_pin(void *eloop_data, void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;
	struct ap_pin_fail_dfl_data *data;

	data = &hapd->ap_pin_fail_data.dfl;
	if (hapd->conf->ap_setup_locked)
		return;
	if (data->consecutive_fail >= 10)
		return;

	wpa_printf(MSG_DEBUG, "WPS: [default] Re-enable AP PIN");
	wps_update_ap_setup_locked(hapd, 0);
}


static int wps_ap_pin_fail_default_fail(struct hostapd_data *hapd)
{
	struct ap_pin_fail_dfl_data *data;

	data = &hapd->ap_pin_fail_data.dfl;
	/*
	 * Registrar failed to prove its knowledge of the AP PIN. Lock AP setup
	 * for some time if this happens multiple times to slow down brute
	 * force attacks.
	 */
	data->accu_fail++;
	data->consecutive_fail++;
	wpa_printf(MSG_DEBUG, "WPS: [default] AP PIN authentication failure number %u "
		   "(%u consecutive)",
		   data->accu_fail, data->consecutive_fail);
	if (data->accu_fail < 3)
		return 0;

	wps_update_ap_setup_locked(hapd, 1);

	if (!hapd->conf->ap_setup_locked &&
	    data->consecutive_fail >= 10) {
		/*
		 * In indefinite lockdown - disable automatic AP PIN
		 * reenablement.
		 */
		eloop_cancel_timeout(hostapd_wps_reenable_ap_pin, hapd, NULL);
		wpa_printf(MSG_ERROR, "WPS:[default] AP PIN disabled indefinitely");
	} else if (!hapd->conf->ap_setup_locked) {
		if (data->lockout_time == 0)
			data->lockout_time = WPS_AP_PIN_LOCKOUT_TIME;
		else if (data->lockout_time < 365 * 24 * 60 * 60)
			data->lockout_time *= 2;

		wpa_printf(MSG_DEBUG, "WPS: [default] Disable AP PIN for %u seconds",
			   data->lockout_time);
		eloop_cancel_timeout(hostapd_wps_reenable_ap_pin, hapd, NULL);
		eloop_register_timeout(data->lockout_time, 0,
				       hostapd_wps_reenable_ap_pin, hapd,
				       NULL);
	}

	return 0;
}


static int wps_ap_pin_fail_default_succ(struct hostapd_data *hapd)
{
	struct ap_pin_fail_dfl_data *data;

	data = &hapd->ap_pin_fail_data.dfl;
	if (data->consecutive_fail == 0)
		return 0;

	wpa_printf(MSG_DEBUG, "WPS: [default] Clear consecutive AP PIN failure counter "
		   "- total validation failures %u (%u consecutive)",
		   data->accu_fail,
		   data->consecutive_fail);
	data->consecutive_fail = 0;

	return 0;
}


static int wps_ap_pin_fail_default_reset(struct hostapd_data *hapd, int keep_data)
{
	struct ap_pin_fail_dfl_data *data;

	data = &hapd->ap_pin_fail_data.dfl;
	eloop_cancel_timeout(hostapd_wps_reenable_ap_pin, hapd, NULL);
	if (!keep_data) {
		data->accu_fail = 0;
		data->consecutive_fail = 0;
	}
	wpa_printf(MSG_DEBUG, "WPS: [default] reset data, keep data = %d", keep_data);

	return 0;
}


static int wps_ap_pin_fail_auto_lockdown_cfg_init(struct hostapd_data *hapd)
{
	struct ap_pin_fail_auto_lockdown_data *data;

	data = &hapd->ap_pin_fail_data.auto_lockdown;
	if (hapd->conf->auto_lockdown_max_retry != data->fail_threshold) {
		data->accu_fail = 0;
		data->fail_threshold = hapd->conf->auto_lockdown_max_retry;
	}
	wpa_printf(MSG_DEBUG, "WPS: [Auto-Lock] init data: max retry(%d)", data->fail_threshold);

	return 0;
}


static int wps_ap_pin_fail_auto_lockdown_fail(struct hostapd_data *hapd)
{
	struct ap_pin_fail_auto_lockdown_data *data;

	data = &hapd->ap_pin_fail_data.auto_lockdown;

	if (data->fail_threshold <= 0 || hapd->wps->ap_setup_locked)
		return 0;

	data->accu_fail++;
	wpa_printf(MSG_DEBUG, "WPS: [Auto-Lock] fail count = %d, threshold = %d",
			data->accu_fail,
			data->fail_threshold);

	if (data->accu_fail < data->fail_threshold)
		return 0;

	wpa_printf(MSG_DEBUG, "WPS: [Auto-Lock] fail count reach threshod, Lock ap set-up");
	wps_update_ap_setup_locked(hapd, 1);

	return 0;
}


static int wps_ap_pin_fail_auto_lockdown_succ(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "WPS: [Auto-Lock] ap pin success");
	return 0;
}


static int wps_ap_pin_fail_auto_lockdown_reset(struct hostapd_data *hapd, int keep_data)
{
	struct ap_pin_fail_auto_lockdown_data *data;

	wpa_printf(MSG_DEBUG, "WPS: [Auto-Lock] reset data");
	data = &hapd->ap_pin_fail_data.auto_lockdown;
	if (!keep_data) {
		data->accu_fail = 0;
	}
	return 0;
}


static struct wps_ap_pin_fail_method ap_pin_fail_methods[] = {
	{
		.name = "default",
		.cfg_init = wps_ap_pin_fail_default_cfg_init,
		.pwd_auth_fail = wps_ap_pin_fail_default_fail,
		.pwd_auth_succ = wps_ap_pin_fail_default_succ,
		.reset = wps_ap_pin_fail_default_reset,
	},
	{
		.name = "auto_lockdown",
		.cfg_init = wps_ap_pin_fail_auto_lockdown_cfg_init,
		.pwd_auth_fail = wps_ap_pin_fail_auto_lockdown_fail,
		.pwd_auth_succ = wps_ap_pin_fail_auto_lockdown_succ,
		.reset = wps_ap_pin_fail_auto_lockdown_reset,
	},
};


struct wps_ap_pin_fail_method *wps_pick_ap_pin_fail_method(const char* name)
{
	struct wps_ap_pin_fail_method *ret;
	int i;

	ret = NULL;
	for (i = 0; i < sizeof(ap_pin_fail_methods)/sizeof(ap_pin_fail_methods[0]); i++) {
		if (strcmp(ap_pin_fail_methods[i].name, name) == 0) {
			ret = &ap_pin_fail_methods[i];
			wpa_printf(MSG_DEBUG, "WPS: find ap pin fail method %s", name);
		}
	}

	return ret;
}


char *hostapd_wps_get_ap_pin_fail_method(struct hostapd_data *hapd)
{
	char *null_str = "NONE";
	struct wps_ap_pin_fail_method *m;

	if (!hapd->ap_pin_fail_m)
		return null_str;

	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;
	return m->name;
}


static int wps_pwd_auth_fail(struct hostapd_data *hapd, void *ctx)
{
	struct wps_event_pwd_auth_fail *data = ctx;
	struct wps_ap_pin_fail_method *m;

	if (!data->enrollee || hapd->conf->ap_pin == NULL || hapd->wps == NULL)
		return 0;

	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;
	if (m != NULL) {
		return m->pwd_auth_fail(hapd);
	}

	return 0;
}


static void hostapd_pwd_auth_fail(struct hostapd_data *hapd,
				  struct wps_event_pwd_auth_fail *data)
{
	wps_pwd_auth_fail(hapd, data);
}


static int wps_ap_pin_success(struct hostapd_data *hapd, void *ctx)
{
	struct wps_ap_pin_fail_method *m;

	if (hapd->conf->ap_pin == NULL || hapd->wps == NULL)
		return 0;

	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;
	if (m != NULL) {
		return m->pwd_auth_succ(hapd);
	}

	return 0;
}


static void hostapd_wps_ap_pin_success(struct hostapd_data *hapd)
{
	wps_ap_pin_success(hapd, NULL);
}


static const char * wps_event_fail_reason[NUM_WPS_EI_VALUES] = {
	"No Error", /* WPS_EI_NO_ERROR */
	"TKIP Only Prohibited", /* WPS_EI_SECURITY_TKIP_ONLY_PROHIBITED */
	"WEP Prohibited" /* WPS_EI_SECURITY_WEP_PROHIBITED */
};

static void hostapd_wps_event_fail(struct hostapd_data *hapd,
				   struct wps_event_fail *fail)
{
	if (fail->error_indication > 0 &&
	    fail->error_indication < NUM_WPS_EI_VALUES) {
		wpa_msg(hapd->msg_ctx, MSG_INFO,
			WPS_EVENT_FAIL "msg=%d config_error=%d reason=%d (%s)",
			fail->msg, fail->config_error, fail->error_indication,
			wps_event_fail_reason[fail->error_indication]);
	} else {
		wpa_msg(hapd->msg_ctx, MSG_INFO,
			WPS_EVENT_FAIL "msg=%d config_error=%d",
			fail->msg, fail->config_error);
	}
}

static void hostapd_wps_event_cb(void *ctx, enum wps_event event,
				 union wps_event_data *data)
{
	struct hostapd_data *hapd = ctx;
	struct wps_context *wps = NULL;
	char *msg = NULL;
	enum wps_external_state new_external_state = WPS_EXT_STATE_UNKNOWN;

	if (hapd != NULL) {
		wps = hapd->wps;
	}

	switch (event) {
	case WPS_EV_FAIL:
		hostapd_wps_event_fail(hapd, &data->fail);
		msg = "WPS failed";
		if (data->fail.config_error == WPS_CFG_MULTIPLE_PBC_DETECTED) {
			new_external_state = WPS_EXT_STATE_MSG_EX_OVERLAP;
			msg = "WPS overlap";
		} else if (data->fail.config_error == WPS_CFG_DEV_PASSWORD_AUTH_FAILURE
				&& !data->fail.enrollee) {
			new_external_state = WPS_EXT_STATE_STA_PIN_FAIL;
			msg = "WPS STA PIN FAIL";
		} else if (data->fail.config_error == WPS_CFG_DEV_PASSWORD_AUTH_FAILURE
				&& data->fail.enrollee) {
				new_external_state = WPS_EXT_STATE_AP_PIN_FAIL;
				msg = "WPS AP PIN FAIL";
		} else {
			new_external_state = WPS_EXT_STATE_MSG_EX_ERROR;
		}
		break;
	case WPS_EV_M2D:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_M2D);
		new_external_state = WPS_EXT_STATE_MSG_EX_ERROR;
		msg = "WPS failed";
		break;
	case WPS_EV_SUCCESS:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_SUCCESS);
		msg = "WPS success";
		new_external_state = WPS_EXT_STATE_PROCESS_SUCCESS;
		break;
	case WPS_EV_PWD_AUTH_FAIL:
		hostapd_pwd_auth_fail(hapd, &data->pwd_auth_fail);
		if (!data->pwd_auth_fail.enrollee) {
			new_external_state = WPS_EXT_STATE_STA_PIN_FAIL;
			msg = "WPS authentication failed";
		} else {
			new_external_state = WPS_EXT_STATE_AP_PIN_FAIL;
			msg = "WPS AP PIN FAIL";
		}
		break;
	case WPS_EV_PBC_START:
		msg = "WPS PBC start";
		new_external_state = WPS_EXT_STATE_PROCESS_START;
		break;
	case WPS_EV_PIN_START:
		msg = "WPS PIN start";
		new_external_state = WPS_EXT_STATE_PROCESS_START;
		break;
	case WPS_EV_EAP_M2_SEND:
		msg = "WPS M2 send";
		new_external_state = WPS_EXT_STATE_EAP_M2_SEND;
		break;
	case WPS_EV_EAP_M8_SEND:
		msg = "WPS M8 send";
		new_external_state = WPS_EXT_STATE_EAP_M8_SEND;
		break;
	case WPS_EV_PBC_OVERLAP:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_OVERLAP);
		msg = "WPS overlap";
		new_external_state = WPS_EXT_STATE_MSG_EX_OVERLAP;
		break;
	case WPS_EV_PBC_TIMEOUT:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_TIMEOUT);
		msg = "WPS PBC timeout";
		new_external_state = WPS_EXT_STATE_TIMEOUT_ERROR;
		break;
	case WPS_EV_ER_AP_ADD:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ER_AP_ADD);
		break;
	case WPS_EV_ER_AP_REMOVE:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ER_AP_REMOVE);
		break;
	case WPS_EV_ER_ENROLLEE_ADD:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ER_ENROLLEE_ADD);
		break;
	case WPS_EV_ER_ENROLLEE_REMOVE:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ER_ENROLLEE_REMOVE);
		break;
	case WPS_EV_ER_AP_SETTINGS:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ER_AP_SETTINGS);
		break;
	case WPS_EV_ER_SET_SELECTED_REGISTRAR:
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_ER_SET_SEL_REG);
		break;
	case WPS_EV_AP_PIN_SUCCESS:
		msg = "WPS AP PIN SUCESS";
		new_external_state = WPS_EXT_STATE_AP_PIN_SUCCESS;
		hostapd_wps_ap_pin_success(hapd);
		break;
	case WPS_EV_STA_CANCEL:
		msg = "WPS STA Cancel";
		new_external_state = WPS_EXT_STATE_STA_CANCEL;
		break;
	case WPS_EV_STA_PIN_FAIL:
		msg = "WPS STA PIN Fail";
		new_external_state = WPS_EXT_STATE_STA_PIN_FAIL;
		break;
	case WPS_EV_INIT:
		new_external_state = WPS_EXT_STATE_INI;
		break;
	case WPS_EV_PIN_TIMEOUT:
		msg = "WPS PIN TIMEOUT";
		new_external_state = WPS_EXT_STATE_PIN_TIMEOUT;
		break;
	default:
		break;
	}
	if (hapd->wps_event_cb)
		hapd->wps_event_cb(hapd->wps_event_cb_ctx, event, data);

	if (wps != NULL && new_external_state != WPS_EXT_STATE_UNKNOWN) {
		wps->wps_external_state = new_external_state;
	}

	if (msg) {
		hostapd_send_wlan_msg(hapd, msg);
	}
}


static void hostapd_wps_clear_ies(struct hostapd_data *hapd)
{
	wpabuf_free(hapd->wps_beacon_ie);
	hapd->wps_beacon_ie = NULL;

	wpabuf_free(hapd->wps_probe_resp_ie);
	hapd->wps_probe_resp_ie = NULL;

	hostapd_set_ap_wps_ie(hapd);
}


static int get_uuid_cb(struct hostapd_iface *iface, void *ctx)
{
	const u8 **uuid = ctx;
	size_t j;

	if (iface == NULL)
		return 0;
	for (j = 0; j < iface->num_bss; j++) {
		struct hostapd_data *hapd = iface->bss[j];
		if (hapd->wps && !is_nil_uuid(hapd->wps->uuid)) {
			*uuid = hapd->wps->uuid;
			return 1;
		}
	}

	return 0;
}


static const u8 * get_own_uuid(struct hostapd_iface *iface)
{
	const u8 *uuid;
	if (iface->for_each_interface == NULL)
		return NULL;
	uuid = NULL;
	iface->for_each_interface(iface->interfaces, get_uuid_cb, &uuid);
	return uuid;
}


static int count_interface_cb(struct hostapd_iface *iface, void *ctx)
{
	int *count= ctx;
	(*count)++;
	return 0;
}


static int interface_count(struct hostapd_iface *iface)
{
	int count = 0;
	if (iface->for_each_interface == NULL)
		return 0;
	iface->for_each_interface(iface->interfaces, count_interface_cb,
				  &count);
	return count;
}


static int hostapd_wps_set_vendor_ext(struct hostapd_data *hapd,
				      struct wps_context *wps)
{
	int i;

	for (i = 0; i < MAX_WPS_VENDOR_EXTENSIONS; i++) {
		wpabuf_free(wps->dev.vendor_ext[i]);
		wps->dev.vendor_ext[i] = NULL;

		if (hapd->conf->wps_vendor_ext[i] == NULL)
			continue;

		wps->dev.vendor_ext[i] =
			wpabuf_dup(hapd->conf->wps_vendor_ext[i]);
		if (wps->dev.vendor_ext[i] == NULL) {
			while (--i >= 0)
				wpabuf_free(wps->dev.vendor_ext[i]);
			return -1;
		}
	}

	return 0;
}


static int hostapd_wps_set_quantenna_ext(struct hostapd_data *hapd,
				      struct wps_context *wps)
{

	wpabuf_free(wps->dev.quantenna_ext);
	wps->dev.quantenna_ext = NULL;

	if (hapd->conf->wps_quantenna_ext == NULL)
		return 0;

	wps->dev.quantenna_ext =
		wpabuf_dup(hapd->conf->wps_quantenna_ext);

	return 0;
}

int hostapd_ctrl_iface_wps_get_status(struct hostapd_data *hapd,
				      char *status_str,
				      const size_t status_len)
{
	int retval = -1;
	const char *equiv_status_str  = NULL;
	char numeric_value[12] = "";
	size_t equiv_status_len = 0;
	size_t numeric_value_len = 0;
	size_t total_status_len = 0;

	if (status_str == NULL || status_len < 1 || hapd == NULL ||
			hapd->wps == NULL)
		return -1;

	equiv_status_str = wps_ext_state_to_str(hapd->wps->wps_external_state);

	if (equiv_status_str == NULL)
		equiv_status_str = "unknown";

	snprintf(&numeric_value[0], sizeof(numeric_value),
		  "%d", (int)(hapd->wps->wps_external_state));
	numeric_value_len = strlen(&numeric_value[0]);

	equiv_status_len = strnlen(equiv_status_str, status_len);

	/*
	 * format is "NN (XXXXXXXX)\n", where NN is the numeric value and
	 * XXXXXXXX is the equivalent string (returned from
	 * wps_ext_state_to_str).  Total length is thus numeric_value_len +
	 * equiv_status_len + 4.
	 */

	total_status_len = equiv_status_len + numeric_value_len + 4;

	if (equiv_status_len + numeric_value_len + 4 < status_len) {
		snprintf(status_str, status_len, "%s (%s)\n",
				&numeric_value[0], equiv_status_str);
		retval = (int)total_status_len;
	}

	return retval;
}

int hostapd_init_wps(struct hostapd_data *hapd,
		     struct hostapd_bss_config *conf)
{
	struct wps_context *wps;
	struct wps_registrar_config cfg;
	struct wps_ap_pin_fail_method *m;

	if (conf->wps_state && conf->ignore_broadcast_ssid) {
		if (hapd->qtn_wps_on_hidden_ssid) {
			wpa_printf(MSG_INFO, "WPS: wps_on_hidden_ssid "
			   "enable WPS when ignore_broadcast_ssid is on");
		} else {
			wpa_printf(MSG_WARNING, "WPS: ignore_broadcast_ssid "
			   "configuration forced WPS to be disabled");
			conf->wps_state = 0;
		}
	}

	if (conf->wps_state == 0) {
		hostapd_wps_clear_ies(hapd);
		return 0;
	}

	wps = os_zalloc(sizeof(*wps));
	if (wps == NULL)
		return -1;

	wps->cred_cb = hostapd_wps_cred_cb;
	wps->event_cb = hostapd_wps_event_cb;
	wps->probing_db_cb = hostapd_wps_probing_db_process;
	wps->cb_ctx = hapd;

	os_memset(&cfg, 0, sizeof(cfg));
	wps->wps_state = hapd->conf->wps_state;

	if (is_nil_uuid(hapd->conf->uuid)) {
#if 0 /* Each BSS should have diffenrent UUID for Win7 to identify,
	 comment out since it might be useful,
	 if needed, will re-design as requirement */
		const u8 *uuid;
		uuid = get_own_uuid(hapd->iface);
		if (uuid) {
			os_memcpy(wps->uuid, uuid, UUID_LEN);
			wpa_hexdump(MSG_DEBUG, "WPS: Clone UUID from another "
				    "interface", wps->uuid, UUID_LEN);
		} else {
#endif
			uuid_gen_mac_addr(hapd->own_addr, wps->uuid);
			wpa_hexdump(MSG_DEBUG, "WPS: UUID based on MAC "
				    "address", wps->uuid, UUID_LEN);
#if 0
		}
#endif
	} else {
		os_memcpy(wps->uuid, hapd->conf->uuid, UUID_LEN);
		wpa_hexdump(MSG_DEBUG, "WPS: Use configured UUID",
			    wps->uuid, UUID_LEN);
	}
	wps->ssid_len = hapd->conf->ssid.ssid_len;
	os_memcpy(wps->ssid, hapd->conf->ssid.ssid, wps->ssid_len);
	wps->ap = 1;
	os_memcpy(wps->dev.mac_addr, hapd->own_addr, ETH_ALEN);
	wps->dev.device_name = hapd->conf->device_name ?
		os_strdup(hapd->conf->device_name) : NULL;
	wps->dev.manufacturer = hapd->conf->manufacturer ?
		os_strdup(hapd->conf->manufacturer) : NULL;
	wps->dev.model_name = hapd->conf->model_name ?
		os_strdup(hapd->conf->model_name) : NULL;
	wps->dev.model_number = hapd->conf->model_number ?
		os_strdup(hapd->conf->model_number) : NULL;
	wps->dev.serial_number = hapd->conf->serial_number ?
		os_strdup(hapd->conf->serial_number) : NULL;
	wps->dev.pp_device_name = hapd->conf->wps_pp_devname ?
		os_strdup(hapd->conf->wps_pp_devname) : NULL;
	wps->dev.pp_device_name_bl = hapd->conf->wps_pp_devname_bl ?
		os_strdup(hapd->conf->wps_pp_devname_bl) : NULL;
	if (wps->dev.pp_device_name == NULL && wps->dev.pp_device_name_bl == NULL)
		wps->dev.pp_enable = 0;
	else
		wps->dev.pp_enable = hapd->conf->pp_enable;
	wps->config_methods =
		wps_config_methods_str2bin(hapd->conf->config_methods);
#ifdef CONFIG_WPS2
	if ((wps->config_methods &
	     (WPS_CONFIG_DISPLAY | WPS_CONFIG_VIRT_DISPLAY |
	      WPS_CONFIG_PHY_DISPLAY)) == WPS_CONFIG_DISPLAY) {
		wpa_printf(MSG_INFO, "WPS: Converting display to "
			   "virtual_display for WPS 2.0 compliance");
		wps->config_methods |= WPS_CONFIG_VIRT_DISPLAY;
	}
	if ((wps->config_methods &
	     (WPS_CONFIG_PUSHBUTTON | WPS_CONFIG_VIRT_PUSHBUTTON |
	      WPS_CONFIG_PHY_PUSHBUTTON)) == WPS_CONFIG_PUSHBUTTON) {
		wpa_printf(MSG_INFO, "WPS: Converting push_button to "
			   "virtual_push_button for WPS 2.0 compliance");
		wps->config_methods |= WPS_CONFIG_VIRT_PUSHBUTTON;
	}
#endif /* CONFIG_WPS2 */
	os_memcpy(wps->dev.pri_dev_type, hapd->conf->device_type,
		  WPS_DEV_TYPE_LEN);

	if (hostapd_wps_set_vendor_ext(hapd, wps) < 0) {
		os_free(wps);
		return -1;
	}

	hostapd_wps_set_quantenna_ext(hapd, wps);

	wps->dev.os_version = WPA_GET_BE32(hapd->conf->os_version);
	wps->dev.rf_bands = hapd->iconf->hw_mode == HOSTAPD_MODE_IEEE80211A ?
		WPS_RF_50GHZ : WPS_RF_24GHZ; /* FIX: dualband AP */

	if (conf->wpa & WPA_PROTO_RSN) {
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK)
			wps->auth_types |= WPS_AUTH_WPA2PSK;
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			wps->auth_types |= WPS_AUTH_WPA2;

		if (conf->rsn_pairwise & WPA_CIPHER_CCMP)
			wps->encr_types |= WPS_ENCR_AES;
		if (conf->rsn_pairwise & WPA_CIPHER_TKIP)
			wps->encr_types |= WPS_ENCR_TKIP;
	}

	if (conf->wpa & WPA_PROTO_WPA) {
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK)
			wps->auth_types |= WPS_AUTH_WPAPSK;
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			wps->auth_types |= WPS_AUTH_WPA;

		if (conf->wpa_pairwise & WPA_CIPHER_CCMP)
			wps->encr_types |= WPS_ENCR_AES;
		if (conf->wpa_pairwise & WPA_CIPHER_TKIP)
			wps->encr_types |= WPS_ENCR_TKIP;
	}

	if (conf->ssid.security_policy == SECURITY_PLAINTEXT) {
		wps->encr_types |= WPS_ENCR_NONE;
		wps->auth_types |= WPS_AUTH_OPEN;
	} else if (conf->ssid.security_policy == SECURITY_STATIC_WEP) {
		wps->encr_types |= WPS_ENCR_WEP;
		if (conf->auth_algs & WPA_AUTH_ALG_OPEN)
			wps->auth_types |= WPS_AUTH_OPEN;
		if (conf->auth_algs & WPA_AUTH_ALG_SHARED)
			wps->auth_types |= WPS_AUTH_SHARED;
	} else if (conf->ssid.security_policy == SECURITY_IEEE_802_1X) {
		wps->auth_types |= WPS_AUTH_OPEN;
		if (conf->default_wep_key_len)
			wps->encr_types |= WPS_ENCR_WEP;
		else
			wps->encr_types |= WPS_ENCR_NONE;
	}

	if (conf->ssid.wpa_psk_file) {
		/* Use per-device PSKs */
	} else if (conf->ssid.wpa_passphrase) {
		if (hapd->non_wps_pp_enable) {
			wps->network_key = (u8 *) os_strdup(conf->ssid.wpa_passphrase_md5);
		} else {
			wps->network_key = (u8 *) os_strdup(conf->ssid.wpa_passphrase);
		}
		wps->network_key_len = os_strlen(conf->ssid.wpa_passphrase);
	} else if (conf->ssid.wpa_psk) {
		wps->network_key = os_malloc(2 * PMK_LEN + 1);
		if (wps->network_key == NULL) {
			os_free(wps);
			return -1;
		}
		if (hapd->non_wps_pp_enable) {
			wpa_snprintf_hex((char *) wps->network_key, 2 * PMK_LEN + 1,
					 conf->ssid.wpa_psk->psk_md5, PMK_LEN);
		} else {
			wpa_snprintf_hex((char *) wps->network_key, 2 * PMK_LEN + 1,
					 conf->ssid.wpa_psk->psk, PMK_LEN);
		}
		wps->network_key_len = 2 * PMK_LEN;
	} else if (conf->ssid.wep.keys_set && conf->ssid.wep.key[0]) {
		wps->network_key = os_malloc(conf->ssid.wep.len[0]);
		if (wps->network_key == NULL) {
			os_free(wps);
			return -1;
		}
		os_memcpy(wps->network_key, conf->ssid.wep.key[0],
			  conf->ssid.wep.len[0]);
		wps->network_key_len = conf->ssid.wep.len[0];
	}

	if (conf->ssid.wpa_psk) {
		if (hapd->non_wps_pp_enable && conf->ssid.wpa_psk->psk_md5) {
			os_memcpy(wps->psk, conf->ssid.wpa_psk->psk_md5, PMK_LEN);
		} else {
			os_memcpy(wps->psk, conf->ssid.wpa_psk->psk, PMK_LEN);
		}
		wps->psk_set = 1;
	}

	if (conf->wps_state == WPS_STATE_NOT_CONFIGURED) {
		/* Override parameters to enable security by default */
		wps->auth_types = WPS_AUTH_WPA2PSK | WPS_AUTH_WPAPSK;
		wps->encr_types = WPS_ENCR_AES | WPS_ENCR_TKIP;
	}

	wps->ap_settings = conf->ap_settings;
	wps->ap_settings_len = conf->ap_settings_len;
	if (conf->wps_vendor_spec != NULL) {
		if (os_strcasecmp(conf->wps_vendor_spec, "Netgear") == 0) {
			wps->vendor = WPS_VENDOR_NETGEAR;
			os_memset(&wps->wps_stas, 0, sizeof(wps->wps_stas));
		}
	}

	cfg.new_psk_cb = hostapd_wps_new_psk_cb;
	cfg.set_ie_cb = hostapd_wps_set_ie_cb;
	cfg.pin_needed_cb = hostapd_wps_pin_needed_cb;
	cfg.reg_success_cb = hostapd_wps_reg_success_cb;
	cfg.enrollee_seen_cb = hostapd_wps_enrollee_seen_cb;
	cfg.cb_ctx = hapd;
	cfg.skip_cred_build = conf->skip_cred_build;
	cfg.extra_cred = conf->extra_cred;
	cfg.extra_cred_len = conf->extra_cred_len;
	cfg.disable_auto_conf = (hapd->conf->wps_cred_processing == 1) &&
		conf->skip_cred_build;
	if (conf->ssid.security_policy == SECURITY_STATIC_WEP)
		cfg.static_wep_only = 1;
	cfg.dualband = interface_count(hapd->iface) > 1;
	if (cfg.dualband)
		wpa_printf(MSG_DEBUG, "WPS: Dualband AP");
	cfg.wps_on_hidden_ssid = hapd->qtn_wps_on_hidden_ssid;
	cfg.ignore_brodcast_ssid = conf->ignore_broadcast_ssid;
	cfg.ssid_set_cb = hostapd_wps_ssid_set_cb;
	cfg.force_broadcast_uuid = hapd->force_broadcast_uuid;
	cfg.wps_reg_event_cb = hostapd_wps_reg_event_cb;
	wps->pbc_in_srcm = hapd->iconf->pbc_in_srcm;

	m = NULL;
	if (conf->ap_pin_fail_method)
		m = wps_pick_ap_pin_fail_method(conf->ap_pin_fail_method);
	else
		m = wps_pick_ap_pin_fail_method("default");
	if (os_strcmp(m->name, "auto_lockdown") == 0 &&
		!conf->ap_setup_locked &&
		hapd->ap_setup_locked_runtime >= 0)
		wps->ap_setup_locked = hapd->ap_setup_locked_runtime;
	else
		wps->ap_setup_locked = conf->ap_setup_locked;
	m->cfg_init(hapd);
	if (hapd->ap_pin_fail_m != (void *)m) {
		m->reset(hapd, 0);
	}
	hapd->ap_pin_fail_m = (void *)m;

	wps->registrar = wps_registrar_init(wps, &cfg);
	if (wps->registrar == NULL) {
		wpa_printf(MSG_ERROR, "Failed to initialize WPS Registrar");
		os_free(wps->network_key);
		os_free(wps);
		return -1;
	}

#ifdef CONFIG_WPS_UPNP
	wps->friendly_name = hapd->conf->friendly_name;
	wps->manufacturer_url = hapd->conf->manufacturer_url;
	wps->model_description = hapd->conf->model_description;
	wps->model_url = hapd->conf->model_url;
	wps->upc = hapd->conf->upc;
#endif /* CONFIG_WPS_UPNP */

	hostapd_register_probereq_cb(hapd, hostapd_wps_probe_req_rx, hapd);

	hapd->wps = wps;

	return 0;
}


int hostapd_init_wps_complete(struct hostapd_data *hapd)
{
	struct wps_context *wps = hapd->wps;

	if (wps == NULL)
		return 0;

#ifdef CONFIG_WPS_UPNP
	if (hostapd_wps_upnp_init(hapd, wps) < 0) {
		wpa_printf(MSG_ERROR, "Failed to initialize WPS UPnP");
		wps_registrar_deinit(wps->registrar);
		os_free(wps->network_key);
		os_free(wps);
		hapd->wps = NULL;
		return -1;
	}
#endif /* CONFIG_WPS_UPNP */

	hostapd_register_probereq_cb(hapd, hostapd_wps_probe_req_rx, hapd);

	wps->wps_external_state = WPS_EXT_STATE_INI;

	hapd->wps = wps;

	if (hapd->eapol_auth != NULL)
		hapd->eapol_auth->conf.wps = wps;

	return 0;
}


void hostapd_deinit_wps(struct hostapd_data *hapd)
{
	struct wps_ap_pin_fail_method *m;

	if (hapd->wps == NULL)
		return;

	/* usually wps params should be reconfigured from conf file
	 * while tis is an internal value which can be diff from the one
	 * in conf file*/
	hapd->ap_setup_locked_runtime = hapd->wps->ap_setup_locked;
	wpa_printf(MSG_DEBUG, "WPS: [ap_setup_locked] backup value (%d)", hapd->ap_setup_locked_runtime);

	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;
	if (m != NULL) {
		m->reset(hapd, 1);
		if (strcmp(m->name, "default") == 0) {
			eloop_cancel_timeout(hostapd_wps_ap_pin_timeout, hapd, NULL);
		}
	}

#ifdef CONFIG_WPS_UPNP
	hostapd_wps_upnp_deinit(hapd);
#endif /* CONFIG_WPS_UPNP */
	wps_registrar_deinit(hapd->wps->registrar);
	os_free(hapd->wps->network_key);
	wps_device_data_free(&hapd->wps->dev);
	wpabuf_free(hapd->wps->dh_pubkey);
	wpabuf_free(hapd->wps->dh_privkey);
	wpabuf_free(hapd->wps->oob_conf.pubkey_hash);
	wpabuf_free(hapd->wps->oob_conf.dev_password);
	wps_free_pending_msgs(hapd->wps->upnp_msgs);
	os_free(hapd->wps);
	hapd->wps = NULL;
	if (hapd->eapol_auth != NULL)
		hapd->eapol_auth->conf.wps = NULL;
	hostapd_wps_clear_ies(hapd);
}


void hostapd_update_wps(struct hostapd_data *hapd)
{
	if (hapd->wps == NULL)
		return;

#ifdef CONFIG_WPS_UPNP
	hapd->wps->friendly_name = hapd->conf->friendly_name;
	hapd->wps->manufacturer_url = hapd->conf->manufacturer_url;
	hapd->wps->model_description = hapd->conf->model_description;
	hapd->wps->model_url = hapd->conf->model_url;
	hapd->wps->upc = hapd->conf->upc;
#endif /* CONFIG_WPS_UPNP */

	hapd->wps->pbc_in_srcm = hapd->iconf->pbc_in_srcm;

	hostapd_wps_set_vendor_ext(hapd, hapd->wps);

	hostapd_wps_set_quantenna_ext(hapd, hapd->wps);

	if (hapd->conf->wps_state)
		wps_registrar_update_ie(hapd->wps->registrar);
	else
		hostapd_deinit_wps(hapd);
}


struct wps_add_pin_data {
	const u8 *addr;
	const u8 *uuid;
	const u8 *pin;
	size_t pin_len;
	int timeout;
	int added;
};


static int wps_add_pin(struct hostapd_data *hapd, void *ctx)
{
	struct wps_add_pin_data *data = ctx;
	int ret;

	if (hapd->wps == NULL)
		return 0;
	ret = wps_registrar_add_pin(hapd->wps->registrar, data->addr,
				    data->uuid, data->pin, data->pin_len,
				    data->timeout);
	if (ret == 0)
		data->added++;
	return ret;
}


int hostapd_wps_add_pin(struct hostapd_data *hapd, int selected, const u8 *addr,
			const char *uuid, const char *pin, int timeout)
{
	u8 u[UUID_LEN];
	struct wps_add_pin_data data;

	data.addr = addr;
	data.uuid = u;
	data.pin = (const u8 *) pin;
	data.pin_len = os_strlen(pin);
	data.timeout = timeout;
	data.added = 0;

	if (os_strcmp(uuid, "any") == 0) {
		data.uuid = NULL;
	} else if (os_strcmp(uuid, "perm") == 0) {
		data.uuid = NULL;
		timeout = 0;
	} else {
		if (uuid_str2bin(uuid, u))
			return -1;
		data.uuid = u;
	}
	if (!selected) {
		if (hostapd_wps_for_each(hapd, wps_add_pin, &data) < 0)
			return -1;
	} else {
		if (wps_add_pin(hapd, &data) < 0)
			return -1;
	}
	return data.added ? 0 : -1;
}

static int wps_button_pushed(struct hostapd_data *hapd, void *ctx)
{
	const u8 *p2p_dev_addr = ctx;
	if (hapd->wps == NULL)
		return 0;
	return wps_registrar_button_pushed(hapd->wps->registrar, p2p_dev_addr);
}


int hostapd_wps_button_pushed(struct hostapd_data *hapd,
			      const u8 *p2p_dev_addr)
{
	struct hostapd_iface *iface;
	int ret;

	iface = hapd->iface;
	if (iface->default_bss_for_pbc != NULL)
		ret = wps_button_pushed(iface->default_bss_for_pbc, p2p_dev_addr);
	else
		ret = hostapd_wps_for_each(hapd, wps_button_pushed, (void *) p2p_dev_addr);

	return ret;
}

int hostapd_wps_button_pushed_interface(struct hostapd_data *hapd,
				const char *interface)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t j;
	int ret;

	for (j = 0; j < iface->num_bss; j++) {
		struct hostapd_data *hapd = iface->bss[j];
		if (os_strcmp(interface, hapd->conf->iface) == 0) {
			ret = wps_button_pushed(hapd, NULL);
			return ret;
		}
	}

	return -1;
}

#ifdef CONFIG_WPS_OOB
int hostapd_wps_start_oob(struct hostapd_data *hapd, char *device_type,
			  char *path, char *method, char *name)
{
	struct wps_context *wps = hapd->wps;
	struct oob_device_data *oob_dev;

	oob_dev = wps_get_oob_device(device_type);
	if (oob_dev == NULL)
		return -1;
	oob_dev->device_path = path;
	oob_dev->device_name = name;
	wps->oob_conf.oob_method = wps_get_oob_method(method);

	if (wps->oob_conf.oob_method == OOB_METHOD_DEV_PWD_R) {
		/*
		 * Use pre-configured DH keys in order to be able to write the
		 * key hash into the OOB file.
		 */
		wpabuf_free(wps->dh_pubkey);
		wpabuf_free(wps->dh_privkey);
		wps->dh_privkey = NULL;
		wps->dh_pubkey = dh_init(dh_groups_get(WPS_DH_GROUP),
					 &wps->dh_privkey);
		wps->dh_pubkey = wpabuf_zeropad(wps->dh_pubkey, 192);
		if (wps->dh_pubkey == NULL) {
			wpa_printf(MSG_ERROR, "WPS: Failed to initialize "
				   "Diffie-Hellman handshake");
			return -1;
		}
	}

	if (wps_process_oob(wps, oob_dev, 1) < 0)
		goto error;

	if ((wps->oob_conf.oob_method == OOB_METHOD_DEV_PWD_E ||
	     wps->oob_conf.oob_method == OOB_METHOD_DEV_PWD_R) &&
	    hostapd_wps_add_pin(hapd, 0, NULL, "any",
				wpabuf_head(wps->oob_conf.dev_password), 0) <
	    0)
		goto error;

	return 0;

error:
	wpabuf_free(wps->dh_pubkey);
	wps->dh_pubkey = NULL;
	wpabuf_free(wps->dh_privkey);
	wps->dh_privkey = NULL;
	return -1;
}
#endif /* CONFIG_WPS_OOB */


static int hostapd_wps_probe_req_rx(void *ctx, const u8 *addr, const u8 *da,
				    const u8 *bssid,
				    const u8 *ie, size_t ie_len)
{
	struct hostapd_data *hapd = ctx;
	struct wpabuf *wps_ie;
	struct ieee802_11_elems elems;

	if (hapd->wps == NULL)
		return 0;

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "WPS: Could not parse ProbeReq from "
			   MACSTR, MAC2STR(addr));
		return 0;
	}

	if (elems.ssid && elems.ssid_len > 0 &&
	    (elems.ssid_len != hapd->conf->ssid.ssid_len ||
	     os_memcmp(elems.ssid, hapd->conf->ssid.ssid, elems.ssid_len) !=
	     0))
		return 0; /* Not for us */

	wps_ie = ieee802_11_vendor_ie_concat(ie, ie_len, WPS_DEV_OUI_WFA);

	if (wps_ie == NULL) {
		if (hapd->wps->vendor == WPS_VENDOR_NETGEAR) {
			if (wps_registrar_check_selected(hapd->wps->registrar) &&
				ap_get_sta(hapd, addr) == NULL) {
				if (hostapd_wps_probing_db_process(hapd, addr, 1) != -1) {
					hostapd_wps_event_cb(hapd, WPS_EV_STA_CANCEL, NULL);
				}
			}
		}
		return 0;
	}
	if (wps_validate_probe_req(wps_ie, addr) < 0) {
		wpabuf_free(wps_ie);
		return 0;
	}

	if (wpabuf_len(wps_ie) > 0) {
		int p2p_wildcard = 0;
#ifdef CONFIG_P2P
		if (elems.ssid && elems.ssid_len == P2P_WILDCARD_SSID_LEN &&
		    os_memcmp(elems.ssid, P2P_WILDCARD_SSID,
			      P2P_WILDCARD_SSID_LEN) == 0)
			p2p_wildcard = 1;
#endif /* CONFIG_P2P */
		wps_registrar_probe_req_rx(hapd->wps->registrar, addr, wps_ie,
					   p2p_wildcard);
#ifdef CONFIG_WPS_UPNP
		/* FIX: what exactly should be included in the WLANEvent?
		 * WPS attributes? Full ProbeReq frame? */
		if (!p2p_wildcard)
			upnp_wps_device_send_wlan_event(
				hapd->wps_upnp, addr,
				UPNP_WPS_WLANEVENT_TYPE_PROBE, wps_ie);
#endif /* CONFIG_WPS_UPNP */
	}

	wpabuf_free(wps_ie);

	return 0;
}


#ifdef CONFIG_WPS_UPNP

static int hostapd_rx_req_put_wlan_response(
	void *priv, enum upnp_wps_wlanevent_type ev_type,
	const u8 *mac_addr, const struct wpabuf *msg,
	enum wps_msg_type msg_type)
{
	struct hostapd_data *hapd = priv;
	struct sta_info *sta;
	struct upnp_pending_message *p;

	wpa_printf(MSG_DEBUG, "WPS UPnP: PutWLANResponse ev_type=%d mac_addr="
		   MACSTR, ev_type, MAC2STR(mac_addr));
	wpa_hexdump(MSG_MSGDUMP, "WPS UPnP: PutWLANResponse NewMessage",
		    wpabuf_head(msg), wpabuf_len(msg));
	if (ev_type != UPNP_WPS_WLANEVENT_TYPE_EAP) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: Ignored unexpected "
			   "PutWLANResponse WLANEventType %d", ev_type);
		return -1;
	}

	/*
	 * EAP response to ongoing to WPS Registration. Send it to EAP-WSC
	 * server implementation for delivery to the peer.
	 */

	sta = ap_get_sta(hapd, mac_addr);
#ifndef CONFIG_WPS_STRICT
	if (!sta) {
		/*
		 * Workaround - Intel wsccmd uses bogus NewWLANEventMAC:
		 * Pick STA that is in an ongoing WPS registration without
		 * checking the MAC address.
		 */
		wpa_printf(MSG_DEBUG, "WPS UPnP: No matching STA found based "
			   "on NewWLANEventMAC; try wildcard match");
		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (sta->eapol_sm && (sta->flags & WLAN_STA_WPS))
				break;
		}
	}
#endif /* CONFIG_WPS_STRICT */

	if (!sta || !(sta->flags & WLAN_STA_WPS)) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: No matching STA found");
		return 0;
	}

	p = os_zalloc(sizeof(*p));
	if (p == NULL)
		return -1;
	os_memcpy(p->addr, sta->addr, ETH_ALEN);
	p->msg = wpabuf_dup(msg);
	p->type = msg_type;
	p->next = hapd->wps->upnp_msgs;
	hapd->wps->upnp_msgs = p;

	return eapol_auth_eap_pending_cb(sta->eapol_sm, sta->eapol_sm->eap);
}


int hostapd_wps_upnp_init(struct hostapd_data *hapd,
				 struct wps_context *wps)
{
	struct upnp_wps_device_ctx *ctx;

	if (!hapd->conf->upnp_iface)
		return 0;
	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
		return -1;

	ctx->rx_req_put_wlan_response = hostapd_rx_req_put_wlan_response;
	if (hapd->conf->ap_pin)
		ctx->ap_pin = os_strdup(hapd->conf->ap_pin);

	hapd->wps_upnp = upnp_wps_device_init(ctx, wps, hapd,
					      hapd->conf->upnp_iface);
	if (hapd->wps_upnp == NULL)
		return -1;
	wps->wps_upnp = hapd->wps_upnp;

	return 0;
}


void hostapd_wps_upnp_deinit(struct hostapd_data *hapd)
{
	upnp_wps_device_deinit(hapd->wps_upnp, hapd);

	/* clean to indicate upnp disabled */
	hapd->wps->wps_upnp = NULL;
	hapd->wps_upnp = NULL;
}

#endif /* CONFIG_WPS_UPNP */


int hostapd_wps_get_mib_sta(struct hostapd_data *hapd, const u8 *addr,
			    char *buf, size_t buflen)
{
	if (hapd->wps == NULL)
		return 0;
	return wps_registrar_get_info(hapd->wps->registrar, addr, buf, buflen);
}


static void hostapd_wps_ap_pin_timeout(void *eloop_data, void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;
	wpa_printf(MSG_DEBUG, "WPS: AP PIN timed out");
	hostapd_wps_ap_pin_disable(hapd);
	wpa_msg(hapd->msg_ctx, MSG_INFO, WPS_EVENT_AP_PIN_DISABLED);
}


void hostapd_wps_ap_pin_enable(struct hostapd_data *hapd, int timeout)
{
	struct wps_ap_pin_fail_method *m;

	if (!hapd->wps)
		return;

	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;

	if (m != NULL) {
		m->reset(hapd, 1);
		if (strcmp(m->name, "default") == 0) {
			eloop_cancel_timeout(hostapd_wps_ap_pin_timeout, hapd, NULL);
			if (timeout > 0) {
				eloop_register_timeout(timeout, 0,
						       hostapd_wps_ap_pin_timeout, hapd, NULL);
			}
		}
	}
	wps_update_ap_setup_locked(hapd, 0);
}


static int wps_ap_pin_disable(struct hostapd_data *hapd, void *ctx)
{
	struct wps_ap_pin_fail_method *m;

	if (!hapd->wps)
		return 0;

	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;
	if (m != NULL) {
		m->reset(hapd, 1);
		if (strcmp(m->name, "default") == 0) {
			/* in my opinion, disabling ap pin is not necessarily
			  related with removing the one already set.
			 * just keep this action within current implementaion
			 * for compatibility concern */
			os_free(hapd->conf->ap_pin);
			hapd->conf->ap_pin = NULL;
#ifdef CONFIG_WPS_UPNP
			upnp_wps_set_ap_pin(hapd->wps_upnp, NULL);
#endif /* CONFIG_WPS_UPNP */
			eloop_cancel_timeout(hostapd_wps_ap_pin_timeout, hapd, NULL);
		}
	}
	wps_update_ap_setup_locked(hapd, 1);

	return 0;
}


void hostapd_wps_ap_pin_disable(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "WPS: Disabling AP PIN");
	wps_ap_pin_disable(hapd, NULL);
}


struct wps_ap_pin_data {
	char pin_txt[WPS_AP_PIN_LEN + 1];
	int timeout;
};


static int wps_ap_pin_set(struct hostapd_data *hapd, void *ctx)
{
	struct wps_ap_pin_data *data = ctx;
	struct wps_ap_pin_fail_method *m;

	if (hapd->wps == NULL || hapd->conf->wps_state == 0) {
		return 0;
	}

	os_free(hapd->conf->ap_pin);
	hapd->conf->ap_pin = os_strdup(data->pin_txt);
#ifdef CONFIG_WPS_UPNP
	upnp_wps_set_ap_pin(hapd->wps_upnp, data->pin_txt);
#endif /* CONFIG_WPS_UPNP */

	/* keep compatibility with current implementation
	 * while make each api do their own job with
	 * new method*/
	m = (struct wps_ap_pin_fail_method *)hapd->ap_pin_fail_m;
	if (m != NULL && strcmp(m->name, "default") == 0)
		hostapd_wps_ap_pin_enable(hapd, data->timeout);
	return 0;
}


const char * hostapd_wps_ap_pin_random(struct hostapd_data *hapd, int timeout)
{
	unsigned int pin;
	struct wps_ap_pin_data data;

	if (hapd->conf->wps_state == 0) {
		return "DISABLED";
	}

	pin = wps_generate_pin();
	os_snprintf(data.pin_txt, sizeof(data.pin_txt), "%08u", pin);
	data.timeout = timeout;
	wps_ap_pin_set(hapd, &data);
	return hapd->conf->ap_pin;
}


const char * hostapd_wps_ap_pin_get(struct hostapd_data *hapd)
{
	return hapd->conf->ap_pin;
}


int hostapd_wps_ap_pin_set(struct hostapd_data *hapd, const char *pin,
			   int timeout)
{
	struct wps_ap_pin_data data;
	int ret;

	ret = os_snprintf(data.pin_txt, sizeof(data.pin_txt), "%s", pin);
	if (ret < 0 || ret >= (int) sizeof(data.pin_txt))
		return -1;
	data.timeout = timeout;
	return wps_ap_pin_set(hapd, &data);
}


static int wps_update_ie(struct hostapd_data *hapd, void *ctx)
{
	if (hapd->wps)
		wps_registrar_update_ie(hapd->wps->registrar);
	return 0;
}


void hostapd_wps_update_ie(struct hostapd_data *hapd)
{
	hostapd_wps_for_each(hapd, wps_update_ie, NULL);
}


int hostapd_wps_config_ap(struct hostapd_data *hapd, const char *ssid,
			  const char *auth, const char *encr, const char *key)
{
	struct wps_credential cred;
	size_t len;

	os_memset(&cred, 0, sizeof(cred));

	len = os_strlen(ssid);
	if ((len & 1) || len > 2 * sizeof(cred.ssid) ||
	    hexstr2bin(ssid, cred.ssid, len / 2))
		return -1;
	cred.ssid_len = len / 2;

	if (os_strncmp(auth, "OPEN", 4) == 0)
		cred.auth_type = WPS_AUTH_OPEN;
	else if (os_strncmp(auth, "WPAPSK", 6) == 0)
		cred.auth_type = WPS_AUTH_WPAPSK;
	else if (os_strncmp(auth, "WPA2PSK", 7) == 0)
		cred.auth_type = WPS_AUTH_WPA2PSK;
	else
		return -1;

	if (encr) {
		if (os_strncmp(encr, "NONE", 4) == 0)
			cred.encr_type = WPS_ENCR_NONE;
		else if (os_strncmp(encr, "WEP", 3) == 0)
			cred.encr_type = WPS_ENCR_WEP;
		else if (os_strncmp(encr, "TKIP", 4) == 0)
			cred.encr_type = WPS_ENCR_TKIP;
		else if (os_strncmp(encr, "CCMP", 4) == 0)
			cred.encr_type = WPS_ENCR_AES;
		else
			return -1;
	} else
		cred.encr_type = WPS_ENCR_NONE;

	if (key) {
		len = os_strlen(key);
		if ((len & 1) || len > 2 * sizeof(cred.key) ||
		    hexstr2bin(key, cred.key, len / 2))
			return -1;
		cred.key_len = len / 2;
	}

	return wps_registrar_config_ap(hapd->wps->registrar, &cred);
}

static void hostapd_wps_sta_cancel(void *eloop_data, void *user_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *)eloop_data;

	printf("[Casper] set externl wps state to WPS CANCEL\n");
	hostapd_wps_event_cb(hapd, WPS_EV_STA_CANCEL, NULL);
}

void hostapd_sta_require_leave(struct hostapd_data *hapd, u8 *addr)
{
	struct sta_info *info;

	if (hapd->wps == NULL)
		return;

	if (hapd->wps->vendor != WPS_VENDOR_NETGEAR)
		return;

	info = ap_get_sta(hapd, addr);
	if (info == NULL || !(info->flags & WLAN_STA_WPS))
		return;

	if (wps_registrar_check_selected(hapd->wps->registrar) &&
		(hapd->wps->wps_external_state == WPS_EXT_STATE_PROCESS_START ||
		 hapd->wps->wps_external_state == WPS_EXT_STATE_EAP_M2_SEND ||
		 hapd->wps->wps_external_state == WPS_EXT_STATE_EAP_M8_SEND))
		eloop_register_timeout(0, 100000, hostapd_wps_sta_cancel, hapd, NULL);
}

int hostapd_wps_probing_db_process(struct hostapd_data *hapd, const u8 *addr, int remove)
{
	struct wps_context *wps = hapd->wps;
	int found;
	int i, j;

	if (wps == NULL)
		return 0;

	found = -1;
	for (i = 0; i < wps->wps_stas.used; i++) {
		if (os_memcmp(wps->wps_stas.stas[i], addr, ETH_ALEN) == 0) {
			found = i;
			if (remove) {
				for (j = i; j < WPS_MAX_STAS_RECORD_SIZE - 1; j++)
					os_memmove(wps->wps_stas.stas[j],
							wps->wps_stas.stas[j + 1],
							ETH_ALEN);
				wps->wps_stas.used--;
			}
			break;
		}
	}

	if (!remove && found == -1 && wps->wps_stas.used < WPS_MAX_STAS_RECORD_SIZE) {
		os_memcpy(wps->wps_stas.stas[wps->wps_stas.used], addr, ETH_ALEN);
		wps->wps_stas.used++;
	}

	return found;
}


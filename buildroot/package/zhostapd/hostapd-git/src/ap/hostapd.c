/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
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
#include <net/if.h>
#include <sys/ioctl.h>
#ifdef USE_KERNEL_HEADERS
#include <linux/if_packet.h>
#else /* USE_KERNEL_HEADERS */
#include <netpacket/packet.h>
#endif /* USE_KERNEL_HEADERS */
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <net80211/ieee80211_ioctl.h>

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "radius/radius_client.h"
#include "drivers/driver.h"
#include "hostapd.h"
#include "authsrv.h"
#include "sta_info.h"
#include "accounting.h"
#include "ap_list.h"
#include "beacon.h"
#include "iapp.h"
#include "ieee802_1x.h"
#include "ieee802_11_auth.h"
#include "vlan_init.h"
#include "wpa_auth.h"
#include "wps_hostapd.h"
#include "hw_features.h"
#include "wpa_auth_glue.h"
#include "ap_drv_ops.h"
#include "ap_config.h"
#include "p2p_hostapd.h"


static int hostapd_flush_old_stations(struct hostapd_data *hapd);
static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd);
static int hostapd_broadcast_wep_clear(struct hostapd_data *hapd);
static int hostapd_setup_bss(struct hostapd_data *hapd, int first);
static void hostapd_cleanup(struct hostapd_data *hapd);
static int hostapd_check_for_removed_bss(struct hostapd_iface *iface, struct hostapd_config *newconf);
static int hostapd_find_bss(struct hostapd_iface *iface, char *ifname);

extern int wpa_debug_level;

static int
hostapd_sta_remove_sm(struct hostapd_data *hapd,
		      struct sta_info *sta, void *ctx)
{
	wpa_auth_sta_deinit(sta->wpa_sm);
	sta->wpa_sm = NULL;
	return 0;
}

static void hostapd_reload_bss(struct hostapd_data *hapd)
{
#ifndef CONFIG_NO_RADIUS
	radius_client_reconfig(hapd->radius, hapd->conf->radius);
#endif /* CONFIG_NO_RADIUS */

	if (hapd->conf->wmm_enabled < 0)
		hapd->conf->wmm_enabled = hapd->iconf->ieee80211n;

	if (hostapd_setup_wpa_psk(hapd->conf)) {
		wpa_printf(MSG_ERROR, "Failed to re-configure WPA PSK "
			   "after reloading configuration");
	}

	if (hapd->conf->ieee802_1x || hapd->conf->wpa)
		hostapd_set_drv_ieee8021x(hapd, hapd->conf->iface, 1);
	else
		hostapd_set_drv_ieee8021x(hapd, hapd->conf->iface, 0);

	ieee802_1x_eap_auth_update(hapd);

	if (hapd->conf->wpa && hapd->wpa_auth == NULL) {
		hostapd_setup_wpa(hapd);
		if (hapd->wpa_auth)
			wpa_init_keys(hapd->wpa_auth);
	} else if (hapd->conf->wpa) {
		const u8 *wpa_ie;
		size_t wpa_ie_len;
		hostapd_reconfig_wpa(hapd);
		wpa_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &wpa_ie_len);
		if (hostapd_set_generic_elem(hapd, wpa_ie, wpa_ie_len))
			wpa_printf(MSG_ERROR, "Failed to configure WPA IE for "
				   "the kernel driver.");
	} else if (hapd->wpa_auth) {
		/* Disabling security */
		ap_for_each_sta(hapd, hostapd_sta_remove_sm, NULL);
		wpa_deinit(hapd->wpa_auth);
		hapd->wpa_auth = NULL;
		hostapd_set_privacy(hapd, 0);
		hostapd_broadcast_wep_clear(hapd);
		hostapd_setup_encryption(hapd->conf->iface, hapd);
		hostapd_set_generic_elem(hapd, (u8 *) "", 0);
	}

	/* Enable or disable SSID broadcast in beacons */
	if (hostapd_set_broadcast_ssid(hapd,
			!!hapd->conf->ignore_broadcast_ssid)) {
		wpa_printf(MSG_WARNING, "Could not modify broadcast SSID flag");
	}

	ieee802_11_set_beacon(hapd);
	hostapd_deinit_wps(hapd);
	if (hostapd_init_wps(hapd, hapd->conf)) {
		wpa_printf(MSG_ERROR, "Could not reconfigure WPS");
	}
	hostapd_init_wps_complete(hapd);

	if (hapd->conf->ssid.ssid_set &&
	    hostapd_set_ssid(hapd, (u8 *) hapd->conf->ssid.ssid,
			     hapd->conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		/* try to continue */
	}

	if (hapd->conf->set_assoc_limit_required &&
			hostapd_set_bss_assoc_limit(hapd, hapd->conf->max_num_sta)) {
		wpa_printf(MSG_ERROR, "Could not set max_num_sta for kernel driver");
	}

	if (hapd->conf && hapd->conf->pairing_id &&
			hostapd_setup_pairing_hash(hapd->conf->pairing_id,
					hapd->own_addr, hapd->conf->pairing_hash) < 0) {
		wpa_printf(MSG_ERROR, "Pairing hash initializing failed");
		return;
	}

	if (hapd->conf->pairing_id && hapd->conf->pairing_enable == PPS2_MODE_ACCEPT) {
		if (hostapd_drv_set_pairing_hash_ie(hapd,
				hapd->conf->pairing_hash, PAIRING_HASH_LEN) < 0) {
			wpa_printf(MSG_ERROR, "Setting pairing hash IE failed");
			return;
		}
	} else {
		if (hostapd_drv_set_pairing_hash_ie(hapd,
						hapd->conf->pairing_hash, 0) < 0) {
			wpa_printf(MSG_ERROR, "Clearing pairing hash IE failed");
			return;
		}
	}

	wpa_printf(MSG_DEBUG, "Reconfigured interface %s", hapd->conf->iface);
}

static int find_bss_config(struct hostapd_config *conf, const char *bss)
{
	size_t j;
	if (conf == NULL)
		return -1;

	for (j = 0; j < MAX_BSSID; j++) {
		if (conf->bss[j].bssconf_in_use && strncmp(conf->bss[j].iface, bss, IFNAMSIZ) == 0)
			return j;
	}

	return -1;
}

int hostapd_create_bss_config(struct hostapd_iface *iface, const char *bss_name)
{
	struct hostapd_config *tmpconf, *curconf;
	int tmp_index, bss_index;
	struct hostapd_data *hapd_bss;
	struct hostapd_bss_config *free_bss_cfg;
	int i,j;

	if (iface->config_read_cb == NULL)
		return -1;
	tmpconf = iface->config_read_cb(iface->config_fname);
	if (tmpconf == NULL)
		return -1;

	tmp_index = find_bss_config(tmpconf, bss_name);
	bss_index = hostapd_find_bss(iface, bss_name);

	if ((tmp_index >= 0) && (bss_index < 0)) {
		curconf = iface->conf;
		if (iface->num_bss > MAX_BSSID) {
			wpa_printf(MSG_ERROR, "max bss num reach, create new bss fail\n");
			hostapd_config_free(tmpconf);
			return -1;
		}

		for (i = 0; i < MAX_BSSID; i++) {
			if (curconf->bss[i].bssconf_in_use == 0) {
				free_bss_cfg = &curconf->bss[i];
				break;
			}
		}

		if (i == MAX_BSSID) {
			wpa_printf(MSG_ERROR, "bss do not reach max(%d), "
					"but bss configs are all in use\n",
					iface->num_bss);
			hostapd_config_free(tmpconf);
			return -1;
		}

		memcpy(free_bss_cfg, &tmpconf->bss[tmp_index], sizeof(struct hostapd_bss_config));
		curconf->num_bss++;
		curconf->last_bss = free_bss_cfg;
		tmpconf->bss[tmp_index].bssconf_in_use = 0;
		free_bss_cfg->bssconf_in_use = 1;

		hapd_bss = hostapd_alloc_bss_data(iface, iface->conf,
				iface->conf->last_bss);
		iface->bss[iface->num_bss++] = hapd_bss;
		hapd_bss->msg_ctx = hapd_bss;
		hostapd_setup_bss(hapd_bss, 0);
	} else {
		wpa_printf(MSG_ERROR, "Invalid bss %s\n", bss_name);
		hostapd_config_free(tmpconf);
		return -1;
	}

	hostapd_config_free(tmpconf);

	if (hostapd_driver_commit(hapd_bss) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
			   "configuration", __func__);
		return -1;
	}

	return 0;
}

int hostapd_remove_bss_config(struct hostapd_iface *iface, const char *bss_name)
{
	struct hostapd_data *primary_bss = iface->bss[0];
	struct hostapd_data *bss_to_remove;
	struct hostapd_config *hapd_conf;
	struct hostapd_bss_config *bssconf_to_remove;
	int bss_index, remove_config_index;
	int i,j;

	hapd_conf = iface->conf;
	bss_index = hostapd_find_bss(iface, bss_name);
	remove_config_index = find_bss_config(hapd_conf, bss_name);

	if (bss_index < 0 || remove_config_index < 0)
		return -1;

	bssconf_to_remove = &hapd_conf->bss[remove_config_index];
	bss_to_remove = iface->bss[bss_index];

	/* could not remove primary interface */
	if (bss_to_remove->primary_interface) {
		wpa_printf(MSG_ERROR, "could not remove primary interface");
		return -1;
	}

	/* restore bss for HW Push button to primary interface */
	if (iface->bss[bss_index] == iface->default_bss_for_pbc) {
		for (i = 0; i < iface->num_bss; i++) {
			if (iface->bss[i]->primary_interface)
				iface->default_bss_for_pbc = iface->bss[i];
		}
	}

	hostapd_flush_old_stations(bss_to_remove);
	hostapd_broadcast_wep_clear(bss_to_remove);

#ifndef CONFIG_NO_RADIUS
	radius_client_flush(bss_to_remove->radius, 0);
#endif /* CONFIG_NO_RADIUS */

	hostapd_free_stas(bss_to_remove);
	hostapd_cleanup(bss_to_remove);
	iface->num_bss--;
	for (j = bss_index; j < (MAX_BSSID - 1); j++)
		iface->bss[j] = iface->bss[j + 1];
	os_free(bss_to_remove);

	hostapd_config_free_bss(bssconf_to_remove);
	hapd_conf->num_bss = iface->num_bss;

	return 0;
}

static int hostapd_is_if_up(int skfd, const char *ifname)
{
	struct ifreq	ifr;
	int retval = -1;

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	retval = ioctl(skfd, SIOCGIFFLAGS, &ifr);

	if (retval >= 0)
	{
		int	interface_up_flags = (IFF_UP | IFF_RUNNING);
		int	result_flags = ifr.ifr_flags & (interface_up_flags);

		if (result_flags == interface_up_flags)
			return 0;
		else
			return -1;
	} else {
		return retval;
	}
}

int hostapd_update_bss_config(struct hostapd_iface *iface, const char *bss)
{
	struct hostapd_config *tmpconf;
	int tmp_index, bss_index;
	struct hostapd_data *hapd_bss;
	int if_up = -1;

	if (iface->config_read_cb == NULL)
		return -1;
	tmpconf = iface->config_read_cb(iface->config_fname);
	if (tmpconf == NULL)
		return -1;

	tmp_index = find_bss_config(tmpconf, bss);
	bss_index = hostapd_find_bss(iface, bss);

	if ((tmp_index >= 0) && (bss_index >= 0)) {
		const char *current_wps_ap_pin = NULL;
		char retained_wps_ap_pin[WPS_AP_PIN_LEN + 1];
		int j;

		hapd_bss = iface->bss[bss_index];
		hostapd_flush_old_stations(hapd_bss);
		hostapd_broadcast_wep_clear(hapd_bss);
		vlan_deinit(hapd_bss);

#ifndef CONFIG_NO_RADIUS
		radius_client_flush(iface->bss[bss_index]->radius, 0);
#endif /* CONFIG_NO_RADIUS */

		hostapd_config_free_bss(hapd_bss->conf);
		memcpy(hapd_bss->conf, &tmpconf->bss[tmp_index], sizeof(struct hostapd_bss_config));
		tmpconf->bss[tmp_index].bssconf_in_use = 0;
		hapd_bss->conf->bssconf_in_use = 1;

		current_wps_ap_pin = hostapd_wps_ap_pin_get(hapd_bss);
		if (current_wps_ap_pin) {
			os_strncpy(retained_wps_ap_pin, current_wps_ap_pin,
					WPS_AP_PIN_LEN);
			retained_wps_ap_pin[WPS_AP_PIN_LEN] = 0;
		}

		hostapd_reload_bss(hapd_bss);

		if (current_wps_ap_pin && !hapd_bss->conf->ap_pin) {
			hostapd_wps_ap_pin_set(hapd_bss, (const char *) &retained_wps_ap_pin,
					WPS_AP_PIN_DEFAULT_TIMEOUT);
		}
		if_up = hostapd_is_if_up(hapd_bss->ctrl_sock, bss);
	}  else {
		hostapd_config_free(tmpconf);
		return -1;
	}

	hostapd_config_free(tmpconf);

	if (if_up >= 0 && hostapd_driver_commit(hapd_bss) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
			   "configuration", __func__);
		return -1;
	}

	return 0;
}

int hostapd_reload_config(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	struct hostapd_config *newconf, *oldconf;
	size_t i, j;

	if (iface->config_read_cb == NULL)
		return -1;
	newconf = iface->config_read_cb(iface->config_fname);
	if (newconf == NULL)
		return -1;

	/*
	 * Deauthenticate all stations since the new configuration may not
	 * allow them to use the BSS anymore.
	 */
	for (j = 0; j < iface->num_bss; j++) {
		hostapd_flush_old_stations(iface->bss[j]);
		hostapd_broadcast_wep_clear(iface->bss[j]);

#ifndef CONFIG_NO_RADIUS
		/* TODO: update dynamic data based on changed configuration
		 * items (e.g., open/close sockets, etc.) */
		radius_client_flush(iface->bss[j]->radius, 0);
#endif /* CONFIG_NO_RADIUS */
	}

	oldconf = hapd->iconf;
	iface->conf = newconf;

	/* Remove old BSSes first */
	while (hostapd_check_for_removed_bss(iface, newconf));

	/* Reconfigure existing and add new interfaces */
	for (i = 0; i < newconf->num_bss; i++) {
		struct hostapd_data *hapd_bss;
		int idx = hostapd_find_bss(iface, newconf->bss[i].iface);
		int if_up = 0;
		if (idx >= 0) {
			const char *current_wps_ap_pin = NULL;
			char retained_wps_ap_pin[WPS_AP_PIN_LEN + 1];

			hapd_bss = iface->bss[idx];

			/* Check if WPS AP Pin is set. If so, retain it. */
			current_wps_ap_pin = hostapd_wps_ap_pin_get(hapd_bss);
			if (current_wps_ap_pin) {
				os_strncpy(retained_wps_ap_pin, current_wps_ap_pin,
						WPS_AP_PIN_LEN);
				retained_wps_ap_pin[WPS_AP_PIN_LEN] = 0;
			}

			hapd_bss->iconf = newconf;
			hapd_bss->conf = &newconf->bss[i];
			/* TODO: compare iface->bss[idx].conf with
			 * newconf->bss[i] and reload iff different */

			hostapd_reload_bss(hapd_bss);

			if (current_wps_ap_pin && !hapd_bss->conf->ap_pin) {
				hostapd_wps_ap_pin_set(hapd_bss, (const char *) &retained_wps_ap_pin,
						WPS_AP_PIN_DEFAULT_TIMEOUT);
			}
			if_up = hostapd_is_if_up(hapd->ctrl_sock, iface->config_fname);
		} else {
			/* Create new BSS */
			hapd_bss = hostapd_alloc_bss_data(iface, newconf,
					&newconf->bss[i]);
			iface->bss[i] = hapd_bss;
			hapd_bss->msg_ctx = hapd_bss;
			hostapd_setup_bss(iface->bss[i], 0);
		}

		if (if_up >= 0 && hostapd_driver_commit(hapd_bss) < 0) {
			wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
				   "configuration", __func__);
			hostapd_config_free(oldconf);
			return -1;
		}
	}

	iface->num_bss = newconf->num_bss;
	hostapd_config_free(oldconf);

	return 0;
}


/* Find the index of a BSS by name on a given parent iface */
static int hostapd_find_bss(struct hostapd_iface *iface, char *ifname)
{
	int i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];
		if (!hapd)
			return -1;

		if (strncmp(ifname, hapd->conf->iface, IFNAMSIZ) == 0)
			return i;
	}

	/* Not found */
	return -1;
}

/*
 * The configuration can contain interfaces in any order. The strategy on
 * configuration reload is to iterate through each item in the new config and
 * compare with the actual running BSSes, not just the current configuration.
 * If a BSS has been removed, the array of pointers to BSSes needs to be
 * "shuffled down" to prevent leaving gaps. But the interfaces can still be in
 * any order, i.e. ifname is not related to their BSS index.
 */

/* Check new configuration against currently configured BSSes. Returns 1 if a
 * BSS was removed, 0 if there were none / no more to remove */
static int hostapd_check_for_removed_bss(struct hostapd_iface *iface,
		struct hostapd_config *newconf)
{
	int i, j;

	for (i = 0; i < iface->num_bss; i++) {
		int active = 0;
		for (j = 0; j < newconf->num_bss; j++) {
			if (strncmp(iface->bss[i]->conf->iface,
					newconf->bss[j].iface,
					IFNAMSIZ) == 0) {
				active = 1;
				break;
			}
		}
		if (!active) {
			struct hostapd_data *bss_hapd = iface->bss[i];
			wpa_printf(MSG_DEBUG, "Interface %s removed",
					bss_hapd->conf->iface);
			hostapd_free_stas(bss_hapd);
			hostapd_flush_old_stations(bss_hapd);
			hostapd_cleanup(bss_hapd);
			os_free(bss_hapd);
			iface->num_bss--;
			/* Shuffle pointers down */
			for (j = i; j < MAX_BSSID - 1; j++)
				iface->bss[j] = iface->bss[j + 1];
			return 1;
		}
	}

	/* Nothing else left to remove */
	return 0;
}



static void hostapd_broadcast_key_clear_iface(struct hostapd_data *hapd,
					      char *ifname)
{
	int i;

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_NONE, NULL, i,
					0, NULL, 0, NULL, 0)) {
			wpa_printf(MSG_DEBUG, "Failed to clear default "
				   "encryption keys (ifname=%s keyidx=%d)",
				   ifname, i);
		}
	}
#ifdef CONFIG_IEEE80211W
	if (hapd->conf->ieee80211w) {
		for (i = NUM_WEP_KEYS; i < NUM_WEP_KEYS + 2; i++) {
			if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_NONE,
						NULL, i, 0, NULL,
						0, NULL, 0)) {
				wpa_printf(MSG_DEBUG, "Failed to clear "
					   "default mgmt encryption keys "
					   "(ifname=%s keyidx=%d)", ifname, i);
			}
		}
	}
#endif /* CONFIG_IEEE80211W */
}


static int hostapd_broadcast_wep_clear(struct hostapd_data *hapd)
{
	hostapd_broadcast_key_clear_iface(hapd, hapd->conf->iface);
	return 0;
}


static int hostapd_broadcast_wep_set(struct hostapd_data *hapd)
{
	int errors = 0, idx;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	idx = ssid->wep.idx;
	if (ssid->wep.default_len &&
	    hostapd_drv_set_key(hapd->conf->iface,
				hapd, WPA_ALG_WEP, broadcast_ether_addr, idx,
				1, NULL, 0, ssid->wep.key[idx],
				ssid->wep.len[idx])) {
		wpa_printf(MSG_WARNING, "Could not set WEP encryption.");
		errors++;
	}

	if (ssid->dyn_vlan_keys) {
		size_t i;
		for (i = 0; i <= ssid->max_dyn_vlan_keys; i++) {
			const char *ifname;
			struct hostapd_wep_keys *key = ssid->dyn_vlan_keys[i];
			if (key == NULL)
				continue;
			ifname = hostapd_get_vlan_id_ifname(hapd->conf->vlan,
							    i);
			if (ifname == NULL)
				continue;

			idx = key->idx;
			if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_WEP,
						broadcast_ether_addr, idx, 1,
						NULL, 0, key->key[idx],
						key->len[idx])) {
				wpa_printf(MSG_WARNING, "Could not set "
					   "dynamic VLAN WEP encryption.");
				errors++;
			}
		}
	}

	return errors;
}

/**
 * hostapd_cleanup - Per-BSS cleanup (deinitialization)
 * @hapd: Pointer to BSS data
 *
 * This function is used to free all per-BSS data structures and resources.
 * This gets called in a loop for each BSS between calls to
 * hostapd_cleanup_iface_pre() and hostapd_cleanup_iface() when an interface
 * is deinitialized. Most of the modules that are initialized in
 * hostapd_setup_bss() are deinitialized here.
 */
static void hostapd_cleanup(struct hostapd_data *hapd)
{
	if (hapd->iface->ctrl_iface_deinit)
		hapd->iface->ctrl_iface_deinit(hapd);

	hostapd_scs_deinit(hapd->scs);
	hapd->scs = NULL;
	iapp_deinit(hapd->iapp);
	hapd->iapp = NULL;
	accounting_deinit(hapd);
	hostapd_deinit_wpa(hapd);
	vlan_deinit(hapd);
	hostapd_acl_deinit(hapd);
#ifndef CONFIG_NO_RADIUS
	radius_client_deinit(hapd->radius);
	hapd->radius = NULL;
#endif /* CONFIG_NO_RADIUS */

	hostapd_deinit_wps(hapd);

	authsrv_deinit(hapd);

	if (hapd->interface_added &&
	    hostapd_if_remove(hapd, WPA_IF_AP_BSS, hapd->conf->iface)) {
		wpa_printf(MSG_WARNING, "Failed to remove BSS interface %s",
			   hapd->conf->iface);
	}

	os_free(hapd->probereq_cb);
	hapd->probereq_cb = NULL;

#ifdef CONFIG_P2P
	wpabuf_free(hapd->p2p_beacon_ie);
	hapd->p2p_beacon_ie = NULL;
	wpabuf_free(hapd->p2p_probe_resp_ie);
	hapd->p2p_probe_resp_ie = NULL;
#endif /* CONFIG_P2P */
}


/**
 * hostapd_cleanup_iface_pre - Preliminary per-interface cleanup
 * @iface: Pointer to interface data
 *
 * This function is called before per-BSS data structures are deinitialized
 * with hostapd_cleanup().
 */
static void hostapd_cleanup_iface_pre(struct hostapd_iface *iface)
{
}


/**
 * hostapd_cleanup_iface - Complete per-interface cleanup
 * @iface: Pointer to interface data
 *
 * This function is called after per-BSS data structures are deinitialized
 * with hostapd_cleanup().
 */
static void hostapd_cleanup_iface(struct hostapd_iface *iface)
{
	hostapd_free_hw_features(iface->hw_features, iface->num_hw_features);
	iface->hw_features = NULL;
	os_free(iface->current_rates);
	iface->current_rates = NULL;
	ap_list_deinit(iface);
	hostapd_config_free(iface->conf);
	iface->conf = NULL;

	os_free(iface->config_fname);
	os_free(iface->bss);
	os_free(iface);
}


static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd)
{
	int i;

	hostapd_broadcast_wep_set(hapd);

	if (hapd->conf->ssid.wep.default_len) {
		hostapd_set_privacy(hapd, 1);
		return 0;
	}

	/*
	 * When IEEE 802.1X is not enabled, the driver may need to know how to
	 * set authentication algorithms for static WEP.
	 */
	hostapd_drv_set_authmode(hapd, hapd->conf->auth_algs);

	for (i = 0; i < 4; i++) {
		if (hapd->conf->ssid.wep.key[i] &&
		    hostapd_drv_set_key(iface, hapd, WPA_ALG_WEP, NULL, i,
					i == hapd->conf->ssid.wep.idx, NULL, 0,
					hapd->conf->ssid.wep.key[i],
					hapd->conf->ssid.wep.len[i])) {
			wpa_printf(MSG_WARNING, "Could not set WEP "
				   "encryption.");
			return -1;
		}
		if (hapd->conf->ssid.wep.key[i] &&
		    i == hapd->conf->ssid.wep.idx)
			hostapd_set_privacy(hapd, 1);
	}

	return 0;
}


static int hostapd_flush_old_stations(struct hostapd_data *hapd)
{
	int ret = 0;
	u8 addr[ETH_ALEN];

	if (hostapd_drv_none(hapd) || hapd->drv_priv == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "Flushing old station entries");
	if (hostapd_flush(hapd)) {
		wpa_printf(MSG_WARNING, "Could not connect to kernel driver.");
		ret = -1;
	}
	wpa_printf(MSG_DEBUG, "Deauthenticate all stations");
	os_memset(addr, 0xff, ETH_ALEN);
	hostapd_drv_sta_deauth(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	hostapd_free_stas(hapd);

	return ret;
}


/**
 * hostapd_validate_bssid_configuration - Validate BSSID configuration
 * @iface: Pointer to interface data
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to validate that the configured BSSIDs are valid.
 */
static int hostapd_validate_bssid_configuration(struct hostapd_iface *iface)
{
#ifdef CONFIG_BSSID_VALIDATION
	u8 mask[ETH_ALEN] = { 0 };
	struct hostapd_data *hapd = iface->bss[0];
	unsigned int i = iface->conf->num_bss, bits = 0, j;
	int res;
	int auto_addr = 0;

	if (hostapd_drv_none(hapd))
		return 0;

	/* Generate BSSID mask that is large enough to cover the BSSIDs. */

	/* Determine the bits necessary to cover the number of BSSIDs. */
	for (i--; i; i >>= 1)
		bits++;

	/* Determine the bits necessary to any configured BSSIDs,
	   if they are higher than the number of BSSIDs. */
	for (j = 0; j < iface->conf->num_bss; j++) {
		if (hostapd_mac_comp_empty(iface->conf->bss[j].bssid) == 0) {
			if (j)
				auto_addr++;
			continue;
		}

		for (i = 0; i < ETH_ALEN; i++) {
			mask[i] |=
				iface->conf->bss[j].bssid[i] ^
				hapd->own_addr[i];
		}
	}

	if (!auto_addr)
		goto skip_mask_ext;

	for (i = 0; i < ETH_ALEN && mask[i] == 0; i++)
		;
	j = 0;
	if (i < ETH_ALEN) {
		j = (5 - i) * 8;

		while (mask[i] != 0) {
			mask[i] >>= 1;
			j++;
		}
	}

	if (bits < j)
		bits = j;

	if (bits > 40) {
		wpa_printf(MSG_ERROR, "Too many bits in the BSSID mask (%u)",
			   bits);
		return -1;
	}

	os_memset(mask, 0xff, ETH_ALEN);
	j = bits / 8;
	for (i = 5; i > 5 - j; i--)
		mask[i] = 0;
	j = bits % 8;
	while (j--)
		mask[i] <<= 1;

skip_mask_ext:
	wpa_printf(MSG_DEBUG, "BSS count %lu, BSSID mask " MACSTR " (%d bits)",
		   (unsigned long) iface->conf->num_bss, MAC2STR(mask), bits);

	res = hostapd_valid_bss_mask(hapd, hapd->own_addr, mask);
	if (res == 0)
		return 0;

	if (res < 0) {
		wpa_printf(MSG_ERROR, "Driver did not accept BSSID mask "
			   MACSTR " for start address " MACSTR ".",
			   MAC2STR(mask), MAC2STR(hapd->own_addr));
		return -1;
	}

	if (!auto_addr)
		return 0;

	for (i = 0; i < ETH_ALEN; i++) {
		if ((hapd->own_addr[i] & mask[i]) != hapd->own_addr[i]) {
			wpa_printf(MSG_ERROR, "Invalid BSSID mask " MACSTR
				   " for start address " MACSTR ".",
				   MAC2STR(mask), MAC2STR(hapd->own_addr));
			wpa_printf(MSG_ERROR, "Start address must be the "
				   "first address in the block (i.e., addr "
				   "AND mask == addr).");
			return -1;
		}
	}

#endif
	return 0;
}


static int mac_in_conf(struct hostapd_config *conf, const void *a)
{
	size_t i;

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_mac_comp(conf->bss[i].bssid, a) == 0) {
			return 1;
		}
	}

	return 0;
}




/**
 * hostapd_setup_bss - Per-BSS setup (initialization)
 * @hapd: Pointer to BSS data
 * @first: Whether this BSS is the first BSS of an interface
 *
 * This function is used to initialize all per-BSS data structures and
 * resources. This gets called in a loop for each BSS when an interface is
 * initialized. Most of the modules that are initialized here will be
 * deinitialized in hostapd_cleanup().
 */
static int hostapd_setup_bss(struct hostapd_data *hapd, int first)
{
	struct hostapd_bss_config *conf = hapd->conf;
	u8 ssid[HOSTAPD_MAX_SSID_LEN + 1];
	int ssid_len, set_ssid;
	char force_ifname[IFNAMSIZ];
	u8 if_addr[ETH_ALEN] = {0};

	if (!first) {
		if (hostapd_mac_comp_empty(hapd->conf->bssid) == 0) {
			/* Don't generate one available BSSID, driver will take care of it. */
			os_memcpy(hapd->own_addr, hapd->conf->bssid, ETH_ALEN);
		} else {
			/* Allocate the configured BSSID. */
			os_memcpy(hapd->own_addr, hapd->conf->bssid, ETH_ALEN);

			if (hostapd_mac_comp(hapd->own_addr,
					     hapd->iface->bss[0]->own_addr) ==
			    0) {
				wpa_printf(MSG_ERROR, "BSS '%s' may not have "
					   "BSSID set to the MAC address of "
					   "the radio", hapd->conf->iface);
				return -1;
			}
		}

		hapd->interface_added = 1;
		if (hostapd_if_add(hapd->iface->bss[0], WPA_IF_AP_BSS,
				   hapd->conf->iface, hapd->own_addr, hapd,
				   &hapd->drv_priv, force_ifname, if_addr,
				   hapd->conf->bridge[0] ? hapd->conf->bridge :
				   NULL)) {
			wpa_printf(MSG_ERROR, "Failed to add BSS (BSSID="
				   MACSTR ")", MAC2STR(hapd->own_addr));
			return -1;
		}

		if (hostapd_mac_comp_empty(if_addr) != 0) {
			/* Driver returned the if address */
			os_memcpy(hapd->own_addr, if_addr, ETH_ALEN);
		}
	}

	hapd->ap_setup_locked_runtime = -1;

	if (conf->wmm_enabled < 0)
		conf->wmm_enabled = hapd->iconf->ieee80211n;

	hostapd_flush_old_stations(hapd);
	hostapd_set_privacy(hapd, 0);

	hostapd_broadcast_wep_clear(hapd);
	if (hostapd_setup_encryption(hapd->conf->iface, hapd))
		return -1;

	/*
	 * Fetch the SSID from the system and use it or,
	 * if one was specified in the config file, verify they
	 * match.
	 */
	ssid_len = hostapd_get_ssid(hapd, ssid, sizeof(ssid));
	if (ssid_len < 0) {
		wpa_printf(MSG_ERROR, "Could not read SSID from system");
		return -1;
	}
	if (conf->ssid.ssid_set) {
		/*
		 * If SSID is specified in the config file and it differs
		 * from what is being used then force installation of the
		 * new SSID.
		 */
		set_ssid = (conf->ssid.ssid_len != (size_t) ssid_len ||
			    os_memcmp(conf->ssid.ssid, ssid, ssid_len) != 0);
	} else {
		/*
		 * No SSID in the config file; just use the one we got
		 * from the system.
		 */
		set_ssid = 0;
		conf->ssid.ssid_len = ssid_len;
		os_memcpy(conf->ssid.ssid, ssid, conf->ssid.ssid_len);
		conf->ssid.ssid[conf->ssid.ssid_len] = '\0';
	}

	if (!hostapd_drv_none(hapd)) {
		wpa_printf(MSG_ERROR, "Using interface %s with hwaddr " MACSTR
			   " and ssid '%s'",
			   hapd->conf->iface, MAC2STR(hapd->own_addr),
			   hapd->conf->ssid.ssid);
	}

	if (hostapd_setup_wpa_psk(conf)) {
		wpa_printf(MSG_ERROR, "WPA-PSK setup failed.");
		return -1;
	}

	/* Set SSID for the kernel driver (to be used in beacon and probe
	 * response frames) */
	if (set_ssid && hostapd_set_ssid(hapd, (u8 *) conf->ssid.ssid,
					 conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		return -1;
	}

	if (conf->set_assoc_limit_required &&
			hostapd_set_bss_assoc_limit(hapd, conf->max_num_sta)) {
		wpa_printf(MSG_ERROR, "Could not set max_num_sta for kernel driver");
		return -1;
	}

	if (wpa_debug_level == MSG_MSGDUMP)
		conf->radius->msg_dumps = 1;
#ifndef CONFIG_NO_RADIUS
	hapd->radius = radius_client_init(hapd, conf->radius);
	if (hapd->radius == NULL) {
		wpa_printf(MSG_ERROR, "RADIUS client initialization failed.");
		return -1;
	}
#endif /* CONFIG_NO_RADIUS */

	if (hostapd_acl_init(hapd)) {
		wpa_printf(MSG_ERROR, "ACL initialization failed.");
		return -1;
	}
	if (hostapd_init_wps(hapd, conf))
		return -1;

	if (authsrv_init(hapd) < 0)
		return -1;

	if (ieee802_1x_init(hapd)) {
		wpa_printf(MSG_ERROR, "IEEE 802.1X initialization failed.");
		return -1;
	}

	if (hapd->conf->wpa && hostapd_setup_wpa(hapd))
		return -1;

	if (accounting_init(hapd)) {
		wpa_printf(MSG_ERROR, "Accounting initialization failed.");
		return -1;
	}

	if (hapd->conf->ieee802_11f &&
	    (hapd->iapp = iapp_init(hapd, hapd->conf->iapp_iface)) == NULL) {
		wpa_printf(MSG_ERROR, "IEEE 802.11F (IAPP) initialization "
			   "failed.");
		return -1;
	}

	if ((hapd->scs = hostapd_scs_init(hapd, hapd->conf->iface)) == NULL) {
		wpa_printf(MSG_ERROR, "SCS initialization "
			   "failed.");
		return -1;
	}

	if (hapd->iface->ctrl_iface_init &&
	    hapd->iface->ctrl_iface_init(hapd)) {
		wpa_printf(MSG_ERROR, "Failed to setup control interface");
		return -1;
	}

	if (!hostapd_drv_none(hapd) && vlan_init(hapd)) {
		wpa_printf(MSG_ERROR, "VLAN initialization failed.");
		return -1;
	}

	ieee802_11_set_beacon(hapd);
	/* Enable or disable SSID broadcast in beacons */
	if (hostapd_set_broadcast_ssid(hapd,
			!!hapd->conf->ignore_broadcast_ssid)) {
		wpa_printf(MSG_WARNING, "Could not modify broadcast SSID flag");
	}

	if (hapd->wpa_auth && wpa_init_keys(hapd->wpa_auth) < 0)
		return -1;

	if (hapd->driver && hapd->driver->set_operstate)
		hapd->driver->set_operstate(hapd->drv_priv, 1);

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


static void hostapd_tx_queue_params(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int i;
	struct hostapd_tx_queue_params *p;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		p = &iface->conf->tx_queue[i];

		if (hostapd_set_tx_queue_params(hapd, i, p->aifs, p->cwmin,
						p->cwmax, p->burst)) {
			wpa_printf(MSG_DEBUG, "Failed to set TX queue "
				   "parameters for queue %d.", i);
			/* Continue anyway */
		}
	}
}


static int setup_interface(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	size_t i;
	char country[4];

	/*
	 * Make sure that all BSSes get configured with a pointer to the same
	 * driver interface.
	 */
	for (i = 1; i < iface->num_bss; i++) {
		iface->bss[i]->driver = hapd->driver;
	}

	if (hostapd_validate_bssid_configuration(iface))
		return -1;

	if (hapd->iconf->country[0] && hapd->iconf->country[1]) {
		os_memcpy(country, hapd->iconf->country, 3);
		country[3] = '\0';
		if (hostapd_set_country(hapd, country) < 0) {
			wpa_printf(MSG_ERROR, "Failed to set country code");
			return -1;
		}
	}

	if (hostapd_get_hw_features(iface)) {
		/* Not all drivers support this yet, so continue without hw
		 * feature data. */
	} else {
		int ret = hostapd_select_hw_mode(iface);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "Could not select hw_mode and "
				   "channel. (%d)", ret);
			return -1;
		}
		ret = hostapd_check_ht_capab(iface);
		if (ret < 0)
			return -1;
		if (ret == 1) {
			wpa_printf(MSG_DEBUG, "Interface initialization will "
				   "be completed in a callback");
			return 0;
		}
	}
	/* Mark first bss as primary */
	hapd->primary_interface = 1;
	iface->default_bss_for_pbc = hapd;

	return hostapd_setup_interface_complete(iface, 0);
}


int hostapd_setup_interface_complete(struct hostapd_iface *iface, int err)
{
	struct hostapd_data *hapd = iface->bss[0];
	size_t j;
	u8 *prev_addr;

	if (err) {
		wpa_printf(MSG_ERROR, "Interface initialization failed");
		eloop_terminate();
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Completing interface initialization");
	if (hapd->iconf->channel) {
		iface->freq = hostapd_hw_get_freq(hapd, hapd->iconf->channel);
		wpa_printf(MSG_DEBUG, "Mode: %s  Channel: %d  "
			   "Frequency: %d MHz",
			   hostapd_hw_mode_txt(hapd->iconf->hw_mode),
			   hapd->iconf->channel, iface->freq);

		if (hostapd_set_freq(hapd, hapd->iconf->hw_mode, iface->freq,
				     hapd->iconf->channel,
				     hapd->iconf->ieee80211n,
				     hapd->iconf->secondary_channel)) {
			wpa_printf(MSG_ERROR, "Could not set channel for "
				   "kernel driver");
			return -1;
		}
	}

	if (iface->current_mode) {
		if (hostapd_prepare_rates(hapd, iface->current_mode)) {
			wpa_printf(MSG_ERROR, "Failed to prepare rates "
				   "table.");
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_WARNING,
				       "Failed to prepare rates table.");
			return -1;
		}
	}

	if (hapd->iconf->rts_threshold > -1 &&
	    hostapd_set_rts(hapd, hapd->iconf->rts_threshold)) {
		wpa_printf(MSG_ERROR, "Could not set RTS threshold for "
			   "kernel driver");
		return -1;
	}

	if (hapd->iconf->fragm_threshold > -1 &&
	    hostapd_set_frag(hapd, hapd->iconf->fragm_threshold)) {
		wpa_printf(MSG_ERROR, "Could not set fragmentation threshold "
			   "for kernel driver");
		return -1;
	}

	if (hapd->iconf->total_assoc_limit >= 0 &&
			hapd->iconf->total_assoc_limit <= MAX_STA_COUNT) {
		if (hostapd_set_total_assoc_limit(hapd, hapd->iconf->total_assoc_limit)) {
			wpa_printf(MSG_ERROR, "Could not set total_assoc_limit "
				   "for kernel driver");
			return -1;
		}
	}

	prev_addr = hapd->own_addr;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (j)
			os_memcpy(hapd->own_addr, prev_addr, ETH_ALEN);
		if (hostapd_setup_bss(hapd, j == 0))
			return -1;
		if (hostapd_mac_comp_empty(hapd->conf->bssid) == 0)
			prev_addr = hapd->own_addr;
	}

	hostapd_tx_queue_params(iface);

	ap_list_init(iface);

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (hostapd_driver_commit(hapd) < 0) {
			wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
				   "configuration", __func__);
			return -1;
		}
	}

	/*
	 * WPS UPnP module can be initialized only when the "upnp_iface" is up.
	 * If "interface" and "upnp_iface" are the same (e.g., non-bridge
	 * mode), the interface is up only after driver_commit, so initialize
	 * WPS after driver_commit.
	 */
	for (j = 0; j < iface->num_bss; j++) {
		if (hostapd_init_wps_complete(iface->bss[j]))
			return -1;
	}

	if (hapd->setup_complete_cb)
		hapd->setup_complete_cb(hapd->setup_complete_cb_ctx);

	wpa_printf(MSG_DEBUG, "%s: Setup of interface done.",
		   iface->bss[0]->conf->iface);

	return 0;
}


/**
 * hostapd_setup_interface - Setup of an interface
 * @iface: Pointer to interface data.
 * Returns: 0 on success, -1 on failure
 *
 * Initializes the driver interface, validates the configuration,
 * and sets driver parameters based on the configuration.
 * Flushes old stations, sets the channel, encryption,
 * beacons, and WDS links based on the configuration.
 */
int hostapd_setup_interface(struct hostapd_iface *iface)
{
	int ret;

	ret = setup_interface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "%s: Unable to setup interface.",
			   iface->bss[0]->conf->iface);
		return -1;
	}

	return 0;
}


/**
 * hostapd_alloc_bss_data - Allocate and initialize per-BSS data
 * @hapd_iface: Pointer to interface data
 * @conf: Pointer to per-interface configuration
 * @bss: Pointer to per-BSS configuration for this BSS
 * Returns: Pointer to allocated BSS data
 *
 * This function is used to allocate per-BSS data structure. This data will be
 * freed after hostapd_cleanup() is called for it during interface
 * deinitialization.
 */
struct hostapd_data *
hostapd_alloc_bss_data(struct hostapd_iface *hapd_iface,
		       struct hostapd_config *conf,
		       struct hostapd_bss_config *bss)
{
	struct hostapd_data *hapd;

	hapd = os_zalloc(sizeof(*hapd));
	if (hapd == NULL)
		return NULL;

	hapd->new_assoc_sta_cb = hostapd_new_assoc_sta;
	hapd->iconf = conf;
	hapd->conf = bss;
	hapd->iface = hapd_iface;
	hapd->driver = hapd->iconf->driver;

	return hapd;
}


void hostapd_interface_deinit(struct hostapd_iface *iface)
{
	size_t j;

	if (iface == NULL)
		return;

	hostapd_cleanup_iface_pre(iface);
	for (j = 0; j < iface->num_bss; j++) {
		struct hostapd_data *hapd = iface->bss[j];
		hostapd_free_stas(hapd);
		hostapd_flush_old_stations(hapd);
		hostapd_cleanup(hapd);
	}
}


void hostapd_interface_free(struct hostapd_iface *iface)
{
	size_t j;
	for (j = 0; j < iface->num_bss; j++)
		os_free(iface->bss[j]);
	hostapd_cleanup_iface(iface);
}


/**
 * hostapd_new_assoc_sta - Notify that a new station associated with the AP
 * @hapd: Pointer to BSS data
 * @sta: Pointer to the associated STA data
 * @reassoc: 1 to indicate this was a re-association; 0 = first association
 *
 * This function will be called whenever a station associates with the AP. It
 * can be called from ieee802_11.c for drivers that export MLME to hostapd and
 * from drv_callbacks.c based on driver events for drivers that take care of
 * management frames (IEEE 802.11 authentication and association) internally.
 */
void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc)
{
	if (hapd->tkip_countermeasures) {
		hostapd_drv_sta_deauth(hapd, sta->addr,
				       WLAN_REASON_MICHAEL_MIC_FAILURE);
		return;
	}

	hostapd_prune_associations(hapd, sta->addr);

	/* IEEE 802.11F (IAPP) */
	if (hapd->conf->ieee802_11f)
		iapp_new_station(hapd->iapp, sta);

#ifdef CONFIG_P2P
	if (sta->p2p_ie == NULL && !sta->no_p2p_set) {
		sta->no_p2p_set = 1;
		hapd->num_sta_no_p2p++;
		if (hapd->num_sta_no_p2p == 1)
			hostapd_p2p_non_p2p_sta_connected(hapd);
	}
#endif /* CONFIG_P2P */

	/* Start accounting here, if IEEE 802.1X and WPA are not used.
	 * IEEE 802.1X/WPA code will start accounting after the station has
	 * been authorized. */
	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa)
		accounting_sta_start(hapd, sta);

	/* Start IEEE 802.1X authentication process for new stations */
	ieee802_1x_new_station(hapd, sta);
	if (reassoc) {
		if (sta->auth_alg != WLAN_AUTH_FT &&
		    !(sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS)))
			wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH);
		else if (sta->auth_alg != WLAN_AUTH_FT &&
			 (sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS)))
			wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH_EAPOL);
	} else
		wpa_auth_sta_associated(hapd->wpa_auth, sta->wpa_sm);
}


#define SCS_BRCM_RECV_IFACE                  "br0"
#define SCS_BRCM_UDP_PORT                    49300
#define SCS_BRCM_BCAST_INTVL                  4
#define SCS_BRCM_PKT_TYPE_STA_BCAST           0
#define SCS_BRCM_PKT_TYPE_STA_UNDISCLOSED     1
#define SCS_BRCM_PKT_TYPE_STA_INTF            2
#define SCS_BRCM_PKT_TYPE_AP_BCAST            3
#define SCS_BRCM_PKT_IP_MIN_LEN               (sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct brcm_info_hdr))

struct scs_data {
	struct hostapd_data *hapd;
	int master;
	char iface_name[IFNAMSIZ];
	struct in_addr own, bcast;
	int packet_sock;
};

struct brcm_info_hdr {
	uint32_t type;
	uint32_t len;
};

/* unpacked format */
struct brcm_info_client_intf {
	uint32_t alert_level;
	unsigned char sta_mac[ETH_ALEN];
	int sta_rssi;
	unsigned rxglitch;
};

uint16_t  network_checksum(void *addr, int count)
{
	/*
	 * Compute Internet Checksum for "count" bytes
	 * beginning at location "addr".
	 */
	int32_t sum = 0;
	uint16_t *source = (uint16_t *) addr;

	while (count > 1)  {
		/* This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if (count > 0) {
		/*
		 * Make sure that the left-over byte is added correctly both
		 * with little and big endian hosts
		 */
		uint16_t tmp = 0;
		*(uint8_t*)&tmp = *(uint8_t*)source;
		sum += tmp;
	}

	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

int hostapd_scs_update_anounce_pkt(struct hostapd_data *hapd, struct scs_data *scs, int force_update_pkt)
{
	struct ifreq ifr;
	struct hostapd_iface *hapd_iface = hapd->iface;
	int udp_sock = hapd_iface->scs_brcm_sock;
	struct sockaddr_in *paddr;
	int changed = 0;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, SCS_BRCM_RECV_IFACE, sizeof(ifr.ifr_name));

	if (ioctl(udp_sock, SIOCGIFADDR, &ifr) != 0) {
		perror("ioctl(SIOCGIFADDR)");
		return 0;
	}
	paddr = (struct sockaddr_in *) &ifr.ifr_addr;
	if (paddr->sin_family != AF_INET) {
		printf("Invalid address family %i (SIOCGIFADDR)\n",
		       paddr->sin_family);
		return 0;
	}
	if (scs->own.s_addr != paddr->sin_addr.s_addr) {
		scs->own.s_addr = paddr->sin_addr.s_addr;
		changed = 1;
	}

	if (ioctl(udp_sock, SIOCGIFBRDADDR, &ifr) != 0) {
		perror("ioctl(SIOCGIFBRDADDR)");
		return 0;
	}
	paddr = (struct sockaddr_in *) &ifr.ifr_addr;
	if (paddr->sin_family != AF_INET) {
		printf("Invalid address family %i (SIOCGIFBRDADDR)\n",
		       paddr->sin_family);
		return 0;
	}
	if (scs->bcast.s_addr != paddr->sin_addr.s_addr) {
		scs->bcast.s_addr = paddr->sin_addr.s_addr;
		changed = 1;
	}

	if (changed || force_update_pkt) {
		struct ether_header *ethh;
		struct iphdr *iph;
		struct udphdr *uh;
		struct brcm_info_hdr *ah;
		/* prepare the ap bcast packet */
		memset(hapd_iface->scs_brcm_pkt_ap_bcast, 0x0, sizeof(hapd_iface->scs_brcm_pkt_ap_bcast));
		ethh = (struct ether_header *)hapd_iface->scs_brcm_pkt_ap_bcast;
		memset(ethh->ether_dhost, 0xff, ETH_ALEN);
		memcpy(ethh->ether_shost, hapd_iface->scs_brcm_rxif_mac, ETH_ALEN);
		ethh->ether_type = htons(0x0800);
		/* prepare udp and part of ip to calc udp checksum */
		iph = (struct iphdr *)(ethh + 1);
		uh = (struct udphdr *)(iph + 1);
		ah = (struct brcm_info_hdr *)(uh + 1);
		ah->type = host_to_le32(SCS_BRCM_PKT_TYPE_AP_BCAST);
		ah->len = host_to_le32(sizeof(struct brcm_info_hdr));
		iph->protocol = IPPROTO_UDP;
		iph->saddr = scs->own.s_addr;
		iph->daddr = 0xFFFFFFFF;
		uh->source = htons(SCS_BRCM_UDP_PORT);
		uh->dest = htons(SCS_BRCM_UDP_PORT);
		uh->len = htons((sizeof(struct udphdr) + sizeof(struct brcm_info_hdr)));
		iph->tot_len = uh->len;
		uh->check = network_checksum(iph, SCS_BRCM_PKT_IP_MIN_LEN);
		/* fill rest of ip and calc ip checksum */
		iph->tot_len = htons(SCS_BRCM_PKT_IP_MIN_LEN);
		iph->ihl = sizeof(struct iphdr) >> 2;
		iph->version = IPVERSION;
		iph->ttl = IPDEFTTL;
		iph->check = network_checksum(iph, sizeof(struct iphdr));
	}

	return 1;
}

static void hostapd_scs_brcm_bcast(struct hostapd_data *hapd)
{
	struct hostapd_iface *hapd_iface = hapd->iface;
	int udp_sock = hapd_iface->scs_brcm_sock;
	struct scs_data *scs = hapd->scs;

	char buf[sizeof(struct brcm_info_hdr)];
	struct brcm_info_hdr *hdr;
	struct sockaddr_in addr;

	if (scs->master && !hostapd_scs_update_anounce_pkt(hapd, scs, 0)) {
		perror("fail to update anounce packet");
		return;
		/* try later */
	}

	hostapd_logger(scs->hapd, NULL, HOSTAPD_MODULE_SCS,
		       HOSTAPD_LEVEL_DEBUG,
		       "Send brcm bcast to %s\n",
		       scs->iface_name);
	if (hapd->driver && hapd->driver->set_brcm_ioctl) {
		struct ieee80211req_brcm brcm;
		memset(&brcm, 0x0, sizeof(struct ieee80211req_brcm));
		brcm.ib_op = IEEE80211REQ_BRCM_PKT;
		brcm.ib_pkt = hapd_iface->scs_brcm_pkt_ap_bcast;
		brcm.ib_pkt_len = SCS_BRCM_PKT_IP_MIN_LEN + sizeof(struct ether_header);
		hapd->driver->set_brcm_ioctl(hapd->drv_priv, &brcm, sizeof(struct ieee80211req_brcm));
	}
}

static void hostapd_scs_receive_udp(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct scs_data *scs = eloop_ctx;
	struct hostapd_data *hapd = scs->hapd;
	struct hostapd_iface *hapd_iface = hapd->iface;
	int udp_sock = hapd_iface->scs_brcm_sock;
	int len;
	unsigned char buf[128];
	struct sockaddr_in from;
	socklen_t fromlen;
	struct brcm_info_hdr *hdr;
	struct brcm_info_client_intf *info;
	int i;

	fromlen = sizeof(from);
	len = recvfrom(udp_sock, buf, sizeof(buf), 0,
		       (struct sockaddr *) &from, &fromlen);
	if (len < 0) {
		perror("recvfrom");
		return;
	}

	if (from.sin_addr.s_addr == scs->own.s_addr)
		return;

	hostapd_logger(scs->hapd, NULL, HOSTAPD_MODULE_SCS,
		       HOSTAPD_LEVEL_DEBUG,
		       "Received %d byte IAPP frame from %s\n",
		       len, inet_ntoa(from.sin_addr));

	if (len < (int) sizeof(*hdr))
		return;

	hdr = (struct brcm_info_hdr *) buf;
	hostapd_logger(scs->hapd, NULL, HOSTAPD_MODULE_SCS,
		       HOSTAPD_LEVEL_DEBUG,
		       "RX: type=%d len=%d\n",
		       hdr->type, hdr->len);

	switch (hdr->type) {
	case SCS_BRCM_PKT_TYPE_STA_BCAST:
		/* hard to know sta's mac, so broadcast in all bss */
		for (i = 0; i < hapd_iface->num_bss; i++) {
			hostapd_scs_brcm_bcast(hapd_iface->bss[i]);
		}
		break;
	case SCS_BRCM_PKT_TYPE_STA_INTF:
		info = (struct brcm_info_client_intf*)(hdr + 1);
		hostapd_logger(scs->hapd, info->sta_mac, HOSTAPD_MODULE_SCS,
		       HOSTAPD_LEVEL_DEBUG,
		       "STA BRCM INTF: rssi=%d rxglitch=%d\n",
		       info->sta_rssi, info->rxglitch);

		if (hapd->driver && hapd->driver->set_brcm_ioctl) {
			struct ieee80211req_brcm brcm;
			memset(&brcm, 0x0, sizeof(struct ieee80211req_brcm));
			brcm.ib_op = IEEE80211REQ_BRCM_INFO;
			brcm.ib_rssi = info->sta_rssi;
			brcm.ib_rxglitch = info->rxglitch;
			memcpy(brcm.ib_macaddr, info->sta_mac, IEEE80211_ADDR_LEN);
			hapd->driver->set_brcm_ioctl(hapd->drv_priv, &brcm, sizeof(struct ieee80211req_brcm));
		}
		break;
	default:
		break;
	}
}

struct scs_data * hostapd_scs_init(struct hostapd_data *hapd, const char *iface)
{
	struct hostapd_iface *hif = hapd->iface;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	int ifindex;
	struct sockaddr_in *paddr, uaddr;
	struct scs_data *scs;
	struct ip_mreqn mreq;
	int on;
	int udp_sock;
	int reuse = 1;

	scs = os_zalloc(sizeof(*scs));
	if (scs == NULL)
		return NULL;
	scs->hapd = hapd;
	scs->packet_sock = -1;
	strcpy(scs->iface_name, iface);

	if (hif->scs_brcm_sock < 0) {
		udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (udp_sock < 0) {
			perror("socket[PF_INET,SOCK_DGRAM]");
			hostapd_scs_deinit(scs);
			return NULL;
		}
		hif->scs_brcm_sock = udp_sock;
		scs->master = 1;

		os_memset(&ifr, 0, sizeof(ifr));
		os_strlcpy(ifr.ifr_name, SCS_BRCM_RECV_IFACE, sizeof(ifr.ifr_name));
		if (ioctl(udp_sock, SIOCGIFHWADDR, &ifr) != 0) {
			perror("ioctl(SIOCGIFHWADDR)");
			hostapd_scs_deinit(scs);
			return NULL;
		}
		memcpy(hif->scs_brcm_rxif_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

		if (!hostapd_scs_update_anounce_pkt(hapd, scs, 1)) {
			perror("fail to update anounce pkt");
			/* try later */
		}

		if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			perror("setsockopt[SOCKET,SO_REUSEADDR]");
			hostapd_scs_deinit(scs);
			return NULL;
		}

		os_memset(&uaddr, 0, sizeof(uaddr));
		uaddr.sin_family = AF_INET;
		uaddr.sin_port = htons(SCS_BRCM_UDP_PORT);
		if (bind(udp_sock, (struct sockaddr *) &uaddr,
			 sizeof(uaddr)) < 0) {
			perror("bind[UDP]");
			hostapd_scs_deinit(scs);
			return NULL;
		}

		if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
			perror("setsockopt[SOCKET,SO_BROADCAST]");
			hostapd_scs_deinit(scs);
			return NULL;
		}

		if (eloop_register_read_sock(udp_sock, hostapd_scs_receive_udp,
				     scs, NULL)) {
			perror("Could not register read socket for SCS BRCM");
			hostapd_scs_deinit(scs);
			return NULL;
		}
	}

	return scs;
}

void hostapd_scs_deinit(struct scs_data *scs)
{
	struct hostapd_data *hapd;
	struct hostapd_iface *hif;

	if (scs == NULL)
		return;

	hapd = scs->hapd;
	hif = hapd->iface;

	if (hif->scs_brcm_sock >= 0) {
		eloop_unregister_read_sock(hif->scs_brcm_sock);
		close(hif->scs_brcm_sock);
		hif->scs_brcm_sock = -1;
	}
	if (hif->scs_ioctl_sock >= 0) {
		close(hif->scs_ioctl_sock);
		hif->scs_ioctl_sock = -1;
	}
	if (scs->packet_sock >= 0) {
		eloop_unregister_read_sock(scs->packet_sock);
		close(scs->packet_sock);
	}

	eloop_cancel_timeout(hostapd_scs_brcm_bcast, scs->hapd, NULL);

	os_free(scs);
}

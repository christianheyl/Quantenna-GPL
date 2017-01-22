/*
 * wpa_supplicant - Temporary BSSID blacklist
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
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

#ifndef BLACKLIST_H
#define BLACKLIST_H

/* Default values */
#define WPA_BLACKLIST_FAIL_MAX_DEFAULT	5	/* Number of failures before BSS is blacklisted */
#define WPA_BLACKLIST_FAIL_CFG_MAX	20	/* Maximum allowed value for failure setting */
#define WPA_BLACKLIST_TIMEOUT_DEFAULT	60	/* Number of seconds a BSS is blacklisted */
#define WPA_BLACKLIST_TIMEOUT_CFG_MAX	300	/* Maximum allowed timeout value */

#define WPA_BLACKLIST_TIMER	10

struct wpa_blacklist {
	struct wpa_blacklist *next;
	u8 bssid[ETH_ALEN];
	int count;
	int timeout;
};

struct wpa_blacklist * wpa_blacklist_get(struct wpa_supplicant *wpa_s,
					 const u8 *bssid);
int wpa_blacklist_add(struct wpa_supplicant *wpa_s, const u8 *bssid);
int wpa_blacklist_del(struct wpa_supplicant *wpa_s, const u8 *bssid);
void wpa_blacklist_clear(struct wpa_supplicant *wpa_s);
void wpa_blacklist_set_default_config(struct wpa_supplicant *wpa_s);
int wpa_blacklist_set_fail_max(struct wpa_supplicant *wpa_s, u32 count);
int wpa_blacklist_set_timeout(struct wpa_supplicant *wpa_s, u32 timeout);
void wpa_blacklist_timer(void *eloop_ctx, void *timer_ctx);

#endif /* BLACKLIST_H */

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

#include "includes.h"

#include "common.h"
#include "wpa_supplicant_i.h"
#include "blacklist.h"
#include "eloop.h"
#include "config.h"

/**
 * wpa_blacklist_get - Get the blacklist entry for a BSSID
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID
 * Returns: Matching blacklist entry for the BSSID or %NULL if not found
 */
struct wpa_blacklist * wpa_blacklist_get(struct wpa_supplicant *wpa_s,
					 const u8 *bssid)
{
	struct wpa_blacklist *e;

	e = wpa_s->blacklist;
	while (e) {
		if (os_memcmp(e->bssid, bssid, ETH_ALEN) == 0)
			return e;
		e = e->next;
	}

	return NULL;
}


/**
 * wpa_blacklist_add - Add an BSSID to the blacklist
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to be added to the blacklist
 * Returns: Current blacklist count on success, -1 on failure
 *
 * This function adds the specified BSSID to the blacklist or increases the
 * blacklist count if the BSSID was already listed. It should be called when
 * an association attempt fails either due to the selected BSS rejecting
 * association or due to timeout.
 *
 * This blacklist is used to force %wpa_supplicant to go through all available
 * BSSes before retrying to associate with an BSS that rejected or timed out
 * association. It does not prevent the listed BSS from being used; it only
 * changes the order in which they are tried.
 */
int wpa_blacklist_add(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_blacklist *e;

	e = wpa_blacklist_get(wpa_s, bssid);
	if (e) {
		e->count++;
		if (e->count >= wpa_s->blacklist_fail_max) {
			if (wpa_s->blacklist_timer == 0) {
				eloop_register_timeout(WPA_BLACKLIST_TIMER,
						       0, wpa_blacklist_timer,
						       wpa_s, NULL);
				wpa_s->blacklist_timer = 1;
				wpa_msg(wpa_s, MSG_DEBUG, "Blacklist timer started");
			}
			e->timeout = wpa_s->blacklist_timeout;
			wpa_msg(wpa_s, MSG_INFO, "BSSID " MACSTR " blacklist timeout set to %d",
			       MAC2STR(bssid), e->timeout);
		}
		wpa_msg(wpa_s, MSG_DEBUG, "BSSID " MACSTR " blacklist count "
			   "incremented to %d",
			   MAC2STR(bssid), e->count);
		return e->count;
	}

	e = os_zalloc(sizeof(*e));
	if (e == NULL)
		return -1;
	os_memcpy(e->bssid, bssid, ETH_ALEN);
	e->count = 1;
	e->timeout = 0;
	e->next = wpa_s->blacklist;
	wpa_s->blacklist = e;
	wpa_printf(MSG_DEBUG, "Added BSSID " MACSTR " into blacklist",
		   MAC2STR(bssid));

	return e->count;
}


/**
 * wpa_blacklist_del - Remove an BSSID from the blacklist
 * @wpa_s: Pointer to wpa_supplicant data
 * @bssid: BSSID to be removed from the blacklist
 * Returns: 0 on success, -1 on failure
 */
int wpa_blacklist_del(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_blacklist *e, *prev = NULL;

	e = wpa_s->blacklist;
	while (e) {
		if (os_memcmp(e->bssid, bssid, ETH_ALEN) == 0) {
			if (prev == NULL) {
				wpa_s->blacklist = e->next;
			} else {
				prev->next = e->next;
			}
			wpa_msg(wpa_s, MSG_INFO, "BSSID " MACSTR " removed from "
				   "blacklist (delete)", MAC2STR(bssid));
			os_free(e);

			if (wpa_s->blacklist == NULL) {
				eloop_cancel_timeout(wpa_blacklist_timer,
							wpa_s, NULL);
				wpa_s->blacklist_timer = 0;
			}
			return 0;
		}
		prev = e;
		e = e->next;
	}
	return -1;
}


/**
 * wpa_blacklist_clear - Clear the blacklist of all entries
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_blacklist_clear(struct wpa_supplicant *wpa_s)
{
	struct wpa_blacklist *e, *prev;

	e = wpa_s->blacklist;
	wpa_s->blacklist = NULL;
	while (e) {
		prev = e;
		e = e->next;
		wpa_msg(wpa_s, MSG_INFO, "BSSID " MACSTR " removed from "
			   "blacklist (clear)", MAC2STR(prev->bssid));
		os_free(prev);
	}

	eloop_cancel_timeout(wpa_blacklist_timer, wpa_s, NULL);
	wpa_s->blacklist_timer = 0;
}

/*
 * wpa_blacklist_config - Configure blacklist timeout and max failures
 * @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_blacklist_set_default_config(struct wpa_supplicant *wpa_s)
{
	wpa_s->blacklist_timeout = WPA_BLACKLIST_TIMEOUT_DEFAULT;
	wpa_s->blacklist_fail_max = WPA_BLACKLIST_FAIL_MAX_DEFAULT;
	wpa_s->blacklist_timer = 0;

	wpa_printf(MSG_DEBUG, "Set blacklist max fail value to %d, "
			"black timeout to %d",
			wpa_s->blacklist_fail_max,
			wpa_s->blacklist_timeout);
}

/*
 * wpa_blacklist_set_fail_max - Configure the maximum failures before a
 *     BSS is blacklisted
 * @wpa_s: Pointer to wpa_supplicant data
 * @count: new maximum failures value
 * Returns: 0 on success, -1 on failure
 */
int wpa_blacklist_set_fail_max(struct wpa_supplicant *wpa_s, u32 count)
{
	if (!wpa_s) {
		return -1;
	}

	if ((count < 1) || (count > WPA_BLACKLIST_FAIL_CFG_MAX)) {
		wpa_msg(wpa_s, MSG_ERROR, "Blacklist failure setting out of range");
		return -1;
	}
	wpa_s->blacklist_fail_max = count;
	wpa_msg(wpa_s, MSG_INFO, "Blacklist failure max set to %d",
		count);
	return 0;
}

/*
 * wpa_blacklist_set_timeout - Configure the number of seconds before
 *        blacklisted BSS is retried
 * @wpa_s: Pointer to wpa_supplicant data
 * @timeout: new timeout value in seconds before the BSS is retried.
 * Returns: 0 on success, -1 on failure
 */
int wpa_blacklist_set_timeout(struct wpa_supplicant *wpa_s, u32 timeout)
{
	if (!wpa_s) {
		return -1;
	}

	if ((timeout < WPA_BLACKLIST_TIMER) || (timeout > WPA_BLACKLIST_TIMEOUT_CFG_MAX)) {
		wpa_msg(wpa_s, MSG_ERROR, "Blacklist timeout setting out of range");
		return -1;
	}
	wpa_s->blacklist_timeout = timeout;
	wpa_msg(wpa_s, MSG_INFO, "Blacklist timeout set to %d",
		timeout);
	return 0;
}

/**
 *  wpa_blacklist_timer - Check the blacklist entries, decrement any
 *  active timers
 *  @wpa_s: Pointer to wpa_supplicant data
 */
void wpa_blacklist_timer(void *eloop_ctx, void *timer_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_blacklist *e;

	e = wpa_s->blacklist;
	while (e) {
		if (e->timeout != 0) {
			e->timeout -= WPA_BLACKLIST_TIMER;
			if (e->timeout <= 0) {
				wpa_msg(wpa_s, MSG_DEBUG, "BSSID " MACSTR
					" blacklist timeout expired",
				        MAC2STR(e->bssid));
				/*
				 * Don't remove the entry, allow one more attempt
				 * before blacklisting the BSS again.
				 */
				e->timeout = 0;
				e->count = wpa_s->blacklist_fail_max - 1;
			} else {
				wpa_msg(wpa_s, MSG_DEBUG, "BSSID " MACSTR
					" blacklist timeout %d",
					MAC2STR(e->bssid), e->timeout);
			}
		}
		e = e->next;
	}

	/* Restart the timer */
	if (wpa_s->blacklist) {
		eloop_register_timeout(WPA_BLACKLIST_TIMER, 0, wpa_blacklist_timer,
					wpa_s, NULL);
	}
}

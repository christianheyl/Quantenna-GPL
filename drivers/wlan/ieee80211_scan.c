/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ieee80211_scan.c 1849 2006-12-08 17:20:08Z proski $
 */
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 scanning support.
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <qtn/qtn_debug.h>
#include <qtn/shared_defs.h>
#include <qtn/shared_params.h>
#include "net80211/if_media.h"

#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_scan.h"

struct scan_state {
	struct ieee80211_scan_state base;	/* public state */

	u_int ss_iflags;				/* flags used internally */
#define	ISCAN_MINDWELL 	0x0001		/* min dwell time reached */
#define	ISCAN_DISCARD	0x0002		/* discard rx'd frames */
#define	ISCAN_CANCEL	0x0004		/* cancel current scan */
#define	ISCAN_START	0x0008		/* 1st time through next_scan */
	unsigned long ss_chanmindwell;		/* min dwell on curchan */
	unsigned long ss_scanend;		/* time scan must stop */
	u_int ss_duration;			/* duration for next scan */
	struct tasklet_struct ss_pwrsav;	/* sta ps ena tasklet */
	struct timer_list ss_scan_timer;	/* scan timer */
	struct timer_list ss_probe_timer;	/* start sending probe requests timer */
};
#define	SCAN_PRIVATE(ss)	((struct scan_state *) ss)

/*
 * Amount of time to go off-channel during a background
 * scan.  This value should be large enough to catch most
 * ap's but short enough that we can return on-channel
 * before our listen interval expires.
 *
 * XXX tunable
 * XXX check against configured listen interval
 */
#define	IEEE80211_SCAN_OFFCHANNEL	msecs_to_jiffies(150)

/*
 * Roaming-related defaults.  RSSI thresholds are as returned by the
 * driver (dBm).  Transmit rate thresholds are IEEE rate codes (i.e
 * .5M units).
 */
#define	SCAN_VALID_DEFAULT		60	/* scan cache valid age (secs) */
#define	ROAM_RSSI_11A_DEFAULT		24	/* rssi threshold for 11a bss */
#define	ROAM_RSSI_11B_DEFAULT		24	/* rssi threshold for 11b bss */
#define	ROAM_RSSI_11BONLY_DEFAULT	24	/* rssi threshold for 11b-only bss */
#define	ROAM_RATE_11A_DEFAULT		2*24	/* tx rate threshold for 11a bss */
#define	ROAM_RATE_11B_DEFAULT		2*9	/* tx rate threshold for 11b bss */
#define	ROAM_RATE_11BONLY_DEFAULT	2*5	/* tx rate threshold for 11b-only bss */

static u_int32_t txpow_rxgain_count = 0;
static u_int32_t txpow_rxgain_state = 1;

static void scan_restart_pwrsav(unsigned long);
static void scan_next(unsigned long);
static void send_probes(unsigned long);
static void scan_saveie(u_int8_t **iep, const u_int8_t *ie);

#ifdef QSCS_ENABLED
int ieee80211_scs_init_ranking_stats(struct ieee80211com *ic)
{
	struct ap_state *as;

	MALLOC(as, struct ap_state *, sizeof(struct ap_state),
		M_SCANCACHE, M_NOWAIT | M_ZERO);
	if (as == NULL) {
		printk("Failed to alloc scs ranking stats\n");
		return -1;
	}

	if (ic->ic_scan != NULL) {
		ic->ic_scan->ss_scs_priv = as;
	} else {
		FREE(as, M_SCANCACHE);
		return -1;
	}

	ieee80211_scs_clean_stats(ic, IEEE80211_SCS_STATE_RESET, 0);

	return 0;
}

void ieee80211_scs_deinit_ranking_stats(struct ieee80211com *ic)
{
	struct ap_state *as = ic->ic_scan->ss_scs_priv;

	if (as != NULL) {
		FREE(as, M_SCANCACHE);
	}

	ic->ic_scan->ss_scs_priv = NULL;
}
#endif

void
ieee80211_scan_attach(struct ieee80211com *ic)
{
	struct scan_state *ss;

	ic->ic_roaming = IEEE80211_ROAMING_AUTO;

	MALLOC(ss, struct scan_state *, sizeof(struct scan_state),
		M_80211_SCAN, M_NOWAIT | M_ZERO);
	if (ss != NULL) {
		init_timer(&ss->ss_scan_timer);
		ss->ss_scan_timer.function = scan_next;
		ss->ss_scan_timer.data = (unsigned long) ss;
		/* Init the send probe timer for active scans */
		init_timer(&ss->ss_probe_timer);
		ss->ss_probe_timer.function = send_probes;
		ss->ss_probe_timer.data = (unsigned long) ss;
		tasklet_init(&ss->ss_pwrsav, scan_restart_pwrsav,
			(unsigned long) ss);
		ss->base.ss_pick_flags = IEEE80211_PICK_DEFAULT;
		ss->base.is_scan_valid = 0;
		ic->ic_scan = &ss->base;
	} else
		ic->ic_scan = NULL;

#ifdef QSCS_ENABLED
	ieee80211_scs_init_ranking_stats(ic);
#endif
}

void
ieee80211_scan_detach(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss != NULL) {
#ifdef QSCS_ENABLED
		ieee80211_scs_deinit_ranking_stats(ic);
#endif
		del_timer(&SCAN_PRIVATE(ss)->ss_scan_timer);
		del_timer(&SCAN_PRIVATE(ss)->ss_probe_timer);
		tasklet_kill(&SCAN_PRIVATE(ss)->ss_pwrsav);
		if (ss->ss_ops != NULL) {
			ss->ss_ops->scan_detach(ss);
			ss->ss_ops = NULL;
		}
		ic->ic_flags &= ~IEEE80211_F_SCAN;
#ifdef QTN_BG_SCAN
		ic->ic_flags_qtn &= ~IEEE80211_QTN_BGSCAN;
#endif /* QTN_BG_SCAN */
		ic->ic_scan = NULL;
		FREE(SCAN_PRIVATE(ss), M_80211_SCAN);
	}
}

void
ieee80211_scan_vattach(struct ieee80211vap *vap)
{
	vap->iv_bgscanidle = msecs_to_jiffies(IEEE80211_BGSCAN_IDLE_DEFAULT);
	vap->iv_bgscanintvl = vap->iv_ic->ic_extender_bgscanintvl;
	vap->iv_scanvalid = SCAN_VALID_DEFAULT * HZ;
	vap->iv_roam.rssi11a = ROAM_RSSI_11A_DEFAULT;
	vap->iv_roam.rssi11b = ROAM_RSSI_11B_DEFAULT;
	vap->iv_roam.rssi11bOnly = ROAM_RSSI_11BONLY_DEFAULT;
	vap->iv_roam.rate11a = ROAM_RATE_11A_DEFAULT;
	vap->iv_roam.rate11b = ROAM_RATE_11B_DEFAULT;
	vap->iv_roam.rate11bOnly = ROAM_RATE_11BONLY_DEFAULT;

	txpow_rxgain_count = 0;
	txpow_rxgain_state = 1;
}

void
ieee80211_scan_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_IRQ(ic);
	if (ss->ss_vap == vap) {
		if ((ic->ic_flags & IEEE80211_F_SCAN)
#ifdef QTN_BG_SCAN
			|| (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN)
#endif /* QTN_BG_SCAN */
		) {
			del_timer(&SCAN_PRIVATE(ss)->ss_scan_timer);
			del_timer(&SCAN_PRIVATE(ss)->ss_probe_timer);
			ic->ic_flags &= ~IEEE80211_F_SCAN;
#ifdef QTN_BG_SCAN
			ic->ic_flags_qtn &= ~IEEE80211_QTN_BGSCAN;
#endif /* QTN_BG_SCAN */
		}
		if (ss->ss_ops != NULL) {
			ss->ss_ops->scan_detach(ss);
			ss->ss_ops = NULL;
		}
	}
	IEEE80211_UNLOCK_IRQ(ic);
}

/*
 * Simple-minded scanner module support.
 */
#define	IEEE80211_SCANNER_MAX	(IEEE80211_M_MONITOR+1)

static const char *scan_modnames[IEEE80211_SCANNER_MAX] = {
	[IEEE80211_M_IBSS]	= "wlan_scan_sta",
	[IEEE80211_M_STA]	= "wlan_scan_sta",
	[IEEE80211_M_AHDEMO]	= "wlan_scan_sta",
	[IEEE80211_M_HOSTAP]	= "wlan_scan_ap",
};
static const struct ieee80211_scanner *scanners[IEEE80211_SCANNER_MAX];

const struct ieee80211_scanner *
ieee80211_scanner_get(enum ieee80211_opmode mode, int tryload)
{
	int err;
	if (mode >= IEEE80211_SCANNER_MAX)
		return NULL;
	if (scan_modnames[mode] == NULL)
		return NULL;
	if (scanners[mode] == NULL && tryload) {
		err = ieee80211_load_module(scan_modnames[mode]);
		if (scanners[mode] == NULL || err)
			printk(KERN_WARNING "unable to load %s\n", scan_modnames[mode]);
	}
	return scanners[mode];
}
EXPORT_SYMBOL(ieee80211_scanner_get);

void
ieee80211_scanner_register(enum ieee80211_opmode mode,
	const struct ieee80211_scanner *scan)
{
	if (mode >= IEEE80211_SCANNER_MAX)
		return;
	scanners[mode] = scan;
}
EXPORT_SYMBOL(ieee80211_scanner_register);

void
ieee80211_scanner_unregister(enum ieee80211_opmode mode,
	const struct ieee80211_scanner *scan)
{
	if (mode >= IEEE80211_SCANNER_MAX)
		return;
	if (scanners[mode] == scan)
		scanners[mode] = NULL;
}
EXPORT_SYMBOL(ieee80211_scanner_unregister);

void
ieee80211_scanner_unregister_all(const struct ieee80211_scanner *scan)
{
	int m;

	for (m = 0; m < IEEE80211_SCANNER_MAX; m++)
		if (scanners[m] == scan)
			scanners[m] = NULL;
}
EXPORT_SYMBOL(ieee80211_scanner_unregister_all);

u_int8_t g_channel_fixed = 0;
static void
change_channel(struct ieee80211com *ic,
	struct ieee80211_channel *chan)
{
#if 1
	/* If channel is fixed using iwconfig then don't do anything */
	if(!g_channel_fixed) 
	{
		ic->ic_prevchan = ic->ic_curchan;
		ic->ic_curchan = chan;
		//printk("Curr chan : %d\n", ic->ic_curchan->ic_ieee);
		ic->ic_set_channel(ic);
	}
#else

		ic->ic_curchan = chan;
#endif
}

static char
channel_type(const struct ieee80211_channel *c)
{
	if (IEEE80211_IS_CHAN_ST(c))
		return 'S';
	if (IEEE80211_IS_CHAN_108A(c))
		return 'T';
	if (IEEE80211_IS_CHAN_108G(c))
		return 'G';
	if (IEEE80211_IS_CHAN_A(c))
		return 'a';
	if (IEEE80211_IS_CHAN_ANYG(c))
		return 'g';
	if (IEEE80211_IS_CHAN_B(c))
		return 'b';
	return 'f';
}

void
ieee80211_scan_dump_channels(const struct ieee80211_scan_state *ss)
{
	struct ieee80211com *ic = ss->ss_vap->iv_ic;
	const char *sep;
	int i;

	sep = "";
	for (i = ss->ss_next; i < ss->ss_last; i++) {
		const struct ieee80211_channel *c = ss->ss_chans[i];

		printf("%s%u%c", sep, ieee80211_chan2ieee(ic, c),
			channel_type(c));
		sep = ", ";
	}
}
EXPORT_SYMBOL(ieee80211_scan_dump_channels);

/*
 * Enable station power save mode and start/restart the scanning thread.
 */
static void
scan_restart_pwrsav(unsigned long arg)
{
	struct scan_state *ss = (struct scan_state *) arg;
	struct ieee80211vap *vap = ss->base.ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	int delay;

	ieee80211_sta_pwrsave(vap, 1);
	/*
	 * Use an initial 1ms delay to ensure the null
	 * data frame has a chance to go out.
	 * XXX 1ms is a lot, better to trigger scan
	 * on tx complete.
	 */
	delay = msecs_to_jiffies(1);
	if (delay < 1)
		delay = 1;

	ic->ic_setparam(vap->iv_bss, IEEE80211_PARAM_BEACON_ALLOW,
			1, NULL, 0);
	ic->ic_scan_start(ic);			/* notify driver */
	ss->ss_scanend = jiffies + delay + ss->ss_duration;
	ss->ss_iflags |= ISCAN_START;
	mod_timer(&ss->ss_scan_timer, jiffies + delay);
	/*
	 * FIXME: Note, we are not delaying probes at the start here so there
	 * may be issues with probe requests not being on the correct
	 * channel for the first channel scanned.
	 */
}

/*
 * Start/restart scanning.  If we're operating in station mode
 * and associated notify the ap we're going into power save mode
 * and schedule a callback to initiate the work (where there's a
 * better context for doing the work).  Otherwise, start the scan
 * directly.
 */
static int
scan_restart(struct scan_state *ss, u_int duration)
{
	struct ieee80211vap *vap = ss->base.ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	int defer = 0;

	if (ss->base.ss_next == ss->base.ss_last) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: no channels to scan\n", __func__);
		return 0;
	}
	if (vap->iv_opmode == IEEE80211_M_STA &&
#ifdef QTN_BG_SCAN
	    /* qtn bgscan sends pwrsav frame in MuC, or use large NAV */
	    (ss->base.ss_flags & IEEE80211_SCAN_QTN_BGSCAN) == 0 &&
#endif /* QTN_BG_SCAN */
	    vap->iv_state == IEEE80211_S_RUN &&
	    (ss->base.ss_flags & IEEE80211_SCAN_OPCHAN) == 0) {
		if ((vap->iv_bss->ni_flags & IEEE80211_NODE_PWR_MGT) == 0) {
			/*
			 * Initiate power save before going off-channel.
			 * Note that we cannot do this directly because
			 * of locking issues; instead we defer it to a
			 * tasklet.
			 */
			ss->ss_duration = duration;
			tasklet_schedule(&ss->ss_pwrsav);
			defer = 1;
		}
	}

	if (!defer) {
		if (vap->iv_opmode == IEEE80211_M_STA &&
#ifdef QTN_BG_SCAN
				!(ss->base.ss_flags & IEEE80211_SCAN_QTN_BGSCAN) &&
#endif
				vap->iv_state == IEEE80211_S_RUN) {
			ic->ic_setparam(vap->iv_bss, IEEE80211_PARAM_BEACON_ALLOW,
				1, NULL, 0);
		}

		ic->ic_scan_start(ic);		/* notify driver */
		ss->ss_scanend = jiffies + duration;
		ss->ss_iflags |= ISCAN_START;
		mod_timer(&ss->ss_scan_timer, jiffies);
		/*
		 * FIXME: Note, we are not delaying probes at the start here so there
		 * may be issues with probe requests not being on the correct
		 * channel for the first channel scanned.
		 */
	}
	return 1;
}

static void
copy_ssid(struct ieee80211vap *vap, struct ieee80211_scan_state *ss,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	if (nssid > IEEE80211_SCAN_MAX_SSID) {
		/* XXX printf */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: too many ssid %d, ignoring all of them\n",
			__func__, nssid);
		return;
	}
	memcpy(ss->ss_ssid, ssids, nssid * sizeof(ssids[0]));
	ss->ss_nssid = nssid;
}

/*
 * Start a scan unless one is already going.
 */
int
ieee80211_start_scan(struct ieee80211vap *vap, int flags, u_int duration,
	u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211com *ic = vap->iv_ic;
	const struct ieee80211_scanner *scan;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ic->sta_dfs_info.sta_dfs_strict_mode) {
		if ((ic->ic_bsschan != IEEE80211_CHAN_ANYC) &&
			IEEE80211_IS_CHAN_CAC_IN_PROGRESS(ic->ic_bsschan)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s: Ignored-CAC in progress\n", __func__);
			return 0;
		}
	}

	scan = ieee80211_scanner_get(vap->iv_opmode, 0);
	if (scan == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: no scanner support for mode %u\n",
			__func__, vap->iv_opmode);
		/* XXX stat */
		return 0;
	}

	if (ic->ic_flags_qtn & IEEE80211_QTN_MONITOR) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: not scanning - monitor mode enabled\n", __func__);
		return 0;
	}

	IEEE80211_LOCK_IRQ(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0
#ifdef QTN_BG_SCAN
		&& (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0
#endif /* QTN_BG_SCAN */
	) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: %s scan, duration %lu, desired mode %s, %s%s%s%s%s\n",
			__func__,
			flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive",
			duration,
			ieee80211_phymode_name[ic->ic_des_mode],
			flags & IEEE80211_SCAN_FLUSH ? "flush" : "append",
			flags & IEEE80211_SCAN_NOPICK ? ", nopick" : "",
			flags & IEEE80211_SCAN_PICK1ST ? ", pick1st" : "",
			flags & IEEE80211_SCAN_ONCE ? ", once" : "",
			flags & IEEE80211_SCAN_OPCHAN ? ", operating channel only" : "");

		ss->ss_vap = vap;
		if (ss->ss_ops != scan) {
			/* switch scanners; detach old, attach new */
			if (ss->ss_ops != NULL)
				ss->ss_ops->scan_detach(ss);
			if (!scan->scan_attach(ss)) {
				/* XXX attach failure */
				/* XXX stat+msg */
				ss->ss_ops = NULL;
			} else
				ss->ss_ops = scan;
		}

		if (ss->ss_ops != NULL) {
			if ((flags & IEEE80211_SCAN_NOSSID) == 0)
				copy_ssid(vap, ss, nssid, ssids);

			/* NB: top 4 bits for internal use */
			ss->ss_flags = flags & 0xfff;
			if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
				vap->iv_stats.is_scan_active++;
			else
				vap->iv_stats.is_scan_passive++;
			if (flags & IEEE80211_SCAN_FLUSH)
				ss->ss_ops->scan_flush(ss);

			/* NB: flush frames rx'd before 1st channel change */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			ss->ss_ops->scan_start(ss, vap);
			if (scan_restart(SCAN_PRIVATE(ss), duration)) {
#ifdef QTN_BG_SCAN
				if (flags & IEEE80211_SCAN_QTN_BGSCAN)
					ic->ic_flags_qtn |= IEEE80211_QTN_BGSCAN;
				else
#endif /*QTN_BG_SCAN */
					ic->ic_flags |= IEEE80211_F_SCAN;
#if defined(QBMPS_ENABLE)
				if ((ic->ic_flags_qtn & IEEE80211_QTN_BMPS) &&
				    (vap->iv_opmode == IEEE80211_M_STA)) {
					/* exit power-saving */
					ieee80211_pm_queue_work(ic);
				}
#endif
			}

#ifdef QTN_BG_SCAN
			if (ic->ic_qtn_bgscan.debug_flags >= 3) {
				printk("BG_SCAN: start %s scanning...\n",
					(ic->ic_flags & IEEE80211_F_SCAN)?"regular":"background");
			}
#endif /*QTN_BG_SCAN */

		}
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: %s scan already in progress\n", __func__,
			ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");
	}
	IEEE80211_UNLOCK_IRQ(ic);

	/* Don't transmit beacons while scanning */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP
#ifdef QTN_BG_SCAN
			&& !(flags & IEEE80211_SCAN_QTN_BGSCAN)
#endif /*QTN_BG_SCAN */
	) {
		ic->ic_beacon_stop(vap);
	}

	/* NB: racey, does it matter? */
	return (ic->ic_flags & IEEE80211_F_SCAN);
}
EXPORT_SYMBOL(ieee80211_start_scan);

/*
 * Under repeater mode, when the AP interface is not in RUN state,
 * hold off scanning procedure on STA interface
 */
int ieee80211_should_scan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *first_ap = NULL;
	struct ieee80211vap *vap_each;
	struct ieee80211vap *vap_tmp;
	int ret = 1;

	if (vap->iv_opmode != IEEE80211_M_STA || !(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER))
		return 1;

	IEEE80211_VAPS_LOCK_BH(ic);

	TAILQ_FOREACH_SAFE(vap_each, &ic->ic_vaps, iv_next, vap_tmp) {
		if (vap_each->iv_opmode == IEEE80211_M_HOSTAP) {
			first_ap = vap_each;
			break;
		}
	}
	KASSERT((first_ap != NULL), ("Repeater mode must have an AP interface"));

	if (first_ap->iv_state != IEEE80211_S_RUN)
		/*
		 * Do not initiate scan for repeater STA if AP interface hasn't
		 * been properly running yet
		 */
		ret = 0;

	IEEE80211_VAPS_UNLOCK_BH(ic);

	return ret;
}

/*
 * Check the scan cache for an ap/channel to use; if that
 * fails then kick off a new scan.
 */
int
ieee80211_check_scan(struct ieee80211vap *vap, int flags, u_int duration,
	u_int nssid, const struct ieee80211_scan_ssid ssids[],
	int (*action)(struct ieee80211vap *, const struct ieee80211_scan_entry *))
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
#ifdef SCAN_CACHE_ENABLE
	int checkscanlist = 0;
#endif

	/*
	 * Check if there's a list of scan candidates already.
	 * XXX want more than the ap we're currently associated with
	 */
	IEEE80211_LOCK_IRQ(ic);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		"%s: %s scan, duration %lu, desired mode %s, %s%s%s%s\n",
		__func__,
		flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive",
		duration,
		ieee80211_phymode_name[ic->ic_des_mode],
		flags & IEEE80211_SCAN_FLUSH ? "flush" : "append",
		flags & IEEE80211_SCAN_NOPICK ? ", nopick" : "",
		flags & IEEE80211_SCAN_PICK1ST ? ", pick1st" : "",
		flags & IEEE80211_SCAN_ONCE ? ", once" : "",
		flags & IEEE80211_SCAN_USECACHE ? ", usecache" : "");

	if (ss->ss_ops != NULL) {
		/* XXX verify ss_ops matches vap->iv_opmode */
		if ((flags & IEEE80211_SCAN_NOSSID) == 0) {
			/*
			 * Update the ssid list and mark flags so if
			 * we call start_scan it doesn't duplicate work.
			 */
			copy_ssid(vap, ss, nssid, ssids);
			flags |= IEEE80211_SCAN_NOSSID;
		}
#ifdef SCAN_CACHE_ENABLE
		if ((ic->ic_flags & IEEE80211_F_SCAN) == 0 &&
#ifdef QTN_BG_SCAN
		     (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0 &&
#endif /* QTN_BG_SCAN */
		     time_before(jiffies, ic->ic_lastscan + vap->iv_scanvalid)) {
			/*
			 * We're not currently scanning and the cache is
			 * deemed hot enough to consult.  Lock out others
			 * by marking IEEE80211_F_SCAN while we decide if
			 * something is already in the scan cache we can
			 * use.  Also discard any frames that might come
			 * in while temporarily marked as scanning.
			 */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			ic->ic_flags |= IEEE80211_F_SCAN;
			checkscanlist = 1;
		}
#endif
	}
	IEEE80211_UNLOCK_IRQ(ic);
#ifdef SCAN_CACHE_ENABLE
	if (checkscanlist) {
		/*
		 * ss must be filled out so scan may be restarted "outside"
		 * of the current callstack.
		 */
		ss->ss_flags = flags;
		ss->ss_duration = duration;
		if (ss->ss_ops->scan_end(ss, ss->ss_vap, action, flags & IEEE80211_SCAN_KEEPMODE)) {
			/* found an ap, just clear the flag */
			ic->ic_flags &= ~IEEE80211_F_SCAN;
			return 1;
		}
		/* no ap, clear the flag before starting a scan */
		ic->ic_flags &= ~IEEE80211_F_SCAN;
	}
#endif
	if ((flags & IEEE80211_SCAN_USECACHE) == 0 &&
			ieee80211_should_scan(vap)) {
		return ieee80211_start_scan(vap, flags, duration, nssid, ssids);
	} else {
		/* If we *must* use the cache and no ap was found, return failure */
		return 0;
	}
}

/*
 * Restart a previous scan.  If the previous scan completed
 * then we start again using the existing channel list.
 */
int
ieee80211_bg_scan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_IRQ(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0
#ifdef QTN_BG_SCAN
		&& (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0
#endif /* QTN_BG_SCAN */
	) {
		u_int duration;
		/*
		 * Go off-channel for a fixed interval that is large
		 * enough to catch most ap's but short enough that
		 * we can return on-channel before our listen interval
		 * expires.
		 */
		duration = IEEE80211_SCAN_OFFCHANNEL;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: %s scan, jiffies %lu duration %lu\n", __func__,
			ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive",
			jiffies, duration);

		if (ss->ss_ops != NULL) {
			ss->ss_vap = vap;
			/*
			 * A background scan does not select a new sta; it
			 * just refreshes the scan cache.  Also, indicate
			 * the scan logic should follow the beacon schedule:
			 * we go off-channel and scan for a while, then
			 * return to the bss channel to receive a beacon,
			 * then go off-channel again.  All during this time
			 * we notify the ap we're in power save mode.  When
			 * the scan is complete we leave power save mode.
			 * If any beacon indicates there are frames pending
			 *for us then we drop out of power save mode
			 * (and background scan) automatically by way of the
			 * usual sta power save logic.
			 */
			ss->ss_flags |= IEEE80211_SCAN_NOPICK |
				IEEE80211_SCAN_BGSCAN;

			if (ic->ic_scan_opchan_enable && vap->iv_opmode == IEEE80211_M_STA) {
				ss->ss_flags |= IEEE80211_SCAN_OPCHAN | IEEE80211_SCAN_ACTIVE;
				ss->ss_ops->scan_start(ss, vap);
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
					"%s: force a new active bgscan", __func__);
			}

			/* if previous scan completed, restart */
			if (ss->ss_next >= ss->ss_last) {
				ss->ss_next = 0;
				if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
					vap->iv_stats.is_scan_active++;
				else
					vap->iv_stats.is_scan_passive++;
				ss->ss_ops->scan_restart(ss, vap);
			}
			/* NB: flush frames rx'd before 1st channel change */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			ss->ss_mindwell = duration;
			if (scan_restart(SCAN_PRIVATE(ss), duration)) {
				ic->ic_flags |= IEEE80211_F_SCAN;
				ic->ic_flags_ext |= IEEE80211_FEXT_BGSCAN;
			}
		} else {
			/* XXX msg+stat */
		}
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: %s scan already in progress\n", __func__,
			ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");
	}
	IEEE80211_UNLOCK_IRQ(ic);

	/* NB: racey, does it matter? */
	return (ic->ic_flags & IEEE80211_F_SCAN);
}
EXPORT_SYMBOL(ieee80211_bg_scan);

static void
_ieee80211_cancel_scan(struct ieee80211vap *vap, int no_wait)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_IRQ(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN)
#ifdef QTN_BG_SCAN
		|| (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN)
#endif /* QTN_BG_SCAN */
	) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: cancel %s scan\n", __func__,
			ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");

		/* clear bg scan NOPICK and mark cancel request */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_CANCEL;
		ss->ss_ops->scan_cancel(ss, vap);

		if (no_wait) {
			/* force it to fire immediately */
			del_timer(&SCAN_PRIVATE(ss)->ss_scan_timer);
			(SCAN_PRIVATE(ss)->ss_scan_timer).function((SCAN_PRIVATE(ss)->ss_scan_timer).data);
		} else {
			/* force it to fire asap */
			mod_timer(&SCAN_PRIVATE(ss)->ss_scan_timer, jiffies);
		}

		/*
		 * The probe timer is not cleared, so there may be some
		 * probe requests sent after the scan, but that should not
		 * cause any issues.
		 */
	}
	IEEE80211_UNLOCK_IRQ(ic);
}

/*
 * Cancel any scan currently going on.
 */
void
ieee80211_cancel_scan(struct ieee80211vap *vap)
{
	_ieee80211_cancel_scan(vap, 0);
}

/*
 * Cancel any scan currently going on immediately
 */
void
ieee80211_cancel_scan_no_wait(struct ieee80211vap *vap)
{
	_ieee80211_cancel_scan(vap, 1);
}

/*
 * Sample the state of an off-channel for Interference Mitigation
 */
void
ieee80211_scan_scs_sample(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	int scanning;
	int16_t scs_chan = ic->ic_scs.scs_last_smpl_chan;
	int16_t chan_count = 0;
	struct ieee80211_channel *chan;
	int cur_bw;

	IEEE80211_LOCK_IRQ(ic);
	scanning = ((ic->ic_flags & IEEE80211_F_SCAN)
#ifdef QTN_BG_SCAN
		|| (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN)
#endif /* QTN_BG_SCAN */
		);
	IEEE80211_UNLOCK_IRQ(ic);
	if (scanning) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: not sampling - scan in progress\n", __func__);
		IEEE80211_SCS_CNT_INC(&ic->ic_scs, IEEE80211_SCS_CNT_IN_SCAN);
		return;
	}

	if (!ic->ic_scs.scs_stats_on) {
		SCSDBG(SCSLOG_INFO, "scs stats is disabled\n");
		return;
	}

	if (vap->iv_state != IEEE80211_S_RUN) {
		SCSDBG(SCSLOG_INFO, "vap is not in running status\n");
		return;
	}

	cur_bw = ieee80211_get_bw(ic);

scan_next_chan:
	chan_count++;
	scs_chan++;

	if (chan_count > ic->ic_nchans) {
		SCSDBG(SCSLOG_INFO, "no available off channel for sampling\n");
		return;
	}

	if (scs_chan >= ic->ic_nchans) {
		scs_chan = 0;
	}

	chan = &ic->ic_channels[scs_chan];

	if (isclr(ic->ic_chan_active, chan->ic_ieee)) {
		goto scan_next_chan;
	}

	/* do not scan current working channel */
	if (chan->ic_ieee == ic->ic_curchan->ic_ieee) {
		goto scan_next_chan;
	}

	if (cur_bw == BW_HT40) {
		if (!(chan->ic_flags & IEEE80211_CHAN_HT40) ||
				(chan->ic_ieee == ieee80211_find_sec_chan(ic->ic_curchan))) {
			goto scan_next_chan;
		}
	}

	if (cur_bw >= BW_HT80) {
		if (!(chan->ic_flags & IEEE80211_CHAN_VHT80) ||
				(chan->ic_center_f_80MHz == ic->ic_curchan->ic_center_f_80MHz)) {
			goto scan_next_chan;
		}
	}

	SCSDBG(SCSLOG_INFO, "choose sampling channel: %u\n", chan->ic_ieee);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		"%s: sampling channel %u freq=%u\n", __func__,
		chan->ic_ieee, chan->ic_freq);

	/* don't move to next until muc finish sampling */
	ic->ic_scs.scs_last_smpl_chan = scs_chan - 1;

	ic->ic_sample_channel(vap, chan);
}
EXPORT_SYMBOL(ieee80211_scan_scs_sample);


int
ap_list_asl_table(struct ieee80211_scan_state *ss)
{
	struct ap_state *as = ss->ss_priv;
	struct ap_scan_entry *se, *next;
	int i;

	printk(KERN_ERR "CHINUSE_START\n");

	for (i = 0; i < IEEE80211_CHAN_MAX; i++) {
		TAILQ_FOREACH_SAFE(se, &as->as_scan_list[i].asl_head, ase_list, next) {
				printk(KERN_EMERG "Channel %d : %d Mhz\n",
				       se->base.se_chan->ic_ieee, se->base.se_chan->ic_freq);
				break;
		}
	}
	printk(KERN_ERR "CHINUSE_END\n");
	return 0;
}

/*
 * Getting maximum and minimum dwell time for scanning
 */
static void
get_max_min_dwell(struct ieee80211_scan_state *ss, struct ieee80211_node *ni,
		int is_passive, int is_obss_scan, int *mindwell, int *maxdwell)
{
	if (is_passive) {
		if (is_obss_scan) {
			*mindwell = msecs_to_jiffies(ni->ni_obss_ie.obss_passive_dwell);
			if (*mindwell > ss->ss_maxdwell_passive)
				*maxdwell = *mindwell;
			else
				*maxdwell = ss->ss_maxdwell_passive;
		} else {
			*mindwell = ss->ss_mindwell_passive;
			*maxdwell = ss->ss_maxdwell_passive;
		}
	} else {
		if (is_obss_scan) {
			*mindwell = msecs_to_jiffies(ni->ni_obss_ie.obss_active_dwell);
			if (*mindwell > ss->ss_maxdwell)
				*maxdwell = *mindwell;
			else
				*maxdwell = ss->ss_maxdwell;
		} else {
			*mindwell = ss->ss_mindwell;
			*maxdwell = ss->ss_maxdwell;
		}
	}
}

/*
 * Switch to the next channel marked for scanning.
 */
static void
scan_next(unsigned long arg)
{
#define	ISCAN_REP	(ISCAN_MINDWELL | ISCAN_START | ISCAN_DISCARD)
	struct ieee80211_scan_state *ss = (struct ieee80211_scan_state *) arg;
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211_channel *chan;
	unsigned long maxdwell, scanend;
	int scanning, scandone;

	/* Make passive channels special */
	int is_passive;
	int is_obss = 0;
	int maxdwell_used;
	int mindwell_used;
#ifdef QTN_BG_SCAN
	int bgscan_dwell = 0;
#endif /* QTN_BG_SCAN */

	if (ss->ss_flags & IEEE80211_SCAN_OBSS) {
		if (ni && IEEE80211_AID(ni->ni_associd))
			is_obss = 1;
		else
			return;
	}

	IEEE80211_LOCK_IRQ(ic);
	scanning = ((ic->ic_flags & IEEE80211_F_SCAN)
#ifdef QTN_BG_SCAN
			|| (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN)
#endif /* QTN_BG_SCAN */
		);
	IEEE80211_UNLOCK_IRQ(ic);
	if (!scanning)			/* canceled */
		return;

again:
	scandone = (ss->ss_next >= ss->ss_last) ||
		(SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0;
	scanend = SCAN_PRIVATE(ss)->ss_scanend;

	if ((vap->iv_opmode == IEEE80211_M_STA) && (ss->ss_next == 0)) {
		/*
		 * Periodically scan using low Rx gain and Tx power in case
		 * association is failing because the AP is too close.
		 * More suitable power settings will be determined after association.
		 */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				  "%s Starting a scan (Low power %s, count %d)\n",
				  __func__, txpow_rxgain_state ? "on" : "off", txpow_rxgain_count);

		if ((ic->ic_pwr_adjust_scancnt > 0) && (ss->is_scan_valid) &&
#ifdef QTN_BG_SCAN
		     !(ss->ss_flags & IEEE80211_SCAN_QTN_BGSCAN) &&
#endif /* QTN_BG_SCAN */
		    (txpow_rxgain_count) && ((txpow_rxgain_count % ic->ic_pwr_adjust_scancnt) == 0)) {
			ieee80211_pwr_adjust(vap, txpow_rxgain_state);
			txpow_rxgain_state = !txpow_rxgain_state;
		}
		txpow_rxgain_count++;
#ifdef QTN_BG_SCAN
		if ((ss->ss_flags & IEEE80211_SCAN_QTN_BGSCAN) &&
				(SCAN_PRIVATE(ss)->ss_iflags & ISCAN_START)) {
			ic->ic_bgscan_start(ic);
		}
#endif /* QTN_BG_SCAN */
	}

	/* Work out the previous channel (to use passive vs. active mindwell) */
	chan = (ss->ss_next) ? ss->ss_chans[ss->ss_next-1] : ss->ss_chans[0];
	is_passive = (!(ss->ss_flags & IEEE80211_SCAN_ACTIVE) ||
				(chan->ic_flags & IEEE80211_CHAN_PASSIVE));
	get_max_min_dwell(ss, ni, is_passive, is_obss, &mindwell_used, &maxdwell_used);

	if (!scandone &&
	    (ss->ss_flags & IEEE80211_SCAN_GOTPICK) == 0 &&
	    ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_START) ||
	     time_before(jiffies + mindwell_used, scanend))) {


		chan = ss->ss_chans[ss->ss_next++];
		is_passive = (!(ss->ss_flags & IEEE80211_SCAN_ACTIVE) ||
					(chan->ic_flags & IEEE80211_CHAN_PASSIVE));
		ic->ic_scanchan = chan;

#ifdef QTN_BG_SCAN
		if (ss->ss_flags & IEEE80211_SCAN_QTN_BGSCAN) {
			uint32_t scan_mode = ss->ss_pick_flags & IEEE80211_PICK_BG_MODE_MASK;

			if (!is_passive) {
				scan_mode = IEEE80211_PICK_BG_ACTIVE;
			}

			if (chan->ic_ieee == ic->ic_bsschan->ic_ieee) {
				if (vap->iv_opmode != IEEE80211_M_STA) {
					scan_mode = IEEE80211_PICK_BG_ACTIVE;
				} else if (scan_mode & IEEE80211_PICK_BG_ACTIVE) {
					scan_mode = 0;
				}
			}

			if (!scan_mode) {
				/*
				 * Auto passive mode selection:
				 * 1) if FAT is larger than the threshold for fast mode
				 *  which is 60% by default, will pick passive fast mode
				 * 2) else if FAT is larger than the threshold for normal mode
				 * which is 30% by default, will pick passive normal mode
				 * 3) else pick passive slow mode.
				 */
				if (ic->ic_scs.scs_cca_idle_smthed >=
						ic->ic_qtn_bgscan.thrshld_fat_passive_fast) {
					scan_mode = IEEE80211_PICK_BG_PASSIVE_FAST;
				} else if (ic->ic_scs.scs_cca_idle_smthed >=
						ic->ic_qtn_bgscan.thrshld_fat_passive_normal) {
					scan_mode = IEEE80211_PICK_BG_PASSIVE_NORMAL;
				} else {
					scan_mode = IEEE80211_PICK_BG_PASSIVE_SLOW;
				}
			}

			if (scan_mode & IEEE80211_PICK_BG_ACTIVE) {
				maxdwell_used = msecs_to_jiffies(ic->ic_qtn_bgscan.duration_msecs_active);
			} else if (scan_mode & IEEE80211_PICK_BG_PASSIVE_FAST) {
				maxdwell_used = msecs_to_jiffies(ic->ic_qtn_bgscan.duration_msecs_passive_fast);
			} else if (scan_mode & IEEE80211_PICK_BG_PASSIVE_NORMAL) {
				maxdwell_used = msecs_to_jiffies(ic->ic_qtn_bgscan.duration_msecs_passive_normal);
			} else {
				maxdwell_used = msecs_to_jiffies(ic->ic_qtn_bgscan.duration_msecs_passive_slow);
			}
			maxdwell = mindwell_used = maxdwell_used;

			if (scan_mode & IEEE80211_PICK_BG_ACTIVE) {
				bgscan_dwell = ic->ic_qtn_bgscan.dwell_msecs_passive;
			} else {
				bgscan_dwell = ic->ic_qtn_bgscan.dwell_msecs_active;
			}

			/*
			 * Workaround: in STA mode, don't send probe request frame
			 * directly because the probe response frame from other AP
			 * may mess up the txalert timer
			 */
			if (chan->ic_ieee == ic->ic_bsschan->ic_ieee &&
					vap->iv_opmode != IEEE80211_M_STA) {
				ieee80211_send_probereq(vap->iv_bss,
						vap->iv_myaddr, vap->iv_dev->broadcast,
						vap->iv_dev->broadcast,
						(u_int8_t *)"", 0,
						vap->iv_opt_ie, vap->iv_opt_ie_len);

			} else {
				ic->ic_bgscan_channel(vap, chan, scan_mode, bgscan_dwell);
			}
		} else {
#endif /* QTN_BG_SCAN */
			/* Reset mindwell and maxdwell as the new channel could be passive */
			get_max_min_dwell(ss, ni, is_passive, is_obss, &mindwell_used, &maxdwell_used);

			/*
			 * Watch for truncation due to the scan end time.
			 */
			if (time_after(jiffies + maxdwell_used, scanend))
				maxdwell = scanend - jiffies;
			else
				maxdwell = maxdwell_used;

			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: chan %3d%c -> %3d%c [%s, dwell min %lu max %lu]\n",
				__func__,
				ieee80211_chan2ieee(ic, ic->ic_curchan),
					channel_type(ic->ic_curchan),
				ieee80211_chan2ieee(ic, chan), channel_type(chan),
				(ss->ss_flags & IEEE80211_SCAN_ACTIVE) &&
					(chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0 ?
					"active" : "passive",
				mindwell_used, maxdwell);

			/*
			 * Potentially change channel and phy mode.
			 */
			/* Channel change done with 20MHz wide channels unless in 40MHz only mode */
			if (!((ss->ss_flags & IEEE80211_SCAN_BGSCAN) &&
					chan->ic_ieee == ic->ic_curchan->ic_ieee)) {
				if (!ic->ic_11n_40_only_mode)
				      ic->ic_flags_ext |= IEEE80211_FEXT_SCAN_20;

				change_channel(ic, chan);

				/* Clear the flag */
				if (!ic->ic_11n_40_only_mode)
				      ic->ic_flags_ext &= ~IEEE80211_FEXT_SCAN_20;
			}
#ifdef QTN_BG_SCAN
		}
#endif /* QTN_BG_SCAN */
		/*
		 * If doing an active scan and the channel is not
		 * marked passive-only then send a probe request.
		 * Otherwise just listen for traffic on the channel.
		 */
		if ((ss->ss_flags & IEEE80211_SCAN_ACTIVE) &&
#ifdef QTN_BG_SCAN
		    /* qtn bgscan sends probe request in MuC */
		    (ss->ss_flags & IEEE80211_SCAN_QTN_BGSCAN) == 0 &&
#endif /* QTN_BG_SCAN */
		    (chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0) {
			/*
			 * Delay sending the probe requests so we are on
			 * the new channel. Current delay is half of maxdwell
			 * to make sure it is well within the dwell time,
			 * this can be fine tuned later if necessary.
			 */
			mod_timer(&SCAN_PRIVATE(ss)->ss_probe_timer,
				  jiffies + (maxdwell / 2));
		}
		SCAN_PRIVATE(ss)->ss_chanmindwell = jiffies + mindwell_used;
		mod_timer(&SCAN_PRIVATE(ss)->ss_scan_timer, jiffies + maxdwell);
		/* clear mindwell lock and initial channel change flush */
		SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_REP;
	} else {
		ic->ic_scan_end(ic);		/* notify driver */
		/*
		 * Record scan complete time.  Note that we also do
		 * this when canceled so any background scan will
		 * not be restarted for a while.
		 */
		if (scandone)
			ic->ic_lastscan = jiffies;

		/* return to the bss channel */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC) {
#ifdef QTN_BG_SCAN
			if (ss->ss_flags & IEEE80211_SCAN_QTN_BGSCAN) {
				struct ieee80211vap *tmp_vap;
				/*
				 * Need to update beacon because beacon may have been
				 * updated when AP is on off channel, and in this case,
				 * the channel offset in beacon's retry table setting
				 * may not be correct.
				 */
				TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
					if (tmp_vap->iv_opmode != IEEE80211_M_HOSTAP)
						continue;
					if (tmp_vap->iv_state != IEEE80211_S_RUN)
						continue;
					ic->ic_beacon_update(tmp_vap);
				}
			}
			else
#endif /* QTN_BG_SCAN */
				change_channel(ic, ic->ic_bsschan);
		}

		/* clear internal flags and any indication of a pick */
		SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_REP;
		ss->ss_flags &= ~IEEE80211_SCAN_GOTPICK;

		if ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) == 0) {
			ieee80211_check_type_of_neighborhood(ic);
#ifdef QSCS_ENABLED
			ieee80211_scs_update_ranking_table_by_scan(ic);
			ieee80211_scs_adjust_cca_threshold(ic);
#endif
		}

		/*
		 * If not canceled and scan completed, do post-processing.
		 * If the callback function returns 0, then it wants to
		 * continue/restart scanning.  Unfortunately we needed to
		 * notify the driver to end the scan above to avoid having
		 * rx frames alter the scan candidate list.
		 */
		if ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) == 0 &&
		    !ss->ss_ops->scan_end(ss, vap, NULL, 0) &&
		    (ss->ss_flags & IEEE80211_SCAN_ONCE) == 0 &&
		    time_before(jiffies + mindwell_used, scanend)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: done, restart "
				"[jiffies %lu, dwell min %lu scanend %lu]\n",
				__func__,
				jiffies, mindwell_used, scanend);
			ss->ss_next = 0;	/* reset to beginning */
			if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
				vap->iv_stats.is_scan_active++;
			else
				vap->iv_stats.is_scan_passive++;

//			ic->ic_scan_start(ic);	/* notify driver */
			goto again;
		} else {
			/* past here, scandone is ``true'' if not in bg mode */
			if ((ss->ss_flags & IEEE80211_SCAN_BGSCAN) == 0)
				scandone = 1;

			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: %s, "
				"[jiffies %lu, dwell min %lu scanend %lu]\n",
				__func__, scandone ? "done" : "stopped",
				jiffies, mindwell_used, scanend);

			/* don't care about bgscan case */
			if ((ic->ic_flags & IEEE80211_F_SCAN)
#ifdef QTN_BG_SCAN
				|| (ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN)
#endif /* QTN_BG_SCAN */
				) {
				wake_up_interruptible_all(&ic->ic_scan_comp);
			}
			/*
			 * Clear the SCAN bit first in case frames are
			 * pending on the station power save queue.  If
			 * we defer this then the dispatch of the frames
			 * may generate a request to cancel scanning.
			 */
			ic->ic_flags &= ~IEEE80211_F_SCAN;
#ifdef QTN_BG_SCAN
			ic->ic_flags_qtn &= ~IEEE80211_QTN_BGSCAN;
#endif /* QTN_BG_SCAN */

#if defined(QBMPS_ENABLE)
			if ((ic->ic_flags_qtn & IEEE80211_QTN_BMPS) &&
			    (vap->iv_opmode == IEEE80211_M_STA)) {
				/* re-enter power-saving if possible */
				ieee80211_pm_queue_work(ic);
			}
#endif
			/*
			 * Drop out of power save mode when a scan has
			 * completed.  If this scan was prematurely terminated
			 * because it is a background scan then don't notify
			 * the ap; we'll either return to scanning after we
			 * receive the beacon frame or we'll drop out of power
			 * save mode because the beacon indicates we have frames
			 * waiting for us.
			 */
			if (scandone) {
				ieee80211_sta_pwrsave(vap, 0);
				if ((vap->iv_state == IEEE80211_S_RUN) &&
				    (vap->iv_opmode == IEEE80211_M_STA)) {
					ic->ic_setparam(vap->iv_bss, IEEE80211_PARAM_BEACON_ALLOW,
							0, NULL, 0);
				}

				if (ss->ss_next >= ss->ss_last) {
					ieee80211_notify_scan_done(vap);
					ic->ic_flags_ext &= ~IEEE80211_FEXT_BGSCAN;
					if (IEEE80211_IS_11NG_40(ic)) {
						if (ic->ic_opmode == IEEE80211_M_STA)
							ieee80211_send_20_40_bss_coex(vap);
						else
							ieee80211_check_mode(vap);
					}
				}
			}

			if ((ic->ic_flags_qtn & IEEE80211_QTN_PRINT_CH_INUSE) &&
			    (ic->ic_opmode == IEEE80211_M_HOSTAP))
				ap_list_asl_table(ss);

			SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_CANCEL;
			ss->ss_flags &=
			    ~(IEEE80211_SCAN_ONCE | IEEE80211_SCAN_PICK1ST);
		}
	}
#undef ISCAN_REP
}

/*
 * Timer handler to send probe requests after a delay when doing an active
 * scan. This is done to allow time for the channel change to complete first.
 */
static void
send_probes(unsigned long arg)
{
	struct ieee80211_scan_state *ss = (struct ieee80211_scan_state *) arg;
	struct ieee80211vap *vap = ss->ss_vap;
	struct net_device *dev = vap->iv_dev;
	int i;

	/*
	 * Send a broadcast probe request followed by
	 * any specified directed probe requests.
	 * XXX suppress broadcast probe req?
	 * XXX remove dependence on vap/vap->iv_bss
	 * XXX move to policy code?
	 */
	if (vap->iv_bss) {
		ieee80211_send_probereq(vap->iv_bss,
			vap->iv_myaddr, dev->broadcast,
			dev->broadcast,
			(u_int8_t *)"", 0,
			vap->iv_opt_ie, vap->iv_opt_ie_len);

		for (i = 0; i < ss->ss_nssid; i++)
			ieee80211_send_probereq(vap->iv_bss,
				vap->iv_myaddr, dev->broadcast,
				dev->broadcast,
				ss->ss_ssid[i].ssid,
				ss->ss_ssid[i].len,
				vap->iv_opt_ie, vap->iv_opt_ie_len);
	}
}

#ifdef IEEE80211_DEBUG
static void
dump_probe_beacon(u_int8_t subtype,
	const u_int8_t mac[IEEE80211_ADDR_LEN],
	const struct ieee80211_scanparams *sp)
{

	printf("[%s] %02x ", ether_sprintf(mac), subtype);
	if (sp) {
		printf("on chan %u (bss chan %u) ", sp->chan, sp->bchan);
		ieee80211_print_essid(sp->ssid + 2, sp->ssid[1]);
	}
	printf("\n");

	if (sp) {
		printf("[%s] caps 0x%x bintval %u erp 0x%x", 
			ether_sprintf(mac), sp->capinfo, sp->bintval, sp->erp);
		if (sp->country != NULL) {
#ifdef __FreeBSD__
			printf(" country info %*D",
				sp->country[1], sp->country + 2, " ");
#else
			int i;
			printf(" country info");
			for (i = 0; i < sp->country[1]; i++)
				printf(" %02x", sp->country[i + 2]);
#endif
		}
		printf("\n");
	}
}
#endif /* IEEE80211_DEBUG */

/*
 * Process a beacon or probe response frame.
 */
void
ieee80211_add_scan(struct ieee80211vap *vap,
	const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh,
	int subtype, int rssi, int rstamp)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	/*
	 * Frames received during startup are discarded to avoid
	 * using scan state setup on the initial entry to the timer
	 * callback.  This can occur because the device may enable
	 * rx prior to our doing the initial channel change in the
	 * timer routine (we defer the channel change to the timer
	 * code to simplify locking on linux).
	 */

	if (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_DISCARD)
		return;
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(vap) && (ic->ic_flags & IEEE80211_F_SCAN) && sp)
		dump_probe_beacon(subtype, wh->i_addr2, sp);
#endif
	if ((ic->ic_opmode == IEEE80211_M_STA) &&
	    (ic->ic_flags & IEEE80211_F_SCAN) &&
#ifdef QTN_BG_SCAN
	    /* For qtn bgscan, probereq is sent by MuC */
	    ((ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0) &&
#endif /* QTN_BG_SCAN */
	    (subtype == IEEE80211_FC0_SUBTYPE_BEACON) &&
	    (ic->ic_curchan->ic_flags & IEEE80211_CHAN_DFS) &&
	    (ic->ic_curchan->ic_flags & IEEE80211_CHAN_PASSIVE)) {
		/* Beacon received on a DFS channel, OK to send probe */
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_scan(vap))
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: sending a probe req on DFS channel %3d%c\n",
				__func__,
				ieee80211_chan2ieee(ic, ic->ic_curchan),
				channel_type(ic->ic_curchan));
#endif
		if (ic->sta_dfs_info.sta_dfs_strict_mode) {
			if ((ss->ss_ops != NULL) &&
					ieee80211_is_chan_cac_required(ic->ic_curchan)) {
				ss->ss_ops->scan_add(ss, sp, wh, subtype, rssi, rstamp);
			}
		}
		send_probes((unsigned long)ss);
	} else if (ss->ss_ops != NULL &&
	    ss->ss_ops->scan_add(ss, sp, wh, subtype, rssi, rstamp)) {
#ifdef QTN_BG_SCAN
		if (ic->ic_qtn_bgscan.debug_flags >= 4) {
			u_int8_t *mac = (u_int8_t *)wh->i_addr2;
			u_int8_t ssid[IEEE80211_NWID_LEN+1] = {0};
			if (sp->ssid[1] && sp->ssid[1] <= IEEE80211_NWID_LEN) {
				memcpy(ssid, sp->ssid + 2, sp->ssid[1]);
			}
			printk("==> Add scan entry -- chan: %u,"
					" mac: %02x:%02x:%02x:%02x:%02x:%02x,"
					" ssid: %s <==\n",
					sp->chan,
					mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
					(char *)ssid);
		}
#endif /*QTN_BG_SCAN */

		/*
		 * If we've reached the min dwell time terminate
		 * the timer so we'll switch to the next channel.
		 */
		if ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_MINDWELL) == 0 &&
#ifdef QTN_BG_SCAN
		    ((ic->ic_flags_qtn & IEEE80211_QTN_BGSCAN) == 0) &&
#endif /* QTN_BG_SCAN */
		    time_after_eq(jiffies, SCAN_PRIVATE(ss)->ss_chanmindwell)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"%s: chan %3d%c min dwell met (%lu > %lu)\n",
				__func__,
				ieee80211_chan2ieee(ic, ic->ic_curchan),
					channel_type(ic->ic_curchan),
				jiffies, SCAN_PRIVATE(ss)->ss_chanmindwell);
			/*
			 * XXX
			 * We want to just kick the timer and still
			 * process frames until it fires but linux
			 * will livelock unless we discard frames.
			 */
#if 0
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_MINDWELL;
#else
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
#endif
			/* NB: trigger at next clock tick */
			mod_timer(&SCAN_PRIVATE(ss)->ss_scan_timer, jiffies);
		}
	}
}
EXPORT_SYMBOL(ieee80211_add_scan);

/*
 * Remove a particular scan entry
 */
void
ieee80211_scan_remove(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		ss->ss_ops->scan_remove(ss, ni);
	}
}

/*
 * Timeout/age scan cache entries; called from sta timeout
 * timer (XXX should be self-contained).
 */
void
_ieee80211_scan_timeout(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL)
		ss->ss_ops->scan_age(ss);
}

void ieee80211_scan_timeout(unsigned long arg)
{
	struct ieee80211com *ic = (struct ieee80211com *) arg;

	if (ic != NULL) {
		_ieee80211_scan_timeout(ic);
		ic->ic_scan_results_expire.expires = jiffies + ic->ic_scan_results_check * HZ;
		add_timer(&ic->ic_scan_results_expire);
	}
}

/*
 * Mark a scan cache entry after a successful associate.
 */
void
ieee80211_scan_assoc_success(struct ieee80211com *ic, const u_int8_t mac[])
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_NOTE_MAC(ss->ss_vap, IEEE80211_MSG_SCAN,
			mac, "%s",  __func__);
		ss->ss_ops->scan_assoc_success(ss, mac);
	}
}

/*
 * Demerit a scan cache entry after failing to associate.
 */
void
ieee80211_scan_assoc_fail(struct ieee80211com *ic,
	const u_int8_t mac[], int reason)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_NOTE_MAC(ss->ss_vap, IEEE80211_MSG_SCAN, mac,
			"%s: reason %u", __func__, reason);
		ss->ss_ops->scan_assoc_fail(ss, mac, reason);
	}
}

/*
 * Iterate over the contents of the scan cache.
 */
int
ieee80211_scan_iterate(struct ieee80211com *ic,
	ieee80211_scan_iter_func *f, void *arg)
{
  int res = 0;
  struct ieee80211_scan_state *ss = ic->ic_scan;
	
  if (ss->ss_ops != NULL) {
    res = ss->ss_ops->scan_iterate(ss, f, arg);
  }
  return res;
}

static void
scan_saveie(u_int8_t **iep, const u_int8_t *ie)
{
	if (ie == NULL) {
		if (*iep) {
			FREE(*iep, M_DEVBUF);
		}
		*iep = NULL;
	} else {
		ieee80211_saveie(iep, ie);
	}
}

void
ieee80211_add_scan_entry(struct ieee80211_scan_entry *ise,
			const struct ieee80211_scanparams *sp,
			const struct ieee80211_frame *wh,
			int subtype, int rssi, int rstamp)
{

	if (sp->ssid[1] != 0 &&
	    (ISPROBE(subtype) || ise->se_ssid[1] == 0)) {
		memcpy(ise->se_ssid, sp->ssid, 2 + sp->ssid[1]);
	}
	memcpy(ise->se_rates, sp->rates,
			2 + IEEE80211_SANITISE_RATESIZE(sp->rates[1]));
	if (sp->xrates != NULL) {
		memcpy(ise->se_xrates, sp->xrates,
				2 + IEEE80211_SANITISE_RATESIZE(sp->xrates[1]));
	} else {
		ise->se_xrates[1] = 0;
	}
	IEEE80211_ADDR_COPY(ise->se_bssid, wh->i_addr3);

	ise->se_rstamp = rstamp;
	memcpy(ise->se_tstamp.data, sp->tstamp, sizeof(ise->se_tstamp));
	ise->se_intval = sp->bintval;
	ise->se_capinfo = sp->capinfo;
	ise->se_chan = sp->rxchan;
	ise->se_fhdwell = sp->fhdwell;
	ise->se_fhindex = sp->fhindex;
	ise->se_erp = sp->erp;
	ise->se_timoff = sp->timoff;
	if (sp->tim != NULL) {
		const struct ieee80211_tim_ie *tim =
		    (const struct ieee80211_tim_ie *) sp->tim;
		ise->se_dtimperiod = tim->tim_period;
	}
	scan_saveie(&ise->se_wme_ie, sp->wme);
	scan_saveie(&ise->se_wpa_ie, sp->wpa);
	scan_saveie(&ise->se_rsn_ie, sp->rsn);
	scan_saveie(&ise->se_wsc_ie, sp->wsc);
	scan_saveie(&ise->se_ath_ie, sp->ath);
	scan_saveie(&ise->se_qtn_ie, sp->qtn);
	if (sp->qtn != NULL) {
		ise->se_qtn_ie_flags = ((struct ieee80211_ie_qtn *)sp->qtn)->qtn_ie_flags;
		ise->se_is_qtn_dev = 1;
	} else {
		ise->se_qtn_ie_flags = 0;
		ise->se_is_qtn_dev = 0;
	}
	scan_saveie(&ise->se_htcap_ie, sp->htcap);
	scan_saveie(&ise->se_htinfo_ie, sp->htinfo);
	scan_saveie(&ise->se_vhtcap_ie, sp->vhtcap);
	scan_saveie(&ise->se_vhtop_ie, sp->vhtop);

	ise->se_ext_role = sp->extender_role;
	scan_saveie(&ise->se_ext_bssid_ie, sp->ext_bssid_ie);
	ise->local_max_txpwr = sp->local_max_txpwr;
}
EXPORT_SYMBOL(ieee80211_add_scan_entry);

static struct ieee80211_channel *
ieee80211_scan_get_channel_by_ieee(struct ieee80211_scan_state *ss, uint8_t chan)
{
	uint32_t i;
	uint8_t tempchan;
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = vap->iv_ic;

	if (chan == 0)
		return NULL;

	for (i = 0; i < ss->ss_last; i++) {
		tempchan = ieee80211_chan2ieee(ic, ss->ss_chans[i]);
		if (!is_channel_valid(tempchan) || chan != tempchan)
			continue;
		return ss->ss_chans[i];
	}

	return NULL;
}

static void
ieee80211_scan_set_channel_obssflag(struct ieee80211_scan_state *ss, uint8_t ch, int flag)
{
	struct ap_state *as = ss->ss_priv;
	struct ieee80211_channel *chan;

	chan = ieee80211_scan_get_channel_by_ieee(ss, ch);
	if (chan == NULL)
		return;

	as->as_obss_chanlayout[ch] |= flag;
}

void
ieee80211_scan_check_secondary_channel(struct ieee80211_scan_state *ss,
			struct ieee80211_scan_entry *ise)
{
	int bss_bw = ieee80211_get_max_ap_bw(ise);
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211_channel *chan;
	uint8_t chan_pri;
	uint8_t chan_sec;

	if (bss_bw <= BW_HT20)
		return;

	ieee80211_find_ht_pri_sec_chan(vap, ise, &chan_pri, &chan_sec);
	if (chan_pri == 0)
		return;

	ieee80211_scan_set_channel_obssflag(ss, chan_pri, IEEE80211_OBSS_CHAN_PRI20);
	ieee80211_scan_set_channel_obssflag(ss, chan_sec, IEEE80211_OBSS_CHAN_SEC20);

	if (bss_bw >= BW_HT80) {
		ieee80211_scan_set_channel_obssflag(ss, chan_pri, IEEE80211_OBSS_CHAN_PRI40);
		ieee80211_scan_set_channel_obssflag(ss, chan_sec, IEEE80211_OBSS_CHAN_PRI40);

		chan = ieee80211_scan_get_channel_by_ieee(ss, chan_pri);
		if (chan) {
			chan_sec = ieee80211_find_sec40u_chan(chan);
			if (chan_sec != 0)
				ieee80211_scan_set_channel_obssflag(ss, chan_sec, IEEE80211_OBSS_CHAN_SEC40);
			chan_sec = ieee80211_find_sec40l_chan(chan);
			if (chan_sec != 0)
				ieee80211_scan_set_channel_obssflag(ss, chan_sec, IEEE80211_OBSS_CHAN_SEC40);
		}
	}
}
EXPORT_SYMBOL(ieee80211_scan_check_secondary_channel);

static int
_ieee80211_prichan_check_newchan(struct ieee80211_scan_state *ss,
			struct ieee80211_channel *chan,
			uint32_t *max_bsscnt)
{
	struct ap_state *as = ss->ss_priv;
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	uint32_t ch;

	if (!chan)
		return -1;

	ch = ieee80211_chan2ieee(ic, chan);
	if (!is_channel_valid(ch))
		return -1;

	if (isset(ic->ic_chan_pri_inactive, ch))
		return -1;

	if (!IEEE80211_IS_OBSS_CHAN_SECONDARY(as->as_obss_chanlayout[ch])) {
		if (as->as_numbeacons[ch] >= *max_bsscnt) {
			*max_bsscnt = as->as_numbeacons[ch];
			return 1;
		}
		return 0;
	}

	return -1;
}

/*
 * Channel selection methods for a VHT BSS,
 * as per IEEE Std 802.11ac 10.39.2, IEEE Std 802.11ac 10.5.3.
 * New BSS's primary channel shall not overlap other BSSs' secondary channels.
 */
struct ieee80211_channel *
ieee80211_scan_switch_pri_chan(struct ieee80211_scan_state *ss,
			struct ieee80211_channel *chan_pri)
{
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	int cur_bw = ieee80211_get_bw(ic);
	struct ieee80211_channel *chan_sec;
	uint32_t ch_pri = 0;
	uint32_t ch_sec;
	uint32_t bsscnt = 0;

	if (!chan_pri)
		return NULL;

	if (cur_bw >= BW_HT20) {
		ch_pri = chan_pri->ic_ieee;
		if (_ieee80211_prichan_check_newchan(ss, chan_pri, &bsscnt) < 0)
			ch_pri = 0;
	}

	if (cur_bw >= BW_HT40) {
		/* we look up operating class to follow different primary channel layouts, esp. 2.4G */
		ch_sec = ieee80211_find_sec_chan_by_operating_class(ic,
					chan_pri->ic_ieee,
					IEEE80211_OC_BEHAV_CHAN_UPPER);
		chan_sec = ieee80211_scan_get_channel_by_ieee(ss, ch_sec);
		if (_ieee80211_prichan_check_newchan(ss, chan_sec, &bsscnt) > 0)
			ch_pri = ch_sec;

		ch_sec = ieee80211_find_sec_chan_by_operating_class(ic,
					chan_pri->ic_ieee,
					IEEE80211_OC_BEHAV_CHAN_LOWWER);
		chan_sec = ieee80211_scan_get_channel_by_ieee(ss, ch_sec);
		if (_ieee80211_prichan_check_newchan(ss, chan_sec, &bsscnt) > 0)
			ch_pri = ch_sec;
	}

	if (cur_bw >= BW_HT80) {
		ch_sec = ieee80211_find_sec40u_chan(chan_pri);
		chan_sec = ieee80211_scan_get_channel_by_ieee(ss, ch_sec);
		if (_ieee80211_prichan_check_newchan(ss, chan_sec, &bsscnt) > 0)
			ch_pri = ch_sec;

		ch_sec = ieee80211_find_sec40l_chan(chan_pri);
		chan_sec = ieee80211_scan_get_channel_by_ieee(ss, ch_sec);
		if (_ieee80211_prichan_check_newchan(ss, chan_sec, &bsscnt) > 0)
			ch_pri = ch_sec;
	}

	return ieee80211_scan_get_channel_by_ieee(ss, ch_pri);
}
EXPORT_SYMBOL(ieee80211_scan_switch_pri_chan);

int
ieee80211_wps_active(uint8_t *wsc_ie)
{
#define IEEE80211_WPS_SELECTED_REGISTRAR 0x1041
	uint16_t type;
	uint16_t len;
	uint8_t *pos;
	uint8_t *end;

	if (!wsc_ie)
		return 0;

	pos = wsc_ie;
	end = wsc_ie + wsc_ie[1];

	pos += (2 + 4);
	while (pos < end) {
		if (end - pos < 4)
			break;

		type = be16toh(*(__be16 *)pos);
		pos += 2;
		len = be16toh(*(__be16 *)pos);
		pos += 2;

		if (len > end - pos)
			break;

		if ((type == IEEE80211_WPS_SELECTED_REGISTRAR) && (len == 1))
			return 1;

		pos += len;
	}

	return 0;
}
EXPORT_SYMBOL(ieee80211_wps_active);

void
ieee80211_dump_scan_res(struct ieee80211_scan_state *ss)
{
#define IEEE80211_BSS_CAPA_STR_LEN 30
	struct ieee80211vap *vap;
	struct sta_table *st;
	struct sta_entry *se, *next;
	struct ieee80211_scan_entry *ise;
	char ssid[IEEE80211_NWID_LEN + 1];
	char bss_capa[IEEE80211_BSS_CAPA_STR_LEN];
	char *pos;
	char *end;
	int len;

	if (!ss)
		return;

	vap = ss->ss_vap;
	st = ss->ss_priv;
	if (!ieee80211_msg(vap, IEEE80211_MSG_SCAN))
		return;

	printk("%-18s  %-33s  %-7s  %-25s  %-5s\n",
		"BSSID", "SSID", "Channel", "BSS Capabilities", "RSSI");

	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		ise = &se->base;
		memset(ssid, 0, sizeof(ssid));
		memcpy(ssid, &ise->se_ssid[2], MIN(sizeof(ssid), ise->se_ssid[1]));

		len = 0;
		pos = bss_capa;
		end = bss_capa + IEEE80211_BSS_CAPA_STR_LEN;
		memset(bss_capa, 0, sizeof(bss_capa));

		if (ise->se_capinfo & IEEE80211_CAPINFO_IBSS) {
			len = snprintf(pos, end - pos, "IBSS");
			pos += len;
		} else if (ise->se_capinfo & IEEE80211_CAPINFO_ESS) {
			len = snprintf(pos, end - pos, "ESS");
			pos += len;
		}

		if (ise->se_wpa_ie) {
			len = snprintf(pos, end - pos, "|WPA");
			pos += len;
		}
		if (ise->se_rsn_ie) {
			len = snprintf(pos, end - pos, "|RSN");
			pos += len;
		}

		if (ieee80211_wps_active(ise->se_wsc_ie))
		      snprintf(pos, end - pos, "|WPS_ACTIVE");
		else if (ise->se_wsc_ie)
		      snprintf(pos, end - pos, "|WPS");

		printk("%-18pM  %-33s  %-7u  %-25s  %-5d\n",
			ise->se_bssid,
			ssid,
			ise->se_chan->ic_ieee,
			bss_capa,
			ise->se_rssi);
	}
}
EXPORT_SYMBOL(ieee80211_dump_scan_res);

/*
 * Flush the contents of the scan cache.
 */
void
ieee80211_scan_flush(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_DPRINTF(ss->ss_vap, IEEE80211_MSG_SCAN,
			"%s\n",  __func__);
		ss->ss_ops->scan_flush(ss);
	}
}
EXPORT_SYMBOL(ieee80211_scan_flush);

/*
 * Check the scan cache for an ap/channel to use
 */
struct ieee80211_channel *
ieee80211_scan_pickchannel(struct ieee80211com *ic, int flags)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_ASSERT(ic);

	if (ss == NULL || ss->ss_ops == NULL || ss->ss_vap == NULL) {
		printk(KERN_WARNING "scan state structure not attached or not initialized\n");
		return NULL;
	}
	if (ss->ss_ops->scan_pickchan == NULL) {
		IEEE80211_DPRINTF(ss->ss_vap, IEEE80211_MSG_SCAN,
		    "%s: scan module does not support picking a channel, "
		    "opmode %s\n", __func__, ss->ss_vap->iv_opmode);
		return NULL;
	}

	return ss->ss_ops->scan_pickchan(ic, ss, flags);
}
EXPORT_SYMBOL(ieee80211_scan_pickchannel);

int ieee80211_get_type_of_neighborhood(struct ieee80211com *ic)
{
	if (ic->ic_neighbor_count < 0)
		return IEEE80211_NEIGHBORHOOD_TYPE_UNKNOWN;
	else if (ic->ic_neighbor_count <= ic->ic_neighbor_cnt_sparse)
		return IEEE80211_NEIGHBORHOOD_TYPE_SPARSE;
	else if (ic->ic_neighbor_count <= ic->ic_neighbor_cnt_dense)
		return IEEE80211_NEIGHBORHOOD_TYPE_DENSE;
	else
		return IEEE80211_NEIGHBORHOOD_TYPE_VERY_DENSE;
}

void ieee80211_check_type_of_neighborhood(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;
	struct ap_state *as = ss->ss_priv;
	struct ap_scan_entry *apse;
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;
	int i;

	ic->ic_neighbor_count = 0;

	if (ss->ss_vap->iv_opmode == IEEE80211_M_HOSTAP) {
		for (i = 0; i < IEEE80211_CHAN_MAX; i++) {
			TAILQ_FOREACH(apse, &as->as_scan_list[i].asl_head, ase_list) {
				ic->ic_neighbor_count++;
			}
		}
	} else if (ss->ss_vap->iv_opmode == IEEE80211_M_STA) {
		TAILQ_FOREACH(se, &st->st_entry, se_list) {
			ic->ic_neighbor_count++;
		}
	}
}


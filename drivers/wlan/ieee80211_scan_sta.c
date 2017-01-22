//swdepot/dev/ums/soc/main2/drivers/wlan/ieee80211_scan_sta.c#7 - edit change 3043 (text)
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
 * $Id: ieee80211_scan_sta.c 2749 2007-10-16 08:58:14Z kelmo $
 */
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 station scanning support.
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "net80211/if_media.h"

#include "net80211/ieee80211_var.h"
#define RSNIE_GROUP_CIPHER_OFFSET       0x7
static void sta_flush_table(struct sta_table *);
static int match_bss(struct ieee80211vap *, const struct ieee80211_scan_state *,
	const struct sta_entry *);
static int match_ssid(const uint8_t *ie, int nssid,
	const struct ieee80211_scan_ssid ssids[]);
static void action_tasklet(IEEE80211_TQUEUE_ARG);

static int
lock_sta_table(struct sta_table *st)
{
	// Call can come in SoftIRQ (timer or tasklet) or process context.
	// For optimization let's disable SoftIRQ only when call comes in process context.
	int bh_disabled = !in_softirq() && !irqs_disabled();

	// We must be not called within hardware interrupt context.
	WARN_ON_ONCE(in_irq());

	spin_lock(&st->st_lock);
	if(bh_disabled) {
		local_bh_disable();
	}

	return bh_disabled;
}
static void
unlock_sta_table(struct sta_table *st, int bh_disabled)
{
	if(bh_disabled) {
		local_bh_enable();
	}
	spin_unlock(&st->st_lock);
}

/*
 * Attach prior to any scanning work.
 */
static int
sta_attach(struct ieee80211_scan_state *ss)
{
	struct sta_table *st;

	_MOD_INC_USE(THIS_MODULE, return 0);

	MALLOC(st, struct sta_table *, sizeof(struct sta_table),
		M_80211_SCAN, M_NOWAIT | M_ZERO);
	if (st == NULL)
		return 0;
	spin_lock_init(&st->st_lock);
	spin_lock_init(&st->st_scanlock);
	TAILQ_INIT(&st->st_entry);
	IEEE80211_INIT_TQUEUE(&st->st_actiontq, action_tasklet, ss);
	ss->ss_priv = st;
	return 1;
}

/**
 * Clean up the scan entry structure, including freeing dynamic memory that may be used
 * to contain received IEs.
 */
static void
cleanup_se(struct sta_entry *se)
{
	struct ieee80211_scan_entry *ise = &se->base;
	if (ise->se_wpa_ie)
	{
		FREE(ise->se_wpa_ie, M_DEVBUF);
		ise->se_wpa_ie = NULL;
	}
	if (ise->se_rsn_ie)
	{
		FREE(ise->se_rsn_ie, M_DEVBUF);
		ise->se_rsn_ie = NULL;
	}
	if (ise->se_wme_ie)
	{
		FREE(ise->se_wme_ie, M_DEVBUF);
		ise->se_wme_ie = NULL;
	}
	if (ise->se_wsc_ie)
	{
		FREE(ise->se_wsc_ie, M_DEVBUF);
		ise->se_wsc_ie = NULL;
	}
	if (ise->se_htcap_ie)
	{
		FREE(ise->se_htcap_ie, M_DEVBUF);
		ise->se_htcap_ie = NULL;
	}
	if (ise->se_htinfo_ie)
	{
		FREE(ise->se_htinfo_ie, M_DEVBUF);
		ise->se_htinfo_ie = NULL;
	}
	if (ise->se_vhtcap_ie)
	{
		FREE(ise->se_vhtcap_ie, M_DEVBUF);
		ise->se_vhtcap_ie = NULL;
	}
	if (ise->se_vhtop_ie)
	{
		FREE(ise->se_vhtop_ie, M_DEVBUF);
		ise->se_vhtop_ie = NULL;
	}
	if (ise->se_ath_ie)
	{
		FREE(ise->se_ath_ie, M_DEVBUF);
		ise->se_ath_ie = NULL;
	}
	if (ise->se_qtn_ie)
	{
		FREE(ise->se_qtn_ie, M_DEVBUF);
		ise->se_qtn_ie = NULL;
	}
	if (ise->se_ext_bssid_ie)
	{
		FREE(ise->se_ext_bssid_ie, M_DEVBUF);
		ise->se_ext_bssid_ie = NULL;
	}
}

/**
 * Free scan entry structure.
 */
static void
free_se(struct sta_entry *se)
{
	cleanup_se(se);
	FREE(se, M_80211_SCAN);
}

/**
 * Free scan entry structure or put request if it is in use.
 * Function must be called with keeping table locked (lock_sta_table(st)/unlock_sta_table(st)).
 */
static void
free_se_request(struct sta_entry *se)
{
	if(se->se_inuse) {
		se->se_request_to_free = 1;
	} else {
		free_se(se);
	}
}

/**
 * Free scan entry function if it is not used and there is request to free it.
 * Function must be called with keeping table locked (lock_sta_table(st)/unlock_sta_table(st)).
 */
static void
free_se_process(struct sta_entry *se)
{
	if(!se->se_inuse && se->se_request_to_free) {
		free_se(se);
	}
}
/**
 * Set in-use flag.
 * Function must be called with keeping table locked (lock_sta_table(st)/unlock_sta_table(st)).
 */
static void
set_se_inuse(struct sta_entry *se)
{
	se->se_inuse = 1;
}
/**
 * Reset in-use flag. 'se' entry can be destroyed.
 * Function must be called with keeping table locked (lock_sta_table(st)/unlock_sta_table(st)).
 */
static void
reset_se_inuse(struct sta_entry *se)
{
	se->se_inuse = 0;
	free_se_process(se);
}

/*
 * Cleanup any private state.
 */
static int
sta_detach(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;

	if (st != NULL) {
		IEEE80211_CANCEL_TQUEUE(&st->st_actiontq);
		sta_flush_table(st);
		FREE(st, M_80211_SCAN);
	}

	_MOD_DEC_USE(THIS_MODULE);
	return 1;
}

/*
 * Flush all per-scan state.
 */
static int
sta_flush(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	sta_flush_table(st);
	unlock_sta_table(st, bh_disabled);
	ss->ss_last = 0;
	return 0;
}

/*
 * Flush all entries in the scan cache.
 */
static void
sta_flush_table(struct sta_table *st)
{
	struct sta_entry *se, *next;

	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		TAILQ_REMOVE(&st->st_entry, se, se_list);
		LIST_REMOVE(se, se_hash);
		free_se_request(se);
		if (st->st_entry_num > 0)
			st->st_entry_num--;
	}
}

/*
 * Compare function for sorting scan results.
 * Return >0 if @b is considered better.
 */
static int
sta_cmp(struct ieee80211_scan_state *ss,
	struct sta_entry *a, struct sta_entry *b)
{
	struct ieee80211_scan_entry *ise_a = &a->base;
	struct ieee80211_scan_entry *ise_b = &b->base;

	/* SSID matched AP preferred */
	if (match_ssid(ise_a->se_ssid, ss->ss_nssid, ss->ss_ssid) &&
			!match_ssid(ise_b->se_ssid, ss->ss_nssid, ss->ss_ssid))
		return -1;
	if (!match_ssid(ise_a->se_ssid, ss->ss_nssid, ss->ss_ssid) &&
			match_ssid(ise_b->se_ssid, ss->ss_nssid, ss->ss_ssid))
		return 1;

	/* WPS active AP preferred */
	if (ieee80211_wps_active(ise_a->se_wsc_ie) &&
			!ieee80211_wps_active(ise_b->se_wsc_ie))
		return -1;
	if (!ieee80211_wps_active(ise_a->se_wsc_ie) &&
			ieee80211_wps_active(ise_b->se_wsc_ie))
		return 1;

	/* WPA/WPA2 support preferred */
	if ((ise_a->se_wpa_ie || ise_a->se_rsn_ie) &&
			((!ise_b->se_wpa_ie && !ise_b->se_rsn_ie)))
		return -1;
	if ((!ise_a->se_wpa_ie && !ise_a->se_rsn_ie) &&
			((ise_b->se_wpa_ie || ise_b->se_rsn_ie)))
		return 1;

	/* Higher RSSI AP preferred */
	if (ise_a->se_rssi > ise_b->se_rssi)
		return -1;

	return 1;
}

/* Caller must lock the st->st_lock */
static void
sta_sort(struct ieee80211_scan_state *ss, struct sta_entry *se)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se_tmp, *next;
	int found = 0;

	TAILQ_FOREACH_SAFE(se_tmp, &st->st_entry, se_list, next) {
		if (sta_cmp(ss, se_tmp, se) > 0) {
			TAILQ_INSERT_BEFORE(se_tmp, se, se_list);
			found = 1;
			break;
		}
	}

	if (!found)
		TAILQ_INSERT_TAIL(&st->st_entry, se, se_list);
}

/*
 * Process a beacon or probe response frame; create an
 * entry in the scan cache or update any previous entry.
 */
static int
sta_add(struct ieee80211_scan_state *ss, const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh, int subtype, int rssi, int rstamp)
{
#define	PICK1ST(_ss) \
	((ss->ss_flags & (IEEE80211_SCAN_PICK1ST | IEEE80211_SCAN_GOTPICK)) == \
	IEEE80211_SCAN_PICK1ST)
	struct sta_table *st = ss->ss_priv;
	const u_int8_t *macaddr = wh->i_addr2;
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct sta_entry *se;
	struct ieee80211_scan_entry *ise;
	int hash;
	int found = 0;
	int bh_disabled;

	if (!sp)
		return 0;

	hash = STA_HASH(macaddr);
	bh_disabled = lock_sta_table(st);
	LIST_FOREACH(se, &st->st_hash[hash], se_hash)
		if (IEEE80211_ADDR_EQ(se->base.se_macaddr, macaddr) &&
		    sp->ssid[1] == se->base.se_ssid[1] &&
		    !memcmp(se->base.se_ssid + 2, sp->ssid + 2, se->base.se_ssid[1])) {
			TAILQ_REMOVE(&st->st_entry, se, se_list);
			found = 1;
			break;
		}

	if (!found) {
		if (st->st_entry_num >= ic->ic_scan_tbl_len_max) {
			if (printk_ratelimit())
				printk("scan found %u scan results but the list is"
					" restricted to %u entries\n", st->st_entry_num,
					ic->ic_scan_tbl_len_max);
			unlock_sta_table(st, bh_disabled);
			return 0;
		}

		MALLOC(se, struct sta_entry *, sizeof(struct sta_entry),
		       M_80211_SCAN, M_NOWAIT | M_ZERO);
		if (se == NULL) {
			if (printk_ratelimit())
				printk("failed to allocate new scan entry\n");
			unlock_sta_table(st, bh_disabled);
			return 0;
		}
		st->st_entry_num++;
		se->se_inuse = 0;
		se->se_request_to_free = 0;
		se->se_scangen = st->st_scangen - 1;
		IEEE80211_ADDR_COPY(se->base.se_macaddr, macaddr);
		LIST_INSERT_HEAD(&st->st_hash[hash], se, se_hash);
	}
	ise = &se->base;

	ieee80211_add_scan_entry(ise, sp, wh, subtype, rssi, rstamp);

	if (se->se_lastupdate == 0) {			/* first sample */
		se->se_avgrssi = RSSI_IN(rssi);
	} else {					/* avg with previous samples */
		RSSI_LPF(se->se_avgrssi, rssi);
	}
	ise->se_rssi = RSSI_GET(se->se_avgrssi);

	/* clear failure count after STA_FAIL_AGE passes */
	if (se->se_fails && (jiffies - se->se_lastfail) > STA_FAILS_AGE*HZ) {
		se->se_fails = 0;
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_SCAN, macaddr,
			"%s: fails %u", __func__, se->se_fails);
	}
	se->se_lastupdate = jiffies;		/* update time */
	se->se_seen = 1;
	se->se_notseen = 0;

	sta_sort(ss, se);

	unlock_sta_table(st, bh_disabled);

	/*
	 * If looking for a quick choice and nothing's
	 * been found check here.
	 */

	if (PICK1ST(ss) && match_bss(vap, ss, se) == 0)
	{
		ss->ss_flags |= IEEE80211_SCAN_GOTPICK;
	}

	return 1;
#undef PICK1ST
}

static struct ieee80211_channel *
find11gchannel(struct ieee80211com *ic, int i, int freq)
{
	struct ieee80211_channel *c;
	int j;

	/*
	 * The normal ordering in the channel list is b channel
	 * immediately followed by g so optimize the search for
	 * this.  We'll still do a full search just in case.
	 */
	for (j = i+1; j < ic->ic_nchans; j++) {
		c = &ic->ic_channels[j];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return c;
	}
	for (j = 0; j < i; j++) {
		c = &ic->ic_channels[j];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return c;
	}
	return NULL;
}

static u_int8_t sschans[IEEE80211_CHAN_BYTES];
static void
add_channels(struct ieee80211com *ic,
	struct ieee80211_scan_state *ss,
	enum ieee80211_phymode mode, const u_int16_t freq[], int nfreq)
{
	struct ieee80211_channel *c, *cg;
	u_int modeflags;
	int i;
	struct ieee80211vap *vap = ss->ss_vap;

	modeflags = ieee80211_get_chanflags(mode);
	for (i = 0; i < nfreq; i++) {
		c = ieee80211_find_channel(ic, freq[i], modeflags);
		if (c == NULL || isclr(ic->ic_chan_active_20, c->ic_ieee))
			continue;
		/* Channel already selected */
		if (isset(sschans, c->ic_ieee))
			continue;
		setbit(sschans, c->ic_ieee);
		if (mode == IEEE80211_MODE_AUTO) {
			/*
			 * XXX special-case 11b/g channels so we select
			 *     the g channel if both are present.
			 */
			if (IEEE80211_IS_CHAN_B(c) &&
			    (cg = find11gchannel(ic, i, c->ic_freq)) != NULL)
				c = cg;
		}
		if (ss->ss_last >= IEEE80211_SCAN_MAX)
			break;

		if (ic->ic_flags_ext & IEEE80211_FEXT_SCAN_FAST_REASS &&
			ic->ic_fast_reass_chan && (ic->ic_fast_reass_chan != IEEE80211_CHAN_ANYC) &&
			(ic->ic_fast_reass_chan->ic_ieee != c->ic_ieee)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
					"Skipping channel %u (fast reassoc channel %u)\n",
					c->ic_ieee, ic->ic_fast_reass_chan->ic_ieee);
			continue;
		} else if (ic->ic_flags_ext & IEEE80211_FEXT_SCAN_FAST_REASS &&
			ic->ic_fast_reass_chan && (ic->ic_fast_reass_chan != IEEE80211_CHAN_ANYC) &&
			(ic->ic_fast_reass_chan->ic_ieee == c->ic_ieee)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"Adding channel %u (fast reassoc channel %u)\n",
				c->ic_ieee, ic->ic_fast_reass_chan->ic_ieee);
		}

		ss->ss_chans[ss->ss_last++] = c;
	}
#undef N
}

static const u_int16_t rcl1[] =		/* 8 FCC channel: 36, 40, 44, 48, 52, 56, 60, 64 */
{ 5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320};
static const u_int16_t rcl2[] =		/* 4 MKK channels: 34, 38, 42, 46 */
{ 5170, 5190, 5210, 5230 };
static const u_int16_t rcl3[] =		/* 2.4Ghz ch: 1,2,3,4,5,6,7,8,9,10,11,12,13 */
	{ 2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472};
static const u_int16_t rcl4[] =		/* 5 FCC channel: 149, 153, 157, 161, 165, 169 */
{ 5745, 5765, 5785, 5805, 5825, 5845 };
static const u_int16_t rcl7[] =		/* 11 FCC channel: 100,104,108,112,116,120,124,128,132,136,140,144 */
{ 5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680, 5700, 5720 };
static const u_int16_t rcl8[] =		/* 2.4Ghz ch: 2,3,4,5,8,9,10,12 */
{ 2417, 2422, 2427, 2432, 2447, 2452, 2457, 2467 };
static const u_int16_t rcl9[] =		/* 2.4Ghz ch: 14 */
{ 2484 };
static const u_int16_t rcl10[] =	/* Added Korean channels 2312-2372 */
{ 2312, 2317, 2322, 2327, 2332, 2337, 2342, 2347, 2352, 2357, 2362, 2367, 2372 };
static const u_int16_t rcl11[] =	/* Added Japan channels in 4.9/5.0 spectrum */
{ 5040, 5060, 5080, 4920, 4940, 4960, 4980 };

/* Other 5GHz channels : 35, 37, 39, 41, 43, 45, 47,
   49, 50, 51, 53, 54, 55, 57, 58, 59, 61, 62, 63, 65, 66 */
static const u_int16_t rcl14[] =
{ 5175, 5185, 5195, 5205, 5215, 5225, 5235,
  5245, 5250, 5255, 5265, 5270, 5275, 5285, 5290, 5295, 5305, 5310, 5315, 5325, 5330 };

/* Other 5GHz channels : 98, 99, 101, 102, 103, 105, 106,
   107, 109, 110, 111, 113, 114, 115, 117, 118, 119, 121,
   122, 123, 125, 126, 127, 129, 130, 131, 133, 134, 135,
   137, 138, 139, 141, 142 */
static const u_int16_t rcl15[] =
{ 5490, 5495, 5505, 5510, 5515, 5525, 5530,
  5535, 5545, 5550, 5555, 5565, 5570, 5575, 5585, 5590, 5595, 5605,
  5610, 5615, 5625, 5630, 5635, 5645, 5650, 5655, 5665, 5670, 5675,
  5685, 5690, 5695, 5705, 5710 };

/* Other 5GHz channels : 147, 148, 150, 151, 152, 154, 155,
   156, 158, 159, 160, 162, 163, 164, 166, 167 */
static const u_int16_t rcl16[] =
{ 5735, 5740, 5750, 5755, 5760, 5770, 5775,
  5780, 5790, 5795, 5800, 5810, 5815, 5820, 5830, 5835 };

/* Other 5GHz channels : 182, 183, 184, 185, 186, 187, 188,
   189, 190, 191, 192, 193, 194, 195, 196, 197, 198 */
static const u_int16_t rcl17[] =
{ 4910, 4915, 4920, 4925, 4930, 4935, 4940,
  4945, 4950, 4955, 4960, 4965, 4970, 4975, 4980, 4985, 4990 };

/* 5GHz channels  - 40 MHz mode:
 * 34, 38, 42, 46, 50, 54, 58, 62, 66, 102, 106, 110, 114, 118,
 * 122, 126, 130, 134, 138, 142, 151, 155, 159, 163, 167
 */
static const u_int16_t rcl18[] =
{ 5170, 5190, 5210, 5230, 5250, 5270, 5290, 5310, 5330, 5510, 5530, 5550, 5570, 5590,
  5610, 5630, 5650, 5670, 5690, 5710, 5755, 5775, 5795, 5815, 5835 };

struct scanlist {
	u_int16_t	mode;
	u_int16_t	count;
	const u_int16_t	*list;
};

#define	IEEE80211_MODE_TURBO_STATIC_A	IEEE80211_MODE_MAX
#define	X(a)	.count = sizeof(a)/sizeof(a[0]), .list = a

static const struct scanlist staScanTable[] = {
	{ IEEE80211_MODE_11B,		X(rcl3) },
	{ IEEE80211_MODE_11A,		X(rcl1) },
	{ IEEE80211_MODE_11A,		X(rcl2) },
	{ IEEE80211_MODE_11B,		X(rcl8) },
	{ IEEE80211_MODE_11B,		X(rcl9) },
	{ IEEE80211_MODE_11A,		X(rcl4) },
	{ IEEE80211_MODE_11A,		X(rcl7) },
	{ IEEE80211_MODE_11B,		X(rcl10) },
	{ IEEE80211_MODE_11A,		X(rcl11) },
	{ IEEE80211_MODE_11NG,		X(rcl3) },
	{ IEEE80211_MODE_11NG_HT40PM,	X(rcl3) },
	{ IEEE80211_MODE_11NA,		X(rcl1) },
	{ IEEE80211_MODE_11NA,		X(rcl7) },
	{ IEEE80211_MODE_11NA,		X(rcl4) },
	{ IEEE80211_MODE_11NA,		X(rcl11) },
	{ IEEE80211_MODE_11NA,		X(rcl2) },
#ifdef QTN_SUPP_ALL_CHAN
	{ IEEE80211_MODE_11NA,		X(rcl14) },
	{ IEEE80211_MODE_11NA,		X(rcl15) },
	{ IEEE80211_MODE_11NA,		X(rcl16) },
	{ IEEE80211_MODE_11NA,		X(rcl17) },
#endif /* QTN_SUPP_ALL_CHAN */
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl1) },
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl4) },
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl7) },
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl11) },
/*	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl2) }, */
#ifdef QTN_SUPP_ALL_CHAN
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl14) },
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl15) },
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl16) },
	{ IEEE80211_MODE_11NA_HT40PM,	X(rcl17) },
#endif /* QTN_SUPP_ALL_CHAN */
	{ IEEE80211_MODE_11AC_VHT20PM,	X(rcl1) },
	{ IEEE80211_MODE_11AC_VHT20PM,	X(rcl4) },
	{ IEEE80211_MODE_11AC_VHT20PM,	X(rcl7) },
	{ IEEE80211_MODE_11AC_VHT20PM,	X(rcl11) },

	{ IEEE80211_MODE_11AC_VHT40PM,	X(rcl1) },
	{ IEEE80211_MODE_11AC_VHT40PM,	X(rcl4) },
	{ IEEE80211_MODE_11AC_VHT40PM,	X(rcl7) },
	{ IEEE80211_MODE_11AC_VHT40PM,	X(rcl11) },

	{ IEEE80211_MODE_11AC_VHT80PM,	X(rcl1) },
	{ IEEE80211_MODE_11AC_VHT80PM,	X(rcl4) },
	{ IEEE80211_MODE_11AC_VHT80PM,	X(rcl7) },
	{ IEEE80211_MODE_11AC_VHT80PM,	X(rcl11) },
	{ .list = NULL }
};

#undef X

/*
 * Start a station-mode scan by populating the channel list.
 */
static int
sta_start(struct ieee80211_scan_state *ss, struct ieee80211vap *vap)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ieee80211com *ic = vap->iv_ic;
	struct sta_table *st = ss->ss_priv;
	const struct scanlist *scan;
	enum ieee80211_phymode mode;
	struct ieee80211_channel *c;
	int i;

	ss->ss_last = 0;
	/* Selected scan channel list */
	memset(sschans, 0, sizeof(sschans));

	if ((ss->ss_flags & IEEE80211_SCAN_OPCHAN) && vap->iv_state == IEEE80211_S_RUN) {
		ss->ss_chans[ss->ss_last++] = ic->ic_curchan;
		goto scan_channel_list_ready;
	}

	/*
	 * Use the table of ordered channels to construct the list
	 * of channels for scanning.  Any channels in the ordered
	 * list not in the master list will be discarded.
	 */
	for (scan = staScanTable; scan->list != NULL; scan++) {
		mode = scan->mode;
		if ((ic->ic_des_mode != IEEE80211_MODE_AUTO) && (ic->ic_rf_chipid != CHIPID_DUAL)) {
			/*
			 * If a desired mode was specified, scan only
			 * channels that satisfy that constraint.
			 */
			if (ic->ic_des_mode != mode) {
				/*
				 * The scan table marks 2.4Ghz channels as b
				 * so if the desired mode is 11g, then use
				 * the 11b channel list but upgrade the mode.
				 */
				if (ic->ic_des_mode != IEEE80211_MODE_11G ||
				    mode != IEEE80211_MODE_11B)
					continue;
				mode = IEEE80211_MODE_11G;	/* upgrade */
			}
		} else if ((ss->ss_flags & IEEE80211_SCAN_OBSS) &&
				!IS_IEEE80211_MODE_24G_BAND(mode)) {
			continue;
		} else {
			/*
			 * This lets ieee80211_scan_add_channels
			 * upgrade an 11b channel to 11g if available.
			 */
			if (mode == IEEE80211_MODE_11B)
				mode = IEEE80211_MODE_AUTO;
		}
		/*
		 * Add the list of the channels; any that are not
		 * in the master channel list will be discarded.
		 */
		add_channels(ic, ss, mode, scan->list, scan->count);
	}

	/*
	 * Add the channels from the ic (from HAL) that are not present
	 * in the staScanTable.
	 */
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (isclr(ic->ic_chan_active_20, c->ic_ieee))
			continue;
		/* No dfs interference detected channels */
		if (c->ic_flags & IEEE80211_CHAN_RADAR)
			continue;
		/*
		 * scan dynamic turbo channels in normal mode.
		 */
		if (IEEE80211_IS_CHAN_DTURBO(c))
			continue;
		mode = ieee80211_chan2mode(c);
		if (ic->ic_des_mode != IEEE80211_MODE_AUTO) {
			/*
			 * If a desired mode was specified, scan only 
			 * channels that satisfy that constraint.
			 */
			if (ic->ic_des_mode != mode)
				continue;
		}
		/* Is channel already selected? */
		if (isset(sschans, c->ic_ieee))
			continue;
		setbit(sschans, c->ic_ieee);
		ss->ss_chans[ss->ss_last++] = c;
	}

scan_channel_list_ready:
	ss->ss_next = 0;

	ss->ss_mindwell = msecs_to_jiffies(ic->ic_mindwell_active);
	ss->ss_mindwell_passive = msecs_to_jiffies(ic->ic_mindwell_passive);
	ss->ss_maxdwell = msecs_to_jiffies(ic->ic_maxdwell_active);
	ss->ss_maxdwell_passive = msecs_to_jiffies(ic->ic_maxdwell_passive);

#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(vap)) {
		printk("%s: scan set ", vap->iv_dev->name);
		ieee80211_scan_dump_channels(ss);
		printk(" dwell min %ld max %ld\n",
			ss->ss_mindwell, ss->ss_maxdwell);
	}
#endif /* IEEE80211_DEBUG */

	st->st_newscan = 1;

	if (ic->ic_flags_ext & IEEE80211_FEXT_SCAN_FAST_REASS &&
		ic->ic_fast_reass_chan && (ic->ic_fast_reass_chan != IEEE80211_CHAN_ANYC)) {

		if (ieee80211_msg(vap, IEEE80211_MSG_SCAN)) {
			int i = 0;
			printk("%p Fast reassoc scan (%u)\n",
					ic, ic->ic_fast_reass_chan->ic_ieee);
			printk("Channel list for scan (should be 1 entry first %u last %u):\n", ss->ss_next, ss->ss_last);
			/* NULL terminate the list to print it out. */
			ss->ss_chans[ss->ss_last] = NULL;
			c = ss->ss_chans[i];
			while(c != IEEE80211_CHAN_ANYC && c != NULL) {
				printk("channel %u ->", c->ic_ieee);
				c = ss->ss_chans[++i];
			}
			printk("<end>\n");
		}

		ic->ic_fast_reass_scan_cnt++;
		if (ic->ic_fast_reass_scan_cnt > IEEE80211_FAST_REASS_SCAN_MAX) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
				"Clearing fast scan channel - tried %u times\n", --ic->ic_fast_reass_scan_cnt);
			ic->ic_fast_reass_chan = IEEE80211_CHAN_ANYC;
			ic->ic_fast_reass_scan_cnt = 0;
		}
	}

	return 0;
#undef N
}

/*
 * Restart a bg scan.
 */
static int
sta_restart(struct ieee80211_scan_state *ss, struct ieee80211vap *vap)
{
	struct sta_table *st = ss->ss_priv;

	st->st_newscan = 1;
	return 0;
}

/*
 * Cancel an ongoing scan.
 */
static int
sta_cancel(struct ieee80211_scan_state *ss, struct ieee80211vap *vap)
{
	struct sta_table *st = ss->ss_priv;

	IEEE80211_CANCEL_TQUEUE(&st->st_actiontq);
	return 0;
}

static u_int8_t
maxrate(const struct ieee80211_scan_entry *se)
{
	u_int8_t max, r;
	int i;

	max = 0;
	for (i = 0; i < se->se_rates[1]; i++) {
		r = se->se_rates[2+i] & IEEE80211_RATE_VAL;
		if (r > max)
			max = r;
	}
	for (i = 0; i < se->se_xrates[1]; i++) {
		r = se->se_xrates[2+i] & IEEE80211_RATE_VAL;
		if (r > max)
			max = r;
	}
	return max;
}

/*
 * Compare the capabilities of two entries and decide which is
 * more desirable (return >0 if a is considered better).  Note
 * that we assume compatibility/usability has already been checked
 * so we don't need to (e.g. validate whether privacy is supported).
 * Used to select the best scan candidate for association in a BSS.
 */
static int
sta_compare(const struct sta_entry *a, const struct sta_entry *b)
{
	u_int8_t maxa, maxb;
	int weight;

	/* privacy support preferred */
	if ((a->base.se_capinfo & IEEE80211_CAPINFO_PRIVACY) &&
	    (b->base.se_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
		return 1;
	if ((a->base.se_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0 &&
	    (b->base.se_capinfo & IEEE80211_CAPINFO_PRIVACY))
		return -1;

	/* compare count of previous failures */
	weight = b->se_fails - a->se_fails;
	if (abs(weight) > 1)
		return weight;

	if (abs(b->base.se_rssi - a->base.se_rssi) < 5) {
		/* best/max rate preferred if signal level close enough XXX */
		maxa = maxrate(&a->base);
		maxb = maxrate(&b->base);
		if (maxa != maxb)
			return maxa - maxb;
		/* XXX use freq for channel preference */
		/* for now just prefer 5Ghz band to all other bands */
		if (IEEE80211_IS_CHAN_5GHZ(a->base.se_chan) &&
		   !IEEE80211_IS_CHAN_5GHZ(b->base.se_chan))
			return 1;
		if (!IEEE80211_IS_CHAN_5GHZ(a->base.se_chan) &&
		    IEEE80211_IS_CHAN_5GHZ(b->base.se_chan))
			return -1;
	}
	/* all things being equal, use signal level */
	return a->base.se_rssi - b->base.se_rssi;
}


/*
 * Check MCS suitability and return the best supported rate.
 */
static int
check_basic_mcs(struct ieee80211vap *vap, const struct ieee80211_scan_entry *se)
{
#ifdef RATE_SUPP_ENABLE
	struct ieee80211com *ic = vap->iv_ic;
	int i, j, okset = 0, okidx = 0, okridx, val = 0;
	u_int8_t mcs = 0;
	struct ieee80211_ie_htinfo *htinfo = (struct ieee80211_ie_htinfo *)se->htinfo;

	/*
	 * first check for the sets that we support
	 */
	for (i = 0; i < IEEE80211_HT_MAXMCS_BASICSET_SUPPORTED; i++)
	{
		mcs = IEEE80211_HTINFO_BASIC_MCS_VALUE(htinfo,i);
		mcs = mcs & (ic->ic_htcap.mcsset[i]);
		IEEE80211_HT_MCS_IDX(mcs,val);
		if (val != 0xFF)
		{
			okidx = val;
			okset = i;
		}
	}

	if !(okidx)
		return IEEE80211_HT_BASIC_RATE;
	else
		okridx = IEEE80211_HT_RATE_TABLE_IDX(okset,okidx)

	/*
	 * now check for the sets that we do not support
	 */
	for (i = 0; i < IEEE80211_HT_MAXMCS_BASICSET_SUPPORTED; i++)
	{
		mcs = IEEE80211_HTINFO_BASIC_MCS_VALUE(htinfo,i);
		IEEE80211_HT_MCS_IDX(mcs,val)
		if (val != 0xFF)
			return (IEEE80211_HT_BASIC_RATE | IEEE80211_HT_RATE_TABLE_IDX(i,val-1));
	}

	return okridx;
#else
		return 0;
#endif
}

/*
 * Check rate set suitability and return the best supported rate.
 */
static int
check_rate(struct ieee80211vap *vap, const struct ieee80211_scan_entry *se)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
	struct ieee80211com *ic = vap->iv_ic;
	const struct ieee80211_rateset *srs;
	int i, j, nrs, r, okrate, badrate, fixedrate;
	const u_int8_t *rs;

	okrate = badrate = fixedrate = 0;
	
	if (IEEE80211_IS_CHAN_HALF(se->se_chan))
		srs = &ic->ic_sup_half_rates;
	else if (IEEE80211_IS_CHAN_QUARTER(se->se_chan))
		srs = &ic->ic_sup_quarter_rates;
	else
		srs = &ic->ic_sup_rates[ieee80211_chan2mode(se->se_chan)];
	nrs = se->se_rates[1];
	rs = se->se_rates + 2;
	fixedrate = IEEE80211_FIXED_RATE_NONE;
again:
	for (i = 0; i < nrs; i++) {
		r = RV(rs[i]);
		badrate = r;
		/*
		 * Check any fixed rate is included. 
		 */
#if 0 /* Not required */
		if (r == vap->iv_fixed_rate)
			fixedrate = r;
#endif
		/*
		 * Check against our supported rates.
		 */
		for (j = 0; j < srs->rs_nrates; j++)
			if (r == RV(srs->rs_rates[j])) {
				if (r > okrate)		/* NB: track max */
					okrate = r;
				break;
			}
	}
	if (rs == se->se_rates+2) {
		/* scan xrates too; sort of an algol68-style for loop */
		nrs = se->se_xrates[1];
		rs = se->se_xrates + 2;
		goto again;
	}
	//if (okrate == 0 || vap->iv_fixed_rate != fixedrate)
	if (okrate == 0)
		return badrate | IEEE80211_RATE_BASIC;
	else
		return RV(okrate);
#undef RV
}

static int
match_ssid(const u_int8_t *ie,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	int i;

	for (i = 0; i < nssid; i++) {
		if (ie[1] == ssids[i].len &&
		     memcmp(ie + 2, ssids[i].ssid, ie[1]) == 0)
			return 1;
	}
	return 0;
}

/*
 * Test a scan candidate for suitability/compatibility.
 */
static int
match_bss(struct ieee80211vap *vap,
	const struct ieee80211_scan_state *ss, const struct sta_entry *se0)
{
	struct ieee80211com *ic = vap->iv_ic;
	const struct ieee80211_scan_entry *se = &se0->base;
	u_int8_t rate;
	int fail;
	u_int8_t ridx;
	uint32_t channel;

	fail = 0;
	channel = ieee80211_chan2ieee(ic, se->se_chan);
	channel = (channel > IEEE80211_CHAN_MAX) ? 0 : channel;
	if (isclr(ic->ic_chan_active_20, channel))
		fail |= 0x01;
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		if ((se->se_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((se->se_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
		if ((se->se_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		/* XXX does this mean privacy is supported or required? */
		if (se->se_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
	rate = check_rate(vap, se);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if ((ss->ss_nssid != 0) &&
	    !match_ssid(se->se_ssid, ss->ss_nssid, ss->ss_ssid))
		fail |= 0x10;
	if ((vap->iv_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(vap->iv_des_bssid, se->se_bssid))
		fail |= 0x20;

	if (se0->se_fails >= STA_FAILS_MAX)
		fail |= 0x40;
	if (se0->se_notseen >= STA_PURGE_SCANS)
		fail |= 0x80;

	ridx = check_basic_mcs(vap, se);
#define IEEE80211_HT_IS_BASIC_MCS(var) var&0x80
	if (IEEE80211_HT_IS_BASIC_MCS(ridx))
		fail |= 0x100;
	/*
	 * Ignore APs that do not support Bridge Mode if Bridge Mode has not been
	 * disabled.
	 * But: dual band RFIC can be used as regular STA connect to 3rd party AP
	 * so disable the bridge mode checking for RFIC5
	 */

	if (ic->ic_rf_chipid != CHIPID_DUAL) {
		if (!(vap->iv_qtn_flags & IEEE80211_QTN_BRIDGEMODE_DISABLED) &&
		    !se->se_qtn_ie_flags & IEEE80211_QTN_BRIDGEMODE) {
			fail |= 0x200;
		}
	}
	/* allow tkip for non US/FCC regions */
	if (!WPA_TKIP_SUPPORT && (ic->ic_country_code == CTRY_UNITED_STATES)) {
		if ((se->se_rsn_ie != NULL) && ((se->se_rsn_ie)[RSNIE_GROUP_CIPHER_OFFSET] == RSN_CSE_TKIP))
			fail |= 0x04;
	}
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg(vap, IEEE80211_MSG_SCAN | IEEE80211_MSG_ROAM)) {
		printf(" %03x", fail);
		printf(" %c %s",
			fail & 0x40 ? '=' : fail & 0x80 ? '^' : fail ? '-' : '+',
			ether_sprintf(se->se_macaddr));
		printf(" %s%c", ether_sprintf(se->se_bssid),
			fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, se->se_chan),
			fail & 0x01 ? '!' : ' ');
		printf(" %+4d", se->se_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
			fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
			(se->se_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
			(se->se_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" : "????",
			fail & 0x02 ? '!' : ' ');
		printf(" %3s%c ",
			(se->se_capinfo & IEEE80211_CAPINFO_PRIVACY) ? "wep" : "no",
			fail & 0x04 ? '!' : ' ');
		printf(" bm=%u/0x%02x%c ",
			(vap->iv_qtn_flags & IEEE80211_QTN_BRIDGEMODE_DISABLED), se->se_qtn_ie_flags,
			fail & 0x200 ? '!' : ' ');
		printk(" %6s%d","htridx",(ridx & IEEE80211_HT_RATE_TABLE_IDX_MASK));
		ieee80211_print_essid(se->se_ssid + 2, se->se_ssid[1]);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}
#endif
	return fail;
}

static void
sta_update_notseen(struct sta_table *st)
{
	struct sta_entry *se;
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH(se, &st->st_entry, se_list) {
		/*
		 * If seen then reset and don't bump the count;
		 * otherwise bump the ``not seen'' count.  Note
		 * that this ensures that stations for which we
		 * see frames while not scanning but not during
		 * this scan will not be penalized.
		 */
		if (se->se_seen)
			se->se_seen = 0;
		else
			se->se_notseen++;
	}
	unlock_sta_table(st, bh_disabled);
}

static void
sta_dec_fails(struct sta_table *st)
{
	struct sta_entry *se;
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH(se, &st->st_entry, se_list)
		if (se->se_fails)
			se->se_fails--;
	unlock_sta_table(st, bh_disabled);
}

static struct sta_entry *
select_bss(struct ieee80211_scan_state *ss, struct ieee80211vap *vap)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *selbs = NULL;
	int bh_disabled;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN | IEEE80211_MSG_ROAM, " %s\n",
		"macaddr          bssid         chan  rssi  rate flag  wep  essid");
	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH(se, &st->st_entry, se_list) {
		if (match_bss(vap, ss, se) == 0) {
			if (selbs == NULL)
				selbs = se;
			else if (sta_compare(se, selbs) > 0)
				selbs = se;
		}
	}
	unlock_sta_table(st, bh_disabled);

	return selbs;
}

/*
 * Pick an ap or ibss network to join or find a channel
 * to use to start an ibss network.
 */
static int
sta_pick_bss(struct ieee80211_scan_state *ss, struct ieee80211vap *vap,
	int (*action)(struct ieee80211vap *, const struct ieee80211_scan_entry *),
	u_int32_t flags)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *selbss;

	KASSERT(vap->iv_opmode == IEEE80211_M_STA,
		("wrong mode %u", vap->iv_opmode));

	if (st->st_newscan) {
		sta_update_notseen(st);
		st->st_newscan = 0;
	}
	if (ss->ss_flags & IEEE80211_SCAN_NOPICK) {
		/*
		 * Manual/background scan, don't select+join the
		 * bss, just return.  The scanning framework will
		 * handle notification that this has completed.
		 */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		return 1;
	}
	/*
	 * Automatic sequencing; look for a candidate and
	 * if found join the network.
	 */
	/* NB: unlocked read should be ok */
	if (TAILQ_FIRST(&st->st_entry) == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: no scan candidate\n", __func__);
notfound:
		/*
		 * If nothing suitable was found decrement
		 * the failure counts so entries will be
		 * reconsidered the next time around.  We
		 * really want to do this only for sta's
		 * where we've previously had some success.
		 */
		sta_dec_fails(st);
		st->st_newscan = 1;
		return 0;			/* restart scan */
	}
	st->st_action = ss->ss_ops->scan_default;
	if (action)
		st->st_action = action;
	if ((selbss = select_bss(ss, vap)) == NULL ) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: select_bss failed\n", __func__);
		goto notfound;
	}
	st->st_selbss = selbss->base;

	/*
	 * Must defer action to avoid possible recursive call through 80211
	 * state machine, which would result in recursive locking.
	 */
	if (!(ss->ss_flags & IEEE80211_SCAN_ONCE)) {
		IEEE80211_SCHEDULE_TQUEUE(&st->st_actiontq);
	}

	return 1;				/* terminate scan */
}


/*
 * Lookup an entry in the scan cache.  We assume we're
 * called from the bottom half or such that we don't need
 * to block the bottom half so that it's safe to return
 * a reference to an entry w/o holding the lock on the table.
 */
static struct sta_entry *
sta_lookup(struct sta_table *st, const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct sta_entry *se;
	int hash = STA_HASH(macaddr);
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	LIST_FOREACH(se, &st->st_hash[hash], se_hash)
		if (IEEE80211_ADDR_EQ(se->base.se_macaddr, macaddr))
			break;
	unlock_sta_table(st, bh_disabled);

	return se;		/* NB: unlocked */
}

static void
sta_roam_check(struct ieee80211_scan_state *ss, struct ieee80211vap *vap)
{
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211com *ic = vap->iv_ic;
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *selbs;
	u_int8_t roamRate, curRate;
	int8_t roamRssi, curRssi;

	se = sta_lookup(st, ni->ni_macaddr);
	if (se == NULL) {
		/* XXX something is wrong */
		return;
	}

	/* XXX do we need 11g too? */
	if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan)) {
		roamRate = vap->iv_roam.rate11b;
		roamRssi = vap->iv_roam.rssi11b;
	} else if (IEEE80211_IS_CHAN_B(ic->ic_bsschan)) {
		roamRate = vap->iv_roam.rate11bOnly;
		roamRssi = vap->iv_roam.rssi11bOnly;
	} else {
		roamRate = vap->iv_roam.rate11a;
		roamRssi = vap->iv_roam.rssi11a;
	}
	/* NB: the most up to date rssi is in the node, not the scan cache */
	curRssi = ic->ic_node_getrssi(ni);
	if (vap->iv_fixed_rate == IEEE80211_FIXED_RATE_NONE) {
		curRate = ni->ni_rates.rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ROAM,
			"%s: currssi %d currate %u roamrssi %d roamrate %u\n",
			__func__, curRssi, curRate, roamRssi, roamRate);
	} else {
		curRate = roamRate;		/* NB: ensure compare below fails */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ROAM,
			"%s: currssi %d roamrssi %d\n",
			__func__, curRssi, roamRssi);
	}
	if (((vap->iv_flags & IEEE80211_F_BGSCAN) || ic->ic_scan_opchan_enable) &&
	    time_after(jiffies, ic->ic_lastscan + vap->iv_scanvalid)) {
		/*
		 * Scan cache contents is too old; check about updating it.
		 */
		if (curRate < roamRate || curRssi < roamRssi) {
			/*
			 * Thresholds exceeded, force a scan now so we
			 * have current state to make a decision with.
			 */
			ieee80211_bg_scan(vap);
		} else if (time_after(jiffies,
			ic->ic_lastdata + vap->iv_bgscanidle)) {
			/*
			 * We're not in need of a new ap, but idle;
			 * kick off a bg scan to replenish the cache.
			 */
			ieee80211_bg_scan(vap);
		}
	} else {
		/*
		 * Scan cache contents are warm enough to use;
		 * check if a new ap should be used and switch.
		 * XXX deauth current ap
		 */
		if (curRate < roamRate || curRssi < roamRssi) {
			se->base.se_rssi = curRssi;
			selbs = select_bss(ss, vap);
			if (selbs != NULL && selbs != se)
				ieee80211_sta_join(vap, &selbs->base);
		}
	}
}

/*
 * Age entries in the scan cache.
 * XXX also do roaming since it's convenient
 */
static void
sta_age(struct ieee80211_scan_state *ss)
{
	struct ieee80211vap *vap = ss->ss_vap;
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *next;
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		if (se->se_notseen >= STA_PURGE_SCANS) {
			TAILQ_REMOVE(&st->st_entry, se, se_list);
			LIST_REMOVE(se, se_hash);
			free_se_request(se);
			if (st->st_entry_num > 0)
				st->st_entry_num--;
		}
	}
	unlock_sta_table(st, bh_disabled);
	/*
	 * If rate control is enabled check periodically to see if
	 * we should roam from our current connection to one that
	 * might be better.  This only applies when we're operating
	 * in sta mode and automatic roaming is set.
	 * XXX defer if busy
	 * XXX repeater station
	 */
	KASSERT(vap->iv_opmode == IEEE80211_M_STA,
		("wrong mode %u", vap->iv_opmode));
	/* XXX turn this off until the ap release is cut */
	if (0 && vap->iv_ic->ic_roaming == IEEE80211_ROAMING_AUTO &&
	    vap->iv_state >= IEEE80211_S_RUN)
		/* XXX vap is implicit */
		sta_roam_check(ss, vap);
}

/*
 * Remove particular entry from the scan cache,
 * if the sta state changes from RUN to any other state
 */
static void
sta_remove(struct ieee80211_scan_state *ss, struct ieee80211_node *ni)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *next;
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		if (IEEE80211_ADDR_EQ(se->base.se_macaddr, ni->ni_macaddr) &&
			!memcmp(se->base.se_ssid + 2, ni->ni_essid, se->base.se_ssid[1])) {
			TAILQ_REMOVE(&st->st_entry, se, se_list);
			LIST_REMOVE(se, se_hash);
			free_se_request(se);
			if (st->st_entry_num > 0)
				st->st_entry_num--;
		}
	}

	unlock_sta_table(st, bh_disabled);
}

/*
 * Iterate over the entries in the scan cache, invoking
 * the callback function on each one.
 */
static int
sta_iterate(struct ieee80211_scan_state *ss, 
	ieee80211_scan_iter_func *f, void *arg)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;
	u_int gen;
	int res = 0;
	int bh_disabled;

	spin_lock(&st->st_scanlock);
	gen = st->st_scangen++;
restart:
	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH(se, &st->st_entry, se_list) {
		if (se->se_scangen != gen) {
			se->se_scangen = gen;
			/* update public state */
			se->base.se_age = jiffies - se->se_lastupdate;
			/* we are going to use entry after unlocking */
			set_se_inuse(se);
			unlock_sta_table(st, bh_disabled);

			res = (*f)(arg, &se->base);

			bh_disabled = lock_sta_table(st);
			reset_se_inuse(se);
			unlock_sta_table(st, bh_disabled);

			if(res != 0) {
			  /* We probably ran out of buffer space. */
			  goto done;
			}
			goto restart;
		}
	}

	unlock_sta_table(st, bh_disabled);

 done:
	spin_unlock(&st->st_scanlock);

	return res;
}

static void
sta_assoc_fail(struct ieee80211_scan_state *ss,
	const u_int8_t macaddr[IEEE80211_ADDR_LEN], int reason)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;

	se = sta_lookup(st, macaddr);
	if (se != NULL) {
		se->se_fails++;
		se->se_lastfail = jiffies;
		IEEE80211_NOTE_MAC(ss->ss_vap, IEEE80211_MSG_SCAN,
			macaddr, "%s: reason %u fails %u",
			__func__, reason, se->se_fails);
	}
}

static void
sta_assoc_success(struct ieee80211_scan_state *ss,
	const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;

	se = sta_lookup(st, macaddr);
	if (se != NULL) {
		se->se_fails = 0;
		IEEE80211_NOTE_MAC(ss->ss_vap, IEEE80211_MSG_SCAN,
			macaddr, "%s: fails %u", __func__, se->se_fails);
		se->se_lastassoc = jiffies;
	}
}

static const struct ieee80211_scanner sta_default = {
	.scan_name		= "default",
	.scan_attach		= sta_attach,
	.scan_detach		= sta_detach,
	.scan_start		= sta_start,
	.scan_restart		= sta_restart,
	.scan_cancel		= sta_cancel,
	.scan_end		= sta_pick_bss,
	.scan_flush		= sta_flush,
	.scan_add		= sta_add,
	.scan_age		= sta_age,
	.scan_iterate		= sta_iterate,
	.scan_assoc_fail	= sta_assoc_fail,
	.scan_assoc_success	= sta_assoc_success,
	.scan_default		= ieee80211_sta_join,
	.scan_remove            = sta_remove,
};

/*
 * Start an adhoc-mode scan by populating the channel list.
 */
static int
adhoc_start(struct ieee80211_scan_state *ss, struct ieee80211vap *vap)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ieee80211com *ic = vap->iv_ic;
	struct sta_table *st = ss->ss_priv;
	const struct scanlist *scan;
	enum ieee80211_phymode mode;
	
	ss->ss_last = 0;
	/*
	 * Use the table of ordered channels to construct the list
	 * of channels for scanning.  Any channels in the ordered
	 * list not in the master list will be discarded.
	 */
	for (scan = staScanTable; scan->list != NULL; scan++) {
		mode = scan->mode;
		if (ic->ic_des_mode != IEEE80211_MODE_AUTO) {
			/*
			 * If a desired mode was specified, scan only 
			 * channels that satisfy that constraint.
			 */
			if (ic->ic_des_mode != mode) {
				/*
				 * The scan table marks 2.4Ghz channels as b
				 * so if the desired mode is 11g, then use
				 * the 11b channel list but upgrade the mode.
				 */
				if (ic->ic_des_mode != IEEE80211_MODE_11G ||
				    mode != IEEE80211_MODE_11B)
					continue;
				mode = IEEE80211_MODE_11G;	/* upgrade */
			}
		} else {
			/*
			 * This lets ieee80211_scan_add_channels
			 * upgrade an 11b channel to 11g if available.
			 */
			if (mode == IEEE80211_MODE_11B)
				mode = IEEE80211_MODE_AUTO;
		}
		/*
		 * Add the list of the channels; any that are not
		 * in the master channel list will be discarded.
		 */
		add_channels(ic, ss, mode, scan->list, scan->count);
	}
	ss->ss_next = 0;
	/* XXX tunables */
	ss->ss_mindwell = msecs_to_jiffies(200);	/* 200ms */
	ss->ss_maxdwell = msecs_to_jiffies(200);	/* 200ms */

#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(vap)) {
		printf("%s: scan set ", vap->iv_dev->name);
		ieee80211_scan_dump_channels(ss);
		printf(" dwell min %ld max %ld\n",
			ss->ss_mindwell, ss->ss_maxdwell);
	}
#endif /* IEEE80211_DEBUG */

	st->st_newscan = 1;

	return 0;
#undef N
}

/*
 * Select a channel to start an adhoc network on.
 * The channel list was populated with appropriate
 * channels so select one that looks least occupied.
 * XXX need regulatory domain constraints
 */
static struct ieee80211_channel *
adhoc_pick_channel(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;
	struct ieee80211_channel *c, *bestchan;
	int i, bestrssi, maxrssi;
	int bh_disabled;

	bestchan = NULL;
	bestrssi = -1;

	bh_disabled = lock_sta_table(st);
	for (i = 0; i < ss->ss_last; i++) {
		c = ss->ss_chans[i];
		maxrssi = 0;
		TAILQ_FOREACH(se, &st->st_entry, se_list) {
			if (se->base.se_chan != c)
				continue;
			if (se->base.se_rssi > maxrssi)
				maxrssi = se->base.se_rssi;
		}
		if (bestchan == NULL || maxrssi < bestrssi)
			bestchan = c;
	}
	unlock_sta_table(st, bh_disabled);

	return bestchan;
}

/*
 * Pick an ibss network to join or find a channel
 * to use to start an ibss network.
 */
static int
adhoc_pick_bss(struct ieee80211_scan_state *ss, struct ieee80211vap *vap,
	int (*action)(struct ieee80211vap *, const struct ieee80211_scan_entry *),
	u_int32_t flags)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *selbs;
	struct ieee80211_channel *chan;
	struct ieee80211com *ic = vap->iv_ic;

	KASSERT(vap->iv_opmode == IEEE80211_M_IBSS ||
		vap->iv_opmode == IEEE80211_M_AHDEMO,
		("wrong opmode %u", vap->iv_opmode));

	if (st->st_newscan) {
		sta_update_notseen(st);
		st->st_newscan = 0;
	}
	if (ss->ss_flags & IEEE80211_SCAN_NOPICK) {
		/*
		 * Manual/background scan, don't select+join the
		 * bss, just return.  The scanning framework will
		 * handle notification that this has completed.
		 */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		return 1;
	}

	st->st_action = ss->ss_ops->scan_default;
	if (action)
		st->st_action = action;

	/*
	 * Automatic sequencing; look for a candidate and
	 * if found join the network.
	 */
	/* NB: unlocked read should be ok */
	if (TAILQ_FIRST(&st->st_entry) == NULL ||
		(selbs = select_bss(ss, vap)) == NULL ) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: no scan candidate\n", __func__);
		if (vap->iv_des_nssid) {
			/*
			 * No existing adhoc network to join and we have
			 * an ssid; start one up.  If no channel was
			 * specified, try to select a channel.
			 */
			if (ic->ic_des_chan == IEEE80211_CHAN_ANYC)
				chan = adhoc_pick_channel(ss);
			else
				chan = ic->ic_des_chan;
			if (chan != NULL) {
				struct ieee80211_scan_entry se;

				memset(&se, 0, sizeof(se));
				se.se_chan = chan;
				st->st_selbss = se;
				/* defer action */
				IEEE80211_SCHEDULE_TQUEUE(&st->st_actiontq);
				return 1;
			}
		}
		/*
		 * If nothing suitable was found decrement
		 * the failure counts so entries will be
		 * reconsidered the next time around.  We
		 * really want to do this only for sta's
		 * where we've previously had some success.
		 */
		sta_dec_fails(st);
		st->st_newscan = 1;
		return 0;			/* restart scan */
	}

	/* 
	 * Must defer action to avoid possible recursive call through 80211
	 * state machine, which would result in recursive locking.
	 */
	st->st_selbss = selbs->base;
	IEEE80211_SCHEDULE_TQUEUE(&st->st_actiontq);

	return 1;				/* terminate scan */
}

/*
 * Age entries in the scan cache.
 */
static void
adhoc_age(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *next;
	int bh_disabled;

	bh_disabled = lock_sta_table(st);
	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		if (se->se_notseen > STA_PURGE_SCANS) {
			TAILQ_REMOVE(&st->st_entry, se, se_list);
			LIST_REMOVE(se, se_hash);
			free_se_request(se);
			if (st->st_entry_num > 0)
				st->st_entry_num--;
		}
	}
	unlock_sta_table(st, bh_disabled);
}

/*
 * Default action to execute when a scan entry is found for adhoc
 * mode.  Return 1 on success, 0 on failure
 */
static int
adhoc_default_action(struct ieee80211vap *vap,
	const struct ieee80211_scan_entry *se)
{
	u_int8_t zeroMacAddr[IEEE80211_ADDR_LEN];

	memset(&zeroMacAddr, 0, IEEE80211_ADDR_LEN);
	if (IEEE80211_ADDR_EQ(se->se_bssid, &zeroMacAddr[0])) {
		ieee80211_create_bss(vap, se->se_chan);
		return 1;
	} else 
		return ieee80211_sta_join(vap,se);
}

static const struct ieee80211_scanner adhoc_default = {
	.scan_name		= "default",
	.scan_attach		= sta_attach,
	.scan_detach		= sta_detach,
	.scan_start		= adhoc_start,
	.scan_restart		= sta_restart,
	.scan_cancel		= sta_cancel,
	.scan_end		= adhoc_pick_bss,
	.scan_flush		= sta_flush,
	.scan_add		= sta_add,
	.scan_age		= adhoc_age,
	.scan_iterate		= sta_iterate,
	.scan_assoc_fail	= sta_assoc_fail,
	.scan_assoc_success	= sta_assoc_success,
	.scan_default		= adhoc_default_action,
};

static void
action_tasklet(IEEE80211_TQUEUE_ARG data)
{
	struct ieee80211_scan_state *ss = (struct ieee80211_scan_state *)data;
	struct sta_table *st = (struct sta_table *)ss->ss_priv;
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan;

	switch (vap->iv_opmode) {
	case IEEE80211_M_STA:
		sta_dec_fails(st);
		st->st_newscan = 1;
		break;
	default:
		/* ADHOC */
        	if (vap->iv_des_nssid) {
			/*
			 * No existing adhoc network to join and we have
			 * an ssid; start one up.  If no channel was
			 * specified, try to select a channel.
			 */
			if (ic->ic_des_chan == IEEE80211_CHAN_ANYC)
				chan = adhoc_pick_channel(ss);
			else
				chan = ic->ic_des_chan;
			if (chan != NULL) {
				struct ieee80211_scan_entry se;
				
				memset(&se, 0, sizeof(se));
				se.se_chan = chan;
				if ((*ss->ss_ops->scan_default)(vap, &se))
					return;
			}
		}
		/*
		 * If nothing suitable was found decrement
	         * the failure counts so entries will be
		 * reconsidered the next time around.  We
		 * really want to do this only for sta's
		 * where we've previously had some success.
		 */
		sta_dec_fails(st);
		st->st_newscan = 1;
			break;
	}

	/*
	 * restart scan
	 */

	/* no ap, clear the flag for a new scan */
	vap->iv_ic->ic_flags &= ~IEEE80211_F_SCAN;
	if ((ss->ss_flags & IEEE80211_SCAN_USECACHE) == 0)
		(void) ieee80211_start_scan(vap, ss->ss_flags, ss->ss_duration, ss->ss_nssid, ss->ss_ssid);
}

/*
 * Module glue.
 */
MODULE_AUTHOR("Errno Consulting, Sam Leffler");
MODULE_DESCRIPTION("802.11 wireless support: default station scanner");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

static int __init
init_scanner_sta(void)
{
	ieee80211_scanner_register(IEEE80211_M_STA, &sta_default);
	ieee80211_scanner_register(IEEE80211_M_IBSS, &adhoc_default);
	ieee80211_scanner_register(IEEE80211_M_AHDEMO, &adhoc_default);
	return 0;
}
module_init(init_scanner_sta);

static void __exit
exit_scanner_sta(void)
{
	ieee80211_scanner_unregister_all(&sta_default);
	ieee80211_scanner_unregister_all(&adhoc_default);
}
module_exit(exit_scanner_sta);

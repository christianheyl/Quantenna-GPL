/*-
 * Copyright (c) 2001 Atsushi Onoe
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
 * $Id: ieee80211_proto.c 1849 2006-12-08 17:20:08Z proski $
 */
#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 protocol support.
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <asm/board/pm.h>

#include "net80211/if_media.h"

#include "net80211/ieee80211_var.h"
#include "net80211/ieee80211_dot11_msg.h"
#include "net80211/ieee80211_tpc.h"

/* XXX tunables */
#define	AGGRESSIVE_MODE_SWITCH_HYSTERESIS	3	/* pkts / 100ms */
#define	HIGH_PRI_SWITCH_THRESH			10	/* pkts / 100ms */

#define	IEEE80211_RATE2MBS(r)	(((r) & IEEE80211_RATE_VAL) / 2)

const char *ieee80211_mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"action",	"reserved#14",	"reserved#15"
};
EXPORT_SYMBOL(ieee80211_mgt_subtype_name);
const char *ieee80211_ctl_subtype_name[] = {
	"reserved#0",	"reserved#1",	"reserved#2",	"reserved#3",
	"reserved#3",	"reserved#5",	"reserved#6",	"reserved#7",
	"reserved#8",	"reserved#9",	"ps_poll",	"rts",
	"cts",		"ack",		"cf_end",	"cf_end_ack"
};
EXPORT_SYMBOL(ieee80211_ctl_subtype_name);
const char *ieee80211_state_name[IEEE80211_S_MAX] = {
	"INIT",		/* IEEE80211_S_INIT */
	"SCAN",		/* IEEE80211_S_SCAN */
	"AUTH",		/* IEEE80211_S_AUTH */
	"ASSOC",	/* IEEE80211_S_ASSOC */
	"RUN"		/* IEEE80211_S_RUN */
};
EXPORT_SYMBOL(ieee80211_state_name);
const char *ieee80211_wme_acnames[] = {
	"WME_AC_BE",
	"WME_AC_BK",
	"WME_AC_VI",
	"WME_AC_VO",
	"WME_UPSD",
};
EXPORT_SYMBOL(ieee80211_wme_acnames);

extern u_int16_t ht_rate_table_20MHz_800[];
extern u_int16_t ht_rate_table_40MHz_800[];

static int ieee80211_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void ieee80211_tx_timeout(unsigned long);
static void ieee80211_test_traffic_timeout(unsigned long);
void ieee80211_auth_setup(void);

void
ieee80211_proto_attach(struct ieee80211com *ic)
{

	ic->ic_protmode = IEEE80211_PROT_CTSONLY;
	ic->ic_flags_ext |= IEEE80211_FEXT_BG_PROTECT;
	ic->ic_flags_ext |= IEEE80211_FEXT_11N_PROTECT;

	ic->ic_wme.wme_hipri_switch_hysteresis =
		AGGRESSIVE_MODE_SWITCH_HYSTERESIS;

	/* initialize management frame handlers */
	ic->ic_recv_mgmt = ieee80211_recv_mgmt;
	ic->ic_send_mgmt = ieee80211_send_mgmt;
	/* TKIP MIC failure report from the lower layers */
	ic->ic_tkip_mic_failure = ieee80211_tkip_mic_failure;

	ieee80211_auth_setup();
}

void
ieee80211_proto_detach(struct ieee80211com *ic)
{
}

void
ieee80211_proto_vattach(struct ieee80211vap *vap)
{
#ifdef notdef
	vap->iv_rtsthreshold = IEEE80211_RTS_DEFAULT;
#else
	vap->iv_rtsthreshold = IEEE80211_RTS_MAX;
#endif
	vap->iv_fragthreshold = 2346;		/* XXX not used yet */
	vap->iv_fixed_rate = IEEE80211_FIXED_RATE_NONE;
	init_timer(&vap->iv_mgtsend);
	init_timer(&vap->iv_xrvapstart);
	init_timer(&vap->iv_swbmiss);
	init_timer(&vap->iv_swberp);
	init_timer(&vap->iv_test_traffic);
	init_timer(&vap->iv_sta_fast_rejoin);
	vap->iv_mgtsend.function = ieee80211_tx_timeout;
	vap->iv_mgtsend.data = (unsigned long) vap;
	vap->iv_test_traffic.function = ieee80211_test_traffic_timeout;
	vap->iv_test_traffic.data = (unsigned long) vap;
	vap->iv_sta_fast_rejoin.function = ieee80211_sta_fast_rejoin;
	vap->iv_sta_fast_rejoin.data = (unsigned long) vap;
	ieee80211_ppqueue_init(vap);

	vap->bcast_pps.max_bcast_pps = 0; /* Zero indicates no limit on bcst pps */
	vap->bcast_pps.rx_bcast_counter = 0;
	vap->bcast_pps.rx_bcast_pps_start_time = 0;
	vap->bcast_pps.tx_bcast_counter = 0;
	vap->bcast_pps.tx_bcast_pps_start_time = 0;

	/* Initilze the timeout functions for non-HT and non-ERP protection */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		/* Initialize the timeout function for non-HT protection */
		vap->iv_swbmiss.function = ieee80211_swbmiss;
		vap->iv_swbmiss.data = (unsigned long) vap;
		vap->iv_swbmiss_period = IEEE80211_TU_TO_JIFFIES(
						IEEE80211_BINTVAL_DEFAULT * 20);

		/* Initialize the timeout function non-ERP protection */
		vap->iv_swberp.function = ieee80211_swberp;
		vap->iv_swberp.data = (unsigned long) vap;
		vap->iv_swberp_period = IEEE80211_TU_TO_JIFFIES(
						IEEE80211_BINTVAL_DEFAULT * 20);
	}

	/* protocol state change handler */
	vap->iv_newstate = ieee80211_newstate;
}

void
ieee80211_proto_vdetach(struct ieee80211vap *vap)
{
	/*
	 * This should not be needed as we detach when reseting
	 * the state but be conservative here since the
	 * authenticator may do things like spawn kernel threads.
	 */
	if (vap->iv_auth->ia_detach)
		vap->iv_auth->ia_detach(vap);

	/*
	 * Detach any ACL'ator.
	 */
	if (vap->iv_acl != NULL)
		vap->iv_acl->iac_detach(vap);

	IEEE80211_LOCK_IRQ(vap->iv_ic);
	ieee80211_adjust_wme_by_vappri(vap->iv_ic);
	IEEE80211_UNLOCK_IRQ(vap->iv_ic);
}

/*
 * Simple-minded authenticator module support.
 */

#define	IEEE80211_AUTH_MAX	(IEEE80211_AUTH_WPA+1)
/* XXX well-known names */
static const char *auth_modnames[IEEE80211_AUTH_MAX] = {
	"wlan_internal",	/* IEEE80211_AUTH_NONE */
	"wlan_internal",	/* IEEE80211_AUTH_OPEN */
	"wlan_internal",	/* IEEE80211_AUTH_SHARED */
	"wlan_xauth",		/* IEEE80211_AUTH_8021X	 */
	"wlan_internal",	/* IEEE80211_AUTH_AUTO */
	"wlan_xauth",		/* IEEE80211_AUTH_WPA */
};
static const struct ieee80211_authenticator *authenticators[IEEE80211_AUTH_MAX];

static const struct ieee80211_authenticator auth_internal = {
	.ia_name		= "wlan_internal",
	.ia_attach		= NULL,
	.ia_detach		= NULL,
	.ia_node_join		= NULL,
	.ia_node_leave		= NULL,
};

/*
 * Setup internal authenticators once; they are never unregistered.
 */
void
ieee80211_auth_setup(void)
{
	ieee80211_authenticator_register(IEEE80211_AUTH_OPEN, &auth_internal);
	ieee80211_authenticator_register(IEEE80211_AUTH_SHARED, &auth_internal);
	ieee80211_authenticator_register(IEEE80211_AUTH_AUTO, &auth_internal);
}

const struct ieee80211_authenticator *
ieee80211_authenticator_get(int auth)
{
	if (auth >= IEEE80211_AUTH_MAX)
		return NULL;
	if (authenticators[auth] == NULL)
		ieee80211_load_module(auth_modnames[auth]);
	return authenticators[auth];
}

void
ieee80211_authenticator_register(int type,
	const struct ieee80211_authenticator *auth)
{
	if (type >= IEEE80211_AUTH_MAX)
		return;
	authenticators[type] = auth;
}
EXPORT_SYMBOL(ieee80211_authenticator_register);

void
ieee80211_authenticator_unregister(int type)
{
	if (type >= IEEE80211_AUTH_MAX)
		return;
	authenticators[type] = NULL;
}
EXPORT_SYMBOL(ieee80211_authenticator_unregister);

/*
 * Very simple-minded authenticator backend module support.
 */
/* XXX just one for now */
static	const struct ieee80211_authenticator_backend *backend = NULL;

void
ieee80211_authenticator_backend_register(
	const struct ieee80211_authenticator_backend *be)
{
	printk(KERN_INFO "wlan: %s backend registered\n", be->iab_name);
	backend = be;
}
EXPORT_SYMBOL(ieee80211_authenticator_backend_register);

void
ieee80211_authenticator_backend_unregister(
	const struct ieee80211_authenticator_backend * be)
{
	if (backend == be)
		backend = NULL;
	printk(KERN_INFO "wlan: %s backend unregistered\n",
		be->iab_name);
}
EXPORT_SYMBOL(ieee80211_authenticator_backend_unregister);

const struct ieee80211_authenticator_backend *
ieee80211_authenticator_backend_get(const char *name)
{
	if (backend == NULL)
		ieee80211_load_module("wlan_radius");
	return backend && strcmp(backend->iab_name, name) == 0 ? backend : NULL;
}
EXPORT_SYMBOL(ieee80211_authenticator_backend_get);

/*
 * Very simple-minded ACL module support.
 */
/* XXX just one for now */
static const struct ieee80211_aclator *acl = NULL;

void
ieee80211_aclator_register(const struct ieee80211_aclator *iac)
{
	printk(KERN_INFO "wlan: %s acl policy registered\n", iac->iac_name);
	acl = iac;
}
EXPORT_SYMBOL(ieee80211_aclator_register);

void
ieee80211_aclator_unregister(const struct ieee80211_aclator *iac)
{
	if (acl == iac)
		acl = NULL;
	printk(KERN_INFO "wlan: %s acl policy unregistered\n", iac->iac_name);
}
EXPORT_SYMBOL(ieee80211_aclator_unregister);

const struct ieee80211_aclator *
ieee80211_aclator_get(const char *name)
{
	if (acl == NULL)
		ieee80211_load_module("wlan_acl");
	return acl && strcmp(acl->iac_name, name) == 0 ? acl : NULL;
}
EXPORT_SYMBOL(ieee80211_aclator_get);

void
ieee80211_print_essid(const u_int8_t *essid, int len)
{
	int i;
	const u_int8_t *p; 

	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;
	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	} else {
		printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%02x", *p);
	}
}
EXPORT_SYMBOL(ieee80211_print_essid);

void
ieee80211_dump_pkt(struct ieee80211com *ic,
	const u_int8_t *buf, int len, int rate, int rssi)
{
	const struct ieee80211_frame *wh;
	int i;

	wh = (const struct ieee80211_frame *)buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		printf("NODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		printf("TODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s)", ether_sprintf(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		printf("FRDS %s", ether_sprintf(wh->i_addr3));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		printf("DSDS %s", ether_sprintf((u_int8_t *)&wh[1]));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s", ether_sprintf(wh->i_addr2));
		printf("->%s)", ether_sprintf(wh->i_addr1));
		break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		printf(" data");
		break;
	case IEEE80211_FC0_TYPE_MGT:
		printf(" %s", ieee80211_mgt_subtype_name[
			(wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
			>> IEEE80211_FC0_SUBTYPE_SHIFT]);
		break;
	default:
		printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
		break;
	}
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		const struct ieee80211_qosframe *qwh = 
			(const struct ieee80211_qosframe *)buf;
		printf(" QoS [TID %u%s]", qwh->i_qos[0] & IEEE80211_QOS_TID,
			qwh->i_qos[0] & IEEE80211_QOS_ACKPOLICY ? " ACM" : "");
	}
	if (wh->i_fc[1] & IEEE80211_FC1_PROT) {
		int off;

		off = ieee80211_anyhdrspace(ic, wh);
		printf(" WEP [IV %.02x %.02x %.02x",
			buf[off+0], buf[off+1], buf[off+2]);
		if (buf[off+IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)
			printf(" %.02x %.02x %.02x",
				buf[off+4], buf[off+5], buf[off+6]);
		printf(" KID %u]", buf[off+IEEE80211_WEP_IVLEN] >> 6);
	}
	if (rate >= 0)
		printf(" %dM", rate / 2);
	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");
	if (len > 0) {
		for (i = 0; i < len; i++) {
			if ((i % 8) == 0)
				printf(" ");
			if ((i % 16) == 0)
				printf("\n");
			printf("%02x ", buf[i]);
		}
		printf("\n\n");
	}
}
EXPORT_SYMBOL(ieee80211_dump_pkt);

int
ieee80211_fix_ht_rate(struct ieee80211_node *ni, int flags)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	int8_t i = 0, j = 0, k = 0;
	int val = 0, rridx = 0;
	u_int8_t rs[IEEE80211_HT_MAXMCS_SET_SUPPORTED] = {0};
	u_int8_t fixed_rate = 0;

	/* check if intersection is required */
	if (flags & IEEE80211_F_DOXSECT)
	{
		k = 0;
		ni->ni_htrates.rs_nrates = 0;

		for (i = 0; i < IEEE80211_HT_MAXMCS_BASICSET_SUPPORTED; i++)
		{
			rs[i] = ni->ni_htcap.mcsset[i];
			rs[i] &= ic->ic_htcap.mcsset[i];

			for(j = 0, val = 0x1; j < 8; j++, val = val << 1)
			{
				if (rs[i] & val)
				{
					ni->ni_htrates.rs_rates[k] = IEEE80211_HT_RATE_TABLE_IDX(i,j);
					ni->ni_htrates.rs_nrates++;
					if ((vap->iv_fixed_rate & 0x80)&& (vap->iv_fixed_rate == (ni->ni_htrates.rs_rates[k]|0x80)))
   	                                     fixed_rate = vap->iv_fixed_rate & 0x7F;
					k++;
				}
				
				if ((vap->iv_fixed_rate & 0x80)&& (vap->iv_fixed_rate == (ni->ni_htrates.rs_rates[k]|0x80)))
					fixed_rate = vap->iv_fixed_rate & 0x7F;
			}
		}

		/* sort the rates in ascending order */
		for (i = 0; i < (ni->ni_htrates.rs_nrates - 1); i++)
		{
			for (j = i + 1; j < ni->ni_htrates.rs_nrates; j++)
			{
				int tempi, tempj;
				tempi = ni->ni_htrates.rs_rates[i];
				tempj = ni->ni_htrates.rs_rates[j];

				if(IEEE80211_IS_CHAN_11N(ic->ic_bsschan))
				{
					if (ht_rate_table_20MHz_800[tempi] > ht_rate_table_20MHz_800[tempj])
					{
						ni->ni_htrates.rs_rates[i] = tempj;
						ni->ni_htrates.rs_rates[j] = tempi;
					}
				}
				else
				{
					if (ht_rate_table_40MHz_800[tempi] > ht_rate_table_40MHz_800[tempj])
					{
						ni->ni_htrates.rs_rates[i] = tempj;
						ni->ni_htrates.rs_rates[j] = tempi;
					}
				}
			}
		}

		rridx = ni->ni_htrates.rs_rates[ni->ni_htrates.rs_nrates - 1];
	}

	/* check if basic rate set is required */
	for (i = 0; i < IEEE80211_HT_MAXMCS_BASICSET_SUPPORTED; i++)
	{
		if ((ni->ni_htcap.mcsset[i] & ic->ic_htinfo.basicmcsset[i]) == ic->ic_htinfo.basicmcsset[i])
		{
			if (flags & IEEE80211_F_DOBRS)
				rs[i] &= ic->ic_htinfo.basicmcsset[i];
		}
		else
		{
			/* basic rate not supported */
			return 0;
		}
	}

	/* keep only those rates that are supported by both STAs */
	if (flags & IEEE80211_F_DODEL)
	{
		for (i = 0; i < IEEE80211_HT_MAXMCS_SET_SUPPORTED; i++)
		{
			ni->ni_htcap.mcsset[i] &= ic->ic_htcap.mcsset[i];
		}
	}

	if (flags & IEEE80211_F_DOFRATE)
	{
		if (fixed_rate != 0)
			return fixed_rate;
		else
			return 0;
	}

	return rridx;
}

int
ieee80211_fix_rate(struct ieee80211_node *ni, int flags)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	int i, j, ignore, error;
	int okrate, badrate, fixedrate;
	struct ieee80211_rateset *srs, *nrs;
	u_int8_t r;

	error = 0;
	okrate = badrate = fixedrate = 0;
	/* Supported rates are depends on the mode setting */
	srs = &ic->ic_sup_rates[ic->ic_curmode];
	nrs = &ni->ni_rates;
	fixedrate = IEEE80211_FIXED_RATE_NONE;
	for (i = 0; i < nrs->rs_nrates; ) {
		ignore = 0;
		if (flags & IEEE80211_F_DOSORT) {
			/*
			 * Sort rates.
			 */
			for (j = i + 1; j < nrs->rs_nrates; j++) {
				if (RV(nrs->rs_rates[i]) > RV(nrs->rs_rates[j])) {
					r = nrs->rs_rates[i];
					nrs->rs_rates[i] = nrs->rs_rates[j];
					nrs->rs_rates[j] = r;
				}
			}
		}
		r = nrs->rs_rates[i] & IEEE80211_RATE_VAL;
		badrate = r;
		if (flags & IEEE80211_F_DONEGO) {
			/*
			 * Check against supported rates.
			 */
			for (j = 0; j < srs->rs_nrates; j++) {
				if (r == RV(srs->rs_rates[j])) {
					/*
					 * Overwrite with the supported rate
					 * value so any basic rate bit is set.
					 * This ensures that response we send
					 * to stations have the necessary basic
					 * rate bit set.
					 */
					nrs->rs_rates[i] = srs->rs_rates[j];
					break;
				}
			}
			if (j == srs->rs_nrates) {
				/*
				 * A rate in the node's rate set is not
				 * supported.  If this is a basic rate and we
				 * are operating as an AP then this is an error.
				 * Otherwise we just discard/ignore the rate.
				 * Note that this is important for 11b stations
				 * when they want to associate with an 11g AP.
				 */
				if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
				    (nrs->rs_rates[i] & IEEE80211_RATE_BASIC))
					error++;
				ignore++;
			}
		}
		if (flags & IEEE80211_F_DODEL) {
			/*
			 * Delete unacceptable rates.
			 */
			if (ignore) {
				nrs->rs_nrates--;
				for (j = i; j < nrs->rs_nrates; j++)
					nrs->rs_rates[j] = nrs->rs_rates[j + 1];
				nrs->rs_rates[j] = 0;
				continue;
			}
		}
		if (!ignore)
			okrate = nrs->rs_rates[i];
		i++;
	}

	if (okrate == 0 || error != 0)
		return badrate | IEEE80211_RATE_BASIC;
	else
		return RV(okrate);
#undef RV
}

/*
 * Reset 11g-related state.
 */
void
ieee80211_reset_erp(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
#define	IS_11G(m) \
	((m) == IEEE80211_MODE_11G || (m) == IEEE80211_MODE_TURBO_G)

	ic->ic_flags &= ~IEEE80211_F_USEPROT;
	/*
	 * Preserve the long slot and nonerp station count if
	 * switching between 11g and turboG. Otherwise, inactivity
	 * will cause the turbo station to disassociate and possibly
	 * try to leave the network.
	 * XXX not right if really trying to reset state
	 */
	if (IS_11G(mode) ^ IS_11G(ic->ic_curmode)) {
		ic->ic_nonerpsta = 0;
		ic->ic_longslotsta = 0;
	}

	/*
	 * Short slot time is enabled only when operating in 11g
	 * and not in an IBSS.  We must also honor whether or not
	 * the driver is capable of doing it.
	 */
	ieee80211_set_shortslottime(ic,
		IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
		(IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
		ic->ic_opmode == IEEE80211_M_HOSTAP &&
		(ic->ic_caps & IEEE80211_C_SHSLOT)));
	/*
	 * Set short preamble and ERP barker-preamble flags.
	 */
	if (IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
	    (ic->ic_caps & IEEE80211_C_SHPREAMBLE)) {
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ic->ic_flags &= ~IEEE80211_F_USEBARKER;
	} else {
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		ic->ic_flags |= IEEE80211_F_USEBARKER;
	}
#undef IS_11G
}

/*
 * Set the short slot time state and notify the driver.
 */
void
ieee80211_set_shortslottime(struct ieee80211com *ic, int onoff)
{
	if (onoff)
		ic->ic_flags |= IEEE80211_F_SHSLOT;
	else
		ic->ic_flags &= ~IEEE80211_F_SHSLOT;
	/* notify driver */
	if (ic->ic_updateslot != NULL)
		ic->ic_updateslot(ic);
}

/*
 * Check if the specified rate set supports ERP.
 * 6, 12 and 24 are the mandantory ERP rates
 */
int
ieee80211_iserp_rateset(struct ieee80211com *ic, struct ieee80211_rateset *rs)
{
	static const int erp_rates[] = { 12, 24, 48 };
	int i, j;

	for (i = 0; i < ARRAY_SIZE(erp_rates); i++) {
		for (j = 0; j < rs->rs_nrates; j++) {
			int r = rs->rs_rates[j] & IEEE80211_RATE_VAL;
			if (erp_rates[i] == r) {
				goto next;
			}
		}
		return 0;
	next:
		;
	}
	return 1;
}

static const struct ieee80211_rateset basic11g[IEEE80211_MODE_MAX] = {
    { 0 },			/* IEEE80211_MODE_AUTO */
    { 3, 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11A */
    { 2, 2, { 2, 4 } },		/* IEEE80211_MODE_11B */
    { 4, 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_11G (mixed b/g) */
    { 0, 0 },			/* IEEE80211_MODE_FH */
    { 3, 3, { 12, 24, 48 } },	/* IEEE80211_MODE_TURBO_A */
    { 4, 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_TURBO_G (mixed b/g) */
};

/*
 * Mark the basic rates for the 11g rate table based on the
 * specified mode.  For 11b compatibility we mark only 11b
 * rates.  There's also a pseudo 11a-mode used to mark only
 * the basic OFDM rates; this is used to exclude 11b stations
 * from an 11g bss.
 */
void
ieee80211_set11gbasicrates(struct ieee80211_rateset *rs, enum ieee80211_phymode mode)
{
	int i, j;

	KASSERT(mode < IEEE80211_MODE_MAX, ("invalid mode %u", mode));
	for (i = 0; i < rs->rs_nrates; i++) {
		rs->rs_rates[i] &= IEEE80211_RATE_VAL;
		for (j = 0; j < basic11g[mode].rs_nrates; j++)
			if (basic11g[mode].rs_rates[j] == rs->rs_rates[i]) {
				rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
				break;
			}
	}
}

/*
 * Deduce the 11g setup by examining the rates
 * that are marked basic.
 */
enum ieee80211_phymode
ieee80211_get11gbasicrates(struct ieee80211_rateset *rs)
{
	struct ieee80211_rateset basic;
	int i;

	memset(&basic, 0, sizeof(basic));
	for (i = 0; i < rs->rs_nrates; i++)
		if (rs->rs_rates[i] & IEEE80211_RATE_BASIC)
			basic.rs_rates[basic.rs_nrates++] =
				rs->rs_rates[i] & IEEE80211_RATE_VAL;
	for (i = 0; i < IEEE80211_MODE_MAX; i++)
		if (memcmp(&basic, &basic11g[i], sizeof(basic)) == 0)
			return i;
	return IEEE80211_MODE_AUTO;
}

struct ieee80211_wme_state *ieee80211_vap_get_wmestate(struct ieee80211vap *vap)
{
	if (vap->iv_opmode != IEEE80211_M_STA) {
		return &vap->iv_wme;
	} else {
		return &vap->iv_ic->ic_wme;
	}
}
EXPORT_SYMBOL(ieee80211_vap_get_wmestate);

void ieee80211_vap_sync_chan_wmestate(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *vap1;

	if (vap->iv_opmode == IEEE80211_M_STA)
		return;

	/* sync to global */
	memcpy(&ic->ic_wme.wme_wmeChanParams, &vap->iv_wme.wme_wmeChanParams,
			sizeof(ic->ic_wme.wme_wmeChanParams));
	memcpy(&ic->ic_wme.wme_chanParams, &vap->iv_wme.wme_chanParams,
			sizeof(ic->ic_wme.wme_chanParams));

	/*
	 * Sync wme chan params across all SSID since we don't support per SSID wme chan params.
	 * We only support per SSID wme bss params.
	 */
	TAILQ_FOREACH(vap1, &ic->ic_vaps, iv_next) {
		if (vap1 == vap)
			continue;
		memcpy(&vap1->iv_wme.wme_wmeChanParams, &vap->iv_wme.wme_wmeChanParams,
				sizeof(vap1->iv_wme.wme_wmeChanParams));
		memcpy(&vap1->iv_wme.wme_chanParams, &vap->iv_wme.wme_chanParams,
				sizeof(vap1->iv_wme.wme_chanParams));
	}
}
EXPORT_SYMBOL(ieee80211_vap_sync_chan_wmestate);

/*
 * Automatically change the wmm params based on how many SSID priorities are used:
 *  1. If all SSID has same priority, then apply default wmm params for all SSID.
 *  2. Otherwise, automatically apply different wmm param set:
 *     SSID priority    wmm bss params
 *     3                all same as original AC_VO
 *     2                all same as original AC_VI
 *     1                all same as original AC_BE
 *     0                all same as original AC_BK
 *  Please note ic lock shall be held when calling this function.
 */
void ieee80211_adjust_wme_by_vappri(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	uint8_t vap_pri;
	uint8_t use_default = 1;
	struct ieee80211_wme_state *wme_dft = &ic->ic_wme;
	struct ieee80211_wme_state *wme;
	uint8_t ac;
	static uint8_t vappri_to_ac[QTN_VAP_PRIORITY_NUM] = {WMM_AC_BK, WMM_AC_BE, WMM_AC_VI, WMM_AC_VO};
	uint8_t mapped_ac;

	vap = TAILQ_FIRST(&ic->ic_vaps);
	if (unlikely(vap == NULL))
		return;

	if (vap->iv_opmode == IEEE80211_M_STA)
		return;

	if (ic->ic_vap_pri_wme) {
		vap_pri = vap->iv_pri;
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if (vap->iv_pri != vap_pri)
				use_default = 0;
		}
	}

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		wme = &vap->iv_wme;
		if (use_default) {
			/* apply default param set: different for different AC */
			memcpy(&wme->wme_wmeBssChanParams.cap_wmeParams,
				&wme_dft->wme_wmeBssChanParams.cap_wmeParams,
				sizeof(wme->wme_wmeBssChanParams.cap_wmeParams));
		} else {
			/* determine wmm params based on vap priority */
			mapped_ac = vappri_to_ac[vap->iv_pri];
			for (ac = 0; ac < WMM_AC_NUM; ac++) {
				memcpy(&wme->wme_wmeBssChanParams.cap_wmeParams[ac],
					&wme_dft->wme_wmeBssChanParams.cap_wmeParams[mapped_ac],
					sizeof(wme->wme_wmeBssChanParams.cap_wmeParams[0]));
			}
		}
		wme->wme_wmeBssChanParams.cap_info_count++;
		ieee80211_wme_updateparams(vap, 0);
	}
}

void
ieee80211_wme_initparams(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	ieee80211_wme_initparams_locked(vap);
	ieee80211_adjust_wme_by_vappri(ic);
	IEEE80211_UNLOCK(ic);
}

void
ieee80211_wme_initparams_locked(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
	typedef struct phyParamType {
		u_int8_t aifsn;
		u_int8_t logcwmin;
		u_int8_t logcwmax;
		u_int16_t txopLimit;
		u_int8_t acm;
	} paramType;
	enum ieee80211_phymode mode;

	paramType *pPhyParam, *pBssPhyParam;

	static struct phyParamType phyParamForAC_BE[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 3, 4,  6,   0, 0 },
	/* IEEE80211_MODE_11A   */ { 3, 4,  6,   0, 0 },
	/* IEEE80211_MODE_11B   */ { 3, 5,  7,   0, 0 },
	/* IEEE80211_MODE_11G   */ { 3, 4,  6,   0, 0 },
	/* IEEE80211_MODE_FH    */ { 3, 5,  7,   0, 0 },
	/* IEEE80211_MODE_TURBO */ { 2, 3,  5,   0, 0 },
	/* IEEE80211_MODE_TURBO */ { 2, 3,  5,   0, 0 }};
	static struct phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 7, 4, 10,   0, 0 },
	/* IEEE80211_MODE_11A   */ { 7, 4, 10,   0, 0 },
	/* IEEE80211_MODE_11B   */ { 7, 5, 10,   0, 0 },
	/* IEEE80211_MODE_11G   */ { 7, 4, 10,   0, 0 },
	/* IEEE80211_MODE_FH    */ { 7, 5, 10,   0, 0 },
	/* IEEE80211_MODE_TURBO */ { 7, 3, 10,   0, 0 },
	/* IEEE80211_MODE_TURBO */ { 7, 3, 10,   0, 0 }};
	static struct phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 1, 3,  4, 188, 0 },
	/* IEEE80211_MODE_11A   */ { 1, 3,  4,  94, 0 },
	/* IEEE80211_MODE_11B   */ { 1, 4,  5, 188, 0 },
	/* IEEE80211_MODE_11G   */ { 1, 3,  4,  94, 0 },
	/* IEEE80211_MODE_FH    */ { 1, 4,  5, 188, 0 },
	/* IEEE80211_MODE_TURBO */ { 1, 2,  3,  94, 0 },
	/* IEEE80211_MODE_TURBO */ { 1, 2,  3,  94, 0 }};
	static struct phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 1, 2,  3, 188, 0 },
	/* IEEE80211_MODE_11A   */ { 1, 2,  3,  47, 0 },
	/* IEEE80211_MODE_11B   */ { 1, 3,  4, 102, 0 },
	/* IEEE80211_MODE_11G   */ { 1, 2,  3,  47, 0 },
	/* IEEE80211_MODE_FH    */ { 1, 3,  4, 102, 0 },
	/* IEEE80211_MODE_TURBO */ { 1, 2,  2,  47, 0 },
	/* IEEE80211_MODE_TURBO */ { 1, 2,  2,  47, 0 }};

	static struct phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 3, 4, 10,   0, 0 },
	/* IEEE80211_MODE_11A   */ { 3, 4, 10,   0, 0 },
	/* IEEE80211_MODE_11B   */ { 3, 5, 10,   0, 0 },
	/* IEEE80211_MODE_11G   */ { 3, 4, 10,   0, 0 },
	/* IEEE80211_MODE_FH    */ { 3, 5, 10,   0, 0 },
	/* IEEE80211_MODE_TURBO */ { 2, 3, 10,   0, 0 },
	/* IEEE80211_MODE_TURBO */ { 2, 3, 10,   0, 0 }};
	static struct phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 2, 3,  4,  94, 0 },
	/* IEEE80211_MODE_11A   */ { 2, 3,  4,  94, 0 },
	/* IEEE80211_MODE_11B   */ { 2, 4,  5, 188, 0 },
	/* IEEE80211_MODE_11G   */ { 2, 3,  4,  94, 0 },
	/* IEEE80211_MODE_FH    */ { 2, 4,  5, 188, 0 },
	/* IEEE80211_MODE_TURBO */ { 2, 2,  3,  94, 0 },
	/* IEEE80211_MODE_TURBO */ { 2, 2,  3,  94, 0 }};
	static struct phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = {
	/* IEEE80211_MODE_AUTO  */ { 2, 2,  3,  47, 0 },
	/* IEEE80211_MODE_11A   */ { 2, 2,  3,  47, 0 },
	/* IEEE80211_MODE_11B   */ { 2, 3,  4, 102, 0 },
	/* IEEE80211_MODE_11G   */ { 2, 2,  3,  47, 0 },
	/* IEEE80211_MODE_FH    */ { 2, 3,  4, 102, 0 },
	/* IEEE80211_MODE_TURBO */ { 1, 2,  2,  47, 0 },
	/* IEEE80211_MODE_TURBO */ { 1, 2,  2,  47, 0 }};

	int i;

	IEEE80211_LOCK_ASSERT(ic);

	mode = IEEE80211_MODE_AUTO;
	for (i = 0; i < WME_NUM_AC; i++) {
		switch (i) {
		case WME_AC_BK:
			pPhyParam = &phyParamForAC_BK[mode];
			pBssPhyParam = &phyParamForAC_BK[mode];
			break;
		case WME_AC_VI:
			pPhyParam = &phyParamForAC_VI[mode];
			pBssPhyParam = &bssPhyParamForAC_VI[mode];
			break;
		case WME_AC_VO:
			pPhyParam = &phyParamForAC_VO[mode];
			pBssPhyParam = &bssPhyParamForAC_VO[mode];
			break;
		case WME_AC_BE:
		default:
			pPhyParam = &phyParamForAC_BE[mode];
			pBssPhyParam = &bssPhyParamForAC_BE[mode];
			break;
		}

		if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
				vap->iv_opmode == IEEE80211_M_WDS) {
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_acm =
				pPhyParam->acm;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_aifsn =
				pPhyParam->aifsn;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmin =
				pPhyParam->logcwmin;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmax =
				pPhyParam->logcwmax;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_txopLimit =
				pPhyParam->txopLimit;
		} else {
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_acm =
				pBssPhyParam->acm;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_aifsn =
				pBssPhyParam->aifsn;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmin =
				pBssPhyParam->logcwmin;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmax =
				pBssPhyParam->logcwmax;
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_txopLimit =
				pBssPhyParam->txopLimit;
		}
		wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_acm =
			pBssPhyParam->acm;
		wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_aifsn =
			pBssPhyParam->aifsn;
		wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_logcwmin =
			pBssPhyParam->logcwmin;
		wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_logcwmax =
			pBssPhyParam->logcwmax;
		wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_txopLimit =
			pBssPhyParam->txopLimit;
	}

	for (i = 0; i < WME_NUM_AC; i++) {
		wme->wme_chanParams.cap_wmeParams[i].wmm_aifsn =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_aifsn;
		wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmin =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmin;
		wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmax =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmax;
		wme->wme_chanParams.cap_wmeParams[i].wmm_txopLimit =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_txopLimit;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_aifsn =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_aifsn;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmin =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_logcwmin;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmax =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_logcwmax;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_txopLimit =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_txopLimit;
	}
	/* Set version to 1 so all STAs will pick up the AP parameters */
	wme->wme_bssChanParams.cap_info_count = 1;
	wme->wme_wmeBssChanParams.cap_info_count = 1;
	wme->wme_chanParams.cap_info_count = 1;
	wme->wme_wmeChanParams.cap_info_count = 1;

	if (vap->iv_opmode != IEEE80211_M_STA)
		memcpy(&ic->ic_wme, &vap->iv_wme, sizeof(ic->ic_wme));
}

/*
 * Update WME parameters for ourself and the BSS.
 */
void
ieee80211_wme_updateparams_locked(struct ieee80211vap *vap)
{
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
	int i;

	/* set up the channel access parameters for the physical device */
	for (i = 0; i < WME_NUM_AC; i++) {
		wme->wme_chanParams.cap_wmeParams[i].wmm_acm =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_acm;
		wme->wme_chanParams.cap_wmeParams[i].wmm_aifsn =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_aifsn;
		wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmin =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmin;
		wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmax =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_logcwmax;
		wme->wme_chanParams.cap_wmeParams[i].wmm_txopLimit =
			wme->wme_wmeChanParams.cap_wmeParams[i].wmm_txopLimit;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_acm =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_acm;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_aifsn =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_aifsn;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmin =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_logcwmin;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmax =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_logcwmax;
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_txopLimit =
			wme->wme_wmeBssChanParams.cap_wmeParams[i].wmm_txopLimit;
	}
	wme->wme_bssChanParams.cap_info_count = wme->wme_wmeBssChanParams.cap_info_count;
	wme->wme_chanParams.cap_info_count = wme->wme_wmeChanParams.cap_info_count;

	/* For AP, wrap cap info count - new parameters may be broadcast */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
#define IEEE80211_MAX_CAP_INFO_COUNT 0xF
		if (wme->wme_chanParams.cap_info_count > IEEE80211_MAX_CAP_INFO_COUNT) {
			wme->wme_wmeChanParams.cap_info_count = 0;
			wme->wme_chanParams.cap_info_count = 0;
		}
		if (wme->wme_bssChanParams.cap_info_count > IEEE80211_MAX_CAP_INFO_COUNT) {
			wme->wme_wmeBssChanParams.cap_info_count = 0;
			wme->wme_bssChanParams.cap_info_count = 0;
		}
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
		"%s: WME params updated, cap_info 0x%x\n", __func__,
		vap->iv_opmode == IEEE80211_M_STA ?
			wme->wme_wmeChanParams.cap_info_count :
			wme->wme_bssChanParams.cap_info_count);
}

void
ieee80211_wme_updateparams(struct ieee80211vap *vap, int sync_chan_wme)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	ieee80211_wme_updateparams_locked(vap);
	if (sync_chan_wme)
		ieee80211_vap_sync_chan_wmestate(vap);
	IEEE80211_UNLOCK(ic);

	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
		(vap->iv_state == IEEE80211_S_RUN)) {

		ic->ic_beacon_update(vap);
	} else if ((vap->iv_opmode == IEEE80211_M_STA) &&
		(vap->iv_state == IEEE80211_S_RUN)) {

		ic->ic_wmm_params_update(vap);
	}
}
EXPORT_SYMBOL(ieee80211_wme_updateparams);

/*
 * Update WME parameters with deltas, for ourself and the BSS.
 */
void
ieee80211_wme_updateparams_delta_locked(struct ieee80211vap *vap)
{
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);
	static const uint8_t local_aifsn_min[WME_NUM_AC] = IEEE80211_DYN_WMM_LOCAL_AIFS_MIN;
	static const uint8_t local_cwmin_min[WME_NUM_AC] = IEEE80211_DYN_WMM_LOCAL_CWMIN_MIN;
	static const uint8_t local_cwmax_min[WME_NUM_AC] = IEEE80211_DYN_WMM_LOCAL_CWMAX_MIN;
	static const uint8_t bss_aifsn_max[WME_NUM_AC] = IEEE80211_DYN_WMM_BSS_AIFS_MAX;
	static const uint8_t bss_cwmin_max[WME_NUM_AC] = IEEE80211_DYN_WMM_BSS_CWMIN_MAX;
	static const uint8_t bss_cwmax_max[WME_NUM_AC] = IEEE80211_DYN_WMM_BSS_CWMAX_MAX;
	int i;

	/* set up the channel access parameters for the physical device */
	for (i = 0; i < WME_NUM_AC; i++) {
		wme->wme_chanParams.cap_wmeParams[i].wmm_aifsn =
			MAX(wme->wme_chanParams.cap_wmeParams[i].wmm_aifsn +
				IEEE80211_DYN_WMM_LOCAL_AIFS_DELTA,
				local_aifsn_min[i]);
		wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmin =
			MAX(wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmin +
				IEEE80211_DYN_WMM_LOCAL_CWMIN_DELTA,
				local_cwmin_min[i]);
		wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmax =
			MAX(wme->wme_chanParams.cap_wmeParams[i].wmm_logcwmax +
				IEEE80211_DYN_WMM_LOCAL_CWMAX_DELTA,
				local_cwmax_min[i]);
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_aifsn =
			MIN(wme->wme_bssChanParams.cap_wmeParams[i].wmm_aifsn +
				IEEE80211_DYN_WMM_BSS_AIFS_DELTA,
				bss_aifsn_max[i]);
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmin =
			MIN(wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmin +
				IEEE80211_DYN_WMM_BSS_CWMIN_DELTA,
				bss_cwmin_max[i]);
		wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmax =
			MIN(wme->wme_bssChanParams.cap_wmeParams[i].wmm_logcwmax +
				IEEE80211_DYN_WMM_BSS_CWMAX_DELTA,
				bss_cwmax_max[i]);
	}
	wme->wme_bssChanParams.cap_info_count = wme->wme_wmeBssChanParams.cap_info_count;
	wme->wme_chanParams.cap_info_count = wme->wme_wmeChanParams.cap_info_count;

	/* For AP, wrap cap info count - new parameters may be broadcast */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
#define IEEE80211_MAX_CAP_INFO_COUNT 0xF
		if (wme->wme_chanParams.cap_info_count > IEEE80211_MAX_CAP_INFO_COUNT) {
			wme->wme_wmeChanParams.cap_info_count = 0;
			wme->wme_chanParams.cap_info_count = 0;
		}
		if (wme->wme_bssChanParams.cap_info_count > IEEE80211_MAX_CAP_INFO_COUNT) {
			wme->wme_wmeBssChanParams.cap_info_count = 0;
			wme->wme_bssChanParams.cap_info_count = 0;
		}
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
		"%s: WME params updated, cap_info 0x%x\n", __func__,
		vap->iv_opmode == IEEE80211_M_STA ?
			wme->wme_wmeChanParams.cap_info_count :
			wme->wme_bssChanParams.cap_info_count);
}

void
ieee80211_wme_updateparams_delta(struct ieee80211vap *vap, uint8_t apply_delta)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_wme_state *wme = ieee80211_vap_get_wmestate(vap);

	IEEE80211_LOCK(ic);
	wme->wme_wmeBssChanParams.cap_info_count++;
	wme->wme_wmeChanParams.cap_info_count++;
	if (apply_delta) {
		ieee80211_wme_updateparams_delta_locked(vap);
	} else {
		ieee80211_wme_updateparams_locked(vap);
	}
	/* sync to global */
	memcpy(&ic->ic_wme.wme_chanParams, &vap->iv_wme.wme_chanParams,
			sizeof(ic->ic_wme.wme_chanParams));
	IEEE80211_UNLOCK(ic);

	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
		(vap->iv_state == IEEE80211_S_RUN)) {

		ic->ic_beacon_update(vap);
	} else if ((vap->iv_opmode == IEEE80211_M_STA) &&
		(vap->iv_state == IEEE80211_S_RUN)) {

		ic->ic_wmm_params_update(vap);
	}
}
EXPORT_SYMBOL(ieee80211_wme_updateparams_delta);


/*
 * Start a vap.  If this is the first vap running on the
 * underlying device then we first bring it up.
 */
int
ieee80211_init(struct net_device *dev, int forcescan)
{
#define	IS_RUNNING(_dev) \
	((_dev->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
	struct ieee80211vap *vap = netdev_priv(dev);
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_DPRINTF(vap,
		IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"start running (state=%d)\n", vap->iv_state);

	if (ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] >= BOARD_PM_LEVEL_DUTY) {
		/* wake up the device once it is brought up */
		pm_qos_update_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_WLAN, BOARD_PM_LEVEL_NO);
		ieee80211_pm_queue_work(ic);
	}

	if ((dev->flags & IFF_RUNNING) == 0) {
		ic->ic_init(ic);

		/*
		 * Mark us running.  Note that we do this after
		 * opening the parent device to avoid recursion.
		 */
		dev->flags |= IFF_RUNNING;		/* mark us running */
	}

	/*
	 * If the parent is up and running, then kick the
	 * 802.11 state machine as appropriate.
	 * XXX parent should always be up+running
	 */
	if (vap->iv_opmode == IEEE80211_M_STA) {
		/*
		 * Try to be intelligent about clocking the state
		 * machine.  If we're currently in RUN state then
		 * we should be able to apply any new state/parameters
		 * simply by re-associating.  Otherwise we need to
		 * re-scan to select an appropriate ap.
		 */
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL) {
			if (vap->iv_state != IEEE80211_S_RUN || forcescan)
				ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
			else
				ieee80211_new_state(vap, IEEE80211_S_ASSOC, 1);
		}
	} else {
		/*
		 * When the old state is running the vap must
		 * be brought to init.
		 */
		if (vap->iv_state == IEEE80211_S_RUN)
			ieee80211_new_state(vap, IEEE80211_S_INIT, -1);
		/*
		 * For monitor+wds modes there's nothing to do but
		 * start running.  Otherwise, if this is the first
		 * vap to be brought up, start a scan which may be
		 * preempted if the station is locked to a particular
		 * channel.
		 */
		if (vap->iv_opmode == IEEE80211_M_MONITOR ||
		    vap->iv_opmode == IEEE80211_M_WDS) {
			ieee80211_new_state(vap, IEEE80211_S_RUN, -1);
		} else
			ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
	}
	return 0;
#undef IS_RUNNING
}

int
ieee80211_open(struct net_device *dev)
{
	return ieee80211_init(dev, 0);
}
EXPORT_SYMBOL(ieee80211_open);

/*
 * Start all runnable vap's on a device.
 */
void
ieee80211_start_running(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	struct net_device *dev;

	/* XXX locking */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		dev = vap->iv_dev;
		if ((dev->flags & IFF_UP) && !(dev->flags & IFF_RUNNING))	/* NB: avoid recursion */
		{
			ieee80211_open(dev);
		}
	}
}
EXPORT_SYMBOL(ieee80211_start_running);

/*
 * Stop a vap.  We force it down using the state machine
 * then mark it's device not running.  If this is the last
 * vap running on the underlying device then we close it
 * too to ensure it will be properly initialized when the
 * next vap is brought up.
 */
int
ieee80211_stop(struct net_device *dev)
{
	struct ieee80211vap *vap = netdev_priv(dev);

	IEEE80211_DPRINTF(vap,
		IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"%s, caller: %p\n", "stop running", __builtin_return_address(0));

	ieee80211_new_state(vap, IEEE80211_S_INIT, -1);
	if (dev->flags & IFF_RUNNING) {
		dev->flags &= ~IFF_RUNNING;		/* mark us stopped */
		vap->iv_mgmt_retry_ni = NULL;
		vap->iv_mgmt_retry_cnt = 0;
		del_timer(&vap->iv_swbmiss);
		del_timer(&vap->iv_swberp);
		del_timer(&vap->iv_mgtsend);
		del_timer(&vap->iv_test_traffic);
		ieee80211_ppqueue_deinit(vap);
		del_timer(&vap->iv_sta_fast_rejoin);
	}
	return 0;
}
EXPORT_SYMBOL(ieee80211_stop);

/*
 * Stop all vap's running on a device.
 */
void
ieee80211_stop_running(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	struct net_device *dev;

	/* XXX locking */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		dev = vap->iv_dev;
		if (dev->flags & IFF_RUNNING)	/* NB: avoid recursion */
			ieee80211_stop(dev);
	}
}
EXPORT_SYMBOL(ieee80211_stop_running);


void
ieee80211_beacon_miss(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	if (ic->ic_flags & IEEE80211_F_SCAN) {
		/* XXX check ic_curchan != ic_bsschan? */
		return;
	}
	/* XXX locking */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		IEEE80211_DPRINTF(vap,
			IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
			"%s\n", "beacon miss");

		/*
		 * Our handling is only meaningful for stations that are
		 * associated; any other conditions else will be handled
		 * through different means (e.g. the tx timeout on mgt frames).
		 */
		if (vap->iv_opmode != IEEE80211_M_STA ||
		    vap->iv_state != IEEE80211_S_RUN)
			continue;
		if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
			/*
			 * Try to reassociate before scanning for a new ap.
			 */
			ieee80211_new_state(vap, IEEE80211_S_ASSOC, 1);
		} else if (vap->iv_bss->ni_inact == 0) {
			/* Send log message out */
			ieee80211_dot11_msg_send(vap,
					(char *)vap->iv_bss->ni_macaddr,
					d11_m[IEEE80211_DOT11_MSG_AP_DISCONNECTED],
					d11_c[IEEE80211_DOT11_MSG_REASON_BEACON_LOSS],
					-1,
					NULL,
					NULL,
					NULL);
			/*
			 * Somebody else is controlling state changes (e.g.
			 * a user-mode app) don't do anything that would
			 * confuse them; just drop into scan mode so they'll
			 * notified of the state change and given control.
			 */
			ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);

		} else {
			mod_timer(&vap->iv_swbmiss, jiffies + vap->iv_swbmiss_period);
		}
	}
}
EXPORT_SYMBOL(ieee80211_beacon_miss);

/*
 * Software OBSS erp timer
 * This timeout function is called when the last non-ERP BSS
 * disappears from the range
 */
void
ieee80211_swberp(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;
	struct ieee80211com *ic = vap->iv_ic;

	if (IEEE80211_BG_PROTECT_ENABLED(ic) && vap->iv_opmode == IEEE80211_M_HOSTAP) {
		if (!ic->ic_nonerpsta && (ic->ic_flags & IEEE80211_F_USEPROT)) {
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
			ic->ic_flags_ext |= IEEE80211_FEXT_ERPUPDATE;

			/* tell Muc to turn off ERP now */
			ic->ic_set_11g_erp(vap, 0);
			ic->ic_beacon_update(vap);
		}
	}
}

/*
 * Software beacon timer callback. In STA mode this timer is triggered when we
 * have a series of beacon misses, up to IEEE80211_SWBMISS_WARNINGS times
 * before finally triggering the beacon missed processing. See
 * ieee80211_recv_mgmt which updates the timer when beacons are properly
 * received.
 *
 * In HOSTAP mode, this function is used to update the beacons when there are
 * no non-HT BSSes on the channel.
 */
void
ieee80211_swbmiss(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;
	struct ieee80211com *ic = vap->iv_ic;

	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		if (ic->ic_non_ht_non_member) {
			ic->ic_non_ht_non_member = 0;
			vap->iv_ht_flags |= IEEE80211_HTF_HTINFOUPDATE;
			ic->ic_beacon_update(vap);
		}
	} else {
		if (vap->iv_swbmiss_warnings) {
			char buf[64];

			ic->ic_iwstats.miss.beacon += (vap->iv_bcn_miss_thr / IEEE80211_SWBMISS_WARNINGS);
			snprintf(buf, sizeof(buf),
					"Missed at least %d consecutive beacons",
					(IEEE80211_SWBMISS_WARNINGS - vap->iv_swbmiss_warnings + 1) *
					(vap->iv_bcn_miss_thr / IEEE80211_SWBMISS_WARNINGS));
			ieee80211_dot11_msg_send(vap,
					(char *)vap->iv_bss->ni_macaddr,
					buf,
					d11_c[IEEE80211_DOT11_MSG_REASON_BEACON_LOSS],
					-1,
					NULL,
					NULL,
					NULL);
			mod_timer(&vap->iv_swbmiss, jiffies + vap->iv_swbmiss_period);

#if defined(QBMPS_ENABLE)
			if ((vap->iv_swbmiss_warnings + 2) <= IEEE80211_SWBMISS_WARNINGS) {
				/* 3 swbmiss warnings received */
				/* exit power-saving mode to help recover from beacon missing */
				vap->iv_swbmiss_bmps_warning = 1;
				if (ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] >= BOARD_PM_LEVEL_DUTY) {
					/* wake up the device once it is brought up */
					ieee80211_pm_queue_work(ic);
				}
			}
#endif
			/* if we've not hit the limit yet, do nothing */
			if (--vap->iv_swbmiss_warnings)
				return;
		}

		if (vap->iv_link_loss_enabled)
			ieee80211_beacon_miss(vap->iv_ic);
	}
}

/*
 * Per-ieee80211vap watchdog timer callback.  This
 * is used only to timeout the xmit of management frames.
 */
static void
ieee80211_tx_timeout(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
		"%s: state %s%s\n", __func__,
		ieee80211_state_name[vap->iv_state],
		vap->iv_ic->ic_flags & IEEE80211_F_SCAN ? ", scan active" : "");

	if (vap->iv_mgmt_retry_ni && vap->iv_mgmt_retry_cnt++ < IEEE80211_MAX_MGMT_RETRY) {
		ieee80211_send_mgmt(vap->iv_mgmt_retry_ni, vap->iv_mgmt_retry_type, vap->iv_mgmt_retry_arg);
	} else {
		vap->iv_mgmt_retry_ni = NULL;
		vap->iv_mgmt_retry_cnt = 0;

		if (vap->iv_state != IEEE80211_S_INIT &&
			(vap->iv_ic->ic_flags & IEEE80211_F_SCAN) == 0) {
			/*
			 * NB: it's safe to specify a timeout as the reason here;
			 *     it'll only be used in the right state.
			 */
			ieee80211_new_state(vap, IEEE80211_S_SCAN,
				IEEE80211_SCAN_FAIL_TIMEOUT);
		}
	}
}

static void
ieee80211_proto_sta_null_pkts(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = arg;

	if (ni->ni_vap == vap && ni->ni_associd != 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
			"%s: Sending null pkt to %p<%s>, refcnt %d\n", __func__, ni,
			ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));

		ieee80211_ref_node(ni);
		if (ni->ni_flags & IEEE80211_NODE_QOS) {
			ieee80211_send_qosnulldata(ni, WME_AC_BK);
		} else {
			ieee80211_send_nulldata(ni);
		}
	}
}

/*
 * Per-ieee80211vap test traffic timer callback.  This
 * is used only to periodically sending null packets to
 * all associated STAs.
 */
static void
ieee80211_test_traffic_timeout(unsigned long arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *) arg;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
		"%s: test traffic timeout on %s, period %d ms.\n", __func__,
		vap->iv_dev->name, jiffies_to_msecs(vap->iv_test_traffic_period));

	ieee80211_iterate_nodes(&vap->iv_ic->ic_sta, ieee80211_proto_sta_null_pkts, vap, 1);

	if (vap->iv_test_traffic_period)
		mod_timer(&vap->iv_test_traffic, jiffies + vap->iv_test_traffic_period);
}

static void
sta_disassoc(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = arg;

	if (ni->ni_vap == vap && ni->ni_associd != 0) {
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DISASSOC,
			IEEE80211_REASON_ASSOC_LEAVE);
		ieee80211_node_leave(ni);
	}
}

void
ieee80211_disconnect_node(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	if (ni->ni_vap == vap && ni->ni_associd != 0) {
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DISASSOC,
			IEEE80211_REASON_ASSOC_TOOMANY);
		ieee80211_node_leave(ni);
	}
}
EXPORT_SYMBOL(ieee80211_disconnect_node);

static void
sta_deauth(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = arg;

	if (ni->ni_vap == vap)
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
			IEEE80211_REASON_ASSOC_LEAVE);
}

/*
 * Context: softIRQ (tasklet) and process
 */
int
ieee80211_new_state(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	int rc;

	/* grab the lock so that only one vap can go through transition at any time */
	IEEE80211_VAPS_LOCK_BH(ic);
	rc = vap->iv_newstate(vap, nstate, arg);
	IEEE80211_VAPS_UNLOCK_BH(ic);
	return rc;
}
EXPORT_SYMBOL(ieee80211_new_state);

static void
ieee80211_create_wds_node(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = TAILQ_FIRST(&ic->ic_vaps)->iv_bss;
	struct ieee80211_node *wds_ni;

	if (IEEE80211_ADDR_NULL(vap->wds_mac))
		return;

	wds_ni = ieee80211_alloc_node(&ic->ic_sta, vap, vap->wds_mac, "wds peer");
	if (wds_ni == NULL) {
		printk(KERN_WARNING "%s: couldn't create WDS node for %s\n",
				vap->iv_dev->name, ether_sprintf(vap->wds_mac));
		return;
	}

	if (ieee80211_aid_acquire(ic, wds_ni)) {
		ieee80211_free_node(wds_ni);
		return;
	}

	if (ieee80211_add_wds_addr(&ic->ic_sta, wds_ni, vap->wds_mac, 1) == 0) {
		ieee80211_node_authorize(wds_ni);
		ieee80211_node_set_chan(ic, wds_ni);
		wds_ni->ni_capinfo = ni->ni_capinfo;
		wds_ni->ni_txpower = ni->ni_txpower;
		wds_ni->ni_ath_flags = vap->iv_ath_cap;
		wds_ni->ni_flags |= IEEE80211_NODE_QOS;
		wds_ni->ni_flags |= IEEE80211_NODE_HT;
		wds_ni->ni_flags &= ~IEEE80211_NODE_VHT;
		wds_ni->ni_flags |= IEEE80211_NODE_WDS_PEER;
		wds_ni->ni_vendor = PEER_VENDOR_QTN;
		wds_ni->ni_node_type = IEEE80211_NODE_TYPE_WDS;
		wds_ni->ni_start_time_assoc = get_jiffies_64();
		if (ic->ic_newassoc != NULL)
			ic->ic_newassoc(wds_ni, 1);
		if (vap->iv_wds_peer_key.wk_keylen != 0) {
			memcpy(&wds_ni->ni_ucastkey, &vap->iv_wds_peer_key, sizeof(vap->iv_wds_peer_key));
			ieee80211_key_update_begin(vap);
			vap->iv_key_set(vap, &vap->iv_wds_peer_key, vap->wds_mac);
			ieee80211_key_update_end(vap);
		}
		/*
		 * WDS node is different from other associated nodes,
		 * no association procedure.
		 * Update these counters when wds node created.
		 */
		IEEE80211_LOCK_IRQ(ic);
		ieee80211_sta_assocs_inc(vap, __func__);
		ic->ic_wds_links++;
		/*
		 * Don't call ieee80211_pm_queue_work here.
		 * We will handle PS in WDS inactivity and BA handling
		 */
		IEEE80211_UNLOCK_IRQ(ic);

		if ((ic->ic_peer_rts_mode == IEEE80211_PEER_RTS_PMP) &&
			((ic->ic_sta_assoc - ic->ic_nonqtn_sta) >= IEEE80211_MAX_STA_CCA_ENABLED)) {

			ic->ic_peer_rts = 1;
			ieee80211_beacon_update_all(ic);
		}

	}
	ieee80211_free_node(wds_ni);
}

static void ieee80211_icac_select(struct ieee80211com *ic, uint32_t *scan_flags)
{
	if (ic->ic_get_init_cac_duration(ic) == 0) {
		if (ic->ic_des_chan == IEEE80211_CHAN_ANYC) {
			/* Select only non-DFS channel at the end; when max_boot_cac is zero */
			*scan_flags |= IEEE80211_SCAN_NO_DFS;
		} else {
			ic->ic_des_chan_after_init_scan = ic->ic_des_chan->ic_ieee;
			ic->ic_des_chan_after_init_cac = 0;
		}
	} else if (ic->ic_get_init_cac_duration(ic) > 0) {
		/* Save ic->ic_des_chan into ic->ic_des_chan_after_init_cac */
		ic->ic_des_chan_after_init_scan = 0;
		ic->ic_des_chan_after_init_cac = (ic->ic_des_chan == IEEE80211_CHAN_ANYC) ?
						0 : ic->ic_des_chan->ic_ieee;
	}
}

static void
ieee80211_sta_leave_run_state(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = vap->iv_ic;

	ieee80211_sta_assocs_dec(vap, __func__);
	ieee80211_nonqtn_sta_leave(vap, ni, __func__);

	ieee80211_sta_leave(ni);
	ieee80211_tdls_free_all_peers(vap);
	ieee80211_restore_bw(vap, ic);
}

static int
__ieee80211_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	enum ieee80211_state ostate;
	uint32_t scan_flags = 0;

	ostate = vap->iv_state;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[ostate], ieee80211_state_name[nstate]);
	vap->iv_state = nstate;			/* state transition */

	/* Legitimate state transition when shutting down; no BSS will be present */
	if (IEEE80211_S_INIT == nstate && IEEE80211_S_INIT == ostate) {
		return 0;
	}

	if (vap->iv_opmode == IEEE80211_M_WDS && ni == NULL) {
		ni = TAILQ_FIRST(&ic->ic_vaps)->iv_bss;
	}

	KASSERT(ni, ("no bss node"));
	ieee80211_ref_node(ni);

	vap->iv_mgmt_retry_ni = NULL;
	vap->iv_mgmt_retry_cnt = 0;
	del_timer(&vap->iv_mgtsend);
	if (vap->iv_opmode != IEEE80211_M_HOSTAP &&
			vap->iv_opmode != IEEE80211_M_WDS &&
			ostate != IEEE80211_S_SCAN) {
		ieee80211_cancel_scan(vap);	/* background scan */
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		switch (ostate) {
		case IEEE80211_S_INIT:
			break;
		case IEEE80211_S_RUN:
			if (vap->iv_opmode == IEEE80211_M_STA) {
				printk(KERN_WARNING "%s: disassociated from AP %s\n",
					vap->iv_dev->name, ether_sprintf(ni->ni_macaddr));

				vap->iv_flags_ext &= ~IEEE80211_FEXT_AP_TDLS_PROHIB;
				vap->iv_flags_ext &= ~IEEE80211_FEXT_TDLS_CS_PROHIB;
				ieee80211_scan_remove(vap);
				IEEE80211_SEND_MGMT(ni,
					IEEE80211_FC0_SUBTYPE_DISASSOC,
					IEEE80211_REASON_ASSOC_LEAVE);
				/*
				 * FIXME: This is nasty, but the simplest method to ensure the disassoc is sent.
				 * FIXME: Revisit this when we have designed a more robust host->MuC synchronisation
				 * mechanism.
				 */
				ieee80211_safe_wait_ms(50, !in_interrupt());
				ieee80211_sta_leave_run_state(vap, ni);
			} else if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
					vap->iv_opmode == IEEE80211_M_WDS) {
				ieee80211_iterate_nodes(&ic->ic_sta,
					sta_disassoc, vap, 1);
			}
			ieee80211_reset_bss(vap);
			break;
		case IEEE80211_S_ASSOC:
			if (vap->iv_opmode == IEEE80211_M_STA) {
				IEEE80211_SEND_MGMT(ni,
					IEEE80211_FC0_SUBTYPE_DEAUTH,
					IEEE80211_REASON_AUTH_LEAVE);
			} else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
				ieee80211_iterate_nodes(&ic->ic_sta,
					sta_deauth, vap, 1);
			}
			ieee80211_reset_bss(vap);
			break;
		case IEEE80211_S_SCAN:
			ieee80211_cancel_scan(vap);
			ieee80211_reset_bss(vap);
			break;
		case IEEE80211_S_AUTH:
			ieee80211_reset_bss(vap);
			break;
		}

		if (vap->iv_auth->ia_detach != NULL)
			vap->iv_auth->ia_detach(vap);
		break;
	case IEEE80211_S_SCAN:
		switch (ostate) {
		case IEEE80211_S_INIT:
		createibss:
			scan_flags |= IEEE80211_SCAN_FLUSH | IEEE80211_SCAN_PICK1ST |
				(IEEE80211_USE_QTN_BGSCAN(vap) ? IEEE80211_SCAN_QTN_BGSCAN : 0);

			if ((vap->iv_opmode == IEEE80211_M_IBSS ||
					vap->iv_opmode == IEEE80211_M_WDS ||
					vap->iv_opmode == IEEE80211_M_AHDEMO) &&
					(ic->ic_des_chan != IEEE80211_CHAN_ANYC) &&
					!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
				ieee80211_create_bss(vap, ic->ic_des_chan);
				break;
			} else if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
				(!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER))) {
				if (ic->ic_get_init_cac_duration(ic) < 0) {
					/* Always do a scan to get a sense of environment after creating BSS */
					if (ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
						ic->ic_curchan = ic->ic_des_chan;
						ic->ic_bsschan = ic->ic_des_chan;
						ic->ic_set_channel(ic);
						scan_flags |= IEEE80211_SCAN_NOPICK;
					}
				} else {
					/*
					 * Always do a scan, when max_boot_cac >= 0;
					 * Initial CAC is done if max_boot_cac >= <1 CAC period>.
					 */
					ieee80211_icac_select(ic, &scan_flags);
				}
			}
			(void) ieee80211_check_scan(vap, scan_flags,
					IEEE80211_SCAN_FOREVER,
					vap->iv_des_nssid, vap->iv_des_ssid,
					NULL);
			break;
		case IEEE80211_S_SCAN:
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
				/*
				 * Scan->Scan:
				 * This can happen when channel scanning is on going and
				 * channel is explicitly set by user at this moment.
				 * If the desired channel is already set, startup immediately;
				 * If not set, do channel re-scan.
				 */
				if ((ostate == IEEE80211_S_SCAN) &&
						(ic->ic_des_chan != IEEE80211_CHAN_ANYC) &&
						!(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
					ieee80211_create_bss(vap, ic->ic_des_chan);
				} else {
					(void) ieee80211_check_scan(vap,
						IEEE80211_SCAN_FLUSH | IEEE80211_SCAN_PICK1ST,
						IEEE80211_SCAN_FOREVER,
						vap->iv_des_nssid, vap->iv_des_ssid,
						NULL);
				}
			} else {
				/*
				 * These can happen either because of a timeout
				 * on an assoc/auth response or because of a
				 * change in state that requires a reset.  For
				 * the former we're called with a non-zero arg
				 * that is the cause for the failure; pass this
				 * to the scan code so it can update state.
				 * Otherwise trigger a new scan unless we're in
				 * manual roaming mode in which case an application
				 * must issue an explicit scan request.
				 */
				if (arg != 0)
					ieee80211_scan_assoc_fail(ic,
						ni->ni_macaddr, arg);
				if (ic->ic_roaming == IEEE80211_ROAMING_AUTO)
				{
					(void) ieee80211_check_scan(vap,
						IEEE80211_SCAN_FLUSH |
						IEEE80211_SCAN_ACTIVE |
						IEEE80211_SCAN_PICK1ST |
						(IEEE80211_USE_QTN_BGSCAN(vap) ? IEEE80211_SCAN_QTN_BGSCAN : 0),
						IEEE80211_SCAN_FOREVER,
						vap->iv_des_nssid, vap->iv_des_ssid,
						NULL);
				}
			}
			break;
		case IEEE80211_S_RUN:		/* beacon miss */
			if (vap->iv_opmode == IEEE80211_M_STA) {
				printk(KERN_WARNING "%s: disassociated from AP %s\n",
					vap->iv_dev->name, ether_sprintf(ni->ni_macaddr));
				IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DISASSOC,
					IEEE80211_REASON_ASSOC_LEAVE);
				ieee80211_sta_leave_run_state(vap, ni);
				vap->iv_flags &= ~IEEE80211_F_SIBSS;	/* XXX */
				if (ic->ic_roaming == IEEE80211_ROAMING_AUTO)
					(void) ieee80211_check_scan(vap,
						IEEE80211_SCAN_FLUSH |
						IEEE80211_SCAN_ACTIVE |
						IEEE80211_SCAN_PICK1ST |
						(IEEE80211_USE_QTN_BGSCAN(vap) ? IEEE80211_SCAN_QTN_BGSCAN : 0),
						IEEE80211_SCAN_FOREVER,
						vap->iv_des_nssid,
						vap->iv_des_ssid,
						NULL);
			} else if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
					vap->iv_opmode == IEEE80211_M_WDS) {
				/* DFS channel switch by CSA, skip disassociation */
				if (!(ic->ic_flags & IEEE80211_F_CHANSWITCH)) {
					ieee80211_iterate_nodes(&ic->ic_sta,
						sta_disassoc, vap, 1);
				}
				goto createibss;
			}
			break;
		}
		break;
	case IEEE80211_S_AUTH:
		/* auth frames are possible between IBSS nodes, see 802.11-1999, chapter 5.7.6 */
		KASSERT(vap->iv_opmode == IEEE80211_M_STA || vap->iv_opmode == IEEE80211_M_IBSS,
			("switch to %s state when operating in mode %u",
			 ieee80211_state_name[nstate], vap->iv_opmode));
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_AUTH, 1);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			switch (arg) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				/* ??? */
				IEEE80211_SEND_MGMT(ni,
					IEEE80211_FC0_SUBTYPE_AUTH, 2);
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				IEEE80211_SEND_MGMT(ni,
					IEEE80211_FC0_SUBTYPE_AUTH, 1);
				break;
			}
			break;
		case IEEE80211_S_RUN:
			printk(KERN_WARNING "%s: deauthenticated from AP %s\n",
				vap->iv_dev->name, ether_sprintf(ni->ni_macaddr));
			switch (arg) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				IEEE80211_SEND_MGMT(ni,
					IEEE80211_FC0_SUBTYPE_AUTH, 2);
				vap->iv_state = ostate;	/* stay RUN */
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				ieee80211_sta_leave_run_state(vap, ni);
				if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
					/* try to reauth */
					IEEE80211_SEND_MGMT(ni,
						IEEE80211_FC0_SUBTYPE_AUTH, 1);
				}
				break;
			}
			break;
		}
		break;
	case IEEE80211_S_ASSOC:
		KASSERT(vap->iv_opmode == IEEE80211_M_STA,
			("switch to %s state when operating in mode %u",
			 ieee80211_state_name[nstate], vap->iv_opmode));
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
				"%s: invalid transition\n", __func__);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			IEEE80211_SEND_MGMT(ni,
				IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			break;
		case IEEE80211_S_RUN:
			printk(KERN_WARNING "%s: disassociated from AP %s\n",
				vap->iv_dev->name, ether_sprintf(ni->ni_macaddr));
			ieee80211_sta_leave_run_state(vap, ni);
			if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
				/* NB: caller specifies ASSOC/REASSOC by arg */
				IEEE80211_SEND_MGMT(ni, arg ?
					IEEE80211_FC0_SUBTYPE_REASSOC_REQ :
					IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			}
			break;
		}
		break;
	case IEEE80211_S_RUN:
		if (vap->iv_flags & IEEE80211_F_WPA) {
			/* XXX validate prerequisites */
		}

		switch (ostate) {
		case IEEE80211_S_INIT:
			if (vap->iv_opmode == IEEE80211_M_MONITOR ||
			    vap->iv_opmode == IEEE80211_M_WDS ||
			    vap->iv_opmode == IEEE80211_M_HOSTAP) {
				/*
				 * Already have a channel; bypass the
				 * scan and startup immediately.
				 */
				ieee80211_create_bss(vap, ic->ic_curchan);

				/*
				 * In wds mode allocate and initialize peer node
				 */
				if (vap->iv_opmode == IEEE80211_M_WDS) {
					ieee80211_create_wds_node(vap);
					if (IEEE80211_COM_WDS_IS_RBS(ic))
						ieee80211_beacon_update_all(ic);
				}
				break;
			}
			/* fall thru... */
		case IEEE80211_S_AUTH:
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
				"%s: invalid transition\n", __func__);
			break;
		case IEEE80211_S_RUN:
			break;
		case IEEE80211_S_SCAN:		/* adhoc/hostap mode */
		case IEEE80211_S_ASSOC:		/* infra mode */
			KASSERT(ni->ni_txrate < ni->ni_rates.rs_nrates,
				("%s: bogus xmit rate %u setup\n", __func__,
				ni->ni_txrate));
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_debug(vap)) {
				ieee80211_note(vap, "%s with %s ssid ",
					(vap->iv_opmode == IEEE80211_M_STA ?
					"associated" : "synchronized "),
					ether_sprintf(ni->ni_bssid));
				ieee80211_print_essid(ni->ni_essid,
					ni->ni_esslen);
				printf(" channel %d start %uMb\n",
					ieee80211_chan2ieee(ic, ic->ic_curchan),
					IEEE80211_RATE2MBS(ni->ni_rates.rs_rates[ni->ni_txrate]));
			}
#endif
			if (vap->iv_opmode == IEEE80211_M_STA) {
				printk(KERN_WARNING "%s: associated with AP %s\n",
					vap->iv_dev->name, ether_sprintf(ni->ni_macaddr));
				ieee80211_scan_assoc_success(ic,
					ni->ni_macaddr);
				ieee80211_notify_node_join(ni,
					(arg == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) | \
					(arg == IEEE80211_FC0_SUBTYPE_REASSOC_RESP));
				if ((vap->iv_qtn_flags & IEEE80211_QTN_BRIDGEMODE_DISABLED) &&
						(vap->iv_qtn_ap_cap & IEEE80211_QTN_BRIDGEMODE)) {
					printk(KERN_WARNING "%s: 4-address mode is supported "
						"by the associated AP but is disabled\n",
						vap->iv_dev->name);
				}
				if (ic->ic_pwr_adjust_scancnt > 0)
					ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_PWR_ADJUST_AUTO, 1, NULL, 0);

				/* check if need to activate tdls discovery timer */
				if ((vap->iv_flags_ext & IEEE80211_FEXT_TDLS_DISABLED) == 0) {
					if ((vap->tdls_discovery_interval > 0) &&
						(!timer_pending(&vap->tdls_rate_detect_timer)))
						ieee80211_tdls_trigger_rate_detection((unsigned long)vap);
				}

				ieee80211_sta_assocs_inc(vap, __func__);
				ieee80211_nonqtn_sta_join(vap, ni, __func__);

				if (ic->sta_dfs_info.sta_dfs_strict_mode) {
					if (ieee80211_is_chan_not_available(ni->ni_chan)) {
						if (ic->ic_mark_channel_availability_status) {
							ic->ic_mark_channel_availability_status(ic,
									ni->ni_chan,
									IEEE80211_CHANNEL_STATUS_AVAILABLE);
						}
					}
				}
			}
			break;
		}

		/* WDS/Repeater: Start software beacon timer for STA */
		if (ostate != IEEE80211_S_RUN &&
		    (vap->iv_opmode == IEEE80211_M_STA &&
		     vap->iv_flags_ext & IEEE80211_FEXT_SWBMISS)) {

			if (!vap->iv_bcn_miss_thr)
				vap->iv_bcn_miss_thr = IEEE80211_NUM_BEACONS_TO_MISS;

			vap->iv_swbmiss.function = ieee80211_swbmiss;
			vap->iv_swbmiss.data = (unsigned long) vap;
			vap->iv_swbmiss_warnings = IEEE80211_SWBMISS_WARNINGS;
			vap->iv_swbmiss_period = IEEE80211_TU_TO_JIFFIES(
				ni->ni_intval * vap->iv_bcn_miss_thr);

#if defined(QBMPS_ENABLE)
			vap->iv_swbmiss_bmps_warning = 0;
#endif
			if (vap->iv_swbmiss_warnings)
				vap->iv_swbmiss_period /= (vap->iv_swbmiss_warnings + 1);

			mod_timer(&vap->iv_swbmiss, jiffies + vap->iv_swbmiss_period);
		}

#if defined(QBMPS_ENABLE)
		if ((vap->iv_opmode == IEEE80211_M_STA) &&
		    (ic->ic_flags_qtn & IEEE80211_QTN_BMPS)) {
			ieee80211_pm_queue_work(ic);
		}
#endif

		/*
		 * Start/stop the authenticator when operating as an
		 * AP.  We delay until here to allow configuration to
		 * happen out of order.
		 */
		/* XXX WDS? */
		if (vap->iv_opmode == IEEE80211_M_HOSTAP && /* XXX IBSS/AHDEMO */
		    vap->iv_auth->ia_attach != NULL) {
			/* XXX check failure */
			vap->iv_auth->ia_attach(vap);
		} else if (vap->iv_auth->ia_detach != NULL)
			vap->iv_auth->ia_detach(vap);
		/*
		 * When 802.1x is not in use mark the port authorized
		 * at this point so traffic can flow.
		 */
		if ((ni->ni_authmode != IEEE80211_AUTH_8021X) &&
			(!(vap->iv_flags & (IEEE80211_F_WPA1 | IEEE80211_F_WPA2)))) {
			ieee80211_node_authorize(ni);
		}
		break;
	}

	ieee80211_free_node(ni);

	return 0;
}

static int
ieee80211_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	enum ieee80211_state ostate;
	struct ieee80211vap *tmpvap;

	ostate = vap->iv_state;

	if (ic->ic_flags_qtn & IEEE80211_QTN_MONITOR) {
		return 0;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
			"%s: %s -> %s\n",
			__FUNCTION__,
			ieee80211_state_name[ostate],
			ieee80211_state_name[nstate]);

	switch (nstate) {
	case IEEE80211_S_SCAN:
		if (ostate == IEEE80211_S_INIT) {
			int nrunning, nscanning;

			nrunning = nscanning = 0;
			TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
				if (vap != tmpvap) {
					if (tmpvap->iv_opmode == IEEE80211_M_MONITOR ||
					    tmpvap->iv_opmode == IEEE80211_M_WDS)
						/*
						 * Skip monitor and WDS vaps as their S_RUN
						 * shouldn't have any influence on modifying
						 * state transition.
						 */
						continue;
					if (tmpvap->iv_state == IEEE80211_S_RUN)
						nrunning++;
					else if (tmpvap->iv_state == IEEE80211_S_SCAN ||
					    tmpvap->iv_state == IEEE80211_S_AUTH || /* STA in WDS/Repeater */
					    tmpvap->iv_state == IEEE80211_S_ASSOC)
						nscanning++;
				}
			}

			KASSERT(!(nscanning && nrunning), ("SCAN and RUN can't happen at the same time\n"));

			if (!nscanning && !nrunning) {
				/* when no one is running or scanning, start a new scan */
				__ieee80211_newstate(vap, nstate, arg);
			} else if (!nscanning && nrunning) {
				/* when no one is scanning but someone is running, bypass
				 * scan and go to run state immediately */
				if (vap->iv_opmode == IEEE80211_M_MONITOR ||
				    vap->iv_opmode == IEEE80211_M_WDS ||
				    vap->iv_opmode == IEEE80211_M_HOSTAP) {
					__ieee80211_newstate(vap, IEEE80211_S_RUN, arg);
				} else {
					/* MW: avoid invalid S_INIT -> S_RUN transition */
					__ieee80211_newstate(vap, nstate, arg);
				}
			} else if (nscanning && !nrunning) {
				/* when someone is scanning and no one is running, set
				 * the scan pending flag. Don't go through state machine */
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
					"%s: %s -> %s with SCAN_PENDING\n",
					__func__,
					ieee80211_state_name[ostate],
					ieee80211_state_name[nstate]);
				vap->iv_flags_ext |= IEEE80211_FEXT_SCAN_PENDING;
			}
		} else {
			TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
				if (vap != tmpvap && tmpvap->iv_state != IEEE80211_S_INIT && !(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
					if ((ic->ic_flags & IEEE80211_F_CHANSWITCH) &&
							(vap->iv_opmode == IEEE80211_M_HOSTAP) &&
							(ic->ic_des_chan != IEEE80211_CHAN_ANYC) &&
							(ostate == IEEE80211_S_RUN) &&
							(tmpvap->iv_opmode == IEEE80211_M_HOSTAP) &&
							(tmpvap->iv_state == IEEE80211_S_RUN)) {
						/*
						 * DFS channel switch enabled
						 * vap is AP mode and old state is RUN;
						 * Desired channel is set;
						 * tmpvap is AP mode and current state is RUN.
						 * Do nothing but pending for vap from SCAN to RUN,
						 * re-enter RUN state and update beacon when pending was cleared.
						 */
						;	/* Noting */
					} else {
						/*
						 * The VAP is forced to scan, we need to change all other vap's state
						 * to INIT and pend for the scan completion.
						 *
						 * For WDS, change state to INIT as long as channel will be changed.
						 */
						tmpvap->iv_newstate(tmpvap, IEEE80211_S_INIT, 0);
					}
					tmpvap->iv_flags_ext |= IEEE80211_FEXT_SCAN_PENDING;
				}
			}

			/* start the new scan */
			__ieee80211_newstate(vap, nstate, arg);
		}
		break;
	case IEEE80211_S_RUN:
		if ((ostate == IEEE80211_S_SCAN ||		/* AP coming out of scan */
				vap->iv_opmode == IEEE80211_M_STA) /* STA in WDS/Repeater needs to bring up other VAPs */
				&& !(ic->ic_flags_ext & IEEE80211_FEXT_REPEATER)) {
			__ieee80211_newstate(vap, nstate, arg);

			/* bring up all other vaps pending on the scan*/
			TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
				if (vap != tmpvap
						&& (tmpvap->iv_flags_ext & IEEE80211_FEXT_SCAN_PENDING)) {
					tmpvap->iv_flags_ext &= ~IEEE80211_FEXT_SCAN_PENDING;
					tmpvap->iv_newstate(tmpvap, IEEE80211_S_RUN, 0);
				}
			}
		} else {
			__ieee80211_newstate(vap, nstate, arg);
		}
		break;
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_INIT && vap->iv_flags_ext & IEEE80211_FEXT_SCAN_PENDING)
			vap->iv_flags_ext &= ~IEEE80211_FEXT_SCAN_PENDING;
		/* fall through */
	default:
		__ieee80211_newstate(vap, nstate, arg);
	}
	return 0;
}


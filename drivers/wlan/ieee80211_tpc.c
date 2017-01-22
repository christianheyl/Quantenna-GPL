/*-
 * Copyright (c) 2013 Quantenna
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
 * $Id: ieeee80211_tpc.c 5000 2013-01-25 10:22:59Z casper $
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include "net80211/if_media.h"
#include "net80211/ieee80211_var.h"
#include "net80211/_ieee80211.h"
#include "net80211/ieee80211_tpc.h"
#include "net80211/ieee80211_linux.h"
#include "net80211/ieee80211_proto.h"	/* IEEE80211_SEND_MGMT */

void tpc_report_callback_success(void *ctx)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	printk("TPC:tx power = %d, link margin = %d from node (%s)\n",
			ni->ni_tpc_info.tpc_report.node_txpow,
			ni->ni_tpc_info.tpc_report.node_link_margin,
			ether_sprintf(ni->ni_macaddr));
}

void tpc_report_callback_fail(void *ctx, int32_t reason)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)ctx;

	printk("TPC:fail to get tpc report from node (%s)\n",
			ether_sprintf(ni->ni_macaddr));
}

/* TPC_REQ_PERIOD */
static void node_send_tpc_request(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211_action_data action_data;
	struct ieee80211_action_tpc_request request;

	request.expire = HZ / 10;
	request.fn_success = tpc_report_callback_success;
	request.fn_fail = tpc_report_callback_fail;
	action_data.cat	= IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action = IEEE80211_ACTION_S_TPC_REQUEST;
	action_data.params = &request;

	switch (ni->ni_vap->iv_opmode) {
	case IEEE80211_M_HOSTAP:
		if ((ni->ni_vap->iv_state == IEEE80211_S_RUN) &&
				(ni->ni_associd != 0) &&
				(ni->ni_capinfo & IEEE80211_CAPINFO_SPECTRUM_MGMT) &&
				(ni->ni_flags & IEEE80211_NODE_TPC)) {
			IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
		}
		break;
	case IEEE80211_M_STA:
		if ((ni->ni_vap->iv_state == IEEE80211_S_RUN) &&
				(ni == ni->ni_vap->iv_bss) &&
				(ni->ni_capinfo & IEEE80211_CAPINFO_SPECTRUM_MGMT) &&
				(ni->ni_flags & IEEE80211_NODE_TPC)) {
			IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
		}
		break;
	default:
		break;
	}
}

static void tpc_query_timer(unsigned long data)
{
	struct ieee80211_tpc_query_info *info = (struct ieee80211_tpc_query_info *)data;
	struct ieee80211com *ic = (struct ieee80211com *)info->target;

	ieee80211_iterate_nodes(&ic->ic_sta, node_send_tpc_request, NULL, 1);
	mod_timer(&info->query_timer, jiffies + info->query_interval * HZ);
}

int ieee80211_tpc_query_init(struct ieee80211_tpc_query_info *info, struct ieee80211com *ic, int query_interval)
{
	if ((info == NULL) || (ic == NULL) || (query_interval < TPC_INTERVAL_MIN))
		return -EINVAL;

	memset(info, 0, sizeof(*info));
	info->target = (void *)ic;
	info->query_interval = query_interval;
	init_timer(&info->query_timer);
	setup_timer(&info->query_timer, tpc_query_timer, (unsigned long)info);

	return 0;
}

void ieee80211_tpc_query_deinit(struct ieee80211_tpc_query_info *info)
{
	if (NULL != info) {
		if (info->is_run) {
			del_timer(&info->query_timer);
			info->is_run = 0;
		}
		memset(info, 0, sizeof(*info));
	}
}

int ieee80211_tpc_query_start(struct ieee80211_tpc_query_info *info)
{
	int ret = -1;
	if (NULL != info) {
		if (0 == info->is_run) {
			mod_timer(&info->query_timer, jiffies + info->query_interval * HZ);
			info->is_run = 1;
			ret = 0;
		}
	}

	return ret;
}

int ieee80211_tpc_query_stop(struct ieee80211_tpc_query_info *info)
{
	int ret = -1;
	if (NULL != info) {
		if (info->is_run) {
			del_timer(&info->query_timer);
			info->is_run = 0;
			ret = 0;
		}
	}

	return ret;
}

int ieee80211_tpc_query_config_interval(struct ieee80211_tpc_query_info *info, int interval)
{
	int ret = -1;

	if ((NULL != info) && (interval >= TPC_INTERVAL_MIN)) {
		info->query_interval = interval;
		if (info->is_run)
			mod_timer(&info->query_timer, jiffies + info->query_interval * HZ);
		ret = 0;
	}

	return ret;
}

int ieee80211_tpc_query_get_interval(struct ieee80211_tpc_query_info *info)
{
	int interval = -1;

	if (NULL != info)
		interval = info->query_interval;

	return interval;
}

int ieee80211_tpc_query_state(struct ieee80211_tpc_query_info *info)
{
	int state = -1;

	if (NULL != info)
		state = info->is_run;

	return state;
}

/* TBD */
int8_t ieee80211_update_tx_power(struct ieee80211com *ic, int8_t txpwr)
{
	return 0;
}

void get_max_in_minpwr(void *arg, struct ieee80211_node *ni)
{
	struct pwr_info_per_vap *p = (struct pwr_info_per_vap *)arg;

	if ((ni->ni_vap == p->vap) && (ni->ni_associd != 0) && (ni->ni_capinfo & IEEE80211_CAPINFO_SPECTRUM_MGMT)) {
		if (ni->ni_tpc_info.tpc_sta_cap.min_txpow > p->max_in_minpwr)
			p->max_in_minpwr = ni->ni_tpc_info.tpc_sta_cap.min_txpow;
	}
}

int ieee80211_parse_local_max_txpwr(struct ieee80211vap *vap, struct ieee80211_scanparams *scan)
{
	u_int8_t	*country = scan->country;
	u_int8_t	*pwr_constraint = scan->pwr_constraint;
	u_int8_t	start_chan;
	u_int8_t	chan_number;
	int8_t		chan_reg_max_txpwr;
	int8_t		constraint;
	u_int8_t	*ie;
	u_int8_t	*ie_end;
	u_int8_t	channel = vap->iv_ic->ic_curchan->ic_ieee;

	scan->local_max_txpwr = -1;
	if (pwr_constraint[1] != 1) {
		IEEE80211_DPRINTF(vap,
				IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
				"invalid pwr_constraint ie (len=%d)\n",
				pwr_constraint[1]);
		return -1;
	}

	constraint = pwr_constraint[2];

	ie	= country + 2 + 3;
	ie_end	= country + country[1] + 2;

	while((ie_end - ie) >= 3) {
		start_chan		= ie[0];
		chan_number		= ie[1];
		chan_reg_max_txpwr	= ie[2];
		if ((channel >= start_chan) && (channel < (start_chan + chan_number))) {
			scan->local_max_txpwr = chan_reg_max_txpwr - constraint;
			IEEE80211_DPRINTF(vap,
					IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
					"chan=%d regulatory powr=%d constraint=%d local max power=%d\n",
					channel,
					chan_reg_max_txpwr,
					constraint,
					scan->local_max_txpwr);
			return 0;
		}
		ie += 3;
	}
	IEEE80211_DPRINTF(vap,
			IEEE80211_MSG_DOTH | IEEE80211_MSG_DEBUG,
			"No power constraint in scan channel %d,but beacon is there, something might be wrong\n",
			channel);
	return -1;
}

void ieee80211_doth_measurement_init(struct ieee80211com *ic)
{
	memset(&ic->ic_measure_info, 0, sizeof(ic->ic_measure_info));
}

void ieee80211_doth_measurement_deinit(struct ieee80211com *ic)
{
	ic->ic_measure_info.status = MEAS_STATUS_DISCRAD;
}

void ieee80211_action_finish_measurement(struct ieee80211com *ic, u_int8_t result)
{
	struct ieee80211_global_measure_info *ic_meas_info = &ic->ic_measure_info;

	switch (ic_meas_info->type) {
	case IEEE80211_CCA_MEASTYPE_BASIC:
		ieee80211_send_meas_report_basic(ic_meas_info->ni,
				result,
				ic_meas_info->frame_token,
				1,
				ic_meas_info->param.basic.channel,
				ic_meas_info->param.basic.tsf,
				ic_meas_info->param.basic.duration_tu,
				ic_meas_info->results.basic);
		break;
	case IEEE80211_CCA_MEASTYPE_CCA:
		ieee80211_send_meas_report_cca(ic_meas_info->ni,
				result,
				ic_meas_info->frame_token,
				1,
				ic_meas_info->param.cca.channel,
				ic_meas_info->param.cca.tsf,
				ic_meas_info->param.cca.duration_tu,
				ic_meas_info->results.cca);
		break;
	case IEEE80211_CCA_MEASTYPE_RPI:
		ieee80211_send_meas_report_rpi(ic_meas_info->ni,
				result,	ic_meas_info->frame_token,
				1, ic_meas_info->param.rpi.channel,
				ic_meas_info->param.rpi.tsf,
				ic_meas_info->param.rpi.duration_tu,
				ic_meas_info->results.rpi);
		break;
	case IEEE80211_RM_MEASTYPE_CH_LOAD:
		ieee80211_send_rm_rep_chan_load(ic_meas_info->ni,
				result, ic_meas_info->frame_token, 1,
				ic_meas_info->param.chan_load.op_class,
				ic_meas_info->param.chan_load.channel,
				ic_meas_info->param.chan_load.duration_tu,
				ic_meas_info->results.chan_load);
		break;
	case IEEE80211_RM_MEASTYPE_NOISE:
		ieee80211_send_rm_rep_noise_his(ic_meas_info->ni, result, ic_meas_info->frame_token,
				1, ic_meas_info->param.noise_his.op_class,
				ic_meas_info->param.noise_his.channel,
				ic_meas_info->param.chan_load.duration_tu,
				255, ic_meas_info->results.noise_his.anpi,
				ic_meas_info->results.noise_his.ipi);

		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(ieee80211_action_finish_measurement);

int ieee80211_action_trigger_measurement(struct ieee80211com *ic)
{
	return ic->ic_do_measurement(ic);
}

int ieee80211_action_measurement_report_fail(struct ieee80211_node *ni,
					u_int8_t type,
					u_int8_t report_mode,
					u_int8_t token,
					u_int8_t meas_token)
{
	struct ieee80211_meas_report_ctrl ctrl;
	struct ieee80211_action_data action_data;

	if (report_mode == 0)
		return -1;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.meas_type = type;
	ctrl.report_mode = report_mode;
	ctrl.token = token;
	ctrl.meas_token = meas_token;
	ctrl.autonomous = 0;

	action_data.cat = (type <= IEEE80211_CCA_MEASTYPE_RPI ? IEEE80211_ACTION_CAT_SPEC_MGMT : IEEE80211_ACTION_CAT_RM);
	action_data.action = (type <= IEEE80211_CCA_MEASTYPE_RPI ? IEEE80211_ACTION_S_MEASUREMENT_REPORT : IEEE80211_ACTION_R_MEASUREMENT_REPORT);
	action_data.params = &ctrl;

	return IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);
}


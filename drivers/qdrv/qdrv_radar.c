/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>

#include <linux/device.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/pm_qos_params.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_hal.h"
#include "qdrv_muc.h"
#include "qdrv_dsp.h"
#include "qtn/registers.h"
#include "qtn/muc_phy_stats.h"
#include "qdrv_comm.h"
#include "qdrv_wlan.h"
#include "qdrv_radar.h"
#include "radar/radar.h"
#include "radar/detect.h"
#include <net/iw_handler.h> /* wireless_send_event(..) */
#include "qdrv_debug.h"
#include <net80211/ieee80211_var.h>
#include "qdrv_control.h"

/* Will move this to a configuration later.  */
#define CONFIG_QHOP 1

#define CAC_PERIOD		(70 * HZ)
#define CAC_WEATHER_PERIOD_EU	(600 * HZ)
#define CAC_PERIOD_QUICK	(30 * HZ)
#define NONOCCUPY_PERIOD_QUICK	(60 * HZ)

#define DFS_CS_TIMER_VAL	(HZ / 10)

#define QDRV_RADAR_SAMPLE_RATE	1	/* sampling rate (seconds) */
#define QDRV_RADAR_SAMPLE_DELAY	10	/* Give MuC time to update stats (jiffies) */

static void qdrv_radar_sample_work(struct work_struct *unused);

static bool qdrv_radar_configured = false;
static bool qdrv_radar_first_call = true;
static bool qdrv_radar_sta_dfs = false;

/*
 * Control block for qdrv_radar
 */
struct qdrv_radar_sample {
	struct delayed_work		sample_work;
	struct detect_drv_sample_t	*sample;
};

static struct {
	bool				enabled;
	bool				xmit_stopped;
	struct qdrv_mac			*mac;
	struct ieee80211com		*ic;
	struct ieee80211_channel	*cac_chan;
	struct timer_list		cac_timer; /* a timer for CAC */
	struct timer_list		nonoccupy_timer[IEEE80211_CHAN_MAX+1];
	struct ieee80211_channel	*dfs_des_chan;
	struct timer_list		dfs_cs_timer; /* a timer for a channel switch */
	struct qdrv_radar_sample	muc_sampling;
	struct muc_tx_stats		*stats_uc_tx_ptr;
	struct notifier_block		pm_notifier;
	struct tasklet_struct		ocac_tasklet;
	uint32_t			region;
} qdrv_radar_cb;

/*
 * Utility macros
 */

/*
 * True if this mode must behave like a DFS master, ie do Channel
 * Check Availability and In Service Monitoring. We need to make sure
 * that all modes cannot send data without being authorized. Such
 * enforcement is not done in monitor mode however.
 */
static inline int ieee80211_is_dfs_master(struct ieee80211com *ic)
{
	KASSERT(ic->ic_opmode != IEEE80211_M_WDS,
		(DBGEFMT "Incorrect ic opmode %d\n", DBGARG, ic->ic_opmode));

	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		return 1;

	if (ieee80211_is_repeater(ic))
		return 1;

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		return 1;

	if (ic->ic_opmode == IEEE80211_M_AHDEMO)
		return 1;

	return 0;
}

static inline int qdrv_is_dfs_master(void)
{
	return ieee80211_is_dfs_master(qdrv_radar_cb.ic);
}

static inline int qdrv_is_dfs_slave(void)
{
	return !ieee80211_is_dfs_master(qdrv_radar_cb.ic);
}

#define GET_CHANIDX(chan)	((chan) - ic->ic_channels)

static void mark_radar(void);
static void stop_cac(void);
static void stop_dfs_cs(void);
static void qdrv_ocac_irqhandler(void *arg1, void *arg2);
static int qdrv_init_ocac_irqhandler(struct qdrv_wlan *qw);
static bool qdrv_radar_is_dfs_chan(uint8_t wifi_chan);
static bool qdrv_radar_is_dfs_weather_chan(uint8_t wifi_chan);

#ifndef SYSTEM_BUILD
#define ic2dev(ic)	((struct ieee80211vap *)(TAILQ_FIRST(&(ic)->ic_vaps)) ? \
			((struct ieee80211vap *)(TAILQ_FIRST(&(ic)->ic_vaps)))->iv_dev : NULL)
#else
#define ic2dev(ic)	NULL
#endif

/* used to report RADAR: messages to event server */
#define radar_event_report(...)			qdrv_eventf(__VA_ARGS__)

#define DBGPRINTF_N_QEVT(qevtdev, ...)		do {\
							DBGPRINTF_N(__VA_ARGS__);\
							radar_event_report(qevtdev, __VA_ARGS__);\
						} while (0)

#ifdef CONFIG_QHOP
/*
 *   RBS reports channel change detect to MBS over the WDS link.
 */
static void
qdrv_qhop_send_rbs_report_frame(struct ieee80211vap *vap, u_int8_t new_chan)
{
	struct ieee80211_node *ni = ieee80211_get_wds_peer_node_ref(vap);
	struct sk_buff *skb;
	int frm_len = sizeof(struct qdrv_vendor_action_header) + sizeof(struct qdrv_vendor_action_qhop_dfs_data);
	u_int8_t *frm;

	if (!ni) {
		DBGPRINTF_E("WDS peer is NULL!\n");
		return;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
	                "%s: Sending action frame with RBS report IE: %u\n", __func__, new_chan);

	skb = ieee80211_getmgtframe(&frm, frm_len);
	if (skb == NULL) {
	        IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni, "%s: cannot get buf; size %u", __func__, frm_len);
	        vap->iv_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
	        return;
	}

	/* Fill in QHOP action header and data */
	*frm++ = IEEE80211_ACTION_CAT_VENDOR;
	frm += 3;
	*frm++ = QDRV_ACTION_TYPE_QHOP;
	*frm++ = QDRV_ACTION_QHOP_DFS_REPORT;
	*frm++ = new_chan;

	ieee80211_mgmt_output(ni, skb, IEEE80211_FC0_SUBTYPE_ACTION, ni->ni_macaddr);
}
#endif

/*
 * Perioodically sample data from the MuC
 */
static void qdrv_radar_sample_work(struct work_struct *unused)
{
	struct muc_tx_stats stats_muc_tx;
	unsigned long lock_flags;

	if (qdrv_radar_cb.stats_uc_tx_ptr == NULL) {
		return;
	}

	memcpy(&stats_muc_tx, qdrv_radar_cb.stats_uc_tx_ptr, sizeof(stats_muc_tx));

	/* Update the structure owned by the radar module */
	spin_lock_irqsave(&qdrv_radar_cb.muc_sampling.sample->lock, lock_flags);

	/* Divide sample values by sample rate to get rate per second */
	qdrv_radar_cb.muc_sampling.sample->tx_pkts =
		stats_muc_tx.tx_sample_pkts / qdrv_radar_cb.ic->ic_sample_rate;
	qdrv_radar_cb.muc_sampling.sample->tx_bytes =
		stats_muc_tx.tx_sample_bytes / qdrv_radar_cb.ic->ic_sample_rate;

	spin_unlock_irqrestore(&qdrv_radar_cb.muc_sampling.sample->lock, lock_flags);

	schedule_delayed_work(&qdrv_radar_cb.muc_sampling.sample_work,
		qdrv_radar_cb.ic->ic_sample_rate * HZ);
}

/*
 * Status-checking inline functions
 */
inline static bool is_cac_started(void)
{
	return (qdrv_radar_cb.cac_chan != NULL);
}

inline static bool is_dfs_cs_started(void)
{
	return (qdrv_radar_cb.dfs_des_chan != NULL);
}

/*
 * Enable radar detection on channel
 */
inline static void sys_enable_rdetection(void)
{
	if (DBG_LOG_FUNC_TEST(QDRV_LF_DFS_DISALLOWRADARDETECT)) {
		DBGPRINTF_N("RADAR: test mode - radar not enabled\n");
		return;
	}
	if (qdrv_is_dfs_slave() && !qdrv_radar_sta_dfs)
		return;
	radar_enable();
}

/*
 * Disable radar detection on channel
 */
inline static void sys_disable_rdetection(void)
{
	radar_disable();
}

/*
 * Start the radar module
 */
inline static bool sys_start_radarmod(const char *region)
{
	bool region_enabled;
	struct ieee80211com *ic = qdrv_radar_cb.ic;

	region_enabled = radar_start(region);
	if (region_enabled) {
		radar_set_bw(ic->ic_radar_bw);
		sys_disable_rdetection();
		radar_register(mark_radar);
		radar_register_is_dfs_chan(qdrv_radar_is_dfs_chan);
		radar_register_is_dfs_weather_chan(qdrv_radar_is_dfs_weather_chan);
	}

	return region_enabled;
}

/*
 * Stop the radar module
 */
inline static void sys_stop_radarmod(void)
{
	radar_stop();
}

static inline void sys_raw_enable_xmit(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);

	qdrv_hostlink_xmitctl(qw, true);
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "transmission enabled\n");
}

static inline void sys_raw_disable_xmit(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);

	qdrv_hostlink_xmitctl(qw, false);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "transmission disabled\n");
}
/*
 * Instruct MuC to enable transmission for AP mode
 */
static void sys_enable_xmit(void)
{
	if (qdrv_radar_cb.xmit_stopped == true) {
		sys_raw_enable_xmit();
		qdrv_radar_cb.xmit_stopped = false;
	} else {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "Xmit already enabled\n");
	}
}

/*
 * Instruct MuC to disable transmission for AP mode
 */
static void sys_disable_xmit(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;

	if (qdrv_radar_cb.xmit_stopped == true) {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "Xmit already disabled\n");
		return;
	}

	/* xmit control not needed for DFS slave */
	if (qdrv_is_dfs_slave() && !ic->sta_dfs_info.sta_dfs_strict_mode)
		return;

	sys_raw_disable_xmit();
	qdrv_radar_cb.xmit_stopped = true;
}

/*
 * Instruct MuC to enable/disable transmission for STA mode
 */
void qdrv_sta_set_xmit(int enable)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	if (!qdrv_radar_cb.ic)
		return;

	if (qdrv_is_dfs_master())
		return;

	if (enable) {
		if (ic->sta_dfs_info.sta_dfs_strict_mode &&
			((ieee80211_is_chan_radar_detected(ic->ic_curchan)) ||
			(ieee80211_is_chan_cac_required(ic->ic_curchan)))) {
			DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
				"\n%s: xmit cannot be enabled on channel %d [%s]\n",
				__func__, ic->ic_curchan ? ic->ic_curchan->ic_ieee : 0,
				ieee80211_is_chan_radar_detected(ic->ic_curchan) ?
				"CAC required" : "in Non-Occupancy list");
			return;
		}
		sys_enable_xmit();
	} else if (qdrv_radar_cb.xmit_stopped == false) {
		sys_raw_disable_xmit();
		qdrv_radar_cb.xmit_stopped = true;
	} else {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "Xmit already disabled\n");
	}
}

/*
 * Start or restart the non-occupy period
 */
static void start_nonoccupy(unsigned chan_idx)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct timer_list *nonoccupy_timer;
	struct ieee80211_channel *chan;
	unsigned long expires;
	unsigned long sta_dfs_timer_expires;

	KASSERT(chan_idx < ic->ic_nchans,
		(DBGEFMT "out-of-range channel idx %u\n", DBGARG, chan_idx));

	chan = &ic->ic_channels[chan_idx];

	if (ic->sta_dfs_info.sta_dfs_strict_mode) {
		/* Check IEEE80211_CHAN_RADAR flag to avoid repeated actions */
		if (chan->ic_flags & IEEE80211_CHAN_RADAR)
			return;
		/*
		 * Mark channel with NOT_AVAILABLE_RADAR_DETECTED flag after a delay
		 * to allow the transmission of the measurement report to the AP
		 * by the STA.
		 */
		ic->sta_dfs_info.sta_radar_timer.data = chan->ic_ieee;
		sta_dfs_timer_expires = jiffies + IEEE80211_MS_TO_JIFFIES(ic->sta_dfs_info.sta_dfs_tx_chan_close_time);
		ic->sta_dfs_info.sta_dfs_radar_detected_timer = true;
		ic->sta_dfs_info.sta_dfs_radar_detected_channel = chan->ic_ieee;
		mod_timer(&ic->sta_dfs_info.sta_radar_timer, sta_dfs_timer_expires);
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "%s: Start sta_radar_timer [expiry:CSA/%lums]\n",
			__func__, ic->sta_dfs_info.sta_dfs_tx_chan_close_time);
	} else if (qdrv_is_dfs_slave()) {
		/* DFS slave depends on a master for this period */
		return;
	}

	if (ieee80211_is_repeater_associated(ic) && !(ic->sta_dfs_info.sta_dfs_strict_mode))
		return;

	chan->ic_flags |= IEEE80211_CHAN_RADAR;
	chan->ic_radardetected++;

	nonoccupy_timer = &qdrv_radar_cb.nonoccupy_timer[chan_idx];

	expires = jiffies + ic->ic_non_occupancy_period;

	if (DBG_LOG_FUNC_TEST(QDRV_LF_DFS_QUICKTIMER)) {
		DBGPRINTF_N("RADAR: test mode - non-occupancy period will expire quickly\n");
		expires = jiffies + NONOCCUPY_PERIOD_QUICK;
	}

	mod_timer(nonoccupy_timer, expires);

	DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: non-occupancy period started for channel %3d "
			"(%4d MHz)\n", chan->ic_ieee, chan->ic_freq);

	if (ic->sta_dfs_info.sta_dfs_strict_mode) {
		return;
	}

	if (ic->ic_mark_channel_availability_status) {
		ic->ic_mark_channel_availability_status(ic, chan, IEEE80211_CHANNEL_STATUS_NOT_AVAILABLE_RADAR_DETECTED);
	}
}

/*
 * Stop active or inactive nonoccupy period
 */
static void raw_stop_nonoccupy(unsigned chan_idx)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct timer_list *nonoccupy_timer;
	struct ieee80211_channel *chan = &ic->ic_channels[chan_idx];

	KASSERT(chan_idx < ic->ic_nchans,
		(DBGFMT "out-of-range channel idx %u\n", DBGARG, chan_idx));

	if (!(chan->ic_flags & IEEE80211_CHAN_RADAR)) {
		return;
	}
	chan->ic_flags &= ~IEEE80211_CHAN_RADAR;

	nonoccupy_timer = &qdrv_radar_cb.nonoccupy_timer[chan_idx];
	del_timer(nonoccupy_timer);

	DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: non-occupancy period stopped for channel %3d "
			 "(%4d MHz)\n", chan->ic_ieee, chan->ic_freq);
}

static void qdrv_radar_enable_action(void)
{
	struct ieee80211com *ic;
	struct qdrv_wlan *qw;
	struct qtn_stats_log *iw_stats_log;

	if (qdrv_radar_first_call == true || !qdrv_radar_configured) {
		DBGPRINTF_E("radar unconfigured\n");
		return;
	}

	if (qdrv_radar_cb.enabled) {
		DBGPRINTF_E("radar already enabled\n");
		return;
	}

	ic = qdrv_radar_cb.ic;
	qw = container_of(ic, struct qdrv_wlan, ic);

	qdrv_mac_enable_irq(qw->mac, RUBY_M2L_IRQ_LO_OCAC);

	/* start sampling */
	iw_stats_log = qdrv_radar_cb.mac->mac_sys_stats;
	if (qdrv_radar_cb.stats_uc_tx_ptr == NULL && iw_stats_log != NULL) {
		qdrv_radar_cb.stats_uc_tx_ptr = ioremap_nocache(
				muc_to_lhost((u32)iw_stats_log->tx_muc_stats),
				sizeof(struct muc_tx_stats));
	}
	schedule_delayed_work(&qdrv_radar_cb.muc_sampling.sample_work,
		(ic->ic_sample_rate * HZ) + QDRV_RADAR_SAMPLE_DELAY);

	qdrv_radar_cb.enabled = true;

	if (ic->ic_curchan != IEEE80211_CHAN_ANYC) {
		qdrv_radar_before_newchan();
		qdrv_radar_on_newchan();
	}

	/* For external stats */
	QDRV_SET_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_RADAR_EN);

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "Radar enabled\n");
}

/*
 * Disable DFS feature
 */
void qdrv_radar_disable(void)
{
	struct ieee80211com *ic;
	struct qdrv_wlan *qw;
	struct qdrv_mac *mac;
	unsigned chan_idx;
	struct ieee80211_channel *chan;

	if (qdrv_radar_first_call == true || !qdrv_radar_configured) {
		DBGPRINTF_E("radar unconfigured\n");
		return;
	}

	if (!qdrv_radar_cb.enabled) {
		DBGPRINTF_E("radar already disabled\n");
		return;
	}

	sys_disable_rdetection();

	mac = qdrv_radar_cb.mac;
	qdrv_mac_disable_irq(mac, RUBY_M2L_IRQ_LO_OCAC);

	/* stop CAC if any */
	stop_cac();

	/* stop CS if any */
	stop_dfs_cs();

	/* stop sampling */
	cancel_delayed_work(&qdrv_radar_cb.muc_sampling.sample_work);
	if (qdrv_radar_cb.stats_uc_tx_ptr != NULL) {
		iounmap(qdrv_radar_cb.stats_uc_tx_ptr);
		qdrv_radar_cb.stats_uc_tx_ptr = NULL;
	}

	ic = qdrv_radar_cb.ic;
	/* delete all nonoccupy timers and clear CAC done flag */
	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		chan = &ic->ic_channels[chan_idx];
		chan->ic_flags &= ~(IEEE80211_CHAN_DFS_CAC_DONE |
				IEEE80211_CHAN_DFS_CAC_IN_PROGRESS);
		raw_stop_nonoccupy(chan_idx);
		if (ic->sta_dfs_info.sta_dfs_strict_mode && (chan->ic_flags & IEEE80211_CHAN_DFS)) {
			ic->ic_chan_availability_status[chan->ic_ieee]
					= IEEE80211_CHANNEL_STATUS_NON_AVAILABLE;
			if (ic->ic_mark_channel_dfs_cac_status) {
				ic->ic_mark_channel_dfs_cac_status(ic, chan,
					IEEE80211_CHAN_DFS_CAC_DONE, false);
				ic->ic_mark_channel_dfs_cac_status(ic, chan,
					IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, false);
			}
		}
	}

	if (ic->sta_dfs_info.sta_dfs_strict_mode) {
		del_timer(&ic->sta_dfs_info.sta_radar_timer);
		ic->sta_dfs_info.sta_dfs_radar_detected_timer = false;
		ic->sta_dfs_info.sta_dfs_radar_detected_channel = 0;
	}

	/* always enable transmission */
	sys_enable_xmit();

	qdrv_radar_cb.enabled = false;
	/* For external stats */
	qw = container_of(ic, struct qdrv_wlan, ic);
	QDRV_CLEAR_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_RADAR_EN);

	/* success */
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "radar disabled\n");
}

void qdrv_set_radar(int enable)
{
	if (qdrv_radar_first_call == true || !qdrv_radar_configured) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
				"radar already unconfigured\n");
		return;
	}

	enable = !!enable;
	if (enable == qdrv_radar_cb.enabled) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "Radar already %s\n",
				enable ? "enabled" : "disabled");
		return;
	}

	if (enable)
		qdrv_radar_enable_action();
	else
		qdrv_radar_disable();

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "Radar configured manually\n");
}

int qdrv_radar_detections_num(uint32_t chan)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	uint32_t chan_idx = 0;

	if (!qdrv_radar_cb.enabled)
		return -1;

	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		if (ic->ic_channels[chan_idx].ic_ieee == chan)
			break;
	}

	if (!(ic->ic_channels[chan_idx].ic_flags & IEEE80211_CHAN_DFS)) {
		return -1;
	} else {
		return (ic->ic_channels[chan_idx].ic_radardetected);
	}
}

static void qdrv_ocac_tasklet(unsigned long data)
{
	struct qtn_ocac_info *ocac_info = (struct qtn_ocac_info *)data;
        struct radar_ocac_info_s *radar_ocac_info = radar_ocac_info_addr_get();
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	uint8_t array_ps = radar_ocac_info->array_ps;
	struct ieee80211_ocac_tsflog *p_tsflog;
	bool radar_enabled = radar_get_status();

	if (!qdrv_radar_cb.enabled)
		return;

	if (!ic->ic_ocac.ocac_chan)
		return;

	/* Only do off channel CAC on non-DFS channel */
	if (qdrv_radar_is_rdetection_required(ic->ic_bsschan))
		return;

	/* enable radar if it is non-DFS channel and radar is disabled */
	if (!radar_enabled) {
		sys_enable_rdetection();
	}

	spin_lock(&radar_ocac_info->lock);
	radar_ocac_info->ocac_radar_pts[array_ps].ocac_status = ocac_info->chan_status;
	DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_RADAR, "status %d\n",
			radar_ocac_info->ocac_radar_pts[array_ps].ocac_status);
	radar_record_buffer_pt(&radar_ocac_info->ocac_radar_pts[array_ps].fifo_pt);
	radar_ocac_info->array_ps++;
	radar_ocac_info->ocac_scan_chan = ic->ic_ocac.ocac_chan->ic_ieee;
	spin_unlock(&radar_ocac_info->lock);

	if (ocac_info->chan_status == QTN_OCAC_ON_DATA_CHAN) {
		ic->ic_ocac.ocac_counts.tasklet_data_chan++;
		ic->ic_ocac.ocac_accum_cac_time_ms += ocac_info->actual_dwell_time;
		p_tsflog = &ic->ic_ocac.ocac_tsflog;
		memcpy(p_tsflog->tsf_log[p_tsflog->log_index], ocac_info->tsf_log,
				sizeof(p_tsflog->tsf_log[p_tsflog->log_index]));
		p_tsflog->log_index = (p_tsflog->log_index + 1) % QTN_OCAC_TSF_LOG_DEPTH;
		ic->ic_chan_switch_reason_record(ic, IEEE80211_CSW_REASON_OCAC_RUN);
	} else {
		ic->ic_ocac.ocac_counts.tasklet_off_chan++;
	}
}

/*
 * Send CSA frame to MuC
 */
#ifndef CONFIG_QHOP
static void sys_send_csa(struct ieee80211vap *vap, struct ieee80211_channel* new_chan, u_int64_t tsf)
{
	struct ieee80211com *ic;

	if ((vap == NULL) || (new_chan == NULL)) {
		DBGPRINTF_E("vap 0x%p, new_chan 0x%p\n", vap, new_chan);
		return;
	}
	ic = vap->iv_ic;
	ic->ic_send_csa_frame(vap, IEEE80211_CSA_MUST_STOP_TX,
				 new_chan->ic_ieee, IEEE80211_RADAR_11HCOUNT, tsf);
}
#endif

static void send_channel_related_event(struct net_device *dev, char *event_string)
{
	if (event_string == NULL || dev == NULL) {
		return;
	}

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
		"send event to userspace, dev=%s msg=%s\n", dev->name, event_string);

	radar_event_report(dev, "%s", event_string);
}


/* notify the dfs reentry demon of the channel switch info */
void dfs_reentry_chan_switch_notify(struct net_device *dev, struct ieee80211_channel *new_chan)
{
	char *dfs_chan_sw = "dfs_csa";
	char *nondfs_chan_sw = "non_dfs_csa";
	char *no_chan_valid = "csa_fail";
	char *notify_string;

	if (NULL == new_chan) {
		notify_string = no_chan_valid;
	} else if (new_chan->ic_flags & IEEE80211_CHAN_DFS){
		notify_string = dfs_chan_sw;
	} else {
		notify_string = nondfs_chan_sw;
	}

	send_channel_related_event(dev, notify_string);
}
EXPORT_SYMBOL(dfs_reentry_chan_switch_notify);



/*
 * Initiate a channel switch
 * - 'new_chan' should not be NULL
 */
static void sys_change_chan(struct ieee80211_channel *new_chan)
{
#define IS_UP(_dev)	(((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define IEEE80211_VAPS_LOCK_BH(_ic)	spin_lock_bh(&(_ic)->ic_vapslock);
#define IEEE80211_VAPS_UNLOCK_BH(_ic)	spin_unlock_bh(&(_ic)->ic_vapslock);

	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (!new_chan || !vap) {
		DBGPRINTF_E("null channel or vap\n");
		return;
	}
	/* if dfs channel the notify will be send after cac */
	if (!(new_chan->ic_flags & IEEE80211_CHAN_DFS))
		dfs_reentry_chan_switch_notify(vap->iv_dev, new_chan);


	if (IS_UP(vap->iv_dev)) {
		ic->ic_prevchan = ic->ic_curchan;
		ic->ic_curchan = ic->ic_des_chan = new_chan;
		ic->ic_csw_reason = IEEE80211_CSW_REASON_DFS;
		IEEE80211_VAPS_LOCK_BH(ic);
		vap->iv_newstate(vap, IEEE80211_S_SCAN, 0);
		IEEE80211_VAPS_UNLOCK_BH(ic);
		ic->ic_flags &= ~IEEE80211_F_CHANSWITCH;
	} else if (vap->iv_state == IEEE80211_S_RUN) {
		/* Normally, we don't get to here */
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if ((vap->iv_opmode == IEEE80211_M_WDS) && (vap->iv_state == IEEE80211_S_RUN)) {
				IEEE80211_VAPS_LOCK_BH(ic);
				vap->iv_newstate(vap, IEEE80211_S_INIT, 0);
				IEEE80211_VAPS_UNLOCK_BH(ic);
			}
		}

		ic->ic_prevchan = ic->ic_curchan;
		ic->ic_curchan = new_chan;
		ic->ic_bsschan = new_chan;
		ic->ic_csw_reason = IEEE80211_CSW_REASON_DFS;
		ic->ic_set_channel(ic);
		ic->ic_flags &= ~IEEE80211_F_CHANSWITCH;

		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if ((vap->iv_opmode == IEEE80211_M_WDS) && (vap->iv_state == IEEE80211_S_INIT)) {
				IEEE80211_VAPS_LOCK_BH(ic);
				vap->iv_newstate(vap, IEEE80211_S_RUN, 0);
				IEEE80211_VAPS_UNLOCK_BH(ic);
			}

			if (vap->iv_opmode != IEEE80211_M_HOSTAP)
				continue;

			if ((vap->iv_state != IEEE80211_S_RUN) && (vap->iv_state != IEEE80211_S_SCAN))
				continue;

			ic->ic_beacon_update(vap);
		}
	} else {
		ic->ic_flags &= ~IEEE80211_F_CHANSWITCH;
		DBGPRINTF_E("channel change failed\n");
	}
}

/*
 * CAC has successfully passed
 */
static void cac_completed_action(unsigned long data)
{
	struct ieee80211com *ic;
	struct qdrv_wlan *qw;
	struct ieee80211_channel *chan;
	struct ieee80211vap *vap;
	int chan_status = 0;

	ic = qdrv_radar_cb.ic;
	if (ic == NULL || !is_cac_started()) {
		DBGPRINTF_E("CAC not in progress\n");
		return;
	}

	vap = TAILQ_FIRST(&ic->ic_vaps);
	if (vap == NULL || vap->iv_dev == NULL) {
		return;
	}
	qw = container_of(ic, struct qdrv_wlan, ic);
	chan = qdrv_radar_cb.cac_chan;
	/* resume normal operation on channel */
	sys_enable_xmit();
	chan->ic_flags |= IEEE80211_CHAN_DFS_CAC_DONE;
	chan->ic_flags &= ~IEEE80211_CHAN_DFS_CAC_IN_PROGRESS;

	if (ic->sta_dfs_info.sta_dfs_strict_mode) {
		if (!(vap->iv_bss && IEEE80211_NODE_AID(vap->iv_bss))) {
			chan_status = IEEE80211_CHANNEL_STATUS_NON_AVAILABLE;
		} else {
			chan_status = IEEE80211_CHANNEL_STATUS_AVAILABLE;
		}
	} else {
		chan_status = IEEE80211_CHANNEL_STATUS_AVAILABLE;
	}

	if (ic->ic_mark_channel_availability_status) {
		ic->ic_mark_channel_availability_status(ic, chan, chan_status);
	}

	if (ic->ic_mark_channel_dfs_cac_status) {
		if (ic->sta_dfs_info.sta_dfs_strict_mode && (chan_status == IEEE80211_CHANNEL_STATUS_NON_AVAILABLE)) {
			ic->ic_mark_channel_dfs_cac_status(ic, chan, IEEE80211_CHAN_DFS_CAC_DONE, false);
		} else {
			ic->ic_mark_channel_dfs_cac_status(ic, chan, IEEE80211_CHAN_DFS_CAC_DONE, true);
		}
		ic->ic_mark_channel_dfs_cac_status(ic, chan, IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, false);
	}

	DBGPRINTF_N_QEVT(vap->iv_dev, "RADAR: CAC completed for channel %3d (%4d MHz)\n",
		chan->ic_ieee, chan->ic_freq);

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "\n%s: chan_status=%d\n", __func__, chan_status);

	QDRV_CLEAR_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_CAC_ACTIVE);
	/* cac has ended, it means can switch to a dfs channel succed*/
	dfs_reentry_chan_switch_notify(vap->iv_dev, qdrv_radar_cb.cac_chan);
	qdrv_radar_cb.cac_chan = NULL;

	if (ic->ic_ap_next_cac) {
		ic->ic_ap_next_cac(ic, vap, CAC_PERIOD, &qdrv_radar_cb.cac_chan,
					IEEE80211_SCAN_PICK_NOT_AVAILABLE_DFS_ONLY);
	}
}

void qdrv_cac_instant_completed(void)
{
	struct timer_list *cac_timer;

	if (!is_cac_started())
		return;

	KASSERT((qdrv_radar_cb.cac_chan->ic_flags & IEEE80211_CHAN_DFS) != 0,
			(DBGEFMT "CAC started on non-DFS channel: %3d (%4d MHz)\n",
			DBGARG, qdrv_radar_cb.cac_chan->ic_ieee,
			qdrv_radar_cb.cac_chan->ic_freq));
	KASSERT(!(qdrv_radar_cb.cac_chan->ic_flags & IEEE80211_CHAN_DFS_CAC_DONE),
			(DBGEFMT "CAC_DONE marked prior to CAC completed\n", DBGARG));

	cac_timer = &qdrv_radar_cb.cac_timer;
	mod_timer(cac_timer, jiffies);
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "RADAR: CAC period will expire instantly\n");
}

/*
 * Start or restart the CAC procedure
 * - precondition: transmission is already disabled
 */
static void start_cac(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *cur_chan = ic->ic_curchan;
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);
	struct timer_list *cac_timer = &qdrv_radar_cb.cac_timer;
	unsigned long expires;

	/* CAC not required for DFS slave */
	if (qdrv_is_dfs_slave() && !(ic->sta_dfs_info.sta_dfs_strict_mode))
		return;

	/* stop cac if any */
	stop_cac();

	KASSERT(qdrv_radar_cb.cac_chan == NULL,
		(DBGEFMT "CAC channel is not null\n", DBGARG));

	if (cur_chan == IEEE80211_CHAN_ANYC) {
		DBGPRINTF_E("operational channel not yet selected\n");
		return;
	}

	/* save the operational channel into the control block */
	qdrv_radar_cb.cac_chan = cur_chan;
	cur_chan->ic_flags |= IEEE80211_CHAN_DFS_CAC_IN_PROGRESS;

	if (ic->ic_mark_channel_dfs_cac_status) {
		ic->ic_mark_channel_dfs_cac_status(ic, cur_chan, IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, true);
	}

	if (ieee80211_is_on_weather_channel(ic, cur_chan))
		expires = jiffies + CAC_WEATHER_PERIOD_EU;
	else
		expires = jiffies + CAC_PERIOD;

	if (DBG_LOG_FUNC_TEST(QDRV_LF_DFS_QUICKTIMER)) {
		DBGPRINTF_N("RADAR: test mode - CAC period will expire quickly\n");
		expires = jiffies + CAC_PERIOD_QUICK;
	}
	mod_timer(cac_timer, expires);

	QDRV_SET_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_CAC_ACTIVE);
	DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: CAC started for channel %3d (%4d MHz)\n",
			 cur_chan->ic_ieee, cur_chan->ic_freq);
}

/*
 * Stop cac procedure
 */
static void raw_stop_cac(void)
{
	struct ieee80211_channel *chan = qdrv_radar_cb.cac_chan;
	struct timer_list *cac_timer = &qdrv_radar_cb.cac_timer;
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (!is_cac_started()) { /* no cac to stop */
		DBGPRINTF_E("CAC is not started\n");
		return;
	}

	del_timer(cac_timer);
	chan->ic_flags &= ~(IEEE80211_CHAN_DFS_CAC_DONE |
			IEEE80211_CHAN_DFS_CAC_IN_PROGRESS);

	if (ic->ic_mark_channel_dfs_cac_status) {
		ic->ic_mark_channel_dfs_cac_status(ic, chan, IEEE80211_CHAN_DFS_CAC_DONE, false);
		ic->ic_mark_channel_dfs_cac_status(ic, chan, IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, false);
	}

	QDRV_CLEAR_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_CAC_ACTIVE);
	DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: CAC stopped for channel %3d (%4d MHz)\n",
			 chan->ic_ieee, chan->ic_freq);

	/* no cac now */
	qdrv_radar_cb.cac_chan = NULL;
	/* take it as an channel switch failed event
	 * to satisfy the dfs reentry demon when it's waiting for the dfs reentry result */
	if (vap && vap->iv_dev)
		dfs_reentry_chan_switch_notify(vap->iv_dev, NULL);
}

static void stop_cac(void)
{
	if (is_cac_started())
		raw_stop_cac();
}

void qdrv_radar_stop_active_cac(void)
{
	if (!qdrv_radar_cb.enabled)
		return;

	if (is_cac_started()) {
		raw_stop_cac();
		sys_enable_xmit();
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "%s: stop CAC\n", __func__);
	}
	return;
}

void sta_dfs_strict_cac_action(struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;

	if (!ic->sta_dfs_info.sta_dfs_strict_mode) {
		return;
	}

	if (!qdrv_radar_cb.enabled)
		return;

	if (ieee80211_is_chan_cac_required(chan)) {
		sys_disable_xmit();
		start_cac();
	} else if (ieee80211_is_chan_not_available(chan) || ieee80211_is_chan_available(chan)) {
		sys_enable_xmit();
	}
}

/*
 * The non-occupancy period expires
 * - the channel is now available for use
 */
static void nonoccupy_expire_action(unsigned long data)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	unsigned chan_idx = data;
	struct ieee80211_channel *chan;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	KASSERT(chan_idx < ic->ic_nchans,
		(DBGEFMT "out-of-range channel idx %u\n", DBGARG, chan_idx));

	chan = &ic->ic_channels[chan_idx];
	chan->ic_flags &= ~IEEE80211_CHAN_RADAR;

	if (ic->ic_flags_qtn & IEEE80211_QTN_RADAR_SCAN_START) {
		if (ic->ic_initiate_scan) {
			ic->ic_initiate_scan(vap);
		}
	}

	/* Mark the channel as not_available and ready for cac */
	if (ic->ic_mark_channel_availability_status) {
		ic->ic_mark_channel_availability_status(ic, chan, IEEE80211_CHANNEL_STATUS_NOT_AVAILABLE_CAC_REQUIRED);
	}

	if ((ic->sta_dfs_info.sta_dfs_strict_mode)
		&& (ic->ic_curchan == chan)) {
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if ((vap->iv_state != IEEE80211_S_RUN)
				&& (vap->iv_state != IEEE80211_S_SCAN)) {
				continue;
			}
			DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "%s: Trigger scan\n", __func__);
			vap->iv_newstate(vap, IEEE80211_S_SCAN, 0);
		}
	}

	DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: non-occupancy period expired for channel %3d "
			 "(%4d MHz)\n", chan->ic_ieee, chan->ic_freq);
}

static void sta_radar_detected_timer_action(unsigned long chan_ic_ieee)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *chan = ieee80211_find_channel_by_ieee(ic, chan_ic_ieee);
	ic->sta_dfs_info.sta_dfs_radar_detected_timer = false;
	if (ic->ic_mark_channel_availability_status) {
		ic->ic_mark_channel_availability_status(ic, chan,
				IEEE80211_CHANNEL_STATUS_NOT_AVAILABLE_RADAR_DETECTED);
	}
	if (ic->ic_chan_compare_equality(ic, ic->ic_curchan, chan)) {
		sys_disable_xmit();
	}
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "%s: sta_radar_timer expired\n", __func__);
}

/*
 * Time to perform channel switch
 */
static void dfs_cs_timer_expire_action(unsigned long data)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (is_dfs_cs_started()) {
		struct ieee80211_channel *chan = qdrv_radar_cb.dfs_des_chan;

		if (qdrv_radar_cb.dfs_des_chan != IEEE80211_CHAN_ANYC){
			DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: DFS channel switch to %3d (%4d MHz)\n",
					 chan->ic_ieee, chan->ic_freq);
			sys_change_chan(chan);
		} else {
			/* disable the transmission before starting the AP scan */
			sys_disable_xmit();

			/* no channel selected by radar module. Call Scanner */
			DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: starting AP scan due to radar "
					 "detection\n");
			(void) ieee80211_start_scan(vap, IEEE80211_SCAN_NO_DFS,
				IEEE80211_SCAN_FOREVER, 0, NULL);
		}

		qdrv_radar_cb.dfs_des_chan = NULL;
	}
}

/*
 * Start a DFS-triggered channel switch
 */
#ifndef CONFIG_QHOP
static void start_dfs_cs(struct ieee80211_channel *new_chan)
{
	struct timer_list *dfs_cs_timer = &qdrv_radar_cb.dfs_cs_timer;

	if (is_dfs_cs_started())
		stop_dfs_cs();

	qdrv_radar_cb.dfs_des_chan = new_chan;
	mod_timer(dfs_cs_timer, jiffies + DFS_CS_TIMER_VAL);
}
#endif

/*
 * Stop the DFS-triggered channel switch
 */
static void stop_dfs_cs()
{
	struct timer_list *dfs_cs_timer = &qdrv_radar_cb.dfs_cs_timer;

	if (is_dfs_cs_started()) {
		del_timer(dfs_cs_timer);
		qdrv_radar_cb.dfs_des_chan = NULL;
	}
}

static struct ieee80211_channel *qdrv_validate_fs_chan(int fast_switch, u_int8_t new_ieee)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *chan = NULL;
	struct ieee80211_channel *new_chan = NULL;
	unsigned chan_idx;

	if (new_ieee == 0) {
		return NULL;
	}

	chan = ic->ic_channels;
	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++, chan++) {
		if (chan->ic_ieee == new_ieee) {
			new_chan = chan;
			break;
		}
	}

	if (new_chan == NULL) {
		DBGPRINTF_E("channel %d not found\n", new_ieee);
	} else if (!ic->ic_check_channel(ic,  chan, fast_switch, 0)) {
		DBGPRINTF_E("channel %d is not usable\n", new_ieee);
		new_chan = NULL;
	}

	return new_chan;
}

/*
 * Select a new channel to use
 * - according to FCC/ETSI rules on uniform spreading, we shall select a
 * channel out of the list of usable channels so that the probability
 * of selecting a given channel shall be the same for all channels
 * (reference: ETSI 301 893 v1.5.1 $4.7.2.6)
 * - possible for this function to return NULL
 * - a random channel can be returned if the specified channel is neither
 *	 found nor usable
 */
struct ieee80211_channel *qdrv_radar_select_newchan(u_int8_t new_ieee)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *chan;
	struct ieee80211_channel *new_chan = NULL;
	unsigned chan_idx;
	int fast_switch = (ic->ic_flags_ext & IEEE80211_FEXT_DFS_FAST_SWITCH) != 0;


	/* check if we can switch to the user configured channel */
	new_chan = qdrv_validate_fs_chan(fast_switch, new_ieee);

	if ((new_chan == NULL) && (new_ieee != ic->ic_ieee_best_alt_chan)) {
		new_chan = qdrv_validate_fs_chan(fast_switch, ic->ic_ieee_best_alt_chan);
	}

	/* select a random channel */
	if (new_chan == NULL) {
		unsigned count;
		chan = ic->ic_channels;
		for (count = 0, chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++, chan++) {
			if (ic->ic_check_channel(ic, chan, fast_switch, 0)) {
				count++;
			}
		}

		if (count != 0) {
			unsigned rand = jiffies % count;

			chan = ic->ic_channels;
			for (count = 0, chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++, chan++) {
				if (ic->ic_check_channel(ic, chan, fast_switch, 0)) {
					if (count++ == rand) {
						new_chan = &ic->ic_channels[chan_idx];
						break;
					}
				}
			}
		}
	}

	if (new_chan) {
		new_chan = ieee80211_chk_update_pri_chan(ic, new_chan, 0, "Radar", 0);
	}

	if (new_chan) {
		DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: new channel selected %d (%d MHz)\n",
				 new_chan->ic_ieee, new_chan->ic_freq);
	} else {
		DBGPRINTF_E("no valid channel found\n");
	}

	return new_chan;
}
EXPORT_SYMBOL(qdrv_radar_select_newchan);

/*
 * Perform the dfs related action after new channel has been selected
 */
static void dfs_action_after_newchan_select(struct ieee80211_channel *new_chan)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *cur_chan = ic->ic_curchan;
	struct ieee80211vap *vap;
	bool vap_found = false;
#ifndef CONFIG_QHOP
	struct ieee80211_channel *csa_chan;
	uint64_t tsf = 0;
#endif

	if (new_chan == NULL) {
		vap = TAILQ_FIRST(&ic->ic_vaps);
		dfs_reentry_chan_switch_notify(vap->iv_dev, new_chan);
		DBGPRINTF_E("new channel not found or usable\n");
		return;
	}

#ifdef CONFIG_QHOP
	/* If the node is MBS send CSA frames */
	if (!ieee80211_scs_is_wds_rbs_node(ic)) {
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if (vap->iv_opmode != IEEE80211_M_HOSTAP)
				continue;

			if (vap->iv_state != IEEE80211_S_RUN)
				continue;

			ieee80211_dfs_send_csa(vap, new_chan->ic_ieee);
		}
	}
#else
	/*
	 * Just use CSA action frame, so set ic_csa_count to zero and
	 * avoid CSA ie included in beacon.
	 */
	ic->ic_flags |= IEEE80211_F_CHANSWITCH;
	ic->ic_csa_count = 0;

	/* send CSA action frame for each vap */
	csa_chan = new_chan;

	ic->ic_get_tsf(&tsf);
	tsf += IEEE80211_MS_TO_USEC(QDRV_RADAR_DFLT_CHANSW_MS);

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		/* Just skip WDS mode, because not sure if we support DFS on STA later */
		if (vap->iv_opmode == IEEE80211_M_WDS)
			continue;

		if ((vap->iv_state != IEEE80211_S_RUN) && (vap->iv_state != IEEE80211_S_SCAN))
			continue;

		vap_found = true;
		sys_send_csa(vap, csa_chan, tsf);
	}

	start_dfs_cs(new_chan);
#endif

	ic->ic_dfs_cce.cce_previous = cur_chan->ic_ieee;
	ic->ic_dfs_cce.cce_current = new_chan->ic_ieee;

	if (vap_found != true )
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_RADAR, "no vap running\n");
}

static void
dfs_send_report_frame(struct ieee80211com *ic, struct ieee80211vap *vap) {
	struct ieee80211_node *ni;
	struct ieee80211_channel *cur_chan;
	struct ieee80211_meas_report_ctrl mreport_ctrl;
	struct ieee80211_action_data action_data;

	/* DFS enabled STA sends Autonomous Measurement Report Action Frame to AP*/
	if (vap == NULL)
		return;

	KASSERT(vap->iv_state == IEEE80211_S_RUN, (DBGEFMT "Radar send reprot "
			"frame, vap state incorrect: %d\n", DBGARG, vap->iv_state));

	memset(&mreport_ctrl, 0, sizeof(mreport_ctrl));
	memset(&action_data, 0, sizeof(action_data));
	ni = vap->iv_bss;
	cur_chan = ic->ic_curchan;

	mreport_ctrl.meas_type = IEEE80211_CCA_MEASTYPE_BASIC;
	mreport_ctrl.report_mode = 0;
	mreport_ctrl.autonomous = 1;
	mreport_ctrl.u.basic.channel = ieee80211_chan2ieee(ic, cur_chan);
	mreport_ctrl.u.basic.basic_report |= IEEE80211_MEASURE_BASIC_REPORT_RADAR;
	action_data.cat		= IEEE80211_ACTION_CAT_SPEC_MGMT;
	action_data.action	= IEEE80211_ACTION_S_MEASUREMENT_REPORT;
	action_data.params	= &mreport_ctrl;
	ic->ic_send_mgmt(ni, IEEE80211_FC0_SUBTYPE_ACTION, (int)&action_data);

	return;
}

static void
dfs_slave_push_state_machine(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if ((vap->iv_state != IEEE80211_S_RUN) &&
		    (vap->iv_state != IEEE80211_S_SCAN)) {
			continue;
		}

		vap->iv_newstate(vap, IEEE80211_S_SCAN, 0);
	}

	ic->ic_chan_switch_reason_record(ic, IEEE80211_CSW_REASON_DFS);
	return;
}

#ifdef CONFIG_QHOP
static void
dfs_send_qhop_report_frame(struct ieee80211com *ic, u_int8_t new_ieee)
{
	struct ieee80211vap *vap;

	/*
	 * If this is an RBS we send the reports to the MBS on the WDS link
	 * Note: We are assuming hub and spoke topology. For general tree or mesh
	 * much more sophisticated routing algorithm should be implemented
	 */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		/* Note: We are assuming hub and spoke topology. For general tree or mesh */
		/* much more sophisticated routing algorithm should be implemented */
		if (IEEE80211_VAP_WDS_IS_RBS(vap)) {
			qdrv_qhop_send_rbs_report_frame(vap, new_ieee);
			return;
		}
	}
}
#endif

/*
 * Perform the dfs action including channel switch.
 */
static void dfs_action(uint8_t new_ieee, bool radar_detected_during_cac)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *new_chan = NULL;
	struct ieee80211vap *vap;
	uint32_t repeater_mode;

#ifdef CONFIG_QHOP
	if (ic->ic_extender_role == IEEE80211_EXTENDER_ROLE_RBS) {
		dfs_send_qhop_report_frame(ic, new_ieee);
		return;
	}
#endif
	vap = TAILQ_FIRST(&ic->ic_vaps);
	repeater_mode = ieee80211_is_repeater(ic);
	if (qdrv_is_dfs_slave() || repeater_mode) {

		bool send_measurement_in_sta_dfs_strict = ic->sta_dfs_info.sta_dfs_strict_mode &&
							ic->sta_dfs_info.sta_dfs_strict_msr_cac &&
							radar_detected_during_cac;

		/* DFS slave just initiates scan as DFS action */
		if (vap && vap->iv_state == IEEE80211_S_RUN) {
			if ((qdrv_radar_cb.xmit_stopped == true)
					&& (!send_measurement_in_sta_dfs_strict)) {
				DBGPRINTF(DBG_LL_WARNING, QDRV_LF_RADAR,
						"%s report radar failed\n",
						repeater_mode ? "Repeater" : "STA");
			} else {
				DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
						"%s report radar to master\n",
						repeater_mode ? "Repeater" : "STA");

				if (send_measurement_in_sta_dfs_strict) {
					DBGPRINTF_N_QEVT(vap->iv_dev, "STA-DFS: Sending measurement frame during CAC\n");
					sys_enable_xmit();
					ic->sta_dfs_info.allow_measurement_report = true;
				}

				dfs_send_report_frame(ic, vap);
				if (qdrv_radar_sta_dfs || repeater_mode)
					return;
			}
		}

		if (!repeater_mode) {
			dfs_slave_push_state_machine(ic);
			return;
		}
		/*
		 * If STA interface of a repeater do not associated with
		 * any root-AP,the repeater should act as if it's an AP.
		 */
	}

	/*
	 * Default behavior for AP:
	 * If radar detected during ICAC, continue ICAC on next NOT_AVAILABLE_DFS channel
	 * If DFS fast switch configured, do random channel selection or fixed channel
	 * based on customer's configuration;
	 * If DFS fast switch not configured, use channel scan to pick up a best non-DFS channel
	 */
	if (ic->ic_get_init_cac_duration(ic) > 0) {
		if (ic->ic_ap_next_cac && vap) {
			ic->ic_ap_next_cac(ic, vap, CAC_PERIOD, &qdrv_radar_cb.cac_chan,
					IEEE80211_SCAN_PICK_NOT_AVAILABLE_DFS_ONLY);
		}
	} else if (ic->ic_flags_ext & IEEE80211_FEXT_DFS_FAST_SWITCH) {
		/*
		 * select one channel at random
		 */
		new_chan = qdrv_radar_select_newchan(new_ieee);
		dfs_action_after_newchan_select(new_chan);
	} else {
		/*
		 * Using channel scan to pick up a best non-DFS channel to switch
		 * Channel switch and DFS action will be done after scanning is done
		 */
		vap = TAILQ_FIRST(&ic->ic_vaps);
		ieee80211_start_scan(vap, IEEE80211_SCAN_FLUSH | IEEE80211_SCAN_NO_DFS
				| IEEE80211_SCAN_DFS_ACTION, IEEE80211_SCAN_FOREVER,
				vap->iv_des_nssid, vap->iv_des_ssid);
	}

	ic->ic_chan_switch_reason_record(ic, IEEE80211_CSW_REASON_DFS);
}

void qdrv_dfs_action_scan_done(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *new_chan = NULL;

	IEEE80211_LOCK_IRQ(ic);
	new_chan = ieee80211_scan_pickchannel(ic, IEEE80211_SCAN_NO_DFS);
	IEEE80211_UNLOCK_IRQ(ic);

	dfs_action_after_newchan_select(new_chan);
}

/*
 * Decide whether or not to detect radar on the channel
 */
bool qdrv_radar_is_rdetection_required(const struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	bool rdetect = false;
	bool doth = ic->ic_flags & IEEE80211_F_DOTH;

	if (DBG_LOG_FUNC_TEST(QDRV_LF_DFS_DONTCAREDOTH)) {
		DBGPRINTF_N("RADAR: test mode - detection enabled\n");
		doth = true;
	}

	if (doth) {
		if (chan == IEEE80211_CHAN_ANYC) {
			DBGPRINTF_E("channel not yet set\n");
			return false;
		}

		if (chan->ic_flags & IEEE80211_CHAN_DFS)
			rdetect = true;
	}

	return rdetect;
}

static int32_t qdrv_radar_off_chan_cac_action(struct ieee80211com *ic)
{
	struct radar_ocac_info_s *qdrv_ocac_info;
	uint8_t ocac_scan_chan;

	if (qdrv_is_dfs_slave())
		return -EINVAL;

	if (ieee80211_is_repeater(ic))
		return -EINVAL;

	qdrv_ocac_info = radar_ocac_info_addr_get();
	ocac_scan_chan = qdrv_ocac_info->ocac_scan_chan;
	if (qdrv_radar_cb.region == DFS_RQMT_US) {
		DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: radar found on channel %u during CAC\n",
				 ocac_scan_chan);
	} else {
		DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: radar found on off channel %u, current "
				 "chan %u\n", ocac_scan_chan, ic->ic_curchan->ic_ieee);
	}

	return ocac_scan_chan;
}

/*
 * Invoked when a radar is detected.
 * Called directly from iwpriv wifi0 doth_radar <new channel>.
 * Called when AP receives a radar detection report from an associated STA.
 */
void qdrv_radar_detected(struct ieee80211com *ic, u_int8_t new_ieee)
{
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);
	int retval = -1;
	uint8_t local_new_ieee = new_ieee;
	uint32_t chan_idx;
	struct ieee80211_channel *chan = NULL;
	struct ieee80211vap *vap;
	bool rdetect;
	int32_t radar_chan;
	bool radar_detected_during_cac = is_cac_started();

	if (!qdrv_radar_configured) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "radar not initialized\n");
		return;
	}

	if (!qdrv_radar_cb.enabled) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "radar not enabled\n");
		return;
	}

	if (ic != qdrv_radar_cb.ic) {
		DBGPRINTF_E("ic 0x%p not matching the configured ic 0x%p\n",
			ic, qdrv_radar_cb.ic);
		return;
	}

	/* stop cac if any */
	stop_cac();

	if (qdrv_is_dfs_slave() && !qdrv_radar_sta_dfs) {
		DBGPRINTF_E("ic mode is %d and sta dfs is %s\n", ic->ic_opmode,
				qdrv_radar_sta_dfs ? "enabled" : "disabled");
		return;
	}

	rdetect = qdrv_radar_is_rdetection_required(ic->ic_curchan);
	if (!rdetect) {
		/* detect radar during off channel CAC */
		if (!ic->ic_ocac.ocac_chan) {
			DBGPRINTF_E("radar operating channel invalid\n");
			return;
		}
		radar_chan = qdrv_radar_off_chan_cac_action(ic);
	} else {
		radar_chan = ic->ic_curchan->ic_ieee;
	}

	if (radar_chan < 0) {
		DBGPRINTF_E("radar operating channel invalid\n");
		return;
	}

	/* get an in-service channel */
	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		if (ic->ic_channels[chan_idx].ic_ieee == radar_chan) {
			chan = &ic->ic_channels[chan_idx];
			break;
		}
	}
	if (!chan) {
		DBGPRINTF_E("no matching in-service channel for freq=%d\n",
				ic->ic_curchan->ic_freq);
		return;
	}
	KASSERT((chan->ic_flags & IEEE80211_CHAN_DFS), (DBGEFMT "Radar"
				" detected on non-DFS channel\n", DBGARG));
	DBGPRINTF_N_QEVT(ic2dev(ic), "RADAR: radar found on channel %3d (%4d MHz)\n",
			 chan->ic_ieee, chan->ic_freq);

	/*
	 * To avoid repeated dfs actions when AP and STAs detected
	 * same radar, test flag here. (only for AP side)
	 */
	if ((chan->ic_flags & IEEE80211_CHAN_RADAR) && !(ic->sta_dfs_info.sta_dfs_strict_mode)) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
				"DFS marked on channel %3d (%4d MHz) already\n",
				chan->ic_ieee, chan->ic_freq);
		return;
	}

	if (qw->radar_detect_callback) {
		retval = qw->radar_detect_callback(chan);
		if (retval == 0)
			return;
	}

	/* check if dfs marking is allowed */
	if (!(ic->ic_flags_ext & IEEE80211_FEXT_MARKDFS)) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "DFS marking disabled\n");
		return;
	}

	/* return immediately if we are in the dfs test mode */
	if (qdrv_radar_is_test_mode()) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_RADAR | QDRV_LF_DFS_TESTMODE,
				"test mode - no DFS action taken\n");
		if (qdrv_is_dfs_master()) {
			DBGPRINTF(DBG_LL_CRIT, QDRV_LF_RADAR | QDRV_LF_DFS_TESTMODE,
					"send CSA action\n");
			vap = TAILQ_FIRST(&ic->ic_vaps);
			ieee80211_dfs_send_csa(vap, ic->ic_curchan->ic_ieee);
		}
		return;
	}

	/* set radar_found_flag eariler to avoid function reentry issue */
	start_nonoccupy(chan_idx);

	/* stop cac if any */
	stop_cac();

	/* OCAC do not required for DFS actions */
	if (!rdetect) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
				"OCAC DFS: no DFS action taken\n");
		return;
	}

	/* disable radar detection to avoid redundant detection */
	sys_disable_rdetection();

	if (local_new_ieee == 0 && ic->ic_ieee_alt_chan != 0) {
		local_new_ieee = ic->ic_ieee_alt_chan;
        }
	/* take a dfs action */
	dfs_action(local_new_ieee, radar_detected_during_cac);

}

/*
 * Invoked when radar is detected
 * - a callback function registered to the radar module
 */
static void mark_radar(void)
{
	qdrv_radar_detected(qdrv_radar_cb.ic, 0);
}

int qdrv_radar_test_mode_enabled(void)
{
	if ((qdrv_radar_cb.enabled == true) && qdrv_radar_is_test_mode())
		return 1;

	return 0;
}

/*
 * Check if safe to perform channel sampling
 * Returns 1 if OK, else 0.
 */
int qdrv_radar_can_sample_chan(void)
{
	if ((qdrv_radar_cb.enabled != 0) &&
		is_cac_started()) {
		return 0;
	}

	if (qdrv_radar_test_mode_enabled()) {
		return 0;
	}

	return 1;
}

/*
 * Take appropriate action(s) right before channel switch
 */
void qdrv_radar_before_newchan(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);
	struct ieee80211_channel *new_chan = NULL;
	bool rdetect;

	if (!qdrv_radar_cb.enabled)
		return;

	if (ic->ic_flags & IEEE80211_F_SCAN) {
		if (is_cac_started()) {
			/* The ongoing CAC is invalid since channel scan is running */
			qdrv_radar_cb.cac_chan->ic_flags &=
				~IEEE80211_CHAN_DFS_CAC_IN_PROGRESS;

			if (ic->ic_mark_channel_dfs_cac_status) {
				ic->ic_mark_channel_dfs_cac_status(ic, qdrv_radar_cb.cac_chan,
							IEEE80211_CHAN_DFS_CAC_IN_PROGRESS, false);
			}
		}
		return;
	}

	/* stop cac if any */
	stop_cac();

	/* now safe to set 'new_chan' */
	new_chan = ic->ic_curchan;

	/* check if the new channel requires radar detection */
	rdetect = qdrv_radar_is_rdetection_required(new_chan);

	/* other channel switches override the DFS-triggered one */
	if (is_dfs_cs_started() && (qdrv_radar_cb.dfs_des_chan != new_chan)) {
		stop_dfs_cs();
	}

	if (rdetect) {
		QDRV_SET_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_RADAR_ACT);
		sys_disable_xmit();
		sys_disable_rdetection();
	} else {
		QDRV_CLEAR_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_RADAR_ACT);
	}
}

void qdrv_radar_enable_radar_detection(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;

	if (!qdrv_radar_cb.enabled)
		return;

	if (ic->ic_flags & IEEE80211_F_SCAN)
		return;

	if (qdrv_radar_is_rdetection_required(ic->ic_curchan)) {
		radar_set_chan(ic->ic_curchan->ic_ieee);
		if (!radar_get_status()) {
			if (ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] < BOARD_PM_LEVEL_DUTY) {
				sys_enable_rdetection();
			}
		}
	}
}

/*
 * Decide what to do on the new channel
 */
void qdrv_radar_on_newchan(void)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *new_chan = NULL;
	bool rdetect;
        int handle_cac = 0;

	if (!qdrv_radar_cb.enabled)
		return;

	if (ic->ic_flags & IEEE80211_F_SCAN)
		return;

	/* now safe to set 'new_chan' */
	new_chan = ic->ic_curchan;

	if (new_chan == NULL) {
		return;
	}

	handle_cac = !(IEEE80211_IS_CHAN_CACDONE(ic->ic_curchan)) &&
                        !(IEEE80211_IS_CHAN_CAC_IN_PROGRESS(ic->ic_curchan));

	/* report new channel to the radar module */
	radar_set_chan(new_chan->ic_ieee);

	/* check if radar detection on the channel is requried */
	rdetect = qdrv_radar_is_rdetection_required(new_chan);

	/* log a new channel info */
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
			"now on channel %3d (%4d MHz) with DFS %s (F_DOTH %d, CHAN_DFS %d)\n",
			new_chan->ic_ieee, new_chan->ic_freq,
			(rdetect) ? "enabled" : "disabled",
			(ic->ic_flags & IEEE80211_F_DOTH) ? 1 : 0 ,
			(new_chan->ic_flags & IEEE80211_CHAN_DFS) ? 1 : 0);

	if (rdetect) {
		if (new_chan->ic_flags & IEEE80211_CHAN_DFS_OCAC_DONE) {
			DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,  "Seamless CAC completed "
					"and no action needed\n");
			sys_enable_xmit();
			new_chan->ic_flags |= IEEE80211_CHAN_DFS_CAC_DONE;
			if (ic->ic_mark_channel_availability_status) {
				ic->ic_mark_channel_availability_status(ic, new_chan, IEEE80211_CHANNEL_STATUS_AVAILABLE);
			}

			if (ic->ic_mark_channel_dfs_cac_status) {
				ic->ic_mark_channel_dfs_cac_status(ic, new_chan, IEEE80211_CHAN_DFS_CAC_DONE, true);
			}

		} else if (qdrv_is_dfs_master()) {
			if (handle_cac) {
				start_cac();
			} else if (ieee80211_is_chan_available(new_chan)) {
				sys_enable_xmit();
			}
		} else if (ic->sta_dfs_info.sta_dfs_strict_mode) {
			sta_dfs_strict_cac_action(new_chan);
		}

		if (!radar_get_status()) {
			if (ic->ic_pm_state[QTN_PM_CURRENT_LEVEL] < BOARD_PM_LEVEL_DUTY) {
				sys_enable_rdetection();
			}
		}
	} else {
		sys_enable_xmit();
		sys_disable_rdetection();
	}
}

void qdrv_sta_dfs_enable(int sta_dfs_enable)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	bool rdetect;

	if (!qdrv_radar_configured)
		return;

	if (qdrv_radar_first_call)
		return;

	if (qdrv_is_dfs_master())
		return;

	if (qdrv_is_dfs_slave() && !qdrv_radar_sta_dfs)
		return;

	if (sta_dfs_enable) {
		rdetect = qdrv_radar_is_rdetection_required(ic->ic_bsschan);
		if (rdetect)
			radar_set_chan(ic->ic_bsschan->ic_ieee);

		qdrv_radar_enable_action();

		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_RADAR, "Station DFS enable\n");
	} else {
		qdrv_radar_disable();

		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_RADAR, "Station DFS disable\n");
	}
}

/*
 * Enable DFS feature
 */
void qdrv_radar_enable(const char *region)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211vap *vap;

	if (!qdrv_radar_configured) {
		DBGPRINTF_E("radar unconfigured\n");
		return;
	}

	if (qdrv_radar_cb.enabled) {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_RADAR, "radar already enabled\n");
		return;
	} else if (strcmp(region, "ru") == 0) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
			"no DFS / radar requirement for regulatory region Russia\n");
		return;
	}

	ieee80211_ocac_update_params(ic, region);

	if (qdrv_radar_first_call) {
		if (false == sys_start_radarmod(region)) {
			DBGPRINTF_E("Fail to start radar module\n");
			return;
		}
		qdrv_radar_first_call = false;
		qdrv_radar_sta_dfs = sta_dfs_is_region_required();
		qdrv_radar_cb.region = dfs_rqmt_code(region);

		/* initialise MuC sampling */
		qdrv_radar_cb.muc_sampling.sample = detect_drv_sample_loc_get();
		ic->ic_sample_rate = QDRV_RADAR_SAMPLE_RATE;
		vap = TAILQ_FIRST(&ic->ic_vaps);
		if (vap != NULL && qdrv_radar_cb.muc_sampling.sample != NULL) {
			ic->ic_setparam(vap->iv_bss, IEEE80211_PARAM_SAMPLE_RATE,
					ic->ic_sample_rate, NULL, 0);
			INIT_DELAYED_WORK(&qdrv_radar_cb.muc_sampling.sample_work,
				qdrv_radar_sample_work);
		} else {
			DBGPRINTF_E("failed to start MuC sampling. vap %p sample %p\n",
					vap, qdrv_radar_cb.muc_sampling.sample);
			return;  /* abort further radar initialization if failure */
		}
	}

	if (qdrv_radar_cb.enabled == true) {
		DBGPRINTF_E("re-enabling radar is not supported - reboot\n");
		/* for future work of re-enabling radar
		sys_stop_radarmod();
		sys_start_radarmod(region);
		 */
	} else {
		qdrv_radar_enable_action();
	}
}

static bool qdrv_radar_is_dfs_chan(uint8_t wifi_chan)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	uint32_t chan_idx;

	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		if (ic->ic_channels[chan_idx].ic_ieee == wifi_chan) {
			if (ic->ic_channels[chan_idx].ic_flags & IEEE80211_CHAN_DFS) {
				return true;
			}
		}
	}
	return false;
}

static bool qdrv_radar_is_dfs_weather_chan(uint8_t wifi_chan)
{
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	uint32_t chan_idx;

	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		if (ic->ic_channels[chan_idx].ic_ieee == wifi_chan) {
			if (ic->ic_channels[chan_idx].ic_flags & IEEE80211_CHAN_WEATHER) {
				return true;
			}
		}
	}
	return false;
}

struct ieee80211_channel * qdrv_radar_get_current_cac_chan(void)
{
	return  qdrv_radar_cb.cac_chan;
}
EXPORT_SYMBOL(qdrv_radar_get_current_cac_chan);

bool qdrv_dfs_is_eu_region()
{
	return qdrv_radar_cb.region == DFS_RQMT_EU;
}

int radar_pm_notify(struct notifier_block *b, unsigned long level, void *v)
{
	static int pm_prev_level = BOARD_PM_LEVEL_NO;
	const int switch_level = BOARD_PM_LEVEL_DUTY;
	struct ieee80211com *ic = qdrv_radar_cb.ic;
	struct ieee80211_channel *operate_chan;
	bool rdetect;

	if (!qdrv_radar_cb.enabled)
		goto out;

	operate_chan = ic->ic_bsschan;
	rdetect = qdrv_radar_is_rdetection_required(operate_chan);

	if (rdetect) {
		if ((pm_prev_level < switch_level) && (level >= switch_level)) {
			sys_disable_rdetection();
		} else if ((pm_prev_level >= switch_level) && (level < switch_level)) {
			radar_set_chan(ic->ic_bsschan->ic_ieee);
			sys_enable_rdetection();
		}
	}

out:
	pm_prev_level = level;
        return NOTIFY_OK;
}

static void qdrv_ocac_irqhandler(void *arg1, void *arg2)
{
	struct qdrv_wlan *qw = arg1;
	struct ieee80211com *ic = &qw->ic;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_ocac_info *ocac_info = sp->ocac_lhost;

	if (ocac_info->chan_status == QTN_OCAC_ON_OFF_CHAN) {
		ic->ic_ocac.ocac_counts.intr_off_chan++;
	} else {
		ic->ic_ocac.ocac_counts.intr_data_chan++;
	}

	tasklet_schedule(&qdrv_radar_cb.ocac_tasklet);
}

static int qdrv_init_ocac_irqhandler(struct qdrv_wlan *qw)
{
	struct int_handler int_handler;

	int_handler.handler = qdrv_ocac_irqhandler;
	int_handler.arg1 = qw;
	int_handler.arg2 = NULL;

	if (qdrv_mac_set_handler(qw->mac, RUBY_M2L_IRQ_LO_OCAC, &int_handler) != 0) {
		DBGPRINTF_E("Could not set ocac irq handler\n");
		return -1;
	}

	return 0;
}

/*
 * initialize qdrv_radar.
 * - Has to be invoked inside or after qdrv_wlan_init()
 */
int qdrv_radar_init(struct qdrv_mac *mac)
{
	struct ieee80211com *ic = &(((struct qdrv_wlan*)mac->data)->ic);
	struct qdrv_wlan *qw = container_of(ic, struct qdrv_wlan, ic);
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_ocac_info *ocac_info = sp->ocac_lhost;
	unsigned chan_idx;
	struct timer_list *cac_timer;
	struct timer_list *dfs_cs_timer;

	if (mac->unit != 0) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
			"init radar request for mac%d ignored\n", mac->unit);
		return 0; /* yes, it is success by design */
	}

	if (qdrv_radar_configured) {
		DBGPRINTF_E("radar already configured\n");
		return -1;
	}

	/* clear the control block */
	memset(&qdrv_radar_cb, 0, sizeof(qdrv_radar_cb));

	qdrv_radar_cb.mac = mac;
	qdrv_radar_cb.ic = ic;

	/* initialize the cac_timer */
	cac_timer = &qdrv_radar_cb.cac_timer;
	init_timer(cac_timer);
	cac_timer->function = cac_completed_action;
	cac_timer->data = (unsigned long) NULL; /* not used */

	/* initialize all nonoccupy timers */
	ic->ic_non_occupancy_period = QDRV_RADAR_DFLT_NONOCCUPY_PERIOD * HZ;
	for (chan_idx = 0; chan_idx < ic->ic_nchans; chan_idx++) {
		struct timer_list *nonoccupy_timer = &qdrv_radar_cb.nonoccupy_timer[chan_idx];

		init_timer(nonoccupy_timer);
		nonoccupy_timer->function = nonoccupy_expire_action;
		nonoccupy_timer->data = chan_idx;
	}

	/* initialize the dfs_cs_timer */
	dfs_cs_timer = &qdrv_radar_cb.dfs_cs_timer;
	dfs_cs_timer->function = dfs_cs_timer_expire_action;
	init_timer(dfs_cs_timer);

	ic->sta_dfs_info.sta_radar_timer.function = sta_radar_detected_timer_action;
	init_timer(&ic->sta_dfs_info.sta_radar_timer);

	qdrv_radar_cb.pm_notifier.notifier_call = radar_pm_notify;
	pm_qos_add_notifier(PM_QOS_POWER_SAVE, &qdrv_radar_cb.pm_notifier);

	/* For off channel CAC */
	tasklet_init(&qdrv_radar_cb.ocac_tasklet, &qdrv_ocac_tasklet, (unsigned long)ocac_info);
	qdrv_init_ocac_irqhandler(qw);

	qdrv_radar_configured = true;

	/* success */
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "radar initialized\n");

	return 0;
}

/*
 * deinitialize qdrv_radar
 */
int qdrv_radar_exit(struct qdrv_mac *mac)
{
	if (mac->unit != 0) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
			"exit request for mac%d ignored\n", mac->unit);
		return 0; /* yes, it is success by design */
	}

	if (qdrv_radar_first_call == true || !qdrv_radar_configured) {
		DBGPRINTF_E("radar already unconfigured\n");
		return -1;
	}

	qdrv_radar_disable();

	tasklet_kill(&qdrv_radar_cb.ocac_tasklet);
	pm_qos_remove_notifier(PM_QOS_POWER_SAVE, &qdrv_radar_cb.pm_notifier);

	/* disable radar detection */
	sys_stop_radarmod();

	/* clear the control block */
	memset(&qdrv_radar_cb, 0, sizeof(qdrv_radar_cb));

	qdrv_radar_configured = false;
	qdrv_radar_sta_dfs = false;

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "radar exited\n");

	return 0;
}

int qdrv_radar_unload(struct qdrv_mac *mac)
{
	if (mac->unit != 0) {
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR,
			"exit request for mac%d ignored\n", mac->unit);
		return 0; /* yes, it is success by design */
	}

	if (qdrv_radar_first_call == true || !qdrv_radar_configured) {
		DBGPRINTF_E("radar already unconfigured\n");
		return -1;
	}

	qdrv_radar_disable();

	/* success */
	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_RADAR, "radar unloaded\n");

	return 0;
}

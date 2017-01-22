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
#ifndef _QDRV_MUC_STATS_H
#define _QDRV_MUC_STATS_H

#include <net80211/ieee80211_var.h>
#include "qdrv_mac.h"

/* enum disconnects the actual display for a particular line of phy stats from the phy stats mode */
enum qdrv_muc_stats_display_choice {
	QDRV_MUC_STATS_SHOW_RSSI = 0,
	QDRV_MUC_STATS_SHOW_RCPI,
	QDRV_MUC_STATS_SHOW_EVM,
};

enum qdrv_muc_stats_display_choice qdrv_muc_stats_get_display_choice(const struct qtn_stats *stats,
								     const struct ieee80211com *ic);
struct qtn_stats *qtn_muc_stats_get_addr_latest_stats(struct qdrv_mac *mac,
						      const struct ieee80211com *ic,
						      int required_phy_stat_mode);
int qdrv_muc_stats_printlog(const struct qdrv_cb *data,
			    struct qdrv_mac *mac,
			    struct ieee80211com *ic,
			    int argc,
			    char **argv);
int qdrv_muc_get_noise(struct qdrv_mac *mac, const struct ieee80211com *ic);
int qdrv_muc_get_rssi_by_chain(struct qdrv_mac *mac,
			       const struct ieee80211com *ic,
			       unsigned int rf_chain);
u_int32_t qdrv_muc_get_rx_gain_fields(struct qdrv_mac *mac, const struct ieee80211com *ic);
int qdrv_muc_get_phy_stat(struct qdrv_mac *mac,
			  const struct ieee80211com *ic,
			  const char *name_of_stat,
			  const unsigned int array_index,
			  int *stat_value);
int qdrv_muc_stats_rxtx_phy_rate(const struct ieee80211_node *, const int is_rx,
		uint8_t *nss, uint8_t *mcs, uint32_t * phy_rate);
int qdrv_muc_stats_rssi(const struct ieee80211_node *);
int qdrv_muc_stats_smoothed_rssi(const struct ieee80211_node *ni);
int qdrv_muc_stats_snr(const struct ieee80211_node *);
int qdrv_muc_stats_max_queue(const struct ieee80211_node *);
u_int32_t qdrv_muc_stats_mcs_to_phyrate(u_int8_t bw, u_int8_t sgi, u_int8_t mcs,
		uint8_t nss, uint8_t vht);
ssize_t qdrv_muc_get_size_rssi_phy_stats(void);
u_int32_t qdrv_muc_stats_tx_failed(const struct ieee80211_node *);
int qdrv_muc_get_last_phy_stats(struct qdrv_mac *mac, struct ieee80211com *ic,
				struct ieee80211_phy_stats *ps, uint8_t all_stats);
int qdrv_muc_stats_hw_noise(const struct ieee80211_node *ni);

u_int32_t qdrv_muc_stats_tx_airtime(const struct ieee80211_node *);
u_int32_t qdrv_muc_stats_tx_accum_airtime(const struct ieee80211_node *);
u_int32_t qdrv_muc_stats_rx_airtime(const struct ieee80211_node *);
u_int32_t qdrv_muc_stats_rx_accum_airtime(const struct ieee80211_node *);
#endif

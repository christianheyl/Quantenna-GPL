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

#ifndef _QDRV_RADAR_H_
#define _QDRV_RADAR_H_

#define QDRV_RADAR_DFLT_CHANSW_MS 50 /* msecs */
#define QDRV_RADAR_DFLT_NONOCCUPY_PERIOD	1800 /* secs */


int qdrv_radar_init(struct qdrv_mac* mac);
int qdrv_radar_exit(struct qdrv_mac* mac);
int qdrv_radar_unload(struct qdrv_mac *mac);

void qdrv_radar_enable(const char* region);
void qdrv_radar_disable(void);
void qdrv_sta_set_xmit(int enable);
void qdrv_set_radar(int enable);

void qdrv_radar_detected(struct ieee80211com* ic, u_int8_t new_ieee);
int qdrv_radar_can_sample_chan(void);
int qdrv_radar_test_mode_enabled(void);
void qdrv_radar_before_newchan(void);
void qdrv_radar_on_newchan(void);
void qdrv_radar_stop_active_cac(void);
void sta_dfs_cac_action(struct ieee80211_channel *chan);
int qdrv_radar_detections_num(uint32_t chan);

bool qdrv_radar_is_rdetection_required(const struct ieee80211_channel *chan);
bool qdrv_dfs_is_eu_region(void);

void qdrv_dfs_action_scan_done(void);

struct ieee80211_channel * qdrv_radar_get_current_cac_chan(void);
void qdrv_radar_enable_radar_detection(void);

#endif

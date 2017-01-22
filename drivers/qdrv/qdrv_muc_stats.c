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
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_comm.h"
#include "qdrv_wlan.h"
#include "qdrv_vap.h"
#include "qdrv_txbf.h"
#include <qtn/qtn_math.h>
#include "qdrv_muc_stats.h"
#include <qtn/txbf_common.h>
#include <qtn/registers.h>
#include <qtn/muc_phy_stats.h>

#define PHY_STATS_SUM_EVM  1
#define TIME_AVERAGE_PHY_STATS   1
#define PHY_STATS_PHY_DATE_RATE  1

/* valid only when Y is a power of 2, redefine more generally */
#define MOD(X,Y) ((X)&(Y-1))

static struct qtn_stats_log host_log;
static unsigned int last_tstamp = 0;
#ifdef TIME_AVERAGE_PHY_STATS

#define QDRV_MUC_STATS_RATE_TABLE_TO_MBPS	10

#endif

#ifdef PHY_STATS_PHY_DATE_RATE
#define UNSUPPORTED_RATE	0
#define MAX_BW_SIZE 2
#define MAX_GI_SIZE 2
#define MAX_MCS_SIZE 77 // index : 0~76

enum muc_stats_opt {
	MSO_DEF		= 0,
	MSO_BOTH	= 1,
	MSO_SU		= 2,
	MSO_MU		= 3,
};

const uint16_t mcs_rate_table[MAX_BW_SIZE * MAX_GI_SIZE * MAX_MCS_SIZE] = {

  //=========20Mhz with long GI from MCS 0 ~ 15===============
   65,  130,  195,  260,  390,  520,  585,  650,
  130,  260,  390,  520,  780, 1040, 1170, 1300,

  //20Mhz with long GI from MCS 16 ~ 31
  195,  390,  585,  780, 1170, 1560, 1755, 1950,
  260,  520,  780, 1040, 1560, 2080, 2340, 2600,

  //20Mhz with long GI from MCS 32 ~ 47
    0,  390,  520,  650,  585,  780,  975,  520,
  650,  650,  780,  910,  910, 1040,  780,  975,

  //20Mhz with long GI from MCS 48 ~ 63
  975,  1170,  1365,  1365,  1560,  650,  780,  910,
  780,   910,  1040,  1170,  1040, 1170, 1300, 1300,

  //20Mhz with long GI from MCS 64 ~ 76
  1430,  975,  1170,  1365,  1170, 1365, 1560, 1755,  1560,  1755,  1950,  1950,  2145,


  //==========20Mhz with short GI from MCS 0 ~ 15==============
   72,  144,  217,   289,  433,   578,   650,   722,
  144,  289,  433,   578,  867,  1156,  1300,  1444,

  //20Mhz with short GI from MCS 16 ~ 31
  217,  433,  650,   867,  1300,  1733,  1950,  2167,
  289,  578,  867,  1156,  1733,  2311,  2600,  2889,

  //20Mhz with short GI from MCS 32 ~ 47
    0,  433,  578,   722,   650,   867,  1083,   578,
  722,  722,  867,  1011,  1011,  1156,   867,  1083,

  //20Mhz with short GI from MCS 48 ~ 63
  1083,  1300,  1517,  1517,  1733,  722,  867,  1011,
   867,  1011,  1156,  1300,  1156, 1300, 1444,  1444,

  //20Mhz with short GI from MCS 64 ~ 76
  1589,  1083,  1300,  1517,  1300, 1517, 1733, 1950,  1733,  1950,  2167,  2167,  2383,


  //======40Mhz with long GI from MCS 0 ~ 15==================
  135,  270,   405,   540,   810,  1080,  1215,  1350,
  270,  540,   810,  1080,  1620,  2160,  2430,  2700,

  //40Mhz with long GI from MCS 16 ~ 31
  405,  810,  1215,  1620,  2430,  3240,  3645,  4050,
  540, 1080,  1620,  2160,  3240,  4320,  4860,  5400,

  //40Mhz with long GI from MCS 32 ~ 47
    60,  810,  1080,  1350, 1215,  1620,  2025,  1080,
  1350, 1350,  1620,  1890, 1890,  2160,  1620,  2025,

  //40Mhz with long GI from MCS 48 ~ 63
  2025,  2430,  2835,  2835,  3240,  1350,  1620,  1890,
  1620,  1890,  2160,  2430,  2160,  2430,  2700,  2700,

  //40Mhz with long GI from MCS 64 ~ 76
  2970,  2025,  2430,  2835,  2430, 2835, 3240, 3645,  3240,  3645,  4050,  4050,  4455,


  //========40Mhz with short GI from MCS 0 ~ 15================
  150,  300,   450,   600,   900,  1200,  1350,  1500,
  300,  600,   900,  1200,  1800,  2400,  2700,  3000,

  //40Mhz with short GI from MCS 16 ~ 31
  450,  900,  1350,  1800,  2700,  3600,  4050,  4500,
  600, 1200,  1800,  2400,  3600,  4800,  5400,  6000,

  //40Mhz with short GI from MCS 32 ~ 47
    67,  900,  1200,  1500,  1350,  1800,  2250,  1200,
  1500, 1500,  1800,  2100,  2100,  2400,  1800,  2250,

  //40Mhz with short GI from MCS 48 ~ 63
  2250,  2700,  3150,  3150,  3600,  1500,  1800,  2100,
  1800,  2100,  2400,  2700,  2400,  2700,  3000,  3000,

  //40Mhz with short GI from MCS 64 ~ 76
  3300,  2250,  2700,  3150,  2700, 3150, 3600, 4050,  3600,  4050,  4500,  4500,  4950
};

#ifdef QDRV_FEATURE_VHT
#define VHT_MAX_BW_SIZE	4
#define VHT_MAX_GI_SIZE	2
#define VHT_MAX_NSS_SIZE	4
#define VHT_MAX_MCS_SIZE	10
/* rate in uint 100kbps */
const uint16_t vht_rate_table[VHT_MAX_BW_SIZE * VHT_MAX_GI_SIZE *
                              VHT_MAX_NSS_SIZE * VHT_MAX_MCS_SIZE] = {
	/* 20MHz, Long GI, Nss = 1, MCS 0 ~ 9 */
	65, 130, 195, 260, 390, 520, 585, 650, 780, UNSUPPORTED_RATE,
	/* 20MHz, Long GI, Nss = 2, MCS 0 ~ 9 */
	130, 260, 390, 520, 780, 1040, 1170, 1300, 1560, UNSUPPORTED_RATE,
	/* 20MHz, Long GI, Nss = 3, MCS 0 ~ 9 */
	195, 390, 585, 780, 1170, 1560, 1755, 1950, 2340, 2600,
	/* 20MHz, Long GI, Nss = 4, MCS 0 ~ 9 */
	260, 520, 780, 1040, 1560, 2080, 2340, 2600, 3120, UNSUPPORTED_RATE,

	/* 20MHz, Short GI, Nss = 1, MCS 0 ~ 9 */
	72, 144, 217, 289, 433, 578, 650, 722, 867, UNSUPPORTED_RATE,
	/* 20MHz, Short GI, Nss = 2, MCS 0 ~ 9 */
	144, 289, 433, 578, 867, 1156, 1300, 1444, 1733, UNSUPPORTED_RATE,
	/* 20MHz, Short GI, Nss = 3, MCS 0 ~ 9 */
	217, 433, 650, 867, 1300, 1733, 1950, 2167, 2600, 2889,
	/* 20MHz, Short GI, Nss = 4, MCS 0 ~ 9 */
	289, 578, 867, 1156, 1733, 2311, 2600, 2889, 3467, UNSUPPORTED_RATE,

	/* 40MHz, Long GI, Nss = 1, MCS 0 ~ 9 */
	135, 270, 405, 540, 810, 1080, 1215, 1350, 1620, 1800,
	/* 40MHz, Long GI, Nss = 2, MCS 0 ~ 9 */
	270, 540, 810, 1080, 1620, 2160, 2430, 2700, 3240, 3600,
	/* 40MHz, Long GI, Nss = 3, MCS 0 ~ 9 */
	405, 810, 1215, 1620, 2430, 3240, 3645, 4050, 4860, 5400,
	/* 40MHz, Long GI, Nss = 4, MCS 0 ~ 9 */
	540, 1080, 1620, 2160, 3240, 4320, 4860, 5400, 6480, 7200,

	/* 40MHz, Short GI, Nss = 1, MCS 0 ~ 9 */
	150, 300, 450, 600, 900, 1200, 1350, 1500, 1800, 2000,
	/* 40MHz, Short GI, Nss = 2, MCS 0 ~ 9 */
	300, 600, 900, 1200, 1800, 2400, 2700, 3000, 3600, 4000,
	/* 40MHz, Short GI, Nss = 3, MCS 0 ~ 9 */
	450, 900, 1350, 1800, 2700, 3600, 4050, 4500, 5400, 6000,
	/* 40MHz, Short GI, Nss = 4, MCS 0 ~ 9 */
	600, 1200, 1800, 2400, 3600, 4800, 5400, 6000, 7200, 8000,

	/* 80MHz, Long GI, Nss = 1, MCS 0 ~ 9 */
	293, 585, 878, 1170, 1755, 2340, 2633, 2925, 3510, 3900,
	/* 80MHz, Long GI, Nss = 2, MCS 0 ~ 9 */
	585, 1170, 1755, 2340, 3510, 4680, 5265, 5850, 7020, 7800,
	/* 80MHz, Long GI, Nss = 3, MCS 0 ~ 9 */
	878, 1755, 2633, 3510, 5265, 7020, UNSUPPORTED_RATE, 8775, 10530, 11700,
	/* 80MHz, Long GI, Nss = 4, MCS 0 ~ 9 */
	1170, 2340, 3510, 4680, 7020, 9360, 10530, 11700, 14040, 15600,

	/* 80MHz, Short GI, Nss = 1, MCS 0 ~ 9 */
	325, 650, 975, 1300, 1950, 2600, 2925, 3250, 3900, 4333,
	/* 80MHz, Short GI, Nss = 2, MCS 0 ~ 9 */
	650, 1300, 1950, 2600, 3900, 5200, 5850, 6500, 7800, 8667,
	/* 80MHz, Short GI, Nss = 3, MCS 0 ~ 9 */
	975, 1950, 2925, 3900, 5850, 7800, UNSUPPORTED_RATE, 9750, 11700, 13000,
	/* 80MHz, Short GI, Nss = 4, MCS 0 ~ 9 */
	1300, 2600, 3900, 5200, 7800, 10400, 11700, 13000, 15600, 17333,

	/* 160MHz, Long GI, Nss = 1, MCS 0 ~ 9 */
	585, 1170, 1755, 2340, 3510, 4680, 5265, 5850, 7020, 7800,
	/* 160MHz, Long GI, Nss = 2, MCS 0 ~ 9 */
	1170, 2340, 3510, 4680, 7020, 9360, 10530, 11700, 14040, 15600,
	/* 160MHz, Long GI, Nss = 3, MCS 0 ~ 9 */
	1755, 3510, 5265, 7020, 10530, 14040, 15795, 17550, 21060, UNSUPPORTED_RATE,
	/* 160MHz, Long GI, Nss = 4, MCS 0 ~ 9 */
	2340, 4680, 7020, 9360, 14040, 18720, 21060, 23400, 28080, 31200,

	/* 160MHz, Short GI, Nss = 1, MCS 0 ~ 9 */
	650, 1300, 1950, 2600, 3900, 5200, 5850, 6500, 7800, 8667,
	/* 160MHz, Short GI, Nss = 2, MCS 0 ~ 9 */
	1300, 2600, 3900, 5200, 7800, 10400, 11700, 13000, 15600, 17333,
	/* 160MHz, Short GI, Nss = 3, MCS 0 ~ 9 */
	1950, 3900, 5850, 7800, 11700, 15600, 17550, 19500, 23400, UNSUPPORTED_RATE,
	/* 160MHz, Short GI, Nss = 4, MCS 0 ~ 9 */
	2600, 5200, 7800, 10400, 15600, 20800, 23400, 26000, 31200, 34667
};
#endif

#endif


#define  QDRV_MUC_STATS_DB_VALUE_LENGTH		8
#define  QDRV_MUC_STATS_DAGC_SHIFT_UP_M		0x00380000
#define  QDRV_MUC_STATS_DAGC_SHIFT_UP_S		19
#define  QDRV_MUC_STATS_DAGC_SHIFT_DOWN_M	0x00070000
#define  QDRV_MUC_STATS_DAGC_SHIFT_DOWN_S	16

#define  QDRV_MUC_STATS_DAGC_SHIFT_FIELD_M	0x00ff0000
#define  QDRV_MUC_STATS_DAGC_SHIFT_FIELD_S	16

#define  MIN_RCPI_VALUE				-1000

/* Tag to filter(grep) statistics in csv format */
#define STAT_CSV_TAG	"*CSV_STAT*"

static uint32_t qdrv_muc_stats_rate_lut(uint8_t bw, uint8_t sgi, uint8_t mcs)
{
	int index;
	uint32_t rate = 0;

	if (bw < MAX_BW_SIZE && sgi < MAX_GI_SIZE && mcs < MAX_MCS_SIZE) {
		index = mcs + (sgi * MAX_MCS_SIZE) + (bw * MAX_GI_SIZE * MAX_MCS_SIZE);
		rate = (uint32_t)mcs_rate_table[index];
	}

	return rate;
}

static uint32_t qdrv_muc_stats_vhtrate_lut(uint8_t bw, uint8_t sgi, uint8_t mcs, uint8_t nss)
{
#ifdef QDRV_FEATURE_VHT
	int index;
	uint32_t rate = UNSUPPORTED_RATE;

	if (bw < VHT_MAX_BW_SIZE && sgi < VHT_MAX_GI_SIZE && nss < VHT_MAX_NSS_SIZE
			&& mcs < VHT_MAX_MCS_SIZE) {
		index = mcs + (nss * VHT_MAX_MCS_SIZE) + (sgi * VHT_MAX_NSS_SIZE * VHT_MAX_MCS_SIZE) +
				(bw * VHT_MAX_GI_SIZE * VHT_MAX_NSS_SIZE * VHT_MAX_MCS_SIZE);
		rate = (uint32_t)vht_rate_table[index];
	}

	return rate;
#else
	return UNSUPPORTED_RATE;
#endif
}

/* The returned rate has unit kbps */
uint32_t qdrv_muc_stats_mcs_to_phyrate(uint8_t bw, uint8_t sgi, uint8_t mcs,
			uint8_t nss, uint8_t vht)
{
	if (vht)
		return qdrv_muc_stats_vhtrate_lut(bw, sgi, mcs, nss) * 100;
	else
		return qdrv_muc_stats_rate_lut(bw, sgi, mcs) * 100;
}

enum qdrv_muc_stats_display_choice qdrv_muc_stats_get_display_choice(const struct qtn_stats *stats,
		const struct ieee80211com *ic)
{
	enum qdrv_muc_stats_display_choice display_choice = QDRV_MUC_STATS_SHOW_RSSI;

	if (ic->ic_mode_get_phy_stats == MUC_PHY_STATS_RSSI_RCPI_ONLY) {
		display_choice = (stats->tstamp & 0x01) ? QDRV_MUC_STATS_SHOW_RSSI : QDRV_MUC_STATS_SHOW_RCPI;
	} else {
		if (qtn_select_rssi_over_error_sums(stats->tstamp, ic->ic_mode_get_phy_stats)) {
			display_choice = QDRV_MUC_STATS_SHOW_RSSI;
		} else {
			display_choice = QDRV_MUC_STATS_SHOW_EVM;
		}
	}

	return(display_choice);
}

static int qtn_muc_stats_get_dagc_shift_count(u_int32_t rx_gain_fields)
{
	int dagc_shift_up =
			(rx_gain_fields & QDRV_MUC_STATS_DAGC_SHIFT_UP_M) >> QDRV_MUC_STATS_DAGC_SHIFT_UP_S;
	int dagc_shift_down =
			(rx_gain_fields & QDRV_MUC_STATS_DAGC_SHIFT_DOWN_M) >> QDRV_MUC_STATS_DAGC_SHIFT_DOWN_S;

	return(dagc_shift_up - dagc_shift_down);
}

/*
 * Display RSSI/EVM stats of the node with the most packets
 */
static struct ieee80211_node *qtn_muc_stats_get_node(struct ieee80211com *ic, int mu)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	struct ieee80211_node *found = NULL;
	unsigned long max_pkts = 0;

	IEEE80211_NODE_LOCK_IRQ(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		unsigned long pkts = ni->ni_shared_stats->rx[mu].pkts + ni->ni_shared_stats->tx[mu].pkts;
		if (pkts > max_pkts || found == NULL) {
			found = ni;
			max_pkts = pkts;
		}
	}

	if (found)
		ieee80211_ref_node(found);

	IEEE80211_NODE_UNLOCK_IRQ(nt);

	return found;
}

static void qtn_muc_stats_fmt_hw_noise(char *buf, const struct qtn_rx_stats *rxstats)
{
	int noise_dbm;
	int int_val;
	int fract_val;

	if (rxstats->hw_noise != MIN_RCPI_VALUE) {
		noise_dbm = rxstats->hw_noise;
		int_val =  noise_dbm / 10;
		fract_val = ABS(noise_dbm) % 10;

		sprintf(buf, "%4d.%d", int_val, fract_val);
	} else {
		strcpy(buf, "  -inf");
	}
}

static const char g_header_line[]       = KERN_DEBUG "Tstamp"" SU/MU"" RxMPDU"" AMSDU"" RxG""   CRC""  Noise"" TxFrame"" Defers"" Touts"" Retries"" ShPmbl""  LgPmbl"" Scale" " " " MCS-(TX/RX)""  RSSI / RCPI / EVM\n";
static const char g_stat_comm_fmt[]     = KERN_DEBUG    "%6d"   "%6s"   "%7d"   "%6d" "%4d"   "%6d"    "%7s"   "%8d"    "%7d"   "%6d"     "%8d"    "%7d"     "%8d"   "%6d"    " ";
static const char g_stat_comm_csv_fmt[] =               "%d,"   "%s,"   "%d,"   "%d," "%d,"   "%d,"    "%s,"   "%d,"    "%d,"   "%d,"     "%d,"    "%d,"     "%d,"   "%d,";

static void qtn_muc_stats_display_common(const struct qtn_stats *stats,
		int mu, int csv_format)
{
	const struct qtn_rx_stats *rxstats[] = {&stats->rx_phy_stats, &stats->mu_rx_phy_stats};
	const struct qtn_tx_stats *txstats[] = {&stats->tx_phy_stats, &stats->mu_tx_phy_stats};

	char noise_strs[QDRV_MUC_STATS_DB_VALUE_LENGTH];

	qtn_muc_stats_fmt_hw_noise(noise_strs, rxstats[mu]);

	printk(csv_format ? g_stat_comm_csv_fmt : g_stat_comm_fmt,
		stats->tstamp,
		(mu) ? "MU" : "SU",
		rxstats[mu]->num_pkts,
		rxstats[mu]->num_amsdu,
		rxstats[mu]->avg_rxgain,
		rxstats[mu]->cnt_mac_crc,
		noise_strs,
		txstats[mu]->num_pkts,
		txstats[mu]->num_defers,
		txstats[mu]->num_timeouts,
		txstats[mu]->num_retries,
		rxstats[mu]->cnt_sp_fail,
		rxstats[mu]->cnt_lp_fail,
		txstats[mu]->last_tx_scale);
}

static void qtn_muc_stats_display_rssi_only(struct ieee80211_node *ni, int mu, int csv_format)
{
	int iter;
	char db_strs[NUM_ANT + 1][QDRV_MUC_STATS_DB_VALUE_LENGTH];
	const struct qtn_node_shared_stats_rx *node_rxstats = &ni->ni_shared_stats->rx[mu];
	const struct qtn_node_shared_stats_tx *node_txstats= &ni->ni_shared_stats->tx[mu];

	/* RSSIs in dBM (units are actually 0.1 dBM) ... */
	for (iter = 0; iter <= NUM_ANT; iter++) {
		int rssi_dbm = node_rxstats->last_rssi_dbm[iter];
		if (rssi_dbm) {
			int int_val = rssi_dbm / 10;
			int fract_val = ABS(rssi_dbm) % 10;
			snprintf(&db_strs[iter][0],
					sizeof( db_strs[iter] ),
					"%d.%d",
					int_val,
					fract_val);
		} else {
			strcpy(&db_strs[iter][0], "-inf");
		}
	}

#ifdef PHY_STATS_PHY_DATE_RATE
#define STAT_RSSI_FMT "%4dM %4dM     %4s %4s %4s %4s dBm %4s avg RSSI\n"
#define STAT_RSSI_CSV_FMT "%d,%d,%s,%s,%s,%s,%s,"
	printk(csv_format ? STAT_RSSI_CSV_FMT : STAT_RSSI_FMT,
		MS(node_txstats->last_mcs, QTN_PHY_STATS_MCS_PHYRATE),
		MS(node_rxstats->last_mcs, QTN_PHY_STATS_MCS_PHYRATE),
		db_strs[0],
		db_strs[1],
		db_strs[2],
		db_strs[3],
		db_strs[4]);
#undef STAT_RSSI_FMT
#undef STAT_RSSI_CSV_FMT
#else
	unsigned int tx_mcs;
	unsigned int rx_mcs;
	rx_mcs = (node_rxstats->last_mcs & QTN_STATS_MCS_RATE_MASK) +
		MS(node_rxstats->last_mcs, QTN_PHY_STATS_MCS_NSS) * 100;
	tx_mcs = (node_txstats->last_mcs & QTN_STATS_MCS_RATE_MASK) +
		MS(node_txstats->last_mcs, QTN_PHY_STATS_MCS_NSS) * 100;

	printk("%5d %5d     %4s %4s %4s %4s dBm %4s avg RSSI\n",
		tx_mcs,
		rx_mcs,
		db_strs[0],
		db_strs[1],
		db_strs[2],
		db_strs[3],
		db_strs[4]);

#endif
}

static void qtn_muc_stats_display_rcpi_only(struct ieee80211_node *ni, int mu)
{
	int i;
	char db_strs[NUM_ANT + 1][QDRV_MUC_STATS_DB_VALUE_LENGTH];

	const struct qtn_node_shared_stats_rx *node_rxstats = &ni->ni_shared_stats->rx[mu];
	const struct qtn_node_shared_stats_tx *node_txstats= &ni->ni_shared_stats->tx[mu];
	unsigned int tx_mcs;
	unsigned int rx_mcs;

	/* RSSIs in dBM (units are actually 0.1 dBM) ... */
	for (i = 0; i <= NUM_ANT; i++) {
		int rcpi_dbm = node_rxstats->last_rcpi_dbm[i];
		if (rcpi_dbm) {
			int int_val = rcpi_dbm / 10;
			int fract_val = ABS(rcpi_dbm) % 10;

			snprintf(db_strs[i],
					sizeof(db_strs[i]),
					"%d.%d",
					int_val,
					fract_val);
		} else {
			strcpy(db_strs[i], "-inf");
		}
	}

	rx_mcs = (node_rxstats->last_mcs & QTN_STATS_MCS_RATE_MASK) +
		MS(node_rxstats->last_mcs, QTN_PHY_STATS_MCS_NSS) * 100;
	tx_mcs = (node_txstats->last_mcs & QTN_STATS_MCS_RATE_MASK) +
		MS(node_txstats->last_mcs, QTN_PHY_STATS_MCS_NSS) * 100;

	printk("%5d %5d     %4s %4s %4s %4s dBm %4s max RCPI\n",
		tx_mcs,
		rx_mcs,
		db_strs[0],
		db_strs[1],
		db_strs[2],
		db_strs[3],
		db_strs[4]);
}

static void qtn_muc_stats_display_evm_only(struct ieee80211_node *ni, int mu, int csv_format)
{
	char evm_strs[NUM_ANT+1][QDRV_MUC_STATS_DB_VALUE_LENGTH];
	int i;
	int evm_int;
	int evm_fract;
	unsigned int tx_mcs;
	unsigned int rx_mcs;

	const struct qtn_node_shared_stats_rx *node_rxstats = &ni->ni_shared_stats->rx[mu];
	const struct qtn_node_shared_stats_tx *node_txstats = &ni->ni_shared_stats->tx[mu];

	for (i = 0; i <= NUM_ANT; i++) {
		int v;

		v = node_rxstats->last_evm_dbm[i];

		snprintf(evm_strs[i],
				sizeof(evm_strs[i]),
				"%d.%d",
				v / 10,
				ABS(v) % 10);
	}

	evm_int = node_rxstats->last_evm_dbm[NUM_ANT] / 10;
	evm_fract = ABS(node_rxstats->last_evm_dbm[NUM_ANT]) % 10;

#ifdef  PHY_STATS_SUM_EVM // doing the dB summation
	int evm_sum=0;

	for (i = 0; i < NUM_ANT; i++) {
		evm_sum += node_rxstats->last_evm_dbm[i];
	}

	evm_int = (int) evm_sum / 10;
        evm_fract = (int) (ABS(evm_sum) % 10);
#endif

	snprintf(&evm_strs[NUM_ANT][0],
		  sizeof(evm_strs[NUM_ANT]),
		  "%d.%d",
		  evm_int,
		  evm_fract);

	rx_mcs = (node_rxstats->last_mcs & QTN_STATS_MCS_RATE_MASK) +
		MS(node_rxstats->last_mcs, QTN_PHY_STATS_MCS_NSS) * 100;
	tx_mcs = (node_txstats->last_mcs & QTN_STATS_MCS_RATE_MASK) +
		MS(node_txstats->last_mcs, QTN_PHY_STATS_MCS_NSS) * 100;

#define STAT_EVM_FMT "%5d %5d %3d %4s %4s %4s %4s dB  %4s avg EVM\n"
#define STAT_EVM_CSV_FMT "%d,%d,%d,%s,%s,%s,%s,%s,"
	printk(csv_format ? STAT_EVM_CSV_FMT : STAT_EVM_FMT,
		tx_mcs,
		rx_mcs,
		node_rxstats->last_rxsym,
		&evm_strs[0][0],
		&evm_strs[1][0],
		&evm_strs[2][0],
		&evm_strs[3][0],
		&evm_strs[4][0]);
#undef STAT_EVM_FMT
#undef STAT_EVM_CSV_FMT
}

static void display_log_node_info(struct ieee80211_node *ni,
		enum qdrv_muc_stats_display_choice display_choice, int mu, int csv_format)
{
	switch (display_choice) {
	case QDRV_MUC_STATS_SHOW_RCPI:
		qtn_muc_stats_display_rcpi_only(ni, mu);
		break;
	case QDRV_MUC_STATS_SHOW_EVM:
		qtn_muc_stats_display_evm_only(ni, mu, csv_format);
		break;
	case QDRV_MUC_STATS_SHOW_RSSI:
	default:
		qtn_muc_stats_display_rssi_only(ni, mu, csv_format);
		break;
	}
}

static void display_log_info(const struct qtn_stats *stats, struct ieee80211com *ic, enum muc_stats_opt opt,
				int show_all_nodes, int csv_format)
{
	struct ieee80211_node *main_ni;
	enum qdrv_muc_stats_display_choice display_choice;

	static int start[] = {STATS_MIN, STATS_MIN, STATS_SU, STATS_MU };
	static int stop[] =  {STATS_MAX, STATS_MAX, STATS_MU, STATS_MAX};
	int mu;
	int ret = 1;

	display_choice = qdrv_muc_stats_get_display_choice(stats, ic);

	for (mu = start[opt]; mu < stop[opt]; mu++) {
		main_ni = qtn_muc_stats_get_node(ic, mu);

		if (unlikely(main_ni == NULL)) {
			continue;
		}
		ret = 0;
		qtn_muc_stats_display_common(stats, mu, csv_format);
		if (csv_format) {
			display_log_node_info(main_ni, QDRV_MUC_STATS_SHOW_RSSI, mu, csv_format);
			display_log_node_info(main_ni, QDRV_MUC_STATS_SHOW_EVM, mu, csv_format);
		} else {
			display_log_node_info(main_ni, display_choice, mu, csv_format);
		}
		ieee80211_free_node(main_ni);
	}

	if (ret) goto exit;

	if (show_all_nodes) {
		struct ieee80211_node_table *nt = &ic->ic_sta;
		struct ieee80211_node *ni;

		IEEE80211_NODE_LOCK_IRQ(nt);
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
			for (mu = start[opt]; mu < stop[opt]; mu++) {
#define STAT_ALL_FMT "\t\t%s\tNode " DBGMACVAR "\trx_mpdu %u tx_frame %u\t"
#define STAT_ALL_CSV_FMT "%s," DBGMACVAR ",%u,%u,"
				printk(csv_format ? STAT_ALL_CSV_FMT : KERN_DEBUG STAT_ALL_FMT,
					(mu == STATS_SU) ? "SU" : "MU",
					DBGMACFMT(ni->ni_macaddr),
					ni->ni_shared_stats->rx[mu].pkts,
					ni->ni_shared_stats->tx[mu].pkts);
				if (csv_format) {
					display_log_node_info(ni, QDRV_MUC_STATS_SHOW_RSSI, mu, csv_format);
					display_log_node_info(ni, QDRV_MUC_STATS_SHOW_EVM, mu, csv_format);
				} else {
					display_log_node_info(ni, display_choice, mu, csv_format);
				}
#undef STAT_ALL_FMT
#undef STAT_ALL_CSV_FMT
			}
		}
		IEEE80211_NODE_UNLOCK_IRQ(nt);
	}
exit:
	return;
}

#define COMMON_CSV_HEADER(x) "Tstamp_"#x","#x",RxPkt_"#x",AMSDU_"#x",RxG_"#x",CRC_"#x",Noise_"#x",TxPkt_"#x",Defers_"#x",Touts_"#x",Retries_"#x",ShPmbl_"#x",LgPmbl_"#x",Scale_"#x","

#define RSSI_AND_EVM_CSV_HEADER(x) "PHY_RATE_TX_"#x"(M),PHY_RATE_RX_"#x"(M),"\
	"RSSI0_"#x"(dBm),RSSI1_"#x"(dBm),RSSI2_"#x"(dBm),RSSI3_"#x"(dBm),RSSV_AVR_"#x"(dBm),"\
	"MCS-TX_"#x",MCS-RX_"#x",RxSym_"#x","\
	"EVM0_"#x"(dB),EVM1_"#x"(dB),EVM2_"#x"(dB),EVM3_"#x"(dB),EVM_SUM_"#x"(dB),"

#define FULL_CSV_HEADER(x) COMMON_CSV_HEADER(x)RSSI_AND_EVM_CSV_HEADER(x)

static void display_log_hdr(struct ieee80211com *ic, enum muc_stats_opt opt,
			int show_all_nodes, int csv_format)
{
	if (!csv_format) {
		printk(g_header_line);
	} else {
		/* New line */
		printk(KERN_DEBUG "");
		/* Comma to separate linux time stamp */
		printk(","STAT_CSV_TAG",");

		if (opt == MSO_BOTH || opt == MSO_SU) {
			printk(FULL_CSV_HEADER(SU));
		}
		if (opt == MSO_BOTH || opt == MSO_MU) {
			printk(FULL_CSV_HEADER(MU));
		}
		if (show_all_nodes) {
			struct ieee80211_node_table *nt = &ic->ic_sta;
			struct ieee80211_node *ni;
			int i = 0;

			IEEE80211_NODE_LOCK_IRQ(nt);
			TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
				if (opt == MSO_BOTH || opt == MSO_SU) {
					printk("SU%u,MAC_SU%u,rxpkts_SU%u,txpkts_SU%u,", i, i, i, i);
					printk("PHY_RATE_TX_SU%u(M),PHY_RATE_RX_SU%u(M),RSSI0_SU%u(dBm),RSSI1_SU%u(dBm),RSSI2_SU%u(dBm),RSSI3_SU%u(dBm),"
						"RSSV_AVR_SU%u(dBm),MCS-TX_SU%u,MCS-RX_SU%u,RxSym_SU%u,EVM0_SU%u(dB),EVM1_SU%u(dB),EVM2_SU%u(dB),"
						"EVM3_SU%u(dB),EVM_SUM_SU%u(dB),", i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
				}
				if (opt == MSO_BOTH || opt == MSO_MU) {
					printk("MU%u,MAC_MU%u,rxpkts_MU%u,txpkts_MU%u,", i, i, i, i);
					printk("PHY_RATE_TX_MU%u(M),PHY_RATE_RX_MU%u(M),RSSI0_MU%u(dBm),RSSI1_MU%u(dBm),RSSI2_MU%u(dBm),RSSI3_MU%u(dBm),"
						"RSSV_AVR_MU%u(dBm),MCS-TX_MU%u,MCS-RX_MU%u,RxSym_MU%u,EVM0_MU%u(dB),EVM1_MU%u(dB),EVM2_MU%u(dB),"
						"EVM3_MU%u(dB),EVM_SUM_MU%u(dB),", i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
				}
			i++;
			}
			IEEE80211_NODE_UNLOCK_IRQ(nt);
		}
	}
}

/*
 * Possible values for required_phy_stat_mode are defined in include/qtn/muc_phy_stats.h
 * They define the possible phy stats mode in the ieee80211com.  As the parameter
 * required_phy_stat_mode the meaning of the values is:
 *
 *      MUC_PHY_STATS_ALTERNATE		- any block will do, so return the latest
 *      MUC_PHY_STATS_RSSI_RCPI_ONLY	- block must have RSSIs and RCPI, not errored sums
 *      MUC_PHY_STATS_ERROR_SUM_ONLY	- block must have errored sums, not RSSIs or RCPIs.
 */
static int qtn_muc_stats_does_stat_have_required(struct qtn_stats *curr_log_ptr,
				   int cur_phy_stats_mode,
				   int required_phy_stat_mode)
{
	int retval = 0;

	if (required_phy_stat_mode == MUC_PHY_STATS_ALTERNATE) {
		retval = 1;
	} else {
		int block_has_rssi = qtn_select_rssi_over_error_sums(curr_log_ptr->tstamp,
								     cur_phy_stats_mode);

		if (block_has_rssi != 0) {
			retval = (required_phy_stat_mode == MUC_PHY_STATS_RSSI_RCPI_ONLY);
		} else {
			retval = (required_phy_stat_mode == MUC_PHY_STATS_ERROR_SUM_ONLY);
		}
	}

	return(retval);
}

/*
 * If required_phy_stat_mode is MUC_PHY_STATS_RSSI_RCPI_ONLY or MUC_PHY_STATS_ERROR_SUM_ONLY,
 * none of the blocks may have what is required (based on the current phy stats mode in the
 * ieee80211com).  If so, return NULL.
 */
struct qtn_stats *qtn_muc_stats_get_addr_latest_stats(struct qdrv_mac *mac,
						  const struct ieee80211com *ic,
						  int required_phy_stat_mode)
{
	struct qtn_stats_log *log = NULL;
	struct qtn_stats *curr_log_ptr = NULL;
	struct qtn_stats *retaddr = NULL;
	int cr_indx;
	unsigned int latest_tstamp = 0;
	int cur_phy_stats_mode;

	if (mac == NULL || ic == NULL) {
		return NULL;
	}

	cur_phy_stats_mode = ic->ic_mode_get_phy_stats;

	log = (struct qtn_stats_log *)mac->mac_sys_stats;
	if (log == NULL) {
		return(NULL);
	} else {
		int	required_phy_stats_available = 1;

		if (required_phy_stat_mode == MUC_PHY_STATS_ERROR_SUM_ONLY &&
		    cur_phy_stats_mode == MUC_PHY_STATS_RSSI_RCPI_ONLY) {
			required_phy_stats_available = 0;
		} else if (required_phy_stat_mode == MUC_PHY_STATS_RSSI_RCPI_ONLY &&
			   cur_phy_stats_mode == MUC_PHY_STATS_ERROR_SUM_ONLY) {
			required_phy_stats_available = 0;
		}

		if (required_phy_stats_available == 0) {
			return(NULL);
		}
	}

	cr_indx = MOD(log->curr_buff + 2, NUM_LOG_BUFFS);
	curr_log_ptr = &log->stat_buffs[cr_indx];

	while (cr_indx != log->curr_buff) {
		if (curr_log_ptr->tstamp > latest_tstamp &&
		    qtn_muc_stats_does_stat_have_required(curr_log_ptr,
							  cur_phy_stats_mode,
							  required_phy_stat_mode)) {
			latest_tstamp = curr_log_ptr->tstamp;
			retaddr = curr_log_ptr;
		}

		cr_indx = MOD(cr_indx + 1, NUM_LOG_BUFFS);
		curr_log_ptr = &log->stat_buffs[cr_indx];
	}

	return(retaddr);
}

/*
 * Parse log is subtly different from get address latest stats.
 *
 *     Parse Log reports ALL stats that have not been previously reported;
 *     thus it uses a file-scope variable "last_tstamp" to track which
 *     stats have not been reported.  If called repeatedly between MUC
 *     updates it returns without reporting anything.
 *
 *     Get Address Latest Stats returns the address of the stats with the
 *     latest time stamp.  It can thus be repeatedly called between updates
 *     from the MUC, with each call in this situation returning the address
 *     of the same qtn_stats.
 */

static void parse_log(struct qtn_stats_log *log, struct ieee80211com *ic, enum muc_stats_opt opt,
			int show_all_nodes, int csv_format)
{
	struct qtn_stats *curr_log_ptr;
	int cr_indx;

	cr_indx = MOD(log->curr_buff + 2, NUM_LOG_BUFFS);

	while(cr_indx != log->curr_buff) {
		curr_log_ptr = &log->stat_buffs[cr_indx];
		cr_indx = MOD(cr_indx+1, NUM_LOG_BUFFS);

		if(curr_log_ptr->tstamp <= last_tstamp) continue;

		if (csv_format) printk("\n,"STAT_CSV_TAG",");

		last_tstamp = curr_log_ptr->tstamp;
		display_log_info(curr_log_ptr, ic, opt, show_all_nodes, csv_format);
	}
}

#if 0
static void dump_log(struct qtn_stats_log *log)
{

	int cr_indx = 0;

	printk("Current Log indx %d\n", log->curr_buff);

	for(cr_indx=0;cr_indx < NUM_LOG_BUFFS; cr_indx++){
		printk("Indx %d, Tstamp %d\n",cr_indx,log->stat_buffs[cr_indx].tstamp);
	}
}
#endif

static void copy_log_to_local(u32 *muc_log_addr,struct qtn_stats_log *log)
{

	memcpy(log,muc_log_addr,sizeof(struct qtn_stats_log));
}

int qdrv_muc_stats_printlog(const struct qdrv_cb *data,
			    struct qdrv_mac *mac,
			    struct ieee80211com *ic,
			    int argc,
			    char **argv)
{
	static int cnt = 0;
	enum muc_stats_opt opt = MSO_BOTH;
	int show_all_nodes = 0;
	int csv_format = 0;
	int title_only = 0;
	int i;

	if (mac->mac_sys_stats == NULL) {
		printk(KERN_DEBUG "No MuC stats available\n");
		return 0;
	}

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "both") == 0) {
			opt = MSO_BOTH;
		} else if (strcmp(argv[i], "su") == 0) {
			opt = MSO_SU;
		} else if (strcmp(argv[i], "mu") == 0) {
			if (!ic->ic_mu_enable) {
				printk("MU not enabled\n");
				return 0;
			}
			opt = MSO_MU;
		} else if (strcmp(argv[i], "all") == 0) {
			show_all_nodes = 1;
		} else if (strcmp(argv[i], "csv") == 0) {
			csv_format = 1;
		} else if (strcmp(argv[i], "title") == 0) {
			title_only = 1;
		}
	}

	if (opt == MSO_BOTH && !ic->ic_mu_enable) {
		opt = MSO_SU;
	}

	if((!csv_format && !(cnt++ & 0x3)) || (csv_format && title_only)) {
		/* Display header periodically */
		display_log_hdr(ic, opt, show_all_nodes, csv_format);
	}

	if (title_only) goto exit;

	copy_log_to_local((u32*)mac->mac_sys_stats, &host_log);
	parse_log(&host_log, ic, opt, show_all_nodes, csv_format);

exit:
	return(0);
}

/*
 * These functions need to be prepared for qtn_muc_stats_get_addr_latest_stats returning NULL.
 *
 * Currently they all return -1 to signal Failed to Get Requested Value.
 */
int qdrv_muc_get_noise(struct qdrv_mac *mac, const struct ieee80211com *ic)
{
	int retval = -1;
	struct qtn_stats *address_current_log = qtn_muc_stats_get_addr_latest_stats(
							mac, ic, MUC_PHY_STATS_ALTERNATE);
	static uint32_t prev_hw_noise = 0;

	if (address_current_log != NULL) {
		if (address_current_log->rx_phy_stats.hw_noise > 0)
			prev_hw_noise = address_current_log->rx_phy_stats.hw_noise;
		retval = prev_hw_noise;
	}

	return(retval);
}

int qdrv_muc_get_rssi_by_chain(struct qdrv_mac *mac, const struct ieee80211com *ic, unsigned int rf_chain)
{
	int retval = -1;
	struct qtn_stats *address_current_log = qtn_muc_stats_get_addr_latest_stats(
							mac, ic, MUC_PHY_STATS_RSSI_RCPI_ONLY);

	if (rf_chain >= NUM_ANT) {
		rf_chain = NUM_ANT - 1;
	}

	if (address_current_log != NULL) {
		retval = address_current_log->rx_phy_stats.last_rssi_evm[rf_chain];
	}

	return(retval);
}

u_int32_t qdrv_muc_get_rx_gain_fields(struct qdrv_mac *mac, const struct ieee80211com *ic)
{
	u_int32_t rx_gain_fields = (u_int32_t) -1;
	struct qtn_stats *address_current_log = qtn_muc_stats_get_addr_latest_stats(mac,
										    ic,
										    MUC_PHY_STATS_RSSI_RCPI_ONLY);

	if (address_current_log != NULL) {
		int8_t dagc_shift = 0;

		rx_gain_fields = address_current_log->rx_phy_stats.rx_gain_fields;
		dagc_shift = (int8_t) qtn_muc_stats_get_dagc_shift_count(rx_gain_fields);

		rx_gain_fields = (rx_gain_fields & ~QDRV_MUC_STATS_DAGC_SHIFT_FIELD_M);
		rx_gain_fields = (rx_gain_fields |
			((dagc_shift << QDRV_MUC_STATS_DAGC_SHIFT_FIELD_S) & QDRV_MUC_STATS_DAGC_SHIFT_FIELD_M));
	}

	return(rx_gain_fields);
}

int qdrv_muc_get_phy_stat(struct qdrv_mac *mac,
			  const struct ieee80211com *ic,
			  const char *name_of_stat,
			  const unsigned int array_index,
			  int *stat_value)
{
	const struct {
		char			*stat_param_name;
		enum qtn_phy_stat_field stat_param_field;
		int			stat_phy_stat_mode;
	} stat_param_table[] = {
		{ QTN_PHY_AVG_ERROR_SUM_NSYM_NAME,
		  QTN_PHY_AVG_ERROR_SUM_NSYM_FIELD,
		  MUC_PHY_STATS_ERROR_SUM_ONLY },
	};

	int iter;
	enum qtn_phy_stat_field	param_field = QTN_PHY_NOSUCH_FIELD;
	int required_phy_stat_mode = MUC_PHY_STATS_ALTERNATE;
	struct qtn_stats *address_current_log = NULL;
	int retval = 0;

	for (iter = 0; iter < ARRAY_SIZE(stat_param_table); iter++) {
		if (strcmp(name_of_stat, stat_param_table[iter].stat_param_name) == 0) {
			param_field = stat_param_table[iter].stat_param_field;
			required_phy_stat_mode = stat_param_table[iter].stat_phy_stat_mode;
			break;
		}
	}

	if (param_field == QTN_PHY_NOSUCH_FIELD) {
#if 0
		DPRINTF(LL_1, LF_ERROR,
			(DBGEFMT "Unknown field %s in get phy_stat.\n", DBGARG, name_of_stat));
#endif
		return -1;
	}

	address_current_log = qtn_muc_stats_get_addr_latest_stats(mac,
								  ic,
								  required_phy_stat_mode);
	if (address_current_log == NULL) {
#if 0
		DPRINTF(LL_1, LF_ERROR,
			(DBGEFMT "Incorrect phy stat mode in get phy_stat for %s.\n", DBGARG, name_of_stat));
#endif
		return -1;
	}

	switch (param_field) {
	case QTN_PHY_AVG_ERROR_SUM_NSYM_FIELD:
		{
			int	sum_snr = 0;
			int i;

			for (i = 0; i < NUM_ANT; i++) {
				sum_snr += address_current_log->rx_phy_stats.last_rssi_evm[i];
			}

			if (sum_snr < 0) {
				sum_snr = (sum_snr - 5) / 10;
			} else {
				sum_snr = (sum_snr + 5) / 10;
			}

			*stat_value = (0 - sum_snr);
		}
		break;

	default:
#if 0
		DPRINTF(LL_1, LF_ERROR,
			(DBGEFMT "No support for %s in get phy_stat.\n", DBGARG, name_of_stat));
#endif
		retval = -1;
		break;
	}

	return retval;
}

int qdrv_muc_stats_rssi(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->rx[mu].last_rssi_dbm[NUM_ANT];
}

int qdrv_muc_stats_smoothed_rssi(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->rx[mu].rssi_dbm_smoothed[NUM_ANT];
}

int qdrv_muc_stats_hw_noise(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->rx[mu].last_hw_noise[NUM_ANT];
}

int qdrv_muc_stats_rxtx_phy_rate(const struct ieee80211_node *ni, const int is_rx,
		uint8_t *nss, uint8_t *mcs, u_int32_t *phy_rate)
{
	unsigned int last_mcs;
	int mu = STATS_SU;

	last_mcs = (is_rx) ? ni->ni_shared_stats->rx[mu].last_mcs :
			ni->ni_shared_stats->tx[mu].last_mcs;

	if (nss)
		*nss = MS(last_mcs, QTN_PHY_STATS_MCS_NSS);
	if (mcs)
		*mcs = MS(last_mcs, QTN_STATS_MCS_RATE_MASK);
	if (phy_rate)
		*phy_rate = MS(last_mcs, QTN_PHY_STATS_MCS_PHYRATE);

	return 0;
}

int qdrv_muc_stats_snr(const struct ieee80211_node *ni)
{
	int	sum_snr = 0;
	int i;
	const struct qtn_node_shared_stats_rx *rxstats;
	int mu = STATS_SU;

	rxstats = &ni->ni_shared_stats->rx[mu];

	for (i = 0; i < NUM_ANT; i++) {
		sum_snr += rxstats->last_evm_dbm[i];
	}

	return sum_snr;
}

int qdrv_muc_stats_max_queue(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->tx[mu].max_queue;
}

ssize_t qdrv_muc_get_size_rssi_phy_stats(void)
{
	return sizeof(struct qtn_stats);
}

u_int32_t qdrv_muc_stats_tx_failed(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->tx[mu].txdone_failed_cum;
}

static void qdrv_muc_stats_get_evm(struct ieee80211_phy_stats *ps,
				const struct qtn_node_shared_stats_rx *node_rxstats)
{
	int iter;
	int v;

	for (iter = 0; iter <= NUM_ANT; iter++) {
		v = node_rxstats->last_evm_dbm[iter];

		if (iter < NUM_ANT) {
			ps->last_evm_array[iter] = v;
		} else {
			ps->last_evm = v;
		}
	}

#ifdef  PHY_STATS_SUM_EVM
	ps->last_evm = 0;
	for (iter = 0; iter < NUM_ANT; iter++) {
		ps->last_evm += node_rxstats->last_evm_dbm[iter];
	}
#endif
}

static void qdrv_muc_stats_get_rssi(struct ieee80211_phy_stats *ps,
				const struct qtn_node_shared_stats_rx *node_rxstats)
{
	int iter;
	int rssi_dbm;

	for (iter = 0; iter <= NUM_ANT; iter++) {
		rssi_dbm = node_rxstats->last_rssi_dbm[iter];
		if (rssi_dbm) {
			if (iter == NUM_ANT) {
				ps->last_rssi = rssi_dbm;
			} else {
				ps->last_rssi_array[iter] = rssi_dbm;
			}
		} else {
			if (iter == NUM_ANT) {
				ps->last_rssi = 0;
			} else {
				ps->last_rssi_array[iter] = 0;
			}
		}
	}
}

void qdrv_muc_update_missing_stats(struct qdrv_mac *mac,
				struct ieee80211com *ic,
				struct ieee80211_phy_stats *ps,
				enum qdrv_muc_stats_display_choice missing_type)
{
	struct qtn_stats *prev_stats = NULL;
	struct qtn_rx_stats *prev_rx_stats = NULL;
	int iter;

	if (missing_type == QDRV_MUC_STATS_SHOW_RSSI) {
		prev_stats = qtn_muc_stats_get_addr_latest_stats(mac,
					ic, MUC_PHY_STATS_RSSI_RCPI_ONLY);
		if (prev_stats == NULL) {
			return;
		}
		prev_rx_stats = &prev_stats->rx_phy_stats;

		memcpy(ps->last_rssi_array, prev_rx_stats->last_rssi_evm,
					sizeof(prev_rx_stats->last_rssi_evm));
	} else {
		prev_stats = qtn_muc_stats_get_addr_latest_stats(mac,
					ic, MUC_PHY_STATS_ERROR_SUM_ONLY);
		if (prev_stats == NULL) {
			return;
		}
		prev_rx_stats = &prev_stats->rx_phy_stats;

		memcpy(ps->last_evm_array, prev_rx_stats->last_rssi_evm,
					sizeof(prev_rx_stats->last_rssi_evm));

#ifdef  PHY_STATS_SUM_EVM
		ps->last_evm = 0;
		for (iter = 0; iter < NUM_ANT; iter++) {
			ps->last_evm += ps->last_evm_array[iter];
		}
#endif
	}
}

int qdrv_muc_get_last_phy_stats(struct qdrv_mac *mac,
				struct ieee80211com *ic,
				struct ieee80211_phy_stats *ps,
				uint8_t all_stats)
{
	struct qtn_stats *stats;
	struct qtn_rx_stats *rx_stats = NULL;
	struct qtn_tx_stats *tx_stats = NULL;
	enum qdrv_muc_stats_display_choice display_choice = QDRV_MUC_STATS_SHOW_RSSI;
	enum qdrv_muc_stats_display_choice missing_type = QDRV_MUC_STATS_SHOW_EVM;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();
	struct qtn_scs_info_set *scs_info_lh = sp->scs_info_lhost;
	struct qtn_scs_info scs_info_read;
	int txpower;
	int rssi = SCS_RSSI_UNINITED;
	int mu = STATS_SU;

	struct ieee80211_node *ni;
	const struct qtn_node_shared_stats_rx *node_rxstats;
	const struct qtn_node_shared_stats_tx *node_txstats;

	DBGPRINTF(DBG_LL_ERR, QDRV_LF_TRACE, "-->Enter\n");

	if (ps == NULL)
		return -1;

	memset(ps, 0, sizeof(struct ieee80211_phy_stats));

	if (unlikely(mac == NULL || mac->data ==NULL || ic == NULL || scs_info_lh == NULL)) {
		return -1;
	}

	memcpy((void *)&scs_info_read, (void *)&scs_info_lh->scs_info[scs_info_lh->valid_index],
			sizeof(struct qtn_scs_info));

	ni = qtn_muc_stats_get_node(ic, mu);

	if (unlikely(ni == NULL)) {
		return -1;
	}

	stats = qtn_muc_stats_get_addr_latest_stats(mac, ic, MUC_PHY_STATS_ALTERNATE);
	if (unlikely(stats == NULL)) {
		ieee80211_free_node(ni);
		return -1;
	}

	rx_stats = &stats->rx_phy_stats;
	tx_stats = &stats->tx_phy_stats;

	display_choice = qdrv_muc_stats_get_display_choice(stats, ic);

	if (ic->ic_opmode & IEEE80211_M_STA) {
		struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
		if (unlikely(vap == NULL)) {
			ieee80211_free_node(ni);
			return -1;
		}
		if (vap->iv_state & IEEE80211_S_RUN)
			ps->assoc = 1;
		else
			ps->assoc = 0;
	} else
		ps->assoc = ic->ic_sta_assoc;

	node_rxstats = &ni->ni_shared_stats->rx[mu];
	node_txstats = &ni->ni_shared_stats->tx[mu];

	ps->tstamp	= stats->tstamp;

	if (scs_info_read.cca_try) {
		ps->cca_tx	= scs_info_read.cca_tx * 1000 / scs_info_read.cca_try;
		ps->cca_rx	= scs_info_read.rx_usecs / scs_info_read.cca_try;
		ps->cca_int	= scs_info_read.cca_interference * 1000 / scs_info_read.cca_try;
		ps->cca_idle	= scs_info_read.cca_idle * 1000 / scs_info_read.cca_try;
		ps->cca_total	= ps->cca_tx + ps->cca_rx + ps->cca_int + ps->cca_idle;
	}

	ps->rx_pkts	= rx_stats->num_pkts;
	ps->rx_gain	= rx_stats->avg_rxgain;
	ps->rx_cnt_crc	= rx_stats->cnt_mac_crc;
	ps->rx_noise	= rx_stats->hw_noise;
	ps->tx_pkts	= tx_stats->num_pkts;
	ps->tx_defers	= tx_stats->num_defers;
	ps->tx_touts	= tx_stats->num_timeouts;
	ps->tx_retries	= tx_stats->num_retries;
	ps->cnt_sp_fail = rx_stats->cnt_sp_fail;
	ps->cnt_lp_fail = rx_stats->cnt_lp_fail;

	ps->last_rx_mcs = node_rxstats->last_mcs & QTN_STATS_MCS_RATE_MASK;
	ps->last_tx_mcs = node_txstats->last_mcs & QTN_STATS_MCS_RATE_MASK;
	ps->last_tx_scale = node_txstats->last_tx_scale;

	txpower = ic->ic_curchan->ic_maxpower;

	if (ic->ic_rssi)
		rssi = ic->ic_rssi(ni);

	if (SCS_RSSI_VALID(rssi)) {
		ps->atten = txpower - rssi / SCS_RSSI_PRECISION_RECIP;
	} else
		ps->atten = 0;

	switch (display_choice) {
	case QDRV_MUC_STATS_SHOW_EVM:
		qdrv_muc_stats_get_evm(ps, node_rxstats);
		missing_type = QDRV_MUC_STATS_SHOW_RSSI;
		break;
	case QDRV_MUC_STATS_SHOW_RCPI:
		ps->last_rcpi = node_rxstats->last_rcpi_dbm[NUM_ANT];
		break;
	case QDRV_MUC_STATS_SHOW_RSSI:
	default:
		/* RSSIs in dBM (units are actually 0.1 dBM) ... */
		qdrv_muc_stats_get_rssi(ps, node_rxstats);
		missing_type = QDRV_MUC_STATS_SHOW_EVM;
	}

	if (all_stats) {
		qdrv_muc_update_missing_stats(mac, ic, ps, missing_type);
	}

	ieee80211_free_node(ni);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

u_int32_t qdrv_muc_stats_tx_airtime(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->tx[mu].tx_airtime;
}

u_int32_t qdrv_muc_stats_tx_accum_airtime(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->tx[mu].tx_accum_airtime;
}


u_int32_t qdrv_muc_stats_rx_airtime(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->rx[mu].rx_airtime;
}

u_int32_t qdrv_muc_stats_rx_accum_airtime(const struct ieee80211_node *ni)
{
	int mu = STATS_SU;
	return ni->ni_shared_stats->rx[mu].rx_accum_airtime;
}

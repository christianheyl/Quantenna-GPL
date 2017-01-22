/**
  Copyright (c) 2014 Quantenna Communications Inc
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

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <qtn/shared_defs.h>
#include <qtn/muc_phy_stats.h>
#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>

#include "qdrv_show.h"
#include "qdrv_control.h"

#define QDRV_INTBASE_10			10

#define QDRV_EMPTY_FILTER_IDX		0xFFFF

#define QDRV_MIN_DBMVAL_BUFSIZE		6
#define QDRV_SHORT_TEXT_BUFSIZE		8

struct qdrv_mcs_info {
	char mcs[QDRV_SHORT_TEXT_BUFSIZE];
	char rate[QDRV_SHORT_TEXT_BUFSIZE];
	char bw[QDRV_SHORT_TEXT_BUFSIZE];
};

static int qdrv_show_assoc_parse_group(const char *text, enum qdrv_show_assoc_group *group)
{
	int retcode = 0;

	if (strcmp(text, "state") == 0)
		*group = QDRV_SHOW_ASSOC_STATE;
	else if (strcmp(text, "ver") == 0)
		*group = QDRV_SHOW_ASSOC_VER;
	else if (strcmp(text, "phy") == 0)
		*group = QDRV_SHOW_ASSOC_PHY;
	else if (strcmp(text, "tx") == 0)
		*group = QDRV_SHOW_ASSOC_TX;
	else if (strcmp(text, "rx") == 0)
		*group = QDRV_SHOW_ASSOC_RX;
	else if (strcmp(text, "all") == 0)
		*group = QDRV_SHOW_ASSOC_ALL;
	else
		retcode = -EINVAL;

	return retcode;
}

void qdrv_show_assoc_init_params(struct qdrv_show_assoc_params *params, struct qdrv_mac *mac)
{
	params->mac = mac;
	params->show_group = QDRV_SHOW_ASSOC_STATE;
	IEEE80211_ADDR_SET_NULL(params->filter_macaddr);
	params->filter_idx = QDRV_EMPTY_FILTER_IDX;
}

int qdrv_show_assoc_parse_params(struct qdrv_show_assoc_params *params, int argc, char *argv[])
{
	int i;
	const char* text;
	unsigned long num;
	enum qdrv_show_assoc_group curr_group = QDRV_SHOW_ASSOC_STATE;
	unsigned char found_group = 0;
	unsigned char found_filter = 0;
	int retcode = 0;

	if (!params)
		return -EINVAL;

	for (i = 0; i < argc; ++i) {
		text = argv[i];

		if (text && *text) {
			if (qdrv_show_assoc_parse_group(text, &params->show_group) == 0) {
				if (found_group) {
					if ((curr_group == QDRV_SHOW_ASSOC_PHY)
						&& (params->show_group == QDRV_SHOW_ASSOC_ALL)) {
						/* detected 'phy all' */
						params->show_group = QDRV_SHOW_ASSOC_PHY_ALL;
					} else {
						retcode = -EINVAL;
						break;
					}
				}

				curr_group = params->show_group;
				found_group = 1;

			} else if (qdrv_parse_mac(text, &params->filter_macaddr[0]) == 0) {
				if (found_filter) {
					retcode = -EINVAL;
					break;
				}

				found_filter = 1;

			} else if (strict_strtoul(text, QDRV_INTBASE_10, &num) == 0) {
				if (found_filter) {
					retcode = -EINVAL;
					break;
				}

				params->filter_idx = (uint16_t)num;

				if (params->filter_idx == QDRV_EMPTY_FILTER_IDX) {
					retcode = -EINVAL;
					break;
				}

				found_filter = 1;
			} else {
				retcode = -EINVAL;
				break;
			}
		}
	}

	return retcode;
}

void qdrv_show_assoc_print_usage(struct seq_file *s, void *data, uint32_t num)
{
	if (!s)
		return;

	seq_printf(s, "Invalid parameter. show_assoc {state | ver | phy {all}| tx | rx | all}"
		      " {macaddr | index}\n");
}

static void qdrv_show_assoc_state(struct seq_file *s, struct ieee80211com *ic,
		struct ieee80211_node *ni)
{
	seq_printf(s, "%-17s %4s %4s %6s %4s %8s %4s %7s %6s   %8s %12s %10s %16s\n",
			"MAC", "Idx", "AID", "Type", "Mode", "Vendor", "BW", "Assoc", "Auth",
			"BA State", "TDLS State", "VAP", "PowerSaveSchemes");

	if (ni)
		get_node_assoc_state(s, ni);
	else
		ic->ic_iterate_nodes(&ic->ic_sta, get_node_assoc_state, (void *)s, 1);
}

static void qdrv_show_assoc_ver(struct seq_file *s, struct ieee80211com *ic,
		struct ieee80211_node *ni)
{
	seq_printf(s, "%-17s %4s %-15s %-8s %-6s %-10s %-s\n",
			"MAC", "Idx", "SW Version", "Platform", "HW Rev", "Timestamp", "Flags");

	if (ni)
		get_node_ver(s, ni);
	else
		ic->ic_iterate_nodes(&ic->ic_sta, get_node_ver, (void *)s, 1);
}

static void qdrv_show_assoc_parse_mcs(uint32_t val, struct qdrv_mcs_info* m)
{
	unsigned char mcs = (unsigned char)(val  & QTN_STATS_MCS_RATE_MASK);
	unsigned char nss = (unsigned char)MS(val, QTN_PHY_STATS_MCS_NSS);
	unsigned phy_rate = MS(val, QTN_PHY_STATS_MCS_PHYRATE);
	const char *bw;
	const char *ht_mode;

	if (val) {
		snprintf(m->mcs, sizeof(m->mcs), "%3u", (unsigned)nss * 100 + mcs);

		if (phy_rate) {
			snprintf(m->rate, sizeof(m->rate), "%4uM", phy_rate);
		} else {
			strcpy(m->rate, "-");
		}

		switch(MS(val, QTN_PHY_STATS_MCS_BW)) {
		case QTN_BW_20M:
			bw = IEEE80211_BWSTR_20;
			break;
		case QTN_BW_40M:
			bw = IEEE80211_BWSTR_40;
			break;
		case QTN_BW_80M:
			bw = IEEE80211_BWSTR_80;
			break;
		default:
			bw = NULL;
			break;
		}

		switch(MS(val, QTN_PHY_STATS_MCS_MODE)) {
		case QTN_PHY_STATS_MODE_11N:
			ht_mode = "h";
			break;
		case QTN_PHY_STATS_MODE_11AC:
			ht_mode = "v";
			break;
		default:
			ht_mode = "";
			break;
		}

		if (bw) {
			snprintf(m->bw, sizeof(m->bw), "%s%s", bw, ht_mode);
		} else {
			strcpy(m->bw, "-");
		}
	} else {
		/* undefined */
		strcpy(m->mcs, "-");
		strcpy(m->rate, "-");
		strcpy(m->bw, "-");
	}
}

static int qdrv_show_assoc_conv_dbm2str(unsigned int val, unsigned char zero_allowed,
		char* buf, unsigned int len)
{
	if (buf && (len > QDRV_MIN_DBMVAL_BUFSIZE)) {
		if (val || zero_allowed) {
			snprintf(buf, len, "%4d.%d", (int)val / 10, ABS((int)val) % 10);
		} else {
			strcpy(buf, "    - ");
		}

		return 0;
	}

	return -EINVAL;
}

static void qdrv_show_assoc_print_indication(struct seq_file *s, const char *name,
		const uint32_t *values, unsigned int num,
		uint32_t sum_val, const char* sum_name, unsigned char zero_allowed)
{
	unsigned int i;
	char tmpbuf[QDRV_SHORT_TEXT_BUFSIZE];

	seq_printf(s, "  %-8s", name);

	for (i = 0; i < num; ++i) {
		if (qdrv_show_assoc_conv_dbm2str(values[i], zero_allowed,
				tmpbuf, sizeof(tmpbuf)) != 0) {
			seq_printf(s, "error\n");
			return;
		}

		seq_printf(s, " %6s", tmpbuf);
	}

	/* summary value */
	if (qdrv_show_assoc_conv_dbm2str(sum_val, zero_allowed, tmpbuf, sizeof(tmpbuf)) != 0) {
		seq_printf(s, "error\n");
		return;
	}

	seq_printf(s, "  %-3s %6s\n", sum_name, tmpbuf);
}

static void qdrv_show_assoc_print_phy_header(struct seq_file *s)
{
	/*                   3    4   5    6    7     8   9     10   11   12    13      14 */
	const char* htx = "Pkts Qual  SNR MCS  Rate   BW Sca   RSSI Cost Fail AvgPER   Acks";
	/*                  15   16   17    18  19  20 */
	const char* hrx = "Pkts MCS  Rate   BW Sym Cost";

	/* header */
	seq_printf(s, "%-17s %4s (TX) %s (RX) %s\n", "MAC", "Idx", htx, hrx);
}

static void qdrv_show_assoc_print_phy_detail(void *data, struct ieee80211_node *ni)
{
	struct seq_file *s = (struct seq_file *)data;
	struct qtn_node_shared_stats_tx *tx = &ni->ni_shared_stats->tx[STATS_SU];
	struct qtn_node_shared_stats_rx *rx = &ni->ni_shared_stats->rx[STATS_SU];
	struct qdrv_mcs_info mi;
	char rssi[QDRV_SHORT_TEXT_BUFSIZE];

	ieee80211_update_node_assoc_qual(ni);

	/* TX */
	qdrv_show_assoc_parse_mcs(tx->last_mcs, &mi);

	if (qdrv_show_assoc_conv_dbm2str(tx->avg_rssi_dbm, 0, rssi, sizeof(rssi)) != 0) {
		seq_printf(s, "error\n");
		return;
	}

	/*              1   2   3   4   5   6   7   8   9  10  11  12  13  14 */
	seq_printf(s, "%pM %4u %9u %4u %4d %3s %5s %4s %3u %6s %4u %4u %6u %6u",
			ni->ni_macaddr,
			IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx),
			tx->pkts,
			ni->ni_linkqual,
			ni->ni_snr,
			mi.mcs,
			mi.rate,
			mi.bw,
			tx->last_tx_scale,
			rssi,
			tx->cost,
			tx->txdone_failed_cum,
			tx->avg_per,
			tx->acks);

	/* RX */
	qdrv_show_assoc_parse_mcs(rx->last_mcs, &mi);

	/*              15  16  17  18  19  20 */
	seq_printf(s, " %9u %3s %5s %4s %3u %4u\n",
			tx->pkts,
			mi.mcs,
			mi.rate,
			mi.bw,
			rx->last_rxsym,
			rx->cost);
}

static void qdrv_show_assoc_print_phy_full(void *data, struct ieee80211_node *ni)
{
	struct seq_file *s = (struct seq_file *)data;
	struct qtn_node_shared_stats_rx *rx = &ni->ni_shared_stats->rx[STATS_SU];
	int i;
	int evm_sum = 0;

	qdrv_show_assoc_print_phy_detail(s, ni);

	qdrv_show_assoc_print_indication(s, "RSSI:", rx->last_rssi_dbm, NUM_ANT,
			rx->last_rssi_dbm[NUM_ANT], "avg", 0);

	qdrv_show_assoc_print_indication(s, "RCPI:", rx->last_rcpi_dbm, NUM_ANT,
			rx->last_rcpi_dbm[NUM_ANT], "max", 0);

	for (i = 0; i < NUM_ANT; i++) {
		evm_sum += (int)rx->last_evm_dbm[i];
	}

	qdrv_show_assoc_print_indication(s, "EVM:", rx->last_evm_dbm, NUM_ANT,
			(uint32_t)evm_sum, "sum", 1);

	qdrv_show_assoc_print_indication(s, "HWNOISE:", rx->last_hw_noise, NUM_ANT,
			rx->last_hw_noise[NUM_ANT], "avg", 0);
}

static void qdrv_show_assoc_phy_stats(struct seq_file *s, struct ieee80211com *ic,
		struct ieee80211_node *ni, unsigned char full)
{
	ieee80211_iter_func *print_phy_stats =
			full ? qdrv_show_assoc_print_phy_full : qdrv_show_assoc_print_phy_detail;

	qdrv_show_assoc_print_phy_header(s);

	if (ni)
		print_phy_stats(s, ni);
	else
		ic->ic_iterate_nodes(&ic->ic_sta, print_phy_stats, (void *)s, 1);
}

static void qdrv_show_assoc_tx_stats(struct seq_file *s, struct ieee80211com *ic,
		struct ieee80211_node *ni)
{
	seq_printf(s, "%-17s %4s %8s %10s %12s %8s %8s %10s %10s %10s %5s %7s\n",
			"MAC (TX)", "Idx",  "Max PR", "Frames", "Bytes", "Errors",
			"Drops", "Unicast", "Multicast", "Broadcast", "MaxQ", "Fails");

	if (ni)
		get_node_tx_stats(s, ni);
	else
		ic->ic_iterate_nodes(&ic->ic_sta, get_node_tx_stats, (void *)s, 1);
}

static void qdrv_show_assoc_rx_stats(struct seq_file *s, struct ieee80211com *ic,
		struct ieee80211_node *ni)
{
	seq_printf(s, "%-17s %4s %8s %8s %10s %12s %8s %8s %10s %10s %10s\n",
			"MAC (RX)", "Idx", "Max PR", "PhyRate", "Frames", "Bytes", "Errors",
			"Drops", "Unicast", "Multicast", "Broadcast");

	if (ni)
		get_node_rx_stats(s, ni);
	else
		ic->ic_iterate_nodes(&ic->ic_sta, get_node_rx_stats, (void *)s, 1);
}

static void qdrv_show_assoc_all(struct seq_file *s, struct ieee80211com *ic,
		struct ieee80211_node *ni)
{
	seq_printf(s, "Assoc State:\n");
	qdrv_show_assoc_state(s, ic, ni);

	seq_printf(s, "\nVersion:\n");
	qdrv_show_assoc_ver(s, ic, ni);

	seq_printf(s, "\nTx Stats:\n");
	qdrv_show_assoc_tx_stats(s, ic, ni);

	seq_printf(s, "\nRx Stats:\n");
	qdrv_show_assoc_rx_stats(s, ic, ni);

	seq_printf(s, "\nPHY Stats:\n");
	qdrv_show_assoc_phy_stats(s, ic, ni, 1);
}

void qdrv_show_assoc_print_stats(struct seq_file *s, void *data, uint32_t num)
{
	struct qdrv_show_assoc_params *params = (struct qdrv_show_assoc_params *)data;
	struct qdrv_wlan *qw;
	struct ieee80211com *ic;
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni = NULL;

	if (!s || !params || !params->mac)
		return;

	qw = (struct qdrv_wlan*)params->mac->data;

	if (!qw)
		return;

	ic = &qw->ic;
	nt = &ic->ic_sta;

	if (!IEEE80211_ADDR_NULL(params->filter_macaddr)) {
		ni = ieee80211_find_node(nt, params->filter_macaddr);

		if (!ni) {
			seq_printf(s, "node %pM not found\n", params->filter_macaddr);
			return;
		}

	} else if (params->filter_idx != QDRV_EMPTY_FILTER_IDX) {
		ni = ieee80211_find_node_by_idx(ic, NULL, params->filter_idx);

		if (!ni) {
			seq_printf(s, "node index %u not found\n", (unsigned)params->filter_idx);
			return;
		}
	}

	switch(params->show_group) {
	case QDRV_SHOW_ASSOC_VER:
		qdrv_show_assoc_ver(s, ic, ni);
		break;
	case QDRV_SHOW_ASSOC_PHY:
		qdrv_show_assoc_phy_stats(s, ic, ni, 0);
		break;
	case QDRV_SHOW_ASSOC_PHY_ALL:
		qdrv_show_assoc_phy_stats(s, ic, ni, 1);
		break;
	case QDRV_SHOW_ASSOC_TX:
		qdrv_show_assoc_tx_stats(s, ic, ni);
		break;
	case QDRV_SHOW_ASSOC_RX:
		qdrv_show_assoc_rx_stats(s, ic, ni);
		break;
	case QDRV_SHOW_ASSOC_ALL:
		qdrv_show_assoc_all(s, ic, ni);
		break;
	case QDRV_SHOW_ASSOC_STATE:
	default:
		qdrv_show_assoc_state(s, ic, ni);
		break;
	}

	if (ni)
		ieee80211_free_node(ni);
}

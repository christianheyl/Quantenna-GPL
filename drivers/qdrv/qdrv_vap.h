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

#ifndef _QDRV_VAP_H
#define _QDRV_VAP_H

/* Include the WLAN 802.11 layer here */
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>
#include "qdrv_mac.h"
#include <qtn/qtn_vlan.h>
/*
 * Default dwell times for scanning channels.
 * These are the original values from ieee80211_scan_sta.c
 * and ieee80211_scan_ap.c
 * Units are milliseconds.
 *
 * Minimum dwell time (MIN_DWELLTIME) must be larger than 1/2 of
 * the maximum dwell time (MAX_DWELLTIME) to allow the delayed
 * probe request timer to work correctly.
 * This is necessary for active scans as the probe requests are
 * sent out at maxdwell/2 and we need to allow time for the probe
 * requests to be sent and the response to come back before mindwell.
 *
 * Note that on the STA passive channels have a larger min and max
 * dwell to increase the probability that we'll 'hear' a beacon on
 * passive channel.
 */
#define QDRV_WLAN_STA_MIN_DWELLTIME_ACTIVE	100
#define QDRV_WLAN_STA_MIN_DWELLTIME_PASSIVE	450
#define QDRV_WLAN_STA_MAX_DWELLTIME_ACTIVE	150
#define QDRV_WLAN_STA_MAX_DWELLTIME_PASSIVE	600

#define QDRV_WLAN_AP_MIN_DWELLTIME_ACTIVE	200
#define QDRV_WLAN_AP_MIN_DWELLTIME_PASSIVE	200
#define QDRV_WLAN_AP_MAX_DWELLTIME_ACTIVE	300
#define QDRV_WLAN_AP_MAX_DWELLTIME_PASSIVE	300

#define QDRV_WLAN_QTN_BGSCAN_DWELLTIME_ACTIVE	20 /* milliseconds */
#define QDRV_WLAN_QTN_BGSCAN_DWELLTIME_PASSIVE	20 /* milliseconds */

#define QDRV_WLAN_QTN_BGSCAN_DURATION_ACTIVE		200 /* milliseconds */
#define QDRV_WLAN_QTN_BGSCAN_DURATION_PASSIVE_FAST	450 /* milliseconds */
#define QDRV_WLAN_QTN_BGSCAN_DURATION_PASSIVE_NORMAL	750 /* milliseconds */
#define QDRV_WLAN_QTN_BGSCAN_DURATION_PASSIVE_SLOW	1400 /* milliseconds */

#define QDRV_WLAN_QTN_BGSCAN_THRESHLD_PASSIVE_FAST	600 /* FAT 600/1000 */
#define QDRV_WLAN_QTN_BGSCAN_THRESHLD_PASSIVE_NORMAL	300 /* FAT 300/1000 */

/**
 * Structure to contain a mapping of DMA mapped addresses to host addresses.
 * This structure lives as a ring, same size as the vap IOCTL ring, with a 
 * 1-1 mapping of IOCTL ring entry to dma allocation entry.
 *
 * When an ioctl ptr is passed back via vnet_alloc_ioctl, the corresponding
 * entry in the dma ring will be filled in based on the input parameters to
 * the alloc call.
 *
 * When the ioctl call completes (interrupt from the MuC), the corresponding
 * dma ring pointer will be freed (if non-NULL), ensuring we don't leak the
 * dma buffers.
 *
 * Synchronisation between the send of the IOCTL and the IOCTL complete
 * should be done for all structures involved or we could get into a bad situation.
 */
struct dma_allocation
{
	void *p_host_addr;
	u_int32_t dma_addr;
	size_t size;
};

struct qdrv_vap_tx_stats
{
	unsigned int tx_;
};

struct qdrv_vap_rx_stats
{
	unsigned int rx_;
};

struct qdrv_vap
{
	struct ieee80211vap iv;	/* Must be first for 802.11 layer */
	struct net_device *ndev;
	uint32_t muc_queued;
	TAILQ_HEAD(, qdrv_node) allnodes;
	void *parent;
	spinlock_t bc_lock;
	struct sk_buff *bc_skb;
	unsigned int devid;

	/* Synchronisation */
	spinlock_t lock;

	struct net_device_stats stats;
	struct qdrv_vap_tx_stats tx_stats;
	struct qdrv_vap_rx_stats rx_stats;

	/* 802.11 layer callbacks and interface */
	int (*qv_newstate)(struct ieee80211vap *, enum ieee80211_state, int);
	struct ieee80211_beacon_offsets qv_boff; /* dynamic update state */
	struct napi_struct napi;

	TAILQ_HEAD(, ieee80211_node) ni_lncb_lst;	/* STAs supporting 4-addr LNCB reception */
	int ni_lncb_cnt;				/* Total entries in LNCB list */
	TAILQ_HEAD(, ieee80211_node) ni_bridge_lst;	/* Associated bridge stations */
	int ni_bridge_cnt;				/* Total entries in bridge STA list */
	spinlock_t ni_lst_lock;				/* Lock for the above fields */
	int iv_3addr_count;
	uint8_t         qv_vap_idx;
	uint32_t	qv_bmps_mode;
};

int qdrv_vap_init(struct qdrv_mac *mac, struct host_ioctl_hifinfo *hifinfo,
	u32 arg1, u32 arg2);
int qdrv_vap_exit(struct qdrv_mac *mac, struct net_device *vnet);
int qdrv_vap_exit_muc_done(struct qdrv_mac *mac, struct net_device *vnet);
int qdrv_exit_all_vaps(struct qdrv_mac *mac);
int qdrv_vap_wds_mode(struct qdrv_vap *qv);
char *qdrv_vap_wds_peer(struct qdrv_vap *qv);
int qdrv_vap_vlan2index_sync(struct qdrv_vap *qv, uint16_t command, uint16_t vlanid);
struct host_ioctl *vnet_alloc_ioctl(struct qdrv_vap *qv);
void vnet_free_ioctl(struct host_ioctl *ioctl);
int vnet_send_ioctl(struct qdrv_vap *qv, struct host_ioctl *block);
void qdrv_tx_sch_attach(struct qdrv_vap *qv);
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
int qdrv_get_active_sub_port(const struct net_bridge_port *p,
		uint32_t *sub_port_bitmap, int size);
int qdrv_check_active_sub_port(const struct net_bridge_port *p,
		const uint32_t sub_port);
#endif

#endif

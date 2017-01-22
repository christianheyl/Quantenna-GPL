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

#ifndef _QDRV_WLAN_H
#define _QDRV_WLAN_H

#include <linux/interrupt.h>

/* Include the WLAN 802.11 layer here */
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <net/ip.h>
#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>
#include "qdrv_comm.h"
#include "qdrv_debug.h"
#include "qtn/qtn_pcap.h"
#include "qdrv/qdrv_bridge.h"
#include "qtn/muc_txrx_stats.h"
#include "qtn/muc_phy_stats.h"
#include "qtn/auc_debug_stats.h"
#include "qtn/skb_recycle.h"
#include "qtn/qdrv_sch_data.h"
#include "qtn/iputil.h"
#ifdef CONFIG_QVSP
#include "qtn/qvsp.h"
#endif
#include "qtn/topaz_congest_queue.h"
#include <qtn/txbf_common.h>
#include <qtn/lhost_muc_comm.h>

#define QNET_TXLIST_ENTRIES_DEFAULT 512
#define QNET_TXLIST_ENTRIES_MAX 2048
#define QNET_TXLIST_ENTRIES_MIN 1
#define QNET_HLRING_ENTRIES 32
#define QDRV_NUM_RF_STREAMS IEEE80211_QTN_NUM_RF_STREAMS

#define RSSI_40M_TO_20M_DBM(x)		(x - 3)
#define RSSI_20M_TO_40M_DBM(x)		(x + 3)
#define RSSI_TOTAL_TO_PERCHAIN_DBM(x)	(x - 6)
#define RSSI_PERCHAIN_TO_TOTAL_DBM(x)	(x + 6)
#define LINK_MARGIN_INVALID		(-127)

enum qdrv_bld_type {
	QDRV_BLD_TYPE_ENG = 1,
	QDRV_BLD_TYPE_BENCH,
	QDRV_BLD_TYPE_BUILDBOT,
	QDRV_BLD_TYPE_REL,
	QDRV_BLD_TYPE_SDK,
	QDRV_BLD_TYPE_GPL
};

struct qdrv_vap;

/* driver-specific node state */
struct qdrv_node
{
	struct ieee80211_node qn_node;  /* Must be first for the 802.11 layer */
	struct kobject kobj;
	TAILQ_ENTRY(qdrv_node) qn_next;
	uint16_t qn_node_idx;		/* a copy of ni_node_idx */
};

#define MAX_MEMDEBUG_WATCHPTS	64
struct qdrv_memdebug_watchpt {
	uint32_t	addr;
	void	*remap_addr;
	size_t	size;
};

#include "qdrv_slab_def.h"

struct qdrv_meminfo {
	struct kmem_cache *caches[QDRV_SLAB_IDX_MAX];
};

struct qtn_auc_stat_field {
	uintptr_t addr;
	const char *name;
};

struct qdrv_auc_intr_stats {
	uint32_t sleep;
	uint32_t jiffies;
	uint32_t aucirq[AUC_TID_NUM];
};

struct qdrv_pktlogger_stats {
	uint32_t pkt_queued;
	uint32_t pkt_dropped;
	uint32_t pkt_failed;
	uint32_t pkt_requeued;
	uint32_t queue_send;
};

struct qdrv_pktlogger {
	struct qdrv_wlan *qw;
	struct net_device *dev;
	__be32 dst_ip;
	__be32 src_ip;
	__be16 dst_port;
	__be16 src_port;
	uint8_t dst_addr[IEEE80211_ADDR_LEN];
	uint8_t src_addr[IEEE80211_ADDR_LEN];
	uint8_t recv_addr[IEEE80211_ADDR_LEN];
	uint32_t maxfraglen;
	uint32_t flag;
	uint16_t ip_id;

	spinlock_t sendq_lock;
	STAILQ_HEAD(,qdrv_pktlogger_data) sendq_head;
	struct work_struct sendq_work;
	int sendq_scheduled;

	struct sock *netlink_socket;
	int netlink_ref;

	struct timer_list stats_timer;
	struct timer_list mem_timer;
	int mem_wp_index;
	struct qdrv_memdebug_watchpt mem_wps[MAX_MEMDEBUG_WATCHPTS];

	struct timer_list rate_timer;
	struct timer_list sysmsg_timer;
	struct timer_list flush_data;

	uint32_t *stats_uc_rx_ptr;
	uint32_t *stats_uc_rx_rate_ptr;
	uint32_t *stats_uc_rx_bf_ptr;
	uint32_t *stats_uc_tx_ptr;
	uint32_t *stats_uc_tx_rate_ptr;
	uint32_t *stats_uc_su_rates_read_ptr;
	uint32_t *stats_uc_mu_rates_read_ptr;
	uint32_t *stats_uc_scs_cnt;
	struct netdev_queue *netdev_q_ptr_w;
	struct netdev_queue *netdev_q_ptr_e;
	struct qdrv_meminfo qmeminfo;
	uint32_t queue_len;
	struct qdrv_pktlogger_stats stats;

	struct net_device *dev_emac0;
	struct net_device *dev_emac1;

	uint32_t *stats_auc_sleep_p;
	uint32_t *stats_auc_jiffies_p;
	uint32_t *stats_auc_intr_p;
	struct auc_dbg_counters *stats_auc_dbg_p;

	struct muc_rx_rates rx_rates[2];
	struct muc_rx_rates *rx_rate_pre;
	struct muc_rx_rates *rx_rate_cur;

	struct muc_rx_rates rx_ratelog[2];
	struct muc_rx_rates *rx_ratelog_pre;
	struct muc_rx_rates *rx_ratelog_cur;
	struct timer_list phy_stats_timer;
};

/**********/
/** SCAN **/
/**********/

struct host_scanif
{
	struct workqueue_struct *workqueue;
	u32 scan_sem_bit;
	u32 tx_sem_bit;
	volatile u32 *sc_res_mbox;
	volatile u32 *sc_req_mbox;
};

/**********/
/** RX   **/
/**********/

struct host_fifo_if
{
	struct host_descfifo *fifo;
	dma_addr_t fifo_dma;
	struct host_rxdesc *pending;
	struct host_rxdesc **descp;
	int ring_size;
	struct dma_pool *df_rxdesc_cache;
};

#define QNET_RXRING_ENTRIES		64
#define QNET_MGMTRING_ENTRIES	4
#define QNET_CTRLRING_ENTRIES	4
#define QNET_ERRRING_ENTRIES	4

struct host_rxif
{
	struct host_fifo_if rx;
	u32 rx_sem_bit;
};

#define QNET_RXRING_SIZE (QNET_RXRING_ENTRIES*sizeof(struct host_rxdesc))
#define QNET_RXBUF_SIZE (QNET_RXRING_ENTRIES*sizeof(struct host_buf))

/**********/
/** TX   **/
/**********/

#define QDRV_TXDESC_DATA		0
#define QDRV_TXDESC_MGMT		1
#define QDRV_TXDESC_QUEUE_MAX		2

/* each node is always allowed this many descriptors */
#define QDRV_TXDESC_THRESH_MAX_MIN	384
/* re-enable a node queue when muc_queued is this much less than max per node */
#define QDRV_TXDESC_THRESH_MIN_DIFF	32

struct host_txif
{
	uint16_t txdesc_cnt[QDRV_TXDESC_QUEUE_MAX];
	uint16_t list_max_size;
	uint16_t muc_thresh_high;
	uint16_t muc_thresh_low;
	struct tasklet_struct txdone_tasklet;

	struct host_ioctl *hl_ring;
	struct host_ioctl *hl_first;
	struct host_ioctl *hl_last;
	dma_addr_t hl_ring_dma;
	int hl_read;
	int hl_write;
	int hl_tosend;
	u32 hl_count;
	spinlock_t hl_flowlock;

	struct dma_pool *df_txdesc_cache;

	struct lhost_txdesc *df_txdesc_list_head;
	struct lhost_txdesc *df_txdesc_list_tail;

	volatile u32 *tx_mbox;
};

#define QNET_HLRING_SIZE (QNET_HLRING_ENTRIES*sizeof(struct host_ioctl))

/**********/
/** WLAN **/
/**********/

#define HOST_TXD_VERSION	0x01
#define CCA_TOKEN_INIT_VAL	0x50
#define QTN_RATE_11N		0x80    /* Same bit setting as in MLME */

#define QTN_RATE_PHY_OFDM	0
#define QTN_RATE_PHY_CCK	1
#define QTN_RATE_PHY_HT		2

struct qtn_rateentry
{
	u_int8_t re_ieeerate;    /* IEEE rate:2*phyrate (for legacy MLME)
							* or MCS index for 11n */
	u_int16_t   re_rate;    /* Rate in 100Kbps */
	u_int8_t    re_ctrlrate;    /* index in rate table of control
                       rate to use with this rate */
	u_int8_t    re_shortpre:1;  /* Must use short preamble */
	u_int8_t    re_basicrate:1; /* Is this rate a basic rate */
	u_int8_t    re_phytype:2;   /* Phy type */
} __packed;

struct qtn_ratetable
{
	u_int8_t        rt_num;     /* Number of entries (legacy + 11n) in the rate table */
	u_int8_t        rt_legacy_num;     /* Number of legacy entries in the rate table */
	struct qtn_rateentry    *rt_entries;    /* Array of entries */
} __packed;

struct qtn_channel
{
	u_int16_t channel_number;	/* IEEE channel number */
	u_int16_t channel_freq;		/* Channel frequency */
	u_int32_t channel_flags;	/* Channel flags */
	u_int16_t center_freq_40M;	/* Channel Center Frequency for 40MHz */
	u_int16_t center_freq_80M;	/* Channel Center Frequency for 80MHz */
	u_int16_t center_freq_160M;	/* Channel Center Frequency for 160MHz */
	u_int32_t channel_ext_flags;	/* Extra channel flags for 80MHZ mode */
} __packed;

#define QDRV_STAT(_qw, _is_tx, _member)	do	\
{						\
	if (_is_tx) {				\
		_qw->tx_stats._member++;	\
	} else {				\
		_qw->rx_stats._member++;	\
	}					\
} while (0)

#define TXSTAT(qw, member) \
	(qw)->tx_stats.member += 1

#define TXSTAT_SET(qw, member, val) \
	(qw)->tx_stats.member = (val)

#define RXSTAT(qw, member) \
	(qw)->rx_stats.member += 1

#define RXSTAT_SET(qw, member, val) \
	(qw)->rx_stats.member = (val)

#define SMSTAT(qw, member) \
	(qw)->sm_stats.member += 1

/* #define QDRV_TX_DEBUG 1 */
#ifdef QDRV_TX_DEBUG

extern uint32_t qdrv_tx_ctr[];
extern uint32_t qdrv_dbg_ctr[];
#define QDRV_TX_CTR_INC(_x)	qdrv_tx_ctr[_x]++;

#define QDRV_TX_DBG(_i, _ni, _fmt, ...) do			\
{								\
	struct ieee80211_node *__ni = _ni;			\
	if (_i >= 0) {						\
		if (qdrv_dbg_ctr[_i] == 0) {			\
			break;					\
		}						\
		qdrv_dbg_ctr[_i]--;				\
	}							\
	if (__ni) {						\
		printk("[%s]", ether_sprintf(__ni->ni_macaddr));\
	}							\
	printk("%s " _fmt,					\
		__func__, ##__VA_ARGS__);			\
} while (0)

#else

#define QDRV_TX_CTR_INC(_x)
#define QDRV_TX_DBG(_i, _ni, _fmt, ...) if (_ni) {}

#endif /* QDRV_TX_DEBUG */

/**
 * \defgroup LHOST_STATS Linux host generated stats
 */
/* @{ */

/**
 * \brief 802.11 state machine statistics.
 *
 * These statistics are updated as 802.11 management packets are sent and received
 * by both the AP and STA.
 */
struct qdrv_wlan_sm_stats {
	/**
	 * The number of times that the state machine went from trying to authenticate
	 * directly back to scanning - i.e. the AUTH request timed out or was rejected.
	 */
	unsigned int sm_scan_auth_fail_scan_pend;

	/**
	 * The number of times that the state machine went from trying to associate
	 * directly back to scanning - i.e. the ASSOC request timed out or was rejected.
	 */
	unsigned int sm_scan_assoc_fail_scan_pend;

	/**
	 * The number of times that a scan has been triggered (excluding failure to connect).
	 */
	unsigned int sm_scan_pend;

	/**
	 * The number of times that an authentication request is sent, waiting on response.
	 */
	unsigned int sm_auth_pend;

	/**
	 * The number of times that a deauth sequence is sent (i.e. going from authenticated
	 * to disconnected state).
	 */
	unsigned int sm_run_deauth_auth_pend;

	/**
	 * The number of times that an association request is sent, waiting on response.
	 */
	unsigned int sm_assoc_pend;

	/**
	 * The number of times that a disassociate sequence is sent (i.e. going from
	 * associated to authenticated state).
	 */
	unsigned int sm_run_disassoc_assoc_pend;

	/**
	 * The number of times a node is authenticated - i.e. becomes ready to send data packets.
	 */
	unsigned int sm_nd_auth;

	/**
	 * The number of times a node is unauthenticated.
	 */
	unsigned int sm_nd_unauth;

	/**
	 * The total number of nodes that are currently authenticated.
	 */
	unsigned int sm_nd_auth_tot;

	/**
	 * The number of times a station goes into connected state - i.e. ready to send data
	 * packets.
	 */
	unsigned int sm_sta_associated;

	/**
	 * The state of the device - composite of flags indicating current operating mode and
	 * radar flags.
	 */
	unsigned int sm_state;
#define QDRV_WLAN_SM_STATE_AP          0x00000001
#define QDRV_WLAN_SM_STATE_STA         0x00000002
#define QDRV_WLAN_SM_STATE_RADAR_EN    0x00000004
#define QDRV_WLAN_SM_STATE_RADAR_ACT   0x00000008
#define QDRV_WLAN_SM_STATE_CAC_ACTIVE  0x00000010
#define QDRV_SET_SM_FLAG(_stats, _flag) (_stats).sm_state |= (_flag)
#define QDRV_CLEAR_SM_FLAG(_stats, _flag) (_stats).sm_state &= ~(_flag)
};

/**
 * \brief WLAN transmit statistics.
 *
 * These statistics are gathered within the WLAN driver on the LHost.
 */
struct qdrv_wlan_tx_stats {
	/**
	 * The total number of management frames enqueued for transmission.
	 */
	unsigned int tx_enqueue_mgmt;

	/**
	 * The total number of driver-generated data frames enqueued for transmission.
	 */
	unsigned int tx_enqueue_80211_data;

	/**
	 * The total number of data packets enqueued for transmission.
	 */
	unsigned int tx_enqueue_data;

	/**
	 * The total number of packets enqueued to the MuC via the mailbox.
	 */
	unsigned int tx_muc_enqueue;

	/**
	 * The number of data packets enqueued to the MuC via the host mailbox (when no current
	 * packets are in the mailbox).
	 */
	unsigned int tx_muc_enqueue_mbox;

	/**
	 * The total number of keep-alive (NULL data) packets transmitted to clients
	 * associated to the AP. These packets are used to check the client is still
	 * connected and able to ACK the AP.
	 */
	unsigned int tx_null_data;

	/**
	 * The number of TX done interrupts received indicating the MuC is not ready.
	 * This figure should always read as zero.
	 */
	unsigned int tx_done_muc_ready_err;

	/**
	 * The number of packets successfully sent (all data, mgmt for all TIDs).
	 */
	unsigned int tx_done_success;

	/**
	 * The number of txdone interrupts received at the LHost.
	 */
	unsigned int tx_done_enable_queues;

	/**
	 * The number of times the transmit queue has stopped.
	 * Generally this is because the MuC backs up and causes backpressure to the
	 * LHost.
	 */
	unsigned int tx_queue_stop;

	/**
	 * The number of times a packet to the MuC was requeued.
	 */
	unsigned int tx_requeue;

	/**
	 * The number of times packet requeuing failed.
	 */
	unsigned int tx_requeue_err;

	/**
	 * The number of times the hardstart function is called.
	 */
	unsigned int tx_hardstart;

	/**
	 * The number of packets completed - marked as done by the MuC.
	 */
	unsigned int tx_complete;

	/**
	 * The size of the skb recycle list shared between the Ethernet and wireless drivers.
	 *
	 * This number will vary as traffic goes through the system.
	 */
	unsigned int tx_min_cl_cnt;

	/**
	 * The number of packets dropped in the driver during a configuration change.
	 */
	unsigned int tx_dropped_config;

	/**
	 * The number of packets dropped in the driver due to the MAC not being enabled.
	 */
	unsigned int tx_dropped_mac_dead;

	/**
	 * The current transmit channel.
	 */
	unsigned int tx_channel;

	/**
	 * The total number of IGMP packets for transmission.
	 */
	unsigned int tx_igmp;

	/**
	 * The number of packets for transmission to unknown destination MAC addresses.
	 */
	unsigned int tx_unknown;

	/**
	 * The number of ARP request packets sent in attempts to discover the location of
	 * unknown destinations.
	 */
	unsigned int tx_arp_req;

	/**
	 * The number of packets transmitted that lie inside the Local Network Control Block
	 * (LNCB), the range 224.0.0.0/24, sent as four address (reliable) multicast
	 * packets to Quantenna bridge stations.
	 */
	unsigned int tx_copy4_mc;

	/**
	 * The number of IGMP packets transmitted as 4 address reliable multicast packets
	 * to Quantenna bridge stations.
	 */
	unsigned int tx_copy4_igmp;

	/**
	 * The number of packets for unknown destination MAC addresses sent as 4 address
	 * reliable multicast packets to bridge stations.
	 */
	unsigned int tx_copy4_unknown;

	/**
	 * The total count of packet retransmissions as reliable, 4 address multicast frames.
	 */
	unsigned int tx_copy4;

	/**
	 * The number of times transmission of a copied packet failed due to lack of
	 * resources.
	 */
	unsigned int tx_copy_fail;

	/**
	 * The number of times transmission of a 4 address packet failed due to the tx
	 * queue being full.
	 */
	unsigned int tx_copy4_busy;

	/**
	 * The number of packets transmitted that lie inside the Local Network Control Block
	 * (LNCB), the range 224.0.0.0/24, sent as three address (unreliable) multicast
	 * packets (to third party clients).
	 */
	unsigned int tx_copy3_mc;

	/**
	 * The number of IGMP packets transmitted as 3 address unreliable multicast
	 * packets (to third party clients).
	 */
	unsigned int tx_copy3_igmp;

	/**
	 * The number of broadcast or multicast packets transmitted as unicast frames.
	 */
	unsigned int tx_copy_uc;

	/**
	 * The number of 3 address broadcast/multicast packets sent to third party STAs.
	 */
	unsigned int tx_copy3;

	/**
	 * The number of broadcast/multicast packets transmitted as group-addressed frames.
	 */
	unsigned int tx_copy_mc;

	/**
	 * The number of broadcast/multicast packets transmitted as directed frames
	 */
	unsigned int tx_copy_mc_to_uc;

	/**
	 * The number of SSDP packets transmitted as directed frames
	 */
	unsigned int tx_copy_ssdp;

	/**
	 * The number of packets that were dropped because the destination station was not
	 * authorised.
	 */
	unsigned int tx_drop_auth;

	/**
	 * The number of packets that were dropped because the destination station had
	 * disassociated.
	 */
	unsigned int tx_drop_aid;

	/**
	 * The number of packets that were dropped because of buffer exhaustion.
	 */
	unsigned int tx_drop_nodesc;

	/**
	 * The number of packets that were dropped because the WDS peer was not
	 * associated.
	 */
	unsigned int tx_drop_wds;

	/**
	 * The number of packets that were dropped because of 3 address mode bridging
	 * rules.
	 */
	unsigned int tx_drop_3addr;

	/**
	 * The number of packets that were dropped because of Video Stream Protection.
	 */
	unsigned int tx_drop_vsp;

	/**
	 * The total count of packets dropped at the wireless interface.
	 */
	unsigned int tx_drop_total;

	/**
	 * The number of data frames forworded to L2 external filter
	 */
	unsigned int tx_l2_ext_filter;

	/**
	 * The number of data frames droped without forwording to L2 external filter
	 */
	unsigned int tx_drop_l2_ext_filter;

	/**
	 * Field for QCAT.
	 */
	unsigned int qcat_state;

	/**
	 * Ticks that DSP waits until wmac is ready before installing the qmatrix.
	 */
	unsigned int txbf_qmat_wait;

	/**
	 * Protocol counts
	 */
	unsigned int prot_ip_udp;
	unsigned int prot_ip_tcp;
	unsigned int prot_ip_icmp;
	unsigned int prot_ip_igmp;
	unsigned int prot_ip_other;
	unsigned int prot_ipv6;
	unsigned int prot_arp;
	unsigned int prot_pae;
	unsigned int prot_other;
};

/**
 * \brief WLAN receive statistics.
 *
 * These statistics are gathered within the WLAN driver on the LHost.
 */
struct qdrv_wlan_rx_stats {

	/**
	 * The number of receive IRQs
	 */
	unsigned int rx_irq;

	/**
	 * The number of times the receive tasklet is scheduled based on the IRQ.
	 */
	unsigned int rx_irq_schedule;

	/**
	 * The number of beacons received.
	 */
	unsigned int rx_beacon;

	/**
	 * The number of non-beacon packets received (eg, other management, control
	 * and data packets combined).
	 */
	unsigned int rx_non_beacon;

	/**
	 * The number of packets received that were sent via the slow WLAN driver path,
	 * which have no node structure associated with them.
	 */
	unsigned int rx_input_all;

	/**
	 * The number of packets received for a specific node (slow WLAN driver path).
	 * The slow path is for management, control or fragmented data packets.
	 */
	unsigned int rx_input_node;

	/**
	 * The number of data packets received which are SNAP encapsulated.
	 */
	unsigned int rx_data_snap;

	/**
	 * The number of packets received with only the to DS bit set.
	 */
	unsigned int rx_data_tods;

	/**
	 * The number of packets received with none of the to/from DS bits set.
	 */
	unsigned int rx_data_nods;

	/**
	 * The number of packets received with only the from DS bit set.
	 */
	unsigned int rx_data_fromds;

	/**
	 * The number of packets received with both the to and from DS bits set.
	 * These are 4 address (bridged) packets.
	 */
	unsigned int rx_data_dstods;

	/**
	 * The number of packets received from unknown STAs - that is, the AP doesn't
	 * have an association with the STA.
	 */
	unsigned int rx_data_no_node;

	/**
	 * The number of packets received which have too short a length. These packets
	 * are dropped.
	 */
	unsigned int rx_data_too_short;

	/**
	 * The number of times the rx poll function is called.
	 */
	unsigned int rx_poll;

	/**
	 * The number of times that a poll is carried on from a previous poll - that is,
	 * the previous poll terminated early due to heavy RX load.
	 */
	unsigned int rx_poll_pending;

	/**
	 * The number of times rx poll terminated due to reaching the end of the received
	 * data chain.
	 */
	unsigned int rx_poll_empty;

	/**
	 * The number of times a poll to the receive mailbox has data available.
	 */
	unsigned int rx_poll_retrieving;

	/**
	 * The number of times that an AMSDU being decapsulated fails due to not having enough headroom
	 * in the packet. Eg, badly formatted AMSDU.
	 */
	unsigned int rx_poll_buffer_err;

	/**
	 * The number of times a receive descriptor allocate for an skb (when used for requeueing the RX descriptor
	 * for the next packet) fails.
	 */
	unsigned int rx_poll_skballoc_err;

	/**
	 * Whether the poll function for receive is currently running (1) or not (0).
	 */
	unsigned int rx_poll_stopped;

	/**
	 * The number of elements on the receive FIFO between the MuC and LHost.
	 */
	unsigned int rx_df_numelems;

	/**
	 * The number of Aggregate MSDUs received.
	 *
	 * This counter is incremented once per AMSDU, NOT once per subframe within
	 * the AMSDU.
	 */
	unsigned int rx_amsdu;

	/**
	 * The number of received packets (singletons, MPDUs or AMSDUs) in the LHost driver.
	 */
	unsigned int rx_packets;

	/**
	 * The number of received bytes (based on received packets counter above), including 802.2, 802.11 headers.
	 */
	unsigned int rx_bytes;

	/**
	 * The number of times chained receive descriptors are read in.
	 */
	unsigned int rx_poll_next;

	/**
	 * The number of times that the poll function completed processing all received packets before using
	 * it's entire budget.
	 */
	unsigned int rx_poll_complete;

	/**
	 * The number of times the receive poll function completes.
	 */
	unsigned int rx_poll_continue;

	/**
	 * The number of times packets received are from unauthenticated STAs.
	 */
	unsigned int rx_poll_vap_err;

	/**
	 * The number of received 802.11 fragmented packets.
	 * Fragmented packets are processed via the slow data path.
	 */
	unsigned int rx_frag;

	/**
	 * The number of packets received for STAs that are currently blacklisted (due to MAC address filtering).
	 */
	unsigned int rx_blacklist;

	/**
	 * The number of received LNCB packets in 4 address mode.
	 */
	unsigned int rx_lncb_4;

	/**
	 * The number of received IGMP packets.
	 */
	unsigned int rx_igmp;

	/**
	 * The number of received IGMP packets in 4 address mode.
	 */
	unsigned int rx_igmp_4;

	/**
	 * The number of IGMP packets dropped due to already receiving the IGMP packet
	 * as a reliable 4 address packet.
	 */
	unsigned int rx_igmp_3_drop;

	/**
	 * The number of received 3 address multicast packets dropped due to already
	 * receiving the same packet as a reliable 4 address packet.
	 */
	unsigned int rx_mc_3_drop;

	/**
	 * Protocol counts
	 */
	unsigned int prot_ip_udp;
	unsigned int prot_ip_tcp;
	unsigned int prot_ip_icmp;
	unsigned int prot_ip_igmp;
	unsigned int prot_ip_other;
	unsigned int prot_ipv6;
	unsigned int prot_arp;
	unsigned int prot_pae;
	unsigned int prot_other;

	/**
	 * Beamforming Statistics
	 */
	unsigned int rx_bf_success[QTN_STATS_NUM_BF_SLOTS];
	unsigned int rx_bf_rejected[QTN_STATS_NUM_BF_SLOTS];

	unsigned int rx_rate_train_invalid;
	unsigned int rx_mac_reserved;
	unsigned int rx_coex_bw_action;
	unsigned int rx_coex_bw_assoc;
	unsigned int rx_coex_bw_scan;
};

struct qdrv_tqe_cgq_stats {
	uint32_t	congest_qlen[TOPAZ_CONGEST_QUEUE_NUM];
	uint32_t	congest_enq_fail[TOPAZ_CONGEST_QUEUE_NUM];
};
/* @} */

/*
 * This can be changed to an array if the stat_parser is enhanced to parse array syntax.
 */
struct qdrv_rx_evm_array {
	unsigned int rx_evm_val[NUM_ANT];
};

/**
 * \brief Transmit power
 *
 * Each member of the array records transmit power of one Tx chain.
 */
struct qdrv_tx_pd_array {
	/**
	 * Transmit power of chain 0-3.
	 */
	uint16_t tx_pd_vol[NUM_ANT];
};

/**
 * \brief Qdisc stats
 *
 * Queue statistics per-node
 */
struct qdrv_netdebug_nd_stats {
	uint32_t	sch_aid;
	uint32_t	sch_mac1;
	uint32_t	sch_mac2;
	uint32_t	sch_ref;
	uint32_t	sch_muc_queued;
	uint32_t	sch_tokens;
	uint32_t	sch_qlen;
	uint32_t	sch_low_rate;
	uint32_t	sch_depth[QDRV_SCH_BANDS];
	uint32_t	sch_sent[QDRV_SCH_BANDS];
	uint32_t	sch_dropped[QDRV_SCH_BANDS];
	uint32_t	sch_victim[QDRV_SCH_BANDS];
};

struct qdrv_sch_stats {
	uint32_t	sch_users;
	uint32_t	sch_tokens;
	uint32_t	sch_cnt;
};

/**
 * \brief Linux memory statistics.
 *
 * This structure contains a sample of different statistics related to the Linux memory
 * management subsystem.
 */
struct qdrv_mem_stats {
	/**
	 * The number of free pages in the system.
	 */
	unsigned long mem_free;
	/**
	 * The number of SLAB pages that can be freed up.
	 */
	unsigned long mem_slab_reclaimable;
	/**
	 * The number of SLAB pages that can't be freed up.
	 */
	unsigned long mem_slab_unreclaimable;
	/**
	 *
	 */
	unsigned long mem_anon;
	unsigned long mem_mapped;
	unsigned long mem_cached;
};

/**
 * \brief Linux misc statistics
 */
struct qdrv_misc_stats {
	/**
	 * CPU awake cycles. When CPU is at full load, this will be at
	 * CPU clock speed (Hz) / stats interval (s)
	 */
	unsigned long cpuawake;
};

/**
 * \brief Statistics indicates the reason for a channel change
 *
 * Each member of this structure records times channel change caused by different reason.
 */
struct qdrv_csw_count_stats {
	/**
	 * Channel change caused by SCS.
	 */
	uint16_t csw_by_scs;
	/**
	 * Channel change caused by DFS.
	 */
	uint16_t csw_by_dfs;
	/**
	 * Channel change caused by User configuration
	 */
	uint16_t csw_by_user;
	/**
	 * Channel change when device does off-channel sampling.
	 */
	uint16_t csw_by_sampling;
	/**
	 * Channel change when device does scan and sample.
	 */
	uint16_t csw_by_tdls;
	/**
	 * Channel change when device does background scanning.
	 */
	uint16_t csw_by_bgscan;
	/**
	 * Channel change after off-channel CAC is completed.
	 */
	uint16_t csw_by_ocac;
	/**
	 * Channel change when off-channel CAC is running.
	 */
	uint16_t csw_by_ocac_run;
	/**
	 * Channel change when received CSAIE from action frame or beacon
	 */
	uint16_t csw_by_csa;
	/**
	 * Channel change when device does regular scanning.
	 */
	uint16_t csw_by_scan;
};

struct tx_power_cal
{
	struct _temp_info {
		int temp_index;
		int real_temp;
		u_int8_t num_zone;
	} temp_info;
	struct delayed_work bbrf_cal_work;
};

#define MAX_UNKNOWN_DP_PER_SECOND 5   /* Rate limit per sec for unknown data pkts */

/* Ext Flags in qdrv_wlan */
#define QDRV_WLAN_MUC_KILLED		0x00000001
#define QDRV_FLAG_3ADDR_BRIDGE_DISABLE	0x00000002
#define QDRV_WLAN_DEBUG_TEST_LNCB	0x00000004
#define QDRV_WLAN_FLAG_UNKNOWN_ARP	0x00000008 /* send ARP requests for unknown destinations */
#define QDRV_WLAN_FLAG_UNKNOWN_FWD	0x00000010 /* send unknown dest pkt to all bridge STAs */
#define QDRV_WLAN_FLAG_AUC_TX		0x00000020 /* enqueue tx packets to AuC, not MuC */

#define QDRV_WLAN_TX_USE_AUC(qw)	( qw->flags_ext & QDRV_WLAN_FLAG_AUC_TX )

#define QDRV_FLAG_3ADDR_BRIDGE_ENABLED() ((qw->flags_ext & QDRV_FLAG_3ADDR_BRIDGE_DISABLE) == 0)

struct qdrv_wlan {
	/* The 802.11 networking structure */
	struct ieee80211com ic;
	int unit;

	struct work_struct scan_task;

	u32 flags_ext;
	u16 flags;
	u8 rf_chipid;
	u8 rf_chip_verid;
	struct qdrv_mac *mac;/* Interrupts per MAC so we need a back pointer */

	/* Synchronization */
	spinlock_t lock;

	char semmap[HOST_NUM_HOSTIFQ];
	char txdoneirq;
	int rxirq;
	int scanirq;

	struct host_scanif scan_if;

	struct host_scanfifo *scan_fifo; /* For iounmap */

	/* Tx to MuC */
	struct host_txif tx_if;
	/* Rx from MuC */
	struct host_rxif rx_if;

	/* Registers */
	u32 host_sem;
	struct qtn_ratetable qw_rates[IEEE80211_MODE_MAX];/* rate tables */
	struct qtn_ratetable *qw_currt;      /* current rate table */
	enum ieee80211_phymode qw_curmode;
	struct qdrv_wlan_tx_stats tx_stats;
	struct qdrv_wlan_rx_stats rx_stats;
	struct qdrv_csw_count_stats csw_stats;
	struct qdrv_pktlogger pktlogger;
	struct qdrv_wlan_sm_stats sm_stats;

	/*congest queue stats*/
	struct qdrv_tqe_cgq_stats cgq_stats;

	/* TX Beamforming support */
	void *txbf_state;

	/* Flow control */
	spinlock_t flowlock;

	struct net_device *br_dev;

	int unknown_dp_count;
	unsigned long unknown_dp_jiffies;

#ifdef CONFIG_QVSP
	struct qvsp_s *qvsp;
	struct qtn_vsp_stats *vsp_stats;
	struct tasklet_struct vsp_tasklet;
	struct timer_list vsp_ba_throt;
#if TOPAZ_QTM
	uint32_t vsp_enabling;		/* VSP is just enabled, need warm up for traffic stats */
	uint32_t vsp_check_intvl;	/* in seconds */
	uint32_t vsp_sync_sched_remain;	/* sched sync task before next vsp interrupt from MuC */
	/*
	 * Used to sync stream stats every second. Since VSP check interval is bigger than 1 second,
	 * we need to sched the sync work one less than the interval. And vsp tasklet will do 1 time.
	 */
	struct delayed_work vsp_sync_work;
#endif
#endif

	/* 3-address mode bridging */
	struct qdrv_br bridge_table;
	int mcs_cap;
	int mcs_odd_even;
	int tx_restrict;
	int tx_restrict_rts;
	int tx_restrict_limit;
	int tx_restrict_rate;
	uint8_t tx_swretry_agg_max;
	uint8_t tx_swretry_noagg_max;
	uint8_t tx_swretry_suspend_xmit;
	struct timer_list hr_timer;
	struct timer_list cca_timer;
	struct timer_list igmp_query_timer;
	struct work_struct cca_wq;
	struct work_struct meas_wq;
	spinlock_t cca_lock;
	struct work_struct scan_wq;
	spinlock_t scan_lock;
	void (*csa_callback)(const struct ieee80211_channel *, u_int64_t);
	struct work_struct csa_wq;
	spinlock_t csa_lock;
	struct work_struct channel_work_wq;
	int (*radar_detect_callback)(const struct ieee80211_channel *);
	unsigned long arp_last_sent;

	struct work_struct remain_chan_wq;

	/* MuC per node stats pool */
	struct qtn_node_shared_stats		*shared_pernode_stats_pool;
	dma_addr_t				shared_pernode_stats_phys;
	TAILQ_HEAD(, qtn_node_shared_stats)	shared_pernode_stats_head;

	struct notifier_block pm_notifier;

#if QTN_GENPCAP
	struct qtn_genpcap_args genpcap_args;
#endif

	struct qdrv_sch_shared_data	*tx_sch_shared_data;
	bool				queue_enabled;
#define QDRV_BR_ISOLATE_NORMAL		BIT(0)
#define QDRV_BR_ISOLATE_VLAN		BIT(1)
	uint16_t br_isolate;
	uint16_t br_isolate_vid;
	uint8_t restrict_wlan_ip;
	struct i2c_client *se95_temp_sensor;
	struct tx_power_cal tx_power_cal_data;
	struct shared_params *sp;
};

/**************/
/** Netdebug **/
/**************/
#define	QDRV_NETDEBUG_NETPOLL_NAME		"qdrv_netpoll"
#define	QDRV_NETDEBUG_NETPOLL_DEV		"eth1_0"

#define	QDRV_NETDEBUG_FLAGS_NO_STATS		0x1
#define	QDRV_NETDEBUG_FLAGS_TRUNCATED		0x2

#define QDRV_NETDEBUG_RADAR_MAXPULSE		(175)
#define QDRV_NETDEBUG_RADAR_PULSESIZE		(8)

#define QDRV_NETDEBUG_TXBF_DATALEN		1024
#define QDRV_NETDEBUG_MEM_DATALEN		1024
#define QDRV_NETDEBUG_SYSMSG_LENGTH		4096
#define QDRV_NETDEBUG_BUILDSTRING_SIZE		32

/**
 * \brief The common header for netdebug (packetlogger) packets.
 */
struct qdrv_pktlogger_hdr {
	struct udphdr udpheader;
	u_int8_t type;
	u_int8_t			opmode;
	/**
	 * The source address (the bridge MAC address).
	 */
	unsigned char			src[IEEE80211_ADDR_LEN];
	u_int32_t			version;
	u_int32_t			builddate;
	/**
	 * Identifying string to easily see in packet dumps that this is a packetlogger packet.
	 */
	char				buildstring[QDRV_NETDEBUG_BUILDSTRING_SIZE];
	u_int8_t			flags;

	/**
	 * Epoch timestamp.
	 */
	u_int32_t			timestamp;
	/**
	 * TSF timestamp low bytes.
	 */
	u_int32_t			tsf_lo;
	/**
	 * TSF timestamp high bytes.
	 */
	u_int32_t			tsf_hi;

	u_int32_t			platform;
	u_int32_t			stats_len;
	char				padding[3];	/* Word align data start */
} __packed;

struct qdrv_netdebug_txbf {
	struct qdrv_pktlogger_hdr	ndb_hdr;
	u_int8_t			stvec_data[QDRV_NETDEBUG_TXBF_DATALEN];
} __packed;

struct qdrv_netdebug_mem {
	struct qdrv_pktlogger_hdr	ndb_hdr;
	u_int8_t			stvec_data[QDRV_NETDEBUG_MEM_DATALEN];
} __packed;

struct qdrv_netdebug_rate {
	struct qdrv_pktlogger_hdr	ndb_hdr;
	struct qtn_rate_su_tx_stats	rate_su_tx_stats[RATES_STATS_NUM_ADAPTATIONS];
	struct qtn_rate_mu_tx_stats	rate_mu_tx_stats[RATES_STATS_NUM_ADAPTATIONS];
	struct qtn_rate_gen_stats	rate_gen_stats;
} __packed;

struct qdrv_radar_stats {
	struct qdrv_pktlogger_hdr	ndb_hdr;
	u_int32_t			numpulses;
	u_int8_t			pulseinfo[QDRV_NETDEBUG_RADAR_PULSESIZE *
						  QDRV_NETDEBUG_RADAR_MAXPULSE];
} __packed;

struct qdrv_muc_rx_rates {
	u_int16_t			rx_mcs[IEEE80211_HT_RATE_MAXSIZE];
	u_int16_t			rx_mcs_pad; /* unique name for packet logger */
} __packed;

struct qdrv_muc_rx_11acrates {
	u_int16_t			rx_11ac_mcs[MUC_VHT_NUM_RATES];
} __packed;

struct qdrv_netdebug_sysmsg {
	struct qdrv_pktlogger_hdr ndb_hdr;
	char msg[QDRV_NETDEBUG_SYSMSG_LENGTH];
} __packed;

/**
 * \brief Statistics on the traffic queueing discipline (qdisc).
 *
 * These statistics are used to track packets that are sent/dropped by the traffic
 * policer on the Ethernet and wireless interfaces.
 *
 * The '_dropped' counters represent true packet loss, due to backpressure from lower
 * parts of the system.
 */
struct qdrv_qdisc_stats {
	/**
	 * The number of packets queued via the qdisc for the wireless interface.
	 */
	u_int32_t wifi_sent;
	/**
	 * The number of packets dropped by the qdisc on the wireless interface.
	 */
	u_int32_t wifi_dropped;
	/**
	 * The number of packets queued via the qdisc for the Ethernet interface.
	 */
	u_int32_t eth_sent;
	/**
	 * The number of packets dropped by the qdisc on the Ethernet interface.
	 */
	u_int32_t eth_dropped;
};

/**
 * \brief Statistics related to the EMAC.
 *
 * This structure contains statistic related to the EMAC block of the system.
 */
struct qdrv_emac_stats {
	/**
	 * The number of packets lost due to no DMA buffers being available on
	 * receive. Each of these represents a genuine single packet loss on
	 * Ethernet receive.
	 */
	u_int32_t rx_emac0_dma_missed;
	u_int32_t rx_emac1_dma_missed;
};

/* This struct must be kept in sync with qtn_scs_cnt*/
struct qdrv_scs_cnt {
	uint32_t scs_iotcl;
	uint32_t scs_noqosnull;
	uint32_t scs_1stcnflct;
	uint32_t scs_qosnul_sntfail;
	uint32_t scs_2ndcnflct;
	uint32_t scs_low_dwell;
	uint32_t scs_offch_scan;
	uint32_t scs_sample_start;
	uint32_t scs_sample_stop;
};

struct qdrv_tqe_stats {
	uint32_t emac0_outc;
	uint32_t emac1_outc;
	uint32_t wmac_outc;
	uint32_t lhost_outc;
	uint32_t muc_outc;
	uint32_t dsp_outc;
	uint32_t auc_outc;
	uint32_t pcie_outc;
	uint32_t drop;
	uint32_t emac0_drop;
	uint32_t emac1_drop;
	uint32_t wmac_drop;
	uint32_t lhost_drop;
	uint32_t muc_drop;
	uint32_t dsp_drop;
	uint32_t auc_drop;
	uint32_t pcie_drop;
};

struct qdrv_hbm_stats {
	uint32_t req_lhost[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_muc[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_emac0[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_emac1[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_wmac[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_tqe[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_auc[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_dsp[TOPAZ_HBM_POOL_COUNT];
	uint32_t req_pcie[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_lhost[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_muc[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_emac0[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_emac1[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_wmac[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_tqe[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_auc[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_dsp[TOPAZ_HBM_POOL_COUNT];
	uint32_t rel_pcie[TOPAZ_HBM_POOL_COUNT];
};

struct qdrv_hbm_stats_oth {
	uint32_t hbm_req;
	uint32_t hbm_rel;
	uint32_t hbm_diff;
	uint32_t hbm_overflow;
	uint32_t hbm_underflow;
};

struct dsp_mu_stats {
	uint32_t mu_u0_aid[QTN_MU_QMAT_MAX_SLOTS];
	uint32_t mu_u1_aid[QTN_MU_QMAT_MAX_SLOTS];
	int32_t  mu_rank[QTN_MU_QMAT_MAX_SLOTS];
};

struct qdrv_netdebug_stats {
	struct qdrv_pktlogger_hdr	ndb_hdr;
	struct muc_rx_stats		stats_muc_rx;
	struct qdrv_muc_rx_rates	rates_muc_rx;
	struct qdrv_muc_rx_11acrates	rates_muc_rx_11ac;
	struct muc_rx_bf_stats		stats_muc_rx_bf;
	struct muc_tx_stats		stats_muc_tx;
	struct qdrv_emac_stats		stats_emac;
	struct qdrv_qdisc_stats		stats_qdisc;

	struct qdrv_wlan_rx_stats	stats_wlan_rx;
	struct qdrv_wlan_tx_stats	stats_wlan_tx;
	struct qdrv_wlan_sm_stats	stats_wlan_sm;

	struct qtn_rx_stats		stats_phy_rx;
	struct qtn_tx_stats		stats_phy_tx;
	struct qdrv_mem_stats		stats_mem;
	struct qdrv_misc_stats		stats_misc;
	struct qdrv_rx_evm_array	stats_evm;
	struct qdrv_csw_count_stats	stats_csw;
	struct qdrv_tx_pd_array		stats_pd_vol;
	struct qdrv_slab_watch		stats_slab;
	struct qdrv_scs_cnt		stats_scs_cnt;
	struct qdrv_auc_intr_stats	stats_auc_intr_count;
	struct auc_dbg_counters		stats_auc_debug_counts;
	struct qdrv_tqe_stats		stats_tqe;
	struct qdrv_tqe_cgq_stats	stats_cgq;
	struct qdrv_hbm_stats		stats_hbm;
	struct qdrv_hbm_stats_oth	stats_hbm_oth;
	struct dsp_mu_stats		stats_dsp_mu;
} __packed;

/* TBD */
#define QDRV_NETDEBUG_EVENT_STR_MAX	127
struct qdrv_netdebug_event {
	u_int8_t 			version;
	u_int8_t 			type;
	u_int8_t 			reserved[2];		/* Reserved for alignment */
	u_int32_t 			tstamp;
	u_int8_t 			event_msg[QDRV_NETDEBUG_EVENT_STR_MAX + 1];
};

struct qdrv_netdebug_per_node_phystats {
	uint8_t node_macaddr[IEEE80211_ADDR_LEN];
	struct qtn_node_shared_stats per_node_phystats;
} __packed;

/*
 * We always have at least one per-node statistic (the board itself)
 * For APs we can have more than one, so added variable length array at the end of structure
 */
struct qdrv_netdebug_phystats {
	struct qdrv_pktlogger_hdr		ndb_hdr;
	struct qtn_stats			stats;
	u_int32_t				per_node_stats_count;
	struct qdrv_netdebug_per_node_phystats	per_node_stats[1];
} __packed;


extern int g_triggers_on;

int topaz_read_internal_temp_sens(int *temp);
void qdrv_halt_muc(void);
int get_temp_zone_from_tprofile(void);
int convert_temp_index(int temp);

/* Support wlan modules */
int qdrv_rx_poll(struct napi_struct *napi, int budget);
int qdrv_rx_start(struct qdrv_mac *mac);
int qdrv_rx_stop(struct qdrv_mac *mac);
int qdrv_rx_init(struct qdrv_wlan *qw, struct host_ioctl_hifinfo *hifinfo);
int qdrv_rx_exit(struct qdrv_wlan *qw);

void qdrv_tx_done_flush_vap(struct qdrv_vap *qv);
int qdrv_tx_hardstart(struct sk_buff *skb, struct net_device *dev);
struct host_txdesc * qdrv_tx_get_mgt_txdesc(struct sk_buff *skb, struct net_device *dev);
void qdrv_tx_release_txdesc(struct qdrv_wlan *qw, struct lhost_txdesc* txdesc);
int qdrv_tx_start(struct qdrv_mac *mac);
int qdrv_tx_stop(struct qdrv_mac *mac);
int qdrv_tx_eth_pause_init(struct qdrv_wlan *qw);
int qdrv_tx_init(struct qdrv_mac *mac, struct host_ioctl_hifinfo *hifinfo,
	u32 arg2);
int qdrv_tx_exit(struct qdrv_wlan *qw);
void qdrv_tx_ba_establish(struct qdrv_vap *qv,
		struct ieee80211_node *ni, uint8_t tid);

int qdrv_scan_start(struct qdrv_mac *mac);
int qdrv_scan_stop(struct qdrv_mac *mac);
int qdrv_scan_init(struct qdrv_wlan *qw, struct host_ioctl_hifinfo *hifinfo);
int qdrv_scan_exit(struct qdrv_wlan *qw);
int qtn_async_cca_read(struct ieee80211com *ic, const struct ieee80211_channel *sample_channel,
		u_int64_t start_tsf, u_int32_t sample_millis);

int qdrv_ap_isolate_filter(struct ieee80211_node *ni, struct sk_buff *skb);

int qdrv_hostlink_msg_cmd(struct qdrv_wlan *qw, u_int32_t cmd, u_int32_t arg);
int qdrv_hostlink_msg_create_vap(struct qdrv_wlan *qw, const char *name, const uint8_t *mac_addr,
		int devid, int opmode, int flags);
int qdrv_hostlink_msg_delete_vap(struct qdrv_wlan *qw, struct net_device *vdev);
int qdrv_hostlink_start(struct qdrv_mac *mac);
int qdrv_hostlink_stop(struct qdrv_mac *mac);
int qdrv_hostlink_init(struct qdrv_wlan *qw,
	struct host_ioctl_hifinfo *hifinfo);
int qdrv_hostlink_exit(struct qdrv_wlan *qw);
int qdrv_hostlink_store_txpow(struct qdrv_wlan *qw, u_int32_t txpower);
int qdrv_hostlink_setchan(struct qdrv_wlan *qw, uint32_t freq_band, uint32_t qtn_chan);
int qdrv_hostlink_sample_chan(struct qdrv_wlan *qw, struct qtn_samp_chan_info *);
int qdrv_hostlink_remain_chan(struct qdrv_wlan *qw, struct qtn_remain_chan_info *remain_chan_bus);
int qdrv_hostlink_set_ocac(struct qdrv_wlan *qw, struct qtn_ocac_info *);
int qdrv_hostlink_suspend_off_chan(struct qdrv_wlan *qw, uint32_t suspend);
int qdrv_hostlink_meas_chan(struct qdrv_wlan *qw, struct qtn_meas_chan_info *meas_chan_bus);
int qdrv_hostlink_rxgain_params(struct qdrv_wlan *qw, uint32_t index, struct qtn_rf_rxgain_params *rx_gain_params);
#ifdef QTN_BG_SCAN
int qdrv_hostlink_bgscan_chan(struct qdrv_wlan *qw, struct qtn_scan_chan_info *);
#endif /* QTN_BG_SCAN */
int qdrv_hostlink_setchan_deferred(struct qdrv_wlan *qw, struct qtn_csa_info*);
int qdrv_hostlink_setscanmode(struct qdrv_wlan *qw, u_int32_t scanmode);
int qdrv_hostlink_xmitctl(struct qdrv_wlan *qw, bool enable_xmit);
int qdrv_hostlink_msg_calcmd(struct qdrv_wlan *qw, int cmdlen, dma_addr_t cmd_dma);
//int qdrv_hostlink_do_txpwr_cal(struct qdrv_wlan *qw, int temp_idx, int pwr);
int qdrv_hostlink_msg_set_wifi_macaddr( struct qdrv_wlan *qw, u8 *new_macaddr );
int qdrv_hlink_get_outstand_msgs(struct qdrv_wlan *qw);
int qdrv_hostlink_set_hrflags(struct qdrv_wlan *qw, u_int32_t flags);
int qdrv_hostlink_power_save(struct qdrv_wlan *qw, int param, int val);
int qdrv_hostlink_tx_airtime_control(struct qdrv_wlan *qw, uint32_t value);
int qdrv_hostlink_mu_group_update(struct qdrv_wlan *qw, struct qtn_mu_group_update_args *args);
void* qdrv_hostlink_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag);
void qdrv_hostlink_free_coherent(struct device *dev, size_t size, void *kvaddr, dma_addr_t dma_handle);
extern u_int8_t get_bootcfg_scancnt(void);
uint8_t get_bootcfg_two_by_four_configuration(void);
enum hw_opt_t get_bootcfg_bond_opt(void);

#ifdef CONFIG_QVSP
int qdrv_hostlink_qvsp(struct qdrv_wlan *qw, uint32_t param, uint32_t value);
int qdrv_wlan_query_wds(struct ieee80211com *ic);
#endif

int qdrv_hostlink_killmuc(struct qdrv_wlan *qw);
int qdrv_hostlink_use_rtscts(struct qdrv_wlan *qw, int rtscts_required);
int qdrv_hostlink_send_ioctl_args(struct qdrv_wlan *qw, uint32_t command,
	uint32_t arg1, uint32_t arg2);

void qdrv_dump_replace_db(struct qdrv_wlan *qw);
int qdrv_hostlink_enable_flush_data(struct qdrv_wlan *qw, int enable);

/* Main wlan module */
#if 0
int qdrv_wlan_start(void *data, char *name);
#endif
int qdrv_wlan_stats(void *data);
int qdrv_wlan_start_vap(struct qdrv_wlan *qw, const char *name,
	uint8_t *mac_addr, int devid, int opmode, int flags);
int qdrv_wlan_stop_vap(struct qdrv_mac *mac, struct net_device *vdev);
int qdrv_wlan_init(struct qdrv_mac *mac, struct host_ioctl_hifinfo *hifinfo,
	u32 arg1, u32 arg2);
int qdrv_wlan_exit(struct qdrv_mac *mac);
int qdrv_wlan_get_assoc_queue_info(void *data);
int qdrv_wlan_get_assoc_info(void *data);
int qdrv_wps_button_init(struct net_device *dev);
void qdrv_wps_button_exit(void);
struct net_device *qdrv_wps_button_get_dev(void);
int qdrv_wlan_tkip_mic_error(struct qdrv_mac *mac, int devid, int count);
int qdrv_wlan_ba_is_ok(struct ieee80211_node *ni, int tid, int direction);
void qdrv_wlan_igmp_query_timer_start(struct qdrv_wlan *qw);
void qdrv_wlan_igmp_timer_stop(struct qdrv_wlan *qw);
void qdrv_wlan_drop_ba(struct ieee80211_node *ni, int tid, int tx, int reason);
void qdrv_wlan_cleanup_before_reload(struct ieee80211com *ic);
int qdrv_get_br_ipaddr(struct qdrv_wlan *qw, __be32 *ipaddr);
int qdrv_is_bridge_ipaddr(struct qdrv_wlan *qw, __be32 ipaddr);
void qdrv_wlan_dump_ba(struct ieee80211_node *ni);
void qdrv_wlan_stats_prot(struct qdrv_wlan *qw, uint8_t is_tx, uint16_t ether_type,
				uint8_t ip_proto);
int qdrv_proxy_arp(struct ieee80211vap *iv,
		struct qdrv_wlan *qw,
		struct ieee80211_node *ni_rx,
		uint8_t *data_start);
#ifdef CONFIG_IPV6
int qdrv_wlan_handle_neigh_msg(struct ieee80211vap *vap, struct qdrv_wlan *qw,
			uint8_t *data_start, uint8_t in_tx,  struct sk_buff *skb,
			uint8_t ip_proto, void *proto_data);
#endif
#ifdef CONFIG_QVSP
int qdrv_wlan_vsp_3rdpt_init(struct qdrv_wlan *qw);
void qdrv_wlan_vsp_3rdpt_exit(struct qdrv_wlan *qw);
#endif

void qdrv_wlan_vlan_enable(struct ieee80211com *ic, int enable);
int qdrv_hostlink_vlan_enable(struct qdrv_wlan *qw, int enable);

void qdrv_tx_airtime_control(struct ieee80211vap *vap, uint32_t vaule);

void qdrv_mu_grp_update(struct ieee80211com *ic, struct qtn_mu_group_update_args *args);

static inline struct qdrv_wlan *qdrv_mac_get_wlan(struct qdrv_mac *mac)
{
	return (struct qdrv_wlan*)mac->data;
}

static inline int
qdrv_wlan_bc_should_forward(const struct ether_header *eh,
			struct ieee80211vap *iv)
{
	if (ieee80211_is_bcst(eh->ether_dhost) && iv->iv_reliable_bcst) {
		return 1;
	}

	return 0;
}

static inline int
qdrv_wlan_mc_should_drop(const struct ether_header *eh, void *p_iphdr,
			struct ieee80211vap *iv, bool is_vap_node, uint8_t ip_proto)
{
	if (IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
			is_vap_node &&
			ip_proto != IPPROTO_ICMPV6 &&
			ip_proto != IPPROTO_IGMP &&
			!iv->iv_forward_unknown_mc &&
			iputil_is_mc_data(eh, p_iphdr)) {
		return 1;
	}

	return 0;
}

static inline int
qdrv_wlan_mc_should_forward(const struct ether_header *eh, void *p_iphdr,
			struct ieee80211vap *iv, bool is_vap_node)
{
	if (iputil_eth_is_multicast(eh) &&
			is_vap_node &&
			iv->iv_forward_unknown_mc) {
		return 1;
	}

	return 0;
}

static inline int qdrv_wlan_is_4addr_mc(const struct ether_header *eh, u_int8_t *data_start,
		struct ieee80211vap *iv, bool is_vap_node)
{
	if (unlikely(IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
		(iputil_is_lncb(eh->ether_dhost, data_start) ||
			qdrv_wlan_mc_should_forward(eh, data_start, iv, is_vap_node) ||
			qdrv_wlan_bc_should_forward(eh, iv)))) {
		return 1;
	}

	return 0;
}

#define IGMP_TYPE_QUERY 0x11
#define IGMP_TYPE_MEMBERSHIP_REPORT1 0x12
#define IGMP_TYPE_MEMBERSHIP_REPORT2 0x16
#define IGMP_TYPE_LEAVE_GROUP 0x17
#define IGMP_TYPE_MEMBERSHIP_REPORT3 0x22

static inline const char *qdrv_igmp_type_to_string(int igmp_type)
{
	switch(igmp_type) {
		case IGMP_TYPE_QUERY:
			return "Query";
		case IGMP_TYPE_MEMBERSHIP_REPORT1:
			return "Membership Report (v1)";
		case IGMP_TYPE_MEMBERSHIP_REPORT2:
			return "Membership Report (v2)";
		case IGMP_TYPE_MEMBERSHIP_REPORT3:
			return "Membership Report (v3)";
		case IGMP_TYPE_LEAVE_GROUP:
			return "Leave Group";
		//default:
			/* Fall through */
	}
	return "Unknown";
}

static inline int qdrv_igmp_type(struct iphdr *p_iphdr, int len)
{
	/* Incomplete, but enough for the field we're interested in. */
	struct igmphdr {
		u_int8_t type;
	};
	if (len > (sizeof(*p_iphdr) + sizeof(struct igmphdr))) {
		if (p_iphdr->protocol == IPPROTO_IGMP) {
			/* Size of IP header is in 4 byte words */
			int hlen = p_iphdr->ihl * (sizeof(u_int32_t));
			struct igmphdr *p_igmp = (struct igmphdr *)((u_int8_t *)p_iphdr + hlen);
			return p_igmp->type;
		}
	}
	return 0;
}

/* Is this an IGMP query? */
static inline int qdrv_is_igmp_query(struct iphdr *p_iphdr, int len)
{
	if (qdrv_igmp_type(p_iphdr, len) == IGMP_TYPE_QUERY) {
		return 1;
	}
	return 0;
}

void qdrv_channel_switch_record(struct ieee80211com *ic, struct ieee80211_channel *new_chan,
		uint32_t reason);
void qdrv_channel_switch_reason_record(struct ieee80211com *ic, int reason);
int8_t qdrv_get_local_tx_power(struct ieee80211com *ic);
int qdrv_get_local_link_margin(struct ieee80211_node *ni, int8_t *result);
int qdrv_wlan_get_shared_vap_stats(struct ieee80211vap *vap);
int qdrv_wlan_reset_shared_vap_stats(struct ieee80211vap *vap);
int qdrv_wlan_get_shared_node_stats(struct ieee80211_node *ni);
int qdrv_wlan_reset_shared_node_stats(struct ieee80211_node *ni);
int qdrv_rxgain_params(struct ieee80211com *ic, int index, struct qtn_rf_rxgain_params *params);
void qdrv_wlan_get_dscp2ac_map(const uint8_t vapid, uint8_t *dscp2ac);
void qdrv_wlan_set_dscp2ac_map(const uint8_t vapid, uint8_t *ip_dscp, uint8_t listlen, uint8_t ac);
void wowlan_wakeup_host(void);
/*
 * Delete all bridge table entries for the peer.  They would eventually
 * age out, but in the mean time data will be directed to the wrong
 * sub_port (node_idx) until the bridge entries are updated by upstream
 * traffic from the endpoint. Multicast port entries for the sub_port
 *  are not aged and would hang around for ever, so they are also deleted.
 */
static inline void qdrv_remove_invalid_sub_port(struct ieee80211vap *vap,
		uint32_t sub_port)
{
	DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_BRIDGE, "Purge subport[0x%x]\n", sub_port);
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	if (br_fdb_delete_by_sub_port_hook && vap->iv_dev->br_port) {
		br_fdb_delete_by_sub_port_hook(vap->iv_dev->br_port->br,
				vap->iv_dev->br_port, sub_port);
	}
#endif
}

int qtn_tsensor_get_temperature(struct i2c_client *client, int *val);

void qdrv_tqe_send_l2_ext_filter(struct qdrv_wlan *qw, struct sk_buff *skb);

/*
 * Called only if packet received has destination address as broadcast.
 */
static inline int check_if_exceeds_max_bcast_pps(struct bcast_pps_info *bcast_pps, int is_rx)
{
	u_int16_t *bcast_counter;
	unsigned long *start_time;
	if (is_rx) {
		bcast_counter = &bcast_pps->rx_bcast_counter;
		start_time = &bcast_pps->rx_bcast_pps_start_time;
	} else {
		bcast_counter = &bcast_pps->tx_bcast_counter;
		start_time = &bcast_pps->tx_bcast_pps_start_time;
	}
	if (time_after(jiffies, ((*start_time) + HZ))) {
		*start_time = jiffies;
		*bcast_counter = 0;
	}
	if (*bcast_counter >= bcast_pps->max_bcast_pps) {
		return 1;
	}
	(*bcast_counter)++;
	return 0;
}

static inline int check_is_bcast_pps_exception(uint16_t ether_type, uint8_t ip_proto,
			void *proto_data)
{
	if (ether_type == __constant_htons(ETH_P_ARP)) {
		return 1;
	}

	if (ether_type == __constant_htons(ETH_P_IP) &&
			proto_data &&
			ip_proto == IPPROTO_UDP) {
		struct udphdr *udph = proto_data;

		if ((udph->dest == __constant_htons(DHCPSERVER_PORT)) ||
				udph->dest == __constant_htons(DHCPCLIENT_PORT)) {
			return 1;
		}
	}
	return 0;
}

static inline int bcast_pps_should_drop(const u8 *dst, struct bcast_pps_info *bcast_pps,
		uint16_t eth_type, uint8_t ip_proto, void *proto_data, int is_rx)
{
	if ((ieee80211_is_bcst(dst))
			&& (bcast_pps->max_bcast_pps)
			&& (!check_is_bcast_pps_exception(eth_type, ip_proto, proto_data))
			&& (check_if_exceeds_max_bcast_pps(bcast_pps, is_rx))) {
		return 1;
	}
	return 0;
}

#endif

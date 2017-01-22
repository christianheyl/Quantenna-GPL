/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2012 Quantenna Communications, Inc.                 **
**                            All Rights Reserved                            **
**                                                                           **
*******************************************************************************
EH0*/

#ifndef _QVSP_NETDBG_H_
#define _QVSP_NETDBG_H_

#include "qtn/qvsp.h"
#include <qtn/qvsp_common.h>
#include <qtn/qvsp_data.h>

struct qvsp_netdbg_data {
	uint32_t		ndb_strm_tot;	/* must be first for stat_parser */
	uint32_t		fat_last;
	uint32_t		fat_avail;
	uint32_t		fat_intf;
	uint32_t		chan_last;
	uint32_t		strm_tot;
	uint32_t		strm_tot_qtn;
	uint32_t		strm_tot_enabled;
	uint32_t		strm_tot_disabled;
	uint32_t		strm_tot_disabled_remote;
	uint32_t		strm_tot_ac[WME_NUM_AC];

	uint32_t		pkts_tx;
	uint32_t		pkts_tx_sent;
	uint32_t		kbps_tx;
	uint32_t		kbps_tx_sent;
	uint32_t		pkts_rx;
	uint32_t		pkts_rx_sent;
	uint32_t		kbps_rx;
	uint32_t		kbps_rx_sent;

	uint32_t		strm_enable;
	uint32_t		strm_disable;
	uint32_t		strm_disable_remote;
	uint32_t		strm_reenable;
	uint32_t		fat_over;
	uint32_t		fat_under;
	uint32_t		fat_chk_disable;
	uint32_t		fat_chk_reenable;
} __packed;

struct qvsp_netdbg_strm {
	uint32_t		strm_hash;
	uint32_t		saddr[4];
	uint32_t		sport;
	uint32_t		daddr[4];
	uint32_t		dport;
	uint32_t		node_idx;
	uint32_t		hairpin_id;
	uint32_t		hairpin_type;
	uint32_t		ac;
	uint32_t		ip_proto;
	enum qvsp_rule_dir_e	dir;
	enum qvsp_strm_state_e	strm_state;
	uint32_t		age;
	uint32_t		pkts;
	uint32_t		pkts_sent;
	uint32_t		kbps;
	uint32_t		kbps_sent;
	uint32_t		phy_rate;
	uint32_t		phy_rate_disabled;
	uint32_t		ni_cost;
	uint32_t		ni_strm_cost;
	uint32_t		tx_last_mcs;
	uint32_t		avg_per;
} __packed;

/* Used by the stat_parser */
struct qvsp_netdbg_rec {
#ifdef QVSP_NETDBG_DUMMY
	struct qdrv_netdebug_hdr	ndb_hdr;
#endif
	struct qvsp_netdbg_data		vsp_data;
	struct qvsp_netdbg_strm		strm_data;
} __packed;

#endif /* _QVSP_NETDBG_H_ */

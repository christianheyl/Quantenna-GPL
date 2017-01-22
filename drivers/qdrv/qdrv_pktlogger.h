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
#ifndef _QDRV_PKTLOGGER_H
#define _QDRV_PKTLOGGER_H

#include "qdrv_mac.h"
#include "qdrv_wlan.h"
#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define QDRV_NETLINK_PKTLOGGER 30
#define PKTLOGGER_DEFAULT_DST_IP 0xFFFFFFFF
#define PKTLOGGER_DEFAULT_SRC_IP 0x01010102
/* Arbitrary unused UDP port numbers */
#define PKTLOGGER_UDP_SRC_PORT 6601
#define PKTLOGGER_UDP_DST_PORT 6602
#define PKTLOGGER_IP_MORE_FRAG_BIT 13
#define PKTLOGGER_IP_TTL 64
#define PKTLOGGER_IP_HEADER_LEN 20

#define QDRV_PKTLOGGER_INTERVAL_DFLT_NONE	0
#define QDRV_PKTLOGGER_INTERVAL_DFLT_STATS	1
#define QDRV_PKTLOGGER_INTERVAL_DFLT_SYSMSG	5
#define QDRV_PKTLOGGER_INTERVAL_DFLT_MEM	1
#define QDRV_PKTLOGGER_INTERVAL_DFLT_RATE	1
#define QDRV_PKTLOGGER_INTERVAL_DFLT_VSP	1000
#define QDRV_PKTLOGGER_INTERVAL_DFLT_PHY_STATS	2
#define QDRV_PKTLOGGER_INTERVAL_DFLT_FLUSH_DATA 1

#define QDRV_PKTLOGGER_QUEUE_LEN_MAX		32

/*
 * Collection of Ethernet device statistics causes severe performance degradation.
 */
#define QDRV_NETDEBUG_ETH_DEV_STATS_ENABLED	0

enum qdrv_netdbg_rectype_e {
	QDRV_NETDEBUG_TYPE_STATS = 1,
	QDRV_NETDEBUG_TYPE_EVENT = 2,
	QDRV_NETDEBUG_TYPE_RADAR = 3,
	QDRV_NETDEBUG_TYPE_TXBF = 4,
	QDRV_NETDEBUG_TYPE_IWEVENT = 5,
	QDRV_NETDEBUG_TYPE_SYSMSG = 6,
	QDRV_NETDEBUG_TYPE_MEM = 7,
	QDRV_NETDEBUG_TYPE_RATE = 8,
	QDRV_NETDEBUG_TYPE_VSP = 9,
	QDRV_NETDEBUG_TYPE_PHY_STATS = 10,
	QDRV_NETDEBUG_TYPE_FLUSH_DATA = 11,
	QDRV_NETDEBUG_TYPE_MAX
};

struct qdrv_pktlogger_types_tbl {
	int id;
	char *name;
	int (*start)(struct qdrv_wlan *qw);
	void (*stop)(struct qdrv_wlan *qw);
	uint32_t interval;
};

struct qdrv_pktlogger_data {
	void *data;
	uint32_t len;
	STAILQ_ENTRY(qdrv_pktlogger_data) entries;
};

__be32 qdrv_dev_ipaddr_get(struct net_device *dev);
struct qdrv_pktlogger_types_tbl *qdrv_pktlogger_get_tbls_by_id(int id);

int qdrv_pktlogger_init(struct qdrv_wlan *qw);
void qdrv_pktlogger_exit(struct qdrv_wlan *qw);
int qdrv_pktlogger_set(struct qdrv_wlan *qw, const char *param, const char *value);
void qdrv_pktlogger_show(struct qdrv_wlan *qw);
int qdrv_pktlogger_start_or_stop(struct qdrv_wlan *qw, const char *type, int start,
		uint32_t interval);

void qdrv_pktlogger_send(void *data, uint32_t len);
void *qdrv_pktlogger_alloc_buffer(char *description, int data_len);
void qdrv_pktlogger_free_buffer(void *data_buffer);
void qdrv_pktlogger_hdr_init(struct qdrv_wlan *qw, struct qdrv_pktlogger_hdr *hdr, int rec_type, int stats_len);

#endif /* _QDRV_PKTLOGGER_ */

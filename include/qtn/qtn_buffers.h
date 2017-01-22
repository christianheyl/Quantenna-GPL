/*
 * (C) Copyright 2012 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __QTN_BUFFERS_H
#define __QTN_BUFFERS_H

#include <common/topaz_config.h>

#define QTN_BUF_USE_11AC_SIZE			1
#define TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT		1

#define TOPAZ_HBM_SKB_ALLOCATOR			2

#if TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT
	/*
	 * Buffer allocation in Topaz when using the switch must be extremely
	 * careful such that the sum of all possible buffer pileup does not
	 * exceed the number of possible payloads. Otherwise the hardware will
	 * start processing null pointers.
	 */
	#define TOPAZ_HBM_PAYLOAD_COUNT_S	TOPAZ_HBM_BUF_EMAC_RX_COUNT_S
	#define QTN_BUFS_EMAC_TX_RING_S		9
	#define QDRV_MAX_QUEUED_MGMT_FRAMES	256
	#define QTN_BUFS_WMAC_RX_RING_S		10
	#define QTN_BUFS_WMAC_TX_NON_LHOST_S	9
	#define QTN_BUFS_WMAC_TX_QDISC_S	10
	#define QDRV_TX_SCH_RED_MASK		((1 << 7) - 1)
#elif QTN_BUF_USE_11AC_SIZE
	/* Topaz with 11ac buffers, no acceleration */
	#define TOPAZ_HBM_PAYLOAD_COUNT_S	TOPAZ_HBM_BUF_EMAC_RX_COUNT_S
	#define QTN_BUFS_EMAC_TX_RING_S		9
	#define QDRV_MAX_QUEUED_MGMT_FRAMES	256
	#define QTN_BUFS_WMAC_RX_RING_S		10
	#define QTN_BUFS_WMAC_TX_NON_LHOST_S	9
	#define QTN_BUFS_WMAC_TX_QDISC_S	11
	#define QDRV_TX_SCH_RED_MASK		((1 << 7) - 1)
#else
	/* Ruby or Topaz with 11n buffers no acceleration */
	#define TOPAZ_HBM_PAYLOAD_COUNT_S	12
	#define QTN_BUFS_EMAC_TX_RING_S		7
	#define QDRV_MAX_QUEUED_MGMT_FRAMES	512
	#define QTN_BUFS_WMAC_RX_RING_S		11
	#define QTN_BUFS_WMAC_TX_NON_LHOST_S	9
	#define QTN_BUFS_WMAC_TX_QDISC_S	11
	#define QDRV_TX_SCH_RED_MASK		((1 << 7) - 1)
#endif

#define TOPAZ_HBM_PAYLOAD_COUNT		(1 << TOPAZ_HBM_PAYLOAD_COUNT_S)

#ifdef QTN_RC_ENABLE_HDP
#define QTN_BUFS_EMAC_RX_RING_S		12
#else
#define QTN_BUFS_EMAC_RX_RING_S		8
#endif
#define QTN_BUFS_EMAC_TX_QDISC_S	7
#define QTN_BUFS_PCIE_TQE_RX_RING_S	12
#if defined (CONFIG_TOPAZ_PCIE_TARGET)
#define QTN_BUFS_LHOST_TQE_RX_RING_S	8
#elif defined (CONFIG_TOPAZ_PCIE_HOST)
#define QTN_BUFS_LHOST_TQE_RX_RING_S	10
#elif defined (TOPAZ_VB_CONFIG)
#define QTN_BUFS_LHOST_TQE_RX_RING_S	11
#else
#define QTN_BUFS_LHOST_TQE_RX_RING_S	9
#endif

#define QTN_BUFS_LHOST_TQE_RX_RING	(1 << QTN_BUFS_LHOST_TQE_RX_RING_S)
#define QTN_BUFS_EMAC_RX_RING		(1 << QTN_BUFS_EMAC_RX_RING_S)
#define QTN_BUFS_EMAC_TX_RING		(1 << QTN_BUFS_EMAC_TX_RING_S)
#define QTN_BUFS_EMAC_TX_QDISC		(1 << QTN_BUFS_EMAC_TX_QDISC_S)
#define QTN_BUFS_WMAC_RX_RING		(1 << QTN_BUFS_WMAC_RX_RING_S)
#define QTN_BUFS_WMAC_TX_NON_LHOST	(1 << QTN_BUFS_WMAC_TX_NON_LHOST_S)
#define QTN_BUFS_WMAC_TX_QDISC		(1 << QTN_BUFS_WMAC_TX_QDISC_S)
#define QTN_BUFS_PCIE_TQE_RX_RING	(1 << QTN_BUFS_PCIE_TQE_RX_RING_S)

#define QTN_BUFS_CPUS_TOTAL		(1 * (QTN_BUFS_LHOST_TQE_RX_RING))
#define QTN_BUFS_EMAC_TOTAL		(2 * (QTN_BUFS_EMAC_RX_RING + QTN_BUFS_EMAC_TX_RING + QTN_BUFS_EMAC_TX_QDISC))
#define QTN_BUFS_WMAC_TOTAL		(1 * (QTN_BUFS_WMAC_RX_RING + QTN_BUFS_WMAC_TX_NON_LHOST + QTN_BUFS_WMAC_TX_QDISC + QDRV_MAX_QUEUED_MGMT_FRAMES))
#define QTN_BUFS_ALLOC_TOTAL		(QTN_BUFS_CPUS_TOTAL /*+ QTN_BUFS_EMAC_TOTAL*/ + QTN_BUFS_WMAC_TOTAL)

#if TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT && (TOPAZ_HBM_PAYLOAD_COUNT <= QTN_BUFS_ALLOC_TOTAL)
	#error "Payload buffers distribution error"
#endif

#endif	// __QTN_BUFFERS_H


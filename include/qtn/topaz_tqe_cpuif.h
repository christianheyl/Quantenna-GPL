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

#ifndef __TOPAZ_TQE_CPUIF_PLATFORM_H
#define __TOPAZ_TQE_CPUIF_PLATFORM_H

#include "mproc_sync_base.h"

enum topaz_tqe_port {
	TOPAZ_TQE_FIRST_PORT	= 0,

	TOPAZ_TQE_EMAC_0_PORT	= 0,
	TOPAZ_TQE_EMAC_1_PORT	= 1,
	TOPAZ_TQE_WMAC_PORT	= 2,
	TOPAZ_TQE_PCIE_PORT	= 3,
	TOPAZ_TQE_LHOST_PORT	= 4,
	TOPAZ_TQE_MUC_PORT	= 5,
	TOPAZ_TQE_DSP_PORT	= 6,
	TOPAZ_TQE_AUC_PORT	= 7,

	TOPAZ_TQE_NUM_PORTS	= 8,
	TOPAZ_TQE_DUMMY_PORT	= 15
};

enum topaz_mproc_tqe_sem_id
{
	TOPAZ_MPROC_TQE_SEM_INVALID	= 0,
	TOPAZ_MPROC_TQE_SEM_LHOST	= 1,
	TOPAZ_MPROC_TQE_SEM_MUC		= 2,
	TOPAZ_MPROC_TQE_SEM_AUC		= 3
};

/* bits of port_id  */
#define PORT_ID_BITS                       8
/* high 3 bits in port_id is used to save dev_id for 2.4G VAP */
#define DEV_ID_BITS                        3

#define MAX_QFP_NETDEV                     (1 << DEV_ID_BITS)
#define MAX_DEV_ID                         (1 << DEV_ID_BITS)
#define MAX_PORT_ID                        (1 << (PORT_ID_BITS - DEV_ID_BITS))

#define GET_BIT_FIELD(var, offset, width) \
		(((var) >> (offset)) & ((1 << (width)) - 1))

#define INJECT_DEV_ID_TO_PORT_ID(port_id, dev_id, target) \
		do {\
			BUG_ON((port_id) >= MAX_PORT_ID || (dev_id) >= MAX_DEV_ID); \
			(target) = (port_id) | ((dev_id) << (PORT_ID_BITS - DEV_ID_BITS)); \
		} while (0)
#define EXTRACT_PORT_ID_FROM_PORT_ID(port_id) \
		GET_BIT_FIELD((port_id), 0, (PORT_ID_BITS - DEV_ID_BITS))
#define EXTRACT_DEV_ID_FROM_PORT_ID(port_id) \
		GET_BIT_FIELD((port_id), (PORT_ID_BITS - DEV_ID_BITS), DEV_ID_BITS)

#define TOPAZ_TQE_PORT_NAMES	{ "emac0", "emac1", "wmac", "pcie", "lhost", "muc", "dsp", "auc", }
#define TOPAZ_TQE_PORT_IS_EMAC(_port)	(((_port) == TOPAZ_TQE_EMAC_0_PORT) || \
						((_port) == TOPAZ_TQE_EMAC_1_PORT))
#define TOPAZ_TQE_PORT_IS_WMAC(_port)	((_port) == TOPAZ_TQE_WMAC_PORT)

#if defined(__linux__)
	#define TOPAZ_TQE_LOCAL_CPU	TOPAZ_TQE_LHOST_PORT
#elif defined(ARCSHELL)
	#define TOPAZ_TQE_LOCAL_CPU	TOPAZ_TQE_LHOST_PORT
#elif defined(MUC_BUILD)
	#define TOPAZ_TQE_LOCAL_CPU	TOPAZ_TQE_MUC_PORT
#elif defined(DSP_BUILD)
	#define TOPAZ_TQE_LOCAL_CPU	TOPAZ_TQE_DSP_PORT
#elif defined(AUC_BUILD)
	#define TOPAZ_TQE_LOCAL_CPU	TOPAZ_TQE_AUC_PORT
#else
	#error No TOPAZ_TQE_LOCAL_CPU set
#endif

union topaz_tqe_cpuif_descr
{
	struct
	{
		uint32_t dw0;
		uint32_t dw1;
		uint32_t dw2;
		uint32_t dw3;
	} raw;
	struct
	{
		signed buff_ptr_offset:16;
		unsigned misc_user:10;
		unsigned __reserved1:5;
		unsigned own:1;
		unsigned length:16;
		enum topaz_tqe_port in_port:4;
		unsigned need_to_free:1;
		unsigned __reserved2:3;
		unsigned control:8;
		void *pkt;
		union topaz_tqe_cpuif_descr *next;
	} data;
};

#define TQE_MISCUSER_A2M_TYPE		0x300
#define TQE_MISCUSER_A2M_TYPE_S		8
#define TQE_MISCUSER_A2M_TYPE_PARAM	0x0FF
#define TQE_MISCUSER_A2M_TYPE_PARAM_S	0

#define TQE_MISCUSER_A2M_TYPE_TXFEEDBACK	0
#define TQE_MISCUSER_A2M_TYPE_RXPKT		1
#define TQE_MISCUSER_A2M_TYPE_TXPKT		2

#define TQE_MISCUSER_M2L_DATA_NODE_IDX		0x7F
#define TQE_MISCUSER_M2L_DATA_NODE_IDX_S	0
#if TOPAZ_SWITCH_OUT_NODE_MASK != TQE_MISCUSER_M2L_DATA_NODE_IDX
	#error Node cache index misc_user must support 128 entries
#endif
#define TQE_MISCUSER_M2L_DATA_3ADDR_BR		0x80
#define TQE_MISCUSER_M2L_DATA_3ADDR_BR_S	7
#define TQE_MISCUSER_M2L_DROP			0x100
#define TQE_MISCUSER_M2L_DROP_S			8

#define TQE_MISCUSER_L2A_NO_AMSDU	0x002
#define TQE_MISCUSER_L2A_RATE_TRAINING	0x008
#define TQE_MISCUSER_L2A_RESERVED_FOR_A2A		0x10	/* place holder for A2A below */

#define TQE_MISCUSER_M2A_MGMT_SKIP_RATE_RETRY1		0x01
#define TQE_MISCUSER_M2A_MGMT_SKIP_RATE_RETRY1_S	0
#define TQE_MISCUSER_M2A_MGMT_OCS_FRAME			0x02
#define TQE_MISCUSER_M2A_MGMT_OCS_FRAME_S		1
#define TQE_MISCUSER_M2A_EVENT_VIA_TQE			0x04
#define TQE_MISCUSER_M2A_EVENT_VIA_TQE_S		2
#define TQE_MISCUSER_M2A_MGMT_PROBE_FRAME		0x08
#define TQE_MISCUSER_M2A_MGMT_PROBE_FRAME_S		3
#define TQE_MISCUSER_M2A_RESERVED_FOR_A2A		0x10	/* place holder for A2A below */
#define TQE_MISCUSER_M2A_MGMT_GROUP			0x20
#define TQE_MISCUSER_M2A_MGMT_GROUP_S			5

/*
 * At the ethq stage, only tqew descriptor is available for use. Some place holder have been added
 * above in M2A and L2A define to reserve bits.
 */
#define TQE_MISCUSER_A2A_GROUP				0x10


#define TQE_MISCUSER_DTIM_GROUP		(TQE_MISCUSER_M2A_MGMT_GROUP | TQE_MISCUSER_A2A_GROUP)

union topaz_tqe_cpuif_q_ptr_status
{
	uint32_t raw;
	struct {
		unsigned write_idx:15;
		unsigned write_idx_wrap:1;
		unsigned read_idx:15;
		unsigned read_idx_wrap:1;
	} data;
};

union topaz_tqe_cpuif_status
{
	uint32_t raw;
	struct {
		unsigned available:16;
		unsigned __reserved1:14;
		unsigned empty:1;
		unsigned full:1;
	} data;
};

union topaz_tqe_cpuif_tx_start
{
#define TOPAZ_TQE_CPUIF_TX_START_NREADY		RUBY_BIT(0)
#define TOPAZ_TQE_CPUIF_TX_START_NOT_SUCCESS	RUBY_BIT(30)
#define TOPAZ_TQE_CPUIF_TX_START_SUCCESS	RUBY_BIT(31)
#define TOPAZ_TQE_CPUIF_TX_START_DELIVERED	(TOPAZ_TQE_CPUIF_TX_START_NOT_SUCCESS | TOPAZ_TQE_CPUIF_TX_START_SUCCESS)
	uint32_t raw;
	struct {
		unsigned nready:1;
		unsigned __reserved1:29;
		unsigned not_success:1;
		unsigned success:1;
	} data;
};

union topaz_tqe_cpuif_ppctl
{
	struct
	{
#define TOPAZ_TQE_CPUIF_SM(val, mask, shift)	(((uint32_t)(val) & (mask)) << (shift))
#define TOPAZ_TQE_CPUIF_PPCTL_DW0(descr)	TOPAZ_TQE_CPUIF_SM(descr, 0xFFFFFFFF, 0)
		uint32_t ppctl0;
#define TOPAZ_TQE_CPUIF_PPCTL_DW1(pkt)		TOPAZ_TQE_CPUIF_SM(pkt, 0xFFFFFFFF, 0)
		uint32_t ppctl1;
#define TOPAZ_TQE_CPUIF_PPCTL_DW2(out_pri, out_node, out_port, out_portal, out_node_1, out_node_1_en, out_node_2, out_node_2_en) \
						TOPAZ_TQE_CPUIF_SM(out_pri, 0xF, 0) | \
						TOPAZ_TQE_CPUIF_SM(out_node, 0x7F, 4) | \
						TOPAZ_TQE_CPUIF_SM(out_port, 0xF, 11) | \
						TOPAZ_TQE_CPUIF_SM(out_portal, 0x1, 15) | \
						TOPAZ_TQE_CPUIF_SM(out_node_1, 0x7F, 16) | \
						TOPAZ_TQE_CPUIF_SM(out_node_1_en, 0x1, 23) | \
						TOPAZ_TQE_CPUIF_SM(out_node_2, 0x7F, 24) | \
						TOPAZ_TQE_CPUIF_SM(out_node_2_en, 0x1, 31)
		uint32_t ppctl2;
#define TOPAZ_TQE_CPUIF_PPCTL_DW3(out_node_3, out_node_3_en, out_node_4, out_node_4_en, out_node_5, out_node_5_en, out_node_6, out_node_6_en) \
						TOPAZ_TQE_CPUIF_SM(out_node_3, 0x7F, 0) | \
						TOPAZ_TQE_CPUIF_SM(out_node_3_en, 0x1, 7) | \
						TOPAZ_TQE_CPUIF_SM(out_node_4, 0x7F, 8) | \
						TOPAZ_TQE_CPUIF_SM(out_node_4_en, 0x1, 15) | \
						TOPAZ_TQE_CPUIF_SM(out_node_5, 0x7F, 16) | \
						TOPAZ_TQE_CPUIF_SM(out_node_5_en, 0x1, 23) | \
						TOPAZ_TQE_CPUIF_SM(out_node_6, 0x7F, 24) | \
						TOPAZ_TQE_CPUIF_SM(out_node_6_en, 0x1, 31)
		uint32_t ppctl3;
#define TOPAZ_TQE_CPUIF_PPCTL_DW4(buff_ptr_offset, sa_match, da_match, mcast, free, buff_pool_num, tqe_free) \
						TOPAZ_TQE_CPUIF_SM(buff_ptr_offset, 0xFFFF, 0) | \
						TOPAZ_TQE_CPUIF_SM(sa_match, 0x1, 24) | \
						TOPAZ_TQE_CPUIF_SM(da_match, 0x1, 25) | \
						TOPAZ_TQE_CPUIF_SM(mcast, 0x1, 26) | \
						TOPAZ_TQE_CPUIF_SM(free, 0x1, 27) | \
						TOPAZ_TQE_CPUIF_SM(buff_pool_num, 0x3, 28) | \
						TOPAZ_TQE_CPUIF_SM(tqe_free, 0x1, 30)
		uint32_t ppctl4;
#define TOPAZ_TQE_CPUIF_PPCTL_DW5(length, misc_user) \
						TOPAZ_TQE_CPUIF_SM(length, 0xFFFF, 0) | \
						TOPAZ_TQE_CPUIF_SM(misc_user, 0x3FF, 16)
		uint32_t ppctl5;
	} raw;
	struct
	{
		void *descr;
		void *pkt;
		unsigned out_pri:4;
		unsigned out_node_0:7;
		enum topaz_tqe_port out_port:4;
		unsigned portal:1;
		unsigned out_node_1:7;
		unsigned out_node_1_en:1;
		unsigned out_node_2:7;
		unsigned out_node_2_en:1;
		unsigned out_node_3:7;
		unsigned out_node_3_en:1;
		unsigned out_node_4:7;
		unsigned out_node_4_en:1;
		unsigned out_node_5:7;
		unsigned out_node_5_en:1;
		unsigned out_node_6:7;
		unsigned out_node_6_en:1;
		signed buff_ptr_offset:16;
		unsigned __reserved1:8;
		unsigned sa_match:1;
		unsigned da_match:1;
		unsigned mcast:1;
		unsigned free:1;
		unsigned buff_pool_num:2;
		unsigned tqe_free:1;
		unsigned __reserved2:1;
		unsigned length:16;
		unsigned misc_user:10;
		unsigned __reserved3:6;
	} data;
};

RUBY_INLINE void
topaz_tqe_cpuif_ppctl_clear(union topaz_tqe_cpuif_ppctl *pp)
{
	pp->raw.ppctl0 = 0;
	pp->raw.ppctl1 = 0;
	pp->raw.ppctl2 = 0;
	pp->raw.ppctl3 = 0;
	pp->raw.ppctl4 = 0;
	pp->raw.ppctl5 = 0;
}

RUBY_INLINE void
topaz_tqe_cpuif_ppctl_init(union topaz_tqe_cpuif_ppctl *pp,
		uint8_t port, const uint8_t *const nodes, uint8_t node_count, uint8_t pri,
		uint8_t portal, uint8_t free, uint8_t buff_pool, uint8_t tqe_free, uint16_t misc_user)
{
	pp->raw.ppctl0 = TOPAZ_TQE_CPUIF_PPCTL_DW0(0);
	pp->raw.ppctl1 = TOPAZ_TQE_CPUIF_PPCTL_DW1(0);
	pp->raw.ppctl2 = TOPAZ_TQE_CPUIF_PPCTL_DW2(pri, nodes ? nodes[0] : 0, port, portal, 0, 0, 0, 0);
	pp->raw.ppctl3 = TOPAZ_TQE_CPUIF_PPCTL_DW3(0, 0, 0, 0, 0, 0, 0, 0);
	pp->raw.ppctl4 = TOPAZ_TQE_CPUIF_PPCTL_DW4(0, 0, 0, (node_count > 1), free, buff_pool, tqe_free);
	pp->raw.ppctl5 = TOPAZ_TQE_CPUIF_PPCTL_DW5(0, misc_user);
#if 0
#define	_outnode(_i)	do {							\
		if ((_i) <= node_count) {					\
			pp->data.out_node_##_i = nodes[(_i)-1];			\
			pp->data.out_node_##_i##_en = 1;			\
		}								\
	} while(0)

	if(nodes) {
		/* Multicast nodes number range from 1->6. unicast set to 0*/
		_outnode(1);
		_outnode(2);
		_outnode(3);
		_outnode(4);
		_outnode(5);
		_outnode(6);
	}
#undef	_outnode
#endif
}

RUBY_INLINE int
topaz_tqe_cpuif_port_to_num(enum topaz_tqe_port port)
{
	if (port == TOPAZ_TQE_PCIE_PORT) {
		return 4;
	} else {
		return port - TOPAZ_TQE_LHOST_PORT;
	}
}

RUBY_INLINE void
__topaz_tqe_cpuif_setup_irq(enum topaz_tqe_port port, int enable, unsigned int threshold)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	uint32_t csr = qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_CSR(num));

	csr &= ~TOPAZ_TQE_CPUIF_CSR_IRQ_THRESHOLD(~0x0);
	if (threshold) {
		csr |= TOPAZ_TQE_CPUIF_CSR_IRQ_THRESHOLD_EN;
		csr |= TOPAZ_TQE_CPUIF_CSR_IRQ_THRESHOLD(threshold);
	} else {
		csr &= ~TOPAZ_TQE_CPUIF_CSR_IRQ_THRESHOLD_EN;
	}

	if (enable) {
		csr |= TOPAZ_TQE_CPUIF_CSR_IRQ_EN;
	} else {
		csr &= ~TOPAZ_TQE_CPUIF_CSR_IRQ_EN;
	}

	qtn_mproc_sync_mem_write_wmb(TOPAZ_TQE_CPUIF_CSR(num), csr); /* can be used to disable/enable, so better to have barrier*/
}

RUBY_INLINE void
topaz_tqe_cpuif_setup_irq(int enable, unsigned int threshold)
{
	__topaz_tqe_cpuif_setup_irq(TOPAZ_TQE_LOCAL_CPU, enable, threshold);
}

RUBY_INLINE void
__topaz_tqe_cpuif_setup_reset(enum topaz_tqe_port port, int reset)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	uint32_t csr = qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_CSR(num));
	if (reset) {
		qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_CSR(num), csr | TOPAZ_TQE_CPUIF_CSR_RESET);
	} else {
		qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_CSR(num), csr & ~TOPAZ_TQE_CPUIF_CSR_RESET);
	}
}

RUBY_INLINE void
topaz_tqe_cpuif_setup_reset(int reset)
{
	__topaz_tqe_cpuif_setup_reset(TOPAZ_TQE_LOCAL_CPU, reset);
}

RUBY_INLINE void
__topaz_tqe_cpuif_setup_ring(enum topaz_tqe_port port, union topaz_tqe_cpuif_descr *base, uint16_t count)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_RX_RING(num), (uint32_t)base);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_RX_RING_SIZE(num), count);
}

RUBY_INLINE void
topaz_tqe_cpuif_setup_ring(union topaz_tqe_cpuif_descr *base, uint16_t count)
{
	__topaz_tqe_cpuif_setup_ring(TOPAZ_TQE_LOCAL_CPU, base, count);
}

RUBY_INLINE uint16_t
__topaz_tqe_cpuif_get_ring_size(enum topaz_tqe_port port)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	return qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_RX_RING_SIZE(num));
}

RUBY_INLINE uint16_t
topaz_tqe_cpuif_get_ring_size(void)
{
	return __topaz_tqe_cpuif_get_ring_size(TOPAZ_TQE_LOCAL_CPU);
}

RUBY_INLINE union topaz_tqe_cpuif_descr*
__topaz_tqe_cpuif_get_curr(enum topaz_tqe_port port)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	return (union topaz_tqe_cpuif_descr*)
		qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_RX_CURPTR(num));
}

RUBY_INLINE union topaz_tqe_cpuif_descr*
topaz_tqe_cpuif_get_curr(void)
{
	return __topaz_tqe_cpuif_get_curr(TOPAZ_TQE_LOCAL_CPU);
}

RUBY_INLINE void
__topaz_tqe_cpuif_put_back(enum topaz_tqe_port port, union topaz_tqe_cpuif_descr * descr)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_PKT_FINISH(num), (uint32_t)descr);
}

RUBY_INLINE void
topaz_tqe_cpuif_put_back(union topaz_tqe_cpuif_descr * descr)
{
	__topaz_tqe_cpuif_put_back(TOPAZ_TQE_LOCAL_CPU, descr);
}

RUBY_INLINE union topaz_tqe_cpuif_q_ptr_status
__topaz_tqe_cpuif_get_q_ptr_status(enum topaz_tqe_port port)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	union topaz_tqe_cpuif_q_ptr_status status;
	status.raw = qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_Q_PTR_STATUS(num));
	return status;
}

RUBY_INLINE union topaz_tqe_cpuif_q_ptr_status
topaz_tqe_cpuif_get_q_ptr_status(void)
{
	return __topaz_tqe_cpuif_get_q_ptr_status(TOPAZ_TQE_LOCAL_CPU);
}

RUBY_INLINE union topaz_tqe_cpuif_status
__topaz_tqe_cpuif_get_status(enum topaz_tqe_port port)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	union topaz_tqe_cpuif_status status;
	status.raw = qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_STATUS(num));
	return status;
}

RUBY_INLINE union topaz_tqe_cpuif_status
topaz_tqe_cpuif_get_status(void)
{
	return __topaz_tqe_cpuif_get_status(TOPAZ_TQE_LOCAL_CPU);
}

RUBY_INLINE int
__topaz_tqe_cpuif_tx_nready(enum topaz_tqe_port port)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	return (qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_TXSTART(num)) &
		TOPAZ_TQE_CPUIF_TX_START_NREADY);
}

RUBY_INLINE int
topaz_tqe_cpuif_tx_nready(void)
{
	return __topaz_tqe_cpuif_tx_nready(TOPAZ_TQE_LOCAL_CPU);
}

RUBY_INLINE int
__topaz_tqe_cpuif_tx_success(enum topaz_tqe_port port)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	uint32_t tx_start = qtn_mproc_sync_mem_read(TOPAZ_TQE_CPUIF_TXSTART(num));

	if ((tx_start & TOPAZ_TQE_CPUIF_TX_START_NREADY) ||
			!(tx_start & TOPAZ_TQE_CPUIF_TX_START_DELIVERED)) {
		return -1;
	} else if (tx_start & TOPAZ_TQE_CPUIF_TX_START_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

RUBY_INLINE int
topaz_tqe_cpuif_tx_success(void)
{
	return __topaz_tqe_cpuif_tx_success(TOPAZ_TQE_LOCAL_CPU);
}

RUBY_INLINE int
__topaz_tqe_cpuif_ppctl_write(enum topaz_tqe_port port, const union topaz_tqe_cpuif_ppctl *ctl)
{
	int num = topaz_tqe_cpuif_port_to_num(port);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_PPCTL0(num), ctl->raw.ppctl0);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_PPCTL1(num), ctl->raw.ppctl1);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_PPCTL2(num), ctl->raw.ppctl2);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_PPCTL3(num), ctl->raw.ppctl3);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_PPCTL4(num), ctl->raw.ppctl4);
	qtn_mproc_sync_mem_write_wmb(TOPAZ_TQE_CPUIF_PPCTL5(num), ctl->raw.ppctl5);
	return num;
}

RUBY_INLINE int
topaz_tqe_cpuif_ppctl_write(const union topaz_tqe_cpuif_ppctl *ctl)
{
	return __topaz_tqe_cpuif_ppctl_write(TOPAZ_TQE_LOCAL_CPU, ctl);
}

RUBY_INLINE void
__topaz_tqe_cpuif_tx_start(enum topaz_tqe_port port, const union topaz_tqe_cpuif_ppctl *ctl)
{
	int num = __topaz_tqe_cpuif_ppctl_write(port, ctl);
	qtn_mproc_sync_mem_write(TOPAZ_TQE_CPUIF_TXSTART(num), TOPAZ_TQE_CPUIF_TX_START_NREADY);
}

RUBY_INLINE void
topaz_tqe_cpuif_tx_start(const union topaz_tqe_cpuif_ppctl *ctl)
{
	__topaz_tqe_cpuif_tx_start(TOPAZ_TQE_LOCAL_CPU, ctl);
}

#define TQE_SEMA_GET_MAX			0xFFFF

#define QTN_WAIT_TQE_CPUIF_LOOP_MASK		0xFFFF
RUBY_INLINE void topaz_tqe_wait(void)
{
	uint32_t loop = 0;

	while (topaz_tqe_cpuif_tx_nready()) {
		loop++;
		if ((loop & ~QTN_WAIT_TQE_CPUIF_LOOP_MASK) &&
				!((loop) & QTN_WAIT_TQE_CPUIF_LOOP_MASK)) {
#ifdef __KERNEL__
			printk("stuck in topaz_tqe_wait()\n");
#endif
#ifdef MUC_BUILD
			uc_printk("stuck in topaz_tqe_wait()\n");
#endif
		}
	}
}

RUBY_INLINE void topaz_tqe_emac_reflect_to(const uint8_t out_port, const int bonded)
{
	if (out_port < TOPAZ_TQE_NUM_PORTS) {
		uint32_t done_dly = qtn_mproc_sync_mem_read(TOPAZ_TQE_MISC);

		done_dly &= ~TOPAZ_TQE_MISC_RFLCT_OUT_PORT;
		done_dly |= SM(out_port, TOPAZ_TQE_MISC_RFLCT_OUT_PORT) |
						TOPAZ_TQE_MISC_RFLCT_OUT_PORT_ENABLE;
		if (bonded) {
			done_dly |= TOPAZ_TQE_MISC_RFLCT_2_OUT_PORT_ENABLE;
		}
		qtn_mproc_sync_mem_write(TOPAZ_TQE_MISC, done_dly);
#if defined (__KERNEL__) && defined (DEBUG)
		printk("TOPAZ_TQE_MISC: 0x%x\n", done_dly);
#endif
	}
}
#endif /* #ifndef __TOPAZ_TQE_CPUIF_PLATFORM_H */



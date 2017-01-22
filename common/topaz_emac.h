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

#ifndef __TOPAZ_EMAC_H
#define __TOPAZ_EMAC_H

#include "topaz_platform.h"
#ifdef _KERNEL
#include "ruby_arasan_emac_ahb.h"
#endif

#define TOPAZ_EMAC_NUM_DPI_FIELDS		32
#define TOPAZ_EMAC_NUM_DPI_FILTERS		16
#define TOPAZ_EMAC_NUM_DPI_IPTUPLES		8

#define TOPAZ_EMAC_WRAP_CTRL			0x300
# define TOPAZ_EMAC_WRAP_CTRL_VERSION			0x0000000f
#define TOPAZ_EMAC_RXP_CTRL			0x304
# define TOPAZ_EMAC_RXP_CTRL_ENABLE			BIT(0)
# define TOPAZ_EMAC_RXP_CTRL_ENDIAN			BIT(1)
# define TOPAZ_EMAC_RXP_CTRL_TQE_SYNC_EN_BP		BIT(2)
# define TOPAZ_EMAC_RXP_CTRL_TQE_SYNC_EN_SUCC		BIT(3)
# define TOPAZ_EMAC_RXP_CTRL_SYNC_NONE			0
# define TOPAZ_EMAC_RXP_CTRL_SYNC_TQE			BIT(4)
# define TOPAZ_EMAC_RXP_CTRL_SYNC_RX_DMA		BIT(5)
# define TOPAZ_EMAC_RXP_CTRL_SYNC_RX_PARSER		(BIT(4) | BIT(5))
# define TOPAZ_EMAC_RXP_CTRL_SRESET			BIT(8)
#define TOPAZ_EMAC_TXP_CTRL			0x308
# define TOPAZ_EMAC_TXP_CTRL_AHB_ENABLE			BIT(0)
# define TOPAZ_EMAC_TXP_CTRL_SRESET			BIT(8)
#define TOPAZ_EMAC_TXP_Q_FULL			0x320
# define TOPAZ_EMAC_TXP_Q_FULL_BIT			BIT(31)
#define TOPAZ_EMAC_TXP_DESC_PTR			0x324
#define TOPAZ_EMAC_DEBUG_BUS_SEL		0x328
#define TOPAZ_EMAC_DEBUG_BUS_SEL_TXP_PTR		0x00000001

#define TOPAZ_EMAC_BUFFER_POOLS			0x32c
# define TOPAZ_EMAC_BUFFER_POOLS_RX_REPLENISH		0x00000003
# define TOPAZ_EMAC_BUFFER_POOLS_RX_REPLENISH_S		0
# define TOPAZ_EMAC_BUFFER_POOLS_TX_RETURN		0x0000000c
# define TOPAZ_EMAC_BUFFER_POOLS_TX_RETURN_S		2
#define TOPAZ_EMAC_TXP_STATUS			0x330

/*
 * EMAC Tx-ring read/write pointer, should write 0x1 to reg_0x328 before read the pointer.
 * Bit[12:0]  : EMAC read pointer
 * Bit[25:13] : TQE write pointer
 */
#define TOPAZ_EMAC_TXP_READ_PTR(stat)			((stat) & 0x1FFF)
#define TOPAZ_EMAC_TXP_WRITE_PTR(stat)			(((stat) >> 13) & 0x1FFF)

#define TOPAZ_EMAC_DESC_LIMIT			0x334
# define TOPAZ_EMAC_DESC_LIMIT_MASK			0x0000ffff
#define TOPAZ_EMAC_RXP_PRIO_CTRL		0x350
# define TOPAZ_EMAC_RXP_PRIO_CTRL_TID_SEL		0x00000003
# define TOPAZ_EMAC_RXP_PRIO_CTRL_TID_SEL_S		0
# define TOPAZ_EMAC_RXP_PRIO_CTRL_TID_MINMAX		0x00000004
# define TOPAZ_EMAC_RXP_PRIO_CTRL_TID_MINMAX_S		2
# define TOPAZ_EMAC_RXP_PRIO_CTRL_SW_TID		0x00000f00
# define TOPAZ_EMAC_RXP_PRIO_CTRL_SW_TID_S		8
# define TOPAZ_EMAC_RXP_PRIO_CTRL_SW_TID_SEL		BIT(12)

#define TOPAZ_EMAC_RXP_OUTPORT_CTRL		0x354
# define TOPAZ_EMAC_RXP_OP_CTRL_DA			0x00000003
# define TOPAZ_EMAC_RXP_OP_CTRL_DA_S			0
# define TOPAZ_EMAC_RXP_OP_CTRL_VLAN			0x0000000c
# define TOPAZ_EMAC_RXP_OP_CTRL_VLAN_S			2
# define TOPAZ_EMAC_RXP_OP_CTRL_IP			0x00000030
# define TOPAZ_EMAC_RXP_OP_CTRL_IP_S			4
# define TOPAZ_EMAC_RXP_OP_CTRL_DPI			0x000000c0
# define TOPAZ_EMAC_RXP_OP_CTRL_DPI_S			6
# define TOPAZ_EMAC_RXP_OP_CTRL_MCAST_EN		BIT(8)
# define TOPAZ_EMAC_RXP_OP_CTRL_MCAST_SEL		BIT(9)
# define TOPAZ_EMAC_RXP_OP_CTRL_MCAST_PORT		0x00003c00
# define TOPAZ_EMAC_RXP_OP_CTRL_MCAST_PORT_S		10
# define TOPAZ_EMAC_RXP_OP_CTRL_DYNAMIC_FAIL_PORT	0x000f0000
# define TOPAZ_EMAC_RXP_OP_CTRL_DYNAMIC_FAIL_PORT_S	16
# define TOPAZ_EMAC_RXP_OP_CTRL_SW_BACKDOOR_PORT	0x00f00000
# define TOPAZ_EMAC_RXP_OP_CTRL_SW_BACKDOOR_PORT_S	20
# define TOPAZ_EMAC_RXP_OP_CTRL_STATIC_FAIL_PORT	0x0f000000
# define TOPAZ_EMAC_RXP_OP_CTRL_STATIC_FAIL_PORT_S	24
# define TOPAZ_EMAC_RXP_OP_CTRL_STATIC_PORT_SEL		0x70000000
# define TOPAZ_EMAC_RXP_OP_CTRL_STATIC_PORT_SEL_S	28
# define TOPAZ_EMAC_RXP_OP_CTRL_STATIC_ENABLE		BIT(31)
#ifndef __ASSEMBLY__
union topaz_emac_rxp_outport_ctrl {
	struct {
		uint32_t word0;
	} raw;
	struct {
		uint32_t da_prio		: 2,
			vlan_prio		: 2,
			ip_prio			: 2,
			dpi_prio		: 2,
			mcast_en		: 1,
			mcast_sel		: 1,
			mcast_port		: 4,
			__unused		: 2,
			dynamic_fail_port	: 4,
			sw_backdoor_port	: 4,
			static_fail_port	: 4,
			static_port_sel		: 3,
			static_mode_en		: 1;
	} data;
};
#endif	// __ASSEMBLY__

#define TOPAZ_EMAC_RXP_OUTNODE_CTRL		0x358
union topaz_emac_rxp_outnode_ctrl {
	struct {
		uint32_t word0;
	} raw;
	struct {
		uint32_t mcast_node		: 6,
			 __unused		: 4,
			 dynamic_fail_node	: 6,
			 sw_backdoor_node	: 6,
			 static_fail_node	: 6,
			 static_node_sel	: 3,
			 __unused2		: 1;
	} data;
};
#define TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID		0x380
# define TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(pcp, tid)	(((tid) & 0xf) << ((pcp) * 4))
#define TOPAZ_EMAC_RXP_VLAN_PRI_CTRL		0x384
# define TOPAZ_EMAC_RXP_VLAN_PRI_CTRL_TAG		0x00000003
# define TOPAZ_EMAC_RXP_VLAN_PRI_CTRL_TAG_S		0
#define TOPAZ_EMAC_RXP_VLAN_TAG_0_1		0x388
# define TOPAZ_EMAC_RXP_VLAN_TAG_0			0x0000ffff
# define TOPAZ_EMAC_RXP_VLAN_TAG_0_S			0
# define TOPAZ_EMAC_RXP_VLAN_TAG_1			0xffff0000
# define TOPAZ_EMAC_RXP_VLAN_TAG_1_S			16
#define TOPAZ_EMAC_RXP_VLAN_TAG_2_3		0x38c
# define TOPAZ_EMAC_RXP_VLAN_TAG_2			0x0000ffff
# define TOPAZ_EMAC_RXP_VLAN_TAG_2_S			0
# define TOPAZ_EMAC_RXP_VLAN_TAG_3			0xffff0000
# define TOPAZ_EMAC_RXP_VLAN_TAG_3_S			16
#define TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REG(x)	(0x390 + 4 * (x))
#define TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REGS	8
#define TOPAZ_EMAC_RXP_IP_CTRL			0x3b0
#define TOPAZ_EMAC_RXP_DPI_TID_MAP_REG(x)	(0x3b4 + 4 * (x))
#define TOPAZ_EMAC_RXP_DPI_TID_MAP_INDEX(x)	TOPAZ_EMAC_RXP_DPI_TID_MAP_REG((x) >> 3)
#define TOPAZ_EMAC_RXP_TID_MAP_INDEX_SHIFT(x)	(((x) & 0x7) << 2)
#define TOPAZ_EMAC_RXP_TID_MAP_INDEX_MASK(x)	(0xF << TOPAZ_EMAC_RXP_TID_MAP_INDEX_SHIFT(x))
#define TOPAZ_EMAC_RXP_DPI_CTRL			0x3bc
# define TOPAZ_EMAC_RXP_DPI_CTRL_DPI_FAIL_TID		0x0000000f
#define TOPAZ_EMAC_RXP_STATUS			0x3c0
#define TOPAZ_EMAC_RXP_CST_SEL			0x3c4
#define TOPAZ_EMAC_RXP_FRAME_CNT_CLEAR		0x3cc
# define TOPAZ_EMAC_RXP_FRAME_CNT_CLEAR_ERROR		BIT(0)
# define TOPAZ_EMAC_RXP_FRAME_CNT_CLEAR_TOTAL		BIT(1)
# define TOPAZ_EMAC_RXP_FRAME_CNT_CLEAR_DA_MATCH	BIT(2)
# define TOPAZ_EMAC_RXP_FRAME_CNT_CLEAR_SA_MATCH	BIT(3)
#define TOPAZ_EMAC_FRM_COUNT_ERRORS		0x3d0
#define TOPAZ_EMAC_FRM_COUNT_TOTAL		0x3d4
#define TOPAZ_EMAC_FRM_COUNT_DA_MATCH		0x3d8
#define TOPAZ_EMAC_FRM_COUNT_SA_MATCH		0x3dc
#define TOPAZ_EMAC_RX_DPI_FIELD_VAL(x)		(0x400 + 4 * (x))
#define TOPAZ_EMAC_RX_DPI_FIELD_MASK(x)		(0x480 + 4 * (x))
#define TOPAZ_EMAC_RX_DPI_FIELD_CTRL(x)		(0x500 + 4 * (x))
#define TOPAZ_EMAC_RX_DPI_FIELD_GROUP(x)	(0x580 + 4 * (x))
#define TOPAZ_EMAC_RX_DPI_OUT_CTRL(x)		(0x5c0 + 4 * (x))
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_NODE	0x0000007f
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_NODE_S	0
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_PORT	0x00000780
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_PORT_S	7
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_COMBO	0x00001800
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_COMBO_S	11
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_OFF		0x0
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_IPTUPLE	0x1
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_DPI		0x2
# define TOPAZ_EMAC_RX_DPI_OUT_CTRL_BOTH	0x3

#ifndef __ASSEMBLY__
enum topaz_emac_rx_dpi_anchor {
	TOPAZ_EMAC_RX_DPI_ANCHOR_FRAME	= 0x0,
	TOPAZ_EMAC_RX_DPI_ANCHOR_VLAN0	= 0x1,
	TOPAZ_EMAC_RX_DPI_ANCHOR_VLAN1	= 0x2,
	TOPAZ_EMAC_RX_DPI_ANCHOR_VLAN2	= 0x3,
	TOPAZ_EMAC_RX_DPI_ANCHOR_VLAN3	= 0x4,
	TOPAZ_EMAC_RX_DPI_ANCHOR_OTHER	= 0x5,
	TOPAZ_EMAC_RX_DPI_ANCHOR_LLC	= 0x6,
	TOPAZ_EMAC_RX_DPI_ANCHOR_IPV4	= 0x7,
	TOPAZ_EMAC_RX_DPI_ANCHOR_IPV6	= 0x8,
	TOPAZ_EMAC_RX_DPI_ANCHOR_TCP	= 0x9,
	TOPAZ_EMAC_RX_DPI_ANCHOR_UDP	= 0xa,
};

enum topaz_emac_rx_dpi_cmp_op {
	TOPAZ_EMAC_RX_DPI_CMP_OP_EQUAL	= 0x0,
	TOPAZ_EMAC_RX_DPI_CMP_OP_NEQUAL	= 0x1,
	TOPAZ_EMAC_RX_DPI_CMP_OP_GT	= 0x2,
	TOPAZ_EMAC_RX_DPI_CMP_OP_LT	= 0x3,
};

#define TOPAZ_EMAC_RX_DPI_ANCHOR_NAMES	{		\
	"frame", "vlan0", "vlan1", "vlan2", "vlan3",	\
	"other", "llc", "ipv4", "ipv6", "tcp", "udp",	\
}

#define TOPAZ_EMAC_RX_DPI_CMP_OP_NAMES	{		\
	"==", "!=", ">", "<",				\
}


union topaz_emac_rx_dpi_ctrl {
	uint32_t raw;
	struct {
		uint32_t offset		: 9,
			 anchor		: 4,
			 cmp_op		: 2,
			 enable		: 1,
			 __unused	: 16;
	} data;
};
#endif	// __ASSEMBLY__

#define TOPAZ_EMAC_RX_DPI_IPT_GROUP(x)		(0x720 + 4 * (x))
#define TOPAZ_EMAC_RX_DPI_IPT_GROUP_SRCADDR(x)	BIT((x) + 0)
#define TOPAZ_EMAC_RX_DPI_IPT_GROUP_DESTADDR(x)	BIT((x) + 8)
#define TOPAZ_EMAC_RX_DPI_IPT_GROUP_DESTPORT(x)	BIT((x) + 16)
#define TOPAZ_EMAC_RX_DPI_IPT_GROUP_SRCPORT(x)	BIT((x) + 24)
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(x)	(0x600 + 4 * (x))
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA_MAX	8
#define TOPAZ_EMAC_RX_DPI_IPT_PORT_DEST		0x0000FFFF
#define TOPAZ_EMAC_RX_DPI_IPT_PORT_DEST_S	0
#define TOPAZ_EMAC_RX_DPI_IPT_PORT_SRC		0xFFFF0000
#define TOPAZ_EMAC_RX_DPI_IPT_PORT_SRC_S	16
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_COM		0x620
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_ENT	0x0000000F
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_ENT_S	0
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_WRITE	BIT(8)
#define TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_READ	BIT(9)
#define TOPAZ_EMAC_RX_DPI_IPT_ENTRIES			9
#define TOPAZ_EMAC_RX_DPI_IPT_ENTRY_SRCADDR_START	0
#define TOPAZ_EMAC_RX_DPI_IPT_ENTRY_SRCADDR_END		4
#define TOPAZ_EMAC_RX_DPI_IPT_ENTRY_DESTADDR_START	4
#define TOPAZ_EMAC_RX_DPI_IPT_ENTRY_DESTADDR_END	8
#define TOPAZ_EMAC_RX_DPI_IPT_ENTRY_PORTS		8


#define TOPAZ_EMAC_IP_PROTO_ENTRY(x)		(0xa000 + 4 * (x))
#define TOPAZ_EMAC_IP_PROTO_ENTRIES		256
#define TOPAZ_EMAC_IP_PROTO_OUT_NODE		0x0000007F
#define TOPAZ_EMAC_IP_PROTO_OUT_NODE_S		0
#define TOPAZ_EMAC_IP_PROTO_OUT_PORT		0x00000780
#define TOPAZ_EMAC_IP_PROTO_OUT_PORT_S		7
#define TOPAZ_EMAC_IP_PROTO_VALID		BIT(11)
#define TOPAZ_EMAC_IP_PROTO_VALID_S		11

#define TOPAZ_EMAC_IPDSCP_HWT_SHIFT	2

#define TOPAZ_EMAC_RXP_PRIO_IS_BIT2	0
#define TOPAZ_EMAC_RXP_PRIO_IS_VLAN	1
#define TOPAZ_EMAC_RXP_PRIO_IS_DSCP	2
#define TOPAZ_EMAC_RXP_PRIO_IS_DPI	3

extern void topaz_emac_to_lhost(uint32_t enable);
extern int topaz_emac_get_bonding(void);

#endif	// __TOPAZ_EMAC_H


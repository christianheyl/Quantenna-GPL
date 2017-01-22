/**
 * (C) Copyright 2011-2012 Quantenna Communications Inc.
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
 **/

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <asm/board/soc.h>

#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_tqe.h>
#include <qtn/topaz_hbm_cpuif.h>
#include <qtn/topaz_hbm.h>
#include <qtn/topaz_fwt.h>
#include <qtn/topaz_ipprt.h>
#include <qtn/topaz_vlan_cpuif.h>

#include <qtn/topaz_dpi.h>
#include <common/topaz_emac.h>
#include <drivers/ruby/emac_lib.h>

#include <qtn/topaz_fwt_sw.h>
#include <qtn/qtn_buffers.h>
#include <qtn/qdrv_sch.h>
#include <qtn/qtn_wmm_ac.h>
#include <qtn/qtn_vlan.h>
#include <qtn/hardware_revision.h>
#include <qtn/shared_params.h>

#include <asm/board/pm.h>

#ifdef CONFIG_QVSP
#include "qtn/qvsp.h"
#endif

#include <compat.h>

#if defined (TOPAZ_VB_CONFIG)
#define EMAC_DESC_USE_SRAM 0
#else
#define EMAC_DESC_USE_SRAM 1
#endif

#define EMAC_BONDING_GROUP 1
#define EMAC_MAX_INTERFACE	2
static int eth_ifindex[EMAC_MAX_INTERFACE]= {0};

struct emac_port_info {
	u32 base_addr;
	u32 mdio_base_addr;
	enum topaz_tqe_port tqe_port;
	int irq;
	const char *proc_name;
};

static int dscp_priority      = 0;
static int dscp_value         = 0;
static int emac_xflow_disable = 0;

#define EMAC_WBSP_CTRL_DISABLED	0
#define EMAC_WBSP_CTRL_ENABLED	1
#define EMAC_WBSP_CTRL_SWAPPED	2
#if defined (ERICSSON_CONFIG)
static int emac_wbsp_ctrl = EMAC_WBSP_CTRL_ENABLED;
#else
static int emac_wbsp_ctrl = EMAC_WBSP_CTRL_DISABLED;
#endif


static const struct emac_port_info iflist[] = {
	{
		RUBY_ENET0_BASE_ADDR,
		RUBY_ENET0_BASE_ADDR,
		TOPAZ_TQE_EMAC_0_PORT,
		RUBY_IRQ_ENET0,
		"arasan_emac0",
	},
	{
		RUBY_ENET1_BASE_ADDR,
		RUBY_ENET0_BASE_ADDR,
		TOPAZ_TQE_EMAC_1_PORT,
		RUBY_IRQ_ENET1,
		"arasan_emac1",
	},
};

static struct net_device *topaz_emacs[ARRAY_SIZE(iflist)];
static int topaz_emac_on[ARRAY_SIZE(iflist)];
static unsigned int topaz_emac_prev_two_connected;
static struct delayed_work topaz_emac_dual_emac_work;

struct topaz_emac_priv {
	struct emac_common com;
	enum topaz_tqe_port tqe_port;
};

static int bonding = 0;
module_param(bonding, int, 0644);
MODULE_PARM_DESC(bonding, "using bonding for emac0 and emac1");

static inline bool is_qtn_oui_packet(unsigned char *pkt_header)
{
	if ((pkt_header[0] == (QTN_OUI & 0xFF)) &&
		(pkt_header[1] == ((QTN_OUI >> 8) & 0xFF)) &&
		(pkt_header[2] == ((QTN_OUI >> 16) & 0xFF)) &&
		(pkt_header[3] >= QTN_OUIE_WIFI_CONTROL_MIN) &&
		(pkt_header[3] <= QTN_OUIE_WIFI_CONTROL_MAX))
		return true;
	else
		return false;
}

static void topaz_emac_start(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;

	/*
	 * These IRQ flags must be cleared when we start as stop_traffic()
	 * relys on them to indicate when activity has stopped.
	 */
	emac_wr(privc, EMAC_DMA_STATUS_IRQ, DmaTxStopped | DmaRxStopped);

	/* Start receive */
	emac_setbits(privc, EMAC_DMA_CTRL, DmaStartRx);
	emac_setbits(privc, EMAC_MAC_RX_CTRL, MacRxEnable);

	/* Start transmit */
	emac_setbits(privc, EMAC_MAC_TX_CTRL, MacTxEnable);
	emac_setbits(privc, EMAC_DMA_CTRL, DmaStartTx);
	emac_wr(privc, EMAC_DMA_TX_AUTO_POLL, 0x200);

	/* Start rxp + txp */
	emac_wr(privc, TOPAZ_EMAC_RXP_CTRL, (TOPAZ_EMAC_RXP_CTRL_ENABLE |
			TOPAZ_EMAC_RXP_CTRL_TQE_SYNC_EN_BP |
			TOPAZ_EMAC_RXP_CTRL_SYNC_TQE));
	emac_wr(privc, TOPAZ_EMAC_TXP_CTRL, TOPAZ_EMAC_TXP_CTRL_AHB_ENABLE);

	/* Clear out EMAC interrupts */
	emac_wr(privc, EMAC_MAC_INT, ~0);
	emac_wr(privc, EMAC_DMA_STATUS_IRQ, ~0);
}

static void topaz_emac_stop(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;

	/* Stop rxp + rxp */
	emac_wr(privc, TOPAZ_EMAC_RXP_CTRL, 0);
	emac_wr(privc, TOPAZ_EMAC_TXP_CTRL, 0);

	/* Stop receive */
	emac_clrbits(privc, EMAC_DMA_CTRL, DmaStartRx);
	emac_clrbits(privc, EMAC_MAC_RX_CTRL, MacRxEnable);

	/* Stop transmit */
	emac_clrbits(privc, EMAC_MAC_TX_CTRL, MacTxEnable);
	emac_clrbits(privc, EMAC_DMA_CTRL, DmaStartTx);
	emac_wr(privc, EMAC_DMA_TX_AUTO_POLL, 0x0);
}

static void topaz_emac_init_rxp_set_default_port(struct emac_common *privc)
{
	union topaz_emac_rxp_outport_ctrl outport;
	union topaz_emac_rxp_outnode_ctrl outnode;

	outport.raw.word0 = 0;
	outnode.raw.word0 = 0;

	/* Lookup priority order: DPI -> VLAN -> IP proto -> FWT */
	outport.data.dpi_prio = 3;
	outport.data.vlan_prio = 2;
	outport.data.da_prio = 0;
	outport.data.ip_prio = 1;
	outport.data.mcast_en = 1;
	outport.data.mcast_port = TOPAZ_TQE_LHOST_PORT;	/* multicast redirect target node */
	outport.data.mcast_sel = 0;	/* 0 = multicast judgement based on emac core status, not DA */
	outport.data.dynamic_fail_port = TOPAZ_TQE_LHOST_PORT;
	outport.data.sw_backdoor_port = TOPAZ_TQE_LHOST_PORT;
	outport.data.static_fail_port = TOPAZ_TQE_LHOST_PORT;
	outport.data.static_port_sel = 0;
	outport.data.static_mode_en = 0;

	outnode.data.mcast_node = 0;
	outnode.data.dynamic_fail_node = 0;
	outnode.data.sw_backdoor_node = 0;
	outnode.data.static_fail_node = 0;
	outnode.data.static_node_sel = 0;

	emac_wr(privc, TOPAZ_EMAC_RXP_OUTPORT_CTRL, outport.raw.word0);
	emac_wr(privc, TOPAZ_EMAC_RXP_OUTNODE_CTRL, outnode.raw.word0);
}

static void topaz_emac_init_rxp_dscp(struct emac_common *privc)
{
	uint8_t dscp_reg_index;

	/*
	 * EMAC RXP has 8 registers for DSCP -> TID mappings. Each register has 8 nibbles;
	 * a single nibble corresponds to a particular DSCP that could be seen in a packet.
	 * There are 64 different possible DSCP values (6 DSCP bits).
	 * For example, register 0's nibbles correspond to:
	 * Reg mask 0x0000000f -> DSCP 0x0. Mask is ANDed with the desired TID for DSCP 0x0.
	 * Reg mask 0x000000f0 -> DSCP 0x1
	 * ...
	 * Reg mask 0xf0000000 -> DSCP 0x7
	 * Next register is used for DSCP 0x8 - 0xf.
	 */
	for (dscp_reg_index = 0; dscp_reg_index < TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REGS; dscp_reg_index++) {
		uint8_t dscp_nibble_index;
		uint32_t dscp_reg_val = 0;

		for (dscp_nibble_index = 0; dscp_nibble_index < 8; dscp_nibble_index++) {
			const uint8_t dscp = dscp_reg_index * 8 + dscp_nibble_index;
			const uint8_t tid =  qdrv_dscp2tid_default(dscp);
			dscp_reg_val |= (tid & 0xF) << (4 * dscp_nibble_index);
		}

		emac_wr(privc, TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REG(dscp_reg_index), dscp_reg_val);
	}
}

static void topaz_emac_init_rxptxp(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;

	emac_wr(privc, TOPAZ_EMAC_RXP_CTRL, 0);
	emac_wr(privc, TOPAZ_EMAC_TXP_CTRL, 0);

	topaz_emac_init_rxp_set_default_port(privc);
	topaz_emac_init_rxp_dscp(privc);

	emac_wr(privc, TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID,
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(0, 0) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(1, 1) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(2, 0) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(3, 5) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(4, 5) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(5, 6) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(6, 6) |
			TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID_PRI(7, 6));

	emac_wr(privc, TOPAZ_EMAC_RXP_PRIO_CTRL,
			SM(TOPAZ_EMAC_RXP_PRIO_IS_DSCP, TOPAZ_EMAC_RXP_PRIO_CTRL_TID_SEL));

	emac_wr(privc, TOPAZ_EMAC_BUFFER_POOLS,
		SM(TOPAZ_HBM_BUF_EMAC_RX_POOL, TOPAZ_EMAC_BUFFER_POOLS_RX_REPLENISH) |
		SM(TOPAZ_HBM_EMAC_TX_DONE_POOL, TOPAZ_EMAC_BUFFER_POOLS_TX_RETURN));

	emac_wr(privc, TOPAZ_EMAC_DESC_LIMIT, privc->tx.desc_count);

	qdrv_dscp2tid_map_init();
}

static void topaz_emac_enable_ints(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;

	/* Clear any pending interrupts */
	emac_wr(privc, EMAC_MAC_INT, emac_rd(privc, EMAC_MAC_INT));
	emac_wr(privc, EMAC_DMA_STATUS_IRQ, emac_rd(privc, EMAC_DMA_STATUS_IRQ));
}

static void topaz_emac_disable_ints(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;

	emac_wr(privc, EMAC_MAC_INT_ENABLE, 0);
	emac_wr(privc, EMAC_DMA_INT_ENABLE, 0);

	emac_wr(privc, EMAC_MAC_INT, ~0x0);
	emac_wr(privc, EMAC_DMA_STATUS_IRQ, ~0x0);
}

static int topaz_emac_ndo_open(struct net_device *dev)
{

	emac_lib_set_rx_mode(dev);
	topaz_emac_start(dev);
	emac_lib_pm_emac_add_notifier(dev);
	topaz_emac_enable_ints(dev);
	emac_lib_phy_start(dev);
	netif_start_queue(dev);

	eth_ifindex[dev->if_port] = dev->ifindex;
	return 0;
}


static int topaz_emac_ndo_stop(struct net_device *dev)
{
	topaz_emac_disable_ints(dev);
	emac_lib_pm_emac_remove_notifier(dev);
	netif_stop_queue(dev);
	emac_lib_phy_stop(dev);
	topaz_emac_stop(dev);

	return 0;
}

static int topaz_emac_ndo_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	union topaz_tqe_cpuif_ppctl ctl;
	int8_t pool = TOPAZ_HBM_EMAC_TX_DONE_POOL;
	int interface;
	struct sk_buff *skb2;
	struct qtn_vlan_dev *vdev = vport_tbl_lhost[priv->tqe_port];

	for (interface = 0; interface < EMAC_MAX_INTERFACE; interface++){
		/* In order to drop a packet, the following conditions has to be met:
		 * emac_xflow_disable == 1
		 * skb->skb_iif has to be none zero
		 * skb->skb_iif the interface is Rx from emac0 or emac1
		 */

		if ((emac_xflow_disable) && (skb->skb_iif) &&
		    ((skb->skb_iif) == eth_ifindex[interface])){
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
        }

	/*
	 * restore VLAN tag to packet if needed
	 */
	skb2 = switch_vlan_from_proto_stack(skb, vdev, 0, 1);
	if (!skb2)
		return NETDEV_TX_OK;

	if (vlan_enabled && !qtn_vlan_egress(vdev, 0, skb2->data)) {
		dev_kfree_skb(skb2);
		return NETDEV_TX_OK;
	}

	/* drop any WBSP control packet towards emac1 (Ethernet type 88b7)
	Quantenna OUI (00 26 86) is located at data[14-16] followed by 1-byte type field [17] */
	if ((emac_wbsp_ctrl == EMAC_WBSP_CTRL_ENABLED && dev->ifindex == eth_ifindex[1]) ||
		(emac_wbsp_ctrl == EMAC_WBSP_CTRL_SWAPPED && dev->ifindex == eth_ifindex[0])) {
		if (skb2->protocol == __constant_htons(ETHERTYPE_802A) &&
			skb2->len > 17 && is_qtn_oui_packet(&skb2->data[14])) {
			dev_kfree_skb(skb2);
			return NETDEV_TX_OK;
		}
	}

	topaz_tqe_cpuif_ppctl_init(&ctl,
			priv->tqe_port, NULL, 1, 0,
			0, 1, pool, 1, 0);

	return tqe_tx(&ctl, skb2);
}

static const struct net_device_ops topaz_emac_ndo = {
	.ndo_open = topaz_emac_ndo_open,
	.ndo_stop = topaz_emac_ndo_stop,
	.ndo_start_xmit = topaz_emac_ndo_start_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_set_multicast_list = emac_lib_set_rx_mode,
	.ndo_get_stats = emac_lib_stats,
	.ndo_do_ioctl = emac_lib_ioctl,
};

static void emac_bufs_free(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;
	int i;

	for (i = 0; i < privc->rx.desc_count; i++) {
		if (privc->rx.descs[i].bufaddr1) {
			topaz_hbm_put_payload_realign_bus((void *) privc->rx.descs[i].bufaddr1,
					TOPAZ_HBM_BUF_EMAC_RX_POOL);
		}
	}
}

int topaz_emac_descs_init(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;
	int i;
	struct emac_desc __iomem *rx_bus_descs = (void *)privc->rx.descs_dma_addr;
	struct emac_desc __iomem *tx_bus_descs = (void *)privc->tx.descs_dma_addr;

	for (i = 0; i < privc->rx.desc_count; i++) {
		unsigned long ctrl;
		int bufsize;
		void * buf_bus;

		bufsize = TOPAZ_HBM_BUF_EMAC_RX_SIZE
			- TOPAZ_HBM_PAYLOAD_HEADROOM
			- SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
		buf_bus = topaz_hbm_get_payload_bus(TOPAZ_HBM_BUF_EMAC_RX_POOL);
		if (unlikely(!buf_bus)) {
			printk("%s: buf alloc error buf 0x%p\n", __FUNCTION__, buf_bus);
			return -1;
		}

		ctrl = min(bufsize, RxDescBuf1SizeMask) << RxDescBuf1SizeShift;
		ctrl |= RxDescChain2ndAddr;

		privc->rx.descs[i].status = RxDescOwn;
		privc->rx.descs[i].control = ctrl;
		privc->rx.descs[i].bufaddr1 = (unsigned long)buf_bus;
		privc->rx.descs[i].bufaddr2 = (unsigned long)&rx_bus_descs[(i + 1) % privc->rx.desc_count];
	}

	for (i = 0; i < privc->tx.desc_count; i++) {
		/*
		 * For each transmitted buffer, TQE will update:
		 * - tdes1 (control) according to TOPAZ_TQE_EMAC_TDES_1_CNTL & payload size
		 * - tdes2 (bufaddr1) with payload dma address
		 * So initializing these fields here is meaningless
		 */
		privc->tx.descs[i].status = 0x0;
		privc->tx.descs[i].control = 0x0;
		privc->tx.descs[i].bufaddr1 = 0x0;
		privc->tx.descs[i].bufaddr2 = (unsigned long)&tx_bus_descs[(i + 1) % privc->tx.desc_count];
	}

	return 0;
}

static void topaz_emac_set_eth_addr(struct net_device *dev, int port_num)
{
	memcpy(dev->dev_addr, get_ethernet_addr(), ETH_ALEN);

	if (port_num > 0) {
		u32 val;

		val = (u32)dev->dev_addr[5] +
			((u32)dev->dev_addr[4] << 8) +
			((u32)dev->dev_addr[3] << 16);
		val += port_num;
		dev->dev_addr[5] = (unsigned char)val;
		dev->dev_addr[4] = (unsigned char)(val >> 8);
		dev->dev_addr[3] = (unsigned char)(val >> 16);
	}
}

static int topaz_emac_proc_rd(char *buf, char **start, off_t offset, int count,
		int *eof, void *data)
{
	char *p = buf;
	struct net_device *dev = data;

	p += emac_lib_stats_sprintf(p, dev);

	*eof = 1;

	return p - buf;
}

static ssize_t topaz_emac_vlan_sel_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct emac_common *privc = netdev_priv(to_net_dev(dev));
	uint32_t reg = emac_rd(privc, TOPAZ_EMAC_RXP_VLAN_PRI_CTRL);

	return sprintf(buf, "%u\n", MS(reg, TOPAZ_EMAC_RXP_VLAN_PRI_CTRL_TAG));
}

static ssize_t topaz_emac_vlan_sel_sysfs_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct emac_common *privc = netdev_priv(to_net_dev(dev));
	uint8_t tag_sel;

	if (sscanf(buf, "%hhu", &tag_sel) == 1) {
		emac_wr(privc, TOPAZ_EMAC_RXP_VLAN_PRI_CTRL,
				SM(tag_sel, TOPAZ_EMAC_RXP_VLAN_PRI_CTRL_TAG));
	}
	return count;
}

static ssize_t topaz_emac_dscp_sysfs_update(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{

	uint32_t dscp = 0;
	uint8_t dscp_reg_index;
	uint8_t dscp_nibble_index;
	struct emac_common *privc = netdev_priv(to_net_dev(dev));

	dscp_reg_index        = (dscp_priority / 8);
	dscp_nibble_index     = (dscp_priority % 8);

	if (buf[0] == 'U' || buf[0] == 'u'){
		dscp = emac_rd(privc, TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REG(dscp_reg_index));
		dscp &= ~((0xF) << (4 * dscp_nibble_index));
		dscp |= (dscp_value & 0xF) << (4 * dscp_nibble_index);
		emac_wr(privc, TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REG(dscp_reg_index), dscp);
		g_dscp_value[privc->mac_id] = dscp_value & 0xFF;
		g_dscp_flag = 1;
	}
	return count;
}

static ssize_t topaz_emac_dscp_prio_val_sysfs_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{

	long num;
	num = simple_strtol(buf, NULL, 10);
	if (num < 0 || num >= 15)
		return -EINVAL;
	dscp_value = num;
	return count;
}

static ssize_t topaz_emac_dscp_prio_sel_sysfs_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{

	long num;
	num = simple_strtol(buf, NULL, 10);
	if (num < QTN_DSCP_MIN || num > QTN_DSCP_MAX)
		return -EINVAL;
	dscp_priority = num;
	return count;
}

static ssize_t topaz_emac_dscp_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t dscp = 0;
	uint8_t dscp_reg_index;
	char *p = 0;
	int index = 0;
	uint8_t dscp_nibble_index;
	struct emac_common *privc = netdev_priv(to_net_dev(dev));

	p = buf;
	p += sprintf(p, "%s\n", "DSCP TABLE:");
	for (dscp_reg_index = 0; dscp_reg_index < TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REGS; dscp_reg_index++) {
		dscp = emac_rd(privc, TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REG(dscp_reg_index));
		for (dscp_nibble_index = 0; dscp_nibble_index < 8; dscp_nibble_index++) {
			p += sprintf(p, "Index \t %d: \t Data %x\n", index++, (dscp & 0xF));
			dscp >>= 0x4;
		}
	}
	return (int)(p - buf);
}

static ssize_t topaz_emacx_xflow_sysfs_show(struct device *dev, struct device_attribute *attr,
						char *buff)
{
	int count = 0;

	count += sprintf(buff + count, "%d\n", emac_xflow_disable);

	return count;
}

static ssize_t topaz_emacx_xflow_sysfs_update(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
        if (buf[0] == 'D' || buf[0] == 'd'){
		emac_xflow_disable = 1;
        } else if (buf[0] == 'E' || buf[0] == 'e'){
		emac_xflow_disable = 0;
	}
        return count;
}


static DEVICE_ATTR(device_dscp_update, S_IWUGO,
		NULL, topaz_emac_dscp_sysfs_update);

static int topaz_emac_dscp_update_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_device_dscp_update.attr);
}

static DEVICE_ATTR(device_emacx_xflow_update, S_IRUGO | S_IWUGO,
		topaz_emacx_xflow_sysfs_show, topaz_emacx_xflow_sysfs_update);

static int topaz_emacs_update_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_device_emacx_xflow_update.attr);
}

static DEVICE_ATTR(device_dscp_prio_val, S_IWUGO,
		NULL, topaz_emac_dscp_prio_val_sysfs_store);

static int topaz_emac_dscp_prio_val_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_device_dscp_prio_val.attr);
}

static DEVICE_ATTR(device_dscp_prio_sel, S_IWUGO,
		NULL, topaz_emac_dscp_prio_sel_sysfs_store);

static int topaz_emac_dscp_prio_sel_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_device_dscp_prio_sel.attr);
}

static DEVICE_ATTR(device_dscp_show, S_IRUGO,
		topaz_emac_dscp_sysfs_show, NULL);

static int topaz_emac_dscp_show_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_device_dscp_show.attr);
}

static DEVICE_ATTR(vlan_sel, S_IRUGO | S_IWUGO,
		topaz_emac_vlan_sel_sysfs_show, topaz_emac_vlan_sel_sysfs_store);

static int topaz_emac_vlan_sel_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_vlan_sel.attr);
}

static void topaz_emac_vlan_sel_sysfs_remove(struct net_device *net_dev)
{
	sysfs_remove_file(&net_dev->dev.kobj, &dev_attr_vlan_sel.attr);
}

static uint8_t topaz_emac_fwt_sw_remap_port(uint8_t in_port, const uint8_t *mac_be)
{
	return mac_be[5] % 2;
}

static ssize_t topaz_emacx_wbsp_ctrl_sysfs_show(struct device *dev, struct device_attribute *attr,
						char *buff)
{
	int count = 0;

	count += sprintf(buff + count, "%d\n", emac_wbsp_ctrl);

	return count;
}

static ssize_t topaz_emacx_wbsp_ctrl_sysfs_update(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
        if (buf[0] == '0') {
		emac_wbsp_ctrl = EMAC_WBSP_CTRL_DISABLED;
        } else if (buf[0] == '1') {
		emac_wbsp_ctrl = EMAC_WBSP_CTRL_ENABLED;
	} else if (buf[0] == '2') {
		emac_wbsp_ctrl = EMAC_WBSP_CTRL_SWAPPED;
	}
        return count;
}

static DEVICE_ATTR(device_emacx_wbsp_ctrl, S_IRUGO | S_IWUGO,
		topaz_emacx_wbsp_ctrl_sysfs_show, topaz_emacx_wbsp_ctrl_sysfs_update);

static int topaz_emacs_wbsp_ctrl_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_device_emacx_wbsp_ctrl.attr);
}

static void topaz_emac_tqe_rx_handler(void *token,
		const union topaz_tqe_cpuif_descr *descr,
		struct sk_buff *skb, uint8_t *whole_frm_hdr)
{
	struct net_device *dev = token;

	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, skb->dev);

	/* discard WBSP control packet coming from emac1 (Ethernet type 88b7)
	Note that in this receive routine, header has been removed from data buffer, so
	Quantenna OUI (00 26 86) is now located at data[0-2] followed by 1-byte type field [3] */
	if ((emac_wbsp_ctrl == EMAC_WBSP_CTRL_ENABLED && dev->ifindex == eth_ifindex[1]) ||
		(emac_wbsp_ctrl == EMAC_WBSP_CTRL_SWAPPED && dev->ifindex == eth_ifindex[0])) {
		if (skb->protocol == __constant_htons(ETHERTYPE_802A) &&
			skb->len > 3 && is_qtn_oui_packet(&skb->data[0])) {
			dev_kfree_skb(skb);
			return;
		}
	}

	skb = switch_vlan_to_proto_stack(skb, 0);
	if (skb)
		netif_receive_skb(skb);
}

#ifdef TOPAZ_EMAC_NULL_BUF_WR
static inline void topaz_hbm_emac_rx_pool_intr_init(void)
{
	uint32_t tmp;

	tmp = readl(TOPAZ_HBM_CSR_REG);
	tmp |= TOPAZ_HBM_CSR_INT_EN | TOPAZ_HBM_CSR_Q_EN(TOPAZ_HBM_BUF_EMAC_RX_POOL);
	writel(tmp, TOPAZ_HBM_CSR_REG);

	tmp = readl(RUBY_SYS_CTL_LHOST_ORINT_EN);
	tmp |= TOPAZ_HBM_INT_EN;
	writel(tmp, RUBY_SYS_CTL_LHOST_ORINT_EN);
}

static inline void topaz_hbm_emac_rx_pool_uf_intr_en(void)
{
	uint32_t tmp = readl(TOPAZ_HBM_CSR_REG);

	tmp &= ~(TOPAZ_HBM_CSR_INT_MSK_RAW);
	tmp |= TOPAZ_HBM_CSR_UFLOW_INT_MASK(TOPAZ_HBM_BUF_EMAC_RX_POOL) |\
		TOPAZ_HBM_CSR_UFLOW_INT_RAW(TOPAZ_HBM_BUF_EMAC_RX_POOL) |\
		TOPAZ_HBM_CSR_INT_EN;
	writel(tmp, TOPAZ_HBM_CSR_REG);
}


static inline int topaz_emac_rx_null_buf_del(struct emac_common *privc, int budget)
{
	uint32_t i;
	uint32_t ei;

	ei = (struct emac_desc *)emac_rd(privc, EMAC_DMA_CUR_RXDESC_PTR)
		- (struct emac_desc *)privc->rx.descs_dma_addr;

	for (i = (ei - budget) % QTN_BUFS_EMAC_RX_RING;
			i != ei; i = (i + 1) % QTN_BUFS_EMAC_RX_RING) {
		if (privc->rx.descs[i].status & RxDescOwn) {
			if (!privc->rx.descs[i].bufaddr1) {
				uint32_t buf_bus;
				buf_bus = (uint32_t)topaz_hbm_get_payload_bus(TOPAZ_HBM_BUF_EMAC_RX_POOL);
				if (buf_bus)
					privc->rx.descs[i].bufaddr1 = buf_bus;
				 else
					return -1;
			}
		}
	}

	return 0;
}

static void topaz_emac_set_outport(struct net_device *ndev, uint32_t enable)
{
#define TOPAZ_EMAC_OUTPORT_ENABLE	1
#define TOPAZ_EMAC_OUTPORT_DISABLE	0
	struct emac_common *privc;
	union topaz_emac_rxp_outport_ctrl outport;
	unsigned long flags;

	privc = netdev_priv(ndev);

	local_irq_save(flags);
	outport.raw.word0 = emac_rd(privc, TOPAZ_EMAC_RXP_OUTPORT_CTRL);
	if (enable) {
		outport.data.static_port_sel = TOPAZ_TQE_LHOST_PORT;
		outport.data.static_mode_en = TOPAZ_EMAC_OUTPORT_ENABLE;
	} else {
		outport.data.static_port_sel = 0;
		outport.data.static_mode_en = TOPAZ_EMAC_OUTPORT_DISABLE;
	}
	emac_wr(privc, TOPAZ_EMAC_RXP_OUTPORT_CTRL, outport.raw.word0);
	local_irq_restore(flags);
}

void topaz_emac_to_lhost(uint32_t enable)
{
	struct net_device *dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(iflist); i++) {
		dev = topaz_emacs[i];
		if (dev)
			topaz_emac_set_outport(dev, enable);
	}
}
EXPORT_SYMBOL(topaz_emac_to_lhost);

int topaz_emac_get_bonding()
{
	return bonding;
}
EXPORT_SYMBOL(topaz_emac_get_bonding);

static inline void topaz_emac_stop_rx(void)
{
	struct net_device *dev;
	struct topaz_emac_priv *priv;
	struct emac_common *privc;
	int i;

	/* Stop the emac, try to take the null buffer off and refill with new buffer */
	for (i = 0; i < ARRAY_SIZE(iflist); i++) {
		dev = topaz_emacs[i];
		if (dev) {
			priv = netdev_priv(dev);
			privc = &priv->com;
			/* Stop rxp + emac rx */
			emac_wr(privc, TOPAZ_EMAC_RXP_CTRL, 0);
			emac_clrbits(privc, EMAC_MAC_RX_CTRL, MacRxEnable);
		}
	}
}

static inline void topaz_emac_start_rx(void)
{
	struct net_device *dev;
	struct topaz_emac_priv *priv;
	struct emac_common *privc;
	int i;

	/* Stop the emac, try to take the null buffer off and refill with new buffer */
	for (i = 0; i < ARRAY_SIZE(iflist); i++) {
		dev = topaz_emacs[i];
		if (dev) {
			priv = netdev_priv(dev);
			privc = &priv->com;
			/* Start rxp + emac rx */
			emac_setbits(privc, EMAC_MAC_RX_CTRL, MacRxEnable);
			emac_wr(privc, TOPAZ_EMAC_RXP_CTRL, (TOPAZ_EMAC_RXP_CTRL_ENABLE |
				TOPAZ_EMAC_RXP_CTRL_TQE_SYNC_EN_BP |
				TOPAZ_EMAC_RXP_CTRL_SYNC_TQE));
		}
	}
}

void  __attribute__((section(".sram.text"))) topaz_emac_null_buf_del(void)
{
	struct net_device *dev;
	struct topaz_emac_priv *priv;
	struct emac_common *privc;
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(iflist); i++) {
		dev = topaz_emacs[i];
		if (dev) {
			priv = netdev_priv(dev);
			privc = &priv->com;
			ret += topaz_emac_rx_null_buf_del(privc, TOPAZ_HBM_BUF_EMAC_RX_COUNT - 1);
		}
	}

	if (ret == 0) {
		topaz_hbm_emac_rx_pool_uf_intr_en();
		topaz_emac_start_rx();
		topaz_emac_null_buf_del_cb = NULL;
	}
}


static irqreturn_t __attribute__((section(".sram.text"))) topaz_hbm_handler(int irq, void *dev_id)
{
	uint32_t tmp;

	tmp = readl(TOPAZ_HBM_CSR_REG);
	if (tmp & TOPAZ_HBM_CSR_UFLOW_INT_RAW(TOPAZ_HBM_BUF_EMAC_RX_POOL)) {
		topaz_emac_stop_rx();
		tmp &= ~(TOPAZ_HBM_CSR_INT_MSK_RAW &
			~(TOPAZ_HBM_CSR_UFLOW_INT_RAW(TOPAZ_HBM_BUF_EMAC_RX_POOL)));
		tmp &= ~(TOPAZ_HBM_CSR_INT_EN);
		writel(tmp, TOPAZ_HBM_CSR_REG);
		topaz_emac_null_buf_del_cb = topaz_emac_null_buf_del;
	}
	return IRQ_HANDLED;
}
#endif

static void __init topaz_dpi_filter_arp(int port_num)
{
	struct topaz_dpi_filter_request req;
	struct topaz_dpi_field_def field;

	/* ARP */
	memset(&req, 0, sizeof(req));
	memset(&field, 0, sizeof(field));

	req.fields = &field;
	req.field_count = 1;
	req.out_port = TOPAZ_TQE_LHOST_PORT;
	req.out_node = 0;
	req.tid = 0;

	field.ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field.ctrl.data.anchor = TOPAZ_DPI_ANCHOR_FRAME_START;
	field.ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field.ctrl.data.offset = 3; /* ethhdr->h_proto: ETH_ALEN*2/sizeof(dword)*/
	field.val = (ETH_P_ARP << 16);
	field.mask = 0xffff0000;

        topaz_dpi_filter_add(port_num, &req);

	/* 8021Q && ARP */
	memset(&req, 0, sizeof(req));
	memset(&field, 0, sizeof(field));

	req.fields = &field;
	req.field_count = 1;
	req.out_port = TOPAZ_TQE_LHOST_PORT;

	field.ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field.ctrl.data.anchor = TOPAZ_DPI_ANCHOR_VLAN0;
	field.ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field.ctrl.data.offset = 1;
	field.val = (ETH_P_ARP << 16);
	field.mask = 0xffff0000;

	topaz_dpi_filter_add(port_num, &req);
}

static void __init topaz_dpi_filter_dhcp(int port_num)
{
	struct topaz_dpi_filter_request req;
	struct topaz_dpi_field_def field[2];

	/* IPv4 && UDP && srcport == 67 && dstport == 68 */

	memset(&req, 0, sizeof(req));
	memset(field, 0, sizeof(field));

	req.fields = field;
	req.field_count = 2;
	req.out_port = TOPAZ_TQE_LHOST_PORT;
	req.out_node = 0;
	req.tid = 0;

	field[0].ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field[0].ctrl.data.anchor = TOPAZ_DPI_ANCHOR_IPV4;
	field[0].ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field[0].ctrl.data.offset = 0;
	field[0].val = IPUTIL_HDR_VER_4 << 28;
	field[0].mask = 0xf0000000;

	field[1].ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field[1].ctrl.data.anchor = TOPAZ_DPI_ANCHOR_UDP;
	field[1].ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field[1].ctrl.data.offset = 0;	/* src_port/dst_port */
	field[1].val = (DHCPSERVER_PORT << 16) | DHCPCLIENT_PORT;
	field[1].mask = 0xffffffff;

	topaz_dpi_filter_add(port_num, &req);
}

#ifdef CONFIG_IPV6
static void __init topaz_dpi_filter_dhcpv6(int port_num)
{
	struct topaz_dpi_filter_request req;
	struct topaz_dpi_field_def field[2];

	/* IPv6 && UDP && srcport == 547 && dstport == 546 */

	memset(&req, 0, sizeof(req));
	memset(field, 0, sizeof(field));

	req.fields = field;
	req.field_count = 2;
	req.out_port = TOPAZ_TQE_LHOST_PORT;

	field[0].ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field[0].ctrl.data.anchor = TOPAZ_DPI_ANCHOR_IPV6;
	field[0].ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field[0].ctrl.data.offset = 1;
	field[0].val = (uint32_t)(IPPROTO_UDP << 8);
	field[0].mask = (uint32_t)(0xff << 8);

	field[1].ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field[1].ctrl.data.anchor = TOPAZ_DPI_ANCHOR_UDP;
	field[1].ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field[1].ctrl.data.offset = 0;
	field[1].val = (DHCPV6SERVER_PORT << 16) | DHCPV6CLIENT_PORT;
	field[1].mask = 0xffffffff;

	topaz_dpi_filter_add(port_num, &req);
}

static void __init topaz_dpi_filter_icmpv6(int port_num)
{
	struct topaz_dpi_filter_request req;
	struct topaz_dpi_field_def field;

        /* IPv6 && ICMPv6 */

	memset(&req, 0, sizeof(req));
	memset(&field, 0, sizeof(field));

	req.fields = &field;
	req.field_count = 1;
	req.out_port = TOPAZ_TQE_LHOST_PORT;

	field.ctrl.data.enable = TOPAZ_DPI_ENABLE;
	field.ctrl.data.anchor = TOPAZ_DPI_ANCHOR_IPV6;
	field.ctrl.data.cmp_op = TOPAZ_DPI_CMPOP_EQ;
	field.ctrl.data.offset = 1;
	field.val = (uint32_t)(IPPROTO_ICMPV6 << 8);
	field.mask = (uint32_t)(0xff << 8);

	topaz_dpi_filter_add(port_num, &req);
}
#endif /* CONFIG_IPV6 */

static struct net_device * __init topaz_emac_init(int port_num)
{
	const struct emac_port_info * const port = &iflist[port_num];
	struct topaz_emac_priv *priv = NULL;
	struct emac_common *privc = NULL;
	struct net_device *dev = NULL;
	int rc;
	int emac_cfg;
	int emac_phy;
	char devname[IFNAMSIZ + 1];

	printk(KERN_INFO "%s, emac%d\n", __FUNCTION__, port_num);

	if (emac_lib_board_cfg(port_num, &emac_cfg, &emac_phy)) {
		return NULL;
	}

	if ((emac_cfg & EMAC_IN_USE) == 0) {
		return NULL;
	}

	/* Allocate device structure */
	sprintf(devname, "eth%d_emac%d", soc_id(), port_num);
	dev = alloc_netdev(sizeof(struct topaz_emac_priv), devname, ether_setup);
	if (!dev) {
		printk(KERN_ERR "%s: alloc_netdev failed\n", __FUNCTION__);
		return NULL;
	}

	/* Initialize device structure fields */
	dev->netdev_ops = &topaz_emac_ndo;
	dev->tx_queue_len = 8;
	dev->irq = port->irq;
	SET_ETHTOOL_OPS(dev, &emac_lib_ethtool_ops);
	topaz_emac_set_eth_addr(dev, port_num);

	/* Initialize private data */
	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(*priv));
	priv->tqe_port = port->tqe_port;
	privc = &priv->com;
	privc->dev = dev;
	privc->mac_id = port_num;
	privc->vbase = port->base_addr;
	privc->mdio_vbase = port->mdio_base_addr;
	privc->emac_cfg = emac_cfg;
	privc->phy_addr = emac_phy;

	/* Map the TQE port to the device port */
	dev->if_port = port->tqe_port;
	/* Initialize MII */
	if (emac_lib_mii_init(dev)) {
		goto mii_init_error;
	}

	/* Allocate descs & buffers */
	if (emac_lib_descs_alloc(dev,
				QTN_BUFS_EMAC_RX_RING, EMAC_DESC_USE_SRAM,
				QTN_BUFS_EMAC_TX_RING, EMAC_DESC_USE_SRAM)) {
		goto descs_alloc_error;
	}
	if (topaz_emac_descs_init(dev)) {
		goto bufs_alloc_error;
	}

	/* Register device */
	if ((rc = register_netdev(dev)) != 0) {
		printk(KERN_ERR "%s: register_netdev returns %d\n", __FUNCTION__, rc);
		goto netdev_register_error;
	}

	BUG_ON(priv->tqe_port != port_num);

	if (switch_alloc_vlan_dev(port_num, EMAC_VDEV_IDX(port_num), dev->ifindex) == NULL) {
		printk(KERN_ERR "%s: switch_alloc_vlan_dev returns error\n", __FUNCTION__);
		goto tqe_register_error;
	}

	if (tqe_port_add_handler(port->tqe_port, &topaz_emac_tqe_rx_handler, dev)) {
		printk(KERN_ERR "%s: topaz_port_add_handler returns error\n", __FUNCTION__);
		goto switch_vlan_alloc_error;
	}

	/* Send EMAC through soft reset */
	emac_wr(privc, EMAC_DMA_CONFIG, DmaSoftReset);
	udelay(1000);
	emac_wr(privc, EMAC_DMA_CONFIG, 0);

	topaz_ipprt_clear_all_entries(port_num);
	topaz_ipprt_set(port_num, IPPROTO_IGMP, TOPAZ_TQE_LHOST_PORT, 0);
#if defined (TOPAZ_VB_CONFIG)
	topaz_ipprt_set(port_num, IPPROTO_ICMP, TOPAZ_TQE_LHOST_PORT, 0);
	topaz_ipprt_set(port_num, IPPROTO_TCP, TOPAZ_TQE_LHOST_PORT, 0);
#endif
	topaz_dpi_init(port_num);
	topaz_dpi_filter_arp(port_num);
	topaz_dpi_filter_dhcp(port_num);
#ifdef CONFIG_IPV6
	topaz_dpi_filter_dhcpv6(port_num);
	topaz_dpi_filter_icmpv6(port_num);
#endif
	emac_lib_init_dma(privc);
	emac_lib_init_mac(dev);
	topaz_emac_init_rxptxp(dev);

	topaz_emac_ndo_stop(dev);

	create_proc_read_entry(port->proc_name, 0, NULL, topaz_emac_proc_rd, dev);
	emac_lib_phy_power_create_proc(dev);
	emac_lib_mdio_sysfs_create(dev);
	topaz_emac_vlan_sel_sysfs_create(dev);
	emac_lib_phy_reg_create_proc(dev);

	if (bonding) {
		fwt_sw_register_port_remapper(port->tqe_port, topaz_emac_fwt_sw_remap_port);
		tqe_port_set_group(port->tqe_port, EMAC_BONDING_GROUP);
		/* Don't multicast to both ports if they are bonded */
		if (port->tqe_port == TOPAZ_TQE_EMAC_0_PORT)
			tqe_port_register(port->tqe_port);
	} else {
		tqe_port_register(port->tqe_port);
	}

	/* Create sysfs for dscp misc operations */

	topaz_emac_dscp_show_sysfs_create(dev);
	topaz_emac_dscp_prio_sel_sysfs_create(dev);
	topaz_emac_dscp_prio_val_sysfs_create(dev);
	topaz_emac_dscp_update_sysfs_create(dev);
	topaz_emacs_update_sysfs_create(dev);

	topaz_emacs_wbsp_ctrl_sysfs_create(dev);

	return dev;

switch_vlan_alloc_error:
	switch_free_vlan_dev_by_idx(EMAC_VDEV_IDX(port_num));
tqe_register_error:
	unregister_netdev(dev);
netdev_register_error:
	emac_bufs_free(dev);
bufs_alloc_error:
	emac_lib_descs_free(dev);
descs_alloc_error:
	emac_lib_mii_exit(dev);
mii_init_error:
	free_netdev(dev);

	return NULL;
}

static void __exit topaz_emac_exit(struct net_device *dev)
{
	struct topaz_emac_priv *priv = netdev_priv(dev);
	struct emac_common *privc = &priv->com;
	const struct emac_port_info * const port = &iflist[privc->mac_id];

	topaz_emac_ndo_stop(dev);

	emac_lib_phy_reg_remove_proc(dev);
	topaz_emac_vlan_sel_sysfs_remove(dev);
	emac_lib_mdio_sysfs_remove(dev);
	emac_lib_phy_power_remove_proc(dev);
	remove_proc_entry(port->proc_name, NULL);

	tqe_port_unregister(priv->tqe_port);
	tqe_port_remove_handler(priv->tqe_port);
	switch_free_vlan_dev_by_idx(EMAC_VDEV_IDX(privc->mac_id));
	unregister_netdev(dev);

	emac_bufs_free(dev);
	emac_lib_descs_free(dev);
	emac_lib_mii_exit(dev);

	free_netdev(dev);
}

static void __init topaz_emac_init_tqe(void)
{
	uint32_t tdes = TxDescIntOnComplete | TxDescFirstSeg | TxDescLastSeg | TxDescChain2ndAddr;
	uint32_t ctrl = 0;

	ctrl |= SM(tdes >> TOPAZ_TQE_EMAC_TDES_1_CNTL_SHIFT, TOPAZ_TQE_EMAC_TDES_1_CNTL_VAL);
	ctrl |= SM(1, TOPAZ_TQE_EMAC_TDES_1_CNTL_MCAST_APPEND_CNTR_EN);

	writel(ctrl, TOPAZ_TQE_EMAC_TDES_1_CNTL);
}

static int topaz_emac_find_emac(const struct net_device *const dev)
{
	int idx = 0;

	while (idx < ARRAY_SIZE(iflist)) {
		if (topaz_emacs[idx] == dev)
			return idx;
		++idx;
	}

	return -1;
}

static int topaz_emacs_connected_num(void)
{
	int idx = 0;
	int in_use = 0;

	while (idx < ARRAY_SIZE(iflist)) {
		if (topaz_emac_on[idx])
			++in_use;
		++idx;
	}

	return in_use;
}

#define TOPAZ_EMAC_LINK_CHECK_PERIOD	5
static int topaz_emac_link_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	unsigned int two_emacs_connected;
	struct net_device *dev = ptr;
	int emac_idx;

	if (event != NETDEV_CHANGE) {
		return NOTIFY_DONE;
	}

	emac_idx = topaz_emac_find_emac(dev);
	if (emac_idx < 0)
		return NOTIFY_DONE;

	topaz_emac_on[emac_idx] = netif_carrier_ok(dev);

	two_emacs_connected = (topaz_emacs_connected_num() > 1);
	if (topaz_emac_prev_two_connected != two_emacs_connected) {
		pm_flush_work(&topaz_emac_dual_emac_work);
		topaz_emac_prev_two_connected = two_emacs_connected;
		pm_queue_work(&topaz_emac_dual_emac_work, HZ * TOPAZ_EMAC_LINK_CHECK_PERIOD);
	}

	return NOTIFY_DONE;
}

void topaz_emac_work_fn(struct work_struct *work)
{
	emac_lib_update_link_vars(topaz_emac_prev_two_connected);
}

static struct notifier_block topaz_link_notifier = {
	.notifier_call = topaz_emac_link_event,
};

static int __init topaz_emac_module_init(void)
{
	int i;
	int found = 0;
	int emac_cfg_p0, emac_cfg_p1;
	int emac_phy;

	printk("emac wbsp: %d\n", emac_wbsp_ctrl);

	if (!TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT) {
		printk(KERN_ERR "%s: switch_emac should be used with topaz hbm skb allocator only\n", __FUNCTION__);
	}
#ifdef TOPAZ_EMAC_NULL_BUF_WR
	if (request_irq(TOPAZ_IRQ_HBM, &topaz_hbm_handler, 0, "hbm", topaz_emacs)) {
		printk(KERN_ERR "Fail to request IRQ %d\n", TOPAZ_IRQ_HBM);
		return -ENODEV;
	}
	topaz_hbm_emac_rx_pool_intr_init();
	topaz_hbm_emac_rx_pool_uf_intr_en();
#endif
	emac_lib_board_cfg(0, &emac_cfg_p0, &emac_phy);
	emac_lib_board_cfg(1, &emac_cfg_p1, &emac_phy);

	if (_read_hardware_revision() >= HARDWARE_REVISION_TOPAZ_A2) {
		topaz_tqe_emac_reflect_to(TOPAZ_TQE_LHOST_PORT, bonding);
		printk("enable A2 %s\n", bonding ? "(bonded)":"(single)");
	}

	/* We only use rtl switch as PHY, do not do reset which will restore
	 * to switch mode again. Only do so when using rtl ethernet tranceiver.
	 */
	if (!emac_lib_rtl_switch(emac_cfg_p0 | emac_cfg_p1)) {
		/* Reset ext PHY. This is for bug#11906 */
		emac_lib_enable(1);
	}

	topaz_emac_init_tqe();
	topaz_vlan_clear_all_entries();

	for (i = 0; i < ARRAY_SIZE(iflist); i++) {
		topaz_emacs[i] = topaz_emac_init(i);
		if (topaz_emacs[i]) {
			topaz_emac_on[i] = netif_carrier_ok(topaz_emacs[i]);
			found++;
		}
	}

	if (!found) {
#ifdef TOPAZ_EMAC_NULL_BUF_WR
		free_irq(TOPAZ_IRQ_HBM, topaz_emacs);
#endif
		return -ENODEV;
	} else {
		if (found > 1) {
			INIT_DELAYED_WORK(&topaz_emac_dual_emac_work, topaz_emac_work_fn);
			register_netdevice_notifier(&topaz_link_notifier);
		}
		emac_lib_pm_save_add_notifier();
	}

	return 0;
}

static void __exit topaz_emac_module_exit(void)
{
	int i;
	int found = 0;

	for (i = 0; i < ARRAY_SIZE(iflist); i++) {
		if (topaz_emacs[i]) {
			topaz_emac_exit(topaz_emacs[i]);
			++found;
		}
	}

	if (found) {
		emac_lib_pm_save_remove_notifier();
	}

	if (found > 1) {
		unregister_netdevice_notifier(&topaz_link_notifier);
		pm_flush_work(&topaz_emac_dual_emac_work);
	}
#ifdef TOPAZ_EMAC_NULL_BUF_WR
	free_irq(TOPAZ_IRQ_HBM, topaz_emacs);
#endif
}

module_init(topaz_emac_module_init);
module_exit(topaz_emac_module_exit);

MODULE_LICENSE("GPL");


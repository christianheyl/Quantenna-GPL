/*
 *  include/asm-arc/arch-arc/arasan_emac_ahb.h
 *
 *  Copyright (c) Quantenna Communications Incorporated 2007.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * This file only defines those settings that are specific to U-Boot.
 * All the common definitions are stored in ruby_arasan_emac_ahb.
 *
 */

#ifndef __ASM_ARCH_ARASAN_EMAC_AHB_H
#define __ASM_ARCH_ARASAN_EMAC_AHB_H	1

#define NUM_TX_BUFS	1
#define TX_BUF_SIZE	2048
#define NUM_RX_BUFS	2
#define RX_BUF_SIZE	2048

#define TX_TIMEOUT_IN_TICKS (CONFIG_SYS_HZ)

/* All the common defines for the Ethernet controller */
#include <config.h>
#include "../../../../common/ruby_arasan_emac_ahb.h"

#ifndef __ASSEMBLY__

/* Taken from Linux MII driver */
#define LPA_100FULL             0x0100  /* Can do 100mbps full-duplex  */
#define LPA_100BASE4            0x0200  /* Can do 100mbps 4k packets   */
#define LPA_100HALF             0x0080  /* Can do 100mbps half-duplex  */
#define LPA_10FULL              0x0040  /* Can do 10mbps full-duplex   */
#define LPA_10HALF              0x0020  /* Can do 10mbps half-duplex   */
#define LPA_1000FULL            0x0800  /* Link partner 1000BASE-T full duplex */
#define LPA_1000HALF            0x0400  /* Link partner 1000BASE-T half duplex */

#define ADVERTISE_10FULL        0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_100FULL       0x0100  /* Try for 100mbps full-duplex */
#define ADVERTISE_100HALF       0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_1000HALF      0x0100  /* Advertise 1000BASE-T half duplex */
#define ADVERTISE_CSMA          0x0001  /* Only selector supported     */

#define ADVERTISE_FULL          (ADVERTISE_100FULL | ADVERTISE_10FULL | ADVERTISE_CSMA)

#define PHY_MAX_ADDR            32

#define ARASAN_MDIO_BASE        RUBY_ENET0_BASE_ADDR

enum DP83865PhyRegVals {
	/* Basic mode control register */
	PhySoftReset = (1 << 15),
	PhyLoopback = (1 << 14),
	PhySetSpeed1G = ((1 << 6) | (0 << 13)),
	PhySetSpeed100M = ((0 << 6) | (1 << 13)),
	PhySetSpeed10M = ((0 << 6) | (0 << 13)),
	PhyAutoNegEnable = (1 << 12),
	PhyPowerDown = (1 << 11),
	PhyIsolate = (1 << 10),
	PhyRestartAutoNeg = (1 << 9),
	PhyFullDuplex = (1 << 8),
	PhyColTest = (1 << 7),
	/* Basic mode status register */
	PhyAutoNegComplete = (1 << 5),
	/* Link and AutoNegotiation register */
	PhySpeedMask = (3 << 3),
	PhySpeed10M = (0 << 3),
	PhySpeed100M = (1 << 3),
	PhySpeed1G = (2 << 3),
	PhyLinkIsUp = (1 << 2),
	PhyLinkIsFullDuplex = (1 << 1),
	PhyInMasterMode = (1 << 0),
};

enum DP83865RegOffset {
	PhyBMCR = 0,
	PhyBMSR = 1,
	PhyPhysID1 = 2,
	PhyPhysID2 = 3,
	PhyAdvertise = 4,
	PhyLpa = 5,
	PhyStat1000 = 10,
};

struct emac_private;

struct emac_private_mdio {
	unsigned long base;
	uint32_t (*read)(struct emac_private *priv, uint8_t reg);
	int (*write)(struct emac_private *priv, uint8_t reg, uint16_t value);
};


/*
 * Shortened version of the equivalent Linux structure - U-Boot does not
 * need most of the members that Linux does.
 */
struct emac_private {
	int tx_ring_elements; /* = number of buffers = number of descriptors */
	int rx_ring_elements; /* = number of buffers = number of descriptors */

	u32 rx_bufs;
	u32 tx_bufs;
	volatile struct emac_desc *rx_descs;
	volatile struct emac_desc *tx_descs;
	u32 rx_buf_size;
	u32 tx_buf_size;
	int rx_index;
	int tx_index;

	// pointer to switch device
	void *ar8236dev;

	// board specific cfg
	int irq;
	int phy_addr;
	int phy_flags;
	int emac;
	unsigned long io_base;
	struct emac_private_mdio *mdio;
	struct eth_device * parent;
};

struct br_private {
	struct emac_private * emacs[2];
	uint8_t nemacs;
	uint8_t dev_state;
	uint8_t send_mask;
	uint32_t phy_addr_mask;
};

void mdio_postrd_raw(unsigned long base, int phy, int reg);
int mdio_postwr_raw(unsigned long base, int phy, int reg, u16 val);
u32 mdio_rdval_raw(unsigned long base, int wait);

#define MDIO_READ_FAIL		((uint32_t) -1)

static inline int mdio_read_failed(uint32_t val)
{
	return val == MDIO_READ_FAIL;
}

#endif /* __ASSEMBLY__ */

#endif

/*
 * Driver for the Marvell 88E6071 switch
 *
 * Copyright (c) Quantenna Communications, Incorporated 2013
 * All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/phy.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/compiler.h>
#include "mv88e6071.h"
#include <common/ruby_arasan_emac_ahb.h>

#define MV88E6XXX_GLOBAL2_PHYADDR	0x17
#define MV88E6XXX_SMI_PHY_CMD		0x18
#define MV88E6XXX_SMI_PHY_DATA		0x19
#define MV88E6XXX_SMI_PHY_CMD_WRITE	BIT(10)
#define MV88E6XXX_SMI_PHY_CMD_READ	BIT(11)
#define MV88E6XXX_SMI_PHY_CMD_MODE	BIT(12)	/* 0 = 802.3 Clause 45 SMI frame, 1 = 802.3 Clause 22 SMI frame */
#define MV88E6XXX_SMI_PHY_CMD_BUSY	BIT(15)

static uint16_t mv88e6071_smi_phy_cmd(uint8_t dev_addr, uint8_t reg_addr, uint16_t op)
{
	uint16_t val = 0;
	val |= dev_addr << 5;
	val |= reg_addr;
	val |= op;
	val |= MV88E6XXX_SMI_PHY_CMD_MODE;
	val |= MV88E6XXX_SMI_PHY_CMD_BUSY;

	return val;
}

static uint16_t mv88e6071_smi_phy_cmd_rd(uint8_t dev_addr, uint8_t reg_addr)
{
	return mv88e6071_smi_phy_cmd(dev_addr, reg_addr, MV88E6XXX_SMI_PHY_CMD_READ);
}

static uint16_t mv88e6071_smi_phy_cmd_wr(uint8_t dev_addr, uint8_t reg_addr)
{
	return mv88e6071_smi_phy_cmd(dev_addr, reg_addr, MV88E6XXX_SMI_PHY_CMD_WRITE);
}

static int mv88e6071_smi_wait(struct emac_common *privc)
{
	/* wait for arasan mdio op to complete */
	if (!mdio_wait(privc, EMAC_MAC_MDIO_CTRL, MacMdioCtrlStart, 0,
				TIMEOUT_MAC_MDIO_CTRL, __FUNCTION__)) {
		return -1;
	};

	/* also make sure that mv88e6071 SMI PHY unit busy is deasserted */
	if (!mdio_wait(privc, MV88E6XXX_SMI_PHY_CMD, MV88E6XXX_SMI_PHY_CMD_BUSY, 0,
				TIMEOUT_MAC_MDIO_CTRL, __FUNCTION__)) {
		return -1;
	}

	return 0;
}

int mv88e6071_mdio_read(struct mii_bus *bus, int dev_addr, int reg_addr)
{
	struct net_device *dev = bus->priv;
	struct emac_common *privc = netdev_priv(dev);
	int rc;

	if (mv88e6071_smi_wait(privc)) {
		return -1;
	}

	/* write to SMI phy control and SMI phy data registers that a read op is requested */
	rc = emac_lib_mdio_write(bus, MV88E6XXX_GLOBAL2_PHYADDR, MV88E6XXX_SMI_PHY_CMD,
				mv88e6071_smi_phy_cmd_rd(dev_addr, reg_addr));
	if (rc < 0) {
		return -1;
	}

	if (mv88e6071_smi_wait(privc)) {
		return -1;
	}

	/* read desired value from the SMI phy data register */
	return emac_lib_mdio_read(bus, MV88E6XXX_GLOBAL2_PHYADDR, MV88E6XXX_SMI_PHY_DATA);
}

int mv88e6071_mdio_write(struct mii_bus *bus, int dev_addr, int reg_addr, uint16_t value)
{
	struct net_device *dev = bus->priv;
	struct emac_common *privc = netdev_priv(dev);
	int rc;

	if (mv88e6071_smi_wait(privc)) {
		return -1;
	}

	/* write value to the SMI phy data register */
	rc = emac_lib_mdio_write(bus, MV88E6XXX_GLOBAL2_PHYADDR, MV88E6XXX_SMI_PHY_DATA, value);
	if (rc < 0) {
		return -1;
	}

	if (mv88e6071_smi_wait(privc)) {
		return -1;
	}

	/* write SMI phy ctrl reg to push through the new SMI phy data reg value */
	rc = emac_lib_mdio_write(bus, MV88E6XXX_GLOBAL2_PHYADDR, MV88E6XXX_SMI_PHY_CMD,
		mv88e6071_smi_phy_cmd_wr(dev_addr, reg_addr));
	if (rc < 0) {
		return -1;
	}

	if (mv88e6071_smi_wait(privc)) {
		return -1;
	}

	return 0;
}

int mv88e6071_init(struct emac_common *privc)
{
	int phyaddr;

	/* disable flow control for each port */
	for (phyaddr = 0x10; phyaddr <= 0x14; phyaddr++) {
		int val;

		/* flow control change */
		val = mv88e6071_mdio_read(privc->mii_bus, phyaddr, MII_ADVERTISE) &
			~(ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);
		mv88e6071_mdio_write(privc->mii_bus, phyaddr, MII_ADVERTISE, val);

		/* reset & restart auto-negotiation */
		val = mv88e6071_mdio_read(privc->mii_bus, phyaddr, MII_BMCR) |
			BMCR_RESET | BMCR_ANRESTART;
		mv88e6071_mdio_write(privc->mii_bus, phyaddr, MII_BMCR, val);
	}

	return 0;
}


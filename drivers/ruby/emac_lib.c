/**
 * Copyright (c) 2008-2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/sysfs.h>
#include <linux/crc32.h>
#include <linux/pm_qos_params.h>

#include <asm/gpio.h>

#include <asm/board/soc.h>
#include <asm/board/board_config.h>

#include <common/ruby_pm.h>
#include <qtn/emac_debug.h>
#include "ar823x.h"
#include "mv88e6071.h"
#include "emac_lib.h"
#include "rtl8367b/rtl8367b_init.h"
#include <common/topaz_emac.h>
#include <qtn/dmautil.h>
#include <qtn/qtn_debug.h>

/* create build error if arasan structure is re-introduced */
struct emac_lib_private {
};

#define MDIO_REGISTERS		32

#define DRV_NAME		"emac_lib"
#define DRV_VERSION		"1.0"
#define DRV_AUTHOR		"Quantenna Communications Inc."
#define DRV_DESC		"Arasan AHB-EMAC on-chip Ethernet driver"

#define PHY_REG_PROC_FILE_NAME	"phy_reg"
#define PHY_PW_PROC_FILE_NAME	"phy_pw"
#define PHY_PW_CMD_MAX_LEN	20

static int mdio_use_noops = 0;
module_param(mdio_use_noops, int, 0);
int mdc_clk_divisor = 1;
module_param(mdc_clk_divisor, int, 0644);

static uint32_t emac_lib_dual_emac;

static ssize_t show_mdio_use_noops(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", mdio_use_noops);
}

static ssize_t store_mdio_use_noops(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (count >= 1)
		mdio_use_noops = (buf[0] == '1');
	if (mdio_use_noops)
		printk(KERN_INFO "Disabling MDIO read/write for %s\n", dev_name(dev));

#if EMAC_REG_DEBUG
	if (buf[0] == 'd') {
		unsigned long base = RUBY_ENET0_BASE_ADDR;
		if (buf[1] == '1')
			base = RUBY_ENET1_BASE_ADDR;
		emac_lib_reg_debug(base);
	}
#endif

#ifdef RTL_SWITCH
	if (buf[0] == 'a') {
		rtl8367b_dump_status();
	}
	if (buf[0] == 's') {
		rtl8367b_dump_stats();
	}
#endif

	return count;
}

static DEVICE_ATTR(mdio_use_noops, S_IRUGO | S_IWUGO, show_mdio_use_noops, store_mdio_use_noops);

int emac_lib_mdio_sysfs_create(struct net_device *net_dev)
{
	return sysfs_create_file(&net_dev->dev.kobj, &dev_attr_mdio_use_noops.attr);
}
EXPORT_SYMBOL(emac_lib_mdio_sysfs_create);

void emac_lib_mdio_sysfs_remove(struct net_device *net_dev)
{
	sysfs_remove_file(&net_dev->dev.kobj, &dev_attr_mdio_use_noops.attr);
}
EXPORT_SYMBOL(emac_lib_mdio_sysfs_remove);

/* noop_values are shared between all devices, could get interesting if there are more than one */
static u32 mdio_noop_values[MDIO_REGISTERS] = {
			    0xFFFF,
			    0x796D,
			    0x243,
			    0xD91,
			    0xDE1,
			    0xCDE1,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0x700,
			    0x7C00,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF,
			    0xFFFF
};


/*
 * This functions polls until register value is changed (apply mask and compare with val).
 * Function has timeout, so polling is not indefinite.
 * Also function try to be clever and work safely in heavy loaded system.
 * It also try to reduce CPU load using sleep or context switch.
 */
int emac_lib_poll_wait(struct emac_common *privc, u32(*read_func)(struct emac_common*, int),
		int reg, u32 mask, u32 val, unsigned long ms, const char *func)
{
	int ret = 1;
	unsigned long ms_warn = ms / 2;

	int first_run = 1;
	unsigned long deadline = jiffies + max(msecs_to_jiffies(ms), 1UL);

	while (((*read_func)(privc, reg) & mask) != val) {
		if (first_run) {
			deadline = jiffies + max(msecs_to_jiffies(ms), 1UL);
			first_run = 0;
		} else if (time_after_eq(jiffies, deadline)) {
			break;
		} else if (irqs_disabled() || in_atomic()) {
			udelay(100);
		} else if (time_before(jiffies + 5, deadline)) {
			msleep(1000 / HZ);
		} else {
			cond_resched();
		}
	}
	if (((*read_func)(privc, reg) & mask) != val) {
		if (func) {
			printk(KERN_ERR "%s %s: err: timeout %lums\n",
					privc->dev->name, func, ms);
		}
		ret = 0;
	} else if(time_after_eq(jiffies + msecs_to_jiffies(ms_warn), deadline)) {
		if (func) {
			if (printk_ratelimit())
				printk(KERN_WARNING "%s %s: warn: system is overloaded : spend more ~%lums!\n",
					privc->dev->name, func, ms_warn);
		}
	}

	return ret;
}
EXPORT_SYMBOL(emac_lib_poll_wait);

int emac_lib_board_cfg(int port, int *cfg, int *phy)
{
	int cfg_param;
	int phy_param;
	int rc;

	if (port == 0) {
		cfg_param = BOARD_CFG_EMAC0;
		phy_param = BOARD_CFG_PHY0_ADDR;
	} else if (port == 1) {
		cfg_param = BOARD_CFG_EMAC1;
		phy_param = BOARD_CFG_PHY1_ADDR;
	} else {
		printk(KERN_ERR "%s invalid port number %d\n", __FUNCTION__, port);
		return -EINVAL;
	}

	rc = get_board_config(cfg_param, cfg);
	if (rc) {
		printk(KERN_ERR "%s get_board_config returns %d\n", __FUNCTION__, rc);
		return rc;
	}

	rc = get_board_config(phy_param, phy);
	if (rc) {
		printk(KERN_ERR "%s get_board_config returns %d\n", __FUNCTION__, rc);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(emac_lib_board_cfg);

static void emac_lib_adjust_speed(struct net_device *dev, int speed, int duplex)
{
	u32 val;
	u32 speed_shift = 0;
	struct emac_common *privc = netdev_priv(dev);

	switch (privc->mac_id) {
		case 0:
			speed_shift = RUBY_SYS_CTL_MASK_GMII0_SHIFT;
			break;
		case 1:
			speed_shift = RUBY_SYS_CTL_MASK_GMII1_SHIFT;
			break;
		default:
			panic("No speed_shift defined for %d\n", (int)privc->mac_id);
			break;
	}

	val = emac_rd(privc, EMAC_MAC_GLOBAL_CTRL) & ~(MacSpeedMask | MacFullDuplex);
	if (duplex == DUPLEX_FULL) {
		val |= MacFullDuplex;
	}
	/* note: this covers emac0 - need to extend for emac1 when we add support */
	writel(RUBY_SYS_CTL_MASK_GMII_TXCLK << speed_shift, IO_ADDRESS(RUBY_SYS_CTL_MASK));
	switch (speed) {
		case SPEED_10:
			emac_wr(privc, EMAC_MAC_GLOBAL_CTRL, val | MacSpeed10M);
			writel(RUBY_SYS_CTL_MASK_GMII_10M << speed_shift, IO_ADDRESS(RUBY_SYS_CTL_CTRL));
			break;
		case SPEED_100:
			emac_wr(privc, EMAC_MAC_GLOBAL_CTRL, val | MacSpeed100M);
			writel(RUBY_SYS_CTL_MASK_GMII_100M << speed_shift, IO_ADDRESS(RUBY_SYS_CTL_CTRL));
			break;
		case SPEED_1000:
			emac_wr(privc, EMAC_MAC_GLOBAL_CTRL, val | MacSpeed1G);
			writel(RUBY_SYS_CTL_MASK_GMII_1000M << speed_shift, IO_ADDRESS(RUBY_SYS_CTL_CTRL));
			break;
		default:
			printk(KERN_WARNING "%s: Speed (%d) is not supported\n", dev->name, speed);
			break;
	}
}

static void emac_lib_adjust_link(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct phy_device *phydev = privc->phy_dev;

	BUG_ON(!privc->phy_dev);
	if (phydev->link != privc->old_link) {
		if (phydev->link) {
			emac_lib_adjust_speed(dev, phydev->speed, phydev->duplex);
			netif_tx_schedule_all(dev);
			printk(KERN_INFO "%s: link up (%d/%s)\n",
					dev->name, phydev->speed,
					phydev->duplex == DUPLEX_FULL ? "Full" : "Half");
		} else {
			printk(KERN_INFO "%s: link down\n", dev->name);
		}
		privc->old_link = phydev->link;
	}
}

/*
 * MII operations
 */
int emac_lib_mdio_read(struct mii_bus *bus, int phy_addr, int reg)
{
	struct net_device *dev;
	struct emac_common *privc;
	u32 mii_control, read_val;

	if (mdio_use_noops) {
		return mdio_noop_values[reg % MDIO_REGISTERS];
	}

	dev = bus->priv;
	privc = netdev_priv(dev);

	if (!mdio_wait(privc, EMAC_MAC_MDIO_CTRL, MacMdioCtrlStart, 0, TIMEOUT_MAC_MDIO_CTRL, __FUNCTION__)) {
		return -1;
	}

	mii_control =
		((reg & MacMdioCtrlRegMask) << MacMdioCtrlRegShift) |
		((phy_addr & MacMdioCtrlPhyMask) << MacMdioCtrlPhyShift) |
		((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
		MacMdioCtrlRead | MacMdioCtrlStart;
	mdio_wr(privc, EMAC_MAC_MDIO_CTRL, mii_control);
	if (!mdio_wait(privc, EMAC_MAC_MDIO_CTRL, MacMdioCtrlStart, 0, TIMEOUT_MAC_MDIO_CTRL, __FUNCTION__)) {
		return -1;
	}

	read_val = mdio_rd(privc, EMAC_MAC_MDIO_DATA) & MacMdioDataMask;
	//printk(KERN_INFO "%s: PHY: %d Reg %d Value 0x%08x\n", __FUNCTION__, phy_addr, reg, read_val);
	mdio_noop_values[reg % MDIO_REGISTERS] = read_val;
	return (int)(read_val);
}

int emac_lib_mdio_write(struct mii_bus *bus, int phy_addr, int reg, uint16_t value)
{
	struct net_device *dev;
	struct emac_common *privc;
	u32 mii_control;

	if (mdio_use_noops) {
		//printk(KERN_WARNING "MII Write is a noop: MII WR: Reg %d Value %08X\n",reg,(unsigned)value);
		return 0;
	}

	dev = bus->priv;
	privc = netdev_priv(dev);

	if (!mdio_wait(privc, EMAC_MAC_MDIO_CTRL, MacMdioCtrlStart, 0, TIMEOUT_MAC_MDIO_CTRL, __FUNCTION__)) {
		return -1;
	}

	mii_control =
		((reg & MacMdioCtrlRegMask) << MacMdioCtrlRegShift) |
		((phy_addr & MacMdioCtrlPhyMask) << MacMdioCtrlPhyShift) |
		((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
		MacMdioCtrlWrite | MacMdioCtrlStart;

	//printk(KERN_INFO "%s: PHY: %d Reg %d Value 0x%08x\n", __FUNCTION__, phy_addr, reg, value);
	mdio_wr(privc, EMAC_MAC_MDIO_DATA, value);
	mdio_wr(privc, EMAC_MAC_MDIO_CTRL, mii_control);
	return 0;
}

static u32 phydev_addr_inuse[2] = { -1, -1 };

static int mii_probe(struct net_device *dev)
{
	struct emac_common * const privc = netdev_priv(dev);
	struct phy_device *phydev = NULL;
	int phy_index;
	int phy_found = 0;
	int port_num = privc->mac_id;
	unsigned long phy_supported = 0;

	privc->phy_dev = NULL;

	if (privc->emac_cfg & (EMAC_PHY_AR8236 | EMAC_PHY_AR8327)) {
		// handle ar823x switch first
		return ar823x_init(privc->phy_addr);
	} else if (privc->emac_cfg & EMAC_PHY_MV88E6071) {
		return mv88e6071_init(privc);
	}

	if (privc->emac_cfg & EMAC_PHY_NOT_IN_USE) {
		// no PHY - just return OK
		return 0;
	}

	/*
	 * Find a matching phy address on the current bus, or the first unused
	 * by index if scanning for phys
	 */
	for (phy_index = 0; phy_index < PHY_MAX_ADDR; phy_index++) {
		struct phy_device *pdev = privc->mii_bus->phy_map[phy_index];
		int in_use = 0;
		int i;

		if (!pdev) {
			continue;
		}

		printk(KERN_INFO DRV_NAME " %s: index %d id 0x%x addr %d\n",
				dev->name, phy_index, pdev->phy_id, pdev->addr);

		if (phy_found) {
			continue;
		}

		/* check that this phy isn't currently in use */
		for (i = 0; i < ARRAY_SIZE(phydev_addr_inuse); i++) {
			if (port_num != i && phydev_addr_inuse[i] == pdev->addr) {
				in_use = 1;
			}
		}

		if (!in_use && (privc->phy_addr == pdev->addr ||
					privc->phy_addr == EMAC_PHY_ADDR_SCAN)) {
			phydev = pdev;
			phydev_addr_inuse[port_num] = pdev->addr;
			phy_found = 1;
		}
	}

	if (!phydev) {
		printk(KERN_ERR DRV_NAME " %s: no PHY found\n", dev->name);
		return -1;
	}
	printk(KERN_INFO DRV_NAME " %s: phy_id 0x%x addr %d\n",
			dev->name, phydev->phy_id, phydev->addr);

	/* now we are supposed to have a proper phydev, to attach to... */
	BUG_ON(phydev->attached_dev);

	/* XXX: Check if this should be done here. Forcing advert of Symm Pause */
	phy_write(phydev, MII_ADVERTISE,
			phy_read(phydev, MII_ADVERTISE) | ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM );

	phydev = phy_connect(dev, dev_name(&phydev->dev), &emac_lib_adjust_link, 0,
			PHY_INTERFACE_MODE_MII);
	if (IS_ERR(phydev)) {
		printk(KERN_ERR DRV_NAME " %s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	/* mask with MAC supported features */
	phy_supported =	SUPPORTED_Autoneg        |
		SUPPORTED_Pause          |
		SUPPORTED_Asym_Pause     |
		SUPPORTED_MII            |
		SUPPORTED_TP;

	if (privc->emac_cfg & EMAC_PHY_FORCE_10MB) {
		phy_supported |= SUPPORTED_10baseT_Half   |
			SUPPORTED_10baseT_Full;
	} else if (privc->emac_cfg & EMAC_PHY_FORCE_100MB) {
		phy_supported |= SUPPORTED_100baseT_Half   |
			SUPPORTED_100baseT_Full;
	} else if (privc->emac_cfg & EMAC_PHY_FORCE_1000MB) {
		phy_supported |= SUPPORTED_1000baseT_Half   |
			SUPPORTED_1000baseT_Full;
	} else {
		phy_supported |= SUPPORTED_10baseT_Half   |
			SUPPORTED_10baseT_Full   |
			SUPPORTED_100baseT_Half  |
			SUPPORTED_100baseT_Full  |
			SUPPORTED_1000baseT_Half |
			SUPPORTED_1000baseT_Full;
	}

	phydev->supported &= phy_supported;
	phydev->advertising = phydev->supported;
	privc->old_link = 0;
	privc->phy_dev = phydev;

	printk(KERN_INFO DRV_NAME " %s: attached PHY driver [%s] "
			"(mii_bus:phy_addr=%s, irq=%d)\n",
			dev->name, phydev->drv->name, phydev->dev.bus->name, phydev->irq);

	return 0;
}

void emac_lib_phy_start(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);

	if ((privc->emac_cfg & EMAC_PHY_NOT_IN_USE) == 0){
		/* cause the PHY state machine to schedule a link state check */
		privc->old_link = 0;
		phy_stop(privc->phy_dev);
		phy_start(privc->phy_dev);
	} else {
		// This is the case of no phy - need to force link speed
		int speed = 0;
		int duplex = DUPLEX_FULL;

		if (privc->emac_cfg & EMAC_PHY_FORCE_10MB)  {
			speed = SPEED_10;
		} else if (privc->emac_cfg & EMAC_PHY_FORCE_100MB)  {
			speed = SPEED_100;
		} else if (privc->emac_cfg & EMAC_PHY_FORCE_1000MB)  {
			speed = SPEED_1000;
		}
		if (privc->emac_cfg & EMAC_PHY_FORCE_HDX) {
			duplex = 0;
		}
		emac_lib_adjust_speed(dev, speed, duplex);
		printk(KERN_INFO DRV_NAME " %s: force link (%d/%s)\n",
				dev->name, speed,
				duplex == DUPLEX_FULL ? "Full" : "Half");
	}

#ifdef RTL_SWITCH
	if (emac_lib_rtl_switch(privc->emac_cfg)) {
		rtl8367b_ext_port_enable(dev->if_port);
	}
#endif
}
EXPORT_SYMBOL(emac_lib_phy_start);

void emac_lib_phy_stop(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);

#ifdef RTL_SWITCH
	if (emac_lib_rtl_switch(privc->emac_cfg)) {
		rtl8367b_ext_port_disable(dev->if_port);
	}
#endif

	if (privc->phy_dev) {
		phy_stop(privc->phy_dev);
	}
}
EXPORT_SYMBOL(emac_lib_phy_stop);

static void emac_lib_enable_gpio_reset_pin(int pin)
{
	printk(KERN_INFO "%s GPIO pin %d reset sequence\n", DRV_NAME, pin);
	if (gpio_request(pin, DRV_NAME) < 0)
		printk(KERN_ERR "%s: Failed to request GPIO%d for GPIO reset\n",
				DRV_NAME, pin);

	gpio_direction_output(pin, 1);
	udelay(100);
	gpio_set_value(pin, 0);
	mdelay(100);
	gpio_set_value(pin, 1);
	gpio_free(pin);
}

static void emac_lib_enable_gpio_reset(uint32_t cfg)
{
	if (cfg & EMAC_PHY_GPIO1_RESET) {
		emac_lib_enable_gpio_reset_pin(RUBY_GPIO_PIN1);
	}

	if (cfg & EMAC_PHY_GPIO13_RESET) {
		emac_lib_enable_gpio_reset_pin(RUBY_GPIO_PIN13);
	}
}

void emac_lib_enable(uint32_t ext_reset)
{
	uint32_t emac0_cfg = EMAC_NOT_IN_USE;
	uint32_t emac1_cfg = EMAC_NOT_IN_USE;
	uint32_t emac_cfg;
	uint32_t rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT;

	get_board_config(BOARD_CFG_RGMII_TIMING, (int *) &rgmii_timing);

	if (get_board_config(BOARD_CFG_EMAC0, (int *) &emac0_cfg) != 0) {
		printk(KERN_ERR "%s: get_board_config returned error status for EMAC0\n", DRV_NAME);
	}

	if (get_board_config(BOARD_CFG_EMAC1, (int *) &emac1_cfg) != 0) {
		printk(KERN_ERR "%s: get_board_config returned error status for EMAC1\n", DRV_NAME);
	}
	emac_cfg = emac0_cfg | emac1_cfg;

	/* Use GPIO to reset ODM PHY */
	emac_lib_enable_gpio_reset(emac_cfg);

	arasan_initialize_release_reset(emac0_cfg, emac1_cfg, rgmii_timing, ext_reset);
}
EXPORT_SYMBOL(emac_lib_enable);

static struct mii_bus* emac_lib_alloc_mii(struct net_device *dev)
{
	int i;
	struct emac_common *privc = netdev_priv(dev);
	struct mii_bus *mii = NULL;

	/* Alloc bus structure */
	mii = mdiobus_alloc();
	if (!mii) {
		goto mii_alloc_err_out;
	}

	/* Initialize mii structure fields */
	mii->priv = dev;

	/* if we are using ar8236 switch, the mdio ops are special */
	if (privc->emac_cfg & (EMAC_PHY_AR8236 | EMAC_PHY_AR8327)) {
		mii->read = ar823x_mdio_read;
		mii->write = ar823x_mdio_write;
	} else if (privc->emac_cfg & EMAC_PHY_MV88E6071) {
		mii->read = mv88e6071_mdio_read;
		mii->write = mv88e6071_mdio_write;
	} else if (emac_lib_rtl_switch(privc->emac_cfg)) {
#ifdef RTL_SWITCH
		if (rtl8367b_init(mii, &emac_lib_mdio_read,
					&emac_lib_mdio_write,
					privc->emac_cfg, privc->mac_id)) {
			goto board_init_err_out;
		}
#else
		printk(KERN_ERR "rtl switch module not available\n");
		goto board_init_err_out;
#endif
	} else {
		mii->read = emac_lib_mdio_read;
		mii->write = emac_lib_mdio_write;
	}
	mii->name = "emac_eth_mii";
	snprintf(mii->id, MII_BUS_ID_SIZE, "%x", privc->mac_id);

	/* Initialize irq field */
	mii->irq = kmalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (!mii->irq) {
		goto irq_alloc_err_out;
	}
	for(i = 0; i < PHY_MAX_ADDR; ++i) {
		mii->irq[i] = PHY_POLL;
	}

	/* Register bus if we are using PHY */
	if ((privc->emac_cfg & EMAC_PHY_NOT_IN_USE) == 0) {
		if (mdiobus_register(mii)) {
			goto mii_register_err_out;
		}
	}
	return mii;

mii_register_err_out:
	kfree(mii->irq);

irq_alloc_err_out:
board_init_err_out:
	mdiobus_free(mii);

mii_alloc_err_out:
	return NULL;
}

void emac_lib_mii_exit(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct mii_bus *mii = privc->mii_bus;

	if (mii) {
#ifdef RTL_SWITCH
		if (emac_lib_rtl_switch(privc->emac_cfg)) {
			rtl8367b_exit();
		}
#endif
		if (mii->irq) {
			kfree(mii->irq);
		}
		mdiobus_unregister(mii);
		mdiobus_free(mii);
	}

	phydev_addr_inuse[privc->mac_id] = -1;
}
EXPORT_SYMBOL(emac_lib_mii_exit);

int emac_lib_mii_init(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);

	privc->mii_bus = emac_lib_alloc_mii(dev);
	if (!privc->mii_bus) {
		goto err_out;
	}

	if (mii_probe(dev)) {
		goto err_out;
	}

	return 0;

err_out:
	if (privc->mii_bus) {
		emac_lib_mii_exit(dev);
		privc->mii_bus = NULL;
	}
	return -ENODEV;
}
EXPORT_SYMBOL(emac_lib_mii_init);

static int emac_lib_ethtool_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct emac_common *privc = netdev_priv(dev);

	if (privc->phy_dev) {
		return phy_ethtool_gset(privc->phy_dev, cmd);
	}

	if (privc->emac_cfg & EMAC_PHY_NOT_IN_USE) {
		uint32_t supported = 0;
		uint16_t speed = 0;
		uint8_t duplex;

		memset(cmd, 0, sizeof(*cmd));

		/*
		 * Return forced settings; used by bonding driver etc
		 */
		if (privc->emac_cfg & EMAC_PHY_FORCE_10MB) {
			supported = SUPPORTED_10baseT_Half |
				SUPPORTED_10baseT_Full;
			speed = SPEED_10;
		} else if (privc->emac_cfg & EMAC_PHY_FORCE_100MB) {
			supported |= SUPPORTED_100baseT_Half |
				SUPPORTED_100baseT_Full;
			speed = SPEED_100;
		} else if (privc->emac_cfg & EMAC_PHY_FORCE_1000MB) {
			supported |= SUPPORTED_1000baseT_Half |
				SUPPORTED_1000baseT_Full;
			speed = SPEED_1000;
		}

		if (privc->emac_cfg & EMAC_PHY_FORCE_HDX) {
			duplex = DUPLEX_HALF;
		} else {
			duplex = DUPLEX_FULL;
		}

		cmd->supported = supported;
		cmd->advertising = supported;
		cmd->speed = speed;
		cmd->duplex = duplex;
		cmd->port = PORT_MII;
		cmd->transceiver = XCVR_EXTERNAL;

		return 0;
	}

	return -EINVAL;
}

static int emac_lib_ethtool_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct emac_common *privc = netdev_priv(dev);

	if (!capable(CAP_NET_ADMIN)) {
		return -EPERM;
	}

	if (privc->phy_dev) {
		return phy_ethtool_sset(privc->phy_dev, cmd);
	}

	return -EINVAL;
}

static void emac_lib_ethtool_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct emac_common *privc = netdev_priv(dev);

	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	info->fw_version[0] = '\0';
	sprintf(info->bus_info, "%s %d", DRV_NAME, privc->mac_id);
	info->regdump_len = 0;
}

const struct ethtool_ops emac_lib_ethtool_ops = {
	.get_settings = emac_lib_ethtool_get_settings,
	.set_settings = emac_lib_ethtool_set_settings,
	.get_drvinfo = emac_lib_ethtool_get_drvinfo,
	.get_link = ethtool_op_get_link,
};
EXPORT_SYMBOL(emac_lib_ethtool_ops);


void emac_lib_descs_free(struct net_device *dev)
{
	/*
	 * All Ethernet activity should have ceased before calling
	 * this function
	 */
	struct emac_common *priv = netdev_priv(dev);

	ALIGNED_DMA_DESC_FREE(&priv->rx);
	ALIGNED_DMA_DESC_FREE(&priv->tx);
}
EXPORT_SYMBOL(emac_lib_descs_free);

int emac_lib_descs_alloc(struct net_device *dev,
		u32 rxdescs, bool rxdescs_sram,
		u32 txdescs, bool txdescs_sram)
{
	struct emac_common *priv = netdev_priv(dev);
	const unsigned long desc_align = 8; /* Descriptors must be aligned on 64-bit for 64-bit EMAC mode (our case) */

	if (ALIGNED_DMA_DESC_ALLOC(&priv->rx, rxdescs, desc_align, rxdescs_sram)) {
		goto bad;
	}

	if (ALIGNED_DMA_DESC_ALLOC(&priv->tx, txdescs, desc_align, txdescs_sram)) {
		goto bad;
	}

	return 0;
bad:
	emac_lib_descs_free(dev);

	return -ENOMEM;
}
EXPORT_SYMBOL(emac_lib_descs_alloc);

void emac_lib_send_pause(struct net_device *dev, int pause_time)
{
	struct emac_common *privc = netdev_priv(dev);
	uint32_t control;

	emac_wr(privc, EMAC_MAC_FLOW_PAUSE_TIMEVAL, pause_time);
	emac_wr(privc, EMAC_MAC_FLOW_PAUSE_GENERATE, 0);
	control  = emac_rd(privc, EMAC_MAC_FLOW_PAUSE_GENERATE);

	emac_wr(privc, EMAC_MAC_FLOW_PAUSE_GENERATE, 1);
	while (control & 0x1) {
		control  = emac_rd(privc, EMAC_MAC_FLOW_PAUSE_GENERATE);
	}
}
EXPORT_SYMBOL(emac_lib_send_pause);

void emac_lib_init_mac(struct net_device *dev)
{
	/* This routine has the side-effect of stopping MAC RX and TX */
	struct emac_common *privc = netdev_priv(dev);

	/* EMAC_MAC_GLOBAL_CTRL set in response to link negotiation */
	emac_wr(privc, EMAC_MAC_TX_CTRL, MacTxAutoRetry);
	emac_wr(privc, EMAC_MAC_RX_CTRL, MacRxEnable | MacRxStripFCS |
			MacRxStoreAndForward | MacAccountVLANs);
	/* FIXME : These values should change based on required MTU size */
	emac_wr(privc, EMAC_MAC_MAX_FRAME_SIZE, 0xC80);
	emac_wr(privc, EMAC_MAC_TX_JABBER_SIZE, 0xCA0);
	emac_wr(privc, EMAC_MAC_RX_JABBER_SIZE, 0xCA0);
	emac_wr(privc, EMAC_MAC_ADDR1_HIGH, *(u16 *)&dev->dev_addr[0]);
	emac_wr(privc, EMAC_MAC_ADDR1_MED, *(u16 *)&dev->dev_addr[2]);
	emac_wr(privc, EMAC_MAC_ADDR1_LOW, *(u16 *)&dev->dev_addr[4]);
	emac_wr(privc, EMAC_MAC_ADDR_CTRL, MacAddr1Enable);

	emac_wr(privc, EMAC_MAC_TABLE1, 0);
	emac_wr(privc, EMAC_MAC_TABLE2, 0);
	emac_wr(privc, EMAC_MAC_TABLE3, 0);
	emac_wr(privc, EMAC_MAC_TABLE4, 0);
	emac_wr(privc, EMAC_MAC_FLOW_CTRL, MacFlowDecodeEnable |
			MacFlowGenerationEnable | MacAutoFlowGenerationEnable |
			MacFlowMulticastMode | MacBlockPauseFrames);
	emac_wr(privc, EMAC_MAC_FLOW_SA_HIGH, *(u16 *)&dev->dev_addr[0]);
	emac_wr(privc, EMAC_MAC_FLOW_SA_MED, *(u16 *)&dev->dev_addr[2]);
	emac_wr(privc, EMAC_MAC_FLOW_SA_LOW, *(u16 *)&dev->dev_addr[4]);

	/* !!! FIXME - whether or not we need this depends on whether
	 * the auto-pause generation uses it.  The auto function may just
	 * use 0xffff val to stop sending & then 0 to restart it.
	 */
	emac_wr(privc, EMAC_MAC_FLOW_PAUSE_TIMEVAL, 100);

	emac_wr(privc, EMAC_MAC_TX_ALMOST_FULL, 0x1f8);
	emac_wr(privc, EMAC_MAC_TX_START_THRESHOLD, 1518);
	/* EMAC_MAC_RX_START_THRESHOLD ignored in store & forward mode */
	emac_wr(privc, EMAC_MAC_INT, MacUnderrun | MacJabber); /* clear ints */
}
EXPORT_SYMBOL(emac_lib_init_mac);

void emac_lib_init_dma(struct emac_common *privc)
{
	emac_wr(privc, EMAC_DMA_CONFIG, DmaRoundRobin | Dma16WordBurst | Dma64BitMode);
	emac_wr(privc, EMAC_DMA_CTRL, 0);
	emac_wr(privc, EMAC_DMA_STATUS_IRQ, (u32)-1);
	emac_wr(privc, EMAC_DMA_INT_ENABLE, 0);
	emac_wr(privc, EMAC_DMA_TX_AUTO_POLL, 0);
	emac_wr(privc, EMAC_DMA_TX_BASE_ADDR, privc->tx.descs_dma_addr);
	emac_wr(privc, EMAC_DMA_RX_BASE_ADDR, privc->rx.descs_dma_addr);
}
EXPORT_SYMBOL(emac_lib_init_dma);

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,30)
static inline int netdev_mc_count(const struct net_device *dev)
{
	return dev->mc_count;
}
#endif

static void set_rx_mode_mcfilter(struct net_device *dev, u32 *mc_filter)
{
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,30)
	int i;
	struct dev_mc_list *mclist;

	for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			i++, mclist = mclist->next) {
		set_bit(ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26, mc_filter);
	}
#else
	struct netdev_hw_addr *ha;

	netdev_for_each_mc_addr(ha, dev) {
		set_bit(ether_crc(ETH_ALEN, ha->addr) >> 26,
				(unsigned long *)mc_filter);
	}
#endif
}

void emac_lib_set_rx_mode(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);

	if (dev->flags & IFF_PROMISC) {
		emac_setbits(arapc, EMAC_MAC_ADDR_CTRL, MacPromiscuous);
	} else if ((dev->flags & IFF_ALLMULTI)  ||
			netdev_mc_count(dev) > MULTICAST_FILTER_LIMIT) {
		emac_wr(arapc, EMAC_MAC_TABLE1, 0xffff);
		emac_wr(arapc, EMAC_MAC_TABLE2, 0xffff);
		emac_wr(arapc, EMAC_MAC_TABLE3, 0xffff);
		emac_wr(arapc, EMAC_MAC_TABLE4, 0xffff);
		emac_clrbits(arapc, EMAC_MAC_ADDR_CTRL, MacPromiscuous);
		printk(KERN_INFO "%s: Pass all multicast\n", dev->name);
	} else {
		u32 mc_filter[2];	/* Multicast hash filter */
		mc_filter[1] = mc_filter[0] = 0;

		set_rx_mode_mcfilter(dev, mc_filter);

		emac_wr(arapc, EMAC_MAC_TABLE1, mc_filter[0] & 0xffff);
		emac_wr(arapc, EMAC_MAC_TABLE2, mc_filter[0] >> 16);
		emac_wr(arapc, EMAC_MAC_TABLE3, mc_filter[1] & 0xffff);
		emac_wr(arapc, EMAC_MAC_TABLE4, mc_filter[1] >> 16);
		emac_clrbits(arapc, EMAC_MAC_ADDR_CTRL, MacPromiscuous);
	}
}
EXPORT_SYMBOL(emac_lib_set_rx_mode);

static void emac_pm_enter_to_halt(struct phy_device *phy_dev)
{
	if (phy_dev && phy_dev->state != PHY_HALTED) {
		phy_stop(phy_dev);
		genphy_suspend(phy_dev);
		printk(KERN_INFO"emac enter halted state\n");
	}
}

static void emac_pm_return_from_halt(struct phy_device *phy_dev)
{
	if (phy_dev->state == PHY_HALTED) {
		genphy_resume(phy_dev);
		phy_start(phy_dev);
		printk(KERN_INFO "%s: emac resumed from halt\n", DRV_NAME);
	}
	/* Delay of about 50ms between PHY is resume and start auto negotiation */
	mdelay(50);
}

static unsigned long emac_pm_power_save_level = PM_QOS_DEFAULT_VALUE;
static int emac_pm_adjust_level(const int pm_emac_level, struct phy_device *phy_dev)
{
	if (pm_emac_level != BOARD_PM_LEVEL_NO &&
			(phy_dev->state == PHY_HALTED || emac_lib_dual_emac)) {
		return pm_emac_level;
	}

	return emac_pm_power_save_level;
}

static void emac_pm_level(struct emac_common *arapc, int level)
{
	struct phy_device *phy_dev = arapc->phy_dev;

	if (arapc->emac_cfg & (EMAC_PHY_NOT_IN_USE |
				EMAC_PHY_NO_COC |
				EMAC_PHY_AUTO_MASK)) {
		return;
	}

	level = emac_pm_adjust_level(level, phy_dev);

	if (level >= BOARD_PM_LEVEL_SUSPEND) {
		if (phy_dev->state != PHY_HALTED) {
			phy_stop(phy_dev);
			genphy_suspend(phy_dev);
			printk(KERN_INFO "%s: emac halted\n", DRV_NAME);
		}
	} else if (level >= BOARD_PM_LEVEL_IDLE) {
		emac_pm_return_from_halt(phy_dev);

		if (!arapc->pm_adv_mode) {
			const uint32_t adv_10M_H = SUPPORTED_10baseT_Half;
			const uint32_t adv_10M_F = SUPPORTED_10baseT_Full;
			const uint32_t adv_100M_F = SUPPORTED_100baseT_Full;
			const uint32_t adv_100M_H = SUPPORTED_100baseT_Half;
			const uint32_t adv_1G_F = SUPPORTED_1000baseT_Full;
			const uint32_t adv_1G_H = SUPPORTED_1000baseT_Half;
			uint32_t clear_adv = 0;

			if (phy_dev->advertising & (adv_10M_F | adv_10M_H)) {
				clear_adv = adv_10M_H | adv_100M_F | adv_100M_H | adv_1G_F | adv_1G_H;
			} else if (phy_dev->advertising & (adv_100M_F | adv_100M_H)) {
				clear_adv = (adv_1G_H | adv_1G_F);
			}

			if (clear_adv) {
				arapc->pm_adv_mode = 1;
				mutex_lock(&phy_dev->lock);
				phy_dev->advertising &= ~clear_adv;
				phy_dev->state = PHY_QTNPM;
				phy_dev->link = 0;
				mutex_unlock(&phy_dev->lock);
			}

			printk(KERN_INFO "%s: emac slowed down\n", DRV_NAME);
		}
	} else {
		emac_pm_return_from_halt(phy_dev);

		if (arapc->pm_adv_mode) {
			arapc->pm_adv_mode = 0;
			mutex_lock(&phy_dev->lock);
			phy_dev->advertising = SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full | \
				SUPPORTED_100baseT_Half | SUPPORTED_10baseT_Full | \
				SUPPORTED_1000baseT_Half | SUPPORTED_1000baseT_Full;
			phy_dev->state = PHY_QTNPM;
			phy_dev->link = 0;
			mutex_unlock(&phy_dev->lock);

			printk(KERN_INFO "%s: emac resumed from slow down\n", DRV_NAME);
		}
	}
}

static int emac_lib_pm_emac_notify(struct notifier_block *b, unsigned long level, void *v)
{
	struct emac_common *arapc = container_of(b, struct emac_common, pm_notifier);

	emac_pm_level(arapc, level);

	return NOTIFY_OK;
}

void emac_lib_pm_emac_add_notifier(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);

	arapc->pm_notifier.notifier_call = emac_lib_pm_emac_notify;
	pm_qos_add_notifier(PM_QOS_POWER_EMAC, &arapc->pm_notifier);
}
EXPORT_SYMBOL(emac_lib_pm_emac_add_notifier);

void emac_lib_pm_emac_remove_notifier(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);

	pm_qos_remove_notifier(PM_QOS_POWER_EMAC, &arapc->pm_notifier);
}
EXPORT_SYMBOL(emac_lib_pm_emac_remove_notifier);

static int emac_pm_save_notify(struct notifier_block *b, unsigned long level, void *v)
{
	emac_pm_power_save_level = level;
	pm_qos_refresh_notifiers(PM_QOS_POWER_EMAC);

	return NOTIFY_OK;
}

static struct notifier_block pm_save_notifier = {
	.notifier_call = emac_pm_save_notify,
};

void emac_lib_pm_save_add_notifier(void)
{
	pm_qos_add_notifier(PM_QOS_POWER_SAVE, &pm_save_notifier);
}
EXPORT_SYMBOL(emac_lib_pm_save_add_notifier);

void emac_lib_pm_save_remove_notifier(void)
{
	pm_qos_remove_notifier(PM_QOS_POWER_SAVE, &pm_save_notifier);
}
EXPORT_SYMBOL(emac_lib_pm_save_remove_notifier);

void emac_lib_update_link_vars(const uint32_t dual_link)
{
	emac_lib_dual_emac = dual_link;
	pm_qos_refresh_notifiers(PM_QOS_POWER_EMAC);
}
EXPORT_SYMBOL(emac_lib_update_link_vars);

static struct phy_device *phy_power_get_phy(struct net_device *dev)
{
	struct phy_device *phy_dev = NULL;
	struct emac_common *arapc;

	if (!dev) {
		return NULL;
	}

	if (!netif_running(dev)) {
		return NULL;
	}

	arapc = netdev_priv(dev);

	if (arapc) {
		phy_dev = arapc->phy_dev;
	}

	return phy_dev;
}

static int phy_power_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char cmd[PHY_PW_CMD_MAX_LEN];
	int ret = 0;
	struct phy_device *phy_dev;
	struct net_device *dev = data;

	phy_dev = phy_power_get_phy(dev);

	if (!phy_dev) return -EINVAL;

	if (!count) {
		return -EINVAL;
	} else if (count > (PHY_PW_CMD_MAX_LEN - 1)) {
		return -EINVAL;
	} else if (copy_from_user(cmd, buffer, count)) {
		return -EINVAL;
	}

	cmd[count - 1] = '\0';

	if (strcmp(cmd, "1") == 0) {
		emac_pm_enter_to_halt(phy_dev);
		printk(KERN_INFO "%s: emac halted\n", DRV_NAME);
	} else if (strcmp(cmd, "0") == 0) {
		emac_pm_return_from_halt(phy_dev);
		printk(KERN_INFO "%s: emac resumed\n", DRV_NAME);
	} else {
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static int phy_power_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct net_device *dev = data;
	struct phy_device *phy_dev;
	int status;

	phy_dev = phy_power_get_phy(dev);

	if (!phy_dev) return -EINVAL;

	if (phy_dev->state == PHY_HALTED) {
		status = 1;
	} else {
		status = 0;
	}

	return sprintf(page, "%d\n", status);
}

static void phy_power_proc_name(char *buf, struct emac_common *arapc)
{
	sprintf(buf, "%s%d", PHY_PW_PROC_FILE_NAME, arapc->mac_id);
}

int emac_lib_phy_power_create_proc(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);
	char proc_name[12];

	phy_power_proc_name(proc_name, arapc);
	struct proc_dir_entry *entry = create_proc_entry(proc_name, 0600, NULL);
	if (!entry) {
		return -ENOMEM;
	}

	entry->write_proc = phy_power_write_proc;
	entry->read_proc = phy_power_read_proc;
	entry->data = dev;

	return 0;
}
EXPORT_SYMBOL(emac_lib_phy_power_create_proc);

void emac_lib_phy_power_remove_proc(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);
	char proc_name[12];

	phy_power_proc_name(proc_name, arapc);
	remove_proc_entry(proc_name, NULL);
}
EXPORT_SYMBOL(emac_lib_phy_power_remove_proc);

static int phy_reg_rw_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char cmd[PHY_PW_CMD_MAX_LEN];
	int ret = 0;
	struct phy_device *phy_dev;
	struct net_device *dev = (struct net_device *)data;
	int phyreg, val;
	char mode;

	phy_dev = phy_power_get_phy(dev);

	if (!phy_dev)
		return -EINVAL;

	if (!count)
		return -EINVAL;
	else if (count > (PHY_PW_CMD_MAX_LEN - 1))
		return -EINVAL;
	else if (copy_from_user(cmd, buffer, count))
		return -EINVAL;

	cmd[count] = '\0';

	sscanf(cmd, "%c %x %x", &mode, &phyreg, &val);

	if (mode == 'r') {
		val =  phy_read(phy_dev, phyreg);
		printk(KERN_ERR"0x%04x\n", val);
	} else if (mode == 'w') {
		ret = phy_write(phy_dev, phyreg, val);
		if (!ret) {
			printk(KERN_ERR"complete\n");
		}
	} else {
		printk(KERN_ERR"usage: echo [r|w] reg [val] > /proc/%s\n", PHY_REG_PROC_FILE_NAME);
	}

	return ret ? ret : count;
}

static void phy_reg_proc_name(char *buf, struct emac_common *arapc)
{
	sprintf(buf, "%s%d", PHY_REG_PROC_FILE_NAME, arapc->mac_id);
}

int emac_lib_phy_reg_create_proc(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);
	char proc_name[12] = {0};

	phy_reg_proc_name(proc_name, arapc);

	struct proc_dir_entry *entry = create_proc_entry(proc_name, 0600, NULL);
	if (!entry) {
		return -ENOMEM;
	}

	entry->write_proc = phy_reg_rw_proc;
	entry->data = dev;

	return 0;
}
EXPORT_SYMBOL(emac_lib_phy_reg_create_proc);

void emac_lib_phy_reg_remove_proc(struct net_device *dev)
{
	struct emac_common *arapc = netdev_priv(dev);
	char proc_name[12];

	phy_reg_proc_name(proc_name, arapc);
	remove_proc_entry(proc_name, NULL);
}
EXPORT_SYMBOL(emac_lib_phy_reg_remove_proc);

/* The max time (in ms) to wait for statistics counters to return a value */
static const int max_stat_loop_count = 20;

static uint32_t emac_lib_rxstatistics_counter(struct net_device *dev, int counter)
{
	uint32_t val;
	struct emac_common *arapc = netdev_priv(dev);

	if (!arapc || counter < 0 || counter > RxLastStatCounter) {
		return 0;
	}

	if (!(arapc->emac_cfg & EMAC_PHY_NOT_IN_USE) && arapc->phy_dev &&
			(arapc->phy_dev->state == PHY_HALTED)) {
		return 0;
	}

	if (!emac_wait(arapc, EMAC_MAC_RXSTAT_CTRL, RxStatReadBusy, 0, 10 * max_stat_loop_count, __FUNCTION__)) {
		return 0;
	}

	emac_wr(arapc, EMAC_MAC_RXSTAT_CTRL, RxStatReadBusy | counter);

	if (!emac_wait(arapc, EMAC_MAC_RXSTAT_CTRL, RxStatReadBusy, 0, max_stat_loop_count, __FUNCTION__)) {
		return 0;
	}

	val = emac_rd(arapc, EMAC_MAC_RXSTAT_DATA_HIGH) << 16;
	val |= (emac_rd(arapc, EMAC_MAC_RXSTAT_DATA_LOW) & 0xffff);

	return val;
}

static uint32_t emac_lib_txstatistics_counter(struct net_device *dev, int counter)
{
	uint32_t val;
	struct emac_common *arapc = netdev_priv(dev);

	if (!arapc || counter < 0 || counter > TxLastStatCounter) {
		return 0;
	}

	if (!(arapc->emac_cfg & EMAC_PHY_NOT_IN_USE) && arapc->phy_dev &&
			(arapc->phy_dev->state == PHY_HALTED)) {
		return 0;
	}

	if (!emac_wait(arapc, EMAC_MAC_TXSTAT_CTRL, TxStatReadBusy, 0, 10 * max_stat_loop_count, __FUNCTION__)) {
		return 0;
	}

	emac_wr(arapc, EMAC_MAC_TXSTAT_CTRL, TxStatReadBusy | counter);

	if (!emac_wait(arapc, EMAC_MAC_TXSTAT_CTRL, TxStatReadBusy, 0, max_stat_loop_count, __FUNCTION__)) {
		return 0;
	}

	val = emac_rd(arapc, EMAC_MAC_TXSTAT_DATA_HIGH) << 16;
	val |= (emac_rd(arapc, EMAC_MAC_TXSTAT_DATA_LOW) & 0xffff);

	return val;
}

static void emac_lib_stat_read_hw_counters(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct emac_stats *stats = &privc->stats[privc->current_stats];
	int i;

	/*
	 * If privc->emac_cfg has flag EMAC_PHY_NOT_IN_USE/EMAC_PHY_AR8236/EMAC_PHY_AR8327/EMAC_PHY_MV88E6071 set,
	 * the privc->phy_dev will be NULL after initialization, it does not mean that there is no phy device,
	 * still can read Tx/Rx statistics from HW
	 */
	if (privc->phy_dev && privc->phy_dev->state != PHY_RUNNING)
		return;

	for (i = 0; i < ARRAY_SIZE(stats->tx); i++)
		stats->tx[i] = emac_lib_txstatistics_counter(dev, i);
	for (i = 0; i < ARRAY_SIZE(stats->rx); i++)
		stats->rx[i] = emac_lib_rxstatistics_counter(dev, i);
}

static void emac_lib_stat_read_hw_dma(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct emac_stats *stats = &privc->stats[privc->current_stats];

	stats->dma[DmaMissedFrame] += emac_rd(privc, EMAC_DMA_MISSED_FRAMES) & 0x7fffffff;
	stats->dma[DmaStopFlush] += emac_rd(privc, EMAC_DMA_STOP_FLUSHES) & 0x7fffffff;
}

static void emac_lib_stat_read_hw(struct net_device *dev)
{
	emac_lib_stat_read_hw_counters(dev);
	emac_lib_stat_read_hw_dma(dev);
}

static int emac_lib_stats_switch(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct emac_stats *becoming_old_stats = &privc->stats[privc->current_stats];
	struct emac_stats *becoming_new_stats = &privc->stats[!privc->current_stats];
	int i;

	emac_lib_stat_read_hw(dev);
	memset(&dev->stats, 0, sizeof(dev->stats));
	memset(becoming_new_stats, 0, sizeof(*becoming_new_stats));

	/* DMA counters not cumulative, so copy them */
	for (i = 0; i <= DmaLastStatCounter; i++)
		becoming_new_stats->dma[i] = becoming_old_stats->dma[i];

	/* Flip stats structures */
	privc->current_stats = !privc->current_stats;

	return 0;
}

static uint32_t emac_lib_stat_rx(struct emac_common *privc, enum ArasanRxStatisticsCounters stat)
{
	return privc->stats[privc->current_stats].rx[stat] -
		privc->stats[!privc->current_stats].rx[stat];
}

static uint32_t emac_lib_stat_tx(struct emac_common *privc, enum ArasanTxStatisticsCounters stat)
{
	return privc->stats[privc->current_stats].tx[stat] -
		privc->stats[!privc->current_stats].tx[stat];
}

static uint32_t emac_lib_stat_dma(struct emac_common *privc, enum emac_dma_counter stat)
{
	return privc->stats[privc->current_stats].dma[stat] -
		privc->stats[!privc->current_stats].dma[stat];
}

struct net_device_stats *emac_lib_stats(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;

	if (!netif_device_present(dev)) {
		return 0;
	}

	emac_lib_stat_read_hw(dev);

	stats->rx_packets = emac_lib_stat_rx(privc, FramesRxTotal);
	stats->tx_packets = emac_lib_stat_tx(privc, FramesSentTotal);
	stats->rx_bytes = emac_lib_stat_rx(privc, OctetsRxTotal);
	stats->tx_bytes = emac_lib_stat_tx(privc, OctetsSentOK);
	stats->rx_errors = emac_lib_stat_rx(privc, FramesRxErrTotal);
	stats->tx_errors = emac_lib_stat_tx(privc, FramesSentError);
	stats->multicast = emac_lib_stat_rx(privc, FramesRxMulticast);
	stats->collisions = emac_lib_stat_tx(privc, FramesSentSingleCol) +
		emac_lib_stat_tx(privc, FramesSentMultipleCol) +
		emac_lib_stat_tx(privc, FramesSentLateCol) +
		emac_lib_stat_tx(privc, FramesSentExcessiveCol);

	stats->rx_length_errors = emac_lib_stat_rx(privc, FramesRxLenErr);
	stats->rx_crc_errors = emac_lib_stat_rx(privc, FramesRxCrcErr);
	stats->rx_frame_errors = emac_lib_stat_rx(privc, FramesRxAlignErr);
	stats->rx_fifo_errors = emac_lib_stat_rx(privc, FramesRxDroppedBufFull) +
		emac_lib_stat_rx(privc, FramesRxTruncatedBufFull);
	stats->rx_missed_errors =  emac_lib_stat_dma(privc, DmaMissedFrame);

	stats->rx_unicast_packets = emac_lib_stat_rx(privc, FramesRxUnicast);
	stats->tx_unicast_packets = emac_lib_stat_tx(privc, FramesSentUnicast);
	stats->tx_multicast_packets = emac_lib_stat_tx(privc, FramesSentMulticast);
	stats->rx_broadcast_packets = emac_lib_stat_rx(privc, FramesRxBroadcast);
	stats->tx_broadcast_packets = emac_lib_stat_tx(privc, FramesSentBroadcast);

	return stats;
}
EXPORT_SYMBOL(emac_lib_stats);

uint32_t qtn_eth_rx_lost_get(struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct emac_stats *stats = &privc->stats[privc->current_stats];

	emac_lib_stat_read_hw_dma(dev);

	return stats->dma[DmaMissedFrame];
}
EXPORT_SYMBOL(qtn_eth_rx_lost_get);

static const char * const tx_stat_names[] = {
	"OK", "Total", "OK", "Err", "SingleClsn", "MultipleClsn",
	"LateClsn", "ExcessiveClsn", "Unicast", "Multicast",
	"Broadcast", "Pause",
};

static const char * tx_stat_name_prefix(enum ArasanTxStatisticsCounters stat)
{
	if (stat == OctetsSentOK)
		return "Octets";
	return "Frames";
}

static const char * const rx_stat_names[] = {
	"OK", "Total", "CrcErr", "AlignErr", "TotalErr", "OK",
	"Total", "Unicast", "Multicast", "Broadcast", "Pause",
	"LenErr", "Undersized", "Oversized", "Frags", "Jabber",
	"Len64", "Len65-127", "Len128-255", "Len256-511", "Len512-1023",
	"Len1024-1518", "LenOver1518", "DroppedBufFull", "TruncBufFull",
};

static const char *rx_stat_name_prefix(enum ArasanRxStatisticsCounters stat)
{
	if (stat == OctetsRxOK || stat == OctetsRxTotal)
		return "Octets";
	return "Frames";
}

static const char * const dma_stat_names[] = {
	"DmaMissedFrame", "DmaStopFlush",
};

int emac_lib_stats_sprintf(char *buf, struct net_device *dev)
{
	struct emac_common *privc = netdev_priv(dev);
	struct emac_stats *stats = &privc->stats[privc->current_stats];
	char *p = buf;
	int i;

	emac_lib_stat_read_hw(dev);

	for (i = 0; i < ARRAY_SIZE(stats->tx); i++)
		p += sprintf(p, "%2s#%02d %6s%-14s : %10d\n",
				"Tx", i, tx_stat_name_prefix(i), tx_stat_names[i], stats->tx[i]);
	for (i = 0; i < ARRAY_SIZE(stats->rx); i++)
		p += sprintf(p, "%2s#%02d %6s%-14s : %10d\n",
				"Rx", i, rx_stat_name_prefix(i), rx_stat_names[i], stats->rx[i]);
	for (i = 0; i < ARRAY_SIZE(stats->dma); i++)
		p += sprintf(p, "%-14s             : %10d\n",
				dma_stat_names[i], stats->dma[i]);

	return p - buf;
}
EXPORT_SYMBOL(emac_lib_stats_sprintf);

int emac_lib_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct emac_common *arapc = netdev_priv(dev);

	if (!netif_running(dev)) {
		return -EINVAL;
	}

	if (!arapc->phy_dev) {
		/* PHY not controllable */
		return -EINVAL;
	}

	switch(cmd) {
	case SIOCRDEVSTATS:
		return emac_lib_stats_switch(dev);

	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		/* Accept these */
		break;
	default:
		/* Reject the rest */
		return -EOPNOTSUPP;
	}

	return phy_mii_ioctl(arapc->phy_dev, if_mii(rq), cmd);
}
EXPORT_SYMBOL(emac_lib_ioctl);

#ifdef CONFIG_QVSP
struct qvsp_wrapper emac_qvsp;
EXPORT_SYMBOL(emac_qvsp);

void qvsp_wrapper_init(struct qvsp_ext_s *qvsp, QVSP_CHECK_FUNC_PROTOTYPE(*check_func))
{
	emac_qvsp.qvsp = qvsp;
	emac_qvsp.qvsp_check_func = check_func;
}
EXPORT_SYMBOL(qvsp_wrapper_init);

void qvsp_wrapper_exit(void)
{
	emac_qvsp.qvsp_check_func = NULL;
	emac_qvsp.qvsp = NULL;
}
EXPORT_SYMBOL(qvsp_wrapper_exit);
#endif	// CONFIG_QVSP

#if EMAC_REG_DEBUG
struct emac_reg_debug_s {
	const char *name;
	uint16_t offset;
	uint16_t count;
};

#define EMACR(x)	{ #x, (x), 1 }
#define EMACRR(x, r)	{ #x, (x), r }
const static struct emac_reg_debug_s emac_reg_debug_list[] = {
	EMACR(EMAC_DMA_CONFIG),
	EMACR(EMAC_DMA_CTRL),
	EMACR(EMAC_DMA_STATUS_IRQ),
	EMACR(EMAC_DMA_INT_ENABLE),
	EMACR(EMAC_DMA_TX_AUTO_POLL),
	EMACR(EMAC_DMA_TX_POLL_DEMAND),
	EMACR(EMAC_DMA_RX_POLL_DEMAND),
	EMACR(EMAC_DMA_TX_BASE_ADDR),
	EMACR(EMAC_DMA_RX_BASE_ADDR),
	EMACR(EMAC_DMA_MISSED_FRAMES),
	EMACR(EMAC_DMA_STOP_FLUSHES),
	EMACR(EMAC_DMA_RX_IRQ_MITIGATION),
	EMACR(EMAC_DMA_CUR_TXDESC_PTR),
	EMACR(EMAC_DMA_CUR_TXBUF_PTR),
	EMACR(EMAC_DMA_CUR_RXDESC_PTR),
	EMACR(EMAC_DMA_CUR_RXBUF_PTR),
	EMACR(EMAC_MAC_GLOBAL_CTRL),
	EMACR(EMAC_MAC_TX_CTRL),
	EMACR(EMAC_MAC_RX_CTRL),
	EMACR(EMAC_MAC_MAX_FRAME_SIZE),
	EMACR(EMAC_MAC_TX_JABBER_SIZE),
	EMACR(EMAC_MAC_RX_JABBER_SIZE),
	EMACR(EMAC_MAC_ADDR_CTRL),
	EMACR(EMAC_MAC_ADDR1_HIGH),
	EMACR(EMAC_MAC_ADDR1_MED),
	EMACR(EMAC_MAC_ADDR1_LOW),
	EMACR(EMAC_MAC_ADDR2_HIGH),
	EMACR(EMAC_MAC_ADDR2_MED),
	EMACR(EMAC_MAC_ADDR2_LOW),
	EMACR(EMAC_MAC_ADDR3_HIGH),
	EMACR(EMAC_MAC_ADDR3_MED),
	EMACR(EMAC_MAC_ADDR3_LOW),
	EMACR(EMAC_MAC_ADDR4_HIGH),
	EMACR(EMAC_MAC_ADDR4_MED),
	EMACR(EMAC_MAC_ADDR4_LOW),
	EMACR(EMAC_MAC_TABLE1),
	EMACR(EMAC_MAC_TABLE2),
	EMACR(EMAC_MAC_TABLE3),
	EMACR(EMAC_MAC_TABLE4),
	EMACR(EMAC_MAC_FLOW_CTRL),
	EMACR(EMAC_MAC_FLOW_PAUSE_GENERATE),
	EMACR(EMAC_MAC_FLOW_SA_HIGH),
	EMACR(EMAC_MAC_FLOW_SA_MED),
	EMACR(EMAC_MAC_FLOW_SA_LOW),
	EMACR(EMAC_MAC_FLOW_DA_HIGH),
	EMACR(EMAC_MAC_FLOW_DA_MED),
	EMACR(EMAC_MAC_FLOW_DA_LOW),
	EMACR(EMAC_MAC_FLOW_PAUSE_TIMEVAL),
	EMACR(EMAC_MAC_MDIO_CTRL),
	EMACR(EMAC_MAC_MDIO_DATA),
	EMACR(EMAC_MAC_RXSTAT_CTRL),
	EMACR(EMAC_MAC_RXSTAT_DATA_HIGH),
	EMACR(EMAC_MAC_RXSTAT_DATA_LOW),
	EMACR(EMAC_MAC_TXSTAT_CTRL),
	EMACR(EMAC_MAC_TXSTAT_DATA_HIGH),
	EMACR(EMAC_MAC_TXSTAT_DATA_LOW),
	EMACR(EMAC_MAC_TX_ALMOST_FULL),
	EMACR(EMAC_MAC_TX_START_THRESHOLD),
	EMACR(EMAC_MAC_RX_START_THRESHOLD),
	EMACR(EMAC_MAC_INT),
	EMACR(EMAC_MAC_INT_ENABLE),
	EMACR(TOPAZ_EMAC_WRAP_CTRL),
	EMACR(TOPAZ_EMAC_RXP_CTRL),
	EMACR(TOPAZ_EMAC_TXP_CTRL),
	EMACR(TOPAZ_EMAC_TXP_Q_FULL),
	EMACR(TOPAZ_EMAC_TXP_DESC_PTR),
	EMACR(TOPAZ_EMAC_BUFFER_POOLS),
	EMACR(TOPAZ_EMAC_TXP_STATUS),
	EMACR(TOPAZ_EMAC_DESC_LIMIT),
	EMACR(TOPAZ_EMAC_RXP_PRIO_CTRL),
	EMACR(TOPAZ_EMAC_RXP_OUTPORT_CTRL),
	EMACR(TOPAZ_EMAC_RXP_OUTNODE_CTRL),
	EMACR(TOPAZ_EMAC_RXP_VLAN_PRI_TO_TID),
	EMACR(TOPAZ_EMAC_RXP_VLAN_PRI_CTRL),
	EMACR(TOPAZ_EMAC_RXP_VLAN_TAG_0_1),
	EMACR(TOPAZ_EMAC_RXP_VLAN_TAG_2_3),
	EMACR(TOPAZ_EMAC_RXP_IP_CTRL),
	EMACR(TOPAZ_EMAC_RXP_DPI_CTRL),
	EMACR(TOPAZ_EMAC_RXP_STATUS),
	EMACR(TOPAZ_EMAC_RXP_CST_SEL),
	EMACR(TOPAZ_EMAC_RXP_FRAME_CNT_CLEAR),
	EMACR(TOPAZ_EMAC_FRM_COUNT_ERRORS),
	EMACR(TOPAZ_EMAC_FRM_COUNT_TOTAL),
	EMACR(TOPAZ_EMAC_FRM_COUNT_DA_MATCH),
	EMACR(TOPAZ_EMAC_FRM_COUNT_SA_MATCH),
	EMACRR(TOPAZ_EMAC_RXP_IP_DIFF_SRV_TID_REG(0), 8),
	EMACR(TOPAZ_EMAC_RXP_IP_CTRL),
	EMACRR(TOPAZ_EMAC_RXP_DPI_TID_MAP_REG(0), 8),
	EMACRR(TOPAZ_EMAC_RX_DPI_FIELD_VAL(0), TOPAZ_EMAC_NUM_DPI_FIELDS),
	EMACRR(TOPAZ_EMAC_RX_DPI_FIELD_MASK(0), TOPAZ_EMAC_NUM_DPI_FIELDS),
	EMACRR(TOPAZ_EMAC_RX_DPI_FIELD_CTRL(0), TOPAZ_EMAC_NUM_DPI_FIELDS),
	EMACRR(TOPAZ_EMAC_RX_DPI_FIELD_GROUP(0), TOPAZ_EMAC_NUM_DPI_FILTERS),
	EMACRR(TOPAZ_EMAC_RX_DPI_OUT_CTRL(0), TOPAZ_EMAC_NUM_DPI_FILTERS),
	EMACRR(TOPAZ_EMAC_RX_DPI_IPT_GROUP(0), TOPAZ_EMAC_NUM_DPI_FILTERS),
};

void emac_lib_reg_debug(u32 base) {
	int i;
	int j;
	for (i = 0; i < ARRAY_SIZE(emac_reg_debug_list); i++) {
		u32 regcount = emac_reg_debug_list[i].count;
		for (j = 0; j < regcount; j++) {
			u32 reg = base + emac_reg_debug_list[i].offset + 4 * j;
			u32 val = readl(reg);
			printk("%s: 0x%08x = 0x%08x %s[%d]\n",
					__FUNCTION__, reg, val, emac_reg_debug_list[i].name, j);
		}
	}
}
EXPORT_SYMBOL(emac_lib_reg_debug);
#endif	// EMAC_REG_DEBUG

MODULE_LICENSE("GPL");


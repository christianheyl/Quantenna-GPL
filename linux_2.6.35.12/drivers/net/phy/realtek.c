/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>


MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

#define RTL821x_PHYSR		0x11
#define RTL821x_PHYSR_DUPLEX	0x2000
#define RTL821x_PHYSR_SPEED	0xc000
#define RTL821x_INER		0x12
#define RTL821x_INER_INIT	0x6400
#define RTL821x_INSR		0x13

#define RTL8211DS_PAGSEL	0x1F	/* Page mode slelect */
#define PAGSEL_PAGEXT		0x7		/* Switch to extension page */
#define PAGSEL_PAG0			0x0     /* Switch to page 0 */

#define RTL8211DS_PAGNO		0x1E	/* Page number */
#define PAGNO_140			0x8c	/* Page 140 */

#define RTL8211DS_RXCONFIG	0x18	/* Rx config ( phy -> mac) */
#define RXCONFIG_LINK		0x8000
#define RXCONFIG_DUPLEX		0x1000
#define RXCONFIG_SPEED		0x0c00
#define RXCONFIG_SPEED_BIT	10

#define RTL8211DS_SDSR		0x16	/* SerDes register */
#define SDSR_SPEED			0x3000	/* Rx clock */
#define SDSR_1000			0x2000
#define SDSR_100			0x1000
#define SDSR_10				0x0000

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl821x_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL821x_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}


static int rtl8211ds_config_init(struct phy_device *phydev)
{

	phydev->autoneg = AUTONEG_DISABLE;
	phydev->interface = PHY_INTERFACE_MODE_SGMII;
	return 0;
}


int rtl8211ds_update_link(struct phy_device *phydev)
{
	int status = 0;
	int link_status = 0;
	int speed_sel = 0;

	phy_write(phydev, RTL8211DS_PAGSEL, PAGSEL_PAGEXT);
	phy_write(phydev, RTL8211DS_PAGNO, PAGNO_140);
	link_status = phy_read(phydev, RTL8211DS_RXCONFIG);
	if (link_status & RXCONFIG_LINK) {
		//printk("<0>RTL8211DS up\n");
		phydev->link = 1;
	} else {
		//printk("<0>RTL8211DS down\n");
		phydev->link = 0;
	}
	speed_sel = phy_read(phydev, RTL8211DS_SDSR);
	speed_sel &= ~SDSR_SPEED;
	switch ((link_status & RXCONFIG_SPEED) >> RXCONFIG_SPEED_BIT) {
	case 0: /* Force 10M */
		//printk("<0>RTL8211DS: 0x%x, 10-", speed_sel);
		phydev->speed = SPEED_10;
		speed_sel |= SDSR_10;
		break;
	case 1: /* Force 100M */
		//printk("<0>RTL8211DS: 0x%x, 100-", speed_sel);
		phydev->speed = SPEED_100;
		speed_sel |= SDSR_100;
		break;
	case 2: /* Force 1G */
		//printk("<0>RTL8211DS: 0x%x, 1000-", speed_sel);
		phydev->speed = SPEED_1000;
		speed_sel |= SDSR_1000;
		 break;
	default:
		//printk("<0>RTL8211DS unknown speed\n");
		status = -1;
		}
	if (link_status & RXCONFIG_DUPLEX) {
		//printk("Full: 0x%x\n",speed_sel);
		phydev->duplex = DUPLEX_FULL;
		}
	else{
		//printk("Half: 0x%x\n",speed_sel);
		phydev->duplex = DUPLEX_HALF;
		}
	if (phydev->link)
		phy_write(phydev, RTL8211DS_SDSR, speed_sel);
	phy_write(phydev, RTL8211DS_PAGSEL, PAGSEL_PAG0);

	return status;
}


/**
 * genphy_read_status - check the link status and update current link state
 * @phydev: target phy_device struct
 *
 * Description: Check the link, then figure out the current state
 *   by comparing what we advertise with what the link partner
 *   advertises.  Start by checking the gigabit possibilities,
 *   then move on to 10/100.
 */
int rtl8211ds_read_status(struct phy_device *phydev)
{
	int status = 0;

	status = rtl8211ds_update_link(phydev);

	return status;
}


/* RTL8211B */
static struct phy_driver rtl821x_driver = {
	.phy_id		= 0x001cc912,
	.name		= "RTL821x Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &rtl821x_ack_interrupt,
	.config_intr	= &rtl821x_config_intr,
	.driver		= { .owner = THIS_MODULE,},
};

static struct phy_driver rtl8211ds_driver = {
	.phy_id		= 0x001cc914,
	.name		= "RTL8211DS Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
	.features	= PHY_GBIT_FEATURES,
	.flags		=  PHY_POLL,
	.config_init	= rtl8211ds_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= rtl8211ds_read_status,
	.driver		= { .owner = THIS_MODULE,},
};

static int __init realtek_init(void)
{
	int ret;

	ret = phy_driver_register(&rtl821x_driver);

	return ret;
}

static void __exit realtek_exit(void)
{
	phy_driver_unregister(&rtl821x_driver);
}

static int __init realtek_8211ds_init(void)
{
	int ret;

	ret = phy_driver_register(&rtl8211ds_driver);

	return ret;
}

static void __exit realtek_8211ds_exit(void)
{
	phy_driver_unregister(&rtl8211ds_driver);
}

module_init(realtek_init);
module_exit(realtek_exit);
module_init(realtek_8211ds_init);
module_exit(realtek_8211ds_exit);


static struct mdio_device_id realtek_tbl[] = {
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);

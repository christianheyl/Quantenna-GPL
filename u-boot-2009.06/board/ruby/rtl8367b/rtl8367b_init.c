/*
 * Copyright (C) 2012 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * $Revision: $
 * $Date: $
 *
 * Purpose : RTK switch driver module init for RTL8367/RTL8367B
 * Feature : Init switch and configure it for operation
 *
 */

#include "rtl8367b_init.h"
#include "rtl8367b_smi.h"
#include <asm/arch/arasan_emac_ahb.h>

ret_t rtl8367b_poll_linkup(struct emac_private *priv)
{
	/* either PHY1 or PHY2 should have link established */
	const uint32_t delay_us = 1000;
	const int poll_secs = 2;
	const uint32_t mask = PhyLinkIsUp | PhyAutoNegComplete;
	const unsigned long start = get_timer(0);

	printf("%s: polling for link up...\n", __FUNCTION__);

	while (1) {
		int phy;

		for (phy = RTL8367B_QTN_EXT_PHY_ADDR_MIN; phy <= RTL8367B_QTN_EXT_PHY_ADDR_MAX; phy++) {
			uint32_t val;

			if (rtl8367b_getAsicPHYReg(phy, PhyBMSR, &val) != RT_ERR_OK) {
				return -1;
			}

			if ((val & mask) == mask) {
				printf("%s: link found, phy %d\n",
						__FUNCTION__, phy);
				return 0;
			}
		}

		if (ctrlc() || (get_timer(start) > (poll_secs * CONFIG_SYS_HZ))) {
			break;
		}

		udelay(delay_us);
	}

	printf("%s: no link found\n", __FUNCTION__);

	return -1;
}

static void rtl8367b_ext_reset(void)
{
	const int delay_us = 20000;
	uint32_t mask = RUBY_SYS_CTL_RESET_EXT;

	writel(mask, RUBY_SYS_CTL_CPU_VEC_MASK);

	writel(0, RUBY_SYS_CTL_CPU_VEC);
	udelay(delay_us);
	writel(mask, RUBY_SYS_CTL_CPU_VEC);
	udelay(delay_us);
	writel(0, RUBY_SYS_CTL_CPU_VEC);
	udelay(delay_us);

	writel(0, RUBY_SYS_CTL_CPU_VEC_MASK);
}

static uint32_t smi_mdio_read_adapter(struct emac_private *priv, uint8_t reg)
{
	uint32_t val;

	if (rtl8367b_getAsicPHYReg(priv->phy_addr, reg, &val) != RT_ERR_OK) {
		return MDIO_READ_FAIL;
	}

	return val;
}

static int smi_mdio_write_adapter(struct emac_private *priv, uint8_t reg, uint16_t value)
{
	return rtl8367b_setAsicPHYReg(priv->phy_addr, reg, value);
}

static ret_t rtl8367b_init_port(rtk_port_mac_ability_t *mac_cfg, rtk_mode_ext_t mode, rtk_ext_port_t port)
{
	ret_t ret;
	rtk_data_t rgmii_tx_delay = 0;
	rtk_data_t rgmii_rx_delay = 4;

	ret = rtk_port_macForceLinkExt_set(port, mode, mac_cfg);
	if (RT_ERR_OK != ret) {
		printf("rtk_port_macForceLinkExt_set failed, port %d (%d)\n", port, ret);
		return ret;
	}

	ret = rtk_port_rgmiiDelayExt_set(port, rgmii_tx_delay, rgmii_rx_delay);
	if (RT_ERR_OK != ret) {
		printf("rtk_port_rgmiiDelayExt_set failed, port %d (%d)\n", port, ret);
		return ret;
	}

	return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367b_init
 * Description:
 *      Initialize RTL8367B Chipsets
 * Return:
 *      RT_ERR_OK	   - Success
 *      RT_ERR_FAILED  - Failure
 * Note:
 *      None
 */
ret_t rtl8367b_init(struct emac_private *priv, uint32_t emac_cfg)
{
	ret_t ret = RT_ERR_OK;
	rtk_port_mac_ability_t mac_cfg;
	rtk_mode_ext_t mode;

	printf("%s...\n", __FUNCTION__);

	priv->mdio->read = &smi_mdio_read_adapter;
	priv->mdio->write = &smi_mdio_write_adapter;

	rtl8367b_ext_reset();

	smi_mdio_base_set(priv->mdio->base);
	ret = rtk_switch_init();
	if (RT_ERR_OK != ret) {
		printf("rtl8635MB switch init failed!!! (%d)\n", ret);
		return ret;
	}

	/*
	 * Set external interface 0 to RGMII with Force mode, 1000M, Full-duplex,
	 * enable TX&RX pause
	 */
	mode = MODE_EXT_RGMII;
	mac_cfg.forcemode = MAC_FORCE;
	mac_cfg.speed = SPD_1000M;
	mac_cfg.duplex = FULL_DUPLEX;
	mac_cfg.link = PORT_LINKUP;
	mac_cfg.nway = DISABLED;
	mac_cfg.txpause = ENABLED;
	mac_cfg.rxpause = ENABLED;

	if (emac_cfg & EMAC_PHY_RTL8363SB_P0) {
		ret = rtl8367b_init_port(&mac_cfg, mode, EXT_PORT_0);
		if (RT_ERR_OK != ret) {
			return ret;
		}
	}

	if (emac_cfg & EMAC_PHY_RTL8363SB_P1) {
		ret = rtl8367b_init_port(&mac_cfg, mode, EXT_PORT_1);
		if (RT_ERR_OK != ret) {
			return ret;
		}
	}

	printf("%s completed successfully\n", __FUNCTION__);

	return ret;
}


/*
 * (C) Copyright 2010 Quantenna Communications Inc.
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

#ifndef _RUBY_BOARD_DB_
#define _RUBY_BOARD_DB_

#include "ruby_platform.h"

#ifdef UBOOT_BOARD_AUTOCONFIG
/*
 * UBoot dynamic board config
 * Placed here to allow for simplified maintenence
 */
struct ruby_cfgstr_map {
	const char *name;
	const int val;
};
#define CFGSTR(x)	{ #x, (x) }

struct ruby_board_param {
	const struct ruby_cfgstr_map * const p_map;
	const uint32_t p_index;
};

const struct ruby_cfgstr_map g_cfgstr_board_id_cfg[] = {
	{ "UNIVERSAL_ID", QTN_RUBY_UNIVERSAL_BOARD_ID },
	{ 0 , 0 }
};

/* Board name */
static const struct ruby_cfgstr_map g_cfgstr_name_cfg[] = {
	{ "hw_QTN_test", 0 },
	{ 0 , 0 }
};

/* DDR Configuration strings */
/* DDR Type */
static const struct ruby_cfgstr_map g_cfgstr_ddr_cfg[] = {
	CFGSTR(DDR_16_ETRON),
	CFGSTR(DDR_32_MICRON),
	CFGSTR(DDR_16_MICRON),
	CFGSTR(DDR_32_ETRON),
	CFGSTR(DDR_32_SAMSUNG),
	CFGSTR(DDR_16_SAMSUNG),
	CFGSTR(DDR_16_HYNIX),
	CFGSTR(DDR3_16_WINBOND),
	CFGSTR(DDR3_32_WINBOND),
	CFGSTR(DEFAULT_DDR_CFG),
	{ 0 , 0 }
};

/* DDR Size */
static const struct ruby_cfgstr_map g_cfgstr_ddr_size[] = {
	CFGSTR(DDR_32MB),
	CFGSTR(DDR_64MB),
	CFGSTR(DDR_128MB),
	CFGSTR(DDR_256MB),
	CFGSTR(DDR_AUTO),
	CFGSTR(DEFAULT_DDR_SIZE),
	{ 0 , 0 }
};

/* DDR Speed */
static const struct ruby_cfgstr_map g_cfgstr_ddr_speed[] = {
	CFGSTR(DDR_160),
	CFGSTR(DDR_250),
	CFGSTR(DDR_320),
	CFGSTR(DDR_400),
	CFGSTR(DDR3_320MHz),
	CFGSTR(DDR3_400MHz),
	CFGSTR(DDR3_500MHz),
	CFGSTR(DDR3_640MHz),
	CFGSTR(DDR3_800MHz),
	CFGSTR(DEFAULT_DDR_SPEED),
	{ 0 , 0 }
};

/* EMAC configuration strings */
static const struct ruby_cfgstr_map g_cfgstr_emac_cfg[] = {
	{ "EMAC_IN_USE", EMAC_IN_USE },
	{ "EMAC_RGMII_AN", EMAC_IN_USE },
	{ "EMAC_NOT_IN_USE", EMAC_NOT_IN_USE },
	{ "EMAC_MII_AN", (EMAC_IN_USE | EMAC_PHY_MII) },
	{ "EMAC_MII_100M", (EMAC_IN_USE | EMAC_PHY_NOT_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB) },
	{ "EMAC_MII_100M_PHY", (EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB) },
	{ "EMAC_AR8327_RGMII", (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_AR8327) },
	{ "EMAC_RTL8363S_RGMII", (EMAC_IN_USE |  EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE) },
	{ "EMAC_RTL8363SB_RGMII_P0", (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_RTL8363SB_P0) },
	{ "EMAC_RTL8363SB_RGMII_P1", (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_RTL8363SB_P1) },
	{ "EMAC_RTL8363SB_RGMII_BONDED", (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE |
			EMAC_PHY_RTL8363SB_P0 | EMAC_PHY_RTL8363SB_P1 | EMAC_BONDED) },
	{ "EMAC_RTL8211E_RGMII", (EMAC_IN_USE |  EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE) },
	{ "EMAC_88E6071_MII", (EMAC_MV88E6071) },
	{ "EMAC_B2B_RGMII", (EMAC_IN_USE |  EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE) },
	{ "EMAC_B2B_RGMII_100M", (EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE) },
	{ "EMAC_B2B_RGMII_1000M", (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE) },
	{ "EMAC_AR8236_MII", (EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_AR8236) },
	{ "EMAC_MII_GPIO1_RST", (EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_GPIO1_RESET) },
	{ "EMAC_MII_100M_GPIO13_RST", (EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_GPIO13_RESET | EMAC_PHY_FORCE_100MB) },
	{ "DEFAULT_EMAC", (EMAC_NOT_IN_USE) },
	{ 0 , 0 }
};


static const struct ruby_cfgstr_map g_cfgstr_emac_phyaddr[] = {
	CFGSTR(24),
	CFGSTR(31),
	CFGSTR(EMAC_PHY_ADDR_SCAN),
	{ "EMAC_PHY0_ADDR", 1 },
	{ "EMAC_PHY1_ADDR", 3 },
	{ "DEFAULT_PHY_ADDR", EMAC_PHY_ADDR_SCAN },
	{ 0 , 0 }
};

/* Wireless PHY */
static const struct ruby_cfgstr_map g_cfgstr_rfpa_cfg[] = {
	CFGSTR(QTN_RUBY_BRINGUP_RWPA),
	CFGSTR(QTN_RUBY_REF_RWPA),
	CFGSTR(QTN_RUBY_SIGE),
	CFGSTR(QTN_RUBY_WIFI_NONE),
	CFGSTR(QTN_TPZ_SE5003L1),
	CFGSTR(QTN_TPZ_SE5003L1_INV),
	CFGSTR(QTN_TPZ_SKY85703),
	CFGSTR(QTN_TPZ_DBS),
	CFGSTR(QTN_TPZ_SKY85405_BPF840),
	CFGSTR(QTN_TPZ_DBS),
	CFGSTR(QTN_TPZ_SE5502L),
	CFGSTR(QTN_TPZ_SKY85710_NG),
	CFGSTR(QTN_TPZ_DBS_5591),
	CFGSTR(QTN_TPZ_DBS_5591),
	CFGSTR(QTN_TPZ_DBS_NXP_BGU7224_BGU7258),
	CFGSTR(QTN_TPZ_2_4GHZ_NXP_BGU7224),
	CFGSTR(QTN_TPZ_5GHZ_NXP_BGU7258),
	CFGSTR(QTN_TPZ_5GHZ_SKY85728),
	{ "DEFAULT_WIFI_HW", QTN_RUBY_REF_RWPA },
	{ 0 , 0 }
};

/* SPI config */
static const struct ruby_cfgstr_map g_cfgstr_spi_cfg[] = {
	CFGSTR(SPI1_IN_USE),
	CFGSTR(SPI1_NOT_IN_USE),
	{ 0 , 0 }
};

/* UART Config */
static const struct ruby_cfgstr_map g_cfgstr_uart_cfg[] = {
	CFGSTR(UART1_NOT_IN_USE),
	CFGSTR(UART1_IN_USE),
	{ "DEFAULT_UART1", UART1_NOT_IN_USE },
	{ 0 , 0 }
};

/* RGMII Timing Config */
static const struct ruby_cfgstr_map g_cfgstr_rgmii_cfg[] = {
	{ "RGMII_DEFAULT_S2p7ns_H1p1ns", CONFIG_ARCH_RGMII_DEFAULT },
	{ "RGMII_S2p4ns_H1p4ns", CONFIG_ARCH_RGMII_DLL_TIMING },
	{ "RGMII_S1p8ns_H1p9ns", CONFIG_ARCH_RGMII_S1P8NS_H1P9NS },
	{ "RGMII_P1RX00TX0E", CONFIG_ARCH_RGMII_P1RX00TX0E },
	{ "RGMII_710F", CONFIG_ARCH_RGMII_710F },
	{ "RGMII_NODELAY", CONFIG_ARCH_RGMII_NODELAY },
	{ "DEFAULT_RGMII_TIMING", CONFIG_ARCH_RGMII_DEFAULT },
	{ 0 , 0 }
};


/* PCIE Config */
static const struct ruby_cfgstr_map g_cfgstr_pcie_cfg[] = {
	CFGSTR(PCIE_NOT_IN_USE),
	CFGSTR(PCIE_ENDPOINT),
	CFGSTR(PCIE_ROOTCOMPLEX),
	{ 0 , 0 }
};

/* Flash config */
static const struct ruby_cfgstr_map g_cfgstr_flash_cfg[] = {
	CFGSTR(FLASH_SIZE_JEDEC),
	CFGSTR(FLASH_32MB),
	CFGSTR(FLASH_16MB),
	CFGSTR(FLASH_8MB),
	CFGSTR(FLASH_4MB),
	CFGSTR(FLASH_2MB),
	CFGSTR(DEFAULT_FLASH_SIZE),
	{ 0 , 0 }
};

static const struct ruby_cfgstr_map g_cfgstr_tx_antenna_num[] = {
	{ "TX_ANTENNA_NUM_1", 1 },
	{ "TX_ANTENNA_NUM_2", 2 },
	{ "TX_ANTENNA_NUM_3", 3 },
	{ "TX_ANTENNA_NUM_4", 4 },
	{ "DEFAULT_TX_ANTENNA_NUM", 4 },
	{ 0 , 0 }
};

#define TX_ANTENNA_GAIN_1_1dB	4506
static const struct ruby_cfgstr_map g_cfgstr_tx_antenna_gain[] = {
	CFGSTR(TX_ANTENNA_GAIN_1_1dB),
	{ "DEFAULT_TX_ANTENNA_GAIN", TX_ANTENNA_GAIN_1_1dB },
	{ 0 , 0 }
};

#define LNA_gain_12dB		12
static const struct ruby_cfgstr_map g_cfgstr_ext_lna_gain[] = {
	CFGSTR(LNA_gain_12dB),
	{ "DEFAULT_EXT_LNA_GAIN", LNA_gain_12dB },
	{ 0 , 0 }
};

#define LNA_gain_BYPASS_N5dB	-5
static const struct ruby_cfgstr_map g_cfgstr_ext_lna_bypass_gain[] = {
	CFGSTR(LNA_gain_BYPASS_N5dB),
	{ "DEFAULT_EXT_LNA_BYPASS_GAIN", LNA_gain_BYPASS_N5dB },
	{ 0 , 0 }
};

#define RFIC_NOT_IN_USE		0
#define RFIC_V4_IN_USE		4
#define RFIC_V6_IN_USE		6
static const struct ruby_cfgstr_map g_cfgstr_rfic[] = {
	CFGSTR(RFIC_NOT_IN_USE),
	CFGSTR(RFIC_V4_IN_USE),
	CFGSTR(RFIC_V6_IN_USE),
	{ "DEFAULT_RFIC", RFIC_V4_IN_USE },
	{ 0 , 0 }
};

#define CALSTATE_VPD_LOG	0
#define CALSTATE_VPD_LINEAR	1
static const struct ruby_cfgstr_map g_cfgstr_txpow_cal[] = {
	CFGSTR(CALSTATE_VPD_LOG),
	CFGSTR(CALSTATE_VPD_LINEAR),
	{ "DEFAULT_CALSTATE_VPD", CALSTATE_VPD_LOG },
	{ 0 , 0 }
};

static const struct ruby_board_param g_custom_board_params[] = {
	{ g_cfgstr_board_id_cfg, BOARD_CFG_ID },
	{ g_cfgstr_name_cfg, BOARD_CFG_NAME },
	{ g_cfgstr_ddr_cfg, BOARD_CFG_DDR_TYPE },
	{ g_cfgstr_ddr_speed, BOARD_CFG_DDR_SPEED },
	{ g_cfgstr_ddr_size, BOARD_CFG_DDR_SIZE },
	{ g_cfgstr_emac_cfg, BOARD_CFG_EMAC0 },
	{ g_cfgstr_emac_cfg, BOARD_CFG_EMAC1 },
	{ g_cfgstr_emac_phyaddr, BOARD_CFG_PHY0_ADDR },
	{ g_cfgstr_emac_phyaddr, BOARD_CFG_PHY1_ADDR },
	{ g_cfgstr_rfpa_cfg, BOARD_CFG_WIFI_HW },
	{ g_cfgstr_spi_cfg, BOARD_CFG_SPI1 },
	{ g_cfgstr_uart_cfg, BOARD_CFG_UART1 },
	{ g_cfgstr_rgmii_cfg, BOARD_CFG_RGMII_TIMING },
	{ g_cfgstr_pcie_cfg, BOARD_CFG_PCIE },
	{ g_cfgstr_flash_cfg, BOARD_CFG_FLASH_SIZE },
	{ g_cfgstr_tx_antenna_num, BOARD_CFG_TX_ANTENNA_NUM },
	{ g_cfgstr_tx_antenna_gain, BOARD_CFG_TX_ANTENNA_GAIN },
	{ g_cfgstr_ext_lna_gain, BOARD_CFG_EXT_LNA_GAIN },
	{ g_cfgstr_ext_lna_bypass_gain, BOARD_CFG_EXT_LNA_BYPASS_GAIN },
	{ g_cfgstr_rfic, BOARD_CFG_RFIC },
	{ g_cfgstr_txpow_cal, BOARD_CFG_CALSTATE_VPD },
	{0, 0 }
};

static board_cfg_t g_custom_board_cfg = {
	.bc_board_id	= QTN_RUBY_AUTOCONFIG_ID,
	.bc_name	= "Autoconfigured board",
	.bc_ddr_type	= DEFAULT_DDR_CFG,
	.bc_ddr_speed	= DEFAULT_DDR_SPEED,
	.bc_ddr_size	= DDR_AUTO,
	.bc_emac0	= EMAC_NOT_IN_USE,
	.bc_emac1	= EMAC_NOT_IN_USE,
	.bc_spi1	= SPI1_NOT_IN_USE,
	.bc_wifi_hw	= QTN_RUBY_WIFI_NONE,
	.bc_uart1	= UART1_NOT_IN_USE,
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,
};

#endif


#define QTN_BOARD_DB					{		\
	{ /* 0 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD,			\
	.bc_name	= "micron32-160, emac0-24, pa0",		\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_32_320,		\
	.bc_name	= "micron32-250, emac0-24, pa0",		\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 2 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_16_320,		\
	.bc_name	= "micron16-250, emac0-24, pa0",		\
	.bc_ddr_type	= DDR_16_MICRON,				\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 3 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_16_160,		\
	.bc_name	= "micron16-160, emac0-24, pa0",		\
	.bc_ddr_type	= DDR_16_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 4 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_ETRON,			\
	.bc_name	= "etron16-250, emac0-24, pa0",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 5 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_ETRON_320,		\
	.bc_name	= "etron16-320, emac0-24, pa0",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 6 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_ETRON_160,		\
	.bc_name	= "etron16-160, emac0-24, pa0",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 7 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_16_200,		\
	.bc_name	= "micron16-200, emac0-24, pa0",		\
	.bc_ddr_type	= DDR_16_MICRON,				\
	.bc_ddr_speed	= DDR_200,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 8 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_32_200,		\
	.bc_name	= "micron32-200, emac0-24, pa0",		\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_200,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 9 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_PCIE,			\
	.bc_name	= "etron16-160, pcie, pa2, phy loopbk",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_USE_PHY_LOOPBK,		\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{								\
	/* test arbitration settings */					\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_32_160_ARB,		\
	.bc_name	= "micron32-160, emac0-24, pa0, arb",		\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{								\
	/* test emac1 */						\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_32_160_ARB_1,		\
	.bc_name	= "micron32-160, emac1-31, pa0, arb",		\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_phy1_addr	= 31,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{	/* 12 - arb, 16bit emac1 */				\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_16_160_ARB_1,		\
	.bc_name	= "micron16-160, emac1-31, pa0, arb",		\
	.bc_ddr_type	= DDR_16_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= 31,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{	/* 13 - arb, 16bit emac1 */				\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_32_160_ARB_0,		\
	.bc_name	= "micron16-160, emac0-24, pa0, arb",		\
	.bc_ddr_type	= DDR_16_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{	/* 14 */						\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_ETRON_160_EMAC1,	\
	.bc_name	= "etron16-160, emac0-24, pa0",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 15 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_ETRON_250_EMAC1,	\
	.bc_name	= "etron16-250, emac1-31, pa0",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_phy1_addr	= 31,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 16 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_ETRON_32_320_EMAC1,	\
	.bc_name	= "etron32-320, emac1-31, pa0",			\
	.bc_ddr_type	= DDR_32_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_phy1_addr	= 31,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 17 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_ETRON32_160,			\
	.bc_name	= "etron32-160, emac0-24, pa0",			\
	.bc_ddr_type	= DDR_32_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_USE_PHY_LOOPBK,		\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 18 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_ETRON32_320,			\
	.bc_name	= "etron32-250, emac0-24, pa0",			\
	.bc_ddr_type	= DDR_32_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_USE_PHY_LOOPBK,		\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 19 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_MICRON_DUALEMAC,	\
	.bc_name	= "micron32-160, emac0, emac1, pa0",		\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 20 */							\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_MICRON_DUALEMAC_MII,	\
	.bc_name	= "micron32-160, emac0-mii-100, emac1-mii-100, pa0",	\
	.bc_ddr_type	= DDR_32_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB,	\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB,	\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,			\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{								\
	/* 21 Bringup board with dual EMAC loopback */			\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_MICRON_DUALEMAC_LOOPBACK,		\
	.bc_name	= "micron16-160, pcie, emac0, emac1, pa0, phy loopback",	\
	.bc_ddr_type	= DDR_16_MICRON,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,				\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_phy1_addr	= 31,						\
	.bc_phy0_addr	= 24,						\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_BRINGUP_RWPA,		\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_USE_PHY_LOOPBK,\
	 },{	/* 22 */												\
	.bc_board_id	= QTN_RUBY_BRINGUP_BOARD_16_160_DUALEMAC,	\
	.bc_name        = "etron16-160, emac1,emac0, pa0",			\
	.bc_ddr_type    = DDR_16_ETRON,								\
	.bc_ddr_speed   = DDR_160,									\
	.bc_ddr_size    = DDR_64MB,									\
	.bc_emac0       = EMAC_IN_USE,								\
	.bc_emac1       = EMAC_IN_USE,								\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,						\
	.bc_phy1_addr   = EMAC_PHY_ADDR_SCAN,						\
	.bc_spi1        = SPI1_IN_USE,								\
	.bc_wifi_hw     = QTN_RUBY_BRINGUP_RWPA,					\
	.bc_uart1       = UART1_IN_USE,								\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,				\
	 },{ /* 1000 */							\
	.bc_board_id	= QTN_RUBY_REFERENCE_DESIGN_BOARD,		\
	.bc_name	= "etron16-160, emac1, pa1",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1001 */							\
	.bc_board_id	= QTN_RUBY_REFERENCE_DESIGN_BOARD_250,		\
	.bc_name	= "etron16-250, emac1, pa1",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1002 */							\
	.bc_board_id	= QTN_RUBY_REF_BOARD_DUAL_CON,			\
	.bc_name	= "etron32-160, emac1, pa1",			\
	.bc_ddr_type	= DDR_32_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_pcie	= PCIE_IN_USE,					\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1003 */							\
	.bc_board_id	= QTN_RUBY_REFERENCE_DESIGN_BOARD_320,		\
	.bc_name	= "etron16-320, emac1, pa1",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1004 */							\
	.bc_board_id	= QTN_RUBY_ETRON_32_320_EMAC1,			\
	.bc_name	= "etron32-320, emac1, pa1",			\
	.bc_ddr_type	= DDR_32_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1005 */							\
	.bc_board_id	= QTN_RUBY_ETRON_32_250_EMAC1,			\
	.bc_name	= "etron32-250, emac1, pa1",			\
	.bc_ddr_type	= DDR_32_ETRON,					\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1006 */							\
	.bc_board_id	= QTN_RUBY_REFERENCE_DESIGN_BOARD_RGMII_DLL,	\
	.bc_name	= "etron16-160, emac1-rgmii-dll, pa2",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DLL_TIMING,		\
	 },{ /* 1007 */							\
	.bc_board_id	= QTN_RUBY_QHS710_5S5_SIGE_DDR250,		\
	.bc_name	= "etron16-250, emac1, pa2",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_250,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1008 */							\
	.bc_board_id	= QTN_RUBY_QHS710_5S5_SIGE_DDR320,		\
	.bc_name	= "etron16-320, emac1, pa2",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1009 */							\
	.bc_board_id	= QTN_RUBY_OHS711_PCIE_320DDR,			\
	.bc_name	= "etron16-320, pcie, pa2",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1170 */							\
	.bc_board_id	= QTN_RUBY_QHS713_5S1_PCIERC_DDR160,		\
	.bc_name	= "etron16-160, emac1, pcie-rc, pa1",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_RC_MODE,			\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1171 */							\
	.bc_board_id	= QTN_RUBY_OHS711_5S13_PCIE_DDR320,		\
	.bc_name	= "etron16-320, pcie-ep, pa2",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1172 */							\
	.bc_board_id	= QTN_RUBY_QHS713_5S1_PCIERC_DDR320,		\
	.bc_name	= "etron16-320, emac1, pcie-rc, pa1",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_320,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_RC_MODE,			\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1200 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_0,				\
	.bc_name	= "etron16-160, emac1-mii, pa1",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_MII,			\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1201 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_1,				\
	.bc_name	= "etron16-160, emac0-mii, pa1",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_MII,			\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1202 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_2,				\
	.bc_name	= "etron16-160, emac1 88e6071-mii, pa1",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_MV88E6071,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1203 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_3,				\
	.bc_name	= "etron16-160, emac1 ar8236-mii, pa1",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_AR8236,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1204 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_4,				\
	.bc_name	= "etron16-160, pcie, pa2",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1205 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_5,				\
	.bc_name	= "etron16-160, emac1, mii-100, pa1",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_NOT_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1206 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_6,				\
	.bc_name	= "etron16-160, emac1, mii-100, pa2",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_NOT_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1207 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_7,				\
	.bc_name	= "etron16-160, emac1, pa2",			\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE,					\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1208 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_8,				\
	.bc_name	= "etron16-160, emac1 ar8327-rgmii, pa1",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_AR8327,     \
	.bc_phy1_addr   = EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_REF_RWPA,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1209*/							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_9,				\
	.bc_name	= "etron16-160, emac1 rtl8363s-rgmii , pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE |  EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1210*/							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_10,			\
	.bc_name	= "etron16-160, emac1 back-to-back-rgmii, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE |  EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1211 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_11,			\
	.bc_name	= "etron16-160, emac0-mii, pa2",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_MII,			\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1212 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_12,			\
	.bc_name	= "etron16-160, emac1 88e6071-mii, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_MV88E6071,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1213 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_13,			\
	.bc_name	= "etron16-160, emac0, emac1 b2b-rgmii 100M, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1214 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_14,			\
	.bc_name	= "etron16-160, emac0-mii-gpio1rst, emac1-mii-gpio13rst-100M, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_GPIO1_RESET,		\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_GPIO13_RESET | EMAC_PHY_FORCE_100MB,	\
	.bc_phy0_addr	= 1,				\
	.bc_phy1_addr	= 2,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1215 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_15,			\
	.bc_name	= "etron16-160, emac0, emac1 b2b-rgmii 1000M, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_710F,			\
	 },{ /* 1216 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_16,			\
	.bc_name	= "etron16-160, emac1 b2b-rgmii 100M, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1217 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_17,			\
	.bc_name	= "etron16-160, emac1 b2b-rgmii 1000M, pa2",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_710F,			\
	 },{ /* 1218 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_18,			\
	.bc_name	= "etron16-160, emac1-mii, pa2",		\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_NOT_IN_USE,				\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1219 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_19,			\
	.bc_name	= "samsung16-160, emac1, mii-100, pa2",		\
	.bc_ddr_type	= DDR_16_SAMSUNG,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_NOT_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB,	\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	 },{ /* 1220 */							\
	.bc_board_id	= QTN_RUBY_ODM_BOARD_20,			\
	.bc_name	= "etron16-160, emac0, emac1 b2b-rgmii 1000M, no wifi",	\
	.bc_ddr_type	= DDR_16_ETRON,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE,	\
	.bc_phy0_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_phy1_addr	= EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw	= QTN_RUBY_WIFI_NONE,				\
	.bc_uart1	= UART1_NOT_IN_USE,				\
	.bc_pcie	= PCIE_IN_USE | PCIE_RC_MODE,			\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1221 */							\
	.bc_board_id = QTN_RUBY_ODM_BOARD_21,				\
	.bc_name = "etron16-160, emac1 ar8236-mii, pa2",		\
	.bc_ddr_type = DDR_16_ETRON,					\
	.bc_ddr_speed = DDR_160,					\
	.bc_ddr_size = DDR_64MB,					\
	.bc_emac1 = EMAC_IN_USE | EMAC_PHY_MII | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_AR8236,                                          \
	.bc_phy1_addr = EMAC_PHY_ADDR_SCAN,				\
	.bc_wifi_hw = QTN_RUBY_SIGE,                                    \
	.bc_uart1 = UART1_NOT_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1222 */							\
	.bc_board_id    = QTN_RUBY_ODM_BOARD_22,                        \
	.bc_name        = "etron16-160, emac1 ar8327-rgmii, pa2",       \
	.bc_ddr_type    = DDR_16_ETRON,                                 \
	.bc_ddr_speed   = DDR_160,                                      \
	.bc_ddr_size    = DDR_64MB,                                     \
	.bc_emac1       = EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_AR8327, \
	.bc_phy1_addr   = EMAC_PHY_ADDR_SCAN,                           \
	.bc_wifi_hw     = QTN_RUBY_SIGE,                                \
	.bc_uart1       = UART1_NOT_IN_USE,                             \
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,                   \
	 },{ /* 1223 */							\
	.bc_board_id	= QTN_TOPAZ_FPGAA_BOARD,		\
	.bc_name	= "FPGA-A(hw_config_id:1223) DDR3, EMAC1, WMAC, RGMII-1G", \
	.bc_ddr_type	= DDR3_16_WINBOND,					\
	.bc_ddr_speed	= DDR3_320MHz,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE, \
	.bc_emac1	= EMAC_NOT_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE, \
	.bc_phy0_addr	= TOPAZ_FPGAA_PHY0_ADDR,			\
	.bc_phy1_addr	= TOPAZ_FPGAA_PHY1_ADDR,			\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1224 */							\
	.bc_board_id	= QTN_TOPAZ_FPGAB_BOARD,		\
	.bc_name	= "FPGA-B(hw_config_id:1224) DDR3, EMAC0, WMAC, RGMII-1G", \
	.bc_ddr_type	= DDR3_16_WINBOND,					\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB, \
	.bc_emac1	= EMAC_NOT_IN_USE | EMAC_PHY_FORCE_100MB,	\
	.bc_phy0_addr	= TOPAZ_FPGAB_PHY0_ADDR,			\
	.bc_phy1_addr	= TOPAZ_FPGAB_PHY1_ADDR,			\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1225 */							\
	.bc_board_id	= QTN_TOPAZ_DUAL_EMAC_FPGAA_BOARD,		\
	.bc_name	= "FPGA-A(hw_config_id:1225) DDR3, EMAC0, EMAC1, WMAC, RGMII-1G", \
	.bc_ddr_type	= DDR3_16_WINBOND,				\
	.bc_ddr_speed	= DDR3_320MHz,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_FPGAA_ONLY, \
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_FPGAA_ONLY, \
	.bc_phy0_addr	= TOPAZ_FPGAA_PHY0_ADDR,			\
	.bc_phy1_addr	= TOPAZ_FPGAA_PHY1_ADDR,			\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1226 */							\
	.bc_board_id	= QTN_TOPAZ_DUAL_EMAC_FPGAB_BOARD,		\
	.bc_name	= "FPGA-B(hw_config_id:1226) DDR3, EMAC0, EMAC1, WMAC, RGMII-1G", \
	.bc_ddr_type	= DDR3_16_WINBOND,				\
	.bc_ddr_speed	= DDR3_320MHz,					\
	.bc_ddr_size	= DDR_64MB,					\
	.bc_emac0	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_FPGAB_ONLY, \
	.bc_emac1	= EMAC_IN_USE | EMAC_PHY_FORCE_100MB | EMAC_PHY_FPGAB_ONLY, \
	.bc_phy0_addr	= TOPAZ_FPGAB_PHY0_ADDR,			\
	.bc_phy1_addr	= TOPAZ_FPGAB_PHY1_ADDR,			\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1227 */							\
	.bc_board_id	= QTN_TOPAZ_RC_BOARD,				\
	.bc_name	= "FPGA-A(hw_config_id:1227) DDR3, EMAC1, WMAC, RGMII-1G", \
	.bc_ddr_type	= DDR3_16_WINBOND,				\
	.bc_ddr_speed	= DDR3_320MHz,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= TOPAZ_PHY0_ADDR,				\
	.bc_phy1_addr	= TOPAZ_PHY1_ADDR,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_WIFI_NONE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_pcie        = PCIE_IN_USE | PCIE_RC_MODE,			\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	 },{ /* 1228 */							\
	.bc_board_id	= QTN_TOPAZ_EP_BOARD,				\
	.bc_name	= "FPGA-B(hw_config_id:1228) DDR3, EMAC0, WMAC, RGMII-1G", \
	.bc_ddr_type	= DDR3_16_WINBOND,				\
	.bc_ddr_speed	= DDR_160,					\
	.bc_ddr_size	= DDR_128MB,					\
	.bc_emac0	= EMAC_IN_USE,					\
	.bc_emac1	= EMAC_NOT_IN_USE,				\
	.bc_phy0_addr	= TOPAZ_PHY0_ADDR,				\
	.bc_phy1_addr	= TOPAZ_PHY1_ADDR,				\
	.bc_spi1	= SPI1_IN_USE,					\
	.bc_wifi_hw	= QTN_RUBY_SIGE,				\
	.bc_uart1	= UART1_IN_USE,					\
	.bc_pcie	= PCIE_IN_USE,					\
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_DEFAULT,			\
	} ,{ /* 1229 */							\
	.bc_board_id    = QTN_TOPAZ_BB_BOARD, \
	.bc_name        = "BB-EVK(hw_config_id:1229) DDR3, EMAC0, WMAC, RGMII-1G", \
	.bc_ddr_type    = DDR3_16_WINBOND, \
	.bc_ddr_speed   = DDR3_400MHz, \
	.bc_ddr_size    = DDR_128MB, \
	.bc_emac0       = EMAC_IN_USE, \
	.bc_emac1       = EMAC_IN_USE, \
	.bc_phy0_addr   = TOPAZ_PHY0_ADDR, \
	.bc_phy1_addr   = TOPAZ_PHY1_ADDR, \
	.bc_spi1        = SPI1_IN_USE, \
	.bc_wifi_hw     = QTN_RUBY_WIFI_NONE, \
	.bc_uart1       = UART1_IN_USE, \
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_NODELAY, \
	} ,{ /* 1230 */ \
	.bc_board_id    = QTN_TOPAZ_RF_BOARD, \
	.bc_name        = "RF-EVK(hw_config_id:1230) DDR3, EMAC0, WMAC, RGMII-1G", \
	.bc_ddr_type    = DDR3_16_WINBOND, \
	.bc_ddr_speed   = DDR3_500MHz, \
	.bc_ddr_size    = DDR_128MB, \
	.bc_emac0       = EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE, \
	.bc_emac1       = EMAC_IN_USE, \
	.bc_phy0_addr   = TOPAZ_PHY0_ADDR, \
	.bc_phy1_addr   = TOPAZ_PHY1_ADDR, \
	.bc_spi1        = SPI1_IN_USE, \
	.bc_wifi_hw     = QTN_TPZ_SE5003L1_INV, \
	.bc_uart1       = UART1_IN_USE, \
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_NODELAY, \
	} ,{ /* 1231 */ \
	.bc_board_id    = QTN_TOPAZ_QHS840_5S1, \
	.bc_name        = "QHS840_5S1 RDK", \
	.bc_ddr_type    = DDR3_16_WINBOND, \
	.bc_ddr_speed   = DDR3_500MHz, \
	.bc_ddr_size    = DDR_128MB, \
	.bc_emac0       = EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE | EMAC_PHY_RTL8363SB_P0, \
	.bc_wifi_hw     = QTN_TPZ_SE5003L1, \
	.bc_rgmii_timing = CONFIG_ARCH_RGMII_NODELAY, \
	} \
}

#endif /* _RUBY_BOARD_DB_ */

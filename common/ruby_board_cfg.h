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

#ifndef __RUBY_BOARD_CFG_H
#define __RUBY_BOARD_CFG_H

#include "ruby_platform.h"

#define	SPI1_NOT_IN_USE			(0)
#define	SPI1_IN_USE			(BIT(0))

/*
 * There is a copy named qdpc_pcie_board_cfg_t in qdpc_config.h they must be the same.
 * The copy is used for Host driver Because the Host driver can't cite to here.
 */
typedef struct board_cfg {
	int bc_board_id;
	char *bc_name;		/* optional name of cfg */
	int bc_ddr_type;	/* ID */
	int bc_ddr_speed;	/* speed in MHz */
	int bc_ddr_size;	/* in bytes */
	int bc_emac0;		/* in use? */
	int bc_emac1;		/* in use? */
	int bc_phy0_addr;	/* address */
	int bc_phy1_addr;	/* address */
	int bc_spi1;		/* in use? */
	int bc_wifi_hw;		/* WiFi hardware type */
	int bc_uart1;		/* in use? */
	int bc_pcie;		/* in use? */
	int bc_rgmii_timing;	/* special timing value for RGMII */
} board_cfg_t;

#define BOARD_CFG_STRUCT_NUM_FIELDS	(sizeof(struct board_cfg) / sizeof(int))

/* These are index into cfg array */
#define BOARD_CFG_ID			(0)
#define BOARD_CFG_NAME			(1)
#define BOARD_CFG_DDR_TYPE		(2)
#define BOARD_CFG_DDR_SPEED		(3)
#define BOARD_CFG_DDR_SIZE		(4)
#define BOARD_CFG_EMAC0			(5)
#define BOARD_CFG_EMAC1			(6)
#define BOARD_CFG_PHY0_ADDR		(7)
#define BOARD_CFG_PHY1_ADDR		(8)
#define BOARD_CFG_SPI1			(9)
#define BOARD_CFG_WIFI_HW		(10)
#define BOARD_CFG_UART1			(11)
#define BOARD_CFG_PCIE			(12)
#define BOARD_CFG_RGMII_TIMING		(13)
#define BOARD_CFG_EXT_LNA_GAIN		(14)
#define BOARD_CFG_TX_ANTENNA_NUM	(15)
#define BOARD_CFG_FLASH_SIZE		(16)
#define BOARD_CFG_TX_ANTENNA_GAIN	(17)
#define BOARD_CFG_EXT_LNA_BYPASS_GAIN	(18)
#define BOARD_CFG_RFIC			(19)
#define BOARD_CFG_CALSTATE_VPD		(20)

#define BOARD_CFG_FIELD_NAMES	{	\
	"bc_board_id",			\
	"bc_name",			\
	"bc_ddr_type",			\
	"bc_ddr_speed",			\
	"bc_ddr_size",			\
	"bc_emac0",			\
	"bc_emac1",			\
	"bc_phy0_addr",			\
	"bc_phy1_addr",			\
	"bc_spi1",			\
	"bc_wifi_hw",			\
	"bc_uart1",			\
	"bc_pcie",			\
	"bc_rgmii_timing",		\
	"bc_ext_lna_gain",		\
	"bc_tx_antenna_num",		\
	"bc_flash_cfg",			\
	"bc_tx_antenna_gain",		\
	"bc_ext_lna_bypass_gain",	\
	"bc_rfic",			\
	"bc_tx_power_cal",		\
}

#define RUBY_BDA_VERSION		0x1000
#define RUBY_BDA_NAMELEN		32

/*
 * quantenna board configuration information,
 * shared between u-boot and linux kernel.
 */
typedef struct qtn_board_cfg_info {
	uint16_t	bda_len;			/* Size of BDA block */
	uint16_t	bda_version;			/* BDA version */
	uint8_t		rsvd[36];
	board_cfg_t	bda_boardcfg;
	uint32_t	bda_flashsz;
	char		bda_boardname[RUBY_BDA_NAMELEN];
} __attribute__ ((packed)) ruby_bda_t;

#endif /* __RUBY_BOARD_CFG_H */

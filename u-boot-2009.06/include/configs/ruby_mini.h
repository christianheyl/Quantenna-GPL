/*
 * (C) Copyright 2011 Quantenna Communications Inc.
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


#ifndef __RUBY_UBOOT_CONFIG_H
#define __RUBY_UBOOT_CONFIG_H

#include "common_mem.h"
#include "ruby_partitions.h"

#define TEXT_BASE		(RUBY_SRAM_BEGIN + TEXT_BASE_OFFSET)

/* generic define for RUBY */
#define CONFIG_RUBY_BOARD

#define CONFIG_LZMA

#ifndef  TOPAZ_EP_MINI_UBOOT
/* Commands */
#define CONFIG_CMD_NET
#define CONFIG_CMD_PING
#define CONFIG_CMD_INTR
#endif

//#define CONFIG_BOOTP_RANDOM_DELAY

/* Suppress inclusion of unwanted functions by pretending we have them */
#define __HAVE_ARCH_STRDUP

/* No flash support */
#define CONFIG_SYS_NO_FLASH

/* Stack grows backward; put stack before the MuC stack */
#define CONFIG_ARC_STACK_BEGIN	(RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_STACK_OFFSET_UBOOT - \
		RUBY_STACK_INIT_OFFSET - CONFIG_ARC_MUC_STACK_SIZE)

/* Misc parameters */
#define CONFIG_SYS_HZ			1000
#define CONFIG_SYS_BAUDRATE_TABLE	{ RUBY_SERIAL_BAUD }
#define CONFIG_BAUD_RATE		RUBY_SERIAL_BAUD

/* Console */
#define CONFIG_SYS_MAXARGS		16 /* Max number of command args */

/* Environment */
#ifdef TOPAZ_EP_MINI_UBOOT
#define	CONFIG_ENV_IS_IN_SPI_FLASH	1
#define CONFIG_ENV_SIZE			BOOT_CFG_SIZE
#define CONFIG_ENV_BASE_SIZE		BOOT_CFG_BASE_SIZE
#define	CONFIG_ENV_OVERWRITE
#else
#define	CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE			512
#endif

#define __MK_STR(_x)	#_x
#define MKSTR(_x)	__MK_STR(_x)

/* Ethernet */
#ifdef CONFIG_CMD_NET
	#define CONFIG_NET_MULTI		1
	#define CONFIG_ARASAN_GBE		1
	#define CONFIG_ETHADDR			00:26:86:00:00:00	/* Quantenna OUI */
	#define CONFIG_IPADDR			1.1.1.2
	#define CONFIG_SERVERIP			1.1.1.1
	#define CONFIG_BOOTFILE			"u-boot.bin"
	#define CONFIG_NET_RETRY_COUNT		2
	#define CONFIG_BOOTP_QTN_VENDORINFO

	#define RUBY_BOOT_METHOD	RUBY_BOOT_METHOD_TRYLOOP
#endif

#ifndef __ASSEMBLY__

#include <linux/types.h>

#endif	// __ASSEMBLY__

/* board config */

#define EMAC0_CONFIG (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE)
#ifdef TOPAZ_VZN_MINI_UBOOT
#define EMAC1_CONFIG (EMAC_IN_USE | EMAC_PHY_FORCE_1000MB | EMAC_PHY_NOT_IN_USE)
#else
#define EMAC1_CONFIG (EMAC_NOT_IN_USE)
#endif
#define EMAC0_PHY_ADDR (EMAC_PHY_ADDR_SCAN)
#define EMAC1_PHY_ADDR (EMAC_PHY_ADDR_SCAN)
#define EMAC_RGMII_TIMING (0)
/* sample config for enable both emac */
/*
#define EMAC0_CONFIG (EMAC_IN_USE)
#define EMAC1_CONFIG (EMAC_IN_USE)
#define EMAC0_PHY_ADDR (1)
#define EMAC1_PHY_ADDR (3)
#define EMAC_RGMII_TIMING 0x81828282
*/
/* enable for emac switch feature */
//#define EMAC_SWITCH

/* enable for emac bonding */
//#define EMAC_BOND_MASTER (0) /* 0 or 1 */


#endif // #ifndef __RUBY_UBOOT_CONFIG_H


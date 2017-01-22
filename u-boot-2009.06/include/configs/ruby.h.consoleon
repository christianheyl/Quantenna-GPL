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


#ifndef __RUBY_UBOOT_CONFIG_H
#define __RUBY_UBOOT_CONFIG_H

#include "common_mem.h"
#include "ruby_partitions.h"

/* generic define for RUBY */
#define CONFIG_RUBY_BOARD

/* Commands */
#define CONFIG_CMD_CONSOLE
#define CONFIG_CMD_ECHO
#define CONFIG_CMD_MEMORY
#define CONFIG_CMD_MISC
#define CONFIG_CMD_RUN
#define CONFIG_CMD_LOADB
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_NET
#define CONFIG_CMD_PING
#undef	CONFIG_CMD_ETHLOOP
#undef	CONFIG_CMD_UC
#define CONFIG_CMD_INTR
#define CONFIG_CMD_SAVEENV
#define CONFIG_CMD_IMI
#define CONFIG_CMD_BOOTD
#undef	CONFIG_CMD_POST
#define CONFIG_CMD_QMEMTEST

/* decompression algorithms compressed images */
//#define CONFIG_BZIP2	// doesn't work with -Os
#define CONFIG_LZMA

/* We have these optimized functions */
#define __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCMP

/* No flash support */
#define CONFIG_SYS_NO_FLASH

/* Enable verbose boot */
#define CONFIG_SHOW_BOOT_PROGRESS

/* Delay before boot. */
#define CONFIG_BOOTDELAY		2

/*
 * U-Boot full is now encapsulated within a decompressing piggy.
 * u-boot-piggy loads at sram + 0x0, jumped to from the bootrom.
 * In the bootloader stage1 (piggy), the start of sram is text, data, bss,
 * with the piggy stack at the end of sram.
 * u-boot-piggy will decompress u-boot-full into the stage2 offset, since
 * the piggy cannot overwrite it's own text or data during decompression.
 * Once decompressed, piggy will jump to u-boot full. U-boot full will use
 * the sram previously occupied by the piggy text/data/bss as stack + heap.
 *
 * Piggy: SRAM: [text, data, bss, <--gap-->, stack, muc_stack, crumbs, <--gap-->]
 * Decompresses u-boot-full into second gap right after muc_stack and crumbs, jumps to gap.
 * Full:  SRAM: [heap, stack, <--unused-->, muc_stack, crumbs, text, data, bss]
 */
#define CONFIG_ARC_STAGE2_OFFSET	(4 * RUBY_SRAM_BANK_SIZE)

#define CONFIG_ARC_UBOOT_BEGIN		RUBY_SRAM_BEGIN
#define CONFIG_ARC_TEXT_OFFSET		CONFIG_ARC_STAGE2_OFFSET
#define CONFIG_ARC_TEXT_BEGIN		(CONFIG_ARC_UBOOT_BEGIN + CONFIG_ARC_TEXT_OFFSET)
#define CONFIG_ARC_TEXT_SIZE		(RUBY_SRAM_SIZE - CONFIG_ARC_TEXT_OFFSET)
#define CONFIG_ARC_TEXT_END		(CONFIG_ARC_TEXT_BEGIN + CONFIG_ARC_TEXT_SIZE)
#define CONFIG_ARC_STACK_SIZE           (8*1024)

#ifdef FLASH_SUPPORT_256KB
	#define CONFIG_ARC_STACK_BEGIN          \
		(CONFIG_ARC_UBOOT_BEGIN + (CONFIG_ARC_MUC_STACK_OFFSET_UBOOT - CONFIG_ARC_MUC_STACK_SIZE) - RUBY_STACK_INIT_OFFSET)
	#define CONFIG_ARC_HEAP_SIZE	((CONFIG_ARC_MUC_STACK_OFFSET_UBOOT - CONFIG_ARC_MUC_STACK_SIZE) - CONFIG_ARC_STACK_SIZE)
#else
	#define CONFIG_ARC_STACK_BEGIN		\
		(CONFIG_ARC_UBOOT_BEGIN + RUBY_UBOOT_PIGGY_MAX_SIZE - RUBY_STACK_INIT_OFFSET)
	#define CONFIG_ARC_HEAP_SIZE	(RUBY_UBOOT_PIGGY_MAX_SIZE - CONFIG_ARC_STACK_SIZE)
#endif

#define CONFIG_ARC_HEAP_BEGIN		CONFIG_ARC_UBOOT_BEGIN
#if CONFIG_ARC_HEAP_SIZE + CONFIG_ARC_STACK_SIZE > RUBY_SRAM_SIZE - CONFIG_ARC_TEXT_SIZE
	#error "Too big heap and stack!"
#endif
#define CONFIG_ARC_FREE_BEGIN		RUBY_DRAM_BEGIN
#define CONFIG_ARC_FREE_END		(RUBY_DRAM_BEGIN + RUBY_MIN_DRAM_SIZE)
#define CONFIG_SYS_MALLOC_LEN		CONFIG_ARC_HEAP_SIZE

#define TEXT_BASE			CONFIG_ARC_TEXT_BEGIN

/* Default load address */
#define CONFIG_SYS_LOAD_ADDR		RUBY_KERNEL_LOAD_DRAM_BEGIN	/* Default load address for bootm command */

/* Memory test */
#define CONFIG_SYS_MEMTEST_START	CONFIG_ARC_FREE_BEGIN
#define CONFIG_SYS_MEMTEST_END		CONFIG_ARC_FREE_END

/* Misc parameters */
#define CONFIG_SYS_HZ			1000
#define CONFIG_SYS_BAUDRATE_TABLE	{ RUBY_SERIAL_BAUD }
#define CONFIG_BAUD_RATE		RUBY_SERIAL_BAUD

/* Console */
#define CONFIG_SYS_MAXARGS		16 /* Max number of command args */
#define CONFIG_SYS_PROMPT		"topaz>"
#define CONFIG_SYS_CBSIZE		256 /* Size of console buffer */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16) /* Print Buffer Size */

/* Environment */
#define	CONFIG_ENV_IS_IN_SPI_FLASH	1
#define CONFIG_ENV_SIZE			BOOT_CFG_SIZE
#define CONFIG_ENV_BASE_SIZE		BOOT_CFG_BASE_SIZE
#define	CONFIG_ENV_OVERWRITE

/* pass bootargs through to linux kernel using a tag */
#define CONFIG_CMDLINE_TAG
#define CONFIG_ATAGS_MAX_SIZE		512

/* Carrier ID */
#define CONFIG_CARRIER_ID		0

#ifndef MK_STR
	#define RUBY_MK_STR
	#define XMK_STR(x)      #x
	#define MK_STR(x)       XMK_STR(x)
#endif

#define SAFETY_IMG_ADDR_ARG		"safety_image_addr"
#define SAFETY_IMG_SIZE_ARG		"safety_image_size"
#define LIVE_IMG_ADDR_ARG		"live_image_addr"
#define LIVE_IMG_SIZE_ARG		"live_image_size"
#define QTNBOOT_COPY_DRAM_ADDR		RUBY_KERNEL_LOAD_DRAM_BEGIN

#define CONFIG_BOOTCMD_BOOTARGS		"setenv bootargs ${bootargs} hw_config_id=${hw_config_id}"
#define CONFIG_BOOTCMD_NET		"bootcmd_net=" CONFIG_BOOTCMD_BOOTARGS " ; tftp ; bootm"

#define CONFIG_EXTRA_ENV_SETTINGS	""

#ifdef RUBY_MK_STR
	#undef MK_STR
	#undef XMK_STR
#endif

/* Ethernet */
#ifdef CONFIG_CMD_NET
	#define CONFIG_NET_MULTI		1
	#define CONFIG_ARASAN_GBE		1
	#define CONFIG_IPADDR			192.168.1.100
	#define CONFIG_SERVERIP			192.168.1.150
	#define CONFIG_ETHADDR			30:46:9a:25:be:e4
	#define CONFIG_BOOTFILE			"topaz-linux.lzma.img"
	#ifndef PLATFORM_NOSPI
		#define CONFIG_BOOTARGS			"console=ttyS0,115200n8 earlyprintk=1"
	#endif
	#define CONFIG_BOOTCMD			CONFIG_BOOTCMD_NET
	#define CONFIG_NET_RETRY_COUNT		2
#endif

#ifdef PLATFORM_NOSPI

	#ifdef PLATFORM_NOSPI
		#define CONFIG_NOSPI	PLATFORM_NOSPI
	#else
		#define CONFIG_NOSPI ""
	#endif /* #ifdef PLATFORM_NOSPI */

	#ifdef PLATFORM_WMAC_MODE
		#define CONFIG_WMAC_MODE	PLATFORM_WMAC_MODE
	#else
		#define CONFIG_WMAC_MODE ""
	#endif /* #ifdef PLATFORM_WMAC_MODE */

	#define CONFIG_BOOTARGS \
		"console=ttyS0," \
		MK_STR(TOPAZ_SERIAL_BAUD) \
		"n8 earlyprintk=1 hw_config_id=" \
		MK_STR(DEFAULT_BOARD_ID) \
		" spi=" MK_STR(CONFIG_NOSPI) \
		" wmac_mode=" MK_STR(CONFIG_WMAC_MODE)

	#define CONFIG_HW_CONFIG_ID		DEFAULT_BOARD_ID

#endif /* #ifdef PLATFORM_NOSPI */

/* enable for emac switch feature */
//#define EMAC_SWITCH

/* enable for emac bonding */
//#define EMAC_BOND_MASTER (0) /* 0 or 1 */


#endif // #ifndef __RUBY_UBOOT_CONFIG_H

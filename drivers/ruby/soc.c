/*
 *  Copyright (c) Quantenna Communications Incorporated 2010.
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/if_ether.h>

#include <asm/board/soc.h>
#include <asm/hardware.h>

#include "qtn/shared_params.h"

#include "qdrv_sch_const.h"
#include "qtn/qdrv_sch.h"
static char board_id[16] = "unknown";
static unsigned char ethernet_addr[ETH_ALEN] = {0x00, 0x08, 0x55, 0x41, 0x00, 0x00};

int global_disable_wd = 0;

__sram_data uint8_t qdrv_sch_tos2ac[] = {
		QDRV_BAND_AC_BE,
		QDRV_BAND_AC_BK,
		QDRV_BAND_AC_BK,
		QDRV_BAND_AC_BE,
		QDRV_BAND_AC_VI,
		QDRV_BAND_AC_VI,
		QDRV_BAND_AC_VO,
		QDRV_BAND_AC_VO
};
EXPORT_SYMBOL(qdrv_sch_tos2ac);

__sram_data uint8_t qdrv_sch_dscp2dot1p[] = {
	/*000xxx*/
	0, 0, 0, 0, 0, 0, 0, 0,
	/*001xxx*/
	1, 1, 1, 1, 1, 1, 1, 1,
	/*010xxx*/
	2, 2, 2, 2, 2, 2, 2, 2,
	/*011xxx*/
	3, 3, 3, 3, 3, 3, 3, 3,
	/*100xxx*/
	4, 4, 4, 4, 4, 4, 4, 4,
	/*101xxx*/
	5, 5, 5, 5, 5, 5, 5, 5,
	/*110xxx*/
	6, 6, 6, 6, 6, 6, 6, 6,
	/*111xxx*/
	7, 7, 7, 7, 7, 7, 7, 7
};
EXPORT_SYMBOL(qdrv_sch_dscp2dot1p);

__sram_data uint8_t qdrv_sch_dscp2tid[QTN_MAX_BSS_VAPS][IP_DSCP_MAPPING_SIZE] = {{0}};
EXPORT_SYMBOL(qdrv_sch_dscp2tid);

__sram_data uint16_t qdrv_sch_vlan2index[QTN_MAX_BSS_VAPS] = {
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL,
	VLANID_INDEX_INITVAL
};
EXPORT_SYMBOL(qdrv_sch_vlan2index);

__sram_data uint8_t qdrv_vap_vlan_max = 0;
EXPORT_SYMBOL(qdrv_vap_vlan_max);

struct shared_params *soc_shared_params = NULL;
EXPORT_SYMBOL(soc_shared_params);

unsigned global_auc_config = SHARED_PARAMS_AUC_CONFIG_ASSERT_EN;
EXPORT_SYMBOL(global_auc_config);

int soc_id(void)
{
	/* Return the SOC we are running on, starting from 1 */
	return 1;
}
EXPORT_SYMBOL(soc_id);

u32 chip_id(void)
{
	return readl(IO_ADDRESS(RUBY_SYS_CTL_CSR));
}
EXPORT_SYMBOL(chip_id);

const char* get_board_id(void)
{
	return board_id;
}
EXPORT_SYMBOL(get_board_id);

const unsigned char* get_ethernet_addr(void)
{
	static int once = 0;
	if (once == 0) {
		get_random_bytes(&ethernet_addr[5], 1);
		printk("Random stuff %02X\n", ethernet_addr[5]);
		once = 1;
	}
	return ethernet_addr;
}
EXPORT_SYMBOL(get_ethernet_addr);

static int __init
setup_disable_wd(char *buf)
{
	if (buf != NULL) {
		if (sscanf(buf, "%d", &global_disable_wd) != 1) {
			printk(KERN_WARNING"Expecting integer value(0/1)\n");
		}
	}
	return 0;
}
early_param("disable_wd", setup_disable_wd);

static int __init
setup_auc_config(char *buf)
{
	if (buf != NULL) {
		if (sscanf(buf, "%x", &global_auc_config) != 1) {
			printk(KERN_WARNING"auc_config expecting hexadecimal integer\n");
		}
	}
	return 0;
}
early_param("auc_config", setup_auc_config);


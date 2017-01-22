/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef _QDRV_SOC_H
#define _QDRV_SOC_H

#include <linux/workqueue.h>
#include <linux/seq_file.h>
#include <asm/io.h>
#include <compat.h>

#include <qtn/mproc_sync.h>

#define  MAX_GPIO_PIN	31
#define  MAX_GPIO_INTR	23


#define HAL_REGISTER_TSF_LOW 0xE5053014 /* FIXME ADM: Variable based on HW version.*/
#define HAL_REGISTER_TSF_HIGH 0xE5053018 /* FIXME ADM: Variable based on HW version.*/

#define VERSION_SIZE	16

struct qdrv_packet_counters
{
	u32	num_tx;
	u32	num_rx;
}; 

struct qdrv_packet_report
{
	struct qdrv_packet_counters	rf1;
	struct qdrv_packet_counters	rf2;
};

struct qdrv_cal_test_setting
{
	u8 antenna;
	u8 mcs;
	u8 bw_set;
	u8 pkt_len;
	u8 is_eleven_N;
	u8 bf_factor_set;
};

#define QDRV_POWER_TABLE_FNAME_MAX_LEN		63
#define	QDRV_POWER_TABLE_CHECKSUM_LEN		32	/* MD5 Hex */

struct qdrv_power_table_checksum_entry
{
	struct qdrv_power_table_checksum_entry *next;
	char fname[QDRV_POWER_TABLE_FNAME_MAX_LEN + 1];
	char checksum[QDRV_POWER_TABLE_CHECKSUM_LEN + 1];
};

struct qdrv_power_table_control
{
	/* the checksum list of the power tables built into image */
	struct qdrv_power_table_checksum_entry *checksum_list;
	struct qdrv_power_table_checksum_entry *reading_checksum;
	uint8_t checksum_list_locked;
	uint8_t power_selection;
	uint8_t power_recheck;
};

struct qdrv_cb
{
	struct device *dev;
	unsigned int resources;
#define QDRV_RESOURCE_MAC0		0x00000008
#define QDRV_RESOURCE_MAC1		0x00000010
#define QDRV_RESOURCE_COMM		0x00000020
#define QDRV_RESOURCE_MUC		0x00000040
#define QDRV_RESOURCE_WLAN		0x00000080
#define QDRV_RESOURCE_DSP		0x00000100
#define QDRV_RESOURCE_AUC		0x00000200
#define QDRV_RESOURCE_MUC_BOOTED	0x00000400
#define QDRV_RESOURCE_UC_PRINT		0x00000800
#define QDRV_RESOURCE_VAP_0		0x00001000
#define QDRV_RESOURCE_VAP(unit)		(QDRV_RESOURCE_VAP_0 << (unit))

	struct qdrv_mac macs[MAC_UNITS];
	u8 mac0[IEEE80211_ADDR_LEN];
	u8 mac1[IEEE80211_ADDR_LEN];
	u8 instances;
	char muc_firmware[64];
	char dsp_firmware[64];
	char auc_firmware[64];
	char algo_version[VERSION_SIZE];
#define QDRV_CMD_LENGTH	128
	char command[QDRV_CMD_LENGTH];
	struct work_struct comm_wq;
	int rc;

	/* Generic driver read support (sequence file in /proc) */
	void (*read_show)(struct seq_file *s, void *data, u32 num);
	int read_start_num;
	int read_num;
	int read_decr;
	void *read_data;

	u32 value_from_muc;

	/* Memory read specific */
	u32 read_addr;
	u32 read_count;
	int values_per_line;

	/* RF register value is now returned from the MuC */
	u32 rf_reg_val;

	/*
	 * DSP GPIO pin levels.  All are reported,
	 * but currently only pins 0 and 8 have any significance
	 */
	u32 dspgpios;

	struct qdrv_packet_report	packet_report;

	struct qdrv_power_table_control power_table_ctrl;

	u8 current_gpio_pin;
	u8 current_gpio_setting;
	struct qdrv_mac_params params; /* MAC parameters configured prior to bringup of the device */
	volatile u32 *hlink_mbox;

	int temperature_rfic_external;
	int temperature_rfic_internal;

	int calstate_vpd;

	union _qdrv_cal_test_report_u_{
		u32 tx_power[4];
		int rssi[4];
		struct qdrv_cal_test_setting setting;
		int post_rfloop_success;
	}qdrv_cal_test_report;
};

static __always_inline int sem_take(u32 sem, u32 bit)
{
	return qtn_mproc_sync_set_hw_sem(sem, bit);
}

static __always_inline void sem_give(u32 sem, u32 bit)
{
	qtn_mproc_sync_clear_hw_sem(sem , bit);
}

static __always_inline void _writel_wmb(u32 val, u32 addr)
{
	writel(val, addr);
	wmb();
	qtn_addr_wmb(addr);
}
#define writel_wmb(v, a) _writel_wmb((__force __u32)(v), (__force __u32)(a))

int qdrv_soc_cb_size(void);
int qdrv_soc_start_vap(struct qdrv_cb *qcb, int devid, struct qdrv_mac *mac,
	char *name, uint8_t *mac_addr, int opmode, int flags);
int qdrv_soc_stop_vap(struct qdrv_cb *qcb, struct qdrv_mac *mac, struct net_device *vdev);
int qdrv_soc_stats(void *data, struct qdrv_mac *mac);
uint32_t qdrv_soc_get_hostlink_mbox(void);
uint32_t qdrv_soc_get_hw_options(void);
char *qdrv_soc_get_hw_desc(enum hw_opt_t bond_opt);
char *qdrv_soc_get_hw_id(enum hw_opt_t bond_opt);
const char *qdrv_soc_get_hw_rev_desc(uint16_t hw_rev);
int qdrv_soc_init(struct device *dev);
int qdrv_soc_exit(struct device *dev);
struct device *qdrv_soc_get_addr_dev(void);
int qdrv_start_dsp_only(struct device *dev);

#endif

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

#ifndef _QDRV_CONTROL_H
#define _QDRV_CONTROL_H

#include <linux/seq_file.h>
#include "qdrv_mac.h"
#include "qdrv_wlan.h"

int qdrv_radar_is_test_mode(void);

int qdrv_get_wps_push_button_config( u8 *p_gpio_pin, u8 *p_use_interrupt, u8 *p_active_logic );
void set_wps_push_button_enabled( void );

void qdrv_control_txbf_pkt_send(void *data, u8 *stvec, u32 bw);
void qdrv_control_sysmsg_send(void *data, char *sysmsg, u_int32_t len, int send_now);
void qdrv_control_sysmsg_timer(unsigned long data);

int qdrv_control_init(struct device *dev);
int qdrv_control_exit(struct device *dev);
int qdrv_control_output(struct device *dev, char *buf);
int qdrv_control_input(struct device *dev, char *buf, unsigned int count);
void qdrv_control_set_show(void (*fn)(struct seq_file *s, void *data, u32 num),
	void *data, int start_num, int decr);
int qdrv_command_read_rf_reg(struct device *dev, int offset);
int qdrv_parse_mac(const char *mac_str, u8 *mac);
void qdrv_pktlogger_flush_data(struct qdrv_wlan *qw);

enum
{
	GET_MAC_ADDRESS1 = 0,
	VCO_CALIBRATION,
	IQ_COMP_CALIBRATION,
	DC_OFFSET_CALIBRATION,
	GET_AP_INFO,
	GET_PHY_INFO,
	SET_MAC_ADDRESS,
	GET_VERSION,
	SET_TXONLY_MODE,
	SET_AFE_PATTERN = 9,
	GET_MAC_ADDRESS,
	GET_CHIP_ID,
	MAX_CMD
};

typedef struct qdrv_event {
	char *str;
	u32 jiffies;
	u32 clk;
	int arg1;
	int arg2;
	int arg3;
	int arg4;
	int arg5;
} qdrv_event_t;

#define QDRV_EVENT_LOG_SIZE	1024
#define QDRV_EVENT_LOG_MASK	(QDRV_EVENT_LOG_SIZE - 1)

enum {

	EXT_TEMPERATURE_SENSOR_REPORT_FLAG = 0x11111111,
	DISABLE_REPORT_FLAG  = 0x22222222
};

void qdrv_event_log(char *str, int arg1, int arg2, int arg3, int arg4, int arg5);
int qdrv_eventf(struct net_device *dev, const char *fmt, ...);
void qdrv_control_dump_active_hwreg(void);

// convenience functions
#define QDRV_EVENT(x)			(qdrv_event_log(x,0,0,0,0,0))
#define QDRV_EVENT_1(x,a)		(qdrv_event_log(x,a,0,0,0,0))
#define QDRV_EVENT_2(x,a,b)		(qdrv_event_log(x,a,b,0,0,0))
#define QDRV_EVENT_3(x,a,b,c)		(qdrv_event_log(x,a,b,c,0,0))
#define QDRV_EVENT_4(x,a,b,c,d)		(qdrv_event_log(x,a,b,c,d,0))
#define QDRV_EVENT_5(x,a,b,c,d,e)	(qdrv_event_log(x,a,b,c,d,e))

/* MU related cmds */
typedef enum {
	QDRV_MU_CMD_SET = 0,
	QDRV_MU_CMD_GET = 1,
	QDRV_MU_CMD_DBG = 2,
	QDRV_MU_CMD_CLR = 3,
	QDRV_MU_CMD_FIRST_IN_GROUP_SELECTION = 4,
} qdrv_mu_cmd;

typedef enum {
	QDRV_MU_SUBMD_GRP = 0,
} qdrv_mu_subcmd;
#endif

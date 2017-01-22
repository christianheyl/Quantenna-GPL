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

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>

#include <qtn/qdrv_sch.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/stddef.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/in.h>
#include <net/sock.h>
#include <net/sch_generic.h>
#include <trace/ippkt.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/gpio.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_config.h"
#include "qdrv_control.h"
#include "qdrv_pktlogger.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_muc_stats.h"
#include "qdrv_wlan.h"
#include <linux/etherdevice.h>
#include "qdrv_radar.h"
#include <qtn/qtn_math.h>
#include "qdrv_vap.h"
#include "qdrv_bridge.h"
#include "qdrv_bld.h"
#include "qdrv_netdebug_checksum.h"
#include "qdrv_bld.h"
#include "qdrv_vlan.h"
#include "qdrv_mac_reserve.h"
#include <radar/radar.h>
#include <asm/board/soc.h>
#include <qtn/mproc_sync_base.h>
#include <common/ruby_version.h>
#include <common/ruby_mem.h>
#include <qtn/qtn_bb_mutex.h>
#include <qtn/hardware_revision.h>
#include <qtn/emac_debug.h>
#include <qtn/ruby_cpumon.h>
#include <qtn/qtn_muc_stats_print.h>
#include <qtn/qtn_vlan.h>
#include <asm/board/troubleshoot.h>
#include <linux/net/bridge/br_public.h>
#include "qdrv_show.h"
#include <qtn/txbf_mbox.h>
#include <net/iw_handler.h> /* wireless_send_event(..) */
#include <linux/pm_qos_params.h>

#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_vlan_cpuif.h>

#ifdef MTEST
#include "../mtest/mtest.h"
#endif

static const struct qtn_auc_stat_field auc_field_stats[] = {
#if !defined(CONFIG_TOPAZ_PCIE_HOST)
#include <qtn/qtn_auc_stats_fields.h>
#endif
};

#define STR2L(_str)    (simple_strtol(_str, 0, 0))

static int qdrv_command_start(struct device *dev, int argc, char *argv[]);
static int qdrv_command_stop(struct device *dev, int argc, char *argv[]);
static int qdrv_command_get(struct device *dev, int argc, char *argv[]);
static int qdrv_command_set(struct device *dev, int argc, char *argv[]);
static int qdrv_command_read(struct device *dev, int argc, char *argv[]);
static int qdrv_command_write(struct device *dev, int argc, char *argv[]);
static int qdrv_command_calcmd(struct device *dev, int argc, char *argv[]);
static int qdrv_command_led(struct device *dev, int argc, char *argv[]);
static int qdrv_command_gpio(struct device *dev, int argc, char *argv[]);
static int qdrv_command_pwm(struct device *dev, int argc, char *argv[]);
static int qdrv_command_memdebug(struct device *dev, int argc, char *argv[]);
static int qdrv_command_radar(struct device *dev, int argc, char *argv[]);
static int qdrv_command_rifs(struct device *dev, int argc, char *argv[]);
static int qdrv_command_bridge(struct device *dev, int argc, char *argv[]);
static int qdrv_command_clearsram(struct device *dev, int argc, char *argv[]);
static int qdrv_command_dump(struct device *dev, int argc, char *argv[]);
static int qdrv_command_muc_memdbg(struct device *dev, int argc, char *argv[]);
static int qdrv_command_pktlogger(struct device *dev, int argc, char *argv[]);
static int qdrv_command_rf_reg_dump(struct device *dev, int argc, char *argv[]);

#if defined(QTN_DEBUG)
static int qdrv_command_dbg(struct device *dev, int argc, char *argv[]);
#endif
#ifdef QDRV_TX_DEBUG
static int qdrv_command_txdbg(struct device *dev, int argc, char *argv[]);
#endif
static int qdrv_command_mu(struct device *dev, int argc, char *argv[]);
unsigned int g_dbg_dump_pkt_len = 0;

unsigned int g_qos_q_merge = 0x00000000;
unsigned int g_catch_fcs_corruption = 0;

static unsigned int g_qdrv_radar_test_mode = 0;
int qdrv_radar_is_test_mode(void)
{
	return !!g_qdrv_radar_test_mode;
}

#if defined (ERICSSON_CONFIG)
int qdrv_wbsp_ctrl = 1;
#else
int qdrv_wbsp_ctrl = 0;
#endif

static u8 wifi_macaddr[IEEE80211_ADDR_LEN];
static unsigned int s_txif_list_max = QNET_TXLIST_ENTRIES_DEFAULT;

uint32_t g_carrier_id = 0;
EXPORT_SYMBOL(g_carrier_id);

#define LED_FILE "/mnt/jffs2/led.txt"
//#define LED_FILE "/scripts/led.txt"
#define QDRV_UC_STATS_DESC_LEN 35

/*
 * In core.c, linux/arch/arm/mach-ums
 */
extern void set_led_gpio(unsigned int gpio_num, int val);
extern int get_led_gpio(unsigned int gpio_num);

#define ENVY2_REV_A 0x0020

static struct semaphore s_output_sem;
static struct qdrv_cb *s_output_qcb = NULL;

/* Static buffer for holding kernel crash across reboot */
static char *qdrv_crash_log = NULL;
static uint32_t qdrv_crash_log_len = 0;

static struct command_cb
{
	char *command;
	int (*fn)(struct device *dev, int argc, char *argv[]);
}

s_command_table[] =
{
	{ "start",	qdrv_command_start },
	{ "stop",	qdrv_command_stop },
	{ "set",	qdrv_command_set },
	{ "get",	qdrv_command_get },
	{ "read",	qdrv_command_read },
	{ "write",	qdrv_command_write },
	{ "calcmd",	qdrv_command_calcmd },
	{ "ledcmd",	qdrv_command_led },
	{ "gpio",	qdrv_command_gpio },
	{ "pwm",	qdrv_command_pwm },
	{ "radar",	qdrv_command_radar },
	{ "bridge",	qdrv_command_bridge },
	{ "memdebug",	qdrv_command_memdebug },
	{ "clearsram",	qdrv_command_clearsram },
	{ "dump",	qdrv_command_dump },
	{ "muc_memdbg",	qdrv_command_muc_memdbg },
	{ "rifs",	qdrv_command_rifs },
#if defined(QTN_DEBUG)
	{ "dbg",	qdrv_command_dbg },
#endif
	{ "pktlogger",	qdrv_command_pktlogger },
#ifdef QDRV_TX_DEBUG
	{ "txdbg",	qdrv_command_txdbg },
#endif
	{ "mu",         qdrv_command_mu },
	{ "rf_regdump",  qdrv_command_rf_reg_dump },
};

#define COMMAND_TABLE_SIZE (sizeof(s_command_table)/sizeof(struct command_cb))

#define membersizeof(type, field) \
	sizeof(((type *) NULL)->field)

static struct param_cb
{
	char *name;
	unsigned int flags;
#define P_FL_TYPE_INT		0x00000001
#define P_FL_TYPE_STRING	0x00000002
#define P_FL_TYPE_MAC		0x00000004
	char *address;
	int offset;
	int size;
}
s_param_table[] =
{
	{
		"mac0addr", P_FL_TYPE_MAC, NULL,
		offsetof(struct qdrv_cb, mac0), membersizeof(struct qdrv_cb, mac0)
	},
	{
		"mac1addr", P_FL_TYPE_MAC, NULL,
		offsetof(struct qdrv_cb, mac1), membersizeof(struct qdrv_cb, mac1)
	},
	{
		"wifimacaddr", P_FL_TYPE_MAC, (char *)&wifi_macaddr[ 0 ],
		0, sizeof(wifi_macaddr)
	},
	{
		"mucfw", P_FL_TYPE_STRING, NULL,
		offsetof(struct qdrv_cb, muc_firmware), membersizeof(struct qdrv_cb, muc_firmware)
	},
	{
		"dspfw", P_FL_TYPE_STRING, NULL,
		offsetof(struct qdrv_cb, dsp_firmware), membersizeof(struct qdrv_cb, dsp_firmware)
	},
	{
		"aucfw", P_FL_TYPE_STRING, NULL,
		offsetof(struct qdrv_cb, auc_firmware), membersizeof(struct qdrv_cb, auc_firmware)
	},
	{
		"dump_pkt_len", P_FL_TYPE_INT, (char *) &g_dbg_dump_pkt_len,
		0, sizeof(g_dbg_dump_pkt_len)
	},
	{
		"uc_flags", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"txif_list_max", P_FL_TYPE_INT, (char *) &s_txif_list_max,
		0, sizeof(s_txif_list_max)
	},
	{
		"catch_fcs_corruption", P_FL_TYPE_INT, (char *) &g_catch_fcs_corruption,
		0, sizeof(g_catch_fcs_corruption)
	},
	{
		"muc_qos_q_merge", P_FL_TYPE_INT, (char *) &g_qos_q_merge,
		0, sizeof(g_qos_q_merge)
	},
	{
		"test1", P_FL_TYPE_INT, (char *) &g_qdrv_radar_test_mode,
		0, sizeof(g_qdrv_radar_test_mode)
	},
	{
		"vendor_fix", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"vap_default_state", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"brcm_rxglitch_thrshlds", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"vlan_promisc", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"pwr_mgmt", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"rxgain_params", P_FL_TYPE_INT, NULL,
		0, sizeof(u_int32_t)
	},
	{
		"wbsp_ctrl", P_FL_TYPE_INT, (char *) &qdrv_wbsp_ctrl,
		0, sizeof(qdrv_wbsp_ctrl)
	},
};

#define PARAM_TABLE_SIZE (sizeof(s_param_table)/sizeof(struct param_cb))

static struct qdrv_event qdrv_event_log_table[QDRV_EVENT_LOG_SIZE];
static int qdrv_event_ptr = 0;
static spinlock_t qdrv_event_lock;

struct qdrv_show_assoc_params g_show_assoc_params;

void qdrv_event_log(char *str, int arg1, int arg2, int arg3, int arg4, int arg5)
{
	spin_lock(&qdrv_event_lock);
	qdrv_event_log_table[qdrv_event_ptr].jiffies = jiffies;
	qdrv_event_log_table[qdrv_event_ptr].clk = 0;
	qdrv_event_log_table[qdrv_event_ptr].str = str;
	qdrv_event_log_table[qdrv_event_ptr].arg1 = arg1;
	qdrv_event_log_table[qdrv_event_ptr].arg2 = arg2;
	qdrv_event_log_table[qdrv_event_ptr].arg3 = arg3;
	qdrv_event_log_table[qdrv_event_ptr].arg4 = arg4;
	qdrv_event_log_table[qdrv_event_ptr].arg5 = arg5;
	qdrv_event_ptr = (qdrv_event_ptr + 1) & QDRV_EVENT_LOG_MASK;
	spin_unlock(&qdrv_event_lock);
}
EXPORT_SYMBOL(qdrv_event_log);

/* used for send formatted string custom event IWEVCUSTOM */
int qdrv_eventf(struct net_device *dev, const char *fmt, ...)
{
	va_list args;
	int i;
	union iwreq_data wreq;
	char buffer[IW_CUSTOM_MAX];

	if (dev == NULL) {
		return 0;
	}

	/* Format the custom wireless event */
	memset(&wreq, 0, sizeof(wreq));

	va_start(args, fmt);
	i = vsnprintf(buffer, IW_CUSTOM_MAX, fmt, args);
	va_end(args);

	wreq.data.length = strnlen(buffer, IW_CUSTOM_MAX);
	wireless_send_event(dev, IWEVCUSTOM, &wreq, buffer);
	return i;
}
EXPORT_SYMBOL(qdrv_eventf);

static int qdrv_command_is_valid_addr(unsigned long addr)
{
	if (is_valid_mem_addr(addr)) {
		return 1;
	} else if (addr >= RUBY_ARC_CACHE_BYPASS) {
		/* ARC's no-cache, TLB-bypass region where we have all our registers */
		return 1;
	}
	return 0;
}

int qdrv_parse_mac(const char *mac_str, uint8_t *mac)
{
	unsigned int tmparray[IEEE80211_ADDR_LEN];

	if (mac_str == NULL)
		return -1;

	if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
			&tmparray[0],
			&tmparray[1],
			&tmparray[2],
			&tmparray[3],
			&tmparray[4],
			&tmparray[5]) != IEEE80211_ADDR_LEN) {
		return -1;
	}

	mac[0] = tmparray[0];
	mac[1] = tmparray[1];
	mac[2] = tmparray[2];
	mac[3] = tmparray[3];
	mac[4] = tmparray[4];
	mac[5] = tmparray[5];

	return 0;
}

static unsigned long qdrv_read_mem(unsigned long read_addr)
{
	unsigned long retval = 0;

	if (!qdrv_command_is_valid_addr(read_addr)) {
		DBGPRINTF_E("Q driver read mem, address 0x%lx is not valid\n", read_addr);
		retval = -1;
	} else {
		unsigned long *segvaddr = ioremap_nocache(read_addr, sizeof(*segvaddr));

		if (segvaddr == NULL) {
			DBGPRINTF_E("Q driver read mem, failed to remap address 0x%lx\n", read_addr);
			retval = -1;
		} else {
			retval = *segvaddr;
			iounmap(segvaddr);
		}
	}

	return retval;
}

static void qdrv_show_memory(struct seq_file *s, void *data, size_t num)
{
	struct qdrv_cb *qcb = data;

	if (!qdrv_command_is_valid_addr(qcb->read_addr)) {
		seq_printf(s, "%08x: invalid addr\n", qcb->read_addr);
	} else {
		unsigned long *segvaddr_base = ioremap_nocache(qcb->read_addr,
				sizeof(*segvaddr_base) * qcb->values_per_line);

		if (!segvaddr_base) {
			seq_printf(s, "%08x: remapping failed\n", qcb->read_addr);
		} else {
			int limit = qcb->values_per_line - 1;
			unsigned long *segvaddr_moving = segvaddr_base;

			seq_printf(s, "%08x: %08lx", qcb->read_addr, *segvaddr_moving);
			qcb->read_addr += sizeof(*segvaddr_moving);
			qcb->read_count--;
			segvaddr_moving++;

			if (limit > qcb->read_count) {
				limit = qcb->read_count;
			}

			if (qcb->values_per_line > 1 && limit > 0) {
				int i;

				for (i = 0; i < limit; i++) {
					seq_printf(s, " %08lx", *segvaddr_moving);
					qcb->read_addr += sizeof(*segvaddr_moving);
					qcb->read_count--;
					segvaddr_moving++;
				}
			}

			seq_printf(s, "\n");
			iounmap(segvaddr_base);
		}
	}
}

/* Post command_set processing - propagate the changes in qcb into other structures */
static void
qdrv_command_set_post(struct qdrv_cb *qcb)
{
	qcb->params.txif_list_max = s_txif_list_max;
}

static struct qdrv_wlan *qdrv_control_wlan_get(struct qdrv_mac *mac)
{
	if (mac->data == NULL) {
		/* This will happen for PCIe host */
		DBGPRINTF_N("WLAN not found\n");
	}

	return (struct qdrv_wlan *)mac->data;
}

static struct qdrv_mac *qdrv_control_mac_get(struct device *dev, const char *argv)
{
	struct qdrv_cb *qcb = dev_get_drvdata(dev);
	unsigned int unit;

	if ((sscanf(argv, "%d", &unit) != 1) || (unit > (MAC_UNITS - 1))) {
		DBGPRINTF_E("Invalid MAC unit %u in control command\n", unit);
		return NULL;
	}

	if (&qcb->macs[unit].data == NULL) {
		return NULL;
	}

	return &qcb->macs[unit];
}

struct ieee80211com *qdrv_get_ieee80211com(struct device *dev)
{
	struct qdrv_wlan *qw;
	struct qdrv_mac *mac = qdrv_control_mac_get(dev, "0");

	if (!mac) {
		return NULL;
	}

	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return NULL;
	}

	return &qw->ic;
}

/*
 * check that MuC has completed its own initialisation code, and the boot complete
 * hostlink message has been processed.
 *
 * 1 on success, 0 otherwise
 */
static int check_muc_boot(struct device *dev, void *token)
{
	(void)token;
	struct qdrv_cb *qcb = dev_get_drvdata(dev);
	uint32_t *resources = &qcb->resources;
	uint32_t mask = QDRV_RESOURCE_WLAN | QDRV_RESOURCE_MUC_BOOTED;
	char *desc;

	if ((*resources & mask) != mask)
		return 0;

	desc = qdrv_soc_get_hw_desc(0);
	if (desc[0] == '\0')
		panic("QDRV: invalid bond option");

	printk("QDRV: hardware is %s\n", desc);

	return 1;
}

/*
 * Check that vap has been created successfully, 1 on success
 */
static int check_vap_created(struct device *dev, void *token)
{
	struct qdrv_cb *qcb = dev_get_drvdata(dev);
	int vap = (int) token;

	if (vap < 0 || vap >= QDRV_MAX_VAPS) {
		DBGPRINTF_E("invalid VAP %d\n", vap);
		return 1;
	}

	return qcb->resources & QDRV_RESOURCE_VAP(vap);
}

static int check_vap_deleted(struct device *dev, void *token)
{
	struct qdrv_cb *qcb = dev_get_drvdata(dev);
	int vap = (int) token;

	if (vap < 0 || vap >= QDRV_MAX_VAPS) {
		DBGPRINTF_E("invalid VAP %d\n", vap);
		return 1;
	}

	return (qcb->resources & QDRV_RESOURCE_VAP(vap)) == 0;
}

/**
 * block until a condition is met, which is provided by check_func.
 * check_func returns 1 on successful condition.
 *
 * if booting the muc for the first time,
 * wlan (mac->data) will not be available yet.
 * assume qcb->macs[0] is the mac of interest here.
 *
 * returns 0 on successful condition, -1 on timeout, crash, or
 * mac/wlan/shared_params not properly initialized
 */
static int qdrv_command_start_block(struct device *dev, const char* description,
		int (*check_func)(struct device *cf_dev, void *cf_token), void *token)
{
	const unsigned long warn_threshold_msecs = 5000;
	unsigned long start_jiff = jiffies;
	unsigned long deadline = start_jiff + (MUC_BOOT_WAIT_SECS * HZ);
	int can_block = !in_atomic();
	int ret = -1;
	int complete = 0;
	struct qdrv_cb *qcb = dev_get_drvdata(dev);
	struct qdrv_mac *mac = &qcb->macs[0];

	BUG_ON(!can_block);
	BUG_ON(!mac);

#ifdef MTEST
	complete = 1;
	ret = 0;
#endif

	while (!complete) {
		if (time_after(jiffies, deadline)) {
			DBGPRINTF_E("Timeout waiting for %s; waited %u seconds\n",
						description, MUC_BOOT_WAIT_SECS);
			complete = 1;
		} else if (mac && mac->dead) {
			DBGPRINTF_E("Failure waiting for %s\n",
						description);
			complete = 1;
		} else if (check_func(dev, token)) {
			unsigned long elapsed_msecs;
			const char *slow_warn = "";

			/* once alive, qw should be initialised */
			BUG_ON(qdrv_control_wlan_get(mac) == NULL);

			elapsed_msecs = jiffies_to_msecs(jiffies - start_jiff);
			if (elapsed_msecs > warn_threshold_msecs) {
				slow_warn = " (SLOW)";
			}

			printk(KERN_INFO "%s succeeded %lu.%03lu seconds%s\n",
					description,
					elapsed_msecs / MSEC_PER_SEC,
					elapsed_msecs % MSEC_PER_SEC,
					slow_warn);

			complete = 1;
			ret = 0;
		}

		if (can_block) {
			msleep(1);
		}
	}

	return ret;
}

static int qdrv_soc_get_next_devid(const struct qdrv_cb *qcb)
{
	int maci;
	int vdevi;
	int ndevs = QDRV_RESERVED_DEVIDS;

	for (maci = 0; maci < MAC_UNITS; maci++) {
		for (vdevi = 0; vdevi < QDRV_MAX_VAPS; vdevi++) {
			if (qcb->macs[maci].vnet[vdevi] == NULL)
				break;

			ndevs++;
		}
	}

	return ndevs;
}

static int qdrv_control_mimo_mode_set(struct qdrv_mac *mac, struct net_device *vdev, int opmode)
{
	struct qdrv_vap *qv = netdev_priv(vdev);
	int mimo_mode;

	if (qv == NULL)
		return -1;

	if (opmode == IEEE80211_M_HOSTAP && mac->mac_active_bss > 1)
		return 0;

	if (ieee80211_swfeat_is_supported(SWFEAT_ID_4X4, 0))
		mimo_mode = 4;
	else if (ieee80211_swfeat_is_supported(SWFEAT_ID_3X3, 0))
		mimo_mode = 3;
	else
		mimo_mode = 2;

	ieee80211_param_to_qdrv(&qv->iv, IEEE80211_PARAM_MIMOMODE, mimo_mode, NULL, 0);

	return 0;
}

static int qdrv_command_dev_init(struct device *dev, struct qdrv_cb *qcb)
{
	char *argv[] = { "test", "31", "0", "4", "0" };
	int bond;

	if (qcb->resources != 0) {
		DBGPRINTF_W("Driver is already started\n");
		return -1;
	}

	if (qdrv_soc_init(dev) < 0)
		panic("Restarting due to SoC failure to initialise");

	if (qdrv_command_start_block(dev, "MuC boot", &check_muc_boot, NULL))
		panic("Restarting due to failed MuC");

	if (get_hardware_revision() >= HARDWARE_REVISION_TOPAZ_A2) {
#ifdef CONFIG_TOPAZ_PCIE_TARGET
		/* There is no bond EMAC for PCIe EP board */
		bond = 0;
#else
		bond = topaz_emac_get_bonding();
#endif
		topaz_tqe_emac_reflect_to(TOPAZ_TQE_LHOST_PORT,  bond);
	}

	qdrv_command_calcmd(dev, ARRAY_SIZE(argv), argv);

	return 0;
}

static int qdrv_command_start(struct device *dev, int argc, char *argv[])
{
	struct qdrv_cb *qcb;
	struct qdrv_mac *mac;
	struct net_device *vdev = NULL;
	uint8_t mac_addr[IEEE80211_ADDR_LEN] = {0};
	int opmode = -1;
	int flags = 0;
	char *p;
	int dev_id;
	int rc;

	qcb = dev_get_drvdata(dev);

	if (argc == 1) {
		if (qdrv_command_dev_init(dev, qcb) < 0)
			return -1;
	} else if ((argc == 4) || (argc == 5)) {
		for (p = argv[1]; *p != '\0'; p++) {
			if (!isdigit(*p)) {
				goto error;
			}
		}

		mac = qdrv_control_mac_get(dev, argv[1]);
		if (mac == NULL) {
			goto error;
		}

		if (strncmp(argv[2], "ap", 2) == 0) {
			DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL, "AP\n");
			opmode = IEEE80211_M_HOSTAP;
		} else if (strncmp(argv[2], "sta", 3) == 0) {
			DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL, "STA\n");
			opmode = IEEE80211_M_STA;
		} else if (strncmp(argv[2], "wds", 3) == 0) {
			DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL, "WDS\n");
			opmode = IEEE80211_M_WDS;
		} else {
			goto error;
		}

		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL,
				"Interface name \"%s\"\n", argv[3]);

		vdev = dev_get_by_name(&init_net, argv[3]);
		if (vdev != NULL) {
			DBGPRINTF_E("The device name \"%s\" already exists\n", argv[3]);
			dev_put(vdev);
			goto error;
		}

		if (argc == 5) {
			if (qdrv_parse_mac(argv[4], mac_addr) < 0) {
				DBGPRINTF_E("Error mac address for new vap\n");
				goto error;
			}
		}

		if (opmode == IEEE80211_M_HOSTAP &&
				mac->mac_active_bss == QDRV_MAX_BSS_VAPS) {
			DBGPRINTF_E("Maximum MBSSID VAPs reached (%d)\n",
				 QDRV_MAX_BSS_VAPS);
			goto error;
		}

		if (opmode == IEEE80211_M_WDS &&
				mac->mac_active_wds == QDRV_MAX_WDS_VAPS) {
			DBGPRINTF_E("Maximum WDS peers reached (%d)\n",
				 QDRV_MAX_WDS_VAPS);
			goto error;
		}
	} else if (argc == 2) {
		if (strncmp(argv[1], "dsp", sizeof("dsp")) == 0)
			(void) qdrv_start_dsp_only(dev);
		else
			goto error;
	} else {
		goto error;
	}

	/* Act on the opmode if set */
	if (opmode > 0) {
		dev_id = qdrv_soc_get_next_devid(qcb);

		if (qdrv_soc_start_vap(qcb, dev_id, mac, argv[3], mac_addr, opmode, flags) < 0) {
			DBGPRINTF_E("Failed to start VAP \"%s\"\n", argv[3]);
			return -1;
		}

		if (qdrv_command_start_block(dev, "VAP create", &check_vap_created,
				(void *)QDRV_WLANID_FROM_DEVID(dev_id) )) {
			return -1;
		}

		if (opmode == IEEE80211_M_HOSTAP) {
			mac->mac_active_bss++;
		} else if (opmode == IEEE80211_M_WDS) {
			mac->mac_active_wds++;
		}

		vdev = dev_get_by_name(&init_net, argv[3]);
		if (vdev != NULL) {
			rc = qdrv_control_mimo_mode_set(mac, vdev, opmode);
			dev_put(vdev);
			if (rc != 0) {
				return -1;
			}
		}
	}

	return 0;

error:
	DBGPRINTF_E("Invalid arguments to start command\n");

	return -1;
}

static int qdrv_command_stop_one_vap(struct device *dev, struct qdrv_mac *mac, const char *vapname)
{
	struct qdrv_cb *qcb = dev_get_drvdata(dev);
	struct net_device *vdev = dev_get_by_name(&init_net, vapname);
	struct qdrv_vap *qv;
	enum ieee80211_opmode opmode;
	unsigned int wlanid;

	if (vdev == NULL) {
		DBGPRINTF_E("net device \"%s\" doesn't exist\n", vapname);
		return -ENODEV;
	}
	dev_put(vdev);

	qv = netdev_priv(vdev);
	opmode = qv->iv.iv_opmode;
	wlanid = QDRV_WLANID_FROM_DEVID(qv->devid);

	if (qdrv_vap_exit(mac, vdev) < 0) {
		DBGPRINTF_E("Failed to exit VAP \"%s\"\n", vapname);
		return -1;
	}

	if (qdrv_soc_stop_vap(qcb, mac, vdev) < 0) {
		DBGPRINTF_E("Failed to stop VAP \"%s\"\n", vapname);
		return -1;
	}

	if (opmode == IEEE80211_M_HOSTAP) {
		mac->mac_active_bss--;
	} else if (opmode == IEEE80211_M_WDS) {
		mac->mac_active_wds--;
	}

	if (qdrv_command_start_block(dev, "VAP delete", &check_vap_deleted, (void *)wlanid)) {
		return -1;
	}

	return 0;
}

static int qdrv_command_stop(struct device *dev, int argc, char *argv[])
{
	struct qdrv_mac *mac;
	char *p;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (argc == 1) {
		if(qdrv_soc_exit(dev) < 0) {
			return(-1);
		}
	} else if (argc == 3) {
		const char *macstr = argv[1];
		const char *vapname = argv[2];

		/* Second argument must be all digits */
		for (p = argv[1]; *p != '\0'; p++) {
			if (!isdigit(*p)) {
				goto error;
			}
		}

		mac = qdrv_control_mac_get(dev, macstr);
		if (mac == NULL) {
			DBGPRINTF_E("no mac for arg: \"%s\"\n", macstr);
			return -ENODEV;
		}

		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL,
				"Interface name \"%s\"\n", vapname);

		if (strncmp(vapname, "all", 3) == 0) {
			int i;
			int res;
			struct qdrv_wlan *qw = qdrv_mac_get_wlan(mac);

			for (i = QDRV_MAX_VAPS - 1; i >= 0; --i) {
				if (mac->vnet[i]) {
					res = qdrv_command_stop_one_vap(dev, mac, mac->vnet[i]->name);
					if (res != 0)
						return res;
				}
			}

			/* deleted DFS/Radar related timers */
			qdrv_radar_unload(mac);
			qdrv_wlan_cleanup_before_reload(&qw->ic);
		} else {
			return qdrv_command_stop_one_vap(dev, mac, vapname);
		}
	} else {
		goto error;
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;

error:
	DBGPRINTF_E("Invalid arguments to stop command\n");

	return -1;
}

int _atoi(const char *str)
{
	int rVal = 0;
	int sign = 1;

	if (!str)
		return 0;

	while (*str && (*str == ' '|| *str == '\t'))
		str++;

	if (*str == '\0')
		return 0;

	if (*str == '+' || *str == '-')
		sign = (*str++ == '+') ? 1 : -1;

	while (*str && *str >= '0' && *str <= '9')
		rVal = rVal * 10 + *str++ - '0';

	return (rVal * sign);
}

#if 0
#define DEFAULT_LENGTH 4
int qdrv_command_calcmd_from_shell (char* calcmd, char **arg, int arc)
{
	int i;
	char shellInterpret[128];
	int length = DEFAULT_LENGTH;

	for(i = 0; i < 128; i++)
	{
		shellInterpret[i] = 0;
	}

	if(strcmp("VCO", arg[1])==0)
	{
		shellInterpret[0] = 0x1;
		shellInterpret[1] = 0x0;
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL | QDRV_LF_CALCMD, "VCO\n");
	}
	else if(strcmp("IQ_COMP", arg[1])==0)
	{
		shellInterpret[0] = 0x2;
		shellInterpret[1] = 0x0;
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL | QDRV_LF_CALCMD, "IQ_COMP\n");
	}
	else if(strcmp("DC_OFFSET", arg[1])==0)
	{
		shellInterpret[0] = 0x3;
		shellInterpret[1] = 0x0;
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL | QDRV_LF_CALCMD, "DC_OFFSET\n");
	}
	else
	{
		shellInterpret[0] = 0x0;
		shellInterpret[1] = 0x0;
	}

	for(i = 2; i < arc; i++)
	{
		shellInterpret[i+2] = _atoi(arg[i]);
		length ++;
	}

	shellInterpret[2] = (length & 0x00FF);
	shellInterpret[3] = (length & 0xFF00)>>8;

	memcpy(calcmd, shellInterpret, length);
	return length;
}
#endif

#define QDRV_LED_MAX_READ_SIZE 32

int set_led_data (char *file, int data1, int data2){
	int fd;
	int ret_val;
	int fr = -EINVAL;
	int data;
	int i;
	char buf[QDRV_LED_MAX_READ_SIZE + 1];
	mm_segment_t OldFs;
	struct file *pfile;
	loff_t pos = 0;
	char p[QDRV_LED_MAX_READ_SIZE];

	memset(buf, 0, sizeof(buf));
	OldFs = get_fs();
	set_fs(KERNEL_DS);

	data = data1;
	fd = sys_open(file, O_RDWR, 0);

	if (fd < 0) {
		fd = sys_open(file, O_CREAT | O_RDWR, 0666);
		if (fd < 0) {
			DBGPRINTF_E("Failed to Create File for LED GPIOs.\n");
			goto setfs_ret;
		}
	}
	if (fd >= 0) {
		fr = sys_read(fd, buf, QDRV_LED_MAX_READ_SIZE);

		if (fr == 0) {
			for (i = 0; i < 2; i++) {
				sprintf(p, "%d ", data);
				pfile = fget(fd);
				if (pfile) {
					pos = pfile->f_pos;
					ret_val = vfs_write(pfile, p, strlen(p), &pos);
					fput(pfile);
					pfile->f_pos = pos;
				}
				data = data2;
			}
		} else if (fr > 0) {
		} else {
			DBGPRINTF_E("Failed to read LED GPIO file\n");
		}
		sys_close(fd);
	}

setfs_ret:
	set_fs(OldFs);
	return fr;
}

/* address range [begin, end], inclusive */
struct reg_range {
	u32 begin;
	u32 end;
	const char *description;
};

/*
 * List below is based on "golden values" registers file
 * Commented memory ranges contains "holes" which are not
 * allowed to be read or they're intentionally missed by
 * qregcheck tool.
 */
static struct reg_range qdrv_hw_regs[] = {
	{0xE0000000, 0xE0000FFF, "System Control regs"},
	/*{0xE1007FFC, 0xE1038FFF, "switch regs"},*/
	{0xE1020000, 0xE1020200, "HBM regs"},
	/*{0xE2000000, 0xE200000F, "SPI0 regs"},*/
	/*{0xE3100000, 0xE310009F, "AHB regs"},*/
	{0xE5030000, 0xE50300FF, "Packet memory"},
	{0xE5040000, 0xE5043FFF, "TCM"},
	{0xE5044000, 0xE50440FF, "Global control"},
	{0xE5050000, 0xE5051FFF, "Global control regs(1): TX frame processor regs"},
	{0xE5052000, 0xE5052FFF, "Global control regs(2): RX frame processor regs"},
	{0xE5050300, 0xE5053FFF, "Global control regs(3): shared frame processor regs"},
	{0xE5090000, 0xE50904FF, "Global control regs(4)"},
	{0xE6000000, 0xE60003FF, "BB Global regs"},
	{0xE6010000, 0xE60103FF, "BB BP regs"},
	{0xE6020000, 0xE602002B, "BB TXVEC regs"},
	{0xE6030000, 0xE6030037, "BB RXVEC regs"},
	{0xE6040000, 0xE604FFFF, "BB SPI0 regs"},
	{0xE6050000, 0xE60504C4, "BB MIMO regs"},
	{0xE6090000, 0xE60906FF, "BB TD (1) regs"},
	{0xE6091000, 0xE60915FF, "BB TD (2) regs"},
	{0xE6092000, 0xE60925FF, "BB TD (3) regs"},
	{0xE60A1000, 0xE60A133F, "BB TD (RFC W mem 0) regs"},
	{0xE60A2000, 0xE60A233F, "BB TD (RFC W mem 1) regs"},
	{0xE60A3000, 0xE60A333F, "BB TD (RFC W mem 2) regs"},
	{0xE60A4000, 0xE60A433F, "BB TD (RFC W mem 3) regs"},
	{0xE60A5000, 0xE60A53FF, "BB TD (RFC TX mem) regs"},
	{0xE60A6000, 0xE60A63FF, "BB TD (RX mem) regs"},
	{0xE60B1000, 0xE60B10FF, "BB TD (gain SG) regs"},
	{0xE60B2000, 0xE60B24FB, "BB TD (gain AG) regs"},
	{0xE60B3000, 0xE60B313F, "BB TD (gain AG) regs"},
	{0xE60B4000, 0xE60B4063, "BB TD (gain AG) regs"},
	{0xE60F0000, 0xE60F0260, "BB 11B regs"},
	{0xE6100000, 0xE6107FFF, "BB QMatrix regs"},
	{0xE6200000, 0xE6200FFB, "BB FFT dump registers rx chain 1"},
	{0xE6201000, 0xE6201FFB, "BB FFT dump registers rx chain 2"},
	{0xE6202000, 0xE6202FFB, "BB FFT dump registers rx chain 3"},
	{0xE6203000, 0xE6203FFB, "BB FFT dump registers rx chain 4"},
	{0xE6400000, 0xE6400FFB, "BB Radar regs"},
	{0xE8000000, 0xE8000800, "EMAC1 Control regs"},
	/*{0xE9000000, 0xE9000bFF, "PCIE regs"},*/
	/*{0xEA000000, 0xEA0003FF, "DMAC Conrol regs"},*/
	{0xED000000, 0xED000800, "EMAC0 Control regs"},
	{0xF0000000, 0xF00000FF, "UART1 Control regs"},
	{0xF1000000, 0xF100003F, "GPIO regs"},
	{0xF2000000, 0xF2000020, "SPI1 regs"},
	{0xF4000000, 0xF40000AF, "Timer Control regs"},
	{0xF5000000, 0xF50000FF, "UART2 Control regs"},
	{0xF6000000, 0xF60008FF, "DDR Control regs"},
	/*{0xF9000000, 0xF90000FC, "I2C regs"},*/
};

/*
 * Variables for the register save/comparison logic. Two buffers for caching the values
 * and an index into the hardware registers structure for the currently measured set
 * of registers.
 */
#define QDRV_MAX_REG_MONITOR ARRAY_SIZE(qdrv_hw_regs)
#define QDRV_MAX_REG_PER_BUF 3
static u32 *p_hw_reg_buf[QDRV_MAX_REG_MONITOR][QDRV_MAX_REG_PER_BUF] = {{NULL},{NULL}};

static inline u32 qdrv_control_hwreg_get_range_len(struct reg_range *range)
{
	return range->end - range->begin + 1;
}

static inline u32 qdrv_control_hwreg_get_range_reg_count(struct reg_range *range)
{
	return qdrv_control_hwreg_get_range_len(range) / 4;
}

static void qdrv_control_hwreg_trigger(int set_num, int buf_num)
{
	int num_regs;
	int i;
	u32 bytes;
	u32 *ptr;
	/* Address-> register count, with each register 32-bits */
	num_regs = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[set_num]);

	for (i = 0; i < QDRV_MAX_REG_PER_BUF - 1; i++) {
		if (p_hw_reg_buf[set_num][i] == NULL) {
			p_hw_reg_buf[set_num][i] = kmalloc(num_regs * sizeof(u32), GFP_KERNEL);
		}
	}

	bytes = qdrv_control_hwreg_get_range_len(&qdrv_hw_regs[set_num]);
	ptr = ioremap_nocache(qdrv_hw_regs[set_num].begin, bytes);
	if (ptr && p_hw_reg_buf[set_num][buf_num]) {
		memcpy(p_hw_reg_buf[set_num][buf_num], ptr, bytes);
		iounmap(ptr);
	}
}

static void qdrv_command_memdbg_usage(void)
{
	printk("Usage:\n"
		"  muc_memdbg 0 dump\n"
		"  muc_memdbg 0 status\n"
		"  muc_memdbg 0 dumpcfg <cmd> <val>\n"
		"     %u - max file descriptors to print\n"
		"     %u - max node structures to print\n"
		"     %u - max bytes per hex dump\n"
		"     %u - print rate control tables (0 or 1)\n"
		"     %u - send MuC msgs to netdebug (0 or 1)\n"
		"     %u - print MuC trace msgs (0 or 1)\n",
		QDRV_CMD_MUC_MEMDBG_FD_MAX,
		QDRV_CMD_MUC_MEMDBG_NODE_MAX,
		QDRV_CMD_MUC_MEMDBG_DUMP_MAX,
		QDRV_CMD_MUC_MEMDBG_RATETBL,
		QDRV_CMD_MUC_MEMDBG_MSG_SEND,
		QDRV_CMD_MUC_MEMDBG_TRACE);
}

void qdrv_control_sysmsg_timer(unsigned long data)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) data;
	struct qdrv_pktlogger_types_tbl *tbl =
			qdrv_pktlogger_get_tbls_by_id(QDRV_NETDEBUG_TYPE_SYSMSG);

	if (!tbl) {
		return;
	}

	qdrv_control_sysmsg_send(qw, NULL, 0, 1);
	mod_timer(&qw->pktlogger.sysmsg_timer, jiffies + (tbl->interval * HZ));
}

static void qdrv_command_muc_memdbgcfg(struct qdrv_mac *mac, struct qdrv_wlan *qw,
					unsigned int param, unsigned int val)
{
	struct qdrv_pktlogger_types_tbl *tbl = NULL;

	if (param == QDRV_CMD_MUC_MEMDBG_MSG_SEND) {
		mac->params.mucdbg_netdbg = !!val;
		if (mac->params.mucdbg_netdbg == 0) {
			qdrv_control_sysmsg_send(qw, NULL, 0, 1);
			del_timer(&qw->pktlogger.sysmsg_timer);
		} else {
			tbl = qdrv_pktlogger_get_tbls_by_id(QDRV_NETDEBUG_TYPE_SYSMSG);
			if (!tbl) {
				return;
			}
			init_timer(&qw->pktlogger.sysmsg_timer);
			qw->pktlogger.sysmsg_timer.function = qdrv_control_sysmsg_timer;
			qw->pktlogger.sysmsg_timer.data = (unsigned long)qw;
			mod_timer(&qw->pktlogger.sysmsg_timer, jiffies +
				(tbl->interval * HZ));
		}
	}

	qdrv_hostlink_msg_cmd(qw, IOCTL_DEV_CMD_MEMDBG_DUMPCFG, (param << 16) | val);
}

static int qdrv_command_muc_memdbg(struct device *dev, int argc, char *argv[])
{
	struct qdrv_mac *mac;
	struct qdrv_wlan *qw;
	u_int32_t arg = 0;
	unsigned int param;
	unsigned int val;

	if (argc < 3) {
		qdrv_command_memdbg_usage();
		return 0;
	}

	mac = qdrv_control_mac_get(dev, argv[1]);
	if (mac == NULL) {
		return -1;
	}

	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return -1;
	}

	if (strcmp(argv[2], "dump") == 0) {
		if (argc > 3) {
			if (strcmp(argv[3], "-v") == 0) {
				arg |= 0x1;
			} else {
				qdrv_command_memdbg_usage();
				return 0;
			}
		}
		qdrv_hostlink_msg_cmd(qw, IOCTL_DEV_CMD_MEMDBG_DUMP, arg);
	} else if (strcmp(argv[2], "dumpcfg") == 0) {
		if (argc < 5) {
			qdrv_command_memdbg_usage();
			return 0;
		}
		sscanf(argv[3], "%u", &param);
		sscanf(argv[4], "%u", &val);
		qdrv_command_muc_memdbgcfg(mac, qw, param, val);
	} else if (strcmp(argv[2], "status") == 0) {
		qdrv_command_muc_memdbgcfg(mac, qw, 0, 0);
	} else if (strcmp(argv[2], "dumpnodes") == 0) {
		qdrv_hostlink_msg_cmd(qw, IOCTL_DEV_CMD_MEMDBG_DUMPNODES, arg);
	} else {
		qdrv_command_memdbg_usage();
		return 0;
	}

	return 0;
}

struct qdrv_hwreg_print_data {
	struct seq_file *s;
	unsigned int set_num;
	unsigned int buf1_num;
	unsigned int buf2_num;
};

#define QDRV_HWREG_PRINT_BUF_LEN 1024
static void qdrv_dump_hwreg_print_func(struct qdrv_hwreg_print_data *sets, const char *f, ...)
{
	char buf[QDRV_HWREG_PRINT_BUF_LEN] = {0};
	va_list args;

	va_start(args, f);
	vsnprintf(buf, QDRV_HWREG_PRINT_BUF_LEN - 1, f, args);
	va_end(args);
	if (sets != NULL && sets->s != NULL) {
		seq_printf(sets->s, buf);
	} else {
		printk(buf);
	}
}

#define QDRV_HWREG_MIN_ARGS_CNT 4
#define QDRV_HWREG_MAX_ARGS_CNT 6
static void qdrv_dump_hwreg_one_reg_output(struct seq_file *s, void *data, uint32_t num)
{
	struct qdrv_hwreg_print_data *sets = (struct qdrv_hwreg_print_data*)data;
	int len = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[sets->set_num]);

	if (sets == NULL) {
		return;
	}

	sets->s = s;
	/* Inverse counter because seq file iterator uses reverse direction */
	num = len - num;
	if (num >= len) {
		printk("%s: seq file iterator: bad register number %u\n", __func__, num);
		return;
	}

	if (num == 0) {
		qdrv_dump_hwreg_print_func(sets, "#  Mac reg set %s:\n",
				qdrv_hw_regs[sets->set_num].description);
	}

	qdrv_dump_hwreg_print_func(sets, s == NULL ? "%d: 0x%08X: 0x%08X " : "%d: 0x%08X: 0x%08X\n",
			num,
			qdrv_hw_regs[sets->set_num].begin + (num*4),
			p_hw_reg_buf[sets->set_num][sets->buf1_num][num]);

	if (s == NULL && num && ((num % 2) == 0)) {
		qdrv_dump_hwreg_print_func(sets, "\n");
	}
}

static void qdrv_dump_hwreg_printk_output(struct qdrv_hwreg_print_data *sets)
{
	int len = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[sets->set_num]);
	int i;

	/*
	 * Index starts from 1 to allow qdrv_dump_hwreg_one_reg_output compatibility with seq files output.
	 */
	for (i = 1; i <= len; i++) {
		qdrv_dump_hwreg_one_reg_output(NULL, (void*)sets, i);
	}
}

#define QDRV_HWREGPRINT_MIN_ARGS_CNT 2
#define QDRV_HWREGPRINT_MAX_ARGS_CNT 3
static void qdrv_dump_hwregprint_one_group(struct seq_file *s, void *data, uint32_t grp_num)
{
	struct qdrv_hwreg_print_data *sets = (struct qdrv_hwreg_print_data*)data;

	if (sets != NULL) {
		sets->s = s;
	}

	/* Inverse counter because seq file iterator uses reverse direction */
	grp_num = QDRV_MAX_REG_MONITOR - grp_num;
	if (grp_num >= QDRV_MAX_REG_MONITOR) {
		printk("%s: seq file iterator: bad group number at %u\n", __func__, grp_num);
		return;
	}

	qdrv_dump_hwreg_print_func(sets, "Set %d (%s, 0x%08X->0x%08X)\n",
		grp_num, qdrv_hw_regs[grp_num].description,
		qdrv_hw_regs[grp_num].begin,
		qdrv_hw_regs[grp_num].end);
}

static void qdrv_dump_hwregprint_printk_output(struct qdrv_hwreg_print_data *sets)
{
	int grp_num;

	qdrv_dump_hwreg_print_func(sets, "\n");
	for (grp_num = QDRV_MAX_REG_MONITOR; grp_num > 0; --grp_num) {
		qdrv_dump_hwregprint_one_group(NULL, (void*)sets, grp_num);
	}
}

#define QDRV_HWREGCMP_MIN_ARGS_CNT 3
#define QDRV_HWREGCMP_MAX_ARGS_CNT 6
static void qdrv_dump_hwregcmp_one_reg(struct seq_file *s, void *data, uint32_t reg_num)
{
	struct qdrv_hwreg_print_data *sets = (struct qdrv_hwreg_print_data*)data;
	int set_num = sets->set_num,
	    buf_1_num = sets->buf1_num,
	    buf_2_num = sets->buf2_num,
	    reg_count;

	sets->s = s;
	if (!p_hw_reg_buf[set_num][buf_1_num] || !p_hw_reg_buf[set_num][buf_2_num]) {
		return;
	}

	reg_count = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[set_num]);
	/* Inverse counter because seq file iterator uses reverse direction */
	reg_num = reg_count - reg_num;
	if (reg_num > reg_count) {
		printk("%s: seq file iterator: bad register number %u\n", __func__, reg_num);
		return;
	}

	if (reg_num == 0) {
		qdrv_dump_hwreg_print_func(sets, "#  Mac reg set %s:\n",
				qdrv_hw_regs[sets->set_num].description);
	}

	if (p_hw_reg_buf[set_num][buf_1_num][reg_num] != p_hw_reg_buf[set_num][buf_2_num][reg_num]) {
		qdrv_dump_hwreg_print_func(sets, "0x%08X: %08X -> %08X\n",
			qdrv_hw_regs[set_num].begin + (reg_num * 4),
			p_hw_reg_buf[set_num][buf_1_num][reg_num],
			p_hw_reg_buf[set_num][buf_2_num][reg_num], NULL);
	} else if (sets->s != NULL) {
		qdrv_dump_hwreg_print_func(sets, "0x%08X: ===== %08X =====\n",
			qdrv_hw_regs[set_num].begin + (reg_num * 4),
			p_hw_reg_buf[set_num][buf_2_num][reg_num], NULL);
	}
}

static void qdrv_dump_hwregcmp_all(struct qdrv_hwreg_print_data *sets)
{
	register int reg_count, reg_num;
	reg_count = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[sets->set_num]);

	for (reg_num = 1; reg_num <= reg_count; reg_num++) {
		qdrv_dump_hwregcmp_one_reg(NULL, (void*)sets, reg_num);
	}
}

void qdrv_control_dump_active_hwreg(void)
{
	int i;

	for (i = 0; i < QDRV_MAX_REG_MONITOR; i++) {
		if (p_hw_reg_buf[i][0]) {
			/*
			 * If the buffer is allocated, do one final dump in the final index.
			 */
			qdrv_control_hwreg_trigger(i, QDRV_MAX_REG_PER_BUF - 1);
			printk("\nDump active registers for set %d (%s)\n",
					i, qdrv_hw_regs[i].description);
			qdrv_dump_hwregcmp_all(&(struct qdrv_hwreg_print_data){NULL,
					i, 0, QDRV_MAX_REG_PER_BUF - 1});
		}
	}
}

static void qdrv_dump_hwregcmp(int argc, char *argv[])
{
	static struct qdrv_hwreg_print_data sets = {NULL, 0, 1};
	int qdrvdata_output = 0;

	if (argc < QDRV_HWREGCMP_MIN_ARGS_CNT || argc > QDRV_HWREGCMP_MAX_ARGS_CNT) {
		printk("Bad arguments count\n");
		return;
	}

	sscanf(argv[2], "%u", &sets.set_num);
	if (sets.set_num > QDRV_MAX_REG_MONITOR - 1) {
		printk("Buffer set value out of range - should be between 0 and %lu\n",
				QDRV_MAX_REG_MONITOR - 1);
		return;
	}

	if (argc > 3) {
		if (strcmp("--qdrvdata", argv[argc - 1]) == 0) {
			qdrvdata_output = 1;
		}

		sscanf(argv[3], "%u", &sets.buf1_num);
		sscanf(argv[4], "%u", &sets.buf2_num);
	}

	if (sets.buf1_num > QDRV_MAX_REG_PER_BUF - 1 || sets.buf2_num > QDRV_MAX_REG_PER_BUF - 1) {
		printk("Invalid buffer index\n");
		return;
	}

	if (qdrvdata_output == 1) {
		int reg_count = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[sets.set_num]);
		qdrv_control_set_show(qdrv_dump_hwregcmp_one_reg, &sets, reg_count, 1);
	} else {
		qdrv_dump_hwregcmp_all(&sets);
	}
}

static void qdrv_dump_hwreg(int argc, char *argv[])
{
	static struct qdrv_hwreg_print_data sets = {NULL, 0, 0};
	int qdrvdata_output = 0;
	int verbose = 0;

	if (argc < QDRV_HWREG_MIN_ARGS_CNT || argc > QDRV_HWREG_MAX_ARGS_CNT) {
		printk("Bad arguments count\n");
		return;
	}

	sscanf(argv[2], "%u", &sets.set_num);
	if (sets.set_num > QDRV_MAX_REG_MONITOR - 1) {
		printk("Buffer value out of range - should be between 0 and %lu\n",
				QDRV_MAX_REG_MONITOR - 1);
		return;
	}

	sscanf(argv[3], "%u", &sets.buf1_num);
	if (sets.buf1_num > (QDRV_MAX_REG_PER_BUF - 1)) {
		printk("Buffer value out of range - should be between 0 and %d\n",
				QDRV_MAX_REG_PER_BUF - 1);
		return;
	}

	printk("[%d]Dump register set \"%s\"\n", sets.set_num, qdrv_hw_regs[sets.set_num].description);
	if (argc > QDRV_HWREG_MIN_ARGS_CNT) {
		int i;
		for (i = QDRV_HWREG_MIN_ARGS_CNT; i < argc; ++i) {
			if (strcmp("--qdrvdata", argv[i]) == 0) {
				qdrvdata_output = 1;
			} else if (strcmp("--verbose", argv[i]) == 0) {
				verbose = 1;
			} else {
				printk("Unknown option \"%s\"\n", argv[i]);
				return;
			}
		}
	}

	if (qdrvdata_output && !verbose)
		printk("Option --qdrvdata doesn't make sense without --verbose option\n");

	qdrv_control_hwreg_trigger(sets.set_num, sets.buf1_num);
	if (verbose) {
		if (qdrvdata_output) {
			int len = qdrv_control_hwreg_get_range_reg_count(&qdrv_hw_regs[sets.set_num]);
			qdrv_control_set_show(qdrv_dump_hwreg_one_reg_output, &sets, len, 1);
		} else {
			qdrv_dump_hwreg_printk_output(&sets);
		}
	}
}

static void qdrv_dump_hwregprint(int argc, char *argv[])
{
	static struct qdrv_hwreg_print_data sets = {NULL, 0, 0};
	int qdrvdata_output = 0;

	printk("\n");
	if (argc > QDRV_HWREGPRINT_MAX_ARGS_CNT) {
		printk("Bad arguments\n");
		return;
	}
	if (argc > QDRV_HWREGPRINT_MIN_ARGS_CNT && strcmp(argv[2], "--qdrvdata") == 0) {
		qdrvdata_output = 1;
	}
	if (qdrvdata_output) {
		qdrv_control_set_show(qdrv_dump_hwregprint_one_group, &sets, QDRV_MAX_REG_MONITOR, 1);
	} else {
		qdrv_dump_hwregprint_printk_output(&sets);
	}
}

static void qdrv_dump_irqstatus(void)
{
	uint32_t *ptr = ioremap_nocache(0xe6000320, 16);
	uint32_t val = *ptr;

	printf("BB IRQ status\n");
	printf("0xe6000320:%08x\n",val);
	printf("td_sigma_hw_noise:%x, radar:%x, rfc:%x, dleaf_overflow:%x, leaf_overflow:%x\n",
			val & 1,(val>>1)&0x3f,(val>>7)&3,(val>>9)&1,(val>>10)&1);
	printf("tx_td_overflow:%x, com_mem:%x, tx_td_underflow:%x, rfic:%x\n",
			(val>>11)&1,(val>>12)&0x1,(val>>13)&1,(val>>14)&1);
	printf("rx_sm_watchdog:%x, tx_sm_watchdog:%x, main_sm_watchdog:%x, hready_watchdog:%x\n",
			(val>>15)&1,(val>>16)&0x1,(val>>17)&1,(val>>18)&1);

	val = *(u32 *)((u32)ptr + 4);
	printf("0xe6000324:%08x\n",val);
	printf("rx_done:%x, rx_phase:%x, rx_start:%x, tx_done:%x, tx_phase:%x, tx_start:%x \n",
			(val>>0)&1,(val>>1)&0x1,(val>>2)&1,(val>>3)&1,(val>>4)&1,(val>>5)&1);
	iounmap(ptr);
}

static void qdrv_dump_txstatus(void)
{
	uint32_t *ptr  = ioremap_nocache(0xe5050000, 0x800);
	uint32_t *ptr1 = ioremap_nocache(0xe5040000, 0x200);
	uint32_t read_ptr;
	uint32_t temp;
	uint32_t i;

	/* For read and write pointers, the MSB is ignored */
	for (i = 0; i < 4; i++) {
		printk("Queue  %x, %x wr:%03x rd:%03x ",
				i, (u32)(0xe5040000 +  i * 32),
				*(u32 *)((u32)ptr + 0x308 + i * 16) + 0xe5030000,
				*(u32 *)((u32)ptr + 0x30c + i * 16) + 0xe5030000);
		temp = *(u32 *)((u32)ptr + 0x400 + i * 4);
		printk("level %d, wrptr:%x, rdptr:%x en:%x ",
				temp & 0xf, (temp >> 4) & 0xf, (temp >> 9) & 0x3, (temp >> 31) & 1);
		read_ptr = (temp >> 9) & 0x3;
		printk("frm:%08x %08x\n",
				*(u32 *)((u32)ptr1 + i * 32 + read_ptr * 8),
				*(u32 *)((u32)ptr1 + i * 32 + 4 + read_ptr * 8));
	}
	for (i = 0; i < 4; i++) {
		read_ptr = ((*(u32 *)((u32)ptr + 0x410 + i * 4) & 0xf) -
				((*(u32 *)((u32)ptr + 0x410 + i * 4) >> 26) & 0xf)) & 0xf;
		printk("Timer0 %x, %x wr:%03x rd:%03x ",
				i, (u32)(0xe5040000 + 0x80 + i * 32),
				*(u32 *)((u32)ptr + 0x348 + i * 16) + 0xe5030000,
				*(u32 *)((u32)ptr + 0x34c + i * 16) + 0xe5030000);
		temp = *(u32 *)((u32)ptr + 0x400 + i * 4);
		printk("level %d, wrptr:%x, rdptr:%x en:%x ",
				temp & 0xf, (temp >> 4) & 0xf, (temp >> 9) & 0x3, (temp >> 31) & 1);
		read_ptr = (temp >> 9) & 0x3;
		printk("frm:%08x %08x\n",
				*(u32 *)((u32)ptr1 + i * 32 + read_ptr * 8 + 0x80),
				*(u32 *)((u32)ptr1 + i * 32 + 4 + read_ptr * 8 + 0x80));
	}
	for (i = 0; i < 4; i++) {
		read_ptr =((*(u32 *)((u32)ptr + 0x420 + i * 4) & 0xf) -
				((*(u32 *)((u32)ptr + 0x420 + i * 4) >> 26) & 0xf)) & 0xf;
		printk("Timer1 %x, %x wr:%03x rd:%03x ",
				i, (u32)(0xe5040000 + 0x100 + i * 32),
				*(u32 *)((u32)ptr + 0x388 + i * 16) + 0xe5030000,
				*(u32 *)((u32)ptr + 0x38c + i * 16) + 0xe5030000);
		temp = *(u32 *)((u32)ptr + 0x400 + i * 4);
		printk("level %d, wrptr:%x, rdptr:%x en:%x ",
				temp & 0xf, (temp >> 4) & 0xf, (temp >> 9) & 0x3, (temp >> 31) & 1);
		read_ptr = (temp >> 9) & 0x3;
		printk("frm:%08x %08x\n",
				*(u32 *)((u32)ptr1 + i * 32 + read_ptr * 8 + 0x100),
				*(u32 *)((u32)ptr1 + i * 32 + 4 + read_ptr * 8 + 0x100));
	}
	iounmap(ptr);
	iounmap(ptr1);
}

static void qdrv_dump_event_log(void)
{
	char buffer[256] = {0};
	int i;

	for (i = 0; i < QDRV_EVENT_LOG_SIZE; i++) {
		if (qdrv_event_log_table[i].str == NULL) {
			break;
		}
		sprintf(buffer,qdrv_event_log_table[i].str,qdrv_event_log_table[i].arg1,
			qdrv_event_log_table[i].arg2,
			qdrv_event_log_table[i].arg3,
			qdrv_event_log_table[i].arg4,
			qdrv_event_log_table[i].arg5);
		printf("%08x:%08x %s\n",qdrv_event_log_table[i].jiffies,qdrv_event_log_table[i].clk,buffer);
	}
}

static void qdrv_dump_muc_log(struct device *dev)
{
		extern int qdrv_dump_log(struct qdrv_wlan *qw);
		struct qdrv_cb *qcb = dev_get_drvdata(dev);
		struct qdrv_wlan *qw = (struct qdrv_wlan *)qcb->macs[0].data;
		qdrv_dump_log(qw);
}

static void qdrv_dump_fctl(int argc, char *argv[])
{
	uint32_t *ptr = NULL;
	uint32_t *ptr1 = NULL;
	uint32_t temp = 0;
	char frame_type[10][12] = {"0 prestore","1 manage","2 data","3 A-MSDU","4-A-MPDU","5 beacon","6 probe","7 vht bf", "8 ht bf", "unknown"};

	sscanf(argv[2],"%x",&temp);
	if (temp == 0) {
		printk("invalid address!\n");
		return;
	}

	ptr = ioremap_nocache(temp, 32 * 4);
	if (ptr == NULL)
		return;

	ptr1 = ptr;
	temp = *ptr1++;
	printk("\nframe_type    %s",frame_type[min((temp>>4) & 0xf, (u32)9)]);
	printk("\nnot sounding: %8x",(temp >> 3) & 0x1 );
	printk("\trate table:   %8x",(temp >> 8) & 0x1 );
	printk("\tUse RA:       %8x",(temp >> 9) & 0x1 );
	printk("\tburst OK:     %8x",(temp >> 10) & 0x1 );
	printk("\nRIFS EN:      %8x",(temp >> 11) & 0x1 );
	printk("\tDur Upadate:  %8x",(temp >> 12) & 0x1 );
	printk("\tencrypt en:   %8x",(temp >> 13) & 0x1 );
	printk("\tignore cca:   %8x",(temp >> 14) & 0x1 );
	printk("\nignore nav:   %8x",(temp >> 15) & 0x1 );
	printk("\tmore frag:    %8x",(temp >> 16) & 0x1 );
	printk("\tnode en:      %8x",(temp >> 17) & 0x1 );
	printk("\tprefetch en:  %8x",(temp >> 18) & 0x1 );
	printk("\nint en:       %8x",(temp >> 19) & 0x1 );
	printk("\tfirst seq sel:%8x",(temp >> 21) & 0x1 );
	printk("\thdr len:      %8x",(temp >> 24) & 0xff );

	temp = *ptr1++;
	printk("\tframe Len:    %8x",(temp >> 0) & 0xffff );
	printk("\nprefectch Len:%8x",(temp >> 16) & 0xffff );
	temp = *ptr1++;
	printk("\tprefetch addr:%08x",temp);
	temp = *ptr1++;
	printk("\tDMA src addr: %08x",temp);
	temp = *ptr1++;
	printk("\tDMA next addr:%08x",temp);

	temp = *ptr1++;
	printk("\nnDMA Len:     %8x",(temp >> 0) & 0xffff );
	printk("\tRFTx Pwr0:    %8x",(temp >> 16) & 0xff );
	printk("\tRFTx Pwr0:    %8x",(temp >> 24) & 0xff );

	temp = *ptr1++;
	printk("\tAIFSN:        %8x  ",(temp >> 0) & 0xf );
	printk("\nECWmin:       %8x",(temp >> 4) & 0xf );
	printk("\tECWmax:       %8x",(temp >> 8) & 0xf );
	printk("\tNode Cache:   %8x",(temp >> 12) & 0x7f );
	printk("\tHW DMA:       %8x",(temp >> 19) & 0x1 );
	printk("\nFrm TimeoutEn:%8x",(temp >> 23) & 0x1 );
	printk("\tTXOP En:      %8x",(temp >> 24) & 0x1 );

	printk("\tA-MPDU Den:   %8x",(temp >> 25) & 0x7 );
	printk("\tSM Tone Grp:  %8x",(temp >> 29) & 0x7 );

	temp = *ptr1++;
	printk("\nFrm timeout:  %8x",temp);
	temp = *ptr1++;
	printk("\tTx Status:    %8x",temp);
	temp = *ptr1++;
	printk("\tRA[31:0]:     %8x",temp);
	temp = *ptr1++;
	printk("\tRA[47:32]:    %8x",(temp >> 0) & 0xffff );
	printk("\nRateRty 0:    %8x",0xe5040000 + ((temp >> 16) & 0xffff));
	temp = *ptr1++;
	printk("\tRateRty 1:    %8x",0xe5040000 + ((temp >> 0) & 0xffff ));
	printk("\tNxt Frm Len:  %8x",(temp >> 16) & 0xffff );

	temp = *ptr1++;
	printk("\tEDCA TxOpLim: %8x  ",(temp >> 0) & 0xff );
	printk("\nTxScale:      %8x",(temp >> 8) & 0x7 );
	printk("\tSmoothing:    %8x",(temp >> 19) & 0x1 );
	printk("\tNot Sounding: %8x",(temp >> 20) & 0x1 );
	printk("\tshort GI:     %8x",(temp >> 24) & 0x1 );
	temp = *ptr1++;
	printk("\nAntSelPtr:    %8x",(temp >> 0) & 0xffff );
	printk("\tFxdRateSeqPtr:%8x",(temp >> 16) & 0xffff );
	temp = *ptr1++;
	printk("\tNumSubFrames: %8x",(temp >> 0) & 0xffff );
	printk("\tTotalDenBytes:%8x",(temp >> 16) & 0xffff );

	temp = *ptr1++;
	printk("\nPN[31:0]:     %8x",temp);
	temp = *ptr1++;
	printk("\tPN[47:0]:     %8x",(temp >> 0) & 0xffff );
	printk("\tRxTxPwr 2:    %8x",(temp >> 16) & 0xff );
	printk("\tRxTxPwr 3:    %8x",(temp >> 24) & 0xff );

	temp = *ptr1++;
	printk("\nTxService:    %8x",(temp >> 0) & 0xffff );
	printk("\tLSIG Rsvd:    %8x",(temp >> 16) & 1 );
	printk("\tHTSIG Rsvd:   %8x",(temp >> 17) & 1 );

	printk("\n");
	iounmap(ptr);
}

static void qdrv_dump_rrt(int argc, char *argv[])
{
	uint32_t *ptr = NULL;
	uint32_t *ptr1 = NULL;
	uint32_t temp = 0;
	int not_done = 4;

	sscanf(argv[2],"%x",&temp);
	if (temp == 0) {
		printk("invalid address!\n");
		return;
	}

	ptr = ioremap_nocache(temp, 32 * 4);
	if (ptr == NULL)
		return;

	ptr1 = ptr;
	do {
		temp = *ptr1++;
		printk("\nRateInd:      %8x",(temp >> 0) & 0x7f );
		printk("\tLongPre:      %8x",(temp >> 7) & 0x1 );
		printk("\t11n:          %8x",(temp >> 8) & 0x1 );
		printk("\tBW:           %8x",(temp >> 9) & 0x3 );
		printk("\nChOff:        %8x",(temp >> 11) & 0x3 );
		printk("\tNEss:         %8x",(temp >> 13) & 0x3 );
		printk("\tShortGI:      %8x",(temp >> 15) & 0x1 );
		printk("\tCount:        %8x",(temp >> 16) & 0xf );
		printk("\nAntSet:       %8x",(temp >> 20) & 0xf );
		printk("\tAntSetOn:     %8x",(temp >> 24) & 0x1 );
		printk("\tNTx:          %8x",(temp >> 25) & 0x3 );
		printk("\tSTBC:         %8x",(temp >> 27) & 0x3 );
		printk("\nExpMatType:   %8x",(temp >> 29) & 0x7 );
		temp = *ptr1++;
		printk("\tSeqPtr:       %8x",(temp >> 0) & 0x7fff);
		printk("\tExpMatPtr:    %8x",(temp >> 16) & 0x3f);
		printk("\tLDPC:         %8x",(temp >> 26) & 0x1);
		printk("\nLDPCAdj:      %8x",(temp >> 27) & 0x1);
		printk("\tShiftVal:     %8x",(temp >> 28) & 0x7);
		printk("\tLastEntry:    %8x",(temp >> 31) & 0x1);
		printk("\t11ac:         %8x",(temp >> 15) & 0x1);
		not_done--;
	} while (((temp & 0x80000000)==0) && not_done);

	printk("\n");
	iounmap(ptr);
}

static void qdrv_dump_mem(int argc, char *argv[])
{
	uint32_t *ptr;
	int num_bytes = 256;
	uint32_t addr;
	int i;

	if (argc < 3) {
		printk("dump mem <addr> [num bytes]\n");
		return;
	}

	sscanf(argv[2],"%x",&addr);

	// if ddr addr, add the ddr bit otherwise assume addr
	// is correct (i.e. user has to correct for sram addr
	addr &= 0xfffffffc;
	if (addr < 0x80000000) {
		addr |= 0x80000000;
	}

	if (!qdrv_command_is_valid_addr(addr)) {
		printk("invalid address\n");
		return;
	}

	if (argc >= 4) {
		sscanf(argv[3],"%x",&num_bytes);
	}
	ptr = ioremap_nocache(addr, num_bytes);
	if (!ptr) {
		printk("remapping failed\n");
	} else {
		for (i = 0; i < num_bytes; i += 4) {
			if ((i%16) == 0) {
				printk("\n%08x: ",addr + i);
			}
			printk("%08x ",*(u32 *)((int)ptr + i));
		}
		printk("\n");
		iounmap(ptr);
	}
}

static void qdrv_dump_dma(int argc, char *argv[])
{
	uint32_t *ptr = NULL;
	int addr;

	if (argc < 3) {
		printk("dump dma <addr>\n");
		return;
	}

	sscanf(argv[2], "%x", &addr);
	addr &= 0xfffffffc;

	while (addr != 0) {
		uint8_t *data_ptr;
		uint32_t src;
		int i;

		// convert from muc addr
		if (addr < 0x80000000) {
			addr |= 0x80000000;
		} else {
			addr |= 0x08000000;
		}

		ptr = ioremap_nocache(addr, 4 * 4);
		if (ptr == NULL) {
			printk("invalid address!\n");
			return;
		}

		printk("\nsrc:   %08x",ptr[0]);
		printk(" dst:   %08x",ptr[1]);
		printk(" next:  %08x",ptr[2]);
		printk(" len:   %08x",ptr[3]&0xffff);
		addr = ptr[2];
		src = ptr[0];
		iounmap(ptr);
		if (src < 0x80000000) {
			src |= 0x80000000;
		} else {
			src |= 0x08000000;
		}

		data_ptr = ioremap_nocache(src, 8);
		if (data_ptr == NULL) {
			printk("invalid data_ptr!\n");
			return;
		}

		for (i = 0; i < 8; i++) {
			printf(" %02x", data_ptr[i]);
		}

		printk("\n");
		iounmap(data_ptr);
	}
}

static void qdrv_dump_usage(void)
{
	printk("usage: dump irqstatus\n");
	printk("usage: dump txstatus\n");
	printk("usage: dump log\n");
	printk("usage: dump muc\n");
	printk("usage: dump fctl\n");
	printk("usage: dump rrt\n");
	printk("usage: dump mem <addr> [num bytes]\n");
	printk("usage: dump dma <addr>\n");
	printk("usage: dump hwregprint [--qdrvdata]\n");
	printk("usage: dump hwreg <reg set: 0-%lu> <buf num: 0-%d> [--verbose [--qdrvdata]]\n",
		QDRV_MAX_REG_MONITOR - 1, QDRV_MAX_REG_PER_BUF - 1);
	printk("usage: dump hwregcmp <set: 0-%lu> [<buf 1: 0-%d> <buf 2: 0-%d>] [--qdrvdata]\n",
		QDRV_MAX_REG_MONITOR - 1, QDRV_MAX_REG_PER_BUF - 1, QDRV_MAX_REG_PER_BUF - 1);
#if QTN_SEM_TRACE
	printk("usage: dump sem\n");
#endif
}

static int qdrv_command_dump(struct device *dev, int argc, char *argv[])
{
	if (argc < 2) {
		qdrv_dump_usage();
		return 0;
	}

	if (strcmp(argv[1], "hwregprint") == 0) {
		qdrv_dump_hwregprint(argc, argv);
	} else if (strcmp(argv[1], "hwreg") == 0) {
		qdrv_dump_hwreg(argc, argv);
	} else if (strcmp(argv[1], "hwregcmp") == 0) {
		qdrv_dump_hwregcmp(argc, argv);
	} else if (strcmp(argv[1], "irqstatus") == 0) {
		qdrv_dump_irqstatus();
	} else if (strcmp(argv[1],"txstatus") == 0) {
		qdrv_dump_txstatus();
	} else if (strcmp(argv[1],"log") == 0) {
		qdrv_dump_event_log();
	} else if (strcmp(argv[1],"muc") == 0) {
		qdrv_dump_muc_log(dev);
	} else if (strcmp(argv[1],"fctl") == 0) {
		qdrv_dump_fctl(argc, argv);
	} else if (strcmp(argv[1],"rrt") == 0) {
		qdrv_dump_rrt(argc, argv);
	} else if (strcmp(argv[1],"mem") == 0) {
		qdrv_dump_mem(argc, argv);
	} else if (strcmp(argv[1],"dma") == 0) {
		qdrv_dump_dma(argc, argv);
#if QTN_SEM_TRACE
	} else if (strcmp(argv[1],"sem") == 0) {
		qtn_mproc_sync_spin_lock_log_dump();
#endif
	} else {
		printk("%s: invalid dump type %s\n", __func__, argv[1]);
	}

	return 0;
}

static int qdrv_command_radar(struct device *dev, int argc, char *argv[])
{
	if ((3 <= argc) && (strcmp(argv[1], "enable")==0)) {
		qdrv_radar_enable(argv[2]);
	} else if ((2 <= argc) && (strcmp(argv[1], "disable")==0)) {
		qdrv_radar_disable();
	} else {
		goto usage;
	}

	return 0;

usage:
	printk("usage: %s (enable <region>|disable)\n", argv[0]);
	return 0;
}

static int qdrv_command_rifs(struct device *dev, int argc, char *argv[])
{
	if (argc == 2 && strcmp(argv[1], "enable") == 0) {
		qtn_rifs_mode_enable(QTN_LHOST_SOC_CPU);
	} else if (argc == 2 && strcmp(argv[1], "disable") == 0) {
		qtn_rifs_mode_disable(QTN_LHOST_SOC_CPU);
	} else {
		printk("usage: %s (enable|disable)\n", argv[0]);
	}

	return 0;
}

static int qdrv_command_led (struct device *dev, int argc, char *argv[]){
	int ret_val, data1, data2;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Check that we have all the arguments */
	if(argc != 3) {
		goto error;
	}

	if(sscanf(argv[1], "%d", &data1) != 1) {
		goto error;
	}

	if(sscanf(argv[2], "%d", &data2) != 1) {
		goto error;
	}

	ret_val = set_led_data(LED_FILE, data1, data2);
	if (ret_val > 1){
		printk("Led GPIO already set.\n");
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return (0);

error:
	DBGPRINTF_E("Invalid arguments to led command\n");

	return(-1);
}

/*
 * GPIO programs - separate from the LED programs
 */

static u8	gpio_wps_push_button = 255;
static u8	wps_push_button_active_logic = 0;
static u8	wps_push_button_interrupts = 0;
static u8	wps_push_button_configured = 0;
static u8	wps_push_button_enabled = 0;

int
qdrv_get_wps_push_button_config( u8 *p_gpio_pin, u8 *p_use_interrupt, u8 *p_active_logic )
{
	int retval = 0;

	if (wps_push_button_configured != 0) {
		*p_gpio_pin = gpio_wps_push_button;
		*p_use_interrupt = wps_push_button_interrupts;
		*p_active_logic = (wps_push_button_active_logic == 0) ? 0 : 1;
	} else {
		retval = -1;
	}

	return( retval );
}

void
set_wps_push_button_enabled( void )
{
	wps_push_button_enabled = 1;
}

static int
qdrv_command_gpio(struct device *dev, int argc, char *argv[])
{
	int		retval = 0;
	u8		gpio_pin = 0;
	unsigned int	tmp_uval = 0;
	int		wps_flag = 0;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (argc < 3)
	{
		DBGPRINTF_E("Not enough arguments to gpio command\n");
		return(-1);
	}
	else
	{
		if (strcmp( argv[ 2 ], "wps" ) == 0)
		  wps_flag = 1;
		else if (sscanf(argv[ 2 ] , "%u", &tmp_uval) != 1)
		{
			goto bad_args;
		}

		gpio_pin = (u8) tmp_uval;
	}

	if (strcmp( argv[ 1 ], "get" ) == 0)
	{
		if (wps_flag == 0)
		{
			retval = -1;
		}
	  /*
	   * For WPS, just report thru printk.
	   * No reporting thru /proc/qdrvdata (qdrv_control_set_show, etc.)
	   */
		else
		{
			if (wps_push_button_configured)
			{
				printk( "WPS push button accessed using GPIO pin %u\n", gpio_wps_push_button );
				printk( "monitored using %s\n", wps_push_button_interrupts ? "interrupt" : "polling" );
			}
			else
			{
				printk( "WPS push button not configured\n" );
			}
		}
	}
	else if (strcmp( argv[ 1 ], "set" ) == 0)
	{
		if (argc < 4)
		{
			DBGPRINTF_E("Not enough arguments for gpio set command\n");
			retval = -1;
		}
		else if (wps_flag == 0)
		{
			retval = -1;
		}
		/*
		 * For WPS, we have "gpio set wps 4" and "gpio set wps 4 intr".
		 * Latter selects interrupt-based monitoring.
		 */
		else if (wps_push_button_enabled == 0)
		{
			unsigned int	tmp_uval_2 = 0;

			if (sscanf(argv[ 3 ] , "%u", &tmp_uval) != 1)
			{
				goto bad_args;
			}

			wps_push_button_interrupts = 0;
			wps_push_button_active_logic = 1;

			if (argc > 4)
			{
				if (strcmp( argv[ 4 ], "intr" ) == 0)
				{
					wps_push_button_interrupts = 1;
					wps_push_button_active_logic = 1;
				}
				else if (sscanf(argv[ 4 ], "%u", &tmp_uval_2 ) == 1)
				{
					wps_push_button_active_logic = (u8) tmp_uval_2;
				}
			}

			if ((wps_push_button_interrupts && tmp_uval > MAX_GPIO_INTR) ||
			    (tmp_uval > MAX_GPIO_PIN))
			{
				printk( "GPIO pin number %u out of range, maximum is %d\n", tmp_uval,
					 wps_push_button_interrupts ? MAX_GPIO_INTR : MAX_GPIO_PIN );
				goto bad_args;
			}
			else
			{
				gpio_wps_push_button = (u8) tmp_uval;
				wps_push_button_configured = 1;
			}
		}
		else
		{
			DBGPRINTF_E("WPS Push button enabled, cannot (re)configure.\n");
		}
	}
	else
	{
		DBGPRINTF_E("Unrecognized gpio subcommand %s\n", argv[1]);
		retval = -1;
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return( retval );

bad_args:
	DBGPRINTF_E("Invalid argument(s) to gpio command\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(-1);
}

static int
qdrv_command_pwm(struct device *dev, int argc, char *argv[])
{
	int retval = 0;
	int pin = 0;
	int high_count = 0;
	int low_count = 0;

	if (argc < 3)
		goto bad_args;
	if (sscanf(argv[2], "%d", &pin) != 1)
		goto bad_args;

	if (strcmp(argv[1], "enable") == 0) {
		if (argc < 5)
			goto bad_args;
		if (sscanf(argv[3], "%d", &high_count) != 1)
			goto bad_args;
		if (sscanf(argv[4], "%d", &low_count) != 1)
			goto bad_args;
		retval = gpio_enable_pwm(pin, high_count - 1, low_count - 1);
	} else if (strcmp(argv[1], "disable") == 0) {
		retval = gpio_disable_pwm(pin);
	} else {
		goto bad_args;
	}

	return ( retval );

bad_args:
	DBGPRINTF_E("Invalid argument(s) to pwm command\n");
	DBGPRINTF_E("usage: %s (enable|disable) <pin> <high_count> <low_count>\n", argv[0]);

	return (-1);
}

static void
qdrv_calcmd_show_packet_counts( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_packet_report	*p_packet_report = (struct qdrv_packet_report *) data;

	seq_printf( s, "RF1_TX = %d, RF2_TX = %d\n", p_packet_report->rf1.num_tx, p_packet_report->rf2.num_tx );
	seq_printf( s, "RF1_RX = %d, RF2_RX = %d\n", p_packet_report->rf1.num_rx, p_packet_report->rf2.num_rx );
}

static void
qdrv_calcmd_show_tx_power( struct seq_file *s, void *data, u32 num )
{
	unsigned int *p_data = (unsigned int *) data;

	seq_printf( s, "%u %u %u %u\n", p_data[0], p_data[1], p_data[2], p_data[3] );
}

static void
qdrv_calcmd_show_rssi( struct seq_file *s, void *data, u32 num )
{
	unsigned int *p_data = (unsigned int *) data;

	seq_printf( s, "%d %d %d %d\n", p_data[0], p_data[1], p_data[2], p_data[3] );
}

static void
qdrv_calcmd_show_test_mode_param( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_cal_test_setting *cal_test_setting= (struct qdrv_cal_test_setting*) data;

	seq_printf( s, "%d %d %d %d %d %d\n",
			cal_test_setting->antenna,
			cal_test_setting->mcs,
			cal_test_setting->bw_set,
			cal_test_setting->pkt_len,
			cal_test_setting->is_eleven_N,
			cal_test_setting->bf_factor_set
		);
}

static void
qdrv_calcmd_show_vpd( struct seq_file *s, void *data, u32 num )
{
	unsigned int *p_data = (unsigned int *) data;

	seq_printf( s, "%d\n", p_data[0] );
}

static void
qdrv_calcmd_show_temperature( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_cb *qcb = (struct qdrv_cb *) data;
	seq_printf( s, "%d %d\n", qcb->temperature_rfic_external, qcb->temperature_rfic_internal);
}

/* RF register value is now returned from the MuC */

static void
qdrv_show_rfmem(struct seq_file *s, void *data, u32 num)
{
	struct qdrv_cb *qcb = (struct qdrv_cb *) data;

	seq_printf(s, "mem[0x%08x] = 0x%08x\n", qcb->read_addr, qcb->rf_reg_val );
}

#ifdef POST_RF_LOOP
static void
qdrv_calcmd_post_rfloop_show(struct seq_file *s, void *data, u32 num)
{
        int *p_data = (int *)data;

        seq_printf(s, "%d\n", *p_data);
}
#endif

char dc_iq_calfile_version[VERSION_SIZE];
char power_calfile_version[VERSION_SIZE];

/*
 * Calcmd format:
 *
 * Each calcmd is a sequence of U8's; thus each element is in the range 0 - 255.
 *
 * The first element identifies the calcmd.  Look in macfw/cal/utils/common/calcmd.h for symbolic
 * enums.  Example values include SET_TEST_MODE and GET_TEST_STATS.  Symbolic values are not used in
 * the Q driver.
 *
 * Second element is required to be 0.
 *
 * Third element is the total length of the calcmd sequence.  If no additional argument are present,
 * this element will be 4.  For each argument, add 2 to this element.
 *
 * Fourth element is also required to be 0.
 *
 * Remaining elements are the arguments, organized as pairs.  First element in each pair is the
 * argument index, numbered starting from 1 - NOT 0.  Second element is the argument value.  Thus
 * ALL calcmd input arguments (output arguments are also possible) are required to be 8 bit values.
 * Larger values (e.g. U16 or U32) have to be passed in 8-bit pieces, with Linux taking the value
 * apart and the MuC reassembling then.
 *
 * Output arguments are returned in the same buffer used to send the calcmd to the MuC.  See
 * GET_TEST_STATS (calcmd = 15) for an example.
 */

static int qdrv_command_calcmd(struct device *dev, int argc, char *argv[])
{
	struct qdrv_cb *qcb;
	char *cmd = NULL;
	dma_addr_t cmd_dma;
	struct qdrv_wlan *qw;
	int cmdlen;

	int temp_calcmd[30] = {0};
	char calcmd[30] = {0};
	int i;
	int evm_int[4] = {0}, evm_frac[4] = {0};

	u32 num_rf1_rx;
	u32 num_rf1_tx;
	u32 num_rf2_rx;
	u32 num_rf2_tx;

	qcb = dev_get_drvdata(dev);
	qw = qdrv_control_wlan_get(&qcb->macs[0]);
	if (!qw) {
		return -1;
	}

	cmd = qdrv_hostlink_alloc_coherent(NULL, sizeof(qcb->command), &cmd_dma, GFP_ATOMIC);
	if (cmd == NULL) {
		DBGPRINTF_E("Failed allocate %ld bytes for cmd\n", sizeof(qcb->command));
		return -1;
	}

	cmdlen = argc - 1;

	for (i = 1; i < argc; i++) {
		temp_calcmd[i-1] = _atoi(argv[i]);
		calcmd[i-1] = (char)temp_calcmd[i-1];
	}

	DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CALCMD, "cmdlen %d\n", cmdlen);
	memcpy(cmd, calcmd, cmdlen);

	qdrv_hostlink_msg_calcmd(qw, cmdlen, cmd_dma);

	if(cmd[0] == 31) {
		sprintf(dc_iq_calfile_version, "V%d.%d", cmd[6], cmd[5]);
		sprintf(power_calfile_version, "V%d.%d", cmd[9], cmd[8]);
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_CALCMD,
				"Calibration version %d.%d\n", cmd[12], cmd[11]);
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_CALCMD,
				"RFIC version %d.%d\n", cmd[15], cmd[14]);
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_CALCMD,
				"BBIC version %d.%d\n", cmd[18], cmd[17]);

	} else if(cmd[0] == 28 || cmd[0] == 29 || cmd[0] == 30) {
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD, ".");

	} else if(cmd[0] == 15) {
		num_rf1_rx = cmd[8] << 24 | cmd[7] << 16 | cmd[6] << 8 | cmd[5];
		num_rf1_tx = cmd[13] << 24 | cmd[12] << 16 | cmd[11] << 8 | cmd[10];
		num_rf2_rx = cmd[18] << 24 | cmd[17] << 16 | cmd[16] << 8 | cmd[15];
		num_rf2_tx = cmd[23] << 24 | cmd[22] << 16 | cmd[21] << 8 | cmd[20];
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"MuC: RF1_TX = %d, RF2_TX = %d\n", num_rf1_tx, num_rf2_tx);
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"MuC: RF1_RX = %d, RF2_RX = %d\n", num_rf1_rx, num_rf2_rx);

		qcb->packet_report.rf1.num_tx = num_rf1_tx;
		qcb->packet_report.rf2.num_tx = num_rf2_tx;

		if (chip_id() == 0x20) {
			qcb->packet_report.rf1.num_rx += num_rf1_rx;
			qcb->packet_report.rf2.num_rx += num_rf2_rx;
		} else {
			qcb->packet_report.rf1.num_rx = num_rf1_rx;
			qcb->packet_report.rf2.num_rx = num_rf2_rx;
		}

		qdrv_control_set_show(qdrv_calcmd_show_packet_counts, (void *) &(qcb->packet_report), 1, 1);

	} else if(cmd[0] == 3) {
		int temp_cal_13_W = 0, temp_cal_13_I, temp_cal_13_P;
		u32 rfic_temp_int;
		u32 rfic_temp_frac;
		u32 flag = (cmd[8] << 24 | cmd[7] << 16 | cmd[6] << 8 | cmd[5]);
		u32 rfic_temp = (cmd[28] << 24 | cmd[27] << 16 | cmd[26] << 8 | cmd[25]);

		qtn_tsensor_get_temperature(qw->se95_temp_sensor, &temp_cal_13_W);
		temp_cal_13_I = (int) (temp_cal_13_W / QDRV_TEMPSENS_COEFF);
		temp_cal_13_P = ABS((temp_cal_13_W - (temp_cal_13_I * QDRV_TEMPSENS_COEFF)));
		rfic_temp_int = rfic_temp / QDRV_TEMPSENS_COEFF10;
		rfic_temp_frac = rfic_temp / QDRV_TEMPSENS_COEFF -
				 (rfic_temp_int * (QDRV_TEMPSENS_COEFF10/QDRV_TEMPSENS_COEFF));

		if (flag == EXT_TEMPERATURE_SENSOR_REPORT_FLAG) {
			DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
					"Gain = %d, %d, %d, %d\n",
					(cmd[15] | cmd[16] << 8), (cmd[17] | cmd[18] << 8),
					(cmd[20] | cmd[21] << 8), (cmd[22] | cmd[23] << 8));

		} else if (flag == DISABLE_REPORT_FLAG) {
			DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD, "Power compensation is Disabled\n");
		} else {
			DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
					"(RF,BB) = (%d, %d), (%d, %d), (%d, %d), (%d, %d)\n",
					cmd[5], cmd[10], cmd[6], cmd[11],
					cmd[7], cmd[12], cmd[8], cmd[13]);
			DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
					" Voltage = %d, %d, %d, %d\n",
					(cmd[15] | cmd[16] << 8), (cmd[17] | cmd[18] << 8),
					(cmd[20] | cmd[21] << 8), (cmd[22] | cmd[23] << 8));
		}
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"TEMPERATURE_RFIC_EXTERNAL= %d.%d\n",
				temp_cal_13_I, temp_cal_13_P);
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"TEMPERATURE_RFIC_INTERNAL = %d.%d\n",
				rfic_temp_int, rfic_temp_frac); /* Please do not delete for future use */
		qcb->temperature_rfic_external = temp_cal_13_W;
		qcb->temperature_rfic_internal = rfic_temp;
		qdrv_control_set_show(qdrv_calcmd_show_temperature, (void *)qcb, 1, 1);

	} else if (cmd[0] == 12)			/* SET_TEST_MODE */ {
		qcb->packet_report.rf1.num_tx = 0;
		qcb->packet_report.rf1.num_rx = 0;
		qcb->packet_report.rf2.num_tx = 0;
		qcb->packet_report.rf2.num_rx = 0;

	} else if (cmd[0] == 33)		/* GET_RFIC_REG */ {
		u32	register_value = cmd[8] << 24 | cmd[7] << 16 | cmd[6] << 8 | cmd[5];
		u32	register_address = cmd[13] << 24 | cmd[12] << 16 | cmd[11] << 8 | cmd[10];

		qcb->read_addr = register_address;
		qcb->rf_reg_val = register_value;

		qdrv_control_set_show(qdrv_show_rfmem, (void *) qcb, 1, 1);

	} else if(cmd[0] == 41) {
		u8 mcs;
		u16 rx_gain;
		u16 evm[4];
		u16 num_rx_sym;

		u8 bw, nsts, format, rssi_flag;
		int16_t rssi[4];

		mcs = cmd[5];
		rx_gain = cmd[8] << 8 | cmd[7];
		num_rx_sym = cmd[21] << 8 | cmd[20];

		bw = cmd[23];
		nsts = cmd[25];
		format = cmd[27];
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"MCS = %d, RX SYMBOL NUM = %d, NSTS = %d, BW = %d, FORMAT = %d  _\n",
				mcs, num_rx_sym, nsts, bw, format);

		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"RX_GAIN = %d  __ (0x%x)\n", rx_gain, rx_gain);

		rssi_flag = cmd[29];
		if(rssi_flag > 0)	//rssi_flag is 0 for EVM measurement
		{
			rssi[0] = (int16_t)(cmd[11] << 8 | cmd[10]);
			rssi[1] = (int16_t)(cmd[13] << 8 | cmd[12]);
			rssi[2] = (int16_t)(cmd[16] << 8 | cmd[15]);
			rssi[3] = (int16_t)(cmd[18] << 8 | cmd[17]);
			if(rssi_flag == 1)
			DBGPRINTF(DBG_LL_HIDDEN, QDRV_LF_CALCMD,
				"RX_RSSI (dBFS) : %d.%d, %d.%d, %d.%d, %d.%d\n",
				rssi[0] / 10, ABS(rssi[0]) % 10 , rssi[1] / 10, ABS(rssi[1]) % 10,
				rssi[2] / 10, ABS(rssi[2]) % 10 , rssi[3] / 10, ABS(rssi[3]) % 10);
			else
			DBGPRINTF(DBG_LL_HIDDEN, QDRV_LF_CALCMD,
				"RX_RSSI (dBm) : %d.%d, %d.%d, %d.%d, %d.%d\n",
				rssi[0] / 10, ABS(rssi[0]) % 10 , rssi[1] / 10, ABS(rssi[1]) % 10,
				rssi[2] / 10, ABS(rssi[2]) % 10 , rssi[3] / 10, ABS(rssi[3]) % 10);
		}
		else
		{
			evm[0] = cmd[11] << 8 | cmd[10];
			evm[1] = cmd[13] << 8 | cmd[12];
			evm[2] = cmd[16] << 8 | cmd[15];
			evm[3] = cmd[18] << 8 | cmd[17];
			if (evm[0] > 0) convert_evm_db(evm[0], num_rx_sym,  &evm_int[0], &evm_frac[0]);
			if (evm[1] > 0) convert_evm_db(evm[1], num_rx_sym,  &evm_int[1], &evm_frac[1]);
			if (evm[2] > 0) convert_evm_db(evm[2], num_rx_sym,  &evm_int[2], &evm_frac[2]);
			if (evm[3] > 0) convert_evm_db(evm[3], num_rx_sym,  &evm_int[3], &evm_frac[3]);

			DBGPRINTF(DBG_LL_HIDDEN, QDRV_LF_CALCMD,
				"RX_EVM[0] = %d.%d  RX_EVM[1] = %d.%d  RX_EVM[2] = %d.%d  RX_EVM[3] = %d.%d \n",
				evm_int[0], evm_frac[0], evm_int[1], evm_frac[1],
				evm_int[2], evm_frac[2], evm_int[3], evm_frac[3]);
		}
	} else if(cmd[0] == 48) {
		s16 pd_vol0_reading = cmd[6] << 8 | cmd[5];
		if (pd_vol0_reading > 511) pd_vol0_reading -= 1024;
		s16 pd_vol1_reading = cmd[9] << 8 | cmd[8];
		if (pd_vol1_reading > 511) pd_vol1_reading -= 1024;
		s16 pd_vol2_reading = cmd[12] << 8 | cmd[11];
		if (pd_vol2_reading > 511) pd_vol2_reading -= 1024;
		s16 pd_vol3_reading = cmd[15] << 8 | cmd[14];
		if (pd_vol3_reading > 511) pd_vol3_reading -= 1024;
		s16 rfic_temp_reading = cmd[18] << 8 | cmd[17]; //need to check specs of RFIC4 to find out dynamic range temp sensor
		u8 pd_dBm0 = cmd[20];
		u8 pd_dBm1 = cmd[22];
		u8 pd_dBm2 = cmd[24];
		u8 pd_dBm3 = cmd[26];
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"OUT PD_LEVEL : (%d, %d, %d, %d) / RFIC_TEMP = %d oC\n",
				pd_vol0_reading, pd_vol1_reading, pd_vol2_reading,
				pd_vol3_reading, rfic_temp_reading);
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"OUT PD_POWER : %d.%ddBm, %d.%ddBm, %d.%ddBm, %d.%ddBm\n",
				pd_dBm0 >> 2, (pd_dBm0 % 4) * 25,
				pd_dBm1 >> 2, (pd_dBm1 % 4) * 25,
				pd_dBm2 >> 2, (pd_dBm2 % 4) * 25,
				pd_dBm3 >> 2, (pd_dBm3 % 4) * 25);

	} else if (cmd[0] == 51) {
		u16 pd_vol0 = cmd[6] << 8 | cmd[5];
		u16 pd_vol1 = cmd[9] << 8 | cmd[8];
		u16 pd_vol2 = cmd[12] << 8 | cmd[11];
		u16 pd_vol3 = cmd[15] << 8 | cmd[14];
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"BASE PD_POWER : %d.%ddBm, %d.%ddBm, %d.%ddBm, %d.%ddBm\n",
				pd_vol0 >> 2, (pd_vol0 % 4) * 25,
				pd_vol1 >> 2, (pd_vol1 % 4) * 25,
				pd_vol2 >> 2, (pd_vol2 % 4) * 25,
				pd_vol3 >> 2, (pd_vol3 % 4) * 25);

		qcb->qdrv_cal_test_report.tx_power[0] = pd_vol0;
		qcb->qdrv_cal_test_report.tx_power[1] = pd_vol1;
		qcb->qdrv_cal_test_report.tx_power[2] = pd_vol2;
		qcb->qdrv_cal_test_report.tx_power[3] = pd_vol3;
		qdrv_control_set_show(qdrv_calcmd_show_tx_power, (void *) &(qcb->qdrv_cal_test_report.tx_power[0]), 1, 1);
	}

	else if(cmd[0] == 54)
	{
		int rssi[4];

		rssi[0] = cmd[8] << 24 | cmd[7] << 16 | cmd[6] << 8 | cmd[5];
		rssi[0] = ((rssi[0] > 0xFFF) ? 0xFFF : rssi[0]);
		rssi[1] = cmd[13] << 24 | cmd[12] << 16 | cmd[11] << 8 | cmd[10];
		rssi[1] = ((rssi[1] > 0xFFF) ? 0xFFF : rssi[1]);
		rssi[2] = cmd[18] << 24 | cmd[17] << 16 | cmd[16] << 8 | cmd[15];
		rssi[2] = ((rssi[2] > 0xFFF) ? 0xFFF : rssi[2]);
		rssi[3] = cmd[23] << 24 | cmd[22] << 16 | cmd[21] << 8 | cmd[20];
		rssi[3] = ((rssi[3] > 0xFFF) ? 0xFFF : rssi[3]);

		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"RSSI (dBm) : %d.%d, %d.%d, %d.%d, %d.%d\n",
				rssi[0] / 10, ABS(rssi[0]) % 10, rssi[1] / 10, ABS(rssi[1]) % 10,
				rssi[2] / 10, ABS(rssi[2]) % 10, rssi[3] / 10, ABS(rssi[3]) % 10);
		qcb->qdrv_cal_test_report.rssi[0] = rssi[0];
		qcb->qdrv_cal_test_report.rssi[1] = rssi[1];
		qcb->qdrv_cal_test_report.rssi[2] = rssi[2];
		qcb->qdrv_cal_test_report.rssi[3] = rssi[3];
		qdrv_control_set_show(qdrv_calcmd_show_rssi, (void *) &(qcb->qdrv_cal_test_report.rssi[0]), 1, 1);
	}

	else if (cmd[0] == 56) {
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"Get Test Mode = Ant_Sel: %d, MCS: %d, BW: %d, Pkt_Len: %d, Protocol: %d, BF: %d\n",
				cmd[5], cmd[7], cmd[9], cmd[11], cmd[13], cmd[15]);

		qcb->qdrv_cal_test_report.setting.antenna = cmd[5];
		qcb->qdrv_cal_test_report.setting.mcs = cmd[7];
		qcb->qdrv_cal_test_report.setting.bw_set = cmd[9];
		qcb->qdrv_cal_test_report.setting.pkt_len = cmd[11];
		qcb->qdrv_cal_test_report.setting.is_eleven_N = cmd[13];
		qcb->qdrv_cal_test_report.setting.bf_factor_set = cmd[15];
		qdrv_control_set_show(qdrv_calcmd_show_test_mode_param, (void *) &qcb->qdrv_cal_test_report.setting, 1, 1);
	}
#ifdef POST_RF_LOOP
	else if (cmd[0] == 60) {
		qcb->qdrv_cal_test_report.post_rfloop_success = cmd[5];
		qdrv_control_set_show(qdrv_calcmd_post_rfloop_show, (void *)&qcb->qdrv_cal_test_report.post_rfloop_success, 1, 1);
	}
#endif
	else if (cmd[0] == 62) {
		DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD, "Calstate TX Power = %d\n", cmd[5]);
		qcb->calstate_vpd = cmd[5];
		qdrv_control_set_show(qdrv_calcmd_show_vpd, (void *) &qcb->calstate_vpd, 1, 1);

	}
	else if (cmd[0] == 63) {
		/* Rx IQ cal cmd: print failed status*/
		if(cmd[5] > 0)
			DBGPRINTF_RAW(DBG_LL_INFO, QDRV_LF_CALCMD,
				"qdrv ERROR: cmd id 63 failed: status= %d\n", cmd[5]);
	}

	qdrv_hostlink_free_coherent(NULL, sizeof(qcb->command), cmd, cmd_dma);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return(0);
}

int qdrv_command_read_rf_reg(struct device *dev, int offset)
{
	struct qdrv_cb *qcb;
	char *cmd = NULL;
	dma_addr_t cmd_dma;
	struct qdrv_wlan *qw;
	int cmdlen;
	char calcmd[8];
	int result = 0;

	qcb = dev_get_drvdata(dev);
	qw = (struct qdrv_wlan *)qcb->macs[0].data;

	cmd = qdrv_hostlink_alloc_coherent(NULL, sizeof(qcb->command), &cmd_dma, GFP_ATOMIC);
	if (cmd == NULL) {
		DBGPRINTF_E("Failed allocate %ld bytes for cmd\n", sizeof(qcb->command));
		return(-1);
	}

	cmdlen = sizeof(calcmd)/sizeof(calcmd[0]);

	calcmd[0] = 33; //GET_RFIC_REG;
	calcmd[1] = 0;
	calcmd[2] = cmdlen;
	calcmd[3] = 0;
	calcmd[4] = 1;
	calcmd[5] = 0;
	calcmd[6] = 2;
	calcmd[7] = offset;

	memcpy(cmd, calcmd, cmdlen);
	qdrv_hostlink_msg_calcmd(qw, cmdlen, cmd_dma);

	result = (cmd[8] << 24 | cmd[7] << 16 | cmd[6] << 8 | cmd[5]);

	qdrv_hostlink_free_coherent(NULL, sizeof(qcb->command), cmd, cmd_dma);

	return result;
}

int qdrv_command_read_chip_ver(struct device *dev)
{
	struct qdrv_cb *qcb;
	char *cmd = NULL;
	dma_addr_t cmd_dma;
	struct qdrv_wlan *qw;
	int cmdlen, i;
	char calcmd[6];
	char ret_val;

	for(i = 0; i < 6; i++)
		calcmd[i] = 0;

	/* Get the private device data */
	qcb = dev_get_drvdata(dev);
	qw = (struct qdrv_wlan *)qcb->macs[0].data;

	cmd = qdrv_hostlink_alloc_coherent(NULL, sizeof(qcb->command), &cmd_dma, GFP_ATOMIC);
	if(cmd == NULL)
	{
		DBGPRINTF_E("Failed allocate %ld bytes for cmd\n", sizeof(qcb->command));
		return(-1);
	}

	cmdlen = 4; //Cmd format : 11 0 4 0 //

	calcmd[0] = GET_CHIP_ID;
	calcmd[1] = 0;
	calcmd[2] = cmdlen;
	calcmd[3] = 0;

	memcpy(cmd, calcmd, cmdlen);

	qdrv_hostlink_msg_calcmd(qw, cmdlen, cmd_dma);

	ret_val = cmd[5];

	qdrv_hostlink_free_coherent(NULL, sizeof(qcb->command), cmd, cmd_dma);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return ret_val;
}

static int qdrv_command_write(struct device *dev, int argc, char *argv[])
{
	u32 addr;
	u32 value;
	u32 *segvaddr;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Check that we have all the arguments */
	if(argc != 4) {
		goto error;
	}

	if(strcmp(argv[1], "addr") == 0) {
		if (sscanf(argv[2], "%x", &addr) != 1) {
			goto error;
		}

		if (sscanf(argv[3], "%x", &value) != 1) {
			goto error;
		}
	} else {
		goto error;
	}

	/* Check that address is valid */
	if (!qdrv_command_is_valid_addr(addr)) {
		DBGPRINTF_E("addr 0x%x is not valid\n", (unsigned)addr);
		goto error;
	}

	DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL, "0x%08x = 0x%08x\n", addr, value);

	segvaddr = ioremap_nocache(addr, 4);
	if (segvaddr == NULL) {
		goto error;
	}
	*segvaddr = value;
	iounmap(segvaddr);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);

error:

	DBGPRINTF_E("Invalid arguments to write command\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(-1);
}

static int qdrv_command_read(struct device *dev, int argc, char *argv[])
{
	u32 addr;
	unsigned int num;
	int values_per_line = 1;
	struct qdrv_cb *qcb = dev_get_drvdata(dev);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if(argc < 4) {
		goto error;
	}

	if(strcmp(argv[1], "addr") == 0) {
		if (sscanf(argv[2], "%x", &addr) != 1) {
			goto error;
		}

		if (sscanf(argv[3], "%u", &num) != 1) {
			goto error;
		}

		qcb->read_count = num;

		if (argc > 4) {
			if (sscanf(argv[4], "%d", &values_per_line) != 1) {
				goto error;
			}

			if (values_per_line != 1 &&
			    values_per_line != 2 &&
			    values_per_line != 4) {
				goto error;
			}

			num = (num + values_per_line - 1) / values_per_line;
		}
	} else {
		goto error;
	}

	DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_QCTRL, "0x%08x (%d)\n", addr, num);

	qdrv_control_set_show(qdrv_show_memory, (void *) qcb, num, 1);

	/* Save the address for a memory read */
	qcb->read_addr = addr;
	qcb->values_per_line = values_per_line;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);

error:
	DBGPRINTF_E("Invalid arguments to read command\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(-1);
}

/*
 * Push selected sysmsgs for capture and debugging
 * - currently only MuC messages are sent
 *
 * Add a debug message to the output buffer.
 * If the output buffer is full it is forwarded to the Ethernet driver.
 * A timer function also calls this function periodically so that data is not left in the
 * buffer for too long.
 */
void qdrv_control_sysmsg_send(void *data, char *sysmsg, u_int32_t text_len, int send_now)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) data;
	struct qdrv_mac *mac = qw->mac;
	uint64_t tsf;
#define QDRV_CMD_SYSMSG_PREF_LEN	25
	char fmt[] = "[%08x.%08x] MuC: %s";
	u_int16_t line_len = QDRV_CMD_SYSMSG_PREF_LEN + text_len;
	static struct qdrv_netdebug_sysmsg *msgbuf = NULL;
	static int msg_len = 0;

	if (mac->params.mucdbg_netdbg == 0 &&
			(qw->pktlogger.flag & BIT(QDRV_NETDEBUG_TYPE_SYSMSG)) == 0) {
		return;
	}

	if ((msgbuf != NULL) &&
			((send_now != 0) || ((msg_len + line_len) > (QDRV_NETDEBUG_SYSMSG_LENGTH - 20)))) {
		int udp_len = sizeof(msgbuf->ndb_hdr) + msg_len + 1;
		qdrv_pktlogger_hdr_init(qw, &msgbuf->ndb_hdr, QDRV_NETDEBUG_TYPE_SYSMSG, udp_len);
		qdrv_pktlogger_send(msgbuf, udp_len);
		msgbuf = NULL;
		msg_len = 0;
	}

	if (text_len == 0 || sysmsg == NULL) {
		return;
	}

	if (msgbuf == NULL) {
		msgbuf = qdrv_pktlogger_alloc_buffer("sysmsg", sizeof(*msgbuf));
		if (msgbuf == NULL) {
			return;
		}
	}

	if (mac->params.mucdbg_netdbg) {
		qw->ic.ic_get_tsf(&tsf);
		snprintf(msgbuf->msg + msg_len, line_len, fmt, U64_HIGH32(tsf), U64_LOW32(tsf), sysmsg),
		msg_len += line_len;
	} else if (qw->pktlogger.flag & BIT(QDRV_NETDEBUG_TYPE_SYSMSG)) {
		sprintf(msgbuf->msg + msg_len, "%s", sysmsg);
		msg_len += text_len;
	}
}

/*
 * Push TXBF pkt for capture and debugging
 */
void qdrv_control_txbf_pkt_send(void *data, u8 *stvec, u32 bw)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *)data;
	void *databuf = NULL;
	struct qdrv_netdebug_txbf *stats;
	int indx;

	for (indx = 0; indx <= !!bw; indx++) {
		databuf = qdrv_pktlogger_alloc_buffer("txbf", sizeof(*stats));
		if (databuf == NULL) {
			return;
		}

		stats = (struct qdrv_netdebug_txbf*)databuf;
		qdrv_pktlogger_hdr_init(qw, &stats->ndb_hdr,
				QDRV_NETDEBUG_TYPE_TXBF, sizeof(*stats));
		if (bw && !indx) {
			stats->ndb_hdr.flags = QDRV_NETDEBUG_FLAGS_TRUNCATED;
		}

		dma_map_single(NULL, stvec + (indx * QDRV_NETDEBUG_TXBF_DATALEN),
			QDRV_NETDEBUG_TXBF_DATALEN, DMA_FROM_DEVICE);
		memcpy(stats->stvec_data, stvec + (indx * QDRV_NETDEBUG_TXBF_DATALEN),
				QDRV_NETDEBUG_TXBF_DATALEN);

		qdrv_pktlogger_send(stats, sizeof(*stats));
		databuf = NULL;
	}
}

static void qdrv_command_memdebug_usage(void)
{
	printk("Usage: \n"
		"\tmemdebug 0 add <address> <size>  - e.g. memdebug add 3e01430 16\n");
}

static int qdrv_command_memdebug(struct device *dev, int argc, char *argv[])
{
	struct qdrv_mac *mac;
	struct qdrv_wlan *qw;
	size_t totalsize = 0;
	int i;
	struct qdrv_memdebug_watchpt *wp;

	if (argc < 2) {
		qdrv_command_memdebug_usage();
		return -1;
	}

	mac = qdrv_control_mac_get(dev, argv[1]);
	if (mac == NULL) {
		return -1;
	}

	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return -1;
	}

	if (strncmp(argv[2], "add", 3) == 0) {
		if (argc < 4) {
			qdrv_command_memdebug_usage();
			return -1;
		}

		if (qw->pktlogger.mem_wp_index >= MAX_MEMDEBUG_WATCHPTS) {
			DBGPRINTF_E("memdebug watchpoint limit reached\n");
			return -1;
		}

		wp = &qw->pktlogger.mem_wps[qw->pktlogger.mem_wp_index];

		if (sscanf(argv[3], "%lx", (unsigned long *)&wp->addr) != 1) {
			DBGPRINTF_E("could not parse hex address\n");
			return -1;
		}
		if ((wp->addr & 0x03)) {
			DBGPRINTF_E("address must be word-aligned\n");
			return -1;
		}

		if (sscanf(argv[4], "%lu", (unsigned long *)&wp->size) != 1) {
			DBGPRINTF_E("could not parse decimal size\n");
			return -1;
		}

		/*
		 * totalsize if the amount of payload:
		 * [wp struct] [data] * num watchpoints
		 * check that we're not requesting too much data, e.g. oversizing the debug packet
		 */
		for (i = 0; i <= qw->pktlogger.mem_wp_index; i++) {
			totalsize += (qw->pktlogger.mem_wps[i].size * sizeof(u32));
			totalsize += sizeof(struct qdrv_memdebug_watchpt);
		}

		if (totalsize > QDRV_NETDEBUG_MEM_DATALEN) {
			DBGPRINTF_E("data monitoring packet limit hit\n");
			return -1;
		}

		wp->remap_addr = ioremap_nocache(wp->addr, (wp->size * sizeof(u32)));
		if (wp->remap_addr == NULL) {
			DBGPRINTF_E("unable to remap address\n");
			return -1;
		}
		printk("%s add %08x %p %u\n", __FUNCTION__, wp->addr, wp->remap_addr, wp->size);

		qw->pktlogger.mem_wp_index++;

	} else {
		qdrv_command_memdebug_usage();
		return -1;
	}

	return 0;
}

#ifdef QDRV_TX_DEBUG
__sram_text uint32_t qdrv_tx_ctr[60] = {0};
__sram_text uint32_t qdrv_dbg_ctr[8] = {0};
/*
 * Display or enable Tx debugs
 *
 * Syntax:
 *   txdbg
 *   - display all qdrv_tx_ctr[] values
 *
 *   txdbg [<ctr> <cnt>] ...
 *   - print the next <cnt> QDRV_TX_DBG(<ctr>, ...) debug messages
 *   - any number of <ctr>/<cnt> pairs may be specified
 */
static int qdrv_command_txdbg(struct device *dev, int argc, char *argv[])
{
	int i;
	int j;

	if (argc > 2) {
		printk("qdrv_dbg_ctr");
		for (i = 1; (i + 1) < argc; i += 2) {
			sscanf(argv[i], "%d", &j);
			if (j > (ARRAY_SIZE(qdrv_dbg_ctr) - 1)) {
				printk("%s: ctr %d exceeds array size\n",
					argv[0], j);
			}
			sscanf(argv[i + 1], "%d", &qdrv_dbg_ctr[j]);
			printk(" [%u]=%u", j, qdrv_dbg_ctr[j]);
		}
		printk("\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(qdrv_tx_ctr); i++) {
		printk("%02u:%-8u ", i, qdrv_tx_ctr[i]);
		if (((i + 1) % 12) == 0) {
			printk("\n");
		}
	}

	return 0;
}
#endif

static int qdrv_command_clearsram(struct device *dev, int argc, char *argv[])
{
	struct qdrv_cb *qcb;
	uint16_t *buf = (uint16_t *) (CONFIG_ARC_MUC_STACK_INIT - CONFIG_ARC_MUC_STACK_SIZE);

	qcb = dev_get_drvdata(dev);

	/* Copy out any crash log, let the parse function deal with validity of the buffer */
	if (qdrv_crash_log == NULL) {

		/*
		 * Format of buffer:
		 *	2 bytes: Header (HEADER_CORE_DUMP)
		 *	2 bytes: Length of the compressed logs (n)
		 *	n bytes: Compressed logs
		 */

	        /* Check if the header exists */
		if (*buf == HEADER_CORE_DUMP) {
			qdrv_crash_log_len = *(buf + 1);

			if (qdrv_crash_log_len  > CONFIG_ARC_MUC_STACK_SIZE) {
				DBGPRINTF_E("%s: qdrv_crash_log_len (%u) out of range\n", __func__, qdrv_crash_log_len);
				return -1;
			}

			qdrv_crash_log = kmalloc(qdrv_crash_log_len, GFP_KERNEL);
			if (!qdrv_crash_log) {
				DBGPRINTF_E("%s: Could not allocate %u bytes for qdrv_crash_log\n", __func__,
					qdrv_crash_log_len);
				return -1;
			}

			/* Strip the header and length while copying */
			memcpy(qdrv_crash_log, (char *) (buf + 2), qdrv_crash_log_len);
		}
	}

	if (qcb->resources == 0) {
		/*
		 * Strictly speaking memory clearing is not necessary as during ELF segments copying
		 * to memory, it is cleared before placing data.
		 * Firmware should clear heaps by itself.
		 * But let's have this function (can be invoked from user-space only by writing
		 * command to sysfs file) to fill holes between segments, and for safety.
		 */

		u32 *p_uncached = ioremap_nocache(RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_SRAM_B1_BASE, CONFIG_ARC_MUC_SRAM_B1_SIZE);
		memset(p_uncached, 0, CONFIG_ARC_MUC_SRAM_B1_SIZE);
		iounmap(p_uncached);

		p_uncached = ioremap_nocache(RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_SRAM_B2_BASE, CONFIG_ARC_MUC_SRAM_B2_SIZE);
		memset(p_uncached, 0, CONFIG_ARC_MUC_SRAM_B2_SIZE);
		iounmap(p_uncached);

		p_uncached = ioremap_nocache(RUBY_CRUMBS_ADDR, RUBY_CRUMBS_SIZE);
		memset(p_uncached, 0, RUBY_CRUMBS_SIZE);
		iounmap(p_uncached);

		p_uncached = ioremap_nocache(RUBY_DRAM_BEGIN + CONFIG_ARC_MUC_BASE, CONFIG_ARC_MUC_SIZE);
		memset(p_uncached, 0, CONFIG_ARC_MUC_SIZE);
		iounmap(p_uncached);

		p_uncached = ioremap_nocache(RUBY_DRAM_BEGIN + CONFIG_ARC_DSP_BASE, CONFIG_ARC_DSP_SIZE);
		memset(p_uncached, 0, CONFIG_ARC_DSP_SIZE);
		iounmap(p_uncached);

	} else {
		DBGPRINTF_E("Resources are held, not freeing SRAM\n");
	}

	return 0;
}

static int qdrv_command_bridge(struct device *dev, int argc, char *argv[])
{
	struct qdrv_mac *mac;
	struct qdrv_wlan *qw;
	struct ieee80211vap *vap;
	char *dev_name;

	if (argc < 2) {
		printk("Usage: bridge 0 {showmacs | enable | disable | clear}\n");
		return -1;
	}

	mac = qdrv_control_mac_get(dev, argv[1]);
	if (mac == NULL) {
		return -1;
	}
	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return -1;
	}

	if (mac->vnet[0] != NULL) {
		dev_name = mac->vnet[0]->name;
	} else {
		/* Maybe never come here */
		DBGPRINTF_E("No primary interface\n");
		return -1;
	}

	vap = TAILQ_FIRST(&qw->ic.ic_vaps);
	if (vap->iv_opmode != IEEE80211_M_STA) {
		DBGPRINTF_E("%s: 3-address mode bridging is only supported on stations\n",
			dev_name);
		return 0;
	}

	if (strncmp(argv[2], "showmacs", 8) == 0) {
		if (!QDRV_FLAG_3ADDR_BRIDGE_ENABLED()) {
			printk("%s: 3-address mode bridging is disabled\n",
				dev_name);
			return 0;
		}
		qdrv_br_show(&qw->bridge_table);
	} else if (strncmp(argv[2], "enable", 6) == 0) {
		if (QDRV_FLAG_3ADDR_BRIDGE_ENABLED()) {
			printk("%s: 3-address mode bridging is already enabled\n",
				dev_name);
			return 0;
		}
		qdrv_br_create(&qw->bridge_table);
		qw->flags_ext &= ~QDRV_FLAG_3ADDR_BRIDGE_DISABLE;
		printk("%s: 3-address mode bridging enabled \n",
			dev_name);
	} else if (strncmp(argv[2], "disable", 7) == 0) {
		if (!QDRV_FLAG_3ADDR_BRIDGE_ENABLED()) {
			printk("%s: 3-address mode bridging is already disabled\n",
				dev_name);
			return 0;
		}
		qw->flags_ext |= QDRV_FLAG_3ADDR_BRIDGE_DISABLE;
		qdrv_br_delete(&qw->bridge_table);
		printk("%s: 3-address mode bridging disabled\n",
			dev_name);
	} else if (strncmp(argv[2], "clear", 5) == 0) {
		if (!QDRV_FLAG_3ADDR_BRIDGE_ENABLED()) {
			printk("%s: 3-address mode bridging is disabled\n",
				dev_name);
			return 0;
		}
		qdrv_br_clear(&qw->bridge_table);
	} else {
		printk("Usage: bridge 0 {showmacs | enable | disable | clear}\n");
	}

	return 0;
}

static inline void qdrv_control_show_wmm_ac_map(
		struct seq_file *s, void *data, u32 num)
{
	uint32_t i;

	seq_printf(s, "TOS/AC:\n");
	for (i = 0; i < IEEE8021P_PRIORITY_NUM; i++) {
		seq_printf(s, "%d/%s\n", i, qdrv_sch_tos2ac_str(i));
	}
}

static inline void qdrv_control_set_wmm_ac_map(
		char *dev_name, int tos, int aid)
{
	struct net_device *ndev = dev_get_by_name(&init_net, dev_name);

	if (ndev) {
		netif_stop_queue(ndev);
		qdrv_sch_set_ac_map(tos, aid);
		netif_start_queue(ndev);

		dev_put(ndev);
	} else {
		printk("Fail to set wmm ac map, device can't be found.\n");
	}
}

static int qdrv_control_set_br_isolate(struct device *dev, int argc, char *argv[])
{
	struct qdrv_mac *mac = NULL;
	struct qdrv_wlan *qw = NULL;
	int val;

	mac = qdrv_control_mac_get(dev, "0");
	if (mac)
		qw = qdrv_control_wlan_get(mac);

	if (!qw)
		return -ENODEV;

	if (argc == 1) {
		val = simple_strtol(argv[0], NULL, 10);
		if (val < 0)
			return -EINVAL;
		else if (val > 0)
			qw->br_isolate |= QDRV_BR_ISOLATE_NORMAL;
		else
			qw->br_isolate &= ~QDRV_BR_ISOLATE_NORMAL;

		return 0;
	}

	if (argc == 2) {
		if (!strcmp(argv[0], "vlan")) {
			if (!strcmp(argv[1], "all")) {
				qw->br_isolate |= QDRV_BR_ISOLATE_VLAN;
				qw->br_isolate_vid = QVLAN_VID_ALL;
				return 0;
			} else if (!strcmp(argv[1], "none")) {
				qw->br_isolate &= ~QDRV_BR_ISOLATE_VLAN;
				return 0;
			}

			val = simple_strtol(argv[1], NULL, 10);
			if (val <= 0 || val >= QVLAN_VID_MAX)
				return -EINVAL;

			qw->br_isolate |= QDRV_BR_ISOLATE_VLAN;
			qw->br_isolate_vid = val;

			return 0;
		}
	}

	return -EINVAL;
}

static inline void qdrv_control_show_dscp_8021p_map(
		struct seq_file *s, void *data, u32 num)
{
	uint8_t i;

	for (i = 0; i < IP_DSCP_NUM; i++) {
		seq_printf(s, "%d", qdrv_sch_get_8021p_map(i));
	}
	seq_printf(s, "\n");
}

static inline void qdrv_control_set_dscp_8021p_map(
		char *dev_name, uint8_t ip_dscp, uint8_t dot1p_up)
{
	qdrv_sch_set_8021p_map(ip_dscp, dot1p_up);
}

static int qdrv_control_set_sta_vlan(struct qdrv_mac *mac, const char *addr, uint16_t vid)
{
	struct qdrv_wlan *qw = qdrv_mac_get_wlan(mac);
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;
	struct qdrv_vap *qv;
	int ret;
	uint8_t sta_addr[ETH_ALEN];
	struct qtn_vlan_dev *vdev;

	sscanf(addr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		&sta_addr[0], &sta_addr[1], &sta_addr[2], &sta_addr[3], &sta_addr[4], &sta_addr[5]);

	ni = ieee80211_find_node(nt, sta_addr);
	if (unlikely(!ni)) {
		printk(KERN_ERR"station %s was not found\n", addr);
		return -EINVAL;
	}
	qv = container_of(ni->ni_vap, struct qdrv_vap, iv);
	vdev = vdev_tbl_lhost[QDRV_WLANID_FROM_DEVID(qv->devid)];

	ret = switch_vlan_set_node(vdev, IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx), vid);
	if (ret)
		printk(KERN_ERR "failed to put station %s into VLAN %u\n", addr, vid);
	else
		printk(KERN_INFO"station %s into VLAN %u\n", addr, vid);

	ieee80211_free_node(ni);
	return ret;
}

static void qdrv_control_vlan_enable(struct qdrv_mac *mac, int enable)
{
	struct qdrv_wlan *qw;

	if (vlan_enabled != enable) {
		vlan_enabled = enable;
#if !defined(CONFIG_TOPAZ_PCIE_HOST) && !defined(CONFIG_TOPAZ_PCIE_TARGET)
		topaz_emac_to_lhost(enable);
#endif
		qw = qdrv_mac_get_wlan(mac);
		qdrv_wlan_vlan_enable(&qw->ic, enable);
	}
}

int qdrv_control_set_vlan_enable(struct qdrv_mac *mac, const char *cmd)
{
	if (strcmp(cmd, "enable") == 0) {
		qdrv_control_vlan_enable(mac, 1);
	} else if (strcmp(cmd, "disable") == 0) {
		qdrv_control_vlan_enable(mac, 0);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int qdrv_control_vlan_config_cmd_parse(
			int argc,
			char *argv[],
			int *vlanid,
			int *tagtx,
			int *pvid,
			int *mode,
			int *add)
{
	if (!strcmp(argv[3], "access")) {
		*mode = QVLAN_MODE_ACCESS;
	} else if (!strcmp(argv[3], "trunk")) {
		*mode = QVLAN_MODE_TRUNK;
	} else if (!strcmp(argv[3], "hybrid")) {
		*mode = QVLAN_MODE_HYBRID;
	} else if (!strcmp(argv[3], "dynamic")) {
		*mode = QVLAN_MODE_DYNAMIC;
		*add = 1;
		return 0;
	} else if (!strcmp(argv[3], "undynamic")) {
		*mode = QVLAN_MODE_DYNAMIC;
		*add = 0;
		return 0;
	} else {
		return -EINVAL;
	}

	*vlanid = simple_strtol(argv[4], NULL, 10);

	if (!strcmp(argv[5], "add"))
		*add = 1;
	else if (!strcmp(argv[5], "del"))
		*add = 0;
	else
		return -EINVAL;

	if (!strcmp(argv[6], "tag"))
		*tagtx = 1;
	else if (!strcmp(argv[6], "untag"))
		*tagtx = 0;
	else
		return -EINVAL;

	if (!strcmp(argv[7], "default"))
		*pvid = 1;
	else if (!strcmp(argv[7], "none"))
		*pvid = 0;
	else
		return -EINVAL;

	return 0;
}

static int qdrv_control_vlan_config_cmd_validation(
			struct qtn_vlan_dev *vdev,
			struct qdrv_mac *mac,
			struct qdrv_vap *qv,
			int wifidev,
			int vlanid,
			int tagtx,
			int pvid,
			int mode,
			int add)
{
	if (mode == QVLAN_MODE_DYNAMIC) {
		if (!wifidev || qv->iv.iv_opmode != IEEE80211_M_HOSTAP) {
			printk(KERN_ERR "Dynamic VLAN applies only to wifi AP interfaces\n");
			return -EINVAL;
		}
		if (add == 1)
			qdrv_control_vlan_enable(mac, 1);
		return 0;
	}

	if (!vlan_enabled) {
		printk(KERN_ERR "VLAN is disabled\n");
		return -EINVAL;
	}

	if (vlanid != QVLAN_VID_ALL && !qtn_vlan_is_valid(vlanid))
		return -EINVAL;

	if (pvid == 1 && vlanid == QVLAN_VID_ALL)
		return -EINVAL;

	if (!qtn_vlan_is_mode(vdev, mode))
		switch_vlan_dev_reset(vdev, mode);

	if (add == 0 && !qtn_vlan_is_member(vdev, vlanid))
		return -EINVAL;

	if (add == 0 && pvid == 1 && !qtn_vlan_is_pvid(vdev, vlanid))
		return -EINVAL;

	if (qtn_vlan_is_pvid(vdev, vlanid) && pvid == 0)
		return -EINVAL;

	if (pvid == 1 && tagtx == 1)
		return -EINVAL;

	switch (mode) {
	case  QVLAN_MODE_ACCESS:
		if (pvid == 0)
			return -EINVAL;
		break;
	case QVLAN_MODE_TRUNK:
		if (tagtx == 0 && pvid == 0)
			return -EINVAL;
		break;
	case QVLAN_MODE_HYBRID:
	default:
		break;
	}

	return 0;
}

static int qdrv_control_vlan_config_cmd_execute(
			struct qtn_vlan_dev *vdev,
			int vlanid,
			int tagtx,
			int pvid,
			int mode,
			int add)
{
	int ret = -1;

	if (mode == QVLAN_MODE_DYNAMIC) {
		if (add == 1) {
			switch_vlan_dyn_enable(vdev);
			return 0;
		} else if (add == 0) {
			if (QVLAN_IS_DYNAMIC(vdev))
				switch_vlan_dyn_disable(vdev);
			return 0;
		} else {
			return -EINVAL;
		}
	}

	if (pvid == 1 && add == 0) {
		pvid = 1;
		vlanid = QVLAN_DEF_PVID;
	}

	if (pvid == 1) {
		ret = switch_vlan_set_pvid(vdev, vlanid);
	} else if (add == 1) {
		ret = switch_vlan_add_member(vdev, vlanid, 1);
		if (ret == 0 && tagtx == 1) {
			ret = switch_vlan_tag_member(vdev, vlanid);
		} else if (ret == 0 && tagtx == 0) {
			ret = switch_vlan_untag_member(vdev, vlanid);
		}
	} else if (add == 0) {
		ret = switch_vlan_del_member(vdev, vlanid);
		if (ret == 0 && (vlanid == QVLAN_VID_ALL ||
					qtn_vlan_is_pvid(vdev, vlanid)))
			ret = switch_vlan_set_pvid(vdev, QVLAN_DEF_PVID);
	}

	return ret;
}

static int qdrv_control_vlan_config(struct qdrv_mac *mac, int argc, char *argv[])
{
	int vlanid = -1;
	int ret;
	int tagtx = -1;
	int pvid = -1;
	int mode = -1;
	int add = -1;
	struct net_device *ndev = NULL;
	struct qdrv_vap *qv = NULL;
	const char *dev_name = NULL;
	struct qtn_vlan_dev *vdev;
	int wifidev;

	if (argc != 3 && argc != 4 && argc != 5 && argc != 8)
		return -EINVAL;

	if (argc == 3)
		return qdrv_control_set_vlan_enable(mac, argv[2]);

	dev_name = argv[2];

	ndev = dev_get_by_name(&init_net, dev_name);
	if (!ndev) {
		printk(KERN_ERR"%s: netdevice %s does not exist\n", __FUNCTION__, dev_name);
		return -EINVAL;
	}
	wifidev = (ndev->qtn_flags & QTN_FLAG_WIFI_DEVICE);
	dev_put(ndev);

	if (wifidev) {
		qv = netdev_priv(ndev);
		vdev = switch_vlan_dev_get_by_idx(QTN_WLANID_FROM_DEVID(qv->devid));
	} else {
		vdev = switch_vlan_dev_get_by_port(ndev->if_port);
	}

	if (unlikely(vdev == NULL))
		return -EINVAL;

	if (argc == 4) {
		switch_vlan_dev_reset(vdev, QVLAN_DEF_PVID);
		return 0;
	}

	ret = qdrv_control_vlan_config_cmd_parse(argc, argv, &vlanid, &tagtx, &pvid, &mode, &add);
	if (ret)
		return ret;

	ret = qdrv_control_vlan_config_cmd_validation(vdev, mac, qv, wifidev, vlanid, tagtx, pvid, mode, add);
	if (ret)
		return ret;

	ret = qdrv_control_vlan_config_cmd_execute(vdev, vlanid, tagtx, pvid, mode, add);
	if (ret)
		return ret;

	if (wifidev)
		return qdrv_vap_vlan2index_sync(qv, mode, vlanid);

	return 0;
}

static int qdrv_control_set_vlan_group(struct qdrv_mac *mac, const char *dev_name, uint16_t vid, int enable)
{
	struct net_device *ndev;
	struct qdrv_vap *qv;
	struct qdrv_node *vlan_group;

	ndev = dev_get_by_name(&init_net, dev_name);
	if (!ndev) {
		printk(KERN_ERR"netdevice %s does not exist\n", dev_name);
		return -EINVAL;
	}
	qv = netdev_priv(ndev);
	dev_put(ndev);

	if (vid >= QVLAN_VID_MAX) {
		printk(KERN_ERR"%u is not a valid VLAN\n", vid);
		return -EINVAL;
	}

	if (enable) {
		vlan_group = qdrv_vlan_find_group_noref(qv, vid);
		if (vlan_group) {
			printk(KERN_INFO"VLAN group %u is present\n", vid);
			return 0;
		}

		vlan_group = qdrv_vlan_alloc_group(qv, vid);
		if (!vlan_group) {
			printk(KERN_INFO"VLAN group %u allocation failed\n", vid);
			return -ENOMEM;
		}

		printk(KERN_INFO"VLAN group %u created\n", vid);

	} else {
		vlan_group = qdrv_vlan_find_group_noref(qv, vid);
		if (!vlan_group) {
			printk(KERN_INFO"VLAN group %u does not exist\n", vid);
			return -EEXIST;
		}

		printk(KERN_INFO"VLAN group %u removed, refcnt %u\n", vid, ieee80211_node_refcnt(&vlan_group->qn_node));

		qdrv_vlan_free_group(vlan_group); /* neutralize VLAN group creation */
	}

	return 0;
}

static int qdrv_control_mac_reserve(int argc, char *argv[])
{
	uint8_t addr[IEEE80211_ADDR_LEN];
	uint8_t mask[IEEE80211_ADDR_LEN];

	if (argc == 0) {
		qdrv_mac_reserve_clear();
		return 0;
	}

	if (qdrv_parse_mac(argv[0], addr) < 0) {
		printk("%s: invalid mac address %s\n", __func__, argv[0]);
		return -1;
	}

	if (argc > 1) {
		if (qdrv_parse_mac(argv[1], mask) < 0) {
			printk("%s: invalid mask %s\n", __func__, argv[1]);
			return -1;
		}
	} else {
		memset(mask, 0xff, ARRAY_SIZE(mask));
	}

	return qdrv_mac_reserve_set(addr, mask);
}

static void qdrv_control_set_power_table_checksum(struct qdrv_cb *qcb, char *fname,
							char *checksum)
{
	struct qdrv_power_table_checksum_entry *p_entry;
	struct qdrv_power_table_checksum_entry *n_entry;

	if (qcb->power_table_ctrl.checksum_list_locked) {
		printk("QDRV: power table checksum list has been locked\n");
		return;
	}

	if (strlen(fname) > QDRV_POWER_TABLE_FNAME_MAX_LEN) {
		printk("QDRV: power table filename is too long\n");
		return;
	}

	if (strlen(checksum) != QDRV_POWER_TABLE_CHECKSUM_LEN) {
		printk("QDRV: power table checksum length is invalid\n");
		return;
	}

	p_entry = qcb->power_table_ctrl.checksum_list;
	while (p_entry) {
		if (strcmp(p_entry->fname, fname) == 0) {
			printk("QDRV: power table checksum for %s exists\n", fname);
			return;
		}
		if (p_entry->next) {
			p_entry = p_entry->next;
		} else {
			break;
		}
	}

	n_entry = kmalloc(sizeof(struct qdrv_power_table_checksum_entry), GFP_KERNEL);
	if (!n_entry) {
		printk("QDRV: set power table checksum malloc failed\n");
		return;
	}

	n_entry->next = NULL;
	strcpy(n_entry->fname, fname);
	strcpy(n_entry->checksum, checksum);
	if (p_entry) {
		p_entry->next = n_entry;
	} else {
		qcb->power_table_ctrl.checksum_list = n_entry;
	}

	printk("QDRV: power table checksum for %s\n", fname);
}

static int qdrv_command_set(struct device *dev, int argc, char *argv[])
{
	struct qdrv_cb *qcb;
	struct qdrv_mac *mac = NULL;
	struct qdrv_wlan *qw = NULL;
	int i;
	char *value;
	char *name;
	char *dest;
	uint32_t vendor_fix;
	uint32_t vap_default_state;
	int32_t brcm_rxglitch_thrshlds;

	qcb = dev_get_drvdata(dev);

	name = argv[1];
	if (name == NULL) {
		DBGPRINTF_E("set command is NULL\n");
		return -1;
	}

	value = argv[2];
	if (value == NULL) {
		DBGPRINTF_E("set command value for %s is NULL\n", name);
		return -1;
	}

	if (strcmp(name, "wmm_ac_map") == 0) {
		char *dev_name = argv[2];
		int tos = simple_strtol(argv[3], NULL, 10);
		int aid = simple_strtol(argv[4], NULL, 10);

		qdrv_control_set_wmm_ac_map(dev_name, tos, aid);
		return 0;
	}
	if (strcmp(name, "dscp_8021p_map") == 0) {
		char *dev_name = argv[2];
		uint8_t ip_dscp = simple_strtol(argv[3], NULL, 10);
		uint8_t dot1p_up = simple_strtol(argv[4], NULL, 10);

		qdrv_control_set_dscp_8021p_map(dev_name, ip_dscp, dot1p_up);
		return 0;
	}
	if (strcmp(name, "power_table_checksum") == 0) {
		char *fname = argv[2];
		char *checksum = argv[3];

		qdrv_control_set_power_table_checksum(qcb, fname, checksum);
		return 0;
	}
	if (strcmp(name, "lock_checksum_list") == 0) {
		qcb->power_table_ctrl.checksum_list_locked = 1;
		return 0;
	}
	if (strcmp(name, "power_selection") == 0) {
		qcb->power_table_ctrl.power_selection = simple_strtol(argv[2], NULL, 10);
		printk("set power_selection %u\n", qcb->power_table_ctrl.power_selection);
		return 0;
	}
	if (strcmp(name, "power_recheck") == 0) {
		qcb->power_table_ctrl.power_recheck = !!simple_strtol(argv[2], NULL, 10);
		printk("set power_recheck %u\n", qcb->power_table_ctrl.power_recheck);
		return 0;
	}
	if (strcmp(name, "vlan") == 0) {
		return qdrv_control_vlan_config((struct qdrv_mac *) (&qcb->macs[0]), argc, argv);
	}

	if (strcmp(name, "dyn-vlan") == 0) {
		const char *addr;
		uint16_t vid;
		int ival;
		if (argc != 4)
			return -EINVAL;
		addr = argv[2];
		ival = simple_strtol(argv[3], NULL, 10);
		vid = (qtn_vlan_is_valid(ival) ? (uint16_t)ival : QVLAN_DEF_PVID);

		return qdrv_control_set_sta_vlan((struct qdrv_mac *) (&qcb->macs[0]),
							addr, vid);
	}

	if (strcmp(name, "vlan-group") == 0) {
		/* set vlan-group {ifname} {vlan_id} [0|1] */
		const char *dev_name;
		uint16_t vid;
		int enable;
		if (argc != 5) {
			printk("vlan-group: invalid argument\n");
			return -EINVAL;
		}
		dev_name = argv[2];
		vid = simple_strtol(argv[3], NULL, 10);
		enable = simple_strtol(argv[4], NULL, 10);
		return qdrv_control_set_vlan_group((struct qdrv_mac *) (&qcb->macs[0]),
						dev_name, vid, !!enable);
	}

	if (strcmp(name, "mac_reserve") == 0)
		return qdrv_control_mac_reserve(argc - 2, &argv[2]);

	if (strcmp(name, "wps_intf") == 0) {
                if (argc != 3)
                        return -EINVAL;

                const char *dev_name = argv[2];
                struct net_device *ndev = dev_get_by_name(&init_net, dev_name);
                if (!ndev)
                        return -EINVAL;
                if (!(ndev->qtn_flags & QTN_FLAG_WIFI_DEVICE)) {
                        dev_put(ndev);
                        return -EINVAL;
                }

                qdrv_wps_button_exit();
                qdrv_wps_button_init(ndev);

                dev_put(ndev);

		return 0;
        }

	if (strcmp(name, "br_isolate") == 0)
		return qdrv_control_set_br_isolate(dev, argc - 2, &argv[2]);

	for (i = 0; i < PARAM_TABLE_SIZE; i++) {
		if (strcmp(name, s_param_table[i].name) == 0) {
			break;
		}
	}

	if (i == PARAM_TABLE_SIZE) {
		DBGPRINTF_E("Parameter %s is not recognized\n", name);
		return -1;
	}

	/* Check if a specific address is given */
	if (s_param_table[i].address != NULL) {
		dest = s_param_table[i].address;
	} else if(strcmp(name, "uc_flags") == 0) {
		/* setting something in shared parameters */
		shared_params *sp = qtn_mproc_sync_shared_params_get();
		if (sp == NULL) {
			DBGPRINTF_E("shared_params struct not yet published\n");
			return(-1);
		}
		dest = (char *) &sp->uc_flags;
	} else if(strcmp(name, "vendor_fix") == 0) {
		dest = (char *)&vendor_fix;
	} else if(strcmp(name, "vap_default_state") == 0) {
		dest = (char *)&vap_default_state;
	} else if(strcmp(name, "brcm_rxglitch_thrshlds") == 0) {
		dest = (char *)&brcm_rxglitch_thrshlds;
	} else {
		/* Use an offset into our control structure */
		dest = (char *) qcb + s_param_table[i].offset;
	}

	if (s_param_table[i].flags & P_FL_TYPE_INT) {
		if (value[0] == '0' && value[1] == 'x') {
			if (sscanf(&value[2], "%x", (int *) dest) != 1)
				goto error;
		} else {
			if (sscanf(value, "%d", (int *) dest) != 1)
				goto error;
		}
	} else if (s_param_table[i].flags & P_FL_TYPE_STRING) {
		strncpy(dest, value, s_param_table[i].size);
		dest[s_param_table[i].size - 1] = '\0';

	} else if(s_param_table[i].flags & P_FL_TYPE_MAC) {
		if (qdrv_parse_mac(value, (uint8_t *) dest) < 0) {
			goto error;
		}

		if ((uint8_t *)dest == &wifi_macaddr[0]) {
			qw = (struct qdrv_wlan *)qcb->macs[0].data;
			mac = (struct qdrv_mac *) (&qcb->macs[0]);
			qdrv_hostlink_msg_set_wifi_macaddr(qw, &wifi_macaddr[0]);
			memcpy(mac->mac_addr, wifi_macaddr, IEEE80211_ADDR_LEN);
			memcpy(qcb->mac0, wifi_macaddr, IEEE80211_ADDR_LEN);
			memcpy(qw->ic.ic_myaddr, wifi_macaddr, IEEE80211_ADDR_LEN);
		}
	}

	/* Propagate any parameters into sub-structures of the qcb. */
	qdrv_command_set_post(qcb);

	if(strcmp(name, "test1") == 0) {
		static int test_mode_pm_overide = 0;

		if (g_qdrv_radar_test_mode == 0x2) {
			if ((test_mode_pm_overide == 0) &&
					(pm_qos_add_requirement(PM_QOS_POWER_SAVE, "war_test1",
							BOARD_PM_LEVEL_FORCE_NO) == 0)) {

				test_mode_pm_overide = 1;
			}
			g_dbg_log_module |= DBG_LM;
			DBG_LOG_FUNC |= QDRV_LF_DFS_TESTMODE;
			DBG_LOG_LEVEL = DBG_LL_NOTICE;
		} else if (g_qdrv_radar_test_mode == 0x0) {
			if (test_mode_pm_overide != 0) {
				pm_qos_remove_requirement(PM_QOS_POWER_SAVE, "war_test1");
				test_mode_pm_overide = 0;
			}
			DBG_LOG_FUNC &= ~QDRV_LF_DFS_TESTMODE;
			DBG_LOG_LEVEL = DBG_LL_WARNING;
		}
	} else if (strcmp(name, "vendor_fix") == 0) {
		struct ieee80211com *ic;
		int update_beacon = 0;

		mac = qdrv_control_mac_get(dev, "0");
		if (mac == NULL) {
			DBGPRINTF_E("mac NULL\n");
			goto error;
                }
                qw = qdrv_control_wlan_get(mac);
		if (!qw) {
			goto error;
		}

		ic = &qw->ic;

		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX, "Previous vendor fix flag is 0x%x\n",
			ic->ic_vendor_fix);
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"DHCP fix is %s\n",
			(vendor_fix & VENDOR_FIX_BRCM_DHCP) ? "enabled" : "disabled");
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"Replace IGMP src mac is %s\n",
			(vendor_fix & VENDOR_FIX_BRCM_REPLACE_IGMP_SRCMAC) ? "enabled" : "disabled");
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"Replace IP src mac is %s\n",
			(vendor_fix & VENDOR_FIX_BRCM_REPLACE_IP_SRCMAC) ? "enabled" : "disabled");
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"Drop STA IGMP query is %s\n",
			(vendor_fix & VENDOR_FIX_BRCM_DROP_STA_IGMPQUERY) ? "enabled" : "disabled");

		if ((ic->ic_vendor_fix & VENDOR_FIX_BRCM_DHCP) != (vendor_fix & VENDOR_FIX_BRCM_DHCP)) {
			update_beacon = 1;
		}
		ic->ic_vendor_fix = vendor_fix;
		if (update_beacon) {
			struct ieee80211vap *vap;
			TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
				if (vap->iv_opmode != IEEE80211_M_HOSTAP)
					continue;
				if (vap->iv_state != IEEE80211_S_RUN)
					continue;
				ic->ic_beacon_update(vap);
			}
		}
	} else if (strcmp(name, "vap_default_state") == 0) {
		struct ieee80211com *ic;

		mac = qdrv_control_mac_get(dev, "0");
		if (mac == NULL) {
			DBGPRINTF_E("mac NULL\n");
			goto error;
                }
                qw = qdrv_control_wlan_get(mac);
		if (!qw) {
			goto error;
		}

		ic = &qw->ic;
		ic->ic_vap_default_state = !!vap_default_state;
	} else if (strcmp(name, "brcm_rxglitch_thrshlds") == 0) {
		struct ieee80211com *ic;
		struct brcm_rxglitch_thrshld_pair *pair;
		int pwr, idx, rssi, pos;
		uint32_t glitch;

		ic = qdrv_get_ieee80211com(dev);
		if (ic == NULL) {
			return -1;
		}
		pair = ic->ic_scs.scs_brcm_rxglitch_thrshlds;

		if ((brcm_rxglitch_thrshlds > 0) && (brcm_rxglitch_thrshlds <= BRCM_RXGLITCH_THRSHLD_SCALE_MAX)) {
			ic->ic_scs.scs_brcm_rxglitch_thrshlds_scale = brcm_rxglitch_thrshlds;
		} else {
			printk("brcm rx glitch thresholds scale must be in range (%u, %u]\n", 0, BRCM_RXGLITCH_THRSHLD_SCALE_MAX);
		}
		printk("brcm rx glitch thresholds scale = %u\n", ic->ic_scs.scs_brcm_rxglitch_thrshlds_scale);
		if (argc >= 7) {
			pwr = STR2L(argv[3]);
			idx = STR2L(argv[4]);
			rssi = STR2L(argv[5]);
			glitch = STR2L(argv[6]);
			pos = pwr * BRCM_RXGLITH_THRSHLD_STEP + idx;
			pair[pos].rssi = rssi;
			pair[pos].rxglitch = glitch;
			printk("Set pwr=%d, idx=%d to rssi=%d, glitch=%u\n", pwr, idx,
					pair[pos].rssi, pair[pos].rxglitch);
		} else {
			printk("current brcm_rxglitch_thresholds:\n");
			for (pwr = 0; pwr < BRCM_RXGLITH_THRSHLD_PWR_NUM; pwr++) {
				for (idx = 0; idx < BRCM_RXGLITH_THRSHLD_STEP; idx++) {
					pos = pwr * BRCM_RXGLITH_THRSHLD_STEP + idx;
					printk("pwr=%d, idx=%d, rssi=%d, glitch=%u\n", pwr, idx,
							pair[pos].rssi, pair[pos].rxglitch);
				}
			}
		}
	} else if(strcmp(name, "vlan_promisc") == 0) {
		br_vlan_set_promisc(*((int*)dest));
	} else if (strcmp(name, "pwr_mgmt") == 0) {
		uint8_t tdls_peer_mac[IEEE80211_ADDR_LEN];
		struct ieee80211com *ic;

		if (qdrv_parse_mac(argv[3], (uint8_t *)tdls_peer_mac) < 0) {
			goto error;
		}

		ic = qdrv_get_ieee80211com(dev);
		if (ic == NULL)
			return -1;

		ieee80211_send_qosnulldata_ext(ic, tdls_peer_mac, *((int*)dest));
	} else if (strcmp(name, "rxgain_params") == 0) {
		struct ieee80211com *ic;
		ic = qdrv_get_ieee80211com(dev);
		if (argc >= 8) {
			struct qtn_rf_rxgain_params rx_gain_params = {0};
			int index = (int)STR2L(argv[2]);
			rx_gain_params.lna_on_indx = (int8_t)STR2L(argv[3]);;
			rx_gain_params.max_gain_idx = (int16_t)STR2L(argv[4]);
			rx_gain_params.cs_thresh_dbm = (int16_t)STR2L(argv[5]);
			rx_gain_params.cca_prim_dbm = (int16_t)STR2L(argv[6]);
			rx_gain_params.cca_sec_scs_off_dbm = (int16_t)STR2L(argv[7]);
			rx_gain_params.cca_sec_scs_on_dbm = (int16_t)STR2L(argv[8]);
			qdrv_rxgain_params(ic, index, &rx_gain_params);
		} else {
			qdrv_rxgain_params(ic, 0, NULL);
		}
	}

	return 0;

error:
	DBGPRINTF_E("Value %s for parameter %s is invalid\n", value, name);

	return -1;
}

static char *qdrv_show_bands(const int chipid)
{
	switch (chipid) {
	case CHIPID_2_4_GHZ:
		return "2.4GHz";
		break;
	case CHIPID_5_GHZ:
		return "5GHz";
		break;
	case CHIPID_DUAL:
		return "dual";
		break;
	}

	return "unknown";
}

#define QDRV_SHOW_PRINT(_s, _fmt, ...) do {		\
	if (s)						\
		seq_printf(_s, _fmt, ##__VA_ARGS__);	\
	else						\
		printk(_fmt, ##__VA_ARGS__);		\
} while(0);

static char *qdrv_control_bld_type_str(enum qdrv_bld_type bld_type)
{
	switch (bld_type) {
	case QDRV_BLD_TYPE_ENG:
		return "eng";
	case QDRV_BLD_TYPE_BENCH:
		return "bench";
	case QDRV_BLD_TYPE_BUILDBOT:
		return "buildbot";
	case QDRV_BLD_TYPE_REL:
		return "release";
	case QDRV_BLD_TYPE_SDK:
		return "SDK";
	case QDRV_BLD_TYPE_GPL:
		return "GPL";
	}

	return "unknown";
}

static void qdrv_show_info(struct seq_file *s, void *data, uint32_t num)
{
	struct qdrv_mac *mac = (struct qdrv_mac *)data;
	struct qdrv_cb *qcb = container_of(mac, struct qdrv_cb, macs[mac->unit]);
	struct ieee80211com *ic = NULL;
	struct qdrv_wlan *qw;
#define QDRV_SWVER_STR_MAX	20
	char swver[QDRV_SWVER_STR_MAX] = { 0 };

	qw = qdrv_control_wlan_get(mac);
	if (qw) {
		ic = &qw->ic;
		snprintf(swver, sizeof(swver) - 1, DBGFMT_BYTEFLD4_P,
			DBGFMT_BYTEFLD4_V(ic->ic_ver_sw));
	}

	QDRV_SHOW_PRINT(s, "Build name:            %s\n", QDRV_BLD_NAME);
	QDRV_SHOW_PRINT(s, "Build revision:        %s\n", QDRV_BLD_REV);
	QDRV_SHOW_PRINT(s, "Build type:            %s\n", qdrv_control_bld_type_str(QDRV_BLD_TYPE));
	QDRV_SHOW_PRINT(s, "Build timestamp:       %lu\n", QDRV_BUILDDATE);
	if (strcmp(QDRV_BLD_NAME, QDRV_BLD_LABEL) != 0)
		QDRV_SHOW_PRINT(s, "Software label:        %s\n", QDRV_BLD_LABEL);
	QDRV_SHOW_PRINT(s, "Platform ID:           %u\n", QDRV_CFG_PLATFORM_ID);
	QDRV_SHOW_PRINT(s, "Hardware ID:           %s\n", qdrv_soc_get_hw_id(0));
	if (ic) {
		QDRV_SHOW_PRINT(s, "Hardware revision:     %s\n", qdrv_soc_get_hw_rev_desc(ic->ic_ver_hw));
		QDRV_SHOW_PRINT(s, "Band:                  %s\n", qdrv_show_bands(ic ? ic->ic_rf_chipid : -1));
	}
	QDRV_SHOW_PRINT(s, "Kernel version:        " DBGFMT_BYTEFLD3_P "\n",
							DBGFMT_BYTEFLD3_V(LINUX_VERSION_CODE));
	QDRV_SHOW_PRINT(s, "Calibration version:   %s\n", qcb->algo_version);
	QDRV_SHOW_PRINT(s, "DC/IQ cal version:     %s\n", dc_iq_calfile_version);
	QDRV_SHOW_PRINT(s, "Power cal version:     %s\n", power_calfile_version);
	QDRV_SHOW_PRINT(s, "MuC firmware:          %s\n", qcb->muc_firmware);
	QDRV_SHOW_PRINT(s, "DSP firmware:          %s\n", qcb->dsp_firmware);
	QDRV_SHOW_PRINT(s, "AuC firmware:          %s\n", qcb->auc_firmware);
	QDRV_SHOW_PRINT(s, "MAC address 0:         %pM\n", qcb->mac0);
	QDRV_SHOW_PRINT(s, "MAC address 1:         %pM\n", qcb->mac1);
	QDRV_SHOW_PRINT(s, "U-Boot version:        %s\n", RUBY_UBOOT_VERSION);
}

static void
qdrv_show_hw_desc( struct seq_file *s, void *data, u32 num )
{
	seq_printf(s, "%s\n", qdrv_soc_get_hw_desc(0));
}

static void
qdrv_show_mucfw( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_cb *qcb = (struct qdrv_cb *) data;

	seq_printf( s, "%s\n", qcb->muc_firmware );
}

static int
qdrv_fw_is_internal(enum qdrv_bld_type bld_type)
{
	switch(bld_type) {
	case QDRV_BLD_TYPE_REL:
	case QDRV_BLD_TYPE_SDK:
	case QDRV_BLD_TYPE_GPL:
		return 0;
	case QDRV_BLD_TYPE_ENG:
	case QDRV_BLD_TYPE_BENCH:
	case QDRV_BLD_TYPE_BUILDBOT:
		return 1;
	}
	return 1;
}

static void
qdrv_show_fw_ver(struct seq_file *s, void *data, u32 num)
{
	struct ieee80211com *ic = data;

	if (qdrv_fw_is_internal(QDRV_BLD_TYPE)) {
		seq_printf(s, "%s\n", QDRV_BLD_NAME);
	} else {
		seq_printf(s, DBGFMT_BYTEFLD4_P "\n", DBGFMT_BYTEFLD4_V(ic->ic_ver_sw));
	}
}

static void
qdrv_show_platform_id( struct seq_file *s, void *data, u32 num )
{
	seq_printf( s, "%u\n", QDRV_CFG_PLATFORM_ID );
}

static void
qdrv_show_checksum( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_cb *qcb = (struct qdrv_cb *) data;

	if (qcb->power_table_ctrl.reading_checksum) {
		seq_printf( s, "%s\n", qcb->power_table_ctrl.reading_checksum->checksum);
	} else {
		seq_printf( s, "NA\n");
	}
	qcb->power_table_ctrl.reading_checksum = NULL;
}

static void
qdrv_show_muc_value( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_cb *qcb = (struct qdrv_cb *) data;

	seq_printf( s, "%d\n", (int) qcb->value_from_muc );
}

static void
qdrv_show_auc_stats(struct seq_file *s, void *data, u32 num)
{
	const size_t nstats = ARRAY_SIZE(auc_field_stats);
	unsigned int i;

	for (i = 0; i < nstats; i++) {
		const uintptr_t addr = auc_field_stats[i].addr;
		const char *const name = auc_field_stats[i].name;
		uint32_t val = *((const uint32_t *) addr);
		seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, name, val);
	}
}

/* Calculate avarage value for histogram */
static uint32_t qdrv_get_hist_avr(uint32_t *histogram, uint32_t size, uint32_t width)
{
	uint32_t i;
	uint32_t sum1, sum2;

	for (i = 0, sum1 = 0, sum2 = 0; i < size; i++) {
		sum1 += (2*width*i + (width - 1))*histogram[i];
		sum2 += histogram[i];
	}

	return sum1/sum2/2;
}

static void
qdrv_show_dsp_time_histogram(struct seq_file *s,
	volatile struct qtn_txbf_mbox *txbf_mbox)
{
	int i;

/* ------------------------------------------------------------------------ */
	seq_printf(s, "%-*s ", QDRV_UC_STATS_DESC_LEN, "dsp_mu_qmat_qmem_copy_time_hist");
	for (i = 0; i < FIELD_ARRAY_SIZE(struct qtn_dsp_stats, dsp_mu_qmat_qmem_copy_time_hist); i++) {
		seq_printf(s, "%-2u-%-2uus ", i*DSP_MU_QMAT_COPY_TIME_HIST_WIDTH_US,
				(i + 1)*DSP_MU_QMAT_COPY_TIME_HIST_WIDTH_US - 1);
	}
	seq_printf(s, "avr us");

	seq_printf(s, "\n%-*s ", QDRV_UC_STATS_DESC_LEN, "dsp_mu_qmat_qmem_copy_time_hist");
	for (i = 0; i < FIELD_ARRAY_SIZE(struct qtn_dsp_stats, dsp_mu_qmat_qmem_copy_time_hist); i++) {
		seq_printf(s, "%-7u ", txbf_mbox->dsp_stats.dsp_mu_qmat_qmem_copy_time_hist[i]);
	}
	seq_printf(s, "%-7u\n", qdrv_get_hist_avr((uint32_t*)&txbf_mbox->dsp_stats.dsp_mu_qmat_qmem_copy_time_hist[0],
			FIELD_ARRAY_SIZE(struct qtn_dsp_stats, dsp_mu_qmat_qmem_copy_time_hist),
			DSP_MU_QMAT_COPY_TIME_HIST_WIDTH_US));
	seq_printf(s, "%-*s", QDRV_UC_STATS_DESC_LEN, "dsp_mu_qmat_qmem_copy_time_hist");
	seq_printf(s, "total maximum is %u us\n", txbf_mbox->dsp_stats.dsp_mu_qmat_qmem_copy_time_max);
/* ------------------------------------------------------------------------ */
	seq_printf(s, "%-*s ", QDRV_UC_STATS_DESC_LEN, "dsp_mu_qmat_inst_time_hist");
	for (i = 0; i < FIELD_ARRAY_SIZE(struct qtn_dsp_stats, dsp_mu_qmat_inst_time_hist); i++) {
		seq_printf(s, "%-2u-%-2ums ", i*DSP_MU_QMAT_INST_TIME_HIST_WIDTH_MS,
				(i + 1)*DSP_MU_QMAT_INST_TIME_HIST_WIDTH_MS - 1);
	}
	seq_printf(s, "avr ms");

	seq_printf(s, "\n%-*s ", QDRV_UC_STATS_DESC_LEN, "dsp_mu_qmat_inst_time_hist");
	for (i = 0; i < FIELD_ARRAY_SIZE(struct qtn_dsp_stats, dsp_mu_qmat_inst_time_hist); i++) {
		seq_printf(s, "%-7u ", txbf_mbox->dsp_stats.dsp_mu_qmat_inst_time_hist[i]);
	}
	seq_printf(s, "%-7u\n", qdrv_get_hist_avr((uint32_t*)&txbf_mbox->dsp_stats.dsp_mu_qmat_inst_time_hist[0],
			FIELD_ARRAY_SIZE(struct qtn_dsp_stats, dsp_mu_qmat_inst_time_hist),
			DSP_MU_QMAT_INST_TIME_HIST_WIDTH_MS));
	seq_printf(s, "%-*s ", QDRV_UC_STATS_DESC_LEN, "dsp_mu_qmat_inst_time_hist");
	seq_printf(s, "total maximum is %u ms\n", txbf_mbox->dsp_stats.dsp_mu_qmat_inst_time_max);
/* ------------------------------------------------------------------------ */

}

static void
qdrv_show_dsp_stats(struct seq_file *s, void *data, u32 num)
{
#if DSP_ENABLE_STATS
	int i;
	volatile struct qtn_txbf_mbox *txbf_mbox = qtn_txbf_mbox_get();

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ndp_rx", txbf_mbox->dsp_stats.dsp_ndp_rx);

	seq_printf(s, "%-*s %-3s %-8s %-8s %-8s %-8s %-8s %-8s %-8s %-8s %-8s %-8s\n",
			QDRV_UC_STATS_DESC_LEN,
			"dsp_act_rx", "aid", "total", "mu_gr_sl", "mu_prec", "su", "bad",
			"mu_drop", "mu_nexp", "mu_lock", "mu_rl_nu", "inv_len");

	for (i = 0; i < ARRAY_SIZE(txbf_mbox->dsp_stats.dsp_act_rx); i++)
		seq_printf(s, "%-*s %-3u %-8u %-8u %-8u %-8u %-8u %-8u %-8u %-8u %-8u %-8u\n",
			QDRV_UC_STATS_DESC_LEN, "dsp_act_rx", i,
			txbf_mbox->dsp_stats.dsp_act_rx[i],
			txbf_mbox->dsp_stats.dsp_act_rx_mu_grp_sel[i],
			txbf_mbox->dsp_stats.dsp_act_rx_mu_prec[i],
			txbf_mbox->dsp_stats.dsp_act_rx_su[i],
			txbf_mbox->dsp_stats.dsp_act_rx_bad[i],
			txbf_mbox->dsp_stats.dsp_act_rx_mu_drop[i],
			txbf_mbox->dsp_stats.dsp_act_rx_mu_nexp[i],
			txbf_mbox->dsp_stats.dsp_act_rx_mu_lock_cache[i],
			txbf_mbox->dsp_stats.dsp_act_rx_mu_rel_nuse[i],
			txbf_mbox->dsp_stats.dsp_act_rx_inval_len[i]);

	seq_printf(s, "%-*s %-3s %-4s %-4s %-8s %-8s %-8s %-8s\n", QDRV_UC_STATS_DESC_LEN,
			"dsp_mu_grp", "grp", "aid0", "aid1", "rank", "inst_ok", "upd_ok",
			"upd_fl");
	for (i = 0; i < ARRAY_SIZE(txbf_mbox->dsp_stats.dsp_mu_grp_inst_success); i++) {
		seq_printf(s, "%-*s %-3u %-4u %-4u %-8d %-8u %-8u %-8u\n",
			QDRV_UC_STATS_DESC_LEN, "dsp_mu_grp", i + 1,
			txbf_mbox->dsp_stats.dsp_mu_grp_aid0[i],
			txbf_mbox->dsp_stats.dsp_mu_grp_aid1[i],
			txbf_mbox->dsp_stats.dsp_mu_grp_rank[i],
			txbf_mbox->dsp_stats.dsp_mu_grp_inst_success[i],
			txbf_mbox->dsp_stats.dsp_mu_grp_update_success[i],
			txbf_mbox->dsp_stats.dsp_mu_grp_update_fail[i]);
	}

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_del_mu_node_rx", txbf_mbox->dsp_stats.dsp_del_mu_node_rx);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_act_tx", txbf_mbox->dsp_stats.dsp_act_tx);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_act_free_tx", txbf_mbox->dsp_stats.dsp_act_free_tx);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ndp_discarded", txbf_mbox->dsp_stats.dsp_ndp_discarded);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ndp_inv_bw", txbf_mbox->dsp_stats.dsp_ndp_inv_bw);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ndp_inv_len", txbf_mbox->dsp_stats.dsp_ndp_inv_len);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ndp_max_len", txbf_mbox->dsp_stats.dsp_ndp_max_len);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_inst_mu_grp_tx", txbf_mbox->dsp_stats.dsp_inst_mu_grp_tx);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "wr", txbf_mbox->wr);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "muc_to_dsp_action_frame_mbox[0]", txbf_mbox->muc_to_dsp_action_frame_mbox[0]);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "muc_to_dsp_action_frame_mbox[1]", txbf_mbox->muc_to_dsp_action_frame_mbox[1]);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "muc_to_dsp_ndp_mbox", txbf_mbox->muc_to_dsp_ndp_mbox);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "muc_to_dsp_del_grp_node_mbox", txbf_mbox->muc_to_dsp_del_grp_node_mbox);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_to_host_mbox", txbf_mbox->dsp_to_host_mbox);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ipc_in", txbf_mbox->dsp_stats.dsp_ipc_in);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ipc_out", txbf_mbox->dsp_stats.dsp_ipc_out);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_exc", txbf_mbox->dsp_stats.dsp_exc);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_ipc_int", txbf_mbox->dsp_stats.dsp_ipc_int);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_timer_int", txbf_mbox->dsp_stats.dsp_timer_int);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_timer1_int", txbf_mbox->dsp_stats.dsp_timer1_int);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_last_int", txbf_mbox->dsp_stats.dsp_last_int);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_flag", txbf_mbox->dsp_stats.dsp_flag);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_point", txbf_mbox->dsp_stats.dsp_point);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_sleep_in", txbf_mbox->dsp_stats.dsp_sleep_in);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_sleep_out", txbf_mbox->dsp_stats.dsp_sleep_out);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_qmat_invalid", txbf_mbox->dsp_stats.dsp_qmat_invalid);

	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_sram_qmat_num", txbf_mbox->dsp_stats.dsp_sram_qmat_num);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_err_neg_qmat_num", txbf_mbox->dsp_stats.dsp_err_neg_qmat_num);

	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_stat_bad_stack", txbf_mbox->dsp_stats.dsp_stat_bad_stack);

	uint32_t reg = qtn_mproc_sync_mem_read(RUBY_SYS_CTL_M2D_INT);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "RUBY_SYS_CTL_M2D_INT", reg);

	reg = qtn_mproc_sync_mem_read(RUBY_SYS_CTL_D2L_INT);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "RUBY_SYS_CTL_D2L_INT", reg);

	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_status32", txbf_mbox->dsp_stats.dsp_status32);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_status32_l1", txbf_mbox->dsp_stats.dsp_status32_l1);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_status32_l2", txbf_mbox->dsp_stats.dsp_status32_l2);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_ilink1", txbf_mbox->dsp_stats.dsp_ilink1);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_ilink2", txbf_mbox->dsp_stats.dsp_ilink2);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_blink", txbf_mbox->dsp_stats.dsp_blink);
	seq_printf(s, "%-*s 0x%08x\n", QDRV_UC_STATS_DESC_LEN, "dsp_sp", txbf_mbox->dsp_stats.dsp_sp);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_time", txbf_mbox->dsp_stats.dsp_time);
	seq_printf(s, "%-*s %d, %d, %d, %d\n", QDRV_UC_STATS_DESC_LEN, "mu_D_user1",
		txbf_mbox->dsp_stats.dspmu_D_user1[0], txbf_mbox->dsp_stats.dspmu_D_user1[1],
		txbf_mbox->dsp_stats.dspmu_D_user1[2], txbf_mbox->dsp_stats.dspmu_D_user1[3]);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "mu_intf_user1", txbf_mbox->dsp_stats.dspmu_max_intf_user1);
	seq_printf(s, "%-*s %d, %d, %d, %d\n", QDRV_UC_STATS_DESC_LEN, "mu_D_user2",
		txbf_mbox->dsp_stats.dspmu_D_user2[0], txbf_mbox->dsp_stats.dspmu_D_user2[1],
		txbf_mbox->dsp_stats.dspmu_D_user2[2], txbf_mbox->dsp_stats.dspmu_D_user2[3]);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "mu_intf_user2", txbf_mbox->dsp_stats.dspmu_max_intf_user2);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "mu rank criteria", txbf_mbox->rank_criteria_to_use);

	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_trig_mu_grp_sel", txbf_mbox->dsp_stats.dsp_trig_mu_grp_sel);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_rank_success", txbf_mbox->dsp_stats.dsp_mu_rank_success);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_rank_fail", txbf_mbox->dsp_stats.dsp_mu_rank_fail);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_grp_inv_act", txbf_mbox->dsp_stats.dsp_mu_grp_inv_act);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_act_cache_expired[grp_sel]", txbf_mbox->dsp_stats.dsp_act_cache_expired[0]);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_act_cache_expired[prec]", txbf_mbox->dsp_stats.dsp_act_cache_expired[1]);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_grp_upd_done", txbf_mbox->dsp_stats.dsp_mu_grp_upd_done);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_node_del", txbf_mbox->dsp_stats.dsp_mu_node_del);
	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_grp_inst_fail", txbf_mbox->dsp_stats.dsp_mu_grp_inst_fail);

	seq_printf(s, "%-*s %d\n", QDRV_UC_STATS_DESC_LEN, "dsp_mimo_ctrl_fail", txbf_mbox->dsp_stats.dsp_mimo_ctrl_fail);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_fb_80mhz", txbf_mbox->dsp_stats.dsp_mu_fb_80mhz);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_fb_40mhz", txbf_mbox->dsp_stats.dsp_mu_fb_40mhz);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_fb_20mhz", txbf_mbox->dsp_stats.dsp_mu_fb_20mhz);
	seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "dsp_mu_drop_20mhz", txbf_mbox->dsp_stats.dsp_mu_drop_20mhz);

	seq_printf(s, "%-*s %-3s %-3s %-3s %-3s %-8s\n", QDRV_UC_STATS_DESC_LEN,
			"txbf_msg_bufs", "idx", "st", "mt", "aid", "cnt");
	for (i = 0; i < ARRAY_SIZE(txbf_mbox->txbf_msg_bufs); i++) {
		seq_printf(s, "%-*s %-3u %-3u %-3u %-3u %-8u\n",
			QDRV_UC_STATS_DESC_LEN, "txbf_msg_bufs", i,
			txbf_mbox->txbf_msg_bufs[i].state,
			txbf_mbox->txbf_msg_bufs[i].msg_type,
			txbf_mbox->txbf_msg_bufs[i].aid,
			txbf_mbox->txbf_msg_bufs[i].counter);
	}

	qdrv_show_dsp_time_histogram(s, txbf_mbox);
#else
	seq_printf(s, "%-*s\n", QDRV_UC_STATS_DESC_LEN, "DSP_ENABLE_STATS must be defined to enable DSP stats");
#endif
}

static void
qdrv_show_uc_tx_stats(struct seq_file *s, void *data, u32 num)
{
	struct qdrv_mac *mac = (struct qdrv_mac *) data;
	struct qtn_stats_log *iw_stats_log;
	struct qdrv_wlan *qw;
	const u32* tx_stats;
	int i;
	const char *tx_stats_names[] = MUC_TX_STATS_NAMES_TABLE;

	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return;
	}

	iw_stats_log = qw->mac->mac_sys_stats;

	if (iw_stats_log == NULL) {
		return;
	}

	qdrv_pktlogger_flush_data(qw);

	if (qw->pktlogger.stats_uc_tx_ptr == NULL) {
		qw->pktlogger.stats_uc_tx_ptr = ioremap_nocache(muc_to_lhost((u32)iw_stats_log->tx_muc_stats),
				sizeof(struct muc_tx_stats));
		if (qw->pktlogger.stats_uc_tx_ptr == NULL)
			return;
	}

	tx_stats = qw->pktlogger.stats_uc_tx_ptr;

	for (i = 0; i < ARRAY_SIZE(tx_stats_names); i++) {
		seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, tx_stats_names[i], tx_stats[i]);
	}
}

static void
qdrv_show_uc_rx_stats(struct seq_file *s, void *data, u32 num)
{
	struct qdrv_mac *mac = (struct qdrv_mac *) data;
	struct qtn_stats_log *iw_stats_log;
	struct qdrv_wlan *qw;
	struct muc_rx_rates *uc_rx_rates;
	struct muc_rx_bf_stats *uc_rx_bf_stats;
	const u32* rx_stats;
	int i;
	const char *rx_stats_names[] = MUC_RX_STATS_NAMES_TABLE;

	qw = qdrv_control_wlan_get(mac);

	if (!qw) {
		return;
	}

	iw_stats_log = qw->mac->mac_sys_stats;

	if (iw_stats_log == NULL) {
		return;
	}

	qdrv_pktlogger_flush_data(qw);

	if (qw->pktlogger.stats_uc_rx_ptr == NULL) {
		qw->pktlogger.stats_uc_rx_ptr = ioremap_nocache(muc_to_lhost((u32)iw_stats_log->rx_muc_stats),
				sizeof(struct muc_rx_stats));
		if (qw->pktlogger.stats_uc_rx_ptr == NULL)
			return;
	}
	if (qw->pktlogger.stats_uc_rx_rate_ptr == NULL) {
		qw->pktlogger.stats_uc_rx_rate_ptr =  ioremap_nocache(muc_to_lhost((u32)iw_stats_log->rx_muc_rates),
				sizeof(struct muc_rx_rates));
		if (qw->pktlogger.stats_uc_rx_rate_ptr == NULL)
			return;
	}
	if (qw->pktlogger.stats_uc_rx_bf_ptr == NULL) {
		qw->pktlogger.stats_uc_rx_bf_ptr =  ioremap_nocache(muc_to_lhost((u32)iw_stats_log->rx_muc_bf_stats),
				sizeof(struct muc_rx_bf_stats));
		if (qw->pktlogger.stats_uc_rx_bf_ptr == NULL)
			return;
	}

	/* Stats */
	rx_stats = qw->pktlogger.stats_uc_rx_ptr;
	uc_rx_rates = (struct muc_rx_rates *)qw->pktlogger.stats_uc_rx_rate_ptr;

	for (i = 0; i < ARRAY_SIZE(rx_stats_names); i++) {
		seq_printf(s, "%-*s %u\n", QDRV_UC_STATS_DESC_LEN, rx_stats_names[i], rx_stats[i]);
	}

	/* Beamforming */
	uc_rx_bf_stats = (struct muc_rx_bf_stats *)qw->pktlogger.stats_uc_rx_bf_ptr;
	for (i = 0; i < QTN_STATS_NUM_BF_SLOTS; i++) {
		seq_printf(s, "\nBF slot=%u aid=%u ng=%u rx:", i, uc_rx_bf_stats->rx_bf_aid[i],
			uc_rx_bf_stats->rx_bf_ng[i]);
		if (uc_rx_bf_stats->rx_bf_valid[i] == 0) {
			seq_printf(s, " free");
		} else if ((i > 0) && (uc_rx_bf_stats->rx_bf_aid[i - 1] == uc_rx_bf_stats->rx_bf_aid[i])) {
			seq_printf(s, " dual slot");
		}
		if (uc_rx_bf_stats->rx_bf_11ac_ndp[i] || uc_rx_bf_stats->rx_bf_11ac_act[i] || uc_rx_bf_stats->rx_bf_11ac_grp_sel[i]) {
			seq_printf(s, " (11ac ndp=%u act=%u grp sel=%u prec=%u "
				"su=%u bad=%u fail=%u gradd=%u grdel=%u)",
				uc_rx_bf_stats->rx_bf_11ac_ndp[i],
				uc_rx_bf_stats->rx_bf_11ac_act[i],
				uc_rx_bf_stats->rx_bf_11ac_grp_sel[i],
				uc_rx_bf_stats->rx_bf_11ac_prec[i],
				uc_rx_bf_stats->rx_bf_11ac_su[i],
				uc_rx_bf_stats->rx_bf_11ac_bad[i],
				uc_rx_bf_stats->rx_bf_11ac_dsp_fail[i],
				uc_rx_bf_stats->mu_grp_add[i],
				uc_rx_bf_stats->mu_grp_del[i]);
		}
		if (uc_rx_bf_stats->rx_bf_11n_ndp[i] || uc_rx_bf_stats->rx_bf_11n_act[i]) {
			seq_printf(s, " (11n ndp=%u act=%u)",
				uc_rx_bf_stats->rx_bf_11n_ndp[i], uc_rx_bf_stats->rx_bf_11n_act[i]);
		}
	}
	seq_printf(s, "\n%-*s %u\n", QDRV_UC_STATS_DESC_LEN, "BF msg_buf_alloc_fail",
		uc_rx_bf_stats->msg_buf_alloc_fail);

	/* 11n rates */
	for (i = 0; i < ARRAY_SIZE(uc_rx_rates->rx_mcs); i++) {
		if ((i % 10) == 0) {
			seq_printf(s, "\nmcs_11n ");
		}
		seq_printf(s, " %2d:%-9u", i, uc_rx_rates->rx_mcs[i]);
	}
	/* 11ac rates */
	for (i = 0; i < ARRAY_SIZE(uc_rx_rates->rx_mcs_11ac); i++) {
		if ((i % 10) == 0) {
			seq_printf(s, "\nmcs_11ac");
		}
		seq_printf(s, " %2d:%-9u", i, uc_rx_rates->rx_mcs_11ac[i]);
	}
	seq_printf(s, "\n");
}

static void
qdrv_show_debug_level( char *type )
{
	printk("%s dump enabled, dump length %d", type, g_dbg_dump_pkt_len);

	if (DBG_LOG_FUNC_TEST(QDRV_LF_DUMP_BEACON)) {
		printk(", dump beacon frame");
	}
	if (DBG_LOG_FUNC_TEST(QDRV_LF_DUMP_ACTION)) {
		printk(", dump action frame");
	}
	if (DBG_LOG_FUNC_TEST(QDRV_LF_DUMP_MGT)) {
		printk(", dump management frame (exclude action frame)");
	}
	if (DBG_LOG_FUNC_TEST(QDRV_LF_DUMP_DATA)) {
		printk(", dump data frame");
	}
	printk(".\n");
}

static void
qdrv_show_vendor_fix( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) data;
	struct ieee80211com *ic = &qw->ic;

	seq_printf(s, "0x%x\n", ic->ic_vendor_fix);
}

static void
qdrv_show_vap_default_state( struct seq_file *s, void *data, u32 num )
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) data;
	struct ieee80211com *ic = &qw->ic;

	seq_printf(s, "%d\n", ic->ic_vap_default_state);
}

static void
qdrv_control_show_vlan_dev(struct seq_file *s, struct qtn_vlan_dev *vdev)
{
	struct qtn_vlan_config *vcfg;
	int ret;

	vcfg = kzalloc(sizeof(struct qtn_vlan_config), GFP_KERNEL);
	if (!vcfg) {
		printk(KERN_ERR"Not enough memory to print QVLAN configuration\n");
		return;
	}

	if (vlan_enabled) {
		vcfg->vlan_cfg = vdev->pvid & QVLAN_MASK_VID;
		vcfg->vlan_cfg |= ((struct qtn_vlan_user_interface *)(vdev->user_data))->mode << QVLAN_SHIFT_MODE;
		memcpy(vcfg->u.dev_config.member_bitmap, vdev->u.member_bitmap, sizeof(vcfg->u.dev_config.member_bitmap));
		memcpy(vcfg->u.dev_config.tag_bitmap, vdev->tag_bitmap, sizeof(vcfg->u.dev_config.tag_bitmap));
	} else {
		memset(vcfg, 0, sizeof(*vcfg));
		vcfg->vlan_cfg = (QVLAN_MODE_DISABLED << QVLAN_SHIFT_MODE);
	}

	ret = seq_write(s, vcfg, sizeof(struct qtn_vlan_config));
	if (ret)
		printk(KERN_ERR"VLAN info could not be written to seq file\n");

	kfree(vcfg);
}

static void
qdrv_control_show_vlan_config(struct seq_file *s, void *data, u32 num)
{
	int dev;
	int ret;
	struct qtn_vlan_dev *vdev;
	struct qtn_vlan_config *vcfg;
	struct net_device *ndev;

	if (!strcmp(data, "tagrx")) {
		vcfg = kzalloc(sizeof(struct qtn_vlan_config), GFP_KERNEL);
		if (!vcfg) {
			printk(KERN_ERR"Not enough memory to print QVLAN configuration\n");
			return;
		}

		if (vlan_enabled) {
			vcfg->vlan_cfg = 0;
			memcpy(vcfg->u.tagrx_config, qtn_vlan_info.vlan_tagrx_bitmap, sizeof(vcfg->u.tagrx_config));
		} else {
			memset(vcfg, 0, sizeof(*vcfg));
			vcfg->vlan_cfg = (QVLAN_MODE_DISABLED << QVLAN_SHIFT_MODE);
		}

		ret = seq_write(s, vcfg, sizeof(struct qtn_vlan_config));
		if (ret)
			printk(KERN_ERR"VLAN info could not be written to seq file\n");
		kfree(vcfg);
	} else {
		for (dev = 0; dev < VLAN_INTERFACE_MAX; dev++) {
			vdev = vdev_tbl_lhost[dev];
			if (vdev) {
				ndev = dev_get_by_index(&init_net, vdev->ifindex);
				if (unlikely(!ndev))
					break;
				if (!strncmp(ndev->name, data, IFNAMSIZ)) {
					dev_put(ndev);
					qdrv_control_show_vlan_dev(s, vdev);
					break;
				} else {
					dev_put(ndev);
				}
			}
		}
	}

}

static void qdrv_control_show_assoc(struct qdrv_mac *mac, int argc, char *argv[])
{
	struct qdrv_show_assoc_params params;

	qdrv_show_assoc_init_params(&params, mac);

	if (qdrv_show_assoc_parse_params(&params, argc, argv) != 0) {
		qdrv_control_set_show(qdrv_show_assoc_print_usage, &g_show_assoc_params, 1, 1);
		return;
	}

	if (down_interruptible(&s_output_sem)) {
		return;
	}

	g_show_assoc_params = params;

	up(&s_output_sem);

	qdrv_control_set_show(qdrv_show_assoc_print_stats, &g_show_assoc_params, 1, 1);
}

static void qdrv_show_core_dump_size( struct seq_file *s, void *data, uint32_t num )
{
	seq_printf(s, "%u\n", *((uint32_t *) data));
}

static void qdrv_show_core_dump( struct seq_file *s, void *data, uint32_t num )
{
	char byte;
	uint32_t i;

	for (i = 0; i < num; ++i) {
/* Only for debug - should be "off" in production code */
#if 0
		byte = (char) ((i % 26) + 'A');
#else
		byte = *(((char *) data) + i);
#endif

		seq_putc(s, byte);
	}
}

static void qdrv_show_wps_intf(struct seq_file *s, void *data, uint32_t num)
{
	struct net_device *ndev = qdrv_wps_button_get_dev();
	seq_printf(s, "%s", ndev->name);
}

static void
qdrv_control_get_br_isolate(struct seq_file *s, void *data, u32 num)
{
	struct qdrv_mac *mac = (struct qdrv_mac *)data;
	struct qdrv_wlan *qw;
	uint32_t val;

	qw = qdrv_control_wlan_get(mac);
	if (unlikely(!qw))
		return;

	val = ((qw->br_isolate_vid << 16) | qw->br_isolate);

	seq_write(s, &val, sizeof(val));
}

static int qdrv_command_get(struct device *dev, int argc, char *argv[])
{
	struct qdrv_cb *qcb;
	struct qdrv_mac *mac;
	struct qdrv_wlan *qw;
	struct ieee80211com *ic = NULL;

	if (!dev) {
		return -1;
	}

	qcb = dev_get_drvdata(dev);
	if (!qcb) {
		return -1;
	}

	if (argc < 3) {
		DBGPRINTF_E("Invalid number of arguments\n");
		return -1;
	}

	mac = qdrv_control_mac_get(dev, argv[1]);
	if (!mac) {
		DBGPRINTF_E("mac not found\n");
		return -1;
	}

	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return -1;
	}

	ic = &qw->ic;

	if(strcmp(argv[2], "stats") == 0) {
		qdrv_soc_stats(qcb, mac);

	} else if(strcmp(argv[2], "info") == 0) {
		qdrv_control_set_show(qdrv_show_info, mac, 1, 1);

	} else if(strcmp(argv[2], "info_log") == 0) {
		qdrv_show_info(NULL, mac, 1);

	} else if(strcmp(argv[2], "muc_stats") == 0) {
		qdrv_muc_stats_printlog(qcb, mac, &(qw->ic), argc - 3, &argv[3]);

	} else if(strcmp(argv[2], "assoc_info") == 0) {
		qdrv_wlan_get_assoc_info(qw);

	} else if(strcmp(argv[2], "show_assoc") == 0) {
		qdrv_control_show_assoc(mac, argc - 3, &argv[3]);

	} else if(strcmp(argv[2], "assoc_q") == 0) {
		qdrv_wlan_get_assoc_queue_info(qw);

	} else if(strcmp(argv[2], "chip_id") == 0) {
		u32 local_chip_id = chip_id();
		printk("Chip ID: %u (0x%x)\n", local_chip_id, local_chip_id );

	} else if (strcmp(argv[2], "mucfw") == 0) {
		qdrv_control_set_show(qdrv_show_mucfw, (void *) qcb, 1, 1);

	} else if (strcmp(argv[2], "fwver") == 0) {
		qdrv_control_set_show(qdrv_show_fw_ver, ic, 1, 1);

	} else if (strcmp(argv[2], "platform_id") == 0) {
		qdrv_control_set_show(qdrv_show_platform_id, (void *) qcb, 1, 1);

	} else if (strcmp(argv[2], "noise") == 0) {
		qcb->value_from_muc = qdrv_muc_get_noise(mac, ic);
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);

	} else if (strcmp(argv[2], "rssi") == 0) {
		unsigned int rf_chain = 0;

		if (argc > 3) {
			if(sscanf(argv[3], "%u", &rf_chain) != 1) {
				rf_chain = 0;
			}
		}

		qcb->value_from_muc = qdrv_muc_get_rssi_by_chain(mac, ic, rf_chain);
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);

	} else if (strcmp(argv[2], "rx_gain") == 0) {
		qcb->value_from_muc = qdrv_muc_get_rx_gain_fields(mac, ic);
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);

	} else if (strcmp(argv[2], "max_gain") == 0) {
#if TOPAZ_FPGA_PLATFORM
		qcb->value_from_muc = (0 && qdrv_read_mem(RUBY_QT3_BB_TD_MAX_GAIN));
#else
		qcb->value_from_muc = qdrv_read_mem(RUBY_QT3_BB_TD_MAX_GAIN);
#endif
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "ext_lna_gain") == 0) {
		shared_params *sp = qtn_mproc_sync_shared_params_get();

		if (sp == NULL) {
			DBGPRINTF_E("shared_params struct not yet published\n");
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return(-1);
		}

		if (sp->ext_lna_gain < QTN_EXT_LNA_GAIN_MAX) {
			printk("ext_lna_gain : %d.", sp->ext_lna_gain);
		} else {
			printk("ext_lna_gain(%d) is invalid.", sp->ext_lna_gain);
		}

		if (sp->ext_lna_bypass_gain < QTN_EXT_LNA_GAIN_MAX) {
			printk("        ext_lna_bypass_gain : %d.\n", sp->ext_lna_bypass_gain);
		} else {
			printk("        ext_lna_bypass_gain(%d) is invalid.\n", sp->ext_lna_bypass_gain);
		}

	} else if (strcmp(argv[2], "node_info") == 0) {
		ieee80211_dump_nodes(&ic->ic_sta);

	} else if (strcmp(argv[2], "phy_stat") == 0) {
		unsigned int stat_index = 0;
		int stat_value;

		if (argc < 4) {
			return -1;
		}
		if (argc > 4) {
			if (sscanf(argv[4], "%u", &stat_index) != 1) {
				return -1;
			}

		}

		if (qdrv_muc_get_phy_stat(mac, ic, argv[3], stat_index, &stat_value)) {
			return -1;
		}
		qcb->value_from_muc = stat_value;
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);

	} else if (strcmp(argv[2], "debug_flag") == 0) {
		if (DBG_LOG_FUNC_TEST(QDRV_LF_DUMP_RX_PKT)) {
			qdrv_show_debug_level("RX pkt");
		} else {
			printk("RX pkt dump disabled.\n");
		}
		if (DBG_LOG_FUNC_TEST(QDRV_LF_DUMP_TX_PKT)) {
			qdrv_show_debug_level("TX pkt");
		} else {
			printk("TX pkt dump disabled.\n");
		}
	} else if (strcmp(argv[2], "vendor_fix") == 0) {
                uint32_t vendor_fix = ic->ic_vendor_fix;
                qdrv_control_set_show(qdrv_show_vendor_fix, (void *) qw, 1, 1);

                DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX, "Current vendor fix flag is 0x%x\n",
			  ic->ic_vendor_fix);
                DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"DHCP fix is %s\n",
			  (vendor_fix & VENDOR_FIX_BRCM_DHCP) ? "enabled" : "disabled");
                DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"Replace IGMP src mac is %s\n",
			  (vendor_fix & VENDOR_FIX_BRCM_REPLACE_IGMP_SRCMAC) ? "enabled" : "disabled");
                DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"Replace IP src mac is %s\n",
			  (vendor_fix & VENDOR_FIX_BRCM_REPLACE_IP_SRCMAC) ? "enabled" : "disabled");
                DBGPRINTF(DBG_LL_CRIT, QDRV_LF_PKT_RX,"Drop STA IGMP query is %s\n",
			  (vendor_fix & VENDOR_FIX_BRCM_DROP_STA_IGMPQUERY) ? "enabled" : "disabled");
	} else if (strcmp(argv[2], "hw_options") == 0) {
		qcb->value_from_muc = qdrv_soc_get_hw_options();
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "hw_desc") == 0) {
		qdrv_control_set_show(qdrv_show_hw_desc, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "rf_chipid") == 0) {
		qcb->value_from_muc = qw->rf_chipid;
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "rf_chip_verid") == 0) {
		qcb->value_from_muc = qw->rf_chip_verid;
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "auc_stats") == 0) {
		qdrv_control_set_show(qdrv_show_auc_stats, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "dsp_stats") == 0) {
		qdrv_control_set_show(qdrv_show_dsp_stats, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "uc_tx_stats") == 0) {
		qdrv_control_set_show(qdrv_show_uc_tx_stats, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "uc_rx_stats") == 0) {
		qdrv_control_set_show(qdrv_show_uc_rx_stats, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "wmm_ac_map") == 0) {
		qdrv_control_set_show(qdrv_control_show_wmm_ac_map, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "br_isolate") == 0) {
		qdrv_control_set_show(qdrv_control_get_br_isolate, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "dscp_8021p_map") == 0) {
		qdrv_control_set_show(qdrv_control_show_dscp_8021p_map, (void *)mac, 1, 1);
	} else if (strcmp(argv[2], "power_table_checksum") == 0) {
		struct qdrv_power_table_checksum_entry *p_entry = qcb->power_table_ctrl.checksum_list;
		while (p_entry) {
			if (strcmp(argv[3], p_entry->fname) == 0) {
				break;
			}
			p_entry = p_entry->next;
		}
		qcb->power_table_ctrl.reading_checksum = p_entry;
		qdrv_control_set_show(qdrv_show_checksum, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "checksum_list") == 0) {
		struct qdrv_power_table_checksum_entry *p_entry = qcb->power_table_ctrl.checksum_list;
		while (p_entry) {
			printk("%s  %s\n", p_entry->checksum, p_entry->fname);
			p_entry = p_entry->next;
		}
	} else if (strcmp(argv[2], "power_selection") == 0) {
		qcb->value_from_muc = qcb->power_table_ctrl.power_selection;
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "power_recheck") == 0) {
		qcb->value_from_muc = qcb->power_table_ctrl.power_recheck;
		qdrv_control_set_show(qdrv_show_muc_value, (void *) qcb, 1, 1);
	} else if (strcmp(argv[2], "vlan_config") == 0) {
		if (argc != 4)
			return -1;
		qdrv_control_set_show(qdrv_control_show_vlan_config, (void *)argv[3], 1, 1);
	} else if(strcmp(argv[2], "mac_reserve") == 0) {
		if (argc != 4)
			return -1;
		qdrv_control_set_show(qdrv_mac_reserve_show, argv[3], 1, 1);
	} else if (strcmp(argv[2], "wbsp_ctrl") == 0) {
		printk("qdrv_wbsp_ctrl = %d\n", qdrv_wbsp_ctrl);
	} else if (strcmp(argv[2], "vap_default_state") == 0) {
                qdrv_control_set_show(qdrv_show_vap_default_state, (void *) qw, 1, 1);
	} else if (strcmp(argv[2], "core_dump_size") == 0) {
		qdrv_control_set_show(qdrv_show_core_dump_size, &qdrv_crash_log_len, 1, 1);
	} else if (strcmp(argv[2], "core_dump") == 0) {
		if (!qdrv_crash_log) {
			DBGPRINTF_E("QDRV: core dump not saved\n");
		} else {
			qdrv_control_set_show(qdrv_show_core_dump, qdrv_crash_log, qdrv_crash_log_len,
				qdrv_crash_log_len);
		}
	} else if (strcmp(argv[2], "wps_intf") == 0) {
		qdrv_control_set_show(qdrv_show_wps_intf, (void *)qw, 1, 1);
	} else {
		DBGPRINTF_E("The get request \"%s\" is unknown.\n", argv[2]);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

static int read_proc_hardware_revision(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	char *p = page;
	uint16_t hw_rev = get_hardware_revision();
	const char *hwrev_str;

	if (offset > 0) {
		*eof = 1;
		return 0;
	}

	hwrev_str = qdrv_soc_get_hw_rev_desc(hw_rev);

	p += sprintf(p, "%s\n", hwrev_str);

	return p - page;
}

static int read_proc_carrier_id(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
	int len;
	char *p = page;
	if (offset > 0) {
		*eof = 1;
		return 0;
	}
	p += sprintf(p, "%d\n",g_carrier_id);

	len = p - page;
	return len;
}

#if defined(QTN_DEBUG)
static int qdrv_command_dbg(struct device *dev, int argc, char *argv[])
{
	u32 module_id = 0;
	u32 dbg_func_mask = 0;
	u32 dbg_log_level = 0;
	u32 arg = 0;
	int i = 0;

	if (argc < 2) {
		goto error;
	}
	if (strcmp(argv[1], "set") == 0) {
		if (argc < 5) {
			goto error;
		}
		for (i = 0; i < (DBG_LM_MAX - 1); i++) {
			if (!strcmp(argv[2], dbg_module_name_entry[i].dbg_module_name)){
				module_id = dbg_module_name_entry[i].dbg_module_id;
				break;
			}
		}
		if (module_id == 0) {
			goto error;
		}

		if (sscanf(argv[3], "%x", &dbg_func_mask) != 1) {
			goto error;
		}

		if (sscanf(argv[4], "%u", &dbg_log_level) != 1) {
			goto error;
		}

		if (module_id != DBG_LM_QMACFW) {
			g_dbg_log_module |= BIT(module_id - 1);
			g_dbg_log_func[module_id - 1] = dbg_func_mask;
			g_dbg_log_level[module_id - 1] = dbg_log_level;
		} else {
			struct qdrv_cb *qcb;
			qcb = dev_get_drvdata(dev);
			struct qdrv_mac *mac = &qcb->macs[0];
			struct qdrv_wlan *qw = (struct qdrv_wlan *) mac->data;
			arg = dbg_func_mask << 4 | dbg_log_level;
			qdrv_hostlink_msg_cmd(qw, IOCTL_DEV_CMD_SET_DRV_DBG, arg);
		}
	} else if (strcmp(argv[1], "get") == 0) {
			for (i = 0; i < (DBG_LM_MAX - 1); i++) {
				if (!strcmp(argv[2], dbg_module_name_entry[i].dbg_module_name)) {
					module_id = dbg_module_name_entry[i].dbg_module_id;
					break;
				}
			}
			if (module_id == 0){
				goto error;
			}
			if (module_id != DBG_LM_QMACFW) {
				printk("module name: %s\n", argv[2]);
				printk("function mask: 0x%08x\n", g_dbg_log_func[module_id - 1]);
				printk("debug level: %u\n", g_dbg_log_level[module_id - 1]);
			} else {
				struct qdrv_cb *qcb;
				qcb = dev_get_drvdata(dev);
				struct qdrv_mac *mac = &qcb->macs[0];
				struct qdrv_wlan *qw = (struct qdrv_wlan *) mac->data;
				qdrv_hostlink_msg_cmd(qw, IOCTL_DEV_CMD_GET_DRV_DBG, 0);
			}
	}
	return(0);

error:
	printk("Usage\n");
	printk("    dbg set <module name> <function mask> <debug level>\n");
	printk("    dbg get <module name>\n");
	printk("Module names:\n");
	for (i = 0; i < (DBG_LM_MAX - 1); i++) {
		printk("    %s\n", dbg_module_name_entry[i].dbg_module_name);
	}
	return(0);
}

#endif

static void qdrv_command_pktlogger_usage (void) {
	printk("Usage:\n"
			"    pktlogger 0 show\n"
			"    pktlogger 0 start <log type> [<interval>]\n"
			"    pktlogger 0 stop <log type>\n"
			"    pktlogger 0 set <parameter> <value>\n"
			"\n"
			"Parameters:\n"
			"    show        display current settings\n"
			"    start       start logging for the specified log type\n"
			"    <interval>  logging frequency\n"
			"    stop        stop logging for the specified log type\n"
			"    <log type>  stats, event, radar, txbf, sysmsg, mem, vsp, phy_stats\n"
			"    set         set a parameter value\n"
			"    <parameter> dstmac, dstip, dstport, srcip, wifimac, interface\n");
}

static int qdrv_command_pktlogger(struct device *dev, int argc, char *argv[])
{
	struct qdrv_mac *mac;
	struct qdrv_wlan *qw;
	int ret = 0;

	if (argc < 2) {
		qdrv_command_pktlogger_usage();
		return 0;
	}

	mac = qdrv_control_mac_get(dev, argv[1]);
	if (mac == NULL) {
		DBGPRINTF_E("mac NULL\n");
		return -1;
	}

	qw = qdrv_control_wlan_get(mac);
	if (!qw) {
		return -1;
	}

	if (strcmp(argv[2], "set") == 0) {
		if (argc < 5) {
			qdrv_command_pktlogger_usage();
			return 0;
		}
		ret = qdrv_pktlogger_set(qw, argv[3], argv[4]);
	} else if (strcmp(argv[2], "show") == 0) {
		qdrv_pktlogger_show(qw);
	} else if (strcmp(argv[2], "start") == 0) {
		unsigned interval = 0;
		if (argc < 4) {
			qdrv_command_pktlogger_usage();
			return 0;
		}
		if (argc >= 5) {
			if (sscanf(argv[4], "%d", &interval) != 1) {
				return 0;
			}
		}
		ret = qdrv_pktlogger_start_or_stop(qw, argv[3], 1, interval);
	} else if (strcmp(argv[2], "stop") == 0) {
		if (argc < 4) {
			qdrv_command_pktlogger_usage();
			return 0;
		}
		ret = qdrv_pktlogger_start_or_stop(qw, argv[3], 0, 0);
	} else {
		qdrv_command_pktlogger_usage();
	}
	return ret;

}

static int qdrv_command_rf_reg_dump(struct device *dev, int argc, char *argv[])
{
        struct qdrv_mac *mac;
        struct qdrv_wlan *qw;
        u_int32_t arg = 0;

        if (argc < 2) {
                qdrv_command_memdbg_usage();
                return 0;
        }
        mac = qdrv_control_mac_get(dev, argv[1]);
        if (mac == NULL) {
                return -1;
        }

        qw = qdrv_control_wlan_get(mac);
        if (!qw) {
                return -1;
        }

	qdrv_hostlink_msg_cmd(qw, IOCTL_DEV_CMD_RF_REG_DUMP, arg);

        return 0;
}


int qdrv_control_output(struct device *dev, char *buf)
{
	struct qdrv_cb *qcb;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Get the private device data */
	qcb = dev_get_drvdata(dev);

	/* Show the return code from the previous command */
	if(qcb->rc == 0)
	{
		strcpy(buf, "ok\n");
	}
	else
	{
		sprintf(buf, "error %d\n", qcb->rc);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(strlen(buf));
}

static int qdrv_parse_args(char *string, int argc, char *argv[])
{
	char *p = string;
	int n = 0;
	char *end;
	int i;

	/* Skip leading white space up to first argument */
	while((*p != '\0') && isspace(*p) && p++);

	/* Check for empty string */
	if (*p == '\0') {
		/* Empty string!! */
		return(0);
	}

	for (i = 0; i < argc; i++) {
		/* Save argument */
		argv[i] = p;

		/* Increment the number of arguments we have found */
		n++;

		/* Skip argument */
		while ((*p != '\0') && !isspace(*p) && p++);

		/* Remember the end of argument */
		end = p;

		/* Skip leading white spaces up to next argument(s) */
		while ((*p != '\0') && isspace(*p) && p++);

		/* Terminate argument */
		*end = '\0';

		/* Check for end of arguments */
		if (*p == '\0') {
			break;
		}
	}

	/* Check if there are arguments left */
	if (*p != '\0') {
		/* Too many arguments */
		return(-1);
	}

	return(n);
}

int qdrv_control_input(struct device *dev, char *buf, unsigned int count)
{
	int n;
	int found = -1;
	struct qdrv_cb *qcb;
	char *argv[19];
	int argc;
	char *p;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Get the private device data */
	qcb = dev_get_drvdata(dev);
	if (!qcb) {
		return -1;
	}

	/* Make sure it fits into our command buffer as a '\0' */
	/* terminated string.                                  */
	if (count >= (sizeof(qcb->command) - 1)) {
		DBGPRINTF_E("Command is too large (%d >= %ld)\n",
			count, sizeof(qcb->command) - 1);
		qcb->rc = -1;
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	/* Copy to a buffer to make a proper C string */
	memcpy(qcb->command, buf, count);
	qcb->command[count] = '\0';

	/* Kill '\n' if there is one */
	p = strrchr(qcb->command, '\n');
	if (p) {
		*p = '\0';
	}

	/* Parse the arguments */
	argc = qdrv_parse_args(qcb->command, 23, argv);

	/* Make sure we got at least a command */
	if (argc == 0) {
		DBGPRINTF_E("No command specified\n");
		qcb->rc = -2;
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	if (argc < 0) {
		DBGPRINTF_E("Too many arguments specified.\n");
		qcb->rc = -3;
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	/* Try to match the commands to the input */
	for (n = 0; found < 0 && n < COMMAND_TABLE_SIZE; n++) {
		if (strcmp(argv[0], s_command_table[n].command) == 0) {
			found = n;
			break;
		}
	}

	/* Call the function if we found a match */
	if (found >= 0) {
		if ((*s_command_table[found].fn)(dev, argc, argv) < 0) {
			DBGPRINTF_E("Failed to execute command \"%s\" (%d)\n",
				s_command_table[found].command, found);

			qcb->rc = -4;
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return(-1);
		}
	} else {
		DBGPRINTF_E("Command \"%s\" is not recognized\n",qcb->command);

		qcb->rc = -5;
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	qcb->rc = 0;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

void qdrv_control_set_show(void (*fn)(struct seq_file *s, void *data, u32 num),
	void *data, int start_num, int decr)
{
	if (data == NULL) {
		DBGPRINTF_E("qdrv_control_set_show called with NULL address\n" );
		return;
	}

	if (down_interruptible(&s_output_sem)) {
		return;
	}

	if (s_output_qcb == NULL) {
		/* Nothing to output */
		up(&s_output_sem);
		return;
	}

	/* Get the number of items to read */
	s_output_qcb->read_start_num = start_num;
	s_output_qcb->read_num = start_num;
	s_output_qcb->read_decr = decr;
	s_output_qcb->read_data = data;
	s_output_qcb->read_show = fn;

	up(&s_output_sem);

	return;
}

static void *qdrv_seq_start(struct seq_file *s, loff_t *pos)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (down_interruptible(&s_output_sem)) {
		return NULL;
	}

	if (s_output_qcb == NULL) {
		/* Nothing to output */
		up(&s_output_sem);
		return NULL;
	}

	up(&s_output_sem);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	/* Return the number of items to read */
	if (*pos >= 0 && *pos <= s_output_qcb->read_start_num / s_output_qcb->read_decr) {
		s_output_qcb->read_num = s_output_qcb->read_start_num -
					*pos * s_output_qcb->read_decr;
		return (void*)s_output_qcb->read_num;
	} else {
		return NULL;
	}
}

static void *qdrv_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (down_interruptible(&s_output_sem)) {
		return NULL;
	}

	if (s_output_qcb == NULL) {
		/* Nothing to output */
		up(&s_output_sem);
		return NULL;
	}

	(*pos)++;
	s_output_qcb->read_num -= s_output_qcb->read_decr;

	if (s_output_qcb->read_num <= 0) {
		/* The iterator is done */
		up(&s_output_sem);
		return NULL;
	}

	up(&s_output_sem);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return (void*)s_output_qcb->read_num;
}

static void qdrv_seq_stop(struct seq_file *s, void *v)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (down_interruptible(&s_output_sem)) {
		return;
	}

	if (s_output_qcb == NULL) {
		/* Not valid any more */
		up(&s_output_sem);
		return;
	}

	if (v == NULL)
		s_output_qcb->read_show = NULL;

	up(&s_output_sem);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return;
}

static int qdrv_seq_show(struct seq_file *s, void *v)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (down_interruptible(&s_output_sem)) {
		return -ERESTARTSYS;
	}

	if (s_output_qcb == NULL) {
		/* Nothing to output */
		up(&s_output_sem);
		return -1;
	}

	if (s_output_qcb->read_show != NULL) {
		(*s_output_qcb->read_show)(s, s_output_qcb->read_data, (u32) v);
	}

	up(&s_output_sem);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

/*
 * MU related commands:
 *    syntax: mu {set | get | dbg | clr} [sub-cmd] [mac_addr] [options]
 *
 *    1. set station group id and user_position
 *       mu set grp {mac_addr} {grp_id} {position}
 *    2. clear station group id
 *       mu clr grp {mac_addr} {grp_id} {position}
 */
static int qdrv_command_mu(struct device *dev, int argc, char *argv[])
{
	int i;
	qdrv_mu_cmd mu_cmd;
	uint8_t mac[IEEE80211_ADDR_LEN];
	uint8_t grp = 0, delete = 0;
	struct ieee80211_node *ni = NULL, *ni1 = NULL;
	struct ieee80211com *ic = qdrv_get_ieee80211com(dev);
	int res = -1;

	if (!ieee80211_swfeat_is_supported(SWFEAT_ID_MU_MIMO, 1))
		return -1;

	for (i = 0; i < argc; i++) {
		printk("arg[%d] %s\n", i, argv[i]);
	}

	/* parse the cmd chains */
	if (strcmp(argv[1], "set") == 0) {
		mu_cmd = QDRV_MU_CMD_SET;
	} else if (strcmp(argv[1], "get") == 0) {
		mu_cmd = QDRV_MU_CMD_GET;
	} else if (strcmp(argv[1], "dbg") == 0) {
		mu_cmd = QDRV_MU_CMD_DBG;
	} else if (strcmp(argv[1], "clr") == 0) {
		mu_cmd = QDRV_MU_CMD_CLR;
		delete = 1;
	} else if (strcmp(argv[1], "sta0") == 0) {
		mu_cmd = QDRV_MU_CMD_FIRST_IN_GROUP_SELECTION;
	} else {
		goto mu_exit;
	}

	if (mu_cmd == QDRV_MU_CMD_SET || mu_cmd == QDRV_MU_CMD_GET) {
		if (qdrv_parse_mac(argv[3], mac) < 0) {
			printk("Error mac address\n");
			goto mu_exit;
		}

		ni = ieee80211_find_node(&ic->ic_sta, mac);
		if (!ni) {
			printk("Can't find node\n");
			goto mu_exit;
		}
	} else if (mu_cmd == QDRV_MU_CMD_FIRST_IN_GROUP_SELECTION) {
		if (qdrv_parse_mac(argv[2], mac) < 0) {
			printk("Error mac address\n");
			goto mu_exit;
		}
	}

	/* parse subcmd & the parameters */
	switch (mu_cmd) {
	case QDRV_MU_CMD_CLR:
	{
		if (argc < 4) {
			goto mu_exit;
		}

		volatile struct qtn_txbf_mbox *txbf_mbox = qtn_txbf_mbox_get();
		grp = _atoi(argv[3]);
		if (!(grp > 0 && grp < ARRAY_SIZE(txbf_mbox->mu_grp_man_rank) + 1)) {
			printk("Group %u is out of range\n", grp);
			goto mu_exit;
		}
		grp--;
		txbf_mbox->mu_grp_man_rank[grp].u0_aid = 0;
		txbf_mbox->mu_grp_man_rank[grp].u1_aid = 0;
		txbf_mbox->mu_grp_man_rank[grp].rank = 0;
		break;
	}
	case QDRV_MU_CMD_GET:
		printk("MU grp: "
			"%02x%02x%02x%02x%02x%02x%02x%02x\n"
			"MU pos: %02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x\n",
			ni->ni_mu_grp.member[7],
			ni->ni_mu_grp.member[6],
			ni->ni_mu_grp.member[5],
			ni->ni_mu_grp.member[4],
			ni->ni_mu_grp.member[3],
			ni->ni_mu_grp.member[2],
			ni->ni_mu_grp.member[1],
			ni->ni_mu_grp.member[0],
			ni->ni_mu_grp.pos[15],
			ni->ni_mu_grp.pos[14],
			ni->ni_mu_grp.pos[13],
			ni->ni_mu_grp.pos[12],
			ni->ni_mu_grp.pos[11],
			ni->ni_mu_grp.pos[10],
			ni->ni_mu_grp.pos[9],
			ni->ni_mu_grp.pos[8],
			ni->ni_mu_grp.pos[7],
			ni->ni_mu_grp.pos[6],
			ni->ni_mu_grp.pos[5],
			ni->ni_mu_grp.pos[4],
			ni->ni_mu_grp.pos[3],
			ni->ni_mu_grp.pos[2],
			ni->ni_mu_grp.pos[1],
			ni->ni_mu_grp.pos[0]);
			break;
	case QDRV_MU_CMD_SET:
	{
		if (argc < 7) {
			goto mu_exit;
		}
		if (qdrv_parse_mac(argv[4], mac) < 0) {
			printk("Error mac address\n");
			goto mu_exit;
		}

		ni1 = ieee80211_find_node(&ic->ic_sta, mac);
		if (!ni1) {
			printk("Can't find node\n");
			goto mu_exit;
		}
		int32_t rank = _atoi(argv[6]);

		volatile struct qtn_txbf_mbox *txbf_mbox = qtn_txbf_mbox_get();
		grp = _atoi(argv[5]);
		if (!(grp > 0 && grp < ARRAY_SIZE(txbf_mbox->mu_grp_man_rank) + 1)) {
			printk("Group %u is out of range\n", grp);
			goto mu_exit;
		}
		grp--;
		txbf_mbox->mu_grp_man_rank[grp].u0_aid= IEEE80211_AID(ni->ni_associd);
		txbf_mbox->mu_grp_man_rank[grp].u1_aid= IEEE80211_AID(ni1->ni_associd);
		txbf_mbox->mu_grp_man_rank[grp].rank = rank;
		break;
	}
	case QDRV_MU_CMD_FIRST_IN_GROUP_SELECTION:
	{
		struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
		if (vap) {
			ieee80211_param_to_qdrv(vap, IEEE80211_PARAM_FIRST_STA_IN_MU_SOUNDING, 0,
						mac, sizeof(mac));
		}

		break;
	}
	default:
		goto mu_exit;
		break;
	}

	res = 0;
mu_exit:
	if (res == -1) {
		printk("Usage: mu {set | get | clr | dbg | sta0} [sub-cmd] [mac_addr] [options]\n");
	}

	if (ni) {
		ieee80211_free_node(ni);
	}

	if (ni1) {
		ieee80211_free_node(ni1);
	}

	return(0);

}

static struct seq_operations s_qdrv_seq_ops =
{
	.start = qdrv_seq_start,
	.next = qdrv_seq_next,
	.stop = qdrv_seq_stop,
	.show = qdrv_seq_show,
};

static int qdrv_proc_open(struct inode *inode, struct file *file)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(seq_open(file, &s_qdrv_seq_ops));
}

static struct file_operations s_qdrv_proc_ops =
{
	.owner   = THIS_MODULE,
	.open    = qdrv_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

int qdrv_control_init(struct device *dev)
{
	struct qdrv_cb *qcb;
	struct proc_dir_entry *entry, *version_entry, *carrier_entry;
	u_int32_t auc_sram_start, auc_sram_end, auc_sram_size, auc_sram_bank_end;
	int i;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	DBGPRINTF_N("qdrv wbsp: %d\n", qdrv_wbsp_ctrl);

	/* Get the private device data */
	qcb = dev_get_drvdata(dev);

	if ((entry = create_proc_entry("qdrvdata", S_IFREG | 0666, NULL)) == NULL) {
		DBGPRINTF_E("Failed to create \"/proc/qdrvdata\"\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	init_MUTEX(&s_output_sem);
	s_output_qcb = qcb;

	entry->proc_fops = &s_qdrv_proc_ops;

	version_entry = create_proc_read_entry("hw_revision", 0, NULL, read_proc_hardware_revision, NULL);
	if (version_entry == NULL) {
		DBGPRINTF_E("Failed to create \"/proc/hw_revision\"\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	carrier_entry = create_proc_read_entry("carrier_id", 0, NULL, read_proc_carrier_id, NULL);
	if (carrier_entry == NULL) {
		DBGPRINTF_E("Failed to create \"/proc/carrier_id\"\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	spin_lock_init(&qdrv_event_lock);
	memset(qdrv_event_log_table,0,sizeof(qdrv_event_log_table));
	QDRV_EVENT("Qdrv Log Init");

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	//hzw: debug print out the SRAM layout
	auc_sram_start = RUBY_SRAM_BEGIN + CONFIG_ARC_AUC_SRAM_BASE;
	auc_sram_end = RUBY_SRAM_BEGIN + CONFIG_ARC_AUC_SRAM_END;
	auc_sram_size = CONFIG_ARC_AUC_SRAM_SIZE;

	printk("Auc SRAM start 0x%08x end 0x%08x size %d\n", auc_sram_start, auc_sram_end, auc_sram_size);
	auc_sram_bank_end = auc_sram_start + RUBY_SRAM_BANK_SIZE;
	for (i = 0; i < 4; i++) {
		printk("Auc SRAM bank %d start 0x%08x end 0x%08x\n", i, auc_sram_start, auc_sram_bank_end);
		auc_sram_start = auc_sram_bank_end;
		auc_sram_bank_end = auc_sram_start + RUBY_SRAM_BANK_SIZE;
		if (auc_sram_start >= auc_sram_end)
			break;
	}
	return(0);
}

int qdrv_control_exit(struct device *dev)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (down_interruptible(&s_output_sem)) {
		return(-ERESTARTSYS);
	}

	s_output_qcb = NULL;

	up(&s_output_sem);

	remove_proc_entry("qdrvdata", NULL);
	remove_proc_entry("hw_revision", NULL);
	remove_proc_entry("carrier_id", NULL);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return(0);
}

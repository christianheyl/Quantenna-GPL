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

#include <linux/device.h>
#include <linux/if_vlan.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_hal.h"
#include "qdrv_muc.h"
#include "qdrv_uc_print.h"
#include "qdrv_dsp.h"
#include "qdrv_auc.h"
#include <qtn/registers.h>
#include <qtn/shared_params.h>
#include <qtn/txbf_mbox.h>
#include <qtn/qtn_bb_mutex.h>
#include <qtn/bootcfg.h>
#include <net80211/if_ethersubr.h>
#include <qtn/qtn_vlan.h>
#include <asm/board/board_config.h>
#include "qdrv_comm.h"
#include "qdrv_wlan.h"
#include "qdrv_radar.h"
#include "qdrv_vap.h"
#include "qdrv_config.h"
#include "qdrv_bld.h"
#include "qdrv_mu.h"
#include "qtn/qdrv_sch.h"
#include <qtn/lhost_muc_comm.h>
#include <common/ruby_config.h>
#include <qtn/qtn_global.h>
#include <qtn/hardware_revision.h>
#include <qtn/topaz_fwt_sw.h>

extern unsigned int g_catch_fcs_corruption;
extern unsigned int g_qos_q_merge;

static int tqe_sem_en = 0;
module_param(tqe_sem_en, int, S_IRWXU);

extern void qtn_show_info_register(void *fn);
extern void qtn_show_info_unregister(void);
extern int qtn_get_hw_config_id(void);

struct _shared_params_alloc
{
	struct shared_params	params;
	struct qtn_txbf_mbox	txbf_mbox;
	struct qtn_muc_dsp_mbox	muc_dsp_mbox;
	struct qtn_bb_mutex	bb_mutex;
	struct qtn_csa_info	csa;
	struct qtn_samp_chan_info	chan_sample;
	struct qtn_scan_chan_info	chan_scan;
	struct qtn_scs_info_set		scs_info_set;
	struct qtn_remain_chan_info	remain_chan;
	struct qtn_ocac_info	ocac;
	struct qtn_meas_chan_info	chan_meas;
#if QTN_SEM_TRACE
	struct qtn_sem_trace_log        sem_trace_log;
#endif
#if defined(QBMPS_ENABLE)
	struct qtn_bmps_info	bmps;
#endif
};

int qdrv_soc_cb_size(void)
{
	return(sizeof(struct qdrv_cb));
}

int qdrv_soc_start_vap(struct qdrv_cb *qcb, int devid, struct qdrv_mac *mac,
	char *name, uint8_t *mac_addr, int opmode, int flags)
{
	if ((mac->enabled != 1) || (mac->data == NULL)) {
		DBGPRINTF_E("MAC unit not enabled\n");
		return(-1);
	}

	if (qdrv_wlan_start_vap((struct qdrv_wlan*)mac->data, name, mac_addr,
				devid, opmode, flags) < 0) {
		DBGPRINTF_E("Failed to start VAP\n");
		return(-1);
	}

	return(0);
}

int qdrv_soc_stop_vap(struct qdrv_cb *qcb, struct qdrv_mac *mac, struct net_device *vdev)
{
	if ((mac->enabled != 1) || (mac->data == NULL)) {
		DBGPRINTF_E("MAC unit not enabled\n");
		return -1;
	}

	if (qdrv_wlan_stop_vap(mac, vdev) < 0) {
		DBGPRINTF_E("qdrv_wlan_stop_vap failed\n");
		return -1;
	}

	return 0;
}

int qdrv_soc_stats(void *data, struct qdrv_mac *mac)
{
	if ((mac->enabled != 1) || (mac->data == NULL)) {
		DBGPRINTF_E("MAC unit not enabled\n");
		return(-1);
	}

	if (qdrv_wlan_stats(mac) < 0) {
		DBGPRINTF_E("Failed to get statistics for VAP\n");
		return(-1);
	}

	return(0);
}

static struct shared_params *params_bus = NULL;

u_int32_t qdrv_soc_get_hostlink_mbox(void)
{
	return soc_shared_params->m2l_hostlink_mbox;
}

char *qdrv_soc_get_hw_desc(enum hw_opt_t bond_opt)
{
	char *desc = "unknown";
	uint8_t rf_chipid = soc_shared_params->rf_chip_id;

	if (bond_opt == 0)
		bond_opt = soc_shared_params->hardware_options;

	/*
	 * These strings are in use by customers and should not be changed.
	 * The platform ID must appear at the end of the string.
	 */
	switch (bond_opt) {
	case HW_OPTION_BONDING_TOPAZ_QD840:
		if (rf_chipid == CHIPID_DUAL)
			desc = "4x4 11n/ac Data Only QD842";
		else
			desc = "4x4 11n/ac Data Only QD840";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV840:
		if (rf_chipid == CHIPID_DUAL)
			desc = "4x4 11ac FO RGMII/PCIe QV842";
		else
			desc = "4x4 11ac FO RGMII/PCIe QV840";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV840_2X4:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x4 11ac FO RGMII/PCIe QV842";
		else
			desc = "2x4 11ac FO RGMII/PCIe QV840";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV840C:
		if (rf_chipid == CHIPID_DUAL)
			desc = "4x4 11ac FO RGMII/PCIe QV842C";
		else
			desc = "4x4 11ac FO RGMII/PCIe QV840C";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV860:
		if (rf_chipid == CHIPID_DUAL)
			desc = "4x4 11ac FO RGMII DBDC QV862";
		else
			desc = "4x4 11ac FO RGMII DBDC QV860";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV860_2X2:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x2 11ac FO RGMII DBDC QV862";
		else
			desc = "2x2 11ac FO RGMII DBDC QV860";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV860_2X4:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x4 11ac FO RGMII DBDC QV862";
		else
			desc = "2x4 11ac FO RGMII DBDC QV860";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV860_3X3:
		if (rf_chipid == CHIPID_DUAL)
			desc = "3x3 11ac FO RGMII DBDC QV862";
		else
			desc = "3x3 11ac FO RGMII DBDC QV860";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV880:
		if (rf_chipid == CHIPID_DUAL)
			desc = "4x4 11ac FO RGMII DBDC QV882";
		else
			desc = "4x4 11ac FO RGMII DBDC QV880";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV880_2X2:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x2 11ac FO RGMII DBDC QV882";
		else
			desc = "2x2 11ac FO RGMII DBDC QV880";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV880_2X4:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x4 11ac FO RGMII DBDC QV882";
		else
			desc = "2x4 11ac FO RGMII DBDC QV880";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV880_3X3:
		if (rf_chipid == CHIPID_DUAL)
			desc = "3x3 11ac FO RGMII DBDC QV882";
		else
			desc = "3x3 11ac FO RGMII DBDC QV880";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV920:
		if (rf_chipid == CHIPID_DUAL)
			desc = "4x4 11ac PCIe Memoryless QV922";
		else
			desc = "4x4 11ac PCIe Memoryless QV920";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV920_2X4:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x4 11ac PCIe Memoryless QV922";
		else
			desc = "2x4 11ac PCIe Memoryless QV920";
		break;
	case HW_OPTION_BONDING_TOPAZ_QV940:
		if (rf_chipid == CHIPID_DUAL)
			desc = "2x4 11ac FO RGMII/PCIe QV942";
		else
			desc = "2x4 11ac FO RGMII/PCIe QV940";
		break;
	}

	return desc;
}

char *qdrv_soc_get_hw_id(enum hw_opt_t bond_opt)
{
	char *hw_desc = qdrv_soc_get_hw_desc(bond_opt);

	/* Last string in the hardware desc is the ID */
	return strrchr(hw_desc, ' ') + 1;
}

uint32_t qdrv_soc_get_hw_options(void)
{
	if (!soc_shared_params)
		return 0;

	return soc_shared_params->hardware_options;
}

static const char *qdrv_hw_ver_descs[] = {
	[HARDWARE_REVISION_UNKNOWN] = "unknown",
	[HARDWARE_REVISION_RUBY_A] = "bbic3_rev_a",
	[HARDWARE_REVISION_RUBY_B] = "bbic3_rev_b_c",
	[HARDWARE_REVISION_RUBY_D] = "bbic3_rev_d",
	[HARDWARE_REVISION_TOPAZ_A] = "bbic4_rev_a0",
	[HARDWARE_REVISION_TOPAZ_B] = "bbic4_rev_a1",
	[HARDWARE_REVISION_TOPAZ_A2] = "bbic4_rev_a2"
};

const char *qdrv_soc_get_hw_rev_desc(uint16_t hw_rev)
{
	if (hw_rev >= ARRAY_SIZE(qdrv_hw_ver_descs))
		hw_rev = HARDWARE_REVISION_UNKNOWN;

	return qdrv_hw_ver_descs[hw_rev];
}
EXPORT_SYMBOL(qdrv_soc_get_hw_rev_desc);

static void qdrv_soc_revoke_params(void)
{
	if (soc_shared_params) {
		dma_free_coherent(
			NULL,
			sizeof(struct _shared_params_alloc),
			container_of(soc_shared_params, struct _shared_params_alloc, params),
			(dma_addr_t)container_of(params_bus, struct _shared_params_alloc, params));
		soc_shared_params = params_bus = NULL;
	}

	qtn_mproc_sync_shared_params_set(0);
}

static int read_hardware_revision(void)
{
	/*
	 * BB should be active and out of reset. ok to query version register
	 *
	 * check that soft reset is low, and global enable is high
	 */
	int ret = HARDWARE_REVISION_UNKNOWN;

	qtn_bb_mutex_enter(QTN_LHOST_SOC_CPU);

	if (readl(RUBY_QT3_BB_GLBL_SOFT_RST) == 0x0) {
		ret = _read_hardware_revision();
	} else {
		printk(KERN_ERR "%s called when BB in soft reset\n", __FUNCTION__);
	}

	qtn_bb_mutex_leave(QTN_LHOST_SOC_CPU);

	return ret;
}

static uint8_t
get_bootcfg_calstate(void)
{
	char tmpbuf[256];
	char *varstart;
	int calstate = QTN_CALSTATE_DEFAULT;

	varstart = bootcfg_get_var("calstate", tmpbuf);
	if (varstart != NULL) {
		if (sscanf(varstart, "=%d", &calstate) != 1) {
			calstate = QTN_CALSTATE_DEFAULT;
		}
	}

	return calstate;
}

static uint8_t get_bootcfg_power_recheck(void)
{
	char tmpbuf[256];
	char *varstart;
	int recheck = 1;

	varstart = bootcfg_get_var("power_recheck", tmpbuf);
	if (varstart != NULL) {
		sscanf(varstart, "=%d", &recheck);
	}

	return recheck;
}

static uint8_t
get_bootcfg_post_mask(void)
{
	char tmpbuf[256];
	char *varstart;
	int post_mask = 0;

	varstart = bootcfg_get_var("post_mask", tmpbuf);
	if (varstart != NULL) {
		if (sscanf(varstart, "=%d", &post_mask) != 1) {
			post_mask = 0;
		}
	}

	return post_mask;
}

static int
get_ext_lna_gain_from_bootcfg(const char *var)
{
	char tmpbuf[256];
	char *varstart;
	int  value;
	int  lna_gain = QTN_EXT_LNA_GAIN_MAX;

	varstart = bootcfg_get_var(var, tmpbuf);
	if (varstart != NULL) {
		if (sscanf(varstart, "=%d", &value) == 1) {
			if ((-127 < value) && (value < QTN_EXT_LNA_GAIN_MAX)) {
				lna_gain = value;
			}
		}
	}

	return lna_gain;
}

static int
get_bootcfg_tx_power_cal(void)
{
	char tmpbuf[256];
	char *varstart;
	int tx_power_cal = 0;

	varstart = bootcfg_get_var("tx_power_cal", tmpbuf);
	if (varstart != NULL) {
		sscanf(varstart, "=%d", &tx_power_cal);
	}

	return tx_power_cal;
}

static int
get_bootcfg_min_tx_power(void)
{
        char tmpbuf[256];
        char *varstart;
        int min_tx_power = 0;

        varstart = bootcfg_get_var("min_tx_power", tmpbuf);
        if (varstart != NULL) {
                sscanf(varstart, "=%d", &min_tx_power);
        }

        return min_tx_power;
}

static int
get_bootcfg_max_tx_power(void)
{
        char tmpbuf[256];
        char *varstart;
        int max_tx_power = 0;
        
        varstart = bootcfg_get_var("max_tx_power", tmpbuf);
        if (varstart != NULL) {
                sscanf(varstart, "=%d", &max_tx_power);
        }

        return max_tx_power;
}

static int
qdrv_soc_publish_params(struct qdrv_cb *qcb)
{
	int ret = 0;
	int current_wifi_hw = 0;
	int current_rf_chip_id;
	struct _shared_params_alloc *params_alloc = NULL, *params_alloc_bus = NULL;

	/* Guard againt second call to function */
	qdrv_soc_revoke_params();

	/* Allocate what we are going to publish.
	 * Pointer can be used by any processor in system,
	 * so published pointer must be "bus" pointer.
	 * If other processors want to convert pointer for example
	 * to be non-cacheable, they must remap it themself.
	 * Structure must be allocated using dma_alloc_coherent().
	 */
	if((params_alloc = (struct _shared_params_alloc*)dma_alloc_coherent(NULL,
		sizeof(struct _shared_params_alloc), (dma_addr_t*)(&params_alloc_bus), GFP_KERNEL)) == NULL)
	{
		DBGPRINTF_E("%s: failed to alloc soc_shared_params\n", __FUNCTION__);
		ret = -1;
		goto bad;
	}
	memset(params_alloc, 0, sizeof(*params_alloc));

	/* Initialize shared soc_shared_params structure */
	soc_shared_params = &params_alloc->params;
	params_bus = &params_alloc_bus->params;
	soc_shared_params->hardware_id = qtn_get_hw_config_id();
	memcpy(soc_shared_params->fw_version, QDRV_BLD_NAME,
			MIN(QTN_FW_VERSION_LENGTH, strlen(QDRV_BLD_NAME)));

	/* Initialize beamforming message box structure */
	soc_shared_params->txbf_mbox_lhost = &params_alloc->txbf_mbox;
	soc_shared_params->txbf_mbox_bus = &params_alloc_bus->txbf_mbox;
	soc_shared_params->muc_dsp_mbox_lhost = &params_alloc->muc_dsp_mbox;
	soc_shared_params->muc_dsp_mbox_bus = &params_alloc_bus->muc_dsp_mbox;
	soc_shared_params->muc_dsp_mbox_lhost->muc_to_dsp_mbox = QTN_TXBF_MBOX_BAD_IDX;
	soc_shared_params->muc_dsp_mbox_lhost->dsp_to_muc_mbox = QTN_TXBF_MBOX_BAD_IDX;
	soc_shared_params->txbf_mbox_lhost->muc_to_dsp_ndp_mbox = QTN_TXBF_MBOX_BAD_IDX;
	int i;
	for (i = 0; i < ARRAY_SIZE(soc_shared_params->txbf_mbox_lhost->muc_to_dsp_action_frame_mbox); i++) {
		soc_shared_params->txbf_mbox_lhost->muc_to_dsp_action_frame_mbox[i] = QTN_TXBF_MBOX_BAD_IDX;
	}

	soc_shared_params->txbf_mbox_lhost->dsp_to_host_mbox = QTN_TXBF_MBOX_BAD_IDX;

	/* Initialize RIFS mode structure */
	soc_shared_params->bb_mutex_lhost = &params_alloc->bb_mutex;
	soc_shared_params->bb_mutex_bus = &params_alloc_bus->bb_mutex;

	/* deferred channel switch */
	soc_shared_params->csa_lhost = &params_alloc->csa;
	soc_shared_params->csa_bus = &params_alloc_bus->csa;

	/* cca scan */
	soc_shared_params->chan_sample_lhost = &params_alloc->chan_sample;
	soc_shared_params->chan_sample_bus = &params_alloc_bus->chan_sample;

	soc_shared_params->chan_scan_lhost = &params_alloc->chan_scan;
	soc_shared_params->chan_scan_bus = &params_alloc_bus->chan_scan;

	/* SCS info */
	soc_shared_params->scs_info_lhost = &params_alloc->scs_info_set;
	soc_shared_params->scs_info_bus = &params_alloc_bus->scs_info_set;

	/* remain channel info */
	soc_shared_params->remain_chan_lhost = &params_alloc->remain_chan;
	soc_shared_params->remain_chan_bus = &params_alloc_bus->remain_chan;
	/* ocac */
	soc_shared_params->ocac_lhost = &params_alloc->ocac;
	soc_shared_params->ocac_bus = &params_alloc_bus->ocac;

	/* Measurement info */
	soc_shared_params->chan_meas_lhost = &params_alloc->chan_meas;
	soc_shared_params->chan_meas_bus = &params_alloc_bus->chan_meas;

	/* remain channel info */
	soc_shared_params->remain_chan_lhost = &params_alloc->remain_chan;
	soc_shared_params->remain_chan_bus = &params_alloc_bus->remain_chan;

#if QTN_SEM_TRACE
	/* semaphore calltrace log */
	soc_shared_params->sem_trace_log_lhost = &params_alloc->sem_trace_log;
	soc_shared_params->sem_trace_log_bus = &params_alloc_bus->sem_trace_log;
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE,
			"semaphore calltrace log buffer: %p %p\n",
			soc_shared_params->sem_trace_log_lhost, soc_shared_params->sem_trace_log_bus);
#endif

	/* FIXME: to be replaced */
	soc_shared_params->vdev_lhost = vdev_tbl_lhost;
	soc_shared_params->vdev_bus = (struct qtn_vlan_dev **)virt_to_bus(vdev_tbl_bus);
	soc_shared_params->vport_lhost = vport_tbl_lhost;
	soc_shared_params->vport_bus = (struct qtn_vlan_dev **)virt_to_bus(vport_tbl_bus);
	soc_shared_params->vlan_info = (struct qtn_vlan_info *)virt_to_bus(&qtn_vlan_info);
	soc_shared_params->auc.vdev_bus = soc_shared_params->vdev_bus;
	soc_shared_params->auc.vport_bus = soc_shared_params->vport_bus;

#if defined(QBMPS_ENABLE)
	/* bmps info */
	soc_shared_params->bmps_lhost = &params_alloc->bmps;
	soc_shared_params->bmps_bus = &params_alloc_bus->bmps;
#endif

	soc_shared_params->ipmac_table_bus = (struct topaz_ipmac_uc_table *)ipmac_hash_bus;

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE,
			"txbf_mbox %p %p shared soc_shared_params %p %p bb_mutex %p %p\n",
		soc_shared_params->txbf_mbox_bus, soc_shared_params->txbf_mbox_lhost, soc_shared_params, params_bus,
			soc_shared_params->bb_mutex_bus, soc_shared_params->bb_mutex_lhost);

	/* Fill shared parameters structure */
	if (get_board_config( BOARD_CFG_WIFI_HW, &current_wifi_hw ) != 0) {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE,
				"%s: get board config returned error status\n", __FUNCTION__);
		/* This error is relatively harmless, so carry on. */
	}

	/* Initialise flag for TQE hang WAR */
#ifdef CONFIG_TOPAZ_PCIE_TARGET
	soc_shared_params->tqe_sem_en = tqe_sem_en;
	soc_shared_params->auc.auc_tqe_sem_en = tqe_sem_en;
#else
	soc_shared_params->tqe_sem_en = 0;
	soc_shared_params->auc.auc_tqe_sem_en = 0;
#endif

	printk("%s: parames->tqe_sem_en %d, auc_tqe_sem_en %d\n", __FUNCTION__, soc_shared_params->tqe_sem_en,
			soc_shared_params->auc.auc_tqe_sem_en);

	soc_shared_params->lh_wifi_hw = current_wifi_hw;
	if (get_board_config( BOARD_CFG_RFIC, &current_rf_chip_id ) != 0) {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE,
				"%s: get board config returned error status\n", __FUNCTION__);
		/* This error is relatively harmless, so carry on. */
	}
	soc_shared_params->rf_chip_id = current_rf_chip_id;
	printk("..... Current RFIC Chip ID -- %d\n", soc_shared_params->rf_chip_id );


	memcpy(soc_shared_params->lh_mac_0, qcb->mac0, sizeof(soc_shared_params->lh_mac_0));
	memcpy(soc_shared_params->lh_mac_1, qcb->mac1, sizeof(soc_shared_params->lh_mac_1));
	soc_shared_params->lh_chip_id = (u_int16_t) readl( RUBY_SYS_CTL_CSR );
	soc_shared_params->lh_num_devices = 1;

	soc_shared_params->uc_flags = g_catch_fcs_corruption;
	soc_shared_params->uc_flags |= g_qos_q_merge;

	soc_shared_params->hardware_revision = read_hardware_revision();
	soc_shared_params->hardware_options = get_bootcfg_bond_opt();

	soc_shared_params->shortrange_scancnt = get_bootcfg_scancnt();
	soc_shared_params->ext_lna_gain = get_ext_lna_gain_from_bootcfg("ext_lna_gain");
	soc_shared_params->ext_lna_bypass_gain = get_ext_lna_gain_from_bootcfg("ext_lna_bypass_gain");
	soc_shared_params->tx_power_cal = get_bootcfg_tx_power_cal();
	soc_shared_params->min_tx_power = get_bootcfg_min_tx_power();
	soc_shared_params->max_tx_power = get_bootcfg_max_tx_power();

	/* This slow ethernet check is done twice in qdrv as sc is initialized at this point */
	soc_shared_params->slow_ethernet_war = board_slow_ethernet();
	soc_shared_params->iot_tweaks = QTN_IOT_DEFAULT_TWEAK;
	soc_shared_params->calstate = get_bootcfg_calstate();
	soc_shared_params->post_rfloop = (get_bootcfg_post_mask() & 0x4) ? 1 : 0;

	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_TRACE,
			"System rev: %08X\n",
			soc_shared_params->lh_chip_id);
	/*
	 * CPUs will access the shared parameters.
	 */
	qtn_mproc_sync_shared_params_set((struct shared_params*)params_bus);

	return ret;

bad:
	qdrv_soc_revoke_params();
	return ret;
}

static void qtn_show_info (void) {
	printk("\nFirmware build version: %s", QDRV_BLD_NAME);
	printk("\nFirmware configuration: %s", QDRV_CFG_TYPE);
	printk("\nHardware ID           : %d\n", qtn_get_hw_config_id());
}

int qdrv_start_dsp_only(struct device *dev)
{
	struct qdrv_cb *qcb;
	int retval = 0;

#ifdef QTN_RC_ENABLE_HDP
	/* for RC only: if not PCIE_TQE_INTR_WORKAROUND ignore the dsp fw download */
	if (!((readl(RUBY_SYS_CTL_CSR) & 0xff) == TOPAZ_BOARD_REVB))
		return retval;
#endif

	qcb = (struct qdrv_cb *) dev_get_drvdata(dev);
	qcb->dev = dev;

	/* Bring up the DSP */
	retval = qdrv_dsp_init(qcb);
	if(retval == 0)
		qcb->resources |= QDRV_RESOURCE_DSP;

	return retval;
}

int qdrv_soc_init(struct device *dev)
{
	struct qdrv_cb *qcb;
	int retval = 0;
	int error_code = 0;

	/* Get the private device data */
	qcb = (struct qdrv_cb *) dev_get_drvdata(dev);

	/* MATS FIX This should be initialized better somewhere else */
	qcb->instances = 1;

	/* Make sure we have firmware image specified for the MuC */
	if(qcb->muc_firmware[0] == '\0')
	{
		error_code = 0x00000001;
		retval = -ENODEV;
		goto error;
	}

	/* Set the device in the control block */
	qcb->dev = dev;

	/* initiate the power control block */
	qcb->power_table_ctrl.power_recheck = get_bootcfg_power_recheck();

	/*
	 * Reset the SoC.
	 *
	 * Must be called *before* qdrv_soc_publish_params, so that BB is out of reset
	 * when reading the version register
	 */
	hal_reset();

	/* Publish SoC parameters */
	if (qdrv_soc_publish_params(qcb) < 0)
	{
		error_code = 0x00000002;
		retval = -ENOMEM;
		goto error;
	}

	/* Register scheduler */
	if(qdrv_sch_module_init() != 0)
	{
		error_code = 0x00000004;
		retval = -ENODEV;
		goto error;
	}

	/* Initialize the MAC 0 device */
	if(qdrv_mac_init(&qcb->macs[0], qcb->mac0, 0, IRQ_MAC0_0, &qcb->params) < 0)
	{
		error_code = 0x00000010;
		retval = -ENODEV;
		goto error;
	}

	/* Mark that we have successfully allocated a resource */
	qcb->resources |= QDRV_RESOURCE_MAC0;

	/* Ruby (ARC host) does not support MAC 1 */
#ifndef CONFIG_ARCH_ARC
	/* Initialize the MAC 1 device */
	if(qdrv_mac_init(&qcb->macs[1], qcb->mac1, 1, IRQ_MAC0_1, &qcb->params) < 0)
	{
		error_code = 0x00000020;
		retval = -ENODEV;
		goto error;
	}

	/* Mark that we have successfully allocated a resource */
	qcb->resources |= QDRV_RESOURCE_MAC1;
#endif

	/* Initialize the message module */
	if(qdrv_comm_init(qcb) < 0)
	{
		error_code = 0x00000040;
		retval = -ENODEV;
		goto error;
	}

	/* Mark that we have successfully allocated a resource */
	qcb->resources |= QDRV_RESOURCE_COMM;

	/* Bring up the DSP */
	if(qdrv_dsp_init(qcb) != 0)
	{
		error_code = 0x00000100;
		retval = -ENODEV;
		goto error;
	}

	/* Mark that we have successfully allocated a resource */
	qcb->resources |= QDRV_RESOURCE_DSP;

	/* Initialise MuC print buf */
	if(qdrv_uc_print_init(qcb) != 0) {
		DBGPRINTF_E("Could not initialise MuC shared print buffer!\n");
	} else {
		qcb->resources |= QDRV_RESOURCE_UC_PRINT;
	}

	/* Bring up the MuC  */
	if(qdrv_muc_init(qcb) != 0)
	{
		error_code = 0x00000080;
		retval = -ENODEV;
		goto error;
	}

	/* Mark that we have successfully allocated a resource */
	qcb->resources |= QDRV_RESOURCE_MUC;

	/* Bring up the AuC  */
	if(qdrv_auc_init(qcb) != 0)
	{
		error_code = 0x00000100;
		retval = -ENODEV;
		goto error;
	}
	if (qdrv_mu_stat_init(qcb) != 0)
	{
		error_code = 0x00000200;
		retval = -ENODEV;
		goto error;
	}
	/* Mark that we have successfully allocated a resource */
	qcb->resources |= QDRV_RESOURCE_AUC;

	qtn_show_info_register(qtn_show_info);

	/* That went well .... */
	return(0);

error:

	DBGPRINTF_E("Failed with error code 0x%08x\n", error_code);

	/* Clean up as much as we can */
	(void) qdrv_soc_exit(dev);

	/* Not so good .... */
	return(retval);
}

int qdrv_soc_exit(struct device *dev)
{
	struct qdrv_cb *qcb;

	/* Get the private device data */
	qcb = dev_get_drvdata(dev);

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE, "Begin resources 0x%08x\n", qcb->resources);

	qtn_show_info_unregister();

	(void)qdrv_mu_stat_exit(qcb);
	/* Release resources in reverse order */
	if(qcb->resources & QDRV_RESOURCE_AUC)
	{
		(void) qdrv_auc_exit(qcb);
		qcb->resources &= ~QDRV_RESOURCE_AUC;
	}

	/* Release resources in reverse order */
	if(qcb->resources & QDRV_RESOURCE_DSP)
	{
		(void) qdrv_dsp_exit(qcb);
		qcb->resources &= ~QDRV_RESOURCE_DSP;
	}

	if(qcb->resources & QDRV_RESOURCE_MUC)
	{
		(void) qdrv_muc_exit(qcb);
		qcb->resources &= ~QDRV_RESOURCE_MUC;
	}

	if(qcb->resources & QDRV_RESOURCE_UC_PRINT)
	{
		(void) qdrv_uc_print_exit(qcb);
		qcb->resources &= ~QDRV_RESOURCE_UC_PRINT;
	}

	if(qcb->resources & QDRV_RESOURCE_MAC0)
	{
		(void) qdrv_mac_exit(&qcb->macs[0]);
		qcb->resources &= ~QDRV_RESOURCE_MAC0;
	}

	if(qcb->resources & QDRV_RESOURCE_MAC1)
	{
		(void) qdrv_mac_exit(&qcb->macs[1]);
		qcb->resources &= ~QDRV_RESOURCE_MAC1;
	}

	if(qcb->resources & QDRV_RESOURCE_COMM)
	{
		(void) qdrv_comm_exit(qcb);
		qcb->resources &= ~QDRV_RESOURCE_COMM;
	}

	if(qcb->resources & QDRV_RESOURCE_WLAN)
	{
		(void) qdrv_wlan_exit(&qcb->macs[0]);
		qcb->resources &= ~QDRV_RESOURCE_WLAN;
	}


	qdrv_sch_module_exit();

	qdrv_soc_revoke_params();

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE, "End resources 0x%08x\n", qcb->resources);

	return(0);
}

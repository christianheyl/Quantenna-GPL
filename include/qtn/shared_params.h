/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2010 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Quantenna Communications Inc                               **
**  File        : shared_params.h                                            **
**  Description :                                                            **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH1*/

#ifndef _SHARED_PARAMS_H_
#define _SHARED_PARAMS_H_

#ifndef SYSTEM_BUILD
#include "../common/ruby_config.h"
#endif
#include <qtn/shared_defs.h>
#include <qtn/topaz_shared_params.h>

/*
 * Forward declarations.
 */
struct qtn_txbf_mbox;
struct qtn_bb_mutex;
struct qtn_csa_info;
struct qtn_samp_chan_info;
struct qtn_scs_info;
struct qtn_scs_info_set;
struct qtn_ocac_info;

#define QTN_SEM_TRACE 1
/*
 * As DSP accesses sempahore with high frequency, to minimize performance impact, it would be better to
 * enable DSP sem trace log only in non-release build.
 */
#define QTN_SEM_TRACE_DSP 0
#if QTN_SEM_TRACE
struct qtn_sem_trace_log;
#endif

#ifndef IEEE80211_ADDR_LEN
	#define IEEE80211_ADDR_LEN	6
#endif

#define MUC_FCS_CORRUPTION	0x00000001
#define MUC_QOS_Q_MERGE		0x00000002

#define MUC_PROFILE_DROP	0x08000000
#define MUC_PROFILE_DCACHE	0x10000000
#define MUC_PROFILE_P		0x20000000
#define MUC_PROFILE_EP		0x40000000
#define MUC_PROFILE_IPTR	0x80000000

#define MUC_BOOT_WAIT_SECS	20

#define CHIP_ID_RUBY			0x30
#define CHIP_ID_TOPAZ			0x40
#define REV_ID_RUBY_A			0x0
#define REV_ID_RUBY_B			0x1
#define REV_ID_RUBY_D			0x3
#define REV_ID_TOPAZ_A			0x0
#define REV_ID_TOPAZ_B			0x1
#define REV_ID_TOPAZ_A2			0x3
#define HARDWARE_REVISION_UNKNOWN	0
#define HARDWARE_REVISION_RUBY_A	1
#define HARDWARE_REVISION_RUBY_B	2
#define HARDWARE_REVISION_RUBY_D	3
#define HARDWARE_REVISION_TOPAZ_A	4
#define HARDWARE_REVISION_TOPAZ_B	5
#define HARDWARE_REVISION_TOPAZ_A2	6
#define CHIP_ID_MASK			0xF0
#define CHIP_ID_SW_MASK			0xC0	/* bits set in sw, for downgrade only */
#define CHIP_REV_ID_MASK		0x0F

#define HW_OPTION_BONDING_RUBY_PROD	(HARDWARE_REVISION_RUBY_D << 8)
#define HW_OPTION_BONDING_RUBY_UNKNOWN	(HW_OPTION_BONDING_RUBY_PROD | 0x00000000)
#define HW_OPTION_BONDING_RUBY_2x4STA	(HW_OPTION_BONDING_RUBY_PROD | 0x00000001)
#define HW_OPTION_BONDING_RUBY_4SS	(HW_OPTION_BONDING_RUBY_PROD | 0x00000002)

#define HW_PLAT_TOPAZ_QV860		0x00
#define HW_PLAT_TOPAZ_QV860_2X2		0x80	/* downgrade only */
#define HW_PLAT_TOPAZ_QV860_2X4		0x40	/* downgrade only */
#define HW_PLAT_TOPAZ_QV860_3X3		0xF0	/* downgrade only */
#define HW_PLAT_TOPAZ_QD840		0x01
#define HW_PLAT_TOPAZ_QV880		0x32
#define HW_PLAT_TOPAZ_QV880_2X2		0xb2	/* downgrade only */
#define HW_PLAT_TOPAZ_QV880_2X4		0x72	/* downgrade only */
#define HW_PLAT_TOPAZ_QV880_3X3		0xF2	/* downgrade only */
#define HW_PLAT_TOPAZ_QV920		0x03
#define HW_PLAT_TOPAZ_QV920_2X4		0x43	/* downgrade only */
#define HW_PLAT_TOPAZ_QV840		0x04
#define HW_PLAT_TOPAZ_QV840_2X4		0x44	/* downgrade only */
#define HW_PLAT_TOPAZ_QV940		0x05
#define HW_PLAT_TOPAZ_QV840C		0x06

#define HW_OPTION_BONDING_TOPAZ_PROD	(HARDWARE_REVISION_TOPAZ_B << 8)
enum hw_opt_t {
	HW_OPTION_BONDING_TOPAZ_QV860 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV860),
	HW_OPTION_BONDING_TOPAZ_QV860_2X2 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV860_2X2),
	HW_OPTION_BONDING_TOPAZ_QV860_2X4 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV860_2X4),
	HW_OPTION_BONDING_TOPAZ_QV860_3X3 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV860_3X3),
	HW_OPTION_BONDING_TOPAZ_QD840 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QD840),
	HW_OPTION_BONDING_TOPAZ_QV880 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV880),
	HW_OPTION_BONDING_TOPAZ_QV880_2X2 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV880_2X2),
	HW_OPTION_BONDING_TOPAZ_QV880_2X4 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV880_2X4),
	HW_OPTION_BONDING_TOPAZ_QV880_3X3 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV880_3X3),
	HW_OPTION_BONDING_TOPAZ_QV920 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV920),
	HW_OPTION_BONDING_TOPAZ_QV920_2X4 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV920_2X4),
	HW_OPTION_BONDING_TOPAZ_QV840 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV840),
	HW_OPTION_BONDING_TOPAZ_QV840_2X4 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV840_2X4),
	HW_OPTION_BONDING_TOPAZ_QV940 = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV940),
	HW_OPTION_BONDING_TOPAZ_QV840C = (HW_OPTION_BONDING_TOPAZ_PROD | HW_PLAT_TOPAZ_QV840C),
};

#define HW_OPTION_BONDING_NOT_SET	0xFFFFFFFF

typedef struct shared_params
{
	u_int32_t		tqe_sem_en; /*replaced for TQE SWR lh_flags; */
	u_int16_t		lh_chip_id;
	u_int8_t		rf_chip_id;
	u_int8_t		vco_lock_detect_mode;
	u_int8_t		lh_wifi_hw;
	u_int8_t		lh_num_devices;
	u_int8_t		lh_mac_0[ IEEE80211_ADDR_LEN ];
	u_int8_t		lh_mac_1[ IEEE80211_ADDR_LEN ];

	u_int32_t		uc_flags;
	u_int32_t		uc_hostlink;

	u_int32_t		m2l_hostlink_mbox;
	u_int32_t		m2l_printbuf_producer;

	u_int64_t		last_chan_sw_tsf;
	u_int32_t		l2m_sem;
	u_int32_t		m2l_sem;

	int			hardware_revision;
	uint32_t		hardware_options;
	uint8_t			swfeat_map[SWFEAT_MAP_SIZE];
	int8_t			shortrange_scancnt;
	uint8_t			slow_ethernet_war;
	uint8_t			calstate;
	uint8_t			post_rfloop;
#define QTN_IOT_INTEL5100_TWEAK		0x00000001
#define QTN_IOT_INTEL6200_TWEAK		0x00000002
#define QTN_IOT_INTEL6300_TWEAK		0x00000004
#define QTN_IOT_INTELFD_TWEAK		0x00000008
#define QTN_IOT_INTEL_SEND_NCW_ACTION   0x00000010   /* IOT action: send Notify Channel Width Action frame to Intel */
#define QTN_IOT_BCM_TWEAK		0x00000020   /* Disable aggregation on Broadcom MBP clients */
#define QTN_IOT_INTEL_NOAGG2TXCHAIN_TWEAK     0x00000040   /* NO Aggregation & 2 Tx chain restriction for some Intel */
#define QTN_IOT_BCM_NO_3SS_MCS_TWEAK	0x00000080   /* Disable 3ss MCS for Broadcom MBP clients */
#define QTN_IOT_BCM_AMSDU_DUTY_TWEAK	0x00000100   /* AMSDU duty cycle tweak */
#define QTN_IOT_BCM_MBA_AMSDU_TWEAK	0x00000200   /* MBA doesn't work with 7.9k AMSDU with security mode */
#define QTN_IOT_RLNK_NO_3SS_MCS_TWEAK	0x00000400   /* Disable 3ss MCS for Ralink clients */
#define QTN_IOT_RTK_NO_AMSDU_TWEAK	0x00000800   /* Disable A-MSDU for Realtek devices */
#define QTN_IOT_DEFAULT_TWEAK		(QTN_IOT_BCM_MBA_AMSDU_TWEAK | \
					 QTN_IOT_BCM_AMSDU_DUTY_TWEAK | \
					 QTN_IOT_RLNK_NO_3SS_MCS_TWEAK | \
					 QTN_IOT_RTK_NO_AMSDU_TWEAK \
					)
	uint32_t		iot_tweaks;

	struct qtn_txbf_mbox*	txbf_mbox_lhost;
	struct qtn_txbf_mbox*	txbf_mbox_bus;

	struct qtn_muc_dsp_mbox *muc_dsp_mbox_lhost;
	struct qtn_muc_dsp_mbox *muc_dsp_mbox_bus;

	struct qtn_bb_mutex*	bb_mutex_lhost;
	struct qtn_bb_mutex*	bb_mutex_bus;

	struct qtn_csa_info*	csa_lhost;
	struct qtn_csa_info*	csa_bus;

	struct qtn_samp_chan_info*	chan_sample_lhost;
	struct qtn_samp_chan_info*	chan_sample_bus;

	struct qtn_scan_chan_info*	chan_scan_lhost;
	struct qtn_scan_chan_info*	chan_scan_bus;

	struct qtn_scs_info_set*	scs_info_lhost;
	struct qtn_scs_info_set*	scs_info_bus;

	struct qtn_remain_chan_info*	remain_chan_lhost;
	struct qtn_remain_chan_info*	remain_chan_bus;
	struct qtn_ocac_info*	ocac_lhost;
	struct qtn_ocac_info*	ocac_bus;

	struct qtn_bmps_info*	bmps_lhost;
	struct qtn_bmps_info*	bmps_bus;

	struct qtn_meas_chan_info*	chan_meas_lhost;
	struct qtn_meas_chan_info*	chan_meas_bus;

#if QTN_SEM_TRACE
	struct qtn_sem_trace_log *sem_trace_log_lhost;
	struct qtn_sem_trace_log *sem_trace_log_bus;
#endif

	struct qtn_vlan_dev **vdev_lhost;
	struct qtn_vlan_dev **vdev_bus;
	struct qtn_vlan_dev **vport_lhost;
	struct qtn_vlan_dev **vport_bus;
	struct topaz_ipmac_uc_table *ipmac_table_bus;
	struct qtn_vlan_info *vlan_info;

#if CONFIG_RUBY_BROKEN_IPC_IRQS
	u_int32_t		m2l_irq[2];
#endif

	void *			p_debug_1;
	int			debug_1_arg;
	void *			p_debug_2;
	int			debug_2_arg;

	u_int32_t		pm_duty_lock;

#define QTN_EXT_LNA_GAIN_MAX	126
	int			ext_lna_gain;
	int			ext_lna_bypass_gain;
	int			tx_power_cal;
	int			hardware_id;
	int			min_tx_power;
	int			max_tx_power;
#define QTN_FW_VERSION_LENGTH	32
	char			fw_version[QTN_FW_VERSION_LENGTH + 1];
#ifndef SYSTEM_BUILD
	shared_params_auc	auc;
#endif

	int			cca_adjusting_flag;
	int			active_tid_num;
	uint32_t		bb_param;
	uint32_t		cs_thresh_base_val;	/* Carrier sense threshold base value */
	uint32_t		qtn_hal_pm_corrupt;
	uint32_t		qtn_hal_pm_corrupt_debug;
	uint32_t                free_airtime;		/* in ms */
} shared_params;

#define QTN_RATE_TRAIN_DATA_LEN		64
#define QTN_RATE_TRAIN_BYTE		0x2A

#endif /* _SHARED_PARAMS_H_ */

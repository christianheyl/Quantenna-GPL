/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2012 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Quantenna Communications Inc                               **
**  File        : topaz_shared_params.h                                            **
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

#ifndef _TOPAZ_SHARED_PARAMS_H_
#define _TOPAZ_SHARED_PARAMS_H_

#include <qtn/mproc_sync_mutex.h>
#include <qtn/qtn_uc_comm.h>
#include <qtn/qtn_wmm_ac.h>

enum shared_params_auc_ipc_cmd
{
	SHARED_PARAMS_IPC_NONE_CMD		= 0,

	/* begining of M2A IPC config commands */
	SHARED_PARAMS_IPC_M2A_CFG_PARAMS_MIN,
	SHARED_PARAMS_IPC_M2A_SLOW_NODE_CREATE_CMD,
	SHARED_PARAMS_IPC_M2A_NODE_CREATE_CMD,
	SHARED_PARAMS_IPC_M2A_NODE_DESTROY_CMD,
	SHARED_PARAMS_IPC_M2A_SLOW_TID_CREATE_CMD,
	SHARED_PARAMS_IPC_M2A_TID_CREATE_CMD,
	SHARED_PARAMS_IPC_M2A_TID_DESTROY_CMD,
	SHARED_PARAMS_IPC_M2A_TID_ACTIVATE_CMD,
	SHARED_PARAMS_IPC_M2A_TID_DEACTIVATE_CMD,
	SHARED_PARAMS_IPC_M2A_TID_CHECK_IDLE_CMD,
	SHARED_PARAMS_IPC_M2A_TID_BA_CTL_CMD,
	SHARED_PARAMS_IPC_M2A_TX_SCALE_CMD,
	SHARED_PARAMS_IPC_M2A_TX_SCALE_BASE_CMD,
	SHARED_PARAMS_IPC_M2A_TX_SCALE_MAX_CMD,
	SHARED_PARAMS_IPC_M2A_TX_AGG_TIMEOUT_CMD,
	SHARED_PARAMS_IPC_M2A_TX_DBG_CMD,
	SHARED_PARAMS_IPC_M2A_TX_QOS_SCH_CMD,
	SHARED_PARAMS_IPC_M2A_TX_AGG_DURATION,
	SHARED_PARAMS_IPC_M2A_FCS_GIVE_CMD,
	SHARED_PARAMS_IPC_M2A_NODE_RATEDATA_CHANGE_CMD,
	SHARED_PARAMS_IPC_M2A_OCS_TX_SUSPEND_CMD,
	SHARED_PARAMS_IPC_M2A_TQEW_DESCR_LIMIT_CMD,
	SHARED_PARAMS_IPC_M2A_ENABLE_VLAN_CMD,
	SHARED_PARAMS_IPC_M2A_MU_GRP_UPDATE_CMD,
	SHARED_PARAMS_IPC_M2A_MU_DBG_FLAG_UPDATE_CMD,
	SHARED_PARAMS_IPC_M2A_MU_AIRTIME_PADDING_UPDATE_CMD,
	/* end of M2A IPC config commands */
	SHARED_PARAMS_IPC_M2A_CFG_PARAMS_MAX,

	SHARED_PARAMS_IPC_M2A_MU_QMAT_UPDATE_CMD,
	SHARED_PARAMS_IPC_M2A_SRESET_BEGIN_CMD,
	SHARED_PARAMS_IPC_M2A_SRESET_END_CMD,
	SHARED_PARAMS_IPC_M2A_PAUSE_ON_CMD,
	SHARED_PARAMS_IPC_M2A_PAUSE_OFF_CMD,

	/*
	 * Following are cmd used in A2M IPC interrupt. Put in same enum so that most code can be
	 * used for both A2M and M2A IPC.
	 */
	SHARED_PARAMS_IPC_A2M_FIRST_CMD = 0x100,
	SHARED_PARAMS_IPC_A2M_AUC_BOOTED_CMD,
	SHARED_PARAMS_IPC_A2M_BA_ADD_START_CMD,
	SHARED_PARAMS_IPC_A2M_PANIC,
	SHARED_PARAMS_IPC_A2M_TDLS_PTI_CMD,
	SHARED_PARAMS_IPC_A2M_BB_RESET,
#if QTN_HDP_MU_FCS_WORKROUND
	SHARED_PARAMS_IPC_A2M_PUSH_WMAC1_FCS,
#endif
	SHARED_PARAMS_IPC_A2M_LAST_CMD,
};

enum qtn_exp_mat_cmd {
	EXP_MAT_DIS_CMD	= 0,
	EXP_MAT_DEL_CMD= EXP_MAT_DIS_CMD,
	EXP_MAT_EN_CMD,
	EXP_MAT_ADD_CMD,
	EXP_MAT_FRZ_CMD,
	EXP_MAT_NUSE_CMD,
};

#define AUC_IPC_CMD_BA_NODE		0x000000FF
#define AUC_IPC_CMD_BA_NODE_S		0
#define AUC_IPC_CMD_BA_TID		0x00000F00
#define AUC_IPC_CMD_BA_TID_S		8
#define AUC_IPC_CMD_BA_STATE		0x0000F000
#define AUC_IPC_CMD_BA_STATE_S		12
#define AUC_IPC_CMD_BA_FLAGS		0xFFFF0000
#define AUC_IPC_CMD_BA_FLAGS_S		16
#define AUC_IPC_CMD_BA_SUBFRM_MAX	0x000000FF
#define AUC_IPC_CMD_BA_SUBFRM_MAX_S	0
#define AUC_IPC_CMD_BA_WINSIZE		0x000FFF00
#define AUC_IPC_CMD_BA_WINSIZE_S	8
#define AUC_IPC_CMD_BA_AMSDU		0x00100000
#define AUC_IPC_CMD_BA_AMSDU_S		20
#define AUC_IPC_CMD_BA_AGG_TIMEOUT	0x0000FFFF
#define AUC_IPC_CMD_BA_AGG_TIMEOUT_S	0
#define AUC_IPC_CMD_AGG_TIMEOUT_UNIT	100	/* us */

#define QTN_BA_ARGS_F_IMPLICIT		BIT(0)
#define QTN_BA_ARGS_F_AMSDU		BIT(1)
#define QTN_BA_ARGS_F_BLOCK_SINGLETON	BIT(2)

#define AUC_IPC_CMD_AGGTIMEOUT_BE	0x0000FFFF
#define AUC_IPC_CMD_AGGTIMEOUT_BE_S	0
#define AUC_IPC_CMD_AGGTIMEOUT_BK	0xFFFF0000
#define AUC_IPC_CMD_AGGTIMEOUT_BK_S	16
#define AUC_IPC_CMD_AGGTIMEOUT_VI	0x0000FFFF
#define AUC_IPC_CMD_AGGTIMEOUT_VI_S	0
#define AUC_IPC_CMD_AGGTIMEOUT_VO	0xFFFF0000
#define AUC_IPC_CMD_AGGTIMEOUT_VO_S	16

/*
 * AuC tx tunable params
 */
#define AUC_QOS_SCH_PARAM	0xF0000000
#define AUC_QOS_SCH_PARAM_S	28
#define AUC_QOS_SCH_VALUE	0x0FFFFFFF
#define AUC_QOS_SCH_VALUE_S	0
#define AUC_QOS_SCH_PARAM_AIRTIME_FAIRNESS	1
#define AUC_QOS_SCH_PARAM_MERCY_RATIO		3
#define AUC_QOS_SCH_PARAM_TID_THROT		4
#define AUC_QOS_SCH_PARAM_AIRTIME_INTRABSS_LOAD_THRSH	5
#define AUC_QOS_SCH_PARAM_AIRTIME_MARGIN	6
#define AUC_QOS_SCH_PARAM_AIRTIME_TWEAK		7
#define AUC_TX_AGG_BASE				8
#define AUC_TX_AGG_FLAG				(AUC_TX_AGG_BASE + 0)
#define AUC_TX_AGG_DYN_EAGER_THRSH		(AUC_TX_AGG_BASE + 1)
#define AUC_TX_AGG_ADAP_SWITCH			(AUC_TX_AGG_BASE + 2)
#define AUC_TX_OPTIM_FLAG				(AUC_TX_AGG_BASE + 3)

#define QTN_AUC_THROT_NODE	0x0FF00000
#define QTN_AUC_THROT_NODE_S	20
#define QTN_AUC_THROT_TID	0x000F0000
#define QTN_AUC_THROT_TID_S	16
#define QTN_AUC_THROT_INTVL	0x0000F800
#define QTN_AUC_THROT_INTVL_S	11
#define QTN_AUC_THROT_QUOTA	0x000007FF
#define QTN_AUC_THROT_QUOTA_S	0

#define QTN_AUC_THROT_INTVL_MAX		(0x1F)
#define QTN_AUC_THROT_INTVL_UNIT	(1 * 5)		/* ms */
#define QTN_AUC_THROT_QUOTA_MAX		(0x7FF)
#define QTN_AUC_THROT_QUOTA_UNIT	(1024 * 5)	/* byte */

#define QTN_AUC_AIRFAIR_DFT	1
#define QTN_AUC_AGG_ADAP_SWITCH_DFT	0
#define QTN_AUC_TQEW_DESCR_LIMIT_PERCENT_DFT 75
#define QTN_AUC_OPTIM_FLAG_DFT	0

/*
 * M2A event setting per-TID flags
 */
#define M2A_TIDFLAG_NODE        0x000000FF
#define M2A_TIDFLAG_NODE_S      0
#define M2A_TIDFLAG_TID         0x00000F00
#define M2A_TIDFLAG_TID_S       8
#define M2A_TIDFLAG_FLAG        0x00FF0000
#define M2A_TIDFLAG_FLAG_S      16
#define M2A_TIDFLAG_VAL         0xFF000000
#define M2A_TIDFLAG_VAL_S       24

enum shared_params_auc_ipc_irq
{
	SHARED_PARAMS_IPC_M2A_SRESET_IRQ	= 0,
	SHARED_PARAMS_IPC_M2A_CONFIG_IRQ,
	SHARED_PARAMS_IPC_M2A_PAUSE_IRQ
};

enum shared_params_a2m_ipc_irq
{
	/*
	 * Currently only use 1 bit of IPC register and use "cmd" to expand the ipc usage.
	 * This makes the top half and bottom half simple.
	 */
	SHARED_PARAMS_IPC_A2M_CFG_IRQ	= 0,
};

/*
 * Command structure for both A2M and M2A IPC
 */
typedef struct shared_params_auc_ipc
{
	uint32_t cmd; /* "enum shared_params_auc_ipc_cmd" type, but want to ensure 32-bit size */
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
	uint32_t ret;
} shared_params_auc_ipc;

struct qtn_auc_per_node_data_s;
struct qtn_auc_misc_data_s;
struct qtn_auc_per_mac_data_s;
struct qtn_auc_mu_grp_tbl_elem_s;
struct qtn_hal_tcm;

typedef struct qtn_shared_node_stats {
	/* Write by Muc only */
	uint32_t qtn_rx_pkts;
	uint32_t qtn_rx_bytes;
	uint32_t qtn_rx_ucast;
	uint32_t qtn_rx_bcast;
	uint32_t qtn_rx_mcast;
	uint32_t qtn_tx_pkts;
	uint32_t qtn_tx_bytes;
	uint32_t qtn_rx_vlan_pkts;

	uint32_t qtn_tx_mcast; /* Lhost */
	uint32_t qtn_muc_tx_mcast; /* Muc */
	/*
	 * The number of dropped data packets failed to transmit through
	 * wireless media for each traffic category(TC).
	 */
	uint32_t qtn_tx_drop_data_msdu[WMM_AC_NUM]; /* AuC */
} qtn_shared_node_stats_t;

typedef struct qtn_shared_vap_stats {
	/* Write by Muc only */
	uint32_t qtn_rx_pkts;
	uint32_t qtn_rx_bytes;
	uint32_t qtn_rx_ucast;
	uint32_t qtn_rx_bcast;
	uint32_t qtn_rx_mcast;
	uint32_t qtn_rx_dropped;
	uint32_t qtn_tx_pkts;
	uint32_t qtn_tx_bytes;

	uint32_t qtn_tx_mcast; /* Lhost */
	uint32_t qtn_muc_tx_mcast; /* Muc */
	uint32_t qtn_tx_dropped; /* Auc */
} qtn_shared_vap_stats_t;

typedef struct shared_params_auc
{
#define SHARED_PARAMS_AUC_CONFIG_ASSERT_EN		BIT(0)
#define SHARED_PARAMS_AUC_CONFIG_PRINT_EN		BIT(1)
	u_int32_t				auc_config;
	u_int32_t				a2l_printbuf_producer;
	uint32_t				auc_tqe_sem_en;
#define SHARED_PARAMS_AUC_IPC_STUB			((shared_params_auc_ipc*)1)
	struct shared_params_auc_ipc		*m2a_ipc;	/* M2A */
	struct shared_params_auc_ipc		*a2m_ipc;	/* A2M */
	/*
	 * 'ma_shared_buf' is used to transfer data btw MuC and AuC in IPC call.
	 * So far it is used to pass node position in node cache and ieee80211
	 * vht group. The buffer size is defined to exactly match those data:
	 * sizeof(struct ieee80211_mu_groups_update)
	 */
#define MA_SHARED_BUF_SIZE	(150)
	uint8_t					(*ma_shared_buf)[MA_SHARED_BUF_SIZE];
	struct qtn_auc_per_node_data_s		**auc_per_node_data_ptr;
	struct qtn_auc_misc_data_s		*auc_misc_data_ptr;
	struct qtn_auc_per_mac_data_s		*auc_per_mac_data_ptr;
	qtn_mproc_sync_mutex			*auc_per_node_mutex;
	struct qtn_hal_tcm			*hal_tcm;
	uint32_t				*auc_last_ilink1_p;
	uint32_t				*auc_last_ilink2_p;
	qtn_shared_node_stats_t			*node_stats;
	qtn_shared_vap_stats_t			*vap_stats;
	uint32_t				*per_ac_traffic_prev_second;
	struct qtn_auc_mu_grp_tbl_elem_s	*mu_grp_tbl;
	struct qtn_hal_tcm                      *hal_wmac1_tcm;
	struct qtn_vlan_dev			**vdev_bus;
	struct qtn_vlan_dev			**vport_bus;
} shared_params_auc;

RUBY_INLINE void
qtn_mproc_sync_auc_ipc_set_arg1(volatile struct shared_params_auc_ipc *ipc, uint32_t arg1)
{
	ipc->arg1 = arg1;
}

RUBY_INLINE void
qtn_mproc_sync_auc_ipc_set_args(volatile struct shared_params_auc_ipc *ipc,
		uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
	ipc->arg1 = arg1;
	ipc->arg2 = arg2;
	ipc->arg3 = arg3;
	ipc->ret = 0;
}

RUBY_INLINE uint32_t
qtn_mproc_sync_auc_ipc_get_arg1(volatile struct shared_params_auc_ipc *ipc)
{
	return ipc->arg1;
}

RUBY_INLINE uint32_t
qtn_mproc_sync_auc_ipc_get_arg2(volatile struct shared_params_auc_ipc *ipc)
{
	return ipc->arg2;
}

RUBY_INLINE uint32_t
qtn_mproc_sync_auc_ipc_get_arg3(volatile struct shared_params_auc_ipc *ipc)
{
	return ipc->arg3;
}

RUBY_INLINE void
qtn_mproc_sync_auc_ipc_set_cmd(volatile struct shared_params_auc_ipc *ipc,
		enum shared_params_auc_ipc_cmd cmd)
{
	ipc->cmd = cmd;
}

RUBY_INLINE int
qtn_mproc_sync_auc_ipc_wait_ready(volatile struct shared_params_auc_ipc *ipc,
		enum shared_params_auc_ipc_cmd cmd, int relax_count, uint32_t loop_count)
{
	uint32_t cnt = 0;

	while (ipc->cmd != cmd) {
		if ((loop_count > 0) && (cnt >= loop_count)) {
			return -1;
		}

		qtn_mproc_sync_mutex_relax(relax_count);

		cnt++;
	}

	return 0;
}

RUBY_INLINE enum shared_params_auc_ipc_cmd
qtn_mproc_sync_auc_ipc_wait_mready(volatile struct shared_params_auc_ipc *ipc,
		int relax_count, int loop_count)
{
	enum shared_params_auc_ipc_cmd cmd;
	int loop = 0;

	while(loop++ < loop_count) {
		cmd = ipc->cmd;
		if (cmd > SHARED_PARAMS_IPC_M2A_CFG_PARAMS_MIN &&
				cmd < SHARED_PARAMS_IPC_M2A_CFG_PARAMS_MAX) {
			return cmd;
		}

		qtn_mproc_sync_mutex_relax(relax_count);
	}

	return SHARED_PARAMS_IPC_NONE_CMD;
}

RUBY_INLINE int
qtn_mproc_sync_auc_ipc_wait_done(volatile struct shared_params_auc_ipc *ipc,
		int relax_count, uint32_t loop_count)
{
	return qtn_mproc_sync_auc_ipc_wait_ready(ipc,
		SHARED_PARAMS_IPC_NONE_CMD, relax_count, loop_count);
}

RUBY_INLINE void
qtn_mproc_sync_auc_ipc_done(volatile struct shared_params_auc_ipc *ipc)
{
	qtn_mproc_sync_auc_ipc_set_cmd(ipc, SHARED_PARAMS_IPC_NONE_CMD);
}

RUBY_INLINE void
qtn_mproc_sync_auc_ipc_req(volatile struct shared_params_auc_ipc *ipc,
		enum shared_params_auc_ipc_cmd cmd, int relax_count)
{
	qtn_mproc_sync_auc_ipc_set_cmd(ipc, cmd);
	qtn_mproc_sync_auc_ipc_wait_done(ipc, relax_count, 0);
}

RUBY_INLINE void
qtn_mproc_sync_auc_ipc_ack(volatile struct shared_params_auc_ipc *ipc,
		enum shared_params_auc_ipc_cmd cmd, int relax_count)
{
	qtn_mproc_sync_auc_ipc_wait_ready(ipc, cmd, relax_count, 0);
	qtn_mproc_sync_auc_ipc_done(ipc);
}

RUBY_INLINE int
qtn_mproc_sync_auc_ipc_init_wait(volatile struct shared_params_auc *params, int relax_count)
{
	while (!params->m2a_ipc) {
		qtn_mproc_sync_mutex_relax(relax_count);
	}
	return (params->m2a_ipc != SHARED_PARAMS_AUC_IPC_STUB);
}

#define topaz_mgmt_fcs_offset(buf, len)	roundup_ptr(((unsigned long) (buf)) + (len))

#endif /* _TOPAZ_SHARED_PARAMS_H_ */


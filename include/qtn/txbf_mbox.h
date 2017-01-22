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

#ifndef __QTN_TXBF_MBOX_H
#define __QTN_TXBF_MBOX_H

#include "mproc_sync.h"
#include "txbf_common.h"

#define QTN_TXBF_MBOX_BAD_IDX			((u_int32_t)-1)

#define QTN_TXBF_MUC_TO_DSP_MBOX_INT		(0)
#define QTN_TXBF_DSP_TO_HOST_MBOX_INT		(0)
#define QTN_TXBF_DSP_TO_MUC_MBOX_INT		(0)

#define QTN_RATE_MUC_DSP_MSG_RING_SIZE		(32)

/*
 * QTN_MAX_MU_SND_NODES nodes (6) + QTN_MAX_SND_NODES (10) + 3 for IPC cmd 19.
 * This value still causes buffer allocation failure, which is probably due to bad DSP performance.
 * With 3 more there is no allocation failure for 4 STAs case.
 */
#define QTN_TXBF_MUC_DSP_MSG_RING_SIZE		(6 + 10 + 3 + 3)

#define QTN_TXBF_NDP_DATA_BUFS			(1)

/* MU group install/delete IPC from DSP to LHost */
#define QTN_TXBF_DSP_TO_HOST_INST_MU_GRP        1
#define QTN_TXBF_DSP_TO_HOST_DELE_MU_GRP        2

#ifndef __ASSEMBLY__

#define DSP_ACT_RX_DBG_SIZE	10

#if DSP_ENABLE_STATS && !defined(QTN_RC_ENABLE_HDP)
#define DSP_UPDATE_STATS(_a, _b)	(qtn_txbf_mbox_get()->dsp_stats._a += (_b))
#define DSP_SETSTAT(_a, _b)		(qtn_txbf_mbox_get()->dsp_stats._a =  (_b))
#else
#define DSP_UPDATE_STATS(_a, _b)
#define DSP_SETSTAT(_a, _b)
#endif

#if DSP_ENABLE_STATS
struct qtn_dsp_stats {
	uint32_t dsp_ndp_rx;

	/* Per-node DSP stats */

	/* Total number of feedbacks received */
	uint32_t dsp_act_rx[DSP_ACT_RX_DBG_SIZE];

	/* Number of SU feedbacks */
	uint32_t dsp_act_rx_su[DSP_ACT_RX_DBG_SIZE];

	/* Number of MU group selection feedbacks */
	uint32_t dsp_act_rx_mu_grp_sel[DSP_ACT_RX_DBG_SIZE];

	/* Number of MU precoding feedbacks */
	uint32_t dsp_act_rx_mu_prec[DSP_ACT_RX_DBG_SIZE];

	/* Number of bad feedbacks, i.e. those that are not met SU nor MU criteria */
	uint32_t dsp_act_rx_bad[DSP_ACT_RX_DBG_SIZE];

	/*
	 * Number of feedbacks that were not places into the cache due to any reason. Counters for two reasons
	 * are just below
	 */
	uint32_t dsp_act_rx_mu_drop[DSP_ACT_RX_DBG_SIZE];

	/* The number of MU feedback not placed into the cache as the previous one has not been exprired */
	uint32_t dsp_act_rx_mu_nexp[DSP_ACT_RX_DBG_SIZE];

	/* The number of MU feedback not placed into the cache due to cache is locked */
	uint32_t dsp_act_rx_mu_lock_cache[DSP_ACT_RX_DBG_SIZE];

	/*
	 * The number of precoding feedback was released unused, i.e. not participated in QMat calculation.
	 * It means the buddy feedback either have not been received or received after cache expiration time
	 */
	uint32_t dsp_act_rx_mu_rel_nuse[DSP_ACT_RX_DBG_SIZE];

	/* The number of MU feedback for which dsp_qmat_check_act_len is failed */
	uint32_t dsp_act_rx_inval_len[DSP_ACT_RX_DBG_SIZE];

	uint32_t dsp_del_mu_node_rx;
	uint32_t dsp_ipc_in;
	uint32_t dsp_ipc_out;
	uint32_t dsp_sleep_in;
	uint32_t dsp_sleep_out;
	uint32_t dsp_act_tx;
	uint32_t dsp_ndp_discarded;
	uint32_t dsp_ndp_inv_len;
	uint32_t dsp_ndp_max_len;
	uint32_t dsp_ndp_inv_bw;
	uint32_t dsp_act_free_tx;
	uint32_t dsp_inst_mu_grp_tx;
	uint32_t dsp_qmat_invalid;
/* Number or QMat currently installed */
	int32_t dsp_sram_qmat_num;
/*
 * Number of times dsp_sram_qmat_num becomes negative. Non zero value signals that the number
 * of QMat de-installation is more than the number of installations. This is an error condition but not a critical one
 */
	uint32_t dsp_err_neg_qmat_num;
	uint32_t dsp_flag;
	/* Interrupts */
	uint32_t dsp_ipc_int;
	uint32_t dsp_timer_int;
	uint32_t dsp_timer1_int;
	uint32_t dsp_last_int;

	uint32_t dsp_exc;
	/* registers */
	uint32_t dsp_status32;
	uint32_t dsp_status32_l1;
	uint32_t dsp_status32_l2;
	uint32_t dsp_ilink1;
	uint32_t dsp_ilink2;
	uint32_t dsp_blink;
	uint32_t dsp_sp;
	uint32_t dsp_time;

	uint32_t dsp_point;
	uint32_t dsp_stat_bad_stack;

	int16_t dspmu_D_user1[4];
	int16_t dspmu_D_user2[4];
	int16_t dspmu_max_intf_user1;
	int16_t dspmu_max_intf_user2;
	int16_t rank_criteria;
	uint32_t dsp_trig_mu_grp_sel;
	uint32_t dsp_mu_rank_success;
	uint32_t dsp_mu_rank_fail;

	/* The number of failed group installations */
	uint32_t dsp_mu_grp_inst_fail;

	/* Per-MU group DSP stats */
	/* The number of successful group installations */
	uint32_t dsp_mu_grp_inst_success[QTN_MU_QMAT_MAX_SLOTS];
	/* The number of successful QMat installations */
	uint32_t dsp_mu_grp_update_success[QTN_MU_QMAT_MAX_SLOTS];
	/* The number of failed QMat installations */
	uint32_t dsp_mu_grp_update_fail[QTN_MU_QMAT_MAX_SLOTS];
	/* Group's AID 0 */
	uint32_t dsp_mu_grp_aid0[QTN_MU_QMAT_MAX_SLOTS];
	/* Group's AID 1 */
	uint32_t dsp_mu_grp_aid1[QTN_MU_QMAT_MAX_SLOTS];
	/* Group's rank */
	int32_t dsp_mu_grp_rank[QTN_MU_QMAT_MAX_SLOTS];

	/*
	 * Distribution (histogram) of MU QMat copying time
	 0:  0- 3us
	 1:  4- 7us
	 ...............
	 3: 12+ us
	 */
#define DSP_MU_QMAT_COPY_TIME_HIST_WIDTH_US	4
	uint32_t dsp_mu_qmat_qmem_copy_time_hist[4];
	uint32_t dsp_mu_qmat_qmem_copy_time_max;

	/*
	 * Distribution (histogram) of MU QMat calculation and installation time
	 0:  0- 3ms
	 1:  4- 7ms
	 ...............
	 3: 12+ ms
	 */
#define DSP_MU_QMAT_INST_TIME_HIST_WIDTH_MS	6
	uint32_t dsp_mu_qmat_inst_time_hist[8];
	uint32_t dsp_mu_qmat_inst_time_max;

	uint32_t dsp_mu_grp_inv_act;
	uint32_t dsp_act_cache_expired[2];
	uint32_t dsp_mu_grp_upd_done;
	uint32_t dsp_mu_node_del;

	uint32_t dsp_mimo_ctrl_fail;
	uint32_t dsp_mu_fb_80mhz;
	uint32_t dsp_mu_fb_40mhz;
	uint32_t dsp_mu_fb_20mhz;
	uint32_t dsp_mu_drop_20mhz;
};
#endif

/* Structure be used for txbf message box */
struct qtn_txbf_mbox
{
	/* Write index in txbf_msg_bufs array. Updated only by a sender */
	volatile u_int32_t wr;

	#define MUC_TO_DSP_ACT_MBOX_SIZE	12
	volatile u_int32_t muc_to_dsp_action_frame_mbox[MUC_TO_DSP_ACT_MBOX_SIZE];
	volatile u_int32_t muc_to_dsp_ndp_mbox;
	volatile u_int32_t muc_to_dsp_del_grp_node_mbox;
	volatile u_int32_t muc_to_dsp_gr_upd_done_mbox;
	volatile u_int32_t muc_to_trig_mu_grp_sel_mbox;
	volatile u_int32_t dsp_to_host_mbox;

	volatile struct txbf_pkts txbf_msg_bufs[QTN_TXBF_MUC_DSP_MSG_RING_SIZE];

	volatile struct txbf_ctrl bfctrl_params;

	/* Debug verbosity level */
#define DEBUG_LVL_NO	0
#define DEBUG_LVL_ALL	1
	volatile uint32_t debug_level;

#define MU_QMAT_FREEZE				0x00000001
#define MU_MANUAL_RANK				0x00000002
#define MU_FREEZE_RANK				0x00000004
#define MU_QMAT_ZERO_STA0			0x00000010
#define MU_QMAT_ZERO_STA1			0x00000020
#define MU_QMAT_PRINT_CHMAT			0x00000100
#define MU_QMAT_PRINT_PRECMAT			0x00000200
#define MU_QMAT_PRINT_SNR			0x00000400
#define MU_QMAT_PRINT_RANK			0x00000800
#define MU_QMAT_PRINT_STUFFMEM			0x00001000
#define MU_QMAT_PRINT_ACTFRM			0x00002000
#define MU_MATLAB_PROCESS			0x00004000
#define MU_V_ANGLE				0x00008000
#define MU_PROJ_PREC_MUEQ_NEED_MASK		0x000F0000
#define MU_PROJ_PREC_MUEQ_NEED_NC0_MASK		0x00030000
#define MU_PROJ_PREC_MUEQ_NEED_NC0_SHIFT	16
#define MU_PROJ_PREC_MUEQ_NEED_NC1_MASK		0x000C0000
#define MU_PROJ_PREC_MUEQ_NEED_NC1_SHIFT	18
#define MU_PRINT_RANK_INFO			0x00100000
#define MU_LIMIT_GRP_ENTRY			0x00300000
	volatile uint32_t debug_flag;
	volatile struct qtn_sram_qmat mu_grp_qmat[QTN_MU_QMAT_MAX_SLOTS];
	/* Used for testing to set rank for STA pairs manually */
	volatile struct qtn_grp_rank mu_grp_man_rank[QTN_MU_QMAT_MAX_SLOTS];
#if DSP_ENABLE_STATS
	volatile struct qtn_dsp_stats dsp_stats;
#endif

#define MU_ALGORITHM_AUTO		0x00000000
#define MU_ALGORITHM_PROJECTION		0x00000001
#define MU_ALGORITHM_ITERATION		0x00000002
#define MU_PRECODING_ALGORITHM_DEFAULT	MU_ALGORITHM_PROJECTION
#define MU_RANKING_ALGORITHM_DEFAULT	MU_ALGORITHM_AUTO
/* in case of adding algorithms above please update below equation accordingly */
#define MU_ALLOWED_ALG(x) ((x)<=MU_ALGORITHM_ITERATION)
	volatile uint32_t ranking_algorithm_to_use;
	volatile uint32_t precoding_algorithm_to_use;
#define RANK_CRIT_ONE_AND_ONE	0x00000000
#define RANK_CRIT_TWO_AND_ONE	0x00000001
#define RANK_CRIT_THREE_AND_ONE	0x00000002
#define RANK_CRIT_ONE_AND_TWO	0x00000003
#define RANK_CRIT_ONE_AND_THREE	0x00000004
#define RANK_CRIT_TWO_AND_TWO	0x00000005
#define RANK_CRIT_MAX_MU_SUB_MAX_SU	0x00000006
#define RANK_CRIT_DEFAULT	RANK_CRIT_TWO_AND_TWO
#define RANK_CRIT_NO_USER_CONF	0x0000000f
	volatile uint32_t rank_criteria_to_use;

	volatile uint32_t mu_prec_cache_max_time;
	volatile int32_t mu_rank_tolerance;
};

struct qtn_muc_dsp_mbox
{
	volatile u_int32_t muc_to_dsp_mbox;
	volatile u_int32_t dsp_to_muc_mbox;
	volatile struct qtn_rate_train_info muc_dsp_msg_bufs[QTN_RATE_MUC_DSP_MSG_RING_SIZE]
				__attribute__ ((aligned (ARC_DCACHE_LINE_LENGTH) ));
};

#define QTN_TXBF_MBOX_PROCESSED 1
#define QTN_TXBF_MBOX_NOT_PROCESSED 0

#if !defined(MUC_BUILD) && !defined(DSP_BUILD) && !defined(AUC_BUILD)

#if CONFIG_USE_SPI1_FOR_IPC
	#define QTN_TXBF_D2L_IRQ	RUBY_IRQ_SPI
	#define QTN_TXBF_D2L_IRQ_NAME	"DSP(spi)"
#else
	#define QTN_TXBF_D2L_IRQ	RUBY_IRQ_DSP
	#define QTN_TXBF_D2L_IRQ_NAME	"DSP(d2l)"
#endif

RUBY_INLINE void
qtn_txbf_lhost_init(void)
{
#if CONFIG_USE_SPI1_FOR_IPC
	/* Initialize SPI controller, keep IRQ disabled */
	qtn_mproc_sync_mem_write(RUBY_SPI1_SPCR,
		RUBY_SPI1_SPCR_SPE | RUBY_SPI1_SPCR_MSTR |
		RUBY_SPI1_SPCR_SPR(0));
	qtn_mproc_sync_mem_write(RUBY_SPI1_SPER,
		RUBY_SPI1_SPER_ESPR(0));
#else
	/* Ack, and keep IRQ disabled */
	qtn_mproc_sync_mem_write(RUBY_SYS_CTL_D2L_INT,
		qtn_mproc_sync_mem_read(RUBY_SYS_CTL_D2L_INT));
	qtn_mproc_sync_mem_write(RUBY_SYS_CTL_D2L_INT_MASK,
		~(1 << QTN_TXBF_DSP_TO_HOST_MBOX_INT));
#endif
}

RUBY_INLINE u_int32_t
qtn_txbf_lhost_irq_ack(struct qdrv_mac *mac)
{
#if CONFIG_USE_SPI1_FOR_IPC
	/*
	 * Only single interrupt is supported now.
	 * If need to support more interrupts then something like
	 * 'status' in RAM, guarded by semaphores has to be implemented.
	 * This should be avoided, as it is performance penalty.
	 */
	qtn_mproc_sync_mem_write(RUBY_SPI1_SPSR,
		qtn_mproc_sync_mem_read(RUBY_SPI1_SPSR));
	return (1 << QTN_TXBF_DSP_TO_HOST_MBOX_INT);
#else
	return qtn_mproc_sync_irq_ack_all((u_int32_t)mac->mac_host_dsp_int_status);
#endif
}

RUBY_INLINE void
qtn_txbf_lhost_irq_enable(struct qdrv_mac *mac)
{
#if CONFIG_USE_SPI1_FOR_IPC
	set_bit(RUBY_SPI1_SPCR_SPIE_BIT, (void*)RUBY_SPI1_SPCR);
#else
	set_bit(QTN_TXBF_DSP_TO_HOST_MBOX_INT, (void*)mac->mac_host_dsp_int_mask);
#endif
}

RUBY_INLINE void
qtn_txbf_lhost_irq_disable(struct qdrv_mac *mac)
{
#if CONFIG_USE_SPI1_FOR_IPC
	clear_bit(RUBY_SPI1_SPCR_SPIE_BIT, (void*)RUBY_SPI1_SPCR);
#else
	clear_bit(QTN_TXBF_DSP_TO_HOST_MBOX_INT, (void*)mac->mac_host_dsp_int_mask);
#endif
}

#endif // #if !defined(MUC_BUILD) && !defined(DSP_BUILD) && !defined(AUC_BUILD)

RUBY_INLINE volatile struct txbf_pkts *
qtn_txbf_mbox_alloc_msg_buf(volatile struct qtn_txbf_mbox* mbox) {
	int i;

	for (i = 0; i < ARRAY_SIZE(mbox->txbf_msg_bufs); i++) {
		int j = (i + mbox->wr) % ARRAY_SIZE(mbox->txbf_msg_bufs);
		if (mbox->txbf_msg_bufs[j].state == TXBF_BUFF_FREE) {
			mbox->wr = j;
			mbox->txbf_msg_bufs[j].state = TXBF_BUFF_IN_USE;
			return &mbox->txbf_msg_bufs[j];
		}
	}

	return NULL;
}

RUBY_INLINE void
qtn_txbf_mbox_free_msg_buf(volatile struct txbf_pkts *msg) {
	msg->state = TXBF_BUFF_FREE;
}

RUBY_INLINE u_int32_t
qtn_txbf_mbox_get_index(volatile struct qtn_txbf_mbox* mbox) {
	return mbox->wr;
}

RUBY_INLINE volatile struct qtn_txbf_mbox*
qtn_txbf_mbox_get(void)
{
#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
	return qtn_mproc_sync_nocache
		(qtn_mproc_sync_shared_params_get()->txbf_mbox_bus);
#else
	/* Linux target */
	return qtn_mproc_sync_shared_params_get()->txbf_mbox_lhost;
#endif
}

RUBY_INLINE volatile struct qtn_muc_dsp_mbox*
qtn_muc_dsp_mbox_get(void)
{
#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
	return qtn_mproc_sync_nocache
		(qtn_mproc_sync_shared_params_get()->muc_dsp_mbox_bus);
#else
	/* Linux target */
	return qtn_mproc_sync_shared_params_get()->muc_dsp_mbox_lhost;
#endif
}

#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
RUBY_INLINE int
qtn_muc_dsp_mbox_send(u_int32_t mbox, u_int32_t idx)
{
	int ret = 0;

	if (qtn_mproc_sync_mem_read(mbox) == QTN_TXBF_MBOX_BAD_IDX) {
		qtn_mproc_sync_mem_write_wmb(mbox, idx);
#if defined(MUC_BUILD)
		qtn_mproc_sync_irq_trigger(RUBY_SYS_CTL_M2D_INT,
			QTN_TXBF_MUC_TO_DSP_MBOX_INT);
#else
		qtn_mproc_sync_irq_trigger(RUBY_SYS_CTL_D2M_INT,
			QTN_TXBF_DSP_TO_MUC_MBOX_INT);
#endif
		ret = 1;
	}

	return ret;
}
#endif

#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
RUBY_INLINE int
qtn_txbf_mbox_send(u_int32_t mbox, u_int32_t idx)
{
	int ret = 0;

	if (qtn_mproc_sync_mem_read(mbox) == QTN_TXBF_MBOX_BAD_IDX) {
		qtn_mproc_sync_mem_write_wmb(mbox, idx);
#if defined(MUC_BUILD)
		qtn_mproc_sync_irq_trigger(RUBY_SYS_CTL_M2D_INT,
			QTN_TXBF_MUC_TO_DSP_MBOX_INT);
#else
	#if CONFIG_USE_SPI1_FOR_IPC
		qtn_mproc_sync_mem_write(RUBY_SPI1_SPDR, 0x1/*value is not important*/);
	#else
		qtn_mproc_sync_irq_trigger(RUBY_SYS_CTL_D2L_INT,
			QTN_TXBF_DSP_TO_HOST_MBOX_INT);
	#endif
#endif
		ret = 1;
	}

	return ret;
}
#endif

RUBY_INLINE u_int32_t
qtn_txbf_mbox_recv(u_int32_t mbox)
{
	u_int32_t ret = QTN_TXBF_MBOX_BAD_IDX;

	ret = qtn_mproc_sync_mem_read(mbox);
	if (ret != QTN_TXBF_MBOX_BAD_IDX) {
		qtn_mproc_sync_mem_write_wmb(mbox, QTN_TXBF_MBOX_BAD_IDX);
	}

	return ret;
}

RUBY_INLINE void
qtn_txbf_fft_lock(void)
{
#ifndef QTN_TXBF_FFT_LOCK_MANUAL
	/* Locking is disabled */
#elif QTN_TXBF_FFT_LOCK_MANUAL
	/* Manual, sw-centric locking. */
	qtn_mproc_sync_mem_write_wmb(RUBY_QT3_BB_MIMO_BF_RX,
		qtn_mproc_sync_mem_read(RUBY_QT3_BB_MIMO_BF_RX) & ~RUBY_QT3_BB_MIMO_BF_RX_DUMP_ENABLE);
#else
	/* Automatic, hw-centric locking.
	 * Hw locks FFT memory automatically after the NDP packet is received.
	 * No need to explicitely lock FFT.
	 */
#endif
}

RUBY_INLINE void
qtn_txbf_fft_unlock(void)
{
#ifndef QTN_TXBF_FFT_LOCK_MANUAL
	/* Locking is disabled */
#elif QTN_TXBF_FFT_LOCK_MANUAL
	/* Manual, sw-centric locking. */
	qtn_mproc_sync_mem_write_wmb(RUBY_QT3_BB_MIMO_BF_RX,
		qtn_mproc_sync_mem_read(RUBY_QT3_BB_MIMO_BF_RX) | RUBY_QT3_BB_MIMO_BF_RX_DUMP_ENABLE);
#else
	/* Automatic, hw-centric locking. */
	qtn_mproc_sync_mem_write_wmb(RUBY_QT3_BB_GLBL_PREG_INTR_STATUS, RUBY_QT3_BB_FFT_INTR);
#endif
}

/*
* qtn_txbf_mbox can be used to set parameters for DSP core from other cores.
* Ideally this way should be reworked but until it happens lets use dedicated macros to access such parameters
* to distibuish this qtn_txbf_mbox usage purpose from others (IPC, BF feedbacks exchange)
*/
#define DSP_PARAM_GET(param) (qtn_txbf_mbox_get()->param)
#define DSP_PARAM_SET(param, value) qtn_txbf_mbox_get()->param = (value)

#endif // #ifndef __ASSEMBLY__

#endif // #ifndef __QTN_TXBF_MBOX_H



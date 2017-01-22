/*
 * Copyright (c) 2013 Quantenna Communications, Inc.
 */

#ifndef _AUC_DEBUG_STATS_H_
#define _AUC_DEBUG_STATS_H_

#include <qtn/auc_share_def.h>

/*
 * Firmware updates counters through such macros as AUC_DBG_INC(), AUC_DBG_INC_OFFSET(), AUC_DBG_INC_COND(), etc.
 * Other CPU (e.g. Lhost) can read structure and dump counters.
 * Feel free to add more counters here.
 * Good to have counters organized and grouped using name prefix.
 */
struct auc_dbg_counters
{
	/* pktlogger expects task_alive_counters to be the first member of this struct */
	uint32_t task_alive_counters[AUC_TID_NUM];
	uint32_t task_false_trigger[AUC_TID_NUM];
	uint32_t tqew_ac[4];
	uint32_t tqew_ac_avail[4];
	uint32_t tqew_air_humble;
	uint32_t tqew_air_suppress;
	uint32_t tqew_air_use_idletime;
	uint32_t tqew_air_dequeue_only;
	uint32_t tqew_pkt_pending_for_txdone;
	uint32_t tqew_descr_alloc_fail;
	uint32_t tqew_ring_alloc_fail;
	uint32_t tqew_pop_alloc_fail;
	uint32_t tqew_pop_sw_limit;
	uint32_t tqew_pop_empty;
	uint32_t tqew_available_set;
	uint32_t tqew_available_reset;
	uint32_t tqew_rx;
	uint32_t tqew_drop;
	uint32_t tqew_free;
	uint32_t tqew_buf_invalid;
	uint32_t wmac_tx_done[4];
	uint32_t agg_aggregate_flag;
	uint32_t agg_aggressive_agg;
	uint32_t hdrs_available_recent_min;
	uint32_t agg_states[QTN_AUC_TID_TX_STATE_MAX + 1];
	uint32_t ethq_push;
	uint32_t ethq_pop;
	uint32_t agg_aggregate_mpdu;
	uint32_t agg_aggregate_msdu;
	uint32_t agg_singleton_mpdu;
	uint32_t agg_singleton_mgmt;
	uint32_t agg_singleton_ctl;
	uint32_t agg_singleton_probe;
	uint32_t agg_4K_amsdu;
	uint32_t agg_8K_amsdu;
	uint32_t agg_11K_amsdu;
	uint32_t tx_feedback_success;
	uint32_t tx_feedback_fail;
	uint32_t tx_done_status_success;
	uint32_t tx_done_status_timeout;
	uint32_t tx_done_status_xretry;
	uint32_t tx_done_status_timeout_xretry;
	uint32_t tx_done_pkt_chain_reset;
	uint32_t tx_done_pkt_chain_success;
	uint32_t tx_done_pkt_chain_drop_tid_down;
	uint32_t tx_done_pkt_chain_drop_xattempts;
	uint32_t tx_done_singleton_finish;
	uint32_t tx_done_singleton_swretry;
	uint32_t tx_done_aggregate_finish;
	uint32_t tx_done_aggregate_hwretry;
	uint32_t tx_done_aggregate_swretry;
	uint32_t tx_done_mpdu_swretry;
	uint32_t tx_sample;
	uint32_t tx_bw_sample;
	uint32_t tx_swretry_lower_bw;
	uint32_t tx_swretry_agg_exceed;
	uint32_t tx_scale_base_20m;
	uint32_t tx_scale_base_40m;
	uint32_t tx_scale_base_80m;
	uint32_t tx_scale_max;
	uint32_t tx_scale_overstep;
	uint32_t alloc_tqew_fast;
	uint32_t free_tqew_fast;
	uint32_t alloc_tqew_slow;
	uint32_t free_tqew_slow;
	uint32_t alloc_tqew_local;
	uint32_t free_tqew_local;
	uint32_t alloc_hdr_fast;
	uint32_t free_hdr_fast;
	uint32_t alloc_hdr_slow;
	uint32_t free_hdr_slow;
	uint32_t alloc_msdu_hdr_failed;
	uint32_t alloc_mpdu_hdr_failed;
	uint32_t alloc_tid_superfast;
	uint32_t free_tid_superfast;
	uint32_t alloc_tid_fast;
	uint32_t free_tid_fast;
	uint32_t alloc_tid_slow;
	uint32_t free_tid_slow;
	uint32_t alloc_node_rate_fast;
	uint32_t free_node_rate_fast;
	uint32_t alloc_node_rate_slow;
	uint32_t free_node_rate_slow;
	uint32_t alloc_node_superfast;
	uint32_t free_node_superfast;
	uint32_t alloc_node_fast;
	uint32_t free_node_fast;
	uint32_t alloc_fcs;
	uint32_t free_fcs;
	uint32_t alloc_mac_descr;
	uint32_t free_mac_descr;
	uint32_t tx_mac_push;
	uint32_t tx_mac_idle;
	uint32_t tx_mac_rts;
	uint32_t tx_mac_cts2self;
	uint32_t tx_vlan_drop;
	uint32_t tx_acm_drop;
	uint32_t tx_ps_drop;
	uint32_t ocs_tx_suspend;
	uint32_t ocs_tx_resume;
	uint32_t ocs_singleton_suspend;
	uint32_t ocs_ampdu_suspend;
	uint32_t ocs_frame_created;
	uint32_t pwr_mgmt_awake;
	uint32_t pwr_mgmt_sleep;
	uint32_t pwr_mgmt_tx;
	uint32_t pspoll_rx;
	uint32_t dtim_q_push;
	uint32_t dtim_q_pop;
	uint32_t dtim_trigger;
	uint32_t dtim_q_overflow;
	uint32_t tx_restrict_dropped;
	uint32_t tx_throt_dropped;
	uint32_t tx_block_singleton;
	uint32_t tx_force_unblock_tid;
	uint32_t tx_ctl_pkt_hbm_alloc_fails;
	uint32_t tx_ctl_pkt_alloc_descr_fails;
	uint32_t tx_bar_alloc_ctl_pkt_fails;
	uint32_t tx_valid_bit_not_set;

	uint32_t wmm_ps_tx;
	uint32_t wmm_ps_tx_null_frames;
	uint32_t wmm_ps_tx_more_data_frames;
	uint32_t wmm_ps_tx_eosp_frames;

	/*
	 * Mu Tx & Done & Retry
	 */
	uint32_t mu_tx_su_count;	/* Can't find buddy, and this AMPDU be sent as SU */

	uint32_t mu_tx_send_mu_fail;	/* Can't be sent as MU, send them as SU */

	uint32_t mu_tx_push_count;
	uint32_t mu_tx_done_count;

	uint32_t mu_tx_done_succ;	/* The succ/fail counter of AMPDU which be sent via WMAC1 */
	uint32_t mu_tx_done_fail;
	uint32_t mu_tx_sample;            /* mu sampling phy rate count */
	uint32_t mu_bar_bitmap_non_zero;
	uint32_t mu_bar_bitmap_zero;
	uint32_t mu_mac_wmac1_ipc_push;
	uint32_t mu_mac_wmac1_auc_push;
	uint32_t mu_wmac1_resets;

	uint32_t mu_tx_swretry_agg_exceed;

	uint32_t mu_tx_buddy_try;
	uint32_t mu_tx_buddy_fail_wmac;
	uint32_t mu_tx_buddy_fail_ptid;
	uint32_t mu_tx_buddy_fail_rate;
	uint32_t mu_tx_buddy_fail_create_agg;

	uint32_t mu_tx_buddy_mu_only_timeout;

	uint32_t mu_tx_another_q_push_succ;
	uint32_t mu_tx_another_q_push_fail;	/* If current cont_q is not ready, try another cont_q */
	uint32_t mu_tx_buddy_multi_tid;

	/* For debug, remove it before submitting */
	uint32_t mu_tx_wmac_0_done_count;
	uint32_t mu_tx_wmac_0_bitmap_non_zero;
	uint32_t mu_tx_wmac_0_bitmap_zero;
	uint32_t mu_tx_wmac_0_done_timeout;
	uint32_t mu_tx_wmac_0_done_succ;
	uint32_t mu_tx_wmac_0_done_fail;

	uint32_t mu_tx_wmac_1_done_succ;
	uint32_t mu_tx_wmac_1_done_fail;

	uint32_t mu_tx_wmac_0_mpdu_total;
	uint32_t mu_tx_wmac_0_mpdu_succ;

	uint32_t mu_tx_wmac_1_mpdu_total;
	uint32_t mu_tx_wmac_1_mpdu_succ;

	uint32_t mu_tx_qnum[AUC_FW_WMAC_TX_QNUM];
	uint32_t tqe_sema_fails;
};
#endif // #ifndef _AUC_DEBUG_STATS_H_


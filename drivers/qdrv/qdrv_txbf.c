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
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <asm/hardware.h>

#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_comm.h"
#include "qdrv_wlan.h"
#include "qdrv_vap.h"
#include "qdrv_txbf.h"
#include "qdrv_hal.h"
#include "qdrv_control.h"
#include "qdrv_soc.h"
#include <qtn/qtn_global.h>
#include <qtn/txbf_mbox.h>
#include <qtn/topaz_hbm.h>

/* send the MU qmat install/delete event to Muc */
static void qdrv_txbf_mu_grp_qmat_update(struct qtn_mu_group_update_args *args, int slot,
					struct ieee80211_node *ni, uint8_t grp_id,
					int delete, int feedback)
{
	struct qdrv_vap *qv = container_of(ni->ni_vap, struct qdrv_vap, iv);

	args->groups[slot].grp_id = grp_id;
	args->groups[slot].ap_devid = qv->devid;
	memcpy(&args->groups[slot].ap_macaddr[0], &ni->ni_macaddr[0], sizeof(uint8_t)*IEEE80211_ADDR_LEN);
}

static int qdrv_txbf_node_mu_grp_update(struct qtn_mu_group_update_args *args,
					int grp_i, int node_i,
					struct ieee80211_node *ni,
					uint8_t grp, uint8_t pos, uint8_t delete)
{
	KASSERT(grp_i >= 0 && grp_i < QTN_MU_QMAT_MAX_SLOTS,
		(DBGEFMT "group index exceeds the limits: %d\n", DBGARG, grp_i));
	KASSERT(node_i >= 0 && node_i < QTN_MU_QMAT_MAX_SLOTS,
		(DBGEFMT "group index exceeds the limits: %d\n", DBGARG, grp_i));

	if (!IEEE80211_MU_GRP_VALID(grp)) {
		IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_VHT,
				  "%s: MU grp id %u invalid\n",
				  __func__, ether_sprintf(ni->ni_macaddr), grp);
		return 0;
	}
	if (!IEEE80211_MU_POS_VALID(pos)) {
		IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_VHT,
				  "%s: MU pos %u invalid\n",
				  __func__, ether_sprintf(ni->ni_macaddr), pos);
		return 0;
	}

	if (delete) {
		IEEE80211_NODE_MU_DEL_GRP(ni, grp);
	} else {
		IEEE80211_NODE_MU_ADD_GRP(ni, grp, pos);
	}

	args->groups[grp_i].nodes[node_i].as_sta = ni->ni_vap->iv_opmode == IEEE80211_M_STA;
	memcpy(&args->groups[grp_i].nodes[node_i].macaddr[0],&ni->ni_macaddr[0], sizeof(uint8_t)*IEEE80211_ADDR_LEN);
	memcpy(&args->groups[grp_i].nodes[node_i].grp,       &ni->ni_mu_grp,     sizeof(struct ieee80211_vht_mu_grp));
	return 1;
}

static struct ieee80211_node * qdrv_txbf_find_txnode(struct qdrv_mac *mac,
		volatile struct txbf_ndp_info *ndp_info)
{
	struct net_device *vdev;
	struct qdrv_vap *qv;

	/* Transmit frame out the primary VAP */
	vdev = mac->vnet[0];
	if (unlikely(vdev == NULL)) {
		return NULL;
	}

	qv = netdev_priv(vdev);

	return ieee80211_find_txnode(&qv->iv, (uint8_t*)ndp_info->macaddr);
}

static size_t qdrv_txbf_act_frm_allheaders_len(void)
{
	return sizeof(struct ieee80211_frame)
		 + sizeof(struct ieee80211_action)
		 + sizeof(struct ht_mimo_ctrl);
}

static struct sk_buff *qdrv_txbf_get_skb(volatile struct txbf_pkts *pkt_info)
{
#if TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT
	const int8_t pool = TOPAZ_HBM_BUF_WMAC_RX_POOL;
	void *buf_bus = (void *) pkt_info->buffer_start;
	struct sk_buff *skb = NULL;

	if (likely(buf_bus)) {
		skb = topaz_hbm_attach_skb_bus(buf_bus, pool);
		if (skb == NULL) {
			topaz_hbm_put_payload_aligned_bus(buf_bus, pool);
		}
	}
	return skb;
#else
	return (struct sk_buff *) pkt_info->skb;
#endif
}

static inline void qdrv_txbf_free_nodes(struct ieee80211_node **node1,
						struct ieee80211_node **node2)
{
	if (*node1) {
		ieee80211_free_node(*node1);
		*node1 = NULL;
	}

	if (*node2) {
		ieee80211_free_node(*node2);
		*node2 = NULL;
	}
}

static inline void qdrv_txbf_dbg_printout(struct ieee80211_node *ni)
{
	if (ni == NULL) {
		return;
	}

	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_VHT,
			"%s: %s MU grp: "
			"%02x%02x%02x%02x%02x%02x%02x%02x\n"
			"MU pos: %02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x\n",
			__func__, ether_sprintf(ni->ni_macaddr),
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
}

static void qdrv_txbf_send_vht_grp_id_act_frm(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = NULL;
	struct ieee80211vap *vap = NULL;
	if (ni == NULL) {
		return;
	}
	ic = ni->ni_ic;
	vap = ni->ni_vap;
	if (vap->iv_opmode != IEEE80211_M_STA) {
		ic->ic_send_vht_grp_id_act(ni->ni_vap, ni);
	}
}

static int qdrv_txbf_process_mu_grp_mbox(struct qdrv_wlan *qw, volatile struct qtn_txbf_mbox *txbf_mbox, uint32_t opcode)
{
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211_node *u0 = NULL;
	struct ieee80211_node *u1 = NULL;
	struct ieee80211_node *ap = NULL;
	volatile struct qtn_sram_qmat *mu_qmat = &txbf_mbox->mu_grp_qmat[0];
	struct qtn_mu_group_update_args grp_update_args = {0};
	int grp_i, node_i;

	opcode -= QTN_TXBF_MUC_DSP_MSG_RING_SIZE;

	switch (opcode) {
	case QTN_TXBF_DSP_TO_HOST_INST_MU_GRP:
		grp_update_args.op = MU_GRP_INST;
		break;
	case QTN_TXBF_DSP_TO_HOST_DELE_MU_GRP:
		grp_update_args.op = MU_GRP_DELE;
		break;
	case QTN_TXBF_MBOX_BAD_IDX:
		return QTN_TXBF_MBOX_NOT_PROCESSED;
	default:
		return QTN_TXBF_MBOX_NOT_PROCESSED;
	}

	if (!ieee80211_swfeat_is_supported(SWFEAT_ID_MU_MIMO, 0)) {
		return QTN_TXBF_MBOX_PROCESSED;
	}

	/* Collect all the necessary data needed to update groups and qmats at MuC */
	for (grp_i = 0; grp_i < QTN_MU_QMAT_MAX_SLOTS; qdrv_txbf_free_nodes(&u0, &u1), grp_i++) {
		node_i = 0;
		if (!mu_qmat[grp_i].valid) {
			continue;
		}

		if (qw->ic.ic_mu_debug_level) {
			printk("dsp to lhost(%u): %s mu grp %d node0 %d node1 %d rank %d tk: 0x%02x\n",
				grp_i, (opcode == QTN_TXBF_DSP_TO_HOST_INST_MU_GRP) ? "install" : "delete",
				mu_qmat[grp_i].grp_id, mu_qmat[grp_i].u0_aid, mu_qmat[grp_i].u1_aid,
				mu_qmat[grp_i].rank, mu_qmat[grp_i].tk);
		}

		u0 = ieee80211_find_node_by_aid(ic, mu_qmat[grp_i].u0_aid);
		u1 = ieee80211_find_node_by_aid(ic, mu_qmat[grp_i].u1_aid);

		if (opcode == QTN_TXBF_DSP_TO_HOST_INST_MU_GRP) {
			if (u0 == NULL || u1 == NULL) {
				continue;
			}
			ap = u0->ni_vap->iv_bss;
			if (qdrv_txbf_node_mu_grp_update(&grp_update_args, grp_i, node_i,
							u0, mu_qmat[grp_i].grp_id, 0, 0)) {
				node_i++;
			}
			if (qdrv_txbf_node_mu_grp_update(&grp_update_args, grp_i, node_i,
							u1, mu_qmat[grp_i].grp_id, 1, 0)) {
				node_i++;
			}
		} else { /* (opcode == QTN_TXBF_DSP_TO_HOST_DELE_MU_GRP) */
			if (u0 == NULL && u1 == NULL) {
				continue;
			}

			if (u0 != NULL) {
				ap = u0->ni_vap->iv_bss;
				if (qdrv_txbf_node_mu_grp_update(&grp_update_args, grp_i,
						node_i, u0, mu_qmat[grp_i].grp_id, 0, 1)) {
					node_i++;
				}
			}

			if (u1 != NULL) {
				ap = u1->ni_vap->iv_bss;
				if (qdrv_txbf_node_mu_grp_update(&grp_update_args, grp_i,
						node_i, u1, mu_qmat[grp_i].grp_id, 1, 1)) {
					node_i++;
				}
			}

		}

		qdrv_txbf_mu_grp_qmat_update(&grp_update_args, grp_i, ap, mu_qmat[grp_i].grp_id, 1, 0);

		qdrv_txbf_send_vht_grp_id_act_frm(u0);
		qdrv_txbf_send_vht_grp_id_act_frm(u1);
		qdrv_txbf_dbg_printout(u0);
		qdrv_txbf_dbg_printout(u1);
	}

	ic->ic_mu_group_update(ic, &grp_update_args);

	return QTN_TXBF_MBOX_PROCESSED;
}

static void qdrv_txbf_pkt_info_clear(volatile struct txbf_pkts *pkt_info, struct sk_buff *skb)
{
	pkt_info->skb = 0;
	pkt_info->act_frame_phys = 0;
	pkt_info->buffer_start = 0;
	qtn_txbf_mbox_free_msg_buf(pkt_info);
	dev_kfree_skb_irq(skb);
}

static int qdrv_txbf_process_txbf_mbox(struct qdrv_wlan *qw, volatile struct qtn_txbf_mbox *txbf_mbox, uint32_t pkt_offset)
{
	volatile struct txbf_state *txbf_state = qw->txbf_state;
	volatile struct txbf_pkts *pkt_info;
	struct ieee80211_node *ni;
	struct sk_buff *skb;

	if (QTN_TXBF_MBOX_BAD_IDX == pkt_offset || pkt_offset >= QTN_TXBF_MUC_DSP_MSG_RING_SIZE) {
		DBGPRINTF_E("%s: bad txbf mbox pkt_offset value %d\n", __func__, pkt_offset);
		return QTN_TXBF_MBOX_NOT_PROCESSED;
	}

	pkt_info = txbf_mbox->txbf_msg_bufs + pkt_offset;

	/* Attach SKB so we can free up the buffer properly. */
	skb = qdrv_txbf_get_skb(pkt_info);
	if (skb == NULL) {
		qtn_txbf_mbox_free_msg_buf(pkt_info);
		return QTN_TXBF_MBOX_NOT_PROCESSED;
	}

	/* DSP is done with a received action frame */
	if (pkt_info->msg_type == QTN_TXBF_ACT_FRM_FREE_MSG) {
		uint8_t slot = pkt_info->slot;

		if (pkt_info->success) {
			txbf_state->stvec_install_success++;
			if (slot < QTN_STATS_NUM_BF_SLOTS) {
				RXSTAT(qw, rx_bf_success[slot]);
			}
			TXSTAT_SET(qw, txbf_qmat_wait, pkt_info->txbf_qmat_install_wait);
		} else {
			txbf_state->stvec_install_fail++;
			if (slot < QTN_STATS_NUM_BF_SLOTS) {
				RXSTAT(qw, rx_bf_rejected[slot]);
			}
		}
		if (pkt_info->bf_compressed) {
			txbf_state->cmp_act_frms_rxd++;
		} else {
			txbf_state->uncmp_act_frms_rxd++;
		}
		txbf_state->qmat_offset = pkt_info->qmat_offset;
		txbf_state->bf_ver = pkt_info->bf_ver;

		if (pkt_info->ndp_info.bw_mode == QTN_BW_80M) {
			txbf_state->qmat_bandwidth = BW_HT80;
		} else if (pkt_info->ndp_info.bw_mode == QTN_BW_40M) {
			txbf_state->qmat_bandwidth = BW_HT40;
		} else {
			txbf_state->qmat_bandwidth = BW_HT20;
		}
		txbf_state->bf_tone_grp = pkt_info->bf_tone_grp;

		qdrv_txbf_pkt_info_clear(pkt_info, skb);

		return QTN_TXBF_MBOX_PROCESSED;
	} else if (pkt_info->msg_type != QTN_TXBF_ACT_FRM_TX_MSG) {
		/* Print an error message for unexpected messages */
		if (pkt_info->msg_type != QTN_TXBF_NDP_DISCARD_MSG) {
			DBGPRINTF_E("Received message not for me: %x\n", pkt_info->msg_type);
		}
		qdrv_txbf_pkt_info_clear(pkt_info, skb);

		return QTN_TXBF_MBOX_PROCESSED;
	}

	/* Process a transmit action frame from the DSP */
	skb_put(skb, pkt_info->act_frame_len);

	if ( txbf_state->send_txbf_netdebug ) {
		txbf_state->send_txbf_netdebug = 0;
		qdrv_control_txbf_pkt_send(qw,
				skb->data +
				qdrv_txbf_act_frm_allheaders_len() + 2,
				pkt_info->ndp_info.bw_mode);
	}

	ni = qdrv_txbf_find_txnode(qw->mac, &pkt_info->ndp_info);

	/* Clear the packet info ready for the next NDP */
	pkt_info->act_frame_phys = 0;
	pkt_info->buffer_start = 0;
	pkt_info->skb = 0;
	qtn_txbf_mbox_free_msg_buf(pkt_info);

	if (ni == NULL) {
		dev_kfree_skb_irq(skb);
	} else {
		ni->ni_ic->ic_send_80211(ni->ni_ic, ni, skb, WME_AC_VO, 0);
		if (pkt_info->bf_compressed) {
			txbf_state->cmp_act_frms_sent++;
		} else {
			txbf_state->uncmp_act_frms_sent++;
		}
		if (pkt_info->ndp_info.bw_mode == QTN_BW_80M) {
			txbf_state->qmat_bandwidth = BW_HT80;
		} else if (pkt_info->ndp_info.bw_mode == QTN_BW_40M) {
			txbf_state->qmat_bandwidth = BW_HT40;
		} else {
			txbf_state->qmat_bandwidth = BW_HT20;
		}
		txbf_state->bf_tone_grp = pkt_info->bf_tone_grp;
	}

	return QTN_TXBF_MBOX_PROCESSED;
}

/*
 * Mailbox tasklet
 * The node structure must be locked before scheduling this process.
 */
static void qdrv_txbf_mbox_tasklet(unsigned long data)
{
	volatile struct qtn_txbf_mbox *txbf_mbox = qtn_txbf_mbox_get();
	struct qdrv_wlan *qw = (struct qdrv_wlan*)data;
	uint32_t pkt_offset;
	uint32_t res;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	pkt_offset = qtn_txbf_mbox_recv(qtn_mproc_sync_addr(&(txbf_mbox->dsp_to_host_mbox)));
	if (QTN_TXBF_MBOX_BAD_IDX == pkt_offset) {
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
		return;
	}

	res = qdrv_txbf_process_mu_grp_mbox(qw, txbf_mbox, pkt_offset);
	if (res == QTN_TXBF_MBOX_NOT_PROCESSED) {
		qdrv_txbf_process_txbf_mbox(qw, txbf_mbox, pkt_offset);
	}

	/* Enable the mbx interrupts */
	qtn_txbf_lhost_irq_enable(qw->mac);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

/* Handler for the DSP interrupt for action frame */
static void qdrv_txbf_mbox_interrupt(void *arg1, void *dev_id)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan*)dev_id;
	struct txbf_state *txbf_state = qw->txbf_state;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Disable mbx interrupts */
	qtn_txbf_lhost_irq_disable(qw->mac);

	if (txbf_state != NULL) {
		tasklet_schedule(&txbf_state->txbf_dsp_mbox_task);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

int qdrv_txbf_config_get(struct qdrv_wlan *qw, u32 *value)
{
	struct txbf_state *txbf_state = (struct txbf_state *) qw->txbf_state;
	volatile struct qtn_txbf_mbox *txbf_mbox = qtn_txbf_mbox_get();
	volatile struct txbf_ctrl *bf_ctrl = NULL;

	printk("Current TXBF Config values are:\n");
	if (txbf_mbox != NULL) {
		bf_ctrl = &txbf_mbox->bfctrl_params;

		printk("    CalcChanInv               = %d\n",
			!!(bf_ctrl->svd_mode & BIT(SVD_MODE_CHANNEL_INV)));
		printk("    CalcTwoStreams            = %d\n",
			!!(bf_ctrl->svd_mode & BIT(SVD_MODE_TWO_STREAM)));
		printk("    ApplyPerAntScaling        = %d\n",
			!!(bf_ctrl->svd_mode & BIT(SVD_MODE_PER_ANT_SCALE)));
		printk("    ApplyStreamMixing         = %d\n",
			!!(bf_ctrl->svd_mode & BIT(SVD_MODE_STREAM_MIXING)));
		printk("    SVD Bypass                = %d\n",
			!!(bf_ctrl->svd_mode & BIT(SVD_MODE_BYPASS)));
	} else {
		printk("    SVD settings not available\n");
	}
	printk("    Stvec install bypass      = %d\n", txbf_state->stmat_install_bypass);
	printk("    Reg Scale fac             = %d\n", txbf_state->st_mat_reg_scale_fac);
	printk("    Stvec install success     = %d\n", txbf_state->stvec_install_success);
	printk("    Stvec install failed      = %d\n", txbf_state->stvec_install_fail);
	printk("    Stvec overwrite           = %d\n", txbf_state->stvec_overwrite);
	printk("    Comp Action Frames Sent   = %d\n", txbf_state->cmp_act_frms_sent);
	printk("    Uncomp Action Frames Sent = %d\n", txbf_state->uncmp_act_frms_sent);
	printk("    Comp Action Frames Recv   = %d\n", txbf_state->cmp_act_frms_rxd);
	printk("    Uncomp Action Frames Recv = %d\n", txbf_state->uncmp_act_frms_rxd);
	printk("    Bandwidth                 = %d\n", txbf_state->qmat_bandwidth);
	if (((txbf_state->qmat_bandwidth == 0) || (txbf_state->qmat_bandwidth == BW_HT80)) &&
			(txbf_state->bf_tone_grp == QTN_TXBF_DEFAULT_QMAT_NG)) {
		/* Assume 80 MHz 11ac node if bw is 0, as hw is providing feedback */
		printk("    1 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_1STRM_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_2STRM_OFFSET(txbf_state->qmat_offset));
		printk("    3 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_3STRM_OFFSET(txbf_state->qmat_offset));
		printk("    4 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_4STRM_OFFSET(txbf_state->qmat_offset));
		printk("    1 Stream 40M Stvec offset = %u\n", QTN_TXBF_QMAT80_1STRM_40M_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream 40M Stvec offset = %u\n", QTN_TXBF_QMAT80_2STRM_40M_OFFSET(txbf_state->qmat_offset));
		printk("    1 Stream 20M Stvec offset = %u\n", QTN_TXBF_QMAT80_1STRM_20M_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream 20M Stvec offset = %u\n", QTN_TXBF_QMAT80_2STRM_20M_OFFSET(txbf_state->qmat_offset));
	} else if ((txbf_state->qmat_bandwidth == 0) || (txbf_state->qmat_bandwidth == BW_HT80)) {
		/* Assume 80 MHz 11ac node if bw is 0, as hw is providing feedback */
		printk("    1 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_NG2_1STRM_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_NG2_2STRM_OFFSET(txbf_state->qmat_offset));
		printk("    3 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_NG2_3STRM_OFFSET(txbf_state->qmat_offset));
		printk("    4 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT80_NG2_4STRM_OFFSET(txbf_state->qmat_offset));
		printk("    1 Stream 40M Stvec offset = %u\n", QTN_TXBF_QMAT80_NG2_1STRM_40M_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream 40M Stvec offset = %u\n", QTN_TXBF_QMAT80_NG2_2STRM_40M_OFFSET(txbf_state->qmat_offset));
		printk("    1 Stream 20M Stvec offset = %u\n", QTN_TXBF_QMAT80_NG2_1STRM_20M_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream 20M Stvec offset = %u\n", QTN_TXBF_QMAT80_NG2_2STRM_20M_OFFSET(txbf_state->qmat_offset));
	} else
	{
		printk("    1 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT40_1STRM_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT40_2STRM_OFFSET(txbf_state->qmat_offset));
		printk("    3 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT40_3STRM_OFFSET(txbf_state->qmat_offset));
		printk("    4 Stream Stvec offset     = %u\n", QTN_TXBF_QMAT40_4STRM_OFFSET(txbf_state->qmat_offset));
		printk("    1 Stream 40M Stvec offset = %u\n", QTN_TXBF_QMAT40_1STRM_40M_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream 40M Stvec offset = %u\n", QTN_TXBF_QMAT40_2STRM_40M_OFFSET(txbf_state->qmat_offset));
		printk("    1 Stream 20M Stvec offset = %u\n", QTN_TXBF_QMAT40_1STRM_20M_OFFSET(txbf_state->qmat_offset));
		printk("    2 Stream 20M Stvec offset = %u\n", QTN_TXBF_QMAT40_2STRM_20M_OFFSET(txbf_state->qmat_offset));
	}
	printk("    BF version                = %u\n", txbf_state->bf_ver);

	*value = 0;
	if (bf_ctrl == NULL) {
		return(0);
	}
	*value |= !!(bf_ctrl->svd_mode & BIT(SVD_MODE_CHANNEL_INV)) << 16;
	*value |= !!(bf_ctrl->svd_mode & BIT(SVD_MODE_TWO_STREAM)) << 12;
	*value |= !!(bf_ctrl->svd_mode & BIT(SVD_MODE_PER_ANT_SCALE)) << 8;
	*value |= !!(bf_ctrl->svd_mode & BIT(SVD_MODE_STREAM_MIXING)) << 4;
	*value |= !!(bf_ctrl->svd_mode & BIT(SVD_MODE_BYPASS));

	return(0);
}

int qdrv_txbf_config_set(struct qdrv_wlan *qw, u32 value)
{
	struct txbf_state *txbf_state = (struct txbf_state *) qw->txbf_state;
	int par0,par1, par2, par3, par4, par5;
        volatile struct qtn_txbf_mbox *txbf_mbox = qtn_txbf_mbox_get();

    DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	txbf_state->send_txbf_netdebug = 1;

	if(value & (0xFF << 24)){
		txbf_state->st_mat_reg_scale_fac = (signed char)((int)value >>24);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(0);
	}

	par5 = (value >> 0) & 0xf;
	par4 = (value >> 4) & 0xf;
	par3 = (value >> 8) & 0xf;
	par2 = (value >> 12) & 0xf;
	par1 = (value >> 16) & 0xf;
	par0 = (value >> 20) & 0xf;

	if (txbf_mbox != NULL) {
		volatile struct txbf_ctrl *bf_ctrl = &txbf_mbox->bfctrl_params;

		bf_ctrl->svd_mode = par0 ? BIT(SVD_MODE_CHANNEL_INV) : 0;
		bf_ctrl->svd_mode |= par1 ? BIT(SVD_MODE_TWO_STREAM) : 0;
		bf_ctrl->svd_mode |= par2 ? BIT(SVD_MODE_PER_ANT_SCALE) : 0;
		bf_ctrl->svd_mode |= par3 ? BIT(SVD_MODE_STREAM_MIXING) : 0;
		bf_ctrl->svd_mode |= par4 ? BIT(SVD_MODE_BYPASS) : 0;
		printk("Beamforming svd mode set to 0x%x\n", bf_ctrl->svd_mode);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(0);
	}
	printk("Beamforming svd mode not set\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return (-1);
}

int qdrv_txbf_init(struct qdrv_wlan *qw)
{
	struct txbf_state *txbf_state;
	struct int_handler dsp_intr_handler;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if((txbf_state = (struct txbf_state *)
		kmalloc(sizeof(struct txbf_state), GFP_KERNEL)) == NULL)
	{
		DBGPRINTF_E("Unable to allocate memory for TXBF state\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-ENOMEM);
	}

	/* Clean the state */
	memset(txbf_state, 0, sizeof(struct txbf_state));

	txbf_state->st_mat_calc_chan_inv = 1;
	txbf_state->st_mat_calc_two_streams = 1;
	txbf_state->st_mat_apply_per_ant_scaling = 1;
	txbf_state->st_mat_apply_stream_mixing = 1;
	txbf_state->st_mat_reg_scale_fac = 10;

	tasklet_init(&txbf_state->txbf_dsp_mbox_task, qdrv_txbf_mbox_tasklet, (unsigned long)qw);

	/*
	 * Register the interrupt handler to be called when DSP pushes
	 * completion message into DSP-to-LHOST mbox
	 */
	dsp_intr_handler.handler = qdrv_txbf_mbox_interrupt;
	dsp_intr_handler.arg1 = NULL;
	dsp_intr_handler.arg2 = qw;
	if (qdrv_mac_set_host_dsp_handler(qw->mac, QTN_TXBF_DSP_TO_HOST_MBOX_INT,
			&dsp_intr_handler) != 0) {
		/* Handle error case */
		DBGPRINTF_E("Set handler error\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

		kfree(txbf_state);

		return(-ENODEV);
	}

	/* Enable the mbx interrupts */
	qtn_txbf_lhost_irq_enable(qw->mac);

	/* We need a back pointer */
	txbf_state->owner = (void *)qw;

	/* Attach the state to the wlan once we are done with everything */
	qw->txbf_state = (void *)txbf_state;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_txbf_exit(struct qdrv_wlan *qw)
{
	struct txbf_state *txbf_state = (struct txbf_state *) qw->txbf_state;

	/* Disable the mbox interrupts */
	qtn_txbf_lhost_irq_disable(qw->mac);

	if (txbf_state)
		tasklet_kill(&txbf_state->txbf_dsp_mbox_task);

	/* Free the memory for maintaining state */
	kfree(txbf_state);

	qw->txbf_state = NULL;

	return(0);
}


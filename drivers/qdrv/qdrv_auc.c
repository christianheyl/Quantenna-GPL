/**
 * Copyright (c) 2008 - 2013 Quantenna Communications Inc
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/version.h>

#include <asm/io.h>
#include <asm/board/soc.h>

#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_debug.h"
#include "qdrv_auc.h"
#include "qdrv_hal.h"
#include "qdrv_wlan.h"
#include "qdrv_vap.h"
#include "qdrv_fw.h"
#include <qtn/topaz_tqe.h>


static qtn_shared_node_stats_t *s_per_node_stats_ptr = NULL;
static qtn_shared_vap_stats_t *s_per_vap_stats_ptr = NULL;

static void auc_clear_addr_range(unsigned long physaddr, unsigned long size)
{
	void *vaddr = ioremap_nocache(physaddr, size);

	if (!vaddr) {
		DBGPRINTF_E("0x%lx, 0x%lx cannot be mapped\n", physaddr, size);
	} else {
		qdrv_fw_auc_memzero(vaddr, size, physaddr);
		iounmap(vaddr);
	}
}

static void auc_clear_mem(void)
{
	auc_clear_addr_range(TOPAZ_AUC_IMEM_ADDR, TOPAZ_AUC_IMEM_SIZE);
	auc_clear_addr_range(TOPAZ_AUC_DMEM_ADDR, TOPAZ_AUC_DMEM_SIZE);
	auc_clear_addr_range(RUBY_DRAM_BEGIN + CONFIG_ARC_AUC_BASE, CONFIG_ARC_AUC_SIZE);
	auc_clear_addr_range(RUBY_SRAM_BEGIN + CONFIG_ARC_AUC_SRAM_BASE, CONFIG_ARC_AUC_SRAM_SIZE);
}

void qdrv_auc_stats_setup(void)
{
	unsigned long phyaddr;
	struct shared_params *sp = qtn_mproc_sync_shared_params_get();

	if (unlikely(!sp || !sp->auc.node_stats || !sp->auc.vap_stats)) {
		DBGPRINTF(DBG_LL_ERR, QDRV_LF_TRACE, "Stats setup: failed\n");
		return;
	}

	if (!s_per_node_stats_ptr) {
		phyaddr = qdrv_fw_auc_to_host_addr((unsigned long)sp->auc.node_stats);
		s_per_node_stats_ptr = ioremap_nocache(phyaddr, QTN_NCIDX_MAX * sizeof(qtn_shared_node_stats_t));
	}

	if (!s_per_vap_stats_ptr) {
		phyaddr = qdrv_fw_auc_to_host_addr((unsigned long)sp->auc.vap_stats);
		s_per_vap_stats_ptr = ioremap_nocache(phyaddr, QTN_MAX_VAPS * sizeof(qtn_shared_vap_stats_t));
	}

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE, "Stats setup: Node : %p - %p\n"
			"             Vap  : %p - %p\n",
			sp->auc.node_stats,
			s_per_node_stats_ptr,
			sp->auc.vap_stats,
			s_per_vap_stats_ptr);
}

void qdrv_auc_stats_unmap(void)
{
	if (s_per_node_stats_ptr)
		iounmap(s_per_node_stats_ptr);
	if (s_per_vap_stats_ptr)
		iounmap(s_per_vap_stats_ptr);
}

qtn_shared_node_stats_t* qdrv_auc_get_node_stats(uint8_t node)
{
	return (s_per_node_stats_ptr) ? (s_per_node_stats_ptr + node) : NULL;
}

qtn_shared_vap_stats_t* qdrv_auc_get_vap_stats(uint8_t vapid)
{
	return (s_per_vap_stats_ptr) ? (s_per_vap_stats_ptr + vapid) : NULL;
}

void qdrv_auc_update_multicast_stats(void *ctx, uint8_t nid)
{
	uint8_t vapid;
	struct ieee80211com *ic = (struct ieee80211com *)ctx;
	struct ieee80211_node *node;
	struct ieee80211vap *vap;
	struct qdrv_vap * qv;
	qtn_shared_node_stats_t *nstats;
	qtn_shared_vap_stats_t *vstats;

	if (!ctx)
		return;

	node = ic->ic_node_idx_ni[nid];
	if (unlikely(!node))
		return;

	vap = node->ni_vap;
	qv = container_of(vap, struct qdrv_vap, iv);
	vapid = QDRV_WLANID_FROM_DEVID(qv->devid);
	nstats = qdrv_auc_get_node_stats(nid);
	vstats = qdrv_auc_get_vap_stats(vapid);

	if (unlikely(!nstats || !vstats))
		return;

	nstats->qtn_tx_mcast++;
	vstats->qtn_tx_mcast++;
}

int qdrv_auc_init(struct qdrv_cb *qcb)
{
	u32 auc_start_addr = 0;
	struct qdrv_wlan *qw = qcb->macs[0].data;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter");

	qtn_mproc_sync_shared_params_get()->auc.auc_config = global_auc_config;

	auc_clear_mem();

	if (qdrv_fw_load_auc(qcb->dev, qcb->auc_firmware, &auc_start_addr) < 0) {
		DBGPRINTF_E("AuC load firmware failed\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_DSP, "Firmware start address is %x\n", auc_start_addr);

	hal_enable_auc();

	tqe_reg_multicast_tx_stats(qdrv_auc_update_multicast_stats, &qw->ic);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

int qdrv_auc_exit(struct qdrv_cb *qcb)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter");

	qdrv_auc_stats_unmap();
	hal_disable_auc();

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}


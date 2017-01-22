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

#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/rtnetlink.h>
#include <asm/hardware.h>
#include <asm/board/board_config.h>

#include <qtn/qdrv_sch.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_fwt_db.h>

#ifdef CONFIG_QVSP
#include "qtn/qvsp.h"
#endif

#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_muc.h"
#include "qdrv_hal.h"
#include "qdrv_comm.h"
#include "qdrv_vap.h"
#include "qdrv_wlan.h"

extern void indicate_association(void);
extern void indicate_disassociation(void);
extern unsigned int g_led_assoc_indicate;

static bool wps_button_not_initd = 1;
static bool igmp_query_timer_not_initd = 1;
// TBD - Need to move this to a more suitable place
static int vnet_init(struct net_device *dev)
{
	struct qdrv_vap *qv = netdev_priv(dev);
	struct qdrv_wlan *qw = qv->parent;
	struct host_ioctl *ioctl;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	ioctl = vnet_alloc_ioctl(qv);
	if (!ioctl) {
		DBGPRINTF_E("Failed to allocate message\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	ioctl->ioctl_command = IOCTL_DEV_DEVOPEN;
	ioctl->ioctl_arg1 = qv->devid;
	ioctl->ioctl_arg2 = 0;
	ioctl->ioctl_argp = (u32) NULL;
	ioctl->ioctl_next = 0;
	ioctl->ioctl_status = 0;

	vnet_send_ioctl(qv, ioctl);

	napi_enable(&qv->napi);
	napi_schedule(&qv->napi);

	netif_start_queue(dev);

	/* Open the device for the 802.11 layer */
	ieee80211_open(dev);

	/* Set up the WPS button IRQ handler */
	// TBD - Need to move this to a more suitable place
	if (wps_button_not_initd) {
		wps_button_not_initd = 0;
		qdrv_wps_button_init(dev);
	}

	if (igmp_query_timer_not_initd &&
			qv->iv.iv_opmode == IEEE80211_M_HOSTAP) {
		qdrv_wlan_igmp_query_timer_start(qw);
		igmp_query_timer_not_initd = 0;
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

static int vnet_stop(struct net_device *dev)
{
	struct qdrv_vap *qv = netdev_priv(dev);
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* close the device for the 802.11 layer */
	ieee80211_stop(dev);

	napi_disable(&qv->napi);
	netif_stop_queue(dev);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

static __sram_data struct net_device_ops vnet_device_ops;

static void vnet_start(struct net_device * dev)
{
	ether_setup(dev);

	dev->netdev_ops = &vnet_device_ops;
	dev->tx_queue_len = 500;
}

static int qdrv_vap_80211_newstate_callback(struct ieee80211vap *vap,
	enum ieee80211_state nstate, int arg)
{
	struct qdrv_vap *qv = container_of(vap, struct qdrv_vap, iv);
	struct qdrv_wlan *qw = (struct qdrv_wlan *)qv->parent;
	int error;
	int stamode;
	struct ieee80211com *ic = vap->iv_ic;
	enum ieee80211_state ostate;

	ostate = vap->iv_state;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter, caller %p\n", __builtin_return_address(0));

	if(vap->iv_state != nstate)
	{
		DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_VAP,
				"New state for \"%s\" %s -> %s\n",
				vap->iv_dev->name, ieee80211_state_name[vap->iv_state],
				ieee80211_state_name[nstate]);
	}

	stamode = (vap->iv_opmode == IEEE80211_M_STA ||
				vap->iv_opmode == IEEE80211_M_IBSS ||
				vap->iv_opmode == IEEE80211_M_AHDEMO);

	switch (nstate) {
	case IEEE80211_S_RUN:
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
				"IEEE80211_S_RUN\n");
		switch (vap->iv_opmode) {

		case IEEE80211_M_HOSTAP:
			DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
					"IEEE80211_M_HOSTAP - Send Beacon\n");
			ic->ic_beacon_update(vap);

			break;
		case IEEE80211_M_STA:
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
				"IEEE80211_M_STA\n");
			indicate_association();
			ic->ic_join_bss(vap);
			SMSTAT(qw, sm_sta_associated);
			break;
		default:
			break;
		}
		break;
	case IEEE80211_S_INIT://    = 0,    /* default state */
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"IEEE80211_S_INIT\n");
		if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
			ic->ic_beacon_stop(vap);
		} else if (vap->iv_opmode == IEEE80211_M_STA) {
			/* Pend disassociation with AP before TDLS link return to base channel */
			if (!ieee80211_tdls_return_to_base_channel(vap, 1))
				return 0;
		}
		indicate_disassociation();
		break;
	case IEEE80211_S_SCAN://    = 1,    /* scanning */
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
				"IEEE80211_S_SCAN\n");
		indicate_disassociation();
		if (arg == IEEE80211_SCAN_FAIL_TIMEOUT) {
			if (ostate == IEEE80211_S_AUTH) {
				SMSTAT(qw, sm_scan_auth_fail_scan_pend);
			} else if (ostate == IEEE80211_S_ASSOC) {
				SMSTAT(qw, sm_scan_assoc_fail_scan_pend);
			}
		}
		else if (arg == 0) {
			SMSTAT(qw, sm_scan_pend);
		}
		break;

	case IEEE80211_S_AUTH://    = 2,    /* try to authenticate */
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
				"IEEE80211_S_AUTH\n");
		indicate_disassociation();
		if (stamode) {
			if (ostate == IEEE80211_S_SCAN) {
				SMSTAT(qw, sm_auth_pend);
			} else {
				SMSTAT(qw, sm_run_deauth_auth_pend);
			}
		}
		break;
	case IEEE80211_S_ASSOC://   = 3,    /* try to assoc */
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
				"IEEE80211_S_ASSOC\n");
		indicate_disassociation();
		if (stamode) {
			if (ostate == IEEE80211_S_AUTH) {
				SMSTAT(qw, sm_assoc_pend);
			} else {
				SMSTAT(qw, sm_run_disassoc_assoc_pend);
			}
		}
		break;
	default:
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
				"<unknown state %d>\n", nstate);
		indicate_disassociation();
		break;
	}

	/* Pend disassociation before tearing down all of TDLS links */
	if (ieee80211_tdls_pend_disassociation(vap, nstate, arg))
		return 0;

	/* Invoke the parent method to complete the work.*/
	error = (*qv->qv_newstate)(vap, nstate, arg);

	return error;
}

int qdrv_vap_wds_mode(struct qdrv_vap *qv)
{
	return(qv->iv.iv_flags_ext & IEEE80211_FEXT_WDS ? 1 : 0);
}

#ifdef CONFIG_QVSP
static int qdrv_qvsp_ioctl(void *qw_, uint32_t param, uint32_t value)
{
#if TOPAZ_QTM
	struct qdrv_wlan *qw = qw_;

	switch (param) {
	case QVSP_CFG_FAT_MIN_CHECK_INTV:
		qw->vsp_check_intvl = value / MSEC_PER_SEC;
		break;
	case QVSP_CFG_ENABLED:
		if (value) {
			/* Give stats part 2 check interval to warm up */
			qw->vsp_enabling = 2;
		}
		break;
	default:
		break;
	}
#endif

	return qdrv_hostlink_qvsp(qw_, param, value);
}
#endif

static void qdrv_vap_set_last(struct qdrv_mac *mac)
{
	int i = QDRV_MAX_VAPS;

	while (i--) {
		if (mac->vnet[i]) {
			break;
		}
	}

	mac->vnet_last = i;
}

int qdrv_vap_vlan2index_sync(struct qdrv_vap *qv, uint16_t mode, uint16_t vid)
{
	uint8_t vap_id = qv->qv_vap_idx;
	uint8_t last = 0;

	if (mode == QVLAN_MODE_DYNAMIC)
		return 0;

	if (mode != QVLAN_MODE_ACCESS)
		vid = VLANID_INDEX_INITVAL;

	qdrv_sch_vlan2index[vap_id] = vid;

	/* find the last vap bound to a vlan */
	for (vap_id = 0; vap_id < ARRAY_SIZE(qdrv_sch_vlan2index); vap_id++) {
		if (qdrv_sch_vlan2index[vap_id] != VLANID_INDEX_INITVAL) {
			last = vap_id + 1;
		}
	}
	qdrv_vap_vlan_max = last;

#if !defined(CONFIG_TOPAZ_PCIE_HOST) && !defined(CONFIG_TOPAZ_PCIE_TARGET)
	qdrv_sch_set_vlanpath();
#endif
	return 0;
}

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
int qdrv_get_active_sub_port(const struct net_bridge_port *p,
		uint32_t *sub_port_bitmap, int size)
{
	struct topaz_fwt_sw_mcast_entry *mcast_entry;
	int i;

	if (size != sizeof(mcast_entry->node_bitmap)) {
		DBGPRINTF_LIMIT_E("bitmap length is invalid - %d/%lu\n",
			size, sizeof(mcast_entry->node_bitmap));
		return 0;
	}

	/* Use the ipff entry, which has the bit set for each active node */
	mcast_entry = fwt_db_get_sw_mcast_ff();
	memcpy(sub_port_bitmap, mcast_entry->node_bitmap, size);
	for (i = 0; i < ARRAY_SIZE(mcast_entry->node_bitmap); i++) {
		if (sub_port_bitmap[i])
			return 1;
	}

	return 0;
}

int qdrv_check_active_sub_port(const struct net_bridge_port *p,
		const uint32_t sub_port)
{
	struct net_device *dev;
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	dev = p->dev;
	vap = netdev_priv(dev);

	if (vap->iv_dev != dev)
		return -1;

	BUG_ON(QTN_NCIDX_MAX < IEEE80211_NODE_IDX_UNMAP(sub_port));

	ic = vap->iv_ic;
	nt = &ic->ic_sta;

	IEEE80211_NODE_LOCK_IRQ(nt);
	ni = ic->ic_node_idx_ni[IEEE80211_NODE_IDX_UNMAP(sub_port)];
	IEEE80211_NODE_UNLOCK_IRQ(nt);

	return (ni && ieee80211_node_is_running(ni));
}
#endif

int qdrv_vap_init(struct qdrv_mac *mac, struct host_ioctl_hifinfo *hifinfo,
	u32 arg1, u32 arg2)
{
	struct qdrv_vap *qv;
	struct net_device *vdev;
	int opmode = 0;
	int vap_idx = 0;
	int i;
	struct ieee80211com *ic = NULL;
	struct ieee80211vap *vap = NULL;
	struct qdrv_wlan *qw;
	int stamode;
	int repeater_mode;
	unsigned int qv_devid = arg1 & IOCTL_DEVATTACH_DEVID_MASK;
	unsigned int dev_devid = QDRV_WLANID_FROM_DEVID(qv_devid);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"name         : %s\n", hifinfo->hi_name);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"semmap       : %d\n", hifinfo->hi_semmap[0]);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"mbox         : 0x%08x\n", hifinfo->hi_mboxstart);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"rxfifo       : 0x%08x\n", hifinfo->hi_rxfifo);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"scanirq      : 0x%08x\n", hifinfo->hi_scanirq);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"scanfifo     : 0x%08x\n", hifinfo->hi_scanfifo);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"qv_devid     : 0x%08x\n", qv_devid);
	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"dev_devid    : 0x%08x\n", dev_devid);

	/* Search for an empty VAP slot */
	for (i = 0; i < QDRV_MAX_VAPS; i++) {
		if (mac->vnet[i] == NULL) {
			/* Found one */
			vap_idx = i;
			break;
		}
	}

	/* Check if we found one */
	if (i == QDRV_MAX_VAPS) {
		DBGPRINTF_E("No empty VAP slot available for \"%s\"\n",
			hifinfo->hi_name);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	vdev = dev_get_by_name(&init_net, hifinfo->hi_name);
	if (vdev != NULL) {
		DBGPRINTF_E("The device name \"%s\" already exists\n",
			hifinfo->hi_name);
		dev_put(vdev);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	/* Allocate our device */
	vdev = alloc_netdev(sizeof(struct qdrv_vap), hifinfo->hi_name, vnet_start);
	if (vdev == NULL) {
		DBGPRINTF_E("Unable to allocate device \"%s\"\n",
			hifinfo->hi_name);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	opmode = (arg1  >> 24) & 0xF;

	qv = netdev_priv(vdev);
	memset(qv, 0, sizeof(struct qdrv_vap));

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"qv 0x%08x iv 0x%08x dev 0x%08x\n",
			(unsigned int) qv, (unsigned int) &qv->iv, (unsigned int) vdev);

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"vap_idx %d opmode %d\n", vap_idx, opmode);

	qv->ndev = vdev;
	qv->parent = mac->data;
	qw = (struct qdrv_wlan *)qv->parent;
	qv->devid = qv_devid;
	qv->qv_vap_idx = hifinfo->hi_vapid;
	if (qv->qv_vap_idx >= QTN_MAX_BSS_VAPS)
		panic("vapid: %u is out of range!\n", qv->qv_vap_idx);
	vdev->dev_id = dev_devid;

	vdev->qtn_flags |= QTN_FLAG_WIFI_DEVICE;
	TAILQ_INIT(&qv->allnodes);

	vdev->if_port = TOPAZ_TQE_WMAC_PORT;

	netif_napi_add(vdev, &qv->napi, qdrv_rx_poll, board_napi_budget());

	memcpy(vdev->dev_addr, hifinfo->hi_macaddr, IEEE80211_ADDR_LEN);

	spin_lock_init(&qv->lock);
	spin_lock_init(&qv->bc_lock);
	spin_lock_init(&qv->ni_lst_lock);

	/* Initiate a VAP setup */
	if (ieee80211_vap_setup(&((struct qdrv_wlan *) mac->data)->ic, vdev,
		hifinfo->hi_name, qv->devid, opmode, IEEE80211_NO_STABEACONS) < 0) {
		DBGPRINTF_E("The 802.11 layer failed to setup the VAP\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	/* Replace newstate function with our own */
	qv->qv_newstate = qv->iv.iv_newstate;
	qv->iv.iv_newstate = qdrv_vap_80211_newstate_callback;

	/* Take the RTNL lock since register_netdevice() is used instead of */
	/* register_netdev() in ieee80211_vap_attach()                      */
	rtnl_lock();

	/* Complete the VAP setup */
	if (ieee80211_vap_attach(&qv->iv,
		ieee80211_media_change, ieee80211_media_status) < 0) {
		DBGPRINTF_E("The 802.11 layer failed to attach the VAP\n");
		rtnl_unlock();
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	ic = &((struct qdrv_wlan *) mac->data)->ic;
	vap = &qv->iv;

	vap->iv_sta_assoc_limit = QTN_ASSOC_LIMIT;
	vap->iv_ssid_group = 0;

	stamode = (vap->iv_opmode == IEEE80211_M_STA ||
				vap->iv_opmode == IEEE80211_M_IBSS ||
				vap->iv_opmode == IEEE80211_M_AHDEMO);

	repeater_mode = (ic->ic_flags_ext & IEEE80211_FEXT_REPEATER);

	if (stamode || repeater_mode) {
		ic->ic_mindwell_active = QDRV_WLAN_STA_MIN_DWELLTIME_ACTIVE;
		ic->ic_mindwell_passive = QDRV_WLAN_STA_MIN_DWELLTIME_PASSIVE;
		ic->ic_maxdwell_active = QDRV_WLAN_STA_MAX_DWELLTIME_ACTIVE;
		ic->ic_maxdwell_passive = QDRV_WLAN_STA_MAX_DWELLTIME_PASSIVE;
		ic->ic_opmode = IEEE80211_M_STA;
		QDRV_SET_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_STA);

		if (repeater_mode)
			ic->ic_roaming = IEEE80211_ROAMING_AUTO;
	} else {
		ic->ic_mindwell_active = QDRV_WLAN_AP_MIN_DWELLTIME_ACTIVE;
		ic->ic_mindwell_passive = QDRV_WLAN_AP_MIN_DWELLTIME_PASSIVE;
		ic->ic_maxdwell_active = QDRV_WLAN_AP_MAX_DWELLTIME_ACTIVE;
		ic->ic_maxdwell_passive = QDRV_WLAN_AP_MAX_DWELLTIME_PASSIVE;
		ic->ic_opmode = IEEE80211_M_HOSTAP;
		QDRV_SET_SM_FLAG(qw->sm_stats, QDRV_WLAN_SM_STATE_AP);

		/* Force auto roaming for AP in case it was set to manual when in STA mode */
		ic->ic_roaming = IEEE80211_ROAMING_AUTO;
	}

#ifdef QTN_BG_SCAN
	ic->ic_qtn_bgscan.dwell_msecs_active = QDRV_WLAN_QTN_BGSCAN_DWELLTIME_ACTIVE;
	ic->ic_qtn_bgscan.dwell_msecs_passive = QDRV_WLAN_QTN_BGSCAN_DWELLTIME_PASSIVE;
	ic->ic_qtn_bgscan.duration_msecs_active = QDRV_WLAN_QTN_BGSCAN_DURATION_ACTIVE;
	ic->ic_qtn_bgscan.duration_msecs_passive_fast = QDRV_WLAN_QTN_BGSCAN_DURATION_PASSIVE_FAST;
	ic->ic_qtn_bgscan.duration_msecs_passive_normal = QDRV_WLAN_QTN_BGSCAN_DURATION_PASSIVE_NORMAL;
	ic->ic_qtn_bgscan.duration_msecs_passive_slow = QDRV_WLAN_QTN_BGSCAN_DURATION_PASSIVE_SLOW;
	ic->ic_qtn_bgscan.thrshld_fat_passive_fast = QDRV_WLAN_QTN_BGSCAN_THRESHLD_PASSIVE_FAST;
	ic->ic_qtn_bgscan.thrshld_fat_passive_normal = QDRV_WLAN_QTN_BGSCAN_THRESHLD_PASSIVE_NORMAL;
	ic->ic_qtn_bgscan.debug_flags = 0;
#endif /* QTN_BG_SCAN */

	rtnl_unlock();

	/* vlan configuration structure associated with the device */
	if (switch_alloc_vlan_dev(TOPAZ_TQE_WMAC_PORT, dev_devid, vdev->ifindex) == NULL) {
		DBGPRINTF_E("failed to bind vlan dev to VAP\n");
		ieee80211_vap_detach(&qv->iv);
		return -1;
	}

	/* Set some debug stuff */
	qv->iv.iv_debug |= IEEE80211_MSG_DEBUG |
				IEEE80211_MSG_INPUT |
				IEEE80211_MSG_ASSOC |
				IEEE80211_MSG_AUTH |
				IEEE80211_MSG_OUTPUT;

	/* Disable some ... */
	qv->iv.iv_debug &= ~IEEE80211_MSG_DEBUG;
	qv->iv.iv_debug  = 0;
	((struct net_device_ops *)(vdev->netdev_ops))->ndo_start_xmit = qdrv_tx_hardstart;
	((struct net_device_ops *)(vdev->netdev_ops))->ndo_open = vnet_init;
	((struct net_device_ops *)(vdev->netdev_ops))->ndo_stop = vnet_stop;

	TAILQ_INIT(&qv->ni_lncb_lst);

#ifdef CONFIG_QVSP
	if (qw->qvsp == NULL) {
		qw->qvsp = qvsp_init(&qdrv_qvsp_ioctl, qw, vdev, stamode,
			ic->ic_vsp_cb_cfg, ic->ic_vsp_cb_strm_ctrl, ic->ic_vsp_cb_strm_ext_throttler,
			sizeof(struct ieee80211_node), sizeof(struct ieee80211vap));
		if (qw->qvsp && qdrv_wlan_vsp_3rdpt_init(qw)) {
			printk("Could not initialize VSP 3rd party client control\n");
		}
	} else if (vap->iv_opmode == IEEE80211_M_WDS) {
		qvsp_inactive_flag_set(qw->qvsp, QVSP_INACTIVE_WDS);
	}
	if (repeater_mode && vap->iv_opmode == IEEE80211_M_HOSTAP)
		ic->ic_vsp_change_stamode(ic, 0);
#endif

	qv->iv.iv_vapnode_idx = IEEE80211_NODE_IDX_MAP(hifinfo->hi_vapnode_idx);
	qdrv_tx_sch_attach(qv);
	if (ic->ic_rf_chipid == CHIPID_DUAL && vap->iv_opmode != IEEE80211_M_WDS) {
		/* Disable one-bit dynamic auto-correlation on RFIC5 */
		ic->ic_setparam(vap->iv_bss, IEEE80211_PARAM_DYNAMIC_AC, 0, NULL, 0);
	}
	/* initial bss node will be created before the qdisc; reinitialize */
	if (vap->iv_bss) {
		qdrv_tx_sch_node_data_init(qdrv_tx_sch_vap_get_qdisc(vdev),
				qw->tx_sch_shared_data, &vap->iv_bss->ni_tx_sch, 1);
	}

	/*
	 * Finally, set vnet pointer. Needs to be done after all init is
	 * complete, or there will be synchronization problem with
	 * qdrv_tx_wake_queue or others.
	 */
	mac->vnet[vap_idx] = vdev;
	qdrv_vap_set_last(mac);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

/* Any resource allocated to VAP cleanup here */
void qdrv_vap_resource_cleanup(struct qdrv_vap *qv)
{
	if (qv->bc_skb != NULL) {
		dev_kfree_skb_any(qv->bc_skb);
		qv->bc_skb = NULL;
		}
}

static int qdrv_get_hostap_count(struct ieee80211com *ic, struct ieee80211vap *vap)
{
	int count = 0;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode == IEEE80211_M_HOSTAP)
			count++;
	}

	return count;
}

int qdrv_vap_exit(struct qdrv_mac *mac, struct net_device *vdev)
{
	struct qdrv_vap *qv = netdev_priv(vdev);
#ifdef CONFIG_QVSP
	struct qdrv_wlan *qw = (struct qdrv_wlan *)qv->parent;
	struct ieee80211com *ic = qv->iv.iv_ic;
	struct ieee80211vap *vap = &qv->iv;
	int repeater_mode = ic->ic_flags_ext & IEEE80211_FEXT_REPEATER;

	if (repeater_mode &&
			vap->iv_opmode == IEEE80211_M_HOSTAP &&
			qdrv_get_hostap_count(ic, vap) == 1)
		ic->ic_vsp_change_stamode(ic, 1);

	qvsp_exit(&qw->qvsp, vdev);

	if (qw->qvsp == NULL) {
		qdrv_wlan_vsp_3rdpt_exit(qw);
	}
#endif

	rtnl_lock();
	ieee80211_vap_detach(&qv->iv);
	rtnl_unlock();

#ifdef CONFIG_QVSP
	if (qw->qvsp && (qdrv_wlan_query_wds(ic) == 0) ) {
		qvsp_inactive_flag_clear(qw->qvsp, QVSP_INACTIVE_WDS);
	}
#endif

	return 0;
}

int qdrv_vap_exit_muc_done(struct qdrv_mac *mac, struct net_device *vdev)
{
	struct qdrv_vap *qv = netdev_priv(vdev);
	int vnet_found = 0;
	int i;

	for (i = 0; i < QDRV_MAX_VAPS; i++) {
		if (mac->vnet[i] == vdev) {
			mac->vnet[i] = NULL;
			qdrv_vap_set_last(mac);
			vnet_found = 1;
		}
	}

	if (!vnet_found) {
		DBGPRINTF_E("vap %s not found in mac\n", vdev->name);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -ENODEV;
	}

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_VAP,
			"Delete VAP qv 0x%p \"%s\" (%d)\n", qv, vdev->name, i);

	if (mac->vnet[0] == NULL && igmp_query_timer_not_initd == 0) {
		struct qdrv_wlan *qw = (struct qdrv_wlan *)qv->parent;
		qdrv_wlan_igmp_timer_stop(qw);
		igmp_query_timer_not_initd = 1;
	}

	if (mac->vnet[0] == NULL && wps_button_not_initd == 0) {
		qdrv_wps_button_exit();
		wps_button_not_initd = 1;
	}

	qdrv_vap_resource_cleanup(qv);

	qdrv_tx_done_flush_vap(qv);

	/* release switch vlan */
	switch_free_vlan_dev_by_idx(vdev->dev_id);

	/* Destroy it ... */
	rtnl_lock();
	ieee80211_vap_detach_late(&qv->iv);
	rtnl_unlock();

	return 0;
}

int qdrv_exit_all_vaps(struct qdrv_mac *mac)
{
	int i;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	for (i = 0; i < QDRV_MAX_VAPS; i++) {
		if (mac->vnet[i]) {
			qdrv_vap_exit(mac, mac->vnet[i]);
		}
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}


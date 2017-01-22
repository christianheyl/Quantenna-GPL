/**
  Copyright (c) 2008 - 2014 Quantenna Communications Inc
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

#include <linux/kernel.h>
#include <linux/if_ether.h>

#include <net80211/if_llc.h>
#include <net80211/if_ethersubr.h>

#include <qtn/qtn_uc_comm.h>
#include <qtn/qtn_vlan.h>

#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_wlan.h"
#include "qdrv_vap.h"
#include "qdrv_vlan.h"

struct qdrv_node *qdrv_vlan_alloc_group(struct qdrv_vap *qv, uint16_t vid)
{
	struct ieee80211com *ic = qv->iv.iv_ic;
	struct ieee80211_node *ni, *bss_ni;
	struct qtn_vlan_dev *vdev = vdev_tbl_lhost[QDRV_WLANID_FROM_DEVID(qv->devid)];
	uint8_t mac[ETH_ALEN];

	qtn_vlan_gen_group_addr(mac, vid, QDRV_WLANID_FROM_DEVID(qv->devid));

	ni = ieee80211_alloc_node(&ic->ic_sta, &qv->iv, mac, "VLAN group");
	if (!ni)
		return NULL;

	bss_ni = qv->iv.iv_bss;
	ni->ni_capinfo = bss_ni->ni_capinfo;
	ni->ni_txpower = bss_ni->ni_txpower;
	ni->ni_ath_flags = qv->iv.iv_ath_cap;
	ni->ni_flags = bss_ni->ni_flags;
	ni->ni_start_time_assoc = get_jiffies_64();
	ni->ni_flags |= (IEEE80211_NODE_AUTH | IEEE80211_NODE_HT);

	if (ic->ic_newassoc)
		ic->ic_newassoc(ni, 1);

	switch_vlan_set_node(vdev, IEEE80211_NODE_IDX_UNMAP(ni->ni_node_idx), vid);

	return container_of(ni, struct qdrv_node, qn_node);
}

void qdrv_vlan_free_group(struct qdrv_node *qn)
{
	struct qdrv_vap *qv = container_of(qn->qn_node.ni_vap, struct qdrv_vap, iv);
	struct qtn_vlan_dev *vdev = vdev_tbl_lhost[QDRV_WLANID_FROM_DEVID(qv->devid)];

	switch_vlan_clr_node(vdev, qn->qn_node_idx);
	ieee80211_free_node(&qn->qn_node);
}

struct qdrv_node *qdrv_vlan_find_group_noref(struct qdrv_vap *qv, uint16_t vid)
{
	struct qdrv_node *ret = NULL, *qn;
	uint8_t	mac[ETH_ALEN];
	unsigned long flags;

	qtn_vlan_gen_group_addr(mac, vid, QDRV_WLANID_FROM_DEVID(qv->devid));

	local_irq_save(flags);

	TAILQ_FOREACH(qn, &qv->allnodes, qn_next) {
		if (memcmp(qn->qn_node.ni_macaddr, mac, ETH_ALEN) == 0) {
			ret = qn;
			break;
		}
	}

	local_irq_restore(flags);

	return ret;
}

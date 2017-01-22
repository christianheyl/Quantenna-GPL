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
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/device.h>
#include <asm/hardware.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_comm.h"
#include "qdrv_hal.h"
#include "qdrv_wlan.h"
#include "qdrv_vap.h"
#include "qdrv_txbf.h"
#include "qdrv_auc.h"
#include <qtn/registers.h>
#include <qtn/shared_params.h>
#include <qtn/muc_phy_stats.h>
#include <qtn/bootcfg.h>
#include <qtn/semaphores.h>
#include <qtn/lhost_muc_comm.h>

static int msg_attach_handler(struct qdrv_cb *qcb, void *msg);
static int msg_detach_handler(struct qdrv_cb *qcb, void *msg);
static int msg_logattach_handler(struct qdrv_cb *qcb, void *msg);
static int msg_temp_attach_handler(struct qdrv_cb *qcb, void *msg);
static int msg_devchange_handler(struct qdrv_cb *qcb, void *msg);
static int msg_null_handler(struct qdrv_cb *qcb, void *msg);
static int msg_fops_handler(struct qdrv_cb *qcb, void *msg);
static int msg_tkip_mic_error_handler(struct qdrv_cb *qcb, void *msg);
static int msg_muc_booted_handler(struct qdrv_cb *qcb, void *msg);
static int msg_drop_ba_handler(struct qdrv_cb *qcb, void *msg);
static int msg_disassoc_sta_handler(struct qdrv_cb *qcb, void *msg);
static int msg_rfic_caused_reboot_handler(struct qdrv_cb *qcb, void *msg);
static int msg_tdls_events(struct qdrv_cb *qcb, void *msg);
static int msg_ba_add_start_handler(struct qdrv_cb *qcb, void *msg);
static int msg_peer_rts_handler(struct qdrv_cb *qcb, void *msg);
static int msg_dyn_wmm_handler(struct qdrv_cb *qcb, void *msg);
static int msg_rate_train(struct qdrv_cb *qcb, void *msg);

static const char * const cal_filenames[] = LHOST_CAL_FILES;

static int (*s_msg_handler_table[])(struct qdrv_cb *qcb, void *msg) =
{
	msg_null_handler,		/*                          */
	msg_attach_handler,		/* IOCTL_HLINK_DEVATTACH    */
	msg_detach_handler,		/* IOCTL_HLINK_DEVDETACH    */
	msg_devchange_handler,		/* IOCTL_HLINK_DEVCHANGE    */
	msg_logattach_handler,		/* IOCTL_HLINK_LOGATTACH    */
	msg_temp_attach_handler,	/* IOCTL_HLINK_TEMP_ATTACH   */
	msg_null_handler,		/* IOCTL_HLINK_SVCERRATTACH */
	msg_null_handler,		/* IOCTL_HLINK_RTNLEVENT    */
	msg_null_handler,		/* IOCTL_HLINK_NDP_FRAME    */
	msg_fops_handler,		/* IOCTL_HLINK_FOPS_REQ  */
	msg_tkip_mic_error_handler,	/* IOCTL_HLINK_MIC_ERR */
	msg_muc_booted_handler,		/* IOCTL_HLINK_BOOTED */
	msg_drop_ba_handler,		/* IOCTL_HLINK_DROP_BA */
	msg_disassoc_sta_handler,	/* IOCTL_HLINK_DISASSOC_STA */
	msg_rfic_caused_reboot_handler, /* IOCTL_HLINK_RFIC_CAUSED_REBOOT */
	msg_ba_add_start_handler,	/* IOCTL_HLINK_BA_ADD_START */
	msg_peer_rts_handler,		/* IOCTL_HLINK_PEER_RTS */
	msg_dyn_wmm_handler,		/* IOCTL_HLINK_DYN_WMM */
	msg_tdls_events,		/* IOCTL_HLINK_TDLS_EVENTS	*/
	msg_rate_train,			/* IOCTL_HLINK_RATE_TRAIN	*/
};

#define MSG_HANDLER_TABLE_SIZE	(ARRAY_SIZE(s_msg_handler_table))

static void comm_irq_handler(void *arg1, void *arg2)
{
	struct qdrv_cb *qcb = (struct qdrv_cb *) arg1;

	schedule_work(&qcb->comm_wq);
}

static int msg_null_handler(struct qdrv_cb *qcb, void *msg)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE,
			"-->Enter  cmd %d %p",
			((struct host_ioctl *) msg)->ioctl_command, msg);

	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_CMM,
			"%u\n", ((struct host_ioctl *) msg)->ioctl_command);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

static int msg_fops_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl *) msg;
	int cmd_id = mp->ioctl_arg1;
	int cmd_arg = mp->ioctl_arg2;
	struct muc_fops_req* fops_req;
	const char* filename = NULL;
	char* fops_data = NULL;
	mm_segment_t old_fs;
	struct file *file;
	loff_t pos = 0;
	u32 open_flags;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	/* argp is already a virtual address */
	fops_req = ioremap_nocache((u32)mp->ioctl_argp, sizeof(*fops_req));

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_TRACE,
			"Entry Cmd %d Arg %d FD %d Ret Val %d\n",
			cmd_id,cmd_arg,fops_req->fd,fops_req->ret_val);
	fops_req->ret_val = -1;


	switch(cmd_id){
		case MUC_FOPS_OPEN:
			if ((fops_req->fd >= ARRAY_SIZE(cal_filenames)) || (fops_req->fd < 0)) {
				return(-1);
			}
			filename = cal_filenames[fops_req->fd];
			open_flags = (cmd_arg & MUC_FOPS_RDONLY ? O_RDONLY : 0)
				| (cmd_arg & MUC_FOPS_WRONLY ? O_WRONLY | O_CREAT : 0)
				| (cmd_arg & MUC_FOPS_RDWR ? O_RDWR : 0)
				| (cmd_arg & MUC_FOPS_APPEND ? O_APPEND : 0);

			DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_TRACE,
					"File name %s\n", filename);

			fops_req->ret_val = sys_open(filename, open_flags, 0);
			if ((fops_req->ret_val < 0) && (open_flags & O_CREAT)){
				if (strstr(filename, "/proc/bootcfg/") != 0) {
					bootcfg_create(&filename[14], 0);
					fops_req->ret_val = sys_open(filename, open_flags, 0);
				}
			}
			break;
		case MUC_FOPS_READ:
			if(fops_req->fd >= 0) {
				fops_data = ioremap_nocache(muc_to_lhost((u32)fops_req->data_buff), cmd_arg);
				if (fops_data) {
					fops_req->ret_val = sys_read(fops_req->fd, fops_data, cmd_arg);
					iounmap(fops_data);
				} else {
					DBGPRINTF_E("could not remap MuC ptr 0x%x (linux 0x%x) for file read, size %u bytes\n",
							(u32)fops_req->data_buff,
							(u32)muc_to_lhost((u32)fops_req->data_buff), (u32)cmd_arg);
				}
			}
			break;
		case MUC_FOPS_WRITE:
			if(fops_req->fd >= 0){
				file = fget(fops_req->fd);
				fops_data = ioremap_nocache(muc_to_lhost((u32)fops_req->data_buff), cmd_arg);
				WARN_ON(!file);
				if (file && fops_data) {
					pos = file->f_pos;
					fops_req->ret_val = vfs_write(file, fops_data, cmd_arg, &pos);
					fput(file);
					file->f_pos = pos;
				}
				if (fops_data) {
					iounmap(fops_data);
				}
			}
			break;
		case MUC_FOPS_LSEEK:
			break;
		case MUC_FOPS_CLOSE:
			if(fops_req->fd >= 0) {
				fops_req->ret_val = sys_close(fops_req->fd);
			}
			break;
	}

	set_fs(old_fs);

	/* clear the argp in the ioctl to prevent MuC from cleaning it up*/
	mp->ioctl_argp = (u32)NULL;

	fops_req->req_state = MUC_FOPS_DONE;

	DBGPRINTF(DBG_LL_NOTICE, QDRV_LF_TRACE, "Exit Cmd %d Arg %d FD %d Ret Val %d\n",
			cmd_id,cmd_arg,fops_req->fd,fops_req->ret_val);

	iounmap(fops_req);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);

}

/**
 * Message from MUC to qdriver indicating a number of TKIP MIC errors (as reported
 * by the MAC hardware).
 *
 * Pass this on to the WLAN driver to thence pass up to the higher layer to act on.
 */
static int msg_tkip_mic_error_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl *) msg;
	u16 count;
	/* IOCTL argument 1 is:
	 * 31      24      16                    0
	 *  | RESVD | UNIT  |      DEVICE ID     |
	 */
	u16 devid = mp->ioctl_arg1 & 0xFFFF;
	u8 unit = mp->ioctl_arg1 & 0xFF0000 >> 24;
	count = mp->ioctl_arg2;
	DBGPRINTF_E("Report from MUC for %d TKIP MIC errors,"
			        " unit %d, devid %d\n", count, unit, devid);
	qdrv_wlan_tkip_mic_error(&qcb->macs[unit], devid, count);
	return(0);
}

static int msg_devchange_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl *) msg;
	u16 devid;
	u16 flags;
	int unit = 0;
	struct net_device *dev = NULL;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	devid = mp->ioctl_arg1 & IOCTL_DEVATTACH_DEVID_MASK;
	flags = mp->ioctl_arg2;

	/* Currently only have a single MAC */
	unit = 0;

	DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM,
			"devid %d unit %d flags 0x%04x\n", devid, unit, flags);

	if(flags & NETDEV_F_EXTERNAL)
	{
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM,
				"NETDEV_F_EXTERNAL\n");
		dev = qcb->macs[unit].vnet[(devid - MAC_UNITS) % QDRV_MAX_VAPS];
	}
	else
	{
		/* "Parent" device */
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM,
				"MAC unit %d is operational\n", unit);

		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(0);
	}

	if(dev == NULL)
	{
		DBGPRINTF_E("No device found.\n");

		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	/* This used to call dev_open() on the parent device - not needed now */

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

static int msg_detach_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl *) msg;
	struct qdrv_mac *mac = NULL;
	u16 devid;
	u16 flags;
	int ret = 0;

	int unit = 0;	/* only have 1 WMAC */
	mac = &qcb->macs[unit];

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	devid = mp->ioctl_arg1;
	flags = mp->ioctl_arg2;

	if (flags & NETDEV_F_EXTERNAL) {
		int i;
		struct net_device *vdev;
		unsigned long resource;

		DBGPRINTF(DBG_LL_ALL, QDRV_LF_CMM,
				"External devid 0x%04x flags 0x%04x\n", devid, flags);

		for (i = 0; i < QDRV_MAX_VAPS; i++) {
			vdev = qcb->macs[unit].vnet[i];
			if (vdev) {
				struct qdrv_vap *qv;
				qv = netdev_priv(vdev);
				DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM, "i %d vdev %p qv %p qv->devid 0x%x\n",
						i, vdev, qv, qv->devid);
				if (qv->devid == devid) {
					break;
				}
			}
			vdev = NULL;
		}

		if (vdev == NULL) {
			DBGPRINTF_E("Could not find net_device for devid: 0x%x\n", devid);
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return -ENODEV;
		}

		if (qdrv_vap_exit_muc_done(mac, vdev) < 0) {
			DBGPRINTF_E("Failed to exit VAP\n");
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return -1;
		}

		resource = QDRV_RESOURCE_VAP(QDRV_WLANID_FROM_DEVID(devid));
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM,
				"removing resource 0x%lx\n", resource);
		qcb->resources &= ~resource;
	} else {
		DBGPRINTF_E("Non external dev detach unimplemented\n");
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return ret;
}

static int msg_attach_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = msg;
	struct host_ioctl_hifinfo *hifinfo = NULL;
	struct qdrv_mac *mac = NULL;
	u16 devid;
	u16 flags;
	u32 version_size;
	int ret = 0;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
#ifdef DEBUG_DGPIO
	printk( "Enter msg_attach_handler\n" );
#endif

	if(hal_range_check_sram_addr((void *) mp->ioctl_argp))
	{

		DBGPRINTF_E("Argument address 0x%08x is invalid\n",
			mp->ioctl_argp);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	devid = mp->ioctl_arg1 & IOCTL_DEVATTACH_DEVID_MASK;
	flags = (mp->ioctl_arg1 & IOCTL_DEVATTACH_DEVFLAG_MASK) >>
		IOCTL_DEVATTACH_DEVFLAG_MASK_S;
	hifinfo = ioremap_nocache(IO_ADDRESS((u32)mp->ioctl_argp), sizeof(*hifinfo));

	version_size = sizeof( qcb->algo_version ) - 1;
	memcpy( qcb->algo_version, hifinfo->hi_algover, version_size );
	qcb->algo_version[ version_size ] = '\0';

	qcb->dspgpios = hifinfo->hi_dspgpios;
#ifdef DEBUG_DGPIO
	printk( "msg_attach_handler, GPIOs: 0x%x\n", qcb->dspgpios );
#endif

	if(flags & NETDEV_F_EXTERNAL)
	{

		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM,
				"External devid 0x%04x flags 0x%04x\n",
				devid, flags);

		mac = &qcb->macs[0];
		if(qdrv_vap_init(mac, hifinfo, mp->ioctl_arg1,
			mp->ioctl_arg2) < 0)
		{
			DBGPRINTF_E("Failed to initialize VAP\n");
			ret = -1;
			goto done;
		}

		qcb->resources |= QDRV_RESOURCE_VAP(QDRV_WLANID_FROM_DEVID(devid));
	}
	else
	{
		DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_CMM,
				"Internal devid 0x%04x flags 0x%04x\n", devid, flags);

		if(devid > (MAC_UNITS - 1))
		{
			DBGPRINTF_E("MAC unit %d is not supported\n", devid);
			ret = -1;
			goto done;
		}

		/* create_qdev() */
		mac = &qcb->macs[devid];
		if(qdrv_wlan_init(mac, hifinfo, mp->ioctl_arg1,
			mp->ioctl_arg2) < 0)
		{
			DBGPRINTF_E("Failed to initialize WLAN feature\n");
			ret = -1;
			goto done;
		}

		/* Mark that we have successfully allocated a resource */
		qcb->resources |= QDRV_RESOURCE_WLAN;
	}

	if (mac) {
		DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE | QDRV_LF_CMM,
				"IOCTL_HLINK_DEVATTACH device enabled\n");
		mac->enabled = 1;
		mac->dead = 0;
	}

done:
	iounmap(hifinfo);
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return ret;
}

static int msg_logattach_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl *) msg;
	u16 devid;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	devid = mp->ioctl_arg1;

	if (qcb->macs[devid].mac_sys_stats != NULL) {
		iounmap(qcb->macs[devid].mac_sys_stats);
	}

	qcb->macs[devid].mac_sys_stats = ioremap_nocache(muc_to_lhost((u32) mp->ioctl_arg2),
							sizeof(*qcb->macs[devid].mac_sys_stats));

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

extern struct _temp_info *p_muc_temp_index;

static int msg_temp_attach_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl *) msg;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Map temp index */
	if (p_muc_temp_index) {
		iounmap(p_muc_temp_index);
	}

	p_muc_temp_index = ioremap_nocache(muc_to_lhost((u32) mp->ioctl_arg1),
					sizeof(*p_muc_temp_index));

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

static int msg_muc_booted_handler(struct qdrv_cb *qcb, void *msg)
{
	qcb->resources |= QDRV_RESOURCE_MUC_BOOTED;
	qdrv_auc_stats_setup();
	return(0);
}

static int msg_disassoc_sta_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl*)msg;
	int ncidx = mp->ioctl_arg1;
	int devid = mp->ioctl_arg2;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = (struct qdrv_wlan*)mac->data;
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211_node *node = NULL;
	struct ieee80211vap *vap;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state == IEEE80211_S_RUN) {
			if (qw->ic.ic_opmode == IEEE80211_M_HOSTAP) {
				node = ieee80211_find_node_by_node_idx(vap, ncidx);
				if (node) {
					ieee80211_disconnect_node(vap, node);
					ieee80211_free_node(node);
				}
			}
		}
	}

	return 0;
}

static int msg_rate_train(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl*)msg;
	uint16_t ncidx = mp->ioctl_arg1 & 0xFFFF;
	uint16_t devid = mp->ioctl_arg1 >> 16;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = (struct qdrv_wlan*)mac->data;
	struct ieee80211_node *ni;

	ni = ieee80211_find_node_by_idx(&qw->ic, NULL, ncidx);
	if (!ni) {
		DBGPRINTF_E("node with idx %u not found\n", ncidx);
		return 0;
	}

	ni->ni_rate_train_hash = mp->ioctl_arg2;
	ieee80211_free_node(ni);

	return 0;
}

static int msg_drop_ba_handler(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl*)msg;
	int relax = mp->ioctl_arg2 >> 24;
	int tid = (mp->ioctl_arg2 >> 16) & 0xFF;
	int devid = mp->ioctl_arg2 & 0xFFFF;
	int send_delba = (mp->ioctl_arg2 >> 24) & 0x0F;
	int ncidx = mp->ioctl_arg1;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = (struct qdrv_wlan*)mac->data;
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211_node *node = NULL;
	struct ieee80211vap *vap;
	int i;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state == IEEE80211_S_RUN) {
			if (qw->ic.ic_opmode == IEEE80211_M_STA) {
				IEEE80211_NODE_LOCK_BH(&vap->iv_ic->ic_sta);
				node = vap->iv_bss;
				if (node) {
					ieee80211_ref_node(node);
				}
				IEEE80211_NODE_UNLOCK_BH(&vap->iv_ic->ic_sta);
			} else {
				node = ieee80211_find_node_by_node_idx(vap, ncidx);
			}
			if (node) {
				if (send_delba) {
					for (i = 0; i < WME_NUM_TID; i++) {
						if (node->ni_ba_rx[i].state == IEEE80211_BA_ESTABLISHED) {
							ieee80211_send_delba(node, i, 0, 39);
							node->ni_ba_rx[i].state = IEEE80211_BA_NOT_ESTABLISHED;
						}
					}
				}
				ieee80211_node_tx_ba_set_state(node, tid,
					IEEE80211_BA_NOT_ESTABLISHED,
					relax ? IEEE80211_TX_BA_REQUEST_LONG_RELAX_TIMEOUT :
						IEEE80211_TX_BA_REQUEST_RELAX_TIMEOUT);
				ieee80211_free_node(node);
				break;
			}
		}
	}

	return 0;
}

static int msg_ba_add_start_handler(struct qdrv_cb *qcb, void *msg)
{
	const struct host_ioctl *mp = (struct host_ioctl*) msg;
	const int tid = (mp->ioctl_arg2 >> 16) & 0xFF;
	const int devid = mp->ioctl_arg2 & 0xFFFF;
	const int node_idx_unmapped = mp->ioctl_arg1;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = mac->data;
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211_node *ni = NULL;
	struct ieee80211vap *vap;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		struct qdrv_vap *qv = container_of(vap, struct qdrv_vap, iv);
		if (vap->iv_state == IEEE80211_S_RUN) {
			ni = ieee80211_find_node_by_node_idx(vap, node_idx_unmapped);
			if (ni) {
				qdrv_tx_ba_establish(qv, ni, tid);
				ieee80211_free_node(ni);
				break;
			}
		}
	}

	return 0;
}

static int msg_peer_rts_handler(struct qdrv_cb *qcb, void *msg)
{
	const struct host_ioctl *mp = (struct host_ioctl*) msg;
	const int devid = mp->ioctl_arg1 & 0xFFFF;
	const int enable = !!mp->ioctl_arg2;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = mac->data;
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211vap *vap;

	/* Save the current status, in case the mode is changed */
	ic->ic_dyn_peer_rts = enable;
	if (ic->ic_peer_rts_mode != IEEE80211_PEER_RTS_DYN) {
		return 0;
	}

	ic->ic_peer_rts = enable;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;
		if (vap->iv_state != IEEE80211_S_RUN)
			continue;
		ic->ic_beacon_update(vap);
	}

	return 0;
}

static int msg_dyn_wmm_handler(struct qdrv_cb *qcb, void *msg)
{
	const struct host_ioctl *mp = (struct host_ioctl*) msg;
	const int devid = mp->ioctl_arg1 & 0xFFFF;
	const int enable = !!mp->ioctl_arg2;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = mac->data;
	struct ieee80211com *ic = &qw->ic;
	struct ieee80211vap *vap;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode != IEEE80211_M_HOSTAP)
			continue;
		if (vap->iv_state != IEEE80211_S_RUN)
			continue;

		ieee80211_wme_updateparams_delta(vap, enable);
	}

	return 0;
}

static int msg_rfic_caused_reboot_handler(struct qdrv_cb *qcb, void *msg)
{
	panic("RFIC reset required\n");

	return (0);
}

static void qdrv_tdls_pti_event(struct host_ioctl *mp,
		struct ieee80211com *ic, struct qtn_tdls_args *tdls_args)
{
	struct ieee80211vap *vap = NULL;
	struct ieee80211_node *ni = NULL;
	enum ieee80211_tdls_operation operation;

	if (likely(tdls_args->tdls_cmd == IOCTL_TDLS_PTI_EVENT)) {
		if (ic->ic_opmode == IEEE80211_M_STA) {
			vap = TAILQ_FIRST(&ic->ic_vaps);
			if (vap && vap->iv_state == IEEE80211_S_RUN)
				ni = ieee80211_find_node(&vap->iv_ic->ic_sta, tdls_args->ni_macaddr);
		}

		if (ni) {
			if (IEEE80211_NODE_IS_TDLS_ACTIVE(ni)) {
				operation = IEEE80211_TDLS_PTI_REQ;
				if (ieee80211_tdls_send_event(ni, IEEE80211_EVENT_TDLS, &operation))
					DBGPRINTF_E("TDLS %s: Send event %d failed\n", __func__, operation);
			}
			ieee80211_free_node(ni);
		} else {
			DBGPRINTF_E("TDLS EVENT: Can find node - "
					"node %pM cmd %d pti 0x%x\n", tdls_args->ni_macaddr,
					tdls_args->tdls_cmd, mp->ioctl_arg2);
		}
	} else {
		DBGPRINTF_E("TDLS EVENT: parameter error for PTI - "
			"node %pM cmd %d pti 0x%x\n", tdls_args->ni_macaddr,
			tdls_args->tdls_cmd, mp->ioctl_arg2);
	}
}

static int msg_tdls_events(struct qdrv_cb *qcb, void *msg)
{
	struct host_ioctl *mp = (struct host_ioctl*)msg;
	int devid = mp->ioctl_arg2;
	struct qdrv_mac *mac = &qcb->macs[devid];
	struct qdrv_wlan *qw = (struct qdrv_wlan*)mac->data;
	struct ieee80211com *ic = &qw->ic;
	struct qtn_tdls_args *tdls_args;
	unsigned long int addr;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	addr = IO_ADDRESS((uint32_t)mp->ioctl_argp);
	tdls_args = ioremap_nocache(addr, sizeof(*tdls_args));

	switch (mp->ioctl_arg1) {
	case IOCTL_TDLS_PTI_EVENT:
		qdrv_tdls_pti_event(mp, ic, tdls_args);
		break;
	default:
		DBGPRINTF_E("Unkown tdls events: arg1 %u arg2 0x%x argp 0x%x\n",
			mp->ioctl_arg1, mp->ioctl_arg2, mp->ioctl_argp);
		break;
	}

	iounmap(tdls_args);
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return (0);
}

static void comm_work(struct work_struct *work)
{
	struct qdrv_cb *qcb = container_of(work, struct qdrv_cb, comm_wq);
	volatile u32 physmp;
	struct host_ioctl *mp;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Initialisation */
	if (qcb->hlink_mbox == 0) {
		/* See if the hostlink address is setup */
		u32 hlink_addr = qdrv_soc_get_hostlink_mbox();
		if (hlink_addr) {
			qcb->hlink_mbox = ioremap_nocache(muc_to_lhost(hlink_addr),
						sizeof(*qcb->hlink_mbox));
		} else {
			panic("Hostlink interrupt, no mailbox setup - reboot\n");
			return;
		}
	}

	/* Take the semaphore */
	while (!sem_take(RUBY_SYS_CTL_L2M_SEM, QTN_SEM_HOST_LINK_SEMNUM));

	/* Get the contents of the message mailbox - physical address */
	physmp = (u32)(*qcb->hlink_mbox);
	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_TRACE | QDRV_LF_CMM, "physmp %08X\n", physmp);

	/* Clear the content of the mailbox. */
	if (physmp) {
		writel_wmb(0, qcb->hlink_mbox);
		/* Translate to a local address. Can't do this earlier or
		 * we'd translate '0' to '0x80000000'
		 */
		physmp = muc_to_lhost(physmp);
	}

	/* Give back the semaphore */
	sem_give(RUBY_SYS_CTL_L2M_SEM, QTN_SEM_HOST_LINK_SEMNUM);
	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_TRACE | QDRV_LF_CMM, "remapped physmp %08X\n", physmp);

	while (physmp) {
		struct host_ioctl loc_ioctl;
		struct host_ioctl *p_save;

		if (hal_range_check_sram_addr((void *) physmp)) {
			DBGPRINTF_E("Message address 0x%08x is invalid\n",physmp);
			DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
			return;
		}

		mp = ioremap_nocache(IO_ADDRESS((u32) physmp), sizeof(*mp));
		loc_ioctl = *mp;
		p_save = mp;
		mp = &loc_ioctl;

		/* Call the right message handle function */
		if (mp->ioctl_command <=  MSG_HANDLER_TABLE_SIZE)
		{
			u_int32_t tmp_argp = mp->ioctl_argp;

			/* If we have argp pointer, translate it */
			if (mp->ioctl_argp) {
				u32 lhost_addr = muc_to_lhost(mp->ioctl_argp);
				if (lhost_addr == RUBY_BAD_BUS_ADDR) {
					panic(KERN_ERR"argp translation is failed: cmd=0x%x\n",
					    (unsigned)mp->ioctl_command);
				}
				mp->ioctl_argp = lhost_addr;
			}
			/* The message code indexes directly into the table */
			if((*s_msg_handler_table[mp->ioctl_command])(qcb, (void *) mp) < 0)
			{
				DBGPRINTF_E("Failed to process message %d\n",
					mp->ioctl_command);
			}
			/* Put argp back */
			mp->ioctl_argp = tmp_argp;
		}
		else
		{
			/* We don't have a handler for this message */
			DBGPRINTF_W("Unknown message %d\n", mp->ioctl_command);
		}

		/* Move to the next one */
		physmp = (u32)mp->ioctl_next;
		if (physmp) {
			DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_HLINK, "Next ioctl %08X\n", physmp);
			physmp = muc_to_lhost(physmp);
			DBGPRINTF(DBG_LL_DEBUG, QDRV_LF_HLINK, "Next ioctl remap %08X\n", physmp);
		}
		p_save->ioctl_rc |= QTN_HLINK_RC_DONE;
		iounmap(p_save);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return;
}

int qdrv_comm_init(struct qdrv_cb *qcb)
{
	struct int_handler int_handler;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* MBOX will be passed in by the MuC when it starts up */
	qcb->hlink_mbox = 0;

	/* Finish the interrupt work in a workqueue */
	INIT_WORK(&qcb->comm_wq, comm_work);

	int_handler.handler = comm_irq_handler;
	int_handler.arg1 = qcb;
	int_handler.arg2 = NULL;

	/* Install handler. Use mac device 0 */
	if (qdrv_mac_set_handler(&qcb->macs[0], RUBY_M2L_IRQ_LO_HLINK, &int_handler) != 0) {
		DBGPRINTF_E("Failed to install handler\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	qdrv_mac_enable_irq(&qcb->macs[0], RUBY_M2L_IRQ_LO_HLINK);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

int qdrv_comm_exit(struct qdrv_cb *qcb)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Make sure work queues are done */
	flush_scheduled_work();

	qdrv_mac_disable_irq(&qcb->macs[0], RUBY_M2L_IRQ_LO_HLINK);

	if (qcb->hlink_mbox) {
		iounmap(qcb->hlink_mbox);
	}

	if (qcb->macs[0].mac_sys_stats) {
		iounmap(qcb->macs[0].mac_sys_stats);
	}

	if (p_muc_temp_index != NULL) {
		iounmap(p_muc_temp_index);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

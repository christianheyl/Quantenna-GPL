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
#include <asm/hardware.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_comm.h"
#include "qdrv_wlan.h"

static void qdrv_scan_irq(void *arg1, void *arg2)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) arg1;
	unsigned long flags;
  
	spin_lock_irqsave(&qw->lock, flags);
	schedule_work(&qw->scan_task);
	spin_unlock_irqrestore(&qw->lock, flags);
}

static void qdrv_scan_work(struct work_struct *work)
{
	struct qdrv_wlan *qw = container_of(work, struct qdrv_wlan, scan_task);
	struct host_scandesc *scan_desc;
	u32* sd;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Protect the scan fifo contention between MuC & Linux using hw sem */
	if(!sem_take(qw->host_sem, qw->scan_if.scan_sem_bit))
	{
		DBGPRINTF_E("Unable to get semaphore - rescedule.\n");
		schedule_work(&qw->scan_task);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return;
	}

	scan_desc = (struct host_scandesc *)
		IO_ADDRESS((u32)*(qw->scan_if.sc_req_mbox));

	sd = (u32 *) (*qw->scan_if.sc_req_mbox);
	if (sd == NULL) {
		sem_give(qw->host_sem, qw->scan_if.scan_sem_bit);
		DBGPRINTF_E("sd NULL\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return;
	}
	writel_wmb(0, qw->scan_if.sc_req_mbox);
	sem_give(qw->host_sem, qw->scan_if.scan_sem_bit);

	/* MATS FIX this. What is it supposed to do */
	/* Call Baseband driver with dev, chan and req_type */
	/* Fake for now. Old wlan_host_scan_process() */
	scan_desc->status = 1;

	if (sem_take(qw->host_sem, qw->scan_if.scan_sem_bit))
	{
		writel_wmb(sd, qw->scan_if.sc_res_mbox);
		sem_give(qw->host_sem, qw->scan_if.scan_sem_bit);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return;
}

int qdrv_scan_start(struct qdrv_mac *mac)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) mac->data;
	struct int_handler int_handler;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	INIT_WORK(&qw->scan_task, qdrv_scan_work);

	int_handler.handler = qdrv_scan_irq;
	int_handler.arg1 = (void *) qw;
	int_handler.arg2 = NULL;

	if(qdrv_mac_set_handler(mac, qw->scanirq, &int_handler) != 0)
	{
		DBGPRINTF_E("Failed to register IRQ handler for %d\n",
			qw->scanirq);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	qdrv_mac_enable_irq(mac, qw->scanirq);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_scan_stop(struct qdrv_mac *mac)
{
	struct qdrv_wlan *qw = (struct qdrv_wlan *) mac->data;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	qdrv_mac_disable_irq(mac, qw->scanirq);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_scan_init(struct qdrv_wlan *qw, struct host_ioctl_hifinfo *hifinfo)
{
	struct host_scanfifo *scan;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/*
	hal_range_check_sram_addr(hifinfo->hi_scanfifo)
	*/

	scan = ioremap_nocache(muc_to_lhost(hifinfo->hi_scanfifo), sizeof(*scan));
	KASSERT(scan != NULL, (DBGEFMT "Unable to ioremap tx done memory area - reboot\n", DBGARG));
	qw->scan_fifo = scan;
	qw->scan_if.sc_res_mbox = (volatile u32 *) &(scan->sf_res);
	qw->scan_if.sc_req_mbox = (volatile u32 *) &(scan->sf_req);
	qw->scanirq = hifinfo->hi_scanirq & IOCTL_DEVATTACH_IRQNUM;
	qw->scan_if.scan_sem_bit = scan->sf_sem;
	qw->scan_if.tx_sem_bit = scan->tx_sem;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_scan_exit(struct qdrv_wlan *qw)
{
	iounmap(qw->scan_fifo);
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

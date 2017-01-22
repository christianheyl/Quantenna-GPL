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

#ifndef _QDRV_MAC_H
#define _QDRV_MAC_H

#include <linux/workqueue.h>
#include <qtn/lhost_muc_comm.h>

#define IEEE80211_ADDR_LEN	6	/* size of 802.11 address */

#define MAC_UNITS		1
#define HOST_INTERRUPTS		16
#define HOST_DSP_INTERRUPTS	16

#define QDRV_RESERVED_DEVIDS		QTN_RESERVED_DEVIDS
#define QDRV_WLANID_FROM_DEVID(devid)	QTN_WLANID_FROM_DEVID(devid)
#define QDRV_MAX_BSS_VAPS 8
#define QDRV_MAX_WDS_VAPS 8
#define QDRV_MAX_VAPS (QDRV_MAX_BSS_VAPS + QDRV_MAX_WDS_VAPS)
#define QDRV_MAX_DEVID (QDRV_MAX_VAPS + QDRV_RESERVED_DEVIDS)

static __always_inline int qdrv_devid_valid(uint8_t devid)
{
	return (devid >= QDRV_RESERVED_DEVIDS) && (devid < QDRV_MAX_DEVID);
}

struct int_handler
{
	void (*handler)(void *arg1, void *arg2);
	void *arg1;
	void *arg2;
};

struct qdrv_mac_params
{
	unsigned int txif_list_max;
	uint8_t mucdbg_netdbg;
};

struct qdrv_mac
{
	int unit;
	struct net_device *vnet[QDRV_MAX_VAPS];
	uint8_t vnet_last;
	uint8_t mac_addr[IEEE80211_ADDR_LEN];
	unsigned int irq;
	uint8_t enabled;
	struct int_handler int_handlers[HOST_INTERRUPTS];
	volatile struct muc_ctrl_reg *reg;
	volatile u32 *mac_host_int_status;
	volatile u32 *mac_host_int_mask;
	volatile u32 *mac_host_sem;
	volatile u32 *mac_uc_intgen;
	struct int_handler mac_host_dsp_int_handlers[HOST_DSP_INTERRUPTS];
	volatile u32 *mac_host_dsp_int_status;
	volatile u32 *mac_host_dsp_int_mask;
	volatile u32 *mac_host_dsp_sem;
	volatile u32 *mac_host_dsp_intgen;
	struct qtn_stats_log *mac_sys_stats;
	void *data;
	struct qdrv_mac_params params; /* Configurable parameters per MAC */
	volatile struct ruby_sys_ctrl_reg *ruby_sysctrl;
	int dead;
	int mgmt_dead;
	int ioctl_fail_count;
	int mac_active_bss;
	int mac_active_wds;
};

int qdrv_mac_init(struct qdrv_mac *mac, u8 *mac_addr, int unit, int irq, struct qdrv_mac_params *params);
int qdrv_mac_exit(struct qdrv_mac *mac);
int qdrv_mac_set_handler(struct qdrv_mac *mac, int irq,
	struct int_handler *handler);
int qdrv_mac_set_host_dsp_handler(struct qdrv_mac *mac, int irq,
	struct int_handler *handler);
int qdrv_mac_clear_handler(struct qdrv_mac *mac, int irq);
void qdrv_mac_interrupt_muc(struct qdrv_mac *mac);
void qdrv_mac_interrupt_muc_high(struct qdrv_mac *mac);
void qdrv_muc_died_sysfs(void);

static __always_inline void
qdrv_mac_enable_irq(struct qdrv_mac *mac, int irq)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	set_bit(irq, (volatile unsigned long *)mac->mac_host_int_mask);
#else
	set_bit(irq, mac->mac_host_int_mask);
#endif
}

static __always_inline void
qdrv_mac_disable_irq(struct qdrv_mac *mac, int irq)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	clear_bit(irq, (volatile unsigned long *)mac->mac_host_int_mask);
#else
	clear_bit(irq, mac->mac_host_int_mask);
#endif

}

static __always_inline void
qdrv_mac_die_action(struct qdrv_mac *mac)
{
	/*
	 * Mark the mac as dead so ioctls and transmissions are
	 * dropped.
	 */
	mac->dead = 1;
	qdrv_muc_died_sysfs();
}

#endif


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
#include <linux/reboot.h>

#include <asm/hardware.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_uc_print.h"
#include "qdrv_soc.h"
#include "qdrv_control.h"
#include <qtn/registers.h>
#include <qtn/mproc_sync_base.h>
#include <qtn/txbf_mbox.h>

#ifdef TOPAZ_AMBER_IP
#include <qtn/amber.h>
#endif

/* Default irq handler for unclaimed interrupts */
static void no_irq_handler(void *arg1, void *arg2)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return;
}

int qdrv_mac_set_handler(struct qdrv_mac *mac, int irq, struct int_handler *handler)
{
	if(irq < 0 || irq >= HOST_INTERRUPTS)
	{
		return(-1);
	}

	/* Copy the handler structure */
	mac->int_handlers[irq] = *handler;

	return(0);
}

int qdrv_mac_set_host_dsp_handler(struct qdrv_mac *mac, int irq, struct int_handler *handler)
{
	if(irq < 0 || irq >= HOST_INTERRUPTS)
	{
		return(-1);
	}

	/* Copy the handler structure */
	mac->mac_host_dsp_int_handlers[irq] = *handler;

	return(0);
}

int qdrv_mac_clear_handler(struct qdrv_mac *mac, int irq)
{
	struct int_handler int_handler;

	/* Install an empty handler so we can avoid checking         */
	/* if a handler is installed every time we take an interrupt */
	int_handler.handler = no_irq_handler;
	int_handler.arg1 = NULL;
	int_handler.arg2 = NULL;

	return(qdrv_mac_set_handler(mac, irq, &int_handler));
}

/* Called by the high priority input to kill off the host and
 * prevent it locking up.
 */
static irqreturn_t qdrv_mac_die(int irq, void *dev_id)
{
	struct qdrv_mac *mac = (struct qdrv_mac *)dev_id;
	u_int32_t status = qtn_mproc_sync_irq_ack_nolock(
		TOPAZ_SYS_CTL_M2L_HI_INT,
		0xFFFF << RUBY_M2L_IPC_HI_IRQ(0) /* high IPC*/);

	if (status & (1 << RUBY_M2L_IRQ_HI_DIE)) {
		DBGPRINTF_E( "IRQ from MAC: dead\n");
		qdrv_mac_die_action(mac);
	}

	if (status & (1 << RUBY_M2L_IRQ_HI_REBOOT)) {
		DBGPRINTF_E( "IRQ from MAC: reboot\n");

		/* MuC ask to restart system.
		 */
#ifdef TOPAZ_AMPER_IP
		amber_set_shutdown_code(AMBER_SD_CODE_EMERGENCY);
#endif

		kernel_restart("MUC restart");
	}


	/* touch uc print buffer work queue to flush shared message buf */
	uc_print_schedule_work();

	return(IRQ_HANDLED);
}

static irqreturn_t __sram_text qdrv_mac_interrupt(int irq, void *dev_id)
{
	int i;
	struct qdrv_mac *mac = (struct qdrv_mac *)dev_id;
	u_int32_t status = qtn_mproc_sync_irq_ack_nolock(
		(u_int32_t)mac->mac_host_int_status,
		0xFFFF /* low IPC*/);

	/* Call the handlers */
	for(i = 0; i < HOST_INTERRUPTS; i++)
	{
		if ((status & (1 << i)) && mac->int_handlers[i].handler)
		{
			(*mac->int_handlers[i].handler)(mac->int_handlers[i].arg1,
				mac->int_handlers[i].arg2);
		}
	}

	/* We are done - See you next time */
	return(IRQ_HANDLED);
}

static irqreturn_t __sram_text qdrv_dsp_interrupt(int irq, void *dev_id)
{
	int i;
	struct qdrv_mac *mac = (struct qdrv_mac *)dev_id;
	u_int32_t status = qtn_txbf_lhost_irq_ack(mac);

	/* Call the handlers */
	for (i = 0; i < HOST_DSP_INTERRUPTS; i++) {
		if ((status & (1 << i)) && mac->mac_host_dsp_int_handlers[i].handler)
		{
//			printk("Interrupt for irq %d dev %p\n", i, dev_id);
			(*mac->mac_host_dsp_int_handlers[i].handler)(
				mac->mac_host_dsp_int_handlers[i].arg1,
				mac->mac_host_dsp_int_handlers[i].arg2);
		}
	}

	/* We are done - See you next time */
	return(IRQ_HANDLED);
}

void __sram_text qdrv_mac_interrupt_muc(struct qdrv_mac *mac)
{
	qtn_mproc_sync_irq_trigger((u_int32_t)mac->mac_uc_intgen, RUBY_L2M_IRQ_HLINK);
}

void __sram_text qdrv_mac_interrupt_muc_high(struct qdrv_mac *mac)
{
	qtn_mproc_sync_irq_trigger((u_int32_t)mac->mac_uc_intgen, RUBY_L2M_IRQ_HIGH);
}

int qdrv_mac_init(struct qdrv_mac *mac, u8 *mac_addr, int unit, int irq, struct qdrv_mac_params *params)
{
	int i;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Reset the MAC data structure */
	memset(mac, 0, sizeof(struct qdrv_mac));

	/* Initialize */
	mac->unit = unit;
	mac->irq = irq;
	mac->reg = (struct muc_ctrl_reg *)
		IO_ADDRESS(MUC_BASE_ADDR + MUC_OFFSET_CTRL_REG);
	mac->ruby_sysctrl = (struct ruby_sys_ctrl_reg *)IO_ADDRESS(RUBY_SYS_CTL_BASE_ADDR);
	memcpy(mac->mac_addr, mac_addr, IEEE80211_ADDR_LEN);
	memcpy(&mac->params, params, sizeof(mac->params));
	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_QCTRL | QDRV_LF_TRACE, "Copied MAC addr etc.\n");

	/* Clear all the interrupt handlers */
	for (i = 0; i < HOST_INTERRUPTS; i++) {
		DBGPRINTF(DBG_LL_CRIT, QDRV_LF_QCTRL | QDRV_LF_TRACE, "Cleared handler %d\n", i);
		qdrv_mac_clear_handler(mac, i);
	}

	/* Set interrupts to low (IRQ) priority */
	mac->reg->mac0_host_int_pri = 0;

	/* Set up pointer to int status register for generic int handler */
	mac->mac_host_int_mask = &mac->ruby_sysctrl->m2l_int_mask;
	mac->mac_host_int_status = &mac->ruby_sysctrl->m2l_int;
	mac->mac_host_sem = &mac->ruby_sysctrl->l2m_sem;
	mac->mac_uc_intgen = &mac->ruby_sysctrl->l2m_int;

	/* Set up pointers for LHost to DSP communication */
	mac->mac_host_dsp_int_mask = &mac->ruby_sysctrl->d2l_int_mask;
	mac->mac_host_dsp_int_status = &mac->ruby_sysctrl->d2l_int;
	mac->mac_host_dsp_sem = &mac->ruby_sysctrl->l2d_sem;
	mac->mac_host_dsp_intgen = &mac->ruby_sysctrl->l2d_int;

	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_QCTRL | QDRV_LF_TRACE, "Requesting IRQs\n");
	/* Register handler with linux for MuC interrupt line. */
	if (request_irq(mac->irq, qdrv_mac_interrupt, IRQF_SAMPLE_RANDOM, "QMAC0low", mac) != 0) {
		DBGPRINTF_E("Can't get MAC0 irq %d\n", mac->irq);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-ENODEV);
	}
	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_QCTRL | QDRV_LF_TRACE, "Requested IRQ QMAC0low %p\n", mac);
	if (request_irq(RUBY_IRQ_IPC_HI, qdrv_mac_die, 0, "QMACDie", mac) != 0) {
		DBGPRINTF_E("Can't get MACDIE irq %d\n", RUBY_IRQ_IPC_HI);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-ENODEV);
	}
	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_QCTRL | QDRV_LF_TRACE, "Requested IRQ DIE %p\n", mac);
	if (request_irq(QTN_TXBF_D2L_IRQ, qdrv_dsp_interrupt, 0, QTN_TXBF_D2L_IRQ_NAME, mac) != 0) {
		DBGPRINTF_E("Can't get DSP irq %d\n", QTN_TXBF_D2L_IRQ);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-ENODEV);
	}
	DBGPRINTF(DBG_LL_CRIT, QDRV_LF_QCTRL | QDRV_LF_TRACE, "Requested IRQ DSP %p\n", mac);

	/* Enable the high priority interrupts */
	for (i = 16; i < 32; i++) {
		qdrv_mac_enable_irq(mac, i);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_mac_exit(struct qdrv_mac *mac)
{
	int i;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (!mac->enabled) {
		DBGPRINTF_E("MAC unit %d is not enabled\n", mac->unit);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	free_irq(mac->irq, mac);
	free_irq(RUBY_IRQ_IPC_HI, mac);
	free_irq(QTN_TXBF_D2L_IRQ, mac);

	for (i = 0; i < HOST_INTERRUPTS; i++) {
		qdrv_mac_disable_irq(mac, i);
		qdrv_mac_clear_handler(mac, i);
	}

	mac->enabled = 0;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

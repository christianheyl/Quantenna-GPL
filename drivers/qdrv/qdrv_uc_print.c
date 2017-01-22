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

#include <linux/version.h>
#include <linux/syscalls.h>
#include <asm/hardware.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_muc.h"
#include "qdrv_uc_print.h"
#include "qdrv_comm.h"
#include "qdrv_control.h"
#include <qtn/shared_params.h>
#include <qtn/shared_print_buf.h>
#include <qtn/mproc_sync_base.h>

#if AUC_LHOST_PRINT_FORMAT
#include <auc/auc_print.h>
#endif

#define	AUC_PREFIX	"AuC"
#define	MUC_PREFIX	"MuC"

static struct shared_print_consumer muc_printbuf = {0};
static struct shared_print_consumer auc_printbuf = {0};
static struct work_struct uc_print_wq;
static struct qdrv_cb *uc_print_qcb = NULL;

/*
 * the uC acts as a producer, writing data into the buffer and updating a count of
 * bytes written. This function acts as a sole consumer, reading data from the
 * uC's buffer line by line.
 *
 * The producer is not aware of the consumer(s), so failure to consume bytes quickly enough
 * result in lost printouts
 */
static void uc_print(struct shared_print_consumer* shared_buf, const char *prefix)
{
#define LINE_MAX	128
	int took_line = 1;

	while(took_line) {
		u32 chars_to_consume = shared_buf->producer->produced - shared_buf->consumed;
		u32 i;
		char stackbuf[LINE_MAX];

		took_line = 0;
		for (i = 0; i < chars_to_consume && i < LINE_MAX - 1; i++) {
			char c = shared_buf->buf[(shared_buf->consumed + i) % shared_buf->producer->bufsize];
			stackbuf[i] = c;
			if (!c || i == LINE_MAX - 2 || c == '\n') {
				took_line = 1;
				if (c == '\n')
					stackbuf[i] = '\0';
				stackbuf[i + 1] = '\0';
				shared_buf->consumed += i + 1;
				if ((uc_print_qcb != NULL) &&
					(uc_print_qcb->macs[0].data != NULL)) {
					qdrv_control_sysmsg_send(uc_print_qcb->macs[0].data,
								stackbuf, i + 1, 0);
				}
				printk(KERN_INFO "%s: %s\n", prefix, stackbuf);
				break;
			}
		}
	}
}

static int uc_print_initialise_shared_data(const char *uc_name, uint32_t addr,
						struct shared_print_consumer *printbuf)
{
	struct shared_print_producer *prod;

	if (!addr) {
		return 0;
	}

	prod = ioremap_nocache(muc_to_lhost(addr), sizeof(*prod));
	if (!prod) {
		panic("%s to lhost printbuf could not translate metadata\n", uc_name);
	}

	printbuf->producer = prod;
	printbuf->buf = ioremap_nocache(muc_to_lhost((u32)prod->buf), prod->bufsize);
	if (!printbuf->buf) {
		panic("%s to lhost printbuf could not translate char buffer %p : %u\n",
			uc_name, prod->buf, prod->bufsize);
	}

	return 1;
}

static int muc_print_initialise_shared_data(void)
{
	return uc_print_initialise_shared_data(MUC_PREFIX,
		qtn_mproc_sync_shared_params_get()->m2l_printbuf_producer,
		&muc_printbuf);
}

static int auc_print_initialise_shared_data(void)
{
	return uc_print_initialise_shared_data(AUC_PREFIX,
		qtn_mproc_sync_shared_params_get()->auc.a2l_printbuf_producer,
		&auc_printbuf);
}

void uc_print_schedule_work(void)
{
	schedule_work(&uc_print_wq);
}

static void muc_print_irq_handler(void *arg1, void *arg2)
{
	uc_print_schedule_work();
}

static void uc_print_work(struct work_struct *work)
{
	if (muc_printbuf.producer || muc_print_initialise_shared_data()) {
		uc_print(&muc_printbuf, MUC_PREFIX);
	}
	if (auc_printbuf.producer || auc_print_initialise_shared_data()) {
#if AUC_LHOST_PRINT_FORMAT
		if (uc_print_auc_cb)
			uc_print_auc_cb(&auc_printbuf);
#else
			uc_print(&auc_printbuf, AUC_PREFIX);
#endif
	}
}

static int qdrv_uc_set_irq_handler(struct qdrv_cb *qcb, void(*handler)(void*, void*), uint32_t num)
{
	struct int_handler int_handler;

	int_handler.handler = handler;
	int_handler.arg1 = qcb;
	int_handler.arg2 = NULL;

	if(qdrv_mac_set_handler(&qcb->macs[0], num, &int_handler) != 0) {
		DBGPRINTF_E( "Set handler failed\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return -1;
	}

	qdrv_mac_enable_irq(&qcb->macs[0], num);

	return 0;
}

int qdrv_uc_print_init(struct qdrv_cb *qcb)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	uc_print_qcb = qcb;

	INIT_WORK(&uc_print_wq, uc_print_work);

	if (qdrv_uc_set_irq_handler(qcb, muc_print_irq_handler, RUBY_M2L_IRQ_LO_PRINT) != 0) {
		return -1;
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

int qdrv_uc_print_exit(struct qdrv_cb *qcb)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	uc_print_qcb = NULL;

	qdrv_mac_disable_irq(&qcb->macs[0], RUBY_M2L_IRQ_LO_PRINT);

	/* TODO: AuC need to use own IRQ handler, similar to RUBY_M2L_IRQ_LO_PRINT. Need  to unregister it here. */

	flush_scheduled_work();

	if (muc_printbuf.buf) {
		iounmap(muc_printbuf.buf);
		muc_printbuf.buf = NULL;
	}

	if (muc_printbuf.producer) {
		iounmap(muc_printbuf.producer);
		muc_printbuf.producer = NULL;
	}

	if (auc_printbuf.buf) {
		iounmap(auc_printbuf.buf);
		auc_printbuf.buf = NULL;
	}

	if (auc_printbuf.producer) {
		iounmap(auc_printbuf.producer);
		auc_printbuf.producer = NULL;
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;
}

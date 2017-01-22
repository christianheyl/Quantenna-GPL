/**
 * Copyright (c) 2012-2013 Quantenna Communications, Inc.
 * All rights reserved.
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
#include <linux/proc_fs.h>
#include <linux/io.h>

#include <linux/netdevice.h>
#include <qtn/topaz_tqe.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_hbm_cpuif.h>
#include <qtn/topaz_hbm.h>
#include <qtn/topaz_congest_queue.h>

#define TOPAZ_DEF_TASKLET_BUDGET	32
#define TOPAZ_DEF_CONGEST_TIMEOUT	(2 * HZ)

#define TOPAZ_HBM_POOL_THRESHOLD	2048

struct topaz_congest_queue *g_congest_queue_ptr = NULL;

void topaz_congest_dump(struct topaz_congest_queue *queue)
{
	struct topaz_congest_q_desc *ptr;
	int i;

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++) {
		ptr = &queue->queues[i];

		if (ptr->valid == 0)
			continue;

		printk("Queue Number: %4d\t", i);
		printk("qlen: %4d head: %4d tail: %4d\t", ptr->qlen, ptr->head, ptr->tail);
		printk("node_id: %4d tid: %4d\n", ptr->node_id, ptr->tid);
	}

	printk("Dump queue length logs:\n");
	for (i = 0; i < TOPAZ_CONGEST_PKT_MAX; i++) {
		printk("%20d:\t%d\n", i, queue->logs[i]);
	}
}
EXPORT_SYMBOL(topaz_congest_dump);

struct reg_congest_stats {
	void (*fn)(void *ctx, uint32_t type, uint8_t q_num, uint32_t q_value);
	void *ctx;
};

struct reg_congest_stats pktlog_update_cgq_stats;
void reg_congest_queue_stats(void (*fn)(void *, uint32_t, uint8_t, uint32_t), void *ctx)
{
	pktlog_update_cgq_stats.fn = fn;
	pktlog_update_cgq_stats.ctx = ctx;
}
EXPORT_SYMBOL(reg_congest_queue_stats);

struct topaz_congest_queue* topaz_congest_queue_get(void)
{
	return g_congest_queue_ptr;
}
EXPORT_SYMBOL(topaz_congest_queue_get);

inline int get_active_tid_num(void)
{
	volatile struct shared_params* sp = qtn_mproc_sync_shared_params_get();
	if (likely(sp)) {
		return sp->active_tid_num;
	} else {
		return 0;
	}
}

void topaz_congest_set_unicast_queue_count(uint32_t qnum)
{
	if (qnum <= TOPAZ_CONGEST_QUEUE_NUM)
		topaz_congest_queue_get()->max_unicast_qcount = qnum;
}
EXPORT_SYMBOL(topaz_congest_set_unicast_queue_count);

struct topaz_congest_q_desc* topaz_congest_alloc_unicast_queue(struct topaz_congest_queue *congest_queue,
														uint32_t node_id,
														uint32_t tid)
{
	struct topaz_congest_q_desc* unicast_queue;

	if (get_active_tid_num() > congest_queue->max_unicast_qcount)
		return NULL;

	if (congest_queue->unicast_qcount >= congest_queue->max_unicast_qcount)
		return NULL;

	unicast_queue = topaz_congest_alloc_queue(congest_queue, node_id, tid);

	if (unicast_queue == NULL)
		return NULL;

	unicast_queue->is_unicast = 1;
	congest_queue->unicast_qcount ++;

	return unicast_queue;
}
EXPORT_SYMBOL(topaz_congest_alloc_unicast_queue);

struct topaz_congest_q_desc* topaz_congest_alloc_queue(struct topaz_congest_queue *congest_queue, uint32_t node_id,	uint32_t tid)
{
	struct topaz_congest_q_desc *queue;
	int i;

	if (congest_queue->total_qlen >= TOPAZ_CONGEST_TOTAL_PKT_MAX)
		return NULL;

	if (topaz_hbm_pool_available(TOPAZ_HBM_BUF_EMAC_RX_POOL) <= TOPAZ_HBM_POOL_THRESHOLD)
		return NULL;

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++) {
		queue = &congest_queue->queues[i];

		if (queue->valid == 0) {
			queue->valid = 1;

			queue->node_id = node_id;
			queue->tid = tid;
			queue->head = 0;
			queue->tail = 0;
			queue->qlen = 0;
			queue->index = i;
			queue->last_retry_success = 1;
			queue->retry_timeout = jiffies + queue->congest_queue->congest_timeout;
			queue->is_unicast = 0;

			congest_queue->ptrs[node_id][tid] = queue;

			return queue;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(topaz_congest_alloc_queue);

__attribute__((section(".sram.text"))) int topaz_congest_enqueue(struct topaz_congest_q_desc* queue, union topaz_tqe_cpuif_ppctl *ppctl)
{
	struct topaz_congest_q_elem *ptr;
	uint32_t index;
	int8_t pool;
	int ret = 0;

	if ((queue->qlen >= TOPAZ_CONGEST_PKT_MAX) ||
			(queue->congest_queue->total_qlen >= TOPAZ_CONGEST_TOTAL_PKT_MAX)) {
		queue->congest_enq_fail++;
		ret = NET_XMIT_CN;
		goto make_stats;
	}

	pool = topaz_hbm_payload_get_pool_bus(ppctl->data.pkt);
	if (topaz_hbm_pool_available(pool) <= TOPAZ_HBM_POOL_THRESHOLD) {
		queue->congest_enq_fail++;
		ret = NET_XMIT_CN;
		goto make_stats;
	}

	queue->congest_queue->logs[queue->qlen]++;

	index = queue->tail;
	ptr = &queue->elems[index];

	ptr->ppctl.raw.ppctl0 = ppctl->raw.ppctl0;
	ptr->ppctl.raw.ppctl1 = ppctl->raw.ppctl1;
	ptr->ppctl.raw.ppctl2 = ppctl->raw.ppctl2;
	ptr->ppctl.raw.ppctl3 = ppctl->raw.ppctl3;
	ptr->ppctl.raw.ppctl4 = ppctl->raw.ppctl4;
	ptr->ppctl.raw.ppctl5 = ppctl->raw.ppctl5;

	if (++index == TOPAZ_CONGEST_PKT_MAX)
		index = 0;

	queue->tail = index;
	queue->qlen++;
	queue->congest_queue->total_qlen++;

make_stats:
	if (pktlog_update_cgq_stats.fn) {
		pktlog_update_cgq_stats.fn(pktlog_update_cgq_stats.ctx,
					TOPAZ_CONGEST_QUEUE_STATS_QLEN,
					queue->index,
					queue->qlen);
		pktlog_update_cgq_stats.fn(pktlog_update_cgq_stats.ctx,
					TOPAZ_CONGEST_QUEUE_STATS_ENQFAIL,
					queue->index,
					queue->congest_enq_fail);
	}

	return ret;
}
EXPORT_SYMBOL(topaz_congest_enqueue);

void topaz_congest_release_queue(struct topaz_congest_queue *congest_queue, uint32_t node_id, uint32_t tid)
{
	struct topaz_congest_q_desc *queue;

	queue = congest_queue->ptrs[node_id][tid];

	BUG_ON(queue->qlen != 0);

	queue->node_id = 0;
	queue->tid = 0;
	queue->head = 0;
	queue->tail = 0;
	queue->qlen = 0;
	queue->last_retry_success = 1;

	queue->valid = 0;

	congest_queue->ptrs[node_id][tid] = NULL;

	if (queue->is_unicast) {
		queue->is_unicast = 0;
		if(congest_queue->unicast_qcount > 0)
			congest_queue->unicast_qcount --;
	}
}
EXPORT_SYMBOL(topaz_congest_release_queue);

inline static union topaz_tqe_cpuif_ppctl *topaz_congest_peek(struct topaz_congest_q_desc *queue)
{
	if (queue->qlen == 0)
		return NULL;

	return &queue->elems[queue->head].ppctl;
}

inline static void topaz_congest_dequeue(struct topaz_congest_q_desc *queue)
{
	if (++queue->head == TOPAZ_CONGEST_PKT_MAX)
		queue->head = 0;

	queue->qlen--;
	queue->congest_queue->total_qlen--;
	if (pktlog_update_cgq_stats.fn)
		pktlog_update_cgq_stats.fn(pktlog_update_cgq_stats.ctx, TOPAZ_CONGEST_QUEUE_STATS_QLEN, queue->index, queue->qlen);

	/* Initial status setting */
	queue->last_retry_success = 1;

	if (queue->qlen == 0)
		topaz_congest_release_queue(queue->congest_queue, queue->node_id, queue->tid);
}

void topaz_hbm_congest_queue_put_buf(const union topaz_tqe_cpuif_ppctl *ppctl)
{
	uint32_t flags;
	uint8_t *buf = ppctl->data.pkt + ppctl->data.buff_ptr_offset;

	local_irq_save(flags);
	topaz_hbm_filter_txdone_buf(buf);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(topaz_hbm_congest_queue_put_buf);

noinline static void topaz_congest_queue_clear(struct topaz_congest_q_desc *queue)
{
	struct topaz_congest_q_elem *ptr;

	queue->congest_drop += queue->qlen;

	while(queue->qlen) {
		ptr = &queue->elems[queue->head];
		topaz_hbm_congest_queue_put_buf(&ptr->ppctl);
		topaz_congest_dequeue(queue);
	}
}

__attribute__((section(".sram.text"))) int topaz_congest_queue_xmit(struct topaz_congest_q_desc *queue, uint32_t budget)
{
	union topaz_tqe_cpuif_ppctl *pp_cntl;
	struct topaz_congest_queue *congest_queue = queue->congest_queue;
	int re_sched = 0;
	int ret;

	congest_queue->xmit_entry++;

	if (queue->last_retry_success == 0) {
		if (time_after(jiffies, queue->retry_timeout)) {
			/* PNPT queue very likely is gone */
			topaz_congest_queue_clear(queue);
			return 0;
		}
	}

	while (1) {
		pp_cntl = topaz_congest_peek(queue);
		if (!pp_cntl)
			break;

		congest_queue->cnt_retries++;

		ret = congest_queue->xmit_func(pp_cntl);

		if (ret == NET_XMIT_CN) {
			queue->last_retry_success = 0;
			re_sched = 1;
			break;
		}

		queue->retry_timeout = jiffies + congest_queue->congest_timeout;
		queue->congest_xmit++;

		/* Transmit successfully */
		topaz_congest_dequeue(queue);

		if (--budget == 0)
			break;
	}

	if (budget == 0)
		re_sched = 1;

	return re_sched;
}
EXPORT_SYMBOL(topaz_congest_queue_xmit);

__attribute__((section(".sram.text"))) void congest_tx_tasklet(unsigned long data)
{
	struct topaz_congest_queue *congest_queue = (struct topaz_congest_queue *)data;
	struct topaz_congest_q_desc *queue;
	int re_sched = 0;
	int ret;
	int i;

	congest_queue->func_entry++;

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++) {
		queue = &congest_queue->queues[i];

		if (queue->valid == 0)
			continue;

		ret = topaz_congest_queue_xmit(queue, congest_queue->tasklet_budget);
		if (ret == 1)
			re_sched = 1;
	}

	if (re_sched == 1) {
		tasklet_schedule(&congest_queue->congest_tx);
	}
}
EXPORT_SYMBOL(congest_tx_tasklet);

struct topaz_congest_queue* topaz_congest_queue_init(void)
{
	struct topaz_congest_queue *queue;
	int i;

	queue = kmalloc(sizeof(struct topaz_congest_queue), GFP_KERNEL | __GFP_ZERO);
	if (queue == NULL) {
		printk(KERN_ERR"Out of memory\n");
		return NULL;
	}
	g_congest_queue_ptr = queue;

	for (i = 0; i < TOPAZ_CONGEST_QUEUE_NUM; i++) {
		queue->queues[i].congest_queue = queue;
		queue->queues[i].last_retry_success = 1;
	}
	queue->tasklet_budget = TOPAZ_DEF_TASKLET_BUDGET;
	queue->congest_timeout = TOPAZ_DEF_CONGEST_TIMEOUT;
	tasklet_init(&queue->congest_tx, congest_tx_tasklet, (unsigned long)queue);
	queue->xmit_func = NULL;
	queue->tasklet_extra_proc = NULL;
	queue->max_unicast_qcount = TOPAZ_CONGEST_MAX_UNICAST_QCOUNT;

	return queue;
}
EXPORT_SYMBOL(topaz_congest_queue_init);

void topaz_congest_queue_exit(struct topaz_congest_queue* queue)
{
	kfree(queue);
}
EXPORT_SYMBOL(topaz_congest_queue_exit);

MODULE_DESCRIPTION("CONGEST QUEUE");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");

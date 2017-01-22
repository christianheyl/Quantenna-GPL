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
#ifndef __DRIVERS_NET_TOPAZ_CONGEST_H
#define __DRIVERS_NET_TOPAZ_CONGEST_H

#include <asm/param.h>
#include <qtn/topaz_tqe_cpuif.h>

/* Total number of congestion queues we will pre-allocated */
#define TOPAZ_CONGEST_QUEUE_NUM	(6)
/* Maximum number of packets in one queue */
#define TOPAZ_CONGEST_PKT_MAX	(2048)
/* Maximum number of packets in all congest queues */
#define TOPAZ_CONGEST_TOTAL_PKT_MAX	(2048)
/* Maximum number of congest queue for unicast frame */
#define TOPAZ_CONGEST_MAX_UNICAST_QCOUNT	(3)

/* Budget for packet number consmued at softirq at one time */
#define TOPAZ_SOFTIRQ_BUDGET  8

#define TOPAZ_PCIE_NODE_MAX	(128)
#define TOPAZ_PCIE_TID_MAX	(16)

#define TOPAZ_CONGEST_QUEUE_STATS_QLEN		0
#define TOPAZ_CONGEST_QUEUE_STATS_ENQFAIL	1

struct topaz_congest_q_elem {
	union topaz_tqe_cpuif_ppctl ppctl;
};

/* Congestion queue descriptor */
struct topaz_congest_q_desc {
	struct topaz_congest_q_elem elems[TOPAZ_CONGEST_PKT_MAX];
	uint32_t head;	/* The index of packet to be sent	*/
	uint32_t tail;	/* The index of packet to be append	*/
	uint32_t qlen;	/* Total number of pending requests per queue*/

	uint32_t valid;
	uint32_t index;
	uint32_t node_id;
	uint32_t tid;

	uint32_t congest_xmit;	/* Packet number forwarded successfully */
	uint32_t congest_drop;	/* Packet number dropped due to transmission time-out */
	uint32_t congest_enq_fail;	/* packet number dropped due to enqueue failure */

	uint32_t last_retry_success;	/* 0: Fail, 1: Success */
	unsigned long retry_timeout;
	uint32_t is_unicast;

	struct topaz_congest_queue *congest_queue;
};

struct topaz_congest_queue {
	struct vmac_priv *vmp;
	int (*xmit_func)(union topaz_tqe_cpuif_ppctl *);
	int (*tasklet_extra_proc)(void *);

        struct topaz_congest_q_desc queues[TOPAZ_CONGEST_QUEUE_NUM];

	/* A pointer array, if queues[node_id][tid] is not NULL, node-tid queue is congested
	* and it will point to attached congested queue.
	*/
	struct topaz_congest_q_desc* ptrs[TOPAZ_PCIE_NODE_MAX][TOPAZ_PCIE_TID_MAX];
	struct tasklet_struct congest_tx;

	/* Counters */
	uint32_t func_entry;	/* tasklet hook function called count */
	uint32_t cnt_retries;	/* tried times on triggering to TQE */
	uint32_t xmit_entry;

	int logs[TOPAZ_CONGEST_PKT_MAX]; /* Used to check queue fullness */

	uint32_t congest_timeout;
	uint32_t tasklet_budget;

	uint32_t total_qlen;	/* Total number of pending requests in all queues*/

	uint32_t unicast_qcount;	/* Total congest queue count of unicast frame */
	uint32_t max_unicast_qcount;	/* Max unicast congest queue count*/
};

struct qdrv_tqe_cgq {
	uint32_t	congest_qlen;
};
/**
* Return NULL if node-tid pair is not congested, not NULL otherwise.
*/
RUBY_INLINE int
topaz_queue_congested(struct topaz_congest_queue *congest_queue, uint32_t node_id, uint32_t tid)
{
	BUG_ON(node_id >= TOPAZ_PCIE_NODE_MAX);
	BUG_ON(tid >= TOPAZ_PCIE_TID_MAX);

	return (int)congest_queue->ptrs[node_id][tid];
}

RUBY_INLINE struct topaz_congest_q_desc*
topaz_get_congest_queue(struct topaz_congest_queue *congest_queue, uint32_t node_id, uint32_t tid)
{
	return congest_queue->ptrs[node_id][tid];
}

static inline uint32_t get_timestamp(void)
{
	return read_new_aux_reg(ARC_REG_TIMER1_CNT);
}

/**
* Return NULL if failed
*/
extern struct topaz_congest_queue* topaz_congest_queue_init(void);

extern void topaz_congest_queue_exit(struct topaz_congest_queue* congest_queue);

/**
* Push ppctl into congestion queue.
*/

extern int topaz_congest_enqueue(struct topaz_congest_q_desc* queue, union topaz_tqe_cpuif_ppctl *ppctl);

extern void topaz_congest_dump(struct topaz_congest_queue *queue);

extern struct topaz_congest_q_desc* topaz_congest_alloc_unicast_queue(struct topaz_congest_queue *congest_queue,
																		uint32_t node_id,
																		uint32_t tid);

extern struct topaz_congest_q_desc* topaz_congest_alloc_queue(struct topaz_congest_queue *congest_queue, uint32_t node_id, uint32_t tid);

extern int topaz_congest_queue_xmit(struct topaz_congest_q_desc *queue, uint32_t budget);

extern void reg_congest_queue_stats(void (*fn)(void *, uint32_t, uint8_t, uint32_t), void *ctx);

extern struct topaz_congest_queue* topaz_congest_queue_get(void);

extern void topaz_hbm_congest_queue_put_buf(const union topaz_tqe_cpuif_ppctl *ppctl);

extern void topaz_congest_set_unicast_queue_count(uint32_t qnum);
#endif

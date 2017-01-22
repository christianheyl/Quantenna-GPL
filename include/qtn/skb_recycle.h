/*
 * (C) Copyright 2011 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __QTN_SKBUFF_H
#define __QTN_SKBUFF_H

#include <common/queue.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/interrupt.h>

#include "qtn_global.h"
#include "qtn_skb_size.h"
#include "qtn_buffers.h"

#define SKB_RECYCLE_STATS

#ifdef SKB_RECYCLE_STATS
#define SKB_RECYCLE_STAT(x)	((x) = (x) + 1)
#else
#define SKB_RECYCLE_STAT(x)
#endif

#define QTN_SKB_RECYCLE_ENABLE		0

/**
 * \addtogroup LHOST_STATS
 */
/** @{ */

/**
 * \brief Linux SKB recycle statistics.
 *
 * These statistics are generated and counted per-interface. They are used
 * primarily as a performance metric - the more recylcling that succeeds, the
 * more efficient the system will be (less chance for bottlenecks/dropped
 * packets).
 */
struct qtn_skb_recycle_stats {
	/**
	 * The number of packets for the given interface that have been
	 * successfully recycled.
	 */
	u32 free_recycle_pass;

	/**
	 * The number of packets for the given interface that have failed to
	 * be recycled. If this counter is constantly increasing at a rapid
	 * rate (approx. the same as the per-packet count of traffic streams),
	 * then this can indicate a performance issue.
	 */
	u32 free_recycle_fail;

	/**
	 * This counter shows the number of undersized packets that have been
	 * incorrectly pushed to the recycle code.
	 *
	 * \note It is an error for this counter to be anything other than
	 * zero.
	 */
	u32 free_recycle_fail_undersize;

	/**
	 * This counter shows the number of packets that have been allocated
	 * off the shared buffer pool.
	 */
	u32 alloc_recycle;

	/**
	 * This counter shows the number of packets that have dropped back to
	 * the kernel alloc function due to not having any packets in the
	 * recycle pool.
	 */
	u32 alloc_kmalloc;
};
/** @} */

struct qtn_skb_recycle_list {
	struct sk_buff_head list;			/* Shared buffers between wireless and Ethernet driver */
	int		max;				/* Maximum size of the skb_list */
	struct qtn_skb_recycle_stats stats_qdrv;	/* skb free/alloc stats for qdrv */
	struct qtn_skb_recycle_stats stats_eth;		/* skb free/alloc stats for ethernet driver */
#if defined(CONFIG_RUBY_PCIE_HOST) || defined(CONFIG_RUBY_PCIE_TARGET) \
	|| defined(CONFIG_TOPAZ_PCIE_HOST) || defined(CONFIG_TOPAZ_PCIE_TARGET)
	struct qtn_skb_recycle_stats stats_pcie;	/* skb free/alloc stats for pcie driver */
#endif
	struct qtn_skb_recycle_stats stats_kfree;	/* skb free stats for the kfree_skb collector */
	int (*recycle_func)(struct qtn_skb_recycle_list *recycle_list,
				struct sk_buff *skb);	/* skb recycling check function */
};


/* Define RX buffer size and mapping */
__inline__ static unsigned long rx_buf_map_size(void)
{
	return RX_BUF_SIZE + roundup(NET_IP_ALIGN, dma_get_cache_alignment());
}
__inline__ static unsigned long qtn_rx_buf_size(void)
{
	/*
	 * Make sure that we can flush cache (both beginning and ending
	 * must be aligned on cache line) - otherwise flush would not work.
	 * Also make sure that we can reserve NET_IP_ALIGN
	 * at the beginning of data after do cache flush.
	 * Without NET_IP_ALIGN reserving IP header will be not aligned
	 * and network stack can kick unaligned access exception, which is expensive
	 * or even would crash kernel if unaligned access handler is not implemented.
	 */
	return rx_buf_map_size() + dma_get_cache_alignment() - 1;
}

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,30)
static inline struct dst_entry *skb_dst(struct sk_buff *skb)
{
	return skb->dst;
}
#endif

static __inline__ int __qtn_skb_recyclable_check(struct qtn_skb_recycle_stats *stats, struct sk_buff *skb)
{
	if (!QTN_SKB_RECYCLE_ENABLE) {
		return 0;
	}

	if (unlikely(skb->next)) {
		printk(KERN_EMERG "skb being recycled: 0x%p is queued\n", skb);
		return 0;
	}

	if (!skb->is_recyclable ||
			skb->next ||
			skb_dst(skb) ||
			skb_shared(skb) ||
			skb_is_nonlinear(skb) ||
			skb_shinfo(skb)->nr_frags ||
			skb_shinfo(skb)->frag_list ||
			skb_shinfo(skb)->gso_size ||
			skb_cloned(skb) ||
			atomic_read(&(skb_shinfo(skb)->dataref)) != 1) {
		return 0;
	}

	/* check for undersize skb; this should never happen, and indicates problems elsewhere */
	if (skb_end_pointer(skb) - skb->head < qtn_rx_buf_size()) {
		SKB_RECYCLE_STAT(stats->free_recycle_fail_undersize);
		return 0;
	}

	return 1;
}

static __inline__ struct sk_buff *qtn_skb_recycle_list_pop(
		struct qtn_skb_recycle_list *recycle_list,
		struct qtn_skb_recycle_stats *stats)
{
	struct sk_buff *skb = NULL;
	unsigned long flags;

	if (!QTN_SKB_RECYCLE_ENABLE) {
		return NULL;
	}

	spin_lock_irqsave(&recycle_list->list.lock, flags);
	skb = __skb_dequeue(&recycle_list->list);
	if (skb) {
		SKB_RECYCLE_STAT(stats->alloc_recycle);
	} else {
		SKB_RECYCLE_STAT(stats->alloc_kmalloc);
	}
	spin_unlock_irqrestore(&recycle_list->list.lock, flags);

	return skb;
}

/*
 * Push a used skb onto the recycle list. returns 1 if it was pushed onto the list
 */
static __inline__ int qtn_skb_recycle_list_push(struct qtn_skb_recycle_list *recycle_list,
		struct qtn_skb_recycle_stats *stats, struct sk_buff *skb)
{
	int pushed = 0;
	unsigned long flags;
	struct skb_shared_info *shinfo;

	if (!QTN_SKB_RECYCLE_ENABLE) {
		return 0;
	}

	spin_lock_irqsave(&recycle_list->list.lock, flags);

	if (skb_queue_len(&recycle_list->list) < recycle_list->max) {
		if (__qtn_skb_recyclable_check(stats, skb)) {
			if (skb->destructor) {
				WARN_ON(in_irq());
				skb->destructor(skb);
				skb->destructor = NULL;
			}

			skb->len = 0;
			skb->priority = 0;
			skb->dest_port = 0;
			skb->src_port = 0;
			skb->is_recyclable = 0;
			skb->tail = skb->data = skb->head;
			skb->vlan_tci = 0;
			skb->orig_dev = NULL;
			skb_reserve(skb, NET_SKB_PAD);

			memset(skb->cb, 0, sizeof(skb->cb));
			memset(&skb->qtn_cb, 0, sizeof(skb->qtn_cb));

			shinfo = skb_shinfo(skb);
			memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));

			__skb_queue_tail(&recycle_list->list, skb);
			pushed = 1;
		}
	}

	spin_unlock_irqrestore(&recycle_list->list.lock, flags);

	if (pushed) {
		SKB_RECYCLE_STAT(stats->free_recycle_pass);
	} else {
		SKB_RECYCLE_STAT(stats->free_recycle_fail);
	}

	return pushed;
}

static __inline__ struct qtn_skb_recycle_list *qtn_get_shared_recycle_list(void)
{
	extern struct qtn_skb_recycle_list __qtn_skb_recycle_list;
	return &__qtn_skb_recycle_list;
}

#endif // __QTN_SKBUFF_H


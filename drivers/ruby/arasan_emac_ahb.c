/**
 * Copyright (c) 2011-2012 Quantenna Communications, Inc.
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
#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/crc32.h>
#include <linux/phy.h>

#include <trace/skb.h>
#include <trace/ippkt.h>

#include <asm/board/soc.h>
#include <asm/board/board_config.h>
#include <asm/board/dma_cache_ops.h>

#include <common/queue.h>

#include <qtn/skb_recycle.h>
#include <qtn/qtn_buffers.h>
#include <qtn/qtn_global.h>
#include <qtn/emac_debug.h>

#include "arasan_emac_ahb.h"
#include "emac_lib.h"
#include <compat.h>

#define DRV_NAME	"emac_eth"
#define DRV_VERSION	"1.0"
#define DRV_AUTHOR	"Quantenna Communications, Inc."
#define DRV_DESC	"Arasan AHB-EMAC on-chip Ethernet driver"

#define TIMEOUT_DMA_STATUS_IRQ	50	/*ms*/

/*
 * 125MHz AHBCLK leads to a max mitigation timeout value of 0.524ms.
 * Under extreme conditions, this is the max packet rx delay, an acceptable
 * one. The benefit will be maximum rx interrupt mitigation.
 * Hence, choose 0xFFFF for EMAC_IRQ_MITIGATION_TIMEOUT
 *
 * Under a 400Mbps rx data flow, roughly 16 to 20 packets arrive at
 * EMAC, thus choose 0x10 for EMAC_IRQ_MITIGATION_FRAME_COUNTER
 * NOTE: Adjust EMAC_IRQ_MITIGATION_FRAME_COUNTER value if you think is appropriate
 */
#define EMAC_IRQ_MITIGATION_FRAME_COUNTER	0x10
#define EMAC_IRQ_MITIGATION_TIMEOUT		0xFFFF
#define EMAC_MITIGATION_TIMER_FREQ		(HZ/10) /* 100 ms */
#define EMAC_MITIGATION_EN_THRESHOLD		250 /* no. of interrupts */
#define EMAC_MITIGATION_DIS_THRESHOLD		100 /* no. of interrupts */
#define EMAC_MITIGATION_ENABLED			01
#define EMAC_MITIGATION_DISABLED		00
#define EMAC_ERROR_FRAME_MASK			(RxDescStatusAlignErr | RxDescStatusRuntFrame |	\
			RxDescStatusCRCErr | RxDescStatusMaxLenErr | RxDescStatusJabberErr)

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE("GPL");

static struct net_device* emac_probe(int port_num);
static void bring_up_interface(struct net_device *dev);
static void shut_down_interface(struct net_device *dev);
static int emac_open(struct net_device *dev);
static int emac_close(struct net_device *dev);
static int arasan_proc_rd(char *buf, char **start, off_t offset, int count, int *eof, void *data);

static struct {
	u32 base_addr;
	u32 mdio_base_addr;
	int irq;
	const char *proc_name;
	int phydev_addr;
	struct net_device *dev;
} iflist[] = {
	{
		IO_ADDRESS(RUBY_ENET0_BASE_ADDR),
		IO_ADDRESS(RUBY_ENET0_BASE_ADDR),
		RUBY_IRQ_ENET0,
		"arasan_emac0",
		-1,
		NULL
	},
	{
		IO_ADDRESS(RUBY_ENET1_BASE_ADDR),
		IO_ADDRESS(RUBY_ENET0_BASE_ADDR),
		RUBY_IRQ_ENET1,
		"arasan_emac1",
		-1,
		NULL
	},
};

/* Disable interrupts */
inline static void disable_emac_ints(struct emac_private *arap)
{
	struct emac_common *arapc = &arap->com;

	/* Disable all ints from block to central interrupt controller */
	emac_wr(arapc, EMAC_MAC_INT_ENABLE, 0);
	emac_wr(arapc, EMAC_DMA_INT_ENABLE, 0);
	/* Clear any remaining interrupts */
	emac_wr(arapc, EMAC_MAC_INT, emac_rd(arapc, EMAC_MAC_INT));
	emac_wr(arapc, EMAC_DMA_STATUS_IRQ, emac_rd(arapc, EMAC_DMA_STATUS_IRQ));
}

static int __sram_text allocate_new_rx_buf(struct net_device *dev, int i)
{
	struct emac_private *arap = netdev_priv(dev);
	struct emac_common *arapc = &arap->com;
	struct sk_buff *skb = NULL;
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();

	/* Initialization */
	/* Make sure that driver is not using this buffer */
	arap->rxbufs[i] = NULL;
	/* Make sure that DMA controller is not using this buffer */
	arapc->rx.descs[i].status = 0;
	/* Clear the DMA mapping start for an buffer previously attached */
	arapc->rx.descs[i].bufaddr2 = 0;
	/* Set up control field */
	arapc->rx.descs[i].control = min(RX_BUF_SIZE_PAYLOAD, RxDescBuf1SizeMask) << RxDescBuf1SizeShift;
	if (i >= RUBY_EMAC_NUM_RX_BUFFERS - 1) {
		arapc->rx.descs[i].control |= RxDescEndOfRing;
	}

	/*
	 * Allocate socket buffer.
	 * Oversize the buffer to allow for cache line alignment.
	 */
	if (recycle_list) {
		skb = qtn_skb_recycle_list_pop(recycle_list, &recycle_list->stats_eth);
	}
	if (!skb) {
		size_t size;
#if TOPAZ_HBM_SKB_ALLOCATOR_DEFAULT
		size = TOPAZ_HBM_BUF_EMAC_RX_SIZE / 2;
#else
		size = qtn_rx_buf_size();
#endif
		skb = dev_alloc_skb(size);
	}

	if (!skb) {
		arap->rx_skb_alloc_failures++;
		return -1;
	}

	skb->recycle_list = recycle_list;
	skb->dev = dev;
	arap->rxbufs[i] = skb;

	trace_skb_perf_start(skb, 0);
	trace_skb_perf_stamp_call(skb);

	/* Move skb->data to a cache line boundary */
	skb_reserve(skb, align_buf_dma_offset(skb->data));

	/* Invalidate cache and map virtual address to bus address. */
	arap->rxdmabufs[i] = cache_op_before_rx(skb->data, rx_buf_map_size(),
						skb->cache_is_cleaned) + NET_IP_ALIGN;
	skb->cache_is_cleaned = 0;
	arapc->rx.descs[i].bufaddr1 = arap->rxdmabufs[i];
	/* buffaddr2 value is ignored by the DMA (its length is specified as
	 * zero), so this is a handy place to store the DMA mapping start
	 * for use later when the buffer is passed over to Linux.
	 */
	skb_reserve(skb, NET_IP_ALIGN);
	arapc->rx.descs[i].bufaddr2 = (u32)skb->data;

	/* Hand off buffer to DMA controller */
	arapc->rx.descs[i].status = RxDescOwn;

	return 0;
}
static void enable_emac_ints(struct emac_private *arap)
{
	struct emac_common *arapc = &arap->com;

	/* Clear any pending interrupts */
	emac_wr(arapc, EMAC_MAC_INT, emac_rd(arapc, EMAC_MAC_INT));
	emac_wr(arapc, EMAC_DMA_STATUS_IRQ, emac_rd(arapc, EMAC_DMA_STATUS_IRQ));
	/* Enable selected ints from block to central interrupt controller */
	emac_wr(arapc, EMAC_MAC_INT_ENABLE, MacUnderrun | MacJabber);
	emac_wr(arapc, EMAC_DMA_INT_ENABLE, DmaTxDone| DmaRxDone | DmaMacInterrupt);
}
__always_inline static volatile struct emac_desc* current_rx_desc(struct emac_private *arap)
{
	return arap->com.rx.descs + arap->rx_index;
}

__always_inline static void disable_emac_irq_mitigation(struct emac_private *arap)
{
	emac_wr(&arap->com, EMAC_DMA_RX_IRQ_MITIGATION, ~(1 << 31));
}

__always_inline static void enable_emac_irq_mitigation(struct emac_private *arap, u_int32_t frame_counter, u_int32_t timeout)
{
	emac_wr(&arap->com, EMAC_DMA_RX_IRQ_MITIGATION,
		(frame_counter & 0xFF) |
		((timeout & 0xFFFF) << 8) |
		(1 << 31) /*enable mitigation*/);
}

static __be16 emac_eth_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct ethhdr *eth;
	unsigned char *rawp;

	skb->mac_header = skb->data;
	skb_pull(skb, ETH_HLEN);
	eth = eth_hdr(skb);

	if (is_multicast_ether_addr(eth->h_dest)) {
		if (!compare_ether_addr(eth->h_dest, dev->broadcast))
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	}

	if (ntohs(eth->h_proto) >= 1536)
		return eth->h_proto;

	rawp = skb->data;

	/*
	 *      This is a magic hack to spot IPX packets. Older Novell breaks
	 *      the protocol design and runs IPX over 802.3 without an 802.2 LLC
	 *      layer. We look for FFFF which isn't a used 802.2 SSAP/DSAP. This
	 *      won't work for fault tolerant netware but does for the rest.
	 */
	if (*(unsigned short *)rawp == 0xFFFF)
		return htons(ETH_P_802_3);

	/*
	 *      Real 802.2 LLC
	 */
	return htons(ETH_P_802_2);
}

#ifdef CONFIG_QVSP
static __sram_text int emac_rx_vsp_should_drop(struct sk_buff *skb, struct ethhdr *eh)
{
	u8 *data_start;
	u16 ether_type = 0;

	if (qvsp_is_active(emac_qvsp.qvsp) && emac_qvsp.qvsp_check_func) {
		data_start = qdrv_sch_find_data_start(skb, (struct ether_header *)eh, &ether_type);
		qdrv_sch_classify(skb, ether_type, data_start);
		if (emac_qvsp.qvsp_check_func(emac_qvsp.qvsp, QVSP_IF_ETH_RX, skb,
				data_start, skb->len - (data_start - skb->data),
				skb->priority)) {
			trace_ippkt_dropped(TRACE_IPPKT_DROP_RSN_VSP, 1, 0);
			return 1;
		}
	}

	return 0;
}
#else
#define emac_rx_vsp_should_drop(_skb, _data_start)	(0)
#endif /* CONFIG_QVSP */

static int __sram_text emac_rx_poll (struct napi_struct *napi, int budget)
{
	struct emac_private *arap = container_of(napi, struct emac_private, napi);
	struct emac_common *arapc = &arap->com;
	struct ethhdr *eth;
	u32 status, dma_ints;
	int processed = 0;
	struct sk_buff *skb_tmp = NULL, *skb;
	struct sk_buff_head skb_list;

	if ((current_rx_desc(arap)->status & RxDescOwn)) {
		napi_complete(napi);
		dma_ints = emac_rd(arapc, EMAC_DMA_INT_ENABLE);
		dma_ints |= DmaRxDone;
		emac_wr(arapc, EMAC_DMA_INT_ENABLE, dma_ints);
		return 0;
	}

	__skb_queue_head_init(&skb_list);

	while (!((status = current_rx_desc(arap)->status) & RxDescOwn) && (processed < budget)) {
		skb = arap->rxbufs[arap->rx_index];

		trace_skb_perf_stamp_call(skb);

		if (!skb) {
			/*
			 * Buffer for this index was used during prev iteration
			 * and then new buffer failed to allocate.
			 * So skip processing and try to allocate it again.
			 */
		} else if (status & EMAC_ERROR_FRAME_MASK) {
			printk(KERN_ERR
				"%s: Discarding corrupted frame and reset buffer. (%08X)\n",
				arapc->dev->name, status);
				dev_kfree_skb(skb);
				bring_up_interface(arapc->dev);
		} else if ((status & RxDescFirstDesc) && (status & RxDescLastDesc)) {
			u32 length = (status >> RxDescFrameLenShift) & RxDescFrameLenMask;
#ifdef INSERT_PCIE_DMA_TES
			extern void pcie_dma_tst(u32 local_vaddr, u32 local_paddr, int len);
			pcie_dma_tst((u32) skb->data, (u32) arap->rxdmabufs[arap->rx_index], length);
#endif
			skb_put(skb, length);
			skb->protocol = emac_eth_type_trans(skb, arap->com.dev);
			skb->src_port = 0;
			processed++;

			eth = eth_hdr(skb);
			trace_ippkt_check(eth, skb->len, TRACE_IPPKT_LOC_ETH_RX);
			if (unlikely(!compare_ether_addr(eth->h_dest, arap->com.dev->dev_addr) ||
						skb->pkt_type == PACKET_BROADCAST ||
						skb->pkt_type == PACKET_MULTICAST)) {
				skb->is_recyclable = 0;
			} else if (emac_rx_vsp_should_drop(skb, eth)) {
				dev_kfree_skb(skb);
				skb = NULL;
			} else {
				skb->is_recyclable = 1;
			}
			if (likely(skb)) {
				if (unlikely(!skb_tmp)) {
					__skb_queue_head(&skb_list, skb);
				} else {
					__skb_append(skb_tmp, skb, &skb_list);
				}
				skb_tmp = skb;
			}
		} else {
			/*
			 * This is bad.  We have set the hardware to discard frames over a
			 * certain size and allocated single buffers large enough to take
			 * frames smaller than that.  Any frames we get should therefore
			 * fit into a single buffer, but this frame is fragmented across
			 * multiple buffers - which should never happen.
			 */
			if (status & RxDescFirstDesc) {
				printk(KERN_ERR
						"%s: Discarding initial frame fragment. (%08X)\n",
						arap->com.dev->name, status);
			} else if (status & RxDescLastDesc) {
				u32 length = (status >> RxDescFrameLenShift) & RxDescFrameLenMask;
				printk(KERN_ERR
						"%s: Discarding final frame fragment.\n"
						"Frame length was %lu bytes. (%08X)\n",
						arap->com.dev->name, (unsigned long)length, status);
				arap->rx_fragmented_frame_discards++;
			} else {
				printk(KERN_ERR
						"%s: Discarding intermediate frame fragment (status %08X).\n",
						arap->com.dev->name, status);
			}
			dev_kfree_skb(skb);
		}
		arap->com.dev->last_rx = jiffies;

		/* We are done with the current buffer attached to this descriptor, so attach a new one. */
		if (allocate_new_rx_buf(arap->com.dev, arap->rx_index)) {
			/* Failed to attach new buffer.
			 * If allocating is failed then it is not a problem - during one of next
			 * iterations allocating will be tried again.
			 */
			break;
		} else {
			emac_wr(&arap->com, EMAC_DMA_RX_POLL_DEMAND, 0);
			if (++arap->rx_index >= RUBY_EMAC_NUM_RX_BUFFERS) {
				arap->rx_index = 0;
			}
		}

		//GPIO_DBG_CLR(PROF_POINT_2);	
	}

	skb_queue_walk_safe(&skb_list, skb, skb_tmp) {
		skb->next = NULL;
		netif_receive_skb(skb);
	}

	if ((current_rx_desc(arap)->status & RxDescOwn) && (processed < budget)) {
		napi_complete(napi);
		dma_ints = emac_rd(&arap->com, EMAC_DMA_INT_ENABLE);
		dma_ints |= DmaRxDone;
		emac_wr(&arap->com, EMAC_DMA_INT_ENABLE, dma_ints);
	}

	return processed;
}

__always_inline static volatile struct emac_desc* current_tx_desc(struct emac_private *arap)
{
	return arap->com.tx.descs + arap->arap_data.tx_head;
}

static void __sram_text emac_tx_stop_queue(struct net_device *dev, unsigned wake_idx)
{
	struct emac_private *arap = netdev_priv(dev);
	volatile struct emac_desc *ptx_desc = arap->com.tx.descs + wake_idx;
	volatile struct emac_desc *prev_ptx_desc = arap->com.tx.descs + EMAC_TX_BUFF_INDX(wake_idx - 1);
	int stopped = 0;
	unsigned long flags;

	local_irq_save(flags);
	if (likely(prev_ptx_desc->status & ptx_desc->status & TxDescOwn)) {
		ptx_desc->control |= TxDescIntOnComplete; /* better to keep it first in block */
		netif_stop_queue(dev);
		arap->tx_queue_stopped = 1;
		stopped = 1;
	}
	local_irq_restore(flags);

	if (likely(stopped)) {
		arap->arap_data.tx_full = 1;
		arap->arap_data.tx_stopped_count++;
	}
}

static int __sram_text emac_tx_pass(struct net_device *dev)
{
#ifndef CONFIG_ARCH_RUBY_EMAC_SMOOTHING

	return 1;

#else

	const unsigned burst_packets = CONFIG_ARCH_RUBY_EMAC_SMOOTHING_BURST_SIZE;
	const unsigned pend_packets_wake_queue = ((burst_packets >> 1) + (burst_packets >> 2));
	const uint64_t tokens_per_packet = (NSEC_PER_SEC / CONFIG_ARCH_RUBY_EMAC_SMOOTHING_RATE);
	const uint64_t max_accum_tokens = (burst_packets * tokens_per_packet);

	static uint64_t tokens = 0;
	static ktime_t last_ts = {0,};

	struct emac_private *arap = netdev_priv(dev);
	int ret = 0;

	/*
	 * If currently have no enough tokens to pass packet
	 * try to put more tokens to bucket based on passed time.
	 */
	if (unlikely(tokens < tokens_per_packet)) {
		struct timespec now_ts;
		ktime_t now;

		ktime_get_ts(&now_ts);
		now = timespec_to_ktime(now_ts);

		tokens += ktime_to_ns(now) - ktime_to_ns(last_ts);
		if (tokens > max_accum_tokens) {
			tokens = max_accum_tokens;
		}

		last_ts = now;
	}

	/* If have enough tokens to pass packet remove these tokens from bucket. */
	if (likely(tokens >= tokens_per_packet)) {
		tokens -= tokens_per_packet;
		ret = 1;
	}

	/* If have no tokens anymore try to disable tx queue if appropriate. */
	if (unlikely(tokens < tokens_per_packet)) {
		unsigned pending = EMAC_TX_BUFF_INDX(arap->arap_data.tx_head -
			arap->arap_data.tx_tail);
		if (unlikely(pending >= burst_packets)) {
			emac_tx_stop_queue(dev, EMAC_TX_BUFF_INDX(arap->arap_data.tx_tail +
				pend_packets_wake_queue));
		}
	}

	return ret;
#endif
}

inline static void emac_recycle_tx_buf(struct sk_buff *skb)
{
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();
	trace_skb_perf_stamp_call(skb);
	trace_skb_perf_finish(skb);

	if (!qtn_skb_recycle_list_push(recycle_list,
				&recycle_list->stats_eth, skb)) {
		dev_kfree_skb(skb);
	}
}

static int __sram_text emac_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct emac_private *arap = netdev_priv(dev);
	volatile struct emac_desc *ptx_desc;
	u32 ctrlval;

	trace_skb_perf_stamp_call(skb);

	/* Free any buffers that the DMA has finished with since the last Tx */
	ptx_desc = arap->com.tx.descs + arap->arap_data.tx_tail;

	while ((arap->arap_data.tx_tail != arap->arap_data.tx_head) &&
			!(ptx_desc->status & TxDescOwn)) {
		if (ptx_desc->bufaddr2) {
			struct sk_buff *skb_old = (struct sk_buff *)ptx_desc->bufaddr2;

			emac_recycle_tx_buf(skb_old);
			ptx_desc->bufaddr2 = 0;
			ptx_desc->bufaddr1 = 0;
		}

		arap->arap_data.tx_tail =
			EMAC_TX_BUFF_INDX(arap->arap_data.tx_tail+1);
		ptx_desc = arap->com.tx.descs + arap->arap_data.tx_tail;
		arap->arap_data.tx_full = 0;
		arap->tx_queue_stopped = 0;
	}

	if (arap->tx_queue_stopped) {
		/* We had run out of descriptors */
		return NETDEV_TX_BUSY;
	}

	if (!emac_tx_pass(dev)) {
		return NETDEV_TX_BUSY;
	}

	ptx_desc = current_tx_desc(arap);

	if (likely(skb->len <= TxDescBuf1SizeMask)) {
		trace_ippkt_check(skb->data, skb->len, TRACE_IPPKT_LOC_ETH_TX);

		/*
		 * Flush and invalidate any cached data in the skb out to memory so that
		 * the DMA engine can see it.
		 * Save mapped memory for use by DMA engine.
		 */
		ptx_desc->bufaddr1 = cache_op_before_tx(align_buf_cache(skb->head),
				align_buf_cache_size(skb->head, skb_headroom(skb) + skb->len)) +
				align_buf_cache_offset(skb->head) + skb_headroom(skb);
		skb->cache_is_cleaned = 1;

		/*
		 * The buffer 2 length is set to 0, so it is ignored by the
		 * DMA engine and can be used as a handy place to store the
		 * skbuff address.  This is used to free the skbuf once the DMA
		 * has completed.
		 */
		KASSERT(ptx_desc->bufaddr2 == 0,
			("EMAC:Non-NULL TX descriptor (%p) - leak?\n", (void *)ptx_desc->bufaddr2));
		ptx_desc->bufaddr2 = (u32)skb;

		ctrlval = TxDescFirstSeg | TxDescLastSeg |
			(skb->len & TxDescBuf1SizeMask) << TxDescBuf1SizeShift;
		if (arap->arap_data.tx_head >= RUBY_EMAC_NUM_TX_BUFFERS - 1) {
			/* Last desc in ring must be marked as such */
			ctrlval |= TxDescEndOfRing;
		}
		ptx_desc->control = ctrlval;

		ptx_desc->status |= TxDescOwn;

		/* give descriptor */
		arap->arap_data.tx_head = EMAC_TX_BUFF_INDX(arap->arap_data.tx_head + 1);
		if (EMAC_TX_BUFF_INDX(arap->arap_data.tx_head + 1) == arap->arap_data.tx_tail) {
			/*
			 * We just used the last descriptor.
			 * Stop TX queue and mark DMA for pkt at 75% empty point, to
			 * restart queue so that link utilisation is maximized.
			 */
			unsigned wake_idx = EMAC_TX_BUFF_INDX(arap->arap_data.tx_head -
				(RUBY_EMAC_NUM_TX_BUFFERS / 4));
			emac_tx_stop_queue(dev, wake_idx);
		}
		emac_wr(&arap->com, EMAC_DMA_TX_POLL_DEMAND, 0);
		dev->trans_start = jiffies;
	} else {
		/*
		 * The allocated buffers should be big enough to take any
		 * frame.  Unexpectedly large frames are dropped.
		 */
		printk(KERN_ERR "%s: Dropping overlength (%d bytes) packet.\n",
				dev->name, skb->len);
		arap->tx_overlength_frame_discards++;
		emac_recycle_tx_buf(skb);
	}

	return 0;
}

static irqreturn_t __sram_text emac_interrupt(int irq, void *dev_id)
{
	u32 pending_ints, mac_ints, dma_ints;
	struct emac_private *arap;
	struct emac_common *arapc;
	struct net_device *dev = (struct net_device *)dev_id;

	const u32 handledDmaInts = DmaTxDone | DmaRxDone | DmaMacInterrupt;


	if (dev == NULL) {
		printk(KERN_ERR "%s: isr: null dev ptr\n", dev->name);
		return IRQ_RETVAL(1);
	}

	arap = netdev_priv(dev);
	arapc = &arap->com;

	pending_ints = emac_rd(arapc, EMAC_DMA_STATUS_IRQ) & handledDmaInts;
	emac_wr(arapc, EMAC_DMA_STATUS_IRQ, pending_ints);

	/* Handle RX interrupts first to minimize chance of overrun */
	if (pending_ints & DmaRxDone) {
		arap->mitg_intr_count++;
		dma_ints = emac_rd(arapc, EMAC_DMA_INT_ENABLE);
		dma_ints &= ~DmaRxDone;
		emac_wr(arapc, EMAC_DMA_INT_ENABLE, dma_ints);
		napi_schedule(&arap->napi);
	}

	/* Handle TX interrupts next to minimize chance of overrun */
	if ((pending_ints & DmaTxDone)) {
		arap->arap_data.tx_done_intr++;
		if (arap->tx_queue_stopped) {
			/* Just wake the queue up if we are stopped*/
			netif_wake_queue(dev);
		}
	}

	if (pending_ints & DmaMacInterrupt) {
		mac_ints = emac_rd(arapc, EMAC_MAC_INT);
		emac_wr(arapc, EMAC_MAC_INT, mac_ints);
		if (mac_ints & MacUnderrun) {
			arap->mac_underrun++;
			printk(KERN_ERR "%s: MAC underrun\n", dev->name);
		}
		if (mac_ints & MacJabber) {
			arap->mac_jabber++;
			printk(KERN_ERR "%s: Jabber detected\n", dev->name);
		}
	}
	return IRQ_RETVAL(1);
}

/*
 * The Tx ring has been full longer than the watchdog timeout
 * value. The transmitter must be hung?
 */
static void emac_tx_timeout(struct net_device *dev)
{
	printk(KERN_ERR "%s: emac_tx_timeout: dev=%p\n", dev->name, dev);
	dev->trans_start = jiffies;
}

static void reset_dma(struct emac_common *arapc)
{
	/* Normally we would read-modify-write to set & clear the reset bit, but
	 * due to a bug we cannot read any Ethernet registers whilst the block
	 * is reset.  We therefore just write the desired initial value back to
	 * the config register and clear the reset.
	 */
	emac_wr(arapc, EMAC_DMA_CONFIG, DmaSoftReset);
	emac_wr(arapc, EMAC_DMA_CONFIG, Dma4WordBurst | Dma64BitMode);
	emac_lib_init_dma(arapc);
}

static void stop_traffic(struct emac_common *arapc)
{
	int force_reset = 0;

	/* Stop transmit */
	emac_clrbits(arapc, EMAC_MAC_TX_CTRL, MacTxEnable);
	emac_clrbits(arapc, EMAC_DMA_CTRL, DmaStartTx);
	if (!emac_wait(arapc, EMAC_DMA_STATUS_IRQ, DmaTxStopped, DmaTxStopped, TIMEOUT_DMA_STATUS_IRQ, NULL)) {
		if ((emac_rd(arapc, EMAC_DMA_STATUS_IRQ) & DmaTxStateMask) != DmaTxStateStopped) {
			printk(KERN_ERR"Failed to stop Ethernet TX DMA\n");
			force_reset = 1;
		}
	}

	/* Stop receive */
	emac_clrbits(arapc, EMAC_MAC_RX_CTRL, MacRxEnable);
	emac_clrbits(arapc, EMAC_DMA_CTRL, DmaStartRx);
	if (!emac_wait(arapc, EMAC_DMA_STATUS_IRQ, DmaRxStopped, DmaRxStopped, TIMEOUT_DMA_STATUS_IRQ, NULL)) {
		if ((emac_rd(arapc, EMAC_DMA_STATUS_IRQ) & DmaRxStateMask) != DmaRxStateStopped) {
			printk(KERN_ERR"Failed to stop Ethernet RX DMA\n");
			force_reset = 1;
		}
	}

	if (force_reset) {
		reset_dma(arapc);
	}
}

static void start_traffic(struct emac_private *arap)
{
	struct emac_common *arapc = &arap->com;

	/* These IRQ flags must be cleared when we start as stop_traffic()
	 * relys on them to indicate when activity has stopped.
	 */
	emac_wr(arapc, EMAC_DMA_STATUS_IRQ, DmaTxStopped | DmaRxStopped);

	/* Start transmit */
	emac_setbits(arapc, EMAC_MAC_TX_CTRL, MacTxEnable);
	emac_setbits(arapc, EMAC_DMA_CTRL, DmaStartTx);

	/* Start receive */
	emac_setbits(arapc, EMAC_DMA_CTRL, DmaStartRx);
	emac_setbits(arapc, EMAC_MAC_RX_CTRL, MacRxEnable);
}

static void clear_buffers(struct emac_private *arap)
{
	/* Discard data from all Rx and Tx buffers */
	struct emac_common *arapc = &arap->com;
	int i;

	arap->arap_data.tx_done_intr = 0;
	arap->arap_data.tx_head = 0;
	arap->arap_data.tx_tail = 0;
	arap->arap_data.tx_full = 0;
	arap->tx_queue_stopped = 0;
	arap->rx_index = 0;
	if (arapc->rx.descs) {
		for (i = 0; i < RUBY_EMAC_NUM_RX_BUFFERS; i++) {
			(arapc->rx.descs + i)->status |= RxDescOwn;
		}
	}
	if (arapc->tx.descs) {
		for (i = 0; i < RUBY_EMAC_NUM_TX_BUFFERS; i++) {
			(arapc->tx.descs + i)->status &= ~TxDescOwn;
			if ((arapc->tx.descs + i)->bufaddr2) {
				struct sk_buff *skb_old = (struct sk_buff *)(arapc->tx.descs + i)->bufaddr2;
				dev_kfree_skb(skb_old);
				(arapc->tx.descs + i)->bufaddr2 = 0;
				(arapc->tx.descs + i)->bufaddr1 = 0;
			}
		}
	}
}

static int __init emac_init_module(void)
{
	int i, found_one = 0;

	emac_lib_enable(0);

	/* Probe devices */
	for(i = 0; i < sizeof(iflist) / sizeof(iflist[0]); i++) {
		struct net_device *dev = emac_probe(i);
		iflist[i].dev = dev;
		if (dev) {
			found_one++;
		}
	}
	if (!found_one) {
		return -ENODEV;
	}

	return 0;
}

static const struct net_device_ops emac_device_ops = {
	.ndo_open = emac_open,
	.ndo_stop = emac_close,
	.ndo_start_xmit = emac_tx,
	.ndo_get_stats = emac_lib_stats,
	.ndo_set_multicast_list = emac_lib_set_rx_mode,
	.ndo_do_ioctl = emac_lib_ioctl,
	.ndo_tx_timeout = emac_tx_timeout,
	.ndo_set_mac_address = eth_mac_addr,
};

static void emac_bufs_free(struct net_device *dev)
{
	struct emac_private *arap = netdev_priv(dev);
	int i;

	for (i = 0; i < RUBY_EMAC_NUM_RX_BUFFERS; i++) {
		if (arap->rxbufs[i]) {
			dev_kfree_skb(arap->rxbufs[i]);
			arap->rxbufs[i] = NULL;
		}
	}
}

int emac_bufs_alloc(struct net_device *dev)
{
	int i;

	/* Allocate rx buffers */
	for (i = 0; i < RUBY_EMAC_NUM_RX_BUFFERS; i++) {
		if (allocate_new_rx_buf(dev, i)) {
			return -1;
		}
	}

	return 0;
}

static void emac_set_eth_addr(struct net_device *dev, int port_num)
{
	memcpy(dev->dev_addr, get_ethernet_addr(), ETH_ALEN);

	if (port_num > 0) {
		u32 val;

		val = (u32)dev->dev_addr[5] +
			((u32)dev->dev_addr[4] << 8) +
			((u32)dev->dev_addr[3] << 16);
		val += port_num;
		dev->dev_addr[5] = (unsigned char)val;
		dev->dev_addr[4] = (unsigned char)(val >> 8);
		dev->dev_addr[3] = (unsigned char)(val >> 16);
	}
}

static struct net_device* emac_probe(int port_num)
{
	struct emac_private *arap = NULL;
	struct emac_common *arapc = NULL;
	struct net_device *dev = NULL;
	int irq, err;
	unsigned long base;
	char devname[IFNAMSIZ + 1];
	int emac_cfg;
	int emac_phy;

	printk(KERN_INFO "%s version %s %s\n", DRV_NAME, DRV_VERSION, DRV_AUTHOR);

	if (emac_lib_board_cfg(port_num, &emac_cfg, &emac_phy)) {
		return NULL;
	}

	if ((emac_cfg & EMAC_IN_USE) == 0) {
		return NULL;
	}

	/* Get requested port data */
	base  = PHYS_IO_ADDRESS(iflist[port_num].base_addr);
	irq = iflist[port_num].irq;

	/* Allocate device structure */
	sprintf(devname, "eth%d_emac%d", soc_id(), port_num);
	dev = alloc_netdev(sizeof(struct emac_private), devname, ether_setup);
	if (!dev) {
		printk(KERN_ERR "%s: alloc_etherdev failed\n", DRV_NAME);
		return NULL;
	}

	/* Initialize device structure fields */
	emac_set_eth_addr(dev, port_num);
	dev->base_addr = base;
	dev->irq = irq;
	dev->watchdog_timeo = ETH_TX_TIMEOUT;
	dev->netdev_ops = &emac_device_ops;
	dev->tx_queue_len = QTN_BUFS_EMAC_TX_QDISC;
	SET_ETHTOOL_OPS(dev, &emac_lib_ethtool_ops);

	/* Initialize private data */
	arap = netdev_priv(dev);
	arapc = &arap->com;
	memset(arap, 0, sizeof(struct emac_private));
	arapc->dev = dev;
	arapc->mac_id = port_num;
	arapc->vbase = iflist[port_num].base_addr;
	arapc->mdio_vbase = iflist[port_num].mdio_base_addr;
	arapc->emac_cfg = emac_cfg;
	arapc->phy_addr = emac_phy;

	/* Initialize MII */
	if (emac_lib_mii_init(dev)) {
		goto mii_init_error;
	}

	/* Allocate descs & buffers */
	if (emac_lib_descs_alloc(dev,
				RUBY_EMAC_NUM_RX_BUFFERS, 0,
				RUBY_EMAC_NUM_TX_BUFFERS, 0)) {
		goto descs_alloc_error;
	}
	if (emac_bufs_alloc(dev)) {
		goto bufs_alloc_error;
	}

	/* Initialize NAPI */
	netif_napi_add(dev, &arap->napi, emac_rx_poll, board_napi_budget());

	/* The interface may have been used by the bootloader, so shut it down
	 * here in preparation for bringing it up later.
	 */
	shut_down_interface(dev);

	/* Register device */
	if ((err = register_netdev(dev)) != 0) {
		printk(KERN_ERR "%s: Cannot register net device, error %d\n", DRV_NAME, err);
		goto netdev_register_error;
	}
	printk(KERN_INFO"%s: Arasan Ethernet found at 0x%lx, irq %d\n", dev->name, base, irq);

	create_proc_read_entry(iflist[arapc->mac_id].proc_name, 0, NULL, arasan_proc_rd, dev);
	emac_lib_phy_power_create_proc(dev);
	emac_lib_phy_reg_create_proc(dev);
	emac_lib_mdio_sysfs_create(dev);

	return dev;

netdev_register_error:
	emac_bufs_free(dev);
bufs_alloc_error:
	emac_lib_descs_free(dev);
descs_alloc_error:
	emac_lib_mii_exit(dev);
mii_init_error:
	free_netdev(dev);

	return NULL;
}

static void release_all(struct net_device *dev)
{
	struct emac_private *arap;
	struct emac_common *arapc;

	if (!dev) {
		return;
	}

	arap = netdev_priv(dev);
	arapc = &arap->com;

	emac_lib_mdio_sysfs_remove(dev);
	emac_lib_phy_reg_remove_proc(dev);
	emac_lib_phy_power_remove_proc(dev);
	remove_proc_entry(iflist[arapc->mac_id].proc_name, NULL);

	unregister_netdev(dev);

	shut_down_interface(dev);
	emac_bufs_free(dev);
	emac_lib_descs_free(dev);
	emac_lib_mii_exit(dev);

	free_netdev(dev);
}

static void bring_up_interface(struct net_device *dev)
{
	/* Interface will be ready to send/receive data, but will need hooking
	 * up to the interrupts before anything will happen.
	 */
	struct emac_private *arap = netdev_priv(dev);

	shut_down_interface(dev);
	start_traffic(arap);
	enable_emac_ints(arap);
	emac_lib_set_rx_mode(dev);
}

static void shut_down_interface(struct net_device *dev)
{
	/* Close down MAC and DMA activity and clear all data. */
	struct emac_private *arap = netdev_priv(dev);
	struct emac_common *arapc = &arap->com;

	disable_emac_ints(arap);
	stop_traffic(arapc);
	emac_lib_init_dma(arapc);
	emac_lib_init_mac(dev);
	clear_buffers(arap);
}

static void on_off_rxmitigation(unsigned long arg)
{
	struct emac_private *arap = (struct emac_private *)arg;
	static int emac_mitig_state = EMAC_MITIGATION_DISABLED;

	if ((emac_mitig_state == EMAC_MITIGATION_DISABLED) &&
		(arap->mitg_intr_count > EMAC_MITIGATION_EN_THRESHOLD)) {
		enable_emac_irq_mitigation(arap, EMAC_IRQ_MITIGATION_FRAME_COUNTER,
					   EMAC_IRQ_MITIGATION_TIMEOUT);
		emac_mitig_state = EMAC_MITIGATION_ENABLED;
	} else if ((emac_mitig_state == EMAC_MITIGATION_ENABLED) &&
		(arap->mitg_intr_count < EMAC_MITIGATION_DIS_THRESHOLD)) {
		disable_emac_irq_mitigation(arap);
		emac_mitig_state = EMAC_MITIGATION_DISABLED;
	}

	arap->mitg_intr_count = 0;
	mod_timer(&arap->mitg_timer, jiffies + EMAC_MITIGATION_TIMER_FREQ);
}

static int emac_open(struct net_device *dev)
{
	int retval = 0;
	struct emac_private *arap = netdev_priv(dev);
	struct emac_common *arapc = &arap->com;

	bring_up_interface(dev);

	napi_enable(&arap->napi);

	if ((retval = request_irq(dev->irq, &emac_interrupt, 0, dev->name, dev))) {
		printk(KERN_ERR "%s: unable to get IRQ %d\n", dev->name, dev->irq);
		goto err_out;
	}

	/* Enable RX mitigation only for gigabit or auto negotiated interfaces */
	if (board_slow_ethernet() == 0) {
		/* Fast and Gig eth: Start RX interrupt mitigation timer */
		init_timer(&arap->mitg_timer);
		arap->mitg_timer.function = on_off_rxmitigation;
		arap->mitg_timer.data = (unsigned long)arapc;
		mod_timer(&arap->mitg_timer, jiffies + EMAC_MITIGATION_TIMER_FREQ);
	} else {
		/* slow eth */
		disable_emac_irq_mitigation(arap);
	}

	emac_lib_phy_start(dev);
	netif_start_queue(dev);
	emac_lib_pm_emac_add_notifier(dev);

	return 0;

err_out:
	napi_disable(&arap->napi);
	return retval;
}

static int emac_close(struct net_device *dev)
{
	struct emac_private *arap = netdev_priv(dev);

	emac_lib_pm_emac_remove_notifier(dev);

	napi_disable(&arap->napi);

	emac_lib_phy_stop(dev);
	shut_down_interface(dev);
	netif_stop_queue(dev);
	del_timer(&arap->mitg_timer);
	free_irq(dev->irq, dev);
	return 0;
}

static void __exit emac_cleanup_module(void)
{
	int i;

	for (i = 0; i < sizeof(iflist) / sizeof(iflist[0]); i++) {
		release_all(iflist[i].dev);
	}
}

static int arasan_proc_rd(char *buf, char **start, off_t offset, int count,
		int *eof, void *data)
{
	char *p = buf;
	struct net_device *dev = data;
	struct emac_private *arap = NULL;

	if (dev) {
		arap = netdev_priv(dev);
	}

	if (!dev || !arap) {
		printk("%s: NULL dev or dev->priv pointer\n", __FUNCTION__);
		*eof = 1;
		return 0;
	}

	p += emac_lib_stats_sprintf(p, dev);
	p += sprintf(p, "%2s#%02d %6s: %d, %d, %d, %d, %d\n", "Queue", 0, "State",
			arap->arap_data.tx_full, arap->arap_data.tx_stopped_count,
			arap->arap_data.tx_done_intr, arap->arap_data.tx_head, arap->arap_data.tx_tail);
	*eof = 1;

	return p - buf;
}

module_init(emac_init_module);
module_exit(emac_cleanup_module);


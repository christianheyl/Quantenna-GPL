/*
 *  drivers/net/arasan_emac_ahb.h
 *
 *  Copyright (c) Quantenna Communications Incorporated 2007.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __DRIVERS_NET_ARASAN_EMAC_AHB_H
#define __DRIVERS_NET_ARASAN_EMAC_AHB_H	1

#define ETH_TX_TIMEOUT (100*HZ)

#include <linux/slab.h>
#include <linux/skbuff.h>
#include "emac_lib.h"

#ifndef __ASSEMBLY__

#if (RUBY_EMAC_NUM_TX_BUFFERS & (RUBY_EMAC_NUM_TX_BUFFERS - 1))
#error "TX BUFFER needs to be power of two for MODULO operations"
#endif
#if (RUBY_EMAC_NUM_RX_BUFFERS & (RUBY_EMAC_NUM_RX_BUFFERS - 1))
#error "RX BUFFER needs to be power of two for MODULO operations"
#endif

#define EMAC_TX_BUFF_INDX(X) ((X) & (RUBY_EMAC_NUM_TX_BUFFERS - 1))
#define EMAC_RX_BUFF_INDX(X) ((X) & (RUBY_EMAC_NUM_RX_BUFFERS - 1))

struct emac_private {
	struct emac_common com;		/* Must be first */

	struct sk_buff *rxbufs[RUBY_EMAC_NUM_RX_BUFFERS];
	dma_addr_t rxdmabufs[RUBY_EMAC_NUM_RX_BUFFERS];
	int rx_index;

	/* Tx_head points to the next descriptor to use in the ring.
	 * Tx_tail points to the start of the descriptors that have been used
	 * for transmission and who need their associated buffers freeing once
	 * the DMA engine has finished with them.  Tx_full is true if the
	 * descriptor ring has filled up whilst waiting for the DMA engine
	 * to send the packets.
	 */

	spinlock_t flowlock;
	int tx_queue_stopped;
	u32 rx_skb_alloc_failures;
	u32 rx_fragmented_frame_discards;
	u32 tx_overlength_frame_discards;
	u32 mac_underrun;
	u32 mac_jabber;

	struct napi_struct napi;

	struct {
		u32 tx_done_intr;
		u32 tx_full;
		u32 tx_stopped_count;
		u32 tx_head;
		u32 tx_tail;
	} arap_data;

	struct timer_list mitg_timer;
	unsigned long mitg_intr_count;
};

#endif /* __ASSEMBLY__ */

#endif

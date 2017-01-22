/*
 * (C) Copyright 2012 Quantenna Communications Inc.
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

#ifndef __TOPAZ_HBM_H
#define __TOPAZ_HBM_H

#include <linux/skbuff.h>
#include <qtn/qtn_buffers.h>
#include <qtn/topaz_hbm_cpuif.h>

#include <qtn/dmautil.h>
#include <asm/cacheflush.h>

#define topaz_hbm_attach_skb(buf_virt, pool, headroom)		\
	_topaz_hbm_attach_skb((buf_virt), (pool), 1, (headroom)	\
			QTN_SKB_ALLOC_TRACE_ARGSRC)
#define topaz_hbm_attach_skb_no_invalidate(buf_virt, pool, headroom)		\
	_topaz_hbm_attach_skb((buf_virt), (pool), 0, (headroom) 	\
			QTN_SKB_ALLOC_TRACE_ARGSRC)
struct sk_buff *_topaz_hbm_attach_skb(void *buf_virt, int8_t pool, int inv, uint8_t headroom
		QTN_SKB_ALLOC_TRACE_ARGS);

#define topaz_hbm_attach_skb_bus(buf_bus, pool)	\
	_topaz_hbm_attach_skb_bus((buf_bus), (pool)	\
			QTN_SKB_ALLOC_TRACE_ARGSRC)
static inline struct sk_buff *
_topaz_hbm_attach_skb_bus(void *buf_bus, int8_t pool
		QTN_SKB_ALLOC_TRACE_ARGS)
{
	void *buf_virt;

	if (unlikely(buf_bus == NULL)) {
		return NULL;
	}

	buf_virt = bus_to_virt((uintptr_t) buf_bus);
	if (unlikely(buf_virt == RUBY_BAD_VIRT_ADDR)) {
		return NULL;
	}

	return _topaz_hbm_attach_skb(buf_virt, pool, 1, 0
			QTN_SKB_ALLOC_TRACE_ARGVARS);
}

static inline void topaz_hbm_flush_skb_cache(struct sk_buff *skb)
{
	uintptr_t flush_start = (uintptr_t) align_buf_cache(skb->head);
	uintptr_t flush_end = align_val_up((uintptr_t) skb_end_pointer(skb),
			dma_get_cache_alignment());
	if (!skb->cache_is_cleaned)
		flush_and_inv_dcache_range(flush_start, flush_end);
}

void topaz_hbm_filter_txdone_pool(void);
void topaz_hbm_filter_txdone_buf(void *const buf_bus);
unsigned int topaz_hbm_pool_available(int8_t pool);
#ifdef TOPAZ_EMAC_NULL_BUF_WR
extern void (*topaz_emac_null_buf_del_cb)(void);
#endif

void topaz_hbm_release_buf_safe(void *const pkt_bus);

struct sk_buff *topaz_hbm_attach_skb_quarantine(void *buf_virt, int pool, int len, uint8_t **whole_frm_hdr);

#endif	/* __TOPAZ_HBM_H */


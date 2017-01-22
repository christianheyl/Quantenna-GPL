/*
 *  Copyright (c) Quantenna Communications, Inc. 2012
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

#ifndef __QTN_DMA_UTIL_H
#define __QTN_DMA_UTIL_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <asm/dma-mapping.h>
#include <asm/cacheflush.h>

#define ALIGNED_DMA_DESC(typeq, type)		\
	struct aligned_dma_##type {		\
		uint32_t desc_count;		\
		typeq type *descs;		\
		unsigned long aligned_vdescs;	\
		unsigned long unaligned_vdescs;	\
		unsigned long descs_dma_addr;	\
	}

typedef ALIGNED_DMA_DESC(, void) aligned_dma_descs;

#define ALIGNED_DMA_DESC_ALLOC(_ptr, _count, _align, _is_sram)			\
	dmautil_aligned_dma_desc_alloc((aligned_dma_descs *)(_ptr),		\
			sizeof((_ptr)->descs[0]), (_count), (_align), (_is_sram))
int dmautil_aligned_dma_desc_alloc(aligned_dma_descs *d,
		unsigned int desc_size, unsigned int desc_count,
		unsigned int align, bool is_sram);

#define ALIGNED_DMA_DESC_FREE(_ptr)	dmautil_aligned_dma_desc_free((aligned_dma_descs *)(_ptr))
void dmautil_aligned_dma_desc_free(aligned_dma_descs *d);

/*
 * Alignment helpers
 */
__always_inline static unsigned long align_val_up(unsigned long val, unsigned long step)
{
	return ((val + step - 1) & (~(step - 1)));
}
__always_inline static unsigned long align_val_down(unsigned long val, unsigned long step)
{
	return (val & (~(step - 1)));
}
__always_inline static void* align_buf_dma(void *addr)
{
	return (void*)align_val_up((unsigned long)addr, dma_get_cache_alignment());
}
__always_inline static unsigned long align_buf_dma_offset(void *addr)
{
	return ((char *)align_buf_dma(addr) - (char *)addr);
}
__always_inline static void* align_buf_cache(void *addr)
{
	return (void*)align_val_down((unsigned long)addr, dma_get_cache_alignment());
}
__always_inline static unsigned long align_buf_cache_offset(void *addr)
{
	return ((char *)addr - (char *)align_buf_cache(addr));
}
__always_inline static unsigned long align_buf_cache_size(void *addr, unsigned long size)
{
	return align_val_up(size + align_buf_cache_offset(addr), dma_get_cache_alignment());
}

__always_inline static void flush_dcache_sizerange_safe(void *p, size_t size)
{
	uintptr_t op_start = (uintptr_t) align_buf_cache(p);
	size_t op_size = align_buf_cache_size(p, size);
	flush_dcache_range(op_start, op_start + op_size);
}

__always_inline static void flush_and_inv_dcache_sizerange_safe(void *p, size_t size)
{
	uintptr_t op_start = (uintptr_t) align_buf_cache(p);
	size_t op_size = align_buf_cache_size(p, size);
	flush_and_inv_dcache_range(op_start, op_start + op_size);
}

__always_inline static void inv_dcache_sizerange_safe(void *p, size_t size)
{
	uintptr_t op_start = (uintptr_t) align_buf_cache(p);
	size_t op_size = align_buf_cache_size(p, size);
	inv_dcache_range(op_start, op_start + op_size);
}

#endif	// __ASSEMBLY__
#endif	// __QTN_DMA_UTIL_H


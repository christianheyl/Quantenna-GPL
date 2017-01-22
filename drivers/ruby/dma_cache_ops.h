/*
 * (C) Copyright 2011 Quantenna Communications Inc.
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


#ifndef __BOARD_RUBY_DMA_CACHE_OPS_H
#define __BOARD_RUBY_DMA_CACHE_OPS_H

static __always_inline dma_addr_t cache_op_before_rx(void *ptr, size_t size,
							uint8_t cache_is_cleaned)
{
	dma_addr_t ret;

	if (cache_is_cleaned) {
		/* Cache is already invalidated, so it is enough to just convert address. */
		ret = plat_kernel_addr_to_dma(NULL, ptr);
	} else {
		ret = dma_map_single(NULL, ptr, size, DMA_FROM_DEVICE);
	}

	return ret;
}

static __always_inline dma_addr_t cache_op_before_tx(void *ptr, size_t size)
{
	return dma_map_single(NULL, ptr, size, DMA_BIDIRECTIONAL);
}

#endif // #ifndef __BOARD_RUBY_DMA_CACHE_OPS_H


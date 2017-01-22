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

#ifndef __QTN_SKB_SIZE_H
#define __QTN_SKB_SIZE_H

#include "qtn_buffers.h"

#define RX_BUF_SIZE_PAYLOAD	(4400)

/*
 * sk_buff size for Ruby qdrv and arasan driver.
 * These should be the same since they share the same recycle list.
 *
 * In Topaz, HBM is used for buffer allocation
 */
#define RX_BUF_SIZE		(RX_BUF_SIZE_PAYLOAD + 2 * L1_CACHE_BYTES - 1)

/*
 * Optimization for buffer allocation.
 * Used to modify kernel to have kmalloc() cache entry of this size.
 */
#define RX_BUF_SIZE_KMALLOC	(roundup((RX_BUF_SIZE) + 256, 256))

/*
 * EMAC buffer limits for software rx/tx, not hardware rxp/txp
 */
#define RUBY_EMAC_NUM_TX_BUFFERS	(1 << 8)
#define RUBY_EMAC_NUM_RX_BUFFERS	(1 << 10)

#endif // #ifndef __QTN_SKB_SIZE_H


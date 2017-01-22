/*
 * Copyright (c) Quantenna Communications, Inc. 2012
 * All rights reserved.
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

#include <qtn/dmautil.h>
#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/cacheflush.h>

#include "mem_check.h"

int dmautil_aligned_dma_desc_alloc(aligned_dma_descs *d,
		unsigned int desc_size, unsigned int desc_count,
		unsigned int align, bool is_sram)
{
	size_t remap_size = desc_size * desc_count;
	size_t alloc_size = remap_size + align - 1;
	void *p;

	memset(d, 0, sizeof(*d));
	d->desc_count = desc_count;

	if (is_sram) {
		p = heap_sram_alloc(alloc_size);
	} else {
		p = kmalloc(alloc_size, GFP_KERNEL);
	}

	if (!p) {
		return -ENOMEM;
	}

	d->unaligned_vdescs = (unsigned long)p;
	flush_dcache_range(d->unaligned_vdescs, d->unaligned_vdescs + alloc_size);
	d->aligned_vdescs = align_val_up(d->unaligned_vdescs, align);
	d->descs_dma_addr = virt_to_bus((void *)d->aligned_vdescs);
	d->descs = ioremap_nocache(d->aligned_vdescs, remap_size);
	if (!d->descs) {
		/* alloc pass but remap failure, free descs */
		dmautil_aligned_dma_desc_free(d);
		return -ENOMEM;
	}

	memset(d->descs, 0, remap_size);

	return 0;
}
EXPORT_SYMBOL(dmautil_aligned_dma_desc_alloc);

void dmautil_aligned_dma_desc_free(aligned_dma_descs *d)
{
	void *p;

	if (!d) {
		return;
	}

	if (d->descs) {
		iounmap(d->descs);
	}

	p = (void *)d->unaligned_vdescs;
	if (is_linux_sram_mem_addr(d->unaligned_vdescs)) {
		heap_sram_free(p);
	} else {
		kfree(p);
	}

	memset(d, 0, sizeof(*d));
}
EXPORT_SYMBOL(dmautil_aligned_dma_desc_free);



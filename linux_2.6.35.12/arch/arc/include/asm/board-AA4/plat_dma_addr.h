/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Feb 2009
 *  -Reworked the API to work with ARC PCI Bridge
 *
 * vineetg: Feb 2009
 *  -For AA4 board, kernel to DMA address APIs
 */

/* Some notes on DMA <=> kernel address generation
 *
 * A simplistic implementation will generate 0 based bus address.
 * For e.g. 0x8AAA_0000 becomes 0x0AAA_0000 bus addr
 * However this doesnt work with PCI devices behind the PCI Host Bridge on AA4
 * which can't allow 0 based addresses. So the API for special case of PCI
 * makes corrections
 *
 * As a small optimisation, if PCI is not enabled we can simply return
 * 0 based bus addr hence the CONFIG_xx check for PCI Host Bridge
 */

#ifndef __PLAT_DMA_ADDR_H
#define __PLAT_DMA_ADDR_H

#include <linux/device.h>

static inline unsigned long plat_dma_addr_to_kernel(
    struct device *dev, dma_addr_t dma_addr)
{
#ifdef CONFIG_ARC_AHB_PCI_BRIDGE
    if (dev && dev->bus && (strncmp(dev->bus->name, "pci", 3) == 0) )
        return (unsigned long) dma_addr;
#endif

    return dma_addr + PAGE_OFFSET;
}

static inline dma_addr_t plat_kernel_addr_to_dma(struct device *dev, void *ptr)
{
    unsigned long addr = (unsigned long)ptr;
    /*
     * To Catch buggy drivers which can call DMA map API with kernel vaddr
     * i.e. for buffers alloc via vmalloc or ioremap which are not gaurnateed
     * to be PHY contiguous and hence unfit for DMA anyways.
     * On ARC kernel virtual address is 0x7000_0000 to 0x7FFF_FFFF, so
     * ideally we want to check this range here, but our implementation is
     * better as it checks for even worse user virtual address as well.
     */
    BUG_ON(addr < PAGE_OFFSET);

#ifdef CONFIG_ARC_AHB_PCI_BRIDGE
    if (dev && dev->bus && (strncmp(dev->bus->name, "pci", 3) == 0) )
        return addr;
#endif

    return addr - PAGE_OFFSET;
}

#endif

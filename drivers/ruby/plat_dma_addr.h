/*************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * Vineetg: Feb 2009
 *  -Reworked the API to work with ARC PCI Bridge
 *
 * vineetg: Feb 2009
 *  -For AA4 board, kernel to DMA address APIs
 ************************************************************************/

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

#ifndef __BOARD_RUBY_PLAT_DMA_ADDR_H
#define __BOARD_RUBY_PLAT_DMA_ADDR_H

#include <linux/device.h>

static inline unsigned long plat_dma_addr_to_kernel(struct device *dev, dma_addr_t dma_addr)
{
      return (unsigned long)bus_to_virt(dma_addr);
}

static inline dma_addr_t plat_kernel_addr_to_dma(struct device *dev, void *ptr)
{
    /*
     * To Catch buggy drivers which can call DMA map API with kernel vaddr
     * i.e. for buffers alloc via vmalloc or ioremap which are not gaurnateed
     * to be PHY contiguous and hence unfit for DMA anyways.
     * On ARC kernel virtual address is 0x7000_0000 to 0x7FFF_FFFF, so 
     * ideally we want to check this range here, but our implementation is  
     * better as it checks for even worse user virtual address as well.
     */
    BUG_ON(ptr < (void*)PAGE_OFFSET);

    return virt_to_bus(ptr);
}

#endif // #ifndef __BOARD_RUBY_PLAT_DMA_ADDR_H


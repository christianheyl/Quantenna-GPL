/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>

#define ARC_IOMAP_NOCACHE 0xff

void __iomem * __ioremap(unsigned long phys_addr, unsigned long size, unsigned long flags)
{
    void __iomem * addr;
    struct vm_struct * area;
    unsigned long offset, last_addr;
    pgprot_t prot;

    /* Don't allow wraparound or zero size */
    last_addr = phys_addr + size - 1;
    if (!size || last_addr < phys_addr)
        return NULL;

    /*
     * Don't allow anybody to remap normal RAM that we're using..
     */

    /*  We are using this API to mark a portion of memory as coherent -
        by mapping the memory to virtual address space to enable
        page translation at mmu and marking the page as uncacheable

    if (phys_addr <= virt_to_phys(high_memory - 1)) {
        char *t_addr, *t_end;
        struct page *page;

        t_addr = __va(phys_addr);
        t_end = t_addr + (size - 1);

        for(page = virt_to_page(t_addr); page <= virt_to_page(t_end); page++)
            if(!PageReserved(page))
                return NULL;
    }
    */

    if( flags == ARC_IOMAP_NOCACHE )
        prot = PAGE_KERNEL_NO_CACHE;
    else
        prot = PAGE_KERNEL;

    /*
     * Mappings have to be page-aligned
     */
    offset = phys_addr & ~PAGE_MASK;
    phys_addr &= PAGE_MASK;
    size = PAGE_ALIGN(last_addr+1) - phys_addr;

    /*
     * Ok, go for it..
     */
    area = get_vm_area(size, VM_IOREMAP);
    if (!area)
        return NULL;
    area->phys_addr = phys_addr;
    addr = (void __iomem *) area->addr;
    if (ioremap_page_range((unsigned long) addr,
            (unsigned long) addr + size, phys_addr, prot)) {
        vunmap((void __force *) addr);
        return NULL;
    }
    return (void __iomem *) (offset + (char __iomem *)addr);
}
EXPORT_SYMBOL(__ioremap);

/* #define IS_KSEG1(addr) (((unsigned long)(addr) & ~0x1fffffffUL) == CKSEG1) */

void iounmap(const volatile void __iomem *addr)
{
    struct vm_struct *p;

/*  if (IS_KSEG1(addr)) */
/*      return; */

    p = remove_vm_area((void *) (PAGE_MASK & (unsigned long __force) addr));
    if (!p)
        printk(KERN_ERR "iounmap: bad address %p\n", addr);

        kfree(p);
}

EXPORT_SYMBOL(iounmap);

void *ioremap_nocache(unsigned long physaddr, unsigned long size)
{
    return __ioremap(physaddr, size, ARC_IOMAP_NOCACHE);
}
EXPORT_SYMBOL(ioremap_nocache);

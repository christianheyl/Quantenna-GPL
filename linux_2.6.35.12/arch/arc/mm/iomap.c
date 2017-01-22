/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ioport.h>

#include <asm/io.h>

void __iomem *ioport_map(unsigned long port, unsigned int nr)
{
    return (void *)port;
}
EXPORT_SYMBOL(ioport_map);

void ioport_unmap(void __iomem *addr)
{
    return;
}
EXPORT_SYMBOL(ioport_unmap);

void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{

    resource_size_t start = pci_resource_start(dev, bar);
    resource_size_t len = pci_resource_len(dev, bar);
    unsigned long flags = pci_resource_flags(dev, bar);

    if (!len || !start)
        return NULL;

    if (maxlen && len > maxlen)
        len = maxlen;

    if (flags & IORESOURCE_IO)
        return ioport_map(start, len);

    if (flags & IORESOURCE_MEM) {
        if (flags & IORESOURCE_CACHEABLE)
            return ioremap(start, len);
        return ioremap_nocache(start, len);
    }

    return NULL;
}

void pci_iounmap(struct pci_dev *dev, void __iomem * addr)
{
    iounmap(addr);
}

EXPORT_SYMBOL(pci_iounmap);

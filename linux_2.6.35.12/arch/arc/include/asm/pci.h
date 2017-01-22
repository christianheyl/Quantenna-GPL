/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASMARC_PCI_H
#define __ASMARC_PCI_H

#ifdef __KERNEL__

/* Can be used to override the logic in pci_scan_bus for skipping
 * already-configured bus numbers - to be used for buggy BIOSes
 * or architectures with incomplete PCI setup by the loader.
 */
#define pcibios_assign_all_busses()	0
#define pcibios_scan_all_fns(a, b)	0

#define PCIBIOS_MIN_IO		0UL
#define PCIBIOS_MIN_MEM		0UL

#define PCI_DMA_BUS_IS_PHYS			(0)

#include <linux/scatterlist.h>
#include <asm/io.h>
/* generic pci stuff */
#include <asm-generic/pci.h>
#include <asm-generic/pci-dma-compat.h>

static inline void pcibios_set_master(struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}

static inline void pcibios_penalize_isa_irq(int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}

#define pci_dac_dma_supported(pci_dev, mask)	(0)
#define pci_controller_num(PDEV)	(0)

#endif
#endif

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_IO_H
#define _ASM_ARC_IO_H

#include <asm/page.h>

#define __raw_readb(addr)       (*(volatile unsigned char *)(addr))
#define __raw_readw(addr)       (*(volatile unsigned short *)(addr))
#define __raw_readl(addr)       (*(volatile unsigned int *)(addr))
/* Sameer: Time interpolator gonna use this. */
#define __raw_readq(addr)       (*(volatile unsigned long *)(addr))

#define __raw_writeb(b,addr) ((*(volatile unsigned char *)(addr)) = (b))
#define __raw_writew(w,addr) ((*(volatile unsigned short *)(addr)) = (w))
#define __raw_writel(l,addr) ((*(volatile unsigned int *)(addr)) = (l))

#define memset_io(a,b,c)        memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)    memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)      memcpy((void *)(a),(b),(c))

/* Sameer: Time interpolator gonna use this. */
#define readl_relaxed(addr) __raw_readl(addr)
#define readq_relaxed(addr) __raw_readq(addr)

#define __io(a)                 (a)
#define __mem_pci(a)            ((unsigned long)(a))
#define __mem_isa(a)            ((unsigned long)(a))

/*
 * Generic virtual read/write
 */
#define __arch_getw(a)          (*(volatile unsigned short *)(a))
#define __arch_putw(v,a)        (*(volatile unsigned short *)(a) = (v))

#define iomem_valid_addr(iomem,sz)      (1)
#define iomem_to_phys(iomem)            (iomem)

#ifdef __io
#define outb(v,p)                       __raw_writeb(v,__io(p))
#define outw(v,p)                       __raw_writew(cpu_to_le16(v),__io(p))
#define outl(v,p)                       __raw_writel(cpu_to_le32(v),__io(p))

#define inb(p)  ({ unsigned int __v = __raw_readb(__io(p)); __v; })
#define inw(p)  ({ unsigned int __v = le16_to_cpu(__raw_readw(__io(p))); __v; })
#define inl(p)  ({ unsigned int __v = le32_to_cpu(__raw_readl(__io(p))); __v; })

#define outsb(p,d,l)                    __raw_writesb(__io(p),d,l)
#define outsw(p,d,l)                    __raw_writesw(__io(p),d,l)
#define outsl(p,d,l)                    __raw_writesl(__io(p),d,l)

#define insb(p,d,l)                     __raw_readsb(__io(p),d,l)
#define insw(p,d,l)                     __raw_readsw(__io(p),d,l)
#define insl(p,d,l)                     __raw_readsl(__io(p),d,l)
#endif

#define outb_p(val,port)                outb((val),(port))
#define outw_p(val,port)                outw((val),(port))
#define outl_p(val,port)                outl((val),(port))
#define inb_p(port)                     inb((port))
#define inw_p(port)                     inw((port))
#define inl_p(port)                     inl((port))

#define outsb_p(port,from,len)          outsb(port,from,len)
#define outsw_p(port,from,len)          outsw(port,from,len)
#define outsl_p(port,from,len)          outsl(port,from,len)
#define insb_p(port,to,len)             insb(port,to,len)
#define insw_p(port,to,len)             insw(port,to,len)
#define insl_p(port,to,len)             insl(port,to,len)

#define readb(addr) (*(volatile unsigned char *) (addr))
#define readw(addr) (*(volatile unsigned short *) (addr))
#define readl(addr) (*(volatile unsigned int *) (addr))
#define writeb(b,addr) (*(volatile unsigned char *) (addr) = (b))
#define writew(b,addr) (*(volatile unsigned short *) (addr) = (b))
#define writel(b,addr) (*(volatile unsigned int *) (addr) = (b))

#define writel_or(b, addr) writel(readl(addr) | (b), (addr))
#define writel_and(b, addr) writel(readl(addr) & (b), (addr))

extern void *__ioremap(unsigned long physaddr, unsigned long size, unsigned long flags);

/**
 * ioremap     -   map bus memory into CPU space
 * @offset:    bus address of the memory
 * @size:      size of the resource to map
 *
 * ioremap performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address.
 */

static inline void __iomem * ioremap(unsigned long offset, unsigned long size)
{
	return __ioremap(offset, size, 0);
}

/* extern void *ioremap(unsigned long physaddr, unsigned long size); */
extern void *ioremap_nocache(unsigned long physaddr, unsigned long size);
extern void iounmap(const volatile void __iomem *addr);
/* extern void iounmap(void *addr); */

static inline void  __raw_writesb(unsigned long port, void *addr, unsigned int count)
{
        while (count--) {
                outb(*(u8 *)addr, port);
                addr++;
        }
}

static inline void __raw_readsb(unsigned long port, void *addr, unsigned int count)
{
        while (count--) {
                *(u8 *)addr = inb(port);
                addr++;
        }
}

static inline void  __raw_writesw(unsigned long port, void *addr, unsigned int count)
{
        while (count--) {
                outw(*(u16 *)addr, port);
                addr += 2;
        }
}

static inline void __raw_readsw(unsigned long port, void *addr, unsigned int count)
{
        while (count--) {
                *(u16 *)addr = inw(port);
                addr += 2;
        }
}

static inline void __raw_writesl(unsigned long port, void *addr, unsigned int count)
{
        while (count--) {
                outl(*(u32 *)addr, port);
                addr += 4;
        }
}

static inline void __raw_readsl(unsigned long port, void *addr, unsigned int count)
{
        while (count--) {
                *(u32 *)addr = inl(port);
                addr += 4;
        }
}
/**
 *	virt_to_phys	-	map virtual addresses to physical
 *	@address: address to remap
 *
 *	The returned physical address is the physical (CPU) mapping for
 *	the memory address given. It is only valid to use this function on
 *	addresses directly mapped or allocated via kmalloc.
 *
 *	This function does not give bus mappings for DMA transfers. In
 *	almost all conceivable cases a device driver should not be using
 *	this function
 */

static inline unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

/**
 *	phys_to_virt	-	map physical address to virtual
 *	@address: address to remap
 *
 *	The returned virtual address is a current CPU mapping for
 *	the memory address given. It is only valid to use this function on
 *	addresses that have a kernel mapping
 *
 *	This function does not handle bus mappings for DMA transfers. In
 *	almost all conceivable cases a device driver should not be using
 *	this function
 */

static inline void * phys_to_virt(unsigned long address)
{
	return __va(address);
}
/* Change struct page to physical address */
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)

#define _inb inb
#define _outb outb

#define IO_SPACE_LIMIT	0xffffffff

#define IOMAP_FULL_CACHING		0
#define IOMAP_NOCACHE_SER		1
#define IOMAP_NOCACHE_NONSER		2
#define IOMAP_WRITETHROUGH		3

/* Sameer: Similar to ARM */
/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p

#ifdef __KERNEL__

#define ioread8(X)			readb(X)
#define ioread16(X)			readw(X)
#define ioread32(X)			readl(X)
#define iowrite8(val,X)			writeb(val,X)
#define iowrite16(val,X)		writew(val,X)
#define iowrite32(val,X)		writel(val,X)

#define iowrite32_rep(a,s,c)   outsl((a),(s),(c))
#define ioread32_rep(a,d,c)   insl((a),(d),(c))

#define mmiowb() barrier()

/* Create a virtual mapping cookie for an IO port range */
extern void __iomem *ioport_map(unsigned long port, unsigned int nr);
extern void ioport_unmap(void __iomem *);

/* Create a virtual mapping cookie for a PCI BAR (memory or IO) */
struct pci_dev;
extern void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long max);
extern void pci_iounmap(struct pci_dev *dev, void __iomem *);

#endif

#endif	/* _ASM_ARC_IO_H */


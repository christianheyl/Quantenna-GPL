#ifndef _ASM_ARC_MEM_H
#define _ASM_ARC_MEM_H

/* Kernels Virtual memory area.
 * Unlike other architectures(MIPS, sh, cris ) ARC 700 does not have a 
 * "kernel translated" region (like KSEG2 in MIPS). So we use a upper part 
 * of the translated bottom 2GB for kernel virtual memory and protect
 * these pages from user accesses by disabling Ru, Eu and Wu.
 */

/*   Kernel Virtual memory Layout
 * 
 * 0x00000000------------------->|---------------|
 *                               |   user space  |
 *                               |               |
 *                               |               |
 * TASK_SIZE-------------------->|---------------|
 *                               |               |  USER_KERNEL_GUTTER(0x01000000, just as ARC_SOME_PADDING in
 *                               |      16M      |  2.6.30, some padding between user space and kernel virtual
 *                               |               |  address space, ARC guys also don't know why need it)
 * VMALLOC_START---------------->|---------------|
 *                               |               |
 *                               |   128M/48M    |  VMALLOC_SIZE
 *                               |               |
 * PCI_REMAP_START(0x58000000)-->|---------------|
 *                               |               |
 *                               |     512M      |  PCI_REMAP_SIZE(0x20000000)
 *                               |               |
 * DMA_NOCACHE_START(0x78000000)>|---------------|
 *                               |               |
 *                               |     128M      |  DMA_NOCACHE_SIZE(0x08000000)
 * DMA_NOCACHE_END-----|         |               |
 * PAGE_OFFSET(0x80000000)------>|---------------|
 *                               |               |
 *                               |   kernel 2G   |
 *                               |               |
 *            (0xffffffff)------>|---------------|
 */

#define DMA_NOCACHE_SIZE	(RUBY_MAX_DRAM_SIZE) /*must be >= memory size, not very scalable with DDR size growing*/
#define DMA_NOCACHE_START	(PAGE_OFFSET - DMA_NOCACHE_SIZE)
#define DMA_NOCACHE_END		(PAGE_OFFSET)

/* PCI remapping */
#define PCI_REMAP_SIZE		(320*1024*1024)
#define PCI_REMAP_START		(DMA_NOCACHE_START - PCI_REMAP_SIZE)
#define PCI_REMAP_END		(DMA_NOCACHE_START)

/* FIXME: this is done to accomodate 64MB DDR systems */
#define VMALLOC_SIZE		(192*1024*1024)
#define VMALLOC_START	(PCI_REMAP_START - VMALLOC_SIZE)
#define VMALLOC_END		(PCI_REMAP_START)
#define VMALLOC_VMADDR(x)	((unsigned long)(x))

#endif /* _ARM_ARC_MEM_H */


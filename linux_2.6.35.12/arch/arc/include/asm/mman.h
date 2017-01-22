/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARC_MMAN_H__
#define __ARC_MMAN_H__

#include <linux/fs.h>
#include <asm-generic/mman.h>

#define MAP_SHARED_CODE 0x20000

#ifdef __KERNEL__
#define ARCH_ELF_DO_MMAP
extern unsigned long do_mmap(struct file *file, unsigned long addr,
	unsigned long len, unsigned long prot,
	unsigned long flag, unsigned long offset);
#endif

#endif /* __ARC_MMAN_H__ */

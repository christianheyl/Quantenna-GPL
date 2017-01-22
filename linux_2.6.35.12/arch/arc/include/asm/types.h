/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_TYPES_H
#define _ASM_ARC_TYPES_H

#include <asm-generic/int-ll64.h>

#ifndef __ASSEMBLY__
typedef unsigned short umode_t;
#endif /* !__ASSEMBLY__ */

#ifdef __KERNEL__
#define BITS_PER_LONG 32
#ifndef __ASSEMBLY__
typedef u32 dma_addr_t;
#endif /* !__ASSEMBLY__ */
#endif /* __KERNEL__ */

#endif


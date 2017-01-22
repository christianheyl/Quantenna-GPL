/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_IRQ_H
#define __ASM_ARC_IRQ_H

#include <asm/ptrace.h>
#include <plat_irq.h>   // Board Specific IRQ assignments

#define irq_canonicalize(i)    (i)

#ifndef __ASSEMBLY__

#define disable_irq_nosync(i) disable_irq(i)

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);
extern int  get_hw_config_num_irq(void);

#endif

#endif


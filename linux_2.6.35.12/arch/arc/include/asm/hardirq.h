/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Feb 2008
 *  -Remove local_irq_count( ) from irq_stat no longer required by
 *  generic code
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 *  -Taken from m68knommu
 */

#ifndef _ASM_ARC_HARDIRQ_H
#define _ASM_ARC_HARDIRQ_H

#include <linux/threads.h>
#include <linux/irq.h>

typedef struct {
    unsigned int __softirq_pending;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>  /* Standard mappings for irq_cpustat_t above */

#endif /* _ASM_ARC_HARDIRQ_H */

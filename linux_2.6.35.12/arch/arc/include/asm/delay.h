/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Delay routines using pre computed loops_per_jiffy value.
 *
 * Amit Bhor: Codito Technologies 2004
 */

#ifndef __ASM_ARC_UDELAY_H
#define __ASM_ARC_UDELAY_H

#include <asm/param.h> /* HZ */

extern __inline__ void __delay(unsigned long loops)
{
      __asm__ __volatile__ ( "1: \n\t"
			     "sub.f %0, %1, 1\n\t"
			     "jpnz 1b"
			     : "=r" (loops)
			     : "0" (loops));
}

extern void __bad_udelay(void);

/*
 * Division by multiplication: you don't have to worry about loss of
 * precision.
 *
 * Use only for very small delays ( < 1 msec).  Should probably use a
 * lookup table, really, as the multiplications take much too long with
 * short delays.  This is a "reasonable" implementation, though (and the
 * first constant multiplications gets optimized away if the delay is
 * a constant)
 */
static inline void __const_udelay(unsigned long xloops)
{
	__asm__ ("mpyhu %0, %1, %2"
		 : "=r" (xloops)
		 : "r" (xloops), "r" (loops_per_jiffy));
       __delay(xloops * HZ);
}

static inline void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 4295);	/* 2**32 / 1000000 */
}

#define udelay(n) (__builtin_constant_p(n) ? \
	((n) > 20000 ? __bad_udelay() : __const_udelay((n) * 4295)) : \
	__udelay(n))

#endif	/* __ASM_ARC_UDELAY_H */

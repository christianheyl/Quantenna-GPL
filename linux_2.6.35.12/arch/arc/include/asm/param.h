/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_PARAM_H
#define _ASM_ARC_PARAM_H

#include <asm/page.h>

#define EXEC_PAGESIZE   PAGE_SIZE

#ifdef __KERNEL__
#define HZ CONFIG_HZ
#endif

#ifndef HZ
/* Rate at which the linux jiffies counter is incremented */
#define HZ 100
#endif

/* Sameer: New notion of USER_HZ is introduced. Dunno why yet */
#define USER_HZ 100

#if defined(__KERNEL__) && (HZ == 100)
#define hz_to_std(a) (a)
#endif

#ifndef NGROUPS
#define NGROUPS         32
#endif

#ifndef NOGROUP
#define NOGROUP         (-1)
#endif

/* max length of hostname */
#define MAXHOSTNAMELEN	64

#ifdef __KERNEL__
# define CLOCKS_PER_SEC	HZ
#endif

#endif	/* _ASM_ARC_PARAM_H */


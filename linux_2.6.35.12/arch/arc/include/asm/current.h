/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: May 16th, 2008
 *  - Current macro is now implemented as "global register" r25
 *
 * Rahul Trivedi, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_CURRENT_H
#define _ASM_ARC_CURRENT_H

#ifndef __ASSEMBLY__

#ifdef CONFIG_ARCH_ARC_CURR_IN_REG

#include <asm/current_reg.h>
#define current (curr_arc)

#else  /* ! CONFIG_ARCH_ARC_CURR_IN_REG */

#include <linux/thread_info.h>

static inline struct task_struct * get_current(void)
{
	return current_thread_info()->task;
}
#define current (get_current())

#endif  /* ! CONFIG_ARCH_ARC_CURR_IN_REG */

#endif /* ! __ASSEMBLY__ */

#endif /* _ASM_ARC_CURRENT_H */

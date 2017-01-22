/**
 * Copyright (c) 2012-2013 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#ifndef __FUN_PROFILER_STAT_H
#define __FUN_PROFILER_STAT_H

#ifdef CONFIG_FUNC_PROFILER_STATS

#include <linux/string.h>

#define func_pf_mempcy(to, from, count) do_pf_memcpy(to, from, count, __FILE__, __LINE__)

extern void func_pf_entry_update(char *file, int line);
extern int func_pf_get_stats(char *page, char **start, off_t off, int count, int *eof, void *data);

static inline void *do_pf_memcpy(void *to, const void *from, size_t count, char *file, int line)
{
	func_pf_entry_update(file, line);
	return memcpy(to, from, count);
}

#endif /* CONFIG_FUNC_PROFILER_STATS */

#endif

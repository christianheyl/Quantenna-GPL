/**
 * Copyright (c) 2011 Quantenna Communications, Inc.
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

#ifndef __KDUMP_H__
#define __KDUMP_H__

#if defined(CONFIG_KERNEL_TEXT_SNAPSHOTS)
void kdump_print_ktext_checksum(void);
#else
static __inline__ void kdump_print_ktext_checksum(void) {}
#endif

#if defined(CONFIG_KERNEL_TEXT_SNAPSHOTS) && CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT > 0
int kdump_take_snapshot(const char* description);
void kdump_compare_all_snapshots(void);
void kdump_add_module(struct module *mod);
void kdump_remove_module(struct module *mod);
#else
static __inline__ int kdump_take_snapshot(const char* description) { return 0; }
static __inline__ void kdump_compare_all_snapshots(void) {}
static __inline__ void kdump_add_module(struct module *mod) {}
static __inline__ void kdump_remove_module(struct module *mod) {}
#endif

void kdump_add_troubleshooter(void (*fn)(void));

#endif	// __KDUMP_H__


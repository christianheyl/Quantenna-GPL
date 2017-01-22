/*
 * Copyright (c) 2011 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * Create a wrapper around other bootcfg datastores which compresses on write
 * and decompresses on read.
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

#ifndef __BOOTCFG_DRV_H__
#define __BOOTCFG_DRV_H__

#include <linux/types.h>

struct bootcfg_store_ops {
	int (*read)(struct bootcfg_store_ops *ops, void *buf, const size_t bytes);
	int (*write)(struct bootcfg_store_ops *ops, const void *buf, const size_t bytes);
	int (*init)(struct bootcfg_store_ops *ops, size_t *store_limit);
	void (*exit)(struct bootcfg_store_ops *ops);
};

struct bootcfg_store_ops *bootcfg_get_datastore(void);
struct bootcfg_store_ops *bootcfg_compression_adapter(struct bootcfg_store_ops *raw_accessor);

#endif	/* __BOOTCFG_DRV_H__ */


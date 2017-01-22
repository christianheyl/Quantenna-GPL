/*
 * Copyright (c) 2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * Bootcfg store through filesystem
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

#include "bootcfg_drv.h"
#include "bootcfg_store_init.h"

#include <qtn/bootcfg.h>
#include <common/ruby_version.h>

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

static const char *env_store_path;

static int bootcfg_do_file_op(int (*op)(struct file *, void *, size_t), void *buf, size_t bytes)
{
	int rc = -1;
	int fd;
	struct file *file;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(env_store_path, O_RDWR, 0);
	if (fd < 0) {
		return fd;
	}

	file = fget(fd);
	if (file) {
		rc = op(file, buf, bytes);
		fput(file);
	}

	sys_close(fd);
	set_fs(old_fs);

	return rc;
}

static int bootcfg_do_file_write(struct file *file, void *buf, size_t bytes)
{
	int rc;
	loff_t offset = 0;

	rc = vfs_write(file, buf, bytes, &offset);

	return rc < 0 ? rc : 0;
}

static int bootcfg_do_file_read(struct file *file, void *buf, size_t bytes)
{
	int rc;
	loff_t offset = 0;

	rc = vfs_read(file, buf, bytes, &offset);

	return rc < 0 ? rc : 0;
}

static int bootcfg_file_write(struct bootcfg_store_ops *ops, const void* buf, const size_t bytes)
{
	return bootcfg_do_file_op(&bootcfg_do_file_write, (void *)buf, bytes);
}

static int bootcfg_file_read(struct bootcfg_store_ops *ops, void* buf, const size_t bytes)
{
	return bootcfg_do_file_op(&bootcfg_do_file_read, (void *)buf, bytes);
}

int __init bootcfg_file_init(struct bootcfg_store_ops *ops, size_t *store_limit)
{
	return 0;
}

void __exit bootcfg_file_exit(struct bootcfg_store_ops *ops)
{
}

static struct bootcfg_store_ops file_store_ops = {
	.read	= bootcfg_file_read,
	.write	= bootcfg_file_write,
	.init	= bootcfg_file_init,
	.exit	= __devexit_p(bootcfg_file_exit),
};

struct bootcfg_store_ops * __init bootcfg_file_get_ops(const char *path)
{
	env_store_path = path;

	printk("%s: using file storage '%s'\n", __FUNCTION__, path);

	return &file_store_ops;
}

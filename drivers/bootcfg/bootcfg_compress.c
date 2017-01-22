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
#include "bootcfg_drv.h"

#include <qtn/bootcfg.h>
#include <common/ruby_partitions.h>
#include <common/ruby_version.h>

#include <linux/init.h>
#include <linux/zlib.h>
#include <linux/zutil.h>
#include <linux/module.h>
#include <linux/slab.h>

struct bootcfg_zops_data {
	struct bootcfg_store_ops outer_ops;
	struct bootcfg_store_ops *inner_ops;
	size_t inner_store_limit;
	z_stream inflate_stream;
	z_stream deflate_stream;
};

static struct bootcfg_zops_data *get_zops_data(struct bootcfg_store_ops *ops)
{
	return (struct bootcfg_zops_data*)ops;
}

static struct bootcfg_store_ops *get_inner_ops(struct bootcfg_store_ops *ops)
{
	return get_zops_data(ops)->inner_ops;
}

int __init bootcfg_zadpt_init(struct bootcfg_store_ops *ops, size_t *store_limit)
{
	struct bootcfg_zops_data *data = get_zops_data(ops);
	struct bootcfg_store_ops *inner_ops = get_inner_ops(ops);
	int ret = 0;

	data->inflate_stream.workspace = kzalloc(zlib_inflate_workspacesize(), GFP_KERNEL);
	if (data->inflate_stream.workspace == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	data->deflate_stream.workspace = kmalloc(zlib_deflate_workspacesize(), GFP_KERNEL);
	if (data->deflate_stream.workspace == NULL) {
		ret = -ENOMEM;
		goto out_free_inf;
	}

	ret = inner_ops->init(inner_ops, &data->inner_store_limit);
	if (ret) {
		goto out_free_both;
	}

	return 0;

out_free_both:
	kfree(data->deflate_stream.workspace);
out_free_inf:
	kfree(data->inflate_stream.workspace);
out:
	kfree(ops);

	return ret;
}

void __exit bootcfg_zadpt_exit(struct bootcfg_store_ops *ops)
{
	struct bootcfg_zops_data *data = get_zops_data(ops);
	struct bootcfg_store_ops *inner_ops = get_inner_ops(ops);

	kfree(data->deflate_stream.workspace);
	kfree(data->inflate_stream.workspace);

	inner_ops->exit(inner_ops);
	kfree(ops);
}

static int bootcfg_zadpt_read(struct bootcfg_store_ops *ops, void* buf, const size_t bytes)
{
	struct bootcfg_zops_data *data = get_zops_data(ops);
	struct bootcfg_store_ops *inner_ops = get_inner_ops(ops);
	uint8_t *inner_buf;
	uint32_t compressed_size = 0;
	size_t inner_buf_size;
	int ret = 0;

	inner_buf_size = bytes;
	if (data->inner_store_limit && data->inner_store_limit < inner_buf_size) {
		inner_buf_size = data->inner_store_limit;
	}

	inner_buf = kzalloc(inner_buf_size, GFP_KERNEL);
	if (inner_buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = inner_ops->read(inner_ops, inner_buf, sizeof(uint32_t));
	if (ret) {
		goto out;
	}

	/* get compressed size (first 4 bytes) and sanity check */
	memcpy(&compressed_size, &inner_buf[0], sizeof(uint32_t));
	if (compressed_size > inner_buf_size - sizeof(uint32_t)) {
		ret = -ENODATA;
		goto out;
	}

	ret = inner_ops->read(inner_ops, inner_buf, compressed_size + sizeof(uint32_t));
	if (ret) {
		goto out;
	}

	data->inflate_stream.next_in = &inner_buf[sizeof(uint32_t)];
	data->inflate_stream.total_in = 0;
	data->inflate_stream.avail_in = compressed_size;
	data->inflate_stream.next_out = buf;
	data->inflate_stream.total_out = 0;
	data->inflate_stream.avail_out = bytes;

	ret = zlib_inflateInit(&data->inflate_stream);
	if (ret != Z_OK) {
		ret = -EIO;
		goto out;
	}

	ret = zlib_inflate(&data->inflate_stream, Z_FINISH);
	if (ret != Z_STREAM_END) {
		printk(KERN_ERR "%s deflate failed: ret %d\n", __FUNCTION__, ret);
		ret = -ENODATA;
		goto out;
	}

#ifdef DEBUG
	printk(KERN_DEBUG "%s zlib decompressed %ld bytes into %ld\n",
			__FUNCTION__, data->inflate_stream.total_in, data->inflate_stream.total_out);
#endif

	ret = 0;

out:
	if (inner_buf) {
		kfree(inner_buf);
	}
	return ret;
}

static int bootcfg_zadpt_write(struct bootcfg_store_ops *ops, const void* buf, const size_t bytes)
{
	struct bootcfg_zops_data *data = get_zops_data(ops);
	struct bootcfg_store_ops *inner_ops = get_inner_ops(ops);
	uint8_t *inner_buf = NULL;
	size_t inner_buf_size;
	uint32_t compressed_size;
	int ret = 0;

	inner_buf_size = bytes;
	if (data->inner_store_limit && data->inner_store_limit < inner_buf_size) {
		inner_buf_size = data->inner_store_limit;
	}

	inner_buf = kzalloc(inner_buf_size, GFP_KERNEL);
	if (inner_buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = zlib_deflateInit(&data->deflate_stream, 3);
	if (ret != Z_OK) {
		ret = -EIO;
		goto out;
	}

	data->deflate_stream.next_in = buf;
	data->deflate_stream.total_in = 0;
	data->deflate_stream.avail_in = bytes;
	data->deflate_stream.next_out = &inner_buf[sizeof(uint32_t)];
	data->deflate_stream.total_out = 0;
	data->deflate_stream.avail_out = inner_buf_size - sizeof(uint32_t);

	ret = zlib_deflate(&data->deflate_stream, Z_FINISH);
	if (ret != Z_STREAM_END) {
		printk(KERN_ERR "%s deflate failed: ret %d\n", __FUNCTION__, ret);
		ret = -ENOSPC;
		goto out;
	}

#ifdef DEBUG
	printk(KERN_DEBUG "%s zlib compressed %ld bytes into %ld\n",
			__FUNCTION__, data->deflate_stream.total_in, data->deflate_stream.total_out);
#endif

	compressed_size = data->deflate_stream.total_out;
	memcpy(inner_buf, &compressed_size, sizeof(compressed_size));

	ret = inner_ops->write(inner_ops, inner_buf, compressed_size + sizeof(uint32_t));

out:
	if (inner_buf) {
		kfree(inner_buf);
	}
	return ret;
}


static const struct bootcfg_store_ops zapdt_outer_ops = {
	.read	= bootcfg_zadpt_read,
	.write	= bootcfg_zadpt_write,
	.init	= bootcfg_zadpt_init,
	.exit	= __devexit_p(bootcfg_zadpt_exit),
};

struct bootcfg_store_ops *bootcfg_compression_adapter(struct bootcfg_store_ops *raw_accessor)
{
	struct bootcfg_zops_data *z_ops = kmalloc(sizeof(*z_ops), GFP_KERNEL);
	if (z_ops == NULL) {
		return NULL;
	}

	z_ops->outer_ops = zapdt_outer_ops;
	z_ops->inner_ops = raw_accessor;

	return &z_ops->outer_ops;
}


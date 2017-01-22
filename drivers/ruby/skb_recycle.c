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

#include <ruby_mem.h>
#include <qtn/skb_recycle.h>
#include <linux/proc_fs.h>

#define IEEE80211_SKB_LIST_MAX_DEFAULT 1024
#define SKB_RECYCLE_MAX_PROC "qtn_skb_recycle_max"
#define SKB_RECYCLE_MAX_CMD_LEN 8

struct proc_dir_entry *skb_recycle_max_proc = NULL;
struct qtn_skb_recycle_list __sram_data __qtn_skb_recycle_list;
EXPORT_SYMBOL(__qtn_skb_recycle_list);

static int
skb_recycle_max_read(char *buffer, char **buffer_location, off_t offset,
	int buffer_length, int *eof, void *data)
{
	int len = 0;
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();

	if (recycle_list)
		len = sprintf(buffer, "%d\n", recycle_list->max);

	return len;
}

static int
skb_recycle_max_write(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	char tmp[SKB_RECYCLE_MAX_CMD_LEN] = {0};
	uint32_t max = 0;

	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();

	if (!recycle_list)
		goto out;

	if (count >= SKB_RECYCLE_MAX_CMD_LEN) {
		printk(KERN_ERR "%s: Invalid parameters\n", __FUNCTION__);
		goto out;
	}

	if (copy_from_user(tmp, buffer, count)) {
		printk(KERN_ERR "%s: Failed\n", __FUNCTION__);
		goto out;
	}

	if (sscanf(tmp, "%u", &max) == 1) {
		recycle_list->max = max;
	} else {
		printk(KERN_ERR "Invalid parameter: %s\n", tmp);
	}

out:
	return count;
}

static int __sram_text skb_list_recycle(struct qtn_skb_recycle_list *recycle_list, struct sk_buff *skb)
{
	/*
	 * must run kernel recycle check to clean up the buffers freed here
	 */
	if (skb_recycle_check(skb, qtn_rx_buf_size()) &&
			qtn_skb_recycle_list_push(recycle_list, &recycle_list->stats_kfree, skb)) {
		return 1;
	}
	return 0;
}

static int __init qtn_recycle_list_init(void)
{
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();

	skb_queue_head_init(&recycle_list->list);
	recycle_list->max = IEEE80211_SKB_LIST_MAX_DEFAULT;
	recycle_list->recycle_func = &skb_list_recycle;

	skb_recycle_max_proc = create_proc_entry(SKB_RECYCLE_MAX_PROC, 0x644, NULL);
	if (skb_recycle_max_proc == NULL) {
		printk(KERN_ERR "unable to create /proc/%s\n", SKB_RECYCLE_MAX_PROC);
		return 0;
	}
	skb_recycle_max_proc->read_proc = skb_recycle_max_read;
	skb_recycle_max_proc->write_proc = skb_recycle_max_write;
	skb_recycle_max_proc->mode = S_IFREG | S_IRUGO;
	skb_recycle_max_proc->uid = 0;
	skb_recycle_max_proc->gid = 0;
	skb_recycle_max_proc->size = 0x1000;
	skb_recycle_max_proc->data = NULL;

	return 0;
}

arch_initcall(qtn_recycle_list_init);


/*-
 * Copyright (c) 2014 Quantenna
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ieee80211_mlme_statistics.c 1 2014-01-17 12:00:00Z vsaiapin $
 */

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/jhash.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#if defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif /* defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS) */
#include <linux/cdev.h>
#include <linux/workqueue.h>

#include "net80211/ieee80211_mlme_statistics.h"
#include "net80211/if_media.h"
#include "net80211/ieee80211_var.h"

#ifdef MLME_STATS_DEBUG
#define MLME_STATS_MAC_HASH_SIZE			8
#define MLME_STATS_MAX_CLIENTS				25
#else /* MLME_STATS_DEBUG */
#define MLME_STATS_MAC_HASH_SIZE			128
#define MLME_STATS_MAX_CLIENTS				128
#endif /* MLME_STATS_DEBUG */

#ifdef MLME_STATS_PROCFS
#define MLME_STATS_PROC_FILENAME			"mlmestats"
#endif /* MLME_STATS_PROCFS */

#ifdef MLME_STATS_DEVFS
#define MLME_STATS_DEVFS_NAME				"mlmestats"
#define MLME_STATS_DEVFS_MAJOR				0xEF
#endif /* MLME_STATS_DEVFS */

struct mlme_delayed_update_task {
	unsigned char mac_addr[IEEE80211_ADDR_LEN];
	unsigned int statistics_entry;
	unsigned int incrementor;
	struct work_struct work;
};

struct mlme_stats_node {
	struct mlme_stats_record stats;
	struct list_head lru_link;
	struct hlist_node hash_link;
};

struct mlme_stats_factory {
	struct hlist_head mac_hash_table[MLME_STATS_MAC_HASH_SIZE];
	unsigned int nodes_count;

	/* This 'lru_list_head' node will be used for anonymous statistics */
	struct mlme_stats_node lru_list_head;
	spinlock_t access_lock;
	struct workqueue_struct *delayed_update_wq;
};

static atomic_t mlme_active = ATOMIC_INIT(0);
static struct mlme_stats_factory mlme_statistics;

static void mlme_stats_lock(void)
{
	spin_lock_bh(&mlme_statistics.access_lock);
}

static void mlme_stats_unlock(void)
{
	spin_unlock_bh(&mlme_statistics.access_lock);
}

static int mlme_stats_mac_hash(unsigned char *mac_addr)
{
	return jhash(mac_addr, IEEE80211_ADDR_LEN, 0) & (MLME_STATS_MAC_HASH_SIZE - 1);
}

/**
 * mlme_stats_update_node - increment required stat in the node
 * @stat_record: client's node
 * @statistics_entry: entry of stats needs to be updated
 * @incrementor: value which will be added to the stat entry
 *
 * Update client's node according parameters.
 * Assumes that factory is locked.
 */
static void mlme_stats_update_node(struct mlme_stats_node *stat_node, unsigned int statistics_entry, unsigned int incrementor)
{
	unsigned int *node_stats_array = (unsigned int*)&stat_node->stats.auth;

	if (statistics_entry >= MLME_STAT_MAX) {
#ifdef MLME_STATS_DEBUG
		printk(KERN_WARNING "Wrong statistics entry, ignoring it");
#endif /* MLME_STATS_DEBUG */
		return;
	}

	node_stats_array[statistics_entry] += incrementor;
}

/**
 * mlme_stats_locate_node - search a node for client
 * @mac_addr: client's mac address
 * @mac_hash: hash of client's mac address
 *
 * Search a node for specified client through
 * lru_list. Return null if node isn't found
 */
static struct mlme_stats_node* mlme_stats_locate_node(unsigned char *mac_addr, int mac_hash)
{
	struct hlist_head *hash_head = &mlme_statistics.mac_hash_table[mac_hash];
	struct mlme_stats_node *node_iterator = NULL;
	struct hlist_node *loop_cursor = NULL;
	hlist_for_each_entry (node_iterator, loop_cursor, hash_head,hash_link) {
		if (IEEE80211_ADDR_EQ(node_iterator->stats.mac_addr, mac_addr)) {
			return node_iterator;
		}
	}
	return NULL;
}

/**
 * mlme_stats_add_node - add new node
 * @mac_addr: mac address of new client
 * @mac_hash: hash of the mac address
 *
 * Add new node, put it to the head of LRU list and
 * add it to the hash table.
 * Assumes that factory is locked.
 */
static struct mlme_stats_node* mlme_stats_add_node(unsigned char *mac_addr, int mac_hash)
{
	struct mlme_stats_node* new_stats_node = NULL;
	if (mlme_statistics.nodes_count == MLME_STATS_MAX_CLIENTS) {
		new_stats_node = list_entry(mlme_statistics.lru_list_head.lru_link.prev,
												struct mlme_stats_node, lru_link);
		hlist_del(&new_stats_node->hash_link);
		list_del(&new_stats_node->lru_link);
	} else {
#ifdef MLME_STATS_DEBUG
		if (in_atomic()) {
			printk("kzalloc in atomic context\n");
		}
		if (in_softirq()) {
			printk("kzalloc in softirq context\n");
		}
#endif /* MLME_STATS_DEBUG */
		new_stats_node = kzalloc(sizeof(*new_stats_node), GFP_KERNEL);
		if (new_stats_node == NULL) {
#ifdef MLME_STATS_DEBUG
			printk(KERN_ERR "Failed to allocate emory for new stats node\n");
#endif /* MLME_STATS_DEBUG */
			return NULL;
		}
		++mlme_statistics.nodes_count;
	}
	memcpy(new_stats_node->stats.mac_addr, mac_addr, IEEE80211_ADDR_LEN);

	INIT_LIST_HEAD(&new_stats_node->lru_link);
	list_add(&new_stats_node->lru_link, &mlme_statistics.lru_list_head.lru_link);
	hlist_add_head(&new_stats_node->hash_link, &mlme_statistics.mac_hash_table[mac_hash]);
	return new_stats_node;
}

#if defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS)
void *mlme_stats_seq_start(struct seq_file *m, loff_t *pos)
{
	loff_t off;
	struct mlme_stats_node *iterator, *iterator_tmp;

	mlme_stats_lock();

	if (!*pos) {
		return SEQ_START_TOKEN;
	}

	off = 1;
	list_for_each_entry_safe (iterator, iterator_tmp, &mlme_statistics.lru_list_head.lru_link, lru_link) {
		if (off++ == *pos) {
			return iterator;
		}
	}

	return NULL;
}

void *mlme_stats_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct mlme_stats_node *next_iterator;

	if (++*pos > mlme_statistics.nodes_count) {
		return NULL;
	}

	if (v == SEQ_START_TOKEN) {
		next_iterator = list_entry(mlme_statistics.lru_list_head.lru_link.next,
								   struct mlme_stats_node, lru_link);
	} else {
		next_iterator = list_entry(((struct mlme_stats_node *)v)->lru_link.next,
								   struct mlme_stats_node, lru_link);
	}

	return next_iterator;
}
int mlme_stats_seq_show(struct seq_file *m, void *v)
{
	int i;
	struct mlme_stats_node *current_node;
	unsigned int *node_stats_array;

	if (v == SEQ_START_TOKEN) {
		seq_printf(m, "                  %11s%11s%11s%11s%11s%11s\n",
				   "auth", "auth_fail", "assoc", "assoc_fail", "deauth", "diassoc");
		node_stats_array = (unsigned int*)&mlme_statistics.lru_list_head.stats.auth;
		seq_printf(m, "00:00:00:00:00:00 ");
	} else {
		current_node = (struct mlme_stats_node *)v;
		node_stats_array = (unsigned int*)&current_node->stats.auth;
		seq_printf(m, "%pM ", current_node->stats.mac_addr);
	}

	for (i = 0;i < MLME_STAT_MAX; ++i) {
		seq_printf(m, "%11u", node_stats_array[i]);
	}
	seq_putc(m, '\n');
	return 0;
}
void mlme_stats_seq_stop(struct seq_file *m, void *v)
{
	mlme_stats_unlock();
}

struct seq_operations mlme_stats_seq_fops = {
	.start   = mlme_stats_seq_start,
	.stop    = mlme_stats_seq_stop,
	.next    = mlme_stats_seq_next,
	.show    = mlme_stats_seq_show,
};
static int mlme_stats_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mlme_stats_seq_fops);
};

static const struct file_operations mlme_stats_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = mlme_stats_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};
#endif /* defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS) */

#ifdef MLME_STATS_DEVFS
static long mlme_stats_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	int ret = 0;
	unsigned int max_clients = MLME_STATS_MAX_CLIENTS;
	struct mlme_stats_node *iterator = NULL, *iterator_tmp = NULL;
	unsigned char *macs_buffer = NULL, *macs_ptr = NULL;

	if (!arg) {
		return -EFAULT;
	}

	mlme_stats_lock();
	switch (cmd) {
		case MLME_STATS_IOC_GET_MAX_CLIENTS:
			if (copy_to_user((unsigned int __user*)arg, &max_clients, sizeof(max_clients)) != 0) {
				ret = -EFAULT;
			}
			break;
		case MLME_STATS_IOC_GET_CUR_CLIENTS:
			if (copy_to_user((unsigned int __user*)arg, &mlme_statistics.nodes_count, sizeof(mlme_statistics.nodes_count)) != 0) {
				ret = -EFAULT;
			}
			break;
		case MLME_STATS_IOC_GET_ALL_MACS:
			if (mlme_statistics.nodes_count < MLME_STATS_MAX_CLIENTS) {
				max_clients = mlme_statistics.nodes_count + 1;
			}
			macs_buffer = kmalloc(IEEE80211_ADDR_LEN * max_clients, GFP_KERNEL);
			if (macs_buffer != NULL) {
				macs_ptr = macs_buffer;
				list_for_each_entry_safe (iterator, iterator_tmp, &mlme_statistics.lru_list_head.lru_link, lru_link) {
					memcpy(macs_ptr, iterator->stats.mac_addr, IEEE80211_ADDR_LEN);
					macs_ptr += IEEE80211_ADDR_LEN;
				}
				if (mlme_statistics.nodes_count < MLME_STATS_MAX_CLIENTS) {
					memset(macs_ptr, 0xFF, IEEE80211_ADDR_LEN);
				}
				if (copy_to_user((unsigned char __user*)arg, macs_buffer, max_clients * IEEE80211_ADDR_LEN) != 0) {
					ret = -EFAULT;
				}
				kfree(macs_buffer);
			} else {
				ret = -ENOMEM;
			}
			break;
		case MLME_STATS_IOC_GET_CLIENT_STATS:
			macs_buffer = kmalloc(IEEE80211_ADDR_LEN, GFP_KERNEL);
			if (macs_buffer != NULL) {
				if (copy_from_user(macs_buffer, (unsigned char __user*)arg, IEEE80211_ADDR_LEN) != 0) {
					ret = -EFAULT;
				} else {
					if (memcmp(macs_buffer, mlme_statistics.lru_list_head.stats.mac_addr, IEEE80211_ADDR_LEN) == 0) {
						if (copy_to_user((struct mlme_stats_record __user*)arg, &mlme_statistics.lru_list_head.stats,
										 sizeof(struct mlme_stats_record))) {
							ret = -EFAULT;
						}
					} else {
						iterator = mlme_stats_locate_node(macs_buffer, mlme_stats_mac_hash(macs_buffer));
						if (iterator != NULL) {
							if (copy_to_user((struct mlme_stats_record __user*)arg, &iterator->stats,
											 sizeof(struct mlme_stats_record))) {
								ret = -EFAULT;
							}
						} else {
							ret = -ENXIO;
						}
					}
				}
				kfree(macs_buffer);
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}
	mlme_stats_unlock();

	return ret;
}

static ssize_t mlme_stats_read(struct file *file, char __user *buf,
							   size_t count, loff_t *ppos)
{
	unsigned char __user* user_ptr = (unsigned char __user*)buf;
	struct mlme_stats_node *iterator,*iterator_tmp;
	unsigned int skip_records = (unsigned int)*ppos /
								(unsigned int)sizeof(struct mlme_stats_record);
	unsigned int record_offset = (unsigned int)*ppos %
								 (unsigned int)sizeof(struct mlme_stats_record);
	size_t bytes_to_copy;
	unsigned int remain_to_copy = count;

	mlme_stats_lock();

	if (skip_records == 0) {
		// Anonymous record should requested as well
		bytes_to_copy = sizeof(struct mlme_stats_record) - record_offset;
		bytes_to_copy = min(remain_to_copy, bytes_to_copy);
		copy_to_user(user_ptr, (unsigned char*)&mlme_statistics.lru_list_head.stats + record_offset, bytes_to_copy);
		user_ptr += bytes_to_copy;
		*ppos += bytes_to_copy;
		remain_to_copy -= bytes_to_copy;
		if (record_offset > 0) {
			record_offset = 0;
		}
	} else {
		--skip_records;
	}
	if (!remain_to_copy) {
		goto all_read;
	}
    list_for_each_entry_safe (iterator, iterator_tmp, &mlme_statistics.lru_list_head.lru_link, lru_link) {
		if (skip_records > 0) {
			--skip_records;
			continue;
		}
		bytes_to_copy = sizeof(struct mlme_stats_record) - record_offset;
		bytes_to_copy = min(remain_to_copy, bytes_to_copy);
		copy_to_user(user_ptr, (unsigned char*)&iterator->stats + record_offset, bytes_to_copy);
		user_ptr += bytes_to_copy;
		*ppos += bytes_to_copy;
		remain_to_copy -= bytes_to_copy;
		if (record_offset > 0) {
			record_offset = 0;
		}
		if (remain_to_copy == 0) {
			goto all_read;
		}
	}
	if (skip_records > 0 || record_offset > 0) {
		mlme_stats_unlock();
		return -ESPIPE;
	}
all_read:
	mlme_stats_unlock();

	return count - remain_to_copy;
}

static const struct file_operations mlme_stats_fops = {
	.owner = THIS_MODULE,
	.read = mlme_stats_read,
	.unlocked_ioctl = mlme_stats_ioctl,
};
static struct cdev mlme_stats_dev;
#endif /* MLME_STATS_DEVFS */

static void perform_delayed_update(struct work_struct *work)
{
	struct mlme_delayed_update_task *owner = container_of(work, struct mlme_delayed_update_task, work);
	mlme_stats_update(owner->mac_addr, owner->statistics_entry, owner->incrementor);
	kfree(owner);
}


void mlme_stats_update(unsigned char *mac_addr, unsigned int statistics_entry,
					   unsigned int incrementor)
{
	if (atomic_read(&mlme_active) != 1) {
		return;
	}

	mlme_stats_lock();
	int mac_hash = mlme_stats_mac_hash(mac_addr);
	struct mlme_stats_node* stats_node = mlme_stats_locate_node(mac_addr, mac_hash);

	/* Client statistics node already exists, simply update it */
	if (stats_node != NULL) {
		mlme_stats_update_node(stats_node, statistics_entry,incrementor);
		list_move(&stats_node->lru_link, &mlme_statistics.lru_list_head.lru_link);
		goto unlock_and_exit;
	}

	/* If client entry doesn't exist and this is not auth request
	 * than update anonymous statistics
	 */
	if (statistics_entry != 0) {
		mlme_stats_update_node(&mlme_statistics.lru_list_head, statistics_entry, incrementor);
		goto unlock_and_exit;
	}

	// Create node for the new client
	stats_node = mlme_stats_add_node(mac_addr, mac_hash);
	if (stats_node == NULL) {
		goto unlock_and_exit;
	}
	mlme_stats_update_node(stats_node, statistics_entry, incrementor);
unlock_and_exit:
	mlme_stats_unlock();
}
EXPORT_SYMBOL(mlme_stats_update);

void mlme_stats_delayed_update(unsigned char *mac_addr, unsigned int statistics_entry,
							   unsigned int incrementor)
{
	struct mlme_delayed_update_task *new_work = NULL;

	if (mlme_statistics.delayed_update_wq == NULL) {
		return;
	}
	if (atomic_read(&mlme_active) != 1) {
		return;
	}
	new_work = kmalloc(sizeof(*new_work), GFP_ATOMIC);
	if (new_work != NULL) {
		INIT_WORK(&new_work->work, perform_delayed_update);
		memcpy(new_work->mac_addr, mac_addr, IEEE80211_ADDR_LEN);
		new_work->statistics_entry = statistics_entry;
		new_work->incrementor = incrementor;
		queue_work(mlme_statistics.delayed_update_wq, &new_work->work);
	}
}
EXPORT_SYMBOL(mlme_stats_delayed_update);

#ifdef MLME_STATS_DEBUG
static void mlme_stats_dump_lru(void)
{
	struct mlme_stats_node *iterator, *iterator_tmp;

	mlme_stats_lock();
	printk("==== BEGIN OF LRU DUMP ====\n");
	list_for_each_entry_safe (iterator, iterator_tmp, &mlme_statistics.lru_list_head.lru_link, lru_link) {
		printk("%pM [%d]\n", iterator->stats.mac_addr, mlme_stats_mac_hash(iterator->stats.mac_addr));
	}
	printk("===== END OF LRU DUMP =====\n");
	mlme_stats_unlock();
}
static void mlme_stats_dump_hash_table(void)
{
	struct hlist_head *hash_head = NULL;
	struct mlme_stats_node *node_iterator = NULL;
	struct hlist_node *loop_cursor = NULL;
	int i;
	mlme_stats_lock();
	printk("==== BEGIN OF HASH TABLE DUMP ====\n");
	for (i = 0;i < MLME_STATS_MAC_HASH_SIZE; ++i) {
		hash_head = &mlme_statistics.mac_hash_table[i];
		printk("%5d  ",i);
		hlist_for_each_entry (node_iterator, loop_cursor, hash_head, hash_link) {
			printk("->%pM", node_iterator->stats.mac_addr);
		}
		printk("\n");
	}
	printk("===== END OF HASH TABLE DUMP =====\n");
	mlme_stats_unlock();
}
void test_func(void)
{
	unsigned char mac[] = {0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
	unsigned short *mac_diff = (unsigned short*)mac;
	int i, j = 1;


	for (i = 0;i < 1024; ++i, j = (j + 1) & 0x0F, *mac_diff = *mac_diff + 1) {
		if (!j) j = 1;
		mlme_stats_update(mac, MLME_STAT_ASSOC, j);
	}

	mlme_stats_dump_lru();
	mlme_stats_dump_hash_table();
}
#endif /* MLME_STATS_DEBUG */

void mlme_stats_init(void)
{
	int i = 0;

	memset(&mlme_statistics, 0x00, sizeof(mlme_statistics));
	for (;i < MLME_STATS_MAC_HASH_SIZE; ++i) {
		INIT_HLIST_HEAD(&mlme_statistics.mac_hash_table[i]);
	}
	spin_lock_init(&mlme_statistics.access_lock);
	INIT_LIST_HEAD(&mlme_statistics.lru_list_head.lru_link);
	mlme_statistics.delayed_update_wq = create_singlethread_workqueue("mlmestats");
	if (mlme_statistics.delayed_update_wq == NULL) {
		printk(KERN_WARNING "Unable to create workqueue for mlme stats\n");
	}
	atomic_set(&mlme_active, 1);

#if defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS)
	if (!proc_create(MLME_STATS_PROC_FILENAME, 0400, NULL, &mlme_stats_proc_fops)) {
		printk(KERN_ERR "Unable to create proc file\n");
	}
#endif /* defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS) */

#ifdef MLME_STATS_DEVFS
	cdev_init(&mlme_stats_dev, &mlme_stats_fops);
	mlme_stats_dev.owner = THIS_MODULE;
	dev_t mlme_stats_dev_id = MKDEV(MLME_STATS_DEVFS_MAJOR, 0);
	if (register_chrdev_region(mlme_stats_dev_id, 1, MLME_STATS_DEVFS_NAME) < 0) {
		printk(KERN_WARNING "Unable to register major number for mlme stats\n");
	} else {
		if (cdev_add(&mlme_stats_dev, mlme_stats_dev_id, 1) < 0) {
			printk(KERN_WARNING "Unable to add character device for mlme stats\n");
		}
	}
#endif /* MLME_STATS_DEVFS */

#ifdef MLME_STATS_DEBUG
	test_func();
#endif /* MLME_STATS_DEBUG */
}
EXPORT_SYMBOL(mlme_stats_init);

void mlme_stats_exit(void)
{
	struct mlme_stats_node *iterator,*iterator_tmp;

#ifdef MLME_STATS_DEVFS
	unregister_chrdev_region(mlme_stats_dev.dev, mlme_stats_dev.count);
	cdev_del(&mlme_stats_dev);
#endif /* MLME_STATS_DEVFS */

#if defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS)
	remove_proc_entry(MLME_STATS_PROC_FILENAME, NULL);
#endif /* defined(CONFIG_PROC_FS) && defined(MLME_STATS_PROCFS) */

	atomic_set(&mlme_active, 0);
	if (mlme_statistics.delayed_update_wq != NULL) {
		flush_workqueue(mlme_statistics.delayed_update_wq);
		destroy_workqueue(mlme_statistics.delayed_update_wq);
	}

	list_for_each_entry_safe (iterator, iterator_tmp, &mlme_statistics.lru_list_head.lru_link, lru_link) {
		list_del(&iterator->lru_link);
		kfree(iterator);
	}
}
EXPORT_SYMBOL(mlme_stats_exit);

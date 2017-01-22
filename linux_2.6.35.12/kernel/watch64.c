/*
 * kernel/watch64.c
 *
 * Copyright (C) 2003 Josef "Jeff" Sipek <jeffpc@xxxxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/param.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/seqlock.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/watch64.h>

/*
 * Watch64 global variables
 */

spinlock_t watch64_biglock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(watch64_head);
struct timer_list watch64_timer;
int watch64_setup;

#if (BITS_PER_LONG == 64)

void watch64_init(void)
{
}

void watch64_run(unsigned long var)
{
}

int watch64_register(unsigned long* ptr, unsigned int interval)
{
	return 0;
}

int watch64_unregister(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

void watch64_rcufree(void* p)
{
}

struct watch64* watch64_find(unsigned long* ptr)
{
	return NULL;
}

struct watch64* __watch64_find(unsigned long* ptr)
{
	return NULL;
}

int watch64_disable(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

int __watch64_disable(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

int watch64_reset(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

int __watch64_reset(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

int watch64_enable(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

int __watch64_enable(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

int watch64_toggle(unsigned long* ptr, struct watch64* st)
{
	return 0;
}

inline u_int64_t watch64_getval(unsigned long* ptr, struct watch64* st)
{
	return (u_int64_t) *ptr;
}

#else

/*
 * Initiate watch64 system
 */

void watch64_init(void)
{
	spin_lock(&watch64_biglock);

	if (watch64_setup==WATCH64_MAGIC) {
		spin_unlock(&watch64_biglock);
		return;
	}

	printk(KERN_WARNING "watch64: 2003/08/22 Josef 'Jeff' Sipek "
			"<jeffpc@xxxxxxxxxxxxx>\n");
	printk(KERN_WARNING "watch64: Enabling Watch64 extensions...");

	init_timer(&watch64_timer);
	watch64_timer.function = watch64_run;
	watch64_timer.data = (unsigned long) NULL;
	watch64_timer.expires = jiffies + WATCH64_MINIMUM;
	add_timer(&watch64_timer);

	printk("done.\n");

	watch64_setup = WATCH64_MAGIC;

	spin_unlock(&watch64_biglock);
}

/*
 * Go through the list of registered variables and check them for changes
 */

void watch64_run(unsigned long var)
{
	struct list_head* entry;
	struct watch64* watch_struct;
	unsigned long tmp;

	rcu_read_lock();
	__list_for_each_rcu(entry, &watch64_head) {
		watch_struct = list_entry(entry, struct watch64, list);
		if (*watch_struct->ptr == watch_struct->oldval)
			continue;

		tmp = *watch_struct->ptr;
		if (tmp > watch_struct->oldval) {
			write_seqlock(&watch_struct->lock);
			watch_struct->total += tmp - watch_struct->oldval;
			write_sequnlock(&watch_struct->lock);
		} else if (tmp < watch_struct->oldval) {
			write_seqlock(&watch_struct->lock);
			watch_struct->total += ((u_int64_t)1 << BITS_PER_LONG) -
					watch_struct->oldval + tmp;
			write_sequnlock(&watch_struct->lock);
		}

		watch_struct->oldval = tmp;
	}
	rcu_read_unlock();

	mod_timer(&watch64_timer, jiffies + WATCH64_MINIMUM);
}

/*
 * Register a new variable with watch64
 */

int watch64_register(unsigned long* ptr, unsigned int interval)
{
	struct watch64* temp;

	temp = (struct watch64*)kmalloc(sizeof(struct watch64), GFP_ATOMIC);

	if (!temp)
		return -ENOMEM;

	if (watch64_setup != WATCH64_MAGIC)
		watch64_init();

	temp->ptr = ptr;
	temp->oldval = 0;
	temp->total = 0;
	if (interval == 0)
		temp->interval = WATCH64_INTERVAL;
	else if (interval < WATCH64_MINIMUM) {
		temp->interval = WATCH64_MINIMUM;
		printk("watch64: attempted to add new watch with "
				"interval below %d jiffies",WATCH64_MINIMUM);
	} else
		temp->interval = interval;

	temp->active = 0;

	seqlock_init(&temp->lock);

	list_add_rcu(&temp->list, &watch64_head);

	return 0;
}

/*
 * Unregister a variable with watch64
 */

int watch64_unregister(unsigned long* ptr, struct watch64* st)
{
	rcu_read_lock();
	if (!st)
		st = __watch64_find(ptr);

	if (!st)
		return -EINVAL;

	__watch64_disable(ptr, st);
	list_del_rcu(&st->list);

	call_rcu(&st->rcuhead, watch64_rcufree);
	rcu_read_unlock();

	return 0;
}

/*
 * Free memory via RCU
 */

void watch64_rcufree(struct rcu_head* p)
{
	kfree(container_of(p, struct watch64, rcuhead));
}

/*
 * Find watch64 structure with RCU lock
 */

struct watch64* watch64_find(unsigned long* ptr)
{
	struct watch64* tmp;

	rcu_read_lock();
	tmp = __watch64_find(ptr);
	rcu_read_unlock();

	return tmp;
}

/*
 * Find watch64 structure without RCU lock
 */

inline struct watch64* __watch64_find(unsigned long* ptr)
{
	struct list_head* tmp;
	struct watch64* watch64_struct;

	__list_for_each_rcu(tmp, &watch64_head) {
		watch64_struct = list_entry(tmp, struct watch64, list);
		if (watch64_struct->ptr == ptr)
			return watch64_struct;
	}

	return NULL;
}

/*
 * Disable a variable watch with RCU lock
 */

int watch64_disable(unsigned long* ptr, struct watch64* st)
{
	int tmp;

	rcu_read_lock();
	tmp = __watch64_disable(ptr,st);
	rcu_read_unlock();

	return tmp;
}

/*
 * Disable a variable watch without RCU lock
 */

inline int __watch64_disable(unsigned long* ptr, struct watch64* st)
{
	if (!st)
		st = watch64_find(ptr);

	if (!st)
		return -EINVAL;

	st->active = 0;

	return 0;
}

inline int __watch64_write(unsigned long* ptr, struct watch64* st, int enable_flag)
{
	if (!st)
		st = __watch64_find(ptr);

	if (!st)
		return -EINVAL;

	st->oldval = *ptr;
	write_seqlock(&st->lock);
	st->total = (u_int64_t) st->oldval;
	write_sequnlock(&st->lock);

	if (enable_flag)
		st->active = 1;

	return 0;
}

/*
 * Reset a variable watch with RCU lock
 */

int watch64_reset(unsigned long* ptr, struct watch64* st)
{
	int tmp;

	rcu_read_lock();
	tmp = __watch64_reset(ptr, st);
	rcu_read_unlock();

	return tmp;
}

/*
 * Reset a variable watch without RCU lock
 */

inline int __watch64_reset(unsigned long* ptr, struct watch64* st)
{
	return __watch64_write(ptr, st, 0);
}

/*
 * Enable a variable watch with RCU lock
 */

int watch64_enable(unsigned long* ptr, struct watch64* st)
{
	int tmp;

	rcu_read_lock();
	tmp = __watch64_enable(ptr,st);
	rcu_read_unlock();

	return tmp;
}

/*
 * Enable a variable watch without RCU lock
 */

inline int __watch64_enable(unsigned long* ptr, struct watch64* st)
{
	return __watch64_write(ptr, st, 1);
}

/*
 * Toggle a variable watch
 */

int watch64_toggle(unsigned long* ptr, struct watch64* st)
{
	rcu_read_lock();
	if (!st)
		st = __watch64_find(ptr);

	if (!st) {
		rcu_read_unlock();
		return -EINVAL;
	}

	if (st->active)
		__watch64_disable(ptr,st);
	else
		__watch64_enable(ptr,st);

	rcu_read_unlock();

	return 0;
}

/*
 * Return the total 64-bit value
 */

u_int64_t watch64_getval(unsigned long* ptr, struct watch64* st)
{
	unsigned int seq;
	u_int64_t total;

	rcu_read_lock();

	if (!st)
		st = __watch64_find(ptr);

	if (!st) {
		rcu_read_unlock();
		return *ptr;
	}

	do {
		seq = read_seqbegin(&st->lock);
		total = st->total;
	} while (read_seqretry(&st->lock, seq));

	rcu_read_unlock();

	return total;
}

#endif /* (BITS_PER_LONG == 64) */

/*
 * Export all the necessary symbols
 */

EXPORT_SYMBOL(watch64_register);
EXPORT_SYMBOL(watch64_unregister);
EXPORT_SYMBOL(watch64_find);
EXPORT_SYMBOL(watch64_disable);
EXPORT_SYMBOL(watch64_reset);
EXPORT_SYMBOL(watch64_enable);
EXPORT_SYMBOL(watch64_toggle);
EXPORT_SYMBOL(watch64_getval);

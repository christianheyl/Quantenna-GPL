/*
 * inclue/linux/watch64.h
 *
 * Copyright (C) 2003 Josef "Jeff" Sipek <jeffpc@xxxxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LINUX_64WATCH_H
#define _LINUX_64WATCH_H

#include <linux/list.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/seqlock.h>
#include <linux/rcupdate.h>

#if 0
#define WATCH64_INTERVAL (HZ/10)
#define WATCH64_MINIMUM (HZ/20)
#else
/*
 * For Quantenna case, take average rate 1Gbps for example,
 * it takes about 30seconds for the rx_bytes and tx_bytes exceeds 4GB.
 * Set interval to 5seconds to save CPU resource.
 */
#define WATCH64_INTERVAL (5 * HZ)
#define WATCH64_MINIMUM (5 * HZ)
#endif
#define WATCH64_MAGIC 0x573634

#if (BITS_PER_LONG == 64)

struct watch64 {
};

#else

struct watch64 {
	struct list_head list;
	unsigned long *ptr;
	unsigned long oldval;
	u_int64_t total;
	unsigned int interval;
	int active;
	seqlock_t lock;
	struct rcu_head rcuhead;
};

#endif /* (BITS_PER_LONG == 64) */

/*
 * Prototypes
 */

void watch64_init(void);
void watch64_run(unsigned long var);
int watch64_register(unsigned long* ptr, unsigned int interval);
int watch64_unregister(unsigned long* ptr, struct watch64* st);
void watch64_rcufree(struct rcu_head* p);
struct watch64* watch64_find(unsigned long* ptr);
inline struct watch64* __watch64_find(unsigned long* ptr);
int watch64_disable(unsigned long* ptr, struct watch64* st);
inline int __watch64_disable(unsigned long* ptr, struct watch64* st);
inline int __watch64_write(unsigned long* ptr, struct watch64* st, int enable_flag);
int watch64_reset(unsigned long* ptr, struct watch64* st);
inline int __watch64_reset(unsigned long* ptr, struct watch64* st);
int watch64_enable(unsigned long* ptr, struct watch64* st);
inline int __watch64_enable(unsigned long* ptr, struct watch64* st);
int watch64_toggle(unsigned long* ptr, struct watch64* st);
u_int64_t watch64_getval(unsigned long* ptr, struct watch64* st);

#endif /* _LINUX_WATCH64_H */

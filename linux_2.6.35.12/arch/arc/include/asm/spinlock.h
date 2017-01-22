/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/rwlock.h>
#include <asm/spinlock_types.h>
#include <asm/processor.h>

/*
 * SMP spinlocks, allowing only a single CPU anywhere
 * (the type definitions are in asm/spinlock_types.h)
 */


#define __raw_spin_is_locked(x)		((x)->slock == RAW_SPIN_LOCK_LOCKED)

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
	unsigned int tmp = RAW_SPIN_LOCK_LOCKED;

	__asm__ __volatile__(
						"1: ex	%0, [%1]\n\t"
						   "cmp %0, %2\n\t"
						   "beq 1b\n\t"
						:
						:"r"(tmp), "m"(lock->slock),"i"(RAW_SPIN_LOCK_LOCKED)
						);
}


/*
 * It is easier for the lock validator if interrupts are not re-enabled
 * in the middle of a lock-acquire. This is a performance feature anyway
 * so we turn it off:
 */

#define __raw_spin_lock_flags(lock, flags)	__raw_spin_lock(lock)


static inline int __raw_spin_trylock(raw_spinlock_t *lock)
{
	unsigned int tmp = RAW_SPIN_LOCK_LOCKED;

	__asm__ __volatile__("ex	%0, [%1]"
						:"=r"(tmp)
						:"m"(lock->slock),"0"(tmp)
						);

	return (tmp == RAW_SPIN_LOCK_UNLOCKED);
}

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
	__asm__ __volatile__("nop_s \n\t"
                         "st	%1, [%0]"
						: /* No output */
						:"r"(&(lock->slock)), "i"(RAW_SPIN_LOCK_UNLOCKED)
						);
}

static inline void __raw_spin_unlock_wait(raw_spinlock_t *lock)
{
	while (__raw_spin_is_locked(lock))
		cpu_relax();
}




/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 */

/* read_can_lock - would read_trylock() succeed? */
#define __raw_read_can_lock(x)		((x)->lock > 0)

/* write_can_lock - would write_trylock() succeed? */
#define __raw_write_can_lock(x)		((x)->lock == RW_LOCK_BIAS)

static inline int __raw_read_trylock(raw_rwlock_t *rw)
{
	__raw_spin_lock (&(rw->lock_mutex));
	if (rw->lock > 0) {
		rw->lock--;
		__raw_spin_unlock (&(rw->lock_mutex));
		return 1;
	}
	__raw_spin_unlock (&(rw->lock_mutex));

	return 0;
}

static inline int __raw_write_trylock(raw_rwlock_t *rw)
{
	__raw_spin_lock (&(rw->lock_mutex));
	if (rw->lock == RW_LOCK_BIAS) {
		rw->lock = 0;
		__raw_spin_unlock (&(rw->lock_mutex));
		return 1;
	}
	__raw_spin_unlock (&(rw->lock_mutex));

	return 0;
}

static inline void __raw_read_lock(raw_rwlock_t *rw)
{
	while(!__raw_read_trylock(rw))
		cpu_relax();
}

static inline void __raw_write_lock(raw_rwlock_t *rw)
{
	while(!__raw_write_trylock(rw))
		cpu_relax();
}

static inline void __raw_read_unlock(raw_rwlock_t *rw)
{
	__raw_spin_lock (&(rw->lock_mutex));
	rw->lock++;
	__raw_spin_unlock (&(rw->lock_mutex));
}

static inline void __raw_write_unlock(raw_rwlock_t *rw)
{
	__raw_spin_lock (&(rw->lock_mutex));
	rw->lock = RW_LOCK_BIAS;
	__raw_spin_unlock (&(rw->lock_mutex));
}

#define _raw_spin_relax(lock)	cpu_relax()
#define _raw_read_relax(lock)	cpu_relax()
#define _raw_write_relax(lock)	cpu_relax()

#endif /* __ASM_SPINLOCK_H */

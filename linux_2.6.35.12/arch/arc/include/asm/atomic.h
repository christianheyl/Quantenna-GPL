/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_ATOMIC_H
#define _ASM_ARC_ATOMIC_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/irqflags.h>

#define ATOMIC_INIT(i)  { (i) }

#if defined(__KERNEL__) && !defined(__ASSEMBLY__)

#ifdef CONFIG_SMP

#include <linux/spinlock_types.h>

extern spinlock_t smp_atomic_ops_lock;
extern unsigned long    _spin_lock_irqsave(spinlock_t *lock);
extern void _spin_unlock_irqrestore(spinlock_t *lock, unsigned long);

#define atomic_ops_lock(flags)   flags = _spin_lock_irqsave(&smp_atomic_ops_lock)
#define atomic_ops_unlock(flags) _spin_unlock_irqrestore(&smp_atomic_ops_lock, flags)

static inline void atomic_set(atomic_t *v, int i)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter = i;
    atomic_ops_unlock(flags);
}
#else

#define atomic_ops_lock(flags)      local_irq_save(flags)
#define atomic_ops_unlock(flags)    local_irq_restore(flags)

#define atomic_set(v,i) (((v)->counter) = (i))

#endif

#define atomic_read(v)  ((v)->counter)

#if defined(CONFIG_ARC_HAS_LLSC)

static inline void atomic_add(int i, atomic_t *v)
{
    unsigned int temp;

    __asm__ __volatile__(
    "1: llock   %0, [%1]    \n"
    "   add     %0, %0, %2  \n"
    "   scond   %0, [%1]    \n"
    "   bnz     1b          \n"
    : "=&r" (temp)       /* Early clobber, to prevent reg reuse */
    : "r" (&v->counter),  "ir" (i)
    : "cc");
}

static inline void atomic_sub(int i, atomic_t *v)
{
    unsigned int temp;

    __asm__ __volatile__(
    "1: llock   %0, [%1]    \n"
    "   sub     %0, %0, %2  \n"
    "   scond   %0, [%1]    \n"
    "   bnz     1b          \n"
    : "=&r" (temp)
    : "r" (&v->counter),  "ir" (i)
    : "cc");
}

/* add and also return the new value */
static inline int atomic_add_return(int i, atomic_t *v)
{
    unsigned int temp;

    __asm__ __volatile__(
    "1: llock   %0, [%1]    \n"
    "   add     %0, %0, %2  \n"
    "   scond   %0, [%1]    \n"
    "   bnz     1b          \n"
    : "=&r" (temp)
    : "r" (&v->counter),  "ir" (i)
    : "cc");

    return temp;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
    unsigned int temp;

    __asm__ __volatile__(
    "1: llock   %0, [%1]    \n"
    "   sub     %0, %0, %2  \n"
    "   scond   %0, [%1]    \n"
    "   bnz     1b          \n"
    : "=&r" (temp)
    : "r" (&v->counter),  "ir" (i)
    : "cc");

    return temp;
}

static inline void atomic_clear_mask(unsigned long mask, unsigned long *addr)
{
    unsigned int temp;

    __asm__ __volatile__(
    "1: llock   %0, [%1]    \n"
    "   bic     %0, %0, %2  \n"
    "   scond   %0, [%1]    \n"
    "   bnz     1b          \n"
    : "=&r" (temp)
    : "r" (addr),  "ir" (mask)
    : "cc");
}

static inline unsigned long cmpxchg(volatile int *p, int expected, int new)
{
    unsigned long prev;

    __asm__ __volatile__(
    "1: llock   %0, [%1]    \n"
    "   brne    %0, %2, 2f  \n"
    "   scond   %3, [%1]    \n"
    "   bnz     1b          \n"
    "2:                     \n"
    : "=&r" (prev)
    : "r" (p),  "ir" (expected),
      "r" (new)  /* this can't be "ir"; scond can't take limm as arg "b" */
    : "cc");

    return prev;
}

#else

static inline void atomic_add(int i, atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter += i;
    atomic_ops_unlock(flags);
}

static inline void atomic_sub(int i, atomic_t *v)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    v->counter -= i;
    atomic_ops_unlock(flags);
}

static inline int atomic_add_return(int i, atomic_t *v)
{
    unsigned long flags ;
    unsigned long temp;

    atomic_ops_lock(flags);
    temp = v->counter;
    temp += i;
    v->counter = temp;
    atomic_ops_unlock(flags);

    return temp;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
    unsigned long flags;
    unsigned long temp;

    atomic_ops_lock(flags);
    temp = v->counter;
    temp -= i;
    v->counter = temp;
    atomic_ops_unlock(flags);

    return temp;
}

static inline void atomic_clear_mask(unsigned long mask, unsigned long *addr)
{
    unsigned long flags;

    atomic_ops_lock(flags);
    *addr &= ~mask;
    atomic_ops_unlock(flags);
}

/* unlike other APIS, cmpxchg is same as atomix_cmpxchg because
 * because the sematics of cmpxchg itself is to be atomic
 */
static inline unsigned long cmpxchg(volatile int *p, int expected, int new)
{
    unsigned long flags;
    int prev;

    atomic_ops_lock(flags);
    if ((prev = *p) == expected)
        *p = new;
    atomic_ops_unlock(flags);
    return(prev);
}

#endif  /* CONFIG_ARC_HAS_LLSC */

#define atomic_cmpxchg(v, o, n) ((int)cmpxchg(&((v)->counter), (o), (n)))
#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

/**
 * atomic_add_unless - add unless the number is a given value
 * @v: pointer of type atomic_t
 * @a: the amount to add to v...
 * @u: ...unless v is equal to u.
 *
 * Atomically adds @a to @v, so long as it was not @u.
 * Returns non-zero if @v was not @u, and zero otherwise.
 */
#define atomic_add_unless(v, a, u)              \
({                              \
    int c, old;                     \
    c = atomic_read(v);                 \
    while (c != (u) && (old = atomic_cmpxchg((v), c, c + (a))) != c) \
        c = old;                    \
    c != (u);                       \
})

#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

#define atomic_inc(v)		atomic_add(1, v)
#define atomic_dec(v)		atomic_sub(1, v)

#define atomic_inc_and_test(v)  (atomic_add_return(1, v) == 0)
#define atomic_dec_and_test(v)	(atomic_sub_return(1, v) == 0)
#define atomic_inc_return(v)    atomic_add_return(1, (v))
#define atomic_dec_return(v)    atomic_sub_return(1, (v))
#define atomic_sub_and_test(i,v)  (atomic_sub_return(i, v) == 0)

#define atomic_add_negative(i,v) (atomic_add_return(i, v) < 0)

#define smp_mb__before_atomic_dec() barrier()
#define smp_mb__after_atomic_dec()  barrier()
#define smp_mb__before_atomic_inc() barrier()
#define smp_mb__after_atomic_inc()  barrier()

#include <asm-generic/atomic-long.h>
#endif
#endif

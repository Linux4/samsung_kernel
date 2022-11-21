/*
 * Queue spinlock
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * (C) Copyright 2013-2014 Hewlett-Packard Development Company, L.P.
 *
 * Authors: Waiman Long <waiman.long@hp.com>
 */
#ifndef __ASM_GENERIC_QSPINLOCK_H
#define __ASM_GENERIC_QSPINLOCK_H

#include <asm-generic/qspinlock_types.h>

/**
 * queue_spin_is_locked - is the spinlock locked?
 * @lock: Pointer to queue spinlock structure
 * Return: 1 if it is locked, 0 otherwise
 */
static __always_inline int queue_spin_is_locked(struct qspinlock *lock)
{
	return atomic_read(&lock->val);
}

/**
 * queue_spin_value_unlocked - is the spinlock structure unlocked?
 * @lock: queue spinlock structure
 * Return: 1 if it is unlocked, 0 otherwise
 *
 * N.B. Whenever there are tasks waiting for the lock, it is considered
 *      locked wrt the lockref code to avoid lock stealing by the lockref
 *      code and change things underneath the lock. This also allows some
 *      optimizations to be applied without conflict with lockref.
 */
static __always_inline int queue_spin_value_unlocked(struct qspinlock lock)
{
	return !atomic_read(&lock.val);
}

/**
 * queue_spin_is_contended - check if the lock is contended
 * @lock : Pointer to queue spinlock structure
 * Return: 1 if lock contended, 0 otherwise
 */
static __always_inline int queue_spin_is_contended(struct qspinlock *lock)
{
	return atomic_read(&lock->val) & ~_Q_LOCKED_MASK;
}
/**
 * queue_spin_trylock - try to acquire the queue spinlock
 * @lock : Pointer to queue spinlock structure
 * Return: 1 if lock acquired, 0 if failed
 */
static __always_inline int queue_spin_trylock(struct qspinlock *lock)
{
	if (!atomic_read(&lock->val) &&
	   (atomic_cmpxchg(&lock->val, 0, _Q_LOCKED_VAL) == 0))
		return 1;
	return 0;
}

#ifndef queue_spin_lock
extern void queue_spin_lock_slowpath(struct qspinlock *lock, u32 val);

/**
 * queue_spin_lock - acquire a queue spinlock
 * @lock: Pointer to queue spinlock structure
 */
static __always_inline void queue_spin_lock(struct qspinlock *lock)
{
	u32 val;

	val = atomic_cmpxchg(&lock->val, 0, _Q_LOCKED_VAL);
	if (likely(val == 0)) {
	/* 
	 * BluesMan: The lock was not taken previously,
	 * we won the fast path. Return peacefully.
	 */
		return;
	}
	else {
		/* 
		 * BluesMan: Ok, there was someone ahead, lets fall back to slowpath.
		 */
		queue_spin_lock_slowpath(lock, val);
	}
}
#endif

#ifndef queue_spin_unlock
/**
 * queue_spin_unlock - release a queue spinlock
 * @lock : Pointer to queue spinlock structure
 */
static __always_inline void queue_spin_unlock(struct qspinlock *lock)
{
	/*
	 * smp_mb__before_atomic() in order to guarantee release semantics
	 */
	smp_mb__before_atomic_dec();
	atomic_sub(_Q_LOCKED_VAL, &lock->val);
}
#endif

/*
 * Initializier
 */
#define	__ARCH_SPIN_LOCK_UNLOCKED	{ ATOMIC_INIT(0) }

/*
 * Remapping spinlock architecture specific functions to the corresponding
 * queue spinlock functions.
 */
#define arch_spin_is_locked(l)		queue_spin_is_locked(l)
#define arch_spin_is_contended(l)	queue_spin_is_contended(l)
#define arch_spin_value_unlocked(l)	queue_spin_value_unlocked(l)
#define arch_spin_lock(l)		queue_spin_lock(l)
#define arch_spin_trylock(l)		queue_spin_trylock(l)
#define arch_spin_unlock(l)		queue_spin_unlock(l)
#define arch_spin_lock_flags(l, f)	queue_spin_lock(l)

#endif /* __ASM_GENERIC_QSPINLOCK_H */

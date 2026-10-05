/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file lifetime_lock.h
 *
 * @brief Implementation of a lifetime lock. (Inspired by XROS lifetime lock)
 *
 * @details
 *
 *******************************************************************************/

#ifndef LIFETIME_LOCK_H
#define LIFETIME_LOCK_H

#include <linux/wait.h>

#include <ar_atomics.h>
#include <ar_common.h>

/**
 * This lock is used to help manage the lifetime of an object that asynchronously
 * has to be created, have work done, and be destroyed.
 *
 * During construction the object should acquire the construction lock
 * in order to ensure the constructor runs exactly once, and before any work
 * is done.
 *
 * Async operations on the object should try to obtain a worker lock
 * via lifetime_acquire. If the lock is acquired the worker may proceed.
 * If lock acquisition fails with -ESHUTDOWN the worker should abort
 * since the object has already been closed.
 *
 * The close operation (commonly called a destructor) must first
 * acquire the destructor lock via lifetime_acquire_for_destruction.
 * This will wait until any in-progress async operations are complete.
 *
 * The destruction lock can be acquired at most once. If the lock
 * cannot be acquired then the object has already been closed and so
 * the closing function should simply exit.
 */
struct lifetime_lock {
	// number of workers outstanding on this lock
	ar_atomic_t workers;

	// waitqueue that threads attempting to lock for destruction will wait on
	wait_queue_head_t wq;

	// object state
	ar_atomic_t state;

	// whether this object is shutting down.
	ar_atomic_t shutting_down;
};

// state flags
#define _LL_CONSTRUCTING 1
#define _LL_CONSTRUCTED 2
#define _LL_TORN_DOWN 4

static inline int lifetime_lock_is_constructing(struct lifetime_lock *lock)
{
	return ar_atomic_load(&lock->state, AR_MEMORY_ORDER_ACQUIRE) &
	       _LL_CONSTRUCTING;
}

static inline int lifetime_lock_is_constructed(struct lifetime_lock *lock)
{
	return ar_atomic_load(&lock->state, AR_MEMORY_ORDER_ACQUIRE) &
	       _LL_CONSTRUCTED;
}

static inline int lifetime_lock_is_shutting_down(struct lifetime_lock *lock)
{
	return ar_atomic_load(&lock->shutting_down, AR_MEMORY_ORDER_ACQUIRE);
}

static inline int lifetime_lock_is_torn_down(struct lifetime_lock *lock)
{
	return ar_atomic_load(&lock->state, AR_MEMORY_ORDER_ACQUIRE) &
	       _LL_TORN_DOWN;
}

static inline int lifetime_lock_has_workers(struct lifetime_lock *lock)
{
	return ar_atomic_load(&lock->workers, AR_MEMORY_ORDER_ACQUIRE) > 0;
}

/**
 * Initialize a lifetime lock.
 */
static inline void lifetime_lock_init(struct lifetime_lock *lock)
{
	init_waitqueue_head(&lock->wq);
	ar_atomic_store(&lock->workers, 0, AR_MEMORY_ORDER_RELEASE);
	ar_atomic_store(&lock->state, 0, AR_MEMORY_ORDER_RELEASE);
	ar_atomic_store(&lock->shutting_down, 0, AR_MEMORY_ORDER_RELEASE);
}

/**
 * Acquire a lifetime lock for work. Error if lock not constructed.
 */
static inline int lifetime_lock_acquire(struct lifetime_lock *lock)
{
	int workers;

	if (AR_UNLIKELY(!lifetime_lock_is_constructed(lock)))
		return -EINVAL;

	/**
	 * Pre-check to avoid extra atomic inc/dec on workers.
	 * Cannot assert on torn down here - just check for now.
	 */
	if (lifetime_lock_is_shutting_down(lock) ||
	    lifetime_lock_is_torn_down(lock))
		return -ESHUTDOWN;

	/**
	 * There is a race here so we can end up increasing this but still getting
	 * this lock in shutdown mode anyways.
	 */
	ar_atomic_fetch_add(&lock->workers, 1, AR_MEMORY_ORDER_ACQ_REL);

	/**
	 * Check if we ended up with a shutdown lock by chance. Unlikely but possible.
	 * If so we need to revert the worker bump and get out. If we were the last
	 * worker we also need to wake up the destruction thread we were racing with.
	 */
	if (AR_UNLIKELY(lifetime_lock_is_shutting_down(lock))) {
		workers = ar_atomic_fetch_add(&lock->workers, -1,
					      AR_MEMORY_ORDER_ACQ_REL);
		if (workers == 1)
			wake_up_all(&lock->wq);
		return -ESHUTDOWN;
	}

	return 0;
}

/**
 * Acquire a lifetime lock for work, sleep until construction.
 */
static inline int lifetime_lock_acquire_wait(struct lifetime_lock *lock)
{
	// wait until the lock is constructed.
	if (wait_event_interruptible(lock->wq,
				     lifetime_lock_is_constructed(lock)))
		return -EINTR;

	return lifetime_lock_acquire(lock);
}

/**
 * Release a lifetime lock previously acquired for work.
 */
static inline void lifetime_lock_release(struct lifetime_lock *lock)
{
	int workers;

	AR_ASSERT(lifetime_lock_is_constructed(lock));
	AR_ASSERT(!lifetime_lock_is_torn_down(lock));
	workers = ar_atomic_fetch_add(&lock->workers, -1,
				      AR_MEMORY_ORDER_RELEASE);
	AR_ASSERT(workers > 0);

	// if we are the only one to release, wake up threads on the waitqueue.
	if (workers == 1)
		wake_up_all(&lock->wq);
}

/**
 * Acquire a lifetime lock for construction. Used to lock out destruction until
 * construction completes, and to ensure construction only happens once.
 *
 * Release the lock with lifetime_lock_construction_release
 */
static inline int lifetime_lock_construction_acquire(struct lifetime_lock *lock)
{
	int new_lock_state = 0;
	int __maybe_unused active_workers;

	if (!ar_atomic_compare_exchange(
		    &lock->state, &new_lock_state, _LL_CONSTRUCTING,
		    AR_MEMORY_ORDER_ACQ_REL, AR_MEMORY_ORDER_ACQUIRE))
		return -EINVAL;

	AR_ASSERT(!lifetime_lock_is_shutting_down(lock));
	AR_ASSERT(!lifetime_lock_is_torn_down(lock));

	// assert 0 active workers, and record that we are actively holding the lock
	active_workers =
		ar_atomic_fetch_add(&lock->workers, 1, AR_MEMORY_ORDER_ACQUIRE);
	AR_ASSERT(active_workers == 0);

	return 0;
}

/**
 * Release a lifetime lock previously acquired for construction, and mark the object as constructed.
 */
static inline void
lifetime_lock_construction_release(struct lifetime_lock *lock)
{
	AR_ASSERT(ar_atomic_load(&lock->state, AR_MEMORY_ORDER_ACQUIRE) ==
		  _LL_CONSTRUCTING);
	AR_ASSERT(ar_atomic_load(&lock->workers, AR_MEMORY_ORDER_ACQUIRE) == 1);

	ar_atomic_store(&lock->state, _LL_CONSTRUCTED, AR_MEMORY_ORDER_RELEASE);

	// this will wake up any threads waiting to acquire the lifetime lock since workers == 1
	lifetime_lock_release(lock);
}

/**
 * Release a lifetime lock previously acquired for construction, but do not complete
 * construction.
 */
static inline void
lifetime_lock_construction_release_err(struct lifetime_lock *lock)
{
	int workers;

	AR_ASSERT(!lifetime_lock_is_shutting_down(lock));
	AR_ASSERT(ar_atomic_load(&lock->state, AR_MEMORY_ORDER_ACQUIRE) ==
		  _LL_CONSTRUCTING);
	AR_ASSERT(ar_atomic_load(&lock->workers, AR_MEMORY_ORDER_ACQUIRE) == 1);

	// on error, we reset the lock state to initial state
	ar_atomic_store(&lock->state, 0, AR_MEMORY_ORDER_RELEASE);

	// we can't call the lifetime_lock_release as it asserts the lifetime lock is constructed
	workers = ar_atomic_fetch_add(&lock->workers, -1,
				      AR_MEMORY_ORDER_RELEASE);
	AR_ASSERT(workers > 0);

	// if we are the only one to release, wake up threads on the waitqueue.
	if (workers == 1)
		wake_up_all(&lock->wq);
}

/**
 * Trigger shutdown on a lifetime lock. Does not acquire any locks, so this can
 * safely be used to trigger a shutdown from any context.
 */
static inline void lifetime_lock_trigger_shutdown(struct lifetime_lock *lock)
{
	ar_atomic_store(&lock->shutting_down, 1, AR_MEMORY_ORDER_RELEASE);
}

/**
 * Acquire a lock for destruction work. This flags the lock as shutting down, then
 * grabs the lock. After grabbing the lock, no further acquires of this lock should
 * succeed: no more work can be done, and each lock can be acquired for destruction
 * exactly once.
 */
static inline int lifetime_lock_destruction_acquire(struct lifetime_lock *lock)
{
	int lock_state = _LL_CONSTRUCTED;

	// set the shutdown atomic, as we are triggering shutdown before teardown
	lifetime_lock_trigger_shutdown(lock);

	/**
	 * Wait for worker count to drop to zero. This assumes:
	 * 1. Shutdown was triggered before worker count is read
	 * 2. Worker count is monotonically decreasing after shutdown
	 * 2. Worker count is decremented before threads are woken on the waitqueue
	 */
	wait_event(lock->wq, ar_atomic_load(&lock->workers,
					    AR_MEMORY_ORDER_ACQUIRE) == 0);

	/**
	 * Attempt to move the lock to a torn down state, from the constructed state.
	 * If this fails, it means either the lock was already torn down, or that the
	 * lock was not constructed yet.
	 */
	if (!ar_atomic_compare_exchange(
		    &lock->state, &lock_state, _LL_CONSTRUCTED | _LL_TORN_DOWN,
		    AR_MEMORY_ORDER_ACQ_REL, AR_MEMORY_ORDER_ACQUIRE)) {
		if (lock_state & _LL_TORN_DOWN)
			return -ESHUTDOWN;
		else if (lock_state != _LL_CONSTRUCTED)
			return -EINVAL;
	}

	return 0;
}

/**
 * Release a work lock previously acquired for destruction work.
 */
static inline void lifetime_lock_destruction_release(struct lifetime_lock *lock)
{
	AR_ASSERT(lifetime_lock_is_shutting_down(lock));
	AR_ASSERT(lifetime_lock_is_constructed(lock));
	AR_ASSERT(lifetime_lock_is_torn_down(lock));
}

#endif // !LIFETIME_LOCK_H

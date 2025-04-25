/*
 * Copyright (c) 2014-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: i_qdf_lock.h
 * Linux-specific definitions for QDF Lock API's
 */

#if !defined(__I_QDF_LOCK_H)
#define __I_QDF_LOCK_H

/* Include Files */
#include <qdf_types.h>
#include <qdf_status.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/device.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>

/* define for flag */
#define QDF_LINUX_UNLOCK_BH  1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum {
	LOCK_RELEASED = 0x11223344,
	LOCK_ACQUIRED,
	LOCK_DESTROYED
};

/**
 * struct qdf_lock_s - mutex abstraction
 * @m_lock: Mutex lock
 * @cookie: Lock cookie
 * @process_id: Process ID to track lock
 * @state: Lock status
 * @refcount: Reference count for recursive lock
 * @stats: a structure that contains usage statistics
 */
struct qdf_lock_s {
	struct mutex m_lock;
	uint32_t cookie;
	int process_id;
	uint32_t state;
	uint8_t refcount;
	struct lock_stats stats;
};

/**
 * typedef __qdf_mutex_t - Mutex abstraction
 */
typedef struct qdf_lock_s __qdf_mutex_t;

/**
 * typedef __qdf_spinlock_t - spinlock abstraction
 * @spinlock: Spin lock
 * @flags: Lock flag
 */
typedef struct __qdf_spinlock {
	spinlock_t spinlock;
	unsigned long flags;
} __qdf_spinlock_t;

/**
 * typedef __qdf_semaphore_t - semaphore abstraction
 */
typedef struct semaphore __qdf_semaphore_t;

/**
 * typedef qdf_wake_lock_t - wakelock abstraction
 * @lock: this lock needs to be used in kernel version < 5.4
 * @priv: this lock pointer needs to be used in kernel version >= 5.4
 */
typedef struct qdf_wake_lock {
	struct wakeup_source lock;
	struct wakeup_source *priv;
} qdf_wake_lock_t;

struct hif_pm_runtime_lock;
typedef struct qdf_runtime_lock {
	struct hif_pm_runtime_lock *lock;
} qdf_runtime_lock_t;

typedef struct rcu_head __qdf_rcu_head_t;

#define __qdf_rcu_callback_t rcu_callback_t
#define __qdf_rcu_dereference(p) rcu_dereference(p)
#define __qdf_rcu_dereference_protected(p, c) rcu_dereference_protected(p, c)
#define __qdf_rcu_assign_pointer(p, v) rcu_assign_pointer(p, v)

static inline bool __qdf_lockdep_is_held(__qdf_spinlock_t *lock)
{
#ifdef CONFIG_LOCKDEP
	return lockdep_is_held(&lock->spinlock);
#else
	return true;
#endif
}

#define LINUX_LOCK_COOKIE 0x12345678

/* Function declarations and documentation */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
/**
 * __qdf_semaphore_init() - initialize the semaphore
 * @m: Semaphore object
 *
 * Return: QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS __qdf_semaphore_init(struct semaphore *m)
{
	init_MUTEX(m);
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS __qdf_semaphore_init(struct semaphore *m)
{
	sema_init(m, 1);
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * __qdf_semaphore_acquire() - acquire semaphore
 * @m: Semaphore object
 *
 * Return: 0
 */
static inline int __qdf_semaphore_acquire(struct semaphore *m)
{
	down(m);
	return 0;
}

/**
 * __qdf_semaphore_acquire_intr() - Take the semaphore, interruptible
 * @m: Semaphore object
 *
 * This function allows a user-space process that is waiting on a
 * semaphore to be interrupted by the user.  If the operation is
 * interrupted, the function returns a nonzero value, and the caller
 * does not hold the semaphore.  Always check the return value and
 * responding accordingly.
 *
 * Return: 0 if the semaphore was acquired, non-zero if not acquired
 */
static inline int __qdf_semaphore_acquire_intr(struct semaphore *m)
{
	return down_interruptible(m);
}

/**
 * __qdf_semaphore_release() - release semaphore
 * @m: Semaphore object
 *
 * Return: result of UP operation in integer
 */
static inline void __qdf_semaphore_release(struct semaphore *m)
{
	up(m);
}

/**
 * __qdf_semaphore_acquire_timeout() - Take the semaphore before timeout
 * @m: semaphore to take
 * @timeout: maximum time to try to take the semaphore
 *
 * Return: int
 */
static inline int __qdf_semaphore_acquire_timeout(struct semaphore *m,
						  unsigned long timeout)
{
	unsigned long jiffie_val = msecs_to_jiffies(timeout);

	return down_timeout(m, jiffie_val);
}

/**
 * __qdf_spinlock_create() - initialize spin lock
 * @lock: Spin lock object
 *
 * Return: QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS __qdf_spinlock_create(__qdf_spinlock_t *lock)
{
	spin_lock_init(&lock->spinlock);
	lock->flags = 0;
	return QDF_STATUS_SUCCESS;
}

#define __qdf_spinlock_destroy(lock)

/**
 * __qdf_spin_lock() - Acquire a Spinlock(SMP) & disable Preemption (Preemptive)
 * @lock: Lock object
 *
 * Return: none
 */
static inline void __qdf_spin_lock(__qdf_spinlock_t *lock)
{
	spin_lock(&lock->spinlock);
}

/**
 * __qdf_spin_unlock() - Unlock the spinlock and enables the Preemption
 * @lock: Lock object
 *
 * Return: none
 */
static inline void __qdf_spin_unlock(__qdf_spinlock_t *lock)
{
	spin_unlock(&lock->spinlock);
}

/**
 * __qdf_spin_lock_irqsave() - Acquire a Spinlock (SMP) & disable Preemption
 * (Preemptive) and disable IRQs
 * @lock: Lock object
 *
 * Return: none
 */
static inline void __qdf_spin_lock_irqsave(__qdf_spinlock_t *lock)
{
	spin_lock_irqsave(&lock->spinlock, lock->flags);
}

/**
 * __qdf_spin_unlock_irqrestore() - Unlock the spinlock and enables the
 * Preemption and enable IRQ
 * @lock: Lock object
 *
 * Return: none
 */
static inline void __qdf_spin_unlock_irqrestore(__qdf_spinlock_t *lock)
{
	spin_unlock_irqrestore(&lock->spinlock, lock->flags);
}

/*
 * Synchronous versions - only for OS' that have interrupt disable
 */
#define __qdf_spin_lock_irq(_p_lock, _flags) spin_lock_irqsave(_p_lock, _flags)
#define __qdf_spin_unlock_irq(_p_lock, _flags) \
	spin_unlock_irqrestore(_p_lock, _flags)

/**
 * __qdf_spin_is_locked() - Test if spinlock is locked
 * @lock: spinlock object
 *
 * Return: nonzero if lock is held.
 */
static inline int __qdf_spin_is_locked(__qdf_spinlock_t *lock)
{
	return spin_is_locked(&lock->spinlock);
}

/**
 * __qdf_spin_trylock_bh() - spin trylock bottomhalf
 * @lock: spinlock object
 *
 * Return: nonzero if lock is acquired
 */
static inline int __qdf_spin_trylock_bh(__qdf_spinlock_t *lock)
{
	if (likely(irqs_disabled() || in_irq() || in_softirq()))
		return spin_trylock(&lock->spinlock);

	if (spin_trylock_bh(&lock->spinlock)) {
		lock->flags |= QDF_LINUX_UNLOCK_BH;
		return 1;
	}

	return 0;
}

/**
 * __qdf_spin_trylock() - spin trylock
 * @lock: spinlock object
 *
 * Return: int
 */
static inline int __qdf_spin_trylock(__qdf_spinlock_t *lock)
{
	return spin_trylock(&lock->spinlock);
}

/**
 * __qdf_spin_lock_bh() - Acquire the spinlock and disable bottom halves
 * @lock: Lock object
 *
 * Return: none
 */
static inline void __qdf_spin_lock_bh(__qdf_spinlock_t *lock)
{
	if (likely(irqs_disabled() || in_irq() || in_softirq())) {
		spin_lock(&lock->spinlock);
	} else {
		spin_lock_bh(&lock->spinlock);
		lock->flags |= QDF_LINUX_UNLOCK_BH;
	}
}

/**
 * __qdf_spin_unlock_bh() - Release the spinlock and enable bottom halves
 * @lock: Lock object
 *
 * Return: none
 */
static inline void __qdf_spin_unlock_bh(__qdf_spinlock_t *lock)
{
	if (unlikely(lock->flags & QDF_LINUX_UNLOCK_BH)) {
		lock->flags &= (unsigned long)~QDF_LINUX_UNLOCK_BH;
		spin_unlock_bh(&lock->spinlock);
	} else
		spin_unlock(&lock->spinlock);
}

/**
 * __qdf_spinlock_irq_exec() - Execute the input function with
 *                             spinlock held and interrupt disabled.
 * @hdl: OS handle
 * @lock: spinlock to be held for the critical region
 * @func: critical region function that to be executed
 * @arg: context of the critical region function
 *
 * Return: Boolean status returned by the critical region function
 */
static inline bool __qdf_spinlock_irq_exec(qdf_handle_t hdl,
			__qdf_spinlock_t *lock,
			qdf_irqlocked_func_t func,
			void *arg)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&lock->spinlock, flags);
	ret = func(arg);
	spin_unlock_irqrestore(&lock->spinlock, flags);

	return ret;
}

/**
 * __qdf_rcu_read_lock_bh() - mark the beginning of an RCU-bh critical section
 *
 * Return: none
 */
static inline void __qdf_rcu_read_lock_bh(void)
{
	rcu_read_lock_bh();
}

/**
 * __qdf_rcu_read_unlock_bh() - marks the end of a softirq-only RCU critical
 * section
 *
 * Return: none
 */
static inline void __qdf_rcu_read_unlock_bh(void)
{
	rcu_read_unlock_bh();
}

/**
 * __qdf_call_rcu() - Marks the end of updater code and the beginning of
 * reclaimer code
 * @head: function parameter for func
 * @func: function to be executed after grace period is over
 *
 * Return: none
 */
static inline
void __qdf_call_rcu(__qdf_rcu_head_t *head, __qdf_rcu_callback_t func)
{
	call_rcu(head, func);
}

/**
 * __qdf_in_softirq() - in soft irq context
 *
 * Return: true if in softirs context else false
 */
static inline bool __qdf_in_softirq(void)
{
	return in_softirq();
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __I_QDF_LOCK_H */

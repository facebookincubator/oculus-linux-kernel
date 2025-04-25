/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/module.h>
#include <qdf_lock.h>
#include <qdf_trace.h>
#include <qdf_module.h>

#include <qdf_types.h>
#include <i_host_diag_core_event.h>
#ifdef FEATURE_RUNTIME_PM
#include <cds_api.h>
#include <hif.h>
#endif
#include <i_qdf_lock.h>
#include <linux/suspend.h>

#undef qdf_mutex_create
QDF_STATUS qdf_mutex_create(qdf_mutex_t *lock, const char *func, int line)
{
	/* check for invalid pointer */
	if (!lock) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: NULL pointer passed in", __func__);
		return QDF_STATUS_E_FAULT;
	}
	/* check for 'already initialized' lock */
	if (LINUX_LOCK_COOKIE == lock->cookie) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: already initialized lock", __func__);
		return QDF_STATUS_E_BUSY;
	}

	if (in_interrupt()) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s cannot be called from interrupt context!!!",
			  __func__);
		return QDF_STATUS_E_FAULT;
	}

	qdf_lock_stats_create(&lock->stats, func, line);

	/* initialize new lock */
	mutex_init(&lock->m_lock);
	lock->cookie = LINUX_LOCK_COOKIE;
	lock->state = LOCK_RELEASED;
	lock->process_id = 0;
	lock->refcount = 0;

	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(qdf_mutex_create);

QDF_STATUS qdf_mutex_acquire(qdf_mutex_t *lock)
{
	int rc;
	/* check for invalid pointer */
	if (!lock) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: NULL pointer passed in", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAULT;
	}
	/* check if lock refers to an initialized object */
	if (LINUX_LOCK_COOKIE != lock->cookie) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: uninitialized lock", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (in_interrupt()) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s cannot be called from interrupt context!!!",
			  __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAULT;
	}
	if ((lock->process_id == current->pid) &&
		(lock->state == LOCK_ACQUIRED)) {
		lock->refcount++;
#ifdef QDF_NESTED_LOCK_DEBUG
			pe_err("%s: %x %d %d", __func__, lock, current->pid,
			  lock->refcount);
#endif
		return QDF_STATUS_SUCCESS;
	}

	BEFORE_LOCK(lock, mutex_is_locked(&lock->m_lock));
	/* acquire a Lock */
	mutex_lock(&lock->m_lock);
	AFTER_LOCK(lock, __func__);
	rc = mutex_is_locked(&lock->m_lock);
	if (rc == 0) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: unable to lock mutex (rc = %d)", __func__, rc);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
#ifdef QDF_NESTED_LOCK_DEBUG
		pe_err("%s: %x %d", __func__, lock, current->pid);
#endif
	if (LOCK_DESTROYED != lock->state) {
		lock->process_id = current->pid;
		lock->refcount++;
		lock->state = LOCK_ACQUIRED;
		return QDF_STATUS_SUCCESS;
	}

	/* lock is already destroyed */
	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
		  "%s: Lock is already destroyed", __func__);
	mutex_unlock(&lock->m_lock);
	QDF_ASSERT(0);
	return QDF_STATUS_E_FAILURE;
}
qdf_export_symbol(qdf_mutex_acquire);

QDF_STATUS qdf_mutex_release(qdf_mutex_t *lock)
{
	/* check for invalid pointer */
	if (!lock) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: NULL pointer passed in", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAULT;
	}

	/* check if lock refers to an uninitialized object */
	if (LINUX_LOCK_COOKIE != lock->cookie) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: uninitialized lock", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (in_interrupt()) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s cannot be called from interrupt context!!!",
			  __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAULT;
	}

	/* current_thread = get_current_thread_id();
	 * Check thread ID of caller against thread ID
	 * of the thread which acquire the lock
	 */
	if (lock->process_id != current->pid) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: current task pid does not match original task pid!!",
			  __func__);
#ifdef QDF_NESTED_LOCK_DEBUG
		pe_err("%s: Lock held by=%d being released by=%d",
			  __func__, lock->process_id, current->pid);
#endif
		QDF_ASSERT(0);
		return QDF_STATUS_E_PERM;
	}
	if ((lock->process_id == current->pid) &&
		(lock->state == LOCK_ACQUIRED)) {
		if (lock->refcount > 0)
			lock->refcount--;
	}
#ifdef QDF_NESTED_LOCK_DEBUG
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR, "%s: %x %d %d", __func__, lock, lock->process_id,
		  lock->refcount);
#endif
	if (lock->refcount)
		return QDF_STATUS_SUCCESS;

	lock->process_id = 0;
	lock->refcount = 0;
	lock->state = LOCK_RELEASED;
	/* release a Lock */
	BEFORE_UNLOCK(lock, 0);
	mutex_unlock(&lock->m_lock);
#ifdef QDF_NESTED_LOCK_DEBUG
	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR, "%s: Freeing lock %x %d %d", lock, lock->process_id,
		  lock->refcount);
#endif
	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(qdf_mutex_release);

#ifdef WLAN_WAKE_LOCK_DEBUG
#include "qdf_tracker.h"

#define qdf_wake_lock_tracker_bits 2 /* 4 buckets */
static qdf_tracker_declare(qdf_wake_lock_tracker, qdf_wake_lock_tracker_bits,
			   "wake lock leaks", "wake lock create",
			   "wake lock destroy");

void qdf_wake_lock_feature_init(void)
{
	qdf_tracker_init(&qdf_wake_lock_tracker);
}

void qdf_wake_lock_feature_deinit(void)
{
	qdf_tracker_deinit(&qdf_wake_lock_tracker);
}

void qdf_wake_lock_check_for_leaks(void)
{
	qdf_tracker_check_for_leaks(&qdf_wake_lock_tracker);
}

static inline QDF_STATUS qdf_wake_lock_dbg_track(qdf_wake_lock_t *lock,
						 const char *func,
						 uint32_t line)
{
	return qdf_tracker_track(&qdf_wake_lock_tracker, lock, func, line);
}

static inline void qdf_wake_lock_dbg_untrack(qdf_wake_lock_t *lock,
					     const char *func, uint32_t line)
{
	qdf_tracker_untrack(&qdf_wake_lock_tracker, lock, func, line);
}
#else
static inline QDF_STATUS qdf_wake_lock_dbg_track(qdf_wake_lock_t *lock,
						 const char *func,
						 uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}

static inline void qdf_wake_lock_dbg_untrack(qdf_wake_lock_t *lock,
					     const char *func, uint32_t line)
{ }
#endif /* WLAN_WAKE_LOCK_DEBUG */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
const char *qdf_wake_lock_name(qdf_wake_lock_t *lock)
{
	if (lock)
		return lock->lock.name;
	return "UNNAMED_WAKELOCK";
}
#else
const char *qdf_wake_lock_name(qdf_wake_lock_t *lock)
{
	return "NO_WAKELOCK_SUPPORT";
}
#endif
qdf_export_symbol(qdf_wake_lock_name);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 110)) || \
	defined(WAKEUP_SOURCE_DEV)
QDF_STATUS __qdf_wake_lock_create(qdf_wake_lock_t *lock, const char *name,
				  const char *func, uint32_t line)
{
	QDF_STATUS status;

	status = qdf_wake_lock_dbg_track(lock, func, line);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	qdf_mem_zero(lock, sizeof(*lock));
	lock->priv = wakeup_source_register(lock->lock.dev, name);
	if (!(lock->priv)) {
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	lock->lock = *(lock->priv);

	return QDF_STATUS_SUCCESS;
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
QDF_STATUS __qdf_wake_lock_create(qdf_wake_lock_t *lock, const char *name,
				  const char *func, uint32_t line)
{
	QDF_STATUS status;

	status = qdf_wake_lock_dbg_track(lock, func, line);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wakeup_source_init(&(lock->lock), name);
	lock->priv = &(lock->lock);

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS __qdf_wake_lock_create(qdf_wake_lock_t *lock, const char *name,
				  const char *func, uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}
#endif
qdf_export_symbol(__qdf_wake_lock_create);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
QDF_STATUS qdf_wake_lock_acquire(qdf_wake_lock_t *lock, uint32_t reason)
{
	host_diag_log_wlock(reason, qdf_wake_lock_name(lock),
			    WIFI_POWER_EVENT_DEFAULT_WAKELOCK_TIMEOUT,
			    WIFI_POWER_EVENT_WAKELOCK_TAKEN);
	__pm_stay_awake(lock->priv);

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS qdf_wake_lock_acquire(qdf_wake_lock_t *lock, uint32_t reason)
{
	return QDF_STATUS_SUCCESS;
}
#endif
qdf_export_symbol(qdf_wake_lock_acquire);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
QDF_STATUS qdf_wake_lock_timeout_acquire(qdf_wake_lock_t *lock, uint32_t msec)
{
	pm_wakeup_ws_event(lock->priv, msec, true);
	return QDF_STATUS_SUCCESS;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
QDF_STATUS qdf_wake_lock_timeout_acquire(qdf_wake_lock_t *lock, uint32_t msec)
{
	/* Wakelock for Rx is frequent.
	 * It is reported only during active debug
	 */
	__pm_wakeup_event(&(lock->lock), msec);
	return QDF_STATUS_SUCCESS;
}
#else /* LINUX_VERSION_CODE */
QDF_STATUS qdf_wake_lock_timeout_acquire(qdf_wake_lock_t *lock, uint32_t msec)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* LINUX_VERSION_CODE */
qdf_export_symbol(qdf_wake_lock_timeout_acquire);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
QDF_STATUS qdf_wake_lock_release(qdf_wake_lock_t *lock, uint32_t reason)
{
	host_diag_log_wlock(reason, qdf_wake_lock_name(lock),
			    WIFI_POWER_EVENT_DEFAULT_WAKELOCK_TIMEOUT,
			    WIFI_POWER_EVENT_WAKELOCK_RELEASED);
	__pm_relax(lock->priv);

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS qdf_wake_lock_release(qdf_wake_lock_t *lock, uint32_t reason)
{
	return QDF_STATUS_SUCCESS;
}
#endif
qdf_export_symbol(qdf_wake_lock_release);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 110)) || \
	defined(WAKEUP_SOURCE_DEV)
void __qdf_wake_lock_destroy(qdf_wake_lock_t *lock,
			     const char *func, uint32_t line)
{
	wakeup_source_unregister(lock->priv);
	qdf_wake_lock_dbg_untrack(lock, func, line);
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
void __qdf_wake_lock_destroy(qdf_wake_lock_t *lock,
			     const char *func, uint32_t line)
{
	wakeup_source_trash(&(lock->lock));
	qdf_wake_lock_dbg_untrack(lock, func, line);
}
#else
void __qdf_wake_lock_destroy(qdf_wake_lock_t *lock,
			     const char *func, uint32_t line)
{
}
#endif
qdf_export_symbol(__qdf_wake_lock_destroy);

void qdf_pm_system_wakeup(void)
{
	pm_system_wakeup();
}

qdf_export_symbol(qdf_pm_system_wakeup);

#ifdef FEATURE_RUNTIME_PM
/**
 * qdf_to_hif_convert_rtpm_id() - Convert QDF Runtime PM ID to HIF RTPM ID
 * @id: Client id
 *
 * Return: HIF Runtime pm ID of client
 */
static uint32_t qdf_to_hif_convert_rtpm_id(uint32_t id)
{
	switch (id) {
	case QDF_RTPM_ID_RESERVED:
		return HIF_RTPM_ID_RESERVED;
	case QDF_RTPM_ID_PM_QOS_NOTIFY:
		return HIF_RTPM_ID_PM_QOS_NOTIFY;
	case QDF_RTPM_ID_WIPHY_SUSPEND:
		return HIF_RTPM_ID_WIPHY_SUSPEND;
	default:
		return HIF_RTPM_ID_MAX;
	}
}

/**
 * qdf_to_hif_convert_rtpm_type() - Convert QDF Runtime PM call type to HIF
 *                                 call type
 * @type: call type
 *
 * Return: HIF runtime PM call type
 */
static uint8_t qdf_to_hif_convert_rtpm_type(uint8_t type)
{
	switch (type) {
	case QDF_RTPM_GET:
		return HIF_RTPM_GET_ASYNC;
	case QDF_RTPM_GET_FORCE:
		return HIF_RTPM_GET_FORCE;
	case QDF_RTPM_GET_SYNC:
		return HIF_RTPM_GET_SYNC;
	case QDF_RTPM_GET_NORESUME:
		return HIF_RTPM_GET_NORESUME;
	case QDF_RTPM_PUT:
		return HIF_RTPM_PUT_ASYNC;
	case QDF_RTPM_PUT_SYNC_SUSPEND:
		return HIF_RTPM_PUT_SYNC_SUSPEND;
	case QDF_RTPM_PUT_NOIDLE:
		return HIF_RTPM_PUT_NOIDLE;
	default:
		return QDF_STATUS_E_NOSUPPORT;
	}
}

QDF_STATUS qdf_rtpm_register(uint32_t id, void (*hif_rpm_cbk)(void))
{
	return hif_rtpm_register(qdf_to_hif_convert_rtpm_id(id), hif_rpm_cbk);
}

qdf_export_symbol(qdf_rtpm_register);

QDF_STATUS qdf_rtpm_deregister(uint32_t id)
{
	return hif_rtpm_deregister(qdf_to_hif_convert_rtpm_id(id));
}

qdf_export_symbol(qdf_rtpm_deregister);

QDF_STATUS __qdf_runtime_lock_init(qdf_runtime_lock_t *lock, const char *name)
{
	return hif_runtime_lock_init(lock, name);
}

qdf_export_symbol(__qdf_runtime_lock_init);

void qdf_runtime_lock_deinit(qdf_runtime_lock_t *lock)
{
	hif_runtime_lock_deinit(lock->lock);
}
qdf_export_symbol(qdf_runtime_lock_deinit);

QDF_STATUS qdf_rtpm_get(uint8_t type, uint32_t id)
{
	return hif_rtpm_get(qdf_to_hif_convert_rtpm_type(type),
			    qdf_to_hif_convert_rtpm_id(id));
}

qdf_export_symbol(qdf_rtpm_get);

QDF_STATUS qdf_rtpm_put(uint8_t type, uint32_t id)
{
	return hif_rtpm_put(qdf_to_hif_convert_rtpm_type(type),
			    qdf_to_hif_convert_rtpm_id(id));
}

qdf_export_symbol(qdf_rtpm_put);

QDF_STATUS qdf_runtime_pm_prevent_suspend(qdf_runtime_lock_t *lock)
{
	return hif_pm_runtime_prevent_suspend(lock->lock);
}

qdf_export_symbol(qdf_runtime_pm_prevent_suspend);

QDF_STATUS qdf_runtime_pm_prevent_suspend_sync(qdf_runtime_lock_t *lock)
{
	return hif_pm_runtime_prevent_suspend_sync(lock->lock);
}

qdf_export_symbol(qdf_runtime_pm_prevent_suspend_sync);

QDF_STATUS qdf_runtime_pm_allow_suspend(qdf_runtime_lock_t *lock)
{
	return hif_pm_runtime_allow_suspend(lock->lock);
}

qdf_export_symbol(qdf_runtime_pm_allow_suspend);

QDF_STATUS qdf_rtpm_sync_resume(void)
{
	return hif_rtpm_sync_resume();
}
#endif
QDF_STATUS qdf_spinlock_acquire(qdf_spinlock_t *lock)
{
	spin_lock(&lock->lock.spinlock);
	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(qdf_spinlock_acquire);


QDF_STATUS qdf_spinlock_release(qdf_spinlock_t *lock)
{
	spin_unlock(&lock->lock.spinlock);
	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(qdf_spinlock_release);

QDF_STATUS qdf_mutex_destroy(qdf_mutex_t *lock)
{
	/* check for invalid pointer */
	if (!lock) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: NULL pointer passed in", __func__);
		return QDF_STATUS_E_FAULT;
	}

	if (LINUX_LOCK_COOKIE != lock->cookie) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: uninitialized lock", __func__);
		return QDF_STATUS_E_INVAL;
	}

	if (in_interrupt()) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s cannot be called from interrupt context!!!",
			  __func__);
		return QDF_STATUS_E_FAULT;
	}

	/* check if lock is released */
	if (!mutex_trylock(&lock->m_lock)) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "%s: lock is not released", __func__);
		return QDF_STATUS_E_BUSY;
	}
	lock->cookie = 0;
	lock->state = LOCK_DESTROYED;
	lock->process_id = 0;
	lock->refcount = 0;

	qdf_lock_stats_destroy(&lock->stats);
	mutex_unlock(&lock->m_lock);

	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(qdf_mutex_destroy);

#if QDF_LOCK_STATS_LIST
struct qdf_lock_cookie {
	union {
		struct {
			struct lock_stats *stats;
			const char *func;
			int line;
		} cookie;
		struct {
			struct qdf_lock_cookie *next;
		} empty_node;
	} u;
};

#ifndef QDF_LOCK_STATS_LIST_SIZE
#define QDF_LOCK_STATS_LIST_SIZE 256
#endif

static qdf_spinlock_t qdf_lock_list_spinlock;
static struct qdf_lock_cookie lock_cookies[QDF_LOCK_STATS_LIST_SIZE];
static struct qdf_lock_cookie *lock_cookie_freelist;
static qdf_atomic_t lock_cookie_get_failures;
static qdf_atomic_t lock_cookie_untracked_num;
/* dummy value */
#define DUMMY_LOCK_COOKIE 0xc00c1e

/**
 * qdf_is_lock_cookie - check if memory is a valid lock cookie
 * @lock_cookie: lock cookie to check
 *
 * Return: true if the memory is within the range of the lock cookie
 *         memory.
 */
static bool qdf_is_lock_cookie(struct qdf_lock_cookie *lock_cookie)
{
	return lock_cookie >= &lock_cookies[0] &&
		lock_cookie <= &lock_cookies[QDF_LOCK_STATS_LIST_SIZE-1];
}

/**
 * qdf_is_lock_cookie_free() -  check if the lock cookie is on the freelist
 * @lock_cookie: lock cookie to check
 *
 * Check that the next field of the lock cookie points to a lock cookie.
 * currently this is only true if the cookie is on the freelist.
 *
 * Checking for the function and line being NULL and 0 should also have worked.
 *
 * Return: true if the cookie is free
 */
static bool qdf_is_lock_cookie_free(struct qdf_lock_cookie *lock_cookie)
{
	struct qdf_lock_cookie *tmp = lock_cookie->u.empty_node.next;

	return qdf_is_lock_cookie(tmp) || (!tmp);
}

static struct qdf_lock_cookie *qdf_get_lock_cookie(void)
{
	struct qdf_lock_cookie *lock_cookie;

	qdf_spin_lock_bh(&qdf_lock_list_spinlock);
	lock_cookie = lock_cookie_freelist;
	if (lock_cookie_freelist)
		lock_cookie_freelist = lock_cookie_freelist->u.empty_node.next;
	qdf_spin_unlock_bh(&qdf_lock_list_spinlock);
	return lock_cookie;
}

static void __qdf_put_lock_cookie(struct qdf_lock_cookie *lock_cookie)
{
	if (!qdf_is_lock_cookie(lock_cookie))
		QDF_BUG(0);

	lock_cookie->u.empty_node.next = lock_cookie_freelist;
	lock_cookie_freelist = lock_cookie;
}

static void qdf_put_lock_cookie(struct qdf_lock_cookie *lock_cookie)
{
	qdf_spin_lock_bh(&qdf_lock_list_spinlock);
	__qdf_put_lock_cookie(lock_cookie);
	qdf_spin_unlock_bh(&qdf_lock_list_spinlock);
}

void qdf_lock_stats_init(void)
{
	int i;

	for (i = 0; i < QDF_LOCK_STATS_LIST_SIZE; i++)
		__qdf_put_lock_cookie(&lock_cookies[i]);

	/* stats must be allocated for the spinlock before the cookie,
	 * otherwise this qdf_lock_list_spinlock wouldn't get initialized
	 * properly
	 */
	qdf_spinlock_create(&qdf_lock_list_spinlock);
	qdf_atomic_init(&lock_cookie_get_failures);
	qdf_atomic_init(&lock_cookie_untracked_num);
}

void qdf_lock_stats_deinit(void)
{
	int i;

	qdf_spinlock_destroy(&qdf_lock_list_spinlock);
	for (i = 0; i < QDF_LOCK_STATS_LIST_SIZE; i++) {
		if (!qdf_is_lock_cookie_free(&lock_cookies[i]))
			QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
				  "%s: lock_not_destroyed, fun: %s, line %d",
				  __func__, lock_cookies[i].u.cookie.func,
				  lock_cookies[i].u.cookie.line);
	}
	lock_cookie_freelist = NULL;
}

/* allocated separate memory in case the lock memory is freed without
 * running the deinitialization code.  The cookie list will not be
 * corrupted.
 */
void qdf_lock_stats_cookie_create(struct lock_stats *stats,
				  const char *func, int line)
{
	struct qdf_lock_cookie *cookie = qdf_get_lock_cookie();

	if (!cookie) {
		int count;

		qdf_atomic_inc(&lock_cookie_get_failures);
		count = qdf_atomic_inc_return(&lock_cookie_untracked_num);
		stats->cookie = (void *) DUMMY_LOCK_COOKIE;
		return;
	}

	stats->cookie = cookie;
	stats->cookie->u.cookie.stats = stats;
	stats->cookie->u.cookie.func = func;
	stats->cookie->u.cookie.line = line;
}

qdf_export_symbol(qdf_lock_stats_cookie_create);

void qdf_lock_stats_cookie_destroy(struct lock_stats *stats)
{
	struct qdf_lock_cookie *cookie = stats->cookie;

	if (!cookie) {
		QDF_DEBUG_PANIC("Lock destroyed twice or never created");
		return;
	}

	stats->cookie = NULL;
	if (cookie == (void *)DUMMY_LOCK_COOKIE) {
		qdf_atomic_dec(&lock_cookie_untracked_num);
		return;
	}

	cookie->u.cookie.stats = NULL;
	cookie->u.cookie.func = NULL;
	cookie->u.cookie.line = 0;

	qdf_put_lock_cookie(cookie);
}

qdf_export_symbol(qdf_lock_stats_cookie_destroy);
#endif

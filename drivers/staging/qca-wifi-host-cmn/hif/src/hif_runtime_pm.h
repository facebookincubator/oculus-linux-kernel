/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __HIF_RUNTIME_PM_H__
#define __HIF_RUNTIME_PM_H__

/**
 * enum hif_rtpm_fill_type - Caller type for Runtime PM stats fill
 * @HIF_RTPM_FILL_TYPE_SYSFS: Sysfs is caller for Runtime PM stats
 * @HIF_RTPM_FILL_TYPE_DEBUGFS: Debugfs is caller for Runtime PM stats
 * @HIF_RTPM_FILL_TYPE_MAX: max value
 */
enum hif_rtpm_fill_type {
	HIF_RTPM_FILL_TYPE_SYSFS,
	HIF_RTPM_FILL_TYPE_DEBUGFS,
	HIF_RTPM_FILL_TYPE_MAX,
};

#ifdef FEATURE_RUNTIME_PM
#include <linux/pm_runtime.h>

/**
 * enum hif_rtpm_state - Driver States for Runtime Power Management
 * @HIF_RTPM_STATE_NONE: runtime pm is off
 * @HIF_RTPM_STATE_ON: runtime pm is active and link is active
 * @HIF_RTPM_STATE_RESUMING: a runtime resume is in progress
 * @HIF_RTPM_STATE_RESUMING_LINKUP: Link is up during resume
 * @HIF_RTPM_STATE_SUSPENDING: a runtime suspend is in progress
 * @HIF_RTPM_STATE_SUSPENDED: the driver is runtime suspended
 */
enum hif_rtpm_state {
	HIF_RTPM_STATE_NONE,
	HIF_RTPM_STATE_ON,
	HIF_RTPM_STATE_RESUMING_LINKUP,
	HIF_RTPM_STATE_RESUMING,
	HIF_RTPM_STATE_SUSPENDING,
	HIF_RTPM_STATE_SUSPENDED,
};

/**
 * struct hif_rtpm_state_stats - Runtime PM stats
 * @resume_count: count of resume calls done
 * @suspend_count: count of suspend calls done
 * @suspend_err_count: count of suspend calls failed
 * @resume_ts: Last resume call timestamp
 * @suspend_ts: Last suspend call timestamp
 * @suspend_err_ts: Last suspend call fail timestamp
 * @last_busy_ts: Last busy timestamp marked
 * @last_busy_id:
 * @last_busy_marker:
 * @request_resume_ts: Last request resume done timestamp
 * @request_resume_id: Client ID requesting resume
 * @prevent_suspend:
 * @allow_suspend:
 * @runtime_get_err:
 */
struct hif_rtpm_state_stats {
	uint32_t resume_count;
	uint32_t suspend_count;
	uint32_t suspend_err_count;
	uint64_t resume_ts;
	uint64_t suspend_ts;
	uint64_t suspend_err_ts;
	uint64_t last_busy_ts;
	uint32_t last_busy_id;
	void *last_busy_marker;
	uint64_t request_resume_ts;
	uint64_t request_resume_id;
	uint32_t prevent_suspend;
	uint32_t allow_suspend;
	uint32_t runtime_get_err;
};

#define HIF_RTPM_BUSY_HIST_MAX  16
#define HIF_RTPM_BUSY_HIST_MASK (HIF_RTPM_BUSY_HIST_MAX - 1)

/**
 * struct hif_rtpm_last_busy_hist - Runtime last busy hist
 * @last_busy_cnt: count of last busy mark
 * @last_busy_idx: last busy history index
 * @last_busy_ts: last busy marked timestamp
 */
struct hif_rtpm_last_busy_hist {
	unsigned long last_busy_cnt;
	unsigned long last_busy_idx;
	uint64_t last_busy_ts[HIF_RTPM_BUSY_HIST_MAX];
};

/**
 * struct hif_rtpm_client - Runtime PM client structure
 * @hif_rtpm_cbk: Callback during resume if get called at suspend and failed
 * @active_count: current active status of client
 * @get_count: count of get calls by this client
 * @put_count: count of put calls by this client
 * @last_busy_cnt:
 * @get_ts: Last get called timestamp
 * @put_ts: Last put called timestamp
 * @last_busy_ts:
 */
struct hif_rtpm_client {
	void (*hif_rtpm_cbk)(void);
	qdf_atomic_t active_count;
	qdf_atomic_t get_count;
	qdf_atomic_t put_count;
	uint32_t last_busy_cnt;
	uint64_t get_ts;
	uint64_t put_ts;
	uint64_t last_busy_ts;
};

/**
 * struct hif_rtpm_ctx - Runtime power management context
 * @enable_rpm:
 * @dev:
 * @runtime_lock: Lock to sync state changes with get calls
 * @runtime_suspend_lock: Suspend lock
 * @client_count: Number of clients currently registered
 * @clients: clients registered to use runtime PM module
 * @prevent_list_lock:
 * @prevent_list:
 * @prevent_cnt:
 * @pm_state: Current runtime pm state
 * @pending_job: bitmap to set the client job to be called at resume
 * @monitor_wake_intr: Monitor waking MSI for runtime PM
 * @stats: Runtime PM stats
 * @pm_dentry: debug fs entry
 * @cfg_delay:
 * @delay:
 * @busy_hist: busy histogram
 */
struct hif_rtpm_ctx {
	bool enable_rpm;
	struct device *dev;
	qdf_spinlock_t runtime_lock;
	qdf_spinlock_t runtime_suspend_lock;
	unsigned int client_count;
	struct hif_rtpm_client *clients[HIF_RTPM_ID_MAX];
	qdf_spinlock_t prevent_list_lock;
	struct list_head prevent_list;
	uint32_t prevent_cnt;
	qdf_atomic_t pm_state;
	unsigned long pending_job;
	qdf_atomic_t monitor_wake_intr;
	struct hif_rtpm_state_stats stats;
	struct dentry *pm_dentry;
	int cfg_delay;
	int delay;
	struct hif_rtpm_last_busy_hist *busy_hist[CE_COUNT_MAX];
};

#define HIF_RTPM_DELAY_MIN 100
#define HIF_RTPM_DELAY_MAX 10000

/**
 * __hif_rtpm_enabled() - Check if runtime pm is enabled from kernel
 * @dev: device structure
 *
 * Return: true if enabled.
 */
static inline bool __hif_rtpm_enabled(struct device *dev)
{
	return pm_runtime_enabled(dev);
}

/**
 * __hif_rtpm_get() - Increment dev usage count and call resume function
 * @dev: device structure
 *
 * Increments usage count and internally queue resume work
 *
 * Return: 1 if state is active. 0 if resume is requested. Error otherwise.
 */
static inline int __hif_rtpm_get(struct device *dev)
{
	return pm_runtime_get(dev);
}

/**
 * __hif_rtpm_get_noresume() - Only increment dev usage count
 * @dev: device structure
 *
 * Return: Void
 */
static inline void __hif_rtpm_get_noresume(struct device *dev)
{
	pm_runtime_get_noresume(dev);
}

/**
 * __hif_rtpm_get_sync() - Increment usage count and set state to active
 * @dev: device structure
 *
 * Return: 1 if state is already active, 0 if state active was done.
 *         Error otherwise.
 */
static inline int __hif_rtpm_get_sync(struct device *dev)
{
	return pm_runtime_get_sync(dev);
}

/**
 * __hif_rtpm_put_auto() - Decrement usage count and call suspend function
 * @dev: device structure
 *
 * Decrements usage count and queue suspend work if usage count is 0
 *
 * Return: 0 if success. Error otherwise.
 */
static inline int __hif_rtpm_put_auto(struct device *dev)
{
	return pm_runtime_put_autosuspend(dev);
}

/**
 * __hif_rtpm_put_noidle() - Decrement usage count
 * @dev: device structure
 *
 * Return: void
 */
static inline void __hif_rtpm_put_noidle(struct device *dev)
{
	pm_runtime_put_noidle(dev);
}

/**
 * __hif_rtpm_put_sync_suspend() - Decrement usage count
 * @dev: device structure
 *
 * Decrement usage_count of device and if 0 synchrounsly call suspend function
 *
 * Return: 0 if success. Error otherwise
 */
static inline int __hif_rtpm_put_sync_suspend(struct device *dev)
{
	return pm_runtime_put_sync_suspend(dev);
}

/**
 * __hif_rtpm_set_autosuspend_delay() - Set delay to trigger RTPM suspend
 * @dev: device structure
 * @delay: delay in ms to be set
 *
 * Return: None
 */
static inline
void __hif_rtpm_set_autosuspend_delay(struct device *dev, int delay)
{
	pm_runtime_set_autosuspend_delay(dev, delay);
}

/**
 * __hif_rtpm_mark_last_busy() - Mark last busy timestamp
 * @dev: device structure
 *
 * Return: Void
 */
static inline void __hif_rtpm_mark_last_busy(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);
}

/**
 * __hif_rtpm_resume() - Do Runtime PM Resume of bus
 * @dev: device structure
 *
 * Return: 0 if success. Error otherwise
 */
static inline int __hif_rtpm_resume(struct device *dev)
{
	return pm_runtime_resume(dev);
}

/**
 * __hif_rtpm_request_resume() - Queue resume work
 * @dev: device structure
 *
 * Return: 1 if already active. 0 if successfully queued. Error otherwise
 */
static inline int __hif_rtpm_request_resume(struct device *dev)
{
	return pm_request_resume(dev);
}

/**
 * hif_rtpm_open() - initialize runtime pm
 * @scn: hif ctx
 *
 * Return: None
 */
void hif_rtpm_open(struct hif_softc *scn);

/**
 * hif_rtpm_close() - close runtime pm
 * @scn: hif ctx
 *
 * ensure runtime_pm is stopped before closing the driver
 *
 * Return: None
 */
void hif_rtpm_close(struct hif_softc *scn);

/**
 * hif_rtpm_start() - start the runtime pm
 * @scn: hif context
 *
 * After this call, runtime pm will be active.
 *
 * Return: None
 */
void hif_rtpm_start(struct hif_softc *scn);

/**
 * hif_rtpm_stop() - stop runtime pm
 * @scn: hif context
 *
 * Turns off runtime pm and frees corresponding resources
 * that were acquired by hif_rtpm_start().
 *
 * Return: None
 */
void hif_rtpm_stop(struct hif_softc *scn);

/**
 * hif_rtpm_log_debug_stats() - fill buffer with runtime pm stats
 * @s: pointer to buffer to fill status
 * @type: type of caller
 *
 * Fills input buffer with runtime pm stats depending on caller type.
 *
 * Return: Num of bytes filled if caller type requires, else 0.
 */
int hif_rtpm_log_debug_stats(void *s, enum hif_rtpm_fill_type type);

#else
static inline void hif_rtpm_open(struct hif_softc *scn) {}
static inline void hif_rtpm_close(struct hif_softc *scn) {}
static inline void hif_rtpm_start(struct hif_softc *scn) {}
static inline void hif_rtpm_stop(struct hif_softc *scn) {}
static inline int hif_rtpm_log_debug_stats(void *s,
					   enum hif_rtpm_fill_type type)
{
	return 0;
}
#endif /* FEATURE_RUNTIME_PM */
#endif /* __HIF_RUNTIME_PM_H__ */

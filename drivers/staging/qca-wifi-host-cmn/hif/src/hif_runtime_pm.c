/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#include "hif_io32.h"
#include "hif_runtime_pm.h"
#include "hif.h"
#include "target_type.h"
#include "hif_main.h"
#include "ce_main.h"
#include "ce_api.h"
#include "ce_internal.h"
#include "ce_reg.h"
#include "ce_bmi.h"
#include "regtable.h"
#include "hif_hw_version.h"
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "qdf_status.h"
#include "qdf_atomic.h"
#include "pld_common.h"
#include "mp_dev.h"
#include "hif_debug.h"

#include "ce_tasklet.h"
#include "targaddrs.h"
#include "hif_exec.h"

#define CNSS_RUNTIME_FILE "cnss_runtime_pm"
#define CNSS_RUNTIME_FILE_PERM QDF_FILE_USR_READ

#ifdef FEATURE_RUNTIME_PM

static struct hif_rtpm_ctx g_hif_rtpm_ctx;
static struct hif_rtpm_ctx *gp_hif_rtpm_ctx;

/**
 * hif_rtpm_id_to_string() - Convert dbgid to respective string
 * @id: debug id
 *
 * Debug support function to convert  dbgid to string.
 * Please note to add new string in the array at index equal to
 * its enum value in wlan_rtpm_dbgid.
 *
 * Return: String of ID
 */
static const char *hif_rtpm_id_to_string(enum hif_rtpm_client_id id)
{
	static const char * const strings[] = {
					"HIF_RTPM_ID_RESERVED",
					"HIF_RTPM_HAL_REO_CMD",
					"HIF_RTPM_WMI",
					"HIF_RTPM_HTT",
					"HIF_RTPM_DP",
					"HIF_RTPM_RING_STATS",
					"HIF_RTPM_CE",
					"HIF_RTPM_FORCE_WAKE",
					"HIF_RTPM_ID_PM_QOS_NOTIFY",
					"HIF_RTPM_ID_WIPHY_SUSPEND",
					"HIF_RTPM_ID_MAX"
	};

	return strings[id];
}

/**
 * hif_rtpm_read_usage_count() - Read device usage count
 *
 * Return: current usage count
 */
static inline int hif_rtpm_read_usage_count(void)
{
	return qdf_atomic_read(&gp_hif_rtpm_ctx->dev->power.usage_count);
}

#define HIF_RTPM_STATS(_s, _rtpm_ctx, _name) \
	seq_printf(_s, "%30s: %u\n", #_name, (_rtpm_ctx)->stats._name)

/**
 * hif_rtpm_debugfs_show(): show debug stats for runtimepm
 * @s: file to print to
 * @data: unused
 *
 * debugging tool added to the debug fs for displaying runtimepm stats
 *
 * Return: 0
 */
static int hif_rtpm_debugfs_show(struct seq_file *s, void *data)
{
	struct hif_rtpm_client *client = NULL;
	struct hif_pm_runtime_lock *ctx;
	static const char * const autopm_state[] = {"NONE", "ON", "RESUMING",
			"RESUMING_LINKUP", "SUSPENDING", "SUSPENDED"};
	int pm_state = qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state);
	int i;

	seq_printf(s, "%30s: %llu\n", "Current timestamp",
		   qdf_get_log_timestamp());

	seq_printf(s, "%30s: %s\n", "Runtime PM state", autopm_state[pm_state]);

	seq_printf(s, "%30s: %llu\n", "Last Busy timestamp",
		   gp_hif_rtpm_ctx->stats.last_busy_ts);

	seq_printf(s, "%30s: %ps\n", "Last Busy Marker",
		   gp_hif_rtpm_ctx->stats.last_busy_marker);

	seq_puts(s, "Rx busy marker counts:\n");
	seq_printf(s, "%30s: %u %llu\n", hif_rtpm_id_to_string(HIF_RTPM_ID_DP),
		   gp_hif_rtpm_ctx->clients[HIF_RTPM_ID_DP]->last_busy_cnt,
		   gp_hif_rtpm_ctx->clients[HIF_RTPM_ID_DP]->last_busy_ts);

	seq_printf(s, "%30s: %u %llu\n", hif_rtpm_id_to_string(HIF_RTPM_ID_CE),
		   gp_hif_rtpm_ctx->clients[HIF_RTPM_ID_CE]->last_busy_cnt,
		   gp_hif_rtpm_ctx->clients[HIF_RTPM_ID_CE]->last_busy_ts);

	HIF_RTPM_STATS(s, gp_hif_rtpm_ctx, last_busy_id);

	if (pm_state == HIF_RTPM_STATE_SUSPENDED) {
		seq_printf(s, "%30s: %llx us\n", "Suspended Since",
			   gp_hif_rtpm_ctx->stats.suspend_ts);
	}

	HIF_RTPM_STATS(s, gp_hif_rtpm_ctx, resume_count);
	HIF_RTPM_STATS(s, gp_hif_rtpm_ctx, suspend_count);
	HIF_RTPM_STATS(s, gp_hif_rtpm_ctx, suspend_err_count);

	seq_printf(s, "%30s: %d\n", "PM Usage count",
		   hif_rtpm_read_usage_count());

	seq_puts(s, "get  put  get-timestamp put-timestamp :DBGID_NAME\n");
	for (i = 0; i < HIF_RTPM_ID_MAX; i++) {
		client = gp_hif_rtpm_ctx->clients[i];
		if (!client)
			continue;
		seq_printf(s, "%-10d ", qdf_atomic_read(&client->get_count));
		seq_printf(s, "%-10d ", qdf_atomic_read(&client->put_count));
		seq_printf(s, "0x%-10llx ", client->get_ts);
		seq_printf(s, "0x%-10llx ", client->put_ts);
		seq_printf(s, ":%-2d %-30s\n", i, hif_rtpm_id_to_string(i));
	}
	seq_puts(s, "\n");

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	if (list_empty(&gp_hif_rtpm_ctx->prevent_list)) {
		qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
		return 0;
	}

	seq_printf(s, "%30s: ", "Active Wakeup_Sources");
	list_for_each_entry(ctx, &gp_hif_rtpm_ctx->prevent_list, list) {
		seq_printf(s, "%s", ctx->name);
		seq_puts(s, " ");
	}
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	seq_puts(s, "\n");

	return 0;
}

#undef HIF_RTPM_STATS

/**
 * hif_rtpm_debugfs_open() - open a debug fs file to access the runtime pm stats
 * @inode:
 * @file:
 *
 * Return: linux error code of single_open.
 */
static int hif_rtpm_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, hif_rtpm_debugfs_show,
			inode->i_private);
}

static const struct file_operations hif_rtpm_fops = {
	.owner          = THIS_MODULE,
	.open           = hif_rtpm_debugfs_open,
	.release        = single_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
};

/**
 * hif_rtpm_debugfs_create() - creates runtimepm debugfs entry
 *
 * creates a debugfs entry to debug the runtime pm feature.
 */
static void hif_rtpm_debugfs_create(void)
{
	gp_hif_rtpm_ctx->pm_dentry = qdf_debugfs_create_entry(CNSS_RUNTIME_FILE,
							CNSS_RUNTIME_FILE_PERM,
							NULL,
							NULL,
							&hif_rtpm_fops);
}

/**
 * hif_rtpm_debugfs_remove() - removes runtimepm debugfs entry
 *
 * removes the debugfs entry to debug the runtime pm feature.
 */
static void hif_rtpm_debugfs_remove(void)
{
	qdf_debugfs_remove_file(gp_hif_rtpm_ctx->pm_dentry);
}

/**
 * hif_rtpm_init() - Initialize Runtime PM
 * @dev: device structure
 * @delay: delay to be configured for auto suspend
 *
 * This function will init all the Runtime PM config.
 *
 * Return: void
 */
static void hif_rtpm_init(struct device *dev, int delay)
{
	pm_runtime_set_autosuspend_delay(dev, delay);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_allow(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_noidle(dev);
	pm_suspend_ignore_children(dev, true);
}

/**
 * hif_rtpm_exit() - Deinit/Exit Runtime PM
 * @dev: device structure
 *
 * This function will deinit all the Runtime PM config.
 *
 * Return: void
 */
static void hif_rtpm_exit(struct device *dev)
{
	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_forbid(dev);
}

static void hif_rtpm_alloc_last_busy_hist(void)
{
	int i;

	for (i = 0; i < CE_COUNT_MAX; i++) {
		if (i != CE_ID_1 && i != CE_ID_2 && i != CE_ID_7) {
			gp_hif_rtpm_ctx->busy_hist[i] = NULL;
			continue;
		}

		gp_hif_rtpm_ctx->busy_hist[i] =
			qdf_mem_malloc(sizeof(struct hif_rtpm_last_busy_hist));
		if (!gp_hif_rtpm_ctx->busy_hist[i])
			return;
	}
}

static void hif_rtpm_free_last_busy_hist(void)
{
	int i;

	for (i = 0; i < CE_COUNT_MAX; i++) {
		if (i != CE_ID_1 && i != CE_ID_2 && i != CE_ID_7)
			continue;

		qdf_mem_free(gp_hif_rtpm_ctx->busy_hist[i]);
	}
}

void hif_rtpm_open(struct hif_softc *scn)
{
	gp_hif_rtpm_ctx = &g_hif_rtpm_ctx;
	gp_hif_rtpm_ctx->dev = scn->qdf_dev->dev;
	qdf_spinlock_create(&gp_hif_rtpm_ctx->runtime_lock);
	qdf_spinlock_create(&gp_hif_rtpm_ctx->runtime_suspend_lock);
	qdf_spinlock_create(&gp_hif_rtpm_ctx->prevent_list_lock);
	qdf_atomic_init(&gp_hif_rtpm_ctx->pm_state);
	qdf_atomic_set(&gp_hif_rtpm_ctx->pm_state, HIF_RTPM_STATE_NONE);
	qdf_atomic_init(&gp_hif_rtpm_ctx->monitor_wake_intr);
	INIT_LIST_HEAD(&gp_hif_rtpm_ctx->prevent_list);
	gp_hif_rtpm_ctx->client_count = 0;
	gp_hif_rtpm_ctx->pending_job = 0;
	hif_rtpm_register(HIF_RTPM_ID_CE, NULL);
	hif_rtpm_register(HIF_RTPM_ID_FORCE_WAKE, NULL);
	hif_rtpm_alloc_last_busy_hist();
	hif_info_high("Runtime PM attached");
}

static int __hif_pm_runtime_allow_suspend(struct hif_pm_runtime_lock *lock);

/**
 * hif_rtpm_sanitize_exit(): sanitize runtime PM gets/puts from driver
 *
 * Ensure all gets/puts are in sync before exiting runtime PM feature.
 * Also make sure all runtime PM locks are deinitialized properly.
 *
 * Return: void
 */
static void hif_rtpm_sanitize_exit(void)
{
	struct hif_pm_runtime_lock *ctx, *tmp;
	struct hif_rtpm_client *client;
	int i, active_count;

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	list_for_each_entry_safe(ctx, tmp,
				 &gp_hif_rtpm_ctx->prevent_list, list) {
		hif_runtime_lock_deinit(ctx);
	}
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);

	/* check if get and put out of sync for all clients */
	for (i = 0; i < HIF_RTPM_ID_MAX; i++) {
		client = gp_hif_rtpm_ctx->clients[i];
		if (client) {
			if (qdf_atomic_read(&client->active_count)) {
				active_count =
					qdf_atomic_read(&client->active_count);
				hif_err("Client active: %u- %s", i,
					hif_rtpm_id_to_string(i));
				QDF_DEBUG_PANIC("Client active on exit!");
				while (active_count--)
					__hif_rtpm_put_noidle(
							gp_hif_rtpm_ctx->dev);
			}
			QDF_DEBUG_PANIC("Client not deinitialized");
			qdf_mem_free(client);
			gp_hif_rtpm_ctx->clients[i] = NULL;
		}
	}
}

/**
 * hif_rtpm_sanitize_ssr_exit() - Empty the suspend list on SSR
 *
 * API is used to empty the runtime pm prevent suspend list.
 *
 * Return: void
 */
static void hif_rtpm_sanitize_ssr_exit(void)
{
	struct hif_pm_runtime_lock *ctx, *tmp;

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	list_for_each_entry_safe(ctx, tmp,
				 &gp_hif_rtpm_ctx->prevent_list, list) {
		__hif_pm_runtime_allow_suspend(ctx);
	}
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
}

void hif_rtpm_close(struct hif_softc *scn)
{
	hif_rtpm_free_last_busy_hist();
	hif_rtpm_deregister(HIF_RTPM_ID_CE);
	hif_rtpm_deregister(HIF_RTPM_ID_FORCE_WAKE);

	hif_is_recovery_in_progress(scn) ?
		hif_rtpm_sanitize_ssr_exit() :
		hif_rtpm_sanitize_exit();

	qdf_mem_set(gp_hif_rtpm_ctx, sizeof(*gp_hif_rtpm_ctx), 0);
	gp_hif_rtpm_ctx = NULL;
	hif_info_high("Runtime PM context detached");
}

void hif_rtpm_start(struct hif_softc *scn)
{
	uint32_t mode = hif_get_conparam(scn);

	gp_hif_rtpm_ctx->enable_rpm = scn->hif_config.enable_runtime_pm;

	if (!gp_hif_rtpm_ctx->enable_rpm) {
		hif_info_high("RUNTIME PM is disabled in ini");
		return;
	}

	if (mode == QDF_GLOBAL_FTM_MODE || QDF_IS_EPPING_ENABLED(mode) ||
	    mode == QDF_GLOBAL_MONITOR_MODE) {
		hif_info("RUNTIME PM is disabled for FTM/EPPING/MONITOR mode");
		return;
	}

	hif_info_high("Enabling RUNTIME PM, Delay: %d ms",
		      scn->hif_config.runtime_pm_delay);

	qdf_atomic_set(&gp_hif_rtpm_ctx->pm_state, HIF_RTPM_STATE_ON);
	hif_rtpm_init(gp_hif_rtpm_ctx->dev, scn->hif_config.runtime_pm_delay);
	gp_hif_rtpm_ctx->cfg_delay = scn->hif_config.runtime_pm_delay;
	gp_hif_rtpm_ctx->delay = gp_hif_rtpm_ctx->cfg_delay;
	hif_rtpm_debugfs_create();
}

void hif_rtpm_stop(struct hif_softc *scn)
{
	uint32_t mode = hif_get_conparam(scn);

	if (!gp_hif_rtpm_ctx->enable_rpm)
		return;

	if (mode == QDF_GLOBAL_FTM_MODE || QDF_IS_EPPING_ENABLED(mode) ||
	    mode == QDF_GLOBAL_MONITOR_MODE)
		return;

	hif_rtpm_exit(gp_hif_rtpm_ctx->dev);

	hif_rtpm_sync_resume();

	qdf_atomic_set(&gp_hif_rtpm_ctx->pm_state, HIF_RTPM_STATE_NONE);
	hif_rtpm_debugfs_remove();
}

QDF_STATUS hif_rtpm_register(uint32_t id, void (*hif_rtpm_cbk)(void))
{
	struct hif_rtpm_client *client;

	if (qdf_unlikely(!gp_hif_rtpm_ctx)) {
		hif_err("Runtime PM context NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (id >= HIF_RTPM_ID_MAX || gp_hif_rtpm_ctx->clients[id]) {
		hif_err("Invalid client %d", id);
		return QDF_STATUS_E_INVAL;
	}

	client = qdf_mem_malloc(sizeof(struct hif_rtpm_client));
	if (!client)
		return QDF_STATUS_E_NOMEM;

	client->hif_rtpm_cbk = hif_rtpm_cbk;
	qdf_atomic_init(&client->active_count);
	qdf_atomic_init(&client->get_count);
	qdf_atomic_init(&client->put_count);

	gp_hif_rtpm_ctx->clients[id] = client;
	gp_hif_rtpm_ctx->client_count++;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hif_rtpm_deregister(uint32_t id)
{
	struct hif_rtpm_client *client;
	int active_count;

	if (qdf_unlikely(!gp_hif_rtpm_ctx)) {
		hif_err("Runtime PM context NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (id >= HIF_RTPM_ID_MAX || !gp_hif_rtpm_ctx->clients[id]) {
		hif_err("invalid client, id: %u", id);
		return QDF_STATUS_E_INVAL;
	}

	client = gp_hif_rtpm_ctx->clients[id];
	if (qdf_atomic_read(&client->active_count)) {
		active_count = qdf_atomic_read(&client->active_count);
		hif_err("Client: %u-%s Runtime PM active",
			id, hif_rtpm_id_to_string(id));
		hif_err("last get called: 0x%llx, get count: %d, put count: %d",
			client->get_ts, qdf_atomic_read(&client->get_count),
			qdf_atomic_read(&client->put_count));
		QDF_DEBUG_PANIC("Get and PUT call out of sync!");
		while (active_count--)
			__hif_rtpm_put_noidle(gp_hif_rtpm_ctx->dev);
	}

	qdf_mem_free(client);
	gp_hif_rtpm_ctx->clients[id] = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hif_rtpm_set_autosuspend_delay(int delay)
{
	if (delay < HIF_RTPM_DELAY_MIN || delay > HIF_RTPM_DELAY_MAX) {
		hif_err("Invalid delay value %d ms", delay);
		return QDF_STATUS_E_INVAL;
	}

	__hif_rtpm_set_autosuspend_delay(gp_hif_rtpm_ctx->dev, delay);
	gp_hif_rtpm_ctx->delay = delay;
	hif_info_high("RTPM delay set: %d ms", delay);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hif_rtpm_restore_autosuspend_delay(void)
{
	if (gp_hif_rtpm_ctx->delay == gp_hif_rtpm_ctx->cfg_delay) {
		hif_info_rl("RTPM delay already default: %d",
			    gp_hif_rtpm_ctx->delay);
		return QDF_STATUS_E_ALREADY;
	}

	__hif_rtpm_set_autosuspend_delay(gp_hif_rtpm_ctx->dev,
					 gp_hif_rtpm_ctx->cfg_delay);
	gp_hif_rtpm_ctx->delay = gp_hif_rtpm_ctx->cfg_delay;
	hif_info_rl("RTPM delay set: %d ms", gp_hif_rtpm_ctx->delay);

	return QDF_STATUS_SUCCESS;
}

int hif_rtpm_get_autosuspend_delay(void)
{
	return gp_hif_rtpm_ctx->delay;
}

int hif_runtime_lock_init(qdf_runtime_lock_t *lock, const char *name)
{
	struct hif_pm_runtime_lock *context;

	if (qdf_unlikely(!gp_hif_rtpm_ctx)) {
		hif_err("Runtime PM context NULL");
		return QDF_STATUS_E_FAILURE;
	}

	hif_debug("Initializing Runtime PM wakelock %s", name);

	context = qdf_mem_malloc(sizeof(*context));
	if (!context)
		return -ENOMEM;

	context->name = name ? name : "Default";
	lock->lock = context;

	return 0;
}

void hif_runtime_lock_deinit(struct hif_pm_runtime_lock *lock)
{
	if (!lock) {
		hif_err("Runtime PM lock already freed");
		return;
	}

	hif_debug("Deinitializing Runtime PM wakelock %s", lock->name);

	if (gp_hif_rtpm_ctx) {
		qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
		__hif_pm_runtime_allow_suspend(lock);
		qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	}

	qdf_mem_free(lock);
}

/**
 * hif_rtpm_enabled() - To check if Runtime PM is enabled
 *
 * This function will check if Runtime PM is enabled or not.
 *
 * Return: void
 */
static bool hif_rtpm_enabled(void)
{
	if (qdf_unlikely(!gp_hif_rtpm_ctx))
		return false;

	if (gp_hif_rtpm_ctx->enable_rpm)
		return true;

	return __hif_rtpm_enabled(gp_hif_rtpm_ctx->dev);
}

QDF_STATUS hif_rtpm_get(uint8_t type, uint32_t id)
{
	struct hif_rtpm_client *client = NULL;
	int ret = QDF_STATUS_E_FAILURE;
	int pm_state;

	if (!hif_rtpm_enabled())
		return QDF_STATUS_SUCCESS;

	if (id >= HIF_RTPM_ID_MAX || !gp_hif_rtpm_ctx->clients[id]) {
		QDF_DEBUG_PANIC("Invalid client, id: %u", id);
		return -QDF_STATUS_E_INVAL;
	}

	client = gp_hif_rtpm_ctx->clients[id];

	if (type != HIF_RTPM_GET_ASYNC) {
		switch (type) {
		case HIF_RTPM_GET_FORCE:
			ret = __hif_rtpm_get(gp_hif_rtpm_ctx->dev);
			break;
		case HIF_RTPM_GET_SYNC:
			ret = __hif_rtpm_get_sync(gp_hif_rtpm_ctx->dev);
			break;
		case HIF_RTPM_GET_NORESUME:
			__hif_rtpm_get_noresume(gp_hif_rtpm_ctx->dev);
			ret = 0;
			break;
		default:
			QDF_DEBUG_PANIC("Invalid call type");
			return QDF_STATUS_E_BADMSG;
		}

		if (ret < 0 && ret != -EINPROGRESS) {
			hif_err("pm_state: %d ret: %d",
				qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state),
				ret);
			__hif_rtpm_put_noidle(gp_hif_rtpm_ctx->dev);
		} else {
			ret = QDF_STATUS_SUCCESS;
		}
		goto out;
	}

	pm_state = qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state);
	if (pm_state <= HIF_RTPM_STATE_RESUMING_LINKUP) {
		ret = __hif_rtpm_get(gp_hif_rtpm_ctx->dev);
		/* Get will return 1 if the device is already active,
		 * just return success in that case
		 */
		if (ret > 0) {
			ret = QDF_STATUS_SUCCESS;
		} else if (ret == 0 || ret == -EINPROGRESS) {
			qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
			pm_state = qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state);
			if (pm_state >= HIF_RTPM_STATE_RESUMING) {
				__hif_rtpm_put_noidle(gp_hif_rtpm_ctx->dev);
				gp_hif_rtpm_ctx->stats.request_resume_ts =
							qdf_get_log_timestamp();
				gp_hif_rtpm_ctx->stats.request_resume_id = id;
				ret = QDF_STATUS_E_FAILURE;
			} else {
				ret = QDF_STATUS_SUCCESS;
			}
			qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);
		} else if (ret < 0) {
			hif_err("pm_state: %d ret: %d",
				qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state),
				ret);
			__hif_rtpm_put_noidle(gp_hif_rtpm_ctx->dev);
		}
	} else if (pm_state >= HIF_RTPM_STATE_RESUMING) {
		/* Do not log in performance path */
		if (id != HIF_RTPM_ID_DP)
			hif_info_high("request RTPM resume by %d- %s",
				      id, hif_rtpm_id_to_string(id));
		__hif_rtpm_request_resume(gp_hif_rtpm_ctx->dev);
		gp_hif_rtpm_ctx->stats.request_resume_ts =
						qdf_get_log_timestamp();
		gp_hif_rtpm_ctx->stats.request_resume_id = id;
		return QDF_STATUS_E_FAILURE;
	}

out:
	if (QDF_IS_STATUS_SUCCESS(ret)) {
		qdf_atomic_inc(&client->active_count);
		qdf_atomic_inc(&client->get_count);
		client->get_ts = qdf_get_log_timestamp();
	}

	return ret;
}

QDF_STATUS hif_rtpm_put(uint8_t type, uint32_t id)
{
	struct hif_rtpm_client *client;
	int usage_count;

	if (!hif_rtpm_enabled())
		return QDF_STATUS_SUCCESS;

	if (id >= HIF_RTPM_ID_MAX || !gp_hif_rtpm_ctx->clients[id]) {
		hif_err("Invalid client, id: %u", id);
		return QDF_STATUS_E_INVAL;
	}

	client = gp_hif_rtpm_ctx->clients[id];

	usage_count = hif_rtpm_read_usage_count();
	if (usage_count == 2 && !gp_hif_rtpm_ctx->enable_rpm) {
		hif_err("Unexpected PUT when runtime PM is disabled");
		QDF_BUG(0);
		return QDF_STATUS_E_CANCELED;
	} else if (!usage_count || !qdf_atomic_read(&client->active_count)) {
		hif_info_high("Put without a Get operation, %u-%s",
			      id, hif_rtpm_id_to_string(id));
		return QDF_STATUS_E_CANCELED;
	}

	switch (type) {
	case HIF_RTPM_PUT_ASYNC:
		__hif_rtpm_put_auto(gp_hif_rtpm_ctx->dev);
		break;
	case HIF_RTPM_PUT_NOIDLE:
		__hif_rtpm_put_noidle(gp_hif_rtpm_ctx->dev);
		break;
	case HIF_RTPM_PUT_SYNC_SUSPEND:
		__hif_rtpm_put_sync_suspend(gp_hif_rtpm_ctx->dev);
		break;
	default:
		QDF_DEBUG_PANIC("Invalid call type");
		return QDF_STATUS_E_BADMSG;
	}

	__hif_rtpm_mark_last_busy(gp_hif_rtpm_ctx->dev);
	qdf_atomic_dec(&client->active_count);
	qdf_atomic_inc(&client->put_count);
	client->put_ts = qdf_get_log_timestamp();
	gp_hif_rtpm_ctx->stats.last_busy_ts = client->put_ts;

	return QDF_STATUS_SUCCESS;
}

/**
 * __hif_pm_runtime_prevent_suspend() - prevent runtime suspend for a protocol
 *                                      reason
 * @lock: runtime_pm lock being acquired
 *
 * Return: 0 if successful.
 */
static int __hif_pm_runtime_prevent_suspend(struct hif_pm_runtime_lock *lock)
{
	int ret = 0;

	if (lock->active)
		return 0;

	ret = __hif_rtpm_get(gp_hif_rtpm_ctx->dev);

	/**
	 * The ret can be -EINPROGRESS, if Runtime status is RPM_RESUMING or
	 * RPM_SUSPENDING. Any other negative value is an error.
	 * We shouldn't do runtime_put here as in later point allow
	 * suspend gets called with the context and there the usage count
	 * is decremented, so suspend will be prevented.
	 */
	if (ret < 0 && ret != -EINPROGRESS) {
		gp_hif_rtpm_ctx->stats.runtime_get_err++;
		hif_err("pm_state: %d ret: %d",
			qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state),
			ret);
	}

	list_add_tail(&lock->list, &gp_hif_rtpm_ctx->prevent_list);
	lock->active = true;
	gp_hif_rtpm_ctx->prevent_cnt++;
	gp_hif_rtpm_ctx->stats.prevent_suspend++;
	return ret;
}

/**
 * __hif_pm_runtime_allow_suspend() - Allow Runtime suspend
 * @lock: runtime pm lock
 *
 * This function will allow runtime suspend, by decrementing
 * device's usage count.
 *
 * Return: status
 */
static int __hif_pm_runtime_allow_suspend(struct hif_pm_runtime_lock *lock)
{
	int ret = 0;
	int usage_count;

	if (gp_hif_rtpm_ctx->prevent_cnt == 0 || !lock->active)
		return ret;

	usage_count = hif_rtpm_read_usage_count();
	/*
	 * For runtime PM enabled case, the usage count should never be 0
	 * at this point. For runtime PM disabled case, it should never be
	 * 2 at this point. Catch unexpected PUT without GET here.
	 */
	if (usage_count == 2 && !gp_hif_rtpm_ctx->enable_rpm) {
		hif_err("Unexpected PUT when runtime PM is disabled");
		QDF_BUG(0);
		return QDF_STATUS_E_CANCELED;
	} else if (!usage_count) {
		hif_info_high("Put without a Get operation, %s", lock->name);
		return QDF_STATUS_E_CANCELED;
	}

	hif_rtpm_mark_last_busy(HIF_RTPM_ID_RESERVED);
	ret = __hif_rtpm_put_auto(gp_hif_rtpm_ctx->dev);

	list_del(&lock->list);
	lock->active = false;
	gp_hif_rtpm_ctx->prevent_cnt--;
	gp_hif_rtpm_ctx->stats.allow_suspend++;
	return ret;
}

int hif_pm_runtime_prevent_suspend(struct hif_pm_runtime_lock *lock)
{
	if (!hif_rtpm_enabled() || !lock)
		return -EINVAL;

	if (in_irq())
		WARN_ON(1);

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	__hif_pm_runtime_prevent_suspend(lock);
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);

	if (qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state) >=
		HIF_RTPM_STATE_SUSPENDING)
		hif_info_high("request RTPM resume by %s",
			      lock->name);

	return 0;
}

/**
 * __hif_pm_runtime_prevent_suspend_sync() - synchronized prevent runtime
 *  suspend for a protocol reason
 * @lock: runtime_pm lock being acquired
 *
 * Return: 0 if successful.
 */
static
int __hif_pm_runtime_prevent_suspend_sync(struct hif_pm_runtime_lock *lock)
{
	int ret = 0;

	if (lock->active)
		return 0;

	ret = __hif_rtpm_get_sync(gp_hif_rtpm_ctx->dev);

	/**
	 * The ret can be -EINPROGRESS, if Runtime status is RPM_RESUMING or
	 * RPM_SUSPENDING. Any other negative value is an error.
	 * We shouldn't do runtime_put here as in later point allow
	 * suspend gets called with the context and there the usage count
	 * is decremented, so suspend will be prevented.
	 */
	if (ret < 0 && ret != -EINPROGRESS) {
		gp_hif_rtpm_ctx->stats.runtime_get_err++;
		hif_err("pm_state: %d ret: %d",
			qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state),
			ret);
	}

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	list_add_tail(&lock->list, &gp_hif_rtpm_ctx->prevent_list);
	lock->active = true;
	gp_hif_rtpm_ctx->prevent_cnt++;
	gp_hif_rtpm_ctx->stats.prevent_suspend++;
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);

	return ret;
}

int hif_pm_runtime_prevent_suspend_sync(struct hif_pm_runtime_lock *lock)
{
	if (!hif_rtpm_enabled())
		return 0;

	if (!lock)
		return -EINVAL;

	if (in_irq())
		WARN_ON(1);

	__hif_pm_runtime_prevent_suspend_sync(lock);

	if (qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state) >=
		HIF_RTPM_STATE_SUSPENDING)
		hif_info_high("request RTPM resume by %s",
			      lock->name);

	return 0;
}

int hif_pm_runtime_allow_suspend(struct hif_pm_runtime_lock *lock)
{
	if (!hif_rtpm_enabled())
		return 0;

	if (!lock)
		return -EINVAL;

	if (in_irq())
		WARN_ON(1);

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);
	__hif_pm_runtime_allow_suspend(lock);
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->prevent_list_lock);

	return 0;
}

QDF_STATUS hif_rtpm_sync_resume(void)
{
	struct device *dev;
	int pm_state;
	int ret;

	if (!hif_rtpm_enabled())
		return 0;

	dev = gp_hif_rtpm_ctx->dev;
	pm_state = qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state);

	ret = __hif_rtpm_resume(dev);
	__hif_rtpm_mark_last_busy(dev);

	if (ret >= 0) {
		gp_hif_rtpm_ctx->stats.resume_count++;
		gp_hif_rtpm_ctx->stats.resume_ts = qdf_get_log_timestamp();
		gp_hif_rtpm_ctx->stats.last_busy_ts =
					gp_hif_rtpm_ctx->stats.resume_ts;
		return QDF_STATUS_SUCCESS;
	}

	hif_err("pm_state: %d, err: %d", pm_state, ret);
	return QDF_STATUS_E_FAILURE;
}

void hif_rtpm_request_resume(void)
{
	__hif_rtpm_request_resume(gp_hif_rtpm_ctx->dev);
	hif_info_high("request RTPM resume %s", (char *)_RET_IP_);
}

void hif_rtpm_check_and_request_resume(void)
{
	hif_rtpm_suspend_lock();
	if (qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state) >=
			HIF_RTPM_STATE_SUSPENDING) {
		hif_rtpm_suspend_unlock();
		__hif_rtpm_request_resume(gp_hif_rtpm_ctx->dev);
		gp_hif_rtpm_ctx->stats.request_resume_ts =
						qdf_get_log_timestamp();
		gp_hif_rtpm_ctx->stats.request_resume_id = HIF_RTPM_ID_RESERVED;
	} else {
		__hif_rtpm_mark_last_busy(gp_hif_rtpm_ctx->dev);
		gp_hif_rtpm_ctx->stats.last_busy_ts = qdf_get_log_timestamp();
		hif_rtpm_suspend_unlock();
	}
}

int hif_rtpm_get_monitor_wake_intr(void)
{
	return qdf_atomic_read(&gp_hif_rtpm_ctx->monitor_wake_intr);
}

void hif_rtpm_set_monitor_wake_intr(int val)
{
	qdf_atomic_set(&gp_hif_rtpm_ctx->monitor_wake_intr, val);
}

void hif_rtpm_display_last_busy_hist(struct hif_opaque_softc *hif_ctx)
{
	struct hif_softc *scn;
	struct hif_rtpm_ctx *rtpm_ctx = gp_hif_rtpm_ctx;
	struct hif_rtpm_last_busy_hist *hist;
	unsigned long cur_idx;
	int i;

	scn = HIF_GET_SOFTC(hif_ctx);
	if (!scn)
		return;

	hif_info_high("RTPM last busy ts:%llu client:%s from:%ps",
		      rtpm_ctx->stats.last_busy_ts,
		      hif_rtpm_id_to_string(rtpm_ctx->stats.last_busy_id),
		      rtpm_ctx->stats.last_busy_marker);

	/*Display CE and DP clients RTPM stats*/
	for (i = 0; i < HIF_RTPM_ID_MAX; i++) {
		if (!rtpm_ctx->clients[i] ||
		    (i != HIF_RTPM_ID_CE && i != HIF_RTPM_ID_DP))
			continue;
		hif_info_high("RTPM client:%s busy_ts:%llu get_ts:%llu put_ts:%llu get_cnt:%d put_cnt:%d",
			      hif_rtpm_id_to_string(i),
			      rtpm_ctx->clients[i]->last_busy_ts,
			      rtpm_ctx->clients[i]->get_ts,
			      rtpm_ctx->clients[i]->put_ts,
			      qdf_atomic_read(&rtpm_ctx->clients[i]->get_count),
			      qdf_atomic_read(&rtpm_ctx->clients[i]->put_count));
	}

	for (i = 0; i < CE_COUNT_MAX; i++) {
		hist = gp_hif_rtpm_ctx->busy_hist[i];
		if (!hist)
			continue;
		cur_idx = hist->last_busy_idx;

		hif_info_high("RTPM CE-%u last busy_cnt:%lu cur_idx:%lu ts1:%llu ts2:%llu ts3:%llu ts4:%llu",
			      i, hist->last_busy_cnt, cur_idx,
			      hist->last_busy_ts[cur_idx & HIF_RTPM_BUSY_HIST_MASK],
			      hist->last_busy_ts[(cur_idx + 4) & HIF_RTPM_BUSY_HIST_MASK],
			      hist->last_busy_ts[(cur_idx + 8) & HIF_RTPM_BUSY_HIST_MASK],
			      hist->last_busy_ts[(cur_idx + 12) & HIF_RTPM_BUSY_HIST_MASK]);
	}
}

void hif_rtpm_record_ce_last_busy_evt(struct hif_softc *scn,
				      unsigned long ce_id)
{
	struct hif_rtpm_last_busy_hist *hist;
	unsigned long idx;

	if (!scn || !gp_hif_rtpm_ctx->busy_hist[ce_id])
		return;

	hist = gp_hif_rtpm_ctx->busy_hist[ce_id];
	hist->last_busy_cnt++;
	hist->last_busy_idx++;
	idx = hist->last_busy_idx & HIF_RTPM_BUSY_HIST_MASK;
	hist->last_busy_ts[idx] = qdf_get_log_timestamp();
}

void hif_rtpm_mark_last_busy(uint32_t id)
{
	__hif_rtpm_mark_last_busy(gp_hif_rtpm_ctx->dev);
	gp_hif_rtpm_ctx->stats.last_busy_ts = qdf_get_log_timestamp();
	gp_hif_rtpm_ctx->stats.last_busy_id = id;
	gp_hif_rtpm_ctx->stats.last_busy_marker = (void *)_RET_IP_;
	if (gp_hif_rtpm_ctx->clients[id]) {
		gp_hif_rtpm_ctx->clients[id]->last_busy_cnt++;
		gp_hif_rtpm_ctx->clients[id]->last_busy_ts =
					gp_hif_rtpm_ctx->stats.last_busy_ts;
	}
}

void hif_rtpm_set_client_job(uint32_t client_id)
{
	int pm_state;

	if (!gp_hif_rtpm_ctx->clients[client_id])
		return;

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
	pm_state = qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state);
	if (pm_state <= HIF_RTPM_STATE_RESUMING_LINKUP &&
	    gp_hif_rtpm_ctx->clients[client_id]->hif_rtpm_cbk)
		gp_hif_rtpm_ctx->clients[client_id]->hif_rtpm_cbk();
	else
		qdf_set_bit(client_id, &gp_hif_rtpm_ctx->pending_job);
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);
}

/**
 * hif_rtpm_pending_job() - continue jobs when bus resumed
 *
 * Return: Void
 */
static void hif_rtpm_pending_job(void)
{
	int i;

	for (i = 0; i < gp_hif_rtpm_ctx->client_count; i++) {
		if (qdf_test_and_clear_bit(i, &gp_hif_rtpm_ctx->pending_job)) {
			qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);
			if (gp_hif_rtpm_ctx->clients[i]->hif_rtpm_cbk)
				gp_hif_rtpm_ctx->clients[i]->hif_rtpm_cbk();
			qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
		}
	}
}

#define PREVENT_LIST_STRING_LEN 200

void hif_rtpm_print_prevent_list(void)
{
	struct hif_rtpm_client *client;
	struct hif_pm_runtime_lock *ctx;
	char *str_buf;
	int i, prevent_list_count, len = 0;

	str_buf = qdf_mem_malloc(PREVENT_LIST_STRING_LEN);
	if (!str_buf)
		return;

	qdf_spin_lock(&gp_hif_rtpm_ctx->prevent_list_lock);
	prevent_list_count = gp_hif_rtpm_ctx->prevent_cnt;
	if (prevent_list_count) {
		list_for_each_entry(ctx, &gp_hif_rtpm_ctx->prevent_list, list)
			len += qdf_scnprintf(str_buf + len,
				PREVENT_LIST_STRING_LEN - len,
				"%s ", ctx->name);
	}
	qdf_spin_unlock(&gp_hif_rtpm_ctx->prevent_list_lock);

	if (prevent_list_count)
		hif_info_high("prevent_suspend_cnt %u, prevent_list: %s",
			      prevent_list_count, str_buf);

	qdf_mem_free(str_buf);

	for (i = 0; i < HIF_RTPM_ID_MAX; i++) {
		client = gp_hif_rtpm_ctx->clients[i];
		if (client && qdf_atomic_read(&client->active_count))
			hif_info_high("client: %d: %s- active count: %d", i,
				      hif_rtpm_id_to_string(i),
				      qdf_atomic_read(&client->active_count));
	}
}

/**
 * hif_rtpm_is_suspend_allowed() - Reject suspend if client is active
 *
 * Return: True if no clients are active
 */
static bool hif_rtpm_is_suspend_allowed(void)
{
	if (!gp_hif_rtpm_ctx || !gp_hif_rtpm_ctx->enable_rpm)
		return false;

	if (!hif_rtpm_read_usage_count())
		return true;

	return false;
}

void hif_rtpm_suspend_lock(void)
{
	qdf_spin_lock_irqsave(&gp_hif_rtpm_ctx->runtime_suspend_lock);
}

void hif_rtpm_suspend_unlock(void)
{
	qdf_spin_unlock_irqrestore(&gp_hif_rtpm_ctx->runtime_suspend_lock);
}

/**
 * hif_rtpm_set_state(): utility function
 * @state: state to set
 *
 * Return: Void
 */
static inline
void hif_rtpm_set_state(enum hif_rtpm_state state)
{
	qdf_atomic_set(&gp_hif_rtpm_ctx->pm_state, state);
}

int hif_rtpm_get_state(void)
{
	return qdf_atomic_read(&gp_hif_rtpm_ctx->pm_state);
}

int hif_pre_runtime_suspend(struct hif_opaque_softc *hif_ctx)
{
	if (!hif_can_suspend_link(hif_ctx)) {
		hif_err("Runtime PM not supported for link up suspend");
		return -EINVAL;
	}

	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
	hif_rtpm_set_state(HIF_RTPM_STATE_SUSPENDING);

	/* keep this after set suspending */
	if (!hif_rtpm_is_suspend_allowed()) {
		qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);
		hif_rtpm_print_prevent_list();
		gp_hif_rtpm_ctx->stats.suspend_err_count++;
		gp_hif_rtpm_ctx->stats.suspend_err_ts = qdf_get_log_timestamp();
		hif_info_high("Runtime PM not allowed now");
		return -EINVAL;
	}

	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);

	return QDF_STATUS_SUCCESS;
}

void hif_process_runtime_suspend_success(void)
{
	hif_rtpm_set_state(HIF_RTPM_STATE_SUSPENDED);
	gp_hif_rtpm_ctx->stats.suspend_count++;
	gp_hif_rtpm_ctx->stats.suspend_ts = qdf_get_log_timestamp();
}

void hif_process_runtime_suspend_failure(void)
{
	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
	hif_rtpm_set_state(HIF_RTPM_STATE_ON);
	hif_rtpm_pending_job();
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);

	gp_hif_rtpm_ctx->stats.suspend_err_count++;
	gp_hif_rtpm_ctx->stats.suspend_err_ts = qdf_get_log_timestamp();
	gp_hif_rtpm_ctx->stats.last_busy_ts = qdf_get_log_timestamp();
	hif_rtpm_mark_last_busy(HIF_RTPM_ID_RESERVED);
}

void hif_pre_runtime_resume(void)
{
	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
	hif_rtpm_set_monitor_wake_intr(0);
	hif_rtpm_set_state(HIF_RTPM_STATE_RESUMING);
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);
}

void hif_process_runtime_resume_linkup(void)
{
	qdf_spin_lock_bh(&gp_hif_rtpm_ctx->runtime_lock);
	hif_rtpm_set_state(HIF_RTPM_STATE_RESUMING_LINKUP);
	hif_rtpm_pending_job();
	qdf_spin_unlock_bh(&gp_hif_rtpm_ctx->runtime_lock);
}

void hif_process_runtime_resume_success(void)
{
	hif_rtpm_set_state(HIF_RTPM_STATE_ON);
	gp_hif_rtpm_ctx->stats.resume_count++;
	gp_hif_rtpm_ctx->stats.resume_ts = qdf_get_log_timestamp();
	gp_hif_rtpm_ctx->stats.last_busy_ts = gp_hif_rtpm_ctx->stats.resume_ts;
	hif_rtpm_mark_last_busy(HIF_RTPM_ID_RESERVED);
}

int hif_runtime_suspend(struct hif_opaque_softc *hif_ctx)
{
	int errno;

	errno = hif_bus_suspend(hif_ctx);
	if (errno) {
		hif_err("Failed bus suspend: %d", errno);
		return errno;
	}

	hif_rtpm_set_monitor_wake_intr(1);

	errno = hif_bus_suspend_noirq(hif_ctx);
	if (errno) {
		hif_err("Failed bus suspend noirq: %d", errno);
		hif_rtpm_set_monitor_wake_intr(0);
		goto bus_resume;
	}

	return 0;

bus_resume:
	QDF_BUG(!hif_bus_resume(hif_ctx));

	return errno;
}

int hif_runtime_resume(struct hif_opaque_softc *hif_ctx)
{
	int errno;

	QDF_BUG(!hif_bus_resume_noirq(hif_ctx));
	errno = hif_bus_resume(hif_ctx);
	if (errno)
		hif_err("Failed runtime resume: %d", errno);

	return errno;
}

void hif_fastpath_resume(struct hif_opaque_softc *hif_ctx)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct CE_state *ce_state;

	if (!scn)
		return;

	if (scn->fastpath_mode_on) {
		if (Q_TARGET_ACCESS_BEGIN(scn) < 0)
			return;

		ce_state = scn->ce_id_to_state[CE_HTT_H2T_MSG];
		qdf_spin_lock_bh(&ce_state->ce_index_lock);

		/*war_ce_src_ring_write_idx_set */
		CE_SRC_RING_WRITE_IDX_SET(scn, ce_state->ctrl_addr,
					  ce_state->src_ring->write_index);
		qdf_spin_unlock_bh(&ce_state->ce_index_lock);
		Q_TARGET_ACCESS_END(scn);
	}
}
#endif /* FEATURE_RUNTIME_PM */

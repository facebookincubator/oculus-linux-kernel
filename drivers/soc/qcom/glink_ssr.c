/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/ipc_logging.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/random.h>
#include <soc/qcom/glink.h>
#include <soc/qcom/subsystem_notif.h>
#include "glink_private.h"

#define GLINK_SSR_REPLY_TIMEOUT	HZ
#define GLINK_SSR_INTENT_REQ_TIMEOUT_MS 500
#define GLINK_SSR_EVENT_INIT ~0
#define NUM_LOG_PAGES 3

#define GLINK_SSR_PRIORITY 1
#define GLINK_SSR_LOG(x...) do { \
	if (glink_ssr_log_ctx) \
		ipc_log_string(glink_ssr_log_ctx, x); \
} while (0)

#define GLINK_SSR_ERR(x...) do { \
	pr_err(x); \
	GLINK_SSR_LOG(x); \
} while (0)

static void *glink_ssr_log_ctx;

/* Global restart counter */
static uint32_t sequence_number;

/* Flag indicating if responses were received for all SSR notifications */
static bool notifications_successful;

/* Completion for setting notifications_successful */
struct completion notifications_successful_complete;

/**
 * struct restart_notifier_block - restart notifier wrapper structure
 * subsystem:	the name of the subsystem as recognized by the SSR framework
 * nb:		notifier block structure used by the SSR framework
 */
struct restart_notifier_block {
	const char *subsystem;
	struct notifier_block nb;
};

/**
 * struct configure_and_open_ch_work - Work structure for used for opening
 *				glink_ssr channels
 * edge:	The G-Link edge obtained from the link state callback
 * transport:	The G-Link transport obtained from the link state callback
 * link_state:	The link state obtained from the link state callback
 * ss_info:	Subsystem information structure containing the info for this
 *		callback
 * work:	Work structure
 */
struct configure_and_open_ch_work {
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
	enum glink_link_state link_state;
	struct subsys_info *ss_info;
	struct work_struct work;
};

/**
 * struct rx_done_ch_work - Work structure used for sending rx_done on
 *				glink_ssr channels
 * handle:	G-Link channel handle to be used for sending rx_done
 * ptr:		Intent pointer data provided in notify rx function
 * work:	Work structure
 */
struct rx_done_ch_work {
	void *handle;
	const void *ptr;
	struct work_struct work;
};

/**
 * struct close_ch_work - Work structure for used for closing glink_ssr channels
 * edge:	The G-Link edge name for the channel being closed
 * handle:	G-Link channel handle to be closed
 * work:	Work structure
 */
struct close_ch_work {
	char edge[GLINK_NAME_SIZE];
	void *handle;
	struct work_struct work;
};

static int glink_ssr_restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data);
static void delete_ss_info_notify_list(struct subsys_info *ss_info);
static int configure_and_open_channel(struct subsys_info *ss_info);
static struct workqueue_struct *glink_ssr_wq;

static LIST_HEAD(subsystem_list);
static atomic_t responses_remaining = ATOMIC_INIT(0);
static wait_queue_head_t waitqueue;

/**
 * cb_data_release() - Free cb_data and set to NULL
 * @kref_ptr:	pointer to kref.
 *
 * This function releses cb_data.
 */
static inline void cb_data_release(struct kref *kref_ptr)
{
	struct ssr_notify_data *cb_data;

	cb_data = container_of(kref_ptr, struct ssr_notify_data, cb_kref);
	kfree(cb_data);
}

/**
 * check_and_get_cb_data() - Try to get reference to kref of cb_data
 * @ss_info:	pointer to subsystem info structure.
 *
 * Return: NULL is cb_data is NULL, pointer to cb_data otherwise
 */
static struct ssr_notify_data *check_and_get_cb_data(
					struct subsys_info *ss_info)
{
	struct ssr_notify_data *cb_data;
	unsigned long flags;

	spin_lock_irqsave(&ss_info->cb_lock, flags);
	if (ss_info->cb_data == NULL) {
		GLINK_SSR_LOG("<SSR> %s: cb_data is NULL\n", __func__);
		spin_unlock_irqrestore(&ss_info->cb_lock, flags);
		return 0;
	}
	kref_get(&ss_info->cb_data->cb_kref);
	cb_data = ss_info->cb_data;
	spin_unlock_irqrestore(&ss_info->cb_lock, flags);
	return cb_data;
}

static void rx_done_cb_worker(struct work_struct *work)
{
	struct rx_done_ch_work *rx_done_work =
		container_of(work, struct rx_done_ch_work, work);

	glink_rx_done(rx_done_work->handle, rx_done_work->ptr, false);
	kfree(rx_done_work);
}

static void link_state_cb_worker(struct work_struct *work)
{
	unsigned long flags;
	struct configure_and_open_ch_work *ch_open_work =
		container_of(work, struct configure_and_open_ch_work, work);
	struct subsys_info *ss_info = ch_open_work->ss_info;

	GLINK_SSR_LOG("<SSR> %s: LINK STATE[%d] %s:%s\n", __func__,
			ch_open_work->link_state, ch_open_work->edge,
			ch_open_work->transport);

	if (ss_info && ch_open_work->link_state == GLINK_LINK_STATE_UP) {
		spin_lock_irqsave(&ss_info->link_up_lock, flags);
		if (!ss_info->link_up) {
			ss_info->link_up = true;
			spin_unlock_irqrestore(&ss_info->link_up_lock, flags);
			if (!configure_and_open_channel(ss_info)) {
				glink_unregister_link_state_cb(
						ss_info->link_state_handle);
				ss_info->link_state_handle = NULL;
			}
			kfree(ch_open_work);
			return;
		}
		spin_unlock_irqrestore(&ss_info->link_up_lock, flags);
	} else {
		if (ss_info) {
			spin_lock_irqsave(&ss_info->link_up_lock, flags);
			ss_info->link_up = false;
			spin_unlock_irqrestore(&ss_info->link_up_lock, flags);
			ss_info->handle = NULL;
		} else {
			GLINK_SSR_ERR("<SSR> %s: ss_info is NULL\n", __func__);
		}
	}

	kfree(ch_open_work);
}

/**
 * glink_lbsrv_link_state_cb() - Callback to receive link state updates
 * @cb_info:	Information containing link & its state.
 * @priv:	Private data passed during the link state registration.
 *
 * This function is called by the G-Link core to notify the glink_ssr module
 * regarding the link state updates. This function is registered with the
 * G-Link core by the loopback server during glink_register_link_state_cb().
 */
static void glink_ssr_link_state_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv)
{
	struct subsys_info *ss_info;
	struct configure_and_open_ch_work *open_ch_work;

	if (!cb_info) {
		GLINK_SSR_ERR("<SSR> %s: Missing cb_data\n", __func__);
		return;
	}

	ss_info = get_info_for_edge(cb_info->edge);

	open_ch_work = kmalloc(sizeof(*open_ch_work), GFP_KERNEL);
	if (!open_ch_work) {
		GLINK_SSR_ERR("<SSR> %s: Could not allocate open_ch_work\n",
				__func__);
		return;
	}

	strlcpy(open_ch_work->edge, cb_info->edge, GLINK_NAME_SIZE);
	strlcpy(open_ch_work->transport, cb_info->transport, GLINK_NAME_SIZE);
	open_ch_work->link_state = cb_info->link_state;
	open_ch_work->ss_info = ss_info;

	INIT_WORK(&open_ch_work->work, link_state_cb_worker);
	queue_work(glink_ssr_wq, &open_ch_work->work);
}

/**
 * glink_ssr_notify_rx() - RX Notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @pkt_priv:	Private packet data
 * @ptr:	Pointer to the data received
 * @size:	Size of the data received
 *
 * This function is a notification callback from the G-Link core that data
 * has been received from the remote side. This data is validate to make
 * sure it is a cleanup_done message and is processed accordingly if it is.
 */
void glink_ssr_notify_rx(void *handle, const void *priv, const void *pkt_priv,
		const void *ptr, size_t size)
{
	struct do_cleanup_msg *do_cleanup_data =
				(struct do_cleanup_msg *)pkt_priv;
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;
	struct cleanup_done_msg *resp = (struct cleanup_done_msg *)ptr;
	struct rx_done_ch_work *rx_done_work;

	rx_done_work = kmalloc(sizeof(*rx_done_work), GFP_ATOMIC);
	if (!rx_done_work) {
		GLINK_SSR_ERR("<SSR> %s: Could not allocate rx_done_work\n",
				__func__);
		return;
	}
	if (unlikely(!do_cleanup_data)) {
		GLINK_SSR_ERR("<SSR> %s: Missing do_cleanup data!\n", __func__);
		goto no_clean_done;
	}
	if (unlikely(!cb_data)) {
		GLINK_SSR_ERR("<SSR> %s: Missing cb_data!\n", __func__);
		goto done;
	}
	if (unlikely(!resp)) {
		GLINK_SSR_ERR("<SSR> %s: Missing response data\n", __func__);
		goto done;
	}
	if (unlikely(resp->version != do_cleanup_data->version)) {
		GLINK_SSR_ERR("<SSR> %s: Version mismatch. %s[%d], %s[%d]\n",
			__func__, "do_cleanup version",
			do_cleanup_data->version, "cleanup_done version",
			resp->version);
		goto done;
	}
	if (unlikely(resp->seq_num != do_cleanup_data->seq_num)) {
		GLINK_SSR_ERR("<SSR> %s: Invalid seq. number %s[%d], %s[%d]\n",
			__func__, "do_cleanup seq num",
			do_cleanup_data->seq_num,
			"cleanup_done seq_num", resp->seq_num);
		goto done;
	}
	if (unlikely(resp->response != GLINK_SSR_CLEANUP_DONE)) {
		GLINK_SSR_ERR("<SSR> %s: Not a cleaup_done message. %s[%d]\n",
			__func__, "cleanup_done response", resp->response);
		goto done;
	}

	cb_data->responded = true;
	atomic_dec(&responses_remaining);

	GLINK_SSR_LOG(
		"<SSR> %s: Response from %s resp[%d] version[%d] seq_num[%d] restarted[%s]\n",
			__func__, cb_data->edge, resp->response,
			resp->version, resp->seq_num,
			do_cleanup_data->name);
done:
	kfree(do_cleanup_data);
no_clean_done:
	rx_done_work->ptr = ptr;
	rx_done_work->handle = handle;
	INIT_WORK(&rx_done_work->work, rx_done_cb_worker);
	queue_work(glink_ssr_wq, &rx_done_work->work);
	wake_up(&waitqueue);
	return;
}

/**
 * glink_ssr_notify_tx_done() - Transmit finished notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @pkt_priv:	Private packet data
 * @ptr:	Pointer to the data received
 *
 * This function is a notification callback from the G-Link core that data
 * we sent has finished transmitting.
 */
void glink_ssr_notify_tx_done(void *handle, const void *priv,
		const void *pkt_priv, const void *ptr)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;

	if (unlikely(!cb_data)) {
		panic("%s: cb_data is NULL!\n", __func__);
		return;
	}

	GLINK_SSR_LOG("<SSR> %s: Notified %s of restart\n",
		__func__, cb_data->edge);

	cb_data->tx_done = true;
}

void close_ch_worker(struct work_struct *work)
{
	unsigned long flags;
	void *link_state_handle;
	struct subsys_info *ss_info;
	struct close_ch_work *close_work =
		container_of(work, struct close_ch_work, work);

	glink_close(close_work->handle);

	ss_info = get_info_for_edge(close_work->edge);
	if (WARN_ON(!ss_info))
		return;

	spin_lock_irqsave(&ss_info->link_up_lock, flags);
	ss_info->link_up = false;
	spin_unlock_irqrestore(&ss_info->link_up_lock, flags);

	if (WARN_ON(ss_info->link_state_handle != NULL))
		return;
	link_state_handle = glink_register_link_state_cb(ss_info->link_info,
			NULL);

	if (IS_ERR_OR_NULL(link_state_handle))
		GLINK_SSR_ERR("<SSR> %s: %s, ret[%d]\n", __func__,
				"Couldn't register link state cb",
				(int)PTR_ERR(link_state_handle));
	else
		ss_info->link_state_handle = link_state_handle;

	if (WARN_ON(!ss_info->cb_data))
		return;
	spin_lock_irqsave(&ss_info->cb_lock, flags);
	kref_put(&ss_info->cb_data->cb_kref, cb_data_release);
	ss_info->cb_data = NULL;
	spin_unlock_irqrestore(&ss_info->cb_lock, flags);
	kfree(close_work);
}

/**
 * glink_ssr_notify_state() - Channel state notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @event:	The state that has been transitioned to
 *
 * This function is a notification callback from the G-Link core that the
 * channel state has changed.
 */
void glink_ssr_notify_state(void *handle, const void *priv, unsigned int event)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;
	struct close_ch_work *close_work;

	if (!cb_data) {
		GLINK_SSR_ERR("<SSR> %s: Could not allocate data for cb_data\n",
				__func__);
	} else {
		GLINK_SSR_LOG("<SSR> %s: event[%d]\n",
				__func__, event);
		cb_data->event = event;
		if (event == GLINK_REMOTE_DISCONNECTED) {
			close_work =
				kmalloc(sizeof(struct close_ch_work),
						GFP_KERNEL);
			if (!close_work) {
				GLINK_SSR_ERR(
					"<SSR> %s: Could not allocate %s\n",
						__func__, "close work");
				return;
			}

			strlcpy(close_work->edge, cb_data->edge,
					sizeof(close_work->edge));
			close_work->handle = handle;
			INIT_WORK(&close_work->work, close_ch_worker);
			queue_work(glink_ssr_wq, &close_work->work);
		}
	}
}

/**
 * glink_ssr_notify_rx_intent_req() - RX intent request notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @req_size:	The size of the requested intent
 *
 * This function is a notification callback from the G-Link core of the remote
 * side's request for an RX intent to be queued.
 *
 * Return: Boolean indicating whether or not the request was successfully
 *         received
 */
bool glink_ssr_notify_rx_intent_req(void *handle, const void *priv,
		size_t req_size)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;

	if (!cb_data) {
		GLINK_SSR_ERR("<SSR> %s: Could not allocate data for cb_data\n",
				__func__);
		return false;
	}
	GLINK_SSR_LOG("<SSR> %s: rx_intent_req of size %zu\n",
			__func__, req_size);
	return true;
}

/**
 * glink_ssr_restart_notifier_cb() - SSR restart notifier callback function
 * @this:	Notifier block used by the SSR framework
 * @code:	The SSR code for which stage of restart is occurring
 * @data:	Structure containing private data - not used here.
 *
 * This function is a callback for the SSR framework. From here we initiate
 * our handling of SSR.
 *
 * Return: Status of SSR handling
 */
static int glink_ssr_restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	int ret = 0;
	struct subsys_info *ss_info = NULL;
	struct restart_notifier_block *notifier =
		container_of(this, struct restart_notifier_block, nb);

	if (code == SUBSYS_AFTER_SHUTDOWN) {
		GLINK_SSR_LOG("<SSR> %s: %s: subsystem restart for %s\n",
				__func__, "SUBSYS_AFTER_SHUTDOWN",
				notifier->subsystem);
		ss_info = get_info_for_subsystem(notifier->subsystem);
		if (ss_info == NULL) {
			GLINK_SSR_ERR("<SSR> %s: ss_info is NULL\n", __func__);
			return -EINVAL;
		}

		glink_ssr(ss_info->edge);
		ret = notify_for_subsystem(ss_info);

		if (ret) {
			GLINK_SSR_ERR("<SSR>: %s: %s, ret[%d]\n", __func__,
					"Subsystem notification failed", ret);
			return ret;
		}
	} else if (code == SUBSYS_AFTER_POWERUP) {
		GLINK_SSR_LOG("<SSR> %s: %s: subsystem restart for %s\n",
				__func__, "SUBSYS_AFTER_POWERUP",
				notifier->subsystem);
		ss_info = get_info_for_subsystem(notifier->subsystem);
		if (ss_info == NULL) {
			GLINK_SSR_ERR("<SSR> %s: ss_info is NULL\n", __func__);
			return -EINVAL;
		}

		glink_subsys_up(ss_info->edge);
	}
	return NOTIFY_DONE;
}

/**
 * notify for subsystem() - Notify other subsystems that a subsystem is being
 *                          restarted
 * @ss_info:	Subsystem info structure for the subsystem being restarted
 *
 * This function sends notifications to affected subsystems that the subsystem
 * in ss_info is being restarted, and waits for the cleanup done response from
 * all of those subsystems. It also initiates any local cleanup that is
 * necessary.
 *
 * Return: 0 on success, standard error codes otherwise
 */
int notify_for_subsystem(struct subsys_info *ss_info)
{
	struct subsys_info *ss_info_channel;
	struct subsys_info_leaf *ss_leaf_entry;
	struct do_cleanup_msg *do_cleanup_data;
	void *handle;
	int wait_ret;
	int ret;
	unsigned long flags;

	if (!ss_info) {
		GLINK_SSR_ERR("<SSR> %s: ss_info structure invalid\n",
				__func__);
		return -EINVAL;
	}

	/*
	 * No locking is needed here because ss_info->notify_list_len is
	 * only modified during setup.
	 */
	atomic_set(&responses_remaining, ss_info->notify_list_len);
	notifications_successful = true;

	list_for_each_entry(ss_leaf_entry, &ss_info->notify_list,
			notify_list_node) {
		GLINK_SSR_LOG(
			"<SSR> %s: Notifying: %s:%s of %s restart, seq_num[%d]\n",
				__func__, ss_leaf_entry->edge,
				ss_leaf_entry->xprt, ss_info->edge,
				sequence_number);

		ss_info_channel =
			get_info_for_subsystem(ss_leaf_entry->ssr_name);
		if (ss_info_channel == NULL) {
			GLINK_SSR_ERR(
				"<SSR> %s: unable to find subsystem name\n",
					__func__);
			return -ENODEV;
		}
		handle = ss_info_channel->handle;
		ss_leaf_entry->cb_data = check_and_get_cb_data(
							ss_info_channel);
		if (!ss_leaf_entry->cb_data) {
			GLINK_SSR_LOG("<SSR> %s: CB data is NULL\n", __func__);
			atomic_dec(&responses_remaining);
			continue;
		}

		spin_lock_irqsave(&ss_info->link_up_lock, flags);
		if (IS_ERR_OR_NULL(ss_info_channel->handle) ||
				!ss_info_channel->link_up ||
				ss_leaf_entry->cb_data->event
						!= GLINK_CONNECTED) {

			GLINK_SSR_LOG(
				"<SSR> %s: %s:%s %s[%d], %s[%p], %s[%d]\n",
				__func__, ss_leaf_entry->edge, "Not connected",
				"resp. remaining",
				atomic_read(&responses_remaining), "handle",
				ss_info_channel->handle, "link_up",
				ss_info_channel->link_up);

			spin_unlock_irqrestore(&ss_info->link_up_lock, flags);
			atomic_dec(&responses_remaining);
			kref_put(&ss_leaf_entry->cb_data->cb_kref,
							cb_data_release);
			continue;
		}
		spin_unlock_irqrestore(&ss_info->link_up_lock, flags);

		do_cleanup_data = kmalloc(sizeof(struct do_cleanup_msg),
				GFP_KERNEL);
		if (!do_cleanup_data) {
			GLINK_SSR_ERR(
				"%s %s: Could not allocate do_cleanup_msg\n",
				"<SSR>", __func__);
			kref_put(&ss_leaf_entry->cb_data->cb_kref,
							cb_data_release);
			return -ENOMEM;
		}

		do_cleanup_data->version = 0;
		do_cleanup_data->command = GLINK_SSR_DO_CLEANUP;
		do_cleanup_data->seq_num = sequence_number;
		do_cleanup_data->name_len = strlen(ss_info->edge);
		strlcpy(do_cleanup_data->name, ss_info->edge,
				do_cleanup_data->name_len + 1);

		if (strcmp(ss_leaf_entry->ssr_name, "rpm"))
			ret = glink_tx(handle, do_cleanup_data,
					do_cleanup_data,
					sizeof(*do_cleanup_data),
					GLINK_TX_REQ_INTENT);
		else
			ret = glink_tx(handle, do_cleanup_data,
					do_cleanup_data,
					sizeof(*do_cleanup_data),
					GLINK_TX_SINGLE_THREADED);

		if (ret) {
			GLINK_SSR_ERR("<SSR> %s: tx failed, ret[%d], %s[%d]\n",
					__func__, ret, "resp. remaining",
					atomic_read(&responses_remaining));
			kfree(do_cleanup_data);

			if (!strcmp(ss_leaf_entry->ssr_name, "rpm"))
				panic("%s: glink_tx() to RPM failed!\n",
						__func__);
			atomic_dec(&responses_remaining);
			kref_put(&ss_leaf_entry->cb_data->cb_kref,
							cb_data_release);
			continue;
		}
		ret = glink_queue_rx_intent(handle, do_cleanup_data,
				sizeof(struct cleanup_done_msg));
		if (ret) {
			GLINK_SSR_ERR(
				"%s %s: %s, ret[%d], resp. remaining[%d]\n",
				"<SSR>", __func__,
				"queue_rx_intent failed", ret,
				atomic_read(&responses_remaining));
			kfree(do_cleanup_data);

			if (!strcmp(ss_leaf_entry->ssr_name, "rpm"))
				panic("%s: Could not queue intent for RPM!\n",
						__func__);
			atomic_dec(&responses_remaining);
			kref_put(&ss_leaf_entry->cb_data->cb_kref,
							cb_data_release);
			continue;
		}
		sequence_number++;
		kref_put(&ss_leaf_entry->cb_data->cb_kref, cb_data_release);
	}

	wait_ret = wait_event_timeout(waitqueue,
			atomic_read(&responses_remaining) == 0,
			GLINK_SSR_REPLY_TIMEOUT);

	list_for_each_entry(ss_leaf_entry, &ss_info->notify_list,
			notify_list_node) {
		ss_info_channel =
			get_info_for_subsystem(ss_leaf_entry->ssr_name);
		if (ss_info_channel == NULL) {
			GLINK_SSR_ERR(
				"<SSR> %s: unable to find subsystem name\n",
					__func__);
			continue;
		}

		ss_leaf_entry->cb_data = check_and_get_cb_data(
							ss_info_channel);
		if (!ss_leaf_entry->cb_data) {
			GLINK_SSR_LOG("<SSR> %s: CB data is NULL\n", __func__);
			continue;
		}
		if (!wait_ret && !IS_ERR_OR_NULL(ss_leaf_entry->cb_data)
				&& !ss_leaf_entry->cb_data->responded) {
			GLINK_SSR_ERR("%s %s: Subsystem %s %s\n",
				"<SSR>", __func__, ss_leaf_entry->edge,
				"failed to respond. Restarting.");

			notifications_successful = false;

			/* Check for RPM, as it can't be restarted */
			if (!strcmp(ss_leaf_entry->ssr_name, "rpm"))
				panic("%s: RPM failed to respond!\n", __func__);
		}
		if (!IS_ERR_OR_NULL(ss_leaf_entry->cb_data))
			ss_leaf_entry->cb_data->responded = false;
		kref_put(&ss_leaf_entry->cb_data->cb_kref, cb_data_release);
	}
	complete(&notifications_successful_complete);
	return 0;
}
EXPORT_SYMBOL(notify_for_subsystem);

/**
 * configure_and_open_channel() - configure and open a G-Link channel for
 *                                the given subsystem
 * @ss_info:	The subsys_info structure where the channel will be stored
 *
 * Return: 0 on success, standard error codes otherwise
 */
static int configure_and_open_channel(struct subsys_info *ss_info)
{
	struct glink_open_config open_cfg;
	struct ssr_notify_data *cb_data = NULL;
	void *handle = NULL;
	unsigned long flags;

	if (!ss_info) {
		GLINK_SSR_ERR("<SSR> %s: ss_info structure invalid\n",
				__func__);
		return -EINVAL;
	}

	cb_data = kmalloc(sizeof(struct ssr_notify_data), GFP_KERNEL);
	if (!cb_data) {
		GLINK_SSR_ERR("<SSR> %s: Could not allocate cb_data\n",
				__func__);
		return -ENOMEM;
	}
	cb_data->responded = false;
	cb_data->event = GLINK_SSR_EVENT_INIT;
	cb_data->edge = ss_info->edge;
	spin_lock_irqsave(&ss_info->cb_lock, flags);
	ss_info->cb_data = cb_data;
	kref_init(&cb_data->cb_kref);
	spin_unlock_irqrestore(&ss_info->cb_lock, flags);

	memset(&open_cfg, 0, sizeof(struct glink_open_config));

	if (ss_info->xprt) {
		open_cfg.transport = ss_info->xprt;
	} else {
		open_cfg.transport = NULL;
		open_cfg.options = GLINK_OPT_INITIAL_XPORT;
	}
	open_cfg.edge = ss_info->edge;
	open_cfg.name = "glink_ssr";
	open_cfg.notify_rx = glink_ssr_notify_rx;
	open_cfg.notify_tx_done = glink_ssr_notify_tx_done;
	open_cfg.notify_state = glink_ssr_notify_state;
	open_cfg.notify_rx_intent_req = glink_ssr_notify_rx_intent_req;
	open_cfg.priv = ss_info->cb_data;
	open_cfg.rx_intent_req_timeout_ms = GLINK_SSR_INTENT_REQ_TIMEOUT_MS;

	handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(handle)) {
		GLINK_SSR_ERR(
			"<SSR> %s:%s %s: unable to open channel, ret[%d]\n",
				 open_cfg.edge, open_cfg.name, __func__,
				 (int)PTR_ERR(handle));
		kfree(cb_data);
		cb_data = NULL;
		ss_info->cb_data = NULL;
		return PTR_ERR(handle);
	}
	ss_info->handle = handle;
	return 0;
}

/**
 * get_info_for_subsystem() - Retrieve information about a subsystem from the
 *                            global subsystem_info_list
 * @subsystem:	The name of the subsystem recognized by the SSR
 *		framework
 *
 * Return: subsys_info structure containing info for the requested subsystem;
 *         NULL if no structure can be found for the requested subsystem
 */
struct subsys_info *get_info_for_subsystem(const char *subsystem)
{
	struct subsys_info *ss_info_entry;

	list_for_each_entry(ss_info_entry, &subsystem_list,
			subsystem_list_node) {
		if (!strcmp(subsystem, ss_info_entry->ssr_name))
			return ss_info_entry;
	}

	return NULL;
}
EXPORT_SYMBOL(get_info_for_subsystem);

/**
 * get_info_for_edge() - Retrieve information about a subsystem from the
 *                       global subsystem_info_list
 * @edge:	The name of the edge recognized by G-Link
 *
 * Return: subsys_info structure containing info for the requested subsystem;
 *         NULL if no structure can be found for the requested subsystem
 */
struct subsys_info *get_info_for_edge(const char *edge)
{
	struct subsys_info *ss_info_entry;

	list_for_each_entry(ss_info_entry, &subsystem_list,
			subsystem_list_node) {
		if (!strcmp(edge, ss_info_entry->edge))
			return ss_info_entry;
	}

	return NULL;
}
EXPORT_SYMBOL(get_info_for_edge);

/**
 * glink_ssr_get_seq_num() - Get the current SSR sequence number
 *
 * Return: The current SSR sequence number
 */
uint32_t glink_ssr_get_seq_num(void)
{
	return sequence_number;
}
EXPORT_SYMBOL(glink_ssr_get_seq_num);

/**
 * delete_ss_info_notify_list() - Delete the notify list for a subsystem
 * @ss_info:	The subsystem info structure
 */
static void delete_ss_info_notify_list(struct subsys_info *ss_info)
{
	struct subsys_info_leaf *leaf, *temp;

	list_for_each_entry_safe(leaf, temp, &ss_info->notify_list,
			notify_list_node) {
		list_del(&leaf->notify_list_node);
		kfree(leaf);
	}
}

/**
 * glink_ssr_wait_cleanup_done() - Get the value of the
 *                                 notifications_successful flag.
 * @timeout_multiplier: timeout multiplier for waiting on all processors
 *
 * Return: True if cleanup_done received from all processors, false otherwise
 */
bool glink_ssr_wait_cleanup_done(unsigned int ssr_timeout_multiplier)
{
	int wait_ret =
		wait_for_completion_timeout(&notifications_successful_complete,
			ssr_timeout_multiplier * GLINK_SSR_REPLY_TIMEOUT);
	reinit_completion(&notifications_successful_complete);

	if (!notifications_successful || !wait_ret)
		return false;
	else
		return true;
}
EXPORT_SYMBOL(glink_ssr_wait_cleanup_done);

/**
 * glink_ssr_probe() - G-Link SSR platform device probe function
 * @pdev:	Pointer to the platform device structure
 *
 * This function parses DT for information on which subsystems should be
 * notified when each subsystem undergoes SSR. The global subsystem information
 * list is built from this information. In addition, SSR notifier callback
 * functions are registered here for the necessary subsystems.
 *
 * Return: 0 on success, standard error codes otherwise
 */
static int glink_ssr_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *phandle_node;
	struct restart_notifier_block *nb;
	struct subsys_info *ss_info;
	struct subsys_info_leaf *ss_info_leaf = NULL;
	struct glink_link_info *link_info;
	char *key;
	const char *edge;
	const char *subsys_name;
	const char *xprt;
	void *handle;
	void *link_state_handle;
	int phandle_index = 0;
	int ret = 0;

	if (!pdev) {
		GLINK_SSR_ERR("<SSR> %s: pdev is NULL\n", __func__);
		ret = -EINVAL;
		goto pdev_null_or_ss_info_alloc_failed;
	}

	node = pdev->dev.of_node;

	ss_info = kmalloc(sizeof(*ss_info), GFP_KERNEL);
	if (!ss_info) {
		GLINK_SSR_ERR("<SSR> %s: %s\n", __func__,
			"Could not allocate subsystem info structure\n");
		ret = -ENOMEM;
		goto pdev_null_or_ss_info_alloc_failed;
	}
	INIT_LIST_HEAD(&ss_info->notify_list);

	link_info = kmalloc(sizeof(struct glink_link_info),
			GFP_KERNEL);
	if (!link_info) {
		GLINK_SSR_ERR("<SSR> %s: %s\n", __func__,
			"Could not allocate link info structure\n");
		ret = -ENOMEM;
		goto link_info_alloc_failed;
	}
	ss_info->link_info = link_info;

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name) {
		GLINK_SSR_ERR("<SSR> %s: missing key %s\n", __func__, key);
		ret = -ENODEV;
		goto label_or_edge_missing;
	}

	key = "qcom,edge";
	edge = of_get_property(node, key, NULL);
	if (!edge) {
		GLINK_SSR_ERR("<SSR> %s: missing key %s\n", __func__, key);
		ret = -ENODEV;
		goto label_or_edge_missing;
	}

	key = "qcom,xprt";
	xprt = of_get_property(node, key, NULL);
	if (!xprt)
		GLINK_SSR_LOG(
			"%s %s: no transport present for subys/edge %s/%s\n",
			"<SSR>", __func__, subsys_name, edge);

	ss_info->ssr_name = subsys_name;
	ss_info->edge = edge;
	ss_info->xprt = xprt;
	ss_info->notify_list_len = 0;
	ss_info->link_info->transport = xprt;
	ss_info->link_info->edge = edge;
	ss_info->link_info->glink_link_state_notif_cb = glink_ssr_link_state_cb;
	ss_info->link_up = false;
	ss_info->handle = NULL;
	ss_info->link_state_handle = NULL;
	ss_info->cb_data = NULL;
	spin_lock_init(&ss_info->link_up_lock);
	spin_lock_init(&ss_info->cb_lock);
	init_waitqueue_head(&waitqueue);
	nb = kmalloc(sizeof(struct restart_notifier_block), GFP_KERNEL);
	if (!nb) {
		GLINK_SSR_ERR("<SSR> %s: Could not allocate notifier block\n",
				__func__);
		ret = -ENOMEM;
		goto label_or_edge_missing;
	}

	nb->subsystem = subsys_name;
	nb->nb.notifier_call = glink_ssr_restart_notifier_cb;
	nb->nb.priority = GLINK_SSR_PRIORITY;

	handle = subsys_notif_register_notifier(nb->subsystem, &nb->nb);
	if (IS_ERR_OR_NULL(handle)) {
		GLINK_SSR_ERR("<SSR> %s: Could not register SSR notifier cb\n",
				__func__);
		ret = -EINVAL;
		goto nb_registration_fail;
	}

	key = "qcom,notify-edges";
	while (true) {
		phandle_node = of_parse_phandle(node, key, phandle_index++);

		if (!phandle_node)
			break;

		ss_info_leaf = kmalloc(sizeof(struct subsys_info_leaf),
				GFP_KERNEL);
		if (!ss_info_leaf) {
			GLINK_SSR_ERR(
				"<SSR> %s: Could not allocate subsys_info_leaf\n",
				__func__);
			ret = -ENOMEM;
			goto notify_edges_no_memory;
		}

		subsys_name = of_get_property(phandle_node, "label", NULL);
		edge = of_get_property(phandle_node, "qcom,edge", NULL);
		xprt = of_get_property(phandle_node, "qcom,xprt", NULL);

		of_node_put(phandle_node);

		if (!subsys_name || !edge) {
			GLINK_SSR_ERR(
				"%s, %s: Found DT node with invalid data!\n",
				"<SSR>", __func__);
			ret = -EINVAL;
			goto invalid_dt_node;
		}

		ss_info_leaf->ssr_name = subsys_name;
		ss_info_leaf->edge = edge;
		ss_info_leaf->xprt = xprt;
		list_add_tail(&ss_info_leaf->notify_list_node,
				&ss_info->notify_list);
		ss_info->notify_list_len++;
	}

	list_add_tail(&ss_info->subsystem_list_node, &subsystem_list);

	link_state_handle = glink_register_link_state_cb(ss_info->link_info,
			NULL);
	if (IS_ERR_OR_NULL(link_state_handle)) {
		GLINK_SSR_ERR("<SSR> %s: Could not register link state cb\n",
				__func__);
		ret = PTR_ERR(link_state_handle);
		goto link_state_register_fail;
	}
	ss_info->link_state_handle = link_state_handle;

	return 0;

link_state_register_fail:
	list_del(&ss_info->subsystem_list_node);
invalid_dt_node:
	kfree(ss_info_leaf);
notify_edges_no_memory:
	subsys_notif_unregister_notifier(handle, &nb->nb);
	delete_ss_info_notify_list(ss_info);
nb_registration_fail:
	kfree(nb);
label_or_edge_missing:
	kfree(link_info);
link_info_alloc_failed:
	kfree(ss_info);
pdev_null_or_ss_info_alloc_failed:
	return ret;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,glink_ssr" },
	{},
};

static struct platform_driver glink_ssr_driver = {
	.probe = glink_ssr_probe,
	.driver = {
		.name = "msm_glink_ssr",
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

static int glink_ssr_init(void)
{
	int ret;

	glink_ssr_log_ctx =
		ipc_log_context_create(NUM_LOG_PAGES, "glink_ssr", 0);
	glink_ssr_wq = create_singlethread_workqueue("glink_ssr_wq");
	ret = platform_driver_register(&glink_ssr_driver);
	if (ret)
		GLINK_SSR_ERR("<SSR> %s: %s ret: %d\n", __func__,
				"glink_ssr driver registration failed", ret);

	notifications_successful = false;
	init_completion(&notifications_successful_complete);
	return 0;
}

module_init(glink_ssr_init);

MODULE_DESCRIPTION("MSM Generic Link (G-Link) SSR Module");
MODULE_LICENSE("GPL v2");

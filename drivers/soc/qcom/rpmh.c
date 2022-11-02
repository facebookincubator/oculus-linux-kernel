// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <soc/qcom/rpmh.h>

#include "rpmh-internal.h"

#define RPMH_TIMEOUT_MS			msecs_to_jiffies(10000)

#define DEFINE_RPMH_MSG_ONSTACK(dev, s, q, name)	\
	struct rpmh_request name = {			\
		.msg = {				\
			.state = s,			\
			.cmds = name.cmd,		\
			.num_cmds = 0,			\
			.wait_for_compl = true,		\
		},					\
		.cmd = { { 0 } },			\
		.completion = q,			\
		.dev = dev,				\
		.needs_free = false,				\
	}

#define ctrlr_to_drv(ctrlr) container_of(ctrlr, struct rsc_drv, client)

/**
 * struct cache_req: the request object for caching
 *
 * @addr: the address of the resource
 * @sleep_val: the sleep vote
 * @wake_val: the wake vote
 * @list: linked list obj
 */
struct cache_req {
	u32 addr;
	u32 sleep_val;
	u32 wake_val;
	struct list_head list;
};

/**
 * struct batch_cache_req - An entry in our batch catch
 *
 * @list: linked list obj
 * @count: number of messages
 * @rpm_msgs: the messages
 */

struct batch_cache_req {
	struct list_head list;
	int count;
	struct rpmh_request *rpm_msgs;
};

static struct rpmh_ctrlr *get_rpmh_ctrlr(const struct device *dev)
{
	struct rsc_drv *drv = dev_get_drvdata(dev->parent);

	return &drv->client;
}

static int check_ctrlr_state(struct rpmh_ctrlr *ctrlr, enum rpmh_state state)
{
	int ret = 0;

	/* Do not allow setting active votes when in solver mode */
	spin_lock(&ctrlr->cache_lock);
	if (ctrlr->in_solver_mode && state == RPMH_ACTIVE_ONLY_STATE)
		ret = -EBUSY;
	spin_unlock(&ctrlr->cache_lock);

	return ret;
}

/**
 * rpmh_mode_solver_set: Indicate that the RSC controller hardware has
 * been configured to be in solver mode
 *
 * @dev: the device making the request
 * @enable: Boolean value indicating if the controller is in solver mode.
 *
 * When solver mode is enabled, passthru API will not be able to send wake
 * votes, just awake and active votes.
 */
int rpmh_mode_solver_set(const struct device *dev, bool enable)
{
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);

	spin_lock(&ctrlr->cache_lock);
	rpmh_rsc_mode_solver_set(ctrlr_to_drv(ctrlr), enable);
	ctrlr->in_solver_mode = enable;
	spin_unlock(&ctrlr->cache_lock);

	return 0;
}
EXPORT_SYMBOL(rpmh_mode_solver_set);

void rpmh_tx_done(const struct tcs_request *msg, int r)
{
	struct rpmh_request *rpm_msg = container_of(msg, struct rpmh_request,
						    msg);
	struct completion *compl = rpm_msg->completion;
	bool free = rpm_msg->needs_free;

	rpm_msg->err = r;

	if (r)
		dev_err(rpm_msg->dev, "RPMH TX fail in msg addr=%#x, err=%d\n",
			rpm_msg->msg.cmds[0].addr, r);

	if (!compl)
		goto exit;

	/* Signal the blocking thread we are done */
	complete(compl);

exit:
	if (free)
		kfree(rpm_msg);
}

static struct cache_req *__find_req(struct rpmh_ctrlr *ctrlr, u32 addr)
{
	struct cache_req *p, *req = NULL;

	list_for_each_entry(p, &ctrlr->cache, list) {
		if (p->addr == addr) {
			req = p;
			break;
		}
	}

	return req;
}

static struct cache_req *cache_rpm_request(struct rpmh_ctrlr *ctrlr,
					   enum rpmh_state state,
					   struct tcs_cmd *cmd)
{
	struct cache_req *req;

	spin_lock(&ctrlr->cache_lock);
	req = __find_req(ctrlr, cmd->addr);
	if (req)
		goto existing;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		req = ERR_PTR(-ENOMEM);
		goto unlock;
	}

	req->addr = cmd->addr;
	req->sleep_val = req->wake_val = UINT_MAX;
	INIT_LIST_HEAD(&req->list);
	list_add_tail(&req->list, &ctrlr->cache);

existing:
	switch (state) {
	case RPMH_ACTIVE_ONLY_STATE:
		if (req->sleep_val != UINT_MAX) {
			req->wake_val = cmd->data;
			ctrlr->dirty = true;
		}
		break;
	case RPMH_WAKE_ONLY_STATE:
		if (req->wake_val != cmd->data) {
			req->wake_val = cmd->data;
			ctrlr->dirty = true;
		}
		break;
	case RPMH_SLEEP_STATE:
		if (req->sleep_val != cmd->data) {
			req->sleep_val = cmd->data;
			ctrlr->dirty = true;
		}
		break;
	default:
		break;
	}

unlock:
	spin_unlock(&ctrlr->cache_lock);

	return req;
}

/**
 * __rpmh_write: Cache and send the RPMH request
 *
 * @dev: The device making the request
 * @state: Active/Sleep request type
 * @rpm_msg: The data that needs to be sent (cmds).
 *
 * Cache the RPMH request and send if the state is ACTIVE_ONLY.
 * SLEEP/WAKE_ONLY requests are not sent to the controller at
 * this time. Use rpmh_flush() to send them to the controller.
 */
static int __rpmh_write(const struct device *dev, enum rpmh_state state,
			struct rpmh_request *rpm_msg)
{
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);
	int ret = -EINVAL;
	struct cache_req *req;
	int i;

	rpm_msg->msg.state = state;

	/* Cache the request in our store and link the payload */
	for (i = 0; i < rpm_msg->msg.num_cmds; i++) {
		req = cache_rpm_request(ctrlr, state, &rpm_msg->msg.cmds[i]);
		if (IS_ERR(req))
			return PTR_ERR(req);
	}

	rpm_msg->msg.state = state;

	if (state == RPMH_ACTIVE_ONLY_STATE) {
		WARN_ON(irqs_disabled());
		ret = rpmh_rsc_send_data(ctrlr_to_drv(ctrlr), &rpm_msg->msg);
	} else {
		/* Clean up our call by spoofing tx_done */
		ret = 0;
		rpmh_tx_done(&rpm_msg->msg, ret);
	}

	return ret;
}

static int __fill_rpmh_msg(struct rpmh_request *req, enum rpmh_state state,
		const struct tcs_cmd *cmd, u32 n)
{
	if (!cmd || !n || n > MAX_RPMH_PAYLOAD)
		return -EINVAL;

	memcpy(req->cmd, cmd, n * sizeof(*cmd));

	req->msg.state = state;
	req->msg.cmds = req->cmd;
	req->msg.num_cmds = n;

	return 0;
}

/**
 * rpmh_write_async: Write a set of RPMH commands
 *
 * @dev: The device making the request
 * @state: Active/sleep set
 * @cmd: The payload data
 * @n: The number of elements in payload
 *
 * Write a set of RPMH commands, the order of commands is maintained
 * and will be sent as a single shot.
 */
int rpmh_write_async(const struct device *dev, enum rpmh_state state,
		     const struct tcs_cmd *cmd, u32 n)
{
	struct rpmh_request *rpm_msg;
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);
	int ret;

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(ctrlr, state);
	if (ret)
		return ret;

	rpm_msg = kzalloc(sizeof(*rpm_msg), GFP_ATOMIC);
	if (!rpm_msg)
		return -ENOMEM;
	rpm_msg->needs_free = true;

	ret = __fill_rpmh_msg(rpm_msg, state, cmd, n);
	if (ret) {
		kfree(rpm_msg);
		return ret;
	}

	return __rpmh_write(dev, state, rpm_msg);
}
EXPORT_SYMBOL(rpmh_write_async);

/**
 * rpmh_write: Write a set of RPMH commands and block until response
 *
 * @rc: The RPMH handle got from rpmh_get_client
 * @state: Active/sleep set
 * @cmd: The payload data
 * @n: The number of elements in @cmd
 *
 * May sleep. Do not call from atomic contexts.
 */
int rpmh_write(const struct device *dev, enum rpmh_state state,
	       const struct tcs_cmd *cmd, u32 n)
{
	DECLARE_COMPLETION_ONSTACK(compl);
	DEFINE_RPMH_MSG_ONSTACK(dev, state, &compl, rpm_msg);
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);
	int ret;

	if (!cmd || !n || n > MAX_RPMH_PAYLOAD)
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(ctrlr, state);
	if (ret)
		return ret;

	memcpy(rpm_msg.cmd, cmd, n * sizeof(*cmd));
	rpm_msg.msg.num_cmds = n;

	ret = __rpmh_write(dev, state, &rpm_msg);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&compl, RPMH_TIMEOUT_MS);
	if (!ret) {
		rpmh_rsc_debug(ctrlr_to_drv(ctrlr), &compl);
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL(rpmh_write);

static void cache_batch(struct rpmh_ctrlr *ctrlr, struct batch_cache_req *req)
{

	spin_lock(&ctrlr->cache_lock);
	list_add_tail(&req->list, &ctrlr->batch_cache);
	spin_unlock(&ctrlr->cache_lock);
}

static int flush_batch(struct rpmh_ctrlr *ctrlr)
{
	struct batch_cache_req *req;
	const struct rpmh_request *rpm_msg;
	int ret = 0;
	int i;

	/* Send Sleep/Wake requests to the controller, expect no response */
	spin_lock(&ctrlr->cache_lock);
	list_for_each_entry(req, &ctrlr->batch_cache, list) {
		for (i = 0; i < req->count; i++) {
			rpm_msg = req->rpm_msgs + i;
			ret = rpmh_rsc_write_ctrl_data(ctrlr_to_drv(ctrlr),
						       &rpm_msg->msg);
			if (ret)
				break;
		}
	}
	spin_unlock(&ctrlr->cache_lock);

	return ret;
}

static void invalidate_batch(struct rpmh_ctrlr *ctrlr)
{
	struct batch_cache_req *req, *tmp;

	spin_lock(&ctrlr->cache_lock);
	list_for_each_entry_safe(req, tmp, &ctrlr->batch_cache, list) {
		list_del(&req->list);
		kfree(req);
	}
	INIT_LIST_HEAD(&ctrlr->batch_cache);
	spin_unlock(&ctrlr->cache_lock);
}

/**
 * rpmh_write_batch: Write multiple sets of RPMH commands and wait for the
 * batch to finish.
 *
 * @dev: the device making the request
 * @state: Active/sleep set
 * @cmd: The payload data
 * @n: The array of count of elements in each batch, 0 terminated.
 *
 * Write a request to the RSC controller without caching. If the request
 * state is ACTIVE, then the requests are treated as completion request
 * and sent to the controller immediately. The function waits until all the
 * commands are complete. If the request was to SLEEP or WAKE_ONLY, then the
 * request is sent as fire-n-forget and no ack is expected.
 *
 * May sleep. Do not call from atomic contexts for ACTIVE_ONLY requests.
 */
int rpmh_write_batch(const struct device *dev, enum rpmh_state state,
		     const struct tcs_cmd *cmd, u32 *n)
{
	struct batch_cache_req *req;
	struct rpmh_request *rpm_msgs;
	struct completion *compls;
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);
	unsigned long time_left;
	int count = 0;
	int ret, i;
	void *ptr;

	if (!cmd || !n)
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(ctrlr, state);
	if (ret)
		return ret;

	while (n[count] > 0)
		count++;
	if (!count)
		return -EINVAL;

	ptr = kzalloc(sizeof(*req) +
		      count * (sizeof(req->rpm_msgs[0]) + sizeof(*compls)),
		      GFP_ATOMIC);
	if (!ptr)
		return -ENOMEM;

	req = ptr;
	rpm_msgs = ptr + sizeof(*req);
	compls = ptr + sizeof(*req) + count * sizeof(*rpm_msgs);

	req->count = count;
	req->rpm_msgs = rpm_msgs;

	for (i = 0; i < count; i++) {
		__fill_rpmh_msg(rpm_msgs + i, state, cmd, n[i]);
		cmd += n[i];
	}

	if (state != RPMH_ACTIVE_ONLY_STATE) {
		cache_batch(ctrlr, req);
		return 0;
	}

	for (i = 0; i < count; i++) {
		struct completion *compl = &compls[i];

		init_completion(compl);
		rpm_msgs[i].completion = compl;
		ret = rpmh_rsc_send_data(ctrlr_to_drv(ctrlr), &rpm_msgs[i].msg);
		if (ret) {
			pr_err("Error(%d) sending RPMH message addr=%#x\n",
			       ret, rpm_msgs[i].msg.cmds[0].addr);
			break;
		}
	}

	time_left = RPMH_TIMEOUT_MS;
	while (i--) {
		time_left = wait_for_completion_timeout(&compls[i], time_left);
		if (!time_left) {
			/*
			 * Better hope they never finish because they'll signal
			 * the completion that we're going to free once
			 * we've returned from this function.
			 */
			rpmh_rsc_debug(ctrlr_to_drv(ctrlr), &compls[i]);
			ret = -ETIMEDOUT;
			goto exit;
		}
	}

exit:
	kfree(ptr);

	return ret;
}
EXPORT_SYMBOL(rpmh_write_batch);

/**
 * rpmh_write_pdc_data: Write PDC data to the controller
 *
 * @dev: the device making the request
 * @cmd: The payload data
 * @n: The number of elements in payload
 *
 * Write PDC data to the controller. The messages are always sent async.
 *
 * May be called from atomic contexts.
 */
int rpmh_write_pdc_data(const struct device *dev,
			const struct tcs_cmd *cmd, u32 n)
{
	DEFINE_RPMH_MSG_ONSTACK(dev, 0, NULL, rpm_msg);
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);

	if (!n || n > MAX_RPMH_PAYLOAD)
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	memcpy(rpm_msg.cmd, cmd, n * sizeof(*cmd));
	rpm_msg.msg.num_cmds = n;
	rpm_msg.msg.wait_for_compl = false;

	return rpmh_rsc_write_pdc_data(ctrlr_to_drv(ctrlr), &rpm_msg.msg);
}
EXPORT_SYMBOL(rpmh_write_pdc_data);

static int is_req_valid(struct cache_req *req)
{
	return (req->sleep_val != UINT_MAX &&
		req->wake_val != UINT_MAX &&
		req->sleep_val != req->wake_val);
}

static int send_single(const struct device *dev, enum rpmh_state state,
		       u32 addr, u32 data)
{
	DEFINE_RPMH_MSG_ONSTACK(dev, state, NULL, rpm_msg);
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);

	/* Wake sets are always complete and sleep sets are not */
	rpm_msg.msg.wait_for_compl = (state == RPMH_WAKE_ONLY_STATE);
	rpm_msg.cmd[0].addr = addr;
	rpm_msg.cmd[0].data = data;
	rpm_msg.msg.num_cmds = 1;

	return rpmh_rsc_write_ctrl_data(ctrlr_to_drv(ctrlr), &rpm_msg.msg);
}

/**
 * rpmh_flush: Flushes the buffered active and sleep sets to TCS
 *
 * @dev: The device making the request
 *
 * Return: -EBUSY if the controller is busy, probably waiting on a response
 * to a RPMH request sent earlier.
 *
 * This function is always called from the sleep code from the last CPU
 * that is powering down the entire system. Since no other RPMH API would be
 * executing at this time, it is safe to run lockless.
 */
int rpmh_flush(const struct device *dev)
{
	struct cache_req *p;
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);
	int ret;

	if (rpmh_standalone)
		return 0;

	if (!ctrlr->dirty) {
		pr_debug("Skipping flush, TCS has latest data.\n");
		return 0;
	}

	do {
		ret = rpmh_rsc_invalidate(ctrlr_to_drv(ctrlr));
	} while (ret == -EAGAIN);

	/* First flush the cached batch requests */
	ret = flush_batch(ctrlr);
	if (ret)
		return ret;

	/*
	 * Nobody else should be calling this function other than system PM,
	 * hence we can run without locks.
	 */
	list_for_each_entry(p, &ctrlr->cache, list) {
		if (!is_req_valid(p)) {
			pr_debug("%s: skipping RPMH req: a:%#x s:%#x w:%#x",
				 __func__, p->addr, p->sleep_val, p->wake_val);
			continue;
		}
		ret = send_single(dev, RPMH_SLEEP_STATE, p->addr, p->sleep_val);
		if (ret)
			return ret;
		ret = send_single(dev, RPMH_WAKE_ONLY_STATE,
				  p->addr, p->wake_val);
		if (ret)
			return ret;
	}

	ctrlr->dirty = false;

	return 0;
}
EXPORT_SYMBOL(rpmh_flush);

/**
 * rpmh_invalidate: Invalidate all sleep and active sets
 * sets.
 *
 * @dev: The device making the request
 *
 * Invalidate the sleep and active values in the TCS blocks.
 */
int rpmh_invalidate(const struct device *dev)
{
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);
	int ret;

	if (rpmh_standalone)
		return 0;

	invalidate_batch(ctrlr);
	ctrlr->dirty = true;

	do {
		ret = rpmh_rsc_invalidate(ctrlr_to_drv(ctrlr));
	} while (ret == -EAGAIN);

	return ret;
}
EXPORT_SYMBOL(rpmh_invalidate);

/**
 * rpmh_ctrlr_idle: Return the controller idle status
 *
 * @dev: the device making the request
 */
int rpmh_ctrlr_idle(const struct device *dev)
{
	struct rpmh_ctrlr *ctrlr = get_rpmh_ctrlr(dev);

	if (rpmh_standalone)
		return 0;

	return rpmh_rsc_ctrlr_is_idle(ctrlr_to_drv(ctrlr));
}
EXPORT_SYMBOL(rpmh_ctrlr_idle);

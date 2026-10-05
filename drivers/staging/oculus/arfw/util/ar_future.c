// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file ar_future.c
 *
 * @brief implementation of AR future objects
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "ar_future.h"

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <ar_common.h>

ar_future_t *ar_create_future(struct device *dev)
{
	ar_future_t *future =
		devm_kzalloc(dev, sizeof(ar_future_t), GFP_KERNEL);

	if (future) {
		init_completion(&future->complete);
		future->dev = dev;
		future->on_complete = NULL;
		future->on_complete_context = NULL;
		future->timeout_ms = 0;
		future->code = 0;
		future->data = NULL;
	}

	return future;
}
EXPORT_SYMBOL(ar_create_future);

void ar_future_set_promise(ar_future_t *future, ar_promise_t *promise)
{
	AR_ASSERT(future);
	AR_ASSERT(promise);
	promise->future = future;
	future->promise = promise;
}
EXPORT_SYMBOL(ar_future_set_promise);

void ar_future_set_on_complete(ar_future_t *future,
			       ar_future_on_complete_t on_complete,
			       void *on_complete_context)
{
	AR_ASSERT(future);
	future->on_complete = on_complete;
	future->on_complete_context = on_complete_context;
}
EXPORT_SYMBOL(ar_future_set_on_complete);

void ar_future_set_timeout(ar_future_t *future, uint32_t timeout_ms)
{
	AR_ASSERT(future);
	future->timeout_ms = timeout_ms;
}
EXPORT_SYMBOL(ar_future_set_timeout);

void ar_future_complete(ar_future_t *future, int code)
{
	AR_ASSERT(future);
	future->code = code;
	complete(&future->complete);
}
EXPORT_SYMBOL(ar_future_complete);

void ar_future_complete_data(ar_future_t *future, void *data)
{
	AR_ASSERT(future);
	future->data = data;
	ar_future_complete(future, 0);
}
EXPORT_SYMBOL(ar_future_complete_data);

static int ar_future_wait_complete(ar_future_t *future)
{
	int err;
	unsigned long flags;
	ar_promise_t *promise;
	struct device *dev;
	int promise_served;

	AR_ASSERT(future);
	dev = future->dev;
	AR_ASSERT(dev);
	promise = future->promise;
	AR_ASSERT(promise);
	AR_ASSERT(promise->lock);
	AR_ASSERT(promise->future == future);

	if (future->timeout_ms != 0) {
		unsigned long timeout_ms = wait_for_completion_timeout(
			&future->complete,
			msecs_to_jiffies(future->timeout_ms));
		err = timeout_ms == 0 ? -ETIMEDOUT : future->code;
		if (err == -ETIMEDOUT)
			pr_warn("[AR] future timed out after %u ms\n",
				future->timeout_ms);
	} else {
		wait_for_completion(&future->complete);
		err = future->code;
	}

	// If the future times out, the promise is freed here but NOT removed from the cmd_reply_list.
	// This causes a double-free when we tear down the driver.
	// Normal invocation paths should use list_del_init so we can differentiate between timeout (we
	// need to remove the promise from the list) and normal path (we don't need to remove the
	// promise from the list).
	// We can use list_del here because we are freeing the promise anyway.
	promise_served = 0;
	spin_lock_irqsave(promise->lock, flags);
	promise_served = list_empty(&promise->list);
	if (!promise_served)
		list_del(&promise->list);
	spin_unlock_irqrestore(promise->lock, flags);

	// If the list was empty and we hit timeout, then we failed to wait for completion.
	// In this case we are racing with ar_pci_bar_handle_reply_packet. This fuction gets
	// the promise and calling the complete callback. In this case we should wait for completion,
	// otherwise we could release promise and future memory while it is still used.
	if ((err == -ETIMEDOUT) && promise_served) {
		wait_for_completion(&future->complete);
		err = future->code;
	}
	if (future->on_complete)
		future->on_complete(err, future->data,
				    future->on_complete_context);

	devm_kfree(dev, promise);

	return err;
}

int ar_future_wait(ar_future_t *future)
{
	int err;

	AR_ASSERT(future);

	err = ar_future_wait_complete(future);
	devm_kfree(future->dev, future);

	return err;
}
EXPORT_SYMBOL(ar_future_wait);

int ar_future_wait_data(ar_future_t *future, void **data)
{
	int err;

	AR_ASSERT(future);
	AR_ASSERT(data);

	err = ar_future_wait_complete(future);
	if (!err)
		*data = future->data;
	devm_kfree(future->dev, future);

	return err;
}
EXPORT_SYMBOL(ar_future_wait_data);

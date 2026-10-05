/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file ar_future.h
 *
 * @brief header definiton for AR future objects
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_FUTURE_H
#define AR_FUTURE_H

#include <linux/completion.h>
#include <linux/list.h>
#include <linux/spinlock.h>

struct device;
typedef struct ar_future ar_future_t;
typedef struct ar_promise ar_promise_t;

typedef void (*ar_future_on_complete_t)(int code, void *data, void *context);

/**
 * A simple wrapper around kernel completion framework.
 * Each future carries a return error code and can have data passed via it.
 * Future can notify subscriber about it's own completion.
 *
 * IMPORTANT:
 * Futures cannot be retried by design, will be cleaned up on completion.
 */
struct ar_future {
	struct device *dev;
	ar_promise_t *promise;
	struct completion complete;
	ar_future_on_complete_t on_complete;
	void *on_complete_context;
	uint32_t timeout_ms;
	int code;
	void *data;
};

/**
 * All client implementations can extend this promise type by
 * embedding the base promise struct as the first member in their custom
 * promise type. This will allow the client to use the base promise to
 * clean things up at the end of the promise lifetime automatically.
 */
struct ar_promise {
	ar_future_t *future;
	struct list_head list;
	spinlock_t *lock;
};

/**
 * Create an empty future object.
 * The future will have no timeout set by default. Will block.
 *
 * @param[in] dev associated device for devres.
 * @retval NULL if allocation failed.
 */
ar_future_t *ar_create_future(struct device *dev);

/**
 * Bind this future to a promise object for cleanup.
 *
 * @param[in] future object to set promise on.
 * @param[in] promise corresponding promise object.
 */
void ar_future_set_promise(ar_future_t *future, ar_promise_t *promise);

/**
 * Set a callback that gets executed on future completion.
 * It gets executed no matter the future state and will get the code.
 *
 * @param[in] future object to set completion callback on.
 * @param[in] on_complete completion callback.
 * @param[in] on_complete_context additional context to pass to the callback.
 */
void ar_future_set_on_complete(ar_future_t *future,
			       ar_future_on_complete_t on_complete,
			       void *on_complete_context);

/**
 * Set a timeout for a future in milliseconds.
 *
 * @param[in] future object to set timeout on.
 * @param[in] timeout_ms timeout in milliseconds.
 */
void ar_future_set_timeout(ar_future_t *future, uint32_t timeout_ms);

/**
 * Complete a future with the given return code. Use 0 for success.
 * This will unblock calls to ar_future_wait/ar_future_wait_data.
 *
 * @param[in] future object to complete.
 * @param[in] code return error value.
 */
void ar_future_complete(ar_future_t *future, int code);

/**
 * Complete a future with data.
 * This completes the future with success error code and saves the data.
 *
 * @param[in] future object to complete.
 * @param[in] data object to save and pass to waiting call sites.
 */
void ar_future_complete_data(ar_future_t *future, void *data);

/**
 * The function will wait for future to complete or timeout if set.
 *
 * @param[in] future object to wait for completion on.
 *
 * @retval future return error code.
 */
int ar_future_wait(ar_future_t *future);

/**
 * The function will wait for future to complete or timeout if set.
 * Additionally will collect passed data.
 *
 * @param[in] future object to wait for completion on.
 * @param[out] data object to store the passed data into.
 *
 * @retval future return error code.
 */
int ar_future_wait_data(ar_future_t *future, void **data);

#endif // !AR_FUTURE_H

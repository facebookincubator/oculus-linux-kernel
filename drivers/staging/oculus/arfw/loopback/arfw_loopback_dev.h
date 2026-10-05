/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_loopback_device.h
 *
 * @brief Definitions for the arfw-loopback layer device
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFIRMWARE_LOOPBACK_DEV_H
#define ARFIRMWARE_LOOPBACK_DEV_H

#include <arfw_ops.h>

/**
 * Driver entry point to handle an arfw_io_request.
 *
 * Note: The driver may assume calls to this function are serialized within a
 * given queue but possibly concurrent across queues.
 *
 * @param[in] queue_context The hardware queue context.
 * @param[in] req Pointer to arfw_io_request.
 * @param[out] consumed Should the data be marked as consumed. If set to false,
 * the driver must later call, arfw_request_consumed.
 *
 * @retval TRUE Polling should continue
 */
bool arfw_loopback_dev_handle_send_queue_request(
	void *queue_context, const struct arfw_io_request *req, bool *consumed);

/**
 * Driver callback when the client has consumed a buffer from a receive queue.
 * This may be required if the driver is sharing the queue directly to hardware
 * as a DMA target and read indexes need to be updated.
 *
 * @param[in] queue handle for the current queue
 * @param[in] queue_context The hardware queue context.
 */
void arfw_loopback_dev_handle_rcv_queue_consume(arfw_client_queue_t queue,
						void *queue_context);

/**
 * Driver entry point when a client allocates memory for the queue data.
 * The queue data context is returned and could be used to get information about
 * the queue data.
 * This function should be protected by registered_devices.lock.
 *
 * @param[in] base_context The base hw dev context.
 * @param[in] queue_params Queue information.
 *
 * @retval Pointer to the queue data memory region structure in case of success,
 *         otherwise NULL.
 */
struct arfw_queue_mem_region *arfw_loopback_dev_handle_queue_data_alloc(
	void *base_context,
	const struct arfw_client_queue_create_params *queue_params);

/**
 * Driver entry point when a client frees memory for the queue data.
 * This function should be protected by registered_devices.lock.
 *
 * @param[in] base_context The base hw dev context.
 * @param[in] queue_data The queue data memory region structure to clean up.
 */
void arfw_loopback_dev_handle_queue_data_free(
	void *base_context, struct arfw_queue_mem_region *queue_data);

/**
 * Driver entry point when a client queue is created.
 * This serves as a point for the driver to allocate a context and set it for
 * the client queue.
 *
 * @param[in] queue client queue created for the request
 * @param[in] base_context The base hw dev context, passed into arfw_device_unregister
 * @param[in] queue_params Queue information
 * @param[in] queue_data Queue data memory region information
 * @param[out] queue_context The context associated with the queue
 *
 * @retval 0 No error.
 *
 * @retval -EEXIST The queue with requested credentials already exists.
 * @retval -ENOMEM Failed to allocated memory for the queue.
 * @retval -ENOSPC No free space to track the queue.
 * @retval -EINVAL Wrong arguments.
 * @retval -ENOENT There is no original queue for the mirror queue.
 */
int arfw_loopback_dev_handle_queue_create_request(
	arfw_client_queue_t queue, void *base_context,
	const struct arfw_client_queue_create_params *queue_params,
	struct arfw_queue_mem_region *queue_data, void **queue_context);

/**
 * Driver entry point when a queue is destroyed.
 * This allows the driver a chance to cancel any outstanding IO, and
 * release hardware resources.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[in] queue_context The context associated with this queue
 *
 * @retval 0 No error.
 *
 * @retval -EINVAL    Wrong arguments.
 * @retval -ESHUTDOWN The queue is already in the shutdown state, nothing to clean up.
 *
 */
int arfw_loopback_dev_handle_queue_destroy_request(void *base_context,
						   void *queue_context);
/**
 * Driver entry point the device information query.
 *
 * @param[in] base_context The base context from arfw_device_register
 * @param[out] device_information The device information structure to populate
 *
 * @retval 0 The structure was populated
 */
int arfw_loopback_dev_handle_get_device_information(
	void *base_context,
	struct arfw_device_information_req *device_information);

/**
 * Driver entry point to pend a receive external payload
 *
 * @param[in] queue handle for the current queue
 * @param[in] queue_context The hardware queue context.
 * @param[in] req external payload pend request.
 *
 * @retval 0 No error.
 */
int arfw_loopback_dev_handle_receive_payload_pend(
	void *queue_context, const struct arfw_payload_pend_req *req);

/**
 * Driver API to notify until the queue is constructed
 *
 * @param[in] queue_context The hardware queue context.
 */
void arfw_loopback_dev_handle_notify_queue_ready(void *queue_context);

/**
 * Mmap the queue data region to the user space.
 *
 * @param[in] base_context The device context which is used to mmap memory
 * @param[in] queue_data Pointer to the queue data memory region to mmap
 * @param[in] vma VMA to map in.
 *
 * @retval 0 in case of success, otherwise return error code.
 *
 * @retval -EINVAL VMA to mmap memory is wrong.
 */
int arfw_loopback_dev_handle_queue_data_mmap(
	void *base_context, struct arfw_queue_mem_region *queue_data,
	struct vm_area_struct *vma);

#endif // !ARFIRMWARE_LOOPBACK_DEV_H

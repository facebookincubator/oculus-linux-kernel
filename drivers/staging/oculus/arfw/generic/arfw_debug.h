/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_debug.h
 *
 * @brief Internal header for arfw debug functionality, mostly debugfs
 *
 * @details
 *
 *******************************************************************************/

#ifndef ARFW_DEBUG_H
#define ARFW_DEBUG_H

#include <linux/arfw_io_interface.h>

#include "arfw_int.h"

/**
 * Add queue directory to get internal information:
 *   queue/<id>/
 *
 * @param[in] client The client queue information to store in debugfs
 *
 * @retval None
 */
void arfw_debug_queue_add(struct arfw_client_queue *client);

/**
 * Remove the queue directory.
 *
 * @param[in] queue The client queue information to remove from debugfs
 *
 * @retval None
 */
void arfw_debug_queue_remove(struct arfw_client_queue *client);

/**
 * Add the device state tracking node.
 * Needs to be invoked after a new device registration.
 *
 * @param[in] device The added device id
 *
 * @retval None
 */
void arfw_debug_device_add(const char *device);

/**
 * Remove the device state tracking node.
 * Needs to be invoked after a device unregistration.
 *
 * @param[in] device The removed device id
 *
 * @retval None
 */
void arfw_debug_device_remove(const char *device);

/**
 * Initialize the debugfs to provide information about arfirmware IPC.
 * The following path will be created:
 *   arfw_ipc
 *
 * @retval 0 on success
 * @retval -err on failure
 */
int arfw_debug_init(void);

/**
 * Destroy the arfirmware IPC debugfs.
 *
 * @retval none
 */
void arfw_debug_destroy(void);

/**
 * Dump the queue state.
 *
 * @retval none
 */
void arfw_debug_dump_queue(struct arfw_client_queue *client);

/**
 * Initialize debug samplers for queue obj.
 */
int arfw_queue_debug_samplers_create(struct device *dev,
				     struct arfw_client_queue *client,
				     struct ar_queue_create_req *req);

#endif // !ARFW_DEBUG_H

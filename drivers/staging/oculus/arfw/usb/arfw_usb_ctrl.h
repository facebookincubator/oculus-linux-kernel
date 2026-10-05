/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_usb_ctrl.h
 *
 * @brief implementation of the arfw hw control packets utils for USB
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_USB_CTRL_H
#define ARFW_USB_CTRL_H

#include <linux/kernel.h>

#include <ar_future.h>

#include "arfw_usb_int.h"
#include "meta_usb_protocol_shared.h"

/**
 * Init control endpoints, allocate packet pools.
 * This also schedules IN packets for the configured pool size.
 * Control endpoints are assumed to be BULK endpoints matching fixed offset.
 * See: meta_usb_protocol_shared.h for more details.
 *
 * NOTE:
 * This is required to be able to initiate a handshake with firmware.
 * Also flow control is implemented as control packets.
 *
 * @param[in] driver Struct that holds subsystem context.
 *
 * @retval    0      No error.
 * @retval   -E...   Otherwise.
 */
int arfw_usb_ctrl_init(struct arfw_usb_driver *driver);

/**
 * De-init control endpoints, de-allocate packet pools.
 * Drain all the packet pools as well.
 * Takes care of flow control packets.
 *
 * @param[in] driver Struct that holds subsystem context.
 */
void arfw_usb_ctrl_exit(struct arfw_usb_driver *driver);

/**
 * Get a control endpoint packet to be used for a control message.
 * Things like create/destroy queue, handshake, flow control.
 *
 * NOTE:
 * This uses OUT endpoint specifically.
 * All IN packets are processed via promises.
 *
 * @param[in] driver  Struct that holds subsystem context.
 * @param[in] size    Buffer size for the packet.
 *
 * @retval    PTR     No error.
 * @retval    PTR_ERR Otherwise.
 */
struct arfw_usb_packet *arfw_usb_ctrl_packet_get(struct arfw_usb_driver *driver,
						 size_t size);

/**
 * Return the packet back to the pool.
 * Or de-allocates it if no pool is used.
 *
 * @param[in] packet Previously acquired packet.
 */
void arfw_usb_ctrl_packet_put(struct arfw_usb_packet *packet);

/**
 * Schedule a control endpoint packet to be used for a control message.
 * Things like create/destroy queue, handshake, flow control.
 *
 * NOTE:
 * This uses OUT endpoint specifically.
 * All IN packets are processed via promises.
 *
 * @param[in] packet Previously acquired packet.
 *
 * @retval    0      No error.
 * @retval   -E...   Otherwise.
 */
int arfw_usb_ctrl_packet_send(struct arfw_usb_packet *packet);

/**
 * Init flow control process using control endpoints.
 * This is needed because IN packets cannot request specific queue.
 *
 * NOTE:
 * It is specifically not implemented using indexes to not
 * force a particular implementation, instead we exchange room.
 *
 * @param[in] driver Struct that holds subsystem context.
 *
 * @retval    0      No error.
 * @retval   -E...   Otherwise.
 */
int arfw_usb_ctrl_sync_room_init(struct arfw_usb_driver *driver);

/**
 * Notify that the room values were updated.
 * If enough bumps happen within the window we flush updates before the
 * timer fires.
 *
 * NOTE:
 * This can be configured using kernel parameters.
 *
 * @param[in] driver Struct that holds subsystem context.
 */
void arfw_usb_ctrl_sync_room_bump(struct arfw_usb_driver *driver);

struct arfw_usb_ctrl_reply_promise;

typedef void(arfw_usb_ctrl_reply_complete_t)(
	struct arfw_usb_ctrl_reply_promise *, meta_usb_ctrl_msg_in_t *);

/**
 * Promise exposed to data path level.
 * This is used to hook completion callbacks to specific control packets.
 * Like create/destroy queue for instance.
 */
struct arfw_usb_ctrl_reply_promise {
	ar_promise_t base;
	uint16_t seq_num;
	void *context;
	arfw_usb_ctrl_reply_complete_t *complete;
};

/**
 * Init a control packet promise, fill out the fields.
 * This should always be called since it does seq_num calculations.
 *
 * @param[in] driver   Struct that holds subsystem context.
 * @param[in] promise  Callee side of the packet completion.
 * @param[in] future   Caller side of the packet completion.
 * @param[in] complete Packet completion callback function.
 */
void arfw_usb_ctrl_reply_promise_init(
	struct arfw_usb_driver *driver,
	struct arfw_usb_ctrl_reply_promise *promise, ar_future_t *future,
	arfw_usb_ctrl_reply_complete_t *complete);

/**
 * Perform control flow for info query.
 * This is effectively our handshake request. We exchange protocol
 * information with firmware (versions, sizes for things, etc).
 *
 * NOTE:
 * Result of the future will be the protocol packet struct.
 *
 * @param[in] driver     Struct that holds subsystem context.
 * @param[in] timeout_ms Timeout to set on the returned future.
 *
 * @retval    PTR        No error.
 * @retval    PTR_ERR    Otherwise.
 */
ar_future_t *arfw_usb_ctrl_info_query(struct arfw_usb_driver *driver,
				      unsigned long timeout_ms);

#endif // !ARFW_USB_CTRL_H

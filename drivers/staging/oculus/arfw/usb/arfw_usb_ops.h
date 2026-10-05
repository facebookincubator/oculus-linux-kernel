/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_usb_ops.h
 *
 * @brief implementation of the arfw hw device driver ops for USB
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_USB_OPS_H
#define ARFW_USB_OPS_H

#include <arfw_ops.h>

#include "arfw_usb_int.h"

/**
 * Accessor for hw device driver ops for USB.
 * See arfw_ops.h for details.
 *
 * @retval Pointer to the hw device driver ops.
 */
const struct arfw_driver_ops *arfw_usb_ops_get(void);

/**
 * Perform handshake with firmware for the given subsystem.
 * Exchange info messages, cache all the fields in the driver struct.
 *
 * NOTE:
 * This requires ctrl endpoints to be initialized and functional.
 * Does not need data endpoints.
 * Timeout is configured via info_timeout_ms kernel parameter.
 *
 * @param[in] driver Struct that holds subsystem context.
 *
 * @retval  0        No error.
 * @retval -E...     Otherwise.
 */
int arfw_usb_ops_fetch_info(struct arfw_usb_driver *driver);

#endif // !ARFW_USB_OPS_H

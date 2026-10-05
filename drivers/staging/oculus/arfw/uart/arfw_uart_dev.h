/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * Common (transport-agnostic) engine for the arfw UART transport: the
 * arfw_driver_ops implementation, COBS+CRC32 framing dispatch, babel_uart
 * wire-header translation, and the tty TX/RX plumbing. A binding layer (e.g.
 * the N_ARFW line discipline in arfw_acro_uart_main.c) owns the tty and drives
 * this engine through the small API below. This file carries no chip specifics.
 */

#ifndef ARFW_UART_DEV_H
#define ARFW_UART_DEV_H

#include <linux/types.h>

#include <arfw_log.h>

struct device;
struct tty_struct;

/* Opaque transport instance; defined in arfw_uart_dev.c. */
struct arfw_uart_driver;

/* Shared log helpers for the arfw UART transport (engine + bindings). */
#define AR_LOG_MODULE_UART "[ARFW-UART]"

#define AR_LOG_UART_ERR(action, str, ...) \
	AR_LOG_ERR(AR_LOG_MODULE_UART, action, str, ##__VA_ARGS__)
#define AR_LOG_UART_INFO(action, str, ...) \
	AR_LOG_INFO(AR_LOG_MODULE_UART, action, str, ##__VA_ARGS__)
#define AR_LOG_UART_DBG(action, str, ...) \
	AR_LOG_DBG(AR_LOG_MODULE_UART, action, str, ##__VA_ARGS__)
#define AR_LOG_UART_DEV_ERR(dev, action, str, ...) \
	AR_LOG_DEV_ERR(dev, AR_LOG_MODULE_UART, action, str, ##__VA_ARGS__)
#define AR_LOG_UART_DEV_INFO(dev, action, str, ...) \
	AR_LOG_DEV_INFO(dev, AR_LOG_MODULE_UART, action, str, ##__VA_ARGS__)

/**
 * arfw_uart_dev_register() - allocate the transport and register its cdev
 * @dev:    device used for log context during registration
 * @dev_id: character-device name to register with the arfw common layer
 *
 * Allocates the transport instance and registers it as an arfw character-device
 * backend named @dev_id. Returns the instance, or NULL on failure.
 *
 * The instance is plain kzalloc'd (not devm): it deliberately outlives any tty
 * later attached to it (an ldisc detach is a link-down, not a teardown) and
 * there is no bus device whose lifetime it should follow. The binding frees it
 * with arfw_uart_dev_unregister() at module exit. (Same model as arfw_loopback.)
 */
struct arfw_uart_driver *arfw_uart_dev_register(struct device *dev,
						const char *dev_id);

/**
 * arfw_uart_dev_unregister() - unregister the cdev, flush RX work, free instance
 * @drv: instance from arfw_uart_dev_register() (NULL tolerated)
 */
void arfw_uart_dev_unregister(struct arfw_uart_driver *drv);

/**
 * arfw_uart_dev_attach() - connect a tty to the transport
 * @drv:      transport instance
 * @tty:      tty carrying the data
 * @dev:      tty device, used for log context
 * @uart_dev: underlying serial controller for runtime-PM bracketing of TX, or
 *            NULL if it could not be resolved
 */
void arfw_uart_dev_attach(struct arfw_uart_driver *drv, struct tty_struct *tty,
			  struct device *dev, struct device *uart_dev);

/**
 * arfw_uart_dev_detach() - disconnect the tty (link-down semantics)
 * @drv: transport instance (NULL tolerated)
 *
 * Shuts down every active session so userspace reconnects when the port
 * returns, then stops using the tty. The instance and cdev persist.
 */
void arfw_uart_dev_detach(struct arfw_uart_driver *drv);

/**
 * arfw_uart_dev_receive() - feed received bytes into the transport
 * @drv:   transport instance
 * @cp:    received bytes
 * @count: number of bytes
 *
 * Returns the number of bytes consumed (may be less than @count if the RX
 * staging buffer is full).
 */
size_t arfw_uart_dev_receive(struct arfw_uart_driver *drv,
			     const unsigned char *cp, size_t count);

#endif /* ARFW_UART_DEV_H */

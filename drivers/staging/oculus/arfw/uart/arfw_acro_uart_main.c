// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * Acropolis binding for the common arfw UART transport engine (arfw_uart_dev.c).
 * Registers the N_ARFW tty line discipline for Janus IPC between the SoC and the
 * Acropolis IMCU: a userspace helper (arfwuartattach) opens /dev/ttyHS1, sets
 * the baud/termios, and attaches this ldisc, at which point the engine registers
 * with the arfw common framework as the AR-ACRO-UART character device.
 *
 * The port is left a plain tty when the ldisc is not attached, so AcroRecovery
 * (which drives /dev/ttyHS1 raw) can use the same UART; the two are arbitrated
 * in userspace (see arfwuartattach / the vendor.acro.uart.recovery property).
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_ldisc.h>

#include "arfw_uart_dev.h"

#define ARFW_UART_DEVICE_ID "AR-ACRO-UART"

/*
 * Single Acropolis-UART transport instance, created on the first ldisc attach
 * and kept for the module lifetime; an ldisc detach (e.g. AcroRecovery
 * preempting the port) is a transport *disconnect* -- like a PCIe link-down --
 * rather than a teardown, so the engine instance and its arfw cdev persist and
 * an in-flight arfw_write() cannot use-after-free. Freed only at module exit.
 * (Same singleton model as the bus-less arfw_loopback transport.)
 */
static struct arfw_uart_driver *g_arfw_uart;
static DEFINE_MUTEX(g_arfw_uart_lock);

/* ---- Line discipline ops ---- */

/*
 * tty is a fixed-ABI ldisc callback slot (tty_ldisc_ops.receive_buf2);
 * const-qualifying it would change the function-pointer type and break
 * assignment into arfw_uart_ldisc.
 */
// cppcheck-suppress constParameterCallback
static int arfw_uart_ldisc_receive_buf2(struct tty_struct *tty,
					const unsigned char *cp, char *fp,
					int count)
{
	struct arfw_uart_driver *drv = tty->disc_data;

	if (!drv || count <= 0)
		return count;

	return (int)arfw_uart_dev_receive(drv, cp, count);
}

static int arfw_uart_ldisc_open(struct tty_struct *tty)
{
	struct arfw_uart_driver *drv;
	struct device *dev = tty->dev;
	struct device *uart_dev = NULL;

	if (!tty->ops || !tty->ops->write)
		return -EINVAL;

	/*
	 * Resolve the serial controller device so the engine can bracket each TX with
	 * runtime-PM get/put (see arfw_uart_tty_write). For a serial_core-backed tty,
	 * tty->driver_data is the uart_state.
	 */
	{
		struct uart_state *uart_state = tty->driver_data;

		if (uart_state && uart_state->uart_port &&
		    uart_state->uart_port->dev)
			uart_dev = uart_state->uart_port->dev;
	}

	mutex_lock(&g_arfw_uart_lock);

	drv = g_arfw_uart;
	if (!drv) {
		/*
		 * First attach: create the persistent instance and register the arfw
		 * cdev once. Both live until module exit, so an ldisc detach is a
		 * disconnect rather than a teardown.
		 */
		drv = arfw_uart_dev_register(dev, ARFW_UART_DEVICE_ID);
		if (!drv) {
			mutex_unlock(&g_arfw_uart_lock);
			return -ENOMEM;
		}
		g_arfw_uart = drv;
	}

	arfw_uart_dev_attach(drv, tty, dev, uart_dev);
	tty->disc_data = drv;

	mutex_unlock(&g_arfw_uart_lock);

	if (!uart_dev)
		AR_LOG_UART_DEV_ERR(
			dev, AR_LOG_INIT,
			"no uart_port dev; TX may be dropped on autosuspend");
	AR_LOG_UART_DEV_INFO(dev, AR_LOG_INIT,
			     "UART transport connected: %s (N_ARFW ldisc)",
			     ARFW_UART_DEVICE_ID);
	return 0;
}

static void arfw_uart_ldisc_close(struct tty_struct *tty)
{
	struct arfw_uart_driver *drv = tty->disc_data;
	struct device *dev = tty->dev;

	if (!drv)
		return;

	arfw_uart_dev_detach(drv);
	tty->disc_data = NULL;

	AR_LOG_UART_DEV_INFO(dev, AR_LOG_SHUTDOWN,
			     "UART transport disconnected (port released)");
}

static struct tty_ldisc_ops arfw_uart_ldisc = {
	.owner = THIS_MODULE,
	.magic = TTY_LDISC_MAGIC,
	.name = "arfw_uart",
	.open = arfw_uart_ldisc_open,
	.close = arfw_uart_ldisc_close,
	.receive_buf2 = arfw_uart_ldisc_receive_buf2,
};

/* ---- Module boilerplate ---- */

static int __init arfw_uart_init(void)
{
	int ret = tty_register_ldisc(N_ARFW, &arfw_uart_ldisc);

	if (ret)
		AR_LOG_UART_ERR(AR_LOG_INIT,
				"failed to register N_ARFW ldisc: %d", ret);
	else
		AR_LOG_UART_INFO(AR_LOG_INIT,
				 "registered N_ARFW (%d) line discipline",
				 N_ARFW);
	return ret;
}
module_init(arfw_uart_init);

static void __exit arfw_uart_exit(void)
{
	tty_unregister_ldisc(N_ARFW);

	mutex_lock(&g_arfw_uart_lock);
	arfw_uart_dev_unregister(g_arfw_uart);
	g_arfw_uart = NULL;
	mutex_unlock(&g_arfw_uart_lock);
}
module_exit(arfw_uart_exit);

MODULE_DESCRIPTION(
	"AR firmware UART transport (N_ARFW line discipline) for Acropolis IPC");
MODULE_LICENSE("GPL v2");

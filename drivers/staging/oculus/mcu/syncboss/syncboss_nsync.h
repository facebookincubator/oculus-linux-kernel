/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_NSYNC_H
#define _SYNCBOSS_NSYNC_H

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/miscfifo.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/syncboss/consumer.h>

struct nsync_dev_data {
	/* Pointer to this device's on device struct, for convenience. */
	struct device *dev;

	/* Device to convey power nsync events to userspace */
	struct mutex miscdevice_mutex;
	struct miscdevice misc_nsync;
	struct miscfifo nsync_fifo;

	/* Nsync client reference count */
	int nsync_client_count;

	/* Nsync IRQ */
	int nsync_irq;

	/*
	 * Timestamp of the last NSYNC event
	 * This signal is driven by the NRF using an otherwise unused
	 * NRF->AP signal and used to synchronize time between the AP
	 * and NRF.
	 */
	uint64_t nsync_irq_timestamp;
	uint64_t nsync_irq_count;

	/* Spinlock used to protect access to the nsync timestamp and count */
	spinlock_t nsync_lock;
};

#endif /* _SYNCBOSS_NSYNC_H */

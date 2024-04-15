/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_TIMESYNC_H
#define _SYNCBOSS_TIMESYNC_H

#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/syncboss/consumer.h>

struct timesync_dev_data {
	/* Pointer to this device's on device struct, for convenience. */
	struct device *dev;

	/* Timesync IRQ */
	int timesync_irq;

	/*
	 * Timestamp of last TE event
	 * This signal is driven by the display hardware's TE line to
	 * the both NRF and AP. This is used to synchronize time between
	 * the AP and NRF.
	 */
	atomic64_t last_te_timestamp_ns;
};

#endif /* _SYNCBOSS_TIMESYNC_H */

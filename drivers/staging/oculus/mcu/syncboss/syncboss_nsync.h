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

	/* Syncboss SPI driver consumer APIs */
	struct syncboss_consumer_ops *syncboss_ops;

	/* Device to convey power nsync events to userspace */
	struct mutex miscdevice_mutex;
	struct miscdevice misc_nsync;
	struct miscfifo nsync_fifo;

	/* Nsync client reference count */
	int nsync_client_count;

	/* Nsync IRQ */
	int nsync_irq;

	/*
	 * Timestamp of the last nsync IRQ
	 * This signal is driven by the NRF using an otherwise unused
	 * NRF->AP signal and used to synchronize time between the AP
	 * and NRF.
	 */
	int64_t nsync_irq_timestamp_ns;
	int64_t nsync_irq_timestamp_us;
	uint64_t nsync_irq_count;

	/*
	 * Indicates whether the nsync IRQ has fired since the last nsync
	 * message from the MCU was consumed.
	 */
	bool nsync_irq_fired;

	/* Timestamp from the last nsync message */
	int64_t prev_mcu_timestamp_us;

	/*
	 * Calculated delta between MCU nsync timestamp and IRQ timestamp,
	 * and a flag indicating the value can be trusted.
	 */
	int64_t nsync_offset_us;
	enum syncboss_time_offset_status nsync_offset_status;

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	int64_t remote_offset_us;
	enum syncboss_time_offset_status remote_offset_status;
#endif

	/* Counters used for detecting warning or error conditions. */
	int consecutive_drift_limit_count;
	int consecutive_drift_limit_max;
	int drift_limit_count;
	int drift_sum_us;
	ktime_t stream_start_time;

	/* Spinlock used to protect access to the nsync timestamp and count */
	spinlock_t nsync_lock;

	/* Notifier blocks for syncboss state changes and received packets */
	struct notifier_block syncboss_state_nb;
	struct notifier_block rx_packet_nb;

	/*
	 * T182999318: temporary property to allow gradual deprecation of the
	 * legacy nsync userspace interface.
	 */
	bool disable_legacy_nsync;
};

/*
 * Limit positive increases in drift between nsync updates to this value.
 * This is chosen to account for NTP update to CLOCK_MONOTONIC, which
 * can contribute up to 500PPM, and other 100PPM to more than cover
 * worse case MCU vs SOC XO drift in opposite directions.
 *
 * If CLOCK_MONOTONIC usage is eventually replaced with
 * CLOCK_MONOTONIC_RAW, this value could be decreased.
 */
#define OFFSET_DRIFT_PPM_LIMIT 600

/*
 * Number of consecutive nsync packets that are allowed to exceed the
 * OFFSET_DRIFT_PPM_LIMIT before an error is logged and  communicated
 * to userspace.
 */
#define MAX_CONSECUTIVE_LIMITED_DRIFTS 25

#endif /* _SYNCBOSS_NSYNC_H */

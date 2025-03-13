/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_NSYNC_H
#define _SYNCBOSS_NSYNC_H

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/syncboss/consumer.h>

/*
 * Max allowed error between AP timestamp delta and MCU timestamp delta.
 * Experimentally derived by looking at the hollywood histogram.

 * nsync can drift because the AP and MCU timebases come from different XOs.
 * 100ppm should more than cover worst-case MCU vs AP XO drift in opposite
 * directions. If we assume a minimum nsync rate of 30 updates/s, each update
 * could be subject to 100ppm ~= 100us / 30 updates == 3.33us per update. Our
 * default max error parameter below is an order of magnitude higher than
 * expected clock drift, so normal drift won't prevent us from acquiring
 * clock sync.
 *
 * Additionally, NTP updates to CLOCK_MONOTONIC (the AP timebase we use) can
 * contribute up to 500ppm. We'll ignore this, since it's an exception case,
 * and we can wait until after the NTP updates settle before updating the
 * nsync offset. If CLOCK_MONOTONIC usage is eventually replaced with
 * CLOCK_MONOTONIC_RAW, this error component just wouldn't exist.
 */
#define VSYNC_MAX_DELTA_ERROR_US 30
#define NSYNC_MAX_DELTA_ERROR_US 30

/*
 * Number of consecutive nsync packets that are allowed to exceed the
 * NSYNC_MAX_DELTA_ERROR_US constraint before an error is logged and
 * communicated to userspace. This is approx. 1 second's worth.
 */
#define VSYNC_MAX_CONSECUTIVE_ERRORS 90
#define NSYNC_MAX_CONSECUTIVE_ERRORS 30

/* This needs to be odd for the way OFFSET is used to be valid */
#define NSYNC_HISTOGRAM_SIZE 255
#define NSYNC_HISTOGRAM_OFFSET (NSYNC_HISTOGRAM_SIZE / 2)

struct nsync_dev_data {
	/* Pointer to this device's on device struct, for convenience. */
	struct device *dev;

	/* Syncboss SPI driver consumer APIs */
	struct syncboss_consumer_ops *syncboss_ops;

	/* Nsync IRQ */
	int nsync_irq;

	/* Nsync IRQ CPU affinity */
	struct cpumask irq_affinity;

	/* See comments for V/NSYNC_MAX_DELTA_ERROR_US */
	int64_t max_delta_error_us;
	/* See comments for V/NSYNC_MAX_CONSECUTIVE_ERRORS */
	unsigned int max_consecutive_errors;

	/* Previous AP timestamp (us) */
	int64_t ap_ts_prev_us;
	/* Previous MCU timestamp (us) */
	int64_t mcu_ts_prev_us;
	/* Current AP timestamp (us) from most recent nsync IRQ */
	int64_t ap_ts_now_us;
	/* Current MCU timestamp is local-only; cached to prev after use */

	/*
	 * Consecutive timestamp pairs that do not seem to strongly correlate or
	 * exhibit unacceptable jitter.
	 */
	unsigned int errors;

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

	/* Spinlock used to protect access to the nsync timestamp and count */
	spinlock_t nsync_lock;

	/* Notifier blocks for syncboss state changes and received packets */
	struct notifier_block syncboss_state_nb;
	struct notifier_block rx_packet_nb;

	/* Debug and statistics; zeroed on alloc but never after */
	struct {
		/* Max errors in a row */
		unsigned int errors_max;
		/*
		 * Max errors in a row before we got time sync, i.e., latency to getting
		 * a time sync.
		 */
		unsigned int sync_max;
		/*
		 * Histogram is in units of (delta / 4) us, so [-3, 3]us == 0,
		 * [4, 7]us == 1, etc.
		 * Max delta is ~= +-508us. Since the delta can be negative, the indexes
		 * into the histogram array are offset:
		 * index 0 -> <= -508us
		 * index 128 -> [-3, 3]us
		 * index 254 -> >= 508us
		 */
		uint32_t histogram[NSYNC_HISTOGRAM_SIZE];
	} debug;
};

#endif /* _SYNCBOSS_NSYNC_H */

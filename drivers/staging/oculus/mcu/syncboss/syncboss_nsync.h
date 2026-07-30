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
 * communicated to userspace. This is approx SYNC_ERROR_WINDOW seconds worth.
 *
 * The longer this error window, the more clock drift we might accumulate, but
 * we've seen system load impact sync jitter for a period of at least 1 second.
 * Instead of always allowing more sync jitter (opening up MAX_DELTA_ERROR_US),
 * we're going to allow more time for sync to occur at the potential cost of
 * drift. Requiring the whole window to sync should be rare.
 */
#define SYNC_ERROR_WINDOW 3
#define VSYNC_NOMINAL_RATE 90
#define VSYNC_MAX_CONSECUTIVE_ERRORS (SYNC_ERROR_WINDOW * VSYNC_NOMINAL_RATE)
#define NSYNC_NOMINAL_RATE 30
#define NSYNC_MAX_CONSECUTIVE_ERRORS (SYNC_ERROR_WINDOW * NSYNC_NOMINAL_RATE)
#define SYNC_MAX_NOMINAL_RATE \
	((VSYNC_NOMINAL_RATE > NSYNC_NOMINAL_RATE) ? VSYNC_NOMINAL_RATE : NSYNC_NOMINAL_RATE)

/* This needs to be odd for the way OFFSET is used to be valid */
#define NSYNC_HISTOGRAM_SIZE 255
#define NSYNC_HISTOGRAM_OFFSET (NSYNC_HISTOGRAM_SIZE / 2)

/* Number of elements to store of sync history. Must be >= 3. */
#define SYNC_HIST_LEN 3
/* index's range is [0, SYNC_HIST_LEN) */
#define SYNC_HERE(dev) ((dev)->index)
#define SYNC_PREV(dev) (((dev)->index + (SYNC_HIST_LEN - 1)) % SYNC_HIST_LEN)
#define SYNC_PPREV(dev) (((dev)->index + (SYNC_HIST_LEN - 2)) % SYNC_HIST_LEN)

struct nsync_debug_state {
	int64_t ap_ts_prev_us;
	int64_t ap_ts_now_us;
	int64_t mcu_ts_prev_us;
	int64_t mcu_ts_now_us;
	unsigned int errors;
	unsigned int long_syncs;
	enum syncboss_time_offset_status status;
	uint32_t seq;
};

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

	/*
	 * Timestamp histories are >= 3 elements deep so we can account for AP-side
	 * jitter in specific cases by examining 2 deltas worth of timestamps.
	 */
	unsigned int index;
	/* AP timestamp history (us); index is newest */
	int64_t ap_ts_us[SYNC_HIST_LEN];
	/* MCU timestamp history (us); index is newest */
	int64_t mcu_ts_us[SYNC_HIST_LEN];

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
		/* Number of times we got sync over a 2-delta window instead of 1 */
		unsigned int long_syncs;
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
		/*
		 * Rotating buffer of algorithm input/state data
		 * Use *_NOMINAL_RATE as the max index
		 */
		struct nsync_debug_state states[SYNC_MAX_NOMINAL_RATE];
		unsigned int states_index;
		unsigned int states_max;
	} debug;
};

#endif /* _SYNCBOSS_NSYNC_H */

/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_TIMESYNC_H
#define _SYNCBOSS_TIMESYNC_H

#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/syncboss/consumer.h>

/* This needs to be odd for the way OFFSET is used to be valid */
#define DRIFT_HISTOGRAM_SIZE 41 /* (-20 to +20 us) */
#define DRIFT_HISTOGRAM_OFFSET (DRIFT_HISTOGRAM_SIZE / 2)

struct timesync_dev_data {
	/* Pointer to this device's on device struct, for convenience. */
	struct device *dev;

	/* Syncboss SPI driver consumer APIs */
	struct syncboss_consumer_ops *syncboss_ops;

	/* Pin control for GPIO toggle (which MCU will observe and timestamp). */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default_state;
	struct pinctrl_state *pinctrl_active_state;
	int gpio;

	/* Timer used for scheduling timesync_gpio toggles */
	struct hrtimer timer;

	/* Period of synchronization (and IRQ toggles) */
	uint32_t period_ms;
	uint32_t period_ktime;

	/* Timestamp of most recent GPIO toggle */
	int64_t ap_ts_us;

	/* True if IRQ was triggered but we're still waiting for a MCU response. */
	bool waiting_for_msg;

	/*
	 * Calculated delta between MCU timesync timestamp and GPIO timestamp,
	 * and a flag indicating the value can be trusted.
	 */
	int64_t timesync_offset_us;
	enum syncboss_time_offset_status timesync_offset_status;

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	int64_t remote_offset_us;
	enum syncboss_time_offset_status remote_offset_status;
#endif

	/*
	 * Lock used to protect access to the timestamps, offsets and
	 * status in this struct.
	 */
	spinlock_t lock;

	struct {
		/*
		 * Histogram is in units of microseconds (us)
		 * With DRIFT_HISTOGRAM_SIZE=41, the max delta is +-20us.
		 * index  0 -> <= -20us
		 * index 20 ->  =   0us
		 * index 40 -> >= +20us
		 */
		uint32_t histogram[DRIFT_HISTOGRAM_SIZE];
		uint64_t prev_ap_ts_us;
		uint64_t prev_mcu_ts_us;
		int64_t max_drift_us;
		int64_t min_drift_us;
	} stats;

	/* Notifier blocks for syncboss state changes and received packets */
	struct notifier_block syncboss_state_nb;
	struct notifier_block rx_packet_nb;
};

#endif /* _SYNCBOSS_TIMESYNC_H */

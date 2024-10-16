/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_POWERSTATE_H
#define _SYNCBOSS_POWERSTATE_H

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/miscfifo.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/syncboss/consumer.h>

struct powerstate_dev_data {
	/* Pointer to this device's on device struct, for convenience */
	struct device *dev;

	/* Syncboss SPI driver consumer APIs */
	struct syncboss_consumer_ops *syncboss_ops;

	/* Device to convey power state events and reasons */
	struct mutex miscdevice_mutex;
	struct miscdevice misc_powerstate;

	/*
	 * FIFO to help push state event data from the SyncBoss to the
	 * misc_powerstate device.
	 */
	struct miscfifo powerstate_fifo;

	/* Powertate event client reference count */
	int powerstate_client_count;

	/* True if syncboss controls a prox sensor */
	bool has_prox;

	/* True if prox calibration data is not required for prox to work */
	bool has_no_prox_cal;

	/* prox calibration values */
	int prox_canc;
	int prox_thdl;
	int prox_thdh;

	/* prox config version */
	u8 prox_config_version;

	/* The most recent power state event */
	int powerstate_last_evt;

	/* True if we should refrain from sending the next system_up event */
	bool eat_next_system_up_event;

	/* True if we should refrain from sending the prox_on events */
	bool eat_prox_on_events;

	/*
	 * True if the MCU state change handling should be supressed
	 * since the events being triggered are due to waking the MCU
	 * for the purpose of configuring prox.
	 */
	bool prox_config_in_progress;

	/* True if power state events are enabled */
	bool powerstate_events_enabled;

	/* Notifier blocks for syncboss state changes and received packets */
	struct notifier_block syncboss_state_nb;
	struct notifier_block rx_packet_nb;
};

/* The message we send to SyncBoss to set the prox calibration */
struct prox_config_data {
	u8 type;
	u16 prox_thdh;
	u16 prox_thdl;
	u16 prox_canc;
} __packed;

/* The message we send to SyncBoss to set the prox calibration */
struct prox_config_version {
	u8 type;
	u8 config_version;
} __packed;

#endif /* _SYNCBOSS_POWERSTATE_H */

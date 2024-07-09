/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_MISCFIFO_H
#define _SYNCBOSS_MISCFIFO_H

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/miscfifo.h>
#include <linux/notifier.h>
#include <linux/syncboss/consumer.h>

struct miscfifo_dev_data {
	/* Pointer to this device's on device struct, for convenience */
	struct device *dev;

	/* Syncboss SPI driver consumer APIs */
	struct syncboss_consumer_ops *syncboss_ops;

	/* Stream miscfifo and accociated state */
	struct mutex stream_mutex;
	struct miscdevice misc_stream;
	struct miscfifo stream_fifo;
	bool stream_fifo_wake_needed;

	/* Control miscfifo and accociated state */
	struct miscdevice misc_control;
	struct miscfifo control_fifo;
	bool control_fifo_wake_needed;

	/* Notifier blocks for syncboss state changes and received packets */
	struct notifier_block syncboss_state_nb;
	struct notifier_block rx_packet_nb;
};

#endif /* _SYNCBOSS_MISCFIFO_H */

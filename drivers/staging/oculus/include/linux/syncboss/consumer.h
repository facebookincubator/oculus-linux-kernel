/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_CONSUMER_H
#define _SYNCBOSS_CONSUMER_H

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <uapi/linux/syncboss.h>

enum syncboss_state_event_types {
	SYNCBOSS_EVENT_MCU_UP,
	SYNCBOSS_EVENT_MCU_DOWN,
	SYNCBOSS_EVENT_MCU_PIN_RESET,
	SYNCBOSS_EVENT_STREAMING_STARTING,
	SYNCBOSS_EVENT_STREAMING_STARTED,
	SYNCBOSS_EVENT_STREAMING_STOPPING,
	SYNCBOSS_EVENT_STREAMING_STOPPED,
	SYNCBOSS_EVENT_STREAMING_SUSPENDING,
	SYNCBOSS_EVENT_STREAMING_SUSPENDED,
	SYNCBOSS_EVENT_STREAMING_RESUMING,
	SYNCBOSS_EVENT_STREAMING_RESUMED,
	SYNCBOSS_EVENT_WAKE_READERS,
};

struct syncboss_consumer_ops {
	/*
	 * Register or unregister for syncboss driver state change events or packets from
	 * the MCU. Notifier callback implementations are allowed to block, but should be
	 * mindful of the performance implications of doing so.
	 */
	int (*state_event_notifier_register)(struct device *dev, struct notifier_block *nb);
	int (*state_event_notifier_unregister)(struct device *dev, struct notifier_block *nb);
	int (*rx_packet_notifier_register)(struct device *dev, struct notifier_block *nb);
	int (*rx_packet_notifier_unregister)(struct device *dev, struct notifier_block *nb);

	/*
	 * Acquire/release a mutex that blocks all state changes by the syncboss_spi driver.
	 * This should only be used at times when SPI communication or power/reset changes
	 * would be unsafe, such as while a SWD update of MCU FW is being performed.
	 */
	void (*syncboss_state_lock)(struct device *dev);
	void (*syncboss_state_unlock)(struct device *dev);

	/* Check if syncboss stream is currently open */
	bool (*get_is_streaming)(struct device *dev);

	/*
	 * Vote for MCU to be awake (enable_mcu()) or remove that vote (disable_mcu()).
	 * Calls are refcounted, so enables and disables should be balanced.
	 * Enabling the MCU does not start the SPI data stream.
	 */
	int (*enable_mcu)(struct device *dev);
	int (*disable_mcu)(struct device *dev);

	/*
	 * Vote for MCU to start the SPI data stream, which also implicitly votes for
	 * the MCU to be kept awake.
	 * Calls are refcounted, so enables and disables should be balanced.
	 */
	int (*enable_stream)(struct device *dev);
	int (*disable_stream)(struct device *dev);

	/*
	 * Queue a packet to be sent to the MCU. If streaming has not been started,
	 * the packet will be held until it has been.
	 *
	 * Pass true for from_user if the buffer is a __user memory pointer.
	 */
	ssize_t (*queue_tx_packet)(struct device *dev, const void *buf, size_t count, bool from_user);
};

struct rx_packet_info {
	struct syncboss_driver_data_header_t header;
	const struct syncboss_data *data;
};

#endif

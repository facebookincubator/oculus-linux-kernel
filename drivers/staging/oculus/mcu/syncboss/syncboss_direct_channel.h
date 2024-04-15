/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _SYNCBOSS_DIRECT_CHANNEL_H
#define _SYNCBOSS_DIRECT_CHANNEL_H

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/syncboss/messages.h>

/* This is the max size of data payload for sensor direct channel. */
#define SYNCBOSS_DIRECT_CHANNEL_MAX_SENSOR_DATA_SIZE (16 * sizeof(float))

struct direct_channel_dev_data {
	/* Pointer to this device's on device struct, for convenience */
	struct device *dev;

	/* Syncboss SPI driver consumer APIs */
	struct syncboss_consumer_ops *syncboss_ops;

	/*
	 * Settings for direct channel mappings per packet type
	 */
	dma_addr_t dma_mask;

	/*
	 * DMA Bufs are created and mapped per sample type and not
	 * multiplexed as is the case for miscfifos. Each client can register
	 * a DMABUF per sample type.
	 */
	struct list_head *direct_channel_data[NUM_PACKET_TYPES];
	struct rw_semaphore direct_channel_rw_lock; /* single semaphore protecting for now, as write is very rare. */
	struct miscdevice misc_directchannel;

	/* Notifier blocks for syncboss state changes and received packets */
	struct notifier_block syncboss_state_nb;
	struct notifier_block rx_packet_nb;
};

struct channel_dma_buf_info {
	void *k_virtptr;
	size_t buffer_size;
	struct dma_buf *dma_buf;
};

struct direct_channel_data {
	struct syncboss_sensor_direct_channel_data *channel_start_ptr;
	struct syncboss_sensor_direct_channel_data *channel_end_ptr;
	struct syncboss_sensor_direct_channel_data *channel_current_ptr;
	u32 counter;
};

struct channel_client_entry {
	struct list_head list_entry;
	struct channel_dma_buf_info dma_buf_info;
	struct direct_channel_data channel_data;
	struct file *file;
	u8 wake_epoll;
	int (*direct_channel_distribute)(struct direct_channel_dev_data *devdata,
					  struct channel_client_entry *client_data,
					  const struct syncboss_data *packet);
};

#endif /* _SYNCBOSS_DIRECT_CHANNEL_H */

/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _SYNCBOSS_DIRECT_CHANNEL_H
#define _SYNCBOSS_DIRECT_CHANNEL_H

#include <linux/list.h>

/* This is the max size of data payload for sensor direct channel. */
#define SYNCBOSS_DIRECT_CHANNEL_MAX_SENSOR_DATA_SIZE (16 * sizeof(float))

struct syncboss_channel_dma_buf_info {
	void *k_virtptr;
	size_t buffer_size;
	struct dma_buf *dma_buf;
};

struct syncboss_direct_channel_data {
	struct syncboss_sensor_direct_channel_data *channel_start_ptr;
	struct syncboss_sensor_direct_channel_data *channel_end_ptr;
	struct syncboss_sensor_direct_channel_data *channel_current_ptr;
	u32 counter;
};

struct syncboss_channel_client_entry {
	struct list_head list_entry;
	struct syncboss_channel_dma_buf_info dma_buf_info;
	struct syncboss_direct_channel_data channel_data;
	struct syncboss_dev_data *devdata;
	struct file *file;
	u8 wake_epoll;
	int (*direct_channel_distribute)(struct syncboss_dev_data *devdata,
					  struct syncboss_channel_client_entry *client_data,
					  const struct syncboss_data *packet);
};

int syncboss_register_direct_channel_interface(
	struct syncboss_dev_data *devdata, struct device *dev);
int syncboss_deregister_direct_channel_interface(
	struct syncboss_dev_data *devdata);

#if defined(CONFIG_SYNCBOSS_DIRECTCHANNEL)
inline int syncboss_direct_channel_send_buf(struct syncboss_dev_data *devdata,
				     const struct syncboss_data *packet);
#else
static inline int syncboss_direct_channel_send_buf(struct syncboss_dev_data *devdata,
				     const struct syncboss_data *packet)
					 { return -ENOTSUPP;}
#endif /* CONFIG_SYNCBOSS_DIRECTCHANNEL */
#endif /* _SYNCBOSS_DIRECT_CHANNEL_H */

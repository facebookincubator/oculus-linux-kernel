// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-direction.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/of_platform.h>
#include <linux/syncboss/consumer.h>
#include <linux/time64.h>

#include <uapi/linux/syncboss.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_direct_channel.h"
#include "syncboss_direct_channel_mcu_defs.h"

#define SYNCBOSS_DIRECTCHANNEL_DEVICE_NAME "syncboss_directchannel0"

static int syncboss_directchannel_open(struct inode *inode, struct file *f)
{
	struct direct_channel_dev_data *devdata = container_of(
		f->private_data, struct direct_channel_dev_data, misc_directchannel);

	dev_dbg(devdata->dev, "direct channel opened by %s (%d)",
		 current->comm, current->pid);

	return 0;
}

static int syncboss_directchannel_release(struct inode *inode, struct file *f)
{
	int status = 0;
	struct direct_channel_dev_data *devdata = container_of(
		f->private_data, struct direct_channel_dev_data, misc_directchannel);
	struct device *dev = devdata->dev;
	struct channel_client_entry *client_data, *tmp_list_node;
	struct channel_dma_buf_info *c_dma_buf_info;
	struct dma_buf *c_dma_buf;
	int i;

	dev_dbg(dev, "releasing direct channel file : %p)", f);

	down_write(
		&devdata->direct_channel_rw_lock); /* rare event, could add jitter to interrupt thread; if issue add locks per sensor type. */
	for (i = SPI_PACKET_MIN_PACKET_TYPE_SUPPORTED; i <= SPI_PACKET_MAX_PACKET_TYPE_SUPPORTED; i++) {
		struct list_head *client_list = devdata->direct_channel_data[i];

		if (!client_list || list_empty(client_list))
			continue;

		dev_dbg(
			dev,
			"releasing direct channel for sensor type Client - %d called by %s (%d)",
			i, current->comm, current->pid);
		list_for_each_entry_safe(client_data, tmp_list_node, client_list, list_entry) {
			if (client_data->file != f)
				continue;

			client_data->channel_data.channel_current_ptr =  NULL;
			c_dma_buf_info = &client_data->dma_buf_info;
			c_dma_buf = c_dma_buf_info->dma_buf;
			dma_resv_lock(c_dma_buf->resv, NULL);
			dma_buf_vunmap(c_dma_buf, c_dma_buf_info->k_virtptr);
			dma_resv_unlock(c_dma_buf->resv);
			dma_buf_put(c_dma_buf); /* decrement ref count */
			list_del(&client_data->list_entry);
			devm_kfree(dev, client_data);

			if (list_empty(client_list)) {
				devdata->direct_channel_data[i] = NULL;
				devm_kfree(dev, client_list);
			}
			break;
		}
	}
	up_write(&devdata->direct_channel_rw_lock);

	return status;
}

/* Update the direct channel buffer with IMU data. */
static int direct_channel_distribute_spi_payload(struct direct_channel_dev_data *devdata,
				   struct channel_client_entry *client_data,
				   const struct rx_packet_info *packet_info)
{
	int status = 0;
	struct device *dev = devdata->dev;
	const struct syncboss_data *packet = packet_info->data;
	struct syncboss_sensor_direct_channel_data *current_ptr;
	struct direct_channel_data *channel_data;
	struct dma_buf *dma_buffer;

	channel_data = &client_data->channel_data;
	current_ptr = (struct syncboss_sensor_direct_channel_data *)
			      channel_data->channel_current_ptr;
	if (current_ptr == NULL) {
		dev_err(dev,
			"%s syncboss dropping data due to null current pointer %d",
			__func__, packet->data_len);
		return -EIO;
	}

	dma_buffer = client_data->dma_buf_info.dma_buf;

	if (packet->data_len > SYNCBOSS_DIRECT_CHANNEL_MAX_SENSOR_DATA_SIZE) {
		dev_err_ratelimited(dev, "%s syncboss data size to large %d",
				    __func__, packet->data_len);
		return -EIO;
	}

	status = dma_buf_begin_cpu_access(dma_buffer, DMA_BIDIRECTIONAL);
	if (status != 0) {
		dev_err_ratelimited(dev, "dma_buf_begin_cpu_access failed %d", status);
		return status;
	}

	/* size/report_token and sensor_type are fixed values and added at init time */
	current_ptr->timestamp = ktime_get_ns();
	memcpy(current_ptr->payload.idata, packet->data, packet->data_len);

	channel_data->counter++;
	if (channel_data->counter == 0)
		channel_data->counter = 1;

	/*
	 *  wmb here to ensure prior writes are complete before we update the counter.
	 *  counter is used by the client code to determine when to read new data.
	 */
	wmb();
	current_ptr->counter = channel_data->counter;
	status = dma_buf_end_cpu_access(
		dma_buffer,
		DMA_BIDIRECTIONAL);
	if (status != 0) {
		dev_err_ratelimited(dev,
			"%s dma_buf_end_cpu_access failed %d",
			__func__, status);
		channel_data->counter--; // undo increment, as this write could have failed.
		return status;
	}

	channel_data->channel_current_ptr++;
	if (channel_data->channel_current_ptr >= channel_data->channel_end_ptr)
		channel_data->channel_current_ptr = channel_data->channel_start_ptr;

	return status;
}

static int init_client_entry_instance(struct channel_client_entry *channel_client_entry_list,
			struct dma_buf *dma_buffer,
			int num_channel_entries,
			struct syncboss_driver_directchannel_shared_memory_config *new_config,
			struct file *file,
			struct direct_channel_dev_data *devdata)
{
	int status = 0;
	void *k_virt_addr;

	dma_resv_lock(dma_buffer->resv, NULL);
	k_virt_addr = dma_buf_vmap(dma_buffer);
	if (IS_ERR_OR_NULL(k_virt_addr)) {
		dev_err(
			devdata->dev,
			"%s :dma_buf_vmap failed %p",
			__func__,
			k_virt_addr);
		dma_resv_unlock(dma_buffer->resv);
		status = -EFAULT;
		goto err;
	}
	dma_resv_unlock(dma_buffer->resv);
	/*
	 * Save a copy of the mapped virtual address to avoid mapping/remapping
	 */
	channel_client_entry_list->dma_buf_info.k_virtptr = k_virt_addr;
	channel_client_entry_list->dma_buf_info.buffer_size = dma_buffer->size;
	channel_client_entry_list->dma_buf_info.dma_buf = dma_buffer;
	channel_client_entry_list->channel_data.channel_start_ptr = k_virt_addr;
	channel_client_entry_list->channel_data.channel_current_ptr = k_virt_addr;
	channel_client_entry_list->channel_data.channel_end_ptr =
		k_virt_addr + (num_channel_entries * sizeof(struct syncboss_sensor_direct_channel_data));
	channel_client_entry_list->channel_data.counter = 0;
	channel_client_entry_list->file = file;
	channel_client_entry_list->wake_epoll = new_config->wake_epoll;

	if (new_config->uapi_pkt_type == SPI_PACKET_TYPE_IMU_DATA) {
		channel_client_entry_list->direct_channel_distribute =
			direct_channel_distribute_spi_payload;
	}

err:
	return status;
}

/*
 * Setup from IOCTL call.
 */
static int syncboss_set_directchannel_sm(
	struct direct_channel_dev_data *devdata, struct file *file,
	const struct syncboss_driver_directchannel_shared_memory_config __user
		*config)
{
	int status = 0;
	struct device *dev = devdata->dev;
	struct syncboss_driver_directchannel_shared_memory_config *new_config = NULL;
	struct dma_buf *dma_buffer;
	uint32_t num_channel_entries;
	struct channel_client_entry *channel_client_entry_list = NULL;
	struct list_head *channel_list = NULL;
	struct syncboss_sensor_direct_channel_data *dc_ptr;

	new_config = kzalloc(sizeof(*new_config), GFP_KERNEL);
	if (!new_config)
		return -ENOMEM;

	status = copy_from_user(new_config, config, sizeof(*new_config));
	if (status != 0) {
		dev_err(dev,
			"Syncboss direct channel ioctl:failed to copy %d bytes from user stream shared memory config",
			status);
		status = -EFAULT;
		goto err;
	}

	dev_dbg(
		dev,
		"Syncboss direct channel ioctl:FileDesc: %d direct channel buffer size:%lu Packet Type:%d Wake epoll:%d",
		new_config->dmabuf_fd,
		new_config->direct_channel_buffer_size,
		new_config->uapi_pkt_type,
		new_config->wake_epoll);

	if (new_config->wake_epoll) {
		dev_err(
			dev,
			"Syncboss direct channel ioctl: poll not supported");
		status = -EINVAL;
		goto err;
	}

	dma_buffer = dma_buf_get(new_config->dmabuf_fd);
	if (PTR_ERR_OR_ZERO(dma_buffer)) {
		status = PTR_ERR(dma_buffer);
		dev_err(
			dev,
			"Syncboss direct channel ioctl: FD is not a dmabuf error:%d",
			status);
		goto err;
	}

	num_channel_entries =
			new_config->direct_channel_buffer_size /
			sizeof(struct syncboss_sensor_direct_channel_data);

	dev_dbg(
			dev,
			"Syncboss direct channel ioctl:dma_buffer size:%lu direct channel buffer size:%lu num_channel_entries:%d entry size:%lu",
			dma_buffer->size,
			new_config->direct_channel_buffer_size,
			num_channel_entries,
			sizeof(struct syncboss_sensor_direct_channel_data));

	if (new_config->direct_channel_buffer_size > dma_buffer->size)	{
		dev_err(
			dev,
			"Syncboss direct channel ioctl: DMA buffer smaller than direct channel buffer size");
		status = -EINVAL;
		goto err;
	}

	if (num_channel_entries == 0) {
		dev_err(
			dev,
			"Syncboss direct channel ioctl: channel entries = 0");

		status = -EINVAL;
		goto err;
	}

	if (new_config->uapi_pkt_type < SPI_PACKET_MIN_PACKET_TYPE_SUPPORTED ||
		new_config->uapi_pkt_type > SPI_PACKET_MAX_PACKET_TYPE_SUPPORTED) {
		dev_err(dev,
			"Syncboss direct channel ioctl unsupported packet type %d",
			new_config->uapi_pkt_type);
		status = -EINVAL;
		goto err;
	}

	/*
	 * Set up memory for list of clients, if not already allocated.
	 */
	if (!devdata->direct_channel_data[new_config->uapi_pkt_type]) {
		channel_list =
			devm_kzalloc(dev, sizeof(*channel_list), GFP_KERNEL);
		if (!channel_list) {
			status = -ENOMEM;
			goto err;
		}
		devdata->direct_channel_data[new_config->uapi_pkt_type] =
			channel_list;
		INIT_LIST_HEAD(
			devdata->direct_channel_data[new_config->uapi_pkt_type]);
	}

	channel_client_entry_list =
		devm_kzalloc(dev, sizeof(*channel_client_entry_list), GFP_KERNEL);
	if (!channel_client_entry_list) {
		status = -ENOMEM;
		goto err;
	}
	INIT_LIST_HEAD(&(channel_client_entry_list->list_entry));

	dev->dma_mask = &devdata->dma_mask;
	status = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (status != 0) {
		dev_err(dev, "dma_set_mask_and_coherent failed rc=%d",
			status);
		status = -EFAULT;
		goto err;
	}

	if (init_client_entry_instance(channel_client_entry_list,
								dma_buffer,
								num_channel_entries,
								new_config,
								file,
								devdata) != 0)
		goto err;

	status = dma_buf_begin_cpu_access(dma_buffer, DMA_BIDIRECTIONAL);
	if (status != 0) {
		dev_err_ratelimited(dev, "dma_buf_begin_cpu_access failed %d", status);
		goto err;
	}

	/*
	 * Initialize the dma buf channel data structure for elements that dont change per update.
	 */
	memset(channel_client_entry_list->dma_buf_info.k_virtptr, 0, num_channel_entries * sizeof(struct syncboss_sensor_direct_channel_data));
	dc_ptr = (struct syncboss_sensor_direct_channel_data *)channel_client_entry_list->dma_buf_info.k_virtptr;

	do {
		dc_ptr->size = sizeof(struct syncboss_sensor_direct_channel_data);
		dc_ptr->report_token = 0;
		dc_ptr->sensor_type = new_config->uapi_pkt_type;
		dc_ptr += 1;
	} while (dc_ptr < (struct syncboss_sensor_direct_channel_data *)channel_client_entry_list->channel_data.channel_end_ptr);

	status = dma_buf_end_cpu_access(dma_buffer, DMA_BIDIRECTIONAL);

	down_write(&devdata->direct_channel_rw_lock);
	list_add(&channel_client_entry_list->list_entry,
		 devdata->direct_channel_data[new_config->uapi_pkt_type]);
	up_write(&devdata->direct_channel_rw_lock);

	kfree(new_config);
	return 0;

err:
	kfree(new_config);
	devm_kfree(dev, channel_client_entry_list);
	devm_kfree(dev, channel_list);
	if (devdata->direct_channel_data[new_config->uapi_pkt_type] == channel_list) /* first time through, remove the empty list pointer.*/
		devdata->direct_channel_data[new_config->uapi_pkt_type] = NULL;
	return status;
}

static int syncboss_clear_directchannel_sm(
	struct direct_channel_dev_data *devdata, struct file *file,
	const struct syncboss_driver_directchannel_shared_memory_clear_config __user
		*config)
{
	int status = -EINVAL;
	struct device *dev = devdata->dev;
	struct syncboss_driver_directchannel_shared_memory_clear_config *new_config = NULL;
	struct channel_client_entry *client_data, *tmp_list_node;
	struct list_head *client_list;
	struct channel_dma_buf_info *c_dma_buf_info;
	struct dma_buf *c_dma_buf;
	struct dma_buf *dma_buffer;

	new_config = kzalloc(sizeof(*new_config), GFP_KERNEL);
	if (!new_config)
		return -ENOMEM;

	status = copy_from_user(new_config, config, sizeof(*new_config));
	if (status != 0) {
		dev_err(dev,
			"Syncboss direct channel ioctl:failed to copy %d bytes from user stream shared memory config",
			status);
		status = -EFAULT;
		goto err;
	}

	dev_dbg(
		dev,
		"Syncboss direct channel ioctl:FileDesc: %d Packet Type:%d",
		new_config->dmabuf_fd,
		new_config->uapi_pkt_type);

	if (new_config->uapi_pkt_type < SPI_PACKET_MIN_PACKET_TYPE_SUPPORTED ||
		new_config->uapi_pkt_type > SPI_PACKET_MAX_PACKET_TYPE_SUPPORTED) {
		dev_err(dev,
			"Syncboss direct channel ioctl unsupported packet type %d",
			new_config->uapi_pkt_type);
		status = -EINVAL;
		goto err;
	}

	dma_buffer = dma_buf_get(new_config->dmabuf_fd);
	if (PTR_ERR_OR_ZERO(dma_buffer)) {
		dev_err(
			dev,
			"Syncboss direct channel ioctl: FD is not a dmabuf error:%d",
			status);
		status = PTR_ERR(dma_buffer);
		goto err;
	}

	down_write(
		&devdata->direct_channel_rw_lock); /* rare event, could add jitter to interrupt thread; if issue add locks per sensor type. */

	client_list = devdata->direct_channel_data[new_config->uapi_pkt_type];

	if (!client_list || list_empty(client_list)) {
		dev_err(dev,
			"Syncboss direct channel ioctl no dmabuf entries for packet type %d",
			new_config->uapi_pkt_type);
		status = -EINVAL;
		goto err_up;
	}

	dev_dbg(
		dev,
		"releasing direct channel dmabuf fd %d for sensor type Client - %d called by %s (%d)", new_config->dmabuf_fd,
		new_config->uapi_pkt_type, current->comm, current->pid);

	list_for_each_entry_safe(client_data, tmp_list_node, client_list, list_entry) {
		if (client_data->file != file || client_data->dma_buf_info.dma_buf != dma_buffer)
			continue;

		status = 0;
		client_data->channel_data.channel_current_ptr =  NULL;
		c_dma_buf_info = &client_data->dma_buf_info;
		c_dma_buf = c_dma_buf_info->dma_buf;
		dma_resv_lock(c_dma_buf->resv, NULL);
		dma_buf_vunmap(c_dma_buf, c_dma_buf_info->k_virtptr);
		dma_resv_unlock(c_dma_buf->resv);
		dma_buf_put(c_dma_buf); /* decrement ref count */
		list_del(&client_data->list_entry);
		devm_kfree(dev, client_data);

		if (list_empty(client_list)) {
			devdata->direct_channel_data[new_config->uapi_pkt_type] = NULL;
			devm_kfree(dev, client_list);
		}
		break;
	}

	up_write(&devdata->direct_channel_rw_lock);
	dma_buf_put(dma_buffer); /* decrement ref count */
	kfree(new_config);
	return status;

err_up:
	up_write(&devdata->direct_channel_rw_lock);
err:
	kfree(new_config);
	return status;
}

static long syncboss_directchannel_ioctl(struct file *f, unsigned int cmd,
					 unsigned long arg)
{
	struct direct_channel_dev_data *devdata = container_of(
		f->private_data, struct direct_channel_dev_data, misc_directchannel);

	switch (cmd) {
	case SYNCBOSS_SET_DIRECTCHANNEL_SHARED_MEMORY_IOCTL:
		dev_dbg(devdata->dev,
			 "direct channel ioctl %d from %s (%d)", cmd,
			 current->comm, current->pid);
		return syncboss_set_directchannel_sm(
			devdata, f,
			(struct syncboss_driver_directchannel_shared_memory_config *)arg);
	case SYNCBOSS_CLEAR_DIRECTCHANNEL_SHARED_MEMORY_IOCTL:
		dev_dbg(devdata->dev,
			"direct channel ioctl %d from %s (%d)", cmd,
			current->comm, current->pid);
		return syncboss_clear_directchannel_sm(
			devdata, f,
			(struct syncboss_driver_directchannel_shared_memory_clear_config *)arg);
	default:
		dev_err(devdata->dev,
			"unrecognized direct channel ioctl %d from %s (%d)",
			cmd, current->comm, current->pid);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations directchannel_fops = {
	.open = syncboss_directchannel_open,
	.release = syncboss_directchannel_release,
	.read = NULL,
	.poll = NULL,
	.unlocked_ioctl = syncboss_directchannel_ioctl
};

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct direct_channel_dev_data *devdata = container_of(nb, struct direct_channel_dev_data, rx_packet_nb);
	const struct rx_packet_info *packet_info = pi;
	int ret = NOTIFY_DONE;
	struct channel_client_entry *client_data;

	/* Acquire Read Lock for Direct Channel Clients - writer is rare. */
	down_read(&devdata->direct_channel_rw_lock);

	if (devdata->direct_channel_data[type]) {
		list_for_each_entry(client_data, devdata->direct_channel_data[type], list_entry) {
			int status = (*client_data->direct_channel_distribute) (devdata, client_data, packet_info);

			if (status >= 0) {
				/* We have delivered to at least one client. */
				ret = NOTIFY_STOP;
			} else {
				dev_dbg(devdata->dev, "direct channel distibute failed error %d", status);
				break;
			}
		}
	}
	up_read(&devdata->direct_channel_rw_lock);

	return ret;
}

static int syncboss_direct_channel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct direct_channel_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct direct_channel_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	devdata->misc_directchannel.name = SYNCBOSS_DIRECTCHANNEL_DEVICE_NAME;
	devdata->misc_directchannel.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_directchannel.fops = &directchannel_fops;

	ret = misc_register(&devdata->misc_directchannel);
	if (ret < 0) {
		dev_err(dev, "failed to register direct channel device, error %d", ret);
		goto out;
	}

	/* Gloal read/write lock to update direct channel clients. */
	init_rwsem(&devdata->direct_channel_rw_lock);

	devdata->rx_packet_nb.notifier_call = rx_packet_handler;
	devdata->rx_packet_nb.priority = SYNCBOSS_PACKET_CONSUMER_PRIORITY_DIRECTCHANNEL;
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register rx packet notifier, error %d", ret);
		goto err_after_misc_reg;
	}

	dev_dbg(dev, "%s register direct channel misc device", __func__);

	return ret;

err_after_misc_reg:
	misc_deregister(&devdata->misc_directchannel);
out:
	return ret;
}

static int syncboss_direct_channel_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct direct_channel_dev_data *devdata = dev_get_drvdata(dev);

	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);

	misc_deregister(&devdata->misc_directchannel);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_direct_channel_match_table[] = {
	{ .compatible = "meta,syncboss-direct-channel", },
	{ },
};
#else
#define syncboss_direct_channel_match_table NULL
#endif

struct platform_driver syncboss_direct_channel_driver = {
	.driver = {
		.name = "syncboss_direct_channel",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_direct_channel_match_table
	},
	.probe = syncboss_direct_channel_probe,
	.remove = syncboss_direct_channel_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_direct_channel_driver,
};

static int __init syncboss_direct_channel_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_direct_channel_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_direct_channel_init);
module_exit(syncboss_direct_channel_exit);
MODULE_DESCRIPTION("Syncboss Direct Channel Interface Driver");
MODULE_LICENSE("GPL v2");

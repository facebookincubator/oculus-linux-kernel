// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_miscfifo.h"

#define MISCFIFO_SIZE 1024
#define STREAM_DEVICE_NAME "syncboss_stream0"
#define CONTROL_DEVICE_NAME "syncboss_control0"

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct miscfifo_dev_data *devdata = container_of(nb, struct miscfifo_dev_data, rx_packet_nb);
	const struct rx_packet_info *packet_info = pi;
	const struct syncboss_driver_data_header_t *header = &packet_info->header;
	const struct syncboss_data *packet = packet_info->data;
	struct miscfifo *fifo_to_use;
	struct uapi_pkt_t uapi_pkt;
	const char *fifo_name;
	bool should_wake;
	int status;
	size_t payload_size = sizeof(*packet) + packet->data_len;

	if (packet->sequence_id == 0) {
		fifo_to_use = &devdata->stream_fifo;
		fifo_name = "stream";
	} else {
		fifo_to_use = &devdata->control_fifo;
		fifo_name = "control";
	}

	if (payload_size > ARRAY_SIZE(uapi_pkt.payload)) {
		dev_warn_ratelimited(devdata->dev,
				     "%s rx packet is too big [id=%d seq=%d sz=%d]",
				     fifo_name,
				     (int) packet->type,
				     (int) packet->sequence_id,
				     (int) packet->data_len);
		return NOTIFY_BAD;
	}

	/* arrange |header|payload| in a single buffer */
	memcpy(&uapi_pkt.header, header, sizeof(uapi_pkt.header));
	memcpy(uapi_pkt.payload, packet, payload_size);

	status = miscfifo_write_buf(fifo_to_use, (u8 *)&uapi_pkt,
			sizeof(uapi_pkt.header) + payload_size, &should_wake);

	if (should_wake) {
		if (fifo_to_use == &devdata->stream_fifo)
			devdata->stream_fifo_wake_needed = true;
		else
			devdata->control_fifo_wake_needed = true;
	}

	if (status < 0) {
		dev_warn_ratelimited(devdata->dev, "uapi %s fifo error (%d)",
				     fifo_name, status);
		return NOTIFY_BAD;
	}

	return NOTIFY_STOP;
}

/*
 * Mark FIFO as runnable if new data has been written.
 * They will wake on the next reschedule.
 */
static int syncboss_state_handler(struct notifier_block *nb, unsigned long type, void *p)
{
	struct miscfifo_dev_data *devdata = container_of(nb, struct miscfifo_dev_data, syncboss_state_nb);

	switch (type) {
	case SYNCBOSS_EVENT_WAKE_READERS:
		if (!devdata->stream_fifo_wake_needed && !devdata->control_fifo_wake_needed)
			return NOTIFY_STOP;

		preempt_disable();
		if (devdata->stream_fifo_wake_needed) {
			miscfifo_wake_waiters_sync(&devdata->stream_fifo);
			devdata->stream_fifo_wake_needed = false;
		}
		if (devdata->control_fifo_wake_needed) {
			miscfifo_wake_waiters_sync(&devdata->control_fifo);
			devdata->control_fifo_wake_needed = false;
		}
		preempt_enable_no_resched();

		return NOTIFY_STOP;

	case SYNCBOSS_EVENT_MCU_DOWN:
		/*
		 * Clear any unread fifo events that userspace has not yet read, so these
		 * messages from the previous boot of the MCU don't get confused as messages
		 * sent after the next boot of the MCU.
		 */
		miscfifo_clear(&devdata->stream_fifo);
		miscfifo_clear(&devdata->control_fifo);

		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static int syncboss_set_stream_type_filter(
	struct miscfifo_dev_data *devdata,
	struct file *file,
	const struct syncboss_driver_stream_type_filter __user *filter)
{
	struct device *dev = devdata->dev;
	struct syncboss_driver_stream_type_filter *new_filter = NULL;
	struct syncboss_driver_stream_type_filter *existing_filter;
	int ret;

	new_filter = devm_kzalloc(dev, sizeof(*new_filter), GFP_KERNEL);
	if (!new_filter)
		return -ENOMEM;

	ret = copy_from_user(new_filter, filter, sizeof(*new_filter));
	if (ret != 0) {
		dev_err(dev, "failed to copy %d bytes from user stream filter\n", ret);
		return -EFAULT;
	}

	/* Sanity check new_filter */
	if (new_filter->num_selected > SYNCBOSS_MAX_FILTERED_TYPES) {
		dev_err(dev, "sanity check of user stream filter failed (num_selected = %d)\n",
			new_filter->num_selected);
		return -EINVAL;
	}

	existing_filter = miscfifo_fop_xchg_context(file, new_filter);
	if (existing_filter)
		devm_kfree(dev, existing_filter);
	return 0;
}

/*
 * This is a function that miscfifo calls to determine if it should
 * send a given packet to a given client.
 */
static bool should_send_stream_packet(const void *context,
				      const u8 *header, size_t header_len,
				      const u8 *payload, size_t payload_len)
{
	int x = 0;
	const struct syncboss_driver_stream_type_filter *stream_type_filter =
		context;
	const struct uapi_pkt_t *uapi_pkt = (struct uapi_pkt_t *) payload;
	const struct syncboss_data *packet;

	/* Special case for when no filter is set or there's no payload */
	if (!stream_type_filter || (stream_type_filter->num_selected == 0) ||
	    !payload)
		return true;

	packet = (struct syncboss_data *) uapi_pkt->payload;
	for (x = 0; x < stream_type_filter->num_selected; ++x) {
		if (packet->type == stream_type_filter->selected_types[x])
			return true;
	}
	return false;
}

static int syncboss_stream_open(struct inode *inode, struct file *f)
{
	struct miscfifo_dev_data *devdata =
		container_of(f->private_data, struct miscfifo_dev_data,
			     misc_stream);
	int ret;

	mutex_lock(&devdata->stream_mutex);
	ret = miscfifo_fop_open(f, &devdata->stream_fifo);
	mutex_unlock(&devdata->stream_mutex);

	dev_info(devdata->dev, "stream fifo opened by %s (%d)",
		 current->comm, current->pid);

	return ret;
}

static long syncboss_stream_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	struct miscfifo_client *client = file->private_data;
	struct miscfifo_dev_data *devdata =
		container_of(client->mf, struct miscfifo_dev_data, stream_fifo);
	int ret;

	mutex_lock(&devdata->stream_mutex);

	switch (cmd) {
	case SYNCBOSS_SET_STREAMFILTER_IOCTL:
		ret = syncboss_set_stream_type_filter(devdata, file,
			(struct syncboss_driver_stream_type_filter *)arg);
		break;
	default:
		dev_err(devdata->dev, "unrecognized stream ioctl %d from %s (%d)",
				cmd, current->comm, current->pid);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&devdata->stream_mutex);

	return ret;
}

static int syncboss_stream_release(struct inode *inode, struct file *file)
{
	struct miscfifo_client *client = file->private_data;
	struct miscfifo_dev_data *devdata =
		container_of(client->mf, struct miscfifo_dev_data, stream_fifo);
	struct syncboss_driver_stream_type_filter *stream_type_filter = NULL;
	int ret;

	mutex_lock(&devdata->stream_mutex);
	stream_type_filter = miscfifo_fop_xchg_context(file, NULL);
	if (stream_type_filter)
		devm_kfree(devdata->dev, stream_type_filter);

	ret = miscfifo_fop_release(inode, file);

	mutex_unlock(&devdata->stream_mutex);

	dev_info(devdata->dev, "stream fifo closed by %s (%d)",
		 current->comm, current->pid);

	return ret;
}

static const struct file_operations stream_fops = {
	.open = syncboss_stream_open,
	.release = syncboss_stream_release,
	.read = miscfifo_fop_read_many,
	.poll = miscfifo_fop_poll,
	.unlocked_ioctl = syncboss_stream_ioctl
};

static int syncboss_control_open(struct inode *inode, struct file *f)
{
	struct miscfifo_dev_data *devdata =
		container_of(f->private_data, struct miscfifo_dev_data,
			     misc_control);

	return miscfifo_fop_open(f, &devdata->control_fifo);
}

static const struct file_operations control_fops = {
	.open = syncboss_control_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll
};

static int syncboss_miscfifo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct miscfifo_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct miscfifo_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	mutex_init(&devdata->stream_mutex);

	devdata->stream_fifo.config.kfifo_size = MISCFIFO_SIZE;
	devdata->stream_fifo.config.filter_fn = should_send_stream_packet;
	ret = devm_miscfifo_register(dev, &devdata->stream_fifo);
	if (ret < 0) {
		dev_err(dev, "failed to register stream miscfifo %d", ret);
		goto out;
	}

	devdata->misc_stream.name = STREAM_DEVICE_NAME;
	devdata->misc_stream.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_stream.fops = &stream_fops;

	ret = misc_register(&devdata->misc_stream);
	if (ret < 0) {
		dev_err(dev, "failed to register stream device, error %d", ret);
		goto out;
	}

	devdata->control_fifo.config.kfifo_size = MISCFIFO_SIZE;
	ret = devm_miscfifo_register(dev, &devdata->control_fifo);
	if (ret < 0) {
		dev_err(dev, "failed to register control miscfifo, error %d", ret);
		goto err_after_stream_reg;
	}

	devdata->misc_control.name = CONTROL_DEVICE_NAME;
	devdata->misc_control.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_control.fops = &control_fops;

	ret = misc_register(&devdata->misc_control);
	if (ret < 0) {
		dev_err(dev, "failed to register control device, error %d", ret);
		goto err_after_stream_reg;
	}

	devdata->syncboss_state_nb.notifier_call = syncboss_state_handler;
	devdata->syncboss_state_nb.priority = SYNCBOSS_STATE_CONSUMER_PRIORITY_MISCFIFO;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->syncboss_state_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register state event notifier, error %d", ret);
		goto err_after_control_reg;
	}

	devdata->rx_packet_nb.notifier_call = rx_packet_handler;
	devdata->rx_packet_nb.priority = SYNCBOSS_PACKET_CONSUMER_PRIORITY_MISCFIFO;
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register rx packet notifier, error %d", ret);
		goto err_after_state_event_reg;
	}

	return ret;

err_after_state_event_reg:
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);
err_after_control_reg:
	misc_deregister(&devdata->misc_control);
err_after_stream_reg:
	misc_deregister(&devdata->misc_stream);
out:
	return ret;
}

static int syncboss_miscfifo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct miscfifo_dev_data *devdata = dev_get_drvdata(dev);

	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);

	misc_deregister(&devdata->misc_control);
	misc_deregister(&devdata->misc_stream);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_miscfifo_match_table[] = {
	{ .compatible = "meta,syncboss-miscfifo", },
	{ },
};
#else
#define syncboss_miscfifo_match_table NULL
#endif

struct platform_driver syncboss_miscfifo_driver = {
	.driver = {
		.name = "syncboss_miscfifo",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_miscfifo_match_table
	},
	.probe = syncboss_miscfifo_probe,
	.remove = syncboss_miscfifo_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_miscfifo_driver,
};

static int __init syncboss_miscfifo_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_miscfifo_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_miscfifo_init);
module_exit(syncboss_miscfifo_exit);
MODULE_DESCRIPTION("Syncboss MiscFIFO Event Driver");
MODULE_LICENSE("GPL v2");

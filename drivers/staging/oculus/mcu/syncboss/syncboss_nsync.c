// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>
#include <uapi/linux/syncboss.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_nsync.h"

#define NSYNC_MISCFIFO_SIZE 256
#define NSYNC_DEVICE_NAME "syncboss_nsync0"

static irqreturn_t isr_primary_nsync(int irq, void *p)
{
	struct nsync_dev_data *devdata = (struct nsync_dev_data *)p;
	unsigned long flags;
	ktime_t kt = ktime_get();

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->nsync_irq_timestamp_ns = ktime_to_ns(kt);
	devdata->nsync_irq_timestamp_us = ktime_to_us(kt);
	++devdata->nsync_irq_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_nsync(int irq, void *p)
{
	struct nsync_dev_data *devdata = (struct nsync_dev_data *)p;
	struct syncboss_nsync_event event;
	unsigned long flags;
	int status;

	if (devdata->nsync_client_count == 0)
		return IRQ_HANDLED;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	event.timestamp = devdata->nsync_irq_timestamp_ns;
	event.count = devdata->nsync_irq_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	if (!devdata->disable_legacy_nsync) {
		status = miscfifo_send_buf(
				&devdata->nsync_fifo,
				(void *)&event,
				sizeof(event));
		if (status < 0)
			dev_warn_ratelimited(devdata->dev,
					"nsync fifo send failure: %d\n", status);
	}

	return IRQ_HANDLED;
}

static void reset_nsync_values(struct nsync_dev_data *devdata)
{
	unsigned long flags;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->nsync_irq_timestamp_ns = 0;
	devdata->nsync_irq_count = 0;

	devdata->nsync_irq_timestamp_us = 0;
	devdata->nsync_offset_us = 0;
	devdata->prev_mcu_timestamp_us = 0;
	devdata->nsync_offset_status = NSYNC_OFFSET_INVALID;

	devdata->consecutive_drift_limit_count = 0;
	devdata->consecutive_drift_limit_max = 0;
	devdata->drift_limit_count = 0;
	devdata->drift_sum_us = 0;
	devdata->stream_start_time = ktime_get();

	spin_unlock_irqrestore(&devdata->nsync_lock, flags);
}

static int syncboss_state_handler(struct notifier_block *nb, unsigned long event, void *p)
{
	struct nsync_dev_data *devdata = container_of(nb, struct nsync_dev_data, syncboss_state_nb);
	int status;

	switch (event) {
	case SYNCBOSS_EVENT_STREAMING_STARTING:
		status = devm_request_threaded_irq(
			devdata->dev, devdata->nsync_irq, isr_primary_nsync,
			isr_thread_nsync, IRQF_TRIGGER_RISING,
			devdata->misc_nsync.name, devdata);
		if (status < 0)
			dev_err(devdata->dev, "nsync irq registration failed");
		// Fall through
	case SYNCBOSS_EVENT_STREAMING_RESUMING:
		reset_nsync_values(devdata);
		miscfifo_clear(&devdata->nsync_fifo);
		return NOTIFY_OK;

	case SYNCBOSS_EVENT_STREAMING_STOPPED:
		devdata->drift_sum_us = 0;
		devm_free_irq(devdata->dev, devdata->nsync_irq, devdata);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static void update_nsync_offset(struct nsync_dev_data *devdata, struct syncboss_nsync_event *nsync)
{
	int64_t mcu_timestamp_us = (int64_t)nsync->timestamp;
	int64_t offset_us, drift_us;
	int64_t drift_limit_us = S64_MAX;
	unsigned long flags;

	if (devdata->nsync_irq_timestamp_us <= 0) {
		dev_err_ratelimited(devdata->dev, "Dropping nsync message received before nsync IRQ\n");
		return;
	}

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	offset_us = devdata->nsync_irq_timestamp_us - mcu_timestamp_us;
	drift_us = offset_us - devdata->nsync_offset_us;

	if (likely(devdata->nsync_offset_status != NSYNC_OFFSET_INVALID)) {
		devdata->drift_sum_us += drift_us;

		/*
		 * Calculate upper-bound for a plausible clock drift based on rate of nsync events.
		 * Add one to cover for any truncation due to division.
		 */
		drift_limit_us = (((mcu_timestamp_us - devdata->prev_mcu_timestamp_us) *
				 OFFSET_DRIFT_PPM_LIMIT) / 1000000) + 1;
	}

	/*
	 * Cap drift increases to a realistic value. Positive drift values greater than this
	 * are assumed to be due delayed interrupt handling and latency in the IRQ
	 * timestamping path. No limit is applied to negative values.
	 *
	 * Keep some counters for diagnostics and error logging.
	 */
	devdata->nsync_offset_status = NSYNC_OFFSET_VALID;
	if (drift_us > drift_limit_us) {
		devdata->drift_limit_count++;
		devdata->consecutive_drift_limit_count++;

		if (devdata->consecutive_drift_limit_count > devdata->consecutive_drift_limit_max)
			devdata->consecutive_drift_limit_max = devdata->consecutive_drift_limit_count;

		if (devdata->consecutive_drift_limit_count > MAX_CONSECUTIVE_LIMITED_DRIFTS) {
			devdata->nsync_offset_status = NSYNC_OFFSET_ERROR;
			dev_err_ratelimited(devdata->dev,
					"nsync offset drift has exceeded limit %d consecutive times\n",
					devdata->consecutive_drift_limit_count);
		}

		drift_us = drift_limit_us;
	} else {
		devdata->consecutive_drift_limit_count = 0;
	}

	/*
	 * Update offset. Equivalent to 'nsync_offset_us = offset_us' in the
	 * common case (where OFFSET_DRIFT_PPM_LIMIT is not exceeded).
	 */
	devdata->nsync_offset_us += drift_us;
	devdata->prev_mcu_timestamp_us = mcu_timestamp_us;

	spin_unlock_irqrestore(&devdata->nsync_lock, flags);
}

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct nsync_dev_data *devdata = container_of(nb, struct nsync_dev_data, rx_packet_nb);
	struct rx_packet_info *packet_info = pi;
	struct syncboss_driver_data_header_t *header = &packet_info->header;
	const struct syncboss_data *packet = packet_info->data;
	int ret;

	/*
	 * SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE: used for HMDs. Userspace doesn't need
	 * these messages since we attach nsync_offset_us to all streaming messages.
	 *
	 * SYNCBOSS_NSYNC_FRAME_MSG_TYPE: used for starlet only. We must pass this
	 * message on to userspace because it contains fields in addition to
	 * nsync_offset_us that userspace also needs.
	 *
	 * For all other message types, pass the nsync_offset_us and continue
	 * with delivery to userspace.
	 */
	switch (type) {
	case SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE:
	case SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE:
		update_nsync_offset(devdata, (struct syncboss_nsync_event *)packet->data);
		ret = devdata->disable_legacy_nsync ? NOTIFY_STOP : NOTIFY_OK;
		break;
	default:
		header->nsync_offset_us = devdata->nsync_offset_us;
		header->nsync_offset_status = devdata->nsync_offset_status;
		ret = NOTIFY_OK;
		break;
	}

	return ret;
}

static int syncboss_nsync_open(struct inode *inode, struct file *f)
{
	int status;
	struct nsync_dev_data *devdata =
		container_of(f->private_data, struct nsync_dev_data,
			     misc_nsync);

	status = mutex_lock_interruptible(&devdata->miscdevice_mutex);
	if (status) {
		dev_warn(devdata->dev, "nsync open by %s (%d) borted due to signal. status=%d",
			       current->comm, current->pid, status);
		return status;
	}

	if (devdata->nsync_client_count != 0) {
		dev_err(devdata->dev, "nsync open by %s (%d) failed: nsync cannot be opened by more than one user",
				current->comm, current->pid);
		status = -EBUSY;
		goto unlock;
	}

	++devdata->nsync_client_count;

	status = miscfifo_fop_open(f, &devdata->nsync_fifo);
	if (status < 0) {
		dev_err(devdata->dev, "nsync miscfifo open by %s (%d) failed (%d)",
			current->comm, current->pid, status);
		goto decrement_ref;
	}

	devdata->nsync_irq_timestamp_ns = 0;
	devdata->nsync_irq_count = 0;

	devdata->nsync_irq_timestamp_us = 0;
	devdata->nsync_offset_us = 0;
	devdata->nsync_offset_status = NSYNC_OFFSET_INVALID;

	irq_set_status_flags(devdata->nsync_irq, IRQ_DISABLE_UNLAZY);

	reset_nsync_values(devdata);

	dev_info(devdata->dev, "nsync handle opened by %s (%d)",
		 current->comm, current->pid);

decrement_ref:
	if (status < 0)
		--devdata->nsync_client_count;
unlock:
	mutex_unlock(&devdata->miscdevice_mutex);
	return status;
}

static int syncboss_nsync_release(struct inode *inode, struct file *file)
{
	int status;
	struct miscfifo_client *client = file->private_data;
	struct nsync_dev_data *devdata =
		container_of(client->mf, struct nsync_dev_data, nsync_fifo);

	/*
	 * It is unsafe to use the interruptible variant here, as the driver can get
	 * in an inconsistent state if this lock fails. We need to make sure release
	 * handling occurs, since we can't retry releasing the file if, e.g., the
	 * file is released when a process is killed.
	 */
	mutex_lock(&devdata->miscdevice_mutex);

	--devdata->nsync_client_count;

	miscfifo_clear(&devdata->nsync_fifo);

	status = miscfifo_fop_release(inode, file);
	if (status) {
		dev_err(devdata->dev, "nsync release by %s (%d) failed with status %d",
				current->comm, current->pid, status);
		goto out;
	}

	dev_info(devdata->dev, "nsync released by %s (%d)",
		 current->comm, current->pid);

out:
	mutex_unlock(&devdata->miscdevice_mutex);
	return status;
}

static const struct file_operations nsync_fops = {
	.open = syncboss_nsync_open,
	.release = syncboss_nsync_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static ssize_t drift_avg_usec_per_sec_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	unsigned long flags;
	int64_t stream_duration_us, avg;

	spin_lock_irqsave(&devdata->nsync_lock, flags);

	// Calculate stream duration, rounding up to 1us to avoid div-by-zero
	stream_duration_us = max(ktime_to_us(ktime_sub(ktime_get(), devdata->stream_start_time)), (int64_t)1);

	// drift_sum_us will be zero if stream is stopped, so avg will be zero
	avg = (devdata->drift_sum_us * USEC_PER_SEC) / stream_duration_us;

	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%lld\n", avg);
}
static DEVICE_ATTR_RO(drift_avg_usec_per_sec);

static ssize_t drift_limit_consecutive_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	unsigned long flags;
	int count;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	count = devdata->consecutive_drift_limit_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%d\n", count);
}
static DEVICE_ATTR_RO(drift_limit_consecutive);

static ssize_t drift_limit_max_consecutive_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	unsigned long flags;
	int count;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	count = devdata->consecutive_drift_limit_max;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%d\n", count);
}
static DEVICE_ATTR_RO(drift_limit_max_consecutive);

static ssize_t drift_limit_total_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	unsigned long flags;
	int count;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	count = devdata->drift_limit_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%d\n", count);
}
static DEVICE_ATTR_RO(drift_limit_total);

static const struct attribute *nsync_attrs[] = {
	&dev_attr_drift_avg_usec_per_sec.attr,
	&dev_attr_drift_limit_consecutive.attr,
	&dev_attr_drift_limit_max_consecutive.attr,
	&dev_attr_drift_limit_total.attr,
	NULL
};

static int syncboss_nsync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret = 0;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct nsync_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);
	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	mutex_init(&devdata->miscdevice_mutex);
	spin_lock_init(&devdata->nsync_lock);

	/*
	 * T182999318: temporary property to allow gradual deprecation of the
	 * legacy nsync userspace interface.
	 */
	devdata->disable_legacy_nsync = of_property_read_bool(node, "meta,disable-legacy-nsync");

	devdata->nsync_irq = platform_get_irq_byname(pdev, "nsync");
	if (devdata->nsync_irq < 0) {
		dev_err(dev, "No nsync IRQ specified");
		return -EINVAL;
	}

	devdata->nsync_fifo.config.kfifo_size = NSYNC_MISCFIFO_SIZE;
	ret = devm_miscfifo_register(dev, &devdata->nsync_fifo);
	if (ret < 0) {
		dev_err(dev, "failed to set up nsync miscfifo, error %d", ret);
		goto out;
	}

	devdata->misc_nsync.name = NSYNC_DEVICE_NAME;
	devdata->misc_nsync.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_nsync.fops = &nsync_fops;

	ret = misc_register(&devdata->misc_nsync);
	if (ret < 0) {
		dev_err(dev, "failed to register nsync misc device, error %d", ret);
		goto out;
	}

	devdata->syncboss_state_nb.notifier_call = syncboss_state_handler;
	devdata->syncboss_state_nb.priority = SYNCBOSS_STATE_CONSUMER_PRIORITY_NSYNC;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->syncboss_state_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register state event notifier, error %d", ret);
		goto err_after_misc_reg;
	}

	devdata->rx_packet_nb.notifier_call = rx_packet_handler;
	devdata->rx_packet_nb.priority = SYNCBOSS_PACKET_CONSUMER_PRIORITY_NSYNC;
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register rx packet notifier, error %d", ret);
		goto err_after_state_event_reg;
	}

	ret = sysfs_create_files(&dev->kobj, nsync_attrs);
	if (ret < 0) {
		dev_err(dev, "failed to register sysfs nodes %d", ret);
		goto err_after_rx_event_reg;
	}

	return 0;

err_after_rx_event_reg:
	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
err_after_state_event_reg:
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);
err_after_misc_reg:
	misc_deregister(&devdata->misc_nsync);
out:
	return ret;
}

static int syncboss_nsync_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);

	sysfs_remove_files(&dev->kobj, nsync_attrs);
	misc_deregister(&devdata->misc_nsync);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_nsync_match_table[] = {
	{ .compatible = "meta,syncboss-nsync", },
	{ },
};
#else
#define syncboss_nsync_match_table NULL
#endif

struct platform_driver syncboss_nsync_driver = {
	.driver = {
		.name = "syncboss_nsync",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_nsync_match_table
	},
	.probe = syncboss_nsync_probe,
	.remove = syncboss_nsync_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_nsync_driver,
};

static int __init syncboss_nsync_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_nsync_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_nsync_init);
module_exit(syncboss_nsync_exit);
MODULE_DESCRIPTION("Syncboss Nsync Event Driver");
MODULE_LICENSE("GPL v2");

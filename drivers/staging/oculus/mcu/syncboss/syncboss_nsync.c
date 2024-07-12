// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>
#include <uapi/linux/syncboss.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_nsync.h"

static irqreturn_t isr_primary_nsync(int irq, void *p)
{
	struct nsync_dev_data *devdata = (struct nsync_dev_data *)p;
	unsigned long flags;
	ktime_t kt = ktime_get();

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->nsync_irq_timestamp_us = ktime_to_us(kt);
	devdata->nsync_irq_fired = true;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return IRQ_HANDLED;
}

static void reset_nsync_values(struct nsync_dev_data *devdata)
{
	unsigned long flags;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->nsync_irq_fired = false;
	devdata->nsync_irq_timestamp_us = 0;
	devdata->nsync_offset_us = 0;
	devdata->prev_mcu_timestamp_us = 0;
	devdata->nsync_offset_status = SYNCBOSS_TIME_OFFSET_INVALID;

	devdata->consecutive_drift_limit_count = 0;
	devdata->consecutive_drift_limit_max = 0;
	devdata->drift_limit_count = 0;
	devdata->drift_sum_us = 0;
	devdata->stream_start_time = ktime_get();

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	devdata->remote_offset_us = 0;
	devdata->remote_offset_status = SYNCBOSS_TIME_OFFSET_INVALID;
#endif

	spin_unlock_irqrestore(&devdata->nsync_lock, flags);
}

static int syncboss_state_handler(struct notifier_block *nb, unsigned long event, void *p)
{
	struct nsync_dev_data *devdata = container_of(nb, struct nsync_dev_data, syncboss_state_nb);
	int status;

	switch (event) {
	case SYNCBOSS_EVENT_STREAMING_STARTING:
		status = devm_request_irq(
			devdata->dev, devdata->nsync_irq, isr_primary_nsync,
			IRQF_TRIGGER_RISING | IRQF_SHARED , dev_name(devdata->dev), devdata);
		if (status < 0)
			dev_err(devdata->dev, "nsync irq registration failed");
		// Fall through
	case SYNCBOSS_EVENT_STREAMING_RESUMING:
		reset_nsync_values(devdata);
		return NOTIFY_OK;

	case SYNCBOSS_EVENT_STREAMING_STOPPED:
		devdata->drift_sum_us = 0;
		devm_free_irq(devdata->dev, devdata->nsync_irq, devdata);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int handle_display_event(struct nsync_dev_data *devdata, const struct syncboss_data *packet)
{
	struct syncboss_display_event *dfevent = (struct syncboss_display_event *)packet->data;
	int64_t mcu_timestamp_us = (int64_t)dfevent->timestamp;
	int64_t offset_us, drift_us;
	int64_t drift_limit_us = S64_MAX;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&devdata->nsync_lock, flags);

	if (!devdata->nsync_irq_fired) {
		dev_err_ratelimited(devdata->dev,
				"Ignoring display frame event message without matching IRQ. Last nsync_irq_timestamp_us was %lld\n",
				devdata->nsync_irq_timestamp_us);
		ret = -EPERM;
		goto out;
	}
	devdata->nsync_irq_fired = false;

	offset_us = devdata->nsync_irq_timestamp_us - mcu_timestamp_us;
	drift_us = offset_us - devdata->nsync_offset_us;

	if (likely(devdata->nsync_offset_status != SYNCBOSS_TIME_OFFSET_INVALID)) {
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
	devdata->nsync_offset_status = SYNCBOSS_TIME_OFFSET_VALID;
	if (drift_us > drift_limit_us) {
		devdata->drift_limit_count++;
		devdata->consecutive_drift_limit_count++;

		if (devdata->consecutive_drift_limit_count > devdata->consecutive_drift_limit_max)
			devdata->consecutive_drift_limit_max = devdata->consecutive_drift_limit_count;

		if (devdata->consecutive_drift_limit_count > MAX_CONSECUTIVE_LIMITED_DRIFTS) {
			devdata->nsync_offset_status = SYNCBOSS_TIME_OFFSET_ERROR;
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

out:
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);
	return ret;
}

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
static void handle_nsync_event(struct nsync_dev_data *devdata, const struct syncboss_data *packet)
{
	struct syncboss_nsync_event *nevent = (struct syncboss_nsync_event *)packet->data;
	int ret;

	/* Start by handling the fields that are common to SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE */
	ret = handle_display_event(devdata, packet);
	if (ret < 0)
		return;

	/* Handle the additional fields that are specific to SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE */
	if (nevent->offset_valid) {
		devdata->remote_offset_us = nevent->offset_us;
		devdata->remote_offset_status = SYNCBOSS_TIME_OFFSET_VALID;
	}
}
#endif

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct nsync_dev_data *devdata = container_of(nb, struct nsync_dev_data, rx_packet_nb);
	struct rx_packet_info *packet_info = pi;
	struct syncboss_driver_data_header_t *header = &packet_info->header;
	const struct syncboss_data *packet = packet_info->data;
	int ret;

	/*
	 * SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE: used for HMDs.
	 * SYNCBOSS_NSYNC_FRAME_MSG_TYPE: used for starlet only.
	 *
	 * For all other message types, add the nsync offset fields and
	 * continue with delivery to userspace.
	 */
	switch (type) {
#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	case SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE:
		handle_nsync_event(devdata, packet);
		ret = NOTIFY_STOP;
		break;
#endif
	case SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE:
		handle_display_event(devdata, packet);
		ret = NOTIFY_STOP;
		break;
	default:
		header->nsync_offset_us = devdata->nsync_offset_us;
		header->nsync_offset_status = devdata->nsync_offset_status;
#ifdef CONFIG_SYNCBOSS_PERIPHERAL
		header->remote_offset_us = devdata->remote_offset_us;
		header->remote_offset_status = devdata->remote_offset_status;
#endif
		ret = NOTIFY_OK;
		break;
	}

	return ret;
}

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

	spin_lock_init(&devdata->nsync_lock);

	devdata->nsync_irq = platform_get_irq_byname(pdev, "nsync");
	if (devdata->nsync_irq < 0) {
		dev_err(dev, "No nsync IRQ specified");
		return -EINVAL;
	}

	devdata->syncboss_state_nb.notifier_call = syncboss_state_handler;
	devdata->syncboss_state_nb.priority = SYNCBOSS_STATE_CONSUMER_PRIORITY_NSYNC;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->syncboss_state_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register state event notifier, error %d", ret);
		return ret;
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
	return ret;
}

static int syncboss_nsync_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_files(&dev->kobj, nsync_attrs);

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

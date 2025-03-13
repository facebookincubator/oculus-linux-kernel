// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>
#include <uapi/linux/syncboss.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_nsync.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
#define irq_set_affinity_and_hint irq_set_affinity_hint
#endif

static irqreturn_t isr_primary_nsync(int irq, void *p)
{
	struct nsync_dev_data *devdata = (struct nsync_dev_data *)p;
	unsigned long flags;
	ktime_t kt = ktime_get();
	int64_t ts_us = ktime_to_us(kt);

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->ap_ts_now_us = ts_us;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return IRQ_HANDLED;
}

static void reset_nsync_values_locked(struct nsync_dev_data *devdata)
{
	devdata->errors = 0;
	devdata->ap_ts_prev_us = 0;
	devdata->ap_ts_now_us = 0;
	devdata->mcu_ts_prev_us = 0;
	devdata->nsync_offset_us = 0;
	devdata->nsync_offset_status = SYNCBOSS_TIME_OFFSET_INVALID;

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	devdata->remote_offset_us = 0;
	devdata->remote_offset_status = SYNCBOSS_TIME_OFFSET_INVALID;
#endif
}

static void reset_nsync_values(struct nsync_dev_data *devdata)
{
	unsigned long flags;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	reset_nsync_values_locked(devdata);
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);
}

static int syncboss_state_handler(struct notifier_block *nb, unsigned long event, void *p)
{
	struct nsync_dev_data *devdata = container_of(nb, struct nsync_dev_data, syncboss_state_nb);
	struct syncboss_state_data *event_data = p;
	int status;

	switch (event) {
	case SYNCBOSS_EVENT_STREAMING_STARTING:
		cpumask_copy(&devdata->irq_affinity, &event_data->irq_affinity);
		if (irq_set_affinity_and_hint(devdata->nsync_irq, &devdata->irq_affinity) < 0)
			dev_err(devdata->dev, "failed to set irq affinity");
		status = devm_request_irq(
			devdata->dev, devdata->nsync_irq, isr_primary_nsync,
			IRQF_NOBALANCING, dev_name(devdata->dev), devdata);
		if (status < 0)
			dev_err(devdata->dev, "nsync irq registration failed");
		fallthrough;
	case SYNCBOSS_EVENT_STREAMING_RESUMING:
		reset_nsync_values(devdata);
		return NOTIFY_OK;

	case SYNCBOSS_EVENT_STREAMING_STOPPED:
		irq_set_affinity_and_hint(devdata->nsync_irq, NULL);
		devm_free_irq(devdata->dev, devdata->nsync_irq, devdata);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int handle_display_event(struct nsync_dev_data *devdata, const struct syncboss_data *packet)
{
	struct syncboss_display_event *dfevent = (struct syncboss_display_event *)packet->data;
	int64_t mcu_ts_now_us = (int64_t)dfevent->timestamp;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&devdata->nsync_lock, flags);

	/*
	 * MCU is the timing source of truth, since its toggles/measurements are
	 * hardware-backed. AP measurements are subject to jitter because they're
	 * executed in software.
	 */
	if (devdata->ap_ts_prev_us != 0 && devdata->ap_ts_now_us != 0 && devdata->mcu_ts_prev_us != 0 && mcu_ts_now_us != 0) {
		int64_t ap_delta_us = devdata->ap_ts_now_us - devdata->ap_ts_prev_us;
		int64_t mcu_delta_us = mcu_ts_now_us - devdata->mcu_ts_prev_us;
		int64_t histogram_index = ap_delta_us - mcu_delta_us;

		/* Stats for characterization/debugging. We don't *need* these, but they're nice to have. */
		histogram_index /= 4;
		if (histogram_index < -NSYNC_HISTOGRAM_OFFSET)
			histogram_index = -NSYNC_HISTOGRAM_OFFSET;
		else if (histogram_index > NSYNC_HISTOGRAM_OFFSET)
			histogram_index = NSYNC_HISTOGRAM_OFFSET;
		histogram_index += NSYNC_HISTOGRAM_OFFSET;
		++devdata->debug.histogram[histogram_index];

		if (abs(ap_delta_us - mcu_delta_us) <= devdata->max_delta_error_us) {
			if (devdata->nsync_offset_status != SYNCBOSS_TIME_OFFSET_VALID)
				if (devdata->errors > devdata->debug.sync_max)
					devdata->debug.sync_max = devdata->errors;

			devdata->nsync_offset_us = devdata->ap_ts_now_us - mcu_ts_now_us;
			devdata->nsync_offset_status = SYNCBOSS_TIME_OFFSET_VALID;
			devdata->errors = 0;
		} else {
			++devdata->errors;

			if (devdata->errors > devdata->debug.errors_max)
				devdata->debug.errors_max = devdata->errors;

			if (devdata->errors >= devdata->max_consecutive_errors)
				ret = -EINVAL;
		}
	} else {
		/*
		 * This creates an artificial "error" on the first packet or on an MCU-driven
		 * re-sync, but we could also interpret "error" as "lock acquisition
		 * latency".
		 */
		++devdata->errors;
	}

	devdata->ap_ts_prev_us = devdata->ap_ts_now_us;
	devdata->mcu_ts_prev_us = mcu_ts_now_us;

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
	 *
	 * TODO(T209987338): use NOTIFY_STOP for NSYNC_FRAME/DISPLAY_FRAME
	 * messages once userspace no longer requires them.
	 *
	 */
	switch (type) {
#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	case SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE:
		handle_nsync_event(devdata, packet);
		ret = NOTIFY_OK;
		break;
#endif
	case SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE:
		handle_display_event(devdata, packet);
		ret = NOTIFY_OK;
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

static ssize_t dump_stats_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	int i;

	/* Logging these instead of snprintf into buf because they could be > PAGE_SIZE */
	dev_info(devdata->dev, "max consecutive errors: %u\n", devdata->debug.errors_max);
	dev_info(devdata->dev, "max deltas to sync: %u\n", devdata->debug.sync_max);
	dev_info(devdata->dev, "histogram[<%d us]: %u\n", -NSYNC_HISTOGRAM_OFFSET * 4, devdata->debug.histogram[0]);
	for (i = 1; i < (NSYNC_HISTOGRAM_SIZE - 1); ++i)
		dev_info(devdata->dev, "histogram[%d us]: %u\n", (i - NSYNC_HISTOGRAM_OFFSET) * 4, devdata->debug.histogram[i]);
	dev_info(devdata->dev, "histogram[>%d us]: %u\n", NSYNC_HISTOGRAM_OFFSET * 4, devdata->debug.histogram[NSYNC_HISTOGRAM_SIZE - 1]);

	return 0;
}
static DEVICE_ATTR_RO(dump_stats);

static const struct attribute *nsync_attrs[] = {
	&dev_attr_dump_stats.attr,
	NULL
};

static int syncboss_nsync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct nsync_dev_data *devdata;
	struct device_node *parent_node = of_get_parent(node);
	bool is_vsync;
	int ret = 0;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		if (parent_node)
			of_node_put(parent_node);
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct nsync_dev_data), GFP_KERNEL);
	if (!devdata) {
		ret = -ENOMEM;
		goto err_after_get_parent;
	}

	dev_set_drvdata(dev, devdata);
	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	is_vsync = of_property_read_bool(node, "meta,is-vsync");
	devdata->max_delta_error_us = is_vsync ? VSYNC_MAX_DELTA_ERROR_US : NSYNC_MAX_DELTA_ERROR_US;
	devdata->max_consecutive_errors = is_vsync ? VSYNC_MAX_CONSECUTIVE_ERRORS : NSYNC_MAX_CONSECUTIVE_ERRORS;
	dev_info(dev, "max-delta-error-us: %lld\n", devdata->max_delta_error_us);
	dev_info(dev, "max-consecutive-errors: %u\n", devdata->max_consecutive_errors);

	spin_lock_init(&devdata->nsync_lock);

	devdata->nsync_irq = platform_get_irq_byname(pdev, "nsync");
	if (devdata->nsync_irq < 0) {
		dev_err(dev, "No nsync IRQ specified");
		ret = -EINVAL;
		goto err_after_get_parent;
	}

	devdata->syncboss_state_nb.notifier_call = syncboss_state_handler;
	devdata->syncboss_state_nb.priority = SYNCBOSS_STATE_CONSUMER_PRIORITY_NSYNC;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->syncboss_state_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register state event notifier, error %d", ret);
		goto err_after_get_parent;
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

	of_node_put(parent_node);

	return 0;

err_after_rx_event_reg:
	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
err_after_state_event_reg:
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);
err_after_get_parent:
	of_node_put(parent_node);
	return ret;
}

static int syncboss_nsync_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);

	sysfs_remove_files(&dev->kobj, nsync_attrs);
	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);

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

// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>
#include <uapi/linux/syncboss.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_timesync.h"

#define DEFAULT_TIMESYNC_PERIOD_MS 33

static void reset_histogram(struct timesync_dev_data *devdata)
{
	memset(devdata->stats.histogram, 0, DRIFT_HISTOGRAM_SIZE * sizeof(*devdata->stats.histogram));
}

static void reset_timesync_values(struct timesync_dev_data *devdata)
{
	spin_lock(&devdata->lock);

	devdata->waiting_for_msg = false;
	devdata->ap_ts_us = 0;

	devdata->timesync_offset_us = 0;
	devdata->timesync_offset_status = SYNCBOSS_TIME_OFFSET_INVALID;

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	devdata->remote_offset_us = 0;
	devdata->remote_offset_status = SYNCBOSS_TIME_OFFSET_INVALID;
#endif

	devdata->stats.prev_ap_ts_us = 0;
	devdata->stats.prev_mcu_ts_us = 0;
	devdata->stats.min_drift_us = 0;
	devdata->stats.max_drift_us = 0;
	reset_histogram(devdata);

	spin_unlock(&devdata->lock);
}

static void trigger_timesync_event(struct timesync_dev_data *devdata)
{
	unsigned long flags;

	/*
	 * Disabling interrupts and preemption is important here for recording
	 * the timestamp as close to the GPIO toggle as possible.
	 */
	spin_lock_irqsave(&devdata->lock, flags);
	if (unlikely(devdata->waiting_for_msg))
		dev_warn_ratelimited(devdata->dev, "triggering new irq before MCU acknowledged last\n");
	devdata->waiting_for_msg = true;

	devdata->stats.prev_ap_ts_us = devdata->ap_ts_us;

	/* The following two lines should be as close together as possible */
	gpio_set_value(devdata->gpio, 1);
	devdata->ap_ts_us = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&devdata->lock, flags);

	udelay(1);
	gpio_set_value(devdata->gpio, 0);
}

static void enable_timesync(struct timesync_dev_data *devdata)
{
	pinctrl_select_state(devdata->pinctrl, devdata->pinctrl_active_state);
	trigger_timesync_event(devdata);

	spin_lock(&devdata->lock);
	devdata->period_ktime = ms_to_ktime(devdata->period_ms);
	spin_unlock(&devdata->lock);

	hrtimer_start(&devdata->timer, devdata->period_ktime, HRTIMER_MODE_REL);
}

static void disable_timesync(struct timesync_dev_data *devdata)
{
	hrtimer_cancel(&devdata->timer);
	pinctrl_select_state(devdata->pinctrl, devdata->pinctrl_default_state);
}

static int syncboss_state_handler(struct notifier_block *nb, unsigned long event, void *p)
{
	struct timesync_dev_data *devdata = container_of(nb, struct timesync_dev_data, syncboss_state_nb);

	switch (event) {
	case SYNCBOSS_EVENT_STREAMING_STARTING:
	case SYNCBOSS_EVENT_STREAMING_RESUMING:
		reset_timesync_values(devdata);
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_STARTED:
	case SYNCBOSS_EVENT_STREAMING_RESUMED:
		enable_timesync(devdata);
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_STOPPING:
	case SYNCBOSS_EVENT_STREAMING_SUSPENDING:
		disable_timesync(devdata);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static void update_stats(struct timesync_dev_data *devdata,
		int64_t ap_ts_us, int64_t mcu_ts_us)
{
	int64_t ap_delta_us, mcu_delta_us, drift_us;
	int idx;

	if (devdata->stats.prev_ap_ts_us && devdata->stats.prev_mcu_ts_us) {
		ap_delta_us = ap_ts_us - devdata->stats.prev_ap_ts_us;
		mcu_delta_us = mcu_ts_us - devdata->stats.prev_mcu_ts_us;
		drift_us = ap_delta_us - mcu_delta_us;

		if (drift_us > devdata->stats.max_drift_us)
			devdata->stats.max_drift_us = drift_us;
		if (drift_us < devdata->stats.min_drift_us)
			devdata->stats.min_drift_us = drift_us;

		if (drift_us <= -DRIFT_HISTOGRAM_OFFSET)
			idx = 0;
		else if (drift_us >= DRIFT_HISTOGRAM_OFFSET)
			idx = DRIFT_HISTOGRAM_SIZE - 1;
		else
			idx = drift_us + DRIFT_HISTOGRAM_OFFSET;
		++devdata->stats.histogram[idx];
	}
}

static void handle_display_event(struct timesync_dev_data *devdata, const struct syncboss_data *packet)
{
	struct syncboss_display_event *dfevent = (struct syncboss_display_event *)packet->data;
	int64_t mcu_ts_us = dfevent->timestamp;
	int64_t ap_ts_us;

	spin_lock(&devdata->lock);
	if (unlikely(!devdata->waiting_for_msg)) {
		dev_warn_ratelimited(devdata->dev, "ignoring mcu timestamp without corresponding IRQ\n");
		goto out;
	}
	devdata->waiting_for_msg = false;

	ap_ts_us = devdata->ap_ts_us;

	devdata->timesync_offset_us = ap_ts_us - mcu_ts_us;
	devdata->timesync_offset_status = SYNCBOSS_TIME_OFFSET_VALID;

	update_stats(devdata, ap_ts_us, mcu_ts_us);

	devdata->stats.prev_mcu_ts_us = mcu_ts_us;
out:
	spin_unlock(&devdata->lock);
}

#ifdef CONFIG_SYNCBOSS_PERIPHERAL
static void handle_nsync_event(struct timesync_dev_data *devdata, const struct syncboss_data *packet)
{
	struct syncboss_nsync_event *nevent = (struct syncboss_nsync_event *)packet->data;

	/* Start by handling the fields that are common to SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE */
	handle_display_event(devdata, packet);

	/* Handle the additional fields that are specific to SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE */
	if (nevent->offset_valid) {
		devdata->remote_offset_us = nevent->offset_us;
		devdata->remote_offset_status = SYNCBOSS_TIME_OFFSET_VALID;
	}
}
#endif

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct timesync_dev_data *devdata = container_of(nb, struct timesync_dev_data, rx_packet_nb);
	struct rx_packet_info *packet_info = pi;
	struct syncboss_driver_data_header_t *header = &packet_info->header;
	const struct syncboss_data *packet = packet_info->data;
	int ret = NOTIFY_OK;

	/*
	 * SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE: used for HMDs.
	 * SYNCBOSS_NSYNC_FRAME_MSG_TYPE: used for starlet only.
	 *
	 * For all other message types, add the timesync offset fields and
	 * continue with delivery to userspace.
	 *
	 * TODO(T209987338): use NOTIFY_STOP for NSYNC_FRAME/DISPLAY_FRAME
	 * messages once userspace no longer requires them.
	 */
	switch (type) {
#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	case SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE:
		handle_nsync_event(devdata, packet);
		break;
#endif
	case SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE:
		handle_display_event(devdata, packet);
		break;
	default:
		break;
	}

	header->nsync_offset_us = devdata->timesync_offset_us;
	header->nsync_offset_status = devdata->timesync_offset_status;
#ifdef CONFIG_SYNCBOSS_PERIPHERAL
	header->remote_offset_us = devdata->remote_offset_us;
	header->remote_offset_status = devdata->remote_offset_status;
#endif

	return ret;
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	struct timesync_dev_data *devdata =
		container_of(timer, struct timesync_dev_data, timer);

	trigger_timesync_event(devdata);
	hrtimer_forward_now(timer, devdata->period_ktime);

	return HRTIMER_RESTART;
}

static ssize_t drift_histogram_show(
		struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);
	int i, pos = 0;

	spin_lock(&devdata->lock);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"usec per %dmsec: count\n",
			devdata->period_ms);

	for (i = 0; i < DRIFT_HISTOGRAM_SIZE; ++i) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"%12s%3d: %u\n",
			i == 0 ? "<=" : (i == DRIFT_HISTOGRAM_SIZE - 1) ? ">=" : "  ",
			i - DRIFT_HISTOGRAM_OFFSET,
			devdata->stats.histogram[i]);
	}
	spin_unlock(&devdata->lock);

	return pos;
}
static ssize_t drift_histogram_store(
		struct device *dev,
		struct device_attribute *attr,
	        const char *buf, size_t count) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);

	spin_lock(&devdata->lock);
	reset_histogram(devdata);
	spin_unlock(&devdata->lock);

	return count;
}
static DEVICE_ATTR_RW(drift_histogram);

static ssize_t max_drift_us_show(
		struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);
	size_t ret;

	spin_lock(&devdata->lock);
	ret = scnprintf(buf, PAGE_SIZE, "%lld\n", devdata->stats.max_drift_us);
	spin_unlock(&devdata->lock);

	return ret;
}
static ssize_t max_drift_us_store(
		struct device *dev,
		struct device_attribute *attr,
	        const char *buf, size_t count) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);

	spin_lock(&devdata->lock);
	devdata->stats.max_drift_us = 0;
	spin_unlock(&devdata->lock);

	return count;
}
static DEVICE_ATTR_RW(max_drift_us);

static ssize_t min_drift_us_show(
		struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);
	size_t ret;

	spin_lock(&devdata->lock);
	ret = scnprintf(buf, PAGE_SIZE, "%lld\n", devdata->stats.min_drift_us);
	spin_unlock(&devdata->lock);

	return ret;
}
static ssize_t min_drift_us_store(
		struct device *dev,
		struct device_attribute *attr,
	        const char *buf, size_t count) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);

	spin_lock(&devdata->lock);
	devdata->stats.min_drift_us = 0;
	spin_unlock(&devdata->lock);

	return count;
}
static DEVICE_ATTR_RW(min_drift_us);

static ssize_t period_ms_show(
		struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);
	int ret;

	spin_lock(&devdata->lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->period_ms);
	spin_unlock(&devdata->lock);

	return ret;
}
static ssize_t period_ms_store(
		struct device *dev,
		struct device_attribute *attr,
	        const char *buf, size_t count) {
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);
	int ret = count;

	spin_lock(&devdata->lock);
	if (kstrtou32(buf, 10, &devdata->period_ms))
		ret = -EINVAL;
	spin_unlock(&devdata->lock);

	return ret;
}
static DEVICE_ATTR_RW(period_ms);

static const struct attribute *timesync_attrs[] = {
	&dev_attr_drift_histogram.attr,
	&dev_attr_min_drift_us.attr,
	&dev_attr_max_drift_us.attr,
	&dev_attr_period_ms.attr,
	NULL
};

static int syncboss_timesync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct timesync_dev_data *devdata;
	struct device_node *parent_node = of_get_parent(node);
	int ret = 0;

	if (!parent_node ||
	    (!of_device_is_compatible(parent_node, "meta,syncboss") &&
	     !of_device_is_compatible(parent_node, "meta,syncboss-spi"))) {
		dev_err(dev, "failed to find compatible parent device");
		if (parent_node)
			of_node_put(parent_node);
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct timesync_dev_data), GFP_KERNEL);
	if (!devdata) {
		ret = -ENOMEM;
		goto err_after_get_parent;
	}

	dev_set_drvdata(dev, devdata);
	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	spin_lock_init(&devdata->lock);

	devdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(devdata->pinctrl)) {
		dev_err(dev, "failed to get pinctrl\n");
		goto err_after_get_parent;
	}

	devdata->pinctrl_default_state = pinctrl_lookup_state(devdata->pinctrl, "default");
	if (IS_ERR(devdata->pinctrl_default_state)) {
		dev_err(dev, "pinctrl has no default state\n");
		goto err_after_get_parent;
	}

	devdata->pinctrl_active_state = pinctrl_lookup_state(devdata->pinctrl, "active");
	if (IS_ERR(devdata->pinctrl_active_state)) {
		dev_err(dev, "pinctrl has no active state\n");
		goto err_after_get_parent;
	}

	devdata->gpio = of_get_named_gpio(
		dev->of_node, "timesync-gpio", 0);
	if (!gpio_is_valid(devdata->gpio)) {
		dev_err(dev, "invalid gpio\n");
		ret = devdata->gpio;
		goto err_after_get_parent;
	}

	ret = of_property_read_u32(node, "meta,period-ms", &devdata->period_ms);
	if (ret < 0)
		devdata->period_ms = DEFAULT_TIMESYNC_PERIOD_MS;

	hrtimer_init(&devdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	devdata->timer.function = timer_callback;

	devdata->syncboss_state_nb.notifier_call = syncboss_state_handler;
	devdata->syncboss_state_nb.priority = SYNCBOSS_STATE_CONSUMER_PRIORITY_TIMESYNC;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->syncboss_state_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register state event notifier, error %d", ret);
		goto err_after_get_parent;
	}

	devdata->rx_packet_nb.notifier_call = rx_packet_handler;
	devdata->rx_packet_nb.priority = SYNCBOSS_PACKET_CONSUMER_PRIORITY_TIMESYNC;
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register rx packet notifier, error %d", ret);
		goto err_after_state_event_reg;
	}

	ret = sysfs_create_files(&dev->kobj,timesync_attrs);
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

static int syncboss_timesync_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);

	sysfs_remove_files(&dev->kobj, timesync_attrs);
	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_timesync_match_table[] = {
	{ .compatible = "meta,syncboss-timesync", },
	{ },
};
#else
#define syncboss_timesync_match_table NULL
#endif

struct platform_driver syncboss_timesync_driver = {
	.driver = {
		.name = "syncboss_timesync",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_timesync_match_table
	},
	.probe = syncboss_timesync_probe,
	.remove = syncboss_timesync_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_timesync_driver,
};

static int __init syncboss_timesync_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_timesync_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_timesync_init);
module_exit(syncboss_timesync_exit);
MODULE_DESCRIPTION("Syncboss Timesync Driver");
MODULE_LICENSE("GPL v2");

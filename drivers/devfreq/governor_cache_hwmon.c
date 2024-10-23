// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2015, 2019-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "cache-hwmon: " fmt

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/devfreq.h>
#include <trace/events/power.h>
#include "governor.h"
#include "governor_cache_hwmon.h"

struct cache_hwmon_node {
	unsigned int		cycles_per_low_req;
	unsigned int		cycles_per_med_req;
	unsigned int		cycles_per_high_req;
	unsigned int		min_busy;
	unsigned int		max_busy;
	unsigned int		tolerance_mrps;
	unsigned int		guard_band_mhz;
	unsigned int		decay_rate;
	unsigned long		prev_mhz;
	ktime_t			prev_ts;
	bool			mon_started;
	struct list_head	list;
	void			*orig_data;
	struct cache_hwmon	*hw;
	struct attribute_group	*attr_grp;
};

static LIST_HEAD(cache_hwmon_list);
static DEFINE_MUTEX(list_lock);

static int use_cnt;
static DEFINE_MUTEX(register_lock);

static DEFINE_MUTEX(monitor_lock);

#define show_attr(name) \
static ssize_t name##_show(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct cache_hwmon_node *hw = df->data;				\
	return scnprintf(buf, PAGE_SIZE, "%u\n", hw->name);		\
}

#define store_attr(name, _min, _max) \
static ssize_t name##_store(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	struct devfreq *df = to_devfreq(dev);				\
	struct cache_hwmon_node *hw = df->data;				\
	ret = kstrtoint(buf, 10, &val);					\
	if (ret < 0)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	hw->name = val;							\
	return count;							\
}

#define MIN_MS	10U
#define MAX_MS	500U

static struct cache_hwmon_node *find_hwmon_node(struct devfreq *df)
{
	struct cache_hwmon_node *node, *found = NULL;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &cache_hwmon_list, list)
		if (node->hw->dev == df->dev.parent ||
		    node->hw->of_node == df->dev.parent->of_node) {
			found = node;
			break;
		}
	mutex_unlock(&list_lock);

	return found;
}

static unsigned long measure_mrps_and_set_irq(struct cache_hwmon_node *node,
			struct mrps_stats *stat)
{
	ktime_t ts;
	unsigned int us;
	struct cache_hwmon *hw = node->hw;

	/*
	 * Since we are stopping the counters, we don't want this short work
	 * to be interrupted by other tasks and cause the measurements to be
	 * wrong. Not blocking interrupts to avoid affecting interrupt
	 * latency and since they should be short anyway because they run in
	 * atomic context.
	 */
	preempt_disable();

	ts = ktime_get();
	us = ktime_to_us(ktime_sub(ts, node->prev_ts));
	if (!us)
		us = 1;

	hw->meas_mrps_and_set_irq(hw, node->tolerance_mrps, us, stat);
	node->prev_ts = ts;

	preempt_enable();

	trace_cache_hwmon_meas(dev_name(hw->df->dev.parent), stat->mrps[HIGH],
			       stat->mrps[MED], stat->mrps[LOW],
			       stat->busy_percent, us);
	return 0;
}

static void compute_cache_freq(struct cache_hwmon_node *node,
		struct mrps_stats *mrps, unsigned long *freq)
{
	unsigned long new_mhz;
	unsigned int busy;

	new_mhz = mrps->mrps[HIGH] * node->cycles_per_high_req
		+ mrps->mrps[MED] * node->cycles_per_med_req
		+ mrps->mrps[LOW] * node->cycles_per_low_req;

	busy = max(node->min_busy, mrps->busy_percent);
	busy = min(node->max_busy, busy);

	new_mhz *= 100;
	new_mhz /= busy;

	if (new_mhz < node->prev_mhz) {
		new_mhz = new_mhz * node->decay_rate + node->prev_mhz
				* (100 - node->decay_rate);
		new_mhz /= 100;
	}
	node->prev_mhz = new_mhz;

	new_mhz += node->guard_band_mhz;
	*freq = new_mhz * 1000;
	trace_cache_hwmon_update(dev_name(node->hw->df->dev.parent), *freq);
}

#define TOO_SOON_US	(1 * USEC_PER_MSEC)
int update_cache_hwmon(struct cache_hwmon *hwmon)
{
	struct cache_hwmon_node *node;
	struct devfreq *df;
	ktime_t ts;
	unsigned int us;
	int ret;

	if (!hwmon)
		return -EINVAL;
	df = hwmon->df;
	if (!df)
		return -ENODEV;
	node = df->data;
	if (!node)
		return -ENODEV;

	mutex_lock(&monitor_lock);
	if (!node->mon_started) {
		mutex_unlock(&monitor_lock);
		return -EBUSY;
	}

	dev_dbg(df->dev.parent, "Got update request\n");
	devfreq_monitor_stop(df);

	/*
	 * Don't recalc cache freq if the interrupt comes right after a
	 * previous cache freq calculation.  This is done for two reasons:
	 *
	 * 1. Sampling the cache request during a very short duration can
	 *    result in a very inaccurate measurement due to very short
	 *    bursts.
	 * 2. This can only happen if the limit was hit very close to the end
	 *    of the previous sample period. Which means the current cache
	 *    request estimate is not very off and doesn't need to be
	 *    readjusted.
	 */
	ts = ktime_get();
	us = ktime_to_us(ktime_sub(ts, node->prev_ts));
	if (us > TOO_SOON_US) {
		mutex_lock(&df->lock);
		ret = update_devfreq(df);
		if (ret < 0)
			dev_err(df->dev.parent,
				"Unable to update freq on req! (%d)\n", ret);
		mutex_unlock(&df->lock);
	}

	devfreq_monitor_start(df);

	mutex_unlock(&monitor_lock);
	return 0;
}

static int devfreq_cache_hwmon_get_freq(struct devfreq *df,
					unsigned long *freq)
{
	struct mrps_stats stat;
	struct cache_hwmon_node *node = df->data;

	memset(&stat, 0, sizeof(stat));
	measure_mrps_and_set_irq(node, &stat);
	compute_cache_freq(node, &stat, freq);

	return 0;
}

show_attr(cycles_per_low_req);
store_attr(cycles_per_low_req, 1U, 100U);
static DEVICE_ATTR_RW(cycles_per_low_req);
show_attr(cycles_per_med_req);
store_attr(cycles_per_med_req, 1U, 100U);
static DEVICE_ATTR_RW(cycles_per_med_req);
show_attr(cycles_per_high_req);
store_attr(cycles_per_high_req, 1U, 100U);
static DEVICE_ATTR_RW(cycles_per_high_req);
show_attr(min_busy);
store_attr(min_busy, 1U, 100U);
static DEVICE_ATTR_RW(min_busy);
show_attr(max_busy);
store_attr(max_busy, 1U, 100U);
static DEVICE_ATTR_RW(max_busy);
show_attr(tolerance_mrps);
store_attr(tolerance_mrps, 0U, 100U);
static DEVICE_ATTR_RW(tolerance_mrps);
show_attr(guard_band_mhz);
store_attr(guard_band_mhz, 0U, 500U);
static DEVICE_ATTR_RW(guard_band_mhz);
show_attr(decay_rate);
store_attr(decay_rate, 0U, 100U);
static DEVICE_ATTR_RW(decay_rate);

static struct attribute *dev_attr[] = {
	&dev_attr_cycles_per_low_req.attr,
	&dev_attr_cycles_per_med_req.attr,
	&dev_attr_cycles_per_high_req.attr,
	&dev_attr_min_busy.attr,
	&dev_attr_max_busy.attr,
	&dev_attr_tolerance_mrps.attr,
	&dev_attr_guard_band_mhz.attr,
	&dev_attr_decay_rate.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name = "cache_hwmon",
	.attrs = dev_attr,
};

static int start_monitoring(struct devfreq *df)
{
	int ret;
	struct mrps_stats mrps;
	struct device *dev = df->dev.parent;
	struct cache_hwmon_node *node;
	struct cache_hwmon *hw;

	node = find_hwmon_node(df);
	if (!node) {
		dev_err(dev, "Unable to find HW monitor!\n");
		return -ENODEV;
	}
	hw = node->hw;
	hw->df = df;
	node->orig_data = df->data;
	df->data = node;

	node->prev_ts = ktime_get();
	node->prev_mhz = 0;
	mrps.mrps[HIGH] = (df->previous_freq / 1000) - node->guard_band_mhz;
	mrps.mrps[HIGH] /= node->cycles_per_high_req;
	mrps.mrps[MED] = mrps.mrps[LOW] = 0;

	ret = hw->start_hwmon(hw, &mrps);
	if (ret < 0) {
		dev_err(dev, "Unable to start HW monitor! (%d)\n", ret);
		goto err_start;
	}

	mutex_lock(&monitor_lock);
	devfreq_monitor_start(df);
	node->mon_started = true;
	mutex_unlock(&monitor_lock);

	ret = sysfs_create_group(&df->dev.kobj, &dev_attr_group);
	if (ret < 0) {
		dev_err(dev, "Error creating sys entries! (%d)\n", ret);
		goto sysfs_fail;
	}

	return 0;

sysfs_fail:
	mutex_lock(&monitor_lock);
	node->mon_started = false;
	devfreq_monitor_stop(df);
	mutex_unlock(&monitor_lock);
	hw->stop_hwmon(hw);
err_start:
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
	return ret;
}

static void stop_monitoring(struct devfreq *df)
{
	struct cache_hwmon_node *node = df->data;
	struct cache_hwmon *hw = node->hw;

	sysfs_remove_group(&df->dev.kobj, &dev_attr_group);
	mutex_lock(&monitor_lock);
	node->mon_started = false;
	devfreq_monitor_stop(df);
	mutex_unlock(&monitor_lock);
	hw->stop_hwmon(hw);
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
}

static int devfreq_cache_hwmon_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret;
	unsigned int sample_ms;

	switch (event) {
	case DEVFREQ_GOV_START:
		sample_ms = df->profile->polling_ms;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		df->profile->polling_ms = sample_ms;

		ret = start_monitoring(df);
		if (ret < 0)
			return ret;

		dev_dbg(df->dev.parent, "Enabled Cache HW monitor governor\n");
		break;
	case DEVFREQ_GOV_STOP:
		stop_monitoring(df);
		dev_dbg(df->dev.parent, "Disabled Cache HW monitor governor\n");
		break;
	case DEVFREQ_GOV_UPDATE_INTERVAL:
		sample_ms = *(unsigned int *)data;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		devfreq_update_interval(df, &sample_ms);
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_cache_hwmon = {
	.name = "cache_hwmon",
	.get_target_freq = devfreq_cache_hwmon_get_freq,
	.event_handler = devfreq_cache_hwmon_ev_handler,
};

int register_cache_hwmon(struct device *dev, struct cache_hwmon *hwmon)
{
	int ret = 0;
	struct cache_hwmon_node *node;

	if (!hwmon->dev && !hwmon->of_node)
		return -EINVAL;

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->cycles_per_med_req = 20;
	node->cycles_per_high_req = 35;
	node->min_busy = 100;
	node->max_busy = 100;
	node->tolerance_mrps = 5;
	node->guard_band_mhz = 100;
	node->decay_rate = 90;
	node->hw = hwmon;
	node->attr_grp = &dev_attr_group;

	mutex_lock(&register_lock);
	if (!use_cnt) {
		ret = devfreq_add_governor(&devfreq_cache_hwmon);
		if (!ret)
			use_cnt++;
	}
	mutex_unlock(&register_lock);

	if (!ret) {
		dev_dbg(dev, "Cache HWmon governor registered.\n");
	} else {
		dev_err(dev, "Failed to add Cache HWmon governor: %d\n", ret);
		return ret;
	}

	mutex_lock(&list_lock);
	list_add_tail(&node->list, &cache_hwmon_list);
	mutex_unlock(&list_lock);

	return ret;
}

MODULE_DESCRIPTION("HW monitor based cache freq driver");
MODULE_LICENSE("GPL v2");

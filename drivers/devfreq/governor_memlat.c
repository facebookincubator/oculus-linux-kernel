// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "mem_lat: " fmt

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
#include <linux/device.h>
#include <linux/of.h>
#include <linux/devfreq.h>
#include "governor.h"
#include "governor_memlat.h"

#include <trace/events/power.h>

struct memlat_node {
	unsigned int		ratio_ceil;
	unsigned int		stall_floor;
	unsigned int		wb_pct_thres;
	unsigned int		wb_filter_ratio;
	bool			mon_started;
	bool			already_zero;
	struct list_head	list;
	void			*orig_data;
	struct memlat_hwmon	*hw;
	struct devfreq_governor	*gov;
	struct attribute_group	*attr_grp;
	unsigned long		resume_freq;
};

static LIST_HEAD(memlat_list);
static DEFINE_MUTEX(list_lock);

static int memlat_use_cnt;
static int compute_use_cnt;
static DEFINE_MUTEX(state_lock);

#define show_attr(name) \
static ssize_t name##_show(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct memlat_node *hw = df->data;				\
	return scnprintf(buf, PAGE_SIZE, "%u\n", hw->name);		\
}

#define store_attr(name, _min, _max) \
static ssize_t name##_store(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct memlat_node *hw = df->data;				\
	int ret;							\
	unsigned int val;						\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret < 0)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	hw->name = val;							\
	return count;							\
}

static ssize_t freq_map_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct memlat_node *n = df->data;
	struct core_dev_map *map = n->hw->freq_map;
	unsigned int cnt = 0;

	cnt += scnprintf(buf, PAGE_SIZE, "Core freq (MHz)\tDevice BW\n");

	while (map->core_mhz && cnt < PAGE_SIZE) {
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "%15u\t%9u\n",
				map->core_mhz, map->target_freq);
		map++;
	}
	if (cnt < PAGE_SIZE)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "\n");

	return cnt;
}

static DEVICE_ATTR_RO(freq_map);

static unsigned long core_to_dev_freq(struct memlat_node *node,
		unsigned long coref)
{
	struct memlat_hwmon *hw = node->hw;
	struct core_dev_map *map = hw->freq_map;
	unsigned long freq = 0;

	if (!map)
		goto out;

	while (map->core_mhz && map->core_mhz < coref)
		map++;
	if (!map->core_mhz)
		map--;
	freq = map->target_freq;

out:
	pr_debug("freq: %lu -> dev: %lu\n", coref, freq);
	return freq;
}

static struct memlat_node *find_memlat_node(struct devfreq *df)
{
	struct memlat_node *node, *found = NULL;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &memlat_list, list)
		if (node->hw->dev == df->dev.parent ||
		    node->hw->of_node == df->dev.parent->of_node) {
			found = node;
			break;
		}
	mutex_unlock(&list_lock);

	return found;
}

static int start_monitor(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;
	struct device *dev = df->dev.parent;
	int ret;

	ret = hw->start_hwmon(hw);

	if (ret < 0) {
		dev_err(dev, "Unable to start HW monitor! (%d)\n", ret);
		return ret;
	}

	if (!hw->should_ignore_df_monitor)
		devfreq_monitor_start(df);

	node->mon_started = true;

	return 0;
}

static void stop_monitor(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;

	node->mon_started = false;

	if (!hw->should_ignore_df_monitor)
		devfreq_monitor_stop(df);

	hw->stop_hwmon(hw);
}

static int gov_start(struct devfreq *df)
{
	int ret = 0;
	struct device *dev = df->dev.parent;
	struct memlat_node *node;
	struct memlat_hwmon *hw;

	node = find_memlat_node(df);
	if (!node) {
		dev_err(dev, "Unable to find HW monitor!\n");
		return -ENODEV;
	}
	hw = node->hw;

	hw->df = df;
	node->orig_data = df->data;
	df->data = node;

	ret = start_monitor(df);
	if (ret < 0)
		goto err_start;

	ret = sysfs_create_group(&df->dev.kobj, node->attr_grp);
	if (ret < 0)
		goto err_sysfs;

	mutex_lock(&df->lock);
	df->min_freq = df->max_freq;
	update_devfreq(df);
	mutex_unlock(&df->lock);

	return 0;

err_sysfs:
	stop_monitor(df);
err_start:
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
	return ret;
}

static int gov_suspend(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;
	unsigned long prev_freq = df->previous_freq;

	node->mon_started = false;
	if (!hw->should_ignore_df_monitor)
		devfreq_monitor_suspend(df);

	mutex_lock(&df->lock);
	update_devfreq(df);
	mutex_unlock(&df->lock);

	node->resume_freq = max(prev_freq, 1UL);

	return 0;
}

static int gov_resume(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;

	mutex_lock(&df->lock);
	update_devfreq(df);
	mutex_unlock(&df->lock);

	node->resume_freq = 0;

	if (!hw->should_ignore_df_monitor)
		devfreq_monitor_resume(df);
	node->mon_started = true;

	return 0;
}

static void gov_stop(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;

	sysfs_remove_group(&df->dev.kobj, node->attr_grp);
	stop_monitor(df);
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
}

static int devfreq_memlat_get_freq(struct devfreq *df,
					unsigned long *freq)
{
	int i, lat_dev = 0;
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;
	unsigned long max_freq = 0;
	unsigned int ratio;

	/*
	 * node->resume_freq is set to 0 at the end of resume (after the update)
	 * and is set to df->prev_freq at the end of suspend (after the update).
	 * This function will be called as part of the update_devfreq call in
	 * both scenarios. As a result, this block will cause a 0 vote during
	 * suspend and a vote for df->prev_freq during resume.
	 */
	if (!node->mon_started) {
		*freq = node->resume_freq;
		return 0;
	}

	hw->get_cnt(hw);

	for (i = 0; i < hw->num_cores; i++) {
		ratio = hw->core_stats[i].inst_count;

		if (hw->core_stats[i].mem_count)
			ratio /= hw->core_stats[i].mem_count;

		if (!hw->core_stats[i].freq)
			continue;

		trace_memlat_dev_meas(dev_name(df->dev.parent),
					hw->core_stats[i].id,
					hw->core_stats[i].inst_count,
					hw->core_stats[i].mem_count,
					hw->core_stats[i].freq,
					hw->core_stats[i].stall_pct,
					hw->core_stats[i].wb_pct, ratio);

		if (((ratio <= node->ratio_ceil
		      && hw->core_stats[i].stall_pct >= node->stall_floor) ||
		      (hw->core_stats[i].wb_pct >= node->wb_pct_thres
		      && ratio <= node->wb_filter_ratio))
		      && (hw->core_stats[i].freq > max_freq)) {
			lat_dev = i;
			max_freq = hw->core_stats[i].freq;
		}
	}

	if (max_freq)
		max_freq = core_to_dev_freq(node, max_freq);

	if (max_freq || !node->already_zero) {
		trace_memlat_dev_update(dev_name(df->dev.parent),
					hw->core_stats[lat_dev].id,
					hw->core_stats[lat_dev].inst_count,
					hw->core_stats[lat_dev].mem_count,
					hw->core_stats[lat_dev].freq,
					max_freq);
	}

	node->already_zero = !max_freq;

	*freq = max_freq;
	return 0;
}

show_attr(ratio_ceil);
store_attr(ratio_ceil, 1U, 50000U);
static DEVICE_ATTR_RW(ratio_ceil);
show_attr(stall_floor);
store_attr(stall_floor, 0U, 100U);
static DEVICE_ATTR_RW(stall_floor);
show_attr(wb_pct_thres);
store_attr(wb_pct_thres, 0U, 100U);
static DEVICE_ATTR_RW(wb_pct_thres);
show_attr(wb_filter_ratio);
store_attr(wb_filter_ratio, 0U, 50000U);
static DEVICE_ATTR_RW(wb_filter_ratio);

static struct attribute *memlat_dev_attr[] = {
	&dev_attr_ratio_ceil.attr,
	&dev_attr_stall_floor.attr,
	&dev_attr_freq_map.attr,
	&dev_attr_wb_pct_thres.attr,
	&dev_attr_wb_filter_ratio.attr,
	NULL,
};

static struct attribute *compute_dev_attr[] = {
	&dev_attr_freq_map.attr,
	NULL,
};

static struct attribute_group memlat_dev_attr_group = {
	.name = "mem_latency",
	.attrs = memlat_dev_attr,
};

static struct attribute_group compute_dev_attr_group = {
	.name = "compute",
	.attrs = compute_dev_attr,
};

#define MIN_MS	0U
#define MAX_MS	500U
static int devfreq_memlat_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret;
	unsigned int sample_ms;
	struct memlat_node *node;
	struct memlat_hwmon *hw;

	switch (event) {
	case DEVFREQ_GOV_START:
		sample_ms = df->profile->polling_ms;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		df->profile->polling_ms = sample_ms;

		ret = gov_start(df);
		if (ret < 0)
			return ret;

		dev_dbg(df->dev.parent,
			"Enabled Memory Latency governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		gov_stop(df);
		dev_dbg(df->dev.parent,
			"Disabled Memory Latency governor\n");
		break;

	case DEVFREQ_GOV_SUSPEND:
		ret = gov_suspend(df);
		if (ret < 0) {
			dev_err(df->dev.parent,
				"Unable to suspend memlat governor (%d)\n",
				ret);
			return ret;
		}

		dev_dbg(df->dev.parent, "Suspended memlat governor\n");
		break;

	case DEVFREQ_GOV_RESUME:
		ret = gov_resume(df);
		if (ret < 0) {
			dev_err(df->dev.parent,
				"Unable to resume memlat governor (%d)\n",
				ret);
			return ret;
		}

		dev_dbg(df->dev.parent, "Resumed memlat governor\n");
		break;

	case DEVFREQ_GOV_UPDATE_INTERVAL:
		node = df->data;
		hw = node->hw;
		sample_ms = *(unsigned int *)data;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		if (hw->request_update_ms)
			hw->request_update_ms(hw, sample_ms);
		if (!hw->should_ignore_df_monitor)
			devfreq_update_interval(df, &sample_ms);
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_gov_memlat = {
	.name = "mem_latency",
	.immutable = 1,
	.get_target_freq = devfreq_memlat_get_freq,
	.event_handler = devfreq_memlat_ev_handler,
};

static struct devfreq_governor devfreq_gov_compute = {
	.name = "compute",
	.immutable = 1,
	.get_target_freq = devfreq_memlat_get_freq,
	.event_handler = devfreq_memlat_ev_handler,
};

#define NUM_COLS	2
static struct core_dev_map *init_core_dev_map(struct device *dev,
					struct device_node *of_node,
					char *prop_name)
{
	int len, nf, i, j;
	u32 data;
	struct core_dev_map *tbl;
	int ret;

	if (!of_find_property(of_node, prop_name, &len))
		return NULL;
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct core_dev_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(of_node, prop_name, j,
				&data);
		if (ret < 0)
			return NULL;
		tbl[i].core_mhz = data / 1000;

		ret = of_property_read_u32_index(of_node, prop_name, j + 1,
				&data);
		if (ret < 0)
			return NULL;
		tbl[i].target_freq = data;
		pr_debug("Entry%d CPU:%u, Dev:%u\n", i, tbl[i].core_mhz,
				tbl[i].target_freq);
	}
	tbl[i].core_mhz = 0;

	return tbl;
}

static struct memlat_node *register_common(struct device *dev,
					   struct memlat_hwmon *hw)
{
	struct memlat_node *node;
	struct device_node *of_node = dev->of_node;

	if (!hw->dev && !hw->of_node)
		return ERR_PTR(-EINVAL);

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return ERR_PTR(-ENOMEM);

	node->ratio_ceil = 10;
	node->wb_pct_thres = 100;
	node->wb_filter_ratio = 25000;
	node->hw = hw;

	if (hw->get_child_of_node)
		of_node = hw->get_child_of_node(dev);

	if (of_parse_phandle(of_node, "qcom,core-dev-table", 0))
		of_node = of_parse_phandle(of_node, "qcom,core-dev-table", 0);

	hw->freq_map = init_core_dev_map(dev, of_node, "qcom,core-dev-table");

	if (!hw->freq_map) {
		dev_err(dev, "Couldn't find the core-dev freq table!\n");
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&list_lock);
	list_add_tail(&node->list, &memlat_list);
	mutex_unlock(&list_lock);

	return node;
}

int register_compute(struct device *dev, struct memlat_hwmon *hw)
{
	struct memlat_node *node;
	int ret = 0;

	node = register_common(dev, hw);
	if (IS_ERR(node)) {
		ret = PTR_ERR(node);
		goto out;
	}

	mutex_lock(&state_lock);
	node->gov = &devfreq_gov_compute;
	node->attr_grp = &compute_dev_attr_group;

	if (!compute_use_cnt)
		ret = devfreq_add_governor(&devfreq_gov_compute);
	if (!ret)
		compute_use_cnt++;
	mutex_unlock(&state_lock);

out:
	if (!ret)
		dev_dbg(dev, "Compute governor registered.\n");
	else
		dev_err(dev, "Compute governor registration failed!\n");

	return ret;
}
EXPORT_SYMBOL(register_compute);

int register_memlat(struct device *dev, struct memlat_hwmon *hw)
{
	struct memlat_node *node;
	int ret = 0;

	node = register_common(dev, hw);
	if (IS_ERR(node)) {
		ret = PTR_ERR(node);
		goto out;
	}

	mutex_lock(&state_lock);
	node->gov = &devfreq_gov_memlat;
	node->attr_grp = &memlat_dev_attr_group;

	if (!memlat_use_cnt)
		ret = devfreq_add_governor(&devfreq_gov_memlat);
	if (!ret)
		memlat_use_cnt++;
	mutex_unlock(&state_lock);

out:
	if (!ret)
		dev_dbg(dev, "Memory Latency governor registered.\n");
	else
		dev_err(dev, "Memory Latency governor registration failed!\n");

	return ret;
}
EXPORT_SYMBOL(register_memlat);

MODULE_DESCRIPTION("HW monitor based dev DDR bandwidth voting driver");
MODULE_LICENSE("GPL v2");

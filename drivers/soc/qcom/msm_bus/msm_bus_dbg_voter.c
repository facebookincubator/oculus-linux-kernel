/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include "msm_bus_adhoc.h"

struct msm_bus_floor_client_type {
	int mas_id;
	int slv_id;
	struct msm_bus_client_handle *vote_handle;
	struct device *dev;
	u64 cur_vote_hz;
	int active_only;
};

static struct class *bus_floor_class;
static DEFINE_RT_MUTEX(msm_bus_floor_vote_lock);
#define MAX_VOTER_NAME	(50)
#define DEFAULT_NODE_WIDTH	(8)
#define DBG_NAME(s)	(strnstr(s, "-", 7) + 1)

static int get_id(void)
{
	static int dev_id = MSM_BUS_INT_TEST_ID;
	int id = dev_id;

	if (id >= MSM_BUS_INT_TEST_LAST)
		id = -EINVAL;
	else
		dev_id++;

	return id;
}

static ssize_t bus_floor_active_only_show(struct device *dev,
			struct device_attribute *dev_attr, char *buf)
{
	struct msm_bus_floor_client_type *cl;

	cl = dev_get_drvdata(dev);

	if (!cl) {
		pr_err("%s: Can't find cl", __func__);
		return 0;
	}
	return snprintf(buf, sizeof(int), "%d", cl->active_only);
}

static ssize_t bus_floor_active_only_store(struct device *dev,
			struct device_attribute *dev_attr, const char *buf,
			size_t n)
{
	struct msm_bus_floor_client_type *cl;

	rt_mutex_lock(&msm_bus_floor_vote_lock);
	cl = dev_get_drvdata(dev);

	if (!cl) {
		pr_err("%s: Can't find cl", __func__);
		rt_mutex_unlock(&msm_bus_floor_vote_lock);
		return 0;
	}

	if (kstrtoint(buf, 10, &cl->active_only) != 0) {
		pr_err("%s:return error", __func__);
		rt_mutex_unlock(&msm_bus_floor_vote_lock);
		return -EINVAL;
	}

	rt_mutex_unlock(&msm_bus_floor_vote_lock);
	return n;
}

static ssize_t bus_floor_vote_show(struct device *dev,
			struct device_attribute *dev_attr, char *buf)
{
	struct msm_bus_floor_client_type *cl;

	cl = dev_get_drvdata(dev);

	if (!cl) {
		pr_err("%s: Can't find cl", __func__);
		return 0;
	}
	return snprintf(buf, sizeof(u64), "%llu", cl->cur_vote_hz);
}

static ssize_t bus_floor_vote_store(struct device *dev,
			struct device_attribute *dev_attr, const char *buf,
			size_t n)
{
	struct msm_bus_floor_client_type *cl;
	int ret = 0;

	rt_mutex_lock(&msm_bus_floor_vote_lock);
	cl = dev_get_drvdata(dev);

	if (!cl) {
		pr_err("%s: Can't find cl", __func__);
		rt_mutex_unlock(&msm_bus_floor_vote_lock);
		return 0;
	}

	if (kstrtoull(buf, 10, &cl->cur_vote_hz) != 0) {
		pr_err("%s:return error", __func__);
		rt_mutex_unlock(&msm_bus_floor_vote_lock);
		return -EINVAL;
	}

	ret = msm_bus_floor_vote_context(dev_name(dev), cl->cur_vote_hz,
					cl->active_only);
	rt_mutex_unlock(&msm_bus_floor_vote_lock);
	return n;
}

static ssize_t bus_floor_vote_store_api(struct device *dev,
			struct device_attribute *dev_attr, const char *buf,
			size_t n)
{
	struct msm_bus_floor_client_type *cl;
	int ret = 0;
	char name[10];
	u64 vote_khz = 0;

	rt_mutex_lock(&msm_bus_floor_vote_lock);
	cl = dev_get_drvdata(dev);

	if (!cl) {
		pr_err("%s: Can't find cl", __func__);
		rt_mutex_unlock(&msm_bus_floor_vote_lock);
		return 0;
	}

	if (sscanf(buf, "%9s %llu", name, &vote_khz) != 2) {
		pr_err("%s:return error", __func__);
		rt_mutex_unlock(&msm_bus_floor_vote_lock);
		return -EINVAL;
	}

	pr_info("%s: name %s vote %llu\n",
			__func__, name, vote_khz);

	ret = msm_bus_floor_vote(name, vote_khz);
	rt_mutex_unlock(&msm_bus_floor_vote_lock);
	return n;
}

static DEVICE_ATTR(floor_vote, S_IRUGO | S_IWUSR,
		bus_floor_vote_show, bus_floor_vote_store);

static DEVICE_ATTR(floor_vote_api, S_IRUGO | S_IWUSR,
		bus_floor_vote_show, bus_floor_vote_store_api);

static DEVICE_ATTR(floor_active_only, S_IRUGO | S_IWUSR,
		bus_floor_active_only_show, bus_floor_active_only_store);

static struct msm_bus_node_device_type *msm_bus_floor_init_dev(
				struct device *fab_dev, bool is_master)
{
	struct msm_bus_node_device_type *bus_node = NULL;
	struct msm_bus_node_device_type *fab_node = NULL;
	struct msm_bus_node_info_type *node_info = NULL;
	struct device *dev = NULL;
	int ret = 0;

	if (!fab_dev) {
		bus_node = ERR_PTR(-ENXIO);
		goto exit_init_bus_dev;
	}

	fab_node = to_msm_bus_node(fab_dev);

	if (!fab_node) {
		pr_info("\n%s: Can't create device", __func__);
		bus_node = ERR_PTR(-ENXIO);
		goto exit_init_bus_dev;
	}

	bus_node = kzalloc(sizeof(struct msm_bus_node_device_type), GFP_KERNEL);
	if (!bus_node) {
		bus_node = ERR_PTR(-ENOMEM);
		goto exit_init_bus_dev;
	}
	dev = &bus_node->dev;
	device_initialize(dev);

	node_info = devm_kzalloc(dev,
		sizeof(struct msm_bus_node_info_type), GFP_KERNEL);

	if (!node_info) {
		pr_err("%s:Bus node info alloc failed\n", __func__);
		devm_kfree(dev, bus_node);
		bus_node = ERR_PTR(-ENOMEM);
		goto exit_init_bus_dev;
	}

	bus_node->node_info = node_info;
	bus_node->ap_owned = true;
	bus_node->node_info->bus_device = fab_dev;
	bus_node->node_info->agg_params.buswidth = 8;
	dev->bus = &msm_bus_type;
	list_add_tail(&bus_node->dev_link, &fab_node->devlist);

	bus_node->node_info->id = get_id();
	if (bus_node->node_info->id < 0) {
		pr_err("%s: Failed to get id for dev. Bus:%s is_master:%d",
			__func__, fab_node->node_info->name, is_master);
		bus_node = ERR_PTR(-ENXIO);
		goto exit_init_bus_dev;
	}

	dev_set_name(dev, "testnode-%s-%s", (is_master ? "mas" : "slv"),
					fab_node->node_info->name);

	ret = device_add(dev);
	if (ret < 0) {
		pr_err("%s: Failed to add %s", __func__, dev_name(dev));
		bus_node = ERR_PTR(ret);
		goto exit_init_bus_dev;
	}

exit_init_bus_dev:
	return bus_node;
}

static int msm_bus_floor_show_info(struct device *dev, void *data)
{
	if (dev)
		pr_err(" %s\n", dev_name(dev));
	return 0;
}

static void msm_bus_floor_pr_usage(void)
{
	pr_err("msm_bus_floor_vote: Supported buses\n");
	class_for_each_device(bus_floor_class, NULL, NULL,
					msm_bus_floor_show_info);
}

static int msm_bus_floor_match(struct device *dev, const void *data)
{
	int ret = 0;

	if (!(dev && data))
		return ret;

	if (strnstr(dev_name(dev), data, MAX_VOTER_NAME))
		ret = 1;

	return ret;
}

int msm_bus_floor_vote(const char *name, u64 floor_hz)
{
	int ret = -EINVAL;
	struct msm_bus_floor_client_type *cl;
	bool found = false;
	struct device *dbg_voter = NULL;

	if (!name) {
		pr_err("%s: NULL name", __func__);
		return -EINVAL;
	}

	dbg_voter = class_find_device(bus_floor_class, NULL,
						name, msm_bus_floor_match);
	if (dbg_voter) {
		found = true;
		cl = dev_get_drvdata(dbg_voter);

		if (!cl) {
			pr_err("%s: Can't find cl", __func__);
			goto exit_bus_floor_vote;
		}

		if (!cl->vote_handle) {
			char cl_name[MAX_VOTER_NAME];

			snprintf(cl_name, MAX_VOTER_NAME, "%s-floor-voter",
						dev_name(cl->dev));
			cl->vote_handle = msm_bus_scale_register(cl->mas_id,
					cl->slv_id, cl_name, false);
			if (!cl->vote_handle) {
				ret = -ENXIO;
				goto exit_bus_floor_vote;
			}
		}

		cl->cur_vote_hz = floor_hz;
		ret = msm_bus_scale_update_bw(cl->vote_handle, 0,
					(floor_hz * DEFAULT_NODE_WIDTH));
		if (ret) {
			pr_err("%s: Failed to update %s", __func__,
								name);
			goto exit_bus_floor_vote;
		}
	} else {
		pr_err("\n%s:No matching voting device found for %s", __func__,
									name);
		msm_bus_floor_pr_usage();
	}

exit_bus_floor_vote:
	if (dbg_voter)
		put_device(dbg_voter);

	return ret;
}
EXPORT_SYMBOL(msm_bus_floor_vote);

int msm_bus_floor_vote_context(const char *name, u64 floor_hz,
						bool active_only)
{
	int ret = -EINVAL;
	struct msm_bus_floor_client_type *cl;
	bool found = false;
	struct device *dbg_voter = NULL;

	if (!name) {
		pr_err("%s: NULL name", __func__);
		return -EINVAL;
	}

	dbg_voter = class_find_device(bus_floor_class, NULL,
						name, msm_bus_floor_match);
	if (dbg_voter) {
		found = true;
		cl = dev_get_drvdata(dbg_voter);

		if (!cl) {
			pr_err("%s: Can't find cl", __func__);
			goto exit_bus_floor_vote_context;
		}

		if (!(cl->vote_handle &&
			(cl->vote_handle->active_only == active_only))) {
			char cl_name[MAX_VOTER_NAME];

			if (cl->vote_handle)
				msm_bus_scale_unregister(cl->vote_handle);

			snprintf(cl_name, MAX_VOTER_NAME, "%s-floor-voter",
						dev_name(cl->dev));
			cl->vote_handle = msm_bus_scale_register(cl->mas_id,
					cl->slv_id, (char *)dev_name(cl->dev),
								active_only);
			if (!cl->vote_handle) {
				ret = -ENXIO;
				goto exit_bus_floor_vote_context;
			}
		}

		cl->cur_vote_hz = floor_hz;
		ret = msm_bus_scale_update_bw(cl->vote_handle, 0,
					(floor_hz * DEFAULT_NODE_WIDTH));
		if (ret) {
			pr_err("%s: Failed to update %s", __func__,
								name);
			goto exit_bus_floor_vote_context;
		}
	} else {
		pr_err("\n%s:No matching voting device found for %s", __func__,
									name);
		msm_bus_floor_pr_usage();
	}

exit_bus_floor_vote_context:
	if (dbg_voter)
		put_device(dbg_voter);

	return ret;
}
EXPORT_SYMBOL(msm_bus_floor_vote_context);

static int msm_bus_floor_setup_dev_conn(
		struct msm_bus_node_device_type *mas_node,
		struct msm_bus_node_device_type *slv_node)
{
	int ret = 0;
	int slv_id = 0;

	if (!(mas_node && slv_node)) {
		pr_err("\n%s: Invalid master/slave device", __func__);
		ret = -ENXIO;
		goto exit_setup_dev_conn;
	}

	slv_id = slv_node->node_info->id;
	mas_node->node_info->num_connections = 1;
	mas_node->node_info->connections = devm_kzalloc(&mas_node->dev,
			(sizeof(int) * mas_node->node_info->num_connections),
			GFP_KERNEL);

	if (!mas_node->node_info->connections) {
		pr_err("%s:Bus node connections info alloc failed\n", __func__);
		ret = -ENOMEM;
		goto exit_setup_dev_conn;
	}

	mas_node->node_info->dev_connections = devm_kzalloc(&mas_node->dev,
			(sizeof(struct device *) *
				mas_node->node_info->num_connections),
			GFP_KERNEL);

	if (!mas_node->node_info->dev_connections) {
		pr_err("%s:Bus node dev connections info alloc failed\n",
								__func__);
		ret = -ENOMEM;
		goto exit_setup_dev_conn;
	}
	mas_node->node_info->connections[0] = slv_id;
	mas_node->node_info->dev_connections[0] = &slv_node->dev;

exit_setup_dev_conn:
	return ret;
}

static int msm_bus_floor_setup_floor_dev(
			struct msm_bus_node_device_type *mas_node,
			struct msm_bus_node_device_type *slv_node,
			struct msm_bus_node_device_type *bus_node)
{
	struct msm_bus_floor_client_type *cl_ptr = NULL;
	int ret = 0;
	char *name = NULL;

	cl_ptr = kzalloc(sizeof(struct msm_bus_floor_client_type), GFP_KERNEL);
	if (!cl_ptr) {
		ret = -ENOMEM;
		goto err_setup_floor_dev;
	}

	if (!bus_floor_class) {
		bus_floor_class = class_create(THIS_MODULE, "bus-voter");
		if (IS_ERR(bus_floor_class)) {
			ret = -ENXIO;
			pr_err("%s: Error creating dev class", __func__);
			goto err_setup_floor_dev;
		}
	}

	name = DBG_NAME(bus_node->node_info->name);
	if (!name) {
		pr_err("%s: Invalid name derived for %s", __func__,
						bus_node->node_info->name);
		ret = -EINVAL;
		goto err_setup_floor_dev;
	}

	cl_ptr->dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!cl_ptr->dev) {
		ret = -ENOMEM;
		pr_err("%s: Failed to create device bus %d", __func__,
			bus_node->node_info->id);
		goto err_setup_floor_dev;
	}

	device_initialize(cl_ptr->dev);
	cl_ptr->dev->class = bus_floor_class;
	dev_set_name(cl_ptr->dev, "%s", name);
	dev_set_drvdata(cl_ptr->dev, cl_ptr);
	ret = device_add(cl_ptr->dev);

	if (ret < 0) {
		pr_err("%s: Failed to add device bus %d", __func__,
			bus_node->node_info->id);
		goto err_setup_floor_dev;
	}

	cl_ptr->mas_id = mas_node->node_info->id;
	cl_ptr->slv_id = slv_node->node_info->id;

	ret = device_create_file(cl_ptr->dev, &dev_attr_floor_vote);
	if (ret < 0)
		goto err_setup_floor_dev;

	ret = device_create_file(cl_ptr->dev, &dev_attr_floor_vote_api);
	if (ret < 0)
		goto err_setup_floor_dev;

	ret = device_create_file(cl_ptr->dev, &dev_attr_floor_active_only);
	if (ret < 0)
		goto err_setup_floor_dev;

	return ret;

err_setup_floor_dev:
	kfree(cl_ptr);
	return ret;
}

int msm_bus_floor_init(struct device *dev)
{
	struct msm_bus_node_device_type *mas_node = NULL;
	struct msm_bus_node_device_type *slv_node = NULL;
	struct msm_bus_node_device_type *bus_node = NULL;
	int ret = 0;

	if (!dev) {
		pr_info("\n%s: Can't create voting client", __func__);
		ret = -ENXIO;
		goto exit_floor_init;
	}

	bus_node = to_msm_bus_node(dev);
	if (!(bus_node && bus_node->node_info->is_fab_dev)) {
		pr_info("\n%s: Can't create voting client, not a fab device",
								__func__);
		ret = -ENXIO;
		goto exit_floor_init;
	}

	mas_node = msm_bus_floor_init_dev(dev, true);
	if (IS_ERR_OR_NULL(mas_node)) {
		pr_err("\n%s: Error setting up master dev, bus %d",
					__func__, bus_node->node_info->id);
		goto exit_floor_init;
	}

	slv_node = msm_bus_floor_init_dev(dev, false);
	if (IS_ERR_OR_NULL(slv_node)) {
		pr_err("\n%s: Error setting up slave dev, bus %d",
					__func__, bus_node->node_info->id);
		goto exit_floor_init;
	}

	ret = msm_bus_floor_setup_dev_conn(mas_node, slv_node);
	if (ret) {
		pr_err("\n%s: Error setting up connections bus %d",
					__func__, bus_node->node_info->id);
		goto err_floor_init;
	}

	ret = msm_bus_floor_setup_floor_dev(mas_node, slv_node, bus_node);
	if (ret) {
		pr_err("\n%s: Error getting mas/slv nodes bus %d",
					__func__, bus_node->node_info->id);
		goto err_floor_init;
	}

exit_floor_init:
	return ret;
err_floor_init:
	kfree(mas_node);
	kfree(slv_node);
	return ret;
}

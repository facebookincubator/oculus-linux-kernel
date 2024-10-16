// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2022, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pmic-voter.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/types.h>

#define CHARGE_PORT_THERMAL_VOTER "CHARGE_PORT_THERM_VOTER"

#define CDEV_NORMAL_STATE           0
#define CDEV_REDUCE_CHARGING_STATE  1
#define CDEV_SUSPEND_CHARGING_STATE 2

struct ctx {
	struct mutex lock;
	struct thermal_cooling_device *cdev;
	struct votable *votable;
	int suspend_vote;
	int reduce_vote;
	bool suspend_vote_enabled;
	bool reduce_vote_enabled;
	unsigned long state;
	unsigned long max_state;
};

struct charging_thermal_controller_device_t {
	struct device *dev;
	struct ctx *ctx;
};

static int cdev_get_max_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	struct ctx *ctx = cdev->devdata;

	if (!ctx)
		return -EINVAL;

	*state = ctx->max_state;

	return 0;
}

static int cdev_get_cur_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	struct ctx *ctx = cdev->devdata;

	if (!ctx)
		return -EINVAL;

	mutex_lock(&ctx->lock);
	*state = ctx->state;
	mutex_unlock(&ctx->lock);

	return 0;
}

static int cdev_set_cur_state(struct thermal_cooling_device *cdev,
		unsigned long state)
{
	struct ctx *ctx = cdev->devdata;
	int ret = 0;

	if (!ctx || (state > ctx->max_state))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ctx->state = state;

	switch (ctx->state) {
	case CDEV_NORMAL_STATE:
		ret = vote(ctx->votable, CHARGE_PORT_THERMAL_VOTER, false, 0);
		break;
	case CDEV_REDUCE_CHARGING_STATE:
		if (ctx->reduce_vote_enabled) {
			ret = vote(ctx->votable, CHARGE_PORT_THERMAL_VOTER, true, ctx->reduce_vote);
			dev_dbg(&cdev->device, "reducing charging\n");
		}
		break;
	case CDEV_SUSPEND_CHARGING_STATE:
		if (ctx->suspend_vote_enabled) {
			ret = vote(ctx->votable, CHARGE_PORT_THERMAL_VOTER, true, ctx->suspend_vote);
			dev_dbg(&cdev->device, "suspending charging\n");
		}
		break;
	default:
		dev_err(&cdev->device, "invalid cooling state=%lu\n", ctx->state);
		ret = -EINVAL;
	}

	mutex_unlock(&ctx->lock);

	return ret;
}

static const struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = cdev_get_max_state,
	.get_cur_state = cdev_get_cur_state,
	.set_cur_state = cdev_set_cur_state,
};

static ssize_t state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_thermal_controller_device_t *ddev =
		(struct charging_thermal_controller_device_t *) dev_get_drvdata(dev);
	int result;
	unsigned long state;

	if (!ddev->ctx)
		return 0;

	result = cdev_get_cur_state(ddev->ctx->cdev, &state);
	if (result < 0)
		return result;

	result = scnprintf(buf, PAGE_SIZE, "%lu\n", state);

	return result;
}
static DEVICE_ATTR_RO(state);

static struct attribute *charging_thermal_controller_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(charging_thermal_controller);

static int register_cdev(struct charging_thermal_controller_device_t *ddev,
		const char *cdev_name)
{
	char cdev_full_name[THERMAL_NAME_LENGTH] = "";

	snprintf(cdev_full_name, THERMAL_NAME_LENGTH, "%s",
			cdev_name);

	if (!IS_ENABLED(CONFIG_THERMAL)) {
		dev_err(ddev->dev, "CONFIG_THERMAL not enabled\n");
		return -EINVAL;
	}

	mutex_init(&ddev->ctx->lock);
	ddev->ctx->max_state = CDEV_SUSPEND_CHARGING_STATE;

	ddev->ctx->cdev = thermal_of_cooling_device_register(ddev->dev->of_node,
							cdev_full_name, ddev->ctx,
							&cooling_ops);

	if (IS_ERR(ddev->ctx->cdev)) {
		dev_err(ddev->dev,
			"Failed to register cdev0 as cooling device\n");
		return PTR_ERR(ddev->ctx->cdev);
	}

	thermal_cdev_update(ddev->ctx->cdev);

	return 0;
}

static int charging_thermal_controller_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct charging_thermal_controller_device_t *ddev;
	const char *votable_name;
	const char *cdev_name;

	dev_dbg(&pdev->dev, "probing\n");

	ddev = devm_kzalloc(&pdev->dev, sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	ddev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, ddev);

	ret = of_property_read_string(pdev->dev.of_node,
					 "cdev-name",
					 &cdev_name);

	if (ret < 0) {
		dev_err(ddev->dev, "Unable to find cdev-name property, ret = %d\n", ret);
		return ret;
	}

	ret = of_property_read_string(pdev->dev.of_node,
					 "votable",
					 &votable_name);

	if (ret < 0) {
		dev_err(ddev->dev, "Unable to find votable property, ret = %d\n", ret);
		return ret;
	}

	ddev->ctx = devm_kzalloc(ddev->dev, sizeof(struct ctx), GFP_KERNEL);
	if (!ddev->ctx)
		return -ENOMEM;

	ddev->ctx->votable = find_votable(votable_name);
	if (!ddev->ctx->votable) {
		dev_err(ddev->dev, "Error finding %s votable, ret = %d\n", votable_name, ret);
		return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"suspend-vote", &ddev->ctx->suspend_vote);
	if (ret == 0)
		ddev->ctx->suspend_vote_enabled = true;

	ret = of_property_read_u32(pdev->dev.of_node,
		"reduce-vote", &ddev->ctx->reduce_vote);
	if (ret == 0)
		ddev->ctx->reduce_vote_enabled = true;

	if (!ddev->ctx->reduce_vote_enabled && !ddev->ctx->suspend_vote_enabled) {
		dev_err(ddev->dev, "Neither suspend or reduce votes are enabled.\n");
		return -EINVAL;
	}
	dev_dbg(ddev->dev, "reduce_vote_enabled=%d reduce_vote=%d suspend_vote_enabled=%d suspend_vote=%d\n",
			ddev->ctx->reduce_vote_enabled,
			ddev->ctx->reduce_vote,
			ddev->ctx->suspend_vote_enabled,
			ddev->ctx->suspend_vote);

	ret = register_cdev(ddev, cdev_name);
	if (ret < 0)
		return ret;

	ret = sysfs_create_groups(&ddev->dev->kobj, charging_thermal_controller_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files, ret = %d\n", ret);

	return 0;
}

static int charging_thermal_controller_remove(struct platform_device *pdev)
{
	struct charging_thermal_controller_device_t *ddev = platform_get_drvdata(pdev);

	sysfs_remove_groups(&ddev->dev->kobj, charging_thermal_controller_groups);
	thermal_cooling_device_unregister(ddev->ctx->cdev);
	mutex_destroy(&ddev->ctx->lock);

	return 0;
}

static const struct of_device_id charging_thermal_controller_table[] = {
	{ .compatible = "oculus,charging-thermal-controller" },
	{}
};

static struct platform_driver charging_thermal_controller_driver = {
	.probe = charging_thermal_controller_probe,
	.remove = charging_thermal_controller_remove,
	.driver = {
		.name = "oculus,charging-thermal-controller",
		.of_match_table = charging_thermal_controller_table,
		.owner = THIS_MODULE,
	},
};

static int __init charging_thermal_controller_init(void)
{
	return platform_driver_register(&charging_thermal_controller_driver);
}
late_initcall(charging_thermal_controller_init);

static void __exit charging_thermal_controller_deinit(void)
{
	platform_driver_unregister(&charging_thermal_controller_driver);
}
module_exit(charging_thermal_controller_deinit);

MODULE_ALIAS("charging_thermal_controller");
MODULE_LICENSE("GPL v2");

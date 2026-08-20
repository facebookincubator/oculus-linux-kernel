// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * MAX17332 Thermal Cooling Device Driver
 *
 * Registers the MAX17332 charger as a thermal cooling device, allowing
 * the thermal framework to disable charging when the device temperature
 * exceeds configured thresholds.
 *
 * Cooling states:
 *   0 - charging enabled  (no thermal throttling)
 *   1 - charging disabled (thermal throttling active)
 */

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/of.h>

#include "max17332.h"

#define MAX17332_CDEV_MAX_STATE 1

struct max17332_cdev {
	uint32_t cur_state;
	uint32_t max_state;
	struct thermal_cooling_device *cdev;
	struct device *dev;
	struct max17332_dev *max17332;
};

/**
 * max17332_cdev_set_cur_state - callback to set the cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: the cooling state to apply.
 *
 * State 0 enables charging, state 1 disables charging via the
 * CHGOff bit in the CommStat register.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int max17332_cdev_set_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long state)
{
	struct max17332_cdev *mcdev = cdev->devdata;
	u16 val;
	int ret;

	dev_info(mcdev->dev, "Setting state to %lu", state);

	if (state > mcdev->max_state)
		return -EINVAL;

	if (mcdev->cur_state == state)
		return 0;

	ret = max17332_read(mcdev->max17332->regmap_pmic, REG_COMMSTAT, &val);
	if (ret < 0) {
		dev_err(mcdev->dev, "Failed to read CommStat: %d\n", ret);
		return ret;
	}

	val &= ~MAX17332_COMMSTAT_CHGOFF;
	if (state > 0)
		val |= MAX17332_COMMSTAT_CHGOFF;

	ret = max17332_write_unlock(mcdev->max17332,
				    mcdev->max17332->regmap_pmic,
				    REG_COMMSTAT, val);
	if (ret < 0) {
		dev_err(mcdev->dev, "Failed to write CommStat: %d\n", ret);
		return ret;
	}

	mcdev->cur_state = state;

	return 0;
}

/**
 * max17332_cdev_get_cur_state - callback to get the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int max17332_cdev_get_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long *state)
{
	struct max17332_cdev *mcdev = cdev->devdata;

	*state = mcdev->cur_state;

	return 0;
}

/**
 * max17332_cdev_get_max_state - callback to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int max17332_cdev_get_max_state(struct thermal_cooling_device *cdev,
				       unsigned long *state)
{
	struct max17332_cdev *mcdev = cdev->devdata;

	*state = mcdev->max_state;

	return 0;
}

static struct thermal_cooling_device_ops max17332_cdev_ops = {
	.get_max_state = max17332_cdev_get_max_state,
	.get_cur_state = max17332_cdev_get_cur_state,
	.set_cur_state = max17332_cdev_set_cur_state,
};

static int max17332_cdev_probe(struct platform_device *pdev)
{
	struct max17332_dev *max17332 = dev_get_drvdata(pdev->dev.parent);
	struct max17332_cdev *mcdev;
	struct device *dev = &pdev->dev;
	u16 val;
	int ret;

	mcdev = devm_kzalloc(dev, sizeof(*mcdev), GFP_KERNEL);
	if (!mcdev)
		return -ENOMEM;

	mcdev->dev = dev;
	mcdev->max17332 = max17332;
	mcdev->max_state = MAX17332_CDEV_MAX_STATE;

	ret = max17332_read(mcdev->max17332->regmap_pmic, REG_COMMSTAT, &val);
	if (ret < 0) {
		dev_err(dev, "Failed to read CommStat: %d\n", ret);
		return ret;
	}
	mcdev->cur_state = (val & MAX17332_COMMSTAT_CHGOFF) ? 1 : 0;

	mcdev->cdev = thermal_of_cooling_device_register(
				max17332->dev->of_node,
				MAX17332_CDEV_NAME, mcdev,
				&max17332_cdev_ops);
	if (IS_ERR(mcdev->cdev)) {
		ret = PTR_ERR(mcdev->cdev);
		dev_err(dev, "Failed to register cooling device: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, mcdev);
	dev_info(dev, "Cooling device [%s] registered.\n", MAX17332_CDEV_NAME);

	return 0;
}

static int max17332_cdev_remove(struct platform_device *pdev)
{
	struct max17332_cdev *mcdev = platform_get_drvdata(pdev);

	if (mcdev->cdev) {
		thermal_cooling_device_unregister(mcdev->cdev);
		mcdev->cdev = NULL;
	}

	return 0;
}

static const struct platform_device_id max17332_cdev_id[] = {
	{ MAX17332_CDEV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17332_cdev_id);

static struct platform_driver max17332_cdev_driver = {
	.driver = {
		.name = MAX17332_CDEV_NAME,
	},
	.probe = max17332_cdev_probe,
	.remove = max17332_cdev_remove,
	.id_table = max17332_cdev_id,
};
module_platform_driver(max17332_cdev_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MAX17332 Thermal Cooling Device");

// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>

static bool st60_disable = false;
static struct gpio_desc *st60_power_supply_gpiod = NULL;
static DEFINE_MUTEX(st60_lock);

static int st60_disable_param_set(const char *val, const struct kernel_param *kp)
{
	bool disable;
	int ret;

	ret = kstrtobool(val, &disable);
	if (ret < 0)
		return ret;

	mutex_lock(&st60_lock);
	st60_disable = disable;

	// Only set the GPIO after the probing function initialize GPIO
	if (st60_power_supply_gpiod) {
		gpiod_set_value(st60_power_supply_gpiod, disable ? 1 : 0);
	}

	mutex_unlock(&st60_lock);

	return ret;
}

static int st60_disable_param_get(char *buffer, const struct kernel_param *kp)
{
	bool disable;

	mutex_lock(&st60_lock);
	disable = st60_disable;
	mutex_unlock(&st60_lock);

	return sprintf(buffer, "%c\n", disable ? 'Y' : 'N');
}

static const struct kernel_param_ops st60_disable_param_ops = {
	.set = st60_disable_param_set,
	.get = st60_disable_param_get,
};

module_param_cb(st60_disable, &st60_disable_param_ops, &st60_disable, 0644);
MODULE_PARM_DESC(st60_disable, "boolean to configure whether shutoff power supply to ST60");

static int st60_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	mutex_lock(&st60_lock);

	st60_power_supply_gpiod = devm_gpiod_get(dev, "power-supply-ctrl", GPIOD_OUT_LOW);
	if (IS_ERR(st60_power_supply_gpiod)) {
		ret = PTR_ERR(st60_power_supply_gpiod);
		st60_power_supply_gpiod = NULL;
		mutex_unlock(&st60_lock);
		dev_err(dev, "Failed to get power supply GPIO: %d\n", ret);
		return ret;
	}

	if (st60_disable) {
		gpiod_set_value(st60_power_supply_gpiod, 1);
	}

	mutex_unlock(&st60_lock);

	return 0;
}

static int st60_ctrl_remove(struct platform_device *pdev)
{
	mutex_lock(&st60_lock);
	st60_power_supply_gpiod = NULL;
	mutex_unlock(&st60_lock);

	return 0;
}

static const struct of_device_id st60_ctrl_dt_match[] = {
	{ .compatible = "meta,st60-ctrl", },
	{ }
};
MODULE_DEVICE_TABLE(of, st60_ctrl_dt_match);

static struct platform_driver st60_ctrl_driver = {
	.probe		= st60_ctrl_probe,
	.remove		= st60_ctrl_remove,
	.driver		= {
		.name	= "st60-ctrl",
		.of_match_table = st60_ctrl_dt_match,
	},
};

module_platform_driver(st60_ctrl_driver);

MODULE_DESCRIPTION("ST60 Wireless Tunneling Chipset Driver");
MODULE_LICENSE("GPL v2");

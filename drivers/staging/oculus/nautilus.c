// SPDX-License-Identifier: GPL-2.0

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

struct nautilus_dev_data {
	struct device *dev;
	struct pinctrl_state *pins_clock;
	struct regulator *vdd_supply;
	struct clk *pclk;
	bool enabled;
	bool enable_on_resume;
	struct mutex state_lock;
};

static int nautilus_enable_locked(struct nautilus_dev_data *devdata, bool enable)
{
	int ret;

	if (enable == devdata->enabled)
		return 0;

	if (enable) {
		ret = regulator_enable(devdata->vdd_supply);
		if (ret) {
			dev_err(devdata->dev, "Failed to enable regulator");
			return ret;
		}

		ret = clk_prepare_enable(devdata->pclk);
		if (ret) {
			dev_err(devdata->dev, "Failed to enable clk");
			regulator_disable(devdata->vdd_supply);
			return ret;
		}
	} else {
		clk_disable_unprepare(devdata->pclk);
		regulator_disable(devdata->vdd_supply);
	}

	devdata->enabled = enable;

	return 0;
}

static ssize_t enable_store(struct device *dev,  struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret;
	bool enable = false;
	struct nautilus_dev_data *devdata =
		(struct nautilus_dev_data *)dev_get_drvdata(dev);

	ret = strtobool(buf, &enable);
	if (ret < 0)
		return ret;

	mutex_lock(&devdata->state_lock);
	ret = nautilus_enable_locked(devdata, enable);
	mutex_unlock(&devdata->state_lock);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(enable, 0644, NULL, enable_store);

static struct attribute *nautilus_attr[] = {
	&dev_attr_enable.attr,
	NULL
};

static const struct attribute_group nautilus_gr = {
	.name = "nautilus",
	.attrs = nautilus_attr
};

static int nautilus_probe(struct platform_device *pdev)
{
	struct nautilus_dev_data *devdata;
	struct device *dev = &pdev->dev;
	int result = 0;

	devdata = devm_kzalloc(dev, sizeof(struct nautilus_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	devdata->dev = dev;
	mutex_init(&devdata->state_lock);

	devdata->vdd_supply = devm_regulator_get(dev, "vdd");
	if (IS_ERR_OR_NULL(devdata->vdd_supply)) {
		result = PTR_ERR(devdata->vdd_supply);
		dev_err(dev, "Failed to get vdd %d", result);
		return result;
	}

	result = pinctrl_pm_select_default_state(dev);
	if (result) {
		dev_err(dev, "Failed to set pinctrl state");
		return result;
	}

	devdata->pclk = devm_clk_get(dev, "nautilus-clock");
	if (IS_ERR(devdata->pclk)) {
		result = PTR_ERR(devdata->pclk);
		dev_err(dev, "Failed to get clk %d", result);
		return result;
	}

	result = sysfs_create_group(&dev->kobj, &nautilus_gr);
	if (result) {
		dev_err(dev, "device_create_file failed\n");
		return result;
	}

	return result;
}

static int nautilus_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_group(&dev->kobj, &nautilus_gr);

	dev_dbg(&pdev->dev, "nautilus remove\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nautilus_suspend(struct device *dev)
{
	int ret = 0;
	struct nautilus_dev_data *devdata =
		(struct nautilus_dev_data *)dev_get_drvdata(dev);

	mutex_lock(&devdata->state_lock);
	if (devdata->enabled) {
		ret = nautilus_enable_locked(devdata, false);
		devdata->enable_on_resume = true;
	} else {
		devdata->enable_on_resume = false;
	}
	mutex_unlock(&devdata->state_lock);

	return ret;
}

static int nautilus_resume(struct device *dev)
{
	int ret = 0;
	struct nautilus_dev_data *devdata =
		(struct nautilus_dev_data *)dev_get_drvdata(dev);

	mutex_lock(&devdata->state_lock);
	if (devdata->enable_on_resume)
		ret = nautilus_enable_locked(devdata, true);
	mutex_unlock(&devdata->state_lock);

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(nautilus_pm_ops, nautilus_suspend, nautilus_resume);

static const struct of_device_id nautilus_of_match[] = {
	{ .compatible = "oculus,nautilus", },
	{}
};

MODULE_DEVICE_TABLE(of, nautilus_of_match);

static struct platform_driver nautilus_driver = {
	.driver = {
		.name = "oculus,nautilus",
		.owner = THIS_MODULE,
		.of_match_table = nautilus_of_match,
		.pm = &nautilus_pm_ops,
	},
	.probe	= nautilus_probe,
	.remove = nautilus_remove,
};

static int __init nautilus_init(void)
{
	return platform_driver_register(&nautilus_driver);
}

static void __exit nautilus_exit(void)
{
	platform_driver_unregister(&nautilus_driver);
}

module_init(nautilus_init);
module_exit(nautilus_exit);

MODULE_LICENSE("GPL v2");

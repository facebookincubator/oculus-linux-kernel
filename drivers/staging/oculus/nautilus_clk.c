#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

struct nautilus_dev_data {
	struct pinctrl_state *pins_clock;
	struct clk *pclk;
	bool clk_enabled;
};

static ssize_t nautilus_clk_enable(struct device *dev,
		  struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	bool clk_enable = false;
	struct nautilus_dev_data *devdata =
		(struct nautilus_dev_data *)dev_get_drvdata(dev);

	ret = strtobool(buf, &clk_enable);
	if (ret < 0)
		return ret;
	if (clk_enable) {
		ret = clk_prepare_enable(devdata->pclk);
		if (ret) {
			dev_err(dev, "Failed to enable clk");
			return ret;
		}
	} else {
		clk_disable_unprepare(devdata->pclk);
	}
	devdata->clk_enabled = clk_enable;
	return count;
}

static DEVICE_ATTR(clk_enable, S_IRUGO | S_IWUSR,
	NULL, nautilus_clk_enable);

static struct attribute *nautilus_clk_attr[] = {
	&dev_attr_clk_enable.attr,
	NULL
};

static const struct attribute_group nautilus_clk_gr = {
	.name = "nautilus_clk",
	.attrs = nautilus_clk_attr
};

static int nautilus_clk_probe(struct platform_device *pdev)
{
	struct nautilus_dev_data *devdata;
	struct pinctrl *pinctrl = NULL;
	int result = 0;

	devdata = kzalloc(sizeof(struct nautilus_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, devdata);

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(&pdev->dev, "Failed to get pin ctrl");
		return PTR_ERR(pinctrl);
	}

	devdata->pins_clock = pinctrl_lookup_state(pinctrl, "nautilus-mclk");
	if (IS_ERR_OR_NULL(devdata->pins_clock)) {
		dev_err(&pdev->dev, "Failed to lookup pinctrl nautilus mclk");
		return PTR_ERR(devdata->pins_clock);
	}

	pinctrl_select_state(pinctrl, devdata->pins_clock);
	devdata->pclk = devm_clk_get(&pdev->dev, "nautilus-clock");
	if (IS_ERR_OR_NULL(devdata->pclk)) {
		dev_err(&pdev->dev, "Failed to get clk");
		return PTR_ERR(devdata->pclk);
	}

	result = sysfs_create_group(&pdev->dev.kobj, &nautilus_clk_gr);
	if (result) {
		dev_err(&pdev->dev, "device_create_file failed\n");
		return result;
	}

	return result;
}

static int nautilus_clk_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_group(&dev->kobj, &nautilus_clk_gr);

	dev_dbg(&pdev->dev, "nautilus remove\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nautilus_clk_suspend(struct device *dev)
{
	struct nautilus_dev_data *devdata =
		(struct nautilus_dev_data *)dev_get_drvdata(dev);

	if (devdata->clk_enabled)
		clk_disable_unprepare(devdata->pclk);

	return 0;
}

static int nautilus_clk_resume(struct device *dev)
{
	int ret = 0;
	struct nautilus_dev_data *devdata =
		(struct nautilus_dev_data *)dev_get_drvdata(dev);

	if (devdata->clk_enabled) {
		ret = clk_prepare_enable(devdata->pclk);
		if (ret)
			dev_err(dev, "Failed to enable clk");
	}

	return ret;
}
#else
#define nautilus_clk_suspend NULL
#define nautilus_clk_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(nautilus_clk_pm_ops, nautilus_clk_suspend, nautilus_clk_resume);

static const struct of_device_id nautilus_clk_of_match[] = {
	{ .compatible = "oculus,nautilus_clk", },
	{}
};

MODULE_DEVICE_TABLE(of, nautilus_clk_of_match);

static struct platform_driver nautilus_clk_driver = {
	.driver = {
		.name = "oculus,nautilus_clk",
		.owner = THIS_MODULE,
		.of_match_table = nautilus_clk_of_match,
		.pm = &nautilus_clk_pm_ops,
	},
	.probe	= nautilus_clk_probe,
	.remove = nautilus_clk_remove,
};

static int __init nautilus_clk_init(void)
{
	return platform_driver_register(&nautilus_clk_driver);
}

static void __exit nautilus_clk_exit(void)
{
	platform_driver_unregister(&nautilus_clk_driver);
}

module_init(nautilus_clk_init);
module_exit(nautilus_clk_exit);

MODULE_AUTHOR("zack wang <zack.wang@goertek.com>");
MODULE_DESCRIPTION("nautilus clock control driver");
MODULE_LICENSE("GPL v2");

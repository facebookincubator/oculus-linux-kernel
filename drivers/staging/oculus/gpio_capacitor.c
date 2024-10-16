// SPDX-License-Identifier: GPL-2.0

#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#define REGULATOR_ENABLE_DELAY_MS_DEFAULT 30

static const struct of_device_id gpio_capacitor_table[] = {
	{ .compatible = "gpio-capacitor" },
	{ },
};

struct gpio_capacitor_ctrl {
	struct device *dev;
	struct regulator *supply;
	struct delayed_work dwork;
	uint32_t enable_delay_ms;
};

static void regulator_enable_func(struct work_struct *work)
{
	int rc = 0;
	struct gpio_capacitor_ctrl *gpio_capacitor_ctrl =
		container_of(work, struct gpio_capacitor_ctrl, dwork.work);

	rc = regulator_enable(gpio_capacitor_ctrl->supply);
	if (rc < 0) {
		dev_err(gpio_capacitor_ctrl->dev,
				"Failed to enable gpio_capacitor supply: %d", rc);
	}
}

static int gpio_capacitor_resume(struct device *dev)
{
	struct gpio_capacitor_ctrl *gpio_capacitor_ctrl = dev_get_drvdata(dev);

	schedule_delayed_work(
			&gpio_capacitor_ctrl->dwork, msecs_to_jiffies(gpio_capacitor_ctrl->enable_delay_ms));

	return 0;
}

static int gpio_capacitor_suspend(struct device *dev)
{
	int rc = 0;
	struct gpio_capacitor_ctrl *gpio_capacitor_ctrl = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&gpio_capacitor_ctrl->dwork);

	rc = regulator_disable(gpio_capacitor_ctrl->supply);
	if (rc < 0) {
		dev_err(gpio_capacitor_ctrl->dev,
				"Failed to disable gpio_capacitor supply: %d", rc);
	}

	return rc;
}

static int gpio_capacitor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_capacitor_ctrl *gpio_capacitor_ctrl = NULL;
	int rc = 0;

	gpio_capacitor_ctrl = devm_kzalloc(dev, sizeof(*gpio_capacitor_ctrl), GFP_KERNEL);
	if (gpio_capacitor_ctrl == NULL)
		return -ENOMEM;

	gpio_capacitor_ctrl->dev = dev;

	gpio_capacitor_ctrl->supply = devm_regulator_get(dev, "gpio-capacitor");
	if (IS_ERR(gpio_capacitor_ctrl)) {
		rc = PTR_ERR(gpio_capacitor_ctrl);
		dev_err(dev, "Failed to get gpio-capacitor supply: %d", rc);
		return rc;
	}

	/* Delay to avoid the capacitor gets charged/discharged in a short time */
	/* while entering suspend mode */
	rc = of_property_read_u32(pdev->dev.of_node,
			"oculus,regulator-enable-delay-ms",
			&gpio_capacitor_ctrl->enable_delay_ms);
	if (rc < 0) {
		gpio_capacitor_ctrl->enable_delay_ms = REGULATOR_ENABLE_DELAY_MS_DEFAULT;
		rc = 0;
	}

	dev_set_drvdata(dev, gpio_capacitor_ctrl);

	INIT_DELAYED_WORK(&gpio_capacitor_ctrl->dwork, regulator_enable_func);

	gpio_capacitor_resume(dev);

	return rc;
}

static int gpio_capacitor_remove(struct platform_device *pdev)
{
	gpio_capacitor_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops gpio_capacitor_pm_ops = {
	.suspend = gpio_capacitor_suspend,
	.resume  = gpio_capacitor_resume,
};

struct platform_driver gpio_capacitor_driver = {
	.driver = {
		.name = "gpio_capacitor",
		.owner = THIS_MODULE,
		.of_match_table = gpio_capacitor_table,
		.pm = &gpio_capacitor_pm_ops,
	},
	.probe	= gpio_capacitor_probe,
	.remove = gpio_capacitor_remove
};

static int __init gpio_capacitor_init(void)
{
	return platform_driver_register(&gpio_capacitor_driver);
}

static void __exit gpio_capacitor_exit(void)
{
	platform_driver_unregister(&gpio_capacitor_driver);
}

module_init(gpio_capacitor_init);
module_exit(gpio_capacitor_exit);
MODULE_DESCRIPTION("Capacitor Controlled by GPIO");
MODULE_LICENSE("GPL v2");

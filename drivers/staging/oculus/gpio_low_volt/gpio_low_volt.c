// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include "gpio_low_volt.h"

#define LOW_VOLT_IRQF (IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_ONESHOT)

#define LOW_VOLT_MAX_LATCH_DELAY_US 100000

enum {
	LOW_VOLT_NONE = 1,
	LOW_VOLT_INT = 2
};

struct low_volt_detect_info {
	struct device *dev;

	struct thermal_zone_device *tzd;
	struct gpio_desc *low_volt_gpiod;
	int low_volt_irq;
	bool is_hw_throttle;
	void (*low_volt_fired_cb)(void *data, bool is_throttle);
	void *low_volt_cb_data;

	long enable_flag;

	struct regulator *vddref;
	bool is_latch_enabled;
};

static struct low_volt_detect_info *stored_info;
static uint32_t latch_delay_us = 1000;

module_param(latch_delay_us, uint, 0644);

static irqreturn_t low_volt_irq_handler(int irq, void *dev_id) {
	struct low_volt_detect_info *info = dev_id;

	info->is_hw_throttle = gpiod_get_value(info->low_volt_gpiod);

	/* Inform thermal zone update */
	thermal_zone_device_update(info->tzd, THERMAL_EVENT_UNSPECIFIED);

	if (info->low_volt_fired_cb)
		info->low_volt_fired_cb(info->low_volt_cb_data, info->is_hw_throttle);

	dev_info(info->dev, "%s: %d\n", __func__, info->is_hw_throttle);

	return IRQ_HANDLED;
}

int low_volt_register_callback(void (*callback)(void *data, bool is_throttle), void *data)
{
	bool prev_irq_status = false;

	if (test_and_clear_bit(0, &stored_info->enable_flag)) {
		disable_irq(stored_info->low_volt_irq);
		prev_irq_status = true;
	}

	stored_info->low_volt_fired_cb = callback;
	stored_info->low_volt_cb_data = data;
	dev_info(stored_info->dev, "registered low voltage callback\n");

	if (prev_irq_status) {
		if (!test_and_set_bit(0, &stored_info->enable_flag)) {
			enable_irq(stored_info->low_volt_irq);
		}
	}

	return 0;
}
EXPORT_SYMBOL(low_volt_register_callback);

static const struct of_device_id gpio_dt_ids[] = {
	{
		.compatible = "gpio,low-volt-detect",
	},
	{/* sentinel */}
};

static int get_temp(void *data, int *state)
{
	struct low_volt_detect_info *info =
		(struct low_volt_detect_info *) data;

	if (info->is_hw_throttle)
		*state = LOW_VOLT_INT;
	else
		*state = LOW_VOLT_NONE;

	return 0;
}

static ssize_t enable_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct low_volt_detect_info *info = platform_get_drvdata(pdev);
	int ret;

	if (!test_and_set_bit(0, &info->enable_flag)) {
		if (info->vddref) {
			ret = regulator_enable(info->vddref);
			if (ret) {
				dev_err(info->dev,
					"regulator enable failed\n");
				return ret;
			}

			dev_info(info->dev, "enable vddref success!\n");
		}

		enable_irq(info->low_volt_irq);
		dev_info(info->dev, "enable\n");
	}

	return count;
}
static DEVICE_ATTR_WO(enable);

static ssize_t latch_clear_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct low_volt_detect_info *info = platform_get_drvdata(pdev);
	int ret;

	if (info->vddref && info->is_latch_enabled) {
		ret = regulator_disable(info->vddref);
		if (ret) {
			dev_err(info->dev,
				"regulator disable failed\n");
			return ret;
		}

		if (latch_delay_us > 0 &&
			latch_delay_us <= LOW_VOLT_MAX_LATCH_DELAY_US)
			usleep_range(latch_delay_us, latch_delay_us + 100);

		ret = regulator_enable(info->vddref);
		if (ret) {
			dev_err(info->dev,
				"regulator enable failed\n");
			return ret;
		}

		dev_info(info->dev, "latch clear completed\n");
	}

	return count;
}
static DEVICE_ATTR_WO(latch_clear);

static struct attribute *gpio_low_volt_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_latch_clear.attr,
        NULL,
};

static struct attribute_group gpio_low_volt_attr_grp = {
	.name = "gpio_low_volt",
	.attrs = gpio_low_volt_attrs,
};

static struct thermal_zone_of_device_ops low_volt_thermal_ops = {
	.get_temp = get_temp,
};

static int low_volt_pdrv_probe(struct platform_device *pdev) {
	int ret;
	struct device *dev = &pdev->dev;
	struct low_volt_detect_info *info;
	struct thermal_zone_device *tzd = NULL;
	struct device_node *node = dev->of_node;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	info->low_volt_gpiod =
		devm_gpiod_get_optional(dev, "low-volt-int", GPIOD_IN);

	if (IS_ERR(info->low_volt_gpiod)) {
		dev_err(dev, "Failed to retrieve low-volt-int gpio: %ld\n",
			PTR_ERR(info->low_volt_gpiod));
		return PTR_ERR(info->low_volt_gpiod);
	}

	info->vddref = devm_regulator_get(dev, "vddref");
	if (IS_ERR(info->vddref)) {
		info->vddref = NULL;
		dev_dbg(dev, "No vddref support, ignore\n");
	}

	info->is_latch_enabled = of_property_read_bool(node,
				"hwcomparator,latch");

	if (info->low_volt_gpiod) {
		info->low_volt_irq = gpiod_to_irq(info->low_volt_gpiod);
		if (info->low_volt_irq < 0) {
			dev_err(dev, "failed to get low_volt IRQ\n");
			return info->low_volt_irq;
		}

		tzd = devm_thermal_zone_of_sensor_register(dev, 0, info,
					&low_volt_thermal_ops);
		if (IS_ERR(tzd)) {
			dev_err(dev, "failed to register thermal zone\n");
			return PTR_ERR(tzd);
		}

		info->tzd = tzd;

		ret = devm_request_threaded_irq(dev, info->low_volt_irq, NULL,
					low_volt_irq_handler, LOW_VOLT_IRQF,
					pdev->name, info);
		if (ret < 0) {
			dev_err(dev, "failed to request ID IRQ\n");
			return ret;
		}

		/* Disable IRQ during boot */
		disable_irq(info->low_volt_irq);

		/* create sysfs groups */
		ret = sysfs_create_group(&pdev->dev.kobj,
			&gpio_low_volt_attr_grp);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"failed to create sysfs files\n");
			return ret;
		}
	}

	device_init_wakeup(dev, true);
	enable_irq_wake(info->low_volt_irq);

	platform_set_drvdata(pdev, info);
	stored_info = info;
	dev_dbg(dev, "Successfully probed\n");
	return 0;
}

static int low_volt_pdrv_remove(struct platform_device *pdev)
{
	struct low_volt_detect_info *info = platform_get_drvdata(pdev);

	disable_irq_wake(info->low_volt_irq);
	device_init_wakeup(&pdev->dev, false);
	info->low_volt_fired_cb = NULL;
	info->low_volt_cb_data = NULL;

	sysfs_remove_group(&pdev->dev.kobj, &gpio_low_volt_attr_grp);

	return 0;
}

#ifdef CONFIG_HIBERNATION
static int low_volt_pdrv_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct low_volt_detect_info *info = platform_get_drvdata(pdev);

	enable_irq_wake(info->low_volt_irq);
	enable_irq(info->low_volt_irq);

	return 0;
}

static int low_volt_pdrv_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct low_volt_detect_info *info = platform_get_drvdata(pdev);

	disable_irq_wake(info->low_volt_irq);
	disable_irq(info->low_volt_irq);

	return 0;
}

static const struct dev_pm_ops low_volt_pm_ops = {
	.freeze  = low_volt_pdrv_freeze,
	.thaw    = low_volt_pdrv_restore,
	.restore = low_volt_pdrv_restore,
};
#endif

static struct platform_driver drv = {
	.probe = low_volt_pdrv_probe,
	.remove = low_volt_pdrv_remove,
	.driver = {
		.name = "gpio_low_volt",
		.of_match_table = of_match_ptr(gpio_dt_ids),
		.owner = THIS_MODULE,
#ifdef CONFIG_HIBERNATION
		.pm = &low_volt_pm_ops,
#endif
	},
};
module_platform_driver(drv);

MODULE_DESCRIPTION("Low volt detected by GPIO");
MODULE_LICENSE("GPL v2");

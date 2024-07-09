// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rt600-ctrl.h>
#include <linux/workqueue.h>

struct rt600_ctrl_ctx {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_boot_normal;
	struct pinctrl_state *pin_boot_flashing;
	struct work_struct boot_work;
	struct work_struct reset_work;
	struct work_struct reset_work_spl;
	unsigned int crash_notify_gpio;
	unsigned int rstn_gpio;
	unsigned int nirq_gpio;
	// port << 8 | pin
	uint32_t nirq_pinmap;
	enum rt600_boot_state boot_state;
	char *state_show;
	struct kernfs_node *crash_attr_node;
	atomic_t reset_complete;
	struct kernfs_node *reset_complete_attr_node;
};

#define RT600_RESET_DELAY    	100
#define RT600_WAKE_TIME_MS 		1000

#define RT600_BOOT_STATE_NORMAL      "normal"
#define RT600_BOOT_STATE_FLASHING    "flashing"

static BLOCKING_NOTIFIER_HEAD(state_subscribers);

static ssize_t boot_state_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", ctx->state_show);
}

static ssize_t boot_state_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	if (!strncmp(buf, RT600_BOOT_STATE_FLASHING,
		     strlen(RT600_BOOT_STATE_FLASHING))) {
		if (ctx->boot_state == flashing) {
			dev_info(ctx->dev, "Already in flashing mode...");
		} else {
			ctx->boot_state = flashing;
			ctx->state_show = RT600_BOOT_STATE_FLASHING;
			schedule_work(&ctx->boot_work);
		}
	} else if (!strncmp(buf, RT600_BOOT_STATE_NORMAL,
			    strlen(RT600_BOOT_STATE_NORMAL))) {
		if (ctx->boot_state == normal) {
			dev_info(ctx->dev, "Already in normal mode...");
		} else {
			ctx->boot_state = normal;
			ctx->state_show = RT600_BOOT_STATE_NORMAL;
			schedule_work(&ctx->boot_work);
		}
	}

	return count;
}
static DEVICE_ATTR_RW(boot_state);

static void toggle_reset(struct rt600_ctrl_ctx *ctx)
{
	gpio_set_value(ctx->rstn_gpio, 1);
	msleep(RT600_RESET_DELAY);
	gpio_set_value(ctx->rstn_gpio, 0);
}

static void toggle_reset_spl(struct rt600_ctrl_ctx *ctx)
{
	gpio_direction_output(ctx->nirq_gpio, 0);
	gpio_set_value(ctx->nirq_gpio, 0);
	toggle_reset(ctx);
	msleep(RT600_RESET_DELAY);
	gpio_direction_input(ctx->nirq_gpio);
}

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res) && res && atomic_xchg(&ctx->reset_complete, 0))
		schedule_work(&ctx->reset_work);

	return count;
}

static ssize_t reset_spl_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res) && res && atomic_xchg(&ctx->reset_complete, 0))
		schedule_work(&ctx->reset_work_spl);

	return count;
}


static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_WO(reset_spl);


static ssize_t nirq_pinmap_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", ctx->nirq_pinmap);
}
static DEVICE_ATTR_RO(nirq_pinmap);

static ssize_t nirq_value_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", gpio_get_value(ctx->nirq_gpio));
}
static DEVICE_ATTR_RO(nirq_value);

static ssize_t crash_notify_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", gpio_get_value(ctx->crash_notify_gpio));
}
static DEVICE_ATTR_RO(crash_notify);

static irqreturn_t crash_notify_irq_handler(int irq, void *data)
{
	struct rt600_ctrl_ctx *ctx = data;

	pm_wakeup_event(ctx->dev, RT600_WAKE_TIME_MS);

	if (ctx && ctx->crash_attr_node) {
		sysfs_notify_dirent(ctx->crash_attr_node);
	}

	return IRQ_HANDLED;
}

int rt600_event_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&state_subscribers, nb);
}
EXPORT_SYMBOL(rt600_event_register);

int rt600_event_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&state_subscribers, nb);
}
EXPORT_SYMBOL(rt600_event_unregister);

static void boot_work(struct work_struct *work)
{
	int rc;
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, boot_work);

	switch (ctx->boot_state) {
	case normal:
		dev_info(ctx->dev, "Setting normal mode...");
		rc = pinctrl_select_state(ctx->pinctrl,
				ctx->pin_boot_normal);
		if (rc) {
			dev_err(ctx->dev, "Failed to select the normal boot state");
			return;
		}
		break;
	case flashing:
		dev_info(ctx->dev, "Setting flashing mode...");
		rc = pinctrl_select_state(ctx->pinctrl,
				ctx->pin_boot_flashing);
		if (rc) {
			dev_err(ctx->dev, "Failed to select the flashing boot state");
			return;
		}
		break;
	}

	toggle_reset(ctx);
	blocking_notifier_call_chain(&state_subscribers, ctx->boot_state, NULL);
}

static void reset_complete_notify(struct rt600_ctrl_ctx *ctx)
{
	atomic_set(&ctx->reset_complete, 1);
	if (ctx && ctx->reset_complete_attr_node) {
		sysfs_notify_dirent(ctx->reset_complete_attr_node);
	} else {
		dev_err(ctx->dev, "failed to notify reset_complete");
	}
}

static void reset_work(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, reset_work);

	dev_info(ctx->dev, "Resetting ...");
	toggle_reset(ctx);
	reset_complete_notify(ctx);
}

static void reset_work_spl(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, reset_work_spl);

	dev_info(ctx->dev, "Resetting to SPL...");
	toggle_reset_spl(ctx);
	reset_complete_notify(ctx);
}

static ssize_t reset_complete_show(struct device *dev,
			                       struct device_attribute *attr,
			                       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ctx->reset_complete));
}
static DEVICE_ATTR_RO(reset_complete);

#define NIRQ_PINMAP_COUNT 2
static int rt600_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rt600_ctrl_ctx *ctx =
		devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int irq_number = 0;
	int rc = 0;
	uint32_t nirq_pinmap[NIRQ_PINMAP_COUNT];

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(ctx->pinctrl)) {
		dev_err(dev, "failed to get pinctrl");
		return PTR_ERR_OR_ZERO(ctx->pinctrl) ?: -EINVAL;
	}

	ctx->pin_boot_normal = pinctrl_lookup_state(ctx->pinctrl, "default");
	if (IS_ERR_OR_NULL(ctx->pin_boot_normal)) {
		dev_err(dev, "failed to look up default pin state");
		return PTR_ERR_OR_ZERO(ctx->pin_boot_normal) ?: -EINVAL;
	}

	ctx->pin_boot_flashing = pinctrl_lookup_state(ctx->pinctrl, "flashing");
	if (IS_ERR_OR_NULL(ctx->pin_boot_flashing)) {
		dev_err(dev, "failed to look up flashing pin state");
		return PTR_ERR_OR_ZERO(ctx->pin_boot_flashing) ?: -EINVAL;
	}

	ctx->rstn_gpio = of_get_named_gpio(dev->of_node, "rstn-gpio", 0);
	if (gpio_is_valid(ctx->rstn_gpio)) {
		if (devm_gpio_request(dev, ctx->rstn_gpio, "rstn_gpio")) {
			dev_err(dev, "failed to request rstn gpio");
			return -EINVAL;
		}
		/* Set to 1 by default */
		gpio_direction_output(ctx->rstn_gpio, 0);
	}

	ctx->nirq_gpio = of_get_named_gpio(dev->of_node, "nirq-gpio", 0);
	if (gpio_is_valid(ctx->nirq_gpio)) {
		if (devm_gpio_request(dev, ctx->nirq_gpio, "nirq_gpio")) {
			dev_err(dev, "failed to request nirq gpio");
			return -EINVAL;
		}
		gpio_direction_input(ctx->nirq_gpio);
	}

	ctx->crash_notify_gpio = of_get_named_gpio(dev->of_node, "crash-notify-gpio", 0);
	if (gpio_is_valid(ctx->crash_notify_gpio)) {
		if (devm_gpio_request(dev, ctx->crash_notify_gpio, "crash_notify_gpio")) {
			dev_warn(dev, "failed to request crash_notify gpio");
		} else {
			gpio_direction_input(ctx->crash_notify_gpio);
			irq_number = gpio_to_irq(ctx->crash_notify_gpio);
			rc = devm_request_irq(dev, irq_number, &crash_notify_irq_handler, IRQF_TRIGGER_FALLING, "crash_notify_gpio", ctx);
			if (rc) {
				dev_err(dev, "crash_notify irq request failure");
				return rc;
			}

			rc = enable_irq_wake(irq_number);
			if (rc) {
				dev_err(dev, "Failed to set IRQ wake for `%d`", irq_number);
				return rc;
			}
		}
	}

	if (of_property_read_u32_array(dev->of_node, "nirq-mcu-map", nirq_pinmap, NIRQ_PINMAP_COUNT)) {
		dev_err(dev, "nirq pinmap not valid");
		return -EINVAL;
	}
	ctx->nirq_pinmap = nirq_pinmap[0] << 8 | nirq_pinmap[1];

	ctx->boot_state = normal;
	ctx->state_show = RT600_BOOT_STATE_NORMAL;

	INIT_WORK(&ctx->boot_work, boot_work);
	INIT_WORK(&ctx->reset_work, reset_work);
	INIT_WORK(&ctx->reset_work_spl, reset_work_spl);

	device_create_file(dev, &dev_attr_boot_state);
	device_create_file(dev, &dev_attr_crash_notify);
	device_create_file(dev, &dev_attr_nirq_value);
	device_create_file(dev, &dev_attr_nirq_pinmap);
	device_create_file(dev, &dev_attr_reset);
	device_create_file(dev, &dev_attr_reset_spl);
	device_create_file(dev, &dev_attr_reset_complete);

	ctx->crash_attr_node = sysfs_get_dirent(ctx->dev->kobj.sd, "crash_notify");
	if(!ctx->crash_attr_node) {
		dev_info(dev, "failed to get crash_notify kernel fs node");
	}

	atomic_set(&ctx->reset_complete, 1);
	ctx->reset_complete_attr_node = sysfs_get_dirent(ctx->dev->kobj.sd, "reset_complete");
	if(!ctx->reset_complete_attr_node) {
		dev_info(dev, "failed to get reset_complete kernel fs node");
	}

	platform_set_drvdata(pdev, ctx);

	rc = device_init_wakeup(ctx->dev, true);
	if (rc) {
		dev_err(dev, "Failed to init wakesource");
		return rc;
	}

	dev_info(dev, "rt600-ctrl probe success.\n");

	return rc;
}

static int rt600_ctrl_remove(struct platform_device *pdev)
{
	struct rt600_ctrl_ctx *ctx = platform_get_drvdata(pdev);

	if (device_init_wakeup(ctx->dev, false)) {
		dev_err(ctx->dev, "Failed to deinit wakesource");
	}

	device_remove_file(ctx->dev, &dev_attr_boot_state);
	device_remove_file(ctx->dev, &dev_attr_crash_notify);
	device_remove_file(ctx->dev, &dev_attr_nirq_value);
	device_remove_file(ctx->dev, &dev_attr_nirq_pinmap);
	device_remove_file(ctx->dev, &dev_attr_reset);
	device_remove_file(ctx->dev, &dev_attr_reset_spl);
	device_remove_file(ctx->dev, &dev_attr_reset_complete);

	devm_kfree(ctx->dev, ctx);

	return 0;
}

static const struct of_device_id rt600_ctrl_of_match[] = {
	{
		.compatible = "meta,rt600_ctrl",
	},
	{},
};

static struct platform_driver rt600_ctrl_driver = {
	.driver = {
		.name = "meta,rt600_ctrl",
		.of_match_table = rt600_ctrl_of_match,
	},
	.probe = rt600_ctrl_probe,
	.remove = rt600_ctrl_remove,
};

static int __init rt600_ctrl_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&rt600_ctrl_driver);
	if (rc) {
		pr_err("Unable to register RT600 ctrl driver:%d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit rt600_ctrl_driver_exit(void)
{
	platform_driver_unregister(&rt600_ctrl_driver);
}

module_init(rt600_ctrl_driver_init);
module_exit(rt600_ctrl_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for RT600 control");

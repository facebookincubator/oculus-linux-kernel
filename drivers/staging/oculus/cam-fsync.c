/*
 * (c) Facebook, Inc. and its affiliates.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/timekeeping.h>
#include <linux/time_namespace.h>

struct cam_fsync_ctx {
	struct device *dev;
	atomic64_t last_timestamp;
	int fsync_gpio;
	int irq;
	int configured;
	struct kernfs_node *timestamp_kn;
};

static ssize_t fsync_timestamp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct cam_fsync_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			 atomic64_read(&ctx->last_timestamp));
}

static DEVICE_ATTR_RO(fsync_timestamp);

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define QTIMER_MUL_FACTOR   10000
#define QTIMER_DIV_FACTOR   192

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct cam_fsync_ctx *ctx = (struct cam_fsync_ctx *)dev_id;
	int64_t timestamp;
	uint64_t ticks;

	ticks = arch_timer_read_counter();
	if (ticks == 0) {
		dev_err(ctx->dev, "qtimer returned 0\n");
		return IRQ_HANDLED;
	}

	timestamp = mul_u64_u32_div(ticks,
			QTIMER_MUL_FACTOR, QTIMER_DIV_FACTOR);

	atomic64_set(&ctx->last_timestamp, timestamp);
	sysfs_notify_dirent(ctx->timestamp_kn);

	return IRQ_HANDLED;
}

static int set_input_pinctrl(struct cam_fsync_ctx *ctx)
{
	int rc = -EINVAL;
	struct device *dev = ctx->dev;
	struct pinctrl_state *pin_input;
	struct pinctrl *pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(dev, "failed to get pinctrl\n");
		return rc;
	}

	pin_input = pinctrl_lookup_state(pinctrl, "input");
	if (IS_ERR_OR_NULL(pin_input)) {
		dev_err(dev, "failed to look up input pin state\n");
		goto done;
	}

	if (pinctrl_select_state(pinctrl, pin_input)) {
		dev_err(dev, "failed to set to input pin state\n");
		goto done;
	}
	rc = 0;

done:
	devm_pinctrl_put(pinctrl);
	return rc;
}

static int configure_intr_gpio(struct cam_fsync_ctx *ctx)
{
	int rc;
	struct device *dev = ctx->dev;

	rc = devm_gpio_request(dev, ctx->fsync_gpio, "cam_fsync");
	if (rc) {
		dev_err(dev, "Failed to request GPIO rc = %d\n", rc);
		return -EINVAL;
	}

	rc = gpio_to_irq(ctx->fsync_gpio);
	if (rc < 0) {
		dev_err(dev, "Failed get IRQ number rc = %d\n", rc);
		goto free_gpio;
	}

	ctx->irq = rc;

	rc = devm_request_irq(dev, ctx->irq, (irq_handler_t)irq_handler,
			      IRQF_TRIGGER_RISING, "cam_fsync", ctx);
	if (rc) {
		dev_err(dev, "Failed request IRQ rc = %d\n", rc);
		goto free_gpio;
	}
	ctx->configured = 1;
	dev_info(dev, "cam_fsync IRQ = %d\n", ctx->irq);

	return 0;

free_gpio:
	devm_gpio_free(dev, ctx->fsync_gpio);

	return rc;
}

static int configure_input_and_intr(struct cam_fsync_ctx *ctx)
{
	int rc;

	rc = set_input_pinctrl(ctx);
	if (!rc)
		rc = configure_intr_gpio(ctx);

	return rc;
}

static ssize_t fsync_config_intr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct cam_fsync_ctx *ctx = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1) && !ctx->configured) {
		configure_input_and_intr(ctx);
	} else {
		if (ctx->configured)
			dev_err(ctx->dev,
				"The intr already configured\n");
		else
			dev_err(ctx->dev,
				"Writing 0 is not supported\n");
	}

	return count;
}

static DEVICE_ATTR_WO(fsync_config_intr);

static int cam_fsync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cam_fsync_ctx *ctx =
		devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	ctx->fsync_gpio = of_get_named_gpio(
		dev->of_node, "fsync-gpio", 0);

	if (!gpio_is_valid(ctx->fsync_gpio)) {
		dev_err(dev, "FSYNC GPIO not valid\n");
		rc = ctx->fsync_gpio;
		goto free_context;
	}

	if (of_find_property(dev->of_node, "delayed-intr-config", NULL)) {
		rc = device_create_file(dev, &dev_attr_fsync_config_intr);
		if (rc) {
			dev_err(dev, "Cannot create fsync_config_intr\n");
			goto free_context;
		}
	} else {
		rc = configure_input_and_intr(ctx);
		if (rc)
			goto free_context;
	}

	platform_set_drvdata(pdev, ctx);

	atomic64_set(&ctx->last_timestamp, 0LL);

	rc = device_create_file(dev, &dev_attr_fsync_timestamp);
	if (rc) {
		dev_err(dev, "Cannot create fsync_timestamp\n");
		goto free_context;
	}

	ctx->timestamp_kn = sysfs_get_dirent(dev->kobj.sd, "fsync_timestamp");

	dev_info(dev, "cam_fsync probe success.\n");

	return rc;

free_context:
	devm_kfree(dev, ctx);

	return rc;
}

static int cam_fsync_remove(struct platform_device *pdev)
{
	struct cam_fsync_ctx *ctx = platform_get_drvdata(pdev);

	devm_kfree(ctx->dev, ctx);

	return 0;
}

static const struct of_device_id cam_fsync_of_match[] = {
	{
		.compatible = "oculus,cam_fsync",
	},
	{},
};

static struct platform_driver cam_fsync_driver = {
	.driver = {
		.name = "oculus,cam_fsync",
		.of_match_table = cam_fsync_of_match,
	},
	.probe = cam_fsync_probe,
	.remove = cam_fsync_remove,
};

static int __init cam_fsync_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&cam_fsync_driver);
	if (rc) {
		pr_err("Unable to register cam fsync driver:%d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit cam_fsync_driver_exit(void)
{
	platform_driver_unregister(&cam_fsync_driver);
}

module_init(cam_fsync_driver_init);
module_exit(cam_fsync_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for Camera Fsync");

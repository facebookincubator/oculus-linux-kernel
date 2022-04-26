// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
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
	struct pinctrl_state *pin_boot_soc_active;
	struct work_struct boot_work;
	struct work_struct reset_work;
	unsigned int rstn_gpio;
	unsigned int spare_gpio;
	enum rt600_boot_state boot_state;
	char *state_show;
};

#define RT600_RESET_DELAY    100

#define RT600_BOOT_STATE_NORMAL      "normal"
#define RT600_BOOT_STATE_FLASHING    "flashing"
#define RT600_BOOT_STATE_SOC_ACTIVE  "soc_active"

#define RT600_SPARE_GPIO_NAME        "rt600-spare-gpio"

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
	} else if (!strncmp(buf, RT600_BOOT_STATE_SOC_ACTIVE,
				strlen(RT600_BOOT_STATE_SOC_ACTIVE))) {
		if (ctx->boot_state == soc_active) {
			dev_info(ctx->dev, "Already in soc_active mode...");
		} else {
			ctx->boot_state = soc_active;
			ctx->state_show = RT600_BOOT_STATE_SOC_ACTIVE;
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

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res) && res)
		schedule_work(&ctx->reset_work);

	return count;
}
static DEVICE_ATTR_WO(reset);

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
	case soc_active:
		/* Don't reset MCU in this case. Just update GPIO state only */
		if (!ctx->pin_boot_soc_active)
			return;

		dev_info(ctx->dev, "Setting soc_active mode...");
		rc = pinctrl_select_state(ctx->pinctrl,
				ctx->pin_boot_soc_active);
		if (rc)
			dev_err(ctx->dev, "Failed to select the soc_active boot state");

		return;
	}

	toggle_reset(ctx);
	blocking_notifier_call_chain(&state_subscribers, ctx->boot_state, NULL);
}

static void reset_work(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, reset_work);

	dev_info(ctx->dev, "Resetting ...");
	toggle_reset(ctx);
}

static int rt600_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rt600_ctrl_ctx *ctx =
		devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(ctx->pinctrl)) {
		rc = PTR_ERR_OR_ZERO(ctx->pinctrl) ?: -EINVAL;
		dev_err(dev, "failed to get pinctrl");
		goto fail_pinctrl;
	}

	ctx->pin_boot_normal = pinctrl_lookup_state(ctx->pinctrl, "default");
	if (IS_ERR_OR_NULL(ctx->pin_boot_normal)) {
		rc = PTR_ERR_OR_ZERO(ctx->pin_boot_normal) ?: -EINVAL;
		dev_err(dev, "failed to look up default pin state");
		goto free_pinctrl;
	}

	ctx->pin_boot_flashing = pinctrl_lookup_state(ctx->pinctrl, "flashing");
	if (IS_ERR_OR_NULL(ctx->pin_boot_flashing)) {
		rc = PTR_ERR_OR_ZERO(ctx->pin_boot_flashing) ?: -EINVAL;
		dev_err(dev, "failed to look up flashing pin state");
		goto free_pinctrl;
	}

	ctx->pin_boot_soc_active = pinctrl_lookup_state(ctx->pinctrl,
					"soc_active");
	if (IS_ERR_OR_NULL(ctx->pin_boot_soc_active))
		ctx->pin_boot_soc_active = NULL;
	else
		dev_info(dev, "soc active state configured\n");

	ctx->rstn_gpio = of_get_named_gpio(dev->of_node, "rstn-gpio", 0);
	if (gpio_is_valid(ctx->rstn_gpio)) {
		if (devm_gpio_request(dev, ctx->rstn_gpio, "rstn_gpio")) {
			dev_err(dev, "failed to request rstn gpio");
			goto free_pinctrl;
		}
		/* Set to 1 by default */
		gpio_direction_output(ctx->rstn_gpio, 0);
	}

	ctx->spare_gpio = of_get_named_gpio(dev->of_node,
					    RT600_SPARE_GPIO_NAME, 0);
	if (gpio_is_valid(ctx->spare_gpio)) {
		if (devm_gpio_request(dev, ctx->spare_gpio, "spare_gpio")) {
			dev_err(dev, "failed to request spare gpio");
			goto free_pinctrl;
		}
		if (gpio_export(ctx->spare_gpio, true)) {
			dev_err(dev, "failed to export spare gpio");
			goto free_pinctrl;
		}
		if (gpio_export_link(dev, RT600_SPARE_GPIO_NAME,
				     ctx->spare_gpio)) {
			dev_err(dev, "failed to export spare gpio link");
			goto free_pinctrl;
		}
	}

	ctx->boot_state = normal;
	ctx->state_show = RT600_BOOT_STATE_NORMAL;

	INIT_WORK(&ctx->boot_work, boot_work);
	INIT_WORK(&ctx->reset_work, reset_work);

	device_create_file(dev, &dev_attr_boot_state);
	device_create_file(dev, &dev_attr_reset);
	platform_set_drvdata(pdev, ctx);

	dev_info(dev, "rt600-ctrl probe success.\n");

	return rc;

free_pinctrl:
	devm_pinctrl_put(ctx->pinctrl);
fail_pinctrl:
	devm_kfree(dev, ctx);

	return rc;
}

static int rt600_ctrl_remove(struct platform_device *pdev)
{
	struct rt600_ctrl_ctx *ctx = platform_get_drvdata(pdev);

	devm_kfree(ctx->dev, ctx);

	return 0;
}

static const struct of_device_id rt600_ctrl_of_match[] = {
	{
		.compatible = "oculus,rt600_ctrl",
	},
	{},
};

static struct platform_driver rt600_ctrl_driver = {
	.driver = {
		.name = "oculus,rt600_ctrl",
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

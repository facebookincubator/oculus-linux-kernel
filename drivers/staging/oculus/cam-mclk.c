/*
 * (c) Facebook, Inc. and its affiliates.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

struct ctx {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_active;
	struct work_struct start_worker;
	struct work_struct stop_worker;
	size_t clk_cnt;
	struct clk **clks;
	u32 *rates;
	bool *clk_enabled;
	bool on;
};

static ssize_t _show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", ctx->on ? "on" : "off");
}

static ssize_t _store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res)) {
		bool turn_on = (res > 0);

		if (ctx->on != turn_on) {
			ctx->on = turn_on;
			if (turn_on)
				schedule_work(&ctx->start_worker);
			else
				schedule_work(&ctx->stop_worker);
		} else {
			dev_info(ctx->dev, "Already %s",
				 ctx->on ? "on" : "off");
		}
	}

	return count;
}

static DEVICE_ATTR(control, 0644, _show, _store);

static int start_clks(struct ctx *ctx)
{
	int i;
	int rc;

	for (i = 0; i < ctx->clk_cnt; i++) {
		if (ctx->clk_enabled[i])
			continue;

		if (ctx->rates[i]) {
			rc = clk_set_rate(ctx->clks[i], ctx->rates[i]);
			if (rc)
				return rc;
		}

		rc = clk_prepare_enable(ctx->clks[i]);
		if (rc)
			return rc;

		ctx->clk_enabled[i] = true;
	}

	return 0;
}

static int pinctrl_init(struct ctx *ctx)
{
	ctx->pinctrl = devm_pinctrl_get(ctx->dev);
	if (IS_ERR_OR_NULL(ctx->pinctrl)) {
		dev_err(ctx->dev, "Failed to look up pinctrl");
		return PTR_ERR_OR_ZERO(ctx->pinctrl) ?: -EINVAL;
	}

	ctx->pin_default = pinctrl_lookup_state(ctx->pinctrl, "standby");
	if (IS_ERR_OR_NULL(ctx->pin_default)) {
		dev_err(ctx->dev, "Failed to look up standby pin state");
		return PTR_ERR_OR_ZERO(ctx->pin_default) ?: -EINVAL;
	}

	ctx->pin_active = pinctrl_lookup_state(ctx->pinctrl, "active");
	if (IS_ERR_OR_NULL(ctx->pin_active)) {
		dev_err(ctx->dev, "Failed to look up active pin state");
		return PTR_ERR_OR_ZERO(ctx->pin_active) ?: -EINVAL;
	}

	return 0;
}

static void stop_clks(struct ctx *ctx)
{
	int i;

	for (i = 0; i < ctx->clk_cnt; i++)
		if (ctx->clk_enabled[i]) {
			clk_disable_unprepare(ctx->clks[i]);
			ctx->clk_enabled[i] = false;
		}
}

static void clock_start_work(struct work_struct *work)
{
	int rc;
	struct ctx *ctx =
		container_of(work, struct ctx, start_worker);

	if (pinctrl_init(ctx)) {
		dev_err(ctx->dev, "Failed to initialize pinctrl");
		return;
	}
	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_active);
	if (rc) {
		dev_err(ctx->dev, "Failed to select active pin state");
		return;
	}

	rc = start_clks(ctx);
	if (rc) {
		dev_err(ctx->dev, "Failed to start mclk");
		stop_clks(ctx);
	}

	dev_info(ctx->dev, "mclk started");
}

static void clock_stop_work(struct work_struct *work)
{
	int rc;
	struct ctx *ctx =
		container_of(work, struct ctx, stop_worker);

	stop_clks(ctx);

	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_default);
	if (rc) {
		dev_err(ctx->dev,
			"Failed to select default pin state");
	}
	devm_pinctrl_put(ctx->pinctrl);
	dev_info(ctx->dev, "mclk stopped");
}

static int cam_mclk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *of_node = dev->of_node;
	int i;
	int rc = 0;
	size_t clock_cnt, rate_cnt;
	const char *clk_name = NULL;
	struct ctx *ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	INIT_WORK(&ctx->start_worker, clock_start_work);
	INIT_WORK(&ctx->stop_worker, clock_stop_work);

	clock_cnt = of_property_count_strings(of_node, "clock-names");
	if (clock_cnt <= 0) {
		dev_err(dev, "No clock-names in device tree\n");
		return -EINVAL;
	}

	rate_cnt = of_property_count_u32_elems(of_node, "clock-rates");
	if (rate_cnt <= 0) {
		dev_err(dev, "No clock-rates in device tree\n");
		return -EINVAL;
	}

	if (clock_cnt != rate_cnt) {
		dev_err(dev,
			"Name and rate number mis-match in device tree\n");
		dev_err(dev,
			"  clock_cnt = %zu, rate_cnt = %zu\n",
			clock_cnt, rate_cnt);
		return -EINVAL;
	}

	ctx->clks = devm_kcalloc(dev, clock_cnt, sizeof(struct clk *),
				 GFP_KERNEL);
	if (!ctx->clks)
		return -ENOMEM;

	ctx->rates = devm_kcalloc(dev, clock_cnt, sizeof(uint32_t),
				  GFP_KERNEL);
	if (!ctx->rates)
		return -ENOMEM;

	ctx->clk_enabled = devm_kcalloc(dev, clock_cnt, sizeof(bool),
					GFP_KERNEL);
	if (!ctx->clk_enabled)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "clock-rates",
					ctx->rates, rate_cnt);
	if (rc < 0) {
		dev_err(dev, "Failed to read clock rates\n");
		return rc;
	}

	for (i = 0; i < clock_cnt; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
						   i, &clk_name);
		if (rc < 0) {
			dev_err(dev,
				"Reading clock name failed\n");
			return rc;
		}

		ctx->clks[i] = devm_clk_get(dev, clk_name);
		if (IS_ERR_OR_NULL(ctx->clks[i]))
			return PTR_ERR_OR_ZERO(ctx->clks[i]) ?: -EINVAL;
	}

	ctx->clk_cnt = clock_cnt;
	device_create_file(dev, &dev_attr_control);
	platform_set_drvdata(pdev, ctx);

	dev_info(dev, "cam-mclk probe success.\n");

	return rc;
}

static const struct of_device_id cam_mclk_of_match[] = {
	{
		.compatible = "oculus,cam-mclk",
	},
	{},
};

static struct platform_driver cam_mclk_driver = {
	.driver = {
		.name = "oculus,cam-mclk",
		.of_match_table = cam_mclk_of_match,
	},
	.probe = cam_mclk_probe,
};

static int __init cam_mclk_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&cam_mclk_driver);
	if (rc) {
		pr_err("Unable to register cam-mclk board driver:%d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit cam_mclk_driver_exit(void)
{
	platform_driver_unregister(&cam_mclk_driver);
}

module_init(cam_mclk_driver_init);
module_exit(cam_mclk_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for cam-mclk");

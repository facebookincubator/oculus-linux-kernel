/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "common.h"
#include "clk-debug.h"

#define to_clk_dummy(_hw)	container_of(_hw, struct clk_dummy, hw)

#define RESET_MAX	100

static int dummy_clk_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct clk_dummy *dummy = to_clk_dummy(hw);

	dummy->rrate = rate;

	pr_debug("set rate: %lu\n", rate);

	return 0;
}

static long dummy_clk_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *parent_rate)
{
	return rate;
}

static unsigned long dummy_clk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_dummy *dummy = to_clk_dummy(hw);

	pr_debug("clock rate: %lu\n", dummy->rrate);

	return dummy->rrate;
}

static int dummy_clk_set_flags(struct clk_hw *hw, unsigned int flags)
{
	return 0;
}

struct clk_ops clk_dummy_ops = {
	.set_rate = dummy_clk_set_rate,
	.round_rate = dummy_clk_round_rate,
	.recalc_rate = dummy_clk_recalc_rate,
	.set_flags = dummy_clk_set_flags,
	.debug_init = clk_debug_measure_add,
};
EXPORT_SYMBOL_GPL(clk_dummy_ops);

static int dummy_reset_assert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	pr_debug("\n");
	return 0;
}

static int dummy_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	pr_debug("\n");
	return 0;
}

static struct reset_control_ops dummy_reset_ops = {
	.assert         = dummy_reset_assert,
	.deassert       = dummy_reset_deassert,
};

/**
 * clk_register_dummy - register dummy clock with the
 *				   clock framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @flags: framework-specific flags
 * @node: device node
 */
static struct clk *clk_register_dummy(struct device *dev, const char *name,
		unsigned long flags, struct device_node *node)
{
	struct clk_dummy *dummy;
	struct clk *clk;
	struct clk_init_data init = {};

	/* allocate dummy clock */
	dummy = kzalloc(sizeof(*dummy), GFP_KERNEL);
	if (!dummy)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_dummy_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.num_parents = 0;
	dummy->hw.init = &init;

	/* register the clock */
	clk = clk_register(dev, &dummy->hw);
	if (IS_ERR(clk)) {
		kfree(dummy);
		return clk;
	}

	dummy->reset.of_node = node;
	dummy->reset.ops = &dummy_reset_ops;
	dummy->reset.nr_resets = RESET_MAX;

	if (reset_controller_register(&dummy->reset))
		pr_err("Failed to register reset controller for %s\n", name);
	else
		pr_info("Successfully registered dummy reset controller for %s\n",
								name);

	return clk;
}

/**
 * of_dummy_clk_setup() - Setup function for simple fixed rate clock
 */
static void of_dummy_clk_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = "dummy_clk";

	of_property_read_string(node, "clock-output-names", &clk_name);

	clk = clk_register_dummy(NULL, clk_name, 0, node);
	if (!IS_ERR(clk)) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	} else {
		pr_err("Failed to register dummy clock controller for %s\n",
								clk_name);
		return;
	}

	pr_info("Successfully registered dummy clock controller for %s\n",
								clk_name);
}
CLK_OF_DECLARE(dummy_clk, "qcom,dummycc", of_dummy_clk_setup);

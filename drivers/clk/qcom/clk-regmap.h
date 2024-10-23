/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014, 2016-2021, The Linux Foundation. All rights reserved. */

#ifndef __QCOM_CLK_REGMAP_H__
#define __QCOM_CLK_REGMAP_H__

#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include "vdd-class.h"

struct regmap;

/**
 * struct clk_regmap_ops - Operations for clk_regmap.
 *
 * @list_registers: Queries the hardware to get the current register contents.
 *		    This callback is optional.
 *
 * @list_rate:  On success, return the nth supported frequency for a given
 *		clock that is below rate_max. Return -ENXIO in case there is
 *		no frequency table.
 *
 * @set_flags: Set custom flags which deal with hardware specifics. Returns 0
 *		on success, error otherwise.
 */
struct clk_regmap_ops {
	void	(*list_registers)(struct seq_file *f,
				  struct clk_hw *hw);
	long	(*list_rate)(struct clk_hw *hw, unsigned int n,
			     unsigned long rate_max);
	int	(*set_flags)(struct clk_hw *clk, unsigned long flags);
};

/**
 * struct clk_regmap - regmap supporting clock
 * @hw:		handle between common and hardware-specific interfaces
 * @dependent_hw: dependent clocks clock hw
 * @regmap:	regmap to use for regmap helpers and/or by providers
 * @enable_reg: register when using regmap enable/disable ops
 * @enable_mask: mask when using regmap enable/disable ops
 * @enable_is_inverted: flag to indicate set enable_mask bits to disable
 *                      when using clock_enable_regmap and friends APIs.
 * @vdd_data:	struct containing vdd-class data for this clock
 * @ops: operations this clk_regmap supports
 */

struct clk_regmap {
	struct clk_hw hw;
	struct clk_hw *dependent_hw;
	struct regmap *regmap;
	unsigned int enable_reg;
	unsigned int enable_mask;
	bool enable_is_inverted;
	struct clk_vdd_class_data vdd_data;
	struct clk_regmap_ops *ops;
	struct list_head list_node;
	struct device *dev;
#define QCOM_CLK_IS_CRITICAL BIT(0)
#define QCOM_CLK_BOOT_CRITICAL BIT(1)
	unsigned long flags;
};

static inline struct clk_regmap *to_clk_regmap(struct clk_hw *hw)
{
	return container_of(hw, struct clk_regmap, hw);
}

int clk_is_enabled_regmap(struct clk_hw *hw);
int clk_enable_regmap(struct clk_hw *hw);
void clk_disable_regmap(struct clk_hw *hw);
int clk_prepare_regmap(struct clk_hw *hw);
void clk_unprepare_regmap(struct clk_hw *hw);
int clk_pre_change_regmap(struct clk_hw *hw, unsigned long cur_rate,
			unsigned long new_rate);
int clk_post_change_regmap(struct clk_hw *hw, unsigned long old_rate,
			unsigned long cur_rate);
int devm_clk_register_regmap(struct device *dev, struct clk_regmap *rclk);

bool clk_is_regmap_clk(struct clk_hw *hw);

struct clk_register_data {
	char *name;
	u32 offset;
};

int clk_runtime_get_regmap(struct clk_regmap *rclk);
void clk_runtime_put_regmap(struct clk_regmap *rclk);

#endif

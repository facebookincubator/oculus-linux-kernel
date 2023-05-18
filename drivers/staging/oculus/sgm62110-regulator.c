// SPDX-License-Identifier: GPL-2.0-or-later
//
// sgm62110-regulator.c  - regulator driver for SGM62110

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>

#define SGM62110_CONTROL_REG 0x01
#define SGM62110_BUCK_ENABLE_MASK 0x20
#define SGM62110_BUCKBOOST_ENABLE_MASK 0x60
#define SGM62110_VOUT1_MASK 0x7f
#define SGM62110_VOUT_N_VOLTAGE		0x80
#define SGM62110_VOUT1_REG 0x04
#define SGM62110_REG_MAX 0x05

#define SGM62110_BUCK_RAMP_DELAY	50

#define SGM62110_BUCK_VOLT_MIN	1800000
#define SGM62110_BUCK_VOLT_MAX	4975000
#define SGM62110_BUCK_VOLT_STEP	25000

#define SGM62110_BUCKBOOST_VOLT_MIN	2025000
#define SGM62110_BUCKBOOST_VOLT_MAX	5200000
#define SGM62110_BUCKBOOST_VOLT_STEP	25000

#define SGM62110_REGULATOR_ID_BUCK 0
#define SGM62110_REGULATOR_ID_BUCKBOOST 1

#define SGM62110_MAX_REGULATORS 2

struct sgm62110_regulator_info {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_desc *rdesc;
	struct gpio_desc *en_gpiod;
	int en_gpio_state;
};

static const struct regmap_config sgm62110_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SGM62110_REG_MAX,
};

static const struct linear_range SGM62110_BUCK_RANGES[] = {
	REGULATOR_LINEAR_RANGE(SGM62110_BUCK_VOLT_MIN, 0x0, 0x7f, SGM62110_BUCK_VOLT_STEP),
};

static const struct linear_range SGM62110_BUCKBOOST_RANGES[] = {
	REGULATOR_LINEAR_RANGE(SGM62110_BUCKBOOST_VOLT_MIN, 0x0, 0x7f, SGM62110_BUCKBOOST_VOLT_STEP),
};

static int sgm62110_set_voltage_time_sel(struct regulator_dev *rdev,
				unsigned int old_selector,
				unsigned int new_selector)
{
	if (new_selector > old_selector) {
		return DIV_ROUND_UP(SGM62110_BUCK_VOLT_STEP *
				(new_selector - old_selector),
				SGM62110_BUCK_RAMP_DELAY);
	}

	return 0;
}

#define SGM62110_BUCK(_id, _name)						\
	[SGM62110_REGULATOR_ID_##_id] = {					\
		.id = SGM62110_REGULATOR_ID_##_id,				\
		.name = "sgm62110-"#_name,					\
		.of_match = of_match_ptr(#_name),				\
		.ops = &sgm62110_common_ops,					\
		.n_voltages = SGM62110_VOUT_N_VOLTAGE,				\
		.enable_reg = SGM62110_CONTROL_REG,				\
		.enable_mask = SGM62110_##_id##_ENABLE_MASK,			\
		.vsel_reg = SGM62110_VOUT1_REG,					\
		.vsel_mask = SGM62110_VOUT1_MASK,				\
		.linear_ranges		= SGM62110_##_id##_RANGES,		\
		.n_linear_ranges	= ARRAY_SIZE(SGM62110_##_id##_RANGES),	\
		.owner = THIS_MODULE,						\
	}

static const struct regulator_ops sgm62110_common_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_linear_range,
	.map_voltage		= regulator_map_voltage_linear_range,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= sgm62110_set_voltage_time_sel,
};

static struct regulator_desc sgm62110_regulators_desc[] = {
	SGM62110_BUCK(BUCK, buck),
	SGM62110_BUCK(BUCKBOOST, buckboost),
};

static int sgm62110_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sgm62110_regulator_info *info;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regulator_desc *desc;
	struct regmap *regmap;
	struct device_node *of_node = dev->of_node;
	bool use_buckboost_mode;

	info = devm_kzalloc(dev, sizeof(struct sgm62110_regulator_info),
				GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->en_gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(info->en_gpiod)) {
		dev_err(dev, "devm_gpiod_get 'enable' failed: %ld\n", PTR_ERR(info->en_gpiod));
		return PTR_ERR(info->en_gpiod);
	}

	if (!IS_ERR(info->en_gpiod)) {
		gpiod_set_value(info->en_gpiod, 1);
		dev_dbg(info->dev, "Set gpio state: %d sucess\n",
			info->en_gpio_state);
	}

	/* 250us for output ramp start delay + 250us buffer */
	usleep_range(500, 1000);

	info->rdesc = sgm62110_regulators_desc;
	regmap = devm_regmap_init_i2c(client, &sgm62110_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to allocate regmap!\n");
		return PTR_ERR(regmap);
	}

	info->regmap = regmap;
	info->dev = dev;

	i2c_set_clientdata(client, info);

	config.dev = dev;
	config.regmap = regmap;
	config.driver_data = info;

	use_buckboost_mode = of_property_read_bool(of_node, "using-buckboost-mode");
	desc = use_buckboost_mode ? &sgm62110_regulators_desc[1] : &sgm62110_regulators_desc[0];
	rdev = devm_regulator_register(dev, desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(dev, "Failed to register regulator!\n");
		return PTR_ERR(rdev);
	}

	return 0;
}

static const struct of_device_id __maybe_unused sgm62110_of_match[] = {
	{ .compatible = "sgm,sgm62110" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sgm62110_of_match);

static const struct i2c_device_id sgm62110_id[] = {
	{ "sgm62110-regulator" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, sgm62110_id);

static struct i2c_driver sgm62110_regulator_driver = {
	.driver = {
		.name = "sgm62110",
		.of_match_table = of_match_ptr(sgm62110_of_match),
	},
	.probe_new = sgm62110_i2c_probe,
	.id_table = sgm62110_id,
};
module_i2c_driver(sgm62110_regulator_driver);

MODULE_AUTHOR("Tim <tim.wangjl@goertek.com>");
MODULE_DESCRIPTION("SGM62110 BUCK-BOOST regulator driver");
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0+

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include "max77813.h"

#define DRIVER_NAME		"max77813"

struct max77813_chip {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
};

static int max77813_get_status(struct regulator_dev *rdev)
{
	struct max77813_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77813_REG_STATUS, &data);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to read i2c: %d @ function %s\n",
			ret, __func__);
		return ret;
	}
	return (data & MAX77813_MASK_ST);
}

static int max77813_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct max77813_chip *pchip = rdev_get_drvdata(rdev);
	int ret;

	if (mode != REGULATOR_MODE_NORMAL)
		ret = regmap_update_bits(pchip->regmap,
				MAX77813_REG_CONFIG1,
				MAX77813_MASK_FPWM, 0);
	else
		ret = regmap_update_bits(pchip->regmap,
				MAX77813_REG_CONFIG1,
				MAX77813_MASK_FPWM, MAX77813_MASK_FPWM);

	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to update i2c: %d @ function %s\n",
			ret, __func__);
	}
	return ret;
}

static unsigned int max77813_get_mode(struct regulator_dev *rdev)
{
	struct max77813_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int rval;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77813_REG_CONFIG1, &rval);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to read i2c: %d @ function %s\n",
			ret, __func__);
		return ret;
	}

	return (rval & MAX77813_MASK_FPWM)
			? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static struct regulator_ops max77813_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.get_status = max77813_get_status,
	.set_mode = max77813_set_mode,
	.get_mode = max77813_get_mode,
};

static const struct regmap_config max77813_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX77813_REG_VOUT,
	.use_single_read = true,
	.use_single_write = true,
};

static struct regulator_desc max77813_reg_vout = {
	.name = "vout",
	.id = 0,
	.ops = &max77813_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.enable_mask = MAX77813_MASK_BB_EN,
	.enable_reg = MAX77813_REG_CONFIG2,
	.min_uV = MAX77813_VOUT_MIN_UV,
	.uV_step = MAX77813_VOUT_STEP_UV,
	.n_voltages = MAX77813_MASK_VOUT + 1,
	.vsel_reg = MAX77813_REG_VOUT,
	.vsel_mask = MAX77813_MASK_VOUT,
	.active_discharge_off = MAX77813_AD_DISABLE,
	.active_discharge_on = MAX77813_MASK_AD,
	.active_discharge_mask = MAX77813_MASK_AD,
	.active_discharge_reg = MAX77813_REG_CONFIG1,
};

static int max77813_init_regulator(struct max77813_chip *pchip,
				   struct device_node *node)
{
	int ret;
	struct regulator_init_data init_data = {
		.constraints = {
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
			.min_uV = MAX77813_VOUT_MIN_UV,
			.max_uV = MAX77813_VOUT_MAX_UV,
			.keep_on = true,
		},
	};
	struct regulator_config config = { };

	config.regmap = pchip->regmap;
	config.driver_data = pchip;
	config.dev = pchip->dev;
	config.init_data = &init_data;

	pchip->regulator_desc = max77813_reg_vout;
	pchip->regulator = devm_regulator_register(pchip->dev,
			&max77813_reg_vout, &config);

	if (IS_ERR(pchip->regulator)) {
		ret = PTR_ERR(pchip->regulator);
		return ret;
	}
	return 0;
}

static int max77813_regulator_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device_node *node = client->dev.of_node;
	struct device *dev = &client->dev;
	struct max77813_chip *pchip;
	int ret;

	pchip = devm_kzalloc(dev, sizeof(struct max77813_chip), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	i2c_set_clientdata(client, pchip);
	pchip->dev = dev;

	pchip->regmap = devm_regmap_init_i2c(client, &max77813_regmap_config);
	if (IS_ERR(pchip->regmap)) {
		ret = PTR_ERR(pchip->regmap);
		dev_err(dev, "failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = max77813_init_regulator(pchip, node);
	if (ret < 0) {
		dev_err(dev, "failed to register regulator: %d\n", ret);
		return ret;
	}

	dev_info(pchip->dev, "max77813 init done\n");
	return 0;
}

static const struct i2c_device_id max77813_i2c_id[] = {
	{DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(i2c, max77813_i2c_id);

static const struct of_device_id max77813_of_match[] = {
	{.compatible = "maxim,max77813"},
	{},
};
MODULE_DEVICE_TABLE(of, max77813_of_match);

static struct i2c_driver max77813_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(max77813_of_match),
	},
	.probe	= max77813_regulator_probe,
	.id_table = max77813_i2c_id,
};
module_i2c_driver(max77813_driver);

MODULE_DESCRIPTION("Regulator Device Driver for Maxim MAX77813");
MODULE_LICENSE("GPL v2");

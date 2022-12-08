// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

/* Compatible table name */
#define AW37504_NODE_TABLE "aw,aw37504"

/* Register MAP */
#define VOUTP_REG 0x00
#define VOUTN_REG 0x01
#define APPS_REG 0x03
#define CTRL_REG 0x04
#define WPRTEN_REG 0x21

/* Register Values */
#define VOLTAGE_VAL 0x14
#define APPS_VAL 0x43
#define ILIMIT_VAL 0x09
#define WPRTEN_OPEN_VAL 0x4C
#define WPRTEN_CLOSE_VAL 0xFF

struct aw_device {
	/* I2C device */
	struct i2c_client *i2c;
	struct regmap *regmap;

	/* regulator values */
	struct regulator_desc reg_desc;
	struct regulator_dev *reg_dev;
	struct regulator_init_data *reg_init_data;

	bool is_enabled;
	bool is_first_enable;
};

static const struct regmap_config aw37504_regmap = {
	.name = "aw37504",
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = WPRTEN_REG,
};

/*
 * Convert from microvolts to a value the register expects
 */
static u8 get_voltage_reg_val(int voltage_uV)
{
	return (voltage_uV - 4000000) / 100000;
}

static int get_reg_to_voltage_val(u8 voltage_uV)
{
	return (voltage_uV * 100000) + 4000000;
}

static int parse_dt(struct device *dev, struct aw_device *aw_dev)
{
	struct regulator_init_data *init_data;

	if (!dev->of_node) {
		dev_err(dev, "%s: No device tree found\n", __func__);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(dev, dev->of_node, &aw_dev->reg_desc);
	if (!init_data)
		return -ENOMEM;

	if (init_data->constraints.min_uV != init_data->constraints.max_uV) {
		dev_err(dev,
			"%s: Fixed regulator specified with variable voltages\n",
			__func__);
		return -EINVAL;
	}

	aw_dev->reg_init_data = init_data;

	return 0;
}

static int read_reg(struct aw_device *aw_dev, u8 reg, u8 *ret)
{
	int rc = 0;
	unsigned int reg_val = 0;

	rc = regmap_read(aw_dev->regmap, reg, &reg_val);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
			"%s: Failed to read fromreg 0x%02x, ret=%d\n",
			__func__, reg, rc);
		return rc;
	}
	*ret = (u8)reg_val;

	return 0;
}

static int write_reg(struct aw_device *aw_dev, u8 reg, u8 val)
{
	int rc = 0;

	rc = regmap_write(aw_dev->regmap, reg, val);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
			"%s: Failed to write 0x%02x to reg 0x%02x, ret=%d\n",
			__func__, val, reg, rc);
		return rc;
	}

	return 0;
}

static bool wpf_enabled(struct aw_device *aw_dev)
{
	int rc = 0;
	u8 reg_val;

	rc = read_reg(aw_dev, WPRTEN_REG, &reg_val);
	if (rc < 0)
		return false;

	return reg_val;
}

static int aw37504_enable(struct regulator_dev *rdev)
{
	struct aw_device *aw_dev = rdev_get_drvdata(rdev);
	bool enabled_on_boot = aw_dev->reg_init_data->constraints.boot_on;

	if (enabled_on_boot && aw_dev->is_first_enable) {
		aw_dev->is_enabled = true;
		aw_dev->is_first_enable = false;
		return 0;
	}

	/*
	 * ENP and ENN enables cannot be pulled high if the
	 * written protect function is not open
	 */
	if (!wpf_enabled(aw_dev))
		write_reg(aw_dev, WPRTEN_REG, WPRTEN_OPEN_VAL);

	aw_dev->is_enabled = true;

	return 0;
}

static int aw37504_disable(struct regulator_dev *rdev)
{
	struct aw_device *aw_dev = rdev_get_drvdata(rdev);

	/* close the written protect function */
	write_reg(aw_dev, WPRTEN_REG, WPRTEN_CLOSE_VAL);

	dev_dbg(&aw_dev->i2c->dev,
		"%s: Setting ENP and ENN GPIOs to 0\n", __func__);

	aw_dev->is_enabled = false;

	return 0;
}

static int aw37504_is_enabled(struct regulator_dev *rdev)
{
	struct aw_device *aw_dev = rdev_get_drvdata(rdev);

	return aw_dev->is_enabled;
}

static int aw37504_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	int rc = 0;
	struct aw_device *aw_dev = rdev_get_drvdata(rdev);

	u8 p_val = get_voltage_reg_val(max_uV);
	u8 n_val = get_voltage_reg_val(min_uV);

	/* write positive voltage to VOUTP */
	rc = write_reg(aw_dev, VOUTP_REG, p_val);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
				"%s: Failed to set the voltage to %d, ret=%d\n",
				__func__, max_uV, rc);
		return rc;
	}

	/* write negative voltage to VOUTN */
	rc = write_reg(aw_dev, VOUTN_REG, n_val);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
				"%s: Failed to set the voltage to -%d, ret=%d\n",
				__func__, min_uV, rc);
		return rc;
	}

	return 0;
}

static int aw37504_get_voltage(struct regulator_dev *rdev)
{
	int rc = 0;
	struct aw_device *aw_dev = rdev_get_drvdata(rdev);
	u8 p_val, n_val;
	int voltage;

	/* read positive voltage to VOUTP */
	rc = read_reg(aw_dev, VOUTP_REG, &p_val);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
				"%s: Failed to get the voltage, ret=%d\n",
				__func__, rc);
		return rc;
	}

	/* read negative voltage to VOUTN */
	rc = read_reg(aw_dev, VOUTN_REG, &n_val);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
				"%s: Failed to get the voltage, ret=%d\n",
				__func__, rc);
		return rc;
	}

	voltage = get_reg_to_voltage_val(p_val);

	if (aw_dev->is_enabled)
		return voltage;
	else
		return 0;
}

static int aw37504_set_load(struct regulator_dev *rdev,
		int uA_load)
{
	int rc = 0;
	struct aw_device *aw_dev = rdev_get_drvdata(rdev);

	if (uA_load < 220000) {
		dev_err(&aw_dev->i2c->dev,
			"%s: The panels cannot operate under 220mA", __func__);
		return -EINVAL;
	}

	/* set the current to >= 220 mA */
	rc = write_reg(aw_dev, APPS_REG, APPS_VAL);
	if (rc < 0) {
		dev_err(&aw_dev->i2c->dev,
			"%s: Failed to set the current load, ret=%d",
			__func__, rc);
		return rc;
	}

	return 0;
}

static unsigned int aw37504_get_optimm_mode(struct regulator_dev *rdev,
		int input_uV, int output_uV, int load_uA)
{
	return 0;
}

static int aw37504_set_mode(struct regulator_dev *rdev,
		unsigned int mode)
{
	return 0;
}

static unsigned int aw37504_get_mode(struct regulator_dev *rdev)
{
	return 0;
}

static struct regulator_ops aw37504_reg_ops = {
	.enable = aw37504_enable,
	.disable = aw37504_disable,
	.is_enabled = aw37504_is_enabled,
	.set_voltage = aw37504_set_voltage,
	.get_voltage = aw37504_get_voltage,
	.set_load = aw37504_set_load,
	.get_optimum_mode = aw37504_get_optimm_mode,
	.set_mode = aw37504_set_mode,
	.get_mode = aw37504_get_mode,
};

static int aw37504_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct aw_device *aw_dev;
	struct regulator_init_data *init_data;
	struct regulator_config reg_cfg = {};

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "%s: No I2C functionality present\n", __func__);
		return -ENODEV;
	}

	aw_dev = devm_kzalloc(&i2c->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (!aw_dev)
		return -ENOMEM;

	aw_dev->regmap = devm_regmap_init_i2c(i2c, &aw37504_regmap);
	if (IS_ERR(aw_dev->regmap)) {
		rc = PTR_ERR(aw_dev->regmap);
		dev_err(&i2c->dev, "%s: Failed to set up AW37504 register map, ret=%d\n",
			__func__, rc);
		return rc;
	}

	i2c_set_clientdata(i2c, aw_dev);

	aw_dev->i2c = i2c;
	parse_dt(&i2c->dev, aw_dev);

	init_data = aw_dev->reg_init_data;
	aw_dev->is_enabled = init_data->constraints.boot_on;
	aw_dev->is_first_enable = true;

	aw_dev->reg_desc.name = "aw37504-bias";
	aw_dev->reg_desc.id = 0;
	aw_dev->reg_desc.type = REGULATOR_VOLTAGE;
	aw_dev->reg_desc.owner = THIS_MODULE;
	aw_dev->reg_desc.ops = &aw37504_reg_ops;

	reg_cfg.dev = &i2c->dev;
	reg_cfg.init_data = init_data;
	init_data->constraints.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_MODE	| REGULATOR_CHANGE_DRMS;
	reg_cfg.driver_data = aw_dev;
	reg_cfg.of_node = i2c->dev.of_node;

	aw_dev->reg_dev = devm_regulator_register(&i2c->dev, &aw_dev->reg_desc,
			&reg_cfg);
	if (IS_ERR(aw_dev->reg_dev)) {
		rc = PTR_ERR(aw_dev->reg_dev);
		dev_err(&i2c->dev, "%s: Failed to register regulator, ret=%d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}

static int aw37504_remove(struct i2c_client *i2c)
{
	return 0;
}

static int aw37504_suspend(struct device *dev)
{
	return 0;
}

static int aw37504_resume(struct device *dev)
{
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = AW37504_NODE_TABLE, },
	{ }
};

static const struct dev_pm_ops aw37504_pm_ops = {
	.suspend = aw37504_suspend,
	.resume  = aw37504_resume,
};

static struct i2c_driver aw37504_driver = {
	.driver = {
		.name = "aw37504-driver",
		.of_match_table = match_table,
		.pm = &aw37504_pm_ops,
	},
	.probe = aw37504_probe,
	.remove = aw37504_remove,
};

module_i2c_driver(aw37504_driver);

MODULE_DESCRIPTION("AW37504 Driver");
MODULE_LICENSE("GPL");

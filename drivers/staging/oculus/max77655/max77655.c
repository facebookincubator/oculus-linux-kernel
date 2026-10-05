// SPDX-License-Identifier: GPL-2.0+
//
// max77655.c  - regulator driver for MAX77655 PMIC (LCOS display)
//
// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Author: Tom McCarthy <tommccarthy@meta.com>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "max77655.h"

static const struct regmap_config max77655_regmap_config = {
	.reg_bits   = 8,
	.val_bits   = 8,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.cache_type = REGCACHE_NONE,
};

static ssize_t max77655_i2c_revision_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct max77655_data *pmic = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", pmic->pmic_rev);
}

static ssize_t max77655_i2c_error_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct max77655_data *pmic = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", pmic->pmic_err);
}

static ssize_t max77655_i2c_dump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 value[16];

	struct max77655_data *pmic = dev_get_drvdata(dev);
	max77655_i2c_read(pmic, 0, &value[0], 16);

	return snprintf(buf, PAGE_SIZE, "0x00: " \
					"%02X %02X %02X %02X " \
					"%02X %02X %02X %02X " \
					"%02X %02X %02X %02X " \
					"%02X %02X %02X %02X\n",
					value[0], value[1], value[2], value[3],
					value[4], value[5], value[6], value[7],
					value[8], value[9], value[10], value[11],
					value[12], value[13], value[14], value[15]);
}

static struct device_attribute attr_max77655_i2c_revision =
		__ATTR(revision, 0444, max77655_i2c_revision_show, NULL);
static struct device_attribute attr_max77655_i2c_error =
		__ATTR(error, 0444, max77655_i2c_error_show, NULL);
static struct device_attribute attr_max77655_i2c_dump =
		__ATTR(dump, 0444, max77655_i2c_dump_show, NULL);

int max77655_i2c_write(struct max77655_data *pmic, u8 reg, void *buffer, unsigned int len)
{
	int ret = 0;
	int retry = 0;
	struct regmap *regmap = pmic->regmap;

	ret = regmap_bulk_write(regmap, (unsigned int)reg, buffer, len);
	while (ret < 0 && retry < MAX77655_MAX_READ_RETRIES) {
		pr_warn("<%s>: regmap_write failed with error %d\n", __func__, ret);
		udelay(I2C_DELAY_US);
		ret = regmap_bulk_write(regmap, (unsigned int)reg, buffer, len);
		retry++;
	}

	if (ret < 0)
		pr_err("<%s>: regmap_write failed with error %d after retries\n", __func__, ret);

	return ret;
}

int max77655_i2c_read(struct max77655_data *pmic, u8 reg, void *buffer, unsigned int len)
{

	int ret = 0;
	int retry = 0;
	struct regmap *regmap = pmic->regmap;

	ret = regmap_bulk_read(regmap, (unsigned int)reg, buffer, len);
	while (ret < 0 && retry < MAX77655_MAX_READ_RETRIES) {
		pr_warn("<%s>: regmap_read failed with error %d\n", __func__, ret);
		udelay(I2C_DELAY_US);
		ret = regmap_bulk_read(regmap, (unsigned int)reg, buffer, len);
		retry++;
	}

	if (ret < 0) {
		pr_err("<%s>: regmap_read failed with error %d after retries\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int max77655_turn_on_pushbutton_mode(struct max77655_data *pmic)
{
	int ret;
	u8 write_reg = 0x00;
	u8 mode_write_buf[1] = {0x38};
	u8 otp_write_buf[8] = {0x70, 0x29, 0x8C, 0x28, 0x28, 0x2B, 0x34, 0x2A};

	ret = max77655_i2c_write(pmic, write_reg, mode_write_buf, 1);
	if (ret < 0)
		goto error;
	write_reg = 0x08;
	ret = max77655_i2c_write(pmic, write_reg, otp_write_buf, 8);
	if (ret < 0)
		goto error;

	return 0;
error:
	dev_err(pmic->dev, "Failed to enable MAX77655 in pushbutton mode %d\n", ret);
	return ret;
}

static int max77655_enable(struct regulator_dev *rdev) {
	int ret = 0;

	struct max77655_data *pmic = rdev_get_drvdata(rdev);

	if (pmic->is_enabled) {
		dev_info(pmic->dev, "MAX77655 already enabled\n");
		return ret;
	}

	if (pmic->is_push_button_mode) {
		gpiod_set_value(pmic->pmic_en_gpio, 1);
		usleep_range(10000, 10000+50);
		ret = max77655_turn_on_pushbutton_mode(pmic);
		if (ret)
			pmic->pmic_err |= 1 << 7;
		usleep_range(10000, 10000+50);
		gpiod_set_value(pmic->pmic_en_gpio, 0);
	} else {
		gpiod_set_value(pmic->pmic_en_gpio, 1);
		usleep_range(10000, 10000+50);
	}

	if (ret == 0) {
		pmic->is_enabled = true;
	}

	return ret;
}

static int max77655_put_in_standy(struct max77655_data *pmic)
{
	int ret = 0;
	u8 write_reg = 0x01;
	u8 stdby_write_buf[1] = {0x03};

	ret = max77655_i2c_write(pmic, write_reg, stdby_write_buf, 1);
	if (ret < 0)
		dev_err(pmic->dev, "Failed to disable MAX77655 %d\n", ret);

	return ret;
}

static int max77655_disable(struct regulator_dev *rdev) {
	int ret = 0;
	u8 pmic_err = 0;

	struct max77655_data *pmic = rdev_get_drvdata(rdev);

	if (!pmic->is_enabled) {
		dev_info(pmic->dev, "MAX77655 already disabled\n");
		return ret;
	}

	max77655_i2c_read(pmic, MAX77655_PMIC_ERR_REG, &pmic_err, 1);
	pmic->pmic_err |= pmic_err;
	if (pmic->is_push_button_mode) {
		ret = max77655_put_in_standy(pmic);
	} else {
		gpiod_set_value(pmic->pmic_en_gpio, 0);
	}
	pmic->is_enabled = false;

	return ret;
}

static int max77655_is_enabled(struct regulator_dev *rdev)
{
	struct max77655_data *pmic = rdev_get_drvdata(rdev);
	return pmic->is_enabled;
}

static const struct regulator_ops max77655_regulator_ops = {
	.is_enabled	   = max77655_is_enabled,
	.enable		   	  = max77655_enable,
	.disable		  = max77655_disable,
};

static const struct regulator_desc max77655_regulator_desc = {
	.name = MAX77655_REGULATOR_NAME,
	.ops = &max77655_regulator_ops,
	.owner = THIS_MODULE,
};

static const struct of_device_id max77655_of_match[] = {
	{ .compatible = MAX77655_DEV_NAME},
	{},
};

static int max77655_probe(struct i2c_client * const i2c, const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct regulator_dev *rdev;
	struct regulator_init_data *init_data;
	struct max77655_data *pmic;
	struct regulator_config config = { };
	bool is_cont_splash_enabled;
	int ret;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "No I2C functionality present\n");
		return -ENODEV;
	}

	is_cont_splash_enabled = of_property_read_bool(i2c->dev.of_node, "continuous-splash");
	pmic = devm_kzalloc(&i2c->dev, sizeof(struct max77655_data), GFP_KERNEL);
	if (pmic == NULL)
		return -ENOMEM;
	pmic->i2c = i2c;
	pmic->dev = dev;
	pmic->adapter = i2c->adapter;
	pmic->addr = i2c->addr;
	pmic->is_enabled = is_cont_splash_enabled; // If continuous splash is enabled, PMIC will be turned on by UEFI
	// DO not set the output for pmic_en_gpio. max77655 (old pmic) and max77675 (new pmic) are using
	// different pmic_en_gpio values and they have been set up properly at UEFI. Changing the values
	// here would break the power supply of display
	pmic->pmic_en_gpio = devm_gpiod_get_index(dev, "pmic", 0, GPIOD_ASIS);
	if (IS_ERR(pmic->pmic_en_gpio)) {
		ret = PTR_ERR(pmic->pmic_en_gpio);
		dev_err(dev, "devm_gpiod_get_index 'pmic' failed with ret: %d\n", ret);
		return ret;
	}
	// If pmic_en_gpio is low, PMIC is initialized in push button mode by UEFI.
	pmic->is_push_button_mode = !gpiod_get_value(pmic->pmic_en_gpio);

	pmic->regmap = devm_regmap_init_i2c(i2c, &max77655_regmap_config);
	if (IS_ERR(pmic->regmap)) {
		ret = PTR_ERR(pmic->regmap);
		pr_err("<%s>: Error %d in initializing regmap\n", __func__, ret);
		return ret;
	}

	i2c_set_clientdata(i2c, pmic);

	max77655_i2c_read(pmic, MAX77655_PMIC_REV_REG, &pmic->pmic_rev, 1);

	if (device_create_file(dev, &attr_max77655_i2c_revision)) {
		dev_err(dev, "device_create_file revision error\n");
	};
	if (device_create_file(dev, &attr_max77655_i2c_error)) {
		dev_err(dev, "device_create_file error\n");
	};
	if (device_create_file(dev, &attr_max77655_i2c_dump)) {
		dev_err(dev, "device_create_file dump error\n");
	};

	config.dev = dev;
	init_data = of_get_regulator_init_data(dev, dev->of_node, &max77655_regulator_desc);
	config.init_data = init_data;
	init_data->constraints.keep_on = 1;
	init_data->constraints.valid_ops_mask = REGULATOR_CHANGE_STATUS;
	config.regmap = pmic->regmap;
	config.driver_data = pmic;
	rdev = devm_regulator_register(dev,
						&max77655_regulator_desc,
						&config);
	if (IS_ERR(rdev)) {
		dev_err(dev, "Failed to register regulator MAX77655.\n");
	}

	ret = devm_regulator_debug_register(dev, rdev);
	if (ret)
		dev_err(dev, "Failed to register debug regulator, rc=%d\n", ret);

	return 0;
}

static int max77655_remove(struct i2c_client *i2c) {
	struct max77655_data *pmic = i2c_get_clientdata(i2c);
	struct device *dev = pmic->dev;
	if (pmic == NULL)
		return -ENODEV;
	device_remove_file(dev, &attr_max77655_i2c_revision);
	device_remove_file(dev, &attr_max77655_i2c_error);
	device_remove_file(dev, &attr_max77655_i2c_dump);
	i2c_set_clientdata(i2c, NULL);
	devm_kfree(dev, pmic);
	return 0;
}

static const struct i2c_device_id max77655_id[] = {
	{ "max77655", },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77655_id);

static struct i2c_driver max77655_driver = {
	.driver = {
		.name = "max77655-driver",
		.of_match_table = max77655_of_match,
	},
	.probe = max77655_probe,
	.remove = max77655_remove,
	.id_table = max77655_id,
};
module_i2c_driver(max77655_driver);

MODULE_AUTHOR("Tom McCarthy <tommccarthy@meta.com>");
MODULE_DESCRIPTION("MAX77655 PMIC regulator driver");
MODULE_LICENSE("GPL");

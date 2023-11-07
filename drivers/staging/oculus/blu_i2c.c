// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/version.h>

#define PWM1_REG 0x00
#define PWM0_REG 0x01
#define CURR_LIM_REG 0x02
#define EN_LED_REG 0x03
#define OVP_REG 0x04
#define BRIGHTNESS_REG 0x05
#define ILED0_REG 0x06
#define ILED1_REG 0x07
#define FUNC_EN_REG 0x08
#define FILTER_REG 0x09
#define FREQ_REG 0x0A
#define DIMMING_REG 0x0B
#define ILED_LO_REG 0x0C
#define ILED_HI_REG 0x0D
#define VOUT_REG 0x0E
#define HEAD_ROOM_REG 0x0F
#define HYST_REG 0x10
#define JUMP_REG 0x11
#define ID_REG 0x1D
#define FAULT_REG 0x1F

static bool mp3314_readable(struct device *dev, unsigned int reg)
{
	return (reg >= PWM1_REG && reg <= JUMP_REG) ||
		(reg >= ID_REG && reg <= FAULT_REG);
}

static bool mp3314_volatile(struct device *dev, unsigned int reg)
{
	return reg == FAULT_REG;
}

static bool mp3314_writeable(struct device *dev, unsigned int reg)
{
	return reg >= PWM1_REG && reg <= JUMP_REG;
}

struct match_data {
	int size;                   // number of entries in regs
	struct reg_sequence regs[];
};

static const struct match_data mp3314_data = {
	.size = 20,
	.regs = {{ PWM1_REG, 0xFF },
		{ PWM0_REG, 0xFF },
		{ CURR_LIM_REG, 0x45 },
		{ EN_LED_REG, 0x43 },
		{ OVP_REG, 0x68 },
		{ BRIGHTNESS_REG, 0x38 },
		{ ILED0_REG, 0x66 },
		{ ILED1_REG, 0x7E },
		{ FUNC_EN_REG, 0xA0 },
		{ FILTER_REG, 0x82 },
		{ FREQ_REG, 0x10 },
		{ DIMMING_REG, 0x66 },
		{ ILED_LO_REG, 0x00 },
		{ ILED_HI_REG, 0x00 },
		{ VOUT_REG, 0x67 },
		{ HEAD_ROOM_REG, 0x2f },
		{ HYST_REG, 0xC0 },
		{ JUMP_REG, 0x00 },
		{ ID_REG, 0x11 },
		{ FAULT_REG, 0x00 }}
};

static const struct match_data mp3314a_data = {
	.size = 20,
	.regs = {{ PWM1_REG, 0xFF },
		{ PWM0_REG, 0xFF },
		{ CURR_LIM_REG, 0x55 },
		{ EN_LED_REG, 0x73 },
		{ OVP_REG, 0x68 },
		{ BRIGHTNESS_REG, 0x38 },
		{ ILED0_REG, 0x66 },
		{ ILED1_REG, 0x7E },
		{ FUNC_EN_REG, 0xA0 },
		{ FILTER_REG, 0x82 },
		{ FREQ_REG, 0x90 },
		{ DIMMING_REG, 0x66 },
		{ ILED_LO_REG, 0x00 },
		{ ILED_HI_REG, 0x00 },
		{ VOUT_REG, 0x6e },
		{ HEAD_ROOM_REG, 0x2f },
		{ HYST_REG, 0xC0 },
		{ JUMP_REG, 0x00 },
		{ ID_REG, 0x11 },
		{ FAULT_REG, 0x00 }}
};

static const struct regmap_config mp3314_regmap = {
	.name = "mp3314",
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = FAULT_REG,
	.readable_reg = mp3314_readable,
	.volatile_reg = mp3314_volatile,
	.writeable_reg = mp3314_writeable,

	.cache_type = REGCACHE_FLAT,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	.use_single_read = true,
	.use_single_write = true,
#else
	.use_single_rw = true,
#endif
};

struct blu_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	const struct match_data *match_data;

	/* regulator values */
	struct regulator_desc reg_desc;
	struct regulator_dev *reg_dev;
	struct regulator_init_data *reg_init_data;
};

static int parse_dt(struct device *dev, struct blu_priv *bld)
{
	struct regulator_init_data *init_data;

	if (!dev->of_node) {
		dev_err(dev, "%s: No device tree found\n", __func__);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(dev, dev->of_node, &bld->reg_desc);
	if (!init_data)
		return -ENOMEM;

	if (init_data->constraints.min_uV != init_data->constraints.max_uV) {
		dev_err(dev,
			"%s: Fixed regulator specified with variable voltages\n",
			__func__);
		return -EINVAL;
	}

	bld->reg_init_data = init_data;

	return 0;
}

static int mp3314_enable(struct regulator_dev *rdev)
{
	struct blu_priv *bld = rdev_get_drvdata(rdev);

	regmap_register_patch(bld->regmap, bld->match_data->regs,
			    bld->match_data->size);

	return 0;
}

static int mp3314_disable(struct regulator_dev *rdev)
{
	struct blu_priv *bld = rdev_get_drvdata(rdev);
	int rc;

	rc = regmap_write(bld->regmap, CURR_LIM_REG, 0x44);
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
				"%s: Failed to write 0x44 to reg ret=%d\n",
				__func__, rc);
		return rc;
	}

	return 0;
}

static int mp3314_is_enabled(struct regulator_dev *rdev)
{
	struct blu_priv *bld = rdev_get_drvdata(rdev);
	int rc;
	unsigned int reg_val = 0;

	rc = regmap_read(bld->regmap, CURR_LIM_REG, &reg_val);
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
				"%s: Failed to read 0x44 to reg ret=%d\n",
				__func__, rc);
		return rc;
	}

	return (reg_val & BIT(0));
}

static int mp3314_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	return 0;
}

static int mp3314_get_voltage(struct regulator_dev *rdev)
{
	return 0;
}

static int mp3314_set_load(struct regulator_dev *rdev,
		int uA_load)
{
	return 0;
}

static unsigned int mp3314_get_optimm_mode(struct regulator_dev *rdev,
		int input_uV, int output_uV, int load_uA)
{
	return 0;
}

static int mp3314_set_mode(struct regulator_dev *rdev,
		unsigned int mode)
{
	return 0;
}

static unsigned int mp3314_get_mode(struct regulator_dev *rdev)
{
	return 0;
}


static struct regulator_ops mp3314_reg_ops = {
	.enable = mp3314_enable,
	.disable = mp3314_disable,
	.is_enabled = mp3314_is_enabled,
	.set_voltage = mp3314_set_voltage,
	.get_voltage = mp3314_get_voltage,
	.set_load = mp3314_set_load,
	.get_optimum_mode = mp3314_get_optimm_mode,
	.set_mode = mp3314_set_mode,
	.get_mode = mp3314_get_mode,
};

static int blu_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	int rc = 0;
	struct blu_priv *bld;
	struct regulator_init_data *init_data;
	struct regulator_config reg_cfg = {};

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "No I2C functionality present\n");
		return -ENODEV;
	}

	bld = devm_kzalloc(&i2c->dev, sizeof(struct blu_priv), GFP_KERNEL);
	if (bld == NULL)
		return -ENOMEM;

	bld->i2c = i2c;
	i2c_set_clientdata(i2c, bld);

	bld->regmap = devm_regmap_init_i2c(i2c, &mp3314_regmap);
	if (IS_ERR(bld->regmap)) {
		dev_err(&i2c->dev, "Failed to set up MP3314 register map\n");
		rc = PTR_ERR(bld->regmap);
	}

	bld->match_data = device_get_match_data(&i2c->dev);
	regmap_register_patch(bld->regmap, bld->match_data->regs,
			bld->match_data->size);

	parse_dt(&i2c->dev, bld);

	init_data = bld->reg_init_data;

	bld->reg_desc.name = i2c->dev.of_node->name;
	bld->reg_desc.id = 0;
	bld->reg_desc.type = REGULATOR_VOLTAGE;
	bld->reg_desc.owner = THIS_MODULE;
	bld->reg_desc.ops = &mp3314_reg_ops;

	reg_cfg.dev = &i2c->dev;
	reg_cfg.init_data = init_data;
	init_data->constraints.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_MODE	| REGULATOR_CHANGE_DRMS;
	reg_cfg.driver_data = bld;
	reg_cfg.of_node = i2c->dev.of_node;

	bld->reg_dev = devm_regulator_register(&i2c->dev, &bld->reg_desc,
			&reg_cfg);
	if (IS_ERR(bld->reg_dev)) {
		rc = PTR_ERR(bld->reg_dev);
		dev_err(&i2c->dev, "%s: Failed to register regulator, ret=%d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}

static int blu_remove(struct i2c_client *i2c)
{
	dev_dbg(&i2c->dev, "mp3314 remove\n");
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "oculus,blu-i2c",
		.data = &mp3314_data},
	{ .compatible = "meta,mp3314",
		.data = &mp3314_data},
	{ .compatible = "meta,mp3314a",
		.data = &mp3314a_data},
	{ }
};

static struct i2c_driver blu_driver = {
	.driver = {
		.name = "blu-i2c-driver",
		.of_match_table = match_table,
	},
	.probe = blu_probe,
	.remove = blu_remove,
};

module_i2c_driver(blu_driver);

MODULE_DESCRIPTION("BLU I2C driver");
MODULE_LICENSE("GPL");

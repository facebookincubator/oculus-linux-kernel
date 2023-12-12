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

#define ILED_FULL_SCALE 0xFFF
#define NUM_IMAX_VALUES 8
#define NUM_REGISTERS 20
#define MAX_REGISTER FAULT_REG
#define MP3314_SET_EN(_reg_val, _en) {					\
												_reg_val &= ~0x1;				\
												_reg_val |= (_en & 0x1);\
										}
#define ILED_SET0_VAL(_iled) (_iled & 0xff)
#define ILED_SET1_VAL(_imax, _iled) (((_imax & 0x7) << 4) | ((_iled & 0xfff) >> 8))
#define ISET_EN(_val) (_val & 0x4)

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
	unsigned int iset_resistor_max;
};

/* MP3314  - 60K, MP3314A - 40K */
static const struct match_data mp3314_data = {
	.iset_resistor_max = 60000,
};

static const struct match_data mp3314a_data = {
	.iset_resistor_max = 40000,
};

/* IMAX Values(uA), ILED_SET_1[6:4] */
static const unsigned int imax_values[NUM_IMAX_VALUES] = {
	5000,
	10000,
	15000,
	20000,
	23000,
	25000,
	30000,
	50000,
};

struct blu_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	const struct match_data *match_data;

	/* regulator values */
	struct regulator_desc reg_desc;
	struct regulator_dev *reg_dev;
	struct regulator_init_data *reg_init_data;

	/* External resistor value */
	unsigned int iset_ext_resistor;
	/* IMAX values based on external resistor when ISET is enabled */
	unsigned int imax_ua[NUM_IMAX_VALUES];
};

static void init_regmap_config(struct regmap_config *regmap_config)
{
	memset(regmap_config, 0, sizeof(*regmap_config));
	regmap_config->name = "mp3314";
	regmap_config->reg_bits = 8;
	regmap_config->val_bits = 8;
	regmap_config->reg_stride = 1;
	regmap_config->max_register = MAX_REGISTER;
	regmap_config->readable_reg = mp3314_readable;
	regmap_config->volatile_reg = mp3314_volatile;
	regmap_config->writeable_reg = mp3314_writeable;
	regmap_config->cache_type = REGCACHE_FLAT,
	regmap_config->use_single_read = true;
	regmap_config->use_single_write = true;
}

static int parse_dt(struct device *dev, struct blu_priv *bld, struct regmap_config *regmap_config)
{
	struct regulator_init_data *init_data;
	struct reg_default *reg_defaults = NULL;
	unsigned char default_reg_values[NUM_REGISTERS * 2];
	int i, num_defaults;

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

	if (of_property_read_u32(dev->of_node, "iset-ext-resistor", &bld->iset_ext_resistor) < 0) {
		dev_err(dev,
			"%s: Can not find iset-ext-resistor value\n",
			__func__);
		return -EINVAL;
	}

	num_defaults = of_property_read_variable_u8_array(dev->of_node, "regs-defaults", default_reg_values, 2, NUM_REGISTERS * 2);

	if (num_defaults > 0) {
		/* Lets set the regmap defaults with the provided register values */
		num_defaults /= 2;
		regmap_config->num_reg_defaults = num_defaults;
		reg_defaults = (struct reg_default *)regmap_config->reg_defaults;
		for (i = 0; i < num_defaults; ++i) {
			reg_defaults[i].reg = default_reg_values[i * 2];
			reg_defaults[i].def = default_reg_values[i * 2 + 1];
		}
	} else {
		/* regcache will be initialized from reading device registers */
		regmap_config->num_reg_defaults_raw = MAX_REGISTER + 1;
		regmap_config->reg_defaults = NULL;
	}

	return 0;
}

static int mp3314_enable(struct regulator_dev *rdev)
{
	struct blu_priv *bld = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int rc;

	regmap_read(bld->regmap, CURR_LIM_REG, &reg_val);
	MP3314_SET_EN(reg_val, 1);
	rc = regmap_write(bld->regmap, CURR_LIM_REG, reg_val);
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write reg 0x%x ret=%d\n",
			__func__, CURR_LIM_REG, rc);
	}

	return rc;
}

static int mp3314_disable(struct regulator_dev *rdev)
{
	struct blu_priv *bld = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int rc;

	regmap_read(bld->regmap, CURR_LIM_REG, &reg_val);
	MP3314_SET_EN(reg_val, 0);
	rc = regmap_write(bld->regmap, CURR_LIM_REG, reg_val);
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write reg 0x%x ret=%d\n",
			__func__, CURR_LIM_REG, rc);
	}

	return rc;
}

static int mp3314_is_enabled(struct regulator_dev *rdev)
{
	struct blu_priv *bld = rdev_get_drvdata(rdev);
	unsigned int reg_val = 0;

	regmap_read(bld->regmap, CURR_LIM_REG, &reg_val);

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
	struct blu_priv *bld = rdev_get_drvdata(rdev);
	int i, rc, imax_reg;
	unsigned int reg_val, imax, target_iled;

	/* Check for if ISET_EN is set */
	regmap_read(bld->regmap, FUNC_EN_REG, &reg_val);

	if (ISET_EN(reg_val)) {
		/* Find imax reg setting for given load */
		for (i = 0; i < NUM_IMAX_VALUES; ++i) {
			if (uA_load < bld->imax_ua[i])
				break;
		}

		if (i == NUM_IMAX_VALUES) {
			dev_err(&bld->i2c->dev, "%s: target current %d uA exceeds the device capability\n",
				__func__, uA_load);
			return -1;
		}

		imax_reg = i;
		imax = bld->imax_ua[imax_reg];
	} else {
		imax_reg = NUM_IMAX_VALUES - 1;
		imax = imax_values[imax_reg];
	}

	target_iled = ILED_FULL_SCALE * uA_load / imax;

	rc = regmap_write(bld->regmap, ILED0_REG, ILED_SET0_VAL(target_iled));
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to ILED0_REG ret=%d\n",
			__func__, rc);
	}

	rc |= regmap_write(bld->regmap, ILED1_REG, ILED_SET1_VAL(imax_reg, target_iled));
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to ILED1_REG ret=%d\n",
			__func__, rc);
	}

	return rc;
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
	int i, rc = 0;
	struct blu_priv *bld;
	struct regulator_init_data *init_data;
	struct regulator_config reg_cfg = {};
	struct regmap_config regmap_config;
	struct reg_default reg_defaults[NUM_REGISTERS];

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "No I2C functionality present\n");
		return -ENODEV;
	}

	bld = devm_kzalloc(&i2c->dev, sizeof(struct blu_priv), GFP_KERNEL);
	if (bld == NULL)
		return -ENOMEM;

	bld->i2c = i2c;
	i2c_set_clientdata(i2c, bld);

	bld->match_data = device_get_match_data(&i2c->dev);

	init_regmap_config(&regmap_config);
	regmap_config.reg_defaults = reg_defaults;

	rc = parse_dt(&i2c->dev, bld, &regmap_config);
	if (rc < 0)
		return rc;

	bld->regmap = devm_regmap_init_i2c(i2c, &regmap_config);
	if (IS_ERR(bld->regmap)) {
		dev_err(&i2c->dev, "Failed to set up MP3314 register map\n");
		rc = PTR_ERR(bld->regmap);
	}

	for (i = 0; i < NUM_IMAX_VALUES; ++i)
		bld->imax_ua[i] = imax_values[i] * bld->match_data->iset_resistor_max / bld->iset_ext_resistor;

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

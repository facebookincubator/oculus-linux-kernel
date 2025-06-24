// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/version.h>

#define MODE_CTRL_CURR_LIM_REG 0x00
#define CHEN_REG 0x01
#define OVP_IMAX_REG 0x02
#define TPWM_REG 0x03
#define ILEDL_L_REG 0x04
#define ILEDL_H_REG 0x05
#define ILEDR_L_REG 0x06
#define ILEDR_H_REG 0x07
#define VOUT_REG 0x08
#define FREQ_SW_REG 0x09
#define HEADROOM_VSTEP_REG 0x0A
#define FAULT0_REG 0x0B
#define FAULT1_REG 0x0C
#define ID_REG 0x1D

#define ILED_FULL_SCALE 0xFFF
#define NUM_IMAX_VALUES 8
#define NUM_REGISTERS 14
#define MIN_REGISTER MODE_CTRL_CURR_LIM_REG
#define MAX_REGISTER ID_REG
#define MAX_WRITEABLE_REGISTER HEADROOM_VSTEP_REG

#define MP3317_SET_EN(_reg_val, _en)            \
	{                                       \
		_reg_val &= ~0x80;              \
		_reg_val |= ((_en & 0x1) << 7); \
	}
#define ILED_SETL_VAL(_iled) (_iled & 0xff)
#define ILED_SETH_VAL(_iled) ((_iled & 0xfff) >> 8)
#define IMAX_BITMASK 0x7
#define ISET_EN(_val) (_val & 0x40)

#define EN_GPIO_DELAY_US (2000)

static bool mp3317_readable(struct device *dev, unsigned int reg)
{
	return reg >= MIN_REGISTER && reg <= MAX_REGISTER;
}

static bool mp3317_volatile(struct device *dev, unsigned int reg)
{
	return reg == FAULT0_REG || reg == FAULT1_REG;
}

static bool mp3317_writeable(struct device *dev, unsigned int reg)
{
	return reg >= MIN_REGISTER && reg <= MAX_WRITEABLE_REGISTER;
}

static const struct reg_default mp3317_reg_defs[] = {
	{ MODE_CTRL_CURR_LIM_REG, 0x95 },
	{ CHEN_REG, 0xFF },
	{ OVP_IMAX_REG, 0x7B },
	{ TPWM_REG, 0x08 },
	{ ILEDL_L_REG, 0xFF },
	{ ILEDL_H_REG, 0x0F },
	{ ILEDR_L_REG, 0xFF },
	{ ILEDR_H_REG, 0x0F },
	{ VOUT_REG, 0x68 },
	{ FREQ_SW_REG, 0xF3 },
	{ HEADROOM_VSTEP_REG, 0x01 },
	{ FAULT0_REG, 0x00 },
	{ FAULT1_REG, 0x00 },
	{ ID_REG, 0x11 },
};

struct match_data {
	unsigned int iset_resistor_max;
};

/* MP3317  - 60K */
static const struct match_data mp3317_data = {
	.iset_resistor_max = 60000,
};

/* IMAX Values(uA) */
static const unsigned int imax_values[NUM_IMAX_VALUES] = {
	5000, 10000, 20000, 40000, 80000, 160000, 160000, 160000,
};

struct mp3317_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	const struct match_data *match_data;

	/* regulator values */
	struct regulator_desc reg_desc;
	struct regulator_dev *reg_dev;
	struct regulator_init_data *reg_init_data;

	/* Enable GPIO */
	int en_gpio;

	/* External resistor value */
	unsigned int iset_ext_resistor;
	/* IMAX values based on external resistor when ISET is enabled */
	unsigned int imax_ua[NUM_IMAX_VALUES];
};

static void init_regmap_config(struct regmap_config *regmap_config)
{
	memset(regmap_config, 0, sizeof(*regmap_config));
	regmap_config->name = "mp3317";
	regmap_config->reg_bits = 8;
	regmap_config->val_bits = 8;
	regmap_config->reg_stride = 1;
	regmap_config->max_register = MAX_REGISTER;
	regmap_config->readable_reg = mp3317_readable;
	regmap_config->volatile_reg = mp3317_volatile;
	regmap_config->writeable_reg = mp3317_writeable;
	regmap_config->cache_type = REGCACHE_FLAT;
	regmap_config->use_single_read = true;
	regmap_config->use_single_write = true;
	regmap_config->reg_defaults = mp3317_reg_defs;
	regmap_config->num_reg_defaults = ARRAY_SIZE(mp3317_reg_defs);
}

static int parse_dt(struct device *dev, struct mp3317_priv *bld,
		    struct regmap_config *regmap_config,
		    struct reg_default *initial_regs,
		    unsigned int *num_initial_regs)
{
	unsigned char default_reg_values[NUM_REGISTERS * 2];
	int i, num_defaults;

	if (!dev->of_node) {
		dev_err(dev, "%s: No device tree found\n", __func__);
		return -EINVAL;
	}

	bld->reg_init_data =
		of_get_regulator_init_data(dev, dev->of_node, &bld->reg_desc);
	if (!bld->reg_init_data)
		return -ENOMEM;

	if (bld->reg_init_data->constraints.min_uV !=
	    bld->reg_init_data->constraints.max_uV) {
		dev_err(dev,
			"%s: Fixed regulator specified with variable voltages\n",
			__func__);
		return -EINVAL;
	}

	bld->en_gpio = of_get_named_gpio(dev->of_node, "enable-gpio", 0);
	if (gpio_is_valid(bld->en_gpio)) {
		if (devm_gpio_request(dev, bld->en_gpio, "mp3317-enable-gpio"))
			dev_err(dev, "%s: Failed to request enable GPIO\n",
				__func__);
	} else
		dev_warn(dev, "%s: No enable GPIO defined\n", __func__);

	if (of_property_read_u32(dev->of_node, "iset-ext-resistor",
				 &bld->iset_ext_resistor) < 0) {
		dev_err(dev, "%s: Can not find iset-ext-resistor value\n",
			__func__);
		return -EINVAL;
	}

	*num_initial_regs = 0;
	num_defaults = of_property_read_variable_u8_array(dev->of_node,
							  "regs-defaults",
							  default_reg_values, 2,
							  NUM_REGISTERS * 2);
	if (num_defaults > 0) {
		num_defaults /= 2;
		for (i = 0; i < num_defaults; ++i) {
			initial_regs[i].reg = default_reg_values[i * 2];
			initial_regs[i].def = default_reg_values[i * 2 + 1];
		}
		*num_initial_regs = num_defaults;
	}

	return 0;
}

static void mp3317_read_initial_state(struct mp3317_priv *bld)
{
	unsigned int initial_state[MAX_REGISTER + 1];
	unsigned int reg, reg_val;

	for (reg = 0; reg <= MAX_REGISTER; reg++) {
		if (!mp3317_readable(NULL, reg) || mp3317_volatile(NULL, reg))
			continue;

		/*
		 * Bypass cached defaults in case the bootloader configured
		 * the chip before the kernel and read in the current values
		 * directly from the hardware instead.
		 */
		regcache_cache_bypass(bld->regmap, true);
		regmap_read(bld->regmap, reg, &reg_val);
		initial_state[reg] = reg_val;
		regcache_cache_bypass(bld->regmap, false);
	}

	/*
	 * Write back initial hardware state into cache only since the hardware
	 * already has these values, there is no need to write through.
	 */
	regcache_cache_only(bld->regmap, true);
	for (reg = 0; reg <= MAX_REGISTER; reg++) {
		if (!mp3317_writeable(NULL, reg) || mp3317_volatile(NULL, reg))
			continue;

		regmap_write(bld->regmap, reg, initial_state[reg]);
	}
	regcache_cache_only(bld->regmap, false);
}

static void mp3317_set_initial_state_from_dt(struct mp3317_priv *bld,
					     struct reg_default *initial_regs,
					     unsigned int num_initial_regs)
{
	int i;

	/*
	 * Write initial state to cache only, since we are not enabled at
	 * time of probe. Register state will be synchronized when the
	 * regulator is enabled.
	 */
	for (i = 0; i < num_initial_regs; i++) {
		regcache_cache_only(bld->regmap, true);
		regmap_write(bld->regmap, initial_regs[i].reg,
			     initial_regs[i].def);
		regcache_cache_only(bld->regmap, false);
	}
}

static int mp3317_enable(struct regulator_dev *rdev)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int rc;

	if (gpio_is_valid(bld->en_gpio)) {
		gpio_direction_output(bld->en_gpio, 1);
		usleep_range(EN_GPIO_DELAY_US, EN_GPIO_DELAY_US + 1000);
	}

	/* Take cache out of cache-only mode since HW is now on */
	regcache_cache_only(bld->regmap, false);
	rc = regcache_sync(bld->regmap);
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to sync register state, ret=%d\n", __func__,
			rc);
		return rc;
	}

	regmap_read(bld->regmap, CHEN_REG, &reg_val);
	MP3317_SET_EN(reg_val, 1);
	rc = regmap_write(bld->regmap, CHEN_REG, reg_val);
	if (rc < 0) {
		dev_err(&bld->i2c->dev, "%s: Failed to write reg 0x%x ret=%d\n",
			__func__, CHEN_REG, rc);
	}

	return rc;
}

static int mp3317_disable(struct regulator_dev *rdev)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int rc;

	regmap_read(bld->regmap, CHEN_REG, &reg_val);
	MP3317_SET_EN(reg_val, 0);
	rc = regmap_write(bld->regmap, CHEN_REG, reg_val);
	if (rc < 0) {
		dev_err(&bld->i2c->dev, "%s: Failed to write reg 0x%x ret=%d\n",
			__func__, CHEN_REG, rc);
	}

	/* Put cache into cache-only mode since HW will be off */
	regcache_cache_only(bld->regmap, true);
	regcache_mark_dirty(bld->regmap);

	if (gpio_is_valid(bld->en_gpio))
		gpio_direction_output(bld->en_gpio, 0);

	return rc;
}

static int mp3317_is_enabled(struct regulator_dev *rdev)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	unsigned int reg_val = 0;

	regmap_read(bld->regmap, CHEN_REG, &reg_val);

	return (reg_val & BIT(7));
}

static int mp3317_set_voltage(struct regulator_dev *rdev, int min_uV,
			      int max_uV, unsigned int *selector)
{
	return 0;
}

static int mp3317_get_voltage(struct regulator_dev *rdev)
{
	return 0;
}

static int mp3317_set_load(struct regulator_dev *rdev, int uA_load)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	int i, rc, imax_reg;
	unsigned int reg_val, imax, target_iled;

	/* Check for if ISET_EN is set */
	regmap_read(bld->regmap, CHEN_REG, &reg_val);

	if (ISET_EN(reg_val)) {
		/* Find imax reg setting for given load */
		for (i = 0; i < NUM_IMAX_VALUES; ++i) {
			if (uA_load < bld->imax_ua[i])
				break;
		}

		if (i == NUM_IMAX_VALUES) {
			dev_err(&bld->i2c->dev,
				"%s: target current %d uA exceeds the device capability\n",
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

	rc = regmap_update_bits(bld->regmap, OVP_IMAX_REG, IMAX_BITMASK,
				imax_reg);
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to OVP_IMAX_REG ret=%d\n",
			__func__, rc);
	}

	rc = regmap_write(bld->regmap, ILEDL_L_REG, ILED_SETL_VAL(target_iled));
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to ILEDL_L_REG ret=%d\n",
			__func__, rc);
	}

	rc |= regmap_write(bld->regmap, ILEDL_H_REG,
			   ILED_SETH_VAL(target_iled));
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to ILEDL_H_REG ret=%d\n",
			__func__, rc);
	}

	rc = regmap_write(bld->regmap, ILEDR_L_REG, ILED_SETL_VAL(target_iled));
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to ILEDR_L_REG ret=%d\n",
			__func__, rc);
	}

	rc |= regmap_write(bld->regmap, ILEDR_H_REG,
			   ILED_SETH_VAL(target_iled));
	if (rc < 0) {
		dev_err(&bld->i2c->dev,
			"%s: Failed to write current to ILEDR_H_REG ret=%d\n",
			__func__, rc);
	}

	return rc;
}

static unsigned int mp3317_get_optimum_mode(struct regulator_dev *rdev,
					    int input_uV, int output_uV,
					    int load_uA)
{
	return 0;
}

static int mp3317_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	return 0;
}

static unsigned int mp3317_get_mode(struct regulator_dev *rdev)
{
	return 0;
}

static struct regulator_ops mp3317_reg_ops = {
	.enable = mp3317_enable,
	.disable = mp3317_disable,
	.is_enabled = mp3317_is_enabled,
	.set_voltage = mp3317_set_voltage,
	.get_voltage = mp3317_get_voltage,
	.set_load = mp3317_set_load,
	.get_optimum_mode = mp3317_get_optimum_mode,
	.set_mode = mp3317_set_mode,
	.get_mode = mp3317_get_mode,
};

static int mp3317_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int i, rc = 0;
	struct mp3317_priv *bld;
	struct regulator_config reg_cfg = {};
	struct regmap_config regmap_config;
	struct reg_default initial_regs[NUM_REGISTERS];
	unsigned int num_initial_regs = 0;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "No I2C functionality present\n");
		return -ENODEV;
	}

	bld = devm_kzalloc(&i2c->dev, sizeof(struct mp3317_priv), GFP_KERNEL);
	if (bld == NULL)
		return -ENOMEM;

	bld->i2c = i2c;
	i2c_set_clientdata(i2c, bld);

	bld->match_data = device_get_match_data(&i2c->dev);

	init_regmap_config(&regmap_config);

	rc = parse_dt(&i2c->dev, bld, &regmap_config, initial_regs,
		      &num_initial_regs);
	if (rc < 0)
		return rc;

	bld->regmap = devm_regmap_init_i2c(i2c, &regmap_config);
	if (IS_ERR(bld->regmap)) {
		dev_err(&i2c->dev, "Failed to set up MP3317 register map\n");
		return PTR_ERR(bld->regmap);
	}

	/*
	 * Force a re-read of hardware state into the cache in case something
	 * else has configured the chip before the kernel starts.
	 * Only allow configuring initial state from the device tree if the
	 * hardware is not configured before the kernel starts.
	 */
	if (bld->reg_init_data->constraints.boot_on)
		mp3317_read_initial_state(bld);
	else
		mp3317_set_initial_state_from_dt(bld, initial_regs,
						 num_initial_regs);

	for (i = 0; i < NUM_IMAX_VALUES; ++i)
		bld->imax_ua[i] = imax_values[i] *
				  bld->match_data->iset_resistor_max /
				  bld->iset_ext_resistor;

	bld->reg_desc.name = i2c->dev.of_node->name;
	bld->reg_desc.id = 0;
	bld->reg_desc.type = REGULATOR_VOLTAGE;
	bld->reg_desc.owner = THIS_MODULE;
	bld->reg_desc.ops = &mp3317_reg_ops;

	reg_cfg.dev = &i2c->dev;
	reg_cfg.init_data = bld->reg_init_data;
	reg_cfg.driver_data = bld;
	reg_cfg.of_node = i2c->dev.of_node;

	bld->reg_dev =
		devm_regulator_register(&i2c->dev, &bld->reg_desc, &reg_cfg);
	if (IS_ERR(bld->reg_dev)) {
		rc = PTR_ERR(bld->reg_dev);
		dev_err(&i2c->dev, "%s: Failed to register regulator, ret=%d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "meta,mp3317", .data = &mp3317_data },
	{}
};

static struct i2c_driver mp3317 = {
	.driver = {
		.name = "mp3317",
		.of_match_table = match_table,
	},
	.probe = mp3317_probe,
};

module_i2c_driver(mp3317);

MODULE_DESCRIPTION("MP3317 I2C driver");
MODULE_LICENSE("GPL");

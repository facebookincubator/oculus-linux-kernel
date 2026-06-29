// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define PM3003A_REG_VSELL	0x00
#define PM3003A_REG_VSELH	0x01
#define PM3003A_REG_CNTL	0x02
#define PM3003A_REG_DEVID1	0x03
#define PM3003A_REG_DEVID2	0x04
#define PM3003A_REG_MONITOR	0x05
#define PM3003A_NUM_REGS	(PM3003A_REG_MONITOR + 1)

#define PM3003A_VID_MASK	GENMASK(7, 0)
#define PM3003A_VSEL_MASK	GENMASK(6, 0)

#define PM3003A_VENDOR_ID	0xA8
#define PM3003A_VOUT_MINUV	600000
#define PM3003A_VOUT_MAXUV	1394000
#define PM3003A_VOUT_STPUV	6250
#define PM3003A_N_VOUTS 	((PM3003A_VOUT_MAXUV - PM3003A_VOUT_MINUV) / PM3003A_VOUT_STPUV + 1)

#define PM3003A_I2CRDY_TIMEUS	100

struct pm3003a_priv {
	struct regulator_desc desc;
	struct gpio_desc *enable_gpio;
	struct regmap *regmap;
	bool enable_state;
	bool gpio_control_enabled;
};

static int pm3003a_enable(struct regulator_dev *rdev)
{
	struct pm3003a_priv *priv = rdev_get_drvdata(rdev);

	if (priv->gpio_control_enabled) {
		if (!priv->enable_gpio)
			return 0;

		gpiod_set_value_cansleep(priv->enable_gpio, 1);
	}
	priv->enable_state = true;

	usleep_range(PM3003A_I2CRDY_TIMEUS, PM3003A_I2CRDY_TIMEUS + 100);

	regcache_cache_only(priv->regmap, false);
	return regcache_sync(priv->regmap);
}

static int pm3003a_disable(struct regulator_dev *rdev)
{
	struct pm3003a_priv *priv = rdev_get_drvdata(rdev);

	if (priv->gpio_control_enabled) {
		if (!priv->enable_gpio)
			return -EINVAL;
	}

	/* Mark regcache as dirty and cache only before HW disabled */
	regcache_cache_only(priv->regmap, true);
	regcache_mark_dirty(priv->regmap);

	priv->enable_state = false;
	if (priv->gpio_control_enabled)
		gpiod_set_value_cansleep(priv->enable_gpio, 0);

	return 0;
}

static int pm3003a_is_enabled(struct regulator_dev *rdev)
{
	struct pm3003a_priv *priv = rdev_get_drvdata(rdev);

	return priv->enable_state ? 1 : 0;
}

static int pm3003a_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int suspend_vsel_reg;
	int vsel;

	vsel = regulator_map_voltage_linear(rdev, uV, uV);
	if (vsel < 0)
		return vsel;

	if (rdev->desc->vsel_reg == PM3003A_REG_VSELL)
		suspend_vsel_reg = PM3003A_REG_VSELH;
	else
		suspend_vsel_reg = PM3003A_REG_VSELL;

	return regmap_update_bits(regmap, suspend_vsel_reg,
				  PM3003A_VSEL_MASK, vsel);
}

static const struct regulator_ops pm3003a_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,

	.enable = pm3003a_enable,
	.disable = pm3003a_disable,
	.is_enabled = pm3003a_is_enabled,

	.set_suspend_voltage = pm3003a_set_suspend_voltage,
};

static bool pm3003a_is_accessible_reg(struct device *dev, unsigned int reg)
{
	if (reg >= PM3003A_REG_VSELL && reg <= PM3003A_REG_MONITOR)
		return true;
	return false;
}

static bool pm3003a_is_volatile_reg(struct device *dev, unsigned int reg)
{
	if (reg == PM3003A_REG_MONITOR)
		return true;
	return false;
}

static const struct regmap_config pm3003a_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PM3003A_REG_MONITOR,
	.num_reg_defaults_raw = PM3003A_NUM_REGS,
	.cache_type = REGCACHE_FLAT,

	.writeable_reg = pm3003a_is_accessible_reg,
	.readable_reg = pm3003a_is_accessible_reg,
	.volatile_reg = pm3003a_is_volatile_reg,
};

static int pm3003a_probe(struct i2c_client *i2c)
{
	struct pm3003a_priv *priv;
	struct regulator_config regulator_cfg = {};
	struct regulator_dev *rdev;
	bool vsel_active_low;
	unsigned int devid;
	int ret;

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	vsel_active_low =
		device_property_present(&i2c->dev, "qcom,vsel-active-low");

	priv->gpio_control_enabled =
		device_property_present(&i2c->dev, "gpio_control_enable");

	if (priv->gpio_control_enabled) {
		priv->enable_gpio =
			devm_gpiod_get_optional(&i2c->dev,
						"enable", GPIOD_OUT_HIGH);
		if (IS_ERR(priv->enable_gpio)) {
			dev_err(&i2c->dev, "Failed to get 'enable' gpio\n");
			return PTR_ERR(priv->enable_gpio);
		}
	}

	priv->enable_state = true;

	usleep_range(PM3003A_I2CRDY_TIMEUS, PM3003A_I2CRDY_TIMEUS + 100);

	priv->regmap = devm_regmap_init_i2c(i2c, &pm3003a_regmap_config);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&i2c->dev, "Failed to init regmap (%d)\n", ret);
		return ret;
	}

	if (!of_property_read_bool(i2c->dev.of_node, "skip-vendor-id-check")) {
		ret = regmap_read(priv->regmap, PM3003A_REG_DEVID1, &devid);
		if (ret)
			return ret;

		if ((devid & PM3003A_VID_MASK) != PM3003A_VENDOR_ID) {
			dev_err(&i2c->dev, "VID not correct [0x%02x]\n", devid);
			return -ENODEV;
		}
	}

	priv->desc.name = "pm3003a-buck";
	priv->desc.type = REGULATOR_VOLTAGE;
	priv->desc.owner = THIS_MODULE;
	priv->desc.min_uV = PM3003A_VOUT_MINUV;
	priv->desc.uV_step = PM3003A_VOUT_STPUV;
	if (vsel_active_low)
		priv->desc.vsel_reg = PM3003A_REG_VSELL;
	else
		priv->desc.vsel_reg = PM3003A_REG_VSELH;
	priv->desc.vsel_mask = PM3003A_VSEL_MASK;
	priv->desc.n_voltages = PM3003A_N_VOUTS;
	priv->desc.ops = &pm3003a_regulator_ops;

	regulator_cfg.dev = &i2c->dev;
	regulator_cfg.of_node = i2c->dev.of_node;
	regulator_cfg.regmap = priv->regmap;
	regulator_cfg.driver_data = priv;
	regulator_cfg.init_data =
		of_get_regulator_init_data(&i2c->dev, i2c->dev.of_node,
					   &priv->desc);

	rdev = devm_regulator_register(&i2c->dev, &priv->desc, &regulator_cfg);
	if (IS_ERR(rdev)) {
		dev_err(&i2c->dev, "Failed to register regulator\n");
		return PTR_ERR(rdev);
	}

	return 0;
}

static const struct of_device_id __maybe_unused pm3003a_of_match_table[] = {
	{ .compatible = "qcom,pm3003a", },
	{ }
};
MODULE_DEVICE_TABLE(of, pm3003a_of_match_table);

static struct i2c_driver pm3003a_driver = {
	.driver = {
		.name = "pm3003a",
		.of_match_table = pm3003a_of_match_table,
	},
	.probe_new = pm3003a_probe,
};
module_i2c_driver(pm3003a_driver);

MODULE_DESCRIPTION("Qualcomm PM3003A voltage regulator driver");
MODULE_AUTHOR("Kiril Petrov <kpetrov@meta.com>");
MODULE_LICENSE("GPL v2");

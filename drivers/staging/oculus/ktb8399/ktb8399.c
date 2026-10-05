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

#define KTB8399_MODE_AUTO	0
#define KTB8399_MODE_FPWM	1

#define KTB8399_REG_VSELL	0x00
#define KTB8399_REG_VSELH	0x01
#define KTB8399_REG_CNTL	0x02
#define KTB8399_REG_ID1		0x03
#define KTB8399_REG_ID2		0x04
#define KTB8399_REG_MONITOR	0x05
#define KTB8399_NUM_REGS	(KTB8399_REG_MONITOR + 1)

#define KTB8399_FPWM_MASK	BIT(3)
#define KTB8399_RAMPRATE_MASK	GENMASK(1, 0)
#define KTB8399_VID_MASK	GENMASK(7, 4)
#define KTB8399_VSEL_MASK	GENMASK(6, 0)
#define KTB8399_HDSTAT_MASK	BIT(4)
#define KTB8399_UVSTAT_MASK	BIT(3)
#define KTB8399_OCSTAT_MASK	BIT(2)
#define KTB8399_TSDSTAT_MASK	BIT(1)
#define KTB8399_PGSTAT_MASK	BIT(0)
#define KTB8399_VSEL_BUCK_EN BIT(7)

#define KTB8399_VENDOR_ID	0xA0
#define KTB8399_VOUT_MINUV_L	600000
#define KTB8399_VOUT_MAXUV_L	1393750
#define KTB8399_VOUT_STPUV_L	6250
#define KTB8399_VOUT_MINUV_H	1440000
#define KTB8399_VOUT_MAXUV_H	3345000
#define KTB8399_VOUT_STPUV_H	15000
#define KTB8399_N_VOUTS 	((KTB8399_VOUT_MAXUV_L - KTB8399_VOUT_MINUV_L) / KTB8399_VOUT_STPUV_L + 1)

#define KTB8399_I2CRDY_TIMEUS	100

struct ktb8399_priv {
	struct regulator_desc desc;
	struct regmap *regmap;
};

static int ktb8399_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int mode_val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		mode_val = KTB8399_FPWM_MASK;
		break;
	case REGULATOR_MODE_NORMAL:
		mode_val = 0;
		break;
	default:
		dev_err(&rdev->dev, "mode not supported\n");
		return -EINVAL;
	}

	return regmap_update_bits(regmap, KTB8399_REG_CNTL, KTB8399_FPWM_MASK, mode_val);
}

static unsigned int ktb8399_get_mode(struct regulator_dev *rdev)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(regmap, KTB8399_REG_CNTL, &val);
	if (ret)
		return ret;

	if (val & KTB8399_FPWM_MASK)
		return REGULATOR_MODE_FAST;

	return REGULATOR_MODE_NORMAL;
}

static int ktb8399_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int suspend_vsel_reg;
	int vsel;

	vsel = regulator_map_voltage_linear(rdev, uV, uV);
	if (vsel < 0)
		return vsel;

	if (rdev->desc->vsel_reg == KTB8399_REG_VSELL)
		suspend_vsel_reg = KTB8399_REG_VSELH;
	else
		suspend_vsel_reg = KTB8399_REG_VSELL;

	return regmap_update_bits(regmap, suspend_vsel_reg,
				  KTB8399_VSEL_MASK, vsel);
}

static int ktb8399_set_suspend_enable(struct regulator_dev *rdev)
{
	struct regmap *regmap = rdev_get_regmap(rdev);

	return regmap_update_bits(regmap, rdev->desc->vsel_reg,
				  KTB8399_VSEL_BUCK_EN, KTB8399_VSEL_BUCK_EN);
}

static int ktb8399_set_suspend_disable(struct regulator_dev *rdev)
{
	struct regmap *regmap = rdev_get_regmap(rdev);

	return regmap_update_bits(regmap, rdev->desc->vsel_reg,
				  KTB8399_VSEL_BUCK_EN, 0);
}

static const struct regulator_ops ktb8399_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = ktb8399_set_mode,
	.get_mode = ktb8399_get_mode,
	.set_suspend_voltage = ktb8399_set_suspend_voltage,
	.set_suspend_enable = ktb8399_set_suspend_enable,
	.set_suspend_disable = ktb8399_set_suspend_disable,
};

static unsigned int ktb8399_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case KTB8399_MODE_FPWM:
		return REGULATOR_MODE_FAST;
	case KTB8399_MODE_AUTO:
		return REGULATOR_MODE_NORMAL;
	}

	return REGULATOR_MODE_INVALID;
}

static bool ktb8399_is_accessible_reg(struct device *dev, unsigned int reg)
{
	if (reg >= KTB8399_REG_VSELL && reg <= KTB8399_REG_MONITOR)
		return true;
	return false;
}

static const struct regmap_config ktb8399_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = KTB8399_REG_MONITOR,
	.num_reg_defaults_raw = KTB8399_NUM_REGS,
	.cache_type = REGCACHE_FLAT,

	.writeable_reg = ktb8399_is_accessible_reg,
	.readable_reg = ktb8399_is_accessible_reg,
};

static int ktb8399_probe(struct i2c_client *i2c)
{
	struct ktb8399_priv *priv;
	struct regulator_config regulator_cfg = {};
	struct device_node *node = i2c->dev.of_node;
	struct regulator_dev *rdev;
	bool vsel_active_low = false;
	unsigned int devid;
	int ret;

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regmap = devm_regmap_init_i2c(i2c, &ktb8399_regmap_config);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&i2c->dev, "Failed to init regmap (%d)\n", ret);
		return ret;
	}

	ret = regmap_read(priv->regmap, KTB8399_REG_ID1, &devid);
	if (ret)
		return ret;

	if ((devid & KTB8399_VID_MASK) != KTB8399_VENDOR_ID) {
		dev_err(&i2c->dev, "VID not correct [0x%02x]\n", devid);
		return -ENODEV;
	}

	vsel_active_low = of_property_read_bool(node, "ktb8399,vsel-active-low");

	priv->desc.name = "ktb8399-buck";
	priv->desc.type = REGULATOR_VOLTAGE;
	priv->desc.owner = THIS_MODULE;

	if (vsel_active_low) {
		priv->desc.vsel_reg = KTB8399_REG_VSELL;
		priv->desc.min_uV = KTB8399_VOUT_MINUV_L;
		priv->desc.uV_step = KTB8399_VOUT_STPUV_L;
	} else {
		priv->desc.vsel_reg = KTB8399_REG_VSELH;
		priv->desc.min_uV = KTB8399_VOUT_MINUV_H;
		priv->desc.uV_step = KTB8399_VOUT_STPUV_H;
	}

	priv->desc.vsel_mask = KTB8399_VSEL_MASK;
	priv->desc.n_voltages = KTB8399_N_VOUTS;
	priv->desc.of_map_mode = ktb8399_of_map_mode;
	priv->desc.ops = &ktb8399_regulator_ops;

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

static const struct of_device_id __maybe_unused ktb8399_of_match_table[] = {
	{ .compatible = "kinetic,ktb8399", },
	{}
};
MODULE_DEVICE_TABLE(of, ktb8399_of_match_table);

static struct i2c_driver ktb8399_driver = {
	.driver = {
		.name = "ktb8399",
		.of_match_table = ktb8399_of_match_table,
	},
	.probe_new = ktb8399_probe,
};
module_i2c_driver(ktb8399_driver);

MODULE_DESCRIPTION("Kinetic KTB8399 voltage regulator driver");
MODULE_LICENSE("GPL v2");

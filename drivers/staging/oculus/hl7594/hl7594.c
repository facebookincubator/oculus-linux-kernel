// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

/* HL7594 Registers */
enum {
	HL7594_REG_VSEL0,
	HL7594_REG_VSEL1,
	HL7594_REG_CTRL,
	HL7594_REG_ID1,
	HL7594_REG_ID2,
	HL7594_REG_MONITOR,
	HL7594_REG_VOUT_DELAY,
};

#define HL7594_MASK_PWM 	(0x3 << 0)
#define HL7594_VSEL_MASK	GENMASK(6, 0)
#define HL7594_VOUT_MINUV_L	270000
#define HL7594_VOUT_MAXUV_L	627200
#define HL7594_VOUT_STPUV_L	2812
#define HL7594_VOUT_MINUV_H	600000
#define HL7594_VOUT_MAXUV_H	1394000
#define HL7594_VOUT_STPUV_H	6250
#define HL7594_N_VOUTS		((HL7594_VOUT_MAXUV_H - HL7594_VOUT_MINUV_H) / HL7594_VOUT_STPUV_H + 1)

struct hl7594_regulator_info {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_desc desc;
	struct regulator_dev *regulator;
	bool use_pwm_mode;
	bool vsel_active_low;
};

static const struct regmap_config hl7594_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = HL7594_REG_VOUT_DELAY,
	.use_single_read = true,
	.use_single_write = true,
};

static const struct regulator_ops hl7594_common_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

static struct regulator_desc hl7594_reg_vout = {
	.name = "hl7594",
	.id = 0,
	.ops = &hl7594_common_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
};

static int hl7594_init_regulator(struct hl7594_regulator_info *info,
				 struct device_node *node)
{
	struct regulator_config config = { };

	info->desc = hl7594_reg_vout;

	if (info->vsel_active_low) {
		info->desc.vsel_reg = HL7594_REG_VSEL0;
		info->desc.min_uV = HL7594_VOUT_MINUV_L;
		info->desc.uV_step = HL7594_VOUT_STPUV_L;
	} else {
		info->desc.vsel_reg = HL7594_REG_VSEL1;
		info->desc.min_uV = HL7594_VOUT_MINUV_H;
		info->desc.uV_step = HL7594_VOUT_STPUV_H;
	}
	info->desc.vsel_mask = HL7594_VSEL_MASK;
	info->desc.n_voltages = HL7594_N_VOUTS;

	config.regmap = info->regmap;
	config.driver_data = info;
	config.dev = info->dev;
	config.of_node = node;
	config.init_data = of_get_regulator_init_data(info->dev, node, &info->desc);

	info->regulator = devm_regulator_register(info->dev, &info->desc, &config);

	if (IS_ERR(info->regulator))
		return PTR_ERR(info->regulator);

	return 0;
}

/**
 * Set Buck mode according the value get in hl7594 probe.
 * value true means Force PWM mode,value false means enable Auto PFM mode
 */
static hl7594_init_pwm(struct hl7594_regulator_info *info)
{
	int ret;
	unsigned int rval;

	if (info->use_pwm_mode) {
		ret = regmap_read(info->regmap, HL7594_REG_CTRL, &rval);
		if (ret < 0) {
			dev_err(info->dev, "PWM read failed, err %d\n", ret);
			return ret;
		}
		dev_dbg(info->dev, "PWM read 0x%2x before\n", rval);

		ret = regmap_update_bits(info->regmap, HL7594_REG_CTRL,
					 HL7594_MASK_PWM, HL7594_MASK_PWM);
		if (ret < 0) {
			dev_err(info->dev, "PWM reg write failed, err %d\n", ret);
			return ret;
		}

		ret = regmap_read(info->regmap, HL7594_REG_CTRL, &rval);
		if (ret < 0) {
			dev_err(info->dev, "PWM read failed, err %d\n", ret);
			return ret;
		}
		dev_dbg(info->dev, "PWM read 0x%2x after\n", rval);
	} else {
		ret = regmap_read(info->regmap, HL7594_REG_CTRL, &rval);
		if (ret < 0) {
			dev_err(info->dev, "NOPWM read failed, err %d\n", ret);
			return ret;
		}
		dev_dbg(info->dev, "NOPWM read 0x%2x before\n", rval);

		if (rval & HL7594_MASK_PWM) {
			ret = regmap_update_bits(info->regmap, HL7594_REG_CTRL,
						 HL7594_MASK_PWM, 0);
			if (ret < 0) {
				dev_err(info->dev, "NOPWM reg write failed, err %d\n", ret);
				return ret;
			}

			ret = regmap_read(info->regmap, HL7594_REG_CTRL, &rval);
			if (ret < 0) {
				dev_err(info->dev, "NOPWM read failed, err %d\n", ret);
				return ret;
			}
			dev_dbg(info->dev, "NOPWM read 0x%2x after\n", rval);
		}
	}
	return 0;
}

static int hl7594_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct device_node *node = client->dev.of_node;
	struct device *dev = &client->dev;
	struct hl7594_regulator_info *info;
	int ret;

	info = devm_kzalloc(dev, sizeof(struct hl7594_regulator_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	i2c_set_clientdata(client, info);
	info->dev = dev;

	info->regmap = devm_regmap_init_i2c(client, &hl7594_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(dev, "Failed to allocate regmap!\n");
		return PTR_ERR(info->regmap);
	}

	info->vsel_active_low =
		of_property_read_bool(node, "hl7594,vsel-active-low");

	ret = hl7594_init_regulator(info, node);
	if (ret < 0) {
		dev_err(dev, "failed to register regulator: %d\n", ret);
		return ret;
	}

	info->use_pwm_mode = of_property_read_bool(node, "hl7594,using-pwm-mode");
	hl7594_init_pwm(info);

	dev_info(dev, "hl7594 init done\n");
	return 0;
}

static const struct i2c_device_id hl7594_id[] = {
	{ "hl7594-regulator" },
	{}
};
MODULE_DEVICE_TABLE(i2c, hl7594_id);

static const struct of_device_id __maybe_unused hl7594_of_match[] = {
	{ .compatible = "hl,hl7594" },
	{}
};
MODULE_DEVICE_TABLE(of, hl7594_of_match);

static struct i2c_driver hl7594_regulator_driver = {
	.driver = {
		.name = "hl7594",
		.of_match_table = of_match_ptr(hl7594_of_match),
	},
	.probe = hl7594_i2c_probe,
	.id_table = hl7594_id,
};
module_i2c_driver(hl7594_regulator_driver);

MODULE_DESCRIPTION("HL7594 BUCK-BOOST regulator driver");
MODULE_LICENSE("GPL v2");

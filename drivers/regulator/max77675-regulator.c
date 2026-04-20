// SPDX-License-Identifier: GPL-2.0+

#define pr_fmt(fmt) "max77675-reg: %s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/proxy-consumer.h>

#define MAX77675_CNFG_GLBL_A_REG 0x00
#define MAX77675_CNFG_GLBL_B_REG 0x01
#define MAX77675_INT_GLBL_REG 0x02
#define MAX77675_INTM_GLBL 0x03
#define MAX77675_STAT_GLBL_REG 0x04
#define MAX77675_ERCFLAG_REG 0x05
#define MAX77675_CID_REG 0x06
#define MAX77675_CNFG_SBB_TOP_A_REG 0x07
#define MAX77675_CNFG_SBB0_A_REG 0x08
#define MAX77675_CNFG_SBB0_B_REG 0x09
#define MAX77675_CNFG_SBB1_A_REG 0x0A
#define MAX77675_CNFG_SBB1_B_REG 0x0B
#define MAX77675_CNFG_SBB2_A_REG 0x0C
#define MAX77675_CNFG_SBB2_B_REG 0x0D
#define MAX77675_CNFG_SBB3_A_REG 0x0E
#define MAX77675_CNFG_SBB3_B_REG 0x0F
#define MAX77675_CNFG_SBB_TOP_B_REG 0x10

#define MAX77675_CNFG_AD_MASK BIT(3)
#define MAX77675_CNFG_AD_DISABLE 0x00
#define MAX77675_CNFG_AD_ENABLE BIT(3)

#define MAX77675_CNFG_EN_CTRL_MASK GENMASK(2, 0)
#define MAX77675_CNFG_SBB_DISABLE BIT(2)
#define MAX77675_CNFG_SBB_ENABLE (BIT(2) | BIT(1))
#define MAX77675_CNFG_TV_SBB_MASK GENMASK(7, 0)

#define MAX77675_CNFG_SBB_ADE (BIT(3))

#define MAX77675_STEP_UVOLT 25000
#define MAX77675_BASE_UVOLT 500000

enum {
	MAX77675_REGULATOR_ID_SBB0 = 0,
	MAX77675_REGULATOR_ID_SBB1,
	MAX77675_REGULATOR_ID_SBB2,
	MAX77675_REGULATOR_ID_SBB3,
	MAX77675_REGULATOR_NUM_REGULATORS,
};

struct max77675_priv {
	struct device *dev;
	struct max77675_regulator_desc **rdescs;
	struct regmap *map;
	unsigned int en_gpio;
	bool enable_state;
};

struct max77675_regulator_desc {
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	unsigned int regA;
	unsigned int regB;
	int enable;
};

static const struct regmap_config max77675_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x10,
};

inline int max77675_reg_read(struct regmap *map, unsigned int reg,
			       unsigned int *val)
{
	pr_debug("Read Addr: %08x", reg);
	return regmap_read(map, reg, val);
}

inline int max77675_reg_write(struct regmap *map, unsigned int reg,
			       unsigned int val)
{
	pr_debug("Write Addr: %08x Data: %08x", reg, val);
	return regmap_write(map, reg, val);
}

inline int max77675_reg_update_bits(struct regmap *map, unsigned int reg,
				     unsigned int mask, unsigned int val)
{
	pr_debug("Update Addr: %08x Mask%08x  Val:%08x",
		 reg, mask, val);
	return regmap_update_bits(map, reg, mask, val);
}

static int max77675_regulator_enable(struct regulator_dev *rdev)
{
	struct max77675_regulator_desc *rdesc;
	struct regmap *map;

	pr_debug("Enable regulator");
	rdesc = rdev_get_drvdata(rdev);
	map = rdev_get_regmap(rdev);

	return max77675_reg_update_bits(map, rdesc->regB,
				  MAX77675_CNFG_EN_CTRL_MASK,
				  MAX77675_CNFG_SBB_ENABLE);
}

static int max77675_regulator_disable(struct regulator_dev *rdev)
{
	struct max77675_regulator_desc *rdesc;
	struct regmap *map;

	pr_debug("Disable regulator");
	rdesc = rdev_get_drvdata(rdev);
	map = rdev_get_regmap(rdev);

	return max77675_reg_update_bits(map, rdesc->regB,
				  MAX77675_CNFG_EN_CTRL_MASK,
				  MAX77675_CNFG_SBB_DISABLE);
}

static int max77675_set_active_discharge(struct regulator_dev *rdev,
					 bool enable)
{
	struct max77675_regulator_desc *rdesc;
	struct regmap *map;

	pr_debug("set_active discharge");
	rdesc = rdev_get_drvdata(rdev);
	map = rdev_get_regmap(rdev);
	if (!enable)
		return max77675_reg_update_bits(map, rdesc->regB,
				MAX77675_CNFG_AD_MASK,
				MAX77675_CNFG_AD_DISABLE);

	return max77675_reg_update_bits(map, rdesc->regB,
				  MAX77675_CNFG_AD_MASK,
				  MAX77675_CNFG_AD_ENABLE);
}

int max77675_get_voltage(struct regulator_dev *rdev)
{
	int regval;
	struct max77675_regulator_desc *rdesc;
	struct regmap *map;

	rdesc = rdev_get_drvdata(rdev);
	map = rdev_get_regmap(rdev);

	if (!max77675_reg_read(map, rdesc->regA, &regval))
		return (regval & MAX77675_CNFG_TV_SBB_MASK) * MAX77675_STEP_UVOLT
			+ MAX77675_BASE_UVOLT;

	return 0;
};

int max77675_set_voltage(struct regulator_dev *rdev,
			 int min_uv, int max_uv, unsigned int *selector)
{
	int regval, mv;
	struct max77675_regulator_desc *rdesc;
	struct regmap *map;

	pr_debug("set_voltage. min_uv(%d) max_uv(%d)", min_uv, max_uv);
	rdesc = rdev_get_drvdata(rdev);
	map = rdev_get_regmap(rdev);

	mv = DIV_ROUND_UP(min_uv, 1000);

	regval = DIV_ROUND_UP(mv < 500 ? 0 : mv - 500, 25);

	return max77675_reg_write(map, rdesc->regA, regval);
};

static int max77675_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct max77675_regulator_desc *rdesc;
	struct regmap *map;
	int val, rc, en;

	pr_debug("is_enabled");
	rdesc = rdev_get_drvdata(rdev);
	map = rdev_get_regmap(rdev);

	rc = max77675_reg_read(map, rdesc->regB, &val);
	if (rc)
		return rc;

	en = val & MAX77675_CNFG_EN_CTRL_MASK;
	return en == MAX77675_CNFG_SBB_ENABLE;
}

static const struct regulator_ops max77675_regulator_sbb_ops = {
	.is_enabled		= max77675_regulator_is_enabled,
	.enable			= max77675_regulator_enable,
	.disable		= max77675_regulator_disable,
	.set_active_discharge	= max77675_set_active_discharge,
	.get_voltage = max77675_get_voltage,
	.set_voltage = max77675_set_voltage,
};

static struct max77675_regulator_desc max77675_sbb0_desc = {
	.desc = {
		.name			= "sbb0",
		.of_match		= of_match_ptr("regulator_sbb0"),
		.regulators_node	= of_match_ptr("regulators"),
		.supply_name		= "simo-sbb0",
		.id			= MAX77675_REGULATOR_ID_SBB0,
		.ops			= &max77675_regulator_sbb_ops,
		.type			= REGULATOR_VOLTAGE,
		.owner			= THIS_MODULE,
	},
	.regA		= MAX77675_CNFG_SBB0_A_REG,
	.regB		= MAX77675_CNFG_SBB0_B_REG,
};

static struct max77675_regulator_desc max77675_sbb1_desc = {
	.desc = {
		.name			= "sbb1",
		.of_match		= of_match_ptr("regulator_sbb1"),
		.regulators_node	= of_match_ptr("regulators"),
		.supply_name		= "simo-sbb1",
		.id			= MAX77675_REGULATOR_ID_SBB0,
		.ops			= &max77675_regulator_sbb_ops,
		.type			= REGULATOR_VOLTAGE,
		.owner			= THIS_MODULE,
	},
	.regA		= MAX77675_CNFG_SBB1_A_REG,
	.regB		= MAX77675_CNFG_SBB1_B_REG,
};

static struct max77675_regulator_desc max77675_sbb2_desc = {
	.desc = {
		.name			= "sbb2",
		.of_match		= of_match_ptr("regulator_sbb2"),
		.regulators_node	= of_match_ptr("regulators"),
		.supply_name		= "simo-sbb2",
		.id			= MAX77675_REGULATOR_ID_SBB2,
		.ops			= &max77675_regulator_sbb_ops,
		.type			= REGULATOR_VOLTAGE,
		.owner			= THIS_MODULE,
	},
	.regA		= MAX77675_CNFG_SBB2_A_REG,
	.regB		= MAX77675_CNFG_SBB2_B_REG,
};

static struct max77675_regulator_desc max77675_sbb3_desc = {
	.desc = {
		.name			= "sbb3",
		.of_match		= of_match_ptr("regulator_sbb3"),
		.regulators_node	= of_match_ptr("regulators"),
		.supply_name		= "simo-sbb3",
		.id			= MAX77675_REGULATOR_ID_SBB3,
		.ops			= &max77675_regulator_sbb_ops,
		.type			= REGULATOR_VOLTAGE,
		.owner			= THIS_MODULE,
	},
	.regA		= MAX77675_CNFG_SBB3_A_REG,
	.regB		= MAX77675_CNFG_SBB3_B_REG,
};

int max77675_pmic_enable(struct max77675_priv *priv)
{
	int rc = 0, i;

	/* Inverter seats between nEN pin of max7767 SIMO PMIC to AP */
	gpio_set_value(priv->en_gpio, 1);
	/* Wait max77675 chipset to be enabled. */
	msleep(100);

	/*  Disable output of all sbb right after enable SIMO */
	for (i = 0; i < MAX77675_REGULATOR_NUM_REGULATORS; i++) {
		// Disable SBB0~3 before regulators are registered by
		// directly wirting into registers
		rc = max77675_reg_update_bits(priv->map, priv->rdescs[i]->regB,
					      MAX77675_CNFG_AD_MASK | MAX77675_CNFG_EN_CTRL_MASK,
					      MAX77675_CNFG_AD_ENABLE | MAX77675_CNFG_SBB_DISABLE);
		if (rc) {
			pr_err("Failed to disable regulator(%d) rc:%d\n", i, rc);
			return rc;
		}
	}

	/* Wait SBB0~3 to be settled. */
	msleep(500);
	return rc;
};

static int max77675_pmic_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	struct max77675_priv *priv;
	struct max77675_regulator_desc **rdescs;
	struct max77675_regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct regulator_config reg_config = {};
	int ret, i = 0;

	pr_info("start");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	i2c_set_clientdata(client, priv);
	rdescs = devm_kcalloc(&client->dev, MAX77675_REGULATOR_NUM_REGULATORS,
			      sizeof(*rdescs), GFP_KERNEL);
	if (!rdescs)
		return -ENOMEM;

	priv->rdescs = rdescs;
	priv->dev = &client->dev;
	priv->map = devm_regmap_init_i2c(client, &max77675_regmap_config);
	if (IS_ERR(priv->map)) {
		ret = PTR_ERR(priv->map);
		pr_err("regmap init failed, err %d\n", ret);
		return ret;
	}

	priv->en_gpio = of_get_named_gpio(client->dev.of_node, "enable-gpio", 0);
	if (gpio_is_valid(priv->en_gpio)) {
		if (devm_gpio_request(priv->dev, priv->en_gpio, "enable-gpio")) {
			pr_err("failed to request enable gpio");
			return -EINVAL;
		}
		/* Set to 0 by default */
		gpio_direction_output(priv->en_gpio, 0);
	}

	rdescs[MAX77675_REGULATOR_ID_SBB0] = &max77675_sbb0_desc;
	rdescs[MAX77675_REGULATOR_ID_SBB1] = &max77675_sbb1_desc;
	rdescs[MAX77675_REGULATOR_ID_SBB2] = &max77675_sbb2_desc;
	rdescs[MAX77675_REGULATOR_ID_SBB3] = &max77675_sbb3_desc;

	max77675_pmic_enable(priv);

	for (i = 0; i < MAX77675_REGULATOR_NUM_REGULATORS; i++) {
		rdesc = rdescs[i];
		reg_config.dev = &client->dev;
		reg_config.regmap = priv->map;
		reg_config.driver_data = rdesc;

		rdev = devm_regulator_register(&client->dev, &rdesc->desc, &reg_config);
		if (IS_ERR(rdev)) {
			pr_err("Failed to register regulator. sbb(%d) rc=%d\n", i, ret);
			return PTR_ERR(rdev);
		}

		rdesc->rdev = rdev;

		ret = devm_regulator_debug_register(&client->dev, rdev);
		if (ret)
			pr_err("Failed to register debug regulator. rc=%d\n", ret);
	}

	pr_info("completed ");
	return 0;
};

static const struct of_device_id max77675_match_tbl[] = {
	{ .compatible = "adi,max77675", },
	{ }
};
MODULE_DEVICE_TABLE(of, max77675_match_tbl);

static const struct i2c_device_id max77675_pmic_id[] = {
	{"max77675-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, max77675_pmic_id);

static struct i2c_driver max77675_pmic_driver = {
	.driver = {
		.name = "max77675-pmic",
		.owner  = THIS_MODULE,
		.of_match_table = max77675_match_tbl,
	},

	.probe = max77675_pmic_probe,
	.id_table = max77675_pmic_id,
};
module_i2c_driver(max77675_pmic_driver);

MODULE_DESCRIPTION("MAX77675 Regulator Driver");
MODULE_AUTHOR("Jason Bak <jasonbak@meta.com>");
MODULE_LICENSE("GPL");

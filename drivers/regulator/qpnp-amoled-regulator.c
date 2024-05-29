// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"AMOLED: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

/* Register definitions */
#define PERIPH_TYPE			0x04
#define IBB_PERIPH_TYPE			0x20
#define AB_PERIPH_TYPE			0x24
#define OLEDB_PERIPH_TYPE		0x2C

#define PERIPH_SUBTYPE			0x05

/* AB */
#define AB_LDO_PD_CTL(chip)		(chip->ab_base + 0x78)

/* AB_LDO_PD_CTL */
#define PULLDN_EN_BIT			BIT(7)

/* IBB */
#define IBB_PD_CTL(chip)		(chip->ibb_base + 0x47)

/* IBB_PD_CTL */
#define ENABLE_PD_BIT			BIT(7)

#define IBB_DUAL_PHASE_CTL(chip)	(chip->ibb_base + 0x70)

/* IBB_DUAL_PHASE_CTL */
#define IBB_DUAL_PHASE_CTL_MASK		GENMASK(2, 0)
#define AUTO_DUAL_PHASE_BIT		BIT(2)
#define FORCE_DUAL_PHASE_BIT		BIT(1)
#define FORCE_SINGLE_PHASE_BIT		BIT(0)

struct amoled_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct device_node	*node;
	unsigned int		mode;
	bool			enabled;
};

struct oledb_regulator {
	struct amoled_regulator	vreg;

	/* DT params */
	bool			swire_control;
};

struct ab_regulator {
	struct amoled_regulator	vreg;

	/* DT params */
	bool			swire_control;
	bool			pd_control;
};

struct ibb_regulator {
	struct amoled_regulator	vreg;
	u8			subtype;

	/* DT params */
	bool			swire_control;
	bool			pd_control;
	bool			single_phase;
};

struct qpnp_amoled {
	struct device		*dev;
	struct regmap		*regmap;
	struct oledb_regulator	oledb;
	struct ab_regulator	ab;
	struct ibb_regulator	ibb;

	/* DT params */
	u32			oledb_base;
	u32			ab_base;
	u32			ibb_base;
};

enum reg_type {
	OLEDB,
	AB,
	IBB,
};

enum ibb_subtype {
	PM8150A_IBB = 0x03,
	PM8350B_IBB = 0x04,
};

static inline bool is_phase_ctrl_supported(struct ibb_regulator *ibb)
{
	if (ibb->subtype == PM8350B_IBB)
		return true;

	return false;
}

static int qpnp_amoled_read(struct qpnp_amoled *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc = 0;

	rc = regmap_bulk_read(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to read from addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

static int qpnp_amoled_write(struct qpnp_amoled *chip,
			u16 addr, u8 *value, u8 count)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, addr, value, count);
	if (rc < 0)
		pr_err("Failed to write to addr=0x%02x rc=%d\n", addr, rc);

	return rc;
}

static int qpnp_amoled_masked_write(struct qpnp_amoled *chip,
				u16 addr, u8 mask, u8 value)
{
	int rc = 0;

	rc = regmap_update_bits(chip->regmap, addr, mask, value);
	if (rc < 0)
		pr_err("Failed to write addr=0x%02x value=0x%02x rc=%d\n",
			addr, value, rc);

	return rc;
}

/* AB regulator */

static int qpnp_ab_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->ab.vreg.enabled;
}

static int qpnp_ab_regulator_enable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ab.vreg.enabled = true;
	return 0;
}

static int qpnp_ab_regulator_disable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ab.vreg.enabled = false;
	return 0;
}

/* IBB regulator */

static int qpnp_ibb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->ibb.vreg.enabled;
}

static int qpnp_ibb_regulator_enable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ibb.vreg.enabled = true;
	return 0;
}

static int qpnp_ibb_regulator_disable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->ibb.vreg.enabled = false;
	return 0;
}

/* common to AB and IBB */

static int qpnp_ab_ibb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->ab.swire_control || chip->ibb.swire_control)
		return 0;

	return 0;
}

static int qpnp_ab_ibb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->ab.swire_control || chip->ibb.swire_control)
		return 0;

	return 0;
}

static int qpnp_ab_pd_control(struct qpnp_amoled *chip, bool en)
{
	u8 val = en ? PULLDN_EN_BIT : 0;

	return qpnp_amoled_write(chip, AB_LDO_PD_CTL(chip), &val, 1);
}

static int qpnp_ibb_pd_control(struct qpnp_amoled *chip, bool en)
{
	u8 val = en ? ENABLE_PD_BIT : 0;

	return qpnp_amoled_masked_write(chip, IBB_PD_CTL(chip), ENABLE_PD_BIT,
					val);
}

static int qpnp_ab_ibb_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);
	int rc = 0;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_STANDBY &&
		mode != REGULATOR_MODE_IDLE) {
		pr_err("Unsupported mode %u\n", mode);
		return -EINVAL;
	}

	if (mode == chip->ab.vreg.mode || mode == chip->ibb.vreg.mode)
		return 0;

	pr_debug("mode: %d\n", mode);

	if (mode == REGULATOR_MODE_NORMAL || mode == REGULATOR_MODE_STANDBY) {
		if (chip->ibb.pd_control) {
			rc = qpnp_ibb_pd_control(chip, true);
			if (rc < 0)
				goto error;
		}

		if (chip->ab.pd_control) {
			rc = qpnp_ab_pd_control(chip, true);
			if (rc < 0)
				goto error;
		}
	} else if (mode == REGULATOR_MODE_IDLE) {
		if (chip->ibb.pd_control) {
			rc = qpnp_ibb_pd_control(chip, false);
			if (rc < 0)
				goto error;
		}

		if (chip->ab.pd_control) {
			rc = qpnp_ab_pd_control(chip, false);
			if (rc < 0)
				goto error;
		}
	}

	chip->ab.vreg.mode = chip->ibb.vreg.mode = mode;
error:
	if (rc < 0)
		pr_err("Failed to configure for mode %d\n", mode);
	return rc;
}

static unsigned int qpnp_ab_ibb_regulator_get_mode(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->ibb.vreg.mode;
}

#define SINGLE_PHASE_ILIMIT_UA	30000

static int qpnp_ibb_regulator_set_load(struct regulator_dev *rdev,
				int load_uA)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);
	u8 ibb_phase;

	if (!is_phase_ctrl_supported(&chip->ibb))
		return 0;

	/* For IBB single phase, it's configured only once. */
	if (chip->ibb.single_phase)
		return 0;

	if (load_uA < 0)
		return -EINVAL;
	else if (load_uA <= SINGLE_PHASE_ILIMIT_UA)
		ibb_phase = AUTO_DUAL_PHASE_BIT;
	else
		ibb_phase = FORCE_DUAL_PHASE_BIT;

	return qpnp_amoled_masked_write(chip, IBB_DUAL_PHASE_CTL(chip),
			IBB_DUAL_PHASE_CTL_MASK, ibb_phase);
}

static struct regulator_ops qpnp_amoled_ab_ops = {
	.enable		= qpnp_ab_regulator_enable,
	.disable	= qpnp_ab_regulator_disable,
	.is_enabled	= qpnp_ab_regulator_is_enabled,
	.set_voltage	= qpnp_ab_ibb_regulator_set_voltage,
	.get_voltage	= qpnp_ab_ibb_regulator_get_voltage,
	.set_mode	= qpnp_ab_ibb_regulator_set_mode,
	.get_mode	= qpnp_ab_ibb_regulator_get_mode,
};

static struct regulator_ops qpnp_amoled_ibb_ops = {
	.enable		= qpnp_ibb_regulator_enable,
	.disable	= qpnp_ibb_regulator_disable,
	.is_enabled	= qpnp_ibb_regulator_is_enabled,
	.set_voltage	= qpnp_ab_ibb_regulator_set_voltage,
	.get_voltage	= qpnp_ab_ibb_regulator_get_voltage,
	.set_mode	= qpnp_ab_ibb_regulator_set_mode,
	.get_mode	= qpnp_ab_ibb_regulator_get_mode,
	.set_load	= qpnp_ibb_regulator_set_load,
};

/* OLEDB regulator */

static int qpnp_oledb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->oledb.vreg.enabled;
}

static int qpnp_oledb_regulator_enable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.enabled = true;
	return 0;
}

static int qpnp_oledb_regulator_disable(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.enabled = false;
	return 0;
}

static int qpnp_oledb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->oledb.swire_control)
		return 0;

	return 0;
}

static int qpnp_oledb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	/* HW controlled */
	if (chip->oledb.swire_control)
		return 0;

	return 0;
}

static int qpnp_oledb_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	chip->oledb.vreg.mode = mode;
	return 0;
}

static unsigned int qpnp_oledb_regulator_get_mode(struct regulator_dev *rdev)
{
	struct qpnp_amoled *chip  = rdev_get_drvdata(rdev);

	return chip->oledb.vreg.mode;
}

static struct regulator_ops qpnp_amoled_oledb_ops = {
	.enable		= qpnp_oledb_regulator_enable,
	.disable	= qpnp_oledb_regulator_disable,
	.is_enabled	= qpnp_oledb_regulator_is_enabled,
	.set_voltage	= qpnp_oledb_regulator_set_voltage,
	.get_voltage	= qpnp_oledb_regulator_get_voltage,
	.set_mode	= qpnp_oledb_regulator_set_mode,
	.get_mode	= qpnp_oledb_regulator_get_mode,
};

static int qpnp_amoled_regulator_register(struct qpnp_amoled *chip,
					enum reg_type type)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct device_node *node;

	if (type == OLEDB) {
		node		= chip->oledb.vreg.node;
		rdesc		= &chip->oledb.vreg.rdesc;
		rdesc->ops	= &qpnp_amoled_oledb_ops;
		rdev		= chip->oledb.vreg.rdev;
	} else if (type == AB) {
		node		= chip->ab.vreg.node;
		rdesc		= &chip->ab.vreg.rdesc;
		rdesc->ops	= &qpnp_amoled_ab_ops;
		rdev		= chip->ab.vreg.rdev;
	} else if (type == IBB) {
		node		= chip->ibb.vreg.node;
		rdesc		= &chip->ibb.vreg.rdesc;
		rdesc->ops	= &qpnp_amoled_ibb_ops;
		rdev		= chip->ibb.vreg.rdev;
	} else {
		pr_err("Invalid regulator type %d\n", type);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(chip->dev, node, rdesc);
	if (!init_data) {
		pr_err("Failed to get regulator_init_data for type %d\n", type);
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		rdesc->owner	= THIS_MODULE;
		rdesc->type	= REGULATOR_VOLTAGE;
		rdesc->name	= init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = node;

		if (of_get_property(chip->dev->of_node, "parent-supply",
				NULL))
			init_data->supply_regulator = "parent";

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS
				| REGULATOR_CHANGE_MODE;
		init_data->constraints.valid_modes_mask
				|= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE
					| REGULATOR_MODE_STANDBY;

		rdev = devm_regulator_register(chip->dev, rdesc, &cfg);
		if (IS_ERR(rdev)) {
			rc = PTR_ERR(rdev);
			rdev = NULL;
			pr_err("Failed to register amoled regulator for type %d rc = %d\n",
				type, rc);
			return rc;
		}

		rc = devm_regulator_debug_register(chip->dev, rdev);
		if (rc) {
			pr_err("failed to register debug regulator rc=%d\n",
				rc);
			rc = 0;
		}

		if (type == OLEDB)
			chip->oledb.vreg.mode = REGULATOR_MODE_NORMAL;
		else if (type == IBB)
			chip->ibb.vreg.mode = REGULATOR_MODE_NORMAL;
		else
			chip->ab.vreg.mode = REGULATOR_MODE_NORMAL;
	} else {
		pr_err("regulator name missing for type %d\n", type);
		return -EINVAL;
	}

	return rc;
}

static int qpnp_amoled_hw_init(struct qpnp_amoled *chip)
{
	int rc;
	u8 val;

	rc = qpnp_amoled_regulator_register(chip, OLEDB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register OLEDB regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = qpnp_amoled_regulator_register(chip, AB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register AB regulator rc=%d\n",
			rc);
		return rc;
	}

	rc = qpnp_amoled_regulator_register(chip, IBB);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to register IBB regulator rc=%d\n",
			rc);
		return rc;
	}

	if (is_phase_ctrl_supported(&chip->ibb) && chip->ibb.single_phase) {
		val = FORCE_SINGLE_PHASE_BIT;

		rc = qpnp_amoled_masked_write(chip, IBB_DUAL_PHASE_CTL(chip),
			IBB_DUAL_PHASE_CTL_MASK, val);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int qpnp_amoled_parse_dt(struct qpnp_amoled *chip)
{
	struct device_node *temp, *node = chip->dev->of_node;
	const __be32 *prop_addr;
	int rc = 0;
	u32 base;
	u8 val[2];

	for_each_available_child_of_node(node, temp) {
		prop_addr = of_get_address(temp, 0, NULL, NULL);
		if (!prop_addr) {
			pr_err("Couldn't get reg address\n");
			return -EINVAL;
		}

		base = be32_to_cpu(*prop_addr);
		rc = qpnp_amoled_read(chip, base + PERIPH_TYPE, val, 2);
		if (rc < 0) {
			pr_err("Couldn't read PERIPH_TYPE for base %x\n", base);
			return rc;
		}

		switch (val[0]) {
		case OLEDB_PERIPH_TYPE:
			chip->oledb_base = base;
			chip->oledb.vreg.node = temp;
			chip->oledb.swire_control = of_property_read_bool(temp,
							"qcom,swire-control");
			break;
		case AB_PERIPH_TYPE:
			chip->ab_base = base;
			chip->ab.vreg.node = temp;
			chip->ab.swire_control = of_property_read_bool(temp,
							"qcom,swire-control");
			chip->ab.pd_control = of_property_read_bool(temp,
							"qcom,aod-pd-control");
			break;
		case IBB_PERIPH_TYPE:
			chip->ibb_base = base;
			chip->ibb.subtype = val[1];
			chip->ibb.vreg.node = temp;
			chip->ibb.swire_control = of_property_read_bool(temp,
							"qcom,swire-control");
			chip->ibb.pd_control = of_property_read_bool(temp,
							"qcom,aod-pd-control");
			chip->ibb.single_phase = of_property_read_bool(temp,
							"qcom,ibb-single-phase");
			break;
		default:
			pr_err("Unknown peripheral type 0x%x\n", val[0]);
			return -EINVAL;
		}
	}

	return 0;
}

static int qpnp_amoled_regulator_probe(struct platform_device *pdev)
{
	int rc;
	struct device_node *node;
	struct qpnp_amoled *chip;

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("No nodes defined\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Failed to get the regmap handle\n");
		rc = -EINVAL;
		goto error;
	}

	dev_set_drvdata(&pdev->dev, chip);

	rc = qpnp_amoled_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to parse DT params rc=%d\n", rc);
		goto error;
	}

	rc = qpnp_amoled_hw_init(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to initialize HW rc=%d\n", rc);

error:
	return rc;
}

static int qpnp_amoled_regulator_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id amoled_match_table[] = {
	{ .compatible = "qcom,qpnp-amoled-regulator", },
	{ },
};

static struct platform_driver qpnp_amoled_regulator_driver = {
	.driver		= {
		.name		= "qpnp-amoled-regulator",
		.of_match_table	= amoled_match_table,
	},
	.probe		= qpnp_amoled_regulator_probe,
	.remove		= qpnp_amoled_regulator_remove,
};

static int __init qpnp_amoled_regulator_init(void)
{
	return platform_driver_register(&qpnp_amoled_regulator_driver);
}
arch_initcall(qpnp_amoled_regulator_init);

static void __exit qpnp_amoled_regulator_exit(void)
{
	platform_driver_unregister(&qpnp_amoled_regulator_driver);
}
module_exit(qpnp_amoled_regulator_exit);

MODULE_DESCRIPTION("QPNP AMOLED regulator driver");
MODULE_LICENSE("GPL v2");

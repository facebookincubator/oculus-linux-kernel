// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/types.h>

#define XO_ADJ_FINE_REG		0x5B

struct pm8150_xo {
	struct device *dev;
	struct regmap *regmap;
	u32 baseaddr;
};

static ssize_t xo_adj_fine_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pm8150_xo *xo = dev_get_drvdata(dev);
	int reg_val = 0;
	int rc = 0;

	rc = regmap_read(xo->regmap, xo->baseaddr + XO_ADJ_FINE_REG,
			 &reg_val);
	if (rc < 0) {
		dev_err(xo->dev, "Couldn't read register 0x%x, rc=%d\n",
			xo->baseaddr + XO_ADJ_FINE_REG, rc);
		return rc;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", reg_val);
}

static ssize_t xo_adj_fine_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct pm8150_xo *xo = dev_get_drvdata(dev);
	int rc, val;

	rc = kstrtoint(buf, 0, &val);
	if (rc)
		return rc;

	/**
	 * Register description:
	 *
	 * XO_ADJ_FINE_REG | 0x505B 5:0 XO_ADJUST_CAP RW
	 * XO_ADJUST<5:0>. 6-bit programmability for adjusting
	 * the load capacitance. Adjusts cap between XTAL1_IN_PAD
	 * and XTAL1_OUT_PAD = AdjCode * 0.2pF
	 */

	if (val < 0 || val > 63)
		return -EINVAL;

	rc = regmap_write(xo->regmap, xo->baseaddr + XO_ADJ_FINE_REG, val);
	if (rc < 0) {
		dev_err(xo->dev, "Couldn't read register 0x%x, rc=%d\n",
			xo->baseaddr + XO_ADJ_FINE_REG, rc);
	}

	return size;
}

static DEVICE_ATTR_RW(xo_adj_fine);

static int pm8150_xo_probe(struct platform_device *pdev)
{
	struct pm8150_xo *xo_ctrl;
	int rc;

	xo_ctrl = devm_kzalloc(&pdev->dev, sizeof(*xo_ctrl), GFP_KERNEL);
	if (!xo_ctrl)
		return -ENOMEM;

	xo_ctrl->dev = &pdev->dev;

	xo_ctrl->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!xo_ctrl->regmap) {
		dev_err(&pdev->dev, "failed to locate regmap\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "reg",
				  &xo_ctrl->baseaddr);
	if (rc)
		return rc;

	dev_set_drvdata(xo_ctrl->dev, xo_ctrl);

	device_create_file(xo_ctrl->dev, &dev_attr_xo_adj_fine);

	return 0;
}

static int pm8150_xo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_xo_adj_fine);

	return 0;
}

static const struct of_device_id pm8150_xo_of_match[] = {
	{.compatible = "qcom,pm8150-xo"},
	{},
};

MODULE_DEVICE_TABLE(of, pm8150_xo_of_match);

static struct platform_driver pm8150_xo_driver = {
	.driver = {
		.name = "qcom,pm8150-xo",
		.of_match_table = pm8150_xo_of_match,
	},
	.probe	= pm8150_xo_probe,
	.remove = pm8150_xo_remove,
};

module_platform_driver(pm8150_xo_driver);

MODULE_DESCRIPTION("Access to Qualcomm's PM8150 XO clock parameters");
MODULE_LICENSE("GPL v2");

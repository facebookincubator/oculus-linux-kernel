// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>

struct fpga_data {
	struct device *dev;
	struct regulator *vcciox;
	struct regulator *vcc;
	struct regulator *vccaux;
	int vcciox_voltage_mv;
	int vcc_voltage_mv;
	int vccaux_voltage_mv;
};

static int fpga_regulator_enable(struct fpga_data *ctx)
{
	int rc = 0;

	if (IS_ERR(ctx->vcciox) ||
	    IS_ERR(ctx->vcc) ||
	    IS_ERR(ctx->vccaux))
		/* Do not allow enable unless all regulators are configured */
		return -EINVAL;

	/*
	 * Power sequence specification requires that the supplies be enabled
	 * in the following order:
	 *
	 * VCCIOX -> VCC -> VCCAUX
	 *
	 * VCCIOX must reach 0.6V before VCC or VCCAUX can be enabled.
	 * VCC must be higher than VCCAUX at all times during init.
	 *
	 * Use a 10ms delay between VCCIOX, VCC, and VCCAUX to attempt to
	 * meet the requirements, since we cannot sense voltage to trigger
	 * the next supply in the sequence.
	 */
	rc = regulator_set_voltage(ctx->vcciox, ctx->vcciox_voltage_mv,
		ctx->vcciox_voltage_mv);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to configure VCCIOX voltage: %d", rc);
		return rc;
	}

	rc = regulator_set_load(ctx->vcciox, 75000);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to configure VCCIOX load: %d", rc);
		return rc;
	}

	rc = regulator_enable(ctx->vcciox);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to enable VCCIOX supply: %d", rc);
		return rc;
	}
	usleep_range(50000, 55000);

	rc = regulator_set_voltage(ctx->vcc, ctx->vcc_voltage_mv,
		ctx->vcc_voltage_mv);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to configure VCC voltage: %d", rc);
		goto fail_vcc;
	}

	rc = regulator_set_load(ctx->vcc, 75000);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to configure VCC load: %d", rc);
		goto fail_vcc;
	}

	rc = regulator_enable(ctx->vcc);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to enable VCC supply: %d", rc);
		goto fail_vcc;
	}
	usleep_range(50000, 55000);

	rc = regulator_set_voltage(ctx->vccaux, ctx->vccaux_voltage_mv,
		ctx->vccaux_voltage_mv);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to configure VCCAUX voltage: %d", rc);
		goto fail_vccaux;
	}

	rc = regulator_set_load(ctx->vccaux, 30000);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to configure VCCAUX load: %d", rc);
		goto fail_vccaux;
	}

	rc = regulator_enable(ctx->vccaux);
	if (rc < 0) {
		dev_err(ctx->dev, "Failed to enable VCCAUX supply: %d", rc);
		goto fail_vccaux;
	}

	dev_dbg(ctx->dev, "Enabled regulators");

	return 0;

	/*
	 * Disable any enabled regulators on failure so all 3 can be enabled
	 * together for any subsequent calls to enable.
	 */
fail_vccaux:
	regulator_disable(ctx->vcc);
fail_vcc:
	regulator_disable(ctx->vcciox);
	return rc;
}

static int fpga_regulator_disable(struct fpga_data *ctx)
{
	int rc = 0;

	/*
	 * There is no requirement for sequencing the calls to disable the
	 * power supplies to the FPGA. Just disable them in the reverse order
	 * they were enabled.
	 *
	 * Don't attempt recovery upon failure. If a supply cannot be disabled
	 * there isn't much that can be done at this point.
	 */
	if (!IS_ERR(ctx->vccaux)) {
		rc = regulator_disable(ctx->vccaux);
		if (rc < 0) {
			dev_warn(ctx->dev, "Failed to disable VCCAUX supply");
			return rc;
		}
	}

	if (!IS_ERR(ctx->vcc)) {
		rc = regulator_disable(ctx->vcc);
		if (rc < 0) {
			dev_warn(ctx->dev, "Failed to disable VCC supply");
			return rc;
		}
	}

	if (!IS_ERR(ctx->vcciox)) {
		rc = regulator_disable(ctx->vcciox);
		if (rc < 0) {
			dev_warn(ctx->dev, "Failed to disable VCCIOX supply");
			return rc;
		}
	}

	dev_dbg(ctx->dev, "Disabled regulators");

	return 0;
}

static int fpga_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fpga_data *ctx;
	int rc = 0;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	rc = of_property_read_u32(dev->of_node, "vcciox-voltage-mv",
			&ctx->vcciox_voltage_mv);
	if (rc < 0) {
		dev_err(dev, "Failed to get VCCIOX voltage: %d", rc);
		return rc;
	}

	ctx->vcciox = devm_regulator_get(dev, "vcciox");
	if (IS_ERR(ctx->vcciox)) {
		rc = PTR_ERR(ctx->vcciox);
		dev_err(dev, "Failed to get VCCIOX supply: %d", rc);
		return rc;
	}

	dev_dbg(dev, "Probed VCCIOX supply");

	rc = of_property_read_u32(dev->of_node, "vcc-voltage-mv",
			&ctx->vcc_voltage_mv);
	if (rc < 0) {
		dev_err(dev, "Failed to get VCC voltage: %d", rc);
		return rc;
	}

	ctx->vcc = devm_regulator_get(dev, "vcc");
	if (IS_ERR(ctx->vcc)) {
		rc = PTR_ERR(ctx->vcc);
		dev_err(dev, "Failed to get VCC supply: %d", rc);
		return rc;
	}

	dev_dbg(dev, "Probed VCC supply");

	rc = of_property_read_u32(dev->of_node, "vccaux-voltage-mv",
			&ctx->vccaux_voltage_mv);
	if (rc < 0) {
		dev_err(dev, "Failed to get VCCAUX voltage: %d", rc);
		return rc;
	}

	ctx->vccaux = devm_regulator_get(dev, "vccaux");
	if (IS_ERR(ctx->vccaux)) {
		rc = PTR_ERR(ctx->vccaux);
		dev_err(dev, "Failed to get VCCAUX supply: %d", rc);
		return rc;
	}

	dev_dbg(dev, "Probed VCCAUX supply");

	platform_set_drvdata(pdev, ctx);

	fpga_regulator_enable(ctx);

	return 0;
}

static int fpga_remove(struct platform_device *pdev)
{
	struct fpga_data *ctx = platform_get_drvdata(pdev);

	fpga_regulator_disable(ctx);

	return 0;
}

static const struct of_device_id fpga_of_match[] = {
	{ .compatible = "oculus,fpga", },
	{}
};

MODULE_DEVICE_TABLE(of, fpga_of_match);

static struct platform_driver fpga_driver = {
	.driver = {
		.name = "oculus,fpga",
		.owner = THIS_MODULE,
		.of_match_table = fpga_of_match,
	},
	.probe = fpga_probe,
	.remove = fpga_remove,
};

static int __init fpga_init(void)
{
	platform_driver_register(&fpga_driver);
	return 0;
}

static void __exit fpga_exit(void)
{
	platform_driver_unregister(&fpga_driver);
}

module_init(fpga_init);
module_exit(fpga_exit);

MODULE_DESCRIPTION("FPGA aggregator driver");
MODULE_LICENSE("GPL v2");

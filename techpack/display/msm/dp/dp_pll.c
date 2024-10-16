// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/of_device.h>
#include "dp_debug.h"
#include "dp_pll.h"

struct dp_pll_ver_spec_info {
	u32 revision;
};

static int dp_pll_fill_io(struct dp_pll *pll)
{
	struct dp_parser *parser = pll->parser;

	pll->io.dp_phy = parser->get_io(parser, "dp_phy");
	if (!pll->io.dp_phy) {
		DP_ERR("Invalid dp_phy resource\n");
		return -ENOMEM;
	}

	pll->io.dp_pll = parser->get_io(parser, "dp_pll");
	if (!pll->io.dp_pll) {
		DP_ERR("Invalid dp_pll resource\n");
		return -ENOMEM;
	}

	pll->io.dp_ln_tx0 = parser->get_io(parser, "dp_ln_tx0");
	if (!pll->io.dp_ln_tx0) {
		DP_ERR("Invalid dp_ln_tx1 resource\n");
		return -ENOMEM;
	}

	pll->io.dp_ln_tx1 = parser->get_io(parser, "dp_ln_tx1");
	if (!pll->io.dp_ln_tx1) {
		DP_ERR("Invalid dp_ln_tx1 resource\n");
		return -ENOMEM;
	}

	pll->io.gdsc = parser->get_io(parser, "gdsc");
	if (!pll->io.gdsc) {
		DP_ERR("Invalid gdsc resource\n");
		return -ENOMEM;
	}

	return 0;
}

static int dp_pll_clock_register(struct dp_pll *pll)
{
	int rc;

	switch (pll->revision) {
	case DP_PLL_5NM_V1:
	case DP_PLL_5NM_V2:
	case DP_PLL_7NM:
		rc = dp_pll_clock_register_5nm(pll);
		break;
	case DP_PLL_4NM_V1:
		rc = dp_pll_clock_register_4nm(pll);
	case EDP_PLL_5NM:
		rc = edp_pll_clock_register_5nm(pll);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

static void dp_pll_clock_unregister(struct dp_pll *pll)
{
	switch (pll->revision) {
	case DP_PLL_5NM_V1:
	case DP_PLL_5NM_V2:
	case DP_PLL_7NM:
	case EDP_PLL_5NM:
		dp_pll_clock_unregister_5nm(pll);
		break;
	case DP_PLL_4NM_V1:
		dp_pll_clock_unregister_4nm(pll);
		break;
	default:
		break;
	}
}

int dp_pll_clock_register_helper(struct dp_pll *pll, struct dp_pll_vco_clk *clks, int num_clks)
{
	int rc = 0, i = 0;
	struct platform_device *pdev;
	struct clk *clk;

	if (!pll || !clks) {
		DP_ERR("input not initialized\n");
		return -EINVAL;
	}

	pdev = pll->pdev;

	for (i = 0; i < num_clks; i++) {
		clks[i].priv = pll;

		clk = clk_register(&pdev->dev, &clks[i].hw);
		if (IS_ERR(clk)) {
			DP_ERR("%s registration failed for DP: %d\n",
			clk_hw_get_name(&clks[i].hw), pll->index);
			return -EINVAL;
		}
		pll->clk_data->clks[i] = clk;
	}

	return rc;
}

struct dp_pll *dp_pll_get(struct dp_pll_in *in)
{
	struct dp_pll *pll;
	struct platform_device *pdev;
	struct device_node *node;
	int rc;

	if (!in || !in->pdev || !in->pdev->dev.of_node || !in->parser) {
		DP_ERR("Invalid resource pointers\n");
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(in->pdev->dev.of_node, "qcom,dp-pll", 0);
	if (!node) {
		DP_ERR("couldn't find dp-pll node\n");
		return ERR_PTR(-EINVAL);
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		DP_ERR("couldn't find dp-pll device\n");
		return ERR_PTR(-ENODEV);
	}

	pll = platform_get_drvdata(pdev);
	if (!pll) {
		DP_ERR("couln't find pll\n");
		return ERR_PTR(-ENODEV);
	}

	pll->parser = in->parser;
	pll->aux = in->aux;

	rc = dp_pll_fill_io(pll);
	if (rc)
		return ERR_PTR(rc);

	return pll;
}

void dp_pll_put(struct dp_pll *pll)
{
}

static const struct dp_pll_ver_spec_info dp_pll_5nm_v1 = {
	.revision = DP_PLL_5NM_V1,
};

static const struct dp_pll_ver_spec_info dp_pll_5nm_v2 = {
	.revision = DP_PLL_5NM_V2,
};

static const struct dp_pll_ver_spec_info dp_pll_7nm = {
	.revision = DP_PLL_7NM,
};

static const struct dp_pll_ver_spec_info edp_pll_5nm = {
	.revision = EDP_PLL_5NM,
};

static const struct of_device_id dp_pll_of_match[] = {
	{ .compatible = "qcom,dp-pll-5nm-v1",
	  .data = &dp_pll_5nm_v1,},
	{ .compatible = "qcom,dp-pll-5nm-v2",
	  .data = &dp_pll_5nm_v2,},
	{ .compatible = "qcom,dp-pll-7nm",
	  .data = &dp_pll_7nm,},
	{ .compatible = "qcom,edp-pll-5nm",
	  .data = &edp_pll_5nm,},
	{}
};

static int dp_pll_driver_probe(struct platform_device *pdev)
{
	struct dp_pll *pll;
	const struct of_device_id *id;
	const struct dp_pll_ver_spec_info *ver_info;
	int rc = 0;

	if (!pdev || !pdev->dev.of_node) {
		DP_ERR("pdev not found\n");
		return -ENODEV;
	}

	id = of_match_node(dp_pll_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	ver_info = id->data;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return -ENOMEM;

	pll->pdev = pdev;
	pll->revision = ver_info->revision;
	pll->name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!pll->name)
		pll->name = "dp0";

	pll->ssc_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,ssc-feature-enable");
	pll->bonding_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,bonding-feature-enable");

	rc = dp_pll_clock_register(pll);
	if (rc)
		goto error;

	DP_INFO("revision=%s, ssc_en=%d, bonding_en=%d\n",
			dp_pll_get_revision(pll->revision), pll->ssc_en,
			pll->bonding_en);

	platform_set_drvdata(pdev, pll);
	return 0;

error:
	kfree(pll);
	return rc;
}

static int dp_pll_driver_remove(struct platform_device *pdev)
{
	struct dp_pll *pll = platform_get_drvdata(pdev);

	dp_pll_clock_unregister(pll);

	kfree(pll);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver dp_pll_platform_driver = {
	.probe      = dp_pll_driver_probe,
	.remove     = dp_pll_driver_remove,
	.driver     = {
		.name   = "dp_pll",
		.of_match_table = dp_pll_of_match,
	},
};

void dp_pll_drv_register(void)
{
	platform_driver_register(&dp_pll_platform_driver);
}

void dp_pll_drv_unregister(void)
{
	platform_driver_unregister(&dp_pll_platform_driver);
}

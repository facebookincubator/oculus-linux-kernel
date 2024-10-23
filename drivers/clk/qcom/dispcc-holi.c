// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,dispcc-holi.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "reset.h"
#include "vdd-level-holi.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_DISP_CC_PLL0_OUT_EVEN,
	P_DISP_CC_PLL0_OUT_MAIN,
	P_DSI0_PHY_PLL_OUT_BYTECLK,
	P_DSI0_PHY_PLL_OUT_DSICLK,
	P_GCC_DISP_GPLL0_CLK,
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
};

/* 1120MHz */
static const struct alpha_pll_config disp_cc_pll0_config = {
	.l = 0x3A,
	.cal_l = 0x32,
	.alpha = 0x5555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.test_ctl_val = 0x40000000,
	.test_ctl_hi_val = 0x00000002,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00004805,
};

static struct clk_alpha_pll disp_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct parent_map disp_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data disp_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .fw_name = "dsi0_phy_pll_out_byteclk", .name =
		"dsi0_phy_pll_out_byteclk" },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map disp_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_DISP_CC_PLL0_OUT_MAIN, 1 },
	{ P_GCC_DISP_GPLL0_CLK, 4 },
	{ P_DISP_CC_PLL0_OUT_EVEN, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data disp_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &disp_cc_pll0.clkr.hw },
	{ .fw_name = "gcc_disp_gpll0_clk" },
	{ .hw = &disp_cc_pll0.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map disp_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_GCC_DISP_GPLL0_CLK, 4 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data disp_cc_parent_data_2[] = {
	{ .fw_name = "bi_tcxo" },
	{ .fw_name = "gcc_disp_gpll0_clk" },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map disp_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data disp_cc_parent_data_3[] = {
	{ .fw_name = "bi_tcxo" },
	{ .fw_name = "dsi0_phy_pll_out_dsiclk", .name =
		"dsi0_phy_pll_out_dsiclk" },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map disp_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data disp_cc_parent_data_4[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct freq_tbl ftbl_disp_cc_mdss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_GCC_DISP_GPLL0_CLK, 8, 0, 0),
	F(75000000, P_GCC_DISP_GPLL0_CLK, 4, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_ahb_clk_src = {
	.cmd_rcgr = 0x115c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_mdss_ahb_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_ahb_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 37500000,
			[VDD_NOMINAL] = 75000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_byte0_clk_src = {
	.cmd_rcgr = 0x10c4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_byte0_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 187500000,
			[VDD_LOW] = 300000000,
			[VDD_LOW_L1] = 358000000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_esc0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_esc0_clk_src = {
	.cmd_rcgr = 0x10e0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_mdss_esc0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_esc0_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_mdp_clk_src[] = {
	F(200000000, P_GCC_DISP_GPLL0_CLK, 1.5, 0, 0),
	F(300000000, P_GCC_DISP_GPLL0_CLK, 1, 0, 0),
	F(373333333, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(448000000, P_DISP_CC_PLL0_OUT_MAIN, 2.5, 0, 0),
	F(560000000, P_DISP_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_mdp_clk_src = {
	.cmd_rcgr = 0x107c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_mdp_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_mdp_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000,
			[VDD_LOW] = 300000000,
			[VDD_LOW_L1] = 373333333,
			[VDD_NOMINAL] = 448000000,
			[VDD_HIGH] = 560000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk0_clk_src = {
	.cmd_rcgr = 0x1064,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_pclk0_clk_src",
		.parent_data = disp_cc_parent_data_3,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_pixel_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 525000000,
			[VDD_LOW_L1] = 625000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_rot_clk_src = {
	.cmd_rcgr = 0x1094,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_mdp_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_rot_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000,
			[VDD_LOW] = 300000000,
			[VDD_LOW_L1] = 373333333,
			[VDD_NOMINAL] = 448000000,
			[VDD_HIGH] = 560000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_vsync_clk_src = {
	.cmd_rcgr = 0x10ac,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.freq_tbl = ftbl_disp_cc_mdss_esc0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_vsync_clk_src",
		.parent_data = disp_cc_parent_data_4,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static struct clk_regmap_div disp_cc_mdss_byte0_div_clk_src = {
	.reg = 0x10dc,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_mdss_byte0_div_clk_src",
		.parent_data = &(const struct clk_parent_data){
			.hw = &disp_cc_mdss_byte0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch disp_cc_mdss_ahb_clk = {
	.halt_reg = 0x104c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x104c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_clk = {
	.halt_reg = 0x102c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x102c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_byte0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_intf_clk = {
	.halt_reg = 0x1030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_intf_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_byte0_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc0_clk = {
	.halt_reg = 0x1034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_esc0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_esc0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_clk = {
	.halt_reg = 0x1010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_lut_clk = {
	.halt_reg = 0x1020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x1020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_lut_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_non_gdsc_ahb_clk = {
	.halt_reg = 0x2004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x2004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_non_gdsc_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk0_clk = {
	.halt_reg = 0x100c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x100c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_pclk0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_pclk0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rot_clk = {
	.halt_reg = 0x1018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rot_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_rot_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rscc_ahb_clk = {
	.halt_reg = 0x200c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x200c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rscc_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rscc_vsync_clk = {
	.halt_reg = 0x2008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rscc_vsync_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_vsync_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync_clk = {
	.halt_reg = 0x1028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_vsync_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &disp_cc_mdss_vsync_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_sleep_clk = {
	.halt_reg = 0x5004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_xo_clk = {
	.halt_reg = 0x5008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *disp_cc_holi_clocks[] = {
	[DISP_CC_MDSS_AHB_CLK] = &disp_cc_mdss_ahb_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK_SRC] = &disp_cc_mdss_ahb_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_CLK] = &disp_cc_mdss_byte0_clk.clkr,
	[DISP_CC_MDSS_BYTE0_CLK_SRC] = &disp_cc_mdss_byte0_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_DIV_CLK_SRC] = &disp_cc_mdss_byte0_div_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_INTF_CLK] = &disp_cc_mdss_byte0_intf_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK] = &disp_cc_mdss_esc0_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK_SRC] = &disp_cc_mdss_esc0_clk_src.clkr,
	[DISP_CC_MDSS_MDP_CLK] = &disp_cc_mdss_mdp_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK_SRC] = &disp_cc_mdss_mdp_clk_src.clkr,
	[DISP_CC_MDSS_MDP_LUT_CLK] = &disp_cc_mdss_mdp_lut_clk.clkr,
	[DISP_CC_MDSS_NON_GDSC_AHB_CLK] = &disp_cc_mdss_non_gdsc_ahb_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK] = &disp_cc_mdss_pclk0_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK_SRC] = &disp_cc_mdss_pclk0_clk_src.clkr,
	[DISP_CC_MDSS_ROT_CLK] = &disp_cc_mdss_rot_clk.clkr,
	[DISP_CC_MDSS_ROT_CLK_SRC] = &disp_cc_mdss_rot_clk_src.clkr,
	[DISP_CC_MDSS_RSCC_AHB_CLK] = &disp_cc_mdss_rscc_ahb_clk.clkr,
	[DISP_CC_MDSS_RSCC_VSYNC_CLK] = &disp_cc_mdss_rscc_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK] = &disp_cc_mdss_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK_SRC] = &disp_cc_mdss_vsync_clk_src.clkr,
	[DISP_CC_PLL0] = &disp_cc_pll0.clkr,
	[DISP_CC_SLEEP_CLK] = &disp_cc_sleep_clk.clkr,
	[DISP_CC_XO_CLK] = &disp_cc_xo_clk.clkr,
};

static const struct qcom_reset_map disp_cc_holi_resets[] = {
	[DISP_CC_MDSS_CORE_BCR] = { 0x1000 },
	[DISP_CC_MDSS_RSCC_BCR] = { 0x2000 },
};

static const struct regmap_config disp_cc_holi_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x10000,
	.fast_io = true,
};

static const struct qcom_cc_desc disp_cc_holi_desc = {
	.config = &disp_cc_holi_regmap_config,
	.clks = disp_cc_holi_clocks,
	.num_clks = ARRAY_SIZE(disp_cc_holi_clocks),
	.resets = disp_cc_holi_resets,
	.num_resets = ARRAY_SIZE(disp_cc_holi_resets),
};

static const struct of_device_id disp_cc_holi_match_table[] = {
	{ .compatible = "qcom,holi-dispcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, disp_cc_holi_match_table);

static int disp_cc_holi_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (PTR_ERR(vdd_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	regmap = qcom_cc_map(pdev, &disp_cc_holi_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	clk_fabia_pll_configure(&disp_cc_pll0, regmap, &disp_cc_pll0_config);

	ret = qcom_cc_really_probe(pdev, &disp_cc_holi_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register DISP CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered DISP CC clocks\n");

	return ret;
}

static void disp_cc_holi_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &disp_cc_holi_desc);
}

static struct platform_driver disp_cc_holi_driver = {
	.probe = disp_cc_holi_probe,
	.driver = {
		.name = "disp_cc-holi",
		.of_match_table = disp_cc_holi_match_table,
		.sync_state = disp_cc_holi_sync_state,
	},
};

static int __init disp_cc_holi_init(void)
{
	return platform_driver_register(&disp_cc_holi_driver);
}
subsys_initcall(disp_cc_holi_init);

static void __exit disp_cc_holi_exit(void)
{
	platform_driver_unregister(&disp_cc_holi_driver);
}
module_exit(disp_cc_holi_exit);

MODULE_DESCRIPTION("QTI DISP_CC HOLI Driver");
MODULE_LICENSE("GPL v2");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,gpucc-diwali.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_HIGH + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mxa, VDD_HIGH + 1, 1, vdd_corner);

static struct clk_vdd_class *gpu_cc_diwali_regulators[] = {
	&vdd_cx,
	&vdd_mxa,
};

enum {
	P_BI_TCXO,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_OUT_MAIN,
	P_GPU_CC_PLL1_OUT_MAIN,
};

#define CX_GMU_CBCR_SLEEP_MASK 0xF0

static struct pll_vco lucid_evo_vco[] = {
	{ 249600000, 2000000000, 0 },
};

/* 460MHz Configuration */
static const struct alpha_pll_config gpu_cc_pll0_config = {
	.l = 0x17,
	.cal_l = 0x44,
	.alpha = 0xF555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00182261,
	.config_ctl_hi1_val = 0x32AA299C,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_evo_vco,
	.num_vco = ARRAY_SIZE(lucid_evo_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_EVO],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_evo_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mxa,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER_D1] = 500000000,
				[VDD_LOWER] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 2000000000},
		},
	},
};

/* 1000MHz Configuration */
static const struct alpha_pll_config gpu_cc_pll1_config = {
	.l = 0x34,
	.cal_l = 0x44,
	.alpha = 0x1555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00182261,
	.config_ctl_hi1_val = 0x32AA299C,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x1000,
	.vco_table = lucid_evo_vco,
	.num_vco = ARRAY_SIZE(lucid_evo_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_EVO],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_evo_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mxa,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER_D1] = 500000000,
				[VDD_LOWER] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 2000000000},
		},
	},
};

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .fw_name = "gpll0_out_main" },
	{ .fw_name = "gpll0_out_main_div" },
};

static const struct parent_map gpu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gpll0_out_main" },
	{ .fw_name = "gpll0_out_main_div" },
};

static const struct parent_map gpu_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_2[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gpll0_out_main" },
	{ .fw_name = "gpll0_out_main_div" },
};

static const struct parent_map gpu_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data gpu_cc_parent_data_3[] = {
	{ .fw_name = "bi_tcxo" },
};

static const struct freq_tbl ftbl_gpu_cc_ff_clk_src[] = {
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_ff_clk_src = {
	.cmd_rcgr = 0x9474,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_ff_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_ff_clk_src",
		.parent_data = gpu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_0),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 200000000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	F(500000000, P_GPU_CC_PLL1_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gmu_clk_src = {
	.cmd_rcgr = 0x9318,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_1,
	.freq_tbl = ftbl_gpu_cc_gmu_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_gmu_clk_src",
		.parent_data = gpu_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_1),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_classes = gpu_cc_diwali_regulators,
		.num_vdd_classes = ARRAY_SIZE(gpu_cc_diwali_regulators),
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 200000000,
			[VDD_LOW] = 500000000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_hub_clk_src[] = {
	F(150000000, P_GPLL0_OUT_MAIN_DIV, 2, 0, 0),
	F(240000000, P_GPLL0_OUT_MAIN, 2.5, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_hub_clk_src = {
	.cmd_rcgr = 0x93ec,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_2,
	.freq_tbl = ftbl_gpu_cc_hub_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_hub_clk_src",
		.parent_data = gpu_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_2),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 150000000,
			[VDD_LOW] = 240000000,
			[VDD_NOMINAL] = 300000000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_xo_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_xo_clk_src = {
	.cmd_rcgr = 0x9010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_3,
	.freq_tbl = ftbl_gpu_cc_xo_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_xo_clk_src",
		.parent_data = gpu_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_3),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 19200000},
	},
};

static struct clk_regmap_div gpu_cc_demet_div_clk_src = {
	.reg = 0x9054,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_demet_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_xo_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div gpu_cc_hub_ahb_div_clk_src = {
	.reg = 0x9430,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_hub_ahb_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_hub_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div gpu_cc_hub_cx_int_div_clk_src = {
	.reg = 0x942c,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_hub_cx_int_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_hub_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div gpu_cc_xo_div_clk_src = {
	.reg = 0x9050,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_xo_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_xo_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch gpu_cc_ahb_clk = {
	.halt_reg = 0x911c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x911c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_ahb_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_hub_ahb_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_crc_ahb_clk = {
	.halt_reg = 0x9120,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9120,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_crc_ahb_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_hub_ahb_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_ff_clk = {
	.halt_reg = 0x914c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x914c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_ff_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_ff_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gmu_clk = {
	.halt_reg = 0x913c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x913c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gmu_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_aon_clk = {
	.halt_reg = 0x9004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cxo_aon_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_clk = {
	.halt_reg = 0x9144,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9144,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cxo_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_demet_clk = {
	.halt_reg = 0x900c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x900c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_demet_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_demet_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_freq_measure_clk = {
	.halt_reg = 0x9008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_freq_measure_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_xo_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_hlos1_vote_gpu_smmu_clk = {
	.halt_reg = 0x7000,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x7000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_hlos1_vote_gpu_smmu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_hub_aon_clk = {
	.halt_reg = 0x93e8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x93e8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_hub_aon_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_hub_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_hub_cx_int_clk = {
	.halt_reg = 0x9148,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9148,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_hub_cx_int_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_hub_cx_int_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_memnoc_gfx_clk = {
	.halt_reg = 0x9150,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9150,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_memnoc_gfx_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_mnd1x_0_gfx3d_clk = {
	.halt_reg = 0x9288,
	.halt_check = BRANCH_HALT_SKIP,
	.clkr = {
		.enable_reg = 0x9288,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_mnd1x_0_gfx3d_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_mnd1x_1_gfx3d_clk = {
	.halt_reg = 0x928c,
	.halt_check = BRANCH_HALT_SKIP,
	.clkr = {
		.enable_reg = 0x928c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_mnd1x_1_gfx3d_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_sleep_clk = {
	.halt_reg = 0x9134,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9134,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *gpu_cc_diwali_clocks[] = {
	[GPU_CC_AHB_CLK] = &gpu_cc_ahb_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_FF_CLK] = &gpu_cc_cx_ff_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_DEMET_CLK] = &gpu_cc_demet_clk.clkr,
	[GPU_CC_DEMET_DIV_CLK_SRC] = &gpu_cc_demet_div_clk_src.clkr,
	[GPU_CC_FF_CLK_SRC] = &gpu_cc_ff_clk_src.clkr,
	[GPU_CC_FREQ_MEASURE_CLK] = &gpu_cc_freq_measure_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK] = &gpu_cc_hlos1_vote_gpu_smmu_clk.clkr,
	[GPU_CC_HUB_AHB_DIV_CLK_SRC] = &gpu_cc_hub_ahb_div_clk_src.clkr,
	[GPU_CC_HUB_AON_CLK] = &gpu_cc_hub_aon_clk.clkr,
	[GPU_CC_HUB_CLK_SRC] = &gpu_cc_hub_clk_src.clkr,
	[GPU_CC_HUB_CX_INT_CLK] = &gpu_cc_hub_cx_int_clk.clkr,
	[GPU_CC_HUB_CX_INT_DIV_CLK_SRC] = &gpu_cc_hub_cx_int_div_clk_src.clkr,
	[GPU_CC_MEMNOC_GFX_CLK] = &gpu_cc_memnoc_gfx_clk.clkr,
	[GPU_CC_MND1X_0_GFX3D_CLK] = &gpu_cc_mnd1x_0_gfx3d_clk.clkr,
	[GPU_CC_MND1X_1_GFX3D_CLK] = &gpu_cc_mnd1x_1_gfx3d_clk.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_PLL1] = &gpu_cc_pll1.clkr,
	[GPU_CC_SLEEP_CLK] = &gpu_cc_sleep_clk.clkr,
	[GPU_CC_XO_CLK_SRC] = &gpu_cc_xo_clk_src.clkr,
	[GPU_CC_XO_DIV_CLK_SRC] = &gpu_cc_xo_div_clk_src.clkr,
};

static const struct qcom_reset_map gpu_cc_diwali_resets[] = {
	[GPUCC_GPU_CC_ACD_BCR] = { 0x9358 },
	[GPUCC_GPU_CC_CX_BCR] = { 0x9104 },
	[GPUCC_GPU_CC_FAST_HUB_BCR] = { 0x93e4 },
	[GPUCC_GPU_CC_FF_BCR] = { 0x9470 },
	[GPUCC_GPU_CC_GFX3D_AON_BCR] = { 0x9198 },
	[GPUCC_GPU_CC_GMU_BCR] = { 0x9314 },
	[GPUCC_GPU_CC_GX_BCR] = { 0x9058 },
	[GPUCC_GPU_CC_XO_BCR] = { 0x9000 },
	[GPUCC_GPU_CC_FREQUENCY_LIMITER_IRQ_CLEAR] = { 0x9538, 0 },
};

static const struct regmap_config gpu_cc_diwali_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x98b4,
	.fast_io = true,
};

static const struct qcom_cc_desc gpu_cc_diwali_desc = {
	.config = &gpu_cc_diwali_regmap_config,
	.clks = gpu_cc_diwali_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_diwali_clocks),
	.resets = gpu_cc_diwali_resets,
	.num_resets = ARRAY_SIZE(gpu_cc_diwali_resets),
	.clk_regulators = gpu_cc_diwali_regulators,
	.num_clk_regulators = ARRAY_SIZE(gpu_cc_diwali_regulators),
};

static const struct of_device_id gpu_cc_diwali_match_table[] = {
	{ .compatible = "qcom,diwali-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_diwali_match_table);

static int gpu_cc_diwali_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &gpu_cc_diwali_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	clk_lucid_evo_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);
	clk_lucid_evo_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll1_config);

	regmap_write(regmap, 0x9534, 0x0);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_diwali_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	/*
	 * Workaround to avoid possibility of in rush current issue due to
	 * AXI and GMU sleep staggering overlap in power down sequence.
	 */
	regmap_update_bits(regmap, gpu_cc_cx_gmu_clk.clkr.enable_reg,
			CX_GMU_CBCR_SLEEP_MASK, CX_GMU_CBCR_SLEEP_MASK);

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");

	return ret;
}

static void gpu_cc_diwali_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &gpu_cc_diwali_desc);
}

static struct platform_driver gpu_cc_diwali_driver = {
	.probe = gpu_cc_diwali_probe,
	.driver = {
		.name = "gpu_cc-diwali",
		.of_match_table = gpu_cc_diwali_match_table,
		.sync_state = gpu_cc_diwali_sync_state,
	},
};

static int __init gpu_cc_diwali_init(void)
{
	return platform_driver_register(&gpu_cc_diwali_driver);
}
subsys_initcall(gpu_cc_diwali_init);

static void __exit gpu_cc_diwali_exit(void)
{
	platform_driver_unregister(&gpu_cc_diwali_driver);
}
module_exit(gpu_cc_diwali_exit);

MODULE_DESCRIPTION("QTI GPU_CC DIWALI Driver");
MODULE_LICENSE("GPL v2");

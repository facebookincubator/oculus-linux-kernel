// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <linux/pm_clock.h>

#include <dt-bindings/clock/qcom,scuba-lpassaoncc.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_lpi_cx, VDD_NUM + 1, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_LPASS_AON_CC_PLL_OUT_AUX,
	P_LPASS_AON_CC_PLL_OUT_AUX2,
	P_LPASS_AON_CC_PLL_OUT_EARLY,
	P_LPASS_AUDIO_CC_PLL_MAIN_DIV_CLK,
	P_LPASS_CORE_CC_PLL_ODD_CLK,
};

static const struct parent_map lpass_aon_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_LPASS_AON_CC_PLL_OUT_EARLY, 2 },
};

static const char * const lpass_aon_cc_parent_names_0[] = {
	"bi_tcxo",
	"lpass_aon_cc_pll",
};

static const struct parent_map lpass_aon_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_BI_TCXO, 4 },
};

static const char * const lpass_aon_cc_parent_names_1_ao[] = {
	"bi_tcxo_ao",
	"bi_tcxo",
};

static const struct parent_map lpass_aon_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_LPASS_AON_CC_PLL_OUT_AUX2, 4 },
};

static const char * const lpass_aon_cc_parent_names_2[] = {
	"bi_tcxo",
	"lpass_aon_cc_pll_out_aux2",
};

static const struct parent_map lpass_aon_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_LPASS_AON_CC_PLL_OUT_AUX, 1 },
	{ P_LPASS_CORE_CC_PLL_ODD_CLK, 2 },
	{ P_LPASS_AUDIO_CC_PLL_MAIN_DIV_CLK, 6 },
};

static const char * const lpass_aon_cc_parent_names_3[] = {
	"bi_tcxo",
	"lpass_aon_cc_pll_out_aux",
	"lpass_core_cc_pll_odd_clk",
	"lpass_audio_cc_pll_main_div_clk",
};

static const struct parent_map lpass_aon_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
};

static const char * const lpass_aon_cc_parent_names_4[] = {
	"bi_tcxo",
};

static const struct pll_vco aoncc_pll_vco[] = {
	{ 1000000000, 2000000000, 0 },
	{ 750000000, 1500000000, 1 },
	{ 500000000, 1000000000, 2 },
	{ 250000000, 500000000, 3 },
};

/* 614.4 MHz Configuration */
static const struct alpha_pll_config lpass_aon_cc_pll_config = {
	.l = 0x20,
	.config_ctl_val = 0x4001055b,
	.user_ctl_val = 0x200101,
	.user_ctl_hi_val = 0x4,
	.test_ctl_val = 0,
	.test_ctl_hi_val = 0x1,
	.main_output_mask = BIT(0),
	.aux_output_mask = BIT(1),
	.aux2_output_mask = BIT(2),
	.early_output_mask = BIT(3),
	.post_div_val = 0x281 << 8,
	.post_div_mask = GENMASK(17, 8),
	.vco_val = 0x2 << 20,
	.vco_mask = GENMASK(21, 20),
	.test_ctl_mask = GENMASK(31, 0),
	.test_ctl_hi_mask = 0x1,
};

static struct clk_alpha_pll lpass_aon_cc_pll = {
	.offset = 0x0,
	.vco_table = aoncc_pll_vco,
	.num_vco = ARRAY_SIZE(aoncc_pll_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_pll",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			.vdd_class = &vdd_lpi_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 1000000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct clk_div_table post_div_table_lpass_aon_cc_pll_out_aux[] = {
	{ 0x5, 5 },
	{ }
};

static struct clk_alpha_pll_postdiv lpass_aon_cc_pll_out_aux = {
	.offset = 0x0,
	.post_div_shift = 15,
	.post_div_table = post_div_table_lpass_aon_cc_pll_out_aux,
	.num_post_div = ARRAY_SIZE(post_div_table_lpass_aon_cc_pll_out_aux),
	.width = 3,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_pll_out_aux",
		.parent_names = (const char *[]){ "lpass_aon_cc_pll" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_fabia_ops,
	},
};

static const struct clk_div_table post_div_table_lpass_aon_cc_pll_out_aux2[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv lpass_aon_cc_pll_out_aux2 = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_lpass_aon_cc_pll_out_aux2,
	.num_post_div = ARRAY_SIZE(post_div_table_lpass_aon_cc_pll_out_aux2),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_pll_out_aux2",
		.parent_names = (const char *[]){ "lpass_aon_cc_pll" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_fabia_ops,
	},
};

static struct clk_regmap_div lpass_aon_cc_cdiv_tx_mclk_div_clk_src = {
	.reg = 0x13010,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_cdiv_tx_mclk_div_clk_src",
		.parent_names = (const char *[])
				{ "lpass_aon_cc_tx_mclk_rcg_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div lpass_aon_cc_cdiv_va_div_clk_src = {
	.reg = 0x12010,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_cdiv_va_div_clk_src",
		.parent_names = (const char *[])
				{ "lpass_aon_cc_va_rcg_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static const struct freq_tbl ftbl_lpass_aon_cc_cpr_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_aon_cc_cpr_clk_src = {
	.cmd_rcgr = 0x2004,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_1,
	.freq_tbl = ftbl_lpass_aon_cc_cpr_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_cpr_clk_src",
		.parent_names = lpass_aon_cc_parent_names_1_ao,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_1_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_lpass_aon_cc_main_rcg_clk_src[] = {
	F(38400000, P_LPASS_AON_CC_PLL_OUT_AUX2, 8, 0, 0),
	F(76800000, P_LPASS_AON_CC_PLL_OUT_AUX2, 4, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_aon_cc_main_rcg_clk_src = {
	.cmd_rcgr = 0x1000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_2,
	.freq_tbl = ftbl_lpass_aon_cc_main_rcg_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_main_rcg_clk_src",
		.parent_names = lpass_aon_cc_parent_names_2,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 38400000,
			[VDD_NOMINAL] = 76800000},
	},
};

static struct clk_rcg2 lpass_aon_cc_q6_xo_clk_src = {
	.cmd_rcgr = 0x8004,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_1,
	.freq_tbl = ftbl_lpass_aon_cc_cpr_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_q6_xo_clk_src",
		.parent_names = lpass_aon_cc_parent_names_1_ao,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_1_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_lpass_aon_cc_tx_mclk_rcg_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(24576000, P_LPASS_AON_CC_PLL_OUT_AUX, 5, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_aon_cc_tx_mclk_rcg_clk_src = {
	.cmd_rcgr = 0x13004,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_3,
	.freq_tbl = ftbl_lpass_aon_cc_tx_mclk_rcg_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_tx_mclk_rcg_clk_src",
		.parent_names = lpass_aon_cc_parent_names_3,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_3),
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 24576000},
	},
};

static const struct freq_tbl ftbl_lpass_aon_cc_va_rcg_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_aon_cc_va_rcg_clk_src = {
	.cmd_rcgr = 0x12004,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_4,
	.freq_tbl = ftbl_lpass_aon_cc_va_rcg_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_va_rcg_clk_src",
		.parent_names = lpass_aon_cc_parent_names_4,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_4),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static const struct freq_tbl ftbl_lpass_aon_cc_vs_vddcx_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(614400000, P_LPASS_AON_CC_PLL_OUT_EARLY, 1, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_aon_cc_vs_vddcx_clk_src = {
	.cmd_rcgr = 0x15010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_aon_cc_vs_vddcx_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_vs_vddcx_clk_src",
		.parent_names = lpass_aon_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 614400000},
	},
};

static struct clk_rcg2 lpass_aon_cc_vs_vddmx_clk_src = {
	.cmd_rcgr = 0x15000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = lpass_aon_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_aon_cc_vs_vddcx_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_aon_cc_vs_vddmx_clk_src",
		.parent_names = lpass_aon_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_aon_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 614400000},
	},
};

static struct clk_branch lpass_aon_cc_ahb_timeout_clk = {
	.halt_reg = 0x9030,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_ahb_timeout_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_aon_h_clk = {
	.halt_reg = 0x903c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x903c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_aon_h_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_bus_alt_clk = {
	.halt_reg = 0x9048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9048,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_bus_alt_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_csr_h_clk = {
	.halt_reg = 0x9010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9010,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_csr_h_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_mcc_access_clk = {
	.halt_reg = 0x904c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x904c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_mcc_access_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_pdc_h_clk = {
	.halt_reg = 0x900c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x900c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_pdc_h_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_q6_atbm_clk = {
	.halt_reg = 0xa010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa010,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_q6_atbm_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_qsm_xo_clk = {
	.halt_reg = 0x6000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_qsm_xo_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_q6_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_rsc_hclk_clk = {
	.halt_reg = 0x9078,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x9078,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x9078,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_rsc_hclk_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_sleep_clk = {
	.halt_reg = 0x10004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_ssc_h_clk = {
	.halt_reg = 0x9040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9040,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_ssc_h_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_tx_mclk_2x_clk = {
	.halt_reg = 0x1300c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1300c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_tx_mclk_2x_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_tx_mclk_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_tx_mclk_clk = {
	.halt_reg = 0x13014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x13014,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_tx_mclk_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_cdiv_tx_mclk_div_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_va_2x_clk = {
	.halt_reg = 0x1200c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1200c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_va_2x_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_va_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_va_clk = {
	.halt_reg = 0x12014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x12014,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_va_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_cdiv_va_div_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_va_mem0_clk = {
	.halt_reg = 0x9028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9028,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_va_mem0_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_vs_vddcx_clk = {
	.halt_reg = 0x15018,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x15018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_vs_vddcx_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_vs_vddcx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_aon_cc_vs_vddmx_clk = {
	.halt_reg = 0x15008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x15008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_aon_cc_vs_vddmx_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_vs_vddmx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_qdsp6ss_sleep_clk = {
	.halt_reg = 0x3c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x3c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_qdsp6ss_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_qdsp6ss_xo_clk = {
	.halt_reg = 0x38,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x38,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_qdsp6ss_xo_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *lpass_aon_cc_scuba_clocks[] = {
	[LPASS_AON_CC_AHB_TIMEOUT_CLK] = &lpass_aon_cc_ahb_timeout_clk.clkr,
	[LPASS_AON_CC_AON_H_CLK] = &lpass_aon_cc_aon_h_clk.clkr,
	[LPASS_AON_CC_BUS_ALT_CLK] = &lpass_aon_cc_bus_alt_clk.clkr,
	[LPASS_AON_CC_CDIV_TX_MCLK_DIV_CLK_SRC] =
				&lpass_aon_cc_cdiv_tx_mclk_div_clk_src.clkr,
	[LPASS_AON_CC_CDIV_VA_DIV_CLK_SRC] =
				&lpass_aon_cc_cdiv_va_div_clk_src.clkr,
	[LPASS_AON_CC_CPR_CLK_SRC] = &lpass_aon_cc_cpr_clk_src.clkr,
	[LPASS_AON_CC_CSR_H_CLK] = &lpass_aon_cc_csr_h_clk.clkr,
	[LPASS_AON_CC_MAIN_RCG_CLK_SRC] = &lpass_aon_cc_main_rcg_clk_src.clkr,
	[LPASS_AON_CC_MCC_ACCESS_CLK] = &lpass_aon_cc_mcc_access_clk.clkr,
	[LPASS_AON_CC_PDC_H_CLK] = &lpass_aon_cc_pdc_h_clk.clkr,
	[LPASS_AON_CC_PLL] = &lpass_aon_cc_pll.clkr,
	[LPASS_AON_CC_PLL_OUT_AUX] = &lpass_aon_cc_pll_out_aux.clkr,
	[LPASS_AON_CC_PLL_OUT_AUX2] = &lpass_aon_cc_pll_out_aux2.clkr,
	[LPASS_AON_CC_Q6_ATBM_CLK] = &lpass_aon_cc_q6_atbm_clk.clkr,
	[LPASS_AON_CC_Q6_XO_CLK_SRC] = &lpass_aon_cc_q6_xo_clk_src.clkr,
	[LPASS_AON_CC_QSM_XO_CLK] = &lpass_aon_cc_qsm_xo_clk.clkr,
	[LPASS_AON_CC_RSC_HCLK_CLK] = &lpass_aon_cc_rsc_hclk_clk.clkr,
	[LPASS_AON_CC_SLEEP_CLK] = &lpass_aon_cc_sleep_clk.clkr,
	[LPASS_AON_CC_SSC_H_CLK] = &lpass_aon_cc_ssc_h_clk.clkr,
	[LPASS_AON_CC_TX_MCLK_2X_CLK] = &lpass_aon_cc_tx_mclk_2x_clk.clkr,
	[LPASS_AON_CC_TX_MCLK_CLK] = &lpass_aon_cc_tx_mclk_clk.clkr,
	[LPASS_AON_CC_TX_MCLK_RCG_CLK_SRC] =
					&lpass_aon_cc_tx_mclk_rcg_clk_src.clkr,
	[LPASS_AON_CC_VA_2X_CLK] = &lpass_aon_cc_va_2x_clk.clkr,
	[LPASS_AON_CC_VA_CLK] = &lpass_aon_cc_va_clk.clkr,
	[LPASS_AON_CC_VA_MEM0_CLK] = &lpass_aon_cc_va_mem0_clk.clkr,
	[LPASS_AON_CC_VA_RCG_CLK_SRC] = &lpass_aon_cc_va_rcg_clk_src.clkr,
	[LPASS_AON_CC_VS_VDDCX_CLK] = &lpass_aon_cc_vs_vddcx_clk.clkr,
	[LPASS_AON_CC_VS_VDDCX_CLK_SRC] = &lpass_aon_cc_vs_vddcx_clk_src.clkr,
	[LPASS_AON_CC_VS_VDDMX_CLK] = &lpass_aon_cc_vs_vddmx_clk.clkr,
	[LPASS_AON_CC_VS_VDDMX_CLK_SRC] = &lpass_aon_cc_vs_vddmx_clk_src.clkr,
	[LPASS_QDSP6SS_SLEEP_CLK] = &lpass_qdsp6ss_sleep_clk.clkr,
	[LPASS_QDSP6SS_XO_CLK] = &lpass_qdsp6ss_xo_clk.clkr,
};

static const struct regmap_config lpass_aon_cc_scuba_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x20008,
	.fast_io = true,
};

static const struct qcom_cc_desc lpass_aon_cc_scuba_desc = {
	.config = &lpass_aon_cc_scuba_regmap_config,
	.clks = lpass_aon_cc_scuba_clocks,
	.num_clks = ARRAY_SIZE(lpass_aon_cc_scuba_clocks),
};

static const struct of_device_id lpass_aon_cc_scuba_match_table[] = {
	{ .compatible = "qcom,lpassaoncc-scuba" },
	{ }
};
MODULE_DEVICE_TABLE(of, lpass_aon_cc_scuba_match_table);

static int lpass_aon_cc_scuba_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	vdd_lpi_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_lpi_cx");
	if (IS_ERR(vdd_lpi_cx.regulator[0])) {
		if (PTR_ERR(vdd_lpi_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_lpi_cx regulator\n");
		return PTR_ERR(vdd_lpi_cx.regulator[0]);
	}

	pm_runtime_enable(&pdev->dev);
	ret = pm_clk_create(&pdev->dev);
	if (ret)
		return ret;

	ret = pm_clk_add(&pdev->dev, "iface_clk");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to acquire gcc sway clock\n");
		goto err_destroy_pm_clk;
	}

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret)
		goto err_destroy_pm_clk;

	regmap = qcom_cc_map(pdev, &lpass_aon_cc_scuba_desc);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		goto err_put_rpm;
	}

	clk_alpha_pll_configure(&lpass_aon_cc_pll, regmap,
					&lpass_aon_cc_pll_config);

	/*
	 * Keep clocks always enabled:
	 *	lpass_aon_cc_q6_ahbs_clk
	 *	lpass_aon_cc_q6_ahbm_clk
	 */
	regmap_update_bits(regmap, 0x9020, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x901C, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &lpass_aon_cc_scuba_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Register Fail LPASS Aon clocks ret=%d\n",
					ret);
		goto err_put_rpm;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered LPASS Aon clocks\n");
	return 0;

err_put_rpm:
	pm_runtime_put_sync(&pdev->dev);
err_destroy_pm_clk:
	pm_clk_destroy(&pdev->dev);
	return ret;
}

static const struct dev_pm_ops lpass_aon_cc_scuba_pm_ops = {
	SET_RUNTIME_PM_OPS(pm_clk_suspend, pm_clk_resume, NULL)
};

static struct platform_driver lpass_aon_cc_scuba_driver = {
	.probe = lpass_aon_cc_scuba_probe,
	.driver = {
		.name = "lpassaoncc-scuba",
		.of_match_table = lpass_aon_cc_scuba_match_table,
		.pm = &lpass_aon_cc_scuba_pm_ops,
	},
};

static int __init lpass_aon_cc_scuba_init(void)
{
	return platform_driver_register(&lpass_aon_cc_scuba_driver);
}
subsys_initcall(lpass_aon_cc_scuba_init);

static void __exit lpass_aon_cc_scuba_exit(void)
{
	platform_driver_unregister(&lpass_aon_cc_scuba_driver);
}
module_exit(lpass_aon_cc_scuba_exit);

MODULE_DESCRIPTION("QTI LPASSAONCC SCUBA Driver");
MODULE_LICENSE("GPL v2");

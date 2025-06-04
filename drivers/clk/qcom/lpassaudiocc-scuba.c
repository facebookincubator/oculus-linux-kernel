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
#include <dt-bindings/clock/qcom,scuba-lpassaudiocc.h>

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
	P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX,
	P_LPASS_AUDIO_CC_PLL_OUT_AUX,
	P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC,
};

static const struct parent_map lpass_audio_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_LPASS_AUDIO_CC_PLL_OUT_AUX, 3 },
	{ P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX, 4 },
	{ P_LPASS_AON_CC_PLL_OUT_AUX, 5 },
	{ P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 6 },
};

static const char * const lpass_audio_cc_parent_names_0[] = {
	"bi_tcxo",
	"lpass_audio_cc_pll_out_aux",
	"lpass_audio_cc_dig_pll_out_aux",
	"lpass_aon_cc_pll_out_aux",
	"lpass_audio_cc_pll_out_aux2_div_clk_src",
};

static const struct parent_map lpass_audio_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX, 4 },
	{ P_LPASS_AON_CC_PLL_OUT_AUX, 5 },
};

static const char * const lpass_audio_cc_parent_names_1[] = {
	"bi_tcxo",
	"lpass_audio_cc_dig_pll_out_aux",
	"lpass_aon_cc_pll_out_aux",
};

static const struct parent_map lpass_audio_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX, 4 },
	{ P_LPASS_AON_CC_PLL_OUT_AUX, 5 },
	{ P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 6 },
};

static const char * const lpass_audio_cc_parent_names_2[] = {
	"bi_tcxo",
	"lpass_audio_cc_dig_pll_out_aux",
	"lpass_aon_cc_pll_out_aux",
	"lpass_audio_cc_pll_out_aux2_div_clk_src",
};

static const u8 clk_alpha_pll_regs_offset[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_BRAMMO] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x14,
		[PLL_OFF_TEST_CTL] = 0x18,
		[PLL_OFF_TEST_CTL_U] = 0x1c,
		[PLL_OFF_STATUS] = 0x20,
	},
};

static const struct pll_vco audiocc_pll_vco[] = {
	{ 500000000, 1250000000, 0 },
};

static const struct pll_vco audiocc_dig_pll_vco[] = {
	{ 1000000000, 2000000000, 0 },
	{ 750000000, 1500000000, 1 },
	{ 500000000, 1000000000, 2 },
	{ 250000000, 500000000, 3 },
};

/* 614.4 MHz Configuration */
static const struct alpha_pll_config lpass_audio_cc_dig_pll_config = {
	.l = 0x20,
	.config_ctl_val = 0x4001055b,
	.user_ctl_val = 0x00200101,
	.user_ctl_hi_val = 0x4,
	.test_ctl_hi_val = 0x1,
	.main_output_mask = BIT(0),
	.aux_output_mask = BIT(1),
	.post_div_val = 0x1 << 8,
	.post_div_mask = GENMASK(11, 8),
	.vco_val = 0x1 << 21,
	.vco_mask = GENMASK(21, 20),
	.test_ctl_hi_mask = 0x1,
};

static struct clk_alpha_pll lpass_audio_cc_dig_pll = {
	.offset = 0x3e8,
	.vco_table = audiocc_dig_pll_vco,
	.num_vco = ARRAY_SIZE(audiocc_dig_pll_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_dig_pll",
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

static const struct clk_div_table post_div_table_audio_cc_dig_pll_out_aux[] = {
	{ 0x5, 5 },
	{ }
};

static struct clk_alpha_pll_postdiv lpass_audio_cc_dig_pll_out_aux = {
	.offset = 0x3e8,
	.post_div_shift = 15,
	.post_div_table = post_div_table_audio_cc_dig_pll_out_aux,
	.num_post_div = ARRAY_SIZE(post_div_table_audio_cc_dig_pll_out_aux),
	.width = 3,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_dig_pll_out_aux",
		.parent_names = (const char *[]){ "lpass_audio_cc_dig_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_ops,
	},
};

/* 1128.96 MHz Configuration */
static const struct alpha_pll_config lpass_audio_cc_pll_config = {
	.l = 0x3a,
	.alpha = 0xcccccccc,
	.alpha_hi = 0xcc,
	.post_div_val = 0x1 << 8,
	.post_div_mask = GENMASK(9, 8),
	.config_ctl_val = 0x00004289,
	.test_ctl_val = 0x08000000,
	.main_output_mask = BIT(0),
	.aux_output_mask = BIT(1),
	.aux2_output_mask = BIT(2),
	.early_output_mask = BIT(3),
	.alpha_en_mask = BIT(24),
	.test_ctl_mask = GENMASK(31, 0),
};

static struct clk_alpha_pll lpass_audio_cc_pll = {
	.offset = 0x7d0,
	.vco_table = audiocc_pll_vco,
	.num_vco = ARRAY_SIZE(audiocc_pll_vco),
	.regs = clk_alpha_pll_regs_offset[CLK_ALPHA_PLL_TYPE_BRAMMO],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_pll",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			.vdd_class = &vdd_lpi_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER] = 1250000000,
				[VDD_LOW] = 1250000000,
				[VDD_NOMINAL] = 1250000000},
		},
	},
};

static const struct clk_div_table post_div_table_audio_cc_pll_out_aux[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv lpass_audio_cc_pll_out_aux = {
	.offset = 0x7d0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_audio_cc_pll_out_aux,
	.num_post_div = ARRAY_SIZE(post_div_table_audio_cc_pll_out_aux),
	.width = 2,
	.regs = clk_alpha_pll_regs_offset[CLK_ALPHA_PLL_TYPE_BRAMMO],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_pll_out_aux",
		.parent_names = (const char *[]){ "lpass_audio_cc_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_ro_ops,
	},
};

static const struct clk_div_table post_div_table_audio_cc_pll_out_aux2[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv lpass_audio_cc_pll_out_aux2 = {
	.offset = 0x7d0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_audio_cc_pll_out_aux2,
	.num_post_div = ARRAY_SIZE(post_div_table_audio_cc_pll_out_aux2),
	.width = 2,
	.regs = clk_alpha_pll_regs_offset[CLK_ALPHA_PLL_TYPE_BRAMMO],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_pll_out_aux2",
		.parent_names = (const char *[]){ "lpass_audio_cc_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_ro_ops,
	},
};

static struct clk_regmap_div lpass_audio_cc_cdiv_rx_mclk_div_clk_src = {
	.reg = 0x240d0,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_cdiv_rx_mclk_div_clk_src",
		.parent_names = (const char *[])
					{ "lpass_audio_cc_rx_mclk_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div lpass_audio_cc_cdiv_wsa_mclk_div_clk_src = {
	.reg = 0x220d0,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_cdiv_wsa_mclk_div_clk_src",
		.parent_names = (const char *[])
					{ "lpass_audio_cc_wsa_mclk_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div lpass_audio_cc_pll_out_aux2_div_clk_src = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_pll_out_aux2_div_clk_src",
		.parent_names = (const char *[])
					{ "lpass_audio_cc_pll_out_aux2" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static const struct freq_tbl ftbl_aud_slimbus_clk_src[] = {
	F(6144000, P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX, 10, 1, 2),
	F(12288000, P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX, 10, 0, 0),
	F(24576000, P_LPASS_AUDIO_CC_DIG_PLL_OUT_AUX, 5, 0, 0),
	{ }
};

static struct clk_rcg2 aud_slimbus_clk_src = {
	.cmd_rcgr = 0x17000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_1,
	.freq_tbl = ftbl_aud_slimbus_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "aud_slimbus_clk_src",
		.parent_names = lpass_audio_cc_parent_names_1,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 24576000},
	},
};

static const struct freq_tbl ftbl_lpass_audio_cc_ext_if1_clk_src[] = {
	F(256000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 32),
	F(352800, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 32),
	F(512000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 16),
	F(705600, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 16),
	F(768000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 16),
	F(1024000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 8),
	F(1411200, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 8),
	F(1536000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 8),
	F(2048000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 4),
	F(2822400, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 4),
	F(3072000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 4),
	F(4096000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 2),
	F(5644800, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 2),
	F(6144000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 2),
	F(8192000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 0, 0),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(11289600, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 0, 0),
	F(12288000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_audio_cc_ext_if1_clk_src = {
	.cmd_rcgr = 0x10004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if1_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_ext_if1_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 1536000,
			[VDD_LOWER] = 3072000,
			[VDD_LOW] = 6144000,
			[VDD_NOMINAL] = 12288000},
	},
};

static const struct freq_tbl ftbl_lpass_audio_cc_ext_if2_clk_src[] = {
	F(256000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 32),
	F(352800, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 32),
	F(512000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 16),
	F(705600, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 16),
	F(768000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 16),
	F(1024000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 8),
	F(1411200, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 8),
	F(1536000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 8),
	F(2048000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 4),
	F(2822400, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 4),
	F(3072000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 4),
	F(4096000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 1, 2),
	F(5644800, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 1, 2),
	F(6144000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 1, 2),
	F(8192000, P_LPASS_AON_CC_PLL_OUT_AUX, 15, 0, 0),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(11289600, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 10, 0, 0),
	F(12288000, P_LPASS_AON_CC_PLL_OUT_AUX, 10, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(22579200, P_LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC, 5, 0, 0),
	F(24576000, P_LPASS_AON_CC_PLL_OUT_AUX, 5, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_audio_cc_ext_if2_clk_src = {
	.cmd_rcgr = 0x11004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if2_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_ext_if2_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 3072000,
			[VDD_LOWER] = 6144000,
			[VDD_LOW] = 12288000,
			[VDD_NOMINAL] = 24576000},
	},
};

static struct clk_rcg2 lpass_audio_cc_ext_if3_clk_src = {
	.cmd_rcgr = 0x12004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if1_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_ext_if3_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 1536000,
			[VDD_LOWER] = 3072000,
			[VDD_LOW] = 6144000,
			[VDD_NOMINAL] = 12288000},
	},
};

static struct clk_rcg2 lpass_audio_cc_ext_if4_clk_src = {
	.cmd_rcgr = 0x13008,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if1_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_ext_if4_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 1536000,
			[VDD_LOWER] = 3072000,
			[VDD_LOW] = 6144000,
			[VDD_NOMINAL] = 12288000},
	},
};

static struct clk_rcg2 lpass_audio_cc_ext_mclk0_clk_src = {
	.cmd_rcgr = 0x20004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if2_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_ext_mclk0_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 24576000},
	},
};

static struct clk_rcg2 lpass_audio_cc_ext_mclk1_clk_src = {
	.cmd_rcgr = 0x21004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if2_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_ext_mclk1_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 24576000},
	},
};

static const struct freq_tbl ftbl_lpass_audio_cc_lpaif_pcmoe_clk_src[] = {
	F(15360000, P_LPASS_AON_CC_PLL_OUT_AUX, 8, 0, 0),
	F(30720000, P_LPASS_AON_CC_PLL_OUT_AUX, 4, 0, 0),
	F(61440000, P_LPASS_AON_CC_PLL_OUT_AUX, 2, 0, 0),
	F(122880000, P_LPASS_AON_CC_PLL_OUT_AUX, 1, 0, 0),
	{ }
};

static struct clk_rcg2 lpass_audio_cc_lpaif_pcmoe_clk_src = {
	.cmd_rcgr = 0x19004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_2,
	.freq_tbl = ftbl_lpass_audio_cc_lpaif_pcmoe_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_lpaif_pcmoe_clk_src",
		.parent_names = lpass_audio_cc_parent_names_2,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 122880000},
	},
};

static struct clk_rcg2 lpass_audio_cc_rx_mclk_clk_src = {
	.cmd_rcgr = 0x24004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if2_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_rx_mclk_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 24576000},
	},
};

static struct clk_rcg2 lpass_audio_cc_wsa_mclk_clk_src = {
	.cmd_rcgr = 0x22004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = lpass_audio_cc_parent_map_0,
	.freq_tbl = ftbl_lpass_audio_cc_ext_if2_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "lpass_audio_cc_wsa_mclk_clk_src",
		.parent_names = lpass_audio_cc_parent_names_0,
		.num_parents = ARRAY_SIZE(lpass_audio_cc_parent_names_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_lpi_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 24576000},
	},
};

static struct clk_branch lpass_audio_cc_aud_slimbus_clk = {
	.halt_reg = 0x17014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x17014,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_aud_slimbus_clk",
			.parent_names = (const char *[]){
				"aud_slimbus_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_aud_slimbus_core_clk = {
	.halt_reg = 0x1e018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_aud_slimbus_core_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_aud_slimbus_npl_clk = {
	.halt_reg = 0x1701c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x1701c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1701c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_aud_slimbus_npl_clk",
			.parent_names = (const char *[]){
				"aud_slimbus_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_bus_clk = {
	.halt_reg = 0x1f000,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x1f000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_bus_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_bus_timeout_clk = {
	.halt_reg = 0x1e014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e014,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_bus_timeout_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_codec_mem0_clk = {
	.halt_reg = 0x1e004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_codec_mem0_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_codec_mem1_clk = {
	.halt_reg = 0x1e008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_codec_mem1_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_codec_mem2_clk = {
	.halt_reg = 0x1e00c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e00c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_codec_mem2_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_codec_mem3_clk = {
	.halt_reg = 0x1e010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e010,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_codec_mem3_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_codec_mem_clk = {
	.halt_reg = 0x1e000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1e000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_codec_mem_clk",
			.parent_names = (const char *[]){
				"lpass_aon_cc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if1_ebit_clk = {
	.halt_reg = 0x10020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10020,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if1_ebit_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if1_ibit_clk = {
	.halt_reg = 0x1001c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1001c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if1_ibit_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_ext_if1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if2_ebit_clk = {
	.halt_reg = 0x11020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x11020,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if2_ebit_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if2_ibit_clk = {
	.halt_reg = 0x1101c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1101c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if2_ibit_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_ext_if2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if3_ebit_clk = {
	.halt_reg = 0x12020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x12020,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if3_ebit_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if3_ibit_clk = {
	.halt_reg = 0x1201c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1201c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if3_ibit_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_ext_if3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if4_ebit_clk = {
	.halt_reg = 0x13024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x13024,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if4_ebit_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_if4_ibit_clk = {
	.halt_reg = 0x13020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x13020,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_if4_ibit_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_ext_if4_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_mclk0_clk = {
	.halt_reg = 0x20018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x20018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_mclk0_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_ext_mclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_ext_mclk1_clk = {
	.halt_reg = 0x21018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x21018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_ext_mclk1_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_ext_mclk1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_lpaif_pcmoe_clk = {
	.halt_reg = 0x19018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x19018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_lpaif_pcmoe_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_lpaif_pcmoe_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_rx_mclk_2x_clk = {
	.halt_reg = 0x240cc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x240cc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_rx_mclk_2x_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_rx_mclk_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_rx_mclk_clk = {
	.halt_reg = 0x240d4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x240d4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_rx_mclk_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_cdiv_rx_mclk_div_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_sampling_clk = {
	.halt_reg = 0x13000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x13000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_sampling_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_wsa_mclk_2x_clk = {
	.halt_reg = 0x220cc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x220cc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_wsa_mclk_2x_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_wsa_mclk_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch lpass_audio_cc_wsa_mclk_clk = {
	.halt_reg = 0x220d4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x220d4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "lpass_audio_cc_wsa_mclk_clk",
			.parent_names = (const char *[]){
				"lpass_audio_cc_cdiv_wsa_mclk_div_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *lpass_audio_cc_scuba_clocks[] = {
	[AUD_SLIMBUS_CLK_SRC] = &aud_slimbus_clk_src.clkr,
	[LPASS_AUDIO_CC_AUD_SLIMBUS_CLK] = &lpass_audio_cc_aud_slimbus_clk.clkr,
	[LPASS_AUDIO_CC_AUD_SLIMBUS_CORE_CLK] =
				&lpass_audio_cc_aud_slimbus_core_clk.clkr,
	[LPASS_AUDIO_CC_AUD_SLIMBUS_NPL_CLK] =
				&lpass_audio_cc_aud_slimbus_npl_clk.clkr,
	[LPASS_AUDIO_CC_BUS_CLK] = &lpass_audio_cc_bus_clk.clkr,
	[LPASS_AUDIO_CC_BUS_TIMEOUT_CLK] = &lpass_audio_cc_bus_timeout_clk.clkr,
	[LPASS_AUDIO_CC_CDIV_RX_MCLK_DIV_CLK_SRC] =
				&lpass_audio_cc_cdiv_rx_mclk_div_clk_src.clkr,
	[LPASS_AUDIO_CC_CDIV_WSA_MCLK_DIV_CLK_SRC] =
				&lpass_audio_cc_cdiv_wsa_mclk_div_clk_src.clkr,
	[LPASS_AUDIO_CC_CODEC_MEM0_CLK] = &lpass_audio_cc_codec_mem0_clk.clkr,
	[LPASS_AUDIO_CC_CODEC_MEM1_CLK] = &lpass_audio_cc_codec_mem1_clk.clkr,
	[LPASS_AUDIO_CC_CODEC_MEM2_CLK] = &lpass_audio_cc_codec_mem2_clk.clkr,
	[LPASS_AUDIO_CC_CODEC_MEM3_CLK] = &lpass_audio_cc_codec_mem3_clk.clkr,
	[LPASS_AUDIO_CC_CODEC_MEM_CLK] = &lpass_audio_cc_codec_mem_clk.clkr,
	[LPASS_AUDIO_CC_DIG_PLL] = &lpass_audio_cc_dig_pll.clkr,
	[LPASS_AUDIO_CC_DIG_PLL_OUT_AUX] = &lpass_audio_cc_dig_pll_out_aux.clkr,
	[LPASS_AUDIO_CC_EXT_IF1_CLK_SRC] = &lpass_audio_cc_ext_if1_clk_src.clkr,
	[LPASS_AUDIO_CC_EXT_IF1_EBIT_CLK] =
				&lpass_audio_cc_ext_if1_ebit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF1_IBIT_CLK] =
				&lpass_audio_cc_ext_if1_ibit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF2_CLK_SRC] = &lpass_audio_cc_ext_if2_clk_src.clkr,
	[LPASS_AUDIO_CC_EXT_IF2_EBIT_CLK] =
				&lpass_audio_cc_ext_if2_ebit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF2_IBIT_CLK] =
				&lpass_audio_cc_ext_if2_ibit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF3_CLK_SRC] = &lpass_audio_cc_ext_if3_clk_src.clkr,
	[LPASS_AUDIO_CC_EXT_IF3_EBIT_CLK] =
				&lpass_audio_cc_ext_if3_ebit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF3_IBIT_CLK] =
				&lpass_audio_cc_ext_if3_ibit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF4_CLK_SRC] = &lpass_audio_cc_ext_if4_clk_src.clkr,
	[LPASS_AUDIO_CC_EXT_IF4_EBIT_CLK] =
				&lpass_audio_cc_ext_if4_ebit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_IF4_IBIT_CLK] =
				&lpass_audio_cc_ext_if4_ibit_clk.clkr,
	[LPASS_AUDIO_CC_EXT_MCLK0_CLK] = &lpass_audio_cc_ext_mclk0_clk.clkr,
	[LPASS_AUDIO_CC_EXT_MCLK0_CLK_SRC] =
				&lpass_audio_cc_ext_mclk0_clk_src.clkr,
	[LPASS_AUDIO_CC_EXT_MCLK1_CLK] = &lpass_audio_cc_ext_mclk1_clk.clkr,
	[LPASS_AUDIO_CC_EXT_MCLK1_CLK_SRC] =
				&lpass_audio_cc_ext_mclk1_clk_src.clkr,
	[LPASS_AUDIO_CC_LPAIF_PCMOE_CLK] = &lpass_audio_cc_lpaif_pcmoe_clk.clkr,
	[LPASS_AUDIO_CC_LPAIF_PCMOE_CLK_SRC] =
				&lpass_audio_cc_lpaif_pcmoe_clk_src.clkr,
	[LPASS_AUDIO_CC_PLL] = &lpass_audio_cc_pll.clkr,
	[LPASS_AUDIO_CC_PLL_OUT_AUX] = &lpass_audio_cc_pll_out_aux.clkr,
	[LPASS_AUDIO_CC_PLL_OUT_AUX2] = &lpass_audio_cc_pll_out_aux2.clkr,
	[LPASS_AUDIO_CC_PLL_OUT_AUX2_DIV_CLK_SRC] =
				&lpass_audio_cc_pll_out_aux2_div_clk_src.clkr,
	[LPASS_AUDIO_CC_RX_MCLK_2X_CLK] = &lpass_audio_cc_rx_mclk_2x_clk.clkr,
	[LPASS_AUDIO_CC_RX_MCLK_CLK] = &lpass_audio_cc_rx_mclk_clk.clkr,
	[LPASS_AUDIO_CC_RX_MCLK_CLK_SRC] = &lpass_audio_cc_rx_mclk_clk_src.clkr,
	[LPASS_AUDIO_CC_SAMPLING_CLK] = &lpass_audio_cc_sampling_clk.clkr,
	[LPASS_AUDIO_CC_WSA_MCLK_2X_CLK] = &lpass_audio_cc_wsa_mclk_2x_clk.clkr,
	[LPASS_AUDIO_CC_WSA_MCLK_CLK] = &lpass_audio_cc_wsa_mclk_clk.clkr,
	[LPASS_AUDIO_CC_WSA_MCLK_CLK_SRC] =
				&lpass_audio_cc_wsa_mclk_clk_src.clkr,
};

static const struct qcom_reset_map lpass_audio_cc_scuba_resets[] = {
	[LPASS_AUDIO_CC_EXT_IF1_BCR] = { 0x10000 },
	[LPASS_AUDIO_CC_EXT_IF2_BCR] = { 0x11000 },
	[LPASS_AUDIO_CC_EXT_IF3_BCR] = { 0x12000 },
	[LPASS_AUDIO_CC_EXT_IF4_BCR] = { 0x13004 },
	[LPASS_AUDIO_CC_EXT_MCLK0_BCR] = { 0x20000 },
	[LPASS_AUDIO_CC_EXT_MCLK1_BCR] = { 0x21000 },
	[LPASS_AUDIO_CC_PCM_DATA_OE_BCR] = { 0x19000 },
	[LPASS_AUDIO_CC_QCA_SLIMBUS_BCR] = { 0x16ffc },
	[LPASS_AUDIO_CC_RX_MCLK_BCR] = { 0x24000 },
	[LPASS_AUDIO_CC_TX_MCLK_BCR] = { 0x23000 },
	[LPASS_AUDIO_CC_WSA_MCLK_BCR] = { 0x22000 },
};

static const struct regmap_config lpass_audio_cc_scuba_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x2f100,
	.fast_io = true,
};

static const struct qcom_cc_desc lpass_audio_cc_scuba_desc = {
	.config = &lpass_audio_cc_scuba_regmap_config,
	.clks = lpass_audio_cc_scuba_clocks,
	.num_clks = ARRAY_SIZE(lpass_audio_cc_scuba_clocks),
	.resets = lpass_audio_cc_scuba_resets,
	.num_resets = ARRAY_SIZE(lpass_audio_cc_scuba_resets),
};

static const struct of_device_id lpass_audio_cc_scuba_match_table[] = {
	{ .compatible = "qcom,lpassaudiocc-scuba" },
	{ }
};
MODULE_DEVICE_TABLE(of, lpass_audio_cc_scuba_match_table);

static int lpass_audio_cc_scuba_probe(struct platform_device *pdev)
{
	static struct regulator *audio_hm_regulator;
	struct regmap *regmap;
	int ret;

	vdd_lpi_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_lpi_cx");
	if (IS_ERR(vdd_lpi_cx.regulator[0])) {
		if (PTR_ERR(vdd_lpi_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_lpi_cx\n");
		return PTR_ERR(vdd_lpi_cx.regulator[0]);
	}

	audio_hm_regulator = devm_regulator_get(&pdev->dev, "audio_hm");
	if (IS_ERR(audio_hm_regulator)) {
		if (PTR_ERR(audio_hm_regulator) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get audio_hm regulator\n");
		return PTR_ERR(audio_hm_regulator);
	}

	platform_set_drvdata(pdev, audio_hm_regulator);

	pm_runtime_enable(&pdev->dev);

	/* Get the GCC LPASS Sway clock */
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

	regmap = qcom_cc_map(pdev, &lpass_audio_cc_scuba_desc);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		goto err_put_rpm;
	}

	clk_alpha_pll_configure(&lpass_audio_cc_dig_pll, regmap,
					&lpass_audio_cc_dig_pll_config);
	clk_alpha_pll_configure(&lpass_audio_cc_pll, regmap,
					&lpass_audio_cc_pll_config);

	ret = qcom_cc_really_probe(pdev, &lpass_audio_cc_scuba_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Audio clocks Register fail ret=%d\n", ret);
		goto err_put_rpm;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered LPASS Audio clocks\n");
	return 0;

err_put_rpm:
	pm_runtime_put_sync(&pdev->dev);
err_destroy_pm_clk:
	pm_clk_destroy(&pdev->dev);
	return ret;
}

static int lpass_audio_cc_scuba_runtime_resume(struct device *dev)
{
	int ret;
	struct regulator *audio_hm_regulator = dev_get_drvdata(dev);

	ret = pm_clk_resume(dev);
	if (ret) {
		dev_err(dev, "Lpass Audio enabling GCC sway clock failed\n");
		return ret;
	}

	return regulator_enable(audio_hm_regulator);
}

static int lpass_audio_cc_scuba_runtime_suspend(struct device *dev)
{
	struct regulator *audio_hm_regulator = dev_get_drvdata(dev);

	regulator_disable(audio_hm_regulator);
	return pm_clk_suspend(dev);
}

static const struct dev_pm_ops lpass_audio_cc_scuba_pm_ops = {
	SET_RUNTIME_PM_OPS(lpass_audio_cc_scuba_runtime_suspend,
				lpass_audio_cc_scuba_runtime_resume, NULL)
};

static struct platform_driver lpass_audio_cc_scuba_driver = {
	.probe = lpass_audio_cc_scuba_probe,
	.driver = {
		.name = "lpassaudiocc-scuba",
		.of_match_table = lpass_audio_cc_scuba_match_table,
		.pm = &lpass_audio_cc_scuba_pm_ops,
	},
};

static int __init lpass_audio_cc_scuba_init(void)
{
	return platform_driver_register(&lpass_audio_cc_scuba_driver);
}
subsys_initcall(lpass_audio_cc_scuba_init);

static void __exit lpass_audio_cc_scuba_exit(void)
{
	platform_driver_unregister(&lpass_audio_cc_scuba_driver);
}
module_exit(lpass_audio_cc_scuba_exit);

MODULE_DESCRIPTION("QTI LPASSAUDIOCC SCUBA Driver");
MODULE_LICENSE("GPL v2");

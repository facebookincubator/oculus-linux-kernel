// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <dt-bindings/clock/qcom,cpu-sdm.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <soc/qcom/pm.h>

#include "clk-pll.h"
#include "clk-debug.h"
#include "clk-rcg.h"
#include "clk-regmap-mux-div.h"
#include "common.h"
#include "vdd-level-cpu.h"

#define to_clk_regmap_mux_div(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_regmap_mux_div, clkr)

static DEFINE_VDD_REGULATORS(vdd_hf_pll, VDD_HF_PLL_NUM, 2, vdd_hf_levels);
static DEFINE_VDD_REGULATORS(vdd_sr2_pll, VDD_HF_PLL_NUM, 2, vdd_hf_levels);
static DEFINE_VDD_REGS_INIT(vdd_cpu_c0, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_c1, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_cci, 1);

enum apcs_mux_clk_parent {
	P_BI_TCXO_AO,
	P_GPLL0_AO_OUT_MAIN,
	P_APCS_CPU_PLL,
};

static const struct parent_map apcs_mux_clk_parent_map0[] = {
	{ P_BI_TCXO_AO, 0 },
	{ P_GPLL0_AO_OUT_MAIN, 4 },
	{ P_APCS_CPU_PLL, 5 },
};

static const char *const apcs_mux_clk_c1_parent_name0[] = {
	"bi_tcxo_ao",
	"gpll0_ao_out_main",
	"apcs_cpu_pll1",
};

static const char *const apcs_mux_clk_c0_parent_name0[] = {
	"bi_tcxo_ao",
	"gpll0_ao_out_main",
	"apcs_cpu_pll0",
};

static const struct parent_map apcs_mux_clk_parent_map1[] = {
	{ P_BI_TCXO_AO, 0 },
	{ P_GPLL0_AO_OUT_MAIN, 4 },
};

static const char *const apcs_mux_clk_parent_name1[] = {
	"bi_tcxo_ao",
	"gpll0_ao_out_main",
};

static unsigned long
calc_rate(unsigned long rate, u32 m, u32 n, u32 mode, u32 hid_div)
{
	u64 tmp = rate;

	if (hid_div) {
		tmp *= 2;
		do_div(tmp, hid_div + 1);
	}

	if (mode) {
		tmp *= m;
		do_div(tmp, n);
	}

	return tmp;
}

static int cpucc_clk_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
						unsigned long prate, u8 index)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	return mux_div_set_src_div(cpuclk, cpuclk->parent_map[index].cfg,
					cpuclk->div);
}

static int cpucc_clk_set_parent(struct clk_hw *hw, u8 index)
{
	return 0;
}

static int cpucc_clk_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	return mux_div_set_src_div(cpuclk, cpuclk->src, cpuclk->div);
}

static bool freq_from_gpll0(unsigned long req_rate, unsigned long gpll0_rate)
{
	unsigned long temp;
	int div;

	for (div = 10; div <= 40; div += 5) {
		temp = mult_frac(gpll0_rate, 10, div);
		if (req_rate == temp)
			return true;
	}

	return false;
}

static int cpucc_clk_determine_rate(struct clk_hw *hw,
					struct clk_rate_request *req)
{
	struct clk_hw *xo, *apcs_gpll0_hw, *apcs_cpu_pll_hw;
	struct clk_rate_request parent_req = { };
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	unsigned long apcs_gpll0_rate, apcs_gpll0_rrate, rate = req->rate;
	unsigned long mask = BIT(cpuclk->hid_width) - 1;
	u32 div = 1;
	int ret;

	xo = clk_hw_get_parent_by_index(hw, P_BI_TCXO_AO);
	if (rate == clk_hw_get_rate(xo)) {
		req->best_parent_hw = xo;
		req->best_parent_rate = rate;
		cpuclk->div = div;
		cpuclk->src = cpuclk->parent_map[P_BI_TCXO_AO].cfg;
		return 0;
	}

	apcs_gpll0_hw = clk_hw_get_parent_by_index(hw, P_GPLL0_AO_OUT_MAIN);
	apcs_cpu_pll_hw = clk_hw_get_parent_by_index(hw, P_APCS_CPU_PLL);

	apcs_gpll0_rate = clk_hw_get_rate(apcs_gpll0_hw);
	apcs_gpll0_rrate = DIV_ROUND_UP(apcs_gpll0_rate, 1000000) * 1000000;

	if (freq_from_gpll0(rate, apcs_gpll0_rrate)) {
		req->best_parent_hw = apcs_gpll0_hw;
		req->best_parent_rate = apcs_gpll0_rrate;
		div = DIV_ROUND_CLOSEST(2 * apcs_gpll0_rrate, rate) - 1;
		div = min_t(unsigned long, div, mask);
		req->rate = calc_rate(req->best_parent_rate, 0,
							0, 0, div);
		cpuclk->src = cpuclk->parent_map[P_GPLL0_AO_OUT_MAIN].cfg;
	} else {
		parent_req.rate = rate;
		parent_req.best_parent_hw = apcs_cpu_pll_hw;

		req->best_parent_hw = apcs_cpu_pll_hw;
		ret = __clk_determine_rate(req->best_parent_hw, &parent_req);
		if (ret)
			return ret;

		req->best_parent_rate = parent_req.rate;
		cpuclk->src = cpuclk->parent_map[P_APCS_CPU_PLL].cfg;
	}

	cpuclk->div = div;

	return 0;
}

static void cpucc_clk_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	int i = 0, size = 0, val;

	static struct clk_register_data data[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
	};

	size = ARRAY_SIZE(data);
	for (i = 0; i < size; i++) {
		regmap_read(cpuclk->clkr.regmap,
				cpuclk->reg_offset + data[i].offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data[i].name, val);
	}
}

static unsigned long cpucc_clk_recalc_rate(struct clk_hw *hw,
					unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	struct clk_hw *parent;
	const char *name = clk_hw_get_name(hw);
	unsigned long parent_rate;
	u32 i, div, src = 0;
	u32 num_parents = clk_hw_get_num_parents(hw);
	int ret;

	ret = mux_div_get_src_div(cpuclk, &src, &div);
	if (ret)
		return ret;

	cpuclk->src = src;
	cpuclk->div = div;

	for (i = 0; i < num_parents; i++) {
		if (src == cpuclk->parent_map[i].cfg) {
			parent = clk_hw_get_parent_by_index(hw, i);
			parent_rate = clk_hw_get_rate(parent);
			return calc_rate(parent_rate, 0, 0, 0, div);
		}
	}
	pr_err("%s: Can't find parent %d\n", name, src);

	return ret;
}

static int cpucc_clk_enable(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.enable(hw);
}

static void cpucc_clk_disable(struct clk_hw *hw)
{
	clk_regmap_mux_div_ops.disable(hw);
}

static u8 cpucc_clk_get_parent(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.get_parent(hw);
}

static void do_nothing(void *unused) { }

/*
 * We use the notifier function for switching to a temporary safe configuration
 * (mux and divider), while the APSS pll is reconfigured.
 */
static int cpucc_notifier_cb(struct notifier_block *nb, unsigned long event,
			     void *data)
{
	struct clk_regmap_mux_div *cpuclk = container_of(nb,
					struct clk_regmap_mux_div, clk_nb);
	bool hw_low_power_ctrl = cpuclk->clk_lpm.hw_low_power_ctrl;
	int ret = 0, safe_src = cpuclk->safe_src;

	switch (event) {
	case PRE_RATE_CHANGE:
		if (hw_low_power_ctrl) {
			memset(&cpuclk->clk_lpm.req, 0,
					sizeof(cpuclk->clk_lpm.req));
			cpumask_copy(&cpuclk->clk_lpm.req.cpus_affine,
			(const struct cpumask *)&cpuclk->clk_lpm.cpu_reg_mask);
			cpuclk->clk_lpm.req.type = PM_QOS_REQ_AFFINE_CORES;
			pm_qos_add_request(&cpuclk->clk_lpm.req,
				PM_QOS_CPU_DMA_LATENCY,
				cpuclk->clk_lpm.cpu_latency_no_l2_pc_us - 1);
			smp_call_function_any(&cpuclk->clk_lpm.cpu_reg_mask,
				do_nothing, NULL, 1);
		}

		/* set the mux to safe source gpll0_ao_out & div */
		mux_div_set_src_div(cpuclk, safe_src, 1);
		break;
	case POST_RATE_CHANGE:
		if (hw_low_power_ctrl)
			pm_qos_remove_request(&cpuclk->clk_lpm.req);

		break;
	case ABORT_RATE_CHANGE:
		pr_err("Error in configuring PLL - stay at safe src only\n");
	}

	return notifier_from_errno(ret);
}

static const struct clk_ops cpucc_clk_ops = {
	.enable = cpucc_clk_enable,
	.disable = cpucc_clk_disable,
	.get_parent = cpucc_clk_get_parent,
	.set_rate = cpucc_clk_set_rate,
	.set_parent = cpucc_clk_set_parent,
	.set_rate_and_parent = cpucc_clk_set_rate_and_parent,
	.determine_rate = cpucc_clk_determine_rate,
	.recalc_rate = cpucc_clk_recalc_rate,
	.debug_init = clk_debug_measure_add,
	.list_registers = cpucc_clk_list_registers,
};

/* Initial configuration for 1305.6MHz */
static const struct pll_config apcs_cpu_pll_config = {
	.l = 0x44,
	.m = 0,
	.n = 1,
	.pre_div_val = 0x0,
	.pre_div_mask = 0x7 << 12,
	.post_div_val = 0x0,
	.post_div_mask = 0x3 << 8,
	.main_output_mask = BIT(0),
	.aux_output_mask = BIT(1),
};

static struct clk_pll apcs_cpu_pll0 = {
	.mode_reg = 0x0,
	.l_reg = 0x4,
	.m_reg = 0x8,
	.n_reg = 0xc,
	.config_reg = 0x10,
	.status_reg = 0x1c,
	.status_bit = 16,
	.spm_ctrl = {
		.offset = 0x50,
		.event_bit = 0x4,
	},
	.clkr.hw.init = &(struct clk_init_data){
		.name = "apcs_cpu_pll0",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_pll_hf_ops,
		.vdd_class = &vdd_sr2_pll,
		.rate_max = (unsigned long[VDD_HF_PLL_NUM]) {
			[VDD_HF_PLL_SVS] = 1000000000,
			[VDD_HF_PLL_NOM] = 1900000000,
		},
		.num_rate_max = VDD_HF_PLL_NUM,
	},
};

static struct clk_regmap_mux_div apcs_mux_c0_clk = {
	.reg_offset = 0x0,
	.hid_width  = 5,
	.hid_shift  = 0,
	.src_width  = 3,
	.src_shift  = 8,
	.safe_src = 4,
	.safe_div = 1,
	.parent_map = apcs_mux_clk_parent_map0,
	.clk_nb.notifier_call = cpucc_notifier_cb,
	.clk_lpm = {
		/* CPU 4 - 7 */
		.cpu_reg_mask = { 0xf0 },
		.latency_lvl = {
			.affinity_level = LPM_AFF_LVL_L2,
			.reset_level = LPM_RESET_LVL_GDHS,
			.level_name = "pwr",
		},
		.cpu_latency_no_l2_pc_us = 300,
	},
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "apcs_mux_c0_clk",
		.parent_names = apcs_mux_clk_c0_parent_name0,
		.num_parents = 3,
		.vdd_class = &vdd_cpu_c0,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &cpucc_clk_ops,
	},
};

static struct clk_pll apcs_cpu_pll1 = {
	.mode_reg = 0x0,
	.l_reg = 0x4,
	.m_reg = 0x8,
	.n_reg = 0xc,
	.config_reg = 0x10,
	.status_reg = 0x1c,
	.status_bit = 16,
	.spm_ctrl = {
		.offset = 0x50,
		.event_bit = 0x4,
	},
	.clkr.hw.init = &(struct clk_init_data){
		.name = "apcs_cpu_pll1",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_pll_hf_ops,
		.vdd_class = &vdd_hf_pll,
		.rate_max = (unsigned long[VDD_HF_PLL_NUM]) {
			[VDD_HF_PLL_SVS] = 1000000000,
			[VDD_HF_PLL_NOM] = 2020000000,
		},
		.num_rate_max = VDD_HF_PLL_NUM,
	},
};

static struct clk_regmap_mux_div apcs_mux_c1_clk = {
	.reg_offset = 0x0,
	.hid_width  = 5,
	.hid_shift  = 0,
	.src_width  = 3,
	.src_shift  = 8,
	.safe_src = 4,
	.safe_div = 1,
	.parent_map = apcs_mux_clk_parent_map0,
	.clk_nb.notifier_call = cpucc_notifier_cb,
	.clk_lpm  = {
		/* CPU 0 - 3*/
		.cpu_reg_mask = { 0xf },
		.latency_lvl = {
			.affinity_level = LPM_AFF_LVL_L2,
			.reset_level = LPM_RESET_LVL_GDHS,
			.level_name = "perf",
		},
		.cpu_latency_no_l2_pc_us = 300,
	},
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "apcs_mux_c1_clk",
		.parent_names = apcs_mux_clk_c1_parent_name0,
		.num_parents = 3,
		.vdd_class = &vdd_cpu_c1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &cpucc_clk_ops,
	},
};

static struct clk_regmap_mux_div apcs_mux_cci_clk = {
	.reg_offset = 0x0,
	.hid_width  = 5,
	.hid_shift  = 0,
	.src_width  = 3,
	.src_shift  = 8,
	.safe_src = 4,
	.safe_div = 1,
	.parent_map = apcs_mux_clk_parent_map1,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "apcs_mux_cci_clk",
		.parent_names = apcs_mux_clk_parent_name1,
		.num_parents = 2,
		.vdd_class = &vdd_cpu_cci,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &cpucc_clk_ops,
	},
};

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-clock-sdm439" },
	{ .compatible = "qcom,cpu-clock-sdm429" },
	{ .compatible = "qcom,cpu-clock-qm215" },
	{}
};

static struct regmap_config cpu_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x34,
	.fast_io	= true,
};

static struct clk_hw *cpu_clks_hws_qm215[] = {
	[APCS_CPU_PLL1] = &apcs_cpu_pll1.clkr.hw,
	[APCS_MUX_C1_CLK] = &apcs_mux_c1_clk.clkr.hw,
};

static struct clk_hw *cpu_clks_hws_sdm429[] = {
	[APCS_CPU_PLL1] = &apcs_cpu_pll1.clkr.hw,
	[APCS_MUX_C1_CLK] = &apcs_mux_c1_clk.clkr.hw,
	[APCS_MUX_CCI_CLK] = &apcs_mux_cci_clk.clkr.hw,
};

static struct clk_hw *cpu_clks_hws_sdm439[] = {
	[APCS_CPU_PLL1] = &apcs_cpu_pll1.clkr.hw,
	[APCS_MUX_C1_CLK] = &apcs_mux_c1_clk.clkr.hw,
	[APCS_MUX_CCI_CLK] = &apcs_mux_cci_clk.clkr.hw,
	[APCS_CPU_PLL0] = &apcs_cpu_pll0.clkr.hw,
	[APCS_MUX_C0_CLK] = &apcs_mux_c0_clk.clkr.hw,
};

static void cpucc_clk_get_speed_bin(struct platform_device *pdev, int *bin,
							int *version)
{
	struct resource *res;
	void  __iomem *base;
	u32  pte_efuse;

	*bin = 0;
	*version = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		dev_info(&pdev->dev,
			"No speed/PVS binning available. Defaulting to 0!\n");
		return;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_info(&pdev->dev,
			"Unable to read efuse data. Defaulting to 0!\n");
		return;
	}

	pte_efuse = readl_relaxed(base);
	iounmap(base);

	*bin = (pte_efuse >> 2) & 0x7;

	dev_info(&pdev->dev, "PVS version: %d speed bin: %d\n", *version, *bin);
}

static int cpucc_clk_get_fmax_vdd_class(struct platform_device *pdev,
			struct clk_init_data *clk_intd, char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	u32 *array;
	int prop_len, i, j, ret;
	int num = vdd->num_regulators + 1;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % num) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= num;
	vdd->level_votes = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->level_votes)
		return -ENOMEM;

	vdd->vdd_uv = devm_kzalloc(&pdev->dev,
				prop_len * sizeof(int) * (num - 1), GFP_KERNEL);
	if (!vdd->vdd_uv)
		return -ENOMEM;

	clk_intd->rate_max = devm_kzalloc(&pdev->dev,
				prop_len * sizeof(unsigned long), GFP_KERNEL);
	if (!clk_intd->rate_max)
		return -ENOMEM;

	array = kzalloc(prop_len * sizeof(u32) * num, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	ret = of_property_read_u32_array(of, prop_name, array, prop_len * num);
	if (ret)
		return -ENOMEM;

	for (i = 0; i < prop_len; i++) {
		clk_intd->rate_max[i] = array[num * i];
		for (j = 1; j < num; j++) {
			vdd->vdd_uv[(num - 1) * i + (j - 1)] =
					array[num * i + j];
		}
	}

	kfree(array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	clk_intd->num_rate_max = prop_len;

	return 0;
}

static int find_vdd_level(struct clk_init_data *clk_intd, unsigned long rate)
{
	int level;

	for (level = 0; level < clk_intd->num_rate_max; level++)
		if (rate <= clk_intd->rate_max[level])
			break;

	if (level == clk_intd->num_rate_max) {
		pr_err("Rate %lu for %s is greater than highest Fmax\n", rate,
				clk_intd->name);
		return -EINVAL;
	}

	return level;
}

static int
cpucc_clk_add_opp(struct clk_hw *hw, struct device *dev, unsigned long max_rate)
{
	struct clk_init_data *clk_intd =  (struct clk_init_data *)hw->init;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	int level, uv, j = 1;
	unsigned long rate = 0;
	long ret;

	if (IS_ERR_OR_NULL(dev)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	while (1) {
		rate = clk_intd->rate_max[j++];
		level = find_vdd_level(clk_intd, rate);
		if (level <= 0) {
			pr_warn("clock-cpu: no corner for %lu.\n", rate);
			return -EINVAL;
		}

		uv = vdd->vdd_uv[level];
		if (uv < 0) {
			pr_warn("clock-cpu: no uv for %lu.\n", rate);
			return -EINVAL;
		}

		ret = dev_pm_opp_add(dev, rate, uv);
		if (ret) {
			pr_warn("clock-cpu: failed to add OPP for %lu\n", rate);
			return rate;
		}

		if (rate >= max_rate)
			break;
	}

	return 0;
}

static void cpucc_clk_print_opp_table(int c0, int c1, bool is_sdm439)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc_c0_fmax, apc_c0_fmin, apc_c1_fmax, apc_c1_fmin;
	u32 max_index;

	max_index = apcs_mux_c1_clk.clkr.hw.init->num_rate_max;
	apc_c1_fmax = apcs_mux_c1_clk.clkr.hw.init->rate_max[max_index - 1];
	apc_c1_fmin = apcs_mux_c1_clk.clkr.hw.init->rate_max[1];

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(c1),
		apc_c1_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(c1),
		apc_c1_fmin, true);

	pr_info("Clock_cpu:(cpu %d) OPP voltage for %lu: %ld\n", c1,
		 apc_c1_fmin, dev_pm_opp_get_voltage(oppfmin));
	pr_info("Clock_cpu:(cpu %d) OPP voltage for %lu: %ld\n", c1,
		 apc_c1_fmax, dev_pm_opp_get_voltage(oppfmax));

	if (is_sdm439) {
		max_index = apcs_mux_c0_clk.clkr.hw.init->num_rate_max;
		apc_c0_fmax =
			apcs_mux_c0_clk.clkr.hw.init->rate_max[max_index - 1];
		apc_c0_fmin = apcs_mux_c0_clk.clkr.hw.init->rate_max[1];

		oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(c0),
			apc_c0_fmax, true);
		oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(c0),
			apc_c0_fmin, true);

		pr_info("Clock_cpu:(cpu %d) OPP voltage for %lu: %ld\n", c0,
			 apc_c0_fmin, dev_pm_opp_get_voltage(oppfmin));
		pr_info("Clock_cpu:(cpu %d) OPP voltage for %lu: %ld\n", c0,
			 apc_c0_fmax, dev_pm_opp_get_voltage(oppfmax));
	}
}

static void cpucc_clk_populate_opp_table(struct platform_device *pdev,
		bool is_sdm439)
{
	unsigned long apc_c1_fmax, apc_c0_fmax;
	u32 max_index;
	int cpu, sdm_cpu0 = 0, sdm_cpu1 = 0;

	if (is_sdm439) {
		max_index = apcs_mux_c0_clk.clkr.hw.init->num_rate_max;
		apc_c0_fmax =
			apcs_mux_c0_clk.clkr.hw.init->rate_max[max_index - 1];
	}

	max_index = apcs_mux_c1_clk.clkr.hw.init->num_rate_max;
	apc_c1_fmax = apcs_mux_c1_clk.clkr.hw.init->rate_max[max_index - 1];

	for_each_possible_cpu(cpu) {
		if (cpu/4 == 0) {
			sdm_cpu1 = cpu;
			WARN(cpucc_clk_add_opp(&apcs_mux_c1_clk.clkr.hw,
			get_cpu_device(cpu), apc_c1_fmax),
			"Failed to add OPP levels for apcs_mux_c1_clk\n");
		} else if (cpu/4 == 1 && is_sdm439) {
			sdm_cpu0 = cpu;
			WARN(cpucc_clk_add_opp(&apcs_mux_c0_clk.clkr.hw,
			get_cpu_device(cpu), apc_c0_fmax),
			"Failed to add OPP levels for apcs_mux_c0_clk\n");
		}
	}
	cpucc_clk_print_opp_table(sdm_cpu0, sdm_cpu1, is_sdm439);
}

static int clock_sdm429_pm_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		clk_unprepare(apcs_mux_c1_clk.clkr.hw.clk);
		clk_unprepare(apcs_mux_cci_clk.clkr.hw.clk);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		clk_prepare(apcs_mux_c1_clk.clkr.hw.clk);
		clk_prepare(apcs_mux_cci_clk.clkr.hw.clk);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block clock_sdm429_pm_notifier = {
	.notifier_call = clock_sdm429_pm_event,
};

static int clock_sdm439_pm_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		clk_unprepare(apcs_mux_c0_clk.clkr.hw.clk);
		clk_unprepare(apcs_mux_c1_clk.clkr.hw.clk);
		clk_unprepare(apcs_mux_cci_clk.clkr.hw.clk);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		clk_prepare(apcs_mux_c0_clk.clkr.hw.clk);
		clk_prepare(apcs_mux_c1_clk.clkr.hw.clk);
		clk_prepare(apcs_mux_cci_clk.clkr.hw.clk);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block clock_sdm439_pm_notifier = {
	.notifier_call = clock_sdm439_pm_event,
};

static int clock_qm215_pm_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		clk_unprepare(apcs_mux_c1_clk.clkr.hw.clk);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		clk_prepare(apcs_mux_c1_clk.clkr.hw.clk);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block clock_qm215_pm_notifier = {
	.notifier_call = clock_qm215_pm_event,
};

static int fixup_for_sdm439(struct platform_device *pdev, int speed_bin,
		int version)
{
	struct resource *res;
	void __iomem *base;
	struct device *dev = &pdev->dev;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	int ret;

	 /* Rail Regulator for apcs_pll0 */
	vdd_sr2_pll.regulator[0] = devm_regulator_get(&pdev->dev,
			"vdd_sr2_pll");
	if (IS_ERR(vdd_sr2_pll.regulator[0])) {
		if (!(PTR_ERR(vdd_sr2_pll.regulator[0]) ==
			-EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get sr2_pll regulator\n");
		return PTR_ERR(vdd_sr2_pll.regulator[0]);
	}

	vdd_sr2_pll.regulator[1] = devm_regulator_get(&pdev->dev,
			"vdd_sr2_dig_ao");
	if (IS_ERR(vdd_sr2_pll.regulator[1])) {
		if (!(PTR_ERR(vdd_sr2_pll.regulator[1]) ==
			-EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get dig_ao regulator\n");
		return PTR_ERR(vdd_sr2_pll.regulator[1]);
	}

	/* Rail Regulator for APCS C0 mux */
	vdd_cpu_c0.regulator[0] = devm_regulator_get(&pdev->dev,
			"cpu-vdd");
	if (IS_ERR(vdd_cpu_c0.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu_c0.regulator[0]) ==
					-EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get C0 cpu-vdd regulator\n");
		return PTR_ERR(vdd_cpu_c0.regulator[0]);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"apcs_pll0");
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get apcs_pll0 resources\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed map apcs_cpu_pll0 register base\n");
		return PTR_ERR(base);
	}

	cpu_regmap_config.name = "apcs_pll0";
	apcs_cpu_pll0.clkr.regmap = devm_regmap_init_mmio(dev, base,
						&cpu_regmap_config);
	if (IS_ERR(apcs_cpu_pll0.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs_cpu_pll0\n");
		return PTR_ERR(apcs_cpu_pll0.clkr.regmap);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		 "apcs-c0-rcg-base");
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get apcs-c0 resources\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed map apcs-c0-rcg register base\n");
		return PTR_ERR(base);
	}

	cpu_regmap_config.name = "apcs-c0-rcg-base";
	apcs_mux_c0_clk.clkr.regmap = devm_regmap_init_mmio(dev, base,
						&cpu_regmap_config);
	if (IS_ERR(apcs_mux_c0_clk.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs-c0-rcg\n");
		return PTR_ERR(apcs_mux_c0_clk.clkr.regmap);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"spm_c0_base");
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get spm-c0 resources\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to ioremap c0 spm registers\n");
		return -ENOMEM;
	}
	apcs_cpu_pll0.spm_ctrl.spm_base = base;

	snprintf(prop_name, ARRAY_SIZE(prop_name),
		"qcom,speed%d-bin-v%d-%s", speed_bin, version, "c0");

	ret = cpucc_clk_get_fmax_vdd_class(pdev,
		(struct clk_init_data *)apcs_mux_c0_clk.clkr.hw.init,
			 prop_name);
	if (ret) {
		dev_err(&pdev->dev, "Didn't get c0 speed bin\n");

		snprintf(prop_name, ARRAY_SIZE(prop_name),
				"qcom,speed0-bin-v0-%s", "c0");
		ret = cpucc_clk_get_fmax_vdd_class(pdev,
			(struct clk_init_data *)
			apcs_mux_c0_clk.clkr.hw.init,
				prop_name);
		if (ret) {
			dev_err(&pdev->dev,
				"Unable to load safe voltage plan for c0\n");
			return ret;
		}
	}
	return 0;
}

static int cpucc_driver_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk_hw_onecell_data *data;
	struct device *dev = &pdev->dev;
	struct clk *clk;
	int i, ret, speed_bin, version, cpu;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	void __iomem *base;
	bool is_sdm439, is_sdm429, is_qm215;

	is_sdm439 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,cpu-clock-sdm439");

	is_sdm429 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,cpu-clock-sdm429");

	is_qm215 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,cpu-clock-qm215");

	clk = clk_get(dev, "xo_ao");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}
	clk_put(clk);

	clk = clk_get(dev, "gpll0_ao");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get GPLL0 clock\n");
		return PTR_ERR(clk);
	}
	clk_put(clk);

	 /* Rail Regulator for apcs_pll */
	vdd_hf_pll.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_hf_pll");
	if (IS_ERR(vdd_hf_pll.regulator[0])) {
		if (!(PTR_ERR(vdd_hf_pll.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_hf_pll regulator\n");
		return PTR_ERR(vdd_hf_pll.regulator[0]);
	}

	vdd_hf_pll.regulator[1] = devm_regulator_get(&pdev->dev, "vdd_dig_ao");
	if (IS_ERR(vdd_hf_pll.regulator[1])) {
		if (!(PTR_ERR(vdd_hf_pll.regulator[1]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_dig_ao regulator\n");
		return PTR_ERR(vdd_hf_pll.regulator[1]);
	}

	/* Rail Regulator for APCS C1 mux */
	vdd_cpu_c1.regulator[0] = devm_regulator_get(&pdev->dev, "cpu-vdd");
	if (IS_ERR(vdd_cpu_c1.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu_c1.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get cpu-vdd regulator\n");
		return PTR_ERR(vdd_cpu_c1.regulator[0]);
	}

	/* Rail Regulator for APCS CCI mux */
	if (is_sdm429 || is_sdm439) {
		vdd_cpu_cci.regulator[0] =
			devm_regulator_get(&pdev->dev, "cpu-vdd");
		if (IS_ERR(vdd_cpu_cci.regulator[0])) {
			if (!(PTR_ERR(vdd_cpu_cci.regulator[0]) ==
						-EPROBE_DEFER))
				dev_err(&pdev->dev,
					"Unable to get cpu-vdd regulator\n");
			return PTR_ERR(vdd_cpu_cci.regulator[0]);
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_pll1");
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get apcs_pll1 resources\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed map apcs_cpu_pll1 register base\n");
		return PTR_ERR(base);
	}

	cpu_regmap_config.name = "apcs_pll1";
	apcs_cpu_pll1.clkr.regmap = devm_regmap_init_mmio(dev, base,
							&cpu_regmap_config);
	if (IS_ERR(apcs_cpu_pll1.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs_cpu_pll1\n");
		return PTR_ERR(apcs_cpu_pll1.clkr.regmap);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"spm_c1_base");
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get spm-c1 resources\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to ioremap c1 spm registers\n");
		return -ENOMEM;
	}
	apcs_cpu_pll1.spm_ctrl.spm_base = base;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			 "apcs-c1-rcg-base");
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get apcs-c1 resources\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed map apcs-c1-rcg register base\n");
		return PTR_ERR(base);
	}

	cpu_regmap_config.name = "apcs-c1-rcg-base";
	apcs_mux_c1_clk.clkr.regmap = devm_regmap_init_mmio(dev, base,
							&cpu_regmap_config);
	if (IS_ERR(apcs_mux_c1_clk.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs-c1-rcg\n");
		return PTR_ERR(apcs_mux_c1_clk.clkr.regmap);
	}

	if (is_sdm429 || is_sdm439) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				 "apcs-cci-rcg-base");
		if (res == NULL) {
			dev_err(&pdev->dev, "Failed to get apcs-cci resources\n");
			return -EINVAL;
		}

		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			dev_err(&pdev->dev, "Failed map apcs-cci-rcg register base\n");
			return PTR_ERR(base);
		}

		cpu_regmap_config.name = "apcs-cci-rcg-base";
		apcs_mux_cci_clk.clkr.regmap = devm_regmap_init_mmio(dev, base,
							&cpu_regmap_config);
		if (IS_ERR(apcs_mux_cci_clk.clkr.regmap)) {
			dev_err(&pdev->dev, "Couldn't get regmap for apcs-cci-rcg\n");
			return PTR_ERR(apcs_mux_cci_clk.clkr.regmap);
		}
	}

	/* Get speed bin information */
	cpucc_clk_get_speed_bin(pdev, &speed_bin, &version);

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d-%s", speed_bin, version, "c1");

	ret = cpucc_clk_get_fmax_vdd_class(pdev,
		(struct clk_init_data *)apcs_mux_c1_clk.clkr.hw.init,
				 prop_name);
	if (ret) {
		dev_err(&pdev->dev, "Didn't get c1 speed bin\n");

		snprintf(prop_name, ARRAY_SIZE(prop_name),
				"qcom,speed0-bin-v0-%s", "c1");
		ret = cpucc_clk_get_fmax_vdd_class(pdev,
				(struct clk_init_data *)
				apcs_mux_c1_clk.clkr.hw.init,
					prop_name);
		if (ret) {
			dev_err(&pdev->dev,
				"Unable to load safe voltage plan for c1\n");
			return ret;
		}
	}

	if (is_sdm429 || is_sdm439) {
		snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d-%s", speed_bin, version, "cci");

		ret = cpucc_clk_get_fmax_vdd_class(pdev,
			(struct clk_init_data *)apcs_mux_cci_clk.clkr.hw.init,
				 prop_name);
		if (ret) {
			dev_err(&pdev->dev, "Didn't get cci speed bin\n");

			snprintf(prop_name, ARRAY_SIZE(prop_name),
				"qcom,speed0-bin-v0-%s", "cci");
			ret = cpucc_clk_get_fmax_vdd_class(pdev,
				(struct clk_init_data *)
				apcs_mux_cci_clk.clkr.hw.init,
					prop_name);
			if (ret) {
				dev_err(&pdev->dev,
					"Unable to load safe voltage plan for cci\n");
				return ret;
			}
		}
	}

	if (is_sdm439) {
		ret = fixup_for_sdm439(pdev, speed_bin, version);
		if (ret) {
			dev_err(&pdev->dev, "Unable get sdm439 clocks\n");
			return ret;
		}
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (is_sdm429) {
		data->num = ARRAY_SIZE(cpu_clks_hws_sdm429);

		for (i = 0; i < ARRAY_SIZE(cpu_clks_hws_sdm429); i++) {
			ret = devm_clk_hw_register(dev,
					cpu_clks_hws_sdm429[i]);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to register clock\n");
				return ret;
			}
			data->hws[i] = cpu_clks_hws_sdm429[i];
		}
	} else if (is_qm215) {
		data->num = ARRAY_SIZE(cpu_clks_hws_qm215);

		for (i = 0; i < ARRAY_SIZE(cpu_clks_hws_qm215); i++) {
			ret = devm_clk_hw_register(dev,
					cpu_clks_hws_qm215[i]);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to register clock\n");
				return ret;
			}
			data->hws[i] = cpu_clks_hws_qm215[i];
		}
	} else if (is_sdm439) {
		data->num = ARRAY_SIZE(cpu_clks_hws_sdm439);

		for (i = 0; i < ARRAY_SIZE(cpu_clks_hws_sdm439); i++) {
			ret = devm_clk_hw_register(dev,
					cpu_clks_hws_sdm439[i]);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to register clock\n");
				return ret;
			}
			data->hws[i] = cpu_clks_hws_sdm439[i];
		}
	}

	ret = of_clk_add_hw_provider(dev->of_node, of_clk_hw_onecell_get, data);
	if (ret) {
		dev_err(&pdev->dev, "CPU clock driver registration failed\n");
		return ret;
	}

	/* For safe freq switching during rate change */
	if (is_sdm439) {
		apcs_mux_c0_clk.clk_lpm.hw_low_power_ctrl = true;
		ret = clk_notifier_register(apcs_mux_c0_clk.clkr.hw.clk,
						&apcs_mux_c0_clk.clk_nb);
		if (ret) {
			dev_err(dev, "failed to register clock notifier: %d\n",
					ret);
			return ret;
		}
		ret = clk_prepare_enable(apcs_cpu_pll0.clkr.hw.clk);
		if (ret) {
			dev_err(dev, "failed to Enable PLL0 clock: %d\n", ret);
			return ret;
		}
	}

	apcs_mux_c1_clk.clk_lpm.hw_low_power_ctrl = true;
	ret = clk_notifier_register(apcs_mux_c1_clk.clkr.hw.clk,
						&apcs_mux_c1_clk.clk_nb);
	if (ret) {
		dev_err(dev, "failed to register clock notifier: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(apcs_cpu_pll1.clkr.hw.clk);
	if (ret) {
		dev_err(dev, "failed to Enable PLL1 clock: %d\n", ret);
		return ret;
	}

	/*
	 * To increase the enable count for the clocks so
	 * that they dont get disabled during late init.
	 */
	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (!(cpu/4)) {
			WARN(clk_prepare_enable(apcs_mux_c1_clk.clkr.hw.clk),
				"Unable to turn on CPU clock\n");
		}

		if (cpu/4 && is_sdm439) {
			WARN(clk_prepare_enable(apcs_mux_c0_clk.clkr.hw.clk),
				"Unable to turn on CPU clock\n");
		}

		if (is_sdm429 || is_sdm439)
			clk_prepare_enable(apcs_mux_cci_clk.clkr.hw.clk);
	}
	put_online_cpus();

	if (is_sdm439)
		register_pm_notifier(&clock_sdm439_pm_notifier);
	else if (is_sdm429)
		register_pm_notifier(&clock_sdm429_pm_notifier);
	else if (is_qm215)
		register_pm_notifier(&clock_qm215_pm_notifier);

	cpucc_clk_populate_opp_table(pdev, is_sdm439);
	dev_info(dev, "CPU clock Driver probed successfully\n");

	return ret;
}

static struct platform_driver cpu_clk_driver = {
	.probe = cpucc_driver_probe,
	.driver = {
		.name = "qcom-cpu-sdm",
		.of_match_table = match_table,
	},
};

static int __init cpu_clk_init(void)
{
	return platform_driver_register(&cpu_clk_driver);
}
subsys_initcall(cpu_clk_init);

static void __exit cpu_clk_exit(void)
{
	platform_driver_unregister(&cpu_clk_driver);
}
module_exit(cpu_clk_exit);

#define REG_OFFSET	0x4
#define APCS_PLL0	0x0b116000
#define APCS_PLL1	0x0b016000
#define A53SS_MUX_C0	0x0b111050
#define A53SS_MUX_C1	0x0b011050

static void config_enable_sr2_pll(void __iomem *base)
{
	/* Configure L/M/N values */
	writel_relaxed(0x34, base + apcs_cpu_pll0.l_reg);
	writel_relaxed(0x0, base + apcs_cpu_pll0.m_reg);
	writel_relaxed(0x1, base + apcs_cpu_pll0.n_reg);

	/* Configure USER_CTL value */
	writel_relaxed(0xf, base + apcs_cpu_pll0.config_reg);

	/* Enable the pll */
	writel_relaxed(0x2, base + apcs_cpu_pll0.mode_reg);
	udelay(2);
	writel_relaxed(0x6, base + apcs_cpu_pll0.mode_reg);
	udelay(50);
	writel_relaxed(0x7, base + apcs_cpu_pll0.mode_reg);
	/* Ensure that the writes go through before enabling PLL */
	mb();
}

static void config_enable_hf_pll(void __iomem *base)
{
	/* Configure USER_CTL value */
	writel_relaxed(0xf, base + apcs_cpu_pll1.config_reg);

	/* Enable the pll */
	writel_relaxed(0x2, base + apcs_cpu_pll1.mode_reg);
	udelay(2);
	writel_relaxed(0x6, base + apcs_cpu_pll1.mode_reg);
	udelay(50);
	writel_relaxed(0x7, base + apcs_cpu_pll1.mode_reg);
	/* Ensure that the writes go through before enabling PLL */
	mb();
}

static int __init cpu_clock_init(void)
{
	struct device_node *dev;
	void __iomem  *base;
	int count, regval = 0;
	bool is_sdm439 = false;
	unsigned long enable_mask = GENMASK(2, 0);
	dev = of_find_compatible_node(NULL, NULL, "qcom,cpu-clock-sdm439");

	if (dev)
		is_sdm439 = true;

	if (!dev)
		dev = of_find_compatible_node(NULL, NULL,
				"qcom,cpu-clock-sdm429");

	if (!dev)
		dev = of_find_compatible_node(NULL, NULL,
				"qcom,cpu-clock-qm215");
	if (!dev) {
		pr_err("device node not initialized\n");
		return -ENOMEM;
	}

	if (is_sdm439) {
		base = ioremap_nocache(APCS_PLL0, SZ_64);
		if (!base)
			return -ENOMEM;

		regval = readl_relaxed(base);
		if (!((regval & enable_mask) == enable_mask))
			config_enable_sr2_pll(base);

		iounmap(base);
	}

	base = ioremap_nocache(APCS_PLL1, SZ_64);
	if (!base)
		return -ENOMEM;

	regval = readl_relaxed(base);
	if (!((regval & enable_mask) == enable_mask))
		config_enable_hf_pll(base);

	iounmap(base);

	base = ioremap_nocache(A53SS_MUX_C1, SZ_8);
	if (!base)
		return -ENOMEM;

	writel_relaxed(0x501, base + REG_OFFSET);

	/* Update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		if ((!(readl_relaxed(base))) & BIT(0))
			break;
		udelay(1);
	}

	/* Update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		if ((!(readl_relaxed(base))) & BIT(0))
			break;
		udelay(1);
	}

	iounmap(base);
	return 0;
}
early_initcall(cpu_clock_init);

static int __init clock_cpu_lpm_get_latency(void)
{
	int rc;
	bool is_sdm439 = false;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
					"qcom,cpu-clock-sdm439");
	if (ofnode)
		is_sdm439 = true;

	if (!ofnode)
		ofnode = of_find_compatible_node(NULL, NULL,
				"qcom,cpu-clock-sdm429");

	if (!ofnode)
		ofnode = of_find_compatible_node(NULL, NULL,
				"qcom,cpu-clock-qm215");

	if (!ofnode) {
		pr_err("device node not initialized\n");
		return -ENOMEM;
	}

	rc = lpm_get_latency(&apcs_mux_c1_clk.clk_lpm.latency_lvl,
			&apcs_mux_c1_clk.clk_lpm.cpu_latency_no_l2_pc_us);
	if (rc < 0)
		pr_err("Failed to get the L2 PC value for perf\n");

	if (is_sdm439) {
		rc = lpm_get_latency(&apcs_mux_c0_clk.clk_lpm.latency_lvl,
			&apcs_mux_c0_clk.clk_lpm.cpu_latency_no_l2_pc_us);
		if (rc < 0)
			pr_err("Failed to get the L2 PC value for pwr\n");
		pr_debug("Latency for pwr cluster : %d\n",
			apcs_mux_c0_clk.clk_lpm.cpu_latency_no_l2_pc_us);
	}

	pr_debug("Latency for perf cluster : %d\n",
		apcs_mux_c1_clk.clk_lpm.cpu_latency_no_l2_pc_us);

	return rc;
}
late_initcall_sync(clock_cpu_lpm_get_latency);

MODULE_ALIAS("platform:cpu");
MODULE_DESCRIPTION("SDM CPU clock Driver");
MODULE_LICENSE("GPL v2");

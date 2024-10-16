// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

/*
 * Display Port PLL driver block diagram for branch clocks
 *
 * +------------------------+       +------------------------+
 * |   dp_phy_pll_link_clk  |       | dp_phy_pll_vco_div_clk |
 * +------------------------+       +------------------------+
 *             |                               |
 *             |                               |
 *             V                               V
 *        dp_link_clk                     dp_pixel_clk
 *
 *
 */

#include <dt-bindings/clock/mdss-5nm-pll-clk.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include "clk-regmap-mux.h"
#include "dp_hpd.h"
#include "dp_debug.h"
#include "dp_pll.h"

#define DP_PHY_CFG				0x0010
#define DP_PHY_CFG_1				0x0014
#define DP_PHY_PD_CTL				0x001C
#define DP_PHY_MODE				0x0020

#define DP_PHY_AUX_CFG2				0x002C

#define DP_PHY_VCO_DIV				0x0074
#define DP_PHY_TX0_TX1_LANE_CTL			0x007C
#define DP_PHY_TX2_TX3_LANE_CTL			0x00A0

#define DP_PHY_SPARE0				0x00CC
#define DP_PHY_STATUS				0x00E0

/* Tx registers */
#define TXn_CLKBUF_ENABLE			0x0000
#define TXn_TX_EMP_POST1_LVL			0x0004
#define TXn_TX_DRV_LVL				0x0014
#define TXn_TX_DRV_LVL_OFFSET			0x0018
#define TXn_RESET_TSYNC_EN			0x001C
#define TXn_LDO_CONFIG				0x0084
#define TXn_TX_BAND				0x0028
#define TXn_INTERFACE_SELECT			0x0024
#define TXn_RES_CODE_LANE_OFFSET_TX0		0x0044
#define TXn_RES_CODE_LANE_OFFSET_TX1		0x0048
#define TXn_TRANSCEIVER_BIAS_EN			0x0054
#define TXn_HIGHZ_DRVR_EN			0x0058
#define TXn_TX_POL_INV				0x005C
#define TXn_PARRATE_REC_DETECT_IDLE_EN		0x0060
#define TXn_LANE_MODE_1				0x0064
#define TXn_TRAN_DRVR_EMP_EN			0x0078
#define TXn_VMODE_CTRL1				0x007C

/* PLL register offset */
#define QSERDES_COM_BG_TIMER			0x000C
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		0x0044
#define QSERDES_COM_CLK_ENABLE1			0x0048
#define QSERDES_COM_SYS_CLK_CTRL		0x004C
#define QSERDES_COM_SYSCLK_BUF_ENABLE		0x0050
#define QSERDES_COM_PLL_IVCO			0x0058

#define QSERDES_COM_CP_CTRL_MODE0		0x0074
#define QSERDES_COM_PLL_RCTRL_MODE0		0x007C
#define QSERDES_COM_PLL_CCTRL_MODE0		0x0084
#define QSERDES_COM_SYSCLK_EN_SEL		0x0094
#define QSERDES_COM_RESETSM_CNTRL		0x009C
#define QSERDES_COM_LOCK_CMP_EN			0x00A4
#define QSERDES_COM_LOCK_CMP1_MODE0		0x00AC
#define QSERDES_COM_LOCK_CMP2_MODE0		0x00B0

#define QSERDES_COM_DEC_START_MODE0		0x00BC
#define QSERDES_COM_DIV_FRAC_START1_MODE0	0x00CC
#define QSERDES_COM_DIV_FRAC_START2_MODE0	0x00D0
#define QSERDES_COM_DIV_FRAC_START3_MODE0	0x00D4
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0	0x00EC
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0	0x00F0
#define QSERDES_COM_VCO_TUNE_CTRL		0x0108
#define QSERDES_COM_VCO_TUNE_MAP		0x010C
#define QSERDES_COM_VCO_TUNE1_MODE0		0x0110
#define QSERDES_COM_VCO_TUNE2_MODE0		0x0114
#define QSERDES_COM_CMN_STATUS			0x0140

#define QSERDES_COM_CLK_SEL			0x0154
#define QSERDES_COM_HSCLK_SEL			0x0158

#define QSERDES_COM_CORECLK_DIV_MODE0		0x0168

#define QSERDES_COM_CORE_CLK_EN			0x0174
#define QSERDES_COM_C_READY_STATUS		0x0178
#define QSERDES_COM_CMN_CONFIG			0x017C

#define QSERDES_COM_SVS_MODE_CLK_SEL		0x0184

#define QSERDES_COM_SSC_EN_CENTER		0x0010
#define QSERDES_COM_SSC_ADJ_PER1		0x0014
#define QSERDES_COM_SSC_ADJ_PER2		0x0018
#define QSERDES_COM_SSC_PER1			0x001C
#define QSERDES_COM_SSC_PER2			0x0020
#define QSERDES_COM_SSC_STEP_SIZE1_MODE0	0x0024
#define QSERDES_COM_SSC_STEP_SIZE2_MODE0	0x0028

#define DP_PHY_PLL_POLL_SLEEP_US		500
#define DP_PHY_PLL_POLL_TIMEOUT_US		10000

#define DP_VCO_RATE_8100MHZDIV1000		8100000UL
#define DP_VCO_RATE_9720MHZDIV1000		9720000UL
#define DP_VCO_RATE_10800MHZDIV1000		10800000UL

#define DP_5NM_C_READY		BIT(0)
#define DP_5NM_FREQ_DONE	BIT(0)
#define DP_5NM_PLL_LOCKED	BIT(1)
#define DP_5NM_PHY_READY	BIT(1)
#define DP_5NM_TSYNC_DONE	BIT(0)

static int edp_vco_pll_init_db_5nm(struct dp_pll_db *pdb,
		unsigned long rate)
{
	struct dp_pll *pll = pdb->pll;
	u32 spare_value = 0;

	spare_value = dp_pll_read(dp_phy, DP_PHY_SPARE0);
	pdb->lane_cnt = spare_value & 0x0F;
	pdb->orientation = (spare_value & 0xF0) >> 4;

	DP_DEBUG("spare_value=0x%x, ln_cnt=0x%x, orientation=0x%x\n",
			spare_value, pdb->lane_cnt, pdb->orientation);

	pdb->div_frac_start1_mode0 = 0x00;
	pdb->integloop_gain0_mode0 = 0x3f;
	pdb->integloop_gain1_mode0 = 0x00;

	switch (rate) {
	case DP_VCO_HSCLK_RATE_1620MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_9720MHZDIV1000);
		pdb->hsclk_sel = 0x05;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x6f;
		pdb->lock_cmp2_mode0 = 0x08;
		pdb->phy_vco_div = 0x1;
		pdb->lock_cmp_en = 0x04;
		pdb->ssc_step_size1_mode0 = 0x45;
		pdb->ssc_step_size2_mode0 = 0x06;
		break;
	case DP_VCO_HSCLK_RATE_2700MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->hsclk_sel = 0x03;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x0f;
		pdb->lock_cmp2_mode0 = 0x0e;
		pdb->phy_vco_div = 0x1;
		pdb->lock_cmp_en = 0x08;
		pdb->ssc_step_size1_mode0 = 0x45;
		pdb->ssc_step_size2_mode0 = 0x06;
		break;
	case DP_VCO_HSCLK_RATE_5400MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->hsclk_sel = 0x01;
		pdb->dec_start_mode0 = 0x8c;
		pdb->div_frac_start2_mode0 = 0x00;
		pdb->div_frac_start3_mode0 = 0x0a;
		pdb->lock_cmp1_mode0 = 0x1f;
		pdb->lock_cmp2_mode0 = 0x1c;
		pdb->phy_vco_div = 0x2;
		pdb->lock_cmp_en = 0x08;
		pdb->ssc_step_size1_mode0 = 0x5c;
		pdb->ssc_step_size2_mode0 = 0x08;
		break;
	case DP_VCO_HSCLK_RATE_8100MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_8100MHZDIV1000);
		pdb->hsclk_sel = 0x00;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x2f;
		pdb->lock_cmp2_mode0 = 0x2a;
		pdb->phy_vco_div = 0x0;
		pdb->lock_cmp_en = 0x08;
		pdb->ssc_step_size1_mode0 = 0x45;
		pdb->ssc_step_size2_mode0 = 0x06;
		break;
	default:
		DP_ERR("unsupported rate %ld\n", rate);
		return -EINVAL;
	}
	return 0;
}

static int edp_config_vco_rate_5nm(struct dp_pll *pll,
		unsigned long rate)
{
	int rc;
	struct dp_pll_db *pdb = &pll->pll_db;
	u32 status;

	rc = edp_vco_pll_init_db_5nm(pdb, rate);
	if (rc < 0) {
		DP_ERR("VCO Init DB failed\n");
		return rc;
	}

	dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x7d);
	dp_pll_write(dp_phy, DP_PHY_MODE, 0xfc);
	/* Make sure the PLL register writes are done */
	wmb();

	if (readl_poll_timeout_atomic((dp_pll_get_base(dp_pll) +
				QSERDES_COM_CMN_STATUS),
				status, ((status & BIT(7)) > 0),
				5, 100)) {
		DP_ERR("refgen not ready. Status=%x\n", status);
	}

	dp_pll_write(dp_ln_tx0, TXn_LDO_CONFIG, 0x01);
	dp_pll_write(dp_ln_tx1, TXn_LDO_CONFIG, 0x01);
	dp_pll_write(dp_ln_tx0, TXn_LANE_MODE_1, 0x00);
	dp_pll_write(dp_ln_tx1, TXn_LANE_MODE_1, 0x00);

	if (pll->ssc_en) {
		dp_pll_write(dp_pll, QSERDES_COM_SSC_EN_CENTER, 0x01);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_ADJ_PER1, 0x00);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_PER1, 0x36);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_PER2, 0x01);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_STEP_SIZE1_MODE0,
				pdb->ssc_step_size1_mode0);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_STEP_SIZE2_MODE0,
				pdb->ssc_step_size2_mode0);
	}

	dp_pll_write(dp_pll, QSERDES_COM_SVS_MODE_CLK_SEL, 0x01);
	dp_pll_write(dp_pll, QSERDES_COM_SYSCLK_EN_SEL, 0x0b);
	dp_pll_write(dp_pll, QSERDES_COM_SYS_CLK_CTRL, 0x02);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_ENABLE1, 0x0c);
	dp_pll_write(dp_pll, QSERDES_COM_SYSCLK_BUF_ENABLE, 0x06);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_SEL, 0x30);
	dp_pll_write(dp_pll,
		QSERDES_COM_HSCLK_SEL, pdb->hsclk_sel);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_IVCO, 0x0f);
	dp_pll_write(dp_pll,
		QSERDES_COM_LOCK_CMP_EN, pdb->lock_cmp_en);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_CCTRL_MODE0, 0x36);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_RCTRL_MODE0, 0x16);
	dp_pll_write(dp_pll, QSERDES_COM_CP_CTRL_MODE0, 0x06);
	dp_pll_write(dp_pll,
		QSERDES_COM_DEC_START_MODE0, pdb->dec_start_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START1_MODE0, pdb->div_frac_start1_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START2_MODE0, pdb->div_frac_start2_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START3_MODE0, pdb->div_frac_start3_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_CMN_CONFIG, 0x02);
	dp_pll_write(dp_pll,
		QSERDES_COM_INTEGLOOP_GAIN0_MODE0, pdb->integloop_gain0_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_INTEGLOOP_GAIN1_MODE0, pdb->integloop_gain1_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_VCO_TUNE_MAP, 0x00);
	dp_pll_write(dp_pll,
		QSERDES_COM_LOCK_CMP1_MODE0, pdb->lock_cmp1_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_LOCK_CMP2_MODE0, pdb->lock_cmp2_mode0);
	dp_pll_write(dp_phy, DP_PHY_VCO_DIV, pdb->phy_vco_div);
	/* Make sure the PLL register writes are done */
	wmb();

	dp_pll_write(dp_pll, QSERDES_COM_BG_TIMER, 0x0a);
	dp_pll_write(dp_pll, QSERDES_COM_CORECLK_DIV_MODE0, 0x14);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	dp_pll_write(dp_pll,
		QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x17);
	dp_pll_write(dp_pll, QSERDES_COM_CORE_CLK_EN, 0x0f);
	/* Make sure the PHY register writes are done */
	wmb();

	/* TX Lane configuration */
	dp_pll_write(dp_phy, DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	dp_pll_write(dp_phy, DP_PHY_TX2_TX3_LANE_CTL, 0x05);

	/* TX-0 register configuration */
	dp_pll_write(dp_ln_tx0, TXn_TRANSCEIVER_BIAS_EN, 0x03);
	dp_pll_write(dp_ln_tx0, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx0, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx0, TXn_TRAN_DRVR_EMP_EN, 0x01);
	dp_pll_write(dp_ln_tx0, TXn_TX_BAND, 0x4);

	/* TX-1 register configuration */
	dp_pll_write(dp_ln_tx1, TXn_TRANSCEIVER_BIAS_EN, 0x03);
	dp_pll_write(dp_ln_tx1, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx1, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx1, TXn_TRAN_DRVR_EMP_EN, 0x01);
	dp_pll_write(dp_ln_tx1, TXn_TX_BAND, 0x4);
	/* Make sure the PHY register writes are done */
	wmb();

	return 0;
}

enum edp_5nm_pll_status {
	C_READY,
	FREQ_DONE,
	PLL_LOCKED,
	PHY_READY,
	TSYNC_DONE,
};

static char *edp_5nm_pll_get_status_name(enum edp_5nm_pll_status status)
{
	switch (status) {
	case C_READY:
		return "C_READY";
	case FREQ_DONE:
		return "FREQ_DONE";
	case PLL_LOCKED:
		return "PLL_LOCKED";
	case PHY_READY:
		return "PHY_READY";
	case TSYNC_DONE:
		return "TSYNC_DONE";
	default:
		return "unknown";
	}
}

static bool edp_5nm_pll_get_status(struct dp_pll *pll,
		enum edp_5nm_pll_status status)
{
	u32 reg, state, bit;
	void __iomem *base;
	bool success = true;

	switch (status) {
	case C_READY:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_C_READY_STATUS;
		bit = DP_5NM_C_READY;
		break;
	case FREQ_DONE:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_CMN_STATUS;
		bit = DP_5NM_FREQ_DONE;
		break;
	case PLL_LOCKED:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_CMN_STATUS;
		bit = DP_5NM_PLL_LOCKED;
		break;
	case PHY_READY:
		base = dp_pll_get_base(dp_phy);
		reg = DP_PHY_STATUS;
		bit = DP_5NM_PHY_READY;
		break;
	case TSYNC_DONE:
		base = dp_pll_get_base(dp_phy);
		reg = DP_PHY_STATUS;
		bit = DP_5NM_TSYNC_DONE;
		break;
	default:
		return false;
	}

	if (readl_poll_timeout_atomic((base + reg), state,
			((state & bit) > 0),
			DP_PHY_PLL_POLL_SLEEP_US,
			DP_PHY_PLL_POLL_TIMEOUT_US)) {
		DP_ERR("%s failed, status=%x\n",
			edp_5nm_pll_get_status_name(status), state);

		success = false;
	}

	return success;
}

static int edp_pll_enable_5nm(struct dp_pll *pll)
{
	int rc = 0;
	u32 bias_en0, drvr_en0, bias_en1, drvr_en1, phy_cfg_1;

	pll->aux->state &= ~DP_STATE_PLL_LOCKED;

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x01);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x05);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x01);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x09);
	dp_pll_write(dp_pll, QSERDES_COM_RESETSM_CNTRL, 0x20);
	wmb();	/* Make sure the PLL register writes are done */

	if (!edp_5nm_pll_get_status(pll, C_READY)) {
		rc = -EINVAL;
		goto lock_err;
	}

	if (!edp_5nm_pll_get_status(pll, FREQ_DONE)) {
		rc = -EINVAL;
		goto lock_err;
	}

	if (!edp_5nm_pll_get_status(pll, PLL_LOCKED)) {
		rc = -EINVAL;
		goto lock_err;
	}

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x19);
	/* Make sure the PHY register writes are done */
	wmb();

	if (pll->pll_db.lane_cnt == 1) {
		bias_en0 = 0x01;
		bias_en1 = 0x00;
		drvr_en0 = 0x06;
		drvr_en1 = 0x07;
		phy_cfg_1 = 0x01;
	} else if (pll->pll_db.lane_cnt == 2) {
		bias_en0 = 0x03;
		bias_en1 = 0x00;
		drvr_en0 = 0x04;
		drvr_en1 = 0x07;
		phy_cfg_1 = 0x03;
	} else {
		bias_en0 = 0x03;
		bias_en1 = 0x03;
		drvr_en0 = 0x04;
		drvr_en1 = 0x04;
		phy_cfg_1 = 0x0f;
	}

	dp_pll_write(dp_ln_tx0, TXn_HIGHZ_DRVR_EN, drvr_en0);
	dp_pll_write(dp_ln_tx0, TXn_TX_POL_INV, 0x00);
	dp_pll_write(dp_ln_tx1, TXn_HIGHZ_DRVR_EN, drvr_en1);
	dp_pll_write(dp_ln_tx1, TXn_TX_POL_INV, 0x00);
	dp_pll_write(dp_ln_tx0, TXn_TX_DRV_LVL_OFFSET, 0x10);
	dp_pll_write(dp_ln_tx1, TXn_TX_DRV_LVL_OFFSET, 0x10);
	dp_pll_write(dp_ln_tx0,
		TXn_RES_CODE_LANE_OFFSET_TX0, 0x11);
	dp_pll_write(dp_ln_tx0,
		TXn_RES_CODE_LANE_OFFSET_TX1, 0x11);
	dp_pll_write(dp_ln_tx1,
		TXn_RES_CODE_LANE_OFFSET_TX0, 0x11);
	dp_pll_write(dp_ln_tx1,
		TXn_RES_CODE_LANE_OFFSET_TX1, 0x11);

	dp_pll_write(dp_ln_tx0, TXn_TX_EMP_POST1_LVL, 0x10);
	dp_pll_write(dp_ln_tx1, TXn_TX_EMP_POST1_LVL, 0x10);
	dp_pll_write(dp_ln_tx0, TXn_TX_DRV_LVL, 0x1f);
	dp_pll_write(dp_ln_tx1, TXn_TX_DRV_LVL, 0x1f);

	/* Make sure the PHY register writes are done */
	wmb();

	dp_pll_write(dp_ln_tx0, TXn_TRANSCEIVER_BIAS_EN,
		bias_en0);
	dp_pll_write(dp_ln_tx1, TXn_TRANSCEIVER_BIAS_EN,
		bias_en1);
	dp_pll_write(dp_phy, DP_PHY_CFG_1, phy_cfg_1);

	if (!edp_5nm_pll_get_status(pll, PHY_READY)) {
		rc = -EINVAL;
		goto lock_err;
	}

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x18);
	udelay(100);

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x19);
	/* Make sure the PHY register writes are done */
	wmb();

	if (!edp_5nm_pll_get_status(pll, TSYNC_DONE)) {
		rc = -EINVAL;
		goto lock_err;
	}

	if (!edp_5nm_pll_get_status(pll, PHY_READY)) {
		rc = -EINVAL;
		goto lock_err;
	}

	pll->aux->state |= DP_STATE_PLL_LOCKED;
	DP_DEBUG("PLL is locked\n");

lock_err:
	return rc;
}

static void edp_pll_disable_5nm(struct dp_pll *pll)
{
	/* Assert DP PHY power down */
	dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x2);
	/*
	 * Make sure all the register writes to disable PLL are
	 * completed before doing any other operation
	 */
	wmb();
}

static int edp_vco_clk_set_div(struct dp_pll *pll, unsigned int div)
{
	u32 val = 0;

	if (!pll) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	if (is_gdsc_disabled(pll))
		return -EINVAL;

	val = dp_pll_read(dp_phy, DP_PHY_VCO_DIV);
	val &= ~0x03;

	switch (div) {
	case 2:
		val |= 1;
		break;
	case 4:
		val |= 2;
		break;
	case 6:
	/* When div = 6, val is 0, so do nothing here */
		;
		break;
	case 8:
		val |= 3;
		break;
	default:
		DP_DEBUG("unsupported div value %d\n", div);
		return -EINVAL;
	}

	dp_pll_write(dp_phy, DP_PHY_VCO_DIV, val);
	/* Make sure the PHY registers writes are done */
	wmb();

	DP_DEBUG("val=%d div=%x\n", val, div);
	return 0;
}

static int edp_vco_set_rate_5nm(struct dp_pll *pll, unsigned long rate)
{
	int rc = 0;

	if (!pll) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	DP_DEBUG("DP lane CLK rate=%ld\n", rate);

	rc = edp_config_vco_rate_5nm(pll, rate);
	if (rc < 0) {
		DP_ERR("Failed to set clk rate\n");
		return rc;
	}

	return rc;
}

static int edp_pll_configure(struct dp_pll *pll, unsigned long rate)
{
	int rc = 0;

	if (!pll || !rate) {
		DP_ERR("invalid input parameters rate = %lu\n", rate);
		return -EINVAL;
	}

	rate = rate * 10;

	if (rate <= DP_VCO_HSCLK_RATE_1620MHZDIV1000)
		rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000;
	else if (rate <= DP_VCO_HSCLK_RATE_2700MHZDIV1000)
		rate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
	else if (rate <= DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		rate = DP_VCO_HSCLK_RATE_5400MHZDIV1000;
	else
		rate = DP_VCO_HSCLK_RATE_8100MHZDIV1000;

	rc = edp_vco_set_rate_5nm(pll, rate);
	if (rc < 0) {
		DP_ERR("pll rate %s set failed\n", rate);
		return rc;
	}

	pll->vco_rate = rate;
	DP_DEBUG("pll rate %lu set success\n", rate);
	return rc;
}

static int edp_pll_prepare(struct dp_pll *pll)
{
	int rc = 0;

	if (!pll) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	rc = edp_pll_enable_5nm(pll);
	if (rc < 0)
		DP_ERR("ndx=%d failed to enable dp pll\n", pll->index);

	return rc;
}

static int  edp_pll_unprepare(struct dp_pll *pll)
{
	int rc = 0;

	if (!pll) {
		DP_ERR("invalid input parameter\n");
		return -EINVAL;
	}

	edp_pll_disable_5nm(pll);

	return rc;
}

static unsigned long edp_pll_link_clk_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;
	unsigned long rate = 0;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	rate = pll->vco_rate;
	rate = pll->vco_rate * 100;

	return rate;
}

static long edp_pll_link_clk_round(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	rate = pll->vco_rate * 100;

	return rate;
}

static unsigned long edp_pll_vco_div_clk_get_rate(struct dp_pll *pll)
{
	if (pll->vco_rate == DP_VCO_HSCLK_RATE_8100MHZDIV1000)
		return (pll->vco_rate / 6 * 1000);
	else if (pll->vco_rate == DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		return (pll->vco_rate / 4 * 1000);
	else
		return (pll->vco_rate / 2 * 1000);
}

static unsigned long edp_pll_vco_div_clk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	return edp_pll_vco_div_clk_get_rate(pll);
}

static long edp_pll_vco_div_clk_round(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	return edp_pll_vco_div_clk_recalc_rate(hw, *parent_rate);
}

static int edp_pll_vco_div_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				       unsigned long parent_rate)
{
	struct dp_pll *pll = NULL;
	struct dp_pll_vco_clk *pll_link = NULL;
	int rc = 0;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	pll_link = to_dp_vco_hw(hw);
	pll = pll_link->priv;

	if (rate != edp_pll_vco_div_clk_get_rate(pll)) {
		DP_ERR("unsupported rate %lu failed\n", rate);
		return rc;
	}

	rate /= 1000;

	rc = edp_vco_clk_set_div(pll, pll->vco_rate / rate);
	if (rc < 0) {
		DP_DEBUG("set rate %lu failed\n", rate);
		return rc;
	}

	DP_DEBUG("set rate %lu success\n", rate);
	return 0;
}

static const struct clk_ops edp_pll_link_clk_ops = {
	.recalc_rate = edp_pll_link_clk_recalc_rate,
	.round_rate = edp_pll_link_clk_round,
};

static const struct clk_ops edp_pll_vco_div_clk_ops = {
	.recalc_rate = edp_pll_vco_div_clk_recalc_rate,
	.round_rate = edp_pll_vco_div_clk_round,
	.set_rate = edp_pll_vco_div_clk_set_rate,
};

static struct clk_init_data edp_phy_pll_clks[DP_PLL_NUM_CLKS] = {
	{
		.name = "_phy_pll_link_clk",
		.ops = &edp_pll_link_clk_ops,
	},
	{
		.name = "_phy_pll_vco_div_clk",
		.ops = &edp_pll_vco_div_clk_ops,
	},
};

static struct dp_pll_vco_clk *edp_pll_get_clks(struct dp_pll *pll)
{
	int i;

	for (i = 0; i < DP_PLL_NUM_CLKS; i++) {
		snprintf(pll->pll_clks[i].name, DP_PLL_NAME_MAX_SIZE,
				"%s%s", pll->name, edp_phy_pll_clks[i].name);
		pll->pll_clks[i].init_data.name = pll->pll_clks[i].name;
		pll->pll_clks[i].init_data.ops = edp_phy_pll_clks[i].ops;
		pll->pll_clks[i].hw.init = &pll->pll_clks[i].init_data;
	}

	return pll->pll_clks;
}

int edp_pll_clock_register_5nm(struct dp_pll *pll)
{
	int rc = 0;
	struct platform_device *pdev;
	struct dp_pll_vco_clk *pll_clks;

	if (!pll) {
		DP_ERR("pll data not initialized\n");
		return -EINVAL;
	}
	pdev = pll->pdev;

	pll->clk_data = kzalloc(sizeof(*pll->clk_data), GFP_KERNEL);
	if (!pll->clk_data)
		return -ENOMEM;

	pll->clk_data->clks = kcalloc(DP_PLL_NUM_CLKS, sizeof(struct clk *),
			GFP_KERNEL);
	if (!pll->clk_data->clks) {
		kfree(pll->clk_data);
		return -ENOMEM;
	}

	pll->clk_data->clk_num = DP_PLL_NUM_CLKS;
	pll->pll_db.pll = pll;

	pll->pll_cfg = edp_pll_configure;
	pll->pll_prepare = edp_pll_prepare;
	pll->pll_unprepare = edp_pll_unprepare;

	pll_clks = edp_pll_get_clks(pll);

	rc = dp_pll_clock_register_helper(pll, pll_clks, DP_PLL_NUM_CLKS);
	if (rc) {
		DP_ERR("Clock register failed rc=%d\n", rc);
		goto clk_reg_fail;
	}

	rc = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, pll->clk_data);
	if (rc) {
		DP_ERR("Clock add provider failed rc=%d\n", rc);
		goto clk_reg_fail;
	}

	DP_DEBUG("success\n");
	return rc;

clk_reg_fail:
	dp_pll_clock_unregister_5nm(pll);
	return rc;
}

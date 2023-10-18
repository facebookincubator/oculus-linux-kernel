// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include "dp_catalog.h"
#include "dp_reg.h"
#include "dp_debug.h"

#define MMSS_DP_M_OFF				(0x8)
#define MMSS_DP_N_OFF				(0xC)

#define dp_catalog_get_priv_v500(x) ({ \
	struct dp_catalog *catalog; \
	catalog = container_of(x, struct dp_catalog, x); \
	container_of(catalog->sub, \
		struct dp_catalog_private_v500, sub); \
})

#define dp_read(x) ({ \
	catalog->sub.read(catalog->dpc, io_data, x); \
})

#define dp_write(x, y) ({ \
	catalog->sub.write(catalog->dpc, io_data, x, y); \
})

#define MAX_VOLTAGE_LEVELS 4
#define MAX_PRE_EMP_LEVELS 4

enum {
	TX_DRIVE_MODE_LOW_SWING_LOW_HBR	= 0,
	TX_DRIVE_MODE_HIGH_SWING_LOW_HBR,
	TX_DRIVE_MODE_LOW_SWING_HIGH_HBR,
	TX_DRIVE_MODE_HIGH_SWING_HIGH_HBR,
	TX_DRIVE_MODE_DP,
	TX_DRIVE_MODE_MINIDP,
	TX_DRIVE_MODE_MAX,
};

static u8 const ldo_config[TX_DRIVE_MODE_MAX] = {
	0x81,	/* 600mV */
	0x00,	/* off */
	0x41,	/* 650mV */
	0x00,	/* off */
	0x00,	/* off */
	0x00,	/* off */
};

static u8 const vm_pre_emphasis
	[TX_DRIVE_MODE_MAX][MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	/* pe0, 0 db; pe1, 2.0 db; pe2, 3.6 db; pe3, 6.0 db */
	{	// Low swing/pre-emphasis, low HBR
		{0x05, 0x12, 0x17, 0x1D}, /* sw0, 0.2v  */
		{0x05, 0x11, 0x18, 0xFF}, /* sw1, 0.25 v */
		{0x06, 0x11, 0xFF, 0xFF}, /* sw2, 0.3 v */
		{0x00, 0xFF, 0xFF, 0xFF}  /* sw3, 0.35 v */
	},
	{	// High swing/pre-emphasis, low HBR
		{0x02, 0x0F, 0x17, 0x1D}, /* sw0, 0.3v  */
		{0x01, 0x0F, 0x17, 0xFF}, /* sw1, 0.35v  */
		{0x02, 0x0F, 0xFF, 0xFF}, /* sw2, 0.4v  */
		{0x00, 0xFF, 0xFF, 0xFF}  /* sw3, 0.45v  */
	},
	{	// Low swing/pre-emphasis, high HBR
		{0x0C, 0x15, 0x19, 0x1E}, /* sw0, 0.2v  */
		{0x08, 0x15, 0x19, 0xFF}, /* sw1, 0.25v  */
		{0x0E, 0x14, 0xFF, 0xFF}, /* sw2, 0.3v  */
		{0x0D, 0xFF, 0xFF, 0xFF}  /* sw3, 0.35v  */
	},
	{	// High swing/pre-emphasis, high HBR
		{0x08, 0x11, 0x17, 0x1B}, /* sw0, 0.3v  */
		{0x00, 0x0C, 0x13, 0xFF}, /* sw1, 0.35v  */
		{0x05, 0x10, 0xFF, 0xFF}, /* sw2, 0.4v  */
		{0x00, 0xFF, 0xFF, 0xFF}  /* sw3, 0.45v  */
	},
	{	// DP-only, DP/USB
		{0x20, 0x2E, 0x35, 0xFF}, /* sw0, 0.4v  */
		{0x20, 0x2E, 0x35, 0xFF}, /* sw1, 0.6v  */
		{0x20, 0x2E, 0xFF, 0xFF}, /* sw2, 0.8v  */
		{0xFF, 0xFF, 0xFF, 0xFF}  /* sw3, 1.2 v, optional */
	},
	{	// MiniDP-only
		{0x00, 0x0E, 0x17, 0xFF}, /* sw0, 0.4v  */
		{0x00, 0x0D, 0x16, 0xFF}, /* sw1, 0.6v  */
		{0x00, 0x0D, 0xFF, 0xFF}, /* sw2, 0.8v  */
		{0xFF, 0xFF, 0xFF, 0xFF}  /* sw3, 1.2 v, optional */
	},
};

/* voltage swing, 0.2v and 1.0v are not support */
static u8 const vm_voltage_swing
	[TX_DRIVE_MODE_MAX][MAX_VOLTAGE_LEVELS][MAX_PRE_EMP_LEVELS] = {
	/* pe0, 0 db; pe1, 2.0 db; pe2, 3.6 db; pe3, 6.0 db */
	{	// Low swing/pre-emphasis, low HBR
		{0x07, 0x0F, 0x16, 0x1F}, /* sw0, 0.2v  */
		{0x0D, 0x16, 0x1E, 0xFF}, /* sw1, 0.25 v */
		{0x11, 0x1B, 0xFF, 0xFF}, /* sw1, 0.3 v */
		{0x16, 0xFF, 0xFF, 0xFF}  /* sw1, 0.35 v */
	},
	{	// High swing/pre-emphasis, low HBR
		{0x05, 0x0C, 0x14, 0x1D}, /* sw0, 0.3v  */
		{0x08, 0x13, 0x1B, 0xFF}, /* sw1, 0.35 v */
		{0x0C, 0x17, 0xFF, 0xFF}, /* sw1, 0.4 v */
		{0x14, 0xFF, 0xFF, 0xFF}  /* sw1, 0.45 v */
	},
	{	// Low swing/pre-emphasis, high HBR
		{0x0B, 0x11, 0x17, 0x1C}, /* sw0, 0.2v  */
		{0x10, 0x19, 0x1F, 0xFF}, /* sw1, 0.25 v */
		{0x19, 0x1F, 0xFF, 0xFF}, /* sw1, 0.3 v */
		{0x1F, 0xFF, 0xFF, 0xFF}  /* sw1, 0.35 v */
	},
	{	// High swing/pre-emphasis, high HBR
		{0x0A, 0x11, 0x17, 0x1F}, /* sw0, 0.3v  */
		{0x0C, 0x14, 0x1D, 0xFF}, /* sw1, 0.35 v */
		{0x15, 0x1F, 0xFF, 0xFF}, /* sw1, 0.4 v */
		{0x17, 0xFF, 0xFF, 0xFF}  /* sw1, 0.45 v */
	},
	{	// DP-only, DP/USB
		{0x27, 0x2F, 0x36, 0xFF}, /* sw0, 0.4v  */
		{0x31, 0x3E, 0x3F, 0xFF}, /* sw1, 0.6v  */
		{0x3A, 0x3F, 0xFF, 0xFF}, /* sw2, 0.8v  */
		{0xFF, 0xFF, 0xFF, 0xFF}  /* sw3, 1.2 v, optional */
	},
	{	// MiniDP-only
		{0x09, 0x17, 0x1F, 0xFF}, /* sw0, 0.4v  */
		{0x11, 0x1D, 0x1F, 0xFF}, /* sw1, 0.6v  */
		{0x1C, 0x1F, 0xFF, 0xFF}, /* sw2, 0.8v  */
		{0xFF, 0xFF, 0xFF, 0xFF}  /* sw3, 1.2 v, optional */
	},
};

struct dp_catalog_private_v500 {
	struct device *dev;
	struct dp_catalog_sub sub;
	struct dp_catalog_io *io;
	struct dp_catalog *dpc;

	struct dp_catalog_ctrl dp_ctrl;
	struct dp_catalog_audio dp_audio;
};

static void dp_catalog_aux_setup_v500(struct dp_catalog_aux *aux,
		struct dp_aux_cfg *cfg)
{
	struct dp_catalog_private_v500 *catalog;
	struct dp_io_data *io_data;
	u32 revision_id = 0;
	int i = 0;

	if (!aux || !cfg) {
		DP_ERR("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v500(aux);

	io_data = catalog->io->dp_phy;
	/* PHY will not work if DP_PHY_MODE is not set */
	revision_id = (dp_read(DP_PHY_REVISION_ID3) & 0xFF) << 8;
	revision_id |= (dp_read(DP_PHY_REVISION_ID2) & 0xFF);
	DP_DEBUG("DP phy revision_id: 0x%X\n", revision_id);
	if (revision_id > 0x5000)
		dp_write(DP_PHY_MODE_V500, 0xfc);
	else
		dp_write(DP_PHY_MODE, 0xfc);

	dp_write(DP_PHY_PD_CTL_V500, 0x7d);
	wmb(); /* make sure PD programming happened */

	/* Turn on BIAS current for PHY/PLL */
	io_data = catalog->io->dp_pll;
	dp_write(QSERDES_COM_BIAS_EN_CLKBUFLR_EN,
			0x17);
	wmb(); /* make sure BIAS programming happened */

	io_data = catalog->io->dp_phy;
	/* DP AUX CFG register programming */
	for (i = 0; i < PHY_AUX_CFG_MAX; i++) {
		DP_DEBUG("%s: offset=0x%08x, value=0x%08x\n",
			dp_phy_aux_config_type_to_string(i),
			cfg[i].offset, cfg[i].lut[cfg[i].current_index]);
		dp_write(cfg[i].offset,
			cfg[i].lut[cfg[i].current_index]);
	}
	wmb(); /* make sure DP AUX CFG programming happened */

	dp_write(DP_PHY_AUX_INTERRUPT_MASK_V500,
			0x1F);
}

static void dp_catalog_aux_clear_hw_interrupts_v500(struct dp_catalog_aux *aux)
{
	struct dp_catalog_private_v500 *catalog;
	struct dp_io_data *io_data;
	u32 data = 0;

	if (!aux) {
		DP_ERR("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v500(aux);
	io_data = catalog->io->dp_phy;

	data = dp_read(DP_PHY_AUX_INTERRUPT_STATUS_V500);

	dp_write(DP_PHY_AUX_INTERRUPT_CLEAR_V500, 0x1f);
	wmb(); /* make sure 0x1f is written before next write */
	dp_write(DP_PHY_AUX_INTERRUPT_CLEAR_V500, 0x9f);
	wmb(); /* make sure 0x9f is written before next write */
	dp_write(DP_PHY_AUX_INTERRUPT_CLEAR_V500, 0);
	wmb(); /* make sure register is cleared */
}

static void dp_catalog_panel_config_msa_v500(struct dp_catalog_panel *panel,
					u32 rate, u32 stream_rate_khz)
{
	u32 pixel_m, pixel_n;
	u32 mvid, nvid, reg_off = 0, mvid_off = 0, nvid_off = 0;
	u32 const nvid_fixed = 0x8000;
	u32 const link_rate_hbr2 = 540000;
	u32 const link_rate_hbr3 = 810000;
	struct dp_catalog_private_v500 *catalog;
	struct dp_io_data *io_data;

	if (!panel || !rate) {
		DP_ERR("invalid input\n");
		return;
	}

	if (panel->stream_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream id:%d\n", panel->stream_id);
		return;
	}

	catalog = dp_catalog_get_priv_v500(panel);
	io_data = catalog->io->dp_mmss_cc;

	if (panel->stream_id == DP_STREAM_1)
		reg_off = catalog->dpc->parser->pixel_base_off[1];
	else
		reg_off = catalog->dpc->parser->pixel_base_off[0];

	pixel_m = dp_read(reg_off + MMSS_DP_M_OFF);
	pixel_n = dp_read(reg_off + MMSS_DP_N_OFF);
	DP_DEBUG("pixel_m=0x%x, pixel_n=0x%x\n", pixel_m, pixel_n);

	mvid = (pixel_m & 0xFFFF) * 5;
	nvid = (0xFFFF & (~pixel_n)) + (pixel_m & 0xFFFF);

	if (nvid < nvid_fixed) {
		u32 temp;

		temp = (nvid_fixed / nvid) * nvid;
		mvid = (nvid_fixed / nvid) * mvid;
		nvid = temp;
	}

	DP_DEBUG("rate = %d\n", rate);

	mvid = mvid * (panel->pclk_factor);

	if (link_rate_hbr2 == rate)
		nvid *= 2;

	if (link_rate_hbr3 == rate)
		nvid *= 3;

	io_data = catalog->io->dp_link;

	if (panel->stream_id == DP_STREAM_1) {
		mvid_off = DP1_SOFTWARE_MVID - DP_SOFTWARE_MVID;
		nvid_off = DP1_SOFTWARE_NVID - DP_SOFTWARE_NVID;
	}

	DP_DEBUG("mvid=0x%x, nvid=0x%x\n", mvid, nvid);
	dp_write(DP_SOFTWARE_MVID + mvid_off, mvid);
	dp_write(DP_SOFTWARE_NVID + nvid_off, nvid);
}

static void dp_catalog_ctrl_phy_lane_cfg_v500(struct dp_catalog_ctrl *ctrl,
		bool flipped, u8 ln_cnt)
{
	u32 info = 0x0;
	struct dp_catalog_private_v500 *catalog;
	u8 orientation = BIT(!!flipped);
	struct dp_io_data *io_data;

	if (!ctrl) {
		DP_ERR("invalid input\n");
		return;
	}

	catalog = dp_catalog_get_priv_v500(ctrl);
	io_data = catalog->io->dp_phy;

	info |= (ln_cnt & 0x0F);
	info |= ((orientation & 0x0F) << 4);
	DP_DEBUG("Shared Info = 0x%x\n", info);

	dp_write(DP_PHY_SPARE0_V500, info);
}

static void dp_catalog_ctrl_update_vx_px_v500(struct dp_catalog_ctrl *ctrl,
		u8 v_level, u8 p_level, bool high)
{
	struct dp_catalog *dp_catalog;
	struct dp_catalog_private_v500 *catalog;
	struct dp_io_data *io_data;
	u8 value0, value1, ldo_cfg;
	int index;

	if (!ctrl || !((v_level < MAX_VOLTAGE_LEVELS)
		&& (p_level < MAX_PRE_EMP_LEVELS))) {
		DP_ERR("invalid input\n");
		return;
	}

	dp_catalog = container_of(ctrl, struct dp_catalog, ctrl);

	catalog = dp_catalog_get_priv_v500(ctrl);

	DP_DEBUG("hw: v=%d p=%d\n", v_level, p_level);

	io_data = catalog->io->dp_phy;

	switch (dp_catalog->parser->hw_cfg.phy_mode) {
	case DP_PHY_MODE_DP:
	case DP_PHY_MODE_UNKNOWN:
		index = TX_DRIVE_MODE_DP;
		break;
	case DP_PHY_MODE_MINIDP:
		index = TX_DRIVE_MODE_MINIDP;
		break;
	case DP_PHY_MODE_EDP:
	default:
		if (!high)
			index = TX_DRIVE_MODE_LOW_SWING_LOW_HBR;
		else
			index = TX_DRIVE_MODE_LOW_SWING_HIGH_HBR;
		break;
	case DP_PHY_MODE_EDP_HIGH_SWING:
		if (!high)
			index = TX_DRIVE_MODE_HIGH_SWING_LOW_HBR;
		else
			index = TX_DRIVE_MODE_HIGH_SWING_HIGH_HBR;
		break;
	}

	value0 = vm_voltage_swing[index][v_level][p_level];
	value1 = vm_pre_emphasis[index][v_level][p_level];
	ldo_cfg = ldo_config[index];

	/* program default setting first */
	io_data = catalog->io->dp_ln_tx0;
	dp_write(TXn_LDO_CONFIG_V500, 0x01);
	dp_write(TXn_TX_DRV_LVL_V500, 0x17);
	dp_write(TXn_TX_EMP_POST1_LVL_V500, 0x00);

	io_data = catalog->io->dp_ln_tx1;
	dp_write(TXn_LDO_CONFIG_V500, 0x01);
	dp_write(TXn_TX_DRV_LVL_V500, 0x2A);
	dp_write(TXn_TX_EMP_POST1_LVL_V500, 0x20);

	/* Configure host and panel only if both values are allowed */
	if (value0 != 0xFF && value1 != 0xFF) {
		io_data = catalog->io->dp_ln_tx0;
		dp_write(TXn_LDO_CONFIG_V500,
				ldo_cfg);
		dp_write(TXn_TX_DRV_LVL_V500,
				value0);
		dp_write(TXn_TX_EMP_POST1_LVL_V500,
				value1);

		io_data = catalog->io->dp_ln_tx1;
		dp_write(TXn_LDO_CONFIG_V500,
				ldo_cfg);
		dp_write(TXn_TX_DRV_LVL_V500,
				value0);
		dp_write(TXn_TX_EMP_POST1_LVL_V500,
				value1);

		DP_DEBUG("hw: vx_value=0x%x px_value=0x%x ldo_value=0x%x\n",
			value0, value1, ldo_cfg);
	} else {
		DP_ERR("invalid vx (0x%x=0x%x), px (0x%x=0x%x)\n",
			v_level, value0, p_level, value1);
	}
}

static void dp_catalog_ctrl_lane_pnswap_v500(struct dp_catalog_ctrl *ctrl,
						u8 ln_pnswap)
{
	struct dp_catalog_private_v500 *catalog;
	struct dp_io_data *io_data;
	u32 cfg0, cfg1;

	catalog = dp_catalog_get_priv_v500(ctrl);

	cfg0 = 0x00;
	cfg1 = 0x00;

	cfg0 |= ((ln_pnswap >> 0) & 0x1) << 0;
	cfg0 |= ((ln_pnswap >> 1) & 0x1) << 1;
	cfg1 |= ((ln_pnswap >> 2) & 0x1) << 0;
	cfg1 |= ((ln_pnswap >> 3) & 0x1) << 1;

	io_data = catalog->io->dp_ln_tx0;
	dp_write(TXn_TX_POL_INV_V500, cfg0);

	io_data = catalog->io->dp_ln_tx1;
	dp_write(TXn_TX_POL_INV_V500, cfg1);
}

static void dp_catalog_ctrl_usb_reset_v500(struct dp_catalog_ctrl *ctrl,
						bool flip)
{
	// USB isn't available
}

static int dp_catalog_ctrl_late_phy_init_v500(struct dp_catalog_ctrl *ctrl,
					u8 lane_cnt, bool flipped)
{
	// No late init
	return 0;
}

static void dp_catalog_put_v500(struct dp_catalog *catalog)
{
	struct dp_catalog_private_v500 *catalog_priv;

	if (!catalog)
		return;

	catalog_priv = container_of(catalog->sub,
			struct dp_catalog_private_v500, sub);
	devm_kfree(catalog_priv->dev, catalog_priv);
}

struct dp_catalog_sub *dp_catalog_get_v500(struct device *dev,
			struct dp_catalog *catalog, struct dp_catalog_io *io)
{
	struct dp_catalog_private_v500 *catalog_priv;

	if (!dev || !catalog) {
		DP_ERR("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	catalog_priv = devm_kzalloc(dev, sizeof(*catalog_priv), GFP_KERNEL);
	if (!catalog_priv)
		return ERR_PTR(-ENOMEM);

	catalog_priv->dev = dev;
	catalog_priv->io = io;
	catalog_priv->dpc = catalog;
	catalog_priv->dp_ctrl = catalog->ctrl;
	catalog_priv->dp_audio = catalog->audio;
	catalog_priv->sub.put	   = dp_catalog_put_v500;

	catalog->aux.setup         = dp_catalog_aux_setup_v500;
	catalog->aux.clear_hw_interrupts =
				dp_catalog_aux_clear_hw_interrupts_v500;
	catalog->panel.config_msa  = dp_catalog_panel_config_msa_v500;

	catalog->ctrl.phy_lane_cfg = dp_catalog_ctrl_phy_lane_cfg_v500;
	catalog->ctrl.update_vx_px = dp_catalog_ctrl_update_vx_px_v500;
	catalog->ctrl.lane_pnswap  = dp_catalog_ctrl_lane_pnswap_v500;
	catalog->ctrl.usb_reset    = dp_catalog_ctrl_usb_reset_v500;
	catalog->ctrl.late_phy_init = dp_catalog_ctrl_late_phy_init_v500;

	return &catalog_priv->sub;
}

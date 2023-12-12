// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include "sde_hwio.h"
#include "sde_hw_ctl.h"
#include "sde_dbg.h"
#include "sde_kms.h"
#include "sde_reg_dma.h"

#define CTL_LAYER(lm)                 \
	(((lm) == LM_5) ? (0x024) : (((lm) - LM_0) * 0x004))
#define CTL_LAYER_EXT(lm)             \
	(0x40 + (((lm) - LM_0) * 0x004))
#define CTL_LAYER_EXT2(lm)             \
	(0x70 + (((lm) - LM_0) * 0x004))
#define CTL_LAYER_EXT3(lm)             \
	(0xA0 + (((lm) - LM_0) * 0x004))
#define CTL_LAYER_EXT4(lm)             \
	(0xB8 + (((lm) - LM_0) * 0x004))
#define CTL_TOP                       0x014
#define CTL_FLUSH                     0x018
#define CTL_START                     0x01C
#define CTL_PREPARE                   0x0d0
#define CTL_SW_RESET                  0x030
#define CTL_SW_RESET_OVERRIDE         0x060
#define CTL_STATUS                    0x064
#define CTL_LAYER_EXTN_OFFSET         0x40
#define CTL_ROT_TOP                   0x0C0
#define CTL_ROT_FLUSH                 0x0C4
#define CTL_ROT_START                 0x0CC

#define CTL_MERGE_3D_ACTIVE           0x0E4
#define CTL_DSC_ACTIVE                0x0E8
#define CTL_WB_ACTIVE                 0x0EC
#define CTL_CWB_ACTIVE                0x0F0
#define CTL_INTF_ACTIVE               0x0F4
#define CTL_CDM_ACTIVE                0x0F8
#define CTL_FETCH_PIPE_ACTIVE         0x0FC

#define CTL_MERGE_3D_FLUSH           0x100
#define CTL_DSC_FLUSH                0x104
#define CTL_WB_FLUSH                 0x108
#define CTL_CWB_FLUSH                0x10C
#define CTL_INTF_FLUSH               0x110
#define CTL_CDM_FLUSH                0x114
#define CTL_PERIPH_FLUSH             0x128
#define CTL_DSPP_0_FLUSH             0x13c

#define CTL_INTF_MASTER               0x134
#define CTL_UIDLE_ACTIVE              0x138

#define CTL_HW_FENCE_CTRL		0x250
#define CTL_FENCE_READY_SW_OVERRIDE	0x254
#define CTL_INPUT_FENCE_ID		0x258
#define CTL_HW_FENCE_STATUS		0x278

#define CTL_MIXER_BORDER_OUT            BIT(24)
#define CTL_FLUSH_MASK_ROT              BIT(27)
#define CTL_FLUSH_MASK_CTL              BIT(17)

#define CTL_NUM_EXT			5
#define CTL_SSPP_MAX_RECTS		2

#define SDE_REG_RESET_TIMEOUT_US        2000
#define SDE_REG_WAIT_RESET_TIMEOUT_US        100000

#define UPDATE_MASK(m, idx, en)           \
	((m) = (en) ? ((m) | BIT((idx))) : ((m) & ~BIT((idx))))

#define CTL_INVALID_BIT                0xffff

#define VDC_IDX(i) ((i) +  16)

#define UPDATE_ACTIVE(r, idx, en)  UPDATE_MASK((r), (idx), (en))

/**
 * List of SSPP bits in CTL_FLUSH
 */
static const u32 sspp_tbl[SSPP_MAX] = { SDE_NONE, 0, 1, 2, 18, 3, 4, 5,
	19, 11, 12, 24, 25, 13, 14, SDE_NONE, SDE_NONE};

/**
 * List of layer mixer bits in CTL_FLUSH
 */
static const u32 mixer_tbl[LM_MAX] = {SDE_NONE, 6, 7, 8, 9, 10, 20,
	SDE_NONE};

/**
 * List of DSPP bits in CTL_FLUSH
 */
static const u32 dspp_tbl[DSPP_MAX] = {SDE_NONE, 13, 14, 15, 21};

/**
 * List of DSPP PA LUT bits in CTL_FLUSH
 */
static const u32 dspp_pav_tbl[DSPP_MAX] = {SDE_NONE, 3, 4, 5, 19};

/**
 * List of CDM LUT bits in CTL_FLUSH
 */
static const u32 cdm_tbl[CDM_MAX] = {SDE_NONE, 26};

/**
 * List of WB bits in CTL_FLUSH
 */
static const u32 wb_tbl[WB_MAX] = {SDE_NONE, SDE_NONE, SDE_NONE, 16};

/**
 * List of ROT bits in CTL_FLUSH
 */
static const u32 rot_tbl[ROT_MAX] = {SDE_NONE, 27};

/**
 * List of INTF bits in CTL_FLUSH
 */
static const u32 intf_tbl[INTF_MAX] = {SDE_NONE, 31, 30, 29, 28};

/**
 * Below definitions are for CTL supporting SDE_CTL_ACTIVE_CFG,
 * certain blocks have the individual flush control as well,
 * for such blocks flush is done by flushing individual control and
 * top level control.
 */

/**
 * List of SSPP bits in CTL_FETCH_PIPE_ACTIVE
 */
static const u32 fetch_tbl[SSPP_MAX] = {CTL_INVALID_BIT, 16, 17, 18, 19,
	CTL_INVALID_BIT, CTL_INVALID_BIT, CTL_INVALID_BIT, CTL_INVALID_BIT, 0,
	1, 2, 3, 4, 5, CTL_INVALID_BIT, CTL_INVALID_BIT};

/**
 * list of WB bits in CTL_WB_FLUSH
 */
static const u32 wb_flush_tbl[WB_MAX] = {SDE_NONE, SDE_NONE, SDE_NONE, 2};

/**
 * list of INTF bits in CTL_INTF_FLUSH
 */
static const u32 intf_flush_tbl[INTF_MAX] = {SDE_NONE, 0, 1, 2, 3, 4, 5, 6, 7, 8};

/**
 * list of DSC bits in CTL_DSC_FLUSH
 */
static const u32 dsc_flush_tbl[DSC_MAX] = {SDE_NONE, 0, 1, 2, 3, 4, 5};

/**
 * list of VDC bits in CTL_DSC_FLUSH
 */
static const u32 vdc_flush_tbl[DSC_MAX] = {SDE_NONE, 16, 17};

/**
 * list of MERGE_3D bits in CTL_MERGE_3D_FLUSH
 */
static const u32 merge_3d_tbl[MERGE_3D_MAX] = {SDE_NONE, 0, 1, 2};

/**
 * list of CDM bits in CTL_CDM_FLUSH
 */
static const u32 cdm_flush_tbl[CDM_MAX] = {SDE_NONE, 0};

/**
 * list of CWB bits in CTL_CWB_FLUSH
 */
static const u32 cwb_flush_tbl[CWB_MAX] = {SDE_NONE, SDE_NONE, 1, 2, 3,
	4, 5};

/**
 * list of CWB bits in CTL_CWB_FLUSH for dedicated cwb
 */
static const u32 dcwb_flush_tbl[CWB_MAX] = {SDE_NONE, SDE_NONE, 0, 1};

/**
 * list of DSPP sub-blk flush bits in CTL_DSPP_x_FLUSH
 */
static const u32 dspp_sub_blk_flush_tbl[SDE_DSPP_MAX] = {
	[SDE_DSPP_IGC] = 2,
	[SDE_DSPP_PCC] = 4,
	[SDE_DSPP_GC] = 5,
	[SDE_DSPP_HSIC] = 0,
	[SDE_DSPP_MEMCOLOR] = 0,
	[SDE_DSPP_SIXZONE] = 0,
	[SDE_DSPP_GAMUT] = 3,
	[SDE_DSPP_DITHER] = 0,
	[SDE_DSPP_HIST] = 0,
	[SDE_DSPP_VLUT] = 1,
	[SDE_DSPP_AD] = 0,
	[SDE_DSPP_LTM] = 7,
	[SDE_DSPP_SPR] = 8,
	[SDE_DSPP_DEMURA] = 9,
	[SDE_DSPP_RC] = 10,
	[SDE_DSPP_SB] = 31,
};

/**
 * struct ctl_sspp_stage_reg_map: Describes bit layout for a sspp stage cfg
 * @ext: Index to indicate LAYER_x_EXT id for given sspp
 * @start: Start position of blend stage bits for given sspp
 * @bits: Number of bits from @start assigned for given sspp
 * @sec_bit_mask: Bitmask to add to LAYER_x_EXT1 for missing bit of sspp
 */
struct ctl_sspp_stage_reg_map {
	u32 ext;
	u32 start;
	u32 bits;
	u32 sec_bit_mask;
};

/* list of ctl_sspp_stage_reg_map for all the sppp */
static const struct ctl_sspp_stage_reg_map
sspp_reg_cfg_tbl[SSPP_MAX][CTL_SSPP_MAX_RECTS] = {
	/* SSPP_NONE */{ {0, 0, 0, 0}, {0, 0, 0, 0} },
	/* SSPP_VIG0 */{ {0, 0, 3, BIT(0)}, {3, 0, 4, 0} },
	/* SSPP_VIG1 */{ {0, 3, 3, BIT(2)}, {3, 4, 4, 0} },
	/* SSPP_VIG2 */{ {0, 6, 3, BIT(4)}, {3, 8, 4, 0} },
	/* SSPP_VIG3 */{ {0, 26, 3, BIT(6)}, {3, 12, 4, 0} },
	/* SSPP_RGB0 */{ {0, 9, 3, BIT(8)}, {0, 0, 0, 0} },
	/* SSPP_RGB1 */{ {0, 12, 3, BIT(10)}, {0, 0, 0, 0} },
	/* SSPP_RGB2 */{ {0, 15, 3, BIT(12)}, {0, 0, 0, 0} },
	/* SSPP_RGB3 */{ {0, 29, 3, BIT(14)}, {0, 0, 0, 0} },
	/* SSPP_DMA0 */{ {0, 18, 3, BIT(16)}, {2, 8, 4, 0} },
	/* SSPP_DMA1 */{ {0, 21, 3, BIT(18)}, {2, 12, 4, 0} },
	/* SSPP_DMA2 */{ {2, 0, 4, 0}, {2, 16, 4, 0} },
	/* SSPP_DMA3 */{ {2, 4, 4, 0}, {2, 20, 4, 0} },
	/* SSPP_DMA4 */{ {4, 0, 4, 0}, {4, 8, 4, 0} },
	/* SSPP_DMA5 */{ {4, 4, 4, 0}, {4, 12, 4, 0} },
	/* SSPP_CURSOR0 */{ {1, 20, 4, 0}, {0, 0, 0, 0} },
	/* SSPP_CURSOR1 */{ {1, 26, 4, 0}, {0, 0, 0, 0} }
};

/**
 * Individual flush bit in CTL_FLUSH
 */
#define WB_IDX         16
#define DSC_IDX        22
#define MERGE_3D_IDX   23
#define CDM_IDX        26
#define CWB_IDX        28
#define DSPP_IDX       29
#define PERIPH_IDX     30
#define INTF_IDX       31

/* struct ctl_hw_flush_cfg: Defines the active ctl hw flush config,
 *     See enum ctl_hw_flush_type for types
 * @blk_max: Maximum hw idx
 * @flush_reg: Register with corresponding active ctl hw
 * @flush_idx: Corresponding index in ctl flush
 * @flush_mask_idx: Index of hw flush mask to use
 * @flush_tbl: Pointer to flush table
 */
struct ctl_hw_flush_cfg {
	u32 blk_max;
	u32 flush_reg;
	u32 flush_idx;
	u32 flush_mask_idx;
	const u32 *flush_tbl;
};

static const struct ctl_hw_flush_cfg
		ctl_hw_flush_cfg_tbl_v1[SDE_HW_FLUSH_MAX] = {
	{WB_MAX, CTL_WB_FLUSH, WB_IDX, SDE_HW_FLUSH_WB,
			wb_flush_tbl}, /* SDE_HW_FLUSH_WB */
	{DSC_MAX, CTL_DSC_FLUSH, DSC_IDX, SDE_HW_FLUSH_DSC,
			dsc_flush_tbl}, /* SDE_HW_FLUSH_DSC */
	/* VDC is flushed to dsc, flush_reg = 0 so flush is done only once */
	{VDC_MAX, 0, DSC_IDX, SDE_HW_FLUSH_DSC,
			vdc_flush_tbl}, /* SDE_HW_FLUSH_VDC */
	{MERGE_3D_MAX, CTL_MERGE_3D_FLUSH, MERGE_3D_IDX, SDE_HW_FLUSH_MERGE_3D,
			merge_3d_tbl}, /* SDE_HW_FLUSH_MERGE_3D */
	{CDM_MAX, CTL_CDM_FLUSH, CDM_IDX, SDE_HW_FLUSH_CDM,
			cdm_flush_tbl}, /* SDE_HW_FLUSH_CDM */
	{CWB_MAX, CTL_CWB_FLUSH, CWB_IDX, SDE_HW_FLUSH_CWB,
			cwb_flush_tbl}, /* SDE_HW_FLUSH_CWB */
	{INTF_MAX, CTL_PERIPH_FLUSH, PERIPH_IDX, SDE_HW_FLUSH_PERIPH,
			intf_flush_tbl }, /* SDE_HW_FLUSH_PERIPH */
	{INTF_MAX, CTL_INTF_FLUSH, INTF_IDX, SDE_HW_FLUSH_INTF,
			intf_flush_tbl } /* SDE_HW_FLUSH_INTF */
};

static struct sde_ctl_cfg *_ctl_offset(enum sde_ctl ctl,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->ctl_count; i++) {
		if (ctl == m->ctl[i].id) {
			b->base_off = addr;
			b->blk_off = m->ctl[i].base;
			b->length = m->ctl[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_CTL;
			return &m->ctl[i];
		}
	}
	return ERR_PTR(-ENOMEM);
}

static int _mixer_stages(const struct sde_lm_cfg *mixer, int count,
		enum sde_lm lm)
{
	int i;
	int stages = -EINVAL;

	for (i = 0; i < count; i++) {
		if (lm == mixer[i].id) {
			stages = mixer[i].sblk->maxblendstages;
			break;
		}
	}

	return stages;
}

static inline bool _is_dspp_flush_pending(struct sde_hw_ctl *ctx)
{
	int i;

	for (i = 0; i < CTL_MAX_DSPP_COUNT; i++) {
		if (ctx->flush.pending_dspp_flush_masks[i])
			return true;
	}

	return false;
}

static inline int sde_hw_ctl_trigger_start(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	SDE_REG_WRITE(&ctx->hw, CTL_START, 0x1);
	return 0;
}

static inline int sde_hw_ctl_get_start_state(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	return SDE_REG_READ(&ctx->hw, CTL_START);
}

static inline int sde_hw_ctl_trigger_pending(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	SDE_REG_WRITE(&ctx->hw, CTL_PREPARE, 0x1);
	return 0;
}

static inline int sde_hw_ctl_clear_pending_flush(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	memset(&ctx->flush, 0, sizeof(ctx->flush));
	return 0;
}

static inline int sde_hw_ctl_update_pending_flush(struct sde_hw_ctl *ctx,
	struct sde_ctl_flush_cfg *cfg)
{
	if (!ctx || !cfg)
		return -EINVAL;

	ctx->flush.pending_flush_mask |= cfg->pending_flush_mask;
	return 0;
}

static int sde_hw_ctl_get_pending_flush(struct sde_hw_ctl *ctx,
		struct sde_ctl_flush_cfg *cfg)
{
	if (!ctx || !cfg)
		return -EINVAL;

	memcpy(cfg, &ctx->flush, sizeof(*cfg));
	return 0;
}

static inline int sde_hw_ctl_trigger_flush(struct sde_hw_ctl *ctx)
{

	if (!ctx)
		return -EINVAL;

	SDE_REG_WRITE(&ctx->hw, CTL_FLUSH, ctx->flush.pending_flush_mask);
	return 0;
}

static inline u32 sde_hw_ctl_get_flush_register(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 rot_op_mode;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	rot_op_mode = SDE_REG_READ(c, CTL_ROT_TOP) & 0x3;

	/* rotate flush bit is undefined if offline mode, so ignore it */
	if (rot_op_mode == SDE_CTL_ROT_OP_MODE_OFFLINE)
		return SDE_REG_READ(c, CTL_FLUSH) & ~CTL_FLUSH_MASK_ROT;

	return SDE_REG_READ(c, CTL_FLUSH);
}

static inline void sde_hw_ctl_uidle_enable(struct sde_hw_ctl *ctx, bool enable)
{
	u32 val;

	if (!ctx)
		return;

	val = SDE_REG_READ(&ctx->hw, CTL_UIDLE_ACTIVE);
	val = (val & ~BIT(0)) | (enable ? BIT(0) : 0);

	SDE_REG_WRITE(&ctx->hw, CTL_UIDLE_ACTIVE, val);
}


static inline int sde_hw_ctl_update_bitmask_sspp(struct sde_hw_ctl *ctx,
		enum sde_sspp sspp,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(sspp > SSPP_NONE) || !(sspp < SSPP_MAX)) {
		SDE_ERROR("Unsupported pipe %d\n", sspp);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, sspp_tbl[sspp], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_mixer(struct sde_hw_ctl *ctx,
		enum sde_lm lm,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(lm > SDE_NONE) || !(lm < LM_MAX)) {
		SDE_ERROR("Unsupported mixer %d\n", lm);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, mixer_tbl[lm], enable);
	ctx->flush.pending_flush_mask |= CTL_FLUSH_MASK_CTL;

	return 0;
}

static inline int sde_hw_ctl_update_bitmask_dspp(struct sde_hw_ctl *ctx,
		enum sde_dspp dspp,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(dspp > SDE_NONE) || !(dspp < DSPP_MAX)) {
		SDE_ERROR("Unsupported dspp %d\n", dspp);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, dspp_tbl[dspp], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_dspp_pavlut(struct sde_hw_ctl *ctx,
		enum sde_dspp dspp, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(dspp > SDE_NONE) || !(dspp < DSPP_MAX)) {
		SDE_ERROR("Unsupported dspp %d\n", dspp);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, dspp_pav_tbl[dspp], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_cdm(struct sde_hw_ctl *ctx,
		enum sde_cdm cdm,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(cdm > SDE_NONE) || !(cdm < CDM_MAX) || (cdm == CDM_1)) {
		SDE_ERROR("Unsupported cdm %d\n", cdm);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, cdm_tbl[cdm], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_wb(struct sde_hw_ctl *ctx,
		enum sde_wb wb, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(wb > SDE_NONE) || !(wb < WB_MAX) ||
			(wb == WB_0) || (wb == WB_1)) {
		SDE_ERROR("Unsupported wb %d\n", wb);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, wb_tbl[wb], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_intf(struct sde_hw_ctl *ctx,
		enum sde_intf intf, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(intf > SDE_NONE) || !(intf < INTF_MAX) || (intf > INTF_4)) {
		SDE_ERROR("Unsupported intf %d\n", intf);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, intf_tbl[intf], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask(struct sde_hw_ctl *ctx,
		enum ctl_hw_flush_type type, u32 blk_idx, bool enable)
{
	int ret = 0;

	if (!ctx)
		return -EINVAL;

	switch (type) {
	case SDE_HW_FLUSH_CDM:
		ret = sde_hw_ctl_update_bitmask_cdm(ctx, blk_idx, enable);
		break;
	case SDE_HW_FLUSH_WB:
		ret = sde_hw_ctl_update_bitmask_wb(ctx, blk_idx, enable);
		break;
	case SDE_HW_FLUSH_INTF:
		ret = sde_hw_ctl_update_bitmask_intf(ctx, blk_idx, enable);
		break;
	default:
		break;
	}

	return ret;
}

static inline int sde_hw_ctl_update_bitmask_v1(struct sde_hw_ctl *ctx,
		enum ctl_hw_flush_type type, u32 blk_idx, bool enable)
{
	const struct ctl_hw_flush_cfg *cfg;

	if (!ctx || !(type < SDE_HW_FLUSH_MAX))
		return -EINVAL;

	cfg = &ctl_hw_flush_cfg_tbl_v1[type];

	if ((blk_idx <= SDE_NONE) || (blk_idx >= cfg->blk_max)) {
		SDE_ERROR("Unsupported hw idx, type:%d, blk_idx:%d, blk_max:%d",
				type, blk_idx, cfg->blk_max);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_hw_flush_mask[cfg->flush_mask_idx],
			cfg->flush_tbl[blk_idx], enable);
	if (ctx->flush.pending_hw_flush_mask[cfg->flush_mask_idx])
		UPDATE_MASK(ctx->flush.pending_flush_mask, cfg->flush_idx, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, cfg->flush_idx, 0);

	return 0;
}

static inline int sde_hw_ctl_update_pending_flush_v1(
		struct sde_hw_ctl *ctx,
		struct sde_ctl_flush_cfg *cfg)
{
	int i = 0;

	if (!ctx || !cfg)
		return -EINVAL;

	for (i = 0; i < SDE_HW_FLUSH_MAX; i++)
		ctx->flush.pending_hw_flush_mask[i] |=
				cfg->pending_hw_flush_mask[i];

	for (i = 0; i < CTL_MAX_DSPP_COUNT; i++)
		ctx->flush.pending_dspp_flush_masks[i] |=
				cfg->pending_dspp_flush_masks[i];

	ctx->flush.pending_flush_mask |= cfg->pending_flush_mask;

	return 0;
}

static inline int sde_hw_ctl_update_bitmask_dspp_subblk(struct sde_hw_ctl *ctx,
		enum sde_dspp dspp, u32 sub_blk, bool enable)
{
	if (!ctx || dspp < DSPP_0 || dspp >= DSPP_MAX ||
			sub_blk < SDE_DSPP_IGC || sub_blk >= SDE_DSPP_MAX) {
		SDE_ERROR("invalid args - ctx %s, dspp %d sub_block %d\n",
				ctx ? "valid" : "invalid", dspp, sub_blk);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_dspp_flush_masks[dspp - DSPP_0],
			dspp_sub_blk_flush_tbl[sub_blk], enable);
	if (_is_dspp_flush_pending(ctx))
		UPDATE_MASK(ctx->flush.pending_flush_mask, DSPP_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, DSPP_IDX, 0);

	return 0;
}

static void sde_hw_ctl_set_fetch_pipe_active(struct sde_hw_ctl *ctx,
		unsigned long *fetch_active)
{
	int i;
	u32 val = 0;

	if (fetch_active) {
		for (i = 0; i < SSPP_MAX; i++) {
			if (test_bit(i, fetch_active) &&
					fetch_tbl[i] != CTL_INVALID_BIT)
				val |= BIT(fetch_tbl[i]);
		}
	}

	SDE_REG_WRITE(&ctx->hw, CTL_FETCH_PIPE_ACTIVE, val);
}

static u32 sde_hw_ctl_get_active_fetch_pipes(struct sde_hw_ctl *ctx)
{
	int i;
	u32 fetch_info, fetch_active = 0;

	if (!ctx)  {
		DRM_ERROR("invalid args - ctx invalid\n");
		return 0;
	}

	fetch_info = SDE_REG_READ(&ctx->hw, CTL_FETCH_PIPE_ACTIVE);

	for (i = SSPP_VIG0; i < SSPP_MAX; i++) {
		if (fetch_tbl[i] != CTL_INVALID_BIT &&
				fetch_info & BIT(fetch_tbl[i])) {
			fetch_active |= BIT(i);
		}
	}

	return fetch_active;
}

static inline void _sde_hw_ctl_write_dspp_flushes(struct sde_hw_ctl *ctx) {
	int i;
	bool has_dspp_flushes = ctx->caps->features &
			BIT(SDE_CTL_UNIFIED_DSPP_FLUSH);

	if (!has_dspp_flushes)
		return;

	for (i = 0; i < CTL_MAX_DSPP_COUNT; i++) {
		u32 pending = ctx->flush.pending_dspp_flush_masks[i];

		if (pending)
			SDE_REG_WRITE(&ctx->hw, CTL_DSPP_0_FLUSH + (i * 4),
					pending);
	}
}

static inline int sde_hw_ctl_trigger_flush_v1(struct sde_hw_ctl *ctx)
{
	int i = 0;

	const struct ctl_hw_flush_cfg *cfg = &ctl_hw_flush_cfg_tbl_v1[0];

	if (!ctx)
		return -EINVAL;

	if (ctx->flush.pending_flush_mask & BIT(DSPP_IDX))
		_sde_hw_ctl_write_dspp_flushes(ctx);

	for (i = 0; i < SDE_HW_FLUSH_MAX; i++)
		if (cfg[i].flush_reg &&
				ctx->flush.pending_flush_mask &
				BIT(cfg[i].flush_idx))
			SDE_REG_WRITE(&ctx->hw,
					cfg[i].flush_reg,
					ctx->flush.pending_hw_flush_mask[i]);

	SDE_REG_WRITE(&ctx->hw, CTL_FLUSH, ctx->flush.pending_flush_mask);

	return 0;
}

static inline u32 sde_hw_ctl_get_intf_v1(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_active;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}

	c = &ctx->hw;
	intf_active = SDE_REG_READ(c, CTL_INTF_ACTIVE);

	return intf_active;
}

static inline u32 sde_hw_ctl_get_intf(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 ctl_top;
	u32 intf_active = 0;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}

	c = &ctx->hw;
	ctl_top = SDE_REG_READ(c, CTL_TOP);

	intf_active = (ctl_top > 0) ?
		BIT(ctl_top - 1) : 0;

	return intf_active;
}

static u32 sde_hw_ctl_poll_reset_status(struct sde_hw_ctl *ctx, u32 timeout_us)
{
	struct sde_hw_blk_reg_map *c;
	ktime_t timeout;
	u32 status;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	timeout = ktime_add_us(ktime_get(), timeout_us);

	/*
	 * it takes around 30us to have mdp finish resetting its ctl path
	 * poll every 50us so that reset should be completed at 1st poll
	 */
	do {
		status = SDE_REG_READ(c, CTL_SW_RESET);
		status &= 0x1;
		if (status)
			usleep_range(20, 50);
	} while (status && ktime_compare_safe(ktime_get(), timeout) < 0);

	return status;
}

static u32 sde_hw_ctl_get_reset_status(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return 0;
	return (u32)SDE_REG_READ(&ctx->hw, CTL_SW_RESET);
}

static u32 sde_hw_ctl_get_scheduler_status(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return INVALID_CTL_STATUS;
	return (u32)SDE_REG_READ(&ctx->hw, CTL_STATUS);
}

static int sde_hw_ctl_reset_control(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	pr_debug("issuing hw ctl reset for ctl:%d\n", ctx->idx);
	SDE_REG_WRITE(c, CTL_SW_RESET, 0x1);
	if (sde_hw_ctl_poll_reset_status(ctx, SDE_REG_RESET_TIMEOUT_US))
		return -EINVAL;

	return 0;
}

static void sde_hw_ctl_hard_reset(struct sde_hw_ctl *ctx, bool enable)
{
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return;

	c = &ctx->hw;
	pr_debug("hw ctl hard reset for ctl:%d, %d\n",
			ctx->idx - CTL_0, enable);
	SDE_REG_WRITE(c, CTL_SW_RESET_OVERRIDE, enable);
}

static int sde_hw_ctl_wait_reset_status(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 status;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	status = SDE_REG_READ(c, CTL_SW_RESET);
	status &= 0x01;
	if (!status)
		return 0;

	pr_debug("hw ctl reset is set for ctl:%d\n", ctx->idx);
	if (sde_hw_ctl_poll_reset_status(ctx, SDE_REG_WAIT_RESET_TIMEOUT_US)) {
		pr_err("hw recovery is not complete for ctl:%d\n", ctx->idx);
		return -EINVAL;
	}

	return 0;
}

static void sde_hw_ctl_clear_all_blendstages(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	int i;

	if (!ctx)
		return;

	c = &ctx->hw;
	for (i = 0; i < ctx->mixer_count; i++) {
		int mixer_id = ctx->mixer_hw_caps[i].id;

		SDE_REG_WRITE(c, CTL_LAYER(mixer_id), 0);
		SDE_REG_WRITE(c, CTL_LAYER_EXT(mixer_id), 0);
		SDE_REG_WRITE(c, CTL_LAYER_EXT2(mixer_id), 0);
		SDE_REG_WRITE(c, CTL_LAYER_EXT3(mixer_id), 0);
		if (ctx->caps->features & BIT(SDE_CTL_DMA4_DMA5))
			SDE_REG_WRITE(c, CTL_LAYER_EXT4(mixer_id), 0);
	}
	SDE_REG_WRITE(c, CTL_FETCH_PIPE_ACTIVE, 0);
}

static void _sde_hw_ctl_get_mixer_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_stage_cfg *stage_cfg, int stages, u32 *cfg,
		int lm_layout)
{
	int i, j, pipes_per_stage;
	const struct ctl_sspp_stage_reg_map *reg_map;

	if (test_bit(SDE_MIXER_SOURCESPLIT, &ctx->mixer_hw_caps->features))
		pipes_per_stage = PIPES_PER_STAGE;
	else
		pipes_per_stage = 1;

	for (i = 0; i <= stages; i++) {
		/* overflow to ext register if 'i + 1 > 7' */
		for (j = 0 ; j < pipes_per_stage; j++) {
			enum sde_sspp pipe = stage_cfg->stage[i][j];
			enum sde_sspp_multirect_index rect_index =
				stage_cfg->multirect_index[i][j];
			enum sde_layout sspp_layout =
				stage_cfg->sspp_layout[i][j];
			u32 mixer_value;

			if (lm_layout >= 0 && sspp_layout && (sspp_layout != lm_layout))
				continue;

			if (!pipe || pipe >= SSPP_MAX || rect_index >= SDE_SSPP_RECT_MAX)
				continue;
			/* Handle multi rect enums */
			if (rect_index == SDE_SSPP_RECT_SOLO)
				rect_index = SDE_SSPP_RECT_0;

			reg_map = &sspp_reg_cfg_tbl[pipe][rect_index-1];
			if (!reg_map->bits)
				continue;

			mixer_value = (i + 1) & (BIT(reg_map->bits) - 1);
			cfg[reg_map->ext] |= (mixer_value << reg_map->start);
			if ((i + 1) > mixer_value)
				cfg[1] |= reg_map->sec_bit_mask;
		}
	}
}

static void sde_hw_ctl_setup_blendstage(struct sde_hw_ctl *ctx,
	enum sde_lm lm, int lm_layout, struct sde_hw_stage_cfg *stage_cfg,
	bool disable_border)
{
	struct sde_hw_blk_reg_map *c;
	u32 cfg[CTL_NUM_EXT] = { 0 };
	int stages;
	bool null_commit;

	if (!ctx)
		return;

	stages = _mixer_stages(ctx->mixer_hw_caps, ctx->mixer_count, lm);
	if (stages < 0)
		return;

	c = &ctx->hw;

	if (stage_cfg)
		_sde_hw_ctl_get_mixer_cfg(ctx, stage_cfg, stages, cfg, lm_layout);

	null_commit = (!cfg[0] && !cfg[1] && !cfg[2] && !cfg[3] && !cfg[4]);
	if (!disable_border && (null_commit || (stage_cfg && !stage_cfg->stage[0][0])))
		cfg[0] |= CTL_MIXER_BORDER_OUT;

	SDE_REG_WRITE(c, CTL_LAYER(lm), cfg[0]);
	SDE_REG_WRITE(c, CTL_LAYER_EXT(lm), cfg[1]);
	SDE_REG_WRITE(c, CTL_LAYER_EXT2(lm), cfg[2]);
	SDE_REG_WRITE(c, CTL_LAYER_EXT3(lm), cfg[3]);
	if (ctx->caps->features & BIT(SDE_CTL_DMA4_DMA5))
		SDE_REG_WRITE(c, CTL_LAYER_EXT4(lm), cfg[4]);
}

static u32 sde_hw_ctl_get_staged_sspp(struct sde_hw_ctl *ctx, enum sde_lm lm,
		struct sde_sspp_index_info *info)
{
	int i, j;
	u32 count = 0;
	u32 mask = 0;
	bool staged;
	u32 mixercfg[CTL_NUM_EXT];
	struct sde_hw_blk_reg_map *c;
	const struct ctl_sspp_stage_reg_map *sspp_cfg;

	if (!ctx || (lm >= LM_DCWB_DUMMY_0) || !info)
		return 0;

	c = &ctx->hw;
	mixercfg[0] = SDE_REG_READ(c, CTL_LAYER(lm));
	mixercfg[1] = SDE_REG_READ(c, CTL_LAYER_EXT(lm));
	mixercfg[2] = SDE_REG_READ(c, CTL_LAYER_EXT2(lm));
	mixercfg[3] = SDE_REG_READ(c, CTL_LAYER_EXT3(lm));
	if (ctx->caps->features & BIT(SDE_CTL_DMA4_DMA5))
		mixercfg[4] = SDE_REG_READ(c, CTL_LAYER_EXT4(lm));

	if (mixercfg[0] & CTL_MIXER_BORDER_OUT)
		info->bordercolor = true;

	for (i = SSPP_VIG0; i < SSPP_MAX; i++) {
		for (j = 0; j < CTL_SSPP_MAX_RECTS; j++) {

			sspp_cfg = &sspp_reg_cfg_tbl[i][j];
			if (!sspp_cfg->bits || sspp_cfg->ext >= CTL_NUM_EXT)
				continue;

			mask = ((0x1 << sspp_cfg->bits) - 1) << sspp_cfg->start;
			staged = mixercfg[sspp_cfg->ext] & mask;
			if (!staged)
				staged = mixercfg[1] & sspp_cfg->sec_bit_mask;

			if (staged) {
				if (j)
					set_bit(i, info->virt_pipes);
				else
					set_bit(i, info->pipes);
				count++;
			}
		}
	}

	return count;
}

static int sde_hw_ctl_intf_cfg_v1(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_active = 0;
	u32 wb_active = 0;
	u32 merge_3d_active = 0;
	u32 cwb_active = 0;
	u32 mode_sel = 0xf0000000;
	u32 cdm_active = 0;
	u32 intf_master = 0;
	u32 i;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	for (i = 0; i < cfg->intf_count; i++) {
		if (cfg->intf[i])
			intf_active |= BIT(cfg->intf[i] - INTF_0);
	}

	if (cfg->intf_count > 1)
		intf_master = BIT(cfg->intf_master - INTF_0);

	for (i = 0; i < cfg->wb_count; i++) {
		if (cfg->wb[i])
			wb_active |= BIT(cfg->wb[i] - WB_0);
	}

	for (i = 0; i < cfg->merge_3d_count; i++) {
		if (cfg->merge_3d[i])
			merge_3d_active |= BIT(cfg->merge_3d[i] - MERGE_3D_0);
	}

	for (i = 0; i < cfg->cwb_count; i++) {
		if (cfg->cwb[i])
			cwb_active |= BIT(cfg->cwb[i] - CWB_0);
	}

	for (i = 0; i < cfg->cdm_count; i++) {
		if (cfg->cdm[i])
			cdm_active |= BIT(cfg->cdm[i] - CDM_0);
	}

	if (cfg->intf_mode_sel == SDE_CTL_MODE_SEL_CMD)
		mode_sel |= BIT(17);

	SDE_REG_WRITE(c, CTL_TOP, mode_sel);
	SDE_REG_WRITE(c, CTL_WB_ACTIVE, wb_active);
	SDE_REG_WRITE(c, CTL_CWB_ACTIVE, cwb_active);
	SDE_REG_WRITE(c, CTL_INTF_ACTIVE, intf_active);
	SDE_REG_WRITE(c, CTL_CDM_ACTIVE, cdm_active);
	SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, merge_3d_active);
	SDE_REG_WRITE(c, CTL_INTF_MASTER, intf_master);
	return 0;
}

static int sde_hw_ctl_reset_post_disable(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg, u32 merge_3d_idx)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_active = 0, wb_active = 0, merge_3d_active = 0;
	u32 intf_flush = 0, wb_flush = 0;
	u32 i;

	if (!ctx || !cfg) {
		SDE_ERROR("invalid hw_ctl or hw_intf blk\n");
		return -EINVAL;
	}

	c = &ctx->hw;
	for (i = 0; i < cfg->intf_count; i++) {
		if (cfg->intf[i]) {
			intf_active &= ~BIT(cfg->intf[i] - INTF_0);
			intf_flush |= BIT(cfg->intf[i] - INTF_0);
		}
	}

	for (i = 0; i < cfg->wb_count; i++) {
		if (cfg->wb[i]) {
			wb_active &= ~BIT(cfg->wb[i] - WB_0);
			wb_flush |= BIT(cfg->wb[i] - WB_0);
		}
	}

	if (merge_3d_idx) {
		/* disable and flush merge3d_blk */
		merge_3d_active &= ~BIT(merge_3d_idx - MERGE_3D_0);
		ctx->flush.pending_hw_flush_mask[SDE_HW_FLUSH_MERGE_3D] =
				BIT(merge_3d_idx - MERGE_3D_0);
		UPDATE_MASK(ctx->flush.pending_flush_mask, MERGE_3D_IDX, 1);
		SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, merge_3d_active);
	}

	sde_hw_ctl_clear_all_blendstages(ctx);

	if (cfg->intf_count) {
		ctx->flush.pending_hw_flush_mask[SDE_HW_FLUSH_INTF] =
				intf_flush;
		UPDATE_MASK(ctx->flush.pending_flush_mask, INTF_IDX, 1);
		SDE_REG_WRITE(c, CTL_INTF_ACTIVE, intf_active);
	}

	if (cfg->wb_count) {
		ctx->flush.pending_hw_flush_mask[SDE_HW_FLUSH_WB] = wb_flush;
		UPDATE_MASK(ctx->flush.pending_flush_mask, WB_IDX, 1);
		SDE_REG_WRITE(c, CTL_WB_ACTIVE, wb_active);
	}

	return 0;
}

static int sde_hw_ctl_update_intf_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg, bool enable)
{
	int i;
	u32 cwb_active = 0;
	u32 merge_3d_active = 0;
	u32 wb_active = 0;
	u32 dsc_active = 0;
	u32 vdc_active = 0;
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;

	if (cfg->cwb_count) {
		cwb_active = SDE_REG_READ(c, CTL_CWB_ACTIVE);
		for (i = 0; i < cfg->cwb_count; i++) {
			if (cfg->cwb[i])
				UPDATE_ACTIVE(cwb_active,
					(cfg->cwb[i] - CWB_0),
					enable);
		}

		wb_active = enable ? BIT(2) : 0;
		SDE_REG_WRITE(c, CTL_CWB_ACTIVE, cwb_active);
		SDE_REG_WRITE(c, CTL_WB_ACTIVE, wb_active);
	}

	if (cfg->merge_3d_count) {
		merge_3d_active = SDE_REG_READ(c, CTL_MERGE_3D_ACTIVE);
		for (i = 0; i < cfg->merge_3d_count; i++) {
			if (cfg->merge_3d[i])
				UPDATE_ACTIVE(merge_3d_active,
					(cfg->merge_3d[i] - MERGE_3D_0),
					enable);
		}

		SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, merge_3d_active);
	}

	if (cfg->dsc_count) {
		dsc_active = SDE_REG_READ(c, CTL_DSC_ACTIVE);
		for (i = 0; i < cfg->dsc_count; i++) {
			if (cfg->dsc[i])
				UPDATE_ACTIVE(dsc_active,
					(cfg->dsc[i] - DSC_0), enable);
		}

		SDE_REG_WRITE(c, CTL_DSC_ACTIVE, dsc_active);
	}

	if (cfg->vdc_count) {
		vdc_active = SDE_REG_READ(c, CTL_DSC_ACTIVE);
		for (i = 0; i < cfg->vdc_count; i++) {
			if (cfg->vdc[i])
				UPDATE_ACTIVE(vdc_active,
					VDC_IDX(cfg->vdc[i] - VDC_0), enable);
		}

		SDE_REG_WRITE(c, CTL_DSC_ACTIVE, vdc_active);
	}
	return 0;
}

static int sde_hw_ctl_intf_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_cfg = 0;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	intf_cfg |= (cfg->intf & 0xF) << 4;

	if (cfg->wb)
		intf_cfg |= (cfg->wb & 0x3) + 2;

	if (cfg->mode_3d) {
		intf_cfg |= BIT(19);
		intf_cfg |= (cfg->mode_3d - 0x1) << 20;
	}

	switch (cfg->intf_mode_sel) {
	case SDE_CTL_MODE_SEL_VID:
		intf_cfg &= ~BIT(17);
		intf_cfg &= ~(0x3 << 15);
		break;
	case SDE_CTL_MODE_SEL_CMD:
		intf_cfg |= BIT(17);
		intf_cfg |= ((cfg->stream_sel & 0x3) << 15);
		break;
	default:
		pr_err("unknown interface type %d\n", cfg->intf_mode_sel);
		return -EINVAL;
	}

	SDE_REG_WRITE(c, CTL_TOP, intf_cfg);
	return 0;
}

static void sde_hw_ctl_update_wb_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg, bool enable)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 intf_cfg = 0;

	if (!cfg->wb)
		return;

	intf_cfg = SDE_REG_READ(c, CTL_TOP);
	if (enable)
		intf_cfg |= (cfg->wb & 0x3) + 2;
	else
		intf_cfg &= ~((cfg->wb & 0x3) + 2);

	SDE_REG_WRITE(c, CTL_TOP, intf_cfg);
}

static inline u32 sde_hw_ctl_read_ctl_layers(struct sde_hw_ctl *ctx, int index)
{
	struct sde_hw_blk_reg_map *c;
	u32 ctl_top;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}
	c = &ctx->hw;
	ctl_top = SDE_REG_READ(c, CTL_LAYER(index));
	pr_debug("Ctl_layer value = 0x%x\n", ctl_top);
	return ctl_top;
}

static inline bool sde_hw_ctl_read_active_status(struct sde_hw_ctl *ctx,
		enum sde_hw_blk_type blk, int index)
{
	struct sde_hw_blk_reg_map *c;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}

	c = &ctx->hw;

	switch (blk) {
	case SDE_HW_BLK_MERGE_3D:
		return (SDE_REG_READ(c, CTL_MERGE_3D_ACTIVE) &
			BIT(index - MERGE_3D_0)) ? true : false;
	case SDE_HW_BLK_DSC:
		return (SDE_REG_READ(c, CTL_DSC_ACTIVE) &
			BIT(index - DSC_0)) ? true : false;
	case SDE_HW_BLK_WB:
		return (SDE_REG_READ(c, CTL_WB_ACTIVE) &
			BIT(index - WB_0)) ? true : false;
	case SDE_HW_BLK_CDM:
		return (SDE_REG_READ(c, CTL_CDM_ACTIVE) &
			BIT(index - CDM_0)) ? true : false;
	case SDE_HW_BLK_INTF:
		return (SDE_REG_READ(c, CTL_INTF_ACTIVE) &
			BIT(index - INTF_0)) ? true : false;
	default:
		pr_err("unsupported blk %d\n", blk);
		return false;
	};

	return false;
}

static int sde_hw_reg_dma_flush(struct sde_hw_ctl *ctx, bool blocking)
{
	struct sde_hw_reg_dma_ops *ops;

	if (!ctx)
		return -EINVAL;

	ops = sde_reg_dma_get_ops(ctx->dpu_idx);

	if (!ops) {
		SDE_ERROR("dma ops is NULL\n");
		return -EINVAL;
	}

	if (ops && ops->last_command)
		return ops->last_command(ctx, DMA_CTL_QUEUE0,
		    (blocking ? REG_DMA_WAIT4_COMP : REG_DMA_NOWAIT));

	return 0;

}

static inline int sde_hw_ctl_get_hw_fence_status(struct sde_hw_ctl *ctx)
{
	return SDE_REG_READ(&ctx->hw, CTL_HW_FENCE_STATUS);
}

static inline int sde_hw_ctl_get_hw_fence_ctrl(struct sde_hw_ctl *ctx)
{
	return SDE_REG_READ(&ctx->hw, CTL_HW_FENCE_CTRL);
}

static inline void sde_hw_ctl_hw_fence_ctrl(struct sde_hw_ctl *ctx, bool sw_override_set,
		bool  sw_override_clear, u32 mode)
{
	u32 val;

	val = SDE_REG_READ(&ctx->hw, CTL_HW_FENCE_CTRL);
	val |= (0x1 & mode) | (sw_override_set ? BIT(5) : 0) | (sw_override_clear ? BIT(4) : 0);
	SDE_REG_WRITE(&ctx->hw, CTL_HW_FENCE_CTRL, val);
}

static inline void sde_hw_ctl_update_input_fence(struct sde_hw_ctl *ctx,
		u32 source_id, u32 signal_id)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;

	c = &ctx->hw;
	val = (source_id << 16) | (signal_id & 0xFF);
	SDE_REG_WRITE(c, CTL_INPUT_FENCE_ID, val);
}

static inline void sde_hw_ctl_trigger_sw_override(struct sde_hw_ctl *ctx)
{
	SDE_REG_WRITE(&ctx->hw, CTL_FENCE_READY_SW_OVERRIDE, 0x1);
}

static void _setup_ctl_ops(struct sde_hw_ctl_ops *ops,
		unsigned long cap)
{
	if (cap & BIT(SDE_CTL_ACTIVE_CFG)) {
		ops->update_pending_flush =
			sde_hw_ctl_update_pending_flush_v1;
		ops->trigger_flush = sde_hw_ctl_trigger_flush_v1;

		ops->setup_intf_cfg_v1 = sde_hw_ctl_intf_cfg_v1;
		ops->update_intf_cfg = sde_hw_ctl_update_intf_cfg;

		ops->update_bitmask = sde_hw_ctl_update_bitmask_v1;
		ops->get_ctl_intf = sde_hw_ctl_get_intf_v1;

		ops->reset_post_disable = sde_hw_ctl_reset_post_disable;
		ops->get_scheduler_status = sde_hw_ctl_get_scheduler_status;
		ops->read_active_status = sde_hw_ctl_read_active_status;
		ops->set_active_pipes = sde_hw_ctl_set_fetch_pipe_active;
		ops->get_active_pipes = sde_hw_ctl_get_active_fetch_pipes;
	} else {
		ops->update_pending_flush = sde_hw_ctl_update_pending_flush;
		ops->trigger_flush = sde_hw_ctl_trigger_flush;

		ops->setup_intf_cfg = sde_hw_ctl_intf_cfg;

		ops->update_bitmask = sde_hw_ctl_update_bitmask;
		ops->get_ctl_intf = sde_hw_ctl_get_intf;
	}
	ops->clear_pending_flush = sde_hw_ctl_clear_pending_flush;
	ops->get_pending_flush = sde_hw_ctl_get_pending_flush;
	ops->get_flush_register = sde_hw_ctl_get_flush_register;
	ops->trigger_start = sde_hw_ctl_trigger_start;
	ops->trigger_pending = sde_hw_ctl_trigger_pending;
	ops->read_ctl_layers = sde_hw_ctl_read_ctl_layers;
	ops->update_wb_cfg = sde_hw_ctl_update_wb_cfg;
	ops->reset = sde_hw_ctl_reset_control;
	ops->get_reset = sde_hw_ctl_get_reset_status;
	ops->hard_reset = sde_hw_ctl_hard_reset;
	ops->wait_reset_status = sde_hw_ctl_wait_reset_status;
	ops->clear_all_blendstages = sde_hw_ctl_clear_all_blendstages;
	ops->setup_blendstage = sde_hw_ctl_setup_blendstage;
	ops->get_staged_sspp = sde_hw_ctl_get_staged_sspp;
	ops->update_bitmask_sspp = sde_hw_ctl_update_bitmask_sspp;
	ops->update_bitmask_mixer = sde_hw_ctl_update_bitmask_mixer;
	ops->reg_dma_flush = sde_hw_reg_dma_flush;
	ops->get_start_state = sde_hw_ctl_get_start_state;

	if (cap & BIT(SDE_CTL_HW_FENCE)) {
		ops->hw_fence_ctrl = sde_hw_ctl_hw_fence_ctrl;
		ops->get_hw_fence_ctrl = sde_hw_ctl_get_hw_fence_ctrl;
		ops->setup_hw_input_fence = sde_hw_ctl_update_input_fence;
		ops->hw_fence_trigger_sw_override = sde_hw_ctl_trigger_sw_override;
		ops->get_hw_fence_status = sde_hw_ctl_get_hw_fence_status;
	}

	if (cap & BIT(SDE_CTL_UNIFIED_DSPP_FLUSH)) {
		ops->update_bitmask_dspp_subblk =
				sde_hw_ctl_update_bitmask_dspp_subblk;
	} else {
		ops->update_bitmask_dspp = sde_hw_ctl_update_bitmask_dspp;
		ops->update_bitmask_dspp_pavlut =
				sde_hw_ctl_update_bitmask_dspp_pavlut;
	}

	if (cap & BIT(SDE_CTL_UIDLE))
		ops->uidle_enable = sde_hw_ctl_uidle_enable;
};

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_ctl *sde_hw_ctl_init(enum sde_ctl idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m,
		u32 dpu_idx)
{
	struct sde_hw_ctl *c;
	struct sde_ctl_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _ctl_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		pr_err("failed to create sde_hw_ctl %d\n", idx);
		return ERR_PTR(-EINVAL);
	}

	c->caps = cfg;
	_setup_ctl_ops(&c->ops, c->caps->features);
	c->idx = idx;
	c->mixer_count = m->mixer_count;
	c->mixer_hw_caps = m->mixer;
	c->dpu_idx = dpu_idx;

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_CTL, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(m->dev, SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;

blk_init_error:
	kfree(c);

	return ERR_PTR(rc);
}

void sde_hw_ctl_destroy(struct sde_hw_ctl *ctx)
{
	if (ctx)
		sde_hw_blk_destroy(&ctx->base);
	kfree(ctx);
}

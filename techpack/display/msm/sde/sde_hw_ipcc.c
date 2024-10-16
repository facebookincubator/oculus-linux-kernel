// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_top.h"
#include "sde_dbg.h"
#include "sde_kms.h"
#include "sde_hw_ipcc.h"

static inline void sde_hw_ipcc_enable_signaling(struct sde_hw_ipcc *ipcc,
	u32 client_id, u32 protocol_id, u32 source_id, u32 signal_id)
{
	struct sde_hw_blk_reg_map *c = &ipcc->hw;
	u32 reg_off;

	reg_off = HW_FENCE_IPC_PROTOCOLp_CLIENTc_CONFIG(protocol_id, client_id);
	SDE_REG_WRITE(c, reg_off, 0x1);

	reg_off = HW_FENCE_IPCC_PROTOCOLp_CLIENTc_RECV_SIGNAL_ENABLE(protocol_id, client_id);
	SDE_REG_WRITE(c, reg_off, ((source_id & 0xFF) << 16) | signal_id);
}

static inline void sde_hw_ipcc_trigger_signal(struct sde_hw_ipcc *ipcc,
	u32 dest_id, u32 protocol_id, u32 source_id, u32 signal_id)
{
	struct sde_hw_blk_reg_map *c = &ipcc->hw;
	u32 reg_off, val;

	reg_off = HW_FENCE_IPCC_PROTOCOLp_CLIENTc_SEND(protocol_id, source_id);
	val = (signal_id) | ((dest_id &  0xFF) << 16);
	SDE_REG_WRITE(c, reg_off, val);
}

struct sde_hw_ipcc *sde_hw_ipcc_init(void __iomem *addr,
	u32 ipcc_len, const struct sde_mdss_cfg *m)
{
	struct sde_hw_ipcc *c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	c->hw.base_off = addr;
	c->hw.blk_off = 0;
	c->hw.length = ipcc_len;
	c->hw.hwversion = m->hwversion;
	c->hw.log_mask = SDE_DBG_MASK_IPCC;

	if (test_bit(SDE_MDP_HAS_HW_FENCE_SUPPORT, &m->mdp[0].features)) {
		c->ops.hw_ipcc_enable_signaling = sde_hw_ipcc_enable_signaling;
		c->ops.hw_ipcc_trigger_signal = sde_hw_ipcc_trigger_signal;
	}

	return c;
}

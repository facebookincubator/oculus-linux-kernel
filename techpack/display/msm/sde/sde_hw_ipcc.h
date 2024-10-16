/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SDE_HW_IPCC_H
#define _SDE_HW_IPCC_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"

#define HW_FENCE_IPCC_PROTOCOLp_CLIENTc(ba, p, c)   (ba + (0x40000*p) + (0x1000*c))
#define HW_FENCE_IPC_PROTOCOLp_CLIENTc_CONFIG(p, c) (0x8 + (0x40000*p) + (0x1000*c))
#define HW_FENCE_IPCC_PROTOCOLp_CLIENTc_SEND(p, c) (0xc + (0x40000*p) + (0x1000*c))
#define HW_FENCE_IPCC_PROTOCOLp_CLIENTc_RECV_ID(ba, p, c) ((ba + 0x10) + (0x40000*p) + (0x1000*c))
#define HW_FENCE_IPCC_PROTOCOLp_CLIENTc_RECV_SIGNAL_ENABLE(p, c) (0x14 + (0x40000*p) + (0x1000*c))
#define HW_FENCE_DPU_INPUT_FENCE_START_N		0
#define HW_FENCE_DPU_OUTPUT_FENCE_START_N		4

#define HW_FENCE_IPC_CLIENT_ID_APPS 8

struct sde_hw_ipcc;

struct sde_hw_ipcc_ops {
	/**
	 * hw_ipcc_enable_signaling - enable signaling for given client id from source id
	 * @ipcc: ipcc ctx pointer
	 * @client_id: client id for which signalling is to be enabled.
	 * @protocol_id: protocol id
	 * @source_id: source id of the ipcc client from which signal to be received
	 * @signal_id: signal id
	 */
	void (*hw_ipcc_enable_signaling)(struct sde_hw_ipcc *ipcc,
		u32 client_id, u32 protocol_id, u32 source_id, u32 signal_id);
	/**
	 * hw_ipcc_trigger_signal - send ipcc signal to dest client from source
	 * @ipcc: ipcc ctx pointer
	 * @dest_id: destination ipc client id
	 * @protocol_id: ipc protocol id
	 * @source_id: source id of the ipc client
	 * @signal_id: signal id
	 */
	void (*hw_ipcc_trigger_signal)(struct sde_hw_ipcc *ipcc, u32 dest_id,
		u32 protocol_id, u32 source_id, u32 signal_id);
};

struct sde_hw_ipcc {
	struct sde_hw_blk_reg_map hw;
	struct sde_hw_ipcc_ops ops;
};

#if IS_ENABLED(CONFIG_DRM_SDE_IPCC)
/**
 * sde_hw_ipcc_init - initialize the ipcc blk reg map
 * @addr: Mapped register io address
 * @ipcc_len: Length of block
 * @m: Pointer to mdss catalog data
 */
struct sde_hw_ipcc *sde_hw_ipcc_init(void __iomem *addr,
		u32 ipcc_len, const struct sde_mdss_cfg *m);
#else
static inline struct sde_hw_ipcc *sde_hw_ipcc_init(void __iomem *addr,
		u32 ipcc_len, const struct sde_mdss_cfg *m)
{
	return NULL;
}
#endif
#endif /* __SDE_HW_IPCC_H_ */

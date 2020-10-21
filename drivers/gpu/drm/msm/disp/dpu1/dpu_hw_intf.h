/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DPU_HW_INTF_H
#define _DPU_HW_INTF_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"
#include "dpu_hw_blk.h"

struct dpu_hw_intf;

/* intf timing settings */
struct intf_timing_params {
	u32 width;		/* active width */
	u32 height;		/* active height */
	u32 xres;		/* Display panel width */
	u32 yres;		/* Display panel height */

	u32 h_back_porch;
	u32 h_front_porch;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 hsync_pulse_width;
	u32 vsync_pulse_width;
	u32 hsync_polarity;
	u32 vsync_polarity;
	u32 border_clr;
	u32 underflow_clr;
	u32 hsync_skew;
};

struct intf_prog_fetch {
	u8 enable;
	/* vsync counter for the front porch pixel line */
	u32 fetch_start;
};

struct intf_status {
	u8 is_en;		/* interface timing engine is enabled or not */
	u32 frame_count;	/* frame count since timing engine enabled */
	u32 line_count;		/* current line count including blanking */
};

/**
 * struct dpu_hw_intf_ops : Interface to the interface Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 * @ setup_timing_gen : programs the timing engine
 * @ setup_prog_fetch : enables/disables the programmable fetch logic
 * @ enable_timing: enable/disable timing engine
 * @ get_status: returns if timing engine is enabled or not
 * @ setup_misr: enables/disables MISR in HW register
 * @ collect_misr: reads and stores MISR data from HW register
 * @ get_line_count: reads current vertical line counter
 */
struct dpu_hw_intf_ops {
	void (*setup_timing_gen)(struct dpu_hw_intf *intf,
			const struct intf_timing_params *p,
			const struct dpu_format *fmt);

	void (*setup_prg_fetch)(struct dpu_hw_intf *intf,
			const struct intf_prog_fetch *fetch);

	void (*enable_timing)(struct dpu_hw_intf *intf,
			u8 enable);

	void (*get_status)(struct dpu_hw_intf *intf,
			struct intf_status *status);

	void (*setup_misr)(struct dpu_hw_intf *intf,
			bool enable, u32 frame_count);

	u32 (*collect_misr)(struct dpu_hw_intf *intf);

	u32 (*get_line_count)(struct dpu_hw_intf *intf);
};

struct dpu_hw_intf {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;

	/* intf */
	enum dpu_intf idx;
	const struct dpu_intf_cfg *cap;
	const struct dpu_mdss_cfg *mdss;

	/* ops */
	struct dpu_hw_intf_ops ops;
};

/**
 * to_dpu_hw_intf - convert base object dpu_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct dpu_hw_intf *to_dpu_hw_intf(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_intf, base);
}

/**
 * dpu_hw_intf_init(): Initializes the intf driver for the passed
 * interface idx.
 * @idx:  interface index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct dpu_hw_intf *dpu_hw_intf_init(enum dpu_intf idx,
		void __iomem *addr,
		struct dpu_mdss_cfg *m);

/**
 * dpu_hw_intf_destroy(): Destroys INTF driver context
 * @intf:   Pointer to INTF driver context
 */
void dpu_hw_intf_destroy(struct dpu_hw_intf *intf);

#endif /*_DPU_HW_INTF_H */

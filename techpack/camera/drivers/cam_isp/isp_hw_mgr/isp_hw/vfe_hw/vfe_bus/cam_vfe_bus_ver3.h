/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */


#ifndef _CAM_VFE_BUS_VER3_H_
#define _CAM_VFE_BUS_VER3_H_

#include "cam_irq_controller.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_hw_intf.h"

#define CAM_VFE_BUS_VER3_MAX_SUB_GRPS        6
#define CAM_VFE_BUS_VER3_MAX_MID_PER_PORT    4
#define CAM_VFE_BUS_VER3_CONS_ERR_MAX        32
#define CAM_VFE_BUS_VER3_MAX_CLIENTS         28
#define CAM_VFE_BUS_VER3_ERR_IRQ_REG_MAX     2
#define CAM_VFE_BUS_VER3_PAYLOAD_MAX         256
#define CAM_VFE_BUS_VER3_IRQ_MAX             2

#define MAX_BUF_UPDATE_REG_NUM   \
	((sizeof(struct cam_vfe_bus_ver3_reg_offset_bus_client) +  \
	sizeof(struct cam_vfe_bus_ver3_reg_offset_ubwc_client))/4)

#define MAX_REG_VAL_PAIR_SIZE    \
	(MAX_BUF_UPDATE_REG_NUM * 2 * CAM_PACKET_MAX_PLANES)

enum cam_vfe_bus_ver3_vfe_core_id {
	CAM_VFE_BUS_VER3_VFE_CORE_0,
	CAM_VFE_BUS_VER3_VFE_CORE_1,
	CAM_VFE_BUS_VER3_VFE_CORE_MAX,
};

enum cam_vfe_bus_ver3_src_grp {
	CAM_VFE_BUS_VER3_SRC_GRP_0,
	CAM_VFE_BUS_VER3_SRC_GRP_1,
	CAM_VFE_BUS_VER3_SRC_GRP_2,
	CAM_VFE_BUS_VER3_SRC_GRP_3,
	CAM_VFE_BUS_VER3_SRC_GRP_4,
	CAM_VFE_BUS_VER3_SRC_GRP_5,
	CAM_VFE_BUS_VER3_SRC_GRP_MAX,
};

enum cam_vfe_bus_ver3_comp_grp_type {
	CAM_VFE_BUS_VER3_COMP_GRP_0,
	CAM_VFE_BUS_VER3_COMP_GRP_1,
	CAM_VFE_BUS_VER3_COMP_GRP_2,
	CAM_VFE_BUS_VER3_COMP_GRP_3,
	CAM_VFE_BUS_VER3_COMP_GRP_4,
	CAM_VFE_BUS_VER3_COMP_GRP_5,
	CAM_VFE_BUS_VER3_COMP_GRP_6,
	CAM_VFE_BUS_VER3_COMP_GRP_7,
	CAM_VFE_BUS_VER3_COMP_GRP_8,
	CAM_VFE_BUS_VER3_COMP_GRP_9,
	CAM_VFE_BUS_VER3_COMP_GRP_10,
	CAM_VFE_BUS_VER3_COMP_GRP_11,
	CAM_VFE_BUS_VER3_COMP_GRP_12,
	CAM_VFE_BUS_VER3_COMP_GRP_13,
	CAM_VFE_BUS_VER3_COMP_GRP_14,
	CAM_VFE_BUS_VER3_COMP_GRP_15,
	CAM_VFE_BUS_VER3_COMP_GRP_16,
	CAM_VFE_BUS_VER3_COMP_GRP_MAX,
};

enum cam_vfe_bus_ver3_vfe_out_type {
	CAM_VFE_BUS_VER3_VFE_OUT_RDI0,
	CAM_VFE_BUS_VER3_VFE_OUT_RDI1,
	CAM_VFE_BUS_VER3_VFE_OUT_RDI2,
	CAM_VFE_BUS_VER3_VFE_OUT_RDI3,
	CAM_VFE_BUS_VER3_VFE_OUT_FULL,
	CAM_VFE_BUS_VER3_VFE_OUT_DS4,
	CAM_VFE_BUS_VER3_VFE_OUT_DS16,
	CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP,
	CAM_VFE_BUS_VER3_VFE_OUT_FD,
	CAM_VFE_BUS_VER3_VFE_OUT_PDAF,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST,
	CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP,
	CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP,
	CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP,
	CAM_VFE_BUS_VER3_VFE_OUT_2PD,
	CAM_VFE_BUS_VER3_VFE_OUT_LCR,
	CAM_VFE_BUS_VER3_VFE_OUT_SPARSE_PD,
	CAM_VFE_BUS_VER3_VFE_OUT_AWB_BFW,
	CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_2PD,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BE,
	CAM_VFE_BUS_VER3_VFE_OUT_LTM_STATS,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_GTM_BHIST,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_BG,
	CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_CAF,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_BAYER_RS,
	CAM_VFE_BUS_VER3_VFE_OUT_PDAF_PARSED,
	CAM_VFE_BUS_VER3_VFE_OUT_STATS_LITE_BHIST,
	CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW1,
	CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW2,
	CAM_VFE_BUS_VER3_VFE_OUT_MAX,
};

enum cam_vfe_bus_ver3_packer_format {
	PACKER_FMT_VER3_PLAIN_128,
	PACKER_FMT_VER3_PLAIN_8,
	PACKER_FMT_VER3_PLAIN_8_ODD_EVEN,
	PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10,
	PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10_ODD_EVEN,
	PACKER_FMT_VER3_PLAIN_16_10BPP,
	PACKER_FMT_VER3_PLAIN_16_12BPP,
	PACKER_FMT_VER3_PLAIN_16_14BPP,
	PACKER_FMT_VER3_PLAIN_16_16BPP,
	PACKER_FMT_VER3_PLAIN_32,
	PACKER_FMT_VER3_PLAIN_64,
	PACKER_FMT_VER3_TP_10,
	PACKER_FMT_VER3_MIPI10,
	PACKER_FMT_VER3_MIPI12,
	PACKER_FMT_VER3_MIPI14,
	PACKER_FMT_VER3_MIPI20,
	PACKER_FMT_VER3_PLAIN32_20BPP,
	PACKER_FMT_VER3_MAX,
};

/*
 * struct cam_vfe_constraint_error_info:
 *
 * @Brief:        Constraint error info
 */
struct cam_vfe_constraint_error_info {
	uint32_t  bitmask;
	char     *error_description;
};

/*
 * struct cam_vfe_bus_ver3_reg_offset_common:
 *
 * @Brief:        Common registers across all BUS Clients
 */
struct cam_vfe_bus_ver3_reg_offset_common {
	uint32_t hw_version;
	uint32_t cgc_ovd;
	uint32_t comp_cfg_0;
	uint32_t comp_cfg_1;
	uint32_t if_frameheader_cfg[CAM_VFE_BUS_VER3_MAX_SUB_GRPS];
	uint32_t tunneling_cfg;
	uint32_t ubwc_static_ctrl;
	uint32_t pwr_iso_cfg;
	uint32_t overflow_status_clear;
	uint32_t ccif_violation_status;
	uint32_t overflow_status;
	uint32_t image_size_violation_status;
	uint32_t debug_status_top_cfg;
	uint32_t debug_status_top;
	uint32_t test_bus_ctrl;
	uint32_t top_irq_mask_0;
	struct cam_irq_controller_reg_info irq_reg_info;
};

/*
 * struct cam_vfe_bus_ver3_reg_offset_ubwc_client:
 *
 * @Brief:        UBWC register offsets for BUS Clients
 */
struct cam_vfe_bus_ver3_reg_offset_ubwc_client {
	uint32_t meta_addr;
	uint32_t meta_cfg;
	uint32_t mode_cfg;
	uint32_t stats_ctrl;
	uint32_t ctrl_2;
	uint32_t lossy_thresh0;
	uint32_t lossy_thresh1;
	uint32_t off_lossy_var;
	uint32_t bw_limit;
	uint32_t ubwc_comp_en_bit;
};

/*
 * struct cam_vfe_bus_ver3_reg_offset_bus_client:
 *
 * @Brief:        Register offsets for BUS Clients
 */
struct cam_vfe_bus_ver3_reg_offset_bus_client {
	uint32_t cfg;
	uint32_t image_addr;
	uint32_t frame_incr;
	uint32_t image_cfg_0;
	uint32_t image_cfg_1;
	uint32_t image_cfg_2;
	uint32_t packer_cfg;
	uint32_t frame_header_addr;
	uint32_t frame_header_incr;
	uint32_t frame_header_cfg;
	uint32_t line_done_cfg;
	uint32_t irq_subsample_period;
	uint32_t irq_subsample_pattern;
	uint32_t framedrop_period;
	uint32_t framedrop_pattern;
	uint32_t mmu_prefetch_cfg;
	uint32_t mmu_prefetch_max_offset;
	uint32_t addr_cfg;
	uint32_t burst_limit;
	uint32_t system_cache_cfg;
	void    *ubwc_regs;
	uint32_t addr_status_0;
	uint32_t addr_status_1;
	uint32_t addr_status_2;
	uint32_t addr_status_3;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
	uint32_t bw_limiter_addr;
	uint32_t comp_group;
	uint32_t tunnel_cfg_idx;
};

enum cam_vfe_bus_ver3_stored_irq_masks {
	CAM_VFE_BUS_VER3_BUF_DONE_MASK,
	CAM_VFE_BUS_VER3_RUP_MASK,
	CAM_VFE_BUS_VER3_MAX_STORED_MASKS,
};

struct cam_vfe_bus_ver3_vfe_out_data {
	uint32_t                              out_type;
	uint32_t                              source_group;
	uint32_t                              buf_done_mask_shift;
	struct cam_vfe_bus_ver3_common_data  *common_data;
	struct cam_vfe_bus_ver3_priv         *bus_priv;

	uint32_t                         num_wm;
	struct cam_isp_resource_node    *wm_res;

	struct cam_isp_resource_node    *comp_grp;
	enum cam_isp_hw_sync_mode        dual_comp_sync_mode;
	uint32_t                         dual_hw_alternate_vfe_id;
	struct list_head                 vfe_out_list;

	uint32_t                         is_master;
	uint32_t                         is_dual;

	uint32_t                         format;
	uint32_t                         max_width;
	uint32_t                         max_height;
	struct cam_cdm_utils_ops        *cdm_util_ops;
	uint32_t                         secure_mode;
	uint32_t                         secure_mask;
	void                            *priv;
	uint32_t                         mid[CAM_VFE_BUS_VER3_MAX_MID_PER_PORT];
	bool                             limiter_enabled;
	uint32_t   stored_irq_masks[CAM_VFE_BUS_VER3_MAX_STORED_MASKS][CAM_VFE_BUS_VER3_IRQ_MAX];
};

/*
 * struct cam_vfe_bus_ver3_vfe_out_hw_info:
 *
 * @Brief:        HW capability of VFE Bus Client
 */
struct cam_vfe_bus_ver3_vfe_out_hw_info {
	enum cam_vfe_bus_ver3_vfe_out_type  vfe_out_type;
	uint32_t                            max_width;
	uint32_t                            max_height;
	uint32_t                            source_group;
	uint32_t                         mid[CAM_VFE_BUS_VER3_MAX_MID_PER_PORT];
	uint32_t                            num_wm;
	uint32_t                            line_based;
	uint32_t                            bufdone_shift;
	uint32_t                            wm_idx[PLANE_MAX];
	uint8_t                            *name[PLANE_MAX];
	uint32_t                            secure_mask;
};

/*
 * struct cam_vfe_bus_ver3_hw_info:
 *
 * @Brief:            HW register info for entire Bus
 *
 * @common_reg:               Common register details
 * @num_client:               Total number of write clients
 * @bus_client_reg:           Bus client register info
 * @vfe_out_hw_info:          VFE output capability
 * @num_cons_err:             Number of constraint errors in list
 * @constraint_error_list:    Static list of all constraint errors
 * @bus_error_irq_mask:       BUS errors mask that SW handles
 * @num_comp_grp:             Number of composite groups
 * @comp_done_shift:          Mask shift for comp done mask
 * @top_irq_shift:            Mask shift for top level BUS WR irq
 * @support_consumed_addr:    Indicate if bus support consumed address
 * @max_out_res:              Max vfe out resource value supported for hw
 * @supported_irq:            Mask to indicate the IRQ supported
 * @comp_cfg_needed:          Composite group config is needed for hw
 * @pack_align_shift:         Shift value for alignment of packer format
 * @max_bw_counter_limit:     Max BW counter limit
 * @no_tunnelingId_shift:     Mask shift for no tunneling ID irq
 * @tunneling_overflow_shift: Mask shift for tunneling overflow irq
 * @support_tunneling:        Indicate if bus support tunneling feature
 * @fifo_depth:               Max fifo depth
 */
struct cam_vfe_bus_ver3_hw_info {
	struct cam_vfe_bus_ver3_reg_offset_common common_reg;
	uint32_t num_client;
	struct cam_vfe_bus_ver3_reg_offset_bus_client
		bus_client_reg[CAM_VFE_BUS_VER3_MAX_CLIENTS];
	uint32_t num_out;
	struct cam_vfe_bus_ver3_vfe_out_hw_info
		vfe_out_hw_info[CAM_VFE_BUS_VER3_VFE_OUT_MAX];
	uint32_t num_cons_err;
	struct cam_vfe_constraint_error_info
		constraint_error_list[CAM_VFE_BUS_VER3_CONS_ERR_MAX];
	uint32_t bus_error_irq_mask[CAM_VFE_BUS_VER3_ERR_IRQ_REG_MAX];
	uint32_t num_comp_grp;
	uint32_t comp_done_shift;
	uint32_t top_irq_shift;
	bool support_consumed_addr;
	uint32_t max_out_res;
	uint32_t supported_irq;
	bool comp_cfg_needed;
	uint32_t pack_align_shift;
	uint32_t max_bw_counter_limit;
	uint32_t no_tunnelingId_shift;
	uint32_t tunneling_overflow_shift;
	bool support_tunneling;
	uint32_t fifo_depth;
};

/**
 * struct cam_vfe_bus_ver3_wm_mini_dump - VFE WM data
 *
 * @width                  Width
 * @height                 Height
 * @stride                 stride
 * @h_init                 init height
 * @acquired_width         acquired width
 * @acquired_height        acquired height
 * @en_cfg                 Enable flag
 * @format                 format
 * @index                  Index
 * @state                  state
 * @name                   Res name
 */
struct cam_vfe_bus_ver3_wm_mini_dump {
	uint32_t   width;
	uint32_t   height;
	uint32_t   stride;
	uint32_t   h_init;
	uint32_t   acquired_width;
	uint32_t   acquired_height;
	uint32_t   en_cfg;
	uint32_t   format;
	uint32_t   index;
	uint32_t   state;
	uint8_t    name[CAM_ISP_RES_NAME_LEN];
};

/**
 * struct cam_vfe_bus_ver3_mini_dump_data - VFE bus mini dump data
 *
 * @wm:              Write Master client information
 * @clk_rate:        Clock rate
 * @num_client:      Num client
 * @hw_state:        hw statte
 * @hw_idx:          Hw index
 */
struct cam_vfe_bus_ver3_mini_dump_data {
	struct cam_vfe_bus_ver3_wm_mini_dump *wm;
	uint64_t                              clk_rate;
	uint32_t                              num_client;
	uint8_t                               hw_state;
	uint8_t                               hw_idx;
};

struct cam_vfe_bus_ver3_wm_resource_data {
	uint32_t             index;
	struct cam_vfe_bus_ver3_common_data            *common_data;
	struct cam_vfe_bus_ver3_reg_offset_bus_client  *hw_regs;

	bool                 init_cfg_done;
	bool                 hfr_cfg_done;

	uint32_t             offset;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             format;
	enum cam_vfe_bus_ver3_packer_format pack_fmt;

	uint32_t             burst_len;

	uint32_t             en_ubwc;
	bool                 ubwc_updated;
	uint32_t             packer_cfg;
	uint32_t             h_init;
	uint32_t             ubwc_meta_addr;
	uint32_t             ubwc_meta_cfg;
	uint32_t             ubwc_mode_cfg;
	uint32_t             ubwc_stats_ctrl;
	uint32_t             ubwc_ctrl_2;

	uint32_t             irq_subsample_period;
	uint32_t             irq_subsample_pattern;
	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             en_cfg;
	uint32_t             is_dual;

	uint32_t             ubwc_lossy_threshold_0;
	uint32_t             ubwc_lossy_threshold_1;
	uint32_t             ubwc_offset_lossy_variance;
	uint32_t             ubwc_bandwidth_limit;
	uint32_t             acquired_width;
	uint32_t             acquired_height;
	uint32_t             default_line_based;
	bool                 use_wm_pack;

	uint32_t             tunnel_id;
	uint32_t             tunnel_en;
};

struct cam_vfe_bus_ver3_common_data {
	uint32_t                                    core_index;
	void __iomem                               *mem_base;
	struct cam_hw_intf                         *hw_intf;
	void                                       *bus_irq_controller;
	void                                       *vfe_irq_controller;
	void                                       *buf_done_controller;
	void                                       *priv;
	struct cam_vfe_bus_ver3_reg_offset_common  *common_reg;
	uint32_t                                    io_buf_update[
		MAX_REG_VAL_PAIR_SIZE];

	struct cam_vfe_bus_irq_evt_payload          evt_payload[
		CAM_VFE_BUS_VER3_PAYLOAD_MAX];
	struct list_head                            free_payload_list;
	spinlock_t                                  spin_lock;
	struct mutex                                bus_mutex;
	uint32_t                                    secure_mode;
	uint32_t                                    num_sec_out;
	uint32_t                                    addr_no_sync;
	uint32_t                                    comp_done_shift;
	uint32_t                                    supported_irq;
	bool                                        comp_config_needed;
	bool                                        is_lite;
	bool                                        hw_init;
	bool                                        support_consumed_addr;
	bool                                        disable_ubwc_comp;
	bool                                        init_irq_subscribed;
	bool                                        disable_mmu_prefetch;
	bool                                        support_tunneling;
	cam_hw_mgr_event_cb_func                    event_cb;
	int                                         rup_irq_handle[
		CAM_VFE_BUS_VER3_SRC_GRP_MAX];
	uint32_t                                    pack_align_shift;
	uint32_t                                    max_bw_counter_limit;
	uint32_t                                    no_tunnelingId_shift;
	uint32_t                                    tunneling_overflow_shift;
	uint32_t                                    out_fifo_depth;
};

int cam_vfe_populate_out(
	struct cam_isp_resource_node      *vfe_out,
	uint32_t                          *reg_payload,
	uint32_t                          *idx);
/*
 * cam_vfe_bus_ver3_init()
 *
 * @Brief:                   Initialize Bus layer
 *
 * @soc_info:                Soc Information for the associated HW
 * @hw_intf:                 HW Interface of HW to which this resource belongs
 * @bus_hw_info:             BUS HW info that contains details of BUS registers
 * @vfe_irq_controller:      VFE IRQ Controller to use for subscribing to Top
 *                           level IRQs
 * @vfe_bus:                 Pointer to vfe_bus structure which will be filled
 *                           and returned on successful initialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_ver3_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus);

/*
 * cam_vfe_bus_ver3_deinit()
 *
 * @Brief:                   Deinitialize Bus layer
 *
 * @vfe_bus:                 Pointer to vfe_bus structure to deinitialize
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_vfe_bus_ver3_deinit(struct cam_vfe_bus     **vfe_bus);

#endif /* _CAM_VFE_BUS_VER3_H_ */

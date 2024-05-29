/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_VFE_TOP_VER4_H_
#define _CAM_VFE_TOP_VER4_H_

#include "cam_vfe_top_common.h"
#include "cam_isp_hw.h"
#include "cam_vfe_top.h"

#define CAM_VFE_RDI_VER2_MAX                           4
#define CAM_VFE_CAMIF_LITE_EVT_MAX                     256
#define CAM_VFE_TOP_DBG_REG_MAX                        17

struct cam_vfe_top_ver4_common_data {
	struct cam_hw_intf                         *hw_intf;
	struct cam_vfe_top_ver4_reg_offset_common  *common_reg;
	struct cam_vfe_top_ver4_hw_info            *hw_info;
};

struct cam_vfe_top_ver4_reg_offset_common {
	uint32_t hw_version;
	uint32_t titan_version;
	uint32_t hw_capability;
	uint32_t lens_feature;
	uint32_t stats_feature;
	uint32_t color_feature;
	uint32_t zoom_feature;
	uint32_t global_reset_cmd;
	uint32_t core_cgc_ovd_0;
	uint32_t core_cgc_ovd_1;
	uint32_t ahb_cgc_ovd;
	uint32_t noc_cgc_ovd;
	uint32_t bus_cgc_ovd;
	uint32_t core_cfg_0;
	uint32_t core_cfg_1;
	uint32_t core_cfg_2;
	uint32_t core_cfg_3;
	uint32_t core_cfg_4;
	uint32_t core_cfg_5;
	uint32_t core_cfg_6;
	uint32_t period_cfg;
	uint32_t reg_update_cmd;
	uint32_t trigger_cdm_events;
	uint32_t ipp_violation_status;
	uint32_t pdaf_violation_status;
	uint32_t custom_frame_idx;
	uint32_t dsp_status;
	uint32_t diag_config;
	uint32_t diag_sensor_status_0;
	uint32_t diag_sensor_status_1;
	uint32_t diag_frm_cnt_status_0;
	uint32_t diag_frm_cnt_status_1;
	uint32_t stats_throttle_cfg_0;
	uint32_t stats_throttle_cfg_1;
	uint32_t stats_throttle_cfg_2;
	uint32_t bus_violation_status;
	uint32_t bus_overflow_status;
	uint32_t irq_sub_pattern_cfg;
	uint32_t epoch0_pattern_cfg;
	uint32_t epoch1_pattern_cfg;
	uint32_t epoch_height_cfg;
	uint32_t top_debug_cfg;
	uint32_t pdaf_input_cfg_0;
	uint32_t pdaf_input_cfg_1;
	uint32_t num_top_debug_reg;
	uint32_t top_debug[CAM_VFE_TOP_DBG_REG_MAX];
};

struct cam_vfe_top_common_cfg {
	uint32_t     vid_ds16_r2pd;
	uint32_t     vid_ds4_r2pd;
	uint32_t     disp_ds16_r2pd;
	uint32_t     disp_ds4_r2pd;
	uint32_t     dsp_streaming_tap_point;
	uint32_t     ihist_src_sel;
	uint32_t     input_pp_fmt;
	uint32_t     hdr_mux_sel_pp;
};

struct cam_vfe_top_ver4_module_desc {
	uint32_t id;
	uint8_t *desc;
};

struct cam_vfe_top_ver4_wr_client_desc {
	uint32_t  wm_id;
	uint8_t  *desc;
};

struct cam_vfe_top_ver4_top_err_irq_desc {
	uint32_t  bitmask;
	char     *err_name;
	char     *desc;
};

struct cam_vfe_top_ver4_pdaf_violation_desc {
	uint32_t  bitmask;
	char     *desc;
};

struct cam_vfe_top_ver4_pdaf_lcr_res_info {
	uint32_t  res_id;
	uint32_t  val;
};

struct cam_vfe_ver4_path_hw_info {
	struct cam_vfe_top_ver4_reg_offset_common  *common_reg;
	struct cam_vfe_ver4_path_reg_data          *reg_data;
};

struct cam_vfe_top_ver4_debug_reg_info {
	uint32_t  shift;
	char     *clc_name;
};

enum cam_vfe_top_ver4_fsm_state {
	VFE_TOP_VER4_FSM_SOF = 0,
	VFE_TOP_VER4_FSM_EPOCH,
	VFE_TOP_VER4_FSM_EOF,
	VFE_TOP_VER4_FSM_MAX,
};

enum cam_vfe_top_ver4_stored_irq_masks {
	VFE_TOP_VER4_FRAME_IRQ_MASK = 0,
	VFE_TOP_VER4_ERR_MASK,
	VFE_TOP_VER4_MAX_STORED_MASKS,
};

struct cam_vfe_mux_ver4_data {
	void __iomem                                *mem_base;
	struct cam_hw_soc_info                      *soc_info;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_top_ver4_reg_offset_common   *common_reg;
	struct cam_vfe_top_common_cfg                cam_common_cfg;
	struct cam_vfe_ver4_path_reg_data           *reg_data;
	struct cam_vfe_top_ver4_priv                *top_priv;

	cam_hw_mgr_event_cb_func             event_cb;
	void                                *priv;
	int                                  irq_err_handle;
	int                                  irq_handle;
	int                                  frame_irq_handle;
	void                                *vfe_irq_controller;
	struct cam_vfe_top_irq_evt_payload   evt_payload[CAM_VFE_CAMIF_EVT_MAX];
	struct list_head                     free_payload_list;
	spinlock_t                           spin_lock;

	enum cam_isp_hw_sync_mode          sync_mode;
	uint32_t                           dsp_mode;
	uint32_t                           pix_pattern;
	uint32_t                           first_pixel;
	uint32_t                           first_line;
	uint32_t                           last_pixel;
	uint32_t                           last_line;
	uint32_t                           hbi_value;
	uint32_t                           vbi_value;
	uint32_t                           irq_debug_cnt;
	uint32_t                           camif_debug;
	uint32_t                           horizontal_bin;
	uint32_t                           qcfa_bin;
	uint32_t                           dual_hw_idx;
	uint32_t                           is_dual;
	uint32_t                           epoch_factor;
	struct timespec64                  sof_ts;
	struct timespec64                  epoch_ts;
	struct timespec64                  eof_ts;
	struct timespec64                  error_ts;
	enum cam_vfe_top_ver4_fsm_state    fsm_state;
	uint32_t                           n_frame_irqs;
	bool                               is_fe_enabled;
	bool                               is_offline;
	bool                               is_lite;
	bool                               is_pixel_path;
	bool                               sfe_binned_epoch_cfg;
	bool                               enable_sof_irq_debug;
	bool                               handle_camif_irq;
	uint32_t  stored_irq_masks[VFE_TOP_VER4_MAX_STORED_MASKS][CAM_IFE_IRQ_REGISTERS_MAX];
};

struct cam_vfe_top_ver4_hw_info {
	struct cam_vfe_top_ver4_reg_offset_common  *common_reg;
	struct cam_vfe_ver4_path_hw_info            vfe_full_hw_info;
	struct cam_vfe_ver4_path_hw_info            pdlib_hw_info;
	struct cam_vfe_ver4_path_hw_info           *rdi_hw_info[CAM_VFE_RDI_VER2_MAX];
	struct cam_vfe_ver4_path_reg_data          *reg_data;
	struct cam_vfe_top_ver4_wr_client_desc     *wr_client_desc;
	struct cam_vfe_top_ver4_module_desc        *ipp_module_desc;
	uint32_t                                    num_reg;
	struct cam_vfe_top_ver4_debug_reg_info     (*debug_reg_info)[][8];
	uint32_t                                    num_mux;
	uint32_t                                    num_path_port_map;
	uint32_t mux_type[CAM_VFE_TOP_MUX_MAX];
	uint32_t path_port_map[CAM_ISP_HW_PATH_PORT_MAP_MAX][2];
	uint32_t                                     num_top_errors;
	struct cam_vfe_top_ver4_top_err_irq_desc    *top_err_desc;
	uint32_t                                     num_pdaf_violation_errors;
	struct cam_vfe_top_ver4_pdaf_violation_desc *pdaf_violation_desc;
	struct cam_vfe_top_ver4_pdaf_lcr_res_info   *pdaf_lcr_res_mask;
	uint32_t                                     num_pdaf_lcr_res;
	bool                                         secure_cdm_support;
};

struct cam_vfe_ver4_path_reg_data {
	uint32_t                                     epoch_line_cfg;
	uint32_t                                     sof_irq_mask;
	uint32_t                                     epoch0_irq_mask;
	uint32_t                                     epoch1_irq_mask;
	uint32_t                                     eof_irq_mask;
	uint32_t                                     error_irq_mask;
	uint32_t                                     enable_diagnostic_hw;
	uint32_t                                     top_debug_cfg_en;
	uint32_t                                     ipp_violation_mask;
	uint32_t                                     pdaf_violation_mask;
};


int cam_vfe_top_ver4_init(struct cam_hw_soc_info     *soc_info,
	struct cam_hw_intf                           *hw_intf,
	void                                         *top_hw_info,
	void                                         *vfe_irq_controller,
	struct cam_vfe_top                          **vfe_top);

int cam_vfe_populate_top(
	struct cam_vfe_top_ver4_priv      *top_priv,
	struct cam_isp_resource_node      *vfe_res,
	uint32_t                          *reg_payload,
	uint32_t                          *idx);

int cam_vfe_top_ver4_deinit(struct cam_vfe_top      **vfe_top);

#define VFE_DBG_INFO(shift_val, name) {.shift = shift_val, .clc_name = name}

#define VFE_DBG_INFO_ARRAY_4bit(name1, name2, name3, name4, name5, name6, name7, name8) \
	{                                                                               \
		VFE_DBG_INFO(0, name1),                                                 \
		VFE_DBG_INFO(4, name2),                                                 \
		VFE_DBG_INFO(8, name3),                                                 \
		VFE_DBG_INFO(12, name4),                                                \
		VFE_DBG_INFO(16, name5),                                                \
		VFE_DBG_INFO(20, name6),                                                \
		VFE_DBG_INFO(24, name7),                                                \
		VFE_DBG_INFO(28, name8),                                                \
	}

#endif /* _CAM_VFE_TOP_VER4_H_ */

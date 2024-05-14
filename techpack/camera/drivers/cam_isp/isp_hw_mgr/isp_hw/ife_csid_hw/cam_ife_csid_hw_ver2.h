/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_HW_VER2_H_
#define _CAM_IFE_CSID_HW_VER2_H_

#include "cam_hw.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_ife_csid_soc.h"
#include "cam_ife_csid_common.h"

#define IFE_CSID_VER2_RX_DL0_EOT_CAPTURED             BIT(0)
#define IFE_CSID_VER2_RX_DL1_EOT_CAPTURED             BIT(1)
#define IFE_CSID_VER2_RX_DL2_EOT_CAPTURED             BIT(2)
#define IFE_CSID_VER2_RX_DL3_EOT_CAPTURED             BIT(3)
#define IFE_CSID_VER2_RX_DL0_SOT_CAPTURED             BIT(4)
#define IFE_CSID_VER2_RX_DL1_SOT_CAPTURED             BIT(5)
#define IFE_CSID_VER2_RX_DL2_SOT_CAPTURED             BIT(6)
#define IFE_CSID_VER2_RX_DL3_SOT_CAPTURED             BIT(7)
#define IFE_CSID_VER2_RX_LONG_PKT_CAPTURED            BIT(8)
#define IFE_CSID_VER2_RX_SHORT_PKT_CAPTURED           BIT(9)
#define IFE_CSID_VER2_RX_CPHY_PKT_HDR_CAPTURED        BIT(10)
#define IFE_CSID_VER2_RX_CPHY_EOT_RECEPTION           BIT(11)
#define IFE_CSID_VER2_RX_CPHY_SOT_RECEPTION           BIT(12)
#define IFE_CSID_VER2_RX_ERROR_CPHY_PH_CRC            BIT(13)
#define IFE_CSID_VER2_RX_WARNING_ECC                  BIT(14)
#define IFE_CSID_VER2_RX_LANE0_FIFO_OVERFLOW          BIT(15)
#define IFE_CSID_VER2_RX_LANE1_FIFO_OVERFLOW          BIT(16)
#define IFE_CSID_VER2_RX_LANE2_FIFO_OVERFLOW          BIT(17)
#define IFE_CSID_VER2_RX_LANE3_FIFO_OVERFLOW          BIT(18)
#define IFE_CSID_VER2_RX_ERROR_CRC                    BIT(19)
#define IFE_CSID_VER2_RX_ERROR_ECC                    BIT(20)
#define IFE_CSID_VER2_RX_MMAPPED_VC_DT                BIT(21)
#define IFE_CSID_VER2_RX_UNMAPPED_VC_DT               BIT(22)
#define IFE_CSID_VER2_RX_STREAM_UNDERFLOW             BIT(23)
#define IFE_CSID_VER2_RX_UNBOUNDED_FRAME              BIT(24)
#define IFE_CSID_VER2_RX_RST_DONE                     BIT(27)

#define CAM_IFE_CSID_VER2_PAYLOAD_MAX           256

#define IFE_CSID_VER2_PATH_ERROR_ILLEGAL_PROGRAM                 BIT(0)
#define IFE_CSID_VER2_PATH_ERROR_FIFO_OVERFLOW                   BIT(2)
#define IFE_CSID_VER2_PATH_CAMIF_EOF                             BIT(3)
#define IFE_CSID_VER2_PATH_CAMIF_SOF                             BIT(4)
#define IFE_CSID_VER2_PATH_INFO_FRAME_DROP_EOF                   BIT(5)
#define IFE_CSID_VER2_PATH_INFO_FRAME_DROP_EOL                   BIT(6)
#define IFE_CSID_VER2_PATH_INFO_FRAME_DROP_SOL                   BIT(7)
#define IFE_CSID_VER2_PATH_INFO_FRAME_DROP_SOF                   BIT(8)
#define IFE_CSID_VER2_PATH_INFO_INPUT_EOF                        BIT(9)
#define IFE_CSID_VER2_PATH_INFO_INPUT_EOL                        BIT(10)
#define IFE_CSID_VER2_PATH_INFO_INPUT_SOL                        BIT(11)
#define IFE_CSID_VER2_PATH_INFO_INPUT_SOF                        BIT(12)
#define IFE_CSID_VER2_PATH_ERROR_PIX_COUNT                       BIT(13)
#define IFE_CSID_VER2_PATH_ERROR_LINE_COUNT                      BIT(14)
#define IFE_CSID_VER2_PATH_VCDT_GRP0_SEL                         BIT(15)
#define IFE_CSID_VER2_PATH_VCDT_GRP1_SEL                         BIT(16)
#define IFE_CSID_VER2_PATH_VCDT_GRP_CHANGE                       BIT(17)
#define IFE_CSID_VER2_PATH_FRAME_DROP                            BIT(18)
#define IFE_CSID_VER2_PATH_RECOVERY_OVERFLOW                     BIT(19)
#define IFE_CSID_VER2_PATH_ERROR_REC_CCIF_VIOLATION              BIT(20)
#define IFE_CSID_VER2_PATH_CAMIF_EPOCH0                          BIT(21)
#define IFE_CSID_VER2_PATH_CAMIF_EPOCH1                          BIT(22)
#define IFE_CSID_VER2_PATH_RUP_DONE                              BIT(23)
#define IFE_CSID_VER2_PATH_ILLEGAL_BATCH_ID                      BIT(24)
#define IFE_CSID_VER2_PATH_BATCH_END_MISSING_VIOLATION           BIT(25)
#define IFE_CSID_VER2_PATH_HEIGHT_VIOLATION                      BIT(26)
#define IFE_CSID_VER2_PATH_WIDTH_VIOLATION                       BIT(27)
#define IFE_CSID_VER2_PATH_SENSOR_SWITCH_OUT_OF_SYNC_FRAME_DROP  BIT(28)
#define IFE_CSID_VER2_PATH_CCIF_VIOLATION                        BIT(29)

/*Each Bit represents the index of cust node hw*/
#define IFE_CSID_VER2_CUST_NODE_IDX_0                      0x1
#define IFE_CSID_VER2_CUST_NODE_IDX_1                      0x2
#define IFE_CSID_VER2_CUST_NODE_IDX_2                      0x4

#define IFE_CSID_VER2_TOP_IRQ_STATUS_BUF_DONE                    BIT(13)

enum cam_ife_csid_ver2_input_core_sel {
	CAM_IFE_CSID_INPUT_CORE_SEL_NONE,
	CAM_IFE_CSID_INPUT_CORE_SEL_INTERNAL,
	CAM_IFE_CSID_INPUT_CORE_SEL_SFE_0,
	CAM_IFE_CSID_INPUT_CORE_SEL_SFE_1,
	CAM_IFE_CSID_INPUT_CORE_SEL_CUST_NODE_0,
	CAM_IFE_CSID_INPUT_CORE_SEL_CUST_NODE_1,
	CAM_IFE_CSID_INPUT_CORE_SEL_CUST_NODE_2,
	CAM_IFE_CSID_INPUT_CORE_SEL_MAX,
};

enum cam_ife_csid_ver2_csid_reset_loc {
	CAM_IFE_CSID_RESET_LOC_PATH_ONLY,
	CAM_IFE_CSID_RESET_LOC_COMPLETE,
	CAM_IFE_CSID_RESET_LOC_MAX,
};

enum cam_ife_csid_ver2_csid_reset_cmd {
	CAM_IFE_CSID_RESET_CMD_IRQ_CTRL,
	CAM_IFE_CSID_RESET_CMD_SW_RST,
	CAM_IFE_CSID_RESET_CMD_HW_RST,
	CAM_IFE_CSID_RESET_CMD_HW_MAX,
};

struct cam_ife_csid_ver2_top_cfg {
	uint32_t      input_core_type;
	uint32_t      dual_sync_core_sel;
	uint32_t      master_slave_sel;
	bool          dual_en;
	bool          offline_sfe_en;
	bool          out_ife_en;
	bool          rdi_lcr;
};

struct cam_ife_csid_ver2_evt_payload {
	struct list_head            list;
	uint32_t                    irq_reg_val[CAM_IFE_CSID_IRQ_REG_MAX];
};


enum cam_ife_csid_ver2_stored_irq_masks {
	CAM_IFE_CSID_TOP_MASK,
	CAM_IFE_CSID_SOF_DISCARD_MASK,
	CAM_IFE_CSID_ERR_MASK,
	CAM_IFE_CSID_MAX_STORED_MASKS,
};

/*
 * struct cam_ife_csid_ver2_path_cfg: place holder for path parameters
 *
 * @camif_data:             CAMIF data
 * @error_ts:               Error timestamp
 * @cid:                    cid value for path
 * @path_format:            Array of Path format which contains format
 *                          info i.e Decode format, Packing format etc
 * @sec_evt_config:         Secondary event config from HW mgr for a given path
 * @in_format:              Array of input format which contains format type
 * @out_format:             output format
 * @start_pixel:            start pixel for horizontal crop
 * @end_pixel:              end pixel for horizontal  crop
 * @start_line:             start line for vertical crop
 * @end_line:               end line for vertical crop
 * @width:                  width of incoming data
 * @height:                 height of incoming data
 * @master_idx:             master idx
 * @horizontal_bin:         horizontal binning enable/disable on path
 * @vertical_bin:           vertical binning enable/disable on path
 * @qcfa_bin    :           qcfa binning enable/disable on path
 * @hor_ver_bin :           horizontal vertical binning enable/disable on path
 * @num_bytes_out:          Number of bytes out
 * @irq_handle:             IRQ handle
 * @err_irq_handle:         Error IRQ handle
 * @discard_irq_handle:     IRQ handle for SOF when discarding initial frames
 * @irq_reg_idx:            IRQ Reg index
 * @sof_cnt:                SOF counter
 * @num_frames_discard:     number of frames to discard
 * @epoch_cfg:              Epoch configured value
 * @stored_irq_masks:       Stored irq mask for each resource path
 * @sync_mode   :           Sync mode--> master/slave/none
 * @vfr_en   :              flag to indicate if variable frame rate is enabled
 * @frame_id_dec_en:        flag to indicate if frame id decoding is enabled
 * @crop_enable:            flag to indicate crop enable
 * @drop_enable:            flag to indicate drop enable
 * @discard_init_frames:    discard initial frames
 * @skip_discard_frame_cfg: Skip handling discard config, the blob order between
 *                          discard config and dynamic switch update cannot be gauranteed
 *                          If we know the number of paths to avoid configuring discard
 *                          for before processing discard config we can skip it for
 *                          the corresponding paths
 * @sfe_shdr:               flag to indicate if sfe is inline shdr
 * @lcr_en:                 Flag to indicate if path is part can be input to LCR
 *
 */
struct cam_ife_csid_ver2_path_cfg {
	struct timespec64                    error_ts;
	struct cam_ife_csid_path_format      path_format[CAM_ISP_VC_DT_CFG];
	struct cam_csid_secondary_evt_config sec_evt_config;
	uint32_t                             cid;
	uint32_t                             in_format[CAM_ISP_VC_DT_CFG];
	uint32_t                             out_format;
	uint32_t                             start_pixel;
	uint32_t                             end_pixel;
	uint32_t                             width;
	uint32_t                             start_line;
	uint32_t                             end_line;
	uint32_t                             height;
	uint32_t                             master_idx;
	uint64_t                             clk_rate;
	uint32_t                             horizontal_bin;
	uint32_t                             vertical_bin;
	uint32_t                             qcfa_bin;
	uint32_t                             hor_ver_bin;
	uint32_t                             num_bytes_out;
	uint32_t                             irq_handle;
	uint32_t                             err_irq_handle;
	uint32_t                             discard_irq_handle;
	uint32_t                             irq_reg_idx;
	uint32_t                             sof_cnt;
	uint32_t                             num_frames_discard;
	uint32_t                             epoch_cfg;
	uint32_t   stored_irq_masks[CAM_IFE_CSID_MAX_STORED_MASKS][CAM_IFE_CSID_IRQ_REG_MAX];
	enum cam_isp_hw_sync_mode            sync_mode;
	bool                                 vfr_en;
	bool                                 frame_id_dec_en;
	bool                                 crop_enable;
	bool                                 drop_enable;
	bool                                 discard_init_frames;
	bool                                 skip_discard_frame_cfg;
	bool                                 sfe_shdr;
	bool                                 lcr_en;
	bool                                 csid_out_unpack_msb;
	bool                                 handle_camif_irq;
};

struct cam_ife_csid_ver2_top_reg_info {
	uint32_t io_path_cfg0_addr[CAM_IFE_CSID_HW_NUM_MAX];
	uint32_t dual_csid_cfg0_addr[CAM_IFE_CSID_HW_NUM_MAX];
	uint32_t input_core_type_shift_val;
	uint32_t sfe_offline_en_shift_val;
	uint32_t out_ife_en_shift_val;
	uint32_t dual_sync_sel_shift_val;
	uint32_t dual_en_shift_val;
	uint32_t master_slave_sel_shift_val;
	uint32_t rdi_lcr_shift_val;
	uint32_t master_sel_val;
	uint32_t slave_sel_val;
	uint32_t io_path_cfg_rst_val;
	uint32_t dual_cfg_rst_val;
};

struct cam_ife_csid_ver2_path_reg_info {
	uint32_t irq_status_addr;
	uint32_t irq_mask_addr;
	uint32_t irq_clear_addr;
	uint32_t irq_set_addr;
	uint32_t cfg0_addr;
	uint32_t ctrl_addr;
	uint32_t debug_clr_cmd_addr;
	uint32_t multi_vcdt_cfg0_addr;
	uint32_t cfg1_addr;
	uint32_t sparse_pd_extractor_cfg_addr;
	uint32_t err_recovery_cfg0_addr;
	uint32_t err_recovery_cfg1_addr;
	uint32_t err_recovery_cfg2_addr;
	uint32_t bin_pd_detect_cfg0_addr;
	uint32_t bin_pd_detect_cfg1_addr;
	uint32_t bin_pd_detect_cfg2_addr;
	uint32_t debug_byte_cntr_ping_addr;
	uint32_t debug_byte_cntr_pong_addr;
	uint32_t camif_frame_cfg_addr;
	uint32_t epoch_irq_cfg_addr;
	uint32_t epoch0_subsample_ptrn_addr;
	uint32_t epoch1_subsample_ptrn_addr;
	uint32_t debug_camif_1_addr;
	uint32_t debug_camif_0_addr;
	uint32_t frm_drop_pattern_addr;
	uint32_t frm_drop_period_addr;
	uint32_t irq_subsample_pattern_addr;
	uint32_t irq_subsample_period_addr;
	uint32_t hcrop_addr;
	uint32_t vcrop_addr;
	uint32_t pix_drop_pattern_addr;
	uint32_t pix_drop_period_addr;
	uint32_t line_drop_pattern_addr;
	uint32_t line_drop_period_addr;
	uint32_t debug_halt_status_addr;
	uint32_t debug_misr_val0_addr;
	uint32_t debug_misr_val1_addr;
	uint32_t debug_misr_val2_addr;
	uint32_t debug_misr_val3_addr;
	uint32_t format_measure_cfg0_addr;
	uint32_t format_measure_cfg1_addr;
	uint32_t format_measure0_addr;
	uint32_t format_measure1_addr;
	uint32_t format_measure2_addr;
	uint32_t timestamp_curr0_sof_addr;
	uint32_t timestamp_curr1_sof_addr;
	uint32_t timestamp_perv0_sof_addr;
	uint32_t timestamp_perv1_sof_addr;
	uint32_t timestamp_curr0_eof_addr;
	uint32_t timestamp_curr1_eof_addr;
	uint32_t timestamp_perv0_eof_addr;
	uint32_t timestamp_perv1_eof_addr;
        uint32_t lut_bank_cfg_addr;
	uint32_t batch_id_cfg0_addr;
	uint32_t batch_id_cfg1_addr;
	uint32_t batch_period_cfg_addr;
	uint32_t batch_stream_id_cfg_addr;
	uint32_t epoch0_cfg_batch_id0_addr;
	uint32_t epoch1_cfg_batch_id0_addr;
	uint32_t epoch0_cfg_batch_id1_addr;
	uint32_t epoch1_cfg_batch_id1_addr;
	uint32_t epoch0_cfg_batch_id2_addr;
	uint32_t epoch1_cfg_batch_id2_addr;
	uint32_t epoch0_cfg_batch_id3_addr;
	uint32_t epoch1_cfg_batch_id3_addr;
	uint32_t epoch0_cfg_batch_id4_addr;
	uint32_t epoch1_cfg_batch_id4_addr;
	uint32_t epoch0_cfg_batch_id5_addr;
	uint32_t epoch1_cfg_batch_id5_addr;

	/*Shift Bit Configurations*/
	uint32_t start_mode_master;
	uint32_t start_mode_internal;
	uint32_t start_mode_global;
	uint32_t start_mode_slave;
	uint32_t start_mode_shift;
	uint32_t start_master_sel_val;
	uint32_t start_master_sel_shift;
	uint32_t resume_frame_boundary;
	uint32_t offline_mode_supported;
	uint32_t mipi_pack_supported;
	uint32_t packing_fmt_shift_val;
	uint32_t plain_fmt_shift_val;
	uint32_t plain_alignment_shift_val;
	uint32_t crop_v_en_shift_val;
	uint32_t crop_h_en_shift_val;
	uint32_t drop_v_en_shift_val;
	uint32_t drop_h_en_shift_val;
        uint32_t pix_store_en_shift_val;
	uint32_t early_eof_en_shift_val;
	uint32_t bin_h_en_shift_val;
	uint32_t bin_v_en_shift_val;
	uint32_t bin_en_shift_val;
	uint32_t bin_qcfa_en_shift_val;
	uint32_t format_measure_en_shift_val;
	uint32_t timestamp_en_shift_val;
	uint32_t min_hbi_shift_val;
	uint32_t start_master_sel_shift_val;
	uint32_t bin_pd_en_shift_val;
	uint32_t bin_pd_blk_w_shift_val;
	uint32_t bin_pd_blk_h_shift_val;
	uint32_t bin_pd_detect_x_offset_shift_val;
	uint32_t bin_pd_detect_x_end_shift_val;
	uint32_t bin_pd_detect_y_offset_shift_val;
	uint32_t bin_pd_detect_y_end_shift_val;
	uint32_t byte_cntr_en_shift_val;
	uint32_t offline_mode_en_shift_val;
	uint32_t debug_byte_cntr_rst_shift_val;
	uint32_t stripe_loc_shift_val;
	uint32_t pix_pattern_shift_val;
	uint32_t ccif_violation_en;
	uint32_t overflow_ctrl_mode_val;
	uint32_t overflow_ctrl_en;
	uint32_t lut_bank_0_sel_val;
	uint32_t lut_bank_1_sel_val;
	uint32_t binning_supported;
	uint32_t fatal_err_mask;
	uint32_t non_fatal_err_mask;
	uint32_t pix_pattern_shift;
	uint32_t rup_irq_mask;
	uint32_t sof_irq_mask;
	uint32_t eof_irq_mask;
	uint32_t epoch0_irq_mask;
	uint32_t epoch1_irq_mask;
	uint32_t rup_aup_mask;
	uint32_t top_irq_mask;
	uint32_t epoch0_cfg_val;
	uint32_t epoch1_cfg_val;
	uint32_t epoch0_shift_val;
	uint32_t epoch1_shift_val;
	uint32_t sof_retiming_dis_shift;
	uint32_t capabilities;
};

struct cam_ife_csid_ver2_common_reg_info {
	uint32_t hw_version_addr;
	uint32_t cfg0_addr;
	uint32_t global_cmd_addr;
	uint32_t reset_cfg_addr;
	uint32_t reset_cmd_addr;
	uint32_t rup_aup_cmd_addr;
	uint32_t offline_cmd_addr;
	uint32_t shdr_master_slave_cfg_addr;
	uint32_t top_irq_status_addr;
	uint32_t top_irq_mask_addr;
	uint32_t top_irq_clear_addr;
	uint32_t top_irq_set_addr;
	uint32_t irq_cmd_addr;
	uint32_t buf_done_irq_status_addr;
	uint32_t buf_done_irq_mask_addr;
	uint32_t buf_done_irq_clear_addr;
	uint32_t buf_done_irq_set_addr;

	/*Shift Bit Configurations*/
	uint32_t rst_done_shift_val;
	uint32_t rst_location_shift_val;
	uint32_t rst_mode_shift_val;
	uint32_t timestamp_stb_sel_shift_val;
	uint32_t frame_id_decode_en_shift_val;
	uint32_t vfr_en_shift_val;
	uint32_t decode_format_shift_val;
	uint32_t decode_format1_shift_val;
	bool     decode_format1_supported;
	uint32_t decode_format_mask;
	uint32_t start_mode_shift_val;
	uint32_t start_cmd_shift_val;
	uint32_t path_en_shift_val;
	uint32_t dt_id_shift_val;
	uint32_t vc_shift_val;
	uint32_t vc_mask;
	uint32_t dt_shift_val;
	uint32_t dt_mask;
	uint32_t crop_shift_val;
	uint32_t debug_frm_drop_rst_shift_val;
	uint32_t debug_timestamp_rst_shift_val;
	uint32_t debug_format_measure_rst_shift_val;
	uint32_t debug_misr_rst_shift_val;
	uint32_t num_padding_pixels_shift_val;
	uint32_t num_padding_rows_shift_val;
	uint32_t num_vbi_lines_shift_val;
	uint32_t num_hbi_cycles_shift_val;
	uint32_t epoch0_line_shift_val;
	uint32_t epoch1_line_shift_val;
	uint32_t camif_width_shift_val;
	uint32_t camif_height_shift_val;
	uint32_t batch_id0_shift_val;
	uint32_t batch_id1_shift_val;
	uint32_t batch_id2_shift_val;
	uint32_t batch_id3_shift_val;
	uint32_t batch_id4_shift_val;
	uint32_t batch_id5_shift_val;
	uint32_t batch_id0_period_shift_val;
	uint32_t batch_id1_period_shift_val;
	uint32_t batch_id2_period_shift_val;
	uint32_t batch_id3_period_shift_val;
	uint32_t batch_id4_period_shift_val;
	uint32_t batch_id5_period_shift_val;
	uint32_t stream_id_len_shift_val;
	uint32_t stream_id_x_offset_shift_val;
	uint32_t stream_id_y_offset_shift_val;
	uint32_t multi_vcdt_vc1_shift_val;
	uint32_t multi_vcdt_dt1_shift_val;
	uint32_t multi_vcdt_ts_combo_en_shift_val;
	uint32_t multi_vcdt_en_shift_val;
	uint32_t mup_shift_val;
	uint32_t shdr_slave_ppp_shift;
	uint32_t shdr_slave_rdi2_shift;
	uint32_t shdr_slave_rdi1_shift;
	uint32_t shdr_master_rdi0_shift;
	uint32_t shdr_master_slave_en_shift;
	/* config Values */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t num_udis;
	uint32_t num_rdis;
	uint32_t num_pix;
	uint32_t num_ppp;
	uint32_t rst_loc_path_only_val;
	uint32_t rst_loc_complete_csid_val;
	uint32_t rst_mode_frame_boundary_val;
	uint32_t rst_mode_immediate_val;
	uint32_t rst_cmd_irq_ctrl_only_val;
	uint32_t rst_cmd_sw_reset_complete_val;
	uint32_t rst_cmd_hw_reset_complete_val;
	uint32_t vfr_supported;
	uint32_t frame_id_dec_supported;
	uint32_t drop_supported;
	uint32_t multi_vcdt_supported;
	uint32_t timestamp_strobe_val;
	uint32_t overflow_ctrl_mode_val;
	uint32_t overflow_ctrl_en;
	uint32_t early_eof_supported;
	uint32_t global_reset;
	uint32_t rup_supported;
	uint32_t only_master_rup;
	bool     timestamp_enabled_in_cfg0;
	bool     camif_irq_support;

	/* Masks */
	uint32_t pxl_cnt_mask;
	uint32_t line_cnt_mask;
	uint32_t hblank_max_mask;
	uint32_t hblank_min_mask;
	uint32_t epoch0_line_mask;
	uint32_t epoch1_line_mask;
	uint32_t camif_width_mask;
	uint32_t camif_height_mask;
	uint32_t crop_pix_start_mask;
	uint32_t crop_pix_end_mask;
	uint32_t crop_line_start_mask;
	uint32_t crop_line_end_mask;
	uint32_t format_measure_height_mask_val;
	uint32_t format_measure_height_shift_val;
	uint32_t format_measure_width_mask_val;
	uint32_t format_measure_width_shift_val;
	uint32_t measure_en_hbi_vbi_cnt_mask;
	uint32_t measure_pixel_line_en_mask;
	uint32_t ipp_irq_mask_all;
	uint32_t rdi_irq_mask_all;
	uint32_t ppp_irq_mask_all;
	uint32_t udi_irq_mask_all;
	uint32_t top_err_irq_mask;
	uint32_t top_reset_irq_mask;
	uint32_t top_buf_done_irq_mask;
	uint32_t epoch_factor;
	uint32_t decode_format_payload_only;
};

struct cam_ife_csid_ver2_reg_info {
	struct cam_irq_controller_reg_info               *irq_reg_info;
	struct cam_irq_controller_reg_info               *buf_done_irq_reg_info;
	const struct cam_ife_csid_ver2_common_reg_info   *cmn_reg;
	const struct cam_ife_csid_csi2_rx_reg_info       *csi2_reg;
	const struct cam_ife_csid_ver2_path_reg_info     *path_reg[
						    CAM_IFE_PIX_PATH_RES_MAX];
	const struct cam_ife_csid_ver2_top_reg_info      *top_reg;
	const uint32_t                                    need_top_cfg;
	const uint32_t                                    csid_cust_node_map[
		    CAM_IFE_CSID_HW_NUM_MAX];
	const int                                         input_core_sel[
		    CAM_IFE_CSID_HW_NUM_MAX][CAM_IFE_CSID_INPUT_CORE_SEL_MAX];
	const struct cam_ife_csid_irq_desc               *rx_irq_desc;
	const struct cam_ife_csid_irq_desc               *path_irq_desc;
	const struct cam_ife_csid_top_irq_desc           *top_irq_desc;
	const uint32_t                                    num_top_err_irqs;
};

/*
 * struct cam_ife_csid_token_info: place holder for csid res path context private data
 *
 * @token:        context private of ife hw manager
 * @res_id:       Unique resource ID within res_type objects
 *                for a particular HW
 *
 */
struct cam_ife_csid_token_info {
	void                    *token;
	int                      res_id;
};

/*
 * struct cam_ife_csid_ver2_hw: place holder for csid hw
 *
 * @path_res:                 array of path resources
 * @cid_data:                 cid data
 * @rx_cfg:                   rx configuration
 * @flags:                    flags
 * @irq_complete:             complete variable for reset irq
 * @debug_info:               Debug info to capture debug info
 * @timestamp:                Timestamp info
 * @timestamp:                Timestamp info
 * @rx_evt_payload            Payload for rx events
 * @path_evt_payload               Payload for path events
 * @rx_free_payload_list:     Free Payload list for rx events
 * @free_payload_list:        Free Payload list for rx events
 * @lock_state :              mutex lock
 * @payload_lock:             spin lock for path payload
 * @rx_payload_lock:          spin lock for rx payload
 * @csid_irq_controller:      common csid irq controller
 * @buf_done_irq_controller:  buf done irq controller
 * @hw_info:                  hw info
 * @core_info:                csid core info
 * @token_data:               array of context private of ife hw manager
 * @event_cb:                 Event cb to ife hw manager
 * @counters:                 counters used in csid hw
 * @log_buf:                  Log Buffer to dump info
 * @clk_rate:                 clk rate for csid hw
 * @res_type:                 cur res type for active hw
 * @dual_core_idx:            core idx in case of dual csid
 * @worker:                   Worker for irq events
 * @reset_irq_handle:         Reset irq handle
 * @buf_done_irq_handle:      Buf done irq handle
 * @sync_mode:                Master/Slave modes
 * @mup:                      MUP for incoming VC of next frame
 * @discard_frame_per_path:   Count of paths dropping initial frames
 * @init_global_reset_cnt:    Count of global reset called during init
 * @rup_aup_mask:             rup aup mask enabled for particular HW
 * @per_port_group_index:     Group index if per port is enabled
 *
 */
struct cam_ife_csid_ver2_hw {
	struct cam_isp_resource_node           path_res
						    [CAM_IFE_PIX_PATH_RES_MAX];
	struct cam_ife_csid_cid_data           cid_data[CAM_IFE_CSID_CID_MAX];
	struct cam_ife_csid_ver2_top_cfg       top_cfg;
	struct cam_ife_csid_rx_cfg             rx_cfg;
	struct cam_ife_csid_hw_counters        counters;
	struct cam_ife_csid_hw_flags           flags;
	struct cam_ife_csid_debug_info         debug_info;
	struct cam_ife_csid_timestamp          timestamp[CAM_IFE_PIX_PATH_RES_MAX];
	struct cam_ife_csid_ver2_evt_payload   rx_evt_payload[
						CAM_IFE_CSID_VER2_PAYLOAD_MAX];
	struct cam_ife_csid_ver2_evt_payload   path_evt_payload[
						CAM_IFE_CSID_VER2_PAYLOAD_MAX];
	struct list_head                       rx_free_payload_list;
	struct list_head                       path_free_payload_list;
	struct mutex                           lock_state;
	spinlock_t                             path_payload_lock;
	spinlock_t                             rx_payload_lock;
	void                                  *csid_irq_controller;
	void                                  *buf_done_irq_controller;
	struct cam_hw_intf                    *hw_intf;
	struct cam_hw_info                    *hw_info;
	struct cam_ife_csid_core_info         *core_info;
	struct cam_ife_csid_token_info         token_data[CAM_IFE_PIX_PATH_RES_MAX];
	cam_hw_mgr_event_cb_func               event_cb;
	uint8_t                                log_buf
						[CAM_IFE_CSID_LOG_BUF_LEN];
	uint64_t                               clk_rate;
	uint32_t                               res_type;
	uint32_t                               dual_core_idx;
	void                                  *worker;
	int                                    reset_irq_handle;
	int                                    buf_done_irq_handle;
	int                                    top_err_irq_handle;
	enum cam_isp_hw_sync_mode              sync_mode;
	uint32_t                               mup;
	atomic_t                               discard_frame_per_path;
	atomic_t                               init_global_reset_cnt;
	uint32_t                               rup_aup_mask;
	int                                    per_port_group_index;
};

/*
 * struct cam_ife_csid_res_mini_dump: CSID Res mini dump place holder
 * @res_id:      Res id
 * @res_name:    Res name
 * @path_cfg:    path configuration
 */
struct cam_ife_csid_ver2_res_mini_dump {
	uint32_t                           res_id;
	uint8_t                            res_name[CAM_ISP_RES_NAME_LEN];
	struct cam_ife_csid_ver2_path_cfg  path_cfg;
};

/*
 * struct cam_ife_csid_mini_dump_data: CSID mini dump place holder
 *
 * @res:             Mini dump data for res
 * @flags:           Flags
 * @rx_cfg:          Rx configuration
 * @cid_data:        CID data
 * @clk_rate:        Clock Rate
 * @hw_state:        hw state
 */
struct cam_ife_csid_ver2_mini_dump_data {
	struct cam_ife_csid_ver2_res_mini_dump  res[CAM_IFE_PIX_PATH_RES_MAX];
	struct cam_ife_csid_hw_flags            flags;
	struct cam_ife_csid_rx_cfg              rx_cfg;
	struct cam_ife_csid_cid_data            cid_data[CAM_IFE_CSID_CID_MAX];
	uint64_t                                clk_rate;
	uint8_t                                 hw_state;
};

int cam_ife_csid_hw_ver2_init(struct cam_hw_intf  *csid_hw_intf,
	struct cam_ife_csid_core_info *csid_core_info,
	bool is_custom);

int cam_ife_csid_hw_ver2_deinit(struct cam_hw_info *hw_priv);
void cam_ife_csid_ver2_print_illegal_programming_irq_status(void *csid_hw, void *res);
void cam_ife_csid_ver2_print_format_measure_info(void *csid_hw, void *res);
void cam_ife_csid_hw_ver2_mup_mismatch_handler(void *csid_hw, void *res);
void cam_ife_csid_hw_ver2_rdi_line_buffer_conflict_handler(void *csid_hw);

#endif

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_IFE_HW_MGR_H_
#define _CAM_IFE_HW_MGR_H_

#include <linux/completion.h>
#include <linux/time.h>
#include "cam_isp_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_cdm_intf_api.h"

/*
 * enum cam_ife_ctx_master_type - HW master type
 * CAM_IFE_CTX_TYPE_NONE: IFE ctx/stream directly connected to CSID
 * CAM_IFE_CTX_TYPE_CUSTOM: IFE ctx/stream connected to custom HW
 * CAM_IFE_CTX_TYPE_SFE: IFE ctx/stream connected to SFE
 */
enum cam_ife_ctx_master_type {
	CAM_IFE_CTX_TYPE_NONE,
	CAM_IFE_CTX_TYPE_CUSTOM,
	CAM_IFE_CTX_TYPE_SFE,
	CAM_IFE_CTX_TYPE_MAX,
};

/* IFE resource constants */
#define CAM_IFE_HW_IN_RES_MAX                (CAM_ISP_IFE_IN_RES_MAX & 0xFF)
#define CAM_SFE_HW_OUT_RES_MAX               (CAM_ISP_SFE_OUT_RES_MAX & 0xFF)
#define CAM_IFE_HW_RES_POOL_MAX               64
#define CAM_IFE_HW_STREAM_GRP_RES_POOL_MAX    32

/* IFE_HW_MGR ctx config */
#define CAM_IFE_CTX_CFG_FRAME_HEADER_TS   BIT(0)
#define CAM_IFE_CTX_CFG_SW_SYNC_ON        BIT(1)
#define CAM_IFE_CTX_CFG_DYNAMIC_SWITCH_ON BIT(2)

/**
 * struct cam_ife_hw_mgr_debug - contain the debug information
 *
 * @dentry:                    Debugfs entry
 * @csid_debug:                csid debug information
 * @enable_recovery:           enable recovery
 * @camif_debug:               camif debug info
 * @enable_csid_recovery:      enable csid recovery
 * @sfe_debug:                 sfe debug config
 * @sfe_sensor_diag_cfg:       sfe sensor diag config
 * @sfe_cache_debug:           sfe cache debug info
 * @enable_req_dump:           Enable request dump on HW errors
 * @per_req_reg_dump:          Enable per request reg dump
 * @disable_ubwc_comp:         Disable UBWC compression
 * @disable_ife_mmu_prefetch:  Disable MMU prefetch for IFE bus WR
 * @disable_line_based_mode:   Disable line based mode for per port
 *                             feature with duplicate sensors
 *
 */
struct cam_ife_hw_mgr_debug {
	struct dentry  *dentry;
	uint64_t       csid_debug;
	uint32_t       enable_recovery;
	uint32_t       camif_debug;
	uint32_t       enable_csid_recovery;
	uint32_t       sfe_debug;
	uint32_t       sfe_sensor_diag_cfg;
	uint32_t       sfe_cache_debug[CAM_SFE_HW_NUM_MAX];
	bool           enable_req_dump;
	bool           per_req_reg_dump;
	bool           disable_ubwc_comp;
	bool           disable_ife_mmu_prefetch;
	bool           disable_line_based_mode;
};

/**
 * struct cam_ife_hw_mgr_ctx_pf_info - pf buf info
 *
 * @out_port_id: Out port id
 * @mid: MID value
 */
struct cam_ife_hw_mgr_ctx_pf_info {
	uint32_t       out_port_id;
	uint32_t       mid;
};

/**
 * struct cam_ife_sfe_scratch_buf_info - Scratch buf info
 *
 * @width: Width in pixels
 * @height: Height in pixels
 * @stride: Stride in pixels
 * @slice_height: Height in lines
 * @io_addr: Buffer address
 * @res_id: Resource type
 * @offset: Buffer offset
 * @config_done: To indicate if RDIx received scratch cfg
 * @is_secure: secure scratch buffer
 */
struct cam_ife_sfe_scratch_buf_info {
	uint32_t   width;
	uint32_t   height;
	uint32_t   stride;
	uint32_t   slice_height;
	dma_addr_t io_addr;
	uint32_t   res_id;
	uint32_t   offset;
	bool       config_done;
	bool       is_secure;
};

/**
 * struct cam_sfe_scratch_buf_cfg - Scratch buf info
 *
 * @num_configs     : Total Number of scratch buffers provided
 * @updated_num_exp : Current num of exposures
 * @buf_info        : Info on each of the buffers
 *
 */
struct cam_sfe_scratch_buf_cfg {
	uint32_t                            num_config;
	uint32_t                            updated_num_exp;
	struct cam_ife_sfe_scratch_buf_info buf_info[
		CAM_SFE_FE_RDI_NUM_MAX];
};

/**
 * struct cam_sfe_scratch_buf_cfg - Scratch buf info
 *
 * @num_ports: Total Number of scratch buffers provided
 * @buf_info : Info on each of the buffers
 *
 */
struct cam_ife_scratch_buf_cfg {
	uint32_t                            num_config;
	struct cam_ife_sfe_scratch_buf_info buf_info[
		CAM_IFE_SCRATCH_NUM_MAX];
};


/**
 * struct cam_ife_hw_mgr_sfe_info - SFE info
 *
 * @skip_scratch_cfg_streamon: Determine if scratch cfg needs to be programmed at stream on
 * @num_fetches:               Indicate number of SFE fetches for this stream
 * @scratch_config:            Scratch buffer config if any for SFE ports
 * @ife_scratch_config:        Scratch buffer config if any for IFE ports
 */
struct cam_ife_hw_mgr_sfe_info {
	bool                            skip_scratch_cfg_streamon;
	uint32_t                        num_fetches;
	struct cam_sfe_scratch_buf_cfg *scratch_config;
	struct cam_ife_scratch_buf_cfg *ife_scratch_config;
};

/**
 * struct cam_ife_hw_mgr_ctx_flags - IFE HW mgr ctx flags
 *
 * @ctx_in_use:               flag to tell whether context is active
 * @init_done:                indicate whether init hw is done
 * @is_fe_enabled:            indicate whether fetch engine\read path is enabled
 * @is_dual:                  indicate whether context is in dual VFE mode
 * @is_offline:               indicate whether context is for offline IFE
 * @dsp_enabled:              indicate whether dsp is enabled in this context
 * @internal_cdm:             indicate whether context uses internal CDM
 * @pf_mid_found:             in page fault, mid found for this ctx.
 * @need_csid_top_cfg:        Flag to indicate if CSID top cfg is needed.
 * @is_rdi_only_context:      flag to specify the context has only rdi resource
 * @is_lite_context:          flag to specify the context has only uses lite
 *                            resources
 * @is_rdi_and_stats_context: flag to specify the context has only uses lite with
 *                            stats and rdi resource
 * @is_sfe_shdr:              indicate if stream is for SFE sHDR
 * @is_sfe_fs:                indicate if stream is for inline SFE FS
 * @dump_on_flush:            Set if reg dump triggered on flush
 * @dump_on_error:            Set if reg dump triggered on error
 * @custom_aeb_mode:          Set if custom AEB stream
 * @rdi_lcr_en:               To indicate if RDI LCR is enabled
 * @sys_cache_usage:          Per context sys cache usage
 *                            The corresponding index will be set
 *                            for the cache type
 * @hybrid_acquire:           Bool for categorising acquire type.
 * @secure_mode:              Flag to check if any out resource is secure
 * @is_independent_crm_mode:  Flag to check if isp ctx is working in independent crm mode
 * @slave_metadata_en:        Flag to indicate if metadata is enabled in RDI path
 * @per_port_en               Indicates if per port feature is enabled or not
 * @is_trigger_type           Context type trigger
 */
struct cam_ife_hw_mgr_ctx_flags {
	bool   ctx_in_use;
	bool   init_done;
	bool   is_fe_enabled;
	bool   is_dual;
	bool   is_offline;
	bool   dsp_enabled;
	bool   internal_cdm;
	bool   pf_mid_found;
	bool   need_csid_top_cfg;
	bool   is_rdi_only_context;
	bool   is_lite_context;
	bool   is_rdi_and_stats_context;
	bool   is_sfe_shdr;
	bool   is_sfe_fs;
	bool   dump_on_flush;
	bool   dump_on_error;
	bool   is_aeb_mode;
	bool   rdi_lcr_en;
	bool   sys_cache_usage[CAM_LLCC_MAX];
	bool   hybrid_acquire;
	bool   secure_mode;
	bool   is_independent_crm_mode;
	bool   slave_metadata_en;
	bool   per_port_en;
	bool   is_trigger_type;
};

/**
 * struct cam_ife_cdm_user_data - IFE HW user data with CDM
 *
 * @prepare:                   hw_update_data
 * @request_id:                Request id
 * @support_cdm_cb_reg_dump:   true for per frame register dump trigger in ife cdm callback
 *                             false for per frame register dump trigger in ife config_hw workqueue
 */
struct cam_ife_cdm_user_data {
	struct cam_isp_prepare_hw_update_data    *hw_update_data;
	uint64_t                                  request_id;
	bool                                      support_cdm_cb_reg_dump;
};

/** struct cam_ife_virtual_rdi_mapping - mapping table between UMd and KMD RDI resources
 *
 * @rdi_path_count           : indicates how many rdi paths are acquired for this sensor
 * @virtual_rdi              : requested virtual RDI port by UMD
 * @acquired_rdi             : acquired RDI port by KMD
 */
struct cam_ife_virtual_rdi_mapping {
	uint32_t   rdi_path_count;
	uint32_t   virtual_rdi[CAM_ISP_STREAM_CFG_MAX];
	uint32_t   acquired_rdi[CAM_ISP_STREAM_CFG_MAX];
};


/**
 * struct cam_ife_hw_mgr_ctx - IFE HW manager Context object
 *
 * @list:                   used by the ctx list.
 * @common:                 common acquired context data
 * @ctx_index:              acquired context id.
 * @left_hw_idx:            hw index for master core [left]
 * @right_hw_idx:           hw index for slave core [right]
 * @hw_mgr:                 IFE hw mgr which owns this context
 * @res_list_csid:          CSID resource list
 * @res_list_ife_src:       IFE input resource list
 * @res_list_sfe_src        SFE input resource list
 * @res_list_ife_in_rd      IFE/SFE input resource list for read path
 * @res_list_ife_out:       IFE output resoruces array
 * @res_list_sfe_out:       SFE output resources array
 * @num_acq_vfe_out:        Number of acquired VFE out resources
 * @num_acq_sfe_out:        Number of acquired SFE out resources
 * @free_res_list:          Free resources list for the branch node
 * @res_pool:               memory storage for the free resource list
 * @irq_status0_mask:       irq_status0_mask for the context
 * @irq_status1_mask:       irq_status1_mask for the context
 * @base                    device base index array contain the all IFE HW
 *                          instance associated with this context.
 * @num_base                number of valid base data in the base array
 * @cdm_handle              cdm hw acquire handle
 * @cdm_ops                 cdm util operation pointer for building
 *                          cdm commands
 * @cdm_cmd                 cdm base and length request pointer
 * @cdm_id                  cdm id of the acquired cdm
 * @sof_cnt                 sof count value per core, used for dual VFE
 * @epoch_cnt               epoch count value per core, used for dual VFE
 * @eof_cnt                 eof count value per core, used for dual VFE
 * @overflow_pending        flat to specify the overflow is pending for the
 *                          context
 * @cdm_done                flag to indicate cdm has finished writing shadow
 *                          registers
 * @last_cdm_done_req:      Last cdm done request
 * @config_done_complete    indicator for configuration complete
 * @reg_dump_buf_desc:      cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:       Count of descriptors in reg_dump_buf_desc
 * @applied_req_id:         Last request id to be applied
 * @ctx_type                Type of IFE ctx [CUSTOM/SFE etc.]
 * @ctx_config              ife ctx config  [bit field]
 * @ts                      captured timestamp when the ctx is acquired
 * @hw_enabled              Array to indicate active HW
 * @buf_done_controller     Buf done controller.
 * @sfe_info                SFE info pertaining to this stream
 * @flags                   Flags pertainting to this ctx
 * @bw_config_version       BW Config version
 * @recovery_id:            Unique ID of the current valid scheduled recovery
 * @current_mup:            Current MUP val, scratch will then apply the same as previously
 *                          applied request
 * @curr_num_exp:           Current num of exposures
 * @num_in_ports:           number of in ports in acquire call
 * @in_ports:               array of inports of length num_in_ports
 * @acquire_type:           Acquire type, can be virtual, real or hybrid
 * @sensor_info:            sensor data for hybrid acquire
 * @sensor_id:              Sensor id for context
 * @num_processed:          number of config_dev processed in virtual acquire
 * @mapping_table:          mapping between virtual rdi and acquired rdi
 * @slave_status:           slave status indicating if it is in running state
 * @primary_rdi_out_res:    Indicates primary rdi resource
 */
struct cam_ife_hw_mgr_ctx {
	struct list_head                     list;
	struct cam_isp_hw_mgr_ctx            common;

	uint32_t                             ctx_index;
	uint32_t                             left_hw_idx;
	uint32_t                             right_hw_idx;
	struct cam_ife_hw_mgr               *hw_mgr;

	struct cam_isp_hw_mgr_res            res_list_ife_in;
	struct list_head                     res_list_ife_csid;
	struct list_head                     res_list_ife_vcsid;
	struct list_head                     res_list_ife_src;
	struct list_head                     res_list_vife_src;
	struct list_head                     res_list_sfe_src;
	struct list_head                     res_list_ife_in_rd;
	struct cam_isp_hw_mgr_res           *res_list_ife_out;
	struct cam_isp_hw_mgr_res            res_list_sfe_out[
					   	CAM_SFE_HW_OUT_RES_MAX];
	struct list_head                     free_res_list;
	struct cam_isp_hw_mgr_res            res_pool[CAM_IFE_HW_RES_POOL_MAX];
	uint32_t                             num_acq_vfe_out;
	uint32_t                             num_acq_sfe_out;

	uint32_t                             irq_status0_mask[CAM_IFE_HW_NUM_MAX];
	uint32_t                             irq_status1_mask[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_ctx_base_info         base[CAM_IFE_HW_NUM_MAX +
					   	CAM_SFE_HW_NUM_MAX];
	uint32_t                             num_base;
	uint32_t                             cdm_handle;
	struct cam_cdm_utils_ops            *cdm_ops;
	struct cam_cdm_bl_request           *cdm_cmd;
	enum cam_cdm_id                      cdm_id;
	uint32_t                             sof_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                             epoch_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                             eof_cnt[CAM_IFE_HW_NUM_MAX];
	atomic_t                             overflow_pending;
	atomic_t                             cdm_done;
	uint64_t                             last_cdm_done_req;
	struct completion                    config_done_complete;
	uint32_t                             hw_version;
	struct cam_cmd_buf_desc              reg_dump_buf_desc[
					   	CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                             num_reg_dump_buf;
	uint64_t                             applied_req_id;
	enum cam_ife_ctx_master_type         ctx_type;
	uint32_t                             ctx_config;
	struct timespec64                    ts;
	void                                *buf_done_controller;
	struct cam_ife_hw_mgr_sfe_info       sfe_info;
	struct cam_ife_hw_mgr_ctx_flags      flags;
	struct cam_ife_hw_mgr_ctx_pf_info    pf_info;
	struct cam_ife_cdm_user_data         cdm_userdata;
	uint32_t                             bw_config_version;
	atomic_t                             recovery_id;
	uint32_t                             current_mup;
	uint32_t                             curr_num_exp;
	uint32_t                             num_in_ports;
	struct cam_isp_in_port_generic_info *in_ports;
	uint32_t                             acquire_type;
	struct cam_ife_hybrid_sensor_data   *sensor_info;
	uint32_t                             sensor_id;
	uint32_t                             num_processed;
	struct cam_ife_virtual_rdi_mapping   mapping_table;
	bool                                 is_slave_down;
	uint32_t                             primary_rdi_out_res;
};

/**
 * struct cam_isp_bus_hw_caps - BUS capabilities
 *
 * @max_vfe_out_res_type  :  max ife out res type value from hw
 * @max_sfe_out_res_type  :  max sfe out res type value from hw
 * @support_consumed_addr :  indicate whether hw supports last consumed address
 * @fifo_depth            :  Max fifo depth supported
 */
struct cam_isp_bus_hw_caps {
	uint32_t     max_vfe_out_res_type;
	uint32_t     max_sfe_out_res_type;
	bool         support_consumed_addr;
	uint32_t     fifo_depth;
};

/*
 * struct cam_isp_sys_cache_info:
 *
 * @Brief:                   ISP Bus sys cache info
 *
 * @type:                    Cache type
 * @scid:                    Cache slice ID
 */
struct cam_isp_sys_cache_info {
	enum cam_sys_cache_config_types type;
	int32_t                         scid;
};

/**
 * struct cam_ife_hw_mgr - IFE HW Manager
 *
 * @mgr_common:            common data for all HW managers
 * @workq_pool:            Workq pool
 * @csid_devices;          csid device instances array. This will be filled by
 *                         HW manager during the initialization.
 * @ife_devices:           IFE device instances array. This will be filled by
 *                         HW layer during initialization
 * @sfe_devices:           SFE device instance array
 * @ctx_mutex:             mutex for the hw context pool
 * @free_ctx_list:         free hw context list
 * @used_ctx_list:         used hw context list
 * @ctx_pool:              context storage
 * @csid_hw_caps           csid hw capability stored per core
 * @ife_dev_caps           ife device capability per core
 * @worker                 worker for IFE hw manager
 * @debug_cfg              debug configuration
 * @isp_bus_caps           Capability of underlying SFE/IFE bus HW
 * @path_port_map          Mapping of outport to IFE mux
 * @phy_ref_cnt            Ref count of secure IFE contexts streaming on PHY
 * @sec_phy_ref_cnt        Ref count of IFE contexts streaming on PHY
 * @is_phy_secure          Flag to indicate if any context connected to PHY is secure
 * @csid_camif_irq_support CSID camif IRQ support
 * @hw_pid_support         hw pid support for this target
 * @csid_rup_en            Reg update at CSID side
 * @csid_global_reset_en   CSID global reset enable
 */
struct cam_ife_hw_mgr {
	struct cam_isp_hw_mgr          mgr_common;
	struct cam_req_mgr_core_worker *workq_pool[CAM_IFE_CTX_MAX];
	struct cam_hw_intf            *csid_devices[CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_isp_hw_intf_data   *ife_devices[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_hw_intf_data   *sfe_devices[CAM_SFE_HW_NUM_MAX];
	struct cam_soc_reg_map        *cdm_reg_map[CAM_IFE_HW_NUM_MAX];

	struct mutex                     ctx_mutex;
	atomic_t                         active_ctx_cnt;
	struct list_head                 free_ctx_list;
	struct list_head                 used_ctx_list;
	struct cam_ife_hw_mgr_ctx        ctx_pool[CAM_IFE_CTX_MAX];

	struct cam_ife_csid_hw_caps      csid_hw_caps[
						CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_vfe_hw_get_hw_cap     ife_dev_caps[CAM_IFE_HW_NUM_MAX];
	struct cam_req_mgr_core_worker   *worker;
	struct cam_ife_hw_mgr_debug      debug_cfg;
	struct cam_isp_bus_hw_caps       isp_bus_caps;
	struct cam_isp_hw_path_port_map  path_port_map;
	struct cam_isp_sys_cache_info    sys_cache_info[CAM_LLCC_MAX];
	uint32_t                         num_caches_found;
	uint32_t                         phy_ref_cnt[CAM_IFE_MAX_PHY_ID + 1];
	uint32_t                         sec_phy_ref_cnt[CAM_IFE_MAX_PHY_ID + 1];
	bool                             is_phy_secure[CAM_IFE_MAX_PHY_ID + 1];
	bool                             csid_camif_irq_support;
	bool                             hw_pid_support;
	bool                             csid_rup_en;
	bool                             csid_global_reset_en;
};

/**
 * struct cam_ife_hw_mgr_sensor_stream_config  -  camera sensor stream configurations
 *
 * @priv                        : Context data
 * @sensor_id                   : camera sensor unique index
 * @num_valid_vc_dt_pxl         : valid vc and dt for pxl path
 * @num_valid_vc_dt_rdi         : valid vc and dt in array for rdi path
 * @pxl_vc                      : input virtual channel number for pxl path
 * @pxl_dt                      : input data type number for pxl path
 * @ppp_vc                      : input virtual channel number for ppp path
 * @ppp_dt                      : input data type number for ppp path
 * @lcr_vc                      : input virtual channel number for lcr path
 * @lcr_dt                      : input data type number for lcr path
 * @rdi_vc                      : input virtual channel number for rdi path
 * @rdi_dt                      : input data type number for rdi path
 * @decode_format               : input data format
 * @rdi_vc_dt_updated           : Indicates count of rdi vc-dt associated to any hw res
 * @pxl_vc_dt_updated           : Indicates if pxl vc-dt is associated to any hw res
 * @lcr_vc_dt_updated           : Indicates if lcr vc-dt associated to any hw res
 * @ppp_vc_dt_updated           : Indicates if ppp vc-dt is associated to any hw res
 * @acquired                    : indicates whether acquire is done for this sensor id
 * @is_streamon                 : indicates whether streamon is done for this sensor id
 */
struct cam_ife_hw_mgr_sensor_stream_config {
	void                                      *priv;
	uint32_t                                   sensor_id;
	uint32_t                                   num_valid_vc_dt_pxl;
	uint32_t                                   num_valid_vc_dt_rdi;
	uint32_t                                   num_valid_vc_dt_ppp;
	uint32_t                                   num_valid_vc_dt_lcr;
	uint32_t                                   pxl_vc;
	uint32_t                                   pxl_dt;
	uint32_t                                   ppp_vc;
	uint32_t                                   ppp_dt;
	uint32_t                                   lcr_vc;
	uint32_t                                   lcr_dt;
	uint32_t                                   rdi_vc[CAM_ISP_VC_DT_CFG];
	uint32_t                                   rdi_dt[CAM_ISP_VC_DT_CFG];
	uint32_t                                   decode_format;
	uint32_t                                   rdi_vc_dt_updated;
	bool                                       pxl_vc_dt_updated;
	uint32_t                                   lcr_vc_dt_updated;
	bool                                       ppp_vc_dt_updated;
	bool                                       acquired;
	bool                                       is_streamon;
};

/**
 * struct cam_ife_hw_mgr_stream_grp_config  -  camera sensor stream group configurations
 *
 * @res_type                      : input resource type
 * @lane_type                     : lane type: c-phy or d-phy.
 * @lane_num                      : active lane number
 * @lane_cfg                      : lane configurations: 4 bits per lane
 * @feature_mask                  : feature flag
 * @acquire_cnt                   : count of number of acquire calls
 * @stream_cfg_cnt                : number of sensor configurations for pxl and rdi paths
 * @rdi_stream_cfg_cnt            : number of sensor configurations for only rdi path
 * @stream_on_cnt                 : count of number of streamon calls for this ife device
 * @res_ife_csid_list             : CSID resource list
 * @res_ife_src_list              : IFE input resource list
 * @res_list_ife_out              : IFE output resources array
 * @lock                          : mutex lock
 * @free_res_list                 : Free resources list for the branch node
 * @acquired_hw_idx               : Index of acquired HW
 * @res_pool                      : memory storage for the free resource list
 * @mapping_table                 : mapping table between UMd and KMD RDI resources
 * @stream_cfg                    : stream config data
 * @recovery_in_progress          : Indicates if ife is process of frame drop recovery
 */
struct cam_ife_hw_mgr_stream_grp_config {
	uint32_t                                      res_type;
	uint32_t                                      lane_type;
	uint32_t                                      lane_num;
	uint32_t                                      lane_cfg;
	uint32_t                                      feature_mask;
	uint32_t                                      acquire_cnt;
	uint32_t                                      stream_cfg_cnt;
	uint32_t                                      rdi_stream_cfg_cnt;
	int32_t                                       stream_on_cnt;
	struct list_head                              res_ife_csid_list;
	struct list_head                              res_ife_src_list;
	struct cam_isp_hw_mgr_res                    *res_list_ife_out;
	struct mutex                                  lock;
	struct list_head                              free_res_list;
	uint32_t                                      acquired_hw_idx;
	struct cam_isp_hw_mgr_res                     res_pool[CAM_IFE_HW_STREAM_GRP_RES_POOL_MAX];
	struct cam_ife_hw_mgr_sensor_stream_config    stream_cfg[CAM_ISP_STREAM_CFG_MAX];
	bool                                          recovery_in_progress;
};

/**
 * struct cam_ife_hw_mgr_sensor_grp_cfg  -  sensor group configurations
 *
 * @num_grp_cfg                 : count of total active group configs
 * @grp_cfg                     : stream group data
 */
struct cam_ife_hw_mgr_sensor_grp_cfg {
	uint32_t                                  num_grp_cfg;
	struct cam_ife_hw_mgr_stream_grp_config  *grp_cfg;
};

/**
 * struct cam_ife_hw_event_recovery_data - Payload for the recovery procedure
 *
 * @error_type:               Error type that causes the recovery
 * @affected_core:            Array of the hardware cores that are affected
 * @affected_ctx:             Array of the hardware contexts that are affected
 * @no_of_context:            Actual number of the affected context
 * @id:                       Unique ID associated with this recovery data (per HW context)
 *
 */
struct cam_ife_hw_event_recovery_data {
	uint32_t                   error_type;
	uint32_t                   affected_core[CAM_ISP_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx *affected_ctx[CAM_IFE_CTX_MAX];
	uint32_t                   no_of_context;
	uint32_t                   id[CAM_IFE_CTX_MAX];
};

/**
 * struct cam_ife_hw_mini_dump_ctx - Mini dump data
 *
 * @base:                   device base index array contain the all IFE HW
 * @pf_info:                Page Fault Info
 * @csid_md:                CSID mini dump data
 * @vfe_md:                 VFE mini dump data
 * @flags:                  Flags pertainting to this ctx
 * @ctx_priv:               Array of the hardware contexts that are affected
 * @last_cdm_done_req:      Last cdm done request
 * @applied_req_id:         Last request id to be applied
 * @cdm_handle:             cdm hw acquire handle
 * @ctx_index:              acquired context id.
 * @left_hw_idx:            hw index for master core [left]
 * @right_hw_idx:           hw index for slave core [right]
 * @num_base:               number of valid base data in the base array
 * @cdm_id:                 cdm id of the acquired cdm
 * @ctx_type:               Type of IFE ctx [CUSTOM/SFE etc.]
 * @overflow_pending:       flat to specify the overflow is pending for the
 * @cdm_done:               flag to indicate cdm has finished writing shadow
 *                          registers
 */
struct cam_ife_hw_mini_dump_ctx {
	struct cam_isp_ctx_base_info          base[CAM_IFE_HW_NUM_MAX +
				         	CAM_SFE_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx_pf_info     pf_info;
	void                                 *csid_md[CAM_IFE_HW_NUM_MAX];
	void                                 *vfe_md[CAM_IFE_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx_flags       flags;
	void                                 *ctx_priv;
	uint64_t                              last_cdm_done_req;
	uint64_t                              applied_req_id;
	uint32_t                              cdm_handle;
	uint8_t                               ctx_index;
	uint8_t                               left_hw_idx;
	uint8_t                               right_hw_idx;
	uint8_t                               num_base;
	enum cam_cdm_id                       cdm_id;
	enum cam_ife_ctx_master_type          ctx_type;
	bool                                  overflow_pending;
	bool                                  cdm_done;
};

/**
 * struct cam_ife_hw_mini_dump_data - Mini dump data
 *
 * @num_ctx:                  Number of context dumped
 * @ctx:                      Array of context
 *
 */
struct cam_ife_hw_mini_dump_data {
	uint32_t                            num_ctx;
	struct cam_ife_hw_mini_dump_ctx    *ctx[CAM_IFE_CTX_MAX];
};

/**
 * cam_ife_hw_mgr_populate_regs()
 *
 * @brief:              populate registers for real or virtual acquire
 *
 * @ctx:                ptr to ife_hw_mgr context
 * @acquire_type:       acquire type, can be virtual or hybrid
 * @pkt:                buffer to append register config
 * @off:                offset to append registers
 * @num:                number of register written
 *
 */
void cam_ife_hw_mgr_populate_regs(struct cam_ife_hw_mgr_ctx *ctx, int acquire_type,
		uint32_t *pkt, uint32_t *off, uint32_t *num);

/**
 * cam_ife_hw_mgr_populate_out_ports()
 *
 * @brief:              populate out_ports for real or virtual acquire
 *
 * @ctx:                ptr to ife_hw_mgr context
 * @acquire_type:       acquire type, can be virtual or hybrid
 * @pkt:                buffer to append register config
 * @off:                offset to append registers
 *
 */
void cam_ife_hw_mgr_populate_out_ports(struct cam_ife_hw_mgr_ctx *ctx,
		int acquire_type, uint32_t *pkt, uint32_t *off);

/**
 * cam_ife_hw_mgr_init()
 *
 * @brief:              Initialize the IFE hardware manger. This is the
 *                      etnry functinon for the IFE HW manager.
 *
 * @hw_mgr_intf:        IFE hardware manager object returned
 * @iommu_hdl:          Iommu handle to be returned
 *
 */
int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl);
void cam_ife_hw_mgr_deinit(void);

#endif /* _CAM_IFE_HW_MGR_H_ */

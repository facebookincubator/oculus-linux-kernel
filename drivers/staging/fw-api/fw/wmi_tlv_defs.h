/*
 * Copyright (c) 2010-2020 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
/*
 * wmi_tlv_defs_expanded.h file can be generated using macro preprocessor output of gcc for ease of debugging and reference by developers
 * Following command has to be run from "fw_common\fw_interface\include" directory to create wmi_tlv_defs_expanded.h file
 * gcc -E wmi_tlv_defs.h > wmi_tlv_defs_expanded.h on Linux
 * and
 * gcc -E wmi_tlv_defs.h -o wmi_tlv_defs_expanded.h on Linux/Windows
 *
 */
#ifndef _WMI_TLV_DEFS_H_
#define _WMI_TLV_DEFS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WMITLV_FIELD_BUF_IS_ALLOCATED(elem_name) \
       is_allocated_##elem_name

#define WMITLV_FIELD_NUM_OF(elem_name) \
       num_##elem_name

/* Define the structure typedef for the TLV parameters of each cmd/event */
#define WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id) \
       wmi_cmd_event_id##_param_tlvs

/*
 * The following macro WMITLV_OP_* are created by the macro WMITLV_ELEM().
 */
/* macro to define the TLV name in the correct order. When (op==TAG_ORDER) */
#define WMITLV_OP_TAG_ORDER_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_tlv_order_##elem_name,

/* macro to define the TLV name with the TLV Tag value. When (op==TAG_ID) */
#define WMITLV_OP_TAG_ID_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_tlv_tag_##elem_name = elem_tlv_tag,

/* macro to define the TLV name with the TLV structure size. May not be accurate when variable length. When (op==TAG_SIZEOF) */
#define WMITLV_OP_TAG_SIZEOF_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_sizeof_##elem_name = sizeof(elem_struc_type),

/* macro to define the TLV name with value indicating whether the TLV is variable length. When (op==TAG_VAR_SIZED) */
#define WMITLV_OP_TAG_VAR_SIZED_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_var_sized_##elem_name = var_len,

/* macro to define the TLV name with value indicating the fixed array size. When (op==TAG_ARR_SIZE) */
#define WMITLV_OP_TAG_ARR_SIZE_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_arr_size_##elem_name = arr_size,

/*
 * macro to define afew fields associated to a TLV. For example, a structure pointer with the TLV name.
 * This macro is expand from WMITLV_ELEM(op) when (op==STRUCT_FIELD).
 * NOTE: If this macro is changed, then "mirror" structure wmitlv_cmd_param_info
 * should be updated too.
 */
#define WMITLV_OP_STRUCT_FIELD_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      elem_struc_type *elem_name; \
      A_UINT32 WMITLV_FIELD_NUM_OF(elem_name); \
      A_UINT32 WMITLV_FIELD_BUF_IS_ALLOCATED(elem_name);

/*
 * A "mirror" structure that contains the fields that is created by the
 * macro WMITLV_OP_STRUCT_FIELD_macro.
 * NOTE: you should modify this structure and WMITLV_OP_STRUCT_FIELD_macro
 * so that they both has the same kind of fields.
 */
typedef struct {
    void *tlv_ptr;            /* Pointer to the TLV Buffer. But the "real" one will have the right type instead of void. */
    A_UINT32 num_elements;    /* Number of elements. For non-array, this is one. For array, this is the number of elements. */
    A_UINT32 buf_is_allocated;/* Boolean flag to indicate that a new buffer is allocated for this TLV. */
} wmitlv_cmd_param_info;

/*
 * NOTE TRICKY MACRO:
 *  WMITLV_ELEM is re-defined to a "op" specific macro.
 *  Eg. WMITLV_OP_TAG_ORDER_macro is created for the op_type=TAG_ORDER.
 */
#define WMITLV_ELEM(wmi_cmd_event_id, op_type, param_ptr, param_len, elem_tlv_tag, elem_struc_type, elem_name, var_len) \
    WMITLV_OP_##op_type##_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, WMITLV_ARR_SIZE_INVALID)
/*
 *  WMITLV_FXAR (FiX ARray) is similar to WMITLV_ELEM except it has an extra parameter for the fixed number of elements.
 *  It is re-defined to a "op" specific macro.
 *  Eg. WMITLV_OP_TAG_ORDER_macro is created for the op_type=TAG_ORDER.
 */
#define WMITLV_FXAR(wmi_cmd_event_id, op_type, param_ptr, param_len, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size) \
    WMITLV_OP_##op_type##_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)

#define WMITLV_TABLE(id,op,buf,len) WMITLV_TABLE_##id(id,op,buf,len)

/*
 * This macro will create various enumerations and structures to describe the TLVs for
 * the given Command/Event ID.
 *
 * For example, the following is for WMI_SERVICE_READY_EVENTID:
 *    #define WMITLV_TABLE_WMI_SERVICE_READY_EVENTID(id,op,buf,len)                                                                                                 \
 *       WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param, wmi_service_ready_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)     \
 *       WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES, HAL_REG_CAPABILITIES, hal_reg_capabilities, WMITLV_SIZE_FIX)  \
 *       WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wmi_service_bitmap, WMITLV_SIZE_FIX, WMI_SERVICE_BM_SIZE) \
 *       WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_host_mem_req, mem_reqs, WMITLV_SIZE_VAR)
 *    WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EVENTID);
 * This macro will create the following text:
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_tlv_order_wmi_service_ready_event_fixed_param,
 *    WMI_SERVICE_READY_EVENTID_tlv_order_hal_reg_capabilities,
 *    WMI_SERVICE_READY_EVENTID_tlv_order_wmi_service_bitmap,
 *    WMI_SERVICE_READY_EVENTID_tlv_order_mem_reqs,
 *    WMI_TLV_HLPR_NUM_TLVS_FOR_WMI_SERVICE_READY_EVENTID
 * } WMI_SERVICE_READY_EVENTID_TAG_ID_enum_type;
 * //NOTE: WMI_TLV_HLPR_NUM_TLVS_FOR_WMI_SERVICE_READY_EVENTID is the number of TLVs.
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_wmi_service_ready_event_fixed_param = WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param,
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_hal_reg_capabilities = WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES,
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_wmi_service_bitmap = WMITLV_TAG_ARRAY_UINT32,
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_mem_reqs = WMITLV_TAG_ARRAY_STRUC,
 * } WMI_SERVICE_READY_EVENTID_TAG_ORDER_enum_type;
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_sizeof_wmi_service_ready_event_fixed_param = sizeof(wmi_service_ready_event_fixed_param),
 *    WMI_SERVICE_READY_EVENTID_sizeof_hal_reg_capabilities = sizeof(HAL_REG_CAPABILITIES),
 *    WMI_SERVICE_READY_EVENTID_sizeof_wmi_service_bitmap = sizeof(A_UINT32),
 *    WMI_SERVICE_READY_EVENTID_sizeof_mem_reqs = sizeof(wlan_host_mem_req),
 * } WMI_SERVICE_READY_EVENTID_TAG_SIZEOF_enum_type;
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_var_sized_wmi_service_ready_event_fixed_param = WMITLV_SIZE_FIX,
 *    WMI_SERVICE_READY_EVENTID_var_sized_hal_reg_capabilities = WMITLV_SIZE_FIX,
 *    WMI_SERVICE_READY_EVENTID_var_sized_wmi_service_bitmap = WMITLV_SIZE_VAR,
 *    WMI_SERVICE_READY_EVENTID_var_sized_mem_reqs = WMITLV_SIZE_VAR,
 * } WMI_SERVICE_READY_EVENTID_TAG_VAR_SIZED_enum_type;
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_arr_size_wmi_service_ready_event_fixed_param = WMITLV_ARR_SIZE_INVALID,
 *    WMI_SERVICE_READY_EVENTID_arr_size_hal_reg_capabilities = WMITLV_ARR_SIZE_INVALID,
 *    WMI_SERVICE_READY_EVENTID_arr_size_wmi_service_bitmap = WMI_SERVICE_BM_SIZE,
 *    WMI_SERVICE_READY_EVENTID_arr_size_mem_reqs = WMITLV_ARR_SIZE_INVALID,
 * } WMI_SERVICE_READY_EVENTID_TAG_ARR_SIZE_enum_type;
 *
 * typedef struct {
 *    wmi_service_ready_event_fixed_param *fixed_param;
 *    A_UINT32 num_fixed_param;
 *    A_UINT32 is_allocated_fixed_param;
 *    HAL_REG_CAPABILITIES *hal_reg_capabilities;
 *    A_UINT32 num_hal_reg_capabilities;
 *    A_UINT32 is_allocated_hal_reg_capabilities;
 *    A_UINT32 *wmi_service_bitmap;
 *    A_UINT32 num_wmi_service_bitmap;
 *    A_UINT32 is_allocated_wmi_service_bitmap;
 *    wlan_host_mem_req *mem_reqs;
 *    A_UINT32 num_mem_reqs;
 *    A_UINT32 is_allocated_mem_reqs;
 *
 * } WMI_SERVICE_READY_EVENTID_param_tlvs;
 *
 */

#define WMITLV_CREATE_PARAM_STRUC(wmi_cmd_event_id)            \
    typedef enum {                                             \
        WMITLV_TABLE(wmi_cmd_event_id, TAG_ORDER, NULL, 0)     \
        WMI_TLV_HLPR_NUM_TLVS_FOR_##wmi_cmd_event_id           \
    } wmi_cmd_event_id##_TAG_ORDER_enum_type;                  \
                                                               \
    typedef struct {                                           \
        WMITLV_TABLE(wmi_cmd_event_id, STRUCT_FIELD, NULL, 0)  \
    } WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id);     \

/** Enum list of TLV Tags for each parameter structure type. */
typedef enum {
    /* 0 to 15 is reserved */
    WMITLV_TAG_LAST_RESERVED = 15,
    WMITLV_TAG_FIRST_ARRAY_ENUM, /* First entry of ARRAY type tags */
    WMITLV_TAG_ARRAY_UINT32 = WMITLV_TAG_FIRST_ARRAY_ENUM,
    WMITLV_TAG_ARRAY_BYTE,
    WMITLV_TAG_ARRAY_STRUC,
    WMITLV_TAG_ARRAY_FIXED_STRUC,
    WMITLV_TAG_ARRAY_INT16,
    WMITLV_TAG_LAST_ARRAY_ENUM = 31,   /* Last entry of ARRAY type tags */
    WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param,
    WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES,
    WMITLV_TAG_STRUC_wlan_host_mem_req,
    WMITLV_TAG_STRUC_wmi_ready_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_tpc_config_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_comb_phyerr_rx_hdr,
    WMITLV_TAG_STRUC_wmi_vdev_start_response_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_stopped_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_install_key_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_sta_kickout_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_rx_hdr,
    WMITLV_TAG_STRUC_wmi_tbtt_offset_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_delba_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_addba_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_event_fixed_param,
    WMITLV_TAG_STRUC_WOW_EVENT_INFO_fixed_param,
    WMITLV_TAG_STRUC_WOW_EVENT_INFO_SECTION_BITMAP,
    WMITLV_TAG_STRUC_wmi_rtt_event_header,
    WMITLV_TAG_STRUC_wmi_rtt_error_report_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rtt_meas_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_echo_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ftm_intg_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_gpio_input_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_csa_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param,
    WMITLV_TAG_STRUC_wmi_igtk_info,
    WMITLV_TAG_STRUC_wmi_dcs_interference_event_fixed_param,
    WMITLV_TAG_STRUC_ath_dcs_cw_int, /* DEPRECATED */
    WMITLV_TAG_STRUC_wlan_dcs_cw_int = /* alias */
        WMITLV_TAG_STRUC_ath_dcs_cw_int,
    WMITLV_TAG_STRUC_ath_dcs_wlan_int_stat, /* DEPRECATED */
    WMITLV_TAG_STRUC_wlan_dcs_im_tgt_stats_t = /* alias */
        WMITLV_TAG_STRUC_ath_dcs_wlan_int_stat,
    WMITLV_TAG_STRUC_wmi_wlan_profile_ctx_t,
    WMITLV_TAG_STRUC_wmi_wlan_profile_t,
    WMITLV_TAG_STRUC_wmi_pdev_qvit_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_swba_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tim_info,
    WMITLV_TAG_STRUC_wmi_p2p_noa_info,
    WMITLV_TAG_STRUC_wmi_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_avoid_freq_ranges_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_avoid_freq_range_desc,
    WMITLV_TAG_STRUC_wmi_gtk_rekey_fail_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_init_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_resource_config,
    WMITLV_TAG_STRUC_wlan_host_memory_chunk,
    WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_stop_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_chan_list_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_channel,
    WMITLV_TAG_STRUC_wmi_pdev_set_regdomain_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_wmm_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wmm_params,
    WMITLV_TAG_STRUC_wmi_pdev_set_quiet_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_create_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_delete_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_noa_descriptor,
    WMITLV_TAG_STRUC_wmi_p2p_go_set_beacon_ie_fixed_param,
    WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_up_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_down_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_install_key_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_create_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_delete_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_flush_tids_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_assoc_complete_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vht_rate_set,
    WMITLV_TAG_STRUC_wmi_bcn_tmpl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_prb_tmpl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bcn_prb_info,
    WMITLV_TAG_STRUC_wmi_peer_tid_addba_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_tid_delba_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_powersave_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_powersave_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_dtim_ps_method_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_mode_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_rssi_threshold_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_period_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_rssi_change_threshold_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_suspend_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_resume_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_add_bcn_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmv_bcn_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_hostwakeup_from_sleep_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_param,
    WMITLV_TAG_STRUC_WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_ARP_OFFLOAD_TUPLE,
    WMITLV_TAG_STRUC_WMI_NS_OFFLOAD_TUPLE,
    WMITLV_TAG_STRUC_wmi_ftm_intg_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_STA_KEEPALIVE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_STA_KEEPALVE_ARP_RESPONSE,
    WMITLV_TAG_STRUC_wmi_p2p_set_vendor_ie_data_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_peer_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_rate_retry_sched_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_trigger_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_set_hist_intvl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_get_prof_data_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_enable_profile_id_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_DEL_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_ADD_DEL_EVT_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_rtt_measreq_head,
    WMITLV_TAG_STRUC_wmi_rtt_measreq_body,
    WMITLV_TAG_STRUC_wmi_rtt_tsf_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_spectral_configure_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_spectral_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_nlo_configured_parameters,
    WMITLV_TAG_STRUC_wmi_csa_offload_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_csa_offload_chanswitch_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_set_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_echo_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_keepalive_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_FORCE_FW_HANG_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_gpio_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_gpio_output_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_add_wds_entry_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_remove_wds_entry_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bcn_tx_hdr,
    WMITLV_TAG_STRUC_wmi_bcn_send_from_host_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_hdr,
    WMITLV_TAG_STRUC_wmi_addba_clear_resp_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_addba_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_delba_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_addba_setresponse_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_send_singleamsdu_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_pktlog_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_pktlog_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_ht_ie_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_vht_ie_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_dscp_tid_map_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_green_ap_ps_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_tpc_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_base_macaddr_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_mcast_group_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_ap_profile_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_profile,
    WMITLV_TAG_STRUC_wmi_scan_sch_priority_table_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_ADD_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WOW_BITMAP_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_IPV4_SYNC_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_IPV6_SYNC_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_MAGIC_PATTERN_CMD,
    WMITLV_TAG_STRUC_WMI_scan_update_request_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_pkt_coalescing_filter,
    WMITLV_TAG_STRUC_wmi_chatter_coalescing_add_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_coalescing_delete_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_coalescing_query_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_txbf_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_debug_log_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nlo_event,
    WMITLV_TAG_STRUC_wmi_chatter_query_reply_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_upload_h_hdr,
    WMITLV_TAG_STRUC_wmi_capture_h_event_hdr,
    WMITLV_TAG_STRUC_WMI_VDEV_WNM_SLEEPMODE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_wmm_addts_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_wmm_delts_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_wmm_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_set_state_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_peer_update_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_peer_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_peer_capabilities,
    WMITLV_TAG_STRUC_wmi_vdev_mcc_set_tbtt_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_chan_list_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_mcc_bcn_intvl_change_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_resmgr_set_chan_time_quota_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_resmgr_set_chan_latency_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ba_req_ssn_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ba_rsp_ssn_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_set_oppps_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_set_noa_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ba_req_ssn_cmd_sub_struct_param,
    WMITLV_TAG_STRUC_wmi_ba_req_ssn_event_sub_struct_param,
    WMITLV_TAG_STRUC_wmi_sta_smps_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_gtx_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mcc_sched_traffic_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mcc_sched_sta_traffic_stats,
    WMITLV_TAG_STRUC_wmi_offload_bcn_tx_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_noa_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_tcp_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_tcp_pkt_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_udp_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_udp_pkt_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_ind_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_pause_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rfkill_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dfs_radar_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_ena_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_dis_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_result_scan_list,
    WMITLV_TAG_STRUC_wmi_batch_scan_result_network_info,
    WMITLV_TAG_STRUC_wmi_batch_scan_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_trigger_result_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_enabled_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_result_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_plmreq_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_plmreq_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_thermal_mgmt_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_thermal_mgmt_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_info_req_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_info,
    WMITLV_TAG_STRUC_wmi_peer_tx_fail_cnt_thr_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_set_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_set_action_period_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mhf_offload_set_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mhf_offload_plumb_routing_table_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_cmd_param,
    WMITLV_TAG_STRUC_wmi_nan_event_hdr,
    WMITLV_TAG_STRUC_wmi_pdev_l1ss_track_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_diag_data_container_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_modem_power_state_cmd_param,
    WMITLV_TAG_STRUC_wmi_peer_get_estimated_linkspeed_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_estimated_linkspeed_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_aggr_state_trig_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_mhf_offload_routing_table_entry,
    WMITLV_TAG_STRUC_wmi_roam_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_req_stats_ext_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_stats_ext_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_obss_scan_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_obss_scan_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_offload_prb_rsp_tx_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_led_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_auto_shutdown_cfg_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_auto_shutdown_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_update_whal_mib_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_avoid_update_cmd_param,
    WMITLV_TAG_STRUC_WOW_IOAC_PKT_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_IOAC_TMR_PATTERN_T,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_KEEPALIVE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_KEEPALIVE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_KEEPALIVE_T,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_start_link_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_clear_link_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_link_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_iface_link_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_radio_link_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_channel_stats,
    WMITLV_TAG_STRUC_wmi_radio_link_stats,
    WMITLV_TAG_STRUC_wmi_rate_stats,
    WMITLV_TAG_STRUC_wmi_peer_link_stats,
    WMITLV_TAG_STRUC_wmi_wmm_ac_stats,
    WMITLV_TAG_STRUC_wmi_iface_link_stats,
    WMITLV_TAG_STRUC_wmi_lpi_mgmt_snooping_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_start_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_stop_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_result_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_state_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_bucket_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_bucket_channel_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_bssid_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_get_cached_results_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_get_wlan_change_results_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_set_capabilities_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_get_capabilities_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_descriptor_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_rssi_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_result_bssid_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_cache_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_monitor_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_hotlist_monitor_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_d0_wow_enable_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_d0_wow_disable_ack_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_11i_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_11r_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_ese_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_synch_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_synch_complete_fixed_param,
    WMITLV_TAG_STRUC_wmi_extwow_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extwow_set_app_type1_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extwow_set_app_type2_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_handoff_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_rate_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_rate_ht_info,
    WMITLV_TAG_STRUC_wmi_ric_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_temperature_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_temperature_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_dhcp_server_offload_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tpc_chainmask_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ric_tspec,
    WMITLV_TAG_STRUC_wmi_tpc_chainmask_config,
    WMITLV_TAG_STRUCT_wmi_ipa_offload_enable_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_prob_req_oui_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_key_material,
    WMITLV_TAG_STRUC_wmi_tdls_set_offchan_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_led_flashing_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_offload_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_set_fqdn_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_set_resp_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_get_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_invoke_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_resume_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_antenna_diversity_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_ofl_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_ofl_add_sta_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_ofl_del_sta_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_apfind_cmd_param,
    WMITLV_TAG_STRUC_wmi_apfind_event_hdr,
    WMITLV_TAG_STRUC_wmi_ocb_set_sched_cmd_fixed_param,     // DEPRECATED
    WMITLV_TAG_STRUC_wmi_ocb_set_sched_event_fixed_param,   // DEPRECATED
    WMITLV_TAG_STRUC_wmi_ocb_set_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_set_config_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_set_utc_time_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_start_timing_advert_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_stop_timing_advert_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_get_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_channel_stats_request,
    WMITLV_TAG_STRUC_wmi_dcc_get_stats_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_clear_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_update_ndl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_update_ndl_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_channel,
    WMITLV_TAG_STRUC_wmi_ocb_schedule_element,
    WMITLV_TAG_STRUC_wmi_dcc_ndl_stats_per_channel,
    WMITLV_TAG_STRUC_wmi_dcc_ndl_chan,
    WMITLV_TAG_STRUC_wmi_qos_parameter,
    WMITLV_TAG_STRUC_wmi_dcc_ndl_active_state_config,
    WMITLV_TAG_STRUC_wmi_roam_scan_extended_threshold_param,
    WMITLV_TAG_STRUC_wmi_roam_filter_fixed_param,
    WMITLV_TAG_STRUC_wmi_passpoint_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_passpoint_event_hdr,
    WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_hotlist_ssid_match_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_tsf_tstamp_action_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_tsf_report_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_fw_mem_dump_fixed_param,
    WMITLV_TAG_STRUC_wmi_update_fw_mem_dump_fixed_param,
    WMITLV_TAG_STRUC_wmi_fw_mem_dump_params,
    WMITLV_TAG_STRUC_wmi_debug_mesg_flush_fixed_param,
    WMITLV_TAG_STRUC_wmi_debug_mesg_flush_complete_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_set_rate_report_condition_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_subnet_change_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_ie_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rssi_breach_monitor_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_rssi_breach_event_fixed_param,
    WMITLV_TAG_STRUC_WOW_EVENT_INITIAL_WAKEUP_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_pcl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_response_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_hw_mode_transition_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_txrx_streams,
    WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_response_vdev_mac_entry,
    WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_response_event_fixed_param,
    WMITLV_TAG_STRUC_WOW_IOAC_SOCK_PATTERN_T,
    WMITLV_TAG_STRUC_wmi_wow_enable_icmpv6_na_flt_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_diag_event_log_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_diag_event_log_supported_event_fixed_params,
    WMITLV_TAG_STRUC_wmi_packet_filter_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_packet_filter_enable_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_set_blacklist_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_compl_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_antenna_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_UDP_SVC_OFLD_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_lro_info_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_earlystop_rssi_thres_param,
    WMITLV_TAG_STRUC_wmi_service_ready_ext_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_mawc_sensor_report_ind_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mawc_enable_sensor_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_configure_mawc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nlo_configure_mawc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_configure_mawc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_assoc_conf_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_hostwakeup_gpio_pin_pattern_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_egap_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_egap_info_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_PMF_OFFLOAD_SET_SA_QUERY_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_scpc_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_egap_info_chainmask_list,
    WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_get_capability_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_capability_info_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_get_vdev_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_vdev_stats_info_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_set_vdev_instructions_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_del_vdev_instructions_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_delete_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_delete_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_dense_thres_param,
    WMITLV_TAG_STRUC_enlo_candidate_score_param,
    WMITLV_TAG_STRUC_wmi_peer_update_wds_entry_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_config_ratemask_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_fips_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_smart_ant_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_smart_ant_set_rx_antenna_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_tx_antenna_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_train_antenna_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_node_config_ops_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_ant_switch_tbl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_ctl_table_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_mimogain_table_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_fwtest_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_atf_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_atf_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_ani_cck_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_ani_ofdm_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_inst_rssi_stats_resp_fixed_param,
    WMITLV_TAG_STRUC_wmi_med_util_report_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_sta_ps_statechange_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_wds_addr_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_ratecode_list_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_nfcal_power_all_channels_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_tpc_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ani_ofdm_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ani_cck_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_channel_hopping_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_fips_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_atf_peer_info,
    WMITLV_TAG_STRUC_wmi_pdev_get_tpc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_filter_nrp_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_QBOOST_CFG_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_smart_ant_gpio_handle,
    WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_tx_antenna_series,
    WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_train_antenna_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_ant_ctrl_chain,
    WMITLV_TAG_STRUC_wmi_peer_cck_ofdm_rate_info,
    WMITLV_TAG_STRUC_wmi_peer_mcs_rate_info,
    WMITLV_TAG_STRUC_wmi_pdev_nfcal_power_all_channels_nfdBr,
    WMITLV_TAG_STRUC_wmi_pdev_nfcal_power_all_channels_nfdBm,
    WMITLV_TAG_STRUC_wmi_pdev_nfcal_power_all_channels_freqNum,
    WMITLV_TAG_STRUC_wmi_mu_report_total_mu,
    WMITLV_TAG_STRUC_wmi_vdev_set_dscp_tid_map_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_set_mbo_fixed_param,
    WMITLV_TAG_STRUC_wmi_mib_stats_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_disc_iface_created_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_disc_iface_deleted_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_started_cluster_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_joined_cluster_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndi_get_cap_req_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_initiator_req_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_responder_req_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_end_req_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndi_cap_rsp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_initiator_rsp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_responder_rsp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_end_rsp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_indication_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_confirm_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_end_indication_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_quiet_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_pcl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_hw_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_mac_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_antenna_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_hw_mode_response_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_hw_mode_transition_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_hw_mode_response_vdev_mac_entry,
    WMITLV_TAG_STRUC_wmi_pdev_set_mac_config_response_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_COEX_CONFIG_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_config_enhanced_mcast_filter_fixed_param,
    WMITLV_TAG_STRUC_WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_periodic_channel_stats_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_custom_aggr_size_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_wal_power_debug_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_MAC_PHY_CAPABILITIES,
    WMITLV_TAG_STRUC_WMI_HW_MODE_CAPABILITIES,
    WMITLV_TAG_STRUC_WMI_SOC_MAC_PHY_HW_MODE_CAPS,
    WMITLV_TAG_STRUC_WMI_HAL_REG_CAPABILITIES_EXT,
    WMITLV_TAG_STRUC_WMI_SOC_HAL_REG_CAPABILITIES,
    WMITLV_TAG_STRUC_wmi_vdev_wisa_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_power_level_stats_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_adaptive_dwell_parameters_tlv,
    WMITLV_TAG_STRUC_wmi_scan_adaptive_dwell_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_set_action_wake_up_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ndp_end_rsp_per_ndi,
    WMITLV_TAG_STRUC_wmi_peer_bwf_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_bwf_peer_info,
    WMITLV_TAG_STRUC_wmi_dbglog_time_stamp_sync_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_set_leader_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_manual_leader_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_per_chain_rssi_stats,
    WMITLV_TAG_STRUC_wmi_rssi_stats,
    WMITLV_TAG_STRUC_wmi_p2p_lo_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_lo_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_lo_stopped_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_reorder_queue_setup_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_reorder_queue_remove_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_multiple_mcast_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_compl_bundle_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_read_data_from_flash_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_read_data_from_flash_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_reorder_timeout_val_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_set_rx_blocksize_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_PDEV_SET_WAKEUP_CONFIG_CMDID_fixed_param,
    WMITLV_TAG_STRUC_wmi_tlv_buf_len_param,
    WMITLV_TAG_STRUC_wmi_service_available_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_antdiv_info_req_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_antdiv_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_antdiv_info,
    WMITLV_TAG_STRUC_wmi_pdev_get_antdiv_status_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_antdiv_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_mnt_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_chip_power_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_chip_power_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_coex_get_antenna_isolation_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_coex_report_isolation_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_cca_stats,
    WMITLV_TAG_STRUC_wmi_peer_signal_stats,
    WMITLV_TAG_STRUC_wmi_tx_stats,
    WMITLV_TAG_STRUC_wmi_peer_ac_tx_stats,
    WMITLV_TAG_STRUC_wmi_rx_stats,
    WMITLV_TAG_STRUC_wmi_peer_ac_rx_stats,
    WMITLV_TAG_STRUC_wmi_report_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_cca_stats_thresh,
    WMITLV_TAG_STRUC_wmi_peer_signal_stats_thresh,
    WMITLV_TAG_STRUC_wmi_tx_stats_thresh,
    WMITLV_TAG_STRUC_wmi_rx_stats_thresh,
    WMITLV_TAG_STRUC_wmi_pdev_set_stats_threshold_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_wlan_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rx_aggr_failure_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rx_aggr_failure_info,
    WMITLV_TAG_STRUC_wmi_vdev_encrypt_decrypt_data_req_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_encrypt_decrypt_data_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_band_to_mac,
    WMITLV_TAG_STRUC_wmi_tbtt_offset_info,
    WMITLV_TAG_STRUC_wmi_tbtt_offset_ext_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sar_limits_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sar_limit_cmd_row,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_phyerr_offload_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_phyerr_offload_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_adfs_ch_cfg_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_adfs_ocac_abort_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_radar_detection_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_adfs_ocac_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_dfs_cac_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vendor_oui,
    WMITLV_TAG_STRUC_wmi_request_rcpi_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_update_rcpi_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_peer_stats_info_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_stats_info,
    WMITLV_TAG_STRUC_wmi_peer_stats_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pkgid_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_connected_nlo_rssi_params,
    WMITLV_TAG_STRUC_wmi_set_current_country_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_regulatory_rule_struct,
    WMITLV_TAG_STRUC_wmi_reg_chan_list_cc_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_11d_scan_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_11d_scan_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_11d_new_country_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_radio_chan_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_radio_chan_stats,
    WMITLV_TAG_STRUC_wmi_radio_chan_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_per_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_add_mac_addr_to_rx_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_add_mac_addr_to_rx_filter_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_set_vdev_active_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hw_data_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_connected_nlo_bss_band_rssi_pref,
    WMITLV_TAG_STRUC_wmi_peer_oper_mode_change_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chip_power_save_failure_detected_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_csa_switch_count_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_update_pkt_routing_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_check_cal_version_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_check_cal_version_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_diversity_gain_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_MAC_PHY_CHAINMASK_COMBO,
    WMITLV_TAG_STRUC_WMI_MAC_PHY_CHAINMASK_CAPABILITY,
    WMITLV_TAG_STRUC_wmi_vdev_set_arp_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_arp_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_arp_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_iface_offload_stats,
    WMITLV_TAG_STRUC_wmi_request_stats_cmd_sub_struc_param,
    WMITLV_TAG_STRUC_rssi_ctl_ext,
    WMITLV_TAG_STRUC_wmi_single_phyerr_ext_rx_hdr,
    WMITLV_TAG_STRUC_wmi_coex_bt_activity_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_tx_power_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_tx_power_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_offchan_data_tx_compl_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_offchan_data_tx_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_send_params,
    WMITLV_TAG_STRUC_wmi_he_rate_set,
    WMITLV_TAG_STRUC_wmi_congestion_stats,
    WMITLV_TAG_STRUC_wmi_set_init_country_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_dbs_duty_cycle_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_dbs_duty_cycle_param_tlv,
    WMITLV_TAG_STRUC_wmi_pdev_div_get_rssi_antid_fixed_param,
    WMITLV_TAG_STRUC_wmi_therm_throt_config_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_therm_throt_level_config_info,
    WMITLV_TAG_STRUC_wmi_therm_throt_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_therm_throt_level_stats_info,
    WMITLV_TAG_STRUC_wmi_pdev_div_rssi_antid_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_OEM_DMA_RING_CAPABILITIES,
    WMITLV_TAG_STRUC_wmi_oem_dma_ring_cfg_req_fixed_param,
    WMITLV_TAG_STRUC_wmi_oem_dma_ring_cfg_rsp_fixed_param,
    WMITLV_TAG_STRUC_wmi_oem_indirect_data,
    WMITLV_TAG_STRUC_wmi_oem_dma_buf_release_fixed_param,
    WMITLV_TAG_STRUC_wmi_oem_dma_buf_release_entry,
    WMITLV_TAG_STRUC_wmi_pdev_bss_chan_info_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_bss_chan_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_lca_disallow_config_tlv_param,
    WMITLV_TAG_STRUC_wmi_vdev_limit_offchan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_rssi_rejection_oce_config_param,
    WMITLV_TAG_STRUC_wmi_roam_rejection_list_config_param = /* alias */
        WMITLV_TAG_STRUC_wmi_roam_rssi_rejection_oce_config_param,
    WMITLV_TAG_STRUC_wmi_unit_test_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_fils_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_pdev_update_pmk_cache_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pmk_cache,
    WMITLV_TAG_STRUC_wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_fils_synch_tlv_param,
    WMITLV_TAG_STRUC_wmi_gtk_offload_extended_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_bg_scan_roaming_param,
    WMITLV_TAG_STRUC_wmi_oic_ping_offload_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_oic_ping_offload_set_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_oic_ping_handoff_event,
    WMITLV_TAG_STRUC_wmi_dhcp_lease_renew_offload_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dhcp_lease_renew_event,
    WMITLV_TAG_STRUC_wmi_btm_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_debug_mesg_fw_data_stall_param,
    WMITLV_TAG_STRUC_wmi_wlm_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_update_ctltable_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_update_ctltable_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_cnd_scoring_param,
    WMITLV_TAG_STRUC_wmi_pdev_config_vendor_oui_action_fixed_param,
    WMITLV_TAG_STRUC_wmi_vendor_oui_ext,
    WMITLV_TAG_STRUC_wmi_roam_synch_frame_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_fd_send_from_host_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_enable_fils_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_swfda_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_bcn_offload_ctrl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_ac_tx_queue_optimized_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_stats_period,
    WMITLV_TAG_STRUC_wmi_ndl_schedule_update_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_tid_msduq_qdepth_thresh_update_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_msduq_qdepth_thresh_update,
    WMITLV_TAG_STRUC_wmi_pdev_set_rx_filter_promiscuous_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sar2_result_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_SAR_CAPABILITIES,
    WMITLV_TAG_STRUC_wmi_sap_obss_detection_cfg_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_obss_detection_info_evt_fixed_param,
    WMITLV_TAG_STRUC_WMI_DMA_RING_CAPABILITIES,
    WMITLV_TAG_STRUC_wmi_dma_ring_cfg_req_fixed_param,
    WMITLV_TAG_STRUC_wmi_dma_ring_cfg_rsp_fixed_param,
    WMITLV_TAG_STRUC_wmi_dma_buf_release_fixed_param,
    WMITLV_TAG_STRUC_wmi_dma_buf_release_entry,
    WMITLV_TAG_STRUC_wmi_sar_get_limits_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sar_get_limits_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sar_get_limits_event_row,
    WMITLV_TAG_STRUC_wmi_offload_11k_report_fixed_param,
    WMITLV_TAG_STRUC_wmi_invoke_neighbor_report_fixed_param,
    WMITLV_TAG_STRUC_wmi_neighbor_report_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_connectivity_check_stats,
    WMITLV_TAG_STRUC_wmi_vdev_get_connectivity_check_stats,
    WMITLV_TAG_STRUC_wmi_bpf_set_vdev_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_set_vdev_work_memory_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_get_vdev_work_memory_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_get_vdev_work_memory_resp_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_nfcal_power_fixed_param,
    WMITLV_TAG_STRUC_wmi_bss_color_change_enable_fixed_param,
    WMITLV_TAG_STRUC_wmi_obss_color_collision_det_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_obss_color_collision_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_runtime_dpd_recal_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_add_dialog_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_del_dialog_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_pause_dialog_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_resume_dialog_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_enable_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_disable_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_add_dialog_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_del_dialog_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_pause_dialog_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_resume_dialog_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_roam_scan_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_tid_configurations_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_tpc_power_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_tpc_power_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_dma_buf_release_spectral_meta_data,
    WMITLV_TAG_STRUC_wmi_motion_det_config_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_motion_det_base_line_config_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_motion_det_start_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_motion_det_base_line_start_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_motion_det_event,
    WMITLV_TAG_STRUC_wmi_motion_det_base_line_event,
    WMITLV_TAG_STRUC_wmi_ndp_transport_ip_param,
    WMITLV_TAG_STRUC_wmi_obss_spatial_reuse_set_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_esp_estimate_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_host_config_param,
    WMITLV_TAG_STRUC_wmi_spectral_bin_scaling_params,
    WMITLV_TAG_STRUC_wmi_peer_cfr_capture_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_chan_width_switch_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_width_peer_list,
    WMITLV_TAG_STRUC_wmi_obss_spatial_reuse_set_def_obss_thresh_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_he_tb_action_frm_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_extd2_stats,
    WMITLV_TAG_STRUC_wmi_hpcs_pulse_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_ctl_failsafe_check_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_chainmask_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_event_info,
    WMITLV_TAG_STRUC_wmi_ndp_channel_info,
    WMITLV_TAG_STRUC_wmi_ndp_cmd_param,
    WMITLV_TAG_STRUC_wmi_ndp_event_param,
    WMITLV_TAG_STRUC_wmi_pdev_pktlog_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_pktlog_filter_info,
    WMITLV_TAG_STRUC_wmi_quiet_offload_info,
    WMITLV_TAG_STRUC_wmi_get_bcn_recv_stats_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_bcn_recv_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_tx_pn_request_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_tx_pn_response_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tlv_arrays_len_param,
    WMITLV_TAG_STRUC_wmi_peer_unmap_response_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_csc_switch_count_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_bss_load_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_blacklist_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_csc_vdev_list,
    WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_info_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_state_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_dpwb_state_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_tdm_state_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_idrx_state_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_antenna_sharing_state_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_wlm_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlm_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_key_material_ext,
    WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_cold_boot_cal_data_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_rap_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_rap_config_on_sta_ps_tlv_param,
    WMITLV_TAG_STRUC_wmi_pdev_rap_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_tdcc_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_deauth_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_idle_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_idle_trigger_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_stats_interference,
    WMITLV_TAG_STRUC_wmi_roam_score_delta_param,
    WMITLV_TAG_STRUC_wmi_roam_cnd_min_rssi_param,
    WMITLV_TAG_STRUC_wmi_chan_rf_characterization_info,
    WMITLV_TAG_STRUC_wmi_wlanfw_iface_cmb_ind_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlanfw_iface_combination_param,
    WMITLV_TAG_STRUC_wmi_wlanfw_iface_limit_param,
    WMITLV_TAG_STRUC_wmi_pdev_dsm_filter_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_bssid_disallow_list_config_param,
    WMITLV_TAG_STRUC_wmi_mgmt_hdr,
    WMITLV_TAG_STRUC_wmi_muedca_params_config_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_btwt_invite_sta_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_btwt_remove_sta_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_btwt_invite_sta_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_btwt_remove_sta_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_delete_all_peer_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_delete_all_peer_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_rf_characterization_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_oem_data_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_enable_disable_trigger_reason_fixed_param,
    WMITLV_TAG_STRUC_wmi_service_ready_ext2_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_preauth_status_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_preauth_start_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_elna_bypass_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_elna_bypass_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_elna_bypass_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_pmkid_request_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_phase_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_aggr_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_audio_aggr_rate_set,
    WMITLV_TAG_STRUC_wmi_audio_aggr_add_group,
    WMITLV_TAG_STRUC_wmi_audio_aggr_del_group,
    WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_rate,
    WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_retry,
    WMITLV_TAG_STRUC_wmi_cfr_capture_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_cfr_filter_group_config,
    WMITLV_TAG_STRUC_wmi_fd_tmpl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_bss_max_idle_time_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_atf_ssid_grp_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_atf_group_info,
    WMITLV_TAG_STRUC_wmi_atf_grp_wmm_ac_cfg_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_atf_group_wmm_ac_info,
    WMITLV_TAG_STRUC_wmi_peer_atf_ext_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_atf_ext_info,
    WMITLV_TAG_STRUC_wmi_get_channel_ani_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_channel_ani_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_get_channel_ani_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_oem_data_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_config_vlan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_trigger_reason_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_channel_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_ap_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_result_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_neighbor_report_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_neighbor_report_channel_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_set_ocl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_auto_rate,
    WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_probe,
    WMITLV_TAG_STRUC_wmi_audio_aggr_update_sta_group_info,
    WMITLV_TAG_STRUC_wmi_audio_sync_trigger_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_sync_qtimer_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_sync_start_stop_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_sync_q_master_slave_offset_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_sync_q_master_slave_times,
    WMITLV_TAG_STRUC_wmi_roam_frame_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_pcl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_create_conf_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_multiple_vdev_restart_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_get_scan_channel_list_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_channel_list_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_big_data_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_send_big_data_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_dmesg_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_frame_inject_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_smartant_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_tbtt_offset_sync_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_rnr_bss_tbtt_info,
    WMITLV_TAG_STRUC_WMI_MAC_PHY_CAPABILITIES_EXT,
    WMITLV_TAG_STRUC_WMI_HAL_REG_CAPABILITIES_EXT2,
    WMITLV_TAG_STRUC_wmi_roam_pmk_cache_synch_tlv_param,
    WMITLV_TAG_STRUC_wmi_mdns_set_staIP_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_big_data_p2_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_send_big_data_p2_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_aggr_get_statistics,
    WMITLV_TAG_STRUC_wmi_audio_aggr_reset_statistics,
    WMITLV_TAG_STRUC_wmi_audio_aggr_statistics_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_aggr_group_stats,
    WMITLV_TAG_STRUC_wmi_audio_aggr_peer_stats,
    WMITLV_TAG_STRUC_wmi_ant_controller_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_srg_bss_color_bitmap_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_srg_partial_bssid_bitmap_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_simulation_test_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_aggr_set_rtscts_config,
    WMITLV_TAG_STRUC_wmi_pdev_sscan_fw_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_sscan_fft_bin_index,
    WMITLV_TAG_STRUC_wmi_pdev_srg_obss_color_enable_bitmap_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_srg_obss_bssid_enable_bitmap_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_non_srg_obss_color_enable_bitmap_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_non_srg_obss_bssid_enable_bitmap_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_capability_report_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pmf_bcn_protect_stats,
    WMITLV_TAG_STRUC_wmi_nan_capabilities,
    WMITLV_TAG_STRUC_wmi_request_ctrl_path_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ctrl_path_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ctrl_path_pdev_stats_struct,
    WMITLV_TAG_STRUC_wmi_pdev_get_tpc_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_tpc_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tpc_configs,
    WMITLV_TAG_STRUC_wmi_max_reg_power_allowed,
    WMITLV_TAG_STRUC_wmi_tpc_rates_array,
    WMITLV_TAG_STRUC_wmi_tpc_ctl_pwr_table,
    WMITLV_TAG_STRUC_wmi_vdev_bcn_latency_fixed_param,
    WMITLV_TAG_STRUC_wmi_audio_aggr_set_sched_method,
    WMITLV_TAG_STRUC_wmi_audio_aggr_get_sched_method,
    WMITLV_TAG_STRUC_wmi_audio_aggr_sched_method_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_disconnect_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_btm_response_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_initial_info_tlv_param,
    WMITLV_TAG_STRUC_wmi_pdev_twt_session_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_twt_session_stats_info,
    WMITLV_TAG_STRUC_wmi_configure_roam_trigger_parameters,
    WMITLV_TAG_STRUC_wmi_vdev_extd_stats,
    WMITLV_TAG_STRUC_wmi_twt_add_dialog_additional_params,
    WMITLV_TAG_STRUC_WMI_SCAN_RADIO_CAPABILITIES_EXT2,
    WMITLV_TAG_STRUC_wmi_wfa_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wfa_config_rsnxe,
    WMITLV_TAG_STRUC_wmi_wfa_config_csa,
    WMITLV_TAG_STRUC_wmi_wfa_config_ocv,
    WMITLV_TAG_STRUC_wmi_wfa_config_saquery,
    WMITLV_TAG_STRUC_wmi_roam_data_rssi_roaming_param,
    WMITLV_TAG_STRUC_wmi_request_unified_ll_get_sta_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sscan_evt_message_fixed_param,
    WMITLV_TAG_STRUC_wmi_qos_null_frame_tx_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_qos_null_frame_tx_compl_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_regulatory_rule_ext_struct,
    WMITLV_TAG_STRUC_wmi_reg_chan_list_cc_event_ext_fixed_param,
    WMITLV_TAG_STRUC_wmi_ctrl_path_mem_stats_struct,
} WMITLV_TAG_ID;

/*
 * IMPORTANT: Please add _ALL_ WMI Commands Here.
 * Otherwise, these WMI TLV Functions will be process them.
 */
#define WMITLV_ALL_CMD_LIST(OP) \
    OP(WMI_INIT_CMDID) \
    OP(WMI_PEER_CREATE_CMDID) \
    OP(WMI_PEER_DELETE_CMDID) \
    OP(WMI_PEER_FLUSH_TIDS_CMDID) \
    OP(WMI_PEER_SET_PARAM_CMDID) \
    OP(WMI_STA_POWERSAVE_MODE_CMDID) \
    OP(WMI_STA_POWERSAVE_PARAM_CMDID) \
    OP(WMI_STA_TDCC_CONFIG_CMDID) \
    OP(WMI_STA_DTIM_PS_METHOD_CMDID) \
    OP(WMI_PDEV_SET_REGDOMAIN_CMDID) \
    OP(WMI_PEER_TID_ADDBA_CMDID) \
    OP(WMI_PEER_TID_DELBA_CMDID) \
    OP(WMI_PDEV_FTM_INTG_CMDID) \
    OP(WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID) \
    OP(WMI_WOW_ENABLE_CMDID) \
    OP(WMI_RMV_BCN_FILTER_CMDID) \
    OP(WMI_ROAM_SCAN_MODE) \
    OP(WMI_ROAM_SCAN_RSSI_THRESHOLD) \
    OP(WMI_ROAM_SCAN_PERIOD) \
    OP(WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD) \
    OP(WMI_START_SCAN_CMDID) \
    OP(WMI_VDEV_PLMREQ_START_CMDID) \
    OP(WMI_VDEV_PLMREQ_STOP_CMDID) \
    OP(WMI_PDEV_SET_CHANNEL_CMDID) \
    OP(WMI_PDEV_SET_WMM_PARAMS_CMDID) \
    OP(WMI_VDEV_START_REQUEST_CMDID) \
    OP(WMI_VDEV_RESTART_REQUEST_CMDID) \
    OP(WMI_P2P_GO_SET_BEACON_IE) \
    OP(WMI_GTK_OFFLOAD_CMDID) \
    OP(WMI_SCAN_CHAN_LIST_CMDID) \
    OP(WMI_STA_UAPSD_AUTO_TRIG_CMDID) \
    OP(WMI_PRB_TMPL_CMDID) \
    OP(WMI_BCN_TMPL_CMDID) \
    OP(WMI_VDEV_INSTALL_KEY_CMDID) \
    OP(WMI_PEER_ASSOC_CMDID) \
    OP(WMI_ADD_BCN_FILTER_CMDID) \
    OP(WMI_STA_KEEPALIVE_CMDID) \
    OP(WMI_SET_ARP_NS_OFFLOAD_CMDID) \
    OP(WMI_P2P_SET_VENDOR_IE_DATA_CMDID) \
    OP(WMI_AP_PS_PEER_PARAM_CMDID) \
    OP(WMI_WLAN_PROFILE_TRIGGER_CMDID) \
    OP(WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID) \
    OP(WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID) \
    OP(WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID) \
    OP(WMI_WOW_DEL_WAKE_PATTERN_CMDID) \
    OP(WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID) \
    OP(WMI_RTT_MEASREQ_CMDID) \
    OP(WMI_RTT_TSF_CMDID) \
    OP(WMI_OEM_REQ_CMDID) \
    OP(WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID) \
    OP(WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID) \
    OP(WMI_REQUEST_STATS_CMDID) \
    OP(WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID) \
    OP(WMI_CSA_OFFLOAD_ENABLE_CMDID) \
    OP(WMI_CSA_OFFLOAD_CHANSWITCH_CMDID) \
    OP(WMI_CHATTER_SET_MODE_CMDID) \
    OP(WMI_ECHO_CMDID) \
    OP(WMI_PDEV_UTF_CMDID) \
    OP(WMI_PDEV_QVIT_CMDID) \
    OP(WMI_VDEV_SET_KEEPALIVE_CMDID) \
    OP(WMI_VDEV_GET_KEEPALIVE_CMDID) \
    OP(WMI_FORCE_FW_HANG_CMDID) \
    OP(WMI_GPIO_CONFIG_CMDID) \
    OP(WMI_GPIO_OUTPUT_CMDID) \
    OP(WMI_PEER_ADD_WDS_ENTRY_CMDID) \
    OP(WMI_PEER_REMOVE_WDS_ENTRY_CMDID) \
    OP(WMI_BCN_TX_CMDID) \
    OP(WMI_PDEV_SEND_BCN_CMDID) \
    OP(WMI_MGMT_TX_CMDID) \
    OP(WMI_ADDBA_CLEAR_RESP_CMDID) \
    OP(WMI_ADDBA_SEND_CMDID) \
    OP(WMI_DELBA_SEND_CMDID) \
    OP(WMI_ADDBA_SET_RESP_CMDID) \
    OP(WMI_SEND_SINGLEAMSDU_CMDID) \
    OP(WMI_PDEV_PKTLOG_ENABLE_CMDID) \
    OP(WMI_PDEV_PKTLOG_DISABLE_CMDID) \
    OP(WMI_PDEV_SET_HT_CAP_IE_CMDID) \
    OP(WMI_PDEV_SET_VHT_CAP_IE_CMDID) \
    OP(WMI_PDEV_SET_DSCP_TID_MAP_CMDID) \
    OP(WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID) \
    OP(WMI_PDEV_GET_TPC_CONFIG_CMDID) \
    OP(WMI_PDEV_SET_BASE_MACADDR_CMDID) \
    OP(WMI_PEER_MCAST_GROUP_CMDID) \
    OP(WMI_ROAM_AP_PROFILE) \
    OP(WMI_SCAN_SCH_PRIO_TBL_CMDID) \
    OP(WMI_PDEV_DFS_ENABLE_CMDID) \
    OP(WMI_PDEV_DFS_DISABLE_CMDID) \
    OP(WMI_WOW_ADD_WAKE_PATTERN_CMDID) \
    OP(WMI_PDEV_SUSPEND_CMDID) \
    OP(WMI_PDEV_RESUME_CMDID) \
    OP(WMI_STOP_SCAN_CMDID) \
    OP(WMI_PDEV_SET_PARAM_CMDID) \
    OP(WMI_PDEV_SET_QUIET_MODE_CMDID) \
    OP(WMI_VDEV_CREATE_CMDID) \
    OP(WMI_VDEV_DELETE_CMDID) \
    OP(WMI_VDEV_UP_CMDID) \
    OP(WMI_VDEV_STOP_CMDID) \
    OP(WMI_VDEV_DOWN_CMDID) \
    OP(WMI_VDEV_SET_PARAM_CMDID) \
    OP(WMI_SCAN_UPDATE_REQUEST_CMDID) \
    OP(WMI_CHATTER_ADD_COALESCING_FILTER_CMDID) \
    OP(WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID) \
    OP(WMI_CHATTER_COALESCING_QUERY_CMDID) \
    OP(WMI_TXBF_CMDID) \
    OP(WMI_DBGLOG_CFG_CMDID) \
    OP(WMI_VDEV_WNM_SLEEPMODE_CMDID) \
    OP(WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID) \
    OP(WMI_VDEV_WMM_ADDTS_CMDID) \
    OP(WMI_VDEV_WMM_DELTS_CMDID) \
    OP(WMI_VDEV_SET_WMM_PARAMS_CMDID) \
    OP(WMI_VDEV_SET_GTX_PARAMS_CMDID) \
    OP(WMI_TDLS_SET_STATE_CMDID) \
    OP(WMI_TDLS_PEER_UPDATE_CMDID) \
    OP(WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID) \
    OP(WMI_ROAM_CHAN_LIST)  \
    OP(WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID)\
    OP(WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID)    \
    OP(WMI_RESMGR_SET_CHAN_LATENCY_CMDID) \
    OP(WMI_BA_REQ_SSN_CMDID) \
    OP(WMI_STA_SMPS_FORCE_MODE_CMDID) \
    OP(WMI_SET_MCASTBCAST_FILTER_CMDID) \
    OP(WMI_P2P_SET_OPPPS_PARAM_CMDID) \
    OP(WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID) \
    OP(WMI_STA_SMPS_PARAM_CMDID) \
    OP(WMI_MCC_SCHED_TRAFFIC_STATS_CMDID) \
    OP(WMI_HB_SET_ENABLE_CMDID) \
    OP(WMI_HB_SET_TCP_PARAMS_CMDID) \
    OP(WMI_HB_SET_TCP_PKT_FILTER_CMDID) \
    OP(WMI_HB_SET_UDP_PARAMS_CMDID) \
    OP(WMI_HB_SET_UDP_PKT_FILTER_CMDID) \
    OP(WMI_PEER_INFO_REQ_CMDID) \
    OP(WMI_RMC_SET_MODE_CMDID) \
    OP(WMI_RMC_SET_ACTION_PERIOD_CMDID) \
    OP(WMI_RMC_CONFIG_CMDID) \
    OP(WMI_MHF_OFFLOAD_SET_MODE_CMDID) \
    OP(WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID) \
    OP(WMI_DFS_PHYERR_FILTER_ENA_CMDID) \
    OP(WMI_DFS_PHYERR_FILTER_DIS_CMDID) \
    OP(WMI_BATCH_SCAN_ENABLE_CMDID) \
    OP(WMI_BATCH_SCAN_DISABLE_CMDID) \
    OP(WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID)\
    OP(WMI_THERMAL_MGMT_CMDID)\
    OP(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID)\
    OP(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID) \
    OP(WMI_NAN_CMDID) \
    OP(WMI_MODEM_POWER_STATE_CMDID) \
    OP(WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID) \
    OP(WMI_ROAM_SCAN_CMD) \
    OP(WMI_REQUEST_STATS_EXT_CMDID) \
    OP(WMI_OBSS_SCAN_ENABLE_CMDID) \
    OP(WMI_OBSS_SCAN_DISABLE_CMDID) \
    OP(WMI_PDEV_SET_LED_CONFIG_CMDID) \
    OP(WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID) \
    OP(WMI_TPC_CHAINMASK_CONFIG_CMDID) \
    OP(WMI_CHAN_AVOID_UPDATE_CMDID) \
    OP(WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID) \
    OP(WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID) \
    OP(WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID) \
    OP(WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID) \
    OP(WMI_REQUEST_LINK_STATS_CMDID) \
    OP(WMI_START_LINK_STATS_CMDID) \
    OP(WMI_CLEAR_LINK_STATS_CMDID) \
    OP(WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID) \
    OP(WMI_LPI_START_SCAN_CMDID) \
    OP(WMI_LPI_STOP_SCAN_CMDID) \
    OP(WMI_EXTSCAN_START_CMDID) \
    OP(WMI_EXTSCAN_STOP_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID) \
    OP(WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID) \
    OP(WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID) \
    OP(WMI_EXTSCAN_SET_CAPABILITIES_CMDID) \
    OP(WMI_EXTSCAN_GET_CAPABILITIES_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID) \
    OP(WMI_D0_WOW_ENABLE_DISABLE_CMDID) \
    OP(WMI_UNIT_TEST_CMDID) \
    OP(WMI_ROAM_SYNCH_COMPLETE) \
    OP(WMI_EXTWOW_ENABLE_CMDID) \
    OP(WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID) \
    OP(WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID) \
    OP(WMI_ROAM_SET_RIC_REQUEST_CMDID) \
    OP(WMI_PDEV_GET_TEMPERATURE_CMDID) \
    OP(WMI_SET_DHCP_SERVER_OFFLOAD_CMDID) \
    OP(WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID)\
    OP(WMI_SCAN_PROB_REQ_OUI_CMDID) \
    OP(WMI_TDLS_SET_OFFCHAN_MODE_CMDID)\
    OP(WMI_PDEV_SET_LED_FLASHING_CMDID) \
    OP(WMI_ROAM_INVOKE_CMDID) \
    OP(WMI_MDNS_OFFLOAD_ENABLE_CMDID) \
    OP(WMI_MDNS_SET_FQDN_CMDID) \
    OP(WMI_MDNS_SET_RESPONSE_CMDID) \
    OP(WMI_MDNS_GET_STATS_CMDID) \
    OP(WMI_MDNS_SET_STAIP_CMDID) \
    OP(WMI_SET_ANTENNA_DIVERSITY_CMDID) \
    OP(WMI_SAP_OFL_ENABLE_CMDID) \
    OP(WMI_APFIND_CMDID) \
    OP(WMI_OCB_SET_SCHED_CMDID) \
    OP(WMI_OCB_SET_CONFIG_CMDID) \
    OP(WMI_OCB_SET_UTC_TIME_CMDID) \
    OP(WMI_OCB_START_TIMING_ADVERT_CMDID) \
    OP(WMI_OCB_STOP_TIMING_ADVERT_CMDID) \
    OP(WMI_OCB_GET_TSF_TIMER_CMDID) \
    OP(WMI_DCC_GET_STATS_CMDID) \
    OP(WMI_DCC_CLEAR_STATS_CMDID) \
    OP(WMI_DCC_UPDATE_NDL_CMDID) \
    OP(WMI_ROAM_FILTER_CMDID) \
    OP(WMI_PASSPOINT_LIST_CONFIG_CMDID) \
    OP(WMI_VDEV_TSF_TSTAMP_ACTION_CMDID) \
    OP(WMI_GET_FW_MEM_DUMP_CMDID) \
    OP(WMI_DEBUG_MESG_FLUSH_CMDID) \
    OP(WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID) \
    OP(WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID) \
    OP(WMI_VDEV_SET_IE_CMDID) \
    OP(WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID) \
    OP(WMI_SOC_SET_PCL_CMDID) \
    OP(WMI_SOC_SET_HW_MODE_CMDID) \
    OP(WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID) \
    OP(WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID) \
    OP(WMI_DIAG_EVENT_LOG_CONFIG_CMDID) \
    OP(WMI_PACKET_FILTER_CONFIG_CMDID) \
    OP(WMI_PACKET_FILTER_ENABLE_CMDID) \
    OP(WMI_SAP_SET_BLACKLIST_PARAM_CMDID) \
    OP(WMI_MGMT_TX_SEND_CMDID) \
    OP(WMI_SOC_SET_ANTENNA_MODE_CMDID) \
    OP(WMI_WOW_UDP_SVC_OFLD_CMDID) \
    OP(WMI_LRO_CONFIG_CMDID) \
    OP(WMI_MAWC_SENSOR_REPORT_IND_CMDID) \
    OP(WMI_ROAM_CONFIGURE_MAWC_CMDID) \
    OP(WMI_NLO_CONFIGURE_MAWC_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_MAWC_CMDID) \
    OP(WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID) \
    OP(WMI_AP_PS_EGAP_PARAM_CMDID) \
    OP(WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID) \
    OP(WMI_TRANSFER_DATA_TO_FLASH_CMDID) \
    OP(WMI_OEM_REQUEST_CMDID) \
    OP(WMI_BPF_GET_CAPABILITY_CMDID) \
    OP(WMI_BPF_GET_VDEV_STATS_CMDID) \
    OP(WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID) \
    OP(WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID) \
    OP(WMI_PEER_UPDATE_WDS_ENTRY_CMDID) \
    OP(WMI_PEER_ADD_PROXY_STA_ENTRY_CMDID) \
    OP(WMI_PDEV_FIPS_CMDID) \
    OP(WMI_PDEV_SMART_ANT_ENABLE_CMDID) \
    OP(WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID) \
    OP(WMI_PDEV_SET_ANTENNA_SWITCH_TABLE_CMDID) \
    OP(WMI_PDEV_SET_CTL_TABLE_CMDID) \
    OP(WMI_PDEV_SET_MIMOGAIN_TABLE_CMDID) \
    OP(WMI_PDEV_GET_TPC_CMDID) \
    OP(WMI_PDEV_GET_ANI_CCK_CONFIG_CMDID) \
    OP(WMI_PDEV_GET_ANI_OFDM_CONFIG_CMDID) \
    OP(WMI_VDEV_RATEMASK_CMDID) \
    OP(WMI_VDEV_ATF_REQUEST_CMDID) \
    OP(WMI_VDEV_SET_DSCP_TID_MAP_CMDID) \
    OP(WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID) \
    OP(WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID) \
    OP(WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID) \
    OP(WMI_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMDID) \
    OP(WMI_PEER_ATF_REQUEST_CMDID) \
    OP(WMI_FWTEST_CMDID) \
    OP(WMI_QBOOST_CFG_CMDID) \
    OP(WMI_ROAM_SET_MBO_PARAM_CMDID) \
    OP(WMI_MIB_STATS_ENABLE_CMDID) \
    OP(WMI_NDI_GET_CAP_REQ_CMDID) \
    OP(WMI_NDP_INITIATOR_REQ_CMDID) \
    OP(WMI_NDP_RESPONDER_REQ_CMDID) \
    OP(WMI_NDP_END_REQ_CMDID) \
    OP(WMI_PDEV_SET_PCL_CMDID) \
    OP(WMI_PDEV_SET_HW_MODE_CMDID) \
    OP(WMI_PDEV_SET_MAC_CONFIG_CMDID) \
    OP(WMI_PDEV_SET_ANTENNA_MODE_CMDID) \
    OP(WMI_VDEV_SET_QUIET_MODE_CMDID) \
    OP(WMI_COEX_CONFIG_CMDID) \
    OP(WMI_CONFIG_ENHANCED_MCAST_FILTER_CMDID) \
    OP(WMI_CHAN_AVOID_RPT_ALLOW_CMDID) \
    OP(WMI_SET_PERIODIC_CHANNEL_STATS_CONFIG_CMDID) \
    OP(WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID) \
    OP(WMI_PDEV_WAL_POWER_DEBUG_CMDID) \
    OP(WMI_VDEV_WISA_CMDID) \
    OP(WMI_SCAN_ADAPTIVE_DWELL_CONFIG_CMDID) \
    OP(WMI_WOW_SET_ACTION_WAKE_UP_CMDID) \
    OP(WMI_PEER_BWF_REQUEST_CMDID) \
    OP(WMI_DBGLOG_TIME_STAMP_SYNC_CMDID) \
    OP(WMI_RMC_SET_MANUAL_LEADER_CMDID) \
    OP(WMI_P2P_LISTEN_OFFLOAD_START_CMDID) \
    OP(WMI_P2P_LISTEN_OFFLOAD_STOP_CMDID) \
    OP(WMI_PEER_REORDER_QUEUE_SETUP_CMDID) \
    OP(WMI_PEER_REORDER_QUEUE_REMOVE_CMDID) \
    OP(WMI_SET_MULTIPLE_MCAST_FILTER_CMDID) \
    OP(WMI_READ_DATA_FROM_FLASH_CMDID) \
    OP(WMI_PDEV_SET_REORDER_TIMEOUT_VAL_CMDID) \
    OP(WMI_PEER_SET_RX_BLOCKSIZE_CMDID) \
    OP(WMI_PDEV_SET_WAKEUP_CONFIG_CMDID) \
    OP(WMI_PEER_ANTDIV_INFO_REQ_CMDID) \
    OP(WMI_PDEV_GET_ANTDIV_STATUS_CMDID) \
    OP(WMI_MNT_FILTER_CMDID) \
    OP(WMI_PDEV_GET_CHIP_POWER_STATS_CMDID) \
    OP(WMI_COEX_GET_ANTENNA_ISOLATION_CMDID) \
    OP(WMI_PDEV_SET_STATS_THRESHOLD_CMDID) \
    OP(WMI_REQUEST_WLAN_STATS_CMDID) \
    OP(WMI_VDEV_ENCRYPT_DECRYPT_DATA_REQ_CMDID) \
    OP(WMI_SAR_LIMITS_CMDID) \
    OP(WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID) \
    OP(WMI_PDEV_DFS_PHYERR_OFFLOAD_DISABLE_CMDID) \
    OP(WMI_VDEV_ADFS_CH_CFG_CMDID) \
    OP(WMI_VDEV_ADFS_OCAC_ABORT_CMDID) \
    OP(WMI_REQUEST_RCPI_CMDID) \
    OP(WMI_REQUEST_PEER_STATS_INFO_CMDID) \
    OP(WMI_SET_CURRENT_COUNTRY_CMDID) \
    OP(WMI_11D_SCAN_START_CMDID) \
    OP(WMI_11D_SCAN_STOP_CMDID) \
    OP(WMI_REQUEST_RADIO_CHAN_STATS_CMDID) \
    OP(WMI_ROAM_PER_CONFIG_CMDID) \
    OP(WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_CMDID) \
    OP(WMI_BPF_SET_VDEV_ACTIVE_MODE_CMDID) \
    OP(WMI_HW_DATA_FILTER_CMDID) \
    OP(WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID) \
    OP(WMI_LPI_OEM_REQ_CMDID) \
    OP(WMI_PDEV_UPDATE_PKT_ROUTING_CMDID) \
    OP(WMI_PDEV_CHECK_CAL_VERSION_CMDID) \
    OP(WMI_PDEV_SET_DIVERSITY_GAIN_CMDID) \
    OP(WMI_VDEV_SET_ARP_STAT_CMDID) \
    OP(WMI_VDEV_GET_ARP_STAT_CMDID) \
    OP(WMI_VDEV_GET_TX_POWER_CMDID) \
    OP(WMI_OFFCHAN_DATA_TX_SEND_CMDID) \
    OP(WMI_SET_INIT_COUNTRY_CMDID) \
    OP(WMI_SET_SCAN_DBS_DUTY_CYCLE_CMDID) \
    OP(WMI_PDEV_DIV_GET_RSSI_ANTID_CMDID) \
    OP(WMI_THERM_THROT_SET_CONF_CMDID) \
    OP(WMI_OEM_DMA_RING_CFG_REQ_CMDID) \
    OP(WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID) \
    OP(WMI_VDEV_LIMIT_OFFCHAN_CMDID) \
    OP(WMI_PDEV_UPDATE_FILS_HLP_PKT_CMDID) \
    OP(WMI_PDEV_UPDATE_PMK_CACHE_CMDID) \
    OP(WMI_HB_OIC_PING_OFFLOAD_PARAM_CMDID) \
    OP(WMI_HB_OIC_PING_OFFLOAD_SET_ENABLE_CMDID) \
    OP(WMI_HB_DHCP_LEASE_RENEW_OFFLOAD_CMDID) \
    OP(WMI_ROAM_BTM_CONFIG_CMDID) \
    OP(WMI_WLM_CONFIG_CMDID) \
    OP(WMI_PDEV_UPDATE_CTLTABLE_REQUEST_CMDID) \
    OP(WMI_PDEV_CONFIG_VENDOR_OUI_ACTION_CMDID) \
    OP(WMI_PDEV_SEND_FD_CMDID) \
    OP(WMI_ENABLE_FILS_CMDID) \
    OP(WMI_BCN_OFFLOAD_CTRL_CMDID) \
    OP(WMI_PDEV_SET_AC_TX_QUEUE_OPTIMIZED_CMDID) \
    OP(WMI_PEER_TID_MSDUQ_QDEPTH_THRESH_UPDATE_CMDID) \
    OP(WMI_PDEV_SET_RX_FILTER_PROMISCUOUS_CMDID) \
    OP(WMI_SAP_OBSS_DETECTION_CFG_CMDID) \
    OP(WMI_PDEV_DMA_RING_CFG_REQ_CMDID) \
    OP(WMI_SAR_GET_LIMITS_CMDID)\
    OP(WMI_11K_OFFLOAD_REPORT_CMDID) \
    OP(WMI_11K_INVOKE_NEIGHBOR_REPORT_CMDID) \
    OP(WMI_BPF_SET_VDEV_ENABLE_CMDID) \
    OP(WMI_BPF_SET_VDEV_WORK_MEMORY_CMDID) \
    OP(WMI_BPF_GET_VDEV_WORK_MEMORY_CMDID) \
    OP(WMI_PDEV_GET_NFCAL_POWER_CMDID) \
    OP(WMI_BSS_COLOR_CHANGE_ENABLE_CMDID) \
    OP(WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID) \
    OP(WMI_RUNTIME_DPD_RECAL_CMDID) \
    OP(WMI_TWT_ENABLE_CMDID) \
    OP(WMI_TWT_DISABLE_CMDID) \
    OP(WMI_TWT_ADD_DIALOG_CMDID) \
    OP(WMI_TWT_DEL_DIALOG_CMDID) \
    OP(WMI_TWT_PAUSE_DIALOG_CMDID) \
    OP(WMI_TWT_RESUME_DIALOG_CMDID) \
    OP(WMI_REQUEST_ROAM_SCAN_STATS_CMDID) \
    OP(WMI_PEER_TID_CONFIGURATIONS_CMDID) \
    OP(WMI_VDEV_SET_CUSTOM_SW_RETRY_TH_CMDID) \
    OP(WMI_GET_TPC_POWER_CMDID) \
    OP(WMI_MOTION_DET_CONFIG_PARAM_CMDID) \
    OP(WMI_MOTION_DET_BASE_LINE_CONFIG_PARAM_CMDID) \
    OP(WMI_MOTION_DET_START_STOP_CMDID) \
    OP(WMI_MOTION_DET_BASE_LINE_START_STOP_CMDID) \
    OP(WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID) \
    OP(WMI_PEER_CFR_CAPTURE_CMDID) \
    OP(WMI_PEER_CHAN_WIDTH_SWITCH_CMDID) \
    OP(WMI_PDEV_OBSS_PD_SPATIAL_REUSE_SET_DEF_OBSS_THRESH_CMDID) \
    OP(WMI_PDEV_HE_TB_ACTION_FRM_CMDID) \
    OP(WMI_HPCS_PULSE_START_CMDID) \
    OP(WMI_VDEV_CHAINMASK_CONFIG_CMDID) \
    OP(WMI_VDEV_BCN_OFFLOAD_QUIET_CONFIG_CMDID) \
    OP(WMI_NDP_CMDID) \
    OP(WMI_PDEV_PKTLOG_FILTER_CMDID) \
    OP(WMI_VDEV_GET_BCN_RECEPTION_STATS_CMDID) \
    OP(WMI_PEER_TX_PN_REQUEST_CMDID) \
    OP(WMI_PEER_UNMAP_RESPONSE_CMDID) \
    OP(WMI_ROAM_BSS_LOAD_CONFIG_CMDID) \
    OP(WMI_VDEV_GET_MWS_COEX_INFO_CMDID) \
    OP(WMI_REQUEST_WLM_STATS_CMDID) \
    OP(WMI_PDEV_SET_RAP_CONFIG_CMDID) \
    OP(WMI_ROAM_DEAUTH_CONFIG_CMDID) \
    OP(WMI_ROAM_IDLE_CONFIG_CMDID) \
    OP(WMI_IDLE_TRIGGER_MONITOR_CMDID) \
    OP(WMI_PDEV_DSM_FILTER_CMDID) \
    OP(WMI_TWT_BTWT_INVITE_STA_CMDID) \
    OP(WMI_TWT_BTWT_REMOVE_STA_CMDID) \
    OP(WMI_VDEV_DELETE_ALL_PEER_CMDID) \
    OP(WMI_OEM_DATA_CMDID) \
    OP(WMI_ROAM_ENABLE_DISABLE_TRIGGER_REASON_CMDID) \
    OP(WMI_ROAM_PREAUTH_STATUS_CMDID) \
    OP(WMI_SET_ELNA_BYPASS_CMDID) \
    OP(WMI_GET_ELNA_BYPASS_CMDID) \
    OP(WMI_AUDIO_AGGR_ENABLE_CMDID) \
    OP(WMI_AUDIO_AGGR_ADD_GROUP_CMDID) \
    OP(WMI_AUDIO_AGGR_DEL_GROUP_CMDID) \
    OP(WMI_AUDIO_AGGR_SET_GROUP_RATE_CMDID) \
    OP(WMI_AUDIO_AGGR_SET_GROUP_RETRY_CMDID) \
    OP(WMI_AUDIO_AGGR_UPDATE_STA_GROUP_INFO_CMDID) \
    OP(WMI_CFR_CAPTURE_FILTER_CMDID) \
    OP(WMI_FD_TMPL_CMDID) \
    OP(WMI_VDEV_BSS_MAX_IDLE_TIME_CMDID) \
    OP(WMI_ATF_SSID_GROUPING_REQUEST_CMDID) \
    OP(WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID) \
    OP(WMI_PEER_ATF_EXT_REQUEST_CMDID) \
    OP(WMI_GET_CHANNEL_ANI_CMDID) \
    OP(WMI_PEER_CONFIG_VLAN_CMDID) \
    OP(WMI_SET_OCL_CMDID) \
    OP(WMI_AUDIO_AGGR_SET_GROUP_AUTO_RATE_CMDID) \
    OP(WMI_AUDIO_AGGR_SET_GROUP_PROBE_CMDID) \
    OP(WMI_VDEV_AUDIO_SYNC_TRIGGER_CMDID) \
    OP(WMI_VDEV_AUDIO_SYNC_QTIMER_CMDID) \
    OP(WMI_VDEV_SET_PCL_CMDID) \
    OP(WMI_ROAM_GET_SCAN_CHANNEL_LIST_CMDID) \
    OP(WMI_VDEV_GET_BIG_DATA_CMDID) \
    OP(WMI_PDEV_FRAME_INJECT_CMDID) \
    OP(WMI_PDEV_TBTT_OFFSET_SYNC_CMDID) \
    OP(WMI_VDEV_GET_BIG_DATA_P2_CMDID) \
    OP(WMI_AUDIO_AGGR_GET_STATISTICS_CMDID) \
    OP(WMI_AUDIO_AGGR_RESET_STATISTICS_CMDID) \
    OP(WMI_ANT_CONTROLLER_CMDID) \
    OP(WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID) \
    OP(WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID) \
    OP(WMI_SIMULATION_TEST_CMDID) \
    OP(WMI_AUDIO_AGGR_SET_RTSCTS_CONFIG_CMDID) \
    OP(WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID) \
    OP(WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID) \
    OP(WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID) \
    OP(WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID) \
    OP(WMI_REQUEST_CTRL_PATH_STATS_CMDID) \
    OP(WMI_PDEV_GET_TPC_STATS_CMDID) \
    OP(WMI_AUDIO_AGGR_SET_SCHED_METHOD_CMDID) \
    OP(WMI_AUDIO_AGGR_GET_SCHED_METHOD_CMDID) \
    OP(WMI_WFA_CONFIG_CMDID) \
    OP(WMI_REQUEST_UNIFIED_LL_GET_STA_CMDID) \
    OP(WMI_QOS_NULL_FRAME_TX_SEND_CMDID) \
    OP(WMI_PDEV_ENABLE_DURATION_BASED_TX_MODE_SELECTION_CMDID) \
    /* add new CMD_LIST elements above this line */


/*
 * IMPORTANT: Please add _ALL_ WMI Events Here.
 * Otherwise, these WMI TLV Functions will be process them.
 */
#define WMITLV_ALL_EVT_LIST(OP) \
    OP(WMI_SERVICE_READY_EVENTID) \
    OP(WMI_SERVICE_READY_EXT_EVENTID) \
    OP(WMI_SERVICE_READY_EXT2_EVENTID) \
    OP(WMI_READY_EVENTID) \
    OP(WMI_SCAN_EVENTID) \
    OP(WMI_PDEV_TPC_CONFIG_EVENTID) \
    OP(WMI_CHAN_INFO_EVENTID) \
    OP(WMI_PHYERR_EVENTID) \
    OP(WMI_VDEV_START_RESP_EVENTID) \
    OP(WMI_VDEV_STOPPED_EVENTID) \
    OP(WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID) \
    OP(WMI_PEER_STA_KICKOUT_EVENTID) \
    OP(WMI_MGMT_RX_EVENTID) \
    OP(WMI_TBTTOFFSET_UPDATE_EVENTID) \
    OP(WMI_TX_DELBA_COMPLETE_EVENTID) \
    OP(WMI_TX_ADDBA_COMPLETE_EVENTID) \
    OP(WMI_ROAM_EVENTID) \
    OP(WMI_WOW_WAKEUP_HOST_EVENTID) \
    OP(WMI_RTT_ERROR_REPORT_EVENTID) \
    OP(WMI_OEM_MEASUREMENT_REPORT_EVENTID) \
    OP(WMI_OEM_ERROR_REPORT_EVENTID) \
    OP(WMI_OEM_CAPABILITY_EVENTID) \
    OP(WMI_ECHO_EVENTID) \
    OP(WMI_PDEV_FTM_INTG_EVENTID) \
    OP(WMI_VDEV_GET_KEEPALIVE_EVENTID) \
    OP(WMI_GPIO_INPUT_EVENTID) \
    OP(WMI_CSA_HANDLING_EVENTID) \
    OP(WMI_DEBUG_MESG_EVENTID) \
    OP(WMI_GTK_OFFLOAD_STATUS_EVENTID) \
    OP(WMI_DCS_INTERFERENCE_EVENTID) \
    OP(WMI_WLAN_PROFILE_DATA_EVENTID) \
    OP(WMI_PDEV_UTF_EVENTID) \
    OP(WMI_DEBUG_PRINT_EVENTID) \
    OP(WMI_RTT_MEASUREMENT_REPORT_EVENTID) \
    OP(WMI_HOST_SWBA_EVENTID) \
    OP(WMI_UPDATE_STATS_EVENTID) \
    OP(WMI_PDEV_QVIT_EVENTID) \
    OP(WMI_WLAN_FREQ_AVOID_EVENTID) \
    OP(WMI_GTK_REKEY_FAIL_EVENTID) \
    OP(WMI_NLO_MATCH_EVENTID) \
    OP(WMI_NLO_SCAN_COMPLETE_EVENTID) \
    OP(WMI_APFIND_EVENTID) \
    OP(WMI_CHATTER_PC_QUERY_EVENTID) \
    OP(WMI_UPLOADH_EVENTID) \
    OP(WMI_CAPTUREH_EVENTID) \
    OP(WMI_TDLS_PEER_EVENTID) \
    OP(WMI_VDEV_MCC_BCN_INTERVAL_CHANGE_REQ_EVENTID) \
    OP(WMI_BA_RSP_SSN_EVENTID) \
    OP(WMI_OFFLOAD_BCN_TX_STATUS_EVENTID) \
    OP(WMI_P2P_NOA_EVENTID) \
    OP(WMI_TX_PAUSE_EVENTID) \
    OP(WMI_RFKILL_STATE_CHANGE_EVENTID) \
    OP(WMI_PEER_INFO_EVENTID) \
    OP(WMI_PEER_TX_FAIL_CNT_THR_EVENTID) \
    OP(WMI_DFS_RADAR_EVENTID) \
    OP(WMI_BATCH_SCAN_ENABLED_EVENTID) \
    OP(WMI_BATCH_SCAN_RESULT_EVENTID) \
    OP(WMI_THERMAL_MGMT_EVENTID) \
    OP(WMI_NAN_EVENTID) \
    OP(WMI_PDEV_L1SS_TRACK_EVENTID) \
    OP(WMI_DIAG_DATA_CONTAINER_EVENTID) \
    OP(WMI_PEER_ESTIMATED_LINKSPEED_EVENTID) \
    OP(WMI_AGGR_STATE_TRIG_EVENTID) \
    OP(WMI_STATS_EXT_EVENTID) \
    OP(WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID) \
    OP(WMI_HOST_AUTO_SHUTDOWN_EVENTID) \
    OP(WMI_UPDATE_WHAL_MIB_STATS_EVENTID) \
    OP(WMI_IFACE_LINK_STATS_EVENTID) \
    OP(WMI_PEER_LINK_STATS_EVENTID) \
    OP(WMI_RADIO_LINK_STATS_EVENTID) \
    OP(WMI_LPI_RESULT_EVENTID) \
    OP(WMI_PEER_STATE_EVENTID) \
    OP(WMI_EXTSCAN_START_STOP_EVENTID) \
    OP(WMI_EXTSCAN_OPERATION_EVENTID) \
    OP(WMI_EXTSCAN_TABLE_USAGE_EVENTID) \
    OP(WMI_EXTSCAN_CACHED_RESULTS_EVENTID) \
    OP(WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID) \
    OP(WMI_EXTSCAN_HOTLIST_MATCH_EVENTID) \
    OP(WMI_EXTSCAN_CAPABILITIES_EVENTID) \
    OP(WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID) \
    OP(WMI_D0_WOW_DISABLE_ACK_EVENTID) \
    OP(WMI_ROAM_SYNCH_EVENTID) \
    OP(WMI_ROAM_SYNCH_FRAME_EVENTID) \
    OP(WMI_LPI_STATUS_EVENTID) \
    OP(WMI_LPI_HANDOFF_EVENTID) \
    OP(WMI_UPDATE_VDEV_RATE_STATS_EVENTID) \
    OP(WMI_PDEV_TEMPERATURE_EVENTID) \
    OP(WMI_DIAG_EVENTID) \
    OP(WMI_MDNS_STATS_EVENTID) \
    OP(WMI_PDEV_RESUME_EVENTID) \
    OP(WMI_SAP_OFL_ADD_STA_EVENTID) \
    OP(WMI_SAP_OFL_DEL_STA_EVENTID) \
    OP(WMI_OCB_SET_SCHED_EVENTID) \
    OP(WMI_OCB_SET_CONFIG_RESP_EVENTID) \
    OP(WMI_OCB_GET_TSF_TIMER_RESP_EVENTID) \
    OP(WMI_DCC_GET_STATS_RESP_EVENTID) \
    OP(WMI_DCC_UPDATE_NDL_RESP_EVENTID) \
    OP(WMI_DCC_STATS_EVENTID) \
    OP(WMI_PASSPOINT_MATCH_EVENTID) \
    OP(WMI_VDEV_TSF_REPORT_EVENTID) \
    OP(WMI_UPDATE_FW_MEM_DUMP_EVENTID) \
    OP(WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID) \
    OP(WMI_RSSI_BREACH_EVENTID) \
    OP(WMI_WOW_INITIAL_WAKEUP_EVENTID) \
    OP(WMI_SOC_SET_HW_MODE_RESP_EVENTID) \
    OP(WMI_SOC_HW_MODE_TRANSITION_EVENTID) \
    OP(WMI_SOC_SET_DUAL_MAC_CONFIG_RESP_EVENTID) \
    OP(WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID) \
    OP(WMI_MGMT_TX_COMPLETION_EVENTID) \
    OP(WMI_MAWC_ENABLE_SENSOR_EVENTID) \
    OP(WMI_PEER_ASSOC_CONF_EVENTID) \
    OP(WMI_AP_PS_EGAP_INFO_EVENTID) \
    OP(WMI_TRANSFER_DATA_TO_FLASH_COMPLETE_EVENTID) \
    OP(WMI_OEM_RESPONSE_EVENTID) \
    OP(WMI_PDEV_UTF_SCPC_EVENTID) \
    OP(WMI_STA_SMPS_FORCE_MODE_COMPLETE_EVENTID) \
    OP(WMI_BPF_CAPABILIY_INFO_EVENTID) \
    OP(WMI_BPF_VDEV_STATS_INFO_EVENTID) \
    OP(WMI_VDEV_DELETE_RESP_EVENTID) \
    OP(WMI_PEER_DELETE_RESP_EVENTID) \
    OP(WMI_PDEV_FIPS_EVENTID) \
    OP(WMI_PDEV_CHANNEL_HOPPING_EVENTID) \
    OP(WMI_PDEV_ANI_CCK_LEVEL_EVENTID) \
    OP(WMI_PDEV_ANI_OFDM_LEVEL_EVENTID) \
    OP(WMI_PDEV_TPC_EVENTID) \
    OP(WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID) \
    OP(WMI_PEER_RATECODE_LIST_EVENTID) \
    OP(WMI_WDS_PEER_EVENTID) \
    OP(WMI_PEER_STA_PS_STATECHG_EVENTID) \
    OP(WMI_INST_RSSI_STATS_EVENTID) \
    OP(WMI_NAN_DISC_IFACE_CREATED_EVENTID) \
    OP(WMI_NAN_DISC_IFACE_DELETED_EVENTID) \
    OP(WMI_NAN_STARTED_CLUSTER_EVENTID) \
    OP(WMI_NAN_JOINED_CLUSTER_EVENTID) \
    OP(WMI_NDI_CAP_RSP_EVENTID) \
    OP(WMI_NDP_INITIATOR_RSP_EVENTID) \
    OP(WMI_NDP_RESPONDER_RSP_EVENTID) \
    OP(WMI_NDP_END_RSP_EVENTID) \
    OP(WMI_NDP_INDICATION_EVENTID) \
    OP(WMI_NDP_CONFIRM_EVENTID) \
    OP(WMI_NDP_END_INDICATION_EVENTID) \
    OP(WMI_PDEV_SET_HW_MODE_RESP_EVENTID) \
    OP(WMI_PDEV_HW_MODE_TRANSITION_EVENTID) \
    OP(WMI_PDEV_SET_MAC_CONFIG_RESP_EVENTID) \
    OP(WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID) \
    OP(WMI_RMC_NEW_LEADER_EVENTID) \
    OP(WMI_P2P_LISTEN_OFFLOAD_STOPPED_EVENTID) \
    OP(WMI_MGMT_TX_BUNDLE_COMPLETION_EVENTID) \
    OP(WMI_READ_DATA_FROM_FLASH_EVENTID) \
    OP(WMI_SERVICE_AVAILABLE_EVENTID) \
    OP(WMI_PEER_ANTDIV_INFO_EVENTID) \
    OP(WMI_PDEV_ANTDIV_STATUS_EVENTID) \
    OP(WMI_PDEV_CHIP_POWER_STATS_EVENTID) \
    OP(WMI_COEX_REPORT_ANTENNA_ISOLATION_EVENTID) \
    OP(WMI_REPORT_STATS_EVENTID) \
    OP(WMI_REPORT_RX_AGGR_FAILURE_EVENTID) \
    OP(WMI_VDEV_ENCRYPT_DECRYPT_DATA_RESP_EVENTID) \
    OP(WMI_TBTTOFFSET_EXT_UPDATE_EVENTID) \
    OP(WMI_PDEV_DFS_RADAR_DETECTION_EVENTID) \
    OP(WMI_VDEV_DFS_CAC_COMPLETE_EVENTID) \
    OP(WMI_VDEV_ADFS_OCAC_COMPLETE_EVENTID) \
    OP(WMI_UPDATE_RCPI_EVENTID) \
    OP(WMI_PEER_STATS_INFO_EVENTID) \
    OP(WMI_PKGID_EVENTID) \
    OP(WMI_REG_CHAN_LIST_CC_EVENTID) \
    OP(WMI_11D_NEW_COUNTRY_EVENTID) \
    OP(WMI_RADIO_CHAN_STATS_EVENTID) \
    OP(WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_STATUS_EVENTID) \
    OP(WMI_PEER_OPER_MODE_CHANGE_EVENTID) \
    OP(WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID) \
    OP(WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID) \
    OP(WMI_PDEV_CHECK_CAL_VERSION_EVENTID) \
    OP(WMI_VDEV_GET_ARP_STAT_EVENTID) \
    OP(WMI_WLAN_COEX_BT_ACTIVITY_EVENTID) \
    OP(WMI_VDEV_GET_TX_POWER_EVENTID) \
    OP(WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID) \
    OP(WMI_THERM_THROT_STATS_EVENTID) \
    OP(WMI_PDEV_DIV_RSSI_ANTID_EVENTID) \
    OP(WMI_OEM_DMA_RING_CFG_RSP_EVENTID) \
    OP(WMI_OEM_DMA_BUF_RELEASE_EVENTID) \
    OP(WMI_OEM_DATA_EVENTID) \
    OP(WMI_PDEV_BSS_CHAN_INFO_EVENTID) \
    OP(WMI_UNIT_TEST_EVENTID) \
    OP(WMI_PDEV_UPDATE_CTLTABLE_EVENTID) \
    OP(WMI_HOST_SWFDA_EVENTID) \
    OP(WMI_NDL_SCHEDULE_UPDATE_EVENTID) \
    OP(WMI_SAP_OBSS_DETECTION_REPORT_EVENTID) \
    OP(WMI_PDEV_DMA_RING_CFG_RSP_EVENTID) \
    OP(WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID) \
    OP(WMI_SAR_GET_LIMITS_EVENTID) \
    OP(WMI_SAR2_RESULT_EVENTID) \
    OP(WMI_BPF_GET_VDEV_WORK_MEMORY_RESP_EVENTID) \
    OP(WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID) \
    OP(WMI_TWT_ENABLE_COMPLETE_EVENTID) \
    OP(WMI_TWT_DISABLE_COMPLETE_EVENTID) \
    OP(WMI_TWT_ADD_DIALOG_COMPLETE_EVENTID) \
    OP(WMI_TWT_DEL_DIALOG_COMPLETE_EVENTID) \
    OP(WMI_TWT_PAUSE_DIALOG_COMPLETE_EVENTID) \
    OP(WMI_TWT_RESUME_DIALOG_COMPLETE_EVENTID) \
    OP(WMI_ROAM_SCAN_STATS_EVENTID) \
    OP(WMI_GET_TPC_POWER_EVENTID) \
    OP(WMI_MOTION_DET_HOST_EVENTID) \
    OP(WMI_MOTION_DET_BASE_LINE_HOST_EVENTID) \
    OP(WMI_ESP_ESTIMATE_EVENTID) \
    OP(WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID) \
    OP(WMI_NDP_EVENTID) \
    OP(WMI_VDEV_BCN_RECEPTION_STATS_EVENTID) \
    OP(WMI_PEER_TX_PN_RESPONSE_EVENTID) \
    OP(WMI_PDEV_CSC_SWITCH_COUNT_STATUS_EVENTID) \
    OP(WMI_ROAM_BLACKLIST_EVENTID) \
    OP(WMI_VDEV_GET_MWS_COEX_STATE_EVENTID) \
    OP(WMI_VDEV_GET_MWS_COEX_DPWB_STATE_EVENTID) \
    OP(WMI_VDEV_GET_MWS_COEX_TDM_STATE_EVENTID) \
    OP(WMI_VDEV_GET_MWS_COEX_IDRX_STATE_EVENTID) \
    OP(WMI_VDEV_GET_MWS_COEX_ANTENNA_SHARING_STATE_EVENTID) \
    OP(WMI_WLM_STATS_EVENTID) \
    OP(WMI_PEER_CFR_CAPTURE_EVENTID) \
    OP(WMI_PDEV_COLD_BOOT_CAL_DATA_EVENTID) \
    OP(WMI_PDEV_RAP_INFO_EVENTID) \
    OP(WMI_IFACE_COMBINATION_IND_EVENTID) \
    OP(WMI_VDEV_MGMT_OFFLOAD_EVENTID) \
    OP(WMI_MUEDCA_PARAMS_CONFIG_EVENTID) \
    OP(WMI_TWT_BTWT_INVITE_STA_COMPLETE_EVENTID) \
    OP(WMI_TWT_BTWT_REMOVE_STA_COMPLETE_EVENTID) \
    OP(WMI_VDEV_DELETE_ALL_PEER_RESP_EVENTID) \
    OP(WMI_CHAN_RF_CHARACTERIZATION_INFO_EVENTID) \
    OP(WMI_ROAM_PREAUTH_START_EVENTID) \
    OP(WMI_GET_ELNA_BYPASS_EVENTID) \
    OP(WMI_ROAM_PMKID_REQUEST_EVENTID) \
    OP(WMI_GET_CHANNEL_ANI_EVENTID) \
    OP(WMI_ROAM_STATS_EVENTID) \
    OP(WMI_VDEV_AUDIO_SYNC_START_STOP_EVENTID) \
    OP(WMI_VDEV_AUDIO_SYNC_Q_MASTER_SLAVE_OFFSET_EVENTID) \
    OP(WMI_PEER_CREATE_CONF_EVENTID) \
    OP(WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID) \
    OP(WMI_ROAM_SCAN_CHANNEL_LIST_EVENTID) \
    OP(WMI_VDEV_SEND_BIG_DATA_EVENTID) \
    OP(WMI_NAN_DMESG_EVENTID) \
    OP(WMI_VDEV_SEND_BIG_DATA_P2_EVENTID) \
    OP(WMI_AUDIO_AGGR_REPORT_STATISTICS_EVENTID) \
    OP(WMI_PDEV_SSCAN_FW_PARAM_EVENTID) \
    OP(WMI_ROAM_CAPABILITY_REPORT_EVENTID) \
    OP(WMI_CTRL_PATH_STATS_EVENTID) \
    OP(WMI_PDEV_GET_TPC_STATS_EVENTID) \
    OP(WMI_VDEV_BCN_LATENCY_EVENTID) \
    OP(WMI_AUDIO_AGGR_SCHED_METHOD_EVENTID) \
    OP(WMI_VDEV_DISCONNECT_EVENTID) \
    OP(WMI_TWT_SESSION_STATS_EVENTID) \
    OP(WMI_SSCAN_EVT_MESSAGE_EVENTID) \
    OP(WMI_QOS_NULL_FRAME_TX_COMPLETION_EVENTID) \
    /* add new EVT_LIST elements above this line */


/* TLV definitions of WMI commands */

/* Init Cmd */
#define WMITLV_TABLE_WMI_INIT_CMDID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_init_cmd_fixed_param, wmi_init_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resource_config, wmi_resource_config, resource_config, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_host_memory_chunk, host_mem_chunks, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_hw_mode_cmd_fixed_param, wmi_pdev_set_hw_mode_cmd_fixed_param, hw_mode, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_band_to_mac, band_to_mac, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_INIT_CMDID);

/* Peer create Cmd */
#define WMITLV_TABLE_WMI_PEER_CREATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_create_cmd_fixed_param, wmi_peer_create_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CREATE_CMDID);

/* Peer delete Cmd */
#define WMITLV_TABLE_WMI_PEER_DELETE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_delete_cmd_fixed_param, wmi_peer_delete_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_DELETE_CMDID);

/* Delete all peer Cmd */
#define WMITLV_TABLE_WMI_VDEV_DELETE_ALL_PEER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_delete_all_peer_cmd_fixed_param, wmi_vdev_delete_all_peer_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DELETE_ALL_PEER_CMDID);

/* Peer unmap response Cmd */
#define WMITLV_TABLE_WMI_PEER_UNMAP_RESPONSE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_unmap_response_cmd_fixed_param, wmi_peer_unmap_response_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, peer_ids, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_UNMAP_RESPONSE_CMDID);

/* Peer flush Cmd */
#define WMITLV_TABLE_WMI_PEER_FLUSH_TIDS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_flush_tids_cmd_fixed_param, wmi_peer_flush_tids_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_FLUSH_TIDS_CMDID);

/* Peer Set Param Cmd */
#define WMITLV_TABLE_WMI_PEER_SET_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_set_param_cmd_fixed_param, wmi_peer_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SET_PARAM_CMDID);

/* STA Powersave Mode Cmd */
#define WMITLV_TABLE_WMI_STA_POWERSAVE_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_powersave_mode_cmd_fixed_param, wmi_sta_powersave_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_POWERSAVE_MODE_CMDID);

/* STA Powersave Param Cmd */
#define WMITLV_TABLE_WMI_STA_POWERSAVE_PARAM_CMDID(id,op,buf,len) \
            WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_powersave_param_cmd_fixed_param, wmi_sta_powersave_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_POWERSAVE_PARAM_CMDID);

/* STA TDCC Param Config Cmd */
#define WMITLV_TABLE_WMI_STA_TDCC_CONFIG_CMDID(id,op,buf,len) \
            WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_tdcc_config_cmd_fixed_param, wmi_sta_tdcc_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_TDCC_CONFIG_CMDID);

/* STA DTIM PS METHOD Cmd */
#define WMITLV_TABLE_WMI_STA_DTIM_PS_METHOD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_dtim_ps_method_cmd_fixed_param, wmi_sta_dtim_ps_method_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_DTIM_PS_METHOD_CMDID);

/* Pdev Set Reg Domain Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_REGDOMAIN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_regdomain_cmd_fixed_param, wmi_pdev_set_regdomain_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_REGDOMAIN_CMDID);


/* Peer TID ADD BA Cmd */
#define WMITLV_TABLE_WMI_PEER_TID_ADDBA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tid_addba_cmd_fixed_param, wmi_peer_tid_addba_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TID_ADDBA_CMDID);

/* Peer TID DEL BA Cmd */
#define WMITLV_TABLE_WMI_PEER_TID_DELBA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tid_delba_cmd_fixed_param, wmi_peer_tid_delba_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TID_DELBA_CMDID);

/* Peer Req Add BA Ssn for staId/tid pair Cmd */
#define WMITLV_TABLE_WMI_BA_REQ_SSN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ba_req_ssn_cmd_fixed_param, wmi_ba_req_ssn_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_ARRAY_STRUC, wmi_ba_req_ssn, ba_req_ssn_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_BA_REQ_SSN_CMDID);


/* PDEV FTM integration Cmd */
#define WMITLV_TABLE_WMI_PDEV_FTM_INTG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ftm_intg_cmd_fixed_param, wmi_ftm_intg_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FTM_INTG_CMDID);

/* WOW Wakeup from sleep Cmd */
#define WMITLV_TABLE_WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_hostwakeup_from_sleep_cmd_fixed_param, wmi_wow_hostwakeup_from_sleep_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);

/* WOW Enable Cmd */
#define WMITLV_TABLE_WMI_WOW_ENABLE_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_enable_cmd_fixed_param, wmi_wow_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ENABLE_CMDID);

/* WOW ICMPv6 NA filtering command */
#define WMITLV_TABLE_WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_enable_icmpv6_na_flt_cmd_fixed_param, wmi_wow_enable_icmpv6_na_flt_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID);

/* Remove Bcn Filter Cmd */
#define WMITLV_TABLE_WMI_RMV_BCN_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmv_bcn_filter_cmd_fixed_param, wmi_rmv_bcn_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMV_BCN_FILTER_CMDID);

/*  Offload 11k related requests */
#define WMITLV_TABLE_WMI_11K_OFFLOAD_REPORT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offload_11k_report_fixed_param, wmi_11k_offload_report_fixed_param,fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_neighbor_report_11k_offload_tlv_param, offload_neighbor_report_param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_11K_OFFLOAD_REPORT_CMDID);

/* invoke 11k neighbor report req from FW */
#define WMITLV_TABLE_WMI_11K_INVOKE_NEIGHBOR_REPORT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_invoke_neighbor_report_fixed_param,wmi_11k_offload_invoke_neighbor_report_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_11K_INVOKE_NEIGHBOR_REPORT_CMDID);

/** Service bit WMI_SERVICE_ROAM_OFFLOAD for Roaming feature */
/* Roam scan mode Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_MODE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_mode_fixed_param, wmi_roam_scan_mode_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param, wmi_start_scan_cmd_fixed_param, scan_params, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_offload_tlv_param, offload_param, WMITLV_SIZE_VAR)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_11i_offload_tlv_param, offload_11i_param, WMITLV_SIZE_VAR)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_11r_offload_tlv_param, offload_11r_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_ese_offload_tlv_param, offload_ese_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tlv_buf_len_param, assoc_ie_len_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, assoc_ie_buf, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_fils_offload_tlv_param, offload_fils_info_param, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_MODE);

/* Roam scan Rssi Threshold Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_RSSI_THRESHOLD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_rssi_threshold_fixed_param, wmi_roam_scan_rssi_threshold_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_scan_extended_threshold_param, extended_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_earlystop_rssi_thres_param, earlystop_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_dense_thres_param, dense_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_bg_scan_roaming_param, bg_scan_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_data_rssi_roaming_param, data_rssi_param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_RSSI_THRESHOLD);

/* Roam Scan Period Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_PERIOD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_period_fixed_param, wmi_roam_scan_period_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_PERIOD);

/* Roam scan change Rssi Threshold Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_rssi_change_threshold_fixed_param, wmi_roam_scan_rssi_change_threshold_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD);

/* Roam Scan Channel list Cmd */
#define WMITLV_TABLE_WMI_ROAM_CHAN_LIST(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_chan_list_fixed_param, wmi_roam_chan_list_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_CHAN_LIST);

/* Roam scan mode Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_CMD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_cmd_fixed_param, wmi_roam_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_CMD);

/* Roam mbo configuration */
#define WMITLV_TABLE_WMI_ROAM_SET_MBO_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_set_mbo_fixed_param, wmi_roam_set_mbo_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, cellular_cap, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_supported_operating_class_param, supp_op_class_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, supp_op_class_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_mbo_non_preferred_channel_report_param, non_prefer_ch_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, non_prefer_ch_attr, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SET_MBO_PARAM_CMDID);

/* Roam PER configure cmd */
#define WMITLV_TABLE_WMI_ROAM_PER_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_per_config_fixed_param, wmi_roam_per_config_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_PER_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_VDEV_PLMREQ_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_plmreq_start_cmd_fixed_param, wmi_vdev_plmreq_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_PLMREQ_START_CMDID);

#define WMITLV_TABLE_WMI_VDEV_PLMREQ_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_plmreq_stop_cmd_fixed_param, wmi_vdev_plmreq_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_PLMREQ_STOP_CMDID);
/* Start scan Cmd */
#define WMITLV_TABLE_WMI_START_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param, wmi_start_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vendor_oui, vendor_oui, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, phymode_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_hint_freq_short_ssid, hint_freq_short_ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_hint_freq_bssid, hint_freq_bssid_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_START_SCAN_CMDID);

/* Scan adaptive dwell mode configuration */
#define WMITLV_TABLE_WMI_SCAN_ADAPTIVE_DWELL_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_adaptive_dwell_config_fixed_param, wmi_scan_adaptive_dwell_config_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_scan_adaptive_dwell_parameters_tlv, param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_ADAPTIVE_DWELL_CONFIG_CMDID);

/* Set scan selection duty cycle */
#define WMITLV_TABLE_WMI_SET_SCAN_DBS_DUTY_CYCLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_dbs_duty_cycle_fixed_param, wmi_scan_dbs_duty_cycle_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_scan_dbs_duty_cycle_tlv_param, param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SET_SCAN_DBS_DUTY_CYCLE_CMDID);

/* Start ExtScan Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_start_cmd_fixed_param, wmi_extscan_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_bucket, bucket_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_bucket_channel, channel_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_START_CMDID);

/* Stop ExtScan Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_stop_cmd_fixed_param, wmi_extscan_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_STOP_CMDID);

/* Start ExtScan BSSID Monitoring Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param, wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_bssid_param, wlan_change_descriptor_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID);

/* Start Hot List Monitoring Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_monitor_cmd_fixed_param, wmi_extscan_configure_hotlist_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_entry, hotlist, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID);

/* Get ExtScan BSSID/RSSI list Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_get_cached_results_cmd_fixed_param, wmi_extscan_get_cached_results_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID);

/* Get ExtScan BSSID monitor results Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_get_wlan_change_results_cmd_fixed_param, wmi_extscan_get_wlan_change_results_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID);

/* Set ExtScan Capabilities Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_SET_CAPABILITIES_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_set_capabilities_cmd_fixed_param, wmi_extscan_set_capabilities_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_cache_capabilities, extscan_cache_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_monitor_capabilities, wlan_change_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_monitor_capabilities, hotlist_capabilities, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_SET_CAPABILITIES_CMDID);

/* Get ExtScan Capabilities Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_GET_CAPABILITIES_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_get_capabilities_cmd_fixed_param, wmi_extscan_get_capabilities_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_GET_CAPABILITIES_CMDID);

/* Start SSID Hot List Monitoring Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param, wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_ssid_entry, hotlist_ssid, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID);

/* P2P set vendor ID data Cmd */
#define WMITLV_TABLE_WMI_P2P_SET_VENDOR_IE_DATA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_set_vendor_ie_data_cmd_fixed_param, wmi_p2p_set_vendor_ie_data_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_SET_VENDOR_IE_DATA_CMDID);

/* P2P set OppPS parameters Cmd */
#define WMITLV_TABLE_WMI_P2P_SET_OPPPS_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_set_oppps_cmd_fixed_param, wmi_p2p_set_oppps_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_SET_OPPPS_PARAM_CMDID);

/* P2P set listen offload start parameters Cmd */
#define WMITLV_TABLE_WMI_P2P_LISTEN_OFFLOAD_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_lo_start_cmd_fixed_param, wmi_p2p_lo_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, device_types_data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, prob_resp_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_LISTEN_OFFLOAD_START_CMDID);

/* P2P set listen offload stop parameters Cmd */
#define WMITLV_TABLE_WMI_P2P_LISTEN_OFFLOAD_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_lo_stop_cmd_fixed_param, wmi_p2p_lo_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_LISTEN_OFFLOAD_STOP_CMDID);

/* P2P set listen offload stopped parameters Event */
#define WMITLV_TABLE_WMI_P2P_LISTEN_OFFLOAD_STOPPED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_lo_stopped_event_fixed_param, wmi_p2p_lo_stopped_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_LISTEN_OFFLOAD_STOPPED_EVENTID);

/* Pdev set channel Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_CHANNEL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_CHANNEL_CMDID);

#define WMITLV_TABLE_WMI_PDEV_UPDATE_PMK_CACHE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_update_pmk_cache_cmd_fixed_param, wmi_pdev_update_pmk_cache_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pmk_cache, pmk_cache, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UPDATE_PMK_CACHE_CMDID);

#define WMITLV_TABLE_WMI_PDEV_UPDATE_FILS_HLP_PKT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param, wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, fils_hlp_pkt, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UPDATE_FILS_HLP_PKT_CMDID);
/* Echo Cmd */
#define WMITLV_TABLE_WMI_ECHO_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_echo_cmd_fixed_param, wmi_echo_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ECHO_CMDID);

/* Pdev set wmm params */
#define WMITLV_TABLE_WMI_PDEV_SET_WMM_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_wmm_params_cmd_fixed_param, wmi_pdev_set_wmm_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_be, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_bk, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_vi, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_vo, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_WMM_PARAMS_CMDID);

/* Vdev start request Cmd */
#define WMITLV_TABLE_WMI_VDEV_START_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param, wmi_vdev_start_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_descriptor, noa_descriptors, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_START_REQUEST_CMDID);

/* Vdev restart request cmd */
#define WMITLV_TABLE_WMI_VDEV_RESTART_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param, wmi_vdev_start_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_descriptor, noa_descriptors, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_RESTART_REQUEST_CMDID);

/* P2P Go set beacon IE cmd */
#define WMITLV_TABLE_WMI_P2P_GO_SET_BEACON_IE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_go_set_beacon_ie_fixed_param, wmi_p2p_go_set_beacon_ie_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_P2P_GO_SET_BEACON_IE);

/* GTK offload Cmd */
#define WMITLV_TABLE_WMI_GTK_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_CMD_fixed_param, WMI_GTK_OFFLOAD_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_gtk_offload_fils_tlv_param, wmi_fils_gtk_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_GTK_OFFLOAD_CMDID);

/* PMF 11w offload Set SA query cmd */
#define WMITLV_TABLE_WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_PMF_OFFLOAD_SET_SA_QUERY_CMD_fixed_param, WMI_PMF_OFFLOAD_SET_SA_QUERY_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID);

/* Scan channel list Cmd */
#define WMITLV_TABLE_WMI_SCAN_CHAN_LIST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_chan_list_cmd_fixed_param, wmi_scan_chan_list_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, chan_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_CHAN_LIST_CMDID);

/* STA UAPSD Auto trigger Cmd */
#define WMITLV_TABLE_WMI_STA_UAPSD_AUTO_TRIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_cmd_fixed_param, wmi_sta_uapsd_auto_trig_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_sta_uapsd_auto_trig_param, ac_param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_UAPSD_AUTO_TRIG_CMDID);

/* Probe template Cmd */
#define WMITLV_TABLE_WMI_PRB_TMPL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_prb_tmpl_cmd_fixed_param, wmi_prb_tmpl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_prb_info, wmi_bcn_prb_info, bcn_prb_info, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PRB_TMPL_CMDID);

/* Beacon template Cmd */
#define WMITLV_TABLE_WMI_BCN_TMPL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_tmpl_cmd_fixed_param, wmi_bcn_tmpl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_prb_info, wmi_bcn_prb_info, bcn_prb_info, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_BCN_TMPL_CMDID);

/* FILS Discovery template Cmd */
#define WMITLV_TABLE_WMI_FD_TMPL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_fd_tmpl_cmd_fixed_param, wmi_fd_tmpl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_FD_TMPL_CMDID);

/* VDEV install key complete Cmd */
#define WMITLV_TABLE_WMI_VDEV_INSTALL_KEY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_install_key_cmd_fixed_param, wmi_vdev_install_key_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, key_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_INSTALL_KEY_CMDID);

/* VDEV WNM SLEEP MODE Cmd */
#define WMITLV_TABLE_WMI_VDEV_WNM_SLEEPMODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_VDEV_WNM_SLEEPMODE_CMD_fixed_param, WMI_VDEV_WNM_SLEEPMODE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WNM_SLEEPMODE_CMDID);

#define WMITLV_TABLE_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param, WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID);

/* Peer Assoc Cmd */
#define WMITLV_TABLE_WMI_PEER_ASSOC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_assoc_complete_cmd_fixed_param, wmi_peer_assoc_complete_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, peer_legacy_rates, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, peer_ht_rates, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vht_rate_set, wmi_vht_rate_set, peer_vht_rates, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_he_rate_set, peer_he_rates, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ASSOC_CMDID);

/* Peer Set Rate Report Condition Cmd */
#define WMITLV_TABLE_WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_set_rate_report_condition_fixed_param, wmi_peer_set_rate_report_condition_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID);

/* Add Beacon filter Cmd */
#define WMITLV_TABLE_WMI_ADD_BCN_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_add_bcn_filter_cmd_fixed_param, wmi_add_bcn_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ie_map, WMITLV_SIZE_FIX, BCN_FLT_MAX_ELEMS_IE_LIST)

WMITLV_CREATE_PARAM_STRUC(WMI_ADD_BCN_FILTER_CMDID);

/* Sta keepalive cmd */
#define WMITLV_TABLE_WMI_STA_KEEPALIVE_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_STA_KEEPALIVE_CMD_fixed_param, WMI_STA_KEEPALIVE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_STA_KEEPALVE_ARP_RESPONSE, WMI_STA_KEEPALVE_ARP_RESPONSE, arp_resp, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_KEEPALIVE_CMDID);

/* Bss Max idle time cmd */
#define WMITLV_TABLE_WMI_VDEV_BSS_MAX_IDLE_TIME_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_bss_max_idle_time_cmd_fixed_param, wmi_vdev_bss_max_idle_time_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_BSS_MAX_IDLE_TIME_CMDID);

/* ARP NS offload Cmd */
#define WMITLV_TABLE_WMI_SET_ARP_NS_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param, WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_NS_OFFLOAD_TUPLE, ns_tuples, WMITLV_SIZE_FIX, WMI_MAX_NS_OFFLOADS) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_ARP_OFFLOAD_TUPLE, arp_tuples, WMITLV_SIZE_FIX, WMI_MAX_ARP_OFFLOADS) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_NS_OFFLOAD_TUPLE, ns_ext_tuples, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SET_ARP_NS_OFFLOAD_CMDID);

/* AP PS peer param Cmd */
#define WMITLV_TABLE_WMI_AP_PS_PEER_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ap_ps_peer_cmd_fixed_param, wmi_ap_ps_peer_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AP_PS_PEER_PARAM_CMDID);

/* AP PS enhanced green ap param Cmd */
#define WMITLV_TABLE_WMI_AP_PS_EGAP_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ap_ps_egap_param_cmd_fixed_param, wmi_ap_ps_egap_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AP_PS_EGAP_PARAM_CMDID);

/* Profile Trigger Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_TRIGGER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_trigger_cmd_fixed_param, wmi_wlan_profile_trigger_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_TRIGGER_CMDID);

/* WLAN Profile set hist interval Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_set_hist_intvl_cmd_fixed_param, wmi_wlan_profile_set_hist_intvl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID);

/* WLAN Profile get profile data Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_get_prof_data_cmd_fixed_param, wmi_wlan_profile_get_prof_data_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID);

/* WLAN Profile enable profile ID Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_enable_profile_id_cmd_fixed_param, wmi_wlan_profile_enable_profile_id_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID);

/* WOW Delete Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_DEL_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_DEL_PATTERN_CMD_fixed_param, WMI_WOW_DEL_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_DEL_WAKE_PATTERN_CMDID);

#define WMITLV_TABLE_WMI_WOW_UDP_SVC_OFLD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_UDP_SVC_OFLD_CMD_fixed_param, WMI_WOW_UDP_SVC_OFLD_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, pattern, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, response, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_UDP_SVC_OFLD_CMDID);

#define WMITLV_TABLE_WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_hostwakeup_gpio_pin_pattern_config_cmd_fixed_param, WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_WOW_SET_ACTION_WAKE_UP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_set_action_wake_up_cmd_fixed_param, WMI_WOW_SET_ACTION_WAKE_UP_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, action_bitmaps_per_category, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_SET_ACTION_WAKE_UP_CMDID);

/* Wow enable/disable wake up Cmd */
#define WMITLV_TABLE_WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_ADD_DEL_EVT_CMD_fixed_param, WMI_WOW_ADD_DEL_EVT_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);

/* RTT measurement request Cmd */
#define WMITLV_TABLE_WMI_RTT_MEASREQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)


WMITLV_CREATE_PARAM_STRUC(WMI_RTT_MEASREQ_CMDID);

/* RTT TSF Cmd */
#define WMITLV_TABLE_WMI_RTT_TSF_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RTT_TSF_CMDID);

/* RTT OEM req Cmd */
#define WMITLV_TABLE_WMI_OEM_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_OEM_REQ_CMDID);

/* RTT OEM request Cmd - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_OEM_REQUEST_CMDID);

/* RTT OEM req Cmd through LPASS */
#define WMITLV_TABLE_WMI_LPI_OEM_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_OEM_REQ_CMDID);

#define WMITLV_TABLE_WMI_OEM_DMA_RING_CFG_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oem_dma_ring_cfg_req_fixed_param, wmi_oem_dma_ring_cfg_req_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_OEM_DMA_RING_CFG_REQ_CMDID);

/* Spectral scan configure Cmd */
#define WMITLV_TABLE_WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_spectral_configure_cmd_fixed_param, wmi_vdev_spectral_configure_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);

/* Spectral scan enable Cmd */
#define WMITLV_TABLE_WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_spectral_enable_cmd_fixed_param, wmi_vdev_spectral_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);

/* Request stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param, wmi_request_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_inst_rssi_stats_params, inst_rssi_params, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_STATS_CMDID);

/* PN Request Cmd */
#define WMITLV_TABLE_WMI_PEER_TX_PN_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tx_pn_request_cmd_fixed_param, wmi_peer_tx_pn_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TX_PN_REQUEST_CMDID);

/* Request for memory dump stats Cmd */
#define WMITLV_TABLE_WMI_GET_FW_MEM_DUMP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_fw_mem_dump_fixed_param, wmi_get_fw_mem_dump_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_fw_mem_dump, fw_mem_dump_params, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_GET_FW_MEM_DUMP_CMDID);

/* flush debug messages */
#define WMITLV_TABLE_WMI_DEBUG_MESG_FLUSH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_debug_mesg_flush_fixed_param, wmi_debug_mesg_flush_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_MESG_FLUSH_CMDID);

/* Request to config the DIAG Events and LOGs*/
#define WMITLV_TABLE_WMI_DIAG_EVENT_LOG_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_diag_event_log_config_fixed_param, wmi_diag_event_log_config_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, diag_events_logs_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_EVENT_LOG_CONFIG_CMDID);

/* Set config params */
#define WMITLV_TABLE_WMI_START_LINK_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_start_link_stats_cmd_fixed_param, wmi_start_link_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_START_LINK_STATS_CMDID);

/* Request to clear link stats */
#define WMITLV_TABLE_WMI_CLEAR_LINK_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_clear_link_stats_cmd_fixed_param, wmi_clear_link_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CLEAR_LINK_STATS_CMDID);

/* Request stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_LINK_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_link_stats_cmd_fixed_param, wmi_request_link_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_LINK_STATS_CMDID);

/* Request for unified Link Layer and get station stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_UNIFIED_LL_GET_STA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_unified_ll_get_sta_cmd_fixed_param, wmi_request_unified_ll_get_sta_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_inst_rssi_stats_params, inst_rssi_params, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_UNIFIED_LL_GET_STA_CMDID);

/* Request wlm stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_WLM_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_wlm_stats_cmd_fixed_param, wmi_request_wlm_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_WLM_STATS_CMDID);

/* Network list offload config Cmd */
#define WMITLV_TABLE_WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param, wmi_nlo_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, nlo_configured_parameters, nlo_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, nlo_channel_prediction_cfg, channel_prediction_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_enlo_candidate_score_param, enlo_candidate_score_params, candidate_score_params, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vendor_oui, vendor_oui, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_connected_nlo_rssi_params, connected_nlo_rssi_params, cnlo_rssi_params, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, connected_nlo_bss_band_rssi_pref, cnlo_bss_band_rssi_pref, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);

/* Passpoint list offload config Cmd */
#define WMITLV_TABLE_WMI_PASSPOINT_LIST_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_passpoint_config_cmd_fixed_param, wmi_passpoint_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PASSPOINT_LIST_CONFIG_CMDID);

/* CSA offload enable Cmd */
#define WMITLV_TABLE_WMI_CSA_OFFLOAD_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_csa_offload_enable_cmd_fixed_param, wmi_csa_offload_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CSA_OFFLOAD_ENABLE_CMDID);

/* CSA offload channel switch Cmd */
#define WMITLV_TABLE_WMI_CSA_OFFLOAD_CHANSWITCH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_csa_offload_chanswitch_cmd_fixed_param, wmi_csa_offload_chanswitch_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CSA_OFFLOAD_CHANSWITCH_CMDID);

/* Chatter set mode Cmd */
#define WMITLV_TABLE_WMI_CHATTER_SET_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_set_mode_cmd_fixed_param, wmi_chatter_set_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_SET_MODE_CMDID);


/* PDEV UTF Cmd */
#define WMITLV_TABLE_WMI_PDEV_UTF_CMDID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UTF_CMDID);

/* PDEV QVIT Cmd */
#define WMITLV_TABLE_WMI_PDEV_QVIT_CMDID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_QVIT_CMDID);

#define WMITLV_TABLE_WMI_PDEV_SET_WAKEUP_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_PDEV_SET_WAKEUP_CONFIG_CMDID_fixed_param, WMI_PDEV_SET_WAKEUP_CONFIG_CMDID_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_WAKEUP_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_PDEV_DMA_RING_CFG_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dma_ring_cfg_req_fixed_param, wmi_dma_ring_cfg_req_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DMA_RING_CFG_REQ_CMDID);

/* Vdev Set keep alive Cmd  */
#define WMITLV_TABLE_WMI_VDEV_SET_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_keepalive_cmd_fixed_param, wmi_vdev_set_keepalive_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_KEEPALIVE_CMDID);

/* Vdev Get keep alive Cmd  */
#define WMITLV_TABLE_WMI_VDEV_GET_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_cmd_fixed_param, wmi_vdev_get_keepalive_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_KEEPALIVE_CMDID);

/* Vdev cmd to enable/disable fw offloaded beacons. */
#define WMITLV_TABLE_WMI_BCN_OFFLOAD_CTRL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_offload_ctrl_cmd_fixed_param, wmi_bcn_offload_ctrl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BCN_OFFLOAD_CTRL_CMDID);

/* FWTEST Set TBTT mode Cmd */
#define WMITLV_TABLE_WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_mcc_set_tbtt_mode_cmd_fixed_param, wmi_vdev_mcc_set_tbtt_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID);

/* FWTEST set NoA parameters Cmd */
#define WMITLV_TABLE_WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_set_noa_cmd_fixed_param, wmi_p2p_set_noa_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_descriptor, noa_descriptor, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID);

/* Unit test FW */
#define WMITLV_TABLE_WMI_UNIT_TEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param, wmi_unit_test_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, args, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UNIT_TEST_CMDID);

/* Force Fw Hang Cmd */
#define WMITLV_TABLE_WMI_FORCE_FW_HANG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_FORCE_FW_HANG_CMD_fixed_param, WMI_FORCE_FW_HANG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_FORCE_FW_HANG_CMDID);

/* Set Mcast address Cmd */
#define WMITLV_TABLE_WMI_SET_MCASTBCAST_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param, WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_MCASTBCAST_FILTER_CMDID);

/* Enhanced Mcast add/delete filter list cmd */
#define WMITLV_TABLE_WMI_SET_MULTIPLE_MCAST_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_set_multiple_mcast_filter_cmd_fixed_param, WMI_SET_MULTIPLE_MCAST_FILTER_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, mcast_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_MULTIPLE_MCAST_FILTER_CMDID);

/* Set dbglog time stamp sync cmd */
#define WMITLV_TABLE_WMI_DBGLOG_TIME_STAMP_SYNC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dbglog_time_stamp_sync_cmd_fixed_param, WMI_DBGLOG_TIME_STAMP_SYNC_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DBGLOG_TIME_STAMP_SYNC_CMDID);

/* GPIO config Cmd */
#define WMITLV_TABLE_WMI_GPIO_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gpio_config_cmd_fixed_param, wmi_gpio_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GPIO_CONFIG_CMDID);

/* GPIO output Cmd */
#define WMITLV_TABLE_WMI_GPIO_OUTPUT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gpio_output_cmd_fixed_param, wmi_gpio_output_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GPIO_OUTPUT_CMDID);

/* Antenna Controller config Cmd */
#define WMITLV_TABLE_WMI_ANT_CONTROLLER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ant_controller_cmd_fixed_param, wmi_ant_controller_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ANT_CONTROLLER_CMDID);

/* Peer add WDA entry Cmd */
#define WMITLV_TABLE_WMI_PEER_ADD_WDS_ENTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_add_wds_entry_cmd_fixed_param, wmi_peer_add_wds_entry_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ADD_WDS_ENTRY_CMDID);

/*Peer remove WDS entry Cmd */
#define WMITLV_TABLE_WMI_PEER_REMOVE_WDS_ENTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_remove_wds_entry_cmd_fixed_param, wmi_peer_remove_wds_entry_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_REMOVE_WDS_ENTRY_CMDID);

/* Beacon tx Cmd */
#define WMITLV_TABLE_WMI_BCN_TX_CMDID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_tx_hdr, wmi_bcn_tx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BCN_TX_CMDID);

/* PDEV send Beacon Cmd */
#define WMITLV_TABLE_WMI_PDEV_SEND_BCN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_send_from_host_cmd_fixed_param, wmi_bcn_send_from_host_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SEND_BCN_CMDID);

/* Management tx Cmd */
#define WMITLV_TABLE_WMI_MGMT_TX_CMDID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_hdr, wmi_mgmt_tx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_CMDID);

/* Management tx send cmd */
#define WMITLV_TABLE_WMI_MGMT_TX_SEND_CMDID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_send_cmd_fixed_param, wmi_mgmt_tx_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_send_params, wmi_tx_send_params, tx_send_params, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_SEND_CMDID);

/* offchan data tx send cmd */
#define WMITLV_TABLE_WMI_OFFCHAN_DATA_TX_SEND_CMDID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offchan_data_tx_send_cmd_fixed_param, wmi_offchan_data_tx_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_send_params, wmi_tx_send_params, tx_send_params, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_OFFCHAN_DATA_TX_SEND_CMDID);

/* QoS null tx send cmd */
#define WMITLV_TABLE_WMI_QOS_NULL_FRAME_TX_SEND_CMDID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_qos_null_frame_tx_send_cmd_fixed_param, wmi_qos_null_frame_tx_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_send_params, wmi_tx_send_params, tx_send_params, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_QOS_NULL_FRAME_TX_SEND_CMDID);

/** PDEV send FILS Discovery frame cmd */
#define WMITLV_TABLE_WMI_PDEV_SEND_FD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_fd_send_from_host_cmd_fixed_param, wmi_fd_send_from_host_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SEND_FD_CMDID);

/** Enable or Disable Fast Initial Link Setup (FILS) feature cmd */
#define WMITLV_TABLE_WMI_ENABLE_FILS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_enable_fils_cmd_fixed_param, wmi_enable_fils_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ENABLE_FILS_CMDID);

/* ADD clear response Cmd */
#define WMITLV_TABLE_WMI_ADDBA_CLEAR_RESP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_addba_clear_resp_cmd_fixed_param, wmi_addba_clear_resp_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ADDBA_CLEAR_RESP_CMDID);

/* ADD BA send Cmd */
#define WMITLV_TABLE_WMI_ADDBA_SEND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_addba_send_cmd_fixed_param, wmi_addba_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ADDBA_SEND_CMDID);

/* DEL BA send Cmd */
#define WMITLV_TABLE_WMI_DELBA_SEND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_delba_send_cmd_fixed_param, wmi_delba_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DELBA_SEND_CMDID);

/* ADD BA set response Cmd */
#define WMITLV_TABLE_WMI_ADDBA_SET_RESP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_addba_setresponse_cmd_fixed_param, wmi_addba_setresponse_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ADDBA_SET_RESP_CMDID);

/* Send single AMSDU Cmd */
#define WMITLV_TABLE_WMI_SEND_SINGLEAMSDU_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_send_singleamsdu_cmd_fixed_param, wmi_send_singleamsdu_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_SEND_SINGLEAMSDU_CMDID);

/* PDev Packet Log enable Cmd */
#define WMITLV_TABLE_WMI_PDEV_PKTLOG_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_pktlog_enable_cmd_fixed_param, wmi_pdev_pktlog_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_PKTLOG_ENABLE_CMDID);

/* PDev Packet Log disable Cmd */
#define WMITLV_TABLE_WMI_PDEV_PKTLOG_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_pktlog_disable_cmd_fixed_param, wmi_pdev_pktlog_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_PKTLOG_DISABLE_CMDID);

/* PDev set HT Cap IE Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_HT_CAP_IE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_ht_ie_cmd_fixed_param, wmi_pdev_set_ht_ie_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_HT_CAP_IE_CMDID);

/* PDev set VHT Cap IE Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_VHT_CAP_IE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_vht_ie_cmd_fixed_param, wmi_pdev_set_vht_ie_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_VHT_CAP_IE_CMDID);

/* PDev Set DSCP to TID map Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_DSCP_TID_MAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_dscp_tid_map_cmd_fixed_param, wmi_pdev_set_dscp_tid_map_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_DSCP_TID_MAP_CMDID);

/* PDev Green AP PS enable Cmd */
#define WMITLV_TABLE_WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_green_ap_ps_enable_cmd_fixed_param, wmi_pdev_green_ap_ps_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID);

/* PDEV Get TPC Config Cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_TPC_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_tpc_config_cmd_fixed_param, wmi_pdev_get_tpc_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TPC_CONFIG_CMDID);

/* PDEV Get Antenna diversity chains rssi and antenna index Cmd */
#define WMITLV_TABLE_WMI_PDEV_DIV_GET_RSSI_ANTID_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_div_get_rssi_antid_fixed_param,  wmi_pdev_div_get_rssi_antid_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DIV_GET_RSSI_ANTID_CMDID);

#define WMITLV_TABLE_WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_bss_chan_info_request_fixed_param,  wmi_pdev_bss_chan_info_request_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID);

/* PDEV update ctl table Cmd */
#define WMITLV_TABLE_WMI_PDEV_UPDATE_CTLTABLE_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_update_ctltable_request_fixed_param,  wmi_pdev_update_ctltable_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE,  A_UINT8, ctltable_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UPDATE_CTLTABLE_REQUEST_CMDID);

/* VDEV Get Tx power Cmd */
#define WMITLV_TABLE_WMI_VDEV_GET_TX_POWER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_tx_power_cmd_fixed_param, wmi_vdev_get_tx_power_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_TX_POWER_CMDID);

/* Limit Offchannel duration Cmd */
#define WMITLV_TABLE_WMI_VDEV_LIMIT_OFFCHAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_limit_offchan_cmd_fixed_param, wmi_vdev_limit_offchan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_LIMIT_OFFCHAN_CMDID);

/* vdev per-AC SW retry configuration cmd */
#define WMITLV_TABLE_WMI_VDEV_SET_CUSTOM_SW_RETRY_TH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param, wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_CUSTOM_SW_RETRY_TH_CMDID);

#define WMITLV_TABLE_WMI_VDEV_CHAINMASK_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_chainmask_config_cmd_fixed_param, wmi_vdev_chainmask_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_CHAINMASK_CONFIG_CMDID);

/* PDEV Set Base Mac Address Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_BASE_MACADDR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_base_macaddr_cmd_fixed_param, wmi_pdev_set_base_macaddr_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_BASE_MACADDR_CMDID);

/* MIB Stats*/
#define WMITLV_TABLE_WMI_MIB_STATS_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mib_stats_enable_cmd_fixed_param, wmi_mib_stats_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_MIB_STATS_ENABLE_CMDID);

/* Peer multicast group Cmd */
#define WMITLV_TABLE_WMI_PEER_MCAST_GROUP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_mcast_group_cmd_fixed_param, wmi_peer_mcast_group_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_MCAST_GROUP_CMDID);

/* Roam AP profile Cmd */
#define WMITLV_TABLE_WMI_ROAM_AP_PROFILE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_ap_profile_fixed_param, wmi_roam_ap_profile_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ap_profile, wmi_ap_profile, ap_profile, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_cnd_scoring_param, wmi_roam_cnd_scoring_param, roam_cnd_scoring_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_score_delta_param, roam_score_delta_param_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_cnd_min_rssi_param, roam_cnd_min_rssi_param_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_AP_PROFILE);

/* Roam sync complete Cmd */
#define WMITLV_TABLE_WMI_ROAM_SYNCH_COMPLETE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_synch_complete_fixed_param, wmi_roam_synch_complete_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SYNCH_COMPLETE);

#define WMITLV_TABLE_WMI_ROAM_SET_RIC_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ric_request_fixed_param, wmi_ric_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ric_tspec, ric_tspec_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SET_RIC_REQUEST_CMDID);

/* Scan scheduler priority Table Cmd */
#define WMITLV_TABLE_WMI_SCAN_SCH_PRIO_TBL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_sch_priority_table_cmd_fixed_param, wmi_scan_sch_priority_table_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, mapping_table, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_SCH_PRIO_TBL_CMDID);

/* PDEV DFS enable Cmd */
#define WMITLV_TABLE_WMI_PDEV_DFS_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_enable_cmd_fixed_param, wmi_pdev_dfs_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_ENABLE_CMDID);

/* PDEV DFS disable Cmd */
#define WMITLV_TABLE_WMI_PDEV_DFS_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_disable_cmd_fixed_param, wmi_pdev_dfs_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_DISABLE_CMDID);

/* DFS phyerr parse/filter offload enable Cmd */
#define WMITLV_TABLE_WMI_DFS_PHYERR_FILTER_ENA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_ena_cmd_fixed_param, wmi_dfs_phyerr_filter_ena_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DFS_PHYERR_FILTER_ENA_CMDID);

/* DFS phyerr parse/filter offload disable Cmd */
#define WMITLV_TABLE_WMI_DFS_PHYERR_FILTER_DIS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_dis_cmd_fixed_param, wmi_dfs_phyerr_filter_dis_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DFS_PHYERR_FILTER_DIS_CMDID);

/* DFS phyerr processing offload enable cmd */
#define WMITLV_TABLE_WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_phyerr_offload_enable_cmd_fixed_param, wmi_pdev_dfs_phyerr_offload_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID);

/* DFS phyerr processing offload disble cmd */
#define WMITLV_TABLE_WMI_PDEV_DFS_PHYERR_OFFLOAD_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_phyerr_offload_disable_cmd_fixed_param, wmi_pdev_dfs_phyerr_offload_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_PHYERR_OFFLOAD_DISABLE_CMDID);

/* set ADFS channel config cmd */
#define WMITLV_TABLE_WMI_VDEV_ADFS_CH_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_adfs_ch_cfg_cmd_fixed_param, wmi_vdev_adfs_ch_cfg_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ADFS_CH_CFG_CMDID);

/* DFS abort ADFS ocac currently in progress */
#define WMITLV_TABLE_WMI_VDEV_ADFS_OCAC_ABORT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_adfs_ocac_abort_cmd_fixed_param, wmi_vdev_adfs_ocac_abort_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ADFS_OCAC_ABORT_CMDID);

/* WOW Add Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_ADD_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_ADD_PATTERN_CMD_fixed_param, WMI_WOW_ADD_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_BITMAP_PATTERN_T, pattern_info_bitmap, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IPV4_SYNC_PATTERN_T, pattern_info_ipv4, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IPV6_SYNC_PATTERN_T, pattern_info_ipv6, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_MAGIC_PATTERN_CMD, pattern_info_magic_pattern, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, pattern_info_timeout, WMITLV_SIZE_VAR) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ra_ratelimit_interval, WMITLV_SIZE_FIX, 1)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ADD_WAKE_PATTERN_CMDID);

/* IOAC add keep alive cmd. */
#define WMITLV_TABLE_WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_KEEPALIVE_CMD_fixed_param, WMI_WOW_IOAC_ADD_KEEPALIVE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_WOW_IOAC_KEEPALIVE_T, keepalive_set, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID);

/* IOAC del keep alive cmd. */
#define WMITLV_TABLE_WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_KEEPALIVE_CMD_fixed_param, WMI_WOW_IOAC_DEL_KEEPALIVE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID);

/* WOW IOAC Add Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_PATTERN_CMD_fixed_param, WMI_WOW_IOAC_ADD_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IOAC_PKT_PATTERN_T, pattern_info_pkt, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IOAC_TMR_PATTERN_T, pattern_info_tmr, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IOAC_SOCK_PATTERN_T, pattern_info_sock, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID);

/* WOW IOAC Delete Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_PATTERN_CMD_fixed_param, WMI_WOW_IOAC_DEL_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID);

/* extwow enable Cmd */
#define WMITLV_TABLE_WMI_EXTWOW_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extwow_enable_cmd_fixed_param, wmi_extwow_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTWOW_ENABLE_CMDID);

/* extwow set wakeup params cmd for app type1 */
#define WMITLV_TABLE_WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extwow_set_app_type1_params_cmd_fixed_param, wmi_extwow_set_app_type1_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID);

/* extwow set wakeup params cmd for app type2 */
#define WMITLV_TABLE_WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extwow_set_app_type2_params_cmd_fixed_param, wmi_extwow_set_app_type2_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID);

/* Stop scan Cmd */
#define WMITLV_TABLE_WMI_STOP_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_stop_scan_cmd_fixed_param, wmi_stop_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STOP_SCAN_CMDID);

#define WMITLV_TABLE_WMI_PDEV_SET_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_param_cmd_fixed_param, wmi_pdev_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_PARAM_CMDID);

/* PDev set quiet Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_QUIET_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_quiet_cmd_fixed_param, wmi_pdev_set_quiet_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_QUIET_MODE_CMDID);

/* VDev set quiet Cmd */
#define WMITLV_TABLE_WMI_VDEV_SET_QUIET_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_quiet_cmd_fixed_param, wmi_vdev_set_quiet_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_QUIET_MODE_CMDID);

/* vdev set offload quiet Cmd */
#define WMITLV_TABLE_WMI_VDEV_BCN_OFFLOAD_QUIET_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param, wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_BCN_OFFLOAD_QUIET_CONFIG_CMDID);

/* Setting custom aggregation size using command */
#define WMITLV_TABLE_WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_custom_aggr_size_cmd_fixed_param, wmi_vdev_set_custom_aggr_size_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID);

/* Vdev create Cmd */
#define WMITLV_TABLE_WMI_VDEV_CREATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_create_cmd_fixed_param, wmi_vdev_create_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_txrx_streams, cfg_txrx_streams, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_CREATE_CMDID);

/* Vdev delete Cmd */
#define WMITLV_TABLE_WMI_VDEV_DELETE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_delete_cmd_fixed_param, wmi_vdev_delete_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DELETE_CMDID);

/* Vdev up Cmd */
#define WMITLV_TABLE_WMI_VDEV_UP_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_up_cmd_fixed_param, wmi_vdev_up_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_UP_CMDID);

/* Vdev stop cmd */
#define WMITLV_TABLE_WMI_VDEV_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_stop_cmd_fixed_param, wmi_vdev_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_STOP_CMDID);

/* Vdev down Cmd */
#define WMITLV_TABLE_WMI_VDEV_DOWN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_down_cmd_fixed_param, wmi_vdev_down_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DOWN_CMDID);

/* Vdev set param Cmd */
#define WMITLV_TABLE_WMI_VDEV_SET_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_param_cmd_fixed_param, wmi_vdev_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_PARAM_CMDID);

/* Pdev suspend Cmd */
#define WMITLV_TABLE_WMI_PDEV_SUSPEND_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_suspend_cmd_fixed_param, wmi_pdev_suspend_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SUSPEND_CMDID);

/* Pdev Resume Cmd */
#define WMITLV_TABLE_WMI_PDEV_RESUME_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_resume_cmd_fixed_param, wmi_pdev_resume_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_RESUME_CMDID);

#define WMITLV_TABLE_WMI_SCAN_UPDATE_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_scan_update_request_cmd_fixed_param, wmi_scan_update_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_UPDATE_REQUEST_CMDID);

#define WMITLV_TABLE_WMI_SCAN_PROB_REQ_OUI_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_prob_req_oui_cmd_fixed_param, wmi_scan_prob_req_oui_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vendor_oui, vendor_oui, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_PROB_REQ_OUI_CMDID);

#define WMITLV_TABLE_WMI_PDEV_CONFIG_VENDOR_OUI_ACTION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_config_vendor_oui_action_fixed_param, \
        wmi_pdev_config_vendor_oui_action_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vendor_oui_ext, vendor_oui_ext, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CONFIG_VENDOR_OUI_ACTION_CMDID);

#define WMITLV_TABLE_WMI_CHATTER_ADD_COALESCING_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_STRUC_wmi_chatter_coalescing_add_filter_cmd_fixed_param, wmi_chatter_coalescing_add_filter_cmd_fixed_param, fixed_param,WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, chatter_pkt_coalescing_filter, coalescing_filter, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_ADD_COALESCING_FILTER_CMDID);

#define WMITLV_TABLE_WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_coalescing_delete_filter_cmd_fixed_param,wmi_chatter_coalescing_delete_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID);

#define WMITLV_TABLE_WMI_CHATTER_COALESCING_QUERY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_coalescing_query_cmd_fixed_param, wmi_chatter_coalescing_query_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_COALESCING_QUERY_CMDID);

#define WMITLV_TABLE_WMI_TXBF_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_STRUC_wmi_txbf_cmd_fixed_param, wmi_txbf_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_TXBF_CMDID);

#define WMITLV_TABLE_WMI_DBGLOG_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_debug_log_config_cmd_fixed_param, wmi_debug_log_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len,WMITLV_TAG_ARRAY_UINT32, A_UINT32, module_id_bitmap, WMITLV_SIZE_FIX, MAX_MODULE_ID_BITMAP_WORDS) \

WMITLV_CREATE_PARAM_STRUC(WMI_DBGLOG_CFG_CMDID);

#define WMITLV_TABLE_WMI_VDEV_WMM_ADDTS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_wmm_addts_cmd_fixed_param, wmi_vdev_wmm_addts_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WMM_ADDTS_CMDID);

#define WMITLV_TABLE_WMI_VDEV_WMM_DELTS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_wmm_delts_cmd_fixed_param, wmi_vdev_wmm_delts_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WMM_DELTS_CMDID);

#define WMITLV_TABLE_WMI_VDEV_SET_WMM_PARAMS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_wmm_params_cmd_fixed_param, wmi_vdev_set_wmm_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_WMM_PARAMS_CMDID);

#define WMITLV_TABLE_WMI_VDEV_SET_GTX_PARAMS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_gtx_params_cmd_fixed_param, wmi_vdev_set_gtx_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_GTX_PARAMS_CMDID);

/* TDLS Enable/Disable Cmd */
#define WMITLV_TABLE_WMI_TDLS_SET_STATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_set_state_cmd_fixed_param, \
            wmi_tdls_set_state_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_SET_STATE_CMDID);

/* TDLS Peer Update Cmd */
#define WMITLV_TABLE_WMI_TDLS_PEER_UPDATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_peer_update_cmd_fixed_param, wmi_tdls_peer_update_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_peer_capabilities, wmi_tdls_peer_capabilities, peer_caps, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, peer_chan_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_PEER_UPDATE_CMDID);

/* Enable/Disable TDLS Offchannel Cmd */
#define WMITLV_TABLE_WMI_TDLS_SET_OFFCHAN_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_set_offchan_mode_cmd_fixed_param, \
            wmi_tdls_set_offchan_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_SET_OFFCHAN_MODE_CMDID);


/* Resmgr Enable/Disable Adaptive OCS CMD */
#define WMITLV_TABLE_WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param, \
            wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID);

/* Resmgr Set Channel Time Quota CMD */
#define WMITLV_TABLE_WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resmgr_set_chan_time_quota_cmd_fixed_param, \
            wmi_resmgr_set_chan_time_quota_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID);

/* Resmgr Set Channel Latency CMD */
#define WMITLV_TABLE_WMI_RESMGR_SET_CHAN_LATENCY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resmgr_set_chan_latency_cmd_fixed_param, \
            wmi_resmgr_set_chan_latency_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RESMGR_SET_CHAN_LATENCY_CMDID);

/* STA SMPS Force Mode CMD */
#define WMITLV_TABLE_WMI_STA_SMPS_FORCE_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_cmd_fixed_param, \
            wmi_sta_smps_force_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_SMPS_FORCE_MODE_CMDID);

/* wlan hb enable/disable CMD */
#define WMITLV_TABLE_WMI_HB_SET_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_enable_cmd_fixed_param, \
            wmi_hb_set_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_ENABLE_CMDID);

/* wlan hb set tcp params CMD */
#define WMITLV_TABLE_WMI_HB_SET_TCP_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_tcp_params_cmd_fixed_param, \
            wmi_hb_set_tcp_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_TCP_PARAMS_CMDID);

/* wlan hb set tcp pkt filter CMD */
#define WMITLV_TABLE_WMI_HB_SET_TCP_PKT_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_tcp_pkt_filter_cmd_fixed_param, \
            wmi_hb_set_tcp_pkt_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_TCP_PKT_FILTER_CMDID);

/* wlan set udp params CMD */
#define WMITLV_TABLE_WMI_HB_SET_UDP_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_udp_params_cmd_fixed_param, \
            wmi_hb_set_udp_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_UDP_PARAMS_CMDID);

/* wlan hb set udp pkt filter CMD */
#define WMITLV_TABLE_WMI_HB_SET_UDP_PKT_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_udp_pkt_filter_cmd_fixed_param, \
            wmi_hb_set_udp_pkt_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_UDP_PKT_FILTER_CMDID);

/* STA SMPS Param CMD */
#define WMITLV_TABLE_WMI_STA_SMPS_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_smps_param_cmd_fixed_param, \
            wmi_sta_smps_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_SMPS_PARAM_CMDID);

/* MCC Adaptive Scheduler Traffic Stats */
#define WMITLV_TABLE_WMI_MCC_SCHED_TRAFFIC_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mcc_sched_traffic_stats_cmd_fixed_param, wmi_mcc_sched_traffic_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_STRUC, wmi_mcc_sched_sta_traffic_stats, mcc_sched_sta_traffic_stats_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_MCC_SCHED_TRAFFIC_STATS_CMDID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_enable_cmd_fixed_param, wmi_batch_scan_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)


WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_ENABLE_CMDID);

#define WMITLV_TABLE_WMI_PEER_INFO_REQ_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_info_req_cmd_fixed_param, \
    wmi_peer_info_req_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_INFO_REQ_CMDID);

#define WMITLV_TABLE_WMI_PEER_ANTDIV_INFO_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_antdiv_info_req_cmd_fixed_param, \
    wmi_peer_antdiv_info_req_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ANTDIV_INFO_REQ_CMDID);

#define WMITLV_TABLE_WMI_RMC_SET_MODE_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmc_set_mode_cmd_fixed_param, \
    wmi_rmc_set_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMC_SET_MODE_CMDID);

#define WMITLV_TABLE_WMI_RMC_SET_ACTION_PERIOD_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmc_set_action_period_cmd_fixed_param, \
    wmi_rmc_set_action_period_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMC_SET_ACTION_PERIOD_CMDID);

#define WMITLV_TABLE_WMI_RMC_CONFIG_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmc_config_cmd_fixed_param, \
    wmi_rmc_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMC_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_MHF_OFFLOAD_SET_MODE_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mhf_offload_set_mode_cmd_fixed_param, \
    wmi_mhf_offload_set_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_MHF_OFFLOAD_SET_MODE_CMDID);

#define WMITLV_TABLE_WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mhf_offload_plumb_routing_table_cmd_fixed_param, \
                wmi_mhf_offload_plumb_routing_table_cmd, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_mhf_offload_routing_table_entry, \
                routing_tbl_entries, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_disable_cmd_fixed_param, wmi_batch_scan_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_DISABLE_CMDID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_trigger_result_cmd_fixed_param, wmi_batch_scan_trigger_result_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID);

/* LPI mgmt snooping config Cmd */
#define WMITLV_TABLE_WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_mgmt_snooping_config_cmd_fixed_param, wmi_lpi_mgmt_snooping_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID);

/* LPI start scan Cmd */
#define WMITLV_TABLE_WMI_LPI_START_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_start_scan_cmd_fixed_param, wmi_lpi_start_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_START_SCAN_CMDID);

/* LPI stop scan Cmd */
#define WMITLV_TABLE_WMI_LPI_STOP_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_stop_scan_cmd_fixed_param, wmi_lpi_stop_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_STOP_SCAN_CMDID);

/* Request for roam stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_ROAM_SCAN_STATS_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_roam_scan_stats_cmd_fixed_param, wmi_request_roam_scan_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_ROAM_SCAN_STATS_CMDID);

/* BSS load parameters for roam trigger */
#define WMITLV_TABLE_WMI_ROAM_BSS_LOAD_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_bss_load_config_cmd_fixed_param, wmi_roam_bss_load_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_BSS_LOAD_CONFIG_CMDID);

/* Deauth roam trigger parameters */
#define WMITLV_TABLE_WMI_ROAM_DEAUTH_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_deauth_config_cmd_fixed_param, wmi_roam_deauth_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_DEAUTH_CONFIG_CMDID);

/* Idle roam trigger parameters */
#define WMITLV_TABLE_WMI_ROAM_IDLE_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_idle_config_cmd_fixed_param, wmi_roam_idle_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_IDLE_CONFIG_CMDID);

/* Roam Pre-Authentication completion status */
#define WMITLV_TABLE_WMI_ROAM_PREAUTH_STATUS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_preauth_status_cmd_fixed_param, wmi_roam_preauth_status_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, pmkid, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, pmk, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_PREAUTH_STATUS_CMDID);

/** Roam PMKID request event */
#define WMITLV_TABLE_WMI_ROAM_PMKID_REQUEST_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_pmkid_request_event_fixed_param, wmi_roam_pmkid_request_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_pmkid_request_tlv_param, pmkid_request, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_PMKID_REQUEST_EVENTID);

/* Screen Status Command parameters */
#define WMITLV_TABLE_WMI_IDLE_TRIGGER_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_idle_trigger_monitor_cmd_fixed_param, wmi_idle_trigger_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_IDLE_TRIGGER_MONITOR_CMDID);

/* Enable or disable roaming trigger reason */
#define WMITLV_TABLE_WMI_ROAM_ENABLE_DISABLE_TRIGGER_REASON_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_enable_disable_trigger_reason_fixed_param, wmi_roam_enable_disable_trigger_reason_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_configure_roam_trigger_parameters, config_roam_trigger_param, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_ENABLE_DISABLE_TRIGGER_REASON_CMDID);

/* DSM filter parameters */
#define WMITLV_TABLE_WMI_PDEV_DSM_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dsm_filter_fixed_param, wmi_pdev_dsm_filter_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_bssid_disallow_list_config_param, bssid_disallow_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DSM_FILTER_CMDID);

#define WMITLV_TABLE_WMI_ROAM_BLACKLIST_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_blacklist_event_fixed_param, wmi_roam_blacklist_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_blacklist_with_timeout_tlv_param, blacklist_with_timeout, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_BLACKLIST_EVENTID);

/* Roam Pre-Authentication Start Event */
#define WMITLV_TABLE_WMI_ROAM_PREAUTH_START_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_preauth_start_event_fixed_param, wmi_roam_preauth_start_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_PREAUTH_START_EVENTID);

#define WMITLV_TABLE_WMI_LPI_RESULT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_result_event_fixed_param, wmi_lpi_result_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_LPI_RESULT_EVENTID);

/* LPI Status Event */
#define WMITLV_TABLE_WMI_LPI_STATUS_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_status_event_fixed_param, wmi_lpi_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_LPI_STATUS_EVENTID);

/* LPI Handoff Event */
#define WMITLV_TABLE_WMI_LPI_HANDOFF_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_handoff_event_fixed_param, wmi_lpi_handoff_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_LPI_HANDOFF_EVENTID);

/* Thermal Manager Params*/
#define WMITLV_TABLE_WMI_THERMAL_MGMT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_thermal_mgmt_cmd_fixed_param, wmi_thermal_mgmt_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_THERMAL_MGMT_CMDID);


#define WMITLV_TABLE_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, pattern, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID);

#define WMITLV_TABLE_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID);

/* NaN Request */
#define WMITLV_TABLE_WMI_NAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_cmd_param, wmi_nan_cmd_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_nan_host_config_param, host_config, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_CMDID);

/* NAN Data Get Capabilities Cmd */
#define WMITLV_TABLE_WMI_NDI_GET_CAP_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndi_get_cap_req_fixed_param, wmi_ndi_get_cap_req_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDI_GET_CAP_REQ_CMDID);

/** NAN Data Initiator Request Cmd
 *
 * TLV (tag length value ) parameters follow the ndp_initiator_req
 * structure. The TLV's are:
 * wmi_channel channel;
 * A_UINT8 ndp_cfg[];
 * A_UINT8 ndp_app_info[];
 * A_UINT8 ndp_pmk[];
 * A_INT8 ndp_passphrase[];
 * A_INT8 nan_servicename[];
 * wmi_ndp_transport_ip_param ndp_transport_ip_param;
 */
#define WMITLV_TABLE_WMI_NDP_INITIATOR_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_initiator_req_fixed_param, wmi_ndp_initiator_req_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, channel, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_cfg, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_app_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_pmk, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_INT8, ndp_passphrase, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_INT8, nan_servicename, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_transport_ip_param, wmi_ndp_transport_ip_param, ndp_transport_ip_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_INITIATOR_REQ_CMDID);

/** NAN Data Responder Request Cmd
 * TLV (tag length value ) parameters follow the ndp_responder_req
 * structure. The TLV's are:
 * A_UINT8 ndp_cfg[];
 * A_UINT8 ndp_app_info[];
 * A_UINT8 ndp_pmk[];
 * A_INT8 ndp_passphrase[];
 * A_INT8 nan_servicename[];
 * wmi_ndp_transport_ip_param ndp_transport_ip_param;
 */
#define WMITLV_TABLE_WMI_NDP_RESPONDER_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_responder_req_fixed_param, wmi_ndp_responder_req_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_cfg, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_app_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_pmk, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_INT8, ndp_passphrase, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_INT8, nan_servicename, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_transport_ip_param, wmi_ndp_transport_ip_param, ndp_transport_ip_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_RESPONDER_REQ_CMDID);

/** NAN Data End Request Cmd
 *
 * TLV (tag length value ) parameters follow the ndp_end_req
 * structure. The TLV's are:
 * wmi_ndp_end_req wmi_ndp_end_req_list[];
 */
#define WMITLV_TABLE_WMI_NDP_END_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_end_req_fixed_param, wmi_ndp_end_req_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ndp_end_req_PROTOTYPE, ndp_end_req_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_END_REQ_CMDID);

/* NDP Request */
#define WMITLV_TABLE_WMI_NDP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_cmd_param, wmi_ndp_cmd_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_CMDID);

/* RCPI Info Request Cmd */
#define WMITLV_TABLE_WMI_REQUEST_RCPI_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_rcpi_cmd_fixed_param, wmi_request_rcpi_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_RCPI_CMDID);

/* Modem power state cmd */
#define WMITLV_TABLE_WMI_MODEM_POWER_STATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_modem_power_state_cmd_param, wmi_modem_power_state_cmd_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MODEM_POWER_STATE_CMDID);

/* SAR limit update cmd */
#define WMITLV_TABLE_WMI_SAR_LIMITS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sar_limits_cmd_fixed_param, wmi_sar_limits_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_sar_limit_cmd_row, sar_limits, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SAR_LIMITS_CMDID);

/* SAR limit request cmd */
#define WMITLV_TABLE_WMI_SAR_GET_LIMITS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sar_get_limits_cmd_fixed_param, wmi_sar_get_limits_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAR_GET_LIMITS_CMDID);

#define WMITLV_TABLE_WMI_SAR2_RESULT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sar2_result_event_fixed_param, wmi_sar2_result_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAR2_RESULT_EVENTID);

/* get estimated link speed cmd */
#define WMITLV_TABLE_WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_get_estimated_linkspeed_cmd_fixed_param, wmi_peer_get_estimated_linkspeed_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID);

/* ext stats Request */
#define WMITLV_TABLE_WMI_REQUEST_STATS_EXT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_req_stats_ext_cmd_fixed_param, wmi_req_stats_ext_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_STATS_EXT_CMDID);

/* 2.4Ghz HT40 OBSS scan enable */
#define WMITLV_TABLE_WMI_OBSS_SCAN_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_obss_scan_enable_cmd_fixed_param, wmi_obss_scan_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, channels, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_field, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, chan_freqs, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OBSS_SCAN_ENABLE_CMDID);

/* 2.4Ghz HT40 OBSS scan disable */
#define WMITLV_TABLE_WMI_OBSS_SCAN_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_obss_scan_disable_cmd_fixed_param, wmi_obss_scan_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OBSS_SCAN_DISABLE_CMDID);

/* Pdev Set LED Config Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_LED_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_led_config_cmd_fixed_param, wmi_pdev_set_led_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_LED_CONFIG_CMDID);

/* host auto shut down config cmd */
#define WMITLV_TABLE_WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_auto_shutdown_cfg_cmd_fixed_param, wmi_host_auto_shutdown_cfg_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID);

/* tpc chainmask config cmd */
#define WMITLV_TABLE_WMI_TPC_CHAINMASK_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tpc_chainmask_config_cmd_fixed_param, wmi_tpc_chainmask_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tpc_chainmask_config, config_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TPC_CHAINMASK_CONFIG_CMDID);


/* Ch avoidance update cmd */
#define WMITLV_TABLE_WMI_CHAN_AVOID_UPDATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chan_avoid_update_cmd_param, wmi_chan_avoid_update_cmd_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CHAN_AVOID_UPDATE_CMDID);

/* Ch avoidance report allow/disallow cmd*/
#define WMITLV_TABLE_WMI_CHAN_AVOID_RPT_ALLOW_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param, WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CHAN_AVOID_RPT_ALLOW_CMDID);

/* D0-WOW Enable Disable Cmd */
#define WMITLV_TABLE_WMI_D0_WOW_ENABLE_DISABLE_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_d0_wow_enable_disable_cmd_fixed_param, wmi_d0_wow_enable_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_D0_WOW_ENABLE_DISABLE_CMDID);

/* Pdev get chip temperature Cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_TEMPERATURE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_temperature_cmd_fixed_param, wmi_pdev_get_temperature_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TEMPERATURE_CMDID);

/* Pdev get ANT DIV feature status Cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_ANTDIV_STATUS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_antdiv_status_cmd_fixed_param, wmi_pdev_get_antdiv_status_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_ANTDIV_STATUS_CMDID);

/* DISA feature : vdev encrypt decrypt request */
#define WMITLV_TABLE_WMI_VDEV_ENCRYPT_DECRYPT_DATA_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_encrypt_decrypt_data_req_cmd_fixed_param, wmi_vdev_encrypt_decrypt_data_req_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ENCRYPT_DECRYPT_DATA_REQ_CMDID);

/* Set antenna diversity Cmd */
#define WMITLV_TABLE_WMI_SET_ANTENNA_DIVERSITY_CMDID(id,op,buf,len) \
WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_pdev_set_antenna_diversity_cmd_fixed_param, wmi_pdev_set_antenna_diversity_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_ANTENNA_DIVERSITY_CMDID);

/* Set rssi monitoring config Cmd */
#define WMITLV_TABLE_WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID(id,op,buf,len) \
WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_rssi_breach_monitor_config_fixed_param, wmi_rssi_breach_monitor_config_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID);

/* DHCP server offload param Cmd */
#define WMITLV_TABLE_WMI_SET_DHCP_SERVER_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_dhcp_server_offload_cmd_fixed_param, wmi_set_dhcp_server_offload_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_DHCP_SERVER_OFFLOAD_CMDID);

/* IPA Offload Enable Disable Cmd */
#define WMITLV_TABLE_WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUCT_wmi_ipa_offload_enable_disable_cmd_fixed_param, wmi_ipa_offload_enable_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID);

/* Set LED flashing parameter Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_LED_FLASHING_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_led_flashing_cmd_fixed_param, wmi_set_led_flashing_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_LED_FLASHING_CMDID);

/* mDNS responder offload param Cmd */
#define WMITLV_TABLE_WMI_MDNS_OFFLOAD_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_offload_cmd_fixed_param, wmi_mdns_offload_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_OFFLOAD_ENABLE_CMDID);

#define WMITLV_TABLE_WMI_MDNS_SET_FQDN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_set_fqdn_cmd_fixed_param, wmi_mdns_set_fqdn_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, fqdn_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_SET_FQDN_CMDID);

#define WMITLV_TABLE_WMI_MDNS_SET_RESPONSE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_set_resp_cmd_fixed_param, wmi_mdns_set_resp_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, resp_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_SET_RESPONSE_CMDID);

#define WMITLV_TABLE_WMI_MDNS_GET_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_get_stats_cmd_fixed_param, wmi_mdns_get_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_GET_STATS_CMDID);

#define WMITLV_TABLE_WMI_MDNS_SET_STAIP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_set_staIP_cmd_fixed_param, wmi_mdns_set_staIP_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_SET_STAIP_CMDID);

/* roam invoke Cmd */
#define WMITLV_TABLE_WMI_ROAM_INVOKE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_invoke_cmd_fixed_param, wmi_roam_invoke_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_tlv_buf_len_param, bcn_prb_buf_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bcn_prb_frm, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_INVOKE_CMDID);

/* SAP Authentication offload param Cmd */
#define WMITLV_TABLE_WMI_SAP_OFL_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_ofl_enable_cmd_fixed_param, wmi_sap_ofl_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, psk, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OFL_ENABLE_CMDID);

/* SAP set blacklist param cmd */
#define WMITLV_TABLE_WMI_SAP_SET_BLACKLIST_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_set_blacklist_param_cmd_fixed_param, wmi_sap_set_blacklist_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_SET_BLACKLIST_PARAM_CMDID);

/* SAP OBSS detection offload param Cmd */
#define WMITLV_TABLE_WMI_SAP_OBSS_DETECTION_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_obss_detection_cfg_cmd_fixed_param, wmi_sap_obss_detection_cfg_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OBSS_DETECTION_CFG_CMDID);

/* STA BSS Color change offload param Cmd */
#define WMITLV_TABLE_WMI_BSS_COLOR_CHANGE_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bss_color_change_enable_fixed_param, wmi_bss_color_change_enable_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BSS_COLOR_CHANGE_ENABLE_CMDID);

/* OBSS Color collision detection config cmd */
#define WMITLV_TABLE_WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_obss_color_collision_det_config_fixed_param, wmi_obss_color_collision_det_config_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID);

/* APFIND Request */
#define WMITLV_TABLE_WMI_APFIND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_apfind_cmd_param, wmi_apfind_cmd_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_APFIND_CMDID);

/* Set OCB schedule cmd, DEPRECATED */
#define WMITLV_TABLE_WMI_OCB_SET_SCHED_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_ocb_set_sched_cmd_fixed_param, wmi_ocb_set_sched_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_SCHED_CMDID);

/* Set OCB configuration cmd */
#define WMITLV_TABLE_WMI_OCB_SET_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_set_config_cmd_fixed_param, wmi_ocb_set_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, chan_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ocb_channel, ocb_chan_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_qos_parameter, qos_parameter_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_chan, chan_cfg, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_active_state_config, ndl_active_state_config_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ocb_schedule_element, schedule_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_CONFIG_CMDID);

/* Set UTC time cmd */
#define WMITLV_TABLE_WMI_OCB_SET_UTC_TIME_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_set_utc_time_cmd_fixed_param, wmi_ocb_set_utc_time_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_UTC_TIME_CMDID);

/* Start timing advertisement cmd */
#define WMITLV_TABLE_WMI_OCB_START_TIMING_ADVERT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_start_timing_advert_cmd_fixed_param, wmi_ocb_start_timing_advert_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_START_TIMING_ADVERT_CMDID);

/* Stop timing advertisement cmd */
#define WMITLV_TABLE_WMI_OCB_STOP_TIMING_ADVERT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_stop_timing_advert_cmd_fixed_param, wmi_ocb_stop_timing_advert_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_STOP_TIMING_ADVERT_CMDID);

/* Get TSF timer cmd */
#define WMITLV_TABLE_WMI_OCB_GET_TSF_TIMER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_cmd_fixed_param, wmi_ocb_get_tsf_timer_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_GET_TSF_TIMER_CMDID);

/* Get DCC stats cmd */
#define WMITLV_TABLE_WMI_DCC_GET_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_get_stats_cmd_fixed_param, wmi_dcc_get_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_channel_stats_request, channel_stats_request, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_GET_STATS_CMDID);

/* Clear DCC stats cmd */
#define WMITLV_TABLE_WMI_DCC_CLEAR_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_clear_stats_cmd_fixed_param, wmi_dcc_clear_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_CLEAR_STATS_CMDID);

/* Update DCC NDL cmd */
#define WMITLV_TABLE_WMI_DCC_UPDATE_NDL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_update_ndl_cmd_fixed_param, wmi_dcc_update_ndl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_chan, chan_ndl_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_active_state_config, ndl_active_state_config_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_UPDATE_NDL_CMDID);

/* Roam filter cmd */
#define WMITLV_TABLE_WMI_ROAM_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_filter_fixed_param, wmi_roam_filter_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_black_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_white_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_preferred_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, bssid_preferred_factor, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_lca_disallow_config_tlv_param, lca_disallow_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_rejection_list_config_param, rssi_rejection_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_FILTER_CMDID);

/* TSF timestamp action cmd */
#define WMITLV_TABLE_WMI_VDEV_TSF_TSTAMP_ACTION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_tsf_tstamp_action_cmd_fixed_param, wmi_vdev_tsf_tstamp_action_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_TSF_TSTAMP_ACTION_CMDID);

/* LFR subnet change config Cmd */
#define WMITLV_TABLE_WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_subnet_change_config_fixed_param, wmi_roam_subnet_change_config_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, skip_subnet_change_detection_bssid_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID);

/* Set the SOC Preferred Channel List (PCL) Cmd - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_SET_PCL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_pcl_cmd_fixed_param, wmi_soc_set_pcl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_PCL_CMDID);

/* Set the PDEV Preferred Channel List (PCL) Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_PCL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_pcl_cmd_fixed_param, wmi_pdev_set_pcl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_weight, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_PCL_CMDID);

/* Set the SOC Hardware Mode Cmd - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_SET_HW_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_cmd_fixed_param, wmi_soc_set_hw_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_HW_MODE_CMDID);

/* Set the PDEV Hardware Mode Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_HW_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_hw_mode_cmd_fixed_param, wmi_pdev_set_hw_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_HW_MODE_CMDID);

/* Set the SOC Dual MAC Config Cmd - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_cmd_fixed_param, wmi_soc_set_dual_mac_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID);

/* Set the PDEV MAC Config Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_MAC_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_mac_config_cmd_fixed_param, wmi_pdev_set_mac_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_MAC_CONFIG_CMDID);

/* Set the SOC Antenna Mode Cmd - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_SET_ANTENNA_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_antenna_mode_cmd_fixed_param, wmi_soc_set_antenna_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_ANTENNA_MODE_CMDID);

/* Set the PDEV Antenna Mode Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_ANTENNA_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_antenna_mode_cmd_fixed_param, wmi_pdev_set_antenna_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_ANTENNA_MODE_CMDID);

#define WMITLV_TABLE_WMI_LRO_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lro_info_cmd_fixed_param, wmi_lro_info_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_LRO_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_TRANSFER_DATA_TO_FLASH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_cmd_fixed_param, wmi_transfer_data_to_flash_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TRANSFER_DATA_TO_FLASH_CMDID);

#define WMITLV_TABLE_WMI_READ_DATA_FROM_FLASH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_read_data_from_flash_cmd_fixed_param, wmi_read_data_from_flash_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_READ_DATA_FROM_FLASH_CMDID);

#define WMITLV_TABLE_WMI_CONFIG_ENHANCED_MCAST_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_config_enhanced_mcast_filter_fixed_param, wmi_config_enhanced_mcast_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CONFIG_ENHANCED_MCAST_FILTER_CMDID);

#define WMITLV_TABLE_WMI_VDEV_WISA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_wisa_cmd_fixed_param, wmi_vdev_wisa_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WISA_CMDID);

/* MAWC sensor report indication cmd */
#define WMITLV_TABLE_WMI_MAWC_SENSOR_REPORT_IND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mawc_sensor_report_ind_cmd_fixed_param, wmi_mawc_sensor_report_ind_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MAWC_SENSOR_REPORT_IND_CMDID);

/* Roam configure MAWC cmd */
#define WMITLV_TABLE_WMI_ROAM_CONFIGURE_MAWC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_configure_mawc_cmd_fixed_param, wmi_roam_configure_mawc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_CONFIGURE_MAWC_CMDID);

/* NLO configure MAWC cmd */
#define WMITLV_TABLE_WMI_NLO_CONFIGURE_MAWC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_configure_mawc_cmd_fixed_param, wmi_nlo_configure_mawc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NLO_CONFIGURE_MAWC_CMDID);

/* Extscan configure MAWC cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_MAWC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_mawc_cmd_fixed_param, wmi_extscan_configure_mawc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_MAWC_CMDID);

/* COEX config cmd */
#define WMITLV_TABLE_WMI_COEX_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_COEX_CONFIG_CMD_fixed_param, WMI_COEX_CONFIG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_COEX_CONFIG_CMDID);

/* Coex get antenna isolation cmd */
#define WMITLV_TABLE_WMI_COEX_GET_ANTENNA_ISOLATION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_coex_get_antenna_isolation_cmd_fixed_param, wmi_coex_get_antenna_isolation_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_COEX_GET_ANTENNA_ISOLATION_CMDID);

/* Cal Get power fixed param */
#define WMITLV_TABLE_WMI_PDEV_GET_NFCAL_POWER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_nfcal_power_fixed_param, wmi_pdev_get_nfcal_power_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_NFCAL_POWER_CMDID);

/* bpf offload capability get cmd */
#define WMITLV_TABLE_WMI_BPF_GET_CAPABILITY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_get_capability_cmd_fixed_param, wmi_bpf_get_capability_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_GET_CAPABILITY_CMDID);

/* bpf offload get vdev status cmd */
#define WMITLV_TABLE_WMI_BPF_GET_VDEV_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_get_vdev_stats_cmd_fixed_param, wmi_bpf_get_vdev_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_GET_VDEV_STATS_CMDID);

/* bpf offload set vdev instructions cmd */
#define WMITLV_TABLE_WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_set_vdev_instructions_cmd_fixed_param, wmi_bpf_set_vdev_instructions_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, buf_inst, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID);

/* bpf offload delete vdev instructions cmd */
#define WMITLV_TABLE_WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_del_vdev_instructions_cmd_fixed_param, wmi_bpf_del_vdev_instructions_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID);

#define WMITLV_TABLE_WMI_BPF_SET_VDEV_ACTIVE_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_set_vdev_active_mode_cmd_fixed_param, wmi_bpf_set_vdev_active_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_SET_VDEV_ACTIVE_MODE_CMDID);

#define WMITLV_TABLE_WMI_BPF_SET_VDEV_ENABLE_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_set_vdev_enable_cmd_fixed_param, wmi_bpf_set_vdev_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_SET_VDEV_ENABLE_CMDID);

#define WMITLV_TABLE_WMI_BPF_SET_VDEV_WORK_MEMORY_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_set_vdev_work_memory_cmd_fixed_param, wmi_bpf_set_vdev_work_memory_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, buf_inst, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_SET_VDEV_WORK_MEMORY_CMDID);

#define WMITLV_TABLE_WMI_BPF_GET_VDEV_WORK_MEMORY_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_get_vdev_work_memory_cmd_fixed_param, wmi_bpf_get_vdev_work_memory_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_GET_VDEV_WORK_MEMORY_CMDID);

/* Enable/Disable Smart Antenna */
#define WMITLV_TABLE_WMI_PDEV_SMART_ANT_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_smart_ant_enable_cmd_fixed_param, wmi_pdev_smart_ant_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_smart_ant_gpio_handle, gpio_handle, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SMART_ANT_ENABLE_CMDID);

/* Set Smart Antenna RX antenna */
#define WMITLV_TABLE_WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_smart_ant_set_rx_antenna_cmd_fixed_param, wmi_pdev_smart_ant_set_rx_antenna_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID);

/* Override the antenna switch table */
#define WMITLV_TABLE_WMI_PDEV_SET_ANTENNA_SWITCH_TABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_ant_switch_tbl_cmd_fixed_param, wmi_pdev_set_ant_switch_tbl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_set_ant_ctrl_chain, ant_ctrl_chain, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_ANTENNA_SWITCH_TABLE_CMDID);

/* Override the CTL table */
#define WMITLV_TABLE_WMI_PDEV_SET_CTL_TABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_ctl_table_cmd_fixed_param, wmi_pdev_set_ctl_table_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ctl_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_CTL_TABLE_CMDID);

/* Override the array gain table */
#define WMITLV_TABLE_WMI_PDEV_SET_MIMOGAIN_TABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_mimogain_table_cmd_fixed_param, wmi_pdev_set_mimogain_table_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, arraygain_tbl, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_MIMOGAIN_TABLE_CMDID);

/* FIPS cmd */
#define WMITLV_TABLE_WMI_PDEV_FIPS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_fips_cmd_fixed_param, wmi_pdev_fips_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FIPS_CMDID);

/* get CCK ANI level */
#define WMITLV_TABLE_WMI_PDEV_GET_ANI_CCK_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_ani_cck_config_cmd_fixed_param, wmi_pdev_get_ani_cck_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_ANI_CCK_CONFIG_CMDID);

/* get OFDM ANI level */
#define WMITLV_TABLE_WMI_PDEV_GET_ANI_OFDM_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_ani_ofdm_config_cmd_fixed_param, wmi_pdev_get_ani_ofdm_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_ANI_OFDM_CONFIG_CMDID);

/* TxPPDU TPC cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_TPC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_tpc_cmd_fixed_param, wmi_pdev_get_tpc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TPC_CMDID);

#define WMITLV_TABLE_WMI_VDEV_RATEMASK_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_config_ratemask_fixed_param, wmi_vdev_config_ratemask_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_RATEMASK_CMDID);

/* ATF VDEV REQUEST commands */
#define WMITLV_TABLE_WMI_VDEV_ATF_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_atf_request_fixed_param, wmi_vdev_atf_request_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ATF_REQUEST_CMDID);

/* Command to send the DSCP-to-TID map to the target for VAP */
#define WMITLV_TABLE_WMI_VDEV_SET_DSCP_TID_MAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_dscp_tid_map_cmd_fixed_param, wmi_vdev_set_dscp_tid_map_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_DSCP_TID_MAP_CMDID);

/* Configure filter for Neighbor Rx Pkt (smart mesh selective listening) */
#define WMITLV_TABLE_WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_filter_nrp_config_cmd_fixed_param, wmi_vdev_filter_nrp_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID);

/* update a wds (4 address) entry */
#define WMITLV_TABLE_WMI_PEER_UPDATE_WDS_ENTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_update_wds_entry_cmd_fixed_param, wmi_peer_update_wds_entry_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_UPDATE_WDS_ENTRY_CMDID);

/* add a proxy sta entry */
#define WMITLV_TABLE_WMI_PEER_ADD_PROXY_STA_ENTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_create_cmd_fixed_param, wmi_peer_create_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ADD_PROXY_STA_ENTRY_CMDID);

/* Set Smart Antenna TX antenna */
#define WMITLV_TABLE_WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_tx_antenna_cmd_fixed_param, wmi_peer_smart_ant_set_tx_antenna_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_smart_ant_set_tx_antenna_series, antenna_series, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID);

/* Set Smart Antenna TX train info */
#define WMITLV_TABLE_WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_train_antenna_cmd_fixed_param, wmi_peer_smart_ant_set_train_antenna_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_smart_ant_set_train_antenna_param, antenna_param, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID);

/* Set SA node config options */
#define WMITLV_TABLE_WMI_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_smart_ant_set_node_config_ops_cmd_fixed_param, wmi_peer_smart_ant_set_node_config_ops_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, args, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMDID);

/* Q-Boost configuration test commands */
#define WMITLV_TABLE_WMI_QBOOST_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_QBOOST_CFG_CMD_fixed_param, WMI_QBOOST_CFG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_QBOOST_CFG_CMDID);

/* set debug and tuning parameters */
#define WMITLV_TABLE_WMI_FWTEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_fwtest_set_param_cmd_fixed_param, wmi_fwtest_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_FWTEST_CMDID);

/* ATF PEER REQUEST commands. */
#define WMITLV_TABLE_WMI_PEER_ATF_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_atf_request_fixed_param, wmi_peer_atf_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_atf_peer_info, peer_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ATF_REQUEST_CMDID);

/* ATF Group Request commands */
#define WMITLV_TABLE_WMI_ATF_SSID_GROUPING_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_atf_ssid_grp_request_fixed_param, wmi_atf_ssid_grp_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_atf_group_info, group_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ATF_SSID_GROUPING_REQUEST_CMDID);

/* ATF Group WMM Request commands */
#define WMITLV_TABLE_WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_atf_grp_wmm_ac_cfg_request_fixed_param, wmi_atf_grp_wmm_ac_cfg_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_atf_group_wmm_ac_info, group_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID);

/* ATF EXTENDED PEER REQUEST commands */
#define WMITLV_TABLE_WMI_PEER_ATF_EXT_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_atf_ext_request_fixed_param, wmi_peer_atf_ext_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_atf_ext_info, peer_ext_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ATF_EXT_REQUEST_CMDID);

/* enable/disable and set the periodicity of periodic channel stats */
#define WMITLV_TABLE_WMI_SET_PERIODIC_CHANNEL_STATS_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_periodic_channel_stats_config_fixed_param, wmi_set_periodic_channel_stats_config_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_PERIODIC_CHANNEL_STATS_CONFIG_CMDID);

/* wal power debug command per pdev */
#define WMITLV_TABLE_WMI_PDEV_WAL_POWER_DEBUG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_wal_power_debug_cmd_fixed_param, wmi_pdev_wal_power_debug_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, args, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_WAL_POWER_DEBUG_CMDID);

/* pdev set reorder timeout val */
#define WMITLV_TABLE_WMI_PDEV_SET_REORDER_TIMEOUT_VAL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_reorder_timeout_val_cmd_fixed_param, wmi_pdev_set_reorder_timeout_val_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_REORDER_TIMEOUT_VAL_CMDID);

/* peer set rx blocksize cmd */
#define WMITLV_TABLE_WMI_PEER_SET_RX_BLOCKSIZE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_set_rx_blocksize_cmd_fixed_param, wmi_peer_set_rx_blocksize_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SET_RX_BLOCKSIZE_CMDID);

/* Bandwidth Fairness (BWF) peer configure commands */
#define WMITLV_TABLE_WMI_PEER_BWF_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_bwf_request_fixed_param, wmi_peer_bwf_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_bwf_peer_info, peer_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_BWF_REQUEST_CMDID);

#define WMITLV_TABLE_WMI_RMC_SET_MANUAL_LEADER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_rmc_set_leader_cmd_fixed_param, wmi_rmc_set_leader_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RMC_SET_MANUAL_LEADER_CMDID);

/* peer reorder queue setup cmd */
#define WMITLV_TABLE_WMI_PEER_REORDER_QUEUE_SETUP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_reorder_queue_setup_cmd_fixed_param, wmi_peer_reorder_queue_setup_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_REORDER_QUEUE_SETUP_CMDID);

/* peer reorder queue remove cmd */
#define WMITLV_TABLE_WMI_PEER_REORDER_QUEUE_REMOVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_reorder_queue_remove_cmd_fixed_param, wmi_peer_reorder_queue_remove_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_REORDER_QUEUE_REMOVE_CMDID);

/* Filter in monitor mode paramters Cmd */
#define WMITLV_TABLE_WMI_MNT_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mnt_filter_cmd_fixed_param, wmi_mnt_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MNT_FILTER_CMDID);

/* WLAN GET Chip power Stats*/
#define WMITLV_TABLE_WMI_PDEV_GET_CHIP_POWER_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_chip_power_stats_cmd_fixed_param, wmi_pdev_get_chip_power_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_CHIP_POWER_STATS_CMDID);

/* WLAN GET mws conflict */
#define WMITLV_TABLE_WMI_VDEV_GET_MWS_COEX_INFO_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_info_cmd_fixed_param, wmi_vdev_get_mws_coex_info_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_MWS_COEX_INFO_CMDID);

/* WLAN GET beacon reception Stats*/
#define WMITLV_TABLE_WMI_VDEV_GET_BCN_RECEPTION_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_bcn_recv_stats_fixed_param, wmi_vdev_get_bcn_recv_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_BCN_RECEPTION_STATS_CMDID);

/* pdev set stats threshold cmd*/
#define WMITLV_TABLE_WMI_PDEV_SET_STATS_THRESHOLD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_stats_threshold_cmd_fixed_param, wmi_pdev_set_stats_threshold_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chan_cca_stats_thresh, wmi_chan_cca_stats_thresh, cca_thresh, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_signal_stats_thresh, wmi_peer_signal_stats_thresh, signal_thresh, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_stats_thresh, wmi_tx_stats_thresh, tx_thresh, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rx_stats_thresh, wmi_rx_stats_thresh, rx_thresh, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_STATS_THRESHOLD_CMDID);

/* Request wlan stats cmd */
#define WMITLV_TABLE_WMI_REQUEST_WLAN_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_wlan_stats_cmd_fixed_param, wmi_request_wlan_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_WLAN_STATS_CMDID);

/* Request peer stats info cmd */
#define WMITLV_TABLE_WMI_REQUEST_PEER_STATS_INFO_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_peer_stats_info_cmd_fixed_param, wmi_request_peer_stats_info_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_PEER_STATS_INFO_CMDID);

/* Request Control Path stats info cmd */
#define WMITLV_TABLE_WMI_REQUEST_CTRL_PATH_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_ctrl_path_stats_cmd_fixed_param, wmi_request_ctrl_path_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, pdev_ids, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, vdev_ids, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, mac_addr_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_CTRL_PATH_STATS_CMDID);

/* Host sets the current country code */
#define WMITLV_TABLE_WMI_SET_CURRENT_COUNTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_current_country_cmd_fixed_param, wmi_set_current_country_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_CURRENT_COUNTRY_CMDID);

/* Host sets the init country code */
#define WMITLV_TABLE_WMI_SET_INIT_COUNTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_init_country_cmd_fixed_param, wmi_set_init_country_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_INIT_COUNTRY_CMDID);

/* Start 11d scan in FW */
#define WMITLV_TABLE_WMI_11D_SCAN_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_11d_scan_start_cmd_fixed_param, wmi_11d_scan_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_11D_SCAN_START_CMDID);

/* Stop 11d scan in FW */
#define WMITLV_TABLE_WMI_11D_SCAN_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_11d_scan_stop_cmd_fixed_param, wmi_11d_scan_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_11D_SCAN_STOP_CMDID);

/* Request radio channel stats cmd */
#define WMITLV_TABLE_WMI_REQUEST_RADIO_CHAN_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_radio_chan_stats_cmd_fixed_param, wmi_request_radio_chan_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_RADIO_CHAN_STATS_CMDID);

/* mac randomization cmd */
#define WMITLV_TABLE_WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_add_mac_addr_to_rx_filter_cmd_fixed_param, wmi_vdev_add_mac_addr_to_rx_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_CMDID);

#define WMITLV_TABLE_WMI_HW_DATA_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hw_data_filter_cmd_fixed_param, wmi_hw_data_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HW_DATA_FILTER_CMDID);

/* Multiple vdev restart request cmd */
#define WMITLV_TABLE_WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param, wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, vdev_ids, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, phymode_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID);

#define WMITLV_TABLE_WMI_PDEV_UPDATE_PKT_ROUTING_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_update_pkt_routing_cmd_fixed_param, wmi_pdev_update_pkt_routing_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UPDATE_PKT_ROUTING_CMDID);

/* Get cal version cmd */
#define WMITLV_TABLE_WMI_PDEV_CHECK_CAL_VERSION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_check_cal_version_cmd_fixed_param, wmi_pdev_check_cal_version_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CHECK_CAL_VERSION_CMDID);

#define WMITLV_TABLE_WMI_PDEV_SET_DIVERSITY_GAIN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_diversity_gain_cmd_fixed_param, wmi_pdev_set_diversity_gain_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, diversity_gains, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_DIVERSITY_GAIN_CMDID);

/* set arp stats cmd */
#define WMITLV_TABLE_WMI_VDEV_SET_ARP_STAT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_arp_stats_cmd_fixed_param, wmi_vdev_set_arp_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_set_connectivity_check_stats, connectivity_check_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_ARP_STAT_CMDID);

/* get arp stats cmd */
#define WMITLV_TABLE_WMI_VDEV_GET_ARP_STAT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_arp_stats_cmd_fixed_param, wmi_vdev_get_arp_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_ARP_STAT_CMDID);

/* Thermal Throttling SET CONFIG commands. */
#define WMITLV_TABLE_WMI_THERM_THROT_SET_CONF_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_therm_throt_config_request_fixed_param, wmi_therm_throt_config_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_therm_throt_level_config_info, therm_throt_level_config_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_THERM_THROT_SET_CONF_CMDID);

/* OIC ping offload cmd */
#define WMITLV_TABLE_WMI_HB_OIC_PING_OFFLOAD_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oic_ping_offload_params_cmd_fixed_param, wmi_oic_ping_offload_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HB_OIC_PING_OFFLOAD_PARAM_CMDID);

#define WMITLV_TABLE_WMI_HB_OIC_PING_OFFLOAD_SET_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oic_ping_offload_set_enable_cmd_fixed_param, wmi_oic_ping_offload_set_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HB_OIC_PING_OFFLOAD_SET_ENABLE_CMDID);

/* DHCP Lease Renew Offload cmd */
#define WMITLV_TABLE_WMI_HB_DHCP_LEASE_RENEW_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_STRUC_wmi_dhcp_lease_renew_offload_cmd_fixed_param, wmi_dhcp_lease_renew_offload_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HB_DHCP_LEASE_RENEW_OFFLOAD_CMDID);

/* BTM config command */
#define WMITLV_TABLE_WMI_ROAM_BTM_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_btm_config_fixed_param, wmi_btm_config_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_BTM_CONFIG_CMDID);

/* vdev latency config cmd */
#define WMITLV_TABLE_WMI_WLM_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlm_config_cmd_fixed_param, wmi_wlm_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLM_CONFIG_CMDID);

/* Event to send roam stats */
#define WMITLV_TABLE_WMI_ROAM_STATS_EVENTID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_stats_event_fixed_param, wmi_roam_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_trigger_reason, roam_trigger_reason, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_scan_info, roam_scan_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_scan_channel_info, roam_scan_chan_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_ap_info, roam_ap_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_result, roam_result, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_neighbor_report_info, roam_neighbor_report_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_neighbor_report_channel_info, roam_neighbor_report_chan_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_frame_info, roam_frame_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_initial_info, roam_initial_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_btm_response_info, roam_btm_response_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_STATS_EVENTID);

/* Motion detection cmd */
#define WMITLV_TABLE_WMI_MOTION_DET_CONFIG_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_motion_det_config_params_cmd_fixed_param, wmi_motion_det_config_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MOTION_DET_CONFIG_PARAM_CMDID);

#define WMITLV_TABLE_WMI_MOTION_DET_BASE_LINE_CONFIG_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_motion_det_base_line_config_params_cmd_fixed_param, wmi_motion_det_base_line_config_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MOTION_DET_BASE_LINE_CONFIG_PARAM_CMDID);

#define WMITLV_TABLE_WMI_MOTION_DET_START_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_motion_det_start_stop_cmd_fixed_param, wmi_motion_det_start_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MOTION_DET_START_STOP_CMDID);

#define WMITLV_TABLE_WMI_MOTION_DET_BASE_LINE_START_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_motion_det_base_line_start_stop_cmd_fixed_param, wmi_motion_det_base_line_start_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MOTION_DET_BASE_LINE_START_STOP_CMDID);

/* Pdev Set AC TX Queue Optimized Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_AC_TX_QUEUE_OPTIMIZED_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_ac_tx_queue_optimized_cmd_fixed_param, wmi_pdev_set_ac_tx_queue_optimized_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_AC_TX_QUEUE_OPTIMIZED_CMDID);

/* Set msduq qdepth threshold value Cmd */
#define WMITLV_TABLE_WMI_PEER_TID_MSDUQ_QDEPTH_THRESH_UPDATE_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tid_msduq_qdepth_thresh_update_cmd_fixed_param, wmi_peer_tid_msduq_qdepth_thresh_update_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_msduq_qdepth_thresh_update, msduq_qdepth_thresh_update, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TID_MSDUQ_QDEPTH_THRESH_UPDATE_CMDID);

/* Pdev Set RX filter promiscuous Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_RX_FILTER_PROMISCUOUS_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_rx_filter_promiscuous_cmd_fixed_param, wmi_pdev_set_rx_filter_promiscuous_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_RX_FILTER_PROMISCUOUS_CMDID);

/* Runtime DPD Recalibration Params */
#define WMITLV_TABLE_WMI_RUNTIME_DPD_RECAL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_runtime_dpd_recal_cmd_fixed_param, wmi_runtime_dpd_recal_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RUNTIME_DPD_RECAL_CMDID);

/** Get TX power Cmd */
#define WMITLV_TABLE_WMI_GET_TPC_POWER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_tpc_power_cmd_fixed_param, wmi_get_tpc_power_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_GET_TPC_POWER_CMDID);

/* TWT enable cmd */
#define WMITLV_TABLE_WMI_TWT_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_enable_cmd_fixed_param, wmi_twt_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_ENABLE_CMDID);

/* TWT disable cmd */
#define WMITLV_TABLE_WMI_TWT_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_disable_cmd_fixed_param, wmi_twt_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_DISABLE_CMDID);

/* TWT add dialog cmd */
#define WMITLV_TABLE_WMI_TWT_ADD_DIALOG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_add_dialog_cmd_fixed_param, wmi_twt_add_dialog_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_ADD_DIALOG_CMDID);

/* TWT delete dialog cmd */
#define WMITLV_TABLE_WMI_TWT_DEL_DIALOG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_del_dialog_cmd_fixed_param, wmi_twt_del_dialog_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_DEL_DIALOG_CMDID);

/* TWT pause dialog cmd */
#define WMITLV_TABLE_WMI_TWT_PAUSE_DIALOG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_pause_dialog_cmd_fixed_param, wmi_twt_pause_dialog_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_PAUSE_DIALOG_CMDID);

/* TWT resume dialog cmd */
#define WMITLV_TABLE_WMI_TWT_RESUME_DIALOG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_resume_dialog_cmd_fixed_param, wmi_twt_resume_dialog_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_RESUME_DIALOG_CMDID);

/* B-TWT STA invitation cmd */
#define WMITLV_TABLE_WMI_TWT_BTWT_INVITE_STA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_btwt_invite_sta_cmd_fixed_param, wmi_twt_btwt_invite_sta_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_BTWT_INVITE_STA_CMDID);

/* B-TWT STA removal cmd */
#define WMITLV_TABLE_WMI_TWT_BTWT_REMOVE_STA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_btwt_remove_sta_cmd_fixed_param, wmi_twt_btwt_remove_sta_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_BTWT_REMOVE_STA_CMDID);

/* Set peer tid configurations Cmd */
#define WMITLV_TABLE_WMI_PEER_TID_CONFIGURATIONS_CMDID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tid_configurations_cmd_fixed_param, wmi_peer_tid_configurations_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TID_CONFIGURATIONS_CMDID);

/* OBSS_PD Spatial_Reuse cnmd */
#define WMITLV_TABLE_WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_obss_spatial_reuse_set_cmd_fixed_param, wmi_obss_spatial_reuse_set_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID);

/* Peer CFR capture cmd */
#define WMITLV_TABLE_WMI_PEER_CFR_CAPTURE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_cfr_capture_cmd_fixed_param, wmi_peer_cfr_capture_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
/*
 * NOTE: from approximately 7/25/19 to 8/16/19, the below TLV was incorrectly
 * added to this WMI_PEER_CFR_CAPTURE_CMDID message.
 * Any targets compiled with these versions of the WMI definitions will
 * expect that if there is a 2nd TLV in the WMI_PEER_CFR_CAPTURE_CMDID,
 * it will be
 * WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_phase_fixed_param
 * If in the future a new 2nd TLV is added to this WMI_PEER_CFR_CAPTURE_CMDID
 * message, that would cause a target compiled with the erroneous old
 * definitions to reject the WMI_PEER_CFR_CAPTURE_CMDID message containing
 * the new 2nd TLV, because it would not match the
 * WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_phase_fixed_param
 * tag expected by the old target.
 *
 *  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_phase_fixed_param, wmi_peer_cfr_capture_event_phase_fixed_param, phase_param, WMITLV_SIZE_FIX)
 */
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CFR_CAPTURE_CMDID);

/* CHANNEL WIDTH SWITCH commands for peers. */
#define WMITLV_TABLE_WMI_PEER_CHAN_WIDTH_SWITCH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_chan_width_switch_cmd_fixed_param, wmi_peer_chan_width_switch_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_chan_width_peer_list, peer_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CHAN_WIDTH_SWITCH_CMDID);

/* OBSS_PD Spatial_Reuse Set Default OBSS Thresholds */
#define WMITLV_TABLE_WMI_PDEV_OBSS_PD_SPATIAL_REUSE_SET_DEF_OBSS_THRESH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_obss_spatial_reuse_set_def_obss_thresh_cmd_fixed_param, wmi_obss_spatial_reuse_set_def_obss_thresh_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_OBSS_PD_SPATIAL_REUSE_SET_DEF_OBSS_THRESH_CMDID);

/* HE TB Action frame enable/disable */
#define WMITLV_TABLE_WMI_PDEV_HE_TB_ACTION_FRM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_he_tb_action_frm_cmd_fixed_param, \
        wmi_pdev_he_tb_action_frm_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_HE_TB_ACTION_FRM_CMDID);

/* HPCS Pulse cmd */
#define WMITLV_TABLE_WMI_HPCS_PULSE_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hpcs_pulse_start_cmd_fixed_param, wmi_hpcs_pulse_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HPCS_PULSE_START_CMDID);

/* PDev Packet Log filter Cmd */
#define WMITLV_TABLE_WMI_PDEV_PKTLOG_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_pktlog_filter_cmd_fixed_param, wmi_pdev_pktlog_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_pktlog_filter_info, pdev_pktlog_filter_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_PKTLOG_FILTER_CMDID);

/* PDev Set Rogue AP Config cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_RAP_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_rap_config_fixed_param, wmi_pdev_set_rap_config_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_set_rap_config_on_sta_ps_tlv_param, rap_param, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_RAP_CONFIG_CMDID);

/* OEM Data config cmd */
#define WMITLV_TABLE_WMI_OEM_DATA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oem_data_cmd_fixed_param, wmi_oem_data_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, oem_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_DATA_CMDID);

/* Set OCL cmd */
#define WMITLV_TABLE_WMI_SET_OCL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_set_ocl_cmd_fixed_param, wmi_set_ocl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_OCL_CMDID);

/* Set ELNA BYPASS cmd */
#define WMITLV_TABLE_WMI_SET_ELNA_BYPASS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_set_elna_bypass_cmd_fixed_param, wmi_set_elna_bypass_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_ELNA_BYPASS_CMDID);

/* Get ELNA BYPASS cmd */
#define WMITLV_TABLE_WMI_GET_ELNA_BYPASS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_get_elna_bypass_cmd_fixed_param, wmi_get_elna_bypass_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GET_ELNA_BYPASS_CMDID);

/* Get channel ANI level cmd */
#define WMITLV_TABLE_WMI_GET_CHANNEL_ANI_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_get_channel_ani_cmd_fixed_param, wmi_get_channel_ani_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_GET_CHANNEL_ANI_CMDID);

/* Audio aggr config cmd */
#define WMITLV_TABLE_WMI_AUDIO_AGGR_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_enable_cmd_fixed_param, wmi_audio_aggr_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_ENABLE_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_ADD_GROUP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_add_group,  wmi_audio_aggr_add_group_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, client_addr, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_ADD_GROUP_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_DEL_GROUP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_del_group, wmi_audio_aggr_del_group_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_DEL_GROUP_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SET_GROUP_RATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_rate, wmi_audio_aggr_set_group_rate_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_AUDIO_AGGR_RATE_SET_T, rate_set, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SET_GROUP_RATE_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SET_GROUP_RETRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_retry, wmi_audio_aggr_set_group_retry_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SET_GROUP_RETRY_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SET_GROUP_AUTO_RATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_auto_rate, wmi_audio_aggr_set_group_auto_rate_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SET_GROUP_AUTO_RATE_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SET_GROUP_PROBE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_set_group_probe, wmi_audio_aggr_set_group_probe_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SET_GROUP_PROBE_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_UPDATE_STA_GROUP_INFO_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_update_sta_group_info,  wmi_audio_aggr_update_sta_group_info_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, au_groups, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_UPDATE_STA_GROUP_INFO_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_GET_STATISTICS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_get_statistics, wmi_audio_aggr_get_statistics_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_GET_STATISTICS_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_RESET_STATISTICS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_reset_statistics, wmi_audio_aggr_reset_statistics_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_RESET_STATISTICS_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SET_RTSCTS_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_set_rtscts_config, wmi_audio_aggr_set_rtscts_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SET_RTSCTS_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SET_SCHED_METHOD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_set_sched_method,wmi_audio_aggr_set_sched_method_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SET_SCHED_METHOD_CMDID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_GET_SCHED_METHOD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_get_sched_method,wmi_audio_aggr_get_sched_method_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_GET_SCHED_METHOD_CMDID);

/* CFR Capture Filter cmd */
#define WMITLV_TABLE_WMI_CFR_CAPTURE_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_cfr_capture_filter_cmd_fixed_param, wmi_cfr_capture_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_cfr_filter_group_config, filter_group_config, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_CFR_CAPTURE_FILTER_CMDID);

/* VLAN config */
#define WMITLV_TABLE_WMI_PEER_CONFIG_VLAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_config_vlan_cmd_fixed_param, wmi_peer_config_vlan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CONFIG_VLAN_CMDID);

#define WMITLV_TABLE_WMI_VDEV_AUDIO_SYNC_TRIGGER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_sync_trigger_cmd_fixed_param, wmi_audio_sync_trigger_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_AUDIO_SYNC_TRIGGER_CMDID);

#define WMITLV_TABLE_WMI_VDEV_AUDIO_SYNC_QTIMER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_sync_qtimer_cmd_fixed_param, wmi_audio_sync_qtimer_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_AUDIO_SYNC_QTIMER_CMDID);

/* Preferred channel list for vdev */
#define WMITLV_TABLE_WMI_VDEV_SET_PCL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_pcl_cmd_fixed_param, wmi_vdev_set_pcl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_weight, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_PCL_CMDID);

/* Get roam scan channel list */
#define WMITLV_TABLE_WMI_ROAM_GET_SCAN_CHANNEL_LIST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_get_scan_channel_list_cmd_fixed_param, wmi_roam_get_scan_channel_list_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_GET_SCAN_CHANNEL_LIST_CMDID);

/* VDEV_GET_BIG_DATA_CMD IS DEPRECATED - DO NOT USE */
#define WMITLV_TABLE_WMI_VDEV_GET_BIG_DATA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_big_data_cmd_fixed_param, wmi_vdev_get_big_data_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_BIG_DATA_CMDID);

/* Get per vdev BIG DATA stats P2 */
#define WMITLV_TABLE_WMI_VDEV_GET_BIG_DATA_P2_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_big_data_p2_cmd_fixed_param, wmi_vdev_get_big_data_p2_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_BIG_DATA_P2_CMDID);

/* Frame inject command */
#define WMITLV_TABLE_WMI_PDEV_FRAME_INJECT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_frame_inject_cmd_fixed_param, wmi_frame_inject_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FRAME_INJECT_CMDID);

/* Get / Set RNR TBTT offset info */
#define WMITLV_TABLE_WMI_PDEV_TBTT_OFFSET_SYNC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_tbtt_offset_sync_cmd_fixed_param, wmi_pdev_tbtt_offset_sync_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_rnr_bss_tbtt_info, rnr_tbtt_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_TBTT_OFFSET_SYNC_CMDID);

/* SRG BSS color Bitmap */
#define WMITLV_TABLE_WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_srg_bss_color_bitmap_cmd_fixed_param, wmi_pdev_srg_bss_color_bitmap_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID);

/* SRG Partial BSSid Bitmap */
#define WMITLV_TABLE_WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_srg_partial_bssid_bitmap_cmd_fixed_param, wmi_pdev_srg_partial_bssid_bitmap_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID);

/* Simulation test command */
#define WMITLV_TABLE_WMI_SIMULATION_TEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_simulation_test_cmd_fixed_param, wmi_simulation_test_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SIMULATION_TEST_CMDID);

/* WFA test config command */
#define WMITLV_TABLE_WMI_WFA_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wfa_config_cmd_fixed_param, wmi_wfa_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wfa_config_rsnxe, wfa_config_rsnxe, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wfa_config_csa, wfa_config_csa, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wfa_config_ocv, wfa_config_ocv, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wfa_config_saquery, wfa_config_saquery, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WFA_CONFIG_CMDID);

/* SRG OBSS color Enable Bitmap */
#define WMITLV_TABLE_WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_srg_obss_color_enable_bitmap_cmd_fixed_param, wmi_pdev_srg_obss_color_enable_bitmap_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);

/* SRG OBSS BSSID Enable Bitmap */
#define WMITLV_TABLE_WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_srg_obss_bssid_enable_bitmap_cmd_fixed_param, wmi_pdev_srg_obss_bssid_enable_bitmap_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);

/* Non_SRG OBSS color Enable Bitmap */
#define WMITLV_TABLE_WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_non_srg_obss_color_enable_bitmap_cmd_fixed_param, wmi_pdev_non_srg_obss_color_enable_bitmap_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);

/* Non_SRG OBSS BSSID Enable Bitmap */
#define WMITLV_TABLE_WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_non_srg_obss_bssid_enable_bitmap_cmd_fixed_param, wmi_pdev_non_srg_obss_bssid_enable_bitmap_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);

/* PDEV Get TPC STATS Cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_TPC_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_tpc_stats_cmd_fixed_param, wmi_pdev_get_tpc_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TPC_STATS_CMDID);

/* Duration based tx mode selection Enable Bitmap */
#define WMITLV_TABLE_WMI_PDEV_ENABLE_DURATION_BASED_TX_MODE_SELECTION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param, wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_ENABLE_DURATION_BASED_TX_MODE_SELECTION_CMDID);


/************************** TLV definitions of WMI events *******************************/

/* Service Ready event */
#define WMITLV_TABLE_WMI_SERVICE_READY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param, wmi_service_ready_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)     \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES, HAL_REG_CAPABILITIES, hal_reg_capabilities, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wmi_service_bitmap, WMITLV_SIZE_FIX, WMI_SERVICE_BM_SIZE) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_host_mem_req, mem_reqs, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wlan_dbs_hw_mode_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EVENTID);

/* service available event */
#define WMITLV_TABLE_WMI_SERVICE_AVAILABLE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_available_event_fixed_param, wmi_service_available_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    /* \
     * The wmi_service_ext_bitmap covers WMI service bits beyond the range \
     * of the fixed_param.wmi_service_segment_bitmap[]. \
     */ \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wmi_service_ext_bitmap, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_AVAILABLE_EVENTID);

/* Service Ready Extension event */
#define WMITLV_TABLE_WMI_SERVICE_READY_EXT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_ext_event_fixed_param, wmi_service_ready_ext_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SOC_MAC_PHY_HW_MODE_CAPS, WMI_SOC_MAC_PHY_HW_MODE_CAPS, soc_hw_mode_caps, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_HW_MODE_CAPABILITIES, hw_mode_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_MAC_PHY_CAPABILITIES, mac_phy_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SOC_HAL_REG_CAPABILITIES, WMI_SOC_HAL_REG_CAPABILITIES, soc_hal_reg_caps, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_HAL_REG_CAPABILITIES_EXT, hal_reg_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_MAC_PHY_CHAINMASK_COMBO, mac_phy_chainmask_combo, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_MAC_PHY_CHAINMASK_CAPABILITY, mac_phy_chainmask_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_OEM_DMA_RING_CAPABILITIES, oem_dma_ring_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SAR_CAPABILITIES, WMI_SAR_CAPABILITIES, sar_caps, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_DMA_RING_CAPABILITIES, dma_ring_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_spectral_bin_scaling_params, wmi_bin_scaling_params, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_CHAN_RF_CHARACTERIZATION_INFO, wmi_chan_rf_characterization_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EXT_EVENTID);

/* SERVICE_READY_EXT2 event */
#define WMITLV_TABLE_WMI_SERVICE_READY_EXT2_EVENTID(id,op,buf,len) \
     WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_ext2_event_fixed_param, wmi_service_ready_ext2_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_DMA_RING_CAPABILITIES, dma_ring_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_spectral_bin_scaling_params, wmi_bin_scaling_params, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_MAC_PHY_CAPABILITIES_EXT, mac_phy_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_HAL_REG_CAPABILITIES_EXT2, hal_reg_caps, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_capabilities, wmi_nan_capabilities, nan_cap, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_SCAN_RADIO_CAPABILITIES_EXT2, wmi_scan_radio_caps, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EXT2_EVENTID);

#define WMITLV_TABLE_WMI_CHAN_RF_CHARACTERIZATION_INFO_EVENTID(id,op,buf,len) \
     WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chan_rf_characterization_info_event_fixed_param, wmi_chan_rf_characterization_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
     WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_CHAN_RF_CHARACTERIZATION_INFO, wmi_chan_rf_characterization_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_CHAN_RF_CHARACTERIZATION_INFO_EVENTID);

/* iface combination indication event */
#define WMITLV_TABLE_WMI_IFACE_COMBINATION_IND_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlanfw_iface_cmb_ind_event_fixed_param, wmi_wlanfw_iface_cmb_ind_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlanfw_iface_combination, combinations, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlanfw_iface_limit, limits, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_IFACE_COMBINATION_IND_EVENTID);

/* Ready event */
#define WMITLV_TABLE_WMI_READY_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ready_event_fixed_param, wmi_ready_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, mac_addr_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_READY_EVENTID);

/* Scan Event */
#define WMITLV_TABLE_WMI_SCAN_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_event_fixed_param, wmi_scan_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_EVENTID);

/* ExtScan Start/Stop Event */
#define WMITLV_TABLE_WMI_EXTSCAN_START_STOP_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param, wmi_extscan_start_stop_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_START_STOP_EVENTID);

/* ExtScan Event */
#define WMITLV_TABLE_WMI_EXTSCAN_OPERATION_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param, wmi_extscan_operation_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, bucket_id, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_OPERATION_EVENTID);

/* ExtScan Table Usage Event */
#define WMITLV_TABLE_WMI_EXTSCAN_TABLE_USAGE_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param, wmi_extscan_table_usage_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_TABLE_USAGE_EVENTID);

/* ExtScan Result Event */
#define WMITLV_TABLE_WMI_EXTSCAN_CACHED_RESULTS_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param, wmi_extscan_cached_results_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_descriptor, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_rssi_info, rssi_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CACHED_RESULTS_EVENTID);

/* ExtScan Monitor RSSI List Event */
#define WMITLV_TABLE_WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param, wmi_extscan_wlan_change_results_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_result_bssid, bssid_signal_descriptor_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, rssi_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID);

/* ExtScan Hot List Match Event */
#define WMITLV_TABLE_WMI_EXTSCAN_HOTLIST_MATCH_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param, wmi_extscan_hotlist_match_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_descriptor, hotlist_match, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_HOTLIST_MATCH_EVENTID);

/* ExtScan Hot List Match Event */
#define WMITLV_TABLE_WMI_EXTSCAN_CAPABILITIES_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param, wmi_extscan_capabilities_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_cache_capabilities, extscan_cache_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_monitor_capabilities, wlan_change_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_monitor_capabilities, hotlist_capabilities, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CAPABILITIES_EVENTID);

/* ExtScan Hot List Match Event */
#define WMITLV_TABLE_WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_hotlist_ssid_match_event_fixed_param, wmi_extscan_hotlist_ssid_match_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_descriptor, hotlist_ssid_match, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID);

/* Update_whal_mib_stats Event */
#define WMITLV_TABLE_WMI_UPDATE_WHAL_MIB_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_update_whal_mib_stats_event_fixed_param, wmi_update_whal_mib_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_WHAL_MIB_STATS_EVENTID);

/* PDEV TPC Config Event */
#define WMITLV_TABLE_WMI_PDEV_TPC_CONFIG_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_tpc_config_event_fixed_param, wmi_pdev_tpc_config_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ratesArray, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tlv_arrays_len_param, ctlPwrTbl_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ctlPwrTbl_buf, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_TPC_CONFIG_EVENTID);

/* PDEV DIV RSSI Antenna index Event */
#define WMITLV_TABLE_WMI_PDEV_DIV_RSSI_ANTID_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_div_rssi_antid_event_fixed_param, wmi_pdev_div_rssi_antid_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DIV_RSSI_ANTID_EVENTID);

/* PDEV BSS CHAN info Event */
#define WMITLV_TABLE_WMI_PDEV_BSS_CHAN_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_bss_chan_info_event_fixed_param, wmi_pdev_bss_chan_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_BSS_CHAN_INFO_EVENTID);

/* PDEV update ctl table Event */
#define WMITLV_TABLE_WMI_PDEV_UPDATE_CTLTABLE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_update_ctltable_event_fixed_param, wmi_pdev_update_ctltable_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UPDATE_CTLTABLE_EVENTID);

/* VDEV Tx Power Event */
#define WMITLV_TABLE_WMI_VDEV_GET_TX_POWER_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_tx_power_event_fixed_param, wmi_vdev_get_tx_power_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_TX_POWER_EVENTID);

/* Channel Info Event */
#define WMITLV_TABLE_WMI_CHAN_INFO_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chan_info_event_fixed_param, wmi_chan_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CHAN_INFO_EVENTID);

/* Phy Error Event */
#define WMITLV_TABLE_WMI_PHYERR_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_comb_phyerr_rx_hdr, wmi_comb_phyerr_rx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_single_phyerr_ext_rx_hdr, single_phyerr_ext, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PHYERR_EVENTID);

/* TX Pause/Unpause event */
#define WMITLV_TABLE_WMI_TX_PAUSE_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_pause_event_fixed_param, wmi_tx_pause_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TX_PAUSE_EVENTID);

/* Mgmt TX completion event */
#define WMITLV_TABLE_WMI_MGMT_TX_COMPLETION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_compl_event_fixed_param, wmi_mgmt_tx_compl_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_mgmt_hdr, mgmt_hdr, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_COMPLETION_EVENTID);

/* offchan data TX completion event */
#define WMITLV_TABLE_WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offchan_data_tx_compl_event_fixed_param, wmi_offchan_data_tx_compl_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID);

/* Bundled Mgmt TX completion event */
#define WMITLV_TABLE_WMI_MGMT_TX_BUNDLE_COMPLETION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_compl_bundle_event_fixed_param, wmi_mgmt_tx_compl_bundle_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, desc_ids, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, status, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ppdu_id, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ack_rssi, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_mgmt_hdr, mgmt_hdr, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_BUNDLE_COMPLETION_EVENTID);

/* QoS null frame TX completion event */
#define WMITLV_TABLE_WMI_QOS_NULL_FRAME_TX_COMPLETION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_qos_null_frame_tx_compl_event_fixed_param, wmi_qos_null_frame_tx_compl_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_QOS_NULL_FRAME_TX_COMPLETION_EVENTID);

/* VDEV Start response Event */
#define WMITLV_TABLE_WMI_VDEV_START_RESP_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_start_response_event_fixed_param, wmi_vdev_start_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_START_RESP_EVENTID);

/* VDEV Stopped Event */
#define WMITLV_TABLE_WMI_VDEV_STOPPED_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_stopped_event_fixed_param, wmi_vdev_stopped_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_STOPPED_EVENTID);

/* VDEV delete response Event */
#define WMITLV_TABLE_WMI_VDEV_DELETE_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_delete_resp_event_fixed_param, wmi_vdev_delete_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DELETE_RESP_EVENTID);

/* VDEV Install Key Complete Event */
#define WMITLV_TABLE_WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_install_key_complete_event_fixed_param, wmi_vdev_install_key_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID);

/* VDEV disconnect Event */
#define WMITLV_TABLE_WMI_VDEV_DISCONNECT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_disconnect_event_fixed_param, wmi_vdev_disconnect_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DISCONNECT_EVENTID);

/* Peer STA Kickout Event */
#define WMITLV_TABLE_WMI_PEER_STA_KICKOUT_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_sta_kickout_event_fixed_param, wmi_peer_sta_kickout_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_STA_KICKOUT_EVENTID);

/* Management Rx Event */
#define WMITLV_TABLE_WMI_MGMT_RX_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_rx_hdr, wmi_mgmt_rx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_rssi_ctl_ext, rssi_ctl_ext, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_RX_EVENTID);

/* TBTT offset Event */
#define WMITLV_TABLE_WMI_TBTTOFFSET_UPDATE_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tbtt_offset_event_fixed_param, wmi_tbtt_offset_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tbttoffset_list, WMITLV_SIZE_FIX, WMI_MAX_AP_VDEV) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tbtt_qtime_low_us_list, WMITLV_SIZE_FIX, WMI_MAX_AP_VDEV) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tbtt_qtime_high_us_list, WMITLV_SIZE_FIX, WMI_MAX_AP_VDEV)
WMITLV_CREATE_PARAM_STRUC(WMI_TBTTOFFSET_UPDATE_EVENTID);

/* TBTT EXT offset Event */
#define WMITLV_TABLE_WMI_TBTTOFFSET_EXT_UPDATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tbtt_offset_ext_event_fixed_param, wmi_tbtt_offset_ext_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tbtt_offset_info, tbtt_offset_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TBTTOFFSET_EXT_UPDATE_EVENTID);

/* TX DELBA Complete Event */
#define WMITLV_TABLE_WMI_TX_DELBA_COMPLETE_EVENTID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_delba_complete_event_fixed_param, wmi_tx_delba_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TX_DELBA_COMPLETE_EVENTID);

/* Tx ADDBA Complete Event */
#define WMITLV_TABLE_WMI_TX_ADDBA_COMPLETE_EVENTID(id,op,buf,len)                                       \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_addba_complete_event_fixed_param, wmi_tx_addba_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TX_ADDBA_COMPLETE_EVENTID);

/* ADD BA Req ssn Event */
#define WMITLV_TABLE_WMI_BA_RSP_SSN_EVENTID(id,op,buf,len)                                       \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ba_rsp_ssn_event_fixed_param, wmi_ba_rsp_ssn_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_STRUC, wmi_ba_event_ssn, ba_event_ssn_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_BA_RSP_SSN_EVENTID);

/* Aggregation Request event */
#define WMITLV_TABLE_WMI_AGGR_STATE_TRIG_EVENTID(id,op,buf,len)                                       \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_aggr_state_trig_event_fixed_param, wmi_aggr_state_trig_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AGGR_STATE_TRIG_EVENTID);

/* Roam Event */
#define WMITLV_TABLE_WMI_ROAM_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_event_fixed_param, wmi_roam_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, deauth_disassoc_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_hw_mode_transition_event_fixed_param, hw_mode_transition_fixed_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_set_hw_mode_response_vdev_mac_entry, wmi_pdev_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_EVENTID);

/* Roam Synch Event */
#define WMITLV_TABLE_WMI_ROAM_SYNCH_EVENTID(id,op,buf,len)                                                      \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_synch_event_fixed_param, wmi_roam_synch_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bcn_probe_rsp_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, reassoc_rsp_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_key_material, key, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, status, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, reassoc_req_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_hw_mode_transition_event_fixed_param, hw_mode_transition_fixed_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_set_hw_mode_response_vdev_mac_entry, wmi_pdev_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_fils_synch_tlv_param, roam_fils_synch_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_key_material_ext, key_ext, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_pmk_cache_synch_tlv_param, roam_pmk_cache_synch_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SYNCH_EVENTID);

/* Roam Synch frame Event */
#define WMITLV_TABLE_WMI_ROAM_SYNCH_FRAME_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_synch_frame_event_fixed_param, wmi_roam_synch_frame_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bcn_probe_rsp_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, reassoc_rsp_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, reassoc_req_frame, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SYNCH_FRAME_EVENTID);

/* WOW Wakeup Host Event */
/* NOTE: Make sure wow_bitmap_info can be zero or one elements only */
#define WMITLV_TABLE_WMI_WOW_WAKEUP_HOST_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WOW_EVENT_INFO_fixed_param, WOW_EVENT_INFO_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_EVENT_INFO_SECTION_BITMAP, wow_bitmap_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, wow_packet_buffer, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_hb_ind_event_fixed_param, hb_indevt, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param, wow_gtkigtk, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_oic_ping_handoff_event, wow_oic_ping_handoff, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dhcp_lease_renew_event, wow_dhcp_lease_renew, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_motion_det_event, md_indevt, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_motion_det_base_line_event, bl_indevt, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_WAKEUP_HOST_EVENTID);

#define WMITLV_TABLE_WMI_WOW_INITIAL_WAKEUP_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WOW_EVENT_INITIAL_WAKEUP_fixed_param, WOW_INITIAL_WAKEUP_EVENT_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_INITIAL_WAKEUP_EVENTID);

/* RTT error report Event */
#define WMITLV_TABLE_WMI_RTT_ERROR_REPORT_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RTT_ERROR_REPORT_EVENTID);

/* Echo Event */
#define WMITLV_TABLE_WMI_ECHO_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_echo_event_fixed_param, wmi_echo_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ECHO_EVENTID);

/* FTM Integration Event */
#define WMITLV_TABLE_WMI_PDEV_FTM_INTG_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ftm_intg_event_fixed_param, wmi_ftm_intg_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FTM_INTG_EVENTID);

/* VDEV get Keepalive Event */
#define WMITLV_TABLE_WMI_VDEV_GET_KEEPALIVE_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_event_fixed_param, wmi_vdev_get_keepalive_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_KEEPALIVE_EVENTID);

/** Get TX power Event */
#define WMITLV_TABLE_WMI_GET_TPC_POWER_EVENTID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_tpc_power_evt_fixed_param, wmi_get_tpc_power_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GET_TPC_POWER_EVENTID);

/** Get MU EDCA report param event */
#define WMITLV_TABLE_WMI_MUEDCA_PARAMS_CONFIG_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_muedca_params_config_event_fixed_param, wmi_muedca_params_config_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MUEDCA_PARAMS_CONFIG_EVENTID);

/* GPIO Input Event */
#define WMITLV_TABLE_WMI_GPIO_INPUT_EVENTID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gpio_input_event_fixed_param, wmi_gpio_input_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GPIO_INPUT_EVENTID);

/* CSA Handling Event */
#define WMITLV_TABLE_WMI_CSA_HANDLING_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_csa_event_fixed_param, wmi_csa_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CSA_HANDLING_EVENTID);

/* Rfkill state change Event */
#define WMITLV_TABLE_WMI_RFKILL_STATE_CHANGE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rfkill_event_fixed_param, wmi_rfkill_mode_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RFKILL_STATE_CHANGE_EVENTID);

/* Smart Antenna state change Event */
#define WMITLV_TABLE_WMI_SMARTANT_STATE_CHANGE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_smartant_event_fixed_param, wmi_smartant_state_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SMARTANT_STATE_CHANGE_EVENTID);

/* Debug Message Event */
#define WMITLV_TABLE_WMI_DEBUG_MESG_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_MESG_EVENTID);

#define WMITLV_TABLE_WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_debug_mesg_flush_complete_fixed_param, wmi_debug_mesg_flush_complete_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_debug_mesg_fw_data_stall_param, data_stall, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID);

#define WMITLV_TABLE_WMI_RSSI_BREACH_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rssi_breach_event_fixed_param, wmi_rssi_breach_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RSSI_BREACH_EVENTID);

#define WMITLV_TABLE_WMI_TRANSFER_DATA_TO_FLASH_COMPLETE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_complete_event_fixed_param, wmi_transfer_data_to_flash_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TRANSFER_DATA_TO_FLASH_COMPLETE_EVENTID);

#define WMITLV_TABLE_WMI_READ_DATA_FROM_FLASH_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_read_data_from_flash_event_fixed_param, wmi_read_data_from_flash_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_READ_DATA_FROM_FLASH_EVENTID);

/* Diagnostics Event */
#define WMITLV_TABLE_WMI_DIAG_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_EVENTID);

/* IGTK Offload Event */
#define WMITLV_TABLE_WMI_GTK_OFFLOAD_STATUS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param, WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GTK_OFFLOAD_STATUS_EVENTID);

/* DCA interferance Event */
#define WMITLV_TABLE_WMI_DCS_INTERFERENCE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcs_interference_event_fixed_param, wmi_dcs_interference_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_dcs_cw_int, cw_int, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_dcs_im_tgt_stats_t, wlan_stat, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCS_INTERFERENCE_EVENTID);

/* Profile data Event */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_DATA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_ctx_t, wmi_wlan_profile_ctx_t, profile_ctx, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wlan_profile_t, profile_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_DATA_EVENTID);

/* PDEV UTF Event */
#define WMITLV_TABLE_WMI_PDEV_UTF_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UTF_EVENTID);

/* Update SCPC calibrated data Event */
#define WMITLV_TABLE_WMI_PDEV_UTF_SCPC_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scpc_event_fixed_param, wmi_scpc_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UTF_SCPC_EVENTID);

/* Debug print Event */
#define WMITLV_TABLE_WMI_DEBUG_PRINT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_PRINT_EVENTID);

/* RTT measurement report Event - DEPRECATED */
#define WMITLV_TABLE_WMI_RTT_MEASUREMENT_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RTT_MEASUREMENT_REPORT_EVENTID);

/*oem measurement report Event - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_MEASUREMENT_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_MEASUREMENT_REPORT_EVENTID);

/*oem error report event - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_ERROR_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_ERROR_REPORT_EVENTID);

/*oem capability report event - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_CAPABILITY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_CAPABILITY_EVENTID);

/*oem response event*/
#define WMITLV_TABLE_WMI_OEM_RESPONSE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oem_indirect_data, wmi_oem_indirect_data, indirect_data, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data2, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_RESPONSE_EVENTID);

/* oem dma buffer release event */
#define WMITLV_TABLE_WMI_OEM_DMA_BUF_RELEASE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oem_dma_buf_release_fixed_param, wmi_oem_dma_buf_release_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_oem_dma_buf_release_entry, entries, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_DMA_BUF_RELEASE_EVENTID);

/* oem data event */
#define WMITLV_TABLE_WMI_OEM_DATA_EVENTID(id,op, buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oem_data_event_fixed_param, wmi_oem_data_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_DATA_EVENTID);

/* HOST SWBA Event */
#define WMITLV_TABLE_WMI_HOST_SWBA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_swba_event_fixed_param, wmi_host_swba_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tim_info, tim_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_info, p2p_noa_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_quiet_offload_info, quiet_offload_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_HOST_SWBA_EVENTID);

/* HOST SWBA Event v2 */
#define WMITLV_TABLE_WMI_HOST_SWBA_V2_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_swba_event_fixed_param, wmi_host_swba_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tim_info_v2, tim_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_info, p2p_noa_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_HOST_SWBA_V2_EVENTID);


/* HOST SWFDA Event  requesting host to queue a FILS Discovery frame for transmission */
#define WMITLV_TABLE_WMI_HOST_SWFDA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_swfda_event_fixed_param, wmi_host_swfda_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_HOST_SWFDA_EVENTID);

/* Update stats Event */
#define WMITLV_TABLE_WMI_UPDATE_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_stats_event_fixed_param, wmi_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_per_chain_rssi_stats, wmi_per_chain_rssi_stats, chain_stats, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_rssi_stats, rssi_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_congestion_stats, congestion_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_extd2_stats, peer_extd2_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pmf_bcn_protect_stats, pmf_bcn_protect_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_extd_stats, vdev_extd_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_STATS_EVENTID);

/* Update PN response Event */
#define WMITLV_TABLE_WMI_PEER_TX_PN_RESPONSE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tx_pn_response_event_fixed_param, wmi_peer_tx_pn_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TX_PN_RESPONSE_EVENTID);

/* For vdev based ht/vht info upload*/
#define WMITLV_TABLE_WMI_UPDATE_VDEV_RATE_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_rate_stats_event_fixed_param, wmi_vdev_rate_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_rate_ht_info, ht_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_VDEV_RATE_STATS_EVENTID);

/* report rx aggregation failure information */
#define WMITLV_TABLE_WMI_REPORT_RX_AGGR_FAILURE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rx_aggr_failure_event_fixed_param, wmi_rx_aggr_failure_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_rx_aggr_failure_info, failure_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REPORT_RX_AGGR_FAILURE_EVENTID);

/* Update memory dump complete Event */
#define  WMITLV_TABLE_WMI_UPDATE_FW_MEM_DUMP_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_update_fw_mem_dump_fixed_param, wmi_update_fw_mem_dump_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_FW_MEM_DUMP_EVENTID);

/* Event indicating the DIAG LOGs/Events supported by FW */
#define WMITLV_TABLE_WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_diag_event_log_supported_event_fixed_params, wmi_diag_event_log_supported_event_fixed_params, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, diag_events_logs_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID);

/* Update iface link stats Event */
#define WMITLV_TABLE_WMI_IFACE_LINK_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_iface_link_stats_event_fixed_param, wmi_iface_link_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_iface_link_stats, iface_link_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wmm_ac_stats, ac, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_iface_offload_stats, iface_offload_stats, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_IFACE_LINK_STATS_EVENTID);

/* Update Peer link stats Event */
#define WMITLV_TABLE_WMI_PEER_LINK_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_stats_event_fixed_param, wmi_peer_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_link_stats, peer_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_rate_stats, peer_rate_stats, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_LINK_STATS_EVENTID);

/* Update radio stats Event */
#define WMITLV_TABLE_WMI_RADIO_LINK_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_radio_link_stats_event_fixed_param, wmi_radio_link_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_radio_link_stats, radio_stats, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel_stats, channel_stats, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RADIO_LINK_STATS_EVENTID);

/* Update WLM stats event */
#define WMITLV_TABLE_WMI_WLM_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlm_stats_event_fixed_param, wmi_wlm_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WLM_STATS_EVENTID);

/* PDEV QVIT Event */
#define WMITLV_TABLE_WMI_PDEV_QVIT_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_QVIT_EVENTID);

/* WLAN Frequency avoid Event */
#define WMITLV_TABLE_WMI_WLAN_FREQ_AVOID_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_avoid_freq_ranges_event_fixed_param, wmi_avoid_freq_ranges_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_avoid_freq_range_desc, avd_freq_range, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_FREQ_AVOID_EVENTID);

/* WLAN SAR LIMITS GET response Event */
#define WMITLV_TABLE_WMI_SAR_GET_LIMITS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sar_get_limits_event_fixed_param, wmi_sar_get_limits_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_sar_get_limit_event_row, sar_get_limits, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SAR_GET_LIMITS_EVENTID);

/* GTK rekey fail Event */
#define WMITLV_TABLE_WMI_GTK_REKEY_FAIL_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gtk_rekey_fail_event_fixed_param, wmi_gtk_rekey_fail_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GTK_REKEY_FAIL_EVENTID);

/* NLO match event */
#define WMITLV_TABLE_WMI_NLO_MATCH_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_event, wmi_nlo_event, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_NLO_MATCH_EVENTID);

/* NLO scan complete event */
#define WMITLV_TABLE_WMI_NLO_SCAN_COMPLETE_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_event, wmi_nlo_event, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_NLO_SCAN_COMPLETE_EVENTID);

/* APFIND event */
#define WMITLV_TABLE_WMI_APFIND_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_apfind_event_hdr, wmi_apfind_event_hdr, hdr, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_APFIND_EVENTID);

/* WMI_PASSPOINT_MATCH_EVENTID */
#define WMITLV_TABLE_WMI_PASSPOINT_MATCH_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_passpoint_event_hdr, wmi_passpoint_event_hdr, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_PASSPOINT_MATCH_EVENTID);

/* Chatter query reply event */
#define WMITLV_TABLE_WMI_CHATTER_PC_QUERY_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_query_reply_event_fixed_param, wmi_chatter_query_reply_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_PC_QUERY_EVENTID);

/* Upload H_CV info event */
#define WMITLV_TABLE_WMI_UPLOADH_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_upload_h_hdr, wmi_upload_h_hdr, hdr, WMITLV_SIZE_FIX)   \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_UPLOADH_EVENTID);

/* Capture H info event */
#define WMITLV_TABLE_WMI_CAPTUREH_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_capture_h_event_hdr, wmi_capture_h_event_hdr, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_CAPTUREH_EVENTID);

/* TDLS Peer Update event */
#define WMITLV_TABLE_WMI_TDLS_PEER_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_peer_event_fixed_param, wmi_tdls_peer_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_PEER_EVENTID);

/* VDEV MCC Beacon Interval Change Request Event */
#define WMITLV_TABLE_WMI_VDEV_MCC_BCN_INTERVAL_CHANGE_REQ_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_mcc_bcn_intvl_change_event_fixed_param, wmi_vdev_mcc_bcn_intvl_change_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_MCC_BCN_INTERVAL_CHANGE_REQ_EVENTID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_ENABLED_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_enabled_event_fixed_param, wmi_batch_scan_enabled_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_ENABLED_EVENTID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_RESULT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_result_event_fixed_param, wmi_batch_scan_result_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_ARRAY_STRUC, wmi_batch_scan_result_scan_list, scan_list, WMITLV_SIZE_VAR)    \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_ARRAY_STRUC, wmi_batch_scan_result_network_info, network_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_RESULT_EVENTID);

#define WMITLV_TABLE_WMI_OFFLOAD_BCN_TX_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offload_bcn_tx_status_event_fixed_param, wmi_offload_bcn_tx_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OFFLOAD_BCN_TX_STATUS_EVENTID);

/* NOA Event */
#define WMITLV_TABLE_WMI_P2P_NOA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_noa_event_fixed_param, wmi_p2p_noa_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_noa_info, wmi_p2p_noa_info, p2p_noa_info, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_NOA_EVENTID);

/* AP PS enhanced green ap Event */
#define WMITLV_TABLE_WMI_AP_PS_EGAP_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ap_ps_egap_info_event_fixed_param, wmi_ap_ps_egap_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ap_ps_egap_info_chainmask_list, chainmask_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_AP_PS_EGAP_INFO_EVENTID);

#define WMITLV_TABLE_WMI_PEER_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_info_event_fixed_param, wmi_peer_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)               \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_info, peer_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_INFO_EVENTID);

#define WMITLV_TABLE_WMI_PEER_ANTDIV_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_antdiv_info_event_fixed_param, wmi_peer_antdiv_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_antdiv_info, peer_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ANTDIV_INFO_EVENTID);

#define WMITLV_TABLE_WMI_PEER_TX_FAIL_CNT_THR_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tx_fail_cnt_thr_event_fixed_param, wmi_peer_tx_fail_cnt_thr_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TX_FAIL_CNT_THR_EVENTID);

#define WMITLV_TABLE_WMI_PEER_OPER_MODE_CHANGE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_oper_mode_change_event_fixed_param, wmi_peer_oper_mode_change_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_OPER_MODE_CHANGE_EVENTID);

/* DFS radar Event */
#define WMITLV_TABLE_WMI_DFS_RADAR_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dfs_radar_event_fixed_param, wmi_dfs_radar_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DFS_RADAR_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_DFS_RADAR_DETECTION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_radar_detection_event_fixed_param, wmi_pdev_dfs_radar_detection_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_RADAR_DETECTION_EVENTID);

#define WMITLV_TABLE_WMI_VDEV_ADFS_OCAC_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_adfs_ocac_complete_event_fixed_param, wmi_vdev_adfs_ocac_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ADFS_OCAC_COMPLETE_EVENTID);

#define WMITLV_TABLE_WMI_VDEV_DFS_CAC_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_dfs_cac_complete_event_fixed_param, wmi_vdev_dfs_cac_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DFS_CAC_COMPLETE_EVENTID);

/* Thermal Event */
#define WMITLV_TABLE_WMI_THERMAL_MGMT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_thermal_mgmt_event_fixed_param, wmi_thermal_mgmt_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_THERMAL_MGMT_EVENTID);

#define WMITLV_TABLE_WMI_OEM_DMA_RING_CFG_RSP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_oem_dma_ring_cfg_rsp_fixed_param, wmi_oem_dma_ring_cfg_rsp_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_DMA_RING_CFG_RSP_EVENTID)

/* NAN Response/Indication Event */
#define WMITLV_TABLE_WMI_NAN_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_event_hdr, wmi_nan_event_hdr, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_nan_event_info, event_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_EVENTID);

/* NAN discovery interface created event */
#define WMITLV_TABLE_WMI_NAN_DISC_IFACE_CREATED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_disc_iface_created_event_fixed_param, wmi_nan_disc_iface_created_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_DISC_IFACE_CREATED_EVENTID);

/* NAN discovery interface deleted event */
#define WMITLV_TABLE_WMI_NAN_DISC_IFACE_DELETED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_disc_iface_deleted_event_fixed_param, wmi_nan_disc_iface_deleted_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_DISC_IFACE_DELETED_EVENTID);

/* NAN device started new cluster event */
#define WMITLV_TABLE_WMI_NAN_STARTED_CLUSTER_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_started_cluster_event_fixed_param, wmi_nan_started_cluster_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_STARTED_CLUSTER_EVENTID);

/* NAN device joined to cluster event */
#define WMITLV_TABLE_WMI_NAN_JOINED_CLUSTER_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_joined_cluster_event_fixed_param, wmi_nan_joined_cluster_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_JOINED_CLUSTER_EVENTID);

/* Coex report antenna isolation event */
#define WMITLV_TABLE_WMI_COEX_REPORT_ANTENNA_ISOLATION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_coex_report_isolation_event_fixed_param, wmi_coex_report_isolation_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_COEX_REPORT_ANTENNA_ISOLATION_EVENTID);

/* NDP capabilities response event */
#define WMITLV_TABLE_WMI_NDI_CAP_RSP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndi_cap_rsp_event_fixed_param, wmi_ndi_cap_rsp_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDI_CAP_RSP_EVENTID);

/* NDP initiator response event */
#define WMITLV_TABLE_WMI_NDP_INITIATOR_RSP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_initiator_rsp_event_fixed_param, wmi_ndp_initiator_rsp_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_INITIATOR_RSP_EVENTID);

/* NDP responder response event */
#define WMITLV_TABLE_WMI_NDP_RESPONDER_RSP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_responder_rsp_event_fixed_param, wmi_ndp_responder_rsp_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_RESPONDER_RSP_EVENTID);

/** NDP end response event
 *
 * TLV (tag length value ) parameters follow the ndp_end_rsp
 * structure. The TLV's are:
 * wmi_ndp_end_rsp_per_ndi ndp_end_rsp_per_ndi_list[];
 * wmi_active_ndp_instance_id active_ndp_instances_id[];
 */
#define WMITLV_TABLE_WMI_NDP_END_RSP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_end_rsp_event_fixed_param, wmi_ndp_end_rsp_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ndp_end_rsp_per_ndi_PROTOTYPE, ndp_end_rsp_per_ndi_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_active_ndp_instance_id_PROTOTYPE, active_ndp_instances_id, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_END_RSP_EVENTID);

/** NDP indication event
 *
 * TLV (tag length value ) parameters follow the ndp_indication
 * structure. The TLV's are:
 * A_UINT8 ndp_cfg[];
 * A_UINT8 ndp_app_info[];
 * wmi_ndp_transport_ip_param ndp_transport_ip_param;
 */
#define WMITLV_TABLE_WMI_NDP_INDICATION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_indication_event_fixed_param, wmi_ndp_indication_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_cfg, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_app_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_scid, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_transport_ip_param, wmi_ndp_transport_ip_param, ndp_transport_ip_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_INDICATION_EVENTID);

/** NDP confirm event
 * TLV (tag length value ) parameters follow the ndp_confirm
 * structure. The TLV's are:
 * A_UINT8 ndp_cfg[];
 * A_UINT8 ndp_app_info[];
 * wmi_channel ndp_channel_list[];
 * wmi_ndp_transport_ip_param ndp_transport_ip_param;
 */
#define WMITLV_TABLE_WMI_NDP_CONFIRM_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_confirm_event_fixed_param, wmi_ndp_confirm_event_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_cfg, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ndp_app_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, ndp_channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, nss_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_transport_ip_param, wmi_ndp_transport_ip_param, ndp_transport_ip_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ndp_channel_info, ndp_channel_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_CONFIRM_EVENTID);

/** NDP end indication event
 *
 * TLV (tag length value ) parameters follow the ndp_end_indication
 * structure. The TLV's are:
 * wmi_ndp_end_indication ndp_end_indication_list[];
 */
#define WMITLV_TABLE_WMI_NDP_END_INDICATION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ndp_end_indication_PROTOTYPE, ndp_end_indication_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_END_INDICATION_EVENTID);

/** NDL schedule update event
 * TLV (tag lenght value ) parameters follow the ndl_schedule_update
 * structure. The TLV's are:
 * A_UINT32 ndp_instance_list[];
 * wmi_channel ndl_channel_list[];
 */
#define WMITLV_TABLE_WMI_NDL_SCHEDULE_UPDATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndl_schedule_update_fixed_param, wmi_ndl_schedule_update_fixed_param_PROTOTYPE, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ndp_instance_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, ndl_channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, nss_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ndp_channel_info, ndp_channel_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NDL_SCHEDULE_UPDATE_EVENTID);

/* NDP Event */
#define WMITLV_TABLE_WMI_NDP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ndp_event_param, wmi_ndp_event_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NDP_EVENTID);

/* Update RCPI Info Event */
#define WMITLV_TABLE_WMI_UPDATE_RCPI_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_update_rcpi_event_fixed_param, wmi_update_rcpi_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_RCPI_EVENTID);

/* L1SS track Event */
#define WMITLV_TABLE_WMI_PDEV_L1SS_TRACK_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_l1ss_track_event_fixed_param, wmi_pdev_l1ss_track_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_L1SS_TRACK_EVENTID);

#define WMITLV_TABLE_WMI_DIAG_DATA_CONTAINER_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_diag_data_container_event_fixed_param, wmi_diag_data_container_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_DATA_CONTAINER_EVENTID);

/* Estimated Link Speed Indication*/
#define WMITLV_TABLE_WMI_PEER_ESTIMATED_LINKSPEED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_estimated_linkspeed_event_fixed_param, wmi_peer_estimated_linkspeed_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ESTIMATED_LINKSPEED_EVENTID);

/* NAN Response/Indication Event */
#define WMITLV_TABLE_WMI_STATS_EXT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_stats_ext_event_fixed_param, wmi_stats_ext_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_STATS_EXT_EVENTID);

#define WMITLV_TABLE_WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offload_prb_rsp_tx_status_event_fixed_param, wmi_offload_prb_rsp_tx_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID);

/* host auto shut down event */
#define WMITLV_TABLE_WMI_HOST_AUTO_SHUTDOWN_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_auto_shutdown_event_fixed_param, wmi_host_auto_shutdown_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HOST_AUTO_SHUTDOWN_EVENTID);

/* peer state Event */
#define WMITLV_TABLE_WMI_PEER_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_state_event_fixed_param, wmi_peer_state_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_STATE_EVENTID);

/* peer delete response Event */
#define WMITLV_TABLE_WMI_PEER_DELETE_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_delete_resp_event_fixed_param, wmi_peer_delete_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_DELETE_RESP_EVENTID);

/* delete all peer response Event */
#define WMITLV_TABLE_WMI_VDEV_DELETE_ALL_PEER_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_delete_all_peer_resp_event_fixed_param, wmi_vdev_delete_all_peer_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DELETE_ALL_PEER_RESP_EVENTID);

/* peer assoc conf Event */
#define WMITLV_TABLE_WMI_PEER_ASSOC_CONF_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_assoc_conf_event_fixed_param, wmi_peer_assoc_conf_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ASSOC_CONF_EVENTID);

/* peer create conf Event */
#define WMITLV_TABLE_WMI_PEER_CREATE_CONF_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_create_conf_event_fixed_param, wmi_peer_create_conf_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CREATE_CONF_EVENTID);

/* D0-WOW Disable Ack event */
#define WMITLV_TABLE_WMI_D0_WOW_DISABLE_ACK_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_d0_wow_disable_ack_event_fixed_param, wmi_d0_wow_disable_ack_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_D0_WOW_DISABLE_ACK_EVENTID);

/* Pdev get chip temperature event */
#define WMITLV_TABLE_WMI_PDEV_TEMPERATURE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_temperature_event_fixed_param, wmi_pdev_temperature_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_TEMPERATURE_EVENTID);

/* Pdev get ANT DIV feature status event */
#define WMITLV_TABLE_WMI_PDEV_ANTDIV_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_antdiv_status_event_fixed_param, wmi_pdev_antdiv_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_ANTDIV_STATUS_EVENTID);

/* mDNS offload stats event */
#define WMITLV_TABLE_WMI_MDNS_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_stats_event_fixed_param, wmi_mdns_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_STATS_EVENTID);

/* pdev resume event */
#define WMITLV_TABLE_WMI_PDEV_RESUME_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_resume_event_fixed_param, wmi_pdev_resume_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_RESUME_EVENTID);

/* SAP Authentication offload event */
#define WMITLV_TABLE_WMI_SAP_OFL_ADD_STA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_ofl_add_sta_event_fixed_param, wmi_sap_ofl_add_sta_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OFL_ADD_STA_EVENTID);

#define WMITLV_TABLE_WMI_SAP_OFL_DEL_STA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_ofl_del_sta_event_fixed_param, wmi_sap_ofl_del_sta_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OFL_DEL_STA_EVENTID);

/* SAP OBSS detection offload event */
#define WMITLV_TABLE_WMI_SAP_OBSS_DETECTION_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_obss_detection_info_evt_fixed_param, wmi_sap_obss_detection_info_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OBSS_DETECTION_REPORT_EVENTID);

/* OBSS Color collision detection event */
#define WMITLV_TABLE_WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_obss_color_collision_evt_fixed_param, wmi_obss_color_collision_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID);

/* Set OCB schedule event, DEPRECATED */
#define WMITLV_TABLE_WMI_OCB_SET_SCHED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_ocb_set_sched_event_fixed_param, wmi_ocb_set_sched_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_SCHED_EVENTID);

/* Set OCB configuration response event */
#define WMITLV_TABLE_WMI_OCB_SET_CONFIG_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_set_config_resp_event_fixed_param, wmi_ocb_set_config_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_CONFIG_RESP_EVENTID);

/* Get TSF timer response event */
#define WMITLV_TABLE_WMI_OCB_GET_TSF_TIMER_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_resp_event_fixed_param, wmi_ocb_get_tsf_timer_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_GET_TSF_TIMER_RESP_EVENTID);

/* Get DCC stats response event */
#define WMITLV_TABLE_WMI_DCC_GET_STATS_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_get_stats_resp_event_fixed_param, wmi_dcc_get_stats_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_stats_per_channel, stats_per_channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_GET_STATS_RESP_EVENTID);

/* Update DCC NDL response event */
#define WMITLV_TABLE_WMI_DCC_UPDATE_NDL_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_update_ndl_resp_event_fixed_param, wmi_dcc_update_ndl_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_UPDATE_NDL_RESP_EVENTID);

/* DCC stats event */
#define WMITLV_TABLE_WMI_DCC_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_stats_event_fixed_param, wmi_dcc_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_stats_per_channel, stats_per_channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_STATS_EVENTID);

/* Read TSF timer response event */
#define WMITLV_TABLE_WMI_VDEV_TSF_REPORT_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_tsf_report_event_fixed_param, wmi_vdev_tsf_report_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_TSF_REPORT_EVENTID);

/* Vdev capabilities IE to be transmitted in mgmt frames */
#define WMITLV_TABLE_WMI_VDEV_SET_IE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_ie_cmd_fixed_param, wmi_vdev_set_ie_cmd_fixed_param, vdev_ie, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_IE_CMDID);

/* SOC Set Hardware Mode Response event - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_SET_HW_MODE_RESP_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_response_event_fixed_param, wmi_soc_set_hw_mode_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_soc_set_hw_mode_response_vdev_mac_entry, wmi_soc_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_HW_MODE_RESP_EVENTID);

/* PDEV Set Hardware Mode Response event */
#define WMITLV_TABLE_WMI_PDEV_SET_HW_MODE_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_hw_mode_response_event_fixed_param, wmi_pdev_set_hw_mode_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_set_hw_mode_response_vdev_mac_entry, wmi_pdev_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_HW_MODE_RESP_EVENTID);

/* SOC Hardware Mode Transition event - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_HW_MODE_TRANSITION_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_hw_mode_transition_event_fixed_param, wmi_soc_hw_mode_transition_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_soc_set_hw_mode_response_vdev_mac_entry, wmi_soc_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_HW_MODE_TRANSITION_EVENTID);

/* PDEV Hardware Mode Transition event */
#define WMITLV_TABLE_WMI_PDEV_HW_MODE_TRANSITION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_hw_mode_transition_event_fixed_param, wmi_pdev_hw_mode_transition_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_set_hw_mode_response_vdev_mac_entry, wmi_pdev_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_HW_MODE_TRANSITION_EVENTID);

/* SOC Set Dual MAC Config Response event - DEPRECATED */
#define WMITLV_TABLE_WMI_SOC_SET_DUAL_MAC_CONFIG_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_response_event_fixed_param, wmi_soc_set_dual_mac_config_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_DUAL_MAC_CONFIG_RESP_EVENTID);

/* PDEV Set Dual MAC Config Response event */
#define WMITLV_TABLE_WMI_PDEV_SET_MAC_CONFIG_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_mac_config_response_event_fixed_param, wmi_pdev_set_mac_config_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_MAC_CONFIG_RESP_EVENTID);

/* Packet Filter configure command*/
#define WMITLV_TABLE_WMI_PACKET_FILTER_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_packet_filter_config_fixed_param, WMI_PACKET_FILTER_CONFIG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PACKET_FILTER_CONFIG_CMDID);

/* Packet Filter enable command*/
#define WMITLV_TABLE_WMI_PACKET_FILTER_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_packet_filter_enable_fixed_param, WMI_PACKET_FILTER_ENABLE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PACKET_FILTER_ENABLE_CMDID);

/* MAWC enable/disable sensor event */
#define WMITLV_TABLE_WMI_MAWC_ENABLE_SENSOR_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mawc_enable_sensor_event_fixed_param, wmi_mawc_enable_sensor_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MAWC_ENABLE_SENSOR_EVENTID);

/* SMPS force mode complete Event */
#define WMITLV_TABLE_WMI_STA_SMPS_FORCE_MODE_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_complete_event_fixed_param, wmi_sta_smps_force_mode_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_STA_SMPS_FORCE_MODE_COMPLETE_EVENTID);

/* bpf offload capability info event */
#define WMITLV_TABLE_WMI_BPF_CAPABILIY_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_capability_info_evt_fixed_param,  wmi_bpf_capability_info_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_CAPABILIY_INFO_EVENTID);

/* bpf offload vdev status info event */
#define WMITLV_TABLE_WMI_BPF_VDEV_STATS_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_vdev_stats_info_evt_fixed_param, wmi_bpf_vdev_stats_info_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_VDEV_STATS_INFO_EVENTID);

/* bpf offload vdev get work memory event */
#define WMITLV_TABLE_WMI_BPF_GET_VDEV_WORK_MEMORY_RESP_EVENTID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_get_vdev_work_memory_resp_evt_fixed_param, wmi_bpf_get_vdev_work_memory_resp_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_GET_VDEV_WORK_MEMORY_RESP_EVENTID);

/* Indicate new country code to host from 11d scan */
#define WMITLV_TABLE_WMI_11D_NEW_COUNTRY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_11d_new_country_event_fixed_param, wmi_11d_new_country_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_11D_NEW_COUNTRY_EVENTID);

/* Regulatory channel list of current country code */
#define WMITLV_TABLE_WMI_REG_CHAN_LIST_CC_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_reg_chan_list_cc_event_fixed_param, wmi_reg_chan_list_cc_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_regulatory_rule_struct, reg_rule_array, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REG_CHAN_LIST_CC_EVENTID);

/* Ext regulatory channel list of current country code */
#define WMITLV_TABLE_WMI_REG_CHAN_LIST_CC_EXT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_reg_chan_list_cc_event_ext_fixed_param, wmi_reg_chan_list_cc_event_ext_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_regulatory_rule_ext_struct, reg_rule_array, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REG_CHAN_LIST_CC_EXT_EVENTID);

/* FIPS event */
#define WMITLV_TABLE_WMI_PDEV_FIPS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_fips_event_fixed_param, wmi_pdev_fips_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FIPS_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_CHANNEL_HOPPING_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_channel_hopping_event_fixed_param, wmi_pdev_channel_hopping_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CHANNEL_HOPPING_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_ANI_CCK_LEVEL_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ani_cck_event_fixed_param, wmi_ani_cck_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_ANI_CCK_LEVEL_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chip_power_save_failure_detected_fixed_param, wmi_chip_power_save_failure_detected_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_CHIP_POWER_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_chip_power_stats_event_fixed_param, wmi_pdev_chip_power_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, debug_registers, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CHIP_POWER_STATS_EVENTID);

#define WMITLV_TABLE_WMI_VDEV_BCN_RECEPTION_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_bcn_recv_stats_event_fixed_param, wmi_vdev_bcn_recv_stats_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_BCN_RECEPTION_STATS_EVENTID);

/* WLAN GET mws coex state */
#define WMITLV_TABLE_WMI_VDEV_GET_MWS_COEX_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_state_fixed_param, wmi_vdev_get_mws_coex_state_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_MWS_COEX_STATE_EVENTID);

/* WLAN GET Dynamic power back-off state */
#define WMITLV_TABLE_WMI_VDEV_GET_MWS_COEX_DPWB_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_dpwb_state_fixed_param, wmi_vdev_get_mws_coex_dpwb_state_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_MWS_COEX_DPWB_STATE_EVENTID);

/* WLAN GET TDM state */
#define WMITLV_TABLE_WMI_VDEV_GET_MWS_COEX_TDM_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_tdm_state_fixed_param, wmi_vdev_get_mws_coex_tdm_state_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_MWS_COEX_TDM_STATE_EVENTID);

/* WLAN GET Idrx state */
#define WMITLV_TABLE_WMI_VDEV_GET_MWS_COEX_IDRX_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_idrx_state_fixed_param, wmi_vdev_get_mws_coex_idrx_state_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_MWS_COEX_IDRX_STATE_EVENTID);

/* WLAN GET antenna sharing state */
#define WMITLV_TABLE_WMI_VDEV_GET_MWS_COEX_ANTENNA_SHARING_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_mws_coex_antenna_sharing_state_fixed_param, wmi_vdev_get_mws_coex_antenna_sharing_state_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_MWS_COEX_ANTENNA_SHARING_STATE_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_ANI_OFDM_LEVEL_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ani_ofdm_event_fixed_param, wmi_ani_ofdm_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_ANI_OFDM_LEVEL_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_TPC_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_tpc_event_fixed_param, wmi_pdev_tpc_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tpc, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_TPC_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_nfcal_power_all_channels_event_fixed_param, wmi_pdev_nfcal_power_all_channels_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_nfcal_power_all_channels_nfdBr, nfdbr, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_nfcal_power_all_channels_nfdBm, nfdbm, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_nfcal_power_all_channels_freqNum, freqnum, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID);

#define WMITLV_TABLE_WMI_PEER_RATECODE_LIST_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_ratecode_list_event_fixed_param, wmi_peer_ratecode_list_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_cck_ofdm_rate_info, ratecode_legacy, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_mcs_rate_info, ratecode_mcs, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_RATECODE_LIST_EVENTID);

#define WMITLV_TABLE_WMI_WDS_PEER_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wds_addr_event_fixed_param, wmi_wds_addr_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WDS_PEER_EVENTID);

#define WMITLV_TABLE_WMI_PEER_STA_PS_STATECHG_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_sta_ps_statechange_event_fixed_param, wmi_peer_sta_ps_statechange_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_STA_PS_STATECHG_EVENTID);

#define WMITLV_TABLE_WMI_INST_RSSI_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_inst_rssi_stats_resp_fixed_param, wmi_inst_rssi_stats_resp_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_INST_RSSI_STATS_EVENTID);

#define WMITLV_TABLE_WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_power_level_stats_evt_fixed_param, wmi_tx_power_level_stats_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tx_time_per_power_level, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID);

#define WMITLV_TABLE_WMI_RMC_NEW_LEADER_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_rmc_manual_leader_event_fixed_param, wmi_rmc_manual_leader_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RMC_NEW_LEADER_EVENTID);

#define WMITLV_TABLE_WMI_VDEV_AUDIO_SYNC_START_STOP_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_audio_sync_start_stop_event_fixed_param, wmi_audio_sync_start_stop_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_AUDIO_SYNC_START_STOP_EVENTID);

#define WMITLV_TABLE_WMI_VDEV_AUDIO_SYNC_Q_MASTER_SLAVE_OFFSET_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_audio_sync_q_master_slave_offset_event_fixed_param, wmi_audio_sync_q_master_slave_offset_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_audio_sync_q_master_slave_times, audio_sync_q_master_slave_times, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_AUDIO_SYNC_Q_MASTER_SLAVE_OFFSET_EVENTID);

/* Layout of WMI_REPORT_STATS_EVENTID message:
 *    fixed_param;
 *    wmi_chan_cca_stats       chan_cca_stats[];         Array length is specified by num_chan_cca_stats
 *    wmi_peer_signal_stats    peer_signal_stats[];      Array length is specified by num_peer_signal_stats
 *    wmi_peer_ac_tx_stats     peer_ac_tx_stats[];       Array length is specified by num_peer_ac_tx_stats
 *    wmi_tx_stats             tx_stats[][];             Array length is num_peer_ac_tx_stats * WLAN_MAX_AC, array index is (peer_index * WLAN_MAX_AC + ac_index)
 *    A_UINT32                 tx_mpdu_aggr[][][];       Array length is num_peer_ac_tx_stats * WLAN_MAX_AC * tx_mpdu_aggr_array_len,
 *                                                       array index is (peer_index * WLAN_MAX_AC + ac_index) * tx_mpdu_aggr_array_len + A-MPDU size index
 *                                                       Contains a histogram of how many A-MPDUs of a given size (i.e. number of MPDUs) have been transmitted.
 *                                                       Element 0 contains the number of PPDUs with a single-MPDU A-MPDU.
 *                                                       Element 1 contains the number of PPDUs with 2 MPDUs.
 *                                                       Element 2 contains the number of PPDUs with 3 MPDUs.
 *                                                       Element tx_mpdu_aggr_array_len-1 contains the number of PPDUs with >= tx_mpdu_aggr_array_len MPDUs.
 *    A_UINT32                 tx_succ_mcs[][][];        Array length is num_peer_ac_tx_stats * WLAN_MAX_AC * tx_succ_mcs_array_len,
 *                                                       array index is (peer_index * WLAN_MAX_AC + ac_index) * tx_succ_mcs_array_len + MCS index
 *                                                       Contains a count of how many tx PPDUs have been acked for each MCS of each AC of each peer.
 *    A_UINT32                 tx_fail_mcs[][][];        Array length is num_peer_ac_tx_stats * WLAN_MAX_AC * tx_fail_mcs_array_len,
 *                                                       array index is (peer_index * WLAN_MAX_AC + ac_index) * tx_fail_mcs_array_len + MCS index
 *                                                       Contains a count of how many PPDUs failed tx due to no ack for each MCS of each AC of each peer.
 *    A_UINT32                 tx_ppdu_delay[][][];      Array length is num_peer_ac_tx_stats * WLAN_MAX_AC * tx_ppdu_delay_array_len,
 *                                                       array index is (peer_index * WLAN_MAX_AC + ac_index) * tx_ppdu_delay_array_len + delay index
 *                                                       Contains a histogram of how many PPDUs encountered each level of delay due to retries or air interface contention.
 *                                                       The time represented by each array element (i.e. histogram bin) is specified by tx_ppdu_delay_bin_size_ms.
 *                                                       Element 0 contains the count of PPDUs delayed by less than tx_ppdu_delay_bin_size_ms.
 *                                                       Element 1 contains the count of PPDUs delayed by more than 1x tx_ppdu_delay_bin_size_ms but less than 2x.
 *                                                       Element tx_ppdu_delay_array_len-1 contains the count of PPDUs delayed by
 *                                                           >= tx_ppdu_delay_bin_size_ms * (tx_ppdu_delay_array_len-1)
 *    wmi_peer_ac_rx_stats     peer_ac_rx_stats[];       Array length is specified by num_peer_ac_rx_stats
 *    wmi_rx_stats             rx_stats[][];             Array length is num_peer_ac_rx_stats * WLAN_MAX_AC, array index is (peer_index * WLAN_MAX_AC + ac_index)
 *    A_UINT32                 rx_mpdu_aggr[][][];       Array length is num_peer_ac_rx_stats * WLAN_MAX_AC * rx_mpdu_aggr_array_len,
 *                                                       array index is (peer_index * WLAN_MAX_AC + ac_index) * rx_mpdu_aggr_array_len + A-MPDU size index
 *                                                       Contains a histogram of how many A-MPDUs of a given size (i.e. number of MPDUs) have been received.
 *                                                       Element 0 contains the number of PPDUs with a single MPDU.
 *                                                       Element 1 contains the number of PPDUs with 2 MPDUs.
 *                                                       Element 2 contains the number of PPDUs with 3 MPDUs.
 *                                                       Element rx_mpdu_aggr_array_len-1 contains the number of PPDUs with >= rx_mpdu_aggr_array_len MPDUs.
 *    A_UINT32                 rx_mcs[][][];             Array length is (num_peer_ac_rx_stats * WLAN_MAX_AC) * rx_mcs_array_len,
 *                                                       array index is (peer_index * WLAN_MAX_AC + ac_index) * rx_mcs_array_len + MCS index
 *                                                       Contains a count of rx PPDUs for each MCS of each AC of each peer.
 *    wmi_stats_period         stats_period[];           Array length is specified by stats_period_array_len
 *    wmi_stats_interference   stats_interference[];     Array length is determied by dividing array level TLV header's length value by array-element TLV header's length value.
 *
 * For example, if there were 2 peers (X and Y) whose stats were being reported,
 * the message and its TLV arrays would look like this:
 * 1.  fixed_param
 * 2.  chan_cca_stats[0]
 * 3.  peer_signal_stats[0] for peer X
 * 4.  peer_signal_stats[1] for peer Y
 * 5.  peer_ac_tx_stats[0] for X
 * 6.  peer_ac_tx_stats[1] for Y
 * 7.  tx_stats[0][0] for peer X, AC 0
 * 8.  tx_stats[0][1] for peer X, AC 1
 * 9.  tx_stats[0][2] for peer X, AC 2
 * 10. tx_stats[0][3] for peer X, AC 3
 * 11. tx_stats[1][0] for peer Y, AC 0
 * 12  tx_stats[1][1] for peer Y, AC 1
 * 13. tx_stats[1][2] for peer Y, AC 2
 * 14. tx_stats[1][3] for peer Y, AC 3
 * 15. tx_mpdu_aggr[0][0][] for peer X, AC 0
 * 16. tx_mpdu_aggr[0][1][] for peer X, AC 1
 * 17. tx_mpdu_aggr[0][2][] for peer X, AC 2
 * 18. tx_mpdu_aggr[0][3][] for peer X, AC 3
 * 19. tx_mpdu_aggr[1][0][] for peer Y, AC 0
 * 20. tx_mpdu_aggr[1][1][] for peer Y, AC 1
 * 21. tx_mpdu_aggr[1][2][] for peer Y, AC 2
 * 22. tx_mpdu_aggr[1][3][] for peer Y, AC 3
 * 23. tx_succ_mcs[0][0][] for peer X, AC 0
 * 24. tx_succ_mcs[0][1][] for peer X, AC 1
 * 25. tx_succ_mcs[0][2][] for peer X, AC 2
 * 26. tx_succ_mcs[0][3][] for peer X, AC 3
 * 27. tx_succ_mcs[1][0][] for peer Y, AC 0
 * 28. tx_succ_mcs[1][1][] for peer Y, AC 1
 * 29. tx_succ_mcs[1][2][] for peer Y, AC 2
 * 30. tx_succ_mcs[1][3][] for peer Y, AC 3
 * 31. tx_fail_mcs[0][0][] for peer X, AC 0
 * 32. tx_fail_mcs[0][1][] for peer X, AC 1
 * 33. tx_fail_mcs[0][2][] for peer X, AC 2
 * 34. tx_fail_mcs[0][3][] for peer X, AC 3
 * 35. tx_fail_mcs[1][0][] for peer Y, AC 0
 * 36. tx_fail_mcs[1][1][] for peer Y, AC 1
 * 37. tx_fail_mcs[1][2][] for peer Y, AC 2
 * 38. tx_fail_mcs[1][3][] for peer Y, AC 3
 * 39. tx_ppdu_delay[0][0][] for peer X, AC 0
 * 40. tx_ppdu_delay[0][1][] for peer X, AC 1
 * 41. tx_ppdu_delay[0][2][] for peer X, AC 2
 * 42. tx_ppdu_delay[0][3][] for peer X, AC 3
 * 43. tx_ppdu_delay[1][0][] for peer Y, AC 0
 * 44. tx_ppdu_delay[1][1][] for peer Y, AC 1
 * 45. tx_ppdu_delay[1][2][] for peer Y, AC 2
 * 46. tx_ppdu_delay[1][3][] for peer Y, AC 3
 * 47. peer_ac_rx_stats[0] for X
 * 48. peer_ac_rx_stats[1] for Y
 * 49. rx_stats[0][0] for peer X, AC 0
 * 50. rx_stats[0][1] for peer X, AC 1
 * 51. rx_stats[0][2] for peer X, AC 2
 * 52. rx_stats[0][3] for peer X, AC 3
 * 53. rx_stats[1][0] for peer Y, AC 0
 * 54. rx_stats[1][1] for peer Y, AC 1
 * 55. rx_stats[1][2] for peer Y, AC 2
 * 56. rx_stats[1][3] for peer Y, AC 3
 * 57. rx_mpdu_aggr[0][0][] for peer X, AC 0
 * 58. rx_mpdu_aggr[0][1][] for peer X, AC 1
 * 59. rx_mpdu_aggr[0][2][] for peer X, AC 2
 * 60. rx_mpdu_aggr[0][3][] for peer X, AC 3
 * 61. rx_mpdu_aggr[1][0][] for peer Y, AC 0
 * 62. rx_mpdu_aggr[1][1][] for peer Y, AC 1
 * 63. rx_mpdu_aggr[1][2][] for peer Y, AC 2
 * 64. rx_mpdu_aggr[1][3][] for peer Y, AC 3
 * 65. rx_mcs[0][0][] for peer X, AC 0
 * 66. rx_mcs[0][1][] for peer X, AC 1
 * 67. rx_mcs[0][2][] for peer X, AC 2
 * 68. rx_mcs[0][3][] for peer X, AC 3
 * 69. rx_mcs[1][0][] for peer Y, AC 0
 * 70. rx_mcs[1][1][] for peer Y, AC 1
 * 71. rx_mcs[1][2][] for peer Y, AC 2
 * 72. rx_mcs[1][3][] for peer Y, AC 3
 **/
#define WMITLV_TABLE_WMI_REPORT_STATS_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_report_stats_event_fixed_param, wmi_report_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_chan_cca_stats, chan_cca_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_signal_stats, peer_signal_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_ac_tx_stats, peer_ac_tx_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tx_stats, tx_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tx_mpdu_aggr, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tx_succ_mcs, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tx_fail_mcs, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tx_ppdu_delay, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_ac_rx_stats, peer_ac_rx_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_rx_stats, rx_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, rx_mpdu_aggr, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, rx_mcs, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_stats_period, stats_period, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_stats_interference, stats_interference, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REPORT_STATS_EVENTID);

#define WMITLV_TABLE_WMI_VDEV_ENCRYPT_DECRYPT_DATA_RESP_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_vdev_encrypt_decrypt_data_resp_event_fixed_param, wmi_vdev_encrypt_decrypt_data_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, enc80211_frame, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ENCRYPT_DECRYPT_DATA_RESP_EVENTID);

#define WMITLV_TABLE_WMI_PEER_STATS_INFO_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_stats_info_event_fixed_param, wmi_peer_stats_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_stats_info, peer_stats_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_STATS_INFO_EVENTID);

/* Update Control Path stats event */
#define WMITLV_TABLE_WMI_CTRL_PATH_STATS_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ctrl_path_stats_event_fixed_param, wmi_ctrl_path_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ctrl_path_pdev_stats_struct, ctrl_path_pdev_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_extd_stats, vdev_extd_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ctrl_path_mem_stats_struct, ctrl_path_mem_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_CTRL_PATH_STATS_EVENTID);

#define WMITLV_TABLE_WMI_RADIO_CHAN_STATS_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_radio_chan_stats_event_fixed_param, wmi_radio_chan_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_radio_chan_stats, radio_chan_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RADIO_CHAN_STATS_EVENTID);

#define WMITLV_TABLE_WMI_PKGID_EVENTID(id, op, buf, len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_pkgid_event_fixed_param, wmi_pkgid_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PKGID_EVENTID);

/* mac randomization event */
#define WMITLV_TABLE_WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_add_mac_addr_to_rx_filter_status_event_fixed_param, wmi_vdev_add_mac_addr_to_rx_filter_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_STATUS_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_csa_switch_count_status_event_fixed_param, wmi_pdev_csa_switch_count_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, vdev_ids, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_CSC_SWITCH_COUNT_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_csc_switch_count_status_event_fixed_param, wmi_pdev_csc_switch_count_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_csc_vdev_list, vdev_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CSC_SWITCH_COUNT_STATUS_EVENTID);

/* cal version response event */
#define WMITLV_TABLE_WMI_PDEV_CHECK_CAL_VERSION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_check_cal_version_event_fixed_param, wmi_pdev_check_cal_version_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CHECK_CAL_VERSION_EVENTID);

/* ARP stats response event */
#define WMITLV_TABLE_WMI_VDEV_GET_ARP_STAT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_arp_stats_event_fixed_param, wmi_vdev_get_arp_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_get_connectivity_check_stats, connectivity_check_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_ARP_STAT_EVENTID);

/* Coex BT activity response event */
#define WMITLV_TABLE_WMI_WLAN_COEX_BT_ACTIVITY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_coex_bt_activity_event_fixed_param, wmi_coex_bt_activity_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_COEX_BT_ACTIVITY_EVENTID);

/* Thermal Throttling stats event */
#define WMITLV_TABLE_WMI_THERM_THROT_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_therm_throt_stats_event_fixed_param, wmi_therm_throt_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_therm_throt_level_stats_info, therm_throt_level_stats_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_THERM_THROT_STATS_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_DMA_RING_CFG_RSP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dma_ring_cfg_rsp_fixed_param, wmi_dma_ring_cfg_rsp_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DMA_RING_CFG_RSP_EVENTID);

/* dma buffer release event */
#define WMITLV_TABLE_WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dma_buf_release_fixed_param, wmi_dma_buf_release_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dma_buf_release_entry, entries, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dma_buf_release_spectral_meta_data, meta_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID);

/* ctl failsafe check event */
#define WMITLV_TABLE_WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_ctl_failsafe_check_fixed_param, wmi_pdev_ctl_failsafe_check_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID);

/* UNIT-TEST Event */
#define WMITLV_TABLE_WMI_UNIT_TEST_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_unit_test_event_fixed_param, wmi_unit_test_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UNIT_TEST_EVENTID);


/* enabling TWT complete Event */
#define WMITLV_TABLE_WMI_TWT_ENABLE_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_enable_complete_event_fixed_param, wmi_twt_enable_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_ENABLE_COMPLETE_EVENTID);

/* disabling TWT complete Event */
#define WMITLV_TABLE_WMI_TWT_DISABLE_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_disable_complete_event_fixed_param, wmi_twt_disable_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_DISABLE_COMPLETE_EVENTID);

/* adding TWT dialog complete Event */
#define WMITLV_TABLE_WMI_TWT_ADD_DIALOG_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_add_dialog_complete_event_fixed_param, wmi_twt_add_dialog_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_twt_add_dialog_additional_params, twt_params, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_ADD_DIALOG_COMPLETE_EVENTID);

/* deleting TWT dialog complete Event */
#define WMITLV_TABLE_WMI_TWT_DEL_DIALOG_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_del_dialog_complete_event_fixed_param, wmi_twt_del_dialog_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_DEL_DIALOG_COMPLETE_EVENTID);

/* pausing TWT dialog complete Event */
#define WMITLV_TABLE_WMI_TWT_PAUSE_DIALOG_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_pause_dialog_complete_event_fixed_param, wmi_twt_pause_dialog_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_PAUSE_DIALOG_COMPLETE_EVENTID);

/* resuming TWT dialog complete Event */
#define WMITLV_TABLE_WMI_TWT_RESUME_DIALOG_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_resume_dialog_complete_event_fixed_param, wmi_twt_resume_dialog_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_RESUME_DIALOG_COMPLETE_EVENTID);

/* inviting STA to B-TWT dialog complete Event */
#define WMITLV_TABLE_WMI_TWT_BTWT_INVITE_STA_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_btwt_invite_sta_complete_event_fixed_param, wmi_twt_btwt_invite_sta_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_BTWT_INVITE_STA_COMPLETE_EVENTID);

/* removing STA from B-TWT dialog complete Event */
#define WMITLV_TABLE_WMI_TWT_BTWT_REMOVE_STA_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_twt_btwt_remove_sta_complete_event_fixed_param, wmi_twt_btwt_remove_sta_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_BTWT_REMOVE_STA_COMPLETE_EVENTID);

/* Event to send roam scan stats */
#define WMITLV_TABLE_WMI_ROAM_SCAN_STATS_EVENTID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_stats_event_fixed_param, wmi_roam_scan_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, client_id, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_roaming_timestamp, timestamp, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, num_channels, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, chan_info, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, old_bssid, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, is_roaming_success, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, new_bssid, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, num_roam_candidates, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_roam_scan_trigger_reason, roam_reason, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, score, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel, WMITLV_SIZE_VAR) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, rssi, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_STATS_EVENTID);

/* Motion detection events */
#define WMITLV_TABLE_WMI_MOTION_DET_HOST_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_motion_det_event, wmi_motion_det_event, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MOTION_DET_HOST_EVENTID);

#define WMITLV_TABLE_WMI_MOTION_DET_BASE_LINE_HOST_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_motion_det_base_line_event, wmi_motion_det_base_line_event, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MOTION_DET_BASE_LINE_HOST_EVENTID);

#define WMITLV_TABLE_WMI_ESP_ESTIMATE_EVENTID(id,op,buf,len) \
  WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_esp_estimate_event_fixed_param, wmi_esp_estimate_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ESP_ESTIMATE_EVENTID);

/* Peer CFR capture event */
#define WMITLV_TABLE_WMI_PEER_CFR_CAPTURE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_fixed_param, wmi_peer_cfr_capture_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_cfr_capture_event_phase_fixed_param, wmi_peer_cfr_capture_event_phase_fixed_param, phase_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CFR_CAPTURE_EVENTID);

#define WMITLV_TABLE_WMI_PDEV_COLD_BOOT_CAL_DATA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_cold_boot_cal_data_fixed_param, wmi_cold_boot_cal_data_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_COLD_BOOT_CAL_DATA_EVENTID);

/* rogue ap info event */
#define WMITLV_TABLE_WMI_PDEV_RAP_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_rap_info_event_fixed_param, wmi_pdev_rap_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_RAP_INFO_EVENTID);

/* Offload TX mgmt frames */
#define WMITLV_TABLE_WMI_VDEV_MGMT_OFFLOAD_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_hdr, wmi_mgmt_hdr, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_MGMT_OFFLOAD_EVENTID);

/* get ELNA BYPASS status event */
#define WMITLV_TABLE_WMI_GET_ELNA_BYPASS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_elna_bypass_event_fixed_param, wmi_get_elna_bypass_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GET_ELNA_BYPASS_EVENTID);

/* channel ANI event */
#define WMITLV_TABLE_WMI_GET_CHANNEL_ANI_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_channel_ani_event_fixed_param, wmi_get_channel_ani_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel_ani_info_tlv_param, ani_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_GET_CHANNEL_ANI_EVENTID);

/* Multiple Vdev Restart response event */
#define WMITLV_TABLE_WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_multiple_vdev_restart_resp_event_fixed_param, wmi_pdev_multiple_vdev_restart_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, vdev_ids_bitmap, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID);

/* Roam scan channel list event */
#define WMITLV_TABLE_WMI_ROAM_SCAN_CHANNEL_LIST_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_channel_list_event_fixed_param, wmi_roam_scan_channel_list_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_CHANNEL_LIST_EVENTID);

/* VDEV_SEND_BIG_DATA_EVENT IS DEPRECATED - DO NOT USE */
#define WMITLV_TABLE_WMI_VDEV_SEND_BIG_DATA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_send_big_data_event_fixed_param, wmi_vdev_send_big_data_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SEND_BIG_DATA_EVENTID);

/* send BIG DATA event to host P2 */
#define WMITLV_TABLE_WMI_VDEV_SEND_BIG_DATA_P2_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_send_big_data_p2_event_fixed_param, wmi_vdev_send_big_data_p2_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, bd_datapath_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SEND_BIG_DATA_P2_EVENTID);

#define WMITLV_TABLE_WMI_NAN_DMESG_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_dmesg_event_fixed_param, wmi_nan_dmesg_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, msg, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_DMESG_EVENTID);

/* report soundbar statistics event to host */
#define WMITLV_TABLE_WMI_AUDIO_AGGR_REPORT_STATISTICS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_statistics_event_fixed_param, wmi_audio_aggr_statistics_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_audio_aggr_group_stats, group_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_audio_aggr_peer_stats, peer_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_REPORT_STATISTICS_EVENTID);

#define WMITLV_TABLE_WMI_AUDIO_AGGR_SCHED_METHOD_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_audio_aggr_sched_method_event_fixed_param, wmi_audio_aggr_sched_method_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AUDIO_AGGR_SCHED_METHOD_EVENTID);

/* Send sscan fw params to host */
#define WMITLV_TABLE_WMI_PDEV_SSCAN_FW_PARAM_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_sscan_fw_cmd_fixed_param, wmi_pdev_sscan_fw_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_pdev_sscan_fft_bin_index, fft_bin_index, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SSCAN_FW_PARAM_EVENTID);

/* Send sscan related event start/stop trigger to host */
#define WMITLV_TABLE_WMI_SSCAN_EVT_MESSAGE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sscan_evt_message_fixed_param, wmi_sscan_evt_message_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SSCAN_EVT_MESSAGE_EVENTID);

/* Roam capability report event */
#define WMITLV_TABLE_WMI_ROAM_CAPABILITY_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_capability_report_event_fixed_param, wmi_roam_capability_report_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_CAPABILITY_REPORT_EVENTID);

/* PDEV TPC STATS Event */
#define WMITLV_TABLE_WMI_PDEV_GET_TPC_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_tpc_stats_event_fixed_param, wmi_pdev_get_tpc_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tpc_configs, tpc_configs, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_max_reg_power_allowed, regulatory_power, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_INT16, A_INT16, reg_buf, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tpc_rates_array, tpc_rates, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_INT16, A_UINT16, rates_buf, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tpc_ctl_pwr_table, ctl_power, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_INT8, ctl_buf, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TPC_STATS_EVENTID);

/* Send Bcn Latency ie related params to host */
#define WMITLV_TABLE_WMI_VDEV_BCN_LATENCY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_bcn_latency_fixed_param, wmi_vdev_bcn_latency_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_BCN_LATENCY_EVENTID);

/* TWT Stats session event */
#define WMITLV_TABLE_WMI_TWT_SESSION_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_twt_session_stats_event_fixed_param, wmi_pdev_twt_session_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_twt_session_stats_info, twt_sessions, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TWT_SESSION_STATS_EVENTID);


#ifdef __cplusplus
}
#endif

#endif /*_WMI_TLV_DEFS_H_*/


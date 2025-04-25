/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/*
 * THIS IS AUTO GENERATED FILE.
 * DO NOT ALTER MANUALLY
 */

#ifndef __WLAN_CP_STATS_CHIPSET_STATS_EVENTS_H
#define __WLAN_CP_STATS_CHIPSET_STATS_EVENTS_H

#include <qdf_types.h>
#include <wmi_unified_param.h>

#define CSTATS_MAC_LEN 4

#define CHIPSET_STATS_HDR_VERSION  0x0000001

enum qca_chipset_stats_event_type {
	WLAN_CHIPSET_STATS_MGMT_AUTH_EVENT_ID = 0,
	WLAN_CHIPSET_STATS_MGMT_ASSOC_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_MGMT_ASSOC_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_MGMT_REASSOC_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_MGMT_REASSOC_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_MGMT_DEAUTH_EVENT_ID,
	WLAN_CHIPSET_STATS_MGMT_DISASSOC_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_CONNECTING_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_CONNECT_SUCCESS_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_CONNECT_FAIL_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_DISCONNECT_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_DISCONNECT_DONE_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_ROAM_SCAN_START_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_ROAM_SCAN_DONE_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_ROAM_RESULT_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_TDLS_DISCOVERY_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_TDLS_DISCOVERY_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_TDLS_SETUP_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_TDLS_SETUP_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_TDLS_SETUP_CONFIRM_EVENT_ID,
	WLAN_CHIPSET_STATS_STA_TDLS_TEARDOWN_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_START_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_STOP_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_CAC_START_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_CAC_END_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_CAC_INTERRUPTED_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_RADAR_DETECTED_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_STA_DISASSOC_EVENT_ID,
	WLAN_CHIPSET_STATS_SAP_GO_STA_ASSOC_REASSOC_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_DISCOVERY_ENABLE_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_DISCOVERY_ENABLE_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_DISCOVERY_DISABLE_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_DISCOVERY_DISABLE_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDI_CREATE_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDI_CREATE_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDI_DELETE_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDI_DELETE_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_INITIATOR_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_INITIATOR_RSP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_RESPONDER_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_RESPONDER_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_END_REQ_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_END_RESP_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_CONFIRM_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_INDICATION_EVENT_ID,
	WLAN_CHIPSET_STATS_NAN_NDP_NEW_PEER_EVENT_ID,
	WLAN_CHIPSET_STATS_DATA_PKT_EVENT_ID,
	WLAN_CHIPSET_STATS_MAX_EVENT_ID,
};

enum cstats_dir {
	CSTATS_DIR_TX = 0,
	CSTATS_DIR_RX,
	CSTATS_DIR_INVAL,
};

enum cstats_flag {
	CSTATS_FLAG_HT = 1,
	CSTATS_FLAG_VHT,
	CSTATS_FLAG_HE,
	CSTATS_FLAG_EHT,
};

enum cstats_pkt_type {
	CSTATS_PKT_TYPE_INVALID = 0,
	CSTATS_EAPOL_M1,
	CSTATS_EAPOL_M2,
	CSTATS_EAPOL_M3,
	CSTATS_EAPOL_M4,
	CSTATS_DHCP_DISCOVER,
	CSTATS_DHCP_OFFER,
	CSTATS_DHCP_REQ,
	CSTATS_DHCP_ACK,
	CSTATS_DHCP_NACK,
	CSTATS_DHCP_RELEASE,
	CSTATS_DHCP_INFORM,
	CSTATS_DHCP_DECLINE,
};

enum cstats_pkt_status {
	CSTATS_STATUS_INVALID = 0,
	CSTATS_TX_STATUS_OK,
	CSTATS_TX_STATUS_FW_DISCARD,
	CSTATS_TX_STATUS_NO_ACK,
	CSTATS_TX_STATUS_DROP,
	CSTATS_TX_STATUS_DOWNLOAD_SUCC,
	CSTATS_TX_STATUS_DEFAULT,
};

struct cstats_hdr {
	uint16_t evt_id;
	uint16_t length;
} qdf_packed;

struct cstats_cmn {
	struct cstats_hdr hdr;
	uint8_t opmode;
	uint8_t vdev_id;
	uint64_t timestamp_us;
	uint64_t time_tick;
} qdf_packed;

struct cstats_sta_roam_scan_cancel {
	uint32_t reason_code;
	uint8_t data_rssi;
	uint8_t data_rssi_threshold;
	uint8_t rx_linkspeed_status;
} qdf_packed;

struct cstats_sta_roam_scan_ap {
	uint8_t bssid[CSTATS_MAC_LEN];
	uint32_t total_score;
	uint32_t rssi;
	uint32_t etp;
	uint16_t freq;
	uint16_t cu_load;
	uint8_t is_mlo;
	uint8_t type;
} qdf_packed;

struct cstats_auth_mgmt_frm {
	struct cstats_cmn cmn;
	uint8_t auth_algo;
	uint8_t auth_seq_num;
	uint8_t status;
	uint8_t direction;
} qdf_packed;

struct cstats_assoc_req_mgmt_frm {
	struct cstats_cmn cmn;
	uint8_t ssid_len;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1];
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t sa[CSTATS_MAC_LEN];
	uint16_t freq;
	uint8_t flags;
	uint8_t direction;
} qdf_packed;

struct cstats_assoc_resp_mgmt_frm {
	struct cstats_cmn cmn;
	uint8_t dest_mac[CSTATS_MAC_LEN];
	uint8_t bssid[CSTATS_MAC_LEN];
	uint16_t status_code;
	uint16_t aid;
	uint8_t direction;
	uint8_t flags;
} qdf_packed;

struct cstats_deauth_mgmt_frm {
	struct cstats_cmn cmn;
	uint16_t reason;
	uint8_t direction;
} qdf_packed;

struct cstats_disassoc_mgmt_frm {
	struct cstats_cmn cmn;
	uint16_t reason;
	uint8_t direction;
} qdf_packed;

struct cstats_sta_connect_req {
	struct cstats_cmn cmn;
	uint8_t ssid_len;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1];
	uint8_t bssid[CSTATS_MAC_LEN];
	uint16_t freq;
} qdf_packed;

struct cstats_sta_connect_resp {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t ssid_len;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1];
	uint16_t result_code;
	uint16_t freq;
	uint8_t chnl_bw;
	uint8_t dot11mode;
	uint8_t qos_capability;
	uint8_t encryption_type;
	uint8_t auth_type;
} qdf_packed;

struct cstats_sta_disconnect_req {
	struct cstats_cmn cmn;
	uint16_t reason_code;
	uint8_t is_no_disassoc_disconnect;
	uint8_t source;
	uint8_t bssid[CSTATS_MAC_LEN];
} qdf_packed;

struct cstats_sta_disconnect_resp {
	struct cstats_cmn cmn;
	uint32_t cm_id;
	uint16_t reason_code;
	uint8_t source;
	uint8_t bssid[CSTATS_MAC_LEN];
} qdf_packed;

struct cstats_sta_roam_scan_start {
	struct cstats_cmn cmn;
	uint32_t trigger_reason;
	uint32_t trigger_sub_reason;
	uint32_t rssi;
	uint32_t rssi_threshold;
	uint32_t timestamp;
	uint16_t cu;
	uint8_t is_full_scan;
	struct cstats_sta_roam_scan_cancel abort_roam;
} qdf_packed;

struct cstats_sta_roam_scan_done {
	struct cstats_cmn cmn;
	uint32_t timestamp;
	uint16_t cand_ap_count;
	uint16_t num_scanned_freq;
	uint16_t scanned_freq[MAX_ROAM_SCAN_CHAN];
	struct cstats_sta_roam_scan_ap ap[MAX_ROAM_CANDIDATE_AP];
	uint8_t is_full_scan;
} qdf_packed;

struct cstats_sta_roam_result {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint32_t timestamp;
	uint32_t status;
	uint32_t fail_reason;
	struct cstats_sta_roam_scan_cancel roam_abort;
	uint32_t roam_cancel_reason;
} qdf_packed;

struct cstats_tdls_disc_req {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t init_sta_addr[CSTATS_MAC_LEN];
	uint8_t resp_sta_addr[CSTATS_MAC_LEN];
	uint8_t act_category;
	uint8_t act;
	uint8_t dt;
	uint8_t direction;
} qdf_packed;

struct cstats_tdls_disc_resp {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t init_sta_addr[CSTATS_MAC_LEN];
	uint8_t resp_sta_addr[CSTATS_MAC_LEN];
	uint8_t act_category;
	uint8_t act;
	uint8_t dt;
	uint8_t direction;
	uint8_t flags;
} qdf_packed;

struct cstats_tdls_setup_req {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t init_sta_addr[CSTATS_MAC_LEN];
	uint8_t resp_sta_addr[CSTATS_MAC_LEN];
	uint8_t act_category;
	uint8_t act;
	uint8_t dt;
	uint8_t direction;
	uint8_t flags;
} qdf_packed;

struct cstats_tdls_setup_resp {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t init_sta_addr[CSTATS_MAC_LEN];
	uint8_t resp_sta_addr[CSTATS_MAC_LEN];
	uint16_t status;
	uint8_t act_category;
	uint8_t act;
	uint8_t dt;
	uint8_t direction;
	uint8_t flags;
} qdf_packed;

struct cstats_tdls_setup_confirm {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t init_sta_addr[CSTATS_MAC_LEN];
	uint8_t resp_sta_addr[CSTATS_MAC_LEN];
	uint16_t status;
	uint8_t act_category;
	uint8_t act;
	uint8_t dt;
	uint8_t direction;
	uint8_t flags;
} qdf_packed;

struct cstats_tdls_tear_down {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t init_sta_addr[CSTATS_MAC_LEN];
	uint8_t resp_sta_addr[CSTATS_MAC_LEN];
	uint16_t reason;
	uint8_t act_category;
	uint8_t act;
	uint8_t direction;
} qdf_packed;

struct cstats_sap_go_start {
	struct cstats_cmn cmn;
	uint8_t status;
	uint16_t operating_chan_freq;
	uint8_t ch_width;
	uint16_t staId;
	uint8_t ssid_len;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1];
	uint8_t bssid[CSTATS_MAC_LEN];
} qdf_packed;

struct cstats_sap_go_stop {
	struct cstats_cmn cmn;
	uint8_t status;
	uint8_t bssid[CSTATS_MAC_LEN];
} qdf_packed;

struct cstats_sap_go_dfs_evt {
	struct cstats_cmn cmn;
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t cc[CSTATS_MAC_LEN];
	uint16_t freq;
} qdf_packed;

struct cstats_sap_go_sta_disassoc {
	struct cstats_cmn cmn;
	uint8_t sta_mac[CSTATS_MAC_LEN];
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t sta_id;
	uint8_t status;
	uint32_t status_code;
	uint8_t reason;
	uint32_t reason_code;
} qdf_packed;

struct cstats_sap_go_sta_assoc_reassoc {
	struct cstats_cmn cmn;
	uint8_t sta_mac[CSTATS_MAC_LEN];
	uint8_t bssid[CSTATS_MAC_LEN];
	uint8_t sta_id;
	uint8_t status;
	uint32_t status_code;
} qdf_packed;

struct cstats_nan_disc_enable {
	struct cstats_cmn cmn;
	uint16_t social_chan_2g_freq;
	uint16_t social_chan_5g_freq;
	uint32_t rtt_cap;
	uint8_t disable_6g_nan;
} qdf_packed;

struct cstats_nan_disc_enable_resp {
	struct cstats_cmn cmn;
	uint8_t is_enable_success;
	uint8_t mac_id;
	uint8_t disc_state;
} qdf_packed;

struct cstats_nan_disc_disable_req {
	struct cstats_cmn cmn;
	uint8_t disable_2g_discovery;
	uint8_t disable_5g_discovery;
} qdf_packed;

struct cstats_nan_disc_disable_resp {
	struct cstats_cmn cmn;
	uint8_t disc_state;
} qdf_packed;

struct cstats_nan_ndi_create_req {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
} qdf_packed;

struct cstats_nan_ndi_create_resp {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
	uint8_t status;
	uint8_t reason;
} qdf_packed;

struct cstats_nan_ndi_delete_req {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
} qdf_packed;

struct cstats_nan_ndi_delete_resp {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
	uint8_t status;
	uint8_t reason;
} qdf_packed;

struct cstats_nan_ndp_initiator_req {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
	uint16_t channel;
	uint32_t channel_cfg;
	uint16_t service_instance_id;
	uint8_t self_ndi_mac_addr[CSTATS_MAC_LEN];
	uint8_t peer_discovery_mac_addr[CSTATS_MAC_LEN];
} qdf_packed;

struct cstats_nan_ndp_initiator_resp {
	struct cstats_cmn cmn;
	uint8_t status;
	uint8_t reason;
	uint16_t transaction_id;
	uint16_t service_instance_id;
} qdf_packed;

struct cstats_nan_ndp_responder_req {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
	uint16_t ndp_instance_id;
	uint8_t ndp_rsp;
	uint32_t ncs_sk_type;
} qdf_packed;

struct cstats_nan_ndp_responder_resp {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
	uint8_t status;
	uint8_t reason;
} qdf_packed;

struct cstats_nan_ndp_end_req {
	struct cstats_cmn cmn;
	uint16_t transaction_id;
	uint8_t num_ndp_instances;
} qdf_packed;

struct cstats_nan_ndp_end_resp {
	struct cstats_cmn cmn;
	uint8_t status;
	uint8_t reason;
	uint16_t transaction_id;
} qdf_packed;

struct cstats_nan_ndp_confirm_ind {
	struct cstats_cmn cmn;
	uint32_t instance_id;
	uint16_t reason_code;
	uint8_t peer_addr[CSTATS_MAC_LEN];
	uint8_t rsp_code;
} qdf_packed;

struct cstats_nan_ndp_ind {
	struct cstats_cmn cmn;
	uint8_t peer_mac[CSTATS_MAC_LEN];
	uint8_t peer_discovery_mac_addr[CSTATS_MAC_LEN];
	uint16_t ndp_instance_id;
	uint32_t service_instance_id;
} qdf_packed;

struct cstats_nan_ndp_new_peer_ind {
	struct cstats_cmn cmn;
	uint8_t peer_mac[CSTATS_MAC_LEN];
	uint16_t sta_id;
} qdf_packed;

struct cstats_pkt_info {
	struct cstats_cmn cmn;
	uint8_t src_mac[CSTATS_MAC_LEN];
	uint8_t dst_mac[CSTATS_MAC_LEN];
	uint8_t type;
	uint8_t dir;
	uint8_t status;
} qdf_packed;

#endif /* __WLAN_CP_STATS_CHIPSET_STATS_EVENTS_H */

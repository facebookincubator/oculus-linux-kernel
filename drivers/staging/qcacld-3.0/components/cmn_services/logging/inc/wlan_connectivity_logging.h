/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DOC: wlan_cm_roam_logging.c
 *
 * Implementation for the Connect/roaming logging.
 */

#ifndef _WLAN_CONNECTIVITY_LOGGING_H_
#define _WLAN_CONNECTIVITY_LOGGING_H_

#include "wlan_logging_sock_svc.h"
#include "wlan_cm_roam_public_struct.h"
#include "wlan_mlo_mgr_public_structs.h"

#define WLAN_MAX_LOGGING_FREQ 120

/**
 * enum wlan_main_tag  - Main Tag used in logging
 * @WLAN_CONNECTING: Connecting
 * @WLAN_CONNECTING_FAIL: Connection failure
 * @WLAN_AUTH_REQ: Authentication request frame
 * @WLAN_AUTH_RESP: Authentication response frame
 * @WLAN_ASSOC_REQ: Association request frame
 * @WLAN_ASSOC_RSP: Association response frame
 * @WLAN_REASSOC_REQ: Reassociation request frame
 * @WLAN_REASSOC_RSP: Reassociation response frame
 * @WLAN_DEAUTH_RX: Deauthentication frame received
 * @WLAN_DEAUTH_TX: Deauthentication frame sent
 * @WLAN_DISASSOC_RX: Disassociation frame received
 * @WLAN_DISASSOC_TX: Disassociation frame sent
 * @WLAN_DISCONN_BMISS: Disconnection due to beacon miss
 * @WLAN_ROAM_SCAN_START: ROAM scan start
 * @WLAN_ROAM_SCAN_DONE: Roam scan done
 * @WLAN_ROAM_SCORE_CURR_AP: Roam score current AP
 * @WLAN_ROAM_SCORE_CAND_AP: Roam Score Candidate AP
 * @WLAN_ROAM_RESULT: Roam Result
 * @WLAN_ROAM_CANCEL: Roam Cancel
 * @WLAN_BTM_REQ:  BTM request
 * @WLAN_BTM_QUERY: BTM Query frame
 * @WLAN_BTM_RESP: BTM response frame
 * @WLAN_BTM_REQ_CANDI: BTM request candidate info
 * @WLAN_ROAM_WTC: ROAM WTC trigger logs
 * @WLAN_DHCP_DISCOVER: DHCP discover frame
 * @WLAN_DHCP_OFFER: DHCP offer frame
 * @WLAN_DHCP_REQUEST: DHCP Request frame
 * @WLAN_DHCP_ACK: DHCP ACK
 * @WLAN_DHCP_NACK: DHCP NACK
 * @WLAN_EAPOL_M1: EAPOL M1
 * @WLAN_EAPOL_M2: EAPOL M2
 * @WLAN_EAPOL_M3: EAPOL M3
 * @WLAN_EAPOL_M4: EAPOL M4
 * @WLAN_GTK_M1: GTK rekey M1 frame
 * @WLAN_GTK_M2: GTK Rekey M2 frame
 * @WLAN_EAP_REQUEST: EAP request frame
 * @WLAN_EAP_RESPONSE: EAP response frame
 * @WLAN_EAP_SUCCESS: EAP success
 * @WLAN_EAP_FAILURE: EAP failure
 * @WLAN_CUSTOM_LOG: Additional WLAN logs
 * @WLAN_TAG_MAX: MAX tag
 */
enum wlan_main_tag {
	WLAN_CONNECTING,
	WLAN_CONNECTING_FAIL,
	WLAN_AUTH_REQ,
	WLAN_AUTH_RESP,
	WLAN_ASSOC_REQ,
	WLAN_ASSOC_RSP,
	WLAN_REASSOC_REQ,
	WLAN_REASSOC_RSP,
	WLAN_DEAUTH_RX,
	WLAN_DEAUTH_TX,
	WLAN_DISASSOC_RX,
	WLAN_DISASSOC_TX,
	WLAN_DISCONN_BMISS,
	WLAN_ROAM_SCAN_START,
	WLAN_ROAM_SCAN_DONE,
	WLAN_ROAM_SCORE_CURR_AP,
	WLAN_ROAM_SCORE_CAND_AP,
	WLAN_ROAM_RESULT,
	WLAN_ROAM_CANCEL,
	WLAN_BTM_REQ,
	WLAN_BTM_QUERY,
	WLAN_BTM_RESP,
	WLAN_BTM_REQ_CANDI,
	WLAN_ROAM_WTC,
	WLAN_DHCP_DISCOVER,
	WLAN_DHCP_OFFER,
	WLAN_DHCP_REQUEST,
	WLAN_DHCP_ACK,
	WLAN_DHCP_NACK,
	WLAN_EAPOL_M1,
	WLAN_EAPOL_M2,
	WLAN_EAPOL_M3,
	WLAN_EAPOL_M4,
	WLAN_GTK_M1,
	WLAN_GTK_M2,
	WLAN_EAP_REQUEST,
	WLAN_EAP_RESPONSE,
	WLAN_EAP_SUCCESS,
	WLAN_EAP_FAILURE,
	WLAN_CUSTOM_LOG,
	/* Keep at last */
	WLAN_TAG_MAX,
};

/**
 * enum qca_conn_diag_log_event_type  - Diag Event subtype used in logging
 * @WLAN_CONN_DIAG_CONNECTING_EVENT: Connecting
 * @WLAN_CONN_DIAG_CONNECT_FAIL_EVENT: Connection failure
 * @WLAN_CONN_DIAG_AUTH_REQ_EVENT: Authentication request frame
 * @WLAN_CONN_DIAG_AUTH_RESP_EVENT: Authentication response frame
 * @WLAN_CONN_DIAG_ASSOC_REQ_EVENT: Association request frame
 * @WLAN_CONN_DIAG_ASSOC_RESP_EVENT: Association response frame
 * @WLAN_CONN_DIAG_REASSOC_REQ_EVENT: Reassociation request frame
 * @WLAN_CONN_DIAG_REASSOC_RESP_EVENT: Reassociation response frame
 * @WLAN_CONN_DIAG_DEAUTH_RX_EVENT: Deauthentication frame received
 * @WLAN_CONN_DIAG_DEAUTH_TX_EVENT: Deauthentication frame sent
 * @WLAN_CONN_DIAG_DISASSOC_RX_EVENT: Disassociation frame received
 * @WLAN_CONN_DIAG_DISASSOC_TX_EVENT: Disassociation frame sent
 * @WLAN_CONN_DIAG_BMISS_EVENT: Disconnection due to beacon miss
 * @WLAN_CONN_DIAG_ROAM_SCAN_START_EVENT: ROAM scan start
 * @WLAN_CONN_DIAG_ROAM_SCAN_DONE_EVENT: Roam scan done
 * @WLAN_CONN_DIAG_ROAM_SCORE_CUR_AP_EVENT: Roam score current AP
 * @WLAN_CONN_DIAG_ROAM_SCORE_CAND_AP_EVENT: Roam Score Candidate AP
 * @WLAN_CONN_DIAG_ROAM_RESULT_EVENT: Roam Result
 * @WLAN_CONN_DIAG_ROAM_CANCEL_EVENT: Roam Cancel
 * @WLAN_CONN_DIAG_BTM_REQ_EVENT:  BTM request
 * @WLAN_CONN_DIAG_BTM_QUERY_EVENT: BTM Query frame
 * @WLAN_CONN_DIAG_BTM_RESP_EVENT: BTM response frame
 * @WLAN_CONN_DIAG_BTM_REQ_CAND_EVENT: BTM request candidate info
 * @WLAN_CONN_DIAG_BTM_WTC_EVENT: ROAM WTC trigger logs
 * @WLAN_CONN_DIAG_DHCP_DISC_EVENT: DHCP discover frame
 * @WLAN_CONN_DIAG_DHCP_OFFER_EVENT: DHCP offer frame
 * @WLAN_CONN_DIAG_DHCP_REQUEST_EVENT: DHCP Request frame
 * @WLAN_CONN_DIAG_DHCP_ACK_EVENT: DHCP ACK
 * @WLAN_CONN_DIAG_DHCP_NACK_EVENT: DHCP NACK
 * @WLAN_CONN_DIAG_EAPOL_M1_EVENT: EAPOL M1
 * @WLAN_CONN_DIAG_EAPOL_M2_EVENT: EAPOL M2
 * @WLAN_CONN_DIAG_EAPOL_M3_EVENT: EAPOL M3
 * @WLAN_CONN_DIAG_EAPOL_M4_EVENT: EAPOL M4
 * @WLAN_CONN_DIAG_GTK_M1_EVENT: GTK rekey M1 frame
 * @WLAN_CONN_DIAG_GTK_M2_EVENT: GTK Rekey M2 frame
 * @WLAN_CONN_DIAG_EAP_REQ_EVENT: EAP request frame
 * @WLAN_CONN_DIAG_EAP_RESP_EVENT: EAP response frame
 * @WLAN_CONN_DIAG_EAP_SUCC_EVENT: EAP success
 * @WLAN_CONN_DIAG_EAP_FAIL_EVENT: EAP failure
 * @WLAN_CONN_DIAG_CUSTOM_EVENT: Additional WLAN logs
 * @WLAN_CONN_DIAG_EAP_START_EVENT: EAPOL start frame
 * @WLAN_CONN_DIAG_NBR_RPT_REQ_EVENT: Neighbor report request
 * @WLAN_CONN_DIAG_NBR_RPT_RESP_EVENT: Neighbor report response
 * @WLAN_CONN_DIAG_BCN_RPT_REQ_EVENT: Beacon report request
 * @WLAN_CONN_DIAG_BCN_RPT_RESP_EVENT: Beacon report response
 * @WLAN_CONN_DIAG_MLO_T2LM_REQ_EVENT: MLO T2LM request
 * @WLAN_CONN_DIAG_MLO_T2LM_RESP_EVENT: MLO T2LM response
 * @WLAN_CONN_DIAG_BTM_BLOCK_EVENT: BTM-drop indication
 * @WLAN_CONN_DIAG_MAX: MAX tag
 */
enum qca_conn_diag_log_event_type {
	WLAN_CONN_DIAG_CONNECTING_EVENT = 0,
	WLAN_CONN_DIAG_CONNECT_FAIL_EVENT,
	WLAN_CONN_DIAG_AUTH_REQ_EVENT,
	WLAN_CONN_DIAG_AUTH_RESP_EVENT,
	WLAN_CONN_DIAG_ASSOC_REQ_EVENT,
	WLAN_CONN_DIAG_ASSOC_RESP_EVENT,
	WLAN_CONN_DIAG_REASSOC_REQ_EVENT,
	WLAN_CONN_DIAG_REASSOC_RESP_EVENT,
	WLAN_CONN_DIAG_DEAUTH_RX_EVENT,
	WLAN_CONN_DIAG_DEAUTH_TX_EVENT,
	WLAN_CONN_DIAG_DISASSOC_RX_EVENT,
	WLAN_CONN_DIAG_DISASSOC_TX_EVENT,
	WLAN_CONN_DIAG_BMISS_EVENT,
	WLAN_CONN_DIAG_ROAM_SCAN_START_EVENT,
	WLAN_CONN_DIAG_ROAM_SCAN_DONE_EVENT,
	WLAN_CONN_DIAG_ROAM_SCORE_CUR_AP_EVENT,
	WLAN_CONN_DIAG_ROAM_SCORE_CAND_AP_EVENT,
	WLAN_CONN_DIAG_ROAM_RESULT_EVENT,
	WLAN_CONN_DIAG_ROAM_CANCEL_EVENT,
	WLAN_CONN_DIAG_BTM_REQ_EVENT,
	WLAN_CONN_DIAG_BTM_QUERY_EVENT,
	WLAN_CONN_DIAG_BTM_RESP_EVENT,
	WLAN_CONN_DIAG_BTM_REQ_CAND_EVENT,
	WLAN_CONN_DIAG_BTM_WTC_EVENT,
	WLAN_CONN_DIAG_DHCP_DISC_EVENT,
	WLAN_CONN_DIAG_DHCP_OFFER_EVENT,
	WLAN_CONN_DIAG_DHCP_REQUEST_EVENT,
	WLAN_CONN_DIAG_DHCP_ACK_EVENT,
	WLAN_CONN_DIAG_DHCP_NACK_EVENT,
	WLAN_CONN_DIAG_EAPOL_M1_EVENT,
	WLAN_CONN_DIAG_EAPOL_M2_EVENT,
	WLAN_CONN_DIAG_EAPOL_M3_EVENT,
	WLAN_CONN_DIAG_EAPOL_M4_EVENT,
	WLAN_CONN_DIAG_GTK_M1_EVENT,
	WLAN_CONN_DIAG_GTK_M2_EVENT,
	WLAN_CONN_DIAG_EAP_REQ_EVENT,
	WLAN_CONN_DIAG_EAP_RESP_EVENT,
	WLAN_CONN_DIAG_EAP_SUCC_EVENT,
	WLAN_CONN_DIAG_EAP_FAIL_EVENT,
	WLAN_CONN_DIAG_CUSTOM_EVENT,
	WLAN_CONN_DIAG_EAP_START_EVENT,
	WLAN_CONN_DIAG_NBR_RPT_REQ_EVENT,
	WLAN_CONN_DIAG_NBR_RPT_RESP_EVENT,
	WLAN_CONN_DIAG_BCN_RPT_REQ_EVENT,
	WLAN_CONN_DIAG_BCN_RPT_RESP_EVENT,
	WLAN_CONN_DIAG_MLO_T2LM_REQ_EVENT,
	WLAN_CONN_DIAG_MLO_T2LM_RESP_EVENT,
	WLAN_CONN_DIAG_BTM_BLOCK_EVENT,
	WLAN_CONN_DIAG_MAX
};

/*
 * enum wlan_diag_wifi_band - Enum describing wifi band
 * @WLAN_INVALID_BAND: invalid band
 * @WLAN_24GHZ_BAND: 2.4 GHz band
 * @WLAN_5GHZ_BAND: 5 GHz band
 * @WLAN_6GHZ_BAND: 6 GHz band
 */
enum wlan_diag_wifi_band {
	WLAN_INVALID_BAND = 0,
	WLAN_24GHZ_BAND,
	WLAN_5GHZ_BAND,
	WLAN_6GHZ_BAND,
};

/**
 * enum wlan_diag_mlo_link_switch_reason - MLO link switch reason enumeration
 * @LINK_STATE_SWITCH_REASON_VDEV_READY: Link switch when vdev is ready
 * @LINK_STATE_SWITCH_REASON_ULL_MODE: Link switch due to ULL mode configuration
 * @LINK_STATE_SWITCH_REASON_T2LM_ENABLE: Link switch due to T2LM enable
 * @LINK_STATE_SWITCH_REASON_T2LM_DISABLE: Link switch due T2LM disable
 * @LINK_STATE_SWITCH_REASON_FORCE_ENABLED: Link switch when link is
 * forcibly enable
 * @LINK_STATE_SWITCH_REASON_FORCE_DISABLED: Link switch when link is
 * forcibly disable
 * @LINK_STATE_SWITCH_REASON_LINK_QUALITY: Link switch due to
 * poor link quality
 * @LINK_STATE_SWITCH_REASON_LINK_CAPACITY: Link switch due to link capacity
 * @LINK_STATE_SWITCH_REASON_RSSI: Link switch due to changes in rssi
 * @LINK_STATE_SWITCH_REASON_BMISS: Link switch due to BMISS
 * @LINK_STATE_SWITCH_REASON_BT_STATUS: Link switch due to BT status
 * @LINK_STATE_SWITCH_REASON_MAX: Max value
 */
enum wlan_diag_mlo_link_switch_reason {
	LINK_STATE_SWITCH_REASON_VDEV_READY = 0,
	LINK_STATE_SWITCH_REASON_ULL_MODE = 1,
	LINK_STATE_SWITCH_REASON_T2LM_ENABLE = 2,
	LINK_STATE_SWITCH_REASON_T2LM_DISABLE = 3,
	LINK_STATE_SWITCH_REASON_FORCE_ENABLED = 4,
	LINK_STATE_SWITCH_REASON_FORCE_DISABLED = 5,
	LINK_STATE_SWITCH_REASON_LINK_QUALITY = 6,
	LINK_STATE_SWITCH_REASON_LINK_CAPACITY = 7,
	LINK_STATE_SWITCH_REASON_RSSI = 8,
	LINK_STATE_SWITCH_REASON_BMISS = 9,
	LINK_STATE_SWITCH_REASON_BT_STATUS = 10,
	LINK_STATE_SWITCH_REASON_MAX,
};

/**
 * enum wlan_bcn_rpt_measurement_mode - Measurement mode enum.
 * Defined in IEEE Std 802.11‐2020 Table 9-103.
 * @MEASURE_MODE_PASSIVE: Passive measurement mode
 * @MEASURE_MODE_ACTIVE: Active measurement mode
 * @MEASURE_MODE_BCN_TABLE: Beacon table measurement mode
 * @MEASURE_MODE_RESERVED: Reserved
 */
enum wlan_bcn_rpt_measurement_mode {
	MEASURE_MODE_PASSIVE = 0,
	MEASURE_MODE_ACTIVE,
	MEASURE_MODE_BCN_TABLE,
	MEASURE_MODE_RESERVED = 0xFF
};

/**
 * enum wlan_diag_connect_fail_reason - WLAN diag connect fail reason code
 * @WLAN_DIAG_UNSPECIFIC_REASON: Unspecific reason
 * @WLAN_DIAG_NO_CANDIDATE_FOUND: No candidate found
 * @WLAN_DIAG_ABORT_DUE_TO_NEW_REQ_RECVD: Aborted as new command is
 * received.
 * @WLAN_DIAG_BSS_SELECT_IND_FAILED: Failed BSS select indication
 * @WLAN_DIAG_PEER_CREATE_FAILED: peer create failed
 * @WLAN_DIAG_JOIN_FAILED: Failed in joining state
 * @WLAN_DIAG_JOIN_TIMEOUT: Did not receive beacon or probe response after
 * unicast probe request
 * @WLAN_DIAG_AUTH_FAILED: Auth rejected by AP
 * @WLAN_DIAG_AUTH_TIMEOUT: No Auth resp from AP
 * @WLAN_DIAG_ASSOC_FAILED: Assoc rejected by AP
 * @WLAN_DIAG_ASSOC_TIMEOUT: No Assoc resp from AP
 * @WLAN_DIAG_HW_MODE_FAILURE: failed to change HW mode
 * @WLAN_DIAG_SER_FAILURE: Failed to serialize command
 * @WLAN_DIAG_SER_TIMEOUT: Serialization cmd timeout
 * @WLAN_DIAG_GENERIC_FAILURE: Generic failure apart from above
 * @WLAN_DIAG_VALID_CANDIDATE_CHECK_FAIL: Valid Candidate Check fail
 */
enum wlan_diag_connect_fail_reason {
	WLAN_DIAG_UNSPECIFIC_REASON = 0,
	WLAN_DIAG_NO_CANDIDATE_FOUND = 1,
	WLAN_DIAG_ABORT_DUE_TO_NEW_REQ_RECVD,
	WLAN_DIAG_BSS_SELECT_IND_FAILED,
	WLAN_DIAG_PEER_CREATE_FAILED,
	WLAN_DIAG_JOIN_FAILED,
	WLAN_DIAG_JOIN_TIMEOUT,
	WLAN_DIAG_AUTH_FAILED,
	WLAN_DIAG_AUTH_TIMEOUT,
	WLAN_DIAG_ASSOC_FAILED,
	WLAN_DIAG_ASSOC_TIMEOUT,
	WLAN_DIAG_HW_MODE_FAILURE,
	WLAN_DIAG_SER_FAILURE,
	WLAN_DIAG_SER_TIMEOUT,
	WLAN_DIAG_GENERIC_FAILURE,
	WLAN_DIAG_VALID_CANDIDATE_CHECK_FAIL,
};

/**
 * enum wlan_diag_btm_block_reason - BTM drop/ignore reason code
 * @WLAN_DIAG_BTM_BLOCK_MBO_WO_PMF: Connected to MBO without PMF capable AP
 * @WLAN_DIAG_BTM_BLOCK_UNSUPPORTED_P2P_CONC: p2p go/cli is present which
 *  restricts BTM roaming
 */
enum wlan_diag_btm_block_reason {
	WLAN_DIAG_BTM_BLOCK_MBO_WO_PMF = 1,
	WLAN_DIAG_BTM_BLOCK_UNSUPPORTED_P2P_CONC = 2,
};

/**
 * struct wlan_connectivity_log_diag_cmn - Structure for diag event
 * @bssid: bssid
 * @vdev_id: Vdev id
 * @timestamp_us: Timestamp(time of the day) in microseconds
 * @fw_timestamp: FW timestamp in microseconds
 * @ktime_us: Kernel Timestamp in microseconds
 */
struct wlan_connectivity_log_diag_cmn {
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint16_t vdev_id;
	uint64_t timestamp_us;
	uint64_t fw_timestamp;
	uint64_t ktime_us;
} qdf_packed;

#define DIAG_STA_INFO_VERSION 1

/**
 * struct wlan_diag_sta_info - STA info structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @is_mlo: MLO connection bit
 * @mac_2g: 2.4 GHz link station mac address
 * @mac_5g: 5 GHz link station mac address
 * @mac_6g: 6 GHz link station mac address
 */
struct wlan_diag_sta_info {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t is_mlo;
	uint8_t mac_2g[QDF_MAC_ADDR_SIZE];
	uint8_t mac_5g[QDF_MAC_ADDR_SIZE];
	uint8_t mac_6g[QDF_MAC_ADDR_SIZE];
} qdf_packed;

/*
 * struct wlan_diag_mlo_cmn_info - MLO common info
 * @band: Indicates link on which mlo setup is initiated.
 * Refer enum enum wlan_diag_wifi_band.
 * @link_id: Link id of the link when link is accepted
 * @vdev_id: vdev id associated with the link
 * @tid_ul: TID-to-link mapping information on the uplink
 * @tid_dl: TID-to-link mapping information on the downlink
 * @status: MLO setup status. 0 - Success, 1 - failure
 * @link_addr: Link address of the link.
 */
struct wlan_diag_mlo_cmn_info {
	uint8_t band;
	uint8_t link_id;
	uint8_t vdev_id;
	uint8_t tid_ul;
	uint8_t tid_dl;
	uint8_t status;
	uint8_t link_addr[QDF_MAC_ADDR_SIZE];
} qdf_packed;

#define DIAG_MLO_SETUP_VERSION 1
#define DIAG_MLO_SETUP_VERSION_V2 2

#define MAX_NUM_LINKS_PER_EVENT 3
/**
 * struct wlan_diag_mlo_setup - MLO setup structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @num_links: Number of links associated for MLO setup
 * @reserved: Reserved field
 * @status: status code of the link. Non-zero value when link is rejected
 * @mlo_cmn_info: MLO common info
 */
struct wlan_diag_mlo_setup {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t num_links;
	uint16_t reserved;
	struct wlan_diag_mlo_cmn_info mlo_cmn_info[MAX_NUM_LINKS_PER_EVENT];
} qdf_packed;

#define DIAG_MLO_RECONFIG_VERSION 1

/**
 * struct wlan_diag_mlo_reconfig - MLO reconfig diag event structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @reserved: Reserved field
 * @mlo_cmn_info: MLO common info
 */
struct wlan_diag_mlo_reconfig {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint32_t version:8;
	uint32_t reserved:24;
	struct wlan_diag_mlo_cmn_info mlo_cmn_info;
} qdf_packed;

#define DIAG_MLO_T2LM_STATUS_VERSION 1
#define DIAG_MLO_T2LM_STATUS_VERSION_V2 2

/**
 * struct wlan_diag_mlo_t2lm_status - MLO T2LM status diag event structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @num_links: Number of links associated for T2LM status
 * @reserved: Reserved field
 * @mlo_cmn_info: MLO common info
 */
struct wlan_diag_mlo_t2lm_status {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t num_links;
	uint16_t reserved;
	struct wlan_diag_mlo_cmn_info mlo_cmn_info[MAX_NUM_LINKS_PER_EVENT];
} qdf_packed;

#define DIAG_MLO_T2LM_REQ_RESP_VERSION 1

/**
 * struct wlan_diag_mlo_t2lm_req_resp - MLO T2LM Req/Resp diag event structure
 * @diag_cmn: Common diag info
 * @version: Structure version
 * @band: Indicates the band of the link
 * @status: status code of TID-to-Link mapping response frame
 * @token: Dialog Token field of TID-To-Link Mapping Request/Response frame
 * @is_rx: Indicates the direction of packet. 0 - TX and 1 - RX
 * @tx_status: tx status of transmitted packet. Refer enum qdf_dp_tx_rx_status
 * @subtype: Subtype of the event
 * @reserved: Reserved field
 */
struct wlan_diag_mlo_t2lm_req_resp {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t band;
	uint8_t status;
	uint8_t token;
	uint8_t is_rx:1;
	uint8_t tx_status:7;
	uint8_t subtype;
	uint16_t reserved;
} qdf_packed;

#define DIAG_MLO_T2LM_TEARDOWN_VERSION 1

/**
 * struct wlan_diag_mlo_t2lm_teardown - MLO T2LM Teardown diag event structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @band: Indicates the band of the link. Refer enum wlan_diag_wifi_band
 * @tx_status: tx status of transmitted packet. Refer enum qdf_dp_tx_rx_status
 * @reserved: Reserved field
 */
struct wlan_diag_mlo_t2lm_teardown {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t band;
	uint8_t tx_status;
	uint8_t reserved;
} qdf_packed;

#define DIAG_MLO_LINK_STATUS_VERSION 1
#define DIAG_MLO_LINK_STATUS_VERSION_2 2
/**
 * struct wlan_diag_mlo_link_status - MLO Link status diag event structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @active_link: List of current active links. BIT 0: 2.4 GHz BIT 1: 5 GHz
 * BIT 2: 6 GHz
 * @prev_active_link: List of previous active links. BIT 0: 2.4 G Hz
 * BIT 1: 5 GHz BIT 2: 6 GHz
 * @associated_links: Links associated in the current connection.
 * BIT 0: 2.4 GHz BIT 1: 5 GHz BIT 2: 6 GHz
 * @reserved: Reserved field
 * @reason: Reason for changed link status. Refer
 * enum wlan_diag_mlo_link_switch_reason
 */
struct wlan_diag_mlo_link_status {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t active_link:5;
	uint8_t prev_active_link:5;
	uint8_t associated_links:5;
	uint8_t reserved:1;
	uint8_t reason;
} qdf_packed;

#define DIAG_NBR_RPT_VERSION 1
#define DIAG_NBR_RPT_VERSION_2 2

/**
 * struct wlan_diag_nbr_rpt - Neighbor report structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @num_rpt: the number of neighbor report elements in response frame.
 * @subtype: Event Subtype
 * @token: dialog token. Dialog Token is a nonzero value chosen by the STA
 * @num_freq: Number of frequency in response frame
 * @ssid_len: SSID length
 * @seq_num: Sequence number
 * @ssid: SSID
 * @freq: Frequency list in response frame
 * @band: Band on which packet was received or transmitted.
 * Refer enum enum wlan_diag_wifi_band
 * @reserved: Reserved field
 */
struct wlan_diag_nbr_rpt {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t num_rpt;
	uint8_t subtype;
	uint8_t token;
	uint16_t num_freq;
	uint16_t ssid_len;
	uint32_t seq_num;
	char ssid[WLAN_SSID_MAX_LEN];
	uint32_t freq[WLAN_MAX_LOGGING_FREQ];
	uint32_t band:8;
	uint32_t reserved:24;
} qdf_packed;

#define DIAG_BCN_RPT_VERSION 1
#define DIAG_BCN_RPT_VERSION_2 2

/**
 * struct wlan_diag_bcn_rpt - Beacon report structure
 * @diag_cmn: Common diag info
 * @version: structure version
 * @subtype: Event Subtype
 * @diag_token: Dialog token
 * @op_class: Operating classes that include primary channels
 * @chan: The channel number field in the beacon report request.
 * @req_mode: hex value defines Duration mandatory, parallel, enable,
 * request, and report bits.
 * @num_rpt: the number of neighbor report elements in response frame.
 * @meas_token: A nonzero number that is unique among the Measurement Request
 * elements
 * @mode: Mode used for measurement.Values defined in IEEE
 * Std 802.11‐2020 Table 9-103.
 * @duration: The duration over which the Beacon report was measured.(in ms)
 * @seq_num: Sequence number.
 * @band: Band on which packet was received or transmitted.
 * Refer enum enum wlan_diag_wifi_band
 * @reserved: Reserved field
 */
struct wlan_diag_bcn_rpt {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t subtype;
	uint8_t diag_token;
	uint8_t op_class;
	uint8_t chan;
	uint8_t req_mode;
	uint8_t num_rpt;
	uint8_t meas_token;
	uint16_t mode;
	uint16_t duration;
	uint32_t seq_num;
	uint32_t band:8;
	uint32_t reserved:24;
} qdf_packed;

#define DIAG_ROAM_CAND_VERSION 1
#define DIAG_ROAM_CAND_VERSION_V2 2

/**
 * struct wlan_diag_roam_candidate_info  - Roam candidate information for
 * logging
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @is_current_ap: Is the entry candidate AP or connected AP
 * @is_mlo: MLO connection indicator
 * @reserved: Reserved
 * @idx: Entry index
 * @cu_load: Channel utilization load of the AP in percentage
 * @subtype: diag event subtype defined in enum qca_conn_diag_log_event_type
 * @total_score: Total candidate AP score
 * @freq: Candidate AP channel frequency in MHz
 * @rssi: Candidate AP RSSI in dBm
 * @etp: Estimated throughput value of the AP in Kbps
 */
struct wlan_diag_roam_candidate_info {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t is_current_ap:1;
	uint8_t is_mlo:1;
	uint8_t reserved:6;
	uint16_t idx;
	uint16_t cu_load;
	uint16_t subtype;
	uint32_t total_score;
	uint32_t freq;
	int32_t rssi;
	uint32_t etp;
} qdf_packed;

#define DIAG_SCAN_DONE_VERSION 1

/**
 * struct wlan_diag_roam_scan_done - Roam scan related information
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @btcoex_active: Is there active bluetooth connection
 * @reserved: Reserved field
 * @cand_ap_count: Roam candidate AP count
 * @num_scanned_freq: Number of scanned frequencies
 * @scan_freq: Array of scanned frequencies value in MHz
 */
struct wlan_diag_roam_scan_done {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t btcoex_active:1;
	uint8_t reserved:7;
	uint16_t cand_ap_count;
	uint16_t num_scanned_freq;
	uint32_t scan_freq[WLAN_MAX_LOGGING_FREQ];
} qdf_packed;

#define DIAG_ROAM_RESULT_VERSION 1

/**
 * struct wlan_diag_roam_result - Roam result data
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @is_roam_successful: True if roamed successfully or false if roaming failed
 * @is_mlo: Indicates whether the current connection is a MLO connection
 * @reserved: Reserved
 * @roam_fail_reason: Roam failure reason code defined in enum
 * wlan_roam_failure_reason_code
 */
struct wlan_diag_roam_result {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t is_roam_successful:1;
	uint8_t is_mlo:1;
	uint8_t reserved:6;
	uint16_t roam_fail_reason;
} qdf_packed;

#define DIAG_ROAM_SCAN_START_VERSION 1
#define DIAG_ROAM_SCAN_START_VERSION_V2 2

/**
 * struct wlan_diag_roam_scan_start - Structure to store roam scan trigger
 * related data.
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @is_full_scan: True if the scan is Full scan. False if the roam scan is
 * partial channel map scan
 * @band: Band involved in the roaming during a MLO connection.
 * Refer enum enum wlan_diag_wifi_band
 * @cu:  Current connected channel load in percentage
 * @trigger_reason: Roam trigger reason defined by enum roam_trigger_reason
 * @trigger_sub_reason: Roam scan trigger sub reason indicating if
 * periodic/inactivity scan timer initiated roam. Defined by enum
 * roam_trigger_sub_reason
 * @rssi: Connected AP RSSI in dBm
 * @rssi_thresh: Roam scan trigger threshold in dBm
 */
struct wlan_diag_roam_scan_start {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t is_full_scan:1;
	uint8_t band:7;
	uint16_t cu;
	uint32_t trigger_reason;
	uint32_t trigger_sub_reason;
	int32_t rssi;
	int32_t rssi_thresh;
} qdf_packed;

#define DIAG_BTM_CAND_VERSION 1

/**
 * struct wlan_diag_btm_cand_info  - BTM candidate information
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @pad: Padded field
 * @idx: Candidate index
 * @preference: Candidate preference
 */
struct wlan_diag_btm_cand_info {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t pad;
	uint16_t idx;
	uint32_t preference;
} qdf_packed;

#define DIAG_BTM_VERSION 1
#define DIAG_BTM_VERSION_2 2

/**
 * struct wlan_diag_btm_info - BTM frame related logging data
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @reason: Query Reason field. Contains one of the values defined in IEEE
 * Std 802.11‐2020 Table 9-198—Transition and Transition Query reasons
 * @mode: BTM Request Mode field
 * @sub_reason: WTC sub reason code field in the BTM WTC vendor specific IE
 * @cand_lst_cnt: Candidates list in the BTM frame
 * @status: BSS Transition management status codes defined in
 * 802.11‐2020 Table 9-428—BTM status code definitions
 * @delay: BSS Termination Delay field
 * @is_disassoc_imminent: Disassociation imminent bit
 * @band: indicates the link involved in MLO conenection.
 * Refer enum enum wlan_diag_wifi_band
 * @token: dialog token. Dialog Token is a nonzero value chosen by the STA
 * @wtc_duration: WTC duration field in minutes
 * while sending the BTM frame to identify the query/request/response
 * transaction
 * @subtype: Event Subtype
 * @validity_timer: Validity interval in TBTT
 * @disassoc_tim: Time after which the AP disassociates the STA, defined
 * in TBTT.
 */
struct wlan_diag_btm_info {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t reason;
	uint8_t mode;
	uint8_t sub_reason;
	uint8_t cand_lst_cnt;
	uint8_t status;
	uint8_t delay;
	uint8_t is_disassoc_imminent:1;
	uint8_t band:7;
	uint8_t token;
	uint8_t subtype;
	uint16_t wtc_duration;
	uint32_t validity_timer;
	uint32_t disassoc_tim;
} qdf_packed;

#define DIAG_MGMT_VERSION 1
#define DIAG_MGMT_VERSION_V2 2
#define MAX_VSIE_LEN 255

/**
 * struct wlan_diag_packet_info - Data packets related info
 * @diag_cmn: Common diag info
 * @version: Structure Version
 * @auth_algo: authentication algorithm number defined in IEEE Std 802.11‐2020
 * @auth_frame_type: Authentication frame sub-type for SAE authentication
 * defined in Section 9.4.1.1 Authentication Algorithm Number field in
 * IEEE Std 802.11‐2020.
 * @auth_seq_num: Authentication frame transaction sequence number
 * @status: Frame status code as defined in IEEE Std
 * 802.11‐2020 Table 9-50—Status codes.
 * @tx_status: Frame TX status defined by enum qdf_dp_tx_rx_status
 * @reason: reason code defined in Table 9-49 Reason codes field’ from the
 * IEEE 802.11 standard document.
 * @is_retry_frame: Retry frame indicator
 * @is_tx: Packet direction indicator. 0 - RX, 1 - TX
 * @supported_links: link id bitmap indicates the links involved
 * in MLO connection.
 * @reserved: Reserved field
 * @subtype: Diag event defined in  enum qca_conn_diag_log_event_type
 * @assoc_id: Association ID
 * @eap_len: EAP data length
 * @eap_type: EAP type. Values defined by IANA at:
 * https://www.iana.org/assignments/eap-numbers
 * @sn: Frame sequence number
 * @rssi: Peer RSSI in dBm
 * @tx_fail_reason: tx failure reason printed on TX_FAIL status.
 * Refer enum qdf_dp_tx_rx_status
 * @mld_addr: MLD mac address
 * @vsie_len: VSIE length
 * @vsie: VSIE
 */
struct wlan_diag_packet_info {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t auth_algo;
	uint8_t auth_frame_type;
	uint8_t auth_seq_num;
	uint8_t status;
	uint8_t tx_status;
	uint8_t reason;
	uint8_t is_retry_frame:1;
	uint8_t is_tx:1;
	uint8_t supported_links:6;
	uint16_t subtype;
	uint16_t assoc_id;
	uint16_t eap_len;
	uint16_t eap_type;
	uint32_t sn;
	int32_t rssi;
	uint8_t tx_fail_reason;
	uint8_t mld_addr[QDF_MAC_ADDR_SIZE];
	uint8_t vsie_len;
	uint8_t vsie[MAX_VSIE_LEN];
} qdf_packed;

#define DIAG_CONN_VERSION 1

/**
 * struct wlan_diag_connect - Connection related info
 * @diag_cmn: Common diag info
 * @version: Version number
 * @auth_algo: Authentication algorithm number field as defined in
 * IEEE 802.11 - 2020 standard section 9.4.1.1
 * @bt_coex: Is there active bluetooth connection
 * @reserved: Reserved field
 * @ssid_len: Length of SSID
 * @ssid: SSID
 * @bssid_hint: BSSID hint provided in the connect request
 * @reason: failure reason. Refer enum wlan_cm_connect_fail_reason
 * @akm: Auth key management suite defined in IEEE Std 802.11‐2020
 * Table 9-151—AKM suite selectors.
 * @subtype: Event subtype defined in enum qca_conn_diag_log_event_type.
 * @freq: Frequency in MHz
 * @freq_hint: Frequency Hint in MHz
 * @pairwise_cipher: Pairwise suite value as defined in IEEE 802.11 2020
 * Table 12-10—Integrity and key wrap algorithms.
 * @grp_cipher: Group cipher suite value as defined in
 * Table 12-10—Integrity and key wrap algorithm in IEEE 802.11 2020.
 * @grp_mgmt: Group management cipher suite as defined in
 * Table 12-10—Integrity and key wrap algorithms in IEEE 802.11 2020.
 */
struct wlan_diag_connect {
	struct wlan_connectivity_log_diag_cmn diag_cmn;
	uint8_t version;
	uint8_t auth_algo;
	uint8_t bt_coex:1;
	uint8_t reserved:7;
	uint8_t ssid_len;
	char ssid[WLAN_SSID_MAX_LEN];
	uint8_t bssid_hint[6];
	uint16_t reason;
	uint32_t akm;
	uint32_t subtype;
	uint32_t freq;
	uint32_t freq_hint;
	uint32_t pairwise_cipher;
	uint32_t grp_cipher;
	uint32_t grp_mgmt;
} qdf_packed;

/**
 * struct wlan_roam_candidate_info  - Roam candidate information for logging
 * @cand_bssid: BSSID of the candidate AP
 * @is_current_ap: Is the entry candidate AP or connected AP
 * @idx: Entry index
 * @cu_load: Channel utilization load of the AP in percentage
 * @freq: Candidate AP channel frequency in MHz
 * @total_score: Total candidate AP score
 * @rssi: Candidate AP RSSI in dBm
 * @etp: Estimated throughput value of the AP in Kbps
 */
struct wlan_roam_candidate_info {
	struct qdf_mac_addr cand_bssid;
	bool is_current_ap;
	uint8_t idx;
	uint8_t cu_load;
	qdf_freq_t freq;
	uint16_t total_score;
	int32_t rssi;
	uint32_t etp;
};

/**
 * struct wlan_roam_scan_info  - Roam scan related information
 * @cand_ap_count: Roam candidate AP count
 * @num_scanned_freq: Number of scanned frequencies
 * @is_btcoex_active: Is bluetooth coex active
 * @scan_freq: Array of scanned frequencies value in MHz
 */
struct wlan_roam_scan_info {
	uint8_t cand_ap_count;
	uint16_t num_scanned_freq;
	bool is_btcoex_active;
	qdf_freq_t scan_freq[NUM_CHANNELS];
};

/**
 * struct wlan_roam_result_info  - Roam result data
 * @roam_fail_reason: Roam failure reason code defined in enum
 * wlan_roam_failure_reason_code
 * @is_roam_successful: True if roamed successfully or false if roaming failed
 */
struct wlan_roam_result_info {
	enum wlan_roam_failure_reason_code roam_fail_reason;
	bool is_roam_successful;
};

/**
 * struct wlan_roam_trigger_info - Structure to store roam trigger related data.
 * @is_full_scan: True if the scan is Full scan. False if the roam scan is
 * partial channel map scan
 * @trigger_reason: Roam trigger reason defined by enum roam_trigger_reason
 * @trigger_sub_reason: Roam scan trigger sub reason indicating if
 * periodic/inactivity scan timer initiated roam. Defined by enum
 * roam_trigger_sub_reason
 * @cu_load:  Current connected channel load in percentage
 * @current_rssi: Connected AP RSSI in dBm
 * @rssi_threshold: Roam scan trigger threshold in dBm
 */
struct wlan_roam_trigger_info {
	bool is_full_scan;
	enum roam_trigger_reason trigger_reason;
	enum roam_trigger_sub_reason trigger_sub_reason;
	uint8_t cu_load;
	int32_t current_rssi;
	int32_t rssi_threshold;
};

/**
 * struct wlan_btm_cand_info  - BTM candidate information
 * @idx: Candidate index
 * @preference: Candidate preference
 * @bssid: candidate bssid
 */
struct wlan_btm_cand_info {
	uint8_t idx;
	uint8_t preference;
	struct qdf_mac_addr bssid;
};

/**
 * struct wlan_roam_btm_info - BTM frame related logging data
 * @reason: Query Reason field. Contains one of the values defined in IEEE
 * Std 802.11‐2020 Table 9-198—Transition and Transition Query reasons
 * @mode: BTM Request Mode field
 * @sub_reason: WTC sub reason code field in the BTM WTC vendor specific IE
 * @candidate_list_count: Candidates list in the BTM frame
 * @btm_status_code: BSS Transition management status codes defined in
 * 802.11‐2020 Table 9-428—BTM status code definitions
 * @btm_delay: BSS Termination Delay field
 * @is_disassoc_imminent: Disassociation imminent bit
 * @token: dialog token. Dialog Token is a nonzero value chosen by the STA
 * while sending the BTM frame to identify the query/request/response
 * transaction
 * @validity_timer: Validity interval in TBTT
 * @disassoc_timer: Time after which the AP disassociates the STA, defined
 * in TBTT.
 * @wtc_duration: WTC duration field in minutes
 * @target_bssid: BTM response target bssid field
 */
struct wlan_roam_btm_info {
	uint8_t reason;
	uint8_t mode;
	uint8_t sub_reason;
	uint8_t candidate_list_count;
	uint8_t btm_status_code;
	uint8_t btm_delay;
	bool is_disassoc_imminent;
	uint8_t token;
	uint8_t validity_timer;
	uint16_t disassoc_timer;
	uint32_t wtc_duration;
	struct qdf_mac_addr target_bssid;
};

/**
 * struct wlan_packet_info  - Data packets related info
 * @tx_status: Frame TX status defined by enum qdf_dp_tx_rx_status
 * @eap_type: EAP type. Values defined by IANA at:
 * https://www.iana.org/assignments/eap-numbers
 * @eap_len: EAP data length
 * @auth_algo: authentication algorithm number defined in IEEE Std 802.11‐2020
 * Section 9.4.1.1 Authentication Algorithm Number field.
 * @auth_seq_num: Authentication frame transaction sequence number
 * @auth_type: Authentication frame sub-type for SAE authentication. Possible
 * values:
 * 1 - SAE commit frame
 * 2 - SAE confirm frame
 * @assoc_id: Association ID received in association response frame as
 * defined in IEEE Std 802.11-2020 Figure 9-91-AID field format.
 * @frame_status_code: Frame status code as defined in IEEE Std
 * 802.11 2020 Table 9-50—Status codes.
 * @seq_num: Frame sequence number
 * @rssi: Peer RSSI in dBm
 * @is_retry_frame: is frame retried
 */
struct wlan_packet_info {
	uint8_t tx_status;
	uint8_t eap_type;
	uint16_t eap_len;
	uint8_t auth_algo;
	uint8_t auth_seq_num;
	uint8_t auth_type;
	uint16_t assoc_id;
	uint16_t frame_status_code;
	uint16_t seq_num;
	int32_t rssi;
	bool is_retry_frame;
};

/**
 * struct wlan_connect_info  - Connection related info
 * @ssid: SSID
 * @ssid_len: Length of the SSID
 * @bssid_hint: BSSID hint provided in the connect request
 * @freq: Frequency in MHz
 * @freq_hint: Frequency Hint in MHz
 * @akm: Auth key management suite defined in IEEE Std 802.11‐2020
 * Table 9-151—AKM suite selectors.
 * @pairwise: Pairwise suite value as defined in IEEE 802.11 2020
 * Table 12-10—Integrity and key wrap algorithms.
 * @group: Group cipher suite value as defined in
 * Table 12-10—Integrity and key wrap algorithms.
 * @group_mgmt: Group management cipher suite as defined in
 * Table 12-10—Integrity and key wrap algorithms.
 * @auth_type: Authentication algorithm number field as defined in
 * IEEE 802.11 - 2020 standard section 9.4.1.1
 * @conn_status: Connection failure status defined by enum
 * wlan_cm_connect_fail_reason
 * @is_bt_coex_active: Is there active bluetooth connection
 */
struct wlan_connect_info {
	char ssid[WLAN_SSID_MAX_LEN];
	uint8_t ssid_len;
	struct qdf_mac_addr bssid_hint;
	qdf_freq_t freq;
	qdf_freq_t freq_hint;
	uint32_t akm;
	uint32_t pairwise;
	uint32_t group;
	uint32_t group_mgmt;
	uint8_t auth_type;
	enum wlan_cm_connect_fail_reason conn_status;
	bool is_bt_coex_active;
};

#define WLAN_MAX_LOG_RECORDS 45
#define WLAN_MAX_LOG_LEN     256
#define WLAN_RECORDS_PER_SEC 20
#define MAX_RECORD_IN_SINGLE_EVT 5

/**
 * struct wlan_log_record  - Structure for individual records in the ring
 * buffer
 * @timestamp_us: Timestamp(time of the day) in microseconds
 * @fw_timestamp_us: timestamp at which roam scan was triggered
 * @ktime_us: kernel timestamp (time of the day) in microseconds
 * @vdev_id: VDEV id
 * @log_subtype: Tag of the log
 * @bssid: AP bssid
 * @is_record_filled: indicates if the current record is empty or not
 * @conn_info: Connection info
 * @pkt_info: Packet info
 * @roam_scan: Roam scan
 * @ap: Roam candidate AP info
 * @roam_result: Roam result
 * @roam_trig: Roam trigger related info
 * @btm_info: BTM info
 * @btm_cand: BTM response candidate info
 */
struct wlan_log_record {
	uint64_t timestamp_us;
	uint64_t fw_timestamp_us;
	uint64_t ktime_us;
	uint8_t vdev_id;
	uint32_t log_subtype;
	struct qdf_mac_addr bssid;
	bool is_record_filled;
	union {
		struct wlan_connect_info conn_info;
		struct wlan_packet_info pkt_info;
		struct wlan_roam_scan_info roam_scan;
		struct wlan_roam_candidate_info ap;
		struct wlan_roam_result_info roam_result;
		struct wlan_roam_trigger_info roam_trig;
		struct wlan_roam_btm_info btm_info;
		struct wlan_btm_cand_info btm_cand;
	};
};

/**
 * struct wlan_cl_osif_cbks  - OSIF callbacks to be invoked for connectivity
 * logging
 * @wlan_connectivity_log_send_to_usr: Send the log buffer to user space
 */
struct wlan_cl_osif_cbks {
	QDF_STATUS
	(*wlan_connectivity_log_send_to_usr) (struct wlan_log_record *rec,
					      void *context,
					      uint8_t num_records);
};

/**
 * struct wlan_connectivity_log_buf_data  - Master structure to hold the
 * pointers to the ring buffers.
 * @psoc: Global psoc pointer
 * @osif_cbks: OSIF callbacks
 * @osif_cb_context: Pointer to the context to be passed to OSIF
 * callback
 * @first_record_timestamp_in_last_sec: First record timestamp
 * @sent_msgs_count: Total sent messages counter in the last 1 sec
 * @head: Pointer to the 1st record allocated in the ring buffer.
 * @read_ptr: Pointer to the next record that can be read.
 * @write_ptr: Pointer to the next empty record to be written.
 * @write_ptr_lock: Spinlock to protect the write_ptr from multiple producers.
 * @max_records: Maximum records in the ring buffer.
 * @read_idx: Read index
 * @write_idx: Write index
 * @dropped_msgs: Dropped logs counter
 * @is_active: If the global buffer is initialized or not
 */
struct wlan_connectivity_log_buf_data {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cl_osif_cbks osif_cbks;
	void *osif_cb_context;
	uint64_t first_record_timestamp_in_last_sec;
	uint64_t sent_msgs_count;
	struct wlan_log_record *head;
	struct wlan_log_record *read_ptr;
	struct wlan_log_record *write_ptr;
	qdf_spinlock_t write_ptr_lock;
	uint8_t max_records;
	uint8_t read_idx;
	uint8_t write_idx;
	qdf_atomic_t dropped_msgs;
	qdf_atomic_t is_active;
};

#define logging_err_rl(params...) \
	QDF_TRACE_ERROR_RL(QDF_MODULE_ID_MLME, ## params)
#define logging_warn_rl(params...) \
	QDF_TRACE_WARN_RL(QDF_MODULE_ID_MLME, ## params)
#define logging_info_rl(params...) \
	QDF_TRACE_INFO_RL(QDF_MODULE_ID_MLME, ## params)

#define logging_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_QDF, ## params)
#define logging_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_QDF, ## params)
#define logging_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_QDF, ## params)

#if (defined(CONNECTIVITY_DIAG_EVENT) && \
	defined(WLAN_FEATURE_ROAM_OFFLOAD))
/**
 * wlan_print_cached_sae_auth_logs() - Enqueue SAE authentication frame logs
 * @psoc: Global psoc pointer
 * @bssid:  BSSID
 * @vdev_id: Vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_print_cached_sae_auth_logs(struct wlan_objmgr_psoc *psoc,
					   struct qdf_mac_addr *bssid,
					   uint8_t vdev_id);

/**
 * wlan_is_log_record_present_for_bssid() - Check if there is existing log
 * record for the given bssid
 * @psoc: Global psoc pointer
 * @bssid: BSSID
 * @vdev_id: vdev id
 *
 * Return: true if record is present else false
 */
bool wlan_is_log_record_present_for_bssid(struct wlan_objmgr_psoc *psoc,
					  struct qdf_mac_addr *bssid,
					  uint8_t vdev_id);

/**
 * wlan_is_sae_auth_log_present_for_bssid() - Is cached SAE auth log record
 * present for the given bssid. This API checks on all the link vdev if the
 * given vdev_id is an MLO vdev and updates the vdev_id to caller in which
 * the auth frame was cached.
 * @psoc: Global psoc pointer
 * @bssid: BSSID
 * @vdev_id: vdev id
 *
 * Return: True if an entry is found
 */
bool
wlan_is_sae_auth_log_present_for_bssid(struct wlan_objmgr_psoc *psoc,
				       struct qdf_mac_addr *bssid,
				       uint8_t *vdev_id);

/**
 * wlan_clear_sae_auth_logs_cache() - Clear the cached auth related logs
 * @psoc: Pointer to global psoc object
 * @vdev_id: vdev id
 *
 * Return: None
 */
void wlan_clear_sae_auth_logs_cache(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id);
#else
static inline
QDF_STATUS wlan_print_cached_sae_auth_logs(struct wlan_objmgr_psoc *psoc,
					   struct qdf_mac_addr *bssid,
					   uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool wlan_is_log_record_present_for_bssid(struct wlan_objmgr_psoc *psoc,
					  struct qdf_mac_addr *bssid,
					  uint8_t vdev_id)
{
	return false;
}

static inline bool
wlan_is_sae_auth_log_present_for_bssid(struct wlan_objmgr_psoc *psoc,
				       struct qdf_mac_addr *bssid,
				       uint8_t *vdev_id)
{
	return false;
}

static inline
void wlan_clear_sae_auth_logs_cache(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id)
{}
#endif

#if defined(CONNECTIVITY_DIAG_EVENT)
/**
 * wlan_connectivity_mgmt_event()  - Fill and enqueue a new record
 * for management frame information.
 * @psoc: Pointer to global psoc object
 * @mac_hdr: 802.11 management frame header
 * @vdev_id: Vdev id
 * @status_code: Frame status code as defined in IEEE 802.11 - 2020 standard
 * section 9.4.1.9
 * @tx_status: Frame TX status defined by enum qdf_dp_tx_rx_status
 * @peer_rssi: Peer RSSI in dBm
 * @auth_algo: Authentication algorithm number field as defined in IEEE 802.11 -
 * 2020 standard section 9.4.1.1
 * @auth_type: indicates SAE authentication frame type. Possible values are:
 * 1 - SAE commit frame
 * 2 - SAE confirm frame
 * @auth_seq: Authentication frame transaction sequence number as defined in
 * IEEE 802.11 - 2020 standard section 9.4.1.2
 * @aid: Association ID
 * @tag: Record type main tag
 *
 * Return: QDF_STATUS
 */
void
wlan_connectivity_mgmt_event(struct wlan_objmgr_psoc *psoc,
			     struct wlan_frame_hdr *mac_hdr,
			     uint8_t vdev_id, uint16_t status_code,
			     enum qdf_dp_tx_rx_status tx_status,
			     int8_t peer_rssi,
			     uint8_t auth_algo, uint8_t auth_type,
			     uint8_t auth_seq, uint16_t aid,
			     enum wlan_main_tag tag);

/**
 * wlan_populate_vsie() - Populate VSIE field for logging
 * @vdev: vdev pointer
 * @data: Diag packet info data
 * @is_tx: flag to indicate whether packet transmitted or received
 *
 * Return: None
 */
void
wlan_populate_vsie(struct wlan_objmgr_vdev *vdev,
		   struct wlan_diag_packet_info *data, bool is_tx);

/**
 * wlan_cdp_set_peer_freq() - API to set frequency to dp peer
 * @psoc: psoc pointer
 * @peer_mac: Bssid of peer
 * @freq: frequency(in MHz)
 * @vdev_id: vdev id
 *
 * Return: None
 */
void
wlan_cdp_set_peer_freq(struct wlan_objmgr_psoc *psoc, uint8_t *peer_mac,
		       uint32_t freq, uint8_t vdev_id);

#ifdef WLAN_FEATURE_11BE_MLO

/**
 * wlan_connectivity_mlo_reconfig_event() -API to log MLO reconfig event
 * @vdev: vdev pointer
 *
 * Return: None
 */
void
wlan_connectivity_mlo_reconfig_event(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_connectivity_mlo_setup_event() - Fill and send MLO setup data
 * @vdev: vdev pointer
 *
 * Return: None
 */
void wlan_connectivity_mlo_setup_event(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_connectivity_t2lm_req_resp_event - API to send t2lm Req/resp
 * event logs to userspace
 * @vdev: vdev pointer
 * @token: dialog Token
 * @t2lm_status: T2LM response status code. Refer enum wlan_t2lm_resp_frm_type
 * @tx_status: TX status
 * @freq: frequency on which frame was transmitted/received
 * @is_rx: Flag to inidcate packet being received
 * @subtype: Determine whether the evnt sent is for t2lm request
 * or t2lm response
 *
 * Return: None
 */
void
wlan_connectivity_t2lm_req_resp_event(struct wlan_objmgr_vdev *vdev,
				      uint8_t token,
				      enum wlan_t2lm_resp_frm_type t2lm_status,
				      enum qdf_dp_tx_rx_status tx_status,
				      qdf_freq_t freq,
				      bool is_rx, uint8_t subtype);
/**
 * wlan_connectivity_t2lm_status_event() - Fill and send T2LM data
 * @vdev: vdev pointer
 *
 * Return: None
 */
void wlan_connectivity_t2lm_status_event(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_populate_mlo_mgmt_event_param() - API to populate MLO management frame
 * parameter
 * @vdev: vdev pointer
 * @data: Buffer to be filled with MLO parameter
 * @tag: WLAN event tag. Refer enum wlan_main_tag
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_populate_mlo_mgmt_event_param(struct wlan_objmgr_vdev *vdev,
				   struct wlan_diag_packet_info *data,
				   enum wlan_main_tag tag);

/**
 * wlan_populate_roam_mld_log_param() - Populate roam MLO log parameters
 * @vdev: Pointer to vdev object
 * @data: Diag event packet info
 * @tag: Main Tag
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_populate_roam_mld_log_param(struct wlan_objmgr_vdev *vdev,
				 struct wlan_diag_packet_info *data,
				 enum wlan_main_tag tag);
#else
static inline void
wlan_connectivity_mlo_reconfig_event(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_connectivity_mlo_setup_event(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_connectivity_t2lm_req_resp_event(struct wlan_objmgr_vdev *vdev,
				      uint8_t token,
				      enum wlan_t2lm_resp_frm_type status,
				      enum qdf_dp_tx_rx_status tx_status,
				      qdf_freq_t freq,
				      bool is_rx, uint8_t subtype)
{}

static inline void
wlan_connectivity_t2lm_status_event(struct wlan_objmgr_vdev *vdev)
{
}

static inline QDF_STATUS
wlan_populate_mlo_mgmt_event_param(struct wlan_objmgr_vdev *vdev,
				   struct wlan_diag_packet_info *data,
				   enum wlan_main_tag tag)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_populate_roam_mld_log_param(struct wlan_objmgr_vdev *vdev,
				 struct wlan_diag_packet_info *data,
				 enum wlan_main_tag tag)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_populate_vsie() - Populate VSIE field for logging
 * @vdev: vdev pointer
 * @data: Diag packet info data
 * @is_tx: flag to indicate whether packet transmitted or received
 *
 * Return: None
 */
void
wlan_populate_vsie(struct wlan_objmgr_vdev *vdev,
		   struct wlan_diag_packet_info *data, bool is_tx);
/**
 * wlan_convert_freq_to_diag_band() - API to convert frequency to band value
 * mentioned in enum wlan_diag_wifi_band
 * @ch_freq: Frequency(in MHz)
 *
 * Return: Band specified in enum wlan_diag_wifi_band
 */
enum wlan_diag_wifi_band
wlan_convert_freq_to_diag_band(uint16_t ch_freq);

static inline void wlan_connectivity_logging_stop(void)
{}

/**
 * wlan_connectivity_sta_info_event() - APi to send STA info event
 * @psoc: Pointer to global psoc object
 * @vdev_id: Vdev id
 * @is_roam: Is sta info event for roaming stats
 *
 * Return: None
 */
void
wlan_connectivity_sta_info_event(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, bool is_roam);

/**
 * wlan_connectivity_connecting_event() - API to log connecting event
 * @vdev: vdev pointer
 * @con_req: Connection request parameter
 *
 * Return: None
 */
void
wlan_connectivity_connecting_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_connect_req *con_req);

#elif defined(WLAN_FEATURE_CONNECTIVITY_LOGGING)
/**
 * wlan_connectivity_logging_start()  - Initialize the connectivity/roaming
 * logging buffer
 * @psoc: Global psoc pointer
 * @osif_cbks: OSIF callbacks
 * @osif_cb_context: OSIF callback context argument
 *
 * Return: None
 */
void wlan_connectivity_logging_start(struct wlan_objmgr_psoc *psoc,
				     struct wlan_cl_osif_cbks *osif_cbks,
				     void *osif_cb_context);

/**
 * wlan_connectivity_logging_stop() - Deinitialize the connectivity logging
 * buffers and spinlocks.
 *
 * Return: None
 */
void wlan_connectivity_logging_stop(void);

/**
 * wlan_connectivity_log_dequeue() - Send the connectivity logs to userspace
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_connectivity_log_dequeue(void);

/**
 * wlan_connectivity_log_enqueue() - Add new record to the logging buffer
 * @new_record: Pointer to the new record to be added
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_connectivity_log_enqueue(struct wlan_log_record *new_record);

/**
 * wlan_connectivity_mgmt_event()  - Fill and enqueue a new record
 * for management frame information.
 * @psoc: Pointer to global psoc object
 * @mac_hdr: 802.11 management frame header
 * @vdev_id: Vdev id
 * @status_code: Frame status code as defined in IEEE 802.11 - 2020 standard
 * section 9.4.1.9
 * @tx_status: Frame TX status defined by enum qdf_dp_tx_rx_status
 * @peer_rssi: Peer RSSI in dBm
 * @auth_algo: Authentication algorithm number field as defined in IEEE 802.11 -
 * 2020 standard section 9.4.1.1
 * @auth_type: indicates SAE authentication frame type. Possible values are:
 * 1 - SAE commit frame
 * 2 - SAE confirm frame
 * @auth_seq: Authentication frame transaction sequence number as defined in
 * IEEE 802.11 - 2020 standard section 9.4.1.2
 * @aid: Association ID
 * @tag: Record type main tag
 *
 * Return: QDF_STATUS
 */
void
wlan_connectivity_mgmt_event(struct wlan_objmgr_psoc *psoc,
			     struct wlan_frame_hdr *mac_hdr,
			     uint8_t vdev_id, uint16_t status_code,
			     enum qdf_dp_tx_rx_status tx_status,
			     int8_t peer_rssi,
			     uint8_t auth_algo, uint8_t auth_type,
			     uint8_t auth_seq, uint16_t aid,
			     enum wlan_main_tag tag);

/**
 * wlan_connectivity_connecting_event() - API to log connecting event
 * @vdev: vdev pointer
 * @con_req: Connection request parameter
 *
 * Return: None
 */
void
wlan_connectivity_connecting_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_connect_req *con_req);

/**
 * wlan_populate_vsie() - Populate VSIE field for logging
 * @vdev: vdev pointer
 * @data: Diag packet info data
 * @is_tx: Flag to indicate whether the packet is transmitted or received
 *
 * Return: None
 */
void
wlan_populate_vsie(struct wlan_objmgr_vdev *vdev,
		   struct wlan_diag_packet_info *data, bool is_tx);

/**
 * wlan_connectivity_sta_info_event() - APi to send STA info event
 * @psoc: Pointer to global psoc object
 * @vdev_id: Vdev id
 * @is_roam: Is sta info event for roaming stats
 */
void
wlan_connectivity_sta_info_event(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, bool is_roam);

/**
 * wlan_convert_freq_to_diag_band() - API to convert frequency to band value
 * mentioned in enum wlan_diag_wifi_band
 * @ch_freq: Frequency(in MHz)
 *
 * Return: Band specified in enum wlan_diag_wifi_band
 */
enum wlan_diag_wifi_band
wlan_convert_freq_to_diag_band(uint16_t ch_freq);

/**
 * wlan_populate_vsie() - Populate VSIE field for logging
 * @vdev: vdev pointer
 * @data: Diag packet info data
 * @is_tx: flag to indicate whether packet transmitted or received
 *
 * Return: None
 */
void
wlan_populate_vsie(struct wlan_objmgr_vdev *vdev,
		   struct wlan_diag_packet_info *data, bool is_tx);

/**
 * wlan_cdp_set_peer_freq() - API to set frequency to dp peer
 * @psoc: psoc pointer
 * @peer_mac: Bssid of peer
 * @freq: frequency(in MHz)
 * @vdev_id: vdev id
 *
 * Return: None
 */
void
wlan_cdp_set_peer_freq(struct wlan_objmgr_psoc *psoc, uint8_t *peer_mac,
		       uint32_t freq, uint8_t vdev_id);

#else
static inline
void wlan_connectivity_logging_start(struct wlan_objmgr_psoc *psoc,
				     struct wlan_cl_osif_cbks *osif_cbks,
				     void *osif_cb_context)
{}

static inline void wlan_connectivity_logging_stop(void)
{}

static inline QDF_STATUS wlan_connectivity_log_dequeue(void)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS wlan_connectivity_log_enqueue(struct wlan_log_record *new_record)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
wlan_connectivity_mgmt_event(struct wlan_objmgr_psoc *psoc,
			     struct wlan_frame_hdr *mac_hdr,
			     uint8_t vdev_id, uint16_t status_code,
			     enum qdf_dp_tx_rx_status tx_status,
			     int8_t peer_rssi,
			     uint8_t auth_algo, uint8_t auth_type,
			     uint8_t auth_seq, uint16_t aid,
			     enum wlan_main_tag tag)
{}

static inline void
wlan_populate_vsie(struct wlan_objmgr_vdev *vdev,
		   struct wlan_diag_packet_info *data, bool is_tx)
{
}

static inline enum wlan_diag_wifi_band
wlan_convert_freq_to_diag_band(uint16_t ch_freq)
{
	return WLAN_INVALID_BAND;
}

static inline QDF_STATUS
wlan_populate_mlo_mgmt_event_param(struct wlan_objmgr_vdev *vdev,
				   struct wlan_diag_packet_info *data,
				   enum wlan_main_tag tag)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
wlan_cdp_set_peer_freq(struct wlan_objmgr_psoc *psoc, uint8_t *peer_mac,
		       uint32_t freq, uint8_t vdev_id)
{}

static inline void
wlan_connectivity_mlo_reconfig_event(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_connectivity_sta_info_event(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, bool is_roam)
{}

static inline void
wlan_connectivity_t2lm_req_resp_event(struct wlan_objmgr_vdev *vdev,
				      uint8_t token,
				      enum wlan_t2lm_resp_frm_type status,
				      enum qdf_dp_tx_rx_status tx_status,
				      qdf_freq_t freq,
				      bool is_rx, uint8_t subtype)
{}

static inline void
wlan_connectivity_t2lm_status_event(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_connectivity_connecting_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_connect_req *con_req)
{
}
#endif

#if defined(CONNECTIVITY_DIAG_EVENT) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * wlan_connectivity_mld_link_status_event() - Send connectivity logging
 * ML Link Status event
 * @psoc: Pointer to global PSOC object
 * @src: Src parameters to be sent
 *
 * Return: None
 */
void
wlan_connectivity_mld_link_status_event(struct wlan_objmgr_psoc *psoc,
					struct mlo_link_switch_params *src);
#else
static inline
void wlan_connectivity_mld_link_status_event(struct wlan_objmgr_psoc *psoc,
					     struct mlo_link_switch_params *src)
{}
#endif /* CONNECTIVITY_DIAG_EVENT && WLAN_FEATURE_11BE_MLO */
#endif /* _WLAN_CONNECTIVITY_LOGGING_H_ */

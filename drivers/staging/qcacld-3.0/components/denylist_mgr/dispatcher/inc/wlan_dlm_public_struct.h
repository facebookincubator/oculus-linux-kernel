/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
/**
 * DOC: define public structures of denylist mgr.
 */

#ifndef _WLAN_DLM_PUBLIC_STRUCT_H
#define _WLAN_DLM_PUBLIC_STRUCT_H

#include <qdf_types.h>
#include "wlan_objmgr_pdev_obj.h"

#define MAX_BAD_AP_LIST_SIZE               28
#define MAX_RSSI_AVOID_BSSID_LIST          10
#define PDEV_MAX_NUM_BSSID_DISALLOW_LIST   28

/**
 * enum dlm_reject_ap_source - Source of adding BSSID to DLM
 * @ADDED_BY_DRIVER: Source adding this BSSID is driver
 * @ADDED_BY_TARGET: Source adding this BSSID is target
 */
enum dlm_reject_ap_source {
	ADDED_BY_DRIVER = 1,
	ADDED_BY_TARGET,
};

/**
 * struct dlm_rssi_disallow_params - structure to specify params for RSSI
 * reject
 * @retry_delay: Time before which the AP doesn't expect a connection.
 * @expected_rssi: RSSI less than which only the STA should try association
 * @received_time: Time at which the AP was added to denylist.
 * @original_timeout: Original timeout which the AP sent while denylisting.
 * @source: Source of adding this BSSID to RSSI reject list
 */
struct dlm_rssi_disallow_params {
	uint32_t retry_delay;
	int8_t expected_rssi;
	qdf_time_t received_time;
	uint32_t original_timeout;
	enum dlm_reject_ap_source source;
};

/**
 * enum dlm_reject_ap_type - Rejection type of the AP
 * @USERSPACE_AVOID_TYPE: userspace wants the AP to be avoided.
 * @USERSPACE_DENYLIST_TYPE: userspace wants the AP to be denylisted.
 * @DRIVER_AVOID_TYPE: driver wants the AP to be avoided.
 * @DRIVER_DENYLIST_TYPE: driver wants the AP to be denylisted.
 * @DRIVER_RSSI_REJECT_TYPE: driver wants the AP to be in driver rssi
 * reject.
 * @DRIVER_MONITOR_TYPE: driver wants the AP to be in monitor list.
 * @REJECT_REASON_UNKNOWN: Rejection reason unknown
 */
enum dlm_reject_ap_type {
	USERSPACE_AVOID_TYPE     = 0,
	USERSPACE_DENYLIST_TYPE  = 1,
	DRIVER_AVOID_TYPE        = 2,
	DRIVER_DENYLIST_TYPE     = 3,
	DRIVER_RSSI_REJECT_TYPE  = 4,
	DRIVER_MONITOR_TYPE      = 5,
	REJECT_REASON_UNKNOWN    = 6,
};

/**
 * enum dlm_reject_ap_reason - Rejection reason for adding BSSID to DLM
 * @REASON_UNKNOWN: Unknown reason
 * @REASON_NUD_FAILURE: NUD failure happened with this BSSID
 * @REASON_STA_KICKOUT: STA kickout happened with this BSSID
 * @REASON_ROAM_HO_FAILURE: HO failure happenend with this BSSID
 * @REASON_ASSOC_REJECT_POOR_RSSI: assoc rsp with reason 71 received from
 * AP.
 * @REASON_ASSOC_REJECT_OCE: OCE assoc reject received from the AP.
 * @REASON_USERSPACE_BL: Userspace wants to denylist this AP.
 * @REASON_USERSPACE_AVOID_LIST: Userspace wants to avoid this AP.
 * @REASON_BTM_DISASSOC_IMMINENT: BTM IE received with disassoc imminent
 * set.
 * @REASON_BTM_BSS_TERMINATION: BTM IE received with BSS termination set.
 * @REASON_BTM_MBO_RETRY: BTM IE received from AP with MBO retry set.
 * @REASON_REASSOC_RSSI_REJECT: Re-Assoc resp received with reason code 34
 * @REASON_REASSOC_NO_MORE_STAS: Re-assoc reject received with reason code
 * 17
 */
enum dlm_reject_ap_reason {
	REASON_UNKNOWN = 0,
	REASON_NUD_FAILURE,
	REASON_STA_KICKOUT,
	REASON_ROAM_HO_FAILURE,
	REASON_ASSOC_REJECT_POOR_RSSI,
	REASON_ASSOC_REJECT_OCE,
	REASON_USERSPACE_BL,
	REASON_USERSPACE_AVOID_LIST,
	REASON_BTM_DISASSOC_IMMINENT,
	REASON_BTM_BSS_TERMINATION,
	REASON_BTM_MBO_RETRY,
	REASON_REASSOC_RSSI_REJECT,
	REASON_REASSOC_NO_MORE_STAS,
};

/**
 * enum dlm_connection_state - State with AP (Connected, Disconnected)
 * @DLM_AP_CONNECTED: Connected with the AP
 * @DLM_AP_DISCONNECTED: Disconnected with the AP
 */
enum dlm_connection_state {
	DLM_AP_CONNECTED,
	DLM_AP_DISCONNECTED,
};

/**
 * struct reject_ap_config_params - Structure to send reject ap list to FW
 * @bssid: BSSID of the AP
 * @reject_ap_type: Type of the rejection done with the BSSID
 * @reject_duration: time left till the AP is in the reject list.
 * @expected_rssi: expected RSSI when the AP expects the connection to be
 * made.
 * @reject_reason: reason to add the BSSID to DLM
 * @source: Source of adding the BSSID to DLM
 * @received_time: Time at which the AP was added to denylist.
 * @original_timeout: Original timeout which the AP sent while denylisting.
 */
struct reject_ap_config_params {
	struct qdf_mac_addr bssid;
	enum dlm_reject_ap_type reject_ap_type;
	uint32_t reject_duration;
	int32_t expected_rssi;
	enum dlm_reject_ap_reason reject_reason;
	enum dlm_reject_ap_source source;
	qdf_time_t received_time;
	uint32_t original_timeout;
};

/**
 * struct reject_ap_params - Struct to send bssid list and there num to FW
 * @num_of_reject_bssid: num of bssid params there in bssid config.
 * @bssid_list: Pointer to the bad bssid list
 */
struct reject_ap_params {
	uint8_t num_of_reject_bssid;
	struct reject_ap_config_params *bssid_list;
};

/**
 * struct wlan_dlm_tx_ops - structure of tx operation function
 * pointers for denylist manager component
 * @dlm_send_reject_ap_list: send reject ap list to fw
 */
struct wlan_dlm_tx_ops {
	QDF_STATUS (*dlm_send_reject_ap_list)(struct wlan_objmgr_pdev *pdev,
					struct reject_ap_params *reject_params);
};

/**
 * struct reject_ap_info - structure to specify the reject ap info.
 * @bssid: BSSID of the AP.
 * @rssi_reject_params: RSSI reject params of the AP is of type RSSI reject
 * @reject_ap_type: Reject type of AP (eg. avoid, denylist, rssi reject
 * etc.)
 * @reject_reason: reason to add the BSSID to DLM
 * @source: Source of adding the BSSID to DLM
 */
struct reject_ap_info {
	struct qdf_mac_addr bssid;
	struct dlm_rssi_disallow_params rssi_reject_params;
	enum dlm_reject_ap_type reject_ap_type;
	enum dlm_reject_ap_reason reject_reason;
	enum dlm_reject_ap_source source;
};

#endif

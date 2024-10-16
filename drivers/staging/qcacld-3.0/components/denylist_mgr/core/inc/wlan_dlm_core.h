/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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
 * DOC: declare internal APIs related to the denylist component
 */

#ifndef _WLAN_DLM_CORE_H_
#define _WLAN_DLM_CORE_H_

#include <wlan_dlm_main.h>

#define DLM_IS_AP_AVOIDED_BY_USERSPACE(cur_node) \
			(cur_node)->userspace_avoidlist

#define DLM_IS_AP_AVOIDED_BY_DRIVER(cur_node) \
		(cur_node)->driver_avoidlist

#define DLM_IS_AP_DENYLISTED_BY_USERSPACE(cur_node) \
		(cur_node)->userspace_denylist

#define DLM_IS_AP_DENYLISTED_BY_DRIVER(cur_node) \
		(cur_node)->driver_denylist

#define DLM_IS_AP_IN_MONITOR_LIST(cur_node) \
		(cur_node)->driver_monitorlist

#define DLM_IS_AP_IN_RSSI_REJECT_LIST(cur_node) \
		(cur_node)->rssi_reject_list

#define DLM_IS_AP_IN_DENYLIST(cur_node) \
		(DLM_IS_AP_DENYLISTED_BY_USERSPACE(cur_node) | \
		 DLM_IS_AP_DENYLISTED_BY_DRIVER(cur_node) | \
		 DLM_IS_AP_IN_RSSI_REJECT_LIST(cur_node))

#define DLM_IS_AP_IN_AVOIDLIST(cur_node) \
		(DLM_IS_AP_AVOIDED_BY_USERSPACE(cur_node) | \
		 DLM_IS_AP_AVOIDED_BY_DRIVER(cur_node))

#define IS_AP_IN_USERSPACE_DENYLIST_ONLY(cur_node) \
		(DLM_IS_AP_DENYLISTED_BY_USERSPACE(cur_node) & \
		!(DLM_IS_AP_IN_AVOIDLIST(cur_node) | \
		 DLM_IS_AP_IN_MONITOR_LIST(cur_node) | \
		 DLM_IS_AP_IN_RSSI_REJECT_LIST(cur_node) | \
		 DLM_IS_AP_DENYLISTED_BY_DRIVER(cur_node)))

#define IS_AP_IN_MONITOR_LIST_ONLY(cur_node) \
		(DLM_IS_AP_IN_MONITOR_LIST(cur_node) & \
		!(DLM_IS_AP_IN_AVOIDLIST(cur_node) | \
		 DLM_IS_AP_IN_DENYLIST(cur_node)))

#define IS_AP_IN_AVOID_LIST_ONLY(cur_node) \
		(DLM_IS_AP_IN_AVOIDLIST(cur_node) & \
		!(DLM_IS_AP_IN_MONITOR_LIST(cur_node) | \
		 DLM_IS_AP_IN_DENYLIST(cur_node)))

#define IS_AP_IN_DRIVER_DENYLIST_ONLY(cur_node) \
		(DLM_IS_AP_DENYLISTED_BY_DRIVER(cur_node) & \
		!(DLM_IS_AP_IN_AVOIDLIST(cur_node) | \
		 DLM_IS_AP_IN_MONITOR_LIST(cur_node) | \
		 DLM_IS_AP_IN_RSSI_REJECT_LIST(cur_node) | \
		 DLM_IS_AP_DENYLISTED_BY_USERSPACE(cur_node)))

#define IS_AP_IN_RSSI_REJECT_LIST_ONLY(cur_node) \
		(DLM_IS_AP_IN_RSSI_REJECT_LIST(cur_node) & \
		!(DLM_IS_AP_IN_AVOIDLIST(cur_node) | \
		 DLM_IS_AP_IN_MONITOR_LIST(cur_node) | \
		 DLM_IS_AP_DENYLISTED_BY_DRIVER(cur_node) | \
		 DLM_IS_AP_DENYLISTED_BY_USERSPACE(cur_node)))

#define IS_AP_IN_USERSPACE_AVOID_LIST_ONLY(cur_node) \
		(DLM_IS_AP_AVOIDED_BY_USERSPACE(cur_node) & \
		!(DLM_IS_AP_AVOIDED_BY_DRIVER(cur_node) | \
		 DLM_IS_AP_IN_MONITOR_LIST(cur_node) | \
		 DLM_IS_AP_IN_DENYLIST(cur_node)))

#define IS_AP_IN_DRIVER_AVOID_LIST_ONLY(cur_node) \
		(DLM_IS_AP_AVOIDED_BY_DRIVER(cur_node) & \
		!(DLM_IS_AP_AVOIDED_BY_USERSPACE(cur_node) | \
		 DLM_IS_AP_IN_MONITOR_LIST(cur_node) | \
		 DLM_IS_AP_IN_DENYLIST(cur_node)))

/**
 * struct dlm_reject_ap_timestamp - Structure to store the reject list BSSIDs
 * entry time stamp.
 * @userspace_avoid_timestamp: Time when userspace adds BSSID to avoid list.
 * @driver_avoid_timestamp: Time when driver adds BSSID to avoid list.
 * @userspace_denylist_timestamp: Time when userspace adds BSSID to deny list.
 * @driver_denylist_timestamp: Time when driver adds BSSID to deny list.
 * @rssi_reject_timestamp: Time when driver adds BSSID to rssi reject list.
 * @driver_monitor_timestamp: Time when driver adds BSSID to monitor list.
 */
struct dlm_reject_ap_timestamp {
	qdf_time_t userspace_avoid_timestamp;
	qdf_time_t driver_avoid_timestamp;
	qdf_time_t userspace_denylist_timestamp;
	qdf_time_t driver_denylist_timestamp;
	qdf_time_t rssi_reject_timestamp;
	qdf_time_t driver_monitor_timestamp;
};

/**
 * struct dlm_reject_ap - Structure of a node added to denylist manager
 * @node: Node of the entry
 * @bssid: Bssid of the AP entry.
 * @rssi_reject_params: Rssi reject params of the AP entry.
 * @bad_bssid_counter: It represent how many times data stall happened.
 * @ap_timestamp: AP timestamp.
 * @reject_ap_type: consolidated bitmap of rejection types for the AP
 * @userspace_denylist: AP in userspace denylist
 * @driver_denylist: AP in driver denylist
 * @userspace_avoidlist: AP in userspace avoidlist
 * @driver_avoidlist: AP in driver avoidlist
 * @rssi_reject_list: AP has bad RSSI
 * @driver_monitorlist: AP is monitored
 * @reject_ap_reason: consolidated bitmap of rejection reasons for the AP
 * @nud_fail: NUD fail reason
 * @sta_kickout: STA kickout reason
 * @ho_fail: Handoff failure reason
 * @poor_rssi: Poor RSSI reason
 * @oce_assoc_reject: OCE association rejected reason
 * @denylist_userspace: Userspace denylist reason
 * @avoid_userspace: Userspace avoidlist reason
 * @btm_disassoc_imminent: BTM disassociation imminent reason
 * @btm_bss_termination: BTM BSS termination reason
 * @btm_mbo_retry: BTM MBO retry reason
 * @reassoc_rssi_reject: Reassociation RSSI rejection reason
 * @no_more_stas: AP reached STA capacity reason
 * @source: source of the rejection
 * @connect_timestamp: Timestamp when the STA got connected with this BSSID
 */
struct dlm_reject_ap {
	qdf_list_node_t node;
	struct qdf_mac_addr bssid;
	struct dlm_rssi_disallow_params rssi_reject_params;
	uint8_t bad_bssid_counter;
	struct dlm_reject_ap_timestamp ap_timestamp;
	union {
		struct {
			uint8_t userspace_denylist:1,
				driver_denylist:1,
				userspace_avoidlist:1,
				driver_avoidlist:1,
				rssi_reject_list:1,
				driver_monitorlist:1;
		};
		uint8_t reject_ap_type;
	};
	union {
		struct {
			uint32_t nud_fail:1,
				 sta_kickout:1,
				 ho_fail:1,
				 poor_rssi:1,
				 oce_assoc_reject:1,
				 denylist_userspace:1,
				 avoid_userspace:1,
				 btm_disassoc_imminent:1,
				 btm_bss_termination:1,
				 btm_mbo_retry:1,
				 reassoc_rssi_reject:1,
				 no_more_stas:1;
		};
		uint32_t reject_ap_reason;
	};
	enum dlm_reject_ap_source source;
	qdf_time_t connect_timestamp;
};

/**
 * dlm_add_bssid_to_reject_list() - Add BSSID to the specific reject list.
 * @pdev: Pdev object
 * @ap_info: Ap info params such as BSSID, and the type of rejection to be done
 *
 * This API will add the BSSID to the reject AP list maintained by the denylist
 * manager.
 *
 * Return: QDF status
 */
QDF_STATUS
dlm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
			     struct reject_ap_info *ap_info);

#if defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * dlm_send_reject_ap_list_to_fw() - Send the denylist BSSIDs to FW
 * @pdev: Pdev object
 * @reject_db_list: List of denylist BSSIDs
 * @cfg: Denylist manager cfg
 *
 * This API will send the denylist BSSIDs to FW for avoiding or denylisting
 * in roaming scenarios.
 *
 * Return: None
 */
void
dlm_send_reject_ap_list_to_fw(struct wlan_objmgr_pdev *pdev,
			      qdf_list_t *reject_db_list,
			      struct dlm_config *cfg);

/**
 * dlm_update_reject_ap_list_to_fw() - Send the denylist BSSIDs to FW
 * @psoc: psoc object
 *
 * This API will send the denylist BSSIDs to FW.
 *
 * Return: None
 */
void dlm_update_reject_ap_list_to_fw(struct wlan_objmgr_psoc *psoc);
#else
static inline void dlm_send_reject_ap_list_to_fw(struct wlan_objmgr_pdev *pdev,
						 qdf_list_t *reject_db_list,
						 struct dlm_config *cfg)
{
}

static inline void
dlm_update_reject_ap_list_to_fw(struct wlan_objmgr_psoc *psoc)
{
}
#endif

/**
 * dlm_add_userspace_deny_list() - Clear already existing userspace BSSID, and
 * add the new ones to denylist manager.
 * @pdev: pdev object
 * @bssid_deny_list: BSSIDs to be denylisted by userspace.
 * @num_of_bssid: num of bssids to be denylisted.
 *
 * This API will Clear already existing userspace BSSID, and add the new ones
 * to denylist manager's reject list.
 *
 * Return: QDF status
 */
QDF_STATUS
dlm_add_userspace_deny_list(struct wlan_objmgr_pdev *pdev,
			    struct qdf_mac_addr *bssid_deny_list,
			    uint8_t num_of_bssid);

/**
 * dlm_update_bssid_connect_params() - Inform the DLM about connect/disconnect
 * with the current AP.
 * @pdev: pdev object
 * @bssid: BSSID of the AP
 * @con_state: Connection state (connected/disconnected)
 *
 * This API will inform the DLM about the state with the AP so that if the AP
 * is selected, and the connection went through, and the connection did not
 * face any data stall till the bad bssid reset timer, DLM can remove the
 * AP from the reject ap list maintained by it.
 *
 * Return: None
 */
void
dlm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				struct qdf_mac_addr bssid,
				enum dlm_connection_state con_state);

/**
 * dlm_flush_reject_ap_list() - Clear away BSSID and destroy the reject ap list
 * @dlm_ctx: denylist manager pdev priv object
 *
 * This API will clear the BSSID info in the reject AP list maintained by the
 * denylist manager, and will destroy the list as well.
 *
 * Return: None
 */
void
dlm_flush_reject_ap_list(struct dlm_pdev_priv_obj *dlm_ctx);

/**
 * dlm_get_bssid_reject_list() - Get the BSSIDs in reject list from DLM
 * @pdev: pdev object
 * @reject_list: reject list to be filled (passed by caller)
 * @max_bssid_to_be_filled: num of bssids filled in reject list by DLM
 * @reject_ap_type: reject ap type of the BSSIDs to be filled.
 *
 * This API will fill the reject ap list requested by caller of type given as
 * argument reject_ap_type, and will return the number of BSSIDs filled.
 *
 * Return: Unsigned integer (number of BSSIDs filled by the denylist manager)
 */
uint8_t
dlm_get_bssid_reject_list(struct wlan_objmgr_pdev *pdev,
			  struct reject_ap_config_params *reject_list,
			  uint8_t max_bssid_to_be_filled,
			  enum dlm_reject_ap_type reject_ap_type);

/**
 * dlm_dump_denylist_bssid - Dump denylisted bssids
 * @pdev: pdev object
 *
 * Return: None
 */
void dlm_dump_denylist_bssid(struct wlan_objmgr_pdev *pdev);

/**
 * dlm_is_bssid_in_reject_list - Check whether a BSSID is present in
 * reject list or not.
 * @pdev: pdev object
 * @bssid: bssid to check
 *
 * Return: true if BSSID is present in reject list
 */
bool dlm_is_bssid_in_reject_list(struct wlan_objmgr_pdev *pdev,
				 struct qdf_mac_addr *bssid);

/**
 * dlm_get_rssi_denylist_threshold() - Get rssi denylist threshold value
 * @pdev: pdev object
 *
 * This API will get the RSSI denylist threshold info.
 *
 * Return: rssi threshold value
 */
int32_t
dlm_get_rssi_denylist_threshold(struct wlan_objmgr_pdev *pdev);
#endif

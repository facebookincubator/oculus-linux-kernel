/*
 * Copyright (c) 2018-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: Define VDEV MLME public APIs
 */

#ifndef _WLAN_VDEV_MLME_API_H_
#define _WLAN_VDEV_MLME_API_H_

#include <wlan_ext_mlme_obj_types.h>

#define WLAN_INVALID_VDEV_ID 255
#define WILDCARD_PDEV_ID 0x0

#ifdef WLAN_FEATURE_SR
#define PSR_DISALLOWED 1
#define NON_SRG_SPR_ENABLE_SIZE 1
#define SR_PARAM_VAL_DBM_UNIT 1
#define SR_PARAM_VAL_DBM_POS 29
#define NON_SRG_PARAM_VAL_DBM_SIZE 1
#define NON_SRG_MAX_PD_OFFSET_POS 0
#define NON_SRG_MAX_PD_OFFSET_SIZE 8
#define SR_PD_THRESHOLD_MIN -82
#define SRG_SPR_ENABLE_POS 30
#define SRG_THRESHOLD_MAX_PD_POS 8
#define NON_SRG_PD_SR_DISALLOWED 0x02
#define HE_SIG_VAL_15_ALLOWED 0x10
#define NON_SRG_OFFSET_PRESENT 0x04
#define SRG_INFO_PRESENT 0x08
#define NON_SRG_SPR_ENABLE_POS 31
#define NON_SRG_SPR_ENABLE 0x80
#define NON_SR_PD_THRESHOLD_DISABLED 0x80
#define SR_PADDING_BYTE 8
#endif


/**
 * enum wlan_mlme_peer_param_id - peer param id in mlme layer
 * @WLAN_MLME_PEER_BW_PUNCTURE: update puncture 20 MHz bitmap
 * @WLAN_MLME_PEER_MAX: max enumeration
 */
enum wlan_mlme_peer_param_id {
	WLAN_MLME_PEER_BW_PUNCTURE,
	WLAN_MLME_PEER_MAX
};

/**
 * wlan_vdev_mlme_get_cmpt_obj - Retrieves MLME component object
 * from VDEV object
 * @vdev: pointer to vdev object
 *
 * Retrieves MLME component object from VDEV object
 *
 * Return: comp handle on SUCCESS
 *         NULL, if it fails to retrieve
 */
struct vdev_mlme_obj *wlan_vdev_mlme_get_cmpt_obj(
						struct wlan_objmgr_vdev *vdev);
/**
 * wlan_vdev_mlme_set_ext_hdl - Sets legacy handle
 * @vdev: pointer to vdev object
 * @ext_hdl: pointer to legacy handle
 *
 * Sets Legacy handle to MLME component object
 *
 * Return:
 */
void wlan_vdev_mlme_set_ext_hdl(struct wlan_objmgr_vdev *vdev,
				mlme_vdev_ext_t *ext_hdl);

/**
 * wlan_vdev_mlme_get_ext_hdl - Returns legacy handle
 * @vdev: pointer to vdev object
 *
 * Retrieves legacy handle from vdev mlme component object
 *
 * Return: legacy handle on SUCCESS
 *         NULL, if it fails to retrieve
 */
mlme_vdev_ext_t *wlan_vdev_mlme_get_ext_hdl(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_sm_deliver_evt() - Delivers event to VDEV MLME SM
 * @vdev: Object manager VDEV object
 * @event: MLME event
 * @event_data_len: data size
 * @event_data: event data
 *
 * API to dispatch event to VDEV MLME SM with lock acquired
 *
 * Return: SUCCESS: on handling event
 *         FAILURE: on ignoring the event
 */
QDF_STATUS wlan_vdev_mlme_sm_deliver_evt(struct wlan_objmgr_vdev *vdev,
					 enum wlan_vdev_sm_evt event,
					 uint16_t event_data_len,
					 void *event_data);

/**
 * wlan_vdev_mlme_sm_deliver_evt_sync() - Delivers event to VDEV MLME SM sync
 * @vdev: Object manager VDEV object
 * @event: MLME event
 * @event_data_len: data size
 * @event_data: event data
 *
 * API to dispatch event to VDEV MLME SM with lock acquired
 *
 * Return: SUCCESS: on handling event
 *         FAILURE: on ignoring the event
 */
QDF_STATUS wlan_vdev_mlme_sm_deliver_evt_sync(struct wlan_objmgr_vdev *vdev,
					      enum wlan_vdev_sm_evt event,
					      uint16_t event_data_len,
					      void *event_data);

#ifdef SM_ENG_HIST_ENABLE
/**
 * wlan_vdev_mlme_sm_history_print() - Prints SM history
 * @vdev: Object manager VDEV object
 *
 * API to print SM history
 *
 * Return: void
 */
void wlan_vdev_mlme_sm_history_print(struct wlan_objmgr_vdev *vdev);

#endif

/**
 * wlan_vdev_allow_connect_n_tx() - Checks whether VDEV is in operational state
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state to allow tx or connections
 *
 * Return: SUCCESS: to allow tx or connection
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_allow_connect_n_tx(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_is_active() - Checks whether VDEV is in active state
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state to check channel is configured in FW
 *
 * Return: SUCCESS: valid channel is configured
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_mlme_is_active(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_chan_config_valid() - Checks whether VDEV chan config valid
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state to check channel is configured in Host
 *
 * Return: SUCCESS: valid channel is configured
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_chan_config_valid(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_is_csa_restart() - Checks whether VDEV MLME SM is in CSA
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state for CSA_RESTART substate
 *
 * Return: SUCCESS: if it is in CSA_RESTART sub state
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_mlme_is_csa_restart(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_is_going_down() - Checks whether VDEV is being brought down
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state to check VDEV is being brought down
 *
 * Return: SUCCESS: valid channel is configured
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_is_going_down(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_is_mlo_peer_create_allowed() - Checks whether PEER can be created
 * @vdev: Object manager VDEV object
 *
 * API to check the VDEV MLME SM state to allow PEER association in MLD
 *
 * Return: SUCCESS: if peer create can be allowed
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_is_mlo_peer_create_allowed(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_is_restart_progress() - Checks VDEV restart is in progress
 * @vdev: Object manager VDEV object
 *
 * API to check whether restarts is in progress
 *
 * Return: SUCCESS: if restart is in progress
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_is_restart_progress(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_is_dfs_cac_wait() - Checks VDEV is in cac wait state
 * @vdev: Object manager VDEV object
 *
 * API to check whether state is cac wait state
 *
 * Return: SUCCESS: if state is cac wait state
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_is_dfs_cac_wait(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_cmd_lock - Acquire lock for command queuing atomicity
 * @vdev: Object manager VDEV object
 *
 * API to take VDEV MLME command lock
 *
 * Return: void
 */
void wlan_vdev_mlme_cmd_lock(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_cmd_unlock - Release lock for command queuing atomicity
 * @vdev: Object manager VDEV object
 *
 * API to release VDEV MLME command lock
 *
 * Return: void
 */
void wlan_vdev_mlme_cmd_unlock(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_is_scan_allowed() - Checks whether scan is allowed
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state to check scan is allowed
 *
 * Return: SUCCESS: if scan is allowed
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_mlme_is_scan_allowed(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_is_init_state() - Checks whether vdev is in init state
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state is in init state or not
 *
 * Return: SUCCESS: if vdev is in init state
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_mlme_is_init_state(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_is_up_active_state() - Checks whether vdev is in up active state
 * @vdev: Object manager VDEV object
 *
 * API to checks the VDEV MLME SM state is in UP ACTIVE state
 *
 * Return: SUCCESS: if vdev is in UP ACTIVE state
 *         FAILURE: otherwise failure
 */
QDF_STATUS wlan_vdev_is_up_active_state(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_vdev_mlme_get_is_mlo_link() - check if its mlo link vdev
 * @psoc: PSOC object
 * @vdev_id: VDEV Id
 *
 * Return: True if it is mlo link, otherwise false.
 */
bool
wlan_vdev_mlme_get_is_mlo_link(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id);

/**
 * wlan_vdev_mlme_get_is_mlo_vdev() - check if its mlo assoc vdev
 * @psoc: PSOC object
 * @vdev_id: VDEV Id
 *
 * Return: True if it is mlo link, otherwise false.
 */
bool
wlan_vdev_mlme_get_is_mlo_vdev(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id);
#else
static inline bool
wlan_vdev_mlme_get_is_mlo_link(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	return false;
}

static inline bool
wlan_vdev_mlme_get_is_mlo_vdev(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	return false;
}
#endif
#ifdef WLAN_FEATURE_SR
/**
 * wlan_mlme_update_sr_data() - Updates SR values
 * @vdev: Object manager VDEV object
 * @val: SR value
 * @srg_pd_threshold: SRG PD threshold sent by userspace
 * @non_srg_pd_threshold: NON SRG PD threshold sent by userspace
 * @is_sr_enable: SR enable/disable from userspace
 *
 * API to Update SR value based on AP advertisement and provided by userspace
 *
 * Return: true/flase
 */
void
wlan_mlme_update_sr_data(struct wlan_objmgr_vdev *vdev, int *val,
			 int32_t srg_pd_threshold, int32_t non_srg_pd_threshold,
			 bool is_sr_enable);
#else
static inline void
wlan_mlme_update_sr_data(struct wlan_objmgr_vdev *vdev, int *val,
			 int32_t srg_pd_threshold, int32_t non_srg_pd_threshold,
			 bool is_sr_enable)
{}
#endif
#endif

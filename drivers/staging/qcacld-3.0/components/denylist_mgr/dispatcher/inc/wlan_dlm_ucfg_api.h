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
 * DOC: declare UCFG APIs exposed by the denylist manager component
 */

#ifndef _WLAN_DLM_UCFG_H_
#define _WLAN_DLM_UCFG_H_

#include "qdf_types.h"
#include "wlan_objmgr_psoc_obj.h"
#include <wlan_dlm_public_struct.h>

#ifdef FEATURE_DENYLIST_MGR

/**
 * ucfg_dlm_init() - initialize denylist mgr context
 *
 * This function initializes the denylist mgr context
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_dlm_init(void);

/**
 * ucfg_dlm_deinit() - De initialize denylist mgr context
 *
 * This function De initializes denylist mgr context
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_dlm_deinit(void);

/**
 * ucfg_dlm_psoc_set_suspended() - API to set denylist mgr state suspended
 * @psoc: pointer to psoc object
 * @state: state to be set
 *
 * This function sets denylist mgr state to suspended
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_dlm_psoc_set_suspended(struct wlan_objmgr_psoc *psoc,
				       bool state);

/**
 * ucfg_dlm_psoc_get_suspended() - API to get denylist mgr state suspended
 * @psoc: pointer to psoc object
 * @state: pointer to get suspend state of denylist manager
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_dlm_psoc_get_suspended(struct wlan_objmgr_psoc *psoc,
				       bool *state);

/**
 * ucfg_dlm_psoc_open() - API to initialize the cfg when psoc is initialized.
 * @psoc: psoc object
 *
 * This function initializes the config of denylist mgr.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_dlm_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dlm_psoc_close() - API to deinit the dlm when psoc is deinitialized.
 * @psoc: psoc object
 *
 * This function deinits the dlm psoc object.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_dlm_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dlm_add_userspace_deny_list() - Clear already existing userspace BSSID,
 * and add the new ones to denylist manager.
 * @pdev: pdev object
 * @bssid_deny_list: BSSIDs to be denylisted by userspace.
 * @num_of_bssid: num of bssids to be denylisted.
 *
 * This API clear already existing userspace BSSID, and add the new ones to
 * denylist manager
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error.
 */
QDF_STATUS
ucfg_dlm_add_userspace_deny_list(struct wlan_objmgr_pdev *pdev,
				 struct qdf_mac_addr *bssid_deny_list,
				 uint8_t num_of_bssid);

/**
 * ucfg_dlm_dump_deny_list_ap() - get denylisted bssid.
 * @pdev: pdev object
 *
 * This API dumps denylist ap
 *
 * Return: None
 */
void ucfg_dlm_dump_deny_list_ap(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_dlm_update_bssid_connect_params() - Inform the DLM about connect or
 * disconnect with the current AP.
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
ucfg_dlm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum dlm_connection_state con_state);

/**
 * ucfg_dlm_add_bssid_to_reject_list() - Add BSSID to the specific reject list.
 * @pdev: Pdev object
 * @ap_info: Ap info params such as BSSID, and the type of rejection to be done
 *
 * This API will add the BSSID to the reject AP list maintained by the denylist
 * manager.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error.
 */
QDF_STATUS
ucfg_dlm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info);

/**
 * ucfg_dlm_wifi_off() - Inform the denylist manager about wifi off
 * @pdev: Pdev object
 *
 * This API will inform the denylist manager that the user has turned wifi off
 * from the UI, and the denylist manager can take action based upon this.
 *
 * Return: None
 */
void
ucfg_dlm_wifi_off(struct wlan_objmgr_pdev *pdev);

#else
static inline
QDF_STATUS ucfg_dlm_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_dlm_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_dlm_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_dlm_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_dlm_dump_deny_list_ap(struct wlan_objmgr_pdev *pdev)
{}

static inline
QDF_STATUS
ucfg_dlm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_dlm_add_userspace_deny_list(struct wlan_objmgr_pdev *pdev,
				 struct qdf_mac_addr *bssid_deny_list,
				 uint8_t num_of_bssid)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_dlm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum dlm_connection_state con_state)
{
}

static inline
void ucfg_dlm_wifi_off(struct wlan_objmgr_pdev *pdev)
{
}

#endif
#endif /* _WLAN_DLM_UCFG_H_ */

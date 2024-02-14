/*
 * Copyright (c) 2018, 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare public API related to the wlan ipa called by north bound
 */

#ifndef _WLAN_IPA_OBJ_MGMT_H_
#define _WLAN_IPA_OBJ_MGMT_H_

#include "wlan_ipa_public_struct.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_ipa_main.h"

#ifdef IPA_OFFLOAD

/**
 * ipa_init() - IPA module initialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ipa_init(void);

/**
 * ipa_deinit() - IPA module deinitialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ipa_deinit(void);

/**
 * ipa_register_is_ipa_ready() - Register IPA ready callback
 * @pdev: pointer to pdev
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ipa_register_is_ipa_ready(struct wlan_objmgr_pdev *pdev);

/**
 * ipa_disable_register_cb() - Reset the IPA is ready flag
 *
 * Return: Set the ipa_is_ready flag to false when module is
 * unloaded to indicate that ipa ready cb is not registered
 */
void ipa_disable_register_cb(void);

/**
 * wlan_ipa_config_is_enabled() - api to get IPA enable status
 *
 * Return: true - ipa is enabled
 *         false - ipa is not enabled
 */
static inline bool wlan_ipa_config_is_enabled(void)
{
	return ipa_config_is_enabled();
}

/**
 * wlan_ipa_get_hdl() - Get ipa hdl set by IPA driver
 * @psoc: void psoc object
 * @pdev_id: pdev id
 *
 * Return: IPA handle
 */
qdf_ipa_wdi_hdl_t wlan_ipa_get_hdl(void *soc, uint8_t pdev_id);

/**
 * wlan_ipa_is_vlan enabled() - get IPA vlan support enable status
 *
 * Return: true - ipa vlan support is enabled
 *         false - ipa vlan support is not enabled
 */
bool wlan_ipa_is_vlan_enabled(void);
#else

static inline QDF_STATUS ipa_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ipa_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ipa_register_is_ipa_ready(
	struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ipa_disable_register_cb(void)
{
}

static inline bool ipa_config_is_opt_wifi_dp_enabled(void)
{
	return false;
}

static inline bool wlan_ipa_config_is_enabled(void)
{
	return false;
}

static inline bool wlan_ipa_is_vlan_enabled(void)
{
	return false;
}
#endif /* IPA_OFFLOAD */

#endif /* _WLAN_IPA_OBJ_MGMT_H_ */

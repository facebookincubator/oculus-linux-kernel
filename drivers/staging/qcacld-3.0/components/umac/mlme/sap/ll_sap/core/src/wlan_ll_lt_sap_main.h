/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains ll_sap_definitions specific to the ll_sap module
 */

#ifndef _WLAN_LL_LT_SAP_MAIN_H_
#define _WLAN_LL_LT_SAP_MAIN_H_

#include "wlan_ll_sap_main.h"
#include "wlan_mlme_public_struct.h"
#include <i_qdf_types.h>
#include <qdf_types.h>
#include "wlan_ll_sap_main.h"
#include "wlan_ll_sap_public_structs.h"

/**
 * ll_lt_sap_is_supported() - Check if ll_lt_sap is supported or not
 * @psoc: Pointer to psoc object
 * Return: True/False
 */
bool ll_lt_sap_is_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ll_lt_sap_get_freq_list() - API to get frequency list for LL_LT_SAP
 * @psoc: Pointer to psoc object
 * @freq_list: Pointer to wlan_ll_lt_sap_freq_list structure
 * @vdev_id: Vdev Id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ll_lt_sap_get_freq_list(struct wlan_objmgr_psoc *psoc,
				   struct wlan_ll_lt_sap_freq_list *freq_list,
				   uint8_t vdev_id);

/**
 * ll_lt_sap_get_valid_freq() - API to get valid frequency for LL_LT_SAP
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev Id of ll_lt_sap
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ll_lt_sap_get_valid_freq(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id);

/*
 * ll_lt_sap_init() - Initialize ll_lt_sap infrastructure
 * @vdev: Pointer to vdev
 *
 * Return: QDF_STATUS_SUCCESS if ll_lt_sap infra initialized successfully else
 * error code
 */
QDF_STATUS ll_lt_sap_init(struct wlan_objmgr_vdev *vdev);

/**
 * ll_lt_sap_deinit() - De-initialize ll_lt_sap infrastructure
 * @vdev: Pointer to vdev
 *
 * Return: QDF_STATUS_SUCCESS if ll_lt_sap infra de-initialized successfully
 * else error code
 */
QDF_STATUS ll_lt_sap_deinit(struct wlan_objmgr_vdev *vdev);
#endif /* _WLAN_LL_SAP_MAIN_H_ */

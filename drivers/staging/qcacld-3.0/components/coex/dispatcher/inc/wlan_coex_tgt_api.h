/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains coex south bound interface definitions
 */

#ifndef _WLAN_COEX_TGT_API_H_
#define _WLAN_COEX_TGT_API_H_

#ifdef WLAN_FEATURE_DBAM_CONFIG
#include "wlan_coex_public_structs.h"
#endif

#ifdef FEATURE_COEX
struct coex_config_params;
struct coex_multi_config;

/**
 * tgt_send_coex_config() - invoke target_if send coex config
 * @vdev: vdev object
 * @param: coex config parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_send_coex_config(struct wlan_objmgr_vdev *vdev,
		     struct coex_config_params *param);

/**
 * tgt_send_coex_multi_config() - invoke target_if send coex multiple config
 * @vdev: vdev object
 * @param: coex multiple config parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_send_coex_multi_config(struct wlan_objmgr_vdev *vdev,
			   struct coex_multi_config *param);

/**
 * tgt_get_coex_multi_config_support() - invoke target_if get coex multiple
 * configure support
 * @psoc: PSOC object
 *
 * Return: true if target support coex multiple config command
 */
bool
tgt_get_coex_multi_config_support(struct wlan_objmgr_psoc *psoc);
#endif

#ifdef WLAN_FEATURE_DBAM_CONFIG
/**
 * tgt_send_dbam_config() - invoke target_if send dbam config
 * @vdev: vdev object
 * @param: dbam config parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_send_dbam_config(struct wlan_objmgr_vdev *vdev,
		     struct coex_dbam_config_params *param);
#endif
#endif

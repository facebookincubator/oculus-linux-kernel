/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Public API declarations of pre cac called from SAP module
 */

#ifndef _WLAN_PRE_CAC_API_H_
#define _WLAN_PRE_CAC_API_H_

#ifdef PRE_CAC_SUPPORT
/**
 * wlan_pre_cac_get_status(): status of pre_cac
 * @psoc: psoc object manager
 *
 * Return: status of pre_cac
 */
bool wlan_pre_cac_get_status(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_pre_cac_set_status() - Set pre cac status
 * @vdev: vdev object manager
 * @status: status of pre_cac
 *
 * Sets pre_cac status
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_pre_cac_set_status(struct wlan_objmgr_vdev *vdev, bool status);

/**
 * wlan_pre_cac_handle_cac_end() - Handle pre cac end
 * @vdev: vdev object manager
 *
 * Return: None
 */
void wlan_pre_cac_handle_cac_end(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_pre_cac_complete_set() - Set pre cac complete status
 * @vdev: vdev object manager
 * @status: status
 *
 * Return: None
 */
void wlan_pre_cac_complete_set(struct wlan_objmgr_vdev *vdev,
			       bool status);

/**
 * wlan_pre_cac_complete_get() - Get pre cac complete status
 * @vdev: vdev object manager
 *
 * Return: pre cac complete status
 */
bool wlan_pre_cac_complete_get(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_pre_cac_set_freq_before_pre_cac() - Set frequency before pre cac
 * @vdev: vdev object manager
 * @freq: frequency
 *
 * Return: None
 */
void wlan_pre_cac_set_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev,
					  qdf_freq_t freq);

/**
 * wlan_pre_cac_get_freq_before_pre_cac() - Get frequency before pre cac
 * @vdev: vdev object manager
 *
 * Return: Frequency before pre cac
 */
qdf_freq_t
wlan_pre_cac_get_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_pre_cac_handle_radar_ind() - Handle pre cac radar indication
 * @vdev: vdev object manager
 *
 * Return: None
 */
void wlan_pre_cac_handle_radar_ind(struct wlan_objmgr_vdev *vdev);
#else
static inline void
wlansap_pre_cac_end_notify(struct sap_context *sap_context,
			   struct mac_context *mac,
			   uint8_t intf)
{
}

static inline bool wlan_pre_cac_get_status(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline QDF_STATUS
wlan_pre_cac_set_status(struct wlan_objmgr_vdev *vdev, bool status)
{
	return false;
}

static inline void
wlan_pre_cac_handle_cac_end(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
wlan_pre_cac_complete_set(struct wlan_objmgr_vdev *vdev,
			  bool status)
{
}

static inline bool
wlan_pre_cac_complete_get(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
wlan_pre_cac_set_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev,
				     qdf_freq_t freq)
{
}

static inline qdf_freq_t
wlan_pre_cac_get_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}
#endif /* PRE_CAC_SUPPORT */
#endif /* _WLAN_PRE_CAC_API_H_ */

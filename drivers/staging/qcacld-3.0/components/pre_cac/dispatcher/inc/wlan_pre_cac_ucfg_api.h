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
 * DOC: Declare public API related to the pre_cac called by north bound
 * HDD/OSIF/LIM
 */

#ifndef _WLAN_PRE_CAC_UCFG_API_H_
#define _WLAN_PRE_CAC_UCFG_API_H_

#include <qdf_status.h>
#include <qdf_types.h>
#include "wlan_pre_cac_public_struct.h"
#include "wlan_objmgr_psoc_obj.h"

#ifdef PRE_CAC_SUPPORT
/**
 * ucfg_pre_cac_init() - pre cac component initialization.
 *
 * This function initializes the pre cac component and registers
 * the handlers which are invoked on vdev creation.
 *
 * Return: For successful registration - QDF_STATUS_SUCCESS,
 *         else QDF_STATUS error codes.
 */
QDF_STATUS ucfg_pre_cac_init(void);

/**
 * ucfg_pre_cac_deinit() - pre cac component deinit.
 *
 * This function deinits pre cac component.
 *
 * Return: None
 */
void ucfg_pre_cac_deinit(void);

/*
 * ucfg_pre_cac_set_osif_cb() - set pre cac osif callbacks.
 * @pre_cac_ops: pre cac ops
 *
 * Return: None
 */
void ucfg_pre_cac_set_osif_cb(struct pre_cac_ops *pre_cac_ops);

/**
 * ucfg_pre_cac_clear_work() - clear pre cac work fn and arg.
 * @psoc: psoc object manager
 *
 * Return: None
 */
void ucfg_pre_cac_clear_work(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pre_cac_is_active(): status of pre_cac
 * @psoc: psoc object manager
 *
 * Return: status of pre_cac
 */
bool ucfg_pre_cac_is_active(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pre_cac_validate_and_get_freq() - Validate and get pre cac frequency
 * @pdev: pdev object manager
 * @chan_freq: Channel frequency requested by userspace
 * @pre_cac_chan_freq: Pointer to the pre CAC channel frequency storage
 *
 * Validates the channel provided by userspace. If user provided channel 0,
 * a valid outdoor channel must be selected from the regulatory channel.
 *
 * Return: Zero on success and non zero value on error
 */
int ucfg_pre_cac_validate_and_get_freq(struct wlan_objmgr_pdev *pdev,
				       uint32_t chan_freq,
				       uint32_t *pre_cac_chan_freq);

#if defined(FEATURE_SAP_COND_CHAN_SWITCH)
/**
 * ucfg_pre_cac_set_status() - Set pre cac status
 * @vdev: vdev object manager
 * @status: status of pre_cac
 *
 * Sets pre_cac status
 *
 * Return: Zero on success and non zero value on error
 */
QDF_STATUS ucfg_pre_cac_set_status(struct wlan_objmgr_vdev *vdev, bool status);
#else
static inline QDF_STATUS
ucfg_pre_cac_set_status(struct wlan_objmgr_vdev *vdev, bool status)
{
}
#endif

/**
 * ucfg_pre_cac_get_vdev_id() - Get pre cac vdev id
 * @psoc: psoc object manager
 * @vdev_id: pre cac vdev id
 *
 * Return: None
 */
void ucfg_pre_cac_get_vdev_id(struct wlan_objmgr_psoc *psoc,
			      uint8_t *vdev_id);

/**
 * ucfg_pre_cac_adapter_set() - Set pre cac adapter flag
 * @vdev: vdev object manager
 * @status: status
 *
 * Return: None
 */
void ucfg_pre_cac_adapter_set(struct wlan_objmgr_vdev *vdev,
			      bool status);

/**
 * ucfg_pre_cac_adapter_is_active() - Get pre cac adapter status
 * @vdev: vdev object manager
 *
 * Return: pre cac complete status
 */
bool ucfg_pre_cac_adapter_is_active(struct wlan_objmgr_vdev *vdev);

#if defined(FEATURE_SAP_COND_CHAN_SWITCH)
/**
 * ucfg_pre_cac_set_freq_before_pre_cac() - Set frequency before pre cac
 * @vdev: vdev object manager
 * @freq: frequency
 *
 * Return: None
 */
void ucfg_pre_cac_set_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev,
					  qdf_freq_t freq);
#else
static inline void
ucfg_pre_cac_set_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev,
				     qdf_freq_t freq)
{
}
#endif

/**
 * ucfg_pre_cac_set_freq() - Set pre cac frequency
 * @vdev: vdev object manager
 * @freq: pre cac frequency
 *
 * Return: None
 */
void ucfg_pre_cac_set_freq(struct wlan_objmgr_vdev *vdev,
			   qdf_freq_t freq);

/**
 * ucfg_pre_cac_get_freq() - Get pre cac frequency
 * @vdev: vdev object manager
 *
 * Return: pre cac frequency
 */
qdf_freq_t ucfg_pre_cac_get_freq(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pre_cac_complete_set() - Set pre cac complete status
 * @vdev: vdev object manager
 * @status: status
 *
 * Return: None
 */
void ucfg_pre_cac_complete_set(struct wlan_objmgr_vdev *vdev,
			       bool status);

/**
 * ucfg_pre_cac_stop() - Stop pre cac
 * @psoc: psoc object manager
 *
 * Return: None
 */
void ucfg_pre_cac_stop(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pre_cac_clean_up() - Cleanup pre cac
 * @psoc: psoc object manager
 *
 * Return: None
 */
void ucfg_pre_cac_clean_up(struct wlan_objmgr_psoc *psoc);
#else
static inline
QDF_STATUS ucfg_pre_cac_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_pre_cac_deinit(void)
{
}

static inline void
ucfg_pre_cac_set_osif_cb(struct pre_cac_ops *pre_cac_ops)
{
}

static inline
void ucfg_pre_cac_clear_work(struct wlan_objmgr_psoc *psoc)
{
}

static inline bool
ucfg_pre_cac_is_active(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline int
ucfg_pre_cac_validate_and_get_freq(struct wlan_objmgr_pdev *pdev,
				   uint32_t chan_freq,
				   uint32_t *pre_cac_chan_freq)
{
	return 0;
}

static inline QDF_STATUS
ucfg_pre_cac_set_status(struct wlan_objmgr_vdev *vdev, bool status)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_pre_cac_get_vdev_id(struct wlan_objmgr_psoc *psoc,
			 uint8_t *vdev_id)
{
}

static inline void
ucfg_pre_cac_adapter_set(struct wlan_objmgr_vdev *vdev,
			 bool status)
{
}

static inline bool
ucfg_pre_cac_adapter_is_active(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
ucfg_pre_cac_set_freq(struct wlan_objmgr_vdev *vdev,
		      qdf_freq_t freq)
{
}

static inline qdf_freq_t
ucfg_pre_cac_get_freq(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline void
ucfg_pre_cac_complete_set(struct wlan_objmgr_vdev *vdev,
			  bool status)
{
}

static inline void
ucfg_pre_cac_stop(struct wlan_objmgr_psoc *psoc)
{
}

static inline void
ucfg_pre_cac_clean_up(struct wlan_objmgr_psoc *psoc)
{
}
#endif /* PRE_CAC_SUPPORT */
#endif /* _WLAN_PRE_CAC_UCFG_API_H_ */

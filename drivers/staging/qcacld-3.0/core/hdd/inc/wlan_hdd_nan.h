/*
 * Copyright (c) 2014-2018 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_NAN_H
#define __WLAN_HDD_NAN_H

/**
 * DOC: wlan_hdd_nan.h
 *
 * WLAN Host Device Driver NAN API specification
 */

struct hdd_context;

#ifdef WLAN_FEATURE_NAN
struct wiphy;
struct wireless_dev;

int wlan_hdd_cfg80211_nan_request(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data,
				  int data_len);

/**
 * wlan_hdd_nan_is_supported() - HDD NAN support query function
 * @hdd_ctx: Pointer to hdd context
 *
 * This function is called to determine if NAN is supported by the
 * driver and by the firmware.
 *
 * Return: true if NAN is supported by the driver and firmware
 */
static inline bool wlan_hdd_nan_is_supported(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->config->enable_nan_support &&
		sme_is_feature_supported_by_fw(NAN);
}

/**
 * wlan_hdd_nan_separate_iface_supported() - HDD separate iface for NAN support
 * query function
 * @hdd_ctx: Pointer to hdd context
 *
 * This function is called to determine to whether separate iface for NAN is
 * enabled or not through INI
 *
 * Return: true if supported by the driver and firmware
 */
static inline bool
wlan_hdd_nan_separate_iface_supported(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->config->nan_separate_iface_support;
}

/**
 * hdd_nan_populate_cds_config() - Populate NAN cds configuration
 * @cds_cfg: CDS Configuration
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static inline void hdd_nan_populate_cds_config(struct cds_config_info *cds_cfg,
			struct hdd_context *hdd_ctx)
{
	cds_cfg->is_nan_enabled = hdd_ctx->config->enable_nan_support;
	cds_cfg->nan_separate_iface_support =
		hdd_ctx->config->nan_separate_iface_support;
}

/**
 * hdd_nan_populate_pmo_config() - Populate NAN pmo configuration
 * @pmo_cfg: PMO Configuration
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static inline void hdd_nan_populate_pmo_config(struct pmo_psoc_cfg *pmo_cfg,
			struct hdd_context *hdd_ctx)
{
	pmo_cfg->nan_enable = hdd_ctx->config->enable_nan_support;
}

/**
 * wlan_hdd_cfg80211_nan_callback() - cfg80211 NAN event handler
 * @hdd_handle: opaque handle to the global HDD context
 * @msg: NAN event message
 *
 * This is a callback function and it gets called when we need to report
 * a nan event to userspace.  The wlan host driver simply encapsulates the
 * event into a netlink payload and then forwards it to userspace via a
 * cfg80211 vendor event.
 *
 * Return: nothing
 */
void wlan_hdd_cfg80211_nan_callback(hdd_handle_t hdd_handle, tSirNanEvent *msg);

/**
 * ucfg_nan_disable_ind_to_userspace() - Send NAN disble ind to userspace
 * @psoc: pointer to psoc object
 *
 * Prepare NAN disable indication and send it to userspace
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_nan_disable_ind_to_userspace(struct hdd_context *hdd_ctx);
#else
static inline bool wlan_hdd_nan_is_supported(struct hdd_context *hdd_ctx)
{
	return false;
}
static inline bool
wlan_hdd_nan_separate_iface_supported(struct hdd_context *hdd_ctx)
{
	return false;
}

static inline void hdd_nan_populate_cds_config(struct cds_config_info *cds_cfg,
			struct hdd_context *hdd_ctx)
{
}

static inline void hdd_nan_populate_pmo_config(struct pmo_psoc_cfg *pmo_cfg,
			struct hdd_context *hdd_ctx)
{
}

static inline
void wlan_hdd_cfg80211_nan_callback(hdd_handle_t hdd_handle, tSirNanEvent *msg)
{
}

static inline
QDF_STATUS hdd_nan_disable_ind_to_userspace(struct hdd_context *hdd_ctx)
{
}
#endif /* WLAN_FEATURE_NAN */
#endif /* __WLAN_HDD_NAN_H */

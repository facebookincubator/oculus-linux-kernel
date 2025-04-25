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

/*
 * DOC: wlan_hdd_afc.c
 *
 * This file has the AFC osif interface with linux kernel.
 */

#include <wlan_hdd_afc.h>
#include <osif_psoc_sync.h>
#include <wlan_cfg80211_afc.h>
#include <wlan_hdd_main.h>
#include <pld_common.h>
#include <wlan_afc_ucfg_api.h>

int wlan_hdd_vendor_afc_response(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx;
	int ret;

	hdd_enter();

	ret = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (ret)
		return ret;

	hdd_ctx = wiphy_priv(wiphy);

	if (!hdd_ctx->psoc) {
		osif_err("psoc is null");
		ret = -EINVAL;
		goto sync_stop;
	}

	if (!hdd_ctx->pdev) {
		osif_err("pdev is null");
		ret = -EINVAL;
		goto sync_stop;
	}

	ret = wlan_cfg80211_vendor_afc_response(hdd_ctx->psoc,
						hdd_ctx->pdev,
						data,
						data_len);
sync_stop:
	osif_psoc_sync_op_stop(psoc_sync);

	hdd_exit();

	return ret;
}

/**
 * wlan_hdd_send_response_to_afcmem() - Callback function register to AFC
 * common component.
 * @psoc: Pointer to PSOC object
 * @pdev: Pointer to PDEV object
 * @data: Pointer to AFC response data pass to target
 * @len: Length of AFC response data pass to target
 *
 * Return: 0 if success, otherwise error code
 */
static int wlan_hdd_send_response_to_afcmem(struct wlan_objmgr_psoc *psoc,
					    struct wlan_objmgr_pdev *pdev,
					    struct wlan_afc_host_resp *data,
					    uint32_t len)
{
	qdf_device_t qdf_dev;
	int pdev_id;
	const uint8_t *afcdb = (const uint8_t *)data;

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!qdf_dev) {
		osif_err("Invalid qdf dev");
		return -EINVAL;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	return pld_send_buffer_to_afcmem(qdf_dev->dev,
					 afcdb,
					 len,
					 pdev_id);
}

void wlan_hdd_register_afc_pld_cb(struct wlan_objmgr_psoc *psoc)
{
	ucfg_afc_register_data_send_cb(psoc, wlan_hdd_send_response_to_afcmem);
}

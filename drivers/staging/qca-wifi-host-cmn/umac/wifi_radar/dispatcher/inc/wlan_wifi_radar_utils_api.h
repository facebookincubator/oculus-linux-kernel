/*
 * Copyright (c) 2021, 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_WIFI_RADAR_UTILS_API_H_
#define _WLAN_WIFI_RADAR_UTILS_API_H_

#include <wlan_objmgr_cmn.h>
#include <qdf_streamfs.h>

#define wifi_radar_alert(format, args...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_err(format, args...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_warn(format, args...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_info(format, args...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

#define wifi_radar_debug(format, args...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_WIFI_RADAR, format, ## args)

/**
 * struct psoc_wifi_radar - private psoc object for WiFi Radar
 * @psoc_obj: pointer to psoc object
 * @is_wifi_radar_capable: flag to determine if wifi radar is enabled or not
 */
struct psoc_wifi_radar {
	struct wlan_objmgr_psoc *psoc_obj;
	uint8_t is_wifi_radar_capable;
};

/**
 * struct pdev_wifi_radar - private pdev object for WiFi Radar
 * @pdev_obj: pointer to pdev object
 * @is_wifi_radar_capable: flag to determine if wifi radar is enabled or not
 * @chan_ptr: Channel in relayfs
 * @dir_ptr: Parent directory of relayfs file
 * @num_subbufs: No. of sub-buffers used in relayfs
 * @subbuf_size: Size of sub-buffer used in relayfs
 */
struct pdev_wifi_radar {
	struct wlan_objmgr_pdev *pdev_obj;
	uint8_t is_wifi_radar_capable;
	qdf_streamfs_chan_t chan_ptr;
	qdf_dentry_t dir_ptr;
	uint32_t num_subbufs;
	uint32_t subbuf_size;
};

/**
 * struct peer_wifi_radar - private peer object for WiFi Radar
 * @peer_obj: pointer to peer_obj
 */
struct peer_wifi_radar {
	struct wlan_objmgr_peer *peer_obj;
};

/**
 * wlan_wifi_radar_init() - Global init for wifi radar.
 *
 * Return: status of global init pass/fail
 */
QDF_STATUS wlan_wifi_radar_init(void);

/**
 * wlan_wifi_radar_deinit() - Global de-init for wifi radar.
 *
 * Return: status of global de-init pass/fail
 */
QDF_STATUS wlan_wifi_radar_deinit(void);

/**
 * wlan_wifi_radar_pdev_open() - pdev_open function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_open pass/fail
 */
QDF_STATUS wlan_wifi_radar_pdev_open(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_wifi_radar_pdev_close() - pdev_close function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_close pass/fail
 */
QDF_STATUS wlan_wifi_radar_pdev_close(struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_initialize_pdev() - pdev_initialize function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_initialize pass/fail
 */
QDF_STATUS wifi_radar_initialize_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_deinitialize_pdev() - pdev_deinitialize function for wifi radar.
 * @pdev: pointer to pdev object
 *
 * Return: status of pdev_deinitialize pass/fail
 */
QDF_STATUS wifi_radar_deinitialize_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_wifi_radar_is_feature_disabled() - Check if wifi radarfeature
 * is disabled
 * @pdev: the physical device object.
 *
 * Return : true if wifi radar is disabled, else false.
 */
bool wlan_wifi_radar_is_feature_disabled(struct wlan_objmgr_pdev *pdev);

#endif

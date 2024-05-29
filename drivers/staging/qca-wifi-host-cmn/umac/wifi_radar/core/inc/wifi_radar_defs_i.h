/*
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WIFI_RADAR_DEFS_I_H_
#define _WIFI_RADAR_DEFS_I_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_list.h>
#include <qdf_timer.h>
#include <qdf_util.h>
#include <qdf_types.h>
#include <wlan_wifi_radar_utils_api.h>

/**
 * wlan_wifi_radar_psoc_obj_create_handler() -
 * psoc object create handler for WiFi Radar
 * @psoc: pointer to psoc object
 * @arg: void pointer in case it needs arguments
 *
 * Return: status of object creation
 */
QDF_STATUS
wlan_wifi_radar_psoc_obj_create_handler(
struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * wlan_wifi_radar_psoc_obj_destroy_handler() -
 * psoc object destroy handler for WiFi Radar
 * @psoc: pointer to psoc object
 * @arg: void pointer in case it needs arguments
 *
 * Return: status of destroy object
 */
QDF_STATUS
wlan_wifi_radar_psoc_obj_destroy_handler(
struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * wlan_wifi_radar_pdev_obj_create_handler() -
 * pdev object create handler for WiFi Radar
 * @pdev: pointer to pdev object
 * @arg: void pointer in case it needs arguments
 *
 * Return: status of object creation
 */
QDF_STATUS
wlan_wifi_radar_pdev_obj_create_handler(
struct wlan_objmgr_pdev *pdev, void *arg);

/**
 * wlan_wifi_radar_pdev_obj_destroy_handler() -
 * pdev object destroy handler for WiFi Radar
 * @pdev: pointer to pdev object
 * @arg: void pointer in case it needs arguments
 *
 * Return: status of destroy object
 */
QDF_STATUS
wlan_wifi_radar_pdev_obj_destroy_handler(
struct wlan_objmgr_pdev *pdev, void *arg);

/**
 * wlan_wifi_radar_peer_obj_create_handler() -
 * peer object create handler for WiFi Radar
 * @peer: pointer to peer object
 * @arg: void pointer in case it needs arguments
 *
 * Return: status of object creation
 */
QDF_STATUS
wlan_wifi_radar_peer_obj_create_handler(
struct wlan_objmgr_peer *peer, void *arg);

/**
 * wlan_wifi_radar_peer_obj_destroy_handler() -
 * peer object destroy handler for WiFi radar
 * @peer: pointer to peer object
 * @arg: void pointer in case it needs arguments
 *
 * Return: status ofi destroy object
 */
QDF_STATUS
wlan_wifi_radar_peer_obj_destroy_handler(
struct wlan_objmgr_peer *peer, void *arg);

/**
 * wifi_radar_streamfs_init() - stream filesystem init
 * @pdev: pointer to pdev object
 *
 * Return: status of fs init
 */
QDF_STATUS
wifi_radar_streamfs_init(struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_streamfs_remove() - stream filesystem remove
 * @pdev: pointer to pdev object
 *
 * Return: status of fs remove
 */
QDF_STATUS
wifi_radar_streamfs_remove(struct wlan_objmgr_pdev *pdev);

/**
 * wifi_radar_streamfs_write() - write to stream filesystem
 * @pa: pointer to pdev_cfr object
 * @write_data: Pointer to data
 * @write_len: data len
 *
 * Return: status of fs write
 */
QDF_STATUS
wifi_radar_streamfs_write(
struct pdev_cfr *pa, const void *write_data,
size_t write_len);

/**
 * wifi_radar_streamfs_flush() - flush the write to streamfs
 * @pa: pointer to pdev_cfr object
 *
 * Return: status of fs flush
 */
QDF_STATUS
wifi_radar_streamfs_flush(struct pdev_cfr *pa);
#endif

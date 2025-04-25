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
 * DOC: wlan_mlo_epcs_ucfg_api.h
 *
 * Implementation for mlo epcs public ucfg API interfaces.
 */

#ifndef _WLAN_MLO_EPCS_UCFG_API_H_
#define _WLAN_MLO_EPCS_UCFG_API_H_

#include "wlan_epcs_api.h"

/**
 * ucfg_epcs_deliver_cmd() - Handler to deliver EPCS command
 * @vdev: vdev pointer
 * @event: EPCS event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_epcs_deliver_cmd(struct wlan_objmgr_vdev *vdev,
				 enum wlan_epcs_evt event);

/**
 * ucfg_epcs_set_config() - Set EPCS enable/disable config
 * @vdev: vdev pointer
 * @flag: EPCS flag
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_epcs_set_config(struct wlan_objmgr_vdev *vdev, uint8_t flag);

#endif /* _WLAN_MLO_EPCS_UCFG_API_H_ */

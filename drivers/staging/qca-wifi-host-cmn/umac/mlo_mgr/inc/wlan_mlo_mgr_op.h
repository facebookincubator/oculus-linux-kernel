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
 * DOC: contains MLO manager public file containing operation functionality
 */
#ifndef _WLAN_MLO_MGR_OP_H_
#define _WLAN_MLO_MGR_OP_H_

#include <wlan_mlo_mgr_cmn.h>
#include <wlan_mlo_mgr_public_structs.h>

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_mlo_set_cu_bpcc() - set the bpcc per link id
 * @vdev: vdev object
 * @bpcc: bss parameters change count
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_set_cu_bpcc(struct wlan_objmgr_vdev *vdev, uint8_t bpcc);

/**
 * wlan_mlo_get_cu_bpcc() - get the bpcc per link id
 * @vdev: vdev object
 * @bpcc: the bss parameters change count pointer to save value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_get_cu_bpcc(struct wlan_objmgr_vdev *vdev, uint8_t *bpcc);

/**
 * wlan_mlo_init_cu_bpcc() - initialize the bpcc for vdev
 * @vdev: vdev object
 *
 * Return: void
 */
void wlan_mlo_init_cu_bpcc(struct wlan_objmgr_vdev *vdev);

#else
static inline void
wlan_mlo_init_cu_bpcc(struct wlan_objmgr_vdev *vdev)
{ }

#endif
#endif //_WLAN_MLO_MGR_OP_H_

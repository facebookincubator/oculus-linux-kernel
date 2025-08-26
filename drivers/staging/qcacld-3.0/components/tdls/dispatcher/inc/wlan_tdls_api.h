/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains tdls link teardown declarations
 */
 #ifndef _WLAN_TDLS_API_H_
 #define _WLAN_TDLS_API_H_

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"

#ifdef FEATURE_WLAN_TDLS

#ifdef FEATURE_SET
/**
 * wlan_tdls_get_features_info() - Get tdls features info
 * @psoc: psoc context
 * @tdls_feature_set: TDLS feature set info structure
 *
 * Return: None
 */

void wlan_tdls_get_features_info(struct wlan_objmgr_psoc *psoc,
				 struct wlan_tdls_features *tdls_feature_set);
#endif

/**
 * wlan_tdls_teardown_links() - notify TDLS module to teardown all TDLS links
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_tdls_teardown_links(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_tdls_teardown_links_sync() - teardown all the TDLS links
 * @psoc: psoc object
 *
 * Return: None
 */
void wlan_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_tdls_notify_sta_disconnect() - notify sta disconnect
 * @vdev_id: pointer to soc object
 * @lfr_roam: indicate, whether disconnect due to lfr roam
 * @user_disconnect: disconnect from user space
 * @vdev: vdev object manager
 *
 * Notify sta disconnect event to TDLS component
 *
 * Return: QDF_STATUS
 */
void wlan_tdls_notify_sta_disconnect(uint8_t vdev_id,
				     bool lfr_roam, bool user_disconnect,
				     struct wlan_objmgr_vdev *vdev);

/**
 * wlan_tdls_notify_sta_connect() - notify sta connect to TDLS
 * @vdev_id: pointer to soc object
 * @tdls_chan_swit_prohibited: indicates channel switch capability
 * @tdls_prohibited: indicates tdls allowed or not
 * @vdev: vdev object manager
 *
 * Notify sta connect event to TDLS component
 *
 * Return: None
 */
void
wlan_tdls_notify_sta_connect(uint8_t vdev_id,
			     bool tdls_chan_swit_prohibited,
			     bool tdls_prohibited,
			     struct wlan_objmgr_vdev *vdev);

/**
 * wlan_tdls_update_tx_pkt_cnt() - update tx pkt count
 * @vdev: tdls vdev object
 * @mac_addr: peer mac address
 *
 * Return: None
 */
void wlan_tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr);

/**
 * wlan_tdls_update_rx_pkt_cnt() - update rx pkt count
 * @vdev: tdls vdev object
 * @mac_addr: peer mac address
 * @dest_mac_addr: dest mac address
 *
 * Return: None
 */
void wlan_tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr);

#else

#ifdef FEATURE_SET
static inline
void wlan_tdls_get_features_info(struct wlan_objmgr_psoc *psoc,
				 struct wlan_tdls_features *tdls_feature_set)
{
}
#endif

static inline QDF_STATUS wlan_tdls_teardown_links(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void wlan_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc)
{}

static inline
void wlan_tdls_notify_sta_disconnect(uint8_t vdev_id,
				     bool lfr_roam, bool user_disconnect,
				     struct wlan_objmgr_vdev *vdev)
{}

static inline void
wlan_tdls_notify_sta_connect(uint8_t vdev_id,
			     bool tdls_chan_swit_prohibited,
			     bool tdls_prohibited,
			     struct wlan_objmgr_vdev *vdev) {}

static inline void
wlan_tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
			    struct qdf_mac_addr *mac_addr)
{
}

static inline
void wlan_tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr)
{
}

#endif
#endif

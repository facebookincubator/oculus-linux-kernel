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
 * DOC: contains tdls link teardown declarations
 */
 #ifndef _WLAN_TDLS_API_H_
 #define _WLAN_TDLS_API_H_

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"

#ifdef FEATURE_WLAN_TDLS
#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_tdls_is_fw_11be_mlo_capable() - Get TDLS 11be mlo capab
 * @psoc: psoc context
 *
 * Return: True if 11be mlo capable
 */
bool wlan_tdls_is_fw_11be_mlo_capable(struct wlan_objmgr_psoc *psoc);
#else
static inline
bool wlan_tdls_is_fw_11be_mlo_capable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif
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
 * wlan_tdls_check_and_teardown_links_sync() - teardown all the TDLS links
 * @psoc: psoc object
 * @vdev: Vdev object pointer
 *
 * Return: None
 */
void wlan_tdls_check_and_teardown_links_sync(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_vdev *vdev);

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
/**
 * wlan_tdls_notify_start_bss_failure() - Notify TDLS module on start bss
 * failure
 * @psoc: Pointer to PSOC object
 *
 * Return: None
 */
void wlan_tdls_notify_start_bss_failure(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_tdls_notify_start_bss() - Notify TDLS module on start bss
 * @psoc: Pointer to PSOC object
 * @vdev: Vdev object pointer
 *
 * Return: None
 */
void wlan_tdls_notify_start_bss(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_TDLS_CONCURRENCIES
/**
 * wlan_tdls_notify_channel_switch_complete() - Notify TDLS module about the
 * channel switch completion
 * @psoc: Pointer to PSOC object
 * @vdev_id: vdev id
 *
 * Return: None
 */
void wlan_tdls_notify_channel_switch_complete(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id);

/**
 * wlan_tdls_notify_channel_switch_start() - Process channel switch start
 * for SAP/P2P GO vdev. For STA vdev, TDLS teardown happens, so explicit
 * disable off channel is not required.
 * @psoc: Pointer to PSOC object
 * @vdev: Pointer to current vdev on which CSA is triggered
 *
 * Return: None
 */
void wlan_tdls_notify_channel_switch_start(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev);

/**
 * wlan_tdls_handle_p2p_client_connect() - Handle P2P Client connect start
 * @psoc: Pointer to PSOC object
 * @vdev: Pointer to P2P client vdev
 *
 * Return: None
 */
void wlan_tdls_handle_p2p_client_connect(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev);
#else
static inline
void wlan_tdls_notify_channel_switch_complete(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id)
{}

static inline
void wlan_tdls_notify_channel_switch_start(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev)
{}

static inline
void wlan_tdls_handle_p2p_client_connect(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev)
{}
#endif /* WLAN_FEATURE_TDLS_CONCURRENCIES */

/**
 * wlan_tdls_increment_discovery_attempts() - Increment TDLS peer discovery
 * attempts
 * @psoc: Pointer to PSOC object
 * @vdev_id: Vdev id
 * @peer_addr: Peer mac address
 *
 * Return: None
 */
void wlan_tdls_increment_discovery_attempts(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    uint8_t *peer_addr);
#else

#ifdef FEATURE_SET
static inline
void wlan_tdls_get_features_info(struct wlan_objmgr_psoc *psoc,
				 struct wlan_tdls_features *tdls_feature_set)
{
}
#endif

static inline
bool wlan_tdls_is_fw_11be_mlo_capable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline QDF_STATUS wlan_tdls_teardown_links(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
wlan_tdls_check_and_teardown_links_sync(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_vdev *vdev)
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

static inline
void wlan_tdls_notify_start_bss(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev)
{}

static inline
void wlan_tdls_notify_channel_switch_complete(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id)
{}

static inline
void wlan_tdls_notify_channel_switch_start(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev)
{}

static inline
void wlan_tdls_handle_p2p_client_connect(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev)
{}

static inline
void wlan_tdls_notify_start_bss_failure(struct wlan_objmgr_psoc *psoc)
{}

static inline
void wlan_tdls_increment_discovery_attempts(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    uint8_t *peer_addr)
{}
#endif
#endif

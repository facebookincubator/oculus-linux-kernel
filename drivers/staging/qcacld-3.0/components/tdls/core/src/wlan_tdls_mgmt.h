/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: wlan_tdls_mgmt.h
 *
 * TDLS management frames include file
 */

#ifndef _WLAN_TDLS_MGMT_H_
#define _WLAN_TDLS_MGMT_H_

#include "wlan_tdls_cmds_process.h"

#define TDLS_PUBLIC_ACTION_FRAME_OFFSET 24
/* TDLS_PUBLIC_ACTION_FRAME_OFFSET(category[1]) + action[1] + dialog[1]
 * + cap[2]
 */
#define TDLS_PUBLIC_ACTION_FRAME_TDLS_IE_OFFSET 29
#define TDLS_PUBLIC_ACTION_FRAME 4
#define TDLS_PUBLIC_ACTION_DISC_RESP 14
#define TDLS_ACTION_FRAME 12
#define TDLS_80211_PEER_ADDR_OFFSET (TDLS_PUBLIC_ACTION_FRAME + \
				     QDF_MAC_ADDR_SIZE)
#define TDLS_ACTION_FRAME_TYPE_MAX 11

/**
 * struct tdls_link_identifier - tdls link identifier ie
 * @id: element id
 * @len: length
 * @bssid: bssid
 * @initsta: the mac address of initiator
 * @respsta: the mac address of responder
 */
struct tdls_link_identifier {
	uint8_t id;
	uint8_t len;
	uint8_t bssid[6];
	uint8_t initsta[6];
	uint8_t respsta[6];
};

/**
 * struct tdls_rx_mgmt_event - tdls rx mgmt frame event
 * @tdls_soc_obj: tdls soc private object
 * @rx_mgmt: tdls rx mgmt frame structure
 */
struct tdls_rx_mgmt_event {
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_rx_mgmt_frame *rx_mgmt;
};

/**
 * tdls_process_mgmt_req() - send a TDLS mgmt request to serialize module
 * @tdls_mgmt_req: tdls management request
 *
 * TDLS request API, called from cfg80211 to send a TDLS frame in
 * serialized manner to PE
 *
 *Return: QDF_STATUS
 */
QDF_STATUS tdls_process_mgmt_req(
			struct tdls_action_frame_request *tdls_mgmt_req);

/**
 * tdls_process_mlo_cal_tdls_link_score() - process mlo cal tdls link
 * @vdev: object manager vdev
 *
 * Converts rx tdls frame freq to a link score and stores the score
 * in relative tdls_vdev object.
 *
 *Return: QDF_STATUS
 */
QDF_STATUS
tdls_process_mlo_cal_tdls_link_score(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_mlo_get_tdls_link_vdev() - wrapper function
 * @vdev: vdev object
 *
 * Return: tdls vdev
 */
struct wlan_objmgr_vdev *
tdls_mlo_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_set_remain_links_unforce() - unforce links
 * @vdev: vdev object
 *
 * Return: void
 */
void tdls_set_remain_links_unforce(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_mgmt_rx_ops() - register or unregister rx callback
 * @psoc: psoc object
 * @isregister: register if true, unregister if false
 *
 * This function registers or unregisters rx callback to mgmt txrx
 * component.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister);

/**
 * tdls_process_rx_frame() - process tdls rx frames
 * @msg: scheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_rx_frame(struct scheduler_msg *msg);

/**
 * tdls_set_rssi() - Set TDLS RSSI on peer given by mac
 * @vdev: vdev object
 * @mac: MAC address of Peer
 * @rssi: rssi value
 *
 * Set RSSI on TDSL peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			 uint8_t *mac, int8_t rssi);

/**
 * tdls_process_mlo_choice_tdls_vdev() - choice one vdev for tdls vdev
 * @vdev: object manager vdev
 *
 * Return: pointer of vdev object
 */
struct wlan_objmgr_vdev *
tdls_process_mlo_choice_tdls_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_set_no_force_vdev() - set no force for the vdev
 * @vdev: object manager vdev
 * @flag: true, set all vdev as no force; false, except the current one.
 *
 * Return: void
 */
void tdls_set_no_force_vdev(struct wlan_objmgr_vdev *vdev, bool flag);

/**
 * tdls_set_link_mode() - force active or unfore link for MLO case
 * @req: the pointer of tdls_action_frame_request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_set_link_mode(struct tdls_action_frame_request *req);
#endif


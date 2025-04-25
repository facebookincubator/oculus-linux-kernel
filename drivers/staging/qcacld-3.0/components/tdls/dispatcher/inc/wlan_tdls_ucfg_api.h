/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_tdls_ucfg_api.h
 *
 * TDLS north bound interface declaration
 */

#if !defined(_WLAN_TDLS_UCFG_API_H_)
#define _WLAN_TDLS_UCFG_API_H_

#include <scheduler_api.h>
#include <wlan_tdls_public_structs.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>

#ifdef FEATURE_WLAN_TDLS

/**
 * ucfg_tdls_init() - TDLS module initialization API
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_init(void);

/**
 * ucfg_tdls_deinit() - TDLS module deinitialization API
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_deinit(void);

/**
 * ucfg_tdls_psoc_open() - TDLS module psoc open API
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_tdls_psoc_close() - TDLS module psoc close API
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_tdls_update_config() - TDLS module start
 * @psoc: psoc object
 * @req: tdls start paramets
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_update_config(struct wlan_objmgr_psoc *psoc,
				   struct tdls_start_params *req);

/**
 * ucfg_tdls_update_fw_wideband_capability() - Update FW TDLS wideband
 *                                             capability in TDLS component
 *
 * @psoc: psoc object
 * @is_fw_tdls_wideband_capable: if fw is tdls wideband capable then it is true
 *
 * Return: void
 */
void ucfg_tdls_update_fw_wideband_capability(struct wlan_objmgr_psoc *psoc,
					     bool is_fw_tdls_wideband_capable);

/**
 * ucfg_tdls_is_fw_wideband_capable() - Get FW TDLS wideband capability from
 *                                      TDLS component.
 * @psoc: psoc object
 *
 * Return: true if fw supports tdls wideband
 */
bool ucfg_tdls_is_fw_wideband_capable(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_11AX
/**
 * ucfg_tdls_update_fw_11ax_capability() - Update FW TDLS 11ax capability in
 *                                      TDLS Component
 * @psoc: psoc object
 * @is_fw_tdls_11ax_capable: bool if fw is tdls 11ax capable then it is true
 *
 * Return: void
 */
void ucfg_tdls_update_fw_11ax_capability(struct wlan_objmgr_psoc *psoc,
					 bool is_fw_tdls_11ax_capable);

/**
 * ucfg_update_fw_tdls_6g_capability() - Update FW TDLS 6g capability in TDLS
 *                                    Component
 * @psoc: psoc object
 * @is_fw_tdls_6g_capable: set to true if firmware supports TDLS on 6G band
 *
 * Return: void
 */
void ucfg_update_fw_tdls_6g_capability(struct wlan_objmgr_psoc *psoc,
				       bool is_fw_tdls_6g_capable);

/**
 * ucfg_tdls_is_fw_11ax_capable() - Get FW TDLS 11ax capability from TLDS
 *                                    component.
 * @psoc: psoc object
 *
 * Return: true if fw supports tdls 11ax
 */
bool ucfg_tdls_is_fw_11ax_capable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_tdls_is_fw_6g_capable() - Get FW TDLS 6g capability from TLDS
 *                                component.
 * @psoc: psoc object
 *
 * Return: true if fw supports tdls on 6ghz band
 */
bool ucfg_tdls_is_fw_6g_capable(struct wlan_objmgr_psoc *psoc);

#else
static inline
void ucfg_tdls_update_fw_11ax_capability(struct wlan_objmgr_psoc *psoc,
					 bool is_fw_tdls_11ax_capable)
{
}

static inline
bool  ucfg_tdls_is_fw_11ax_capable(struct wlan_objmgr_psoc *psoc)
{
return false;
}

static inline
void ucfg_update_fw_tdls_6g_capability(struct wlan_objmgr_psoc *psoc,
				       bool is_fw_tdls_6g_capable)
{
}

static inline
bool  ucfg_tdls_is_fw_6g_capable(struct wlan_objmgr_psoc *psoc)
{
return false;
}
#endif

#ifdef WLAN_FEATURE_11BE
/**
 * ucfg_tdls_update_fw_mlo_capability() - update fw mlo capability
 * @psoc: psoc object
 * @is_fw_tdls_mlo_capable: bool value
 *
 * Return: none
 */
void ucfg_tdls_update_fw_mlo_capability(struct wlan_objmgr_psoc *psoc,
					bool is_fw_tdls_mlo_capable);
#else
static inline
void ucfg_tdls_update_fw_mlo_capability(struct wlan_objmgr_psoc *psoc,
					bool is_fw_tdls_mlo_capable)
{
}
#endif

/**
 * ucfg_tdls_link_vdev_is_matching() - check whether vdev is matching link vdev
 * @vdev: vdev object
 *
 * Return: bool
 */
bool ucfg_tdls_link_vdev_is_matching(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_tdls_get_tdls_link_vdev() - get tdls link vdev
 * @vdev: vdev object
 * @dbg_id: debug id
 *
 * Return: vdev pointer
 */
struct wlan_objmgr_vdev *
ucfg_tdls_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev,
			     wlan_objmgr_ref_dbgid dbg_id);

/**
 * ucfg_tdls_put_tdls_link_vdev() - put tdls link vdev
 * @vdev: vdev odject
 * @dbg_id: debug id
 *
 * Return: void
 */
void ucfg_tdls_put_tdls_link_vdev(struct wlan_objmgr_vdev *vdev,
				  wlan_objmgr_ref_dbgid dbg_id);

/**
 * ucfg_tdls_psoc_enable() - TDLS module enable API
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_tdls_psoc_disable() - TDLS module disable API
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_tdls_add_peer() - handle TDLS add peer
 * @vdev: vdev object
 * @add_peer_req: add peer request parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_add_peer(struct wlan_objmgr_vdev *vdev,
			      struct tdls_add_peer_params *add_peer_req);

/**
 * ucfg_tdls_update_peer() - handle TDLS update peer
 * @vdev: vdev object
 * @update_peer: update TDLS request parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_update_peer(struct wlan_objmgr_vdev *vdev,
				 struct tdls_update_peer_params *update_peer);

/**
 * ucfg_tdls_oper() - handle TDLS oper functions
 * @vdev: vdev object
 * @macaddr: MAC address of TDLS peer
 * @cmd: oper cmd
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_oper(struct wlan_objmgr_vdev *vdev,
			  const uint8_t *macaddr, enum tdls_command_type cmd);

/**
 * ucfg_tdls_get_all_peers() - get all tdls peers from the list
 * @vdev: vdev object
 * @buf: output buffer
 * @buflen: length of written data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_get_all_peers(struct wlan_objmgr_vdev *vdev,
				   char *buf, int buflen);

/**
 * ucfg_tdls_send_mgmt_frame() - send TDLS mgmt frame
 * @mgmt_req: pointer to TDLS action frame request struct
 *
 * This will TDLS action frames to peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_send_mgmt_frame(
				struct tdls_action_frame_request *mgmt_req);

/**
 * ucfg_tdls_responder() - set responder in TDLS peer
 * @msg_req: responder msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_responder(struct tdls_set_responder_req *msg_req);

/**
 * ucfg_tdls_teardown_links() - notify TDLS modules to teardown all TDLS links.
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_teardown_links(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_tdls_teardown_links_sync() - teardown all TDLS links.
 * @psoc: psoc object
 * @vdev: Vdev object pointer
 *
 * Return: None
 */
void ucfg_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_tdls_notify_reset_adapter() - notify reset adapter
 * @vdev: vdev object manager
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_notify_reset_adapter(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_tdls_set_operating_mode() - set operating mode
 * @set_mode_params: set mode params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_set_operating_mode(
			struct tdls_set_mode_params *set_mode_params);

/**
 * ucfg_tdls_update_rx_pkt_cnt() - update rx pkt count
 * @vdev: tdls vdev object
 * @mac_addr: peer mac address
 * @dest_mac_addr: dest mac address
 *
 * Return: None
 */
void ucfg_tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr);

/**
 * ucfg_tdls_update_tx_pkt_cnt() - update tx pkt count
 * @vdev: tdls vdev object
 * @mac_addr: peer mac address
 *
 * Return: None
 */
void ucfg_tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr);

/**
 * ucfg_tdls_antenna_switch() - tdls antenna switch
 * @vdev: tdls vdev object
 * @mode: antenna mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_antenna_switch(struct wlan_objmgr_vdev *vdev,
				    uint32_t mode);

/**
 * ucfg_set_tdls_offchannel() - Handle TDLS set offchannel
 * @vdev: vdev object
 * @offchannel: updated offchannel
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_set_tdls_offchannel(struct wlan_objmgr_vdev *vdev,
				    int offchannel);

/**
 * ucfg_set_tdls_offchan_mode() - Handle TDLS set offchannel mode
 * @vdev: vdev object
 * @offchanmode: updated off-channel mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_set_tdls_offchan_mode(struct wlan_objmgr_vdev *vdev,
				      int offchanmode);

/**
 * ucfg_set_tdls_secoffchanneloffset() - Handle TDLS set offchannel offset
 * @vdev: vdev object
 * @offchanoffset: tdls off-channel offset
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_set_tdls_secoffchanneloffset(struct wlan_objmgr_vdev *vdev,
					     int offchanoffset);

/**
 * ucfg_tdls_discovery_on_going() - check discovery is on going
 * @vdev: vdev object
 *
 * Return: true if tdls discovery on going else false
 */
bool ucfg_tdls_discovery_on_going(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_tdls_get_mlo_vdev() - get mlo vdev for tdls
 * @vdev: vdev object
 * @index: index of vdev in mlo list
 * @dbg_id: debug id
 *
 * Return: vdev pointer
 */
struct wlan_objmgr_vdev *ucfg_tdls_get_mlo_vdev(struct wlan_objmgr_vdev *vdev,
						uint8_t index,
						wlan_objmgr_ref_dbgid dbg_id);

/**
 * ucfg_tdls_release_mlo_vdev() - release mlo vdev for tdls
 * @vdev: vdev object
 * @dbg_id: debug id
 *
 * Return: void
 */
void ucfg_tdls_release_mlo_vdev(struct wlan_objmgr_vdev *vdev,
				wlan_objmgr_ref_dbgid dbg_id);
/**
 * ucfg_tdls_set_rssi() - API to set TDLS RSSI on peer given by mac
 * @vdev: vdev object
 * @mac: MAC address of Peer
 * @rssi: rssi value
 *
 * Set RSSI on TDLS peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			      uint8_t *mac, int8_t rssi);

/**
 * ucfg_tdls_notify_connect_failure() - This api is called if STA/P2P
 * connection fails on one iface and to enable/disable TDLS on the other
 * STA/P2P iface which is already connected.It is a wrapper function to
 * API wlan_tdls_notify_connect_failure()
 * @psoc: psoc object
 *
 * Return: void
 */
void ucfg_tdls_notify_connect_failure(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_get_tdls_conn_peer_count() - This api is called to get number of
 *                                    connected TDLS peer
 * @vdev: vdev object
 *
 * Return: tdls connected peer count
 */
uint16_t ucfg_get_tdls_conn_peer_count(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_get_tdls_vdev() - Ucfg api to get tdls specific vdev object
 * @psoc: wlan psoc object manager
 * @dbg_id: debug id
 *
 * If TDLS is enabled on any vdev then return the corresponding vdev.
 *
 * This api increases the ref count of the returned vdev.
 * Return: vdev manager pointer or NULL.
 */
struct wlan_objmgr_vdev *ucfg_get_tdls_vdev(struct wlan_objmgr_psoc *psoc,
					    wlan_objmgr_ref_dbgid dbg_id);

/**
 * ucfg_tdls_check_is_tdls_allowed() - Ucfg api to check is tdls allowed or not
 * @vdev: vdev object
 *
 * Function determines the whether TDLS allowed in the system
 *
 * Return: true or false
 */
bool ucfg_tdls_check_is_tdls_allowed(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_tdls_set_user_tdls_enable() - ucfg api to set tdls is enable or not
 * from userspace
 * @vdev: vdev object
 * @is_user_tdls_enable: true if tdls is enabled from userspace
 *
 * Return: void
 */
void ucfg_tdls_set_user_tdls_enable(struct wlan_objmgr_vdev *vdev,
				    bool is_user_tdls_enable);

#else
static inline
bool ucfg_tdls_link_vdev_is_matching(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
struct wlan_objmgr_vdev *
ucfg_tdls_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev,
			     wlan_objmgr_ref_dbgid dbg_id)
{
	return NULL;
}

static inline
void ucfg_tdls_put_tdls_link_vdev(struct wlan_objmgr_vdev *vdev,
				  wlan_objmgr_ref_dbgid dbg_id)
{
}

static inline
QDF_STATUS ucfg_tdls_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_tdls_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_tdls_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_tdls_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_tdls_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_tdls_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr)
{
}

static inline
void ucfg_tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr)
{
}

static inline
QDF_STATUS ucfg_tdls_teardown_links(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_vdev *vdev)
{
}

static inline
QDF_STATUS ucfg_tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			      uint8_t *mac, int8_t rssi)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_tdls_notify_connect_failure(struct wlan_objmgr_psoc *psoc)
{
}

static inline
struct wlan_objmgr_vdev *ucfg_get_tdls_vdev(struct wlan_objmgr_psoc *psoc,
					    wlan_objmgr_ref_dbgid dbg_id)
{
	return NULL;
}

static inline
bool ucfg_tdls_check_is_tdls_allowed(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
void ucfg_tdls_set_user_tdls_enable(struct wlan_objmgr_vdev *vdev,
				    bool is_user_tdls_enable)
{
}

static inline
void ucfg_tdls_update_fw_11ax_capability(struct wlan_objmgr_psoc *psoc,
					 bool is_fw_tdls_11ax_capable)
{
}

static inline
bool  ucfg_tdls_is_fw_11ax_capable(struct wlan_objmgr_psoc *psoc)
{
return false;
}
#endif /* FEATURE_WLAN_TDLS */
#endif

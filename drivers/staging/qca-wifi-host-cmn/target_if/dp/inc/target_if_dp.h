/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: This target interface shall be used by DP
 *      to communicate with target using WMI.
 */

#ifndef _WLAN_TARGET_IF_DP_H_
#define _WLAN_TARGET_IF_DP_H_

#include <qdf_types.h>
#include <qdf_status.h>
#include <wmi_unified_priv.h>
#include <wlan_objmgr_psoc_obj.h>
#include <target_if.h>
#include <cdp_txrx_ops.h>

#define PEER_ROUTING_LMAC_ID_INDEX	6
#define PEER_ROUTING_LMAC_ID_BITS	2
/**
 * struct reorder_q_setup - reorder queue setup params
 * @psoc: psoc
 * @vdev_id: vdev id
 * @pdev_id: pdev id
 * @peer_macaddr: peer mac address
 * @hw_qdesc: hw queue descriptor
 * @tid: tid number
 * @queue_no: queue number
 * @ba_window_size_valid: BA window size validity flag
 * @ba_window_size: BA window size
 */
struct reorder_q_setup {
	struct cdp_ctrl_objmgr_psoc *psoc;
	uint8_t vdev_id;
	uint8_t pdev_id;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	qdf_dma_addr_t hw_qdesc_paddr;
	uint8_t tid;
	uint16_t queue_no;
	uint8_t ba_window_size_valid;
	uint16_t ba_window_size;
};

/**
 * target_if_get_active_mac_phy_number() - Get max MAC-PHY number enabled by
 * target
 * @psoc: psoc
 *
 * Get max active MAC-PHY number in all type of hw modes.
 *
 * return: active number of MAC-PHY pairs
 */
uint32_t target_if_get_active_mac_phy_number(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_peer_set_default_routing() - set peer default routing
 * @psoc: psoc pointer
 * @pdev_id: pdev id
 * @peer_macaddr: peer mac address
 * @vdev_id: vdev id
 * @hash_based: hash based routing
 * @ring_num: ring number
 * @lmac_peer_id_msb: lmac_peer_id_msb
 *
 * return: void
 */
void
target_if_peer_set_default_routing(struct cdp_ctrl_objmgr_psoc *psoc,
				   uint8_t pdev_id,
				   uint8_t *peer_macaddr, uint8_t vdev_id,
				   bool hash_based, uint8_t ring_num,
				   uint8_t lmac_peer_id_msb);
/**
 * target_if_peer_rx_reorder_queue_setup() - setup rx reorder queue
 * @pdev: pdev pointer
 * @pdev_id: pdev id
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @hw_qdesc: hw queue descriptor
 * @tid: tid number
 * @queue_no: queue number
 * @ba_window_size_valid: BA window size validity flag
 * @ba_window_size: BA window size
 *
 * return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
target_if_peer_rx_reorder_queue_setup(struct cdp_ctrl_objmgr_psoc *psoc,
				      uint8_t pdev_id,
				      uint8_t vdev_id, uint8_t *peer_macaddr,
				      qdf_dma_addr_t hw_qdesc, int tid,
				      uint16_t queue_no,
				      uint8_t ba_window_size_valid,
				      uint16_t ba_window_size);

/**
 * target_if_peer_rx_reorder_queue_remove() - remove rx reorder queue
 * @psoc: psoc pointer
 * @pdev_id: pdev id
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @peer_tid_bitmap: peer tid bitmap
 *
 * return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
target_if_peer_rx_reorder_queue_remove(struct cdp_ctrl_objmgr_psoc *psoc,
				       uint8_t pdev_id,
				       uint8_t vdev_id, uint8_t *peer_macaddr,
				       uint32_t peer_tid_bitmap);

/**
 * target_if_lro_hash_config() - send LRO hash config to FW
 * @psoc_handle: psoc handle pointer
 * @lro_hash_cfg: LRO hash config parameters
 *
 * return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
target_if_lro_hash_config(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
			  struct cdp_lro_hash_config *lro_hash_cfg);

#ifdef WLAN_SUPPORT_PPEDS
/**
 * target_if_peer_set_ppeds_default_routing() - Set PPE DS routing API
 * @soc: psoc handle pointer
 * @peer_macaddr: Peer MAC address
 * @service_code: Service code
 * @priority_valid: Priority valid field
 * @src_info: Source information
 * @vdev_id: VDEV ID
 * @use_ppe: use ppe field value
 * @ppe_routing_enabled: PPE routing enabled
 *
 * This API is used for setting PPE default routing configuration
 *
 * return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
target_if_peer_set_ppeds_default_routing(struct cdp_ctrl_objmgr_psoc *soc,
					 uint8_t *peer_macaddr,
					 uint16_t service_code,
					 uint8_t priority_valid,
					 uint16_t src_info,
					 uint8_t vdev_id, uint8_t use_ppe,
					 uint8_t ppe_routing_enabled);
#endif

#ifdef WDS_CONV_TARGET_IF_OPS_ENABLE
/**
 * target_if_add_wds_entry() - send wds peer add command to fw
 * @soc: SoC handle
 * @vdev_id: vdev_id
 * @peer_mac: peer mac address
 * @dest_mac: MAC address of ast node
 * @flags: WDS entry type WMI_HOST_WDS_FLAG_STATIC for static entry
 * @type: type from enum cdp_txrx_ast_entry_type
 *
 * This API is used by WDS source port learning function to
 * add a new AST entry in the fw.
 *
 * return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
target_if_add_wds_entry(struct cdp_ctrl_objmgr_psoc *soc, uint8_t vdev_id,
			uint8_t *peer_mac, const uint8_t *dest_mac,
			uint32_t flags, uint8_t type);

/**
 * target_if_del_wds_entry() - send wds peer del command to fw
 * @soc: SoC handle
 * @vdev_id: vdev_id
 * @dest_mac: MAC address of ast node
 * @type: type from enum cdp_txrx_ast_entry_type
 * @delete_in_fw: flag to indicate if entry needs to be deleted in fw
 *
 * This API is used to delete an AST entry from fw
 *
 * Return: None
 */
void
target_if_del_wds_entry(struct cdp_ctrl_objmgr_psoc *soc, uint8_t vdev_id,
			uint8_t *dest_mac, uint8_t type, uint8_t delete_in_fw);

/**
 * target_if_update_wds_entry() - send wds peer update command to fw
 * @soc: SoC handle
 * @vdev_id: vdev_id
 * @dest_mac: MAC address of ast node
 * @peer_mac: peer mac address
 * @flags: WDS entry type WMI_HOST_WDS_FLAG_STATIC for static entry
 *
 * This API is used by update the peer mac address for the ast
 * in the fw.
 *
 * return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
target_if_update_wds_entry(struct cdp_ctrl_objmgr_psoc *soc, uint8_t vdev_id,
			   uint8_t *dest_mac, uint8_t *peer_mac,
			   uint32_t flags);
#else
static inline QDF_STATUS
target_if_add_wds_entry(struct cdp_ctrl_objmgr_psoc *soc, uint8_t vdev_id,
			uint8_t *peer_mac, const uint8_t *dest_mac,
			uint32_t flags, uint8_t type)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
target_if_del_wds_entry(struct cdp_ctrl_objmgr_psoc *soc, uint8_t vdev_id,
			uint8_t *dest_mac, uint8_t type, uint8_t delete_in_fw)
{
}

static inline QDF_STATUS
target_if_update_wds_entry(struct cdp_ctrl_objmgr_psoc *soc, uint8_t vdev_id,
			   uint8_t *dest_mac, uint8_t *peer_mac,
			   uint32_t flags)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_MCL_REPEATER */

#ifdef WLAN_FEATURE_PEER_TXQ_FLUSH_CONF
/**
 * target_if_peer_txq_flush_config() - Send flush command for pending frames
 * @psoc: psoc handle pointer
 * @vdev_id: VDEV id
 * @mac: MAC addr of peer for which the tx queue flush is intended
 * @ac: AC mask for identifying the tx queues to be flushed
 * @tid: TID mask for identifying the tx queues to be flushed
 * @policy: Defines the flush policy
 *
 * Return: 0 for success or error code
 */
int target_if_peer_txq_flush_config(struct cdp_ctrl_objmgr_psoc *psoc,
				    uint8_t vdev_id, uint8_t *mac,
				    uint8_t ac, uint32_t tid, uint32_t policy);
#else
static inline int
target_if_peer_txq_flush_config(struct cdp_ctrl_objmgr_psoc *psoc,
				uint8_t vdev_id, uint8_t *mac,
				uint8_t ac, uint32_t tid,
				enum cdp_peer_txq_flush_policy policy)
{
	return 0;
}
#endif /* WLAN_FEATURE_PEER_TXQ_FLUSH_CONF */
#endif /* _WLAN_TARGET_IF_DP_H_ */

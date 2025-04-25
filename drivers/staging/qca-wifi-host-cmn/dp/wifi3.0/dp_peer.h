/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef _DP_PEER_H_
#define _DP_PEER_H_

#include <qdf_types.h>
#include <qdf_lock.h>
#include "dp_types.h"
#include "dp_internal.h"

#ifdef DUMP_REO_QUEUE_INFO_IN_DDR
#include "hal_reo.h"
#endif

#define DP_INVALID_PEER_ID 0xffff

#define DP_PEER_MAX_MEC_IDX 1024	/* maximum index for MEC table */
#define DP_PEER_MAX_MEC_ENTRY 4096	/* maximum MEC entries in MEC table */

#define DP_FW_PEER_STATS_CMP_TIMEOUT_MSEC 5000

#define DP_PEER_HASH_LOAD_MULT  2
#define DP_PEER_HASH_LOAD_SHIFT 0

/* Threshold for peer's cached buf queue beyond which frames are dropped */
#define DP_RX_CACHED_BUFQ_THRESH 64

#define dp_peer_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_DP_PEER, params)
#define dp_peer_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_DP_PEER, params)
#define dp_peer_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_DP_PEER, params)
#define dp_peer_info(params...) \
	__QDF_TRACE_FL(QDF_TRACE_LEVEL_INFO_HIGH, QDF_MODULE_ID_DP_PEER, ## params)
#define dp_peer_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_DP_PEER, params)

#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
/**
 * enum dp_bands - WiFi Band
 *
 * @DP_BAND_INVALID: Invalid band
 * @DP_BAND_2GHZ: 2GHz link
 * @DP_BAND_5GHZ: 5GHz link
 * @DP_BAND_6GHZ: 6GHz link
 * @DP_BAND_UNKNOWN: Unknown band
 */
enum dp_bands {
	DP_BAND_INVALID = 0,
	DP_BAND_2GHZ = 1,
	DP_BAND_5GHZ = 2,
	DP_BAND_6GHZ = 3,
	DP_BAND_UNKNOWN = 4,
};

/**
 * dp_freq_to_band() - Convert frequency to band
 * @freq: peer frequency
 *
 * Return: band for input frequency
 */
enum dp_bands dp_freq_to_band(qdf_freq_t freq);
#endif

void check_free_list_for_invalid_flush(struct dp_soc *soc);

static inline
void add_entry_alloc_list(struct dp_soc *soc, struct dp_rx_tid *rx_tid,
			  struct dp_peer *peer, void *hw_qdesc_vaddr)
{
	uint32_t max_list_size;
	unsigned long curr_ts = qdf_get_system_timestamp();
	uint32_t qref_index = soc->free_addr_list_idx;

	max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	if (max_list_size == 0)
		return;

	soc->list_qdesc_addr_alloc[qref_index].hw_qdesc_paddr =
							 rx_tid->hw_qdesc_paddr;
	soc->list_qdesc_addr_alloc[qref_index].ts_qdesc_mem_hdl = curr_ts;
	soc->list_qdesc_addr_alloc[qref_index].hw_qdesc_vaddr_align =
								 hw_qdesc_vaddr;
	soc->list_qdesc_addr_alloc[qref_index].hw_qdesc_vaddr_unalign =
					       rx_tid->hw_qdesc_vaddr_unaligned;
	soc->list_qdesc_addr_alloc[qref_index].peer_id = peer->peer_id;
	soc->list_qdesc_addr_alloc[qref_index].tid = rx_tid->tid;
	soc->alloc_addr_list_idx++;

	if (soc->alloc_addr_list_idx == max_list_size)
		soc->alloc_addr_list_idx = 0;
}

static inline
void add_entry_free_list(struct dp_soc *soc, struct dp_rx_tid *rx_tid)
{
	uint32_t max_list_size;
	unsigned long curr_ts = qdf_get_system_timestamp();
	uint32_t qref_index = soc->free_addr_list_idx;

	max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	if (max_list_size == 0)
		return;

	soc->list_qdesc_addr_free[qref_index].ts_qdesc_mem_hdl = curr_ts;
	soc->list_qdesc_addr_free[qref_index].hw_qdesc_paddr =
							 rx_tid->hw_qdesc_paddr;
	soc->list_qdesc_addr_free[qref_index].hw_qdesc_vaddr_align =
						 rx_tid->hw_qdesc_vaddr_aligned;
	soc->list_qdesc_addr_free[qref_index].hw_qdesc_vaddr_unalign =
					       rx_tid->hw_qdesc_vaddr_unaligned;
	soc->free_addr_list_idx++;

	if (soc->free_addr_list_idx == max_list_size)
		soc->free_addr_list_idx = 0;
}

static inline
void add_entry_write_list(struct dp_soc *soc, struct dp_peer *peer,
			  uint32_t tid)
{
	uint32_t max_list_size;
	unsigned long curr_ts = qdf_get_system_timestamp();

	max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	if (max_list_size == 0)
		return;

	soc->reo_write_list[soc->write_paddr_list_idx].ts_qaddr_del = curr_ts;
	soc->reo_write_list[soc->write_paddr_list_idx].peer_id = peer->peer_id;
	soc->reo_write_list[soc->write_paddr_list_idx].paddr =
					       peer->rx_tid[tid].hw_qdesc_paddr;
	soc->reo_write_list[soc->write_paddr_list_idx].tid = tid;
	soc->write_paddr_list_idx++;

	if (soc->write_paddr_list_idx == max_list_size)
		soc->write_paddr_list_idx = 0;
}

#ifdef REO_QDESC_HISTORY
enum reo_qdesc_event_type {
	REO_QDESC_UPDATE_CB = 0,
	REO_QDESC_FREE,
};

struct reo_qdesc_event {
	qdf_dma_addr_t qdesc_addr;
	uint64_t ts;
	enum reo_qdesc_event_type type;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
};
#endif

struct ast_del_ctxt {
	bool age;
	int del_count;
};

#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * dp_peer_is_wds_ext_peer() - peer is WDS_EXT peer
 *
 * @peer: DP peer context
 *
 * This API checks whether the peer is WDS_EXT peer or not
 *
 * Return: true in the wds_ext peer else flase
 */
static inline bool dp_peer_is_wds_ext_peer(struct dp_txrx_peer *peer)
{
	return qdf_atomic_test_bit(WDS_EXT_PEER_INIT_BIT, &peer->wds_ext.init);
}
#else
static inline bool dp_peer_is_wds_ext_peer(struct dp_txrx_peer *peer)
{
	return false;
}
#endif

typedef void dp_peer_iter_func(struct dp_soc *soc, struct dp_peer *peer,
			       void *arg);
/**
 * dp_peer_unref_delete() - unref and delete peer
 * @peer: Datapath peer handle
 * @id: ID of module releasing reference
 *
 */
void dp_peer_unref_delete(struct dp_peer *peer, enum dp_mod_id id);

/**
 * dp_txrx_peer_unref_delete() - unref and delete peer
 * @handle: Datapath txrx ref handle
 * @id: Module ID of the caller
 *
 */
void dp_txrx_peer_unref_delete(dp_txrx_ref_handle handle, enum dp_mod_id id);

/**
 * dp_peer_find_hash_find() - returns legacy or mlo link peer from
 *			      peer_hash_table matching vdev_id and mac_address
 * @soc: soc handle
 * @peer_mac_addr: peer mac address
 * @mac_addr_is_aligned: is mac addr aligned
 * @vdev_id: vdev_id
 * @mod_id: id of module requesting reference
 *
 * return: peer in success
 *         NULL in failure
 */
struct dp_peer *dp_peer_find_hash_find(struct dp_soc *soc,
				       uint8_t *peer_mac_addr,
				       int mac_addr_is_aligned,
				       uint8_t vdev_id,
				       enum dp_mod_id mod_id);

/**
 * dp_peer_find_by_id_valid - check if peer exists for given id
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 *
 * Return: true if peer exists of false otherwise
 */
bool dp_peer_find_by_id_valid(struct dp_soc *soc, uint16_t peer_id);

/**
 * dp_peer_get_ref() - Returns peer object given the peer id
 *
 * @soc: core DP soc context
 * @peer: DP peer
 * @mod_id: id of module requesting the reference
 *
 * Return:	QDF_STATUS_SUCCESS if reference held successfully
 *		else QDF_STATUS_E_INVAL
 */
static inline
QDF_STATUS dp_peer_get_ref(struct dp_soc *soc,
			   struct dp_peer *peer,
			   enum dp_mod_id mod_id)
{
	if (!qdf_atomic_inc_not_zero(&peer->ref_cnt))
		return QDF_STATUS_E_INVAL;

	if (mod_id > DP_MOD_ID_RX)
		qdf_atomic_inc(&peer->mod_refs[mod_id]);

	return QDF_STATUS_SUCCESS;
}

/**
 * __dp_peer_get_ref_by_id() - Returns peer object given the peer id
 *
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 * @mod_id: module id
 *
 * Return: struct dp_peer*: Pointer to DP peer object
 */
static inline struct dp_peer *
__dp_peer_get_ref_by_id(struct dp_soc *soc,
			uint16_t peer_id,
			enum dp_mod_id mod_id)

{
	struct dp_peer *peer;

	qdf_spin_lock_bh(&soc->peer_map_lock);
	peer = (peer_id >= soc->max_peer_id) ? NULL :
				soc->peer_id_to_obj_map[peer_id];
	if (!peer ||
	    (dp_peer_get_ref(soc, peer, mod_id) != QDF_STATUS_SUCCESS)) {
		qdf_spin_unlock_bh(&soc->peer_map_lock);
		return NULL;
	}

	qdf_spin_unlock_bh(&soc->peer_map_lock);
	return peer;
}

/**
 * dp_peer_get_ref_by_id() - Returns peer object given the peer id
 *                        if peer state is active
 *
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 * @mod_id: ID of module requesting reference
 *
 * Return: struct dp_peer*: Pointer to DP peer object
 */
static inline
struct dp_peer *dp_peer_get_ref_by_id(struct dp_soc *soc,
				      uint16_t peer_id,
				      enum dp_mod_id mod_id)
{
	struct dp_peer *peer;

	qdf_spin_lock_bh(&soc->peer_map_lock);
	peer = (peer_id >= soc->max_peer_id) ? NULL :
				soc->peer_id_to_obj_map[peer_id];

	if (!peer || peer->peer_state >= DP_PEER_STATE_LOGICAL_DELETE ||
	    (dp_peer_get_ref(soc, peer, mod_id) != QDF_STATUS_SUCCESS)) {
		qdf_spin_unlock_bh(&soc->peer_map_lock);
		return NULL;
	}

	qdf_spin_unlock_bh(&soc->peer_map_lock);

	return peer;
}

/**
 * dp_txrx_peer_get_ref_by_id() - Returns txrx peer object given the peer id
 *
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 * @handle: reference handle
 * @mod_id: ID of module requesting reference
 *
 * Return: struct dp_txrx_peer*: Pointer to txrx DP peer object
 */
static inline struct dp_txrx_peer *
dp_txrx_peer_get_ref_by_id(struct dp_soc *soc,
			   uint16_t peer_id,
			   dp_txrx_ref_handle *handle,
			   enum dp_mod_id mod_id)

{
	struct dp_peer *peer;

	peer = dp_peer_get_ref_by_id(soc, peer_id, mod_id);
	if (!peer)
		return NULL;

	if (!peer->txrx_peer) {
		dp_peer_unref_delete(peer, mod_id);
		return NULL;
	}

	*handle = (dp_txrx_ref_handle)peer;
	return peer->txrx_peer;
}

#ifdef PEER_CACHE_RX_PKTS
/**
 * dp_rx_flush_rx_cached() - flush cached rx frames
 * @peer: peer
 * @drop: set flag to drop frames
 *
 * Return: None
 */
void dp_rx_flush_rx_cached(struct dp_peer *peer, bool drop);
#else
static inline void dp_rx_flush_rx_cached(struct dp_peer *peer, bool drop)
{
}
#endif

static inline void
dp_clear_peer_internal(struct dp_soc *soc, struct dp_peer *peer)
{
	qdf_spin_lock_bh(&peer->peer_info_lock);
	peer->state = OL_TXRX_PEER_STATE_DISC;
	qdf_spin_unlock_bh(&peer->peer_info_lock);

	dp_rx_flush_rx_cached(peer, true);
}

/**
 * dp_vdev_iterate_peer() - API to iterate through vdev peer list
 *
 * @vdev: DP vdev context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 *
 * Return: void
 */
static inline void
dp_vdev_iterate_peer(struct dp_vdev *vdev, dp_peer_iter_func *func, void *arg,
		     enum dp_mod_id mod_id)
{
	struct dp_peer *peer;
	struct dp_peer *tmp_peer;
	struct dp_soc *soc = NULL;

	if (!vdev || !vdev->pdev || !vdev->pdev->soc)
		return;

	soc = vdev->pdev->soc;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH_SAFE(peer, &vdev->peer_list,
			   peer_list_elem,
			   tmp_peer) {
		if (dp_peer_get_ref(soc, peer, mod_id) ==
					QDF_STATUS_SUCCESS) {
			(*func)(soc, peer, arg);
			dp_peer_unref_delete(peer, mod_id);
		}
	}
	qdf_spin_unlock_bh(&vdev->peer_list_lock);
}

/**
 * dp_pdev_iterate_peer() - API to iterate through all peers of pdev
 *
 * @pdev: DP pdev context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 *
 * Return: void
 */
static inline void
dp_pdev_iterate_peer(struct dp_pdev *pdev, dp_peer_iter_func *func, void *arg,
		     enum dp_mod_id mod_id)
{
	struct dp_vdev *vdev;

	if (!pdev)
		return;

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	DP_PDEV_ITERATE_VDEV_LIST(pdev, vdev)
		dp_vdev_iterate_peer(vdev, func, arg, mod_id);
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);
}

/**
 * dp_soc_iterate_peer() - API to iterate through all peers of soc
 *
 * @soc: DP soc context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 *
 * Return: void
 */
static inline void
dp_soc_iterate_peer(struct dp_soc *soc, dp_peer_iter_func *func, void *arg,
		    enum dp_mod_id mod_id)
{
	struct dp_pdev *pdev;
	int i;

	if (!soc)
		return;

	for (i = 0; i < MAX_PDEV_CNT && soc->pdev_list[i]; i++) {
		pdev = soc->pdev_list[i];
		dp_pdev_iterate_peer(pdev, func, arg, mod_id);
	}
}

/**
 * dp_vdev_iterate_peer_lock_safe() - API to iterate through vdev list
 *
 * This API will cache the peers in local allocated memory and calls
 * iterate function outside the lock.
 *
 * As this API is allocating new memory it is suggested to use this
 * only when lock cannot be held
 *
 * @vdev: DP vdev context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 *
 * Return: void
 */
static inline void
dp_vdev_iterate_peer_lock_safe(struct dp_vdev *vdev,
			       dp_peer_iter_func *func,
			       void *arg,
			       enum dp_mod_id mod_id)
{
	struct dp_peer *peer;
	struct dp_peer *tmp_peer;
	struct dp_soc *soc = NULL;
	struct dp_peer **peer_array = NULL;
	int i = 0;
	uint32_t num_peers = 0;

	if (!vdev || !vdev->pdev || !vdev->pdev->soc)
		return;

	num_peers = vdev->num_peers;

	soc = vdev->pdev->soc;

	peer_array = qdf_mem_malloc(num_peers * sizeof(struct dp_peer *));
	if (!peer_array)
		return;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH_SAFE(peer, &vdev->peer_list,
			   peer_list_elem,
			   tmp_peer) {
		if (i >= num_peers)
			break;

		if (dp_peer_get_ref(soc, peer, mod_id) == QDF_STATUS_SUCCESS) {
			peer_array[i] = peer;
			i = (i + 1);
		}
	}
	qdf_spin_unlock_bh(&vdev->peer_list_lock);

	for (i = 0; i < num_peers; i++) {
		peer = peer_array[i];

		if (!peer)
			continue;

		(*func)(soc, peer, arg);
		dp_peer_unref_delete(peer, mod_id);
	}

	qdf_mem_free(peer_array);
}

/**
 * dp_pdev_iterate_peer_lock_safe() - API to iterate through all peers of pdev
 *
 * This API will cache the peers in local allocated memory and calls
 * iterate function outside the lock.
 *
 * As this API is allocating new memory it is suggested to use this
 * only when lock cannot be held
 *
 * @pdev: DP pdev context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 *
 * Return: void
 */
static inline void
dp_pdev_iterate_peer_lock_safe(struct dp_pdev *pdev,
			       dp_peer_iter_func *func,
			       void *arg,
			       enum dp_mod_id mod_id)
{
	struct dp_peer *peer;
	struct dp_peer *tmp_peer;
	struct dp_soc *soc = NULL;
	struct dp_vdev *vdev = NULL;
	struct dp_peer **peer_array[DP_PDEV_MAX_VDEVS] = {0};
	int i = 0;
	int j = 0;
	uint32_t num_peers[DP_PDEV_MAX_VDEVS] = {0};

	if (!pdev || !pdev->soc)
		return;

	soc = pdev->soc;

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	DP_PDEV_ITERATE_VDEV_LIST(pdev, vdev) {
		num_peers[i] = vdev->num_peers;
		peer_array[i] = qdf_mem_malloc(num_peers[i] *
					       sizeof(struct dp_peer *));
		if (!peer_array[i])
			break;

		qdf_spin_lock_bh(&vdev->peer_list_lock);
		TAILQ_FOREACH_SAFE(peer, &vdev->peer_list,
				   peer_list_elem,
				   tmp_peer) {
			if (j >= num_peers[i])
				break;

			if (dp_peer_get_ref(soc, peer, mod_id) ==
					QDF_STATUS_SUCCESS) {
				peer_array[i][j] = peer;

				j = (j + 1);
			}
		}
		qdf_spin_unlock_bh(&vdev->peer_list_lock);
		i = (i + 1);
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);

	for (i = 0; i < DP_PDEV_MAX_VDEVS; i++) {
		if (!peer_array[i])
			break;

		for (j = 0; j < num_peers[i]; j++) {
			peer = peer_array[i][j];

			if (!peer)
				continue;

			(*func)(soc, peer, arg);
			dp_peer_unref_delete(peer, mod_id);
		}

		qdf_mem_free(peer_array[i]);
	}
}

/**
 * dp_soc_iterate_peer_lock_safe() - API to iterate through all peers of soc
 *
 * This API will cache the peers in local allocated memory and calls
 * iterate function outside the lock.
 *
 * As this API is allocating new memory it is suggested to use this
 * only when lock cannot be held
 *
 * @soc: DP soc context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 *
 * Return: void
 */
static inline void
dp_soc_iterate_peer_lock_safe(struct dp_soc *soc,
			      dp_peer_iter_func *func,
			      void *arg,
			      enum dp_mod_id mod_id)
{
	struct dp_pdev *pdev;
	int i;

	if (!soc)
		return;

	for (i = 0; i < MAX_PDEV_CNT && soc->pdev_list[i]; i++) {
		pdev = soc->pdev_list[i];
		dp_pdev_iterate_peer_lock_safe(pdev, func, arg, mod_id);
	}
}

#ifdef DP_PEER_STATE_DEBUG
#define DP_PEER_STATE_ASSERT(_peer, _new_state, _condition) \
	do {  \
		if (!(_condition)) { \
			dp_alert("Invalid state shift from %u to %u peer " \
				 QDF_MAC_ADDR_FMT, \
				 (_peer)->peer_state, (_new_state), \
				 QDF_MAC_ADDR_REF((_peer)->mac_addr.raw)); \
			QDF_ASSERT(0); \
		} \
	} while (0)

#else
#define DP_PEER_STATE_ASSERT(_peer, _new_state, _condition) \
	do {  \
		if (!(_condition)) { \
			dp_alert("Invalid state shift from %u to %u peer " \
				 QDF_MAC_ADDR_FMT, \
				 (_peer)->peer_state, (_new_state), \
				 QDF_MAC_ADDR_REF((_peer)->mac_addr.raw)); \
		} \
	} while (0)
#endif

/**
 * dp_peer_state_cmp() - compare dp peer state
 *
 * @peer: DP peer
 * @state: state
 *
 * Return: true if state matches with peer state
 *	   false if it does not match
 */
static inline bool
dp_peer_state_cmp(struct dp_peer *peer,
		  enum dp_peer_state state)
{
	bool is_status_equal = false;

	qdf_spin_lock_bh(&peer->peer_state_lock);
	is_status_equal = (peer->peer_state == state);
	qdf_spin_unlock_bh(&peer->peer_state_lock);

	return is_status_equal;
}

/**
 * dp_print_ast_stats() - Dump AST table contents
 * @soc: Datapath soc handle
 *
 * Return: void
 */
void dp_print_ast_stats(struct dp_soc *soc);

/**
 * dp_rx_peer_map_handler() - handle peer map event from firmware
 * @soc: generic soc handle
 * @peer_id: peer_id from firmware
 * @hw_peer_id: ast index for this peer
 * @vdev_id: vdev ID
 * @peer_mac_addr: mac address of the peer
 * @ast_hash: ast hash value
 * @is_wds: flag to indicate peer map event for WDS ast entry
 *
 * associate the peer_id that firmware provided with peer entry
 * and update the ast table in the host with the hw_peer_id.
 *
 * Return: QDF_STATUS code
 */

QDF_STATUS dp_rx_peer_map_handler(struct dp_soc *soc, uint16_t peer_id,
				  uint16_t hw_peer_id, uint8_t vdev_id,
				  uint8_t *peer_mac_addr, uint16_t ast_hash,
				  uint8_t is_wds);

/**
 * dp_rx_peer_unmap_handler() - handle peer unmap event from firmware
 * @soc: generic soc handle
 * @peer_id: peer_id from firmware
 * @vdev_id: vdev ID
 * @peer_mac_addr: mac address of the peer or wds entry
 * @is_wds: flag to indicate peer map event for WDS ast entry
 * @free_wds_count: number of wds entries freed by FW with peer delete
 *
 * Return: none
 */
void dp_rx_peer_unmap_handler(struct dp_soc *soc, uint16_t peer_id,
			      uint8_t vdev_id, uint8_t *peer_mac_addr,
			      uint8_t is_wds, uint32_t free_wds_count);

#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
/**
 * dp_rx_peer_ext_evt() - handle peer extended event from firmware
 * @soc: DP soc handle
 * @info: extended evt info
 *
 *
 * Return: QDF_STATUS
 */

QDF_STATUS
dp_rx_peer_ext_evt(struct dp_soc *soc, struct dp_peer_ext_evt_info *info);
#endif
#ifdef DP_RX_UDP_OVER_PEER_ROAM
/**
 * dp_rx_reset_roaming_peer() - Reset the roamed peer in vdev
 * @soc: dp soc pointer
 * @vdev_id: vdev id
 * @peer_mac_addr: mac address of the peer
 *
 * This function resets the roamed peer auth status and mac address
 * after peer map indication of same peer is received from firmware.
 *
 * Return: None
 */
void dp_rx_reset_roaming_peer(struct dp_soc *soc, uint8_t vdev_id,
			      uint8_t *peer_mac_addr);
#else
static inline void dp_rx_reset_roaming_peer(struct dp_soc *soc, uint8_t vdev_id,
					    uint8_t *peer_mac_addr)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_rx_mlo_peer_map_handler() - handle MLO peer map event from firmware
 * @soc: generic soc handle
 * @peer_id: ML peer_id from firmware
 * @peer_mac_addr: mac address of the peer
 * @mlo_flow_info: MLO AST flow info
 * @mlo_link_info: MLO link info
 *
 * associate the ML peer_id that firmware provided with peer entry
 * and update the ast table in the host with the hw_peer_id.
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS
dp_rx_mlo_peer_map_handler(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t *peer_mac_addr,
			   struct dp_mlo_flow_override_info *mlo_flow_info,
			   struct dp_mlo_link_info *mlo_link_info);

/**
 * dp_rx_mlo_peer_unmap_handler() - handle MLO peer unmap event from firmware
 * @soc: generic soc handle
 * @peer_id: peer_id from firmware
 *
 * Return: none
 */
void dp_rx_mlo_peer_unmap_handler(struct dp_soc *soc, uint16_t peer_id);
#endif

void dp_rx_sec_ind_handler(struct dp_soc *soc, uint16_t peer_id,
			   enum cdp_sec_type sec_type, int is_unicast,
			   u_int32_t *michael_key, u_int32_t *rx_pn);

uint8_t dp_get_peer_mac_addr_frm_id(struct cdp_soc_t *soc_handle,
		uint16_t peer_id, uint8_t *peer_mac);

/**
 * dp_peer_add_ast() - Allocate and add AST entry into peer list
 * @soc: SoC handle
 * @peer: peer to which ast node belongs
 * @mac_addr: MAC address of ast node
 * @type: AST entry type
 * @flags: AST configuration flags
 *
 * This API is used by WDS source port learning function to
 * add a new AST entry into peer AST list
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS dp_peer_add_ast(struct dp_soc *soc, struct dp_peer *peer,
			   uint8_t *mac_addr, enum cdp_txrx_ast_entry_type type,
			   uint32_t flags);

/**
 * dp_peer_del_ast() - Delete and free AST entry
 * @soc: SoC handle
 * @ast_entry: AST entry of the node
 *
 * This function removes the AST entry from peer and soc tables
 * It assumes caller has taken the ast lock to protect the access to these
 * tables
 *
 * Return: None
 */
void dp_peer_del_ast(struct dp_soc *soc, struct dp_ast_entry *ast_entry);

void dp_peer_ast_unmap_handler(struct dp_soc *soc,
			       struct dp_ast_entry *ast_entry);

/**
 * dp_peer_update_ast() - Delete and free AST entry
 * @soc: SoC handle
 * @peer: peer to which ast node belongs
 * @ast_entry: AST entry of the node
 * @flags: wds or hmwds
 *
 * This function update the AST entry to the roamed peer and soc tables
 * It assumes caller has taken the ast lock to protect the access to these
 * tables
 *
 * Return: 0 if ast entry is updated successfully
 *         -1 failure
 */
int dp_peer_update_ast(struct dp_soc *soc, struct dp_peer *peer,
			struct dp_ast_entry *ast_entry,	uint32_t flags);

/**
 * dp_peer_ast_hash_find_by_pdevid() - Find AST entry by MAC address
 * @soc: SoC handle
 * @ast_mac_addr: Mac address
 * @pdev_id: pdev Id
 *
 * It assumes caller has taken the ast lock to protect the access to
 * AST hash table
 *
 * Return: AST entry
 */
struct dp_ast_entry *dp_peer_ast_hash_find_by_pdevid(struct dp_soc *soc,
						     uint8_t *ast_mac_addr,
						     uint8_t pdev_id);

/**
 * dp_peer_ast_hash_find_by_vdevid() - Find AST entry by MAC address
 * @soc: SoC handle
 * @ast_mac_addr: Mac address
 * @vdev_id: vdev Id
 *
 * It assumes caller has taken the ast lock to protect the access to
 * AST hash table
 *
 * Return: AST entry
 */
struct dp_ast_entry *dp_peer_ast_hash_find_by_vdevid(struct dp_soc *soc,
						     uint8_t *ast_mac_addr,
						     uint8_t vdev_id);

/**
 * dp_peer_ast_hash_find_soc() - Find AST entry by MAC address
 * @soc: SoC handle
 * @ast_mac_addr: Mac address
 *
 * It assumes caller has taken the ast lock to protect the access to
 * AST hash table
 *
 * Return: AST entry
 */
struct dp_ast_entry *dp_peer_ast_hash_find_soc(struct dp_soc *soc,
					       uint8_t *ast_mac_addr);

/**
 * dp_peer_ast_hash_find_soc_by_type() - Find AST entry by MAC address
 * and AST type
 * @soc: SoC handle
 * @ast_mac_addr: Mac address
 * @type: AST entry type
 *
 * It assumes caller has taken the ast lock to protect the access to
 * AST hash table
 *
 * Return: AST entry
 */
struct dp_ast_entry *dp_peer_ast_hash_find_soc_by_type(
					struct dp_soc *soc,
					uint8_t *ast_mac_addr,
					enum cdp_txrx_ast_entry_type type);

/**
 * dp_peer_ast_get_pdev_id() - get pdev_id from the ast entry
 * @soc: SoC handle
 * @ast_entry: AST entry of the node
 *
 * This function gets the pdev_id from the ast entry.
 *
 * Return: (uint8_t) pdev_id
 */
uint8_t dp_peer_ast_get_pdev_id(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry);


/**
 * dp_peer_ast_get_next_hop() - get next_hop from the ast entry
 * @soc: SoC handle
 * @ast_entry: AST entry of the node
 *
 * This function gets the next hop from the ast entry.
 *
 * Return: (uint8_t) next_hop
 */
uint8_t dp_peer_ast_get_next_hop(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry);

/**
 * dp_peer_ast_set_type() - set type from the ast entry
 * @soc: SoC handle
 * @ast_entry: AST entry of the node
 * @type: AST entry type
 *
 * This function sets the type in the ast entry.
 *
 * Return:
 */
void dp_peer_ast_set_type(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry,
				enum cdp_txrx_ast_entry_type type);

void dp_peer_ast_send_wds_del(struct dp_soc *soc,
			      struct dp_ast_entry *ast_entry,
			      struct dp_peer *peer);

#ifdef WLAN_FEATURE_MULTI_AST_DEL
void dp_peer_ast_send_multi_wds_del(
		struct dp_soc *soc, uint8_t vdev_id,
		struct peer_del_multi_wds_entries *wds_list);
#endif

void dp_peer_free_hmwds_cb(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			   struct cdp_soc *dp_soc,
			   void *cookie,
			   enum cdp_ast_free_status status);

/**
 * dp_peer_ast_hash_remove() - Look up and remove AST entry from hash table
 * @soc: SoC handle
 * @ase: Address search entry
 *
 * This function removes the AST entry from soc AST hash table
 * It assumes caller has taken the ast lock to protect the access to this table
 *
 * Return: None
 */
void dp_peer_ast_hash_remove(struct dp_soc *soc,
			     struct dp_ast_entry *ase);

/**
 * dp_peer_free_ast_entry() - Free up the ast entry memory
 * @soc: SoC handle
 * @ast_entry: Address search entry
 *
 * This API is used to free up the memory associated with
 * AST entry.
 *
 * Return: None
 */
void dp_peer_free_ast_entry(struct dp_soc *soc,
			    struct dp_ast_entry *ast_entry);

/**
 * dp_peer_unlink_ast_entry() - Free up the ast entry memory
 * @soc: SoC handle
 * @ast_entry: Address search entry
 * @peer: peer
 *
 * This API is used to remove/unlink AST entry from the peer list
 * and hash list.
 *
 * Return: None
 */
void dp_peer_unlink_ast_entry(struct dp_soc *soc,
			      struct dp_ast_entry *ast_entry,
			      struct dp_peer *peer);

/**
 * dp_peer_mec_detach_entry() - Detach the MEC entry
 * @soc: SoC handle
 * @mecentry: MEC entry of the node
 * @ptr: pointer to free list
 *
 * The MEC entry is detached from MEC table and added to free_list
 * to free the object outside lock
 *
 * Return: None
 */
void dp_peer_mec_detach_entry(struct dp_soc *soc, struct dp_mec_entry *mecentry,
			      void *ptr);

/**
 * dp_peer_mec_free_list() - free the MEC entry from free_list
 * @soc: SoC handle
 * @ptr: pointer to free list
 *
 * Return: None
 */
void dp_peer_mec_free_list(struct dp_soc *soc, void *ptr);

/**
 * dp_peer_mec_add_entry()
 * @soc: SoC handle
 * @vdev: vdev to which mec node belongs
 * @mac_addr: MAC address of mec node
 *
 * This function allocates and adds MEC entry to MEC table.
 * It assumes caller has taken the mec lock to protect the access to these
 * tables
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_peer_mec_add_entry(struct dp_soc *soc,
				 struct dp_vdev *vdev,
				 uint8_t *mac_addr);

/**
 * dp_peer_mec_hash_find_by_pdevid() - Find MEC entry by PDEV Id
 * within pdev
 * @soc: SoC handle
 * @pdev_id: pdev Id
 * @mec_mac_addr: MAC address of mec node
 *
 * It assumes caller has taken the mec_lock to protect the access to
 * MEC hash table
 *
 * Return: MEC entry
 */
struct dp_mec_entry *dp_peer_mec_hash_find_by_pdevid(struct dp_soc *soc,
						     uint8_t pdev_id,
						     uint8_t *mec_mac_addr);

#define DP_AST_ASSERT(_condition) \
	do { \
		if (!(_condition)) { \
			dp_print_ast_stats(soc);\
			QDF_BUG(_condition); \
		} \
	} while (0)

/**
 * dp_peer_update_inactive_time() - Update inactive time for peer
 * @pdev: pdev object
 * @tag_type: htt_tlv_tag type
 * @tag_buf: buf message
 */
void
dp_peer_update_inactive_time(struct dp_pdev *pdev, uint32_t tag_type,
			     uint32_t *tag_buf);

#ifndef QCA_MULTIPASS_SUPPORT
static inline
/**
 * dp_peer_set_vlan_id() - set vlan_id for this peer
 * @cdp_soc: soc handle
 * @vdev_id: id of vdev object
 * @peer_mac: mac address
 * @vlan_id: vlan id for peer
 *
 * Return: void
 */
void dp_peer_set_vlan_id(struct cdp_soc_t *cdp_soc,
			 uint8_t vdev_id, uint8_t *peer_mac,
			 uint16_t vlan_id)
{
}

/**
 * dp_set_vlan_groupkey() - set vlan map for vdev
 * @soc_hdl: pointer to soc
 * @vdev_id: id of vdev handle
 * @vlan_id: vlan_id
 * @group_key: group key for vlan
 *
 * Return: set success/failure
 */
static inline
QDF_STATUS dp_set_vlan_groupkey(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				uint16_t vlan_id, uint16_t group_key)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_peer_multipass_list_init() - initialize multipass peer list
 * @vdev: pointer to vdev
 *
 * Return: void
 */
static inline
void dp_peer_multipass_list_init(struct dp_vdev *vdev)
{
}

/**
 * dp_peer_multipass_list_remove() - remove peer from special peer list
 * @peer: peer handle
 *
 * Return: void
 */
static inline
void dp_peer_multipass_list_remove(struct dp_peer *peer)
{
}
#else
void dp_peer_set_vlan_id(struct cdp_soc_t *cdp_soc,
			 uint8_t vdev_id, uint8_t *peer_mac,
			 uint16_t vlan_id);
QDF_STATUS dp_set_vlan_groupkey(struct cdp_soc_t *soc, uint8_t vdev_id,
				uint16_t vlan_id, uint16_t group_key);
void dp_peer_multipass_list_init(struct dp_vdev *vdev);
void dp_peer_multipass_list_remove(struct dp_peer *peer);
#endif


#ifndef QCA_PEER_MULTIQ_SUPPORT
/**
 * dp_peer_reset_flowq_map() - reset peer flowq map table
 * @peer: dp peer handle
 *
 * Return: none
 */
static inline
void dp_peer_reset_flowq_map(struct dp_peer *peer)
{
}

/**
 * dp_peer_ast_index_flow_queue_map_create() - create ast index flow queue map
 * @soc_hdl: generic soc handle
 * @is_wds: flag to indicate if peer is wds
 * @peer_id: peer_id from htt peer map message
 * @peer_mac_addr: mac address of the peer
 * @ast_info: ast flow override information from peer map
 *
 * Return: none
 */
static inline
void dp_peer_ast_index_flow_queue_map_create(void *soc_hdl,
		    bool is_wds, uint16_t peer_id, uint8_t *peer_mac_addr,
		    struct dp_ast_flow_override_info *ast_info)
{
}
#else
void dp_peer_reset_flowq_map(struct dp_peer *peer);

void dp_peer_ast_index_flow_queue_map_create(void *soc_hdl,
		    bool is_wds, uint16_t peer_id, uint8_t *peer_mac_addr,
		    struct dp_ast_flow_override_info *ast_info);
#endif

#ifdef QCA_PEER_EXT_STATS
/**
 * dp_peer_delay_stats_ctx_alloc() - Allocate peer delay stats content
 * @soc: DP SoC context
 * @txrx_peer: DP txrx peer context
 *
 * Allocate the peer delay stats context
 *
 * Return: QDF_STATUS_SUCCESS if allocation is
 *	   successful
 */
QDF_STATUS dp_peer_delay_stats_ctx_alloc(struct dp_soc *soc,
					 struct dp_txrx_peer *txrx_peer);

/**
 * dp_peer_delay_stats_ctx_dealloc() - Dealloc the peer delay stats context
 * @soc: DP SoC context
 * @txrx_peer: txrx DP peer context
 *
 * Free the peer delay stats context
 *
 * Return: Void
 */
void dp_peer_delay_stats_ctx_dealloc(struct dp_soc *soc,
				     struct dp_txrx_peer *txrx_peer);

/**
 * dp_peer_delay_stats_ctx_clr() - Clear delay stats context of peer
 * @txrx_peer: dp_txrx_peer handle
 *
 * Return: void
 */
void dp_peer_delay_stats_ctx_clr(struct dp_txrx_peer *txrx_peer);
#else
static inline
QDF_STATUS dp_peer_delay_stats_ctx_alloc(struct dp_soc *soc,
					 struct dp_txrx_peer *txrx_peer)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_peer_delay_stats_ctx_dealloc(struct dp_soc *soc,
				     struct dp_txrx_peer *txrx_peer)
{
}

static inline
void dp_peer_delay_stats_ctx_clr(struct dp_txrx_peer *txrx_peer)
{
}
#endif

#ifdef WLAN_PEER_JITTER
/**
 * dp_peer_jitter_stats_ctx_alloc() - Allocate jitter stats context for peer
 * @pdev: Datapath pdev handle
 * @txrx_peer: dp_txrx_peer handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_peer_jitter_stats_ctx_alloc(struct dp_pdev *pdev,
					  struct dp_txrx_peer *txrx_peer);

/**
 * dp_peer_jitter_stats_ctx_dealloc() - Deallocate jitter stats context
 * @pdev: Datapath pdev handle
 * @txrx_peer: dp_txrx_peer handle
 *
 * Return: void
 */
void dp_peer_jitter_stats_ctx_dealloc(struct dp_pdev *pdev,
				      struct dp_txrx_peer *txrx_peer);

/**
 * dp_peer_jitter_stats_ctx_clr() - Clear jitter stats context of peer
 * @txrx_peer: dp_txrx_peer handle
 *
 * Return: void
 */
void dp_peer_jitter_stats_ctx_clr(struct dp_txrx_peer *txrx_peer);
#else
static inline
QDF_STATUS dp_peer_jitter_stats_ctx_alloc(struct dp_pdev *pdev,
					  struct dp_txrx_peer *txrx_peer)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_peer_jitter_stats_ctx_dealloc(struct dp_pdev *pdev,
				      struct dp_txrx_peer *txrx_peer)
{
}

static inline
void dp_peer_jitter_stats_ctx_clr(struct dp_txrx_peer *txrx_peer)
{
}
#endif

#ifndef CONFIG_SAWF_DEF_QUEUES
static inline QDF_STATUS dp_peer_sawf_ctx_alloc(struct dp_soc *soc,
						struct dp_peer *peer)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_peer_sawf_ctx_free(struct dp_soc *soc,
					       struct dp_peer *peer)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifndef CONFIG_SAWF
static inline
QDF_STATUS dp_peer_sawf_stats_ctx_alloc(struct dp_soc *soc,
					struct dp_txrx_peer *txrx_peer)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_peer_sawf_stats_ctx_free(struct dp_soc *soc,
				       struct dp_txrx_peer *txrx_peer)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_vdev_bss_peer_ref_n_get: Get bss peer of a vdev
 * @soc: DP soc
 * @vdev: vdev
 * @mod_id: id of module requesting reference
 *
 * Return: VDEV BSS peer
 */
struct dp_peer *dp_vdev_bss_peer_ref_n_get(struct dp_soc *soc,
					   struct dp_vdev *vdev,
					   enum dp_mod_id mod_id);

/**
 * dp_sta_vdev_self_peer_ref_n_get: Get self peer of sta vdev
 * @soc: DP soc
 * @vdev: vdev
 * @mod_id: id of module requesting reference
 *
 * Return: VDEV self peer
 */
struct dp_peer *dp_sta_vdev_self_peer_ref_n_get(struct dp_soc *soc,
						struct dp_vdev *vdev,
						enum dp_mod_id mod_id);

void dp_peer_ast_table_detach(struct dp_soc *soc);

/**
 * dp_peer_find_map_detach() - cleanup memory for peer_id_to_obj_map
 * @soc: soc handle
 *
 * Return: none
 */
void dp_peer_find_map_detach(struct dp_soc *soc);

void dp_soc_wds_detach(struct dp_soc *soc);
QDF_STATUS dp_peer_ast_table_attach(struct dp_soc *soc);

/**
 * dp_find_peer_by_macaddr() - Finding the peer from mac address provided.
 * @soc: soc handle
 * @mac_addr: MAC address to be used to find peer
 * @vdev_id: VDEV id
 * @mod_id: MODULE ID
 *
 * Return: struct dp_peer
 */
struct dp_peer *dp_find_peer_by_macaddr(struct dp_soc *soc, uint8_t *mac_addr,
					uint8_t vdev_id, enum dp_mod_id mod_id);
/**
 * dp_peer_ast_hash_attach() - Allocate and initialize AST Hash Table
 * @soc: SoC handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_peer_ast_hash_attach(struct dp_soc *soc);

/**
 * dp_peer_mec_hash_attach() - Allocate and initialize MEC Hash Table
 * @soc: SoC handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_peer_mec_hash_attach(struct dp_soc *soc);

/**
 * dp_del_wds_entry_wrapper() - delete a WDS AST entry
 * @soc: DP soc structure pointer
 * @vdev_id: vdev_id
 * @wds_macaddr: MAC address of ast node
 * @type: type from enum cdp_txrx_ast_entry_type
 * @delete_in_fw: Flag to indicate if entry needs to be deleted in fw
 *
 * This API is used to delete an AST entry from fw
 *
 * Return: None
 */
void dp_del_wds_entry_wrapper(struct dp_soc *soc, uint8_t vdev_id,
			      uint8_t *wds_macaddr, uint8_t type,
			      uint8_t delete_in_fw);

void dp_soc_wds_attach(struct dp_soc *soc);

/**
 * dp_peer_mec_hash_detach() - Free MEC Hash table
 * @soc: SoC handle
 *
 * Return: None
 */
void dp_peer_mec_hash_detach(struct dp_soc *soc);

/**
 * dp_peer_ast_hash_detach() - Free AST Hash table
 * @soc: SoC handle
 *
 * Return: None
 */
void dp_peer_ast_hash_detach(struct dp_soc *soc);

#ifdef FEATURE_AST
/**
 * dp_peer_delete_ast_entries(): Delete all AST entries for a peer
 * @soc: datapath soc handle
 * @peer: datapath peer handle
 *
 * Delete the AST entries belonging to a peer
 */
static inline void dp_peer_delete_ast_entries(struct dp_soc *soc,
					      struct dp_peer *peer)
{
	struct dp_ast_entry *ast_entry, *temp_ast_entry;

	dp_peer_debug("peer: %pK, self_ast: %pK", peer, peer->self_ast_entry);
	/*
	 * Delete peer self ast entry. This is done to handle scenarios
	 * where peer is freed before peer map is received(for ex in case
	 * of auth disallow due to ACL) in such cases self ast is not added
	 * to peer->ast_list.
	 */
	if (peer->self_ast_entry) {
		dp_peer_del_ast(soc, peer->self_ast_entry);
		peer->self_ast_entry = NULL;
	}

	DP_PEER_ITERATE_ASE_LIST(peer, ast_entry, temp_ast_entry)
		dp_peer_del_ast(soc, ast_entry);
}

/**
 * dp_print_peer_ast_entries() - Dump AST entries of peer
 * @soc: Datapath soc handle
 * @peer: Datapath peer
 * @arg: argument to iterate function
 *
 * Return: void
 */
void dp_print_peer_ast_entries(struct dp_soc *soc, struct dp_peer *peer,
			       void *arg);
#else
static inline void dp_print_peer_ast_entries(struct dp_soc *soc,
					     struct dp_peer *peer, void *arg)
{
}

static inline void dp_peer_delete_ast_entries(struct dp_soc *soc,
					      struct dp_peer *peer)
{
}
#endif

#ifdef FEATURE_MEC
/**
 * dp_peer_mec_spinlock_create() - Create the MEC spinlock
 * @soc: SoC handle
 *
 * Return: none
 */
void dp_peer_mec_spinlock_create(struct dp_soc *soc);

/**
 * dp_peer_mec_spinlock_destroy() - Destroy the MEC spinlock
 * @soc: SoC handle
 *
 * Return: none
 */
void dp_peer_mec_spinlock_destroy(struct dp_soc *soc);

/**
 * dp_peer_mec_flush_entries() - Delete all mec entries in table
 * @soc: Datapath SOC
 *
 * Return: None
 */
void dp_peer_mec_flush_entries(struct dp_soc *soc);
#else
static inline void dp_peer_mec_spinlock_create(struct dp_soc *soc)
{
}

static inline void dp_peer_mec_spinlock_destroy(struct dp_soc *soc)
{
}

static inline void dp_peer_mec_flush_entries(struct dp_soc *soc)
{
}
#endif

static inline int dp_peer_find_mac_addr_cmp(
	union dp_align_mac_addr *mac_addr1,
	union dp_align_mac_addr *mac_addr2)
{
		/*
		 * Intentionally use & rather than &&.
		 * because the operands are binary rather than generic boolean,
		 * the functionality is equivalent.
		 * Using && has the advantage of short-circuited evaluation,
		 * but using & has the advantage of no conditional branching,
		 * which is a more significant benefit.
		 */
	return !((mac_addr1->align4.bytes_abcd == mac_addr2->align4.bytes_abcd)
		 & (mac_addr1->align4.bytes_ef == mac_addr2->align4.bytes_ef));
}

/**
 * dp_peer_delete() - delete DP peer
 *
 * @soc: Datatpath soc
 * @peer: Datapath peer
 * @arg: argument to iter function
 *
 * Return: void
 */
void dp_peer_delete(struct dp_soc *soc,
		    struct dp_peer *peer,
		    void *arg);

/**
 * dp_mlo_peer_delete() - delete MLO DP peer
 *
 * @soc: Datapath soc
 * @peer: Datapath peer
 * @arg: argument to iter function
 *
 * Return: void
 */
void dp_mlo_peer_delete(struct dp_soc *soc, struct dp_peer *peer, void *arg);

#ifdef WLAN_FEATURE_11BE_MLO

/* is MLO connection mld peer */
#define IS_MLO_DP_MLD_TXRX_PEER(_peer) ((_peer)->mld_peer)

/* set peer type */
#define DP_PEER_SET_TYPE(_peer, _type_val) \
	((_peer)->peer_type = (_type_val))

/* is legacy peer */
#define IS_DP_LEGACY_PEER(_peer) \
	((_peer)->peer_type == CDP_LINK_PEER_TYPE && !((_peer)->mld_peer))
/* is MLO connection link peer */
#define IS_MLO_DP_LINK_PEER(_peer) \
	((_peer)->peer_type == CDP_LINK_PEER_TYPE && (_peer)->mld_peer)
/* is MLO connection mld peer */
#define IS_MLO_DP_MLD_PEER(_peer) \
	((_peer)->peer_type == CDP_MLD_PEER_TYPE)
/* Get Mld peer from link peer */
#define DP_GET_MLD_PEER_FROM_PEER(link_peer) \
	((link_peer)->mld_peer)

#ifdef WLAN_MLO_MULTI_CHIP
static inline uint8_t dp_get_chip_id(struct dp_soc *soc)
{
	if (soc->arch_ops.mlo_get_chip_id)
		return soc->arch_ops.mlo_get_chip_id(soc);

	return 0;
}

static inline struct dp_peer *
dp_link_peer_hash_find_by_chip_id(struct dp_soc *soc,
				  uint8_t *peer_mac_addr,
				  int mac_addr_is_aligned,
				  uint8_t vdev_id,
				  uint8_t chip_id,
				  enum dp_mod_id mod_id)
{
	if (soc->arch_ops.mlo_link_peer_find_hash_find_by_chip_id)
		return soc->arch_ops.mlo_link_peer_find_hash_find_by_chip_id
							(soc, peer_mac_addr,
							 mac_addr_is_aligned,
							 vdev_id, chip_id,
							 mod_id);

	return NULL;
}
#else
static inline uint8_t dp_get_chip_id(struct dp_soc *soc)
{
	return 0;
}

static inline struct dp_peer *
dp_link_peer_hash_find_by_chip_id(struct dp_soc *soc,
				  uint8_t *peer_mac_addr,
				  int mac_addr_is_aligned,
				  uint8_t vdev_id,
				  uint8_t chip_id,
				  enum dp_mod_id mod_id)
{
	return dp_peer_find_hash_find(soc, peer_mac_addr,
				      mac_addr_is_aligned,
				      vdev_id, mod_id);
}
#endif

/**
 * dp_mld_peer_find_hash_find() - returns mld peer from mld peer_hash_table
 *				  matching mac_address
 * @soc: soc handle
 * @peer_mac_addr: mld peer mac address
 * @mac_addr_is_aligned: is mac addr aligned
 * @vdev_id: vdev_id
 * @mod_id: id of module requesting reference
 *
 * Return: peer in success
 *         NULL in failure
 */
static inline
struct dp_peer *dp_mld_peer_find_hash_find(struct dp_soc *soc,
					   uint8_t *peer_mac_addr,
					   int mac_addr_is_aligned,
					   uint8_t vdev_id,
					   enum dp_mod_id mod_id)
{
	if (soc->arch_ops.mlo_peer_find_hash_find)
		return soc->arch_ops.mlo_peer_find_hash_find(soc,
					      peer_mac_addr,
					      mac_addr_is_aligned,
					      mod_id, vdev_id);
	return NULL;
}

/**
 * dp_peer_hash_find_wrapper() - find link peer or mld per according to
 *				 peer_type
 * @soc: DP SOC handle
 * @peer_info: peer information for hash find
 * @mod_id: ID of module requesting reference
 *
 * Return: peer handle
 */
static inline
struct dp_peer *dp_peer_hash_find_wrapper(struct dp_soc *soc,
					  struct cdp_peer_info *peer_info,
					  enum dp_mod_id mod_id)
{
	struct dp_peer *peer = NULL;

	if (peer_info->peer_type == CDP_LINK_PEER_TYPE ||
	    peer_info->peer_type == CDP_WILD_PEER_TYPE) {
		peer = dp_peer_find_hash_find(soc, peer_info->mac_addr,
					      peer_info->mac_addr_is_aligned,
					      peer_info->vdev_id,
					      mod_id);
		if (peer)
			return peer;
	}
	if (peer_info->peer_type == CDP_MLD_PEER_TYPE ||
	    peer_info->peer_type == CDP_WILD_PEER_TYPE)
		peer = dp_mld_peer_find_hash_find(
					soc, peer_info->mac_addr,
					peer_info->mac_addr_is_aligned,
					peer_info->vdev_id,
					mod_id);
	return peer;
}

/**
 * dp_link_peer_add_mld_peer() - add mld peer pointer to link peer,
 *				 increase mld peer ref_cnt
 * @link_peer: link peer pointer
 * @mld_peer: mld peer pointer
 *
 * Return: none
 */
static inline
void dp_link_peer_add_mld_peer(struct dp_peer *link_peer,
			       struct dp_peer *mld_peer)
{
	/* increase mld_peer ref_cnt */
	dp_peer_get_ref(NULL, mld_peer, DP_MOD_ID_CDP);
	link_peer->mld_peer = mld_peer;
}

/**
 * dp_link_peer_del_mld_peer() - delete mld peer pointer from link peer,
 *				 decrease mld peer ref_cnt
 * @link_peer: link peer pointer
 *
 * Return: None
 */
static inline
void dp_link_peer_del_mld_peer(struct dp_peer *link_peer)
{
	dp_peer_unref_delete(link_peer->mld_peer, DP_MOD_ID_CDP);
	link_peer->mld_peer = NULL;
}

/**
 * dp_mld_peer_init_link_peers_info() - init link peers info in mld peer
 * @mld_peer: mld peer pointer
 *
 * Return: None
 */
static inline
void dp_mld_peer_init_link_peers_info(struct dp_peer *mld_peer)
{
	int i;

	qdf_spinlock_create(&mld_peer->link_peers_info_lock);
	mld_peer->num_links = 0;
	for (i = 0; i < DP_MAX_MLO_LINKS; i++)
		mld_peer->link_peers[i].is_valid = false;
}

/**
 * dp_mld_peer_deinit_link_peers_info() - Deinit link peers info in mld peer
 * @mld_peer: mld peer pointer
 *
 * Return: None
 */
static inline
void dp_mld_peer_deinit_link_peers_info(struct dp_peer *mld_peer)
{
	qdf_spinlock_destroy(&mld_peer->link_peers_info_lock);
}

/**
 * dp_mld_peer_add_link_peer() - add link peer info to mld peer
 * @mld_peer: mld dp peer pointer
 * @link_peer: link dp peer pointer
 * @is_bridge_peer: flag to indicate if peer is bridge peer
 *
 * Return: None
 */
static inline
void dp_mld_peer_add_link_peer(struct dp_peer *mld_peer,
			       struct dp_peer *link_peer,
			       uint8_t is_bridge_peer)
{
	int i;
	struct dp_peer_link_info *link_peer_info;
	struct dp_soc *soc = mld_peer->vdev->pdev->soc;

	qdf_spin_lock_bh(&mld_peer->link_peers_info_lock);
	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		link_peer_info = &mld_peer->link_peers[i];
		if (!link_peer_info->is_valid) {
			qdf_mem_copy(link_peer_info->mac_addr.raw,
				     link_peer->mac_addr.raw,
				     QDF_MAC_ADDR_SIZE);
			link_peer_info->is_valid = true;
			link_peer_info->vdev_id = link_peer->vdev->vdev_id;
			link_peer_info->chip_id =
				dp_get_chip_id(link_peer->vdev->pdev->soc);
			link_peer_info->is_bridge_peer = is_bridge_peer;
			mld_peer->num_links++;
			break;
		}
	}
	qdf_spin_unlock_bh(&mld_peer->link_peers_info_lock);

	dp_peer_info("%s addition of link peer %pK (" QDF_MAC_ADDR_FMT ") "
		     "to MLD peer %pK (" QDF_MAC_ADDR_FMT "), "
		     "idx %u num_links %u",
		     (i != DP_MAX_MLO_LINKS) ? "Successful" : "Failed",
		     link_peer, QDF_MAC_ADDR_REF(link_peer->mac_addr.raw),
		     mld_peer, QDF_MAC_ADDR_REF(mld_peer->mac_addr.raw),
		     i, mld_peer->num_links);

	dp_cfg_event_record_mlo_link_delink_evt(soc, DP_CFG_EVENT_MLO_ADD_LINK,
						mld_peer, link_peer, i,
						(i != DP_MAX_MLO_LINKS) ? 1 : 0);
}

/**
 * dp_mld_peer_del_link_peer() - Delete link peer info from MLD peer
 * @mld_peer: MLD dp peer pointer
 * @link_peer: link dp peer pointer
 *
 * Return: number of links left after deletion
 */
static inline
uint8_t dp_mld_peer_del_link_peer(struct dp_peer *mld_peer,
				  struct dp_peer *link_peer)
{
	int i;
	struct dp_peer_link_info *link_peer_info;
	uint8_t num_links;
	struct dp_soc *soc = mld_peer->vdev->pdev->soc;

	qdf_spin_lock_bh(&mld_peer->link_peers_info_lock);
	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		link_peer_info = &mld_peer->link_peers[i];
		if (link_peer_info->is_valid &&
		    !dp_peer_find_mac_addr_cmp(&link_peer->mac_addr,
					&link_peer_info->mac_addr)) {
			link_peer_info->is_valid = false;
			mld_peer->num_links--;
			break;
		}
	}
	num_links = mld_peer->num_links;
	qdf_spin_unlock_bh(&mld_peer->link_peers_info_lock);

	dp_peer_info("%s deletion of link peer %pK (" QDF_MAC_ADDR_FMT ") "
		     "from MLD peer %pK (" QDF_MAC_ADDR_FMT "), "
		     "idx %u num_links %u",
		     (i != DP_MAX_MLO_LINKS) ? "Successful" : "Failed",
		     link_peer, QDF_MAC_ADDR_REF(link_peer->mac_addr.raw),
		     mld_peer, QDF_MAC_ADDR_REF(mld_peer->mac_addr.raw),
		     i, mld_peer->num_links);

	dp_cfg_event_record_mlo_link_delink_evt(soc, DP_CFG_EVENT_MLO_DEL_LINK,
						mld_peer, link_peer, i,
						(i != DP_MAX_MLO_LINKS) ? 1 : 0);

	return num_links;
}

/**
 * dp_get_link_peers_ref_from_mld_peer() - get link peers pointer and
 *					   increase link peers ref_cnt
 * @soc: dp_soc handle
 * @mld_peer: dp mld peer pointer
 * @mld_link_peers: structure that hold links peers pointer array and number
 * @mod_id: id of module requesting reference
 *
 * Return: None
 */
static inline
void dp_get_link_peers_ref_from_mld_peer(
				struct dp_soc *soc,
				struct dp_peer *mld_peer,
				struct dp_mld_link_peers *mld_link_peers,
				enum dp_mod_id mod_id)
{
	struct dp_peer *peer;
	uint8_t i = 0, j = 0;
	struct dp_peer_link_info *link_peer_info;

	qdf_mem_zero(mld_link_peers, sizeof(*mld_link_peers));
	qdf_spin_lock_bh(&mld_peer->link_peers_info_lock);
	for (i = 0; i < DP_MAX_MLO_LINKS; i++)  {
		link_peer_info = &mld_peer->link_peers[i];
		if (link_peer_info->is_valid) {
			peer = dp_link_peer_hash_find_by_chip_id(
						soc,
						link_peer_info->mac_addr.raw,
						true,
						link_peer_info->vdev_id,
						link_peer_info->chip_id,
						mod_id);
			if (peer)
				mld_link_peers->link_peers[j++] = peer;
		}
	}
	qdf_spin_unlock_bh(&mld_peer->link_peers_info_lock);

	mld_link_peers->num_links = j;
}

/**
 * dp_release_link_peers_ref() - release all link peers reference
 * @mld_link_peers: structure that hold links peers pointer array and number
 * @mod_id: id of module requesting reference
 *
 * Return: None.
 */
static inline
void dp_release_link_peers_ref(
			struct dp_mld_link_peers *mld_link_peers,
			enum dp_mod_id mod_id)
{
	struct dp_peer *peer;
	uint8_t i;

	for (i = 0; i < mld_link_peers->num_links; i++) {
		peer = mld_link_peers->link_peers[i];
		if (peer)
			dp_peer_unref_delete(peer, mod_id);
		mld_link_peers->link_peers[i] = NULL;
	}

	 mld_link_peers->num_links = 0;
}

/**
 * dp_get_link_peer_id_by_lmac_id() - Get link peer id using peer id and lmac id
 * @soc: Datapath soc handle
 * @peer_id: peer id
 * @lmac_id: lmac id to find the link peer on given lmac
 *
 * Return: peer_id of link peer if found
 *         else return HTT_INVALID_PEER
 */
static inline
uint16_t dp_get_link_peer_id_by_lmac_id(struct dp_soc *soc, uint16_t peer_id,
					uint8_t lmac_id)
{
	uint8_t i;
	struct dp_peer *peer;
	struct dp_peer *link_peer;
	struct dp_soc *link_peer_soc;
	struct dp_mld_link_peers link_peers_info;
	uint16_t link_peer_id = HTT_INVALID_PEER;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_CDP);

	if (!peer)
		return HTT_INVALID_PEER;

	if (IS_MLO_DP_MLD_PEER(peer)) {
		/* get link peers with reference */
		dp_get_link_peers_ref_from_mld_peer(soc, peer, &link_peers_info,
						    DP_MOD_ID_CDP);

		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			link_peer_soc = link_peer->vdev->pdev->soc;
			if ((link_peer_soc == soc) &&
			    (link_peer->vdev->pdev->lmac_id == lmac_id)) {
				link_peer_id = link_peer->peer_id;
				break;
			}
		}
		/* release link peers reference */
		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
	} else {
		link_peer_id = peer_id;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return link_peer_id;
}

/**
 * dp_peer_get_tgt_peer_hash_find() - get dp_peer handle
 * @soc: soc handle
 * @peer_mac: peer mac address
 * @mac_addr_is_aligned: is mac addr aligned
 * @vdev_id: vdev_id
 * @mod_id: id of module requesting reference
 *
 * for MLO connection, get corresponding MLD peer,
 * otherwise get link peer for non-MLO case.
 *
 * Return: peer in success
 *         NULL in failure
 */
static inline
struct dp_peer *dp_peer_get_tgt_peer_hash_find(struct dp_soc *soc,
					       uint8_t *peer_mac,
					       int mac_addr_is_aligned,
					       uint8_t vdev_id,
					       enum dp_mod_id mod_id)
{
	struct dp_peer *ta_peer = NULL;
	struct dp_peer *peer = dp_peer_find_hash_find(soc,
						      peer_mac, 0, vdev_id,
						      mod_id);

	if (peer) {
		/* mlo connection link peer, get mld peer with reference */
		if (IS_MLO_DP_LINK_PEER(peer)) {
			/* increase mld peer ref_cnt */
			if (QDF_STATUS_SUCCESS ==
			    dp_peer_get_ref(soc, peer->mld_peer, mod_id))
				ta_peer = peer->mld_peer;
			else
				ta_peer = NULL;

			/* release peer reference that added by hash find */
			dp_peer_unref_delete(peer, mod_id);
		} else {
		/* mlo MLD peer or non-mlo link peer */
			ta_peer = peer;
		}
	} else {
		dp_peer_err("fail to find peer:" QDF_MAC_ADDR_FMT " vdev_id: %u",
			    QDF_MAC_ADDR_REF(peer_mac), vdev_id);
	}

	return ta_peer;
}

/**
 * dp_peer_get_tgt_peer_by_id() - Returns target peer object given the peer id
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 * @mod_id: ID of module requesting reference
 *
 * for MLO connection, get corresponding MLD peer,
 * otherwise get link peer for non-MLO case.
 *
 * Return: peer in success
 *         NULL in failure
 */
static inline
struct dp_peer *dp_peer_get_tgt_peer_by_id(struct dp_soc *soc,
					   uint16_t peer_id,
					   enum dp_mod_id mod_id)
{
	struct dp_peer *ta_peer = NULL;
	struct dp_peer *peer = dp_peer_get_ref_by_id(soc, peer_id, mod_id);

	if (peer) {
		/* mlo connection link peer, get mld peer with reference */
		if (IS_MLO_DP_LINK_PEER(peer)) {
			/* increase mld peer ref_cnt */
			if (QDF_STATUS_SUCCESS ==
				dp_peer_get_ref(soc, peer->mld_peer, mod_id))
				ta_peer = peer->mld_peer;
			else
				ta_peer = NULL;

			/* release peer reference that added by hash find */
			dp_peer_unref_delete(peer, mod_id);
		} else {
		/* mlo MLD peer or non-mlo link peer */
			ta_peer = peer;
		}
	}

	return ta_peer;
}

/**
 * dp_peer_mlo_delete() - peer MLO related delete operation
 * @peer: DP peer handle
 * Return: None
 */
static inline
void dp_peer_mlo_delete(struct dp_peer *peer)
{
	struct dp_peer *ml_peer;
	struct dp_soc *soc;

	dp_info("peer " QDF_MAC_ADDR_FMT " type %d",
		QDF_MAC_ADDR_REF(peer->mac_addr.raw), peer->peer_type);

	/* MLO connection link peer */
	if (IS_MLO_DP_LINK_PEER(peer)) {
		ml_peer = peer->mld_peer;
		soc = ml_peer->vdev->pdev->soc;

		/* if last link peer deletion, delete MLD peer */
		if (dp_mld_peer_del_link_peer(peer->mld_peer, peer) == 0)
			dp_peer_delete(soc, peer->mld_peer, NULL);
	}
}

/**
 * dp_peer_mlo_setup() - create MLD peer and MLO related initialization
 * @soc: Soc handle
 * @peer: DP peer handle
 * @vdev_id: Vdev ID
 * @setup_info: peer setup information for MLO
 */
QDF_STATUS dp_peer_mlo_setup(
			struct dp_soc *soc,
			struct dp_peer *peer,
			uint8_t vdev_id,
			struct cdp_peer_setup_info *setup_info);

/**
 * dp_get_tgt_peer_from_peer() - Get target peer from the given peer
 * @peer: datapath peer
 *
 * Return: MLD peer in case of MLO Link peer
 *	   Peer itself in other cases
 */
static inline
struct dp_peer *dp_get_tgt_peer_from_peer(struct dp_peer *peer)
{
	return IS_MLO_DP_LINK_PEER(peer) ? peer->mld_peer : peer;
}

/**
 * dp_get_primary_link_peer_by_id(): Get primary link peer from the given
 *					peer id
 * @soc: core DP soc context
 * @peer_id: peer id
 * @mod_id: ID of module requesting reference
 *
 * Return: primary link peer for the MLO peer
 *	   legacy peer itself in case of legacy peer
 */
static inline
struct dp_peer *dp_get_primary_link_peer_by_id(struct dp_soc *soc,
					       uint16_t peer_id,
					       enum dp_mod_id mod_id)
{
	uint8_t i;
	struct dp_mld_link_peers link_peers_info;
	struct dp_peer *peer;
	struct dp_peer *link_peer;
	struct dp_peer *primary_peer = NULL;

	peer = dp_peer_get_ref_by_id(soc, peer_id, mod_id);

	if (!peer)
		return NULL;

	if (IS_MLO_DP_MLD_PEER(peer)) {
		/* get link peers with reference */
		dp_get_link_peers_ref_from_mld_peer(soc, peer, &link_peers_info,
						    mod_id);

		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			if (link_peer->primary_link) {
				/*
				 * Take additional reference over
				 * primary link peer.
				 */
				if (QDF_STATUS_SUCCESS ==
				    dp_peer_get_ref(NULL, link_peer, mod_id))
					primary_peer = link_peer;
				break;
			}
		}
		/* release link peers reference */
		dp_release_link_peers_ref(&link_peers_info, mod_id);
		dp_peer_unref_delete(peer, mod_id);
	} else {
		primary_peer = peer;
	}

	return primary_peer;
}

/**
 * dp_get_txrx_peer() - Get dp_txrx_peer from passed dp_peer
 * @peer: Datapath peer
 *
 * Return: dp_txrx_peer from MLD peer if peer type is link peer
 *	   dp_txrx_peer from peer itself for other cases
 */
static inline
struct dp_txrx_peer *dp_get_txrx_peer(struct dp_peer *peer)
{
	return IS_MLO_DP_LINK_PEER(peer) ?
				peer->mld_peer->txrx_peer : peer->txrx_peer;
}

/**
 * dp_peer_is_primary_link_peer() - Check if peer is primary link peer
 * @peer: Datapath peer
 *
 * Return: true if peer is primary link peer or legacy peer
 *	   false otherwise
 */
static inline
bool dp_peer_is_primary_link_peer(struct dp_peer *peer)
{
	if (IS_MLO_DP_LINK_PEER(peer) && peer->primary_link)
		return true;
	else if (IS_DP_LEGACY_PEER(peer))
		return true;
	else
		return false;
}

/**
 * dp_tgt_txrx_peer_get_ref_by_id() - Gets tgt txrx peer for given the peer id
 *
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 * @handle: reference handle
 * @mod_id: ID of module requesting reference
 *
 * Return: struct dp_txrx_peer*: Pointer to txrx DP peer object
 */
static inline struct dp_txrx_peer *
dp_tgt_txrx_peer_get_ref_by_id(struct dp_soc *soc,
			       uint16_t peer_id,
			       dp_txrx_ref_handle *handle,
			       enum dp_mod_id mod_id)

{
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;

	peer = dp_peer_get_ref_by_id(soc, peer_id, mod_id);
	if (!peer)
		return NULL;

	txrx_peer = dp_get_txrx_peer(peer);
	if (txrx_peer) {
		*handle = (dp_txrx_ref_handle)peer;
		return txrx_peer;
	}

	dp_peer_unref_delete(peer, mod_id);
	return NULL;
}

/**
 * dp_print_mlo_ast_stats_be() - Print AST stats for MLO peers
 *
 * @soc: core DP soc context
 *
 * Return: void
 */
void dp_print_mlo_ast_stats_be(struct dp_soc *soc);

/**
 * dp_get_peer_link_id() - Get Link peer Link ID
 * @peer: Datapath peer
 *
 * Return: Link peer Link ID
 */
uint8_t dp_get_peer_link_id(struct dp_peer *peer);
#else

#define IS_MLO_DP_MLD_TXRX_PEER(_peer) false

#define DP_PEER_SET_TYPE(_peer, _type_val) /* no op */
/* is legacy peer */
#define IS_DP_LEGACY_PEER(_peer) true
#define IS_MLO_DP_LINK_PEER(_peer) false
#define IS_MLO_DP_MLD_PEER(_peer) false
#define DP_GET_MLD_PEER_FROM_PEER(link_peer) NULL

static inline
struct dp_peer *dp_peer_hash_find_wrapper(struct dp_soc *soc,
					  struct cdp_peer_info *peer_info,
					  enum dp_mod_id mod_id)
{
	return dp_peer_find_hash_find(soc, peer_info->mac_addr,
				      peer_info->mac_addr_is_aligned,
				      peer_info->vdev_id,
				      mod_id);
}

static inline
struct dp_peer *dp_peer_get_tgt_peer_hash_find(struct dp_soc *soc,
					       uint8_t *peer_mac,
					       int mac_addr_is_aligned,
					       uint8_t vdev_id,
					       enum dp_mod_id mod_id)
{
	return dp_peer_find_hash_find(soc, peer_mac,
				      mac_addr_is_aligned, vdev_id,
				      mod_id);
}

static inline
struct dp_peer *dp_peer_get_tgt_peer_by_id(struct dp_soc *soc,
					   uint16_t peer_id,
					   enum dp_mod_id mod_id)
{
	return dp_peer_get_ref_by_id(soc, peer_id, mod_id);
}

static inline
QDF_STATUS dp_peer_mlo_setup(
			struct dp_soc *soc,
			struct dp_peer *peer,
			uint8_t vdev_id,
			struct cdp_peer_setup_info *setup_info)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_mld_peer_init_link_peers_info(struct dp_peer *mld_peer)
{
}

static inline
void dp_mld_peer_deinit_link_peers_info(struct dp_peer *mld_peer)
{
}

static inline
void dp_link_peer_del_mld_peer(struct dp_peer *link_peer)
{
}

static inline
void dp_peer_mlo_delete(struct dp_peer *peer)
{
}

static inline
void dp_mlo_peer_authorize(struct dp_soc *soc,
			   struct dp_peer *link_peer)
{
}

static inline uint8_t dp_get_chip_id(struct dp_soc *soc)
{
	return 0;
}

static inline struct dp_peer *
dp_link_peer_hash_find_by_chip_id(struct dp_soc *soc,
				  uint8_t *peer_mac_addr,
				  int mac_addr_is_aligned,
				  uint8_t vdev_id,
				  uint8_t chip_id,
				  enum dp_mod_id mod_id)
{
	return dp_peer_find_hash_find(soc, peer_mac_addr,
				      mac_addr_is_aligned,
				      vdev_id, mod_id);
}

static inline
struct dp_peer *dp_get_tgt_peer_from_peer(struct dp_peer *peer)
{
	return peer;
}

static inline
struct dp_peer *dp_get_primary_link_peer_by_id(struct dp_soc *soc,
					       uint16_t peer_id,
					       enum dp_mod_id mod_id)
{
	return dp_peer_get_ref_by_id(soc, peer_id, mod_id);
}

static inline
struct dp_txrx_peer *dp_get_txrx_peer(struct dp_peer *peer)
{
	return peer->txrx_peer;
}

static inline
bool dp_peer_is_primary_link_peer(struct dp_peer *peer)
{
	return true;
}

/**
 * dp_tgt_txrx_peer_get_ref_by_id() - Gets tgt txrx peer for given the peer id
 *
 * @soc: core DP soc context
 * @peer_id: peer id from peer object can be retrieved
 * @handle: reference handle
 * @mod_id: ID of module requesting reference
 *
 * Return: struct dp_txrx_peer*: Pointer to txrx DP peer object
 */
static inline struct dp_txrx_peer *
dp_tgt_txrx_peer_get_ref_by_id(struct dp_soc *soc,
			       uint16_t peer_id,
			       dp_txrx_ref_handle *handle,
			       enum dp_mod_id mod_id)

{
	return dp_txrx_peer_get_ref_by_id(soc, peer_id, handle, mod_id);
}

static inline
uint16_t dp_get_link_peer_id_by_lmac_id(struct dp_soc *soc, uint16_t peer_id,
					uint8_t lmac_id)
{
	return peer_id;
}

static inline void dp_print_mlo_ast_stats_be(struct dp_soc *soc)
{
}

static inline uint8_t dp_get_peer_link_id(struct dp_peer *peer)
{
	return 0;
}
#endif /* WLAN_FEATURE_11BE_MLO */

static inline
void dp_peer_defrag_rx_tids_init(struct dp_txrx_peer *txrx_peer)
{
	uint8_t i;

	qdf_mem_zero(&txrx_peer->rx_tid, DP_MAX_TIDS *
		     sizeof(struct dp_rx_tid_defrag));

	for (i = 0; i < DP_MAX_TIDS; i++)
		qdf_spinlock_create(&txrx_peer->rx_tid[i].defrag_tid_lock);
}

static inline
void dp_peer_defrag_rx_tids_deinit(struct dp_txrx_peer *txrx_peer)
{
	uint8_t i;

	for (i = 0; i < DP_MAX_TIDS; i++)
		qdf_spinlock_destroy(&txrx_peer->rx_tid[i].defrag_tid_lock);
}

#ifdef PEER_CACHE_RX_PKTS
static inline
void dp_peer_rx_bufq_resources_init(struct dp_txrx_peer *txrx_peer)
{
	qdf_spinlock_create(&txrx_peer->bufq_info.bufq_lock);
	txrx_peer->bufq_info.thresh = DP_RX_CACHED_BUFQ_THRESH;
	qdf_list_create(&txrx_peer->bufq_info.cached_bufq,
			DP_RX_CACHED_BUFQ_THRESH);
}

static inline
void dp_peer_rx_bufq_resources_deinit(struct dp_txrx_peer *txrx_peer)
{
	qdf_list_destroy(&txrx_peer->bufq_info.cached_bufq);
	qdf_spinlock_destroy(&txrx_peer->bufq_info.bufq_lock);
}

#else
static inline
void dp_peer_rx_bufq_resources_init(struct dp_txrx_peer *txrx_peer)
{
}

static inline
void dp_peer_rx_bufq_resources_deinit(struct dp_txrx_peer *txrx_peer)
{
}
#endif

/**
 * dp_peer_update_state() - update dp peer state
 *
 * @soc: core DP soc context
 * @peer: DP peer
 * @state: new state
 *
 * Return: None
 */
static inline void
dp_peer_update_state(struct dp_soc *soc,
		     struct dp_peer *peer,
		     enum dp_peer_state state)
{
	uint8_t peer_state;

	qdf_spin_lock_bh(&peer->peer_state_lock);
	peer_state = peer->peer_state;

	switch (state) {
	case DP_PEER_STATE_INIT:
		DP_PEER_STATE_ASSERT
			(peer, state, (peer_state != DP_PEER_STATE_ACTIVE) &&
			(peer_state != DP_PEER_STATE_LOGICAL_DELETE));
		break;

	case DP_PEER_STATE_ACTIVE:
		DP_PEER_STATE_ASSERT(peer, state,
				     (peer_state == DP_PEER_STATE_INIT));
		break;

	case DP_PEER_STATE_LOGICAL_DELETE:
		DP_PEER_STATE_ASSERT(peer, state,
				     (peer_state == DP_PEER_STATE_ACTIVE) ||
				     (peer_state == DP_PEER_STATE_INIT));
		break;

	case DP_PEER_STATE_INACTIVE:
		if (IS_MLO_DP_MLD_PEER(peer))
			DP_PEER_STATE_ASSERT
				(peer, state,
				 (peer_state == DP_PEER_STATE_ACTIVE));
		else
			DP_PEER_STATE_ASSERT
				(peer, state,
				 (peer_state == DP_PEER_STATE_LOGICAL_DELETE));
		break;

	case DP_PEER_STATE_FREED:
		if (peer->sta_self_peer)
			DP_PEER_STATE_ASSERT
			(peer, state, (peer_state == DP_PEER_STATE_INIT));
		else
			DP_PEER_STATE_ASSERT
				(peer, state,
				 (peer_state == DP_PEER_STATE_INACTIVE) ||
				 (peer_state == DP_PEER_STATE_LOGICAL_DELETE));
		break;

	default:
		qdf_spin_unlock_bh(&peer->peer_state_lock);
		dp_alert("Invalid peer state %u for peer " QDF_MAC_ADDR_FMT,
			 state, QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		return;
	}
	peer->peer_state = state;
	qdf_spin_unlock_bh(&peer->peer_state_lock);
	dp_info("Updating peer state from %u to %u mac " QDF_MAC_ADDR_FMT "\n",
		peer_state, state,
		QDF_MAC_ADDR_REF(peer->mac_addr.raw));
}

/**
 * dp_vdev_iterate_specific_peer_type() - API to iterate through vdev peer
 * list based on type of peer (Legacy or MLD peer)
 *
 * @vdev: DP vdev context
 * @func: function to be called for each peer
 * @arg: argument need to be passed to func
 * @mod_id: module_id
 * @peer_type: type of peer - MLO Link Peer or Legacy Peer
 *
 * Return: void
 */
static inline void
dp_vdev_iterate_specific_peer_type(struct dp_vdev *vdev,
				   dp_peer_iter_func *func,
				   void *arg, enum dp_mod_id mod_id,
				   enum dp_peer_type peer_type)
{
	struct dp_peer *peer;
	struct dp_peer *tmp_peer;
	struct dp_soc *soc = NULL;

	if (!vdev || !vdev->pdev || !vdev->pdev->soc)
		return;

	soc = vdev->pdev->soc;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH_SAFE(peer, &vdev->peer_list,
			   peer_list_elem,
			   tmp_peer) {
		if (dp_peer_get_ref(soc, peer, mod_id) ==
					QDF_STATUS_SUCCESS) {
			if ((peer_type == DP_PEER_TYPE_LEGACY &&
			     (IS_DP_LEGACY_PEER(peer))) ||
			    (peer_type == DP_PEER_TYPE_MLO_LINK &&
			     (IS_MLO_DP_LINK_PEER(peer)))) {
				(*func)(soc, peer, arg);
			}
			dp_peer_unref_delete(peer, mod_id);
		}
	}
	qdf_spin_unlock_bh(&vdev->peer_list_lock);
}

#ifdef REO_SHARED_QREF_TABLE_EN
void dp_peer_rx_reo_shared_qaddr_delete(struct dp_soc *soc,
					struct dp_peer *peer);
#else
static inline void dp_peer_rx_reo_shared_qaddr_delete(struct dp_soc *soc,
						      struct dp_peer *peer) {}
#endif

/**
 * dp_peer_check_wds_ext_peer() - Check WDS ext peer
 *
 * @peer: DP peer
 *
 * Return: True for WDS ext peer, false otherwise
 */
bool dp_peer_check_wds_ext_peer(struct dp_peer *peer);

/**
 * dp_gen_ml_peer_id() - Generate MLD peer id for DP
 *
 * @soc: DP soc context
 * @peer_id: mld peer id
 *
 * Return: DP MLD peer id
 */
uint16_t dp_gen_ml_peer_id(struct dp_soc *soc, uint16_t peer_id);

#ifdef FEATURE_AST
/**
 * dp_peer_host_add_map_ast() - Add ast entry with HW AST Index
 * @soc: SoC handle
 * @peer_id: peer id from firmware
 * @mac_addr: MAC address of ast node
 * @hw_peer_id: HW AST Index returned by target in peer map event
 * @vdev_id: vdev id for VAP to which the peer belongs to
 * @ast_hash: ast hash value in HW
 * @is_wds: flag to indicate peer map event for WDS ast entry
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS dp_peer_host_add_map_ast(struct dp_soc *soc, uint16_t peer_id,
				    uint8_t *mac_addr, uint16_t hw_peer_id,
				    uint8_t vdev_id, uint16_t ast_hash,
				    uint8_t is_wds);
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
/**
 * dp_map_link_id_band: Set link id to band mapping in txrx_peer
 * @peer: dp peer pointer
 *
 * Return: None
 */
void dp_map_link_id_band(struct dp_peer *peer);
#else
static inline
void dp_map_link_id_band(struct dp_peer *peer)
{
}
#endif
#endif /* _DP_PEER_H_ */

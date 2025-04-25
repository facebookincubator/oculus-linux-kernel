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

#include <qdf_types.h>
#include <qdf_lock.h>
#include <hal_hw_headers.h>
#include "dp_htt.h"
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_peer.h"
#include "dp_rx_defrag.h"
#include "dp_rx.h"
#include <hal_api.h>
#include <hal_reo.h>
#include <cdp_txrx_handle.h>
#include <wlan_cfg.h>
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#include <qdf_module.h>
#ifdef QCA_PEER_EXT_STATS
#include "dp_hist.h"
#endif
#ifdef BYPASS_OL_OPS
#include <target_if_dp.h>
#endif
#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
#include "reg_services_common.h"
#endif
#ifdef FEATURE_AST
#ifdef BYPASS_OL_OPS
/**
 * dp_add_wds_entry_wrapper() - Add new AST entry for the wds station
 * @soc: DP soc structure pointer
 * @peer: dp peer structure
 * @dest_macaddr: MAC address of ast node
 * @flags: wds or hmwds
 * @type: type from enum cdp_txrx_ast_entry_type
 *
 * This API is used by WDS source port learning function to
 * add a new AST entry in the fw.
 *
 * Return: 0 on success, error code otherwise.
 */
static int dp_add_wds_entry_wrapper(struct dp_soc *soc,
				    struct dp_peer *peer,
				    const uint8_t *dest_macaddr,
				    uint32_t flags,
				    uint8_t type)
{
	QDF_STATUS status;

	status = target_if_add_wds_entry(soc->ctrl_psoc,
					 peer->vdev->vdev_id,
					 peer->mac_addr.raw,
					 dest_macaddr,
					 WMI_HOST_WDS_FLAG_STATIC,
					 type);

	return qdf_status_to_os_return(status);
}

/**
 * dp_update_wds_entry_wrapper() - update an existing wds entry with new peer
 * @soc: DP soc structure pointer
 * @peer: dp peer structure
 * @dest_macaddr: MAC address of ast node
 * @flags: wds or hmwds
 *
 * This API is used by update the peer mac address for the ast
 * in the fw.
 *
 * Return: 0 on success, error code otherwise.
 */
static int dp_update_wds_entry_wrapper(struct dp_soc *soc,
				       struct dp_peer *peer,
				       uint8_t *dest_macaddr,
				       uint32_t flags)
{
	QDF_STATUS status;

	status = target_if_update_wds_entry(soc->ctrl_psoc,
					    peer->vdev->vdev_id,
					    dest_macaddr,
					    peer->mac_addr.raw,
					    WMI_HOST_WDS_FLAG_STATIC);

	return qdf_status_to_os_return(status);
}

/**
 * dp_del_wds_entry_wrapper() - delete a WSD AST entry
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
void dp_del_wds_entry_wrapper(struct dp_soc *soc,
			      uint8_t vdev_id,
			      uint8_t *wds_macaddr,
			      uint8_t type,
			      uint8_t delete_in_fw)
{
	target_if_del_wds_entry(soc->ctrl_psoc, vdev_id,
				wds_macaddr, type, delete_in_fw);
}
#else
static int dp_add_wds_entry_wrapper(struct dp_soc *soc,
				    struct dp_peer *peer,
				    const uint8_t *dest_macaddr,
				    uint32_t flags,
				    uint8_t type)
{
	int status;

	status = soc->cdp_soc.ol_ops->peer_add_wds_entry(
					soc->ctrl_psoc,
					peer->vdev->vdev_id,
					peer->mac_addr.raw,
					peer->peer_id,
					dest_macaddr,
					peer->mac_addr.raw,
					flags,
					type);

	return status;
}

static int dp_update_wds_entry_wrapper(struct dp_soc *soc,
				       struct dp_peer *peer,
				       uint8_t *dest_macaddr,
				       uint32_t flags)
{
	int status;

	status = soc->cdp_soc.ol_ops->peer_update_wds_entry(
				soc->ctrl_psoc,
				peer->vdev->vdev_id,
				dest_macaddr,
				peer->mac_addr.raw,
				flags);

	return status;
}

void dp_del_wds_entry_wrapper(struct dp_soc *soc,
			      uint8_t vdev_id,
			      uint8_t *wds_macaddr,
			      uint8_t type,
			      uint8_t delete_in_fw)
{
	soc->cdp_soc.ol_ops->peer_del_wds_entry(soc->ctrl_psoc,
						vdev_id,
						wds_macaddr,
						type,
						delete_in_fw);
}
#endif /* BYPASS_OL_OPS */
#else
void dp_del_wds_entry_wrapper(struct dp_soc *soc,
			      uint8_t vdev_id,
			      uint8_t *wds_macaddr,
			      uint8_t type,
			      uint8_t delete_in_fw)
{
}
#endif /* FEATURE_AST */

#ifdef FEATURE_WDS
static inline bool
dp_peer_ast_free_in_unmap_supported(struct dp_soc *soc,
				    struct dp_ast_entry *ast_entry)
{
	/* if peer map v2 is enabled we are not freeing ast entry
	 * here and it is supposed to be freed in unmap event (after
	 * we receive delete confirmation from target)
	 *
	 * if peer_id is invalid we did not get the peer map event
	 * for the peer free ast entry from here only in this case
	 */

	if ((ast_entry->type != CDP_TXRX_AST_TYPE_WDS_HM_SEC) &&
	    (ast_entry->type != CDP_TXRX_AST_TYPE_SELF))
		return true;

	return false;
}
#else
static inline bool
dp_peer_ast_free_in_unmap_supported(struct dp_soc *soc,
				    struct dp_ast_entry *ast_entry)
{
	return false;
}

void dp_soc_wds_attach(struct dp_soc *soc)
{
}

void dp_soc_wds_detach(struct dp_soc *soc)
{
}
#endif

#ifdef QCA_SUPPORT_WDS_EXTENDED
bool dp_peer_check_wds_ext_peer(struct dp_peer *peer)
{
	struct dp_vdev *vdev = peer->vdev;
	struct dp_txrx_peer *txrx_peer;

	if (!vdev->wds_ext_enabled)
		return false;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer)
		return false;

	if (qdf_atomic_test_bit(WDS_EXT_PEER_INIT_BIT,
				&txrx_peer->wds_ext.init))
		return true;

	return false;
}
#else
bool dp_peer_check_wds_ext_peer(struct dp_peer *peer)
{
	return false;
}
#endif

QDF_STATUS dp_peer_ast_table_attach(struct dp_soc *soc)
{
	uint32_t max_ast_index;

	max_ast_index = wlan_cfg_get_max_ast_idx(soc->wlan_cfg_ctx);
	/* allocate ast_table for ast entry to ast_index map */
	dp_peer_info("\n%pK:<=== cfg max ast idx %d ====>", soc, max_ast_index);
	soc->ast_table = qdf_mem_malloc(max_ast_index *
					sizeof(struct dp_ast_entry *));
	if (!soc->ast_table) {
		dp_peer_err("%pK: ast_table memory allocation failed", soc);
		return QDF_STATUS_E_NOMEM;
	}
	return QDF_STATUS_SUCCESS; /* success */
}

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
					uint8_t vdev_id, enum dp_mod_id mod_id)
{
	bool ast_ind_disable = wlan_cfg_get_ast_indication_disable(
							    soc->wlan_cfg_ctx);
	struct cdp_peer_info peer_info = {0};

	if ((!soc->ast_offload_support) || (!ast_ind_disable)) {
		struct dp_ast_entry *ast_entry = NULL;
		uint16_t peer_id;

		qdf_spin_lock_bh(&soc->ast_lock);

		if (vdev_id == DP_VDEV_ALL)
			ast_entry = dp_peer_ast_hash_find_soc(soc, mac_addr);
		else
			ast_entry = dp_peer_ast_hash_find_by_vdevid
						(soc, mac_addr, vdev_id);

		if (!ast_entry) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			dp_err("NULL ast entry");
			return NULL;
		}

		peer_id = ast_entry->peer_id;
		qdf_spin_unlock_bh(&soc->ast_lock);

		if (peer_id == HTT_INVALID_PEER)
			return NULL;

		return dp_peer_get_ref_by_id(soc, peer_id, mod_id);
	}

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, mac_addr, false,
				 CDP_WILD_PEER_TYPE);
	return dp_peer_hash_find_wrapper(soc, &peer_info, mod_id);
}

/**
 * dp_peer_find_map_attach() - allocate memory for peer_id_to_obj_map
 * @soc: soc handle
 *
 * return: QDF_STATUS
 */
static QDF_STATUS dp_peer_find_map_attach(struct dp_soc *soc)
{
	uint32_t max_peers, peer_map_size;

	max_peers = soc->max_peer_id;
	/* allocate the peer ID -> peer object map */
	dp_peer_info("\n%pK:<=== cfg max peer id %d ====>", soc, max_peers);
	peer_map_size = max_peers * sizeof(soc->peer_id_to_obj_map[0]);
	soc->peer_id_to_obj_map = qdf_mem_malloc(peer_map_size);
	if (!soc->peer_id_to_obj_map) {
		dp_peer_err("%pK: peer map memory allocation failed", soc);
		return QDF_STATUS_E_NOMEM;
	}

	/*
	 * The peer_id_to_obj_map doesn't really need to be initialized,
	 * since elements are only used after they have been individually
	 * initialized.
	 * However, it is convenient for debugging to have all elements
	 * that are not in use set to 0.
	 */
	qdf_mem_zero(soc->peer_id_to_obj_map, peer_map_size);

	qdf_spinlock_create(&soc->peer_map_lock);
	return QDF_STATUS_SUCCESS; /* success */
}

#define DP_AST_HASH_LOAD_MULT  2
#define DP_AST_HASH_LOAD_SHIFT 0

static inline uint32_t
dp_peer_find_hash_index(struct dp_soc *soc,
			union dp_align_mac_addr *mac_addr)
{
	uint32_t index;

	index =
		mac_addr->align2.bytes_ab ^
		mac_addr->align2.bytes_cd ^
		mac_addr->align2.bytes_ef;

	index ^= index >> soc->peer_hash.idx_bits;
	index &= soc->peer_hash.mask;
	return index;
}

struct dp_peer *dp_peer_find_hash_find(
				struct dp_soc *soc, uint8_t *peer_mac_addr,
				int mac_addr_is_aligned, uint8_t vdev_id,
				enum dp_mod_id mod_id)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	uint32_t index;
	struct dp_peer *peer;

	if (!soc->peer_hash.bins)
		return NULL;

	if (mac_addr_is_aligned) {
		mac_addr = (union dp_align_mac_addr *)peer_mac_addr;
	} else {
		qdf_mem_copy(
			&local_mac_addr_aligned.raw[0],
			peer_mac_addr, QDF_MAC_ADDR_SIZE);
		mac_addr = &local_mac_addr_aligned;
	}
	index = dp_peer_find_hash_index(soc, mac_addr);
	qdf_spin_lock_bh(&soc->peer_hash_lock);
	TAILQ_FOREACH(peer, &soc->peer_hash.bins[index], hash_list_elem) {
		if (dp_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0 &&
		    ((peer->vdev->vdev_id == vdev_id) ||
		     (vdev_id == DP_VDEV_ALL))) {
			/* take peer reference before returning */
			if (dp_peer_get_ref(soc, peer, mod_id) !=
						QDF_STATUS_SUCCESS)
				peer = NULL;

			qdf_spin_unlock_bh(&soc->peer_hash_lock);
			return peer;
		}
	}
	qdf_spin_unlock_bh(&soc->peer_hash_lock);
	return NULL; /* failure */
}

qdf_export_symbol(dp_peer_find_hash_find);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_peer_find_hash_detach() - cleanup memory for peer_hash table
 * @soc: soc handle
 *
 * return: none
 */
static void dp_peer_find_hash_detach(struct dp_soc *soc)
{
	if (soc->peer_hash.bins) {
		qdf_mem_free(soc->peer_hash.bins);
		soc->peer_hash.bins = NULL;
		qdf_spinlock_destroy(&soc->peer_hash_lock);
	}

	if (soc->arch_ops.mlo_peer_find_hash_detach)
		soc->arch_ops.mlo_peer_find_hash_detach(soc);
}

/**
 * dp_peer_find_hash_attach() - allocate memory for peer_hash table
 * @soc: soc handle
 *
 * return: QDF_STATUS
 */
static QDF_STATUS dp_peer_find_hash_attach(struct dp_soc *soc)
{
	int i, hash_elems, log2;

	/* allocate the peer MAC address -> peer object hash table */
	hash_elems = soc->max_peers;
	hash_elems *= DP_PEER_HASH_LOAD_MULT;
	hash_elems >>= DP_PEER_HASH_LOAD_SHIFT;
	log2 = dp_log2_ceil(hash_elems);
	hash_elems = 1 << log2;

	soc->peer_hash.mask = hash_elems - 1;
	soc->peer_hash.idx_bits = log2;
	/* allocate an array of TAILQ peer object lists */
	soc->peer_hash.bins = qdf_mem_malloc(
		hash_elems * sizeof(TAILQ_HEAD(anonymous_tail_q, dp_peer)));
	if (!soc->peer_hash.bins)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&soc->peer_hash.bins[i]);

	qdf_spinlock_create(&soc->peer_hash_lock);

	if (soc->arch_ops.mlo_peer_find_hash_attach &&
	    (soc->arch_ops.mlo_peer_find_hash_attach(soc) !=
			QDF_STATUS_SUCCESS)) {
		dp_peer_find_hash_detach(soc);
		return QDF_STATUS_E_NOMEM;
	}
	return QDF_STATUS_SUCCESS;
}

void dp_peer_find_hash_add(struct dp_soc *soc, struct dp_peer *peer)
{
	unsigned index;

	index = dp_peer_find_hash_index(soc, &peer->mac_addr);
	if (peer->peer_type == CDP_LINK_PEER_TYPE) {
		qdf_spin_lock_bh(&soc->peer_hash_lock);

		if (QDF_IS_STATUS_ERROR(dp_peer_get_ref(soc, peer,
							DP_MOD_ID_CONFIG))) {
			dp_err("fail to get peer ref:" QDF_MAC_ADDR_FMT,
			       QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			qdf_spin_unlock_bh(&soc->peer_hash_lock);
			return;
		}

		/*
		 * It is important to add the new peer at the tail of
		 * peer list with the bin index. Together with having
		 * the hash_find function search from head to tail,
		 * this ensures that if two entries with the same MAC address
		 * are stored, the one added first will be found first.
		 */
		TAILQ_INSERT_TAIL(&soc->peer_hash.bins[index], peer,
				  hash_list_elem);

		qdf_spin_unlock_bh(&soc->peer_hash_lock);
	} else if (peer->peer_type == CDP_MLD_PEER_TYPE) {
		if (soc->arch_ops.mlo_peer_find_hash_add)
			soc->arch_ops.mlo_peer_find_hash_add(soc, peer);
	} else {
		dp_err("unknown peer type %d", peer->peer_type);
	}
}

void dp_peer_find_hash_remove(struct dp_soc *soc, struct dp_peer *peer)
{
	unsigned index;
	struct dp_peer *tmppeer = NULL;
	int found = 0;

	index = dp_peer_find_hash_index(soc, &peer->mac_addr);

	if (peer->peer_type == CDP_LINK_PEER_TYPE) {
		/* Check if tail is not empty before delete*/
		QDF_ASSERT(!TAILQ_EMPTY(&soc->peer_hash.bins[index]));

		qdf_spin_lock_bh(&soc->peer_hash_lock);
		TAILQ_FOREACH(tmppeer, &soc->peer_hash.bins[index],
			      hash_list_elem) {
			if (tmppeer == peer) {
				found = 1;
				break;
			}
		}
		QDF_ASSERT(found);
		TAILQ_REMOVE(&soc->peer_hash.bins[index], peer,
			     hash_list_elem);

		dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
		qdf_spin_unlock_bh(&soc->peer_hash_lock);
	} else if (peer->peer_type == CDP_MLD_PEER_TYPE) {
		if (soc->arch_ops.mlo_peer_find_hash_remove)
			soc->arch_ops.mlo_peer_find_hash_remove(soc, peer);
	} else {
		dp_err("unknown peer type %d", peer->peer_type);
	}
}

uint8_t dp_get_peer_link_id(struct dp_peer *peer)
{
	uint8_t link_id;

	link_id = IS_MLO_DP_LINK_PEER(peer) ? peer->link_id + 1 : 0;
	if (link_id < 1 || link_id > DP_MAX_MLO_LINKS)
		link_id = 0;

	return link_id;
}
#else
static QDF_STATUS dp_peer_find_hash_attach(struct dp_soc *soc)
{
	int i, hash_elems, log2;

	/* allocate the peer MAC address -> peer object hash table */
	hash_elems = soc->max_peers;
	hash_elems *= DP_PEER_HASH_LOAD_MULT;
	hash_elems >>= DP_PEER_HASH_LOAD_SHIFT;
	log2 = dp_log2_ceil(hash_elems);
	hash_elems = 1 << log2;

	soc->peer_hash.mask = hash_elems - 1;
	soc->peer_hash.idx_bits = log2;
	/* allocate an array of TAILQ peer object lists */
	soc->peer_hash.bins = qdf_mem_malloc(
		hash_elems * sizeof(TAILQ_HEAD(anonymous_tail_q, dp_peer)));
	if (!soc->peer_hash.bins)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&soc->peer_hash.bins[i]);

	qdf_spinlock_create(&soc->peer_hash_lock);
	return QDF_STATUS_SUCCESS;
}

static void dp_peer_find_hash_detach(struct dp_soc *soc)
{
	if (soc->peer_hash.bins) {
		qdf_mem_free(soc->peer_hash.bins);
		soc->peer_hash.bins = NULL;
		qdf_spinlock_destroy(&soc->peer_hash_lock);
	}
}

void dp_peer_find_hash_add(struct dp_soc *soc, struct dp_peer *peer)
{
	unsigned index;

	index = dp_peer_find_hash_index(soc, &peer->mac_addr);
	qdf_spin_lock_bh(&soc->peer_hash_lock);

	if (QDF_IS_STATUS_ERROR(dp_peer_get_ref(soc, peer, DP_MOD_ID_CONFIG))) {
		dp_err("unable to get peer ref at MAP mac: "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		qdf_spin_unlock_bh(&soc->peer_hash_lock);
		return;
	}

	/*
	 * It is important to add the new peer at the tail of the peer list
	 * with the bin index.  Together with having the hash_find function
	 * search from head to tail, this ensures that if two entries with
	 * the same MAC address are stored, the one added first will be
	 * found first.
	 */
	TAILQ_INSERT_TAIL(&soc->peer_hash.bins[index], peer, hash_list_elem);

	qdf_spin_unlock_bh(&soc->peer_hash_lock);
}

void dp_peer_find_hash_remove(struct dp_soc *soc, struct dp_peer *peer)
{
	unsigned index;
	struct dp_peer *tmppeer = NULL;
	int found = 0;

	index = dp_peer_find_hash_index(soc, &peer->mac_addr);
	/* Check if tail is not empty before delete*/
	QDF_ASSERT(!TAILQ_EMPTY(&soc->peer_hash.bins[index]));

	qdf_spin_lock_bh(&soc->peer_hash_lock);
	TAILQ_FOREACH(tmppeer, &soc->peer_hash.bins[index], hash_list_elem) {
		if (tmppeer == peer) {
			found = 1;
			break;
		}
	}
	QDF_ASSERT(found);
	TAILQ_REMOVE(&soc->peer_hash.bins[index], peer, hash_list_elem);

	dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
	qdf_spin_unlock_bh(&soc->peer_hash_lock);
}


#endif/* WLAN_FEATURE_11BE_MLO */

void dp_peer_vdev_list_add(struct dp_soc *soc, struct dp_vdev *vdev,
			   struct dp_peer *peer)
{
	/* only link peer will be added to vdev peer list */
	if (IS_MLO_DP_MLD_PEER(peer))
		return;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	if (QDF_IS_STATUS_ERROR(dp_peer_get_ref(soc, peer, DP_MOD_ID_CONFIG))) {
		dp_err("unable to get peer ref at MAP mac: "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		qdf_spin_unlock_bh(&vdev->peer_list_lock);
		return;
	}

	/* add this peer into the vdev's list */
	if (wlan_op_mode_sta == vdev->opmode)
		TAILQ_INSERT_HEAD(&vdev->peer_list, peer, peer_list_elem);
	else
		TAILQ_INSERT_TAIL(&vdev->peer_list, peer, peer_list_elem);

	vdev->num_peers++;
	qdf_spin_unlock_bh(&vdev->peer_list_lock);
}

void dp_peer_vdev_list_remove(struct dp_soc *soc, struct dp_vdev *vdev,
			      struct dp_peer *peer)
{
	uint8_t found = 0;
	struct dp_peer *tmppeer = NULL;

	/* only link peer will be added to vdev peer list */
	if (IS_MLO_DP_MLD_PEER(peer))
		return;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH(tmppeer, &peer->vdev->peer_list, peer_list_elem) {
		if (tmppeer == peer) {
			found = 1;
			break;
		}
	}

	if (found) {
		TAILQ_REMOVE(&peer->vdev->peer_list, peer,
			     peer_list_elem);
		dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
		vdev->num_peers--;
	} else {
		/*Ignoring the remove operation as peer not found*/
		dp_peer_debug("%pK: peer:%pK not found in vdev:%pK peerlist:%pK"
			      , soc, peer, vdev, &peer->vdev->peer_list);
	}
	qdf_spin_unlock_bh(&vdev->peer_list_lock);
}

void dp_txrx_peer_attach_add(struct dp_soc *soc,
			     struct dp_peer *peer,
			     struct dp_txrx_peer *txrx_peer)
{
	qdf_spin_lock_bh(&soc->peer_map_lock);

	peer->txrx_peer = txrx_peer;
	txrx_peer->bss_peer = peer->bss_peer;

	if (peer->peer_id == HTT_INVALID_PEER) {
		qdf_spin_unlock_bh(&soc->peer_map_lock);
		return;
	}

	txrx_peer->peer_id = peer->peer_id;

	QDF_ASSERT(soc->peer_id_to_obj_map[peer->peer_id]);

	qdf_spin_unlock_bh(&soc->peer_map_lock);
}

void dp_peer_find_id_to_obj_add(struct dp_soc *soc,
				struct dp_peer *peer,
				uint16_t peer_id)
{
	QDF_ASSERT(peer_id <= soc->max_peer_id);

	qdf_spin_lock_bh(&soc->peer_map_lock);

	peer->peer_id = peer_id;

	if (QDF_IS_STATUS_ERROR(dp_peer_get_ref(soc, peer, DP_MOD_ID_CONFIG))) {
		dp_err("unable to get peer ref at MAP mac: "QDF_MAC_ADDR_FMT" peer_id %u",
		       QDF_MAC_ADDR_REF(peer->mac_addr.raw), peer_id);
		qdf_spin_unlock_bh(&soc->peer_map_lock);
		return;
	}

	if (!soc->peer_id_to_obj_map[peer_id]) {
		soc->peer_id_to_obj_map[peer_id] = peer;
		if (peer->txrx_peer)
			peer->txrx_peer->peer_id = peer_id;
	} else {
		/* Peer map event came for peer_id which
		 * is already mapped, this is not expected
		 */
		dp_err("peer %pK(" QDF_MAC_ADDR_FMT ")map failed, id %d mapped "
		       "to peer %pK, Stats: peer(map %u unmap %u "
		       "invalid unmap %u) mld per(map %u unmap %u)",
		       peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw), peer_id,
		       soc->peer_id_to_obj_map[peer_id],
		       soc->stats.t2h_msg_stats.peer_map,
		       (soc->stats.t2h_msg_stats.peer_unmap -
			soc->stats.t2h_msg_stats.ml_peer_unmap),
		       soc->stats.t2h_msg_stats.invalid_peer_unmap,
		       soc->stats.t2h_msg_stats.ml_peer_map,
		       soc->stats.t2h_msg_stats.ml_peer_unmap);
		dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
		qdf_assert_always(0);
	}
	qdf_spin_unlock_bh(&soc->peer_map_lock);
}

void dp_peer_find_id_to_obj_remove(struct dp_soc *soc,
				   uint16_t peer_id)
{
	struct dp_peer *peer = NULL;
	QDF_ASSERT(peer_id <= soc->max_peer_id);

	qdf_spin_lock_bh(&soc->peer_map_lock);
	peer = soc->peer_id_to_obj_map[peer_id];
	if (!peer) {
		dp_err("unable to get peer during peer id obj map remove");
		qdf_spin_unlock_bh(&soc->peer_map_lock);
		return;
	}
	peer->peer_id = HTT_INVALID_PEER;
	if (peer->txrx_peer)
		peer->txrx_peer->peer_id = HTT_INVALID_PEER;
	soc->peer_id_to_obj_map[peer_id] = NULL;
	dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
	qdf_spin_unlock_bh(&soc->peer_map_lock);
}

#ifdef FEATURE_MEC
QDF_STATUS dp_peer_mec_hash_attach(struct dp_soc *soc)
{
	int log2, hash_elems, i;

	log2 = dp_log2_ceil(DP_PEER_MAX_MEC_IDX);
	hash_elems = 1 << log2;

	soc->mec_hash.mask = hash_elems - 1;
	soc->mec_hash.idx_bits = log2;

	dp_peer_info("%pK: max mec index: %d",
		     soc, DP_PEER_MAX_MEC_IDX);

	/* allocate an array of TAILQ mec object lists */
	soc->mec_hash.bins = qdf_mem_malloc(hash_elems *
					    sizeof(TAILQ_HEAD(anonymous_tail_q,
							      dp_mec_entry)));

	if (!soc->mec_hash.bins)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&soc->mec_hash.bins[i]);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_peer_mec_hash_index() - Compute the MEC hash from MAC address
 * @soc: SoC handle
 * @mac_addr: MAC address
 *
 * Return: MEC hash
 */
static inline uint32_t dp_peer_mec_hash_index(struct dp_soc *soc,
					      union dp_align_mac_addr *mac_addr)
{
	uint32_t index;

	index =
		mac_addr->align2.bytes_ab ^
		mac_addr->align2.bytes_cd ^
		mac_addr->align2.bytes_ef;
	index ^= index >> soc->mec_hash.idx_bits;
	index &= soc->mec_hash.mask;
	return index;
}

struct dp_mec_entry *dp_peer_mec_hash_find_by_pdevid(struct dp_soc *soc,
						     uint8_t pdev_id,
						     uint8_t *mec_mac_addr)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	uint32_t index;
	struct dp_mec_entry *mecentry;

	qdf_mem_copy(&local_mac_addr_aligned.raw[0],
		     mec_mac_addr, QDF_MAC_ADDR_SIZE);
	mac_addr = &local_mac_addr_aligned;

	index = dp_peer_mec_hash_index(soc, mac_addr);
	TAILQ_FOREACH(mecentry, &soc->mec_hash.bins[index], hash_list_elem) {
		if ((pdev_id == mecentry->pdev_id) &&
		    !dp_peer_find_mac_addr_cmp(mac_addr, &mecentry->mac_addr))
			return mecentry;
	}

	return NULL;
}

/**
 * dp_peer_mec_hash_add() - Add MEC entry into hash table
 * @soc: SoC handle
 * @mecentry: MEC entry
 *
 * This function adds the MEC entry into SoC MEC hash table
 *
 * Return: None
 */
static inline void dp_peer_mec_hash_add(struct dp_soc *soc,
					struct dp_mec_entry *mecentry)
{
	uint32_t index;

	index = dp_peer_mec_hash_index(soc, &mecentry->mac_addr);
	qdf_spin_lock_bh(&soc->mec_lock);
	TAILQ_INSERT_TAIL(&soc->mec_hash.bins[index], mecentry, hash_list_elem);
	qdf_spin_unlock_bh(&soc->mec_lock);
}

QDF_STATUS dp_peer_mec_add_entry(struct dp_soc *soc,
				 struct dp_vdev *vdev,
				 uint8_t *mac_addr)
{
	struct dp_mec_entry *mecentry = NULL;
	struct dp_pdev *pdev = NULL;

	if (!vdev) {
		dp_peer_err("%pK: Peers vdev is NULL", soc);
		return QDF_STATUS_E_INVAL;
	}

	pdev = vdev->pdev;

	if (qdf_unlikely(qdf_atomic_read(&soc->mec_cnt) >=
					 DP_PEER_MAX_MEC_ENTRY)) {
		dp_peer_warn("%pK: max MEC entry limit reached mac_addr: "
			     QDF_MAC_ADDR_FMT, soc, QDF_MAC_ADDR_REF(mac_addr));
		return QDF_STATUS_E_NOMEM;
	}

	qdf_spin_lock_bh(&soc->mec_lock);
	mecentry = dp_peer_mec_hash_find_by_pdevid(soc, pdev->pdev_id,
						   mac_addr);
	if (qdf_likely(mecentry)) {
		mecentry->is_active = TRUE;
		qdf_spin_unlock_bh(&soc->mec_lock);
		return QDF_STATUS_E_ALREADY;
	}

	qdf_spin_unlock_bh(&soc->mec_lock);

	dp_peer_debug("%pK: pdevid: %u vdev: %u type: MEC mac_addr: "
		      QDF_MAC_ADDR_FMT,
		      soc, pdev->pdev_id, vdev->vdev_id,
		      QDF_MAC_ADDR_REF(mac_addr));

	mecentry = (struct dp_mec_entry *)
			qdf_mem_malloc(sizeof(struct dp_mec_entry));

	if (qdf_unlikely(!mecentry)) {
		dp_peer_err("%pK: fail to allocate mecentry", soc);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_copy_macaddr((struct qdf_mac_addr *)&mecentry->mac_addr.raw[0],
			 (struct qdf_mac_addr *)mac_addr);
	mecentry->pdev_id = pdev->pdev_id;
	mecentry->vdev_id = vdev->vdev_id;
	mecentry->is_active = TRUE;
	dp_peer_mec_hash_add(soc, mecentry);

	qdf_atomic_inc(&soc->mec_cnt);
	DP_STATS_INC(soc, mec.added, 1);

	return QDF_STATUS_SUCCESS;
}

void dp_peer_mec_detach_entry(struct dp_soc *soc, struct dp_mec_entry *mecentry,
			      void *ptr)
{
	uint32_t index = dp_peer_mec_hash_index(soc, &mecentry->mac_addr);

	TAILQ_HEAD(, dp_mec_entry) * free_list = ptr;

	TAILQ_REMOVE(&soc->mec_hash.bins[index], mecentry,
		     hash_list_elem);
	TAILQ_INSERT_TAIL(free_list, mecentry, hash_list_elem);
}

void dp_peer_mec_free_list(struct dp_soc *soc, void *ptr)
{
	struct dp_mec_entry *mecentry, *mecentry_next;

	TAILQ_HEAD(, dp_mec_entry) * free_list = ptr;

	TAILQ_FOREACH_SAFE(mecentry, free_list, hash_list_elem,
			   mecentry_next) {
		dp_peer_debug("%pK: MEC delete for mac_addr " QDF_MAC_ADDR_FMT,
			      soc, QDF_MAC_ADDR_REF(&mecentry->mac_addr));
		qdf_mem_free(mecentry);
		qdf_atomic_dec(&soc->mec_cnt);
		DP_STATS_INC(soc, mec.deleted, 1);
	}
}

void dp_peer_mec_hash_detach(struct dp_soc *soc)
{
	dp_peer_mec_flush_entries(soc);
	qdf_mem_free(soc->mec_hash.bins);
	soc->mec_hash.bins = NULL;
}

void dp_peer_mec_spinlock_destroy(struct dp_soc *soc)
{
	qdf_spinlock_destroy(&soc->mec_lock);
}

void dp_peer_mec_spinlock_create(struct dp_soc *soc)
{
	qdf_spinlock_create(&soc->mec_lock);
}
#else
QDF_STATUS dp_peer_mec_hash_attach(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

void dp_peer_mec_hash_detach(struct dp_soc *soc)
{
}
#endif

#ifdef FEATURE_AST
#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_peer_exist_on_pdev() - check if peer with mac address exist on pdev
 *
 * @soc: Datapath SOC handle
 * @peer_mac_addr: peer mac address
 * @mac_addr_is_aligned: is mac address aligned
 * @pdev: Datapath PDEV handle
 *
 * Return: true if peer found else return false
 */
static bool dp_peer_exist_on_pdev(struct dp_soc *soc,
				  uint8_t *peer_mac_addr,
				  int mac_addr_is_aligned,
				  struct dp_pdev *pdev)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	unsigned int index;
	struct dp_peer *peer;
	bool found = false;

	if (mac_addr_is_aligned) {
		mac_addr = (union dp_align_mac_addr *)peer_mac_addr;
	} else {
		qdf_mem_copy(
			&local_mac_addr_aligned.raw[0],
			peer_mac_addr, QDF_MAC_ADDR_SIZE);
		mac_addr = &local_mac_addr_aligned;
	}
	index = dp_peer_find_hash_index(soc, mac_addr);
	qdf_spin_lock_bh(&soc->peer_hash_lock);
	TAILQ_FOREACH(peer, &soc->peer_hash.bins[index], hash_list_elem) {
		if (dp_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0 &&
		    (peer->vdev->pdev == pdev)) {
			found = true;
			break;
		}
	}
	qdf_spin_unlock_bh(&soc->peer_hash_lock);

	if (found)
		return found;

	peer = dp_mld_peer_find_hash_find(soc, peer_mac_addr,
					  mac_addr_is_aligned, DP_VDEV_ALL,
					  DP_MOD_ID_CDP);
	if (peer) {
		if (peer->vdev->pdev == pdev)
			found = true;
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	}

	return found;
}
#else
static bool dp_peer_exist_on_pdev(struct dp_soc *soc,
				  uint8_t *peer_mac_addr,
				  int mac_addr_is_aligned,
				  struct dp_pdev *pdev)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	unsigned int index;
	struct dp_peer *peer;
	bool found = false;

	if (mac_addr_is_aligned) {
		mac_addr = (union dp_align_mac_addr *)peer_mac_addr;
	} else {
		qdf_mem_copy(
			&local_mac_addr_aligned.raw[0],
			peer_mac_addr, QDF_MAC_ADDR_SIZE);
		mac_addr = &local_mac_addr_aligned;
	}
	index = dp_peer_find_hash_index(soc, mac_addr);
	qdf_spin_lock_bh(&soc->peer_hash_lock);
	TAILQ_FOREACH(peer, &soc->peer_hash.bins[index], hash_list_elem) {
		if (dp_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0 &&
		    (peer->vdev->pdev == pdev)) {
			found = true;
			break;
		}
	}
	qdf_spin_unlock_bh(&soc->peer_hash_lock);
	return found;
}
#endif /* WLAN_FEATURE_11BE_MLO */

QDF_STATUS dp_peer_ast_hash_attach(struct dp_soc *soc)
{
	int i, hash_elems, log2;
	unsigned int max_ast_idx = wlan_cfg_get_max_ast_idx(soc->wlan_cfg_ctx);

	hash_elems = ((max_ast_idx * DP_AST_HASH_LOAD_MULT) >>
		DP_AST_HASH_LOAD_SHIFT);

	log2 = dp_log2_ceil(hash_elems);
	hash_elems = 1 << log2;

	soc->ast_hash.mask = hash_elems - 1;
	soc->ast_hash.idx_bits = log2;

	dp_peer_info("%pK: ast hash_elems: %d, max_ast_idx: %d",
		     soc, hash_elems, max_ast_idx);

	/* allocate an array of TAILQ peer object lists */
	soc->ast_hash.bins = qdf_mem_malloc(
		hash_elems * sizeof(TAILQ_HEAD(anonymous_tail_q,
				dp_ast_entry)));

	if (!soc->ast_hash.bins)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&soc->ast_hash.bins[i]);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_peer_ast_cleanup() - cleanup the references
 * @soc: SoC handle
 * @ast: ast entry
 *
 * Return: None
 */
static inline void dp_peer_ast_cleanup(struct dp_soc *soc,
				       struct dp_ast_entry *ast)
{
	txrx_ast_free_cb cb = ast->callback;
	void *cookie = ast->cookie;

	dp_peer_debug("mac_addr: " QDF_MAC_ADDR_FMT ", cb: %pK, cookie: %pK",
		      QDF_MAC_ADDR_REF(ast->mac_addr.raw), cb, cookie);

	/* Call the callbacks to free up the cookie */
	if (cb) {
		ast->callback = NULL;
		ast->cookie = NULL;
		cb(soc->ctrl_psoc,
		   dp_soc_to_cdp_soc(soc),
		   cookie,
		   CDP_TXRX_AST_DELETE_IN_PROGRESS);
	}
}

void dp_peer_ast_hash_detach(struct dp_soc *soc)
{
	unsigned int index;
	struct dp_ast_entry *ast, *ast_next;

	if (!soc->ast_hash.mask)
		return;

	if (!soc->ast_hash.bins)
		return;

	dp_peer_debug("%pK: num_ast_entries: %u", soc, soc->num_ast_entries);

	qdf_spin_lock_bh(&soc->ast_lock);
	for (index = 0; index <= soc->ast_hash.mask; index++) {
		if (!TAILQ_EMPTY(&soc->ast_hash.bins[index])) {
			TAILQ_FOREACH_SAFE(ast, &soc->ast_hash.bins[index],
					   hash_list_elem, ast_next) {
				TAILQ_REMOVE(&soc->ast_hash.bins[index], ast,
					     hash_list_elem);
				dp_peer_ast_cleanup(soc, ast);
				soc->num_ast_entries--;
				qdf_mem_free(ast);
			}
		}
	}
	qdf_spin_unlock_bh(&soc->ast_lock);

	qdf_mem_free(soc->ast_hash.bins);
	soc->ast_hash.bins = NULL;
}

/**
 * dp_peer_ast_hash_index() - Compute the AST hash from MAC address
 * @soc: SoC handle
 * @mac_addr: MAC address
 *
 * Return: AST hash
 */
static inline uint32_t dp_peer_ast_hash_index(struct dp_soc *soc,
					      union dp_align_mac_addr *mac_addr)
{
	uint32_t index;

	index =
		mac_addr->align2.bytes_ab ^
		mac_addr->align2.bytes_cd ^
		mac_addr->align2.bytes_ef;
	index ^= index >> soc->ast_hash.idx_bits;
	index &= soc->ast_hash.mask;
	return index;
}

/**
 * dp_peer_ast_hash_add() - Add AST entry into hash table
 * @soc: SoC handle
 * @ase: AST entry
 *
 * This function adds the AST entry into SoC AST hash table
 * It assumes caller has taken the ast lock to protect the access to this table
 *
 * Return: None
 */
static inline void dp_peer_ast_hash_add(struct dp_soc *soc,
					struct dp_ast_entry *ase)
{
	uint32_t index;

	index = dp_peer_ast_hash_index(soc, &ase->mac_addr);
	TAILQ_INSERT_TAIL(&soc->ast_hash.bins[index], ase, hash_list_elem);
}

void dp_peer_ast_hash_remove(struct dp_soc *soc,
			     struct dp_ast_entry *ase)
{
	unsigned index;
	struct dp_ast_entry *tmpase;
	int found = 0;

	if (soc->ast_offload_support && !soc->host_ast_db_enable)
		return;

	index = dp_peer_ast_hash_index(soc, &ase->mac_addr);
	/* Check if tail is not empty before delete*/
	QDF_ASSERT(!TAILQ_EMPTY(&soc->ast_hash.bins[index]));

	dp_peer_debug("ID: %u idx: %u mac_addr: " QDF_MAC_ADDR_FMT,
		      ase->peer_id, index, QDF_MAC_ADDR_REF(ase->mac_addr.raw));

	TAILQ_FOREACH(tmpase, &soc->ast_hash.bins[index], hash_list_elem) {
		if (tmpase == ase) {
			found = 1;
			break;
		}
	}

	QDF_ASSERT(found);

	if (found)
		TAILQ_REMOVE(&soc->ast_hash.bins[index], ase, hash_list_elem);
}

struct dp_ast_entry *dp_peer_ast_hash_find_by_vdevid(struct dp_soc *soc,
						     uint8_t *ast_mac_addr,
						     uint8_t vdev_id)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	uint32_t index;
	struct dp_ast_entry *ase;

	qdf_mem_copy(&local_mac_addr_aligned.raw[0],
		     ast_mac_addr, QDF_MAC_ADDR_SIZE);
	mac_addr = &local_mac_addr_aligned;

	index = dp_peer_ast_hash_index(soc, mac_addr);
	TAILQ_FOREACH(ase, &soc->ast_hash.bins[index], hash_list_elem) {
		if ((vdev_id == ase->vdev_id) &&
		    !dp_peer_find_mac_addr_cmp(mac_addr, &ase->mac_addr)) {
			return ase;
		}
	}

	return NULL;
}

struct dp_ast_entry *dp_peer_ast_hash_find_by_pdevid(struct dp_soc *soc,
						     uint8_t *ast_mac_addr,
						     uint8_t pdev_id)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	uint32_t index;
	struct dp_ast_entry *ase;

	qdf_mem_copy(&local_mac_addr_aligned.raw[0],
		     ast_mac_addr, QDF_MAC_ADDR_SIZE);
	mac_addr = &local_mac_addr_aligned;

	index = dp_peer_ast_hash_index(soc, mac_addr);
	TAILQ_FOREACH(ase, &soc->ast_hash.bins[index], hash_list_elem) {
		if ((pdev_id == ase->pdev_id) &&
		    !dp_peer_find_mac_addr_cmp(mac_addr, &ase->mac_addr)) {
			return ase;
		}
	}

	return NULL;
}

struct dp_ast_entry *dp_peer_ast_hash_find_soc(struct dp_soc *soc,
					       uint8_t *ast_mac_addr)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	unsigned index;
	struct dp_ast_entry *ase;

	if (!soc->ast_hash.bins)
		return NULL;

	qdf_mem_copy(&local_mac_addr_aligned.raw[0],
			ast_mac_addr, QDF_MAC_ADDR_SIZE);
	mac_addr = &local_mac_addr_aligned;

	index = dp_peer_ast_hash_index(soc, mac_addr);
	TAILQ_FOREACH(ase, &soc->ast_hash.bins[index], hash_list_elem) {
		if (dp_peer_find_mac_addr_cmp(mac_addr, &ase->mac_addr) == 0) {
			return ase;
		}
	}

	return NULL;
}

struct dp_ast_entry *dp_peer_ast_hash_find_soc_by_type(
					struct dp_soc *soc,
					uint8_t *ast_mac_addr,
					enum cdp_txrx_ast_entry_type type)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	unsigned index;
	struct dp_ast_entry *ase;

	if (!soc->ast_hash.bins)
		return NULL;

	qdf_mem_copy(&local_mac_addr_aligned.raw[0],
			ast_mac_addr, QDF_MAC_ADDR_SIZE);
	mac_addr = &local_mac_addr_aligned;

	index = dp_peer_ast_hash_index(soc, mac_addr);
	TAILQ_FOREACH(ase, &soc->ast_hash.bins[index], hash_list_elem) {
		if (dp_peer_find_mac_addr_cmp(mac_addr, &ase->mac_addr) == 0 &&
		    ase->type == type) {
			return ase;
		}
	}

	return NULL;
}

/**
 * dp_peer_map_ipa_evt() - Send peer map event to IPA
 * @soc: SoC handle
 * @peer: peer to which ast node belongs
 * @ast_entry: AST entry
 * @mac_addr: MAC address of ast node
 *
 * Return: None
 */
#if defined(IPA_OFFLOAD) && defined(QCA_IPA_LL_TX_FLOW_CONTROL)
static inline
void dp_peer_map_ipa_evt(struct dp_soc *soc, struct dp_peer *peer,
			 struct dp_ast_entry *ast_entry, uint8_t *mac_addr)
{
	if (ast_entry || (peer->vdev && peer->vdev->proxysta_vdev)) {
		if (soc->cdp_soc.ol_ops->peer_map_event) {
			soc->cdp_soc.ol_ops->peer_map_event(
			soc->ctrl_psoc, ast_entry->peer_id,
			ast_entry->ast_idx, ast_entry->vdev_id,
			mac_addr, ast_entry->type, ast_entry->ast_hash_value);
		}
	} else {
		dp_peer_info("%pK: AST entry not found", soc);
	}
}

/**
 * dp_peer_unmap_ipa_evt() - Send peer unmap event to IPA
 * @soc: SoC handle
 * @peer_id: Peerid
 * @vdev_id: Vdev id
 * @mac_addr: Peer mac address
 *
 * Return: None
 */
static inline
void dp_peer_unmap_ipa_evt(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t vdev_id, uint8_t *mac_addr)
{
	if (soc->cdp_soc.ol_ops->peer_unmap_event) {
		soc->cdp_soc.ol_ops->peer_unmap_event(soc->ctrl_psoc,
						      peer_id, vdev_id,
						      mac_addr);
	}
}
#else
static inline
void dp_peer_unmap_ipa_evt(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t vdev_id, uint8_t *mac_addr)
{
}

static inline
void dp_peer_map_ipa_evt(struct dp_soc *soc, struct dp_peer *peer,
			 struct dp_ast_entry *ast_entry, uint8_t *mac_addr)
{
}
#endif

QDF_STATUS dp_peer_host_add_map_ast(struct dp_soc *soc, uint16_t peer_id,
				    uint8_t *mac_addr, uint16_t hw_peer_id,
				    uint8_t vdev_id, uint16_t ast_hash,
				    uint8_t is_wds)
{
	struct dp_vdev *vdev;
	struct dp_ast_entry *ast_entry;
	enum cdp_txrx_ast_entry_type type;
	struct dp_peer *peer;
	struct dp_peer *old_peer;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (is_wds)
		type = CDP_TXRX_AST_TYPE_WDS;
	else
		type = CDP_TXRX_AST_TYPE_STATIC;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_HTT);
	if (!peer) {
		dp_peer_info("Peer not found soc:%pK: peer_id %d, peer_mac " QDF_MAC_ADDR_FMT ", vdev_id %d",
			     soc, peer_id,
			     QDF_MAC_ADDR_REF(mac_addr), vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!is_wds && IS_MLO_DP_MLD_PEER(peer))
		type = CDP_TXRX_AST_TYPE_MLD;

	vdev = peer->vdev;
	if (!vdev) {
		dp_peer_err("%pK: Peers vdev is NULL", soc);
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	if (!dp_peer_state_cmp(peer, DP_PEER_STATE_ACTIVE)) {
		if (type != CDP_TXRX_AST_TYPE_STATIC &&
		    type != CDP_TXRX_AST_TYPE_MLD &&
		    type != CDP_TXRX_AST_TYPE_SELF) {
			status = QDF_STATUS_E_BUSY;
			goto fail;
		}
	}

	dp_peer_debug("%pK: vdev: %u  ast_entry->type: %d peer_mac: " QDF_MAC_ADDR_FMT " peer: %pK mac " QDF_MAC_ADDR_FMT,
		      soc, vdev->vdev_id, type,
		      QDF_MAC_ADDR_REF(peer->mac_addr.raw), peer,
		      QDF_MAC_ADDR_REF(mac_addr));

	/*
	 * In MLO scenario, there is possibility for same mac address
	 * on both link mac address and MLD mac address.
	 * Duplicate AST map needs to be handled for non-mld type.
	 */
	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_soc(soc, mac_addr);
	if (ast_entry && type != CDP_TXRX_AST_TYPE_MLD) {
		dp_peer_debug("AST present ID %d vid %d mac " QDF_MAC_ADDR_FMT,
			      hw_peer_id, vdev_id,
			      QDF_MAC_ADDR_REF(mac_addr));

		old_peer = __dp_peer_get_ref_by_id(soc, ast_entry->peer_id,
						   DP_MOD_ID_AST);
		if (!old_peer) {
			dp_peer_info("Peer not found soc:%pK: peer_id %d, peer_mac " QDF_MAC_ADDR_FMT ", vdev_id %d",
				     soc, ast_entry->peer_id,
				     QDF_MAC_ADDR_REF(mac_addr), vdev_id);
			qdf_spin_unlock_bh(&soc->ast_lock);
			status = QDF_STATUS_E_INVAL;
			goto fail;
		}

		dp_peer_unlink_ast_entry(soc, ast_entry, old_peer);
		dp_peer_free_ast_entry(soc, ast_entry);
		if (old_peer)
			dp_peer_unref_delete(old_peer, DP_MOD_ID_AST);
	}

	ast_entry = (struct dp_ast_entry *)
		qdf_mem_malloc(sizeof(struct dp_ast_entry));
	if (!ast_entry) {
		dp_peer_err("%pK: fail to allocate ast_entry", soc);
		qdf_spin_unlock_bh(&soc->ast_lock);
		QDF_ASSERT(0);
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	qdf_mem_copy(&ast_entry->mac_addr.raw[0], mac_addr, QDF_MAC_ADDR_SIZE);
	ast_entry->pdev_id = vdev->pdev->pdev_id;
	ast_entry->is_mapped = false;
	ast_entry->delete_in_progress = false;
	ast_entry->next_hop = 0;
	ast_entry->vdev_id = vdev->vdev_id;
	ast_entry->type = type;

	switch (type) {
	case CDP_TXRX_AST_TYPE_STATIC:
		if (peer->vdev->opmode == wlan_op_mode_sta)
			ast_entry->type = CDP_TXRX_AST_TYPE_STA_BSS;
		break;
	case CDP_TXRX_AST_TYPE_WDS:
		ast_entry->next_hop = 1;
		break;
	case CDP_TXRX_AST_TYPE_MLD:
		break;
	default:
		dp_peer_alert("%pK: Incorrect AST entry type", soc);
	}

	ast_entry->is_active = TRUE;
	DP_STATS_INC(soc, ast.added, 1);
	soc->num_ast_entries++;
	dp_peer_ast_hash_add(soc, ast_entry);

	ast_entry->ast_idx = hw_peer_id;
	ast_entry->ast_hash_value = ast_hash;
	ast_entry->peer_id = peer_id;
	TAILQ_INSERT_TAIL(&peer->ast_entry_list, ast_entry,
			  ase_list_elem);

	dp_peer_map_ipa_evt(soc, peer, ast_entry, mac_addr);

	qdf_spin_unlock_bh(&soc->ast_lock);
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_HTT);

	return status;
}

/**
 * dp_peer_map_ast() - Map the ast entry with HW AST Index
 * @soc: SoC handle
 * @peer: peer to which ast node belongs
 * @mac_addr: MAC address of ast node
 * @hw_peer_id: HW AST Index returned by target in peer map event
 * @vdev_id: vdev id for VAP to which the peer belongs to
 * @ast_hash: ast hash value in HW
 * @is_wds: flag to indicate peer map event for WDS ast entry
 *
 * Return: QDF_STATUS code
 */
static inline QDF_STATUS dp_peer_map_ast(struct dp_soc *soc,
					 struct dp_peer *peer,
					 uint8_t *mac_addr,
					 uint16_t hw_peer_id,
					 uint8_t vdev_id,
					 uint16_t ast_hash,
					 uint8_t is_wds)
{
	struct dp_ast_entry *ast_entry = NULL;
	enum cdp_txrx_ast_entry_type peer_type = CDP_TXRX_AST_TYPE_STATIC;
	void *cookie = NULL;
	txrx_ast_free_cb cb = NULL;
	QDF_STATUS err = QDF_STATUS_SUCCESS;

	if (soc->ast_offload_support && !wlan_cfg_get_dp_soc_dpdk_cfg(soc->ctrl_psoc))
		return QDF_STATUS_SUCCESS;

	dp_peer_err("%pK: peer %pK ID %d vid %d mac " QDF_MAC_ADDR_FMT,
		    soc, peer, hw_peer_id, vdev_id,
		    QDF_MAC_ADDR_REF(mac_addr));

	qdf_spin_lock_bh(&soc->ast_lock);

	ast_entry = dp_peer_ast_hash_find_by_vdevid(soc, mac_addr, vdev_id);

	if (is_wds) {
		/*
		 * While processing peer map of AST entry if the next hop peer is
		 * deleted free the AST entry as it is not attached to peer yet
		 */
		if (!peer) {
			if (ast_entry)
				dp_peer_free_ast_entry(soc, ast_entry);

			qdf_spin_unlock_bh(&soc->ast_lock);

			dp_peer_alert("Peer is NULL for WDS entry mac "
				      QDF_MAC_ADDR_FMT " ",
				      QDF_MAC_ADDR_REF(mac_addr));
			return QDF_STATUS_E_INVAL;
		}
		/*
		 * In certain cases like Auth attack on a repeater
		 * can result in the number of ast_entries falling
		 * in the same hash bucket to exceed the max_skid
		 * length supported by HW in root AP. In these cases
		 * the FW will return the hw_peer_id (ast_index) as
		 * 0xffff indicating HW could not add the entry in
		 * its table. Host has to delete the entry from its
		 * table in these cases.
		 */
		if (hw_peer_id == HTT_INVALID_PEER) {
			DP_STATS_INC(soc, ast.map_err, 1);
			if (ast_entry) {
				if (ast_entry->is_mapped) {
					soc->ast_table[ast_entry->ast_idx] =
						NULL;
				}

				cb = ast_entry->callback;
				cookie = ast_entry->cookie;
				peer_type = ast_entry->type;

				dp_peer_unlink_ast_entry(soc, ast_entry, peer);
				dp_peer_free_ast_entry(soc, ast_entry);

				qdf_spin_unlock_bh(&soc->ast_lock);

				if (cb) {
					cb(soc->ctrl_psoc,
					   dp_soc_to_cdp_soc(soc),
					   cookie,
					   CDP_TXRX_AST_DELETED);
				}
			} else {
				qdf_spin_unlock_bh(&soc->ast_lock);
				dp_peer_alert("AST entry not found with peer %pK peer_id %u peer_mac " QDF_MAC_ADDR_FMT " mac_addr " QDF_MAC_ADDR_FMT " vdev_id %u next_hop %u",
					      peer, peer->peer_id,
					      QDF_MAC_ADDR_REF(peer->mac_addr.raw),
					      QDF_MAC_ADDR_REF(mac_addr),
					      vdev_id, is_wds);
			}
			err = QDF_STATUS_E_INVAL;

			dp_hmwds_ast_add_notify(peer, mac_addr,
						peer_type, err, true);

			return err;
		}
	}

	if (!peer) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		dp_peer_alert("Peer is NULL for mac " QDF_MAC_ADDR_FMT " ",
			      QDF_MAC_ADDR_REF(mac_addr));
		return QDF_STATUS_E_INVAL;
	}

	if (ast_entry) {
		ast_entry->ast_idx = hw_peer_id;
		soc->ast_table[hw_peer_id] = ast_entry;
		ast_entry->is_active = TRUE;
		peer_type = ast_entry->type;
		ast_entry->ast_hash_value = ast_hash;
		ast_entry->is_mapped = TRUE;
		qdf_assert_always(ast_entry->peer_id == HTT_INVALID_PEER);

		ast_entry->peer_id = peer->peer_id;
		TAILQ_INSERT_TAIL(&peer->ast_entry_list, ast_entry,
				  ase_list_elem);
	}

	if (ast_entry || (peer->vdev && peer->vdev->proxysta_vdev) ||
	    wlan_cfg_get_dp_soc_dpdk_cfg(soc->ctrl_psoc)) {
		if (soc->cdp_soc.ol_ops->peer_map_event) {
			soc->cdp_soc.ol_ops->peer_map_event(
			soc->ctrl_psoc, peer->peer_id,
			hw_peer_id, vdev_id,
			mac_addr, peer_type, ast_hash);
		}
	} else {
		dp_peer_err("%pK: AST entry not found", soc);
		err = QDF_STATUS_E_NOENT;
	}

	qdf_spin_unlock_bh(&soc->ast_lock);

	dp_hmwds_ast_add_notify(peer, mac_addr,
				peer_type, err, true);

	return err;
}

void dp_peer_free_hmwds_cb(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			   struct cdp_soc *dp_soc,
			   void *cookie,
			   enum cdp_ast_free_status status)
{
	struct dp_ast_free_cb_params *param =
		(struct dp_ast_free_cb_params *)cookie;
	struct dp_soc *soc = (struct dp_soc *)dp_soc;
	struct dp_peer *peer = NULL;
	QDF_STATUS err = QDF_STATUS_SUCCESS;

	if (status != CDP_TXRX_AST_DELETED) {
		qdf_mem_free(cookie);
		return;
	}

	peer = dp_peer_find_hash_find(soc, &param->peer_mac_addr.raw[0],
				      0, param->vdev_id, DP_MOD_ID_AST);
	if (peer) {
		err = dp_peer_add_ast(soc, peer,
				      &param->mac_addr.raw[0],
				      param->type,
				      param->flags);

		dp_hmwds_ast_add_notify(peer, &param->mac_addr.raw[0],
					param->type, err, false);

		dp_peer_unref_delete(peer, DP_MOD_ID_AST);
	}
	qdf_mem_free(cookie);
}

QDF_STATUS dp_peer_add_ast(struct dp_soc *soc,
			   struct dp_peer *peer,
			   uint8_t *mac_addr,
			   enum cdp_txrx_ast_entry_type type,
			   uint32_t flags)
{
	struct dp_ast_entry *ast_entry = NULL;
	struct dp_vdev *vdev = NULL;
	struct dp_pdev *pdev = NULL;
	txrx_ast_free_cb cb = NULL;
	void *cookie = NULL;
	struct dp_peer *vap_bss_peer = NULL;
	bool is_peer_found = false;
	int status = 0;

	if (soc->ast_offload_support)
		return QDF_STATUS_E_INVAL;

	vdev = peer->vdev;
	if (!vdev) {
		dp_peer_err("%pK: Peers vdev is NULL", soc);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	pdev = vdev->pdev;

	is_peer_found = dp_peer_exist_on_pdev(soc, mac_addr, 0, pdev);

	qdf_spin_lock_bh(&soc->ast_lock);

	if (!dp_peer_state_cmp(peer, DP_PEER_STATE_ACTIVE)) {
		if ((type != CDP_TXRX_AST_TYPE_STATIC) &&
		    (type != CDP_TXRX_AST_TYPE_SELF)) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			return QDF_STATUS_E_BUSY;
		}
	}

	dp_peer_debug("%pK: pdevid: %u vdev: %u  ast_entry->type: %d flags: 0x%x peer_mac: " QDF_MAC_ADDR_FMT " peer: %pK mac " QDF_MAC_ADDR_FMT,
		      soc, pdev->pdev_id, vdev->vdev_id, type, flags,
		      QDF_MAC_ADDR_REF(peer->mac_addr.raw), peer,
		      QDF_MAC_ADDR_REF(mac_addr));

	/* fw supports only 2 times the max_peers ast entries */
	if (soc->num_ast_entries >=
	    wlan_cfg_get_max_ast_idx(soc->wlan_cfg_ctx)) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		dp_peer_err("%pK: Max ast entries reached", soc);
		return QDF_STATUS_E_RESOURCES;
	}

	/* If AST entry already exists , just return from here
	 * ast entry with same mac address can exist on different radios
	 * if ast_override support is enabled use search by pdev in this
	 * case
	 */
	if (soc->ast_override_support) {
		ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, mac_addr,
							    pdev->pdev_id);
		if (ast_entry) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			return QDF_STATUS_E_ALREADY;
		}

		if (is_peer_found) {
			/* During WDS to static roaming, peer is added
			 * to the list before static AST entry create.
			 * So, allow AST entry for STATIC type
			 * even if peer is present
			 */
			if (type != CDP_TXRX_AST_TYPE_STATIC) {
				qdf_spin_unlock_bh(&soc->ast_lock);
				return QDF_STATUS_E_ALREADY;
			}
		}
	} else {
		/* For HWMWDS_SEC entries can be added for same mac address
		 * do not check for existing entry
		 */
		if (type == CDP_TXRX_AST_TYPE_WDS_HM_SEC)
			goto add_ast_entry;

		ast_entry = dp_peer_ast_hash_find_soc(soc, mac_addr);

		if (ast_entry) {
			if ((ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM) &&
			    !ast_entry->delete_in_progress) {
				qdf_spin_unlock_bh(&soc->ast_lock);
				return QDF_STATUS_E_ALREADY;
			}

			/* Add for HMWDS entry we cannot be ignored if there
			 * is AST entry with same mac address
			 *
			 * if ast entry exists with the requested mac address
			 * send a delete command and register callback which
			 * can take care of adding HMWDS ast entry on delete
			 * confirmation from target
			 */
			if (type == CDP_TXRX_AST_TYPE_WDS_HM) {
				struct dp_ast_free_cb_params *param = NULL;

				if (ast_entry->type ==
					CDP_TXRX_AST_TYPE_WDS_HM_SEC)
					goto add_ast_entry;

				/* save existing callback */
				if (ast_entry->callback) {
					cb = ast_entry->callback;
					cookie = ast_entry->cookie;
				}

				param = qdf_mem_malloc(sizeof(*param));
				if (!param) {
					QDF_TRACE(QDF_MODULE_ID_TXRX,
						  QDF_TRACE_LEVEL_ERROR,
						  "Allocation failed");
					qdf_spin_unlock_bh(&soc->ast_lock);
					return QDF_STATUS_E_NOMEM;
				}

				qdf_mem_copy(&param->mac_addr.raw[0], mac_addr,
					     QDF_MAC_ADDR_SIZE);
				qdf_mem_copy(&param->peer_mac_addr.raw[0],
					     &peer->mac_addr.raw[0],
					     QDF_MAC_ADDR_SIZE);
				param->type = type;
				param->flags = flags;
				param->vdev_id = vdev->vdev_id;
				ast_entry->callback = dp_peer_free_hmwds_cb;
				ast_entry->pdev_id = vdev->pdev->pdev_id;
				ast_entry->type = type;
				ast_entry->cookie = (void *)param;
				if (!ast_entry->delete_in_progress)
					dp_peer_del_ast(soc, ast_entry);

				qdf_spin_unlock_bh(&soc->ast_lock);

				/* Call the saved callback*/
				if (cb) {
					cb(soc->ctrl_psoc,
					   dp_soc_to_cdp_soc(soc),
					   cookie,
					   CDP_TXRX_AST_DELETE_IN_PROGRESS);
				}
				return QDF_STATUS_E_AGAIN;
			}

			qdf_spin_unlock_bh(&soc->ast_lock);
			return QDF_STATUS_E_ALREADY;
		}
	}

add_ast_entry:
	ast_entry = (struct dp_ast_entry *)
			qdf_mem_malloc(sizeof(struct dp_ast_entry));

	if (!ast_entry) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		dp_peer_err("%pK: fail to allocate ast_entry", soc);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_copy(&ast_entry->mac_addr.raw[0], mac_addr, QDF_MAC_ADDR_SIZE);
	ast_entry->pdev_id = vdev->pdev->pdev_id;
	ast_entry->is_mapped = false;
	ast_entry->delete_in_progress = false;
	ast_entry->peer_id = HTT_INVALID_PEER;
	ast_entry->next_hop = 0;
	ast_entry->vdev_id = vdev->vdev_id;

	switch (type) {
	case CDP_TXRX_AST_TYPE_STATIC:
		peer->self_ast_entry = ast_entry;
		ast_entry->type = CDP_TXRX_AST_TYPE_STATIC;
		if (peer->vdev->opmode == wlan_op_mode_sta)
			ast_entry->type = CDP_TXRX_AST_TYPE_STA_BSS;
		break;
	case CDP_TXRX_AST_TYPE_SELF:
		peer->self_ast_entry = ast_entry;
		ast_entry->type = CDP_TXRX_AST_TYPE_SELF;
		break;
	case CDP_TXRX_AST_TYPE_WDS:
		ast_entry->next_hop = 1;
		ast_entry->type = CDP_TXRX_AST_TYPE_WDS;
		break;
	case CDP_TXRX_AST_TYPE_WDS_HM:
		ast_entry->next_hop = 1;
		ast_entry->type = CDP_TXRX_AST_TYPE_WDS_HM;
		break;
	case CDP_TXRX_AST_TYPE_WDS_HM_SEC:
		ast_entry->next_hop = 1;
		ast_entry->type = CDP_TXRX_AST_TYPE_WDS_HM_SEC;
		ast_entry->peer_id = peer->peer_id;
		TAILQ_INSERT_TAIL(&peer->ast_entry_list, ast_entry,
				  ase_list_elem);
		break;
	case CDP_TXRX_AST_TYPE_DA:
		vap_bss_peer = dp_vdev_bss_peer_ref_n_get(soc, vdev,
							  DP_MOD_ID_AST);
		if (!vap_bss_peer) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			qdf_mem_free(ast_entry);
			return QDF_STATUS_E_FAILURE;
		}
		peer = vap_bss_peer;
		ast_entry->next_hop = 1;
		ast_entry->type = CDP_TXRX_AST_TYPE_DA;
		break;
	default:
		dp_peer_err("%pK: Incorrect AST entry type", soc);
	}

	ast_entry->is_active = TRUE;
	DP_STATS_INC(soc, ast.added, 1);
	soc->num_ast_entries++;
	dp_peer_ast_hash_add(soc, ast_entry);

	if ((ast_entry->type != CDP_TXRX_AST_TYPE_STATIC) &&
	    (ast_entry->type != CDP_TXRX_AST_TYPE_SELF) &&
	    (ast_entry->type != CDP_TXRX_AST_TYPE_STA_BSS) &&
	    (ast_entry->type != CDP_TXRX_AST_TYPE_WDS_HM_SEC))
		status = dp_add_wds_entry_wrapper(soc,
						  peer,
						  mac_addr,
						  flags,
						  ast_entry->type);

	if (vap_bss_peer)
		dp_peer_unref_delete(vap_bss_peer, DP_MOD_ID_AST);

	qdf_spin_unlock_bh(&soc->ast_lock);
	return qdf_status_from_os_return(status);
}

qdf_export_symbol(dp_peer_add_ast);

void dp_peer_free_ast_entry(struct dp_soc *soc,
			    struct dp_ast_entry *ast_entry)
{
	/*
	 * NOTE: Ensure that call to this API is done
	 * after soc->ast_lock is taken
	 */
	dp_peer_debug("type: %d ID: %u vid: %u mac_addr: " QDF_MAC_ADDR_FMT,
		      ast_entry->type, ast_entry->peer_id, ast_entry->vdev_id,
		      QDF_MAC_ADDR_REF(ast_entry->mac_addr.raw));

	ast_entry->callback = NULL;
	ast_entry->cookie = NULL;

	DP_STATS_INC(soc, ast.deleted, 1);
	dp_peer_ast_hash_remove(soc, ast_entry);
	dp_peer_ast_cleanup(soc, ast_entry);
	qdf_mem_free(ast_entry);
	soc->num_ast_entries--;
}

void dp_peer_unlink_ast_entry(struct dp_soc *soc,
			      struct dp_ast_entry *ast_entry,
			      struct dp_peer *peer)
{
	if (!peer) {
		dp_info_rl("NULL peer");
		return;
	}

	if (ast_entry->peer_id == HTT_INVALID_PEER) {
		dp_info_rl("Invalid peer id in AST entry mac addr:"QDF_MAC_ADDR_FMT" type:%d",
			  QDF_MAC_ADDR_REF(ast_entry->mac_addr.raw),
			  ast_entry->type);
		return;
	}
	/*
	 * NOTE: Ensure that call to this API is done
	 * after soc->ast_lock is taken
	 */

	qdf_assert_always(ast_entry->peer_id == peer->peer_id);
	TAILQ_REMOVE(&peer->ast_entry_list, ast_entry, ase_list_elem);

	if (ast_entry == peer->self_ast_entry)
		peer->self_ast_entry = NULL;

	/*
	 * release the reference only if it is mapped
	 * to ast_table
	 */
	if (ast_entry->is_mapped)
		soc->ast_table[ast_entry->ast_idx] = NULL;

	ast_entry->peer_id = HTT_INVALID_PEER;
}

void dp_peer_del_ast(struct dp_soc *soc, struct dp_ast_entry *ast_entry)
{
	struct dp_peer *peer = NULL;

	if (soc->ast_offload_support)
		return;

	if (!ast_entry) {
		dp_info_rl("NULL AST entry");
		return;
	}

	if (ast_entry->delete_in_progress) {
		dp_info_rl("AST entry deletion in progress mac addr:"QDF_MAC_ADDR_FMT" type:%d",
			  QDF_MAC_ADDR_REF(ast_entry->mac_addr.raw),
			  ast_entry->type);
		return;
	}

	dp_peer_debug("call by %ps: ID: %u vid: %u mac_addr: " QDF_MAC_ADDR_FMT,
		      (void *)_RET_IP_, ast_entry->peer_id, ast_entry->vdev_id,
		      QDF_MAC_ADDR_REF(ast_entry->mac_addr.raw));

	ast_entry->delete_in_progress = true;

	/* In teardown del ast is called after setting logical delete state
	 * use __dp_peer_get_ref_by_id to get the reference irrespective of
	 * state
	 */
	peer = __dp_peer_get_ref_by_id(soc, ast_entry->peer_id,
				       DP_MOD_ID_AST);

	dp_peer_ast_send_wds_del(soc, ast_entry, peer);

	/* Remove SELF and STATIC entries in teardown itself */
	if (!ast_entry->next_hop)
		dp_peer_unlink_ast_entry(soc, ast_entry, peer);

	if (ast_entry->is_mapped)
		soc->ast_table[ast_entry->ast_idx] = NULL;

	/* if peer map v2 is enabled we are not freeing ast entry
	 * here and it is supposed to be freed in unmap event (after
	 * we receive delete confirmation from target)
	 *
	 * if peer_id is invalid we did not get the peer map event
	 * for the peer free ast entry from here only in this case
	 */
	if (dp_peer_ast_free_in_unmap_supported(soc, ast_entry))
		goto end;

	/* for WDS secondary entry ast_entry->next_hop would be set so
	 * unlinking has to be done explicitly here.
	 * As this entry is not a mapped entry unmap notification from
	 * FW will not come. Hence unlinkling is done right here.
	 */

	if (ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM_SEC)
		dp_peer_unlink_ast_entry(soc, ast_entry, peer);

	dp_peer_free_ast_entry(soc, ast_entry);

end:
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_AST);
}

int dp_peer_update_ast(struct dp_soc *soc, struct dp_peer *peer,
		       struct dp_ast_entry *ast_entry, uint32_t flags)
{
	int ret = -1;
	struct dp_peer *old_peer;

	if (soc->ast_offload_support)
		return QDF_STATUS_E_INVAL;

	dp_peer_debug("%pK: ast_entry->type: %d pdevid: %u vdevid: %u flags: 0x%x mac_addr: " QDF_MAC_ADDR_FMT " peer_mac: " QDF_MAC_ADDR_FMT "\n",
		      soc, ast_entry->type, peer->vdev->pdev->pdev_id,
		      peer->vdev->vdev_id, flags,
		      QDF_MAC_ADDR_REF(ast_entry->mac_addr.raw),
		      QDF_MAC_ADDR_REF(peer->mac_addr.raw));

	/* Do not send AST update in below cases
	 *  1) Ast entry delete has already triggered
	 *  2) Peer delete is already triggered
	 *  3) We did not get the HTT map for create event
	 */
	if (ast_entry->delete_in_progress ||
	    !dp_peer_state_cmp(peer, DP_PEER_STATE_ACTIVE) ||
	    !ast_entry->is_mapped)
		return ret;

	if ((ast_entry->type == CDP_TXRX_AST_TYPE_STATIC) ||
	    (ast_entry->type == CDP_TXRX_AST_TYPE_SELF) ||
	    (ast_entry->type == CDP_TXRX_AST_TYPE_STA_BSS) ||
	    (ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM_SEC))
		return 0;

	/*
	 * Avoids flood of WMI update messages sent to FW for same peer.
	 */
	if (qdf_unlikely(ast_entry->peer_id == peer->peer_id) &&
	    (ast_entry->type == CDP_TXRX_AST_TYPE_WDS) &&
	    (ast_entry->vdev_id == peer->vdev->vdev_id) &&
	    (ast_entry->is_active))
		return 0;

	old_peer = dp_peer_get_ref_by_id(soc, ast_entry->peer_id,
					 DP_MOD_ID_AST);
	if (!old_peer)
		return 0;

	TAILQ_REMOVE(&old_peer->ast_entry_list, ast_entry, ase_list_elem);

	dp_peer_unref_delete(old_peer, DP_MOD_ID_AST);

	ast_entry->peer_id = peer->peer_id;
	ast_entry->type = CDP_TXRX_AST_TYPE_WDS;
	ast_entry->pdev_id = peer->vdev->pdev->pdev_id;
	ast_entry->vdev_id = peer->vdev->vdev_id;
	ast_entry->is_active = TRUE;
	TAILQ_INSERT_TAIL(&peer->ast_entry_list, ast_entry, ase_list_elem);

	ret = dp_update_wds_entry_wrapper(soc,
					  peer,
					  ast_entry->mac_addr.raw,
					  flags);

	return ret;
}

uint8_t dp_peer_ast_get_pdev_id(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry)
{
	return ast_entry->pdev_id;
}

uint8_t dp_peer_ast_get_next_hop(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry)
{
	return ast_entry->next_hop;
}

void dp_peer_ast_set_type(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry,
				enum cdp_txrx_ast_entry_type type)
{
	ast_entry->type = type;
}

void dp_peer_ast_send_wds_del(struct dp_soc *soc,
			      struct dp_ast_entry *ast_entry,
			      struct dp_peer *peer)
{
	bool delete_in_fw = false;

	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_TRACE,
		  "%s: ast_entry->type: %d pdevid: %u vdev: %u mac_addr: "QDF_MAC_ADDR_FMT" next_hop: %u peer_id: %uM\n",
		  __func__, ast_entry->type, ast_entry->pdev_id,
		  ast_entry->vdev_id,
		  QDF_MAC_ADDR_REF(ast_entry->mac_addr.raw),
		  ast_entry->next_hop, ast_entry->peer_id);

	/*
	 * If peer state is logical delete, the peer is about to get
	 * teared down with a peer delete command to firmware,
	 * which will cleanup all the wds ast entries.
	 * So, no need to send explicit wds ast delete to firmware.
	 */
	if (ast_entry->next_hop) {
		if (peer && dp_peer_state_cmp(peer,
					      DP_PEER_STATE_LOGICAL_DELETE))
			delete_in_fw = false;
		else
			delete_in_fw = true;

		dp_del_wds_entry_wrapper(soc,
					 ast_entry->vdev_id,
					 ast_entry->mac_addr.raw,
					 ast_entry->type,
					 delete_in_fw);
	}
}
#else
void dp_peer_free_ast_entry(struct dp_soc *soc,
			    struct dp_ast_entry *ast_entry)
{
}

void dp_peer_unlink_ast_entry(struct dp_soc *soc,
			      struct dp_ast_entry *ast_entry,
			      struct dp_peer *peer)
{
}

void dp_peer_ast_hash_remove(struct dp_soc *soc,
			     struct dp_ast_entry *ase)
{
}

struct dp_ast_entry *dp_peer_ast_hash_find_by_vdevid(struct dp_soc *soc,
						     uint8_t *ast_mac_addr,
						     uint8_t vdev_id)
{
	return NULL;
}

QDF_STATUS dp_peer_add_ast(struct dp_soc *soc,
			   struct dp_peer *peer,
			   uint8_t *mac_addr,
			   enum cdp_txrx_ast_entry_type type,
			   uint32_t flags)
{
	return QDF_STATUS_E_FAILURE;
}

void dp_peer_del_ast(struct dp_soc *soc, struct dp_ast_entry *ast_entry)
{
}

int dp_peer_update_ast(struct dp_soc *soc, struct dp_peer *peer,
			struct dp_ast_entry *ast_entry, uint32_t flags)
{
	return 1;
}

struct dp_ast_entry *dp_peer_ast_hash_find_soc(struct dp_soc *soc,
					       uint8_t *ast_mac_addr)
{
	return NULL;
}

struct dp_ast_entry *dp_peer_ast_hash_find_soc_by_type(
					struct dp_soc *soc,
					uint8_t *ast_mac_addr,
					enum cdp_txrx_ast_entry_type type)
{
	return NULL;
}

static inline
QDF_STATUS dp_peer_host_add_map_ast(struct dp_soc *soc, uint16_t peer_id,
				    uint8_t *mac_addr, uint16_t hw_peer_id,
				    uint8_t vdev_id, uint16_t ast_hash,
				    uint8_t is_wds)
{
	return QDF_STATUS_SUCCESS;
}

struct dp_ast_entry *dp_peer_ast_hash_find_by_pdevid(struct dp_soc *soc,
						     uint8_t *ast_mac_addr,
						     uint8_t pdev_id)
{
	return NULL;
}

QDF_STATUS dp_peer_ast_hash_attach(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_peer_map_ast(struct dp_soc *soc,
					 struct dp_peer *peer,
					 uint8_t *mac_addr,
					 uint16_t hw_peer_id,
					 uint8_t vdev_id,
					 uint16_t ast_hash,
					 uint8_t is_wds)
{
	return QDF_STATUS_SUCCESS;
}

void dp_peer_ast_hash_detach(struct dp_soc *soc)
{
}

void dp_peer_ast_set_type(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry,
				enum cdp_txrx_ast_entry_type type)
{
}

uint8_t dp_peer_ast_get_pdev_id(struct dp_soc *soc,
				struct dp_ast_entry *ast_entry)
{
	return 0xff;
}

uint8_t dp_peer_ast_get_next_hop(struct dp_soc *soc,
				 struct dp_ast_entry *ast_entry)
{
	return 0xff;
}

void dp_peer_ast_send_wds_del(struct dp_soc *soc,
			      struct dp_ast_entry *ast_entry,
			      struct dp_peer *peer)
{
}

static inline
void dp_peer_unmap_ipa_evt(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t vdev_id, uint8_t *mac_addr)
{
}
#endif

#ifdef WLAN_FEATURE_MULTI_AST_DEL
void dp_peer_ast_send_multi_wds_del(
		struct dp_soc *soc, uint8_t vdev_id,
		struct peer_del_multi_wds_entries *wds_list)
{
	struct cdp_soc_t *cdp_soc = &soc->cdp_soc;

	if (cdp_soc && cdp_soc->ol_ops &&
	    cdp_soc->ol_ops->peer_del_multi_wds_entry)
		cdp_soc->ol_ops->peer_del_multi_wds_entry(soc->ctrl_psoc,
							  vdev_id, wds_list);
}
#endif

#ifdef FEATURE_WDS
/**
 * dp_peer_ast_free_wds_entries() - Free wds ast entries associated with peer
 * @soc: soc handle
 * @peer: peer handle
 *
 * Free all the wds ast entries associated with peer
 *
 * Return: Number of wds ast entries freed
 */
static uint32_t dp_peer_ast_free_wds_entries(struct dp_soc *soc,
					     struct dp_peer *peer)
{
	TAILQ_HEAD(, dp_ast_entry) ast_local_list = {0};
	struct dp_ast_entry *ast_entry, *temp_ast_entry;
	uint32_t num_ast = 0;

	TAILQ_INIT(&ast_local_list);
	qdf_spin_lock_bh(&soc->ast_lock);

	DP_PEER_ITERATE_ASE_LIST(peer, ast_entry, temp_ast_entry) {
		if (ast_entry->next_hop)
			num_ast++;

		if (ast_entry->is_mapped)
			soc->ast_table[ast_entry->ast_idx] = NULL;

		dp_peer_unlink_ast_entry(soc, ast_entry, peer);
		DP_STATS_INC(soc, ast.deleted, 1);
		dp_peer_ast_hash_remove(soc, ast_entry);
		TAILQ_INSERT_TAIL(&ast_local_list, ast_entry,
				  ase_list_elem);
		soc->num_ast_entries--;
	}

	qdf_spin_unlock_bh(&soc->ast_lock);

	TAILQ_FOREACH_SAFE(ast_entry, &ast_local_list, ase_list_elem,
			   temp_ast_entry) {
		if (ast_entry->callback)
			ast_entry->callback(soc->ctrl_psoc,
					    dp_soc_to_cdp_soc(soc),
					    ast_entry->cookie,
					    CDP_TXRX_AST_DELETED);

		qdf_mem_free(ast_entry);
	}

	return num_ast;
}
/**
 * dp_peer_clean_wds_entries() - Clean wds ast entries and compare
 * @soc: soc handle
 * @peer: peer handle
 * @free_wds_count: number of wds entries freed by FW with peer delete
 *
 * Free all the wds ast entries associated with peer and compare with
 * the value received from firmware
 *
 * Return: Number of wds ast entries freed
 */
static void
dp_peer_clean_wds_entries(struct dp_soc *soc, struct dp_peer *peer,
			  uint32_t free_wds_count)
{
	uint32_t wds_deleted = 0;
	bool ast_ind_disable;

	if (soc->ast_offload_support && !soc->host_ast_db_enable)
		return;

	ast_ind_disable = wlan_cfg_get_ast_indication_disable
		(soc->wlan_cfg_ctx);

	wds_deleted = dp_peer_ast_free_wds_entries(soc, peer);
	if ((DP_PEER_WDS_COUNT_INVALID != free_wds_count) &&
	    (free_wds_count != wds_deleted) && !ast_ind_disable) {
		DP_STATS_INC(soc, ast.ast_mismatch, 1);
		dp_alert("For peer %pK (mac: "QDF_MAC_ADDR_FMT")number of wds entries deleted by fw = %d during peer delete is not same as the numbers deleted by host = %d",
			 peer, peer->mac_addr.raw, free_wds_count,
			 wds_deleted);
	}
}

#else
static void
dp_peer_clean_wds_entries(struct dp_soc *soc, struct dp_peer *peer,
			  uint32_t free_wds_count)
{
	struct dp_ast_entry *ast_entry, *temp_ast_entry;

	qdf_spin_lock_bh(&soc->ast_lock);

	DP_PEER_ITERATE_ASE_LIST(peer, ast_entry, temp_ast_entry) {
		dp_peer_unlink_ast_entry(soc, ast_entry, peer);

		if (ast_entry->is_mapped)
			soc->ast_table[ast_entry->ast_idx] = NULL;

		dp_peer_free_ast_entry(soc, ast_entry);
	}

	peer->self_ast_entry = NULL;
	qdf_spin_unlock_bh(&soc->ast_lock);
}
#endif

/**
 * dp_peer_ast_free_entry_by_mac() - find ast entry by MAC address and delete
 * @soc: soc handle
 * @peer: peer handle
 * @vdev_id: vdev_id
 * @mac_addr: mac address of the AST entry to searc and delete
 *
 * find the ast entry from the peer list using the mac address and free
 * the entry.
 *
 * Return: SUCCESS or NOENT
 */
static int dp_peer_ast_free_entry_by_mac(struct dp_soc *soc,
					 struct dp_peer *peer,
					 uint8_t vdev_id,
					 uint8_t *mac_addr)
{
	struct dp_ast_entry *ast_entry;
	void *cookie = NULL;
	txrx_ast_free_cb cb = NULL;

	/*
	 * release the reference only if it is mapped
	 * to ast_table
	 */

	qdf_spin_lock_bh(&soc->ast_lock);

	ast_entry = dp_peer_ast_hash_find_by_vdevid(soc, mac_addr, vdev_id);
	if (!ast_entry) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return QDF_STATUS_E_NOENT;
	} else if (ast_entry->is_mapped) {
		soc->ast_table[ast_entry->ast_idx] = NULL;
	}

	cb = ast_entry->callback;
	cookie = ast_entry->cookie;


	dp_peer_unlink_ast_entry(soc, ast_entry, peer);

	dp_peer_free_ast_entry(soc, ast_entry);

	qdf_spin_unlock_bh(&soc->ast_lock);

	if (cb) {
		cb(soc->ctrl_psoc,
		   dp_soc_to_cdp_soc(soc),
		   cookie,
		   CDP_TXRX_AST_DELETED);
	}

	return QDF_STATUS_SUCCESS;
}

void dp_peer_find_hash_erase(struct dp_soc *soc)
{
	int i;

	/*
	 * Not really necessary to take peer_ref_mutex lock - by this point,
	 * it's known that the soc is no longer in use.
	 */
	for (i = 0; i <= soc->peer_hash.mask; i++) {
		if (!TAILQ_EMPTY(&soc->peer_hash.bins[i])) {
			struct dp_peer *peer, *peer_next;

			/*
			 * TAILQ_FOREACH_SAFE must be used here to avoid any
			 * memory access violation after peer is freed
			 */
			TAILQ_FOREACH_SAFE(peer, &soc->peer_hash.bins[i],
				hash_list_elem, peer_next) {
				/*
				 * Don't remove the peer from the hash table -
				 * that would modify the list we are currently
				 * traversing, and it's not necessary anyway.
				 */
				/*
				 * Artificially adjust the peer's ref count to
				 * 1, so it will get deleted by
				 * dp_peer_unref_delete.
				 */
				/* set to zero */
				qdf_atomic_init(&peer->ref_cnt);
				for (i = 0; i < DP_MOD_ID_MAX; i++)
					qdf_atomic_init(&peer->mod_refs[i]);
				/* incr to one */
				qdf_atomic_inc(&peer->ref_cnt);
				qdf_atomic_inc(&peer->mod_refs
						[DP_MOD_ID_CONFIG]);
				dp_peer_unref_delete(peer,
						     DP_MOD_ID_CONFIG);
			}
		}
	}
}

void dp_peer_ast_table_detach(struct dp_soc *soc)
{
	if (soc->ast_table) {
		qdf_mem_free(soc->ast_table);
		soc->ast_table = NULL;
	}
}

void dp_peer_find_map_detach(struct dp_soc *soc)
{
	struct dp_peer *peer = NULL;
	uint32_t i = 0;

	if (soc->peer_id_to_obj_map) {
		for (i = 0; i < soc->max_peer_id; i++) {
			peer = soc->peer_id_to_obj_map[i];
			if (peer)
				dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
		}
		qdf_mem_free(soc->peer_id_to_obj_map);
		soc->peer_id_to_obj_map = NULL;
		qdf_spinlock_destroy(&soc->peer_map_lock);
	}
}

#ifndef AST_OFFLOAD_ENABLE
QDF_STATUS dp_peer_find_attach(struct dp_soc *soc)
{
	QDF_STATUS status;

	status = dp_peer_find_map_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = dp_peer_find_hash_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto map_detach;

	status = dp_peer_ast_table_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto hash_detach;

	status = dp_peer_ast_hash_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto ast_table_detach;

	status = dp_peer_mec_hash_attach(soc);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		dp_soc_wds_attach(soc);
		return status;
	}

	dp_peer_ast_hash_detach(soc);
ast_table_detach:
	dp_peer_ast_table_detach(soc);
hash_detach:
	dp_peer_find_hash_detach(soc);
map_detach:
	dp_peer_find_map_detach(soc);

	return status;
}
#else
QDF_STATUS dp_peer_find_attach(struct dp_soc *soc)
{
	QDF_STATUS status;

	status = dp_peer_find_map_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = dp_peer_find_hash_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto map_detach;

	return status;
map_detach:
	dp_peer_find_map_detach(soc);

	return status;
}
#endif

#ifdef REO_SHARED_QREF_TABLE_EN
void dp_peer_rx_reo_shared_qaddr_delete(struct dp_soc *soc,
					struct dp_peer *peer)
{
	uint8_t tid;
	uint16_t peer_id;
	uint32_t max_list_size;

	max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	peer_id = peer->peer_id;

	if (peer_id > soc->max_peer_id)
		return;
	if (IS_MLO_DP_LINK_PEER(peer))
		return;

	if (max_list_size) {
		unsigned long curr_ts = qdf_get_system_timestamp();
		struct dp_peer *primary_peer = peer;
		uint16_t chip_id = 0xFFFF;
		uint32_t qref_index;

		qref_index = soc->shared_qaddr_del_idx;

		soc->list_shared_qaddr_del[qref_index].peer_id =
							  primary_peer->peer_id;
		soc->list_shared_qaddr_del[qref_index].ts_qaddr_del = curr_ts;
		soc->list_shared_qaddr_del[qref_index].chip_id = chip_id;
		soc->shared_qaddr_del_idx++;

		if (soc->shared_qaddr_del_idx == max_list_size)
			soc->shared_qaddr_del_idx = 0;
	}

	if (hal_reo_shared_qaddr_is_enable(soc->hal_soc)) {
		for (tid = 0; tid < DP_MAX_TIDS; tid++) {
			hal_reo_shared_qaddr_write(soc->hal_soc,
						   peer_id, tid, 0);
		}
	}
}
#endif

/**
 * dp_peer_find_add_id() - map peer_id with peer
 * @soc: soc handle
 * @peer_mac_addr: peer mac address
 * @peer_id: peer id to be mapped
 * @hw_peer_id: HW ast index
 * @vdev_id: vdev_id
 * @peer_type: peer type (link or MLD)
 *
 * return: peer in success
 *         NULL in failure
 */
static inline struct dp_peer *dp_peer_find_add_id(struct dp_soc *soc,
	uint8_t *peer_mac_addr, uint16_t peer_id, uint16_t hw_peer_id,
	uint8_t vdev_id, enum cdp_peer_type peer_type)
{
	struct dp_peer *peer;
	struct cdp_peer_info peer_info = { 0 };

	QDF_ASSERT(peer_id <= soc->max_peer_id);
	/* check if there's already a peer object with this MAC address */
	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac_addr,
				 false, peer_type);
	peer = dp_peer_hash_find_wrapper(soc, &peer_info, DP_MOD_ID_CONFIG);
	dp_peer_debug("%pK: peer %pK ID %d vid %d mac " QDF_MAC_ADDR_FMT,
		      soc, peer, peer_id, vdev_id,
		      QDF_MAC_ADDR_REF(peer_mac_addr));

	if (peer) {
		/* peer's ref count was already incremented by
		 * peer_find_hash_find
		 */
		dp_peer_info("%pK: ref_cnt: %d", soc,
			     qdf_atomic_read(&peer->ref_cnt));

		/*
		 * if peer is in logical delete CP triggered delete before map
		 * is received ignore this event
		 */
		if (dp_peer_state_cmp(peer, DP_PEER_STATE_LOGICAL_DELETE)) {
			dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
			dp_alert("Peer %pK["QDF_MAC_ADDR_FMT"] logical delete state vid %d",
				 peer, QDF_MAC_ADDR_REF(peer_mac_addr),
				 vdev_id);
			return NULL;
		}

		if (peer->peer_id == HTT_INVALID_PEER) {
			if (!IS_MLO_DP_MLD_PEER(peer))
				dp_monitor_peer_tid_peer_id_update(soc, peer,
								   peer_id);
		} else {
			dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
			QDF_ASSERT(0);
			return NULL;
		}
		dp_peer_find_id_to_obj_add(soc, peer, peer_id);
		if (soc->arch_ops.dp_partner_chips_map)
			soc->arch_ops.dp_partner_chips_map(soc, peer, peer_id);

		dp_peer_update_state(soc, peer, DP_PEER_STATE_ACTIVE);
		return peer;
	}

	return NULL;
}

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef DP_USE_REDUCED_PEER_ID_FIELD_WIDTH
uint16_t dp_gen_ml_peer_id(struct dp_soc *soc, uint16_t peer_id)
{
	return ((peer_id & soc->peer_id_mask) | (1 << soc->peer_id_shift));
}
#else
uint16_t dp_gen_ml_peer_id(struct dp_soc *soc, uint16_t peer_id)
{
	return (peer_id | (1 << HTT_RX_PEER_META_DATA_V1_ML_PEER_VALID_S));
}
#endif

QDF_STATUS
dp_rx_mlo_peer_map_handler(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t *peer_mac_addr,
			   struct dp_mlo_flow_override_info *mlo_flow_info,
			   struct dp_mlo_link_info *mlo_link_info)
{
	struct dp_peer *peer = NULL;
	uint16_t hw_peer_id = mlo_flow_info[0].ast_idx;
	uint16_t ast_hash = mlo_flow_info[0].cache_set_num;
	uint8_t vdev_id = 0;
	uint8_t is_wds = 0;
	int i;
	uint16_t ml_peer_id = dp_gen_ml_peer_id(soc, peer_id);
	enum cdp_txrx_ast_entry_type type = CDP_TXRX_AST_TYPE_STATIC;
	QDF_STATUS err = QDF_STATUS_SUCCESS;
	struct dp_soc *primary_soc = NULL;

	dp_cfg_event_record_peer_map_unmap_evt(soc, DP_CFG_EVENT_MLO_PEER_MAP,
					       NULL, peer_mac_addr,
					       1, peer_id, ml_peer_id, 0,
					       vdev_id);

	dp_info("mlo_peer_map_event (soc:%pK): peer_id %d ml_peer_id %d, peer_mac "QDF_MAC_ADDR_FMT,
		soc, peer_id, ml_peer_id,
		QDF_MAC_ADDR_REF(peer_mac_addr));

	DP_STATS_INC(soc, t2h_msg_stats.ml_peer_map, 1);
	/* Get corresponding vdev ID for the peer based
	 * on chip ID obtained from mlo peer_map event
	 */
	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		if (mlo_link_info[i].peer_chip_id == dp_get_chip_id(soc)) {
			vdev_id = mlo_link_info[i].vdev_id;
			break;
		}
	}

	peer = dp_peer_find_add_id(soc, peer_mac_addr, ml_peer_id,
				   hw_peer_id, vdev_id, CDP_MLD_PEER_TYPE);
	if (peer) {
		if (wlan_op_mode_sta == peer->vdev->opmode &&
		    qdf_mem_cmp(peer->mac_addr.raw,
				peer->vdev->mld_mac_addr.raw,
				QDF_MAC_ADDR_SIZE) != 0) {
			dp_peer_info("%pK: STA vdev bss_peer!!!!", soc);
			peer->bss_peer = 1;
			if (peer->txrx_peer)
				peer->txrx_peer->bss_peer = 1;
		}

		if (peer->vdev->opmode == wlan_op_mode_sta) {
			peer->vdev->bss_ast_hash = ast_hash;
			peer->vdev->bss_ast_idx = hw_peer_id;
		}

		/* Add ast entry incase self ast entry is
		 * deleted due to DP CP sync issue
		 *
		 * self_ast_entry is modified in peer create
		 * and peer unmap path which cannot run in
		 * parllel with peer map, no lock need before
		 * referring it
		 */
		if (!peer->self_ast_entry) {
			dp_info("Add self ast from map "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(peer_mac_addr));
			dp_peer_add_ast(soc, peer,
					peer_mac_addr,
					type, 0);
		}
		/* If peer setup and hence rx_tid setup got called
		 * before htt peer map then Qref write to LUT did not
		 * happen in rx_tid setup as peer_id was invalid.
		 * So defer Qref write to peer map handler. Check if
		 * rx_tid qdesc for tid 0 is already setup and perform
		 * qref write to LUT for Tid 0 and 16.
		 *
		 * Peer map could be obtained on assoc link, hence
		 * change to primary link's soc.
		 */
		primary_soc = peer->vdev->pdev->soc;
		if (hal_reo_shared_qaddr_is_enable(primary_soc->hal_soc) &&
		    peer->rx_tid[0].hw_qdesc_vaddr_unaligned) {
			hal_reo_shared_qaddr_write(primary_soc->hal_soc,
						   ml_peer_id,
						   0,
						   peer->rx_tid[0].hw_qdesc_paddr);
			hal_reo_shared_qaddr_write(primary_soc->hal_soc,
						   ml_peer_id,
						   DP_NON_QOS_TID,
						   peer->rx_tid[DP_NON_QOS_TID].hw_qdesc_paddr);
		}
	}

	if (!primary_soc)
		primary_soc = soc;

	err = dp_peer_map_ast(soc, peer, peer_mac_addr, hw_peer_id,
			      vdev_id, ast_hash, is_wds);

	/*
	 * If AST offload and host AST DB is enabled, populate AST entries on
	 * host based on mlo peer map event from FW
	 */
	if (peer && soc->ast_offload_support && soc->host_ast_db_enable) {
		dp_peer_host_add_map_ast(primary_soc, ml_peer_id, peer_mac_addr,
					 hw_peer_id, vdev_id,
					 ast_hash, is_wds);
	}

	return err;
}
#endif

#ifdef DP_RX_UDP_OVER_PEER_ROAM
void dp_rx_reset_roaming_peer(struct dp_soc *soc, uint8_t vdev_id,
			      uint8_t *peer_mac_addr)
{
	struct dp_vdev *vdev = NULL;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_HTT);
	if (vdev) {
		if (qdf_mem_cmp(vdev->roaming_peer_mac.raw, peer_mac_addr,
				QDF_MAC_ADDR_SIZE) == 0) {
			vdev->roaming_peer_status =
						WLAN_ROAM_PEER_AUTH_STATUS_NONE;
			qdf_mem_zero(vdev->roaming_peer_mac.raw,
				     QDF_MAC_ADDR_SIZE);
		}
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_HTT);
	}
}
#endif

#ifdef WLAN_SUPPORT_PPEDS
static void
dp_tx_ppeds_cfg_astidx_cache_mapping(struct dp_soc *soc, struct dp_vdev *vdev,
				     bool peer_map)
{
	if (soc->arch_ops.dp_tx_ppeds_cfg_astidx_cache_mapping)
		soc->arch_ops.dp_tx_ppeds_cfg_astidx_cache_mapping(soc, vdev,
								   peer_map);
}
#else
static void
dp_tx_ppeds_cfg_astidx_cache_mapping(struct dp_soc *soc, struct dp_vdev *vdev,
				     bool peer_map)
{
}
#endif

QDF_STATUS
dp_rx_peer_map_handler(struct dp_soc *soc, uint16_t peer_id,
		       uint16_t hw_peer_id, uint8_t vdev_id,
		       uint8_t *peer_mac_addr, uint16_t ast_hash,
		       uint8_t is_wds)
{
	struct dp_peer *peer = NULL;
	struct dp_vdev *vdev = NULL;
	enum cdp_txrx_ast_entry_type type = CDP_TXRX_AST_TYPE_STATIC;
	QDF_STATUS err = QDF_STATUS_SUCCESS;

	dp_cfg_event_record_peer_map_unmap_evt(soc, DP_CFG_EVENT_PEER_MAP,
					       NULL, peer_mac_addr, 1, peer_id,
					       0, 0, vdev_id);
	dp_info("peer_map_event (soc:%pK): peer_id %d, hw_peer_id %d, peer_mac "QDF_MAC_ADDR_FMT", vdev_id %d",
		soc, peer_id, hw_peer_id,
		QDF_MAC_ADDR_REF(peer_mac_addr), vdev_id);
	DP_STATS_INC(soc, t2h_msg_stats.peer_map, 1);

	/* Peer map event for WDS ast entry get the peer from
	 * obj map
	 */
	if (is_wds) {
		if (!soc->ast_offload_support) {
			peer = dp_peer_get_ref_by_id(soc, peer_id,
						     DP_MOD_ID_HTT);

			err = dp_peer_map_ast(soc, peer, peer_mac_addr,
					      hw_peer_id,
					      vdev_id, ast_hash, is_wds);
			if (peer)
				dp_peer_unref_delete(peer, DP_MOD_ID_HTT);
		}
	} else {
		/*
		 * It's the responsibility of the CP and FW to ensure
		 * that peer is created successfully. Ideally DP should
		 * not hit the below condition for directly associated
		 * peers.
		 */
		if ((!soc->ast_offload_support) && ((hw_peer_id < 0) ||
		    (hw_peer_id >=
		     wlan_cfg_get_max_ast_idx(soc->wlan_cfg_ctx)))) {
			dp_peer_err("%pK: invalid hw_peer_id: %d", soc, hw_peer_id);
			qdf_assert_always(0);
		}

		peer = dp_peer_find_add_id(soc, peer_mac_addr, peer_id,
					   hw_peer_id, vdev_id,
					   CDP_LINK_PEER_TYPE);

		if (peer) {
			bool peer_map = true;

			/* Updating ast_hash and ast_idx in peer level */
			peer->ast_hash = ast_hash;
			peer->ast_idx = hw_peer_id;
			vdev = peer->vdev;
			/* Only check for STA Vdev and peer is not for TDLS */
			if (wlan_op_mode_sta == vdev->opmode &&
			    !peer->is_tdls_peer) {
				if (qdf_mem_cmp(peer->mac_addr.raw,
						vdev->mac_addr.raw,
						QDF_MAC_ADDR_SIZE) != 0) {
					dp_info("%pK: STA vdev bss_peer", soc);
					peer->bss_peer = 1;
					if (peer->txrx_peer)
						peer->txrx_peer->bss_peer = 1;
				}

				dp_info("bss ast_hash 0x%x, ast_index 0x%x",
					ast_hash, hw_peer_id);
				vdev->bss_ast_hash = ast_hash;
				vdev->bss_ast_idx = hw_peer_id;

				dp_tx_ppeds_cfg_astidx_cache_mapping(soc, vdev,
								     peer_map);
			}

			/* Add ast entry incase self ast entry is
			 * deleted due to DP CP sync issue
			 *
			 * self_ast_entry is modified in peer create
			 * and peer unmap path which cannot run in
			 * parllel with peer map, no lock need before
			 * referring it
			 */
			if (!soc->ast_offload_support &&
				!peer->self_ast_entry) {
				dp_info("Add self ast from map "QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(peer_mac_addr));
				dp_peer_add_ast(soc, peer,
						peer_mac_addr,
						type, 0);
			}

			/* If peer setup and hence rx_tid setup got called
			 * before htt peer map then Qref write to LUT did
			 * not happen in rx_tid setup as peer_id was invalid.
			 * So defer Qref write to peer map handler. Check if
			 * rx_tid qdesc for tid 0 is already setup perform qref
			 * write to LUT for Tid 0 and 16.
			 */
			if (hal_reo_shared_qaddr_is_enable(soc->hal_soc) &&
			    peer->rx_tid[0].hw_qdesc_vaddr_unaligned &&
			    !IS_MLO_DP_LINK_PEER(peer)) {
				add_entry_write_list(soc, peer, 0);
				hal_reo_shared_qaddr_write(soc->hal_soc,
							   peer_id,
							   0,
							   peer->rx_tid[0].hw_qdesc_paddr);
				add_entry_write_list(soc, peer, DP_NON_QOS_TID);
				hal_reo_shared_qaddr_write(soc->hal_soc,
							   peer_id,
							   DP_NON_QOS_TID,
							   peer->rx_tid[DP_NON_QOS_TID].hw_qdesc_paddr);
			}
		}

		err = dp_peer_map_ast(soc, peer, peer_mac_addr, hw_peer_id,
				      vdev_id, ast_hash, is_wds);
	}

	dp_rx_reset_roaming_peer(soc, vdev_id, peer_mac_addr);

	/*
	 * If AST offload and host AST DB is enabled, populate AST entries on
	 * host based on peer map event from FW
	 */
	if (soc->ast_offload_support && soc->host_ast_db_enable) {
		dp_peer_host_add_map_ast(soc, peer_id, peer_mac_addr,
					 hw_peer_id, vdev_id,
					 ast_hash, is_wds);
	}

	return err;
}

void
dp_rx_peer_unmap_handler(struct dp_soc *soc, uint16_t peer_id,
			 uint8_t vdev_id, uint8_t *mac_addr,
			 uint8_t is_wds, uint32_t free_wds_count)
{
	struct dp_peer *peer;
	struct dp_vdev *vdev = NULL;

	DP_STATS_INC(soc, t2h_msg_stats.peer_unmap, 1);

	/*
	 * If FW AST offload is enabled and host AST DB is enabled,
	 * the AST entries are created during peer map from FW.
	 */
	if (soc->ast_offload_support && is_wds) {
		if (!soc->host_ast_db_enable)
			return;
	}

	peer = __dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_HTT);

	/*
	 * Currently peer IDs are assigned for vdevs as well as peers.
	 * If the peer ID is for a vdev, then the peer pointer stored
	 * in peer_id_to_obj_map will be NULL.
	 */
	if (!peer) {
		dp_err("Received unmap event for invalid peer_id %u",
		       peer_id);
		DP_STATS_INC(soc, t2h_msg_stats.invalid_peer_unmap, 1);
		return;
	}

	vdev = peer->vdev;

	if (peer->txrx_peer) {
		struct cdp_txrx_peer_params_update params = {0};

		params.vdev_id = vdev->vdev_id;
		params.peer_mac = peer->mac_addr.raw;
		params.chip_id = dp_get_chip_id(soc);
		params.pdev_id = vdev->pdev->pdev_id;

		dp_wdi_event_handler(WDI_EVENT_PEER_UNMAP, soc,
				     (void *)&params, peer_id,
				     WDI_NO_VAL, vdev->pdev->pdev_id);
	}

	/*
	 * In scenario where assoc peer soc id is different from
	 * primary soc id, reset the soc to point to primary psoc.
	 * Since map is received on primary soc, the unmap should
	 * also delete ast on primary soc.
	 */
	soc = peer->vdev->pdev->soc;

	/* If V2 Peer map messages are enabled AST entry has to be
	 * freed here
	 */
	if (is_wds) {
		if (!dp_peer_ast_free_entry_by_mac(soc, peer, vdev_id,
						   mac_addr)) {
			dp_peer_unmap_ipa_evt(soc, peer_id, vdev_id, mac_addr);
			dp_peer_unref_delete(peer, DP_MOD_ID_HTT);
			return;
		}

		dp_alert("AST entry not found with peer %pK peer_id %u peer_mac "QDF_MAC_ADDR_FMT" mac_addr "QDF_MAC_ADDR_FMT" vdev_id %u next_hop %u",
			  peer, peer->peer_id,
			  QDF_MAC_ADDR_REF(peer->mac_addr.raw),
			  QDF_MAC_ADDR_REF(mac_addr), vdev_id,
			  is_wds);

		dp_peer_unref_delete(peer, DP_MOD_ID_HTT);
		return;
	}

	dp_peer_clean_wds_entries(soc, peer, free_wds_count);

	dp_cfg_event_record_peer_map_unmap_evt(soc, DP_CFG_EVENT_PEER_UNMAP,
					       peer, mac_addr, 0, peer_id,
					       0, 0, vdev_id);
	dp_info("peer_unmap_event (soc:%pK) peer_id %d peer %pK",
		soc, peer_id, peer);

	/* Clear entries in Qref LUT */
	/* TODO: Check if this is to be called from
	 * dp_peer_delete for MLO case if there is race between
	 * new peer id assignment and still not having received
	 * peer unmap for MLD peer with same peer id.
	 */
	dp_peer_rx_reo_shared_qaddr_delete(soc, peer);

	vdev = peer->vdev;

	/* only if peer is in STA mode and not tdls peer */
	if (wlan_op_mode_sta == vdev->opmode && !peer->is_tdls_peer) {
		bool peer_map = false;

		dp_tx_ppeds_cfg_astidx_cache_mapping(soc, vdev, peer_map);
	}

	dp_peer_find_id_to_obj_remove(soc, peer_id);

	if (soc->arch_ops.dp_partner_chips_unmap)
		soc->arch_ops.dp_partner_chips_unmap(soc, peer_id);

	peer->peer_id = HTT_INVALID_PEER;

	/*
	 *	 Reset ast flow mapping table
	 */
	if (!soc->ast_offload_support)
		dp_peer_reset_flowq_map(peer);

	if (soc->cdp_soc.ol_ops->peer_unmap_event) {
		soc->cdp_soc.ol_ops->peer_unmap_event(soc->ctrl_psoc,
				peer_id, vdev_id, mac_addr);
	}

	dp_update_vdev_stats_on_peer_unmap(vdev, peer);

	dp_peer_update_state(soc, peer, DP_PEER_STATE_INACTIVE);
	dp_peer_unref_delete(peer, DP_MOD_ID_HTT);
	/*
	 * Remove a reference to the peer.
	 * If there are no more references, delete the peer object.
	 */
	dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
enum dp_bands dp_freq_to_band(qdf_freq_t freq)
{
	if (REG_IS_24GHZ_CH_FREQ(freq))
		return DP_BAND_2GHZ;
	else if (REG_IS_5GHZ_FREQ(freq) || REG_IS_49GHZ_FREQ(freq))
		return DP_BAND_5GHZ;
	else if (REG_IS_6GHZ_FREQ(freq))
		return DP_BAND_6GHZ;
	return DP_BAND_INVALID;
}

void dp_map_link_id_band(struct dp_peer *peer)
{
	struct dp_txrx_peer *txrx_peer = NULL;
	enum dp_bands band;

	txrx_peer = dp_get_txrx_peer(peer);
	if (txrx_peer) {
		band = dp_freq_to_band(peer->freq);
		txrx_peer->band[peer->link_id + 1] = band;
		dp_info("Band(Freq: %u): %u mapped to Link ID: %u",
			peer->freq, band, peer->link_id);
	} else {
		dp_info("txrx_peer NULL for peer: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer->mac_addr.raw));
	}
}

QDF_STATUS
dp_rx_peer_ext_evt(struct dp_soc *soc, struct dp_peer_ext_evt_info *info)
{
	struct dp_peer *peer = NULL;
	struct cdp_peer_info peer_info = { 0 };

	QDF_ASSERT(info->peer_id <= soc->max_peer_id);

	DP_PEER_INFO_PARAMS_INIT(&peer_info, info->vdev_id, info->peer_mac_addr,
				 false, CDP_LINK_PEER_TYPE);
	peer = dp_peer_hash_find_wrapper(soc, &peer_info, DP_MOD_ID_CONFIG);

	if (!peer) {
		dp_err("peer NULL, id %u, MAC " QDF_MAC_ADDR_FMT ", vdev_id %u",
		       info->peer_id, QDF_MAC_ADDR_REF(info->peer_mac_addr),
		       info->vdev_id);

		return QDF_STATUS_E_FAILURE;
	}

	peer->link_id = info->link_id;
	peer->link_id_valid = info->link_id_valid;

	if (peer->freq)
		dp_map_link_id_band(peer);

	dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);

	return QDF_STATUS_SUCCESS;
}
#endif
#ifdef WLAN_FEATURE_11BE_MLO
void dp_rx_mlo_peer_unmap_handler(struct dp_soc *soc, uint16_t peer_id)
{
	uint16_t ml_peer_id = dp_gen_ml_peer_id(soc, peer_id);
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE] = {0};
	uint8_t vdev_id = DP_VDEV_ALL;
	uint8_t is_wds = 0;

	dp_cfg_event_record_peer_map_unmap_evt(soc, DP_CFG_EVENT_MLO_PEER_UNMAP,
					       NULL, mac_addr, 0, peer_id,
					       0, 0, vdev_id);
	dp_info("MLO peer_unmap_event (soc:%pK) peer_id %d",
		soc, peer_id);
	DP_STATS_INC(soc, t2h_msg_stats.ml_peer_unmap, 1);

	dp_rx_peer_unmap_handler(soc, ml_peer_id, vdev_id,
				 mac_addr, is_wds,
				 DP_PEER_WDS_COUNT_INVALID);
}
#endif

#ifndef AST_OFFLOAD_ENABLE
void
dp_peer_find_detach(struct dp_soc *soc)
{
	dp_soc_wds_detach(soc);
	dp_peer_find_map_detach(soc);
	dp_peer_find_hash_detach(soc);
	dp_peer_ast_hash_detach(soc);
	dp_peer_ast_table_detach(soc);
	dp_peer_mec_hash_detach(soc);
}
#else
void
dp_peer_find_detach(struct dp_soc *soc)
{
	dp_peer_find_map_detach(soc);
	dp_peer_find_hash_detach(soc);
}
#endif

void dp_peer_rx_init(struct dp_pdev *pdev, struct dp_peer *peer)
{
	dp_peer_rx_tid_setup(peer);

	peer->active_ba_session_cnt = 0;
	peer->hw_buffer_size = 0;
	peer->kill_256_sessions = 0;

	/*
	 * Set security defaults: no PN check, no security. The target may
	 * send a HTT SEC_IND message to overwrite these defaults.
	 */
	if (peer->txrx_peer)
		peer->txrx_peer->security[dp_sec_ucast].sec_type =
			peer->txrx_peer->security[dp_sec_mcast].sec_type =
				cdp_sec_type_none;
}

#ifdef WLAN_FEATURE_11BE_MLO
static void dp_peer_rx_init_reorder_queue(struct dp_pdev *pdev,
					  struct dp_peer *peer)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_peer *mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	struct dp_rx_tid *rx_tid = NULL;
	uint32_t ba_window_size, tid;
	QDF_STATUS status;

	if (dp_get_peer_vdev_roaming_in_progress(peer))
		return;

	tid = DP_NON_QOS_TID;
	rx_tid = &mld_peer->rx_tid[tid];
	ba_window_size = rx_tid->ba_status == DP_RX_BA_ACTIVE ?
					rx_tid->ba_win_size : 1;
	status = dp_peer_rx_reorder_queue_setup(soc, peer, BIT(tid), ba_window_size);
	/* Do not return on failure, continue for other tids. */
	dp_info("peer %pK " QDF_MAC_ADDR_FMT " type %d setup tid %d ba_win_size %d%s",
		peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		peer->peer_type, tid, ba_window_size,
		QDF_IS_STATUS_SUCCESS(status) ? " SUCCESS" : " FAILED");

	for (tid = 0; tid < DP_MAX_TIDS - 1; tid++) {
		rx_tid = &mld_peer->rx_tid[tid];
		ba_window_size = rx_tid->ba_status == DP_RX_BA_ACTIVE ?
						rx_tid->ba_win_size : 1;
		status = dp_peer_rx_reorder_queue_setup(soc, peer, BIT(tid),
							ba_window_size);
		/* Do not return on failure, continue for other tids. */
		dp_info("peer %pK " QDF_MAC_ADDR_FMT " type %d setup tid %d ba_win_size %d%s",
			peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
			peer->peer_type, tid, ba_window_size,
			QDF_IS_STATUS_SUCCESS(status) ? " SUCCESS" : " FAILED");
	}
}

void dp_peer_rx_init_wrapper(struct dp_pdev *pdev, struct dp_peer *peer,
			     struct cdp_peer_setup_info *setup_info)
{
	if (setup_info && !setup_info->is_first_link)
		dp_peer_rx_init_reorder_queue(pdev, peer);
	else
		dp_peer_rx_init(pdev, peer);
}
#else
void dp_peer_rx_init_wrapper(struct dp_pdev *pdev, struct dp_peer *peer,
			     struct cdp_peer_setup_info *setup_info)
{
	dp_peer_rx_init(pdev, peer);
}
#endif

void dp_peer_cleanup(struct dp_vdev *vdev, struct dp_peer *peer)
{
	enum wlan_op_mode vdev_opmode;
	uint8_t vdev_mac_addr[QDF_MAC_ADDR_SIZE];
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_soc *soc = pdev->soc;

	/* save vdev related member in case vdev freed */
	vdev_opmode = vdev->opmode;

	if (!IS_MLO_DP_MLD_PEER(peer))
		dp_monitor_peer_tx_cleanup(vdev, peer);

	if (vdev_opmode != wlan_op_mode_monitor)
	/* cleanup the Rx reorder queues for this peer */
		dp_peer_rx_cleanup(vdev, peer);

	dp_peer_rx_tids_destroy(peer);

	if (IS_MLO_DP_LINK_PEER(peer))
		dp_link_peer_del_mld_peer(peer);
	if (IS_MLO_DP_MLD_PEER(peer))
		dp_mld_peer_deinit_link_peers_info(peer);

	qdf_mem_copy(vdev_mac_addr, vdev->mac_addr.raw,
		     QDF_MAC_ADDR_SIZE);

	if (soc->cdp_soc.ol_ops->peer_unref_delete)
		soc->cdp_soc.ol_ops->peer_unref_delete(
				soc->ctrl_psoc,
				vdev->pdev->pdev_id,
				peer->mac_addr.raw, vdev_mac_addr,
				vdev_opmode);
}

QDF_STATUS
dp_set_key_sec_type_wifi3(struct cdp_soc_t *soc, uint8_t vdev_id,
			  uint8_t *peer_mac, enum cdp_sec_type sec_type,
			  bool is_unicast)
{
	struct dp_peer *peer =
			dp_peer_get_tgt_peer_hash_find((struct dp_soc *)soc,
						       peer_mac, 0, vdev_id,
						       DP_MOD_ID_CDP);
	int sec_index;

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!\n", soc);
		return QDF_STATUS_E_FAILURE;
	}

	if (!peer->txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		dp_peer_debug("%pK: txrx peer is NULL!\n", soc);
		return QDF_STATUS_E_FAILURE;
	}

	dp_peer_info("%pK: key sec spec for peer %pK " QDF_MAC_ADDR_FMT ": %s key of type %d",
		     soc, peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		     is_unicast ? "ucast" : "mcast", sec_type);

	sec_index = is_unicast ? dp_sec_ucast : dp_sec_mcast;
	peer->txrx_peer->security[sec_index].sec_type = sec_type;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

void
dp_rx_sec_ind_handler(struct dp_soc *soc, uint16_t peer_id,
		      enum cdp_sec_type sec_type, int is_unicast,
		      u_int32_t *michael_key,
		      u_int32_t *rx_pn)
{
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;
	int sec_index;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_HTT);
	if (!peer) {
		dp_peer_err("Couldn't find peer from ID %d - skipping security inits",
			    peer_id);
		return;
	}
	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_peer_err("Couldn't find txrx peer from ID %d - skipping security inits",
			    peer_id);
		return;
	}

	dp_peer_info("%pK: sec spec for peer %pK " QDF_MAC_ADDR_FMT ": %s key of type %d",
		     soc, peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
			  is_unicast ? "ucast" : "mcast", sec_type);
	sec_index = is_unicast ? dp_sec_ucast : dp_sec_mcast;

	peer->txrx_peer->security[sec_index].sec_type = sec_type;
#ifdef notyet /* TODO: See if this is required for defrag support */
	/* michael key only valid for TKIP, but for simplicity,
	 * copy it anyway
	 */
	qdf_mem_copy(
		&peer->txrx_peer->security[sec_index].michael_key[0],
		michael_key,
		sizeof(peer->txrx_peer->security[sec_index].michael_key));
#ifdef BIG_ENDIAN_HOST
	OL_IF_SWAPBO(peer->txrx_peer->security[sec_index].michael_key[0],
		     sizeof(peer->txrx_peer->security[sec_index].michael_key));
#endif /* BIG_ENDIAN_HOST */
#endif

#ifdef notyet /* TODO: Check if this is required for wifi3.0 */
	if (sec_type != cdp_sec_type_wapi) {
		qdf_mem_zero(peer->tids_last_pn_valid, _EXT_TIDS);
	} else {
		for (i = 0; i < DP_MAX_TIDS; i++) {
			/*
			 * Setting PN valid bit for WAPI sec_type,
			 * since WAPI PN has to be started with predefined value
			 */
			peer->tids_last_pn_valid[i] = 1;
			qdf_mem_copy(
				(u_int8_t *) &peer->tids_last_pn[i],
				(u_int8_t *) rx_pn, sizeof(union htt_rx_pn_t));
			peer->tids_last_pn[i].pn128[1] =
				qdf_cpu_to_le64(peer->tids_last_pn[i].pn128[1]);
			peer->tids_last_pn[i].pn128[0] =
				qdf_cpu_to_le64(peer->tids_last_pn[i].pn128[0]);
		}
	}
#endif
	/* TODO: Update HW TID queue with PN check parameters (pn type for
	 * all security types and last pn for WAPI) once REO command API
	 * is available
	 */

	dp_peer_unref_delete(peer, DP_MOD_ID_HTT);
}

#ifdef QCA_PEER_EXT_STATS
QDF_STATUS dp_peer_delay_stats_ctx_alloc(struct dp_soc *soc,
					 struct dp_txrx_peer *txrx_peer)
{
	uint8_t tid, ctx_id;

	if (!soc || !txrx_peer) {
		dp_warn("Null soc%pK or peer%pK", soc, txrx_peer);
		return QDF_STATUS_E_INVAL;
	}

	if (!wlan_cfg_is_peer_ext_stats_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	/*
	 * Allocate memory for peer extended stats.
	 */
	txrx_peer->delay_stats =
			qdf_mem_malloc(sizeof(struct dp_peer_delay_stats));
	if (!txrx_peer->delay_stats) {
		dp_err("Peer extended stats obj alloc failed!!");
		return QDF_STATUS_E_NOMEM;
	}

	for (tid = 0; tid < CDP_MAX_DATA_TIDS; tid++) {
		for (ctx_id = 0; ctx_id < CDP_MAX_TXRX_CTX; ctx_id++) {
			struct cdp_delay_tx_stats *tx_delay =
			&txrx_peer->delay_stats->delay_tid_stats[tid][ctx_id].tx_delay;
			struct cdp_delay_rx_stats *rx_delay =
			&txrx_peer->delay_stats->delay_tid_stats[tid][ctx_id].rx_delay;

			dp_hist_init(&tx_delay->tx_swq_delay,
				     CDP_HIST_TYPE_SW_ENQEUE_DELAY);
			dp_hist_init(&tx_delay->hwtx_delay,
				     CDP_HIST_TYPE_HW_COMP_DELAY);
			dp_hist_init(&rx_delay->to_stack_delay,
				     CDP_HIST_TYPE_REAP_STACK);
		}
	}

	return QDF_STATUS_SUCCESS;
}

void dp_peer_delay_stats_ctx_dealloc(struct dp_soc *soc,
				     struct dp_txrx_peer *txrx_peer)
{
	if (!txrx_peer) {
		dp_warn("peer_ext dealloc failed due to NULL peer object");
		return;
	}

	if (!wlan_cfg_is_peer_ext_stats_enabled(soc->wlan_cfg_ctx))
		return;

	if (!txrx_peer->delay_stats)
		return;

	qdf_mem_free(txrx_peer->delay_stats);
	txrx_peer->delay_stats = NULL;
}

void dp_peer_delay_stats_ctx_clr(struct dp_txrx_peer *txrx_peer)
{
	if (txrx_peer->delay_stats)
		qdf_mem_zero(txrx_peer->delay_stats,
			     sizeof(struct dp_peer_delay_stats));
}
#endif

#ifdef WLAN_PEER_JITTER
QDF_STATUS dp_peer_jitter_stats_ctx_alloc(struct dp_pdev *pdev,
					  struct dp_txrx_peer *txrx_peer)
{
	if (!pdev || !txrx_peer) {
		dp_warn("Null pdev or peer");
		return QDF_STATUS_E_INVAL;
	}

	if (!wlan_cfg_is_peer_jitter_stats_enabled(pdev->soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	if (wlan_cfg_get_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx)) {
		/*
		 * Allocate memory on per tid basis when nss is enabled
		 */
		txrx_peer->jitter_stats =
			qdf_mem_malloc(sizeof(struct cdp_peer_tid_stats)
					* DP_MAX_TIDS);
	} else {
		/*
		 * Allocate memory on per tid per ring basis
		 */
		txrx_peer->jitter_stats =
			qdf_mem_malloc(sizeof(struct cdp_peer_tid_stats)
					* DP_MAX_TIDS * CDP_MAX_TXRX_CTX);
	}

	if (!txrx_peer->jitter_stats) {
		dp_warn("Jitter stats obj alloc failed!!");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

void dp_peer_jitter_stats_ctx_dealloc(struct dp_pdev *pdev,
				      struct dp_txrx_peer *txrx_peer)
{
	if (!pdev || !txrx_peer) {
		dp_warn("Null pdev or peer");
		return;
	}

	if (!wlan_cfg_is_peer_jitter_stats_enabled(pdev->soc->wlan_cfg_ctx))
		return;

	if (txrx_peer->jitter_stats) {
		qdf_mem_free(txrx_peer->jitter_stats);
		txrx_peer->jitter_stats = NULL;
	}
}

void dp_peer_jitter_stats_ctx_clr(struct dp_txrx_peer *txrx_peer)
{
	struct cdp_peer_tid_stats *jitter_stats = NULL;

	if (!txrx_peer) {
		dp_warn("Null peer");
		return;
	}

	if (!wlan_cfg_is_peer_jitter_stats_enabled(txrx_peer->
						   vdev->
						   pdev->soc->wlan_cfg_ctx))
		return;

	jitter_stats = txrx_peer->jitter_stats;
	if (!jitter_stats)
		return;

	if (wlan_cfg_get_dp_pdev_nss_enabled(txrx_peer->
					     vdev->pdev->wlan_cfg_ctx))
		qdf_mem_zero(jitter_stats,
			     sizeof(struct cdp_peer_tid_stats) *
			     DP_MAX_TIDS);

	else
		qdf_mem_zero(jitter_stats,
			     sizeof(struct cdp_peer_tid_stats) *
			     DP_MAX_TIDS * CDP_MAX_TXRX_CTX);

}
#endif

#ifdef DP_PEER_EXTENDED_API
/**
 * dp_peer_set_bw() - Set bandwidth and mpdu retry count threshold for peer
 * @soc: DP soc handle
 * @txrx_peer: Core txrx_peer handle
 * @set_bw: enum of bandwidth to be set for this peer connection
 *
 * Return: None
 */
static void dp_peer_set_bw(struct dp_soc *soc, struct dp_txrx_peer *txrx_peer,
			   enum cdp_peer_bw set_bw)
{
	if (!txrx_peer)
		return;

	txrx_peer->bw = set_bw;

	switch (set_bw) {
	case CDP_160_MHZ:
	case CDP_320_MHZ:
		txrx_peer->mpdu_retry_threshold =
				soc->wlan_cfg_ctx->mpdu_retry_threshold_2;
		break;
	case CDP_20_MHZ:
	case CDP_40_MHZ:
	case CDP_80_MHZ:
	default:
		txrx_peer->mpdu_retry_threshold =
				soc->wlan_cfg_ctx->mpdu_retry_threshold_1;
		break;
	}

	dp_info("Peer id: %u: BW: %u, mpdu retry threshold: %u",
		txrx_peer->peer_id, txrx_peer->bw,
		txrx_peer->mpdu_retry_threshold);
}

#ifdef WLAN_FEATURE_11BE_MLO
QDF_STATUS dp_register_peer(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			    struct ol_txrx_desc_type *sta_desc)
{
	struct dp_peer *peer;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	peer = dp_peer_find_hash_find(soc, sta_desc->peer_addr.bytes,
				      0, DP_VDEV_ALL, DP_MOD_ID_CDP);

	if (!peer)
		return QDF_STATUS_E_FAULT;

	qdf_spin_lock_bh(&peer->peer_info_lock);
	peer->state = OL_TXRX_PEER_STATE_CONN;
	qdf_spin_unlock_bh(&peer->peer_info_lock);

	dp_peer_set_bw(soc, peer->txrx_peer, sta_desc->bw);

	dp_rx_flush_rx_cached(peer, false);

	if (IS_MLO_DP_LINK_PEER(peer) && peer->first_link) {
		dp_peer_info("register for mld peer" QDF_MAC_ADDR_FMT,
			     QDF_MAC_ADDR_REF(peer->mld_peer->mac_addr.raw));
		qdf_spin_lock_bh(&peer->mld_peer->peer_info_lock);
		peer->mld_peer->state = peer->state;
		qdf_spin_unlock_bh(&peer->mld_peer->peer_info_lock);
		dp_rx_flush_rx_cached(peer->mld_peer, false);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_peer_state_update(struct cdp_soc_t *soc_hdl, uint8_t *peer_mac,
				enum ol_txrx_peer_state state)
{
	struct dp_peer *peer;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	peer =  dp_peer_find_hash_find(soc, peer_mac, 0, DP_VDEV_ALL,
				       DP_MOD_ID_CDP);
	if (!peer) {
		dp_peer_err("%pK: Failed to find peer[" QDF_MAC_ADDR_FMT "]",
			    soc, QDF_MAC_ADDR_REF(peer_mac));
		return QDF_STATUS_E_FAILURE;
	}
	peer->state = state;
	peer->authorize = (state == OL_TXRX_PEER_STATE_AUTH) ? 1 : 0;

	if (peer->txrx_peer)
		peer->txrx_peer->authorize = peer->authorize;

	dp_peer_info("peer %pK MAC " QDF_MAC_ADDR_FMT " state %d",
		     peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		     peer->state);

	if (IS_MLO_DP_LINK_PEER(peer) && peer->first_link) {
		peer->mld_peer->state = peer->state;
		peer->mld_peer->txrx_peer->authorize = peer->authorize;
		dp_peer_info("mld peer %pK MAC " QDF_MAC_ADDR_FMT " state %d",
			     peer->mld_peer,
			     QDF_MAC_ADDR_REF(peer->mld_peer->mac_addr.raw),
			     peer->mld_peer->state);
	}

	/* ref_cnt is incremented inside dp_peer_find_hash_find().
	 * Decrement it here.
	 */
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS dp_register_peer(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			    struct ol_txrx_desc_type *sta_desc)
{
	struct dp_peer *peer;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	peer = dp_peer_find_hash_find(soc, sta_desc->peer_addr.bytes,
				      0, DP_VDEV_ALL, DP_MOD_ID_CDP);

	if (!peer)
		return QDF_STATUS_E_FAULT;

	qdf_spin_lock_bh(&peer->peer_info_lock);
	peer->state = OL_TXRX_PEER_STATE_CONN;
	qdf_spin_unlock_bh(&peer->peer_info_lock);

	dp_peer_set_bw(soc, peer->txrx_peer, sta_desc->bw);

	dp_rx_flush_rx_cached(peer, false);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_peer_state_update(struct cdp_soc_t *soc_hdl, uint8_t *peer_mac,
				enum ol_txrx_peer_state state)
{
	struct dp_peer *peer;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	peer =  dp_peer_find_hash_find(soc, peer_mac, 0, DP_VDEV_ALL,
				       DP_MOD_ID_CDP);
	if (!peer) {
		dp_peer_err("%pK: Failed to find peer for: [" QDF_MAC_ADDR_FMT "]",
			    soc, QDF_MAC_ADDR_REF(peer_mac));
		return QDF_STATUS_E_FAILURE;
	}
	peer->state = state;
	peer->authorize = (state == OL_TXRX_PEER_STATE_AUTH) ? 1 : 0;

	if (peer->txrx_peer)
		peer->txrx_peer->authorize = peer->authorize;

	dp_info("peer %pK state %d", peer, peer->state);
	/* ref_cnt is incremented inside dp_peer_find_hash_find().
	 * Decrement it here.
	 */
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
dp_clear_peer(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	      struct qdf_mac_addr peer_addr)
{
	struct dp_peer *peer;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	peer = dp_peer_find_hash_find(soc, peer_addr.bytes,
				      0, DP_VDEV_ALL, DP_MOD_ID_CDP);

	if (!peer)
		return QDF_STATUS_E_FAULT;
	if (!peer->valid) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAULT;
	}

	dp_clear_peer_internal(soc, peer);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_get_vdevid(struct cdp_soc_t *soc_hdl, uint8_t *peer_mac,
			 uint8_t *vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer =
		dp_peer_find_hash_find(soc, peer_mac, 0, DP_VDEV_ALL,
				       DP_MOD_ID_CDP);

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	dp_info("peer %pK vdev %pK vdev id %d",
		peer, peer->vdev, peer->vdev->vdev_id);
	*vdev_id = peer->vdev->vdev_id;
	/* ref_cnt is incremented inside dp_peer_find_hash_find().
	 * Decrement it here.
	 */
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

struct cdp_vdev *
dp_get_vdev_by_peer_addr(struct cdp_pdev *pdev_handle,
			 struct qdf_mac_addr peer_addr)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_peer *peer = NULL;
	struct cdp_vdev *vdev = NULL;

	if (!pdev) {
		dp_peer_info("PDEV not found for peer_addr: " QDF_MAC_ADDR_FMT,
			     QDF_MAC_ADDR_REF(peer_addr.bytes));
		return NULL;
	}

	peer = dp_peer_find_hash_find(pdev->soc, peer_addr.bytes, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_HIGH,
			  "PDEV not found for peer_addr: "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(peer_addr.bytes));
		return NULL;
	}

	vdev = (struct cdp_vdev *)peer->vdev;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return vdev;
}

struct cdp_vdev *dp_get_vdev_for_peer(void *peer_handle)
{
	struct dp_peer *peer = peer_handle;

	DP_TRACE(DEBUG, "peer %pK vdev %pK", peer, peer->vdev);
	return (struct cdp_vdev *)peer->vdev;
}

uint8_t *dp_peer_get_peer_mac_addr(void *peer_handle)
{
	struct dp_peer *peer = peer_handle;
	uint8_t *mac;

	mac = peer->mac_addr.raw;
	dp_info("peer %pK mac 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		peer, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return peer->mac_addr.raw;
}

int dp_get_peer_state(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		      uint8_t *peer_mac, bool slowpath)
{
	enum ol_txrx_peer_state peer_state;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct cdp_peer_info peer_info = { 0 };
	struct dp_peer *peer;
	struct dp_peer *tgt_peer;

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac,
				 false, CDP_WILD_PEER_TYPE);

	peer =  dp_peer_hash_find_wrapper(soc, &peer_info, DP_MOD_ID_CDP);

	if (!peer)
		return OL_TXRX_PEER_STATE_INVALID;

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	peer_state = tgt_peer->state;

	if (slowpath)
		dp_peer_info("peer %pK tgt_peer: %pK peer MAC "
			    QDF_MAC_ADDR_FMT " tgt peer MAC "
			    QDF_MAC_ADDR_FMT " tgt peer state %d",
			    peer, tgt_peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
			    QDF_MAC_ADDR_REF(tgt_peer->mac_addr.raw),
			    tgt_peer->state);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return peer_state;
}

void dp_local_peer_id_pool_init(struct dp_pdev *pdev)
{
	int i;

	/* point the freelist to the first ID */
	pdev->local_peer_ids.freelist = 0;

	/* link each ID to the next one */
	for (i = 0; i < OL_TXRX_NUM_LOCAL_PEER_IDS; i++) {
		pdev->local_peer_ids.pool[i] = i + 1;
		pdev->local_peer_ids.map[i] = NULL;
	}

	/* link the last ID to itself, to mark the end of the list */
	i = OL_TXRX_NUM_LOCAL_PEER_IDS;
	pdev->local_peer_ids.pool[i] = i;

	qdf_spinlock_create(&pdev->local_peer_ids.lock);
	dp_info("Peer pool init");
}

void dp_local_peer_id_alloc(struct dp_pdev *pdev, struct dp_peer *peer)
{
	int i;

	qdf_spin_lock_bh(&pdev->local_peer_ids.lock);
	i = pdev->local_peer_ids.freelist;
	if (pdev->local_peer_ids.pool[i] == i) {
		/* the list is empty, except for the list-end marker */
		peer->local_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
	} else {
		/* take the head ID and advance the freelist */
		peer->local_id = i;
		pdev->local_peer_ids.freelist = pdev->local_peer_ids.pool[i];
		pdev->local_peer_ids.map[i] = peer;
	}
	qdf_spin_unlock_bh(&pdev->local_peer_ids.lock);
	dp_info("peer %pK, local id %d", peer, peer->local_id);
}

void dp_local_peer_id_free(struct dp_pdev *pdev, struct dp_peer *peer)
{
	int i = peer->local_id;
	if ((i == OL_TXRX_INVALID_LOCAL_PEER_ID) ||
	    (i >= OL_TXRX_NUM_LOCAL_PEER_IDS)) {
		return;
	}

	/* put this ID on the head of the freelist */
	qdf_spin_lock_bh(&pdev->local_peer_ids.lock);
	pdev->local_peer_ids.pool[i] = pdev->local_peer_ids.freelist;
	pdev->local_peer_ids.freelist = i;
	pdev->local_peer_ids.map[i] = NULL;
	qdf_spin_unlock_bh(&pdev->local_peer_ids.lock);
}

bool dp_find_peer_exist_on_vdev(struct cdp_soc_t *soc_hdl,
				uint8_t vdev_id, uint8_t *peer_addr)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer = NULL;

	peer = dp_peer_find_hash_find(soc, peer_addr, 0, vdev_id,
				      DP_MOD_ID_CDP);
	if (!peer)
		return false;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return true;
}

bool dp_find_peer_exist_on_other_vdev(struct cdp_soc_t *soc_hdl,
				      uint8_t vdev_id, uint8_t *peer_addr,
				      uint16_t max_bssid)
{
	int i;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer = NULL;

	for (i = 0; i < max_bssid; i++) {
		/* Need to check vdevs other than the vdev_id */
		if (vdev_id == i)
			continue;
		peer = dp_peer_find_hash_find(soc, peer_addr, 0, i,
					      DP_MOD_ID_CDP);
		if (peer) {
			dp_err("Duplicate peer "QDF_MAC_ADDR_FMT" already exist on vdev %d",
			       QDF_MAC_ADDR_REF(peer_addr), i);
			dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
			return true;
		}
	}

	return false;
}

void dp_set_peer_as_tdls_peer(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			      uint8_t *peer_mac, bool val)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer = NULL;

	peer = dp_peer_find_hash_find(soc, peer_mac, 0, vdev_id,
				      DP_MOD_ID_CDP);
	if (!peer) {
		dp_err("Failed to find peer for:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(peer_mac));
		return;
	}

	dp_info("Set tdls flag %d for peer:" QDF_MAC_ADDR_FMT,
		val, QDF_MAC_ADDR_REF(peer_mac));
	peer->is_tdls_peer = val;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
}
#endif

bool dp_find_peer_exist(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			uint8_t *peer_addr)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer = NULL;

	peer = dp_peer_find_hash_find(soc, peer_addr, 0, DP_VDEV_ALL,
				      DP_MOD_ID_CDP);
	if (peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return true;
	}

	return false;
}

QDF_STATUS
dp_set_michael_key(struct cdp_soc_t *soc,
		   uint8_t vdev_id,
		   uint8_t *peer_mac,
		   bool is_unicast, uint32_t *key)
{
	uint8_t sec_index = is_unicast ? 1 : 0;
	struct dp_peer *peer =
			dp_peer_get_tgt_peer_hash_find((struct dp_soc *)soc,
						       peer_mac, 0, vdev_id,
						       DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_err("%pK: peer not found ", soc);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(&peer->txrx_peer->security[sec_index].michael_key[0],
		     key, IEEE80211_WEP_MICLEN);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}


struct dp_peer *dp_vdev_bss_peer_ref_n_get(struct dp_soc *soc,
					   struct dp_vdev *vdev,
					   enum dp_mod_id mod_id)
{
	struct dp_peer *peer = NULL;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		if (peer->bss_peer)
			break;
	}

	if (!peer) {
		qdf_spin_unlock_bh(&vdev->peer_list_lock);
		return NULL;
	}

	if (dp_peer_get_ref(soc, peer, mod_id) == QDF_STATUS_SUCCESS) {
		qdf_spin_unlock_bh(&vdev->peer_list_lock);
		return peer;
	}

	qdf_spin_unlock_bh(&vdev->peer_list_lock);
	return peer;
}

struct dp_peer *dp_sta_vdev_self_peer_ref_n_get(struct dp_soc *soc,
						struct dp_vdev *vdev,
						enum dp_mod_id mod_id)
{
	struct dp_peer *peer;

	if (vdev->opmode != wlan_op_mode_sta)
		return NULL;

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		if (peer->sta_self_peer)
			break;
	}

	if (!peer) {
		qdf_spin_unlock_bh(&vdev->peer_list_lock);
		return NULL;
	}

	if (dp_peer_get_ref(soc, peer, mod_id) == QDF_STATUS_SUCCESS) {
		qdf_spin_unlock_bh(&vdev->peer_list_lock);
		return peer;
	}

	qdf_spin_unlock_bh(&vdev->peer_list_lock);
	return peer;
}

void dp_peer_flush_frags(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			 uint8_t *peer_mac)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer = dp_peer_get_tgt_peer_hash_find(soc, peer_mac, 0,
							      vdev_id,
							      DP_MOD_ID_CDP);
	struct dp_txrx_peer *txrx_peer;
	uint8_t tid;
	struct dp_rx_tid_defrag *defrag_rx_tid;

	if (!peer)
		return;

	if (!peer->txrx_peer)
		goto fail;

	dp_info("Flushing fragments for peer " QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(peer->mac_addr.raw));

	txrx_peer = peer->txrx_peer;

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		defrag_rx_tid = &txrx_peer->rx_tid[tid];

		qdf_spin_lock_bh(&defrag_rx_tid->defrag_tid_lock);
		dp_rx_defrag_waitlist_remove(txrx_peer, tid);
		dp_rx_reorder_flush_frag(txrx_peer, tid);
		qdf_spin_unlock_bh(&defrag_rx_tid->defrag_tid_lock);
	}
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
}

bool dp_peer_find_by_id_valid(struct dp_soc *soc, uint16_t peer_id)
{
	struct dp_peer *peer = dp_peer_get_ref_by_id(soc, peer_id,
						     DP_MOD_ID_HTT);

	if (peer) {
		/*
		 * Decrement the peer ref which is taken as part of
		 * dp_peer_get_ref_by_id if PEER_LOCK_REF_PROTECT is enabled
		 */
		dp_peer_unref_delete(peer, DP_MOD_ID_HTT);

		return true;
	}

	return false;
}

qdf_export_symbol(dp_peer_find_by_id_valid);

#ifdef QCA_MULTIPASS_SUPPORT
void dp_peer_multipass_list_remove(struct dp_peer *peer)
{
	struct dp_vdev *vdev = peer->vdev;
	struct dp_txrx_peer *tpeer = NULL;
	bool found = 0;

	qdf_spin_lock_bh(&vdev->mpass_peer_mutex);
	TAILQ_FOREACH(tpeer, &vdev->mpass_peer_list, mpass_peer_list_elem) {
		if (tpeer == peer->txrx_peer) {
			found = 1;
			TAILQ_REMOVE(&vdev->mpass_peer_list, peer->txrx_peer,
				     mpass_peer_list_elem);
			break;
		}
	}

	qdf_spin_unlock_bh(&vdev->mpass_peer_mutex);

	if (found)
		dp_peer_unref_delete(peer, DP_MOD_ID_TX_MULTIPASS);
}

/**
 * dp_peer_multipass_list_add() - add to new multipass list
 * @soc: soc handle
 * @peer_mac: mac address
 * @vdev_id: vdev id for peer
 * @vlan_id: vlan_id
 *
 * return: void
 */
static void dp_peer_multipass_list_add(struct dp_soc *soc, uint8_t *peer_mac,
				       uint8_t vdev_id, uint16_t vlan_id)
{
	struct dp_peer *peer =
			dp_peer_get_tgt_peer_hash_find(soc, peer_mac, 0,
						       vdev_id,
						       DP_MOD_ID_TX_MULTIPASS);

	if (qdf_unlikely(!peer)) {
		qdf_err("NULL peer");
		return;
	}

	if (qdf_unlikely(!peer->txrx_peer))
		goto fail;

	/* If peer already exists in vdev multipass list, do not add it.
	 * This may happen if key install comes twice or re-key
	 * happens for a peer.
	 */
	if (peer->txrx_peer->vlan_id) {
		dp_debug("peer already added to vdev multipass list"
			 "MAC: "QDF_MAC_ADDR_FMT" vlan: %d ",
			 QDF_MAC_ADDR_REF(peer->mac_addr.raw),
			 peer->txrx_peer->vlan_id);
		goto fail;
	}

	/*
	 * Ref_cnt is incremented inside dp_peer_find_hash_find().
	 * Decrement it when element is deleted from the list.
	 */
	peer->txrx_peer->vlan_id = vlan_id;
	qdf_spin_lock_bh(&peer->txrx_peer->vdev->mpass_peer_mutex);
	TAILQ_INSERT_HEAD(&peer->txrx_peer->vdev->mpass_peer_list,
			  peer->txrx_peer,
			  mpass_peer_list_elem);
	qdf_spin_unlock_bh(&peer->txrx_peer->vdev->mpass_peer_mutex);
	return;

fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_TX_MULTIPASS);
}

void dp_peer_set_vlan_id(struct cdp_soc_t *cdp_soc,
			 uint8_t vdev_id, uint8_t *peer_mac,
			 uint16_t vlan_id)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_vdev *vdev =
		dp_vdev_get_ref_by_id((struct dp_soc *)soc, vdev_id,
				      DP_MOD_ID_TX_MULTIPASS);

	dp_info("vdev_id %d, vdev %pK, multipass_en %d, peer_mac " QDF_MAC_ADDR_FMT " vlan %d",
		vdev_id, vdev, vdev ? vdev->multipass_en : 0,
		QDF_MAC_ADDR_REF(peer_mac), vlan_id);
	if (vdev && vdev->multipass_en) {
		dp_peer_multipass_list_add(soc, peer_mac, vdev_id, vlan_id);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_TX_MULTIPASS);
	}
}
#endif /* QCA_MULTIPASS_SUPPORT */

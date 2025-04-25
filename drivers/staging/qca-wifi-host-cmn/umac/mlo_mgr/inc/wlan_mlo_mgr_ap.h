/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains MLO manager public file containing AP functionality
 */
#ifndef _WLAN_MLO_MGR_AP_H_
#define _WLAN_MLO_MGR_AP_H_

#include <wlan_mlo_mgr_cmn.h>
#include <wlan_mlo_mgr_public_structs.h>
#include "wlan_mlo_mgr_msgq.h"

#define WLAN_RESV_AID_BITS 0xc000
#define WLAN_AID(b)    ((b) & ~0xc000)
/**
 * mlo_ap_vdev_attach() - update vdev obj and vdev count to
 *                         wlan_mlo_dev_context
 * @vdev: vdev pointer
 * @link_id: link id
 * @vdev_count: number of vdev in the mlo
 *
 * Return: true if succeeds
 */
bool mlo_ap_vdev_attach(struct wlan_objmgr_vdev *vdev,
			uint8_t link_id,
			uint16_t vdev_count);

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
/**
 * mlo_ap_get_bridge_vdev_list() - get mlo bridge vdev list
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_bridge_vdev_list: bridge vdev list
 *
 * This API gets all partner bridge vdevs.
 *
 * It takes references for all vdev's with bit set in the list. Callers
 * of this API should properly release references before destroying the
 * list.
 *
 * Return: None
 */
void mlo_ap_get_bridge_vdev_list(struct wlan_objmgr_vdev *vdev,
				 uint16_t *vdev_count,
				 struct wlan_objmgr_vdev **wlan_bridge_vdev_list);

/**
 * mlo_ap_get_bridge_vdev_count() - get mlo bridge vdev count
 * @mld_ctx: mld context
 * @vdev_count: vdev count
 *
 * This API gets count of all partner bridge vdevs
 *
 * Return: None
 */
QDF_STATUS mlo_ap_get_bridge_vdev_count(struct wlan_mlo_dev_context *mld_ctx,
					uint16_t *vdev_count);

/**
 * mlo_ap_get_vdev_list_no_flag() - get mlo vdev list
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * This API gets all partner vdev's without checking for WLAN_VDEV_FEXT2_MLO.
 *
 * It takes references for all vdev's with bit set in the list. Callers
 * of this API should properly release references before destroying the
 * list.
 *
 * Return: None
 */
void mlo_ap_get_vdev_list_no_flag(struct wlan_objmgr_vdev *vdev,
				  uint16_t *vdev_count,
				  struct wlan_objmgr_vdev **wlan_vdev_list);
#else
static inline void
mlo_ap_get_bridge_vdev_list(struct wlan_objmgr_vdev *vdev,
			    uint16_t *vdev_count,
			    struct wlan_objmgr_vdev **wlan_bridge_vdev_list)
{
}

static inline QDF_STATUS
mlo_ap_get_bridge_vdev_count(struct wlan_mlo_dev_context *mld_ctx,
			     uint16_t *vdev_count)
{
	if (!vdev_count)
		return QDF_STATUS_E_NULL_VALUE;

	*vdev_count = 0;

	return QDF_STATUS_SUCCESS;
}

static inline void
mlo_ap_get_vdev_list_no_flag(struct wlan_objmgr_vdev *vdev,
			     uint16_t *vdev_count,
			     struct wlan_objmgr_vdev **wlan_vdev_list)
{
}
#endif

/**
 * mlo_ap_get_vdev_list() - get mlo vdev list
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * This API gets all partner vdev's which have WLAN_VDEV_FEXT2_MLO bit
 * set.
 *
 * It takes references for all vdev's with bit set in the list. Callers
 * of this API should properly release references before destroying the
 * list.
 *
 * Return: None
 */
void mlo_ap_get_vdev_list(struct wlan_objmgr_vdev *vdev,
			  uint16_t *vdev_count,
			  struct wlan_objmgr_vdev **wlan_vdev_list);

/**
 * mlo_peer_get_vdev_list() - get mlo peer vdev list
 * @peer: peer pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * This API gets all partner vdev's which have WLAN_VDEV_FEXT2_MLO bit
 * set.
 *
 * It takes references for all vdev's with bit set in the list. Callers
 * of this API should properly release references before destroying the
 * list.
 *
 * Return: None
 */
void mlo_peer_get_vdev_list(struct wlan_objmgr_peer *peer,
			    uint16_t *vdev_count,
			    struct wlan_objmgr_vdev **wlan_vdev_list);

/**
 * mlo_ap_get_active_vdev_list() - get mlo vdev list
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * This API gets all active partner vdev's which have WLAN_VDEV_FEXT2_MLO bit
 * set.
 *
 * It takes references for all vdev's with bit set in the list. Callers
 * of this API should properly release references before destroying the
 * list.
 *
 * Return: None
 */
void mlo_ap_get_active_vdev_list(struct wlan_objmgr_vdev *vdev,
				 uint16_t *vdev_count,
				 struct wlan_objmgr_vdev **wlan_vdev_list);

/**
 * mlo_ap_get_partner_vdev_list_from_mld() - get partner vdev from MLD
 *                                           vdev_list without checking
 *                                           WLAN_VDEV_FEXT2_MLO bit
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * This API gets all partner vdev's irrespective of WLAN_VDEV_FEXT2_MLO
 * bit. Ideally, it copies all partners of the MLD with references.
 *
 * It takes references for all vdev's in the list. The callers of this
 * API should properly release references before destroying the list.
 *
 * Return: None
 */
void mlo_ap_get_partner_vdev_list_from_mld(
		struct wlan_objmgr_vdev *vdev,
		uint16_t *vdev_count,
		struct wlan_objmgr_vdev **wlan_vdev_list);

/**
 * mlo_ap_link_sync_wait_notify() - notify the mlo manager, once vdev
 *                                  enters WLAN_VDEV_SS_MLO_SYNC_WAIT
 * @vdev: vdev pointer
 *
 * Return: true if MLO_SYNC_COMPLETE is posted, else false
 */
bool mlo_ap_link_sync_wait_notify(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_ap_link_start_rsp_notify - Notify that the link start is completed
 *
 * @vdev: pointer to vdev
 *
 * Return: none
 */
void mlo_ap_link_start_rsp_notify(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_ap_vdev_detach() - notify the mlo manager to detach given vdev
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlo_ap_vdev_detach(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_ap_link_down_cmpl_notify() - notify the mlo manager, once vdev
 *                                  is down completely
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlo_ap_link_down_cmpl_notify(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_mlme_aid_mgr_max_aid_set() - set VDEV Max AID
 * @vdev: vdev pointer
 * @max_aid: max AID
 *
 * This function sets max AID for the VDEV
 *
 * Return: void
 */
void wlan_vdev_mlme_aid_mgr_max_aid_set(struct wlan_objmgr_vdev *vdev,
					uint16_t max_aid);

/**
 * wlan_vdev_mlme_set_start_aid() - set VDEV start AID
 * @vdev: vdev pointer
 * @start_aid: start AID
 *
 * This function sets start AID for the VDEV
 *
 * Return: void
 */
QDF_STATUS wlan_vdev_mlme_set_start_aid(struct wlan_objmgr_vdev *vdev,
					uint16_t start_aid);

/**
 * wlan_vdev_mlme_get_start_aid() - set VDEV start AID
 * @vdev: vdev pointer
 *
 * This function sets start AID for the VDEV
 *
 * Return: start AID
 */
uint16_t wlan_vdev_mlme_get_start_aid(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_vdev_init_mbss_aid_mgr() - Assigns tx vdev aid mgr to a VDEV
 * @ml_dev: MLO DEV context
 * @vdev: VDEV
 * @tx_vdev: Transmit VDEV
 *
 * This function assigns Tx VDEV's AID mgr to non-Tx VDEV
 *
 * Return: SUCCESS if assigned successfully
 */
QDF_STATUS wlan_mlo_vdev_init_mbss_aid_mgr(struct wlan_mlo_dev_context *ml_dev,
					   struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_vdev *tx_vdev);

/**
 * wlan_mlo_vdev_deinit_mbss_aid_mgr() - Resets aid mgr to a non-Tx VDEV
 * @mldev: MLO DEV context
 * @vdev: VDEV
 * @tx_vdev: Transmit VDEV
 *
 * This function resets AID mgr of non-Tx VDEV
 *
 * Return: SUCCESS if reset successfully
 */
QDF_STATUS wlan_mlo_vdev_deinit_mbss_aid_mgr(struct wlan_mlo_dev_context *mldev,
					     struct wlan_objmgr_vdev *vdev,
					     struct wlan_objmgr_vdev *tx_vdev);

/**
 * wlan_mlme_vdev_init_mbss_aid_mgr() - Assigns tx vdev aid mgr to a VDEV
 * @vdev: VDEV
 * @tx_vdev: Transmit VDEV
 *
 * This function assigns Tx VDEV's AID mgr to non-Tx VDEV
 *
 * Return: SUCCESS if assigned successfully
 */
QDF_STATUS wlan_mlme_vdev_init_mbss_aid_mgr(struct wlan_objmgr_vdev *vdev,
					    struct wlan_objmgr_vdev *tx_vdev);

/**
 * wlan_mlme_vdev_deinit_mbss_aid_mgr() - Resets aid mgr to a non-Tx VDEV
 * @vdev: VDEV
 * @tx_vdev: Transmit VDEV
 *
 * This function resets AID mgr of non-Tx VDEV
 *
 * Return: SUCCESS if reset successfully
 */
QDF_STATUS wlan_mlme_vdev_deinit_mbss_aid_mgr(struct wlan_objmgr_vdev *vdev,
					      struct wlan_objmgr_vdev *tx_vdev);

/**
 * wlan_vdev_aid_mgr_init() - VDEV AID mgr init
 * @max_aid: max AID
 *
 * This function is called as part of vdev/MLO DEV initialization.
 * This will allocate aid mgr structure for a VDEV
 *
 * Return: aid_mgr
 */
struct wlan_vdev_aid_mgr *wlan_vdev_aid_mgr_init(uint16_t max_aid);

/**
 * wlan_vdev_aid_mgr_free() - VDEV AID mgr free
 * @aid_mgr: AID mgr
 *
 * This function frees the aid mgr of the VDEV
 *
 * Return: void
 */
void wlan_vdev_aid_mgr_free(struct wlan_vdev_aid_mgr *aid_mgr);

/**
 * wlan_mlo_vdev_aid_mgr_init() - MLO AID mgr init
 * @ml_dev: MLO DEV context
 *
 * This function allocate AID space for all associated VDEVs of MLD
 *
 * Return: SUCCESS if allocated successfully
 */
QDF_STATUS wlan_mlo_vdev_aid_mgr_init(struct wlan_mlo_dev_context *ml_dev);

/**
 * wlan_mlo_vdev_aid_mgr_deinit() - MLO AID mgr free
 * @ml_dev: MLO DEV context
 *
 * This function frees AID space for all associated VDEVs of MLD
 *
 * Return: void
 */
void wlan_mlo_vdev_aid_mgr_deinit(struct wlan_mlo_dev_context *ml_dev);

/**
 * wlan_mlo_vdev_alloc_aid_mgr() - Allocate AID space for a VDEV
 * @ml_dev: MLO DEV context
 * @vdev: VDEV
 *
 * This function allocates AID space for an associated VDEV of MLD
 *
 * Return: SUCCESS if allocated successfully
 */
QDF_STATUS wlan_mlo_vdev_alloc_aid_mgr(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_vdev_free_aid_mgr() - Free AID space for a VDEV
 * @ml_dev: MLO DEV context
 * @vdev: VDEV
 *
 * This function frees AID space for an associated VDEV of MLD
 *
 * Return: SUCCESS if freed successfully
 */
QDF_STATUS wlan_mlo_vdev_free_aid_mgr(struct wlan_mlo_dev_context *ml_dev,
				      struct wlan_objmgr_vdev *vdev);

/**
 * mlo_peer_allocate_aid() - Allocate AID for MLO peer
 * @ml_dev: MLO DEV context
 * @ml_peer: MLO peer object
 *
 * This function allocates AID for an MLO peer
 *
 * Return: SUCCESS if allocated successfully
 */
QDF_STATUS mlo_peer_allocate_aid(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer);

/**
 * mlo_get_aid() - Allocate AID for a associated station
 * @vdev: VDEV
 *
 * This function allocates AID for an associated station of MLD
 *
 * Return: AID
 */
uint16_t mlo_get_aid(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_free_aid() - Frees AID for a station
 * @vdev: VDEV
 * @assoc_id: Assoc ID
 *
 * This function frees AID for an associated station of MLD
 *
 * Return: SUCCESS if freed
 */
QDF_STATUS mlo_free_aid(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id);

/**
 * mlme_get_aid() - Allocate AID for a non-MLD station
 * @vdev: VDEV
 *
 * This function allocates AID for an associated NON-MLD station of MLD
 *
 * Return: AID
 */
uint16_t mlme_get_aid(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_is_aid_set() - Check whether the AID is already allocated
 * @vdev: VDEV
 * @assoc_id: Assoc ID
 *
 * This function checks whether the AID is already allocated
 *
 * Return: 1 for AID is already allocated
 *         0 for AID is available
 */
int mlme_is_aid_set(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id);

/**
 * wlan_mlo_peer_free_aid() - Free assoc id
 * @ml_aid_mgr: MLO AID mgr
 * @link_ix: Link index
 * @assoc_id: Assoc id to be freed
 *
 * This function frees assoc id, resets bit in all bitmaps
 *
 * Return: SUCCESS,if it freed
 */
QDF_STATUS wlan_mlo_peer_free_aid(
		struct wlan_ml_vdev_aid_mgr *ml_aid_mgr,
		uint8_t link_ix,
		uint16_t assoc_id);

/**
 * wlan_mlme_peer_free_aid() - Free link assoc id
 * @vdev_aid_mgr: VDEV AID mgr
 * @no_lock: lock needed for the operation
 * @assoc_id: Assoc id to be freed
 *
 * This function frees assoc id of a specific VDEV
 *
 * Return: void
 */
void wlan_mlme_peer_free_aid(
		struct wlan_vdev_aid_mgr *vdev_aid_mgr,
		bool no_lock, uint16_t assoc_id);

/**
 * mlo_peer_free_aid() - public API to free AID
 * @ml_dev: MLO DEV context
 * @ml_peer: MLO peer object
 *
 * This function invokes low level API to free assoc id
 *
 * Return: SUCCESS, if it freed
 */
QDF_STATUS mlo_peer_free_aid(struct wlan_mlo_dev_context *ml_dev,
			     struct wlan_mlo_peer_context *ml_peer);

/**
 * mlme_free_aid() - public API to free link assoc id
 * @vdev: VDEV object
 * @assoc_id: Assoc id to be freed
 *
 * This function invokes low level API to free assoc id of a specific VDEV
 *
 * Return: void
 */
void mlme_free_aid(struct wlan_objmgr_vdev *vdev, uint16_t assoc_id);

/**
 * mlo_set_aid() - public API to reserve AID
 * @vdev: VDEV object
 * @assoc_id: Assoc id to be reserved
 *
 * This function reserves AID of MLO VDEV
 *
 * Return: SUCCESS, if it is reserved
 *         FAILURE, if it is already allocated
 */
QDF_STATUS mlo_set_aid(struct wlan_objmgr_vdev *vdev,
		       uint16_t assoc_id);

/**
 * mlme_set_aid() - public API to reserve AID
 * @vdev: VDEV object
 * @assoc_id: Assoc id to be reserved
 *
 * This function reserves AID of VDEV
 *
 * Return: SUCCESS, if it is reserved
 *         FAILURE, if it is already allocated
 */
QDF_STATUS mlme_set_aid(struct wlan_objmgr_vdev *vdev,
			uint16_t assoc_id);

/**
 * wlan_mlme_get_aid_count() - public API to get AID count
 * @vdev: VDEV object
 *
 * This function counts number AIDs allocated for the VDEV
 *
 * Return: aid count value
 */
uint16_t wlan_mlme_get_aid_count(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_ap_update_max_ml_peer_ids() - public API to update max MLO peer ids
 * @pdev_id: PDEV id
 * @max_ml_peer_ids: maximum ml peer ids supported
 *
 * This function updated the maximum MLO peer ids supported for the psoc
 */
QDF_STATUS mlo_ap_update_max_ml_peer_ids(
		uint32_t pdev_id, uint32_t max_ml_peer_ids);

/**
 * mlo_ap_ml_peerid_alloc() - public API to allocate MLO peer id
 *
 * This function allocates MLO peer ID
 *
 * Return: mlo_peer_id on success,
 *	 MLO_INVALID_PEER_ID on failure
 */
uint16_t mlo_ap_ml_peerid_alloc(void);

/**
 * mlo_ap_ml_peerid_free() - public API to free MLO peer id
 * @mlo_peer_id: ML peer id
 *
 * This function frees MLO peer ID
 *
 * Return: void
 */
void mlo_ap_ml_peerid_free(uint16_t mlo_peer_id);

#define ML_PRIMARY_UMAC_ID_INVAL 0xff
/**
 * mlo_peer_assign_primary_umac() - Assign Primary UMAC
 * @ml_peer: MLO peer object
 * @peer_entry: Link peer entry
 *
 * This function assigns primary UMAC flag in peer entry
 *
 * Return: SUCCESS,if it allocated
 */
void mlo_peer_assign_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_mlo_link_peer_entry *peer_entry);

/**
 * mlo_peer_allocate_primary_umac() - Allocate Primary UMAC
 * @ml_dev: MLO DEV context
 * @ml_peer: MLO peer object
 * @link_vdevs: link vdev array
 *
 * This function allocates primary UMAC for a MLO peer
 *
 * Return: SUCCESS,if it allocated
 */
QDF_STATUS mlo_peer_allocate_primary_umac(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[]);

/**
 * mlo_peer_free_primary_umac() - Free Primary UMAC
 * @ml_dev: MLO DEV context
 * @ml_peer: MLO peer object
 *
 * This function frees primary UMAC for a MLO peer
 *
 * Return: SUCCESS,if it is freed
 */
QDF_STATUS mlo_peer_free_primary_umac(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer);

/**
 * mlo_ap_vdev_quiet_set() - Set quiet bitmap for requested vdev
 * @vdev: Pointer to object manager vdev
 *
 * Return: void
 */
void mlo_ap_vdev_quiet_set(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_ap_vdev_quiet_clear() - Clear quiet bitmap for requested vdev
 * @vdev: Pointer to object manager vdev
 *
 * Return: void
 */
void mlo_ap_vdev_quiet_clear(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_ap_vdev_quiet_is_any_idx_set() - Check if any index is set in
 * quiet bitmap
 * @vdev: Pointer to object manager vdev
 *
 * Return: true, if any index is set, else false
 */
bool mlo_ap_vdev_quiet_is_any_idx_set(struct wlan_objmgr_vdev *vdev);

#if defined(MESH_MODE_SUPPORT) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * mlo_peer_populate_mesh_params() - Populate mesh parameters in ml_peer
 * @ml_peer: ml_peer to which mesh config parameters need to be populated
 * @ml_info: ml_info with mesh config associated with this link
 *
 * Return: void
 */
void mlo_peer_populate_mesh_params(
		struct wlan_mlo_peer_context *ml_peer,
		struct mlo_partner_info *ml_info);
#else
static inline
void mlo_peer_populate_mesh_params(
		struct wlan_mlo_peer_context *ml_peer,
		struct mlo_partner_info *ml_info)
{
}
#endif

#ifdef UMAC_SUPPORT_MLNAWDS
/**
 * mlo_peer_populate_nawds_params() - Populate nawds parameters in ml_peer
 * @ml_peer: ml_peer to which nawds config parameters need to be populated
 * @ml_info: ml_info with nawds config associated with this link
 *
 * Return: void
 */
void mlo_peer_populate_nawds_params(
		struct wlan_mlo_peer_context *ml_peer,
		struct mlo_partner_info *ml_info);
#else
static inline
void mlo_peer_populate_nawds_params(
		struct wlan_mlo_peer_context *ml_peer,
		struct mlo_partner_info *ml_info)
{
}
#endif

/**
 * mlo_peer_create_get_frm_buf() - get frm_buf to peer_create
 * @ml_peer: MLO peer
 * @peer_create: pointer to peer_create_notif context
 * @frm_buf: pointer to frame buffer to be cloned to peer_create
 *
 * Return: SUCCESS if
 * - peer_create frame buffer cloned successfully in non NAWDS case Or
 * - ml_peer is in NAWDS mode.
 */
QDF_STATUS mlo_peer_create_get_frm_buf(
		struct wlan_mlo_peer_context *ml_peer,
		struct peer_create_notif_s *peer_create,
		qdf_nbuf_t frm_buf);

/**
 * wlan_mlo_ap_get_active_links() - Get number of active link VDEVs of MLD
 * @vdev: vdev pointer
 *
 * Return: active vdev count.
 */
uint16_t wlan_mlo_ap_get_active_links(struct wlan_objmgr_vdev *vdev);

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * mlo_ap_ml_ptqm_peerid_free() - API to clear ml peer id bmap set for
 * ptqm migration
 * @ml_dev: ML dev pointer
 * @mlo_peer_id: MLO peer id
 *
 * Return: none
 */
void mlo_ap_ml_ptqm_peerid_free(struct wlan_mlo_dev_context *ml_dev,
				uint16_t mlo_peer_id);
#else
static inline
void mlo_ap_ml_ptqm_peerid_free(struct wlan_mlo_dev_context *ml_dev,
				uint16_t mlo_peer_id)
{ }
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */
#endif

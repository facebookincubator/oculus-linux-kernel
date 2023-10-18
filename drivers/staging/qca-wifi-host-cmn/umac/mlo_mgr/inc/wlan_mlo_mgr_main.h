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
 * DOC: contains MLO manager containing init/deinit public api's
 */
#ifndef _WLAN_MLO_MGR_MAIN_H_
#define _WLAN_MLO_MGR_MAIN_H_

#include <qdf_atomic.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>

#ifdef WLAN_FEATURE_11BE_MLO
#include <wlan_mlo_mgr_public_structs.h>

/**
 * wlan_mlo_mgr_init() - Initialize the MLO data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_init(void);

/**
 * wlan_mlo_mgr_deinit() - De-init the MLO data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_deinit(void);

/**
 * wlan_mlo_mgr_vdev_created_notification() - mlo mgr vdev create handler
 * @vdev: vdev object
 * @arg_list: Argument list
 *
 * This function is called as part of vdev creation. This will initialize
 * the MLO dev context if the interface type is ML.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_vdev_created_notification(struct wlan_objmgr_vdev *vdev,
						  void *arg_list);

/**
 * wlan_mlo_mgr_vdev_destroyed_notification() - mlo mgr vdev delete handler
 * @vdev: vdev object
 * @arg_list: Argument list
 *
 * This function is called as part of vdev delete. This will de-initialize
 * the MLO dev context if the interface type is ML.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_vdev_destroyed_notification(struct wlan_objmgr_vdev *vdev,
						    void *arg_list);

#ifdef WLAN_MLO_USE_SPINLOCK
/**
 * ml_link_lock_create - Create MLO link mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline
void ml_link_lock_create(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spinlock_create(&mlo_ctx->ml_dev_list_lock);
}

/**
 * ml_link_lock_destroy - Destroy ml link mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
ml_link_lock_destroy(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spinlock_destroy(&mlo_ctx->ml_dev_list_lock);
}

/**
 * ml_link_lock_acquire - acquire ml link mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void ml_link_lock_acquire(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spin_lock_bh(&mlo_ctx->ml_dev_list_lock);
}

/**
 * ml_link_lock_release - release MLO dev mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void ml_link_lock_release(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spin_unlock_bh(&mlo_ctx->ml_dev_list_lock);
}

/**
 * mlo_dev_lock_create - Create MLO device mutex/spinlock
 * @mldev:  ML device context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
mlo_dev_lock_create(struct wlan_mlo_dev_context *mldev)
{
	qdf_spinlock_create(&mldev->mlo_dev_lock);
}

/**
 * mlo_dev_lock_destroy - Destroy CM SM mutex/spinlock
 * @mldev:  ML device context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
mlo_dev_lock_destroy(struct wlan_mlo_dev_context *mldev)
{
	qdf_spinlock_destroy(&mldev->mlo_dev_lock);
}

/**
 * mlo_dev_lock_acquire - acquire CM SM mutex/spinlock
 * @mldev:  ML device context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void mlo_dev_lock_acquire(struct wlan_mlo_dev_context *mldev)
{
	qdf_spin_lock_bh(&mldev->mlo_dev_lock);
}

/**
 * mlo_dev_lock_release - release MLO dev mutex/spinlock
 * @mldev:  ML device context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void mlo_dev_lock_release(struct wlan_mlo_dev_context *mldev)
{
	qdf_spin_unlock_bh(&mldev->mlo_dev_lock);
}

/**
 * ml_aid_lock_create - Create MLO aid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline
void ml_aid_lock_create(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spinlock_create(&mlo_ctx->aid_lock);
}

/**
 * ml_aid_lock_destroy - Destroy ml aid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
ml_aid_lock_destroy(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spinlock_destroy(&mlo_ctx->aid_lock);
}

/**
 * ml_aid_lock_acquire - acquire ml aid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void ml_aid_lock_acquire(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spin_lock_bh(&mlo_ctx->aid_lock);
}

/**
 * ml_aid_lock_release - release MLO aid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void ml_aid_lock_release(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spin_unlock_bh(&mlo_ctx->aid_lock);
}

/**
 * ml_peerid_lock_create - Create MLO peer mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline
void ml_peerid_lock_create(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spinlock_create(&mlo_ctx->ml_peerid_lock);
}

/**
 * ml_peerid_lock_destroy - Destroy ml peerid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
ml_peerid_lock_destroy(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spinlock_destroy(&mlo_ctx->ml_peerid_lock);
}

/**
 * ml_peerid_lock_acquire - acquire ml peerid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void ml_peerid_lock_acquire(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spin_lock_bh(&mlo_ctx->ml_peerid_lock);
}

/**
 * ml_peerid_lock_release - release MLO peerid mutex/spinlock
 * @mlo_ctx:  MLO manager global context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void ml_peerid_lock_release(struct mlo_mgr_context *mlo_ctx)
{
	qdf_spin_unlock_bh(&mlo_ctx->ml_peerid_lock);
}

/**
 * mlo_peer_lock_create - Create MLO peer mutex/spinlock
 * @mlpeer:  ML peer
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
mlo_peer_lock_create(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_spinlock_create(&mlpeer->mlo_peer_lock);
}

/**
 * mlo_peer_lock_destroy - Destroy MLO peer mutex/spinlock
 * @mlpeer:  ML peer
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
mlo_peer_lock_destroy(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_spinlock_destroy(&mlpeer->mlo_peer_lock);
}

/**
 * mlo_peer_lock_acquire - acquire mlo peer mutex/spinlock
 * @mlpeer:  MLO peer context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void mlo_peer_lock_acquire(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_spin_lock_bh(&mlpeer->mlo_peer_lock);
}

/**
 * mlo_peer_lock_release - release MLO peer mutex/spinlock
 * @mlpeer:  MLO peer context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void mlo_peer_lock_release(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_spin_unlock_bh(&mlpeer->mlo_peer_lock);
}

/**
 * ml_peerlist_lock_create - Create MLO peer list mutex/spinlock
 * @ml_peerlist:  ML peer list context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
ml_peerlist_lock_create(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_spinlock_create(&ml_peerlist->peer_list_lock);
}

/**
 * ml_peerlist_lock_destroy - Destroy MLO peer list mutex/spinlock
 * @ml_peerlist:  ML peer list context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
ml_peerlist_lock_destroy(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_spinlock_destroy(&ml_peerlist->peer_list_lock);
}

/**
 * ml_peerlist_lock_acquire - acquire ML peer list mutex/spinlock
 * @ml_peerlist:  ML peer list context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void ml_peerlist_lock_acquire(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_spin_lock_bh(&ml_peerlist->peer_list_lock);
}

/**
 * ml_peerlist_lock_release - release ML peer list mutex/spinlock
 * @ml_peerlist:  ML peer list context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void ml_peerlist_lock_release(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_spin_unlock_bh(&ml_peerlist->peer_list_lock);
}

/**
 * copied_conn_req_lock_create - Create original connect req mutex/spinlock
 * @sta_ctx:  MLO STA related information
 *
 * Create mutex/spinlock
 *
 * return: void
 */
static inline
void copied_conn_req_lock_create(struct wlan_mlo_sta *sta_ctx)
{
	qdf_spinlock_create(&sta_ctx->copied_conn_req_lock);
}

/**
 * copied_conn_req_lock_destroy - Destroy original connect req mutex/spinlock
 * @sta_ctx:  MLO STA related information
 *
 * Destroy mutex/spinlock
 *
 * return: void
 */
static inline
void copied_conn_req_lock_destroy(struct wlan_mlo_sta *sta_ctx)
{
	qdf_spinlock_destroy(&sta_ctx->copied_conn_req_lock);
}

/**
 * copied_conn_req_lock_acquire - Acquire original connect req mutex/spinlock
 * @sta_ctx:  MLO STA related information
 *
 * Acquire mutex/spinlock
 *
 * return: void
 */
static inline
void copied_conn_req_lock_acquire(struct wlan_mlo_sta *sta_ctx)
{
	qdf_spin_lock_bh(&sta_ctx->copied_conn_req_lock);
}

/**
 * copied_conn_req_lock_release - Release original connect req mutex/spinlock
 * @sta_ctx:  MLO STA related information
 *
 * Release mutex/spinlock
 *
 * return: void
 */
static inline
void copied_conn_req_lock_release(struct wlan_mlo_sta *sta_ctx)
{
	qdf_spin_unlock_bh(&sta_ctx->copied_conn_req_lock);
}

/**
 * tsf_recalculation_lock_create - Create TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
tsf_recalculation_lock_create(struct wlan_mlo_dev_context *mldev)
{
	qdf_spinlock_create(&mldev->tsf_recalculation_lock);
}

/**
 * tsf_recalculation_lock_destroy - Destroy TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
tsf_recalculation_lock_destroy(struct wlan_mlo_dev_context *mldev)
{
	qdf_spinlock_destroy(&mldev->tsf_recalculation_lock);
}

/**
 * tsf_recalculation_lock_acquire - Acquire TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * Acquire mutex/spinlock
 *
 * return: void
 */
static inline
void tsf_recalculation_lock_acquire(struct wlan_mlo_dev_context *mldev)
{
	qdf_spin_lock_bh(&mldev->tsf_recalculation_lock);
}

/**
 * tsf_recalculation_lock_release - Release TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void tsf_recalculation_lock_release(struct wlan_mlo_dev_context *mldev)
{
	qdf_spin_unlock_bh(&mldev->tsf_recalculation_lock);
}

/**
 * mlo_ap_lock_create - Create MLO AP mutex/spinlock
 * @ap_ctx:  ML device AP context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline
void mlo_ap_lock_create(struct wlan_mlo_ap *ap_ctx)
{
	qdf_spinlock_create(&ap_ctx->mlo_ap_lock);
}

/**
 * mlo_ap_lock_destroy - Destroy MLO AP mutex/spinlock
 * @ap_ctx:  ML device AP context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline
void mlo_ap_lock_destroy(struct wlan_mlo_ap *ap_ctx)
{
	qdf_spinlock_destroy(&ap_ctx->mlo_ap_lock);
}

/**
 * mlo_ap_lock_acquire - Acquire MLO AP mutex/spinlock
 * @ap_ctx:  ML device AP context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void mlo_ap_lock_acquire(struct wlan_mlo_ap *ap_ctx)
{
	qdf_spin_lock_bh(&ap_ctx->mlo_ap_lock);
}

/**
 * mlo_ap_lock_release - Release MLO AP mutex/spinlock
 * @ap_ctx:  ML device AP context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void mlo_ap_lock_release(struct wlan_mlo_ap *ap_ctx)
{
	qdf_spin_unlock_bh(&ap_ctx->mlo_ap_lock);
}
#else /* WLAN_MLO_USE_SPINLOCK */
static inline
void ml_link_lock_create(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_create(&mlo_ctx->ml_dev_list_lock);
}

static inline void
ml_link_lock_destroy(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_destroy(&mlo_ctx->ml_dev_list_lock);
}

static inline
void ml_link_lock_acquire(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_acquire(&mlo_ctx->ml_dev_list_lock);
}

static inline
void ml_link_lock_release(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_release(&mlo_ctx->ml_dev_list_lock);
}

static inline
void mlo_dev_lock_create(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_create(&mldev->mlo_dev_lock);
}

static inline
void mlo_dev_lock_destroy(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_destroy(&mldev->mlo_dev_lock);
}

static inline void mlo_dev_lock_acquire(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_acquire(&mldev->mlo_dev_lock);
}

static inline void mlo_dev_lock_release(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_release(&mldev->mlo_dev_lock);
}

static inline
void ml_aid_lock_create(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_create(&mlo_ctx->aid_lock);
}

static inline void
ml_aid_lock_destroy(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_destroy(&mlo_ctx->aid_lock);
}

static inline
void ml_aid_lock_acquire(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_acquire(&mlo_ctx->aid_lock);
}

static inline
void ml_aid_lock_release(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_release(&mlo_ctx->aid_lock);
}

static inline
void ml_peerid_lock_create(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_create(&mlo_ctx->ml_peerid_lock);
}

static inline void
ml_peerid_lock_destroy(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_destroy(&mlo_ctx->ml_peerid_lock);
}

static inline
void ml_peerid_lock_acquire(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_acquire(&mlo_ctx->ml_peerid_lock);
}

static inline
void ml_peerid_lock_release(struct mlo_mgr_context *mlo_ctx)
{
	qdf_mutex_release(&mlo_ctx->ml_peerid_lock);
}

static inline void
mlo_peer_lock_create(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_mutex_create(&mlpeer->mlo_peer_lock);
}

static inline void
mlo_peer_lock_destroy(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_mutex_destroy(&mlpeer->mlo_peer_lock);
}

static inline
void mlo_peer_lock_acquire(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_mutex_acquire(&mlpeer->mlo_peer_lock);
}

static inline
void mlo_peer_lock_release(struct wlan_mlo_peer_context *mlpeer)
{
	qdf_mutex_release(&mlpeer->mlo_peer_lock);
}

static inline void
ml_peerlist_lock_create(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_mutex_create(&ml_peerlist->peer_list_lock);
}

static inline void
ml_peerlist_lock_destroy(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_mutex_destroy(&ml_peerlist->peer_list_lock);
}

static inline
void ml_peerlist_lock_acquire(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_mutex_acquire(&ml_peerlist->peer_list_lock);
}

static inline
void ml_peerlist_lock_release(struct wlan_mlo_peer_list *ml_peerlist)
{
	qdf_mutex_release(&ml_peerlist->peer_list_lock);
}

static inline
void copied_conn_req_lock_create(struct wlan_mlo_sta *sta_ctx)
{
	qdf_mutex_create(&sta_ctx->copied_conn_req_lock);
}

static inline
void copied_conn_req_lock_destroy(struct wlan_mlo_sta *sta_ctx)
{
	qdf_mutex_destroy(&sta_ctx->copied_conn_req_lock);
}

static inline
void copied_conn_req_lock_acquire(struct wlan_mlo_sta *sta_ctx)
{
	qdf_mutex_acquire(&sta_ctx->copied_conn_req_lock);
}

static inline
void copied_conn_req_lock_release(struct wlan_mlo_sta *sta_ctx)
{
	qdf_mutex_release(&sta_ctx->copied_conn_req_lock);
}

/**
 * tsf_recalculation_lock_create - Create TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
tsf_recalculation_lock_create(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_create(&mldev->tsf_recalculation_lock);
}

/**
 * tsf_recalculation_lock_destroy - Destroy TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
tsf_recalculation_lock_destroy(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_destroy(&mldev->tsf_recalculation_lock);
}

/**
 * tsf_recalculation_lock_acquire - Acquire TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * Acquire mutex/spinlock
 *
 * return: void
 */
static inline
void tsf_recalculation_lock_acquire(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_acquire(&mldev->tsf_recalculation_lock);
}

/**
 * tsf_recalculation_lock_release - Release TSF recalculation mutex/spinlock
 * @mldev:  ML device context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void tsf_recalculation_lock_release(struct wlan_mlo_dev_context *mldev)
{
	qdf_mutex_release(&mldev->tsf_recalculation_lock);
}

static inline
void mlo_ap_lock_create(struct wlan_mlo_ap *ap_ctx)
{
	qdf_mutex_create(&ap_ctx->mlo_ap_lock);
}

static inline
void mlo_ap_lock_destroy(struct wlan_mlo_ap *ap_ctx)
{
	qdf_mutex_destroy(&ap_ctx->mlo_ap_lock);
}

static inline
void mlo_ap_lock_acquire(struct wlan_mlo_ap *ap_ctx)
{
	qdf_mutex_acquire(&ap_ctx->mlo_ap_lock);
}

static inline
void mlo_ap_lock_release(struct wlan_mlo_ap *ap_ctx)
{
	qdf_mutex_release(&ap_ctx->mlo_ap_lock);
}
#endif /* WLAN_MLO_USE_SPINLOCK */

/**
 * wlan_mlo_mgr_psoc_enable() - MLO psoc enable handler
 * @psoc: psoc pointer
 *
 * API to execute operations on psoc enable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlo_mgr_psoc_disable() - MLO psoc disable handler
 * @psoc: psoc pointer
 *
 * API to execute operations on psoc disable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlo_mgr_update_mld_addr() - Update MLD MAC address
 * @old_mac: Old MLD MAC address
 * @new_mac: New MLD MAC address
 *
 * API to update MLD MAC address once ML dev context is created.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_update_mld_addr(struct qdf_mac_addr *old_mac,
					struct qdf_mac_addr *new_mac);

/**
 * wlan_mlo_is_mld_ctx_exist() - check whether MLD exist with MLD MAC address
 * @mldaddr: MLD MAC address
 *
 * API to check whether MLD is present with MLD MAC address.
 *
 * Return: true, if it is present
 *         false, if it is not present
 */
bool wlan_mlo_is_mld_ctx_exist(struct qdf_mac_addr *mldaddr);

/**
 * wlan_mlo_get_sta_mld_ctx_count() - Get number of sta mld device context
 *
 * API to get number of sta mld device context
 *
 * Return: number of sta mld device context
 */
uint8_t wlan_mlo_get_sta_mld_ctx_count(void);

/**
 * wlan_mlo_get_mld_ctx_by_mldaddr() - Get mld device context using mld
 *                                     MAC address
 *
 * @mldaddr: MAC address of the MLD device
 *
 * API to get mld device context using the mld mac address
 *
 * Return: Pointer to mlo device context
 */
struct wlan_mlo_dev_context
*wlan_mlo_get_mld_ctx_by_mldaddr(struct qdf_mac_addr *mldaddr);

/**
 * wlan_mlo_check_valid_config() - Check vap config is valid for mld
 *
 * @ml_dev: Pointer to structure of mlo device context
 * @pdev: Reference pdev to check against MLD list
 * @opmode: Operating mode of vdev (SAP/STA etc..)
 *
 * API to check if vaps config is valid
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_check_valid_config(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_objmgr_pdev *pdev,
				       enum QDF_OPMODE opmode);

/**
 * mlo_mgr_ml_peer_exist_on_diff_ml_ctx() - Check if MAC address matches any
 * MLD address
 * @peer_addr: Address to search for a match
 * @peer_vdev_id: vdev ID of peer
 *
 * The API iterates through all the ML dev ctx in the global MLO
 * manager to check if MAC address pointed by @peer_addr matches
 * the MLD address of any ML dev context or its ML peers.
 * If @peer_vdev_id is a valid pointer address, then API returns
 * true only if the matching MAC address is not part of the same
 * ML dev context.
 *
 * Return: True if a matching entity is found else false.
 */
bool mlo_mgr_ml_peer_exist_on_diff_ml_ctx(uint8_t *peer_addr,
					  uint8_t *peer_vdev_id);

/**
 * wlan_mlo_update_action_frame_from_user() - Change MAC address in WLAN frame
 * received from userspace.
 * @vdev: VDEV objmgr pointer.
 * @frame: Pointer to start of WLAN MAC frame.
 * @frame_len: Length of the frame.
 *
 * The API will translate MLD address in the SA, DA, BSSID for the action
 * frames received from userspace with link address to send over the air.
 * The API will not modify if the frame is a Public Action category frame and
 * for VDEV other then STA mode.
 *
 * Return: void
 */
void wlan_mlo_update_action_frame_from_user(struct wlan_objmgr_vdev *vdev,
					    uint8_t *frame,
					    uint32_t frame_len);

/**
 * wlan_mlo_update_action_frame_to_user() - Change MAC address in WLAN frame
 * received over the air.
 * @vdev: VDEV objmgr pointer.
 * @frame: Pointer to start of WLAN MAC frame.
 * @frame_len: Length of the frame.
 *
 * The API will translate link address in the SA, DA, BSSID for the action
 * frames received over the air with MLD address to send to userspace.
 * The API will not modify if the frame is a Public Action category frame and
 * for VDEV other then STA mode.
 *
 * Return: void
 */
void wlan_mlo_update_action_frame_to_user(struct wlan_objmgr_vdev *vdev,
					  uint8_t *frame,
					  uint32_t frame_len);
#else
static inline QDF_STATUS wlan_mlo_mgr_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS wlan_mlo_mgr_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlo_mgr_update_mld_addr(struct qdf_mac_addr *old_mac,
			     struct qdf_mac_addr *new_mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool mlo_mgr_ml_peer_exist_on_diff_ml_ctx(uint8_t *peer_addr,
					  uint8_t *peer_vdev_id)
{
	return false;
}

static inline
void wlan_mlo_update_action_frame_from_user(struct wlan_objmgr_vdev *vdev,
					    uint8_t *frame,
					    uint32_t frame_len)
{
}

static inline
void wlan_mlo_update_action_frame_to_user(struct wlan_objmgr_vdev *vdev,
					  uint8_t *frame,
					  uint32_t frame_len)
{
}

static inline
uint8_t wlan_mlo_get_sta_mld_ctx_count(void)
{
	return 0;
}
#endif
#endif

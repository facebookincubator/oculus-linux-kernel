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
 * DOC: contains MLO manager init/deinit api's
 */
#include "wlan_cmn.h"
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_mlo_mgr_main.h"
#include <wlan_mlo_mgr_ap.h>
#include <wlan_mlo_mgr_peer.h>
#include <wlan_mlo_mgr_setup.h>
#include <wlan_cm_public_struct.h>
#include "wlan_mlo_mgr_msgq.h"
#include <target_if_mlo_mgr.h>
#include <wlan_mlo_t2lm.h>
#include <wlan_cm_api.h>
#include <wlan_mlo_mgr_public_api.h>
#include "cdp_txrx_cmn.h"

#ifdef WLAN_WSI_STATS_SUPPORT
/*
 * wlan_mlo_wsi_get_num_psocs() - Get the number of attached PSOCs
 * @psoc: Pointer to psoc
 * @arg: Pointer to variable to store count
 * @index: Index for iteration function
 */
static void wlan_mlo_wsi_get_num_psocs(struct wlan_objmgr_psoc *psoc,
				       void *arg, uint8_t index)
{
	/* If arg is NULL then skip increment */
	if (!arg)
		return;

	(*(uint32_t *)arg)++;
}

static void mlo_wsi_link_info_deinit(struct mlo_mgr_context *mlo_mgr)
{
	if (!mlo_mgr)
		return;

	if (mlo_mgr->wsi_info) {
		qdf_mem_free(mlo_mgr->wsi_info);
		mlo_mgr->wsi_info = NULL;
	}
}

#ifdef WLAN_MLO_MULTI_CHIP
void mlo_wsi_link_info_update_soc(struct wlan_objmgr_psoc *psoc,
				  uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_wsi_psoc_grp *mlo_grp_info;
	uint8_t i, j;

	if (!mlo_ctx) {
		mlo_err("Invalid mlo_mgr_ctx");
		return;
	}

	mlo_grp_info = &mlo_ctx->wsi_info->mlo_psoc_grp[grp_id];
	if (!mlo_grp_info) {
		mlo_err("mlo_grp_info is invalid for ix %d", i);
		return;
	}

	mlo_ctx->wsi_info->num_psoc++;

	/* Set the PSOC order for the MLO group */
	for (j = 0; j < WLAN_OBJMGR_MAX_DEVICES; j++) {
		if (mlo_grp_info->psoc_order[j] == MLO_WSI_PSOC_ID_MAX) {
			mlo_grp_info->psoc_order[j] = wlan_psoc_get_id(psoc);
			mlo_grp_info->num_psoc++;
			break;
		}
	}
}
#endif

static void
mlo_wsi_link_info_setup_mlo_grps(struct mlo_mgr_context *mlo_mgr)
{
	struct mlo_wsi_psoc_grp *mlo_grp_info;
	uint8_t i, j;

	if (!mlo_mgr) {
		mlo_err("Invalid mlo_mgr");
		return;
	}

	if (!mlo_mgr->wsi_info) {
		mlo_err("Invalid wsi_info");
		return;
	}

	wlan_objmgr_iterate_psoc_list(wlan_mlo_wsi_get_num_psocs,
				      &mlo_mgr->wsi_info->num_psoc,
				      WLAN_MLO_MGR_ID);
	if (!mlo_mgr->wsi_info->num_psoc)
		mlo_info("Could not find active PSOCs");

	for (i = 0; i < MLO_WSI_MAX_MLO_GRPS; i++) {
		mlo_grp_info =
			&mlo_mgr->wsi_info->mlo_psoc_grp[i];
		if (!mlo_grp_info) {
			mlo_err("mlo_grp_info is invalid for ix %d", i);
			continue;
		}

		/* Set the PSOC order for the MLO group */
		for (j = 0; j < WLAN_OBJMGR_MAX_DEVICES; j++) {
			/*
			 * NOTE: Inclusion of more MLO groups will require
			 * changes to this block where rvalue will need
			 * to be checked against the group they need to
			 * be assigned to.
			 */
			if (j < mlo_mgr->wsi_info->num_psoc) {
				mlo_grp_info->psoc_order[j] = j;
				mlo_grp_info->num_psoc++;
			} else {
				mlo_grp_info->psoc_order[j] =
							MLO_WSI_PSOC_ID_MAX;
			}
			mlo_err("PSOC order %d, index %d",
				mlo_grp_info->psoc_order[j], j);
		}
	}
}

static void mlo_wsi_link_info_init(struct mlo_mgr_context *mlo_mgr)
{
	uint8_t i;

	if (!mlo_mgr)
		return;

	/* Initialize the mlo_wsi_link_info structure */
	mlo_mgr->wsi_info = qdf_mem_malloc(
					sizeof(struct mlo_wsi_info));
	if (!mlo_mgr->wsi_info) {
		mlo_err("Could not allocate memory for wsi_link_info");
		return;
	}

	/* Initialize the MLO group context in the WSI stats */
	for (i = 0; i < MLO_WSI_MAX_MLO_GRPS; i++)
		mlo_wsi_link_info_setup_mlo_grps(mlo_mgr);
}
#else
static void mlo_wsi_link_info_init(struct mlo_mgr_context *mlo_mgr)
{
}

static void mlo_wsi_link_info_deinit(struct mlo_mgr_context *mlo_mgr)
{
}
#endif

static void mlo_global_ctx_deinit(void)
{
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx)
		return;

	if (qdf_list_empty(&mlo_mgr_ctx->ml_dev_list))
		mlo_debug("ML dev list is not empty");

	/* Deallocation of the WSI link information */
	mlo_wsi_link_info_deinit(mlo_mgr_ctx);

	mlo_setup_deinit();
	mlo_msgq_free();
	ml_peerid_lock_destroy(mlo_mgr_ctx);
	ml_link_lock_destroy(mlo_mgr_ctx);
	ml_aid_lock_destroy(mlo_mgr_ctx);
	qdf_list_destroy(&mlo_mgr_ctx->ml_dev_list);

	qdf_mem_free(mlo_mgr_ctx);
	wlan_objmgr_set_mlo_ctx(NULL);
}

static void mlo_global_ctx_init(void)
{
	struct mlo_mgr_context *mlo_mgr_ctx;

	/* If it is already created, ignore */
	if (wlan_objmgr_get_mlo_ctx()) {
		mlo_err("Global object is already created");
		return;
	}

	/* Allocation of memory for Global object */
	mlo_mgr_ctx = (struct mlo_mgr_context *)
			qdf_mem_malloc(sizeof(*mlo_mgr_ctx));
	if (!mlo_mgr_ctx)
		return;

	wlan_objmgr_set_mlo_ctx(mlo_mgr_ctx);

	qdf_list_create(&mlo_mgr_ctx->ml_dev_list, WLAN_UMAC_MLO_MAX_DEV);
	mlo_mgr_ctx->max_mlo_peer_id = MAX_MLO_PEER_ID;
	mlo_mgr_ctx->last_mlo_peer_id = 0;
	ml_peerid_lock_create(mlo_mgr_ctx);
	ml_link_lock_create(mlo_mgr_ctx);
	ml_aid_lock_create(mlo_mgr_ctx);
	mlo_mgr_ctx->mlo_is_force_primary_umac = 0;
	mlo_mgr_ctx->force_non_assoc_prim_umac = 0;
	mlo_msgq_init();

	/* Allocation of the WSI link information */
	mlo_wsi_link_info_init(mlo_mgr_ctx);
}

/**
 * wlan_mlo_check_psoc_capable() - Check if psoc is mlo capable
 * @psoc: psoc pointer
 *
 * API to check if psoc is mlo capable
 *
 * Return: bool, true if capable else false
 */
#ifdef WLAN_MLO_MULTI_CHIP
static bool wlan_mlo_check_psoc_capable(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlo_get_psoc_capable(psoc);
}
#else
static bool wlan_mlo_check_psoc_capable(struct wlan_objmgr_psoc *psoc)
{
	return true;
}
#endif

QDF_STATUS wlan_mlo_mgr_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;

	if (!psoc) {
		mlo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!wlan_mlo_check_psoc_capable(psoc))
		return QDF_STATUS_SUCCESS;

	mlo_tx_ops = target_if_mlo_get_tx_ops(psoc);
	if (!mlo_tx_ops) {
		mlo_err("tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_tx_ops->register_events) {
		mlo_err("register_events function is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mlo_tx_ops->register_events(psoc);
}

QDF_STATUS wlan_mlo_mgr_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;

	if (!psoc) {
		mlo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = target_if_mlo_get_tx_ops(psoc);
	if (!mlo_tx_ops) {
		mlo_err("tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_tx_ops->unregister_events) {
		mlo_err("unregister_events function is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return mlo_tx_ops->unregister_events(psoc);
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
QDF_STATUS
mlo_mgr_register_link_switch_notifier(enum wlan_umac_comp_id comp_id,
				      mlo_mgr_link_switch_notifier_cb cb)
{
	struct mlo_mgr_context *g_mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!g_mlo_mgr_ctx) {
		mlo_err("global mlo mgr not initialized");
		return QDF_STATUS_E_INVAL;
	}

	if (!cb || comp_id >= WLAN_UMAC_COMP_ID_MAX) {
		mlo_err("Invalid component");
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (g_mlo_mgr_ctx->lswitch_notifier[comp_id].in_use)
		return QDF_STATUS_E_ALREADY;

	g_mlo_mgr_ctx->lswitch_notifier[comp_id].in_use = true;
	g_mlo_mgr_ctx->lswitch_notifier[comp_id].cb = cb;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
mlo_mgr_unregister_link_switch_notifier(enum wlan_umac_comp_id comp_id)
{
	struct mlo_mgr_context *g_mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!g_mlo_mgr_ctx) {
		mlo_err("global mlo mgr not initialized");
		return QDF_STATUS_E_INVAL;
	}

	if (comp_id >= WLAN_UMAC_COMP_ID_MAX) {
		mlo_err("Invalid component");
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!g_mlo_mgr_ctx->lswitch_notifier[comp_id].in_use)
		return QDF_STATUS_E_INVAL;

	g_mlo_mgr_ctx->lswitch_notifier[comp_id].in_use = false;
	g_mlo_mgr_ctx->lswitch_notifier[comp_id].cb = NULL;
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlo_mgr_init_link_switch_notifier(void)
{
	int i;
	struct mlo_mgr_context *g_mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!g_mlo_mgr_ctx)
		return QDF_STATUS_E_INVAL;

	for (i = 0; i < WLAN_UMAC_COMP_ID_MAX; i++) {
		g_mlo_mgr_ctx->lswitch_notifier[i].in_use = false;
		g_mlo_mgr_ctx->lswitch_notifier[i].cb = NULL;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS mlo_mgr_init_link_switch_notifier(void)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */

QDF_STATUS wlan_mlo_mgr_init(void)
{
	QDF_STATUS status;

	mlo_global_ctx_init();

	status = wlan_objmgr_register_vdev_create_handler(
		WLAN_UMAC_COMP_MLO_MGR,
		wlan_mlo_mgr_vdev_created_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Failed to register vdev create handler");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(WLAN_UMAC_COMP_MLO_MGR,
		wlan_mlo_mgr_vdev_destroyed_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_debug("Failed to register VDEV destroy handler");
		wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_MLO_MGR,
					wlan_mlo_mgr_vdev_created_notification, NULL);
		return status;
	}

	status = mlo_mgr_init_link_switch_notifier();
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = mlo_mgr_register_link_switch_notifier(WLAN_UMAC_COMP_MLO_MGR,
							       mlo_mgr_link_switch_notification);
		return status;
	}
	if (status == QDF_STATUS_E_NOSUPPORT)
		status = QDF_STATUS_SUCCESS;

	return status;
}

QDF_STATUS wlan_mlo_mgr_deinit(void)
{
	QDF_STATUS status;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx) {
		mlo_err("MLO global object is not allocated");
		return QDF_STATUS_E_FAILURE;
	}

	wlan_mlo_mgr_unregister_link_switch_notifier(WLAN_UMAC_COMP_MLO_MGR);

	mlo_global_ctx_deinit();

	status = wlan_objmgr_unregister_vdev_create_handler(
		WLAN_UMAC_COMP_MLO_MGR,
		wlan_mlo_mgr_vdev_created_notification, NULL);
	if (status != QDF_STATUS_SUCCESS)
		mlo_err("Failed to unregister vdev create handler");

	status = wlan_objmgr_unregister_vdev_destroy_handler(
			WLAN_UMAC_COMP_MLO_MGR,
			wlan_mlo_mgr_vdev_destroyed_notification, NULL);
	if (status != QDF_STATUS_SUCCESS)
		mlo_err("Failed to unregister vdev delete handler");

	return status;
}

struct wlan_mlo_dev_context *wlan_mlo_list_peek_head(
					qdf_list_t *ml_list)
{
	struct wlan_mlo_dev_context *mld_ctx;
	qdf_list_node_t *ml_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (qdf_list_peek_front(ml_list, &ml_node) != QDF_STATUS_SUCCESS)
		return NULL;

	mld_ctx = qdf_container_of(ml_node, struct wlan_mlo_dev_context,
				   node);

	return mld_ctx;
}

struct wlan_mlo_dev_context *wlan_mlo_get_next_mld_ctx(qdf_list_t *ml_list,
					struct wlan_mlo_dev_context *mld_cur)
{
	struct wlan_mlo_dev_context *mld_next;
	qdf_list_node_t *node = &mld_cur->node;
	qdf_list_node_t *next_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (!node)
		return NULL;

	if (qdf_list_peek_next(ml_list, node, &next_node) !=
						QDF_STATUS_SUCCESS)
		return NULL;

	mld_next = qdf_container_of(next_node, struct wlan_mlo_dev_context,
				    node);
	return mld_next;
}

uint8_t wlan_mlo_get_sta_mld_ctx_count(void)
{
	struct wlan_mlo_dev_context *mld_cur;
	struct wlan_mlo_dev_context *mld_next;
	qdf_list_t *ml_list;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t count = 0;

	if (!mlo_mgr_ctx)
		return count;

	ml_link_lock_acquire(mlo_mgr_ctx);
	ml_list = &mlo_mgr_ctx->ml_dev_list;
	/* Get first mld context */
	mld_cur = wlan_mlo_list_peek_head(ml_list);

	while (mld_cur) {
		/* get next mld node */
		if (mld_cur->sta_ctx)
			count++;
		mld_next = wlan_mlo_get_next_mld_ctx(ml_list, mld_cur);
		mld_cur = mld_next;
	}
	ml_link_lock_release(mlo_mgr_ctx);

	return count;
}

struct wlan_mlo_dev_context
*wlan_mlo_get_mld_ctx_by_mldaddr(struct qdf_mac_addr *mldaddr)
{
	struct wlan_mlo_dev_context *mld_cur;
	struct wlan_mlo_dev_context *mld_next;
	qdf_list_t *ml_list;
	struct mlo_mgr_context *mlo_mgr_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_mgr_ctx)
		return NULL;

	ml_link_lock_acquire(mlo_mgr_ctx);
	ml_list = &mlo_mgr_ctx->ml_dev_list;
	/* Get first mld context */
	mld_cur = wlan_mlo_list_peek_head(ml_list);
	/**
	 * Iterate through ml list, till ml mldaddr matches with
	 * entry of list
	 */
	while (mld_cur) {
		if (QDF_IS_STATUS_SUCCESS(WLAN_ADDR_EQ(&mld_cur->mld_addr,
					  mldaddr))) {
			ml_link_lock_release(mlo_mgr_ctx);
			return mld_cur;
		}
		/* get next mld node */
		mld_next = wlan_mlo_get_next_mld_ctx(ml_list, mld_cur);
		mld_cur = mld_next;
	}
	ml_link_lock_release(mlo_mgr_ctx);

	return NULL;
}

bool wlan_mlo_is_mld_ctx_exist(struct qdf_mac_addr *mldaddr)
{
	struct wlan_mlo_dev_context *mld_ctx = NULL;

	mld_ctx = wlan_mlo_get_mld_ctx_by_mldaddr(mldaddr);
	if (mld_ctx)
		return true;

	return false;
}

#ifdef WLAN_MLO_MULTI_CHIP
QDF_STATUS mlo_mgr_is_mld_has_active_link(bool *is_active)
{
	qdf_list_t *ml_list;
	uint32_t idx, count;
	struct wlan_mlo_dev_context *mld_cur, *mld_next;
	struct wlan_objmgr_vdev *vdev;
	struct mlo_mgr_context *g_mlo_ctx = wlan_objmgr_get_mlo_ctx();
	QDF_STATUS status;

	if (!g_mlo_ctx || !is_active)
		return QDF_STATUS_E_FAILURE;

	ml_link_lock_acquire(g_mlo_ctx);
	ml_list = &g_mlo_ctx->ml_dev_list;
	if (!qdf_list_size(ml_list)) {
		ml_link_lock_release(g_mlo_ctx);
		return QDF_STATUS_E_FAILURE;
	}

	*is_active = false;
	mld_cur = wlan_mlo_list_peek_head(ml_list);
	while (mld_cur) {
		mlo_dev_lock_acquire(mld_cur);
		count = QDF_ARRAY_SIZE(mld_cur->wlan_vdev_list);
		for (idx = 0; idx < count; idx++) {
			vdev = mld_cur->wlan_vdev_list[idx];
			if (!vdev)
				continue;

			status = wlan_vdev_mlme_is_init_state(vdev);
			if (QDF_STATUS_SUCCESS == status)
				continue;

			qdf_err("VDEV [vdev_id %u, pdev_id %u, psoc_id %u, state %u] is still active",
				wlan_vdev_get_id(vdev),
				wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev)),
				wlan_vdev_get_psoc_id(vdev),
				wlan_vdev_mlme_get_state(vdev));
			*is_active = true;
			mlo_dev_lock_release(mld_cur);
			ml_link_lock_release(g_mlo_ctx);
			return QDF_STATUS_SUCCESS;
		}
		mld_next = wlan_mlo_get_next_mld_ctx(ml_list, mld_cur);
		mlo_dev_lock_release(mld_cur);
		mld_cur = mld_next;
	}
	ml_link_lock_release(g_mlo_ctx);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(mlo_mgr_is_mld_has_active_link);
#endif

#ifdef WLAN_FEATURE_11BE_MLO
bool mlo_mgr_ml_peer_exist_on_diff_ml_ctx(uint8_t *peer_addr,
					  uint8_t *peer_vdev_id)
{
	qdf_list_t *ml_list;
	uint32_t idx, count;
	struct wlan_mlo_dev_context *mld_cur, *mld_next;
	struct wlan_mlo_peer_list *mlo_peer_list;
	struct wlan_objmgr_vdev *vdev;
	bool ret_status = false, same_ml_ctx = false;
	struct mlo_mgr_context *g_mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!g_mlo_ctx || !peer_addr ||
	    qdf_is_macaddr_zero((struct qdf_mac_addr *)peer_addr))
		return ret_status;

	ml_link_lock_acquire(g_mlo_ctx);
	ml_list = &g_mlo_ctx->ml_dev_list;
	if (!qdf_list_size(ml_list))
		goto g_ml_ref;

	mld_cur = wlan_mlo_list_peek_head(ml_list);
	while (mld_cur) {
		mlo_dev_lock_acquire(mld_cur);
		if (qdf_is_macaddr_equal(&mld_cur->mld_addr,
					 (struct qdf_mac_addr *)peer_addr)) {
			/* For self peer, the address passed will match the
			 * MLD address of its own ML dev context, so allow
			 * peer creation in this scenario as both are in
			 * same ML dev context.
			 */
			if (peer_vdev_id) {
				count = QDF_ARRAY_SIZE(mld_cur->wlan_vdev_list);
				for (idx = 0; idx < count; idx++) {
					vdev = mld_cur->wlan_vdev_list[idx];
					if (!vdev)
						continue;
					if (*peer_vdev_id ==
					    wlan_vdev_get_id(vdev)) {
						same_ml_ctx = true;
						break;
					}
				}
			}
			mlo_dev_lock_release(mld_cur);
			mlo_err("MLD ID %d exists with mac " QDF_MAC_ADDR_FMT,
				mld_cur->mld_id, QDF_MAC_ADDR_REF(peer_addr));
			ret_status = true;
			goto check_same_ml_ctx;
		}

		/* Check the peer list for a MAC address match */
		mlo_peer_list = &mld_cur->mlo_peer_list;
		ml_peerlist_lock_acquire(mlo_peer_list);
		if (mlo_get_mlpeer(mld_cur, (struct qdf_mac_addr *)peer_addr)) {
			/* If peer_vdev_id is NULL, then API will treat any
			 * match as happening on another dev context
			 */
			if (peer_vdev_id) {
				count = QDF_ARRAY_SIZE(mld_cur->wlan_vdev_list);
				for (idx = 0; idx < count; idx++) {
					vdev = mld_cur->wlan_vdev_list[idx];
					if (!vdev)
						continue;
					if (*peer_vdev_id ==
					    wlan_vdev_get_id(vdev)) {
						same_ml_ctx = true;
						break;
					}
				}
			}
			ml_peerlist_lock_release(mlo_peer_list);
			mlo_dev_lock_release(mld_cur);
			mlo_err("MLD ID %d ML Peer exists with mac " QDF_MAC_ADDR_FMT,
				mld_cur->mld_id, QDF_MAC_ADDR_REF(peer_addr));
			ret_status = true;
			goto check_same_ml_ctx;
		}
		ml_peerlist_lock_release(mlo_peer_list);

		mld_next = wlan_mlo_get_next_mld_ctx(ml_list, mld_cur);
		mlo_dev_lock_release(mld_cur);
		mld_cur = mld_next;
	}

check_same_ml_ctx:
	if (same_ml_ctx)
		ret_status = false;

g_ml_ref:
	ml_link_lock_release(g_mlo_ctx);
	return ret_status;
}

#define WLAN_HDD_MGMT_FRAME_DA_OFFSET 4
#define WLAN_HDD_MGMT_FRAME_SA_OFFSET (WLAN_HDD_MGMT_FRAME_DA_OFFSET + 6)
#define WLAN_HDD_MGMT_FRAME_BSSID_OFFSET (WLAN_HDD_MGMT_FRAME_SA_OFFSET + 6)
#define WLAN_HDD_MGMT_FRAME_ACTION_CATEGORY_OFFSET \
				(WLAN_HDD_MGMT_FRAME_BSSID_OFFSET + 6 + 2)
#define WLAN_HDD_MGMT_FRAME_ACTION_TYPE_OFFSET \
				(WLAN_HDD_MGMT_FRAME_ACTION_CATEGORY_OFFSET + 1)
#define WLAN_HDD_ACTION_FRAME_CATEGORY_PUBLIC 0x04

/*
 * Typical 802.11 Action Frame Format
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 * | FC | DUR |  DA  |   SA  | BSSID |Seq.|Cat.|Act|   Elements   | FCS |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *    2    2     6       6       6     2    1    1   Variable Len    4
 */
void wlan_mlo_update_action_frame_from_user(struct wlan_objmgr_vdev *vdev,
					    uint8_t *frame,
					    uint32_t frame_len)
{
	struct wlan_objmgr_peer *peer;
	uint8_t *da, *sa, *bssid;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev) ||
	    (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE))
		return;

	if (frame_len <= WLAN_HDD_MGMT_FRAME_ACTION_TYPE_OFFSET) {
		mlo_debug("Not a valid Action frame len: %d", frame_len);
		return;
	}

	/* Translate address only for action frames
	 * which are not of public category.
	 * Reference: 802.11-2012, Subclause: 8.5
	 */

	if (frame[WLAN_HDD_MGMT_FRAME_ACTION_CATEGORY_OFFSET] ==
				WLAN_HDD_ACTION_FRAME_CATEGORY_PUBLIC)
		return;

	da = frame + WLAN_HDD_MGMT_FRAME_DA_OFFSET;
	sa = frame + WLAN_HDD_MGMT_FRAME_SA_OFFSET;
	bssid = frame + WLAN_HDD_MGMT_FRAME_BSSID_OFFSET;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		mlo_debug("Peer not found");
		return;
	}

	mlo_debug("Change MLD addr to link addr for non-Public action frame");
	/* DA = VDEV's BSS peer's link address.
	 * SA = VDEV's link address.
	 * BSSID = VDEV's BSS peer's link address.
	 */

	qdf_ether_addr_copy(da, wlan_peer_get_macaddr(peer));
	qdf_ether_addr_copy(sa, wlan_vdev_mlme_get_macaddr(vdev));
	qdf_ether_addr_copy(bssid, wlan_peer_get_macaddr(peer));

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
}

void wlan_mlo_update_action_frame_to_user(struct wlan_objmgr_vdev *vdev,
					  uint8_t *frame,
					  uint32_t frame_len)
{
	struct wlan_objmgr_peer *peer;
	uint8_t *da, *sa, *bssid;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev) ||
	    (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE))
		return;

	if (frame_len <= WLAN_HDD_MGMT_FRAME_ACTION_TYPE_OFFSET) {
		mlo_debug("Not a valid Action frame len: %d", frame_len);
		return;
	}

	/* Translate address only for action frames
	 * which are not of public category.
	 * Reference: 802.11-2012, Subclause: 8.5
	 */

	if (frame[WLAN_HDD_MGMT_FRAME_ACTION_CATEGORY_OFFSET] ==
				WLAN_HDD_ACTION_FRAME_CATEGORY_PUBLIC)
		return;

	da = frame + WLAN_HDD_MGMT_FRAME_DA_OFFSET;
	sa = frame + WLAN_HDD_MGMT_FRAME_SA_OFFSET;
	bssid = frame + WLAN_HDD_MGMT_FRAME_BSSID_OFFSET;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		mlo_debug("Peer not found");
		return;
	}

	mlo_debug("Change link addr to MLD addr for non-Public action frame");
	/* DA = VDEV's MLD address.
	 * SA = VDEV's BSS peer's MLD address.
	 * BSSID = VDEV's BSS peer's MLD address.
	 */

	qdf_ether_addr_copy(da, wlan_vdev_mlme_get_mldaddr(vdev));
	qdf_ether_addr_copy(sa, wlan_peer_mlme_get_mldaddr(peer));
	qdf_ether_addr_copy(bssid, wlan_peer_mlme_get_mldaddr(peer));

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
}
#endif

static QDF_STATUS mlo_ap_ctx_deinit(struct wlan_mlo_dev_context *ml_dev)
{
	wlan_mlo_vdev_aid_mgr_deinit(ml_dev);
	mlo_ap_lock_destroy(ml_dev->ap_ctx);
	qdf_mem_free(ml_dev->ap_ctx);
	ml_dev->ap_ctx = NULL;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlo_ap_ctx_init(struct wlan_mlo_dev_context *ml_dev)
{
	struct wlan_mlo_ap *ap_ctx;

	ap_ctx = qdf_mem_malloc(sizeof(*ap_ctx));
	if (!ap_ctx) {
		mlo_err("MLO AP ctx alloc failure");
		return QDF_STATUS_E_NOMEM;
	}

	ml_dev->ap_ctx = ap_ctx;
	mlo_ap_lock_create(ml_dev->ap_ctx);
	if (wlan_mlo_vdev_aid_mgr_init(ml_dev) != QDF_STATUS_SUCCESS) {
		mlo_ap_ctx_deinit(ml_dev);
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_AP_PLATFORM
static inline
QDF_STATUS wlan_mlo_check_grp_id(uint8_t ref_id,
				 struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	uint8_t grp_id = 0;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!mlo_psoc_get_grp_id(psoc, &grp_id)) {
		mlo_err("Unable to get mlo group id");
		return QDF_STATUS_E_FAILURE;
	}

	if (grp_id != ref_id) {
		mlo_err("Error : MLD VAP Configuration with different WSI/MLD Groups");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_mlo_pdev_check(struct wlan_objmgr_pdev *ref_pdev,
			       struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	uint8_t grp_id = 0;

	pdev = wlan_vdev_get_pdev(vdev);

	psoc = wlan_pdev_get_psoc(ref_pdev);
	if (!mlo_psoc_get_grp_id(psoc, &grp_id)) {
		mlo_err("Unable to get the MLO Group ID for the vdev");
		return QDF_STATUS_E_FAILURE;
	}

	if (mlo_check_all_pdev_state(psoc, grp_id, MLO_LINK_SETUP_DONE)) {
		mlo_err("Pdev link is not in ready state, initial link setup failed");
		return QDF_STATUS_E_FAILURE;
	}

	if (ref_pdev == pdev) {
		mlo_err("MLD vdev for this pdev already found, investigate config");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_mlo_check_grp_id(grp_id, vdev))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS mlo_dev_config_check(struct wlan_mlo_dev_context *ml_dev,
				struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

#else
static inline
QDF_STATUS mlo_dev_config_check(struct wlan_mlo_dev_context *ml_dev,
				struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);

	if (wlan_mlo_check_valid_config(ml_dev, wlan_vdev_get_pdev(vdev),
					opmode) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_mlo_pdev_check(struct wlan_objmgr_pdev *ref_pdev,
			       struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_mlo_check_valid_config(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_objmgr_pdev *pdev,
				       enum QDF_OPMODE opmode)
{
	uint32_t id = 0;
	struct wlan_objmgr_vdev *vdev;

	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	mlo_dev_lock_acquire(ml_dev);
	while (id < WLAN_UMAC_MLO_MAX_VDEVS) {
		vdev = ml_dev->wlan_vdev_list[id];
		if (vdev) {
			if (wlan_mlo_pdev_check(pdev, vdev)) {
				mlo_dev_lock_release(ml_dev);
				return QDF_STATUS_E_FAILURE;
			}

			if (wlan_vdev_mlme_get_opmode(vdev) != opmode) {
				mlo_err("Invalid opmode %d type found expected %d, investigate config",
					wlan_vdev_mlme_get_opmode(vdev),
					opmode);
				mlo_dev_lock_release(ml_dev);
				return QDF_STATUS_E_FAILURE;
			}
		}
		id++;
	}

	mlo_dev_lock_release(ml_dev);
	return QDF_STATUS_SUCCESS;
}

/**
 * mlo_t2lm_ctx_init() - API to initialize the t2lm context with the default
 * values.
 * @ml_dev: Pointer to ML Dev context
 * @vdev: Pointer to vdev structure
 *
 * Return: None
 */
static inline void mlo_t2lm_ctx_init(struct wlan_mlo_dev_context *ml_dev,
				     struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_info *t2lm;

	t2lm = &ml_dev->t2lm_ctx.established_t2lm.t2lm;

	qdf_mem_zero(&ml_dev->t2lm_ctx, sizeof(struct wlan_t2lm_context));

	t2lm->direction = WLAN_T2LM_BIDI_DIRECTION;
	t2lm->default_link_mapping = 1;
	t2lm->link_mapping_size = 0;

	wlan_mlo_t2lm_timer_init(vdev);
	wlan_mlo_t2lm_register_link_update_notify_handler(ml_dev);
}

/**
 * mlo_epcs_ctx_init() - API to initialize the epcs context with the
 * default values.
 * @ml_dev: Pointer to ML Dev context
 *
 * Return: None
 */
static inline void mlo_epcs_ctx_init(struct wlan_mlo_dev_context *ml_dev)
{
	struct wlan_epcs_context *epcs_ctx;

	epcs_ctx = &ml_dev->epcs_ctx;
	qdf_mem_zero(epcs_ctx, sizeof(struct wlan_epcs_context));
	epcs_dev_lock_create(epcs_ctx);
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * mlo_ptqm_migration_init() - API to initialize ptqm migration timer
 * @ml_dev: Pointer to ML Dev context
 *
 * Return: None
 */
static inline void mlo_ptqm_migration_init(struct wlan_mlo_dev_context *ml_dev)
{
	qdf_timer_init(NULL, &ml_dev->ptqm_migrate_timer,
		       mlo_mlme_ptqm_migrate_timer_cb, (void *)(ml_dev),
		       QDF_TIMER_TYPE_WAKE_APPS);
}
#else
static inline void mlo_ptqm_migration_init(struct wlan_mlo_dev_context *ml_dev)
{ }
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
static QDF_STATUS
mlo_add_to_bridge_vdev_list(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct qdf_mac_addr *mld_addr;
	uint8_t id = 0;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	ml_dev = wlan_mlo_get_mld_ctx_by_mldaddr(mld_addr);

	if (ml_dev) {
		mlo_dev_lock_acquire(ml_dev);
		while (id < WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS) {
			if (ml_dev->wlan_bridge_vdev_list[id]) {
				id++;
				continue;
			}

			ml_dev->wlan_bridge_vdev_list[id] = vdev;
			ml_dev->wlan_bridge_vdev_count++;
			vdev->mlo_dev_ctx = ml_dev;
			break;
		}
		mlo_dev_lock_release(ml_dev);

		if (id == WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS)
			return QDF_STATUS_E_FAILURE;

		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
mld_delete_from_bridge_vdev_list(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct qdf_mac_addr *mld_addr;
	uint8_t id = 0;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	ml_dev = wlan_mlo_get_mld_ctx_by_mldaddr(mld_addr);

	if (ml_dev) {
		mlo_dev_lock_acquire(ml_dev);
		while (id < WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS) {
			if (ml_dev->wlan_bridge_vdev_list[id] == vdev) {
				vdev->mlo_dev_ctx = NULL;
				ml_dev->wlan_bridge_vdev_list[id] = NULL;
				ml_dev->wlan_bridge_vdev_count--;
				break;
			}
			id++;
		}
		mlo_dev_lock_release(ml_dev);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}
#else
static QDF_STATUS
mlo_add_to_bridge_vdev_list(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
mld_delete_from_bridge_vdev_list(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS mlo_dev_ctx_init(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr *mld_addr;
	struct mlo_mgr_context *g_mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id = 0;
	struct wlan_objmgr_psoc *psoc = NULL;

	if (wlan_vdev_mlme_is_mlo_bridge_vdev(vdev)) {
		status = mlo_add_to_bridge_vdev_list(vdev);
		if (!QDF_IS_STATUS_SUCCESS(status))
			mlo_err("Failed to init bridge vap ctx");
		return status;
	}

	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	ml_dev = wlan_mlo_get_mld_ctx_by_mldaddr(mld_addr);
	psoc = wlan_vdev_get_psoc(vdev);

	if (!psoc) {
		mlo_err("Failed to get psoc");
		return QDF_STATUS_E_FAILURE;
	}

	if (ml_dev) {
		if (mlo_dev_config_check(ml_dev, vdev) != QDF_STATUS_SUCCESS)
			return QDF_STATUS_E_FAILURE;

		mlo_dev_lock_acquire(ml_dev);
		while (id < WLAN_UMAC_MLO_MAX_VDEVS) {
			if (ml_dev->wlan_vdev_list[id]) {
				id++;
				continue;
			}

			ml_dev->wlan_vdev_list[id] = vdev;
			ml_dev->wlan_vdev_count++;
			vdev->mlo_dev_ctx = ml_dev;

			if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
				wlan_mlo_vdev_alloc_aid_mgr(ml_dev, vdev);

			break;
		}
		mlo_dev_lock_release(ml_dev);
		return QDF_STATUS_SUCCESS;
	}

	/* Create a new ML dev context */
	ml_dev = qdf_mem_malloc(sizeof(*ml_dev));
	if (!ml_dev) {
		mlo_err("Failed to allocate memory for ML dev");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_copy_macaddr(&ml_dev->mld_addr, mld_addr);
	ml_dev->wlan_vdev_list[0] = vdev;
	ml_dev->wlan_vdev_count++;
	vdev->mlo_dev_ctx = ml_dev;

	mlo_dev_lock_create(ml_dev);
	tsf_recalculation_lock_create(ml_dev);
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		ml_dev->sta_ctx = qdf_mem_malloc(sizeof(struct wlan_mlo_sta));
		if (!ml_dev->sta_ctx) {
			tsf_recalculation_lock_destroy(ml_dev);
			mlo_dev_lock_destroy(ml_dev);
			qdf_mem_free(ml_dev);
			return QDF_STATUS_E_NOMEM;
		}
		copied_conn_req_lock_create(ml_dev->sta_ctx);
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
		ml_dev->bridge_sta_ctx = qdf_mem_malloc(sizeof(struct wlan_mlo_bridge_sta));
		if (!ml_dev->bridge_sta_ctx) {
			tsf_recalculation_lock_destroy(ml_dev);
			mlo_dev_lock_destroy(ml_dev);
			qdf_mem_free(ml_dev->sta_ctx);
			qdf_mem_free(ml_dev);
			return QDF_STATUS_E_NOMEM;
		}
#endif
	} else if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
		if (mlo_ap_ctx_init(ml_dev) != QDF_STATUS_SUCCESS) {
			tsf_recalculation_lock_destroy(ml_dev);
			mlo_dev_lock_destroy(ml_dev);
			qdf_mem_free(ml_dev);
			mlo_err("Failed to allocate memory for ap ctx");
			return QDF_STATUS_E_NOMEM;
		}
	}

	/* Create DP MLO Device Context */
	if (cdp_mlo_dev_ctxt_create(wlan_psoc_get_dp_handle(psoc),
				    (uint8_t *)mld_addr) !=
				    QDF_STATUS_SUCCESS) {
		tsf_recalculation_lock_destroy(ml_dev);
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
			qdf_mem_free(ml_dev->sta_ctx);
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
			qdf_mem_free(ml_dev->bridge_sta_ctx);
#endif
		} else if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
			mlo_ap_ctx_deinit(ml_dev);
		}
		mlo_dev_lock_destroy(ml_dev);
		qdf_mem_free(ml_dev);
		mlo_err("Failed to create DP MLO Dev ctxt");
		return QDF_STATUS_E_NOMEM;
	}

	ml_dev->mlo_max_recom_simult_links =
		WLAN_UMAC_MLO_RECOM_MAX_SIMULT_LINKS_DEFAULT;

	mlo_dev_mlpeer_list_init(ml_dev);

	ml_link_lock_acquire(g_mlo_ctx);
	if (qdf_list_size(&g_mlo_ctx->ml_dev_list) < WLAN_UMAC_MLO_MAX_DEV)
		qdf_list_insert_back(&g_mlo_ctx->ml_dev_list, &ml_dev->node);
	ml_link_lock_release(g_mlo_ctx);

	mlo_t2lm_ctx_init(ml_dev, vdev);
	mlo_epcs_ctx_init(ml_dev);
	mlo_ptqm_migration_init(ml_dev);
	mlo_mgr_link_switch_init(psoc, ml_dev);

	return status;
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * mlo_ptqm_migration_deinit() - API to deinitialize ptqm migration timer
 * @ml_dev: Pointer to ML Dev context
 *
 * Return: None
 */
static inline void mlo_ptqm_migration_deinit(
			struct wlan_mlo_dev_context *ml_dev)
{
	qdf_timer_free(&ml_dev->ptqm_migrate_timer);
}
#else
static inline void mlo_ptqm_migration_deinit(
			struct wlan_mlo_dev_context *ml_dev)
{ }
#endif

/**
 * mlo_t2lm_ctx_deinit() - API to deinitialize the t2lm context with the default
 * values.
 * @vdev: Pointer to vdev structure
 * @ml_dev: Pointer to mlo dev context
 *
 * Return: None
 */
static inline void mlo_t2lm_ctx_deinit(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlo_dev_context *ml_dev)
{
	wlan_mlo_t2lm_timer_deinit(vdev);
	wlan_unregister_t2lm_link_update_notify_handler(
			ml_dev, ml_dev->t2lm_ctx.link_update_callback_index);
}

/**
 * mlo_epcs_ctx_deinit() - API to deinitialize the epcs context with the default
 * values.
 * @mlo_dev_ctx: MLO dev context pointer
 *
 * Return: None
 */
static inline void mlo_epcs_ctx_deinit(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
	epcs_dev_lock_destroy(&mlo_dev_ctx->epcs_ctx);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void ml_free_copied_reassoc_rsp(struct wlan_mlo_sta *sta_ctx)
{
	wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
}
#else
static void ml_free_copied_reassoc_rsp(struct wlan_mlo_sta *sta_ctx)
{
	return;
}
#endif

static QDF_STATUS mlo_dev_ctx_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr *mld_addr;
	struct mlo_mgr_context *g_mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id = 0;
	struct wlan_cm_connect_req *connect_req;
	struct wlan_objmgr_psoc *psoc = NULL;

	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	ml_dev = wlan_mlo_get_mld_ctx_by_mldaddr(mld_addr);
	psoc = wlan_vdev_get_psoc(vdev);

	if (!psoc) {
		mlo_err("Failed to get psoc");
		return QDF_STATUS_E_FAILURE;
	}

	if (!ml_dev) {
		mlo_err("Failed to get MLD dev context by mld addr "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mld_addr->bytes));
		if (!vdev->mlo_dev_ctx) {
			mlo_err("Failed to get MLD dev context from vdev");
			return QDF_STATUS_SUCCESS;
		}
		ml_dev = vdev->mlo_dev_ctx;
	}

	mlo_debug("deleting vdev from MLD device ctx "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(mld_addr->bytes));

	mlo_dev_lock_acquire(ml_dev);
	while (id < WLAN_UMAC_MLO_MAX_VDEVS) {
		if (ml_dev->wlan_vdev_list[id] == vdev) {
			if (wlan_vdev_mlme_get_opmode(vdev) ==
							QDF_SAP_MODE)
				wlan_mlo_vdev_free_aid_mgr(ml_dev,
							   vdev);
			ml_dev->wlan_vdev_list[id] = NULL;
			ml_dev->wlan_vdev_count--;
			vdev->mlo_dev_ctx = NULL;
			break;
		}
		id++;
	}
	mlo_dev_lock_release(ml_dev);

	if (wlan_vdev_mlme_is_mlo_bridge_vdev(vdev)) {
		status = mld_delete_from_bridge_vdev_list(vdev);
		if (!QDF_IS_STATUS_SUCCESS(status))
			mlo_err("Failed to deinit bridge vap ctx");
	}

	ml_link_lock_acquire(g_mlo_ctx);
	if (!ml_dev->wlan_vdev_count && !ml_dev->wlan_bridge_vdev_count) {
		if (ml_dev->ap_ctx)
			mlo_ap_ctx_deinit(ml_dev);

		mlo_dev_mlpeer_list_deinit(ml_dev);
		qdf_list_remove_node(&g_mlo_ctx->ml_dev_list,
				     &ml_dev->node);
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
			connect_req = ml_dev->sta_ctx->connect_req;
			wlan_cm_free_connect_req(connect_req);

			if (ml_dev->sta_ctx->disconn_req)
				qdf_mem_free(ml_dev->sta_ctx->disconn_req);

			if (ml_dev->sta_ctx->assoc_rsp.ptr)
				qdf_mem_free(ml_dev->sta_ctx->assoc_rsp.ptr);

			ml_free_copied_reassoc_rsp(ml_dev->sta_ctx);

			copied_conn_req_lock_destroy(ml_dev->sta_ctx);

			qdf_mem_free(ml_dev->sta_ctx);
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
			qdf_mem_free(ml_dev->bridge_sta_ctx);
#endif
		}
		else if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
			qdf_mem_free(ml_dev->ap_ctx);

		mlo_ptqm_migration_deinit(ml_dev);
		mlo_mgr_link_switch_deinit(ml_dev);
		mlo_t2lm_ctx_deinit(vdev, ml_dev);
		mlo_epcs_ctx_deinit(ml_dev);

		/* Destroy DP MLO Device Context */
		if (cdp_mlo_dev_ctxt_destroy(wlan_psoc_get_dp_handle(psoc),
					     (uint8_t *)mld_addr) !=
					     QDF_STATUS_SUCCESS) {
			mlo_err("Failed to destroy DP MLO Dev ctxt");
			QDF_BUG(0);
		}

		tsf_recalculation_lock_destroy(ml_dev);
		mlo_dev_lock_destroy(ml_dev);
		qdf_mem_free(ml_dev);
	}
	ml_link_lock_release(g_mlo_ctx);
	return status;
}

QDF_STATUS wlan_mlo_mgr_vdev_created_notification(struct wlan_objmgr_vdev *vdev,
						  void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr *mld_addr;

	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	if (qdf_is_macaddr_zero(mld_addr)) {
		/* It's not a ML interface*/
		return QDF_STATUS_SUCCESS;
	}
	mlo_debug("MLD addr" QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(mld_addr->bytes));
	status = mlo_dev_ctx_init(vdev);

	wlan_vdev_set_link_id(vdev, WLAN_LINK_ID_INVALID);

	return status;
}

QDF_STATUS wlan_mlo_mgr_vdev_destroyed_notification(struct wlan_objmgr_vdev *vdev,
						    void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr *mld_addr;

	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
	if (qdf_is_macaddr_zero(mld_addr)) {
		/* It's not a ML interface*/
		return QDF_STATUS_SUCCESS;
	}
	mlo_debug("MLD addr" QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(mld_addr->bytes));

	status = mlo_dev_ctx_deinit(vdev);

	return status;
}

QDF_STATUS wlan_mlo_mgr_update_mld_addr(struct qdf_mac_addr *old_mac,
					struct qdf_mac_addr *new_mac)
{
	struct wlan_mlo_dev_context *ml_dev;

	ml_dev = wlan_mlo_get_mld_ctx_by_mldaddr(old_mac);
	if (!ml_dev) {
		mlo_err("ML dev context not found for MLD:" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(old_mac->bytes));
		return QDF_STATUS_E_INVAL;
	}
	mlo_dev_lock_acquire(ml_dev);
	qdf_copy_macaddr(&ml_dev->mld_addr, new_mac);
	mlo_dev_lock_release(ml_dev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_mgr_mld_vdev_attach(struct wlan_objmgr_vdev *vdev,
					struct qdf_mac_addr *mld_addr)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc = NULL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		mlo_err("Failed to get psoc");
		return QDF_STATUS_E_FAILURE;
	}

	wlan_vdev_obj_lock(vdev);
	wlan_vdev_mlme_set_mldaddr(vdev, (uint8_t *)&mld_addr->bytes[0]);
	wlan_vdev_obj_unlock(vdev);

	status = mlo_dev_ctx_init(vdev);

	if (cdp_mlo_dev_ctxt_attach(wlan_psoc_get_dp_handle(psoc),
				    wlan_vdev_get_id(vdev),
				    (uint8_t *)mld_addr)
				    != QDF_STATUS_SUCCESS) {
		mlo_err("Failed to attach DP vdev  (" QDF_MAC_ADDR_FMT ") to"
			" MLO Dev ctxt (" QDF_MAC_ADDR_FMT ")",
			QDF_MAC_ADDR_REF(wlan_vdev_mlme_get_macaddr(vdev)),
			QDF_MAC_ADDR_REF(mld_addr->bytes));
	}
	return status;
}

QDF_STATUS wlan_mlo_mgr_mld_vdev_detach(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr *mld_addr;
	struct wlan_objmgr_psoc *psoc = NULL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		mlo_err("Failed to get psoc");
		return QDF_STATUS_E_FAILURE;
	}

	status = mlo_dev_ctx_deinit(vdev);

	/* Detach DP vdev from DP MLO Device Context */
	mld_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);

	if (cdp_mlo_dev_ctxt_detach(wlan_psoc_get_dp_handle(psoc),
				    wlan_vdev_get_id(vdev),
				    (uint8_t *)mld_addr)
				    != QDF_STATUS_SUCCESS) {
		mlo_err("Failed to detach DP vdev (" QDF_MAC_ADDR_FMT ") from"
			" MLO Dev ctxt (" QDF_MAC_ADDR_FMT ")",
			QDF_MAC_ADDR_REF(wlan_vdev_mlme_get_macaddr(vdev)),
			QDF_MAC_ADDR_REF(mld_addr->bytes));
	}

	wlan_vdev_obj_lock(vdev);
	wlan_vdev_mlme_reset_mldaddr(vdev);
	wlan_vdev_mlme_op_flags_clear(vdev, WLAN_VDEV_OP_MLO_REMOVE_LINK_VDEV);
	wlan_vdev_obj_unlock(vdev);

	return status;
}

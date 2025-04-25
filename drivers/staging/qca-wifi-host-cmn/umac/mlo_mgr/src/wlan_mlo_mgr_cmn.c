/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains MLO manager ap related functionality
 */
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_mlo_mgr_main.h"
#include "wlan_mlo_mgr_sta.h"
#ifdef WLAN_MLO_MULTI_CHIP
#include "wlan_lmac_if_def.h"
#endif
#include "wlan_serialization_api.h"
#include <target_if_mlo_mgr.h>
#include <cdp_txrx_cmn.h>
#include <wlan_cfg.h>
#include "wlan_utility.h"

void mlo_get_link_information(struct qdf_mac_addr *mld_addr,
			      struct mlo_link_info *info)
{
/* Pass the partner link information*/
}

void is_mlo_all_links_up(struct wlan_mlo_dev_context *mldev)
{
/* Loop through all the vdev's part of the ML device*/
/* STA: Loop through all the associated vdev status. */
}

struct wlan_objmgr_vdev *mlo_get_vdev_by_link_id(struct wlan_objmgr_vdev *vdev,
						 uint8_t link_id,
						 wlan_objmgr_ref_dbgid id)
{
	struct wlan_mlo_dev_context *dev_ctx;
	int i;
	struct wlan_objmgr_vdev *partner_vdev = NULL;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Invalid input");
		return partner_vdev;
	}

	dev_ctx = vdev->mlo_dev_ctx;

	mlo_dev_lock_acquire(dev_ctx);
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (dev_ctx->wlan_vdev_list[i] &&
		    wlan_vdev_mlme_is_mlo_vdev(dev_ctx->wlan_vdev_list[i]) &&
		    dev_ctx->wlan_vdev_list[i]->vdev_mlme.mlo_link_id ==
		    link_id) {
			if (wlan_objmgr_vdev_try_get_ref(
						dev_ctx->wlan_vdev_list[i],
						id) == QDF_STATUS_SUCCESS)
				partner_vdev = dev_ctx->wlan_vdev_list[i];

			break;
		}
	}
	mlo_dev_lock_release(dev_ctx);

	return partner_vdev;
}

void mlo_release_vdev_ref(struct wlan_objmgr_vdev *vdev)
{
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
}

QDF_STATUS mlo_reg_mlme_ext_cb(struct mlo_mgr_context *ctx,
			       struct mlo_mlme_ext_ops *ops)
{
	if (!ctx)
		return QDF_STATUS_E_FAILURE;

	ctx->mlme_ops = ops;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlo_unreg_mlme_ext_cb(struct mlo_mgr_context *ctx)
{
	if (!ctx)
		return QDF_STATUS_E_FAILURE;

	ctx->mlme_ops = NULL;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
QDF_STATUS wlan_mlo_mgr_register_osif_ext_ops(struct mlo_mgr_context *mlo_ctx,
					      struct mlo_osif_ext_ops *ops)
{
	if (!ops || !mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	mlo_ctx->osif_ops = ops;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_mgr_unregister_osif_ext_ops(struct mlo_mgr_context *mlo_ctx)
{
	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	mlo_ctx->osif_ops = NULL;
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS mlo_mlme_clone_sta_security(struct wlan_objmgr_vdev *vdev,
				       struct wlan_cm_connect_req *req)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!req || !mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_validate_conn_req)
		return QDF_STATUS_E_FAILURE;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	if (mlo_ctx->mlme_ops->mlo_mlme_ext_clone_security_param) {
		status =
			mlo_ctx->mlme_ops->mlo_mlme_ext_clone_security_param(
				vdev_mlme, req);
	}

	return status;
}

QDF_STATUS mlo_mlme_sta_op_class(struct wlan_objmgr_vdev *vdev,
				 uint8_t *ml_ie)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_validate_conn_req)
		return QDF_STATUS_E_FAILURE;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	if (mlo_ctx->mlme_ops->mlo_mlme_ext_sta_op_class)
		status =
			mlo_ctx->mlme_ops->mlo_mlme_ext_sta_op_class(
				vdev_mlme, ml_ie);

	return status;
}

QDF_STATUS mlo_mlme_validate_conn_req(struct wlan_objmgr_vdev *vdev,
				      void *ext_data)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status;

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_validate_conn_req)
		return QDF_STATUS_E_FAILURE;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	status =
		mlo_ctx->mlme_ops->mlo_mlme_ext_validate_conn_req(vdev_mlme,
								  ext_data);
	return status;
}

QDF_STATUS mlo_mlme_create_link_vdev(struct wlan_objmgr_vdev *vdev,
				     void *ext_data)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status;

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_create_link_vdev)
		return QDF_STATUS_E_FAILURE;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	status =
		mlo_ctx->mlme_ops->mlo_mlme_ext_create_link_vdev(vdev_mlme,
								 ext_data);
	return status;
}

void mlo_mlme_peer_create(struct wlan_objmgr_vdev *vdev,
			  struct wlan_mlo_peer_context *ml_peer,
			  struct qdf_mac_addr *addr,
			  qdf_nbuf_t frm_buf)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_peer_create)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_peer_create(vdev, ml_peer,
						    addr, frm_buf);
}

void mlo_mlme_bridge_peer_create(struct wlan_objmgr_vdev *vdev,
				 struct wlan_mlo_peer_context *ml_peer,
				 struct qdf_mac_addr *addr,
				 qdf_nbuf_t frm_buf)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_bridge_peer_create)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_bridge_peer_create(vdev, ml_peer,
							   addr, frm_buf);
}

void mlo_mlme_peer_assoc(struct wlan_objmgr_peer *peer)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_peer_assoc)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_peer_assoc(peer);
}

void mlo_mlme_peer_assoc_fail(struct wlan_objmgr_peer *peer)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_peer_assoc_fail)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_peer_assoc_fail(peer);
}

void mlo_mlme_peer_delete(struct wlan_objmgr_peer *peer)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_peer_delete)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_peer_delete(peer);
}

void mlo_mlme_peer_assoc_resp(struct wlan_objmgr_peer *peer)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_assoc_resp)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_assoc_resp(peer);
}

qdf_nbuf_t mlo_mlme_get_link_assoc_req(struct wlan_objmgr_peer *peer,
				       uint8_t link_ix)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_get_link_assoc_req)
		return NULL;

	return mlo_ctx->mlme_ops->mlo_mlme_get_link_assoc_req(peer, link_ix);
}

void mlo_mlme_peer_deauth(struct wlan_objmgr_peer *peer, uint8_t is_disassoc)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_deauth)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_deauth(peer, is_disassoc);
}

#ifdef UMAC_MLO_AUTH_DEFER
void mlo_mlme_peer_process_auth(struct mlpeer_auth_params *auth_param)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_peer_process_auth)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_peer_process_auth(auth_param);
}
#endif

void mlo_mlme_peer_reassoc(struct wlan_objmgr_vdev *vdev,
			   struct wlan_mlo_peer_context *ml_peer,
			   struct qdf_mac_addr *addr,
			   qdf_nbuf_t frm_buf)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_peer_reassoc)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_peer_reassoc(vdev, ml_peer, addr,
						     frm_buf);
}

uint8_t mlo_get_link_vdev_ix(struct wlan_mlo_dev_context *ml_dev,
			     struct wlan_objmgr_vdev *vdev)
{
	uint8_t i;

	mlo_dev_lock_acquire(ml_dev);
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (vdev == ml_dev->wlan_vdev_list[i]) {
			mlo_dev_lock_release(ml_dev);
			return i;
		}
	}
	mlo_dev_lock_release(ml_dev);

	return (uint8_t)-1;
}

#ifdef WLAN_MLO_MULTI_CHIP
int8_t wlan_mlo_get_max_num_links(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx;

	mlo_ctx = wlan_objmgr_get_mlo_ctx();
	if (!mlo_ctx)
		return WLAN_MLO_INVALID_NUM_LINKS;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return WLAN_MLO_INVALID_NUM_LINKS;
	}

	return (mlo_ctx->setup_info[grp_id].tot_socs *
		WLAN_MAX_MLO_LINKS_PER_SOC);
}

int8_t wlan_mlo_get_num_active_links(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx;

	mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return WLAN_MLO_INVALID_NUM_LINKS;

	if (grp_id >= mlo_ctx->total_grp) {
		qdf_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return WLAN_MLO_INVALID_NUM_LINKS;
	}

	return mlo_ctx->setup_info[grp_id].tot_links;
}

uint16_t wlan_mlo_get_valid_link_bitmap(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx;

	mlo_ctx = wlan_objmgr_get_mlo_ctx();
	if (!mlo_ctx)
		return 0;

	if (grp_id >= mlo_ctx->total_grp) {
		qdf_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return 0;
	}

	return mlo_ctx->setup_info[grp_id].valid_link_bitmap;
}

uint8_t wlan_mlo_get_psoc_mlo_chip_id(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	uint8_t mlo_chip_id = WLAN_MLO_CHIP_ID_INVALID;

	if (!psoc) {
		qdf_err("PSOC is NULL");
		return mlo_chip_id;
	}

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (tx_ops && tx_ops->mops.get_psoc_mlo_chip_id)
		mlo_chip_id = tx_ops->mops.get_psoc_mlo_chip_id(psoc);

	return mlo_chip_id;
}

qdf_export_symbol(wlan_mlo_get_psoc_mlo_chip_id);

uint8_t wlan_mlo_get_psoc_group_id(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	uint8_t ml_grp_id = WLAN_MLO_GROUP_INVALID;

	if (!psoc) {
		qdf_err("PSOC is NULL");
		return -EINVAL;
	}

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (tx_ops && tx_ops->mops.get_psoc_mlo_group_id)
		ml_grp_id = tx_ops->mops.get_psoc_mlo_group_id(psoc);

	return ml_grp_id;
}

qdf_export_symbol(wlan_mlo_get_psoc_group_id);

bool wlan_mlo_get_psoc_capable(struct wlan_objmgr_psoc *psoc)
{
	struct target_psoc_info *tgt_hdl;

	if (!psoc) {
		qdf_err("PSOC is NULL");
		return false;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_hdl) {
		target_if_err("target_psoc_info is null");
		return false;
	}

	if ((tgt_hdl->tif_ops) &&
	    (tgt_hdl->tif_ops->mlo_capable))
		return tgt_hdl->tif_ops->mlo_capable(psoc);

	return false;
}

uint16_t wlan_mlo_get_pdev_hw_link_id(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_tx_ops *tx_ops;
	uint16_t hw_link_id = INVALID_HW_LINK_ID;

	psoc = wlan_pdev_get_psoc(pdev);
	if (psoc) {
		tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
		if (tx_ops && tx_ops->mops.get_hw_link_id)
			hw_link_id = tx_ops->mops.get_hw_link_id(pdev);
	}

	return hw_link_id;
}

qdf_export_symbol(wlan_mlo_get_pdev_hw_link_id);

static void wlan_pdev_hw_link_iterator(struct wlan_objmgr_psoc *psoc,
				       void *obj, void *arg)
{
	struct hw_link_id_iterator *itr = (struct hw_link_id_iterator *)arg;
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)obj;
	uint16_t hw_link_id;
	uint8_t ml_grp_id;

	if (itr->pdev)
		return;

	ml_grp_id = wlan_mlo_get_psoc_group_id(psoc);
	if (ml_grp_id > WLAN_MAX_MLO_GROUPS)
		return;

	if (ml_grp_id != itr->mlo_grp_id)
		return;

	hw_link_id = wlan_mlo_get_pdev_hw_link_id(pdev);
	if (hw_link_id == itr->hw_link_id) {
		if (wlan_objmgr_pdev_try_get_ref(pdev, itr->dbgid) ==
							QDF_STATUS_SUCCESS)
			itr->pdev = pdev;
	}
}

static void wlan_mlo_find_hw_link_id(struct wlan_objmgr_psoc *psoc,
				     void *arg,
				     uint8_t index)
{
	struct hw_link_id_iterator *itr = (struct hw_link_id_iterator *)arg;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
				     wlan_pdev_hw_link_iterator,
				     arg, false, itr->dbgid);
}

struct wlan_objmgr_pdev *
wlan_mlo_get_pdev_by_hw_link_id(uint16_t hw_link_id, uint8_t ml_grp_id,
				wlan_objmgr_ref_dbgid refdbgid)
{
	struct hw_link_id_iterator itr;

	itr.hw_link_id = hw_link_id;
	itr.pdev = NULL;
	itr.mlo_grp_id = ml_grp_id;
	itr.dbgid = refdbgid;

	wlan_objmgr_iterate_psoc_list(wlan_mlo_find_hw_link_id,
				      &itr, refdbgid);

	return itr.pdev;
}

qdf_export_symbol(wlan_mlo_get_pdev_by_hw_link_id);
#endif /*WLAN_MLO_MULTI_CHIP*/

void mlo_get_ml_vdev_list(struct wlan_objmgr_vdev *vdev,
			  uint16_t *vdev_count,
			  struct wlan_objmgr_vdev **wlan_vdev_list)
{
	struct wlan_mlo_dev_context *dev_ctx;
	int i;
	QDF_STATUS status;

	*vdev_count = 0;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Invalid input");
		return;
	}

	dev_ctx = vdev->mlo_dev_ctx;

	mlo_dev_lock_acquire(dev_ctx);
	*vdev_count = 0;
	for (i = 0; i < QDF_ARRAY_SIZE(dev_ctx->wlan_vdev_list); i++) {
		if (dev_ctx->wlan_vdev_list[i] &&
		    wlan_vdev_mlme_is_mlo_vdev(dev_ctx->wlan_vdev_list[i])) {
			status = wlan_objmgr_vdev_try_get_ref(
						dev_ctx->wlan_vdev_list[i],
						WLAN_MLO_MGR_ID);
			if (QDF_IS_STATUS_ERROR(status))
				break;
			wlan_vdev_list[*vdev_count] =
				dev_ctx->wlan_vdev_list[i];
			(*vdev_count) += 1;
		}
	}
	mlo_dev_lock_release(dev_ctx);
}

/**
 * mlo_link_set_active() - send MLO link set active command
 * @psoc: PSOC object
 * @req: MLO link set active request
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
mlo_link_set_active(struct wlan_objmgr_psoc *psoc,
		    struct mlo_link_set_active_req *req)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct mlo_link_set_active_param *param = &req->param;
	QDF_STATUS status;
	struct mlo_link_set_active_resp rsp_evt;

	if (!psoc) {
		mlo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = target_if_mlo_get_tx_ops(psoc);
	if (!mlo_tx_ops) {
		mlo_err("tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_tx_ops->link_set_active) {
		mlo_err("link_set_active function is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (req->ctx.validate_set_mlo_link_cb) {
		status = req->ctx.validate_set_mlo_link_cb(psoc, param);
		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mem_zero(&rsp_evt, sizeof(rsp_evt));
			rsp_evt.status = status;
			if (req->ctx.set_mlo_link_cb)
				req->ctx.set_mlo_link_cb(req->ctx.vdev,
							 req->ctx.cb_arg,
							 &rsp_evt);
			return status;
		}
	}

	return mlo_tx_ops->link_set_active(psoc, param);
}

/**
 * mlo_release_ser_link_set_active_cmd() - releases serialization command for
 *  forcing MLO link active/inactive
 * @vdev: Object manager vdev
 *
 * Return: None
 */
static void
mlo_release_ser_link_set_active_cmd(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_serialization_queued_cmd_info cmd = {0};

	cmd.cmd_type = WLAN_SER_CMD_SET_MLO_LINK;
	cmd.requestor = WLAN_UMAC_COMP_MLO_MGR;
	cmd.cmd_id = 0;
	cmd.vdev = vdev;

	mlo_debug("release serialization command");
	wlan_serialization_remove_cmd(&cmd);
}

/**
 * mlo_link_set_active_resp_vdev_handler() - vdev handler for mlo link set
 * active response event.
 * @psoc: psoc object
 * @obj: vdev object
 * @arg: mlo link set active response
 *
 * Return: None
 */
static void
mlo_link_set_active_resp_vdev_handler(struct wlan_objmgr_psoc *psoc,
				      void *obj, void *arg)
{
	struct mlo_link_set_active_req *req;
	struct wlan_objmgr_vdev *vdev = obj;
	struct mlo_link_set_active_resp *event = arg;

	if (event->evt_handled)
		return;
	req = wlan_serialization_get_active_cmd(wlan_vdev_get_psoc(vdev),
						wlan_vdev_get_id(vdev),
						WLAN_SER_CMD_SET_MLO_LINK);
	if (!req)
		return;

	if (req->ctx.set_mlo_link_cb)
		req->ctx.set_mlo_link_cb(vdev, req->ctx.cb_arg, event);

	mlo_release_ser_link_set_active_cmd(vdev);
	event->evt_handled = true;
}

QDF_STATUS
mlo_process_link_set_active_resp(struct wlan_objmgr_psoc *psoc,
				 struct mlo_link_set_active_resp *event)
{
	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     mlo_link_set_active_resp_vdev_handler,
				     event, true, WLAN_MLO_MGR_ID);
	if (!event->evt_handled)
		mlo_debug("link set resp evt not handled");

	return QDF_STATUS_SUCCESS;
}

/**
 * mlo_ser_set_link_cb() - Serialization callback function
 * @cmd: Serialization command info
 * @reason: Serialization reason for callback execution
 *
 * Return: Status of callback execution
 */
static QDF_STATUS
mlo_ser_set_link_cb(struct wlan_serialization_command *cmd,
		    enum wlan_serialization_cb_reason reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct mlo_link_set_active_req *req;
	struct mlo_mgr_context *mlo_ctx;

	if (!cmd || !cmd->vdev)
		return QDF_STATUS_E_FAILURE;

	mlo_ctx = wlan_objmgr_get_mlo_ctx();
	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(cmd->vdev);
	if (!psoc) {
		mlo_err("psoc is NULL, reason: %d", reason);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = cmd->umac_cmd;
	if (!req)
		return QDF_STATUS_E_INVAL;

	vdev = cmd->vdev;
	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		status = mlo_link_set_active(psoc, req);
		break;
	case WLAN_SER_CB_CANCEL_CMD:
	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		mlo_err("vdev %d command not execute: %d",
			wlan_vdev_get_id(vdev), reason);
		if (req->ctx.set_mlo_link_cb)
			req->ctx.set_mlo_link_cb(vdev, req->ctx.cb_arg, NULL);
		break;
	case WLAN_SER_CB_RELEASE_MEM_CMD:
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		qdf_mem_free(req);
		break;
	default:
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

#define MLO_SER_CMD_TIMEOUT_MS 5000
QDF_STATUS mlo_ser_set_link_req(struct mlo_link_set_active_req *req)
{
	struct wlan_serialization_command cmd = {0, };
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	if (!req)
		return QDF_STATUS_E_INVAL;

	vdev = req->ctx.vdev;
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLO_MGR_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("vdev %d unable to get reference",
			wlan_vdev_get_id(vdev));
		return status;
	}

	cmd.cmd_type = WLAN_SER_CMD_SET_MLO_LINK;
	cmd.cmd_id = 0;
	cmd.cmd_cb = mlo_ser_set_link_cb;
	cmd.source = WLAN_UMAC_COMP_MLO_MGR;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = MLO_SER_CMD_TIMEOUT_MS;
	cmd.vdev = vdev;
	cmd.is_blocking = true;
	cmd.umac_cmd = (void *)req;

	ser_cmd_status = wlan_serialization_request(&cmd);
	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list.Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	default:
		mlo_err("vdev %d ser cmd status %d",
			wlan_vdev_get_id(vdev), ser_cmd_status);
		status = QDF_STATUS_E_FAILURE;
	}

	if (QDF_IS_STATUS_SUCCESS(status))
		return status;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return status;
}

void mlo_mlme_handle_sta_csa_param(struct wlan_objmgr_vdev *vdev,
				   struct csa_offload_params *csa_param)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx || !mlo_ctx->mlme_ops ||
	    !mlo_ctx->mlme_ops->mlo_mlme_ext_handle_sta_csa_param)
		return;

	mlo_ctx->mlme_ops->mlo_mlme_ext_handle_sta_csa_param(vdev, csa_param);
}

QDF_STATUS
mlo_get_mlstats_vdev_params(struct wlan_objmgr_psoc *psoc,
			    struct mlo_stats_vdev_params *info,
			    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *ml_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	struct wlan_objmgr_vdev *vdev;
	int i;
	uint16_t ml_vdev_cnt = 0;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev) {
		mlo_err("vdev object is NULL for vdev %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	mlo_get_ml_vdev_list(vdev, &ml_vdev_cnt, ml_vdev_list);
	for (i = 0; i < ml_vdev_cnt; i++) {
		info->ml_vdev_id[i] = wlan_vdev_get_id(ml_vdev_list[i]);
		mlo_release_vdev_ref(ml_vdev_list[i]);
	}
	info->ml_vdev_count = ml_vdev_cnt;
	mlo_release_vdev_ref(vdev);

	return QDF_STATUS_SUCCESS;
}

static void ml_extract_link_state(struct wlan_objmgr_psoc *psoc,
				  struct ml_link_state_info_event *event)
{
	QDF_STATUS status;
	get_ml_link_state_cb resp_cb = NULL;
	void *context = NULL;
	uint8_t vdev_id;

	vdev_id = event->vdev_id;

	status = mlo_get_link_state_context(psoc,
					    &resp_cb, &context, vdev_id);

	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (resp_cb)
		resp_cb(event, context);
}

QDF_STATUS
wlan_handle_ml_link_state_info_event(struct wlan_objmgr_psoc *psoc,
				     struct ml_link_state_info_event *event)
{
	if (!event)
		return QDF_STATUS_E_NULL_VALUE;

	ml_extract_link_state(psoc, event);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ml_get_link_state_req_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct mlo_link_state_cmd_params cmd = {0};
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct wlan_objmgr_psoc *psoc;
	int status = 0;

	if (!vdev) {
		mlo_err("null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);

	if (!psoc) {
		mlo_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;

	if (!mlo_tx_ops) {
		mlo_err("tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlo_err("vdev is not MLO vdev");
		return status;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	cmd.vdev_id = vdev->vdev_objmgr.vdev_id;
	qdf_mem_copy(&cmd.mld_mac[0], &mlo_dev_ctx->mld_addr,
		     QDF_MAC_ADDR_SIZE);

	if (!mlo_tx_ops->request_link_state_info_cmd) {
		mlo_err("handler is not registered");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = mlo_tx_ops->request_link_state_info_cmd(psoc, &cmd);

	if (QDF_IS_STATUS_ERROR(status))
		mlo_err("failed to send ml link info command to FW");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
mlo_get_link_state_register_resp_cb(struct wlan_objmgr_vdev *vdev,
				    struct ml_link_state_cmd_info *req)
{
	struct wlan_mlo_dev_context *mlo_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!vdev || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return QDF_STATUS_E_NULL_VALUE;
	mlo_ctx = vdev->mlo_dev_ctx;

	if (!mlo_ctx) {
		mlo_err("null mlo_dev_ctx");
		return QDF_STATUS_E_NULL_VALUE;
	}

	sta_ctx = mlo_ctx->sta_ctx;

	if (!sta_ctx)
		return QDF_STATUS_E_INVAL;

	mlo_dev_lock_acquire(mlo_ctx);

	sta_ctx->ml_link_state.ml_link_state_resp_cb =
		req->ml_link_state_resp_cb;
	sta_ctx->ml_link_state.ml_link_state_req_context =
		req->request_cookie;
	mlo_dev_lock_release(mlo_ctx);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ml_get_link_state_req_flush_cb(struct scheduler_msg *msg)
{
	mlo_debug("ml_get_link_state_req flush callback");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ml_post_get_link_state_msg(struct wlan_objmgr_vdev *vdev)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS qdf_status = 0;

	msg.bodyptr = vdev;
	msg.callback = ml_get_link_state_req_cb;
	msg.flush_callback = ml_get_link_state_req_flush_cb;

	qdf_status = scheduler_post_message(
				QDF_MODULE_ID_OS_IF,
				QDF_MODULE_ID_MLME,
				QDF_MODULE_ID_OS_IF,
				&msg);
	return qdf_status;
}

bool
wlan_mlo_is_csa_allow(struct wlan_objmgr_vdev *vdev, uint16_t csa_freq)
{
	struct wlan_channel *chan;
	struct wlan_objmgr_vdev *ml_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	uint16_t ml_vdev_cnt = 0;
	struct wlan_objmgr_vdev *t_vdev;
	int i;
	bool is_allow = true;

	if (!vdev) {
		mlo_err("vdev is NULL");
		return false;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev))
		return true;

	mlo_get_ml_vdev_list(vdev, &ml_vdev_cnt, ml_vdev_list);
	for (i = 0; i < ml_vdev_cnt; i++) {
		t_vdev = ml_vdev_list[i];
		if (t_vdev == vdev)
			goto next;
		chan = wlan_vdev_get_active_channel(t_vdev);
		if (!chan)
			goto next;

		if (csa_freq == chan->ch_freq) {
			mlo_err("vdev %d will SCC with vdev %d on freq %d",
				wlan_vdev_get_id(vdev),
				wlan_vdev_get_id(t_vdev), csa_freq);
			is_allow = false;
		}
next:
		mlo_release_vdev_ref(t_vdev);
	}

	return is_allow;
}


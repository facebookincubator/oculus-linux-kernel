/*
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
/**
 * DOC: define internal APIs related to the mlme component, legacy APIs are
 *	called for the time being, but will be cleaned up after convergence
 */
#include "wifi_pos_api.h"
#include "wlan_wifi_pos_interface.h"
#include "wma_pasn_peer_api.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
/**
 * wlan_wifi_pos_pasn_peer_create() - Callback to create ranging peer
 * @psoc: Pointer to PSOC
 * @peer_addr: Address of the peer for which PASN peer is to be created
 * @vdev_id: Vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_wifi_pos_pasn_peer_create(struct wlan_objmgr_psoc *psoc,
						 struct qdf_mac_addr *peer_addr,
						 uint8_t vdev_id)
{
	return wma_pasn_peer_create(psoc, peer_addr, vdev_id);
}

/**
 * wlan_wifi_pos_pasn_peer_delete() - Callback to delete ranging peer
 * @psoc: Pointer to PSOC
 * @peer_addr: Address of the peer for which PASN peer is to be deleted
 * @vdev_id: Vdev id
 * @no_fw_peer_delete: if true do not seend peer delete to firmware
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_wifi_pos_pasn_peer_delete(struct wlan_objmgr_psoc *psoc,
						 struct qdf_mac_addr *peer_addr,
						 uint8_t vdev_id,
						 bool no_fw_peer_delete)
{
	return wma_pasn_peer_remove(psoc, peer_addr, vdev_id,
				    no_fw_peer_delete);
}

/**
 * wlan_wifi_pos_vdev_delete_resume() - Resume vdev delete operation
 * after deleting all pasn peers
 * @vdev: Pointer to objmgr vdev
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_wifi_pos_vdev_delete_resume(struct wlan_objmgr_vdev *vdev)
{
	return wma_pasn_peer_delete_all_complete(vdev);
}

bool
wlan_wifi_pos_pasn_peer_delete_all(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct pasn_peer_delete_msg *req;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_TGT_IF_ID);
	if (!vdev) {
		mlme_err("Vdev is not found for id:%d", vdev_id);
		return false;
	}

	if (!(vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE ||
	      vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_TGT_IF_ID);
		return false;
	}

	if (!wifi_pos_get_pasn_peer_count(vdev) ||
	    wifi_pos_is_delete_all_peer_in_progress(vdev)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_TGT_IF_ID);
		return false;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_TGT_IF_ID);

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return false;

	req->vdev_id = vdev_id;

	msg.type = WIFI_POS_PASN_PEER_DELETE_ALL;
	msg.bodyptr = req;

	status = scheduler_post_message(QDF_MODULE_ID_WIFIPOS,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(req);
		mlme_err("Delete all pasn peers failed");
		return false;
	}

	return true;
}

static struct wifi_pos_legacy_ops wifi_pos_ops = {
	.pasn_peer_create_cb = wlan_wifi_pos_pasn_peer_create,
	.pasn_peer_delete_cb = wlan_wifi_pos_pasn_peer_delete,
	.pasn_vdev_delete_resume_cb = wlan_wifi_pos_vdev_delete_resume,
};

QDF_STATUS
wifi_pos_register_legacy_ops(struct wlan_objmgr_psoc *psoc)
{
	return wifi_pos_set_legacy_ops(psoc, &wifi_pos_ops);
}

QDF_STATUS
wifi_pos_deregister_legacy_ops(struct wlan_objmgr_psoc *psoc)
{
	return wifi_pos_set_legacy_ops(psoc, NULL);
}
#endif

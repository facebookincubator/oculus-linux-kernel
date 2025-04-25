/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_cm_roam_logging.c
 *
 * Implementation for the connectivity and roam logging api.
 */
#include "wlan_connectivity_logging.h"
#include "wlan_cm_api.h"
#include "wlan_mlme_main.h"
#include "wlan_mlo_mgr_sta.h"
#include "wlan_mlme_api.h"
#include "cdp_txrx_ctrl.h"
#include "wlan_mlo_mgr_peer.h"
#include "wlan_scan_api.h"

#ifdef WLAN_FEATURE_CONNECTIVITY_LOGGING
static struct wlan_connectivity_log_buf_data global_cl;

static void
wlan_connectivity_logging_register_callbacks(
				struct wlan_cl_osif_cbks *osif_cbks,
				void *osif_cb_context)
{
	global_cl.osif_cbks.wlan_connectivity_log_send_to_usr =
			osif_cbks->wlan_connectivity_log_send_to_usr;
	global_cl.osif_cb_context = osif_cb_context;
}

void wlan_connectivity_logging_start(struct wlan_objmgr_psoc *psoc,
				     struct wlan_cl_osif_cbks *osif_cbks,
				     void *osif_cb_context)
{
	global_cl.head = qdf_mem_valloc(sizeof(*global_cl.head) *
					WLAN_MAX_LOG_RECORDS);
	if (!global_cl.head) {
		QDF_BUG(0);
		return;
	}

	global_cl.psoc = psoc;
	global_cl.write_idx = 0;
	global_cl.read_idx = 0;

	qdf_atomic_init(&global_cl.dropped_msgs);
	qdf_spinlock_create(&global_cl.write_ptr_lock);

	global_cl.read_ptr = global_cl.head;
	global_cl.write_ptr = global_cl.head;
	global_cl.max_records = WLAN_MAX_LOG_RECORDS;

	wlan_connectivity_logging_register_callbacks(osif_cbks,
						     osif_cb_context);
	qdf_atomic_set(&global_cl.is_active, 1);
}

void wlan_connectivity_logging_stop(void)
{
	if (!qdf_atomic_read(&global_cl.is_active))
		return;

	qdf_spin_lock_bh(&global_cl.write_ptr_lock);

	global_cl.psoc = NULL;
	global_cl.osif_cb_context = NULL;
	global_cl.osif_cbks.wlan_connectivity_log_send_to_usr = NULL;

	qdf_atomic_set(&global_cl.is_active, 0);
	global_cl.read_ptr = NULL;
	global_cl.write_ptr = NULL;
	global_cl.read_idx = 0;
	global_cl.write_idx = 0;

	qdf_mem_vfree(global_cl.head);
	global_cl.head = NULL;
	qdf_spin_unlock_bh(&global_cl.write_ptr_lock);
	qdf_spinlock_destroy(&global_cl.write_ptr_lock);
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && \
	defined(WLAN_FEATURE_11BE_MLO)
static void
wlan_clear_ml_vdev_sae_auth_logs(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_objmgr_vdev *link_vdev;
	struct mlme_legacy_priv *mlme_priv;
	uint8_t i, link_vdev_id;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		logging_err_rl("mlo_dev ctx is NULL for vdev:%d",
			       wlan_vdev_get_id(vdev));
		return;
	}

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		link_vdev_id =
			wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		link_vdev = mlo_dev_ctx->wlan_vdev_list[i];

		mlme_priv = wlan_vdev_mlme_get_ext_hdl(link_vdev);
		if (!mlme_priv) {
			logging_err_rl("vdev:%d legacy private object is NULL",
				       link_vdev_id);
			return;
		}

		logging_debug("vdev:%d clear sae auth logs cache",
			      link_vdev_id);
		qdf_mem_zero(mlme_priv->auth_log, sizeof(mlme_priv->auth_log));
	}
}
#else
static inline void
wlan_clear_ml_vdev_sae_auth_logs(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev)
{}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void wlan_clear_sae_auth_logs_cache(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_err_rl("Invalid vdev:%d", vdev_id);
		return;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		wlan_clear_ml_vdev_sae_auth_logs(psoc, vdev);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		logging_err_rl("vdev legacy private object is NULL");
		return;
	}

	logging_debug("vdev:%d clear sae auth logs cache", vdev_id);
	qdf_mem_zero(mlme_priv->auth_log, sizeof(mlme_priv->auth_log));

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(CONNECTIVITY_DIAG_EVENT)
QDF_STATUS wlan_print_cached_sae_auth_logs(struct wlan_objmgr_psoc *psoc,
					   struct qdf_mac_addr *bssid,
					   uint8_t vdev_id)
{
	uint8_t i, j;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_err_rl("Invalid vdev:%d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		logging_err_rl("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Get the index of matching bssid and queue all the records for
	 * that bssid
	 */
	for (i = 0; i < MAX_ROAM_CANDIDATE_AP; i++) {
		if (!mlme_priv->auth_log[i][0].diag_cmn.ktime_us)
			continue;

		if (qdf_is_macaddr_equal(bssid,
					 (struct qdf_mac_addr *)mlme_priv->auth_log[i][0].diag_cmn.bssid))
			break;
	}

	/*
	 * No matching bssid found in cached log records.
	 * So return from here.
	 */
	if (i >= MAX_ROAM_CANDIDATE_AP) {
		logging_debug("No cached SAE auth logs");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		return QDF_STATUS_E_FAILURE;
	}

	for (j = 0; j < WLAN_ROAM_MAX_CACHED_AUTH_FRAMES; j++) {
		if (!mlme_priv->auth_log[i][j].diag_cmn.ktime_us)
			continue;

		WLAN_HOST_DIAG_EVENT_REPORT(&mlme_priv->auth_log[i][j],
					    EVENT_WLAN_MGMT);
		qdf_mem_zero(&mlme_priv->auth_log[i][j],
			     sizeof(struct wlan_diag_packet_info));
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return QDF_STATUS_SUCCESS;
}

bool wlan_is_log_record_present_for_bssid(struct wlan_objmgr_psoc *psoc,
					  struct qdf_mac_addr *bssid,
					  uint8_t vdev_id)
{
	struct wlan_diag_packet_info *pkt_info;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	int i;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_err_rl("Invalid vdev:%d", vdev_id);
		return false;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		logging_err_rl("vdev legacy private object is NULL");
		return false;
	}

	for (i = 0; i < MAX_ROAM_CANDIDATE_AP; i++) {
		pkt_info = &mlme_priv->auth_log[i][0];
		if (!pkt_info->diag_cmn.ktime_us)
			continue;

		if (qdf_is_macaddr_equal(bssid,
					 (struct qdf_mac_addr *)pkt_info->diag_cmn.bssid)) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
			return true;
		}
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return false;
}

#ifdef WLAN_FEATURE_11BE_MLO
bool
wlan_is_sae_auth_log_present_for_bssid(struct wlan_objmgr_psoc *psoc,
				       struct qdf_mac_addr *bssid,
				       uint8_t *vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint8_t i, link_vdev_id;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, *vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_err_rl("Invalid vdev:%d", *vdev_id);
		return false;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		return wlan_is_log_record_present_for_bssid(psoc, bssid,
							    *vdev_id);
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	if (!mlo_dev_ctx) {
		logging_err_rl("mlo_dev ctx is NULL for vdev:%d", *vdev_id);
		return wlan_is_log_record_present_for_bssid(psoc, bssid,
							    *vdev_id);
	}

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		link_vdev_id = wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		if (wlan_is_log_record_present_for_bssid(psoc, bssid,
							 link_vdev_id)) {
			*vdev_id = link_vdev_id;
			return true;
		}
	}

	return false;
}
#else
bool
wlan_is_sae_auth_log_present_for_bssid(struct wlan_objmgr_psoc *psoc,
				       struct qdf_mac_addr *bssid,
				       uint8_t *vdev_id)
{
	return wlan_is_log_record_present_for_bssid(psoc, bssid, *vdev_id);
}
#endif

/**
 * wlan_add_sae_log_record_to_available_slot() - Add a new log record into the
 * cache for the queue.
 * @psoc: objmgr psoc object
 * @vdev: objmgr vdev object
 * @pkt_info: Log packet record pointer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_add_sae_log_record_to_available_slot(struct wlan_objmgr_psoc *psoc,
					  struct wlan_objmgr_vdev *vdev,
					  struct wlan_diag_packet_info *pkt_info)
{
	struct mlme_legacy_priv *mlme_priv;
	uint8_t i, j;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	bool is_entry_exist =
		wlan_is_log_record_present_for_bssid(psoc,
						     (struct qdf_mac_addr *)pkt_info->diag_cmn.bssid,
						     vdev_id);

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		logging_err_rl("vdev:%d legacy private object is NULL",
			       vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < MAX_ROAM_CANDIDATE_AP; i++) {
		if (is_entry_exist &&
		    mlme_priv->auth_log[i][0].diag_cmn.ktime_us &&
		    qdf_is_macaddr_equal((struct qdf_mac_addr *)pkt_info->diag_cmn.bssid,
					 (struct qdf_mac_addr *)mlme_priv->auth_log[i][0].diag_cmn.bssid)) {
			/*
			 * Frames for given bssid already exists store the new
			 * frame in corresponding array in empty slot
			 */
			for (j = 0; j < WLAN_ROAM_MAX_CACHED_AUTH_FRAMES; j++) {
				if (mlme_priv->auth_log[i][j].diag_cmn.ktime_us)
					continue;

				logging_debug("vdev:%d added at [i][j]:[%d][%d]",
					      vdev_id, i, j);
				mlme_priv->auth_log[i][j] = *pkt_info;
				break;
			}

		} else if (!is_entry_exist &&
			   !mlme_priv->auth_log[i][0].diag_cmn.ktime_us) {
			/*
			 * For given record, there is no existing bssid
			 * so add the entry at first available slot
			 */
			logging_debug("vdev:%d added entry at [i][j]:[%d][%d]",
				      vdev_id, i, 0);
			mlme_priv->auth_log[i][0] = *pkt_info;
			break;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static void
wlan_cache_connectivity_log(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			    struct wlan_diag_packet_info *pkt_info)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_err_rl("Invalid vdev:%d", vdev_id);
		return;
	}

	wlan_add_sae_log_record_to_available_slot(psoc, vdev, pkt_info);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
}
#endif

#define WLAN_SAE_AUTH_ALGO_NUMBER 3
#ifdef CONNECTIVITY_DIAG_EVENT

#ifdef WLAN_FEATURE_11BE_MLO
void
wlan_connectivity_t2lm_req_resp_event(struct wlan_objmgr_vdev *vdev,
				      uint8_t token,
				      enum wlan_t2lm_resp_frm_type t2lm_status,
				      enum qdf_dp_tx_rx_status tx_status,
				      qdf_freq_t freq,
				      bool is_rx, uint8_t subtype)
{
	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event,
				 struct wlan_diag_mlo_t2lm_req_resp);

	wlan_diag_event.diag_cmn.vdev_id = wlan_vdev_get_id(vdev);
	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();

	wlan_diag_event.version = DIAG_MLO_T2LM_REQ_RESP_VERSION;

	wlan_diag_event.token = token;
	wlan_diag_event.subtype = subtype;

	wlan_diag_event.status = t2lm_status;
	wlan_diag_event.tx_status = wlan_get_diag_tx_status(tx_status);
	wlan_diag_event.is_rx = is_rx;

	wlan_diag_event.band = wlan_convert_freq_to_diag_band(freq);

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event,
				    EVENT_WLAN_MLO_T2LM_REQ_RESP);
}

void
wlan_connectivity_mlo_reconfig_event(struct wlan_objmgr_vdev *vdev)
{
	struct mlo_link_info *link_info = NULL;
	struct wlan_channel *chan_info = NULL;
	uint8_t link_id;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event,
				 struct wlan_diag_mlo_reconfig);

	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.version = DIAG_MLO_RECONFIG_VERSION;

	link_id = wlan_vdev_get_link_id(vdev);
	wlan_diag_event.mlo_cmn_info.link_id = link_id;

	if (!vdev->mlo_dev_ctx)
		return;

	link_info = mlo_mgr_get_ap_link_by_link_id(vdev->mlo_dev_ctx, link_id);
	if (!link_info) {
		mlme_err("linl: %d Link info not found", link_id);
		return;
	}
	chan_info = link_info->link_chan_info;
	if (!chan_info) {
		mlme_err("link: %d Chan info not found", link_id);
		return;
	}

	wlan_diag_event.mlo_cmn_info.band =
			wlan_convert_freq_to_diag_band(chan_info->ch_freq);

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_MLO_RECONFIG);
}

static QDF_STATUS
wlan_populate_link_addr(struct wlan_objmgr_vdev *vdev,
			struct wlan_diag_sta_info *wlan_diag_event)
{
	uint i = 0;
	struct mlo_link_switch_context *link_ctx = vdev->mlo_dev_ctx->link_ctx;
	struct wlan_channel *link_chan_info;

	if (!link_ctx)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < WLAN_MAX_ML_BSS_LINKS; i++) {
		link_chan_info = link_ctx->links_info[i].link_chan_info;

		if (wlan_reg_is_24ghz_ch_freq(
		    (qdf_freq_t)link_chan_info->ch_freq)) {
			qdf_mem_copy(wlan_diag_event->mac_2g,
				     link_ctx->links_info[i].link_addr.bytes,
				     QDF_MAC_ADDR_SIZE);
		} else if (wlan_reg_is_5ghz_ch_freq(
			   (qdf_freq_t)link_chan_info->ch_freq)) {
			qdf_mem_copy(wlan_diag_event->mac_5g,
				     link_ctx->links_info[i].link_addr.bytes,
				     QDF_MAC_ADDR_SIZE);
		} else if (wlan_reg_is_6ghz_chan_freq(
			   link_chan_info->ch_freq)) {
			qdf_mem_copy(wlan_diag_event->mac_6g,
				     link_ctx->links_info[i].link_addr.bytes,
				     QDF_MAC_ADDR_SIZE);
		}
	}

	return QDF_STATUS_SUCCESS;
}

static uint8_t
wlan_populate_band_bitmap(struct mlo_link_switch_context *link_ctx)
{
	uint8_t i, band_bitmap = 0, band;
	struct wlan_channel *link_chan_info;

	for (i = 0; i < WLAN_MAX_ML_BSS_LINKS; i++) {
		link_chan_info = link_ctx->links_info[i].link_chan_info;

		band = wlan_reg_freq_to_band((qdf_freq_t)
					     link_chan_info->ch_freq);

		band_bitmap |= BIT(band);
	}

	return band_bitmap;
}

QDF_STATUS
wlan_populate_mlo_mgmt_event_param(struct wlan_objmgr_vdev *vdev,
				   struct wlan_diag_packet_info *data,
				   enum wlan_main_tag tag)
{
	struct mlo_link_switch_context *link_ctx;
	struct qdf_mac_addr peer_mac, peer_mld_mac;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mlo_is_mld_sta(vdev))
		return status;

	if (wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return QDF_STATUS_E_INVAL;

	status = wlan_vdev_get_bss_peer_mac(vdev, &peer_mac);
	if (QDF_IS_STATUS_ERROR(status)) {
		logging_err("vdev: %d bss peer not found",
			    wlan_vdev_get_id(vdev));
		return status;
	}

	qdf_mem_copy(data->diag_cmn.bssid, peer_mac.bytes, QDF_MAC_ADDR_SIZE);

	status = wlan_vdev_get_bss_peer_mld_mac(vdev, &peer_mld_mac);
	if (QDF_IS_STATUS_ERROR(status)) {
		logging_err("vdev: %d failed to get mld mac address of peer",
			    wlan_vdev_get_id(vdev));
		return status;
	}

	qdf_mem_copy(data->mld_addr, peer_mld_mac.bytes, QDF_MAC_ADDR_SIZE);

	if (tag != WLAN_ASSOC_REQ && tag != WLAN_REASSOC_REQ)
		return status;

	link_ctx = vdev->mlo_dev_ctx->link_ctx;
	if (!link_ctx) {
		logging_debug("vdev: %d link_ctx not found",
			      wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	data->supported_links = wlan_populate_band_bitmap(link_ctx);

	return status;
}

QDF_STATUS
wlan_populate_roam_mld_log_param(struct wlan_objmgr_vdev *vdev,
				 struct wlan_diag_packet_info *data,
				 enum wlan_main_tag tag)
{
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mlo_is_mld_sta(vdev))
		return status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_INVAL;

	status = wlan_scan_get_mld_addr_by_link_addr(
			pdev, (struct qdf_mac_addr *)data->diag_cmn.bssid,
			(struct qdf_mac_addr *)data->mld_addr);
	if (QDF_IS_STATUS_ERROR(status))
		logging_err_rl("vdev:%d Not able to fetch MLD addr for link addr: " QDF_MAC_ADDR_FMT,
			       wlan_vdev_get_id(vdev),
			       QDF_MAC_ADDR_REF(data->diag_cmn.bssid));

	return status;
}

enum wlan_diag_wifi_band
wlan_convert_freq_to_diag_band(uint16_t ch_freq)
{
	enum reg_wifi_band band;

	band = wlan_reg_freq_to_band((qdf_freq_t)ch_freq);

	switch (band) {
	case REG_BAND_2G:
		return WLAN_24GHZ_BAND;
	case REG_BAND_5G:
		return WLAN_5GHZ_BAND;
	case REG_BAND_6G:
		return WLAN_6GHZ_BAND;
	default:
		return WLAN_INVALID_BAND;
	}
}

#define REJECTED_LINK_STATUS 1

void
wlan_connectivity_mlo_setup_event(struct wlan_objmgr_vdev *vdev)
{
	uint i = 0;
	struct mlo_link_switch_context *link_ctx = NULL;
	struct wlan_channel *chan_info;
	uint8_t num_links = 0;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event,
				 struct wlan_diag_mlo_setup);

	if (!mlo_is_mld_sta(vdev))
		return;

	qdf_mem_zero(&wlan_diag_event, sizeof(struct wlan_diag_mlo_setup));

	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.version = DIAG_MLO_SETUP_VERSION_V2;

	if (!vdev->mlo_dev_ctx) {
		logging_err("vdev: %d MLO dev ctx not found",
			    wlan_vdev_get_id(vdev));
		return;
	}

	link_ctx = vdev->mlo_dev_ctx->link_ctx;
	if (!link_ctx) {
		logging_err("vdev: %d mlo link ctx not found",
			    wlan_vdev_get_id(vdev));
		return;
	}

	for (i = 0; i < WLAN_MAX_ML_BSS_LINKS; i++) {
		if (link_ctx->links_info[i].link_id == WLAN_INVALID_LINK_ID)
			continue;

		chan_info = link_ctx->links_info[i].link_chan_info;
		if (!chan_info) {
			logging_debug("link %d: chan_info not found",
				      link_ctx->links_info[i].link_id);
			continue;
		}

		wlan_diag_event.mlo_cmn_info[num_links].link_id =
				link_ctx->links_info[i].link_id;
		wlan_diag_event.mlo_cmn_info[num_links].vdev_id =
				link_ctx->links_info[i].vdev_id;

		qdf_mem_copy(wlan_diag_event.mlo_cmn_info[num_links].link_addr,
			     link_ctx->links_info[i].ap_link_addr.bytes,
			     QDF_MAC_ADDR_SIZE);

		wlan_diag_event.mlo_cmn_info[num_links].band =
			wlan_convert_freq_to_diag_band(chan_info->ch_freq);

		if (wlan_diag_event.mlo_cmn_info[num_links].band ==
							 WLAN_INVALID_BAND)
			wlan_diag_event.mlo_cmn_info[i].status =
							REJECTED_LINK_STATUS;

		num_links++;
	}

	wlan_diag_event.num_links = num_links;

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_MLO_SETUP);
}

#define IS_LINK_SET(link_bitmap, link_id) ((link_bitmap) & (BIT(link_id)))
#define DEFAULT_TID_MAP 0xFF

static void
wlan_populate_tid_link_id_bitmap(struct wlan_t2lm_info *t2lm,
				 struct mlo_link_info *link_info,
				 struct wlan_diag_mlo_t2lm_status *buf,
				 uint8_t bss_link)
{
	uint8_t link_id;
	uint8_t dir, i;
	uint16_t freq;

	link_id = link_info->link_id;
	freq = link_info->link_chan_info->ch_freq;
	buf->mlo_cmn_info[bss_link].band =
		wlan_convert_freq_to_diag_band(freq);
	buf->mlo_cmn_info[bss_link].vdev_id = link_info->vdev_id;

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (t2lm[dir].default_link_mapping) {
			buf->mlo_cmn_info[bss_link].tid_ul = DEFAULT_TID_MAP;
			buf->mlo_cmn_info[bss_link].tid_dl = DEFAULT_TID_MAP;
			continue;
		}

		for (i = 0; i < T2LM_MAX_NUM_TIDS; i++) {
			switch (t2lm[dir].direction) {
			case WLAN_T2LM_DL_DIRECTION:
				if (
				IS_LINK_SET(
				t2lm[dir].ieee_link_map_tid[i], link_id))
					buf->mlo_cmn_info[bss_link].tid_dl |=
									BIT(i);
				break;
			case WLAN_T2LM_UL_DIRECTION:
				if (
				IS_LINK_SET(
				t2lm[dir].ieee_link_map_tid[i], link_id))
					buf->mlo_cmn_info[bss_link].tid_ul |=
									BIT(i);
				break;
			case WLAN_T2LM_BIDI_DIRECTION:
				if (
				IS_LINK_SET(
				t2lm[dir].ieee_link_map_tid[i], link_id)) {
					buf->mlo_cmn_info[bss_link].tid_dl |=
									BIT(i);
					buf->mlo_cmn_info[bss_link].tid_ul |=
									BIT(i);
				}
				break;
			default:
				logging_debug("Invalid direction %d",
					      t2lm[dir].direction);
				break;
			}
		}
	}
}

void
wlan_connectivity_t2lm_status_event(struct wlan_objmgr_vdev *vdev)
{
	uint8_t i = 0, dir, num_links = 0;
	QDF_STATUS status;
	struct mlo_link_info *link_info;
	struct wlan_t2lm_info t2lm[WLAN_T2LM_MAX_DIRECTION] = {0};

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event,
				 struct wlan_diag_mlo_t2lm_status);

	if (!mlo_is_mld_sta(vdev))
		return;

	if (!vdev->mlo_dev_ctx) {
		logging_err("MLO dev ctx not found");
		return;
	}
	link_info = mlo_mgr_get_ap_link(vdev);
	if (!link_info) {
		logging_err("link_info invalid");
		return;
	}

	if (mlo_mgr_is_link_switch_in_progress(vdev))
		return;

	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.version = DIAG_MLO_T2LM_STATUS_VERSION_V2;

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++)
		t2lm[dir].direction = WLAN_T2LM_INVALID_DIRECTION;

	status = wlan_get_t2lm_mapping_status(vdev, t2lm);
	if (QDF_IS_STATUS_ERROR(status)) {
		logging_err("Unable to get t2lm_mapping");
		return;
	}

	for (i = 0;
	     i < WLAN_MAX_ML_BSS_LINKS && i < MAX_NUM_LINKS_PER_EVENT; i++) {
		if (qdf_is_macaddr_zero(&link_info->ap_link_addr) &&
		    link_info->link_id == WLAN_INVALID_LINK_ID)
			continue;

		wlan_populate_tid_link_id_bitmap(t2lm, link_info,
						 &wlan_diag_event, num_links);
		link_info++;
		num_links++;
	}

	wlan_diag_event.num_links = num_links;

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event,
				    EVENT_WLAN_MLO_T2LM_STATUS);
}

#else
static QDF_STATUS
wlan_populate_link_addr(struct wlan_objmgr_vdev *vdev,
			struct wlan_diag_sta_info *wlan_diag_event)
{
	return QDF_STATUS_SUCCESS;
}
#endif

void
wlan_cdp_set_peer_freq(struct wlan_objmgr_psoc *psoc, uint8_t *peer_mac,
		       uint32_t freq, uint8_t vdev_id)
{
	ol_txrx_soc_handle soc_txrx_handle;
	cdp_config_param_type val = {0};

	soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

	val.cdp_peer_param_freq = freq;
	cdp_txrx_set_peer_param(soc_txrx_handle, vdev_id, peer_mac,
				CDP_CONFIG_PEER_FREQ, val);
}

void
wlan_connectivity_sta_info_event(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 bool is_roam)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev = NULL;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event, struct wlan_diag_sta_info);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_err_rl("Invalid vdev:%d", vdev_id);
		return;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE ||
	    (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	     (wlan_vdev_mlme_is_mlo_link_switch_in_progress(vdev) ||
	      wlan_vdev_mlme_is_mlo_link_vdev(vdev))))
		goto out;

	if (!is_roam && !wlan_cm_is_first_candidate_connect_attempt(vdev))
		goto out;

	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.diag_cmn.vdev_id = vdev_id;
	wlan_diag_event.is_mlo = wlan_vdev_mlme_is_mlo_vdev(vdev);

	if (wlan_diag_event.is_mlo) {
		qdf_mem_copy(wlan_diag_event.diag_cmn.bssid,
			     wlan_vdev_mlme_get_mldaddr(vdev),
			     QDF_MAC_ADDR_SIZE);
		status = wlan_populate_link_addr(vdev, &wlan_diag_event);
		if (QDF_IS_STATUS_ERROR(status)) {
			logging_err_rl("wlan_populate_link_addr failed");
			goto out;
		}
	} else {
		qdf_mem_copy(wlan_diag_event.diag_cmn.bssid,
			     wlan_vdev_mlme_get_macaddr(vdev),
			     QDF_MAC_ADDR_SIZE);
	}

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_STA_INFO);
out:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
}

void
wlan_populate_vsie(struct wlan_objmgr_vdev *vdev,
		   struct wlan_diag_packet_info *data,
		   bool is_tx)
{
	struct element_info *vsie_info = NULL;

	if (is_tx)
		vsie_info = mlme_get_self_disconnect_ies(vdev);
	else
		vsie_info = mlme_get_peer_disconnect_ies(vdev);

	if (!vsie_info)
		return;

	data->vsie_len = vsie_info->len;
	if (data->vsie_len > MAX_VSIE_LEN)
		data->vsie_len = MAX_VSIE_LEN;

	qdf_mem_copy(data->vsie, vsie_info->ptr, data->vsie_len);
}

void
wlan_connectivity_mgmt_event(struct wlan_objmgr_psoc *psoc,
			     struct wlan_frame_hdr *mac_hdr,
			     uint8_t vdev_id, uint16_t status_code,
			     enum qdf_dp_tx_rx_status tx_status,
			     int8_t peer_rssi,
			     uint8_t auth_algo, uint8_t auth_type,
			     uint8_t auth_seq, uint16_t aid,
			     enum wlan_main_tag tag)
{
	enum QDF_OPMODE opmode;
	bool cache_sae_frame_cap, is_initial_connection;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event, struct wlan_diag_packet_info);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		logging_debug("Unable to find vdev:%d", vdev_id);
		return;
	}

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	if (opmode != QDF_STA_MODE)
		goto out;

	is_initial_connection = wlan_cm_is_vdev_connecting(vdev);
	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    (wlan_vdev_mlme_is_mlo_link_switch_in_progress(vdev) ||
	     (is_initial_connection &&
	      wlan_vdev_mlme_is_mlo_link_vdev(vdev)))) {
		logging_debug("vdev:%d is_connection:%d | %s skip mgmt event",
			      vdev_id, is_initial_connection,
			      wlan_vdev_mlme_is_mlo_link_vdev(vdev) ?
			      "link_vdev" : wlan_vdev_mlme_is_mlo_link_switch_in_progress(vdev) ? "link switch in prog" : "");
		goto out;
	}

	qdf_mem_zero(&wlan_diag_event, sizeof(struct wlan_diag_packet_info));

	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.diag_cmn.vdev_id = vdev_id;
	wlan_diag_event.subtype = (uint8_t)tag;

	qdf_mem_copy(wlan_diag_event.diag_cmn.bssid, &mac_hdr->i_addr3[0],
		     QDF_MAC_ADDR_SIZE);

	if (is_initial_connection) {
		status = wlan_populate_mlo_mgmt_event_param(vdev,
							    &wlan_diag_event,
							    tag);
		if (QDF_IS_STATUS_ERROR(status))
			goto out;
	}

	wlan_diag_event.version = DIAG_MGMT_VERSION_V2;
	wlan_diag_event.tx_fail_reason = tx_status;
	wlan_diag_event.tx_status = wlan_get_diag_tx_status(tx_status);
	wlan_diag_event.rssi = peer_rssi;
	wlan_diag_event.sn =
		(le16toh(*(uint16_t *)mac_hdr->i_seq) >> WLAN_SEQ_SEQ_SHIFT);
	wlan_diag_event.status = status_code;
	wlan_diag_event.auth_algo = auth_algo;
	wlan_diag_event.auth_frame_type = auth_type;
	wlan_diag_event.auth_seq_num = auth_seq;
	wlan_diag_event.assoc_id = aid;

	if (tag == WLAN_DEAUTH_RX || tag == WLAN_DISASSOC_RX)
		wlan_populate_vsie(vdev, &wlan_diag_event, false);

	if (wlan_diag_event.subtype > WLAN_CONN_DIAG_REASSOC_RESP_EVENT &&
	    wlan_diag_event.subtype < WLAN_CONN_DIAG_BMISS_EVENT)
		wlan_diag_event.reason = status_code;

	wlan_diag_event.is_retry_frame =
			(mac_hdr->i_fc[1] & IEEE80211_FC1_RETRY);

	/*
	 * Cache the SAE auth frames info in vdev mlme private and return in
	 * case of roam preauth
	 */
	cache_sae_frame_cap =
		wlan_psoc_nif_fw_ext2_cap_get(psoc,
					      WLAN_ROAM_STATS_FRAME_INFO_PER_CANDIDATE);
	if (!is_initial_connection &&
	    (tag == WLAN_AUTH_REQ || tag == WLAN_AUTH_RESP) &&
	    auth_algo == WLAN_SAE_AUTH_ALGO_NUMBER && cache_sae_frame_cap) {
		wlan_cache_connectivity_log(psoc, vdev_id, &wlan_diag_event);
		goto out;
	}

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_MGMT);

	if (tag == WLAN_ASSOC_RSP || tag == WLAN_REASSOC_RSP)
		wlan_connectivity_mlo_setup_event(vdev);

out:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
}

void
wlan_connectivity_connecting_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_connect_req *con_req)
{
	QDF_STATUS status;
	struct wlan_cm_connect_req req;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event, struct wlan_diag_connect);

	if (!wlan_cm_is_first_candidate_connect_attempt(vdev))
		return;

	/* for candidate not found case*/
	if (con_req) {
		req = *con_req;
	} else {
		status = wlan_cm_get_active_connect_req_param(vdev, &req);
		if (QDF_IS_STATUS_ERROR(status)) {
			logging_err("vdev: %d failed to get active cmd request",
				    wlan_vdev_get_id(vdev));
			return;
		}
	}

	wlan_diag_event.version = DIAG_CONN_VERSION;
	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.diag_cmn.vdev_id = wlan_vdev_get_id(vdev);
	wlan_diag_event.subtype = WLAN_CONN_DIAG_CONNECTING_EVENT;

	wlan_diag_event.ssid_len = req.ssid.length;

	if (req.ssid.length > WLAN_SSID_MAX_LEN)
		wlan_diag_event.ssid_len = WLAN_SSID_MAX_LEN;

	qdf_mem_copy(wlan_diag_event.ssid, req.ssid.ssid,
		     wlan_diag_event.ssid_len);

	if (!qdf_is_macaddr_zero(&req.bssid))
		qdf_mem_copy(wlan_diag_event.diag_cmn.bssid, req.bssid.bytes,
			     QDF_MAC_ADDR_SIZE);
	else if (!qdf_is_macaddr_zero(&req.bssid_hint))
		qdf_mem_copy(wlan_diag_event.bssid_hint, req.bssid_hint.bytes,
			     QDF_MAC_ADDR_SIZE);

	if (req.chan_freq)
		wlan_diag_event.freq = req.chan_freq;
	else if (req.chan_freq_hint)
		wlan_diag_event.freq_hint = req.chan_freq_hint;

	wlan_diag_event.pairwise_cipher	= req.crypto.user_cipher_pairwise;
	wlan_diag_event.grp_cipher = req.crypto.user_grp_cipher;
	wlan_diag_event.akm = req.crypto.user_akm_suite;
	wlan_diag_event.auth_algo = req.crypto.user_auth_type;

	wlan_diag_event.bt_coex =
		wlan_mlme_get_bt_profile_con(wlan_vdev_get_psoc(vdev));

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_CONN);
}

#ifdef WLAN_FEATURE_11BE_MLO

#define BAND_TO_BITMAP(band) (band - 1)

static uint8_t
wlan_convert_link_id_to_diag_band(struct qdf_mac_addr *peer_mld,
				  uint16_t link_bitmap)
{
	uint8_t i, band_bitmap = 0, band;
	struct wlan_mlo_dev_context *mldev = NULL;
	struct wlan_mlo_peer_context *mlpeer = NULL;
	struct mlo_link_info *link_info = NULL;
	uint32_t freq;

	mlpeer = wlan_mlo_get_mlpeer_by_peer_mladdr(peer_mld, &mldev);
	if (!mlpeer) {
		logging_err("ml peer not found");
		return 0;
	}

	for (i = 0; i < MAX_MLO_LINK_ID; i++) {
		if (IS_LINK_SET(link_bitmap, i)) {
			link_info = mlo_mgr_get_ap_link_by_link_id(mldev, i);
			if (!link_info) {
				logging_err("link: %d info does not exist", i);
				return 0;
			}

			freq = link_info->link_chan_info->ch_freq;
			band = wlan_convert_freq_to_diag_band(freq);
			if (band == WLAN_INVALID_BAND)
				continue;

			band_bitmap |= BIT(BAND_TO_BITMAP(band));
		}
	}

	return band_bitmap;
}

static uint8_t
wlan_get_supported_link_band_bitmap(struct mlo_link_switch_context *link_ctx)
{
	uint8_t band_bitmap = 0, i = 0;
	struct mlo_link_info link_info;
	struct wlan_channel *chan_info;
	enum wlan_diag_wifi_band band;

	for (i = 0; i < WLAN_MAX_ML_BSS_LINKS; i++) {
		link_info = link_ctx->links_info[i];

		chan_info = link_info.link_chan_info;
		if (!chan_info)
			continue;

		band = wlan_convert_freq_to_diag_band(chan_info->ch_freq);
		if (band == WLAN_INVALID_BAND)
			continue;

		band_bitmap |= BIT(band - 1);
	}

	return band_bitmap;
}

void wlan_connectivity_mld_link_status_event(struct wlan_objmgr_psoc *psoc,
					     struct mlo_link_switch_params *src)
{
	struct wlan_mlo_peer_context *ml_peer = NULL;
	struct wlan_mlo_dev_context *mld_ctx = NULL;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event,
				 struct wlan_diag_mlo_link_status);

	qdf_mem_zero(&wlan_diag_event,
		     sizeof(struct wlan_diag_mlo_link_status));

	ml_peer = wlan_mlo_get_mlpeer_by_peer_mladdr(&src->mld_addr, &mld_ctx);

	if (!mld_ctx) {
		logging_err("mlo dev ctx for mld_mac: "
			    QDF_MAC_ADDR_FMT
			    " not found",
			    QDF_MAC_ADDR_REF(src->mld_addr.bytes));
		return;
	}

	wlan_diag_event.diag_cmn.timestamp_us = qdf_get_time_of_the_day_us();
	wlan_diag_event.diag_cmn.ktime_us = qdf_ktime_to_us(qdf_ktime_get());
	wlan_diag_event.version = DIAG_MLO_LINK_STATUS_VERSION_2;

	wlan_diag_event.active_link =
		wlan_convert_link_id_to_diag_band(&src->mld_addr,
						  src->active_link_bitmap);
	if (!wlan_diag_event.active_link)
		return;
	wlan_diag_event.prev_active_link =
		wlan_convert_link_id_to_diag_band(&src->mld_addr,
						  src->prev_link_bitmap);
	if (!wlan_diag_event.prev_active_link)
		return;

	if (!mld_ctx->link_ctx) {
		logging_err("link ctx for mld_mac: "
			    QDF_MAC_ADDR_FMT
			    " not found",
			    QDF_MAC_ADDR_REF(src->mld_addr.bytes));
		return;
	}

	wlan_diag_event.associated_links =
			wlan_get_supported_link_band_bitmap(mld_ctx->link_ctx);

	if (!wlan_diag_event.associated_links)
		return;

	wlan_diag_event.reason = src->reason_code;
	/*
	 * FW timestamp received from FW in milliseconds and to be sent to
	 * userspace in microseconds
	 */
	wlan_diag_event.diag_cmn.fw_timestamp = src->fw_timestamp * 1000;

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event,
				    EVENT_WLAN_MLO_LINK_STATUS);
}
#endif
#endif

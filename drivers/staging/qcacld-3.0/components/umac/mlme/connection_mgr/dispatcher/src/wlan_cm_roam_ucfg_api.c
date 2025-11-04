/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_cm_roam_ucfg_api.c
 *
 * Implementation for roaming ucfg public functionality.
 */

#include "wlan_mlme_ucfg_api.h"
#include "wlan_cm_roam_ucfg_api.h"
#include "../../core/src/wlan_cm_roam_offload.h"
#include "wlan_reg_ucfg_api.h"
#include "wlan_mlo_mgr_sta.h"

bool ucfg_is_rso_enabled(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	return wlan_is_rso_enabled(pdev, vdev_id);
}

QDF_STATUS
ucfg_user_space_enable_disable_rso(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id,
				   const bool is_fast_roam_enabled)
{
	bool supplicant_disabled_roaming;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;
	bool lfr_enabled;
	enum roam_offload_state state;
	uint32_t set_val = 0;
	enum roam_offload_state  cur_state;

	/*
	 * If the ini "FastRoamEnabled" is disabled, don't allow the
	 * userspace to enable roam offload
	 */
	ucfg_mlme_is_lfr_enabled(psoc, &lfr_enabled);
	if (!lfr_enabled) {
		mlme_debug("ROAM_CONFIG: Fast roam ini is disabled. is_fast_roam_enabled %d",
			   is_fast_roam_enabled);
		if (!is_fast_roam_enabled)
			return QDF_STATUS_SUCCESS;

		return  QDF_STATUS_E_FAILURE;
	}

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	if (cur_state == WLAN_ROAM_INIT) {
		if (!is_fast_roam_enabled)
			set_val =
			WMI_VDEV_ROAM_11KV_CTRL_DISABLE_FW_TRIGGER_ROAMING;

		status = cm_roam_send_disable_config(psoc, vdev_id, set_val);
		if (!QDF_IS_STATUS_SUCCESS(status))
			mlme_err("ROAM: update fast roaming failed, status: %d",
				 status);
	}
	wlan_mlme_set_usr_disabled_roaming(psoc, !is_fast_roam_enabled);

	/*
	 * Supplicant_disabled_roaming flag is the global flag to control
	 * roam offload from supplicant. Driver cannot enable roaming if
	 * supplicant disabled roaming is set.
	 * is_fast_roam_enabled: true - enable RSO if not disabled by driver
	 *                       false - Disable RSO. Send RSO stop if false
	 *                       is set.
	 */
	supplicant_disabled_roaming =
		mlme_get_supplicant_disabled_roaming(psoc, vdev_id);
	if (!is_fast_roam_enabled && supplicant_disabled_roaming) {
		mlme_debug("ROAM_CONFIG: RSO already disabled by supplicant");
		return QDF_STATUS_E_ALREADY;
	}

	mlme_set_supplicant_disabled_roaming(psoc, vdev_id,
					     !is_fast_roam_enabled);

	/* For mlo connection, before all links are up
	 * supplicant can enable roaming, to handle this
	 * drop the enable rso as host will enable roaming
	 * once all links are up
	 */
	if (mlo_is_ml_connection_in_progress(psoc, vdev_id)) {
		mlme_debug("mlo connection in progress");
		return QDF_STATUS_SUCCESS;
	}

	state = (is_fast_roam_enabled) ?
		WLAN_ROAM_RSO_ENABLED : WLAN_ROAM_RSO_STOPPED;
	status = cm_roam_state_change(pdev, vdev_id, state,
				      REASON_SUPPLICANT_DISABLED_ROAMING,
				      NULL, false);

	return status;
}

QDF_STATUS ucfg_cm_abort_roam_scan(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	bool roam_scan_offload_enabled;

	ucfg_mlme_is_roam_scan_offload_enabled(psoc,
					       &roam_scan_offload_enabled);
	if (!roam_scan_offload_enabled)
		return QDF_STATUS_SUCCESS;

	status = cm_roam_send_rso_cmd(psoc, vdev_id,
				      ROAM_SCAN_OFFLOAD_ABORT_SCAN,
				      REASON_ROAM_ABORT_ROAM_SCAN);

	return status;
}

QDF_STATUS ucfg_cm_get_roam_band(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint32_t *roam_band)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id, ROAM_BAND, &temp);

	*roam_band = temp.uint_value;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
QDF_STATUS ucfg_cm_set_ese_roam_scan_channel_list(struct wlan_objmgr_pdev *pdev,
						  uint8_t vdev_id,
						  qdf_freq_t *chan_freq_list,
						  uint8_t num_chan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum band_info band = -1;
	uint32_t band_bitmap;
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}
	status = cm_roam_acquire_lock(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_ref;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		status = QDF_STATUS_E_FAILURE;
		goto error;
	}

	mlme_debug("Chan list Before");
	cm_dump_freq_list(&rso_cfg->roam_scan_freq_lst);
	ucfg_reg_get_band(pdev, &band_bitmap);
	band = wlan_reg_band_bitmap_to_band_info(band_bitmap);
	status = cm_create_roam_scan_channel_list(pdev, rso_cfg, num_chan,
						  chan_freq_list, num_chan,
						  band);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mlme_debug("Chan list After");
		cm_dump_freq_list(&rso_cfg->roam_scan_freq_lst);
	}

	if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
		wlan_roam_update_cfg(psoc, vdev_id,
				     REASON_CHANNEL_LIST_CHANGED);


error:
	cm_roam_release_lock(vdev);
release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return status;
}

QDF_STATUS ucfg_cm_set_cckm_ie(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			       const uint8_t *cck_ie, const uint8_t cck_ie_len)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_CM_ID);

	if (!vdev) {
		mlme_err("vdev not found for id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(mlme_priv->connect_info.cckm_ie, cck_ie, cck_ie_len);
	mlme_priv->connect_info.cckm_ie_len = cck_ie_len;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS
ucfg_cm_update_fils_config(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id,
			   struct wlan_fils_con_info *fils_info)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = wlan_cm_update_mlme_fils_info(vdev, fils_info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}
#endif

QDF_STATUS
ucfg_wlan_cm_roam_invoke(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			 struct qdf_mac_addr *bssid, qdf_freq_t ch_freq,
			 enum wlan_cm_source source)
{
	return wlan_cm_roam_invoke(pdev, vdev_id, bssid, ch_freq, source);
}

#ifdef WLAN_FEATURE_HOST_ROAM
void ucfg_cm_set_ft_pre_auth_state(struct wlan_objmgr_vdev *vdev, bool state)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	mlme_priv->connect_info.ft_info.set_ft_preauth_state = state;
}

static bool ucfg_cm_get_ft_pre_auth_state(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return false;

	return mlme_priv->connect_info.ft_info.set_ft_preauth_state;
}

void ucfg_cm_set_ft_ies(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			const uint8_t *ft_ies, uint16_t ft_ies_length)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	if (!ft_ies) {
		mlme_err("ft ies is NULL");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		goto end;

	status = cm_roam_acquire_lock(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto end;

	mlme_debug("FT IEs Req is received in state %d",
		  mlme_priv->connect_info.ft_info.ft_state);

	/* Global Station FT State */
	switch (mlme_priv->connect_info.ft_info.ft_state) {
	case FT_START_READY:
	case FT_AUTH_REQ_READY:
		mlme_debug("ft_ies_length: %d", ft_ies_length);
		ft_ies_length = QDF_MIN(ft_ies_length, MAX_FTIE_SIZE);
		mlme_priv->connect_info.ft_info.auth_ie_len = ft_ies_length;
		qdf_mem_copy(mlme_priv->connect_info.ft_info.auth_ft_ie,
			     ft_ies, ft_ies_length);
		mlme_priv->connect_info.ft_info.ft_state = FT_AUTH_REQ_READY;
		break;

	case FT_REASSOC_REQ_WAIT:
		/*
		 * We are done with pre-auth, hence now waiting for
		 * reassoc req. This is the new FT Roaming in place At
		 * this juncture we'r ready to start sending Reassoc req
		 */

		ft_ies_length = QDF_MIN(ft_ies_length, MAX_FTIE_SIZE);

		mlme_debug("New Reassoc Req: %pK in state %d",
			   ft_ies, mlme_priv->connect_info.ft_info.ft_state);
		mlme_priv->connect_info.ft_info.reassoc_ie_len =
							ft_ies_length;
		qdf_mem_copy(mlme_priv->connect_info.ft_info.reassoc_ft_ie,
				ft_ies, ft_ies_length);

		mlme_priv->connect_info.ft_info.ft_state = FT_SET_KEY_WAIT;
		mlme_debug("ft_ies_length: %d state: %d", ft_ies_length,
			   mlme_priv->connect_info.ft_info.ft_state);
		break;

	default:
		mlme_warn("Unhandled state: %d",
			  mlme_priv->connect_info.ft_info.ft_state);
		break;
	}
	cm_roam_release_lock(vdev);
end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

QDF_STATUS ucfg_cm_check_ft_status(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return status;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		goto end;

	status = cm_roam_acquire_lock(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto end;

	mlme_debug("FT update key is received in state %d",
		   mlme_priv->connect_info.ft_info.ft_state);

	/* Global Station FT State */
	switch (mlme_priv->connect_info.ft_info.ft_state) {
	case FT_SET_KEY_WAIT:
		if (ucfg_cm_get_ft_pre_auth_state(vdev)) {
			mlme_priv->connect_info.ft_info.ft_state = FT_START_READY;
			mlme_debug("state changed to %d",
				   mlme_priv->connect_info.ft_info.ft_state);
			break;
		}
		fallthrough;
	default:
		mlme_debug("Unhandled state:%d",
			   mlme_priv->connect_info.ft_info.ft_state);
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	cm_roam_release_lock(vdev);
end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return status;
}

bool ucfg_cm_ft_key_ready_for_install(struct wlan_objmgr_vdev *vdev)
{
	bool ret = false;
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return ret;

	if (ucfg_cm_get_ft_pre_auth_state(vdev) &&
	    mlme_priv->connect_info.ft_info.ft_state == FT_START_READY) {
		ret = true;
		ucfg_cm_set_ft_pre_auth_state(vdev, false);
	}

	return ret;
}

void ucfg_cm_ft_reset(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	qdf_mem_zero(&mlme_priv->connect_info.ft_info,
		     sizeof(struct ft_context));

	mlme_priv->connect_info.ft_info.ft_state = FT_START_READY;
}
#endif /* WLAN_FEATURE_HOST_ROAM */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef FEATURE_WLAN_ESE
static void
ucfg_cm_reset_esecckm_info(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}

	qdf_mem_zero(rso_cfg->krk, WMI_KRK_KEY_LEN);
	qdf_mem_zero(rso_cfg->btk, WMI_BTK_KEY_LEN);
	rso_cfg->is_ese_assoc = false;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

}
#else
static inline
void ucfg_cm_reset_esecckm_info(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
}
#endif

void ucfg_cm_reset_key(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	wlan_cm_set_psk_pmk(pdev, vdev_id, NULL, 0);
	ucfg_cm_reset_esecckm_info(pdev, vdev_id);
}

QDF_STATUS
ucfg_cm_roam_send_rt_stats_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint8_t param_value)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	return cm_roam_send_rt_stats_config(psoc, vdev_id, param_value);
}

QDF_STATUS
ucfg_cm_roam_send_ho_delay_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint16_t param_value)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	return cm_roam_send_ho_delay_config(psoc, vdev_id, param_value);
}

QDF_STATUS
ucfg_cm_exclude_rm_partial_scan_freq(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id, uint8_t param_value)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	return cm_exclude_rm_partial_scan_freq(psoc, vdev_id, param_value);
}

QDF_STATUS ucfg_cm_roam_full_scan_6ghz_on_disc(struct wlan_objmgr_pdev *pdev,
					       uint8_t vdev_id,
					       uint8_t param_value)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	return cm_roam_full_scan_6ghz_on_disc(psoc, vdev_id, param_value);
}
#ifdef WLAN_VENDOR_HANDOFF_CONTROL
QDF_STATUS
ucfg_cm_roam_send_vendor_handoff_param_req(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   uint32_t param_id,
					   void *vendor_handoff_context)
{
	return cm_roam_send_vendor_handoff_param_req(psoc, vdev_id, param_id,
						     vendor_handoff_context);
}

bool
ucfg_cm_roam_is_vendor_handoff_control_enable(struct wlan_objmgr_psoc *psoc)
{
	return cm_roam_is_vendor_handoff_control_enable(psoc);
}

#endif
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

QDF_STATUS
ucfg_cm_get_roam_intra_band(struct wlan_objmgr_psoc *psoc, uint16_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.lfr.roam_intra_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_cm_get_roam_rescan_rssi_diff(struct wlan_objmgr_psoc *psoc, uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.lfr.roam_rescan_rssi_diff;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_cm_get_neighbor_lookup_rssi_threshold(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   uint8_t *lookup_threshold)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id, NEXT_RSSI_THRESHOLD, &temp);
	*lookup_threshold = temp.uint_value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_cm_get_empty_scan_refresh_period(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id,
				      uint16_t *refresh_threshold)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   EMPTY_SCAN_REFRESH_PERIOD, &temp);
	*refresh_threshold = temp.uint_value;

	return QDF_STATUS_SUCCESS;
}

uint16_t
ucfg_cm_get_neighbor_scan_min_chan_time(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   SCAN_MIN_CHAN_TIME, &temp);

	return temp.uint_value;
}

QDF_STATUS
ucfg_cm_get_roam_rssi_diff(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint8_t *rssi_diff)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   ROAM_RSSI_DIFF, &temp);
	*rssi_diff = temp.uint_value;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
bool ucfg_cm_get_is_ese_feature_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.lfr.ese_enabled;
}
#endif

uint16_t
ucfg_cm_get_neighbor_scan_max_chan_time(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   SCAN_MAX_CHAN_TIME, &temp);

	return temp.uint_value;
}

uint16_t
ucfg_cm_get_neighbor_scan_period(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   NEIGHBOR_SCAN_PERIOD, &temp);
	return temp.uint_value;
}

bool ucfg_cm_get_wes_mode(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.lfr.wes_mode_enabled;
}

bool ucfg_cm_get_is_lfr_feature_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.lfr.lfr_enabled;
}

bool ucfg_cm_get_is_ft_feature_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.lfr.fast_transition_enabled;
}

QDF_STATUS
ucfg_cm_get_roam_scan_home_away_time(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     uint16_t *roam_scan_home_away_time)
{
	struct cm_roam_values_copy temp;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id, SCAN_HOME_AWAY, &temp);

	*roam_scan_home_away_time = temp.uint_value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_cm_get_roam_opportunistic_scan_threshold_diff(
						struct wlan_objmgr_psoc *psoc,
						int8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.lfr.opportunistic_scan_threshold_diff;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_cm_get_neighbor_scan_refresh_period(struct wlan_objmgr_psoc *psoc,
					 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*value = mlme_obj->cfg.lfr.neighbor_scan_results_refresh_period;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_cm_get_empty_scan_refresh_period_global(struct wlan_objmgr_psoc *psoc,
					     uint16_t *roam_scan_period_global)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*roam_scan_period_global =
			mlme_obj->cfg.lfr.empty_scan_refresh_period;

	return QDF_STATUS_SUCCESS;
}

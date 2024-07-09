/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
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
 *  DOC:    wma_power.c
 *  This file contains powersave related functions.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"
#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"

#include "cds_utils.h"

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"

#include "wma_internal.h"
#include "wlan_pmo_ucfg_api.h"

/**
 * wma_unified_modem_power_state() - set modem power state to fw
 * @wmi_handle: wmi handle
 * @param_value: parameter value
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_unified_modem_power_state(wmi_unified_t wmi_handle, uint32_t param_value)
{
	QDF_STATUS status;
	wmi_modem_power_state_cmd_param *cmd;
	wmi_buf_t buf;
	uint16_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return -ENOMEM;

	cmd = (wmi_modem_power_state_cmd_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_modem_power_state_cmd_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_modem_power_state_cmd_param));
	cmd->modem_power_state = param_value;
	WMA_LOGD("%s: Setting cmd->modem_power_state = %u", __func__,
		 param_value);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_MODEM_POWER_STATE_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * wma_unified_set_sta_ps_param() - set sta power save parameter to fw
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 * @param: param
 * @value: parameter value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_unified_set_sta_ps_param(wmi_unified_t wmi_handle,
					    uint32_t vdev_id, uint32_t param,
					    uint32_t value)
{
	tp_wma_handle wma;
	struct wma_txrx_node *iface;
	struct sta_ps_params sta_ps_param = {0};
	QDF_STATUS status;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: wma is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (!wma_is_vdev_valid(vdev_id))
		return QDF_STATUS_E_INVAL;

	WMA_LOGD("Set Sta Ps param vdevId %d Param %d val %d",
		 vdev_id, param, value);
	iface = &wma->interfaces[vdev_id];

	sta_ps_param.vdev_id = vdev_id;
	sta_ps_param.param_id = param;
	sta_ps_param.value = value;
	status = wmi_unified_sta_ps_cmd_send(wmi_handle, &sta_ps_param);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return status;
}

#ifdef QCA_IBSS_SUPPORT
QDF_STATUS
wma_set_ibss_pwrsave_params(tp_wma_handle wma, uint8_t vdev_id)
{
	QDF_STATUS ret;

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_ATIM_WINDOW_LENGTH,
			wma->wma_ibss_power_save_params.atimWindowLength);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed to set WMI_VDEV_PARAM_ATIM_WINDOW_LENGTH ret = %d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_IS_IBSS_POWER_SAVE_ALLOWED,
			wma->wma_ibss_power_save_params.isPowerSaveAllowed);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IS_IBSS_POWER_SAVE_ALLOWED ret=%d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_IS_POWER_COLLAPSE_ALLOWED,
			wma->wma_ibss_power_save_params.isPowerCollapseAllowed);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IS_POWER_COLLAPSE_ALLOWED ret=%d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			 WMI_VDEV_PARAM_IS_AWAKE_ON_TXRX_ENABLED,
			 wma->wma_ibss_power_save_params.isAwakeonTxRxEnabled);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IS_AWAKE_ON_TXRX_ENABLED ret=%d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_INACTIVITY_CNT,
			wma->wma_ibss_power_save_params.inactivityCount);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_INACTIVITY_CNT ret=%d",
			 ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_TXSP_END_INACTIVITY_TIME_MS,
			wma->wma_ibss_power_save_params.txSPEndInactivityTime);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_TXSP_END_INACTIVITY_TIME_MS ret=%d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_IBSS_PS_WARMUP_TIME_SECS,
			wma->wma_ibss_power_save_params.ibssPsWarmupTime);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IBSS_PS_WARMUP_TIME_SECS ret=%d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_ENABLE,
			wma->wma_ibss_power_save_params.ibssPs1RxChainInAtimEnable);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed to set IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_ENABLE ret=%d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_IBSS_SUPPORT */

/**
 * wma_set_ap_peer_uapsd() - set powersave parameters in ap mode to fw
 * @wma: wma handle
 * @vdev_id: vdev id
 * @peer_addr: peer mac address
 * @uapsd_value: uapsd value
 * @max_sp: maximum service period
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_set_ap_peer_uapsd(tp_wma_handle wma, uint32_t vdev_id,
			      uint8_t *peer_addr, uint8_t uapsd_value,
			      uint8_t max_sp)
{
	uint32_t uapsd = 0;
	uint32_t max_sp_len = 0;
	QDF_STATUS ret;
	struct ap_ps_params param = {0};

	if (uapsd_value & UAPSD_VO_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC3_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC3_TRIGGER_EN;
	}

	if (uapsd_value & UAPSD_VI_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC2_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC2_TRIGGER_EN;
	}

	if (uapsd_value & UAPSD_BK_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC1_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC1_TRIGGER_EN;
	}

	if (uapsd_value & UAPSD_BE_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC0_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC0_TRIGGER_EN;
	}

	switch (max_sp) {
	case UAPSD_MAX_SP_LEN_2:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_2;
		break;
	case UAPSD_MAX_SP_LEN_4:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_4;
		break;
	case UAPSD_MAX_SP_LEN_6:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_6;
		break;
	default:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_UNLIMITED;
		break;
	}

	WMA_LOGD("Set WMI_AP_PS_PEER_PARAM_UAPSD 0x%x for "QDF_MAC_ADDR_FMT,
		 uapsd, QDF_MAC_ADDR_REF(peer_addr));
	param.vdev_id = vdev_id;
	param.param = WMI_AP_PS_PEER_PARAM_UAPSD;
	param.value = uapsd;
	ret = wmi_unified_ap_ps_cmd_send(wma->wmi_handle, peer_addr,
						&param);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed to set WMI_AP_PS_PEER_PARAM_UAPSD for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(peer_addr));
		return ret;
	}

	WMA_LOGD("Set WMI_AP_PS_PEER_PARAM_MAX_SP 0x%x for "QDF_MAC_ADDR_FMT,
		 max_sp_len, QDF_MAC_ADDR_REF(peer_addr));

	param.vdev_id = vdev_id;
	param.param = WMI_AP_PS_PEER_PARAM_MAX_SP;
	param.value = max_sp_len;
	ret = wmi_unified_ap_ps_cmd_send(wma->wmi_handle, peer_addr,
					  &param);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Failed to set WMI_AP_PS_PEER_PARAM_MAX_SP for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(peer_addr));
		return ret;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_update_edca_params_for_ac() - to update per ac EDCA parameters
 * @edca_param: EDCA parameters
 * @wmm_param: wmm parameters
 * @ac: access category
 *
 * Return: none
 */
void wma_update_edca_params_for_ac(tSirMacEdcaParamRecord *edca_param,
				   struct wmi_host_wme_vparams *wmm_param,
				   int ac, bool mu_edca_param)
{
	wmm_param->cwmin = WMA_WMM_EXPO_TO_VAL(edca_param->cw.min);
	wmm_param->cwmax = WMA_WMM_EXPO_TO_VAL(edca_param->cw.max);
	wmm_param->aifs = edca_param->aci.aifsn;
	if (mu_edca_param)
		wmm_param->mu_edca_timer = edca_param->mu_edca_timer;
	else
		wmm_param->txoplimit = edca_param->txoplimit;
	wmm_param->acm = edca_param->aci.acm;

	wmm_param->noackpolicy = edca_param->no_ack;

	WMA_LOGD("WMM PARAMS AC[%d]: AIFS %d Min %d Max %d %s %d ACM %d NOACK %d",
			ac, wmm_param->aifs, wmm_param->cwmin,
			wmm_param->cwmax,
			mu_edca_param ? "MU_EDCA TIMER" : "TXOP",
			mu_edca_param ? wmm_param->mu_edca_timer :
				wmm_param->txoplimit,
			wmm_param->acm, wmm_param->noackpolicy);
}

/**
 * wma_set_tx_power() - set tx power limit in fw
 * @handle: wma handle
 * @tx_pwr_params: tx power parameters
 *
 * Return: none
 */
void wma_set_tx_power(WMA_HANDLE handle,
		      tMaxTxPowerParams *tx_pwr_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	uint8_t vdev_id;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;
	int8_t max_reg_power;
	struct wma_txrx_node *iface;

	if (tx_pwr_params->dev_mode == QDF_SAP_MODE ||
	    tx_pwr_params->dev_mode == QDF_P2P_GO_MODE) {
		ret = wma_find_vdev_id_by_addr(wma_handle,
					       tx_pwr_params->bssId.bytes,
					       &vdev_id);
	} else {
		ret = wma_find_vdev_id_by_bssid(wma_handle,
						tx_pwr_params->bssId.bytes,
						&vdev_id);
	}
	if (ret) {
		WMA_LOGE("vdev id is invalid for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(tx_pwr_params->bssId.bytes));
		qdf_mem_free(tx_pwr_params);
		return;
	}

	if (!wma_is_vdev_up(vdev_id)) {
		WMA_LOGE("%s: vdev id %d is not up for "QDF_MAC_ADDR_FMT,
			 __func__, vdev_id,
			 QDF_MAC_ADDR_REF(tx_pwr_params->bssId.bytes));
		qdf_mem_free(tx_pwr_params);
		return;
	}

	iface = &wma_handle->interfaces[vdev_id];
	if (tx_pwr_params->power == 0) {
		/* set to default. Since the app does not care the tx power
		 * we keep the previous setting
		 */
		mlme_set_tx_power(iface->vdev, tx_pwr_params->power);
		ret = 0;
		goto end;
	}

	max_reg_power = mlme_get_max_reg_power(iface->vdev);

	if (max_reg_power != 0) {
		/* make sure tx_power less than max_tx_power */
		if (tx_pwr_params->power > max_reg_power) {
			tx_pwr_params->power = max_reg_power;
		}
	}
	if (mlme_get_tx_power(iface->vdev) != tx_pwr_params->power) {

		/* tx_power changed, Push the tx_power to FW */
		WMA_LOGI("%s: Set TX pwr limit [WMI_VDEV_PARAM_TX_PWRLIMIT] to %d",
			__func__, tx_pwr_params->power);
		ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_TX_PWRLIMIT,
					      tx_pwr_params->power);
		if (ret == QDF_STATUS_SUCCESS)
			mlme_set_tx_power(iface->vdev, tx_pwr_params->power);
	} else {
		/* no tx_power change */
		ret = QDF_STATUS_SUCCESS;
	}
end:
	qdf_mem_free(tx_pwr_params);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Failed to set vdev param WMI_VDEV_PARAM_TX_PWRLIMIT");
}

void wma_send_max_tx_pwrlmt(WMA_HANDLE handle, uint8_t vdev_id)
{
	uint32_t max_tx_pwr;
	struct wma_txrx_node *iface;
	struct vdev_mlme_obj *mlme_obj;
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	iface = &wma_handle->interfaces[vdev_id];
	if (!iface) {
		wma_err("Failed to get iface handle for vdev_id %d", vdev_id);
		return;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!mlme_obj)
		return;

	max_tx_pwr = mlme_obj->mgmt.generic.tx_pwrlimit;
	if (!max_tx_pwr)
		return;

	wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
			   WMI_VDEV_PARAM_TX_PWRLIMIT,
			   max_tx_pwr);
}

/**
 * wma_set_max_tx_power() - set max tx power limit in fw
 * @handle: wma handle
 * @tx_pwr_params: tx power parameters
 *
 * Return: none
 */
void wma_set_max_tx_power(WMA_HANDLE handle,
			  tMaxTxPowerParams *tx_pwr_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	uint8_t vdev_id;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;
	int8_t prev_max_power;
	int8_t max_reg_power;
	struct wma_txrx_node *iface;

	if (tx_pwr_params->dev_mode == QDF_SAP_MODE ||
	    tx_pwr_params->dev_mode == QDF_P2P_GO_MODE) {
		ret = wma_find_vdev_id_by_addr(wma_handle,
					       tx_pwr_params->bssId.bytes,
					       &vdev_id);
	} else {
		ret = wma_find_vdev_id_by_bssid(wma_handle,
						tx_pwr_params->bssId.bytes,
						&vdev_id);
	}
	if (ret) {
		WMA_LOGE("vdev id is invalid for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(tx_pwr_params->bssId.bytes));
		qdf_mem_free(tx_pwr_params);
		return;
	}

	if (!wma_is_vdev_up(vdev_id)) {
		WMA_LOGE("%s: vdev id %d is not up", __func__, vdev_id);
		qdf_mem_free(tx_pwr_params);
		return;
	}

	iface = &wma_handle->interfaces[vdev_id];
	if (mlme_get_max_reg_power(iface->vdev) == tx_pwr_params->power) {
		ret = QDF_STATUS_SUCCESS;
		goto end;
	}
	prev_max_power = mlme_get_max_reg_power(iface->vdev);

	mlme_set_max_reg_power(iface->vdev, tx_pwr_params->power);

	max_reg_power = mlme_get_max_reg_power(iface->vdev);

	if (max_reg_power == 0) {
		ret = QDF_STATUS_SUCCESS;
		goto end;
	}
	WMA_LOGI("Set MAX TX pwr limit [WMI_VDEV_PARAM_TX_PWRLIMIT] to %d",
		 max_reg_power);
	ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_TX_PWRLIMIT,
				max_reg_power);
	if (ret == QDF_STATUS_SUCCESS)
		mlme_set_tx_power(iface->vdev, max_reg_power);
	else
		mlme_set_max_reg_power(iface->vdev, prev_max_power);
end:
	qdf_mem_free(tx_pwr_params);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("%s: Failed to set vdev param WMI_VDEV_PARAM_TX_PWRLIMIT",
			__func__);
}

/**
 * wmi_unified_set_sta_ps() - set sta powersave params in fw
 * @handle: wma handle
 * @vdev_id: vdev id
 * @val: value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS wmi_unified_set_sta_ps(wmi_unified_t wmi_handle,
					 uint32_t vdev_id, uint8_t val)
{
	QDF_STATUS ret;

	ret = wmi_unified_set_sta_ps_mode(wmi_handle, vdev_id,
				   val);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Failed to send set Mimo PS ret = %d", ret);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_uapsd_mask() - get uapsd mask based on uapsd parameters
 * @uapsd_params: uapsed parameters
 *
 * Return: uapsd mask
 */
static inline uint32_t wma_get_uapsd_mask(tpUapsd_Params uapsd_params)
{
	uint32_t uapsd_val = 0;

	if (uapsd_params->beDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC0_DELIVERY_EN;

	if (uapsd_params->beTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC0_TRIGGER_EN;

	if (uapsd_params->bkDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC1_DELIVERY_EN;

	if (uapsd_params->bkTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC1_TRIGGER_EN;

	if (uapsd_params->viDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC2_DELIVERY_EN;

	if (uapsd_params->viTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC2_TRIGGER_EN;

	if (uapsd_params->voDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC3_DELIVERY_EN;

	if (uapsd_params->voTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC3_TRIGGER_EN;

	return uapsd_val;
}

/**
 * wma_set_force_sleep() - set power save parameters to fw
 * @wma: wma handle
 * @vdev_id: vdev id
 * @enable: enable/disable
 * @power_config: power configuration
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS wma_set_force_sleep(tp_wma_handle wma,
				uint32_t vdev_id,
				uint8_t enable,
				enum powersave_mode power_config,
				bool enable_ps)
{
	QDF_STATUS ret;
	uint32_t cfg_data_val = 0;
	/* get mac to access CFG data base */
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint32_t rx_wake_policy;
	uint32_t tx_wake_threshold;
	uint32_t pspoll_count;
	uint32_t inactivity_time;
	uint32_t psmode;

	WMA_LOGD("Set Force Sleep vdevId %d val %d", vdev_id, enable);

	if (!mac) {
		WMA_LOGE("%s: Unable to get PE context", __func__);
		return QDF_STATUS_E_NOMEM;
	}

	inactivity_time = mac->mlme_cfg->timeouts.ps_data_inactivity_timeout;

	if (enable) {
		/* override normal configuration and force station asleep */
		rx_wake_policy = WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
		tx_wake_threshold = WMI_STA_PS_TX_WAKE_THRESHOLD_NEVER;

		if (ucfg_pmo_get_max_ps_poll(mac->psoc))
			pspoll_count =
				(uint32_t)ucfg_pmo_get_max_ps_poll(mac->psoc);
		else
			pspoll_count = WMA_DEFAULT_MAX_PSPOLL_BEFORE_WAKE;

		psmode = WMI_STA_PS_MODE_ENABLED;
	} else {
		/* Ps Poll Wake Policy */
		if (ucfg_pmo_get_max_ps_poll(mac->psoc)) {
			/* Ps Poll is enabled */
			rx_wake_policy = WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
			pspoll_count =
				(uint32_t)ucfg_pmo_get_max_ps_poll(mac->psoc);
			tx_wake_threshold = WMI_STA_PS_TX_WAKE_THRESHOLD_NEVER;
		} else {
			rx_wake_policy = WMI_STA_PS_RX_WAKE_POLICY_WAKE;
			pspoll_count = WMI_STA_PS_PSPOLL_COUNT_NO_MAX;
			tx_wake_threshold = WMI_STA_PS_TX_WAKE_THRESHOLD_ALWAYS;
		}
		psmode = WMI_STA_PS_MODE_ENABLED;
	}

	/*
	 * Advanced power save is enabled by default in Firmware
	 * So Disable advanced power save explicitly
	 */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_ENABLE_QPOWER,
					   power_config);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("%s(%d) Power Failed vdevId %d",
			power_config ? "Enable" : "Disable",
			power_config, vdev_id);
		return ret;
	}
	WMA_LOGD("Power %s(%d) vdevId %d",
		 power_config ? "Enabled" : "Disabled",
		 power_config, vdev_id);

	/* Set the Wake Policy to WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_RX_WAKE_POLICY,
					   rx_wake_policy);

	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Setting wake policy Failed vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("Setting wake policy to %d vdevId %d",
		 rx_wake_policy, vdev_id);

	/* Set the Tx Wake Threshold */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD,
					   tx_wake_threshold);

	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Setting TxWake Threshold vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("Setting TxWake Threshold to %d vdevId %d",
		 tx_wake_threshold, vdev_id);

	/* Set the Ps Poll Count */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_PSPOLL_COUNT,
					   pspoll_count);

	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Set Ps Poll Count Failed vdevId %d ps poll cnt %d",
			 vdev_id, pspoll_count);
		return ret;
	}
	WMA_LOGD("Set Ps Poll Count vdevId %d ps poll cnt %d",
		 vdev_id, pspoll_count);

	/* Set the Tx/Rx InActivity */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_INACTIVITY_TIME,
					   inactivity_time);

	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Setting Tx/Rx InActivity Failed vdevId %d InAct %d",
			 vdev_id, inactivity_time);
		return ret;
	}
	WMA_LOGD("Set Tx/Rx InActivity vdevId %d InAct %d",
		 vdev_id, inactivity_time);

	/* Enable Sta Mode Power save */
	if (enable_ps) {
		ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, true);

		if (QDF_IS_STATUS_ERROR(ret)) {
			WMA_LOGE("Enable Sta Mode Ps Failed vdevId %d",
				vdev_id);
			return ret;
		}
	}

	/* Set Listen Interval */
	cfg_data_val = mac->mlme_cfg->sap_cfg.listen_interval;
	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_LISTEN_INTERVAL,
					      cfg_data_val);
	if (QDF_IS_STATUS_ERROR(ret)) {
		/* Even it fails continue Fw will take default LI */
		WMA_LOGE("Failed to Set Listen Interval vdevId %d", vdev_id);
	}
	WMA_LOGD("Set Listen Interval vdevId %d Listen Intv %d",
		 vdev_id, cfg_data_val);

	return QDF_STATUS_SUCCESS;
}

static uint8_t wma_get_power_config(tp_wma_handle wma)
{
	WMA_LOGD("POWER mode is %d", wma->powersave_mode);

	return wma->powersave_mode;
}

void wma_enable_sta_ps_mode(tpEnablePsParams ps_req)
{
	uint32_t vdev_id = ps_req->sessionid;
	QDF_STATUS ret;
	enum powersave_mode power_config;
	struct wma_txrx_node *iface;
	t_wma_handle *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("wma handle is null");
		return;
	}

	iface = &wma_handle->interfaces[vdev_id];

	if (!iface || !iface->vdev) {
		WMA_LOGE("%s: vdev is NULL for vdev_%d", __func__, vdev_id);
		return;
	}

	power_config = wma_get_power_config(wma_handle);
	if (eSIR_ADDON_NOTHING == ps_req->psSetting) {
		if (power_config && iface->uapsd_cached_val) {
			power_config = 0;
			WMA_LOGD("Advanced power save is disabled");
		}
		WMA_LOGD("Enable Sta Mode Ps vdevId %d", vdev_id);
		ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
						   vdev_id,
				WMI_STA_PS_PARAM_UAPSD, 0);
		if (QDF_IS_STATUS_ERROR(ret)) {
			WMA_LOGE("Set Uapsd param 0 Failed vdevId %d", vdev_id);
			return;
		}

		ret = wma_set_force_sleep(wma_handle, vdev_id, false,
					  power_config, true);
		if (QDF_IS_STATUS_ERROR(ret)) {
			WMA_LOGE("Enable Sta Ps Failed vdevId %d", vdev_id);
			return;
		}
	} else if (eSIR_ADDON_ENABLE_UAPSD == ps_req->psSetting) {
		uint32_t uapsd_val = 0;

		uapsd_val = wma_get_uapsd_mask(&ps_req->uapsdParams);
		if (uapsd_val != iface->uapsd_cached_val) {
			WMA_LOGD("Enable Uapsd vdevId %d Mask %d",
					vdev_id, uapsd_val);
			ret =
			 wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
						      vdev_id,
						      WMI_STA_PS_PARAM_UAPSD,
						      uapsd_val);
			if (QDF_IS_STATUS_ERROR(ret)) {
				WMA_LOGE("Enable Uapsd Failed vdevId %d",
						vdev_id);
				return;
			}
			/* Cache the Uapsd Mask */
			iface->uapsd_cached_val = uapsd_val;
		} else {
			WMA_LOGD("Already Uapsd Enabled vdevId %d Mask %d",
					vdev_id, uapsd_val);
		}

		if (power_config && iface->uapsd_cached_val) {
			power_config = 0;
			WMA_LOGD("Qpower is disabled");
		}
		WMA_LOGD("Enable Forced Sleep vdevId %d", vdev_id);
		ret = wma_set_force_sleep(wma_handle, vdev_id, true,
					  power_config, true);

		if (QDF_IS_STATUS_ERROR(ret)) {
			WMA_LOGE("Enable Forced Sleep Failed vdevId %d",
				 vdev_id);
			return;
		}
	}

	if (wma_handle->ito_repeat_count) {
		WMA_LOGI("Set ITO count to %d for vdevId %d",
					wma_handle->ito_repeat_count, vdev_id);

		ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
			vdev_id,
			WMI_STA_PS_PARAM_MAX_RESET_ITO_COUNT_ON_TIM_NO_TXRX,
			wma_handle->ito_repeat_count);
		if (QDF_IS_STATUS_ERROR(ret)) {
			WMA_LOGE("Set ITO count failed vdevId %d Error %d",
				 vdev_id, ret);
			return;
		}
	}

	/* power save request succeeded */
	iface->in_bmps = true;
}


/**
 * wma_disable_sta_ps_mode() - disable sta powersave params in fw
 * @wma: wma handle
 * @ps_req: power save request
 *
 * Return: none
 */
void wma_disable_sta_ps_mode(tpDisablePsParams ps_req)
{
	QDF_STATUS ret;
	uint32_t vdev_id = ps_req->sessionid;
	struct wma_txrx_node *iface;
	t_wma_handle *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("wma handle is null");
		return;
	}

	iface = &wma_handle->interfaces[vdev_id];

	WMA_LOGD("Disable Sta Mode Ps vdevId %d", vdev_id);

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma_handle->wmi_handle, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		return;
	}
	iface->in_bmps = false;

	/* Disable UAPSD incase if additional Req came */
	if (eSIR_ADDON_DISABLE_UAPSD == ps_req->psSetting) {
		WMA_LOGD("Disable Uapsd vdevId %d", vdev_id);
		ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
						   vdev_id,
				WMI_STA_PS_PARAM_UAPSD, 0);
		if (QDF_IS_STATUS_ERROR(ret)) {
			WMA_LOGE("Disable Uapsd Failed vdevId %d", vdev_id);
			/*
			 * Even this fails we can proceed as success
			 * since we disabled powersave
			 */
		}
	}
}

QDF_STATUS wma_set_power_config(uint8_t vdev_id, enum powersave_mode power)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		WMA_LOGE("%s: WMA context is invald!", __func__);
		return QDF_STATUS_E_INVAL;
	}

	WMA_LOGI("configuring power: %d", power);
	wma->powersave_mode = power;
	return wma_unified_set_sta_ps_param(wma->wmi_handle,
					    vdev_id,
					    WMI_STA_PS_ENABLE_QPOWER,
					    wma_get_power_config(wma));
}

void wma_enable_uapsd_mode(tp_wma_handle wma, tpEnableUapsdParams ps_req)
{
	QDF_STATUS ret;
	uint32_t vdev_id = ps_req->sessionid;
	uint32_t uapsd_val = 0;
	enum powersave_mode power_config = wma_get_power_config(wma);
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];

	if (!iface->vdev) {
		WMA_LOGE("%s: vdev is NULL for vdev_%d", __func__, vdev_id);
		return;
	}

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		return;
	}

	uapsd_val = wma_get_uapsd_mask(&ps_req->uapsdParams);

	WMA_LOGD("Enable Uapsd vdevId %d Mask %d", vdev_id, uapsd_val);
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
			WMI_STA_PS_PARAM_UAPSD, uapsd_val);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Enable Uapsd Failed vdevId %d", vdev_id);
		return;
	}

	if (power_config && uapsd_val) {
		power_config = 0;
		WMA_LOGD("Disable power %d", vdev_id);
	}
	iface->uapsd_cached_val = uapsd_val;
	WMA_LOGD("Enable Forced Sleep vdevId %d", vdev_id);
	ret = wma_set_force_sleep(wma, vdev_id, true,
			power_config, ps_req->uapsdParams.enable_ps);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Enable Forced Sleep Failed vdevId %d", vdev_id);
		return;
	}

}

/**
 * wma_disable_uapsd_mode() - disable uapsd mode in fw
 * @wma: wma handle
 * @ps_req: power save request
 *
 * Return: none
 */
void wma_disable_uapsd_mode(tp_wma_handle wma,
			    tpDisableUapsdParams ps_req)
{
	QDF_STATUS ret;
	uint32_t vdev_id = ps_req->sessionid;
	enum powersave_mode power_config = wma_get_power_config(wma);

	WMA_LOGD("Disable Uapsd vdevId %d", vdev_id);

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		return;
	}

	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
			WMI_STA_PS_PARAM_UAPSD, 0);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Disable Uapsd Failed vdevId %d", vdev_id);
		return;
	}

	/* Re enable Sta Mode Powersave with proper configuration */
	ret = wma_set_force_sleep(wma, vdev_id, false,
			power_config, true);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Disable Forced Sleep Failed vdevId %d", vdev_id);
		return;
	}
}

/**
 * wma_set_sta_uapsd_auto_trig_cmd() - set uapsd auto trigger command
 * @wmi_handle: wma handle
 * @vdevid: vdev id
 * @peer_addr: peer mac address
 * @trig_param: auto trigger parameters
 * @num_ac: number of access category
 *
 * This function sets the trigger
 * uapsd params such as service interval, delay interval
 * and suspend interval which will be used by the firmware
 * to send trigger frames periodically when there is no
 * traffic on the transmit side.
 *
 * Return: 0 for success or error code.
 */
static QDF_STATUS wma_set_sta_uapsd_auto_trig_cmd(wmi_unified_t wmi_handle,
					uint32_t vdevid,
					uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
					struct sta_uapsd_params *trig_param,
					uint32_t num_ac)
{
	QDF_STATUS ret;
	struct sta_uapsd_trig_params cmd = {0};

	cmd.vdevid = vdevid;
	cmd.auto_triggerparam = trig_param;
	cmd.num_ac = num_ac;

	qdf_mem_copy((uint8_t *) cmd.peer_addr, (uint8_t *) peer_addr,
		     sizeof(uint8_t) * QDF_MAC_ADDR_SIZE);
	ret = wmi_unified_set_sta_uapsd_auto_trig_cmd(wmi_handle,
				   &cmd);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Failed to send set uapsd param ret = %d", ret);

	return ret;
}

QDF_STATUS wma_trigger_uapsd_params(tp_wma_handle wma_handle, uint32_t vdev_id,
				    tp_wma_trigger_uapsd_params
				    trigger_uapsd_params)
{
	QDF_STATUS ret;
	uint8_t *bssid;
	struct sta_uapsd_params uapsd_trigger_param;

	WMA_LOGD("Trigger uapsd params vdev id %d", vdev_id);

	WMA_LOGD("WMM AC %d User Priority %d SvcIntv %d DelIntv %d SusIntv %d",
		 trigger_uapsd_params->wmm_ac,
		 trigger_uapsd_params->user_priority,
		 trigger_uapsd_params->service_interval,
		 trigger_uapsd_params->delay_interval,
		 trigger_uapsd_params->suspend_interval);

	if (!wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_sta_uapsd_basic_auto_trig) ||
	    !wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_sta_uapsd_var_auto_trig)) {
		WMA_LOGD("Trigger uapsd is not supported vdev id %d", vdev_id);
		return QDF_STATUS_SUCCESS;
	}

	uapsd_trigger_param.wmm_ac = trigger_uapsd_params->wmm_ac;
	uapsd_trigger_param.user_priority = trigger_uapsd_params->user_priority;
	uapsd_trigger_param.service_interval =
		trigger_uapsd_params->service_interval;
	uapsd_trigger_param.suspend_interval =
		trigger_uapsd_params->suspend_interval;
	uapsd_trigger_param.delay_interval =
		trigger_uapsd_params->delay_interval;

	bssid = wma_get_vdev_bssid(wma_handle->interfaces[vdev_id].vdev);
	if (!bssid) {
		WMA_LOGE("%s: Failed to get bssid for vdev_%d",
			 __func__, vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	ret = wma_set_sta_uapsd_auto_trig_cmd(wma_handle->wmi_handle,
			vdev_id, bssid,
			&uapsd_trigger_param, 1);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Fail to send uapsd param cmd for vdevid %d ret = %d",
			 ret, vdev_id);
		return ret;
	}

	return ret;
}

QDF_STATUS wma_disable_uapsd_per_ac(tp_wma_handle wma_handle,
				    uint32_t vdev_id, enum uapsd_ac ac)
{
	QDF_STATUS ret;
	uint8_t *bssid;
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];
	struct sta_uapsd_params uapsd_trigger_param;
	enum uapsd_up user_priority;

	WMA_LOGD("Disable Uapsd per ac vdevId %d ac %d", vdev_id, ac);

	switch (ac) {
	case UAPSD_VO:
		iface->uapsd_cached_val &=
			~(WMI_STA_PS_UAPSD_AC3_DELIVERY_EN |
			  WMI_STA_PS_UAPSD_AC3_TRIGGER_EN);
		user_priority = UAPSD_UP_VO;
		break;
	case UAPSD_VI:
		iface->uapsd_cached_val &=
			~(WMI_STA_PS_UAPSD_AC2_DELIVERY_EN |
			  WMI_STA_PS_UAPSD_AC2_TRIGGER_EN);
		user_priority = UAPSD_UP_VI;
		break;
	case UAPSD_BK:
		iface->uapsd_cached_val &=
			~(WMI_STA_PS_UAPSD_AC1_DELIVERY_EN |
			  WMI_STA_PS_UAPSD_AC1_TRIGGER_EN);
		user_priority = UAPSD_UP_BK;
		break;
	case UAPSD_BE:
		iface->uapsd_cached_val &=
			~(WMI_STA_PS_UAPSD_AC0_DELIVERY_EN |
			  WMI_STA_PS_UAPSD_AC0_TRIGGER_EN);
		user_priority = UAPSD_UP_BE;
		break;
	default:
		WMA_LOGE("Invalid AC vdevId %d ac %d", vdev_id, ac);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Disable Auto Trigger Functionality before
	 * disabling uapsd for a particular AC
	 */
	uapsd_trigger_param.wmm_ac = ac;
	uapsd_trigger_param.user_priority = user_priority;
	uapsd_trigger_param.service_interval = 0;
	uapsd_trigger_param.suspend_interval = 0;
	uapsd_trigger_param.delay_interval = 0;

	bssid = wma_get_vdev_bssid(wma_handle->interfaces[vdev_id].vdev);
	if (!bssid) {
		WMA_LOGE("%s: Failed to get bssid for vdev_%d",
			 __func__, vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	ret = wma_set_sta_uapsd_auto_trig_cmd(wma_handle->wmi_handle,
		vdev_id, bssid,
		&uapsd_trigger_param, 1);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Fail to send auto trig cmd for vdevid %d ret = %d",
			 ret, vdev_id);
		return ret;
	}

	ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_UAPSD,
					   iface->uapsd_cached_val);
	if (QDF_IS_STATUS_ERROR(ret)) {
		WMA_LOGE("Disable Uapsd per ac Failed vdevId %d ac %d", vdev_id,
			 ac);
		return ret;
	}
	WMA_LOGD("Disable Uapsd per ac vdevId %d val %d", vdev_id,
		 iface->uapsd_cached_val);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_temperature() - get pdev temperature req
 * @wmi_handle: wma handle
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_get_temperature(tp_wma_handle wma_handle)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	ret = wmi_unified_get_temperature(wma_handle->wmi_handle);
	if (ret)
		WMA_LOGE("Failed to send set Mimo PS ret = %d", ret);

	return ret;
}

/**
 * wma_pdev_temperature_evt_handler() - pdev temperature event handler
 * @handle: wma handle
 * @event: event buffer
 * @len : length
 *
 * Return: 0 for success or error code.
 */
int wma_pdev_temperature_evt_handler(void *handle, uint8_t *event,
				     uint32_t len)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct scheduler_msg sme_msg = { 0 };
	WMI_PDEV_TEMPERATURE_EVENTID_param_tlvs *param_buf;
	wmi_pdev_temperature_event_fixed_param *wmi_event;

	param_buf = (WMI_PDEV_TEMPERATURE_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		WMA_LOGE("Invalid pdev_temperature event buffer");
		return -EINVAL;
	}

	wmi_event = param_buf->fixed_param;
	WMA_LOGI(FL("temperature: %d"), wmi_event->value);

	sme_msg.type = eWNI_SME_MSG_GET_TEMPERATURE_IND;
	sme_msg.bodyptr = NULL;
	sme_msg.bodyval = wmi_event->value;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		WMA_LOGE(FL("Fail to post get temperature ind msg"));
	return 0;
}

/**
 * wma_process_tx_power_limits() - sends the power limits for 2g/5g to firmware
 * @handle: wma handle
 * @ptxlim: power limit value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_process_tx_power_limits(WMA_HANDLE handle,
				       struct tx_power_limit *ptxlim)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	int32_t ret = 0;
	uint32_t txpower_params2g = 0;
	uint32_t txpower_params5g = 0;
	struct pdev_params pdevparam;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue tx power limit",
			 __func__);
		return QDF_STATUS_E_INVAL;
	}
	/* Set value and reason code for 2g and 5g power limit */

	SET_PDEV_PARAM_TXPOWER_REASON(txpower_params2g,
				      WMI_PDEV_PARAM_TXPOWER_REASON_SAR);
	SET_PDEV_PARAM_TXPOWER_VALUE(txpower_params2g, ptxlim->txPower2g);

	SET_PDEV_PARAM_TXPOWER_REASON(txpower_params5g,
				      WMI_PDEV_PARAM_TXPOWER_REASON_SAR);
	SET_PDEV_PARAM_TXPOWER_VALUE(txpower_params5g, ptxlim->txPower5g);

	WMA_LOGD("%s: txpower2g: %x txpower5g: %x",
		 __func__, txpower_params2g, txpower_params5g);

	pdevparam.param_id = WMI_PDEV_PARAM_TXPOWER_LIMIT2G;
	pdevparam.param_value = txpower_params2g;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &pdevparam,
					 WMA_WILDCARD_PDEV_ID);
	if (ret) {
		WMA_LOGE("%s: Failed to set txpower 2g (%d)", __func__, ret);
		return QDF_STATUS_E_FAILURE;
	}
	pdevparam.param_id = WMI_PDEV_PARAM_TXPOWER_LIMIT5G;
	pdevparam.param_value = txpower_params5g;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &pdevparam,
					 WMA_WILDCARD_PDEV_ID);
	if (ret) {
		WMA_LOGE("%s: Failed to set txpower 5g (%d)", __func__, ret);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_WMI_BCN
/**
 * wma_add_p2p_ie() - add p2p IE
 * @frm: ptr where p2p ie needs to add
 *
 * Return: ptr after p2p ie
 */
static uint8_t *wma_add_p2p_ie(uint8_t *frm)
{
	uint8_t wfa_oui[3] = WMA_P2P_WFA_OUI;
	struct p2p_ie *p2p_ie = (struct p2p_ie *)frm;

	p2p_ie->p2p_id = WMA_P2P_IE_ID;
	p2p_ie->p2p_oui[0] = wfa_oui[0];
	p2p_ie->p2p_oui[1] = wfa_oui[1];
	p2p_ie->p2p_oui[2] = wfa_oui[2];
	p2p_ie->p2p_oui_type = WMA_P2P_WFA_VER;
	p2p_ie->p2p_len = 4;
	return frm + sizeof(struct p2p_ie);
}

/**
 * wma_update_beacon_noa_ie() - update beacon ie
 * @bcn: beacon info
 * @new_noa_sub_ie_len: ie length
 *
 * Return: none
 */
static void wma_update_beacon_noa_ie(struct beacon_info *bcn,
				     uint16_t new_noa_sub_ie_len)
{
	struct p2p_ie *p2p_ie;
	uint8_t *buf;

	/* if there is nothing to add, just return */
	if (new_noa_sub_ie_len == 0) {
		if (bcn->noa_sub_ie_len && bcn->noa_ie) {
			WMA_LOGD("%s: NoA is present in previous beacon, but not present in swba event, So Reset the NoA",
				 __func__);
			/* TODO: Assuming p2p noa ie is last ie in the beacon */
			qdf_mem_zero(bcn->noa_ie, (bcn->noa_sub_ie_len +
						   sizeof(struct p2p_ie)));
			bcn->len -= (bcn->noa_sub_ie_len +
				     sizeof(struct p2p_ie));
			bcn->noa_ie = NULL;
			bcn->noa_sub_ie_len = 0;
		}
		WMA_LOGD("%s: No need to update NoA", __func__);
		return;
	}

	if (bcn->noa_sub_ie_len && bcn->noa_ie) {
		/* NoA present in previous beacon, update it */
		WMA_LOGD("%s: NoA present in previous beacon, update the NoA IE, bcn->len %u bcn->noa_sub_ie_len %u",
			 __func__, bcn->len, bcn->noa_sub_ie_len);
		bcn->len -= (bcn->noa_sub_ie_len + sizeof(struct p2p_ie));
		qdf_mem_zero(bcn->noa_ie,
			     (bcn->noa_sub_ie_len + sizeof(struct p2p_ie)));
	} else {                /* NoA is not present in previous beacon */
		WMA_LOGD("%s: NoA not present in previous beacon, add it bcn->len %u",
			 __func__, bcn->len);
		buf = qdf_nbuf_data(bcn->buf);
		bcn->noa_ie = buf + bcn->len;
	}

	bcn->noa_sub_ie_len = new_noa_sub_ie_len;
	wma_add_p2p_ie(bcn->noa_ie);
	p2p_ie = (struct p2p_ie *)bcn->noa_ie;
	p2p_ie->p2p_len += new_noa_sub_ie_len;
	qdf_mem_copy((bcn->noa_ie + sizeof(struct p2p_ie)), bcn->noa_sub_ie,
		     new_noa_sub_ie_len);

	bcn->len += (new_noa_sub_ie_len + sizeof(struct p2p_ie));
	WMA_LOGI("%s: Updated beacon length with NoA Ie is %u",
		 __func__, bcn->len);
}

/**
 * wma_p2p_create_sub_ie_noa() - put p2p noa ie
 * @buf: buffer
 * @noa: noa element ie
 * @new_noa_sub_ie_len: ie length
 *
 * Return: none
 */
static void wma_p2p_create_sub_ie_noa(uint8_t *buf,
				      struct p2p_sub_element_noa *noa,
				      uint16_t *new_noa_sub_ie_len)
{
	uint8_t tmp_octet = 0;
	int i;
	uint8_t *buf_start = buf;

	*buf++ = WMA_P2P_SUB_ELEMENT_NOA;       /* sub-element id */
	ASSERT(noa->num_descriptors <= WMA_MAX_NOA_DESCRIPTORS);

	/*
	 * Length = (2 octets for Index and CTWin/Opp PS) and
	 * (13 octets for each NOA Descriptors)
	 */
	P2PIE_PUT_LE16(buf, WMA_NOA_IE_SIZE(noa->num_descriptors));
	buf += 2;

	*buf++ = noa->index;    /* Instance Index */

	tmp_octet = noa->ctwindow & WMA_P2P_NOA_IE_CTWIN_MASK;
	if (noa->oppPS)
		tmp_octet |= WMA_P2P_NOA_IE_OPP_PS_SET;
	*buf++ = tmp_octet;     /* Opp Ps and CTWin capabilities */

	for (i = 0; i < noa->num_descriptors; i++) {
		ASSERT(noa->noa_descriptors[i].type_count != 0);

		*buf++ = noa->noa_descriptors[i].type_count;

		P2PIE_PUT_LE32(buf, noa->noa_descriptors[i].duration);
		buf += 4;
		P2PIE_PUT_LE32(buf, noa->noa_descriptors[i].interval);
		buf += 4;
		P2PIE_PUT_LE32(buf, noa->noa_descriptors[i].start_time);
		buf += 4;
	}
	*new_noa_sub_ie_len = (buf - buf_start);
}

/**
 * wma_update_noa() - update noa params
 * @beacon: beacon info
 * @noa_ie: noa ie
 *
 * Return: none
 */
void wma_update_noa(struct beacon_info *beacon,
		    struct p2p_sub_element_noa *noa_ie)
{
	uint16_t new_noa_sub_ie_len;

	/* Call this function by holding the spinlock on beacon->lock */

	if (noa_ie) {
		if ((noa_ie->ctwindow == 0) && (noa_ie->oppPS == 0) &&
		    (noa_ie->num_descriptors == 0)) {
			/* NoA is not present */
			WMA_LOGD("%s: NoA is not present", __func__);
			new_noa_sub_ie_len = 0;
		} else {
			/* Create the binary blob containing NOA sub-IE */
			WMA_LOGD("%s: Create NOA sub ie", __func__);
			wma_p2p_create_sub_ie_noa(&beacon->noa_sub_ie[0],
						  noa_ie, &new_noa_sub_ie_len);
		}
	} else {
		WMA_LOGD("%s: No need to add NOA", __func__);
		new_noa_sub_ie_len = 0; /* no NOA IE sub-attributes */
	}

	wma_update_beacon_noa_ie(beacon, new_noa_sub_ie_len);
}

/**
 * wma_update_probe_resp_noa() - update noa IE in probe response
 * @wma_handle: wma handle
 * @noa_ie: noa ie
 *
 * Return: none
 */
void wma_update_probe_resp_noa(tp_wma_handle wma_handle,
			       struct p2p_sub_element_noa *noa_ie)
{
	tSirP2PNoaAttr *noa_attr = qdf_mem_malloc(sizeof(tSirP2PNoaAttr));
	WMA_LOGD("Received update NoA event");
	if (!noa_attr)
		return;

	qdf_mem_zero(noa_attr, sizeof(tSirP2PNoaAttr));

	noa_attr->index = noa_ie->index;
	noa_attr->oppPsFlag = noa_ie->oppPS;
	noa_attr->ctWin = noa_ie->ctwindow;
	if (!noa_ie->num_descriptors) {
		WMA_LOGD("Zero NoA descriptors");
	} else {
		WMA_LOGD("%d NoA descriptors", noa_ie->num_descriptors);
		noa_attr->uNoa1IntervalCnt =
			noa_ie->noa_descriptors[0].type_count;
		noa_attr->uNoa1Duration = noa_ie->noa_descriptors[0].duration;
		noa_attr->uNoa1Interval = noa_ie->noa_descriptors[0].interval;
		noa_attr->uNoa1StartTime =
			noa_ie->noa_descriptors[0].start_time;
		if (noa_ie->num_descriptors > 1) {
			noa_attr->uNoa2IntervalCnt =
				noa_ie->noa_descriptors[1].type_count;
			noa_attr->uNoa2Duration =
				noa_ie->noa_descriptors[1].duration;
			noa_attr->uNoa2Interval =
				noa_ie->noa_descriptors[1].interval;
			noa_attr->uNoa2StartTime =
				noa_ie->noa_descriptors[1].start_time;
		}
	}
	WMA_LOGI("Sending SIR_HAL_P2P_NOA_ATTR_IND to LIM");
	wma_send_msg(wma_handle, SIR_HAL_P2P_NOA_ATTR_IND, (void *)noa_attr, 0);
}

#else
static inline uint8_t *wma_add_p2p_ie(uint8_t *frm)
{
	return 0;
}

static inline void
wma_update_beacon_noa_ie(struct beacon_info *bcn,
			 uint16_t new_noa_sub_ie_len)
{
}

static inline void
wma_p2p_create_sub_ie_noa(uint8_t *buf,
			  struct p2p_sub_element_noa *noa,
			  uint16_t *new_noa_sub_ie_len)
{
}

void wma_update_noa(struct beacon_info *beacon,
		    struct p2p_sub_element_noa *noa_ie)
{
}

void wma_update_probe_resp_noa(tp_wma_handle wma_handle,
			       struct p2p_sub_element_noa *noa_ie)
{
}

#endif

/**
 * wma_process_set_mimops_req() - Set the received MiMo PS state to firmware
 * @handle: wma handle
 * @mimops: MIMO powersave params
 *
 * Return: none
 */
void wma_process_set_mimops_req(tp_wma_handle wma_handle,
				tSetMIMOPS *mimops)
{
	/* Translate to what firmware understands */
	if (mimops->htMIMOPSState == eSIR_HT_MIMO_PS_DYNAMIC)
		mimops->htMIMOPSState = WMI_PEER_MIMO_PS_DYNAMIC;
	else if (mimops->htMIMOPSState == eSIR_HT_MIMO_PS_STATIC)
		mimops->htMIMOPSState = WMI_PEER_MIMO_PS_STATIC;
	else if (mimops->htMIMOPSState == eSIR_HT_MIMO_PS_NO_LIMIT)
		mimops->htMIMOPSState = WMI_PEER_MIMO_PS_NONE;

	wma_debug("htMIMOPSState = %d, sessionId = %d peerMac <"QDF_MAC_ADDR_FMT">",
		 mimops->htMIMOPSState, mimops->sessionId,
		 QDF_MAC_ADDR_REF(mimops->peerMac));

	wma_set_peer_param(wma_handle, mimops->peerMac,
			   WMI_PEER_MIMO_PS_STATE, mimops->htMIMOPSState,
			   mimops->sessionId);
}

/**
 * wma_set_mimops() - set MIMO powersave
 * @handle: wma handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_mimops(tp_wma_handle wma, uint8_t vdev_id, int value)
{
	QDF_STATUS ret;

	ret = wmi_unified_set_mimops(wma->wmi_handle, vdev_id,
				   value);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Failed to send set Mimo PS ret = %d", ret);

	return ret;
}

/**
 * wma_notify_modem_power_state() - notify modem power state
 * @wma_ptr: wma handle
 * @pReq: modem power state
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_notify_modem_power_state(void *wma_ptr,
					tSirModemPowerStateInd *pReq)
{
	QDF_STATUS status;
	tp_wma_handle wma = (tp_wma_handle) wma_ptr;

	WMA_LOGD("%s: WMA notify Modem Power State %d", __func__, pReq->param);

	status = wma_unified_modem_power_state(wma->wmi_handle, pReq->param);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to notify Modem Power State %d", pReq->param);
		return status;
	}

	WMA_LOGD("Successfully notify Modem Power State %d", pReq->param);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_idle_ps_config() - enable/disble Low Power Support(Pdev Specific)
 * @wma_ptr: wma handle
 * @idle_ps: idle powersave
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_idle_ps_config(void *wma_ptr, uint32_t idle_ps)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle) wma_ptr;
	struct pdev_params pdevparam;

	WMA_LOGD("WMA Set Idle Ps Config [1:set 0:clear] val %d", idle_ps);

	/* Set Idle Mode Power Save Config */
	pdevparam.param_id = WMI_PDEV_PARAM_IDLE_PS_CONFIG;
	pdevparam.param_value = idle_ps;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &pdevparam,
					 WMA_WILDCARD_PDEV_ID);

	if (ret) {
		WMA_LOGE("Fail to Set Idle Ps Config %d", idle_ps);
		return QDF_STATUS_E_FAILURE;
	}
	wma->in_imps = !!idle_ps;

	WMA_LOGD("Successfully Set Idle Ps Config %d", idle_ps);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_smps_params() - set smps params
 * @wma: wma handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_smps_params(tp_wma_handle wma, uint8_t vdev_id,
			       int value)
{
	QDF_STATUS ret;

	ret = wmi_unified_set_smps_params(wma->wmi_handle, vdev_id,
				   value);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Failed to send set Mimo PS ret = %d", ret);

	return ret;
}

#ifdef FEATURE_TX_POWER
/**
 * wma_set_tx_power_scale() - set tx power scale
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_tx_power_scale(uint8_t vdev_id, int value)
{
	QDF_STATUS ret;
	tp_wma_handle wma_handle =
			(tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		WMA_LOGE("%s: wma_handle is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (!wma_is_vdev_up(vdev_id)) {
		WMA_LOGE("%s: vdev id %d is not up", __func__, vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_TXPOWER_SCALE, value);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Set tx power scale failed");

	return ret;
}

/**
 * wma_set_tx_power_scale_decr_db() - decrease power by DB value
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_tx_power_scale_decr_db(uint8_t vdev_id, int value)
{
	QDF_STATUS ret;
	tp_wma_handle wma_handle =
			(tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		WMA_LOGE("%s: wma_handle is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (!wma_is_vdev_up(vdev_id)) {
		WMA_LOGE("%s: vdev id %d is not up", __func__, vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_TXPOWER_SCALE_DECR_DB, value);
	if (QDF_IS_STATUS_ERROR(ret))
		WMA_LOGE("Decrease tx power value failed");

	return ret;
}
#endif /* FEATURE_TX_POWER */


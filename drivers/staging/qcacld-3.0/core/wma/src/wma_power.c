/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
	wma_debug("Setting cmd->modem_power_state = %u", param_value);
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
	if (!wma)
		return QDF_STATUS_E_FAILURE;
	if (!wma_is_vdev_valid(vdev_id))
		return QDF_STATUS_E_INVAL;

	wma_debug("Set Sta Ps param vdevId %d Param %d val %d",
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

	wma_debug("Set WMI_AP_PS_PEER_PARAM_UAPSD 0x%x for "QDF_MAC_ADDR_FMT,
		 uapsd, QDF_MAC_ADDR_REF(peer_addr));
	param.vdev_id = vdev_id;
	param.param = WMI_AP_PS_PEER_PARAM_UAPSD;
	param.value = uapsd;
	ret = wmi_unified_ap_ps_cmd_send(wma->wmi_handle, peer_addr,
						&param);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Failed to set WMI_AP_PS_PEER_PARAM_UAPSD for "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_addr));
		return ret;
	}

	wma_debug("Set WMI_AP_PS_PEER_PARAM_MAX_SP 0x%x for "QDF_MAC_ADDR_FMT,
		 max_sp_len, QDF_MAC_ADDR_REF(peer_addr));

	param.vdev_id = vdev_id;
	param.param = WMI_AP_PS_PEER_PARAM_MAX_SP;
	param.value = max_sp_len;
	ret = wmi_unified_ap_ps_cmd_send(wma->wmi_handle, peer_addr,
					  &param);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Failed to set WMI_AP_PS_PEER_PARAM_MAX_SP for "QDF_MAC_ADDR_FMT,
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
				   int ac, bool mu_edca_param,
				   uint8_t *debug_str,
				   uint32_t debug_str_size, uint32_t *len)
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

	*len += qdf_scnprintf(debug_str + *len, debug_str_size - *len,
			      "AC[%d]: AIFS %d Min %d Max %d %s %d ACM %d NOACK %d, ",
			      ac, wmm_param->aifs, wmm_param->cwmin,
			      wmm_param->cwmax,
			      mu_edca_param ? "MU_EDCA TIMER" : "TXOP",
			      mu_edca_param ? wmm_param->mu_edca_timer : wmm_param->txoplimit,
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
		wma_err("vdev id is invalid for "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(tx_pwr_params->bssId.bytes));
		qdf_mem_free(tx_pwr_params);
		return;
	}

	if (!wma_is_vdev_up(vdev_id)) {
		wma_err("vdev id %d is not up for "QDF_MAC_ADDR_FMT, vdev_id,
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
		wma_nofl_debug("TXP[W][set_tx_pwr]: %d", tx_pwr_params->power);
		ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
					 wmi_vdev_param_tx_pwrlimit,
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
		wma_err("Failed to set vdev param wmi_vdev_param_tx_pwrlimit");
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
	int8_t max_reg_power;
	struct wma_txrx_node *iface;
	int8_t max_tx_power;
	struct wlan_channel *channel;
	uint16_t ch_freq;

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
		wma_err("vdev id is invalid for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(tx_pwr_params->bssId.bytes));
		qdf_mem_free(tx_pwr_params);
		return;
	}

	if (!wma_is_vdev_up(vdev_id)) {
		wma_err("vdev id %d is not up", vdev_id);
		qdf_mem_free(tx_pwr_params);
		return;
	}

	iface = &wma_handle->interfaces[vdev_id];
	channel = wlan_vdev_get_active_channel(iface->vdev);
	if (channel) {
		ch_freq = channel->ch_freq;
	} else {
		wma_err("Failed to get active channel");
		qdf_mem_free(tx_pwr_params);
		return;
	}
	max_reg_power = wlan_reg_get_channel_reg_power_for_freq(
			wma_handle->mac_context->pdev, ch_freq);
	/*
	 * When user tx power as auto, host will configure
	 * the tx power as max regulatory power allowed for
	 * that channel which signifies that it will be the
	 * upper limit for tx power used while transmission
	 */
	if (tx_pwr_params->power == 0)
		max_tx_power = max_reg_power;
	else
		max_tx_power = QDF_MIN(tx_pwr_params->power, max_reg_power);

	wma_nofl_debug("TXP[W][set_max_pwr_req]: %d", max_tx_power);
	ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				wmi_vdev_param_tx_pwrlimit,
				max_tx_power);
	if (ret == QDF_STATUS_SUCCESS)
		mlme_set_tx_power(iface->vdev, max_tx_power);
	qdf_mem_free(tx_pwr_params);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Failed to set vdev param wmi_vdev_param_tx_pwrlimit");
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
		wma_err("Failed to send set Mimo PS ret = %d", ret);

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
 * @ps_param: OPM params
 * @enable_ps: enable power save
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS wma_set_force_sleep(tp_wma_handle wma,
				uint32_t vdev_id,
				uint8_t enable,
				struct wma_ps_params *ps_params,
				bool enable_ps)
{
	QDF_STATUS ret;
	/* get mac to access CFG data base */
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint32_t rx_wake_policy;
	uint32_t tx_wake_threshold;
	uint32_t pspoll_count;
	uint32_t psmode;
	struct wlan_objmgr_vdev *vdev;
	u32 listen_interval = 0;

	wma_debug("Set Force Sleep vdevId %d val %d", vdev_id, enable);

	if (!mac) {
		wma_err("Unable to get PE context");
		return QDF_STATUS_E_NOMEM;
	}

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
					   WMI_STA_PS_ENABLE_OPM,
					   ps_params->opm_mode);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("%s(%d) Power Failed vdevId %d",
			ps_params->opm_mode ? "Enable" : "Disable",
			ps_params->opm_mode, vdev_id);
		return ret;
	}
	wma_debug("Power %s(%d) vdevId %d",
		 ps_params->opm_mode ? "Enabled" : "Disabled",
		 ps_params->opm_mode, vdev_id);

	/* Set the Tx/Rx InActivity */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_INACTIVITY_TIME,
					   ps_params->ps_ito);

	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Setting Tx/Rx InActivity Failed vdevId %d InAct %d",
			vdev_id, ps_params->ps_ito);
		return ret;
	}
	wma_debug("Set Tx/Rx InActivity vdevId %d InAct %d",
		  vdev_id, ps_params->ps_ito);

	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_SPEC_WAKE_INTERVAL,
					   ps_params->spec_wake);

	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Setting Spec wake Failed vdevId %d InAct %d",
			vdev_id, ps_params->spec_wake);
		return ret;
	}
	wma_debug("Set Spec wake vdevId %d InAct %d",
		  vdev_id, ps_params->spec_wake);

	/* Set the Wake Policy to WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_RX_WAKE_POLICY,
					   rx_wake_policy);

	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Setting wake policy Failed vdevId %d", vdev_id);
		return ret;
	}
	wma_debug("Setting wake policy to %d vdevId %d",
		 rx_wake_policy, vdev_id);

	/* Set the Tx Wake Threshold */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD,
					   tx_wake_threshold);

	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Setting TxWake Threshold vdevId %d", vdev_id);
		return ret;
	}
	wma_debug("Setting TxWake Threshold to %d vdevId %d",
		 tx_wake_threshold, vdev_id);

	/* Set the Ps Poll Count */
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_PSPOLL_COUNT,
					   pspoll_count);

	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Set Ps Poll Count Failed vdevId %d ps poll cnt %d",
			 vdev_id, pspoll_count);
		return ret;
	}
	wma_debug("Set Ps Poll Count vdevId %d ps poll cnt %d",
		 vdev_id, pspoll_count);

	/* Enable Sta Mode Power save */
	if (enable_ps) {
		ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, true);

		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Enable Sta Mode Ps Failed vdevId %d",
				vdev_id);
			return ret;
		}
	}

	/* Set Listen Interval */
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_WMA_ID);
	/* If user has configured listen interval already
	 * No need to send vdev set param cmd
	 */
	if (vdev) {
		ret = wlan_pmo_get_listen_interval(vdev, &listen_interval);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
	}

	if (!listen_interval || QDF_IS_STATUS_ERROR(ret)) {
		listen_interval = mac->mlme_cfg->sap_cfg.listen_interval;
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					 wmi_vdev_param_listen_interval,
					 listen_interval);
	}
	if (QDF_IS_STATUS_ERROR(ret)) {
		/* Even it fails continue Fw will take default LI */
		wma_err("Failed to Set Listen Interval vdevId %d", vdev_id);
	}
	wma_debug("Set Listen Interval vdevId %d Listen Intv %d",
		 vdev_id, listen_interval);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wma_wlan_pmo_get_ps_params(struct wlan_objmgr_vdev *vdev,
					     struct wma_ps_params *ps_params)
{
	struct pmo_ps_params pmo_ps_param = {0};
	QDF_STATUS status;

	status = wlan_pmo_get_ps_params(vdev, &pmo_ps_param);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	switch (pmo_ps_param.opm_mode) {
	case PMO_PS_ADVANCED_POWER_SAVE_DISABLE:
		ps_params->opm_mode = WMI_STA_PS_OPM_CONSERVATIVE;
		break;
	case PMO_PS_ADVANCED_POWER_SAVE_ENABLE:
		ps_params->opm_mode = WMI_STA_PS_OPM_AGGRESSIVE;
		break;
	case PMO_PS_ADVANCED_POWER_SAVE_USER_DEFINED:
		ps_params->opm_mode = WMI_STA_PS_USER_DEF;
		break;
	default:
		wma_err("Invalid opm_mode:%d", pmo_ps_param.opm_mode);
		return QDF_STATUS_E_INVAL;
	}
	ps_params->ps_ito = pmo_ps_param.ps_ito;
	ps_params->spec_wake = pmo_ps_param.spec_wake;

	return status;
}

void wma_enable_sta_ps_mode(tpEnablePsParams ps_req)
{
	uint32_t vdev_id = ps_req->sessionid;
	QDF_STATUS ret;
	struct wma_txrx_node *iface;
	t_wma_handle *wma_handle;
	struct wma_ps_params ps_params = {0};

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle)
		return;

	iface = &wma_handle->interfaces[vdev_id];

	if (!iface || !iface->vdev) {
		wma_err("vdev is NULL for vdev_%d", vdev_id);
		return;
	}

	ret = wma_wlan_pmo_get_ps_params(iface->vdev, &ps_params);
	if (QDF_IS_STATUS_ERROR(ret))
		return;

	if (eSIR_ADDON_NOTHING == ps_req->psSetting) {
		if (ps_params.opm_mode && iface->uapsd_cached_val) {
			ps_params.opm_mode = WMI_STA_PS_OPM_CONSERVATIVE;
			wma_debug("Advanced power save is disabled");
		}
		wma_debug("Enable Sta Mode Ps vdevId %d", vdev_id);
		ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
						   vdev_id,
				WMI_STA_PS_PARAM_UAPSD, 0);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Set Uapsd param 0 Failed vdevId %d", vdev_id);
			return;
		}

		ret = wma_set_force_sleep(wma_handle, vdev_id, false,
					  &ps_params, true);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Enable Sta Ps Failed vdevId %d", vdev_id);
			return;
		}
	} else if (eSIR_ADDON_ENABLE_UAPSD == ps_req->psSetting) {
		uint32_t uapsd_val = 0;

		uapsd_val = wma_get_uapsd_mask(&ps_req->uapsdParams);
		if (uapsd_val != iface->uapsd_cached_val) {
			wma_debug("Enable Uapsd vdevId %d Mask %d",
					vdev_id, uapsd_val);
			ret =
			 wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
						      vdev_id,
						      WMI_STA_PS_PARAM_UAPSD,
						      uapsd_val);
			if (QDF_IS_STATUS_ERROR(ret)) {
				wma_err("Enable Uapsd Failed vdevId %d",
					vdev_id);
				return;
			}
			/* Cache the Uapsd Mask */
			iface->uapsd_cached_val = uapsd_val;
		} else {
			wma_debug("Already Uapsd Enabled vdevId %d Mask %d",
					vdev_id, uapsd_val);
		}

		if (ps_params.opm_mode && iface->uapsd_cached_val) {
			ps_params.opm_mode = WMI_STA_PS_OPM_CONSERVATIVE;
			wma_debug("Qpower is disabled");
		}
		wma_debug("Enable Forced Sleep vdevId %d", vdev_id);
		ret = wma_set_force_sleep(wma_handle, vdev_id, true,
					  &ps_params, true);

		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Enable Forced Sleep Failed vdevId %d",
				 vdev_id);
			return;
		}
	}

	if (wma_handle->ito_repeat_count) {
		wma_debug("Set ITO count to %d for vdevId %d",
			 wma_handle->ito_repeat_count, vdev_id);

		ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
			vdev_id,
			WMI_STA_PS_PARAM_MAX_RESET_ITO_COUNT_ON_TIM_NO_TXRX,
			wma_handle->ito_repeat_count);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Set ITO count failed vdevId %d Error %d",
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
	if (!wma_handle)
		return;

	iface = &wma_handle->interfaces[vdev_id];

	wma_debug("Disable Sta Mode Ps vdevId %d", vdev_id);

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma_handle->wmi_handle, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		return;
	}
	iface->in_bmps = false;

	/* Disable UAPSD incase if additional Req came */
	if (eSIR_ADDON_DISABLE_UAPSD == ps_req->psSetting) {
		wma_debug("Disable Uapsd vdevId %d", vdev_id);
		ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle,
						   vdev_id,
				WMI_STA_PS_PARAM_UAPSD, 0);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Disable Uapsd Failed vdevId %d", vdev_id);
			/*
			 * Even this fails we can proceed as success
			 * since we disabled powersave
			 */
		}
	}
}

/**
 * wma_convert_opm_mode() - convert opm with equivalent wmi opm
 * @opm_mode: Optimized power management mode
 *
 * Return: enum wmi_sta_ps_scheme_cfg
 */
static enum wmi_sta_ps_scheme_cfg
wma_convert_opm_mode(enum wma_sta_ps_scheme_cfg opm_mode)
{
	switch (opm_mode) {
	case WMA_STA_PS_OPM_CONSERVATIVE:
		return WMI_STA_PS_OPM_CONSERVATIVE;
	case WMA_STA_PS_OPM_AGGRESSIVE:
		return WMI_STA_PS_OPM_AGGRESSIVE;
	case WMA_STA_PS_USER_DEF:
		return WMI_STA_PS_USER_DEF;
	default:
		wma_err("Invalid opm_mode: %d", opm_mode);
		return WMI_STA_PS_OPM_CONSERVATIVE;
	}
}

QDF_STATUS wma_set_power_config(uint8_t vdev_id,
				enum wma_sta_ps_scheme_cfg power)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma)
		return QDF_STATUS_E_INVAL;

	wma_info("configuring power: %d", power);
	return wma_unified_set_sta_ps_param(wma->wmi_handle,
					    vdev_id,
					    WMI_STA_PS_ENABLE_OPM,
					    wma_convert_opm_mode(power));
}

QDF_STATUS wma_set_power_config_ito(uint8_t vdev_id, uint16_t ps_ito)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_INACTIVITY_TIME,
					   ps_ito);
}

QDF_STATUS wma_set_power_config_spec_wake(uint8_t vdev_id, uint16_t spec_wake)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
			WMI_STA_PS_PARAM_SPEC_WAKE_INTERVAL,
			spec_wake);
}

void wma_enable_uapsd_mode(tp_wma_handle wma, tpEnableUapsdParams ps_req)
{
	QDF_STATUS ret;
	uint32_t vdev_id = ps_req->sessionid;
	uint32_t uapsd_val = 0;
	struct wma_ps_params ps_params = {0};
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];

	if (!iface->vdev) {
		wma_err("vdev is NULL for vdev_%d", vdev_id);
		return;
	}

	ret = wma_wlan_pmo_get_ps_params(iface->vdev, &ps_params);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("ps_param is invalid for vdev_%d", vdev_id);
		return;
	}

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		return;
	}

	uapsd_val = wma_get_uapsd_mask(&ps_req->uapsdParams);

	wma_debug("Enable Uapsd vdevId %d Mask %d", vdev_id, uapsd_val);
	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
			WMI_STA_PS_PARAM_UAPSD, uapsd_val);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Enable Uapsd Failed vdevId %d", vdev_id);
		return;
	}

	if (ps_params.opm_mode && uapsd_val) {
		ps_params.opm_mode = 0;
		wma_debug("Disable power %d", vdev_id);
	}
	iface->uapsd_cached_val = uapsd_val;
	wma_debug("Enable Forced Sleep vdevId %d", vdev_id);
	ret = wma_set_force_sleep(wma, vdev_id, true,
				  &ps_params, ps_req->uapsdParams.enable_ps);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Enable Forced Sleep Failed vdevId %d", vdev_id);
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
	struct wma_ps_params ps_params = {0};
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];

	if (!iface->vdev) {
		wma_err("vdev is null for vdev_%d", vdev_id);
		return;
	}

	wma_debug("Disable Uapsd vdevId %d", vdev_id);

	ret = wma_wlan_pmo_get_ps_params(iface->vdev, &ps_params);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("ps_param is invalid for vdev_%d", vdev_id);
		return;
	}

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		return;
	}

	ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
			WMI_STA_PS_PARAM_UAPSD, 0);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Disable Uapsd Failed vdevId %d", vdev_id);
		return;
	}

	/* Re enable Sta Mode Powersave with proper configuration */
	ret = wma_set_force_sleep(wma, vdev_id, false,
			&ps_params, true);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Disable Forced Sleep Failed vdevId %d", vdev_id);
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
		wma_err("Failed to send set uapsd param ret = %d", ret);

	return ret;
}

QDF_STATUS wma_trigger_uapsd_params(tp_wma_handle wma_handle, uint32_t vdev_id,
				    tp_wma_trigger_uapsd_params
				    trigger_uapsd_params)
{
	QDF_STATUS ret;
	uint8_t *bssid;
	struct sta_uapsd_params uapsd_trigger_param;

	wma_debug("Trigger uapsd params vdev id %d", vdev_id);

	wma_debug("WMM AC %d User Priority %d SvcIntv %d DelIntv %d SusIntv %d",
		 trigger_uapsd_params->wmm_ac,
		 trigger_uapsd_params->user_priority,
		 trigger_uapsd_params->service_interval,
		 trigger_uapsd_params->delay_interval,
		 trigger_uapsd_params->suspend_interval);

	if (!wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_sta_uapsd_basic_auto_trig) ||
	    !wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_sta_uapsd_var_auto_trig)) {
		wma_debug("Trigger uapsd is not supported vdev id %d", vdev_id);
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
		wma_err("Failed to get bssid for vdev_%d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	ret = wma_set_sta_uapsd_auto_trig_cmd(wma_handle->wmi_handle,
			vdev_id, bssid,
			&uapsd_trigger_param, 1);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Fail to send uapsd param cmd for vdevid %d ret = %d",
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

	wma_debug("Disable Uapsd per ac vdevId %d ac %d", vdev_id, ac);

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
		wma_err("Invalid AC vdevId %d ac %d", vdev_id, ac);
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
		wma_err("Failed to get bssid for vdev_%d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	ret = wma_set_sta_uapsd_auto_trig_cmd(wma_handle->wmi_handle,
		vdev_id, bssid,
		&uapsd_trigger_param, 1);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Fail to send auto trig cmd for vdevid %d ret = %d",
			ret, vdev_id);
		return ret;
	}

	ret = wma_unified_set_sta_ps_param(wma_handle->wmi_handle, vdev_id,
					   WMI_STA_PS_PARAM_UAPSD,
					   iface->uapsd_cached_val);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Disable Uapsd per ac Failed vdevId %d ac %d", vdev_id,
			ac);
		return ret;
	}
	wma_debug("Disable Uapsd per ac vdevId %d val %d", vdev_id,
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
		wma_err("Failed to send set Mimo PS ret = %d", ret);

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
		wma_err("Invalid pdev_temperature event buffer");
		return -EINVAL;
	}

	wmi_event = param_buf->fixed_param;
	wma_info("temperature: %d", wmi_event->value);

	sme_msg.type = eWNI_SME_MSG_GET_TEMPERATURE_IND;
	sme_msg.bodyptr = NULL;
	sme_msg.bodyval = wmi_event->value;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		wma_err("Fail to post get temperature ind msg");
	return 0;
}

#define MAX_PDEV_TXPOWER_PARAMS 2
/* params being sent:
 * wmi_pdev_param_txpower_limit2g
 * wmi_pdev_param_txpower_limit5g
 */

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
	struct wmi_unified *wmi_handle;
	struct dev_set_param setparam[MAX_PDEV_TXPOWER_PARAMS] = {};
	uint8_t index = 0;

	if (wma_validate_handle(wma))
		return QDF_STATUS_E_INVAL;

	wmi_handle = wma->wmi_handle;
	if (wmi_validate_handle(wmi_handle))
		return QDF_STATUS_E_INVAL;

	/* Set value and reason code for 2g and 5g power limit */

	SET_PDEV_PARAM_TXPOWER_REASON(txpower_params2g,
				      wmi_pdev_param_txpower_reason_sar);
	SET_PDEV_PARAM_TXPOWER_VALUE(txpower_params2g, ptxlim->txPower2g);

	SET_PDEV_PARAM_TXPOWER_REASON(txpower_params5g,
				      wmi_pdev_param_txpower_reason_sar);
	SET_PDEV_PARAM_TXPOWER_VALUE(txpower_params5g, ptxlim->txPower5g);

	wma_debug("txpower2g: %x txpower5g: %x",
		 txpower_params2g, txpower_params5g);
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_txpower_limit2g,
					txpower_params2g, index++,
					MAX_PDEV_TXPOWER_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("failed at wmi_pdev_param_txpower_limit2g");
		goto error;
	}
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_txpower_limit5g,
					txpower_params5g, index++,
					MAX_PDEV_TXPOWER_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("failed at wmi_pdev_param_txpower_limit5g");
		goto error;
	}
	ret = wma_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						  WMI_PDEV_ID_SOC, setparam,
						  index);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("failed to send tx power pdev set params");
error:
	return ret;
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
			wma_debug("NoA is present in previous beacon, but not present in swba event, So Reset the NoA");
			/* TODO: Assuming p2p noa ie is last ie in the beacon */
			qdf_mem_zero(bcn->noa_ie, (bcn->noa_sub_ie_len +
						   sizeof(struct p2p_ie)));
			bcn->len -= (bcn->noa_sub_ie_len +
				     sizeof(struct p2p_ie));
			bcn->noa_ie = NULL;
			bcn->noa_sub_ie_len = 0;
		}
		wma_debug("No need to update NoA");
		return;
	}

	if (bcn->noa_sub_ie_len && bcn->noa_ie) {
		/* NoA present in previous beacon, update it */
		wma_debug("NoA present in previous beacon, update the NoA IE, bcn->len %u bcn->noa_sub_ie_len %u",
			 bcn->len, bcn->noa_sub_ie_len);
		bcn->len -= (bcn->noa_sub_ie_len + sizeof(struct p2p_ie));
		qdf_mem_zero(bcn->noa_ie,
			     (bcn->noa_sub_ie_len + sizeof(struct p2p_ie)));
	} else {                /* NoA is not present in previous beacon */
		wma_debug("NoA not present in previous beacon, add it bcn->len %u",
			 bcn->len);
		buf = qdf_nbuf_data(bcn->buf);
		bcn->noa_ie = buf + bcn->len;
	}

	if (bcn->len + sizeof(struct p2p_ie) + new_noa_sub_ie_len >
	    SIR_MAX_BEACON_SIZE) {
		wma_err("exceed max beacon length, bcn->len %d, new_noa_sub_ie_len %d, p2p len %u",
			bcn->len, new_noa_sub_ie_len,
			(uint32_t)sizeof(struct p2p_ie));
		return;
	}

	bcn->noa_sub_ie_len = new_noa_sub_ie_len;
	wma_add_p2p_ie(bcn->noa_ie);
	p2p_ie = (struct p2p_ie *)bcn->noa_ie;
	p2p_ie->p2p_len += new_noa_sub_ie_len;
	qdf_mem_copy((bcn->noa_ie + sizeof(struct p2p_ie)), bcn->noa_sub_ie,
		     new_noa_sub_ie_len);

	bcn->len += (new_noa_sub_ie_len + sizeof(struct p2p_ie));
	wma_debug("Updated beacon length with NoA Ie is %u", bcn->len);
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
			wma_debug("NoA is not present");
			new_noa_sub_ie_len = 0;
		} else {
			/* Create the binary blob containing NOA sub-IE */
			wma_debug("Create NOA sub ie");
			wma_p2p_create_sub_ie_noa(&beacon->noa_sub_ie[0],
						  noa_ie, &new_noa_sub_ie_len);
		}
	} else {
		wma_debug("No need to add NOA");
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
	wma_debug("Received update NoA event");
	if (!noa_attr)
		return;

	qdf_mem_zero(noa_attr, sizeof(tSirP2PNoaAttr));

	noa_attr->index = noa_ie->index;
	noa_attr->oppPsFlag = noa_ie->oppPS;
	noa_attr->ctWin = noa_ie->ctwindow;
	if (!noa_ie->num_descriptors) {
		wma_debug("Zero NoA descriptors");
	} else {
		wma_debug("%d NoA descriptors", noa_ie->num_descriptors);
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
	wma_debug("Sending SIR_HAL_P2P_NOA_ATTR_IND to LIM");
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
			   WMI_HOST_PEER_MIMO_PS_STATE, mimops->htMIMOPSState,
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
		wma_err("Failed to send set Mimo PS ret = %d", ret);

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

	wma_debug("WMA notify Modem Power State %d", pReq->param);

	status = wma_unified_modem_power_state(wma->wmi_handle, pReq->param);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to notify Modem Power State %d", pReq->param);
		return status;
	}

	wma_debug("Successfully notify Modem Power State %d", pReq->param);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_idle_ps_config() - enable/disable Low Power Support(Pdev Specific)
 * @wma_ptr: wma handle
 * @idle_ps: idle powersave
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_idle_ps_config(void *wma_ptr, uint32_t idle_ps)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle) wma_ptr;
	struct pdev_params pdevparam = {};

	wma_debug("WMA Set Idle Ps Config [1:set 0:clear] val %d", idle_ps);

	/* Set Idle Mode Power Save Config */
	pdevparam.param_id = wmi_pdev_param_idle_ps_config;
	pdevparam.param_value = idle_ps;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &pdevparam,
					 WMA_WILDCARD_PDEV_ID);

	if (ret) {
		wma_err("Fail to Set Idle Ps Config %d", idle_ps);
		return QDF_STATUS_E_FAILURE;
	}
	wma->in_imps = !!idle_ps;

	wma_debug("Successfully Set Idle Ps Config %d", idle_ps);
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

	if (!wma_is_vdev_valid(vdev_id)) {
		wma_err("Invalid VDEV ID: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	ret = wmi_unified_set_smps_params(wma->wmi_handle, vdev_id,
				   value);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Failed to send set Mimo PS ret = %d", ret);

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
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle)
		return QDF_STATUS_E_FAILURE;

	if (!wma_is_vdev_up(vdev_id)) {
		wma_err("vdev id %d is not up", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				 wmi_vdev_param_txpower_scale, value);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Set tx power scale failed");

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
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle)
		return QDF_STATUS_E_FAILURE;

	if (!wma_is_vdev_up(vdev_id)) {
		wma_err("vdev id %d is not up", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				 wmi_vdev_param_txpower_scale_decr_db, value);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("Decrease tx power value failed");

	return ret;
}
#endif /* FEATURE_TX_POWER */


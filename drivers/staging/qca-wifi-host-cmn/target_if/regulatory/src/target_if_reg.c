/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
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
 * DOC: target_if_reg.c
 * This file contains regulatory target interfaces.
 */

#include <wmi_unified_api.h>
#include <reg_services_public_struct.h>
#include <wlan_reg_tgt_api.h>
#include <target_if.h>
#include <target_if_reg.h>
#include <wmi_unified_reg_api.h>
#include <qdf_platform.h>
#include <target_if_reg_11d.h>
#include <target_if_reg_lte.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_utility.h>
#ifdef CONFIG_REG_CLIENT
#include <wlan_dcs_tgt_api.h>
#endif

/**
 * get_chan_list_cc_event_id() - Get chan_list_cc event id
 *
 * Return: Event id
 */
static inline uint32_t get_chan_list_cc_event_id(void)
{
	return wmi_reg_chan_list_cc_event_id;
}

/**
 * tgt_if_regulatory_is_regdb_offloaded() - Check if regdb is offloaded
 * @psoc: Pointer to psoc
 *
 * Return: true if regdb if offloaded, else false
 */
static bool tgt_if_regulatory_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return false;
	}

	if (!wmi_handle)
		return false;

	if (reg_rx_ops->reg_ignore_fw_reg_offload_ind &&
	    reg_rx_ops->reg_ignore_fw_reg_offload_ind(psoc)) {
		target_if_debug("User disabled regulatory offload from ini");
		return 0;
	}

	return wmi_service_enabled(wmi_handle, wmi_service_regulatory_db);
}

/**
 * tgt_if_regulatory_is_6ghz_supported() - Check if 6 GHz is supported
 * @psoc: Pointer to psoc
 *
 * Return: true if regdb if offloaded, else false
 */
static bool tgt_if_regulatory_is_6ghz_supported(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle, wmi_service_6ghz_support);
}

/**
 * tgt_if_regulatory_is_5dot9_ghz_supported() - Check if 5.9 GHz is supported
 * @psoc: Pointer to psoc
 *
 * Return: true if 5.9 GHz is supported, else false
 */
static bool
tgt_if_regulatory_is_5dot9_ghz_supported(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle, wmi_service_5dot9_ghz_support);
}

/**
 * tgt_if_regulatory_is_there_serv_ready_extn() - Check for service ready
 * extension
 * @psoc: Pointer to psoc object
 *
 * Return: true if service ready extension is present, else false.
 */
static bool tgt_if_regulatory_is_there_serv_ready_extn(
		struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle, wmi_service_ext_msg);
}

/**
 * target_if_regulatory_get_rx_ops() - Get regdb rx ops
 * @psoc: Pointer to psoc object
 *
 * Return: Reg rx_ops
 */
struct wlan_lmac_if_reg_rx_ops *
target_if_regulatory_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		target_if_err("rx_ops is NULL");
		return NULL;
	}

	return &rx_ops->reg_rx_ops;
}

struct wlan_lmac_if_reg_tx_ops *
target_if_regulatory_get_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		target_if_err("tx_ops is NULL");
		return NULL;
	}

	return &tx_ops->reg_ops;
}

QDF_STATUS target_if_reg_set_offloaded_info(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_regdb_offloaded)
		reg_rx_ops->reg_set_regdb_offloaded(
				psoc,
				tgt_if_regulatory_is_regdb_offloaded(psoc));

	if (reg_rx_ops->reg_set_11d_offloaded)
		reg_rx_ops->reg_set_11d_offloaded(
				psoc, tgt_if_regulatory_is_11d_offloaded(psoc));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS target_if_reg_set_6ghz_info(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_6ghz_supported)
		reg_rx_ops->reg_set_6ghz_supported(
			psoc,
			tgt_if_regulatory_is_6ghz_supported(psoc));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS target_if_reg_set_5dot9_ghz_info(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_5dot9_ghz_supported)
		reg_rx_ops->reg_set_5dot9_ghz_supported(
			psoc,
			tgt_if_regulatory_is_5dot9_ghz_supported(psoc));

	return QDF_STATUS_SUCCESS;
}

bool
target_if_reg_is_reg_cc_ext_event_host_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	bool reg_ext_cc_supp = false;

	reg_tx_ops = target_if_regulatory_get_tx_ops(psoc);
	if (!reg_tx_ops) {
		target_if_err("reg_tx_ops is NULL");
		return reg_ext_cc_supp;
	}

	if (reg_tx_ops->register_master_ext_handler)
		reg_ext_cc_supp = true;

	return reg_ext_cc_supp;
}

/**
 * tgt_reg_chan_list_update_handler() - Channel list update handler
 * @handle: scn handle
 * @event_buf: pointer to event buffer
 * @len: buffer length
 *
 * Return: 0 on success
 */
static int tgt_reg_chan_list_update_handler(ol_scn_t handle, uint8_t *event_buf,
					    uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
	struct cur_regulatory_info *reg_info;
	QDF_STATUS status;
	struct wmi_unified *wmi_handle;
	int ret_val = 0;

	TARGET_IF_ENTER();

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		return -EINVAL;
	}

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return -EINVAL;
	}

	if (!reg_rx_ops->master_list_handler) {
		target_if_err("master_list_handler is NULL");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("invalid wmi handle");
		return -EINVAL;
	}

	reg_info = qdf_mem_malloc(sizeof(*reg_info));
	if (!reg_info)
		return -ENOMEM;

	if (wmi_extract_reg_chan_list_update_event(wmi_handle,
						   event_buf, reg_info, len)
	    != QDF_STATUS_SUCCESS) {
		target_if_err("Extraction of channel list event failed");
		ret_val = -EFAULT;
		goto clean;
	}

	if (reg_info->phy_id >= PSOC_MAX_PHY_REG_CAP) {
		target_if_err_rl("phy_id %d is out of bounds",
				 reg_info->phy_id);
		ret_val = -EFAULT;
		goto clean;
	}

	reg_info->psoc = psoc;

	status = reg_rx_ops->master_list_handler(reg_info);
	if (status != QDF_STATUS_SUCCESS) {
		target_if_err("Failed to process master channel list handler");
		ret_val = -EFAULT;
	}

clean:
	qdf_mem_free(reg_info->reg_rules_2g_ptr);
	qdf_mem_free(reg_info->reg_rules_5g_ptr);
	qdf_mem_free(reg_info);

	TARGET_IF_EXIT();

	return ret_val;
}

/**
 * tgt_if_regulatory_register_master_list_handler() - Register master channel
 * list
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_register_master_list_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_register_event_handler(
			wmi_handle, wmi_reg_chan_list_cc_event_id,
			tgt_reg_chan_list_update_handler, WMI_RX_WORK_CTX);
}

/**
 * tgt_if_regulatory_unregister_master_list_handler() - Unregister master
 * channel list
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_unregister_master_list_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_unregister_event_handler(
			wmi_handle, wmi_reg_chan_list_cc_event_id);
}

#ifdef CONFIG_BAND_6GHZ
#ifdef CONFIG_REG_CLIENT
/**
 * tgt_reg_mem_free_fcc_rules() - Free regulatory fcc rules
 * @reg_info: Pointer to regulatory info
 *
 */
static void tgt_reg_mem_free_fcc_rules(struct cur_regulatory_info *reg_info)
{
	qdf_mem_free(reg_info->fcc_rules_ptr);
}
#else
static void tgt_reg_mem_free_fcc_rules(struct cur_regulatory_info *reg_info)
{
}
#endif

/**
 * tgt_reg_chan_list_ext_update_handler() - Extended channel list update handler
 * @handle: scn handle
 * @event_buf: pointer to event buffer
 * @len: buffer length
 *
 * Return: 0 on success
 */
static int tgt_reg_chan_list_ext_update_handler(ol_scn_t handle,
						uint8_t *event_buf,
						uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
	struct cur_regulatory_info *reg_info;
	QDF_STATUS status;
	struct wmi_unified *wmi_handle;
	int ret_val = 0;
	uint32_t i;

	TARGET_IF_ENTER();

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		return -EINVAL;
	}

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return -EINVAL;
	}

	if (!reg_rx_ops->master_list_ext_handler) {
		target_if_err("master_list_ext_handler is NULL");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("invalid wmi handle");
		return -EINVAL;
	}

	reg_info = qdf_mem_malloc(sizeof(*reg_info));
	if (!reg_info)
		return -ENOMEM;

	status = wmi_extract_reg_chan_list_ext_update_event(wmi_handle,
							    event_buf,
							    reg_info, len);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		target_if_err("Extraction of ext channel list event failed");
		ret_val = -EFAULT;
		goto clean;
	}

	if (reg_info->phy_id >= PSOC_MAX_PHY_REG_CAP) {
		target_if_err_rl("phy_id %d is out of bounds",
				 reg_info->phy_id);
		ret_val = -EFAULT;
		goto clean;
	}

	reg_info->psoc = psoc;

	status = reg_rx_ops->master_list_ext_handler(reg_info);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		target_if_err("Failed to process master ext channel list handler");
		ret_val = -EFAULT;
	}

clean:
	qdf_mem_free(reg_info->reg_rules_2g_ptr);
	qdf_mem_free(reg_info->reg_rules_5g_ptr);
	tgt_reg_mem_free_fcc_rules(reg_info);

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		qdf_mem_free(reg_info->reg_rules_6g_ap_ptr[i]);
		qdf_mem_free(reg_info->
			reg_rules_6g_client_ptr[i][REG_DEFAULT_CLIENT]);
		qdf_mem_free(reg_info->
			reg_rules_6g_client_ptr[i][REG_SUBORDINATE_CLIENT]);
	}

	qdf_mem_free(reg_info);

	TARGET_IF_EXIT();

	return ret_val;
}

/**
 * tgt_if_regulatory_register_master_list_ext_handler() - Register extended
 * master channel list event handler
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_register_master_list_ext_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_register_event_handler(
			wmi_handle, wmi_reg_chan_list_cc_ext_event_id,
			tgt_reg_chan_list_ext_update_handler, WMI_RX_WORK_CTX);
}

/**
 * tgt_if_regulatory_unregister_master_list_ext_handler() - Unregister extended
 * master channel list event handler
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_unregister_master_list_ext_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_unregister_event_handler(
			wmi_handle, wmi_reg_chan_list_cc_ext_event_id);
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * tgt_afc_event_handler() - Handler for AFC Event
 * @handle: scn handle
 * @event_buf: pointer to event buffer
 * @len: buffer length
 *
 * Return: 0 on success
 */
static int
tgt_afc_event_handler(ol_scn_t handle, uint8_t *event_buf, uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
	struct afc_regulatory_info *afc_info;
	QDF_STATUS status;
	struct wmi_unified *wmi_handle;
	int ret_val = 0;

	TARGET_IF_ENTER();

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		return -EINVAL;
	}

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return -EINVAL;
	}

	if (!reg_rx_ops->afc_event_handler) {
		target_if_err("afc_event_handler is NULL");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("invalid wmi handle");
		return -EINVAL;
	}

	afc_info = qdf_mem_malloc(sizeof(*afc_info));
	if (!afc_info)
		return -ENOMEM;

	status = wmi_extract_afc_event(wmi_handle, event_buf, afc_info, len);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		target_if_err("Extraction of AFC event failed");
		ret_val = -EFAULT;
		goto clean;
	}

	if (afc_info->phy_id >= PSOC_MAX_PHY_REG_CAP) {
		target_if_err_rl("phy_id %d is out of bounds",
				 afc_info->phy_id);
		ret_val = -EFAULT;
		goto clean;
	}

	afc_info->psoc = psoc;

	status = reg_rx_ops->afc_event_handler(afc_info);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		target_if_err("Failed to process AFC event handler");
		ret_val = -EFAULT;
		goto clean;
	}

clean:
	qdf_mem_free(afc_info);
	TARGET_IF_EXIT();

	return ret_val;
}

/**
 * tgt_if_regulatory_register_afc_event_handler() - Register AFC event
 * handler
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_register_afc_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_register_event_handler(
			wmi_handle, wmi_afc_event_id,
			tgt_afc_event_handler, WMI_RX_WORK_CTX);
}

/**
 * tgt_if_regulatory_unregister_afc_event_handler() - Unregister AFC event
 * handler
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
tgt_if_regulatory_unregister_afc_event_handler(struct wlan_objmgr_psoc *psoc,
					       void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_unregister_event_handler(
			wmi_handle, wmi_afc_event_id);
}
#endif
#endif

/**
 * tgt_if_regulatory_set_country_code() - Set country code
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_set_country_code(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_set_country_cmd_send(wmi_handle, arg);
}

/**
 * tgt_if_regulatory_set_user_country_code() - Set user country code
 * @psoc: Pointer to psoc
 * @pdev_id: Pdev id
 * @rd: Pointer to regdomain structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_set_user_country_code(
	struct wlan_objmgr_psoc *psoc, uint8_t pdev_id, struct cc_regdmn_s *rd)
{
	struct wlan_objmgr_pdev *pdev;
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id,
					  WLAN_REGULATORY_NB_ID);

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);

	if (!wmi_handle) {
		status = QDF_STATUS_E_FAILURE;
		goto free_pdevref;
	}

	status = wmi_unified_set_user_country_code_cmd_send(wmi_handle,
							    pdev_id, rd);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Set user country code failed,status %d",
			      status);
free_pdevref:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_NB_ID);

	return status;
}

QDF_STATUS tgt_if_regulatory_modify_freq_range(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap;

	reg_cap = ucfg_reg_get_hal_reg_cap(psoc);
	if (!reg_cap) {
		target_if_err("reg cap is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!(reg_cap->wireless_modes & HOST_REGDMN_MODE_11A)) {
		reg_cap->low_5ghz_chan = 0;
		reg_cap->high_5ghz_chan = 0;
	}

	if (!(reg_cap->wireless_modes &
	     (HOST_REGDMN_MODE_11B | HOST_REGDMN_MODE_PUREG))) {
		reg_cap->low_2ghz_chan = 0;
		reg_cap->high_2ghz_chan = 0;
	}

	target_if_debug("phy_id = %d - low_2ghz_chan = %d high_2ghz_chan = %d low_5ghz_chan = %d high_5ghz_chan = %d",
			reg_cap->phy_id,
			reg_cap->low_2ghz_chan,
			reg_cap->high_2ghz_chan,
			reg_cap->low_5ghz_chan,
			reg_cap->high_5ghz_chan);

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_REG_CLIENT
/**
 * tgt_if_regulatory_send_ctl_info() - Send CTL info to firmware
 * @psoc: Pointer to psoc
 * @params: Pointer to reg control params
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
tgt_if_regulatory_send_ctl_info(struct wlan_objmgr_psoc *psoc,
				struct reg_ctl_params *params)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_send_regdomain_info_to_fw_cmd(wmi_handle,
							 params->regd,
							 params->regd_2g,
							 params->regd_5g,
							 params->ctl_2g,
							 params->ctl_5g);
}
#else
static QDF_STATUS
tgt_if_regulatory_send_ctl_info(struct wlan_objmgr_psoc *psoc,
				struct reg_ctl_params *params)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * tgt_if_regulatory_get_phy_id_from_pdev_id() - Get phy_id from pdev_id
 * @psoc: Pointer to psoc
 * @pdev_id: Pdev id
 * @phy_id: phy_id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_get_phy_id_from_pdev_id(
	struct wlan_objmgr_psoc *psoc, uint8_t pdev_id, uint8_t *phy_id)
{
	struct target_psoc_info *tgt_if_handle = psoc->tgt_if_handle;
	uint8_t ret;

	if (pdev_id >= WLAN_UMAC_MAX_PDEVS) {
		target_if_err("pdev_id is greater than WLAN_UMAC_MAX_PDEVS");
		return QDF_STATUS_E_FAILURE;
	}

	/* By default pdev_id and phy_id have one to one mapping */
	*phy_id = pdev_id;

	if (!(tgt_if_handle &&
	      tgt_if_handle->info.is_pdevid_to_phyid_map))
		return QDF_STATUS_SUCCESS;

	ret = tgt_if_handle->info.pdev_id_to_phy_id_map[pdev_id];

	if (ret < PSOC_MAX_PHY_REG_CAP) {
		*phy_id = ret;
	} else {
		target_if_err("phy_id is greater than PSOC_MAX_PHY_REG_CAP");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * tgt_if_regulatory_get_pdev_id_from_phy_id() - Get pdev_id for phy_id
 * @psoc: Pointer to psoc
 * @phy_id: Phy id
 * @pdev_id: Pdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tgt_if_regulatory_get_pdev_id_from_phy_id(
	struct wlan_objmgr_psoc *psoc, uint8_t phy_id, uint8_t *pdev_id)
{
	struct target_psoc_info *tgt_if_handle = psoc->tgt_if_handle;
	uint8_t i;

	if (phy_id >= PSOC_MAX_PHY_REG_CAP) {
		target_if_err("phy_id is greater than PSOC_MAX_PHY_REG_CAP");
		return QDF_STATUS_E_FAILURE;
	}

	/* By default pdev_id and phy_id have one to one mapping */
	*pdev_id = phy_id;

	if (!(tgt_if_handle &&
	      tgt_if_handle->info.is_pdevid_to_phyid_map))
		return QDF_STATUS_SUCCESS;

	for (i = 0; i < WLAN_UMAC_MAX_PDEVS; i++) {
		if (tgt_if_handle->info.pdev_id_to_phy_id_map[i] == phy_id)
			break;
	}

	if (i < WLAN_UMAC_MAX_PDEVS) {
		*pdev_id = i;
	} else {
		target_if_err("pdev_id is greater than WLAN_UMAC_MAX_PDEVS");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_BAND_6GHZ
static void target_if_register_master_ext_handler(
				struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
	reg_ops->register_master_ext_handler =
		tgt_if_regulatory_register_master_list_ext_handler;

	reg_ops->unregister_master_ext_handler =
		tgt_if_regulatory_unregister_master_list_ext_handler;
}

#ifdef CONFIG_AFC_SUPPORT
static void target_if_register_afc_event_handler(
				struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
	reg_ops->register_afc_event_handler =
		tgt_if_regulatory_register_afc_event_handler;

	reg_ops->unregister_afc_event_handler =
		tgt_if_regulatory_unregister_afc_event_handler;
}

#ifdef CONFIG_REG_CLIENT
static void target_if_register_acs_trigger_for_afc
				(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
	reg_ops->trigger_acs_for_afc = tgt_afc_trigger_dcs;
}
#else
static void target_if_register_acs_trigger_for_afc
				(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
	reg_ops->trigger_acs_for_afc = NULL;
}
#endif
#else
static void target_if_register_afc_event_handler(
				struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
}

static void target_if_register_acs_trigger_for_afc
				(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
}
#endif
#else
static inline void
target_if_register_master_ext_handler(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
}

static void target_if_register_afc_event_handler(
				struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
}

static void target_if_register_acs_trigger_for_afc
				(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
}
#endif

static QDF_STATUS
tgt_if_regulatory_set_tpc_power(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				struct reg_tpc_power_info *param)
{
	wmi_unified_t wmi_handle;
	uint8_t pdev_id;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	pdev_id = wlan_get_pdev_id_from_vdev_id(psoc,
						vdev_id, WLAN_REGULATORY_NB_ID);

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id,
					  WLAN_REGULATORY_NB_ID);

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);

	if (!wmi_handle) {
		status = QDF_STATUS_E_FAILURE;
		goto free_pdevref;
	}
	status = wmi_unified_send_set_tpc_power_cmd(wmi_handle, vdev_id, param);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("send tpc power cmd failed, status: %d", status);

free_pdevref:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_NB_ID);

	return status;
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * tgt_if_regulatory_send_afc_cmd() - Send AFC command to the FW
 *
 * @psoc: Pointer to psoc
 * @pdev_id: Pdev id
 * @param: Pointer to hold AFC indication.
 *
 * Return: QDF_STATUS_SUCCESS if WMI_AFC_CMD is sent, else QDF_STATUS_E_FAILURE
 */
static QDF_STATUS
tgt_if_regulatory_send_afc_cmd(struct wlan_objmgr_psoc *psoc,
			       uint8_t pdev_id,
			       struct reg_afc_resp_rx_ind_info *param)
{
	struct wlan_objmgr_pdev *pdev;
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id,
					  WLAN_REGULATORY_NB_ID);

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);

	if (!wmi_handle) {
		status = QDF_STATUS_E_FAILURE;
		goto free_pdevref;
	}

	status = wmi_unified_send_afc_cmd(wmi_handle, pdev_id, param);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("send afc  cmd failed, status: %d", status);

free_pdevref:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_NB_ID);

	return status;
}

static void
tgt_if_register_afc_callback(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
	reg_ops->send_afc_ind = tgt_if_regulatory_send_afc_cmd;
	reg_ops->reg_get_min_psd = NULL;
	reg_ops->trigger_update_channel_list = NULL;
}
#else
static void
tgt_if_register_afc_callback(struct wlan_lmac_if_reg_tx_ops *reg_ops)
{
}
#endif

/**
 * tgt_if_regulatory_is_ext_tpc_supported() - Check if FW supports new
 * WMI command for TPC power
 *
 * @psoc: Pointer to psoc
 *
 * Return: true if FW supports new WMI command for TPC, else false
 */
static bool
tgt_if_regulatory_is_ext_tpc_supported(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle,
				   wmi_service_ext_tpc_reg_support);
}

QDF_STATUS target_if_regulatory_set_ext_tpc(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_ext_tpc_supported)
		reg_rx_ops->reg_set_ext_tpc_supported(
			psoc,
			tgt_if_regulatory_is_ext_tpc_supported(psoc));

	return QDF_STATUS_SUCCESS;
}

#if defined(CONFIG_BAND_6GHZ) && defined(CONFIG_AFC_SUPPORT)
void tgt_if_set_reg_afc_configure(struct target_psoc_info *tgt_hdl,
				  struct wlan_objmgr_psoc *psoc)
{
	struct tgt_info *info;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle || !tgt_hdl)
		return;

	if (!wmi_service_enabled(wmi_handle, wmi_service_afc_support)) {
		target_if_err("service afc support not enable");
		return;
	}

	info = (&tgt_hdl->info);

	info->wlan_res_cfg.is_6ghz_sp_pwrmode_supp_enabled =
		ucfg_reg_get_enable_6ghz_sp_mode_support(psoc);
	info->wlan_res_cfg.afc_timer_check_disable =
		ucfg_reg_get_afc_disable_timer_check(psoc);
	info->wlan_res_cfg.afc_req_id_check_disable =
		ucfg_reg_get_afc_disable_request_id_check(psoc);
}
#endif

#if defined(CONFIG_BAND_6GHZ)
/**
 * tgt_if_regulatory_is_lower_6g_edge_ch_supp() - Check if lower 6 GHz
 * edge channel (5935 MHz) is supported
 * @psoc: Pointer to psoc
 *
 * Return: true if channel is supported, else false
 */
static bool
tgt_if_regulatory_is_lower_6g_edge_ch_supp(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle,
				   wmi_service_lower_6g_edge_ch_supp);
}

/**
 * tgt_if_regulatory_is_upper_6g_edge_ch_disabled() - Check if upper
 * 6 GHz edge channel (7115 MHz) is disabled
 * @psoc: Pointer to psoc
 *
 * Return: true if channel is disabled, else false
 */
static bool
tgt_if_regulatory_is_upper_6g_edge_ch_disabled(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle,
				   wmi_service_disable_upper_6g_edge_ch_supp);
}

QDF_STATUS
target_if_reg_set_lower_6g_edge_ch_info(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_lower_6g_edge_ch_supp)
		reg_rx_ops->reg_set_lower_6g_edge_ch_supp(
			psoc,
			tgt_if_regulatory_is_lower_6g_edge_ch_supp(psoc));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_reg_set_disable_upper_6g_edge_ch_info(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_disable_upper_6g_edge_ch_supp)
		reg_rx_ops->reg_set_disable_upper_6g_edge_ch_supp(
			psoc,
			tgt_if_regulatory_is_upper_6g_edge_ch_disabled(psoc));

	return QDF_STATUS_SUCCESS;
}
#else
static inline bool
tgt_if_regulatory_is_lower_6g_edge_ch_supp(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool
tgt_if_regulatory_is_upper_6g_edge_ch_disabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

#if defined(CONFIG_AFC_SUPPORT)
QDF_STATUS
target_if_reg_set_afc_dev_type(struct wlan_objmgr_psoc *psoc,
			       struct target_psoc_info *tgt_hdl)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
	struct tgt_info *info;

	if (!tgt_hdl) {
		target_if_err("target_psoc_info is null");
		return QDF_STATUS_E_FAILURE;
	}

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	info = (&tgt_hdl->info);

	if (reg_rx_ops->reg_set_afc_dev_type)
		reg_rx_ops->reg_set_afc_dev_type(
			psoc,
			info->service_ext2_param.afc_dev_type);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_reg_get_afc_dev_type(struct wlan_objmgr_psoc *psoc,
			       enum reg_afc_dev_deploy_type *reg_afc_dev_type)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_get_afc_dev_type)
		reg_rx_ops->reg_get_afc_dev_type(
			psoc,
			reg_afc_dev_type);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_regulatory_is_eirp_preferred_support() - Check if FW prefers EIRP
 * support for TPC power command.
 * @psoc: Pointer to psoc
 *
 * Return: true if FW prefers EIRP format for TPC, else false
 */
static bool
target_if_regulatory_is_eirp_preferred_support(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return false;

	return wmi_service_enabled(wmi_handle,
				   wmi_service_eirp_preferred_support);
}

QDF_STATUS
target_if_set_regulatory_eirp_preferred_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rx_ops->reg_set_eirp_preferred_support)
		reg_rx_ops->reg_set_eirp_preferred_support(
			psoc,
			target_if_regulatory_is_eirp_preferred_support(psoc));

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * tgt_if_reg_is_chip_11be_cap() - Finds out if the hardware is capable
 * of 11BE. The capability bit is read from mac_phy_cap populated by the
 * FW per pdev.
 * @psoc: Pointer to psoc
 * @phy_id: phy_id
 *
 * Return: True if chip is 11BE capable, false otherwise.
 */
#ifdef WLAN_FEATURE_11BE
static bool tgt_if_reg_is_chip_11be_cap(struct wlan_objmgr_psoc *psoc,
					uint16_t phy_id)
{
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap_arr, *mac_phy_cap;
	struct target_psoc_info *tgt_hdl;
	uint8_t pdev_id;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;

	reg_tx_ops = target_if_regulatory_get_tx_ops(psoc);

	if (!reg_tx_ops) {
		target_if_err("reg_tx_ops is NULL");
		return false;
	}

	if (reg_tx_ops->get_pdev_id_from_phy_id)
		reg_tx_ops->get_pdev_id_from_phy_id(psoc, phy_id, &pdev_id);
	else
		pdev_id = phy_id;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (tgt_hdl) {
		mac_phy_cap_arr = target_psoc_get_mac_phy_cap(tgt_hdl);
		if (!mac_phy_cap_arr)
			return false;
		mac_phy_cap = &mac_phy_cap_arr[pdev_id];
		if (mac_phy_cap && mac_phy_cap->supports_11be)
			return true;
	}
	return false;
}
#else
static bool tgt_if_reg_is_chip_11be_cap(struct wlan_objmgr_psoc *psoc,
					uint16_t phy_id)
{
	return false;
}
#endif

static int
tgt_rate_to_power_complete_handler(ol_scn_t handle, uint8_t *event_buf,
				   uint32_t len)
{
	int ret_val = 0;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
	QDF_STATUS status;
	struct wmi_unified *wmi_handle;
	struct r2p_table_update_status_obj *update_status_obj = NULL;
	uint32_t pdev_id;

	TARGET_IF_ENTER();

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		ret_val = -EINVAL;
		goto clean;
	}

	reg_rx_ops = target_if_regulatory_get_rx_ops(psoc);
	if (!reg_rx_ops) {
		target_if_err("reg_rx_ops is NULL");
		ret_val = -EINVAL;
		goto clean;
	}

	if (!reg_rx_ops->reg_r2p_table_update_response_handler) {
		target_if_err("reg_r2p_table_update_response_handler is NULL");
		ret_val = -EINVAL;
		goto clean;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("invalid wmi handle");
		ret_val = -EINVAL;
		goto clean;
	}

	update_status_obj = qdf_mem_malloc(sizeof(*update_status_obj));
	if (!update_status_obj) {
		ret_val = -ENOMEM;
		goto clean;
	}

	status = wmi_extract_tgtr2p_table_event(wmi_handle, event_buf,
						update_status_obj, len);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		target_if_err("Extraction of r2p update response event failed");
		ret_val = -EFAULT;
		goto clean;
	}

	pdev_id = update_status_obj->pdev_id;
	status = reg_rx_ops->reg_r2p_table_update_response_handler(psoc,
								   pdev_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		target_if_err("Failed to process r2p update response");
		ret_val = -EFAULT;
		goto clean;
	}
clean:
	qdf_mem_free(update_status_obj);
	TARGET_IF_EXIT();

	return ret_val;
}

/**
 * tgt_if_regulatory_register_rate2power_table_update_handler() - Register
 * rate2power table update handler
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
tgt_if_regulatory_register_rate2power_table_update_handler(
						struct wlan_objmgr_psoc *psoc,
						void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_register_event_handler(
			wmi_handle, wmi_pdev_set_tgtr2p_table_eventid,
			tgt_rate_to_power_complete_handler, WMI_RX_WORK_CTX);
}

/**
 * tgt_if_regulatory_unregister_rate2power_table_update_handler() - Unregister
 * rate2power table update handler
 * @psoc: Pointer to psoc
 * @arg: Pointer to argument list
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
tgt_if_regulatory_unregister_rate2power_table_update_handler(
						struct wlan_objmgr_psoc *psoc,
						void *arg)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_unregister_event_handler(
			wmi_handle, wmi_pdev_set_tgtr2p_table_eventid);
}

QDF_STATUS target_if_register_regulatory_tx_ops(
		struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_reg_tx_ops *reg_ops = &tx_ops->reg_ops;

	reg_ops->register_master_handler =
		tgt_if_regulatory_register_master_list_handler;

	reg_ops->unregister_master_handler =
		tgt_if_regulatory_unregister_master_list_handler;

	target_if_register_master_ext_handler(reg_ops);

	target_if_register_afc_event_handler(reg_ops);

	reg_ops->set_country_code = tgt_if_regulatory_set_country_code;

	reg_ops->fill_umac_legacy_chanlist = NULL;

	reg_ops->set_wait_for_init_cc_response_event = NULL;

	target_if_register_acs_trigger_for_afc(reg_ops);

	reg_ops->register_11d_new_cc_handler =
		tgt_if_regulatory_register_11d_new_cc_handler;

	reg_ops->unregister_11d_new_cc_handler =
		tgt_if_regulatory_unregister_11d_new_cc_handler;

	reg_ops->start_11d_scan = tgt_if_regulatory_start_11d_scan;

	reg_ops->stop_11d_scan = tgt_if_regulatory_stop_11d_scan;

	reg_ops->is_there_serv_ready_extn =
		tgt_if_regulatory_is_there_serv_ready_extn;

	reg_ops->set_user_country_code =
		tgt_if_regulatory_set_user_country_code;

	reg_ops->register_ch_avoid_event_handler =
		tgt_if_regulatory_register_ch_avoid_event_handler;

	reg_ops->unregister_ch_avoid_event_handler =
		tgt_if_regulatory_unregister_ch_avoid_event_handler;

	reg_ops->send_ctl_info = tgt_if_regulatory_send_ctl_info;

	reg_ops->get_phy_id_from_pdev_id =
			tgt_if_regulatory_get_phy_id_from_pdev_id;

	reg_ops->get_pdev_id_from_phy_id =
			tgt_if_regulatory_get_pdev_id_from_phy_id;

	reg_ops->set_tpc_power = tgt_if_regulatory_set_tpc_power;

	reg_ops->get_opclass_tbl_idx = NULL;

	tgt_if_register_afc_callback(reg_ops);

	reg_ops->is_chip_11be = tgt_if_reg_is_chip_11be_cap;

	reg_ops->register_rate2power_table_update_event_handler =
		tgt_if_regulatory_register_rate2power_table_update_handler;

	reg_ops->unregister_rate2power_table_update_event_handler =
		tgt_if_regulatory_unregister_rate2power_table_update_handler;

	reg_ops->end_r2p_table_update_wait = NULL;

	reg_ops->is_80p80_supported = NULL;

	reg_ops->is_freq_80p80_supported = NULL;

	return QDF_STATUS_SUCCESS;
}

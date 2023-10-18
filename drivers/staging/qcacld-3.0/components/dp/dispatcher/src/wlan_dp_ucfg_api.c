/*
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
 * DOC: public API related to the DP called by north bound HDD/OSIF
 */

#include "wlan_dp_ucfg_api.h"
#include "wlan_ipa_ucfg_api.h"
#include "wlan_dp_main.h"
#include "wlan_dp_objmgr.h"
#include "wlan_pmo_obj_mgmt_api.h"
#include "cdp_txrx_cmn.h"
#include "cfg_ucfg_api.h"
#include "wlan_pmo_obj_mgmt_api.h"
#include "wlan_dp_objmgr.h"
#include "wlan_dp_bus_bandwidth.h"
#include "wlan_dp_periodic_sta_stats.h"
#include "wlan_dp_nud_tracking.h"
#include "wlan_dp_txrx.h"
#include "wlan_nlink_common.h"
#include "wlan_pkt_capture_api.h"
#include <cdp_txrx_ctrl.h>
#include <qdf_net_stats.h>
#include "wlan_dp_prealloc.h"
#include "wlan_dp_rx_thread.h"

#ifdef FEATURE_DIRECT_LINK
/**
 * wlan_dp_set_vdev_direct_link_cfg() - Set direct link config in DP vdev
 * @psoc: objmgr psoc handle
 * @dp_intf: pointer to DP component interface handle
 *
 * Return: direct link configuration
 */
static inline
QDF_STATUS wlan_dp_set_vdev_direct_link_cfg(struct wlan_objmgr_psoc *psoc,
					    struct wlan_dp_intf *dp_intf)
{
	cdp_config_param_type vdev_param = {0};

	if (dp_intf->device_mode != QDF_SAP_MODE ||
	    !dp_intf->dp_ctx->dp_direct_link_ctx)
		return QDF_STATUS_SUCCESS;

	vdev_param.cdp_vdev_tx_to_fw = dp_intf->direct_link_config.config_set;

	return cdp_txrx_set_vdev_param(wlan_psoc_get_dp_handle(psoc),
				       dp_intf->intf_id, CDP_VDEV_TX_TO_FW,
				       vdev_param);
}
#else
static inline
QDF_STATUS wlan_dp_set_vdev_direct_link_cfg(struct wlan_objmgr_psoc *psoc,
					    struct wlan_dp_intf *dp_intf)
{
	return QDF_STATUS_SUCCESS;
}
#endif

void ucfg_dp_update_inf_mac(struct wlan_objmgr_psoc *psoc,
			    struct qdf_mac_addr *cur_mac,
			    struct qdf_mac_addr *new_mac)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_psoc_get_priv(psoc);

	dp_intf = dp_get_intf_by_macaddr(dp_ctx, cur_mac);
	if (!dp_intf) {
		dp_err("DP interface not found addr:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(cur_mac->bytes));
		QDF_BUG(0);
		return;
	}

	dp_info("MAC update from " QDF_MAC_ADDR_FMT " to " QDF_MAC_ADDR_FMT "",
		QDF_MAC_ADDR_REF(cur_mac->bytes),
		QDF_MAC_ADDR_REF(new_mac->bytes));

	qdf_copy_macaddr(&dp_intf->mac_addr, new_mac);

	wlan_dp_set_vdev_direct_link_cfg(psoc, dp_intf);
}

QDF_STATUS
ucfg_dp_create_intf(struct wlan_objmgr_psoc *psoc,
		    struct qdf_mac_addr *intf_addr,
		    qdf_netdev_t ndev)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_get_context();

	dp_info("DP interface create addr:" QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(intf_addr->bytes));

	dp_intf = __qdf_mem_malloc(sizeof(*dp_intf), __func__, __LINE__);
	if (!dp_intf) {
		dp_err("DP intf memory alloc failed addr:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(intf_addr->bytes));
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf->dp_ctx = dp_ctx;
	dp_intf->dev = ndev;
	dp_intf->intf_id = WLAN_UMAC_VDEV_ID_MAX;
	qdf_copy_macaddr(&dp_intf->mac_addr, intf_addr);
	qdf_spinlock_create(&dp_intf->vdev_lock);

	qdf_spin_lock_bh(&dp_ctx->intf_list_lock);
	qdf_list_insert_front(&dp_ctx->intf_list, &dp_intf->node);
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);

	dp_periodic_sta_stats_init(dp_intf);
	dp_periodic_sta_stats_mutex_create(dp_intf);
	dp_nud_init_tracking(dp_intf);
	dp_mic_init_work(dp_intf);
	qdf_atomic_init(&dp_ctx->num_latency_critical_clients);
	qdf_atomic_init(&dp_intf->gro_disallowed);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_dp_destroy_intf(struct wlan_objmgr_psoc *psoc,
		     struct qdf_mac_addr *intf_addr)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_get_context();

	dp_info("DP interface destroy addr:" QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(intf_addr->bytes));

	dp_intf = dp_get_intf_by_macaddr(dp_ctx, intf_addr);
	if (!dp_intf) {
		dp_err("DP interface not found addr:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(intf_addr->bytes));
		return QDF_STATUS_E_FAILURE;
	}

	if (dp_intf->device_mode == QDF_SAP_MODE)
		dp_config_direct_link(dp_intf, false, false);

	dp_periodic_sta_stats_mutex_destroy(dp_intf);
	dp_nud_deinit_tracking(dp_intf);
	dp_mic_deinit_work(dp_intf);
	qdf_spinlock_destroy(&dp_intf->vdev_lock);

	qdf_spin_lock_bh(&dp_ctx->intf_list_lock);
	qdf_list_remove_node(&dp_ctx->intf_list, &dp_intf->node);
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);

	__qdf_mem_free(dp_intf);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_init(void)
{
	QDF_STATUS status;

	dp_info("DP module dispatcher init");

	if (dp_allocate_ctx() != QDF_STATUS_SUCCESS) {
		dp_err("DP ctx allocation failed");
		return QDF_STATUS_E_FAULT;
	}

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_COMP_DP,
			dp_psoc_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to register psoc create handler for DP");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_COMP_DP,
			dp_psoc_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to register psoc destroy handler for DP");
		goto fail_destroy_psoc;
	}

	status = wlan_objmgr_register_pdev_create_handler(
			WLAN_COMP_DP,
			dp_pdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to register pdev create handler for DP");
		goto fail_create_pdev;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(
			WLAN_COMP_DP,
			dp_pdev_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to register pdev destroy handler for DP");
		goto fail_destroy_pdev;
	}

	status = wlan_objmgr_register_vdev_create_handler(
			WLAN_COMP_DP,
			dp_vdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to register vdev create handler");
		goto fail_create_vdev;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
			WLAN_COMP_DP,
			dp_vdev_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to register vdev destroy handler");
		goto fail_destroy_vdev;
	}

	status = wlan_objmgr_register_peer_create_handler(
		WLAN_COMP_DP,
		dp_peer_obj_create_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("wlan_objmgr_register_peer_create_handler failed");
		goto fail_create_peer;
	}

	status = wlan_objmgr_register_peer_destroy_handler(
		WLAN_COMP_DP,
		dp_peer_obj_destroy_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("wlan_objmgr_register_peer_destroy_handler failed");
	else
		return QDF_STATUS_SUCCESS;

	wlan_objmgr_unregister_peer_create_handler(WLAN_COMP_DP,
					dp_peer_obj_create_notification,
					NULL);

fail_create_peer:
	wlan_objmgr_unregister_vdev_destroy_handler(WLAN_COMP_DP,
					dp_vdev_obj_destroy_notification,
					NULL);

fail_destroy_vdev:
	wlan_objmgr_unregister_vdev_create_handler(
				WLAN_COMP_DP,
				dp_vdev_obj_create_notification, NULL);

fail_create_vdev:
	wlan_objmgr_unregister_pdev_destroy_handler(
				WLAN_COMP_DP,
				dp_pdev_obj_destroy_notification, NULL);

fail_destroy_pdev:
	wlan_objmgr_unregister_pdev_create_handler(
				WLAN_COMP_DP,
				dp_pdev_obj_create_notification, NULL);

fail_create_pdev:
	wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_COMP_DP,
				dp_psoc_obj_destroy_notification, NULL);
fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(
				WLAN_COMP_DP,
				dp_psoc_obj_create_notification, NULL);

	dp_free_ctx();
	return status;
}

QDF_STATUS ucfg_dp_deinit(void)
{
	QDF_STATUS status;

	dp_info("DP module dispatcher deinit");

	/* de-register peer delete handler functions. */
	status = wlan_objmgr_unregister_peer_destroy_handler(
				WLAN_COMP_DP,
				dp_peer_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister DP peer destroy handler: %d", status);

	/* de-register peer create handler functions. */
	status = wlan_objmgr_unregister_peer_create_handler(
				WLAN_COMP_DP,
				dp_peer_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister DP peer create handler: %d", status);

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_COMP_DP,
				dp_vdev_obj_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister vdev delete handler:%d", status);

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_COMP_DP,
				dp_vdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister vdev create handler:%d", status);

	status = wlan_objmgr_unregister_pdev_destroy_handler(
				WLAN_COMP_DP,
				dp_pdev_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister pdev destroy handler:%d", status);

	status = wlan_objmgr_unregister_pdev_create_handler(
				WLAN_COMP_DP,
				dp_pdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister pdev create handler:%d", status);

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_COMP_DP,
				dp_psoc_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister DP psoc delete handle:%d", status);

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_COMP_DP,
				dp_psoc_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("Failed to unregister DP psoc create handle:%d", status);

	dp_free_ctx();

	return status;
}

/**
 * ucfg_dp_suspend_handler() - suspend handler registered with PMO component
 * @psoc: psoc handle
 * @arg: Arguments passed by the suspend handler.
 *
 * This handler is used to update the wiphy suspend state in DP context
 *
 * Return: QDF_STATUS status -in case of success else return error
 */
static QDF_STATUS
ucfg_dp_suspend_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_dp_intf *dp_intf, *dp_intf_next = NULL;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}
	if (dp_ctx->enable_dp_rx_threads) {
		status = dp_txrx_suspend(cds_get_context(QDF_MODULE_ID_SOC));

		if (status != QDF_STATUS_SUCCESS) {
			dp_txrx_resume(cds_get_context(QDF_MODULE_ID_SOC));
			return status;
			}
	}
	dp_ctx->is_suspend = true;
	cdp_set_tx_pause(soc, true);
	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		dp_intf->sap_tx_block_mask |= WLAN_DP_SUSPEND;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_dp_resume_handler() - resume handler registered with PMO component
 * @psoc: psoc handle
 * @arg: Arguments passed by the resume handler.
 *
 * This handler is used to update the wiphy resume state in DP context
 *
 * Return: QDF_STATUS status -in case of success else return error
 */
static QDF_STATUS
ucfg_dp_resume_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_dp_intf *dp_intf, *dp_intf_next = NULL;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}

	dp_ctx->is_suspend = false;
	cdp_set_tx_pause(soc, false);
	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		dp_intf->sap_tx_block_mask &= ~WLAN_DP_SUSPEND;
	}
	if (dp_ctx->enable_dp_rx_threads)
		dp_txrx_resume(cds_get_context(QDF_MODULE_ID_SOC));
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_register_pmo_handler() - register suspend and resume handler
 * with PMO component
 *
 * Return: None
 */
static inline void dp_register_pmo_handler(void)
{
	pmo_register_suspend_handler(WLAN_COMP_DP,
				     ucfg_dp_suspend_handler, NULL);

	pmo_register_resume_handler(WLAN_COMP_DP,
				    ucfg_dp_resume_handler, NULL);
}

/**
 * dp_unregister_pmo_handler() - unregister suspend and resume handler
 * with PMO component
 *
 * Return: None
 */
static inline void dp_unregister_pmo_handler(void)
{
	pmo_unregister_suspend_handler(WLAN_COMP_DP, ucfg_dp_suspend_handler);

	pmo_unregister_resume_handler(WLAN_COMP_DP, ucfg_dp_resume_handler);
}

/**
 * ucfg_dp_store_qdf_dev() - Store qdf device instance in DP component
 * @psoc: psoc handle
 *
 * Return: QDF_STATUS status -in case of success else return error
 */
static inline QDF_STATUS
ucfg_dp_store_qdf_dev(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}

	dp_ctx->qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!dp_ctx->qdf_dev) {
		dp_err("QDF_DEV is NULL");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}

	ucfg_dp_store_qdf_dev(psoc);
	dp_rtpm_tput_policy_init(psoc);
	dp_register_pmo_handler();
	dp_trace_init(psoc);
	dp_bus_bandwidth_init(psoc);
	qdf_wake_lock_create(&dp_ctx->rx_wake_lock, "qcom_rx_wakelock");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}

	dp_rtpm_tput_policy_deinit(psoc);
	dp_unregister_pmo_handler();
	dp_bus_bandwidth_deinit(psoc);
	qdf_wake_lock_destroy(&dp_ctx->rx_wake_lock);

	return QDF_STATUS_SUCCESS;
}

void ucfg_dp_suspend_wlan(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return;
	}

	dp_ctx->is_wiphy_suspended = true;
}

void ucfg_dp_resume_wlan(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return;
	}

	dp_ctx->is_wiphy_suspended = false;
}

void ucfg_dp_wait_complete_tasks(void)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_get_context();
	dp_wait_complete_tasks(dp_ctx);
}

/*
 * During connect/disconnect this needs to be updated
 */

void ucfg_dp_remove_conn_info(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	qdf_mem_zero(&dp_intf->conn_info,
		     sizeof(struct wlan_dp_conn_info));
}

void ucfg_dp_conn_info_set_bssid(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *bssid)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	qdf_copy_macaddr(&dp_intf->conn_info.bssid, bssid);
}

void ucfg_dp_conn_info_set_arp_service(struct wlan_objmgr_vdev *vdev,
				       uint8_t proxy_arp_service)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	dp_intf->conn_info.proxy_arp_service = proxy_arp_service;
}

void ucfg_dp_conn_info_set_peer_authenticate(struct wlan_objmgr_vdev *vdev,
					     uint8_t is_authenticated)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	dp_intf->conn_info.is_authenticated = is_authenticated;
}

void ucfg_dp_conn_info_set_peer_mac(struct wlan_objmgr_vdev *vdev,
				    struct qdf_mac_addr *peer_mac)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	qdf_copy_macaddr(&dp_intf->conn_info.peer_macaddr, peer_mac);
}

void ucfg_dp_softap_check_wait_for_tx_eap_pkt(struct wlan_objmgr_vdev *vdev,
					      struct qdf_mac_addr *mac_addr)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	dp_softap_check_wait_for_tx_eap_pkt(dp_intf, mac_addr);
}

void ucfg_dp_update_dhcp_state_on_disassoc(struct wlan_objmgr_vdev *vdev,
					   struct qdf_mac_addr *mac_addr)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_objmgr_peer *peer;
	struct wlan_dp_sta_info *stainfo;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	peer = wlan_objmgr_get_peer_by_mac(dp_intf->dp_ctx->psoc,
					   mac_addr->bytes,
					   WLAN_DP_ID);
	if (!peer) {
		dp_err("Peer object not found mac:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(mac_addr->bytes));
		return;
	}

	stainfo = dp_get_peer_priv_obj(peer);
	if (!stainfo) {
		wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
		return;
	}

	/* Send DHCP STOP indication to FW */
	stainfo->dhcp_phase = DHCP_PHASE_ACK;
	if (stainfo->dhcp_nego_status == DHCP_NEGO_IN_PROGRESS)
		dp_post_dhcp_ind(dp_intf,
				 stainfo->sta_mac.bytes,
				 0);
	stainfo->dhcp_nego_status = DHCP_NEGO_STOP;
	wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
}

void ucfg_dp_set_dfs_cac_tx(struct wlan_objmgr_vdev *vdev, bool tx_block)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	if (tx_block)
		dp_intf->sap_tx_block_mask |= DP_TX_DFS_CAC_BLOCK;
	else
		dp_intf->sap_tx_block_mask &= ~DP_TX_DFS_CAC_BLOCK;
}

void ucfg_dp_set_bss_state_start(struct wlan_objmgr_vdev *vdev, bool start)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}

	if (start) {
		dp_intf->sap_tx_block_mask &= ~DP_TX_SAP_STOP;
		dp_intf->bss_state = BSS_INTF_START;
	} else {
		dp_intf->sap_tx_block_mask |= DP_TX_SAP_STOP;
		dp_intf->bss_state = BSS_INTF_STOP;
	}
}

QDF_STATUS ucfg_dp_lro_set_reset(struct wlan_objmgr_vdev *vdev,
				 uint8_t enable_flag)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	return dp_lro_set_reset(dp_intf, enable_flag);
}

bool ucfg_dp_is_ol_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return 0;
	}

	return dp_ctx->ol_enable;
}

#ifdef RECEIVE_OFFLOAD
void ucfg_dp_rx_handle_concurrency(struct wlan_objmgr_psoc *psoc,
				   bool disable)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return;
	}

	if (disable) {
		if (DP_BUS_BW_CFG(dp_ctx->dp_cfg.enable_tcp_delack)) {
			struct wlan_rx_tp_data rx_tp_data;

			dp_info("Enable TCP delack as LRO disabled in concurrency");
			rx_tp_data.rx_tp_flags = TCP_DEL_ACK_IND;
			rx_tp_data.level =
				DP_BUS_BW_GET_RX_LVL(dp_ctx);
			wlan_dp_update_tcp_rx_param(dp_ctx, &rx_tp_data);
			dp_ctx->en_tcp_delack_no_lro = 1;
		}
		qdf_atomic_set(&dp_ctx->disable_rx_ol_in_concurrency, 1);
	} else {
		if (DP_BUS_BW_CFG(dp_ctx->dp_cfg.enable_tcp_delack)) {
			dp_info("Disable TCP delack as LRO is enabled");
			dp_ctx->en_tcp_delack_no_lro = 0;
			dp_reset_tcp_delack(psoc);
		}
		qdf_atomic_set(&dp_ctx->disable_rx_ol_in_concurrency, 0);
	}
}

QDF_STATUS ucfg_dp_rx_ol_init(struct wlan_objmgr_psoc *psoc,
			      bool is_wifi3_0_target)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_INVAL;
	}

	return dp_rx_ol_init(dp_ctx, is_wifi3_0_target);
}
#else /* RECEIVE_OFFLOAD */

QDF_STATUS ucfg_dp_rx_ol_init(struct wlan_objmgr_psoc *psoc,
			      bool is_wifi3_0_target)
{
	dp_err("Rx_OL, LRO/GRO not supported");
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

bool ucfg_dp_is_rx_common_thread_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_INVAL;
	}

	return dp_ctx->enable_rxthread;
}

bool ucfg_dp_is_rx_threads_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_INVAL;
	}

	return dp_ctx->enable_dp_rx_threads;
}

#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
/**
 * dp_get_config_rx_softirq_limits() - Update DP rx softirq limit config
 *                          datapath
 * @psoc: psoc handle
 * @params: DP Configuration parameters
 *
 * Return: None
 */
static
void dp_get_config_rx_softirq_limits(struct wlan_objmgr_psoc *psoc,
				     struct cdp_config_params *params)
{
	params->tx_comp_loop_pkt_limit = cfg_get(psoc,
						 CFG_DP_TX_COMP_LOOP_PKT_LIMIT);
	params->rx_reap_loop_pkt_limit = cfg_get(psoc,
						 CFG_DP_RX_REAP_LOOP_PKT_LIMIT);
	params->rx_hp_oos_update_limit = cfg_get(psoc,
						 CFG_DP_RX_HP_OOS_UPDATE_LIMIT);
}
#else
static
void dp_get_config_rx_softirq_limits(struct wlan_objmgr_psoc *psoc,
				     struct cdp_config_params *params)
{
}
#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/**
 * dp_get_config_queue_threshold() - Update DP tx flow limit config
 *                          datapath
 * @psoc: psoc handle
 * @params: DP Configuration parameters
 *
 * Return: None
 */
static void
dp_get_config_queue_threshold(struct wlan_objmgr_psoc *psoc,
			      struct cdp_config_params *params)
{
	params->tx_flow_stop_queue_threshold =
			cfg_get(psoc, CFG_DP_TX_FLOW_STOP_QUEUE_TH);
	params->tx_flow_start_queue_offset =
			cfg_get(psoc, CFG_DP_TX_FLOW_START_QUEUE_OFFSET);
}
#else
static inline void
dp_get_config_queue_threshold(struct wlan_objmgr_psoc *psoc,
			      struct cdp_config_params *params)
{
}
#endif

QDF_STATUS
ucfg_dp_update_config(struct wlan_objmgr_psoc *psoc,
		      struct wlan_dp_user_config *req)
{
	struct cdp_config_params params = {0};
	struct wlan_dp_psoc_context *dp_ctx;
	QDF_STATUS status;
	void *soc;

	dp_ctx =  dp_psoc_get_priv(psoc);

	dp_ctx->arp_connectivity_map = req->arp_connectivity_map;
	soc = cds_get_context(QDF_MODULE_ID_SOC);
	params.tso_enable = cfg_get(psoc, CFG_DP_TSO);
	dp_ctx->dp_cfg.lro_enable = cfg_get(psoc, CFG_DP_LRO);
	params.lro_enable = dp_ctx->dp_cfg.lro_enable;

	dp_get_config_queue_threshold(psoc, &params);
	params.flow_steering_enable =
		cfg_get(psoc, CFG_DP_FLOW_STEERING_ENABLED);
	params.napi_enable = dp_ctx->napi_enable;
	params.p2p_tcp_udp_checksumoffload =
		cfg_get(psoc, CFG_DP_P2P_TCP_UDP_CKSUM_OFFLOAD);
	params.nan_tcp_udp_checksumoffload =
		cfg_get(psoc, CFG_DP_NAN_TCP_UDP_CKSUM_OFFLOAD);
	params.tcp_udp_checksumoffload =
		cfg_get(psoc, CFG_DP_TCP_UDP_CKSUM_OFFLOAD);
	params.ipa_enable = req->ipa_enable;
	dp_ctx->dp_cfg.gro_enable = cfg_get(psoc, CFG_DP_GRO);
	params.gro_enable = dp_ctx->dp_cfg.gro_enable;
	params.tx_comp_loop_pkt_limit = cfg_get(psoc,
						CFG_DP_TX_COMP_LOOP_PKT_LIMIT);
	params.rx_reap_loop_pkt_limit = cfg_get(psoc,
						CFG_DP_RX_REAP_LOOP_PKT_LIMIT);
	params.rx_hp_oos_update_limit = cfg_get(psoc,
						CFG_DP_RX_HP_OOS_UPDATE_LIMIT);
	dp_get_config_rx_softirq_limits(psoc, &params);

	status = cdp_update_config_parameters(soc, &params);
	if (status) {
		dp_err("Failed to attach config parameters");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

uint64_t
ucfg_dp_get_rx_softirq_yield_duration(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	return dp_ctx->dp_cfg.rx_softirq_max_yield_duration_ns;
}

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * dp_rx_register_fisa_ops() - FISA callback functions
 * @txrx_ops: operations handle holding callback functions
 *
 * Return: None
 */
static inline void
dp_rx_register_fisa_ops(struct ol_txrx_ops *txrx_ops)
{
	txrx_ops->rx.osif_fisa_rx = wlan_dp_rx_fisa_cbk;
	txrx_ops->rx.osif_fisa_flush = wlan_dp_rx_fisa_flush_by_ctx_id;
}
#else
static inline void
dp_rx_register_fisa_ops(struct ol_txrx_ops *txrx_ops)
{
}
#endif

#ifdef CONFIG_DP_PKT_ADD_TIMESTAMP
static QDF_STATUS wlan_dp_get_tsf_time(void *dp_intf_ctx,
				       uint64_t input_time,
				       uint64_t *tsf_time)
{
	struct wlan_dp_intf *dp_intf = (struct wlan_dp_intf *)dp_intf_ctx;
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_intf->dp_ctx->dp_ops;

	dp_ops->dp_get_tsf_time(dp_intf->intf_id,
				input_time,
				tsf_time);
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS wlan_dp_get_tsf_time(void *dp_intf_ctx,
				       uint64_t input_time,
				       uint64_t *tsf_time)
{
	*tsf_time = 0;
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

QDF_STATUS ucfg_dp_sta_register_txrx_ops(struct wlan_objmgr_vdev *vdev)
{
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	/* Register the vdev transmit and receive functions */
	qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));

	if (dp_intf->dp_ctx->enable_dp_rx_threads) {
		txrx_ops.rx.rx = dp_rx_pkt_thread_enqueue_cbk;
		txrx_ops.rx.rx_stack = dp_rx_packet_cbk;
		txrx_ops.rx.rx_flush = dp_rx_flush_packet_cbk;
		txrx_ops.rx.rx_gro_flush = dp_rx_thread_gro_flush_ind_cbk;
		dp_intf->rx_stack = dp_rx_packet_cbk;
	} else {
		txrx_ops.rx.rx = dp_rx_packet_cbk;
		txrx_ops.rx.rx_stack = NULL;
		txrx_ops.rx.rx_flush = NULL;
	}

	if (dp_intf->dp_ctx->dp_cfg.fisa_enable &&
		(dp_intf->device_mode != QDF_MONITOR_MODE)) {
		dp_debug("FISA feature enabled");
		dp_rx_register_fisa_ops(&txrx_ops);
	}

	txrx_ops.rx.stats_rx = dp_tx_rx_collect_connectivity_stats_info;

	txrx_ops.tx.tx_comp = dp_sta_notify_tx_comp_cb;
	txrx_ops.tx.tx = NULL;
	txrx_ops.get_tsf_time = wlan_dp_get_tsf_time;
	cdp_vdev_register(soc, dp_intf->intf_id, (ol_osif_vdev_handle)dp_intf,
			  &txrx_ops);
	if (!txrx_ops.tx.tx) {
		dp_err("vdev register fail");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf->tx_fn = txrx_ops.tx.tx;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_TDLS
QDF_STATUS ucfg_dp_tdlsta_register_txrx_ops(struct wlan_objmgr_vdev *vdev)
{
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	/* Register the vdev transmit and receive functions */
	qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));
	if (dp_intf->dp_ctx->enable_dp_rx_threads) {
		txrx_ops.rx.rx = dp_rx_pkt_thread_enqueue_cbk;
		txrx_ops.rx.rx_stack = dp_rx_packet_cbk;
		txrx_ops.rx.rx_flush = dp_rx_flush_packet_cbk;
		txrx_ops.rx.rx_gro_flush = dp_rx_thread_gro_flush_ind_cbk;
		dp_intf->rx_stack = dp_rx_packet_cbk;
	} else {
		txrx_ops.rx.rx = dp_rx_packet_cbk;
		txrx_ops.rx.rx_stack = NULL;
		txrx_ops.rx.rx_flush = NULL;
	}
	if (dp_intf->dp_ctx->dp_cfg.fisa_enable &&
	    dp_intf->device_mode != QDF_MONITOR_MODE) {
		dp_debug("FISA feature enabled");
		dp_rx_register_fisa_ops(&txrx_ops);
	}

	txrx_ops.rx.stats_rx = dp_tx_rx_collect_connectivity_stats_info;

	txrx_ops.tx.tx_comp = dp_sta_notify_tx_comp_cb;
	txrx_ops.tx.tx = NULL;

	cdp_vdev_register(soc, dp_intf->intf_id, (ol_osif_vdev_handle)dp_intf,
			  &txrx_ops);

	if (!txrx_ops.tx.tx) {
		dp_err("vdev register fail");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf->tx_fn = txrx_ops.tx.tx;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS ucfg_dp_ocb_register_txrx_ops(struct wlan_objmgr_vdev *vdev)
{
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	/* Register the vdev transmit and receive functions */
	qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));
	txrx_ops.rx.rx = dp_rx_packet_cbk;
	txrx_ops.rx.stats_rx = dp_tx_rx_collect_connectivity_stats_info;

	cdp_vdev_register(soc, dp_intf->intf_id, (ol_osif_vdev_handle)dp_intf,
			  &txrx_ops);
	if (!txrx_ops.tx.tx) {
		dp_err("vdev register fail");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf->tx_fn = txrx_ops.tx.tx;

	qdf_copy_macaddr(&dp_intf->conn_info.peer_macaddr,
			 &dp_intf->mac_addr);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
QDF_STATUS ucfg_dp_mon_register_txrx_ops(struct wlan_objmgr_vdev *vdev)
{
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));
	txrx_ops.rx.rx = dp_mon_rx_packet_cbk;
	dp_monitor_set_rx_monitor_cb(&txrx_ops, dp_rx_monitor_callback);
	cdp_vdev_register(soc, dp_intf->intf_id,
			  (ol_osif_vdev_handle)dp_intf,
			  &txrx_ops);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS ucfg_dp_softap_register_txrx_ops(struct wlan_objmgr_vdev *vdev,
					    struct ol_txrx_ops *txrx_ops)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	/* Register the vdev transmit and receive functions */
	txrx_ops->tx.tx_comp = dp_softap_notify_tx_compl_cbk;

	if (dp_intf->dp_ctx->enable_dp_rx_threads) {
		txrx_ops->rx.rx = dp_rx_pkt_thread_enqueue_cbk;
		txrx_ops->rx.rx_stack = dp_softap_rx_packet_cbk;
		txrx_ops->rx.rx_flush = dp_rx_flush_packet_cbk;
		txrx_ops->rx.rx_gro_flush = dp_rx_thread_gro_flush_ind_cbk;
		dp_intf->rx_stack = dp_softap_rx_packet_cbk;
	} else {
		txrx_ops->rx.rx = dp_softap_rx_packet_cbk;
		txrx_ops->rx.rx_stack = NULL;
		txrx_ops->rx.rx_flush = NULL;
	}

	txrx_ops->get_tsf_time = wlan_dp_get_tsf_time;
	cdp_vdev_register(soc,
			  dp_intf->intf_id,
			  (ol_osif_vdev_handle)dp_intf,
			  txrx_ops);
	if (!txrx_ops->tx.tx) {
		dp_err("vdev register fail");
		return QDF_STATUS_E_FAILURE;
	}

	dp_intf->tx_fn = txrx_ops->tx.tx;
	dp_intf->sap_tx_block_mask &= ~DP_TX_FN_CLR;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_register_pkt_capture_callbacks(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	return wlan_pkt_capture_register_callbacks(vdev,
						   dp_mon_rx_packet_cbk,
						   dp_intf);
}

QDF_STATUS ucfg_dp_init_txrx(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_deinit_txrx(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	dp_intf->tx_fn = NULL;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_softap_init_txrx(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(&dp_intf->stats, sizeof(qdf_net_dev_stats));
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_softap_deinit_txrx(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	dp_intf->tx_fn = NULL;
	dp_intf->sap_tx_block_mask |= DP_TX_FN_CLR;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dp_start_xmit(qdf_nbuf_t nbuf, struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;
	QDF_STATUS status;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	qdf_atomic_inc(&dp_intf->num_active_task);
	status = dp_start_xmit(dp_intf, nbuf);
	qdf_atomic_dec(&dp_intf->num_active_task);

	return status;
}

QDF_STATUS ucfg_dp_rx_packet_cbk(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t nbuf)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	return dp_rx_packet_cbk(dp_intf, nbuf);
}

void ucfg_dp_tx_timeout(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return;
	}

	dp_tx_timeout(dp_intf);
}

QDF_STATUS
ucfg_dp_softap_start_xmit(qdf_nbuf_t nbuf, struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;
	QDF_STATUS status;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return QDF_STATUS_E_INVAL;
	}

	qdf_atomic_inc(&dp_intf->num_active_task);
	status = dp_softap_start_xmit(nbuf, dp_intf);
	qdf_atomic_dec(&dp_intf->num_active_task);

	return status;
}

void ucfg_dp_softap_tx_timeout(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return;
	}

	dp_softap_tx_timeout(dp_intf);
}

qdf_net_dev_stats *ucfg_dp_get_dev_stats(qdf_netdev_t dev)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_get_context();

	dp_intf = dp_get_intf_by_netdev(dp_ctx, dev);
	if (!dp_intf) {
		dp_err("DP interface not found dev: %s",
		       qdf_netdev_get_devname(dev));
		QDF_BUG(0);
		return NULL;
	}

	return &dp_intf->stats;
}

void ucfg_dp_inc_rx_pkt_stats(struct wlan_objmgr_vdev *vdev,
			      uint32_t pkt_len,
			      bool delivered)
{
	struct wlan_dp_intf *dp_intf;
	struct dp_tx_rx_stats *stats;
	unsigned int cpu_index;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return;
	}

	cpu_index = qdf_get_cpu();
	stats = &dp_intf->dp_stats.tx_rx_stats;

	++stats->per_cpu[cpu_index].rx_packets;
	qdf_net_stats_add_rx_pkts(&dp_intf->stats, 1);
	qdf_net_stats_add_rx_bytes(&dp_intf->stats, pkt_len);

	if (delivered)
		++stats->per_cpu[cpu_index].rx_delivered;
	else
		++stats->per_cpu[cpu_index].rx_refused;
}

void ucfg_dp_register_rx_mic_error_ind_handler(void *soc)
{
	cdp_register_rx_mic_error_ind_handler(soc, dp_rx_mic_error_ind);
}

#ifdef WLAN_NUD_TRACKING
bool
ucfg_dp_is_roam_after_nud_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	struct wlan_dp_psoc_cfg *dp_cfg = &dp_ctx->dp_cfg;

	if (dp_cfg->enable_nud_tracking == DP_ROAM_AFTER_NUD_FAIL ||
	    dp_cfg->enable_nud_tracking == DP_DISCONNECT_AFTER_ROAM_FAIL)
		return true;

	return false;
}

bool
ucfg_dp_is_disconect_after_roam_fail(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	struct wlan_dp_psoc_cfg *dp_cfg = &dp_ctx->dp_cfg;

	if (dp_cfg->enable_nud_tracking == DP_DISCONNECT_AFTER_ROAM_FAIL)
		return true;

	return false;
}
#endif

int ucfg_dp_bbm_context_init(struct wlan_objmgr_psoc *psoc)
{
	return dp_bbm_context_init(psoc);
}

void ucfg_dp_bbm_context_deinit(struct wlan_objmgr_psoc *psoc)
{
	dp_bbm_context_deinit(psoc);
}

void ucfg_dp_bbm_apply_independent_policy(struct wlan_objmgr_psoc *psoc,
					  struct bbm_params *params)
{
	dp_bbm_apply_independent_policy(psoc, params);
}

void ucfg_dp_set_rx_mode_rps(bool enable)
{
	dp_set_rx_mode_rps(enable);
}

void ucfg_dp_periodic_sta_stats_start(struct wlan_objmgr_vdev *vdev)
{
	dp_periodic_sta_stats_start(vdev);
}

void ucfg_dp_periodic_sta_stats_stop(struct wlan_objmgr_vdev *vdev)
{
	dp_periodic_sta_stats_stop(vdev);
}

void ucfg_dp_try_send_rps_ind(struct wlan_objmgr_vdev *vdev)
{
	dp_try_send_rps_ind(vdev);
}

void ucfg_dp_reg_ipa_rsp_ind(struct wlan_objmgr_pdev *pdev)
{
	ucfg_ipa_reg_rps_enable_cb(pdev, dp_set_rps);
}

void ucfg_dp_try_set_rps_cpu_mask(struct wlan_objmgr_psoc *psoc)
{
	dp_try_set_rps_cpu_mask(psoc);
}

void ucfg_dp_add_latency_critical_client(struct wlan_objmgr_vdev *vdev,
					 enum qca_wlan_802_11_mode phymode)
{
	dp_add_latency_critical_client(vdev, phymode);
}

void ucfg_dp_del_latency_critical_client(struct wlan_objmgr_vdev *vdev,
					 enum qca_wlan_802_11_mode phymode)
{
	dp_del_latency_critical_client(vdev, phymode);
}

void ucfg_dp_reset_tcp_delack(struct wlan_objmgr_psoc *psoc)
{
	dp_reset_tcp_delack(psoc);
}

void
ucfg_dp_set_current_throughput_level(struct wlan_objmgr_psoc *psoc,
				     enum pld_bus_width_type next_vote_level)
{
	dp_set_current_throughput_level(psoc, next_vote_level);
}

void
ucfg_dp_set_high_bus_bw_request(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				bool high_bus_bw)
{
	dp_set_high_bus_bw_request(psoc, vdev_id, high_bus_bw);
}

void ucfg_wlan_dp_display_tx_rx_histogram(struct wlan_objmgr_psoc *psoc)
{
	wlan_dp_display_tx_rx_histogram(psoc);
}

void ucfg_wlan_dp_clear_tx_rx_histogram(struct wlan_objmgr_psoc *psoc)
{
	wlan_dp_clear_tx_rx_histogram(psoc);
}

void ucfg_dp_bus_bw_compute_timer_start(struct wlan_objmgr_psoc *psoc)
{
	dp_bus_bw_compute_timer_start(psoc);
}

void ucfg_dp_bus_bw_compute_timer_try_start(struct wlan_objmgr_psoc *psoc)
{
	dp_bus_bw_compute_timer_try_start(psoc);
}

void ucfg_dp_bus_bw_compute_timer_stop(struct wlan_objmgr_psoc *psoc)
{
	dp_bus_bw_compute_timer_stop(psoc);
}

void ucfg_dp_bus_bw_compute_timer_try_stop(struct wlan_objmgr_psoc *psoc)
{
	dp_bus_bw_compute_timer_try_stop(psoc);
}

void ucfg_dp_bus_bw_compute_prev_txrx_stats(struct wlan_objmgr_vdev *vdev)
{
	dp_bus_bw_compute_prev_txrx_stats(vdev);
}

void
ucfg_dp_bus_bw_compute_reset_prev_txrx_stats(struct wlan_objmgr_vdev *vdev)
{
	dp_bus_bw_compute_reset_prev_txrx_stats(vdev);
}

void ucfg_dp_nud_set_gateway_addr(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr gw_mac_addr)
{
	dp_nud_set_gateway_addr(vdev, gw_mac_addr);
}

void ucfg_dp_nud_event(struct qdf_mac_addr *netdev_mac_addr,
		       struct qdf_mac_addr *gw_mac_addr,
		       uint8_t nud_state)
{
	dp_nud_netevent_cb(netdev_mac_addr, gw_mac_addr, nud_state);
}

QDF_STATUS ucfg_dp_get_arp_stats_event_handler(struct wlan_objmgr_psoc *psoc,
					       struct dp_rsp_stats *rsp)
{
	return dp_get_arp_stats_event_handler(psoc, rsp);
}

void *ucfg_dp_get_arp_request_ctx(struct wlan_objmgr_psoc *psoc)
{
	return dp_get_arp_request_ctx(psoc);
}

void ucfg_dp_nud_reset_tracking(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("Unable to get DP Interface");
		return;
	}
	dp_nud_reset_tracking(dp_intf);
}

uint8_t ucfg_dp_nud_tracking_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_ctx->dp_cfg.enable_nud_tracking;
}

void ucfg_dp_nud_indicate_roam(struct wlan_objmgr_vdev *vdev)
{
	dp_nud_indicate_roam(vdev);
}

void ucfg_dp_clear_arp_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	qdf_mem_zero(&dp_intf->dp_stats.arp_stats,
		     sizeof(dp_intf->dp_stats.arp_stats));
}

void ucfg_dp_clear_dns_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	qdf_mem_zero(&dp_intf->dp_stats.dns_stats,
		     sizeof(dp_intf->dp_stats.dns_stats));
}

void ucfg_dp_clear_tcp_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	qdf_mem_zero(&dp_intf->dp_stats.tcp_stats,
		     sizeof(dp_intf->dp_stats.tcp_stats));
}

void ucfg_dp_clear_icmpv4_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	qdf_mem_zero(&dp_intf->dp_stats.icmpv4_stats,
		     sizeof(dp_intf->dp_stats.icmpv4_stats));
}

void ucfg_dp_clear_dns_payload_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	qdf_mem_zero(dp_intf->dns_payload, dp_intf->track_dns_domain_len);
}

void ucfg_dp_set_pkt_type_bitmap_value(struct wlan_objmgr_vdev *vdev,
				       uint32_t value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	dp_intf->pkt_type_bitmap = value;
}

uint32_t ucfg_dp_intf_get_pkt_type_bitmap_value(void *intf_ctx)
{
	struct wlan_dp_intf *dp_intf = (struct wlan_dp_intf *)intf_ctx;

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}

	return dp_intf->pkt_type_bitmap;
}

void ucfg_dp_set_track_dest_ipv4_value(struct wlan_objmgr_vdev *vdev,
				       uint32_t value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	dp_intf->track_dest_ipv4 = value;
}

void ucfg_dp_set_track_dest_port_value(struct wlan_objmgr_vdev *vdev,
				       uint32_t value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	dp_intf->track_dest_port = value;
}

void ucfg_dp_set_track_src_port_value(struct wlan_objmgr_vdev *vdev,
				      uint32_t value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	dp_intf->track_src_port = value;
}

void ucfg_dp_set_track_dns_domain_len_value(struct wlan_objmgr_vdev *vdev,
					    uint32_t value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	dp_intf->track_dns_domain_len = value;
}

void ucfg_dp_set_track_arp_ip_value(struct wlan_objmgr_vdev *vdev,
				    uint32_t value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	dp_intf->track_arp_ip = value;
}

uint32_t ucfg_dp_get_pkt_type_bitmap_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->pkt_type_bitmap;
}

void ucfg_dp_get_dns_payload_value(struct wlan_objmgr_vdev *vdev,
				   uint8_t *dns_query)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return;
	}
	qdf_mem_copy(dns_query, dp_intf->dns_payload,
		     dp_intf->track_dns_domain_len);
}

uint32_t ucfg_dp_get_track_dns_domain_len_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->track_dns_domain_len;
}

uint32_t ucfg_dp_get_track_dest_port_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->track_dest_port;
}

uint32_t ucfg_dp_get_track_src_port_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->track_src_port;
}

uint32_t ucfg_dp_get_track_dest_ipv4_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->track_dest_ipv4;
}

bool ucfg_dp_get_dad_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->dad;
}

bool ucfg_dp_get_con_status_value(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->con_status;
}

uint8_t ucfg_dp_get_intf_id(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return 0;
	}
	return dp_intf->intf_id;
}

struct dp_arp_stats *ucfg_dp_get_arp_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return NULL;
	}
	return &dp_intf->dp_stats.arp_stats;
}

struct dp_icmpv4_stats *ucfg_dp_get_icmpv4_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return NULL;
	}
	return &dp_intf->dp_stats.icmpv4_stats;
}

struct dp_tcp_stats *ucfg_dp_get_tcp_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return NULL;
	}
	return &dp_intf->dp_stats.tcp_stats;
}

struct dp_dns_stats *ucfg_dp_get_dns_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("DP Context is NULL");
		return NULL;
	}
	return &dp_intf->dp_stats.dns_stats;
}

void ucfg_dp_set_nud_stats_cb(struct wlan_objmgr_psoc *psoc, void *cookie)
{
	struct wlan_dp_psoc_sb_ops *sb_ops = dp_intf_get_tx_ops(psoc);

	if (!sb_ops) {
		dp_err("Unable to get ops");
		return;
	}

	sb_ops->dp_arp_stats_register_event_handler(psoc);
	sb_ops->arp_request_ctx = cookie;
}

void ucfg_dp_clear_nud_stats_cb(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_sb_ops *sb_ops = dp_intf_get_tx_ops(psoc);

	if (!sb_ops) {
		dp_err("Unable to get ops");
		return;
	}

	sb_ops->dp_arp_stats_unregister_event_handler(psoc);
}

void ucfg_dp_set_dump_dp_trace(uint16_t cmd_type, uint16_t count)
{
	dp_set_dump_dp_trace(cmd_type, count);
}

int ucfg_dp_get_current_throughput_level(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx)
		return 0;

	return dp_get_current_throughput_level(dp_ctx);
}

uint32_t ucfg_dp_get_bus_bw_high_threshold(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx)
		return 0;

	return dp_get_bus_bw_high_threshold(dp_ctx);
}

QDF_STATUS
ucfg_dp_req_get_arp_stats(struct wlan_objmgr_psoc *psoc,
			  struct dp_get_arp_stats_params *params)
{
	struct wlan_dp_psoc_sb_ops *sb_ops = dp_intf_get_tx_ops(psoc);

	if (!sb_ops) {
		dp_err("Unable to get ops");
		return QDF_STATUS_E_INVAL;
	}

	return sb_ops->dp_get_arp_req_stats(psoc, params);
}

QDF_STATUS
ucfg_dp_req_set_arp_stats(struct wlan_objmgr_psoc *psoc,
			  struct dp_set_arp_stats_params *params)
{
	struct wlan_dp_psoc_sb_ops *sb_ops = dp_intf_get_tx_ops(psoc);

	if (!sb_ops) {
		dp_err("Unable to get ops");
		return QDF_STATUS_E_INVAL;
	}

	return sb_ops->dp_set_arp_req_stats(psoc, params);
}

void ucfg_dp_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				    struct wlan_dp_psoc_callbacks *cb_obj)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP ctx is NULL");
		return;
	}
	dp_ctx->dp_ops.callback_ctx = cb_obj->callback_ctx;
	dp_ctx->dp_ops.wlan_dp_sta_get_dot11mode =
		cb_obj->wlan_dp_sta_get_dot11mode;
	dp_ctx->dp_ops.wlan_dp_get_ap_client_count =
		cb_obj->wlan_dp_get_ap_client_count;
	dp_ctx->dp_ops.wlan_dp_sta_ndi_connected =
		cb_obj->wlan_dp_sta_ndi_connected;
	dp_ctx->dp_ops.dp_any_adapter_connected =
		cb_obj->dp_any_adapter_connected;
	dp_ctx->dp_ops.dp_send_svc_nlink_msg = cb_obj->dp_send_svc_nlink_msg;
	dp_ctx->dp_ops.dp_pm_qos_update_request =
		cb_obj->dp_pm_qos_update_request;
	dp_ctx->dp_ops.dp_pld_remove_pm_qos = cb_obj->dp_pld_remove_pm_qos;
	dp_ctx->dp_ops.dp_pld_request_pm_qos = cb_obj->dp_pld_request_pm_qos;
	dp_ctx->dp_ops.dp_pm_qos_add_request = cb_obj->dp_pm_qos_add_request;
	dp_ctx->dp_ops.dp_pm_qos_remove_request =
		cb_obj->dp_pm_qos_remove_request;
	dp_ctx->dp_ops.wlan_dp_display_tx_multiq_stats =
		cb_obj->wlan_dp_display_tx_multiq_stats;
	dp_ctx->dp_ops.wlan_dp_display_netif_queue_history =
		cb_obj->wlan_dp_display_netif_queue_history;
	dp_ctx->dp_ops.dp_send_mscs_action_frame =
		cb_obj->dp_send_mscs_action_frame;
	dp_ctx->dp_ops.dp_pktlog_enable_disable =
		cb_obj->dp_pktlog_enable_disable;
	dp_ctx->dp_ops.dp_is_roaming_in_progress =
		cb_obj->dp_is_roaming_in_progress;
	dp_ctx->dp_ops.dp_is_ap_active = cb_obj->dp_is_ap_active;
	dp_ctx->dp_ops.dp_disable_rx_ol_for_low_tput =
		cb_obj->dp_disable_rx_ol_for_low_tput;
	dp_ctx->dp_ops.dp_napi_apply_throughput_policy =
		cb_obj->dp_napi_apply_throughput_policy;
	dp_ctx->dp_ops.dp_is_link_adapter = cb_obj->dp_is_link_adapter;
	dp_ctx->dp_ops.dp_get_pause_map = cb_obj->dp_get_pause_map;
	dp_ctx->dp_ops.dp_nud_failure_work = cb_obj->dp_nud_failure_work;

	dp_ctx->dp_ops.dp_get_tx_resource = cb_obj->dp_get_tx_resource;
	dp_ctx->dp_ops.dp_get_tx_flow_low_watermark =
		cb_obj->dp_get_tx_flow_low_watermark;
	dp_ctx->dp_ops.dp_get_tsf_time = cb_obj->dp_get_tsf_time;
	dp_ctx->dp_ops.dp_tsf_timestamp_rx = cb_obj->dp_tsf_timestamp_rx;
	dp_ctx->dp_ops.dp_gro_rx_legacy_get_napi =
		cb_obj->dp_gro_rx_legacy_get_napi;
	dp_ctx->dp_ops.dp_get_netdev_by_vdev_mac =
		cb_obj->dp_get_netdev_by_vdev_mac;

	dp_ctx->dp_ops.dp_nbuf_push_pkt = cb_obj->dp_nbuf_push_pkt;
	dp_ctx->dp_ops.dp_rx_napi_gro_flush = cb_obj->dp_rx_napi_gro_flush;
	dp_ctx->dp_ops.dp_rx_thread_napi_gro_flush =
	    cb_obj->dp_rx_thread_napi_gro_flush;
	dp_ctx->dp_ops.dp_rx_napi_gro_receive = cb_obj->dp_rx_napi_gro_receive;
	dp_ctx->dp_ops.dp_lro_rx_cb = cb_obj->dp_lro_rx_cb;
	dp_ctx->dp_ops.dp_register_rx_offld_flush_cb =
		cb_obj->dp_register_rx_offld_flush_cb;
	dp_ctx->dp_ops.dp_rx_check_qdisc_configured =
		cb_obj->dp_rx_check_qdisc_configured;
	dp_ctx->dp_ops.dp_is_gratuitous_arp_unsolicited_na =
		cb_obj->dp_is_gratuitous_arp_unsolicited_na;
	dp_ctx->dp_ops.dp_send_rx_pkt_over_nl = cb_obj->dp_send_rx_pkt_over_nl;
	dp_ctx->dp_ops.osif_dp_send_tcp_param_update_event =
		cb_obj->osif_dp_send_tcp_param_update_event;
	dp_ctx->dp_ops.os_if_dp_nud_stats_info =
		cb_obj->os_if_dp_nud_stats_info;
	dp_ctx->dp_ops.osif_dp_process_mic_error =
		cb_obj->osif_dp_process_mic_error;
	dp_ctx->dp_ops.link_monitoring_cb = cb_obj->link_monitoring_cb;
	}

void ucfg_dp_register_event_handler(struct wlan_objmgr_psoc *psoc,
				    struct wlan_dp_psoc_nb_ops *cb_obj)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP ctx is NULL");
		return;
	}

	dp_ctx->nb_ops.osif_dp_get_arp_stats_evt =
		cb_obj->osif_dp_get_arp_stats_evt;
}

uint32_t ucfg_dp_get_bus_bw_compute_interval(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP ctx is NULL");
		return 0;
	}
	return DP_BUS_BW_CFG(dp_ctx->dp_cfg.bus_bw_compute_interval);
}

QDF_STATUS ucfg_dp_get_txrx_stats(struct wlan_objmgr_vdev *vdev,
				  struct dp_tx_rx_stats *dp_stats)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);
	struct dp_tx_rx_stats *txrx_stats;
	int i = 0, rx_mcast_drp = 0;

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return QDF_STATUS_E_INVAL;
	}

	txrx_stats = &dp_intf->dp_stats.tx_rx_stats;
	for (i = 0; i < NUM_CPUS; i++) {
		dp_stats->per_cpu[i].rx_packets = txrx_stats->per_cpu[i].rx_packets;
		dp_stats->per_cpu[i].rx_dropped = txrx_stats->per_cpu[i].rx_dropped;
		dp_stats->per_cpu[i].rx_delivered = txrx_stats->per_cpu[i].rx_delivered;
		dp_stats->per_cpu[i].rx_refused = txrx_stats->per_cpu[i].rx_refused;
		dp_stats->per_cpu[i].tx_called = txrx_stats->per_cpu[i].tx_called;
		dp_stats->per_cpu[i].tx_dropped = txrx_stats->per_cpu[i].tx_dropped;
		dp_stats->per_cpu[i].tx_orphaned = txrx_stats->per_cpu[i].tx_orphaned;
	}
	rx_mcast_drp = qdf_atomic_read(&txrx_stats->rx_usolict_arp_n_mcast_drp);
	qdf_atomic_set(&dp_stats->rx_usolict_arp_n_mcast_drp, rx_mcast_drp);

	dp_stats->rx_aggregated = txrx_stats->rx_aggregated;
	dp_stats->rx_gro_dropped = txrx_stats->rx_gro_dropped;
	dp_stats->rx_non_aggregated = txrx_stats->rx_non_aggregated;
	dp_stats->rx_gro_flush_skip = txrx_stats->rx_gro_flush_skip;
	dp_stats->rx_gro_low_tput_flush = txrx_stats->rx_gro_low_tput_flush;
	dp_stats->tx_timeout_cnt = txrx_stats->tx_timeout_cnt;
	dp_stats->cont_txtimeout_cnt = txrx_stats->cont_txtimeout_cnt;
	dp_stats->last_txtimeout = txrx_stats->last_txtimeout;

	return QDF_STATUS_SUCCESS;
}

void ucfg_dp_get_net_dev_stats(struct wlan_objmgr_vdev *vdev,
			       qdf_net_dev_stats *stats)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return;
	}

	dp_get_net_dev_stats(dp_intf, stats);
}

void ucfg_dp_clear_net_dev_stats(qdf_netdev_t dev)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_get_context();
	if (qdf_unlikely(!dp_ctx)) {
		dp_err_rl("DP context not found");
		return;
	}

	dp_intf = dp_get_intf_by_netdev(dp_ctx, dev);
	if (qdf_unlikely(!dp_intf)) {
		dp_err_rl("DP interface not found");
		return;
	}

	dp_clear_net_dev_stats(dp_intf);
}

void ucfg_dp_reset_cont_txtimeout_cnt(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return;
	}
	dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt = 0;
}

void ucfg_dp_set_rx_thread_affinity(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	struct wlan_dp_psoc_cfg *cfg;

	if (!dp_ctx) {
		dp_err("DP ctx is NULL");
		return;
	}
	cfg = &dp_ctx->dp_cfg;

	if (cfg->rx_thread_affinity_mask)
		cds_set_rx_thread_cpu_mask(cfg->rx_thread_affinity_mask);

	if (cfg->rx_thread_ul_affinity_mask)
		cds_set_rx_thread_ul_cpu_mask(cfg->rx_thread_ul_affinity_mask);
}

void ucfg_dp_get_disable_rx_ol_val(struct wlan_objmgr_psoc *psoc,
				   uint8_t *disable_conc,
				   uint8_t *disable_low_tput)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}
	*disable_conc = qdf_atomic_read(&dp_ctx->disable_rx_ol_in_concurrency);
	*disable_low_tput = qdf_atomic_read(&dp_ctx->disable_rx_ol_in_low_tput);
}

uint32_t ucfg_dp_get_rx_aggregation_val(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return 0;
	}
	return qdf_atomic_read(&dp_ctx->dp_agg_param.rx_aggregation);
}

void ucfg_dp_set_rx_aggregation_val(struct wlan_objmgr_psoc *psoc,
				    uint32_t value)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}
	qdf_atomic_set(&dp_ctx->dp_agg_param.rx_aggregation, !!value);
}

void ucfg_dp_set_tc_based_dyn_gro(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP ctx is NULL");
		return;
	}
	dp_ctx->dp_agg_param.tc_based_dyn_gro = value;
}

void ucfg_dp_runtime_disable_rx_thread(struct wlan_objmgr_vdev *vdev,
				       bool value)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return;
	}
	dp_intf->runtime_disable_rx_thread = value;
}

bool ucfg_dp_get_napi_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return 0;
	}
	return dp_ctx->napi_enable;
}

void ucfg_dp_set_tc_ingress_prio(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP ctx is NULL");
		return;
	}
	dp_ctx->dp_agg_param.tc_ingress_prio = value;
}

bool ucfg_dp_nud_fail_data_stall_evt_enabled(void)
{
	return dp_is_data_stall_event_enabled(DP_HOST_NUD_FAILURE);
}

uint32_t ucfg_dp_fw_data_stall_evt_enabled(void)
{
	return cdp_cfg_get(cds_get_context(QDF_MODULE_ID_SOC),
			   cfg_dp_enable_data_stall) & FW_DATA_STALL_EVT_MASK;
}

void ucfg_dp_event_eapol_log(qdf_nbuf_t nbuf, enum qdf_proto_dir dir)
{
	dp_event_eapol_log(nbuf, dir);
}

QDF_STATUS
ucfg_dp_softap_inspect_dhcp_packet(struct wlan_objmgr_vdev *vdev,
				   qdf_nbuf_t nbuf, enum qdf_proto_dir dir)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return QDF_STATUS_E_INVAL;
	}

	return dp_softap_inspect_dhcp_packet(dp_intf, nbuf, dir);
}

void
dp_ucfg_enable_link_monitoring(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_vdev *vdev,
			       uint32_t threshold)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}
	dp_intf->link_monitoring.rx_linkspeed_threshold = threshold;
	dp_intf->link_monitoring.enabled = true;
}

void
dp_ucfg_disable_link_monitoring(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (unlikely(!dp_intf)) {
		dp_err("DP interface not found");
		return;
	}
	dp_intf->link_monitoring.enabled = false;
	dp_intf->link_monitoring.rx_linkspeed_threshold = 0;
}

#ifdef DP_TRAFFIC_END_INDICATION
QDF_STATUS
ucfg_dp_traffic_end_indication_get(struct wlan_objmgr_vdev *vdev,
				   struct dp_traffic_end_indication *info)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return QDF_STATUS_E_INVAL;
	}

	info->enabled = dp_intf->traffic_end_ind.enabled;
	info->def_dscp = dp_intf->traffic_end_ind.def_dscp;
	info->spl_dscp = dp_intf->traffic_end_ind.spl_dscp;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_dp_traffic_end_indication_set(struct wlan_objmgr_vdev *vdev,
				   struct dp_traffic_end_indication info)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);
	cdp_config_param_type vdev_param;

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return QDF_STATUS_E_INVAL;
	}

	dp_intf->traffic_end_ind = info;

	dp_debug("enabled:%u default dscp:%u special dscp:%u",
		 dp_intf->traffic_end_ind.enabled,
		 dp_intf->traffic_end_ind.def_dscp,
		 dp_intf->traffic_end_ind.spl_dscp);

	vdev_param.cdp_vdev_param_traffic_end_ind = info.enabled;
	if (cdp_txrx_set_vdev_param(cds_get_context(QDF_MODULE_ID_SOC),
				    dp_intf->intf_id,
				    CDP_ENABLE_TRAFFIC_END_INDICATION,
				    vdev_param))
		dp_err("Failed to set traffic end indication param on DP vdev");

	return QDF_STATUS_SUCCESS;
}

void ucfg_dp_traffic_end_indication_update_dscp(struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						unsigned char *dscp)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_dp_intf *dp_intf;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_DP_ID);
	if (vdev) {
		dp_intf = dp_get_vdev_priv_obj(vdev);

		if (!dp_intf) {
			dp_err("Unable to get DP interface");
			goto end;
		}

		if (!dp_intf->traffic_end_ind.enabled)
			goto end;

		if (*dscp == dp_intf->traffic_end_ind.spl_dscp)
			*dscp = dp_intf->traffic_end_ind.def_dscp;
end:
		wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID);
	}
}
#endif

QDF_STATUS ucfg_dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc)
{
	return dp_prealloc_init(ctrl_psoc);
}

void ucfg_dp_prealloc_deinit(void)
{
	dp_prealloc_deinit();
}

#ifdef DP_MEM_PRE_ALLOC
void *ucfg_dp_prealloc_get_consistent_mem_unaligned(qdf_size_t size,
						    qdf_dma_addr_t *base_addr,
						    uint32_t ring_type)
{
	return dp_prealloc_get_consistent_mem_unaligned(size, base_addr,
							ring_type);
}

void ucfg_dp_prealloc_put_consistent_mem_unaligned(void *va_unaligned)
{
	dp_prealloc_put_consistent_mem_unaligned(va_unaligned);
}
#endif

#if defined(WLAN_SUPPORT_RX_FISA)
void ucfg_dp_rx_skip_fisa(uint32_t value)
{
	void *dp_soc;

	dp_soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (dp_soc)
		dp_rx_skip_fisa(dp_soc, value);
}
#endif

#ifdef FEATURE_DIRECT_LINK
QDF_STATUS ucfg_dp_direct_link_init(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}

	return dp_direct_link_init(dp_ctx);
}

void ucfg_dp_direct_link_deinit(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("DP context not found");
		return;
	}

	dp_direct_link_deinit(dp_ctx);
}

void
ucfg_dp_wfds_handle_request_mem_ind(struct wlan_qmi_wfds_mem_ind_msg *mem_msg)
{
	dp_wfds_handle_request_mem_ind(mem_msg);
}

void
ucfg_dp_wfds_handle_ipcc_map_n_cfg_ind(struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg *ipcc_msg)
{
	dp_wfds_handle_ipcc_map_n_cfg_ind(ipcc_msg);
}

QDF_STATUS ucfg_dp_wfds_new_server(void)
{
	return dp_wfds_new_server();
}

void ucfg_dp_wfds_del_server(void)
{
	dp_wfds_del_server();
}

QDF_STATUS ucfg_dp_config_direct_link(struct wlan_objmgr_vdev *vdev,
				      bool config_direct_link,
				      bool enable_low_latency)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		return QDF_STATUS_E_INVAL;
	}

	return dp_config_direct_link(dp_intf, config_direct_link,
				     enable_low_latency);
}
#endif

QDF_STATUS ucfg_dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			     struct dp_txrx_config *config)
{
	return dp_txrx_init(soc, pdev_id, config);
}

QDF_STATUS ucfg_dp_txrx_deinit(ol_txrx_soc_handle soc)
{
	return dp_txrx_deinit(soc);
}

QDF_STATUS ucfg_dp_txrx_ext_dump_stats(ol_txrx_soc_handle soc,
				       uint8_t stats_id)
{
	return dp_txrx_ext_dump_stats(soc, stats_id);
}

QDF_STATUS ucfg_dp_txrx_set_cpu_mask(ol_txrx_soc_handle soc,
				     qdf_cpu_mask *new_mask)
{
	return dp_txrx_set_cpu_mask(soc, new_mask);
}

/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: cdp_txrx_ipa.h
 *      Define the host data path IP Acceleraor API functions
 */
#ifndef _CDP_TXRX_IPA_H_
#define _CDP_TXRX_IPA_H_

#ifdef IPA_OFFLOAD
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)
#include <qdf_ipa_wdi3.h>
#else
#include <qdf_ipa.h>
#endif
#include <cdp_txrx_cmn.h>
#include "cdp_txrx_handle.h"
#ifdef IPA_OPT_WIFI_DP
#include <target_if.h>
#endif

/**
 * cdp_ipa_get_resource() - Get allocated WLAN resources for IPA data path
 * @soc: data path soc handle
 * @pdev_id: device instance id
 *
 * Get allocated WLAN resources for IPA data path
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_get_resource(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_get_resource)
		return soc->ops->ipa_ops->ipa_get_resource(soc, pdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_set_doorbell_paddr() - give IPA db paddr to FW
 * @soc: data path soc handle
 * @pdev_id: device instance id
 *
 * give IPA db paddr to FW
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_set_doorbell_paddr(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_set_doorbell_paddr)
		return soc->ops->ipa_ops->ipa_set_doorbell_paddr(soc, pdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_iounmap_doorbell_vaddr() - unmap IPA RX db vaddr
 * @soc: data path soc handle
 * @pdev_id: device instance id
 *
 * Unmap IPA RX db vaddr
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_iounmap_doorbell_vaddr(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			 "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_iounmap_doorbell_vaddr)
		return soc->ops->ipa_ops->ipa_iounmap_doorbell_vaddr(
					soc, pdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_set_active() - activate/de-ctivate IPA offload path
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @uc_active: activate or de-activate
 * @is_tx: toggle tx or rx data path
 *
 * activate/de-ctivate IPA offload path
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_set_active(ol_txrx_soc_handle soc, uint8_t pdev_id, bool uc_active,
		   bool is_tx)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_set_active)
		return soc->ops->ipa_ops->ipa_set_active(soc, pdev_id,
				uc_active, is_tx);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_op_response() - event handler from FW
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @op_msg: event contents from firmware
 *
 * event handler from FW
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_op_response(ol_txrx_soc_handle soc, uint8_t pdev_id, uint8_t *op_msg)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_op_response)
		return soc->ops->ipa_ops->ipa_op_response(soc, pdev_id, op_msg);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_register_op_cb() - register event handler function pointer
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @op_cb: event handler callback function pointer
 * @usr_ctxt: user context to registered
 *
 * register event handler function pointer
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_register_op_cb(ol_txrx_soc_handle soc, uint8_t pdev_id,
		       ipa_uc_op_cb_type op_cb, void *usr_ctxt)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_register_op_cb)
		return soc->ops->ipa_ops->ipa_register_op_cb(soc, pdev_id,
							     op_cb, usr_ctxt);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_deregister_op_cb() - deregister event handler function pointer
 * @soc: data path soc handle
 * @pdev_id: device instance id
 *
 * Deregister event handler function pointer from pdev
 *
 * return QDF_STATUS_SUCCESS
 */
static inline
void cdp_ipa_deregister_op_cb(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->ipa_ops->ipa_deregister_op_cb)
		soc->ops->ipa_ops->ipa_deregister_op_cb(soc, pdev_id);
}

/**
 * cdp_ipa_get_stat() - get IPA data path stats from FW
 * @soc: data path soc handle
 * @pdev_id: device instance id
 *
 * get IPA data path stats from FW async
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_get_stat(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_get_stat)
		return soc->ops->ipa_ops->ipa_get_stat(soc, pdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_tx_send_data_frame() - send IPA data frame
 * @soc: data path soc handle
 * @vdev_id: vdev id
 * @skb: skb
 *
 * Return: skb/ NULL is for success
 */
static inline qdf_nbuf_t cdp_ipa_tx_send_data_frame(ol_txrx_soc_handle soc,
						    uint8_t vdev_id,
						    qdf_nbuf_t skb)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return skb;
	}

	if (soc->ops->ipa_ops->ipa_tx_data_frame)
		return soc->ops->ipa_ops->ipa_tx_data_frame(soc, vdev_id, skb);

	return skb;
}

/**
 * cdp_ipa_set_uc_tx_partition_base() - set tx packet partition base
 * @soc: data path soc handle
 * @cfg_pdev: physical device instance config
 * @value: partition base value
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_set_uc_tx_partition_base(ol_txrx_soc_handle soc,
				struct cdp_cfg *cfg_pdev, uint32_t value)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops || !cfg_pdev) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_set_uc_tx_partition_base)
		soc->ops->ipa_ops->ipa_set_uc_tx_partition_base(cfg_pdev,
								value);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_METERING
/**
 * cdp_ipa_uc_get_share_stats() - get Tx/Rx byte stats from FW
 * @soc: data path soc handle
 * @pdev_id: physical device instance number
 * @value: reset stats
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_uc_get_share_stats(ol_txrx_soc_handle soc, uint8_t pdev_id,
			   uint8_t value)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_uc_get_share_stats)
		return soc->ops->ipa_ops->ipa_uc_get_share_stats(soc, pdev_id,
								 value);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_uc_set_quota() - set quota limit to FW
 * @soc: data path soc handle
 * @pdev_id: physical device instance number
 * @value: quota limit bytes
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_uc_set_quota(ol_txrx_soc_handle soc, uint8_t pdev_id, uint64_t value)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_uc_set_quota)
		return soc->ops->ipa_ops->ipa_uc_set_quota(soc, pdev_id, value);

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * cdp_ipa_enable_autonomy() - Enable autonomy RX data path
 * @soc: data path soc handle
 * @pdev_id: physical device instance number
 *
 * IPA Data path is enabled and resumed.
 * All autonomy data path elements are ready to deliver packet
 * All RX packet should routed to IPA_REO ring, then IPA can receive packet
 * from WLAN
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_enable_autonomy(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_enable_autonomy)
		return soc->ops->ipa_ops->ipa_enable_autonomy(soc, pdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_disable_autonomy() - Disable autonomy RX data path
 * @soc: data path soc handle
 * @pdev_id: physical device instance number
 *
 * IPA Data path is enabled and resumed.
 * All autonomy datapath elements are ready to deliver packet
 * All RX packet should routed to IPA_REO ring, then IPA can receive packet
 * from WLAN
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_disable_autonomy(ol_txrx_soc_handle soc, uint8_t pdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	if (soc->ops->ipa_ops->ipa_disable_autonomy)
		return soc->ops->ipa_ops->ipa_disable_autonomy(soc, pdev_id);

	return QDF_STATUS_SUCCESS;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)

/**
 * cdp_ipa_setup() - Setup and connect IPA pipes
 * @soc: data path soc handle
 * @pdev_id: handle to the device instance number
 * @ipa_i2w_cb: IPA to WLAN callback
 * @ipa_w2i_cb: WLAN to IPA callback
 * @ipa_wdi_meter_notifier_cb: IPA WDI metering callback
 * @ipa_desc_size: IPA descriptor size
 * @ipa_priv: handle to the HTT instance
 * @is_rm_enabled: Is IPA RM enabled or not
 * @tx_pipe_handle: pointer to Tx pipe handle
 * @rx_pipe_handle: pointer to Rx pipe handle
 * @is_smmu_enabled: Is SMMU enabled or not
 * @sys_in: parameters to setup sys pipe in mcc mode
 * @over_gsi: Is IPA using GSI
 * @hdl: IPA handle
 * @id: IPA instance id
 * @ipa_ast_notify_cb: IPA to WLAN callback for ast create
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_setup(ol_txrx_soc_handle soc, uint8_t pdev_id, void *ipa_i2w_cb,
	      void *ipa_w2i_cb, void *ipa_wdi_meter_notifier_cb,
	      uint32_t ipa_desc_size, void *ipa_priv, bool is_rm_enabled,
	      uint32_t *tx_pipe_handle, uint32_t *rx_pipe_handle,
	      bool is_smmu_enabled, qdf_ipa_sys_connect_params_t *sys_in,
	      bool over_gsi, qdf_ipa_wdi_hdl_t hdl, qdf_ipa_wdi_hdl_t id,
	      void *ipa_ast_notify_cb)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_setup)
		return soc->ops->ipa_ops->ipa_setup(soc, pdev_id, ipa_i2w_cb,
						    ipa_w2i_cb,
						    ipa_wdi_meter_notifier_cb,
						    ipa_desc_size, ipa_priv,
						    is_rm_enabled,
						    tx_pipe_handle,
						    rx_pipe_handle,
						    is_smmu_enabled,
						    sys_in, over_gsi, hdl, id,
						    ipa_ast_notify_cb);

	return QDF_STATUS_SUCCESS;
}
#else /* CONFIG_IPA_WDI_UNIFIED_API */
/**
 * cdp_ipa_setup() - Setup and connect IPA pipes
 * @soc: data path soc handle
 * @pdev_id: handle to the device instance number
 * @ipa_i2w_cb: IPA to WLAN callback
 * @ipa_w2i_cb: WLAN to IPA callback
 * @ipa_wdi_meter_notifier_cb: IPA WDI metering callback
 * @ipa_desc_size: IPA descriptor size
 * @ipa_priv: handle to the HTT instance
 * @is_rm_enabled: Is IPA RM enabled or not
 * @tx_pipe_handle: pointer to Tx pipe handle
 * @rx_pipe_handle: pointer to Rx pipe handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_setup(ol_txrx_soc_handle soc, uint8_t pdev_id, void *ipa_i2w_cb,
	      void *ipa_w2i_cb, void *ipa_wdi_meter_notifier_cb,
	      uint32_t ipa_desc_size, void *ipa_priv, bool is_rm_enabled,
	      uint32_t *tx_pipe_handle, uint32_t *rx_pipe_handle)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_setup)
		return soc->ops->ipa_ops->ipa_setup(soc, pdev_id, ipa_i2w_cb,
						    ipa_w2i_cb,
						    ipa_wdi_meter_notifier_cb,
						    ipa_desc_size, ipa_priv,
						    is_rm_enabled,
						    tx_pipe_handle,
						    rx_pipe_handle);

	return QDF_STATUS_SUCCESS;
}
#endif /* CONFIG_IPA_WDI_UNIFIED_API */

/**
 * cdp_ipa_cleanup() - Disconnect IPA pipes
 * @soc: data path soc handle
 * @pdev_id: handle to the device instance number
 * @tx_pipe_handle: Tx pipe handle
 * @rx_pipe_handle: Rx pipe handle
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_cleanup(ol_txrx_soc_handle soc, uint8_t pdev_id,
		uint32_t tx_pipe_handle, uint32_t rx_pipe_handle,
		qdf_ipa_wdi_hdl_t hdl)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_cleanup)
		return soc->ops->ipa_ops->ipa_cleanup(soc, pdev_id,
						      tx_pipe_handle,
						      rx_pipe_handle, hdl);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_setup_iface() - Setup IPA header and register interface
 * @soc: data path soc handle
 * @ifname: Interface name
 * @mac_addr: Interface MAC address
 * @prod_client: IPA prod client type
 * @cons_client: IPA cons client type
 * @session_id: Session ID
 * @is_ipv6_enabled: Is IPV6 enabled or not
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_setup_iface(ol_txrx_soc_handle soc, char *ifname, uint8_t *mac_addr,
		    qdf_ipa_client_type_t prod_client,
		    qdf_ipa_client_type_t cons_client,
		    uint8_t session_id, bool is_ipv6_enabled,
		    qdf_ipa_wdi_hdl_t hdl)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_setup_iface)
		return soc->ops->ipa_ops->ipa_setup_iface(ifname, mac_addr,
							  prod_client,
							  cons_client,
							  session_id,
							  is_ipv6_enabled,
							  hdl);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_cleanup_iface() - Cleanup IPA header and deregister interface
 * @soc: data path soc handle
 * @ifname: Interface name
 * @is_ipv6_enabled: Is IPV6 enabled or not
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_cleanup_iface(ol_txrx_soc_handle soc, char *ifname,
		      bool is_ipv6_enabled, qdf_ipa_wdi_hdl_t hdl)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_cleanup_iface)
		return soc->ops->ipa_ops->ipa_cleanup_iface(ifname,
							    is_ipv6_enabled,
							    hdl);

	return QDF_STATUS_SUCCESS;
}

 /**
 * cdp_ipa_uc_enable_pipes() - Enable and resume traffic on Tx/Rx pipes
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_enable_pipes(ol_txrx_soc_handle soc, uint8_t pdev_id,
		     qdf_ipa_wdi_hdl_t hdl)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_enable_pipes)
		return soc->ops->ipa_ops->ipa_enable_pipes(soc, pdev_id, hdl);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_disable_pipes() - Suspend traffic and disable Tx/Rx pipes
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_disable_pipes(ol_txrx_soc_handle soc, uint8_t pdev_id,
		      qdf_ipa_wdi_hdl_t hdl)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_disable_pipes)
		return soc->ops->ipa_ops->ipa_disable_pipes(soc, pdev_id, hdl);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_set_perf_level() - Set IPA clock bandwidth based on data rates
 * @soc: data path soc handle
 * @client: WLAN Client ID
 * @max_supported_bw_mbps: Maximum bandwidth needed (in Mbps)
 * @hdl: IPA handle
 *
 * Return: 0 on success, negative errno on error
 */
static inline QDF_STATUS
cdp_ipa_set_perf_level(ol_txrx_soc_handle soc, int client,
		       uint32_t max_supported_bw_mbps, qdf_ipa_wdi_hdl_t hdl)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_set_perf_level)
		return soc->ops->ipa_ops->ipa_set_perf_level(client,
				max_supported_bw_mbps, hdl);

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * cdp_ipa_rx_wdsext_iface() - Forward RX exception packets to wdsext interface
 * @soc: data path soc handle
 * @peer_id: Peer id to get respective peer
 * @skb: socket buffer
 *
 * Return: true if packets sent to wds ext interface, else false.
 */
static inline bool
cdp_ipa_rx_wdsext_iface(ol_txrx_soc_handle soc, uint8_t peer_id,
			qdf_nbuf_t skb)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return false;
	}

	if (soc->ops->ipa_ops->ipa_rx_wdsext_iface)
		return soc->ops->ipa_ops->ipa_rx_wdsext_iface(soc, peer_id,
							      skb);

	return false;
}
#endif

/**
 * cdp_ipa_rx_intrabss_fwd() - Perform intra-bss fwd for IPA RX path
 *
 * @soc: data path soc handle
 * @vdev_id: vdev id
 * @nbuf: pointer to skb of ethernet packet received from IPA RX path
 * @fwd_success: pointer to indicate if skb succeeded in intra-bss TX
 *
 * This function performs intra-bss forwarding for WDI 3.0 IPA RX path.
 *
 * Return: true if packet is intra-bss fwd-ed and no need to pass to
 *	   network stack. false if packet needs to be passed to network stack.
 */
static inline bool
cdp_ipa_rx_intrabss_fwd(ol_txrx_soc_handle soc, uint8_t vdev_id,
			qdf_nbuf_t nbuf, bool *fwd_success)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops || !fwd_success) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_rx_intrabss_fwd)
		return soc->ops->ipa_ops->ipa_rx_intrabss_fwd(soc, vdev_id,
							      nbuf,
							      fwd_success);

	/* Fall back to pass up to stack */
	return false;
}

/**
 * cdp_ipa_tx_buf_smmu_mapping() - Create SMMU mappings for Tx
 *				   buffers allocated to IPA
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @line: line number
 * @func: function name
 *
 * Create SMMU mappings for Tx buffers allocated to IPA
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_tx_buf_smmu_mapping(ol_txrx_soc_handle soc, uint8_t pdev_id,
			    const char *func, uint32_t line)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_tx_buf_smmu_mapping)
		return soc->ops->ipa_ops->ipa_tx_buf_smmu_mapping(soc, pdev_id,
								  func,
								  line);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_tx_buf_smmu_unmapping() - Release SMMU mappings for Tx
 *				     buffers allocated to IPA
 * @soc: data path soc handle
 * @pdev_id: device instance id
 * @line: line number
 * @func: function name
 *
 * Release SMMU mappings for Tx buffers allocated to IPA
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_tx_buf_smmu_unmapping(ol_txrx_soc_handle soc, uint8_t pdev_id,
			      const char *func, uint32_t line)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_tx_buf_smmu_unmapping)
		return soc->ops->ipa_ops->ipa_tx_buf_smmu_unmapping(soc,
								    pdev_id,
								    func,
								    line);

	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_ipa_rx_buf_smmu_pool_mapping() - Create SMMU mappings for Rx pool
 * @soc: data path soc handle
 * @pdev_id: pdev id
 * @create: Map/unmap
 * @line: line number
 * @func: function name
 *
 * Create SMMU map/unmap for Rx buffers allocated to IPA
 *
 * return QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
cdp_ipa_rx_buf_smmu_pool_mapping(ol_txrx_soc_handle soc, uint8_t pdev_id,
				 bool create, const char *func, uint32_t line)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_rx_buf_smmu_pool_mapping)
		return soc->ops->ipa_ops->ipa_rx_buf_smmu_pool_mapping(soc,
						pdev_id, create, func, line);

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS cdp_ipa_set_smmu_mapped(ol_txrx_soc_handle soc,
						 int val)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_set_smmu_mapped)
		return soc->ops->ipa_ops->ipa_set_smmu_mapped(soc, val);

	return QDF_STATUS_SUCCESS;
}

static inline int cdp_ipa_get_smmu_mapped(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_get_smmu_mapped)
		return soc->ops->ipa_ops->ipa_get_smmu_mapped(soc);

	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_WDS_EASYMESH_FEATURE
/**
 * cdp_ipa_ast_create() - Create/update AST entry in AST table
 *			  for learning/roaming packets from IPA
 * @soc: data path soc handle
 * @data: Structure used for updating the AST table
 *
 * Create/update AST entry in AST table for learning/roaming packets from IPA
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_ast_create(ol_txrx_soc_handle soc, qdf_ipa_ast_info_type_t *data)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_ast_create)
		return soc->ops->ipa_ops->ipa_ast_create(soc, data);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef IPA_OPT_WIFI_DP
/*
 * cdp_ipa_pcie_link_up() - Send request to hold PCIe link in L0
 * @soc - cdp soc handle
 *
 * Return: 0 for success, negative for failure
 */
static inline int
cdp_ipa_pcie_link_up(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_pcie_link_up)
		return soc->ops->ipa_ops->ipa_pcie_link_up(soc);

	return QDF_STATUS_SUCCESS;
}

/*
 * cdp_ipa_pcie_link_down() - Release request to hold PCIe link in L0
 * @soc - cdp soc handle
 *
 * Return: 0 for success, negative for failure
 */
static inline int
cdp_ipa_pcie_link_down(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_pcie_link_down)
		soc->ops->ipa_ops->ipa_pcie_link_down(soc);

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * cdp_ipa_update_peer_rx_stats() - update peer rx stats
 * @soc: data path soc handle
 * @vdev_id: vdev id
 * @peer_mac: Peer Mac Address
 * @nbuf: pointer to data packet
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_ipa_update_peer_rx_stats(ol_txrx_soc_handle soc, uint8_t vdev_id,
			     uint8_t *peer_mac, qdf_nbuf_t nbuf)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_update_peer_rx_stats)
		return soc->ops->ipa_ops->ipa_update_peer_rx_stats(soc,
								   vdev_id,
								   peer_mac,
								   nbuf);

	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_OPT_WIFI_DP
#define RX_CCE_SUPER_RULE_SETUP_NUM 2
struct addr_params {
	uint8_t valid;
	uint8_t src_ipv4_addr[4];
	uint8_t dst_ipv4_addr[4];
	uint8_t src_ipv6_addr[16];
	uint8_t dst_ipv6_addr[16];
	uint8_t l4_type;
	uint16_t l3_type;
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t flt_hdl;
	uint8_t ipa_flt_evnt_required;
	bool ipa_flt_in_use;
};

struct wifi_dp_flt_setup {
	uint8_t pdev_id;
	uint8_t op;
	uint8_t num_filters;
	uint32_t ipa_flt_evnt_response;
	struct addr_params flt_addr_params[RX_CCE_SUPER_RULE_SETUP_NUM];
};

static inline QDF_STATUS
cdp_ipa_rx_cce_super_rule_setup(ol_txrx_soc_handle soc,
				void *flt_params)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->ipa_ops->ipa_rx_super_rule_setup)
		return soc->ops->ipa_ops->ipa_rx_super_rule_setup(soc,
								  flt_params);

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cdp_ipa_opt_dp_enable_disable_low_power_mode(struct wlan_objmgr_pdev *pdev,
					     uint32_t pdev_id, int param_val)
{
	wmi_unified_t wmi_handle;
	struct wmi_unified *pdev_wmi_handle = NULL;
	struct wlan_objmgr_psoc *psoc;
	struct pdev_params pparam;
	uint32_t vdev_id, val;
	QDF_STATUS status;

	psoc = wlan_pdev_get_psoc(pdev);
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "Unable to get wmi handle");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pdev_wmi_handle = pdev->tgt_if_handle->wmi_handle;
	qdf_mem_set(&pparam, sizeof(pparam), 0);
	pparam.is_host_pdev_id = false;

	/* Enable-disable IMPS */
	pparam.param_id = WMI_PDEV_PARAM_IDLE_PS_CONFIG;
	pparam.param_value = param_val;
	status =  wmi_unified_pdev_param_send(wmi_handle,
					      &pparam, pdev_id);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s Unable to enable/disable:(%d) IMPS", __func__,
			  param_val);
		return QDF_STATUS_E_FAILURE;
	}

	/* Enable-disable ILP */
	pparam.param_id = WMI_PDEV_PARAM_PCIE_HW_ILP;
	pparam.param_value = param_val;
	status =  wmi_unified_pdev_param_send(pdev_wmi_handle,
					      &pparam, pdev_id);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s Unable to enable/disable:(%d) ILP", __func__,
			  param_val);
		return QDF_STATUS_E_FAILURE;
	}

	/* Enable-disable BMPS */
	val = param_val;
	vdev_id = 0; //TODO fix vdev_id
	status = wmi_unified_set_sta_ps_mode(wmi_handle, vdev_id, val);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s Unable to enable/disable:(%d) BMPS", __func__,
			  param_val);
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}
#endif /* IPA_OPT_WIFI_DP */

/**
 * cdp_ipa_get_wdi_version - Get WDI version
 * @soc: data path soc handle
 * @wdi_ver: Out param for wdi version
 *
 * Return: None
 */
static inline void
cdp_ipa_get_wdi_version(ol_txrx_soc_handle soc, uint8_t *wdi_ver)
{
	if (!soc || !soc->ops || !soc->ops->ipa_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->ipa_ops->ipa_get_wdi_version)
		soc->ops->ipa_ops->ipa_get_wdi_version(soc, wdi_ver);
}
#endif /* IPA_OFFLOAD */
#endif /* _CDP_TXRX_IPA_H_ */

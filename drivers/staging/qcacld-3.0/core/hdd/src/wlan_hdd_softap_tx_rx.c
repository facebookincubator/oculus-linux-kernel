/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/* denote that this file does not allow legacy hddLog */
#define HDD_DISALLOW_LEGACY_HDDLOG 1

/* Include files */
#include <linux/semaphore.h>
#include "osif_sync.h"
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <qdf_types.h>
#include <ani_global.h>
#include <qdf_types.h>
#include <net/ieee80211_radiotap.h>
#include <cds_sched.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cds_utils.h>
#include <cdp_txrx_flow_ctrl_v2.h>
#include <cdp_txrx_misc.h>
#include <wlan_hdd_object_manager.h>
#include "wlan_p2p_ucfg_api.h"
#include <wlan_hdd_regulatory.h>
#include "wlan_ipa_ucfg_api.h"
#include "wlan_dp_ucfg_api.h"
#include "wlan_policy_mgr_ucfg.h"
#include <wma_types.h>
#include "wlan_hdd_sta_info.h"
#include "ol_defines.h"
#include <wlan_hdd_sar_limits.h>
#include "wlan_hdd_tsf.h"
#include "wlan_hdd_wds.h"
#include <cdp_txrx_ctrl.h>
#ifdef FEATURE_WDS
#include <net/llc_pdu.h>
#endif
#include <os_if_dp.h>
#include <cfg_ucfg_api.h>
#include <wlan_twt_ucfg_ext_api.h>
#include "wlan_hdd_stats.h"

/* Preprocessor definitions and constants */
#undef QCA_HDD_SAP_DUMP_SK_BUFF

/* Type declarations */
#ifdef FEATURE_WDS
/**
 * struct l2_update_frame - Layer-2 update frame format
 * @eh: ethernet header
 * @l2_update_pdu: llc pdu format
 * @l2_update_xid_info: xid command information field
 */
struct l2_update_frame {
	struct ethhdr eh;
	struct llc_pdu_un l2_update_pdu;
	struct llc_xid_info l2_update_xid_info;
} qdf_packed;
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if (!adapter) {
		hdd_err("NULL adapter");
		return;
	}

	hdd_debug("Enabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_WAKE_ALL_NETIF_QUEUE,
				     WLAN_CONTROL_PATH);
}

/**
 * hdd_softap_tx_resume_false() - Resume OS TX Q false leads to queue disabling
 * @adapter: pointer to hdd adapter
 * @tx_resume: TX Q resume trigger
 *
 *
 * Return: None
 */
static void
hdd_softap_tx_resume_false(struct hdd_adapter *adapter, bool tx_resume)
{
	QDF_STATUS status;
	qdf_mc_timer_t *fc_timer;

	if (true == tx_resume)
		return;

	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_STOP_ALL_NETIF_QUEUE,
				     WLAN_DATA_FLOW_CONTROL);

	fc_timer = &adapter->tx_flow_control_timer;
	if (QDF_TIMER_STATE_STOPPED != qdf_mc_timer_get_current_state(fc_timer))
		return;

	status = qdf_mc_timer_start(fc_timer,
				    WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to start tx_flow_control_timer");
	else
		adapter->deflink->hdd_stats.tx_rx_stats.txflow_timer_cnt++;
}

void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if (!adapter) {
		hdd_err("NULL adapter");
		return;
	}

	/* Resume TX  */
	if (true == tx_resume) {
		if (QDF_TIMER_STATE_STOPPED !=
		    qdf_mc_timer_get_current_state(&adapter->
						   tx_flow_control_timer)) {
			qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		}

		hdd_debug("Enabling queues");
		wlan_hdd_netif_queue_control(adapter,
					WLAN_WAKE_ALL_NETIF_QUEUE,
					WLAN_DATA_FLOW_CONTROL);
	}
	hdd_softap_tx_resume_false(adapter, tx_resume);
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#ifndef MDM_PLATFORM
void hdd_ipa_update_rx_mcbc_stats(struct hdd_adapter *adapter,
				  struct sk_buff *skb)
{
	struct hdd_station_info *hdd_sta_info;
	struct qdf_mac_addr *src_mac;
	qdf_ether_header_t *eh;

	src_mac = (struct qdf_mac_addr *)(skb->data +
					  QDF_NBUF_SRC_MAC_OFFSET);

	hdd_sta_info = hdd_get_sta_info_by_mac(
				&adapter->sta_info_list,
				src_mac->bytes,
				STA_INFO_SOFTAP_IPA_RX_PKT_CALLBACK);
	if (!hdd_sta_info)
		return;

	if (qdf_nbuf_data_is_ipv4_mcast_pkt(skb->data))
		hdd_sta_info->rx_mc_bc_cnt++;

	eh = (qdf_ether_header_t *)qdf_nbuf_data(skb);
	if (QDF_IS_ADDR_BROADCAST(eh->ether_dhost))
		hdd_sta_info->rx_mc_bc_cnt++;

	hdd_put_sta_info_ref(&adapter->sta_info_list, &hdd_sta_info,
			     true, STA_INFO_SOFTAP_IPA_RX_PKT_CALLBACK);
}
#else
void hdd_ipa_update_rx_mcbc_stats(struct hdd_adapter *adapter,
				  struct sk_buff *skb)
{
}
#endif

/**
 * __hdd_softap_hard_start_xmit() - Transmit a frame
 * @skb: pointer to OS packet (sk_buff)
 * @dev: pointer to network device
 *
 * Function registered with the Linux OS for transmitting
 * packets. This version of the function directly passes
 * the packet to Datapath Layer.
 * In case of any error, drop the packet.
 *
 * Return: None
 */
static void __hdd_softap_hard_start_xmit(struct sk_buff *skb,
					 struct net_device *dev)
{
	sme_ac_enum_type ac = SME_AC_BE;
	struct hdd_adapter *adapter = (struct hdd_adapter *)netdev_priv(dev);
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	struct hdd_tx_rx_stats *stats =
				&adapter->deflink->hdd_stats.tx_rx_stats;
	int cpu = qdf_get_smp_processor_id();
	QDF_STATUS status;

	osif_dp_mark_pkt_type(skb);
	hdd_tx_latency_record_ingress_ts(adapter, skb);

	/* Get TL AC corresponding to Qdisc queue index/AC. */
	ac = hdd_qdisc_ac_to_tl_ac[skb->queue_mapping];
	++stats->per_cpu[cpu].tx_classified_ac[ac];

	status = ucfg_dp_softap_start_xmit((qdf_nbuf_t)skb,
					   adapter->deflink->vdev);
	if (QDF_IS_STATUS_ERROR(status))
		++stats->per_cpu[cpu].tx_dropped_ac[ac];

	netif_trans_update(dev);

	wlan_hdd_sar_unsolicited_timer_start(hdd_ctx);
}

netdev_tx_t hdd_softap_hard_start_xmit(struct sk_buff *skb,
				       struct net_device *net_dev)
{
	__hdd_softap_hard_start_xmit(skb, net_dev);

	return NETDEV_TX_OK;
}

QDF_STATUS hdd_softap_ipa_start_xmit(qdf_nbuf_t nbuf, qdf_netdev_t dev)
{
	if (NETDEV_TX_OK == hdd_softap_hard_start_xmit(
					(struct sk_buff *)nbuf,
					(struct net_device *)dev))
		return QDF_STATUS_SUCCESS;
	else
		return QDF_STATUS_E_FAILURE;
}

static void __hdd_softap_tx_timeout(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct netdev_queue *txq;
	int i;

	DPTRACE(qdf_dp_trace(NULL, QDF_DP_TRACE_HDD_SOFTAP_TX_TIMEOUT,
			QDF_TRACE_DEFAULT_PDEV_ID,
			NULL, 0, QDF_TX));
	/* Getting here implies we disabled the TX queues for too
	 * long. Queues are disabled either because of disassociation
	 * or low resource scenarios. In case of disassociation it is
	 * ok to ignore this. But if associated, we have do possible
	 * recovery here
	 */
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
			 "%s: Recovery in Progress. Ignore!!!", __func__);
		return;
	}

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_debug("wlan is suspended, ignore timeout");
		return;
	}

	TX_TIMEOUT_TRACE(dev, QDF_MODULE_ID_HDD_SAP_DATA);

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(dev, i);
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
			  QDF_TRACE_LEVEL_DEBUG,
			  "Queue: %d status: %d txq->trans_start: %lu",
			  i, netif_tx_queue_stopped(txq), txq->trans_start);
	}

	wlan_hdd_display_adapter_netif_queue_history(adapter);

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_DP_ID);
	if (vdev) {
		ucfg_dp_softap_tx_timeout(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}

	QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_DEBUG,
			"carrier state: %d", netif_carrier_ok(dev));
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_softap_tx_timeout(struct net_device *net_dev, unsigned int txqueue)
#else
void hdd_softap_tx_timeout(struct net_device *net_dev)
#endif
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return;

	__hdd_softap_tx_timeout(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);
}

static void
hdd_reset_sta_info_during_reattach(struct hdd_station_info *sta_info)
{
	sta_info->in_use = 0;
	sta_info->sta_id = 0;
	sta_info->sta_type = 0;
	qdf_mem_zero(&sta_info->sta_mac, QDF_MAC_ADDR_SIZE);
	sta_info->peer_state = 0;
	sta_info->is_qos_enabled = 0;
	sta_info->is_deauth_in_progress = 0;
	sta_info->nss = 0;
	sta_info->rate_flags = 0;
	sta_info->ecsa_capable = 0;
	sta_info->max_phy_rate = 0;
	sta_info->tx_packets = 0;
	sta_info->tx_bytes = 0;
	sta_info->rx_packets = 0;
	sta_info->rx_bytes = 0;
	sta_info->last_tx_rx_ts = 0;
	sta_info->assoc_ts = 0;
	sta_info->disassoc_ts = 0;
	sta_info->tx_rate = 0;
	sta_info->rx_rate = 0;
	sta_info->ampdu = 0;
	sta_info->sgi_enable = 0;
	sta_info->tx_stbc = 0;
	sta_info->rx_stbc = 0;
	sta_info->ch_width = 0;
	sta_info->mode = 0;
	sta_info->max_supp_idx = 0;
	sta_info->max_ext_idx = 0;
	sta_info->max_mcs_idx = 0;
	sta_info->rx_mcs_map = 0;
	sta_info->tx_mcs_map = 0;
	sta_info->freq = 0;
	sta_info->dot11_mode = 0;
	sta_info->ht_present = 0;
	sta_info->vht_present = 0;
	qdf_mem_zero(&sta_info->ht_caps, sizeof(sta_info->ht_caps));
	qdf_mem_zero(&sta_info->vht_caps, sizeof(sta_info->vht_caps));
	sta_info->reason_code = 0;
	sta_info->rssi = 0;
	sta_info->dhcp_phase = 0;
	sta_info->dhcp_nego_status = 0;
	sta_info->capability = 0;
	sta_info->support_mode = 0;
	sta_info->rx_retry_cnt = 0;
	sta_info->rx_mc_bc_cnt = 0;

	if (sta_info->assoc_req_ies.len) {
		qdf_mem_free(sta_info->assoc_req_ies.ptr);
		sta_info->assoc_req_ies.ptr = NULL;
		sta_info->assoc_req_ies.len = 0;
	}

	sta_info->pending_eap_frm_type = 0;
}

/**
 * hdd_sta_info_re_attach() - Re-Attach the station info structure into the list
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that is to be attached to the
 *            container object.
 * @sta_mac: MAC address of the station
 *
 * This function re-attaches the station if it gets re-connect after
 * disconnecting and before its all references are released.
 *
 * Return: QDF STATUS SUCCESS on successful attach, error code otherwise
 */
static QDF_STATUS hdd_sta_info_re_attach(
				struct hdd_sta_info_obj *sta_info_container,
				struct hdd_station_info *sta_info,
				struct qdf_mac_addr *sta_mac)
{
	if (!sta_info_container || !sta_info) {
		hdd_err("Parameter(s) null");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	if (sta_info->is_attached) {
		qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);
		hdd_err("sta info is already attached");
		return QDF_STATUS_SUCCESS;
	}

	hdd_reset_sta_info_during_reattach(sta_info);
	/* Add one extra ref for reattach */
	hdd_take_sta_info_ref(sta_info_container, sta_info, false,
			      STA_INFO_ATTACH_DETACH);
	qdf_mem_copy(&sta_info->sta_mac, sta_mac, sizeof(struct qdf_mac_addr));
	sta_info->is_attached = true;
	qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_softap_init_tx_rx_sta(struct hdd_adapter *adapter,
				     struct qdf_mac_addr *sta_mac)
{
	struct hdd_station_info *sta_info;
	QDF_STATUS status;

	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   sta_mac->bytes,
					   STA_INFO_SOFTAP_INIT_TX_RX_STA);

	if (sta_info) {
		hdd_err("Reinit of in use station " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(sta_mac->bytes));
		status = hdd_sta_info_re_attach(&adapter->sta_info_list,
						sta_info, sta_mac);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_INIT_TX_RX_STA);
		return status;
	}

	sta_info = qdf_mem_malloc(sizeof(struct hdd_station_info));
	if (!sta_info)
		return QDF_STATUS_E_NOMEM;

	sta_info->is_deauth_in_progress = false;
	qdf_mem_copy(&sta_info->sta_mac, sta_mac, sizeof(struct qdf_mac_addr));

	status = hdd_sta_info_attach(&adapter->sta_info_list, sta_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to attach station: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(sta_mac->bytes));
		qdf_mem_free(sta_info);
	}

	return status;
}

QDF_STATUS hdd_softap_deregister_sta(struct hdd_adapter *adapter,
				     struct hdd_station_info **sta_info)
{
	struct hdd_context *hdd_ctx;
	struct qdf_mac_addr *mac_addr;
	struct hdd_station_info *sta = *sta_info;
	struct hdd_ap_ctx *ap_ctx;
	struct wlan_objmgr_vdev *vdev;

	if (!adapter) {
		hdd_err("NULL adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("Invalid adapter magic");
		return QDF_STATUS_E_INVAL;
	}

	if (!sta) {
		hdd_err("Invalid station");
		return QDF_STATUS_E_INVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * If the address is a broadcast address then the CDP layers expects
	 * the self mac address of the adapter.
	 */
	if (QDF_IS_ADDR_BROADCAST(sta->sta_mac.bytes)) {
		mac_addr = &adapter->mac_addr;
	} else {
		if (wlan_vdev_mlme_is_mlo_vdev(adapter->deflink->vdev) &&
		    !qdf_is_macaddr_zero(&sta->mld_addr))
			mac_addr = &sta->mld_addr;
		else
			mac_addr = &sta->sta_mac;
	}

	if (ucfg_ipa_is_enabled()) {
		if (ucfg_ipa_wlan_evt(hdd_ctx->pdev, adapter->dev,
				      adapter->device_mode,
				      adapter->deflink->vdev_id,
				      WLAN_IPA_CLIENT_DISCONNECT,
				      mac_addr->bytes,
				      false) != QDF_STATUS_SUCCESS)
			hdd_debug("WLAN_CLIENT_DISCONNECT event failed");
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_DP_ID);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	ucfg_dp_del_latency_critical_client(vdev, sta->dot11_mode);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter->deflink);
	if (!QDF_IS_ADDR_BROADCAST(sta->sta_mac.bytes) &&
	    sta->dot11_mode < QCA_WLAN_802_11_MODE_INVALID)
		ap_ctx->client_count[sta->dot11_mode]--;

	hdd_sta_info_detach(&adapter->sta_info_list, &sta);

	ucfg_mlme_update_oce_flags(hdd_ctx->pdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_softap_register_sta(struct wlan_hdd_link_info *link_info,
				   bool auth_required, bool privacy_required,
				   struct qdf_mac_addr *sta_mac,
				   tSap_StationAssocReassocCompleteEvent *event)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct ol_txrx_desc_type txrx_desc = {0};
	struct hdd_adapter *adapter = link_info->adapter;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct hdd_ap_ctx *ap_ctx;
	struct hdd_station_info *sta_info;
	bool wmm_enabled = false;
	enum qca_wlan_802_11_mode dot11mode = QCA_WLAN_802_11_MODE_INVALID;
	bool is_macaddr_broadcast = false;
	enum phy_ch_width ch_width;
	struct wlan_objmgr_vdev *vdev;

	if (event) {
		wmm_enabled = event->wmmEnabled;
		dot11mode = hdd_convert_dot11mode_from_phymode(event->chan_info.info);
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(link_info);

	/*
	 * If the address is a broadcast address, then provide the self mac addr
	 * to the data path. Else provide the mac address of the connected peer.
	 */
	if (qdf_is_macaddr_broadcast(sta_mac)) {
		qdf_mem_copy(&txrx_desc.peer_addr, &adapter->mac_addr,
			     QDF_MAC_ADDR_SIZE);
		is_macaddr_broadcast = true;
	} else {
		qdf_mem_copy(&txrx_desc.peer_addr, sta_mac,
			     QDF_MAC_ADDR_SIZE);
	}

	qdf_status = hdd_softap_init_tx_rx_sta(adapter, sta_mac);
	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   sta_mac->bytes,
					   STA_INFO_SOFTAP_REGISTER_STA);

	if (!sta_info) {
		hdd_debug("STA not found");
		return QDF_STATUS_E_INVAL;
	}

	txrx_desc.is_qos_enabled = wmm_enabled;

	vdev = hdd_objmgr_get_vdev_by_user(link_info, WLAN_DP_ID);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	ucfg_dp_add_latency_critical_client(vdev, dot11mode);

	if (is_macaddr_broadcast) {
		/*
		 * Register the vdev transmit and receive functions once with
		 * CDP layer. Broadcast STA is registered only once when BSS is
		 * started.
		 */
		qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));
		txrx_ops.tx.tx_classify_critical_pkt_cb =
					hdd_wmm_classify_pkt_cb;
		ucfg_dp_softap_register_txrx_ops(vdev, &txrx_ops);
	}
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

	ch_width = ucfg_mlme_get_peer_ch_width(adapter->hdd_ctx->psoc,
					       txrx_desc.peer_addr.bytes);
	txrx_desc.bw = hdd_convert_ch_width_to_cdp_peer_bw(ch_width);
	qdf_status = cdp_peer_register(soc, OL_TXRX_PDEV_ID, &txrx_desc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_debug("cdp_peer_register() failed to register.  Status = %d [0x%08X]",
			  qdf_status, qdf_status);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_REGISTER_STA);
		return qdf_status;
	}

	/* if ( WPA ), tell TL to go to 'connected' and after keys come to the
	 * driver then go to 'authenticated'.  For all other authentication
	 * types (those that do not require upper layer authentication) we can
	 * put TL directly into 'authenticated' state
	 */

	sta_info->is_qos_enabled = wmm_enabled;

	if (!auth_required) {
		hdd_debug("open/shared auth STA MAC= " QDF_MAC_ADDR_FMT
			  ".  Changing TL state to AUTHENTICATED at Join time",
			 QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));

		/* Connections that do not need Upper layer auth,
		 * transition TL directly to 'Authenticated' state.
		 */
		qdf_status = hdd_change_peer_state(link_info,
						   txrx_desc.peer_addr.bytes,
						   OL_TXRX_PEER_STATE_AUTH);

		sta_info->peer_state = OL_TXRX_PEER_STATE_AUTH;
		if (!qdf_is_macaddr_broadcast(sta_mac))
			qdf_status = wlan_hdd_send_sta_authorized_event(
							adapter, hdd_ctx,
							sta_mac);
	} else {

		hdd_debug("ULA auth STA MAC = " QDF_MAC_ADDR_FMT
			  ".  Changing TL state to CONNECTED at Join time",
			 QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));

		qdf_status = hdd_change_peer_state(link_info,
						   txrx_desc.peer_addr.bytes,
						   OL_TXRX_PEER_STATE_CONN);

		sta_info->peer_state = OL_TXRX_PEER_STATE_CONN;
	}

	if (!qdf_is_macaddr_broadcast(sta_mac) &&
	    dot11mode < QCA_WLAN_802_11_MODE_INVALID)
		ap_ctx->client_count[dot11mode]++;
	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_REGISTER_STA);

	if (is_macaddr_broadcast) {
		hdd_debug("Enabling queues");
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
					     WLAN_CONTROL_PATH);
	}
	ucfg_mlme_update_oce_flags(hdd_ctx->pdev);
	ucfg_twt_init_context(hdd_ctx->psoc, sta_mac,
			      TWT_ALL_SESSIONS_DIALOG_ID);
	return qdf_status;
}

QDF_STATUS
hdd_softap_register_bc_sta(struct wlan_hdd_link_info *link_info,
			   bool privacy_required)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct qdf_mac_addr broadcast_macaddr = QDF_MAC_ADDR_BCAST_INIT;
	struct hdd_ap_ctx *ap_ctx;
	uint8_t sta_id;

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(link_info);
	sta_id = ap_ctx->broadcast_sta_id;
	if (sta_id >= WLAN_MAX_STA_COUNT) {
		hdd_err("Error: Invalid sta_id: %u", sta_id);
		return qdf_status;
	}

	qdf_status = hdd_softap_register_sta(link_info, false,
					     privacy_required,
					     &broadcast_macaddr, NULL);

	return qdf_status;
}

QDF_STATUS hdd_softap_stop_bss(struct hdd_adapter *adapter)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t indoor_chnl_marking = 0;
	struct hdd_context *hdd_ctx;
	struct hdd_station_info *sta_info, *tmp = NULL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = ucfg_policy_mgr_get_indoor_chnl_marking(hdd_ctx->psoc,
							 &indoor_chnl_marking);
	if (QDF_STATUS_SUCCESS != status)
		hdd_err("can't get indoor channel marking, using default");
	/* This is stop bss callback running in scheduler thread so do not
	 * driver unload in progress check otherwise it can lead to peer
	 * object leak
	 */

	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_SOFTAP_STOP_BSS) {
		status = hdd_softap_deregister_sta(adapter, &sta_info);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_STOP_BSS);
	}

	if (adapter->device_mode == QDF_SAP_MODE &&
	    !hdd_ctx->config->disable_channel)
		wlan_hdd_restore_channels(hdd_ctx);

	/*  Mark the indoor channel (passive) to enable  */
	if (indoor_chnl_marking && adapter->device_mode == QDF_SAP_MODE) {
		hdd_update_indoor_channel(hdd_ctx, false);
		sme_update_channel_list(hdd_ctx->mac_handle);
	}

	if (ucfg_ipa_is_enabled()) {
		if (ucfg_ipa_wlan_evt(hdd_ctx->pdev,
				      adapter->dev,
				      adapter->device_mode,
				      adapter->deflink->vdev_id,
				      WLAN_IPA_AP_DISCONNECT,
				      adapter->dev->dev_addr,
				      false) != QDF_STATUS_SUCCESS)
			hdd_err("WLAN_AP_DISCONNECT event failed");
	}

	/* Setting the RTS profile to original value */
	if (sme_cli_set_command(adapter->deflink->vdev_id,
				wmi_vdev_param_enable_rtscts,
				cfg_get(hdd_ctx->psoc,
					CFG_ENABLE_FW_RTS_PROFILE),
				VDEV_CMD))
		hdd_debug("Failed to set RTS_PROFILE");

	return status;
}

/**
 * hdd_softap_change_per_sta_state() - Change the state of a SoftAP station
 * @adapter: pointer to adapter context
 * @sta_mac: MAC address of the station
 * @state: new state of the station
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
static QDF_STATUS hdd_softap_change_per_sta_state(struct hdd_adapter *adapter,
						  struct qdf_mac_addr *sta_mac,
						  enum ol_txrx_peer_state state)
{
	QDF_STATUS qdf_status;
	struct hdd_station_info *sta_info;
	struct qdf_mac_addr mac_addr;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(adapter->dev);

	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   sta_mac->bytes,
					   STA_INFO_SOFTAP_CHANGE_STA_STATE);

	if (!sta_info) {
		hdd_debug("Failed to find right station MAC: " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(sta_mac->bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_is_macaddr_broadcast(&sta_info->sta_mac))
		qdf_mem_copy(&mac_addr, &adapter->mac_addr, QDF_MAC_ADDR_SIZE);
	else
		qdf_mem_copy(&mac_addr, sta_mac, QDF_MAC_ADDR_SIZE);

	qdf_status =
		hdd_change_peer_state(adapter->deflink, mac_addr.bytes, state);
	hdd_debug("Station " QDF_MAC_ADDR_FMT " changed to state %d",
		  QDF_MAC_ADDR_REF(mac_addr.bytes), state);

	if (QDF_IS_STATUS_ERROR(qdf_status))
		goto put_ref;

	sta_info->peer_state = OL_TXRX_PEER_STATE_AUTH;
	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_OSIF_ID);
	if (vdev) {
		p2p_peer_authorized(vdev, sta_mac->bytes);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
	} else {
		hdd_err("vdev is NULL");
	}

put_ref:
	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_CHANGE_STA_STATE);
	hdd_exit();
	return qdf_status;
}

QDF_STATUS hdd_softap_change_sta_state(struct hdd_adapter *adapter,
				       struct qdf_mac_addr *sta_mac,
				       enum ol_txrx_peer_state state)
{
	struct qdf_mac_addr *mldaddr;
	struct wlan_objmgr_peer *peer;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct hdd_context *hdd_ctx;

	status = hdd_softap_change_per_sta_state(adapter, sta_mac, state);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!hdd_ctx) {
		hdd_err("hdd ctx is null");
		return status;
	}
	peer = wlan_objmgr_get_peer_by_mac(hdd_ctx->psoc,
					   sta_mac->bytes,
					   WLAN_LEGACY_MAC_ID);

	if (!peer) {
		hdd_debug("peer is null");
		return status;
	}
	mldaddr = (struct qdf_mac_addr *)wlan_peer_mlme_get_mldaddr(peer);
	if (mldaddr && !qdf_is_macaddr_zero(mldaddr))
		status = hdd_softap_change_per_sta_state(adapter, mldaddr,
							 state);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);

	return status;
}

#ifdef FEATURE_WDS
QDF_STATUS hdd_softap_ind_l2_update(struct hdd_adapter *adapter,
				    struct qdf_mac_addr *sta_mac)
{
	qdf_nbuf_t nbuf;
	struct l2_update_frame *msg;

	nbuf = qdf_nbuf_alloc(NULL, sizeof(*msg), 0, 4, false);
	if (!nbuf)
		return QDF_STATUS_E_FAILURE;

	msg = (struct l2_update_frame *)qdf_nbuf_data(nbuf);

	/* 802.2 LLC XID update frame carried over 802.3 */
	ether_addr_copy(msg->eh.h_source, sta_mac->bytes);
	eth_broadcast_addr(msg->eh.h_dest);
	/* packet length - dummy 802.3 packet */
	msg->eh.h_proto = htons(sizeof(*msg) - sizeof(struct ethhdr));

	/* null DSAP and a null SSAP is a way to solicit a response from any
	 * station (i.e., any DA)
	 */
	msg->l2_update_pdu.dsap = LLC_NULL_SAP;
	msg->l2_update_pdu.ssap = LLC_NULL_SAP;

	/*
	 * unsolicited XID response frame to announce presence.
	 * lsb.11110101.
	 */
	msg->l2_update_pdu.ctrl_1 = LLC_PDU_TYPE_U | LLC_1_PDU_CMD_XID;

	/* XID information field 129.1.0 to indicate connectionless service */
	msg->l2_update_xid_info.fmt_id = LLC_XID_FMT_ID;
	msg->l2_update_xid_info.type = LLC_XID_NULL_CLASS_1;
	msg->l2_update_xid_info.rw = 0;

	qdf_nbuf_set_pktlen(nbuf, sizeof(*msg));
	nbuf->dev = adapter->dev;
	nbuf->protocol = eth_type_trans(nbuf, adapter->dev);
	qdf_net_buf_debug_release_skb(nbuf);
	netif_rx_ni(nbuf);

	return QDF_STATUS_SUCCESS;
}
#endif

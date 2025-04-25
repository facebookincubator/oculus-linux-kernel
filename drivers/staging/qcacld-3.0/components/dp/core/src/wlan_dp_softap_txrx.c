/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
  * DOC: wlan_dp_softap_txrx.c
  * DP Soft AP TX/RX path implementation
  *
  *
  */

#include <wlan_dp_priv.h>
#include <wlan_dp_main.h>
#include <wlan_dp_txrx.h>
#include "wlan_dp_public_struct.h"
#include <qdf_types.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include <cdp_txrx_flow_ctrl_v2.h>
#include "wlan_dp_rx_thread.h"
#include "nan_public_structs.h"
#include "nan_ucfg_api.h"
#include <wlan_cm_ucfg_api.h>
#include <enet.h>
#include <cds_utils.h>
#include <wlan_dp_bus_bandwidth.h>
#include <wlan_tdls_ucfg_api.h>
#include <qdf_trace.h>
#include <qdf_nbuf.h>
#include <qdf_net_stats.h>

/* Preprocessor definitions and constants */
#undef QCA_DP_SAP_DUMP_SK_BUFF

/* Type declarations */

/* Function definitions and documentation */
#ifdef QCA_DP_SAP_DUMP_SK_BUFF
/**
 * dp_softap_dump_nbuf() - Dump an nbuf
 * @nbuf: nbuf to dump
 *
 * Return: None
 */
static void dp_softap_dump_nbuf(qdf_nbuf_t nbuf)
{
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "%s: head = %pK ", __func__, nbuf->head);
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO,
		  "%s: tail = %pK ", __func__, nbuf->tail);
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "%s: end = %pK ", __func__, nbuf->end);
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "%s: len = %d ", __func__, nbuf->len);
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "%s: data_len = %d ", __func__, nbuf->data_len);
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "%s: mac_len = %d", __func__, nbuf->mac_len);

	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x ", nbuf->data[0],
		  nbuf->data[1], nbuf->data[2], nbuf->data[3], nbuf->data[4],
		  nbuf->data[5], nbuf->data[6], nbuf->data[7]);
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", nbuf->data[8],
		  nbuf->data[9], nbuf->data[10], nbuf->data[11], nbuf->data[12],
		  nbuf->data[13], nbuf->data[14], nbuf->data[15]);
}
#else
static inline void dp_softap_dump_nbuf(qdf_nbuf_t nbuf)
{
}
#endif

#define IEEE8021X_AUTH_TYPE_EAP 0
#define EAP_CODE_OFFSET 18
#define EAP_CODE_FAILURE 4

/* Wait EAP Failure frame timeout in (MS) */
#define EAP_FRM_TIME_OUT 80

/**
 * dp_softap_inspect_tx_eap_pkt() - Inspect eap pkt tx/tx-completion
 * @dp_intf: pointer to DP interface
 * @nbuf: pointer to n/w buffer
 * @tx_comp: tx sending or tx completion
 *
 * Inspect the EAP-Failure pkt tx sending and tx completion.
 *
 * Return: void
 */
static void dp_softap_inspect_tx_eap_pkt(struct wlan_dp_intf *dp_intf,
					 qdf_nbuf_t nbuf,
					 bool tx_comp)
{
	struct qdf_mac_addr *mac_addr;
	uint8_t *data;
	uint8_t auth_type, eap_code;
	struct wlan_objmgr_peer *peer;
	struct wlan_dp_sta_info *sta_info;

	if (qdf_likely(QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) !=
	    QDF_NBUF_CB_PACKET_TYPE_EAPOL) ||
	    qdf_nbuf_len(nbuf) < (EAP_CODE_OFFSET + 1))
		return;

	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state() ||
	    cds_is_load_or_unload_in_progress()) {
		dp_debug("Recovery/(Un)load in Progress. Ignore!!!");
		return;
	}

	if (dp_intf->device_mode != QDF_P2P_GO_MODE)
		return;

	if (dp_intf->bss_state != BSS_INTF_START) {
		dp_debug("BSS intf state is not START");
		return;
	}
	data = qdf_nbuf_data(nbuf);
	auth_type = *(uint8_t *)(data + EAPOL_PACKET_TYPE_OFFSET);
	if (auth_type != IEEE8021X_AUTH_TYPE_EAP)
		return;
	eap_code = *(uint8_t *)(data + EAP_CODE_OFFSET);
	if (eap_code != EAP_CODE_FAILURE)
		return;
	mac_addr = (struct qdf_mac_addr *)qdf_nbuf_data(nbuf) +
		QDF_NBUF_DEST_MAC_OFFSET;

	peer = wlan_objmgr_get_peer_by_mac(dp_intf->dp_ctx->psoc,
					   mac_addr->bytes,
					   WLAN_DP_ID);
	if (!peer) {
		dp_err("Peer object not found");
		return;
	}
	sta_info = dp_get_peer_priv_obj(peer);
	if (!sta_info) {
		wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
		return;
	}

	if (tx_comp) {
		dp_info("eap_failure frm tx done" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		qdf_atomic_clear_bit(DP_PENDING_TYPE_EAP_FAILURE,
				     &sta_info->pending_eap_frm_type);
		qdf_event_set(&dp_intf->qdf_sta_eap_frm_done_event);
	} else {
		dp_info("eap_failure frm tx pending" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		qdf_event_reset(&dp_intf->qdf_sta_eap_frm_done_event);
		qdf_atomic_set_bit(DP_PENDING_TYPE_EAP_FAILURE,
				   &sta_info->pending_eap_frm_type);
		QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(nbuf) = 1;
	}
	wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
}

void dp_softap_check_wait_for_tx_eap_pkt(struct wlan_dp_intf *dp_intf,
					 struct qdf_mac_addr *mac_addr)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_dp_sta_info *sta_info;
	QDF_STATUS qdf_status;

	if (dp_intf->device_mode != QDF_P2P_GO_MODE)
		return;

	peer = wlan_objmgr_get_peer_by_mac(dp_intf->dp_ctx->psoc,
					   mac_addr->bytes,
					   WLAN_DP_ID);
	if (!peer) {
		dp_err("Peer object not found");
		return;
	}

	sta_info = dp_get_peer_priv_obj(peer);
	if (qdf_atomic_test_bit(DP_PENDING_TYPE_EAP_FAILURE,
				&sta_info->pending_eap_frm_type)) {
		dp_info("eap_failure frm pending" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		qdf_status = qdf_wait_for_event_completion(
				&dp_intf->qdf_sta_eap_frm_done_event,
				EAP_FRM_TIME_OUT);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			dp_debug("eap_failure tx timeout");
	}
	wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
}

#ifdef SAP_DHCP_FW_IND
/**
 * dp_post_dhcp_ind() - Send DHCP START/STOP indication to FW
 * @dp_link: DP link handle
 * @mac_addr: mac address
 * @dhcp_start: dhcp start
 *
 * Return: error number
 */
int dp_post_dhcp_ind(struct wlan_dp_link *dp_link, uint8_t *mac_addr,
		     bool dhcp_start)
{
	struct wlan_dp_intf *dp_intf;
	struct dp_dhcp_ind msg;
	struct wlan_dp_psoc_sb_ops *sb_ops;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	dp_info("Post DHCP indication,sta_mac=" QDF_MAC_ADDR_FMT
		 " ,  start=%u", QDF_MAC_ADDR_REF(mac_addr), dhcp_start);

	if (!is_dp_link_valid(dp_link)) {
		dp_err("NULL DP link");
		return QDF_STATUS_E_INVAL;
	}

	dp_intf = dp_link->dp_intf;
	/*
	 * If DP RX thread is enabled, RX DHCP packets are enqueue into
	 * DP RX thread queue, defer DHCP inspection until host has
	 * resumed entirely, no issue to send DHCP indication MSG.
	 * If DP RX thread is disabled, DHCP inspection happens earlier,
	 * skip sending DHCP indication MSG if host has not resumed.
	 */
	if (qdf_unlikely(!dp_intf->dp_ctx->enable_dp_rx_threads &&
			 dp_intf->dp_ctx->is_suspend)) {
		dp_err_rl("Device is system suspended, skip DHCP Ind");
		return QDF_STATUS_E_INVAL;
	}

	sb_ops = &dp_intf->dp_ctx->sb_ops;
	msg.dhcp_start = dhcp_start;
	msg.device_mode = dp_intf->device_mode;
	qdf_mem_copy(msg.intf_mac_addr.bytes,
		     dp_intf->mac_addr.bytes,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(msg.peer_mac_addr.bytes,
		     mac_addr,
		     QDF_MAC_ADDR_SIZE);

	status = sb_ops->dp_send_dhcp_ind(dp_link->link_id, &msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("Post DHCP Ind MSG fail");
		return QDF_STATUS_E_FAULT;
	}

	return 0;
}

#define DHCP_CLIENT_MAC_ADDR_OFFSET 0x46

/**
 * dp_softap_notify_dhcp_ind() - Notify SAP for DHCP indication for tx desc
 * @link_context: DP link context
 * @nbuf: pointer to OS packet (sk_buff)
 *
 * Return: None
 */
static void dp_softap_notify_dhcp_ind(void *link_context, qdf_nbuf_t nbuf)
{
	uint8_t *dest_mac_addr;
	struct wlan_dp_link *dp_link = link_context;

	if (!is_dp_link_valid(dp_link))
		return;

	dest_mac_addr = qdf_nbuf_data(nbuf) + DHCP_CLIENT_MAC_ADDR_OFFSET;

	/*stop dhcp indication*/
	dp_post_dhcp_ind(dp_link, dest_mac_addr, false);
}

int dp_softap_inspect_dhcp_packet(struct wlan_dp_link *dp_link,
				  qdf_nbuf_t nbuf,
				  enum qdf_proto_dir dir)
{
	struct wlan_dp_intf *dp_intf = dp_link->dp_intf;
	enum qdf_proto_subtype subtype = QDF_PROTO_INVALID;
	struct wlan_objmgr_peer *peer;
	struct wlan_dp_sta_info *sta_info;
	int errno = 0;
	struct qdf_mac_addr *src_mac;

	if (((dp_intf->device_mode == QDF_SAP_MODE) ||
	     (dp_intf->device_mode == QDF_P2P_GO_MODE)) &&
	    ((dir == QDF_TX && QDF_NBUF_CB_PACKET_TYPE_DHCP ==
				QDF_NBUF_CB_GET_PACKET_TYPE(nbuf)) ||
	     (dir == QDF_RX && qdf_nbuf_is_ipv4_dhcp_pkt(nbuf) == true))) {
		src_mac = (struct qdf_mac_addr *)(qdf_nbuf_data(nbuf) +
						  DHCP_CLIENT_MAC_ADDR_OFFSET);

		subtype = qdf_nbuf_get_dhcp_subtype(nbuf);

		peer = wlan_objmgr_get_peer_by_mac(dp_intf->dp_ctx->psoc,
						   src_mac->bytes,
						   WLAN_DP_ID);
		if (!peer) {
			dp_err("Peer object not found");
			return QDF_STATUS_E_INVAL;
		}

		sta_info = dp_get_peer_priv_obj(peer);
		if (!sta_info) {
			dp_err("Station not found");
			wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
			return QDF_STATUS_E_INVAL;
		}

		dp_info("ENTER: type=%d, phase=%d, nego_status=%d",
			subtype,
			sta_info->dhcp_phase,
			sta_info->dhcp_nego_status);

		switch (subtype) {
		case QDF_PROTO_DHCP_DISCOVER:
			if (dir != QDF_RX)
				break;
			if (sta_info->dhcp_nego_status == DHCP_NEGO_STOP)
				errno =	dp_post_dhcp_ind(
						dp_link,
						sta_info->sta_mac.bytes,
						true);
			sta_info->dhcp_phase = DHCP_PHASE_DISCOVER;
			if (QDF_IS_STATUS_SUCCESS(errno))
				sta_info->dhcp_nego_status =
						DHCP_NEGO_IN_PROGRESS;
			break;
		case QDF_PROTO_DHCP_OFFER:
			sta_info->dhcp_phase = DHCP_PHASE_OFFER;
			break;
		case QDF_PROTO_DHCP_REQUEST:
			if (dir != QDF_RX)
				break;
			if (sta_info->dhcp_nego_status == DHCP_NEGO_STOP)
				errno = dp_post_dhcp_ind(
						dp_link,
						sta_info->sta_mac.bytes,
						true);
			if (QDF_IS_STATUS_SUCCESS(errno))
				sta_info->dhcp_nego_status =
						DHCP_NEGO_IN_PROGRESS;
			fallthrough;
		case QDF_PROTO_DHCP_DECLINE:
			if (dir == QDF_RX)
				sta_info->dhcp_phase = DHCP_PHASE_REQUEST;
			break;
		case QDF_PROTO_DHCP_ACK:
		case QDF_PROTO_DHCP_NACK:
			sta_info->dhcp_phase = DHCP_PHASE_ACK;
			if (sta_info->dhcp_nego_status ==
				DHCP_NEGO_IN_PROGRESS) {
				dp_debug("Setting NOTIFY_COMP Flag");
				QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(nbuf) = 1;
			}
			sta_info->dhcp_nego_status = DHCP_NEGO_STOP;
			break;
		default:
			break;
		}

		wlan_objmgr_peer_release_ref(peer, WLAN_DP_ID);
		dp_info("EXIT: phase=%d, nego_status=%d",
			sta_info->dhcp_phase,
			sta_info->dhcp_nego_status);
	}

	return errno;
}
#else
static void dp_softap_notify_dhcp_ind(void *context, qdf_nbuf_t nbuf)
{
}
#endif /* SAP_DHCP_FW_IND */

#if defined(IPA_OFFLOAD)
static
qdf_nbuf_t dp_sap_nbuf_orphan(struct wlan_dp_intf *dp_intf,
			      qdf_nbuf_t nbuf)
{
	if (!qdf_nbuf_ipa_owned_get(nbuf)) {
		nbuf = dp_nbuf_orphan(dp_intf, nbuf);
	} else {
		/*
		 * Clear the IPA ownership after check it to avoid ipa_free_skb
		 * is called when Tx completed for intra-BSS Tx packets
		 */
		qdf_nbuf_ipa_owned_clear(nbuf);
	}
	return nbuf;
}
#else
static inline
qdf_nbuf_t dp_sap_nbuf_orphan(struct wlan_dp_intf *dp_intf,
			      qdf_nbuf_t nbuf)
{
	return dp_nbuf_orphan(dp_intf, nbuf);
}
#endif /* IPA_OFFLOAD */

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
static
void dp_softap_get_tx_resource(struct wlan_dp_link *dp_link,
			       qdf_nbuf_t nbuf)
{
	struct wlan_dp_intf *dp_intf = dp_link->dp_intf;

	if (QDF_NBUF_CB_GET_IS_BCAST(nbuf) || QDF_NBUF_CB_GET_IS_MCAST(nbuf))
		dp_get_tx_resource(dp_link, &dp_intf->mac_addr);
	else
		dp_get_tx_resource(dp_link,
				   (struct qdf_mac_addr *)(qdf_nbuf_data(nbuf) +
							   QDF_NBUF_DEST_MAC_OFFSET));
}
#else
#define dp_softap_get_tx_resource(dp_intf, nbuf)
#endif

#ifdef FEATURE_WDS
static void
dp_wds_replace_peer_mac(void *soc, struct wlan_dp_link *dp_link,
			uint8_t *mac_addr)
{
	struct cdp_ast_entry_info ast_entry_info = {0};
	cdp_config_param_type val;
	QDF_STATUS status;

	if (!cdp_find_peer_exist(soc, OL_TXRX_PDEV_ID, mac_addr)) {
		status = cdp_txrx_get_vdev_param(soc, dp_link->link_id,
						 CDP_ENABLE_WDS, &val);
		if (!QDF_IS_STATUS_SUCCESS(status))
			return;

		if (!val.cdp_vdev_param_wds)
			return;

		if (!cdp_peer_get_ast_info_by_soc(soc,  mac_addr,
						  &ast_entry_info))
			return;

		qdf_mem_copy(mac_addr, ast_entry_info.peer_mac_addr,
			     QDF_MAC_ADDR_SIZE);
	}
}
#else
static inline
void dp_wds_replace_peer_mac(void *soc, struct wlan_dp_link *dp_link,
			     uint8_t *mac_addr)
{
}
#endif /* FEATURE_WDS*/

static QDF_STATUS dp_softap_validate_peer_state(struct wlan_dp_link *dp_link,
						qdf_nbuf_t nbuf)
{
	struct qdf_mac_addr *dest_mac_addr;
	struct qdf_mac_addr mac_addr;
	enum ol_txrx_peer_state peer_state;
	void *soc;

	dest_mac_addr = (struct qdf_mac_addr *)(qdf_nbuf_data(nbuf) +
						QDF_NBUF_DEST_MAC_OFFSET);

	if (QDF_NBUF_CB_GET_IS_BCAST(nbuf) || QDF_NBUF_CB_GET_IS_MCAST(nbuf))
		return QDF_STATUS_SUCCESS;

	/* for a unicast frame */
	qdf_copy_macaddr(&mac_addr, dest_mac_addr);
	soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_BUG(soc);
	dp_wds_replace_peer_mac(soc, dp_link, mac_addr.bytes);
	peer_state = cdp_peer_state_get(soc, dp_link->link_id,
					mac_addr.bytes, false);

	if (peer_state == OL_TXRX_PEER_STATE_INVALID) {
		dp_debug_rl("Failed to find right station");
		return QDF_STATUS_E_FAILURE;
	}

	if (peer_state != OL_TXRX_PEER_STATE_CONN &&
	    peer_state != OL_TXRX_PEER_STATE_AUTH) {
		dp_debug_rl("Station not connected yet");
		return QDF_STATUS_E_FAILURE;
	}

	if (peer_state == OL_TXRX_PEER_STATE_CONN) {
		if (qdf_ntohs(qdf_nbuf_get_protocol(nbuf)) != ETHERTYPE_PAE &&
		    qdf_ntohs(qdf_nbuf_get_protocol(nbuf)) != ETHERTYPE_WAI) {
			dp_debug_rl("NON-EAPOL/WAPI pkt in non-Auth state");
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_softap_validate_driver_state(struct wlan_dp_intf *dp_intf)
{
	if (qdf_unlikely(cds_is_driver_transitioning())) {
		dp_err_rl("driver is transitioning, drop pkt");
		return QDF_STATUS_E_ABORTED;
	}

	/*
	 * below unified mask will take care of SAP TX block
	 * WLAN suspend state check
	 * BSS start check and
	 * DP TX function register check
	 */
	if (qdf_unlikely(dp_intf->sap_tx_block_mask)) {
		dp_err_rl("Softap TX blocked mask: %u",
			  dp_intf->sap_tx_block_mask);
		return QDF_STATUS_E_ABORTED;
	}

	return QDF_STATUS_SUCCESS;
}

static void dp_softap_config_tx_pkt_tracing(struct wlan_dp_intf *dp_intf,
					    qdf_nbuf_t nbuf)
{
	if (dp_is_current_high_throughput(dp_intf->dp_ctx))
		return;

	QDF_NBUF_CB_TX_PACKET_TRACK(nbuf) = QDF_NBUF_TX_PKT_DATA_TRACK;
	QDF_NBUF_UPDATE_TX_PKT_COUNT(nbuf, QDF_NBUF_TX_PKT_DP);
	qdf_dp_trace_set_track(nbuf, QDF_TX);
	DPTRACE(qdf_dp_trace(nbuf, QDF_DP_TRACE_TX_PACKET_PTR_RECORD,
			     QDF_TRACE_DEFAULT_PDEV_ID,
			     qdf_nbuf_data_addr(nbuf),
			     sizeof(qdf_nbuf_data(nbuf)),
			     QDF_TX));
}

#ifdef DP_TRAFFIC_END_INDICATION
/**
 * wlan_dp_traffic_end_indication_update_dscp() - Compare dscp derived from
 *                                                provided tos value with
 *                                                stored value and update if
 *                                                it's equal to special dscp
 * @dp_intf: pointer to DP interface
 * @tos: pointer to tos
 *
 * Return: True if tos is updated else False
 */
static inline bool
wlan_dp_traffic_end_indication_update_dscp(struct wlan_dp_intf *dp_intf,
					   uint8_t *tos)
{
	bool update;
	uint8_t dscp, ecn;

	ecn = (*tos & ~QDF_NBUF_PKT_IPV4_DSCP_MASK);
	dscp = (*tos & QDF_NBUF_PKT_IPV4_DSCP_MASK) >>
		QDF_NBUF_PKT_IPV4_DSCP_SHIFT;
	update = (dp_intf->traffic_end_ind.spl_dscp == dscp);
	if (update)
		*tos = ((dp_intf->traffic_end_ind.def_dscp <<
			 QDF_NBUF_PKT_IPV4_DSCP_SHIFT) | ecn);
	return update;
}

/**
 * dp_softap_inspect_traffic_end_indication_pkt() - Restore tos field for last
 *                                                  packet in data stream
 * @dp_intf: pointer to DP interface
 * @nbuf: pointer to OS packet
 *
 * Return: None
 */
static inline void
dp_softap_inspect_traffic_end_indication_pkt(struct wlan_dp_intf *dp_intf,
					     qdf_nbuf_t nbuf)
{
	uint8_t tos, tc;
	bool ret;

	if (qdf_nbuf_data_is_ipv4_pkt(qdf_nbuf_data(nbuf))) {
		tos = qdf_nbuf_data_get_ipv4_tos(qdf_nbuf_data(nbuf));
		ret = wlan_dp_traffic_end_indication_update_dscp(dp_intf, &tos);
		if (ret) {
			qdf_nbuf_data_set_ipv4_tos(qdf_nbuf_data(nbuf), tos);
			if (qdf_nbuf_is_ipv4_last_fragment(nbuf))
				QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) =
					QDF_NBUF_CB_PACKET_TYPE_END_INDICATION;
		}
	} else if (qdf_nbuf_is_ipv6_pkt(nbuf)) {
		tc = qdf_nbuf_data_get_ipv6_tc(qdf_nbuf_data(nbuf));
		ret = wlan_dp_traffic_end_indication_update_dscp(dp_intf, &tc);
		if (ret) {
			qdf_nbuf_data_set_ipv6_tc(qdf_nbuf_data(nbuf), tc);
			QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) =
				QDF_NBUF_CB_PACKET_TYPE_END_INDICATION;
		}
	}
}

/**
 * dp_softap_traffic_end_indication_enabled() - Check if traffic end indication
 *                                              is enabled or not
 * @dp_intf: pointer to DP interface
 *
 * Return: True or False
 */
static inline bool
dp_softap_traffic_end_indication_enabled(struct wlan_dp_intf *dp_intf)
{
	return qdf_unlikely(dp_intf->traffic_end_ind.enabled);
}
#else
static inline bool
dp_softap_traffic_end_indication_enabled(struct wlan_dp_intf *dp_intf)
{
	return false;
}

static inline void
dp_softap_inspect_traffic_end_indication_pkt(struct wlan_dp_intf *dp_intf,
					     qdf_nbuf_t nbuf)
{}
#endif

/**
 * dp_softap_start_xmit() - Transmit a frame
 * @nbuf: pointer to Network buffer
 * @dp_link: DP link handle
 *
 * Return: QDF_STATUS_SUCCESS on successful transmission
 */
QDF_STATUS dp_softap_start_xmit(qdf_nbuf_t nbuf, struct wlan_dp_link *dp_link)
{
	struct wlan_dp_intf *dp_intf = dp_link->dp_intf;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct qdf_mac_addr *dest_mac_addr;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t num_seg;
	struct dp_tx_rx_stats *stats = &dp_intf->dp_stats.tx_rx_stats;
	int cpu = qdf_get_smp_processor_id();

	dest_mac_addr = (struct qdf_mac_addr *)(qdf_nbuf_data(nbuf) +
						QDF_NBUF_DEST_MAC_OFFSET);
	++stats->per_cpu[cpu].tx_called;
	stats->cont_txtimeout_cnt = 0;

	if (QDF_IS_STATUS_ERROR(dp_softap_validate_driver_state(dp_intf)))
		goto drop_pkt;

	wlan_dp_pkt_add_timestamp(dp_intf, QDF_PKT_TX_DRIVER_ENTRY, nbuf);

	if (QDF_IS_STATUS_ERROR(dp_softap_validate_peer_state(dp_link, nbuf)))
		goto drop_pkt;

	dp_softap_get_tx_resource(dp_link, nbuf);

	nbuf = dp_sap_nbuf_orphan(dp_intf, nbuf);
	if (!nbuf)
		goto drop_pkt_accounting;

	qdf_net_buf_debug_acquire_skb(nbuf, __FILE__, __LINE__);

	qdf_net_stats_add_tx_bytes(&dp_intf->stats, qdf_nbuf_len(nbuf));

	if (qdf_nbuf_is_tso(nbuf)) {
		num_seg = qdf_nbuf_get_tso_num_seg(nbuf);
		qdf_net_stats_add_tx_pkts(&dp_intf->stats, num_seg);
	} else {
		qdf_net_stats_add_tx_pkts(&dp_intf->stats, 1);
		dp_ctx->no_tx_offload_pkt_cnt++;
	}

	QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(nbuf) = 0;

	if (qdf_unlikely(QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) ==
			 QDF_NBUF_CB_PACKET_TYPE_DHCP))
		dp_softap_inspect_dhcp_packet(dp_link, nbuf, QDF_TX);

	if (qdf_unlikely(QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) ==
			 QDF_NBUF_CB_PACKET_TYPE_EAPOL)) {
		dp_softap_inspect_tx_eap_pkt(dp_intf, nbuf, false);
		dp_event_eapol_log(nbuf, QDF_TX);
	}

	if (dp_softap_traffic_end_indication_enabled(dp_intf))
		dp_softap_inspect_traffic_end_indication_pkt(dp_intf, nbuf);

	dp_softap_config_tx_pkt_tracing(dp_intf, nbuf);

	/* check whether need to linearize skb, like non-linear udp data */
	if (dp_nbuf_nontso_linearize(nbuf) != QDF_STATUS_SUCCESS) {
		dp_debug_rl("nbuf %pK linearize failed. drop the pkt", nbuf);
		goto drop_pkt_and_release_skb;
	}

	if (dp_intf->txrx_ops.tx.tx(soc, dp_link->link_id, nbuf)) {
		dp_debug("Failed to send packet to txrx for sta: "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(dest_mac_addr->bytes));
		goto drop_pkt_and_release_skb;
	}

	return QDF_STATUS_SUCCESS;

drop_pkt_and_release_skb:
	qdf_net_buf_debug_release_skb(nbuf);
drop_pkt:
	qdf_dp_trace_data_pkt(nbuf, QDF_TRACE_DEFAULT_PDEV_ID,
			      QDF_DP_TRACE_DROP_PACKET_RECORD, 0,
			      QDF_TX);
	qdf_nbuf_kfree(nbuf);
drop_pkt_accounting:
	qdf_net_stats_inc_tx_dropped(&dp_intf->stats);

	return QDF_STATUS_E_FAILURE;
}

void dp_softap_tx_timeout(struct wlan_dp_intf *dp_intf)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	cdp_dump_flow_pool_info(cds_get_context(QDF_MODULE_ID_SOC));

	++dp_intf->dp_stats.tx_rx_stats.tx_timeout_cnt;
	++dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt;

	if (dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt >
	    DP_TX_STALL_THRESHOLD) {
		dp_err("Detected data stall due to continuous TX timeouts");
		dp_intf->dp_stats.tx_rx_stats.cont_txtimeout_cnt = 0;

		if (dp_is_data_stall_event_enabled(DP_HOST_SAP_TX_TIMEOUT))
			cdp_post_data_stall_event(soc,
					  DATA_STALL_LOG_INDICATOR_HOST_DRIVER,
					  DATA_STALL_LOG_HOST_SOFTAP_TX_TIMEOUT,
					  OL_TXRX_PDEV_ID, 0xFF,
					  DATA_STALL_LOG_RECOVERY_TRIGGER_PDR);
	}
}

/**
 * dp_softap_notify_tx_compl_cbk() - callback to notify tx completion
 * @nbuf: pointer to n/w buffer
 * @context: pointer to DP interface
 * @flag: tx status flag
 *
 * Return: None
 */
void dp_softap_notify_tx_compl_cbk(qdf_nbuf_t nbuf,
				   void *context, uint16_t flag)
{
	struct wlan_dp_link *dp_link = context;
	struct wlan_dp_intf *dp_intf;

	if (!is_dp_link_valid(dp_link))
		return;

	dp_intf = dp_link->dp_intf;
	if (QDF_NBUF_CB_PACKET_TYPE_DHCP == QDF_NBUF_CB_GET_PACKET_TYPE(nbuf)) {
		dp_debug("sending DHCP indication");
		dp_softap_notify_dhcp_ind(context, nbuf);
	} else if (QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) ==
						QDF_NBUF_CB_PACKET_TYPE_EAPOL) {
		dp_softap_inspect_tx_eap_pkt(dp_intf, nbuf, true);
	}
}

#ifdef WLAN_FEATURE_TSF_PLUS_SOCK_TS
static inline
void dp_softap_tsf_timestamp_rx(struct wlan_dp_psoc_context *dp_ctx,
				qdf_nbuf_t netbuf)
{
	dp_ctx->dp_ops.dp_tsf_timestamp_rx(dp_ctx->dp_ops.callback_ctx,
					   netbuf);
}
#else
static inline
void dp_softap_tsf_timestamp_rx(struct wlan_dp_psoc_context *dp_ctx,
				qdf_nbuf_t netbuf)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
static inline bool dp_nbuf_dst_addr_is_mld_addr(struct wlan_dp_intf *dp_intf,
						qdf_nbuf_t nbuf)
{
	struct qdf_mac_addr *mld_addr;

	mld_addr = (struct qdf_mac_addr *)&dp_intf->mac_addr;

	if (!qdf_is_macaddr_zero(mld_addr) &&
	    !qdf_mem_cmp(mld_addr->bytes,
			 (qdf_nbuf_data(nbuf) +
			  QDF_NBUF_DEST_MAC_OFFSET),
			 QDF_MAC_ADDR_SIZE))
		return true;

	return false;
}
#else
static inline bool dp_nbuf_dst_addr_is_mld_addr(struct wlan_dp_intf *dp_intf,
						qdf_nbuf_t nbuf)
{
	return false;
}
#endif

QDF_STATUS dp_softap_rx_packet_cbk(void *link_ctx, qdf_nbuf_t rx_buf)
{
	struct wlan_dp_intf *dp_intf = NULL;
	struct wlan_dp_link *dp_link = NULL;
	QDF_STATUS qdf_status;
	unsigned int cpu_index;
	qdf_nbuf_t nbuf = NULL;
	qdf_nbuf_t next = NULL;
	struct wlan_dp_psoc_context *dp_ctx = NULL;
	bool is_eapol = false;
	struct dp_tx_rx_stats *stats;

	/* Sanity check on inputs */
	if (unlikely(!link_ctx || !rx_buf)) {
		dp_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	dp_link = (struct wlan_dp_link *)link_ctx;
	dp_intf = dp_link->dp_intf;
	dp_ctx = dp_intf->dp_ctx;

	stats = &dp_intf->dp_stats.tx_rx_stats;
	/* walk the chain until all are processed */
	next = rx_buf;

	while (next) {
		nbuf = next;
		next = qdf_nbuf_next(nbuf);
		qdf_nbuf_set_next(nbuf, NULL);

		dp_softap_dump_nbuf(nbuf);

		qdf_nbuf_set_dev(nbuf, dp_intf->dev);

		cpu_index = qdf_get_cpu();
		++stats->per_cpu[cpu_index].rx_packets;
		qdf_net_stats_add_rx_pkts(&dp_intf->stats, 1);
		/* count aggregated RX frame into stats */
		qdf_net_stats_add_rx_pkts(&dp_intf->stats,
					  qdf_nbuf_get_gso_segs(nbuf));
		qdf_net_stats_add_rx_bytes(&dp_intf->stats,
					   qdf_nbuf_len(nbuf));

		dp_softap_inspect_dhcp_packet(dp_link, nbuf, QDF_RX);

		if (qdf_nbuf_is_ipv4_eapol_pkt(nbuf))
			is_eapol = true;

		if (qdf_unlikely(is_eapol &&
		    !(!qdf_mem_cmp(dp_link->mac_addr.bytes,
				   qdf_nbuf_data(nbuf) +
				   QDF_NBUF_DEST_MAC_OFFSET,
				   QDF_MAC_ADDR_SIZE) ||
		    dp_nbuf_dst_addr_is_mld_addr(dp_intf, nbuf)))) {
			qdf_nbuf_free(nbuf);
			continue;
		}

		wlan_dp_pkt_add_timestamp(dp_intf,
					  QDF_PKT_RX_DRIVER_EXIT, nbuf);

		dp_event_eapol_log(nbuf, QDF_RX);
		qdf_dp_trace_log_pkt(dp_link->link_id,
				     nbuf, QDF_RX, QDF_TRACE_DEFAULT_PDEV_ID,
				     dp_intf->device_mode);
		DPTRACE(qdf_dp_trace(nbuf,
				     QDF_DP_TRACE_RX_PACKET_PTR_RECORD,
				     QDF_TRACE_DEFAULT_PDEV_ID,
				     qdf_nbuf_data_addr(nbuf),
				     sizeof(qdf_nbuf_data(nbuf)), QDF_RX));
		DPTRACE(qdf_dp_trace_data_pkt(nbuf, QDF_TRACE_DEFAULT_PDEV_ID,
					      QDF_DP_TRACE_RX_PACKET_RECORD,
					      0, QDF_RX));

		if (dp_rx_pkt_tracepoints_enabled())
			qdf_trace_dp_packet(nbuf, QDF_RX, NULL, 0);

		qdf_nbuf_set_protocol_eth_tye_trans(nbuf);

		/* hold configurable wakelock for unicast traffic */
		if (!dp_is_current_high_throughput(dp_ctx) &&
		    dp_ctx->dp_cfg.rx_wakelock_timeout &&
		    !qdf_nbuf_pkt_type_is_mcast(nbuf) &&
		    !qdf_nbuf_pkt_type_is_bcast(nbuf)) {
			cds_host_diag_log_work(&dp_ctx->rx_wake_lock,
					dp_ctx->dp_cfg.rx_wakelock_timeout,
					WIFI_POWER_EVENT_WAKELOCK_HOLD_RX);
			qdf_wake_lock_timeout_acquire(&dp_ctx->rx_wake_lock,
					dp_ctx->dp_cfg.rx_wakelock_timeout);
		}

		/* Remove SKB from internal tracking table before submitting
		 * it to stack
		 */
		qdf_net_buf_debug_release_skb(nbuf);

		dp_softap_tsf_timestamp_rx(dp_ctx, nbuf);

		if (is_eapol && dp_ctx->dp_ops.dp_send_rx_pkt_over_nl) {
			if (dp_ctx->dp_ops.dp_send_rx_pkt_over_nl(dp_intf->dev,
					(u8 *)&dp_link->conn_info.peer_macaddr,
								  nbuf, false))
				qdf_status = QDF_STATUS_SUCCESS;
			else
				qdf_status = QDF_STATUS_E_INVAL;
			qdf_nbuf_dev_kfree(nbuf);
		} else {
			qdf_status = wlan_dp_rx_deliver_to_stack(dp_intf, nbuf);
		}

		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			++stats->per_cpu[cpu_index].rx_delivered;
		else
			++stats->per_cpu[cpu_index].rx_refused;
	}

	return QDF_STATUS_SUCCESS;
}

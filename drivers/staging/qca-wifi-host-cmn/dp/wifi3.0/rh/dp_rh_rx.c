/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "cdp_txrx_cmn_struct.h"
#include "hal_hw_headers.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "dp_rx_defrag.h"
#include "dp_rh_rx.h"
#include "dp_rh_htt.h"
#include "dp_peer.h"
#include "hal_rx.h"
#include "hal_rh_rx.h"
#include "hal_api.h"
#include "hal_rh_api.h"
#include "qdf_nbuf.h"
#include "dp_internal.h"
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#include "dp_hist.h"
#include "dp_rx_buffer_pool.h"
#include "dp_rh.h"

static inline uint8_t dp_rx_get_ctx_id_frm_napiid(uint8_t napi_id)
{
	/*
	 * This is NAPI to CE then to rx context id mapping
	 * example: CE1 is assigned with napi id 3(ce_id+1)
	 * CE1 maps to RX context id 0, so napi id 2 maps to
	 * RX context id 0, this need to optimized further.
	 */
	switch (napi_id) {
	case 2:
		return 0;
	case 11:
		return 1;
	case 12:
		return 2;
	default:
		dp_err("Invalid napi id: %u, this should not happen", napi_id);
		qdf_assert_always(0);
		break;
	}
	return 0;
}

void
dp_rx_data_flush(void *data)
{
	struct qca_napi_info *napi_info = (struct qca_napi_info *)data;
	uint8_t rx_ctx_id = dp_rx_get_ctx_id_frm_napiid(napi_info->id);
	struct dp_soc *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct dp_vdev *vdev;
	int i;

	if (rx_ctx_id == 0 && soc->rx.flags.defrag_timeout_check) {
		uint32_t now_ms =
			qdf_system_ticks_to_msecs(qdf_system_ticks());

		if (now_ms >= soc->rx.defrag.next_flush_ms)
			dp_rx_defrag_waitlist_flush(soc);
	}

	/*Get first available vdev to flush all RX packets across soc*/
	for (i = 0; i < MAX_VDEV_CNT; i++) {
		vdev = dp_vdev_get_ref_by_id(soc, i, DP_MOD_ID_RX);
		if (vdev && vdev->osif_fisa_flush)
			vdev->osif_fisa_flush(soc, rx_ctx_id);

		if (vdev && vdev->osif_gro_flush) {
			vdev->osif_gro_flush(vdev->osif_vdev,
					     rx_ctx_id);
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
			return;
		}
		if (vdev)
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
	}
}

static inline
bool is_sa_da_idx_valid(uint32_t max_ast,
			qdf_nbuf_t nbuf, struct hal_rx_msdu_metadata msdu_info)
{
	if ((qdf_nbuf_is_sa_valid(nbuf) && (msdu_info.sa_idx > max_ast)) ||
	    (!qdf_nbuf_is_da_mcbc(nbuf) && qdf_nbuf_is_da_valid(nbuf) &&
	     (msdu_info.da_idx > max_ast)))
		return false;

	return true;
}

#if defined(FEATURE_MCL_REPEATER) && defined(FEATURE_MEC)
/**
 * dp_rx_mec_check_wrapper() - wrapper to dp_rx_mcast_echo_check
 * @soc: core DP main context
 * @txrx_peer: dp peer handler
 * @rx_tlv_hdr: start of the rx TLV header
 * @nbuf: pkt buffer
 *
 * Return: bool (true if it is a looped back pkt else false)
 */
static inline bool dp_rx_mec_check_wrapper(struct dp_soc *soc,
					   struct dp_txrx_peer *txrx_peer,
					   uint8_t *rx_tlv_hdr,
					   qdf_nbuf_t nbuf)
{
	return dp_rx_mcast_echo_check(soc, txrx_peer, rx_tlv_hdr, nbuf);
}
#else
static inline bool dp_rx_mec_check_wrapper(struct dp_soc *soc,
					   struct dp_txrx_peer *txrx_peer,
					   uint8_t *rx_tlv_hdr,
					   qdf_nbuf_t nbuf)
{
	return false;
}
#endif

static bool
dp_rx_intrabss_ucast_check_rh(struct dp_soc *soc, qdf_nbuf_t nbuf,
			      struct dp_txrx_peer *ta_txrx_peer,
			      struct hal_rx_msdu_metadata *msdu_metadata,
			      uint8_t *p_tx_vdev_id)
{
	uint16_t da_peer_id;
	struct dp_txrx_peer *da_peer;
	struct dp_ast_entry *ast_entry;
	dp_txrx_ref_handle txrx_ref_handle = NULL;

	if (!qdf_nbuf_is_da_valid(nbuf) || qdf_nbuf_is_da_mcbc(nbuf))
		return false;

	ast_entry = soc->ast_table[msdu_metadata->da_idx];
	if (!ast_entry)
		return false;

	if (ast_entry->type == CDP_TXRX_AST_TYPE_DA) {
		ast_entry->is_active = TRUE;
		return false;
	}

	da_peer_id = ast_entry->peer_id;
	/* TA peer cannot be same as peer(DA) on which AST is present
	 * this indicates a change in topology and that AST entries
	 * are yet to be updated.
	 */
	if (da_peer_id == ta_txrx_peer->peer_id ||
	    da_peer_id == HTT_INVALID_PEER)
		return false;

	da_peer = dp_txrx_peer_get_ref_by_id(soc, da_peer_id,
					     &txrx_ref_handle, DP_MOD_ID_RX);
	if (!da_peer)
		return false;

	*p_tx_vdev_id = da_peer->vdev->vdev_id;
	/* If the source or destination peer in the isolation
	 * list then dont forward instead push to bridge stack.
	 */
	if (dp_get_peer_isolation(ta_txrx_peer) ||
	    dp_get_peer_isolation(da_peer) ||
	    da_peer->vdev->vdev_id != ta_txrx_peer->vdev->vdev_id) {
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
		return false;
	}

	if (da_peer->bss_peer) {
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
		return false;
	}

	dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
	return true;
}

/*
 * dp_rx_intrabss_fwd_rh() - Implements the Intra-BSS forwarding logic
 *
 * @soc: core txrx main context
 * @ta_txrx_peer	: source peer entry
 * @rx_tlv_hdr	: start address of rx tlvs
 * @nbuf	: nbuf that has to be intrabss forwarded
 *
 * Return: bool: true if it is forwarded else false
 */
static bool
dp_rx_intrabss_fwd_rh(struct dp_soc *soc,
		      struct dp_txrx_peer *ta_txrx_peer,
		      uint8_t *rx_tlv_hdr,
		      qdf_nbuf_t nbuf,
		      struct hal_rx_msdu_metadata msdu_metadata,
		      struct cdp_tid_rx_stats *tid_stats)
{
	uint8_t tx_vdev_id;

	/* if it is a broadcast pkt (eg: ARP) and it is not its own
	 * source, then clone the pkt and send the cloned pkt for
	 * intra BSS forwarding and original pkt up the network stack
	 * Note: how do we handle multicast pkts. do we forward
	 * all multicast pkts as is or let a higher layer module
	 * like igmpsnoop decide whether to forward or not with
	 * Mcast enhancement.
	 */
	if (qdf_nbuf_is_da_mcbc(nbuf) && !ta_txrx_peer->bss_peer)
		return dp_rx_intrabss_mcbc_fwd(soc, ta_txrx_peer, rx_tlv_hdr,
					       nbuf, tid_stats, 0);

	if (dp_rx_intrabss_eapol_drop_check(soc, ta_txrx_peer, rx_tlv_hdr,
					    nbuf))
		return true;

	if (dp_rx_intrabss_ucast_check_rh(soc, nbuf, ta_txrx_peer,
					  &msdu_metadata, &tx_vdev_id))
		return dp_rx_intrabss_ucast_fwd(soc, ta_txrx_peer, tx_vdev_id,
						rx_tlv_hdr, nbuf, tid_stats,
						0);

	return false;
}

#ifdef RX_DESC_DEBUG_CHECK
static
QDF_STATUS dp_rx_desc_nbuf_sanity_check_rh(struct dp_soc *soc,
					   uint32_t *msg_word,
					   struct dp_rx_desc *rx_desc)
{
	uint64_t paddr;

	paddr = (HTT_RX_DATA_MSDU_INFO_BUFFER_ADDR_LOW_GET(*msg_word) |
		 ((uint64_t)(HTT_RX_DATA_MSDU_INFO_BUFFER_ADDR_HIGH_GET(*(msg_word + 1))) << 32));

	/* Sanity check for possible buffer paddr corruption */
	if (dp_rx_desc_paddr_sanity_check(rx_desc, paddr))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

#else
static inline
QDF_STATUS dp_rx_desc_nbuf_sanity_check_rh(struct dp_soc *soc,
					   uint32_t *msg_word,
					   struct dp_rx_desc *rx_desc)
#endif

#ifdef DUP_RX_DESC_WAR
static
void dp_rx_dump_info_and_assert_rh(struct dp_soc *soc,
				   uint32_t *msg_word,
				   struct dp_rx_desc *rx_desc)
{
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   msg_word, HTT_RX_DATA_MSDU_INFO_SIZE);
	dp_rx_desc_dump(rx_desc);
}
#else
static
void dp_rx_dump_info_and_assert_rh(struct dp_soc *soc,
				   uint32_t *msg_word,
				   struct dp_rx_desc *rx_desc)
{
	dp_rx_desc_dump(rx_desc);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   msg_word, HTT_RX_DATA_MSDU_INFO_SIZE);
	qdf_assert_always(0);
}
#endif

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
static void
dp_rx_ring_record_entry_rh(struct dp_soc *soc, uint8_t ring_num,
			   uint32_t *msg_word)
{
	struct dp_buf_info_record *record;
	uint32_t idx;

	if (qdf_unlikely(!soc->rx_ring_history[ring_num]))
		return;

	idx = dp_history_get_next_index(&soc->rx_ring_history[ring_num]->index,
					DP_RX_HIST_MAX);

	/* No NULL check needed for record since its an array */
	record = &soc->rx_ring_history[ring_num]->entry[idx];

	record->timestamp = qdf_get_log_timestamp();
	record->hbi.paddr =
		(HTT_RX_DATA_MSDU_INFO_BUFFER_ADDR_LOW_GET(*msg_word) |
		((uint64_t)(HTT_RX_DATA_MSDU_INFO_BUFFER_ADDR_HIGH_GET(*(msg_word + 1))) << 32));
	record->hbi.sw_cookie =
		HTT_RX_DATA_MSDU_INFO_SW_BUFFER_COOKIE_GET(*(msg_word + 1));
}
#else
static inline void
dp_rx_ring_record_entry_rh(struct dp_soc *soc, uint8_t rx_ring_num,
			   uint32_t *msg_word) {}
#endif

#ifdef WLAN_FEATURE_MARK_FIRST_WAKEUP_PACKET
static inline void
dp_rx_mark_first_packet_after_wow_wakeup_rh(struct dp_soc *soc,
					    uint32_t *msg_word,
					    qdf_nbuf_t nbuf)
{
	struct dp_pdev *pdev = soc->pdev_list[0];

	if (!pdev->is_first_wakeup_packet)
		return;

	if (HTT_RX_DATA_MSDU_INFO_IS_FIRST_PKT_AFTER_WKP_GET(*(msg_word + 2))) {
		qdf_nbuf_mark_wakeup_frame(nbuf);
		dp_info("First packet after WOW Wakeup rcvd");
	}
}
#else
static inline void
dp_rx_mark_first_packet_after_wow_wakeup_rh(struct dp_soc *soc,
					    uint32_t *msg_word,
					    qdf_nbuf_t nbuf) {}
#endif

#ifdef QCA_SUPPORT_EAPOL_OVER_CONTROL_PORT
static void
dp_rx_deliver_to_osif_stack_rh(struct dp_soc *soc,
			       struct dp_vdev *vdev,
			       struct dp_txrx_peer *txrx_peer,
			       qdf_nbuf_t nbuf,
			       qdf_nbuf_t tail,
			       bool is_eapol)
{
	if (is_eapol && soc->eapol_over_control_port)
		dp_rx_eapol_deliver_to_stack(soc, vdev, txrx_peer, nbuf, NULL);
	else
		dp_rx_deliver_to_stack(soc, vdev, txrx_peer, nbuf, NULL);
}
#else
static void
dp_rx_deliver_to_osif_stack_rh(struct dp_soc *soc,
			       struct dp_vdev *vdev,
			       struct dp_txrx_peer *txrx_peer,
			       qdf_nbuf_t nbuf,
			       qdf_nbuf_t tail,
			       bool is_eapol)
{
	dp_rx_deliver_to_stack(soc, vdev, txrx_peer, nbuf, NULL);
}
#endif

static void
dp_rx_decrypt_unecrypt_err_handler_rh(struct dp_soc *soc, qdf_nbuf_t nbuf,
				      uint8_t error_code, uint8_t mac_id)
{
	uint32_t pkt_len, l2_hdr_offset;
	uint16_t msdu_len;
	struct dp_vdev *vdev;
	struct dp_txrx_peer *txrx_peer = NULL;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	qdf_ether_header_t *eh;
	bool is_broadcast;
	uint8_t *rx_tlv_hdr;
	uint16_t peer_id;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	rx_tlv_hdr = qdf_nbuf_data(nbuf);

	/*
	 * Check if DMA completed -- msdu_done is the last bit
	 * to be written
	 */
	if (!hal_rx_attn_msdu_done_get(soc->hal_soc, rx_tlv_hdr)) {
		dp_err_rl("MSDU DONE failure");

		hal_rx_dump_pkt_tlvs(soc->hal_soc, rx_tlv_hdr,
				     QDF_TRACE_LEVEL_INFO);
		qdf_assert(0);
	}

	if (qdf_unlikely(qdf_nbuf_is_rx_chfrag_cont(nbuf))) {
		dp_err("Unsupported MSDU format rcvd for error:%u", error_code);
		qdf_assert_always(0);
		goto free_nbuf;
	}

	peer_id =  QDF_NBUF_CB_RX_PEER_ID(nbuf);
	txrx_peer = dp_tgt_txrx_peer_get_ref_by_id(soc, peer_id,
						   &txrx_ref_handle,
						   DP_MOD_ID_RX);
	if (!txrx_peer) {
		QDF_TRACE_ERROR_RL(QDF_MODULE_ID_DP, "txrx_peer is NULL");
		DP_STATS_INC_PKT(soc, rx.err.rx_invalid_peer, 1,
				 qdf_nbuf_len(nbuf));
		/* Trigger invalid peer handler wrapper */
		dp_rx_process_invalid_peer_wrapper(soc, nbuf, true, mac_id);
		return;
	}

	l2_hdr_offset = hal_rx_msdu_end_l3_hdr_padding_get(soc->hal_soc,
							   rx_tlv_hdr);
	msdu_len = hal_rx_msdu_start_msdu_len_get(soc->hal_soc, rx_tlv_hdr);
	pkt_len = msdu_len + l2_hdr_offset + soc->rx_pkt_tlv_size;

	if (qdf_unlikely(pkt_len > buf_size)) {
		DP_STATS_INC_PKT(soc, rx.err.rx_invalid_pkt_len,
				 1, pkt_len);
		goto free_nbuf;
	}

	/* Set length in nbuf */
	qdf_nbuf_set_pktlen(nbuf, pkt_len);

	qdf_nbuf_set_next(nbuf, NULL);

	qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
	qdf_nbuf_set_rx_chfrag_end(nbuf, 1);

	vdev = txrx_peer->vdev;
	if (!vdev) {
		dp_rx_info_rl("%pK: INVALID vdev %pK OR osif_rx", soc,
			      vdev);
		DP_STATS_INC(soc, rx.err.invalid_vdev, 1);
		goto free_nbuf;
	}

	/*
	 * Advance the packet start pointer by total size of
	 * pre-header TLV's
	 */
	dp_rx_skip_tlvs(soc, nbuf, l2_hdr_offset);

	/*
	 * WAPI cert AP sends rekey frames as unencrypted.
	 * Thus RXDMA will report unencrypted frame error.
	 * To pass WAPI cert case, SW needs to pass unencrypted
	 * rekey frame to stack.
	 *
	 * In dynamic WEP case rekey frames are not encrypted
	 * similar to WAPI. Allow EAPOL when 8021+wep is enabled and
	 * key install is already done
	 */
	if ((qdf_nbuf_is_ipv4_wapi_pkt(nbuf)) ||
	    ((vdev->sec_type == cdp_sec_type_wep104) &&
	     (qdf_nbuf_is_ipv4_eapol_pkt(nbuf)))) {
		if (qdf_unlikely(hal_rx_msdu_end_da_is_mcbc_get(soc->hal_soc,
								rx_tlv_hdr) &&
				 (vdev->rx_decap_type ==
				  htt_cmn_pkt_type_ethernet))) {
			eh = (qdf_ether_header_t *)qdf_nbuf_data(nbuf);
			is_broadcast = (QDF_IS_ADDR_BROADCAST
					(eh->ether_dhost)) ? 1 : 0;
			DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.multicast,
						      1, qdf_nbuf_len(nbuf), 0);
			if (is_broadcast) {
				DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.bcast,
							      1, qdf_nbuf_len(nbuf), 0);
			}
		} else {
			DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.unicast, 1,
						      qdf_nbuf_len(nbuf),
						      0);
		}

		if (qdf_unlikely(vdev->rx_decap_type == htt_cmn_pkt_type_raw)) {
			dp_rx_deliver_raw(vdev, nbuf, txrx_peer, 0);
		} else {
			/* Update the protocol tag in SKB based on CCE metadata */
			dp_rx_update_protocol_tag(soc, vdev, nbuf, rx_tlv_hdr,
						  EXCEPTION_DEST_RING_ID, true, true);
			/* Update the flow tag in SKB based on FSE metadata */
			dp_rx_update_flow_tag(soc, vdev, nbuf, rx_tlv_hdr, true);
			DP_PEER_STATS_FLAT_INC(txrx_peer, to_stack.num, 1);
			qdf_nbuf_set_exc_frame(nbuf, 1);
			dp_rx_deliver_to_osif_stack_rh(soc, vdev, txrx_peer, nbuf, NULL,
						       qdf_nbuf_is_ipv4_eapol_pkt(nbuf));
		}
	}

	if (txrx_peer)
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
	return;

free_nbuf:
	if (txrx_peer)
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
	dp_rx_nbuf_free(nbuf);
}

static void
dp_rx_2k_jump_oor_err_handler_rh(struct dp_soc *soc, qdf_nbuf_t nbuf,
				 uint32_t error_code)
{
	uint32_t frame_mask;
	struct dp_txrx_peer *txrx_peer = NULL;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	uint8_t *rx_tlv_hdr;
	uint16_t peer_id;

	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	if (qdf_unlikely(qdf_nbuf_is_rx_chfrag_cont(nbuf))) {
		dp_err("Unsupported MSDU format rcvd for error:%u", error_code);
		qdf_assert_always(0);
		goto free_nbuf;
	}

	peer_id =  QDF_NBUF_CB_RX_PEER_ID(nbuf);
	txrx_peer = dp_tgt_txrx_peer_get_ref_by_id(soc, peer_id,
						   &txrx_ref_handle,
						   DP_MOD_ID_RX);
	if (!txrx_peer) {
		dp_info_rl("peer not found");
		goto free_nbuf;
	}

	if (error_code == HTT_RXDATA_ERR_OOR) {
		frame_mask = FRAME_MASK_IPV4_ARP | FRAME_MASK_IPV4_DHCP |
			FRAME_MASK_IPV4_EAPOL | FRAME_MASK_IPV6_DHCP;
	} else {
		frame_mask = FRAME_MASK_IPV4_ARP;
	}

	if (dp_rx_deliver_special_frame(soc, txrx_peer, nbuf, frame_mask,
					rx_tlv_hdr)) {
		if (error_code == HTT_RXDATA_ERR_OOR) {
			DP_STATS_INC(soc, rx.err.reo_err_oor_to_stack, 1);
		} else {
			DP_STATS_INC(soc, rx.err.rx_2k_jump_to_stack, 1);
		}

		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
		return;
	}

free_nbuf:
	if (txrx_peer)
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);

	if (error_code == HTT_RXDATA_ERR_OOR) {
		DP_STATS_INC(soc, rx.err.reo_err_oor_drop, 1);
	} else {
		DP_STATS_INC(soc, rx.err.rx_2k_jump_drop, 1);
	}

	dp_rx_nbuf_free(nbuf);
}

static void dp_rx_mic_err_handler_rh(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	struct dp_vdev *vdev;
	struct dp_pdev *pdev;
	struct dp_txrx_peer *txrx_peer = NULL;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	struct ol_if_ops *tops;
	uint16_t rx_seq, fragno;
	uint8_t is_raw;
	uint16_t peer_id;
	unsigned int tid;
	QDF_STATUS status;
	struct cdp_rx_mic_err_info mic_failure_info;

	/*
	 * only first msdu, mpdu start description tlv valid?
	 * and use it for following msdu.
	 */
	if (!hal_rx_msdu_end_first_msdu_get(soc->hal_soc,
					    qdf_nbuf_data(nbuf)))
		return;

	peer_id =  QDF_NBUF_CB_RX_PEER_ID(nbuf);
	txrx_peer = dp_tgt_txrx_peer_get_ref_by_id(soc, peer_id,
						   &txrx_ref_handle,
						   DP_MOD_ID_RX);
	if (!txrx_peer) {
		dp_info_rl("txrx_peer not found");
		goto fail;
	}

	vdev = txrx_peer->vdev;
	if (!vdev) {
		dp_info_rl("VDEV not found");
		goto fail;
	}

	pdev = vdev->pdev;
	if (!pdev) {
		dp_info_rl("PDEV not found");
		goto fail;
	}

	/*TODO is raw support required for evros check*/
	is_raw = HAL_IS_DECAP_FORMAT_RAW(soc->hal_soc, qdf_nbuf_data(nbuf));
	if (is_raw) {
		fragno = dp_rx_frag_get_mpdu_frag_number(soc,
							 qdf_nbuf_data(nbuf));
		/* Can get only last fragment */
		if (fragno) {
			tid = hal_rx_mpdu_start_tid_get(soc->hal_soc,
							qdf_nbuf_data(nbuf));
			rx_seq = hal_rx_get_rx_sequence(soc->hal_soc,
							qdf_nbuf_data(nbuf));

			status = dp_rx_defrag_add_last_frag(soc, txrx_peer,
							    tid, rx_seq, nbuf);
			dp_info_rl("Frag pkt seq# %d frag# %d consumed " "status %d !",
				   rx_seq, fragno, status);
			if (txrx_peer)
				dp_txrx_peer_unref_delete(txrx_ref_handle,
							  DP_MOD_ID_RX);
			return;
		}
	}

	if (qdf_unlikely(qdf_nbuf_is_rx_chfrag_cont(nbuf))) {
		dp_err("Unsupported MSDU format rcvd in MIC error handler");
		qdf_assert_always(0);
		goto fail;
	}

	if (hal_rx_mpdu_get_addr1(soc->hal_soc, qdf_nbuf_data(nbuf),
				  &mic_failure_info.da_mac_addr.bytes[0])) {
		dp_err_rl("Failed to get da_mac_addr");
		goto fail;
	}

	if (hal_rx_mpdu_get_addr2(soc->hal_soc, qdf_nbuf_data(nbuf),
				  &mic_failure_info.ta_mac_addr.bytes[0])) {
		dp_err_rl("Failed to get ta_mac_addr");
		goto fail;
	}

	mic_failure_info.key_id = 0;
	mic_failure_info.multicast =
		IEEE80211_IS_MULTICAST(mic_failure_info.da_mac_addr.bytes);
	qdf_mem_zero(mic_failure_info.tsc, MIC_SEQ_CTR_SIZE);
	mic_failure_info.frame_type = cdp_rx_frame_type_802_11;
	mic_failure_info.data = NULL;
	mic_failure_info.vdev_id = vdev->vdev_id;

	tops = pdev->soc->cdp_soc.ol_ops;
	if (tops->rx_mic_error)
		tops->rx_mic_error(soc->ctrl_psoc, pdev->pdev_id,
				   &mic_failure_info);

fail:
	dp_rx_nbuf_free(nbuf);
	if (txrx_peer)
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
}

static QDF_STATUS dp_rx_err_handler_rh(struct dp_soc *soc,
				       struct dp_rx_desc *rx_desc,
				       uint32_t error_code)
{
	switch (error_code) {
	case HTT_RXDATA_ERR_MSDU_LIMIT:
	case HTT_RXDATA_ERR_FLUSH_REQUEST:
	case HTT_RXDATA_ERR_ZERO_LEN_MSDU:
		dp_rx_nbuf_free(rx_desc->nbuf);
		dp_err_rl("MSDU rcvd with error code: %u", error_code);
		break;
	case HTT_RXDATA_ERR_TKIP_MIC:
		dp_rx_mic_err_handler_rh(soc, rx_desc->nbuf);
		break;
	case HTT_RXDATA_ERR_OOR:
	case HTT_RXDATA_ERR_2K_JUMP:
		dp_rx_2k_jump_oor_err_handler_rh(soc, rx_desc->nbuf,
						 error_code);
		break;
	case HTT_RXDATA_ERR_DECRYPT:
	case HTT_RXDATA_ERR_UNENCRYPTED:
		dp_rx_decrypt_unecrypt_err_handler_rh(soc, rx_desc->nbuf,
						      error_code,
						      rx_desc->pool_id);
		break;
	default:
		dp_err("Invalid error packet rcvd, code: %u", error_code);
		dp_rx_desc_dump(rx_desc);
		qdf_assert_always(0);
		dp_rx_nbuf_free(rx_desc->nbuf);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

void
dp_rx_data_indication_handler(struct dp_soc *soc, qdf_nbuf_t data_ind,
			      uint16_t vdev_id, uint16_t peer_id,
			      uint16_t msdu_count)
{
	uint8_t *data_ind_msg;
	uint32_t *msg_word;
	uint32_t rx_ctx_id;
	hal_soc_handle_t hal_soc;
	struct dp_rx_desc *rx_desc = NULL;
	qdf_nbuf_t nbuf, next;
	union dp_rx_desc_list_elem_t *head[MAX_PDEV_CNT];
	union dp_rx_desc_list_elem_t *tail[MAX_PDEV_CNT];
	uint32_t num_pending = msdu_count;
	uint32_t rx_buf_cookie;
	uint16_t msdu_len = 0;
	struct dp_txrx_peer *txrx_peer;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	struct dp_vdev *vdev;
	uint32_t pkt_len = 0;
	uint8_t *rx_tlv_hdr;
	uint32_t rx_bufs_reaped[MAX_PDEV_CNT];
	uint8_t mac_id = 0;
	struct dp_pdev *rx_pdev;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	struct cdp_tid_rx_stats *tid_stats;
	qdf_nbuf_t nbuf_head;
	qdf_nbuf_t nbuf_tail;
	qdf_nbuf_t deliver_list_head;
	qdf_nbuf_t deliver_list_tail;
	uint32_t num_rx_bufs_reaped = 0;
	struct hif_opaque_softc *scn;
	int32_t tid = 0;
	bool is_prev_msdu_last = true;
	uint32_t rx_ol_pkt_cnt = 0;
	struct hal_rx_msdu_metadata msdu_metadata;
	qdf_nbuf_t ebuf_head;
	qdf_nbuf_t ebuf_tail;
	uint8_t pkt_capture_offload = 0;
	uint32_t old_tid;
	uint32_t peer_ext_stats;
	uint32_t dsf;
	uint32_t max_ast;
	uint64_t current_time = 0;
	uint32_t error;
	uint32_t error_code;
	QDF_STATUS status;
	uint16_t buf_size;

	DP_HIST_INIT();

	qdf_assert_always(soc && msdu_count);
	hal_soc = soc->hal_soc;
	qdf_assert_always(hal_soc);

	scn = soc->hif_handle;
	dp_runtime_pm_mark_last_busy(soc);
	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	/* reset local variables here to be re-used in the function */
	nbuf_head = NULL;
	nbuf_tail = NULL;
	deliver_list_head = NULL;
	deliver_list_tail = NULL;
	txrx_peer = NULL;
	vdev = NULL;
	num_rx_bufs_reaped = 0;
	ebuf_head = NULL;
	ebuf_tail = NULL;

	qdf_mem_zero(rx_bufs_reaped, sizeof(rx_bufs_reaped));
	qdf_mem_zero(head, sizeof(head));
	qdf_mem_zero(tail, sizeof(tail));
	old_tid = 0xff;
	dsf = 0;
	peer_ext_stats = 0;
	max_ast = 0;
	rx_pdev = NULL;
	tid_stats = NULL;

	dp_pkt_get_timestamp(&current_time);

	peer_ext_stats = wlan_cfg_is_peer_ext_stats_enabled(soc->wlan_cfg_ctx);
	max_ast = wlan_cfg_get_max_ast_idx(soc->wlan_cfg_ctx);

	data_ind_msg = qdf_nbuf_data(data_ind);
	msg_word =
		(uint32_t *)(data_ind_msg + HTT_RX_DATA_IND_HDR_SIZE);
	rx_ctx_id =
		dp_rx_get_ctx_id_frm_napiid(QDF_NBUF_CB_RX_CTX_ID(data_ind));

	while (qdf_likely(num_pending)) {
		dp_rx_ring_record_entry_rh(soc, rx_ctx_id, msg_word);
		rx_buf_cookie =
			HTT_RX_DATA_MSDU_INFO_SW_BUFFER_COOKIE_GET(*(msg_word + 1));
		rx_desc = dp_rx_cookie_2_va_rxdma_buf(soc, rx_buf_cookie);
		if (qdf_unlikely(!rx_desc && !rx_desc->nbuf &&
				 !rx_desc->in_use)) {
			dp_err("Invalid RX descriptor");
			qdf_assert_always(0);
			/* TODO handle this if its valid case */
		}

		status = dp_rx_desc_nbuf_sanity_check_rh(soc, msg_word,
							 rx_desc);
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(status))) {
			DP_STATS_INC(soc, rx.err.nbuf_sanity_fail, 1);
			dp_info_rl("Nbuf sanity check failure!");
			dp_rx_dump_info_and_assert_rh(soc, msg_word, rx_desc);
			rx_desc->in_err_state = 1;
			continue;
		}

		if (qdf_unlikely(!dp_rx_desc_check_magic(rx_desc))) {
			dp_err("Invalid rx_desc cookie=%d", rx_buf_cookie);
			DP_STATS_INC(soc, rx.err.rx_desc_invalid_magic, 1);
			dp_rx_dump_info_and_assert_rh(soc, msg_word, rx_desc);
			continue;
		}

		msdu_len =
			HTT_RX_DATA_MSDU_INFO_MSDU_LENGTH_GET(*(msg_word + 2));

		if (qdf_unlikely(
		HTT_RX_DATA_MSDU_INFO_MSDU_CONTINUATION_GET(*(msg_word + 2)))) {
			/* previous msdu has end bit set, so current one is
			 * the new MPDU
			 */
			if (is_prev_msdu_last) {
				/* For new MPDU check if we can read complete
				 * MPDU by comparing the number of buffers
				 * available and number of buffers needed to
				 * reap this MPDU
				 */
				if ((msdu_len /
				     (buf_size -
				      soc->rx_pkt_tlv_size) + 1) >
				    num_pending) {
					DP_STATS_INC(soc,
						     rx.msdu_scatter_wait_break,
						     1);
					/* This is not expected host cannot deal
					 * with partial frame in single DATA
					 * indication, F.W has to submit full
					 * frame in single DATA indication
					 */
					qdf_assert_always(0);
				}
				is_prev_msdu_last = false;
			}
		}

		if (HTT_RX_DATA_MSDU_INFO_MPDU_RETRY_BIT_GET(*(msg_word + 2)))
			qdf_nbuf_set_rx_retry_flag(rx_desc->nbuf, 1);

		if (HTT_RX_DATA_MSDU_INFO_RAW_MPDU_FRAME_GET(*(msg_word + 2)))
			qdf_nbuf_set_raw_frame(rx_desc->nbuf, 1);

		/*
		 * end MSDU has continuation bit set to zero using this to detect
		 * full MSDU
		 */
		if (!is_prev_msdu_last &&
		    !HTT_RX_DATA_MSDU_INFO_MSDU_CONTINUATION_GET(*(msg_word + 2)))
			is_prev_msdu_last = true;

		rx_bufs_reaped[rx_desc->pool_id]++;
		QDF_NBUF_CB_RX_PEER_ID(rx_desc->nbuf) = peer_id;
		QDF_NBUF_CB_RX_VDEV_ID(rx_desc->nbuf) = vdev_id;
		dp_rx_mark_first_packet_after_wow_wakeup_rh(soc, msg_word,
							    rx_desc->nbuf);

		/*
		 * save msdu flags first, last and continuation msdu in
		 * nbuf->cb, also save mcbc, is_da_valid, is_sa_valid and
		 * length to nbuf->cb. This ensures the info required for
		 * per pkt processing is always in the same cache line.
		 * This helps in improving throughput for smaller pkt
		 * sizes.
		 */
		if (HTT_RX_DATA_MSDU_INFO_FIRST_MSDU_IN_MPDU_GET(*(msg_word + 2)))
			qdf_nbuf_set_rx_chfrag_start(rx_desc->nbuf, 1);

		if (HTT_RX_DATA_MSDU_INFO_MSDU_CONTINUATION_GET(*(msg_word + 2)))
			qdf_nbuf_set_rx_chfrag_cont(rx_desc->nbuf, 1);

		if (HTT_RX_DATA_MSDU_INFO_LAST_MSDU_IN_MPDU_GET(*(msg_word + 2)))
			qdf_nbuf_set_rx_chfrag_end(rx_desc->nbuf, 1);

		if (HTT_RX_DATA_MSDU_INFO_DA_IS_MCBC_GET(*(msg_word + 2)))
			qdf_nbuf_set_da_mcbc(rx_desc->nbuf, 1);

		if (HTT_RX_DATA_MSDU_INFO_DA_IS_VALID_GET(*(msg_word + 2)))
			qdf_nbuf_set_da_valid(rx_desc->nbuf, 1);

		if (HTT_RX_DATA_MSDU_INFO_SA_IS_VALID_GET(*(msg_word + 2)))
			qdf_nbuf_set_sa_valid(rx_desc->nbuf, 1);

		qdf_nbuf_set_tid_val(rx_desc->nbuf,
			HTT_RX_DATA_MSDU_INFO_TID_INFO_GET(*(msg_word + 2)));

		/* set whether packet took offloads path */
		 qdf_nbuf_set_rx_reo_dest_ind_or_sw_excpt(
		 rx_desc->nbuf,
		 HTT_RX_DATA_MSDU_INFO_FW_OFFLOADS_INSPECTED_GET(*(msg_word + 1)));

		QDF_NBUF_CB_RX_PKT_LEN(rx_desc->nbuf) = msdu_len;

		QDF_NBUF_CB_RX_CTX_ID(rx_desc->nbuf) = rx_ctx_id;

		/*
		 * TODO  move unmap after scattered msdu waiting break logic
		 * in case double skb unmap happened.
		 */
		dp_rx_nbuf_unmap(soc, rx_desc, rx_ctx_id);

		error = HTT_RX_DATA_MSDU_INFO_ERROR_VALID_GET(*(msg_word + 3));
		if (qdf_unlikely(error)) {
			dp_rx_err("MSDU RX error encountered error:%u", error);
			error_code =
			HTT_RX_DATA_MSDU_INFO_ERROR_INFO_GET(*(msg_word + 3));
			dp_rx_err_handler_rh(soc, rx_desc, error_code);

		} else {
			DP_RX_PROCESS_NBUF(soc, nbuf_head, nbuf_tail, ebuf_head,
					   ebuf_tail, rx_desc);
		}

		num_pending -= 1;

		dp_rx_add_to_free_desc_list(&head[rx_desc->pool_id],
					    &tail[rx_desc->pool_id], rx_desc);
		num_rx_bufs_reaped++;

		msg_word += HTT_RX_DATA_MSDU_INFO_SIZE >> 2;
	}

	dp_rx_per_core_stats_update(soc, rx_ctx_id, num_rx_bufs_reaped);

	for (mac_id = 0; mac_id < MAX_PDEV_CNT; mac_id++) {
		/*
		 * continue with next mac_id if no pkts were reaped
		 * from that pool
		 */
		if (!rx_bufs_reaped[mac_id])
			continue;

		dp_rxdma_srng = &soc->rx_refill_buf_ring[mac_id];

		rx_desc_pool = &soc->rx_desc_buf[mac_id];

		dp_rx_buffers_replenish_simple(soc, mac_id, dp_rxdma_srng,
					       rx_desc_pool,
					       rx_bufs_reaped[mac_id],
					       &head[mac_id], &tail[mac_id]);
	}

	dp_verbose_debug("replenished %u", rx_bufs_reaped[0]);
	/* Peer can be NULL is case of LFR */
	if (qdf_likely(txrx_peer))
		vdev = NULL;

	/*
	 * BIG loop where each nbuf is dequeued from global queue,
	 * processed and queued back on a per vdev basis. These nbufs
	 * are sent to stack as and when we run out of nbufs
	 * or a new nbuf dequeued from global queue has a different
	 * vdev when compared to previous nbuf.
	 */
	nbuf = nbuf_head;
	while (nbuf) {
		next = nbuf->next;

		if (qdf_unlikely(dp_rx_is_raw_frame_dropped(nbuf))) {
			nbuf = next;
			DP_STATS_INC(soc, rx.err.raw_frm_drop, 1);
			continue;
		}

		rx_tlv_hdr = qdf_nbuf_data(nbuf);
		vdev_id = QDF_NBUF_CB_RX_VDEV_ID(nbuf);
		peer_id =  QDF_NBUF_CB_RX_PEER_ID(nbuf);

		/* Get TID from struct cb->tid_val, save to tid */
		if (qdf_nbuf_is_rx_chfrag_start(nbuf)) {
			tid = qdf_nbuf_get_tid_val(nbuf);
			if (tid >= CDP_MAX_DATA_TIDS) {
				DP_STATS_INC(soc, rx.err.rx_invalid_tid_err, 1);
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			}
		}

		if (qdf_unlikely(!txrx_peer)) {
			txrx_peer =
			dp_rx_get_txrx_peer_and_vdev(soc, nbuf, peer_id,
						     &txrx_ref_handle,
						     pkt_capture_offload,
						     &vdev,
						     &rx_pdev, &dsf,
						     &old_tid);
			if (qdf_unlikely(!txrx_peer) || qdf_unlikely(!vdev)) {
				nbuf = next;
				continue;
			}
		} else if (txrx_peer && txrx_peer->peer_id != peer_id) {
			dp_txrx_peer_unref_delete(txrx_ref_handle,
						  DP_MOD_ID_RX);

			txrx_peer =
			dp_rx_get_txrx_peer_and_vdev(soc, nbuf, peer_id,
						     &txrx_ref_handle,
						     pkt_capture_offload,
						     &vdev,
						     &rx_pdev, &dsf,
						     &old_tid);
			if (qdf_unlikely(!txrx_peer) || qdf_unlikely(!vdev)) {
				nbuf = next;
				continue;
			}
		}

		if (txrx_peer) {
			QDF_NBUF_CB_DP_TRACE_PRINT(nbuf) = false;
			qdf_dp_trace_set_track(nbuf, QDF_RX);
			QDF_NBUF_CB_RX_DP_TRACE(nbuf) = 1;
			QDF_NBUF_CB_RX_PACKET_TRACK(nbuf) =
				QDF_NBUF_RX_PKT_DATA_TRACK;
		}

		/* when hlos tid override is enabled, save tid in
		 * skb->priority
		 */
		if (qdf_unlikely(vdev->skip_sw_tid_classification &
					DP_TXRX_HLOS_TID_OVERRIDE_ENABLED))
			qdf_nbuf_set_priority(nbuf, tid);

		DP_RX_TID_SAVE(nbuf, tid);
		if (qdf_unlikely(dsf) || qdf_unlikely(peer_ext_stats) ||
		    dp_rx_pkt_tracepoints_enabled())
			qdf_nbuf_set_timestamp(nbuf);

		if (qdf_likely(old_tid != tid)) {
			tid_stats =
		&rx_pdev->stats.tid_stats.tid_rx_stats[rx_ctx_id][tid];
			old_tid = tid;
		}

		/*
		 * Check if DMA completed -- msdu_done is the last bit
		 * to be written
		 */
		if (qdf_likely(!qdf_nbuf_is_rx_chfrag_cont(nbuf))) {
			if (qdf_unlikely(!hal_rx_attn_msdu_done_get_rh(
								 rx_tlv_hdr))) {
				dp_err_rl("MSDU DONE failure");
				DP_STATS_INC(soc, rx.err.msdu_done_fail, 1);
				hal_rx_dump_pkt_tlvs(hal_soc, rx_tlv_hdr,
						     QDF_TRACE_LEVEL_INFO);
				tid_stats->fail_cnt[MSDU_DONE_FAILURE]++;
				qdf_assert(0);
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			} else if (qdf_unlikely(hal_rx_attn_msdu_len_err_get_rh(
								 rx_tlv_hdr))) {
				DP_STATS_INC(soc, rx.err.msdu_len_err, 1);
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			}
		}

		DP_HIST_PACKET_COUNT_INC(vdev->pdev->pdev_id);
		/*
		 * First IF condition:
		 * This condition is valid when 802.11 fragemented
		 * pkts reinjected back, even though this case is
		 * not valid for Rhine keeping it for sanity, verify
		 * and remove this first if condition based on test.
		 * Second IF condition:
		 * The below condition happens when an MSDU is spread
		 * across multiple buffers. This can happen in two cases
		 * 1. The nbuf size is smaller then the received msdu.
		 *    ex: we have set the nbuf size to 2048 during
		 *        nbuf_alloc. but we received an msdu which is
		 *        2304 bytes in size then this msdu is spread
		 *        across 2 nbufs.
		 *
		 * 2. AMSDUs when RAW mode is enabled.
		 *    ex: 1st MSDU is in 1st nbuf and 2nd MSDU is spread
		 *        across 1st nbuf and 2nd nbuf and last MSDU is
		 *        spread across 2nd nbuf and 3rd nbuf.
		 *
		 * for these scenarios let us create a skb frag_list and
		 * append these buffers till the last MSDU of the AMSDU
		 * Third condition:
		 * This is the most likely case, we receive 802.3 pkts
		 * decapsulated by HW, here we need to set the pkt length.
		 */
		hal_rx_msdu_metadata_get(hal_soc, rx_tlv_hdr, &msdu_metadata);
		if (qdf_unlikely(qdf_nbuf_is_frag(nbuf))) {
			bool is_mcbc, is_sa_vld, is_da_vld;

			is_mcbc = hal_rx_msdu_end_da_is_mcbc_get(soc->hal_soc,
								 rx_tlv_hdr);
			is_sa_vld =
				hal_rx_msdu_end_sa_is_valid_get(soc->hal_soc,
								rx_tlv_hdr);
			is_da_vld =
				hal_rx_msdu_end_da_is_valid_get(soc->hal_soc,
								rx_tlv_hdr);

			qdf_nbuf_set_da_mcbc(nbuf, is_mcbc);
			qdf_nbuf_set_da_valid(nbuf, is_da_vld);
			qdf_nbuf_set_sa_valid(nbuf, is_sa_vld);

			qdf_nbuf_pull_head(nbuf, soc->rx_pkt_tlv_size);
		} else if (qdf_nbuf_is_rx_chfrag_cont(nbuf)) {
			msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
			nbuf = dp_rx_sg_create(soc, nbuf);
			next = nbuf->next;

			if (qdf_nbuf_is_raw_frame(nbuf)) {
				DP_STATS_INC(vdev->pdev, rx_raw_pkts, 1);
				DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
							      rx.raw, 1,
							      msdu_len,
							      0);
			} else {
				dp_rx_nbuf_free(nbuf);
				DP_STATS_INC(soc, rx.err.scatter_msdu, 1);
				dp_info_rl("scatter msdu len %d, dropped",
					   msdu_len);
				nbuf = next;
				continue;
			}
		} else {
			msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
			pkt_len = msdu_len +
				  msdu_metadata.l3_hdr_pad +
				  soc->rx_pkt_tlv_size;

			qdf_nbuf_set_pktlen(nbuf, pkt_len);
			dp_rx_skip_tlvs(soc, nbuf, msdu_metadata.l3_hdr_pad);
		}

		dp_rx_send_pktlog(soc, rx_pdev, nbuf, QDF_TX_RX_STATUS_OK);

		if (!dp_wds_rx_policy_check(rx_tlv_hdr, vdev, txrx_peer)) {
			dp_rx_err("%pK: Policy Check Drop pkt", soc);
			DP_PEER_PER_PKT_STATS_INC(txrx_peer,
						  rx.policy_check_drop, 1, 0);
			tid_stats->fail_cnt[POLICY_CHECK_DROP]++;
			/* Drop & free packet */
			dp_rx_nbuf_free(nbuf);
			/* Statistics */
			nbuf = next;
			continue;
		}

		/*
		 * Drop non-EAPOL frames from unauthorized peer.
		 */
		if (qdf_likely(txrx_peer) &&
		    qdf_unlikely(!txrx_peer->authorize) &&
		    !qdf_nbuf_is_raw_frame(nbuf)) {
			bool is_eapol = qdf_nbuf_is_ipv4_eapol_pkt(nbuf) ||
					qdf_nbuf_is_ipv4_wapi_pkt(nbuf);

			if (!is_eapol) {
				DP_PEER_PER_PKT_STATS_INC(txrx_peer,
							  rx.peer_unauth_rx_pkt_drop,
							  1, 0);
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			}
		}

		if (soc->process_rx_status)
			dp_rx_cksum_offload(vdev->pdev, nbuf, rx_tlv_hdr);

		dp_rx_msdu_stats_update(soc, nbuf, rx_tlv_hdr, txrx_peer,
					rx_ctx_id, tid_stats, 0);

		if (qdf_likely(vdev->rx_decap_type ==
			       htt_cmn_pkt_type_ethernet)) {
			/* Due to HW issue, sometimes we see that the sa_idx
			 * and da_idx are invalid with sa_valid and da_valid
			 * bits set
			 *
			 * in this case we also see that value of
			 * sa_sw_peer_id is set as 0
			 *
			 * Drop the packet if sa_idx and da_idx OOB or
			 * sa_sw_peerid is 0
			 */
			if (!is_sa_da_idx_valid(max_ast, nbuf,
						msdu_metadata)) {
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				DP_STATS_INC(soc, rx.err.invalid_sa_da_idx, 1);
				continue;
			}
			if (qdf_unlikely(dp_rx_mec_check_wrapper(soc,
								 txrx_peer,
								 rx_tlv_hdr,
								 nbuf))) {
				/* this is a looped back MCBC pkt,drop it */
				DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
							      rx.mec_drop, 1,
							      QDF_NBUF_CB_RX_PKT_LEN(nbuf),
							      0);
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			}
			/* WDS Source Port Learning */
			if (qdf_likely(vdev->wds_enabled))
				dp_rx_wds_srcport_learn(soc,
							rx_tlv_hdr,
							txrx_peer,
							nbuf,
							msdu_metadata);

			/* Intrabss-fwd */
			if (dp_rx_check_ap_bridge(vdev))
				if (dp_rx_intrabss_fwd_rh(soc, txrx_peer,
							  rx_tlv_hdr,
							  nbuf,
							  msdu_metadata,
							  tid_stats)) {
					nbuf = next;
					tid_stats->intrabss_cnt++;
					continue; /* Get next desc */
				}
		}

		dp_rx_fill_gro_info(soc, rx_tlv_hdr, nbuf, &rx_ol_pkt_cnt);

		dp_rx_update_stats(soc, nbuf);

		dp_pkt_add_timestamp(txrx_peer->vdev, QDF_PKT_RX_DRIVER_ENTRY,
				     current_time, nbuf);

		DP_RX_LIST_APPEND(deliver_list_head,
				  deliver_list_tail,
				  nbuf);
		DP_PEER_STATS_FLAT_INC_PKT(txrx_peer, to_stack, 1,
					   QDF_NBUF_CB_RX_PKT_LEN(nbuf));
		if (qdf_unlikely(txrx_peer->in_twt))
			DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
						      rx.to_stack_twt, 1,
						      QDF_NBUF_CB_RX_PKT_LEN(nbuf),
						      0);

		tid_stats->delivered_to_stack++;
		nbuf = next;
	}

	DP_RX_DELIVER_TO_STACK(soc, vdev, txrx_peer, peer_id,
			       pkt_capture_offload,
			       deliver_list_head,
			       deliver_list_tail);

	if (qdf_likely(txrx_peer))
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);

	if (vdev && vdev->osif_fisa_flush)
		vdev->osif_fisa_flush(soc, rx_ctx_id);

	if (vdev && vdev->osif_gro_flush && rx_ol_pkt_cnt) {
		vdev->osif_gro_flush(vdev->osif_vdev,
					rx_ctx_id);
	}

	/* Update histogram statistics by looping through pdev's */
	DP_RX_HIST_STATS_PER_PDEV();
}

/*
 * dp_rx_defrag_deliver_rh(): Deliver defrag packet to stack
 * @peer: Pointer to the peer
 * @tid: Transmit Identifier
 * @head: Nbuf to be delivered
 *
 * Returns: None
 */
static inline void dp_rx_defrag_deliver_rh(struct dp_txrx_peer *txrx_peer,
					   unsigned int tid,
					   qdf_nbuf_t head)
{
	struct dp_vdev *vdev = txrx_peer->vdev;
	struct dp_soc *soc = vdev->pdev->soc;
	qdf_nbuf_t deliver_list_head = NULL;
	qdf_nbuf_t deliver_list_tail = NULL;
	uint8_t *rx_tlv_hdr;

	rx_tlv_hdr = qdf_nbuf_data(head);

	QDF_NBUF_CB_RX_VDEV_ID(head) = vdev->vdev_id;
	qdf_nbuf_set_tid_val(head, tid);
	qdf_nbuf_pull_head(head, soc->rx_pkt_tlv_size);

	DP_RX_LIST_APPEND(deliver_list_head, deliver_list_tail,
			  head);
	dp_rx_deliver_to_stack(soc, vdev, txrx_peer, deliver_list_head,
			       deliver_list_tail);
}

static
QDF_STATUS dp_rx_defrag_store_fragment_rh(struct dp_soc *soc, qdf_nbuf_t frag)
{
	struct dp_rx_reorder_array_elem *rx_reorder_array_elem;
	struct dp_pdev *pdev;
	struct dp_txrx_peer *txrx_peer = NULL;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	uint16_t peer_id, tid;
	uint8_t fragno, more_frag, all_frag_present = 0;
	uint16_t rxseq;
	QDF_STATUS status;
	struct dp_rx_tid_defrag *rx_tid;
	uint8_t mpdu_sequence_control_valid;
	uint8_t mpdu_frame_control_valid;
	uint8_t *rx_buf_start = qdf_nbuf_data(frag);
	uint32_t msdu_len;

	if (qdf_nbuf_len(frag) > 0) {
		dp_rx_info("Dropping unexpected packet with skb_len: %d, data len: %d",
			   (uint32_t)qdf_nbuf_len(frag), frag->data_len);
		DP_STATS_INC(soc, rx.rx_frag_err_len_error, 1);
		goto discard_frag;
	}

	msdu_len = QDF_NBUF_CB_RX_PKT_LEN(frag);
	qdf_nbuf_set_pktlen(frag, (msdu_len + soc->rx_pkt_tlv_size));
	qdf_nbuf_append_ext_list(frag, NULL, 0);

	/* Check if the packet is from a valid peer */
	peer_id = QDF_NBUF_CB_RX_PEER_ID(frag);
	txrx_peer = dp_txrx_peer_get_ref_by_id(soc, peer_id, &txrx_ref_handle,
					       DP_MOD_ID_RX);

	if (!txrx_peer) {
		/* We should not receive anything from unknown peer
		 * however, that might happen while we are in the monitor mode.
		 * We don't need to handle that here
		 */
		dp_rx_info_rl("Unknown peer with peer_id %d, dropping fragment",
			      peer_id);
		DP_STATS_INC(soc, rx.rx_frag_err_no_peer, 1);
		goto discard_frag;
	}

	tid = qdf_nbuf_get_tid_val(frag);
	if (tid >= DP_MAX_TIDS) {
		dp_rx_info("TID out of bounds: %d", tid);
		qdf_assert_always(0);
		goto discard_frag;
	}

	mpdu_sequence_control_valid =
		hal_rx_get_mpdu_sequence_control_valid(soc->hal_soc,
						       rx_buf_start);

	/* Invalid MPDU sequence control field, MPDU is of no use */
	if (!mpdu_sequence_control_valid) {
		dp_rx_err("Invalid MPDU seq control field, dropping MPDU");

		qdf_assert(0);
		goto discard_frag;
	}

	mpdu_frame_control_valid =
		hal_rx_get_mpdu_frame_control_valid(soc->hal_soc,
						    rx_buf_start);

	/* Invalid frame control field */
	if (!mpdu_frame_control_valid) {
		dp_rx_err("Invalid frame control field, dropping MPDU");

		qdf_assert(0);
		goto discard_frag;
	}

	/* Current mpdu sequence */
	more_frag = dp_rx_frag_get_more_frag_bit(soc, rx_buf_start);

	/* HW does not populate the fragment number as of now
	 * need to get from the 802.11 header
	 */
	fragno = dp_rx_frag_get_mpdu_frag_number(soc, rx_buf_start);
	rxseq = dp_rx_frag_get_mpdu_seq_number(soc, rx_buf_start);

	pdev = txrx_peer->vdev->pdev;
	rx_tid = &txrx_peer->rx_tid[tid];

	qdf_spin_lock_bh(&rx_tid->defrag_tid_lock);
	rx_reorder_array_elem = txrx_peer->rx_tid[tid].array;
	if (!rx_reorder_array_elem) {
		dp_err_rl("Rcvd Fragmented pkt before tid setup for peer %pK",
			  txrx_peer);
		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
		goto discard_frag;
	}

	/*
	 * !more_frag: no more fragments to be delivered
	 * !frag_no: packet is not fragmented
	 * !rx_reorder_array_elem->head: no saved fragments so far
	 */
	if (!more_frag && !fragno && !rx_reorder_array_elem->head) {
		/* We should not get into this situation here.
		 * It means an unfragmented packet with fragment flag
		 * is delivered over frag indication.
		 * Typically it follows normal rx path.
		 */
		dp_rx_err("Rcvd unfragmented pkt on fragmented path, dropping");

		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
		qdf_assert(0);
		goto discard_frag;
	}

	/* Check if the fragment is for the same sequence or a different one */
	dp_rx_debug("rx_tid %d", tid);
	if (rx_reorder_array_elem->head) {
		dp_rx_debug("rxseq %d\n", rxseq);
		if (rxseq != rx_tid->curr_seq_num) {
			dp_rx_debug("mismatch cur_seq %d rxseq %d\n",
				    rx_tid->curr_seq_num, rxseq);
			/* Drop stored fragments if out of sequence
			 * fragment is received
			 */
			dp_rx_reorder_flush_frag(txrx_peer, tid);

			DP_STATS_INC(soc, rx.rx_frag_oor, 1);

			dp_rx_debug("cur rxseq %d\n", rxseq);
			/*
			 * The sequence number for this fragment becomes the
			 * new sequence number to be processed
			 */
			rx_tid->curr_seq_num = rxseq;
		}
	} else {
		/* Check if we are processing first fragment if it is
		 * not first fragment discard fragment.
		 */
		if (fragno) {
			qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
			goto discard_frag;
		}
		dp_rx_debug("cur rxseq %d\n", rxseq);
		/* Start of a new sequence */
		dp_rx_defrag_cleanup(txrx_peer, tid);
		rx_tid->curr_seq_num = rxseq;
	}

	/*
	 * If the earlier sequence was dropped, this will be the fresh start.
	 * Else, continue with next fragment in a given sequence
	 */
	status = dp_rx_defrag_fraglist_insert(txrx_peer, tid,
					      &rx_reorder_array_elem->head,
					      &rx_reorder_array_elem->tail,
					      frag, &all_frag_present);

	if (pdev->soc->rx.flags.defrag_timeout_check)
		dp_rx_defrag_waitlist_remove(txrx_peer, tid);

	/* Yet to receive more fragments for this sequence number */
	if (!all_frag_present) {
		uint32_t now_ms =
			qdf_system_ticks_to_msecs(qdf_system_ticks());

		txrx_peer->rx_tid[tid].defrag_timeout_ms =
			now_ms + pdev->soc->rx.defrag.timeout_ms;

		if (pdev->soc->rx.flags.defrag_timeout_check)
			dp_rx_defrag_waitlist_add(txrx_peer, tid);
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX_ERR);
		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);

		return QDF_STATUS_SUCCESS;
	}

	dp_rx_debug("All fragments received for sequence: %d", rxseq);

	/* Process the fragments */
	status = dp_rx_defrag(txrx_peer, tid, rx_reorder_array_elem->head,
			      rx_reorder_array_elem->tail);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_rx_err("Fragment processing failed");

		dp_rx_defrag_cleanup(txrx_peer, tid);
		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
		goto end;
	}

	dp_rx_defrag_deliver_rh(txrx_peer, tid, rx_reorder_array_elem->head);
	dp_rx_debug("Fragmented sequence successfully reinjected");

	dp_rx_defrag_cleanup(txrx_peer, tid);
	qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);

	dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX_ERR);

	return QDF_STATUS_SUCCESS;

discard_frag:
	dp_rx_nbuf_free(frag);
end:
	if (txrx_peer)
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX_ERR);

	DP_STATS_INC(soc, rx.rx_frag_err, 1);
	return QDF_STATUS_E_DEFRAG_ERROR;
}

void
dp_rx_frag_indication_handler(struct dp_soc *soc, qdf_nbuf_t data_ind,
			      uint16_t vdev_id, uint16_t peer_id)
{
	uint8_t *data_ind_msg;
	uint32_t *msg_word;
	uint32_t rx_ctx_id;
	qdf_nbuf_t nbuf;
	union dp_rx_desc_list_elem_t *head = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t rx_buf_cookie;
	struct dp_rx_desc *rx_desc;
	uint8_t mac_id = 0;

	qdf_assert(soc);

	data_ind_msg = qdf_nbuf_data(data_ind);
	msg_word =
		(uint32_t *)(data_ind_msg + HTT_RX_DATA_IND_HDR_SIZE);
	rx_ctx_id =
		dp_rx_get_ctx_id_frm_napiid(QDF_NBUF_CB_RX_CTX_ID(data_ind));

	rx_buf_cookie =
		HTT_RX_DATA_MSDU_INFO_SW_BUFFER_COOKIE_GET(*(msg_word + 1));
	rx_desc = dp_rx_cookie_2_va_rxdma_buf(soc, rx_buf_cookie);
	if (qdf_unlikely(!rx_desc && !rx_desc->nbuf &&
			 !rx_desc->in_use)) {
		dp_rx_err("Invalid RX descriptor");
		qdf_assert_always(0);
		/* TODO handle this if its valid case */
	}

	if (qdf_unlikely(!dp_rx_desc_check_magic(rx_desc))) {
		dp_err("Invalid rx_desc cookie=%d", rx_buf_cookie);
		DP_STATS_INC(soc, rx.err.rx_desc_invalid_magic, 1);
		qdf_assert(0);
	}

	nbuf = rx_desc->nbuf;
	QDF_NBUF_CB_RX_PKT_LEN(nbuf) =
		HTT_RX_DATA_MSDU_INFO_MSDU_LENGTH_GET(*(msg_word + 2));
	qdf_nbuf_set_tid_val(nbuf, HTT_RX_DATA_MSDU_INFO_TID_INFO_GET(*(msg_word + 2)));
	QDF_NBUF_CB_RX_PEER_ID(nbuf) = peer_id;
	QDF_NBUF_CB_RX_VDEV_ID(nbuf) = vdev_id;
	QDF_NBUF_CB_RX_CTX_ID(nbuf) = rx_ctx_id;

	dp_rx_nbuf_unmap(soc, rx_desc, rx_ctx_id);

	dp_rx_add_to_free_desc_list(&head, &tail, rx_desc);

	dp_rx_buffers_replenish_simple(soc, rx_desc->pool_id,
				       &soc->rx_refill_buf_ring[mac_id],
				       &soc->rx_desc_buf[rx_desc->pool_id],
				       1, &head, &tail);

	if (dp_rx_buffer_pool_refill(soc, nbuf, rx_desc->pool_id))
		/* fragment queued back to the pool no frag to handle*/
		return;

	/* Process fragment-by-fragment */
	status = dp_rx_defrag_store_fragment_rh(soc, nbuf);
	if (QDF_IS_STATUS_ERROR(status))
		dp_rx_err("Unable to handle frag ret:%u", status);
}

QDF_STATUS dp_rx_desc_pool_init_rh(struct dp_soc *soc,
				   struct rx_desc_pool *rx_desc_pool,
				   uint32_t pool_id)
{
	return dp_rx_desc_pool_init_generic(soc, rx_desc_pool, pool_id);
}

void dp_rx_desc_pool_deinit_rh(struct dp_soc *soc,
			       struct rx_desc_pool *rx_desc_pool,
			       uint32_t pool_id)
{
}

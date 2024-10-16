/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#include <hal_api.h>
#include <wlan_cfg.h>
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_htt.h"
#include "dp_mon.h"
#include "htt.h"
#include "htc_api.h"
#include "htc.h"
#include "htc_packet.h"
#include "dp_mon_filter.h"
#include <dp_mon_2.0.h>
#include <dp_rx_mon_2.0.h>
#include <dp_mon_filter_2.0.h>
#include <dp_be.h>
#ifdef QCA_SUPPORT_LITE_MONITOR
#include "dp_lite_mon.h"
#endif

#define HTT_MSG_BUF_SIZE(msg_bytes) \
   ((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

void dp_mon_filter_dealloc_2_0(struct dp_pdev *pdev)
{
	enum dp_mon_filter_mode mode;
	struct dp_mon_filter_be **mon_filter = NULL;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("Pdev context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	mon_filter = mon_pdev_be->filter_be;
	if (!mon_filter) {
		dp_mon_filter_err("Found NULL memory for the Monitor filter");
		return;
	}

	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		if (!mon_filter[mode])
			continue;

		qdf_mem_free(mon_filter[mode]);
		mon_filter[mode] = NULL;
	}

	qdf_mem_free(mon_filter);
	mon_pdev_be->filter_be = NULL;
}

QDF_STATUS dp_mon_filter_alloc_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be **mon_filter = NULL;
	enum dp_mon_filter_mode mode;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return QDF_STATUS_E_FAILURE;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return QDF_STATUS_E_FAILURE;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	mon_filter = (struct dp_mon_filter_be **)qdf_mem_malloc(
			(sizeof(struct dp_mon_filter_be *) *
			 DP_MON_FILTER_MAX_MODE));
	if (!mon_filter) {
		dp_mon_filter_err("Monitor filter mem allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_zero(mon_filter,
		     sizeof(struct dp_mon_filter_be *) * DP_MON_FILTER_MAX_MODE);

	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		mon_filter[mode] = qdf_mem_malloc(sizeof(struct dp_mon_filter_be) *
						  DP_MON_FILTER_SRNG_TYPE_MAX);
		mon_pdev_be->filter_be = mon_filter;
		if (!mon_filter[mode])
			goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	dp_mon_filter_dealloc(mon_pdev);
	return QDF_STATUS_E_FAILURE;
}

void dp_rx_mon_hdr_length_set(uint32_t *msg_word,
			      struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (!msg_word || !tlv_filter)
		return;

	HTT_RX_RING_SELECTION_CFG_RX_HDR_LEN_SET(*msg_word,
						 tlv_filter->rx_hdr_length);
}

void dp_rx_mon_packet_length_set(uint32_t *msg_word,
				 struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (!msg_word || !tlv_filter)
		return;

	HTT_RX_RING_SELECTION_CFG_CONFIG_LENGTH_MGMT_SET(*msg_word,
							 tlv_filter->mgmt_dma_length);
	HTT_RX_RING_SELECTION_CFG_CONFIG_LENGTH_CTRL_SET(*msg_word,
							 tlv_filter->ctrl_dma_length);
	HTT_RX_RING_SELECTION_CFG_CONFIG_LENGTH_DATA_SET(*msg_word,
							 tlv_filter->data_dma_length);
}

void dp_rx_mon_enable_set(uint32_t *msg_word,
			  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (!msg_word || !tlv_filter)
		return;

	HTT_RX_RING_SELECTION_CFG_RX_MON_GLOBAL_EN_SET(*msg_word,
						       tlv_filter->enable);
}

void dp_rx_mon_enable_mpdu_logging(uint32_t *msg_word,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (!msg_word || !tlv_filter)
		return;

	if (tlv_filter->mgmt_dma_length) {
		HTT_RX_RING_SELECTION_CFG_PKT_TYPE_ENABLE_MSDU_MPDU_LOGGING_SET(*msg_word, 1);
		HTT_RX_RING_SELECTION_CFG_DMA_MPDU_MGMT_SET(*msg_word, tlv_filter->mgmt_mpdu_log);
	}

	if (tlv_filter->ctrl_dma_length) {
		HTT_RX_RING_SELECTION_CFG_PKT_TYPE_ENABLE_MSDU_MPDU_LOGGING_SET(*msg_word, 2);
		HTT_RX_RING_SELECTION_CFG_DMA_MPDU_CTRL_SET(*msg_word, tlv_filter->ctrl_mpdu_log);
	}

	if (tlv_filter->data_dma_length) {
		HTT_RX_RING_SELECTION_CFG_PKT_TYPE_ENABLE_MSDU_MPDU_LOGGING_SET(*msg_word, 4);
		HTT_RX_RING_SELECTION_CFG_DMA_MPDU_DATA_SET(*msg_word, tlv_filter->data_mpdu_log);
	}
}

void
dp_rx_mon_word_mask_subscribe(uint32_t *msg_word,
				  struct htt_rx_ring_tlv_filter *tlv_filter)
{

#ifdef QCA_MONITOR_2_0_SUPPORT_WAR /* Yet to get FW support */
	HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_WORD_MASK_SET(*msg_word,
			tlv_filter->rx_mpdu_end_wmask);
#endif
	/* word 15 */
	msg_word++;

	/* word 16 */
	msg_word++;
	*msg_word = 0;
	if (tlv_filter->rx_pkt_tlv_offset) {
		HTT_RX_RING_SELECTION_CFG_ENABLE_RX_PKT_TLV_OFFSET_SET(*msg_word, 1);
		HTT_RX_RING_SELECTION_CFG_RX_PKT_TLV_OFFSET_SET(*msg_word,
								tlv_filter->rx_pkt_tlv_offset);
	}
}

void
dp_rx_mon_enable_fpmo(uint32_t *msg_word,
		      struct htt_rx_ring_tlv_filter *tlv_filter)
{
#ifdef FW_SUPPORT_NOT_YET
	if (!msg_word || !tlv_filter)
		return;

	if (tlv_filter->enable_fpmo) {
		/* TYPE: MGMT */
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0000,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0001,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_ASSOC_RES) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0010,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0011,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_REASSOC_RES) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0100,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_PROBE_REQ) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0101,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_PROBE_RES) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0110,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_TIM_ADVT) ? 1 : 0);
		/* reserved */
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 0111,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_RESERVED_7) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1000,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_BEACON) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1001,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_ATIM) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1010,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_DISASSOC) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1011,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_AUTH) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1100,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_DEAUTH) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1101,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_ACTION) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1110,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
		/* reserved*/
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, MGMT, 1111,
			(tlv_filter->fpmo_mgmt_filter &
			FILTER_MGMT_RESERVED_15) ? 1 : 0);

		/* TYPE: CTRL */
		/* reserved */
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0000,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_RESERVED_1) ? 1 : 0);
		/* reserved */
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0001,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_RESERVED_2) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0010,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_TRIGGER) ? 1 : 0);
		/* reserved */
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0011,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_RESERVED_4) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0100,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0101,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_VHT_NDP) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0110,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_FRAME_EXT) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 0111,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_CTRLWRAP) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1000,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_BA_REQ) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1001,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_BA) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1010,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_PSPOLL) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1011,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_RTS) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1100,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_CTS) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1101,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_ACK) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1110,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_CFEND) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG0,
			FPMO, CTRL, 1111,
			(tlv_filter->fpmo_ctrl_filter &
			FILTER_CTRL_CFEND_CFACK) ? 1 : 0);

		/* word 18 */
		msg_word++;
		*msg_word = 0;

		/* TYPE: DATA */
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG1,
			FPMO, DATA, MCAST,
			(tlv_filter->fpmo_data_filter &
			FILTER_DATA_MCAST) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG1,
			FPMO, DATA, UCAST,
			(tlv_filter->fpmo_data_filter &
			FILTER_DATA_UCAST) ? 1 : 0);
		htt_rx_ring_pkt_enable_subtype_set(*msg_word, FLAG1,
			FPMO, DATA, NULL,
			(tlv_filter->fpmo_data_filter &
			FILTER_DATA_NULL) ? 1 : 0);

	} else {
		/* clear word 18 if fpmo is disabled
		 * word 17 is already cleared by caller
		 */

		/* word 18 */
		msg_word++;
		*msg_word = 0;
	}
#endif
}

static void
htt_tx_tlv_filter_mask_set_in0(uint32_t *msg_word,
			       struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct dp_tx_mon_downstream_tlv_config *tlv = &htt_tlv_filter->dtlvs;

	if (tlv->tx_fes_setup)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_FES_SETUP,
							 tlv->tx_fes_setup);

	if (tlv->tx_peer_entry)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_PEER_ENTRY,
							 tlv->tx_peer_entry);

	if (tlv->tx_queue_extension)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_QUEUE_EXTENSION,
							 tlv->tx_queue_extension);

	if (tlv->tx_last_mpdu_end)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_LAST_MPDU_END,
							 tlv->tx_last_mpdu_end);

	if (tlv->tx_last_mpdu_fetched)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_LAST_MPDU_FETCHED,
							 tlv->tx_last_mpdu_fetched);

	if (tlv->tx_data_sync)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_DATA_SYNC,
							 tlv->tx_data_sync);

	if (tlv->pcu_ppdu_setup_init)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 PCU_PPDU_SETUP_INIT,
							 tlv->pcu_ppdu_setup_init);

	if (tlv->fw2s_mon)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 FW2SW_MON,
							 tlv->fw2s_mon);

	if (tlv->tx_loopback_setup)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_LOOPBACK_SETUP,
							 tlv->tx_loopback_setup);

	if (tlv->sch_critical_tlv_ref)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 SCH_CRITICAL_TLV_REFERENCE,
							 tlv->sch_critical_tlv_ref);

	if (tlv->ndp_preamble_done)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 NDP_PREAMBLE_DONE,
							 tlv->ndp_preamble_done);

	if (tlv->tx_raw_frame_setup)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_RAW_OR_NATIVE_FRAME_SETUP,
							 tlv->tx_raw_frame_setup);

	if (tlv->txpcu_user_setup)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TXPCU_USER_SETUP,
							 tlv->txpcu_user_setup);

	if (tlv->rxpcu_setup)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 RXPCU_SETUP,
							 tlv->rxpcu_setup);

	if (tlv->rxpcu_setup_complete)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 RXPCU_SETUP_COMPLETE,
							 tlv->rxpcu_setup_complete);

	if (tlv->coex_tx_req)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 COEX_TX_REQ,
							 tlv->coex_tx_req);

	if (tlv->rxpcu_user_setup)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 RXPCU_USER_SETUP,
							 tlv->rxpcu_user_setup);

	if (tlv->rxpcu_user_setup_ext)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 RXPCU_USER_SETUP_EXT,
							 tlv->rxpcu_user_setup_ext);

	if (tlv->wur_data)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_WUR_DATA,
							 tlv->wur_data);

	if (tlv->tqm_mpdu_global_start)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TQM_MPDU_GLOBAL_START,
							 tlv->tqm_mpdu_global_start);

	if (tlv->tx_fes_setup_complete)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 TX_FES_SETUP_COMPLETE,
							 tlv->tx_fes_setup_complete);

	if (tlv->scheduler_end)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 SCHEDULER_END,
							 tlv->scheduler_end);

	if (tlv->sch_wait_instr_tx_path)
		htt_tx_monitor_tlv_filter_in0_enable_set(*msg_word,
							 SCH_WAIT_INSTR_TX_PATH,
							 tlv->sch_wait_instr_tx_path);
}

static void
htt_tx_tlv_filter_mask_set_in1(uint32_t *msg_word,
			       struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct dp_tx_mon_upstream_tlv_config *tlv = &htt_tlv_filter->utlvs;

	if (tlv->rx_response_required_info)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RX_RESPONSE_REQUIRED_INFO,
							 tlv->rx_response_required_info);

	if (tlv->response_start_status)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RESPONSE_START_STATUS,
							 tlv->response_start_status);

	if (tlv->response_end_status)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RESPONSE_END_STATUS,
							 tlv->response_end_status);

	if (tlv->tx_fes_status_start)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_START,
							 tlv->tx_fes_status_start);

	if (tlv->tx_fes_status_end)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_END,
							 tlv->tx_fes_status_end);

	if (tlv->tx_fes_status_start_ppdu)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_START_PPDU,
							 tlv->tx_fes_status_start_ppdu);

	if (tlv->tx_fes_status_user_ppdu)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_USER_PPDU,
							 tlv->tx_fes_status_user_ppdu);

	if (tlv->tx_fes_status_ack_or_ba)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_ACK_OR_BA,
							 tlv->tx_fes_status_ack_or_ba);

	if (tlv->tx_fes_status_1k_ba)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_1K_BA,
							 tlv->tx_fes_status_1k_ba);

	if (tlv->tx_fes_status_start_prot)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_START_PROT,
							 tlv->tx_fes_status_start_prot);

	if (tlv->tx_fes_status_prot)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_PROT,
							 tlv->tx_fes_status_prot);

	if (tlv->tx_fes_status_user_response)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TX_FES_STATUS_USER_RESPONSE,
							 tlv->tx_fes_status_user_response);

	if (tlv->rx_frame_bitmap_ack)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RX_FRAME_BITMAP_ACK,
							 tlv->rx_frame_bitmap_ack);

	if (tlv->rx_frame_1k_bitmap_ack)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RX_FRAME_1K_BITMAP_ACK,
							 tlv->rx_frame_1k_bitmap_ack);

	if (tlv->coex_tx_status)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 COEX_TX_STATUS,
							 tlv->coex_tx_status);

	if (tlv->received_response_info)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RECEIVED_RESPONSE_INFO,
							 tlv->received_response_info);

	if (tlv->received_response_info_p2)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RECEIVED_RESPONSE_INFO_PART2,
							 tlv->received_response_info_p2);

	if (tlv->ofdma_trigger_details)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 OFDMA_TRIGGER_DETAILS,
							 tlv->ofdma_trigger_details);

	if (tlv->received_trigger_info)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 RECEIVED_TRIGGER_INFO,
							 tlv->received_trigger_info);

	if (tlv->pdg_tx_request)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 PDG_TX_REQUEST,
							 tlv->pdg_tx_request);

	if (tlv->pdg_response)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 PDG_RESPONSE,
							 tlv->pdg_response);

	if (tlv->pdg_trig_response)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 PDG_TRIG_RESPONSE,
							 tlv->pdg_trig_response);

	if (tlv->trigger_response_tx_done)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 TRIGGER_RESPONSE_TX_DONE,
							 tlv->trigger_response_tx_done);

	if (tlv->prot_tx_end)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 PROT_TX_END,
							 tlv->prot_tx_end);

	if (tlv->ppdu_tx_end)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 PPDU_TX_END,
							 tlv->ppdu_tx_end);

	if (tlv->r2r_status_end)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 R2R_STATUS_END,
							 tlv->r2r_status_end);

	if (tlv->flush_req)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 FLUSH_REQ,
							 tlv->flush_req);

	if (tlv->mactx_phy_desc)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 MACTX_PHY_DESC,
							 tlv->mactx_phy_desc);

	if (tlv->mactx_user_desc_cmn)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 MACTX_USER_DESC_COMMON,
							 tlv->mactx_user_desc_cmn);

	if (tlv->mactx_user_desc_per_usr)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 MACTX_USER_DESC_PER_USER,
							 tlv->mactx_user_desc_per_usr);

	if (tlv->l_sig_a)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 L_SIG_A,
							 tlv->l_sig_a);

	if (tlv->l_sig_b)
		htt_tx_monitor_tlv_filter_in1_enable_set(*msg_word,
							 L_SIG_B,
							 tlv->l_sig_b);
}

static void
htt_tx_tlv_filter_mask_set_in2(uint32_t *msg_word,
			       struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct dp_tx_mon_upstream_tlv_config *tlv = &htt_tlv_filter->utlvs;

	if (tlv->ht_sig)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HT_SIG,
							 tlv->ht_sig);

	if (tlv->vht_sig_a)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_A,
							 tlv->vht_sig_a);

	if (tlv->vht_sig_b_su20)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_SU20,
							 tlv->vht_sig_b_su20);

	if (tlv->vht_sig_b_su40)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_SU40,
							 tlv->vht_sig_b_su40);

	if (tlv->vht_sig_b_su80)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_SU80,
							 tlv->vht_sig_b_su80);

	if (tlv->vht_sig_b_su160)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_SU160,
							 tlv->vht_sig_b_su160);

	if (tlv->vht_sig_b_mu20)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_MU20,
							 tlv->vht_sig_b_mu20);

	if (tlv->vht_sig_b_mu40)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_MU40,
							 tlv->vht_sig_b_mu40);

	if (tlv->vht_sig_b_mu80)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_MU80,
							 tlv->vht_sig_b_mu80);

	if (tlv->vht_sig_b_mu160)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 VHT_SIG_B_MU160,
							 tlv->vht_sig_b_mu160);

	if (tlv->tx_service)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TX_SERVICE,
							 tlv->tx_service);

	if (tlv->he_sig_a_su)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HE_SIG_A_SU,
							 tlv->he_sig_a_su);

	if (tlv->he_sig_a_mu_dl)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HE_SIG_A_MU_DL,
							 tlv->he_sig_a_mu_dl);

	if (tlv->he_sig_a_mu_ul)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HE_SIG_A_MU_UL,
							 tlv->he_sig_a_mu_ul);

	if (tlv->he_sig_b1_mu)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HE_SIG_B1_MU,
							 tlv->he_sig_b1_mu);

	if (tlv->he_sig_b2_mu)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HE_SIG_B2_MU,
							 tlv->he_sig_b2_mu);

	if (tlv->he_sig_b2_ofdma)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 HE_SIG_B2_OFDMA,
							 tlv->he_sig_b2_ofdma);

	if (tlv->u_sig_eht_su_mu)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 U_SIG_EHT_SU_MU,
							 tlv->u_sig_eht_su_mu);

	if (tlv->u_sig_eht_su)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 U_SIG_EHT_SU,
							 tlv->u_sig_eht_su);

	if (tlv->u_sig_eht_tb)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 U_SIG_EHT_TB,
							 tlv->u_sig_eht_tb);

	if (tlv->eht_sig_usr_su)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 EHT_SIG_USR_SU,
							 tlv->eht_sig_usr_su);

	if (tlv->eht_sig_usr_mu_mimo)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 EHT_SIG_USR_MU_MIMO,
							 tlv->eht_sig_usr_mu_mimo);

	if (tlv->eht_sig_usr_ofdma)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 EHT_SIG_USR_OFDMA,
							 tlv->eht_sig_usr_ofdma);

	if (tlv->phytx_ppdu_header_info_request)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 PHYTX_PPDU_HEADER_INFO_REQUEST,
							 tlv->phytx_ppdu_header_info_request);

	if (tlv->tqm_update_tx_mpdu_count)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TQM_UPDATE_TX_MPDU_COUNT,
							 tlv->tqm_update_tx_mpdu_count);

	if (tlv->tqm_acked_mpdu)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TQM_ACKED_MPDU,
							 tlv->tqm_acked_mpdu);

	if (tlv->tqm_acked_1k_mpdu)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TQM_ACKED_1K_MPDU,
							 tlv->tqm_acked_1k_mpdu);

	if (tlv->txpcu_buf_status)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TXPCU_BUFFER_STATUS,
							 tlv->txpcu_buf_status);

	if (tlv->txpcu_user_buf_status)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TXPCU_USER_BUFFER_STATUS,
							 tlv->txpcu_user_buf_status);

	if (tlv->txdma_stop_request)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TXDMA_STOP_REQUEST,
							 tlv->txdma_stop_request);

	if (tlv->expected_response)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 EXPECTED_RESPONSE,
							 tlv->expected_response);

	if (tlv->tx_mpdu_count_transfer_end)
		htt_tx_monitor_tlv_filter_in2_enable_set(*msg_word,
							 TX_MPDU_COUNT_TRANSFER_END,
							 tlv->tx_mpdu_count_transfer_end);
}

static void
htt_tx_tlv_filter_mask_set_in3(uint32_t *msg_word,
			       struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct dp_tx_mon_upstream_tlv_config *tlv = &htt_tlv_filter->utlvs;

	if (tlv->rx_trig_info)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_TRIG_INFO,
							 tlv->rx_trig_info);

	if (tlv->rxpcu_tx_setup_clear)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RXPCU_TX_SETUP_CLEAR,
							 tlv->rxpcu_tx_setup_clear);

	if (tlv->rx_frame_bitmap_req)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_FRAME_BITMAP_REQ,
							 tlv->rx_frame_bitmap_req);

	if (tlv->rx_phy_sleep)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_PHY_SLEEP,
							 tlv->rx_phy_sleep);

	if (tlv->txpcu_preamble_done)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 TXPCU_PREAMBLE_DONE,
							 tlv->txpcu_preamble_done);

	if (tlv->txpcu_phytx_debug32)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 TXPCU_PHYTX_DEBUG32,
							 tlv->txpcu_phytx_debug32);

	if (tlv->txpcu_phytx_other_transmit_info32)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 TXPCU_PHYTX_OTHER_TRANSMIT_INFO32,
							 tlv->txpcu_phytx_other_transmit_info32);

	if (tlv->rx_ppdu_noack_report)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_PPDU_NO_ACK_REPORT,
							 tlv->rx_ppdu_noack_report);

	if (tlv->rx_ppdu_ack_report)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_PPDU_ACK_REPORT,
							 tlv->rx_ppdu_ack_report);

	if (tlv->coex_rx_status)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 COEX_RX_STATUS,
							 tlv->coex_rx_status);

	if (tlv->rx_start_param)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_START_PARAM,
							 tlv->rx_start_param);

	if (tlv->tx_cbf_info)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 TX_CBF_INFO,
							 tlv->tx_cbf_info);

	if (tlv->rxpcu_early_rx_indication)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RXPCU_EARLY_RX_INDICATION,
							 tlv->rxpcu_early_rx_indication);

	if (tlv->received_response_user_7_0)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RECEIVED_RESPONSE_USER_7_0,
							 tlv->received_response_user_7_0);

	if (tlv->received_response_user_15_8)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RECEIVED_RESPONSE_USER_15_8,
							 tlv->received_response_user_15_8);

	if (tlv->received_response_user_23_16)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RECEIVED_RESPONSE_USER_23_16,
							 tlv->received_response_user_23_16);

	if (tlv->received_response_user_31_24)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RECEIVED_RESPONSE_USER_31_24,
							 tlv->received_response_user_31_24);

	if (tlv->received_response_user_36_32)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RECEIVED_RESPONSE_USER_36_32,
							 tlv->received_response_user_36_32);

	if (tlv->rx_pm_info)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_PM_INFO,
							 tlv->rx_pm_info);

	if (tlv->rx_preamble)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 RX_PREAMBLE,
							 tlv->rx_preamble);

	if (tlv->others)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 OTHERS,
							 tlv->others);

	if (tlv->mactx_pre_phy_desc)
		htt_tx_monitor_tlv_filter_in3_enable_set(*msg_word,
							 MACTX_PRE_PHY_DESC,
							 tlv->mactx_pre_phy_desc);
}

/*
 * dp_htt_h2t_send_complete_free_netbuf() - Free completed buffer
 * @soc:	SOC handl
 * @status:	Completion status
 * @netbuf:	HTT buffer
 */
static void
dp_htt_h2t_send_complete_free_netbuf(
	void *soc, A_STATUS status, qdf_nbuf_t netbuf)
{
	qdf_nbuf_free(netbuf);
}

/*
 * htt_h2t_tx_ring_cfg() - Send SRNG packet and TLV filter
 * config message to target
 * @htt_soc:	HTT SOC handle
 * @pdev_id:	WIN- PDEV Id, MCL- mac id
 * @hal_srng:	Opaque HAL SRNG pointer
 * @hal_ring_type:	SRNG ring type
 * @ring_buf_size:	SRNG buffer size
 * @htt_tlv_filter:	Rx SRNG TLV and filter setting
 * Return: 0 on success; error code on failure
 */
int htt_h2t_tx_ring_cfg(struct htt_soc *htt_soc, int pdev_id,
			hal_ring_handle_t hal_ring_hdl,
			int hal_ring_type, int ring_buf_size,
			struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct htt_soc *soc = (struct htt_soc *)htt_soc;
	struct dp_htt_htc_pkt *pkt;
	qdf_nbuf_t htt_msg;
	uint32_t *msg_word;
	struct hal_srng_params srng_params;
	uint32_t htt_ring_id;
	uint8_t *htt_logger_bufp;
	int target_pdev_id;
	QDF_STATUS status;

	htt_msg = qdf_nbuf_alloc(soc->osdev,
				 HTT_MSG_BUF_SIZE(HTT_TX_MONITOR_CFG_SZ),

	/* reserve room for the HTC header */
	HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
	if (!htt_msg)
		goto fail0;

	hal_get_srng_params(soc->hal_soc, hal_ring_hdl, &srng_params);

	switch (hal_ring_type) {
	case TX_MONITOR_BUF:
		htt_ring_id = HTT_TX_MON_HOST2MON_BUF_RING;
		break;
	case TX_MONITOR_DST:
		htt_ring_id = HTT_TX_MON_MON2HOST_DEST_RING;
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Ring currently not supported", __func__);
		goto fail1;
	}

	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to qdf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	if (qdf_nbuf_put_tail(htt_msg, HTT_TX_MONITOR_CFG_SZ) == NULL) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Failed to expand head for TX Ring Cfg msg",
			  __func__);
		goto fail1; /* failure */
	}

	msg_word = (uint32_t *)qdf_nbuf_data(htt_msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(htt_msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	htt_logger_bufp = (uint8_t *)msg_word;
	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_TX_MONITOR_CFG);

	/*
	 * pdev_id is indexed from 0 whereas mac_id is indexed from 1
	 * SW_TO_SW and SW_TO_HW rings are unaffected by this
	 */
	target_pdev_id =
	dp_get_target_pdev_id_for_host_pdev_id(soc->dp_soc, pdev_id);

	HTT_TX_MONITOR_CFG_PDEV_ID_SET(*msg_word,
				       target_pdev_id);

	HTT_TX_MONITOR_CFG_RING_ID_SET(*msg_word, htt_ring_id);

	HTT_TX_MONITOR_CFG_STATUS_TLV_SET(*msg_word,
		!!(srng_params.flags & HAL_SRNG_MSI_SWAP));

	HTT_TX_MONITOR_CFG_TX_MON_GLOBAL_EN_SET(*msg_word,
						htt_tlv_filter->enable);

	/* word 1 */
	msg_word++;
	*msg_word = 0;
	HTT_TX_MONITOR_CFG_RING_BUFFER_SIZE_SET(*msg_word,
						ring_buf_size);

	if (htt_tlv_filter->mgmt_filter)
		htt_tx_ring_pkt_type_set(*msg_word, ENABLE_FLAGS,
					 MGMT, 1);

	if (htt_tlv_filter->ctrl_filter)
		htt_tx_ring_pkt_type_set(*msg_word, ENABLE_FLAGS,
					 CTRL, 2);

	if (htt_tlv_filter->data_filter)
		htt_tx_ring_pkt_type_set(*msg_word, ENABLE_FLAGS,
					 DATA, 4);

	if (htt_tlv_filter->mgmt_dma_length)
		HTT_TX_MONITOR_CFG_CONFIG_LENGTH_MGMT_SET(*msg_word,
							  htt_tlv_filter->mgmt_dma_length);

	if (htt_tlv_filter->ctrl_dma_length)
		HTT_TX_MONITOR_CFG_CONFIG_LENGTH_CTRL_SET(*msg_word,
							  htt_tlv_filter->ctrl_dma_length);

	if (htt_tlv_filter->data_dma_length)
		HTT_TX_MONITOR_CFG_CONFIG_LENGTH_DATA_SET(*msg_word,
							  htt_tlv_filter->data_dma_length);

	/* word 2*/
	msg_word++;
	*msg_word = 0;
	if (htt_tlv_filter->mgmt_filter)
		HTT_TX_MONITOR_CFG_PKT_TYPE_ENABLE_FLAGS_SET(*msg_word, 1);

	if (htt_tlv_filter->ctrl_filter)
		HTT_TX_MONITOR_CFG_PKT_TYPE_ENABLE_FLAGS_SET(*msg_word, 2);

	if (htt_tlv_filter->data_filter)
		HTT_TX_MONITOR_CFG_PKT_TYPE_ENABLE_FLAGS_SET(*msg_word, 4);

	if (htt_tlv_filter->mgmt_mpdu_start)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MPDU_START_MGMT_SET(*msg_word, 1);

	if (htt_tlv_filter->ctrl_mpdu_start)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MPDU_START_CTRL_SET(*msg_word, 1);

	if (htt_tlv_filter->data_mpdu_start)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MPDU_START_DATA_SET(*msg_word, 1);

	if (htt_tlv_filter->mgmt_msdu_start)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MSDU_START_MGMT_SET(*msg_word, 1);

	if (htt_tlv_filter->ctrl_msdu_start)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MSDU_START_CTRL_SET(*msg_word, 1);

	if (htt_tlv_filter->data_msdu_start)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MSDU_START_DATA_SET(*msg_word, 1);

	if (htt_tlv_filter->mgmt_mpdu_end)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MPDU_END_MGMT_SET(*msg_word, 1);

	if (htt_tlv_filter->ctrl_mpdu_end)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MPDU_END_CTRL_SET(*msg_word, 1);

	if (htt_tlv_filter->data_mpdu_end)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MPDU_END_DATA_SET(*msg_word, 1);

	if (htt_tlv_filter->mgmt_msdu_end)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MSDU_END_MGMT_SET(*msg_word, 1);

	if (htt_tlv_filter->ctrl_msdu_end)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MSDU_END_CTRL_SET(*msg_word, 1);

	if (htt_tlv_filter->data_msdu_end)
		HTT_TX_MONITOR_CFG_FILTER_IN_TX_MSDU_END_DATA_SET(*msg_word, 1);

	/* word 3 */
	msg_word++;
	*msg_word = 0;

	htt_tx_tlv_filter_mask_set_in0(msg_word, htt_tlv_filter);

	/* word 4 */
	msg_word++;
	*msg_word = 0;

	htt_tx_tlv_filter_mask_set_in1(msg_word, htt_tlv_filter);

	/* word 5 */
	msg_word++;
	*msg_word = 0;

	htt_tx_tlv_filter_mask_set_in2(msg_word, htt_tlv_filter);

	/* word 6 */
	msg_word++;
	*msg_word = 0;

	htt_tx_tlv_filter_mask_set_in3(msg_word, htt_tlv_filter);

	/* word 7 */
	msg_word++;
	*msg_word = 0;
	if (htt_tlv_filter->wmask.tx_fes_setup)
		HTT_TX_MONITOR_CFG_TX_FES_SETUP_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.tx_fes_setup);

	if (htt_tlv_filter->wmask.tx_peer_entry)
		HTT_TX_MONITOR_CFG_TX_PEER_ENTRY_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.tx_peer_entry);

	if (htt_tlv_filter->wmask.tx_queue_ext)
		HTT_TX_MONITOR_CFG_TX_QUEUE_EXT_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.tx_queue_ext);

	if (htt_tlv_filter->wmask.tx_msdu_start)
		HTT_TX_MONITOR_CFG_TX_MSDU_START_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.tx_msdu_start);

	/* word 8 */
	msg_word++;
	*msg_word = 0;
	if (htt_tlv_filter->wmask.pcu_ppdu_setup_init)
		HTT_TX_MONITOR_CFG_PCU_PPDU_SETUP_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.pcu_ppdu_setup_init);

	/* word 9 */
	msg_word++;
	*msg_word = 0;

	if (htt_tlv_filter->wmask.tx_mpdu_start)
		HTT_TX_MONITOR_CFG_TX_MPDU_START_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.tx_mpdu_start);

	if (htt_tlv_filter->wmask.rxpcu_user_setup)
		HTT_TX_MONITOR_CFG_RXPCU_USER_SETUP_WORD_MASK_SET(*msg_word,
					htt_tlv_filter->wmask.rxpcu_user_setup);

	htt_tx_ring_pkt_type_set(*msg_word, ENABLE_MSDU_OR_MPDU_LOGGING,
				 MGMT,
				 htt_tlv_filter->mgmt_mpdu_log);

	htt_tx_ring_pkt_type_set(*msg_word, ENABLE_MSDU_OR_MPDU_LOGGING,
				 CTRL,
				 htt_tlv_filter->ctrl_mpdu_log);

	htt_tx_ring_pkt_type_set(*msg_word, ENABLE_MSDU_OR_MPDU_LOGGING,
				 DATA,
				 htt_tlv_filter->data_mpdu_log);

	HTT_TX_MONITOR_CFG_DMA_MPDU_MGMT_SET(*msg_word,
					     htt_tlv_filter->mgmt_mpdu_log);
	HTT_TX_MONITOR_CFG_DMA_MPDU_CTRL_SET(*msg_word,
					     htt_tlv_filter->ctrl_mpdu_log);
	HTT_TX_MONITOR_CFG_DMA_MPDU_DATA_SET(*msg_word,
					     htt_tlv_filter->data_mpdu_log);

	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt)
		goto fail1;

	pkt->soc_ctxt = NULL; /* not used during send-done callback */

	SET_HTC_PACKET_INFO_TX(
		&pkt->htc_pkt,
		dp_htt_h2t_send_complete_free_netbuf,
		qdf_nbuf_data(htt_msg),
		qdf_nbuf_len(htt_msg),
		soc->htc_endpoint,
		HTC_TX_PACKET_TAG_RUNTIME_PUT); /* tag for no FW response msg */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, htt_msg);
	status = DP_HTT_SEND_HTC_PKT(soc, pkt,
				     HTT_H2T_MSG_TYPE_TX_MONITOR_CFG,
				     htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(htt_msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;

fail1:
	qdf_nbuf_free(htt_msg);
fail0:
	return QDF_STATUS_E_FAILURE;
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
void dp_mon_filter_setup_enhanced_stats_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_ENHACHED_STATS_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_filter *rx_tlv_filter;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	rx_tlv_filter = &filter.rx_tlv_filter;
	dp_mon_filter_set_status_cmn(mon_pdev, rx_tlv_filter);
	/* Setup the filter */
	rx_tlv_filter->tlv_filter.ppdu_end_user_stats_ext = 0;
	rx_tlv_filter->tlv_filter.enable_mo = 0;
	rx_tlv_filter->tlv_filter.mo_mgmt_filter = 0;
	rx_tlv_filter->tlv_filter.mo_ctrl_filter = 0;
	rx_tlv_filter->tlv_filter.mo_data_filter = 0;
	rx_tlv_filter->tlv_filter.ppdu_start_user_info = 1;
	/* Enabled the filter */
	rx_tlv_filter->valid = true;

	dp_mon_filter_show_rx_filter_be(mode, &filter);

	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_reset_enhanced_stats_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_ENHACHED_STATS_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	mon_pdev_be->filter_be[mode][srng_type] = filter;
}
#endif /* QCA_ENHANCED_STATS_SUPPORT */

#ifdef QCA_UNDECODED_METADATA_SUPPORT
void
dp_mon_filter_setup_undecoded_metadata_capture_2_0(struct dp_pdev *pdev)
{
}

void
dp_mon_filter_reset_undecoded_metadata_capture_2_0(struct dp_pdev *pdev)
{
}
#endif

void dp_tx_mon_filter_set_downstream_tlvs(struct htt_tx_ring_tlv_filter *filter)
{
	filter->dtlvs.tx_fes_setup = 1;
	filter->dtlvs.pcu_ppdu_setup_init = 1;
	filter->dtlvs.tx_peer_entry = 1;
	filter->dtlvs.tx_queue_extension = 1;
	filter->dtlvs.fw2s_mon = 1;
}

void dp_tx_mon_filter_set_upstream_tlvs(struct htt_tx_ring_tlv_filter *filter)
{
	filter->utlvs.tx_fes_status_end = 1;
	filter->utlvs.rx_response_required_info = 1;
	filter->utlvs.response_end_status = 1;
	filter->utlvs.tx_fes_status_start = 1;
	filter->utlvs.tx_fes_status_start_prot = 1;
	filter->utlvs.tx_fes_status_prot = 1;
	filter->utlvs.tx_fes_status_start_ppdu = 1;
	filter->utlvs.tx_fes_status_user_ppdu = 1;
	filter->utlvs.coex_tx_status = 1;
	filter->utlvs.rx_frame_bitmap_ack = 1;
	filter->utlvs.rx_frame_1k_bitmap_ack = 1;
	filter->utlvs.he_sig_a_su = 1;
	filter->utlvs.he_sig_a_mu_dl = 1;
	filter->utlvs.he_sig_b1_mu = 1;
	filter->utlvs.he_sig_b2_mu = 1;
	filter->utlvs.he_sig_b2_ofdma = 1;
	filter->utlvs.l_sig_b = 1;
	filter->utlvs.l_sig_a = 1;
	filter->utlvs.ht_sig = 1;
	filter->utlvs.vht_sig_a = 1;
	filter->utlvs.mactx_phy_desc = 1;
	filter->utlvs.mactx_user_desc_cmn = 1;
	filter->utlvs.mactx_user_desc_per_usr = 1;
}

void dp_tx_mon_filter_set_word_mask(struct htt_tx_ring_tlv_filter *filter)
{
	filter->wmask.tx_fes_setup = 1;
	filter->wmask.tx_peer_entry = 1;
	filter->wmask.tx_queue_ext = 1;
	filter->wmask.tx_msdu_start = 1;
	filter->wmask.tx_mpdu_start = 1;
	filter->wmask.pcu_ppdu_setup_init = 1;
	filter->wmask.rxpcu_user_setup = 1;
}

void dp_tx_mon_filter_set_all(struct dp_mon_pdev_be *mon_pdev_be,
			      struct htt_tx_ring_tlv_filter *filter)
{
	qdf_mem_zero(&filter->dtlvs,
		     sizeof(filter->dtlvs));
	qdf_mem_zero(&filter->utlvs,
		     sizeof(filter->utlvs));
	qdf_mem_zero(&filter->wmask,
		     sizeof(filter->wmask));

	dp_tx_mon_filter_set_downstream_tlvs(filter);
	dp_tx_mon_filter_set_upstream_tlvs(filter);
	dp_tx_mon_filter_set_word_mask(filter);

	filter->mgmt_filter = 0x1;
	filter->data_filter = 0x1;
	filter->ctrl_filter = 0x1;

	filter->mgmt_mpdu_end = 1;
	filter->mgmt_msdu_end = 1;
	filter->mgmt_msdu_start = 1;
	filter->mgmt_mpdu_start = 1;
	filter->ctrl_mpdu_end = 1;
	filter->ctrl_msdu_end = 1;
	filter->ctrl_msdu_start = 1;
	filter->ctrl_mpdu_start = 1;
	filter->data_mpdu_end = 1;
	filter->data_msdu_end = 1;
	filter->data_msdu_start = 1;
	filter->data_mpdu_start = 1;
	filter->mgmt_mpdu_log = 1;
	filter->ctrl_mpdu_log = 1;
	filter->data_mpdu_log = 1;

	filter->mgmt_dma_length = mon_pdev_be->tx_mon_filter_length;
	filter->ctrl_dma_length = mon_pdev_be->tx_mon_filter_length;
	filter->data_dma_length = mon_pdev_be->tx_mon_filter_length;
}

void dp_mon_filter_setup_tx_mon_mode_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	filter.tx_valid = !!mon_pdev_be->tx_mon_mode;
	dp_tx_mon_filter_set_all(mon_pdev_be, &filter.tx_tlv_filter);
	dp_mon_filter_show_tx_filter_be(mode, &filter);
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_reset_tx_mon_mode_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_soc *mon_soc;
	struct dp_mon_pdev_be *mon_pdev_be;
	struct dp_mon_soc_be *mon_soc_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	mon_soc = soc->monitor_soc;
	mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	mon_soc_be->tx_mon_ring_fill_level = DP_MON_RING_FILL_LEVEL_DEFAULT;
	mon_soc_be->rx_mon_ring_fill_level = DP_MON_RING_FILL_LEVEL_DEFAULT;

	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

static void dp_mon_filter_set_mon_2_0(struct dp_mon_pdev *mon_pdev,
				      struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 1;
	filter->tlv_filter.packet = 1;
	filter->tlv_filter.packet_header = 1;
	filter->tlv_filter.header_per_msdu = 1;
	filter->tlv_filter.rx_hdr_length = RX_HDR_DMA_LENGTH_64B;
	filter->tlv_filter.msdu_end = 1;
	filter->tlv_filter.mpdu_end = 1;
	filter->tlv_filter.attention = 0;
	filter->tlv_filter.ppdu_start = 1;
	filter->tlv_filter.ppdu_end = 1;
	filter->tlv_filter.ppdu_end_user_stats = 1;
	filter->tlv_filter.ppdu_end_user_stats_ext = 1;
	filter->tlv_filter.ppdu_end_status_done = 1;
	filter->tlv_filter.ppdu_start_user_info = 1;
	filter->tlv_filter.enable_fp =
		(mon_pdev->mon_filter_mode & MON_FILTER_PASS) ? 1 : 0;
	filter->tlv_filter.enable_mo =
		(mon_pdev->mon_filter_mode & MON_FILTER_OTHER) ? 1 : 0;
	filter->tlv_filter.fp_mgmt_filter = mon_pdev->fp_mgmt_filter;
	filter->tlv_filter.fp_ctrl_filter = mon_pdev->fp_ctrl_filter;
	filter->tlv_filter.fp_data_filter = mon_pdev->fp_data_filter;
	filter->tlv_filter.mo_mgmt_filter = mon_pdev->mo_mgmt_filter;
	filter->tlv_filter.mo_ctrl_filter = mon_pdev->mo_ctrl_filter;
	filter->tlv_filter.mo_data_filter = mon_pdev->mo_data_filter;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.enable_fpmo = 0;
	filter->tlv_filter.offset_valid = false;
	filter->tlv_filter.mgmt_dma_length = DEFAULT_DMA_LENGTH;
	filter->tlv_filter.data_dma_length = DEFAULT_DMA_LENGTH;
	filter->tlv_filter.ctrl_dma_length = DEFAULT_DMA_LENGTH;
	 /* compute offset size in QWORDS */
	filter->tlv_filter.rx_pkt_tlv_offset = DP_RX_MON_PACKET_OFFSET / 8;
	filter->tlv_filter.mgmt_mpdu_log = DP_MON_MSDU_LOGGING;
	filter->tlv_filter.ctrl_mpdu_log = DP_MON_MSDU_LOGGING;
	filter->tlv_filter.data_mpdu_log = DP_MON_MSDU_LOGGING;


	if (mon_pdev->mon_filter_mode & MON_FILTER_OTHER) {
		filter->tlv_filter.enable_mo = 1;
		filter->tlv_filter.mo_mgmt_filter = FILTER_MGMT_ALL;
		filter->tlv_filter.mo_ctrl_filter = FILTER_CTRL_ALL;
		filter->tlv_filter.mo_data_filter = FILTER_DATA_ALL;
	} else {
		filter->tlv_filter.enable_mo = 0;
	}
}

void dp_mon_filter_setup_rx_mon_mode_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	struct dp_mon_filter *rx_tlv_filter;
	struct dp_soc *soc;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("mon_pdev Context is null");
		return;
	}
	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	rx_tlv_filter = &filter.rx_tlv_filter;
	rx_tlv_filter->valid = true;

	dp_mon_filter_set_mon_2_0(mon_pdev, rx_tlv_filter);
	dp_mon_filter_show_rx_filter_be(mode, &filter);

	/* Store the above filter */
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_reset_rx_mon_mode_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	struct dp_mon_filter *rx_tlv_filter;
	struct dp_soc *soc = NULL;

	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("mon_pdev Context is null");
		return;
	}
	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	rx_tlv_filter = &filter.rx_tlv_filter;
	rx_tlv_filter->valid = true;

	qdf_mem_zero(&(filter), sizeof(struct dp_mon_filter));
	/* Store the above filter */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

static void dp_rx_mon_filter_show_filter(struct dp_mon_filter_be *filter)
{
	struct htt_rx_ring_tlv_filter *rx_tlv_filter =
		&filter->rx_tlv_filter.tlv_filter;

	DP_MON_FILTER_PRINT("Enable: %d", rx_tlv_filter->enable);
	DP_MON_FILTER_PRINT("mpdu_start: %d", rx_tlv_filter->mpdu_start);
	DP_MON_FILTER_PRINT("msdu_start: %d", rx_tlv_filter->msdu_start);
	DP_MON_FILTER_PRINT("packet: %d", rx_tlv_filter->packet);
	DP_MON_FILTER_PRINT("msdu_end: %d", rx_tlv_filter->msdu_end);
	DP_MON_FILTER_PRINT("mpdu_end: %d", rx_tlv_filter->mpdu_end);
	DP_MON_FILTER_PRINT("packet_header: %d",
			    rx_tlv_filter->packet_header);
	DP_MON_FILTER_PRINT("attention: %d", rx_tlv_filter->attention);
	DP_MON_FILTER_PRINT("ppdu_start: %d", rx_tlv_filter->ppdu_start);
	DP_MON_FILTER_PRINT("ppdu_end: %d", rx_tlv_filter->ppdu_end);
	DP_MON_FILTER_PRINT("ppdu_end_user_stats: %d",
			    rx_tlv_filter->ppdu_end_user_stats);
	DP_MON_FILTER_PRINT("ppdu_end_user_stats_ext: %d",
			    rx_tlv_filter->ppdu_end_user_stats_ext);
	DP_MON_FILTER_PRINT("ppdu_end_status_done: %d",
			    rx_tlv_filter->ppdu_end_status_done);
	DP_MON_FILTER_PRINT("ppdu_start_user_info: %d",
			    rx_tlv_filter->ppdu_start_user_info);
	DP_MON_FILTER_PRINT("header_per_msdu: %d",
			    rx_tlv_filter->header_per_msdu);
	DP_MON_FILTER_PRINT("enable_fp: %d", rx_tlv_filter->enable_fp);
	DP_MON_FILTER_PRINT("enable_md: %d", rx_tlv_filter->enable_md);
	DP_MON_FILTER_PRINT("enable_mo: %d", rx_tlv_filter->enable_mo);
	DP_MON_FILTER_PRINT("enable_fpmo: %d", rx_tlv_filter->enable_fpmo);
	DP_MON_FILTER_PRINT("fp_mgmt_filter: 0x%x",
			    rx_tlv_filter->fp_mgmt_filter);
	DP_MON_FILTER_PRINT("mo_mgmt_filter: 0x%x",
			    rx_tlv_filter->mo_mgmt_filter);
	DP_MON_FILTER_PRINT("fp_ctrl_filter: 0x%x",
			    rx_tlv_filter->fp_ctrl_filter);
	DP_MON_FILTER_PRINT("mo_ctrl_filter: 0x%x",
			    rx_tlv_filter->mo_ctrl_filter);
	DP_MON_FILTER_PRINT("fp_data_filter: 0x%x",
			    rx_tlv_filter->fp_data_filter);
	DP_MON_FILTER_PRINT("mo_data_filter: 0x%x",
			    rx_tlv_filter->mo_data_filter);
	DP_MON_FILTER_PRINT("md_data_filter: 0x%x",
			    rx_tlv_filter->md_data_filter);
	DP_MON_FILTER_PRINT("md_mgmt_filter: 0x%x",
			    rx_tlv_filter->md_mgmt_filter);
	DP_MON_FILTER_PRINT("md_ctrl_filter: 0x%x",
			    rx_tlv_filter->md_ctrl_filter);
	DP_MON_FILTER_PRINT("fpmo_data_filter: 0x%x",
			    rx_tlv_filter->fpmo_data_filter);
	DP_MON_FILTER_PRINT("fpmo_mgmt_filter: 0x%x",
			    rx_tlv_filter->fpmo_mgmt_filter);
	DP_MON_FILTER_PRINT("fpmo_ctrl_filter: 0x%x",
			    rx_tlv_filter->fpmo_ctrl_filter);
	DP_MON_FILTER_PRINT("mgmt_dma_length: %d",
			    rx_tlv_filter->mgmt_dma_length);
	DP_MON_FILTER_PRINT("ctrl_dma_length: %d",
			    rx_tlv_filter->ctrl_dma_length);
	DP_MON_FILTER_PRINT("data_dma_length: %d",
			    rx_tlv_filter->data_dma_length);
	DP_MON_FILTER_PRINT("rx_mpdu_start_wmask: 0x%x",
			    rx_tlv_filter->rx_mpdu_start_wmask);
	DP_MON_FILTER_PRINT("rx_msdu_end_wmask: 0x%x",
			    rx_tlv_filter->rx_msdu_end_wmask);
	DP_MON_FILTER_PRINT("rx_hdr_length: %d",
			    rx_tlv_filter->rx_hdr_length);
	DP_MON_FILTER_PRINT("mgmt_mpdu_log: 0x%x",
			    rx_tlv_filter->mgmt_mpdu_log);
	DP_MON_FILTER_PRINT("data_mpdu_log: 0x%x",
			    rx_tlv_filter->data_mpdu_log);
	DP_MON_FILTER_PRINT("ctrl_mpdu_log: 0x%x",
			    rx_tlv_filter->ctrl_mpdu_log);
	DP_MON_FILTER_PRINT("mgmt_dma_length: 0x%x",
			    rx_tlv_filter->mgmt_dma_length);
	DP_MON_FILTER_PRINT("data_dma_length: 0x%x",
			    rx_tlv_filter->data_dma_length);
	DP_MON_FILTER_PRINT("ctrl_dma_length: 0x%x",
			    rx_tlv_filter->ctrl_dma_length);
}

static void dp_tx_mon_filter_show_filter(struct dp_mon_filter_be *filter)
{
	struct htt_tx_ring_tlv_filter *tlv_filter = &filter->tx_tlv_filter;

	DP_MON_FILTER_PRINT("TX Monitor Filter configuration:");
	DP_MON_FILTER_PRINT("Enable: %d", tlv_filter->enable);
	DP_MON_FILTER_PRINT("mgmt_filter: %d", tlv_filter->mgmt_filter);
	DP_MON_FILTER_PRINT("data_filter: %d", tlv_filter->data_filter);
	DP_MON_FILTER_PRINT("ctrl_filter: %d", tlv_filter->ctrl_filter);
	DP_MON_FILTER_PRINT("mgmt_dma_length: %d", tlv_filter->mgmt_dma_length);
	DP_MON_FILTER_PRINT("ctrl_dma_length: %d", tlv_filter->ctrl_dma_length);
	DP_MON_FILTER_PRINT("data_dma_length: %d", tlv_filter->data_dma_length);
	DP_MON_FILTER_PRINT("mgmt_mpdu_end: %d", tlv_filter->mgmt_mpdu_end);
	DP_MON_FILTER_PRINT("mgmt_msdu_end: %d", tlv_filter->mgmt_msdu_end);
	DP_MON_FILTER_PRINT("mgmt_mpdu_start: %d", tlv_filter->mgmt_mpdu_start);
	DP_MON_FILTER_PRINT("mgmt_msdu_start: %d", tlv_filter->mgmt_msdu_start);
	DP_MON_FILTER_PRINT("ctrl_mpdu_end: %d", tlv_filter->ctrl_mpdu_end);
	DP_MON_FILTER_PRINT("ctrl_msdu_end: %d", tlv_filter->ctrl_msdu_end);
	DP_MON_FILTER_PRINT("ctrl_mpdu_start: %d", tlv_filter->ctrl_mpdu_start);
	DP_MON_FILTER_PRINT("ctrl_msdu_start: %d", tlv_filter->ctrl_msdu_start);
	DP_MON_FILTER_PRINT("data_mpdu_end: %d", tlv_filter->data_mpdu_end);
	DP_MON_FILTER_PRINT("data_msdu_end: %d", tlv_filter->data_msdu_end);
	DP_MON_FILTER_PRINT("data_mpdu_start: %d", tlv_filter->data_mpdu_start);
	DP_MON_FILTER_PRINT("data_msdu_start: %d", tlv_filter->data_msdu_start);
	DP_MON_FILTER_PRINT("mgmt_mpdu_log: %d", tlv_filter->mgmt_mpdu_log);
	DP_MON_FILTER_PRINT("ctrl_mpdu_log: %d", tlv_filter->ctrl_mpdu_log);
	DP_MON_FILTER_PRINT("data_mpdu_log: %d", tlv_filter->data_mpdu_log);

	/* Downstream TLVs */
	DP_MON_FILTER_PRINT("Downstream TLVs");
	DP_MON_FILTER_PRINT("tx_fes_setup: %d", tlv_filter->dtlvs.tx_fes_setup);
	DP_MON_FILTER_PRINT("tx_peer_entry: %d",
			    tlv_filter->dtlvs.tx_peer_entry);
	DP_MON_FILTER_PRINT("tx_queue_extension: %d",
			    tlv_filter->dtlvs.tx_queue_extension);
	DP_MON_FILTER_PRINT("tx_last_mpdu_fetched: %d",
			    tlv_filter->dtlvs.tx_last_mpdu_fetched);
	DP_MON_FILTER_PRINT("tx_data_sync: %d", tlv_filter->dtlvs.tx_data_sync);
	DP_MON_FILTER_PRINT("pcu_ppdu_setup_init: %d",
			    tlv_filter->dtlvs.pcu_ppdu_setup_init);
	DP_MON_FILTER_PRINT("fw2s_mon: %d", tlv_filter->dtlvs.fw2s_mon);
	DP_MON_FILTER_PRINT("tx_loopback_setup: %d",
			    tlv_filter->dtlvs.tx_loopback_setup);
	DP_MON_FILTER_PRINT("sch_critical_tlv_ref: %d",
			    tlv_filter->dtlvs.sch_critical_tlv_ref);
	DP_MON_FILTER_PRINT("ndp_preamble_done: %d",
			    tlv_filter->dtlvs.ndp_preamble_done);
	DP_MON_FILTER_PRINT("tx_raw_frame_setup: %d",
			    tlv_filter->dtlvs.tx_raw_frame_setup);
	DP_MON_FILTER_PRINT("txpcu_user_setup: %d",
			    tlv_filter->dtlvs.txpcu_user_setup);
	DP_MON_FILTER_PRINT("rxpcu_setup: %d", tlv_filter->dtlvs.rxpcu_setup);
	DP_MON_FILTER_PRINT("rxpcu_setup_complete: %d",
			    tlv_filter->dtlvs.rxpcu_setup_complete);
	DP_MON_FILTER_PRINT("coex_tx_req: %d", tlv_filter->dtlvs.coex_tx_req);
	DP_MON_FILTER_PRINT("rxpcu_user_setup: %d",
			    tlv_filter->dtlvs.rxpcu_user_setup);
	DP_MON_FILTER_PRINT("rxpcu_user_setup_ext: %d",
			    tlv_filter->dtlvs.rxpcu_user_setup_ext);
	DP_MON_FILTER_PRINT("wur_data: %d", tlv_filter->dtlvs.wur_data);
	DP_MON_FILTER_PRINT("tqm_mpdu_global_start: %d",
			    tlv_filter->dtlvs.tqm_mpdu_global_start);
	DP_MON_FILTER_PRINT("tx_fes_setup_complete: %d",
			    tlv_filter->dtlvs.tx_fes_setup_complete);
	DP_MON_FILTER_PRINT("scheduler_end: %d",
			    tlv_filter->dtlvs.scheduler_end);
	DP_MON_FILTER_PRINT("sch_wait_instr_tx_path: %d",
			    tlv_filter->dtlvs.sch_wait_instr_tx_path);

	/* Upstream TLVs */
	DP_MON_FILTER_PRINT("Upstream TLVs");
	DP_MON_FILTER_PRINT("rx_response_required_info: %d",
			    tlv_filter->utlvs.rx_response_required_info);
	DP_MON_FILTER_PRINT("response_start_status: %d",
			    tlv_filter->utlvs.response_start_status);
	DP_MON_FILTER_PRINT("response_end_status: %d",
			    tlv_filter->utlvs.response_end_status);
	DP_MON_FILTER_PRINT("tx_fes_status_start: %d",
			    tlv_filter->utlvs.tx_fes_status_start);
	DP_MON_FILTER_PRINT("tx_fes_status_end: %d",
			    tlv_filter->utlvs.tx_fes_status_end);
	DP_MON_FILTER_PRINT("tx_fes_status_start_ppdu: %d",
			    tlv_filter->utlvs.tx_fes_status_start_ppdu);
	DP_MON_FILTER_PRINT("tx_fes_status_user_ppdu: %d",
			    tlv_filter->utlvs.tx_fes_status_user_ppdu);
	DP_MON_FILTER_PRINT("tx_fes_status_ack_or_ba: %d",
			    tlv_filter->utlvs.tx_fes_status_ack_or_ba);
	DP_MON_FILTER_PRINT("tx_fes_status_1k_ba: %d",
			    tlv_filter->utlvs.tx_fes_status_1k_ba);
	DP_MON_FILTER_PRINT("tx_fes_status_start_prot: %d",
			    tlv_filter->utlvs.tx_fes_status_start_prot);
	DP_MON_FILTER_PRINT("tx_fes_status_prot: %d",
			    tlv_filter->utlvs.tx_fes_status_prot);
	DP_MON_FILTER_PRINT("tx_fes_status_user_response: %d",
			    tlv_filter->utlvs.tx_fes_status_user_response);
	DP_MON_FILTER_PRINT("rx_frame_bitmap_ack: %d",
			    tlv_filter->utlvs.rx_frame_bitmap_ack);
	DP_MON_FILTER_PRINT("rx_frame_1k_bitmap_ack: %d",
			    tlv_filter->utlvs.rx_frame_1k_bitmap_ack);
	DP_MON_FILTER_PRINT("coex_tx_status: %d",
			    tlv_filter->utlvs.coex_tx_status);
	DP_MON_FILTER_PRINT("received_response_info: %d",
			    tlv_filter->utlvs.received_response_info);
	DP_MON_FILTER_PRINT("received_response_info_p2: %d",
			    tlv_filter->utlvs.received_response_info_p2);
	DP_MON_FILTER_PRINT("ofdma_trigger_details: %d",
			    tlv_filter->utlvs.ofdma_trigger_details);
	DP_MON_FILTER_PRINT("received_trigger_info: %d",
			    tlv_filter->utlvs.received_trigger_info);
	DP_MON_FILTER_PRINT("pdg_tx_request: %d",
			    tlv_filter->utlvs.pdg_tx_request);
	DP_MON_FILTER_PRINT("pdg_response: %d",
			    tlv_filter->utlvs.pdg_response);
	DP_MON_FILTER_PRINT("pdg_trig_response: %d",
			    tlv_filter->utlvs.pdg_trig_response);
	DP_MON_FILTER_PRINT("trigger_response_tx_done: %d",
			    tlv_filter->utlvs.trigger_response_tx_done);
	DP_MON_FILTER_PRINT("prot_tx_end: %d", tlv_filter->utlvs.prot_tx_end);
	DP_MON_FILTER_PRINT("ppdu_tx_end: %d", tlv_filter->utlvs.ppdu_tx_end);
	DP_MON_FILTER_PRINT("r2r_status_end: %d",
			    tlv_filter->utlvs.r2r_status_end);
	DP_MON_FILTER_PRINT("flush_req: %d", tlv_filter->utlvs.flush_req);
	DP_MON_FILTER_PRINT("mactx_phy_desc: %d",
			    tlv_filter->utlvs.mactx_phy_desc);
	DP_MON_FILTER_PRINT("mactx_user_desc_cmn: %d",
			    tlv_filter->utlvs.mactx_user_desc_cmn);
	DP_MON_FILTER_PRINT("mactx_user_desc_per_usr: %d",
			    tlv_filter->utlvs.mactx_user_desc_per_usr);

	DP_MON_FILTER_PRINT("tqm_acked_1k_mpdu: %d",
			    tlv_filter->utlvs.tqm_acked_1k_mpdu);
	DP_MON_FILTER_PRINT("tqm_acked_mpdu: %d",
			    tlv_filter->utlvs.tqm_acked_mpdu);
	DP_MON_FILTER_PRINT("tqm_update_tx_mpdu_count: %d",
			    tlv_filter->utlvs.tqm_update_tx_mpdu_count);
	DP_MON_FILTER_PRINT("phytx_ppdu_header_info_request: %d",
			    tlv_filter->utlvs.phytx_ppdu_header_info_request);
	DP_MON_FILTER_PRINT("u_sig_eht_su_mu: %d",
			    tlv_filter->utlvs.u_sig_eht_su_mu);
	DP_MON_FILTER_PRINT("u_sig_eht_su: %d", tlv_filter->utlvs.u_sig_eht_su);
	DP_MON_FILTER_PRINT("u_sig_eht_tb: %d", tlv_filter->utlvs.u_sig_eht_tb);
	DP_MON_FILTER_PRINT("eht_sig_usr_su: %d",
			    tlv_filter->utlvs.eht_sig_usr_su);
	DP_MON_FILTER_PRINT("eht_sig_usr_mu_mimo: %d",
			    tlv_filter->utlvs.eht_sig_usr_mu_mimo);
	DP_MON_FILTER_PRINT("eht_sig_usr_ofdma: %d",
			    tlv_filter->utlvs.eht_sig_usr_ofdma);
	DP_MON_FILTER_PRINT("he_sig_a_su: %d",
			    tlv_filter->utlvs.he_sig_a_su);
	DP_MON_FILTER_PRINT("he_sig_a_mu_dl: %d",
			    tlv_filter->utlvs.he_sig_a_mu_dl);
	DP_MON_FILTER_PRINT("he_sig_a_mu_ul: %d",
			    tlv_filter->utlvs.he_sig_a_mu_ul);
	DP_MON_FILTER_PRINT("he_sig_b1_mu: %d",
			    tlv_filter->utlvs.he_sig_b1_mu);
	DP_MON_FILTER_PRINT("he_sig_b2_mu: %d",
			    tlv_filter->utlvs.he_sig_b2_mu);
	DP_MON_FILTER_PRINT("he_sig_b2_ofdma: %d",
			    tlv_filter->utlvs.he_sig_b2_ofdma);
	DP_MON_FILTER_PRINT("vht_sig_b_mu160: %d",
			    tlv_filter->utlvs.vht_sig_b_mu160);
	DP_MON_FILTER_PRINT("vht_sig_b_mu80: %d",
			    tlv_filter->utlvs.vht_sig_b_mu80);
	DP_MON_FILTER_PRINT("vht_sig_b_mu40: %d",
			    tlv_filter->utlvs.vht_sig_b_mu40);
	DP_MON_FILTER_PRINT("vht_sig_b_mu20: %d",
			    tlv_filter->utlvs.vht_sig_b_mu20);
	DP_MON_FILTER_PRINT("vht_sig_b_su160: %d",
			    tlv_filter->utlvs.vht_sig_b_su160);
	DP_MON_FILTER_PRINT("vht_sig_b_su80: %d",
			    tlv_filter->utlvs.vht_sig_b_su80);
	DP_MON_FILTER_PRINT("vht_sig_b_su40: %d",
			    tlv_filter->utlvs.vht_sig_b_su40);
	DP_MON_FILTER_PRINT("vht_sig_b_su20: %d",
			    tlv_filter->utlvs.vht_sig_b_su20);
	DP_MON_FILTER_PRINT("vht_sig_a: %d", tlv_filter->utlvs.vht_sig_a);
	DP_MON_FILTER_PRINT("ht_sig: %d", tlv_filter->utlvs.ht_sig);
	DP_MON_FILTER_PRINT("l_sig_b: %d", tlv_filter->utlvs.l_sig_b);
	DP_MON_FILTER_PRINT("l_sig_a: %d", tlv_filter->utlvs.l_sig_a);
	DP_MON_FILTER_PRINT("tx_service: %d", tlv_filter->utlvs.tx_service);

	DP_MON_FILTER_PRINT("txpcu_buf_status: %d",
			    tlv_filter->utlvs.txpcu_buf_status);
	DP_MON_FILTER_PRINT("txpcu_user_buf_status: %d",
			    tlv_filter->utlvs.txpcu_user_buf_status);
	DP_MON_FILTER_PRINT("txdma_stop_request: %d",
			    tlv_filter->utlvs.txdma_stop_request);
	DP_MON_FILTER_PRINT("expected_response: %d",
			    tlv_filter->utlvs.expected_response);
	DP_MON_FILTER_PRINT("tx_mpdu_count_transfer_end: %d",
			    tlv_filter->utlvs.tx_mpdu_count_transfer_end);
	DP_MON_FILTER_PRINT("rx_trig_info: %d",
			    tlv_filter->utlvs.rx_trig_info);
	DP_MON_FILTER_PRINT("rxpcu_tx_setup_clear: %d",
			    tlv_filter->utlvs.rxpcu_tx_setup_clear);
	DP_MON_FILTER_PRINT("rx_frame_bitmap_req: %d",
			    tlv_filter->utlvs.rx_frame_bitmap_req);
	DP_MON_FILTER_PRINT("rx_phy_sleep: %d",
			    tlv_filter->utlvs.rx_phy_sleep);
	DP_MON_FILTER_PRINT("txpcu_preamble_done: %d",
			    tlv_filter->utlvs.txpcu_preamble_done);
	DP_MON_FILTER_PRINT("txpcu_phytx_debug32: %d",
			    tlv_filter->utlvs.txpcu_phytx_debug32);
	DP_MON_FILTER_PRINT("txpcu_phytx_other_transmit_info32: %d",
			    tlv_filter->utlvs.txpcu_phytx_other_transmit_info32);
	DP_MON_FILTER_PRINT("rx_ppdu_noack_report: %d",
			    tlv_filter->utlvs.rx_ppdu_noack_report);
	DP_MON_FILTER_PRINT("rx_ppdu_ack_report: %d",
			    tlv_filter->utlvs.rx_ppdu_ack_report);
	DP_MON_FILTER_PRINT("coex_rx_status: %d",
			    tlv_filter->utlvs.coex_rx_status);
	DP_MON_FILTER_PRINT("rx_start_param: %d",
			    tlv_filter->utlvs.rx_start_param);
	DP_MON_FILTER_PRINT("tx_cbf_info: %d",
			    tlv_filter->utlvs.tx_cbf_info);
	DP_MON_FILTER_PRINT("rxpcu_early_rx_indication: %d",
			    tlv_filter->utlvs.rxpcu_early_rx_indication);
	DP_MON_FILTER_PRINT("received_response_user_7_0: %d",
			    tlv_filter->utlvs.received_response_user_7_0);
	DP_MON_FILTER_PRINT("received_response_user_15_8: %d",
			    tlv_filter->utlvs.received_response_user_15_8);
	DP_MON_FILTER_PRINT("received_response_user_23_16: %d",
			    tlv_filter->utlvs.received_response_user_23_16);
	DP_MON_FILTER_PRINT("received_response_user_31_24: %d",
			    tlv_filter->utlvs.received_response_user_31_24);
	DP_MON_FILTER_PRINT("received_response_user_36_32: %d",
			    tlv_filter->utlvs.received_response_user_36_32);
	DP_MON_FILTER_PRINT("rx_pm_info: %d",
			    tlv_filter->utlvs.rx_pm_info);
	DP_MON_FILTER_PRINT("rx_preamble: %d",
			    tlv_filter->utlvs.rx_preamble);
	DP_MON_FILTER_PRINT("others: %d",
			    tlv_filter->utlvs.others);
	DP_MON_FILTER_PRINT("mactx_pre_phy_desc: %d",
			    tlv_filter->utlvs.mactx_pre_phy_desc);

	/* Word mask subscription */
	DP_MON_FILTER_PRINT("wmask tx_fes_setup: %d",
			    tlv_filter->wmask.tx_fes_setup);
	DP_MON_FILTER_PRINT("wmask tx_peer_entry: %d",
			    tlv_filter->wmask.tx_peer_entry);
	DP_MON_FILTER_PRINT("wmask tx_queue_ext: %d",
			    tlv_filter->wmask.tx_queue_ext);
	DP_MON_FILTER_PRINT("wmask tx_msdu_start: %d",
			    tlv_filter->wmask.tx_msdu_start);
	DP_MON_FILTER_PRINT("wmask tx_mpdu_start: %d",
			    tlv_filter->wmask.tx_mpdu_start);
	DP_MON_FILTER_PRINT("wmask pcu_ppdu_setup_init: %d",
			    tlv_filter->wmask.pcu_ppdu_setup_init);
	DP_MON_FILTER_PRINT("wmask rxpcu_user_setup: %d",
			    tlv_filter->wmask.rxpcu_user_setup);
}

void dp_mon_filter_show_rx_filter_be(enum dp_mon_filter_mode mode,
				     struct dp_mon_filter_be *filter)
{
	DP_MON_FILTER_PRINT("RX MON RING TLV FILTER CONFIG:");
	DP_MON_FILTER_PRINT("[Mode %d]: Valid: %d",
			    mode, filter->rx_tlv_filter.valid);

	if (filter->rx_tlv_filter.valid)
		dp_rx_mon_filter_show_filter(filter);
}

void dp_mon_filter_show_tx_filter_be(enum dp_mon_filter_mode mode,
				     struct dp_mon_filter_be *filter)
{
	dp_mon_filter_err("TX MON RING TLV FILTER CONFIG:");
	dp_mon_filter_err("[Mode %d]: Valid: %d", mode, filter->tx_valid);

	if (filter->tx_valid)
		dp_tx_mon_filter_show_filter(filter);
}

#ifdef WDI_EVENT_ENABLE
void dp_mon_filter_setup_rx_pkt_log_full_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_FULL_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter =
		&filter.rx_tlv_filter.tlv_filter;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	/* Enabled the filter */
	filter.rx_tlv_filter.valid = true;
	dp_mon_filter_set_status_cmn(&mon_pdev_be->mon_pdev,
				     &filter.rx_tlv_filter);

	/* Setup the filter */
	rx_tlv_filter->packet_header = 1;
	rx_tlv_filter->msdu_start = 1;
	rx_tlv_filter->msdu_end = 1;
	rx_tlv_filter->mpdu_end = 1;

	dp_mon_filter_show_rx_filter_be(mode, &filter);
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_reset_rx_pkt_log_full_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_FULL_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_setup_rx_pkt_log_lite_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_LITE_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	/* Enabled the filter */
	filter.rx_tlv_filter.valid = true;
	dp_mon_filter_set_status_cmn(&mon_pdev_be->mon_pdev,
				     &filter.rx_tlv_filter);

	dp_mon_filter_show_rx_filter_be(mode, &filter);
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_reset_rx_pkt_log_lite_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_LITE_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

#ifdef QCA_MONITOR_PKT_SUPPORT
static void
dp_mon_filter_set_reset_rx_pkt_log_cbf_dest_2_0(struct dp_pdev_be *pdev_be,
						struct dp_mon_filter_be *filter)
{
	struct dp_soc *soc = pdev_be->pdev.soc;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type;
	struct dp_mon_pdev *mon_pdev = pdev_be->pdev.monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct htt_rx_ring_tlv_filter *rx_tlv_filter =
		&filter->rx_tlv_filter.tlv_filter;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
		     DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
		     DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	/*set the filter */
	if (filter->rx_tlv_filter.valid) {
		dp_mon_filter_set_cbf_cmn(&pdev_be->pdev,
					  &filter->rx_tlv_filter);

		rx_tlv_filter->attention = 0;
		dp_mon_filter_show_rx_filter_be(mode, filter);
		mon_pdev_be->filter_be[mode][srng_type] = *filter;
	} else /* reset the filter */
		mon_pdev_be->filter_be[mode][srng_type] = *filter;
}
#else
static void
dp_mon_filter_set_reset_rx_pkt_log_cbf_dest_2_0(struct dp_pdev_be *pdev,
						struct dp_mon_filter_be *filter)
{
}
#endif

void dp_mon_filter_setup_rx_pkt_log_cbf_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;
	struct dp_pdev_be *pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	pdev_be = dp_get_be_pdev_from_dp_pdev(pdev);

	/* Enabled the filter */
	filter.rx_tlv_filter.valid = true;

	dp_mon_filter_set_status_cbf(pdev, &filter.rx_tlv_filter);
	dp_mon_filter_show_rx_filter_be(mode, &filter);
	mon_pdev_be->filter_be[mode][srng_type] = filter;

	/* Clear the filter as the same filter will be used to set the
	 * monitor status ring
	 */
	qdf_mem_zero(&filter, sizeof(struct dp_mon_filter_be));

	filter.rx_tlv_filter.valid = true;
	dp_mon_filter_set_reset_rx_pkt_log_cbf_dest_2_0(pdev_be, &filter);
}

void dp_mon_filter_reset_rx_pktlog_cbf_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;
	struct dp_pdev_be *pdev_be = NULL;

	if (!pdev) {
		QDF_TRACE(QDF_MODULE_ID_MON_FILTER, QDF_TRACE_LEVEL_ERROR,
			  FL("pdev Context is null"));
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	pdev_be = dp_get_be_pdev_from_dp_pdev(pdev);

	/* Enabled the filter */
	filter.rx_tlv_filter.valid = true;

	dp_mon_filter_set_reset_rx_pkt_log_cbf_dest_2_0(pdev_be, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_setup_pktlog_hybrid_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_HYBRID_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;
	struct htt_tx_ring_tlv_filter *tlv_filter = &filter.tx_tlv_filter;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	/* Enabled the filter */
	filter.tx_valid = true;

	/* Setup the filter */
	tlv_filter->utlvs.tx_fes_status_start = 1;
	tlv_filter->utlvs.tx_fes_status_start_prot = 1;
	tlv_filter->utlvs.tx_fes_status_prot = 1;
	tlv_filter->utlvs.tx_fes_status_start_ppdu = 1;
	tlv_filter->utlvs.tx_fes_status_user_ppdu = 1;
	tlv_filter->utlvs.tx_fes_status_ack_or_ba = 1;
	tlv_filter->utlvs.tx_fes_status_1k_ba = 1;
	tlv_filter->utlvs.tx_fes_status_user_response = 1;
	tlv_filter->utlvs.tx_fes_status_end = 1;
	tlv_filter->utlvs.response_start_status = 1;
	tlv_filter->utlvs.received_response_info = 1;
	tlv_filter->utlvs.received_response_info_p2 = 1;
	tlv_filter->utlvs.response_end_status = 1;

	tlv_filter->mgmt_filter = 0x1;
	tlv_filter->data_filter = 0x1;
	tlv_filter->ctrl_filter = 0x1;

	tlv_filter->mgmt_mpdu_end = 1;
	tlv_filter->mgmt_msdu_end = 1;
	tlv_filter->mgmt_msdu_start = 1;
	tlv_filter->mgmt_mpdu_start = 1;
	tlv_filter->ctrl_mpdu_end = 1;
	tlv_filter->ctrl_msdu_end = 1;
	tlv_filter->ctrl_msdu_start = 1;
	tlv_filter->ctrl_mpdu_start = 1;
	tlv_filter->data_mpdu_end = 1;
	tlv_filter->data_msdu_end = 1;
	tlv_filter->data_msdu_start = 1;
	tlv_filter->data_mpdu_start = 1;
	tlv_filter->mgmt_mpdu_log = 1;
	tlv_filter->ctrl_mpdu_log = 1;
	tlv_filter->data_mpdu_log = 1;

	tlv_filter->mgmt_dma_length = mon_pdev_be->tx_mon_filter_length;
	tlv_filter->ctrl_dma_length = mon_pdev_be->tx_mon_filter_length;
	tlv_filter->data_dma_length = mon_pdev_be->tx_mon_filter_length;
	dp_mon_filter_show_tx_filter_be(mode, &filter);
	mon_pdev_be->filter_be[mode][srng_type] = filter;
}

void dp_mon_filter_reset_pktlog_hybrid_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_HYBRID_MODE;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;
	struct dp_mon_pdev *mon_pdev = NULL;
	struct dp_mon_pdev_be *mon_pdev_be = NULL;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev context is null");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	mon_pdev_be->filter_be[mode][srng_type] = filter;
}
#endif /* WDI_EVENT_ENABLE */

/**
 * dp_rx_mon_filter_h2t_setup() - Setup the filter for the Target setup
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 * @srng_type: The srng type for which filter will be set
 * @filter: tlv filter
 */
static void
dp_rx_mon_filter_h2t_setup(struct dp_soc *soc, struct dp_pdev *pdev,
			   enum dp_mon_filter_srng_type srng_type,
			   struct dp_mon_filter *filter)
{
	int32_t current_mode = 0;
	struct htt_rx_ring_tlv_filter *tlv_filter = &filter->tlv_filter;
	struct htt_rx_ring_tlv_filter *src_tlv_filter;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_mon_filter_be *mon_filter;
	uint32_t src_filter = 0, dst_filter = 0;

	/*
	 * Loop through all the modes.
	 */
	for (current_mode = 0; current_mode < DP_MON_FILTER_MAX_MODE;
	     current_mode++) {
		mon_filter =
			&mon_pdev_be->filter_be[current_mode][srng_type];
		src_tlv_filter = &mon_filter->rx_tlv_filter.tlv_filter;

		/*
		 * Check if the correct mode is enabled or not.
		 */
		if (!mon_filter->rx_tlv_filter.valid)
			continue;

		filter->valid = true;

		/*
		 * Set the super bit fields
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_TLV);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_TLV);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_TLV, dst_filter);

		/*
		 * Set the filter management filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_FP_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_FP_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_MGMT, dst_filter);

		/*
		 * Set the monitor other management filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_MO_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_MGMT, dst_filter);

		/*
		 * Set the filter pass control filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_FP_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_FP_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_CTRL, dst_filter);

		/*
		 * Set the monitor other control filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_MO_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_CTRL, dst_filter);

		/*
		 * Set the filter pass data filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_FP_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter,
					       FILTER_FP_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_DATA, dst_filter);

		/*
		 * Set the monitor other data filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_MO_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_DATA, dst_filter);

		/*
		 * Set the monitor direct data filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_MD_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter,
					       FILTER_MD_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter,
				  FILTER_MD_DATA, dst_filter);

		/*
		 * Set the monitor direct management filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_MD_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MD_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MD_MGMT, dst_filter);

		/*
		 * Set the monitor direct management filter.
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->rx_tlv_filter.tlv_filter,
					  FILTER_MD_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MD_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MD_CTRL, dst_filter);

		/*
		 * set the dma length for type mgmt
		 */
		if (src_tlv_filter->mgmt_dma_length &&
		    !tlv_filter->mgmt_dma_length)
			tlv_filter->mgmt_dma_length =
				src_tlv_filter->mgmt_dma_length;

		/*
		 * set the dma length for type ctrl
		 */
		if (src_tlv_filter->ctrl_dma_length &&
		    !tlv_filter->ctrl_dma_length)
			tlv_filter->ctrl_dma_length =
				src_tlv_filter->ctrl_dma_length;

		/*
		 * set the dma length for type data
		 */
		if (src_tlv_filter->data_dma_length &&
		    !tlv_filter->data_dma_length)
			tlv_filter->data_dma_length =
				src_tlv_filter->data_dma_length;

		/*
		 * set mpdu logging for type mgmt
		 */
		if (src_tlv_filter->mgmt_mpdu_log &&
		    !tlv_filter->mgmt_mpdu_log)
			tlv_filter->mgmt_mpdu_log =
				src_tlv_filter->mgmt_mpdu_log;

		/*
		 * set mpdu logging for type ctrl
		 */
		if (src_tlv_filter->ctrl_mpdu_log &&
		    !tlv_filter->ctrl_mpdu_log)
			tlv_filter->ctrl_mpdu_log =
				src_tlv_filter->ctrl_mpdu_log;

		/*
		 * set mpdu logging for type data
		 */
		if (src_tlv_filter->data_mpdu_log &&
		    !tlv_filter->data_mpdu_log)
			tlv_filter->data_mpdu_log =
				src_tlv_filter->data_mpdu_log;

		/*
		 * set mpdu start wmask
		 */
		if (src_tlv_filter->rx_mpdu_start_wmask &&
		    !tlv_filter->rx_mpdu_start_wmask)
			tlv_filter->rx_mpdu_start_wmask =
				src_tlv_filter->rx_mpdu_start_wmask;

		/*
		 * set msdu end wmask
		 */
		if (src_tlv_filter->rx_msdu_end_wmask &&
		    !tlv_filter->rx_msdu_end_wmask)
			tlv_filter->rx_msdu_end_wmask =
				src_tlv_filter->rx_msdu_end_wmask;

		/*
		 * set hdr tlv length
		 */
		if (src_tlv_filter->rx_hdr_length &&
		    !tlv_filter->rx_hdr_length)
			tlv_filter->rx_hdr_length =
				src_tlv_filter->rx_hdr_length;

		if (src_tlv_filter->rx_pkt_tlv_offset &&
		    !tlv_filter->rx_pkt_tlv_offset)
			tlv_filter->rx_pkt_tlv_offset =
				src_tlv_filter->rx_pkt_tlv_offset;

		/*
		 * set fpmo filter settings
		 */
		if (src_tlv_filter->enable_fpmo &&
		    !tlv_filter->enable_fpmo) {
			tlv_filter->enable_fpmo =
				src_tlv_filter->enable_fpmo;
			tlv_filter->fpmo_data_filter =
				src_tlv_filter->fpmo_data_filter;
			tlv_filter->fpmo_mgmt_filter =
				src_tlv_filter->fpmo_mgmt_filter;
			tlv_filter->fpmo_ctrl_filter =
				src_tlv_filter->fpmo_ctrl_filter;
		}

		dp_mon_filter_show_rx_filter_be(current_mode, mon_filter);
	}
}

static
void dp_tx_mon_downstream_tlv_set(struct htt_tx_ring_tlv_filter *dst_filter,
				  struct htt_tx_ring_tlv_filter *src_filter)
{
	dst_filter->dtlvs.tx_fes_setup |=
		src_filter->dtlvs.tx_fes_setup;
	dst_filter->dtlvs.tx_peer_entry |=
		src_filter->dtlvs.tx_peer_entry;
	dst_filter->dtlvs.tx_queue_extension |=
		src_filter->dtlvs.tx_queue_extension;
	dst_filter->dtlvs.tx_last_mpdu_end |=
		src_filter->dtlvs.tx_last_mpdu_end;
	dst_filter->dtlvs.tx_last_mpdu_fetched |=
		src_filter->dtlvs.tx_last_mpdu_fetched;
	dst_filter->dtlvs.tx_data_sync |=
		src_filter->dtlvs.tx_data_sync;
	dst_filter->dtlvs.pcu_ppdu_setup_init |=
		src_filter->dtlvs.pcu_ppdu_setup_init;
	dst_filter->dtlvs.fw2s_mon |=
		src_filter->dtlvs.fw2s_mon;
	dst_filter->dtlvs.tx_loopback_setup |=
		src_filter->dtlvs.tx_loopback_setup;
	dst_filter->dtlvs.sch_critical_tlv_ref |=
		src_filter->dtlvs.sch_critical_tlv_ref;
	dst_filter->dtlvs.ndp_preamble_done |=
		src_filter->dtlvs.ndp_preamble_done;
	dst_filter->dtlvs.tx_raw_frame_setup |=
		src_filter->dtlvs.tx_raw_frame_setup;
	dst_filter->dtlvs.txpcu_user_setup |=
		src_filter->dtlvs.txpcu_user_setup;
	dst_filter->dtlvs.rxpcu_setup |=
		src_filter->dtlvs.rxpcu_setup;
	dst_filter->dtlvs.rxpcu_setup_complete |=
		src_filter->dtlvs.rxpcu_setup_complete;
	dst_filter->dtlvs.coex_tx_req |=
		src_filter->dtlvs.coex_tx_req;
	dst_filter->dtlvs.rxpcu_user_setup |=
		src_filter->dtlvs.rxpcu_user_setup;
	dst_filter->dtlvs.rxpcu_user_setup_ext |=
		src_filter->dtlvs.rxpcu_user_setup_ext;
	dst_filter->dtlvs.wur_data |= src_filter->dtlvs.wur_data;
	dst_filter->dtlvs.tqm_mpdu_global_start |=
		src_filter->dtlvs.tqm_mpdu_global_start;
	dst_filter->dtlvs.tx_fes_setup_complete |=
		src_filter->dtlvs.tx_fes_setup_complete;
	dst_filter->dtlvs.scheduler_end |= src_filter->dtlvs.scheduler_end;
	dst_filter->dtlvs.sch_wait_instr_tx_path |=
		src_filter->dtlvs.sch_wait_instr_tx_path;
}

static
void dp_tx_mon_upstream_tlv_set(struct htt_tx_ring_tlv_filter *dst_filter,
				struct htt_tx_ring_tlv_filter *src_filter)
{
	dst_filter->utlvs.rx_response_required_info |=
		src_filter->utlvs.rx_response_required_info;
	dst_filter->utlvs.response_start_status |=
		src_filter->utlvs.response_start_status;
	dst_filter->utlvs.response_end_status |=
		src_filter->utlvs.response_end_status;
	dst_filter->utlvs.tx_fes_status_start |=
		src_filter->utlvs.tx_fes_status_start;
	dst_filter->utlvs.tx_fes_status_end |=
		src_filter->utlvs.tx_fes_status_end;
	dst_filter->utlvs.tx_fes_status_start_ppdu |=
		src_filter->utlvs.tx_fes_status_start_ppdu;
	dst_filter->utlvs.tx_fes_status_user_ppdu |=
		src_filter->utlvs.tx_fes_status_user_ppdu;
	dst_filter->utlvs.tx_fes_status_ack_or_ba |=
		src_filter->utlvs.tx_fes_status_ack_or_ba;
	dst_filter->utlvs.tx_fes_status_1k_ba |=
		src_filter->utlvs.tx_fes_status_1k_ba;
	dst_filter->utlvs.tx_fes_status_start_prot |=
		src_filter->utlvs.tx_fes_status_start_prot;
	dst_filter->utlvs.tx_fes_status_prot |=
		src_filter->utlvs.tx_fes_status_prot;
	dst_filter->utlvs.tx_fes_status_user_response |=
		src_filter->utlvs.tx_fes_status_user_response;
	dst_filter->utlvs.rx_frame_bitmap_ack |=
		src_filter->utlvs.rx_frame_bitmap_ack;
	dst_filter->utlvs.rx_frame_1k_bitmap_ack |=
		src_filter->utlvs.rx_frame_1k_bitmap_ack;
	dst_filter->utlvs.coex_tx_status |=
		src_filter->utlvs.coex_tx_status;
	dst_filter->utlvs.received_response_info |=
		src_filter->utlvs.received_response_info;
	dst_filter->utlvs.received_response_info_p2 |=
		src_filter->utlvs.received_response_info_p2;
	dst_filter->utlvs.ofdma_trigger_details |=
		src_filter->utlvs.ofdma_trigger_details;
	dst_filter->utlvs.received_trigger_info |=
		src_filter->utlvs.received_trigger_info;
	dst_filter->utlvs.pdg_tx_request |=
		src_filter->utlvs.pdg_tx_request;
	dst_filter->utlvs.pdg_response |=
		src_filter->utlvs.pdg_response;
	dst_filter->utlvs.pdg_trig_response |=
		src_filter->utlvs.pdg_trig_response;
	dst_filter->utlvs.trigger_response_tx_done |=
		src_filter->utlvs.trigger_response_tx_done;
	dst_filter->utlvs.prot_tx_end |=
		src_filter->utlvs.prot_tx_end;
	dst_filter->utlvs.ppdu_tx_end |=
		src_filter->utlvs.ppdu_tx_end;
	dst_filter->utlvs.r2r_status_end |=
		src_filter->utlvs.r2r_status_end;
	dst_filter->utlvs.flush_req |=
		src_filter->utlvs.flush_req;
	dst_filter->utlvs.mactx_phy_desc |=
		src_filter->utlvs.mactx_phy_desc;
	dst_filter->utlvs.mactx_user_desc_cmn |=
		src_filter->utlvs.mactx_user_desc_cmn;
	dst_filter->utlvs.mactx_user_desc_per_usr |=
		src_filter->utlvs.mactx_user_desc_per_usr;

	dst_filter->utlvs.tqm_acked_1k_mpdu |=
		src_filter->utlvs.tqm_acked_1k_mpdu;
	dst_filter->utlvs.tqm_acked_mpdu |=
		src_filter->utlvs.tqm_acked_mpdu;
	dst_filter->utlvs.tqm_update_tx_mpdu_count |=
		src_filter->utlvs.tqm_update_tx_mpdu_count;
	dst_filter->utlvs.phytx_ppdu_header_info_request |=
		src_filter->utlvs.phytx_ppdu_header_info_request;
	dst_filter->utlvs.u_sig_eht_su_mu |=
		src_filter->utlvs.u_sig_eht_su_mu;
	dst_filter->utlvs.u_sig_eht_su |=
		src_filter->utlvs.u_sig_eht_su;
	dst_filter->utlvs.u_sig_eht_tb |=
		src_filter->utlvs.u_sig_eht_tb;
	dst_filter->utlvs.eht_sig_usr_su |=
		src_filter->utlvs.eht_sig_usr_su;
	dst_filter->utlvs.eht_sig_usr_mu_mimo |=
		src_filter->utlvs.eht_sig_usr_mu_mimo;
	dst_filter->utlvs.eht_sig_usr_ofdma |=
		src_filter->utlvs.eht_sig_usr_ofdma;
	dst_filter->utlvs.he_sig_a_su |=
		src_filter->utlvs.he_sig_a_su;
	dst_filter->utlvs.he_sig_a_mu_dl |=
		src_filter->utlvs.he_sig_a_mu_dl;
	dst_filter->utlvs.he_sig_a_mu_ul |=
		src_filter->utlvs.he_sig_a_mu_ul;
	dst_filter->utlvs.he_sig_b1_mu |=
		src_filter->utlvs.he_sig_b1_mu;
	dst_filter->utlvs.he_sig_b2_mu |=
		src_filter->utlvs.he_sig_b2_mu;
	dst_filter->utlvs.he_sig_b2_ofdma |=
		src_filter->utlvs.he_sig_b2_ofdma;
	dst_filter->utlvs.vht_sig_b_mu160 |=
		src_filter->utlvs.vht_sig_b_mu160;
	dst_filter->utlvs.vht_sig_b_mu80 |=
		src_filter->utlvs.vht_sig_b_mu80;
	dst_filter->utlvs.vht_sig_b_mu40 |=
		src_filter->utlvs.vht_sig_b_mu40;
	dst_filter->utlvs.vht_sig_b_mu20 |=
		src_filter->utlvs.vht_sig_b_mu20;
	dst_filter->utlvs.vht_sig_b_su160 |=
		src_filter->utlvs.vht_sig_b_su160;
	dst_filter->utlvs.vht_sig_b_su80 |=
		src_filter->utlvs.vht_sig_b_su80;
	dst_filter->utlvs.vht_sig_b_su40 |=
		src_filter->utlvs.vht_sig_b_su40;
	dst_filter->utlvs.vht_sig_b_su20 |=
		src_filter->utlvs.vht_sig_b_su20;
	dst_filter->utlvs.vht_sig_a |=
		src_filter->utlvs.vht_sig_a;
	dst_filter->utlvs.ht_sig |=
		src_filter->utlvs.ht_sig;
	dst_filter->utlvs.l_sig_b |=
		src_filter->utlvs.l_sig_b;
	dst_filter->utlvs.l_sig_a |=
		src_filter->utlvs.l_sig_a;
	dst_filter->utlvs.tx_service |=
		src_filter->utlvs.tx_service;

	dst_filter->utlvs.txpcu_buf_status |=
		src_filter->utlvs.txpcu_buf_status;
	dst_filter->utlvs.txpcu_user_buf_status |=
		src_filter->utlvs.txpcu_user_buf_status;
	dst_filter->utlvs.txdma_stop_request |=
		src_filter->utlvs.txdma_stop_request;
	dst_filter->utlvs.expected_response |=
		src_filter->utlvs.expected_response;
	dst_filter->utlvs.tx_mpdu_count_transfer_end |=
		src_filter->utlvs.tx_mpdu_count_transfer_end;
	dst_filter->utlvs.rx_trig_info |=
		src_filter->utlvs.rx_trig_info;
	dst_filter->utlvs.rxpcu_tx_setup_clear |=
		src_filter->utlvs.rxpcu_tx_setup_clear;
	dst_filter->utlvs.rx_frame_bitmap_req |=
		src_filter->utlvs.rx_frame_bitmap_req;
	dst_filter->utlvs.rx_phy_sleep |=
		src_filter->utlvs.rx_phy_sleep;
	dst_filter->utlvs.txpcu_preamble_done |=
		src_filter->utlvs.txpcu_preamble_done;
	dst_filter->utlvs.txpcu_phytx_debug32 |=
		src_filter->utlvs.txpcu_phytx_debug32;
	dst_filter->utlvs.txpcu_phytx_other_transmit_info32 |=
		src_filter->utlvs.txpcu_phytx_other_transmit_info32;
	dst_filter->utlvs.rx_ppdu_noack_report |=
		src_filter->utlvs.rx_ppdu_noack_report;
	dst_filter->utlvs.rx_ppdu_ack_report |=
		src_filter->utlvs.rx_ppdu_ack_report;
	dst_filter->utlvs.coex_rx_status |=
		src_filter->utlvs.coex_rx_status;
	dst_filter->utlvs.rx_start_param |=
		src_filter->utlvs.rx_start_param;
	dst_filter->utlvs.tx_cbf_info |=
		src_filter->utlvs.tx_cbf_info;
	dst_filter->utlvs.rxpcu_early_rx_indication |=
		src_filter->utlvs.rxpcu_early_rx_indication;
	dst_filter->utlvs.received_response_user_7_0 |=
		src_filter->utlvs.received_response_user_7_0;
	dst_filter->utlvs.received_response_user_15_8 |=
		src_filter->utlvs.received_response_user_15_8;
	dst_filter->utlvs.received_response_user_23_16 |=
		src_filter->utlvs.received_response_user_23_16;
	dst_filter->utlvs.received_response_user_31_24 |=
		src_filter->utlvs.received_response_user_31_24;
	dst_filter->utlvs.received_response_user_36_32 |=
		src_filter->utlvs.received_response_user_36_32;
	dst_filter->utlvs.rx_pm_info |=
		src_filter->utlvs.rx_pm_info;
	dst_filter->utlvs.rx_preamble |=
		src_filter->utlvs.rx_preamble;
	dst_filter->utlvs.others |=
		src_filter->utlvs.others;
	dst_filter->utlvs.mactx_pre_phy_desc |=
		src_filter->utlvs.mactx_pre_phy_desc;
}

static
void dp_tx_mon_wordmask_config_set(struct htt_tx_ring_tlv_filter *dst_filter,
				   struct htt_tx_ring_tlv_filter *src_filter)
{
	dst_filter->wmask.tx_fes_setup |=
		src_filter->wmask.tx_fes_setup;
	dst_filter->wmask.tx_peer_entry |=
		src_filter->wmask.tx_peer_entry;
	dst_filter->wmask.tx_queue_ext |=
		src_filter->wmask.tx_queue_ext;
	dst_filter->wmask.tx_msdu_start |=
		src_filter->wmask.tx_msdu_start;
	dst_filter->wmask.tx_mpdu_start |=
		src_filter->wmask.tx_mpdu_start;
	dst_filter->wmask.pcu_ppdu_setup_init |=
		src_filter->wmask.pcu_ppdu_setup_init;
	dst_filter->wmask.rxpcu_user_setup |=
		src_filter->wmask.rxpcu_user_setup;
}

/**
 * dp_tx_mon_filter_h2t_setup() - Setup the filter
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 * @srng_type: The srng type for which filter will be set
 * @filter: tlv filter
 */
static
void dp_tx_mon_filter_h2t_setup(struct dp_soc *soc, struct dp_pdev *pdev,
				enum dp_mon_filter_srng_type srng_type,
				struct dp_mon_filter_be *filter)
{
	int32_t current_mode = 0;
	struct htt_tx_ring_tlv_filter *dst_filter = &filter->tx_tlv_filter;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	/*
	 * Loop through all the modes.
	 */
	for (current_mode = 0; current_mode < DP_MON_FILTER_MAX_MODE;
	     current_mode++) {
		struct dp_mon_filter_be *mon_filter =
			&mon_pdev_be->filter_be[current_mode][srng_type];
		struct htt_tx_ring_tlv_filter *src_filter =
			&mon_filter->tx_tlv_filter;

		/*
		 * Check if the correct mode is enabled or not.
		 */
		if (!mon_filter->tx_valid)
			continue;

		dst_filter->enable = 1;

		dp_tx_mon_downstream_tlv_set(dst_filter, src_filter);
		dp_tx_mon_upstream_tlv_set(dst_filter, src_filter);
		dp_tx_mon_wordmask_config_set(dst_filter, src_filter);

		dst_filter->mgmt_filter |= src_filter->mgmt_filter;
		dst_filter->data_filter |= src_filter->data_filter;
		dst_filter->ctrl_filter |= src_filter->ctrl_filter;
		dst_filter->mgmt_dma_length |= src_filter->mgmt_dma_length;
		dst_filter->ctrl_dma_length |= src_filter->ctrl_dma_length;
		dst_filter->data_dma_length |= src_filter->data_dma_length;
		dst_filter->mgmt_mpdu_end |= src_filter->mgmt_mpdu_end;
		dst_filter->mgmt_msdu_end |= src_filter->mgmt_msdu_end;
		dst_filter->mgmt_msdu_start |= src_filter->mgmt_msdu_start;
		dst_filter->mgmt_mpdu_start |= src_filter->mgmt_mpdu_start;
		dst_filter->ctrl_mpdu_end |= src_filter->mgmt_mpdu_end;
		dst_filter->ctrl_msdu_end |= src_filter->mgmt_msdu_end;
		dst_filter->ctrl_msdu_start |= src_filter->mgmt_msdu_start;
		dst_filter->ctrl_mpdu_start |= src_filter->mgmt_mpdu_start;
		dst_filter->data_mpdu_end |= src_filter->mgmt_mpdu_end;
		dst_filter->data_msdu_end |= src_filter->mgmt_msdu_end;
		dst_filter->data_msdu_start |= src_filter->mgmt_msdu_start;
		dst_filter->data_mpdu_start |= src_filter->mgmt_mpdu_start;
		dst_filter->mgmt_mpdu_log |= src_filter->mgmt_mpdu_log;
		dst_filter->ctrl_mpdu_log |= src_filter->ctrl_mpdu_log;
		dst_filter->data_mpdu_log |= src_filter->data_mpdu_log;
	}
	DP_MON_FILTER_PRINT("TXMON FINAL FILTER CONFIG:");
	dp_tx_mon_filter_show_filter(filter);
}

static QDF_STATUS
dp_tx_mon_ht2_ring_cfg(struct dp_soc *soc,
		       struct dp_pdev *pdev,
		       enum dp_mon_filter_srng_type srng_type,
		       struct htt_tx_ring_tlv_filter *tlv_filter)
{
	int mac_id;
	int max_mac_rings = wlan_cfg_get_num_mac_rings(pdev->wlan_cfg_ctx);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	dp_mon_filter_info("%pK: srng type %d Max_mac_rings %d ",
			   soc, srng_type, max_mac_rings);

	for (mac_id = 0; mac_id < max_mac_rings; mac_id++) {
		int mac_for_pdev =
			dp_get_mac_id_for_pdev(mac_id, pdev->pdev_id);
		int lmac_id = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev->pdev_id);
		int hal_ring_type, ring_buf_size;
		hal_ring_handle_t hal_ring_hdl;

		hal_ring_hdl =
			mon_soc_be->tx_mon_dst_ring[lmac_id].hal_srng;
		hal_ring_type = TX_MONITOR_DST;
		ring_buf_size = 2048;

		status = htt_h2t_tx_ring_cfg(soc->htt_handle, mac_for_pdev,
					     hal_ring_hdl, hal_ring_type,
					     ring_buf_size,
					     tlv_filter);
		if (status != QDF_STATUS_SUCCESS)
			return status;
	}

	return status;
}

QDF_STATUS dp_tx_mon_filter_update_2_0(struct dp_pdev *pdev)
{
	struct dp_soc *soc;
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return QDF_STATUS_E_FAILURE;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("soc Context is null");
		return QDF_STATUS_E_FAILURE;
	}

	dp_tx_mon_filter_h2t_setup(soc, pdev, srng_type, &filter);
	dp_tx_mon_ht2_ring_cfg(soc, pdev, srng_type,
			       &filter.tx_tlv_filter);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_mon_filter_update_2_0(struct dp_pdev *pdev)
{
	struct dp_soc *soc;
	struct dp_mon_filter_be filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return QDF_STATUS_E_FAILURE;
	}
	soc = pdev->soc;

	rx_tlv_filter = &filter.rx_tlv_filter.tlv_filter;
	dp_rx_mon_filter_h2t_setup(soc, pdev, srng_type, &filter.rx_tlv_filter);
	if (filter.rx_tlv_filter.valid)
		rx_tlv_filter->enable = 1;
	else
		rx_tlv_filter->enable = 0;

	dp_mon_ht2_rx_ring_cfg(soc, pdev, srng_type,
			       &filter.rx_tlv_filter.tlv_filter);
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_LITE_MONITOR
void
dp_mon_filter_reset_rx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode filter_mode =
				DP_MON_FILTER_LITE_MON_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_lite_mon_rx_config *config = NULL;

	be_mon_pdev->filter_be[filter_mode][srng_type] = filter;
	config = be_mon_pdev->lite_mon_rx_config;
	if (config)
		config->fp_type_subtype_filter_all = false;
}

void
dp_mon_filter_setup_rx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev)
{
	struct dp_mon_filter_be filter = {0};
	struct dp_mon_filter *rx_tlv_filter;
	enum dp_mon_filter_mode filter_mode =
				DP_MON_FILTER_LITE_MON_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct dp_lite_mon_rx_config *config = NULL;
	uint16_t max_custom_len = 0;
	uint16_t mgmt_len = 0;
	uint16_t ctrl_len = 0;
	uint16_t data_len = 0;

	config = be_mon_pdev->lite_mon_rx_config;
	if (!config)
		return;

	rx_tlv_filter = &filter.rx_tlv_filter;
	rx_tlv_filter->valid = true;
	/* configure fp filters if enabled */
	if (config->rx_config.fp_enabled) {
		rx_tlv_filter->tlv_filter.enable_fp = 1;
		rx_tlv_filter->tlv_filter.fp_mgmt_filter =
			config->rx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_FP];
		rx_tlv_filter->tlv_filter.fp_ctrl_filter =
			config->rx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_FP];
		rx_tlv_filter->tlv_filter.fp_data_filter =
			config->rx_config.data_filter[DP_MON_FRM_FILTER_MODE_FP];
		if ((config->rx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_FP] ==
		     CDP_LITE_MON_FILTER_ALL) &&
		    (config->rx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_FP] ==
		     CDP_LITE_MON_FILTER_ALL) &&
		    (config->rx_config.data_filter[DP_MON_FRM_FILTER_MODE_FP] ==
		     CDP_LITE_MON_FILTER_ALL))
			config->fp_type_subtype_filter_all = true;
	}

	/* configure md filters if enabled */
	if (config->rx_config.md_enabled) {
		rx_tlv_filter->tlv_filter.enable_md = 1;
		rx_tlv_filter->tlv_filter.md_mgmt_filter =
			config->rx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_MD];
		rx_tlv_filter->tlv_filter.md_ctrl_filter =
			config->rx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_MD];
		rx_tlv_filter->tlv_filter.md_data_filter =
			config->rx_config.data_filter[DP_MON_FRM_FILTER_MODE_MD];
	}

	/* configure mo filters if enabled */
	if (config->rx_config.mo_enabled) {
		rx_tlv_filter->tlv_filter.enable_mo = 1;
		rx_tlv_filter->tlv_filter.mo_mgmt_filter =
			config->rx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_MO];
		rx_tlv_filter->tlv_filter.mo_ctrl_filter =
			config->rx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_MO];
		rx_tlv_filter->tlv_filter.mo_data_filter =
			config->rx_config.data_filter[DP_MON_FRM_FILTER_MODE_MO];
	}

	/* configure fpmo filters if enabled */
	if (config->rx_config.fpmo_enabled) {
		rx_tlv_filter->tlv_filter.enable_fpmo = 1;
		rx_tlv_filter->tlv_filter.fpmo_mgmt_filter =
			config->rx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_FP_MO];
		rx_tlv_filter->tlv_filter.fpmo_ctrl_filter =
			config->rx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_FP_MO];
		rx_tlv_filter->tlv_filter.fpmo_data_filter =
			config->rx_config.data_filter[DP_MON_FRM_FILTER_MODE_FP_MO];
	}

	mgmt_len = config->rx_config.len[WLAN_FC0_TYPE_MGMT];
	ctrl_len = config->rx_config.len[WLAN_FC0_TYPE_CTRL];
	data_len = config->rx_config.len[WLAN_FC0_TYPE_DATA];
	/* if full len is configured for any of the types, subscribe
	 * for full dma length else set it to min dma length(fw sets
	 * full length by default) to avoid unnecessary dma since we
	 * do not have hw support to control rx pkt tlvs per type. To
	 * get custom len pkt we make use of rx hdr tlv instead.
	 */
	if (dp_lite_mon_is_full_len_configured(mgmt_len,
					       ctrl_len,
					       data_len)) {
		rx_tlv_filter->tlv_filter.packet = 1;
		/* get offset size in QWORDS */
		rx_tlv_filter->tlv_filter.rx_pkt_tlv_offset =
				DP_GET_NUM_QWORDS(DP_RX_MON_PACKET_OFFSET);
		if (mgmt_len == CDP_LITE_MON_LEN_FULL)
			rx_tlv_filter->tlv_filter.mgmt_dma_length =
							DEFAULT_DMA_LENGTH;
		else
			rx_tlv_filter->tlv_filter.mgmt_dma_length =
							DMA_LENGTH_64B;

		if (ctrl_len == CDP_LITE_MON_LEN_FULL)
			rx_tlv_filter->tlv_filter.ctrl_dma_length =
							DEFAULT_DMA_LENGTH;
		else
			rx_tlv_filter->tlv_filter.ctrl_dma_length =
							DMA_LENGTH_64B;

		if (data_len == CDP_LITE_MON_LEN_FULL)
			rx_tlv_filter->tlv_filter.data_dma_length =
							DEFAULT_DMA_LENGTH;
		else
			rx_tlv_filter->tlv_filter.data_dma_length =
							DMA_LENGTH_64B;
	} else  {
		/* if full len not configured set to min len */
		rx_tlv_filter->tlv_filter.mgmt_dma_length = DMA_LENGTH_64B;
		rx_tlv_filter->tlv_filter.ctrl_dma_length = DMA_LENGTH_64B;
		rx_tlv_filter->tlv_filter.data_dma_length = DMA_LENGTH_64B;
	}

	rx_tlv_filter->tlv_filter.packet_header = 1;
	/* set rx hdr tlv len, default len is 128B */
	max_custom_len = dp_lite_mon_get_max_custom_len(mgmt_len, ctrl_len,
							data_len);
	if (max_custom_len == CDP_LITE_MON_LEN_64B)
		rx_tlv_filter->tlv_filter.rx_hdr_length =
						RX_HDR_DMA_LENGTH_64B;
	else if (max_custom_len == CDP_LITE_MON_LEN_128B)
		rx_tlv_filter->tlv_filter.rx_hdr_length =
						RX_HDR_DMA_LENGTH_128B;
	else if (max_custom_len == CDP_LITE_MON_LEN_256B)
		rx_tlv_filter->tlv_filter.rx_hdr_length =
						RX_HDR_DMA_LENGTH_256B;

	if ((config->rx_config.level == CDP_LITE_MON_LEVEL_MSDU) ||
	    dp_lite_mon_is_full_len_configured(mgmt_len, ctrl_len, data_len)) {
		rx_tlv_filter->tlv_filter.header_per_msdu = 1;
		rx_tlv_filter->tlv_filter.msdu_end = 1;
	}

	rx_tlv_filter->tlv_filter.ppdu_start = 1;
	rx_tlv_filter->tlv_filter.ppdu_end = 1;
	rx_tlv_filter->tlv_filter.mpdu_start = 1;
	rx_tlv_filter->tlv_filter.mpdu_end = 1;

	rx_tlv_filter->tlv_filter.ppdu_end_user_stats = 1;
	rx_tlv_filter->tlv_filter.ppdu_end_user_stats_ext = 1;
	rx_tlv_filter->tlv_filter.ppdu_end_status_done = 1;
	rx_tlv_filter->tlv_filter.ppdu_start_user_info = 1;

	dp_mon_filter_show_rx_filter_be(filter_mode, &filter);
	be_mon_pdev->filter_be[filter_mode][srng_type] = filter;
}

uint8_t tx_lite_mon_set_len(uint16_t len)
{
	switch (len) {
	case CDP_LITE_MON_LEN_64B:
		return DMA_LENGTH_64B;
	case CDP_LITE_MON_LEN_128B:
		return DMA_LENGTH_128B;
	case CDP_LITE_MON_LEN_256B:
		return DMA_LENGTH_256B;
	case CDP_LITE_MON_LEN_FULL:
		return DEFAULT_DMA_LENGTH;
	default:
		dp_mon_filter_err("Invalid length %d, Using minimal length of 64B",
				  len);
		return DMA_LENGTH_64B;
	}
}

void
dp_mon_filter_reset_tx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode filter_mode =
				DP_MON_FILTER_LITE_MON_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;
	struct dp_lite_mon_tx_config *config = NULL;

	be_mon_pdev->filter_be[filter_mode][srng_type] = filter;
	config = be_mon_pdev->lite_mon_tx_config;
	if (!config)
		return;
	config->subtype_filtering = false;
	config->sw_peer_filtering = false;

}

void
dp_mon_filter_setup_tx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev)
{
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_LITE_MON_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_TXMON_DEST;
	struct htt_tx_ring_tlv_filter *tx_tlv_filter = &filter.tx_tlv_filter;
	struct dp_lite_mon_tx_config *config = NULL;

	config = be_mon_pdev->lite_mon_tx_config;
	if (!config)
		return;

	/* tx monitor supports only filter pass mode */
	if (config->tx_config.md_enabled || config->tx_config.mo_enabled ||
	    config->tx_config.fpmo_enabled) {
		dp_mon_filter_err("md mo and fpmo are invalid filter configuration for Tx");
		return;
	}

	/* Enable tx monitor filter */
	filter.tx_valid = true;
	tx_tlv_filter->enable = 1;

	dp_tx_mon_filter_set_downstream_tlvs(tx_tlv_filter);
	dp_tx_mon_filter_set_upstream_tlvs(tx_tlv_filter);
	dp_tx_mon_filter_set_word_mask(tx_tlv_filter);

	/* configure mgmt filters */
	if (config->tx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_FP]) {
		tx_tlv_filter->mgmt_filter = 1;
		tx_tlv_filter->mgmt_dma_length =
			tx_lite_mon_set_len(config->tx_config.len[WLAN_FC0_TYPE_MGMT]);
		if ((config->tx_config.level == CDP_LITE_MON_LEVEL_MPDU) ||
		    (config->tx_config.level == CDP_LITE_MON_LEVEL_PPDU))
			tx_tlv_filter->mgmt_mpdu_log = 1;
		if (config->tx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_FP] !=
		    CDP_LITE_MON_FILTER_ALL)
			config->subtype_filtering = true;
	}

	/* configure ctrl filters */
	if (config->tx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_FP]) {
		tx_tlv_filter->ctrl_filter = 1;
		tx_tlv_filter->ctrl_dma_length =
			tx_lite_mon_set_len(config->tx_config.len[WLAN_FC0_TYPE_CTRL]);
		if ((config->tx_config.level == CDP_LITE_MON_LEVEL_MPDU) ||
		    (config->tx_config.level == CDP_LITE_MON_LEVEL_PPDU))
			tx_tlv_filter->ctrl_mpdu_log = 1;
	}
	/* Since ctrl frames are generated in host, we need to do subtype
	 * filtering even though ctrl filters are not enabled
	 */
	if (config->tx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_FP] !=
	    CDP_LITE_MON_FILTER_ALL)
		config->subtype_filtering = true;
	/* configure data filters */
	if (config->tx_config.data_filter[DP_MON_FRM_FILTER_MODE_FP]) {
		tx_tlv_filter->data_filter = 1;
		tx_tlv_filter->data_dma_length =
			tx_lite_mon_set_len(config->tx_config.len[WLAN_FC0_TYPE_DATA]);
		if ((config->tx_config.level == CDP_LITE_MON_LEVEL_MPDU) ||
		    (config->tx_config.level == CDP_LITE_MON_LEVEL_PPDU))
			tx_tlv_filter->data_mpdu_log = 1;
		if (config->tx_config.data_filter[DP_MON_FRM_FILTER_MODE_FP] !=
		    CDP_LITE_MON_FILTER_ALL)
			config->subtype_filtering = true;
	}

	dp_mon_filter_show_tx_filter_be(mode, &filter);
	be_mon_pdev->filter_be[mode][srng_type] = filter;
}
#endif /* QCA_SUPPORT_LITE_MONITOR */

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
/**
 * dp_cfr_filter_2_0() - Configure HOST monitor destination ring for CFR
 *
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @enable: Enable/Disable CFR
 * @filter_val: Flag to select Filter for monitor mode
 * @cfr_enable_monitor_mode: Flag to be enabled when scan radio is brought up
 * in special vap mode
 *
 * Return: void
 */
static void dp_cfr_filter_2_0(struct cdp_soc_t *soc_hdl,
			      uint8_t pdev_id,
			      bool enable,
			      struct cdp_monitor_filter *filter_val,
			      bool cfr_enable_monitor_mode)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = NULL;
	struct htt_rx_ring_tlv_filter *htt_tlv_filter;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_filter_be filter = {0};
	enum dp_mon_filter_srng_type srng_type =
		DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_mon_err("pdev is NULL");
		return;
	}

	mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->mvdev) {
		if (enable && cfr_enable_monitor_mode)
			pdev->cfr_rcc_mode = true;
		else
			pdev->cfr_rcc_mode = false;
		return;
	}

	soc = pdev->soc;
	pdev->cfr_rcc_mode = false;

	/* Get default tlv settings */
	htt_tlv_filter = &filter.rx_tlv_filter.tlv_filter;
	dp_rx_mon_filter_h2t_setup(soc, pdev, srng_type, &filter.rx_tlv_filter);

	if (filter.rx_tlv_filter.valid)
		htt_tlv_filter->enable = 1;
	else
		htt_tlv_filter->enable = 0;

	dp_mon_info("enable : %d, mode: 0x%x", enable, filter_val->mode);

	if (enable) {
		pdev->cfr_rcc_mode = true;
		htt_tlv_filter->ppdu_start = 1;
		htt_tlv_filter->ppdu_end = 1;
		htt_tlv_filter->ppdu_end_user_stats = 1;
		htt_tlv_filter->ppdu_end_user_stats_ext = 1;
		htt_tlv_filter->ppdu_end_status_done = 1;
		htt_tlv_filter->mpdu_start = 1;
		htt_tlv_filter->offset_valid = false;

		htt_tlv_filter->enable_fp =
			(filter_val->mode & MON_FILTER_PASS) ? 1 : 0;
		htt_tlv_filter->enable_md = 0;
		htt_tlv_filter->enable_mo =
			(filter_val->mode & MON_FILTER_OTHER) ? 1 : 0;
		htt_tlv_filter->fp_mgmt_filter = filter_val->fp_mgmt;
		htt_tlv_filter->fp_ctrl_filter = filter_val->fp_ctrl;
		htt_tlv_filter->fp_data_filter = filter_val->fp_data;
		htt_tlv_filter->mo_mgmt_filter = filter_val->mo_mgmt;
		htt_tlv_filter->mo_ctrl_filter = filter_val->mo_ctrl;
		htt_tlv_filter->mo_data_filter = filter_val->mo_data;
	}

	dp_mon_ht2_rx_ring_cfg(soc, pdev, srng_type,
			       &filter.rx_tlv_filter.tlv_filter);
}

void dp_cfr_filter_register_2_0(struct cdp_ops *ops)
{
	ops->cfr_ops->txrx_cfr_filter = dp_cfr_filter_2_0;
}
#endif

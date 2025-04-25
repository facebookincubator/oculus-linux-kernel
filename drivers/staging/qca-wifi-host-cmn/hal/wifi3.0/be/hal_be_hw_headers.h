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

#ifndef _HAL_BE_HW_INTERNAL_H_
#define _HAL_BE_HW_INTERNAL_H_

#include "wcss_seq_hwioreg_umac.h"
#include "phyrx_location.h"
#include "receive_rssi_info.h"
#include "buffer_addr_info.h"

#include "wbm2sw_completion_ring_tx.h"
#include "wbm2sw_completion_ring_rx.h"

#if defined(QCA_WIFI_KIWI)
#include "msmhwioreg.h"
#endif
#include "phyrx_common_user_info.h"

/* TX MONITOR */
#if  defined(WLAN_PKT_CAPTURE_TX_2_0) || \
defined(WLAN_PKT_CAPTURE_RX_2_0)
#include "mon_buffer_addr.h"
/* FES WINDOW OPEN */
#include "tx_fes_setup.h"
#include "rx_response_required_info.h"

/* FES WINDOW CLOSE */
#include "tx_fes_status_end.h"
#include "response_end_status.h"
/* WIFITX_FLUSH_E */
#include "pcu_ppdu_setup_init.h"
#include "tx_peer_entry.h"
#include "tx_queue_extension.h"
#include "tx_mpdu_start.h"
#include "tx_msdu_start.h"
/* WIFITX_DATA_E */
#include "mon_buffer_addr.h"
/* WIFITX_MPDU_END_E */
/* WIFITX_MSDU_END_E */
/* WIFITX_LAST_MPDU_FETCHED_E */
/* WIFITX_LAST_MPDU_END_E */
#include "coex_tx_req.h"
#include "tx_raw_or_native_frame_setup.h"
/* WIFINDP_PREAMBLE_DONE_E */
/* WIFISCH_CRITICAL_TLV_REFERENCE_E */
/* WIFITX_LOOPBACK_SETUP_E */
/* WIFITX_FES_SETUP_COMPLETE_E */
/* WIFITQM_MPDU_GLOBAL_START_E */
/* WIFITX_WUR_DATA_E */
/* WIFISCHEDULER_END_E */
#include "pdg_tx_req.h"
#include "tx_fes_status_start.h"
#include "tx_fes_status_prot.h"
#include "tx_fes_status_start_prot.h"
/* WIFIPROT_TX_END_E */
#include "tx_fes_status_start_ppdu.h"
#include "tx_fes_status_user_ppdu.h"
/* WIFIPPDU_TX_END_E */
#include "tx_fes_status_user_response.h"
#include "tx_fes_status_ack_or_ba.h"
#include "tx_fes_status_1k_ba.h"
#include "received_response_user_7_0.h"
#include "received_response_user_15_8.h"
#include "received_response_user_23_16.h"
#include "received_response_user_31_24.h"
#include "received_response_user_36_32.h"
#include "txpcu_buffer_status.h"
#include "txpcu_user_buffer_status.h"
/* WIFITXDMA_STOP_REQUEST_E */
#include "tx_cbf_info.h"
/* WIFITX_MPDU_COUNT_TRANSFER_END_E */
#include "pdg_response.h"
/* WIFIPDG_TRIG_RESPONSE_E */
#include "received_trigger_info.h"
#include "ofdma_trigger_details.h"
#include "rx_frame_bitmap_ack.h"
#include "rx_frame_1k_bitmap_ack.h"
#include "response_start_status.h"
#include "rx_start_param.h"
#include "rxpcu_early_rx_indication.h"
/* WIFIRX_PM_INFO_E */
#include "tx_flush_req.h"
#include "coex_tx_status.h"
/* WIFIR2R_STATUS_END_E */
#include "rx_preamble.h"
#include "mactx_u_sig_eht_su_mu.h"
#include "mactx_u_sig_eht_tb.h"
#include "mactx_eht_sig_usr_ofdma.h"
#include "mactx_eht_sig_usr_mu_mimo.h"
#include "mactx_eht_sig_usr_su.h"
#include "mactx_he_sig_a_su.h"
#include "mactx_he_sig_a_mu_dl.h"
#include "mactx_he_sig_a_mu_ul.h"
#include "mactx_he_sig_b1_mu.h"
#include "mactx_he_sig_b2_mu.h"
#include "mactx_he_sig_b2_ofdma.h"
#include "mactx_l_sig_a.h"
#include "mactx_l_sig_b.h"
#include "mactx_ht_sig.h"
#include "mactx_vht_sig_a.h"
#include "mactx_vht_sig_b_mu160.h"
#include "mactx_vht_sig_b_mu80.h"
#include "mactx_vht_sig_b_mu40.h"
#include "mactx_vht_sig_b_mu20.h"
#include "mactx_vht_sig_b_su160.h"
#include "mactx_vht_sig_b_su80.h"
#include "mactx_vht_sig_b_su40.h"
#include "mactx_vht_sig_b_su20.h"
#include "phytx_ppdu_header_info_request.h"
#include "mactx_user_desc_per_user.h"
#include "mactx_user_desc_common.h"
#include "mactx_phy_desc.h"
#include "coex_rx_status.h"
#include "rx_ppdu_ack_report.h"
#include "rx_ppdu_no_ack_report.h"
/* WIFITXPCU_PHYTX_OTHER_TRANSMIT_INFO32_E */
/* WIFITXPCU_PHYTX_DEBUG32_E */
/* WIFITXPCU_PREAMBLE_DONE_E */
/* WIFIRX_PHY_SLEEP_E */
#include "rx_frame_bitmap_req.h"
/* WIFIRXPCU_TX_SETUP_CLEAR_E */
#include "rx_trig_info.h"
#include "expected_response.h"
/* WIFITRIGGER_RESPONSE_TX_DONE_E */
#endif /* WLAN_PKT_CAPTURE_TX_2_0 */

#include <reo_descriptor_threshold_reached_status.h>
#include <reo_flush_queue.h>
#ifdef REO_SHARED_QREF_TABLE_EN
#include "rx_reo_queue_reference.h"
#endif
#define HAL_DESC_64_SET_FIELD(_desc, _word, _fld, _value) do { \
	((uint64_t *)(_desc))[(_word ## _ ## _fld ## _OFFSET) >> 3] &= \
		~(_word ## _ ## _fld ## _MASK); \
	((uint64_t *)(_desc))[(_word ## _ ## _fld ## _OFFSET) >> 3] |= \
		(((uint64_t)(_value)) << _word ## _ ## _fld ## _LSB); \
} while (0)

#endif /* _HAL_BE_HW_INTERNAL_H_ */

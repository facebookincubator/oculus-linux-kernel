/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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

#include "hal_hw_headers.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_peer.h"
#include "hal_rx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "qdf_nbuf.h"
#include "hal_api_mon.h"
#include "dp_internal.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "dp_htt.h"
#include "dp_mon.h"
#include "dp_rx_mon.h"

#include "htt.h"
#ifdef FEATURE_PERPKT_INFO
#include "dp_ratetable.h"
#endif

#ifndef IEEE80211_FCO_SUBTYPE_ACTION_NO_ACK
#define IEEE80211_FCO_SUBTYPE_ACTION_NO_ACK 0xe0
#endif

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
void
dp_rx_mon_handle_cfr_mu_info(struct dp_pdev *pdev,
			     struct hal_rx_ppdu_info *ppdu_info,
			     struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	struct dp_peer *peer;
	struct dp_soc *soc = pdev->soc;
	struct mon_rx_user_status *rx_user_status;
	struct cdp_rx_stats_ppdu_user *rx_stats_peruser;
	uint32_t num_users;
	int user_id;
	uint16_t sw_peer_id;

	num_users = ppdu_info->com_info.num_users;
	for (user_id = 0; user_id < num_users; user_id++) {
		if (user_id > OFDMA_NUM_USERS) {
			return;
		}

		rx_user_status =  &ppdu_info->rx_user_status[user_id];
		rx_stats_peruser = &cdp_rx_ppdu->user[user_id];
		sw_peer_id = rx_user_status->sw_peer_id;
		peer = dp_peer_get_ref_by_id(soc, sw_peer_id,
					     DP_MOD_ID_RX_PPDU_STATS);
		if (!peer) {
			rx_stats_peruser->peer_id = HTT_INVALID_PEER;
			continue;
		}

		qdf_mem_copy(rx_stats_peruser->mac_addr,
			     peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
		dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);
	}
}

void
dp_rx_mon_populate_cfr_ppdu_info(struct dp_pdev *pdev,
				 struct hal_rx_ppdu_info *ppdu_info,
				 struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	struct dp_peer *peer;
	struct dp_soc *soc = pdev->soc;
	int chain;
	uint16_t sw_peer_id;
	struct mon_rx_user_status *rx_user_status;
	uint32_t num_users = ppdu_info->com_info.num_users;

	cdp_rx_ppdu->ppdu_id = ppdu_info->com_info.ppdu_id;
	cdp_rx_ppdu->timestamp = ppdu_info->rx_status.tsft;
	cdp_rx_ppdu->u.ppdu_type = ppdu_info->rx_status.reception_type;

	for (chain = 0; chain < MAX_CHAIN; chain++)
		cdp_rx_ppdu->per_chain_rssi[chain] =
			ppdu_info->rx_status.rssi[chain];

	cdp_rx_ppdu->u.ltf_size = ppdu_info->rx_status.ltf_size;
	cdp_rx_ppdu->beamformed = ppdu_info->rx_status.beamformed;
	cdp_rx_ppdu->u.ldpc = ppdu_info->rx_status.ldpc;

	if ((ppdu_info->rx_status.sgi == VHT_SGI_NYSM) &&
	    (ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AC))
		cdp_rx_ppdu->u.gi = CDP_SGI_0_4_US;
	else
		cdp_rx_ppdu->u.gi = ppdu_info->rx_status.sgi;

	if (ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AC) {
		cdp_rx_ppdu->u.stbc = ppdu_info->rx_status.is_stbc;
	} else if (ppdu_info->rx_status.preamble_type ==
			HAL_RX_PKT_TYPE_11AX) {
		cdp_rx_ppdu->u.stbc = (ppdu_info->rx_status.he_data3 >>
				       QDF_MON_STATUS_STBC_SHIFT) & 0x1;
		cdp_rx_ppdu->u.dcm = (ppdu_info->rx_status.he_data3 >>
				      QDF_MON_STATUS_DCM_SHIFT) & 0x1;
	}

	qdf_assert_always(num_users <= CDP_MU_MAX_USERS);
	dp_rx_mon_handle_cfr_mu_info(pdev, ppdu_info, cdp_rx_ppdu);
	rx_user_status = &ppdu_info->rx_user_status[num_users - 1];
	sw_peer_id = rx_user_status->sw_peer_id;
	peer = dp_peer_get_ref_by_id(soc, sw_peer_id, DP_MOD_ID_RX_PPDU_STATS);
	if (!peer) {
		cdp_rx_ppdu->peer_id = HTT_INVALID_PEER;
		cdp_rx_ppdu->num_users = 0;
		return;
	}

	cdp_rx_ppdu->peer_id = peer->peer_id;
	cdp_rx_ppdu->vdev_id = peer->vdev->vdev_id;
	cdp_rx_ppdu->num_users = num_users;

	dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);
}

bool
dp_cfr_rcc_mode_status(struct dp_pdev *pdev)
{
	return pdev->cfr_rcc_mode;
}

void
dp_rx_mon_populate_cfr_info(struct dp_pdev *pdev,
			    struct hal_rx_ppdu_info *ppdu_info,
			    struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	struct cdp_rx_ppdu_cfr_info *cfr_info;

	if (!qdf_unlikely(dp_cfr_rcc_mode_status(pdev)))
		return;

	cfr_info = &cdp_rx_ppdu->cfr_info;

	cfr_info->bb_captured_channel
		= ppdu_info->cfr_info.bb_captured_channel;
	cfr_info->bb_captured_timeout
		= ppdu_info->cfr_info.bb_captured_timeout;
	cfr_info->bb_captured_reason
		= ppdu_info->cfr_info.bb_captured_reason;
	cfr_info->rx_location_info_valid
		= ppdu_info->cfr_info.rx_location_info_valid;
	cfr_info->chan_capture_status
		= ppdu_info->cfr_info.chan_capture_status;
	cfr_info->rtt_che_buffer_pointer_high8
		= ppdu_info->cfr_info.rtt_che_buffer_pointer_high8;
	cfr_info->rtt_che_buffer_pointer_low32
		= ppdu_info->cfr_info.rtt_che_buffer_pointer_low32;
	cfr_info->rtt_cfo_measurement
		= (int16_t)ppdu_info->cfr_info.rtt_cfo_measurement;
	cfr_info->agc_gain_info0
		= ppdu_info->cfr_info.agc_gain_info0;
	cfr_info->agc_gain_info1
		= ppdu_info->cfr_info.agc_gain_info1;
	cfr_info->agc_gain_info2
		= ppdu_info->cfr_info.agc_gain_info2;
	cfr_info->agc_gain_info3
		= ppdu_info->cfr_info.agc_gain_info3;
	cfr_info->rx_start_ts
		= ppdu_info->cfr_info.rx_start_ts;
	cfr_info->mcs_rate
		= ppdu_info->cfr_info.mcs_rate;
	cfr_info->gi_type
		= ppdu_info->cfr_info.gi_type;
}

void
dp_update_cfr_dbg_stats(struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_rx_ppdu_cfr_info *cfr = &ppdu_info->cfr_info;

	DP_STATS_INC(pdev,
		     rcc.chan_capture_status[cfr->chan_capture_status], 1);
	if (cfr->rx_location_info_valid) {
		DP_STATS_INC(pdev, rcc.rx_loc_info_valid_cnt, 1);
		if (cfr->bb_captured_channel) {
			DP_STATS_INC(pdev, rcc.bb_captured_channel_cnt, 1);
			DP_STATS_INC(pdev,
				     rcc.reason_cnt[cfr->bb_captured_reason],
				     1);
		} else if (cfr->bb_captured_timeout) {
			DP_STATS_INC(pdev, rcc.bb_captured_timeout_cnt, 1);
			DP_STATS_INC(pdev,
				     rcc.reason_cnt[cfr->bb_captured_reason],
				     1);
		}
	}
}

void
dp_rx_handle_cfr(struct dp_soc *soc, struct dp_pdev *pdev,
		 struct hal_rx_ppdu_info *ppdu_info)
{
	qdf_nbuf_t ppdu_nbuf;
	struct cdp_rx_indication_ppdu *cdp_rx_ppdu;

	dp_update_cfr_dbg_stats(pdev, ppdu_info);
	if (!ppdu_info->cfr_info.bb_captured_channel)
		return;

	ppdu_nbuf = qdf_nbuf_alloc(soc->osdev,
				   sizeof(struct cdp_rx_indication_ppdu),
				   0,
				   0,
				   FALSE);
	if (ppdu_nbuf) {
		cdp_rx_ppdu = (struct cdp_rx_indication_ppdu *)ppdu_nbuf->data;

		dp_rx_mon_populate_cfr_info(pdev, ppdu_info, cdp_rx_ppdu);
		dp_rx_mon_populate_cfr_ppdu_info(pdev, ppdu_info, cdp_rx_ppdu);
		qdf_nbuf_put_tail(ppdu_nbuf,
				  sizeof(struct cdp_rx_indication_ppdu));
		dp_wdi_event_handler(WDI_EVENT_RX_PPDU_DESC, soc,
				     ppdu_nbuf, HTT_INVALID_PEER,
				     WDI_NO_VAL, pdev->pdev_id);
	}
}

void
dp_rx_populate_cfr_non_assoc_sta(struct dp_pdev *pdev,
				 struct hal_rx_ppdu_info *ppdu_info,
				 struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	if (!dp_cfr_rcc_mode_status(pdev))
		return;

	if (ppdu_info->cfr_info.bb_captured_channel)
		dp_rx_mon_populate_cfr_ppdu_info(pdev, ppdu_info, cdp_rx_ppdu);
}

/**
 * dp_bb_captured_chan_status() - Get the bb_captured_channel status
 * @pdev: pdev ctx
 * @ppdu_info: structure for rx ppdu ring
 *
 * Return: Success/ Failure
 */
static inline QDF_STATUS
dp_bb_captured_chan_status(struct dp_pdev *pdev,
			   struct hal_rx_ppdu_info *ppdu_info)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct hal_rx_ppdu_cfr_info *cfr = &ppdu_info->cfr_info;

	if (dp_cfr_rcc_mode_status(pdev)) {
		if (cfr->bb_captured_channel)
			status = QDF_STATUS_SUCCESS;
	}

	return status;
}
#else
static inline QDF_STATUS
dp_bb_captured_chan_status(struct dp_pdev *pdev,
			   struct hal_rx_ppdu_info *ppdu_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_CFR_ENABLE */

#ifdef QCA_ENHANCED_STATS_SUPPORT
#ifdef QCA_RSSI_DB2DBM
/**
 * dp_rx_mon_rf_index_conv() - this function will convert BB index to RF
 *			index in the rssi_chain[chain][bw] array
 *
 * @chain: BB chain index
 * @mon_pdev: pdev structure
 *
 * Return: return RF chain index
 *
 * Computation:
 *  3 Bytes of xbar_config are used for RF to BB mapping
 *  Samples of xbar_config,
 *
 * If xbar_config is 0x688FAC(hex):
 *     RF chains 0-3 are connected to BB chains 4-7
 *     RF chains 4-7 are connected to BB chains 0-3
 *     here,
 *     bits 0 to 2 = 4, maps BB chain 4 for RF chain 0
 *     bits 3 to 5 = 5, maps BB chain 5 for RF chain 1
 *     bits 6 to 8 = 6, maps BB chain 6 for RF chain 2
 *     bits 9 to 11 = 7, maps BB chain 7 for RF chain 3
 *     bits 12 to 14 = 0, maps BB chain 0 for RF chain 4
 *     bits 15 to 17 = 1, maps BB chain 1 for RF chain 5
 *     bits 18 to 20 = 2, maps BB chain 2 for RF chain 6
 *     bits 21 to 23 = 3, maps BB chain 3 for RF chain 7
 */
static uint8_t dp_rx_mon_rf_index_conv(uint8_t chain,
				       struct dp_mon_pdev *mon_pdev)
{
	uint32_t xbar_config = mon_pdev->rssi_offsets.xbar_config;

	if (mon_pdev->rssi_dbm_conv_support && xbar_config)
		return ((xbar_config >> (3 * chain)) & 0x07);
	return chain;
}
#else
static uint8_t dp_rx_mon_rf_index_conv(uint8_t chain,
				       struct dp_mon_pdev *mon_pdev)
{
	return chain;
}
#endif
void
dp_rx_populate_rx_rssi_chain(struct hal_rx_ppdu_info *ppdu_info,
			     struct cdp_rx_indication_ppdu *cdp_rx_ppdu,
			     struct dp_pdev *pdev)
{
	uint8_t chain, bw;
	uint8_t rssi;
	uint8_t chain_rf;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	for (chain = 0; chain < SS_COUNT; chain++) {
		for (bw = 0; bw < MAX_BW; bw++) {
			chain_rf = dp_rx_mon_rf_index_conv(chain, mon_pdev);
			rssi = ppdu_info->rx_status.rssi_chain[chain_rf][bw];
			if (rssi != DP_RSSI_INVAL)
				cdp_rx_ppdu->rssi_chain[chain_rf][bw] = rssi;
			else
				cdp_rx_ppdu->rssi_chain[chain_rf][bw] = 0;
		}
	}
}

void
dp_rx_populate_su_evm_details(struct hal_rx_ppdu_info *ppdu_info,
			      struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	uint8_t pilot_evm;
	uint8_t nss_count;
	uint8_t pilot_count;

	nss_count = ppdu_info->evm_info.nss_count;
	pilot_count = ppdu_info->evm_info.pilot_count;

	if ((nss_count * pilot_count) > DP_RX_MAX_SU_EVM_COUNT) {
		qdf_err("pilot evm count is more than expected");
		return;
	}
	cdp_rx_ppdu->evm_info.pilot_count = pilot_count;
	cdp_rx_ppdu->evm_info.nss_count = nss_count;

	/* Populate evm for pilot_evm  = nss_count*pilot_count */
	for (pilot_evm = 0; pilot_evm < nss_count * pilot_count; pilot_evm++) {
		cdp_rx_ppdu->evm_info.pilot_evm[pilot_evm] =
			ppdu_info->evm_info.pilot_evm[pilot_evm];
	}
}

/**
 * dp_rx_inc_rusize_cnt() - increment pdev stats based on RU size
 * @pdev: pdev ctx
 * @rx_user_status: mon rx user status
 *
 * Return: bool
 */
static inline bool
dp_rx_inc_rusize_cnt(struct dp_pdev *pdev,
		     struct mon_rx_user_status *rx_user_status)
{
	uint32_t ru_size;
	bool is_data;

	ru_size = rx_user_status->ofdma_ru_size;

	if (dp_is_subtype_data(rx_user_status->frame_control)) {
		DP_STATS_INC(pdev,
			     ul_ofdma.data_rx_ru_size[ru_size], 1);
		is_data = true;
	} else {
		DP_STATS_INC(pdev,
			     ul_ofdma.nondata_rx_ru_size[ru_size], 1);
		is_data = false;
	}

	return is_data;
}

/**
 * dp_rx_populate_cdp_indication_ppdu_user() - Populate per user cdp indication
 * @pdev: pdev ctx
 * @ppdu_info: ppdu info structure from ppdu ring
 * @cdp_rx_ppdu: Rx PPDU indication structure
 *
 * Return: none
 */
static void
dp_rx_populate_cdp_indication_ppdu_user(struct dp_pdev *pdev,
					struct hal_rx_ppdu_info *ppdu_info,
					struct cdp_rx_indication_ppdu
					*cdp_rx_ppdu)
{
	struct dp_peer *peer;
	struct dp_soc *soc = pdev->soc;
	int i;
	struct mon_rx_user_status *rx_user_status;
	struct mon_rx_user_info *rx_user_info;
	struct cdp_rx_stats_ppdu_user *rx_stats_peruser;
	int ru_size;
	bool is_data = false;
	uint32_t num_users;
	struct dp_mon_ops *mon_ops;
	uint16_t sw_peer_id;

	num_users = ppdu_info->com_info.num_users;
	for (i = 0; i < num_users; i++) {
		if (i > OFDMA_NUM_USERS)
			return;

		rx_user_status =  &ppdu_info->rx_user_status[i];
		rx_user_info = &ppdu_info->rx_user_info[i];
		rx_stats_peruser = &cdp_rx_ppdu->user[i];

		sw_peer_id = rx_user_status->sw_peer_id;
		peer = dp_peer_get_ref_by_id(soc, sw_peer_id,
					     DP_MOD_ID_RX_PPDU_STATS);
		if (qdf_unlikely(!peer)) {
			rx_stats_peruser->peer_id = HTT_INVALID_PEER;
			continue;
		}
		rx_stats_peruser->is_bss_peer = peer->bss_peer;

		rx_stats_peruser->first_data_seq_ctrl =
			rx_user_status->first_data_seq_ctrl;

		rx_stats_peruser->frame_control_info_valid =
			rx_user_status->frame_control_info_valid;
		rx_stats_peruser->frame_control =
			rx_user_status->frame_control;

		rx_stats_peruser->qos_control_info_valid =
			rx_user_info->qos_control_info_valid;
		rx_stats_peruser->qos_control =
			rx_user_info->qos_control;
		rx_stats_peruser->tcp_msdu_count =
			rx_user_status->tcp_msdu_count;
		rx_stats_peruser->udp_msdu_count =
			rx_user_status->udp_msdu_count;
		rx_stats_peruser->other_msdu_count =
			rx_user_status->other_msdu_count;

		rx_stats_peruser->num_msdu =
			rx_stats_peruser->tcp_msdu_count +
			rx_stats_peruser->udp_msdu_count +
			rx_stats_peruser->other_msdu_count;

		rx_stats_peruser->preamble_type =
				cdp_rx_ppdu->u.preamble;
		rx_stats_peruser->mpdu_cnt_fcs_ok =
			rx_user_status->mpdu_cnt_fcs_ok;
		rx_stats_peruser->mpdu_cnt_fcs_err =
			rx_user_status->mpdu_cnt_fcs_err;
		qdf_mem_copy(&rx_stats_peruser->mpdu_fcs_ok_bitmap,
			     &rx_user_status->mpdu_fcs_ok_bitmap,
			     HAL_RX_NUM_WORDS_PER_PPDU_BITMAP *
			     sizeof(rx_user_status->mpdu_fcs_ok_bitmap[0]));
		rx_stats_peruser->mpdu_ok_byte_count =
			rx_user_status->mpdu_ok_byte_count;
		rx_stats_peruser->mpdu_err_byte_count =
			rx_user_status->mpdu_err_byte_count;

		cdp_rx_ppdu->num_mpdu += rx_user_status->mpdu_cnt_fcs_ok;
		cdp_rx_ppdu->num_msdu += rx_stats_peruser->num_msdu;
		rx_stats_peruser->retries =
			CDP_FC_IS_RETRY_SET(rx_stats_peruser->frame_control) ?
			rx_stats_peruser->mpdu_cnt_fcs_ok : 0;
		cdp_rx_ppdu->retries += rx_stats_peruser->retries;

		if (rx_stats_peruser->mpdu_cnt_fcs_ok > 1)
			rx_stats_peruser->is_ampdu = 1;
		else
			rx_stats_peruser->is_ampdu = 0;

		rx_stats_peruser->tid = ppdu_info->rx_status.tid;

		qdf_mem_copy(rx_stats_peruser->mac_addr,
			     peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
		rx_stats_peruser->peer_id = peer->peer_id;
		cdp_rx_ppdu->vdev_id = peer->vdev->vdev_id;
		rx_stats_peruser->vdev_id = peer->vdev->vdev_id;
		rx_stats_peruser->mu_ul_info_valid = 0;

		mon_ops = dp_mon_ops_get(soc);
		if (mon_ops && mon_ops->mon_rx_populate_ppdu_usr_info)
			mon_ops->mon_rx_populate_ppdu_usr_info(rx_user_status,
							       rx_stats_peruser);

		dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);
		if (cdp_rx_ppdu->u.ppdu_type == HAL_RX_TYPE_MU_OFDMA ||
		    cdp_rx_ppdu->u.ppdu_type == HAL_RX_TYPE_MU_MIMO) {
			if (rx_user_status->mu_ul_info_valid) {
				rx_stats_peruser->nss = rx_user_status->nss;
				cdp_rx_ppdu->usr_nss_sum += rx_stats_peruser->nss;
				rx_stats_peruser->mcs = rx_user_status->mcs;
				rx_stats_peruser->mu_ul_info_valid =
					rx_user_status->mu_ul_info_valid;
				rx_stats_peruser->ofdma_ru_start_index =
					rx_user_status->ofdma_ru_start_index;
				rx_stats_peruser->ofdma_ru_width =
					rx_user_status->ofdma_ru_width;
				cdp_rx_ppdu->usr_ru_tones_sum +=
					rx_stats_peruser->ofdma_ru_width;
				rx_stats_peruser->user_index = i;
				ru_size = rx_user_status->ofdma_ru_size;
				/*
				 * max RU size will be equal to
				 * HTT_UL_OFDMA_V0_RU_SIZE_RU_996x2
				 */
				if (qdf_unlikely(ru_size >= OFDMA_NUM_RU_SIZE)) {
					dp_err("invalid ru_size %d\n",
					       ru_size);
					return;
				}
				is_data = dp_rx_inc_rusize_cnt(pdev,
							       rx_user_status);
			}
			if (is_data) {
				/* counter to get number of MU OFDMA */
				pdev->stats.ul_ofdma.data_rx_ppdu++;
				pdev->stats.ul_ofdma.data_users[num_users]++;
			}
		}
	}
}

/**
 * dp_rx_populate_cdp_indication_ppdu() - Populate cdp rx indication structure
 * @pdev: pdev ctx
 * @ppdu_info: ppdu info structure from ppdu ring
 * @cdp_rx_ppdu: Rx PPDU indication structure
 *
 * Return: none
 */
static void
dp_rx_populate_cdp_indication_ppdu(struct dp_pdev *pdev,
				   struct hal_rx_ppdu_info *ppdu_info,
				   struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	struct dp_peer *peer;
	struct dp_soc *soc = pdev->soc;
	uint32_t i;
	struct dp_mon_ops *mon_ops;
	uint16_t sw_peer_id;
	struct mon_rx_user_status *rx_user_status;
	uint32_t num_users = ppdu_info->com_info.num_users;

	cdp_rx_ppdu->first_data_seq_ctrl =
		ppdu_info->rx_status.first_data_seq_ctrl;
	cdp_rx_ppdu->frame_ctrl =
		ppdu_info->rx_status.frame_control;
	cdp_rx_ppdu->tcp_msdu_count = ppdu_info->rx_status.tcp_msdu_count;
	cdp_rx_ppdu->udp_msdu_count = ppdu_info->rx_status.udp_msdu_count;
	cdp_rx_ppdu->other_msdu_count = ppdu_info->rx_status.other_msdu_count;
	/* num mpdu is consolidated and added together in num user loop */
	cdp_rx_ppdu->num_mpdu = ppdu_info->com_info.mpdu_cnt_fcs_ok;
	/* num msdu is consolidated and added together in num user loop */
	cdp_rx_ppdu->num_msdu = (cdp_rx_ppdu->tcp_msdu_count +
				 cdp_rx_ppdu->udp_msdu_count +
				 cdp_rx_ppdu->other_msdu_count);

	cdp_rx_ppdu->retries = CDP_FC_IS_RETRY_SET(cdp_rx_ppdu->frame_ctrl) ?
		ppdu_info->com_info.mpdu_cnt_fcs_ok : 0;

	if (ppdu_info->com_info.mpdu_cnt_fcs_ok > 1)
		cdp_rx_ppdu->is_ampdu = 1;
	else
		cdp_rx_ppdu->is_ampdu = 0;
	cdp_rx_ppdu->tid = ppdu_info->rx_status.tid;

	qdf_assert_always(num_users <= CDP_MU_MAX_USERS);
	rx_user_status = &ppdu_info->rx_user_status[num_users - 1];
	sw_peer_id = rx_user_status->sw_peer_id;
	peer = dp_peer_get_ref_by_id(soc, sw_peer_id,
				     DP_MOD_ID_RX_PPDU_STATS);
	if (qdf_unlikely(!peer)) {
		cdp_rx_ppdu->peer_id = HTT_INVALID_PEER;
		cdp_rx_ppdu->num_users = 0;
		goto end;
	}

	qdf_mem_copy(cdp_rx_ppdu->mac_addr,
		     peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
	cdp_rx_ppdu->peer_id = peer->peer_id;
	cdp_rx_ppdu->vdev_id = peer->vdev->vdev_id;

	cdp_rx_ppdu->ppdu_id = ppdu_info->com_info.ppdu_id;
	cdp_rx_ppdu->length = ppdu_info->rx_status.ppdu_len;
	cdp_rx_ppdu->duration = ppdu_info->rx_status.duration;
	cdp_rx_ppdu->u.nss = ppdu_info->rx_status.nss;
	cdp_rx_ppdu->u.mcs = ppdu_info->rx_status.mcs;
	if ((ppdu_info->rx_status.sgi == VHT_SGI_NYSM) &&
	    (ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AC))
		cdp_rx_ppdu->u.gi = CDP_SGI_0_4_US;
	else
		cdp_rx_ppdu->u.gi = ppdu_info->rx_status.sgi;
	cdp_rx_ppdu->u.ldpc = ppdu_info->rx_status.ldpc;
	cdp_rx_ppdu->u.ppdu_type = ppdu_info->rx_status.reception_type;
	cdp_rx_ppdu->u.ltf_size = (ppdu_info->rx_status.he_data5 >>
				   QDF_MON_STATUS_HE_LTF_SIZE_SHIFT) & 0x3;
	cdp_rx_ppdu->rssi = ppdu_info->rx_status.rssi_comb;
	cdp_rx_ppdu->timestamp = ppdu_info->rx_status.tsft;
	cdp_rx_ppdu->channel = ppdu_info->rx_status.chan_num;
	cdp_rx_ppdu->beamformed = ppdu_info->rx_status.beamformed;
	cdp_rx_ppdu->num_bytes = ppdu_info->rx_status.ppdu_len;
	cdp_rx_ppdu->lsig_a = ppdu_info->rx_status.rate;
	cdp_rx_ppdu->u.ltf_size = ppdu_info->rx_status.ltf_size;

	if (ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AC) {
		cdp_rx_ppdu->u.stbc = ppdu_info->rx_status.is_stbc;
	} else if (ppdu_info->rx_status.preamble_type ==
			HAL_RX_PKT_TYPE_11AX) {
		cdp_rx_ppdu->u.stbc = (ppdu_info->rx_status.he_data3 >>
				       QDF_MON_STATUS_STBC_SHIFT) & 0x1;
		cdp_rx_ppdu->u.dcm = (ppdu_info->rx_status.he_data3 >>
				      QDF_MON_STATUS_DCM_SHIFT) & 0x1;
	}
	dp_rx_populate_rx_rssi_chain(ppdu_info, cdp_rx_ppdu, pdev);
	dp_rx_populate_su_evm_details(ppdu_info, cdp_rx_ppdu);
	cdp_rx_ppdu->rx_antenna = ppdu_info->rx_status.rx_antenna;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_rx_populate_ppdu_info)
		mon_ops->mon_rx_populate_ppdu_info(ppdu_info,
						   cdp_rx_ppdu);

	cdp_rx_ppdu->nf = ppdu_info->rx_status.chan_noise_floor;
	for (i = 0; i < MAX_CHAIN; i++)
		cdp_rx_ppdu->per_chain_rssi[i] = ppdu_info->rx_status.rssi[i];

	cdp_rx_ppdu->is_mcast_bcast = ppdu_info->nac_info.mcast_bcast;

	cdp_rx_ppdu->num_users = ppdu_info->com_info.num_users;

	dp_rx_populate_cdp_indication_ppdu_user(pdev, ppdu_info, cdp_rx_ppdu);

	dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);

	return;
end:
	dp_rx_populate_cfr_non_assoc_sta(pdev, ppdu_info, cdp_rx_ppdu);
}

/**
 * dp_rx_rate_stats_update() - Update per-peer rate statistics
 * @peer: Datapath peer handle
 * @ppdu: PPDU Descriptor
 * @user: user index
 *
 * Return: None
 */
static inline void dp_rx_rate_stats_update(struct dp_peer *peer,
					   struct cdp_rx_indication_ppdu *ppdu,
					   uint32_t user)
{
	uint32_t ratekbps = 0;
	uint32_t ppdu_rx_rate = 0;
	uint32_t nss = 0;
	uint8_t mcs = 0;
	uint32_t rix;
	uint16_t ratecode = 0;
	struct cdp_rx_stats_ppdu_user *ppdu_user = NULL;
	struct dp_mon_peer *mon_peer = NULL;

	if (!peer || !ppdu)
		return;

	mon_peer = peer->monitor_peer;
	ppdu_user = &ppdu->user[user];

	if (!mon_peer)
		return;

	if (ppdu->u.ppdu_type != HAL_RX_TYPE_SU) {
		if (ppdu_user->nss == 0)
			nss = 0;
		else
			nss = ppdu_user->nss - 1;
		mcs = ppdu_user->mcs;

		mon_peer->stats.rx.nss_info = ppdu_user->nss;
		mon_peer->stats.rx.mcs_info = ppdu_user->mcs;
	} else {
		if (ppdu->u.nss == 0)
			nss = 0;
		else
			nss = ppdu->u.nss - 1;
		mcs = ppdu->u.mcs;

		mon_peer->stats.rx.nss_info = ppdu->u.nss;
		mon_peer->stats.rx.mcs_info = ppdu->u.mcs;
	}

	ratekbps = dp_getrateindex(ppdu->u.gi,
				   mcs,
				   nss,
				   ppdu->u.preamble,
				   ppdu->u.bw,
				   ppdu->punc_bw,
				   &rix,
				   &ratecode);

	if (!ratekbps) {
		ppdu->rix = 0;
		ppdu_user->rix = 0;
		ppdu->rx_ratekbps = 0;
		ppdu->rx_ratecode = 0;
		ppdu_user->rx_ratekbps = 0;
		return;
	}

	mon_peer->stats.rx.bw_info = ppdu->u.bw;
	mon_peer->stats.rx.gi_info = ppdu->u.gi;
	mon_peer->stats.rx.preamble_info = ppdu->u.preamble;

	ppdu->rix = rix;
	ppdu_user->rix = rix;
	DP_STATS_UPD(mon_peer, rx.last_rx_rate, ratekbps);
	mon_peer->stats.rx.avg_rx_rate =
		dp_ath_rate_lpf(mon_peer->stats.rx.avg_rx_rate, ratekbps);
	ppdu_rx_rate = dp_ath_rate_out(mon_peer->stats.rx.avg_rx_rate);
	DP_STATS_UPD(mon_peer, rx.rnd_avg_rx_rate, ppdu_rx_rate);
	ppdu->rx_ratekbps = ratekbps;
	ppdu->rx_ratecode = ratecode;
	ppdu_user->rx_ratekbps = ratekbps;

	if (peer->vdev)
		peer->vdev->stats.rx.last_rx_rate = ratekbps;
}

#ifdef WLAN_FEATURE_11BE
static inline uint8_t dp_get_bw_offset_frm_bw(struct dp_soc *soc,
					      enum CMN_BW_TYPES bw)
{
	uint8_t pkt_bw_offset;

	switch (bw) {
	case CMN_BW_20MHZ:
		pkt_bw_offset = PKT_BW_GAIN_20MHZ;
		break;
	case CMN_BW_40MHZ:
		pkt_bw_offset = PKT_BW_GAIN_40MHZ;
		break;
	case CMN_BW_80MHZ:
		pkt_bw_offset = PKT_BW_GAIN_80MHZ;
		break;
	case CMN_BW_160MHZ:
		pkt_bw_offset = PKT_BW_GAIN_160MHZ;
		break;
	case CMN_BW_320MHZ:
		pkt_bw_offset = PKT_BW_GAIN_320MHZ;
		break;
	default:
		pkt_bw_offset = 0;
		dp_rx_mon_status_debug("%pK: Invalid BW index = %d",
				       soc, bw);
	}

	return pkt_bw_offset;
}
#else
static inline uint8_t dp_get_bw_offset_frm_bw(struct dp_soc *soc,
					      enum CMN_BW_TYPES bw)
{
	uint8_t pkt_bw_offset;

	switch (bw) {
	case CMN_BW_20MHZ:
		pkt_bw_offset = PKT_BW_GAIN_20MHZ;
		break;
	case CMN_BW_40MHZ:
		pkt_bw_offset = PKT_BW_GAIN_40MHZ;
		break;
	case CMN_BW_80MHZ:
		pkt_bw_offset = PKT_BW_GAIN_80MHZ;
		break;
	case CMN_BW_160MHZ:
		pkt_bw_offset = PKT_BW_GAIN_160MHZ;
		break;
	default:
		pkt_bw_offset = 0;
		dp_rx_mon_status_debug("%pK: Invalid BW index = %d",
				       soc, bw);
	}

	return pkt_bw_offset;
}
#endif

#ifdef WLAN_TELEMETRY_STATS_SUPPORT
static void
dp_ppdu_desc_user_rx_time_update(struct dp_pdev *pdev,
				 struct dp_peer *peer,
				 struct cdp_rx_indication_ppdu *ppdu_desc,
				 struct cdp_rx_stats_ppdu_user *user)
{
	uint32_t nss_ru_width_sum = 0;
	struct dp_mon_peer *mon_peer = NULL;
	uint8_t ac = 0;

	if (!pdev || !ppdu_desc || !user || !peer)
		return;

	nss_ru_width_sum = ppdu_desc->usr_nss_sum * ppdu_desc->usr_ru_tones_sum;
	if (!nss_ru_width_sum)
		nss_ru_width_sum = 1;

	if (ppdu_desc->u.ppdu_type == HAL_RX_TYPE_MU_OFDMA ||
	    ppdu_desc->u.ppdu_type == HAL_RX_TYPE_MU_MIMO) {
		user->rx_time_us = (ppdu_desc->duration *
				    user->nss * user->ofdma_ru_width) /
				    nss_ru_width_sum;
	} else {
		user->rx_time_us = ppdu_desc->duration;
	}

	mon_peer = peer->monitor_peer;
	if (qdf_unlikely(!mon_peer))
		return;

	ac = TID_TO_WME_AC(user->tid);
	DP_STATS_INC(mon_peer, airtime_consumption[ac].consumption,
		     user->rx_time_us);
}
#else
static inline void
dp_ppdu_desc_user_rx_time_update(struct dp_pdev *pdev,
				 struct dp_peer *peer,
				 struct cdp_rx_indication_ppdu *ppdu_desc,
				 struct cdp_rx_stats_ppdu_user *user)
{ }
#endif

static void dp_rx_stats_update(struct dp_pdev *pdev,
			       struct cdp_rx_indication_ppdu *ppdu)
{
	struct dp_soc *soc = NULL;
	uint8_t mcs, preamble, ac = 0, nss, ppdu_type;
	uint32_t num_msdu;
	uint8_t pkt_bw_offset;
	struct dp_peer *peer;
	struct dp_mon_peer *mon_peer;
	struct cdp_rx_stats_ppdu_user *ppdu_user;
	uint32_t i;
	enum cdp_mu_packet_type mu_pkt_type;
	struct dp_mon_ops *mon_ops;
	struct dp_mon_pdev *mon_pdev = NULL;
	uint64_t byte_count;

	if (qdf_likely(pdev))
		soc = pdev->soc;
	else
		return;

	if (qdf_likely(!soc) || soc->process_rx_status)
		return;

	mon_pdev = pdev->monitor_pdev;

	preamble = ppdu->u.preamble;
	ppdu_type = ppdu->u.ppdu_type;

	for (i = 0; i < ppdu->num_users && i < CDP_MU_MAX_USERS; i++) {
		peer = NULL;
		ppdu_user = &ppdu->user[i];
		peer = dp_peer_get_ref_by_id(soc, ppdu_user->peer_id,
					     DP_MOD_ID_RX_PPDU_STATS);

		if (qdf_unlikely(!peer))
			mon_peer = mon_pdev->invalid_mon_peer;
		else
			mon_peer = peer->monitor_peer;

		if (qdf_unlikely(!mon_peer)) {
			if (peer)
				dp_peer_unref_delete(peer,
						     DP_MOD_ID_RX_PPDU_STATS);

			continue;
		}

		if ((preamble == DOT11_A) || (preamble == DOT11_B))
			ppdu->u.nss = 1;

		if (ppdu_type == HAL_RX_TYPE_SU) {
			mcs = ppdu->u.mcs;
			nss = ppdu->u.nss;
		} else {
			mcs = ppdu_user->mcs;
			nss = ppdu_user->nss;
		}

		num_msdu = ppdu_user->num_msdu;
		byte_count = ppdu_user->mpdu_ok_byte_count +
			ppdu_user->mpdu_err_byte_count;

		pkt_bw_offset = dp_get_bw_offset_frm_bw(soc, ppdu->u.bw);
		DP_STATS_UPD(mon_peer, rx.snr, (ppdu->rssi + pkt_bw_offset));

		if (qdf_unlikely(mon_peer->stats.rx.avg_snr == CDP_INVALID_SNR))
			mon_peer->stats.rx.avg_snr =
				CDP_SNR_IN(mon_peer->stats.rx.snr);
		else
			CDP_SNR_UPDATE_AVG(mon_peer->stats.rx.avg_snr,
					   mon_peer->stats.rx.snr);

		if (ppdu_type == HAL_RX_TYPE_SU) {
			if (nss) {
				DP_STATS_INC(mon_peer, rx.nss[nss - 1], num_msdu);
				DP_STATS_INC(mon_peer, rx.ppdu_nss[nss - 1], 1);
			}

			DP_STATS_INC(mon_peer, rx.mpdu_cnt_fcs_ok,
				     ppdu_user->mpdu_cnt_fcs_ok);
			DP_STATS_INC(mon_peer, rx.mpdu_cnt_fcs_err,
				     ppdu_user->mpdu_cnt_fcs_err);
		}

		if (ppdu_type >= HAL_RX_TYPE_MU_MIMO &&
		    ppdu_type <= HAL_RX_TYPE_MU_OFDMA) {
			if (ppdu_type == HAL_RX_TYPE_MU_MIMO)
				mu_pkt_type = TXRX_TYPE_MU_MIMO;
			else
				mu_pkt_type = TXRX_TYPE_MU_OFDMA;

			if (qdf_likely(nss)) {
				DP_STATS_INC(mon_peer, rx.nss[nss - 1], num_msdu);
				DP_STATS_INC(mon_peer,
					rx.rx_mu[mu_pkt_type].ppdu_nss[nss - 1],
					1);
			}

			DP_STATS_INC(mon_peer,
				     rx.rx_mu[mu_pkt_type].mpdu_cnt_fcs_ok,
				     ppdu_user->mpdu_cnt_fcs_ok);
			DP_STATS_INC(mon_peer,
				     rx.rx_mu[mu_pkt_type].mpdu_cnt_fcs_err,
				     ppdu_user->mpdu_cnt_fcs_err);
		}

		DP_STATS_INC(mon_peer, rx.sgi_count[ppdu->u.gi], num_msdu);
		DP_STATS_INC(mon_peer, rx.bw[ppdu->u.bw], num_msdu);
		DP_STATS_INC(mon_peer, rx.reception_type[ppdu->u.ppdu_type],
			     num_msdu);
		DP_STATS_INC(mon_peer, rx.ppdu_cnt[ppdu->u.ppdu_type], 1);
		DP_STATS_INCC(mon_peer, rx.ampdu_cnt, num_msdu,
			      ppdu_user->is_ampdu);
		DP_STATS_INCC(mon_peer, rx.non_ampdu_cnt, num_msdu,
			      !(ppdu_user->is_ampdu));
		DP_STATS_UPD(mon_peer, rx.rx_rate, mcs);
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
			((mcs >= MAX_MCS_11A) && (preamble == DOT11_A)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[mcs], num_msdu,
			((mcs < MAX_MCS_11A) && (preamble == DOT11_A)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
			((mcs >= MAX_MCS_11B) && (preamble == DOT11_B)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[mcs], num_msdu,
			((mcs < MAX_MCS_11B) && (preamble == DOT11_B)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
			((mcs >= MAX_MCS_11A) && (preamble == DOT11_N)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[mcs], num_msdu,
			((mcs < MAX_MCS_11A) && (preamble == DOT11_N)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
			((mcs >= MAX_MCS_11AC) && (preamble == DOT11_AC)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[mcs], num_msdu,
			((mcs < MAX_MCS_11AC) && (preamble == DOT11_AC)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
			((mcs >= (MAX_MCS_11AX)) && (preamble == DOT11_AX)));
		DP_STATS_INCC(mon_peer,
			rx.pkt_type[preamble].mcs_count[mcs], num_msdu,
			((mcs < (MAX_MCS_11AX)) && (preamble == DOT11_AX)));
		DP_STATS_INCC(mon_peer,
			rx.su_ax_ppdu_cnt.mcs_count[MAX_MCS - 1], 1,
			((mcs >= (MAX_MCS_11AX)) && (preamble == DOT11_AX) &&
			(ppdu_type == HAL_RX_TYPE_SU)));
		DP_STATS_INCC(mon_peer,
			rx.su_ax_ppdu_cnt.mcs_count[mcs], 1,
			((mcs < (MAX_MCS_11AX)) && (preamble == DOT11_AX) &&
			(ppdu_type == HAL_RX_TYPE_SU)));
		DP_STATS_INCC(mon_peer,
			rx.rx_mu[TXRX_TYPE_MU_OFDMA].ppdu.mcs_count[MAX_MCS - 1],
			1, ((mcs >= (MAX_MCS_11AX)) &&
			(preamble == DOT11_AX) &&
			(ppdu_type == HAL_RX_TYPE_MU_OFDMA)));
		DP_STATS_INCC(mon_peer,
			rx.rx_mu[TXRX_TYPE_MU_OFDMA].ppdu.mcs_count[mcs],
			1, ((mcs < (MAX_MCS_11AX)) &&
			(preamble == DOT11_AX) &&
			(ppdu_type == HAL_RX_TYPE_MU_OFDMA)));
		DP_STATS_INCC(mon_peer,
			rx.rx_mu[TXRX_TYPE_MU_MIMO].ppdu.mcs_count[MAX_MCS - 1],
			1, ((mcs >= (MAX_MCS_11AX)) &&
			(preamble == DOT11_AX) &&
			(ppdu_type == HAL_RX_TYPE_MU_MIMO)));
		DP_STATS_INCC(mon_peer,
			rx.rx_mu[TXRX_TYPE_MU_MIMO].ppdu.mcs_count[mcs],
			1, ((mcs < (MAX_MCS_11AX)) &&
			(preamble == DOT11_AX) &&
			(ppdu_type == HAL_RX_TYPE_MU_MIMO)));

		/*
		 * If invalid TID, it could be a non-qos frame, hence do not
		 * update any AC counters
		 */
		ac = TID_TO_WME_AC(ppdu_user->tid);

		if (qdf_likely(ppdu->tid != HAL_TID_INVALID)) {
			DP_STATS_INC(mon_peer, rx.wme_ac_type[ac], num_msdu);
			DP_STATS_INC(mon_peer, rx.wme_ac_type_bytes[ac],
				     byte_count);
		}

		DP_STATS_INC(mon_peer, rx.rx_ppdus, 1);
		DP_STATS_INC(mon_peer, rx.rx_mpdus,
			(ppdu_user->mpdu_cnt_fcs_ok + ppdu_user->mpdu_cnt_fcs_err));

		mon_ops = dp_mon_ops_get(soc);
		if (qdf_likely(mon_ops && mon_ops->mon_rx_stats_update))
			mon_ops->mon_rx_stats_update(mon_peer, ppdu, ppdu_user);

		if (qdf_unlikely(!peer))
			continue;

		dp_peer_stats_notify(pdev, peer);
		DP_STATS_UPD(mon_peer, rx.last_snr,
			     (ppdu->rssi + pkt_bw_offset));

		dp_peer_qos_stats_notify(pdev, ppdu_user);

		if (dp_is_subtype_data(ppdu->frame_ctrl))
			dp_rx_rate_stats_update(peer, ppdu, i);

		dp_send_stats_event(pdev, peer, ppdu_user->peer_id);

		dp_ppdu_desc_user_rx_time_update(pdev, peer, ppdu, ppdu_user);
		dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);
	}
}

void
dp_rx_handle_ppdu_stats(struct dp_soc *soc, struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info)
{
	qdf_nbuf_t ppdu_nbuf;
	struct cdp_rx_indication_ppdu *cdp_rx_ppdu;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	uint64_t size = 0;
	uint8_t num_users = 0;

	/*
	 * Do not allocate if fcs error,
	 * ast idx invalid / fctl invalid
	 *
	 * In CFR RCC mode - PPDU status TLVs of error pkts are also needed
	 */
	if (qdf_unlikely(ppdu_info->com_info.mpdu_cnt_fcs_ok == 0))
		return;

	if (qdf_unlikely(mon_pdev->neighbour_peers_added)) {
		if (ppdu_info->nac_info.fc_valid &&
		    ppdu_info->nac_info.to_ds_flag &&
		    ppdu_info->nac_info.mac_addr2_valid) {
			struct dp_neighbour_peer *peer = NULL;
			uint8_t rssi = ppdu_info->rx_status.rssi_comb;

			qdf_spin_lock_bh(&mon_pdev->neighbour_peer_mutex);
			if (mon_pdev->neighbour_peers_added) {
				TAILQ_FOREACH(peer, &mon_pdev->neighbour_peers_list,
					      neighbour_peer_list_elem) {
					if (!qdf_mem_cmp(&peer->neighbour_peers_macaddr,
							 &ppdu_info->nac_info.mac_addr2,
							 QDF_MAC_ADDR_SIZE)) {
						peer->rssi = rssi;
						break;
					}
				}
			}
			qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);
		} else {
			dp_info("Neighbour peers RSSI update failed! fc_valid = %d, to_ds_flag = %d and mac_addr2_valid = %d",
					ppdu_info->nac_info.fc_valid,
					ppdu_info->nac_info.to_ds_flag,
					ppdu_info->nac_info.mac_addr2_valid);
		}
	}

	/* need not generate wdi event when mcopy, cfr rcc mode and
	 * enhanced stats are not enabled
	 */
	if (qdf_unlikely(!mon_pdev->mcopy_mode &&
			 !mon_pdev->enhanced_stats_en &&
			 !dp_cfr_rcc_mode_status(pdev)))
		return;

	if (qdf_unlikely(dp_cfr_rcc_mode_status(pdev)))
		dp_update_cfr_dbg_stats(pdev, ppdu_info);

	if (qdf_unlikely(!ppdu_info->rx_status.frame_control_info_valid ||
			 ppdu_info->rx_status.ast_index == HAL_AST_IDX_INVALID)) {
		if (!(mon_pdev->mcopy_mode ||
		      (dp_bb_captured_chan_status(pdev, ppdu_info) ==
		       QDF_STATUS_SUCCESS)))
			return;
	}
	num_users = ppdu_info->com_info.num_users;
	qdf_assert_always(num_users <= CDP_MU_MAX_USERS);
	size = sizeof(struct cdp_rx_indication_ppdu) +
		num_users * sizeof(struct cdp_rx_stats_ppdu_user);
	ppdu_nbuf = qdf_nbuf_alloc(soc->osdev,
				   size,
				   0, 0, FALSE);
	if (qdf_likely(ppdu_nbuf)) {
		cdp_rx_ppdu = (struct cdp_rx_indication_ppdu *)qdf_nbuf_data(ppdu_nbuf);

		qdf_mem_zero(cdp_rx_ppdu, size);
		dp_rx_mon_populate_cfr_info(pdev, ppdu_info, cdp_rx_ppdu);
		dp_rx_populate_cdp_indication_ppdu(pdev,
						   ppdu_info, cdp_rx_ppdu);
		if (!qdf_unlikely(qdf_nbuf_put_tail(ppdu_nbuf,
				       sizeof(struct cdp_rx_indication_ppdu))))
			return;

		dp_rx_stats_update(pdev, cdp_rx_ppdu);

		if (qdf_unlikely(cdp_rx_ppdu->peer_id != HTT_INVALID_PEER)) {
			dp_wdi_event_handler(WDI_EVENT_RX_PPDU_DESC,
					     soc, ppdu_nbuf,
					     cdp_rx_ppdu->peer_id,
					     WDI_NO_VAL, pdev->pdev_id);
		} else if (qdf_unlikely(mon_pdev->mcopy_mode || dp_cfr_rcc_mode_status(pdev))) {
			dp_wdi_event_handler(WDI_EVENT_RX_PPDU_DESC, soc,
					     ppdu_nbuf, HTT_INVALID_PEER,
					     WDI_NO_VAL, pdev->pdev_id);
		} else {
			qdf_nbuf_free(ppdu_nbuf);
		}
	}
}
#endif/* QCA_ENHANCED_STATS_SUPPORT */

#ifdef QCA_UNDECODED_METADATA_SUPPORT
#define RX_PHYERR_MASK_GET64(_val1, _val2) (((uint64_t)(_val2) << 32) | (_val1))
/**
 * dp_rx_populate_cdp_indication_ppdu_undecoded_metadata() - Populate cdp
 * rx indication structure
 * @pdev: pdev ctx
 * @ppdu_info: ppdu info structure from ppdu ring
 * @cdp_rx_ppdu: Rx PPDU indication structure
 *
 * Return: none
 */
static void
dp_rx_populate_cdp_indication_ppdu_undecoded_metadata(struct dp_pdev *pdev,
				struct hal_rx_ppdu_info *ppdu_info,
				struct cdp_rx_indication_ppdu *cdp_rx_ppdu)
{
	uint32_t chain;

	cdp_rx_ppdu->phyrx_abort = ppdu_info->rx_status.phyrx_abort;
	cdp_rx_ppdu->phyrx_abort_reason =
		ppdu_info->rx_status.phyrx_abort_reason;

	cdp_rx_ppdu->first_data_seq_ctrl =
		ppdu_info->rx_status.first_data_seq_ctrl;
	cdp_rx_ppdu->frame_ctrl =
		ppdu_info->rx_status.frame_control;
	cdp_rx_ppdu->tcp_msdu_count = ppdu_info->rx_status.tcp_msdu_count;
	cdp_rx_ppdu->udp_msdu_count = ppdu_info->rx_status.udp_msdu_count;
	cdp_rx_ppdu->other_msdu_count = ppdu_info->rx_status.other_msdu_count;
	cdp_rx_ppdu->u.preamble = ppdu_info->rx_status.preamble_type;
	cdp_rx_ppdu->num_mpdu = ppdu_info->com_info.mpdu_cnt_fcs_ok;
	cdp_rx_ppdu->num_msdu = (cdp_rx_ppdu->tcp_msdu_count +
				 cdp_rx_ppdu->udp_msdu_count +
				 cdp_rx_ppdu->other_msdu_count);

	cdp_rx_ppdu->retries = CDP_FC_IS_RETRY_SET(cdp_rx_ppdu->frame_ctrl) ?
		ppdu_info->com_info.mpdu_cnt_fcs_ok : 0;

	if (ppdu_info->com_info.mpdu_cnt_fcs_ok > 1)
		cdp_rx_ppdu->is_ampdu = 1;
	else
		cdp_rx_ppdu->is_ampdu = 0;
	cdp_rx_ppdu->tid = ppdu_info->rx_status.tid;

	cdp_rx_ppdu->ppdu_id = ppdu_info->com_info.ppdu_id;
	cdp_rx_ppdu->length = ppdu_info->rx_status.ppdu_len;
	cdp_rx_ppdu->duration = ppdu_info->rx_status.duration;
	cdp_rx_ppdu->u.bw = ppdu_info->rx_status.bw;
	cdp_rx_ppdu->u.nss = ppdu_info->rx_status.nss;
	cdp_rx_ppdu->u.mcs = ppdu_info->rx_status.mcs;
	if (ppdu_info->rx_status.sgi == VHT_SGI_NYSM &&
	    ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AC)
		cdp_rx_ppdu->u.gi = CDP_SGI_0_4_US;
	else
		cdp_rx_ppdu->u.gi = ppdu_info->rx_status.sgi;

	cdp_rx_ppdu->u.ldpc = ppdu_info->rx_status.ldpc;
	cdp_rx_ppdu->u.ppdu_type = ppdu_info->rx_status.reception_type;
	cdp_rx_ppdu->u.ltf_size = (ppdu_info->rx_status.he_data5 >>
				   QDF_MON_STATUS_HE_LTF_SIZE_SHIFT) & 0x3;

	cdp_rx_ppdu->rssi = ppdu_info->rx_status.rssi_comb;
	cdp_rx_ppdu->timestamp = ppdu_info->rx_status.tsft;
	cdp_rx_ppdu->channel = ppdu_info->rx_status.chan_num;
	cdp_rx_ppdu->beamformed = ppdu_info->rx_status.beamformed;
	cdp_rx_ppdu->num_bytes = ppdu_info->rx_status.ppdu_len;
	cdp_rx_ppdu->lsig_a = ppdu_info->rx_status.rate;
	cdp_rx_ppdu->u.ltf_size = ppdu_info->rx_status.ltf_size;

	if (ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AC) {
		cdp_rx_ppdu->u.stbc = ppdu_info->rx_status.is_stbc;
		cdp_rx_ppdu->vht_no_txop_ps =
			ppdu_info->rx_status.vht_no_txop_ps;
		cdp_rx_ppdu->vht_crc = ppdu_info->rx_status.vht_crc;
		cdp_rx_ppdu->group_id = ppdu_info->rx_status.vht_flag_values5;
	} else if (ppdu_info->rx_status.preamble_type ==
			HAL_RX_PKT_TYPE_11AX) {
		cdp_rx_ppdu->u.stbc = (ppdu_info->rx_status.he_data3 >>
				       QDF_MON_STATUS_STBC_SHIFT) & 0x1;
		cdp_rx_ppdu->u.dcm = (ppdu_info->rx_status.he_data3 >>
				      QDF_MON_STATUS_DCM_SHIFT) & 0x1;
	} else {
		cdp_rx_ppdu->u.stbc = ppdu_info->rx_status.ht_stbc;
		cdp_rx_ppdu->ht_length = ppdu_info->rx_status.ht_length;
		cdp_rx_ppdu->ht_smoothing = ppdu_info->rx_status.smoothing;
		cdp_rx_ppdu->ht_not_sounding =
			ppdu_info->rx_status.not_sounding;
		cdp_rx_ppdu->ht_aggregation = ppdu_info->rx_status.aggregation;
		cdp_rx_ppdu->ht_stbc = ppdu_info->rx_status.ht_stbc;
		cdp_rx_ppdu->ht_crc = ppdu_info->rx_status.ht_crc;
	}

	cdp_rx_ppdu->l_sig_length = ppdu_info->rx_status.l_sig_length;
	cdp_rx_ppdu->l_sig_a_parity = ppdu_info->rx_status.l_sig_a_parity;
	cdp_rx_ppdu->l_sig_a_pkt_type = ppdu_info->rx_status.l_sig_a_pkt_type;

	if (ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11AX) {
		cdp_rx_ppdu->he_crc = ppdu_info->rx_status.he_crc;
		cdp_rx_ppdu->bss_color_id =
			ppdu_info->rx_status.he_data3 & 0x3F;
		cdp_rx_ppdu->beam_change = (ppdu_info->rx_status.he_data3 >>
				QDF_MON_STATUS_BEAM_CHANGE_SHIFT) & 0x1;
		cdp_rx_ppdu->dl_ul_flag = (ppdu_info->rx_status.he_data3 >>
		QDF_MON_STATUS_DL_UL_SHIFT) & 0x1;
		cdp_rx_ppdu->ldpc_extra_sym = (ppdu_info->rx_status.he_data3 >>
				QDF_MON_STATUS_LDPC_EXTRA_SYMBOL_SHIFT) & 0x1;
		cdp_rx_ppdu->special_reuse =
			ppdu_info->rx_status.he_data4 & 0xF;
		cdp_rx_ppdu->ltf_sym = (ppdu_info->rx_status.he_data5 >>
				QDF_MON_STATUS_HE_LTF_SYM_SHIFT) & 0x7;
		cdp_rx_ppdu->txbf = (ppdu_info->rx_status.he_data5 >>
				QDF_MON_STATUS_TXBF_SHIFT) & 0x1;
		cdp_rx_ppdu->pe_disambiguity = (ppdu_info->rx_status.he_data5 >>
				QDF_MON_STATUS_PE_DISAMBIGUITY_SHIFT) & 0x1;
		cdp_rx_ppdu->pre_fec_pad = (ppdu_info->rx_status.he_data5 >>
				QDF_MON_STATUS_PRE_FEC_PAD_SHIFT) & 0x3;
		cdp_rx_ppdu->dopplar = (ppdu_info->rx_status.he_data6 >>
				QDF_MON_STATUS_DOPPLER_SHIFT) & 0x1;
		cdp_rx_ppdu->txop_duration = (ppdu_info->rx_status.he_data6 >>
				QDF_MON_STATUS_TXOP_SHIFT) & 0x7F;
		cdp_rx_ppdu->sig_b_mcs = ppdu_info->rx_status.he_flags1 & 0x7;
		cdp_rx_ppdu->sig_b_dcm = (ppdu_info->rx_status.he_flags1 >>
				QDF_MON_STATUS_DCM_FLAG_1_SHIFT) & 0x1;
		cdp_rx_ppdu->sig_b_sym = (ppdu_info->rx_status.he_flags2 >>
				QDF_MON_STATUS_NUM_SIG_B_SYMBOLS_SHIFT) & 0xF;
		cdp_rx_ppdu->sig_b_comp = (ppdu_info->rx_status.he_flags2 >>
			QDF_MON_STATUS_SIG_B_COMPRESSION_FLAG_2_SHIFT) & 0x1;
	}
	dp_rx_populate_rx_rssi_chain(ppdu_info, cdp_rx_ppdu, pdev);
	dp_rx_populate_su_evm_details(ppdu_info, cdp_rx_ppdu);
	cdp_rx_ppdu->rx_antenna = ppdu_info->rx_status.rx_antenna;

	cdp_rx_ppdu->nf = ppdu_info->rx_status.chan_noise_floor;
	for (chain = 0; chain < MAX_CHAIN; chain++)
		cdp_rx_ppdu->per_chain_rssi[chain] =
			ppdu_info->rx_status.rssi[chain];

	cdp_rx_ppdu->is_mcast_bcast = ppdu_info->nac_info.mcast_bcast;

	cdp_rx_ppdu->num_users = ppdu_info->com_info.num_users;

	dp_rx_populate_cdp_indication_ppdu_user(pdev, ppdu_info, cdp_rx_ppdu);
}

/**
 * dp_rx_is_valid_undecoded_frame() - Check unencoded frame received valid
 * or not against configured error mask
 * @err_mask: configured err mask
 * @err_code: Received error reason code for phy abort
 *
 * Return: true / false
 */
static inline bool
dp_rx_is_valid_undecoded_frame(uint64_t err_mask, uint8_t err_code)
{
	if (err_code < CDP_PHYRX_ERR_MAX &&
	    (err_mask & (1L << err_code)))
		return true;

	return false;
}

void
dp_rx_handle_ppdu_undecoded_metadata(struct dp_soc *soc, struct dp_pdev *pdev,
				     struct hal_rx_ppdu_info *ppdu_info)
{
	qdf_nbuf_t ppdu_nbuf;
	struct cdp_rx_indication_ppdu *cdp_rx_ppdu;
	uint8_t abort_reason = 0;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	uint64_t mask64;

	 /* Return if RX_ABORT not set */
	if (ppdu_info->rx_status.phyrx_abort == 0)
		return;

	mask64 = RX_PHYERR_MASK_GET64(mon_pdev->phyrx_error_mask,
				      mon_pdev->phyrx_error_mask_cont);
	abort_reason = ppdu_info->rx_status.phyrx_abort_reason;

	if (!dp_rx_is_valid_undecoded_frame(mask64, abort_reason))
		return;

	ppdu_nbuf = qdf_nbuf_alloc(soc->osdev,
				   sizeof(struct cdp_rx_indication_ppdu),
				   0, 0, FALSE);
	if (ppdu_nbuf) {
		cdp_rx_ppdu = ((struct cdp_rx_indication_ppdu *)
				qdf_nbuf_data(ppdu_nbuf));

		qdf_mem_zero(cdp_rx_ppdu,
			     sizeof(struct cdp_rx_indication_ppdu));
		dp_rx_populate_cdp_indication_ppdu_undecoded_metadata(pdev,
				ppdu_info, cdp_rx_ppdu);

		if (!qdf_nbuf_put_tail(ppdu_nbuf,
				       sizeof(struct cdp_rx_indication_ppdu))) {
			return;
		}

		mon_pdev->rx_mon_stats.rx_undecoded_count++;
		mon_pdev->rx_mon_stats.rx_undecoded_error[abort_reason] += 1;

		dp_wdi_event_handler(WDI_EVENT_RX_PPDU_DESC_UNDECODED_METADATA,
				     soc, ppdu_nbuf, HTT_INVALID_PEER,
				     WDI_NO_VAL, pdev->pdev_id);
	}
}
#endif/* QCA_UNDECODED_METADATA_SUPPORT */

#ifdef QCA_MCOPY_SUPPORT
QDF_STATUS
dp_rx_handle_mcopy_mode(struct dp_soc *soc, struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info, qdf_nbuf_t nbuf,
			uint8_t fcs_ok_mpdu_cnt, bool deliver_frame)
{
	uint16_t size = 0;
	struct ieee80211_frame *wh;
	uint32_t *nbuf_data;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!ppdu_info->ppdu_msdu_info[fcs_ok_mpdu_cnt].first_msdu_payload)
		return QDF_STATUS_SUCCESS;

	/* For M_COPY mode only one msdu per ppdu is sent to upper layer*/
	if (mon_pdev->mcopy_mode == M_COPY) {
		if (mon_pdev->m_copy_id.rx_ppdu_id == ppdu_info->com_info.ppdu_id)
			return QDF_STATUS_SUCCESS;
	}

	wh = (struct ieee80211_frame *)(ppdu_info->ppdu_msdu_info[fcs_ok_mpdu_cnt].first_msdu_payload + 4);

	size = (ppdu_info->ppdu_msdu_info[fcs_ok_mpdu_cnt].first_msdu_payload -
				qdf_nbuf_data(nbuf));

	if (qdf_nbuf_pull_head(nbuf, size) == NULL)
		return QDF_STATUS_SUCCESS;

	if (((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	     IEEE80211_FC0_TYPE_MGT) ||
	     ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	     IEEE80211_FC0_TYPE_CTL)) {
		return QDF_STATUS_SUCCESS;
	}

	nbuf_data = (uint32_t *)qdf_nbuf_data(nbuf);
	*nbuf_data = mon_pdev->ppdu_info.com_info.ppdu_id;
	/* only retain RX MSDU payload in the skb */
	qdf_nbuf_trim_tail(nbuf, qdf_nbuf_len(nbuf) - ppdu_info->ppdu_msdu_info[fcs_ok_mpdu_cnt].payload_len);
	if (deliver_frame) {
		mon_pdev->m_copy_id.rx_ppdu_id = ppdu_info->com_info.ppdu_id;
		dp_wdi_event_handler(WDI_EVENT_RX_DATA, soc,
				     nbuf, HTT_INVALID_PEER,
				     WDI_NO_VAL, pdev->pdev_id);
	}
	return QDF_STATUS_E_ALREADY;
}

void
dp_rx_mcopy_handle_last_mpdu(struct dp_soc *soc, struct dp_pdev *pdev,
			     struct hal_rx_ppdu_info *ppdu_info,
			     qdf_nbuf_t status_nbuf)
{
	QDF_STATUS mcopy_status;
	qdf_nbuf_t nbuf_clone = NULL;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/* If the MPDU end tlv and RX header are received in different buffers,
	 * process the RX header based on fcs status.
	 */
	if (mon_pdev->mcopy_status_nbuf) {
		/* For M_COPY mode only one msdu per ppdu is sent to upper layer*/
		if (mon_pdev->mcopy_mode == M_COPY) {
			if (mon_pdev->m_copy_id.rx_ppdu_id ==
			    ppdu_info->com_info.ppdu_id)
				goto end1;
		}

		if (ppdu_info->is_fcs_passed) {
			nbuf_clone = qdf_nbuf_clone(mon_pdev->mcopy_status_nbuf);
			if (!nbuf_clone) {
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "Failed to clone nbuf");
				goto end1;
			}

			mon_pdev->m_copy_id.rx_ppdu_id = ppdu_info->com_info.ppdu_id;
			dp_wdi_event_handler(WDI_EVENT_RX_DATA, soc,
					     nbuf_clone,
					     HTT_INVALID_PEER,
					     WDI_NO_VAL, pdev->pdev_id);
			ppdu_info->is_fcs_passed = false;
		}
end1:
		qdf_nbuf_free(mon_pdev->mcopy_status_nbuf);
		mon_pdev->mcopy_status_nbuf = NULL;
	}

	/* If the MPDU end tlv and RX header are received in different buffers,
	 * preserve the RX header as the fcs status will be received in MPDU
	 * end tlv in next buffer. So, cache the buffer to be processd in next
	 * iteration
	 */
	if ((ppdu_info->fcs_ok_cnt + ppdu_info->fcs_err_cnt) !=
	    ppdu_info->com_info.mpdu_cnt) {
		mon_pdev->mcopy_status_nbuf = qdf_nbuf_clone(status_nbuf);
		if (mon_pdev->mcopy_status_nbuf) {
			mcopy_status = dp_rx_handle_mcopy_mode(
							soc, pdev,
							ppdu_info,
							mon_pdev->mcopy_status_nbuf,
							ppdu_info->fcs_ok_cnt,
							false);
			if (mcopy_status == QDF_STATUS_SUCCESS) {
				qdf_nbuf_free(mon_pdev->mcopy_status_nbuf);
				mon_pdev->mcopy_status_nbuf = NULL;
			}
		}
	}
}

void
dp_rx_mcopy_process_ppdu_info(struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info,
			      uint32_t tlv_status)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (qdf_unlikely(!mon_pdev->mcopy_mode))
		return;

	/* The fcs status is received in MPDU end tlv. If the RX header
	 * and its MPDU end tlv are received in different status buffer then
	 * to process that header ppdu_info->is_fcs_passed is used.
	 * If end tlv is received in next status buffer then com_info.mpdu_cnt
	 * will be 0 at the time of receiving MPDU end tlv and we update the
	 * is_fcs_passed flag based on ppdu_info->fcs_err.
	 */
	if (tlv_status != HAL_TLV_STATUS_MPDU_END)
		return;

	if (!ppdu_info->fcs_err) {
		if (ppdu_info->fcs_ok_cnt >
		    HAL_RX_MAX_MPDU_H_PER_STATUS_BUFFER) {
			dp_err("No. of MPDUs(%d) per status buff exceeded",
					ppdu_info->fcs_ok_cnt);
			return;
		}
		if (ppdu_info->com_info.mpdu_cnt)
			ppdu_info->fcs_ok_cnt++;
		else
			ppdu_info->is_fcs_passed = true;
	} else {
		if (ppdu_info->com_info.mpdu_cnt)
			ppdu_info->fcs_err_cnt++;
		else
			ppdu_info->is_fcs_passed = false;
	}
}

void
dp_rx_process_mcopy_mode(struct dp_soc *soc, struct dp_pdev *pdev,
			 struct hal_rx_ppdu_info *ppdu_info,
			 uint32_t tlv_status,
			 qdf_nbuf_t status_nbuf)
{
	QDF_STATUS mcopy_status;
	qdf_nbuf_t nbuf_clone = NULL;
	uint8_t fcs_ok_mpdu_cnt = 0;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	dp_rx_mcopy_handle_last_mpdu(soc, pdev, ppdu_info, status_nbuf);

	if (qdf_unlikely(!ppdu_info->com_info.mpdu_cnt))
		goto end;

	if (qdf_unlikely(!ppdu_info->fcs_ok_cnt))
		goto end;

	/* For M_COPY mode only one msdu per ppdu is sent to upper layer*/
	if (mon_pdev->mcopy_mode == M_COPY)
		ppdu_info->fcs_ok_cnt = 1;

	while (fcs_ok_mpdu_cnt < ppdu_info->fcs_ok_cnt) {
		nbuf_clone = qdf_nbuf_clone(status_nbuf);
		if (!nbuf_clone) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
				  "Failed to clone nbuf");
			goto end;
		}

		mcopy_status = dp_rx_handle_mcopy_mode(soc, pdev,
						       ppdu_info,
						       nbuf_clone,
						       fcs_ok_mpdu_cnt,
						       true);

		if (mcopy_status == QDF_STATUS_SUCCESS)
			qdf_nbuf_free(nbuf_clone);

		fcs_ok_mpdu_cnt++;
	}
end:
	qdf_nbuf_free(status_nbuf);
	ppdu_info->fcs_ok_cnt = 0;
	ppdu_info->fcs_err_cnt = 0;
	ppdu_info->com_info.mpdu_cnt = 0;
	qdf_mem_zero(&ppdu_info->ppdu_msdu_info,
		     HAL_RX_MAX_MPDU_H_PER_STATUS_BUFFER
		     * sizeof(struct hal_rx_msdu_payload_info));
}
#endif /* QCA_MCOPY_SUPPORT */

int
dp_rx_handle_smart_mesh_mode(struct dp_soc *soc, struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info,
			      qdf_nbuf_t nbuf)
{
	uint8_t size = 0;
	struct dp_mon_vdev *mon_vdev;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev->mvdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "[%s]:[%d] Monitor vdev is NULL !!",
			  __func__, __LINE__);
		return 1;
	}

	mon_vdev = mon_pdev->mvdev->monitor_vdev;

	if (!ppdu_info->msdu_info.first_msdu_payload) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "[%s]:[%d] First msdu payload not present",
			  __func__, __LINE__);
		return 1;
	}

	/* Adding 4 bytes to get to start of 802.11 frame after phy_ppdu_id */
	size = (ppdu_info->msdu_info.first_msdu_payload -
		qdf_nbuf_data(nbuf)) + 4;
	ppdu_info->msdu_info.first_msdu_payload = NULL;

	if (!qdf_nbuf_pull_head(nbuf, size)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "[%s]:[%d] No header present",
			  __func__, __LINE__);
		return 1;
	}

	/* Only retain RX MSDU payload in the skb */
	qdf_nbuf_trim_tail(nbuf, qdf_nbuf_len(nbuf) -
			   ppdu_info->msdu_info.payload_len);
	if (!qdf_nbuf_update_radiotap(&mon_pdev->ppdu_info.rx_status, nbuf,
				      qdf_nbuf_headroom(nbuf))) {
		DP_STATS_INC(pdev, dropped.mon_radiotap_update_err, 1);
		return 1;
	}

	mon_vdev->osif_rx_mon(mon_pdev->mvdev->osif_vdev,
			      nbuf, NULL);
	mon_pdev->ppdu_info.rx_status.monitor_direct_used = 0;
	return 0;
}

qdf_nbuf_t
dp_rx_nbuf_prepare(struct dp_soc *soc, struct dp_pdev *pdev)
{
	uint8_t *buf;
	int32_t nbuf_retry_count;
	QDF_STATUS ret;
	qdf_nbuf_t nbuf = NULL;

	for (nbuf_retry_count = 0; nbuf_retry_count <
		QDF_NBUF_ALLOC_MAP_RETRY_THRESHOLD;
			nbuf_retry_count++) {
		/* Allocate a new skb using alloc_skb */
		nbuf = qdf_nbuf_alloc_no_recycler(RX_MON_STATUS_BUF_SIZE,
						  RX_MON_STATUS_BUF_RESERVATION,
						  RX_DATA_BUFFER_ALIGNMENT);

		if (!nbuf) {
			DP_STATS_INC(pdev, replenish.nbuf_alloc_fail, 1);
			continue;
		}

		buf = qdf_nbuf_data(nbuf);

		memset(buf, 0, RX_MON_STATUS_BUF_SIZE);

		ret = qdf_nbuf_map_nbytes_single(soc->osdev, nbuf,
						 QDF_DMA_FROM_DEVICE,
						 RX_MON_STATUS_BUF_SIZE);

		/* nbuf map failed */
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
			qdf_nbuf_free(nbuf);
			DP_STATS_INC(pdev, replenish.map_err, 1);
			continue;
		}
		/* qdf_nbuf alloc and map succeeded */
		break;
	}

	/* qdf_nbuf still alloc or map failed */
	if (qdf_unlikely(nbuf_retry_count >=
			QDF_NBUF_ALLOC_MAP_RETRY_THRESHOLD))
		return NULL;

	return nbuf;
}

#ifndef DISABLE_MON_CONFIG
uint32_t
dp_mon_process(struct dp_soc *soc, struct dp_intr *int_ctx,
	       uint32_t mac_id, uint32_t quota)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (mon_soc && mon_soc->mon_rx_process)
		return mon_soc->mon_rx_process(soc, int_ctx,
					       mac_id, quota);
	return 0;
}
#else
uint32_t
dp_mon_process(struct dp_soc *soc, struct dp_intr *int_ctx,
	       uint32_t mac_id, uint32_t quota)
{
	return 0;
}
#endif

/**
 * dp_send_mgmt_packet_to_stack(): send indicataion to upper layers
 *
 * @soc: soc handle
 * @nbuf: Mgmt packet
 * @pdev: pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_INVAL in error
 */
#ifdef QCA_MCOPY_SUPPORT
static inline QDF_STATUS
dp_send_mgmt_packet_to_stack(struct dp_soc *soc,
			     qdf_nbuf_t nbuf,
			     struct dp_pdev *pdev)
{
	uint32_t *nbuf_data;
	struct ieee80211_frame *wh;
	qdf_frag_t addr;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!nbuf)
		return QDF_STATUS_E_INVAL;

	/* Get addr pointing to80211 header */
	addr = dp_rx_mon_get_nbuf_80211_hdr(nbuf);
	if (qdf_unlikely(!addr)) {
		qdf_nbuf_free(nbuf);
		return QDF_STATUS_E_INVAL;
	}

	/*check if this is not a mgmt packet*/
	wh = (struct ieee80211_frame *)addr;
	if (((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	     IEEE80211_FC0_TYPE_MGT) &&
	     ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	     IEEE80211_FC0_TYPE_CTL)) {
		qdf_nbuf_free(nbuf);
		return QDF_STATUS_E_INVAL;
	}
	nbuf_data = (uint32_t *)qdf_nbuf_push_head(nbuf, 4);
	if (!nbuf_data) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_ERROR,
			  FL("No headroom"));
		qdf_nbuf_free(nbuf);
		return QDF_STATUS_E_INVAL;
	}
	*nbuf_data = mon_pdev->ppdu_info.com_info.ppdu_id;

	dp_wdi_event_handler(WDI_EVENT_RX_MGMT_CTRL, soc, nbuf,
			     HTT_INVALID_PEER,
			     WDI_NO_VAL, pdev->pdev_id);
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_send_mgmt_packet_to_stack(struct dp_soc *soc,
			     qdf_nbuf_t nbuf,
			     struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_MCOPY_SUPPORT */

QDF_STATUS dp_rx_mon_process_dest_pktlog(struct dp_soc *soc,
					 uint32_t mac_id,
					 qdf_nbuf_t mpdu)
{
	uint32_t event, msdu_timestamp = 0;
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	void *data;
	struct ieee80211_frame *wh;
	uint8_t type, subtype;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->rx_pktlog_cbf) {
		if (qdf_nbuf_get_nr_frags(mpdu))
			data = qdf_nbuf_get_frag_addr(mpdu, 0);
		else
			data = qdf_nbuf_data(mpdu);

		/* CBF logging required, doesn't matter if it is a full mode
		 * or lite mode.
		 * Need to look for mpdu with:
		 * TYPE = ACTION, SUBTYPE = NO ACK in the header
		 */
		event = WDI_EVENT_RX_CBF;

		wh = (struct ieee80211_frame *)data;
		type = (wh)->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
		subtype = (wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (type == IEEE80211_FC0_TYPE_MGT &&
		    subtype == IEEE80211_FCO_SUBTYPE_ACTION_NO_ACK) {
			msdu_timestamp = mon_pdev->ppdu_info.rx_status.tsft;
			dp_rx_populate_cbf_hdr(soc,
					       mac_id, event,
					       mpdu,
					       msdu_timestamp);
		}
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_mon_deliver(struct dp_soc *soc, uint32_t mac_id,
			     qdf_nbuf_t head_msdu, qdf_nbuf_t tail_msdu)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct cdp_mon_status *rs;
	qdf_nbuf_t mon_skb, skb_next;
	qdf_nbuf_t mon_mpdu = NULL;
	struct dp_mon_vdev *mon_vdev;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev)
		goto mon_deliver_fail;

	mon_pdev = pdev->monitor_pdev;
	rs = &mon_pdev->rx_mon_recv_status;

	if (!mon_pdev->mvdev && !mon_pdev->mcopy_mode &&
	    !mon_pdev->rx_pktlog_cbf)
		goto mon_deliver_fail;

	/* restitch mon MPDU for delivery via monitor interface */
	mon_mpdu = dp_rx_mon_restitch_mpdu(soc, mac_id, head_msdu,
					   tail_msdu, rs);

	/* If MPDU restitch fails, free buffers*/
	if (!mon_mpdu) {
		dp_info("MPDU restitch failed, free buffers");
		goto mon_deliver_fail;
	}

	dp_rx_mon_process_dest_pktlog(soc, mac_id, mon_mpdu);

	/* monitor vap cannot be present when mcopy is enabled
	 * hence same skb can be consumed
	 */
	if (mon_pdev->mcopy_mode)
		return dp_send_mgmt_packet_to_stack(soc, mon_mpdu, pdev);

	if (mon_mpdu && mon_pdev->mvdev &&
	    mon_pdev->mvdev->osif_vdev &&
	    mon_pdev->mvdev->monitor_vdev &&
	    mon_pdev->mvdev->monitor_vdev->osif_rx_mon) {
		mon_vdev = mon_pdev->mvdev->monitor_vdev;

		mon_pdev->ppdu_info.rx_status.ppdu_id =
			mon_pdev->ppdu_info.com_info.ppdu_id;
		mon_pdev->ppdu_info.rx_status.device_id = soc->device_id;
		mon_pdev->ppdu_info.rx_status.chan_noise_floor =
			pdev->chan_noise_floor;
		dp_handle_tx_capture(soc, pdev, mon_mpdu);

		if (!qdf_nbuf_update_radiotap(&mon_pdev->ppdu_info.rx_status,
					      mon_mpdu,
					      qdf_nbuf_headroom(mon_mpdu))) {
			DP_STATS_INC(pdev, dropped.mon_radiotap_update_err, 1);
			goto mon_deliver_fail;
		}

		dp_rx_mon_update_pf_tag_to_buf_headroom(soc, mon_mpdu);
		mon_vdev->osif_rx_mon(mon_pdev->mvdev->osif_vdev,
				      mon_mpdu,
				      &mon_pdev->ppdu_info.rx_status);
	} else {
		dp_rx_mon_dest_debug("%pK: mon_mpdu=%pK monitor_vdev %pK osif_vdev %pK"
				     , soc, mon_mpdu, mon_pdev->mvdev,
				     (mon_pdev->mvdev ? mon_pdev->mvdev->osif_vdev
				     : NULL));
		goto mon_deliver_fail;
	}

	return QDF_STATUS_SUCCESS;

mon_deliver_fail:
	mon_skb = head_msdu;
	while (mon_skb) {
		skb_next = qdf_nbuf_next(mon_skb);

		 dp_rx_mon_dest_debug("%pK: [%s][%d] mon_skb=%pK len %u",
				      soc,  __func__, __LINE__, mon_skb, mon_skb->len);

		qdf_nbuf_free(mon_skb);
		mon_skb = skb_next;
	}
	return QDF_STATUS_E_INVAL;
}

QDF_STATUS dp_rx_mon_deliver_non_std(struct dp_soc *soc,
				     uint32_t mac_id)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	ol_txrx_rx_mon_fp osif_rx_mon;
	qdf_nbuf_t dummy_msdu;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_vdev *mon_vdev;

	/* Sanity checking */
	if (!pdev || !pdev->monitor_pdev)
		goto mon_deliver_non_std_fail;

	mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev->mvdev || !mon_pdev->mvdev ||
	    !mon_pdev->mvdev->monitor_vdev ||
	    !mon_pdev->mvdev->monitor_vdev->osif_rx_mon)
		goto mon_deliver_non_std_fail;

	mon_vdev = mon_pdev->mvdev->monitor_vdev;
	/* Generate a dummy skb_buff */
	osif_rx_mon = mon_vdev->osif_rx_mon;
	dummy_msdu = qdf_nbuf_alloc(soc->osdev, MAX_MONITOR_HEADER,
				    MAX_MONITOR_HEADER, 4, FALSE);
	if (!dummy_msdu)
		goto allocate_dummy_msdu_fail;

	qdf_nbuf_set_pktlen(dummy_msdu, 0);
	qdf_nbuf_set_next(dummy_msdu, NULL);

	mon_pdev->ppdu_info.rx_status.ppdu_id =
		mon_pdev->ppdu_info.com_info.ppdu_id;

	/* Apply the radio header to this dummy skb */
	if (!qdf_nbuf_update_radiotap(&mon_pdev->ppdu_info.rx_status, dummy_msdu,
				      qdf_nbuf_headroom(dummy_msdu))) {
		DP_STATS_INC(pdev, dropped.mon_radiotap_update_err, 1);
		qdf_nbuf_free(dummy_msdu);
		goto mon_deliver_non_std_fail;
	}

	/* deliver to the user layer application */
	osif_rx_mon(mon_pdev->mvdev->osif_vdev,
		    dummy_msdu, NULL);

	/* Clear rx_status*/
	qdf_mem_zero(&mon_pdev->ppdu_info.rx_status,
		     sizeof(mon_pdev->ppdu_info.rx_status));
	mon_pdev->mon_ppdu_status = DP_PPDU_STATUS_START;

	return QDF_STATUS_SUCCESS;

allocate_dummy_msdu_fail:
		 dp_rx_mon_dest_debug("%pK: mon_skb=%pK ",
				      soc, dummy_msdu);

mon_deliver_non_std_fail:
	return QDF_STATUS_E_INVAL;
}

/**
 * dp_rx_process_peer_based_pktlog() - Process Rx pktlog if peer based
 *                                     filtering enabled
 * @soc: core txrx main context
 * @ppdu_info: Structure for rx ppdu info
 * @status_nbuf: Qdf nbuf abstraction for linux skb
 * @pdev_id: mac_id/pdev_id correspondinggly for MCL and WIN
 *
 * Return: none
 */
void
dp_rx_process_peer_based_pktlog(struct dp_soc *soc,
				struct hal_rx_ppdu_info *ppdu_info,
				qdf_nbuf_t status_nbuf, uint32_t pdev_id)
{
	struct dp_peer *peer;
	struct mon_rx_user_status *rx_user_status;
	uint32_t num_users = ppdu_info->com_info.num_users;
	uint16_t sw_peer_id;

	/* Sanity check for num_users */
	if (!num_users)
		return;

	qdf_assert_always(num_users <= CDP_MU_MAX_USERS);
	rx_user_status = &ppdu_info->rx_user_status[num_users - 1];

	sw_peer_id = rx_user_status->sw_peer_id;

	peer = dp_peer_get_ref_by_id(soc, sw_peer_id,
				     DP_MOD_ID_RX_PPDU_STATS);

	if (!peer)
		return;

	if ((peer->peer_id != HTT_INVALID_PEER) && (peer->monitor_peer) &&
	    (peer->monitor_peer->peer_based_pktlog_filter)) {
		dp_wdi_event_handler(
				     WDI_EVENT_RX_DESC, soc,
				     status_nbuf,
				     peer->peer_id,
				     WDI_NO_VAL, pdev_id);
	}
	dp_peer_unref_delete(peer,
			     DP_MOD_ID_RX_PPDU_STATS);
}

uint32_t
dp_mon_rx_add_tlv(uint8_t id, uint16_t len, void *value, qdf_nbuf_t mpdu_nbuf)
{
	uint8_t *dest = NULL;
	uint32_t num_bytes_pushed = 0;

	/* Add tlv id field */
	dest = qdf_nbuf_push_head(mpdu_nbuf, sizeof(uint8_t));
	if (qdf_likely(dest)) {
		*((uint8_t *)dest) = id;
		num_bytes_pushed += sizeof(uint8_t);
	}

	/* Add tlv len field */
	dest = qdf_nbuf_push_head(mpdu_nbuf, sizeof(uint16_t));
	if (qdf_likely(dest)) {
		*((uint16_t *)dest) = len;
		num_bytes_pushed += sizeof(uint16_t);
	}

	/* Add tlv value field */
	dest = qdf_nbuf_push_head(mpdu_nbuf, len);
	if (qdf_likely(dest)) {
		qdf_mem_copy(dest, value, len);
		num_bytes_pushed += len;
	}

	return num_bytes_pushed;
}

void
dp_mon_rx_stats_update_rssi_dbm_params(struct dp_mon_pdev *mon_pdev,
				       struct hal_rx_ppdu_info *ppdu_info)
{
	ppdu_info->rx_status.rssi_offset = mon_pdev->rssi_offsets.rssi_offset;
	ppdu_info->rx_status.rssi_dbm_conv_support =
				mon_pdev->rssi_dbm_conv_support;
	ppdu_info->rx_status.chan_noise_floor =
		mon_pdev->rssi_offsets.rssi_offset;
}

#ifdef WLAN_SUPPORT_CTRL_FRAME_STATS
void dp_rx_mon_update_user_ctrl_frame_stats(struct dp_pdev *pdev,
					    struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_peer *peer;
	struct dp_mon_peer *mon_peer;
	struct dp_soc *soc = pdev->soc;
	uint16_t fc, sw_peer_id;
	uint8_t i;

	if (qdf_unlikely(!ppdu_info))
		return;

	fc = ppdu_info->nac_info.frame_control;
	if (qdf_likely((qdf_cpu_to_le16(fc) & QDF_IEEE80211_FC0_TYPE_MASK) !=
	    QDF_IEEE80211_FC0_TYPE_CTL))
		return;

	for (i = 0; i < ppdu_info->com_info.num_users; i++) {
		sw_peer_id = ppdu_info->rx_user_status[i].sw_peer_id;
		peer = dp_peer_get_ref_by_id(soc, sw_peer_id,
					     DP_MOD_ID_RX_PPDU_STATS);
		if (qdf_unlikely(!peer))
			continue;
		mon_peer = peer->monitor_peer;
		if (qdf_unlikely(!mon_peer)) {
			dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);
			continue;
		}
		DP_STATS_INCC(mon_peer, rx.ndpa_cnt, 1,
			      ppdu_info->ctrl_frm_info[i].ndpa);
		DP_STATS_INCC(mon_peer, rx.bar_cnt, 1,
			      ppdu_info->ctrl_frm_info[i].bar);

		dp_peer_unref_delete(peer, DP_MOD_ID_RX_PPDU_STATS);
	}
}
#endif /* WLAN_SUPPORT_CTRL_FRAME_STATS */

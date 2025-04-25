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

#ifndef _HAL_BE_GENERIC_API_H_
#define _HAL_BE_GENERIC_API_H_

#include <hal_be_hw_headers.h>
#include "hal_be_tx.h"
#include "hal_be_reo.h"
#include <hal_api_mon.h>
#include <hal_generic_api.h>
#include "txmon_tlvs.h"

/*
 * Debug macro to print the TLV header tag
 */
#define SHOW_DEFINED(x) do {} while (0)

#if defined(WLAN_PKT_CAPTURE_TX_2_0) && !defined(TX_MONITOR_WORD_MASK)
typedef struct tx_fes_setup hal_tx_fes_setup_t;
typedef struct tx_peer_entry hal_tx_peer_entry_t;
typedef struct tx_queue_extension hal_tx_queue_ext_t;
typedef struct tx_msdu_start hal_tx_msdu_start_t;
typedef struct tx_mpdu_start hal_tx_mpdu_start_t;
typedef struct tx_fes_status_end hal_tx_fes_status_end_t;
typedef struct response_end_status hal_response_end_status_t;
typedef struct tx_fes_status_prot hal_tx_fes_status_prot_t;
typedef struct pcu_ppdu_setup_init hal_pcu_ppdu_setup_t;
#endif

#if defined(WLAN_FEATURE_TSF_AUTO_REPORT) || defined(WLAN_CONFIG_TX_DELAY)
static inline void
hal_tx_comp_get_buffer_timestamp_be(void *desc,
				    struct hal_tx_completion_status *ts)
{
	ts->buffer_timestamp = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
					       BUFFER_TIMESTAMP);
}
#else /* !(WLAN_FEATURE_TSF_AUTO_REPORT || WLAN_CONFIG_TX_DELAY) */
static inline void
hal_tx_comp_get_buffer_timestamp_be(void *desc,
				    struct hal_tx_completion_status *ts)
{
}
#endif /* WLAN_FEATURE_TSF_AUTO_REPORT || WLAN_CONFIG_TX_DELAY */

/**
 * hal_tx_comp_get_status_generic_be() - TQM Release reason
 * @desc: WBM descriptor
 * @ts1: completion ring Tx status
 * @hal: hal_soc
 *
 * This function will parse the WBM completion descriptor and populate in
 * HAL structure
 *
 * Return: none
 */
static inline void
hal_tx_comp_get_status_generic_be(void *desc, void *ts1,
				  struct hal_soc *hal)
{
	uint8_t rate_stats_valid = 0;
	uint32_t rate_stats = 0;
	struct hal_tx_completion_status *ts =
		(struct hal_tx_completion_status *)ts1;

	ts->ppdu_id = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
				      TQM_STATUS_NUMBER);
	ts->ack_frame_rssi = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
					     ACK_FRAME_RSSI);
	ts->first_msdu = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
					 FIRST_MSDU);
	ts->last_msdu = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
					LAST_MSDU);
#if 0
	// TODO -  This has to be calculated form first and last msdu
	ts->msdu_part_of_amsdu = HAL_TX_DESC_GET(desc,
						 WBM2SW_COMPLETION_RING_TX,
						 MSDU_PART_OF_AMSDU);
#endif

	ts->peer_id = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
				      SW_PEER_ID);
	ts->tid = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX, TID);
	ts->transmit_cnt = HAL_TX_DESC_GET(desc, WBM2SW_COMPLETION_RING_TX,
					   TRANSMIT_COUNT);

	rate_stats = HAL_TX_DESC_GET(desc, HAL_TX_COMP, TX_RATE_STATS);

	rate_stats_valid = HAL_TX_MS(TX_RATE_STATS_INFO,
			TX_RATE_STATS_INFO_VALID, rate_stats);

	ts->valid = rate_stats_valid;

	if (rate_stats_valid) {
		ts->bw = HAL_TX_MS(TX_RATE_STATS_INFO, TRANSMIT_BW,
				rate_stats);
		ts->pkt_type = HAL_TX_MS(TX_RATE_STATS_INFO,
				TRANSMIT_PKT_TYPE, rate_stats);
		ts->stbc = HAL_TX_MS(TX_RATE_STATS_INFO,
				TRANSMIT_STBC, rate_stats);
		ts->ldpc = HAL_TX_MS(TX_RATE_STATS_INFO, TRANSMIT_LDPC,
				rate_stats);
		ts->sgi = HAL_TX_MS(TX_RATE_STATS_INFO, TRANSMIT_SGI,
				rate_stats);
		ts->mcs = HAL_TX_MS(TX_RATE_STATS_INFO, TRANSMIT_MCS,
				rate_stats);
		ts->ofdma = HAL_TX_MS(TX_RATE_STATS_INFO, OFDMA_TRANSMISSION,
				rate_stats);
		ts->tones_in_ru = HAL_TX_MS(TX_RATE_STATS_INFO, TONES_IN_RU,
				rate_stats);
	}

	ts->release_src = hal_tx_comp_get_buffer_source_generic_be(desc);
	ts->status = hal_tx_comp_get_release_reason(
					desc,
					hal_soc_to_hal_soc_handle(hal));

	ts->tsf = HAL_TX_DESC_GET(desc, UNIFIED_WBM_RELEASE_RING_6,
			TX_RATE_STATS_INFO_TX_RATE_STATS);
	hal_tx_comp_get_buffer_timestamp_be(desc, ts);
}

/**
 * hal_tx_set_pcp_tid_map_generic_be() - Configure default PCP to TID map table
 * @soc: HAL SoC context
 * @map: PCP-TID mapping table
 *
 * PCP are mapped to 8 TID values using TID values programmed
 * in one set of mapping registers PCP_TID_MAP_<0 to 6>
 * The mapping register has TID mapping for 8 PCP values
 *
 * Return: none
 */
static void hal_tx_set_pcp_tid_map_generic_be(struct hal_soc *soc, uint8_t *map)
{
	uint32_t addr, value;

	addr = HWIO_TCL_R0_PCP_TID_MAP_ADDR(
				MAC_TCL_REG_REG_BASE);

	value = (map[0] |
		(map[1] << HWIO_TCL_R0_PCP_TID_MAP_PCP_1_SHFT) |
		(map[2] << HWIO_TCL_R0_PCP_TID_MAP_PCP_2_SHFT) |
		(map[3] << HWIO_TCL_R0_PCP_TID_MAP_PCP_3_SHFT) |
		(map[4] << HWIO_TCL_R0_PCP_TID_MAP_PCP_4_SHFT) |
		(map[5] << HWIO_TCL_R0_PCP_TID_MAP_PCP_5_SHFT) |
		(map[6] << HWIO_TCL_R0_PCP_TID_MAP_PCP_6_SHFT) |
		(map[7] << HWIO_TCL_R0_PCP_TID_MAP_PCP_7_SHFT));

	HAL_REG_WRITE(soc, addr, (value & HWIO_TCL_R0_PCP_TID_MAP_RMSK));
}

/**
 * hal_tx_update_pcp_tid_generic_be() - Update the pcp tid map table with
 *					value received from user-space
 * @soc: HAL SoC context
 * @pcp: pcp value
 * @tid : tid value
 *
 * Return: void
 */
static void
hal_tx_update_pcp_tid_generic_be(struct hal_soc *soc,
				 uint8_t pcp, uint8_t tid)
{
	uint32_t addr, value, regval;

	addr = HWIO_TCL_R0_PCP_TID_MAP_ADDR(
				MAC_TCL_REG_REG_BASE);

	value = (uint32_t)tid << (HAL_TX_BITS_PER_TID * pcp);

	/* Read back previous PCP TID config and update
	 * with new config.
	 */
	regval = HAL_REG_READ(soc, addr);
	regval &= ~(HAL_TX_TID_BITS_MASK << (HAL_TX_BITS_PER_TID * pcp));
	regval |= value;

	HAL_REG_WRITE(soc, addr,
		      (regval & HWIO_TCL_R0_PCP_TID_MAP_RMSK));
}

/**
 * hal_tx_update_tidmap_prty_generic_be() - Update the tid map priority
 * @soc: HAL SoC context
 * @value: priority value
 *
 * Return: void
 */
static
void hal_tx_update_tidmap_prty_generic_be(struct hal_soc *soc, uint8_t value)
{
	uint32_t addr;

	addr = HWIO_TCL_R0_TID_MAP_PRTY_ADDR(
				MAC_TCL_REG_REG_BASE);

	HAL_REG_WRITE(soc, addr,
		      (value & HWIO_TCL_R0_TID_MAP_PRTY_RMSK));
}

/**
 * hal_rx_get_tlv_size_generic_be() - Get rx packet tlv size
 * @rx_pkt_tlv_size: TLV size for regular RX packets
 * @rx_mon_pkt_tlv_size: TLV size for monitor mode packets
 *
 * Return: size of rx pkt tlv before the actual data
 */
static void hal_rx_get_tlv_size_generic_be(uint16_t *rx_pkt_tlv_size,
					   uint16_t *rx_mon_pkt_tlv_size)
{
	*rx_pkt_tlv_size = RX_PKT_TLVS_LEN;
	/* For now mon pkt tlv is same as rx pkt tlv */
	*rx_mon_pkt_tlv_size = MON_RX_PKT_TLVS_LEN;
}

/**
 * hal_rx_flow_get_tuple_info_be() - Setup a flow search entry in HW FST
 * @rx_fst: Pointer to the Rx Flow Search Table
 * @hal_hash: HAL 5 tuple hash
 * @flow_tuple_info: 5-tuple info of the flow returned to the caller
 *
 * Return: Success/Failure
 */
static void *
hal_rx_flow_get_tuple_info_be(uint8_t *rx_fst, uint32_t hal_hash,
			      uint8_t *flow_tuple_info)
{
	struct hal_rx_fst *fst = (struct hal_rx_fst *)rx_fst;
	void *hal_fse = NULL;
	struct hal_flow_tuple_info *tuple_info
		= (struct hal_flow_tuple_info *)flow_tuple_info;

	hal_fse = (uint8_t *)fst->base_vaddr +
		(hal_hash * HAL_RX_FST_ENTRY_SIZE);

	if (!hal_fse || !tuple_info)
		return NULL;

	if (!HAL_GET_FLD(hal_fse, RX_FLOW_SEARCH_ENTRY, VALID))
		return NULL;

	tuple_info->src_ip_127_96 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      SRC_IP_127_96));
	tuple_info->src_ip_95_64 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      SRC_IP_95_64));
	tuple_info->src_ip_63_32 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      SRC_IP_63_32));
	tuple_info->src_ip_31_0 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      SRC_IP_31_0));
	tuple_info->dest_ip_127_96 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      DEST_IP_127_96));
	tuple_info->dest_ip_95_64 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      DEST_IP_95_64));
	tuple_info->dest_ip_63_32 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      DEST_IP_63_32));
	tuple_info->dest_ip_31_0 =
				qdf_ntohl(HAL_GET_FLD(hal_fse,
						      RX_FLOW_SEARCH_ENTRY,
						      DEST_IP_31_0));
	tuple_info->dest_port = HAL_GET_FLD(hal_fse,
					    RX_FLOW_SEARCH_ENTRY,
					    DEST_PORT);
	tuple_info->src_port = HAL_GET_FLD(hal_fse,
					   RX_FLOW_SEARCH_ENTRY,
					   SRC_PORT);
	tuple_info->l4_protocol = HAL_GET_FLD(hal_fse,
					      RX_FLOW_SEARCH_ENTRY,
					      L4_PROTOCOL);

	return hal_fse;
}

/**
 * hal_rx_flow_delete_entry_be() - Setup a flow search entry in HW FST
 * @rx_fst: Pointer to the Rx Flow Search Table
 * @hal_rx_fse: Pointer to the Rx Flow that is to be deleted from the FST
 *
 * Return: Success/Failure
 */
static QDF_STATUS
hal_rx_flow_delete_entry_be(uint8_t *rx_fst, void *hal_rx_fse)
{
	uint8_t *fse = (uint8_t *)hal_rx_fse;

	if (!HAL_GET_FLD(fse, RX_FLOW_SEARCH_ENTRY, VALID))
		return QDF_STATUS_E_NOENT;

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, VALID);

	return QDF_STATUS_SUCCESS;
}

/**
 * hal_rx_fst_get_fse_size_be() - Retrieve the size of each entry in Rx FST
 *
 * Return: size of each entry/flow in Rx FST
 */
static inline uint32_t
hal_rx_fst_get_fse_size_be(void)
{
	return HAL_RX_FST_ENTRY_SIZE;
}

/*
 * TX MONITOR
 */

#ifdef WLAN_PKT_CAPTURE_TX_2_0
/**
 * hal_txmon_is_mon_buf_addr_tlv_generic_be() - api to find mon buffer tlv
 * @tx_tlv_hdr: pointer to TLV header
 *
 * Return: bool based on tlv tag matches monitor buffer address tlv
 */
static inline bool
hal_txmon_is_mon_buf_addr_tlv_generic_be(void *tx_tlv_hdr)
{
	uint32_t tlv_tag;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(tx_tlv_hdr);

	if (WIFIMON_BUFFER_ADDR_E == tlv_tag)
		return true;

	return false;
}

/**
 * hal_txmon_populate_packet_info_generic_be() - api to populate packet info
 * @tx_tlv: pointer to TLV header
 * @packet_info: place holder for packet info
 *
 * Return: Address to void
 */
static inline void
hal_txmon_populate_packet_info_generic_be(void *tx_tlv, void *packet_info)
{
	struct hal_mon_packet_info *pkt_info;
	struct mon_buffer_addr *addr = (struct mon_buffer_addr *)tx_tlv;

	pkt_info = (struct hal_mon_packet_info *)packet_info;
	pkt_info->sw_cookie = (((uint64_t)addr->buffer_virt_addr_63_32 << 32) |
			       (addr->buffer_virt_addr_31_0));
	pkt_info->dma_length = addr->dma_length + 1;
	pkt_info->msdu_continuation = addr->msdu_continuation;
	pkt_info->truncated = addr->truncated;
}

/**
 * hal_txmon_parse_tx_fes_setup() - parse tx_fes_setup tlv
 *
 * @tx_tlv: pointer to tx_fes_setup tlv header
 * @tx_ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_tx_fes_setup(void *tx_tlv,
			     struct hal_tx_ppdu_info *tx_ppdu_info)
{
	hal_tx_fes_setup_t *tx_fes_setup = (hal_tx_fes_setup_t *)tx_tlv;

	tx_ppdu_info->num_users = tx_fes_setup->number_of_users;
	if (tx_ppdu_info->num_users == 0)
		tx_ppdu_info->num_users = 1;

	TXMON_HAL(tx_ppdu_info, ppdu_id) = tx_fes_setup->schedule_id;
	TXMON_HAL_STATUS(tx_ppdu_info, ppdu_id) = tx_fes_setup->schedule_id;
}

/**
 * hal_txmon_get_num_users() - get num users from tx_fes_setup tlv
 *
 * @tx_tlv: pointer to tx_fes_setup tlv header
 *
 * Return: number of users
 */
static inline uint8_t
hal_txmon_get_num_users(void *tx_tlv)
{
	hal_tx_fes_setup_t *tx_fes_setup = (hal_tx_fes_setup_t *)tx_tlv;

	return tx_fes_setup->number_of_users;
}

/**
 * hal_txmon_parse_tx_fes_status_end() - parse tx_fes_status_end tlv
 *
 * @tx_tlv: pointer to tx_fes_status_end tlv header
 * @ppdu_info: pointer to hal_tx_ppdu_info
 * @tx_status_info: pointer to hal_tx_status_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_tx_fes_status_end(void *tx_tlv,
				  struct hal_tx_ppdu_info *ppdu_info,
				  struct hal_tx_status_info *tx_status_info)
{
	hal_tx_fes_status_end_t *tx_fes_end = (hal_tx_fes_status_end_t *)tx_tlv;

	if (tx_fes_end->phytx_abort_request_info_valid) {
		TXMON_STATUS_INFO(tx_status_info, phy_abort_reason) =
		tx_fes_end->phytx_abort_request_info_details.phytx_abort_reason;
		TXMON_STATUS_INFO(tx_status_info, phy_abort_user_number) =
		tx_fes_end->phytx_abort_request_info_details.user_number;
	}

	TXMON_STATUS_INFO(tx_status_info,
			  response_type) = tx_fes_end->response_type;
	TXMON_STATUS_INFO(tx_status_info,
			  r2r_to_follow) = tx_fes_end->r2r_end_status_to_follow;
	/*  update phy timestamp to ppdu timestamp */
	TXMON_HAL_STATUS(ppdu_info, ppdu_timestamp) =
		(tx_fes_end->start_of_frame_timestamp_15_0 |
		 tx_fes_end->start_of_frame_timestamp_31_16 <<
		 HAL_TX_LSB(TX_FES_STATUS_END, START_OF_FRAME_TIMESTAMP_31_16));
}

/**
 * hal_txmon_parse_response_end_status() - parse response_end_status tlv
 *
 * @tx_tlv: pointer to response_end_status tlv header
 * @ppdu_info: pointer to hal_tx_ppdu_info
 * @tx_status_info: pointer to hal_tx_status_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_response_end_status(void *tx_tlv,
				    struct hal_tx_ppdu_info *ppdu_info,
				    struct hal_tx_status_info *tx_status_info)
{
	hal_response_end_status_t *resp_end_status = NULL;

	resp_end_status = (hal_response_end_status_t *)tx_tlv;
	TXMON_HAL_STATUS(ppdu_info, bw) = resp_end_status->coex_based_tx_bw;
	TXMON_STATUS_INFO(tx_status_info, generated_response) =
		resp_end_status->generated_response;
	TXMON_STATUS_INFO(tx_status_info, mba_count) =
		resp_end_status->mba_user_count;
	TXMON_STATUS_INFO(tx_status_info, mba_fake_bitmap_count) =
		resp_end_status->mba_fake_bitmap_count;
	TXMON_HAL_STATUS(ppdu_info, ppdu_timestamp) =
		(resp_end_status->start_of_frame_timestamp_15_0 |
		 (resp_end_status->start_of_frame_timestamp_31_16 << 16));
}

/**
 * hal_txmon_parse_pcu_ppdu_setup_init() - parse pcu_ppdu_setup_init tlv
 *
 * @tx_tlv: pointer to pcu_ppdu_setup_init tlv header
 * @data_status_info: pointer to data hal_tx_status_info
 * @prot_status_info: pointer to protection hal_tx_status_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_pcu_ppdu_setup_init(void *tx_tlv,
				    struct hal_tx_status_info *data_status_info,
				    struct hal_tx_status_info *prot_status_info)
{
	hal_pcu_ppdu_setup_t *pcu_init = (hal_pcu_ppdu_setup_t *)tx_tlv;

	prot_status_info->protection_addr =
		pcu_init->use_address_fields_for_protection;
	/* protection frame address 1 */
	*(uint32_t *)&prot_status_info->addr1[0] =
		pcu_init->protection_frame_ad1_31_0;
	*(uint16_t *)&prot_status_info->addr1[4] =
		pcu_init->protection_frame_ad1_47_32;
	/* protection frame address 2 */
	*(uint32_t *)&prot_status_info->addr2[0] =
		pcu_init->protection_frame_ad2_15_0;
	*(uint32_t *)&prot_status_info->addr2[2] =
		pcu_init->protection_frame_ad2_47_16;
	/* protection frame address 3 */
	*(uint32_t *)&prot_status_info->addr3[0] =
		pcu_init->protection_frame_ad3_31_0;
	*(uint16_t *)&prot_status_info->addr3[4] =
		pcu_init->protection_frame_ad3_47_32;
	/* protection frame address 4 */
	*(uint32_t *)&prot_status_info->addr4[0] =
		pcu_init->protection_frame_ad4_15_0;
	*(uint32_t *)&prot_status_info->addr4[2] =
		pcu_init->protection_frame_ad4_47_16;
}

/**
 * hal_txmon_parse_peer_entry() - parse peer entry tlv
 *
 * @tx_tlv: pointer to peer_entry tlv header
 * @user_id: user_id
 * @tx_ppdu_info: pointer to hal_tx_ppdu_info
 * @tx_status_info: pointer to hal_tx_status_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_peer_entry(void *tx_tlv,
			   uint8_t user_id,
			   struct hal_tx_ppdu_info *tx_ppdu_info,
			   struct hal_tx_status_info *tx_status_info)
{
	hal_tx_peer_entry_t *peer_entry = (hal_tx_peer_entry_t *)tx_tlv;

	*(uint32_t *)&tx_status_info->addr1[0] =
				peer_entry->mac_addr_a_31_0;
	*(uint16_t *)&tx_status_info->addr1[4] =
				peer_entry->mac_addr_a_47_32;
	*(uint32_t *)&tx_status_info->addr2[0] =
				peer_entry->mac_addr_b_15_0;
	*(uint32_t *)&tx_status_info->addr2[2] =
				peer_entry->mac_addr_b_47_16;
	TXMON_HAL_USER(tx_ppdu_info, user_id, sw_peer_id) =
				peer_entry->sw_peer_id;
}

/**
 * hal_txmon_parse_queue_exten() - parse queue exten tlv
 *
 * @tx_tlv: pointer to queue exten tlv header
 * @tx_ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_queue_exten(void *tx_tlv,
			    struct hal_tx_ppdu_info *tx_ppdu_info)
{
	hal_tx_queue_ext_t *queue_ext = (hal_tx_queue_ext_t *)tx_tlv;

	TXMON_HAL_STATUS(tx_ppdu_info, frame_control) = queue_ext->frame_ctl;
	TXMON_HAL_STATUS(tx_ppdu_info, frame_control_info_valid) = true;
}

/**
 * hal_txmon_parse_mpdu_start() - parse mpdu start tlv
 *
 * @tx_tlv: pointer to mpdu start tlv header
 * @user_id: user id
 * @tx_ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_mpdu_start(void *tx_tlv, uint8_t user_id,
			   struct hal_tx_ppdu_info *tx_ppdu_info)
{
	hal_tx_mpdu_start_t *mpdu_start = (hal_tx_mpdu_start_t *)tx_tlv;

	TXMON_HAL_USER(tx_ppdu_info, user_id, start_seq) =
		mpdu_start->mpdu_sequence_number;
	TXMON_HAL(tx_ppdu_info, cur_usr_idx) = user_id;
}

/**
 * hal_txmon_parse_msdu_start() - parse msdu start tlv
 *
 * @tx_tlv: pointer to msdu start tlv header
 * @user_id: user id
 * @tx_ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_msdu_start(void *tx_tlv, uint8_t user_id,
			   struct hal_tx_ppdu_info *tx_ppdu_info)
{
}

/**
 * hal_txmon_parse_tx_fes_status_prot() - parse tx_fes_status_prot tlv
 *
 * @tx_tlv: pointer to pcu_ppdu_setup_init tlv header
 * @ppdu_info: pointer to hal_tx_ppdu_info
 * @tx_status_info: pointer to hal_tx_status_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_tx_fes_status_prot(void *tx_tlv,
				   struct hal_tx_ppdu_info *ppdu_info,
				   struct hal_tx_status_info *tx_status_info)
{
	hal_tx_fes_status_prot_t *fes_prot = (hal_tx_fes_status_prot_t *)tx_tlv;

	TXMON_HAL_STATUS(ppdu_info, ppdu_timestamp) =
		(fes_prot->start_of_frame_timestamp_15_0 |
		 fes_prot->start_of_frame_timestamp_31_16 << 15);
}

/**
 * get_ru_offset_from_start_index() - api to get ru offset from ru index
 *
 * @ru_size: RU size
 * @start_idx: Start index
 *
 * Return: uint8_t ru allocation offset
 */
static inline
uint8_t get_ru_offset_from_start_index(uint8_t ru_size, uint8_t start_idx)
{
	uint8_t ru_alloc_offset[HAL_MAX_DL_MU_USERS][HAL_MAX_RU_INDEX] = {
		{0, 0, 0, 0, 0, 0, 0},
		{1, 0, 0, 0, 0, 0, 0},
		{2, 1, 0, 0, 0, 0, 0},
		{3, 1, 0, 0, 0, 0, 0},
		{4, 0, 0, 0, 0, 0, 0},
		{5, 2, 1, 0, 0, 0, 0},
		{6, 2, 1, 0, 0, 0, 0},
		{7, 3, 1, 0, 0, 0, 0},
		{8, 3, 1, 0, 0, 0, 0},
		{9, 4, 2, 1, 0, 0, 0},
		{10, 4, 2, 1, 0, 0, 0},
		{11, 5, 2, 1, 0, 0, 0},
		{12, 5, 2, 1, 0, 0, 0},
		{13, 0, 0, 1, 0, 0, 0},
		{14, 6, 3, 1, 0, 0, 0},
		{15, 6, 3, 1, 0, 0, 0},
		{16, 7, 3, 1, 0, 0, 0},
		{17, 7, 3, 1, 0, 0, 0},
		{18, 0, 0, 0, 0, 0, 0},
		{19, 8, 4, 2, 1, 0, 0},
		{20, 8, 4, 2, 1, 0, 0},
		{21, 9, 4, 2, 1, 0, 0},
		{22, 9, 4, 2, 1, 0, 0},
		{23, 0, 0, 2, 1, 0, 0},
		{24, 10, 5, 2, 1, 0, 0},
		{25, 10, 5, 2, 1, 0, 0},
		{26, 11, 5, 2, 1, 0, 0},
		{27, 11, 5, 2, 1, 0, 0},
		{28, 12, 6, 3, 1, 0, 0},
		{29, 12, 6, 3, 1, 0, 0},
		{30, 13, 6, 3, 1, 0, 0},
		{31, 13, 6, 3, 1, 0, 0},
		{32, 0, 0, 3, 1, 0, 0},
		{33, 14, 7, 3, 1, 0, 0},
		{34, 14, 7, 3, 1, 0, 0},
		{35, 15, 7, 3, 1, 0, 0},
		{36, 15, 7, 3, 1, 0, 0},
	};

	if (start_idx >= HAL_MAX_UL_MU_USERS || ru_size >= HAL_MAX_RU_INDEX)
		return 0;

	return ru_alloc_offset[start_idx][ru_size];
}

/**
 * hal_txmon_parse_fw2sw() - parse firmware to software tlv
 *
 * @tx_tlv: pointer to firmware to software tlvmpdu start tlv header
 * @type: place where this tlv is generated
 * @status_info: pointer to hal_tx_status_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_fw2sw(void *tx_tlv, uint8_t type,
		      struct hal_tx_status_info *status_info)
{
	uint32_t *msg = (uint32_t *)tx_tlv;

	switch (type) {
	case TXMON_FW2SW_TYPE_FES_SETUP:
	{
		uint32_t schedule_id;
		uint16_t c_freq1;
		uint16_t c_freq2;
		uint16_t freq_mhz;
		uint8_t phy_mode;

		c_freq1 = TXMON_FW2SW_MON_FES_SETUP_BAND_CENTER_FREQ1_GET(*msg);
		c_freq2 = TXMON_FW2SW_MON_FES_SETUP_BAND_CENTER_FREQ2_GET(*msg);

		msg++;
		phy_mode = TXMON_FW2SW_MON_FES_SETUP_PHY_MODE_GET(*msg);
		freq_mhz = TXMON_FW2SW_MON_FES_SETUP_MHZ_GET(*msg);

		msg++;
		schedule_id = TXMON_FW2SW_MON_FES_SETUP_SCHEDULE_ID_GET(*msg);

		TXMON_STATUS_INFO(status_info, band_center_freq1) = c_freq1;
		TXMON_STATUS_INFO(status_info, band_center_freq2) = c_freq2;
		TXMON_STATUS_INFO(status_info, freq) = freq_mhz;
		TXMON_STATUS_INFO(status_info, phy_mode) = phy_mode;
		TXMON_STATUS_INFO(status_info, schedule_id) = schedule_id;

		break;
	}
	case TXMON_FW2SW_TYPE_FES_SETUP_USER:
	{
		break;
	}
	case TXMON_FW2SW_TYPE_FES_SETUP_EXT:
	{
		break;
	}
	};
}

/**
 * hal_txmon_parse_u_sig_hdr() - parse u_sig header information from tlv
 *
 * @tx_tlv: pointer to mactx_u_sig_eht_su_mu/tb tlv
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_u_sig_hdr(void *tx_tlv, struct hal_tx_ppdu_info *ppdu_info)
{
	struct hal_mon_usig_hdr *usig = (struct hal_mon_usig_hdr *)tx_tlv;
	struct hal_mon_usig_cmn *usig_1 = &usig->usig_1;
	uint8_t bad_usig_crc;

	bad_usig_crc = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_U_SIG_EHT_SU_MU_MACTX_U_SIG_EHT_SU_MU_INFO_DETAILS,
					  CRC) ? 0 : 1;

	TXMON_HAL_STATUS(ppdu_info, usig_common) |=
			QDF_MON_STATUS_USIG_PHY_VERSION_KNOWN |
			QDF_MON_STATUS_USIG_BW_KNOWN |
			QDF_MON_STATUS_USIG_UL_DL_KNOWN |
			QDF_MON_STATUS_USIG_BSS_COLOR_KNOWN |
			QDF_MON_STATUS_USIG_TXOP_KNOWN;

	TXMON_HAL_STATUS(ppdu_info, usig_common) |=
			(usig_1->phy_version <<
			 QDF_MON_STATUS_USIG_PHY_VERSION_SHIFT);
	TXMON_HAL_STATUS(ppdu_info, usig_common) |=
			(usig_1->bw << QDF_MON_STATUS_USIG_BW_SHIFT);
	TXMON_HAL_STATUS(ppdu_info, usig_common) |=
			(usig_1->ul_dl << QDF_MON_STATUS_USIG_UL_DL_SHIFT);
	TXMON_HAL_STATUS(ppdu_info, usig_common) |=
			(usig_1->bss_color <<
			 QDF_MON_STATUS_USIG_BSS_COLOR_SHIFT);
	TXMON_HAL_STATUS(ppdu_info, usig_common) |=
			(usig_1->txop << QDF_MON_STATUS_USIG_TXOP_SHIFT);
	TXMON_HAL_STATUS(ppdu_info, usig_common) |= bad_usig_crc;
	TXMON_HAL_STATUS(ppdu_info, bw) = usig_1->bw;

	TXMON_HAL_STATUS(ppdu_info, usig_flags) = 1;
}

/**
 * hal_txmon_populate_he_data_per_user() - populate he data per user
 *
 * @usr: pointer to hal_txmon_user_desc_per_user
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_populate_he_data_per_user(struct hal_txmon_user_desc_per_user *usr,
				    uint32_t user_id,
				    struct hal_tx_ppdu_info *ppdu_info)
{
	uint32_t he_data1 = TXMON_HAL_USER(ppdu_info, user_id, he_data1);
	uint32_t he_data2 = TXMON_HAL_USER(ppdu_info, user_id, he_data2);
	uint32_t he_data3 = TXMON_HAL_USER(ppdu_info, user_id, he_data3);
	uint32_t he_data5 = TXMON_HAL_USER(ppdu_info, user_id, he_data5);
	uint32_t he_data6 = TXMON_HAL_USER(ppdu_info, user_id, he_data6);

	/* populate */
	/* BEAM CHANGE */
	he_data1 |= QDF_MON_STATUS_HE_BEAM_CHANGE_KNOWN;
	he_data1 |= QDF_MON_STATUS_TXBF_KNOWN;
	he_data5 |= (!!usr->user_bf_type << QDF_MON_STATUS_TXBF_SHIFT);
	he_data3 |= (!!usr->user_bf_type << QDF_MON_STATUS_BEAM_CHANGE_SHIFT);

	/* UL/DL known */
	he_data1 |= QDF_MON_STATUS_HE_DL_UL_KNOWN;
	he_data3 |= (1 << QDF_MON_STATUS_DL_UL_SHIFT);

	/* MCS */
	he_data1 |= QDF_MON_STATUS_HE_MCS_KNOWN;
	he_data3 |= (usr->mcs << QDF_MON_STATUS_TRANSMIT_MCS_SHIFT);
	/* DCM */
	he_data1 |= QDF_MON_STATUS_HE_DCM_KNOWN;
	he_data3 |= (usr->dcm << QDF_MON_STATUS_DCM_SHIFT);
	/* LDPC EXTRA SYMB */
	he_data1 |= QDF_MON_STATUS_HE_LDPC_EXTRA_SYMBOL_KNOWN;
	he_data3 |= (usr->ldpc_extra_symbol <<
		     QDF_MON_STATUS_LDPC_EXTRA_SYMBOL_SHIFT);
	/* RU offset and RU */
	he_data2 |= QDF_MON_STATUS_RU_ALLOCATION_OFFSET_KNOWN;
	he_data2 |= (get_ru_offset_from_start_index(usr->ru_size,
						    usr->ru_start_index) <<
		     QDF_MON_STATUS_RU_ALLOCATION_SHIFT);
	/* Data BW and RU allocation */
	if (usr->ru_size < HAL_MAX_RU_INDEX) {
		/* update bandwidth if it is full bandwidth */
		he_data1 |= QDF_MON_STATUS_HE_DATA_BW_RU_KNOWN;
		he_data5 = (he_data5 & 0xFFF0) | (4 + usr->ru_size);
	}

	he_data6 |= (usr->nss & 0xF);
	TXMON_HAL_USER(ppdu_info, user_id, mcs) = usr->mcs;

	/* update stack variable to ppdu_info */
	TXMON_HAL_USER(ppdu_info, user_id, he_data1) = he_data1;
	TXMON_HAL_USER(ppdu_info, user_id, he_data2) = he_data2;
	TXMON_HAL_USER(ppdu_info, user_id, he_data3) = he_data3;
	TXMON_HAL_USER(ppdu_info, user_id, he_data5) = he_data5;
	TXMON_HAL_USER(ppdu_info, user_id, he_data6) = he_data6;
}

/**
 * hal_txmon_get_user_desc_per_user() - get mactx user desc per user from tlv
 *
 * @tx_tlv: pointer to mactx_user_desc_per_user tlv
 * @usr: pointer to hal_txmon_user_desc_per_user
 *
 * Return: void
 */
static inline void
hal_txmon_get_user_desc_per_user(void *tx_tlv,
				 struct hal_txmon_user_desc_per_user *usr)
{
	usr->psdu_length = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
					      PSDU_LENGTH);
	usr->ru_start_index = HAL_TX_DESC_GET_64(tx_tlv,
						 MACTX_USER_DESC_PER_USER,
						 RU_START_INDEX);
	usr->ru_size = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
					  RU_SIZE);
	usr->ofdma_mu_mimo_enabled =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
				   OFDMA_MU_MIMO_ENABLED);
	usr->nss = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
				      NSS) + 1;
	usr->stream_offset = HAL_TX_DESC_GET_64(tx_tlv,
						MACTX_USER_DESC_PER_USER,
						STREAM_OFFSET);
	usr->mcs = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER, MCS);
	usr->dcm = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER, DCM);
	usr->fec_type = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
					   FEC_TYPE);
	usr->user_bf_type = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
					       USER_BF_TYPE);
	usr->drop_user_cbf = HAL_TX_DESC_GET_64(tx_tlv,
						MACTX_USER_DESC_PER_USER,
						DROP_USER_CBF);
	usr->ldpc_extra_symbol = HAL_TX_DESC_GET_64(tx_tlv,
						    MACTX_USER_DESC_PER_USER,
						    LDPC_EXTRA_SYMBOL);
	usr->force_extra_symbol = HAL_TX_DESC_GET_64(tx_tlv,
						     MACTX_USER_DESC_PER_USER,
						     FORCE_EXTRA_SYMBOL);
	usr->sw_peer_id = HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_PER_USER,
					     SW_PEER_ID);
}

/**
 * hal_txmon_populate_eht_sig_per_user() - populate eht sig user information
 *
 * @usr: pointer to hal_txmon_user_desc_per_user
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_populate_eht_sig_per_user(struct hal_txmon_user_desc_per_user *usr,
				    uint32_t user_id,
				    struct hal_tx_ppdu_info *ppdu_info)
{
	uint32_t eht_known = 0;
	uint32_t eht_data[6] = {0};
	uint8_t i = 0;

	eht_known = QDF_MON_STATUS_EHT_LDPC_EXTRA_SYMBOL_SEG_KNOWN;

	eht_data[0] |= (usr->ldpc_extra_symbol <<
			QDF_MON_STATUS_EHT_LDPC_EXTRA_SYMBOL_SEG_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, eht_known) |= eht_known;

	for (i = 0; i < 6; i++)
		TXMON_HAL_STATUS(ppdu_info, eht_data[i]) |= eht_data[i];
}

/**
 * hal_txmon_parse_user_desc_per_user() - parse mactx user desc per user
 *
 * @tx_tlv: pointer to mactx_user_desc_per_user tlv
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_user_desc_per_user(void *tx_tlv, uint32_t user_id,
				   struct hal_tx_ppdu_info *ppdu_info)
{
	struct hal_txmon_user_desc_per_user usr_info = {0};

	hal_txmon_get_user_desc_per_user(tx_tlv, &usr_info);

	/* based on preamble type populate user desc user info */
	if (TXMON_HAL_STATUS(ppdu_info, he_flags))
		hal_txmon_populate_he_data_per_user(&usr_info,
						    user_id, ppdu_info);

	hal_txmon_populate_eht_sig_per_user(&usr_info, user_id, ppdu_info);
}

/**
 * hal_txmon_get_user_desc_common() - update hal_txmon_usr_desc_common from tlv
 *
 * @tx_tlv: pointer to mactx_user_desc_common tlv
 * @usr_common: pointer to hal_txmon_usr_desc_common
 *
 * Return: void
 */
static inline void
hal_txmon_get_user_desc_common(void *tx_tlv,
			       struct hal_txmon_usr_desc_common *usr_common)
{
	usr_common->ltf_size =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON, LTF_SIZE);
	usr_common->pkt_extn_pe =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
				   PACKET_EXTENSION_PE_DISAMBIGUITY);
	usr_common->a_factor =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
				   PACKET_EXTENSION_A_FACTOR);
	usr_common->center_ru_0 =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON, CENTER_RU_0);
	usr_common->center_ru_1 =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON, CENTER_RU_1);
	usr_common->num_ltf_symbols =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
				   NUM_LTF_SYMBOLS);
	usr_common->doppler_indication =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
				   DOPPLER_INDICATION);
	usr_common->spatial_reuse =
		HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
				   SPATIAL_REUSE);

	usr_common->ru_channel_0[0] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND0_0);
	usr_common->ru_channel_0[1] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND0_1);
	usr_common->ru_channel_0[2] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND0_2);
	usr_common->ru_channel_0[3] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND0_3);
	usr_common->ru_channel_0[4] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND0_0);
	usr_common->ru_channel_0[5] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND0_1);
	usr_common->ru_channel_0[6] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND0_2);
	usr_common->ru_channel_0[7] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND0_3);

	usr_common->ru_channel_1[0] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND1_0);
	usr_common->ru_channel_1[1] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND1_1);
	usr_common->ru_channel_1[2] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND1_2);
	usr_common->ru_channel_1[3] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_0123_DETAILS_RU_ALLOCATION_BAND1_3);
	usr_common->ru_channel_1[4] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND1_0);
	usr_common->ru_channel_1[5] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND1_1);
	usr_common->ru_channel_1[6] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND1_2);
	usr_common->ru_channel_1[7] =
	HAL_TX_DESC_GET_64(tx_tlv, MACTX_USER_DESC_COMMON,
			   RU_ALLOCATION_4567_DETAILS_RU_ALLOCATION_BAND1_3);
}

/**
 * hal_txmon_populate_he_data_common() - populate he data common information
 *
 * @usr_common: pointer to hal_txmon_usr_desc_common
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_populate_he_data_common(struct hal_txmon_usr_desc_common *usr_common,
				  uint32_t user_id,
				  struct hal_tx_ppdu_info *ppdu_info)
{
	/* HE data 1 */
	TXMON_HAL_USER(ppdu_info,
		       user_id, he_data1) |= QDF_MON_STATUS_HE_DOPPLER_KNOWN;

	/* HE data 2 */
	TXMON_HAL_USER(ppdu_info, user_id,
		       he_data2) |= (QDF_MON_STATUS_PE_DISAMBIGUITY_KNOWN |
				     QDF_MON_STATUS_LTF_SYMBOLS_KNOWN);

	/* HE data 5 */
	TXMON_HAL_USER(ppdu_info, user_id, he_data5) |=
		(usr_common->pkt_extn_pe <<
		 QDF_MON_STATUS_PE_DISAMBIGUITY_SHIFT) |
		(usr_common->a_factor << QDF_MON_STATUS_PRE_FEC_PAD_SHIFT) |
		((1 + usr_common->ltf_size) <<
		 QDF_MON_STATUS_HE_LTF_SIZE_SHIFT) |
		(usr_common->num_ltf_symbols <<
		 QDF_MON_STATUS_HE_LTF_SYM_SHIFT);

	/* HE data 6 */
	TXMON_HAL_USER(ppdu_info, user_id,
		       he_data6) |= (usr_common->doppler_indication <<
				     QDF_MON_STATUS_DOPPLER_SHIFT);
}

/**
 * hal_txmon_populate_he_mu_common() - populate he mu common information
 *
 * @usr_common: pointer to hal_txmon_usr_desc_common
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_populate_he_mu_common(struct hal_txmon_usr_desc_common *usr_common,
				uint32_t user_id,
				struct hal_tx_ppdu_info *ppdu_info)
{
	uint16_t he_mu_flag_1 = 0;
	uint16_t he_mu_flag_2 = 0;
	uint16_t i = 0;

	he_mu_flag_1 |= (QDF_MON_STATUS_CHANNEL_2_CENTER_26_RU_KNOWN |
			 QDF_MON_STATUS_CHANNEL_1_CENTER_26_RU_KNOWN |
			 ((usr_common->center_ru_0 <<
			   QDF_MON_STATUS_CHANNEL_1_CENTER_26_RU_SHIFT) &
			  QDF_MON_STATUS_CHANNEL_1_CENTER_26_RU_VALUE));
	he_mu_flag_2 |= ((usr_common->center_ru_1 <<
			  QDF_MON_STATUS_CHANNEL_2_CENTER_26_RU_SHIFT) &
			 QDF_MON_STATUS_CHANNEL_2_CENTER_26_RU_VALUE);

	for (i = 0; i < usr_common->num_users; i++) {
		TXMON_HAL_USER(ppdu_info, i, he_flags1) |= he_mu_flag_1;
		TXMON_HAL_USER(ppdu_info, i, he_flags2) |= he_mu_flag_2;

		/* channel 1 */
		TXMON_HAL_USER(ppdu_info, i, he_RU[0]) =
					usr_common->ru_channel_0[0];
		TXMON_HAL_USER(ppdu_info, i, he_RU[1]) =
					usr_common->ru_channel_0[1];
		TXMON_HAL_USER(ppdu_info, i, he_RU[2]) =
					usr_common->ru_channel_0[2];
		TXMON_HAL_USER(ppdu_info, i, he_RU[3]) =
					usr_common->ru_channel_0[3];
		/* channel 2 */
		TXMON_HAL_USER(ppdu_info, i, he_RU[4]) =
					usr_common->ru_channel_1[0];
		TXMON_HAL_USER(ppdu_info, i, he_RU[5]) =
					usr_common->ru_channel_1[1];
		TXMON_HAL_USER(ppdu_info, i, he_RU[6]) =
					usr_common->ru_channel_1[2];
		TXMON_HAL_USER(ppdu_info, i, he_RU[7]) =
					usr_common->ru_channel_1[3];
	}
}

/**
 * hal_txmon_populate_eht_sig_common() - populate eht sig common information
 *
 * @usr_common: pointer to hal_txmon_usr_desc_common
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_populate_eht_sig_common(struct hal_txmon_usr_desc_common *usr_common,
				  uint32_t user_id,
				  struct hal_tx_ppdu_info *ppdu_info)
{
	uint32_t eht_known = 0;
	uint32_t eht_data[9] = {0};
	uint8_t num_ru_allocation_known = 0;
	uint8_t i = 0;

	eht_known = (QDF_MON_STATUS_EHT_SPATIAL_REUSE_KNOWN |
		     QDF_MON_STATUS_EHT_EHT_LTF_KNOWN |
		     QDF_MON_STATUS_EHT_PRE_FEC_PADDING_FACTOR_KNOWN |
		     QDF_MON_STATUS_EHT_PE_DISAMBIGUITY_KNOWN |
		     QDF_MON_STATUS_EHT_DISREARD_KNOWN);

	eht_data[0] |= (usr_common->spatial_reuse <<
			QDF_MON_STATUS_EHT_SPATIAL_REUSE_SHIFT);
	eht_data[0] |= (usr_common->num_ltf_symbols <<
			QDF_MON_STATUS_EHT_EHT_LTF_SHIFT);
	eht_data[0] |= (usr_common->a_factor <<
			QDF_MON_STATUS_EHT_PRE_FEC_PADDING_FACTOR_SHIFT);
	eht_data[0] |= (usr_common->pkt_extn_pe <<
			QDF_MON_STATUS_EHT_PE_DISAMBIGUITY_SHIFT);
	eht_data[0] |= (0xF << QDF_MON_STATUS_EHT_DISREGARD_SHIFT);

	switch (TXMON_HAL_STATUS(ppdu_info, bw)) {
	case HAL_EHT_BW_320_2:
	case HAL_EHT_BW_320_1:
		num_ru_allocation_known += 4;

		eht_data[3] |= (usr_common->ru_channel_0[7] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION2_6_SHIFT);
		eht_data[3] |= (usr_common->ru_channel_0[6] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION2_5_SHIFT);
		eht_data[3] |= (usr_common->ru_channel_0[5] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION2_4_SHIFT);
		eht_data[2] |= (usr_common->ru_channel_0[4] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION2_3_SHIFT);
		fallthrough;
	case HAL_EHT_BW_160:
		num_ru_allocation_known += 2;

		eht_data[2] |= (usr_common->ru_channel_0[3] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION2_2_SHIFT);
		eht_data[2] |= (usr_common->ru_channel_0[2] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION2_1_SHIFT);
		fallthrough;
	case HAL_EHT_BW_80:
		num_ru_allocation_known += 1;

		eht_data[1] |= (usr_common->ru_channel_0[1] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION1_2_SHIFT);
		fallthrough;
	case HAL_EHT_BW_40:
	case HAL_EHT_BW_20:
		num_ru_allocation_known += 1;

		eht_data[1] |= (usr_common->ru_channel_0[0] <<
				QDF_MON_STATUS_EHT_RU_ALLOCATION1_1_SHIFT);
		break;
	default:
		break;
	}

	eht_known |= (num_ru_allocation_known <<
		      QDF_MON_STATUS_EHT_NUM_KNOWN_RU_ALLOCATIONS_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, eht_known) |= eht_known;

	for (i = 0; i < 4; i++)
		TXMON_HAL_STATUS(ppdu_info, eht_data[i]) |= eht_data[i];
}

/**
 * hal_txmon_parse_user_desc_common() - parse mactx user desc common tlv
 *
 * @tx_tlv: pointer to mactx_user_desc_common tlv
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_user_desc_common(void *tx_tlv, uint32_t user_id,
				 struct hal_tx_ppdu_info *ppdu_info)
{
	struct hal_txmon_usr_desc_common usr_common = {0};

	usr_common.num_users = TXMON_HAL(ppdu_info, num_users);
	hal_txmon_get_user_desc_common(tx_tlv, &usr_common);

	TXMON_HAL_STATUS(ppdu_info,
			 he_mu_flags) = IS_MULTI_USERS(usr_common.num_users);

	switch (TXMON_HAL_STATUS(ppdu_info, preamble_type)) {
	case TXMON_PKT_TYPE_11AX:
		if (TXMON_HAL_STATUS(ppdu_info, he_flags))
			hal_txmon_populate_he_data_common(&usr_common,
							  user_id, ppdu_info);
		if (TXMON_HAL_STATUS(ppdu_info, he_mu_flags))
			hal_txmon_populate_he_mu_common(&usr_common,
							user_id, ppdu_info);
		break;
	case TXMON_PKT_TYPE_11BE:
		hal_txmon_populate_eht_sig_common(&usr_common,
						  user_id, ppdu_info);
		break;
	}
}

/**
 * hal_txmon_parse_eht_sig_non_mumimo_user_info() - parse eht sig non mumimo tlv
 *
 * @tx_tlv: pointer to hal_eht_sig_non_mu_mimo_user_info
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_eht_sig_non_mumimo_user_info(void *tx_tlv, uint32_t user_id,
					     struct hal_tx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_non_mu_mimo_user_info *user_info;
	uint32_t idx = TXMON_HAL_STATUS(ppdu_info, num_eht_user_info_valid);

	user_info = (struct hal_eht_sig_non_mu_mimo_user_info *)tx_tlv;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
		QDF_MON_STATUS_EHT_USER_STA_ID_KNOWN |
		QDF_MON_STATUS_EHT_USER_MCS_KNOWN |
		QDF_MON_STATUS_EHT_USER_CODING_KNOWN |
		QDF_MON_STATUS_EHT_USER_NSS_KNOWN |
		QDF_MON_STATUS_EHT_USER_BEAMFORMING_KNOWN;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->sta_id <<
				 QDF_MON_STATUS_EHT_USER_STA_ID_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->mcs <<
				 QDF_MON_STATUS_EHT_USER_MCS_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, mcs) = user_info->mcs;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->nss <<
				 QDF_MON_STATUS_EHT_USER_NSS_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, nss) = user_info->nss + 1;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->beamformed <<
				 QDF_MON_STATUS_EHT_USER_BEAMFORMING_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->coding <<
				 QDF_MON_STATUS_EHT_USER_CODING_SHIFT);

	/* TODO: CRC */

	TXMON_HAL_STATUS(ppdu_info, num_eht_user_info_valid) += 1;
}

/**
 * hal_txmon_parse_eht_sig_mumimo_user_info() - parse eht sig mumimo tlv
 *
 * @tx_tlv: pointer to hal_eht_sig_mu_mimo_user_info
 * @user_id: user index
 * @ppdu_info: pointer to hal_tx_ppdu_info
 *
 * Return: void
 */
static inline void
hal_txmon_parse_eht_sig_mumimo_user_info(void *tx_tlv, uint32_t user_id,
					 struct hal_tx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_mu_mimo_user_info *user_info;
	uint32_t idx = TXMON_HAL_STATUS(ppdu_info, num_eht_user_info_valid);

	user_info = (struct hal_eht_sig_mu_mimo_user_info *)tx_tlv;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
		QDF_MON_STATUS_EHT_USER_STA_ID_KNOWN |
		QDF_MON_STATUS_EHT_USER_MCS_KNOWN |
		QDF_MON_STATUS_EHT_USER_CODING_KNOWN |
		QDF_MON_STATUS_EHT_USER_SPATIAL_CONFIG_KNOWN;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->sta_id <<
				 QDF_MON_STATUS_EHT_USER_STA_ID_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->mcs <<
				 QDF_MON_STATUS_EHT_USER_MCS_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, mcs) = user_info->mcs;

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->coding <<
				 QDF_MON_STATUS_EHT_USER_CODING_SHIFT);

	TXMON_HAL_STATUS(ppdu_info, eht_user_info[idx]) |=
				(user_info->spatial_coding <<
				 QDF_MON_STATUS_EHT_USER_SPATIAL_CONFIG_SHIFT);
	/*  TODO: CRC */

	TXMON_HAL_STATUS(ppdu_info, num_eht_user_info_valid) += 1;
}

/**
 * hal_txmon_status_get_num_users_generic_be() - api to get num users
 * from start of fes window
 *
 * @tx_tlv_hdr: pointer to TLV header
 * @num_users: reference to number of user
 *
 * Return: status
 */
static inline uint32_t
hal_txmon_status_get_num_users_generic_be(void *tx_tlv_hdr, uint8_t *num_users)
{
	uint32_t tlv_tag, user_id, tlv_len;
	uint32_t tlv_status = HAL_MON_TX_STATUS_PPDU_NOT_DONE;
	void *tx_tlv;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(tx_tlv_hdr);
	user_id = HAL_RX_GET_USER_TLV32_USERID(tx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(tx_tlv_hdr);

	tx_tlv = (uint8_t *)tx_tlv_hdr + HAL_RX_TLV64_HDR_SIZE;
	/* window starts with either initiator or response */
	switch (tlv_tag) {
	case WIFITX_FES_SETUP_E:
	{
		*num_users = hal_txmon_get_num_users(tx_tlv);
		if (*num_users == 0)
			*num_users = 1;

		tlv_status = HAL_MON_TX_FES_SETUP;
		break;
	}
	case WIFIRX_RESPONSE_REQUIRED_INFO_E:
	{
		*num_users = HAL_TX_DESC_GET_64(tx_tlv,
						RX_RESPONSE_REQUIRED_INFO,
						RESPONSE_STA_COUNT);
		if (*num_users == 0)
			*num_users = 1;
		tlv_status = HAL_MON_RX_RESPONSE_REQUIRED_INFO;
		break;
	}
	};

	return tlv_status;
}

#ifdef MONITOR_TLV_RECORDING_ENABLE
static inline void
hal_tx_tlv_record_set_data_ppdu_info(struct hal_tx_ppdu_info *ppdu_info)
{
	ppdu_info->tx_tlv_info.is_data_ppdu_info = 1;
}
#else
static inline void
hal_tx_tlv_record_set_data_ppdu_info(struct hal_tx_ppdu_info *ppdu_info)
{
}
#endif
/**
 * hal_txmon_get_word_mask_generic_be() - api to get word mask for tx monitor
 * @wmask: pointer to hal_txmon_word_mask_config_t
 *
 * Return: void
 */
static inline
void hal_txmon_get_word_mask_generic_be(void *wmask)
{
	hal_txmon_word_mask_config_t *word_mask = NULL;

	word_mask = (hal_txmon_word_mask_config_t *)wmask;
	qdf_mem_set(word_mask, sizeof(hal_txmon_word_mask_config_t), 0xFF);
	word_mask->compaction_enable = 0;
}

/**
 * hal_tx_get_ppdu_info() - api to get tx ppdu info
 * @data_info: populate dp_ppdu_info data
 * @prot_info: populate dp_ppdu_info protection
 * @tlv_tag: Tag
 *
 * Return: dp_tx_ppdu_info pointer
 */
static inline void *
hal_tx_get_ppdu_info(void *data_info, void *prot_info, uint32_t tlv_tag)
{
	struct hal_tx_ppdu_info *prot_ppdu_info = prot_info;

	switch (tlv_tag) {
	case WIFITX_FES_SETUP_E:/* DOWNSTREAM */
	case WIFITX_FLUSH_E:/* DOWNSTREAM */
	case WIFIPCU_PPDU_SETUP_INIT_E:/* DOWNSTREAM */
	case WIFITX_PEER_ENTRY_E:/* DOWNSTREAM */
	case WIFITX_QUEUE_EXTENSION_E:/* DOWNSTREAM */
	case WIFITX_MPDU_START_E:/* DOWNSTREAM */
	case WIFITX_MSDU_START_E:/* DOWNSTREAM */
	case WIFITX_DATA_E:/* DOWNSTREAM */
	case WIFIMON_BUFFER_ADDR_E:/* DOWNSTREAM */
	case WIFITX_MPDU_END_E:/* DOWNSTREAM */
	case WIFITX_MSDU_END_E:/* DOWNSTREAM */
	case WIFITX_LAST_MPDU_FETCHED_E:/* DOWNSTREAM */
	case WIFITX_LAST_MPDU_END_E:/* DOWNSTREAM */
	case WIFICOEX_TX_REQ_E:/* DOWNSTREAM */
	case WIFITX_RAW_OR_NATIVE_FRAME_SETUP_E:/* DOWNSTREAM */
	case WIFINDP_PREAMBLE_DONE_E:/* DOWNSTREAM */
	case WIFISCH_CRITICAL_TLV_REFERENCE_E:/* DOWNSTREAM */
	case WIFITX_LOOPBACK_SETUP_E:/* DOWNSTREAM */
	case WIFITX_FES_SETUP_COMPLETE_E:/* DOWNSTREAM */
	case WIFITQM_MPDU_GLOBAL_START_E:/* DOWNSTREAM */
	case WIFITX_WUR_DATA_E:/* DOWNSTREAM */
	case WIFISCHEDULER_END_E:/* DOWNSTREAM */
	case WIFITX_FES_STATUS_START_PPDU_E:/* UPSTREAM */
	{
		hal_tx_tlv_record_set_data_ppdu_info(data_info);
		return data_info;
	}
	}

	/*
	 * check current prot_tlv_status is start protection
	 * check current tlv_tag is either start protection or end protection
	 */
	if (TXMON_HAL(prot_ppdu_info,
		      prot_tlv_status) == WIFITX_FES_STATUS_START_PROT_E) {
		return prot_info;
	} else if (tlv_tag == WIFITX_FES_STATUS_PROT_E ||
		   tlv_tag == WIFITX_FES_STATUS_START_PROT_E) {
		TXMON_HAL(prot_ppdu_info, prot_tlv_status) = tlv_tag;
		return prot_info;
	}

	hal_tx_tlv_record_set_data_ppdu_info(data_info);
	return data_info;
}

#ifdef MONITOR_TLV_RECORDING_ENABLE
static inline void
hal_tx_record_tlv_info(struct hal_tx_ppdu_info *ppdu_info,
		       uint32_t tlv_tag)
{
	ppdu_info->tx_tlv_info.tlv_tag = tlv_tag;
	switch (tlv_tag) {
	case WIFITX_FES_SETUP_E:
	case WIFITXPCU_BUFFER_STATUS_E:
	case WIFIPCU_PPDU_SETUP_INIT_E:
	case WIFISCH_CRITICAL_TLV_REFERENCE_E:
	case WIFITX_PEER_ENTRY_E:
	case WIFITX_RAW_OR_NATIVE_FRAME_SETUP_E:
	case WIFITX_QUEUE_EXTENSION_E:
	case WIFITX_FES_SETUP_COMPLETE_E:
	case WIFIFW2SW_MON_E:
	case WIFISCHEDULER_END_E:
	case WIFITQM_MPDU_GLOBAL_START_E:
		ppdu_info->tx_tlv_info.tlv_category = CATEGORY_PPDU_START;
		break;

	case WIFITX_MPDU_START_E:
	case WIFITX_MSDU_START_E:
	case WIFITX_DATA_E:
	case WIFITX_MSDU_END_E:
	case WIFITX_MPDU_END_E:
		ppdu_info->tx_tlv_info.tlv_category = CATEGORY_MPDU;
		break;

	case WIFITX_LAST_MPDU_FETCHED_E:
	case WIFITX_LAST_MPDU_END_E:
	case WIFIPDG_TX_REQ_E:
	case WIFITX_FES_STATUS_START_PPDU_E:
	case WIFIPHYTX_PPDU_HEADER_INFO_REQUEST_E:
	case WIFIMACTX_L_SIG_A_E:
	case WIFITXPCU_PREAMBLE_DONE_E:
	case WIFIMACTX_USER_DESC_COMMON_E:
	case WIFIMACTX_SERVICE_E:
	case WIFITXDMA_STOP_REQUEST_E:
	case WIFITXPCU_USER_BUFFER_STATUS_E:
	case WIFITX_FES_STATUS_USER_PPDU_E:
	case WIFITX_MPDU_COUNT_TRANSFER_END_E:
	case WIFIRX_START_PARAM_E:
	case WIFITX_FES_STATUS_ACK_OR_BA_E:
	case WIFITX_FES_STATUS_USER_RESPONSE_E:
	case WIFITX_FES_STATUS_END_E:
	case WIFITX_FES_STATUS_PROT_E:
	case WIFIMACTX_PHY_DESC_E:
	case WIFIMACTX_HE_SIG_A_SU_E:
		ppdu_info->tx_tlv_info.tlv_category = CATEGORY_PPDU_END;
		break;
	}
}
#else
static inline void
hal_tx_record_tlv_info(struct hal_tx_ppdu_info *ppdu_info,
		       uint32_t tlv_tag)
{
}
#endif

/**
 * hal_txmon_status_parse_tlv_generic_be() - api to parse status tlv.
 * @data_ppdu_info: hal_txmon data ppdu info
 * @prot_ppdu_info: hal_txmon prot ppdu info
 * @data_status_info: pointer to data status info
 * @prot_status_info: pointer to prot status info
 * @tx_tlv_hdr: fragment of tx_tlv_hdr
 * @status_frag: qdf_frag_t buffer
 *
 * Return: status
 */
static inline uint32_t
hal_txmon_status_parse_tlv_generic_be(void *data_ppdu_info,
				      void *prot_ppdu_info,
				      void *data_status_info,
				      void *prot_status_info,
				      void *tx_tlv_hdr,
				      qdf_frag_t status_frag)
{
	struct hal_tx_ppdu_info *ppdu_info;
	struct hal_tx_status_info *tx_status_info;
	struct hal_mon_packet_info *packet_info = NULL;
	uint32_t tlv_tag, user_id, tlv_len, tlv_user_id;
	uint32_t status = HAL_MON_TX_STATUS_PPDU_NOT_DONE;
	void *tx_tlv;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(tx_tlv_hdr);
	tlv_user_id = HAL_RX_GET_USER_TLV32_USERID(tx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(tx_tlv_hdr);

	tx_tlv = (uint8_t *)tx_tlv_hdr + HAL_RX_TLV64_HDR_SIZE;

	/* parse tlv and populate tx_ppdu_info */
	ppdu_info = hal_tx_get_ppdu_info(data_ppdu_info,
					 prot_ppdu_info, tlv_tag);
	tx_status_info = (ppdu_info->is_data ? data_status_info :
			  prot_status_info);

	user_id = (tlv_user_id > ppdu_info->num_users ? 0 : tlv_user_id);
	hal_tx_record_tlv_info(ppdu_info, tlv_tag);

	switch (tlv_tag) {
	/* start of initiator FES window */
	case WIFITX_FES_SETUP_E:/* DOWNSTREAM - COMPACTION */
	{
		/* initiator PPDU window start */
		hal_txmon_parse_tx_fes_setup(tx_tlv, ppdu_info);

		status = HAL_MON_TX_FES_SETUP;
		SHOW_DEFINED(WIFITX_FES_SETUP_E);
		break;
	}
	/* end of initiator FES window */
	case WIFITX_FES_STATUS_END_E:/* UPSTREAM - COMPACTION */
	{
		hal_txmon_parse_tx_fes_status_end(tx_tlv, ppdu_info,
						  tx_status_info);

		status = HAL_MON_TX_FES_STATUS_END;
		SHOW_DEFINED(WIFITX_FES_STATUS_END_E);
		break;
	}
	/* response window open */
	case WIFIRX_RESPONSE_REQUIRED_INFO_E:/* UPSTREAM */
	{
		/* response PPDU window start */
		uint32_t ppdu_id = 0;
		uint8_t reception_type = 0;
		uint8_t response_sta_count = 0;

		status = HAL_MON_RX_RESPONSE_REQUIRED_INFO;

		ppdu_id = HAL_TX_DESC_GET_64(tx_tlv,
					     RX_RESPONSE_REQUIRED_INFO,
					     PHY_PPDU_ID);
		reception_type =
			HAL_TX_DESC_GET_64(tx_tlv, RX_RESPONSE_REQUIRED_INFO,
					   SU_OR_UPLINK_MU_RECEPTION);
		response_sta_count =
			HAL_TX_DESC_GET_64(tx_tlv, RX_RESPONSE_REQUIRED_INFO,
					   RESPONSE_STA_COUNT);

		/* get mac address */
		*(uint32_t *)&tx_status_info->addr1[0] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_RESPONSE_REQUIRED_INFO,
						   ADDR1_31_0);
		*(uint32_t *)&tx_status_info->addr1[4] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_RESPONSE_REQUIRED_INFO,
						   ADDR1_47_32);
		*(uint32_t *)&tx_status_info->addr2[0] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_RESPONSE_REQUIRED_INFO,
						   ADDR2_15_0);
		*(uint32_t *)&tx_status_info->addr2[2] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_RESPONSE_REQUIRED_INFO,
						   ADDR2_47_16);

		TXMON_HAL(ppdu_info, ppdu_id) = ppdu_id;
		TXMON_HAL_STATUS(ppdu_info, ppdu_id) = ppdu_id;

		if (response_sta_count == 0)
			response_sta_count = 1;
		TXMON_HAL(ppdu_info, num_users) = response_sta_count;

		if (reception_type)
			TXMON_STATUS_INFO(tx_status_info,
					  transmission_type) =
							TXMON_SU_TRANSMISSION;
		else
			TXMON_STATUS_INFO(tx_status_info,
					  transmission_type) =
							TXMON_MU_TRANSMISSION;

		SHOW_DEFINED(WIFIRX_RESPONSE_REQUIRED_INFO_E);
		break;
	}
	/* Response window close */
	case WIFIRESPONSE_END_STATUS_E:/* UPSTREAM - COMPACTION */
	{
		/* response PPDU window end */
		hal_txmon_parse_response_end_status(tx_tlv, ppdu_info,
						    tx_status_info);

		status = HAL_MON_RESPONSE_END_STATUS_INFO;
		SHOW_DEFINED(WIFIRESPONSE_END_STATUS_E);
		break;
	}
	case WIFITX_FLUSH_E:/* DOWNSTREAM */
	{
		SHOW_DEFINED(WIFITX_FLUSH_E);
		break;
	}
	/* Downstream tlv */
	case WIFIPCU_PPDU_SETUP_INIT_E:/* DOWNSTREAM - COMPACTION */
	{
		hal_txmon_parse_pcu_ppdu_setup_init(tx_tlv, data_status_info,
						    prot_status_info);
		status = HAL_MON_TX_PCU_PPDU_SETUP_INIT;
		SHOW_DEFINED(WIFIPCU_PPDU_SETUP_INIT_E);
		break;
	}
	case WIFITX_PEER_ENTRY_E:/* DOWNSTREAM - COMPACTION */
	{
		hal_txmon_parse_peer_entry(tx_tlv, user_id,
					   ppdu_info, tx_status_info);
		SHOW_DEFINED(WIFITX_PEER_ENTRY_E);
		break;
	}
	case WIFITX_QUEUE_EXTENSION_E:/* DOWNSTREAM - COMPACTION */
	{
		status = HAL_MON_TX_QUEUE_EXTENSION;
		hal_txmon_parse_queue_exten(tx_tlv, ppdu_info);

		SHOW_DEFINED(WIFITX_QUEUE_EXTENSION_E);
		break;
	}
	/* payload and data frame handling */
	case WIFITX_MPDU_START_E:/* DOWNSTREAM - COMPACTION */
	{
		hal_txmon_parse_mpdu_start(tx_tlv, user_id, ppdu_info);

		status = HAL_MON_TX_MPDU_START;
		SHOW_DEFINED(WIFITX_MPDU_START_E);
		break;
	}
	case WIFITX_MSDU_START_E:/* DOWNSTREAM - COMPACTION */
	{
		hal_txmon_parse_msdu_start(tx_tlv, user_id, ppdu_info);
		/* we expect frame to be 802.11 frame type */
		status = HAL_MON_TX_MSDU_START;
		SHOW_DEFINED(WIFITX_MSDU_START_E);
		break;
	}
	case WIFITX_DATA_E:/* DOWNSTREAM */
	{
		status = HAL_MON_TX_DATA;
		/*
		 * TODO: do we need a conversion api to convert
		 * user_id from hw to get host user_index
		 */
		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;
		TXMON_STATUS_INFO(tx_status_info,
				  buffer) = (void *)status_frag;
		TXMON_STATUS_INFO(tx_status_info,
				  offset) = ((void *)tx_tlv -
					     (void *)status_frag);
		TXMON_STATUS_INFO(tx_status_info,
				  length) = tlv_len;

		/*
		 * reference of the status buffer will be held in
		 * dp_tx_update_ppdu_info_status()
		 */
		SHOW_DEFINED(WIFITX_DATA_E);
		break;
	}
	case WIFIMON_BUFFER_ADDR_E:/* DOWNSTREAM */
	{
		packet_info = &ppdu_info->packet_info;
		status = HAL_MON_TX_BUFFER_ADDR;
		/*
		 * TODO: do we need a conversion api to convert
		 * user_id from hw to get host user_index
		 */
		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;

		hal_txmon_populate_packet_info_generic_be(tx_tlv, packet_info);

		SHOW_DEFINED(WIFIMON_BUFFER_ADDR_E);
		break;
	}
	case WIFITX_MPDU_END_E:/* DOWNSTREAM */
	{
		/* no tlv content */
		SHOW_DEFINED(WIFITX_MPDU_END_E);
		break;
	}
	case WIFITX_MSDU_END_E:/* DOWNSTREAM */
	{
		/* no tlv content */
		SHOW_DEFINED(WIFITX_MSDU_END_E);
		break;
	}
	case WIFITX_LAST_MPDU_FETCHED_E:/* DOWNSTREAM */
	{
		/* no tlv content */
		SHOW_DEFINED(WIFITX_LAST_MPDU_FETCHED_E);
		break;
	}
	case WIFITX_LAST_MPDU_END_E:/* DOWNSTREAM */
	{
		/* no tlv content */
		SHOW_DEFINED(WIFITX_LAST_MPDU_END_E);
		break;
	}
	case WIFICOEX_TX_REQ_E:/* DOWNSTREAM */
	{
		/*
		 * transmitting power
		 * minimum transmitting power
		 * desired nss
		 * tx chain mask
		 * desired bw
		 * duration of transmit and response
		 *
		 * since most of the field we are deriving from other tlv
		 * we don't need to enable this in our tlv.
		 */
		SHOW_DEFINED(WIFICOEX_TX_REQ_E);
		break;
	}
	case WIFITX_RAW_OR_NATIVE_FRAME_SETUP_E:/* DOWNSTREAM */
	{
		/* user tlv */
		/*
		 * All Tx monitor will have 802.11 hdr
		 * we don't need to enable this TLV
		 */
		SHOW_DEFINED(WIFITX_RAW_OR_NATIVE_FRAME_SETUP_E);
		break;
	}
	case WIFINDP_PREAMBLE_DONE_E:/* DOWNSTREAM */
	{
		/*
		 * no tlv content
		 *
		 * TLV that indicates to TXPCU that preamble phase for the NDP
		 * frame transmission is now over
		 */
		SHOW_DEFINED(WIFINDP_PREAMBLE_DONE_E);
		break;
	}
	case WIFISCH_CRITICAL_TLV_REFERENCE_E:/* DOWNSTREAM */
	{
		/*
		 * no tlv content
		 *
		 * TLV indicates to the SCH that all timing critical TLV
		 * has been passed on to the transmit path
		 */
		SHOW_DEFINED(WIFISCH_CRITICAL_TLV_REFERENCE_E);
		break;
	}
	case WIFITX_LOOPBACK_SETUP_E:/* DOWNSTREAM */
	{
		/*
		 * Loopback specific setup info - not needed for Tx monitor
		 */
		SHOW_DEFINED(WIFITX_LOOPBACK_SETUP_E);
		break;
	}
	case WIFITX_FES_SETUP_COMPLETE_E:/* DOWNSTREAM */
	{
		/*
		 * no tlv content
		 *
		 * TLV indicates that other modules besides the scheduler can
		 * now also start generating TLV's
		 * prevent colliding or generating TLV's out of order
		 */
		SHOW_DEFINED(WIFITX_FES_SETUP_COMPLETE_E);
		break;
	}
	case WIFITQM_MPDU_GLOBAL_START_E:/* DOWNSTREAM */
	{
		/*
		 * no tlv content
		 *
		 * TLV indicates to SCH that a burst of MPDU info will
		 * start to come in over the TLV
		 */
		SHOW_DEFINED(WIFITQM_MPDU_GLOBAL_START_E);
		break;
	}
	case WIFITX_WUR_DATA_E:/* DOWNSTREAM */
	{
		SHOW_DEFINED(WIFITX_WUR_DATA_E);
		break;
	}
	case WIFISCHEDULER_END_E:/* DOWNSTREAM */
	{
		/*
		 * no tlv content
		 *
		 * TLV indicates END of all TLV's within the scheduler TLV
		 */
		SHOW_DEFINED(WIFISCHEDULER_END_E);
		break;
	}

	/* Upstream tlv */
	case WIFIPDG_TX_REQ_E:
	{
		SHOW_DEFINED(WIFIPDG_TX_REQ_E);
		break;
	}
	case WIFITX_FES_STATUS_START_E:
	{
		/*
		 * TLV indicating that first transmission on the medium
		 */
		uint8_t medium_prot_type = 0;

		status = HAL_MON_TX_FES_STATUS_START;

		medium_prot_type = HAL_TX_DESC_GET_64(tx_tlv,
						      TX_FES_STATUS_START,
						      MEDIUM_PROT_TYPE);

		ppdu_info = (struct hal_tx_ppdu_info *)prot_ppdu_info;
		/* update what type of medium protection frame */
		TXMON_STATUS_INFO(tx_status_info,
				  medium_prot_type) = medium_prot_type;
		SHOW_DEFINED(WIFITX_FES_STATUS_START_E);
		break;
	}
	case WIFITX_FES_STATUS_PROT_E:/* UPSTREAM - COMPACTION */
	{
		hal_txmon_parse_tx_fes_status_prot(tx_tlv, ppdu_info,
						   tx_status_info);

		status = HAL_MON_TX_FES_STATUS_PROT;
		TXMON_HAL(ppdu_info, prot_tlv_status) = tlv_tag;

		SHOW_DEFINED(WIFITX_FES_STATUS_PROT_E);
		break;
	}
	case WIFITX_FES_STATUS_START_PROT_E:
	{
		uint64_t tsft_64;
		uint32_t response_type;
		status = HAL_MON_TX_FES_STATUS_START_PROT;
		TXMON_HAL(ppdu_info, prot_tlv_status) = tlv_tag;
		/* timestamp */
		tsft_64 = HAL_TX_DESC_GET_64(tx_tlv,
					     TX_FES_STATUS_START_PROT,
					     PROT_TIMESTAMP_LOWER_32);
		tsft_64 |= (HAL_TX_DESC_GET_64(tx_tlv,
					       TX_FES_STATUS_START_PROT,
					       PROT_TIMESTAMP_UPPER_32) << 32);

		response_type = HAL_TX_DESC_GET_64(tx_tlv,
						   TX_FES_STATUS_START_PROT,
						   RESPONSE_TYPE);

		TXMON_STATUS_INFO(tx_status_info,
				  response_type) = response_type;
		TXMON_HAL_STATUS(ppdu_info, tsft) = tsft_64;

		SHOW_DEFINED(WIFITX_FES_STATUS_START_PROT_E);
		break;
	}
	case WIFIPROT_TX_END_E:
	{
		/*
		 * no tlv content
		 *
		 * generated by TXPCU the moment that protection frame
		 * transmission has finished on the medium
		 */
		SHOW_DEFINED(WIFIPROT_TX_END_E);
		break;
	}
	case WIFITX_FES_STATUS_START_PPDU_E:
	{
		uint64_t tsft_64;
		uint8_t ndp_frame;

		status = HAL_MON_TX_FES_STATUS_START_PPDU;
		tsft_64 = HAL_TX_DESC_GET_64(tx_tlv,
					     TX_FES_STATUS_START_PPDU,
					     PPDU_TIMESTAMP_LOWER_32);
		tsft_64 |= (HAL_TX_DESC_GET_64(tx_tlv,
					       TX_FES_STATUS_START_PPDU,
					       PPDU_TIMESTAMP_UPPER_32) << 32);

		ndp_frame = HAL_TX_DESC_GET_64(tx_tlv,
					       TX_FES_STATUS_START_PPDU,
					       NDP_FRAME);

		TXMON_STATUS_INFO(tx_status_info, ndp_frame) = ndp_frame;
		TXMON_HAL_STATUS(ppdu_info, tsft) = tsft_64;

		SHOW_DEFINED(WIFITX_FES_STATUS_START_PPDU_E);
		break;
	}
	case WIFITX_FES_STATUS_USER_PPDU_E:
	{
		/* user tlv */
		uint16_t duration;
		uint8_t transmitted_tid;

		duration = HAL_TX_DESC_GET_64(tx_tlv,
					      TX_FES_STATUS_USER_PPDU,
					      DURATION);
		transmitted_tid = HAL_TX_DESC_GET_64(tx_tlv,
						     TX_FES_STATUS_USER_PPDU,
						     TRANSMITTED_TID);

		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;
		TXMON_HAL_USER(ppdu_info, user_id, tid) = transmitted_tid;
		TXMON_HAL_USER(ppdu_info, user_id, duration) = duration;

		status = HAL_MON_TX_FES_STATUS_USER_PPDU;
		SHOW_DEFINED(WIFITX_FES_STATUS_USER_PPDU_E);
		break;
	}
	case WIFIPPDU_TX_END_E:
	{
		/*
		 * no tlv content
		 *
		 * generated by TXPCU the moment that PPDU transmission has
		 * finished on the medium
		 */
		SHOW_DEFINED(WIFIPPDU_TX_END_E);
		break;
	}

	case WIFITX_FES_STATUS_USER_RESPONSE_E:
	{
		/*
		 * TLV contains the FES transmit result of the each
		 * of the MAC users. TLV are forwarded to HWSCH
		 */
		SHOW_DEFINED(WIFITX_FES_STATUS_USER_RESPONSE_E);
		break;
	}
	case WIFITX_FES_STATUS_ACK_OR_BA_E:
	{
		/* user tlv */
		/*
		 * TLV generated by RXPCU and provide information related to
		 * the received BA or ACK frame
		 */
		SHOW_DEFINED(WIFITX_FES_STATUS_ACK_OR_BA_E);
		break;
	}
	case WIFITX_FES_STATUS_1K_BA_E:
	{
		/* user tlv */
		/*
		 * TLV generated by RXPCU and providing information related
		 * to the received BA frame in case of 512/1024 bitmaps
		 */
		SHOW_DEFINED(WIFITX_FES_STATUS_1K_BA_E);
		break;
	}
	case WIFIRECEIVED_RESPONSE_USER_7_0_E:
	{
		SHOW_DEFINED(WIFIRECEIVED_RESPONSE_USER_7_0_E);
		break;
	}
	case WIFIRECEIVED_RESPONSE_USER_15_8_E:
	{
		SHOW_DEFINED(WIFIRECEIVED_RESPONSE_USER_15_8_E);
		break;
	}
	case WIFIRECEIVED_RESPONSE_USER_23_16_E:
	{
		SHOW_DEFINED(WIFIRECEIVED_RESPONSE_USER_23_16_E);
		break;
	}
	case WIFIRECEIVED_RESPONSE_USER_31_24_E:
	{
		SHOW_DEFINED(WIFIRECEIVED_RESPONSE_USER_31_24_E);
		break;
	}
	case WIFIRECEIVED_RESPONSE_USER_36_32_E:
	{
		/*
		 * RXPCU generates this TLV when it receives a response frame
		 * that TXPCU pre-announced it was waiting for and in
		 * RXPCU_SETUP TLV, TLV generated before the
		 * RECEIVED_RESPONSE_INFO TLV.
		 *
		 * received info user fields are there which is not needed
		 * for TX monitor
		 */
		SHOW_DEFINED(WIFIRECEIVED_RESPONSE_USER_36_32_E);
		break;
	}

	case WIFITXPCU_BUFFER_STATUS_E:
	{
		SHOW_DEFINED(WIFITXPCU_BUFFER_STATUS_E);
		break;
	}
	case WIFITXPCU_USER_BUFFER_STATUS_E:
	{
		/*
		 * WIFITXPCU_USER_BUFFER_STATUS_E - user tlv
		 * for TX monitor we aren't interested in this tlv
		 */
		SHOW_DEFINED(WIFITXPCU_USER_BUFFER_STATUS_E);
		break;
	}
	case WIFITXDMA_STOP_REQUEST_E:
	{
		/*
		 * no tlv content
		 *
		 * TLV is destined to TXDMA and informs TXDMA to stop
		 * pushing data into the transmit path.
		 */
		SHOW_DEFINED(WIFITXDMA_STOP_REQUEST_E);
		break;
	}
	case WIFITX_CBF_INFO_E:
	{
		/*
		 * After NDPA + NDP is received, RXPCU sends the TX_CBF_INFO to
		 * TXPCU to respond the CBF frame
		 *
		 * compressed beamforming pkt doesn't has mac header
		 * Tx monitor not interested in this pkt.
		 */
		SHOW_DEFINED(WIFITX_CBF_INFO_E);
		break;
	}
	case WIFITX_MPDU_COUNT_TRANSFER_END_E:
	{
		/*
		 * no tlv content
		 *
		 * TLV indicates that TXPCU has finished generating the
		 * TQM_UPDATE_TX_MPDU_COUNT TLV for all users
		 */
		SHOW_DEFINED(WIFITX_MPDU_COUNT_TRANSFER_END_E);
		break;
	}
	case WIFIPDG_RESPONSE_E:
	{
		/*
		 * most of the feilds are already covered in
		 * other TLV
		 * This is generated by TX_PCU to PDG to calculate
		 * all the PHY header info.
		 *
		 * some useful fields like min transmit power,
		 * rate used for transmitting packet is present.
		 */
		SHOW_DEFINED(WIFIPDG_RESPONSE_E);
		break;
	}
	case WIFIPDG_TRIG_RESPONSE_E:
	{
		/* no tlv content */
		SHOW_DEFINED(WIFIPDG_TRIG_RESPONSE_E);
		break;
	}
	case WIFIRECEIVED_TRIGGER_INFO_E:
	{
		/*
		 * TLV generated by RXPCU to inform the scheduler that
		 * a trigger frame has been received
		 */
		SHOW_DEFINED(WIFIRECEIVED_TRIGGER_INFO_E);
		break;
	}
	case WIFIOFDMA_TRIGGER_DETAILS_E:
	{
		SHOW_DEFINED(WIFIOFDMA_TRIGGER_DETAILS_E);
		break;
	}
	case WIFIRX_FRAME_BITMAP_ACK_E:
	{
		/* user tlv */
		status = HAL_MON_RX_FRAME_BITMAP_ACK;
		SHOW_DEFINED(WIFIRX_FRAME_BITMAP_ACK_E);
		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;
		TXMON_STATUS_INFO(tx_status_info, no_bitmap_avail) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   NO_BITMAP_AVAILABLE);

		TXMON_STATUS_INFO(tx_status_info, explicit_ack) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   EXPLICIT_ACK);
		/*
		 * get mac address, since address is received frame
		 * change the order and store it
		 */
		*(uint32_t *)&tx_status_info->addr2[0] =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   ADDR1_31_0);
		*(uint16_t *)&tx_status_info->addr2[4] =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   ADDR1_47_32);
		*(uint32_t *)&tx_status_info->addr1[0] =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   ADDR2_15_0);
		*(uint32_t *)&tx_status_info->addr1[2] =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   ADDR2_47_16);

		TXMON_STATUS_INFO(tx_status_info, explicit_ack_type) =
				HAL_TX_DESC_GET_64(tx_tlv, RX_FRAME_BITMAP_ACK,
						   EXPLICT_ACK_TYPE);

		TXMON_HAL_USER(ppdu_info, user_id, tid) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   BA_TID);
		TXMON_HAL_USER(ppdu_info, user_id, aid) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   STA_FULL_AID);
		TXMON_HAL_USER(ppdu_info, user_id, start_seq) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   BA_TS_SEQ);
		TXMON_HAL_USER(ppdu_info, user_id, ba_control) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   BA_TS_CTRL);
		TXMON_HAL_USER(ppdu_info, user_id, ba_bitmap_sz) =
					HAL_TX_DESC_GET_64(tx_tlv,
							   RX_FRAME_BITMAP_ACK,
							   BA_BITMAP_SIZE);

		/* ba bitmap */
		qdf_mem_copy(TXMON_HAL_USER(ppdu_info, user_id, ba_bitmap),
			     &HAL_SET_FLD_OFFSET_64(tx_tlv,
						    RX_FRAME_BITMAP_ACK,
						    BA_TS_BITMAP_31_0, 0), 32);

		break;
	}
	case WIFIRX_FRAME_1K_BITMAP_ACK_E:
	{
		/* user tlv */
		status = HAL_MON_RX_FRAME_BITMAP_BLOCK_ACK_1K;
		SHOW_DEFINED(WIFIRX_FRAME_1K_BITMAP_ACK_E);
		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;
		TXMON_HAL_USER(ppdu_info, user_id, ba_bitmap_sz) =
			(4 + HAL_TX_DESC_GET_64(tx_tlv, RX_FRAME_1K_BITMAP_ACK,
						BA_BITMAP_SIZE));
		TXMON_HAL_USER(ppdu_info, user_id, tid) =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   BA_TID);
		TXMON_HAL_USER(ppdu_info, user_id, aid) =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   STA_FULL_AID);
		/* get mac address */
		*(uint32_t *)&tx_status_info->addr1[0] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   ADDR1_31_0);
		*(uint16_t *)&tx_status_info->addr1[4] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   ADDR1_47_32);
		*(uint32_t *)&tx_status_info->addr2[0] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   ADDR2_15_0);
		*(uint32_t *)&tx_status_info->addr2[2] =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   ADDR2_47_16);

		TXMON_HAL_USER(ppdu_info, user_id, start_seq) =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   BA_TS_SEQ);
		TXMON_HAL_USER(ppdu_info, user_id, ba_control) =
				HAL_TX_DESC_GET_64(tx_tlv,
						   RX_FRAME_1K_BITMAP_ACK,
						   BA_TS_CTRL);
		/* memcpy  ba bitmap */
		qdf_mem_copy(TXMON_HAL_USER(ppdu_info, user_id, ba_bitmap),
			     &HAL_SET_FLD_OFFSET_64(tx_tlv,
						    RX_FRAME_1K_BITMAP_ACK,
						    BA_TS_BITMAP_31_0, 0),
			     4 << TXMON_HAL_USER(ppdu_info,
						 user_id, ba_bitmap_sz));

		break;
	}
	case WIFIRESPONSE_START_STATUS_E:
	{
		/*
		 * TLV indicates which HW response the TXPCU
		 * started generating
		 *
		 * HW generated frames like
		 * ACK frame - handled
		 * CTS frame - handled
		 * BA frame - handled
		 * MBA frame - handled
		 * CBF frame - no frame header
		 * Trigger response - TODO
		 * NDP LMR - no frame header
		 */
		SHOW_DEFINED(WIFIRESPONSE_START_STATUS_E);
		break;
	}
	case WIFIRX_START_PARAM_E:
	{
		/*
		 * RXPCU send this TLV after PHY RX detected a frame
		 * in the medium
		 *
		 * TX monitor not interested in this TLV
		 */
		SHOW_DEFINED(WIFIRX_START_PARAM_E);
		break;
	}
	case WIFIRXPCU_EARLY_RX_INDICATION_E:
	{
		/*
		 * early indication of pkt type and mcs rate
		 * already captured in other tlv
		 */
		SHOW_DEFINED(WIFIRXPCU_EARLY_RX_INDICATION_E);
		break;
	}
	case WIFIRX_PM_INFO_E:
	{
		SHOW_DEFINED(WIFIRX_PM_INFO_E);
		break;
	}

	/* Active window */
	case WIFITX_FLUSH_REQ_E:
	{
		SHOW_DEFINED(WIFITX_FLUSH_REQ_E);
		break;
	}
	case WIFICOEX_TX_STATUS_E:
	{
		/* duration are retrieved from coex tx status */
		uint16_t duration;
		uint8_t status_reason;

		status = HAL_MON_COEX_TX_STATUS;
		duration = HAL_TX_DESC_GET_64(tx_tlv,
					      COEX_TX_STATUS,
					      CURRENT_TX_DURATION);
		status_reason = HAL_TX_DESC_GET_64(tx_tlv,
						   COEX_TX_STATUS,
						   TX_STATUS_REASON);

		/* update duration */
		if (status_reason == COEX_FES_TX_START ||
		    status_reason == COEX_RESPONSE_TX_START)
			TXMON_HAL_USER(ppdu_info, user_id, duration) = duration;

		SHOW_DEFINED(WIFICOEX_TX_STATUS_E);
		break;
	}
	case WIFIR2R_STATUS_END_E:
	{
		SHOW_DEFINED(WIFIR2R_STATUS_END_E);
		break;
	}
	case WIFIRX_PREAMBLE_E:
	{
		SHOW_DEFINED(WIFIRX_PREAMBLE_E);
		break;
	}
	case WIFIMACTX_SERVICE_E:
	{
		SHOW_DEFINED(WIFIMACTX_SERVICE_E);
		break;
	}

	case WIFIMACTX_U_SIG_EHT_SU_MU_E:
	{
		struct hal_mon_usig_hdr *usig = NULL;
		struct hal_mon_usig_mu *usig_mu = NULL;

		usig = (struct hal_mon_usig_hdr *)tx_tlv;
		usig_mu = &usig->usig_2.mu;

		hal_txmon_parse_u_sig_hdr(tx_tlv, ppdu_info);

		TXMON_HAL_STATUS(ppdu_info, usig_mask) |=
			QDF_MON_STATUS_USIG_DISREGARD_KNOWN |
			QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_KNOWN |
			QDF_MON_STATUS_USIG_VALIDATE_KNOWN |
			QDF_MON_STATUS_USIG_MU_VALIDATE1_KNOWN |
			QDF_MON_STATUS_USIG_MU_PUNCTURE_CH_INFO_KNOWN |
			QDF_MON_STATUS_USIG_MU_VALIDATE2_KNOWN |
			QDF_MON_STATUS_USIG_MU_EHT_SIG_MCS_KNOWN |
			QDF_MON_STATUS_USIG_MU_NUM_EHT_SIG_SYM_KNOWN |
			QDF_MON_STATUS_USIG_CRC_KNOWN |
			QDF_MON_STATUS_USIG_TAIL_KNOWN;

		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x1F << QDF_MON_STATUS_USIG_DISREGARD_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x1 << QDF_MON_STATUS_USIG_MU_VALIDATE1_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_mu->ppdu_type_comp_mode <<
			 QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x1 << QDF_MON_STATUS_USIG_VALIDATE_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_mu->punc_ch_info <<
			 QDF_MON_STATUS_USIG_MU_PUNCTURE_CH_INFO_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x1 << QDF_MON_STATUS_USIG_MU_VALIDATE2_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_mu->eht_sig_mcs <<
			 QDF_MON_STATUS_USIG_MU_EHT_SIG_MCS_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_mu->num_eht_sig_sym <<
			 QDF_MON_STATUS_USIG_MU_NUM_EHT_SIG_SYM_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_mu->crc << QDF_MON_STATUS_USIG_CRC_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_mu->tail << QDF_MON_STATUS_USIG_TAIL_SHIFT);

		SHOW_DEFINED(WIFIMACTX_U_SIG_EHT_SU_MU_E);
		break;
	}
	case WIFIMACTX_U_SIG_EHT_TB_E:
	{
		struct hal_mon_usig_hdr *usig = NULL;
		struct hal_mon_usig_tb *usig_tb = NULL;

		usig = (struct hal_mon_usig_hdr *)tx_tlv;
		usig_tb = &usig->usig_2.tb;

		hal_txmon_parse_u_sig_hdr(tx_tlv, ppdu_info);

		TXMON_HAL_STATUS(ppdu_info, usig_mask) |=
			QDF_MON_STATUS_USIG_DISREGARD_KNOWN |
			QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_KNOWN |
			QDF_MON_STATUS_USIG_VALIDATE_KNOWN |
			QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_1_KNOWN |
			QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_2_KNOWN |
			QDF_MON_STATUS_USIG_TB_DISREGARD1_KNOWN |
			QDF_MON_STATUS_USIG_CRC_KNOWN |
			QDF_MON_STATUS_USIG_TAIL_KNOWN;

		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x3F << QDF_MON_STATUS_USIG_DISREGARD_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_tb->ppdu_type_comp_mode <<
			 QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x1 << QDF_MON_STATUS_USIG_VALIDATE_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_tb->spatial_reuse_1 <<
			 QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_1_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_tb->spatial_reuse_2 <<
			 QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_2_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(0x1F << QDF_MON_STATUS_USIG_TB_DISREGARD1_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_tb->crc << QDF_MON_STATUS_USIG_CRC_SHIFT);
		TXMON_HAL_STATUS(ppdu_info, usig_value) |=
			(usig_tb->tail << QDF_MON_STATUS_USIG_TAIL_SHIFT);

		SHOW_DEFINED(WIFIMACTX_U_SIG_EHT_TB_E);
		break;
	}
	case WIFIMACTX_EHT_SIG_USR_OFDMA_E:
	{
		hal_txmon_parse_eht_sig_non_mumimo_user_info(tx_tlv, user_id,
							     ppdu_info);
		TXMON_HAL_STATUS(ppdu_info, eht_flags) = 1;
		SHOW_DEFINED(WIFIMACTX_EHT_SIG_USR_OFDMA_E);
		break;
	}
	case WIFIMACTX_EHT_SIG_USR_MU_MIMO_E:
	{
		hal_txmon_parse_eht_sig_mumimo_user_info(tx_tlv, user_id,
							 ppdu_info);
		TXMON_HAL_STATUS(ppdu_info, eht_flags) = 1;
		SHOW_DEFINED(WIFIMACTX_EHT_SIG_USR_MU_MIMO_E);
		break;
	}
	case WIFIMACTX_EHT_SIG_USR_SU_E:
	{
		hal_txmon_parse_eht_sig_non_mumimo_user_info(tx_tlv, user_id,
							     ppdu_info);
		TXMON_HAL_STATUS(ppdu_info, eht_flags) = 1;
		SHOW_DEFINED(WIFIMACTX_EHT_SIG_USR_SU_E);
		/* TODO: no radiotap info available */
		break;
	}

	case WIFIMACTX_HE_SIG_A_SU_E:
	{
		uint16_t he_mu_flag_1 = 0;
		uint16_t he_mu_flag_2 = 0;
		uint16_t num_users = 0;
		uint8_t mcs_of_sig_b = 0;
		uint8_t dcm_of_sig_b = 0;
		uint8_t sig_a_bw = 0;
		uint8_t i = 0;
		uint8_t bss_color_id;
		uint8_t coding;
		uint8_t stbc;
		uint8_t a_factor;
		uint8_t pe_disambiguity;
		uint8_t txbf;
		uint8_t txbw;
		uint8_t txop;

		status = HAL_MON_MACTX_HE_SIG_A_SU;
		num_users = TXMON_HAL(ppdu_info, num_users);

		mcs_of_sig_b = HAL_TX_DESC_GET_64(tx_tlv,
						  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
						  TRANSMIT_MCS);
		dcm_of_sig_b = HAL_TX_DESC_GET_64(tx_tlv,
						  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
						  DCM);
		sig_a_bw = HAL_TX_DESC_GET_64(tx_tlv,
					      MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					      TRANSMIT_BW);

		bss_color_id = HAL_TX_DESC_GET_64(tx_tlv,
						  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
						  BSS_COLOR_ID);
		coding = HAL_TX_DESC_GET_64(tx_tlv,
					    MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					    CODING);
		stbc = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					  STBC);
		a_factor = HAL_TX_DESC_GET_64(tx_tlv,
					      MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					      PACKET_EXTENSION_A_FACTOR);
		pe_disambiguity = HAL_TX_DESC_GET_64(tx_tlv,
						     MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
						     PACKET_EXTENSION_PE_DISAMBIGUITY);
		txbf = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					  TXBF);
		txbw = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					  TRANSMIT_BW);
		txop = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS,
					  TXOP_DURATION);

		he_mu_flag_1 |= QDF_MON_STATUS_SIG_B_MCS_KNOWN |
				QDF_MON_STATUS_SIG_B_DCM_KNOWN |
				QDF_MON_STATUS_CHANNEL_2_CENTER_26_RU_KNOWN |
				QDF_MON_STATUS_CHANNEL_1_RU_KNOWN |
				QDF_MON_STATUS_CHANNEL_2_RU_KNOWN |
				QDF_MON_STATUS_CHANNEL_1_CENTER_26_RU_KNOWN;

		/* MCS */
		he_mu_flag_1 |= mcs_of_sig_b <<
				QDF_MON_STATUS_SIG_B_MCS_SHIFT;
		/* DCM */
		he_mu_flag_1 |= dcm_of_sig_b <<
				QDF_MON_STATUS_SIG_B_DCM_SHIFT;
		/* bandwidth */
		he_mu_flag_2 |= QDF_MON_STATUS_SIG_A_BANDWIDTH_KNOWN;
		he_mu_flag_2 |= sig_a_bw <<
				QDF_MON_STATUS_SIG_A_BANDWIDTH_SHIFT;

		TXMON_HAL_STATUS(ppdu_info,
				 he_mu_flags) = IS_MULTI_USERS(num_users);
		for (i = 0; i < num_users; i++) {
			TXMON_HAL_USER(ppdu_info, i, he_flags1) |= he_mu_flag_1;
			TXMON_HAL_USER(ppdu_info, i, he_flags2) |= he_mu_flag_2;
		}

		/* HE data 1 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data1) |=
			QDF_MON_STATUS_HE_BSS_COLOR_KNOWN |
			QDF_MON_STATUS_HE_CODING_KNOWN;

		/* HE data 2 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data2) |=
			QDF_MON_STATUS_TXBF_KNOWN |
			QDF_MON_STATUS_PE_DISAMBIGUITY_KNOWN |
			QDF_MON_STATUS_TXOP_KNOWN |
			QDF_MON_STATUS_PRE_FEC_PADDING_KNOWN |
			QDF_MON_STATUS_MIDABLE_PERIODICITY_KNOWN;

		/* HE data 3 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |=
			bss_color_id |
			(!!txbf << QDF_MON_STATUS_BEAM_CHANGE_SHIFT) |
			(coding << QDF_MON_STATUS_CODING_SHIFT) |
			(stbc << QDF_MON_STATUS_STBC_SHIFT);

		/* HE data 6 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data6) |=
				(txop << QDF_MON_STATUS_TXOP_SHIFT);

		SHOW_DEFINED(WIFIMACTX_HE_SIG_A_SU_E);
		break;
	}
	case WIFIMACTX_HE_SIG_A_MU_DL_E:
	{
		uint16_t he_mu_flag_1 = 0;
		uint16_t he_mu_flag_2 = 0;
		uint16_t num_users = 0;
		uint8_t bss_color_id;
		uint8_t txop;
		uint8_t mcs_of_sig_b = 0;
		uint8_t dcm_of_sig_b = 0;
		uint8_t sig_a_bw = 0;
		uint8_t num_sig_b_symb = 0;
		uint8_t comp_mode_sig_b = 0;
		uint8_t punc_bw = 0;
		uint8_t i = 0;

		status = HAL_MON_MACTX_HE_SIG_A_MU_DL;
		num_users = TXMON_HAL(ppdu_info, num_users);

		mcs_of_sig_b = HAL_TX_DESC_GET_64(tx_tlv,
						  MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
						  MCS_OF_SIG_B);
		dcm_of_sig_b = HAL_TX_DESC_GET_64(tx_tlv,
						  MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
						  DCM_OF_SIG_B);
		sig_a_bw = HAL_TX_DESC_GET_64(tx_tlv,
					      MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
					      TRANSMIT_BW);
		num_sig_b_symb = HAL_TX_DESC_GET_64(tx_tlv,
						    MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
						    NUM_SIG_B_SYMBOLS);
		comp_mode_sig_b = HAL_TX_DESC_GET_64(tx_tlv,
						     MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
						     COMP_MODE_SIG_B);
		bss_color_id = HAL_TX_DESC_GET_64(tx_tlv,
						  MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
						  BSS_COLOR_ID);
		txop = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_HE_SIG_A_MU_DL_MACTX_HE_SIG_A_MU_DL_INFO_DETAILS,
					  TXOP_DURATION);

		he_mu_flag_1 |= QDF_MON_STATUS_SIG_B_MCS_KNOWN |
				QDF_MON_STATUS_SIG_B_DCM_KNOWN |
				QDF_MON_STATUS_SIG_B_SYM_NUM_KNOWN |
				QDF_MON_STATUS_CHANNEL_2_CENTER_26_RU_KNOWN |
				QDF_MON_STATUS_CHANNEL_1_RU_KNOWN |
				QDF_MON_STATUS_CHANNEL_2_RU_KNOWN |
				QDF_MON_STATUS_CHANNEL_1_CENTER_26_RU_KNOWN |
				QDF_MON_STATUS_SIG_B_COMPRESSION_FLAG_1_KNOWN |
				QDF_MON_STATUS_SIG_B_SYMBOL_USER_KNOWN;

		/* MCS */
		he_mu_flag_1 |= mcs_of_sig_b <<
				QDF_MON_STATUS_SIG_B_MCS_SHIFT;
		/* DCM */
		he_mu_flag_1 |= dcm_of_sig_b <<
				QDF_MON_STATUS_SIG_B_DCM_SHIFT;
		/* Compression */
		he_mu_flag_2 |= comp_mode_sig_b <<
				QDF_MON_STATUS_SIG_B_COMPRESSION_FLAG_2_SHIFT;
		/* bandwidth */
		he_mu_flag_2 |= QDF_MON_STATUS_SIG_A_BANDWIDTH_KNOWN;
		he_mu_flag_2 |= sig_a_bw <<
				QDF_MON_STATUS_SIG_A_BANDWIDTH_SHIFT;
		he_mu_flag_2 |= comp_mode_sig_b <<
				QDF_MON_STATUS_SIG_B_COMPRESSION_FLAG_2_SHIFT;
		/* number of symbol */
		he_mu_flag_2 |= num_sig_b_symb <<
				QDF_MON_STATUS_NUM_SIG_B_SYMBOLS_SHIFT;
		/* puncture bw */
		he_mu_flag_2 |= QDF_MON_STATUS_SIG_A_PUNC_BANDWIDTH_KNOWN;
		punc_bw = sig_a_bw;
		he_mu_flag_2 |=
			punc_bw << QDF_MON_STATUS_SIG_A_PUNC_BANDWIDTH_SHIFT;

		/* copy per user info to all user */
		TXMON_HAL_STATUS(ppdu_info,
				 he_mu_flags) = IS_MULTI_USERS(num_users);
		for (i = 0; i < num_users; i++) {
			TXMON_HAL_USER(ppdu_info, i, he_flags1) |= he_mu_flag_1;
			TXMON_HAL_USER(ppdu_info, i, he_flags2) |= he_mu_flag_2;
		}

		/* HE data 1 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data1) |=
				QDF_MON_STATUS_HE_BSS_COLOR_KNOWN;

		/* HE data 2 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data2) |=
				QDF_MON_STATUS_TXOP_KNOWN;

		/* HE data 3 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |= bss_color_id;

		/* HE data 6 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data6) |=
				(txop << QDF_MON_STATUS_TXOP_SHIFT);

		SHOW_DEFINED(WIFIMACTX_HE_SIG_A_MU_DL_E);
		break;
	}
	case WIFIMACTX_HE_SIG_A_MU_UL_E:
	{
		SHOW_DEFINED(WIFIMACTX_HE_SIG_A_MU_UL_E);
		break;
	}
	case WIFIMACTX_HE_SIG_B1_MU_E:
	{
		status = HAL_MON_MACTX_HE_SIG_B1_MU;
		SHOW_DEFINED(WIFIMACTX_HE_SIG_B1_MU_E);
		break;
	}
	case WIFIMACTX_HE_SIG_B2_MU_E:
	{
		/* user tlv */
		uint16_t sta_id = 0;
		uint16_t sta_spatial_config = 0;
		uint8_t sta_mcs = 0;
		uint8_t coding = 0;
		uint8_t nss = 0;
		uint8_t user_order = 0;

		status = HAL_MON_MACTX_HE_SIG_B2_MU;

		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;

		sta_id = HAL_TX_DESC_GET_64(tx_tlv,
					    MACTX_HE_SIG_B2_MU_MACTX_HE_SIG_B2_MU_INFO_DETAILS,
					    STA_ID);
		sta_spatial_config = HAL_TX_DESC_GET_64(tx_tlv,
							MACTX_HE_SIG_B2_MU_MACTX_HE_SIG_B2_MU_INFO_DETAILS,
							STA_SPATIAL_CONFIG);
		sta_mcs = HAL_TX_DESC_GET_64(tx_tlv,
					     MACTX_HE_SIG_B2_MU_MACTX_HE_SIG_B2_MU_INFO_DETAILS,
					     STA_MCS);
		coding = HAL_TX_DESC_GET_64(tx_tlv,
					    MACTX_HE_SIG_B2_MU_MACTX_HE_SIG_B2_MU_INFO_DETAILS,
					    STA_CODING);
		nss = HAL_TX_DESC_GET_64(tx_tlv,
					 MACTX_HE_SIG_B2_MU_MACTX_HE_SIG_B2_MU_INFO_DETAILS,
					 NSTS) + 1;
		user_order = HAL_TX_DESC_GET_64(tx_tlv,
						MACTX_HE_SIG_B2_MU_MACTX_HE_SIG_B2_MU_INFO_DETAILS,
						USER_ORDER);

		/* HE data 1 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data1) |=
				QDF_MON_STATUS_HE_MCS_KNOWN |
				QDF_MON_STATUS_HE_CODING_KNOWN;
		/* HE data 2 */

		/* HE data 3 */
		TXMON_HAL_USER(ppdu_info, user_id, mcs) = sta_mcs;
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |=
				sta_mcs << QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |=
				coding << QDF_MON_STATUS_CODING_SHIFT;

		/* HE data 4 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data4) |=
				sta_id << QDF_MON_STATUS_STA_ID_SHIFT;

		/* HE data 5 */

		/* HE data 6 */
		TXMON_HAL_USER(ppdu_info, user_id, nss) = nss;
		TXMON_HAL_USER(ppdu_info, user_id, he_data6) |= nss;

		SHOW_DEFINED(WIFIMACTX_HE_SIG_B2_MU_E);
		break;
	}
	case WIFIMACTX_HE_SIG_B2_OFDMA_E:
	{
		/* user tlv */
		uint8_t *he_sig_b2_ofdma_info = NULL;
		uint16_t sta_id = 0;
		uint8_t nss = 0;
		uint8_t txbf = 0;
		uint8_t sta_mcs = 0;
		uint8_t sta_dcm = 0;
		uint8_t coding = 0;
		uint8_t user_order = 0;

		status = HAL_MON_MACTX_HE_SIG_B2_OFDMA;

		TXMON_HAL(ppdu_info, cur_usr_idx) = user_id;

		he_sig_b2_ofdma_info = (uint8_t *)tx_tlv +
			HAL_OFFSET(MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
				   STA_ID);

		sta_id = HAL_TX_DESC_GET_64(tx_tlv,
					    MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
					    STA_ID);
		nss = HAL_TX_DESC_GET_64(tx_tlv,
					 MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
					 NSTS);
		txbf = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
					  TXBF);
		sta_mcs = HAL_TX_DESC_GET_64(tx_tlv,
					     MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
					     STA_MCS);
		sta_dcm = HAL_TX_DESC_GET_64(tx_tlv,
					     MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
					     STA_DCM);
		coding = HAL_TX_DESC_GET_64(tx_tlv,
					    MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
					    STA_CODING);
		user_order = HAL_TX_DESC_GET_64(tx_tlv,
						MACTX_HE_SIG_B2_OFDMA_MACTX_HE_SIG_B2_OFDMA_INFO_DETAILS,
						USER_ORDER);

		/* HE data 1 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data1) |=
				QDF_MON_STATUS_HE_MCS_KNOWN |
				QDF_MON_STATUS_HE_CODING_KNOWN |
				QDF_MON_STATUS_HE_DCM_KNOWN;
		/* HE data 2 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data2) |=
				QDF_MON_STATUS_TXBF_KNOWN;

		/* HE data 3 */
		TXMON_HAL_USER(ppdu_info, user_id, mcs) = sta_mcs;
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |=
				sta_mcs << QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |=
				sta_dcm << QDF_MON_STATUS_DCM_SHIFT;
		TXMON_HAL_USER(ppdu_info, user_id, he_data3) |=
				coding << QDF_MON_STATUS_CODING_SHIFT;

		/* HE data 4 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data4) |=
				sta_id << QDF_MON_STATUS_STA_ID_SHIFT;

		/* HE data 5 */
		TXMON_HAL_USER(ppdu_info, user_id, he_data5) |=
				txbf << QDF_MON_STATUS_TXBF_SHIFT;

		/* HE data 6 */
		TXMON_HAL_USER(ppdu_info, user_id, nss) = nss;
		TXMON_HAL_USER(ppdu_info, user_id, he_data6) |= nss;

		SHOW_DEFINED(WIFIMACTX_HE_SIG_B2_OFDMA_E);
		break;
	}
	case WIFIMACTX_L_SIG_A_E:
	{
		uint8_t *l_sig_a_info = NULL;
		uint8_t rate = 0;

		status = HAL_MON_MACTX_L_SIG_A;

		l_sig_a_info = (uint8_t *)tx_tlv +
			HAL_OFFSET(MACTX_L_SIG_A_MACTX_L_SIG_A_INFO_DETAILS,
				   RATE);
		rate = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_L_SIG_A_MACTX_L_SIG_A_INFO_DETAILS,
					  RATE);

		switch (rate) {
		case 8:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_0MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS0;
			break;
		case 9:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_1MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS1;
			break;
		case 10:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_2MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS2;
			break;
		case 11:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_3MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS3;
			break;
		case 12:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_4MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS4;
			break;
		case 13:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_5MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS5;
			break;
		case 14:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_6MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS6;
			break;
		case 15:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11A_RATE_7MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS7;
			break;
		default:
			break;
		}

		TXMON_HAL_STATUS(ppdu_info, ofdm_flag) = 1;
		TXMON_HAL_STATUS(ppdu_info, reception_type) = HAL_RX_TYPE_SU;
		TXMON_HAL_STATUS(ppdu_info,
				 l_sig_a_info) = *((uint32_t *)l_sig_a_info);

		SHOW_DEFINED(WIFIMACTX_L_SIG_A_E);
		break;
	}
	case WIFIMACTX_L_SIG_B_E:
	{
		uint8_t *l_sig_b_info = NULL;
		uint8_t rate = 0;

		status = HAL_MON_MACTX_L_SIG_B;

		l_sig_b_info = (uint8_t *)tx_tlv +
			HAL_OFFSET(MACTX_L_SIG_B_MACTX_L_SIG_B_INFO_DETAILS,
				   RATE);
		rate = HAL_TX_DESC_GET_64(tx_tlv,
					  MACTX_L_SIG_B_MACTX_L_SIG_B_INFO_DETAILS,
					  RATE);

		switch (rate) {
		case 1:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_3MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS3;
			break;
		case 2:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_2MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS2;
			break;
		case 3:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_1MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS1;
			break;
		case 4:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_0MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS0;
			break;
		case 5:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_6MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS6;
			break;
		case 6:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_5MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS5;
			break;
		case 7:
			TXMON_HAL_STATUS(ppdu_info, rate) = HAL_11B_RATE_4MCS;
			TXMON_HAL_STATUS(ppdu_info, mcs) = HAL_LEGACY_MCS4;
			break;
		default:
			break;
		}

		TXMON_HAL_STATUS(ppdu_info, cck_flag) = 1;
		TXMON_HAL_STATUS(ppdu_info, reception_type) = HAL_RX_TYPE_SU;
		TXMON_HAL_STATUS(ppdu_info, l_sig_b_info) = *l_sig_b_info;

		SHOW_DEFINED(WIFIMACTX_L_SIG_B_E);
		break;
	}
	case WIFIMACTX_HT_SIG_E:
	{
		uint8_t mcs = 0;
		uint8_t bw = 0;
		uint8_t is_stbc = 0;
		uint8_t coding = 0;
		uint8_t gi = 0;

		status = HAL_MON_MACTX_HT_SIG;
		mcs = HAL_TX_DESC_GET_64(tx_tlv, HT_SIG_INFO, MCS);
		bw = HAL_TX_DESC_GET_64(tx_tlv, HT_SIG_INFO, CBW);
		is_stbc = HAL_TX_DESC_GET_64(tx_tlv, HT_SIG_INFO, STBC);
		coding = HAL_TX_DESC_GET_64(tx_tlv, HT_SIG_INFO, FEC_CODING);
		gi = HAL_TX_DESC_GET_64(tx_tlv, HT_SIG_INFO, SHORT_GI);

		TXMON_HAL_STATUS(ppdu_info, ldpc) =
				(coding == HAL_SU_MU_CODING_LDPC) ? 1 : 0;
		TXMON_HAL_STATUS(ppdu_info, ht_mcs) = mcs;
		TXMON_HAL_STATUS(ppdu_info, bw) = bw;
		TXMON_HAL_STATUS(ppdu_info, sgi) = gi;
		TXMON_HAL_STATUS(ppdu_info, is_stbc) = is_stbc;
		TXMON_HAL_STATUS(ppdu_info, reception_type) = HAL_RX_TYPE_SU;

		SHOW_DEFINED(WIFIMACTX_HT_SIG_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_A_E:
	{
		uint8_t bandwidth = 0;
		uint8_t is_stbc = 0;
		uint8_t group_id = 0;
		uint32_t nss_comb = 0;
		uint8_t nss_su = 0;
		uint8_t nss_mu[4] = {0};
		uint8_t sgi = 0;
		uint8_t coding = 0;
		uint8_t mcs = 0;
		uint8_t beamformed = 0;
		uint8_t partial_aid = 0;

		status = HAL_MON_MACTX_VHT_SIG_A;
		bandwidth = HAL_TX_DESC_GET_64(tx_tlv,
					       MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					       BANDWIDTH);
		is_stbc = HAL_TX_DESC_GET_64(tx_tlv,
					     MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					     STBC);
		group_id = HAL_TX_DESC_GET_64(tx_tlv,
					      MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					      GROUP_ID);
		/* nss_comb is su nss, MU nss and partial AID */
		nss_comb = HAL_TX_DESC_GET_64(tx_tlv,
					      MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					      N_STS);
		/* if it is SU */
		nss_su = (nss_comb & 0x7) + 1;
		/* partial aid - applicable only for SU */
		partial_aid = (nss_comb >> 3) & 0x1F;
		/* if it is MU */
		nss_mu[0] = (nss_comb & 0x7) + 1;
		nss_mu[1] = ((nss_comb >> 3) & 0x7) + 1;
		nss_mu[2] = ((nss_comb >> 6) & 0x7) + 1;
		nss_mu[3] = ((nss_comb >> 9) & 0x7) + 1;

		sgi = HAL_TX_DESC_GET_64(tx_tlv,
					 MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					 GI_SETTING);
		coding = HAL_TX_DESC_GET_64(tx_tlv,
					    MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					    SU_MU_CODING);
		mcs = HAL_TX_DESC_GET_64(tx_tlv,
					 MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
					 MCS);
		beamformed = HAL_TX_DESC_GET_64(tx_tlv,
						MACTX_VHT_SIG_A_MACTX_VHT_SIG_A_INFO_DETAILS,
						BEAMFORMED);

		TXMON_HAL_STATUS(ppdu_info, ldpc) =
			(coding == HAL_SU_MU_CODING_LDPC) ? 1 : 0;
		TXMON_STATUS_INFO(tx_status_info, sw_frame_group_id) = group_id;

		TXMON_HAL_STATUS(ppdu_info, sgi) = sgi;
		TXMON_HAL_STATUS(ppdu_info, is_stbc) = is_stbc;
		TXMON_HAL_STATUS(ppdu_info, bw) = bandwidth;
		TXMON_HAL_STATUS(ppdu_info, beamformed) = beamformed;

		if (group_id == 0 || group_id == 63) {
			TXMON_HAL_STATUS(ppdu_info, reception_type) =
						HAL_RX_TYPE_SU;
			TXMON_HAL_STATUS(ppdu_info, mcs) = mcs;
			TXMON_HAL_STATUS(ppdu_info, nss) =
						nss_su & VHT_SIG_SU_NSS_MASK;

			TXMON_HAL_USER(ppdu_info, user_id,
				       vht_flag_values3[0]) = ((mcs << 4) |
							       nss_su);
		} else {
			TXMON_HAL_STATUS(ppdu_info, reception_type) =
						HAL_RX_TYPE_MU_MIMO;
			TXMON_HAL_USER(ppdu_info, user_id, mcs) = mcs;
			TXMON_HAL_USER(ppdu_info, user_id, nss) =
						nss_su & VHT_SIG_SU_NSS_MASK;

			TXMON_HAL_USER(ppdu_info, user_id,
				       vht_flag_values3[0]) = ((mcs << 4) |
							       nss_su);
			TXMON_HAL_USER(ppdu_info, user_id,
				       vht_flag_values3[1]) = ((mcs << 4) |
							       nss_mu[1]);
			TXMON_HAL_USER(ppdu_info, user_id,
				       vht_flag_values3[2]) = ((mcs << 4) |
							       nss_mu[2]);
			TXMON_HAL_USER(ppdu_info, user_id,
				       vht_flag_values3[3]) = ((mcs << 4) |
							       nss_mu[3]);
		}

		/* TODO: loop over multiple user */
		TXMON_HAL_USER(ppdu_info, user_id,
			       vht_flag_values2) = bandwidth;
		TXMON_HAL_USER(ppdu_info, user_id,
			       vht_flag_values4) = coding;
		TXMON_HAL_USER(ppdu_info, user_id,
			       vht_flag_values5) = group_id;
		TXMON_HAL_USER(ppdu_info, user_id,
			       vht_flag_values6) = partial_aid;
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_A_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_MU160_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_MU160_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_MU80_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_MU80_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_MU40_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_MU40_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_MU20_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_MU20_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_SU160_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_SU160_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_SU80_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_SU80_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_SU40_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_SU40_E);
		break;
	}
	case WIFIMACTX_VHT_SIG_B_SU20_E:
	{
		SHOW_DEFINED(WIFIMACTX_VHT_SIG_B_SU20_E);
		break;
	}
	case WIFIPHYTX_PPDU_HEADER_INFO_REQUEST_E:
	{
		SHOW_DEFINED(WIFIPHYTX_PPDU_HEADER_INFO_REQUEST_E);
		break;
	}
	case WIFIMACTX_USER_DESC_PER_USER_E:
	{
		hal_txmon_parse_user_desc_per_user(tx_tlv, user_id, ppdu_info);

		SHOW_DEFINED(WIFIMACTX_USER_DESC_PER_USER_E);
		break;
	}
	case WIFIMACTX_USER_DESC_COMMON_E:
	{
		hal_txmon_parse_user_desc_common(tx_tlv, user_id, ppdu_info);

		/* copy per user info to all user */
		SHOW_DEFINED(WIFIMACTX_USER_DESC_COMMON_E);
		break;
	}
	case WIFIMACTX_PHY_DESC_E:
	{
		/* pkt_type - preamble type */
		uint32_t pkt_type = 0;
		uint8_t bandwidth = 0;
		uint8_t is_stbc = 0;
		uint8_t is_triggered = 0;
		uint8_t gi = 0;
		uint8_t he_ppdu_subtype = 0;
		uint32_t ltf_size = 0;
		uint32_t he_data1 = 0;
		uint32_t he_data2 = 0;
		uint32_t he_data3 = 0;
		uint32_t he_data5 = 0;
		uint16_t he_mu_flag_1 = 0;
		uint16_t he_mu_flag_2 = 0;
		uint16_t num_users = 0;
		uint8_t i = 0;

		SHOW_DEFINED(WIFIMACTX_PHY_DESC_E);
		status = HAL_MON_MACTX_PHY_DESC;

		num_users = TXMON_HAL(ppdu_info, num_users);
		pkt_type = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC, PKT_TYPE);
		is_stbc = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC, STBC);
		is_triggered = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC,
						  TRIGGERED);
		if (!is_triggered) {
			bandwidth = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC,
						       BANDWIDTH);
		} else {
			/*
			 * is_triggered, bw is minimum of AP pkt bw
			 * or STA bw
			 */
			bandwidth = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC,
						       AP_PKT_BW);
		}

		gi = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC,
					CP_SETTING);
		ltf_size = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC, LTF_SIZE);
		he_ppdu_subtype = HAL_TX_DESC_GET_64(tx_tlv, MACTX_PHY_DESC,
						     HE_PPDU_SUBTYPE);

		TXMON_HAL_STATUS(ppdu_info, preamble_type) = pkt_type;
		TXMON_HAL_STATUS(ppdu_info, ltf_size) = ltf_size;
		TXMON_HAL_STATUS(ppdu_info, is_stbc) = is_stbc;
		TXMON_HAL_STATUS(ppdu_info, bw) = bandwidth;

		switch (ppdu_info->rx_status.preamble_type) {
		case TXMON_PKT_TYPE_11N_MM:
			TXMON_HAL_STATUS(ppdu_info, ht_flags) = 1;
			TXMON_HAL_STATUS(ppdu_info,
					 rtap_flags) |= HT_SGI_PRESENT;
			break;
		case TXMON_PKT_TYPE_11AC:
			TXMON_HAL_STATUS(ppdu_info, vht_flags) = 1;
			break;
		case TXMON_PKT_TYPE_11AX:
			TXMON_HAL_STATUS(ppdu_info, he_flags) = 1;
			break;
		default:
			break;
		}

		if (!TXMON_HAL_STATUS(ppdu_info, he_flags))
			break;

		/* update he flags */
		/* PPDU FORMAT */
		switch (he_ppdu_subtype) {
		case TXMON_HE_SUBTYPE_SU:
			TXMON_HAL_STATUS(ppdu_info, he_data1) |=
					QDF_MON_STATUS_HE_SU_FORMAT_TYPE;
			break;
		case TXMON_HE_SUBTYPE_TRIG:
			TXMON_HAL_STATUS(ppdu_info, he_data1) |=
					QDF_MON_STATUS_HE_TRIG_FORMAT_TYPE;
			break;
		case TXMON_HE_SUBTYPE_MU:
			TXMON_HAL_STATUS(ppdu_info, he_data1) |=
					QDF_MON_STATUS_HE_MU_FORMAT_TYPE;
			break;
		case TXMON_HE_SUBTYPE_EXT_SU:
			TXMON_HAL_STATUS(ppdu_info, he_data1) |=
					QDF_MON_STATUS_HE_EXT_SU_FORMAT_TYPE;
			break;
		};

		/* STBC */
		he_data1 |= QDF_MON_STATUS_HE_STBC_KNOWN;
		he_data3 |= (is_stbc << QDF_MON_STATUS_STBC_SHIFT);

		/* GI */
		he_data2 |= QDF_MON_STATUS_HE_GI_KNOWN;
		he_data5 |= (gi << QDF_MON_STATUS_GI_SHIFT);

		/* Data BW and RU allocation */
		he_data1 |= QDF_MON_STATUS_HE_DATA_BW_RU_KNOWN;
		he_data5 = (he_data5 & 0xFFF0) | bandwidth;

		he_data2 |= QDF_MON_STATUS_LTF_SYMBOLS_KNOWN;
		he_data5 |= ((1 + ltf_size) <<
			     QDF_MON_STATUS_HE_LTF_SIZE_SHIFT);

		TXMON_HAL_STATUS(ppdu_info,
				 he_mu_flags) = IS_MULTI_USERS(num_users);
		/* MAC TX PHY DESC is not a user tlv */
		for (i = 0; i < num_users; i++) {
			TXMON_HAL_USER(ppdu_info, i, he_data1) = he_data1;
			TXMON_HAL_USER(ppdu_info, i, he_data2) = he_data2;
			TXMON_HAL_USER(ppdu_info, i, he_data3) = he_data3;
			TXMON_HAL_USER(ppdu_info, i, he_data5) = he_data5;

			/* HE MU flags */
			TXMON_HAL_USER(ppdu_info, i, he_flags1) |= he_mu_flag_1;
			TXMON_HAL_USER(ppdu_info, i, he_flags2) |= he_mu_flag_2;
		}
		break;
	}
	case WIFICOEX_RX_STATUS_E:
	{
		SHOW_DEFINED(WIFICOEX_RX_STATUS_E);
		break;
	}
	case WIFIRX_PPDU_ACK_REPORT_E:
	{
		SHOW_DEFINED(WIFIRX_PPDU_ACK_REPORT_E);
		break;
	}
	case WIFIRX_PPDU_NO_ACK_REPORT_E:
	{
		SHOW_DEFINED(WIFIRX_PPDU_NO_ACK_REPORT_E);
		break;
	}
	case WIFITXPCU_PHYTX_OTHER_TRANSMIT_INFO32_E:
	{
		SHOW_DEFINED(WIFITXPCU_PHYTX_OTHER_TRANSMIT_INFO32_E);
		break;
	}
	case WIFITXPCU_PHYTX_DEBUG32_E:
	{
		SHOW_DEFINED(WIFITXPCU_PHYTX_DEBUG32_E);
		break;
	}
	case WIFITXPCU_PREAMBLE_DONE_E:
	{
		SHOW_DEFINED(WIFITXPCU_PREAMBLE_DONE_E);
		break;
	}
	case WIFIRX_PHY_SLEEP_E:
	{
		SHOW_DEFINED(WIFIRX_PHY_SLEEP_E);
		break;
	}
	case WIFIRX_FRAME_BITMAP_REQ_E:
	{
		SHOW_DEFINED(WIFIRX_FRAME_BITMAP_REQ_E);
		break;
	}
	case WIFIRXPCU_TX_SETUP_CLEAR_E:
	{
		SHOW_DEFINED(WIFIRXPCU_TX_SETUP_CLEAR_E);
		break;
	}
	case WIFIRX_TRIG_INFO_E:
	{
		SHOW_DEFINED(WIFIRX_TRIG_INFO_E);
		break;
	}
	case WIFIEXPECTED_RESPONSE_E:
	{
		SHOW_DEFINED(WIFIEXPECTED_RESPONSE_E);
		break;
	}
	case WIFITRIGGER_RESPONSE_TX_DONE_E:
	{
		SHOW_DEFINED(WIFITRIGGER_RESPONSE_TX_DONE_E);
		break;
	}
	case WIFIFW2SW_MON_E:
	{
		/* parse fw2sw tlv */
		hal_txmon_parse_fw2sw(tx_tlv, tlv_user_id, data_status_info);
		status = HAL_MON_TX_FW2SW;
		SHOW_DEFINED(WIFIFW2SW_MON_E);
		break;
	}
	}

	return status;
}
#endif /* WLAN_PKT_CAPTURE_TX_2_0 */

#ifdef REO_SHARED_QREF_TABLE_EN
static void hal_reo_shared_qaddr_cache_clear_be(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_val = 0;

	/* Set Qdesc clear bit to erase REO internal storage for Qdesc pointers
	 * of 37 peer/tids
	 */
	reg_val = HAL_REG_READ(hal, HWIO_REO_R0_QDESC_ADDR_READ_ADDR(REO_REG_REG_BASE));
	reg_val |= HAL_SM(HWIO_REO_R0_QDESC_ADDR_READ, CLEAR_QDESC_ARRAY, 1);
	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_ADDR_READ_ADDR(REO_REG_REG_BASE),
		      reg_val);

	/* Clear Qdesc clear bit to erase REO internal storage for Qdesc pointers
	 * of 37 peer/tids
	 */
	reg_val &= ~(HAL_SM(HWIO_REO_R0_QDESC_ADDR_READ, CLEAR_QDESC_ARRAY, 1));
	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_ADDR_READ_ADDR(REO_REG_REG_BASE),
		      reg_val);

	hal_verbose_debug("hal_soc: %pK :Setting CLEAR_DESC_ARRAY field of"
			  "WCSS_UMAC_REO_R0_QDESC_ADDR_READ and resetting back"
			  "to erase stale entries in reo storage: regval:%x", hal, reg_val);
}

/* hal_reo_shared_qaddr_write(): Write REO tid queue addr
 * LUT shared by SW and HW at the index given by peer id
 * and tid.
 *
 * @hal_soc: hal soc pointer
 * @reo_qref_addr: pointer to index pointed to be peer_id
 * and tid
 * @tid: tid queue number
 * @hw_qdesc_paddr: reo queue addr
 */

static void hal_reo_shared_qaddr_write_be(hal_soc_handle_t hal_soc_hdl,
					  uint16_t peer_id,
					  int tid,
					  qdf_dma_addr_t hw_qdesc_paddr)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;
	struct rx_reo_queue_reference *reo_qref;
	uint32_t peer_tid_idx;

	/* Plug hw_desc_addr in Host reo queue reference table */
	if (HAL_PEER_ID_IS_MLO(peer_id)) {
		peer_tid_idx = ((peer_id - HAL_ML_PEER_ID_START) *
				DP_MAX_TIDS) + tid;
		reo_qref = (struct rx_reo_queue_reference *)
			&hal->reo_qref.mlo_reo_qref_table_vaddr[peer_tid_idx];
	} else {
		peer_tid_idx = (peer_id * DP_MAX_TIDS) + tid;
		reo_qref = (struct rx_reo_queue_reference *)
			&hal->reo_qref.non_mlo_reo_qref_table_vaddr[peer_tid_idx];
	}
	reo_qref->rx_reo_queue_desc_addr_31_0 =
		hw_qdesc_paddr & 0xffffffff;
	reo_qref->rx_reo_queue_desc_addr_39_32 =
		(hw_qdesc_paddr & 0xff00000000) >> 32;
	if (hw_qdesc_paddr != 0)
		reo_qref->receive_queue_number = tid;
	else
		reo_qref->receive_queue_number = 0;

	hal_reo_shared_qaddr_cache_clear_be(hal_soc_hdl);
	hal_verbose_debug("hw_qdesc_paddr: %pK, tid: %d, reo_qref:%pK,"
			  "rx_reo_queue_desc_addr_31_0: %x,"
			  "rx_reo_queue_desc_addr_39_32: %x",
			  (void *)hw_qdesc_paddr, tid, reo_qref,
			  reo_qref->rx_reo_queue_desc_addr_31_0,
			  reo_qref->rx_reo_queue_desc_addr_39_32);
}

#ifdef BIG_ENDIAN_HOST
static inline void hal_reo_shared_qaddr_enable(struct hal_soc *hal)
{
	HAL_REG_WRITE(hal, HWIO_REO_R0_QDESC_ADDR_READ_ADDR(REO_REG_REG_BASE),
		      HAL_SM(HWIO_REO_R0_QDESC_ADDR_READ, GXI_SWAP, 1) |
		      HAL_SM(HWIO_REO_R0_QDESC_ADDR_READ, LUT_FEATURE_ENABLE, 1));
}
#else
static inline void hal_reo_shared_qaddr_enable(struct hal_soc *hal)
{
	HAL_REG_WRITE(hal, HWIO_REO_R0_QDESC_ADDR_READ_ADDR(REO_REG_REG_BASE),
		      HAL_SM(HWIO_REO_R0_QDESC_ADDR_READ, LUT_FEATURE_ENABLE, 1));
}
#endif

/**
 * hal_reo_shared_qaddr_setup_be() - Allocate MLO and Non MLO reo queue
 * reference table shared between SW and HW and initialize in Qdesc Base0
 * base1 registers provided by HW.
 *
 * @hal_soc_hdl: HAL Soc handle
 * @reo_qref: REO queue reference table
 *
 * Return: QDF_STATUS_SUCCESS on success else a QDF error.
 */
static QDF_STATUS
hal_reo_shared_qaddr_setup_be(hal_soc_handle_t hal_soc_hdl,
			      struct reo_queue_ref_table *reo_qref)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;

	reo_qref->reo_qref_table_en = 1;

	reo_qref->mlo_reo_qref_table_vaddr =
		(uint64_t *)qdf_mem_alloc_consistent(
				hal->qdf_dev, hal->qdf_dev->dev,
				REO_QUEUE_REF_ML_TABLE_SIZE,
				&reo_qref->mlo_reo_qref_table_paddr);
	if (!reo_qref->mlo_reo_qref_table_vaddr)
		return QDF_STATUS_E_NOMEM;

	reo_qref->non_mlo_reo_qref_table_vaddr =
		(uint64_t *)qdf_mem_alloc_consistent(
				hal->qdf_dev, hal->qdf_dev->dev,
				REO_QUEUE_REF_NON_ML_TABLE_SIZE,
				&reo_qref->non_mlo_reo_qref_table_paddr);
	if (!reo_qref->non_mlo_reo_qref_table_vaddr) {
		qdf_mem_free_consistent(
				hal->qdf_dev, hal->qdf_dev->dev,
				REO_QUEUE_REF_ML_TABLE_SIZE,
				reo_qref->mlo_reo_qref_table_vaddr,
				reo_qref->mlo_reo_qref_table_paddr,
				0);
		reo_qref->mlo_reo_qref_table_vaddr = NULL;
		return QDF_STATUS_E_NOMEM;
	}

	hal_verbose_debug("MLO table start paddr:%pK,"
			  "Non-MLO table start paddr:%pK,"
			  "MLO table start vaddr: %pK,"
			  "Non MLO table start vaddr: %pK",
			  (void *)reo_qref->mlo_reo_qref_table_paddr,
			  (void *)reo_qref->non_mlo_reo_qref_table_paddr,
			  reo_qref->mlo_reo_qref_table_vaddr,
			  reo_qref->non_mlo_reo_qref_table_vaddr);

	return QDF_STATUS_SUCCESS;
}

/**
 * hal_reo_shared_qaddr_init_be() - Zero out REO qref LUT and
 * write start addr of MLO and Non MLO table in HW
 *
 * @hal_soc_hdl: HAL Soc handle
 * @qref_reset: reset qref LUT
 *
 * Return: None
 */
static void hal_reo_shared_qaddr_init_be(hal_soc_handle_t hal_soc_hdl,
					 int qref_reset)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;

	if (qref_reset) {
		qdf_mem_zero(hal->reo_qref.mlo_reo_qref_table_vaddr,
			     REO_QUEUE_REF_ML_TABLE_SIZE);
		qdf_mem_zero(hal->reo_qref.non_mlo_reo_qref_table_vaddr,
			     REO_QUEUE_REF_NON_ML_TABLE_SIZE);
	}
	/* LUT_BASE0 and BASE1 registers expect upper 32bits of LUT base address
	 * and lower 8 bits to be 0. Shift the physical address by 8 to plug
	 * upper 32bits only
	 */
	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_LUT_BASE0_ADDR_ADDR(REO_REG_REG_BASE),
		      hal->reo_qref.non_mlo_reo_qref_table_paddr >> 8);
	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_LUT_BASE1_ADDR_ADDR(REO_REG_REG_BASE),
		      hal->reo_qref.mlo_reo_qref_table_paddr >> 8);
	hal_reo_shared_qaddr_enable(hal);
	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_MAX_SW_PEER_ID_ADDR(REO_REG_REG_BASE),
		      HAL_MS(HWIO_REO_R0_QDESC, MAX_SW_PEER_ID_MAX_SUPPORTED,
			     0x1fff));
}

/**
 * hal_reo_shared_qaddr_detach_be() - Free MLO and Non MLO reo queue
 * reference table shared between SW and HW
 *
 * @hal_soc_hdl: HAL Soc handle
 *
 * Return: None
 */
static void hal_reo_shared_qaddr_detach_be(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;

	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_LUT_BASE0_ADDR_ADDR(REO_REG_REG_BASE),
		      0);
	HAL_REG_WRITE(hal,
		      HWIO_REO_R0_QDESC_LUT_BASE1_ADDR_ADDR(REO_REG_REG_BASE),
		      0);
}
#endif

/**
 * hal_tx_vdev_mismatch_routing_set_generic_be() - set vdev mismatch exception routing
 * @hal_soc_hdl: HAL SoC context
 * @config: HAL_TX_VDEV_MISMATCH_TQM_NOTIFY - route via TQM
 *          HAL_TX_VDEV_MISMATCH_FW_NOTIFY - route via FW
 *
 * Return: void
 */
#ifdef HWIO_TCL_R0_CMN_CONFIG_VDEVID_MISMATCH_EXCEPTION_BMSK
static inline void
hal_tx_vdev_mismatch_routing_set_generic_be(hal_soc_handle_t hal_soc_hdl,
					    enum hal_tx_vdev_mismatch_notify
					    config)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;
	uint32_t val = 0;

	reg_addr = HWIO_TCL_R0_CMN_CONFIG_ADDR(MAC_TCL_REG_REG_BASE);

	val = HAL_REG_READ(hal_soc, reg_addr);

	/* reset the corresponding bits in register */
	val &= (~(HWIO_TCL_R0_CMN_CONFIG_VDEVID_MISMATCH_EXCEPTION_BMSK));

	/* set config value */
	reg_val = val | (config <<
			HWIO_TCL_R0_CMN_CONFIG_VDEVID_MISMATCH_EXCEPTION_SHFT);

	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#else
static inline void
hal_tx_vdev_mismatch_routing_set_generic_be(hal_soc_handle_t hal_soc_hdl,
					    enum hal_tx_vdev_mismatch_notify
					    config)
{
}
#endif

/**
 * hal_tx_mcast_mlo_reinject_routing_set_generic_be() - set MLO multicast reinject routing
 * @hal_soc_hdl: HAL SoC context
 * @config: HAL_TX_MCAST_MLO_REINJECT_FW_NOTIFY - route via FW
 *          HAL_TX_MCAST_MLO_REINJECT_TQM_NOTIFY - route via TQM
 *
 * Return: void
 */
#if defined(HWIO_TCL_R0_CMN_CONFIG_MCAST_CMN_PN_SN_MLO_REINJECT_ENABLE_BMSK) && \
	defined(WLAN_MCAST_MLO)
static inline void
hal_tx_mcast_mlo_reinject_routing_set_generic_be(
				hal_soc_handle_t hal_soc_hdl,
				enum hal_tx_mcast_mlo_reinject_notify config)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;
	uint32_t val = 0;

	reg_addr = HWIO_TCL_R0_CMN_CONFIG_ADDR(MAC_TCL_REG_REG_BASE);
	val = HAL_REG_READ(hal_soc, reg_addr);

	/* reset the corresponding bits in register */
	val &= (~(HWIO_TCL_R0_CMN_CONFIG_MCAST_CMN_PN_SN_MLO_REINJECT_ENABLE_BMSK));

	/* set config value */
	reg_val = val | (config << HWIO_TCL_R0_CMN_CONFIG_MCAST_CMN_PN_SN_MLO_REINJECT_ENABLE_SHFT);

	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#else
static inline void
hal_tx_mcast_mlo_reinject_routing_set_generic_be(
				hal_soc_handle_t hal_soc_hdl,
				enum hal_tx_mcast_mlo_reinject_notify config)
{
}
#endif

/**
 * hal_get_ba_aging_timeout_be_generic() - Get BA Aging timeout
 *
 * @hal_soc_hdl: Opaque HAL SOC handle
 * @ac: Access category
 * @value: window size to get
 */

static inline
void hal_get_ba_aging_timeout_be_generic(hal_soc_handle_t hal_soc_hdl,
					 uint8_t ac, uint32_t *value)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;

	switch (ac) {
	case WME_AC_BE:
		*value = HAL_REG_READ(soc,
				      HWIO_REO_R0_AGING_THRESHOLD_IX_0_ADDR(
				      REO_REG_REG_BASE)) / 1000;
		break;
	case WME_AC_BK:
		*value = HAL_REG_READ(soc,
				      HWIO_REO_R0_AGING_THRESHOLD_IX_1_ADDR(
				      REO_REG_REG_BASE)) / 1000;
		break;
	case WME_AC_VI:
		*value = HAL_REG_READ(soc,
				      HWIO_REO_R0_AGING_THRESHOLD_IX_2_ADDR(
				      REO_REG_REG_BASE)) / 1000;
		break;
	case WME_AC_VO:
		*value = HAL_REG_READ(soc,
				      HWIO_REO_R0_AGING_THRESHOLD_IX_3_ADDR(
				      REO_REG_REG_BASE)) / 1000;
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "Invalid AC: %d\n", ac);
	}
}

/**
 * hal_setup_link_idle_list_generic_be - Setup scattered idle list using the
 * buffer list provided
 *
 * @soc: Opaque HAL SOC handle
 * @scatter_bufs_base_paddr: Array of physical base addresses
 * @scatter_bufs_base_vaddr: Array of virtual base addresses
 * @num_scatter_bufs: Number of scatter buffers in the above lists
 * @scatter_buf_size: Size of each scatter buffer
 * @last_buf_end_offset: Offset to the last entry
 * @num_entries: Total entries of all scatter bufs
 *
 * Return: None
 */
static inline void
hal_setup_link_idle_list_generic_be(struct hal_soc *soc,
				    qdf_dma_addr_t scatter_bufs_base_paddr[],
				    void *scatter_bufs_base_vaddr[],
				    uint32_t num_scatter_bufs,
				    uint32_t scatter_buf_size,
				    uint32_t last_buf_end_offset,
				    uint32_t num_entries)
{
	int i;
	uint32_t *prev_buf_link_ptr = NULL;
	uint32_t reg_scatter_buf_size, reg_tot_scatter_buf_size;
	uint32_t val;

	/* Link the scatter buffers */
	for (i = 0; i < num_scatter_bufs; i++) {
		if (i > 0) {
			prev_buf_link_ptr[0] =
				scatter_bufs_base_paddr[i] & 0xffffffff;
			prev_buf_link_ptr[1] = HAL_SM(
				HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB,
				BASE_ADDRESS_39_32,
				((uint64_t)(scatter_bufs_base_paddr[i])
				 >> 32)) | HAL_SM(
				HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB,
				ADDRESS_MATCH_TAG,
				ADDRESS_MATCH_TAG_VAL);
		}
		prev_buf_link_ptr = (uint32_t *)(scatter_bufs_base_vaddr[i] +
			scatter_buf_size - WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE);
	}

	/* TBD: Register programming partly based on MLD & the rest based on
	 * inputs from HW team. Not complete yet.
	 */

	reg_scatter_buf_size = (scatter_buf_size -
				WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE) / 64;
	reg_tot_scatter_buf_size = ((scatter_buf_size -
		WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE) * num_scatter_bufs) / 64;

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_IDLE_LIST_CONTROL_ADDR(
		WBM_REG_REG_BASE),
		HAL_SM(HWIO_WBM_R0_IDLE_LIST_CONTROL, SCATTER_BUFFER_SIZE,
		reg_scatter_buf_size) |
		HAL_SM(HWIO_WBM_R0_IDLE_LIST_CONTROL, LINK_DESC_IDLE_LIST_MODE,
		0x1));

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_IDLE_LIST_SIZE_ADDR(
		WBM_REG_REG_BASE),
		HAL_SM(HWIO_WBM_R0_IDLE_LIST_SIZE,
		SCATTER_RING_SIZE_OF_IDLE_LINK_DESC_LIST,
		reg_tot_scatter_buf_size));

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_LSB_ADDR(
		WBM_REG_REG_BASE),
		scatter_bufs_base_paddr[0] & 0xffffffff);

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB_ADDR(
		WBM_REG_REG_BASE),
		((uint64_t)(scatter_bufs_base_paddr[0]) >> 32) &
		HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB_BASE_ADDRESS_39_32_BMSK);

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB_ADDR(
		WBM_REG_REG_BASE),
		HAL_SM(HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB,
		BASE_ADDRESS_39_32, ((uint64_t)(scatter_bufs_base_paddr[0])
								>> 32)) |
		HAL_SM(HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB,
		ADDRESS_MATCH_TAG, ADDRESS_MATCH_TAG_VAL));

	/* ADDRESS_MATCH_TAG field in the above register is expected to match
	 * with the upper bits of link pointer. The above write sets this field
	 * to zero and we are also setting the upper bits of link pointers to
	 * zero while setting up the link list of scatter buffers above
	 */

	/* Setup head and tail pointers for the idle list */
	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX0_ADDR(
		WBM_REG_REG_BASE),
		scatter_bufs_base_paddr[num_scatter_bufs - 1] & 0xffffffff);
	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX1_ADDR(
		WBM_REG_REG_BASE),
		HAL_SM(HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX1,
		BUFFER_ADDRESS_39_32,
		((uint64_t)(scatter_bufs_base_paddr[num_scatter_bufs - 1])
								>> 32)) |
		HAL_SM(HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX1,
		HEAD_POINTER_OFFSET, last_buf_end_offset >> 2));

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX0_ADDR(
		WBM_REG_REG_BASE),
		scatter_bufs_base_paddr[0] & 0xffffffff);

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_TAIL_INFO_IX0_ADDR(
		WBM_REG_REG_BASE),
		scatter_bufs_base_paddr[0] & 0xffffffff);
	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_TAIL_INFO_IX1_ADDR(
		WBM_REG_REG_BASE),
		HAL_SM(HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_TAIL_INFO_IX1,
		BUFFER_ADDRESS_39_32,
		((uint64_t)(scatter_bufs_base_paddr[0]) >>
		32)) | HAL_SM(HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_TAIL_INFO_IX1,
		TAIL_POINTER_OFFSET, 0));

	HAL_REG_WRITE(soc,
		HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HP_ADDR(
		WBM_REG_REG_BASE),
		2 * num_entries);

	/* Set RING_ID_DISABLE */
	val = HAL_SM(HWIO_WBM_R0_WBM_IDLE_LINK_RING_MISC, RING_ID_DISABLE, 1);

	/*
	 * SRNG_ENABLE bit is not available in HWK v1 (QCA8074v1). Hence
	 * check the presence of the bit before toggling it.
	 */
#ifdef HWIO_WBM_R0_WBM_IDLE_LINK_RING_MISC_SRNG_ENABLE_BMSK
	val |= HAL_SM(HWIO_WBM_R0_WBM_IDLE_LINK_RING_MISC, SRNG_ENABLE, 1);
#endif
	HAL_REG_WRITE(soc,
		      HWIO_WBM_R0_WBM_IDLE_LINK_RING_MISC_ADDR(WBM_REG_REG_BASE),
		      val);
}

#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
#define HAL_WBM_MISC_CONTROL_SPARE_CONTROL_FIELD_BIT15 0x8000
#endif

/**
 * hal_cookie_conversion_reg_cfg_generic_be() - set cookie conversion relevant register
 *					for REO/WBM
 * @hal_soc_hdl: HAL soc handle
 * @cc_cfg: structure pointer for HW cookie conversion configuration
 *
 * Return: None
 */
static inline
void hal_cookie_conversion_reg_cfg_generic_be(hal_soc_handle_t hal_soc_hdl,
					      struct hal_hw_cc_config *cc_cfg)
{
	uint32_t reg_addr, reg_val = 0;
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;

	/* REO CFG */
	reg_addr = HWIO_REO_R0_SW_COOKIE_CFG0_ADDR(REO_REG_REG_BASE);
	reg_val = cc_cfg->lut_base_addr_31_0;
	HAL_REG_WRITE(soc, reg_addr, reg_val);

	reg_addr = HWIO_REO_R0_SW_COOKIE_CFG1_ADDR(REO_REG_REG_BASE);
	reg_val = 0;
	reg_val |= HAL_SM(HWIO_REO_R0_SW_COOKIE_CFG1,
			  SW_COOKIE_CONVERT_GLOBAL_ENABLE,
			  cc_cfg->cc_global_en);
	reg_val |= HAL_SM(HWIO_REO_R0_SW_COOKIE_CFG1,
			  SW_COOKIE_CONVERT_ENABLE,
			  cc_cfg->cc_global_en);
	reg_val |= HAL_SM(HWIO_REO_R0_SW_COOKIE_CFG1,
			  PAGE_ALIGNMENT,
			  cc_cfg->page_4k_align);
	reg_val |= HAL_SM(HWIO_REO_R0_SW_COOKIE_CFG1,
			  COOKIE_OFFSET_MSB,
			  cc_cfg->cookie_offset_msb);
	reg_val |= HAL_SM(HWIO_REO_R0_SW_COOKIE_CFG1,
			  COOKIE_PAGE_MSB,
			  cc_cfg->cookie_page_msb);
	reg_val |= HAL_SM(HWIO_REO_R0_SW_COOKIE_CFG1,
			  CMEM_LUT_BASE_ADDR_39_32,
			  cc_cfg->lut_base_addr_39_32);
	HAL_REG_WRITE(soc, reg_addr, reg_val);

	/* WBM CFG */
	reg_addr = HWIO_WBM_R0_SW_COOKIE_CFG0_ADDR(WBM_REG_REG_BASE);
	reg_val = cc_cfg->lut_base_addr_31_0;
	HAL_REG_WRITE(soc, reg_addr, reg_val);

	reg_addr = HWIO_WBM_R0_SW_COOKIE_CFG1_ADDR(WBM_REG_REG_BASE);
	reg_val = 0;
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CFG1,
			  PAGE_ALIGNMENT,
			  cc_cfg->page_4k_align);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CFG1,
			  COOKIE_OFFSET_MSB,
			  cc_cfg->cookie_offset_msb);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CFG1,
			  COOKIE_PAGE_MSB,
			  cc_cfg->cookie_page_msb);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CFG1,
			  CMEM_LUT_BASE_ADDR_39_32,
			  cc_cfg->lut_base_addr_39_32);
	HAL_REG_WRITE(soc, reg_addr, reg_val);

	/*
	 * WCSS_UMAC_WBM_R0_SW_COOKIE_CONVERT_CFG default value is 0x1FE,
	 */
	reg_addr = HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG_ADDR(WBM_REG_REG_BASE);
	reg_val = 0;
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM_COOKIE_CONV_GLOBAL_ENABLE,
			  cc_cfg->cc_global_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW6_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw6_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW5_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw5_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW4_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw4_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW3_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw3_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW2_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw2_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW1_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw1_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2SW0_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2sw0_cc_en);
	reg_val |= HAL_SM(HWIO_WBM_R0_SW_COOKIE_CONVERT_CFG,
			  WBM2FW_COOKIE_CONVERSION_EN,
			  cc_cfg->wbm2fw_cc_en);
	HAL_REG_WRITE(soc, reg_addr, reg_val);

#ifdef HWIO_WBM_R0_WBM_CFG_2_COOKIE_DEBUG_SEL_BMSK
	reg_addr = HWIO_WBM_R0_WBM_CFG_2_ADDR(WBM_REG_REG_BASE);
	reg_val = 0;
	reg_val |= HAL_SM(HWIO_WBM_R0_WBM_CFG_2,
			  COOKIE_DEBUG_SEL,
			  cc_cfg->cc_global_en);

	reg_val |= HAL_SM(HWIO_WBM_R0_WBM_CFG_2,
			  COOKIE_CONV_INDICATION_EN,
			  cc_cfg->cc_global_en);

	reg_val |= HAL_SM(HWIO_WBM_R0_WBM_CFG_2,
			  ERROR_PATH_COOKIE_CONV_EN,
			  cc_cfg->error_path_cookie_conv_en);

	reg_val |= HAL_SM(HWIO_WBM_R0_WBM_CFG_2,
			  RELEASE_PATH_COOKIE_CONV_EN,
			  cc_cfg->release_path_cookie_conv_en);

	HAL_REG_WRITE(soc, reg_addr, reg_val);
#endif
#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
	/*
	 * To enable indication for HW cookie conversion done or not for
	 * WBM, WCSS_UMAC_WBM_R0_MISC_CONTROL spare_control field 15th
	 * bit spare_control[15] should be set.
	 */
	reg_addr = HWIO_WBM_R0_MISC_CONTROL_ADDR(WBM_REG_REG_BASE);
	reg_val = HAL_REG_READ(soc, reg_addr);
	reg_val |= HAL_SM(HWIO_WCSS_UMAC_WBM_R0_MISC_CONTROL,
			  SPARE_CONTROL,
			  HAL_WBM_MISC_CONTROL_SPARE_CONTROL_FIELD_BIT15);
	HAL_REG_WRITE(soc, reg_addr, reg_val);
#endif
}

/**
 * hal_set_ba_aging_timeout_be_generic() - Set BA Aging timeout
 * @hal_soc_hdl: Opaque HAL SOC handle
 * @ac: Access category
 * ac: 0 - Background, 1 - Best Effort, 2 - Video, 3 - Voice
 * @value: Input value to set
 */
static inline
void hal_set_ba_aging_timeout_be_generic(hal_soc_handle_t hal_soc_hdl,
					 uint8_t ac, uint32_t value)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;

	switch (ac) {
	case WME_AC_BE:
		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_AGING_THRESHOLD_IX_0_ADDR(
			      REO_REG_REG_BASE),
			      value * 1000);
		break;
	case WME_AC_BK:
		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_AGING_THRESHOLD_IX_1_ADDR(
			      REO_REG_REG_BASE),
			      value * 1000);
		break;
	case WME_AC_VI:
		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_AGING_THRESHOLD_IX_2_ADDR(
			      REO_REG_REG_BASE),
			      value * 1000);
		break;
	case WME_AC_VO:
		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_AGING_THRESHOLD_IX_3_ADDR(
			      REO_REG_REG_BASE),
			      value * 1000);
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "Invalid AC: %d\n", ac);
	}
}

/**
 * hal_tx_populate_bank_register_be() - populate the bank register with
 *		the software configs.
 * @hal_soc_hdl: HAL soc handle
 * @config: bank config
 * @bank_id: bank id to be configured
 *
 * Returns: None
 */
#ifdef HWIO_TCL_R0_SW_CONFIG_BANK_n_MCAST_PACKET_CTRL_SHFT
static inline void
hal_tx_populate_bank_register_be(hal_soc_handle_t hal_soc_hdl,
				 union hal_tx_bank_config *config,
				 uint8_t bank_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;

	reg_addr = HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDR(MAC_TCL_REG_REG_BASE,
						     bank_id);

	reg_val |= (config->epd << HWIO_TCL_R0_SW_CONFIG_BANK_n_EPD_SHFT);
	reg_val |= (config->encap_type <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ENCAP_TYPE_SHFT);
	reg_val |= (config->encrypt_type <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ENCRYPT_TYPE_SHFT);
	reg_val |= (config->src_buffer_swap <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_SRC_BUFFER_SWAP_SHFT);
	reg_val |= (config->link_meta_swap <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_LINK_META_SWAP_SHFT);
	reg_val |= (config->index_lookup_enable <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_INDEX_LOOKUP_ENABLE_SHFT);
	reg_val |= (config->addrx_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDRX_EN_SHFT);
	reg_val |= (config->addry_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDRY_EN_SHFT);
	reg_val |= (config->mesh_enable <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_MESH_ENABLE_SHFT);
	reg_val |= (config->vdev_id_check_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_VDEV_ID_CHECK_EN_SHFT);
	reg_val |= (config->pmac_id <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_PMAC_ID_SHFT);
	reg_val |= (config->mcast_pkt_ctrl <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_MCAST_PACKET_CTRL_SHFT);

	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#else
static inline void
hal_tx_populate_bank_register_be(hal_soc_handle_t hal_soc_hdl,
				 union hal_tx_bank_config *config,
				 uint8_t bank_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;

	reg_addr = HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDR(MAC_TCL_REG_REG_BASE,
						     bank_id);

	reg_val |= (config->epd << HWIO_TCL_R0_SW_CONFIG_BANK_n_EPD_SHFT);
	reg_val |= (config->encap_type <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ENCAP_TYPE_SHFT);
	reg_val |= (config->encrypt_type <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ENCRYPT_TYPE_SHFT);
	reg_val |= (config->src_buffer_swap <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_SRC_BUFFER_SWAP_SHFT);
	reg_val |= (config->link_meta_swap <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_LINK_META_SWAP_SHFT);
	reg_val |= (config->index_lookup_enable <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_INDEX_LOOKUP_ENABLE_SHFT);
	reg_val |= (config->addrx_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDRX_EN_SHFT);
	reg_val |= (config->addry_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDRY_EN_SHFT);
	reg_val |= (config->mesh_enable <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_MESH_ENABLE_SHFT);
	reg_val |= (config->vdev_id_check_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_VDEV_ID_CHECK_EN_SHFT);
	reg_val |= (config->pmac_id <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_PMAC_ID_SHFT);
	reg_val |= (config->dscp_tid_map_id <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_DSCP_TID_TABLE_NUM_SHFT);

	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#endif


#ifdef HWIO_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_VAL_SHFT

#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_REG_ID(vdev_id) (vdev_id >> 0x4)
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_INDEX_IN_REG(vdev_id) (vdev_id & 0xF)
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK 0x3
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT 0x2

/**
 * hal_tx_vdev_mcast_ctrl_set_be() - set mcast_ctrl value
 * @hal_soc_hdl: HAL SoC context
 * @vdev_id: vdev identifier
 * @mcast_ctrl_val: mcast ctrl value for this VAP
 *
 * Return: void
 */
static inline void
hal_tx_vdev_mcast_ctrl_set_be(hal_soc_handle_t hal_soc_hdl,
			      uint8_t vdev_id, uint8_t mcast_ctrl_val)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;
	uint32_t val;
	uint8_t reg_idx = HAL_TCL_VDEV_MCAST_PACKET_CTRL_REG_ID(vdev_id);
	uint8_t index_in_reg =
		HAL_TCL_VDEV_MCAST_PACKET_CTRL_INDEX_IN_REG(vdev_id);

	reg_addr =
	HWIO_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_ADDR(MAC_TCL_REG_REG_BASE,
						      reg_idx);

	val = HAL_REG_READ(hal_soc, reg_addr);

	/* mask out other stored value */
	val &= (~(HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK <<
		  (HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT * index_in_reg)));

	reg_val = val |
		((HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK & mcast_ctrl_val) <<
		 (HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT * index_in_reg));

	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#else
static inline void
hal_tx_vdev_mcast_ctrl_set_be(hal_soc_handle_t hal_soc_hdl,
			      uint8_t vdev_id, uint8_t mcast_ctrl_val)
{
}
#endif

#endif /* _HAL_BE_GENERIC_API_H_ */

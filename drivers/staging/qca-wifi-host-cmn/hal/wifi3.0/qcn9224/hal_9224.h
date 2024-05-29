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
#include "qdf_types.h"
#include "qdf_util.h"
#include "qdf_mem.h"
#include "qdf_nbuf.h"
#include "qdf_module.h"

#include "target_type.h"
#include "wcss_version.h"

#include "hal_be_hw_headers.h"
#include "hal_internal.h"
#include "hal_api.h"
#include "hal_flow.h"
#include "rx_flow_search_entry.h"
#include "hal_rx_flow_info.h"
#include "hal_be_api.h"
#include "tcl_entrance_from_ppe_ring.h"
#include "sw_monitor_ring.h"
#include "wcss_seq_hwioreg_umac.h"
#include "wfss_ce_reg_seq_hwioreg.h"
#include <uniform_reo_status_header.h>
#include <wbm_release_ring_tx.h>
#include <phyrx_location.h>
#ifdef QCA_MONITOR_2_0_SUPPORT
#include <mon_ingress_ring.h>
#include <mon_destination_ring.h>
#endif
#include "rx_reo_queue_1k.h"

#include <hal_be_rx.h>

#define UNIFIED_RX_MPDU_START_0_RX_MPDU_INFO_RX_MPDU_INFO_DETAILS_OFFSET \
	RX_MPDU_START_RX_MPDU_INFO_DETAILS_RXPCU_MPDU_FILTER_IN_CATEGORY_OFFSET
#define UNIFIED_RX_MSDU_LINK_8_RX_MSDU_DETAILS_MSDU_0_OFFSET \
	RX_MSDU_LINK_MSDU_BUFFER_ADDR_INFO_DETAILS_BUFFER_ADDR_31_0_OFFSET
#define UNIFIED_RX_MSDU_DETAILS_2_RX_MSDU_DESC_INFO_RX_MSDU_DESC_INFO_DETAILS_OFFSET \
	RX_MSDU_DETAILS_RX_MSDU_DESC_INFO_DETAILS_FIRST_MSDU_IN_MPDU_FLAG_OFFSET
#define UNIFIED_RX_MPDU_DETAILS_2_RX_MPDU_DESC_INFO_RX_MPDU_DESC_INFO_DETAILS_OFFSET \
	RX_MPDU_DETAILS_RX_MPDU_DESC_INFO_DETAILS_MSDU_COUNT_OFFSET
#define UNIFIED_REO_DESTINATION_RING_2_RX_MPDU_DESC_INFO_RX_MPDU_DESC_INFO_DETAILS_OFFSET \
	REO_DESTINATION_RING_RX_MPDU_DESC_INFO_DETAILS_MSDU_COUNT_OFFSET
#define UNIFORM_REO_STATUS_HEADER_STATUS_HEADER \
	STATUS_HEADER_REO_STATUS_NUMBER
#define UNIFORM_REO_STATUS_HEADER_STATUS_HEADER_GENERIC \
	STATUS_HEADER_TIMESTAMP
#define UNIFIED_RX_MSDU_DETAILS_2_RX_MSDU_DESC_INFO_RX_MSDU_DESC_INFO_DETAILS_OFFSET \
	RX_MSDU_DETAILS_RX_MSDU_DESC_INFO_DETAILS_FIRST_MSDU_IN_MPDU_FLAG_OFFSET
#define UNIFIED_RX_MSDU_LINK_8_RX_MSDU_DETAILS_MSDU_0_OFFSET \
	RX_MSDU_LINK_MSDU_BUFFER_ADDR_INFO_DETAILS_BUFFER_ADDR_31_0_OFFSET
#define UNIFIED_TCL_DATA_CMD_0_BUFFER_ADDR_INFO_BUF_ADDR_INFO_OFFSET \
	TCL_DATA_CMD_BUF_ADDR_INFO_BUFFER_ADDR_31_0_OFFSET
#define UNIFIED_TCL_DATA_CMD_1_BUFFER_ADDR_INFO_BUF_ADDR_INFO_OFFSET \
	TCL_DATA_CMD_BUF_ADDR_INFO_BUFFER_ADDR_39_32_OFFSET
#define UNIFIED_TCL_DATA_CMD_2_BUF_OR_EXT_DESC_TYPE_OFFSET \
	TCL_DATA_CMD_BUF_OR_EXT_DESC_TYPE_OFFSET
#define UNIFIED_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_LSB \
	BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_LSB
#define UNIFIED_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_MASK \
	BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_MASK
#define UNIFIED_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_LSB \
	BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_LSB
#define UNIFIED_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_MASK \
	BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_MASK
#define UNIFIED_BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_LSB \
	BUFFER_ADDR_INFO_RETURN_BUFFER_MANAGER_LSB
#define UNIFIED_BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_MASK \
	BUFFER_ADDR_INFO_RETURN_BUFFER_MANAGER_MASK
#define UNIFIED_BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_LSB \
	BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_LSB
#define UNIFIED_BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_MASK \
	BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_MASK
#define UNIFIED_TCL_DATA_CMD_2_BUF_OR_EXT_DESC_TYPE_LSB \
	TCL_DATA_CMD_BUF_OR_EXT_DESC_TYPE_LSB
#define UNIFIED_TCL_DATA_CMD_2_BUF_OR_EXT_DESC_TYPE_MASK \
	TCL_DATA_CMD_BUF_OR_EXT_DESC_TYPE_MASK
#define UNIFIED_WBM_RELEASE_RING_6_TX_RATE_STATS_INFO_TX_RATE_STATS_MASK \
	WBM_RELEASE_RING_TX_TX_RATE_STATS_PPDU_TRANSMISSION_TSF_MASK
#define UNIFIED_WBM_RELEASE_RING_6_TX_RATE_STATS_INFO_TX_RATE_STATS_OFFSET \
	WBM_RELEASE_RING_TX_TX_RATE_STATS_PPDU_TRANSMISSION_TSF_OFFSET
#define UNIFIED_WBM_RELEASE_RING_6_TX_RATE_STATS_INFO_TX_RATE_STATS_LSB \
	WBM_RELEASE_RING_TX_TX_RATE_STATS_PPDU_TRANSMISSION_TSF_LSB

#ifdef QCA_MONITOR_2_0_SUPPORT
#include "hal_be_api_mon.h"
#endif

#ifdef CONFIG_WIFI_EMULATION_WIFI_3_0
#define CMEM_REG_BASE 0x0010e000

#define CMEM_WINDOW_ADDRESS_9224 \
		((CMEM_REG_BASE >> WINDOW_SHIFT) & WINDOW_VALUE_MASK)
#endif

#define CE_WINDOW_ADDRESS_9224 \
		((CE_WFSS_CE_REG_BASE >> WINDOW_SHIFT) & WINDOW_VALUE_MASK)

#define UMAC_WINDOW_ADDRESS_9224 \
		((UMAC_BASE >> WINDOW_SHIFT) & WINDOW_VALUE_MASK)

#ifdef CONFIG_WIFI_EMULATION_WIFI_3_0
#define WINDOW_CONFIGURATION_VALUE_9224 \
		((CE_WINDOW_ADDRESS_9224 << 6) |\
		 (UMAC_WINDOW_ADDRESS_9224 << 12) | \
		 CMEM_WINDOW_ADDRESS_9224 | \
		 WINDOW_ENABLE_BIT)
#else
#define WINDOW_CONFIGURATION_VALUE_9224 \
		((CE_WINDOW_ADDRESS_9224 << 6) |\
		 (UMAC_WINDOW_ADDRESS_9224 << 12) | \
		 WINDOW_ENABLE_BIT)
#endif

/* For Berryllium sw2rxdma ring size increased to 20 bits */
#define HAL_RXDMA_MAX_RING_SIZE_BE 0xFFFFF

#include "hal_9224_rx.h"
#include "hal_9224_tx.h"
#include "hal_be_rx_tlv.h"
#include <hal_be_generic_api.h>

#define PMM_REG_BASE_QCN9224 0xB500F8

/**
 * hal_read_pmm_scratch_reg(): API to read PMM Scratch register
 *
 * @soc: HAL soc
 * @base_addr: Base PMM register
 * @reg_enum: Enum of the scratch register
 *
 * Return: uint32_t
 */
static inline
uint32_t hal_read_pmm_scratch_reg(struct hal_soc *soc,
				  uint32_t base_addr,
				  enum hal_scratch_reg_enum reg_enum)
{
	uint32_t val = 0;

	pld_reg_read(soc->qdf_dev->dev, base_addr + (reg_enum * 4), &val, NULL);
	return val;
}

/**
 * hal_get_tsf2_scratch_reg_qcn9224(): API to read tsf2 scratch register
 *
 * @hal_soc_hdl: HAL soc context
 * @mac_id: mac id
 * @value: Pointer to update tsf2 value
 *
 * Return: void
 */
static void hal_get_tsf2_scratch_reg_qcn9224(hal_soc_handle_t hal_soc_hdl,
					     uint8_t mac_id, uint64_t *value)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t offset_lo, offset_hi;
	enum hal_scratch_reg_enum enum_lo, enum_hi;

	hal_get_tsf_enum(DEFAULT_TSF_ID, mac_id, &enum_lo, &enum_hi);

	offset_lo = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224,
					     enum_lo);

	offset_hi = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224,
					     enum_hi);

	*value = ((uint64_t)(offset_hi) << 32 | offset_lo);
}

/**
 * hal_get_tqm_scratch_reg_qcn9224(): API to read tqm scratch register
 *
 * @hal_soc_hdl: HAL soc context
 * @value: Pointer to update tqm value
 *
 * Return: void
 */
static void hal_get_tqm_scratch_reg_qcn9224(hal_soc_handle_t hal_soc_hdl,
					    uint64_t *value)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t offset_lo, offset_hi;

	offset_lo = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224,
					     PMM_TQM_CLOCK_OFFSET_LO_US);

	offset_hi = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224,
					     PMM_TQM_CLOCK_OFFSET_HI_US);

	*value = ((uint64_t)(offset_hi) << 32 | offset_lo);
}

#define LINK_DESC_SIZE (NUM_OF_DWORDS_RX_MSDU_LINK << 2)
#define HAL_PPE_VP_ENTRIES_MAX 32
#define HAL_PPE_VP_SEARCH_IDX_REG_MAX 8

/**
 * hal_get_link_desc_size_9224(): API to get the link desc size
 *
 * Return: uint32_t
 */
static uint32_t hal_get_link_desc_size_9224(void)
{
	return LINK_DESC_SIZE;
}

/**
 * hal_rx_get_tlv_9224(): API to get the tlv
 *
 * @rx_tlv: TLV data extracted from the rx packet
 * Return: uint8_t
 */
static uint8_t hal_rx_get_tlv_9224(void *rx_tlv)
{
	return HAL_RX_GET(rx_tlv, PHYRX_RSSI_LEGACY, RECEIVE_BANDWIDTH);
}

/**
 * hal_rx_wbm_err_msdu_continuation_get_9224 () - API to check if WBM
 * msdu continuation bit is set
 *
 *@wbm_desc: wbm release ring descriptor
 *
 * Return: true if msdu continuation bit is set.
 */
static inline
uint8_t hal_rx_wbm_err_msdu_continuation_get_9224(void *wbm_desc)
{
	uint32_t comp_desc = *(uint32_t *)(((uint8_t *)wbm_desc) +
	WBM_RELEASE_RING_RX_RX_MSDU_DESC_INFO_DETAILS_MSDU_CONTINUATION_OFFSET);

	return (comp_desc &
	WBM_RELEASE_RING_RX_RX_MSDU_DESC_INFO_DETAILS_MSDU_CONTINUATION_MASK) >>
	WBM_RELEASE_RING_RX_RX_MSDU_DESC_INFO_DETAILS_MSDU_CONTINUATION_LSB;
}

#if (defined(WLAN_SA_API_ENABLE)) && (defined(QCA_WIFI_QCA9574))
#define HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, evm, pilot) \
	(ppdu_info)->evm_info.pilot_evm[pilot] = HAL_RX_GET(rx_tlv, \
				PHYRX_OTHER_RECEIVE_INFO, \
				SU_EVM_DETAILS_##evm##_PILOT_##pilot##_EVM)

static inline void
hal_rx_update_su_evm_info(void *rx_tlv,
			  void *ppdu_info_hdl)
{
	struct hal_rx_ppdu_info *ppdu_info =
			(struct hal_rx_ppdu_info *)ppdu_info_hdl;

	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 1, 0);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 2, 1);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 3, 2);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 4, 3);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 5, 4);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 6, 5);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 7, 6);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 8, 7);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 9, 8);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 10, 9);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 11, 10);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 12, 11);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 13, 12);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 14, 13);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 15, 14);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 16, 15);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 17, 16);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 18, 17);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 19, 18);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 20, 19);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 21, 20);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 22, 21);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 23, 22);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 24, 23);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 25, 24);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 26, 25);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 27, 26);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 28, 27);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 29, 28);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 30, 29);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 31, 30);
	HAL_RX_UPDATE_SU_EVM_INFO(rx_tlv, ppdu_info, 32, 31);
}

static void hal_rx_get_evm_info(void *rx_tlv_hdr, void *ppdu_info_hdl)
{
	struct hal_rx_ppdu_info *ppdu_info  = ppdu_info_hdl;
	void *rx_tlv = (uint8_t *)rx_tlv_hdr + HAL_RX_TLV32_HDR_SIZE;
	uint32_t tlv_tag;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv_hdr);

	switch (tlv_tag) {
	case WIFIPHYRX_OTHER_RECEIVE_INFO_EVM_DETAILS_E:

		/* Skip TLV length to get TLV content */
		rx_tlv = (uint8_t *)rx_tlv + HAL_RX_TLV32_HDR_SIZE;

		ppdu_info->evm_info.number_of_symbols = HAL_RX_GET(rx_tlv,
				PHYRX_OTHER_RECEIVE_INFO,
				SU_EVM_DETAILS_0_NUMBER_OF_SYMBOLS);
		ppdu_info->evm_info.pilot_count = HAL_RX_GET(rx_tlv,
				PHYRX_OTHER_RECEIVE_INFO,
				SU_EVM_DETAILS_0_PILOT_COUNT);
		ppdu_info->evm_info.nss_count = HAL_RX_GET(rx_tlv,
				PHYRX_OTHER_RECEIVE_INFO,
				SU_EVM_DETAILS_0_NSS_COUNT);
		hal_rx_update_su_evm_info(rx_tlv, ppdu_info_hdl);
		break;
	}
}
#else /* WLAN_SA_API_ENABLE && QCA_WIFI_QCA9574 */
static void hal_rx_get_evm_info(void *tlv_tag, void *ppdu_info_hdl)
{
}
#endif /* WLAN_SA_API_ENABLE && QCA_WIFI_QCA9574 */

/**
 * hal_rx_proc_phyrx_other_receive_info_tlv_9224(): API to get tlv info
 *
 * Return: uint32_t
 */
static inline
void hal_rx_proc_phyrx_other_receive_info_tlv_9224(void *rx_tlv_hdr,
						   void *ppdu_info_hdl)
{
	uint32_t tlv_tag, tlv_len;
	uint32_t temp_len, other_tlv_len, other_tlv_tag;
	void *rx_tlv = (uint8_t *)rx_tlv_hdr + HAL_RX_TLV32_HDR_SIZE;
	void *other_tlv_hdr = NULL;
	void *other_tlv = NULL;

	/* Get evm info for Smart Antenna */
	hal_rx_get_evm_info(rx_tlv_hdr, ppdu_info_hdl);

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv_hdr);
	temp_len = 0;

	other_tlv_hdr = rx_tlv + HAL_RX_TLV32_HDR_SIZE;
	other_tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(other_tlv_hdr);
	other_tlv_len = HAL_RX_GET_USER_TLV32_LEN(other_tlv_hdr);

	temp_len += other_tlv_len;
	other_tlv = other_tlv_hdr + HAL_RX_TLV32_HDR_SIZE;

	switch (other_tlv_tag) {
	default:
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s unhandled TLV type: %d, TLV len:%d",
			  __func__, other_tlv_tag, other_tlv_len);
	break;
	}
}

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
static inline
void hal_rx_get_bb_info_9224(void *rx_tlv, void *ppdu_info_hdl)
{
	struct hal_rx_ppdu_info *ppdu_info  = ppdu_info_hdl;

	ppdu_info->cfr_info.bb_captured_channel =
		HAL_RX_GET_64(rx_tlv, RXPCU_PPDU_END_INFO, BB_CAPTURED_CHANNEL);

	ppdu_info->cfr_info.bb_captured_timeout =
		HAL_RX_GET_64(rx_tlv, RXPCU_PPDU_END_INFO, BB_CAPTURED_TIMEOUT);

	ppdu_info->cfr_info.bb_captured_reason =
		HAL_RX_GET_64(rx_tlv, RXPCU_PPDU_END_INFO, BB_CAPTURED_REASON);
}

static inline
void hal_rx_get_rtt_info_9224(void *rx_tlv, void *ppdu_info_hdl)
{
	struct hal_rx_ppdu_info *ppdu_info  = ppdu_info_hdl;

	ppdu_info->cfr_info.rx_location_info_valid =
	HAL_RX_GET_64(rx_tlv, PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RX_LOCATION_INFO_VALID);

	ppdu_info->cfr_info.rtt_che_buffer_pointer_low32 =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RTT_CHE_BUFFER_POINTER_LOW32);

	ppdu_info->cfr_info.rtt_che_buffer_pointer_high8 =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RTT_CHE_BUFFER_POINTER_HIGH8);

	ppdu_info->cfr_info.chan_capture_status =
	HAL_GET_RX_LOCATION_INFO_CHAN_CAPTURE_STATUS(rx_tlv);

	ppdu_info->cfr_info.rx_start_ts =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RX_START_TS);

	ppdu_info->cfr_info.rtt_cfo_measurement = (int16_t)
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RTT_CFO_MEASUREMENT);

	ppdu_info->cfr_info.agc_gain_info0 =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      GAIN_CHAIN0);

	ppdu_info->cfr_info.agc_gain_info0 |=
	(((uint32_t)HAL_RX_GET_64(rx_tlv,
		    PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		    GAIN_CHAIN1)) << 16);

	ppdu_info->cfr_info.agc_gain_info1 =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      GAIN_CHAIN2);

	ppdu_info->cfr_info.agc_gain_info1 |=
	(((uint32_t)HAL_RX_GET_64(rx_tlv,
		    PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		    GAIN_CHAIN3)) << 16);

	ppdu_info->cfr_info.agc_gain_info2 = 0;

	ppdu_info->cfr_info.agc_gain_info3 = 0;

	ppdu_info->cfr_info.mcs_rate =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RTT_MCS_RATE);
	ppdu_info->cfr_info.gi_type =
	HAL_RX_GET_64(rx_tlv,
		      PHYRX_LOCATION_RX_LOCATION_INFO_DETAILS,
		      RTT_GI_TYPE);
}
#endif

#ifdef CONFIG_WORD_BASED_TLV
/**
 * hal_rx_dump_mpdu_start_tlv_9224: dump RX mpdu_start TLV in structured
 *			       human readable format.
 * @mpdu_start: pointer the rx_attention TLV in pkt.
 * @dbg_level: log level.
 *
 * Return: void
 */
static inline void hal_rx_dump_mpdu_start_tlv_9224(void *mpdustart,
						   uint8_t dbg_level)
{
	struct rx_mpdu_start_compact *mpdu_info =
		(struct rx_mpdu_start_compact *)mpdustart;

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (1/5) - "
		  "rx_reo_queue_desc_addr_39_32 :%x"
		  "receive_queue_number:%x "
		  "pre_delim_err_warning:%x "
		  "first_delim_err:%x "
		  "pn_31_0:%x "
		  "pn_63_32:%x "
		  "pn_95_64:%x ",
		  mpdu_info->rx_reo_queue_desc_addr_39_32,
		  mpdu_info->receive_queue_number,
		  mpdu_info->pre_delim_err_warning,
		  mpdu_info->first_delim_err,
		  mpdu_info->pn_31_0,
		  mpdu_info->pn_63_32,
		  mpdu_info->pn_95_64);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (2/5) - "
		  "ast_index:%x "
		  "sw_peer_id:%x "
		  "mpdu_frame_control_valid:%x "
		  "mpdu_duration_valid:%x "
		  "mac_addr_ad1_valid:%x "
		  "mac_addr_ad2_valid:%x "
		  "mac_addr_ad3_valid:%x "
		  "mac_addr_ad4_valid:%x "
		  "mpdu_sequence_control_valid :%x"
		  "mpdu_qos_control_valid:%x "
		  "mpdu_ht_control_valid:%x "
		  "frame_encryption_info_valid :%x",
		  mpdu_info->ast_index,
		  mpdu_info->sw_peer_id,
		  mpdu_info->mpdu_frame_control_valid,
		  mpdu_info->mpdu_duration_valid,
		  mpdu_info->mac_addr_ad1_valid,
		  mpdu_info->mac_addr_ad2_valid,
		  mpdu_info->mac_addr_ad3_valid,
		  mpdu_info->mac_addr_ad4_valid,
		  mpdu_info->mpdu_sequence_control_valid,
		  mpdu_info->mpdu_qos_control_valid,
		  mpdu_info->mpdu_ht_control_valid,
		  mpdu_info->frame_encryption_info_valid);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (3/5) - "
		  "mpdu_fragment_number:%x "
		  "more_fragment_flag:%x "
		  "fr_ds:%x "
		  "to_ds:%x "
		  "encrypted:%x "
		  "mpdu_retry:%x "
		  "mpdu_sequence_number:%x ",
		  mpdu_info->mpdu_fragment_number,
		  mpdu_info->more_fragment_flag,
		  mpdu_info->fr_ds,
		  mpdu_info->to_ds,
		  mpdu_info->encrypted,
		  mpdu_info->mpdu_retry,
		  mpdu_info->mpdu_sequence_number);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (4/5) - "
		  "mpdu_frame_control_field:%x "
		  "mpdu_duration_field:%x ",
		  mpdu_info->mpdu_frame_control_field,
		  mpdu_info->mpdu_duration_field);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (5/5) - "
		  "mac_addr_ad1_31_0:%x "
		  "mac_addr_ad1_47_32:%x "
		  "mac_addr_ad2_15_0:%x "
		  "mac_addr_ad2_47_16:%x "
		  "mac_addr_ad3_31_0:%x "
		  "mac_addr_ad3_47_32:%x "
		  "mpdu_sequence_control_field :%x",
		  mpdu_info->mac_addr_ad1_31_0,
		  mpdu_info->mac_addr_ad1_47_32,
		  mpdu_info->mac_addr_ad2_15_0,
		  mpdu_info->mac_addr_ad2_47_16,
		  mpdu_info->mac_addr_ad3_31_0,
		  mpdu_info->mac_addr_ad3_47_32,
		  mpdu_info->mpdu_sequence_control_field);
}

/**
 * hal_rx_dump_msdu_end_tlv_9224: dump RX msdu_end TLV in structured
 *			     human readable format.
 * @ msdu_end: pointer the msdu_end TLV in pkt.
 * @ dbg_level: log level.
 *
 * Return: void
 */
static void hal_rx_dump_msdu_end_tlv_9224(void *msduend,
					  uint8_t dbg_level)
{
	struct rx_msdu_end_compact *msdu_end =
		(struct rx_msdu_end_compact *)msduend;

	QDF_TRACE(QDF_MODULE_ID_DP, dbg_level,
		  "rx_msdu_end tlv - "
		  "key_id_octet: %d "
		  "tcp_udp_chksum: %d "
		  "sa_idx_timeout: %d "
		  "da_idx_timeout: %d "
		  "msdu_limit_error: %d "
		  "flow_idx_timeout: %d "
		  "flow_idx_invalid: %d "
		  "wifi_parser_error: %d "
		  "sa_is_valid: %d "
		  "da_is_valid: %d "
		  "da_is_mcbc: %d "
		  "tkip_mic_err: %d "
		  "l3_header_padding: %d "
		  "first_msdu: %d "
		  "last_msdu: %d "
		  "sa_idx: %d "
		  "msdu_drop: %d "
		  "reo_destination_indication: %d "
		  "flow_idx: %d "
		  "fse_metadata: %d "
		  "cce_metadata: %d "
		  "sa_sw_peer_id: %d ",
		  msdu_end->key_id_octet,
		  msdu_end->tcp_udp_chksum,
		  msdu_end->sa_idx_timeout,
		  msdu_end->da_idx_timeout,
		  msdu_end->msdu_limit_error,
		  msdu_end->flow_idx_timeout,
		  msdu_end->flow_idx_invalid,
		  msdu_end->wifi_parser_error,
		  msdu_end->sa_is_valid,
		  msdu_end->da_is_valid,
		  msdu_end->da_is_mcbc,
		  msdu_end->tkip_mic_err,
		  msdu_end->l3_header_padding,
		  msdu_end->first_msdu,
		  msdu_end->last_msdu,
		  msdu_end->sa_idx,
		  msdu_end->msdu_drop,
		  msdu_end->reo_destination_indication,
		  msdu_end->flow_idx,
		  msdu_end->fse_metadata,
		  msdu_end->cce_metadata,
		  msdu_end->sa_sw_peer_id);
}
#else
static inline void hal_rx_dump_mpdu_start_tlv_9224(void *mpdustart,
						   uint8_t dbg_level)
{
	struct rx_mpdu_start *mpdu_start = (struct rx_mpdu_start *)mpdustart;
	struct rx_mpdu_info *mpdu_info =
		(struct rx_mpdu_info *)&mpdu_start->rx_mpdu_info_details;

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (1/5) - "
		  "rx_reo_queue_desc_addr_31_0 :%x"
		  "rx_reo_queue_desc_addr_39_32 :%x"
		  "receive_queue_number:%x "
		  "pre_delim_err_warning:%x "
		  "first_delim_err:%x "
		  "reserved_2a:%x "
		  "pn_31_0:%x "
		  "pn_63_32:%x "
		  "pn_95_64:%x "
		  "pn_127_96:%x "
		  "epd_en:%x "
		  "all_frames_shall_be_encrypted  :%x"
		  "encrypt_type:%x "
		  "wep_key_width_for_variable_key :%x"
		  "mesh_sta:%x "
		  "bssid_hit:%x "
		  "bssid_number:%x "
		  "tid:%x "
		  "reserved_7a:%x ",
		  mpdu_info->rx_reo_queue_desc_addr_31_0,
		  mpdu_info->rx_reo_queue_desc_addr_39_32,
		  mpdu_info->receive_queue_number,
		  mpdu_info->pre_delim_err_warning,
		  mpdu_info->first_delim_err,
		  mpdu_info->reserved_2a,
		  mpdu_info->pn_31_0,
		  mpdu_info->pn_63_32,
		  mpdu_info->pn_95_64,
		  mpdu_info->pn_127_96,
		  mpdu_info->epd_en,
		  mpdu_info->all_frames_shall_be_encrypted,
		  mpdu_info->encrypt_type,
		  mpdu_info->wep_key_width_for_variable_key,
		  mpdu_info->mesh_sta,
		  mpdu_info->bssid_hit,
		  mpdu_info->bssid_number,
		  mpdu_info->tid,
		  mpdu_info->reserved_7a);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (2/5) - "
		  "ast_index:%x "
		  "sw_peer_id:%x "
		  "mpdu_frame_control_valid:%x "
		  "mpdu_duration_valid:%x "
		  "mac_addr_ad1_valid:%x "
		  "mac_addr_ad2_valid:%x "
		  "mac_addr_ad3_valid:%x "
		  "mac_addr_ad4_valid:%x "
		  "mpdu_sequence_control_valid :%x"
		  "mpdu_qos_control_valid:%x "
		  "mpdu_ht_control_valid:%x "
		  "frame_encryption_info_valid :%x",
		  mpdu_info->ast_index,
		  mpdu_info->sw_peer_id,
		  mpdu_info->mpdu_frame_control_valid,
		  mpdu_info->mpdu_duration_valid,
		  mpdu_info->mac_addr_ad1_valid,
		  mpdu_info->mac_addr_ad2_valid,
		  mpdu_info->mac_addr_ad3_valid,
		  mpdu_info->mac_addr_ad4_valid,
		  mpdu_info->mpdu_sequence_control_valid,
		  mpdu_info->mpdu_qos_control_valid,
		  mpdu_info->mpdu_ht_control_valid,
		  mpdu_info->frame_encryption_info_valid);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (3/5) - "
		  "mpdu_fragment_number:%x "
		  "more_fragment_flag:%x "
		  "reserved_11a:%x "
		  "fr_ds:%x "
		  "to_ds:%x "
		  "encrypted:%x "
		  "mpdu_retry:%x "
		  "mpdu_sequence_number:%x ",
		  mpdu_info->mpdu_fragment_number,
		  mpdu_info->more_fragment_flag,
		  mpdu_info->reserved_11a,
		  mpdu_info->fr_ds,
		  mpdu_info->to_ds,
		  mpdu_info->encrypted,
		  mpdu_info->mpdu_retry,
		  mpdu_info->mpdu_sequence_number);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (4/5) - "
		  "mpdu_frame_control_field:%x "
		  "mpdu_duration_field:%x ",
		  mpdu_info->mpdu_frame_control_field,
		  mpdu_info->mpdu_duration_field);

	QDF_TRACE(QDF_MODULE_ID_HAL, dbg_level,
		  "rx_mpdu_start tlv (5/5) - "
		  "mac_addr_ad1_31_0:%x "
		  "mac_addr_ad1_47_32:%x "
		  "mac_addr_ad2_15_0:%x "
		  "mac_addr_ad2_47_16:%x "
		  "mac_addr_ad3_31_0:%x "
		  "mac_addr_ad3_47_32:%x "
		  "mpdu_sequence_control_field :%x"
		  "mac_addr_ad4_31_0:%x "
		  "mac_addr_ad4_47_32:%x "
		  "mpdu_qos_control_field:%x ",
		  mpdu_info->mac_addr_ad1_31_0,
		  mpdu_info->mac_addr_ad1_47_32,
		  mpdu_info->mac_addr_ad2_15_0,
		  mpdu_info->mac_addr_ad2_47_16,
		  mpdu_info->mac_addr_ad3_31_0,
		  mpdu_info->mac_addr_ad3_47_32,
		  mpdu_info->mpdu_sequence_control_field,
		  mpdu_info->mac_addr_ad4_31_0,
		  mpdu_info->mac_addr_ad4_47_32,
		  mpdu_info->mpdu_qos_control_field);
}

static void hal_rx_dump_msdu_end_tlv_9224(void *msduend,
					  uint8_t dbg_level)
{
	struct rx_msdu_end *msdu_end =
		(struct rx_msdu_end *)msduend;

	QDF_TRACE(QDF_MODULE_ID_DP, dbg_level,
		  "rx_msdu_end tlv - "
		  "key_id_octet: %d "
		  "cce_super_rule: %d "
		  "cce_classify_not_done_truncat: %d "
		  "cce_classify_not_done_cce_dis: %d "
		  "rule_indication_31_0: %d "
		  "tcp_udp_chksum: %d "
		  "sa_idx_timeout: %d "
		  "da_idx_timeout: %d "
		  "msdu_limit_error: %d "
		  "flow_idx_timeout: %d "
		  "flow_idx_invalid: %d "
		  "wifi_parser_error: %d "
		  "sa_is_valid: %d "
		  "da_is_valid: %d "
		  "da_is_mcbc: %d "
		  "tkip_mic_err: %d "
		  "l3_header_padding: %d "
		  "first_msdu: %d "
		  "last_msdu: %d "
		  "sa_idx: %d "
		  "msdu_drop: %d "
		  "reo_destination_indication: %d "
		  "flow_idx: %d "
		  "fse_metadata: %d "
		  "cce_metadata: %d "
		  "sa_sw_peer_id: %d ",
		  msdu_end->key_id_octet,
		  msdu_end->cce_super_rule,
		  msdu_end->cce_classify_not_done_truncate,
		  msdu_end->cce_classify_not_done_cce_dis,
		  msdu_end->rule_indication_31_0,
		  msdu_end->tcp_udp_chksum,
		  msdu_end->sa_idx_timeout,
		  msdu_end->da_idx_timeout,
		  msdu_end->msdu_limit_error,
		  msdu_end->flow_idx_timeout,
		  msdu_end->flow_idx_invalid,
		  msdu_end->wifi_parser_error,
		  msdu_end->sa_is_valid,
		  msdu_end->da_is_valid,
		  msdu_end->da_is_mcbc,
		  msdu_end->tkip_mic_err,
		  msdu_end->l3_header_padding,
		  msdu_end->first_msdu,
		  msdu_end->last_msdu,
		  msdu_end->sa_idx,
		  msdu_end->msdu_drop,
		  msdu_end->reo_destination_indication,
		  msdu_end->flow_idx,
		  msdu_end->fse_metadata,
		  msdu_end->cce_metadata,
		  msdu_end->sa_sw_peer_id);
}
#endif

/**
 * hal_reo_status_get_header_9224 - Process reo desc info
 * @d - Pointer to reo descriptor
 * @b - tlv type info
 * @h1 - Pointer to hal_reo_status_header where info to be stored
 *
 * Return - none.
 *
 */
static void hal_reo_status_get_header_9224(hal_ring_desc_t ring_desc,
					   int b, void *h1)
{
	uint64_t *d = (uint64_t *)ring_desc;
	uint64_t val1 = 0;
	struct hal_reo_status_header *h =
			(struct hal_reo_status_header *)h1;

	/* Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	d += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	switch (b) {
	case HAL_REO_QUEUE_STATS_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
			STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	case HAL_REO_FLUSH_QUEUE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_FLUSH_QUEUE_STATUS,
			STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	case HAL_REO_FLUSH_CACHE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
			STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	case HAL_REO_UNBLK_CACHE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_UNBLOCK_CACHE_STATUS,
			STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	case HAL_REO_TIMOUT_LIST_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_FLUSH_TIMEOUT_LIST_STATUS,
			STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	case HAL_REO_DESC_THRES_STATUS_TLV:
		val1 =
		  d[HAL_OFFSET_QW(REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
		  STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	case HAL_REO_UPDATE_RX_QUEUE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_UPDATE_RX_REO_QUEUE_STATUS,
			STATUS_HEADER_REO_STATUS_NUMBER)];
		break;
	default:
		qdf_nofl_err("ERROR: Unknown tlv\n");
		break;
	}
	h->cmd_num =
		HAL_GET_FIELD(
			      UNIFORM_REO_STATUS_HEADER, REO_STATUS_NUMBER,
			      val1);
	h->exec_time =
		HAL_GET_FIELD(UNIFORM_REO_STATUS_HEADER,
			      CMD_EXECUTION_TIME, val1);
	h->status =
		HAL_GET_FIELD(UNIFORM_REO_STATUS_HEADER,
			      REO_CMD_EXECUTION_STATUS, val1);
	switch (b) {
	case HAL_REO_QUEUE_STATS_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
			STATUS_HEADER_TIMESTAMP)];
		break;
	case HAL_REO_FLUSH_QUEUE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_FLUSH_QUEUE_STATUS,
			STATUS_HEADER_TIMESTAMP)];
		break;
	case HAL_REO_FLUSH_CACHE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
			STATUS_HEADER_TIMESTAMP)];
		break;
	case HAL_REO_UNBLK_CACHE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_UNBLOCK_CACHE_STATUS,
			STATUS_HEADER_TIMESTAMP)];
		break;
	case HAL_REO_TIMOUT_LIST_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_FLUSH_TIMEOUT_LIST_STATUS,
			STATUS_HEADER_TIMESTAMP)];
		break;
	case HAL_REO_DESC_THRES_STATUS_TLV:
		val1 =
		  d[HAL_OFFSET_QW(REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
		  STATUS_HEADER_TIMESTAMP)];
		break;
	case HAL_REO_UPDATE_RX_QUEUE_STATUS_TLV:
		val1 = d[HAL_OFFSET_QW(REO_UPDATE_RX_REO_QUEUE_STATUS,
			STATUS_HEADER_TIMESTAMP)];
		break;
	default:
		qdf_nofl_err("ERROR: Unknown tlv\n");
		break;
	}
	h->tstamp =
		HAL_GET_FIELD(UNIFORM_REO_STATUS_HEADER, TIMESTAMP, val1);
}

static
void *hal_rx_msdu0_buffer_addr_lsb_9224(void *link_desc_va)
{
	return (void *)HAL_RX_MSDU0_BUFFER_ADDR_LSB(link_desc_va);
}

static
void *hal_rx_msdu_desc_info_ptr_get_9224(void *msdu0)
{
	return (void *)HAL_RX_MSDU_DESC_INFO_PTR_GET(msdu0);
}

static
void *hal_ent_mpdu_desc_info_9224(void *ent_ring_desc)
{
	return (void *)HAL_ENT_MPDU_DESC_INFO(ent_ring_desc);
}

static
void *hal_dst_mpdu_desc_info_9224(void *dst_ring_desc)
{
	return (void *)HAL_DST_MPDU_DESC_INFO(dst_ring_desc);
}

/**
 * hal_reo_config_9224(): Set reo config parameters
 * @soc: hal soc handle
 * @reg_val: value to be set
 * @reo_params: reo parameters
 *
 * Return: void
 */
static void
hal_reo_config_9224(struct hal_soc *soc,
		    uint32_t reg_val,
		    struct hal_reo_params *reo_params)
{
	HAL_REO_R0_CONFIG(soc, reg_val, reo_params);
}

/**
 * hal_rx_msdu_desc_info_get_ptr_9224() - Get msdu desc info ptr
 * @msdu_details_ptr - Pointer to msdu_details_ptr
 *
 * Return - Pointer to rx_msdu_desc_info structure.
 *
 */
static void *hal_rx_msdu_desc_info_get_ptr_9224(void *msdu_details_ptr)
{
	return HAL_RX_MSDU_DESC_INFO_GET(msdu_details_ptr);
}

/**
 * hal_rx_link_desc_msdu0_ptr_9224 - Get pointer to rx_msdu details
 * @link_desc - Pointer to link desc
 *
 * Return - Pointer to rx_msdu_details structure
 *
 */
static void *hal_rx_link_desc_msdu0_ptr_9224(void *link_desc)
{
	return HAL_RX_LINK_DESC_MSDU0_PTR(link_desc);
}

/**
 * hal_get_window_address_9224(): Function to get hp/tp address
 * @hal_soc: Pointer to hal_soc
 * @addr: address offset of register
 *
 * Return: modified address offset of register
 */

static inline qdf_iomem_t hal_get_window_address_9224(struct hal_soc *hal_soc,
						      qdf_iomem_t addr)
{
	uint32_t offset = addr - hal_soc->dev_base_addr;
	qdf_iomem_t new_offset;

	/*
	 * If offset lies within DP register range, use 3rd window to write
	 * into DP region.
	 */
	if ((offset ^ UMAC_BASE) < WINDOW_RANGE_MASK) {
		new_offset = (hal_soc->dev_base_addr + (3 * WINDOW_START) +
			  (offset & WINDOW_RANGE_MASK));
	/*
	 * If offset lies within CE register range, use 2nd window to write
	 * into CE region.
	 */
	} else if ((offset ^ CE_WFSS_CE_REG_BASE) < WINDOW_RANGE_MASK) {
		new_offset = (hal_soc->dev_base_addr + (2 * WINDOW_START) +
			  (offset & WINDOW_RANGE_MASK));
	} else {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: ERROR: Accessing Wrong register\n", __func__);
		qdf_assert_always(0);
		return 0;
	}
	return new_offset;
}

static inline void hal_write_window_register(struct hal_soc *hal_soc)
{
	/* Write value into window configuration register */
	qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_REG_ADDRESS,
		      WINDOW_CONFIGURATION_VALUE_9224);
}

static
void hal_compute_reo_remap_ix2_ix3_9224(uint32_t *ring, uint32_t num_rings,
					uint32_t *remap1, uint32_t *remap2)
{
	switch (num_rings) {
	case 1:
		*remap1 = HAL_REO_REMAP_IX2(ring[0], 16) |
				HAL_REO_REMAP_IX2(ring[0], 17) |
				HAL_REO_REMAP_IX2(ring[0], 18) |
				HAL_REO_REMAP_IX2(ring[0], 19) |
				HAL_REO_REMAP_IX2(ring[0], 20) |
				HAL_REO_REMAP_IX2(ring[0], 21) |
				HAL_REO_REMAP_IX2(ring[0], 22) |
				HAL_REO_REMAP_IX2(ring[0], 23);

		*remap2 = HAL_REO_REMAP_IX3(ring[0], 24) |
				HAL_REO_REMAP_IX3(ring[0], 25) |
				HAL_REO_REMAP_IX3(ring[0], 26) |
				HAL_REO_REMAP_IX3(ring[0], 27) |
				HAL_REO_REMAP_IX3(ring[0], 28) |
				HAL_REO_REMAP_IX3(ring[0], 29) |
				HAL_REO_REMAP_IX3(ring[0], 30) |
				HAL_REO_REMAP_IX3(ring[0], 31);
		break;
	case 2:
		*remap1 = HAL_REO_REMAP_IX2(ring[0], 16) |
				HAL_REO_REMAP_IX2(ring[0], 17) |
				HAL_REO_REMAP_IX2(ring[1], 18) |
				HAL_REO_REMAP_IX2(ring[1], 19) |
				HAL_REO_REMAP_IX2(ring[0], 20) |
				HAL_REO_REMAP_IX2(ring[0], 21) |
				HAL_REO_REMAP_IX2(ring[1], 22) |
				HAL_REO_REMAP_IX2(ring[1], 23);

		*remap2 = HAL_REO_REMAP_IX3(ring[0], 24) |
				HAL_REO_REMAP_IX3(ring[0], 25) |
				HAL_REO_REMAP_IX3(ring[1], 26) |
				HAL_REO_REMAP_IX3(ring[1], 27) |
				HAL_REO_REMAP_IX3(ring[0], 28) |
				HAL_REO_REMAP_IX3(ring[0], 29) |
				HAL_REO_REMAP_IX3(ring[1], 30) |
				HAL_REO_REMAP_IX3(ring[1], 31);
		break;
	case 3:
		*remap1 = HAL_REO_REMAP_IX2(ring[0], 16) |
				HAL_REO_REMAP_IX2(ring[1], 17) |
				HAL_REO_REMAP_IX2(ring[2], 18) |
				HAL_REO_REMAP_IX2(ring[0], 19) |
				HAL_REO_REMAP_IX2(ring[1], 20) |
				HAL_REO_REMAP_IX2(ring[2], 21) |
				HAL_REO_REMAP_IX2(ring[0], 22) |
				HAL_REO_REMAP_IX2(ring[1], 23);

		*remap2 = HAL_REO_REMAP_IX3(ring[2], 24) |
				HAL_REO_REMAP_IX3(ring[0], 25) |
				HAL_REO_REMAP_IX3(ring[1], 26) |
				HAL_REO_REMAP_IX3(ring[2], 27) |
				HAL_REO_REMAP_IX3(ring[0], 28) |
				HAL_REO_REMAP_IX3(ring[1], 29) |
				HAL_REO_REMAP_IX3(ring[2], 30) |
				HAL_REO_REMAP_IX3(ring[0], 31);
		break;
	case 4:
		*remap1 = HAL_REO_REMAP_IX2(ring[0], 16) |
				HAL_REO_REMAP_IX2(ring[1], 17) |
				HAL_REO_REMAP_IX2(ring[2], 18) |
				HAL_REO_REMAP_IX2(ring[3], 19) |
				HAL_REO_REMAP_IX2(ring[0], 20) |
				HAL_REO_REMAP_IX2(ring[1], 21) |
				HAL_REO_REMAP_IX2(ring[2], 22) |
				HAL_REO_REMAP_IX2(ring[3], 23);

		*remap2 = HAL_REO_REMAP_IX3(ring[0], 24) |
				HAL_REO_REMAP_IX3(ring[1], 25) |
				HAL_REO_REMAP_IX3(ring[2], 26) |
				HAL_REO_REMAP_IX3(ring[3], 27) |
				HAL_REO_REMAP_IX3(ring[0], 28) |
				HAL_REO_REMAP_IX3(ring[1], 29) |
				HAL_REO_REMAP_IX3(ring[2], 30) |
				HAL_REO_REMAP_IX3(ring[3], 31);
		break;
	}
}

static
void hal_compute_reo_remap_ix0_9224(struct hal_soc *soc)
{
	uint32_t remap0;

	remap0 = HAL_REG_READ(soc, HWIO_REO_R0_DESTINATION_RING_CTRL_IX_0_ADDR
			      (REO_REG_REG_BASE));

	remap0 &= ~(HAL_REO_REMAP_IX0(0xF, 6));
	remap0 |= HAL_REO_REMAP_IX0(REO2PPE_DST_RING, 6);

	HAL_REG_WRITE(soc, HWIO_REO_R0_DESTINATION_RING_CTRL_IX_0_ADDR
		      (REO_REG_REG_BASE), remap0);

	hal_debug("HWIO_REO_R0_DESTINATION_RING_CTRL_IX_0_ADDR 0x%x",
		  HAL_REG_READ(soc, HWIO_REO_R0_DESTINATION_RING_CTRL_IX_0_ADDR
		  (REO_REG_REG_BASE)));
}

/**
 * hal_rx_flow_setup_fse_9224() - Setup a flow search entry in HW FST
 * @fst: Pointer to the Rx Flow Search Table
 * @table_offset: offset into the table where the flow is to be setup
 * @flow: Flow Parameters
 *
 * Return: Success/Failure
 */
static void *
hal_rx_flow_setup_fse_9224(uint8_t *rx_fst, uint32_t table_offset,
			   uint8_t *rx_flow)
{
	struct hal_rx_fst *fst = (struct hal_rx_fst *)rx_fst;
	struct hal_rx_flow *flow = (struct hal_rx_flow *)rx_flow;
	uint8_t *fse;
	bool fse_valid;

	if (table_offset >= fst->max_entries) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "HAL FSE table offset %u exceeds max entries %u",
			  table_offset, fst->max_entries);
		return NULL;
	}

	fse = (uint8_t *)fst->base_vaddr +
			(table_offset * HAL_RX_FST_ENTRY_SIZE);

	fse_valid = HAL_GET_FLD(fse, RX_FLOW_SEARCH_ENTRY, VALID);

	if (fse_valid) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  "HAL FSE %pK already valid", fse);
		return NULL;
	}

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, SRC_IP_127_96) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, SRC_IP_127_96,
			       qdf_htonl(flow->tuple_info.src_ip_127_96));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, SRC_IP_95_64) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, SRC_IP_95_64,
			       qdf_htonl(flow->tuple_info.src_ip_95_64));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, SRC_IP_63_32) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, SRC_IP_63_32,
			       qdf_htonl(flow->tuple_info.src_ip_63_32));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, SRC_IP_31_0) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, SRC_IP_31_0,
			       qdf_htonl(flow->tuple_info.src_ip_31_0));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, DEST_IP_127_96) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, DEST_IP_127_96,
			       qdf_htonl(flow->tuple_info.dest_ip_127_96));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, DEST_IP_95_64) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, DEST_IP_95_64,
			       qdf_htonl(flow->tuple_info.dest_ip_95_64));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, DEST_IP_63_32) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, DEST_IP_63_32,
			       qdf_htonl(flow->tuple_info.dest_ip_63_32));

	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, DEST_IP_31_0) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, DEST_IP_31_0,
			       qdf_htonl(flow->tuple_info.dest_ip_31_0));

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, DEST_PORT);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, DEST_PORT) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, DEST_PORT,
			       (flow->tuple_info.dest_port));

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, SRC_PORT);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, SRC_PORT) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, SRC_PORT,
			       (flow->tuple_info.src_port));

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, L4_PROTOCOL);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, L4_PROTOCOL) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, L4_PROTOCOL,
			       flow->tuple_info.l4_protocol);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, USE_PPE);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, USE_PPE) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, USE_PPE, flow->use_ppe_ds);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, PRIORITY_VALID);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, PRIORITY_VALID) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, PRIORITY_VALID,
			       flow->priority_vld);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, SERVICE_CODE);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, SERVICE_CODE) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, SERVICE_CODE,
			       flow->service_code);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, REO_DESTINATION_HANDLER);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, REO_DESTINATION_HANDLER) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, REO_DESTINATION_HANDLER,
			       flow->reo_destination_handler);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, VALID);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, VALID) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, VALID, 1);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, METADATA);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, METADATA) =
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY, METADATA,
			       flow->fse_metadata);

	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, REO_DESTINATION_INDICATION);
	HAL_SET_FLD(fse, RX_FLOW_SEARCH_ENTRY, REO_DESTINATION_INDICATION) |=
		HAL_SET_FLD_SM(RX_FLOW_SEARCH_ENTRY,
			       REO_DESTINATION_INDICATION,
			       flow->reo_destination_indication);

	/* Reset all the other fields in FSE */
	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, RESERVED_9);
	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, MSDU_DROP);
	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, MSDU_COUNT);
	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, MSDU_BYTE_COUNT);
	HAL_CLR_FLD(fse, RX_FLOW_SEARCH_ENTRY, TIMESTAMP);

	return fse;
}

#ifndef NO_RX_PKT_HDR_TLV
/**
 * hal_rx_dump_pkt_hdr_tlv: dump RX pkt header TLV in hex format
 * @ pkt_hdr_tlv: pointer the pkt_hdr_tlv in pkt.
 * @ dbg_level: log level.
 *
 * Return: void
 */
static inline void hal_rx_dump_pkt_hdr_tlv_9224(struct rx_pkt_tlvs *pkt_tlvs,
						uint8_t dbg_level)
{
	struct rx_pkt_hdr_tlv *pkt_hdr_tlv = &pkt_tlvs->pkt_hdr_tlv;

	hal_verbose_debug("\n---------------\n"
			  "rx_pkt_hdr_tlv\n"
			  "---------------\n"
			  "phy_ppdu_id %llu ",
			  pkt_hdr_tlv->phy_ppdu_id);

	hal_verbose_hex_dump(pkt_hdr_tlv->rx_pkt_hdr,
			     sizeof(pkt_hdr_tlv->rx_pkt_hdr));
}
#else
/**
 * hal_rx_dump_pkt_hdr_tlv: dump RX pkt header TLV in hex format
 * @ pkt_hdr_tlv: pointer the pkt_hdr_tlv in pkt.
 * @ dbg_level: log level.
 *
 * Return: void
 */
static inline void hal_rx_dump_pkt_hdr_tlv_9224(struct rx_pkt_tlvs *pkt_tlvs,
						uint8_t dbg_level)
{
}
#endif

/*
 * hal_tx_dump_ppe_vp_entry_9224()
 * @hal_soc_hdl: HAL SoC handle
 *
 * Return: void
 */
static inline
void hal_tx_dump_ppe_vp_entry_9224(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0, i;

	for (i = 0; i < HAL_PPE_VP_ENTRIES_MAX; i++) {
		reg_addr =
			HWIO_TCL_R0_PPE_VP_CONFIG_TABLE_n_ADDR(
				MAC_TCL_REG_REG_BASE,
				i);
		reg_val = HAL_REG_READ(soc, reg_addr);
		hal_verbose_debug("%d: 0x%x\n", i, reg_val);
	}
}

/**
 * hal_rx_dump_pkt_tlvs_9224(): API to print RX Pkt TLVS QCN9224
 * @hal_soc_hdl: hal_soc handle
 * @buf: pointer the pkt buffer
 * @dbg_level: log level
 *
 * Return: void
 */
#ifdef CONFIG_WORD_BASED_TLV
static void hal_rx_dump_pkt_tlvs_9224(hal_soc_handle_t hal_soc_hdl,
				      uint8_t *buf, uint8_t dbg_level)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_end_compact *msdu_end =
					&pkt_tlvs->msdu_end_tlv.rx_msdu_end;
	struct rx_mpdu_start_compact *mpdu_start =
				&pkt_tlvs->mpdu_start_tlv.rx_mpdu_start;

	hal_rx_dump_msdu_end_tlv_9224(msdu_end, dbg_level);
	hal_rx_dump_mpdu_start_tlv_9224(mpdu_start, dbg_level);
	hal_rx_dump_pkt_hdr_tlv_9224(pkt_tlvs, dbg_level);
}
#else
static void hal_rx_dump_pkt_tlvs_9224(hal_soc_handle_t hal_soc_hdl,
				      uint8_t *buf, uint8_t dbg_level)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_end *msdu_end = &pkt_tlvs->msdu_end_tlv.rx_msdu_end;
	struct rx_mpdu_start *mpdu_start =
				&pkt_tlvs->mpdu_start_tlv.rx_mpdu_start;

	hal_rx_dump_msdu_end_tlv_9224(msdu_end, dbg_level);
	hal_rx_dump_mpdu_start_tlv_9224(mpdu_start, dbg_level);
	hal_rx_dump_pkt_hdr_tlv_9224(pkt_tlvs, dbg_level);
}
#endif

#define HAL_NUM_TCL_BANKS_9224 48

/**
 * hal_cmem_write_9224() - function for CMEM buffer writing
 * @hal_soc_hdl: HAL SOC handle
 * @offset: CMEM address
 * @value: value to write
 *
 * Return: None.
 */
static void hal_cmem_write_9224(hal_soc_handle_t hal_soc_hdl,
				uint32_t offset,
				uint32_t value)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;

	pld_reg_write(hal->qdf_dev->dev, offset, value, NULL);
}

/**
 * hal_tx_get_num_tcl_banks_9224() - Get number of banks in target
 *
 * Returns: number of bank
 */
static uint8_t hal_tx_get_num_tcl_banks_9224(void)
{
	return HAL_NUM_TCL_BANKS_9224;
}

static void hal_reo_setup_9224(struct hal_soc *soc, void *reoparams,
			       int qref_reset)
{
	uint32_t reg_val;
	struct hal_reo_params *reo_params = (struct hal_reo_params *)reoparams;

	reg_val = HAL_REG_READ(soc, HWIO_REO_R0_GENERAL_ENABLE_ADDR(
		REO_REG_REG_BASE));

	hal_reo_config_9224(soc, reg_val, reo_params);
	/* Other ring enable bits and REO_ENABLE will be set by FW */

	/* TODO: Setup destination ring mapping if enabled */

	/* TODO: Error destination ring setting is left to default.
	 * Default setting is to send all errors to release ring.
	 */

	/* Set the reo descriptor swap bits in case of BIG endian platform */
	hal_setup_reo_swap(soc);

	HAL_REG_WRITE(soc,
		      HWIO_REO_R0_AGING_THRESHOLD_IX_0_ADDR(REO_REG_REG_BASE),
		      HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_MS * 1000);

	HAL_REG_WRITE(soc,
		      HWIO_REO_R0_AGING_THRESHOLD_IX_1_ADDR(REO_REG_REG_BASE),
		      (HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_MS * 1000));

	HAL_REG_WRITE(soc,
		      HWIO_REO_R0_AGING_THRESHOLD_IX_2_ADDR(REO_REG_REG_BASE),
		      (HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_MS * 1000));

	HAL_REG_WRITE(soc,
		      HWIO_REO_R0_AGING_THRESHOLD_IX_3_ADDR(REO_REG_REG_BASE),
		      (HAL_DEFAULT_VO_REO_TIMEOUT_MS * 1000));

	/*
	 * When hash based routing is enabled, routing of the rx packet
	 * is done based on the following value: 1 _ _ _ _ The last 4
	 * bits are based on hash[3:0]. This means the possible values
	 * are 0x10 to 0x1f. This value is used to look-up the
	 * ring ID configured in Destination_Ring_Ctrl_IX_* register.
	 * The Destination_Ring_Ctrl_IX_2 and Destination_Ring_Ctrl_IX_3
	 * registers need to be configured to set-up the 16 entries to
	 * map the hash values to a ring number. There are 3 bits per
	 * hash entry  which are mapped as follows:
	 * 0: TCL, 1:SW1, 2:SW2, * 3:SW3, 4:SW4, 5:Release, 6:FW(WIFI),
	 * 7: NOT_USED.
	 */
	if (reo_params->rx_hash_enabled) {
		hal_compute_reo_remap_ix0_9224(soc);

		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_DESTINATION_RING_CTRL_IX_1_ADDR
			      (REO_REG_REG_BASE), reo_params->remap0);

		hal_debug("HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR 0x%x",
			  HAL_REG_READ(soc,
				       HWIO_REO_R0_DESTINATION_RING_CTRL_IX_1_ADDR(
				       REO_REG_REG_BASE)));

		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR
			      (REO_REG_REG_BASE), reo_params->remap1);

		hal_debug("HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR 0x%x",
			  HAL_REG_READ(soc,
				       HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR(
				       REO_REG_REG_BASE)));

		HAL_REG_WRITE(soc,
			      HWIO_REO_R0_DESTINATION_RING_CTRL_IX_3_ADDR
			      (REO_REG_REG_BASE), reo_params->remap2);

		hal_debug("HWIO_REO_R0_DESTINATION_RING_CTRL_IX_3_ADDR 0x%x",
			  HAL_REG_READ(soc,
				       HWIO_REO_R0_DESTINATION_RING_CTRL_IX_3_ADDR(
				       REO_REG_REG_BASE)));
	}

	/* TODO: Check if the following registers shoould be setup by host:
	 * AGING_CONTROL
	 * HIGH_MEMORY_THRESHOLD
	 * GLOBAL_LINK_DESC_COUNT_THRESH_IX_0[1,2]
	 * GLOBAL_LINK_DESC_COUNT_CTRL
	 */

	soc->reo_qref = *reo_params->reo_qref;
	hal_reo_shared_qaddr_init((hal_soc_handle_t)soc, qref_reset);
}

static uint16_t hal_get_rx_max_ba_window_qcn9224(int tid)
{
	return HAL_RX_BA_WINDOW_1024;
}

/**
 * hal_qcn9224_get_reo_qdesc_size()- Get the reo queue descriptor size
 *			  from the give Block-Ack window size
 * Return: reo queue descriptor size
 */
static uint32_t hal_qcn9224_get_reo_qdesc_size(uint32_t ba_window_size, int tid)
{
	/* Hardcode the ba_window_size to HAL_RX_MAX_BA_WINDOW for
	 * NON_QOS_TID until HW issues are resolved.
	 */
	if (tid != HAL_NON_QOS_TID)
		ba_window_size = hal_get_rx_max_ba_window_qcn9224(tid);

	/* Return descriptor size corresponding to window size of 2 since
	 * we set ba_window_size to 2 while setting up REO descriptors as
	 * a WAR to get 2k jump exception aggregates are received without
	 * a BA session.
	 */
	if (ba_window_size <= 1) {
		if (tid != HAL_NON_QOS_TID)
			return sizeof(struct rx_reo_queue) +
				sizeof(struct rx_reo_queue_ext);
		else
			return sizeof(struct rx_reo_queue);
	}

	if (ba_window_size <= 105)
		return sizeof(struct rx_reo_queue) +
			sizeof(struct rx_reo_queue_ext);

	if (ba_window_size <= 210)
		return sizeof(struct rx_reo_queue) +
			(2 * sizeof(struct rx_reo_queue_ext));

	if (ba_window_size <= 256)
		return sizeof(struct rx_reo_queue) +
			(3 * sizeof(struct rx_reo_queue_ext));

	return sizeof(struct rx_reo_queue) +
		(10 * sizeof(struct rx_reo_queue_ext)) +
		sizeof(struct rx_reo_queue_1k);
}

/*
 * hal_tx_get_num_ppe_vp_tbl_entries_9224()
 * @hal_soc_hdl: HAL SoC handle
 *
 * Return: Number of PPE VP entries
 */
static
uint32_t hal_tx_get_num_ppe_vp_tbl_entries_9224(hal_soc_handle_t hal_soc_hdl)
{
	return HAL_PPE_VP_ENTRIES_MAX;
}

/*
 * hal_tx_get_num_ppe_vp_search_idx_reg_entries_9224()
 * @hal_soc_hdl: HAL SoC handle
 *
 * Return: Number of PPE VP search index registers
 */
static
uint32_t hal_tx_get_num_ppe_vp_search_idx_reg_entries_9224(hal_soc_handle_t hal_soc_hdl)
{
	return HAL_PPE_VP_SEARCH_IDX_REG_MAX;
}

/**
 * hal_rx_tlv_msdu_done_copy_get_9224() - Get msdu done copy bit from rx_tlv
 *
 * Returns: msdu done copy bit
 */
static inline uint32_t hal_rx_tlv_msdu_done_copy_get_9224(uint8_t *buf)
{
	return HAL_RX_TLV_MSDU_DONE_COPY_GET(buf);
}

static void hal_hw_txrx_ops_attach_qcn9224(struct hal_soc *hal_soc)
{
	/* init and setup */
	hal_soc->ops->hal_srng_dst_hw_init = hal_srng_dst_hw_init_generic;
	hal_soc->ops->hal_srng_src_hw_init = hal_srng_src_hw_init_generic;
	hal_soc->ops->hal_srng_hw_disable = hal_srng_hw_disable_generic;
	hal_soc->ops->hal_get_hw_hptp = hal_get_hw_hptp_generic;
	hal_soc->ops->hal_get_window_address = hal_get_window_address_9224;
	hal_soc->ops->hal_cmem_write = hal_cmem_write_9224;

	/* tx */
	hal_soc->ops->hal_tx_set_dscp_tid_map = hal_tx_set_dscp_tid_map_9224;
	hal_soc->ops->hal_tx_update_dscp_tid = hal_tx_update_dscp_tid_9224;
	hal_soc->ops->hal_tx_comp_get_status =
			hal_tx_comp_get_status_generic_be;
	hal_soc->ops->hal_tx_init_cmd_credit_ring =
			hal_tx_init_cmd_credit_ring_9224;
	hal_soc->ops->hal_tx_set_ppe_cmn_cfg =
			hal_tx_set_ppe_cmn_config_9224;
	hal_soc->ops->hal_tx_set_ppe_vp_entry =
			hal_tx_set_ppe_vp_entry_9224;
	hal_soc->ops->hal_ppeds_cfg_ast_override_map_reg =
			hal_ppeds_cfg_ast_override_map_reg_9224;
	hal_soc->ops->hal_tx_set_ppe_pri2tid =
			hal_tx_set_ppe_pri2tid_map_9224;
	hal_soc->ops->hal_tx_update_ppe_pri2tid =
			hal_tx_update_ppe_pri2tid_9224;
	hal_soc->ops->hal_tx_dump_ppe_vp_entry =
			hal_tx_dump_ppe_vp_entry_9224;
	hal_soc->ops->hal_tx_get_num_ppe_vp_tbl_entries =
			hal_tx_get_num_ppe_vp_tbl_entries_9224;
	hal_soc->ops->hal_tx_enable_pri2tid_map =
			hal_tx_enable_pri2tid_map_9224;
	hal_soc->ops->hal_tx_config_rbm_mapping_be =
				hal_tx_config_rbm_mapping_be_9224;

	/* rx */
	hal_soc->ops->hal_rx_msdu_start_nss_get = hal_rx_tlv_nss_get_be;
	hal_soc->ops->hal_rx_mon_hw_desc_get_mpdu_status =
		hal_rx_mon_hw_desc_get_mpdu_status_be;
	hal_soc->ops->hal_rx_get_tlv = hal_rx_get_tlv_9224;
	hal_soc->ops->hal_rx_proc_phyrx_other_receive_info_tlv =
				hal_rx_proc_phyrx_other_receive_info_tlv_9224;

	hal_soc->ops->hal_rx_dump_msdu_end_tlv = hal_rx_dump_msdu_end_tlv_9224;
	hal_soc->ops->hal_rx_dump_mpdu_start_tlv =
					hal_rx_dump_mpdu_start_tlv_9224;
	hal_soc->ops->hal_rx_dump_pkt_tlvs = hal_rx_dump_pkt_tlvs_9224;

	hal_soc->ops->hal_get_link_desc_size = hal_get_link_desc_size_9224;
	hal_soc->ops->hal_rx_mpdu_start_tid_get = hal_rx_tlv_tid_get_be;
	hal_soc->ops->hal_rx_msdu_start_reception_type_get =
					hal_rx_tlv_reception_type_get_be;
	hal_soc->ops->hal_rx_msdu_end_da_idx_get =
					hal_rx_msdu_end_da_idx_get_be;
	hal_soc->ops->hal_rx_msdu_desc_info_get_ptr =
					hal_rx_msdu_desc_info_get_ptr_9224;
	hal_soc->ops->hal_rx_link_desc_msdu0_ptr =
					hal_rx_link_desc_msdu0_ptr_9224;
	hal_soc->ops->hal_reo_status_get_header =
					hal_reo_status_get_header_9224;
#ifdef QCA_MONITOR_2_0_SUPPORT
	hal_soc->ops->hal_rx_status_get_tlv_info =
					hal_rx_status_get_tlv_info_wrapper_be;
#endif
	hal_soc->ops->hal_rx_wbm_err_info_get =
					hal_rx_wbm_err_info_get_generic_be;
	hal_soc->ops->hal_tx_set_pcp_tid_map =
					hal_tx_set_pcp_tid_map_generic_be;
	hal_soc->ops->hal_tx_update_pcp_tid_map =
					hal_tx_update_pcp_tid_generic_be;
	hal_soc->ops->hal_tx_set_tidmap_prty =
					hal_tx_update_tidmap_prty_generic_be;
	hal_soc->ops->hal_rx_get_rx_fragment_number =
					hal_rx_get_rx_fragment_number_be,
	hal_soc->ops->hal_rx_msdu_end_da_is_mcbc_get =
					hal_rx_tlv_da_is_mcbc_get_be;
	hal_soc->ops->hal_rx_msdu_end_is_tkip_mic_err =
					hal_rx_tlv_is_tkip_mic_err_get_be;
	hal_soc->ops->hal_rx_msdu_end_sa_is_valid_get =
					hal_rx_tlv_sa_is_valid_get_be;
	hal_soc->ops->hal_rx_msdu_end_sa_idx_get = hal_rx_tlv_sa_idx_get_be;
	hal_soc->ops->hal_rx_desc_is_first_msdu = hal_rx_desc_is_first_msdu_be;
	hal_soc->ops->hal_rx_msdu_end_l3_hdr_padding_get =
		hal_rx_tlv_l3_hdr_padding_get_be;
	hal_soc->ops->hal_rx_encryption_info_valid =
					hal_rx_encryption_info_valid_be;
	hal_soc->ops->hal_rx_print_pn = hal_rx_print_pn_be;
	hal_soc->ops->hal_rx_msdu_end_first_msdu_get =
					hal_rx_tlv_first_msdu_get_be;
	hal_soc->ops->hal_rx_msdu_end_da_is_valid_get =
					hal_rx_tlv_da_is_valid_get_be;
	hal_soc->ops->hal_rx_msdu_end_last_msdu_get =
					hal_rx_tlv_last_msdu_get_be;
	hal_soc->ops->hal_rx_get_mpdu_mac_ad4_valid =
					hal_rx_get_mpdu_mac_ad4_valid_be;
	hal_soc->ops->hal_rx_mpdu_start_sw_peer_id_get =
		hal_rx_mpdu_start_sw_peer_id_get_be;
	hal_soc->ops->hal_rx_tlv_peer_meta_data_get =
		hal_rx_msdu_peer_meta_data_get_be;
	hal_soc->ops->hal_rx_mpdu_get_to_ds = hal_rx_mpdu_get_to_ds_be;
	hal_soc->ops->hal_rx_mpdu_get_fr_ds = hal_rx_mpdu_get_fr_ds_be;
	hal_soc->ops->hal_rx_get_mpdu_frame_control_valid =
		hal_rx_get_mpdu_frame_control_valid_be;
	hal_soc->ops->hal_rx_mpdu_get_addr1 = hal_rx_mpdu_get_addr1_be;
	hal_soc->ops->hal_rx_mpdu_get_addr2 = hal_rx_mpdu_get_addr2_be;
	hal_soc->ops->hal_rx_mpdu_get_addr3 = hal_rx_mpdu_get_addr3_be;
	hal_soc->ops->hal_rx_get_mpdu_sequence_control_valid =
		hal_rx_get_mpdu_sequence_control_valid_be;
	hal_soc->ops->hal_rx_is_unicast = hal_rx_is_unicast_be;
	hal_soc->ops->hal_rx_tid_get = hal_rx_tid_get_be;
	hal_soc->ops->hal_rx_mpdu_start_mpdu_qos_control_valid_get =
		hal_rx_mpdu_start_mpdu_qos_control_valid_get_be;
	hal_soc->ops->hal_rx_msdu_end_sa_sw_peer_id_get =
					hal_rx_msdu_end_sa_sw_peer_id_get_be;
	hal_soc->ops->hal_rx_msdu0_buffer_addr_lsb =
					hal_rx_msdu0_buffer_addr_lsb_9224;
	hal_soc->ops->hal_rx_msdu_desc_info_ptr_get =
					hal_rx_msdu_desc_info_ptr_get_9224;
	hal_soc->ops->hal_ent_mpdu_desc_info = hal_ent_mpdu_desc_info_9224;
	hal_soc->ops->hal_dst_mpdu_desc_info = hal_dst_mpdu_desc_info_9224;
	hal_soc->ops->hal_rx_get_fc_valid = hal_rx_get_fc_valid_be;
	hal_soc->ops->hal_rx_get_to_ds_flag = hal_rx_get_to_ds_flag_be;
	hal_soc->ops->hal_rx_get_mac_addr2_valid =
						hal_rx_get_mac_addr2_valid_be;
	hal_soc->ops->hal_reo_config = hal_reo_config_9224;
	hal_soc->ops->hal_rx_msdu_flow_idx_get = hal_rx_msdu_flow_idx_get_be;
	hal_soc->ops->hal_rx_msdu_flow_idx_invalid =
					hal_rx_msdu_flow_idx_invalid_be;
	hal_soc->ops->hal_rx_msdu_flow_idx_timeout =
					hal_rx_msdu_flow_idx_timeout_be;
	hal_soc->ops->hal_rx_msdu_fse_metadata_get =
					hal_rx_msdu_fse_metadata_get_be;
	hal_soc->ops->hal_rx_msdu_cce_match_get =
					hal_rx_msdu_cce_match_get_be;
	hal_soc->ops->hal_rx_msdu_cce_metadata_get =
					hal_rx_msdu_cce_metadata_get_be;
	hal_soc->ops->hal_rx_msdu_get_flow_params =
					hal_rx_msdu_get_flow_params_be;
	hal_soc->ops->hal_rx_tlv_get_tcp_chksum = hal_rx_tlv_get_tcp_chksum_be;
	hal_soc->ops->hal_rx_get_rx_sequence = hal_rx_get_rx_sequence_be;

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
	hal_soc->ops->hal_rx_get_bb_info = hal_rx_get_bb_info_9224;
	hal_soc->ops->hal_rx_get_rtt_info = hal_rx_get_rtt_info_9224;
#else
	hal_soc->ops->hal_rx_get_bb_info = NULL;
	hal_soc->ops->hal_rx_get_rtt_info = NULL;
#endif

	/* rx - msdu fast path info fields */
	hal_soc->ops->hal_rx_msdu_packet_metadata_get =
				hal_rx_msdu_packet_metadata_get_generic_be;
	hal_soc->ops->hal_rx_mpdu_start_tlv_tag_valid =
				hal_rx_mpdu_start_tlv_tag_valid_be;
	hal_soc->ops->hal_rx_wbm_err_msdu_continuation_get =
				hal_rx_wbm_err_msdu_continuation_get_9224;

	/* rx - TLV struct offsets */
	hal_soc->ops->hal_rx_msdu_end_offset_get =
		hal_rx_msdu_end_offset_get_generic;
	hal_soc->ops->hal_rx_mpdu_start_offset_get =
					hal_rx_mpdu_start_offset_get_generic;
#ifndef NO_RX_PKT_HDR_TLV
	hal_soc->ops->hal_rx_pkt_tlv_offset_get =
					hal_rx_pkt_tlv_offset_get_generic;
#endif
	hal_soc->ops->hal_rx_flow_setup_fse = hal_rx_flow_setup_fse_9224;

	hal_soc->ops->hal_rx_flow_get_tuple_info =
					hal_rx_flow_get_tuple_info_be;
	 hal_soc->ops->hal_rx_flow_delete_entry =
					hal_rx_flow_delete_entry_be;
	hal_soc->ops->hal_rx_fst_get_fse_size = hal_rx_fst_get_fse_size_be;
	hal_soc->ops->hal_compute_reo_remap_ix2_ix3 =
					hal_compute_reo_remap_ix2_ix3_9224;

	hal_soc->ops->hal_rx_msdu_get_reo_destination_indication =
				hal_rx_msdu_get_reo_destination_indication_be;
	hal_soc->ops->hal_rx_get_tlv_size = hal_rx_get_tlv_size_generic_be;
	hal_soc->ops->hal_rx_msdu_is_wlan_mcast =
					hal_rx_msdu_is_wlan_mcast_generic_be;
	hal_soc->ops->hal_tx_get_num_tcl_banks = hal_tx_get_num_tcl_banks_9224;
	hal_soc->ops->hal_rx_tlv_decap_format_get =
					hal_rx_tlv_decap_format_get_be;
#ifdef RECEIVE_OFFLOAD
	hal_soc->ops->hal_rx_tlv_get_offload_info =
					hal_rx_tlv_get_offload_info_be;
	hal_soc->ops->hal_rx_get_proto_params = hal_rx_get_proto_params_be;
	hal_soc->ops->hal_rx_get_l3_l4_offsets = hal_rx_get_l3_l4_offsets_be;
#endif
	hal_soc->ops->hal_rx_tlv_msdu_done_get =
					hal_rx_tlv_msdu_done_copy_get_9224;
	hal_soc->ops->hal_rx_tlv_msdu_len_get =
					hal_rx_msdu_start_msdu_len_get_be;
	hal_soc->ops->hal_rx_get_frame_ctrl_field =
					hal_rx_get_frame_ctrl_field_be;
	hal_soc->ops->hal_rx_tlv_csum_err_get = hal_rx_tlv_csum_err_get_be;
#ifndef CONFIG_WORD_BASED_TLV
	hal_soc->ops->hal_rx_mpdu_info_ampdu_flag_get =
					hal_rx_mpdu_info_ampdu_flag_get_be;
	hal_soc->ops->hal_rx_mpdu_get_addr4 = hal_rx_mpdu_get_addr4_be;
	hal_soc->ops->hal_rx_hw_desc_get_ppduid_get =
		hal_rx_hw_desc_get_ppduid_get_be;
	hal_soc->ops->hal_rx_get_ppdu_id = hal_rx_get_ppdu_id_be;
	hal_soc->ops->hal_rx_tlv_phy_ppdu_id_get =
					hal_rx_attn_phy_ppdu_id_get_be;
	hal_soc->ops->hal_rx_get_filter_category =
						hal_rx_get_filter_category_be;
#endif
	hal_soc->ops->hal_rx_tlv_msdu_len_set =
					hal_rx_msdu_start_msdu_len_set_be;
	hal_soc->ops->hal_rx_tlv_sgi_get = hal_rx_tlv_sgi_get_be;
	hal_soc->ops->hal_rx_tlv_rate_mcs_get = hal_rx_tlv_rate_mcs_get_be;
	hal_soc->ops->hal_rx_tlv_bw_get = hal_rx_tlv_bw_get_be;
	hal_soc->ops->hal_rx_tlv_get_pkt_type = hal_rx_tlv_get_pkt_type_be;
	hal_soc->ops->hal_rx_tlv_mic_err_get = hal_rx_tlv_mic_err_get_be;
	hal_soc->ops->hal_rx_tlv_decrypt_err_get =
					hal_rx_tlv_decrypt_err_get_be;
	hal_soc->ops->hal_rx_tlv_first_mpdu_get = hal_rx_tlv_first_mpdu_get_be;
	hal_soc->ops->hal_rx_tlv_get_is_decrypted =
					hal_rx_tlv_get_is_decrypted_be;
	hal_soc->ops->hal_rx_msdu_get_keyid = hal_rx_msdu_get_keyid_be;
	hal_soc->ops->hal_rx_tlv_get_freq = hal_rx_tlv_get_freq_be;
	hal_soc->ops->hal_rx_priv_info_set_in_tlv =
			hal_rx_priv_info_set_in_tlv_be;
	hal_soc->ops->hal_rx_priv_info_get_from_tlv =
			hal_rx_priv_info_get_from_tlv_be;
	hal_soc->ops->hal_rx_pkt_hdr_get = hal_rx_pkt_hdr_get_be;
	hal_soc->ops->hal_reo_setup = hal_reo_setup_9224;
	hal_soc->ops->hal_reo_config_reo2ppe_dest_info = NULL;
#ifdef REO_SHARED_QREF_TABLE_EN
	hal_soc->ops->hal_reo_shared_qaddr_setup = hal_reo_shared_qaddr_setup_be;
	hal_soc->ops->hal_reo_shared_qaddr_init = hal_reo_shared_qaddr_init_be;
	hal_soc->ops->hal_reo_shared_qaddr_detach = hal_reo_shared_qaddr_detach_be;
	hal_soc->ops->hal_reo_shared_qaddr_write = hal_reo_shared_qaddr_write_be;
	hal_soc->ops->hal_reo_shared_qaddr_cache_clear = hal_reo_shared_qaddr_cache_clear_be;
#endif
	/* Overwrite the default BE ops */
	hal_soc->ops->hal_get_rx_max_ba_window =
					hal_get_rx_max_ba_window_qcn9224;
	hal_soc->ops->hal_get_reo_qdesc_size = hal_qcn9224_get_reo_qdesc_size;
	/* TX MONITOR */
#ifdef QCA_MONITOR_2_0_SUPPORT
	hal_soc->ops->hal_txmon_is_mon_buf_addr_tlv =
				hal_txmon_is_mon_buf_addr_tlv_generic_be;
	hal_soc->ops->hal_txmon_populate_packet_info =
				hal_txmon_populate_packet_info_generic_be;
	hal_soc->ops->hal_txmon_status_parse_tlv =
				hal_txmon_status_parse_tlv_generic_be;
	hal_soc->ops->hal_txmon_status_get_num_users =
				hal_txmon_status_get_num_users_generic_be;
#endif /* QCA_MONITOR_2_0_SUPPORT */
	hal_soc->ops->hal_compute_reo_remap_ix0 = NULL;
	hal_soc->ops->hal_tx_vdev_mismatch_routing_set =
		hal_tx_vdev_mismatch_routing_set_generic_be;
	hal_soc->ops->hal_tx_mcast_mlo_reinject_routing_set =
		hal_tx_mcast_mlo_reinject_routing_set_generic_be;
	hal_soc->ops->hal_get_ba_aging_timeout =
		hal_get_ba_aging_timeout_be_generic;
	hal_soc->ops->hal_setup_link_idle_list =
		hal_setup_link_idle_list_generic_be;
	hal_soc->ops->hal_cookie_conversion_reg_cfg_be =
		hal_cookie_conversion_reg_cfg_generic_be;
	hal_soc->ops->hal_set_ba_aging_timeout =
		hal_set_ba_aging_timeout_be_generic;
	hal_soc->ops->hal_tx_populate_bank_register =
		hal_tx_populate_bank_register_be;
	hal_soc->ops->hal_tx_vdev_mcast_ctrl_set =
		hal_tx_vdev_mcast_ctrl_set_be;
#ifdef CONFIG_WORD_BASED_TLV
	hal_soc->ops->hal_rx_mpdu_start_wmask_get =
					hal_rx_mpdu_start_wmask_get_be;
	hal_soc->ops->hal_rx_msdu_end_wmask_get =
					hal_rx_msdu_end_wmask_get_be;
#endif
	hal_soc->ops->hal_get_tsf2_scratch_reg =
					hal_get_tsf2_scratch_reg_qcn9224;
	hal_soc->ops->hal_get_tqm_scratch_reg =
					hal_get_tqm_scratch_reg_qcn9224;
	hal_soc->ops->hal_tx_ring_halt_set = hal_tx_ppe2tcl_ring_halt_set_9224;
	hal_soc->ops->hal_tx_ring_halt_reset =
					hal_tx_ppe2tcl_ring_halt_reset_9224;
	hal_soc->ops->hal_tx_ring_halt_poll =
					hal_tx_ppe2tcl_ring_halt_done_9224;
	hal_soc->ops->hal_tx_get_num_ppe_vp_search_idx_tbl_entries =
			hal_tx_get_num_ppe_vp_search_idx_reg_entries_9224;
};

/**
 * hal_srng_hw_reg_offset_init_qcn9224() - Initialize the HW srng reg offset
 *				applicable only for QCN9224
 * @hal_soc: HAL Soc handle
 *
 * Return: None
 */
static inline void hal_srng_hw_reg_offset_init_qcn9224(struct hal_soc *hal_soc)
{
	int32_t *hw_reg_offset = hal_soc->hal_hw_reg_offset;

	hw_reg_offset[DST_MSI2_BASE_LSB] = REG_OFFSET(DST, MSI2_BASE_LSB),
	hw_reg_offset[DST_MSI2_BASE_MSB] = REG_OFFSET(DST, MSI2_BASE_MSB),
	hw_reg_offset[DST_MSI2_DATA] = REG_OFFSET(DST, MSI2_DATA),
	hw_reg_offset[DST_PRODUCER_INT2_SETUP] =
					REG_OFFSET(DST, PRODUCER_INT2_SETUP);
}

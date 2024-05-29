/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _HAL_API_MON_H_
#define _HAL_API_MON_H_

#include "qdf_types.h"
#include "hal_internal.h"
#include "hal_rx.h"
#include "hal_hw_headers.h"
#include <target_type.h>

#define HAL_RX_PHY_DATA_RADAR 0x01
#define HAL_SU_MU_CODING_LDPC 0x01

#define HAL_RX_FCS_LEN (4)
#define KEY_EXTIV 0x20

#define HAL_ALIGN(x, a)				HAL_ALIGN_MASK(x, (a)-1)
#define HAL_ALIGN_MASK(x, mask)	(typeof(x))(((uint32)(x) + (mask)) & ~(mask))

#define HAL_RX_TLV32_HDR_SIZE			4

#define HAL_RX_GET_USER_TLV32_TYPE(rx_status_tlv_ptr) \
		((*((uint32_t *)(rx_status_tlv_ptr)) & \
		HAL_RX_USER_TLV32_TYPE_MASK) >> \
		HAL_RX_USER_TLV32_TYPE_LSB)

#define HAL_RX_GET_USER_TLV32_LEN(rx_status_tlv_ptr) \
		((*((uint32_t *)(rx_status_tlv_ptr)) & \
		HAL_RX_USER_TLV32_LEN_MASK) >> \
		HAL_RX_USER_TLV32_LEN_LSB)

#define HAL_RX_GET_USER_TLV32_USERID(rx_status_tlv_ptr) \
		((*((uint32_t *)(rx_status_tlv_ptr)) & \
		HAL_RX_USER_TLV32_USERID_MASK) >> \
		HAL_RX_USER_TLV32_USERID_LSB)

#define HAL_RX_TLV64_HDR_SIZE			8

#define HAL_RX_GET_USER_TLV64_TYPE(rx_status_tlv_ptr) \
		((*((uint64_t *)(rx_status_tlv_ptr)) & \
		HAL_RX_USER_TLV64_TYPE_MASK) >> \
		HAL_RX_USER_TLV64_TYPE_LSB)

#define HAL_RX_GET_USER_TLV64_LEN(rx_status_tlv_ptr) \
		((*((uint64_t *)(rx_status_tlv_ptr)) & \
		HAL_RX_USER_TLV64_LEN_MASK) >> \
		HAL_RX_USER_TLV64_LEN_LSB)

#define HAL_RX_GET_USER_TLV64_USERID(rx_status_tlv_ptr) \
		((*((uint64_t *)(rx_status_tlv_ptr)) & \
		HAL_RX_USER_TLV64_USERID_MASK) >> \
		HAL_RX_USER_TLV64_USERID_LSB)

#define HAL_TLV_STATUS_PPDU_NOT_DONE 0
#define HAL_TLV_STATUS_PPDU_DONE 1
#define HAL_TLV_STATUS_BUF_DONE 2
#define HAL_TLV_STATUS_PPDU_NON_STD_DONE 3
#define HAL_TLV_STATUS_PPDU_START 4
#define HAL_TLV_STATUS_HEADER 5
#define HAL_TLV_STATUS_MPDU_END 6
#define HAL_TLV_STATUS_MSDU_START 7
#define HAL_TLV_STATUS_MSDU_END 8
#define HAL_TLV_STATUS_MON_BUF_ADDR 9
#define HAL_TLV_STATUS_MPDU_START 10
#define HAL_TLV_STATUS_MON_DROP 11

#define HAL_MAX_UL_MU_USERS	37

#define HAL_RX_PKT_TYPE_11A	0
#define HAL_RX_PKT_TYPE_11B	1
#define HAL_RX_PKT_TYPE_11N	2
#define HAL_RX_PKT_TYPE_11AC	3
#define HAL_RX_PKT_TYPE_11AX	4
#ifdef WLAN_FEATURE_11BE
#define HAL_RX_PKT_TYPE_11BE	6
#endif

#define HAL_RX_RECEPTION_TYPE_SU	0
#define HAL_RX_RECEPTION_TYPE_MU_MIMO	1
#define HAL_RX_RECEPTION_TYPE_OFDMA	2
#define HAL_RX_RECEPTION_TYPE_MU_OFDMA	3

/* Multiply rate by 2 to avoid float point
 * and get rate in units of 500kbps
 */
#define HAL_11B_RATE_0MCS	11*2
#define HAL_11B_RATE_1MCS	5.5*2
#define HAL_11B_RATE_2MCS	2*2
#define HAL_11B_RATE_3MCS	1*2
#define HAL_11B_RATE_4MCS	11*2
#define HAL_11B_RATE_5MCS	5.5*2
#define HAL_11B_RATE_6MCS	2*2

#define HAL_11A_RATE_0MCS	48*2
#define HAL_11A_RATE_1MCS	24*2
#define HAL_11A_RATE_2MCS	12*2
#define HAL_11A_RATE_3MCS	6*2
#define HAL_11A_RATE_4MCS	54*2
#define HAL_11A_RATE_5MCS	36*2
#define HAL_11A_RATE_6MCS	18*2
#define HAL_11A_RATE_7MCS	9*2

#define HAL_LEGACY_MCS0  0
#define HAL_LEGACY_MCS1  1
#define HAL_LEGACY_MCS2  2
#define HAL_LEGACY_MCS3  3
#define HAL_LEGACY_MCS4  4
#define HAL_LEGACY_MCS5  5
#define HAL_LEGACY_MCS6  6
#define HAL_LEGACY_MCS7  7

#define HE_GI_0_8 0
#define HE_GI_0_4 1
#define HE_GI_1_6 2
#define HE_GI_3_2 3

#define HE_GI_RADIOTAP_0_8 0
#define HE_GI_RADIOTAP_1_6 1
#define HE_GI_RADIOTAP_3_2 2
#define HE_GI_RADIOTAP_RESERVED 3

#define HE_LTF_RADIOTAP_UNKNOWN 0
#define HE_LTF_RADIOTAP_1_X 1
#define HE_LTF_RADIOTAP_2_X 2
#define HE_LTF_RADIOTAP_4_X 3

#define HT_SGI_PRESENT 0x80

#define HE_LTF_1_X 0
#define HE_LTF_2_X 1
#define HE_LTF_4_X 2
#define HE_LTF_UNKNOWN 3
#define VHT_SIG_SU_NSS_MASK	0x7
#define HT_SIG_SU_NSS_SHIFT	0x3

#define HAL_TID_INVALID 31
#define HAL_AST_IDX_INVALID 0xFFFF

#ifdef GET_MSDU_AGGREGATION
#define HAL_RX_GET_MSDU_AGGREGATION(rx_desc, rs)\
{\
	struct rx_msdu_end *rx_msdu_end;\
	bool first_msdu, last_msdu; \
	rx_msdu_end = &rx_desc->msdu_end_tlv.rx_msdu_end;\
	first_msdu = HAL_RX_GET(rx_msdu_end, RX_MSDU_END_5, FIRST_MSDU);\
	last_msdu = HAL_RX_GET(rx_msdu_end, RX_MSDU_END_5, LAST_MSDU);\
	if (first_msdu && last_msdu)\
		rs->rs_flags &= (~IEEE80211_AMSDU_FLAG);\
	else\
		rs->rs_flags |= (IEEE80211_AMSDU_FLAG); \
} \

#define HAL_RX_SET_MSDU_AGGREGATION((rs_mpdu), (rs_ppdu))\
{\
	if (rs_mpdu->rs_flags & IEEE80211_AMSDU_FLAG)\
		rs_ppdu->rs_flags |= IEEE80211_AMSDU_FLAG;\
} \

#else
#define HAL_RX_GET_MSDU_AGGREGATION(rx_desc, rs)
#define HAL_RX_SET_MSDU_AGGREGATION(rs_mpdu, rs_ppdu)
#endif

/* Max MPDUs per status buffer */
#define HAL_RX_MAX_MPDU 256
#define HAL_RX_NUM_WORDS_PER_PPDU_BITMAP (HAL_RX_MAX_MPDU >> 5)
#define HAL_RX_MAX_MPDU_H_PER_STATUS_BUFFER 16

/* Max pilot count */
#define HAL_RX_MAX_SU_EVM_COUNT 32

#define HAL_RX_FRAMECTRL_TYPE_MASK 0x0C
#define HAL_RX_GET_FRAME_CTRL_TYPE(fc)\
		(((fc) & HAL_RX_FRAMECTRL_TYPE_MASK) >> 2)
#define HAL_RX_FRAME_CTRL_TYPE_MGMT 0x0
#define HAL_RX_FRAME_CTRL_TYPE_CTRL 0x1
#define HAL_RX_FRAME_CTRL_TYPE_DATA 0x2

/**
 * hal_dl_ul_flag - flag to indicate UL/DL
 * @dl_ul_flag_is_dl_or_tdls: DL
 * @dl_ul_flag_is_ul: UL
 */
enum hal_dl_ul_flag {
	dl_ul_flag_is_dl_or_tdls,
	dl_ul_flag_is_ul,
};

/*
 * hal_eht_ppdu_sig_cmn_type - PPDU type
 * @eht_ppdu_sig_tb_or_dl_ofdma: TB/DL_OFDMA PPDU
 * @eht_ppdu_sig_su: SU PPDU
 * @eht_ppdu_sig_dl_mu_mimo: DL_MU_MIMO PPDU
 */
enum hal_eht_ppdu_sig_cmn_type {
	eht_ppdu_sig_tb_or_dl_ofdma,
	eht_ppdu_sig_su,
	eht_ppdu_sig_dl_mu_mimo,
};

/*
 * hal_mon_packet_info - packet info
 * @sw_cookie: 64-bit SW desc virtual address
 * @dma_length: packet DMA length
 * @msdu_continuation: msdu continulation in next buffer
 * @truncated: packet is truncated
 */
struct hal_mon_packet_info {
	uint64_t sw_cookie;
	uint32_t dma_length : 16,
		 msdu_continuation : 1,
		 truncated : 1;
};

/*
 * hal_rx_mon_msdu_info - msdu info
 * @first_buffer: first buffer of msdu
 * @last_buffer: last buffer of msdu
 * @first_mpdu: first MPDU
 * @mpdu_length_err: MPDU length error
 * @fcs_err: FCS error
 * @first_msdu: first msdu
 * @decap_type: decap type
 * @last_msdu: last msdu
 * @l3_header_padding: L3 padding header
 * @stbc: stbc enabled
 * @sgi: SGI value
 * @reception_type: reception type
 * @msdu_index: msdu index
 * @buffer_len: buffer len
 * @frag_len: frag len
 * @msdu_len: msdu len
 * @user_rssi: user rssi
 */
struct hal_rx_mon_msdu_info {
	uint32_t first_buffer : 1,
		 last_buffer : 1,
		 first_mpdu : 1,
		 mpdu_length_err : 1,
		 fcs_err : 1,
		 first_msdu : 1,
		 decap_type : 3,
		 last_msdu : 1,
		 l3_header_padding : 3,
		 stbc : 1,
		 sgi : 2,
		 reception_type : 3,
		 msdu_index : 4;
	uint16_t buffer_len : 12;
	uint16_t frag_len : 12;
	uint16_t msdu_len;
	int16_t user_rssi;
};

/*
 * hal_rx_mon_mpdu_info - MPDU info
 * @decap_type: decap_type
 * @mpdu_length_err: MPDU length error
 * @fcs_err: FCS error
 * @overflow_err: overflow error
 * @decrypt_err: decrypt error
 * @mpdu_start_received: MPDU start received
 * @full_pkt: Full MPDU received
 * @first_rx_hdr_rcvd: First rx_hdr received
 * @truncated: truncated MPDU
 */
struct hal_rx_mon_mpdu_info {
	uint32_t decap_type : 8,
		 mpdu_length_err : 1,
		 fcs_err : 1,
		 overflow_err : 1,
		 decrypt_err : 1,
		 mpdu_start_received : 1,
		 full_pkt : 1,
		 first_rx_hdr_rcvd : 1,
		 truncated : 1;
};

/**
 * struct hal_rx_mon_desc_info () - HAL Rx Monitor descriptor info
 *
 * @ppdu_id:                 PHY ppdu id
 * @status_ppdu_id:          status PHY ppdu id
 * @status_buf_count:        number of status buffer count
 * @rxdma_push_reason:       rxdma push reason
 * @rxdma_error_code:        rxdma error code
 * @msdu_cnt:                msdu count
 * @end_of_ppdu:             end of ppdu
 * @link_desc:               msdu link descriptor address
 * @status_buf:              for a PPDU, status buffers can span acrosss
 *                           multiple buffers, status_buf points to first
 *                           status buffer address of PPDU
 * @drop_ppdu:               flag to indicate current destination
 *                           ring ppdu drop
 */
struct hal_rx_mon_desc_info {
	uint16_t ppdu_id;
	uint16_t status_ppdu_id;
	uint8_t status_buf_count;
	uint8_t rxdma_push_reason;
	uint8_t rxdma_error_code;
	uint8_t msdu_count;
	uint8_t end_of_ppdu;
	struct hal_buf_info link_desc;
	struct hal_buf_info status_buf;
	bool drop_ppdu;
};

/*
 * Struct hal_rx_su_evm_info - SU evm info
 * @number_of_symbols: number of symbols
 * @nss_count:         nss count
 * @pilot_count:       pilot count
 * @pilot_evm:         Array of pilot evm values
 */
struct hal_rx_su_evm_info {
	uint32_t number_of_symbols;
	uint8_t  nss_count;
	uint8_t  pilot_count;
	uint32_t pilot_evm[HAL_RX_MAX_SU_EVM_COUNT];
};

enum {
	DP_PPDU_STATUS_START,
	DP_PPDU_STATUS_DONE,
};

/**
 * struct hal_rx_ppdu_drop_cnt - PPDU drop count
 * @ppdu_drop_cnt: PPDU drop count
 * @mpdu_drop_cnt: MPDU drop count
 * @end_of_ppdu_drop_cnt: End of PPDU drop count
 * @tlv_drop_cnt: TLV drop count
 */
struct hal_rx_ppdu_drop_cnt {
	uint8_t ppdu_drop_cnt;
	uint16_t mpdu_drop_cnt;
	uint8_t end_of_ppdu_drop_cnt;
	uint16_t tlv_drop_cnt;
};

static inline QDF_STATUS
hal_rx_reo_ent_get_src_link_id(hal_soc_handle_t hal_soc_hdl,
			       hal_rxdma_desc_t rx_desc,
			       uint8_t *src_link_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (!hal_soc || !hal_soc->ops) {
		hal_err("hal handle is NULL");
		QDF_BUG(0);
		return QDF_STATUS_E_INVAL;
	}

	if (hal_soc->ops->hal_rx_reo_ent_get_src_link_id)
		return hal_soc->ops->hal_rx_reo_ent_get_src_link_id(rx_desc,
								    src_link_id);

	return QDF_STATUS_E_INVAL;
}

/**
 * hal_rx_reo_ent_buf_paddr_get: Gets the physical address and
 *			cookie from the REO entrance ring element
 * @hal_rx_desc_cookie: Opaque cookie pointer used by HAL to get to
 * the current descriptor
 * @ buf_info: structure to return the buffer information
 * @ msdu_cnt: pointer to msdu count in MPDU
 *
 * CAUTION: This API calls a hal_soc ops, so be careful before calling this in
 * per packet path
 *
 * Return: void
 */
static inline
void hal_rx_reo_ent_buf_paddr_get(hal_soc_handle_t hal_soc_hdl,
				  hal_rxdma_desc_t rx_desc,
				  struct hal_buf_info *buf_info,
				  uint32_t *msdu_cnt)
{
	struct reo_entrance_ring *reo_ent_ring =
		(struct reo_entrance_ring *)rx_desc;
	struct buffer_addr_info *buf_addr_info;
	struct rx_mpdu_desc_info *rx_mpdu_desc_info_details;
	uint32_t loop_cnt;

	rx_mpdu_desc_info_details =
	&reo_ent_ring->reo_level_mpdu_frame_info.rx_mpdu_desc_info_details;

	*msdu_cnt = HAL_RX_GET(rx_mpdu_desc_info_details,
				HAL_RX_MPDU_DESC_INFO, MSDU_COUNT);

	loop_cnt = HAL_RX_GET(reo_ent_ring, HAL_REO_ENTRANCE_RING,
			      LOOPING_COUNT);

	buf_addr_info =
	&reo_ent_ring->reo_level_mpdu_frame_info.msdu_link_desc_addr_info;

	hal_rx_buf_cookie_rbm_get(hal_soc_hdl, (uint32_t *)buf_addr_info,
				  buf_info);
	buf_info->paddr =
		(HAL_RX_BUFFER_ADDR_31_0_GET(buf_addr_info) |
		((uint64_t)
		(HAL_RX_BUFFER_ADDR_39_32_GET(buf_addr_info)) << 32));

	dp_nofl_debug("[%s][%d] ReoAddr=%pK, addrInfo=%pK, paddr=0x%llx, loopcnt=%d",
		      __func__, __LINE__, reo_ent_ring, buf_addr_info,
	(unsigned long long)buf_info->paddr, loop_cnt);
}

static inline
void hal_rx_mon_next_link_desc_get(hal_soc_handle_t hal_soc_hdl,
				   void *rx_msdu_link_desc,
				   struct hal_buf_info *buf_info)
{
	struct rx_msdu_link *msdu_link =
		(struct rx_msdu_link *)rx_msdu_link_desc;
	struct buffer_addr_info *buf_addr_info;

	buf_addr_info = &msdu_link->next_msdu_link_desc_addr_info;

	hal_rx_buf_cookie_rbm_get(hal_soc_hdl, (uint32_t *)buf_addr_info,
				  buf_info);

	buf_info->paddr =
		(HAL_RX_BUFFER_ADDR_31_0_GET(buf_addr_info) |
		((uint64_t)
		(HAL_RX_BUFFER_ADDR_39_32_GET(buf_addr_info)) << 32));
}

static inline
uint8_t *HAL_RX_MON_DEST_GET_DESC(uint8_t *data)
{
	return data;
}

static inline uint32_t
hal_rx_tlv_mpdu_len_err_get(hal_soc_handle_t hal_soc_hdl, void *hw_desc_addr)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (!hal_soc || !hal_soc->ops) {
		hal_err("hal handle is NULL");
		QDF_BUG(0);
		return 0;
	}

	if (hal_soc->ops->hal_rx_tlv_mpdu_len_err_get)
		return hal_soc->ops->hal_rx_tlv_mpdu_len_err_get(hw_desc_addr);

	return 0;
}

static inline uint32_t
hal_rx_tlv_mpdu_fcs_err_get(hal_soc_handle_t hal_soc_hdl, void *hw_desc_addr)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (!hal_soc || !hal_soc->ops) {
		hal_err("hal handle is NULL");
		QDF_BUG(0);
		return 0;
	}

	if (hal_soc->ops->hal_rx_tlv_mpdu_fcs_err_get)
		return hal_soc->ops->hal_rx_tlv_mpdu_fcs_err_get(hw_desc_addr);

	return 0;
}

#ifdef notyet
/*
 * HAL_RX_HW_DESC_MPDU_VALID() - check MPDU start TLV tag in MPDU
 *			start TLV of Hardware TLV descriptor
 * @hw_desc_addr: Hardware descriptor address
 *
 * Return: bool: if TLV tag match
 */
static inline
bool HAL_RX_HW_DESC_MPDU_VALID(void *hw_desc_addr)
{
	struct rx_mon_pkt_tlvs *rx_desc =
		(struct rx_mon_pkt_tlvs *)hw_desc_addr;
	uint32_t tlv_tag;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(&rx_desc->mpdu_start_tlv);

	return tlv_tag == WIFIRX_MPDU_START_E ? true : false;
}
#endif

/*
 * HAL_RX_HW_DESC_MPDU_VALID() - check MPDU start TLV user id in MPDU
 *			start TLV of Hardware TLV descriptor
 * @hw_desc_addr: Hardware descriptor address
 *
 * Return: unit32_t: user id
 */
static inline uint32_t
hal_rx_hw_desc_mpdu_user_id(hal_soc_handle_t hal_soc_hdl,
			    void *hw_desc_addr)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (!hal_soc || !hal_soc->ops) {
		hal_err("hal handle is NULL");
		QDF_BUG(0);
		return 0;
	}

	if (hal_soc->ops->hal_rx_hw_desc_mpdu_user_id)
		return hal_soc->ops->hal_rx_hw_desc_mpdu_user_id(hw_desc_addr);

	return 0;
}

/* TODO: Move all Rx descriptor functions to hal_rx.h to avoid duplication */

/**
 * hal_rx_msdu_link_desc_set: Retrieves MSDU Link Descriptor to WBM
 *
 * @ soc		: HAL version of the SOC pointer
 * @ src_srng_desc	: void pointer to the WBM Release Ring descriptor
 * @ buf_addr_info	: void pointer to the buffer_addr_info
 *
 * Return: void
 */
static inline
void hal_rx_mon_msdu_link_desc_set(hal_soc_handle_t hal_soc_hdl,
				   void *src_srng_desc,
				   hal_buff_addrinfo_t buf_addr_info)
{
	struct buffer_addr_info *wbm_srng_buffer_addr_info =
			(struct buffer_addr_info *)src_srng_desc;
	uint64_t paddr;
	struct buffer_addr_info *p_buffer_addr_info =
			(struct buffer_addr_info *)buf_addr_info;

	paddr =
		(HAL_RX_BUFFER_ADDR_31_0_GET(buf_addr_info) |
		((uint64_t)
		(HAL_RX_BUFFER_ADDR_39_32_GET(buf_addr_info)) << 32));

	dp_nofl_debug("[%s][%d] src_srng_desc=%pK, buf_addr=0x%llx, cookie=0x%llx",
		      __func__, __LINE__, src_srng_desc, (unsigned long long)paddr,
		      (unsigned long long)p_buffer_addr_info->sw_buffer_cookie);

	/* Structure copy !!! */
	*wbm_srng_buffer_addr_info =
		*((struct buffer_addr_info *)buf_addr_info);
}

/**
 * hal_get_rx_msdu_link_desc_size() - Get msdu link descriptor size
 *
 * Return: size of rx_msdu_link
 */
static inline
uint32_t hal_get_rx_msdu_link_desc_size(void)
{
	return sizeof(struct rx_msdu_link);
}

enum {
	HAL_PKT_TYPE_OFDM = 0,
	HAL_PKT_TYPE_CCK,
	HAL_PKT_TYPE_HT,
	HAL_PKT_TYPE_VHT,
	HAL_PKT_TYPE_HE,
};

enum {
	HAL_SGI_0_8_US,
	HAL_SGI_0_4_US,
	HAL_SGI_1_6_US,
	HAL_SGI_3_2_US,
};

#ifdef WLAN_FEATURE_11BE
enum {
	HAL_FULL_RX_BW_20,
	HAL_FULL_RX_BW_40,
	HAL_FULL_RX_BW_80,
	HAL_FULL_RX_BW_160,
	HAL_FULL_RX_BW_320,
};
#else
enum {
	HAL_FULL_RX_BW_20,
	HAL_FULL_RX_BW_40,
	HAL_FULL_RX_BW_80,
	HAL_FULL_RX_BW_160,
};
#endif

enum {
	HAL_RX_TYPE_SU,
	HAL_RX_TYPE_MU_MIMO,
	HAL_RX_TYPE_MU_OFDMA,
	HAL_RX_TYPE_MU_OFDMA_MIMO,
};

enum {
	HAL_RX_TYPE_DL,
	HAL_RX_TYPE_UL,
};

/*
 * enum
 * @HAL_RECEPTION_TYPE_SU: Basic SU reception
 * @HAL_RECEPTION_TYPE_DL_MU_MIMO: DL MU_MIMO reception
 * @HAL_RECEPTION_TYPE_DL_MU_OFMA: DL MU_OFMA reception
 * @HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO: DL MU_OFDMA_MIMO reception
 * @HAL_RECEPTION_TYPE_UL_MU_MIMO: UL MU_MIMO reception
 * @HAL_RECEPTION_TYPE_UL_MU_OFDMA: UL MU_OFMA reception
 * @HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO: UL MU_OFDMA_MIMO reception
 */
enum {
	HAL_RECEPTION_TYPE_SU,
	HAL_RECEPTION_TYPE_DL_MU_MIMO,
	HAL_RECEPTION_TYPE_DL_MU_OFMA,
	HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO,
	HAL_RECEPTION_TYPE_UL_MU_MIMO,
	HAL_RECEPTION_TYPE_UL_MU_OFDMA,
	HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO
};

/**
 * enum
 * @HAL_RX_MON_PPDU_START: PPDU start TLV is decoded in HAL
 * @HAL_RX_MON_PPDU_END: PPDU end TLV is decoded in HAL
 * @HAL_RX_MON_PPDU_RESET: Not PPDU start and end TLV
 */
enum {
	HAL_RX_MON_PPDU_START = 0,
	HAL_RX_MON_PPDU_END,
	HAL_RX_MON_PPDU_RESET,
};

/* struct hal_rx_ppdu_common_info  - common ppdu info
 * @ppdu_id - ppdu id number
 * @ppdu_timestamp - timestamp at ppdu received
 * @mpdu_cnt_fcs_ok - mpdu count in ppdu with fcs ok
 * @mpdu_cnt_fcs_err - mpdu count in ppdu with fcs err
 * @mpdu_fcs_ok_bitmap - fcs ok mpdu count in ppdu bitmap
 * @last_ppdu_id - last received ppdu id
 * @mpdu_cnt - total mpdu count
 * @num_users - num users
 */
struct hal_rx_ppdu_common_info {
	uint32_t ppdu_id;
	uint64_t ppdu_timestamp;
	uint16_t mpdu_cnt_fcs_ok;
	uint8_t mpdu_cnt_fcs_err;
	uint8_t num_users;
	uint32_t mpdu_fcs_ok_bitmap[HAL_RX_NUM_WORDS_PER_PPDU_BITMAP];
	uint32_t last_ppdu_id;
	uint16_t mpdu_cnt;
};

/**
 * struct hal_rx_msdu_payload_info - msdu payload info
 * @first_msdu_payload: pointer to first msdu payload
 * @payload_len: payload len
 */
struct hal_rx_msdu_payload_info {
	uint8_t *first_msdu_payload;
	uint8_t payload_len;
};

/**
 * struct hal_rx_nac_info - struct for neighbour info
 * @fc_valid: flag indicate if it has valid frame control information
 * @frame_control: frame control from each MPDU
 * @to_ds_flag: flag indicate to_ds bit
 * @mac_addr2_valid: flag indicate if mac_addr2 is valid
 * @mcast_bcast: multicast/broadcast
 * @mac_addr2: mac address2 in wh
 */
struct hal_rx_nac_info {
	uint32_t fc_valid : 1,
		 frame_control : 16,
		 to_ds_flag : 1,
		 mac_addr2_valid : 1,
		 mcast_bcast : 1;
	uint8_t mac_addr2[QDF_MAC_ADDR_SIZE];
};

/**
 * struct hal_rx_ppdu_msdu_info - struct for msdu info from HW TLVs
 * @fse_metadata: cached FSE metadata value received in the MSDU END TLV
 * @cce_metadata: cached CCE metadata value received in the MSDU_END TLV
 * @is_flow_idx_timeout: flag to indicate if flow search timeout occurred
 * @is_flow_idx_invalid: flag to indicate if flow idx is valid or not
 * @flow_idx: flow idx matched in FSE received in the MSDU END TLV
 */
struct hal_rx_ppdu_msdu_info {
	uint32_t fse_metadata;
	uint32_t cce_metadata : 16,
		 is_flow_idx_timeout : 1,
		 is_flow_idx_invalid : 1;
	uint32_t flow_idx : 20;
};

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
/**
 * struct hal_rx_ppdu_cfr_user_info - struct for storing peer info extracted
 * from HW TLVs, this will be used for correlating CFR data with multiple peers
 * in MU PPDUs
 *
 * @peer_macaddr: macaddr of the peer
 * @ast_index: AST index of the peer
 */
struct hal_rx_ppdu_cfr_user_info {
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	uint16_t ast_index;
};

/**
 * struct hal_rx_ppdu_cfr_info - struct for storing ppdu info extracted from HW
 * TLVs, this will be used for CFR correlation
 *
 * @bb_captured_channel : Set by RXPCU when MACRX_FREEZE_CAPTURE_CHANNEL TLV is
 * sent to PHY, SW checks it to correlate current PPDU TLVs with uploaded
 * channel information.
 *
 * @bb_captured_timeout : Set by RxPCU to indicate channel capture condition is
 * met, but MACRX_FREEZE_CAPTURE_CHANNEL is not sent to PHY due to AST delay,
 * which means the rx_frame_falling edge to FREEZE TLV ready time exceeds
 * the threshold time defined by RXPCU register FREEZE_TLV_DELAY_CNT_THRESH.
 * Bb_captured_reason is still valid in this case.
 *
 * @rx_location_info_valid: Indicates whether CFR DMA address in the PPDU TLV
 * is valid
 * <enum 0 rx_location_info_is_not_valid>
 * <enum 1 rx_location_info_is_valid>
 * <legal all>
 *
 * @bb_captured_reason : Copy capture_reason of MACRX_FREEZE_CAPTURE_CHANNEL
 * TLV to here for FW usage. Valid when bb_captured_channel or
 * bb_captured_timeout is set.
 * <enum 0 freeze_reason_TM>
 * <enum 1 freeze_reason_FTM>
 * <enum 2 freeze_reason_ACK_resp_to_TM_FTM>
 * <enum 3 freeze_reason_TA_RA_TYPE_FILTER>
 * <enum 4 freeze_reason_NDPA_NDP>
 * <enum 5 freeze_reason_ALL_PACKET>
 * <legal 0-5>
 *
 * @rtt_che_buffer_pointer_low32 : The low 32 bits of the 40 bits pointer to
 * external RTT channel information buffer
 *
 * @rtt_che_buffer_pointer_high8 : The high 8 bits of the 40 bits pointer to
 * external RTT channel information buffer
 *
 * @chan_capture_status : capture status reported by ucode
 * a. CAPTURE_IDLE: FW has disabled "REPETITIVE_CHE_CAPTURE_CTRL"
 * b. CAPTURE_BUSY: previous PPDU’s channel capture upload DMA ongoing. (Note
 * that this upload is triggered after receiving freeze_channel_capture TLV
 * after last PPDU is rx)
 * c. CAPTURE_ACTIVE: channel capture is enabled and no previous channel
 * capture ongoing
 * d. CAPTURE_NO_BUFFER: next buffer in IPC ring not available
 *
 * @cfr_user_info: Peer mac for upto 4 MU users
 *
 * @rtt_cfo_measurement : raw cfo data extracted from hardware, which is 14 bit
 * signed number. The first bit used for sign representation and 13 bits for
 * fractional part.
 *
 * @agc_gain_info0: Chain 0 & chain 1 agc gain information reported by PHY
 *
 * @agc_gain_info1: Chain 2 & chain 3 agc gain information reported by PHY
 *
 * @agc_gain_info2: Chain 4 & chain 5 agc gain information reported by PHY
 *
 * @agc_gain_info3: Chain 6 & chain 7 agc gain information reported by PHY
 *
 * @rx_start_ts: Rx packet timestamp, the time the first L-STF ADC sample
 * arrived at Rx antenna.
 *
 * @mcs_rate: Indicates the mcs/rate in which packet is received.
 * If HT,
 *    0-7: MCS0-MCS7
 * If VHT,
 *    0-9: MCS0 to MCS9
 * If HE,
 *    0-11: MCS0 to MCS11,
 *    12-13: 4096QAM,
 *    14-15: reserved
 * If Legacy,
 *    0: 48 Mbps
 *    1: 24 Mbps
 *    2: 12 Mbps
 *    3: 6 Mbps
 *    4: 54 Mbps
 *    5: 36 Mbps
 *    6: 18 Mbps
 *    7: 9 Mbps
 *
 * @gi_type: Indicates the guard interval.
 *    0: 0.8 us
 *    1: 0.4 us
 *    2: 1.6 us
 *    3: 3.2 us
 */
struct hal_rx_ppdu_cfr_info {
	bool bb_captured_channel;
	bool bb_captured_timeout;
	uint8_t bb_captured_reason;
	bool rx_location_info_valid;
	uint8_t chan_capture_status;
	uint8_t rtt_che_buffer_pointer_high8;
	uint32_t rtt_che_buffer_pointer_low32;
	int16_t rtt_cfo_measurement;
	uint32_t agc_gain_info0;
	uint32_t agc_gain_info1;
	uint32_t agc_gain_info2;
	uint32_t agc_gain_info3;
	uint32_t rx_start_ts;
	uint32_t mcs_rate;
	uint32_t gi_type;
};
#else
struct hal_rx_ppdu_cfr_info {};
#endif

struct mon_rx_info {
	uint8_t  qos_control_info_valid;
	uint16_t qos_control;
	uint8_t mac_addr1_valid;
	uint8_t mac_addr1[QDF_MAC_ADDR_SIZE];
	uint16_t user_id;
};

struct mon_rx_user_info {
	uint16_t qos_control;
	uint8_t qos_control_info_valid;
};

#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
struct hal_rx_frm_type_info {
	uint8_t rx_mgmt_cnt;
	uint8_t rx_ctrl_cnt;
	uint8_t rx_data_cnt;
};
#else
struct hal_rx_frm_type_info {};
#endif

struct hal_mon_usig_cmn {
	uint32_t phy_version : 3,
		 bw : 3,
		 ul_dl : 1,
		 bss_color : 6,
		 txop : 7,
		 disregard : 5,
		 validate_0 : 1,
		 reserved : 6;
};

struct hal_mon_usig_tb {
	uint32_t ppdu_type_comp_mode : 2,
		 validate_1 : 1,
		 spatial_reuse_1 : 4,
		 spatial_reuse_2 : 4,
		 disregard_1 : 5,
		 crc : 4,
		 tail : 6,
		 reserved : 5,
		 rx_integrity_check_passed : 1;
};

struct hal_mon_usig_mu {
	uint32_t ppdu_type_comp_mode : 2,
		 validate_1 : 1,
		 punc_ch_info : 5,
		 validate_2 : 1,
		 eht_sig_mcs : 2,
		 num_eht_sig_sym : 5,
		 crc : 4,
		 tail : 6,
		 reserved : 5,
		 rx_integrity_check_passed : 1;
};

/**
 * union hal_mon_usig_non_cmn: Version dependent USIG fields
 * @tb: trigger based frame USIG header
 * @mu: MU frame USIG header
 */
union hal_mon_usig_non_cmn {
	struct hal_mon_usig_tb tb;
	struct hal_mon_usig_mu mu;
};

/**
 * struct hal_mon_usig_hdr: U-SIG header for EHT (and subsequent) frames
 * @usig_1: USIG common header fields
 * @usig_2: USIG version dependent fields
 */
struct hal_mon_usig_hdr {
	struct hal_mon_usig_cmn usig_1;
	union hal_mon_usig_non_cmn usig_2;
};

#define HAL_RX_MON_USIG_PPDU_TYPE_N_COMP_MODE_MASK	0x0000000300000000
#define HAL_RX_MON_USIG_PPDU_TYPE_N_COMP_MODE_LSB	32

#define HAL_RX_MON_USIG_GET_PPDU_TYPE_N_COMP_MODE(usig_tlv_ptr)	\
		((*((uint64_t *)(usig_tlv_ptr)) & \
		 HAL_RX_MON_USIG_PPDU_TYPE_N_COMP_MODE_MASK) >> \
		 HAL_RX_MON_USIG_PPDU_TYPE_N_COMP_MODE_LSB)

#define HAL_RX_MON_USIG_RX_INTEGRITY_CHECK_PASSED_MASK	0x8000000000000000
#define HAL_RX_MON_USIG_RX_INTEGRITY_CHECK_PASSED_LSB	63

#define HAL_RX_MON_USIG_GET_RX_INTEGRITY_CHECK_PASSED(usig_tlv_ptr)	\
		((*((uint64_t *)(usig_tlv_ptr)) & \
		 HAL_RX_MON_USIG_RX_INTEGRITY_CHECK_PASSED_MASK) >> \
		 HAL_RX_MON_USIG_RX_INTEGRITY_CHECK_PASSED_LSB)

/**
 * enum hal_eht_bw: Reception bandwidth
 * @HAL_EHT_BW_20: 20Mhz
 * @HAL_EHT_BW_40: 40Mhz
 * @HAL_EHT_BW_80: 80Mhz
 * @HAL_EHT_BW_160: 160Mhz
 * @HAL_EHT_BW_320_1: 320_1 band
 * @HAL_EHT_BW_320_2: 320_2 band
 */
enum hal_eht_bw {
	HAL_EHT_BW_20 = 0,
	HAL_EHT_BW_40,
	HAL_EHT_BW_80,
	HAL_EHT_BW_160,
	HAL_EHT_BW_320_1,
	HAL_EHT_BW_320_2,
};

struct hal_eht_sig_mu_mimo_user_info {
	uint32_t sta_id : 11,
		 mcs : 4,
		 coding : 1,
		 spatial_coding : 6,
		 crc : 4;
};

struct hal_eht_sig_non_mu_mimo_user_info {
	uint32_t sta_id : 11,
		 mcs : 4,
		 validate : 1,
		 nss : 4,
		 beamformed : 1,
		 coding : 1,
		 crc : 4;
};

/**
 * union hal_eht_sig_user_field: User field in EHTSIG
 * @mu_mimo_usr: MU-MIMO user field information in EHTSIG
 * @non_mu_mimo_usr: Non MU-MIMO user field information in EHTSIG
 */
union hal_eht_sig_user_field {
	struct hal_eht_sig_mu_mimo_user_info mu_mimo_usr;
	struct hal_eht_sig_non_mu_mimo_user_info non_mu_mimo_usr;
};

struct hal_eht_sig_ofdma_cmn_eb1 {
	uint64_t spatial_reuse : 4,
		 gi_ltf : 2,
		 num_ltf_sym : 3,
		 ldpc_extra_sym : 1,
		 pre_fec_pad_factor : 2,
		 pe_disambiguity : 1,
		 disregard : 4,
		 ru_allocation1_1 : 9,
		 ru_allocation1_2 : 9,
		 crc : 4;
};

struct hal_eht_sig_ofdma_cmn_eb2 {
	uint64_t ru_allocation2_1 : 9,
		 ru_allocation2_2 : 9,
		 ru_allocation2_3 : 9,
		 ru_allocation2_4 : 9,
		 ru_allocation2_5 : 9,
		 ru_allocation2_6 : 9,
		 crc : 4;
};

struct hal_eht_sig_cc_usig_overflow {
	uint32_t spatial_reuse : 4,
		 gi_ltf : 2,
		 num_ltf_sym : 3,
		 ldpc_extra_sym : 1,
		 pre_fec_pad_factor : 2,
		 pe_disambiguity : 1,
		 disregard : 4;
};

struct hal_eht_sig_non_ofdma_cmn_eb {
	uint32_t spatial_reuse : 4,
		 gi_ltf : 2,
		 num_ltf_sym : 3,
		 ldpc_extra_sym : 1,
		 pre_fec_pad_factor : 2,
		 pe_disambiguity : 1,
		 disregard : 4,
		 num_users : 3;
	union hal_eht_sig_user_field user_field;
};

struct hal_eht_sig_ndp_cmn_eb {
	uint32_t spatial_reuse : 4,
		 gi_ltf : 2,
		 num_ltf_sym : 3,
		 nss : 4,
		 beamformed : 1,
		 disregard : 2,
		 crc : 4;
};

/* Different allowed RU in 11BE */
#define HAL_EHT_RU_26		0ULL
#define HAL_EHT_RU_52		1ULL
#define HAL_EHT_RU_78		2ULL
#define HAL_EHT_RU_106		3ULL
#define HAL_EHT_RU_132		4ULL
#define HAL_EHT_RU_242		5ULL
#define HAL_EHT_RU_484		6ULL
#define HAL_EHT_RU_726		7ULL
#define HAL_EHT_RU_996		8ULL
#define HAL_EHT_RU_996x2	9ULL
#define HAL_EHT_RU_996x3	10ULL
#define HAL_EHT_RU_996x4	11ULL
#define HAL_EHT_RU_NONE		15ULL
#define HAL_EHT_RU_INVALID	31ULL
/*
 * MRUs spanning above 80Mhz
 * HAL_EHT_RU_996_484 = HAL_EHT_RU_484 + HAL_EHT_RU_996 + 4 (reserved)
 */
#define HAL_EHT_RU_996_484	18ULL
#define HAL_EHT_RU_996x2_484	28ULL
#define HAL_EHT_RU_996x3_484	40ULL
#define HAL_EHT_RU_996_484_242	23ULL

/**
 * enum ieee80211_eht_ru_size: RU type id in EHTSIG radiotap header
 * @IEEE80211_EHT_RU_26: RU26
 * @IEEE80211_EHT_RU_52: RU52
 * @IEEE80211_EHT_RU_106: RU106
 * @IEEE80211_EHT_RU_242: RU242
 * @IEEE80211_EHT_RU_484: RU484
 * @IEEE80211_EHT_RU_996: RU996
 * @IEEE80211_EHT_RU_996x2: RU996x2
 * @IEEE80211_EHT_RU_996x4: RU996x4
 * @IEEE80211_EHT_RU_52_26: RU52+RU26
 * @IEEE80211_EHT_RU_106_26: RU106+RU26
 * @IEEE80211_EHT_RU_484_242: RU484+RU242
 * @IEEE80211_EHT_RU_996_484: RU996+RU484
 * @IEEE80211_EHT_RU_996_484_242: RU996+RU484+RU242
 * @IEEE80211_EHT_RU_996x2_484: RU996x2 + RU484
 * @IEEE80211_EHT_RU_996x3: RU996x3
 * @IEEE80211_EHT_RU_996x3_484: RU996x3 + RU484
 * @IEEE80211_EHT_RU_INVALID: Invalid/Max RU
 */
enum ieee80211_eht_ru_size {
	IEEE80211_EHT_RU_26,
	IEEE80211_EHT_RU_52,
	IEEE80211_EHT_RU_106,
	IEEE80211_EHT_RU_242,
	IEEE80211_EHT_RU_484,
	IEEE80211_EHT_RU_996,
	IEEE80211_EHT_RU_996x2,
	IEEE80211_EHT_RU_996x4,
	IEEE80211_EHT_RU_52_26,
	IEEE80211_EHT_RU_106_26,
	IEEE80211_EHT_RU_484_242,
	IEEE80211_EHT_RU_996_484,
	IEEE80211_EHT_RU_996_484_242,
	IEEE80211_EHT_RU_996x2_484,
	IEEE80211_EHT_RU_996x3,
	IEEE80211_EHT_RU_996x3_484,
	IEEE80211_EHT_RU_INVALID,
};

#define NUM_RU_BITS_PER80	16
#define NUM_RU_BITS_PER20	4

/* Different per_80Mhz band in 320Mhz bandwidth */
#define HAL_80_0	0
#define HAL_80_1	1
#define HAL_80_2	2
#define HAL_80_3	3

#define HAL_RU_SHIFT(num_80mhz_band, ru_index_per_80)	\
		((NUM_RU_BITS_PER80 * (num_80mhz_band)) +	\
		 (NUM_RU_BITS_PER20 * (ru_index_per_80)))

/* MRU-996+484 */
#define HAL_EHT_RU_996_484_0	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_0, 1)) |	\
				 (HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_1, 0)))
#define HAL_EHT_RU_996_484_1	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_1, 0)))
#define HAL_EHT_RU_996_484_2	((HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 1)))
#define HAL_EHT_RU_996_484_3	((HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 0)))
#define HAL_EHT_RU_996_484_4	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 1)) |	\
				 (HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996_484_5	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996_484_6	((HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_3, 1)))
#define HAL_EHT_RU_996_484_7	((HAL_EHT_RU_996 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_3, 0)))

/* MRU-996x2+484 */
#define HAL_EHT_RU_996x2_484_0	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_0, 1)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)))
#define HAL_EHT_RU_996x2_484_1	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)))
#define HAL_EHT_RU_996x2_484_2	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 1)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)))
#define HAL_EHT_RU_996x2_484_3	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)))
#define HAL_EHT_RU_996x2_484_4	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 1)))
#define HAL_EHT_RU_996x2_484_5	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 0)))
#define HAL_EHT_RU_996x2_484_6	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 1)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x2_484_7	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x2_484_8	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 1)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x2_484_9	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x2_484_10	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_3, 1)))
#define HAL_EHT_RU_996x2_484_11	((HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x2 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_3, 0)))

/* MRU-996x3+484 */
#define HAL_EHT_RU_996x3_484_0	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_0, 1)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x3_484_1	((HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x3_484_2	((HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 1)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x3_484_3	((HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x3_484_4	((HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 1)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x3_484_5	((HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_3, 0)))
#define HAL_EHT_RU_996x3_484_6	((HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_3, 1)))
#define HAL_EHT_RU_996x3_484_7	((HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_0, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_1, 0)) |	\
				 (HAL_EHT_RU_996x3 << HAL_RU_SHIFT(HAL_80_2, 0)) |	\
				 (HAL_EHT_RU_484 << HAL_RU_SHIFT(HAL_80_3, 0)))

#define HAL_RX_MON_MAX_AGGR_SIZE	128

/**
 * struct hal_rx_tlv_aggr_info - Data structure to hold
 *		metadata for aggregatng repeated TLVs
 * @in_progress: Flag to indicate if TLV aggregation is in progress
 * @cur_len: Total length of currently aggregated TLV
 * @tlv_tag: TLV tag which is currently being aggregated
 * @buf: Buffer containing aggregated TLV data
 */
struct hal_rx_tlv_aggr_info {
	uint8_t in_progress;
	uint16_t cur_len;
	uint32_t tlv_tag;
	uint8_t buf[HAL_RX_MON_MAX_AGGR_SIZE];
};

/* struct hal_rx_u_sig_info - Certain fields from U-SIG header which are used
 *		for other header field parsing.
 * @ul_dl: UL or DL
 * @bw: EHT BW
 * @ppdu_type_comp_mode: PPDU TYPE
 * @eht_sig_mcs: EHT SIG MCS
 * @num_eht_sig_sym: Number of EHT SIG symbols
 */
struct hal_rx_u_sig_info {
	uint32_t ul_dl : 1,
		 bw : 3,
		 ppdu_type_comp_mode : 2,
		 eht_sig_mcs : 2,
		 num_eht_sig_sym : 5;
};

#ifdef WLAN_SUPPORT_CTRL_FRAME_STATS
struct hal_rx_user_ctrl_frm_info {
	uint8_t bar : 1,
		ndpa : 1;
};
#else
struct hal_rx_user_ctrl_frm_info {};
#endif /* WLAN_SUPPORT_CTRL_FRAME_STATS */

struct hal_rx_ppdu_info {
	struct hal_rx_ppdu_common_info com_info;
	struct hal_rx_u_sig_info u_sig_info;
	struct mon_rx_status rx_status;
	struct mon_rx_user_status rx_user_status[HAL_MAX_UL_MU_USERS];
	struct mon_rx_info rx_info;
	struct mon_rx_user_info rx_user_info[HAL_MAX_UL_MU_USERS];
	struct hal_rx_msdu_payload_info msdu_info;
	struct hal_rx_msdu_payload_info fcs_ok_msdu_info;
	struct hal_rx_nac_info nac_info;
	/* status ring PPDU start and end state */
	uint8_t rx_state;
	/* MU user id for status ring TLV */
	uint8_t user_id;
	/* MPDU/MSDU truncated to 128 bytes header start addr in status skb */
	unsigned char *data;
	/* MPDU/MSDU truncated to 128 bytes header real length */
	uint32_t hdr_len;
	/* MPDU FCS error */
	bool fcs_err;
	/* Id to indicate how to process mpdu */
	uint8_t sw_frame_group_id;
	struct hal_rx_ppdu_msdu_info rx_msdu_info[HAL_MAX_UL_MU_USERS];
	/* fcs passed mpdu count in rx monitor status buffer */
	uint8_t fcs_ok_cnt;
	/* fcs error mpdu count in rx monitor status buffer */
	uint8_t fcs_err_cnt;
	/* MPDU FCS passed */
	bool is_fcs_passed;
	/* first msdu payload for all mpdus in rx monitor status buffer */
	struct hal_rx_msdu_payload_info ppdu_msdu_info[HAL_RX_MAX_MPDU_H_PER_STATUS_BUFFER];
	/* evm info */
	struct hal_rx_su_evm_info evm_info;
	/**
	 * Will be used to store ppdu info extracted from HW TLVs,
	 * and for CFR correlation as well
	 */
	struct hal_rx_ppdu_cfr_info cfr_info;
	/* per frame type counts */
	struct hal_rx_frm_type_info frm_type_info;
	/* TLV aggregation metadata context */
	struct hal_rx_tlv_aggr_info tlv_aggr;
	/* EHT SIG user info */
	uint32_t eht_sig_user_info;
	/*per user mpdu count */
	uint8_t mpdu_count[HAL_MAX_UL_MU_USERS];
	/*per user msdu count */
	uint8_t msdu_count[HAL_MAX_UL_MU_USERS];
	/* Placeholder to update per user last processed msdu’s info */
	struct hal_rx_mon_msdu_info  msdu[HAL_MAX_UL_MU_USERS];
	/* Placeholder to update per user last processed mpdu’s info */
	struct hal_rx_mon_mpdu_info mpdu_info[HAL_MAX_UL_MU_USERS];
	 /* placeholder to hold packet buffer info */
	struct hal_mon_packet_info packet_info;
#ifdef QCA_MONITOR_2_0_SUPPORT
	 /* per user per MPDU queue */
	qdf_nbuf_queue_t mpdu_q[HAL_MAX_UL_MU_USERS];
#endif
	 /* ppdu info list element */
	TAILQ_ENTRY(hal_rx_ppdu_info) ppdu_list_elem;
	 /* ppdu info free list element */
	TAILQ_ENTRY(hal_rx_ppdu_info) ppdu_free_list_elem;
	/* placeholder to track if RX_HDR is received */
	uint8_t rx_hdr_rcvd[HAL_MAX_UL_MU_USERS];
	/* Per user BAR and NDPA bit flag */
	struct hal_rx_user_ctrl_frm_info ctrl_frm_info[HAL_MAX_UL_MU_USERS];
	/* PPDU end user stats count */
	uint8_t end_user_stats_cnt;
	/* PPDU start user info count */
	uint8_t start_user_info_cnt;
	/* PPDU drop cnt */
	struct hal_rx_ppdu_drop_cnt drop_cnt;
};

static inline uint32_t
hal_get_rx_status_buf_size(void) {
	/* RX status buffer size is hard coded for now */
	return 2048;
}

static inline uint8_t*
hal_rx_status_get_next_tlv(uint8_t *rx_tlv, bool is_tlv_hdr_64_bit) {
	uint32_t tlv_len, tlv_tag, tlv_hdr_size;

	if (is_tlv_hdr_64_bit) {
		tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv);
		tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv);

		tlv_hdr_size = HAL_RX_TLV64_HDR_SIZE;
	} else {
		tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv);
		tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv);

		tlv_hdr_size = HAL_RX_TLV32_HDR_SIZE;
	}

	/* The actual length of PPDU_END is the combined length of many PHY
	 * TLVs that follow. Skip the TLV header and
	 * rx_rxpcu_classification_overview that follows the header to get to
	 * next TLV.
	 */
	if (tlv_tag == WIFIRX_PPDU_END_E)
		tlv_len = sizeof(struct rx_rxpcu_classification_overview);

	return (uint8_t *)(uintptr_t)qdf_align((uint64_t)((uintptr_t)rx_tlv +
							  tlv_len +
							  tlv_hdr_size),
					       tlv_hdr_size);
}

/**
 * hal_rx_proc_phyrx_other_receive_info_tlv()
 *				    - process other receive info TLV
 * @rx_tlv_hdr: pointer to TLV header
 * @ppdu_info: pointer to ppdu_info
 *
 * Return: None
 */
static inline void hal_rx_proc_phyrx_other_receive_info_tlv(struct hal_soc *hal_soc,
						     void *rx_tlv_hdr,
						     struct hal_rx_ppdu_info
						     *ppdu_info)
{
	hal_soc->ops->hal_rx_proc_phyrx_other_receive_info_tlv(rx_tlv_hdr,
							(void *)ppdu_info);
}

/**
 * hal_rx_status_get_tlv_info() - process receive info TLV
 * @rx_tlv_hdr: pointer to TLV header
 * @ppdu_info: pointer to ppdu_info
 * @hal_soc: HAL soc handle
 * @nbuf: PPDU status network buffer
 *
 * Return: HAL_TLV_STATUS_PPDU_NOT_DONE or HAL_TLV_STATUS_PPDU_DONE from tlv
 */
static inline uint32_t
hal_rx_status_get_tlv_info(void *rx_tlv_hdr, void *ppdu_info,
			   hal_soc_handle_t hal_soc_hdl,
			   qdf_nbuf_t nbuf)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_rx_status_get_tlv_info(
						rx_tlv_hdr,
						ppdu_info,
						hal_soc_hdl,
						nbuf);
}

static inline
uint32_t hal_get_rx_status_done_tlv_size(hal_soc_handle_t hal_soc_hdl)
{
	return HAL_RX_TLV32_HDR_SIZE;
}

static inline QDF_STATUS
hal_get_rx_status_done(uint8_t *rx_tlv)
{
	uint32_t tlv_tag;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv);

	if (tlv_tag == WIFIRX_STATUS_BUFFER_DONE_E)
		return QDF_STATUS_SUCCESS;
	else
		return QDF_STATUS_E_EMPTY;
}

static inline QDF_STATUS
hal_clear_rx_status_done(uint8_t *rx_tlv)
{
	*(uint32_t *)rx_tlv = 0;
	return QDF_STATUS_SUCCESS;
}
#endif

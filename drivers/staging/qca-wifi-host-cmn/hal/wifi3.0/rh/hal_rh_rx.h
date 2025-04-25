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

#ifndef _HAL_RH_RX_H_
#define _HAL_RH_RX_H_

#include <hal_rx.h>

#define HAL_RX_BUF_COOKIE_GET(buff_addr_info)			\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(buff_addr_info,		\
		BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_OFFSET)),	\
		BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_MASK,	\
		BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_LSB))

#define HAL_RX_BUF_RBM_GET(buff_addr_info)			\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(buff_addr_info,		\
		BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_OFFSET)),\
		BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_MASK,	\
		BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_LSB))
/*
 * macro to set the cookie into the rxdma ring entry
 */
#define HAL_RXDMA_COOKIE_SET(buff_addr_info, cookie) \
		((*(((unsigned int *)buff_addr_info) + \
		(BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_OFFSET >> 2))) &= \
		~BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_MASK); \
		((*(((unsigned int *)buff_addr_info) + \
		(BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_OFFSET >> 2))) |= \
		((cookie) << BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_LSB) & \
		BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_MASK)

/*
 * macro to set the manager into the rxdma ring entry
 */
#define HAL_RXDMA_MANAGER_SET(buff_addr_info, manager) \
		((*(((unsigned int *)buff_addr_info) + \
		(BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_OFFSET >> 2))) &= \
		~BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_MASK); \
		((*(((unsigned int *)buff_addr_info) + \
		(BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_OFFSET >> 2))) |= \
		((manager) << BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_LSB) & \
		BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_MASK)

/*
 * NOTE: None of the following _GET macros need a right
 * shift by the corresponding _LSB. This is because, they are
 * finally taken and "OR'ed" into a single word again.
 */

#define HAL_RX_MSDU_CONTINUATION_FLAG_GET(msdu_info_ptr)	\
	((*_OFFSET_TO_WORD_PTR(msdu_info_ptr,			\
		RX_MSDU_DESC_INFO_0_MSDU_CONTINUATION_OFFSET)) & \
		RX_MSDU_DESC_INFO_0_MSDU_CONTINUATION_MASK)

#define HAL_RX_MSDU_SA_IS_VALID_FLAG_GET(msdu_info_ptr)		\
	((*_OFFSET_TO_WORD_PTR(msdu_info_ptr,			\
		RX_MSDU_DESC_INFO_0_SA_IS_VALID_OFFSET)) &	\
		RX_MSDU_DESC_INFO_0_SA_IS_VALID_MASK)

#define HAL_RX_MSDU_SA_IDX_TIMEOUT_FLAG_GET(msdu_info_ptr)	\
	((*_OFFSET_TO_WORD_PTR(msdu_info_ptr,			\
		RX_MSDU_DESC_INFO_0_SA_IDX_TIMEOUT_OFFSET)) &	\
		RX_MSDU_DESC_INFO_0_SA_IDX_TIMEOUT_MASK)

#define HAL_RX_MSDU_DA_IS_VALID_FLAG_GET(msdu_info_ptr)		\
	((*_OFFSET_TO_WORD_PTR(msdu_info_ptr,			\
		RX_MSDU_DESC_INFO_0_DA_IS_VALID_OFFSET)) &	\
		RX_MSDU_DESC_INFO_0_DA_IS_VALID_MASK)

#define HAL_RX_MSDU_DA_IS_MCBC_FLAG_GET(msdu_info_ptr)		\
	((*_OFFSET_TO_WORD_PTR(msdu_info_ptr,			\
		RX_MSDU_DESC_INFO_0_DA_IS_MCBC_OFFSET)) &	\
		RX_MSDU_DESC_INFO_0_DA_IS_MCBC_MASK)

#define HAL_RX_MSDU_DA_IDX_TIMEOUT_FLAG_GET(msdu_info_ptr) \
	((*_OFFSET_TO_WORD_PTR(msdu_info_ptr,			\
		RX_MSDU_DESC_INFO_0_DA_IDX_TIMEOUT_OFFSET)) &	\
		RX_MSDU_DESC_INFO_0_DA_IDX_TIMEOUT_MASK)

/*
 * Structures & Macros to obtain fields from the TLV's in the Rx packet
 * pre-header.
 */

/*
 * Every Rx packet starts at an offset from the top of the buffer.
 * If the host hasn't subscribed to any specific TLV, there is
 * still space reserved for the following TLV's from the start of
 * the buffer:
 *	-- RX ATTENTION
 *	-- RX MPDU START
 *	-- RX MSDU START
 *	-- RX MSDU END
 *	-- RX MPDU END
 *	-- RX PACKET HEADER (802.11)
 * If the host subscribes to any of the TLV's above, that TLV
 * if populated by the HW
 */

#define NUM_DWORDS_TAG		1

/* By default the packet header TLV is 128 bytes */
#define  NUM_OF_BYTES_RX_802_11_HDR_TLV		128
#define  NUM_OF_DWORDS_RX_802_11_HDR_TLV	\
		(NUM_OF_BYTES_RX_802_11_HDR_TLV >> 2)

#define RX_PKT_OFFSET_WORDS					\
	(							\
	 NUM_OF_DWORDS_RX_ATTENTION + NUM_DWORDS_TAG		\
	 NUM_OF_DWORDS_RX_MPDU_START + NUM_DWORDS_TAG		\
	 NUM_OF_DWORDS_RX_MSDU_START + NUM_DWORDS_TAG		\
	 NUM_OF_DWORDS_RX_MSDU_END + NUM_DWORDS_TAG		\
	 NUM_OF_DWORDS_RX_MPDU_END + NUM_DWORDS_TAG		\
	 NUM_OF_DWORDS_RX_802_11_HDR_TLV + NUM_DWORDS_TAG	\
	)

#define RX_PKT_OFFSET_BYTES			\
	(RX_PKT_OFFSET_WORDS << 2)

#define RX_PKT_HDR_TLV_LEN		120

/*
 * Each RX descriptor TLV is preceded by 1 DWORD "tag"
 */
struct rx_attention_tlv {
	uint32_t tag;
	struct rx_attention rx_attn;
};

struct rx_mpdu_start_tlv {
	uint32_t tag;
	struct rx_mpdu_start rx_mpdu_start;
};

struct rx_msdu_start_tlv {
	uint32_t tag;
	struct rx_msdu_start rx_msdu_start;
};

struct rx_msdu_end_tlv {
	uint32_t tag;
	struct rx_msdu_end rx_msdu_end;
};

struct rx_mpdu_end_tlv {
	uint32_t tag;
	struct rx_mpdu_end rx_mpdu_end;
};

struct rx_pkt_hdr_tlv {
	uint32_t tag;				/* 4 B */
	uint32_t phy_ppdu_id;                   /* 4 B */
	char rx_pkt_hdr[RX_PKT_HDR_TLV_LEN];	/* 120 B */
};

/* rx_pkt_tlvs structure should be used to process Data buffers, monitor status
 * buffers, monitor destination buffers and monitor descriptor buffers.
 */
#ifdef RXDMA_OPTIMIZATION
/*
 * The RX_PADDING_BYTES is required so that the TLV's don't
 * spread across the 128 byte boundary
 * RXDMA optimization requires:
 * 1) MSDU_END & ATTENTION TLV's follow in that order
 * 2) TLV's don't span across 128 byte lines
 * 3) Rx Buffer is nicely aligned on the 128 byte boundary
 */
#define RX_PADDING0_BYTES	4
#define RX_PADDING1_BYTES	16
struct rx_pkt_tlvs {
	struct rx_msdu_end_tlv   msdu_end_tlv;	/*  72 bytes */
	struct rx_attention_tlv  attn_tlv;	/*  16 bytes */
	struct rx_msdu_start_tlv msdu_start_tlv;/*  40 bytes */
	uint8_t rx_padding0[RX_PADDING0_BYTES];	/*   4 bytes */
	struct rx_mpdu_start_tlv mpdu_start_tlv;/*  96 bytes */
	struct rx_mpdu_end_tlv   mpdu_end_tlv;	/*  12 bytes */
	uint8_t rx_padding1[RX_PADDING1_BYTES];	/*  16 bytes */
#ifndef NO_RX_PKT_HDR_TLV
	struct rx_pkt_hdr_tlv	 pkt_hdr_tlv;	/* 128 bytes */
#endif
};
#else /* RXDMA_OPTIMIZATION */
struct rx_pkt_tlvs {
	struct rx_attention_tlv  attn_tlv;
	struct rx_mpdu_start_tlv mpdu_start_tlv;
	struct rx_msdu_start_tlv msdu_start_tlv;
	struct rx_msdu_end_tlv   msdu_end_tlv;
	struct rx_mpdu_end_tlv   mpdu_end_tlv;
	struct rx_pkt_hdr_tlv	 pkt_hdr_tlv;
};
#endif /* RXDMA_OPTIMIZATION */

/* rx_mon_pkt_tlvs structure should be used to process monitor data buffers */
#ifdef RXDMA_OPTIMIZATION
struct rx_mon_pkt_tlvs {
	struct rx_msdu_end_tlv   msdu_end_tlv;	/*  72 bytes */
	struct rx_attention_tlv  attn_tlv;	/*  16 bytes */
	struct rx_msdu_start_tlv msdu_start_tlv;/*  40 bytes */
	uint8_t rx_padding0[RX_PADDING0_BYTES];	/*   4 bytes */
	struct rx_mpdu_start_tlv mpdu_start_tlv;/*  96 bytes */
	struct rx_mpdu_end_tlv   mpdu_end_tlv;	/*  12 bytes */
	uint8_t rx_padding1[RX_PADDING1_BYTES];	/*  16 bytes */
	struct rx_pkt_hdr_tlv	 pkt_hdr_tlv;	/* 128 bytes */
};
#else /* RXDMA_OPTIMIZATION */
struct rx_mon_pkt_tlvs {
	struct rx_attention_tlv  attn_tlv;
	struct rx_mpdu_start_tlv mpdu_start_tlv;
	struct rx_msdu_start_tlv msdu_start_tlv;
	struct rx_msdu_end_tlv   msdu_end_tlv;
	struct rx_mpdu_end_tlv   mpdu_end_tlv;
	struct rx_pkt_hdr_tlv	 pkt_hdr_tlv;
};
#endif

#define SIZE_OF_MONITOR_TLV sizeof(struct rx_mon_pkt_tlvs)
#define SIZE_OF_DATA_RX_TLV sizeof(struct rx_pkt_tlvs)

#define RX_PKT_TLVS_LEN		SIZE_OF_DATA_RX_TLV

#define RX_PKT_TLV_OFFSET(field) qdf_offsetof(struct rx_pkt_tlvs, field)

#define HAL_RX_PKT_TLV_MPDU_START_OFFSET(hal_soc) \
					RX_PKT_TLV_OFFSET(mpdu_start_tlv)
#define HAL_RX_PKT_TLV_MPDU_END_OFFSET(hal_soc) RX_PKT_TLV_OFFSET(mpdu_end_tlv)
#define HAL_RX_PKT_TLV_MSDU_START_OFFSET(hal_soc) \
					RX_PKT_TLV_OFFSET(msdu_start_tlv)
#define HAL_RX_PKT_TLV_MSDU_END_OFFSET(hal_soc) RX_PKT_TLV_OFFSET(msdu_end_tlv)
#define HAL_RX_PKT_TLV_ATTN_OFFSET(hal_soc) RX_PKT_TLV_OFFSET(attn_tlv)
#define HAL_RX_PKT_TLV_PKT_HDR_OFFSET(hal_soc) RX_PKT_TLV_OFFSET(pkt_hdr_tlv)

/**
 * hal_rx_get_pkt_tlvs(): Function to retrieve pkt tlvs from nbuf
 *
 * @rx_buf_start: Pointer to data buffer field
 *
 * Returns: pointer to rx_pkt_tlvs
 */
static inline
struct rx_pkt_tlvs *hal_rx_get_pkt_tlvs(uint8_t *rx_buf_start)
{
	return (struct rx_pkt_tlvs *)rx_buf_start;
}

/**
 * hal_rx_get_mpdu_info(): Function to retrieve mpdu info from pkt tlvs
 *
 * @pkt_tlvs: Pointer to pkt_tlvs
 * Returns: pointer to rx_mpdu_info structure
 */
static inline
struct rx_mpdu_info *hal_rx_get_mpdu_info(struct rx_pkt_tlvs *pkt_tlvs)
{
	return &pkt_tlvs->mpdu_start_tlv.rx_mpdu_start.rx_mpdu_info_details;
}

/**
 * hal_rx_mon_dest_get_buffer_info_from_tlv(): Retrieve mon dest frame info
 * from the reserved bytes of rx_tlv_hdr.
 * @buf: start of rx_tlv_hdr
 * @buf_info: hal_rx_mon_dest_buf_info structure
 *
 * Return: void
 */
static inline void hal_rx_mon_dest_get_buffer_info_from_tlv(
				uint8_t *buf,
				struct hal_rx_mon_dest_buf_info *buf_info)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;

	qdf_mem_copy(buf_info, pkt_tlvs->rx_padding0,
		     sizeof(struct hal_rx_mon_dest_buf_info));
}

/*
 * Get msdu_done bit from the RX_ATTENTION TLV
 */
#define HAL_RX_ATTN_MSDU_DONE_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,	\
		RX_ATTENTION_2_MSDU_DONE_OFFSET)),	\
		RX_ATTENTION_2_MSDU_DONE_MASK,		\
		RX_ATTENTION_2_MSDU_DONE_LSB))

#define HAL_RX_ATTN_FIRST_MPDU_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,	\
		RX_ATTENTION_1_FIRST_MPDU_OFFSET)),	\
		RX_ATTENTION_1_FIRST_MPDU_MASK,		\
		RX_ATTENTION_1_FIRST_MPDU_LSB))

#define HAL_RX_ATTN_TCP_UDP_CKSUM_FAIL_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,		\
		RX_ATTENTION_1_TCP_UDP_CHKSUM_FAIL_OFFSET)),	\
		RX_ATTENTION_1_TCP_UDP_CHKSUM_FAIL_MASK,	\
		RX_ATTENTION_1_TCP_UDP_CHKSUM_FAIL_LSB))

/*
 * hal_rx_attn_tcp_udp_cksum_fail_get(): get tcp_udp cksum fail bit
 * from rx attention
 * @buf: pointer to rx_pkt_tlvs
 *
 * Return: tcp_udp_cksum_fail
 */
static inline bool
hal_rx_attn_tcp_udp_cksum_fail_get(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint8_t tcp_udp_cksum_fail;

	tcp_udp_cksum_fail = HAL_RX_ATTN_TCP_UDP_CKSUM_FAIL_GET(rx_attn);

	return !!tcp_udp_cksum_fail;
}

#define HAL_RX_ATTN_IP_CKSUM_FAIL_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,	\
		RX_ATTENTION_1_IP_CHKSUM_FAIL_OFFSET)),	\
		RX_ATTENTION_1_IP_CHKSUM_FAIL_MASK,	\
		RX_ATTENTION_1_IP_CHKSUM_FAIL_LSB))

/*
 * hal_rx_attn_ip_cksum_fail_get(): get ip cksum fail bit
 * from rx attention
 * @buf: pointer to rx_pkt_tlvs
 *
 * Return: ip_cksum_fail
 */
static inline bool
hal_rx_attn_ip_cksum_fail_get(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint8_t	 ip_cksum_fail;

	ip_cksum_fail = HAL_RX_ATTN_IP_CKSUM_FAIL_GET(rx_attn);

	return !!ip_cksum_fail;
}

#define HAL_RX_ATTN_PHY_PPDU_ID_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,	\
		RX_ATTENTION_0_PHY_PPDU_ID_OFFSET)),	\
		RX_ATTENTION_0_PHY_PPDU_ID_MASK,	\
		RX_ATTENTION_0_PHY_PPDU_ID_LSB))

#define HAL_RX_ATTN_CCE_MATCH_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,	\
		RX_ATTENTION_1_CCE_MATCH_OFFSET)),		\
		RX_ATTENTION_1_CCE_MATCH_MASK,			\
		RX_ATTENTION_1_CCE_MATCH_LSB))

/*
 * hal_rx_msdu_cce_match_get_rh(): get CCE match bit
 * from rx attention
 * @buf: pointer to rx_pkt_tlvs
 * Return: CCE match value
 */
static inline bool
hal_rx_msdu_cce_match_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint8_t cce_match_val;

	cce_match_val = HAL_RX_ATTN_CCE_MATCH_GET(rx_attn);
	return !!cce_match_val;
}

/*
 * Get peer_meta_data from RX_MPDU_INFO within RX_MPDU_START
 */
#define HAL_RX_MPDU_PEER_META_DATA_GET(_rx_mpdu_info)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_mpdu_info,	\
		RX_MPDU_INFO_8_PEER_META_DATA_OFFSET)),	\
		RX_MPDU_INFO_8_PEER_META_DATA_MASK,	\
		RX_MPDU_INFO_8_PEER_META_DATA_LSB))

static inline uint32_t
hal_rx_mpdu_peer_meta_data_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_mpdu_start *mpdu_start =
				 &pkt_tlvs->mpdu_start_tlv.rx_mpdu_start;

	struct rx_mpdu_info *mpdu_info = &mpdu_start->rx_mpdu_info_details;
	uint32_t peer_meta_data;

	peer_meta_data = HAL_RX_MPDU_PEER_META_DATA_GET(mpdu_info);

	return peer_meta_data;
}

#define HAL_RX_MPDU_INFO_AMPDU_FLAG_GET(_rx_mpdu_info)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_mpdu_info,	\
		RX_MPDU_INFO_12_AMPDU_FLAG_OFFSET)),	\
		RX_MPDU_INFO_12_AMPDU_FLAG_MASK,	\
		RX_MPDU_INFO_12_AMPDU_FLAG_LSB))

/*
 * LRO information needed from the TLVs
 */
#define HAL_RX_TLV_GET_LRO_ELIGIBLE(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_end_tlv.rx_msdu_end), \
			 RX_MSDU_END_9_LRO_ELIGIBLE_OFFSET)), \
		RX_MSDU_END_9_LRO_ELIGIBLE_MASK, \
		RX_MSDU_END_9_LRO_ELIGIBLE_LSB))

#define HAL_RX_TLV_GET_TCP_ACK(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_end_tlv.rx_msdu_end), \
			 RX_MSDU_END_8_TCP_ACK_NUMBER_OFFSET)), \
		RX_MSDU_END_8_TCP_ACK_NUMBER_MASK, \
		RX_MSDU_END_8_TCP_ACK_NUMBER_LSB))

#define HAL_RX_TLV_GET_TCP_SEQ(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_end_tlv.rx_msdu_end), \
			 RX_MSDU_END_7_TCP_SEQ_NUMBER_OFFSET)), \
		RX_MSDU_END_7_TCP_SEQ_NUMBER_MASK, \
		RX_MSDU_END_7_TCP_SEQ_NUMBER_LSB))

#define HAL_RX_TLV_GET_TCP_WIN(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_end_tlv.rx_msdu_end), \
			 RX_MSDU_END_9_WINDOW_SIZE_OFFSET)), \
		RX_MSDU_END_9_WINDOW_SIZE_MASK, \
		RX_MSDU_END_9_WINDOW_SIZE_LSB))

#define HAL_RX_TLV_GET_TCP_PURE_ACK(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_2_TCP_ONLY_ACK_OFFSET)), \
		RX_MSDU_START_2_TCP_ONLY_ACK_MASK, \
		RX_MSDU_START_2_TCP_ONLY_ACK_LSB))

#define HAL_RX_TLV_GET_TCP_PROTO(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_2_TCP_PROTO_OFFSET)), \
		RX_MSDU_START_2_TCP_PROTO_MASK, \
		RX_MSDU_START_2_TCP_PROTO_LSB))

#define HAL_RX_TLV_GET_UDP_PROTO(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_2_UDP_PROTO_OFFSET)), \
		RX_MSDU_START_2_UDP_PROTO_MASK, \
		RX_MSDU_START_2_UDP_PROTO_LSB))

#define HAL_RX_TLV_GET_IPV6(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_2_IPV6_PROTO_OFFSET)), \
		RX_MSDU_START_2_IPV6_PROTO_MASK, \
		RX_MSDU_START_2_IPV6_PROTO_LSB))

#define HAL_RX_TLV_GET_IP_OFFSET(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_1_L3_OFFSET_OFFSET)), \
		RX_MSDU_START_1_L3_OFFSET_MASK, \
		RX_MSDU_START_1_L3_OFFSET_LSB))

#define HAL_RX_TLV_GET_TCP_OFFSET(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_1_L4_OFFSET_OFFSET)), \
		RX_MSDU_START_1_L4_OFFSET_MASK, \
		RX_MSDU_START_1_L4_OFFSET_LSB))

#define HAL_RX_TLV_GET_FLOW_ID_TOEPLITZ(buf) \
	(_HAL_MS( \
		 (*_OFFSET_TO_WORD_PTR(&(((struct rx_pkt_tlvs *)(buf))->\
			 msdu_start_tlv.rx_msdu_start), \
			 RX_MSDU_START_4_FLOW_ID_TOEPLITZ_OFFSET)), \
		RX_MSDU_START_4_FLOW_ID_TOEPLITZ_MASK, \
		RX_MSDU_START_4_FLOW_ID_TOEPLITZ_LSB))

#define HAL_RX_MSDU_START_MSDU_LEN_GET(_rx_msdu_start)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_msdu_start,		\
		RX_MSDU_START_1_MSDU_LENGTH_OFFSET)),		\
		RX_MSDU_START_1_MSDU_LENGTH_MASK,		\
		RX_MSDU_START_1_MSDU_LENGTH_LSB))

#define HAL_RX_MSDU_START_BW_GET(_rx_msdu_start)     \
	(_HAL_MS((*_OFFSET_TO_WORD_PTR((_rx_msdu_start),\
	RX_MSDU_START_5_RECEIVE_BANDWIDTH_OFFSET)), \
	RX_MSDU_START_5_RECEIVE_BANDWIDTH_MASK,     \
	RX_MSDU_START_5_RECEIVE_BANDWIDTH_LSB))

#define HAL_RX_MSDU_START_SGI_GET(_rx_msdu_start)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR((_rx_msdu_start),\
		RX_MSDU_START_5_SGI_OFFSET)),		\
		RX_MSDU_START_5_SGI_MASK,		\
		RX_MSDU_START_5_SGI_LSB))

#define HAL_RX_MSDU_START_RATE_MCS_GET(_rx_msdu_start)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR((_rx_msdu_start),\
		RX_MSDU_START_5_RATE_MCS_OFFSET)),	\
		RX_MSDU_START_5_RATE_MCS_MASK,		\
		RX_MSDU_START_5_RATE_MCS_LSB))

#define HAL_RX_ATTN_DECRYPT_STATUS_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,		\
		RX_ATTENTION_2_DECRYPT_STATUS_CODE_OFFSET)),	\
		RX_ATTENTION_2_DECRYPT_STATUS_CODE_MASK,	\
		RX_ATTENTION_2_DECRYPT_STATUS_CODE_LSB))

/*
 * Get key index from RX_MSDU_END
 */
#define HAL_RX_MSDU_END_KEYID_OCTET_GET(_rx_msdu_end)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_msdu_end,	\
		RX_MSDU_END_2_KEY_ID_OCTET_OFFSET)),	\
		RX_MSDU_END_2_KEY_ID_OCTET_MASK,	\
		RX_MSDU_END_2_KEY_ID_OCTET_LSB))

#define HAL_RX_MSDU_START_RSSI_GET(_rx_msdu_start)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_msdu_start,  \
		RX_MSDU_START_5_USER_RSSI_OFFSET)),	\
		RX_MSDU_START_5_USER_RSSI_MASK,		\
		RX_MSDU_START_5_USER_RSSI_LSB))

#define HAL_RX_MSDU_START_FREQ_GET(_rx_msdu_start)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_msdu_start,		\
		RX_MSDU_START_7_SW_PHY_META_DATA_OFFSET)),      \
		RX_MSDU_START_7_SW_PHY_META_DATA_MASK,		\
		RX_MSDU_START_7_SW_PHY_META_DATA_LSB))

#define HAL_RX_MSDU_START_PKT_TYPE_GET(_rx_msdu_start)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_msdu_start,  \
		RX_MSDU_START_5_PKT_TYPE_OFFSET)),      \
		RX_MSDU_START_5_PKT_TYPE_MASK,		\
		RX_MSDU_START_5_PKT_TYPE_LSB))

#define HAL_RX_MPDU_AD4_31_0_GET(_rx_mpdu_info)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_mpdu_info, \
		RX_MPDU_INFO_20_MAC_ADDR_AD4_31_0_OFFSET)), \
		RX_MPDU_INFO_20_MAC_ADDR_AD4_31_0_MASK,	\
		RX_MPDU_INFO_20_MAC_ADDR_AD4_31_0_LSB))

#define HAL_RX_MPDU_AD4_47_32_GET(_rx_mpdu_info)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_mpdu_info, \
		RX_MPDU_INFO_21_MAC_ADDR_AD4_47_32_OFFSET)), \
		RX_MPDU_INFO_21_MAC_ADDR_AD4_47_32_MASK,	\
		RX_MPDU_INFO_21_MAC_ADDR_AD4_47_32_LSB))

/*******************************************************************************
 * RX ERROR APIS
 ******************************************************************************/

#define HAL_RX_MPDU_END_DECRYPT_ERR_GET(_rx_mpdu_end)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR((_rx_mpdu_end),\
		RX_MPDU_END_1_RX_IN_TX_DECRYPT_BYP_OFFSET)),	\
		RX_MPDU_END_1_RX_IN_TX_DECRYPT_BYP_MASK,	\
		RX_MPDU_END_1_RX_IN_TX_DECRYPT_BYP_LSB))

#define HAL_RX_MPDU_END_MIC_ERR_GET(_rx_mpdu_end)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR((_rx_mpdu_end),\
		RX_MPDU_END_1_TKIP_MIC_ERR_OFFSET)),	\
		RX_MPDU_END_1_TKIP_MIC_ERR_MASK,	\
		RX_MPDU_END_1_TKIP_MIC_ERR_LSB))

#define HAL_RX_MPDU_GET_FRAME_CONTROL_FIELD(_rx_mpdu_info)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_mpdu_info,	\
		RX_MPDU_INFO_14_MPDU_FRAME_CONTROL_FIELD_OFFSET)),	\
		RX_MPDU_INFO_14_MPDU_FRAME_CONTROL_FIELD_MASK,	\
		RX_MPDU_INFO_14_MPDU_FRAME_CONTROL_FIELD_LSB))

/**
 * hal_rx_attn_msdu_done_get_rh() - Get msdi done flag from RX TLV
 * @buf: RX tlv address
 *
 * Return: msdu done flag
 */
static inline uint32_t hal_rx_attn_msdu_done_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint32_t msdu_done;

	msdu_done = HAL_RX_ATTN_MSDU_DONE_GET(rx_attn);

	return msdu_done;
}

#define HAL_RX_MSDU_FLAGS_GET(msdu_info_ptr) \
	(HAL_RX_FIRST_MSDU_IN_MPDU_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_LAST_MSDU_IN_MPDU_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_MSDU_CONTINUATION_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_MSDU_SA_IS_VALID_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_MSDU_SA_IDX_TIMEOUT_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_MSDU_DA_IS_VALID_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_MSDU_DA_IS_MCBC_FLAG_GET(msdu_info_ptr) | \
	HAL_RX_MSDU_DA_IDX_TIMEOUT_FLAG_GET(msdu_info_ptr))

/**
 * hal_rx_msdu_flags_get_rh() - Get msdu flags from ring desc
 * @msdu_desc_info_hdl: msdu desc info handle
 *
 * Return: msdu flags
 */
	static inline
uint32_t hal_rx_msdu_flags_get_rh(rx_msdu_desc_info_t msdu_desc_info_hdl)
{
	struct rx_msdu_desc_info *msdu_desc_info =
		(struct rx_msdu_desc_info *)msdu_desc_info_hdl;

	return HAL_RX_MSDU_FLAGS_GET(msdu_desc_info);
}

#define HAL_RX_ATTN_MSDU_LEN_ERR_GET(_rx_attn)		\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(_rx_attn,	\
		RX_ATTENTION_1_MSDU_LENGTH_ERR_OFFSET)),	\
		RX_ATTENTION_1_MSDU_LENGTH_ERR_MASK,		\
		RX_ATTENTION_1_MSDU_LENGTH_ERR_LSB))

/**
 * hal_rx_attn_msdu_len_err_get_rh(): Get msdu_len_err value from
 *  rx attention tlvs
 * @buf: pointer to rx pkt tlvs hdr
 *
 * Return: msdu_len_err value
 */
static inline uint32_t
hal_rx_attn_msdu_len_err_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;

	return HAL_RX_ATTN_MSDU_LEN_ERR_GET(rx_attn);
}
#endif /* _HAL_RH_RX_H_ */

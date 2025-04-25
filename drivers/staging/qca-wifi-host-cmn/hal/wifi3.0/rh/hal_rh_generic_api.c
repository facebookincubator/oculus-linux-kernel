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

#include "hal_rh_api.h"
#include "hal_rx.h"
#include "hal_rh_rx.h"
#include "hal_tx.h"
#include <hal_api_mon.h>

static uint32_t hal_get_reo_qdesc_size_rh(uint32_t ba_window_size,
					  int tid)
{
	return 0;
}

static uint16_t hal_get_rx_max_ba_window_rh(int tid)
{
	return 0;
}

static void hal_set_link_desc_addr_rh(void *desc, uint32_t cookie,
				      qdf_dma_addr_t link_desc_paddr,
				      uint8_t bm_id)
{
	uint32_t *buf_addr = (uint32_t *)desc;

	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO_0, BUFFER_ADDR_31_0,
			   link_desc_paddr & 0xffffffff);
	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO_1, BUFFER_ADDR_39_32,
			   (uint64_t)link_desc_paddr >> 32);
	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO_1, RETURN_BUFFER_MANAGER,
			   bm_id);
	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO_1, SW_BUFFER_COOKIE,
			   cookie);
}

static void hal_tx_init_data_ring_rh(hal_soc_handle_t hal_soc_hdl,
				     hal_ring_handle_t hal_ring_hdl)
{
}

static void hal_get_ba_aging_timeout_rh(hal_soc_handle_t hal_soc_hdl,
					uint8_t ac, uint32_t *value)
{
}

static void hal_set_ba_aging_timeout_rh(hal_soc_handle_t hal_soc_hdl,
					uint8_t ac, uint32_t value)
{
}

static uint32_t hal_get_reo_reg_base_offset_rh(void)
{
	return 0;
}

static void hal_rx_reo_buf_paddr_get_rh(hal_ring_desc_t rx_desc,
					struct hal_buf_info *buf_info)
{
}

static void
hal_rx_msdu_link_desc_set_rh(hal_soc_handle_t hal_soc_hdl,
			     void *src_srng_desc,
			     hal_buff_addrinfo_t buf_addr_info,
			     uint8_t bm_action)
{
}

static uint8_t hal_rx_ret_buf_manager_get_rh(hal_ring_desc_t ring_desc)
{
	return HAL_RX_BUF_RBM_GET(ring_desc);
}

static
void hal_rx_buf_cookie_rbm_get_rh(uint32_t *buf_addr_info_hdl,
				  hal_buf_info_t buf_info_hdl)
{
	struct hal_buf_info *buf_info =
		(struct hal_buf_info *)buf_info_hdl;
	struct buffer_addr_info *buf_addr_info =
		(struct buffer_addr_info *)buf_addr_info_hdl;

	buf_info->sw_cookie = HAL_RX_BUF_COOKIE_GET(buf_addr_info);
	/*
	 * buffer addr info is the first member of ring desc, so the typecast
	 * can be done.
	 */
	buf_info->rbm =
		hal_rx_ret_buf_manager_get_rh((hal_ring_desc_t)buf_addr_info);
}

static uint32_t hal_rx_get_reo_error_code_rh(hal_ring_desc_t rx_desc)
{
	return 0;
}

static uint32_t
hal_gen_reo_remap_val_generic_rh(enum hal_reo_remap_reg remap_reg,
				 uint8_t *ix0_map)
{
	return 0;
}

static void hal_rx_mpdu_desc_info_get_rh(void *desc_addr,
					 void *mpdu_desc_info_hdl)
{
}

static uint8_t hal_rx_err_status_get_rh(hal_ring_desc_t rx_desc)
{
	return 0;
}

static uint8_t hal_rx_reo_buf_type_get_rh(hal_ring_desc_t rx_desc)
{
	return 0;
}

static uint32_t hal_rx_wbm_err_src_get_rh(hal_ring_desc_t ring_desc)
{
	return 0;
}

static void
hal_rx_wbm_rel_buf_paddr_get_rh(hal_ring_desc_t rx_desc,
				struct hal_buf_info *buf_info)
{
}

static int hal_reo_send_cmd_rh(hal_soc_handle_t hal_soc_hdl,
			       hal_ring_handle_t  hal_ring_hdl,
			       enum hal_reo_cmd_type cmd,
			       void *params)
{
	return 0;
}

static void
hal_reo_qdesc_setup_rh(hal_soc_handle_t hal_soc_hdl, int tid,
		       uint32_t ba_window_size,
		       uint32_t start_seq, void *hw_qdesc_vaddr,
		       qdf_dma_addr_t hw_qdesc_paddr,
		       int pn_type, uint8_t vdev_stats_id)
{
}

static inline uint32_t
hal_rx_msdu_reo_dst_ind_get_rh(hal_soc_handle_t hal_soc_hdl,
			       void *msdu_link_desc)
{
	return 0;
}

static inline void
hal_msdu_desc_info_set_rh(hal_soc_handle_t hal_soc_hdl,
			  void *msdu_desc, uint32_t dst_ind,
			  uint32_t nbuf_len)
{
}

static inline void
hal_mpdu_desc_info_set_rh(hal_soc_handle_t hal_soc_hdl, void *ent_desc,
			  void *mpdu_desc, uint32_t seq_no)
{
}

static QDF_STATUS hal_reo_status_update_rh(hal_soc_handle_t hal_soc_hdl,
					   hal_ring_desc_t reo_desc,
					   void *st_handle,
					   uint32_t tlv, int *num_ref)
{
	return QDF_STATUS_SUCCESS;
}

static uint8_t hal_get_tlv_hdr_size_rh(void)
{
	return sizeof(struct tlv_32_hdr);
}

static inline
uint8_t *hal_get_reo_ent_desc_qdesc_addr_rh(uint8_t *desc)
{
	return 0;
}

static inline
void hal_set_reo_ent_desc_reo_dest_ind_rh(uint8_t *desc,
					  uint32_t dst_ind)
{
}

static uint64_t hal_rx_get_qdesc_addr_rh(uint8_t *dst_ring_desc,
					 uint8_t *buf)
{
	return 0;
}

static uint8_t hal_get_idle_link_bm_id_rh(uint8_t chip_id)
{
	return 0;
}
/*
 * hal_rx_msdu_is_wlan_mcast_generic_rh(): Check if the buffer is for multicast
 *					address
 * @nbuf: Network buffer
 *
 * Returns: flag to indicate whether the nbuf has MC/BC address
 */
static uint32_t hal_rx_msdu_is_wlan_mcast_generic_rh(qdf_nbuf_t nbuf)
{
	uint8_t *buf = qdf_nbuf_data(nbuf);

	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;

	return rx_attn->mcast_bcast;
}

/**
 * hal_rx_tlv_decap_format_get_rh() - Get packet decap format from the TLV
 * @hw_desc_addr: rx tlv desc
 *
 * Return: pkt decap format
 */
static uint32_t hal_rx_tlv_decap_format_get_rh(void *hw_desc_addr)
{
	struct rx_msdu_start *rx_msdu_start;
	struct rx_pkt_tlvs *rx_desc = (struct rx_pkt_tlvs *)hw_desc_addr;

	rx_msdu_start = &rx_desc->msdu_start_tlv.rx_msdu_start;

	return HAL_RX_GET(rx_msdu_start, RX_MSDU_START_2, DECAP_FORMAT);
}

/**
 * hal_rx_dump_pkt_tlvs_rh(): API to print all member elements of
 *			 RX TLVs
 * @hal_soc_hdl: HAL SOC handle
 * @buf: pointer the pkt buffer
 * @dbg_level: log level
 *
 * Return: void
 */
static void hal_rx_dump_pkt_tlvs_rh(hal_soc_handle_t hal_soc_hdl,
				    uint8_t *buf, uint8_t dbg_level)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_rx_dump_msdu_end_tlv(hal_soc, pkt_tlvs, dbg_level);
	hal_rx_dump_rx_attention_tlv(hal_soc, pkt_tlvs, dbg_level);
	hal_rx_dump_msdu_start_tlv(hal_soc, pkt_tlvs, dbg_level);
	hal_rx_dump_mpdu_start_tlv(hal_soc, pkt_tlvs, dbg_level);
	hal_rx_dump_mpdu_end_tlv(hal_soc, pkt_tlvs, dbg_level);
	hal_rx_dump_pkt_hdr_tlv(hal_soc, pkt_tlvs, dbg_level);
}

/**
 * hal_rx_tlv_get_offload_info_rh() - Get the offload info from TLV
 * @rx_tlv: RX tlv start address in buffer
 * @offload_info: Buffer to store the offload info
 *
 * Return: 0 on success, -EINVAL on failure.
 */
static int
hal_rx_tlv_get_offload_info_rh(uint8_t *rx_tlv,
			       struct hal_offload_info *offload_info)
{
	offload_info->flow_id = HAL_RX_TLV_GET_FLOW_ID_TOEPLITZ(rx_tlv);
	offload_info->ipv6_proto = HAL_RX_TLV_GET_IPV6(rx_tlv);
	offload_info->lro_eligible = HAL_RX_TLV_GET_LRO_ELIGIBLE(rx_tlv);
	offload_info->tcp_proto = HAL_RX_TLV_GET_TCP_PROTO(rx_tlv);

	if (offload_info->tcp_proto) {
		offload_info->tcp_pure_ack =
					HAL_RX_TLV_GET_TCP_PURE_ACK(rx_tlv);
		offload_info->tcp_offset = HAL_RX_TLV_GET_TCP_OFFSET(rx_tlv);
		offload_info->tcp_win = HAL_RX_TLV_GET_TCP_WIN(rx_tlv);
		offload_info->tcp_seq_num = HAL_RX_TLV_GET_TCP_SEQ(rx_tlv);
		offload_info->tcp_ack_num = HAL_RX_TLV_GET_TCP_ACK(rx_tlv);
	}
	return 0;
}

/*
 * hal_rx_attn_phy_ppdu_id_get_rh(): get phy_ppdu_id value
 * from rx attention
 * @buf: pointer to rx_pkt_tlvs
 *
 * Return: phy_ppdu_id
 */
static uint16_t hal_rx_attn_phy_ppdu_id_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint16_t phy_ppdu_id;

	phy_ppdu_id = HAL_RX_ATTN_PHY_PPDU_ID_GET(rx_attn);

	return phy_ppdu_id;
}

/**
 * hal_rx_msdu_start_msdu_len_get_rh(): API to get the MSDU length
 * from rx_msdu_start TLV
 *
 * @buf: pointer to the start of RX PKT TLV headers
 *
 * Return: msdu length
 */
static uint32_t hal_rx_msdu_start_msdu_len_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
			&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	uint32_t msdu_len;

	msdu_len = HAL_RX_MSDU_START_MSDU_LEN_GET(msdu_start);

	return msdu_len;
}

/**
 * hal_rx_get_proto_params_rh() - Get l4 proto values from TLV
 * @buf: rx tlv address
 * @proto_params: Buffer to store proto parameters
 *
 * Return: 0 on success.
 */
static int hal_rx_get_proto_params_rh(uint8_t *buf, void *proto_params)
{
	struct hal_proto_params *param =
				(struct hal_proto_params *)proto_params;

	param->tcp_proto = HAL_RX_TLV_GET_TCP_PROTO(buf);
	param->udp_proto = HAL_RX_TLV_GET_UDP_PROTO(buf);
	param->ipv6_proto = HAL_RX_TLV_GET_IPV6(buf);

	return 0;
}

/**
 * hal_rx_get_l3_l4_offsets_rh() - Get l3/l4 header offset from TLV
 * @buf: rx tlv start address
 * @l3_hdr_offset: buffer to store l3 offset
 * @l4_hdr_offset: buffer to store l4 offset
 *
 * Return: 0 on success.
 */
static int hal_rx_get_l3_l4_offsets_rh(uint8_t *buf, uint32_t *l3_hdr_offset,
				       uint32_t *l4_hdr_offset)
{
	*l3_hdr_offset = HAL_RX_TLV_GET_IP_OFFSET(buf);
	*l4_hdr_offset = HAL_RX_TLV_GET_TCP_OFFSET(buf);

	return 0;
}

/**
 * hal_rx_tlv_get_pn_num_rh() - Get packet number from RX TLV
 * @buf: rx tlv address
 * @pn_num: buffer to store packet number
 *
 * Return: None
 */
static inline void hal_rx_tlv_get_pn_num_rh(uint8_t *buf, uint64_t *pn_num)
{
	struct rx_pkt_tlvs *rx_pkt_tlv =
			(struct rx_pkt_tlvs *)buf;
	struct rx_mpdu_info *rx_mpdu_info_details =
	 &rx_pkt_tlv->mpdu_start_tlv.rx_mpdu_start.rx_mpdu_info_details;

	pn_num[0] = rx_mpdu_info_details->pn_31_0;
	pn_num[0] |=
		((uint64_t)rx_mpdu_info_details->pn_63_32 << 32);
	pn_num[1] = rx_mpdu_info_details->pn_95_64;
	pn_num[1] |=
		((uint64_t)rx_mpdu_info_details->pn_127_96 << 32);
}

#ifdef NO_RX_PKT_HDR_TLV
/**
 * hal_rx_pkt_hdr_get_rh() - Get rx packet header start address.
 * @buf: packet start address
 *
 * Return: packet data start address.
 */
static inline uint8_t *hal_rx_pkt_hdr_get_rh(uint8_t *buf)
{
	return buf + RX_PKT_TLVS_LEN;
}
#else
static inline uint8_t *hal_rx_pkt_hdr_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;

	return pkt_tlvs->pkt_hdr_tlv.rx_pkt_hdr;
}
#endif

/**
 * hal_rx_priv_info_set_in_tlv_rh(): Save the private info to
 *				the reserved bytes of rx_tlv_hdr
 * @buf: start of rx_tlv_hdr
 * @priv_data: hal_wbm_err_desc_info structure
 * @len: length of the private data
 * Return: void
 */
static inline void
hal_rx_priv_info_set_in_tlv_rh(uint8_t *buf, uint8_t *priv_data,
			       uint32_t len)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	uint32_t copy_len = (len > RX_PADDING0_BYTES) ?
			    RX_PADDING0_BYTES : len;

	qdf_mem_copy(pkt_tlvs->rx_padding0, priv_data, copy_len);
}

/**
 * hal_rx_priv_info_get_from_tlv_rh(): retrieve the private data from
 *				the reserved bytes of rx_tlv_hdr.
 * @buf: start of rx_tlv_hdr
 * @priv_data: hal_wbm_err_desc_info structure
 * @len: length of the private data
 * Return: void
 */
static inline void
hal_rx_priv_info_get_from_tlv_rh(uint8_t *buf, uint8_t *priv_data,
				 uint32_t len)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	uint32_t copy_len = (len > RX_PADDING0_BYTES) ?
			    RX_PADDING0_BYTES : len;

	qdf_mem_copy(priv_data, pkt_tlvs->rx_padding0, copy_len);
}

/**
 * hal_rx_get_tlv_size_generic_rh() - Get rx packet tlv size
 * @rx_pkt_tlv_size: TLV size for regular RX packets
 * @rx_mon_pkt_tlv_size: TLV size for monitor mode packets
 *
 * Return: size of rx pkt tlv before the actual data
 */
static void hal_rx_get_tlv_size_generic_rh(uint16_t *rx_pkt_tlv_size,
					   uint16_t *rx_mon_pkt_tlv_size)
{
	*rx_pkt_tlv_size = RX_PKT_TLVS_LEN;
	*rx_mon_pkt_tlv_size = SIZE_OF_MONITOR_TLV;
}

/*
 * hal_rxdma_buff_addr_info_set() - set the buffer_addr_info of the
 *				    rxdma ring entry.
 * @rxdma_entry: descriptor entry
 * @paddr: physical address of nbuf data pointer.
 * @cookie: SW cookie used as a index to SW rx desc.
 * @manager: who owns the nbuf (host, NSS, etc...).
 *
 */
static void
hal_rxdma_buff_addr_info_set_rh(void *rxdma_entry, qdf_dma_addr_t paddr,
				uint32_t cookie, uint8_t manager)
{
	uint32_t paddr_lo = ((u64)paddr & 0x00000000ffffffff);
	uint32_t paddr_hi = ((u64)paddr & 0xffffffff00000000) >> 32;

	HAL_RXDMA_PADDR_LO_SET(rxdma_entry, paddr_lo);
	HAL_RXDMA_PADDR_HI_SET(rxdma_entry, paddr_hi);
	HAL_RXDMA_COOKIE_SET(rxdma_entry, cookie);
	HAL_RXDMA_MANAGER_SET(rxdma_entry, manager);
}

/**
 * hal_rx_tlv_csum_err_get_rh() - Get IP and tcp-udp checksum fail flag
 * @rx_tlv_hdr: start address of rx_tlv_hdr
 * @ip_csum_err: buffer to return ip_csum_fail flag
 * @tcp_udp_csum_err: placeholder to return tcp-udp checksum fail flag
 *
 * Return: None
 */
static inline void
hal_rx_tlv_csum_err_get_rh(uint8_t *rx_tlv_hdr, uint32_t *ip_csum_err,
			   uint32_t *tcp_udp_csum_err)
{
	*ip_csum_err = hal_rx_attn_ip_cksum_fail_get(rx_tlv_hdr);
	*tcp_udp_csum_err = hal_rx_attn_tcp_udp_cksum_fail_get(rx_tlv_hdr);
}

static void
hal_rx_tlv_get_pkt_capture_flags_rh(uint8_t *rx_tlv_pkt_hdr,
				    struct hal_rx_pkt_capture_flags *flags)
{
	struct rx_pkt_tlvs *rx_tlv_hdr = (struct rx_pkt_tlvs *)rx_tlv_pkt_hdr;
	struct rx_attention *rx_attn = &rx_tlv_hdr->attn_tlv.rx_attn;
	struct rx_mpdu_start *mpdu_start =
				&rx_tlv_hdr->mpdu_start_tlv.rx_mpdu_start;
	struct rx_mpdu_end *mpdu_end = &rx_tlv_hdr->mpdu_end_tlv.rx_mpdu_end;
	struct rx_msdu_start *msdu_start =
				&rx_tlv_hdr->msdu_start_tlv.rx_msdu_start;

	flags->encrypt_type = mpdu_start->rx_mpdu_info_details.encrypt_type;
	flags->fcs_err = mpdu_end->fcs_err;
	flags->fragment_flag = rx_attn->fragment_flag;
	flags->chan_freq = HAL_RX_MSDU_START_FREQ_GET(msdu_start);
	flags->rssi_comb = HAL_RX_MSDU_START_RSSI_GET(msdu_start);
	flags->tsft = msdu_start->ppdu_start_timestamp;
}

static inline bool
hal_rx_mpdu_info_ampdu_flag_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_mpdu_start *mpdu_start =
				 &pkt_tlvs->mpdu_start_tlv.rx_mpdu_start;

	struct rx_mpdu_info *mpdu_info = &mpdu_start->rx_mpdu_info_details;
	bool ampdu_flag;

	ampdu_flag = HAL_RX_MPDU_INFO_AMPDU_FLAG_GET(mpdu_info);

	return ampdu_flag;
}

static
uint32_t hal_rx_tlv_mpdu_len_err_get_rh(void *hw_desc_addr)
{
	struct rx_attention *rx_attn;
	struct rx_mon_pkt_tlvs *rx_desc =
		(struct rx_mon_pkt_tlvs *)hw_desc_addr;

	rx_attn = &rx_desc->attn_tlv.rx_attn;

	return HAL_RX_GET(rx_attn, RX_ATTENTION_1, MPDU_LENGTH_ERR);
}

static
uint32_t hal_rx_tlv_mpdu_fcs_err_get_rh(void *hw_desc_addr)
{
	struct rx_attention *rx_attn;
	struct rx_mon_pkt_tlvs *rx_desc =
		(struct rx_mon_pkt_tlvs *)hw_desc_addr;

	rx_attn = &rx_desc->attn_tlv.rx_attn;

	return HAL_RX_GET(rx_attn, RX_ATTENTION_1, FCS_ERR);
}

#ifdef NO_RX_PKT_HDR_TLV
static uint8_t *hal_rx_desc_get_80211_hdr_rh(void *hw_desc_addr)
{
	uint8_t *rx_pkt_hdr;
	struct rx_mon_pkt_tlvs *rx_desc =
		(struct rx_mon_pkt_tlvs *)hw_desc_addr;

	rx_pkt_hdr = &rx_desc->pkt_hdr_tlv.rx_pkt_hdr[0];

	return rx_pkt_hdr;
}
#else
static uint8_t *hal_rx_desc_get_80211_hdr_rh(void *hw_desc_addr)
{
	uint8_t *rx_pkt_hdr;

	struct rx_pkt_tlvs *rx_desc = (struct rx_pkt_tlvs *)hw_desc_addr;

	rx_pkt_hdr = &rx_desc->pkt_hdr_tlv.rx_pkt_hdr[0];

	return rx_pkt_hdr;
}
#endif

static uint32_t hal_rx_hw_desc_mpdu_user_id_rh(void *hw_desc_addr)
{
	struct rx_mon_pkt_tlvs *rx_desc =
		(struct rx_mon_pkt_tlvs *)hw_desc_addr;
	uint32_t user_id;

	user_id = HAL_RX_GET_USER_TLV32_USERID(
		&rx_desc->mpdu_start_tlv);

	return user_id;
}

/**
 * hal_rx_msdu_start_msdu_len_set_rh(): API to set the MSDU length
 * from rx_msdu_start TLV
 *
 * @buf: pointer to the start of RX PKT TLV headers
 * @len: msdu length
 *
 * Return: none
 */
static inline void
hal_rx_msdu_start_msdu_len_set_rh(uint8_t *buf, uint32_t len)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
			&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	void *wrd1;

	wrd1 = (uint8_t *)msdu_start + RX_MSDU_START_1_MSDU_LENGTH_OFFSET;
	*(uint32_t *)wrd1 &= (~RX_MSDU_START_1_MSDU_LENGTH_MASK);
	*(uint32_t *)wrd1 |= len;
}

/*
 * hal_rx_tlv_bw_get_rh(): API to get the Bandwidth
 * Interval from rx_msdu_start
 *
 * @buf: pointer to the start of RX PKT TLV header
 * Return: uint32_t(bw)
 */
static inline uint32_t hal_rx_tlv_bw_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
		&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	uint32_t bw;

	bw = HAL_RX_MSDU_START_BW_GET(msdu_start);

	return bw;
}

/*
 * hal_rx_tlv_get_freq_rh(): API to get the frequency of operating channel
 * from rx_msdu_start
 *
 * @buf: pointer to the start of RX PKT TLV header
 * Return: uint32_t(frequency)
 */
static inline uint32_t
hal_rx_tlv_get_freq_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
		&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	uint32_t freq;

	freq = HAL_RX_MSDU_START_FREQ_GET(msdu_start);

	return freq;
}

/**
 * hal_rx_tlv_sgi_get_rh(): API to get the Short Guard
 * Interval from rx_msdu_start TLV
 *
 * @buf: pointer to the start of RX PKT TLV headers
 * Return: uint32_t(sgi)
 */
static inline uint32_t
hal_rx_tlv_sgi_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
		&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	uint32_t sgi;

	sgi = HAL_RX_MSDU_START_SGI_GET(msdu_start);

	return sgi;
}

/**
 * hal_rx_tlv_rate_mcs_get_rh(): API to get the MCS rate
 * from rx_msdu_start TLV
 *
 * @buf: pointer to the start of RX PKT TLV headers
 * Return: uint32_t(rate_mcs)
 */
static inline uint32_t
hal_rx_tlv_rate_mcs_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
		&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	uint32_t rate_mcs;

	rate_mcs = HAL_RX_MSDU_START_RATE_MCS_GET(msdu_start);

	return rate_mcs;
}

/*
 * hal_rx_tlv_get_pkt_type_rh(): API to get the pkt type
 * from rx_msdu_start
 *
 * @buf: pointer to the start of RX PKT TLV header
 * Return: uint32_t(pkt type)
 */

static inline uint32_t hal_rx_tlv_get_pkt_type_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_start *msdu_start =
		&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	uint32_t pkt_type;

	pkt_type = HAL_RX_MSDU_START_PKT_TYPE_GET(msdu_start);

	return pkt_type;
}

/**
 * hal_rx_tlv_mic_err_get_rh(): API to get the MIC ERR
 * from rx_mpdu_end TLV
 *
 * @buf: pointer to the start of RX PKT TLV headers
 * Return: uint32_t(mic_err)
 */
static inline uint32_t
hal_rx_tlv_mic_err_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_mpdu_end *mpdu_end =
		&pkt_tlvs->mpdu_end_tlv.rx_mpdu_end;
	uint32_t mic_err;

	mic_err = HAL_RX_MPDU_END_MIC_ERR_GET(mpdu_end);

	return mic_err;
}

/**
 * hal_rx_tlv_decrypt_err_get_rh(): API to get the Decrypt ERR
 * from rx_mpdu_end TLV
 *
 * @buf: pointer to the start of RX PKT TLV headers
 * Return: uint32_t(decrypt_err)
 */
static inline uint32_t
hal_rx_tlv_decrypt_err_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_mpdu_end *mpdu_end =
		&pkt_tlvs->mpdu_end_tlv.rx_mpdu_end;
	uint32_t decrypt_err;

	decrypt_err = HAL_RX_MPDU_END_DECRYPT_ERR_GET(mpdu_end);

	return decrypt_err;
}

/*
 * hal_rx_tlv_first_mpdu_get_rh(): get fist_mpdu bit from rx attention
 * @buf: pointer to rx_pkt_tlvs
 *
 * reutm: uint32_t(first_msdu)
 */
static inline uint32_t
hal_rx_tlv_first_mpdu_get_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint32_t first_mpdu;

	first_mpdu = HAL_RX_ATTN_FIRST_MPDU_GET(rx_attn);

	return first_mpdu;
}

/*
 * hal_rx_msdu_get_keyid_rh(): API to get the key id if the decrypted packet
 * from rx_msdu_end
 *
 * @buf: pointer to the start of RX PKT TLV header
 * Return: uint32_t(key id)
 */
static inline uint8_t
hal_rx_msdu_get_keyid_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_msdu_end *msdu_end = &pkt_tlvs->msdu_end_tlv.rx_msdu_end;
	uint32_t keyid_octet;

	keyid_octet = HAL_RX_MSDU_END_KEYID_OCTET_GET(msdu_end);

	return keyid_octet & 0x3;
}

/*
 * hal_rx_tlv_get_is_decrypted_rh(): API to get the decrypt status of the
 *  packet from rx_attention
 *
 * @buf: pointer to the start of RX PKT TLV header
 * Return: uint32_t(decryt status)
 */
static inline uint32_t
hal_rx_tlv_get_is_decrypted_rh(uint8_t *buf)
{
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)buf;
	struct rx_attention *rx_attn = &pkt_tlvs->attn_tlv.rx_attn;
	uint32_t is_decrypt = 0;
	uint32_t decrypt_status;

	decrypt_status = HAL_RX_ATTN_DECRYPT_STATUS_GET(rx_attn);

	if (!decrypt_status)
		is_decrypt = 1;

	return is_decrypt;
}

static inline uint8_t hal_rx_get_phy_ppdu_id_size_rh(void)
{
	return sizeof(uint32_t);
}

/**
 * hal_hw_txrx_default_ops_attach_rh() - Attach the default hal ops for
 *		Rh arch chipsets.
 * @hal_soc: HAL soc handle
 *
 * Return: None
 */
void hal_hw_txrx_default_ops_attach_rh(struct hal_soc *hal_soc)
{
	hal_soc->ops->hal_get_reo_qdesc_size = hal_get_reo_qdesc_size_rh;
	hal_soc->ops->hal_get_rx_max_ba_window =
					hal_get_rx_max_ba_window_rh;
	hal_soc->ops->hal_set_link_desc_addr = hal_set_link_desc_addr_rh;
	hal_soc->ops->hal_tx_init_data_ring = hal_tx_init_data_ring_rh;
	hal_soc->ops->hal_get_ba_aging_timeout = hal_get_ba_aging_timeout_rh;
	hal_soc->ops->hal_set_ba_aging_timeout = hal_set_ba_aging_timeout_rh;
	hal_soc->ops->hal_get_reo_reg_base_offset =
					hal_get_reo_reg_base_offset_rh;
	hal_soc->ops->hal_rx_get_tlv_size = hal_rx_get_tlv_size_generic_rh;
	hal_soc->ops->hal_rx_msdu_is_wlan_mcast =
					hal_rx_msdu_is_wlan_mcast_generic_rh;
	hal_soc->ops->hal_rx_tlv_decap_format_get =
					hal_rx_tlv_decap_format_get_rh;
	hal_soc->ops->hal_rx_dump_pkt_tlvs = hal_rx_dump_pkt_tlvs_rh;
	hal_soc->ops->hal_rx_tlv_get_offload_info =
					hal_rx_tlv_get_offload_info_rh;
	hal_soc->ops->hal_rx_tlv_phy_ppdu_id_get =
					hal_rx_attn_phy_ppdu_id_get_rh;
	hal_soc->ops->hal_rx_tlv_msdu_done_get = hal_rx_attn_msdu_done_get_rh;
	hal_soc->ops->hal_rx_tlv_msdu_len_get =
					hal_rx_msdu_start_msdu_len_get_rh;
	hal_soc->ops->hal_rx_get_proto_params = hal_rx_get_proto_params_rh;
	hal_soc->ops->hal_rx_get_l3_l4_offsets = hal_rx_get_l3_l4_offsets_rh;

	hal_soc->ops->hal_rx_reo_buf_paddr_get = hal_rx_reo_buf_paddr_get_rh;
	hal_soc->ops->hal_rx_msdu_link_desc_set = hal_rx_msdu_link_desc_set_rh;
	hal_soc->ops->hal_rx_buf_cookie_rbm_get = hal_rx_buf_cookie_rbm_get_rh;
	hal_soc->ops->hal_rx_ret_buf_manager_get =
					hal_rx_ret_buf_manager_get_rh;
	hal_soc->ops->hal_rxdma_buff_addr_info_set =
					hal_rxdma_buff_addr_info_set_rh;
	hal_soc->ops->hal_rx_msdu_flags_get = hal_rx_msdu_flags_get_rh;
	hal_soc->ops->hal_rx_get_reo_error_code = hal_rx_get_reo_error_code_rh;
	hal_soc->ops->hal_gen_reo_remap_val =
					hal_gen_reo_remap_val_generic_rh;
	hal_soc->ops->hal_rx_tlv_csum_err_get =
					hal_rx_tlv_csum_err_get_rh;
	hal_soc->ops->hal_rx_mpdu_desc_info_get =
				hal_rx_mpdu_desc_info_get_rh;
	hal_soc->ops->hal_rx_err_status_get = hal_rx_err_status_get_rh;
	hal_soc->ops->hal_rx_reo_buf_type_get = hal_rx_reo_buf_type_get_rh;
	hal_soc->ops->hal_rx_pkt_hdr_get = hal_rx_pkt_hdr_get_rh;
	hal_soc->ops->hal_rx_wbm_err_src_get = hal_rx_wbm_err_src_get_rh;
	hal_soc->ops->hal_rx_wbm_rel_buf_paddr_get =
					hal_rx_wbm_rel_buf_paddr_get_rh;
	hal_soc->ops->hal_rx_priv_info_set_in_tlv =
					hal_rx_priv_info_set_in_tlv_rh;
	hal_soc->ops->hal_rx_priv_info_get_from_tlv =
					hal_rx_priv_info_get_from_tlv_rh;
	hal_soc->ops->hal_rx_mpdu_info_ampdu_flag_get =
					hal_rx_mpdu_info_ampdu_flag_get_rh;
	hal_soc->ops->hal_rx_tlv_mpdu_len_err_get =
					hal_rx_tlv_mpdu_len_err_get_rh;
	hal_soc->ops->hal_rx_tlv_mpdu_fcs_err_get =
					hal_rx_tlv_mpdu_fcs_err_get_rh;
	hal_soc->ops->hal_reo_send_cmd = hal_reo_send_cmd_rh;
	hal_soc->ops->hal_rx_tlv_get_pkt_capture_flags =
					hal_rx_tlv_get_pkt_capture_flags_rh;
	hal_soc->ops->hal_rx_desc_get_80211_hdr = hal_rx_desc_get_80211_hdr_rh;
	hal_soc->ops->hal_rx_hw_desc_mpdu_user_id =
					hal_rx_hw_desc_mpdu_user_id_rh;
	hal_soc->ops->hal_reo_qdesc_setup = hal_reo_qdesc_setup_rh;
	hal_soc->ops->hal_rx_tlv_msdu_len_set =
					hal_rx_msdu_start_msdu_len_set_rh;
	hal_soc->ops->hal_rx_tlv_bw_get = hal_rx_tlv_bw_get_rh;
	hal_soc->ops->hal_rx_tlv_get_freq = hal_rx_tlv_get_freq_rh;
	hal_soc->ops->hal_rx_tlv_sgi_get = hal_rx_tlv_sgi_get_rh;
	hal_soc->ops->hal_rx_tlv_rate_mcs_get = hal_rx_tlv_rate_mcs_get_rh;
	hal_soc->ops->hal_rx_tlv_get_pkt_type = hal_rx_tlv_get_pkt_type_rh;
	hal_soc->ops->hal_rx_tlv_get_pn_num = hal_rx_tlv_get_pn_num_rh;
	hal_soc->ops->hal_rx_tlv_mic_err_get = hal_rx_tlv_mic_err_get_rh;
	hal_soc->ops->hal_rx_tlv_decrypt_err_get =
					hal_rx_tlv_decrypt_err_get_rh;
	hal_soc->ops->hal_rx_tlv_first_mpdu_get = hal_rx_tlv_first_mpdu_get_rh;
	hal_soc->ops->hal_rx_tlv_get_is_decrypted =
					hal_rx_tlv_get_is_decrypted_rh;
	hal_soc->ops->hal_rx_msdu_get_keyid = hal_rx_msdu_get_keyid_rh;
	hal_soc->ops->hal_rx_msdu_reo_dst_ind_get =
					hal_rx_msdu_reo_dst_ind_get_rh;
	hal_soc->ops->hal_msdu_desc_info_set = hal_msdu_desc_info_set_rh;
	hal_soc->ops->hal_mpdu_desc_info_set = hal_mpdu_desc_info_set_rh;
	hal_soc->ops->hal_reo_status_update = hal_reo_status_update_rh;
	hal_soc->ops->hal_get_tlv_hdr_size = hal_get_tlv_hdr_size_rh;
	hal_soc->ops->hal_get_reo_ent_desc_qdesc_addr =
				hal_get_reo_ent_desc_qdesc_addr_rh;
	hal_soc->ops->hal_rx_get_qdesc_addr = hal_rx_get_qdesc_addr_rh;
	hal_soc->ops->hal_set_reo_ent_desc_reo_dest_ind =
				hal_set_reo_ent_desc_reo_dest_ind_rh;
	hal_soc->ops->hal_get_idle_link_bm_id = hal_get_idle_link_bm_id_rh;
	hal_soc->ops->hal_rx_get_phy_ppdu_id_size =
					hal_rx_get_phy_ppdu_id_size_rh;
}

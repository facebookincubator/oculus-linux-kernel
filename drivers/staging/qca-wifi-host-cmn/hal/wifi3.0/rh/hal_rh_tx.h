/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#ifndef _HAL_RH_TX_H_
#define _HAL_RH_TX_H_

enum hal_rh_tx_ret_buf_manager {
	HAL_RH_WBM_SW0_BM_ID = 3,
	HAL_RH_WBM_SW1_BM_ID = 4,
	HAL_RH_WBM_SW2_BM_ID = 5,
	HAL_RH_WBM_SW3_BM_ID = 6,
	HAL_RH_WBM_SW4_BM_ID = 7,
};

/*---------------------------------------------------------------------------
 * Function declarations and documentation
 * ---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 * TCL Descriptor accessor APIs
 * ---------------------------------------------------------------------------
 */

/**
 * hal_tx_desc_set_buf_addr - Fill Buffer Address information in Tx Descriptor
 * @hal_soc_hdl: HAL SOC handle
 * @desc: Handle to Tx Descriptor
 * @paddr: Physical Address
 * @pool_id: Return Buffer Manager ID
 * @desc_id: Descriptor ID
 * @type: 0 - Address points to a MSDU buffer
 *		1 - Address points to MSDU extension descriptor
 *
 * Return: void
 */
static inline
void hal_tx_desc_set_buf_addr(hal_soc_handle_t hal_soc_hdl, void *desc,
			      dma_addr_t paddr,
			      uint8_t pool_id, uint32_t desc_id,
			      uint8_t type)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_desc_set_buf_addr(desc, paddr, pool_id,
						desc_id, type);
}

/**
 * hal_tx_desc_set_lmac_id - Set the lmac_id value
 * @hal_soc_hdl: HAL SOC handle
 * @desc: Handle to Tx Descriptor
 * @lmac_id: mac Id to ast matching
 *                     b00 – mac 0
 *                     b01 – mac 1
 *                     b10 – mac 2
 *                     b11 – all macs (legacy HK way)
 *
 * Return: void
 */
static inline void hal_tx_desc_set_lmac_id(hal_soc_handle_t hal_soc_hdl,
					   void *desc, uint8_t lmac_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_desc_set_lmac_id(desc, lmac_id);
}

/**
 * hal_tx_desc_set_search_type - Set the search type value
 * @hal_soc_hdl: HAL SOC handle
 * @desc: Handle to Tx Descriptor
 * @search_type: search type
 *		     0 – Normal search
 *		     1 – Index based address search
 *		     2 – Index based flow search
 *
 * Return: void
 */
static inline void hal_tx_desc_set_search_type(hal_soc_handle_t hal_soc_hdl,
					       void *desc,
					       uint8_t search_type)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_desc_set_search_type(desc, search_type);
}

/**
 * hal_tx_desc_set_search_index - Set the search index value
 * @hal_soc_hdl: HAL SOC handle
 * @desc: Handle to Tx Descriptor
 * @search_index: The index that will be used for index based address or
 *                flow search. The field is valid when 'search_type' is
 *                1 0r 2
 *
 * Return: void
 */
static inline void hal_tx_desc_set_search_index(hal_soc_handle_t hal_soc_hdl,
						void *desc,
						uint32_t search_index)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_desc_set_search_index(desc, search_index);
}

/**
 * hal_tx_desc_set_cache_set_num - Set the cache-set-num value
 * @hal_soc_hdl: HAL SOC handle
 * @desc: Handle to Tx Descriptor
 * @cache_num: Cache set number that should be used to cache the index
 *                based search results, for address and flow search.
 *                This value should be equal to LSB four bits of the hash value
 *                of match data, in case of search index points to an entry
 *                which may be used in content based search also. The value can
 *                be anything when the entry pointed by search index will not be
 *                used for content based search.
 *
 * Return: void
 */
static inline void hal_tx_desc_set_cache_set_num(hal_soc_handle_t hal_soc_hdl,
						 void *desc,
						 uint8_t cache_num)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_desc_set_cache_set_num(desc, cache_num);
}

/**
 * hal_tx_desc_set_buf_length - Set Data length in bytes in Tx Descriptor
 * @desc: Handle to Tx Descriptor
 * @data_length: MSDU length in case of direct descriptor.
 *              Length of link extension descriptor in case of Link extension
 *              descriptor.Includes the length of Metadata
 * Return: None
 */
static inline void  hal_tx_desc_set_buf_length(void *desc,
					       uint16_t data_length)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_3, DATA_LENGTH) |=
		HAL_TX_SM(TCL_DATA_CMD_3, DATA_LENGTH, data_length);
}

/**
 * hal_tx_desc_set_buf_offset - Sets Packet Offset field in Tx descriptor
 * @desc: Handle to Tx Descriptor
 * @offset: Packet offset from Metadata in case of direct buffer descriptor.
 *
 * Return: void
 */
static inline void hal_tx_desc_set_buf_offset(void *desc,
					      uint8_t offset)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_3, PACKET_OFFSET) |=
		HAL_TX_SM(TCL_DATA_CMD_3, PACKET_OFFSET, offset);
}

/**
 * hal_tx_desc_set_encap_type - Set encapsulation type in Tx Descriptor
 * @desc: Handle to Tx Descriptor
 * @encap_type: Encapsulation that HW will perform
 *
 * Return: void
 *
 */
static inline void hal_tx_desc_set_encap_type(void *desc,
					      enum hal_tx_encap_type encap_type)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_2, ENCAP_TYPE) |=
		HAL_TX_SM(TCL_DATA_CMD_2, ENCAP_TYPE, encap_type);
}

/**
 * hal_tx_desc_set_encrypt_type - Sets the Encrypt Type in Tx Descriptor
 * @desc: Handle to Tx Descriptor
 * @type: Encrypt Type
 *
 * Return: void
 */
static inline void hal_tx_desc_set_encrypt_type(void *desc,
						enum hal_tx_encrypt_type type)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_2, ENCRYPT_TYPE) |=
		HAL_TX_SM(TCL_DATA_CMD_2, ENCRYPT_TYPE, type);
}

/**
 * hal_tx_desc_set_addr_search_flags - Enable AddrX and AddrY search flags
 * @desc: Handle to Tx Descriptor
 * @flags: Bit 0 - AddrY search enable, Bit 1 - AddrX search enable
 *
 * Return: void
 */
static inline void hal_tx_desc_set_addr_search_flags(void *desc,
						     uint8_t flags)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_2, ADDRX_EN) |=
		HAL_TX_SM(TCL_DATA_CMD_2, ADDRX_EN, (flags & 0x1));

	HAL_SET_FLD(desc, TCL_DATA_CMD_2, ADDRY_EN) |=
		HAL_TX_SM(TCL_DATA_CMD_2, ADDRY_EN, (flags >> 1));
}

/**
 * hal_tx_desc_set_l4_checksum_en -  Set TCP/IP checksum enable flags
 * Tx Descriptor for MSDU_buffer type
 * @desc: Handle to Tx Descriptor
 * @en: UDP/TCP over ipv4/ipv6 checksum enable flags (5 bits)
 *
 * Return: void
 */
static inline void hal_tx_desc_set_l4_checksum_en(void *desc,
						  uint8_t en)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_3, IPV4_CHECKSUM_EN) |=
		(HAL_TX_SM(TCL_DATA_CMD_3, UDP_OVER_IPV4_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD_3, UDP_OVER_IPV6_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD_3, TCP_OVER_IPV4_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD_3, TCP_OVER_IPV6_CHECKSUM_EN, en));
}

/**
 * hal_tx_desc_set_l3_checksum_en -  Set IPv4 checksum enable flag in
 * Tx Descriptor for MSDU_buffer type
 * @desc: Handle to Tx Descriptor
 * @en: ipv4 checksum enable flags
 *
 * Return: void
 */
static inline void hal_tx_desc_set_l3_checksum_en(void *desc,
						  uint8_t en)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_3, IPV4_CHECKSUM_EN) |=
		HAL_TX_SM(TCL_DATA_CMD_3, IPV4_CHECKSUM_EN, en);
}

/**
 * hal_tx_desc_set_fw_metadata- Sets the metadata that is part of TCL descriptor
 * @desc:Handle to Tx Descriptor
 * @metadata: Metadata to be sent to Firmware
 *
 * Return: void
 */
static inline void hal_tx_desc_set_fw_metadata(void *desc,
					       uint16_t metadata)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_2, TCL_CMD_NUMBER) |=
		HAL_TX_SM(TCL_DATA_CMD_2, TCL_CMD_NUMBER, metadata);
}

/**
 * hal_tx_desc_set_to_fw - Set To_FW bit in Tx Descriptor.
 * @desc:Handle to Tx Descriptor
 * @to_fw: if set, Forward packet to FW along with classification result
 *
 * Return: void
 */
static inline void hal_tx_desc_set_to_fw(void *desc, uint8_t to_fw)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_3, TO_FW) |=
		HAL_TX_SM(TCL_DATA_CMD_3, TO_FW, to_fw);
}

/**
 * hal_tx_desc_set_mesh_en - Set mesh_enable flag in Tx descriptor
 * @hal_soc_hdl: hal soc handle
 * @desc: Handle to Tx Descriptor
 * @en:   For raw WiFi frames, this indicates transmission to a mesh STA,
 *        enabling the interpretation of the 'Mesh Control Present' bit
 *        (bit 8) of QoS Control (otherwise this bit is ignored),
 *        For native WiFi frames, this indicates that a 'Mesh Control' field
 *        is present between the header and the LLC.
 *
 * Return: void
 */
static inline void hal_tx_desc_set_mesh_en(hal_soc_handle_t hal_soc_hdl,
					   void *desc, uint8_t en)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_desc_set_mesh_en(desc, en);
}

/**
 * hal_tx_desc_set_hlos_tid - Set the TID value (override DSCP/PCP fields in
 * frame) to be used for Tx Frame
 * @desc: Handle to Tx Descriptor
 * @hlos_tid: HLOS TID
 *
 * Return: void
 */
static inline void hal_tx_desc_set_hlos_tid(void *desc,
					    uint8_t hlos_tid)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD_4, HLOS_TID) |=
		HAL_TX_SM(TCL_DATA_CMD_4, HLOS_TID, hlos_tid);

	HAL_SET_FLD(desc, TCL_DATA_CMD_4, HLOS_TID_OVERWRITE) |=
	   HAL_TX_SM(TCL_DATA_CMD_4, HLOS_TID_OVERWRITE, 1);
}

/**
 * hal_tx_desc_clear - Clear the HW descriptor entry
 * @hw_desc: Hardware descriptor to be cleared
 *
 * Return: void
 */
static inline void hal_tx_desc_clear(void *hw_desc)
{
	qdf_mem_set(hw_desc + sizeof(struct tlv_32_hdr),
		    HAL_TX_DESC_LEN_BYTES, 0);
}

/**
 * hal_tx_desc_sync - Commit the descriptor to Hardware
 * @hal_tx_desc_cached: Cached descriptor that software maintains
 * @hw_desc: Hardware descriptor to be updated
 */
static inline void hal_tx_desc_sync(void *hal_tx_desc_cached,
				    void *hw_desc)
{
	qdf_mem_copy((hw_desc + sizeof(struct tlv_32_hdr)),
		     hal_tx_desc_cached, HAL_TX_DESC_LEN_BYTES);
}

/*---------------------------------------------------------------------------
 * WBM Descriptor accessor APIs for Tx completions
 *---------------------------------------------------------------------------
 */

/**
 * hal_tx_get_wbm_sw0_bm_id() - Get the BM ID for first tx completion ring
 *
 * Return: BM ID for first tx completion ring
 */
static inline uint32_t hal_tx_get_wbm_sw0_bm_id(void)
{
	return HAL_RH_WBM_SW0_BM_ID;
}
#endif /* _HAL_RH_TX_H_ */

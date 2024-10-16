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

#ifndef _HAL_BE_TX_H_
#define _HAL_BE_TX_H_

#include "hal_be_hw_headers.h"
#include "hal_tx.h"

/* Number of TX banks reserved i.e, will not be used by host driver. */
/* MAX_TCL_BANK reserved for FW use */
#define HAL_TX_NUM_RESERVED_BANKS 1

/*
 * Number of Priority to TID mapping
 */
#define HAL_BE_TX_MAP0_PRI2TID_MAX 10
#define HAL_BE_TX_MAP1_PRI2TID_MAX 6

enum hal_be_tx_ret_buf_manager {
	HAL_BE_WBM_SW0_BM_ID = 5,
	HAL_BE_WBM_SW1_BM_ID = 6,
	HAL_BE_WBM_SW2_BM_ID = 7,
	HAL_BE_WBM_SW3_BM_ID = 8,
	HAL_BE_WBM_SW4_BM_ID = 9,
	HAL_BE_WBM_SW5_BM_ID = 10,
	HAL_BE_WBM_SW6_BM_ID = 11,
};

enum hal_tx_mcast_ctrl {
	/* mcast traffic exceptioned to FW
	 * valid only for AP VAP default for AP
	 */
	HAL_TX_MCAST_CTRL_FW_EXCEPTION = 0,
	/* mcast traffic dropped in TCL*/
	HAL_TX_MCAST_CTRL_DROP,
	/* MEC notification are enabled
	 * valid only for client VAP
	 */
	HAL_TX_MCAST_CTRL_MEC_NOTIFY,
	/* no special routing for mcast
	 * valid for client vap when index search is enabled
	 */
	HAL_TX_MCAST_CTRL_NO_SPECIAL,
};

/* enum hal_tx_notify_frame_type - TX notify frame type
 * @NO_TX_NOTIFY: Not a notify frame
 * @TX_HARD_NOTIFY: Hard notify TX frame
 * @TX_SOFT_NOTIFY_E: Soft Notify Tx frame
 * @TX_SEMI_HARD_NOTIFY_E: Semi Hard notify TX frame
 */
enum hal_tx_notify_frame_type {
	NO_TX_NOTIFY = 0,
	TX_HARD_NOTIFY = 1,
	TX_SOFT_NOTIFY_E = 2,
	TX_SEMI_HARD_NOTIFY_E = 3
};

/*---------------------------------------------------------------------------
 * Structures
 * ---------------------------------------------------------------------------
 */
/**
 * struct hal_tx_bank_config - SW config bank params
 * @epd: EPD indication flag
 * @encap_type: encapsulation type
 * @encrypt_type: encrypt type
 * @src_buffer_swap: big-endia switch for packet buffer
 * @link_meta_swap: big-endian switch for link metadata
 * @index_lookup_enable: Enable index lookup
 * @addrx_en: Address-X search
 * @addry_en: Address-Y search
 * @mesh_enable:mesh enable flag
 * @vdev_id_check_en: vdev id check
 * @pmac_id: mac id
 * @mcast_pkt_ctrl: mulitcast packet control
 * @val: value representing bank config
 */
union hal_tx_bank_config {
	struct {
		uint32_t epd:1,
			 encap_type:2,
			 encrypt_type:4,
			 src_buffer_swap:1,
			 link_meta_swap:1,
			 index_lookup_enable:1,
			 addrx_en:1,
			 addry_en:1,
			 mesh_enable:2,
			 vdev_id_check_en:1,
			 pmac_id:2,
			 mcast_pkt_ctrl:2,
			 dscp_tid_map_id:6,
			 reserved:7;
	};
	uint32_t val;
};

/**
 * struct hal_tx_cmn_config_ppe - SW config exception related parameters
 * @drop_prec_err - Exception drop_prec errors.
 * @fake_mac_hdr - Exception fake mac header.
 * @cpu_code_inv - Exception cpu code invalid.
 * @data_buff_err - Exception buffer length/offset erorors.
 * @l3_l4_err - Exception m3_l4 checksum errors
 * @data_offset_max - Maximum data offset allowed.
 * @data_len_max - Maximum data length allowed.
 */
union hal_tx_cmn_config_ppe {
	struct {
		uint32_t drop_prec_err:1,
			 fake_mac_hdr:1,
			 cpu_code_inv:1,
			 data_buff_err:1,
			 l3_l4_err:1,
			 data_offset_max:12,
			 data_len_max:14;
	};
	uint32_t val;
};

/**
 * hal_tx_ppe_vp_config - SW config PPE VP table
 * @vp_num - Virtual port number
 * @pmac_id - Lmac ID
 * @bank_id: Bank ID corresponding to this I/F.
 * @vdev_id: VDEV ID of the I/F.
 * @search_idx_reg_num: Register number of this SI.
 * @use_ppe_int_pri: Use the PPE INT_PRI to TID table
 * @to_fw: Use FW
 * @drop_prec_enable: Enable precedence drop.
 */
union hal_tx_ppe_vp_config {
	struct {
		uint32_t vp_num:8,
			 pmac_id:2,
			 bank_id:6,
			 vdev_id:8,
			 search_idx_reg_num:3,
			 use_ppe_int_pri:1,
			 to_fw:1,
			 drop_prec_enable:1;
	};
	uint32_t val;
};

/**
 * hal_tx_cmn_ppe_idx_map_config: Use ppe index mapping table
 * @search_idx: Search index
 * @cache_set: Cache set number
 */
union hal_tx_ppe_idx_map_config {
	struct {
		uint32_t search_idx:20,
			 cache_set:4;
	};
	uint32_t val;
};

/**
 * hal_tx_ppe_pri2tid_map0_config : Configure ppe INT_PRI to tid map
 * @int_pri0: INT_PRI_0
 * @int_pri1: INT_PRI_1
 * @int_pri2: INT_PRI_2
 * @int_pri3: INT_PRI_3
 * @int_pri4: INT_PRI_4
 * @int_pri5: INT_PRI_5
 * @int_pri6: INT_PRI_6
 * @int_pri7: INT_PRI_7
 * @int_pri8: INT_PRI_8
 * @int_pri9: INT_PRI_9
 */
union hal_tx_ppe_pri2tid_map0_config {
	struct {
		uint32_t int_pri0:3,
			 int_pri1:3,
			 int_pri2:3,
			 int_pri3:3,
			 int_pri4:3,
			 int_pri5:3,
			 int_pri6:3,
			 int_pri7:3,
			 int_pri8:3,
			 int_pri9:3;
	};
	uint32_t val;
};

/**
 * hal_tx_ppe_pri2tid_map1_config : Configure ppe INT_PRI to tid map
 * @int_pri0: INT_PRI_10
 * @int_pri1: INT_PRI_11
 * @int_pri2: INT_PRI_12
 * @int_pri3: INT_PRI_13
 * @int_pri4: INT_PRI_14
 * @int_pri5: INT_PRI_15
 */
union hal_tx_ppe_pri2tid_map1_config {
	struct {
		uint32_t int_pri10:3,
			 int_pri11:3,
			 int_pri12:3,
			 int_pri13:3,
			 int_pri14:3,
			 int_pri15:3;
	};
	uint32_t val;
};

/*---------------------------------------------------------------------------
 *  Function declarations and documentation
 * ---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 *  TCL Descriptor accessor APIs
 *---------------------------------------------------------------------------
 */

/**
 * hal_tx_desc_set_tx_notify_frame - Set TX notify_frame field in Tx desc
 * @desc: Handle to Tx Descriptor
 * @val: Value to be set
 *
 * Return: None
 */
static inline void hal_tx_desc_set_tx_notify_frame(void *desc,
						   uint8_t val)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, TX_NOTIFY_FRAME) |=
		HAL_TX_SM(TCL_DATA_CMD, TX_NOTIFY_FRAME, val);
}

/**
 * hal_tx_desc_set_flow_override_enable - Set flow_override_enable field
 * @desc: Handle to Tx Descriptor
 * @val: Value to be set
 *
 * Return: None
 */
static inline void  hal_tx_desc_set_flow_override_enable(void *desc,
							 uint8_t val)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, FLOW_OVERRIDE_ENABLE) |=
		HAL_TX_SM(TCL_DATA_CMD, FLOW_OVERRIDE_ENABLE, val);
}

/**
 * hal_tx_desc_set_flow_override - Set flow_override field in TX desc
 * @desc: Handle to Tx Descriptor
 * @val: Value to be set
 *
 * Return: None
 */
static inline void  hal_tx_desc_set_flow_override(void *desc,
						  uint8_t val)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, FLOW_OVERRIDE) |=
		HAL_TX_SM(TCL_DATA_CMD, FLOW_OVERRIDE, val);
}

/**
 * hal_tx_desc_set_who_classify_info_sel - Set who_classify_info_sel field
 * @desc: Handle to Tx Descriptor
 * @val: Value to be set
 *
 * Return: None
 */
static inline void  hal_tx_desc_set_who_classify_info_sel(void *desc,
							  uint8_t val)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, WHO_CLASSIFY_INFO_SEL) |=
		HAL_TX_SM(TCL_DATA_CMD, WHO_CLASSIFY_INFO_SEL, val);
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
	HAL_SET_FLD(desc, TCL_DATA_CMD, DATA_LENGTH) |=
		HAL_TX_SM(TCL_DATA_CMD, DATA_LENGTH, data_length);
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
	HAL_SET_FLD(desc, TCL_DATA_CMD, PACKET_OFFSET) |=
		HAL_TX_SM(TCL_DATA_CMD, PACKET_OFFSET, offset);
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
	HAL_SET_FLD(desc, TCL_DATA_CMD, IPV4_CHECKSUM_EN) |=
		(HAL_TX_SM(TCL_DATA_CMD, UDP_OVER_IPV4_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD, UDP_OVER_IPV6_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD, TCP_OVER_IPV4_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD, TCP_OVER_IPV6_CHECKSUM_EN, en));
}

/**
 * hal_tx_desc_set_l3_checksum_en -  Set IPv4 checksum enable flag in
 * Tx Descriptor for MSDU_buffer type
 * @desc: Handle to Tx Descriptor
 * @checksum_en_flags: ipv4 checksum enable flags
 *
 * Return: void
 */
static inline void hal_tx_desc_set_l3_checksum_en(void *desc,
						  uint8_t en)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, IPV4_CHECKSUM_EN) |=
		HAL_TX_SM(TCL_DATA_CMD, IPV4_CHECKSUM_EN, en);
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
	HAL_SET_FLD(desc, TCL_DATA_CMD, TCL_CMD_NUMBER) |=
		HAL_TX_SM(TCL_DATA_CMD, TCL_CMD_NUMBER, metadata);
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
	HAL_SET_FLD(desc, TCL_DATA_CMD, TO_FW) |=
		HAL_TX_SM(TCL_DATA_CMD, TO_FW, to_fw);
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
	HAL_SET_FLD(desc, TCL_DATA_CMD, HLOS_TID) |=
		HAL_TX_SM(TCL_DATA_CMD, HLOS_TID, hlos_tid);

	HAL_SET_FLD(desc, TCL_DATA_CMD, HLOS_TID_OVERWRITE) |=
	   HAL_TX_SM(TCL_DATA_CMD, HLOS_TID_OVERWRITE, 1);
}

/**
 * hal_tx_desc_sync - Commit the descriptor to Hardware
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @hw_desc: Hardware descriptor to be updated
 */
static inline void hal_tx_desc_sync(void *hal_tx_desc_cached,
				    void *hw_desc, uint8_t num_bytes)
{
	qdf_mem_copy(hw_desc, hal_tx_desc_cached, num_bytes);
}

/**
 * hal_tx_desc_set_vdev_id - set vdev id to the descriptor to Hardware
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @vdev_id: vdev id
 */
static inline void hal_tx_desc_set_vdev_id(void *desc, uint8_t vdev_id)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, VDEV_ID) |=
		HAL_TX_SM(TCL_DATA_CMD, VDEV_ID, vdev_id);
}

/**
 * hal_tx_desc_set_bank_id - set bank id to the descriptor to Hardware
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @bank_id: bank id
 */
static inline void hal_tx_desc_set_bank_id(void *desc, uint8_t bank_id)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, BANK_ID) |=
		HAL_TX_SM(TCL_DATA_CMD, BANK_ID, bank_id);
}

/**
 * hal_tx_desc_set_tcl_cmd_type - set tcl command type to the descriptor
 * to Hardware
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @tcl_cmd_type: tcl command type
 */
static inline void
hal_tx_desc_set_tcl_cmd_type(void *desc, uint8_t tcl_cmd_type)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, TCL_CMD_TYPE) |=
		HAL_TX_SM(TCL_DATA_CMD, TCL_CMD_TYPE, tcl_cmd_type);
}

/**
 * hal_tx_desc_set_lmac_id_be - set lmac id to the descriptor to Hardware
 * @hal_soc_hdl: hal soc handle
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @lmac_id: lmac id
 */
static inline void
hal_tx_desc_set_lmac_id_be(hal_soc_handle_t hal_soc_hdl, void *desc,
			   uint8_t lmac_id)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, PMAC_ID) |=
		HAL_TX_SM(TCL_DATA_CMD, PMAC_ID, lmac_id);
}

/**
 * hal_tx_desc_set_search_index_be - set search index to the
 * descriptor to Hardware
 * @hal_soc_hdl: hal soc handle
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @search_index: search index
 */
static inline void
hal_tx_desc_set_search_index_be(hal_soc_handle_t hal_soc_hdl, void *desc,
				uint32_t search_index)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, SEARCH_INDEX) |=
		HAL_TX_SM(TCL_DATA_CMD, SEARCH_INDEX, search_index);
}

/**
 * hal_tx_desc_set_cache_set_num - set cache set num to the
 * descriptor to Hardware
 * @hal_soc_hdl: hal soc handle
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @cache_num: cache number
 */
static inline void
hal_tx_desc_set_cache_set_num(hal_soc_handle_t hal_soc_hdl, void *desc,
			      uint8_t cache_num)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, CACHE_SET_NUM) |=
		HAL_TX_SM(TCL_DATA_CMD, CACHE_SET_NUM, cache_num);
}

/**
 * hal_tx_desc_set_lookup_override_num - set lookup override num
 * to the descriptor to Hardware
 * @hal_soc_hdl: hal soc handle
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @cache_num: set numbernumber
 */
static inline void
hal_tx_desc_set_index_lookup_override(hal_soc_handle_t hal_soc_hdl,
				      void *desc, uint8_t num)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, INDEX_LOOKUP_OVERRIDE) |=
		HAL_TX_SM(TCL_DATA_CMD, INDEX_LOOKUP_OVERRIDE, num);
}

/*---------------------------------------------------------------------------
 * WBM Descriptor accessor APIs for Tx completions
 * ---------------------------------------------------------------------------
 */

/**
 * hal_tx_get_wbm_sw0_bm_id() - Get the BM ID for first tx completion ring
 *
 * Return: BM ID for first tx completion ring
 */
static inline uint32_t hal_tx_get_wbm_sw0_bm_id(void)
{
	return HAL_BE_WBM_SW0_BM_ID;
}

/**
 * hal_tx_comp_get_desc_id() - Get TX descriptor id within comp descriptor
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will tx descriptor id, cookie, within hardware completion
 * descriptor. For cases when cookie conversion is disabled, the sw_cookie
 * is present in the 2nd DWORD.
 *
 * Return: cookie
 */
static inline uint32_t hal_tx_comp_get_desc_id(void *hal_desc)
{
	uint32_t comp_desc =
		*(uint32_t *)(((uint8_t *)hal_desc) +
			       BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_OFFSET);

	/* Cookie is placed on 2nd word */
	return (comp_desc & BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_MASK) >>
		BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_LSB;
}

/**
 * hal_tx_comp_get_paddr() - Get paddr within comp descriptor
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will get buffer physical address within hardware completion
 * descriptor
 *
 * Return: Buffer physical address
 */
static inline qdf_dma_addr_t hal_tx_comp_get_paddr(void *hal_desc)
{
	uint32_t paddr_lo;
	uint32_t paddr_hi;

	paddr_lo = *(uint32_t *)(((uint8_t *)hal_desc) +
			BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_OFFSET);

	paddr_hi = *(uint32_t *)(((uint8_t *)hal_desc) +
			BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_OFFSET);

	paddr_hi = (paddr_hi & BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_MASK) >>
		BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_LSB;

	return (qdf_dma_addr_t)(paddr_lo | (((uint64_t)paddr_hi) << 32));
}

#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
/* HW set dowrd-2 bit30 to 1 if HW CC is done */
#define HAL_WBM2SW_COMPLETION_RING_TX_CC_DONE_OFFSET 0x8
#define HAL_WBM2SW_COMPLETION_RING_TX_CC_DONE_MASK 0x40000000
#define HAL_WBM2SW_COMPLETION_RING_TX_CC_DONE_LSB 0x1E
/**
 * hal_tx_comp_get_cookie_convert_done() - Get cookie conversion done flag
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will get the bit value that indicate HW cookie
 * conversion done or not
 *
 * Return: 1 - HW cookie conversion done, 0 - not
 */
static inline uint8_t hal_tx_comp_get_cookie_convert_done(void *hal_desc)
{
	return HAL_TX_DESC_GET(hal_desc, HAL_WBM2SW_COMPLETION_RING_TX,
			       CC_DONE);
}
#endif

/**
 * hal_tx_comp_set_desc_va_63_32() - Set bit 32~63 value for 64 bit VA
 * @hal_desc: completion ring descriptor pointer
 * @val: value to be set
 *
 * Return: None
 */
static inline void hal_tx_comp_set_desc_va_63_32(void *hal_desc, uint32_t val)
{
	HAL_SET_FLD(hal_desc,
		    WBM2SW_COMPLETION_RING_TX,
		    BUFFER_VIRT_ADDR_63_32) = val;
}

/**
 * hal_tx_comp_get_desc_va() - Get Desc virtual address within completion Desc
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will get the TX Desc virtual address
 *
 * Return: TX desc virtual address
 */
static inline uint64_t hal_tx_comp_get_desc_va(void *hal_desc)
{
	uint64_t va_from_desc;

	va_from_desc = HAL_TX_DESC_GET(hal_desc,
				       WBM2SW_COMPLETION_RING_TX,
				       BUFFER_VIRT_ADDR_31_0) |
			(((uint64_t)HAL_TX_DESC_GET(
					hal_desc,
					WBM2SW_COMPLETION_RING_TX,
					BUFFER_VIRT_ADDR_63_32)) << 32);

	return va_from_desc;
}

/*---------------------------------------------------------------------------
 * TX BANK register accessor APIs
 * ---------------------------------------------------------------------------
 */

/**
 * hal_tx_get_num_tcl_banks() - Get number of banks for target
 *
 * Return: None
 */
static inline uint8_t
hal_tx_get_num_tcl_banks(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	int  hal_banks = 0;

	if (hal_soc->ops->hal_tx_get_num_tcl_banks) {
		hal_banks =  hal_soc->ops->hal_tx_get_num_tcl_banks();
		hal_banks -= HAL_TX_NUM_RESERVED_BANKS;
		hal_banks = (hal_banks < 0) ? 0 : hal_banks;
	}

	return hal_banks;
}

/**
 * hal_tx_populate_bank_register() - populate the bank register with
 *		the software configs.
 * @soc: HAL soc handle
 * @config: bank config
 * @bank_id: bank id to be configured
 *
 * Returns: None
 */
static inline void
hal_tx_populate_bank_register(hal_soc_handle_t hal_soc_hdl,
			      union hal_tx_bank_config *config,
			      uint8_t bank_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_populate_bank_register(hal_soc_hdl, config,
						    bank_id);
}

#ifdef DP_TX_IMPLICIT_RBM_MAPPING

#define RBM_MAPPING_BMSK HWIO_TCL_R0_RBM_MAPPING0_SW2TCL1_RING_BMSK
#define RBM_MAPPING_SHFT HWIO_TCL_R0_RBM_MAPPING0_SW2TCL2_RING_SHFT

#define RBM_PPE2TCL_OFFSET \
			(HWIO_TCL_R0_RBM_MAPPING0_PPE2TCL1_RING_SHFT >> 2)
#define RBM_TCL_CMD_CREDIT_OFFSET \
			(HWIO_TCL_R0_RBM_MAPPING0_SW2TCL_CREDIT_RING_SHFT >> 2)

/**
 * hal_tx_config_rbm_mapping_be() - Update return buffer manager ring id
 * @hal_soc: HAL SoC context
 * @hal_ring_hdl: Source ring pointer
 * @rbm_id: return buffer manager ring id
 *
 * Return: void
 */
static inline void
hal_tx_config_rbm_mapping_be(hal_soc_handle_t hal_soc_hdl,
			     hal_ring_handle_t hal_ring_hdl,
			     uint8_t rbm_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_config_rbm_mapping_be(hal_soc_hdl, hal_ring_hdl,
						   rbm_id);
}
#else
static inline void
hal_tx_config_rbm_mapping_be(hal_soc_handle_t hal_soc_hdl,
			     hal_ring_handle_t hal_ring_hdl,
			     uint8_t rbm_id)
{
}
#endif

/**
 * hal_tx_desc_set_buf_addr_be - Fill Buffer Address information in Tx Desc
 * @desc: Handle to Tx Descriptor
 * @paddr: Physical Address
 * @pool_id: Return Buffer Manager ID
 * @desc_id: Descriptor ID
 * @type: 0 - Address points to a MSDU buffer
 *		1 - Address points to MSDU extension descriptor
 *
 * Return: void
 */
#ifdef DP_TX_IMPLICIT_RBM_MAPPING
static inline void
hal_tx_desc_set_buf_addr_be(hal_soc_handle_t hal_soc_hdl, void *desc,
			    dma_addr_t paddr, uint8_t rbm_id,
			    uint32_t desc_id, uint8_t type)
{
	/* Set buffer_addr_info.buffer_addr_31_0 */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_BUFFER_ADDR_31_0) =
		HAL_TX_SM(TCL_DATA_CMD, BUF_ADDR_INFO_BUFFER_ADDR_31_0, paddr);

	/* Set buffer_addr_info.buffer_addr_39_32 */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_BUFFER_ADDR_39_32) |=
		HAL_TX_SM(TCL_DATA_CMD, BUF_ADDR_INFO_BUFFER_ADDR_39_32,
			  (((uint64_t)paddr) >> 32));

	/* Set buffer_addr_info.sw_buffer_cookie = desc_id */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_SW_BUFFER_COOKIE) |=
		HAL_TX_SM(TCL_DATA_CMD, BUF_ADDR_INFO_SW_BUFFER_COOKIE,
			  desc_id);

	/* Set  Buffer or Ext Descriptor Type */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_OR_EXT_DESC_TYPE) |=
		HAL_TX_SM(TCL_DATA_CMD, BUF_OR_EXT_DESC_TYPE, type);
}
#else
static inline void
hal_tx_desc_set_buf_addr_be(hal_soc_handle_t hal_soc_hdl, void *desc,
			    dma_addr_t paddr, uint8_t rbm_id,
			    uint32_t desc_id, uint8_t type)
{
	/* Set buffer_addr_info.buffer_addr_31_0 */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_BUFFER_ADDR_31_0) =
		HAL_TX_SM(TCL_DATA_CMD, BUF_ADDR_INFO_BUFFER_ADDR_31_0, paddr);

	/* Set buffer_addr_info.buffer_addr_39_32 */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_BUFFER_ADDR_39_32) |=
		HAL_TX_SM(TCL_DATA_CMD, BUF_ADDR_INFO_BUFFER_ADDR_39_32,
			  (((uint64_t)paddr) >> 32));

	/* Set buffer_addr_info.return_buffer_manager = rbm id */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_RETURN_BUFFER_MANAGER) |=
		HAL_TX_SM(TCL_DATA_CMD,
			  BUF_ADDR_INFO_RETURN_BUFFER_MANAGER, rbm_id);

	/* Set buffer_addr_info.sw_buffer_cookie = desc_id */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_ADDR_INFO_SW_BUFFER_COOKIE) |=
		HAL_TX_SM(TCL_DATA_CMD, BUF_ADDR_INFO_SW_BUFFER_COOKIE,
			  desc_id);

	/* Set  Buffer or Ext Descriptor Type */
	HAL_SET_FLD(desc, TCL_DATA_CMD,
		    BUF_OR_EXT_DESC_TYPE) |=
		HAL_TX_SM(TCL_DATA_CMD, BUF_OR_EXT_DESC_TYPE, type);
}
#endif

/**
 * hal_tx_vdev_mismatch_routing_set - set vdev mismatch exception routing
 * @hal_soc: HAL SoC context
 * @config: HAL_TX_VDEV_MISMATCH_TQM_NOTIFY - route via TQM
 *          HAL_TX_VDEV_MISMATCH_FW_NOTIFY - route via FW
 *
 * Return: void
 */
#ifdef HWIO_TCL_R0_CMN_CONFIG_VDEVID_MISMATCH_EXCEPTION_BMSK
static inline void
hal_tx_vdev_mismatch_routing_set(hal_soc_handle_t hal_soc_hdl,
				 enum hal_tx_vdev_mismatch_notify config)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_vdev_mismatch_routing_set(hal_soc_hdl, config);
}
#else
static inline void
hal_tx_vdev_mismatch_routing_set(hal_soc_handle_t hal_soc_hdl,
				 enum hal_tx_vdev_mismatch_notify config)
{
}
#endif

/**
 * hal_tx_mcast_mlo_reinject_routing_set - set MLO multicast reinject routing
 * @hal_soc: HAL SoC context
 * @config: HAL_TX_MCAST_MLO_REINJECT_FW_NOTIFY - route via FW
 *          HAL_TX_MCAST_MLO_REINJECT_TQM_NOTIFY - route via TQM
 *
 * Return: void
 */
#if defined(HWIO_TCL_R0_CMN_CONFIG_MCAST_CMN_PN_SN_MLO_REINJECT_ENABLE_BMSK) && \
	defined(WLAN_MCAST_MLO)
static inline void
hal_tx_mcast_mlo_reinject_routing_set(
				hal_soc_handle_t hal_soc_hdl,
				enum hal_tx_mcast_mlo_reinject_notify config)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	hal_soc->ops->hal_tx_mcast_mlo_reinject_routing_set(hal_soc_hdl,
							    config);
}
#else
static inline void
hal_tx_mcast_mlo_reinject_routing_set(
				hal_soc_handle_t hal_soc_hdl,
				enum hal_tx_mcast_mlo_reinject_notify config)
{
}
#endif

/**
 * hal_reo_config_reo2ppe_dest_info() - Configure reo2ppe dest info
 * @hal_soc_hdl: HAL SoC Context
 *
 * Return: None.
 */
static inline
void hal_reo_config_reo2ppe_dest_info(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_reo_config_reo2ppe_dest_info)
		hal_soc->ops->hal_reo_config_reo2ppe_dest_info(hal_soc_hdl);
}

/**
 * hal_tx_get_num_ppe_vp_tbl_entries() - Get the total number of VP table
 * @hal_soc: HAL SoC Context
 *
 * Return: Total number of entries.
 */
static inline
uint32_t hal_tx_get_num_ppe_vp_tbl_entries(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_tx_get_num_ppe_vp_tbl_entries(hal_soc_hdl);
}

/**
 * hal_tx_get_num_ppe_vp_search_idx_tbl_entries() - Get the total number of search idx registers
 * @hal_soc: HAL SoC Context
 *
 * Return: Total number of entries.
 */
static inline
uint32_t hal_tx_get_num_ppe_vp_search_idx_tbl_entries(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_tx_get_num_ppe_vp_search_idx_tbl_entries(hal_soc_hdl);
}

/**
 * hal_tx_set_ppe_cmn_cfg()- Set the PPE common config
 * @hal_soc: HAL SoC context
 * @cmn_cfg: HAL PPE VP common config
 *
 * Return: void
 */
static inline void
hal_tx_set_ppe_cmn_cfg(hal_soc_handle_t hal_soc_hdl,
		       union hal_tx_cmn_config_ppe *cmn_cfg)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_set_ppe_cmn_cfg(hal_soc_hdl, cmn_cfg);
}

/**
 * hal_tx_populate_ppe_vp_entry -  Populate ppe VP entry
 * @hal_soc: HAL SoC context
 * @vp_cfg: HAL PPE VP config
 * @ppe_vp_idx: PPE VP index
 *
 * Return: void
 */
static inline void
hal_tx_populate_ppe_vp_entry(hal_soc_handle_t hal_soc_hdl,
			     union hal_tx_ppe_vp_config *vp_cfg,
			     int ppe_vp_idx)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_set_ppe_vp_entry(hal_soc_hdl, vp_cfg, ppe_vp_idx);
}

/**
 * hal_ppeds_cfg_ast_override_map_reg - Set ppe index mapping table value
 * @hal_soc: HAL SoC context
 * @reg_idx: index into the table
 * @overide_map: HAL PPE INDEX MAPPING config
 *
 * Return: void
 */
static inline void
hal_ppeds_cfg_ast_override_map_reg(hal_soc_handle_t hal_soc_hdl,
	uint8_t reg_idx, union hal_tx_ppe_idx_map_config *overide_map)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_ppeds_cfg_ast_override_map_reg)
		hal_soc->ops->hal_ppeds_cfg_ast_override_map_reg(hal_soc_hdl,
								 reg_idx,
								 overide_map);
}

/**
 * hal_tx_set_int_pri2id - Set the prit2tid table.
 * @hal_soc: HAL SoC context
 * @pri2tid: Reference to SW INT_PRI to TID table
 *
 * Return: void
 */
static inline void
hal_tx_set_int_pri2tid(hal_soc_handle_t hal_soc_hdl,
		       uint32_t val, uint8_t map_no)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_set_ppe_pri2tid(hal_soc_hdl, val, map_no);
}

/**
 * hal_tx_update_int_pri2id - Populate the prit2tid table.
 * @hal_soc: HAL SoC context
 * @pri: INT_PRI value
 * @tid: Wi-Fi TID
 *
 * Return: void
 */
static inline void
hal_tx_update_int_pri2tid(hal_soc_handle_t hal_soc_hdl,
			  uint8_t pri, uint8_t tid)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_update_ppe_pri2tid(hal_soc_hdl, pri, tid);
}

/**
 * hal_tx_dump_ppe_vp_entry - Dump the PPE VP entry
 * @hal_soc_hdl: HAL SoC context
 *
 * Return: void
 */
static inline void
hal_tx_dump_ppe_vp_entry(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_dump_ppe_vp_entry(hal_soc_hdl);
}

/**
 * hal_tx_enable_pri2tid_map- Enable the priority to tid mapping
 * @hal_soc_hdl: HAL SoC context
 * @val: True/False value
 *
 * Return: void
 */
static inline void
hal_tx_enable_pri2tid_map(hal_soc_handle_t hal_soc_hdl, bool val,
			  uint8_t ppe_vp_idx)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_enable_pri2tid_map(hal_soc_hdl, val,
						ppe_vp_idx);
}

#ifdef HWIO_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_VAL_SHFT
static inline void
hal_tx_vdev_mcast_ctrl_set(hal_soc_handle_t hal_soc_hdl,
		uint8_t vdev_id, uint8_t mcast_ctrl_val)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_tx_vdev_mcast_ctrl_set(hal_soc_hdl, vdev_id,
						 mcast_ctrl_val);
}
#else
static inline void
hal_tx_vdev_mcast_ctrl_set(hal_soc_handle_t hal_soc_hdl,
		uint8_t vdev_id, uint8_t mcast_ctrl_val)
{
}
#endif
#endif /* _HAL_BE_TX_H_ */

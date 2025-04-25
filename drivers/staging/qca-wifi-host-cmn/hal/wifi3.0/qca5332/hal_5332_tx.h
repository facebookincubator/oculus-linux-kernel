/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE
 */
#ifndef _HAL_5332_TX_H_
#define _HAL_5332_TX_H_

#include "tcl_data_cmd.h"
#include "phyrx_rssi_legacy.h"
#include "hal_internal.h"
#include "qdf_trace.h"
#include "hal_rx.h"
#include "hal_tx.h"
#include "hal_api_mon.h"
#include <hal_be_tx.h>

#define DSCP_TID_TABLE_SIZE 24
#define NUM_WORDS_PER_DSCP_TID_TABLE (DSCP_TID_TABLE_SIZE / 4)
#define HAL_TX_NUM_DSCP_REGISTER_SIZE 32

/**
 * hal_tx_set_dscp_tid_map_5332() - Configure default DSCP to TID map table
 * @hal_soc: HAL SoC context
 * @map: DSCP-TID mapping table
 * @id: mapping table ID - 0-31
 *
 * DSCP are mapped to 8 TID values using TID values programmed
 * in any of the 32 DSCP_TID_MAPS (id = 0-31).
 *
 * Return: none
 */
static void hal_tx_set_dscp_tid_map_5332(struct hal_soc *hal_soc, uint8_t *map,
					 uint8_t id)
{
	int i;
	uint32_t addr, cmn_reg_addr;
	uint32_t value = 0, regval;
	uint8_t val[DSCP_TID_TABLE_SIZE], cnt = 0;

	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	if (id >= HAL_MAX_HW_DSCP_TID_V2_MAPS_5332)
		return;

	cmn_reg_addr = HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_ADDR(
					MAC_TCL_REG_REG_BASE);

	addr = HWIO_TCL_R0_DSCP_TID_MAP_n_ADDR(
				MAC_TCL_REG_REG_BASE,
				id * NUM_WORDS_PER_DSCP_TID_TABLE);

	/* Enable read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval |=
	    (1 <<
	    HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_SHFT);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);

	/* Write 8 (24 bits) DSCP-TID mappings in each iteration */
	for (i = 0; i < 64; i += 8) {
		value = (map[i] |
			(map[i + 1] << 0x3) |
			(map[i + 2] << 0x6) |
			(map[i + 3] << 0x9) |
			(map[i + 4] << 0xc) |
			(map[i + 5] << 0xf) |
			(map[i + 6] << 0x12) |
			(map[i + 7] << 0x15));

		qdf_mem_copy(&val[cnt], (void *)&value, 3);
		cnt += 3;
	}

	for (i = 0; i < DSCP_TID_TABLE_SIZE; i += 4) {
		regval = *(uint32_t *)(val + i);
		HAL_REG_WRITE(soc, addr,
			      (regval & HWIO_TCL_R0_DSCP_TID_MAP_n_RMSK));
		addr += 4;
	}

	/* Disable read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval &=
	~(HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_BMSK);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);
}

/**
 * hal_tx_update_dscp_tid_5332() - Update the dscp tid map table as updated
 *                                 by the user
 * @soc: HAL SoC context
 * @tid: TID
 * @id: MAP ID
 * @dscp: DSCP_TID map index
 *
 * Return: void
 */
static void hal_tx_update_dscp_tid_5332(struct hal_soc *soc, uint8_t tid,
					uint8_t id, uint8_t dscp)
{
	uint32_t addr, addr1, cmn_reg_addr;
	uint32_t start_value = 0, end_value = 0;
	uint32_t regval;
	uint8_t end_bits = 0;
	uint8_t start_bits = 0;
	uint32_t start_index, end_index;

	cmn_reg_addr = HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_ADDR(
					MAC_TCL_REG_REG_BASE);

	addr = HWIO_TCL_R0_DSCP_TID_MAP_n_ADDR(
				MAC_TCL_REG_REG_BASE,
				id * NUM_WORDS_PER_DSCP_TID_TABLE);

	start_index = dscp * HAL_TX_BITS_PER_TID;
	end_index = (start_index + (HAL_TX_BITS_PER_TID - 1))
		    % HAL_TX_NUM_DSCP_REGISTER_SIZE;
	start_index = start_index % HAL_TX_NUM_DSCP_REGISTER_SIZE;
	addr += (4 * ((dscp * HAL_TX_BITS_PER_TID) /
			HAL_TX_NUM_DSCP_REGISTER_SIZE));

	if (end_index < start_index) {
		end_bits = end_index + 1;
		start_bits = HAL_TX_BITS_PER_TID - end_bits;
		start_value = tid << start_index;
		end_value = tid >> start_bits;
		addr1 = addr + 4;
	} else {
		start_bits = HAL_TX_BITS_PER_TID - end_bits;
		start_value = tid << start_index;
		addr1 = 0;
	}

	/* Enable read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval |=
	(1 << HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_SHFT);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);

	regval = HAL_REG_READ(soc, addr);

	if (end_index < start_index)
		regval &= (~0) >> start_bits;
	else
		regval &= ~(7 << start_index);

	regval |= start_value;

	HAL_REG_WRITE(soc, addr, (regval & HWIO_TCL_R0_DSCP_TID_MAP_n_RMSK));

	if (addr1) {
		regval = HAL_REG_READ(soc, addr1);
		regval &= (~0) << end_bits;
		regval |= end_value;

		HAL_REG_WRITE(soc, addr1, (regval &
			     HWIO_TCL_R0_DSCP_TID_MAP_n_RMSK));
	}

	/* Disable read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval &=
	~(HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_BMSK);
	HAL_REG_WRITE(soc, cmn_reg_addr, regval);
}

#ifdef DP_TX_IMPLICIT_RBM_MAPPING

#define RBM_MAPPING_BMSK HWIO_TCL_R0_RBM_MAPPING0_SW2TCL1_RING_BMSK
#define RBM_MAPPING_SHFT HWIO_TCL_R0_RBM_MAPPING0_SW2TCL2_RING_SHFT

#define RBM_TCL_CMD_CREDIT_OFFSET \
			(HWIO_TCL_R0_RBM_MAPPING0_SW2TCL_CREDIT_RING_SHFT >> 2)

/**
 * hal_tx_config_rbm_mapping_be_5332() - Update return buffer manager ring id
 * @hal_soc_hdl: HAL SoC context
 * @hal_ring_hdl: Source ring pointer
 * @rbm_id: return buffer manager ring id
 *
 * Return: void
 */
static inline void
hal_tx_config_rbm_mapping_be_5332(hal_soc_handle_t hal_soc_hdl,
				  hal_ring_handle_t hal_ring_hdl,
				  uint8_t rbm_id)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr = 0;
	uint32_t reg_val = 0;
	uint32_t val = 0;
	uint8_t ring_num;
	enum hal_ring_type ring_type;

	ring_type = srng->ring_type;
	ring_num = hal_soc->hw_srng_table[ring_type].start_ring_id;
	ring_num = srng->ring_id - ring_num;

	reg_addr = HWIO_TCL_R0_RBM_MAPPING0_ADDR(MAC_TCL_REG_REG_BASE);

	if (ring_type == TCL_CMD_CREDIT)
		ring_num = ring_num + RBM_TCL_CMD_CREDIT_OFFSET;

	/* get current value stored in register address */
	val = HAL_REG_READ(hal_soc, reg_addr);

	/* mask out other stored value */
	val &= (~(RBM_MAPPING_BMSK << (RBM_MAPPING_SHFT * ring_num)));

	reg_val = val | ((RBM_MAPPING_BMSK & rbm_id) <<
			 (RBM_MAPPING_SHFT * ring_num));

	/* write rbm mapped value to register address */
	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#else
static inline void
hal_tx_config_rbm_mapping_be_5332(hal_soc_handle_t hal_soc_hdl,
				  hal_ring_handle_t hal_ring_hdl,
				  uint8_t rbm_id)
{
}
#endif

/**
 * hal_tx_init_cmd_credit_ring_5332() - Initialize command/credit SRNG
 * @hal_soc_hdl: Handle to HAL SoC structure
 * @hal_ring_hdl: Handle to HAL SRNG structure
 *
 * Return: none
 */
static inline void
hal_tx_init_cmd_credit_ring_5332(hal_soc_handle_t hal_soc_hdl,
				 hal_ring_handle_t hal_ring_hdl)
{
}

/* TX MONITOR */
#if defined(WLAN_PKT_CAPTURE_TX_2_0) && defined(TX_MONITOR_WORD_MASK)

#define TX_FES_SETUP_MASK 0x3
typedef struct tx_fes_setup_compact_5332 hal_tx_fes_setup_t;
struct tx_fes_setup_compact_5332 {
	/* DWORD - 0 */
	uint32_t schedule_id;
	/* DWORD - 1 */
	uint32_t reserved_1a			: 7,  // [0: 6]
		transmit_start_reason		: 3,  // [7: 9]
		reserved_1b			: 13, // [10: 22]
		number_of_users			: 6,  // [28: 23]
		mu_type				: 1,  // [29]
		reserved_1c			: 2;  // [30]
	/* DWORD - 2 */
	uint32_t reserved_2a			: 4,  // [0: 3]
		ndp_frame			: 2,  // [4: 5]
		txbf				: 1,  // [6]
		reserved_2b			: 3,  // [7: 9]
		static_bandwidth		: 3,  // [12: 10]
		reserved_2c			: 1,  // [13]
		transmission_contains_mu_rts	: 1,  // [14]
		reserved_2d			: 17; // [15: 31]
	/* DWORD - 3 */
	uint32_t reserved_3a			: 15, // [0: 14]
		mu_ndp				: 1,  // [15]
		reserved_3b			: 11, // [16: 26]
		ndpa				: 1,  // [27]
		reserved_3c			: 4;  // [28: 31]
};

#define TX_PEER_ENTRY_MASK 0x103
typedef struct tx_peer_entry_compact_5332 hal_tx_peer_entry_t;
struct tx_peer_entry_compact_5332 {
	/* DWORD - 0 */
	uint32_t mac_addr_a_31_0		: 32;
	/* DWORD - 1 */
	uint32_t mac_addr_a_47_32		: 16,
		 mac_addr_b_15_0		: 16;
	/* DWORD - 2 */
	uint32_t mac_addr_b_47_16		: 32;
	/* DWORD - 3 */
	uint32_t reserved_3			: 32;
	/* DWORD - 16 */
	uint32_t reserved_16			: 32;
	/* DWORD - 17 */
	uint32_t multi_link_addr_crypto_enable	: 1,
		 reserved_17_a			: 15,
		 sw_peer_id			: 16;
};

#define TX_QUEUE_EXT_MASK 0x1
typedef struct tx_queue_ext_compact_5332 hal_tx_queue_ext_t;
struct tx_queue_ext_compact_5332 {
	/* DWORD - 0 */
	uint32_t frame_ctl			: 16,
		 qos_ctl			: 16;
	/* DWORD - 1 */
	uint32_t ampdu_flag			: 1,
		 reserved_1			: 31;
};

#define TX_MSDU_START_MASK 0x1
typedef struct tx_msdu_start_compact_5332 hal_tx_msdu_start_t;
struct tx_msdu_start_compact_5332 {
	/* DWORD - 0 */
	uint32_t reserved_0			: 32;
	/* DWORD - 1 */
	uint32_t reserved_1			: 32;
};

#define TX_MPDU_START_MASK 0x3
typedef struct tx_mpdu_start_compact_5332 hal_tx_mpdu_start_t;
struct tx_mpdu_start_compact_5332 {
	/* DWORD - 0 */
	uint32_t mpdu_length			: 14,
		 frame_not_from_tqm		: 1,
		 vht_control_present		: 1,
		 mpdu_header_length		: 8,
		 retry_count			: 7,
		 wds				: 1;
	/* DWORD - 1 */
	uint32_t pn_31_0			: 32;
	/* DWORD - 2 */
	uint32_t pn_47_32			: 16,
		 mpdu_sequence_number		: 12,
		 raw_already_encrypted		: 1,
		 frame_type			: 2,
		 txdma_dropped_mpdu_warning	: 1;
	/* DWORD - 3 */
	uint32_t reserved_3			: 32;
};

typedef struct rxpcu_user_setup_compact_5332  hal_rxpcu_user_setup_t;
struct rxpcu_user_setup_compact_5332 {
};

#define TX_FES_STATUS_END_MASK 0x7
typedef struct tx_fes_status_end_compact_5332 hal_tx_fes_status_end_t;
struct tx_fes_status_end_compact_5332 {
	/* DWORD - 0 */
	uint32_t reserved_0			: 32;
	/* DWORD - 1 */
	struct {
	uint16_t phytx_abort_reason		: 8,
		 user_number			: 6,
		 reserved_1a			: 2;
	} phytx_abort_request_info_details;
	uint16_t reserved_1b			: 12,
		 phytx_abort_request_info_valid	: 1,
		 reserved_1c			: 3;
	/* DWORD - 2 */
	uint32_t start_of_frame_timestamp_15_0	: 16,
		 start_of_frame_timestamp_31_16 : 16;
	/* DWORD - 3 */
	uint32_t end_of_frame_timestamp_15_0	: 16,
		 end_of_frame_timestamp_31_16	: 16;
	/* DWORD - 4 */
	uint32_t terminate_ranging_sequence	: 1,
		 reserved_4a			: 7,
		 timing_status			: 2,
		 response_type			: 5,
		 r2r_end_status_to_follow	: 1,
		 transmit_delay			: 16;
	/* DWORD - 5 */
	uint32_t reserved_5			: 32;
};

#define RESPONSE_END_STATUS_MASK 0xD
typedef struct response_end_status_compact_5332 hal_response_end_status_t;
struct response_end_status_compact_5332 {
	/* DWORD - 0 */
	uint32_t coex_bt_tx_while_wlan_tx	: 1,
		 coex_wan_tx_while_wlan_tx	: 1,
		 coex_wlan_tx_while_wlan_tx	: 1,
		 global_data_underflow_warning	: 1,
		 response_transmit_status	: 4,
		 phytx_pkt_end_info_valid	: 1,
		 phytx_abort_request_info_valid	: 1,
		 generated_response		: 3,
		 mba_user_count			: 7,
		 mba_fake_bitmap_count		: 7,
		 coex_based_tx_bw		: 3,
		 trig_response_related		: 1,
		 dpdtrain_done			: 1;
	/* DWORD - 1 */
	uint32_t reserved_1			: 32;
	/* DWORD - 4 */
	uint32_t reserved_4			: 32;
	/* DWORD - 5 */
	uint32_t start_of_frame_timestamp_15_0	: 16,
		 start_of_frame_timestamp_31_16 : 16;
	/* DWORD - 6 */
	uint32_t end_of_frame_timestamp_15_0	: 16,
		 end_of_frame_timestamp_31_16	: 16;
	/* DWORD - 7 */
	uint32_t reserved_7			: 32;
};

#define TX_FES_STATUS_PROT_MASK	0x2
typedef struct tx_fes_status_prot_compact_5332 hal_tx_fes_status_prot_t;
struct tx_fes_status_prot_compact_5332 {
	/* DWORD - 2 */
	uint32_t start_of_frame_timestamp_15_0	: 16,
		 start_of_frame_timestamp_31_16 : 16;
	/* DWROD - 3 */
	uint32_t end_of_frame_timestamp_15_0	: 16,
		 end_of_frame_timestamp_31_16	: 16;
};

#define PCU_PPDU_SETUP_INIT_MASK 0x1E800000
typedef struct pcu_ppdu_setup_init_compact_5332 hal_pcu_ppdu_setup_t;
struct pcu_ppdu_setup_init_compact_5332 {
	/* DWORD - 46 */
	uint32_t reserved_46				: 32;
	/* DWORD - 47 */
	uint32_t r2r_group_id				: 6,
		 r2r_response_frame_type		: 4,
		 r2r_sta_partial_aid			: 11,
		 use_address_fields_for_protection	: 1,
		 r2r_set_required_response_time		: 1,
		 reserved_47				: 9;
	/* DWORD - 50 */
	uint32_t reserved_50				: 32;
	/* DWORD - 51 */
	uint32_t protection_frame_ad1_31_0		: 32;
	/* DWORD - 52 */
	uint32_t protection_frame_ad1_47_32		: 16,
		 protection_frame_ad2_15_0		: 16;
	/* DWORD - 53 */
	uint32_t protection_frame_ad2_47_16		: 32;
	/* DWORD - 54 */
	uint32_t reserved_54				: 32;
	/* DWORD - 55 */
	uint32_t protection_frame_ad3_31_0		: 32;
	/* DWORD - 56 */
	uint32_t protection_frame_ad3_47_32		: 16,
		 protection_frame_ad4_15_0		: 16;
	/* DWORD - 57 */
	uint32_t protection_frame_ad4_47_16		: 32;
};

/**
 * hal_txmon_get_word_mask_qca5332() - api to get word mask for tx monitor
 * @wmask: pointer to hal_txmon_word_mask_config_t
 *
 * Return: void
 */
static inline
void hal_txmon_get_word_mask_qca5332(void *wmask)
{
	hal_txmon_word_mask_config_t *word_mask = NULL;

	word_mask = (hal_txmon_word_mask_config_t *)wmask;

	word_mask->compaction_enable = 1;
	word_mask->tx_fes_setup = TX_FES_SETUP_MASK;
	word_mask->tx_peer_entry = TX_PEER_ENTRY_MASK;
	word_mask->tx_queue_ext = TX_QUEUE_EXT_MASK;
	word_mask->tx_msdu_start = TX_MSDU_START_MASK;
	word_mask->pcu_ppdu_setup_init = PCU_PPDU_SETUP_INIT_MASK;
	word_mask->tx_mpdu_start = TX_MPDU_START_MASK;
	word_mask->rxpcu_user_setup = 0xFF;
	word_mask->tx_fes_status_end = TX_FES_STATUS_END_MASK;
	word_mask->response_end_status = RESPONSE_END_STATUS_MASK;
	word_mask->tx_fes_status_prot = TX_FES_STATUS_PROT_MASK;
}
#endif /* WLAN_PKT_CAPTURE_TX_2_0 && TX_MONITOR_WORD_MASK */
#endif /* _HAL_5332_TX_H_ */

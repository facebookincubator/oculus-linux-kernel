/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @soc: HAL SoC context
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

	/* Diasble read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval &=
	~(HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_BMSK);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);
}

/**
 * hal_tx_update_dscp_tid_5332() - Update the dscp tid map table as updated
 *					by the user
 * @soc: HAL SoC context
 * @map: DSCP-TID mapping table
 * @id : MAP ID
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

	/* Diasble read/write access */
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
 * hal_tx_config_rbm_mapping_be() - Update return buffer manager ring id
 * @hal_soc: HAL SoC context
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
 * @hal_srng: Handle to HAL SRNG structure
 *
 * Return: none
 */
static inline void
hal_tx_init_cmd_credit_ring_5332(hal_soc_handle_t hal_soc_hdl,
				 hal_ring_handle_t hal_ring_hdl)
{
}

/* TX MONITOR */
#ifdef QCA_MONITOR_2_0_SUPPORT

#if defined(TX_MONITOR_WORD_MASK)
typedef struct tx_fes_setup_compact_5332 hal_tx_fes_setup_t;
struct tx_fes_setup_compact_5332 {
	/* DWORD - 0 */
	uint32_t schedule_id;
	/* DWORD - 1 */
	uint32_t reserved_1a			: 7,  // [0: 6]
		transmit_start_reason		: 3,  // [7: 9]
		reserved_1b			: 13, // [10: 22]
		number_of_users			: 6,  // [28: 23]
		MU_type				: 1,  // [29]
		reserved_1c			: 2;  // [30]
	/* DWORD - 2 */
	uint32_t reserved_2a			: 4,  // [0: 3]
		ndp_frame			: 2,  // [4: 5]
		txbf				: 1,  // [6]
		reserved_2b			: 3,  // [7: 9]
		static_bandwidth		: 3,  // [12: 10]
		reserved_2c			: 1,  // [13]
		transmission_contains_MU_RTS	: 1,  // [14]
		reserved_2d			: 17; // [15: 31]
	/* DWORD - 3 */
	uint32_t reserved_3a			: 15, // [0: 14]
		mu_ndp				: 1,  // [15]
		reserved_3b			: 11, // [16: 26]
		ndpa				: 1,  // [27]
		reserved_3c			: 4;  // [28: 31]
};
#endif
#endif /* QCA_MONITOR_2_0_SUPPORT */
#endif /* _HAL_5332_TX_H_ */

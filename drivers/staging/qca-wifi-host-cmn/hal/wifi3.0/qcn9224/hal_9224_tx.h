/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
#ifndef _HAL_9224_TX_H_
#define _HAL_9224_TX_H_

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
 * hal_tx_ppe2tcl_ring_halt_set() - Enable ring halt for the ppe2tcl ring
 * @hal_soc: HAL SoC context
 *
 * Return: none
 */
static void hal_tx_ppe2tcl_ring_halt_set_9224(hal_soc_handle_t hal_soc)
{
	uint32_t cmn_reg_addr;
	uint32_t regval;
	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	cmn_reg_addr =
		HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_ADDR(MAC_TCL_REG_REG_BASE);

	/* Enable RING_HALT */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval |=
	    (1 <<
	    HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_SHFT);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);
}

/**
 * hal_tx_ppe2tcl_ring_halt_reset() - Disable ring halt for the ppe2tcl ring
 * @hal_soc: HAL SoC context
 *
 * Return: none
 */
static void hal_tx_ppe2tcl_ring_halt_reset_9224(hal_soc_handle_t hal_soc)
{
	uint32_t cmn_reg_addr;
	uint32_t regval;
	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	cmn_reg_addr =
		HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_ADDR(MAC_TCL_REG_REG_BASE);

	/* Disable RING_HALT */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval &= ~(1 <<
	    HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_SHFT);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);
}

/**
 * hal_tx_ppe2tcl_ring_halt_done() - Check if ring halt is done for ppe2tcl ring
 * @hal_soc: HAL SoC context
 *
 * Return: true if halt done
 */
static bool hal_tx_ppe2tcl_ring_halt_done_9224(hal_soc_handle_t hal_soc)
{
	uint32_t cmn_reg_addr;
	uint32_t regval;
	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	cmn_reg_addr =
		HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_ADDR(MAC_TCL_REG_REG_BASE);

	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval &= (1 << HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_STAT_SHFT);

	return(!!regval);
}

/**
 * hal_tx_set_dscp_tid_map_9224() - Configure default DSCP to TID map table
 * @soc: HAL SoC context
 * @map: DSCP-TID mapping table
 * @id: mapping table ID - 0-31
 *
 * DSCP are mapped to 8 TID values using TID values programmed
 * in any of the 32 DSCP_TID_MAPS (id = 0-31).
 *
 * Return: none
 */
static void hal_tx_set_dscp_tid_map_9224(struct hal_soc *hal_soc, uint8_t *map,
					 uint8_t id)
{
	int i;
	uint32_t addr, cmn_reg_addr;
	uint32_t value = 0, regval;
	uint8_t val[DSCP_TID_TABLE_SIZE], cnt = 0;

	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	if (id >= HAL_MAX_HW_DSCP_TID_V2_MAPS)
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
 * hal_tx_update_dscp_tid_9224() - Update the dscp tid map table as updated
 *					by the user
 * @soc: HAL SoC context
 * @map: DSCP-TID mapping table
 * @id : MAP ID
 * @dscp: DSCP_TID map index
 *
 * Return: void
 */
static void hal_tx_update_dscp_tid_9224(struct hal_soc *soc, uint8_t tid,
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
hal_tx_config_rbm_mapping_be_9224(hal_soc_handle_t hal_soc_hdl,
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

	if (ring_type == PPE2TCL)
		ring_num = ring_num + RBM_PPE2TCL_OFFSET;
	else if (ring_type == TCL_CMD_CREDIT)
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
hal_tx_config_rbm_mapping_be_9224(hal_soc_handle_t hal_soc_hdl,
				  hal_ring_handle_t hal_ring_hdl,
				  uint8_t rbm_id)
{
}
#endif

/**
 * hal_tx_init_cmd_credit_ring_9224() - Initialize command/credit SRNG
 * @hal_soc_hdl: Handle to HAL SoC structure
 * @hal_srng: Handle to HAL SRNG structure
 *
 * Return: none
 */
static inline void
hal_tx_init_cmd_credit_ring_9224(hal_soc_handle_t hal_soc_hdl,
				 hal_ring_handle_t hal_ring_hdl)
{
}

/* TX MONITOR */
#ifdef QCA_MONITOR_2_0_SUPPORT

#if defined(TX_MONITOR_WORD_MASK)
typedef struct tx_fes_setup_compact_9224 hal_tx_fes_setup_t;
struct tx_fes_setup_compact_9224 {
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
/**
 * hal_tx_set_ppe_cmn_config_9224() - Set the PPE common config register
 * @hal_soc_hdl: HAL SoC handle
 * @cmn_cfg: Common PPE config
 *
 * Based on the PPE2TCL descriptor below errors, if the below register
 * values are set then the packets are forward to Tx rule handler if 1'0b
 * or to TCL exit base if 1'1b.
 *
 * Return: void
 */
static inline
void hal_tx_set_ppe_cmn_config_9224(hal_soc_handle_t hal_soc_hdl,
				    union hal_tx_cmn_config_ppe *cmn_cfg)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	union hal_tx_cmn_config_ppe *cfg =
		(union hal_tx_cmn_config_ppe *)cmn_cfg;
	uint32_t reg_addr, reg_val = 0;

	reg_addr = HWIO_TCL_R0_CMN_CONFIG_PPE_ADDR(MAC_TCL_REG_REG_BASE);

	reg_val = HAL_REG_READ(soc, reg_addr);

	reg_val &= ~HWIO_TCL_R0_CMN_CONFIG_PPE_DROP_PREC_ERR_EXCEPTION_BMSK;
	reg_val |=
		(cfg->drop_prec_err &
		 HWIO_TCL_R0_CMN_CONFIG_PPE_DROP_PREC_ERR_EXCEPTION_BMSK) <<
		 HWIO_TCL_R0_CMN_CONFIG_PPE_DROP_PREC_ERR_EXCEPTION_SHFT;

	reg_val &= ~HWIO_TCL_R0_CMN_CONFIG_PPE_FAKE_MAC_HDR_EXCEPTION_BMSK;
	reg_val |=
		(cfg->fake_mac_hdr &
		 HWIO_TCL_R0_CMN_CONFIG_PPE_FAKE_MAC_HDR_EXCEPTION_BMSK) <<
		 HWIO_TCL_R0_CMN_CONFIG_PPE_FAKE_MAC_HDR_EXCEPTION_SHFT;

	reg_val &= ~HWIO_TCL_R0_CMN_CONFIG_PPE_CPU_CODE_VALID_EXCEPTION_BMSK;
	reg_val |=
		(cfg->cpu_code_inv &
		 HWIO_TCL_R0_CMN_CONFIG_PPE_CPU_CODE_VALID_EXCEPTION_BMSK) <<
		 HWIO_TCL_R0_CMN_CONFIG_PPE_CPU_CODE_VALID_EXCEPTION_SHFT;

	reg_val &= ~HWIO_TCL_R0_CMN_CONFIG_PPE_L3_L4_CSUM_ERR_EXCEPTION_BMSK;
	reg_val |=
		(cfg->l3_l4_err &
		 HWIO_TCL_R0_CMN_CONFIG_PPE_L3_L4_CSUM_ERR_EXCEPTION_BMSK) <<
		 HWIO_TCL_R0_CMN_CONFIG_PPE_L3_L4_CSUM_ERR_EXCEPTION_SHFT;

	HAL_REG_WRITE(soc, reg_addr, reg_val);
}

/**
 * hal_tx_set_ppe_vp_entry_9224() - Set the PPE VP entry
 * @hal_soc_hdl: HAL SoC handle
 * @vp_cfg: PPE VP config
 * @ppe_vp_idx : PPE VP index to the table
 *
 * Return: void
 */
static inline
void hal_tx_set_ppe_vp_entry_9224(hal_soc_handle_t hal_soc_hdl,
				  union hal_tx_ppe_vp_config *cfg,
				  int ppe_vp_idx)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr;

	reg_addr = HWIO_TCL_R0_PPE_VP_CONFIG_TABLE_n_ADDR(MAC_TCL_REG_REG_BASE,
							  ppe_vp_idx);

	HAL_REG_WRITE(soc, reg_addr, cfg->val);
}

/**
 * hal_ppeds_cfg_ast_override_map_reg_9224() - Set the PPE index mapping table
 * @hal_soc_hdl: HAL SoC context
 * @idx: index into the table
 * @idx_map: HAL PPE INDESX MAPPING config
 *
 * Return: void
 */
static inline void
hal_ppeds_cfg_ast_override_map_reg_9224(hal_soc_handle_t hal_soc_hdl,
					uint8_t idx,
					union hal_tx_ppe_idx_map_config *idx_map)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr;

	reg_addr =
		HWIO_TCL_R0_PPE_INDEX_MAPPING_TABLE_n_ADDR(MAC_TCL_REG_REG_BASE,
							   idx);

	HAL_REG_WRITE(soc, reg_addr, idx_map->val);
}

/**
 * hal_tx_set_ppe_pri2tid_map1_9224()
 * @hal_soc_hdl: HAL SoC handle
 * @val : PRI to TID value
 * @map_no: Map number
 *
 * Return: void
 */
static inline
void hal_tx_set_ppe_pri2tid_map_9224(hal_soc_handle_t hal_soc_hdl,
				     uint32_t val, uint8_t map_no)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;

	if (map_no == 0)
		reg_addr =
		HWIO_TCL_R0_PPE_INT_PRI_TID_MAP0_ADDR(MAC_TCL_REG_REG_BASE);
	else
		reg_addr =
		HWIO_TCL_R0_PPE_INT_PRI_TID_MAP1_ADDR(MAC_TCL_REG_REG_BASE);

	reg_val |= val;
	HAL_REG_WRITE(soc, reg_addr, reg_val);
}

/**
 * hal_tx_set_ppe_pri2tid_map1_9224()
 * @hal_soc_hdl: HAL SoC handle
 * @val : PRI to TID value
 * @map_no: Map number
 *
 * Return: void
 */
static inline
void hal_tx_enable_pri2tid_map_9224(hal_soc_handle_t hal_soc_hdl,
				    bool val, uint8_t ppe_vp_idx)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;

	reg_addr = HWIO_TCL_R0_PPE_VP_CONFIG_TABLE_n_ADDR(MAC_TCL_REG_REG_BASE,
							  ppe_vp_idx);

	/*
	 * Drop precedence is enabled by default.
	 */
	reg_val = HAL_REG_READ(soc, reg_addr);

	reg_val &=
	  ~HWIO_TCL_R0_PPE_VP_CONFIG_TABLE_n_USE_PPE_INT_PRI_FOR_TID_BMSK;

	reg_val |=
	(val &
	 HWIO_TCL_R0_PPE_VP_CONFIG_TABLE_n_USE_PPE_INT_PRI_FOR_TID_BMSK) <<
	 HWIO_TCL_R0_PPE_VP_CONFIG_TABLE_n_USE_PPE_INT_PRI_FOR_TID_SHFT;

	HAL_REG_WRITE(soc, reg_addr, reg_val);
}

/**
 * hal_tx_update_ppe_pri2tid_9224()
 * @hal_soc_hdl: HAL SoC handle
 * @pri: INT_PRI
 * @tid: Wi-Fi TID
 *
 * Return: void
 */
static inline
void hal_tx_update_ppe_pri2tid_9224(hal_soc_handle_t hal_soc_hdl,
				    uint8_t pri, uint8_t tid)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0, mask, shift;

	/*
	 * INT_PRI 0..9 is in MAP0 register and INT_PRI 10..15
	 * is in MAP1 register.
	 */
	switch (pri) {
	case 0 ... 9:
	  reg_addr =
	  HWIO_TCL_R0_PPE_INT_PRI_TID_MAP0_ADDR(MAC_TCL_REG_REG_BASE);
	  mask =
	  (HWIO_TCL_R0_PPE_INT_PRI_TID_MAP0_INT_PRI_0_BMSK << (0x3 * pri));
	  shift = HWIO_TCL_R0_PPE_INT_PRI_TID_MAP0_INT_PRI_0_SHFT + (pri * 0x3);
		break;
	case 10 ... 15:
	   pri = pri - 10;
	   reg_addr =
	   HWIO_TCL_R0_PPE_INT_PRI_TID_MAP1_ADDR(MAC_TCL_REG_REG_BASE);
	   mask =
	   (HWIO_TCL_R0_PPE_INT_PRI_TID_MAP1_INT_PRI_10_BMSK << (0x3 * pri));
	   shift =
	   HWIO_TCL_R0_PPE_INT_PRI_TID_MAP1_INT_PRI_10_SHFT + (pri * 0x3);
		break;
	default:
		return;
	}

	reg_val = HAL_REG_READ(soc, reg_addr);
	reg_val &= ~mask;
	reg_val |= (pri << shift) & mask;

	HAL_REG_WRITE(soc, reg_addr, reg_val);
}
#endif /* _HAL_9224_TX_H_ */

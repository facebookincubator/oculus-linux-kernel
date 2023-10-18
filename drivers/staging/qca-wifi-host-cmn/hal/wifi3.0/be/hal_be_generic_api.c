/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#include <qdf_module.h>
#include "hal_be_api.h"
#include "hal_be_hw_headers.h"
#include "hal_be_reo.h"
#include "hal_tx.h"	//HAL_SET_FLD
#include "hal_be_rx.h"	//HAL_RX_BUF_RBM_GET
#include "rx_reo_queue_1k.h"
#include "hal_be_rx_tlv.h"

/*
 * The 4 bits REO destination ring value is defined as: 0: TCL
 * 1:SW1  2:SW2  3:SW3  4:SW4  5:Release  6:FW(WIFI)  7:SW5
 * 8:SW6 9:SW7  10:SW8  11: NOT_USED.
 *
 */
uint32_t reo_dest_ring_remap[] = {REO_REMAP_SW1, REO_REMAP_SW2,
				  REO_REMAP_SW3, REO_REMAP_SW4,
				  REO_REMAP_SW5, REO_REMAP_SW6,
				  REO_REMAP_SW7, REO_REMAP_SW8};
/*
 * WBM idle link descriptor for Return Buffer Manager in case of
 * multi-chip configuration.
 */
#define HAL_NUM_CHIPS 4
#define HAL_WBM_CHIP_INVALID	    0
#define HAL_WBM_CHIP0_IDLE_DESC_MAP 1
#define HAL_WBM_CHIP1_IDLE_DESC_MAP 2
#define HAL_WBM_CHIP2_IDLE_DESC_MAP 3
#define HAL_WBM_CHIP3_IDLE_DESC_MAP 12

uint8_t wbm_idle_link_bm_map[] = {HAL_WBM_CHIP0_IDLE_DESC_MAP,
				  HAL_WBM_CHIP1_IDLE_DESC_MAP,
				  HAL_WBM_CHIP2_IDLE_DESC_MAP,
				  HAL_WBM_CHIP3_IDLE_DESC_MAP};

#if defined(QDF_BIG_ENDIAN_MACHINE)
void hal_setup_reo_swap(struct hal_soc *soc)
{
	uint32_t reg_val;

	reg_val = HAL_REG_READ(soc, HWIO_REO_R0_CACHE_CTL_CONFIG_ADDR(
		REO_REG_REG_BASE));

	reg_val |= HAL_SM(HWIO_REO_R0_CACHE_CTL_CONFIG, WRITE_STRUCT_SWAP, 1);
	reg_val |= HAL_SM(HWIO_REO_R0_CACHE_CTL_CONFIG, READ_STRUCT_SWAP, 1);

	HAL_REG_WRITE(soc, HWIO_REO_R0_CACHE_CTL_CONFIG_ADDR(
		REO_REG_REG_BASE), reg_val);
}
#else
void hal_setup_reo_swap(struct hal_soc *soc)
{
}
#endif

/**
 * hal_tx_init_data_ring_be() - Initialize all the TCL Descriptors in SRNG
 * @hal_soc_hdl: Handle to HAL SoC structure
 * @hal_srng: Handle to HAL SRNG structure
 *
 * Return: none
 */
static void
hal_tx_init_data_ring_be(hal_soc_handle_t hal_soc_hdl,
			 hal_ring_handle_t hal_ring_hdl)
{
}

void hal_reo_setup_generic_be(struct hal_soc *soc, void *reoparams,
			      int qref_reset)
{
	uint32_t reg_val;
	struct hal_reo_params *reo_params = (struct hal_reo_params *)reoparams;

	reg_val = HAL_REG_READ(soc, HWIO_REO_R0_GENERAL_ENABLE_ADDR(
		REO_REG_REG_BASE));

	hal_reo_config(soc, reg_val, reo_params);
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
		HAL_REG_WRITE(soc,
			HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR(
			REO_REG_REG_BASE),
			reo_params->remap1);

		hal_debug("HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR 0x%x",
			  HAL_REG_READ(soc,
				       HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_ADDR(
				       REO_REG_REG_BASE)));

		HAL_REG_WRITE(soc,
			HWIO_REO_R0_DESTINATION_RING_CTRL_IX_3_ADDR(
			REO_REG_REG_BASE),
			reo_params->remap2);

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
}

void hal_set_link_desc_addr_be(void *desc, uint32_t cookie,
			       qdf_dma_addr_t link_desc_paddr,
			       uint8_t bm_id)
{
	uint32_t *buf_addr = (uint32_t *)desc;

	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO, BUFFER_ADDR_31_0,
			   link_desc_paddr & 0xffffffff);
	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO, BUFFER_ADDR_39_32,
			   (uint64_t)link_desc_paddr >> 32);
	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO, RETURN_BUFFER_MANAGER,
			   bm_id);
	HAL_DESC_SET_FIELD(buf_addr, BUFFER_ADDR_INFO, SW_BUFFER_COOKIE,
			   cookie);
}

static uint16_t hal_get_rx_max_ba_window_be(int tid)
{
	return  HAL_RX_BA_WINDOW_256;
}

static uint32_t hal_get_reo_qdesc_size_be(uint32_t ba_window_size, int tid)
{
	/* Hardcode the ba_window_size to HAL_RX_MAX_BA_WINDOW for
	 * NON_QOS_TID until HW issues are resolved.
	 */
	if (tid != HAL_NON_QOS_TID)
		ba_window_size = hal_get_rx_max_ba_window_be(tid);

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

	return sizeof(struct rx_reo_queue) +
		(3 * sizeof(struct rx_reo_queue_ext));
}

void *hal_rx_msdu_ext_desc_info_get_ptr_be(void *msdu_details_ptr)
{
	return HAL_RX_MSDU_EXT_DESC_INFO_GET(msdu_details_ptr);
}

#if defined(QCA_WIFI_KIWI) && !defined(QCA_WIFI_KIWI_V2)
static inline uint32_t
hal_wbm2sw_release_source_get(void *hal_desc, enum hal_be_wbm_release_dir dir)
{
	uint32_t buf_src;

	buf_src = HAL_WBM2SW_RELEASE_SRC_GET(hal_desc);
	switch (buf_src) {
	case HAL_BE_RX_WBM_ERR_SRC_RXDMA:
		return HAL_RX_WBM_ERR_SRC_RXDMA;
	case HAL_BE_RX_WBM_ERR_SRC_REO:
		return HAL_RX_WBM_ERR_SRC_REO;
	case HAL_BE_RX_WBM_ERR_SRC_FW_RX:
		if (dir != HAL_BE_WBM_RELEASE_DIR_RX)
			qdf_assert_always(0);
		return HAL_RX_WBM_ERR_SRC_FW;
	case HAL_BE_RX_WBM_ERR_SRC_SW_RX:
		if (dir != HAL_BE_WBM_RELEASE_DIR_RX)
			qdf_assert_always(0);
		return HAL_RX_WBM_ERR_SRC_SW;
	case HAL_BE_RX_WBM_ERR_SRC_TQM:
		return HAL_RX_WBM_ERR_SRC_TQM;
	case HAL_BE_RX_WBM_ERR_SRC_FW_TX:
		if (dir != HAL_BE_WBM_RELEASE_DIR_TX)
			qdf_assert_always(0);
		return HAL_RX_WBM_ERR_SRC_FW;
	case HAL_BE_RX_WBM_ERR_SRC_SW_TX:
		if (dir != HAL_BE_WBM_RELEASE_DIR_TX)
			qdf_assert_always(0);
		return HAL_RX_WBM_ERR_SRC_SW;
	default:
		qdf_assert_always(0);
	}

	return buf_src;
}
#else
static inline uint32_t
hal_wbm2sw_release_source_get(void *hal_desc, enum hal_be_wbm_release_dir dir)
{
	return HAL_WBM2SW_RELEASE_SRC_GET(hal_desc);
}
#endif

uint32_t hal_tx_comp_get_buffer_source_generic_be(void *hal_desc)
{
	return hal_wbm2sw_release_source_get(hal_desc,
					     HAL_BE_WBM_RELEASE_DIR_TX);
}

/**
 * hal_tx_comp_get_release_reason_generic_be() - TQM Release reason
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will return the type of pointer - buffer or descriptor
 *
 * Return: buffer type
 */
static uint8_t hal_tx_comp_get_release_reason_generic_be(void *hal_desc)
{
	uint32_t comp_desc = *(uint32_t *)(((uint8_t *)hal_desc) +
			WBM2SW_COMPLETION_RING_TX_TQM_RELEASE_REASON_OFFSET);

	return (comp_desc &
		WBM2SW_COMPLETION_RING_TX_TQM_RELEASE_REASON_MASK) >>
		WBM2SW_COMPLETION_RING_TX_TQM_RELEASE_REASON_LSB;
}

/**
 * hal_get_wbm_internal_error_generic_be() - is WBM internal error
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will return 0 or 1  - is it WBM internal error or not
 *
 * Return: uint8_t
 */
static uint8_t hal_get_wbm_internal_error_generic_be(void *hal_desc)
{
	/*
	 * TODO -  This func is called by tx comp and wbm error handler
	 * Check if one needs to use WBM2SW-TX and other WBM2SW-RX
	 */
	uint32_t comp_desc =
		*(uint32_t *)(((uint8_t *)hal_desc) +
			      HAL_WBM_INTERNAL_ERROR_OFFSET);

	return (comp_desc & HAL_WBM_INTERNAL_ERROR_MASK) >>
		HAL_WBM_INTERNAL_ERROR_LSB;
}

/**
 * hal_rx_wbm_err_src_get_be() - Get WBM error source from descriptor
 * @ring_desc: ring descriptor
 *
 * Return: wbm error source
 */
static uint32_t hal_rx_wbm_err_src_get_be(hal_ring_desc_t ring_desc)
{
	return hal_wbm2sw_release_source_get(ring_desc,
					     HAL_BE_WBM_RELEASE_DIR_RX);
}

/**
 * hal_rx_ret_buf_manager_get_be() - Get return buffer manager from ring desc
 * @ring_desc: ring descriptor
 *
 * Return: rbm
 */
uint8_t hal_rx_ret_buf_manager_get_be(hal_ring_desc_t ring_desc)
{
	/*
	 * The following macro takes buf_addr_info as argument,
	 * but since buf_addr_info is the first field in ring_desc
	 * Hence the following call is OK
	 */
	return HAL_RX_BUF_RBM_GET(ring_desc);
}

#define HAL_RX_WBM_REO_PUSH_REASON_GET(wbm_desc) (((*(((uint32_t *)wbm_desc) + \
		(WBM2SW_COMPLETION_RING_RX_REO_PUSH_REASON_OFFSET >> 2))) & \
		WBM2SW_COMPLETION_RING_RX_REO_PUSH_REASON_MASK) >> \
		WBM2SW_COMPLETION_RING_RX_REO_PUSH_REASON_LSB)

#define HAL_RX_WBM_REO_ERROR_CODE_GET(wbm_desc) (((*(((uint32_t *)wbm_desc) + \
		(WBM2SW_COMPLETION_RING_RX_REO_ERROR_CODE_OFFSET >> 2))) & \
		WBM2SW_COMPLETION_RING_RX_REO_ERROR_CODE_MASK) >> \
		WBM2SW_COMPLETION_RING_RX_REO_ERROR_CODE_LSB)

#define HAL_RX_WBM_RXDMA_PUSH_REASON_GET(wbm_desc)	\
	(((*(((uint32_t *)wbm_desc) +			\
	(WBM2SW_COMPLETION_RING_RX_RXDMA_PUSH_REASON_OFFSET >> 2))) & \
	WBM2SW_COMPLETION_RING_RX_RXDMA_PUSH_REASON_MASK) >>	\
	WBM2SW_COMPLETION_RING_RX_RXDMA_PUSH_REASON_LSB)

#define HAL_RX_WBM_RXDMA_ERROR_CODE_GET(wbm_desc)	\
	(((*(((uint32_t *)wbm_desc) +			\
	(WBM2SW_COMPLETION_RING_RX_RXDMA_ERROR_CODE_OFFSET >> 2))) & \
	WBM2SW_COMPLETION_RING_RX_RXDMA_ERROR_CODE_MASK) >>	\
	WBM2SW_COMPLETION_RING_RX_RXDMA_ERROR_CODE_LSB)

/**
 * hal_rx_wbm_err_info_get_generic_be(): Retrieves WBM error code and reason and
 *	save it to hal_wbm_err_desc_info structure passed by caller
 * @wbm_desc: wbm ring descriptor
 * @wbm_er_info1: hal_wbm_err_desc_info structure, output parameter.
 * Return: void
 */
void hal_rx_wbm_err_info_get_generic_be(void *wbm_desc, void *wbm_er_info1)
{
	struct hal_wbm_err_desc_info *wbm_er_info =
		(struct hal_wbm_err_desc_info *)wbm_er_info1;

	wbm_er_info->wbm_err_src = hal_rx_wbm_err_src_get_be(wbm_desc);
	wbm_er_info->reo_psh_rsn = HAL_RX_WBM_REO_PUSH_REASON_GET(wbm_desc);
	wbm_er_info->reo_err_code = HAL_RX_WBM_REO_ERROR_CODE_GET(wbm_desc);
	wbm_er_info->rxdma_psh_rsn = HAL_RX_WBM_RXDMA_PUSH_REASON_GET(wbm_desc);
	wbm_er_info->rxdma_err_code = HAL_RX_WBM_RXDMA_ERROR_CODE_GET(wbm_desc);
}

static void hal_rx_reo_buf_paddr_get_be(hal_ring_desc_t rx_desc,
					struct hal_buf_info *buf_info)
{
	struct reo_destination_ring *reo_ring =
		 (struct reo_destination_ring *)rx_desc;

	buf_info->paddr =
	 (HAL_RX_REO_BUFFER_ADDR_31_0_GET(reo_ring) |
	  ((uint64_t)(HAL_RX_REO_BUFFER_ADDR_39_32_GET(reo_ring)) << 32));
	buf_info->sw_cookie = HAL_RX_REO_BUF_COOKIE_GET(reo_ring);
}

static void hal_rx_msdu_link_desc_set_be(hal_soc_handle_t hal_soc_hdl,
					 void *src_srng_desc,
					 hal_buff_addrinfo_t buf_addr_info,
					 uint8_t bm_action)
{
	/*
	 * The offsets for fields used in this function are same in
	 * wbm_release_ring for Lithium and wbm_release_ring_tx
	 * for Beryllium. hence we can use wbm_release_ring directly.
	 */
	struct wbm_release_ring *wbm_rel_srng =
			(struct wbm_release_ring *)src_srng_desc;
	uint32_t addr_31_0;
	uint8_t addr_39_32;

	/* Structure copy !!! */
	wbm_rel_srng->released_buff_or_desc_addr_info =
				*((struct buffer_addr_info *)buf_addr_info);

	addr_31_0 =
	wbm_rel_srng->released_buff_or_desc_addr_info.buffer_addr_31_0;
	addr_39_32 =
	wbm_rel_srng->released_buff_or_desc_addr_info.buffer_addr_39_32;

	HAL_DESC_SET_FIELD(src_srng_desc, HAL_SW2WBM_RELEASE_RING,
			   RELEASE_SOURCE_MODULE, HAL_RX_WBM_ERR_SRC_SW);
	HAL_DESC_SET_FIELD(src_srng_desc, HAL_SW2WBM_RELEASE_RING, BM_ACTION,
			   bm_action);
	HAL_DESC_SET_FIELD(src_srng_desc, HAL_SW2WBM_RELEASE_RING,
			   BUFFER_OR_DESC_TYPE,
			   HAL_RX_WBM_BUF_TYPE_MSDU_LINK_DESC);

	/* WBM error is indicated when any of the link descriptors given to
	 * WBM has a NULL address, and one those paths is the link descriptors
	 * released from host after processing RXDMA errors,
	 * or from Rx defrag path, and we want to add an assert here to ensure
	 * host is not releasing descriptors with NULL address.
	 */

	if (qdf_unlikely(!addr_31_0 && !addr_39_32)) {
		hal_dump_wbm_rel_desc(src_srng_desc);
		qdf_assert_always(0);
	}
}

/**
 * hal_rx_reo_ent_buf_paddr_get_be: Gets the physical address and
 * cookie from the REO entrance ring element
 *
 * @ hal_rx_desc_cookie: Opaque cookie pointer used by HAL to get to
 * the current descriptor
 * @ buf_info: structure to return the buffer information
 * @ msdu_cnt: pointer to msdu count in MPDU
 * Return: void
 */
static
void hal_rx_buf_cookie_rbm_get_be(uint32_t *buf_addr_info_hdl,
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
	buf_info->rbm = hal_rx_ret_buf_manager_get_be(
						(hal_ring_desc_t)buf_addr_info);
}

/*
 * hal_rxdma_buff_addr_info_set_be() - set the buffer_addr_info of the
 *				    rxdma ring entry.
 * @rxdma_entry: descriptor entry
 * @paddr: physical address of nbuf data pointer.
 * @cookie: SW cookie used as a index to SW rx desc.
 * @manager: who owns the nbuf (host, NSS, etc...).
 *
 */
static inline void
hal_rxdma_buff_addr_info_set_be(void *rxdma_entry,
				qdf_dma_addr_t paddr, uint32_t cookie,
				uint8_t manager)
{
	uint32_t paddr_lo = ((u64)paddr & 0x00000000ffffffff);
	uint32_t paddr_hi = ((u64)paddr & 0xffffffff00000000) >> 32;

	HAL_RXDMA_PADDR_LO_SET(rxdma_entry, paddr_lo);
	HAL_RXDMA_PADDR_HI_SET(rxdma_entry, paddr_hi);
	HAL_RXDMA_COOKIE_SET(rxdma_entry, cookie);
	HAL_RXDMA_MANAGER_SET(rxdma_entry, manager);
}

/**
 * hal_rx_get_reo_error_code_be() - Get REO error code from ring desc
 * @rx_desc: rx descriptor
 *
 * Return: REO error code
 */
static uint32_t hal_rx_get_reo_error_code_be(hal_ring_desc_t rx_desc)
{
	struct reo_destination_ring *reo_desc =
			(struct reo_destination_ring *)rx_desc;

	return HAL_RX_REO_ERROR_GET(reo_desc);
}

/**
 * hal_gen_reo_remap_val_generic_be() - Generate the reo map value
 * @ix0_map: mapping values for reo
 *
 * Return: IX0 reo remap register value to be written
 */
static uint32_t
hal_gen_reo_remap_val_generic_be(enum hal_reo_remap_reg remap_reg,
				 uint8_t *ix0_map)
{
	uint32_t ix_val = 0;

	switch (remap_reg) {
	case HAL_REO_REMAP_REG_IX0:
		ix_val = HAL_REO_REMAP_IX0(ix0_map[0], 0) |
			HAL_REO_REMAP_IX0(ix0_map[1], 1) |
			HAL_REO_REMAP_IX0(ix0_map[2], 2) |
			HAL_REO_REMAP_IX0(ix0_map[3], 3) |
			HAL_REO_REMAP_IX0(ix0_map[4], 4) |
			HAL_REO_REMAP_IX0(ix0_map[5], 5) |
			HAL_REO_REMAP_IX0(ix0_map[6], 6) |
			HAL_REO_REMAP_IX0(ix0_map[7], 7);
		break;
	case HAL_REO_REMAP_REG_IX2:
		ix_val = HAL_REO_REMAP_IX2(ix0_map[0], 16) |
			HAL_REO_REMAP_IX2(ix0_map[1], 17) |
			HAL_REO_REMAP_IX2(ix0_map[2], 18) |
			HAL_REO_REMAP_IX2(ix0_map[3], 19) |
			HAL_REO_REMAP_IX2(ix0_map[4], 20) |
			HAL_REO_REMAP_IX2(ix0_map[5], 21) |
			HAL_REO_REMAP_IX2(ix0_map[6], 22) |
			HAL_REO_REMAP_IX2(ix0_map[7], 23);
		break;
	default:
		break;
	}

	return ix_val;
}

static uint8_t hal_rx_err_status_get_be(hal_ring_desc_t rx_desc)
{
	return HAL_RX_ERROR_STATUS_GET(rx_desc);
}

static QDF_STATUS hal_reo_status_update_be(hal_soc_handle_t hal_soc_hdl,
					   hal_ring_desc_t reo_desc,
					   void *st_handle,
					   uint32_t tlv, int *num_ref)
{
	union hal_reo_status *reo_status_ref;

	reo_status_ref = (union hal_reo_status *)st_handle;

	switch (tlv) {
	case HAL_REO_QUEUE_STATS_STATUS_TLV:
		hal_reo_queue_stats_status_be(reo_desc,
					      &reo_status_ref->queue_status,
					      hal_soc_hdl);
		*num_ref = reo_status_ref->queue_status.header.cmd_num;
		break;
	case HAL_REO_FLUSH_QUEUE_STATUS_TLV:
		hal_reo_flush_queue_status_be(reo_desc,
					      &reo_status_ref->fl_queue_status,
					      hal_soc_hdl);
		*num_ref = reo_status_ref->fl_queue_status.header.cmd_num;
		break;
	case HAL_REO_FLUSH_CACHE_STATUS_TLV:
		hal_reo_flush_cache_status_be(reo_desc,
					      &reo_status_ref->fl_cache_status,
					      hal_soc_hdl);
		*num_ref = reo_status_ref->fl_cache_status.header.cmd_num;
		break;
	case HAL_REO_UNBLK_CACHE_STATUS_TLV:
		hal_reo_unblock_cache_status_be
			(reo_desc, hal_soc_hdl,
			 &reo_status_ref->unblk_cache_status);
		*num_ref = reo_status_ref->unblk_cache_status.header.cmd_num;
		break;
	case HAL_REO_TIMOUT_LIST_STATUS_TLV:
		hal_reo_flush_timeout_list_status_be(
					reo_desc,
					&reo_status_ref->fl_timeout_status,
					hal_soc_hdl);
		*num_ref = reo_status_ref->fl_timeout_status.header.cmd_num;
		break;
	case HAL_REO_DESC_THRES_STATUS_TLV:
		hal_reo_desc_thres_reached_status_be(
						reo_desc,
						&reo_status_ref->thres_status,
						hal_soc_hdl);
		*num_ref = reo_status_ref->thres_status.header.cmd_num;
		break;
	case HAL_REO_UPDATE_RX_QUEUE_STATUS_TLV:
		hal_reo_rx_update_queue_status_be(
					reo_desc,
					&reo_status_ref->rx_queue_status,
					hal_soc_hdl);
		*num_ref = reo_status_ref->rx_queue_status.header.cmd_num;
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_DP_REO, QDF_TRACE_LEVEL_WARN,
			  "hal_soc %pK: no handler for TLV:%d",
			   hal_soc_hdl, tlv);
		return QDF_STATUS_E_FAILURE;
	} /* switch */

	return QDF_STATUS_SUCCESS;
}

static uint8_t hal_rx_reo_buf_type_get_be(hal_ring_desc_t rx_desc)
{
	return HAL_RX_REO_BUF_TYPE_GET(rx_desc);
}

#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
#define HAL_WBM_MISC_CONTROL_SPARE_CONTROL_FIELD_BIT15 0x8000
#endif
void hal_cookie_conversion_reg_cfg_be(hal_soc_handle_t hal_soc_hdl,
				      struct hal_hw_cc_config *cc_cfg)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_cookie_conversion_reg_cfg_be(hal_soc_hdl, cc_cfg);
}
qdf_export_symbol(hal_cookie_conversion_reg_cfg_be);

static inline void
hal_msdu_desc_info_set_be(hal_soc_handle_t hal_soc_hdl,
			  void *msdu_desc, uint32_t dst_ind,
			  uint32_t nbuf_len)
{
	struct rx_msdu_desc_info *msdu_desc_info =
		(struct rx_msdu_desc_info *)msdu_desc;
	struct rx_msdu_ext_desc_info *msdu_ext_desc_info =
		(struct rx_msdu_ext_desc_info *)(msdu_desc_info + 1);

	HAL_RX_MSDU_DESC_INFO_SET(msdu_desc_info,
				  FIRST_MSDU_IN_MPDU_FLAG, 1);
	HAL_RX_MSDU_DESC_INFO_SET(msdu_desc_info,
				  LAST_MSDU_IN_MPDU_FLAG, 1);
	HAL_RX_MSDU_DESC_INFO_SET(msdu_desc_info,
				  MSDU_CONTINUATION, 0x0);
	HAL_RX_MSDU_DESC_INFO_SET(msdu_desc_info,
				  MSDU_LENGTH, nbuf_len);
	HAL_RX_MSDU_DESC_INFO_SET(msdu_desc_info,
				  SA_IS_VALID, 1);
	HAL_RX_MSDU_DESC_INFO_SET(msdu_desc_info,
				  DA_IS_VALID, 1);
	HAL_RX_MSDU_REO_DST_IND_SET(msdu_ext_desc_info,
				    REO_DESTINATION_INDICATION, dst_ind);
}

static inline void
hal_mpdu_desc_info_set_be(hal_soc_handle_t hal_soc_hdl,
			  void *ent_desc,
			  void *mpdu_desc,
			  uint32_t seq_no)
{
	struct rx_mpdu_desc_info *mpdu_desc_info =
			(struct rx_mpdu_desc_info *)mpdu_desc;
	uint8_t *desc = (uint8_t *)ent_desc;

	HAL_RX_FLD_SET(desc, REO_ENTRANCE_RING,
		       MPDU_SEQUENCE_NUMBER, seq_no);

	HAL_RX_MPDU_DESC_INFO_SET(mpdu_desc_info,
				  MSDU_COUNT, 0x1);
	/* unset frag bit */
	HAL_RX_MPDU_DESC_INFO_SET(mpdu_desc_info,
				  FRAGMENT_FLAG, 0x0);
	HAL_RX_MPDU_DESC_INFO_SET(mpdu_desc_info,
				  RAW_MPDU, 0x0);
}

/**
 * hal_rx_msdu_reo_dst_ind_get: Gets the REO
 * destination ring ID from the msdu desc info
 *
 * @msdu_link_desc : Opaque cookie pointer used by HAL to get to
 * the current descriptor
 *
 * Return: dst_ind (REO destination ring ID)
 */
static inline
uint32_t hal_rx_msdu_reo_dst_ind_get_be(hal_soc_handle_t hal_soc_hdl,
					void *msdu_link_desc)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct rx_msdu_details *msdu_details;
	struct rx_msdu_desc_info *msdu_desc_info;
	struct rx_msdu_link *msdu_link = (struct rx_msdu_link *)msdu_link_desc;
	uint32_t dst_ind;

	msdu_details = hal_rx_link_desc_msdu0_ptr(msdu_link, hal_soc);

	/* The first msdu in the link should exist */
	msdu_desc_info = hal_rx_msdu_ext_desc_info_get_ptr(&msdu_details[0],
							   hal_soc);
	dst_ind = HAL_RX_MSDU_REO_DST_IND_GET(msdu_desc_info);
	return dst_ind;
}

uint32_t
hal_reo_ix_remap_value_get_be(hal_soc_handle_t hal_soc_hdl,
			      uint8_t rx_ring_mask)
{
	uint32_t num_rings = 0;
	uint32_t i = 0;
	uint32_t ring_remap_arr[HAL_MAX_REO2SW_RINGS] = {0};
	uint32_t reo_remap_val = 0;
	uint32_t ring_idx = 0;
	uint8_t ix_map[HAL_NUM_RX_RING_PER_IX_MAP] = {0};

	/* create reo ring remap array */
	while (i < HAL_MAX_REO2SW_RINGS) {
		if (rx_ring_mask & (1 << i)) {
			ring_remap_arr[num_rings] = reo_dest_ring_remap[i];
			num_rings++;
		}
		i++;
	}

	for (i = 0; i < HAL_NUM_RX_RING_PER_IX_MAP; i++) {
		if (rx_ring_mask) {
			ix_map[i] = ring_remap_arr[ring_idx];
			ring_idx = ((ring_idx + 1) % num_rings);
		} else {
			/* if ring mask is zero configure to release to WBM */
			ix_map[i] = REO_REMAP_RELEASE;
		}
	}

	reo_remap_val = HAL_REO_REMAP_IX0(ix_map[0], 0) |
					  HAL_REO_REMAP_IX0(ix_map[1], 1) |
					  HAL_REO_REMAP_IX0(ix_map[2], 2) |
					  HAL_REO_REMAP_IX0(ix_map[3], 3) |
					  HAL_REO_REMAP_IX0(ix_map[4], 4) |
					  HAL_REO_REMAP_IX0(ix_map[5], 5) |
					  HAL_REO_REMAP_IX0(ix_map[6], 6) |
					  HAL_REO_REMAP_IX0(ix_map[7], 7);

	return reo_remap_val;
}

qdf_export_symbol(hal_reo_ix_remap_value_get_be);

uint8_t hal_reo_ring_remap_value_get_be(uint8_t rx_ring_id)
{
	if (rx_ring_id >= HAL_MAX_REO2SW_RINGS)
		return REO_REMAP_RELEASE;

	return reo_dest_ring_remap[rx_ring_id];
}

qdf_export_symbol(hal_reo_ring_remap_value_get_be);

uint8_t hal_get_idle_link_bm_id_be(uint8_t chip_id)
{
	if (chip_id >= HAL_NUM_CHIPS)
		return HAL_WBM_CHIP_INVALID;

	return wbm_idle_link_bm_map[chip_id];
}

#ifdef DP_FEATURE_HW_COOKIE_CONVERSION
#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
static inline void
hal_rx_wbm_rel_buf_paddr_get_be(hal_ring_desc_t rx_desc,
				struct hal_buf_info *buf_info)
{
	if (hal_rx_wbm_get_cookie_convert_done(rx_desc))
		buf_info->paddr =
			(HAL_RX_WBM_COMP_BUF_ADDR_31_0_GET(rx_desc) |
			 ((uint64_t)(HAL_RX_WBM_COMP_BUF_ADDR_39_32_GET(rx_desc)) << 32));
	else
		buf_info->paddr =
			(HAL_RX_WBM_BUF_ADDR_31_0_GET(rx_desc) |
			 ((uint64_t)(HAL_RX_WBM_BUF_ADDR_39_32_GET(rx_desc)) << 32));
}
#else
static inline void
hal_rx_wbm_rel_buf_paddr_get_be(hal_ring_desc_t rx_desc,
				struct hal_buf_info *buf_info)
{
	buf_info->paddr =
		(HAL_RX_WBM_COMP_BUF_ADDR_31_0_GET(rx_desc) |
		 ((uint64_t)(HAL_RX_WBM_COMP_BUF_ADDR_39_32_GET(rx_desc)) << 32));
}
#endif
#else /* !DP_FEATURE_HW_COOKIE_CONVERSION */
static inline void
hal_rx_wbm_rel_buf_paddr_get_be(hal_ring_desc_t rx_desc,
				struct hal_buf_info *buf_info)
{
	buf_info->paddr =
		(HAL_RX_WBM_BUF_ADDR_31_0_GET(rx_desc) |
		 ((uint64_t)(HAL_RX_WBM_BUF_ADDR_39_32_GET(rx_desc)) << 32));
}
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * hal_unregister_reo_send_cmd_be() - Unregister Reo send command callback.
 * @hal_soc_hdl: HAL soc handle
 *
 * Return: None
 */
static
void hal_unregister_reo_send_cmd_be(struct hal_soc *hal_soc)
{
	hal_soc->ops->hal_reo_send_cmd = NULL;
}

/**
 * hal_register_reo_send_cmd_be() - Register Reo send command callback.
 * @hal_soc_hdl: HAL soc handle
 *
 * Return: None
 */
static
void hal_register_reo_send_cmd_be(struct hal_soc *hal_soc)
{
	hal_soc->ops->hal_reo_send_cmd = hal_reo_send_cmd_be;
}

/**
 * hal_reset_rx_reo_tid_q_be() - reset the reo tid queue.
 * @hal_soc_hdl: HAL soc handle
 * @hw_qdesc_vaddr:start address of the tid queue
 * @size:size of address pointed by hw_qdesc_vaddr
 *
 * Return: None
 */
static void
hal_reset_rx_reo_tid_q_be(struct hal_soc *hal_soc, void *hw_qdesc_vaddr,
			  uint32_t size)
{
	struct rx_reo_queue *hw_qdesc = (struct rx_reo_queue *)hw_qdesc_vaddr;
	int i;

	if (!hw_qdesc)
		return;

	hw_qdesc->svld = 0;
	hw_qdesc->ssn = 0;
	hw_qdesc->current_index = 0;
	hw_qdesc->pn_valid = 0;
	hw_qdesc->pn_31_0 = 0;
	hw_qdesc->pn_63_32 = 0;
	hw_qdesc->pn_95_64 = 0;
	hw_qdesc->pn_127_96 = 0;
	hw_qdesc->last_rx_enqueue_timestamp = 0;
	hw_qdesc->last_rx_dequeue_timestamp = 0;
	hw_qdesc->ptr_to_next_aging_queue_39_32 = 0;
	hw_qdesc->ptr_to_next_aging_queue_31_0 = 0;
	hw_qdesc->ptr_to_previous_aging_queue_31_0 = 0;
	hw_qdesc->ptr_to_previous_aging_queue_39_32 = 0;
	hw_qdesc->rx_bitmap_31_0 = 0;
	hw_qdesc->rx_bitmap_63_32 = 0;
	hw_qdesc->rx_bitmap_95_64 = 0;
	hw_qdesc->rx_bitmap_127_96 = 0;
	hw_qdesc->rx_bitmap_159_128 = 0;
	hw_qdesc->rx_bitmap_191_160 = 0;
	hw_qdesc->rx_bitmap_223_192 = 0;
	hw_qdesc->rx_bitmap_255_224 = 0;
	hw_qdesc->rx_bitmap_287_256 = 0;
	hw_qdesc->current_msdu_count = 0;
	hw_qdesc->current_mpdu_count = 0;
	hw_qdesc->last_sn_reg_index = 0;

	if (size > sizeof(struct rx_reo_queue)) {
		struct rx_reo_queue_ext *ext_desc;
		struct rx_reo_queue_1k *kdesc;

		i = ((size - sizeof(struct rx_reo_queue)) /
				sizeof(struct rx_reo_queue_ext));

		if (i > 10) {
			i = 10;
			kdesc = (struct rx_reo_queue_1k *)
				(hw_qdesc_vaddr + sizeof(struct rx_reo_queue) +
				 (10 * sizeof(struct rx_reo_queue_ext)));

			kdesc->rx_bitmap_319_288 = 0;
			kdesc->rx_bitmap_351_320 = 0;
			kdesc->rx_bitmap_383_352 = 0;
			kdesc->rx_bitmap_415_384 = 0;
			kdesc->rx_bitmap_447_416 = 0;
			kdesc->rx_bitmap_479_448 = 0;
			kdesc->rx_bitmap_511_480 = 0;
			kdesc->rx_bitmap_543_512 = 0;
			kdesc->rx_bitmap_575_544 = 0;
			kdesc->rx_bitmap_607_576 = 0;
			kdesc->rx_bitmap_639_608 = 0;
			kdesc->rx_bitmap_671_640 = 0;
			kdesc->rx_bitmap_703_672 = 0;
			kdesc->rx_bitmap_735_704 = 0;
			kdesc->rx_bitmap_767_736 = 0;
			kdesc->rx_bitmap_799_768 = 0;
			kdesc->rx_bitmap_831_800 = 0;
			kdesc->rx_bitmap_863_832 = 0;
			kdesc->rx_bitmap_895_864 = 0;
			kdesc->rx_bitmap_927_896 = 0;
			kdesc->rx_bitmap_959_928 = 0;
			kdesc->rx_bitmap_991_960 = 0;
			kdesc->rx_bitmap_1023_992 = 0;
		}

		ext_desc = (struct rx_reo_queue_ext *)
			(hw_qdesc_vaddr + (sizeof(struct rx_reo_queue)));

		while (i > 0) {
			qdf_mem_zero(&ext_desc->mpdu_link_pointer_0,
				     (15 * sizeof(struct rx_mpdu_link_ptr)));

			ext_desc++;
			i--;
		}
	}
}
#endif

/**
 * hal_hw_txrx_default_ops_attach_be() - Attach the default hal ops for
 *		beryllium chipsets.
 * @hal_soc_hdl: HAL soc handle
 *
 * Return: None
 */
void hal_hw_txrx_default_ops_attach_be(struct hal_soc *hal_soc)
{
	hal_soc->ops->hal_get_reo_qdesc_size = hal_get_reo_qdesc_size_be;
	hal_soc->ops->hal_get_rx_max_ba_window = hal_get_rx_max_ba_window_be;
	hal_soc->ops->hal_set_link_desc_addr = hal_set_link_desc_addr_be;
	hal_soc->ops->hal_tx_init_data_ring = hal_tx_init_data_ring_be;
	hal_soc->ops->hal_get_reo_reg_base_offset =
					hal_get_reo_reg_base_offset_be;
	hal_soc->ops->hal_reo_setup = hal_reo_setup_generic_be;
	hal_soc->ops->hal_rx_reo_buf_paddr_get = hal_rx_reo_buf_paddr_get_be;
	hal_soc->ops->hal_rx_msdu_link_desc_set = hal_rx_msdu_link_desc_set_be;
	hal_soc->ops->hal_rx_buf_cookie_rbm_get = hal_rx_buf_cookie_rbm_get_be;

	hal_soc->ops->hal_rx_ret_buf_manager_get =
						hal_rx_ret_buf_manager_get_be;
	hal_soc->ops->hal_rxdma_buff_addr_info_set =
					hal_rxdma_buff_addr_info_set_be;
	hal_soc->ops->hal_rx_msdu_flags_get = hal_rx_msdu_flags_get_be;
	hal_soc->ops->hal_rx_get_reo_error_code = hal_rx_get_reo_error_code_be;
	hal_soc->ops->hal_gen_reo_remap_val =
				hal_gen_reo_remap_val_generic_be;
	hal_soc->ops->hal_tx_comp_get_buffer_source =
				hal_tx_comp_get_buffer_source_generic_be;
	hal_soc->ops->hal_tx_comp_get_release_reason =
				hal_tx_comp_get_release_reason_generic_be;
	hal_soc->ops->hal_get_wbm_internal_error =
					hal_get_wbm_internal_error_generic_be;
	hal_soc->ops->hal_rx_mpdu_desc_info_get =
				hal_rx_mpdu_desc_info_get_be;
	hal_soc->ops->hal_rx_err_status_get = hal_rx_err_status_get_be;
	hal_soc->ops->hal_rx_reo_buf_type_get = hal_rx_reo_buf_type_get_be;
	hal_soc->ops->hal_rx_wbm_err_src_get = hal_rx_wbm_err_src_get_be;
	hal_soc->ops->hal_rx_wbm_rel_buf_paddr_get =
					hal_rx_wbm_rel_buf_paddr_get_be;

	hal_soc->ops->hal_reo_send_cmd = hal_reo_send_cmd_be;
	hal_soc->ops->hal_reo_qdesc_setup = hal_reo_qdesc_setup_be;
	hal_soc->ops->hal_reo_status_update = hal_reo_status_update_be;
	hal_soc->ops->hal_get_tlv_hdr_size = hal_get_tlv_hdr_size_be;
	hal_soc->ops->hal_rx_msdu_reo_dst_ind_get =
						hal_rx_msdu_reo_dst_ind_get_be;
	hal_soc->ops->hal_get_idle_link_bm_id = hal_get_idle_link_bm_id_be;
	hal_soc->ops->hal_rx_msdu_ext_desc_info_get_ptr =
					hal_rx_msdu_ext_desc_info_get_ptr_be;
	hal_soc->ops->hal_msdu_desc_info_set = hal_msdu_desc_info_set_be;
	hal_soc->ops->hal_mpdu_desc_info_set = hal_mpdu_desc_info_set_be;
#ifdef DP_UMAC_HW_RESET_SUPPORT
	hal_soc->ops->hal_unregister_reo_send_cmd =
					hal_unregister_reo_send_cmd_be;
	hal_soc->ops->hal_register_reo_send_cmd = hal_register_reo_send_cmd_be;
	hal_soc->ops->hal_reset_rx_reo_tid_q = hal_reset_rx_reo_tid_q_be;
#endif
	hal_soc->ops->hal_rx_tlv_get_pn_num = hal_rx_tlv_get_pn_num_be;
#ifndef CONFIG_WORD_BASED_TLV
	hal_soc->ops->hal_rx_get_qdesc_addr = hal_rx_get_qdesc_addr_be;
#endif
	hal_soc->ops->hal_set_reo_ent_desc_reo_dest_ind =
					hal_set_reo_ent_desc_reo_dest_ind_be;
	hal_soc->ops->hal_get_reo_ent_desc_qdesc_addr =
					hal_get_reo_ent_desc_qdesc_addr_be;
}

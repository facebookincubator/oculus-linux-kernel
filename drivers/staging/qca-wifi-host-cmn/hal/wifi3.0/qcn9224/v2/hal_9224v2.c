/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "hal_9224.h"

struct hal_hw_srng_config hw_srng_table_9224v2[] = {
	/* TODO: max_rings can populated by querying HW capabilities */
	{ /* REO_DST */
		.start_ring_id = HAL_SRNG_REO2SW1,
		.max_rings = 8,
		.entry_size = sizeof(struct reo_destination_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
			HWIO_REO_R0_REO2SW1_RING_BASE_LSB_ADDR(
				REO_REG_REG_BASE),
			HWIO_REO_R2_REO2SW1_RING_HP_ADDR(
				REO_REG_REG_BASE)
		},
		.reg_size = {
			HWIO_REO_R0_REO2SW2_RING_BASE_LSB_ADDR(0) -
				HWIO_REO_R0_REO2SW1_RING_BASE_LSB_ADDR(0),
			HWIO_REO_R2_REO2SW2_RING_HP_ADDR(0) -
				HWIO_REO_R2_REO2SW1_RING_HP_ADDR(0),
		},
		.max_size =
			HWIO_REO_R0_REO2SW1_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_REO_R0_REO2SW1_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* REO_EXCEPTION */
		/* Designating REO2SW0 ring as exception ring. This ring is
		 * similar to other REO2SW rings though it is named as REO2SW0.
		 * Any of theREO2SW rings can be used as exception ring.
		 */
		.start_ring_id = HAL_SRNG_REO2SW0,
		.max_rings = 1,
		.entry_size = sizeof(struct reo_destination_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
			HWIO_REO_R0_REO2SW0_RING_BASE_LSB_ADDR(
				REO_REG_REG_BASE),
			HWIO_REO_R2_REO2SW0_RING_HP_ADDR(
				REO_REG_REG_BASE)
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size =
			HWIO_REO_R0_REO2SW0_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_REO_R0_REO2SW0_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* REO_REINJECT */
		.start_ring_id = HAL_SRNG_SW2REO,
		.max_rings = 4,
		.entry_size = sizeof(struct reo_entrance_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
			HWIO_REO_R0_SW2REO_RING_BASE_LSB_ADDR(
				REO_REG_REG_BASE),
			HWIO_REO_R2_SW2REO_RING_HP_ADDR(
				REO_REG_REG_BASE)
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {
			HWIO_REO_R0_SW2REO1_RING_BASE_LSB_ADDR(0) -
				HWIO_REO_R0_SW2REO_RING_BASE_LSB_ADDR(0),
			HWIO_REO_R2_SW2REO1_RING_HP_ADDR(0) -
				HWIO_REO_R2_SW2REO_RING_HP_ADDR(0)
		},
		.max_size = HWIO_REO_R0_SW2REO_RING_BASE_MSB_RING_SIZE_BMSK >>
				HWIO_REO_R0_SW2REO_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* REO_CMD */
		.start_ring_id = HAL_SRNG_REO_CMD,
		.max_rings = 1,
		.entry_size = (sizeof(struct tlv_32_hdr) +
			sizeof(struct reo_get_queue_stats)) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
			HWIO_REO_R0_REO_CMD_RING_BASE_LSB_ADDR(
				REO_REG_REG_BASE),
			HWIO_REO_R2_REO_CMD_RING_HP_ADDR(
				REO_REG_REG_BASE),
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size = HWIO_REO_R0_REO_CMD_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_REO_R0_REO_CMD_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* REO_STATUS */
		.start_ring_id = HAL_SRNG_REO_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct tlv_32_hdr) +
			sizeof(struct reo_get_queue_stats_status)) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
			HWIO_REO_R0_REO_STATUS_RING_BASE_LSB_ADDR(
				REO_REG_REG_BASE),
			HWIO_REO_R2_REO_STATUS_RING_HP_ADDR(
				REO_REG_REG_BASE),
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size =
		HWIO_REO_R0_REO_STATUS_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_REO_R0_REO_STATUS_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* TCL_DATA */
		.start_ring_id = HAL_SRNG_SW2TCL1,
		.max_rings = 6,
		.entry_size = sizeof(struct tcl_data_cmd) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
			HWIO_TCL_R0_SW2TCL1_RING_BASE_LSB_ADDR(
				MAC_TCL_REG_REG_BASE),
			HWIO_TCL_R2_SW2TCL1_RING_HP_ADDR(
				MAC_TCL_REG_REG_BASE),
		},
		.reg_size = {
			HWIO_TCL_R0_SW2TCL2_RING_BASE_LSB_ADDR(0) -
				HWIO_TCL_R0_SW2TCL1_RING_BASE_LSB_ADDR(0),
			HWIO_TCL_R2_SW2TCL2_RING_HP_ADDR(0) -
				HWIO_TCL_R2_SW2TCL1_RING_HP_ADDR(0),
		},
		.max_size =
			HWIO_TCL_R0_SW2TCL1_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_TCL_R0_SW2TCL1_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* TCL_CMD/CREDIT */
	  /* qca8074v2 and qcn9224 uses this ring for data commands */
		.start_ring_id = HAL_SRNG_SW2TCL_CMD,
		.max_rings = 1,
		.entry_size = sizeof(struct tcl_data_cmd) >> 2,
		.lmac_ring =  FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
			HWIO_TCL_R0_SW2TCL_CREDIT_RING_BASE_LSB_ADDR(
				MAC_TCL_REG_REG_BASE),
			HWIO_TCL_R2_SW2TCL_CREDIT_RING_HP_ADDR(
				MAC_TCL_REG_REG_BASE),
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size =
		HWIO_TCL_R0_SW2TCL_CREDIT_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_TCL_R0_SW2TCL_CREDIT_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* TCL_STATUS */
		.start_ring_id = HAL_SRNG_TCL_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct tlv_32_hdr) +
			sizeof(struct tcl_status_ring)) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
			HWIO_TCL_R0_TCL_STATUS1_RING_BASE_LSB_ADDR(
				MAC_TCL_REG_REG_BASE),
			HWIO_TCL_R2_TCL_STATUS1_RING_HP_ADDR(
				MAC_TCL_REG_REG_BASE),
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size =
			HWIO_TCL_R0_TCL_STATUS1_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_TCL_R0_TCL_STATUS1_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* CE_SRC */
		.start_ring_id = HAL_SRNG_CE_0_SRC,
		.max_rings = 16,
		.entry_size = sizeof(struct ce_src_desc) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
		HWIO_WFSS_CE_CHANNEL_SRC_R0_SRC_RING_BASE_LSB_ADDR(
				WFSS_CE_0_CHANNEL_SRC_REG_REG_BASE),
		HWIO_WFSS_CE_CHANNEL_SRC_R2_SRC_RING_HP_ADDR(
				WFSS_CE_0_CHANNEL_SRC_REG_REG_BASE),
		},
		.reg_size = {
		WFSS_CE_1_CHANNEL_SRC_REG_REG_BASE -
		WFSS_CE_0_CHANNEL_SRC_REG_REG_BASE,
		WFSS_CE_1_CHANNEL_SRC_REG_REG_BASE -
		WFSS_CE_0_CHANNEL_SRC_REG_REG_BASE,
		},
		.max_size =
		HWIO_WFSS_CE_CHANNEL_SRC_R0_SRC_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WFSS_CE_CHANNEL_SRC_R0_SRC_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* CE_DST */
		.start_ring_id = HAL_SRNG_CE_0_DST,
		.max_rings = 16,
		.entry_size = 8 >> 2,
		/*TODO: entry_size above should actually be
		 * sizeof(struct ce_dst_desc) >> 2, but couldn't find definition
		 * of struct ce_dst_desc in HW header files
		 */
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
		HWIO_WFSS_CE_CHANNEL_DST_R0_DEST_RING_BASE_LSB_ADDR(
		WFSS_CE_0_CHANNEL_DST_REG_REG_BASE),
		HWIO_WFSS_CE_CHANNEL_DST_R2_DEST_RING_HP_ADDR(
		WFSS_CE_0_CHANNEL_DST_REG_REG_BASE),
		},
		.reg_size = {
		WFSS_CE_1_CHANNEL_DST_REG_REG_BASE -
		WFSS_CE_0_CHANNEL_DST_REG_REG_BASE,
		WFSS_CE_1_CHANNEL_DST_REG_REG_BASE -
		WFSS_CE_0_CHANNEL_DST_REG_REG_BASE,
		},
		.max_size =
		HWIO_WFSS_CE_CHANNEL_DST_R0_DEST_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WFSS_CE_CHANNEL_DST_R0_DEST_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* CE_DST_STATUS */
		.start_ring_id = HAL_SRNG_CE_0_DST_STATUS,
		.max_rings = 16,
		.entry_size = sizeof(struct ce_stat_desc) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
		HWIO_WFSS_CE_CHANNEL_DST_R0_STATUS_RING_BASE_LSB_ADDR(
				WFSS_CE_0_CHANNEL_DST_REG_REG_BASE),
		HWIO_WFSS_CE_CHANNEL_DST_R2_STATUS_RING_HP_ADDR(
				WFSS_CE_0_CHANNEL_DST_REG_REG_BASE),
		},
		/* TODO: check destination status ring registers */
		.reg_size = {
		WFSS_CE_1_CHANNEL_DST_REG_REG_BASE -
		WFSS_CE_0_CHANNEL_DST_REG_REG_BASE,
		WFSS_CE_1_CHANNEL_DST_REG_REG_BASE -
		WFSS_CE_0_CHANNEL_DST_REG_REG_BASE,
		},
		.max_size =
	HWIO_WFSS_CE_CHANNEL_DST_R0_STATUS_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WFSS_CE_CHANNEL_DST_R0_STATUS_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* WBM_IDLE_LINK */
		.start_ring_id = HAL_SRNG_WBM_IDLE_LINK,
		.max_rings = 1,
		.entry_size = sizeof(struct wbm_link_descriptor_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
		HWIO_WBM_R0_WBM_IDLE_LINK_RING_BASE_LSB_ADDR(WBM_REG_REG_BASE),
		HWIO_WBM_R2_WBM_IDLE_LINK_RING_HP_ADDR(WBM_REG_REG_BASE),
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size =
		HWIO_WBM_R0_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WBM_R0_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* SW2WBM_RELEASE */
		.start_ring_id = HAL_SRNG_WBM_SW_RELEASE,
		.max_rings = 2,
		.entry_size = sizeof(struct wbm_release_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
		HWIO_WBM_R0_SW_RELEASE_RING_BASE_LSB_ADDR(WBM_REG_REG_BASE),
		HWIO_WBM_R2_SW_RELEASE_RING_HP_ADDR(WBM_REG_REG_BASE),
		},
		.reg_size = {
		HWIO_WBM_R0_SW1_RELEASE_RING_BASE_LSB_ADDR(WBM_REG_REG_BASE) -
		HWIO_WBM_R0_SW_RELEASE_RING_BASE_LSB_ADDR(WBM_REG_REG_BASE),
		HWIO_WBM_R2_SW1_RELEASE_RING_HP_ADDR(WBM_REG_REG_BASE) -
		HWIO_WBM_R2_SW_RELEASE_RING_HP_ADDR(WBM_REG_REG_BASE)
		},
		.max_size =
		HWIO_WBM_R0_SW_RELEASE_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WBM_R0_SW_RELEASE_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* WBM2SW_RELEASE */
		.start_ring_id = HAL_SRNG_WBM2SW0_RELEASE,
		.max_rings = 8,
		.entry_size = sizeof(struct wbm_release_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
		HWIO_WBM_R0_WBM2SW0_RELEASE_RING_BASE_LSB_ADDR(
				WBM_REG_REG_BASE),
		HWIO_WBM_R2_WBM2SW0_RELEASE_RING_HP_ADDR(
				WBM_REG_REG_BASE),
		},
		.reg_size = {
		HWIO_WBM_R0_WBM2SW1_RELEASE_RING_BASE_LSB_ADDR(
				WBM_REG_REG_BASE) -
		HWIO_WBM_R0_WBM2SW0_RELEASE_RING_BASE_LSB_ADDR(
				WBM_REG_REG_BASE),
		HWIO_WBM_R2_WBM2SW1_RELEASE_RING_HP_ADDR(
				WBM_REG_REG_BASE) -
		HWIO_WBM_R2_WBM2SW0_RELEASE_RING_HP_ADDR(
				WBM_REG_REG_BASE),
		},
		.max_size =
		HWIO_WBM_R0_WBM2SW0_RELEASE_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WBM_R0_WBM2SW0_RELEASE_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* RXDMA_BUF */
		.start_ring_id = HAL_SRNG_WMAC1_SW2RXDMA0_BUF0,
#ifdef IPA_OFFLOAD
#ifdef IPA_WDI3_VLAN_SUPPORT
		.max_rings = 4,
#else
		.max_rings = 3,
#endif
#else
		.max_rings = 3,
#endif
		.entry_size = sizeof(struct wbm_buffer_ring) >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE,
	},
	{ /* RXDMA_DST */
		.start_ring_id = HAL_SRNG_WMAC1_RXDMA2SW0,
		.max_rings = 0,
		.entry_size = 0/*sizeof(struct reo_entrance_ring) >> 2*/,
		.lmac_ring =  TRUE,
		.ring_dir = HAL_SRNG_DST_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE,
	},
#ifdef WLAN_PKT_CAPTURE_RX_2_0
	{ /* RXDMA_MONITOR_BUF */
		.start_ring_id = HAL_SRNG_WMAC1_SW2RXDMA2_BUF,
		.max_rings = 1,
		.entry_size = sizeof(struct mon_ingress_ring) >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
	},
#else
	{},
#endif
	{ /* RXDMA_MONITOR_STATUS */
		.start_ring_id = HAL_SRNG_WMAC1_SW2RXDMA1_STATBUF,
		.max_rings = 0,
		.entry_size = sizeof(struct wbm_buffer_ring) >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE,
	},
#ifdef WLAN_PKT_CAPTURE_RX_2_0
	{ /* RXDMA_MONITOR_DST */
		.start_ring_id = HAL_SRNG_WMAC1_RXMON2SW0,
		.max_rings = 2,
		.entry_size = sizeof(struct mon_destination_ring) >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_DST_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
	},
#else
	{},
#endif
	{ /* RXDMA_MONITOR_DESC */
		.start_ring_id = HAL_SRNG_WMAC1_SW2RXDMA1_DESC,
		.max_rings = 0,
		.entry_size = 0/*sizeof(struct sw_monitor_ring) >> 2*/,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_DST_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
	},

	{ /* DIR_BUF_RX_DMA_SRC */
		.start_ring_id = HAL_SRNG_DIR_BUF_RX_SRC_DMA_RING,
		/* one ring for spectral, one ring for cfr and
		 * another one ring for txbf cv upload
		 */
		.max_rings = 3,
		.entry_size = 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE,
	},
#ifdef WLAN_FEATURE_CIF_CFR
	{ /* WIFI_POS_SRC */
		.start_ring_id = HAL_SRNG_WIFI_POS_SRC_DMA_RING,
		.max_rings = 1,
		.entry_size = sizeof(wmi_oem_dma_buf_release_entry)  >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
	},
#endif
	{ /* REO2PPE */
		.start_ring_id = HAL_SRNG_REO2PPE,
		.max_rings = 1,
		.entry_size = sizeof(struct reo_destination_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_DST_RING,
		.reg_start = {
			HWIO_REO_R0_REO2PPE_RING_BASE_LSB_ADDR(
				REO_REG_REG_BASE),
			HWIO_REO_R2_REO2PPE_RING_HP_ADDR(
				REO_REG_REG_BASE),
		},
		/* Single ring - provide ring size if multiple rings of this
		 * type are supported
		 */
		.reg_size = {},
		.max_size =
		HWIO_REO_R0_REO2PPE_RING_BASE_LSB_RING_BASE_ADDR_LSB_BMSK >>
		HWIO_REO_R0_REO2PPE_RING_BASE_LSB_RING_BASE_ADDR_LSB_SHFT,
	},
	{ /* PPE2TCL */
		.start_ring_id = HAL_SRNG_PPE2TCL1,
		.max_rings = 1,
		.entry_size = sizeof(struct tcl_entrance_from_ppe_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
			HWIO_TCL_R0_PPE2TCL1_RING_BASE_LSB_ADDR(
				MAC_TCL_REG_REG_BASE),
			HWIO_TCL_R2_PPE2TCL1_RING_HP_ADDR(
				MAC_TCL_REG_REG_BASE),
		},
		.reg_size = {},
		.max_size =
			HWIO_TCL_R0_SW2TCL1_RING_BASE_MSB_RING_SIZE_BMSK >>
			HWIO_TCL_R0_SW2TCL1_RING_BASE_MSB_RING_SIZE_SHFT,
	},
	{ /* PPE_RELEASE */
		.start_ring_id = HAL_SRNG_WBM_PPE_RELEASE,
		.max_rings = 1,
		.entry_size = sizeof(struct wbm_release_ring) >> 2,
		.lmac_ring = FALSE,
		.ring_dir = HAL_SRNG_SRC_RING,
		.reg_start = {
		HWIO_WBM_R0_PPE_RELEASE_RING_BASE_LSB_ADDR(WBM_REG_REG_BASE),
		HWIO_WBM_R2_PPE_RELEASE_RING_HP_ADDR(WBM_REG_REG_BASE),
		},
		.reg_size = {},
		.max_size =
		HWIO_WBM_R0_PPE_RELEASE_RING_BASE_MSB_RING_SIZE_BMSK >>
		HWIO_WBM_R0_PPE_RELEASE_RING_BASE_MSB_RING_SIZE_SHFT,
	},
#ifdef WLAN_PKT_CAPTURE_TX_2_0
	{ /* TX_MONITOR_BUF */
		.start_ring_id = HAL_SRNG_SW2TXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct mon_ingress_ring) >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
	},
	{ /* TX_MONITOR_DST */
		.start_ring_id = HAL_SRNG_WMAC1_TXMON2SW0,
		.max_rings = 2,
		.entry_size = sizeof(struct mon_destination_ring) >> 2,
		.lmac_ring = TRUE,
		.ring_dir = HAL_SRNG_DST_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
	},
#else
	{},
	{},
#endif
	{ /* SW2RXDMA */
		.start_ring_id = HAL_SRNG_SW2RXDMA_BUF0,
		.max_rings = 3,
		.entry_size = sizeof(struct reo_entrance_ring) >> 2,
		.lmac_ring =  TRUE,
		.ring_dir = HAL_SRNG_SRC_RING,
		/* reg_start is not set because LMAC rings are not accessed
		 * from host
		 */
		.reg_start = {},
		.reg_size = {},
		.max_size = HAL_RXDMA_MAX_RING_SIZE_BE,
		.dmac_cmn_ring = TRUE,
	},
	{ /* SW2RXDMA_LINK_RELEASE */ 0},
};

/**
 * hal_reo_config_reo2ppe_dest_info_9224() - Configure reo2ppe dest info
 * @hal_soc_hdl: HAL SoC Context
 *
 * Return: None.
 */
static inline
void hal_reo_config_reo2ppe_dest_info_9224(hal_soc_handle_t hal_soc_hdl)
{
	HAL_REG_WRITE((struct hal_soc *)hal_soc_hdl,
		      HWIO_REO_R0_REO2PPE_DEST_INFO_ADDR(REO_REG_REG_BASE),
		      REO2PPE_RULE_FAIL_FB);
}

#define PMM_REG_BASE_QCN9224_V2 0xB500FC

/**
 * hal_get_tsf2_scratch_reg_qcn9224_v2() - API to read tsf2 scratch register
 * @hal_soc_hdl: HAL soc context
 * @mac_id: mac id
 * @value: Pointer to update tsf2 value
 *
 * Return: void
 */
static void hal_get_tsf2_scratch_reg_qcn9224_v2(hal_soc_handle_t hal_soc_hdl,
						uint8_t mac_id, uint64_t *value)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t offset_lo, offset_hi;
	enum hal_scratch_reg_enum enum_lo, enum_hi;

	hal_get_tsf_enum(DEFAULT_TSF_ID, mac_id, &enum_lo, &enum_hi);

	offset_lo = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224_V2,
					     enum_lo);

	offset_hi = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224_V2,
					     enum_hi);

	*value = ((uint64_t)(offset_hi) << 32 | offset_lo);
}

/**
 * hal_get_tqm_scratch_reg_qcn9224_v2() - API to read tqm scratch register
 * @hal_soc_hdl: HAL soc context
 * @value: Pointer to update tqm value
 *
 * Return: void
 */
static void hal_get_tqm_scratch_reg_qcn9224_v2(hal_soc_handle_t hal_soc_hdl,
					       uint64_t *value)
{
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t offset_lo, offset_hi;

	offset_lo = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224_V2,
					     PMM_TQM_CLOCK_OFFSET_LO_US);

	offset_hi = hal_read_pmm_scratch_reg(soc,
					     PMM_REG_BASE_QCN9224_V2,
					     PMM_TQM_CLOCK_OFFSET_HI_US);

	*value = ((uint64_t)(offset_hi) << 32 | offset_lo);
}

static void hal_hw_txrx_ops_override_qcn9224_v2(struct hal_soc *hal_soc)
{
	hal_soc->ops->hal_reo_config_reo2ppe_dest_info =
					hal_reo_config_reo2ppe_dest_info_9224;

	hal_soc->ops->hal_get_tsf2_scratch_reg =
					hal_get_tsf2_scratch_reg_qcn9224_v2;
	hal_soc->ops->hal_get_tqm_scratch_reg =
					hal_get_tqm_scratch_reg_qcn9224_v2;
}
/**
 * hal_qcn9224v2_attach() - Attach 9224v2 target specific hal_soc ops,
 *			    offset and srng table
 * @hal_soc: HAL SoC context
 *
 * Return: void
 */
void hal_qcn9224v2_attach(struct hal_soc *hal_soc)
{
	hal_soc->hw_srng_table = hw_srng_table_9224v2;

	hal_srng_hw_reg_offset_init_generic(hal_soc);
	hal_srng_hw_reg_offset_init_qcn9224(hal_soc);

	hal_hw_txrx_default_ops_attach_be(hal_soc);
	hal_hw_txrx_ops_attach_qcn9224(hal_soc);
	if (hal_soc->static_window_map)
		hal_write_window_register(hal_soc);
	hal_soc->dmac_cmn_src_rxbuf_ring = true;

	hal_hw_txrx_ops_override_qcn9224_v2(hal_soc);
}

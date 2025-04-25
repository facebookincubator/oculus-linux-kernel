/*
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
#ifndef __DP_RH_TX_H
#define __DP_RH_TX_H

#include <dp_types.h>

#define DP_RH_TX_HDR_SIZE_NATIVE_WIFI	30
#define DP_RH_TX_HDR_SIZE_802_11_RAW	36
#define DP_RH_TX_HDR_SIZE_ETHERNET	14
#define DP_RH_TX_HDR_SIZE_IP		16
#define DP_RH_TX_HDR_SIZE_802_1Q	4
#define DP_RH_TX_HDR_SIZE_LLC_SNAP	8

#define DP_RH_TX_HDR_SIZE_OUTER_HDR_MAX DP_RH_TX_HDR_SIZE_802_11_RAW

#define DP_RH_TX_TLV_HDR_SIZE	sizeof(struct tlv_32_hdr)
#define DP_RH_TX_TCL_DESC_SIZE	(HAL_TX_DESC_LEN_BYTES + DP_RH_TX_TLV_HDR_SIZE)

/*
 * NB: intentionally not using kernel-doc comment because the kernel-doc
 *     script does not handle the qdf_dma_mem_context macro
 * struct dp_tx_tcl_desc_pool_s - Tx Extension Descriptor Pool
 * @elem_count: Number of descriptors in the pool
 * @elem_size: Size of each descriptor
 * @desc_pages: multiple page allocation information for actual descriptors
 * @freelist: freelist of TCL descriptors
 * @memctx:
 */
struct dp_tx_tcl_desc_pool_s {
	uint16_t elem_count;
	int elem_size;
	struct qdf_mem_multi_page_t desc_pages;
	uint32_t *freelist;
	qdf_dma_mem_context(memctx);
};

/**
 * dp_tx_hw_enqueue_rh() - Enqueue to TCL HW for transmit
 * @soc: DP Soc Handle
 * @vdev: DP vdev handle
 * @tx_desc: Tx Descriptor Handle
 * @fw_metadata: Metadata to send to Target Firmware along with frame
 * @tx_exc_metadata: Handle that holds exception path meta data
 * @msdu_info: Holds the MSDU information to be transmitted
 *
 *  Gets the next free TCL HW DMA descriptor and sets up required parameters
 *  from software Tx descriptor
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_RESOURCES: Error return
 */
QDF_STATUS
dp_tx_hw_enqueue_rh(struct dp_soc *soc, struct dp_vdev *vdev,
		    struct dp_tx_desc_s *tx_desc, uint16_t fw_metadata,
		    struct cdp_tx_exception_metadata *tx_exc_metadata,
		    struct dp_tx_msdu_info_s *msdu_info);
/**
 * dp_tx_comp_get_params_from_hal_desc_rh() - Get TX desc from HAL comp desc
 * @soc: DP soc handle
 * @tx_comp_hal_desc: HAL TX Comp Descriptor
 * @r_tx_desc: SW Tx Descriptor retrieved from HAL desc.
 *
 * Return: QDF_STATUS return codes
 */
QDF_STATUS
dp_tx_comp_get_params_from_hal_desc_rh(struct dp_soc *soc,
				       void *tx_comp_hal_desc,
				       struct dp_tx_desc_s **r_tx_desc);

/**
 * dp_tx_process_htt_completion_rh() - Tx HTT Completion Indication Handler
 * @soc: Handle to DP soc structure
 * @tx_desc: software descriptor head pointer
 * @status : Tx completion status from HTT descriptor
 * @ring_id: ring number
 *
 * This function will process HTT Tx indication messages from Target
 *
 * Return: none
 */
void dp_tx_process_htt_completion_rh(struct dp_soc *soc,
				     struct dp_tx_desc_s *tx_desc,
				     uint8_t *status,
				     uint8_t ring_id);

/**
 * dp_tx_desc_pool_init_rh() - Initialize Tx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @num_elem: pool descriptor number
 * @pool_id: pool to allocate
 * @spcl_tx_desc: if special desc
 *
 * Return: QDF_STATUS_SUCCESS - success, others - failure
 */
QDF_STATUS dp_tx_desc_pool_init_rh(struct dp_soc *soc,
				   uint32_t num_elem,
				   uint8_t pool_id,
				   bool spcl_tx_desc);

/**
 * dp_tx_desc_pool_deinit_rh() - De-initialize Tx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @tx_desc_pool: Tx descriptor pool handler
 * @pool_id: pool to deinit
 * @spcl_tx_desc: if special desc
 *
 * Return: None.
 */
void dp_tx_desc_pool_deinit_rh(struct dp_soc *soc,
			       struct dp_tx_desc_pool_s *tx_desc_pool,
			       uint8_t pool_id, bool spcl_tx_desc);

/**
 * dp_tx_compute_tx_delay_rh() - Compute HW Tx completion delay
 * @soc: Handle to DP Soc structure
 * @vdev: vdev
 * @ts: Tx completion status
 * @delay_us: Delay to be calculated in microseconds
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_tx_compute_tx_delay_rh(struct dp_soc *soc,
				     struct dp_vdev *vdev,
				     struct hal_tx_completion_status *ts,
				     uint32_t *delay_us);

/**
 * dp_tx_desc_pool_alloc_rh() - Allocate coherent memory for TCL descriptors
 * @soc: Handle to DP Soc structure
 * @num_elem: Number of elements to allocate
 * @pool_id: TCL descriptor pool ID
 *
 * Return: QDF_STATUS_SUCCESS - success, others - failure
 */
QDF_STATUS dp_tx_desc_pool_alloc_rh(struct dp_soc *soc, uint32_t num_elem,
				    uint8_t pool_id);

/**
 * dp_tx_desc_pool_free_rh() - Free TCL descriptor memory
 * @soc: Handle to DP Soc structure
 * @pool_id: TCL descriptor pool ID
 *
 * Return: none
 */
void dp_tx_desc_pool_free_rh(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_compl_handler_rh() - TX completion handler for Rhine
 * @soc: Handle to DP Soc structure
 * @htt_msg: TX completion HTT message
 *
 * Return: none
 */
void dp_tx_compl_handler_rh(struct dp_soc *soc, qdf_nbuf_t htt_msg);

/**
 * dp_flush_tx_ring_rh() - flush tx ring write index
 * @pdev: dp pdev handle
 * @ring_id: Tx ring id
 *
 * Return: 0 on success and error code on failure
 */
int dp_flush_tx_ring_rh(struct dp_pdev *pdev, int ring_id);
#endif

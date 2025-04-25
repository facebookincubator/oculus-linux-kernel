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
#ifndef __DP_BE_TX_H
#define __DP_BE_TX_H
/**
 *  DOC: dp_be_tx.h
 *
 * BE specific TX Datapath header file. Need not be exposed to common DP code.
 *
 */

#include <dp_types.h>
#include "dp_be.h"

struct __attribute__((__packed__)) dp_tx_comp_peer_id {
	uint16_t peer_id:13,
		 ml_peer_valid:1,
		 reserved:2;
};

/* Invalid TX Bank ID value */
#define DP_BE_INVALID_BANK_ID -1

/* Extraction of msdu queue information from per packet sawf metadata */
#define DP_TX_HLOS_TID_GET(_var) \
	(((_var) & 0x0e) >> 1)
#define DP_TX_FLOW_OVERRIDE_GET(_var) \
	((_var >> 3) & 0x1)
#define DP_TX_WHO_CLFY_INF_SEL_GET(_var) \
	(((_var) & 0x30) >> 4)
#define DP_TX_FLOW_OVERRIDE_ENABLE 0x1

#define DP_TX_FAST_DESC_SIZE	28
#define DP_TX_L3_L4_CSUM_ENABLE	0x1f

#ifdef DP_USE_REDUCED_PEER_ID_FIELD_WIDTH
static inline uint16_t
dp_tx_comp_adjust_peer_id_be(struct dp_soc *soc, uint16_t peer_id)
{
	struct dp_tx_comp_peer_id *tx_peer_id =
		(struct dp_tx_comp_peer_id *)&peer_id;

	return (tx_peer_id->peer_id |
		(tx_peer_id->ml_peer_valid << soc->peer_id_shift));
}
/**
 * dp_tx_comp_get_peer_id_be() - Get peer ID from TX Comp Desc
 * @soc: Handle to DP Soc structure
 * @tx_comp_hal_desc: TX comp ring descriptor
 *
 * Return: Peer ID
 */
static inline uint16_t dp_tx_comp_get_peer_id_be(struct dp_soc *soc,
						 void *tx_comp_hal_desc)
{
	uint16_t peer_id = hal_tx_comp_get_peer_id(tx_comp_hal_desc);

	return dp_tx_comp_adjust_peer_id_be(soc, peer_id);
}
#else
static inline uint16_t
dp_tx_comp_adjust_peer_id_be(struct dp_soc *soc, uint16_t peer_id)
{
	return peer_id;
}
static inline uint16_t dp_tx_comp_get_peer_id_be(struct dp_soc *soc,
						 void *tx_comp_hal_desc)
{
	return hal_tx_comp_get_peer_id(tx_comp_hal_desc);

}
#endif

/**
 * dp_tx_hw_enqueue_be() - Enqueue to TCL HW for transmit for BE target
 * @soc: DP Soc Handle
 * @vdev: DP vdev handle
 * @tx_desc: Tx Descriptor Handle
 * @fw_metadata: Metadata to send to Target Firmware along with frame
 * @metadata: Handle that holds exception path meta data
 * @msdu_info: msdu_info containing information about TX buffer
 *
 *  Gets the next free TCL HW DMA descriptor and sets up required parameters
 *  from software Tx descriptor
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_RESOURCES: Error return
 */
QDF_STATUS dp_tx_hw_enqueue_be(struct dp_soc *soc, struct dp_vdev *vdev,
			       struct dp_tx_desc_s *tx_desc,
				uint16_t fw_metadata,
				struct cdp_tx_exception_metadata *metadata,
				struct dp_tx_msdu_info_s *msdu_info);

#ifdef QCA_DP_TX_NBUF_LIST_FREE
/**
 * dp_tx_fast_send_be() - Transmit a frame on a given VAP
 * @soc_hdl: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @nbuf: skb
 *
 * Entry point for Core Tx layer (DP_TX) invoked from
 * hard_start_xmit in OSIF/HDD or from dp_rx_process for intravap forwarding
 * cases
 *
 * Return: NULL on success,
 *         nbuf when it fails to send
 */
qdf_nbuf_t dp_tx_fast_send_be(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			      qdf_nbuf_t nbuf);
#else
static inline qdf_nbuf_t dp_tx_fast_send_be(struct cdp_soc_t *soc, uint8_t vdev_id,
					    qdf_nbuf_t nbuf)
{
	return NULL;
}
#endif

/**
 * dp_tx_comp_get_params_from_hal_desc_be() - Get TX desc from HAL comp desc
 * @soc: DP soc handle
 * @tx_comp_hal_desc: HAL TX Comp Descriptor
 * @r_tx_desc: SW Tx Descriptor retrieved from HAL desc.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_tx_comp_get_params_from_hal_desc_be(struct dp_soc *soc,
				       void *tx_comp_hal_desc,
				       struct dp_tx_desc_s **r_tx_desc);

/**
 * dp_tx_process_htt_completion_be() - Tx HTT Completion Indication Handler
 * @soc: Handle to DP soc structure
 * @tx_desc: software descriptor head pointer
 * @status: Tx completion status from HTT descriptor
 * @ring_id: ring number
 *
 * This function will process HTT Tx indication messages from Target
 *
 * Return: none
 */
void dp_tx_process_htt_completion_be(struct dp_soc *soc,
				     struct dp_tx_desc_s *tx_desc,
				     uint8_t *status,
				     uint8_t ring_id);

/**
 * dp_tx_init_bank_profiles() - Init TX bank profiles
 * @soc: DP soc handle
 *
 * Return: QDF_STATUS_SUCCESS or QDF error code.
 */
QDF_STATUS dp_tx_init_bank_profiles(struct dp_soc_be *soc);

/**
 * dp_tx_deinit_bank_profiles() - De-Init TX bank profiles
 * @soc: DP soc handle
 *
 * Return: None
 */
void dp_tx_deinit_bank_profiles(struct dp_soc_be *soc);

/**
 * dp_tx_get_bank_profile() - get TX bank profile for vdev
 * @soc: DP soc handle
 * @be_vdev: BE vdev pointer
 *
 * Return: bank profile allocated to vdev or DP_BE_INVALID_BANK_ID
 */
int dp_tx_get_bank_profile(struct dp_soc_be *soc,
			   struct dp_vdev_be *be_vdev);

/**
 * dp_tx_put_bank_profile() - release TX bank profile for vdev
 * @soc: DP soc handle
 * @be_vdev: pointer to be_vdev structure
 *
 * Return: None
 */
void dp_tx_put_bank_profile(struct dp_soc_be *soc, struct dp_vdev_be *be_vdev);

/**
 * dp_tx_update_bank_profile() - release existing and allocate new bank profile
 * @be_soc: DP soc handle
 * @be_vdev: pointer to be_vdev structure
 *
 * The function releases the existing bank profile allocated to the vdev and
 * looks for a new bank profile based on updated dp_vdev TX params.
 *
 * Return: None
 */
void dp_tx_update_bank_profile(struct dp_soc_be *be_soc,
			       struct dp_vdev_be *be_vdev);

/**
 * dp_tx_desc_pool_init_be() - Initialize Tx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @num_elem: number of descriptor in pool
 * @pool_id: pool ID to allocate
 * @spcl_tx_desc: if special desc
 *
 * Return: QDF_STATUS_SUCCESS - success, others - failure
 */
QDF_STATUS dp_tx_desc_pool_init_be(struct dp_soc *soc,
				   uint32_t num_elem,
				   uint8_t pool_id,
				   bool spcl_tx_desc);
/**
 * dp_tx_desc_pool_deinit_be() - De-initialize Tx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @tx_desc_pool: Tx descriptor pool handler
 * @pool_id: pool ID to deinit
 * @spcl_tx_desc: if special desc
 *
 * Return: None
 */
void dp_tx_desc_pool_deinit_be(struct dp_soc *soc,
			       struct dp_tx_desc_pool_s *tx_desc_pool,
			       uint8_t pool_id, bool spcl_tx_desc);

#ifdef WLAN_SUPPORT_PPEDS
/**
 * dp_ppeds_tx_comp_handler()- Handle tx completions for ppe2tcl ring
 * @be_soc: Handle to DP Soc structure
 * @quota: Max number of tx completions to process
 *
 * Return: Number of tx completions processed
 */
int dp_ppeds_tx_comp_handler(struct dp_soc_be *be_soc, uint32_t quota);

/*
 * dp_ppeds_stats() - Accounting fw2wbm_tx_drop drops in Tx path
 * @soc: Handle to DP Soc structure
 * @peer_id: Peer ID in the descriptor
 *
 * Return: NONE
 */

static inline
void dp_ppeds_stats(struct dp_soc *soc, uint16_t peer_id);

#endif
#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_tx_mlo_mcast_handler_be() - Tx handler for Mcast packets
 * @soc: Handle to DP Soc structure
 * @vdev: DP vdev handle
 * @nbuf: nbuf to be enqueued
 *
 * Return: None
 */
void dp_tx_mlo_mcast_handler_be(struct dp_soc *soc,
				struct dp_vdev *vdev,
				qdf_nbuf_t nbuf);

/**
 * dp_tx_mlo_is_mcast_primary_be() - Function to check for primary mcast vdev
 * @soc: Handle to DP Soc structure
 * @vdev: DP vdev handle
 *
 * Return: True if vdev is mcast primary
 *         False for all othercase
 */
bool dp_tx_mlo_is_mcast_primary_be(struct dp_soc *soc,
				   struct dp_vdev *vdev);
#ifdef WLAN_MCAST_MLO
#ifdef WLAN_MLO_MULTI_CHIP
#ifdef CONFIG_MLO_SINGLE_DEV
/**
 * dp_tx_mlo_mcast_send_be() - Tx send handler for mlo mcast enhance
 * @soc: DP soc handle
 * @vdev: DP vdev handle
 * @nbuf: skb
 * @tx_exc_metadata: Handle that holds exception path meta data
 *
 * Return: NULL for success
 *         nbuf for failure
 */

qdf_nbuf_t dp_tx_mlo_mcast_send_be(struct dp_soc *soc, struct dp_vdev *vdev,
				   qdf_nbuf_t nbuf,
				   struct cdp_tx_exception_metadata
				   *tx_exc_metadata);
#endif
/**
 * dp_tx_mlo_mcast_pkt_send() - handler to send MLO Mcast packets
 * @be_vdev: Handle to DP be_vdev structure
 * @ptnr_vdev: DP ptnr_vdev handle
 * @arg: nbuf to be enqueued
 *
 * Return: None
 */
void dp_tx_mlo_mcast_pkt_send(struct dp_vdev_be *be_vdev,
			      struct dp_vdev *ptnr_vdev,
			      void *arg);
#endif
#endif
#endif

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * dp_tx_comp_nf_handler() - Tx completion ring Near full scenario handler
 * @int_ctx: Interrupt context
 * @soc: Datapath SoC handle
 * @hal_ring_hdl: TX completion ring handle
 * @ring_id: TX completion ring number
 * @quota: Quota of the work to be done
 *
 * Return: work done
 */
uint32_t dp_tx_comp_nf_handler(struct dp_intr *int_ctx, struct dp_soc *soc,
			       hal_ring_handle_t hal_ring_hdl, uint8_t ring_id,
			       uint32_t quota);
#else
static inline
uint32_t dp_tx_comp_nf_handler(struct dp_intr *int_ctx, struct dp_soc *soc,
			       hal_ring_handle_t hal_ring_hdl, uint8_t ring_id,
			       uint32_t quota)
{
	return 0;
}
#endif /* WLAN_FEATURE_NEAR_FULL_IRQ */

/**
 * dp_tx_compute_tx_delay_be() - Compute HW Tx completion delay
 * @soc: Handle to DP Soc structure
 * @vdev: vdev
 * @ts: Tx completion status
 * @delay_us: Delay to be calculated in microseconds
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_tx_compute_tx_delay_be(struct dp_soc *soc,
				     struct dp_vdev *vdev,
				     struct hal_tx_completion_status *ts,
				     uint32_t *delay_us);

/**
 * dp_tx_desc_pool_alloc_be() - Allocate TX descriptor pool
 * @soc: Handle to DP Soc structure
 * @num_elem: Number of elements to allocate
 * @pool_id: TCL descriptor pool ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_tx_desc_pool_alloc_be(struct dp_soc *soc, uint32_t num_elem,
				    uint8_t pool_id);

/**
 * dp_tx_desc_pool_free_be() - Free TX descriptor pool
 * @soc: Handle to DP Soc structure
 * @pool_id: TCL descriptor pool ID
 *
 * Return: none
 */
void dp_tx_desc_pool_free_be(struct dp_soc *soc, uint8_t pool_id);
#endif

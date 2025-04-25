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

#ifndef _DP_LI_RX_H_
#define _DP_LI_RX_H_

#include <dp_types.h>
#include <dp_rx.h>
#include "dp_li.h"

/**
 * dp_rx_process_li() - Brain of the Rx processing functionality
 *		     Called from the bottom half (tasklet/NET_RX_SOFTIRQ)
 * @int_ctx: per interrupt context
 * @hal_ring_hdl: opaque pointer to the HAL Rx Ring, which will be serviced
 * @reo_ring_num: ring number (0, 1, 2 or 3) of the reo ring.
 * @quota: No. of units (packets) that can be serviced in one shot.
 *
 * This function implements the core of Rx functionality. This is
 * expected to handle only non-error frames.
 *
 * Return: uint32_t: No. of elements processed
 */
uint32_t dp_rx_process_li(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl, uint8_t reo_ring_num,
			  uint32_t quota);

/**
 * dp_rx_chain_msdus_li() - Function to chain all msdus of a mpdu
 *			    to pdev invalid peer list
 *
 * @soc: core DP main context
 * @nbuf: Buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @mac_id: mac id
 *
 *  Return: bool: true for last msdu of mpdu
 */
bool dp_rx_chain_msdus_li(struct dp_soc *soc, qdf_nbuf_t nbuf,
			  uint8_t *rx_tlv_hdr, uint8_t mac_id);

/**
 * dp_rx_desc_pool_init_li() - Initialize Rx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @rx_desc_pool: Rx descriptor pool handler
 * @pool_id: Rx descriptor pool ID
 *
 * Return: None
 */
QDF_STATUS dp_rx_desc_pool_init_li(struct dp_soc *soc,
				   struct rx_desc_pool *rx_desc_pool,
				   uint32_t pool_id);

/**
 * dp_rx_desc_pool_deinit_li() - De-initialize Rx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @rx_desc_pool: Rx descriptor pool handler
 * @pool_id: Rx descriptor pool ID
 *
 * Return: None
 */
void dp_rx_desc_pool_deinit_li(struct dp_soc *soc,
			       struct rx_desc_pool *rx_desc_pool,
			       uint32_t pool_id);

/**
 * dp_wbm_get_rx_desc_from_hal_desc_li() - Get corresponding Rx Desc
 *					address from WBM ring Desc
 * @soc: Handle to DP Soc structure
 * @ring_desc: ring descriptor structure pointer
 * @r_rx_desc: pointer to a pointer of Rx Desc
 *
 * Return: QDF_STATUS_SUCCESS - succeeded, others - failed
 */
QDF_STATUS dp_wbm_get_rx_desc_from_hal_desc_li(
					struct dp_soc *soc,
					void *ring_desc,
					struct dp_rx_desc **r_rx_desc);
/**
 * dp_rx_get_reo_qdesc_addr_li(): API to get qdesc address of reo
 * entrance ring desc
 *
 * @hal_soc: Handle to HAL Soc structure
 * @dst_ring_desc: reo dest ring descriptor (used for Lithium DP)
 * @buf: pointer to the start of RX PKT TLV headers
 * @txrx_peer: pointer to txrx_peer
 * @tid: tid value
 *
 * Return: qdesc address in reo destination ring buffer
 */
static inline
uint64_t dp_rx_get_reo_qdesc_addr_li(hal_soc_handle_t hal_soc,
				     uint8_t *dst_ring_desc,
				     uint8_t *buf,
				     struct dp_txrx_peer *txrx_peer,
				     unsigned int tid)
{
	return hal_rx_get_qdesc_addr(hal_soc, dst_ring_desc, buf);
}

/**
 * dp_rx_desc_cookie_2_va_li() - Convert RX Desc cookie ID to VA
 * @soc:Handle to DP Soc structure
 * @cookie: cookie used to lookup virtual address
 *
 * Return: Rx descriptor virtual address
 */
static inline
struct dp_rx_desc *dp_rx_desc_cookie_2_va_li(struct dp_soc *soc,
					     uint32_t cookie)
{
	return dp_rx_cookie_2_va_rxdma_buf(soc, cookie);
}

#define DP_PEER_METADATA_VDEV_ID_MASK	0x003f0000
#define DP_PEER_METADATA_VDEV_ID_SHIFT	16
#define DP_PEER_METADATA_OFFLOAD_MASK	0x01000000
#define DP_PEER_METADATA_OFFLOAD_SHIFT	24

#define DP_PEER_METADATA_VDEV_ID_GET_LI(_peer_metadata)		\
	(((_peer_metadata) & DP_PEER_METADATA_VDEV_ID_MASK)	\
			>> DP_PEER_METADATA_VDEV_ID_SHIFT)

#define DP_PEER_METADATA_OFFLOAD_GET_LI(_peer_metadata)		\
	(((_peer_metadata) & DP_PEER_METADATA_OFFLOAD_MASK)	\
			>> DP_PEER_METADATA_OFFLOAD_SHIFT)

static inline uint16_t
dp_rx_peer_metadata_peer_id_get_li(struct dp_soc *soc, uint32_t peer_metadata)
{
	struct htt_rx_peer_metadata_v0 *metadata =
			(struct htt_rx_peer_metadata_v0 *)&peer_metadata;

	return metadata->peer_id;
}

bool
dp_rx_intrabss_handle_nawds_li(struct dp_soc *soc, struct dp_txrx_peer *ta_peer,
			       qdf_nbuf_t nbuf_copy,
			       struct cdp_tid_rx_stats *tid_stats,
			       uint8_t link_id);

#ifdef QCA_DP_RX_NBUF_AND_NBUF_DATA_PREFETCH
static inline
void dp_rx_prefetch_nbuf_data(qdf_nbuf_t nbuf, qdf_nbuf_t next)
{
	struct rx_pkt_tlvs *pkt_tlvs;

	if (next) {
		/* prefetch skb->next and first few bytes of skb->cb */
		qdf_prefetch(next);
		/* skb->cb spread across 2 cache lines hence below prefetch */
		qdf_prefetch(&next->_skb_refdst);
		qdf_prefetch(&next->len);
		qdf_prefetch(&next->protocol);
		pkt_tlvs = (struct rx_pkt_tlvs *)next->data;
		/* sa_idx, da_idx, l3_pad in RX msdu_end TLV */
		qdf_prefetch(pkt_tlvs);
		/* msdu_done in RX attention TLV */
		qdf_prefetch(&pkt_tlvs->attn_tlv);
		/* fr_ds & to_ds in RX MPDU start TLV */
		if (qdf_nbuf_is_rx_chfrag_end(nbuf))
			qdf_prefetch(&pkt_tlvs->mpdu_start_tlv);
	}
}
#else
static inline
void dp_rx_prefetch_nbuf_data(qdf_nbuf_t nbuf, qdf_nbuf_t next)
{
}
#endif

#ifdef QCA_DP_RX_HW_SW_NBUF_DESC_PREFETCH
/**
 * dp_rx_cookie_2_va_rxdma_buf_prefetch() - function to prefetch the SW desc
 * @soc: Handle to DP Soc structure
 * @cookie: cookie used to lookup virtual address
 *
 * Return: prefetched Rx descriptor virtual address
 */
static inline
void *dp_rx_cookie_2_va_rxdma_buf_prefetch(struct dp_soc *soc, uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_COOKIE_POOL_ID_GET(cookie);
	uint16_t index = DP_RX_DESC_COOKIE_INDEX_GET(cookie);
	struct rx_desc_pool *rx_desc_pool;
	void *prefetch_desc;

	if (qdf_unlikely(pool_id >= MAX_RXDESC_POOLS))
		return NULL;

	rx_desc_pool = &soc->rx_desc_buf[pool_id];

	if (qdf_unlikely(index >= rx_desc_pool->pool_size))
		return NULL;

	prefetch_desc = &soc->rx_desc_buf[pool_id].array[index].rx_desc;
	qdf_prefetch(prefetch_desc);
	return prefetch_desc;
}

/**
 * dp_rx_prefetch_hw_sw_nbuf_desc() - function to prefetch HW and SW desc
 * @soc: Handle to DP Soc structure
 * @hal_soc: Handle to HAL Soc structure
 * @num_entries: valid number of HW descriptors
 * @hal_ring_hdl: Destination ring pointer
 * @last_prefetched_hw_desc: pointer to the last prefetched HW descriptor
 * @last_prefetched_sw_desc: input & output param of last prefetch SW desc
 *
 * Return: None
 */
static inline
void dp_rx_prefetch_hw_sw_nbuf_desc(struct dp_soc *soc,
				    hal_soc_handle_t hal_soc,
				    uint32_t num_entries,
				    hal_ring_handle_t hal_ring_hdl,
				    hal_ring_desc_t *last_prefetched_hw_desc,
				    struct dp_rx_desc **last_prefetched_sw_desc)
{
	if (*last_prefetched_sw_desc) {
		qdf_prefetch((uint8_t *)(*last_prefetched_sw_desc)->nbuf);
		qdf_prefetch((uint8_t *)(*last_prefetched_sw_desc)->nbuf + 64);
	}

	if (num_entries) {
		*last_prefetched_sw_desc = dp_rx_cookie_2_va_rxdma_buf_prefetch(soc, HAL_RX_REO_BUF_COOKIE_GET(*last_prefetched_hw_desc));
		*last_prefetched_hw_desc = hal_srng_dst_prefetch_next_cached_desc(hal_soc,
										  hal_ring_hdl,
										  (uint8_t *)*last_prefetched_hw_desc);
	}
}
#else
static inline
void dp_rx_prefetch_hw_sw_nbuf_desc(struct dp_soc *soc,
				    hal_soc_handle_t hal_soc,
				    uint32_t quota,
				    hal_ring_handle_t hal_ring_hdl,
				    hal_ring_desc_t *last_prefetched_hw_desc,
				    struct dp_rx_desc **last_prefetched_sw_desc)
{
}
#endif

static inline
QDF_STATUS dp_peer_rx_reorder_queue_setup_li(struct dp_soc *soc,
					     struct dp_peer *peer,
					     uint32_t tid_bitmap,
					     uint32_t ba_window_size)
{
	int tid;
	struct dp_rx_tid *rx_tid;

	if (!soc->cdp_soc.ol_ops->peer_rx_reorder_queue_setup) {
		dp_peer_debug("peer_rx_reorder_queue_setup NULL");
		return QDF_STATUS_SUCCESS;
	}

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		if (!(BIT(tid) & tid_bitmap))
			continue;

		rx_tid = &peer->rx_tid[tid];
		if (!rx_tid->hw_qdesc_paddr) {
			tid_bitmap &= ~BIT(tid);
			continue;
		}

		if (soc->cdp_soc.ol_ops->peer_rx_reorder_queue_setup(
		    soc->ctrl_psoc,
		    peer->vdev->pdev->pdev_id,
		    peer->vdev->vdev_id,
		    peer->mac_addr.raw, rx_tid->hw_qdesc_paddr, tid, tid,
		    1, ba_window_size)) {
			dp_peer_err("%pK: Fail to send reo q setup. tid %d",
				    soc, tid);
			return QDF_STATUS_E_FAILURE;
		}

		if (!tid_bitmap) {
			dp_peer_err("tid_bitmap=0. All tids setup fail");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_wbm_err_reap_desc_li() - Function to reap and replenish
 *                                WBM RX Error descriptors
 *
 * @int_ctx: pointer to DP interrupt context
 * @soc: core DP main context
 * @hal_ring_hdl: opaque pointer to the HAL Rx Error Ring, to be serviced
 * @quota: No. of units (packets) that can be serviced in one shot.
 * @rx_bufs_used: No. of descriptors reaped
 *
 * This function implements the core Rx functionality like reap and
 * replenish the RX error ring Descriptors, and create a nbuf list
 * out of it. It also reads wbm error information from descriptors
 * and update the nbuf tlv area.
 *
 * Return: qdf_nbuf_t: head pointer to the nbuf list created
 */
qdf_nbuf_t
dp_rx_wbm_err_reap_desc_li(struct dp_intr *int_ctx, struct dp_soc *soc,
			   hal_ring_handle_t hal_ring_hdl, uint32_t quota,
			   uint32_t *rx_bufs_used);

/**
 * dp_rx_null_q_desc_handle_li() - Function to handle NULL Queue
 *                                 descriptor violation on either a
 *                                 REO or WBM ring
 *
 * @soc: core DP main context
 * @nbuf: buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @pool_id: mac id
 * @txrx_peer: txrx peer handle
 * @is_reo_exception: flag to check if the error is from REO or WBM
 * @link_id: link Id on which packet is received
 *
 * This function handles NULL queue descriptor violations arising out
 * a missing REO queue for a given peer or a given TID. This typically
 * may happen if a packet is received on a QOS enabled TID before the
 * ADDBA negotiation for that TID, when the TID queue is setup. Or
 * it may also happen for MC/BC frames if they are not routed to the
 * non-QOS TID queue, in the absence of any other default TID queue.
 * This error can show up both in a REO destination or WBM release ring.
 *
 * Return: QDF_STATUS_SUCCESS, if nbuf handled successfully. QDF status code
 *         if nbuf could not be handled or dropped.
 */
QDF_STATUS
dp_rx_null_q_desc_handle_li(struct dp_soc *soc, qdf_nbuf_t nbuf,
			    uint8_t *rx_tlv_hdr, uint8_t pool_id,
			    struct dp_txrx_peer *txrx_peer,
			    bool is_reo_exception,
			    uint8_t link_id);
#endif

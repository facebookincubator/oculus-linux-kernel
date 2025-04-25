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

#ifndef _DP_BE_RX_H_
#define _DP_BE_RX_H_

#include <dp_types.h>
#include "dp_be.h"
#include "dp_peer.h"
#include <dp_rx.h>
#include "hal_be_rx.h"
#include "hal_be_rx_tlv.h"

/*
 * dp_be_intrabss_params
 *
 * @dest_soc: dest soc to forward the packet to
 * @tx_vdev_id: vdev id retrieved from dest peer
 */
struct dp_be_intrabss_params {
	struct dp_soc *dest_soc;
	uint8_t tx_vdev_id;
};

#ifndef QCA_HOST_MODE_WIFI_DISABLED

/**
 * dp_rx_intrabss_fwd_be() - API for intrabss fwd. For EAPOL
 *  pkt with DA not equal to vdev mac addr, fwd is not allowed.
 * @soc: core txrx main context
 * @ta_txrx_peer: source peer entry
 * @rx_tlv_hdr: start address of rx tlvs
 * @nbuf: nbuf that has to be intrabss forwarded
 * @link_id: link id on which the packet is received
 *
 * Return: true if it is forwarded else false
 */
bool dp_rx_intrabss_fwd_be(struct dp_soc *soc,
			   struct dp_txrx_peer *ta_txrx_peer,
			   uint8_t *rx_tlv_hdr,
			   qdf_nbuf_t nbuf,
			   uint8_t link_id);
#endif

/**
 * dp_rx_intrabss_mcast_handler_be() - intrabss mcast handler
 * @soc: core txrx main context
 * @ta_txrx_peer: source txrx_peer entry
 * @nbuf_copy: nbuf that has to be intrabss forwarded
 * @tid_stats: tid_stats structure
 * @link_id: link id on which the packet is received
 *
 * Return: true if it is forwarded else false
 */
bool
dp_rx_intrabss_mcast_handler_be(struct dp_soc *soc,
				struct dp_txrx_peer *ta_txrx_peer,
				qdf_nbuf_t nbuf_copy,
				struct cdp_tid_rx_stats *tid_stats,
				uint8_t link_id);

void dp_rx_word_mask_subscribe_be(struct dp_soc *soc,
				  uint32_t *msg_word,
				  void *rx_filter);

/**
 * dp_rx_process_be() - Brain of the Rx processing functionality
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
uint32_t dp_rx_process_be(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl, uint8_t reo_ring_num,
			  uint32_t quota);

/**
 * dp_rx_desc_pool_init_be() - Initialize Rx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @rx_desc_pool: Rx descriptor pool handler
 * @pool_id: Rx descriptor pool ID
 *
 * Return: QDF_STATUS_SUCCESS - succeeded, others - failed
 */
QDF_STATUS dp_rx_desc_pool_init_be(struct dp_soc *soc,
				   struct rx_desc_pool *rx_desc_pool,
				   uint32_t pool_id);

/**
 * dp_rx_desc_pool_deinit_be() - De-initialize Rx Descriptor pool(s)
 * @soc: Handle to DP Soc structure
 * @rx_desc_pool: Rx descriptor pool handler
 * @pool_id: Rx descriptor pool ID
 *
 * Return: None
 */
void dp_rx_desc_pool_deinit_be(struct dp_soc *soc,
			       struct rx_desc_pool *rx_desc_pool,
			       uint32_t pool_id);

/**
 * dp_wbm_get_rx_desc_from_hal_desc_be() - Get corresponding Rx Desc
 *					address from WBM ring Desc
 * @soc: Handle to DP Soc structure
 * @ring_desc: ring descriptor structure pointer
 * @r_rx_desc: pointer to a pointer of Rx Desc
 *
 * Return: QDF_STATUS_SUCCESS - succeeded, others - failed
 */
QDF_STATUS dp_wbm_get_rx_desc_from_hal_desc_be(struct dp_soc *soc,
					       void *ring_desc,
					       struct dp_rx_desc **r_rx_desc);

/**
 * dp_rx_desc_cookie_2_va_be() - Convert RX Desc cookie ID to VA
 * @soc:Handle to DP Soc structure
 * @cookie: cookie used to lookup virtual address
 *
 * Return: Rx descriptor virtual address
 */
struct dp_rx_desc *dp_rx_desc_cookie_2_va_be(struct dp_soc *soc,
					     uint32_t cookie);

#if !defined(DP_FEATURE_HW_COOKIE_CONVERSION) || \
		defined(DP_HW_COOKIE_CONVERT_EXCEPTION)
/**
 * dp_rx_desc_sw_cc_check() - check if RX desc VA is got correctly,
 *			      if not, do SW cookie conversion.
 * @soc:Handle to DP Soc structure
 * @rx_buf_cookie: RX desc cookie ID
 * @r_rx_desc: double pointer for RX desc
 *
 * Return: None
 */
static inline void
dp_rx_desc_sw_cc_check(struct dp_soc *soc,
		       uint32_t rx_buf_cookie,
		       struct dp_rx_desc **r_rx_desc)
{
	if (qdf_unlikely(!(*r_rx_desc))) {
		*r_rx_desc = (struct dp_rx_desc *)
				dp_cc_desc_find(soc,
						rx_buf_cookie);
	}
}
#else
static inline void
dp_rx_desc_sw_cc_check(struct dp_soc *soc,
		       uint32_t rx_buf_cookie,
		       struct dp_rx_desc **r_rx_desc)
{
}
#endif /* DP_FEATURE_HW_COOKIE_CONVERSION && DP_HW_COOKIE_CONVERT_EXCEPTION */

struct dp_rx_desc *dp_rx_desc_ppeds_cookie_2_va(struct dp_soc *soc,
						unsigned long cookie);

#define DP_PEER_METADATA_OFFLOAD_GET_BE(_peer_metadata)		(0)

#define HTT_RX_PEER_META_DATA_FIELD_GET(_var, _field_s, _field_m) \
	(((_var) & (_field_m)) >> (_field_s))

#ifdef DP_USE_REDUCED_PEER_ID_FIELD_WIDTH
static inline uint16_t
dp_rx_peer_metadata_peer_id_get_be(struct dp_soc *soc, uint32_t peer_metadata)
{
	uint8_t ml_peer_valid;
	uint16_t peer_id;

	peer_id = HTT_RX_PEER_META_DATA_FIELD_GET(peer_metadata,
						  soc->htt_peer_id_s,
						  soc->htt_peer_id_m);
	ml_peer_valid = HTT_RX_PEER_META_DATA_FIELD_GET(
						peer_metadata,
						soc->htt_mld_peer_valid_s,
						soc->htt_mld_peer_valid_m);

	return (peer_id | (ml_peer_valid << soc->peer_id_shift));
}
#else
/* Combine ml_peer_valid and peer_id field */
#define DP_BE_PEER_METADATA_PEER_ID_MASK	0x00003fff
#define DP_BE_PEER_METADATA_PEER_ID_SHIFT	0

static inline uint16_t
dp_rx_peer_metadata_peer_id_get_be(struct dp_soc *soc, uint32_t peer_metadata)
{
	return ((peer_metadata & DP_BE_PEER_METADATA_PEER_ID_MASK) >>
		DP_BE_PEER_METADATA_PEER_ID_SHIFT);
}
#endif

static inline uint16_t
dp_rx_peer_metadata_vdev_id_get_be(struct dp_soc *soc, uint32_t peer_metadata)
{

	return HTT_RX_PEER_META_DATA_FIELD_GET(peer_metadata,
					       soc->htt_vdev_id_s,
					       soc->htt_vdev_id_m);
}

static inline uint8_t
dp_rx_peer_metadata_lmac_id_get_be(uint32_t peer_metadata)
{
	return HTT_RX_PEER_META_DATA_V1_LMAC_ID_GET(peer_metadata);
}


#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * dp_rx_nf_process() - Near Full state handler for RX rings.
 * @int_ctx: interrupt context
 * @hal_ring_hdl: Rx ring handle
 * @reo_ring_num: RX ring number
 * @quota: Quota of work to be done
 *
 * Return: work done in the handler
 */
uint32_t dp_rx_nf_process(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl,
			  uint8_t reo_ring_num,
			  uint32_t quota);
#else
static inline
uint32_t dp_rx_nf_process(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl,
			  uint8_t reo_ring_num,
			  uint32_t quota)
{
	return 0;
}
#endif /*WLAN_FEATURE_NEAR_FULL_IRQ */

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
struct dp_soc *
dp_rx_replenish_soc_get(struct dp_soc *soc, uint8_t chip_id);

struct dp_soc *
dp_soc_get_by_idle_bm_id(struct dp_soc *soc, uint8_t idle_bm_id);

uint8_t dp_soc_get_num_soc_be(struct dp_soc *soc);
#else
static inline struct dp_soc *
dp_rx_replenish_soc_get(struct dp_soc *soc, uint8_t chip_id)
{
	return soc;
}

static inline uint8_t
dp_soc_get_num_soc_be(struct dp_soc *soc)
{
	return 1;
}
#endif

static inline QDF_STATUS
dp_peer_rx_reorder_q_setup_per_tid(struct dp_peer *peer,
				   uint32_t tid_bitmap,
				   uint32_t ba_window_size)
{
	int tid;
	struct dp_rx_tid *rx_tid;
	struct dp_soc *soc = peer->vdev->pdev->soc;

	if (!soc->cdp_soc.ol_ops->peer_rx_reorder_queue_setup) {
		dp_peer_debug("%pK: rx_reorder_queue_setup NULL bitmap 0x%x",
			      soc, tid_bitmap);
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
		    peer->mac_addr.raw,
		    rx_tid->hw_qdesc_paddr,
		    tid, tid,
		    1, ba_window_size)) {
			dp_peer_err("%pK: Fail to send reo q to FW. tid %d",
				    soc, tid);
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (!tid_bitmap) {
		dp_peer_err("tid_bitmap=0. All tids setup fail");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_peer_multi_tid_params_setup(struct dp_peer *peer,
		uint32_t tid_bitmap,
		uint32_t ba_window_size,
		struct multi_rx_reorder_queue_setup_params *tid_params)
{
	struct dp_rx_tid *rx_tid;
	int tid;

	tid_params->peer_macaddr = peer->mac_addr.raw;
	tid_params->tid_bitmap = tid_bitmap;
	tid_params->vdev_id = peer->vdev->vdev_id;

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		if (!(BIT(tid) & tid_bitmap))
			continue;

		rx_tid = &peer->rx_tid[tid];
		if (!rx_tid->hw_qdesc_paddr) {
			tid_params->tid_bitmap &= ~BIT(tid);
			continue;
		}

		tid_params->tid_num++;
		tid_params->queue_params_list[tid].hw_qdesc_paddr =
			rx_tid->hw_qdesc_paddr;
		tid_params->queue_params_list[tid].queue_no = tid;
		tid_params->queue_params_list[tid].ba_window_size_valid = 1;
		tid_params->queue_params_list[tid].ba_window_size =
			ba_window_size;
	}

	if (!tid_params->tid_bitmap) {
		dp_peer_err("tid_bitmap=0. All tids setup fail");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_peer_rx_reorder_multi_q_setup(struct dp_peer *peer,
				 uint32_t tid_bitmap,
				 uint32_t ba_window_size)
{
	QDF_STATUS status;
	struct dp_soc *soc = peer->vdev->pdev->soc;
	struct multi_rx_reorder_queue_setup_params tid_params = {0};

	if (!soc->cdp_soc.ol_ops->peer_multi_rx_reorder_queue_setup) {
		dp_peer_debug("%pK: callback NULL", soc);
		return QDF_STATUS_SUCCESS;
	}

	status = dp_peer_multi_tid_params_setup(peer, tid_bitmap,
						ba_window_size,
						&tid_params);
	if (qdf_unlikely(QDF_IS_STATUS_ERROR(status)))
		return status;

	if (soc->cdp_soc.ol_ops->peer_multi_rx_reorder_queue_setup(
	    soc->ctrl_psoc,
	    peer->vdev->pdev->pdev_id,
	    &tid_params)) {
		dp_peer_err("%pK: multi_reorder_q_setup fail. tid_bitmap 0x%x",
			    soc, tid_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_rx_mlo_igmp_handler() - Rx handler for Mcast packets
 * @soc: Handle to DP Soc structure
 * @vdev: DP vdev handle
 * @peer: DP peer handle
 * @nbuf: nbuf to be enqueued
 * @link_id: link id on which the packet is received
 *
 * Return: true when packet sent to stack, false failure
 */
bool dp_rx_mlo_igmp_handler(struct dp_soc *soc,
			    struct dp_vdev *vdev,
			    struct dp_txrx_peer *peer,
			    qdf_nbuf_t nbuf,
			    uint8_t link_id);

/**
 * dp_peer_rx_reorder_queue_setup_be() - Send reo queue
 *        setup wmi cmd to FW per peer type
 * @soc: DP Soc handle
 * @peer: dp peer to operate on
 * @tid_bitmap: TIDs to be set up
 * @ba_window_size: BlockAck window size
 *
 * Return: 0 - success, others - failure
 */
static inline
QDF_STATUS dp_peer_rx_reorder_queue_setup_be(struct dp_soc *soc,
					     struct dp_peer *peer,
					     uint32_t tid_bitmap,
					     uint32_t ba_window_size)
{
	uint8_t i;
	struct dp_mld_link_peers link_peers_info;
	struct dp_peer *link_peer;
	struct dp_rx_tid *rx_tid;
	int tid;
	QDF_STATUS status;

	if (hal_reo_shared_qaddr_is_enable(soc->hal_soc)) {
		/* Some BE targets dont require WMI and use shared
		 * table managed by host for storing Reo queue ref structs
		 */
		if (IS_MLO_DP_LINK_PEER(peer) ||
		    peer->peer_id == HTT_INVALID_PEER) {
			/* Return if this is for MLD link peer and table
			 * is not used in MLD link peer case as MLD peer's
			 * qref is written to LUT in peer setup or peer map.
			 * At this point peer setup for link peer is called
			 * before peer map, hence peer id is not assigned.
			 * This could happen if peer_setup is called before
			 * host receives HTT peer map. In this case return
			 * success with no op and let peer map handle
			 * writing the reo_qref to LUT.
			 */
			dp_peer_debug("Invalid peer id for dp_peer:%pK", peer);
			return QDF_STATUS_SUCCESS;
		}

		for (tid = 0; tid < DP_MAX_TIDS; tid++) {
			if (!((1 << tid) & tid_bitmap))
				continue;

			rx_tid = &peer->rx_tid[tid];
			if (!rx_tid->hw_qdesc_paddr) {
				tid_bitmap &= ~BIT(tid);
				continue;
			}

			hal_reo_shared_qaddr_write(soc->hal_soc,
						   peer->peer_id,
						   tid, peer->rx_tid[tid].
							hw_qdesc_paddr);

			if (!tid_bitmap) {
				dp_peer_err("tid_bitmap=0. All tids setup fail");
				return QDF_STATUS_E_FAILURE;
			}
		}

		return QDF_STATUS_SUCCESS;
	}

	/* when (!hal_reo_shared_qaddr_is_enable(soc->hal_soc)) is true: */
	if (IS_MLO_DP_MLD_PEER(peer)) {
		/* get link peers with reference */
		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_CDP);
		/* send WMI cmd to each link peers */
		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			if (soc->features.multi_rx_reorder_q_setup_support)
				status = dp_peer_rx_reorder_multi_q_setup(
					link_peer, tid_bitmap, ba_window_size);
			else
				status = dp_peer_rx_reorder_q_setup_per_tid(
							link_peer,
							tid_bitmap,
							ba_window_size);
			if (QDF_IS_STATUS_ERROR(status)) {
				dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
				return status;
			}
		}
		/* release link peers reference */
		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
	} else if (peer->peer_type == CDP_LINK_PEER_TYPE) {
		if (soc->features.multi_rx_reorder_q_setup_support)
			return dp_peer_rx_reorder_multi_q_setup(peer,
								tid_bitmap,
								ba_window_size);
		else
			return dp_peer_rx_reorder_q_setup_per_tid(peer,
							tid_bitmap,
							ba_window_size);
	} else {
		dp_peer_err("invalid peer type %d", peer->peer_type);

		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS dp_peer_rx_reorder_queue_setup_be(struct dp_soc *soc,
					     struct dp_peer *peer,
					     uint32_t tid_bitmap,
					     uint32_t ba_window_size)
{
	if (soc->features.multi_rx_reorder_q_setup_support)
		return dp_peer_rx_reorder_multi_q_setup(peer,
							tid_bitmap,
							ba_window_size);
	else
		return dp_peer_rx_reorder_q_setup_per_tid(peer,
							  tid_bitmap,
							  ba_window_size);
}
#endif /* WLAN_FEATURE_11BE_MLO */

#ifdef QCA_DP_RX_NBUF_AND_NBUF_DATA_PREFETCH
static inline
void dp_rx_prefetch_nbuf_data_be(qdf_nbuf_t nbuf, qdf_nbuf_t next)
{
	if (next) {
		/* prefetch skb->next and first few bytes of skb->cb */
		qdf_prefetch(next);
		/* skb->cb spread across 2 cache lines hence below prefetch */
		qdf_prefetch(&next->_skb_refdst);
		qdf_prefetch(&next->protocol);
		qdf_prefetch(&next->data);
		qdf_prefetch(next->data);
		qdf_prefetch(next->data + 64);
	}
}
#else
static inline
void dp_rx_prefetch_nbuf_data_be(qdf_nbuf_t nbuf, qdf_nbuf_t next)
{
}
#endif

#ifdef QCA_DP_RX_HW_SW_NBUF_DESC_PREFETCH
/**
 * dp_rx_va_prefetch() - function to prefetch the SW desc
 * @last_prefetched_hw_desc: HW desc
 *
 * Return: prefetched Rx descriptor virtual address
 */
static inline
void *dp_rx_va_prefetch(void *last_prefetched_hw_desc)
{
	void *prefetch_desc;

	prefetch_desc = (void *)hal_rx_get_reo_desc_va(last_prefetched_hw_desc);
	qdf_prefetch(prefetch_desc);
	return prefetch_desc;
}

/**
 * dp_rx_prefetch_hw_sw_nbuf_32_byte_desc() - function to prefetch HW and SW
 *                                            descriptors
 * @soc: DP soc context
 * @hal_soc: Handle to HAL Soc structure
 * @num_entries: valid number of HW descriptors
 * @hal_ring_hdl: Destination ring pointer
 * @last_prefetched_hw_desc: pointer to the last prefetched HW descriptor
 * @last_prefetched_sw_desc: input & output param of last prefetch SW desc
 *
 * Return: None
 */
static inline void
dp_rx_prefetch_hw_sw_nbuf_32_byte_desc(struct dp_soc *soc,
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
		*last_prefetched_sw_desc =
			dp_rx_va_prefetch(*last_prefetched_hw_desc);

		if ((uintptr_t)*last_prefetched_hw_desc & 0x3f)
			*last_prefetched_hw_desc =
				hal_srng_dst_prefetch_next_cached_desc(hal_soc,
					  hal_ring_hdl,
					  (uint8_t *)*last_prefetched_hw_desc);
		else
			*last_prefetched_hw_desc =
				hal_srng_dst_get_next_32_byte_desc(hal_soc,
				   hal_ring_hdl,
				   (uint8_t *)*last_prefetched_hw_desc);
	}
}
#else
static inline void
dp_rx_prefetch_hw_sw_nbuf_32_byte_desc(struct dp_soc *soc,
			       hal_soc_handle_t hal_soc,
			       uint32_t num_entries,
			       hal_ring_handle_t hal_ring_hdl,
			       hal_ring_desc_t *last_prefetched_hw_desc,
			       struct dp_rx_desc **last_prefetched_sw_desc)
{
}
#endif
#ifdef CONFIG_WORD_BASED_TLV
/**
 * dp_rx_get_reo_qdesc_addr_be(): API to get qdesc address of reo
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
uint64_t dp_rx_get_reo_qdesc_addr_be(hal_soc_handle_t hal_soc,
				     uint8_t *dst_ring_desc,
				     uint8_t *buf,
				     struct dp_txrx_peer *txrx_peer,
				     unsigned int tid)
{
	struct dp_peer *peer = NULL;
	uint64_t qdesc_addr = 0;

	if (hal_reo_shared_qaddr_is_enable(hal_soc)) {
		qdesc_addr = (uint64_t)txrx_peer->peer_id;
	} else {
		peer = dp_peer_get_ref_by_id(txrx_peer->vdev->pdev->soc,
					     txrx_peer->peer_id,
					     DP_MOD_ID_CONFIG);
		if (!peer)
			return 0;

		qdesc_addr = (uint64_t)peer->rx_tid[tid].hw_qdesc_paddr;
		dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
	}
	return qdesc_addr;
}
#else
static inline
uint64_t dp_rx_get_reo_qdesc_addr_be(hal_soc_handle_t hal_soc,
				     uint8_t *dst_ring_desc,
				     uint8_t *buf,
				     struct dp_txrx_peer *txrx_peer,
				     unsigned int tid)
{
	return hal_rx_get_qdesc_addr(hal_soc, dst_ring_desc, buf);
}
#endif

/**
 * dp_rx_wbm_err_reap_desc_be() - Function to reap and replenish
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
dp_rx_wbm_err_reap_desc_be(struct dp_intr *int_ctx, struct dp_soc *soc,
			   hal_ring_handle_t hal_ring_hdl, uint32_t quota,
			   uint32_t *rx_bufs_used);

/**
 * dp_rx_null_q_desc_handle_be() - Function to handle NULL Queue
 *                                 descriptor violation on either a
 *                                 REO or WBM ring
 *
 * @soc: core DP main context
 * @nbuf: buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @pool_id: mac id
 * @txrx_peer: txrx peer handle
 * @is_reo_exception: flag to check if the error is from REO or WBM
 * @link_id: link Id on which the packet is received
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
dp_rx_null_q_desc_handle_be(struct dp_soc *soc, qdf_nbuf_t nbuf,
			    uint8_t *rx_tlv_hdr, uint8_t pool_id,
			    struct dp_txrx_peer *txrx_peer,
			    bool is_reo_exception, uint8_t link_id);

#if defined(DP_PKT_STATS_PER_LMAC) && defined(WLAN_FEATURE_11BE_MLO)
static inline void
dp_rx_set_msdu_lmac_id(qdf_nbuf_t nbuf, uint32_t peer_mdata)
{
	uint8_t lmac_id;

	lmac_id = dp_rx_peer_metadata_lmac_id_get_be(peer_mdata);
	qdf_nbuf_set_lmac_id(nbuf, lmac_id);
}
#else
static inline void
dp_rx_set_msdu_lmac_id(qdf_nbuf_t nbuf, uint32_t peer_mdata)
{
}
#endif

#ifndef CONFIG_NBUF_AP_PLATFORM
#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
static inline uint8_t
dp_rx_peer_mdata_link_id_get_be(uint32_t peer_mdata)
{
	uint8_t link_id;

	link_id = HTT_RX_PEER_META_DATA_V1A_LOGICAL_LINK_ID_GET(peer_mdata) + 1;
	if (link_id > DP_MAX_MLO_LINKS)
		link_id = 0;

	return link_id;
}
#else
static inline uint8_t
dp_rx_peer_mdata_link_id_get_be(uint32_t peer_metadata)
{
	return 0;
}
#endif /* DP_MLO_LINK_STATS_SUPPORT */

static inline void
dp_rx_set_mpdu_seq_number_be(qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
	QDF_NBUF_CB_RX_MPDU_SEQ_NUM(nbuf) =
		hal_rx_mpdu_sequence_number_get_be(rx_tlv_hdr);
}

static inline void
dp_rx_set_link_id_be(qdf_nbuf_t nbuf, uint32_t peer_mdata)
{
	uint8_t logical_link_id;

	logical_link_id = dp_rx_peer_mdata_link_id_get_be(peer_mdata);
	QDF_NBUF_CB_RX_LOGICAL_LINK_ID(nbuf) = logical_link_id;
}

static inline uint16_t
dp_rx_get_peer_id_be(qdf_nbuf_t nbuf)
{
	return QDF_NBUF_CB_RX_PEER_ID(nbuf);
}

static inline void
dp_rx_set_mpdu_msdu_desc_info_in_nbuf(qdf_nbuf_t nbuf,
				      uint32_t mpdu_desc_info,
				      uint32_t peer_mdata,
				      uint32_t msdu_desc_info)
{
}

static inline uint8_t dp_rx_copy_desc_info_in_nbuf_cb(struct dp_soc *soc,
						      hal_ring_desc_t ring_desc,
						      qdf_nbuf_t nbuf,
						      uint8_t reo_ring_num)
{
	struct hal_rx_mpdu_desc_info mpdu_desc_info;
	struct hal_rx_msdu_desc_info msdu_desc_info;
	uint8_t pkt_capture_offload = 0;
	uint32_t peer_mdata = 0;

	qdf_mem_zero(&mpdu_desc_info, sizeof(mpdu_desc_info));
	qdf_mem_zero(&msdu_desc_info, sizeof(msdu_desc_info));

	/* Get MPDU DESC info */
	hal_rx_mpdu_desc_info_get_be(ring_desc, &mpdu_desc_info);

	/* Get MSDU DESC info */
	hal_rx_msdu_desc_info_get_be(ring_desc, &msdu_desc_info);

	/* Set the end bit to identify the last buffer in MPDU */
	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_LAST_MSDU_IN_MPDU)
		qdf_nbuf_set_rx_chfrag_end(nbuf, 1);

	if (mpdu_desc_info.mpdu_flags & HAL_MPDU_F_RETRY_BIT)
		qdf_nbuf_set_rx_retry_flag(nbuf, 1);

	if (qdf_unlikely(mpdu_desc_info.mpdu_flags & HAL_MPDU_F_RAW_AMPDU))
		qdf_nbuf_set_raw_frame(nbuf, 1);

	peer_mdata = mpdu_desc_info.peer_meta_data;
	QDF_NBUF_CB_RX_PEER_ID(nbuf) =
		dp_rx_peer_metadata_peer_id_get_be(soc, peer_mdata);
	QDF_NBUF_CB_RX_VDEV_ID(nbuf) =
		dp_rx_peer_metadata_vdev_id_get_be(soc, peer_mdata);
	dp_rx_set_msdu_lmac_id(nbuf, peer_mdata);
	dp_rx_set_link_id_be(nbuf, peer_mdata);

	/* to indicate whether this msdu is rx offload */
	pkt_capture_offload =
		DP_PEER_METADATA_OFFLOAD_GET_BE(peer_mdata);

	/*
	 * save msdu flags first, last and continuation msdu in
	 * nbuf->cb, also save mcbc, is_da_valid, is_sa_valid and
	 * length to nbuf->cb. This ensures the info required for
	 * per pkt processing is always in the same cache line.
	 * This helps in improving throughput for smaller pkt
	 * sizes.
	 */
	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_FIRST_MSDU_IN_MPDU)
		qdf_nbuf_set_rx_chfrag_start(nbuf, 1);

	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_MSDU_CONTINUATION)
		qdf_nbuf_set_rx_chfrag_cont(nbuf, 1);

	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_DA_IS_MCBC)
		qdf_nbuf_set_da_mcbc(nbuf, 1);

	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_DA_IS_VALID)
		qdf_nbuf_set_da_valid(nbuf, 1);

	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_SA_IS_VALID)
		qdf_nbuf_set_sa_valid(nbuf, 1);

	if (msdu_desc_info.msdu_flags & HAL_MSDU_F_INTRA_BSS)
		qdf_nbuf_set_intra_bss(nbuf, 1);

	if (qdf_likely(mpdu_desc_info.mpdu_flags &
		       HAL_MPDU_F_QOS_CONTROL_VALID))
		qdf_nbuf_set_tid_val(nbuf, mpdu_desc_info.tid);

	/* set sw exception */
	qdf_nbuf_set_rx_reo_dest_ind_or_sw_excpt(
			nbuf,
			hal_rx_sw_exception_get_be(ring_desc));

	QDF_NBUF_CB_RX_PKT_LEN(nbuf) = msdu_desc_info.msdu_len;

	QDF_NBUF_CB_RX_CTX_ID(nbuf) = reo_ring_num;

	return pkt_capture_offload;
}

static inline uint8_t hal_rx_get_l3_pad_bytes_be(qdf_nbuf_t nbuf,
						 uint8_t *rx_tlv_hdr)
{
	return HAL_RX_TLV_L3_HEADER_PADDING_GET(rx_tlv_hdr);
}

static inline uint8_t
dp_rx_wbm_err_msdu_continuation_get(struct dp_soc *soc,
				    hal_ring_desc_t ring_desc,
				    qdf_nbuf_t nbuf)
{
	return hal_rx_wbm_err_msdu_continuation_get(soc->hal_soc,
						    ring_desc);
}
#else
static inline void
dp_rx_set_link_id_be(qdf_nbuf_t nbuf, uint32_t peer_mdata)
{
}

static inline void
dp_rx_set_mpdu_seq_number_be(qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
}

static inline uint16_t
dp_rx_get_peer_id_be(qdf_nbuf_t nbuf)
{
	uint32_t peer_metadata = QDF_NBUF_CB_RX_MPDU_DESC_INFO_2(nbuf);

	return ((peer_metadata & DP_BE_PEER_METADATA_PEER_ID_MASK) >>
		DP_BE_PEER_METADATA_PEER_ID_SHIFT);
}

static inline void
dp_rx_set_mpdu_msdu_desc_info_in_nbuf(qdf_nbuf_t nbuf,
				      uint32_t mpdu_desc_info,
				      uint32_t peer_mdata,
				      uint32_t msdu_desc_info)
{
	QDF_NBUF_CB_RX_MPDU_DESC_INFO_1(nbuf) = mpdu_desc_info;
	QDF_NBUF_CB_RX_MPDU_DESC_INFO_2(nbuf) = peer_mdata;
	QDF_NBUF_CB_RX_MSDU_DESC_INFO(nbuf) = msdu_desc_info;
}

static inline uint8_t dp_rx_copy_desc_info_in_nbuf_cb(struct dp_soc *soc,
						      hal_ring_desc_t ring_desc,
						      qdf_nbuf_t nbuf,
						      uint8_t reo_ring_num)
{
	uint32_t mpdu_desc_info = 0;
	uint32_t msdu_desc_info = 0;
	uint32_t peer_mdata = 0;

	/* get REO mpdu & msdu desc info */
	hal_rx_get_mpdu_msdu_desc_info_be(ring_desc,
					  &mpdu_desc_info,
					  &peer_mdata,
					  &msdu_desc_info);

	dp_rx_set_mpdu_msdu_desc_info_in_nbuf(nbuf,
					      mpdu_desc_info,
					      peer_mdata,
					      msdu_desc_info);

		return 0;
}

static inline uint8_t hal_rx_get_l3_pad_bytes_be(qdf_nbuf_t nbuf,
						 uint8_t *rx_tlv_hdr)
{
	return QDF_NBUF_CB_RX_L3_PAD_MSB(nbuf) ? 2 : 0;
}

static inline uint8_t
dp_rx_wbm_err_msdu_continuation_get(struct dp_soc *soc,
				    hal_ring_desc_t ring_desc,
				    qdf_nbuf_t nbuf)
{
	return qdf_nbuf_is_rx_chfrag_cont(nbuf);
}
#endif /* CONFIG_NBUF_AP_PLATFORM */

/**
 * dp_rx_wbm_err_copy_desc_info_in_nbuf(): API to copy WBM dest ring
 * descriptor information in nbuf CB/TLV
 *
 * @soc: pointer to Soc structure
 * @ring_desc: wbm dest ring descriptor
 * @nbuf: nbuf to save descriptor information
 * @pool_id: pool id part of wbm error info
 *
 * Return: wbm error information details
 */
static inline uint32_t
dp_rx_wbm_err_copy_desc_info_in_nbuf(struct dp_soc *soc,
				     hal_ring_desc_t ring_desc,
				     qdf_nbuf_t nbuf,
				     uint8_t pool_id)
{
	uint32_t mpdu_desc_info = 0;
	uint32_t msdu_desc_info = 0;
	uint32_t peer_mdata = 0;
	union hal_wbm_err_info_u wbm_err = { 0 };

	/* get WBM mpdu & msdu desc info */
	hal_rx_wbm_err_mpdu_msdu_info_get_be(ring_desc,
					     &wbm_err.info,
					     &mpdu_desc_info,
					     &msdu_desc_info,
					     &peer_mdata);

	wbm_err.info_bit.pool_id = pool_id;
	dp_rx_set_mpdu_msdu_desc_info_in_nbuf(nbuf,
					      mpdu_desc_info,
					      peer_mdata,
					      msdu_desc_info);
	dp_rx_set_wbm_err_info_in_nbuf(soc, nbuf, wbm_err);
	return wbm_err.info;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
struct dp_soc *
dp_get_soc_by_chip_id_be(struct dp_soc *soc, uint8_t chip_id);
#else
static inline struct dp_soc *
dp_get_soc_by_chip_id_be(struct dp_soc *soc, uint8_t chip_id)
{
	return soc;
}
#endif
#endif

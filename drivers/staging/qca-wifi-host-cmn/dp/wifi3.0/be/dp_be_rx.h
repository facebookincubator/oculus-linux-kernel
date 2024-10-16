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

/*
 * dp_rx_intrabss_fwd_be() - API for intrabss fwd. For EAPOL
 *  pkt with DA not equal to vdev mac addr, fwd is not allowed.
 * @soc: core txrx main context
 * @ta_txrx_peer: source peer entry
 * @rx_tlv_hdr: start address of rx tlvs
 * @nbuf: nbuf that has to be intrabss forwarded
 * @msdu_metadata: msdu metadata
 *
 * Return: true if it is forwarded else false
 */

bool dp_rx_intrabss_fwd_be(struct dp_soc *soc,
			   struct dp_txrx_peer *ta_txrx_peer,
			   uint8_t *rx_tlv_hdr,
			   qdf_nbuf_t nbuf,
			   struct hal_rx_msdu_metadata msdu_metadata);
#endif

/**
 * dp_rx_intrabss_mcast_handler_be() - intrabss mcast handler
 * @soc: core txrx main context
 * @ta_txrx_peer: source txrx_peer entry
 * @nbuf_copy: nbuf that has to be intrabss forwarded
 * @tid_stats: tid_stats structure
 *
 * Return: true if it is forwarded else false
 */
bool
dp_rx_intrabss_mcast_handler_be(struct dp_soc *soc,
				struct dp_txrx_peer *ta_txrx_peer,
				qdf_nbuf_t nbuf_copy,
				struct cdp_tid_rx_stats *tid_stats);

void dp_rx_word_mask_subscribe_be(struct dp_soc *soc,
				  uint32_t *msg_word,
				  void *rx_filter);

uint32_t dp_rx_process_be(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl, uint8_t reo_ring_num,
			  uint32_t quota);

/**
 * dp_rx_chain_msdus_be() - Function to chain all msdus of a mpdu
 *			    to pdev invalid peer list
 *
 * @soc: core DP main context
 * @nbuf: Buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @mac_id: mac id
 *
 *  Return: bool: true for last msdu of mpdu
 */
bool dp_rx_chain_msdus_be(struct dp_soc *soc, qdf_nbuf_t nbuf,
			  uint8_t *rx_tlv_hdr, uint8_t mac_id);

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
			      if not, do SW cookie conversion.
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

#define DP_PEER_METADATA_OFFLOAD_GET_BE(_peer_metadata)		(0)

#ifdef DP_USE_REDUCED_PEER_ID_FIELD_WIDTH
static inline uint16_t
dp_rx_peer_metadata_peer_id_get_be(struct dp_soc *soc, uint32_t peer_metadata)
{
	struct htt_rx_peer_metadata_v1 *metadata =
			(struct htt_rx_peer_metadata_v1 *)&peer_metadata;
	uint16_t peer_id;

	peer_id = metadata->peer_id |
		  (metadata->ml_peer_valid << soc->peer_id_shift);

	return peer_id;
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
	struct htt_rx_peer_metadata_v1 *metadata =
			(struct htt_rx_peer_metadata_v1 *)&peer_metadata;

	return metadata->vdev_id;
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
dp_rx_replensih_soc_get(struct dp_soc *soc, uint8_t chip_id);

struct dp_soc *
dp_soc_get_by_idle_bm_id(struct dp_soc *soc, uint8_t idle_bm_id);

uint8_t dp_soc_get_num_soc_be(struct dp_soc *soc);
#else
static inline struct dp_soc *
dp_rx_replensih_soc_get(struct dp_soc *soc, uint8_t chip_id)
{
	return soc;
}

static inline uint8_t
dp_soc_get_num_soc_be(struct dp_soc *soc)
{
	return 1;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_rx_mlo_igmp_handler() - Rx handler for Mcast packets
 * @soc: Handle to DP Soc structure
 * @vdev: DP vdev handle
 * @peer: DP peer handle
 * @nbuf: nbuf to be enqueued
 *
 * Return: true when packet sent to stack, false failure
 */
bool dp_rx_mlo_igmp_handler(struct dp_soc *soc,
			    struct dp_vdev *vdev,
			    struct dp_txrx_peer *peer,
			    qdf_nbuf_t nbuf);

/**
 * dp_peer_rx_reorder_queue_setup() - Send reo queue setup wmi cmd to FW
				      per peer type
 * @soc: DP Soc handle
 * @peer: dp peer to operate on
 * @tid: TID
 * @ba_window_size: BlockAck window size
 *
 * Return: 0 - success, others - failure
 */
static inline
QDF_STATUS dp_peer_rx_reorder_queue_setup_be(struct dp_soc *soc,
					     struct dp_peer *peer,
					     int tid,
					     uint32_t ba_window_size)
{
	uint8_t i;
	struct dp_mld_link_peers link_peers_info;
	struct dp_peer *link_peer;
	struct dp_rx_tid *rx_tid;
	struct dp_soc *link_peer_soc;

	rx_tid = &peer->rx_tid[tid];
	if (!rx_tid->hw_qdesc_paddr)
		return QDF_STATUS_E_INVAL;

	if (!hal_reo_shared_qaddr_is_enable(soc->hal_soc)) {
		if (IS_MLO_DP_MLD_PEER(peer)) {
			/* get link peers with reference */
			dp_get_link_peers_ref_from_mld_peer(soc, peer,
							    &link_peers_info,
							    DP_MOD_ID_CDP);
			/* send WMI cmd to each link peers */
			for (i = 0; i < link_peers_info.num_links; i++) {
				link_peer = link_peers_info.link_peers[i];
				link_peer_soc = link_peer->vdev->pdev->soc;
				if (link_peer_soc->cdp_soc.ol_ops->
						peer_rx_reorder_queue_setup) {
					if (link_peer_soc->cdp_soc.ol_ops->
						peer_rx_reorder_queue_setup(
					    link_peer_soc->ctrl_psoc,
					    link_peer->vdev->pdev->pdev_id,
					    link_peer->vdev->vdev_id,
					    link_peer->mac_addr.raw,
					    rx_tid->hw_qdesc_paddr,
					    tid, tid,
					    1, ba_window_size)) {
						dp_peer_err("%pK: Failed to send reo queue setup to FW - tid %d\n",
							    link_peer_soc, tid);
						return QDF_STATUS_E_FAILURE;
					}
				}
			}
			/* release link peers reference */
			dp_release_link_peers_ref(&link_peers_info,
						  DP_MOD_ID_CDP);
		} else if (peer->peer_type == CDP_LINK_PEER_TYPE) {
			if (soc->cdp_soc.ol_ops->peer_rx_reorder_queue_setup) {
				if (soc->cdp_soc.ol_ops->
					peer_rx_reorder_queue_setup(
				    soc->ctrl_psoc,
				    peer->vdev->pdev->pdev_id,
				    peer->vdev->vdev_id,
				    peer->mac_addr.raw,
				    rx_tid->hw_qdesc_paddr,
				    tid, tid,
				    1, ba_window_size)) {
					dp_peer_err("%pK: Failed to send reo queue setup to FW - tid %d\n",
						    soc, tid);
					return QDF_STATUS_E_FAILURE;
				}
			}
		} else {
			dp_peer_err("invalid peer type %d", peer->peer_type);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
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

		hal_reo_shared_qaddr_write(soc->hal_soc,
					   peer->peer_id,
					   tid, peer->rx_tid[tid].hw_qdesc_paddr);
	}
	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS dp_peer_rx_reorder_queue_setup_be(struct dp_soc *soc,
					     struct dp_peer *peer,
					     int tid,
					     uint32_t ba_window_size)
{
	struct dp_rx_tid *rx_tid = &peer->rx_tid[tid];

	if (!rx_tid->hw_qdesc_paddr)
		return QDF_STATUS_E_INVAL;

	if (soc->cdp_soc.ol_ops->peer_rx_reorder_queue_setup) {
		if (soc->cdp_soc.ol_ops->peer_rx_reorder_queue_setup(
		    soc->ctrl_psoc,
		    peer->vdev->pdev->pdev_id,
		    peer->vdev->vdev_id,
		    peer->mac_addr.raw, rx_tid->hw_qdesc_paddr, tid, tid,
		    1, ba_window_size)) {
			dp_peer_err("%pK: Failed to send reo queue setup to FW - tid %d\n",
				    soc, tid);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
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
		qdf_prefetch(&next->len);
		qdf_prefetch(&next->protocol);
		qdf_prefetch(next->data);
		qdf_prefetch(next->data + 64);
		qdf_prefetch(next->data + 128);
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
 * dp_rx_cookie_2_va_rxdma_buf_prefetch() - function to prefetch the SW desc
 * @soc: Handle to DP Soc structure
 * @cookie: cookie used to lookup virtual address
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
 * dp_rx_prefetch_hw_sw_nbuf_desc() - function to prefetch HW and SW desc
 * @soc: Handle to HAL Soc structure
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
#endif

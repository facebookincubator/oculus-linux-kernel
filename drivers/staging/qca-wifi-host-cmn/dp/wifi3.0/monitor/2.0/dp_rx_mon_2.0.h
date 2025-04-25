/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#ifndef _DP_RX_MON_2_0_H_
#define _DP_RX_MON_2_0_H_

#include <qdf_nbuf_frag.h>
#include <hal_be_api_mon.h>
#include <dp_mon_2.0.h>

#define DP_RX_MON_PACKET_OFFSET 8
#define DP_RX_MON_RX_HDR_OFFSET 8
#define DP_GET_NUM_QWORDS(num)	((num) >> 3)

#define DP_RX_MON_TLV_HDR_MARKER 0xFEED
#define DP_RX_MON_TLV_HDR_MARKER_LEN 2
#define DP_RX_MON_TLV_HDR_LEN 3 /* TLV ID field sz + TLV len field sz */
#define DP_RX_MON_TLV_TOTAL_LEN 2

#define DP_RX_MON_TLV_PF_ID 1
#define DP_RX_MON_TLV_PPDU_ID 2
#define DP_RX_MON_MAX_TLVS 2

#define DP_RX_MON_TLV_MSDU_CNT 2
#define DP_RX_MON_MAX_MSDU 16
#define DP_RX_MON_PF_TLV_LEN (((DP_RX_MON_PF_TAG_LEN_PER_FRAG)\
			       * (DP_RX_MON_MAX_MSDU) * 2)\
			       + (DP_RX_MON_TLV_MSDU_CNT))

#define DP_RX_MON_PPDU_ID_LEN 4

#define DP_RX_MON_INDIV_TLV_LEN ((DP_RX_MON_PF_TLV_LEN)\
				 + (DP_RX_MON_PPDU_ID_LEN))
#define DP_RX_MON_TLV_ROOM ((DP_RX_MON_INDIV_TLV_LEN)\
			    + ((DP_RX_MON_TLV_HDR_LEN) * (DP_RX_MON_MAX_TLVS))\
			    + (DP_RX_MON_TLV_HDR_MARKER_LEN)\
			    + (DP_RX_MON_TLV_TOTAL_LEN))

#define DP_RX_MON_WQ_THRESHOLD 128

#define DP_RX_MON_MAX_RX_HEADER_LEN 128

#ifdef WLAN_PKT_CAPTURE_RX_2_0
QDF_STATUS dp_mon_pdev_ext_init_2_0(struct dp_pdev *pdev);
QDF_STATUS dp_mon_pdev_ext_deinit_2_0(struct dp_pdev *pdev);

#ifdef QCA_KMEM_CACHE_SUPPORT
QDF_STATUS dp_rx_mon_ppdu_info_cache_create(struct dp_pdev *pdev);
void dp_rx_mon_ppdu_info_cache_destroy(struct dp_pdev *pdev);
struct hal_rx_ppdu_info*
dp_rx_mon_get_ppdu_info(struct dp_mon_pdev *mon_pdev);
void
dp_rx_mon_free_ppdu_info(struct dp_pdev *pdev,
			 struct hal_rx_ppdu_info *ppdu_info);
void
__dp_rx_mon_free_ppdu_info(struct dp_mon_pdev *mon_pdev,
			   struct hal_rx_ppdu_info *ppdu_info);
#else
static inline QDF_STATUS dp_rx_mon_ppdu_info_cache_create(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_rx_mon_ppdu_info_cache_destroy(struct dp_pdev *pdev)
{
}

static inline struct hal_rx_ppdu_info*
dp_rx_mon_get_ppdu_info(struct dp_mon_pdev *mon_pdev)
{
	qdf_mem_zero(&mon_pdev->ppdu_info, sizeof(struct hal_rx_ppdu_info));
	return &mon_pdev->ppdu_info;
}

static inline void
dp_rx_mon_free_ppdu_info(struct dp_pdev *pdev,
			 struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline void
__dp_rx_mon_free_ppdu_info(struct dp_mon_pdev *mon_pdev,
			   struct hal_rx_ppdu_info *ppdu_info)
{
}
#endif

QDF_STATUS dp_rx_mon_pdev_htt_srng_setup_2_0(struct dp_soc *soc,
					    struct dp_pdev *pdev,
					    int mac_id,
					    int mac_for_pdev);
QDF_STATUS dp_rx_mon_soc_htt_srng_setup_2_0(struct dp_soc *soc,
					    int mac_id);
QDF_STATUS dp_rx_mon_pdev_rings_alloc_2_0(struct dp_pdev *pdev, int lmac_id);
void dp_rx_mon_pdev_rings_free_2_0(struct dp_pdev *pdev, int lmac_id);
QDF_STATUS dp_rx_mon_pdev_rings_init_2_0(struct dp_pdev *pdev, int lmac_id);
void dp_rx_mon_pdev_rings_deinit_2_0(struct dp_pdev *pdev, int lmac_id);
QDF_STATUS dp_rx_mon_soc_init_2_0(struct dp_soc *soc);

/*
 * dp_rx_mon_buffers_alloc() - allocate rx monitor buffers
 * @soc: DP soc handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS
dp_rx_mon_buffers_alloc(struct dp_soc *soc, uint32_t size);

/*
 * dp_rx_mon_buffers_free() - free rx monitor buffers
 * @soc: dp soc handle
 *
 */
void
dp_rx_mon_buffers_free(struct dp_soc *soc);

/*
 * dp_rx_mon_desc_pool_deinit() - deinit rx monitor descriptor pool
 * @soc: dp soc handle
 *
 */
void
dp_rx_mon_buf_desc_pool_deinit(struct dp_soc *soc);

/*
 * dp_rx_mon_desc_pool_deinit() - deinit rx monitor descriptor pool
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS
dp_rx_mon_buf_desc_pool_init(struct dp_soc *soc);

/*
 * dp_rx_mon_buf_desc_pool_free() - free rx monitor descriptor pool
 * @soc: dp soc handle
 *
 */
void dp_rx_mon_buf_desc_pool_free(struct dp_soc *soc);

/*
 * dp_rx_mon_buf_desc_pool_alloc() - allocate rx monitor descriptor pool
 * @soc: DP soc handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS
dp_rx_mon_buf_desc_pool_alloc(struct dp_soc *soc);

/**
 * dp_rx_mon_stats_update_2_0() - update rx stats
 *
 * @mon_peer: monitor peer handle
 * @ppdu: Rx PPDU status metadata object
 * @ppdu_user: Rx PPDU user status metadata object
 *
 * Return: Void
 */
void dp_rx_mon_stats_update_2_0(struct dp_mon_peer *mon_peer,
				struct cdp_rx_indication_ppdu *ppdu,
				struct cdp_rx_stats_ppdu_user *ppdu_user);

/**
 * dp_rx_mon_populate_ppdu_usr_info_2_0() - Populate ppdu user info
 *
 * @rx_user_status: Rx user status
 * @ppdu_user: ppdu user metadata
 *
 * Return: void
 */
void
dp_rx_mon_populate_ppdu_usr_info_2_0(struct mon_rx_user_status *rx_user_status,
				     struct cdp_rx_stats_ppdu_user *ppdu_user);

/**
 * dp_rx_mon_populate_ppdu_info_2_0() --  Populate ppdu info
 *
 * @hal_ppdu_info: HAL PPDU info
 * @ppdu: Rx PPDU status metadata object
 *
 * Return: void
 */
void
dp_rx_mon_populate_ppdu_info_2_0(struct hal_rx_ppdu_info *hal_ppdu_info,
				 struct cdp_rx_indication_ppdu *ppdu);

QDF_STATUS dp_rx_mon_soc_attach_2_0(struct dp_soc *soc, int lmac_id);
void  dp_rx_mon_soc_detach_2_0(struct dp_soc *soc, int lmac_id);
void dp_rx_mon_soc_deinit_2_0(struct dp_soc *soc, uint32_t lmac_id);

#ifndef QCA_MONITOR_2_0_PKT_SUPPORT
static inline QDF_STATUS dp_rx_mon_init_wq_sm(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_rx_mon_deinit_wq_sm(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_rx_mon_add_ppdu_info_to_wq(struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info)
{
	return QDF_STATUS_SUCCESS;
}

static inline int
dp_rx_mon_flush_packet_tlv(struct dp_pdev *pdev, void *buf, uint16_t end_offset,
			   union dp_mon_desc_list_elem_t **desc_list,
			   union dp_mon_desc_list_elem_t **tail)
{
	return 0;
}

static inline void
dp_rx_mon_handle_rx_hdr(struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info,
			void *status_frag)
{
}

static inline uint16_t
dp_rx_mon_handle_mon_buf_addr(struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info,
			      union dp_mon_desc_list_elem_t **desc_list,
			      union dp_mon_desc_list_elem_t **tail)
{
	return 0;
}

static inline void
dp_rx_mon_handle_msdu_end(struct dp_pdev *pdev,
			  struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline void
dp_rx_mon_reset_mpdu_q(struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline void
dp_rx_mon_handle_mpdu_start(struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline void
dp_rx_mon_handle_mpdu_end(struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline QDF_STATUS
dp_rx_mon_nbuf_add_rx_frag(qdf_nbuf_t nbuf, qdf_frag_t *frag,
			   uint16_t frag_len, uint16_t offset,
			   uint16_t buf_size, bool frag_ref)
{
	return 0;
}

static inline void
dp_rx_mon_pf_tag_to_buf_headroom_2_0(void *nbuf,
				     struct hal_rx_ppdu_info *ppdu_info,
				     struct dp_pdev *pdev, struct dp_soc *soc)
{
}
#endif
#else
static inline QDF_STATUS dp_mon_pdev_ext_init_2_0(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_mon_pdev_ext_deinit_2_0(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_rx_mon_ppdu_info_cache_create(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_rx_mon_ppdu_info_cache_destroy(struct dp_pdev *pdev)
{
}

static inline struct hal_rx_ppdu_info*
dp_rx_mon_get_ppdu_info(struct dp_mon_pdev *mon_pdev)
{
	return NULL;
}

static inline void
dp_rx_mon_free_ppdu_info(struct dp_pdev *pdev,
			 struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline QDF_STATUS
dp_rx_mon_buffers_alloc(struct dp_soc *soc, uint32_t size)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_mon_soc_init_2_0(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_rx_mon_buffers_free(struct dp_soc *soc)

{
}

static inline void
dp_rx_mon_buf_desc_pool_deinit(struct dp_soc *soc)
{
}

static inline QDF_STATUS
dp_rx_mon_buf_desc_pool_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_rx_mon_buf_desc_pool_free(struct dp_soc *soc)
{
}

static inline QDF_STATUS
dp_rx_mon_buf_desc_pool_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_rx_mon_stats_update_2_0(struct dp_mon_peer *mon_peer,
				struct cdp_rx_indication_ppdu *ppdu,
				struct cdp_rx_stats_ppdu_user *ppdu_user)
{
}

static inline void
dp_rx_mon_populate_ppdu_usr_info_2_0(struct mon_rx_user_status *rx_user_status,
				     struct cdp_rx_stats_ppdu_user *ppdu_user)
{
}

static inline void
dp_rx_mon_populate_ppdu_info_2_0(struct hal_rx_ppdu_info *hal_ppdu_info,
				 struct cdp_rx_indication_ppdu *ppdu)
{
}

static inline
QDF_STATUS dp_rx_mon_pdev_htt_srng_setup_2_0(struct dp_soc *soc,
					    struct dp_pdev *pdev,
					    int mac_id,
					    int mac_for_pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_mon_soc_htt_srng_setup_2_0(struct dp_soc *soc,
					    int mac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_mon_pdev_rings_alloc_2_0(struct dp_pdev *pdev, int lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_rx_mon_pdev_rings_free_2_0(struct dp_pdev *pdev, int lmac_id)
{
}

static inline
QDF_STATUS dp_rx_mon_pdev_rings_init_2_0(struct dp_pdev *pdev, int lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_rx_mon_pdev_rings_deinit_2_0(struct dp_pdev *pdev, int lmac_id)
{
}

static inline
QDF_STATUS dp_rx_mon_soc_attach_2_0(struct dp_soc *soc, int lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_rx_mon_soc_detach_2_0(struct dp_soc *soc, int lmac_id)
{
}

static inline
void dp_rx_mon_soc_deinit_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
}
#endif

#if !defined(DISABLE_MON_CONFIG) && defined(WLAN_PKT_CAPTURE_RX_2_0)
/*
 * dp_rx_mon_process_2_0() - Process Rx monitor interrupt
 *
 * @soc: DP soc handle
 * @int_ctx: Interrupt context
 * @mac_id: LMAC id
 * @quota: quota to reap
 */
uint32_t
dp_rx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota);

/**
 * dp_rx_mon_process_ppdu() - RxMON Workqueue processing API
 *
 * @context: workqueue context
 */
void dp_rx_mon_process_ppdu(void *context);
#else
static inline uint32_t
dp_rx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota)
{
	return 0;
}

static inline void dp_rx_mon_process_ppdu(void *context)
{
}
#endif /* DISABLE_MON_CONFIG */

/**
 * dp_rx_mon_handle_full_mon() - Handle full monitor MPDU restitch
 *
 * @pdev: DP pdev
 * @ppdu_info: PPDU info
 * @mpdu: mpdu buf
 *
 * Return: SUCCESS or Failure
 */
QDF_STATUS
dp_rx_mon_handle_full_mon(struct dp_pdev *pdev,
			  struct hal_rx_ppdu_info *ppdu_info,
			  qdf_nbuf_t mpdu);

/**
 * dp_rx_mon_drain_wq() - Drain monitor buffers from rxmon workqueue
 *
 * @pdev: DP pdev handle
 *
 * Return: Void
 */
void dp_rx_mon_drain_wq(struct dp_pdev *pdev);

/**
 * dp_mon_free_parent_nbuf() - Free parent SKB
 *
 * @mon_pdev: monitor pdev
 * @nbuf: SKB to be freed
 *
 * Return: void
 */
void dp_mon_free_parent_nbuf(struct dp_mon_pdev *mon_pdev,
			qdf_nbuf_t nbuf);

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_rx_print_advanced_stats_2_0() - print advanced monitor statistics
 *
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 *
 * Return: void
 */
void dp_mon_rx_print_advanced_stats_2_0(struct dp_soc *soc,
					struct dp_pdev *pdev);
#else
static inline
void dp_mon_rx_print_advanced_stats_2_0(struct dp_soc *soc,
					struct dp_pdev *pdev)
{
}
#endif

#ifdef BE_PKTLOG_SUPPORT
/**
 * dp_rx_process_pktlog_be() - process pktlog
 * @soc: dp soc handle
 * @pdev: dp pdev handle
 * @ppdu_info: HAL PPDU info
 * @status_frag: frag pointer which needs to be added to nbuf
 * @end_offset: Offset in frag to be added to nbuf_frags
 *
 * Return: QDF_STATUS_SUCCESS or Failure
 */
QDF_STATUS
dp_rx_process_pktlog_be(struct dp_soc *soc, struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info,
			void *status_frag, uint32_t end_offset);
#else
static inline QDF_STATUS
dp_rx_process_pktlog_be(struct dp_soc *soc, struct dp_pdev *pdev,
			struct hal_rx_ppdu_info *ppdu_info,
			void *status_frag, uint32_t end_offset)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_rx_mon_append_nbuf() - Append nbuf to parent nbuf
 * @nbuf: Parent nbuf
 * @tmp_nbuf: nbuf to be attached to parent
 *
 * Return: void
 */
void dp_rx_mon_append_nbuf(qdf_nbuf_t nbuf, qdf_nbuf_t tmp_nbuf);
#endif /* _DP_RX_MON_2_0_H_ */

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

#ifndef _DP_MON_2_0_H_
#define _DP_MON_2_0_H_

#if !defined(DISABLE_MON_CONFIG)
#include <qdf_lock.h>
#include <dp_types.h>
#include <dp_mon.h>
#include <dp_mon_filter.h>
#include <dp_htt.h>
#include <dp_mon.h>
#include <dp_tx_mon_2.0.h>

#define DP_MON_RING_FILL_LEVEL_DEFAULT 2048
#define DP_MON_DATA_BUFFER_SIZE     2048
#define DP_MON_DESC_MAGIC 0xdeadabcd
#define DP_MON_MAX_STATUS_BUF 1200
#define DP_MON_QUEUE_DEPTH_MAX 16
#define DP_MON_MSDU_LOGGING 0
#define DP_MON_MPDU_LOGGING 1

#define DP_MON_DECAP_FORMAT_INVALID 0xff
#define DP_MON_MIN_FRAGS_FOR_RESTITCH 2

/* monitor frame filter modes */
enum dp_mon_frm_filter_mode {
	/* mode filter pass */
	DP_MON_FRM_FILTER_MODE_FP = 0,
	/* mode monitor direct */
	DP_MON_FRM_FILTER_MODE_MD = 1,
	/* mode monitor other */
	DP_MON_FRM_FILTER_MODE_MO = 2,
	/* mode filter pass monitor other */
	DP_MON_FRM_FILTER_MODE_FP_MO = 3,
};

/* mpdu filter categories */
enum dp_mpdu_filter_category {
	/* category filter pass */
	DP_MPDU_FILTER_CATEGORY_FP = 0,
	/* category monitor direct */
	DP_MPDU_FILTER_CATEGORY_MD = 1,
	/* category monitor other */
	DP_MPDU_FILTER_CATEGORY_MO = 2,
	/* category filter pass monitor override */
	DP_MPDU_FILTER_CATEGORY_FP_MO = 3,
};

/**
 * struct dp_mon_filter_be - Monitor TLV filter
 * @rx_tlv_filter: Rx MON TLV filter
 * @tx_tlv_filter: Tx MON TLV filter
 * @tx_valid: enable/disable Tx Mon TLV filter
 */
struct dp_mon_filter_be {
	struct dp_mon_filter rx_tlv_filter;
#ifdef QCA_MONITOR_2_0_SUPPORT
	struct htt_tx_ring_tlv_filter tx_tlv_filter;
#endif
	bool tx_valid;
};

/**
 * struct dp_mon_desc
 *
 * @buf_addr: virtual address
 * @paddr: physical address
 * @in_use: desc is in use
 * @unmapped: used to mark desc an unmapped if the corresponding
 * nbuf is already unmapped
 * @end_offset: offset in status buffer where DMA ended
 * @cookie: unique desc identifier
 * @magic: magic number to validate desc data
 */
struct dp_mon_desc {
	uint8_t *buf_addr;
	qdf_dma_addr_t paddr;
	uint8_t in_use:1,
		unmapped:1;
	uint16_t end_offset;
	uint32_t cookie;
	uint32_t magic;
};

/**
 * struct dp_mon_desc_list_elem_t
 * @next: Next pointer to form free list
 * @mon_desc: DP mon descriptor
 */
union dp_mon_desc_list_elem_t {
	union dp_mon_desc_list_elem_t *next;
	struct dp_mon_desc mon_desc;
};

/**
 * struct dp_mon_desc_pool - monitor desc pool
 * @pool_size: number of descriptor in the pool
 * @array: pointer to array of descriptor
 * @freelist: pointer to free descriptor list
 * @lock: Protection for the descriptor pool
 * @owner: owner for nbuf
 * @buf_size: Buffer size
 * @buf_alignment: Buffer alignment
 * @pf_cache: page frag cache
 */
struct dp_mon_desc_pool {
	uint32_t pool_size;
	union dp_mon_desc_list_elem_t *array;
	union dp_mon_desc_list_elem_t *freelist;
	qdf_spinlock_t lock;
	uint8_t owner;
	uint16_t buf_size;
	uint8_t buf_alignment;
	qdf_frag_cache_t pf_cache;
};

/*
 * NB: intentionally not using kernel-doc comment because the kernel-doc
 *     script does not handle the TAILQ_HEAD macro
 * struct dp_mon_pdev_be - BE specific monitor pdev object
 * @mon_pdev: monitor pdev structure
 * @filter_be: filters sent to fw
 * @tx_mon_mode: tx monitor mode
 * @tx_mon_filter_length: tx monitor filter length
 * @tx_monitor_be: pointer to tx monitor be structure
 * @tx_stats: tx monitor drop stats
 * @rx_mon_wq_lock: Rx mon workqueue lock
 * @rx_mon_workqueue: Rx mon workqueue
 * @rx_mon_work: Rx mon work
 * @rx_mon_queue: RxMON queue
 * @rx_mon_free_queue: RxMON ppdu info free element queue
 * @ppdu_info_lock: RxPPDU ppdu info queue lock
 * @rx_mon_queue_depth: RxMON queue depth
 * @desc_count: reaped status desc count
 * @status: reaped status buffer per ppdu
 * @lite_mon_rx_config: rx litemon config
 * @lite_mon_tx_config: tx litemon config
 * @prev_rxmon_desc: prev destination desc
 * @prev_rxmon_cookie: prev rxmon cookie
 * @ppdu_info_cache: PPDU info cache
 * @total_free_elem: total free element in queue
 */
struct dp_mon_pdev_be {
	struct dp_mon_pdev mon_pdev;
	struct dp_mon_filter_be **filter_be;
	uint8_t tx_mon_mode;
	uint8_t tx_mon_filter_length;
	struct dp_pdev_tx_monitor_be tx_monitor_be;
	struct dp_tx_monitor_drop_stats tx_stats;
	qdf_spinlock_t rx_mon_wq_lock;
	qdf_workqueue_t *rx_mon_workqueue;
	qdf_work_t rx_mon_work;

	TAILQ_HEAD(, hal_rx_ppdu_info) rx_mon_queue;
	TAILQ_HEAD(, hal_rx_ppdu_info) rx_mon_free_queue;
	qdf_spinlock_t ppdu_info_lock;
	uint16_t rx_mon_queue_depth;
	uint16_t desc_count;
	struct dp_mon_desc *status[DP_MON_MAX_STATUS_BUF];
#ifdef QCA_SUPPORT_LITE_MONITOR
	struct dp_lite_mon_rx_config *lite_mon_rx_config;
	struct dp_lite_mon_tx_config *lite_mon_tx_config;
#endif
	void *prev_rxmon_desc;
	uint32_t prev_rxmon_cookie;
	qdf_kmem_cache_t ppdu_info_cache;
	uint32_t total_free_elem;
};

/**
 * struct dp_mon_soc_be - BE specific monitor soc
 * @mon_soc: Monitor soc structure
 * @tx_mon_buf_ring: TxMon replenish ring
 * @tx_mon_dst_ring: TxMon Destination ring
 * @tx_desc_mon: descriptor pool for tx mon src ring
 * @rx_desc_mon: descriptor pool for rx mon src ring
 * @rx_mon_ring_fill_level: rx mon ring refill level
 * @tx_mon_ring_fill_level: tx mon ring refill level
 * @tx_low_thresh_intrs: number of tx mon low threshold interrupts received
 * @rx_low_thresh_intrs: number of rx mon low threshold interrupts received
 * @is_dp_mon_soc_initialized: flag to indicate soc is initialized
 */
struct dp_mon_soc_be {
	struct dp_mon_soc mon_soc;
	/* Source ring for Tx monitor */
	struct dp_srng tx_mon_buf_ring;
	struct dp_srng tx_mon_dst_ring[MAX_NUM_LMAC_HW];

	/* Sw descriptor pool for tx mon source ring */
	struct dp_mon_desc_pool tx_desc_mon;
	/* Sw descriptor pool for rx mon source ring */
	struct dp_mon_desc_pool rx_desc_mon;

	uint16_t rx_mon_ring_fill_level;
	uint16_t tx_mon_ring_fill_level;
	uint32_t tx_low_thresh_intrs;
	uint32_t rx_low_thresh_intrs;

	bool is_dp_mon_soc_initialized;
};
#endif

/**
 * dp_mon_desc_pool_init() - Monitor descriptor pool init
 * @mon_desc_pool: mon desc pool
 * @pool_size: Pool size
 *
 * Return: non-zero for failure, zero for success
 */
QDF_STATUS
dp_mon_desc_pool_init(struct dp_mon_desc_pool *mon_desc_pool,
		      uint32_t pool_size);

/**
 * dp_mon_desc_pool_deinit()- monitor descriptor pool deinit
 * @mon_desc_pool: mon desc pool
 *
 * Return: None
 *
 */
void dp_mon_desc_pool_deinit(struct dp_mon_desc_pool *mon_desc_pool);

/**
 * dp_mon_desc_pool_free()- monitor descriptor pool free
 * @mon_desc_pool: mon desc pool
 *
 * Return: None
 *
 */
void dp_mon_desc_pool_free(struct dp_mon_desc_pool *mon_desc_pool);

/**
 * dp_mon_desc_pool_alloc() - Monitor descriptor pool alloc
 * @pool_size: Pool size
 * @mon_desc_pool: mon desc pool
 *
 * Return: non-zero for failure, zero for success
 */
QDF_STATUS dp_mon_desc_pool_alloc(uint32_t pool_size,
				  struct dp_mon_desc_pool *mon_desc_pool);

/**
 * dp_mon_pool_frag_unmap_and_free() - free the mon desc frag called during
 *			    de-initialization of wifi module.
 *
 * @dp_soc: DP soc handle
 * @mon_desc_pool: monitor descriptor pool pointer
 *
 * Return: None
 */
void dp_mon_pool_frag_unmap_and_free(struct dp_soc *dp_soc,
				     struct dp_mon_desc_pool *mon_desc_pool);

/**
 * dp_mon_buffers_replenish() - replenish monitor ring with nbufs
 *
 * @dp_soc: core txrx main context
 * @dp_mon_srng: dp monitor circular ring
 * @mon_desc_pool: Pointer to free mon descriptor pool
 * @num_req_buffers: number of buffer to be replenished
 * @desc_list: list of descs if called from dp_rx_process
 *	       or NULL during dp rx initialization or out of buffer
 *	       interrupt.
 * @tail: tail of descs list
 * @replenish_cnt_ref: pointer to update replenish_cnt
 *
 * Return: return success or failure
 */
QDF_STATUS dp_mon_buffers_replenish(struct dp_soc *dp_soc,
				struct dp_srng *dp_mon_srng,
				struct dp_mon_desc_pool *mon_desc_pool,
				uint32_t num_req_buffers,
				union dp_mon_desc_list_elem_t **desc_list,
				union dp_mon_desc_list_elem_t **tail,
				uint32_t *replenish_cnt_ref);

/**
 * dp_mon_filter_show_tx_filter_be() - Show the set filters
 * @mode: The filter modes
 * @filter: tlv filter
 */
void dp_mon_filter_show_tx_filter_be(enum dp_mon_filter_mode mode,
				     struct dp_mon_filter_be *filter);

/**
 * dp_mon_filter_show_rx_filter_be() - Show the set filters
 * @mode: The filter modes
 * @filter: tlv filter
 */
void dp_mon_filter_show_rx_filter_be(enum dp_mon_filter_mode mode,
				     struct dp_mon_filter_be *filter);

/**
 * dp_vdev_set_monitor_mode_buf_rings_tx_2_0() - Add buffers to tx ring
 * @pdev: Pointer to dp_pdev object
 * @num_of_buffers: Number of buffers to allocate
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_vdev_set_monitor_mode_buf_rings_tx_2_0(struct dp_pdev *pdev,
						     uint16_t num_of_buffers);

/**
 * dp_vdev_set_monitor_mode_buf_rings_rx_2_0() - Add buffers to rx ring
 * @pdev: Pointer to dp_pdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_vdev_set_monitor_mode_buf_rings_rx_2_0(struct dp_pdev *pdev);

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_get_puncture_type() - Get puncture type
 * @puncture_pattern: puncture bitmap
 * @bw: Bandwidth
 */
enum cdp_punctured_modes
dp_mon_get_puncture_type(uint16_t puncture_pattern, uint8_t bw);
#endif

/**
 * dp_mon_desc_get() - get monitor sw descriptor
 *
 * @cookie: cookie
 *
 * Return: dp_mon_desc
 */
static inline
struct dp_mon_desc *dp_mon_desc_get(uint64_t *cookie)
{
	return (struct dp_mon_desc *)cookie;
}

/**
 * __dp_mon_add_to_free_desc_list() - Adds to a local free descriptor list
 *
 * @head: pointer to the head of local free list
 * @tail: pointer to the tail of local free list
 * @new: new descriptor that is added to the free list
 * @func_name: caller func name
 *
 * Return: void
 */
static inline
void __dp_mon_add_to_free_desc_list(union dp_mon_desc_list_elem_t **head,
				    union dp_mon_desc_list_elem_t **tail,
				    struct dp_mon_desc *new,
				    const char *func_name)
{
	qdf_assert(head && new);

	new->buf_addr = NULL;
	new->in_use = 0;

	((union dp_mon_desc_list_elem_t *)new)->next = *head;
	*head = (union dp_mon_desc_list_elem_t *)new;
	 /* reset tail if head->next is NULL */
	if (!*tail || !(*head)->next)
		*tail = *head;
}

#define dp_mon_add_to_free_desc_list(head, tail, new) \
	__dp_mon_add_to_free_desc_list(head, tail, new, __func__)

/**
 * dp_mon_add_desc_list_to_free_list() - append unused desc_list back to
 * freelist.
 *
 * @soc: core txrx main context
 * @local_desc_list: local desc list provided by the caller
 * @tail: attach the point to last desc of local desc list
 * @mon_desc_pool: monitor descriptor pool pointer
 */

void
dp_mon_add_desc_list_to_free_list(struct dp_soc *soc,
				  union dp_mon_desc_list_elem_t **local_desc_list,
				  union dp_mon_desc_list_elem_t **tail,
				  struct dp_mon_desc_pool *mon_desc_pool);

/**
 * dp_rx_mon_add_frag_to_skb() - Add page frag to skb
 *
 * @ppdu_info: PPDU status info
 * @nbuf: SKB to which frag need to be added
 * @status_frag: Frag to add
 *
 * Return: void
 */
static inline void
dp_rx_mon_add_frag_to_skb(struct hal_rx_ppdu_info *ppdu_info,
			  qdf_nbuf_t nbuf,
			  qdf_frag_t status_frag)
{
	uint16_t num_frags;

	num_frags = qdf_nbuf_get_nr_frags(nbuf);
	if (num_frags < QDF_NBUF_MAX_FRAGS) {
		qdf_nbuf_add_rx_frag(status_frag, nbuf,
				     ppdu_info->data - (unsigned char *)status_frag,
				     ppdu_info->hdr_len,
				     RX_MONITOR_BUFFER_SIZE,
				     false);
	} else {
		dp_mon_err("num_frags exceeding MAX frags");
		qdf_assert_always(0);
	}
}

#if defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) ||\
	defined(WLAN_SUPPORT_RX_FLOW_TAG)
/**
 * dp_mon_rx_update_rx_protocol_tag_stats() - Update mon protocols's
 *					      statistics from given protocol
 *					      type
 * @pdev: pdev handle
 * @protocol_index: Protocol index for which the stats should be incremented
 *
 * Return: void
 */
void dp_mon_rx_update_rx_protocol_tag_stats(struct dp_pdev *pdev,
					    uint16_t protocol_index);
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

#if !defined(DISABLE_MON_CONFIG) && defined(QCA_MONITOR_2_0_SUPPORT)
/**
 * dp_mon_get_context_size_be() - get BE specific size for mon pdev/soc
 * @context_type: context type for which the size is needed
 *
 * Return: size in bytes for the context_type
 */
static inline
qdf_size_t dp_mon_get_context_size_be(enum dp_context_type context_type)
{
	switch (context_type) {
	case DP_CONTEXT_TYPE_MON_SOC:
		return sizeof(struct dp_mon_soc_be);
	case DP_CONTEXT_TYPE_MON_PDEV:
		return sizeof(struct dp_mon_pdev_be);
	default:
		return 0;
	}
}
#endif

#ifdef QCA_MONITOR_2_0_SUPPORT
/**
 * dp_get_be_mon_soc_from_dp_mon_soc() - get dp_mon_soc_be from dp_mon_soc
 * @soc: dp_mon_soc pointer
 *
 * Return: dp_mon_soc_be pointer
 */
static inline
struct dp_mon_soc_be *dp_get_be_mon_soc_from_dp_mon_soc(struct dp_mon_soc *soc)
{
	return (struct dp_mon_soc_be *)soc;
}

/**
 * dp_get_be_mon_pdev_from_dp_mon_pdev() - get dp_mon_pdev_be from dp_mon_pdev
 * @mon_pdev: dp_mon_pdev pointer
 *
 * Return: dp_mon_pdev_be pointer
 */
static inline
struct dp_mon_pdev_be *dp_get_be_mon_pdev_from_dp_mon_pdev(struct dp_mon_pdev *mon_pdev)
{
	return (struct dp_mon_pdev_be *)mon_pdev;
}
#endif
#endif /* _DP_MON_2_0_H_ */

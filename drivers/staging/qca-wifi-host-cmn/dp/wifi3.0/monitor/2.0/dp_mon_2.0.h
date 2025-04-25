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
#ifdef WLAN_PKT_CAPTURE_TX_2_0
#include <dp_tx_mon_2.0.h>
#endif
#define DP_MON_RING_FILL_LEVEL_DEFAULT 2048
#define DP_MON_DATA_BUFFER_SIZE     2048
#define DP_MON_DESC_MAGIC 0xdeadabcd
#define DP_MON_MAX_STATUS_BUF 1200
#define DP_MON_QUEUE_DEPTH_MAX 16
#define DP_MON_MSDU_LOGGING 0
#define DP_MON_MPDU_LOGGING 1
#define DP_MON_DESC_ADDR_MASK 0x000000FFFFFFFFFF
#define DP_MON_DESC_ADDR_SHIFT 40
#define DP_MON_DESC_FIXED_ADDR_MASK 0xFFFFFF
#define DP_MON_DESC_FIXED_ADDR ((uint64_t)DP_MON_DESC_FIXED_ADDR_MASK << \
	DP_MON_DESC_COOKIE_LSB)
#define DP_MON_DESC_COOKIE_MASK 0xFFFFFF0000000000
#define DP_MON_DESC_COOKIE_SHIFT 24
#define DP_MON_DESC_COOKIE_LSB 40
#define DP_MON_GET_COOKIE(mon_desc) \
	((uint32_t)(((unsigned long long)(mon_desc) & DP_MON_DESC_COOKIE_MASK) \
	>> DP_MON_DESC_COOKIE_LSB))

#ifdef DP_RX_MON_DESC_64_BIT
#define  DP_MON_GET_DESC(mon_desc) \
	((struct dp_mon_desc *)(uintptr_t)(((unsigned long long)(mon_desc) & \
	DP_MON_DESC_ADDR_MASK) | ((unsigned long long)DP_MON_DESC_FIXED_ADDR)))

#else
#define  DP_MON_GET_DESC(mon_desc) \
	((struct dp_mon_desc *)(uintptr_t)(((unsigned long)(mon_desc) & \
	DP_MON_DESC_ADDR_MASK)))
#endif

#define DP_MON_DECAP_FORMAT_INVALID 0xff
#define DP_MON_MIN_FRAGS_FOR_RESTITCH 2

#ifdef MONITOR_TLV_RECORDING_ENABLE
#define MONITOR_TLV_RECORDING_RX 1
#define MONITOR_TLV_RECORDING_TX 2
#define MONITOR_TLV_RECORDING_RXTX 3

#define MAX_TLV_LOGGING_SIZE 1024

#define MAX_PPDU_START_TLV_NUM 38
#define MAX_MPDU_TLV_NUM 160
#define MAX_PPDU_END_TLV_NUM 57

#define MAX_NUM_PPDU_RECORD 4
#define MAX_TLVS_PER_PPDU 255

/*
 * struct dp_mon_tlv_info - recorded information of each TLV
 * @tlv_tag: tlv tag
 * @data: union of struct of fields to be recorded for each TLV
 *
 * Tag and its corresponding important fields are stored in this struct
 */
struct dp_mon_tlv_info {
	uint32_t tlv_tag:10;
	union {
		struct hal_ppdu_start_tlv_record ppdu_start;
		struct hal_ppdu_start_user_info_tlv_record  ppdu_start_user_info;
		struct hal_mpdu_start_tlv_record mpdu_start;
		struct hal_mpdu_end_tlv_record mpdu_end;
		struct hal_header_tlv_record header;
		struct hal_msdu_end_tlv_record msdu_end;
		struct hal_mon_buffer_addr_tlv_record mon_buffer_addr;
		struct hal_phy_location_tlv_record phy_location;
		struct hal_ppdu_end_user_stats_tlv_record ppdu_end_user_stats;
		struct hal_pcu_ppdu_end_info_tlv_record pcu_ppdu_end_info;
		struct hal_phy_rx_ht_sig_tlv_record phy_rx_ht_sig;
		uint32_t data:22;
	} data;
};

/*
 * struct dp_tx_mon_tlv_info - recorded information of each Tx TLV
 * @tlv_tag: tlv tag
 * @data: union of struct of fields to be recorded for each TLV
 *
 * Tag and its corresponding important fields are stored in this struct
 */

struct dp_tx_mon_tlv_info {
	uint32_t tlv_tag:10;
	union {
		/*struct of Tx TLVs to be added here*/
		uint32_t data:22;
	} data;
};

/**
 * struct dp_mon_tlv_logger - contains indexes and other data of the buffer
 * @buff: buffer in which TLVs are stored
 * @curr_ppdu_pos: position of the next ppdu to be written
 * @ppdu_start_idx: starting index form which PPDU start level TLVs are stored for a ppdu
 * @mpdu_idx: starting index form which MPDU TLVs are stored for a ppdu
 * @ppdu_end_idx: starting index form which PPDU end level TLVs are stored for a ppdu
 * @max_ppdu_start_idx: ending index for PPDU start level TLVs for a ppdu
 * @max_mpdu_idx: ending index for MPDU level TLVs for a ppdu
 * @max_ppdu_end_idx: ending index for PPDU end level TLVs for a ppdu
 * @wrap_flag: flag toggle between consecutive PPDU
 * @tlv_logging_enable: check is tlv logging is enabled
 *
 */
struct dp_mon_tlv_logger {
	void *buff;
	uint16_t curr_ppdu_pos;
	uint16_t ppdu_start_idx;
	uint16_t mpdu_idx;
	uint16_t ppdu_end_idx;
	uint16_t max_ppdu_start_idx;
	uint16_t max_ppdu_end_idx;
	uint16_t max_mpdu_idx;
	uint8_t wrap_flag;
	bool tlv_logging_enable;
};
#endif

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
#ifdef WLAN_PKT_CAPTURE_TX_2_0
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
 * @cookie_2: unique cookie provided as part of 64 bit cookie to HW
 * @end_offset: offset in status buffer where DMA ended
 * @cookie: unique desc identifier
 * @magic: magic number to validate desc data
 */
struct dp_mon_desc {
	uint8_t *buf_addr;
	qdf_dma_addr_t paddr;
	uint32_t in_use:1,
		unmapped:1,
		cookie_2:24;
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
 * @ppdu_info_cache: PPDU info cache
 * @desc_count: reaped status desc count
 * @status: reaped status buffer per ppdu
 * @lite_mon_rx_config: rx litemon config
 * @lite_mon_tx_config: tx litemon config
 * @prev_rxmon_desc: prev destination desc
 * @prev_rxmon_cookie: prev rxmon cookie
 * @prev_rxmon_pkt_desc: prev packet buff desc
 * @prev_rxmon_pkt_cookie: prev packet buff desc cookie
 * @total_free_elem: total free element in queue
 * @rx_tlv_logger: Rx TLV logger struct
 */
struct dp_mon_pdev_be {
	struct dp_mon_pdev mon_pdev;
	struct dp_mon_filter_be **filter_be;
#ifdef WLAN_PKT_CAPTURE_TX_2_0
	uint8_t tx_mon_mode;
	uint8_t tx_mon_filter_length;
	struct dp_pdev_tx_monitor_be tx_monitor_be;
	struct dp_tx_monitor_drop_stats tx_stats;
#endif
#if defined(WLAN_PKT_CAPTURE_RX_2_0) && defined(QCA_MONITOR_2_0_PKT_SUPPORT)
	qdf_spinlock_t rx_mon_wq_lock;
	qdf_workqueue_t *rx_mon_workqueue;
	qdf_work_t rx_mon_work;

	TAILQ_HEAD(, hal_rx_ppdu_info) rx_mon_queue;
	TAILQ_HEAD(, hal_rx_ppdu_info) rx_mon_free_queue;
	qdf_spinlock_t ppdu_info_lock;
	qdf_kmem_cache_t ppdu_info_cache;
#endif
	uint16_t rx_mon_queue_depth;
	uint16_t desc_count;
	struct dp_mon_desc *status[DP_MON_MAX_STATUS_BUF];
#ifdef QCA_SUPPORT_LITE_MONITOR
	struct dp_lite_mon_rx_config *lite_mon_rx_config;
	struct dp_lite_mon_tx_config *lite_mon_tx_config;
#endif
	void *prev_rxmon_desc;
	uint32_t prev_rxmon_cookie;
	void *prev_rxmon_pkt_desc;
	uint32_t prev_rxmon_pkt_cookie;
	uint32_t total_free_elem;
#ifdef MONITOR_TLV_RECORDING_ENABLE
	struct dp_mon_tlv_logger *rx_tlv_log;
	struct dp_mon_tlv_logger *tx_tlv_log;
#endif
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
 * @soc: DP soc handle
 * @mon_desc_pool: mon desc pool
 * @ctx_type: DP context type
 *
 * Return: None
 *
 */
void dp_mon_desc_pool_free(struct dp_soc *soc,
			   struct dp_mon_desc_pool *mon_desc_pool,
			   enum dp_ctxt_type ctx_type);

/**
 * dp_mon_desc_pool_alloc() - Monitor descriptor pool alloc
 * @soc: DP soc handle
 * @ctx_type: DP context type
 * @pool_size: Pool size
 * @mon_desc_pool: mon desc pool
 *
 * Return: non-zero for failure, zero for success
 */
QDF_STATUS dp_mon_desc_pool_alloc(struct dp_soc *soc,
				  enum dp_ctxt_type ctx_type,
				  uint32_t pool_size,
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
	if (!(head && new))
		return;

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

#if !defined(DISABLE_MON_CONFIG) && (defined(WLAN_PKT_CAPTURE_TX_2_0) || \
	defined(WLAN_PKT_CAPTURE_RX_2_0))
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

#ifdef QCA_ENHANCED_STATS_SUPPORT
/*
 * dp_enable_enhanced_stats_2_0() - BE Wrapper to enable stats
 * @soc: Datapath soc handle
 * @pdev_id: Pdev Id on which stats will get enable
 *
 * Return: status success/failure
 */
QDF_STATUS
dp_enable_enhanced_stats_2_0(struct cdp_soc_t *soc, uint8_t pdev_id);

/*
 * dp_disable_enhanced_stats_2_0() - BE Wrapper to disable stats
 * @soc: Datapath soc handle
 * @pdev_id: Pdev Id on which stats will get disable
 *
 * Return: status success/failure
 */
QDF_STATUS
dp_disable_enhanced_stats_2_0(struct cdp_soc_t *soc, uint8_t pdev_id);
#endif /* QCA_ENHANCED_STATS_SUPPORT */

#ifdef WLAN_PKT_CAPTURE_RX_2_0
static inline unsigned long long
dp_mon_get_debug_desc_addr(union dp_mon_desc_list_elem_t **desc_list)
{
	unsigned long long desc;

	desc = (unsigned long)&((*desc_list)->mon_desc);
	desc = (unsigned long long)((unsigned long long)desc & DP_MON_DESC_ADDR_MASK);
	desc = (desc | ((unsigned long long)(*desc_list)->mon_desc.cookie_2 << DP_MON_DESC_ADDR_SHIFT));
	return desc;
}
#else
static inline unsigned long long
dp_mon_get_debug_desc_addr(union dp_mon_desc_list_elem_t **desc_list)
{
	unsigned long long desc = (unsigned long long)&((*desc_list)->mon_desc);
	return desc;
}
#endif
#endif /* _DP_MON_2_0_H_ */

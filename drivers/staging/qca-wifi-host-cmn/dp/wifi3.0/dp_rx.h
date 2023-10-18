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

#ifndef _DP_RX_H
#define _DP_RX_H

#include "hal_rx.h"
#include "dp_peer.h"
#include "dp_internal.h"
#include <qdf_tracepoint.h>
#include "dp_ipa.h"

#ifdef RXDMA_OPTIMIZATION
#ifndef RX_DATA_BUFFER_ALIGNMENT
#define RX_DATA_BUFFER_ALIGNMENT        128
#endif
#ifndef RX_MONITOR_BUFFER_ALIGNMENT
#define RX_MONITOR_BUFFER_ALIGNMENT     128
#endif
#else /* RXDMA_OPTIMIZATION */
#define RX_DATA_BUFFER_ALIGNMENT        4
#define RX_MONITOR_BUFFER_ALIGNMENT     4
#endif /* RXDMA_OPTIMIZATION */

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define DP_WBM2SW_RBM(sw0_bm_id)	HAL_RX_BUF_RBM_SW1_BM(sw0_bm_id)
/* RBM value used for re-injecting defragmented packets into REO */
#define DP_DEFRAG_RBM(sw0_bm_id)	HAL_RX_BUF_RBM_SW3_BM(sw0_bm_id)
#endif

/* Max buffer in invalid peer SG list*/
#define DP_MAX_INVALID_BUFFERS 10
#ifdef DP_INVALID_PEER_ASSERT
#define DP_PDEV_INVALID_PEER_MSDU_CHECK(head, tail) \
		do {                                \
			qdf_assert_always(!(head)); \
			qdf_assert_always(!(tail)); \
		} while (0)
#else
#define DP_PDEV_INVALID_PEER_MSDU_CHECK(head, tail) /* no op */
#endif

#define RX_BUFFER_RESERVATION   0
#ifdef BE_PKTLOG_SUPPORT
#define BUFFER_RESIDUE 1
#define RX_MON_MIN_HEAD_ROOM   64
#endif

#define DP_DEFAULT_NOISEFLOOR	(-96)

#define DP_RX_DESC_MAGIC 0xdec0de

#define dp_rx_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_DP_RX, params)
#define dp_rx_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_DP_RX, params)
#define dp_rx_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_DP_RX, params)
#define dp_rx_info(params...) \
	__QDF_TRACE_FL(QDF_TRACE_LEVEL_INFO_HIGH, QDF_MODULE_ID_DP_RX, ## params)
#define dp_rx_info_rl(params...) \
	__QDF_TRACE_RL(QDF_TRACE_LEVEL_INFO_HIGH, QDF_MODULE_ID_DP_RX, ## params)
#define dp_rx_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_DP_RX, params)

/**
 * enum dp_rx_desc_state
 *
 * @RX_DESC_REPLENISHED: rx desc replenished
 * @RX_DESC_IN_FREELIST: rx desc in freelist
 */
enum dp_rx_desc_state {
	RX_DESC_REPLENISHED,
	RX_DESC_IN_FREELIST,
};

#ifndef QCA_HOST_MODE_WIFI_DISABLED
/**
 * struct dp_rx_desc_dbg_info
 *
 * @freelist_caller: name of the function that put the
 *  the rx desc in freelist
 * @freelist_ts: timestamp when the rx desc is put in
 *  a freelist
 * @replenish_caller: name of the function that last
 *  replenished the rx desc
 * @replenish_ts: last replenish timestamp
 * @prev_nbuf: previous nbuf info
 * @prev_nbuf_data_addr: previous nbuf data address
 */
struct dp_rx_desc_dbg_info {
	char freelist_caller[QDF_MEM_FUNC_NAME_SIZE];
	uint64_t freelist_ts;
	char replenish_caller[QDF_MEM_FUNC_NAME_SIZE];
	uint64_t replenish_ts;
	qdf_nbuf_t prev_nbuf;
	uint8_t *prev_nbuf_data_addr;
};

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * struct dp_rx_desc
 *
 * @nbuf:		VA of the "skb" posted
 * @rx_buf_start:	VA of the original Rx buffer, before
 *			movement of any skb->data pointer
 * @paddr_buf_start:	PA of the original Rx buffer, before
 *                      movement of any frag pointer
 * @cookie:		index into the sw array which holds
 *			the sw Rx descriptors
 *			Cookie space is 21 bits:
 *			lower 18 bits -- index
 *			upper  3 bits -- pool_id
 * @pool_id:		pool Id for which this allocated.
 *			Can only be used if there is no flow
 *			steering
 * @chip_id:		chip_id indicating MLO chip_id
 *			valid or used only in case of multi-chip MLO
 * @magic:
 * @nbuf_data_addr:	VA of nbuf data posted
 * @dbg_info:
 * @in_use:		rx_desc is in use
 * @unmapped:		used to mark rx_desc an unmapped if the corresponding
 *			nbuf is already unmapped
 * @in_err_state:	Nbuf sanity failed for this descriptor.
 */
struct dp_rx_desc {
	qdf_nbuf_t nbuf;
	uint8_t *rx_buf_start;
	qdf_dma_addr_t paddr_buf_start;
	uint32_t cookie;
	uint8_t	 pool_id;
	uint8_t chip_id;
#ifdef RX_DESC_DEBUG_CHECK
	uint32_t magic;
	uint8_t *nbuf_data_addr;
	struct dp_rx_desc_dbg_info *dbg_info;
#endif
	uint8_t	in_use:1,
		unmapped:1,
		in_err_state:1;
};

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef ATH_RX_PRI_SAVE
#define DP_RX_TID_SAVE(_nbuf, _tid) \
	(qdf_nbuf_set_priority(_nbuf, _tid))
#else
#define DP_RX_TID_SAVE(_nbuf, _tid)
#endif

/* RX Descriptor Multi Page memory alloc related */
#define DP_RX_DESC_OFFSET_NUM_BITS 8
#define DP_RX_DESC_PAGE_ID_NUM_BITS 8
#define DP_RX_DESC_POOL_ID_NUM_BITS 4

#define DP_RX_DESC_PAGE_ID_SHIFT DP_RX_DESC_OFFSET_NUM_BITS
#define DP_RX_DESC_POOL_ID_SHIFT \
		(DP_RX_DESC_OFFSET_NUM_BITS + DP_RX_DESC_PAGE_ID_NUM_BITS)
#define RX_DESC_MULTI_PAGE_COOKIE_POOL_ID_MASK \
	(((1 << DP_RX_DESC_POOL_ID_NUM_BITS) - 1) << DP_RX_DESC_POOL_ID_SHIFT)
#define RX_DESC_MULTI_PAGE_COOKIE_PAGE_ID_MASK	\
			(((1 << DP_RX_DESC_PAGE_ID_NUM_BITS) - 1) << \
			 DP_RX_DESC_PAGE_ID_SHIFT)
#define RX_DESC_MULTI_PAGE_COOKIE_OFFSET_MASK \
			((1 << DP_RX_DESC_OFFSET_NUM_BITS) - 1)
#define DP_RX_DESC_MULTI_PAGE_COOKIE_GET_POOL_ID(_cookie)		\
	(((_cookie) & RX_DESC_MULTI_PAGE_COOKIE_POOL_ID_MASK) >>	\
			DP_RX_DESC_POOL_ID_SHIFT)
#define DP_RX_DESC_MULTI_PAGE_COOKIE_GET_PAGE_ID(_cookie)		\
	(((_cookie) & RX_DESC_MULTI_PAGE_COOKIE_PAGE_ID_MASK) >>	\
			DP_RX_DESC_PAGE_ID_SHIFT)
#define DP_RX_DESC_MULTI_PAGE_COOKIE_GET_OFFSET(_cookie)		\
	((_cookie) & RX_DESC_MULTI_PAGE_COOKIE_OFFSET_MASK)

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#define RX_DESC_COOKIE_INDEX_SHIFT		0
#define RX_DESC_COOKIE_INDEX_MASK		0x3ffff /* 18 bits */
#define RX_DESC_COOKIE_POOL_ID_SHIFT		18
#define RX_DESC_COOKIE_POOL_ID_MASK		0x1c0000

#define DP_RX_DESC_COOKIE_MAX	\
	(RX_DESC_COOKIE_INDEX_MASK | RX_DESC_COOKIE_POOL_ID_MASK)

#define DP_RX_DESC_COOKIE_POOL_ID_GET(_cookie)		\
	(((_cookie) & RX_DESC_COOKIE_POOL_ID_MASK) >>	\
			RX_DESC_COOKIE_POOL_ID_SHIFT)

#define DP_RX_DESC_COOKIE_INDEX_GET(_cookie)		\
	(((_cookie) & RX_DESC_COOKIE_INDEX_MASK) >>	\
			RX_DESC_COOKIE_INDEX_SHIFT)

#define dp_rx_add_to_free_desc_list(head, tail, new) \
	__dp_rx_add_to_free_desc_list(head, tail, new, __func__)

#define dp_rx_buffers_replenish(soc, mac_id, rxdma_srng, rx_desc_pool, \
				num_buffers, desc_list, tail, req_only) \
	__dp_rx_buffers_replenish(soc, mac_id, rxdma_srng, rx_desc_pool, \
				  num_buffers, desc_list, tail, req_only, \
				  __func__)

#ifdef WLAN_SUPPORT_RX_FISA
/**
 * dp_rx_set_hdr_pad() - set l3 padding in nbuf cb
 * @nbuf: pkt skb pointer
 * @l3_padding: l3 padding
 *
 * Return: None
 */
static inline
void dp_rx_set_hdr_pad(qdf_nbuf_t nbuf, uint32_t l3_padding)
{
	QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(nbuf) = l3_padding;
}
#else
static inline
void dp_rx_set_hdr_pad(qdf_nbuf_t nbuf, uint32_t l3_padding)
{
}
#endif

#ifdef DP_RX_SPECIAL_FRAME_NEED
/**
 * dp_rx_is_special_frame() - check is RX frame special needed
 *
 * @nbuf: RX skb pointer
 * @frame_mask: the mask for special frame needed
 *
 * Check is RX frame wanted matched with mask
 *
 * Return: true - special frame needed, false - no
 */
static inline
bool dp_rx_is_special_frame(qdf_nbuf_t nbuf, uint32_t frame_mask)
{
	if (((frame_mask & FRAME_MASK_IPV4_ARP) &&
	     qdf_nbuf_is_ipv4_arp_pkt(nbuf)) ||
	    ((frame_mask & FRAME_MASK_IPV4_DHCP) &&
	     qdf_nbuf_is_ipv4_dhcp_pkt(nbuf)) ||
	    ((frame_mask & FRAME_MASK_IPV4_EAPOL) &&
	     qdf_nbuf_is_ipv4_eapol_pkt(nbuf)) ||
	    ((frame_mask & FRAME_MASK_IPV6_DHCP) &&
	     qdf_nbuf_is_ipv6_dhcp_pkt(nbuf)))
		return true;

	return false;
}

/**
 * dp_rx_deliver_special_frame() - Deliver the RX special frame to stack
 *				   if matches mask
 *
 * @soc: Datapath soc handler
 * @peer: pointer to DP peer
 * @nbuf: pointer to the skb of RX frame
 * @frame_mask: the mask for special frame needed
 * @rx_tlv_hdr: start of rx tlv header
 *
 * note: Msdu_len must have been stored in QDF_NBUF_CB_RX_PKT_LEN(nbuf) and
 * single nbuf is expected.
 *
 * Return: true - nbuf has been delivered to stack, false - not.
 */
bool dp_rx_deliver_special_frame(struct dp_soc *soc, struct dp_txrx_peer *peer,
				 qdf_nbuf_t nbuf, uint32_t frame_mask,
				 uint8_t *rx_tlv_hdr);
#else
static inline
bool dp_rx_is_special_frame(qdf_nbuf_t nbuf, uint32_t frame_mask)
{
	return false;
}

static inline
bool dp_rx_deliver_special_frame(struct dp_soc *soc, struct dp_txrx_peer *peer,
				 qdf_nbuf_t nbuf, uint32_t frame_mask,
				 uint8_t *rx_tlv_hdr)
{
	return false;
}
#endif

#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * dp_rx_data_is_specific() - Used to exclude specific frames
 *                            not practical for getting rx
 *                            stats like rate, mcs, nss, etc.
 *
 * @hal_soc_hdl: soc handler
 * @rx_tlv_hdr: rx tlv header
 * @nbuf: RX skb pointer
 *
 * Return: true - a specific frame  not suitable
 *                for getting rx stats from it.
 *         false - a common frame suitable for
 *                 getting rx stats from it.
 */
static inline
bool dp_rx_data_is_specific(hal_soc_handle_t hal_soc_hdl,
			    uint8_t *rx_tlv_hdr,
			    qdf_nbuf_t nbuf)
{
	if (qdf_unlikely(qdf_nbuf_is_da_mcbc(nbuf)))
		return true;

	if (!hal_rx_tlv_first_mpdu_get(hal_soc_hdl, rx_tlv_hdr))
		return true;

	if (!hal_rx_msdu_end_first_msdu_get(hal_soc_hdl, rx_tlv_hdr))
		return true;

	/* ARP, EAPOL is neither IPV6 ETH nor IPV4 ETH from L3 level */
	if (qdf_likely(hal_rx_tlv_l3_type_get(hal_soc_hdl, rx_tlv_hdr) ==
	    QDF_NBUF_TRAC_IPV4_ETH_TYPE)) {
		if (qdf_nbuf_is_ipv4_dhcp_pkt(nbuf))
			return true;
	} else if (qdf_likely(hal_rx_tlv_l3_type_get(hal_soc_hdl, rx_tlv_hdr) ==
		   QDF_NBUF_TRAC_IPV6_ETH_TYPE)) {
		if (qdf_nbuf_is_ipv6_dhcp_pkt(nbuf))
			return true;
	} else {
		return true;
	}
	return false;
}
#else
static inline
bool dp_rx_data_is_specific(hal_soc_handle_t hal_soc_hdl,
			    uint8_t *rx_tlv_hdr,
			    qdf_nbuf_t nbuf)

{
	/*
	 * default return is true to make sure that rx stats
	 * will not be handled when this feature is disabled
	 */
	return true;
}
#endif /* FEATURE_RX_LINKSPEED_ROAM_TRIGGER */

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef DP_RX_DISABLE_NDI_MDNS_FORWARDING
static inline
bool dp_rx_check_ndi_mdns_fwding(struct dp_txrx_peer *ta_txrx_peer,
				 qdf_nbuf_t nbuf)
{
	if (ta_txrx_peer->vdev->opmode == wlan_op_mode_ndi &&
	    qdf_nbuf_is_ipv6_mdns_pkt(nbuf)) {
		DP_PEER_PER_PKT_STATS_INC(ta_txrx_peer,
					  rx.intra_bss.mdns_no_fwd, 1);
		return false;
	}
	return true;
}
#else
static inline
bool dp_rx_check_ndi_mdns_fwding(struct dp_txrx_peer *ta_txrx_peer,
				 qdf_nbuf_t nbuf)
{
	return true;
}
#endif
#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/* DOC: Offset to obtain LLC hdr
 *
 * In the case of Wifi parse error
 * to reach LLC header from beginning
 * of VLAN tag we need to skip 8 bytes.
 * Vlan_tag(4)+length(2)+length added
 * by HW(2) = 8 bytes.
 */
#define DP_SKIP_VLAN		8

#ifndef QCA_HOST_MODE_WIFI_DISABLED

/**
 * struct dp_rx_cached_buf - rx cached buffer
 * @node: linked list node
 * @buf: skb buffer
 */
struct dp_rx_cached_buf {
	qdf_list_node_t node;
	qdf_nbuf_t buf;
};

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_rx_xor_block() - xor block of data
 * @b: destination data block
 * @a: source data block
 * @len: length of the data to process
 *
 * Return: None
 */
static inline void dp_rx_xor_block(uint8_t *b, const uint8_t *a, qdf_size_t len)
{
	qdf_size_t i;

	for (i = 0; i < len; i++)
		b[i] ^= a[i];
}

/**
 * dp_rx_rotl() - rotate the bits left
 * @val: unsigned integer input value
 * @bits: number of bits
 *
 * Return: Integer with left rotated by number of 'bits'
 */
static inline uint32_t dp_rx_rotl(uint32_t val, int bits)
{
	return (val << bits) | (val >> (32 - bits));
}

/**
 * dp_rx_rotr() - rotate the bits right
 * @val: unsigned integer input value
 * @bits: number of bits
 *
 * Return: Integer with right rotated by number of 'bits'
 */
static inline uint32_t dp_rx_rotr(uint32_t val, int bits)
{
	return (val >> bits) | (val << (32 - bits));
}

/**
 * dp_set_rx_queue() - set queue_mapping in skb
 * @nbuf: skb
 * @queue_id: rx queue_id
 *
 * Return: void
 */
#ifdef QCA_OL_RX_MULTIQ_SUPPORT
static inline void dp_set_rx_queue(qdf_nbuf_t nbuf, uint8_t queue_id)
{
	qdf_nbuf_record_rx_queue(nbuf, queue_id);
	return;
}
#else
static inline void dp_set_rx_queue(qdf_nbuf_t nbuf, uint8_t queue_id)
{
}
#endif

/**
 * dp_rx_xswap() - swap the bits left
 * @val: unsigned integer input value
 *
 * Return: Integer with bits swapped
 */
static inline uint32_t dp_rx_xswap(uint32_t val)
{
	return ((val & 0x00ff00ff) << 8) | ((val & 0xff00ff00) >> 8);
}

/**
 * dp_rx_get_le32_split() - get little endian 32 bits split
 * @b0: byte 0
 * @b1: byte 1
 * @b2: byte 2
 * @b3: byte 3
 *
 * Return: Integer with split little endian 32 bits
 */
static inline uint32_t dp_rx_get_le32_split(uint8_t b0, uint8_t b1, uint8_t b2,
					uint8_t b3)
{
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

/**
 * dp_rx_get_le32() - get little endian 32 bits
 * @p: source 32-bit value
 *
 * Return: Integer with little endian 32 bits
 */
static inline uint32_t dp_rx_get_le32(const uint8_t *p)
{
	return dp_rx_get_le32_split(p[0], p[1], p[2], p[3]);
}

/**
 * dp_rx_put_le32() - put little endian 32 bits
 * @p: destination char array
 * @v: source 32-bit integer
 *
 * Return: None
 */
static inline void dp_rx_put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (v) & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}

/* Extract michal mic block of data */
#define dp_rx_michael_block(l, r)	\
	do {					\
		r ^= dp_rx_rotl(l, 17);	\
		l += r;				\
		r ^= dp_rx_xswap(l);		\
		l += r;				\
		r ^= dp_rx_rotl(l, 3);	\
		l += r;				\
		r ^= dp_rx_rotr(l, 2);	\
		l += r;				\
	} while (0)

/**
 * struct dp_rx_desc_list_elem_t
 *
 * @next: Next pointer to form free list
 * @rx_desc: DP Rx descriptor
 */
union dp_rx_desc_list_elem_t {
	union dp_rx_desc_list_elem_t *next;
	struct dp_rx_desc rx_desc;
};

#ifdef RX_DESC_MULTI_PAGE_ALLOC
/**
 * dp_rx_desc_find() - find dp rx descriptor from page ID and offset
 * @page_id: Page ID
 * @offset: Offset of the descriptor element
 * @rx_pool: RX pool
 *
 * Return: RX descriptor element
 */
union dp_rx_desc_list_elem_t *dp_rx_desc_find(uint16_t page_id, uint16_t offset,
					      struct rx_desc_pool *rx_pool);

static inline
struct dp_rx_desc *dp_get_rx_desc_from_cookie(struct dp_soc *soc,
					      struct rx_desc_pool *pool,
					      uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_POOL_ID(cookie);
	uint16_t page_id = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_PAGE_ID(cookie);
	uint8_t offset = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_OFFSET(cookie);
	struct rx_desc_pool *rx_desc_pool;
	union dp_rx_desc_list_elem_t *rx_desc_elem;

	if (qdf_unlikely(pool_id >= MAX_PDEV_CNT))
		return NULL;

	rx_desc_pool = &pool[pool_id];
	rx_desc_elem = (union dp_rx_desc_list_elem_t *)
		(rx_desc_pool->desc_pages.cacheable_pages[page_id] +
		rx_desc_pool->elem_size * offset);

	return &rx_desc_elem->rx_desc;
}

static inline
struct dp_rx_desc *dp_get_rx_mon_status_desc_from_cookie(struct dp_soc *soc,
							 struct rx_desc_pool *pool,
							 uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_POOL_ID(cookie);
	uint16_t page_id = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_PAGE_ID(cookie);
	uint8_t offset = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_OFFSET(cookie);
	struct rx_desc_pool *rx_desc_pool;
	union dp_rx_desc_list_elem_t *rx_desc_elem;

	if (qdf_unlikely(pool_id >= NUM_RXDMA_RINGS_PER_PDEV))
		return NULL;

	rx_desc_pool = &pool[pool_id];
	rx_desc_elem = (union dp_rx_desc_list_elem_t *)
		(rx_desc_pool->desc_pages.cacheable_pages[page_id] +
		rx_desc_pool->elem_size * offset);

	return &rx_desc_elem->rx_desc;
}

/**
 * dp_rx_cookie_2_va_rxdma_buf() - Converts cookie to a virtual address of
 *			 the Rx descriptor on Rx DMA source ring buffer
 * @soc: core txrx main context
 * @cookie: cookie used to lookup virtual address
 *
 * Return: Pointer to the Rx descriptor
 */
static inline
struct dp_rx_desc *dp_rx_cookie_2_va_rxdma_buf(struct dp_soc *soc,
					       uint32_t cookie)
{
	return dp_get_rx_desc_from_cookie(soc, &soc->rx_desc_buf[0], cookie);
}

/**
 * dp_rx_cookie_2_va_mon_buf() - Converts cookie to a virtual address of
 *			 the Rx descriptor on monitor ring buffer
 * @soc: core txrx main context
 * @cookie: cookie used to lookup virtual address
 *
 * Return: Pointer to the Rx descriptor
 */
static inline
struct dp_rx_desc *dp_rx_cookie_2_va_mon_buf(struct dp_soc *soc,
					     uint32_t cookie)
{
	return dp_get_rx_desc_from_cookie(soc, &soc->rx_desc_mon[0], cookie);
}

/**
 * dp_rx_cookie_2_va_mon_status() - Converts cookie to a virtual address of
 *			 the Rx descriptor on monitor status ring buffer
 * @soc: core txrx main context
 * @cookie: cookie used to lookup virtual address
 *
 * Return: Pointer to the Rx descriptor
 */
static inline
struct dp_rx_desc *dp_rx_cookie_2_va_mon_status(struct dp_soc *soc,
						uint32_t cookie)
{
	return dp_get_rx_mon_status_desc_from_cookie(soc,
						     &soc->rx_desc_status[0],
						     cookie);
}
#else

void dp_rx_desc_pool_init(struct dp_soc *soc, uint32_t pool_id,
			  uint32_t pool_size,
			  struct rx_desc_pool *rx_desc_pool);

/**
 * dp_rx_cookie_2_va_rxdma_buf() - Converts cookie to a virtual address of
 *			 the Rx descriptor on Rx DMA source ring buffer
 * @soc: core txrx main context
 * @cookie: cookie used to lookup virtual address
 *
 * Return: void *: Virtual Address of the Rx descriptor
 */
static inline
void *dp_rx_cookie_2_va_rxdma_buf(struct dp_soc *soc, uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_COOKIE_POOL_ID_GET(cookie);
	uint16_t index = DP_RX_DESC_COOKIE_INDEX_GET(cookie);
	struct rx_desc_pool *rx_desc_pool;

	if (qdf_unlikely(pool_id >= MAX_RXDESC_POOLS))
		return NULL;

	rx_desc_pool = &soc->rx_desc_buf[pool_id];

	if (qdf_unlikely(index >= rx_desc_pool->pool_size))
		return NULL;

	return &rx_desc_pool->array[index].rx_desc;
}

/**
 * dp_rx_cookie_2_va_mon_buf() - Converts cookie to a virtual address of
 *			 the Rx descriptor on monitor ring buffer
 * @soc: core txrx main context
 * @cookie: cookie used to lookup virtual address
 *
 * Return: void *: Virtual Address of the Rx descriptor
 */
static inline
void *dp_rx_cookie_2_va_mon_buf(struct dp_soc *soc, uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_COOKIE_POOL_ID_GET(cookie);
	uint16_t index = DP_RX_DESC_COOKIE_INDEX_GET(cookie);
	/* TODO */
	/* Add sanity for pool_id & index */
	return &(soc->rx_desc_mon[pool_id].array[index].rx_desc);
}

/**
 * dp_rx_cookie_2_va_mon_status() - Converts cookie to a virtual address of
 *			 the Rx descriptor on monitor status ring buffer
 * @soc: core txrx main context
 * @cookie: cookie used to lookup virtual address
 *
 * Return: void *: Virtual Address of the Rx descriptor
 */
static inline
void *dp_rx_cookie_2_va_mon_status(struct dp_soc *soc, uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_COOKIE_POOL_ID_GET(cookie);
	uint16_t index = DP_RX_DESC_COOKIE_INDEX_GET(cookie);
	/* TODO */
	/* Add sanity for pool_id & index */
	return &(soc->rx_desc_status[pool_id].array[index].rx_desc);
}
#endif /* RX_DESC_MULTI_PAGE_ALLOC */

#ifndef QCA_HOST_MODE_WIFI_DISABLED

static inline bool dp_rx_check_ap_bridge(struct dp_vdev *vdev)
{
	return vdev->ap_bridge_enabled;
}

#ifdef DP_RX_DESC_COOKIE_INVALIDATE
static inline QDF_STATUS
dp_rx_cookie_check_and_invalidate(hal_ring_desc_t ring_desc)
{
	if (qdf_unlikely(HAL_RX_REO_BUF_COOKIE_INVALID_GET(ring_desc)))
		return QDF_STATUS_E_FAILURE;

	HAL_RX_REO_BUF_COOKIE_INVALID_SET(ring_desc);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_cookie_reset_invalid_bit() - Reset the invalid bit of the cookie
 *  field in ring descriptor
 * @ring_desc: ring descriptor
 *
 * Return: None
 */
static inline void
dp_rx_cookie_reset_invalid_bit(hal_ring_desc_t ring_desc)
{
	HAL_RX_REO_BUF_COOKIE_INVALID_RESET(ring_desc);
}
#else
static inline QDF_STATUS
dp_rx_cookie_check_and_invalidate(hal_ring_desc_t ring_desc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_rx_cookie_reset_invalid_bit(hal_ring_desc_t ring_desc)
{
}
#endif

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#if defined(RX_DESC_MULTI_PAGE_ALLOC) && \
	defined(DP_WAR_VALIDATE_RX_ERR_MSDU_COOKIE)
/**
 * dp_rx_is_sw_cookie_valid() - check whether SW cookie valid
 * @soc: dp soc ref
 * @cookie: Rx buf SW cookie value
 *
 * Return: true if cookie is valid else false
 */
static inline bool dp_rx_is_sw_cookie_valid(struct dp_soc *soc,
					    uint32_t cookie)
{
	uint8_t pool_id = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_POOL_ID(cookie);
	uint16_t page_id = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_PAGE_ID(cookie);
	uint8_t offset = DP_RX_DESC_MULTI_PAGE_COOKIE_GET_OFFSET(cookie);
	struct rx_desc_pool *rx_desc_pool;

	if (qdf_unlikely(pool_id >= MAX_PDEV_CNT))
		goto fail;

	rx_desc_pool = &soc->rx_desc_buf[pool_id];

	if (page_id >= rx_desc_pool->desc_pages.num_pages ||
	    offset >= rx_desc_pool->desc_pages.num_element_per_page)
		goto fail;

	return true;

fail:
	DP_STATS_INC(soc, rx.err.invalid_cookie, 1);
	return false;
}
#else
/**
 * dp_rx_is_sw_cookie_valid() - check whether SW cookie valid
 * @soc: dp soc ref
 * @cookie: Rx buf SW cookie value
 *
 * When multi page alloc is disabled SW cookie validness is
 * checked while fetching Rx descriptor, so no need to check here
 *
 * Return: true if cookie is valid else false
 */
static inline bool dp_rx_is_sw_cookie_valid(struct dp_soc *soc,
					    uint32_t cookie)
{
	return true;
}
#endif

/**
 * dp_rx_desc_pool_is_allocated() - check if memory is allocated for the
 *					rx descriptor pool
 * @rx_desc_pool: rx descriptor pool pointer
 *
 * Return: QDF_STATUS  QDF_STATUS_SUCCESS
 *		       QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_rx_desc_pool_is_allocated(struct rx_desc_pool *rx_desc_pool);

/**
 * dp_rx_desc_pool_alloc() - Allocate a memory pool for software rx
 *			     descriptors
 * @soc: core txrx main context
 * @pool_size: number of rx descriptors (size of the pool)
 * @rx_desc_pool: rx descriptor pool pointer
 *
 * Return: QDF_STATUS  QDF_STATUS_SUCCESS
 *		       QDF_STATUS_E_NOMEM
 *		       QDF_STATUS_E_FAULT
 */
QDF_STATUS dp_rx_desc_pool_alloc(struct dp_soc *soc,
				 uint32_t pool_size,
				 struct rx_desc_pool *rx_desc_pool);

/**
 * dp_rx_desc_pool_init() - Initialize the software RX descriptor pool
 * @soc: core txrx main context
 * @pool_id: pool_id which is one of 3 mac_ids
 * @pool_size: size of the rx descriptor pool
 * @rx_desc_pool: rx descriptor pool pointer
 *
 * Convert the pool of memory into a list of rx descriptors and create
 * locks to access this list of rx descriptors.
 *
 */
void dp_rx_desc_pool_init(struct dp_soc *soc, uint32_t pool_id,
			  uint32_t pool_size,
			  struct rx_desc_pool *rx_desc_pool);

/**
 * dp_rx_add_desc_list_to_free_list() - append unused desc_list back to
 *					freelist.
 * @soc: core txrx main context
 * @local_desc_list: local desc list provided by the caller
 * @tail: attach the point to last desc of local desc list
 * @pool_id: pool_id which is one of 3 mac_ids
 * @rx_desc_pool: rx descriptor pool pointer
 */
void dp_rx_add_desc_list_to_free_list(struct dp_soc *soc,
				union dp_rx_desc_list_elem_t **local_desc_list,
				union dp_rx_desc_list_elem_t **tail,
				uint16_t pool_id,
				struct rx_desc_pool *rx_desc_pool);

/**
 * dp_rx_get_free_desc_list() - provide a list of descriptors from
 *				the free rx desc pool.
 * @soc: core txrx main context
 * @pool_id: pool_id which is one of 3 mac_ids
 * @rx_desc_pool: rx descriptor pool pointer
 * @num_descs: number of descs requested from freelist
 * @desc_list: attach the descs to this list (output parameter)
 * @tail: attach the point to last desc of free list (output parameter)
 *
 * Return: number of descs allocated from free list.
 */
uint16_t dp_rx_get_free_desc_list(struct dp_soc *soc, uint32_t pool_id,
				struct rx_desc_pool *rx_desc_pool,
				uint16_t num_descs,
				union dp_rx_desc_list_elem_t **desc_list,
				union dp_rx_desc_list_elem_t **tail);

/**
 * dp_rx_pdev_desc_pool_alloc() -  allocate memory for software rx descriptor
 *				   pool
 * @pdev: core txrx pdev context
 *
 * Return: QDF_STATUS - QDF_STATUS_SUCCESS
 *			QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_rx_pdev_desc_pool_alloc(struct dp_pdev *pdev);

/**
 * dp_rx_pdev_desc_pool_free() - free software rx descriptor pool
 * @pdev: core txrx pdev context
 */
void dp_rx_pdev_desc_pool_free(struct dp_pdev *pdev);

/**
 * dp_rx_pdev_desc_pool_init() - initialize software rx descriptors
 * @pdev: core txrx pdev context
 *
 * Return: QDF_STATUS - QDF_STATUS_SUCCESS
 *			QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_rx_pdev_desc_pool_init(struct dp_pdev *pdev);

/**
 * dp_rx_pdev_desc_pool_deinit() - de-initialize software rx descriptor pools
 * @pdev: core txrx pdev context
 *
 * This function resets the freelist of rx descriptors and destroys locks
 * associated with this list of descriptors.
 */
void dp_rx_pdev_desc_pool_deinit(struct dp_pdev *pdev);

void dp_rx_desc_pool_deinit(struct dp_soc *soc,
			    struct rx_desc_pool *rx_desc_pool,
			    uint32_t pool_id);

QDF_STATUS dp_rx_pdev_attach(struct dp_pdev *pdev);

/**
 * dp_rx_pdev_buffers_alloc() - Allocate nbufs (skbs) and replenish RxDMA ring
 * @pdev: core txrx pdev context
 *
 * Return: QDF_STATUS - QDF_STATUS_SUCCESS
 *			QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_rx_pdev_buffers_alloc(struct dp_pdev *pdev);

/**
 * dp_rx_pdev_buffers_free() - Free nbufs (skbs)
 * @pdev: core txrx pdev context
 */
void dp_rx_pdev_buffers_free(struct dp_pdev *pdev);

void dp_rx_pdev_detach(struct dp_pdev *pdev);

/**
 * dp_print_napi_stats() - NAPI stats
 * @soc: soc handle
 */
void dp_print_napi_stats(struct dp_soc *soc);

/**
 * dp_rx_vdev_detach() - detach vdev from dp rx
 * @vdev: virtual device instance
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_RESOURCES: Error return
 */
QDF_STATUS dp_rx_vdev_detach(struct dp_vdev *vdev);

#ifndef QCA_HOST_MODE_WIFI_DISABLED

uint32_t
dp_rx_process(struct dp_intr *int_ctx, hal_ring_handle_t hal_ring_hdl,
	      uint8_t reo_ring_num,
	      uint32_t quota);

/**
 * dp_rx_err_process() - Processes error frames routed to REO error ring
 * @int_ctx: pointer to DP interrupt context
 * @soc: core txrx main context
 * @hal_ring_hdl: opaque pointer to the HAL Rx Error Ring, which will be serviced
 * @quota: No. of units (packets) that can be serviced in one shot.
 *
 * This function implements error processing and top level demultiplexer
 * for all the frames routed to REO error ring.
 *
 * Return: uint32_t: No. of elements processed
 */
uint32_t dp_rx_err_process(struct dp_intr *int_ctx, struct dp_soc *soc,
			   hal_ring_handle_t hal_ring_hdl, uint32_t quota);

/**
 * dp_rx_wbm_err_process() - Processes error frames routed to WBM release ring
 * @int_ctx: pointer to DP interrupt context
 * @soc: core txrx main context
 * @hal_ring_hdl: opaque pointer to the HAL Rx Error Ring, which will be
 *                serviced
 * @quota: No. of units (packets) that can be serviced in one shot.
 *
 * This function implements error processing and top level demultiplexer
 * for all the frames routed to WBM2HOST sw release ring.
 *
 * Return: uint32_t: No. of elements processed
 */
uint32_t
dp_rx_wbm_err_process(struct dp_intr *int_ctx, struct dp_soc *soc,
		      hal_ring_handle_t hal_ring_hdl, uint32_t quota);

/**
 * dp_rx_sg_create() - create a frag_list for MSDUs which are spread across
 *		     multiple nbufs.
 * @soc: core txrx main context
 * @nbuf: pointer to the first msdu of an amsdu.
 *
 * This function implements the creation of RX frag_list for cases
 * where an MSDU is spread across multiple nbufs.
 *
 * Return: returns the head nbuf which contains complete frag_list.
 */
qdf_nbuf_t dp_rx_sg_create(struct dp_soc *soc, qdf_nbuf_t nbuf);

/**
 * dp_rx_is_sg_supported() - SG packets processing supported or not.
 *
 * Return: returns true when processing is supported else false.
 */
bool dp_rx_is_sg_supported(void);

/**
 * dp_rx_desc_nbuf_and_pool_free() - free the sw rx desc pool called during
 *				     de-initialization of wifi module.
 *
 * @soc: core txrx main context
 * @pool_id: pool_id which is one of 3 mac_ids
 * @rx_desc_pool: rx descriptor pool pointer
 *
 * Return: None
 */
void dp_rx_desc_nbuf_and_pool_free(struct dp_soc *soc, uint32_t pool_id,
				   struct rx_desc_pool *rx_desc_pool);

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_rx_desc_nbuf_free() - free the sw rx desc nbufs called during
 *			    de-initialization of wifi module.
 *
 * @soc: core txrx main context
 * @rx_desc_pool: rx descriptor pool pointer
 * @is_mon_pool: true if this is a monitor pool
 *
 * Return: None
 */
void dp_rx_desc_nbuf_free(struct dp_soc *soc,
			  struct rx_desc_pool *rx_desc_pool,
			  bool is_mon_pool);

#ifdef DP_RX_MON_MEM_FRAG
/**
 * dp_rx_desc_frag_free() - free the sw rx desc frag called during
 *			    de-initialization of wifi module.
 *
 * @soc: core txrx main context
 * @rx_desc_pool: rx descriptor pool pointer
 *
 * Return: None
 */
void dp_rx_desc_frag_free(struct dp_soc *soc,
			  struct rx_desc_pool *rx_desc_pool);
#else
static inline
void dp_rx_desc_frag_free(struct dp_soc *soc,
			  struct rx_desc_pool *rx_desc_pool)
{
}
#endif
/**
 * dp_rx_desc_pool_free() - free the sw rx desc array called during
 *			    de-initialization of wifi module.
 *
 * @soc: core txrx main context
 * @rx_desc_pool: rx descriptor pool pointer
 *
 * Return: None
 */
void dp_rx_desc_pool_free(struct dp_soc *soc,
			  struct rx_desc_pool *rx_desc_pool);

/**
 * dp_rx_deliver_raw() - process RAW mode pkts and hand over the
 *				pkts to RAW mode simulation to
 *				decapsulate the pkt.
 * @vdev: vdev on which RAW mode is enabled
 * @nbuf_list: list of RAW pkts to process
 * @txrx_peer: peer object from which the pkt is rx
 *
 * Return: void
 */
void dp_rx_deliver_raw(struct dp_vdev *vdev, qdf_nbuf_t nbuf_list,
				struct dp_txrx_peer *txrx_peer);

#ifdef RX_DESC_LOGGING
/**
 * dp_rx_desc_alloc_dbg_info() - Alloc memory for rx descriptor debug
 *  structure
 * @rx_desc: rx descriptor pointer
 *
 * Return: None
 */
static inline
void dp_rx_desc_alloc_dbg_info(struct dp_rx_desc *rx_desc)
{
	rx_desc->dbg_info = qdf_mem_malloc(sizeof(struct dp_rx_desc_dbg_info));
}

/**
 * dp_rx_desc_free_dbg_info() - Free rx descriptor debug
 *  structure memory
 * @rx_desc: rx descriptor pointer
 *
 * Return: None
 */
static inline
void dp_rx_desc_free_dbg_info(struct dp_rx_desc *rx_desc)
{
	qdf_mem_free(rx_desc->dbg_info);
}

/**
 * dp_rx_desc_update_dbg_info() - Update rx descriptor debug info
 *  structure memory
 * @rx_desc: rx descriptor pointer
 * @func_name: name of calling function
 * @flag:
 *
 * Return: None
 */
static
void dp_rx_desc_update_dbg_info(struct dp_rx_desc *rx_desc,
				const char *func_name, uint8_t flag)
{
	struct dp_rx_desc_dbg_info *info = rx_desc->dbg_info;

	if (!info)
		return;

	if (flag == RX_DESC_REPLENISHED) {
		qdf_str_lcopy(info->replenish_caller, func_name,
			      QDF_MEM_FUNC_NAME_SIZE);
		info->replenish_ts = qdf_get_log_timestamp();
	} else {
		qdf_str_lcopy(info->freelist_caller, func_name,
			      QDF_MEM_FUNC_NAME_SIZE);
		info->freelist_ts = qdf_get_log_timestamp();
		info->prev_nbuf = rx_desc->nbuf;
		info->prev_nbuf_data_addr = rx_desc->nbuf_data_addr;
		rx_desc->nbuf_data_addr = NULL;
	}
}
#else

static inline
void dp_rx_desc_alloc_dbg_info(struct dp_rx_desc *rx_desc)
{
}

static inline
void dp_rx_desc_free_dbg_info(struct dp_rx_desc *rx_desc)
{
}

static inline
void dp_rx_desc_update_dbg_info(struct dp_rx_desc *rx_desc,
				const char *func_name, uint8_t flag)
{
}
#endif /* RX_DESC_LOGGING */

/**
 * __dp_rx_add_to_free_desc_list() - Adds to a local free descriptor list
 *
 * @head: pointer to the head of local free list
 * @tail: pointer to the tail of local free list
 * @new: new descriptor that is added to the free list
 * @func_name: caller func name
 *
 * Return: void:
 */
static inline
void __dp_rx_add_to_free_desc_list(union dp_rx_desc_list_elem_t **head,
				 union dp_rx_desc_list_elem_t **tail,
				 struct dp_rx_desc *new, const char *func_name)
{
	qdf_assert(head && new);

	dp_rx_desc_update_dbg_info(new, func_name, RX_DESC_IN_FREELIST);

	new->nbuf = NULL;
	new->in_use = 0;

	((union dp_rx_desc_list_elem_t *)new)->next = *head;
	*head = (union dp_rx_desc_list_elem_t *)new;
	/* reset tail if head->next is NULL */
	if (!*tail || !(*head)->next)
		*tail = *head;
}

/**
 * dp_rx_process_invalid_peer(): Function to pass invalid peer list to umac
 * @soc: DP SOC handle
 * @nbuf: network buffer
 * @mac_id: mac_id which is one of 3 mac_ids(Assuming mac_id and
 * pool_id has same mapping)
 *
 * Return: integer type
 */
uint8_t dp_rx_process_invalid_peer(struct dp_soc *soc, qdf_nbuf_t nbuf,
				   uint8_t mac_id);

/**
 * dp_rx_process_invalid_peer_wrapper(): Function to wrap invalid peer handler
 * @soc: DP SOC handle
 * @mpdu: mpdu for which peer is invalid
 * @mpdu_done: if an mpdu is completed
 * @mac_id: mac_id which is one of 3 mac_ids(Assuming mac_id and
 * pool_id has same mapping)
 *
 * Return: integer type
 */
void dp_rx_process_invalid_peer_wrapper(struct dp_soc *soc,
		qdf_nbuf_t mpdu, bool mpdu_done, uint8_t mac_id);

/**
 * dp_rx_process_mic_error(): Function to pass mic error indication to umac
 * @soc: core DP main context
 * @nbuf: buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @txrx_peer: txrx peer handle
 *
 * Return: void
 */
void dp_rx_process_mic_error(struct dp_soc *soc, qdf_nbuf_t nbuf,
			     uint8_t *rx_tlv_hdr,
			     struct dp_txrx_peer *txrx_peer);

/**
 * dp_2k_jump_handle() - Function to handle 2k jump exception
 *                        on WBM ring
 * @soc: core DP main context
 * @nbuf: buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @peer_id: peer id of first msdu
 * @tid: Tid for which exception occurred
 *
 * This function handles 2k jump violations arising out
 * of receiving aggregates in non BA case. This typically
 * may happen if aggregates are received on a QOS enabled TID
 * while Rx window size is still initialized to value of 2. Or
 * it may also happen if negotiated window size is 1 but peer
 * sends aggregates.
 */
void dp_2k_jump_handle(struct dp_soc *soc, qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr,
		       uint16_t peer_id, uint8_t tid);

#define DP_RX_HEAD_APPEND(head, elem) \
	do {                                                            \
		qdf_nbuf_set_next((elem), (head));			\
		(head) = (elem);                                        \
	} while (0)


#define DP_RX_LIST_APPEND(head, tail, elem) \
	do {                                                          \
		if (!(head)) {                                        \
			(head) = (elem);                              \
			QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head) = 1;\
		} else {                                              \
			qdf_nbuf_set_next((tail), (elem));            \
			QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head)++;  \
		}                                                     \
		(tail) = (elem);                                      \
		qdf_nbuf_set_next((tail), NULL);                      \
	} while (0)

#define DP_RX_MERGE_TWO_LIST(phead, ptail, chead, ctail) \
	do {                                                          \
		if (!(phead)) {                                       \
			(phead) = (chead);                            \
		} else {                                              \
			qdf_nbuf_set_next((ptail), (chead));          \
			QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(phead) += \
			QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(chead);   \
		}                                                     \
		(ptail) = (ctail);                                    \
		qdf_nbuf_set_next((ptail), NULL);                     \
	} while (0)

#if defined(QCA_PADDR_CHECK_ON_3TH_PLATFORM)
/*
 * on some third-party platform, the memory below 0x2000
 * is reserved for target use, so any memory allocated in this
 * region should not be used by host
 */
#define MAX_RETRY 50
#define DP_PHY_ADDR_RESERVED	0x2000
#elif defined(BUILD_X86)
/*
 * in M2M emulation platforms (x86) the memory below 0x50000000
 * is reserved for target use, so any memory allocated in this
 * region should not be used by host
 */
#define MAX_RETRY 100
#define DP_PHY_ADDR_RESERVED	0x50000000
#endif

#if defined(QCA_PADDR_CHECK_ON_3TH_PLATFORM) || defined(BUILD_X86)
/**
 * dp_check_paddr() - check if current phy address is valid or not
 * @dp_soc: core txrx main context
 * @rx_netbuf: skb buffer
 * @paddr: physical address
 * @rx_desc_pool: struct of rx descriptor pool
 * check if the physical address of the nbuf->data is less
 * than DP_PHY_ADDR_RESERVED then free the nbuf and try
 * allocating new nbuf. We can try for 100 times.
 *
 * This is a temp WAR till we fix it properly.
 *
 * Return: success or failure.
 */
static inline
int dp_check_paddr(struct dp_soc *dp_soc,
		   qdf_nbuf_t *rx_netbuf,
		   qdf_dma_addr_t *paddr,
		   struct rx_desc_pool *rx_desc_pool)
{
	uint32_t nbuf_retry = 0;
	int32_t ret;

	if (qdf_likely(*paddr > DP_PHY_ADDR_RESERVED))
		return QDF_STATUS_SUCCESS;

	do {
		dp_debug("invalid phy addr 0x%llx, trying again",
			 (uint64_t)(*paddr));
		nbuf_retry++;
		if ((*rx_netbuf)) {
			/* Not freeing buffer intentionally.
			 * Observed that same buffer is getting
			 * re-allocated resulting in longer load time
			 * WMI init timeout.
			 * This buffer is anyway not useful so skip it.
			 *.Add such buffer to invalid list and free
			 *.them when driver unload.
			 **/
			qdf_nbuf_unmap_nbytes_single(dp_soc->osdev,
						     *rx_netbuf,
						     QDF_DMA_FROM_DEVICE,
						     rx_desc_pool->buf_size);
			qdf_nbuf_queue_add(&dp_soc->invalid_buf_queue,
					   *rx_netbuf);
		}

		*rx_netbuf = qdf_nbuf_alloc(dp_soc->osdev,
					    rx_desc_pool->buf_size,
					    RX_BUFFER_RESERVATION,
					    rx_desc_pool->buf_alignment,
					    FALSE);

		if (qdf_unlikely(!(*rx_netbuf)))
			return QDF_STATUS_E_FAILURE;

		ret = qdf_nbuf_map_nbytes_single(dp_soc->osdev,
						 *rx_netbuf,
						 QDF_DMA_FROM_DEVICE,
						 rx_desc_pool->buf_size);

		if (qdf_unlikely(ret == QDF_STATUS_E_FAILURE)) {
			qdf_nbuf_free(*rx_netbuf);
			*rx_netbuf = NULL;
			continue;
		}

		*paddr = qdf_nbuf_get_frag_paddr(*rx_netbuf, 0);

		if (qdf_likely(*paddr > DP_PHY_ADDR_RESERVED))
			return QDF_STATUS_SUCCESS;

	} while (nbuf_retry < MAX_RETRY);

	if ((*rx_netbuf)) {
		qdf_nbuf_unmap_nbytes_single(dp_soc->osdev,
					     *rx_netbuf,
					     QDF_DMA_FROM_DEVICE,
					     rx_desc_pool->buf_size);
		qdf_nbuf_queue_add(&dp_soc->invalid_buf_queue,
				   *rx_netbuf);
	}

	return QDF_STATUS_E_FAILURE;
}

#else
static inline
int dp_check_paddr(struct dp_soc *dp_soc,
		   qdf_nbuf_t *rx_netbuf,
		   qdf_dma_addr_t *paddr,
		   struct rx_desc_pool *rx_desc_pool)
{
	return QDF_STATUS_SUCCESS;
}

#endif

/**
 * dp_rx_cookie_2_link_desc_va() - Converts cookie to a virtual address of
 *				   the MSDU Link Descriptor
 * @soc: core txrx main context
 * @buf_info: buf_info includes cookie that is used to lookup
 * virtual address of link descriptor after deriving the page id
 * and the offset or index of the desc on the associatde page.
 *
 * This is the VA of the link descriptor, that HAL layer later uses to
 * retrieve the list of MSDU's for a given MPDU.
 *
 * Return: void *: Virtual Address of the Rx descriptor
 */
static inline
void *dp_rx_cookie_2_link_desc_va(struct dp_soc *soc,
				  struct hal_buf_info *buf_info)
{
	void *link_desc_va;
	struct qdf_mem_multi_page_t *pages;
	uint16_t page_id = LINK_DESC_COOKIE_PAGE_ID(buf_info->sw_cookie);

	pages = &soc->link_desc_pages;
	if (!pages)
		return NULL;
	if (qdf_unlikely(page_id >= pages->num_pages))
		return NULL;
	link_desc_va = pages->dma_pages[page_id].page_v_addr_start +
		(buf_info->paddr - pages->dma_pages[page_id].page_p_addr);
	return link_desc_va;
}

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef DISABLE_EAPOL_INTRABSS_FWD
#ifdef WLAN_FEATURE_11BE_MLO
static inline bool dp_nbuf_dst_addr_is_mld_addr(struct dp_vdev *vdev,
						qdf_nbuf_t nbuf)
{
	struct qdf_mac_addr *self_mld_mac_addr =
				(struct qdf_mac_addr *)vdev->mld_mac_addr.raw;
	return qdf_is_macaddr_equal(self_mld_mac_addr,
				    (struct qdf_mac_addr *)qdf_nbuf_data(nbuf) +
				    QDF_NBUF_DEST_MAC_OFFSET);
}
#else
static inline bool dp_nbuf_dst_addr_is_mld_addr(struct dp_vdev *vdev,
						qdf_nbuf_t nbuf)
{
	return false;
}
#endif

static inline bool dp_nbuf_dst_addr_is_self_addr(struct dp_vdev *vdev,
						 qdf_nbuf_t nbuf)
{
	return qdf_is_macaddr_equal((struct qdf_mac_addr *)vdev->mac_addr.raw,
				    (struct qdf_mac_addr *)qdf_nbuf_data(nbuf) +
				    QDF_NBUF_DEST_MAC_OFFSET);
}

/**
 * dp_rx_intrabss_eapol_drop_check() - API For EAPOL
 *  pkt with DA not equal to vdev mac addr, fwd is not allowed.
 * @soc: core txrx main context
 * @ta_txrx_peer: source peer entry
 * @rx_tlv_hdr: start address of rx tlvs
 * @nbuf: nbuf that has to be intrabss forwarded
 *
 * Return: true if it is forwarded else false
 */
static inline
bool dp_rx_intrabss_eapol_drop_check(struct dp_soc *soc,
				     struct dp_txrx_peer *ta_txrx_peer,
				     uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf)
{
	if (qdf_unlikely(qdf_nbuf_is_ipv4_eapol_pkt(nbuf) &&
			 !(dp_nbuf_dst_addr_is_self_addr(ta_txrx_peer->vdev,
							 nbuf) ||
			   dp_nbuf_dst_addr_is_mld_addr(ta_txrx_peer->vdev,
							nbuf)))) {
		qdf_nbuf_free(nbuf);
		DP_STATS_INC(soc, rx.err.intrabss_eapol_drop, 1);
		return true;
	}

	return false;
}
#else /* DISABLE_EAPOL_INTRABSS_FWD */

static inline
bool dp_rx_intrabss_eapol_drop_check(struct dp_soc *soc,
				     struct dp_txrx_peer *ta_txrx_peer,
				     uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf)
{
	return false;
}
#endif /* DISABLE_EAPOL_INTRABSS_FWD */

/**
 * dp_rx_intrabss_mcbc_fwd() - Does intrabss forward for mcast packets
 * @soc: core txrx main context
 * @ta_peer: source peer entry
 * @rx_tlv_hdr: start address of rx tlvs
 * @nbuf: nbuf that has to be intrabss forwarded
 * @tid_stats: tid stats pointer
 *
 * Return: bool: true if it is forwarded else false
 */
bool dp_rx_intrabss_mcbc_fwd(struct dp_soc *soc,
			     struct dp_txrx_peer *ta_peer,
			     uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf,
			     struct cdp_tid_rx_stats *tid_stats);

/**
 * dp_rx_intrabss_ucast_fwd() - Does intrabss forward for unicast packets
 * @soc: core txrx main context
 * @ta_peer: source peer entry
 * @tx_vdev_id: VDEV ID for Intra-BSS TX
 * @rx_tlv_hdr: start address of rx tlvs
 * @nbuf: nbuf that has to be intrabss forwarded
 * @tid_stats: tid stats pointer
 *
 * Return: bool: true if it is forwarded else false
 */
bool dp_rx_intrabss_ucast_fwd(struct dp_soc *soc,
			      struct dp_txrx_peer *ta_peer,
			      uint8_t tx_vdev_id,
			      uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf,
			      struct cdp_tid_rx_stats *tid_stats);

/**
 * dp_rx_defrag_concat() - Concatenate the fragments
 *
 * @dst: destination pointer to the buffer
 * @src: source pointer from where the fragment payload is to be copied
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS dp_rx_defrag_concat(qdf_nbuf_t dst, qdf_nbuf_t src)
{
	/*
	 * Inside qdf_nbuf_cat, if it is necessary to reallocate dst
	 * to provide space for src, the headroom portion is copied from
	 * the original dst buffer to the larger new dst buffer.
	 * (This is needed, because the headroom of the dst buffer
	 * contains the rx desc.)
	 */
	if (!qdf_nbuf_cat(dst, src)) {
		/*
		 * qdf_nbuf_cat does not free the src memory.
		 * Free src nbuf before returning
		 * For failure case the caller takes of freeing the nbuf
		 */
		qdf_nbuf_free(src);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_DEFRAG_ERROR;
}

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifndef FEATURE_WDS
void dp_rx_da_learn(struct dp_soc *soc, uint8_t *rx_tlv_hdr,
		    struct dp_txrx_peer *ta_txrx_peer, qdf_nbuf_t nbuf);

static inline QDF_STATUS dp_rx_ast_set_active(struct dp_soc *soc, uint16_t sa_idx, bool is_active)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_rx_wds_srcport_learn(struct dp_soc *soc,
			uint8_t *rx_tlv_hdr,
			struct dp_txrx_peer *txrx_peer,
			qdf_nbuf_t nbuf,
			struct hal_rx_msdu_metadata msdu_metadata)
{
}

static inline void
dp_rx_ipa_wds_srcport_learn(struct dp_soc *soc,
			    struct dp_peer *ta_peer, qdf_nbuf_t nbuf,
			    struct hal_rx_msdu_metadata msdu_end_info,
			    bool ad4_valid, bool chfrag_start)
{
}
#endif

/**
 * dp_rx_desc_dump() - dump the sw rx descriptor
 *
 * @rx_desc: sw rx descriptor
 */
static inline void dp_rx_desc_dump(struct dp_rx_desc *rx_desc)
{
	dp_info("rx_desc->nbuf: %pK, rx_desc->cookie: %d, rx_desc->pool_id: %d, rx_desc->in_use: %d, rx_desc->unmapped: %d",
		rx_desc->nbuf, rx_desc->cookie, rx_desc->pool_id,
		rx_desc->in_use, rx_desc->unmapped);
}

#ifndef QCA_HOST_MODE_WIFI_DISABLED

/**
 * check_qwrap_multicast_loopback() - Check if rx packet is a loopback packet.
 *					In qwrap mode, packets originated from
 *					any vdev should not loopback and
 *					should be dropped.
 * @vdev: vdev on which rx packet is received
 * @nbuf: rx pkt
 *
 */
#if ATH_SUPPORT_WRAP
static inline bool check_qwrap_multicast_loopback(struct dp_vdev *vdev,
						qdf_nbuf_t nbuf)
{
	struct dp_vdev *psta_vdev;
	struct dp_pdev *pdev = vdev->pdev;
	uint8_t *data = qdf_nbuf_data(nbuf);

	if (qdf_unlikely(vdev->proxysta_vdev)) {
		/* In qwrap isolation mode, allow loopback packets as all
		 * packets go to RootAP and Loopback on the mpsta.
		 */
		if (vdev->isolation_vdev)
			return false;
		TAILQ_FOREACH(psta_vdev, &pdev->vdev_list, vdev_list_elem) {
			if (qdf_unlikely(psta_vdev->proxysta_vdev &&
					 !qdf_mem_cmp(psta_vdev->mac_addr.raw,
						      &data[QDF_MAC_ADDR_SIZE],
						      QDF_MAC_ADDR_SIZE))) {
				/* Drop packet if source address is equal to
				 * any of the vdev addresses.
				 */
				return true;
			}
		}
	}
	return false;
}
#else
static inline bool check_qwrap_multicast_loopback(struct dp_vdev *vdev,
						qdf_nbuf_t nbuf)
{
	return false;
}
#endif

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#if defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) ||\
	defined(WLAN_SUPPORT_RX_TAG_STATISTICS) ||\
	defined(WLAN_SUPPORT_RX_FLOW_TAG)
#include "dp_rx_tag.h"
#endif

#if !defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) &&\
	!defined(WLAN_SUPPORT_RX_FLOW_TAG)
/**
 * dp_rx_update_protocol_tag() - Reads CCE metadata from the RX MSDU end TLV
 *                              and set the corresponding tag in QDF packet
 * @soc: core txrx main context
 * @vdev: vdev on which the packet is received
 * @nbuf: QDF pkt buffer on which the protocol tag should be set
 * @rx_tlv_hdr: rBbase address where the RX TLVs starts
 * @ring_index: REO ring number, not used for error & monitor ring
 * @is_reo_exception: flag to indicate if rx from REO ring or exception ring
 * @is_update_stats: flag to indicate whether to update stats or not
 *
 * Return: void
 */
static inline void
dp_rx_update_protocol_tag(struct dp_soc *soc, struct dp_vdev *vdev,
			  qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr,
			  uint16_t ring_index,
			  bool is_reo_exception, bool is_update_stats)
{
}
#endif

#ifndef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
/**
 * dp_rx_err_cce_drop() - Reads CCE metadata from the RX MSDU end TLV
 *                        and returns whether cce metadata matches
 * @soc: core txrx main context
 * @vdev: vdev on which the packet is received
 * @nbuf: QDF pkt buffer on which the protocol tag should be set
 * @rx_tlv_hdr: rBbase address where the RX TLVs starts
 *
 * Return: bool
 */
static inline bool
dp_rx_err_cce_drop(struct dp_soc *soc, struct dp_vdev *vdev,
		   qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
	return false;
}

#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

#ifndef WLAN_SUPPORT_RX_FLOW_TAG
/**
 * dp_rx_update_flow_tag() - Reads FSE metadata from the RX MSDU end TLV
 *                           and set the corresponding tag in QDF packet
 * @soc: core txrx main context
 * @vdev: vdev on which the packet is received
 * @nbuf: QDF pkt buffer on which the protocol tag should be set
 * @rx_tlv_hdr: base address where the RX TLVs starts
 * @update_stats: flag to indicate whether to update stats or not
 *
 * Return: void
 */
static inline void
dp_rx_update_flow_tag(struct dp_soc *soc, struct dp_vdev *vdev,
		      qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr, bool update_stats)
{
}
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */

#define CRITICAL_BUFFER_THRESHOLD	64
/**
 * __dp_rx_buffers_replenish() - replenish rxdma ring with rx nbufs
 *			       called during dp rx initialization
 *			       and at the end of dp_rx_process.
 *
 * @dp_soc: core txrx main context
 * @mac_id: mac_id which is one of 3 mac_ids
 * @dp_rxdma_srng: dp rxdma circular ring
 * @rx_desc_pool: Pointer to free Rx descriptor pool
 * @num_req_buffers: number of buffer to be replenished
 * @desc_list: list of descs if called from dp_rx_process
 *	       or NULL during dp rx initialization or out of buffer
 *	       interrupt.
 * @tail: tail of descs list
 * @req_only: If true don't replenish more than req buffers
 * @func_name: name of the caller function
 *
 * Return: return success or failure
 */
QDF_STATUS __dp_rx_buffers_replenish(struct dp_soc *dp_soc, uint32_t mac_id,
				 struct dp_srng *dp_rxdma_srng,
				 struct rx_desc_pool *rx_desc_pool,
				 uint32_t num_req_buffers,
				 union dp_rx_desc_list_elem_t **desc_list,
				 union dp_rx_desc_list_elem_t **tail,
				 bool req_only,
				 const char *func_name);

/**
 * __dp_rx_buffers_no_map_replenish() - replenish rxdma ring with rx nbufs
 *					use direct APIs to get invalidate
 *					and get the physical address of the
 *					nbuf instead of map api,called during
 *					dp rx initialization and at the end
 *					of dp_rx_process.
 *
 * @dp_soc: core txrx main context
 * @mac_id: mac_id which is one of 3 mac_ids
 * @dp_rxdma_srng: dp rxdma circular ring
 * @rx_desc_pool: Pointer to free Rx descriptor pool
 * @num_req_buffers: number of buffer to be replenished
 * @desc_list: list of descs if called from dp_rx_process
 *	       or NULL during dp rx initialization or out of buffer
 *	       interrupt.
 * @tail: tail of descs list
 *
 * Return: return success or failure
 */
QDF_STATUS
__dp_rx_buffers_no_map_replenish(struct dp_soc *dp_soc, uint32_t mac_id,
				 struct dp_srng *dp_rxdma_srng,
				 struct rx_desc_pool *rx_desc_pool,
				 uint32_t num_req_buffers,
				 union dp_rx_desc_list_elem_t **desc_list,
				 union dp_rx_desc_list_elem_t **tail);

/**
 * __dp_rx_buffers_no_map_lt_replenish() - replenish rxdma ring with rx nbufs
 *					use direct APIs to get invalidate
 *					and get the physical address of the
 *					nbuf instead of map api,called when
 *					low threshold interrupt is triggered
 *
 * @dp_soc: core txrx main context
 * @mac_id: mac_id which is one of 3 mac_ids
 * @dp_rxdma_srng: dp rxdma circular ring
 * @rx_desc_pool: Pointer to free Rx descriptor pool
 *
 * Return: return success or failure
 */
QDF_STATUS
__dp_rx_buffers_no_map_lt_replenish(struct dp_soc *dp_soc, uint32_t mac_id,
				    struct dp_srng *dp_rxdma_srng,
				    struct rx_desc_pool *rx_desc_pool);

/**
 * __dp_pdev_rx_buffers_no_map_attach() - replenish rxdma ring with rx nbufs
 *					use direct APIs to get invalidate
 *					and get the physical address of the
 *					nbuf instead of map api,called during
 *					dp rx initialization.
 *
 * @dp_soc: core txrx main context
 * @mac_id: mac_id which is one of 3 mac_ids
 * @dp_rxdma_srng: dp rxdma circular ring
 * @rx_desc_pool: Pointer to free Rx descriptor pool
 * @num_req_buffers: number of buffer to be replenished
 *
 * Return: return success or failure
 */
QDF_STATUS __dp_pdev_rx_buffers_no_map_attach(struct dp_soc *dp_soc,
					      uint32_t mac_id,
					      struct dp_srng *dp_rxdma_srng,
					      struct rx_desc_pool *rx_desc_pool,
					      uint32_t num_req_buffers);

/**
 * dp_pdev_rx_buffers_attach() - replenish rxdma ring with rx nbufs
 *                               called during dp rx initialization
 *
 * @dp_soc: core txrx main context
 * @mac_id: mac_id which is one of 3 mac_ids
 * @dp_rxdma_srng: dp rxdma circular ring
 * @rx_desc_pool: Pointer to free Rx descriptor pool
 * @num_req_buffers: number of buffer to be replenished
 *
 * Return: return success or failure
 */
QDF_STATUS
dp_pdev_rx_buffers_attach(struct dp_soc *dp_soc, uint32_t mac_id,
			  struct dp_srng *dp_rxdma_srng,
			  struct rx_desc_pool *rx_desc_pool,
			  uint32_t num_req_buffers);

/**
 * dp_rx_link_desc_return() - Return a MPDU link descriptor to HW
 *			      (WBM), following error handling
 *
 * @soc: core DP main context
 * @ring_desc: opaque pointer to the REO error ring descriptor
 * @bm_action: put to idle_list or release to msdu_list
 *
 * Return: QDF_STATUS_E_FAILURE for failure else QDF_STATUS_SUCCESS
 */
QDF_STATUS
dp_rx_link_desc_return(struct dp_soc *soc, hal_ring_desc_t ring_desc,
		       uint8_t bm_action);

/**
 * dp_rx_link_desc_return_by_addr - Return a MPDU link descriptor to
 *					(WBM) by address
 *
 * @soc: core DP main context
 * @link_desc_addr: link descriptor addr
 * @bm_action: put to idle_list or release to msdu_list
 *
 * Return: QDF_STATUS_E_FAILURE for failure else QDF_STATUS_SUCCESS
 */
QDF_STATUS
dp_rx_link_desc_return_by_addr(struct dp_soc *soc,
			       hal_buff_addrinfo_t link_desc_addr,
			       uint8_t bm_action);

/**
 * dp_rxdma_err_process() - RxDMA error processing functionality
 * @int_ctx: pointer to DP interrupt context
 * @soc: core txrx main context
 * @mac_id: mac id which is one of 3 mac_ids
 * @quota: No. of units (packets) that can be serviced in one shot.
 *
 * Return: num of buffers processed
 */
uint32_t
dp_rxdma_err_process(struct dp_intr *int_ctx, struct dp_soc *soc,
		     uint32_t mac_id, uint32_t quota);

/**
 * dp_rx_fill_mesh_stats() - Fills the mesh per packet receive stats
 * @vdev: DP Virtual device handle
 * @nbuf: Buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @txrx_peer: pointer to peer
 *
 * This function allocated memory for mesh receive stats and fill the
 * required stats. Stores the memory address in skb cb.
 *
 * Return: void
 */
void dp_rx_fill_mesh_stats(struct dp_vdev *vdev, qdf_nbuf_t nbuf,
			   uint8_t *rx_tlv_hdr,
			   struct dp_txrx_peer *txrx_peer);

/**
 * dp_rx_filter_mesh_packets() - Filters mesh unwanted packets
 * @vdev: DP Virtual device handle
 * @nbuf: Buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 *
 * This checks if the received packet is matching any filter out
 * catogery and and drop the packet if it matches.
 *
 * Return: QDF_STATUS_SUCCESS indicates drop,
 *         QDF_STATUS_E_FAILURE indicate to not drop
 */
QDF_STATUS dp_rx_filter_mesh_packets(struct dp_vdev *vdev, qdf_nbuf_t nbuf,
					uint8_t *rx_tlv_hdr);

int dp_wds_rx_policy_check(uint8_t *rx_tlv_hdr, struct dp_vdev *vdev,
			   struct dp_txrx_peer *peer);

/**
 * dp_rx_dump_info_and_assert() - dump RX Ring info and Rx Desc info
 *
 * @soc: core txrx main context
 * @hal_ring_hdl: opaque pointer to the HAL Rx Ring, which will be serviced
 * @ring_desc: opaque pointer to the RX ring descriptor
 * @rx_desc: host rx descriptor
 *
 * Return: void
 */
void dp_rx_dump_info_and_assert(struct dp_soc *soc,
				hal_ring_handle_t hal_ring_hdl,
				hal_ring_desc_t ring_desc,
				struct dp_rx_desc *rx_desc);

/**
 * dp_rx_compute_delay() - Compute and fill in all timestamps
 *				to pass in correct fields
 * @vdev: pdev handle
 * @nbuf: network buffer
 *
 * Return: none
 */
void dp_rx_compute_delay(struct dp_vdev *vdev, qdf_nbuf_t nbuf);

#ifdef QCA_PEER_EXT_STATS

/**
 * dp_rx_compute_tid_delay - Compute per TID delay stats
 * @stats: TID delay stats to update
 * @nbuf: NBuffer
 *
 * Return: Void
 */
void dp_rx_compute_tid_delay(struct cdp_delay_tid_stats *stats,
			     qdf_nbuf_t nbuf);
#endif /* QCA_PEER_EXT_STATS */

#ifdef RX_DESC_DEBUG_CHECK
/**
 * dp_rx_desc_check_magic() - check the magic value in dp_rx_desc
 * @rx_desc: rx descriptor pointer
 *
 * Return: true, if magic is correct, else false.
 */
static inline bool dp_rx_desc_check_magic(struct dp_rx_desc *rx_desc)
{
	if (qdf_unlikely(rx_desc->magic != DP_RX_DESC_MAGIC))
		return false;

	rx_desc->magic = 0;
	return true;
}

/**
 * dp_rx_desc_prep() - prepare rx desc
 * @rx_desc: rx descriptor pointer to be prepared
 * @nbuf_frag_info_t: struct dp_rx_nbuf_frag_info *
 *
 * Note: assumption is that we are associating a nbuf which is mapped
 *
 * Return: none
 */
static inline
void dp_rx_desc_prep(struct dp_rx_desc *rx_desc,
		     struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	rx_desc->magic = DP_RX_DESC_MAGIC;
	rx_desc->nbuf = (nbuf_frag_info_t->virt_addr).nbuf;
	rx_desc->unmapped = 0;
	rx_desc->nbuf_data_addr = (uint8_t *)qdf_nbuf_data(rx_desc->nbuf);
}

/**
 * dp_rx_desc_frag_prep() - prepare rx desc
 * @rx_desc: rx descriptor pointer to be prepared
 * @nbuf_frag_info_t: struct dp_rx_nbuf_frag_info *
 *
 * Note: assumption is that we frag address is mapped
 *
 * Return: none
 */
#ifdef DP_RX_MON_MEM_FRAG
static inline
void dp_rx_desc_frag_prep(struct dp_rx_desc *rx_desc,
			  struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	rx_desc->magic = DP_RX_DESC_MAGIC;
	rx_desc->rx_buf_start =
		(uint8_t *)((nbuf_frag_info_t->virt_addr).vaddr);
	rx_desc->paddr_buf_start = nbuf_frag_info_t->paddr;
	rx_desc->unmapped = 0;
}
#else
static inline
void dp_rx_desc_frag_prep(struct dp_rx_desc *rx_desc,
			  struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
}
#endif /* DP_RX_MON_MEM_FRAG */

/**
 * dp_rx_desc_paddr_sanity_check() - paddr sanity for ring desc vs rx_desc
 * @rx_desc: rx descriptor
 * @ring_paddr: paddr obatined from the ring
 *
 * Return: QDF_STATUS
 */
static inline
bool dp_rx_desc_paddr_sanity_check(struct dp_rx_desc *rx_desc,
				   uint64_t ring_paddr)
{
	return (ring_paddr == qdf_nbuf_get_frag_paddr(rx_desc->nbuf, 0));
}
#else

static inline bool dp_rx_desc_check_magic(struct dp_rx_desc *rx_desc)
{
	return true;
}

static inline
void dp_rx_desc_prep(struct dp_rx_desc *rx_desc,
		     struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	rx_desc->nbuf = (nbuf_frag_info_t->virt_addr).nbuf;
	rx_desc->unmapped = 0;
}

#ifdef DP_RX_MON_MEM_FRAG
static inline
void dp_rx_desc_frag_prep(struct dp_rx_desc *rx_desc,
			  struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	rx_desc->rx_buf_start =
		(uint8_t *)((nbuf_frag_info_t->virt_addr).vaddr);
	rx_desc->paddr_buf_start = nbuf_frag_info_t->paddr;
	rx_desc->unmapped = 0;
}
#else
static inline
void dp_rx_desc_frag_prep(struct dp_rx_desc *rx_desc,
			  struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
}
#endif /* DP_RX_MON_MEM_FRAG */

static inline
bool dp_rx_desc_paddr_sanity_check(struct dp_rx_desc *rx_desc,
				   uint64_t ring_paddr)
{
	return true;
}
#endif /* RX_DESC_DEBUG_CHECK */

/**
 * dp_rx_enable_mon_dest_frag() - Enable frag processing for
 *              monitor destination ring via frag.
 * @rx_desc_pool: Rx desc pool
 * @is_mon_dest_desc: Is it for monitor dest buffer
 *
 * Enable this flag only for monitor destination buffer processing
 * if DP_RX_MON_MEM_FRAG feature is enabled.
 * If flag is set then frag based function will be called for alloc,
 * map, prep desc and free ops for desc buffer else normal nbuf based
 * function will be called.
 *
 * Return: None
 */
void dp_rx_enable_mon_dest_frag(struct rx_desc_pool *rx_desc_pool,
				bool is_mon_dest_desc);

/**
 * dp_rx_process_rxdma_err() - Function to deliver rxdma unencrypted_err
 *			       frames to OS or wifi parse errors.
 * @soc: core DP main context
 * @nbuf: buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @txrx_peer: peer reference
 * @err_code: rxdma err code
 * @mac_id: mac_id which is one of 3 mac_ids(Assuming mac_id and
 * pool_id has same mapping)
 *
 * Return: None
 */
void
dp_rx_process_rxdma_err(struct dp_soc *soc, qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr, struct dp_txrx_peer *txrx_peer,
			uint8_t err_code, uint8_t mac_id);

#ifndef QCA_MULTIPASS_SUPPORT
static inline
bool dp_rx_multipass_process(struct dp_txrx_peer *peer, qdf_nbuf_t nbuf,
			     uint8_t tid)
{
	return false;
}
#else
/**
 * dp_rx_multipass_process - insert vlan tag on frames for traffic separation
 * @txrx_peer: DP txrx peer handle
 * @nbuf: skb
 * @tid: traffic priority
 *
 * Return: bool: true in case of success else false
 * Success is considered if:
 *  i. If frame has vlan header
 *  ii. If the frame comes from different peer and dont need multipass processing
 * Failure is considered if:
 *  i. Frame comes from multipass peer but doesn't contain vlan header.
 *  In failure case, drop such frames.
 */
bool dp_rx_multipass_process(struct dp_txrx_peer *txrx_peer, qdf_nbuf_t nbuf,
			     uint8_t tid);
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED

#ifndef WLAN_RX_PKT_CAPTURE_ENH
static inline
QDF_STATUS dp_peer_set_rx_capture_enabled(struct dp_pdev *pdev,
					  struct dp_peer *peer_handle,
					  bool value, uint8_t *mac_addr)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_rx_deliver_to_stack() - deliver pkts to network stack
 * Caller to hold peer refcount and check for valid peer
 * @soc: soc
 * @vdev: vdev
 * @peer: txrx peer
 * @nbuf_head: skb list head
 * @nbuf_tail: skb list tail
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_deliver_to_stack(struct dp_soc *soc,
				  struct dp_vdev *vdev,
				  struct dp_txrx_peer *peer,
				  qdf_nbuf_t nbuf_head,
				  qdf_nbuf_t nbuf_tail);

#ifdef QCA_SUPPORT_EAPOL_OVER_CONTROL_PORT
/**
 * dp_rx_eapol_deliver_to_stack() - deliver pkts to network stack
 * caller to hold peer refcount and check for valid peer
 * @soc: soc
 * @vdev: vdev
 * @peer: peer
 * @nbuf_head: skb list head
 * @nbuf_tail: skb list tail
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_eapol_deliver_to_stack(struct dp_soc *soc,
					struct dp_vdev *vdev,
					struct dp_txrx_peer *peer,
					qdf_nbuf_t nbuf_head,
					qdf_nbuf_t nbuf_tail);
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED

#ifdef QCA_OL_RX_LOCK_LESS_ACCESS
/**
 * dp_rx_srng_access_start()- Wrapper function to log access start of a hal ring
 * @int_ctx: pointer to DP interrupt context
 * @soc: DP soc structure pointer
 * @hal_ring_hdl: HAL ring handle
 *
 * Return: 0 on success; error on failure
 */
static inline int
dp_rx_srng_access_start(struct dp_intr *int_ctx, struct dp_soc *soc,
			hal_ring_handle_t hal_ring_hdl)
{
	return hal_srng_access_start_unlocked(soc->hal_soc, hal_ring_hdl);
}

/**
 * dp_rx_srng_access_end()- Wrapper function to log access end of a hal ring
 * @int_ctx: pointer to DP interrupt context
 * @soc: DP soc structure pointer
 * @hal_ring_hdl: HAL ring handle
 *
 * Return: None
 */
static inline void
dp_rx_srng_access_end(struct dp_intr *int_ctx, struct dp_soc *soc,
		      hal_ring_handle_t hal_ring_hdl)
{
	hal_srng_access_end_unlocked(soc->hal_soc, hal_ring_hdl);
}
#else
static inline int
dp_rx_srng_access_start(struct dp_intr *int_ctx, struct dp_soc *soc,
			hal_ring_handle_t hal_ring_hdl)
{
	return dp_srng_access_start(int_ctx, soc, hal_ring_hdl);
}

static inline void
dp_rx_srng_access_end(struct dp_intr *int_ctx, struct dp_soc *soc,
		      hal_ring_handle_t hal_ring_hdl)
{
	dp_srng_access_end(int_ctx, soc, hal_ring_hdl);
}
#endif

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_rx_wbm_sg_list_reset() - Initialize sg list
 *
 * This api should be called at soc init and afterevery sg processing.
 *@soc: DP SOC handle
 */
static inline void dp_rx_wbm_sg_list_reset(struct dp_soc *soc)
{
	if (soc) {
		soc->wbm_sg_param.wbm_is_first_msdu_in_sg = false;
		soc->wbm_sg_param.wbm_sg_nbuf_head = NULL;
		soc->wbm_sg_param.wbm_sg_nbuf_tail = NULL;
		soc->wbm_sg_param.wbm_sg_desc_msdu_len = 0;
	}
}

/**
 * dp_rx_wbm_sg_list_deinit() - De-initialize sg list
 *
 * This api should be called in down path, to avoid any leak.
 *@soc: DP SOC handle
 */
static inline void dp_rx_wbm_sg_list_deinit(struct dp_soc *soc)
{
	if (soc) {
		if (soc->wbm_sg_param.wbm_sg_nbuf_head)
			qdf_nbuf_list_free(soc->wbm_sg_param.wbm_sg_nbuf_head);

		dp_rx_wbm_sg_list_reset(soc);
	}
}

#ifndef QCA_HOST_MODE_WIFI_DISABLED

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
#define DP_RX_PROCESS_NBUF(soc, head, tail, ebuf_head, ebuf_tail, rx_desc) \
	do {								   \
		if (!soc->rx_buff_pool[rx_desc->pool_id].is_initialized) { \
			DP_RX_LIST_APPEND(head, tail, rx_desc->nbuf);	   \
			break;						   \
		}							   \
		DP_RX_LIST_APPEND(ebuf_head, ebuf_tail, rx_desc->nbuf);	   \
		if (!qdf_nbuf_is_rx_chfrag_cont(rx_desc->nbuf)) {	   \
			if (!dp_rx_buffer_pool_refill(soc, ebuf_head,	   \
						      rx_desc->pool_id))   \
				DP_RX_MERGE_TWO_LIST(head, tail,	   \
						     ebuf_head, ebuf_tail);\
			ebuf_head = NULL;				   \
			ebuf_tail = NULL;				   \
		}							   \
	} while (0)
#else
#define DP_RX_PROCESS_NBUF(soc, head, tail, ebuf_head, ebuf_tail, rx_desc) \
	DP_RX_LIST_APPEND(head, tail, rx_desc->nbuf)
#endif /* WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL */

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_rx_link_desc_refill_duplicate_check() - check if link desc duplicate
 *					      to refill
 * @soc: DP SOC handle
 * @buf_info: the last link desc buf info
 * @ring_buf_info: current buf address pointor including link desc
 *
 * Return: none.
 */
void dp_rx_link_desc_refill_duplicate_check(
				struct dp_soc *soc,
				struct hal_buf_info *buf_info,
				hal_buff_addrinfo_t ring_buf_info);

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * dp_rx_deliver_to_pkt_capture() - deliver rx packet to packet capture
 * @soc : dp_soc handle
 * @pdev: dp_pdev handle
 * @peer_id: peer_id of the peer for which completion came
 * @is_offload:
 * @netbuf: Buffer pointer
 *
 * This function is used to deliver rx packet to packet capture
 */
void dp_rx_deliver_to_pkt_capture(struct dp_soc *soc,  struct dp_pdev *pdev,
				  uint16_t peer_id, uint32_t is_offload,
				  qdf_nbuf_t netbuf);
void dp_rx_deliver_to_pkt_capture_no_peer(struct dp_soc *soc, qdf_nbuf_t nbuf,
					  uint32_t is_offload);
#else
static inline void
dp_rx_deliver_to_pkt_capture(struct dp_soc *soc,  struct dp_pdev *pdev,
			     uint16_t peer_id, uint32_t is_offload,
			     qdf_nbuf_t netbuf)
{
}

static inline void
dp_rx_deliver_to_pkt_capture_no_peer(struct dp_soc *soc, qdf_nbuf_t nbuf,
				     uint32_t is_offload)
{
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef FEATURE_MEC
/**
 * dp_rx_mcast_echo_check() - check if the mcast pkt is a loop
 *			      back on same vap or a different vap.
 * @soc: core DP main context
 * @peer: dp peer handler
 * @rx_tlv_hdr: start of the rx TLV header
 * @nbuf: pkt buffer
 *
 * Return: bool (true if it is a looped back pkt else false)
 *
 */
bool dp_rx_mcast_echo_check(struct dp_soc *soc,
			    struct dp_txrx_peer *peer,
			    uint8_t *rx_tlv_hdr,
			    qdf_nbuf_t nbuf);
#else
static inline bool dp_rx_mcast_echo_check(struct dp_soc *soc,
					  struct dp_txrx_peer *peer,
					  uint8_t *rx_tlv_hdr,
					  qdf_nbuf_t nbuf)
{
	return false;
}
#endif /* FEATURE_MEC */
#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifdef RECEIVE_OFFLOAD
/**
 * dp_rx_fill_gro_info() - Fill GRO info from RX TLV into skb->cb
 * @soc: DP SOC handle
 * @rx_tlv: RX TLV received for the msdu
 * @msdu: msdu for which GRO info needs to be filled
 * @rx_ol_pkt_cnt: counter to be incremented for GRO eligible packets
 *
 * Return: None
 */
void dp_rx_fill_gro_info(struct dp_soc *soc, uint8_t *rx_tlv,
			 qdf_nbuf_t msdu, uint32_t *rx_ol_pkt_cnt);
#else
static inline
void dp_rx_fill_gro_info(struct dp_soc *soc, uint8_t *rx_tlv,
			 qdf_nbuf_t msdu, uint32_t *rx_ol_pkt_cnt)
{
}
#endif

/**
 * dp_rx_msdu_stats_update() - update per msdu stats.
 * @soc: core txrx main context
 * @nbuf: pointer to the first msdu of an amsdu.
 * @rx_tlv_hdr: pointer to the start of RX TLV headers.
 * @txrx_peer: pointer to the txrx peer object.
 * @ring_id: reo dest ring number on which pkt is reaped.
 * @tid_stats: per tid rx stats.
 *
 * update all the per msdu stats for that nbuf.
 *
 * Return: void
 */
void dp_rx_msdu_stats_update(struct dp_soc *soc, qdf_nbuf_t nbuf,
			     uint8_t *rx_tlv_hdr,
			     struct dp_txrx_peer *txrx_peer,
			     uint8_t ring_id,
			     struct cdp_tid_rx_stats *tid_stats);

/**
 * dp_rx_deliver_to_stack_no_peer() - try deliver rx data even if
 *				      no corresbonding peer found
 * @soc: core txrx main context
 * @nbuf: pkt skb pointer
 *
 * This function will try to deliver some RX special frames to stack
 * even there is no peer matched found. for instance, LFR case, some
 * eapol data will be sent to host before peer_map done.
 *
 * Return: None
 */
void dp_rx_deliver_to_stack_no_peer(struct dp_soc *soc, qdf_nbuf_t nbuf);

/**
 * dp_rx_srng_get_num_pending() - get number of pending entries
 * @hal_soc: hal soc opaque pointer
 * @hal_ring_hdl: opaque pointer to the HAL Rx Ring
 * @num_entries: number of entries in the hal_ring.
 * @near_full: pointer to a boolean. This is set if ring is near full.
 *
 * The function returns the number of entries in a destination ring which are
 * yet to be reaped. The function also checks if the ring is near full.
 * If more than half of the ring needs to be reaped, the ring is considered
 * approaching full.
 * The function uses hal_srng_dst_num_valid_locked to get the number of valid
 * entries. It should not be called within a SRNG lock. HW pointer value is
 * synced into cached_hp.
 *
 * Return: Number of pending entries if any
 */
uint32_t dp_rx_srng_get_num_pending(hal_soc_handle_t hal_soc,
				    hal_ring_handle_t hal_ring_hdl,
				    uint32_t num_entries,
				    bool *near_full);

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
/**
 * dp_rx_ring_record_entry() - Record an entry into the rx ring history.
 * @soc: Datapath soc structure
 * @ring_num: REO ring number
 * @ring_desc: REO ring descriptor
 *
 * Return: None
 */
void dp_rx_ring_record_entry(struct dp_soc *soc, uint8_t ring_num,
			     hal_ring_desc_t ring_desc);
#else
static inline void
dp_rx_ring_record_entry(struct dp_soc *soc, uint8_t ring_num,
			hal_ring_desc_t ring_desc)
{
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef RX_DESC_SANITY_WAR
QDF_STATUS dp_rx_desc_sanity(struct dp_soc *soc, hal_soc_handle_t hal_soc,
			     hal_ring_handle_t hal_ring_hdl,
			     hal_ring_desc_t ring_desc,
			     struct dp_rx_desc *rx_desc);
#else
static inline
QDF_STATUS dp_rx_desc_sanity(struct dp_soc *soc, hal_soc_handle_t hal_soc,
			     hal_ring_handle_t hal_ring_hdl,
			     hal_ring_desc_t ring_desc,
			     struct dp_rx_desc *rx_desc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef DP_RX_DROP_RAW_FRM
/**
 * dp_rx_is_raw_frame_dropped() - if raw frame nbuf, free and drop
 * @nbuf: pkt skb pointer
 *
 * Return: true - raw frame, dropped
 *	   false - not raw frame, do nothing
 */
bool dp_rx_is_raw_frame_dropped(qdf_nbuf_t nbuf);
#else
static inline
bool dp_rx_is_raw_frame_dropped(qdf_nbuf_t nbuf)
{
	return false;
}
#endif

#ifdef RX_DESC_DEBUG_CHECK
/**
 * dp_rx_desc_nbuf_sanity_check - Add sanity check to catch REO rx_desc paddr
 *				  corruption
 * @soc: DP SoC context
 * @ring_desc: REO ring descriptor
 * @rx_desc: Rx descriptor
 *
 * Return: NONE
 */
QDF_STATUS dp_rx_desc_nbuf_sanity_check(struct dp_soc *soc,
					hal_ring_desc_t ring_desc,
					struct dp_rx_desc *rx_desc);
#else
static inline
QDF_STATUS dp_rx_desc_nbuf_sanity_check(struct dp_soc *soc,
					hal_ring_desc_t ring_desc,
					struct dp_rx_desc *rx_desc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
/**
 * dp_rx_update_stats() - Update soc level rx packet count
 * @soc: DP soc handle
 * @nbuf: nbuf received
 *
 * Return: none
 */
void dp_rx_update_stats(struct dp_soc *soc, qdf_nbuf_t nbuf);
#else
static inline
void dp_rx_update_stats(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
}
#endif

/**
 * dp_rx_cksum_offload() - set the nbuf checksum as defined by hardware.
 * @pdev: dp_pdev handle
 * @nbuf: pointer to the first msdu of an amsdu.
 * @rx_tlv_hdr: pointer to the start of RX TLV headers.
 *
 * The ipsumed field of the skb is set based on whether HW validated the
 * IP/TCP/UDP checksum.
 *
 * Return: void
 */
#if defined(MAX_PDEV_CNT) && (MAX_PDEV_CNT == 1)
static inline
void dp_rx_cksum_offload(struct dp_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint8_t *rx_tlv_hdr)
{
	qdf_nbuf_rx_cksum_t cksum = {0};
	//TODO - Move this to ring desc api
	//HAL_RX_MSDU_DESC_IP_CHKSUM_FAIL_GET
	//HAL_RX_MSDU_DESC_TCP_UDP_CHKSUM_FAIL_GET
	uint32_t ip_csum_err, tcp_udp_csum_er;

	hal_rx_tlv_csum_err_get(pdev->soc->hal_soc, rx_tlv_hdr, &ip_csum_err,
				&tcp_udp_csum_er);

	if (qdf_likely(!ip_csum_err && !tcp_udp_csum_er)) {
		cksum.l4_result = QDF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY;
		qdf_nbuf_set_rx_cksum(nbuf, &cksum);
	} else {
		DP_STATS_INCC(pdev, err.ip_csum_err, 1, ip_csum_err);
		DP_STATS_INCC(pdev, err.tcp_udp_csum_err, 1, tcp_udp_csum_er);
	}
}
#else
static inline
void dp_rx_cksum_offload(struct dp_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint8_t *rx_tlv_hdr)
{
}
#endif
#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
static inline
bool dp_rx_reap_loop_pkt_limit_hit(struct dp_soc *soc, int num_reaped,
				   int max_reap_limit)
{
	bool limit_hit = false;

	limit_hit =
		(num_reaped >= max_reap_limit) ? true : false;

	if (limit_hit)
		DP_STATS_INC(soc, rx.reap_loop_pkt_limit_hit, 1)

	return limit_hit;
}

static inline
bool dp_rx_enable_eol_data_check(struct dp_soc *soc)
{
	return soc->wlan_cfg_ctx->rx_enable_eol_data_check;
}

static inline int dp_rx_get_loop_pkt_limit(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;

	return cfg->rx_reap_loop_pkt_limit;
}
#else
static inline
bool dp_rx_reap_loop_pkt_limit_hit(struct dp_soc *soc, int num_reaped,
				   int max_reap_limit)
{
	return false;
}

static inline
bool dp_rx_enable_eol_data_check(struct dp_soc *soc)
{
	return false;
}

static inline int dp_rx_get_loop_pkt_limit(struct dp_soc *soc)
{
	return 0;
}
#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */

void dp_rx_update_stats(struct dp_soc *soc, qdf_nbuf_t nbuf);

#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * dp_rx_is_list_ready() - Make different lists for 4-address
 *			   and 3-address frames
 * @nbuf_head: skb list head
 * @vdev: vdev
 * @txrx_peer : txrx_peer
 * @peer_id: peer id of new received frame
 * @vdev_id: vdev_id of new received frame
 *
 * Return: true if peer_ids are different.
 */
static inline bool
dp_rx_is_list_ready(qdf_nbuf_t nbuf_head,
		    struct dp_vdev *vdev,
		    struct dp_txrx_peer *txrx_peer,
		    uint16_t peer_id,
		    uint8_t vdev_id)
{
	if (nbuf_head && txrx_peer && txrx_peer->peer_id != peer_id)
		return true;

	return false;
}
#else
static inline bool
dp_rx_is_list_ready(qdf_nbuf_t nbuf_head,
		    struct dp_vdev *vdev,
		    struct dp_txrx_peer *txrx_peer,
		    uint16_t peer_id,
		    uint8_t vdev_id)
{
	if (nbuf_head && vdev && (vdev->vdev_id != vdev_id))
		return true;

	return false;
}
#endif

#ifdef WLAN_FEATURE_MARK_FIRST_WAKEUP_PACKET
/**
 * dp_rx_mark_first_packet_after_wow_wakeup - get first packet after wow wakeup
 * @pdev: pointer to dp_pdev structure
 * @rx_tlv: pointer to rx_pkt_tlvs structure
 * @nbuf: pointer to skb buffer
 *
 * Return: None
 */
void dp_rx_mark_first_packet_after_wow_wakeup(struct dp_pdev *pdev,
					      uint8_t *rx_tlv,
					      qdf_nbuf_t nbuf);
#else
static inline void
dp_rx_mark_first_packet_after_wow_wakeup(struct dp_pdev *pdev,
					 uint8_t *rx_tlv,
					 qdf_nbuf_t nbuf)
{
}
#endif

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
static inline uint8_t
dp_rx_get_defrag_bm_id(struct dp_soc *soc)
{
	return DP_DEFRAG_RBM(soc->wbm_sw0_bm_id);
}

static inline uint8_t
dp_rx_get_rx_bm_id(struct dp_soc *soc)
{
	return DP_WBM2SW_RBM(soc->wbm_sw0_bm_id);
}
#else
static inline uint8_t
dp_rx_get_rx_bm_id(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *cfg_ctx = soc->wlan_cfg_ctx;
	uint8_t wbm2_sw_rx_rel_ring_id;

	wbm2_sw_rx_rel_ring_id = wlan_cfg_get_rx_rel_ring_id(cfg_ctx);

	return HAL_RX_BUF_RBM_SW_BM(soc->wbm_sw0_bm_id,
				    wbm2_sw_rx_rel_ring_id);
}

static inline uint8_t
dp_rx_get_defrag_bm_id(struct dp_soc *soc)
{
	return dp_rx_get_rx_bm_id(soc);
}
#endif

static inline uint16_t
dp_rx_peer_metadata_peer_id_get(struct dp_soc *soc, uint32_t peer_metadata)
{
	return soc->arch_ops.dp_rx_peer_metadata_peer_id_get(soc,
							     peer_metadata);
}

/**
 * dp_rx_desc_pool_init_generic() - Generic Rx descriptors initialization
 * @soc: SOC handle
 * @rx_desc_pool: pointer to RX descriptor pool
 * @pool_id: pool ID
 *
 * Return: None
 */
QDF_STATUS dp_rx_desc_pool_init_generic(struct dp_soc *soc,
				  struct rx_desc_pool *rx_desc_pool,
				  uint32_t pool_id);

void dp_rx_desc_pool_deinit_generic(struct dp_soc *soc,
				  struct rx_desc_pool *rx_desc_pool,
				  uint32_t pool_id);

/**
 * dp_rx_pkt_tracepoints_enabled() - Get the state of rx pkt tracepoint
 *
 * Return: True if any rx pkt tracepoint is enabled else false
 */
static inline
bool dp_rx_pkt_tracepoints_enabled(void)
{
	return (qdf_trace_dp_rx_tcp_pkt_enabled() ||
		qdf_trace_dp_rx_udp_pkt_enabled() ||
		qdf_trace_dp_rx_pkt_enabled());
}

#ifdef FEATURE_DIRECT_LINK
/**
 * dp_audio_smmu_map()- Map memory region into Audio SMMU CB
 * @qdf_dev: pointer to QDF device structure
 * @paddr: physical address
 * @iova: DMA address
 * @size: memory region size
 *
 * Return: 0 on success else failure code
 */
static inline
int dp_audio_smmu_map(qdf_device_t qdf_dev, qdf_dma_addr_t paddr,
		      qdf_dma_addr_t iova, qdf_size_t size)
{
	return pld_audio_smmu_map(qdf_dev->dev, paddr, iova, size);
}

/**
 * dp_audio_smmu_unmap()- Remove memory region mapping from Audio SMMU CB
 * @qdf_dev: pointer to QDF device structure
 * @iova: DMA address
 * @size: memory region size
 *
 * Return: None
 */
static inline
void dp_audio_smmu_unmap(qdf_device_t qdf_dev, qdf_dma_addr_t iova,
			 qdf_size_t size)
{
	pld_audio_smmu_unmap(qdf_dev->dev, iova, size);
}
#else
static inline
int dp_audio_smmu_map(qdf_device_t qdf_dev, qdf_dma_addr_t paddr,
		      qdf_dma_addr_t iova, qdf_size_t size)
{
	return 0;
}

static inline
void dp_audio_smmu_unmap(qdf_device_t qdf_dev, qdf_dma_addr_t iova,
			 qdf_size_t size)
{
}
#endif

#if defined(QCA_DP_RX_NBUF_NO_MAP_UNMAP) && !defined(BUILD_X86)
static inline
QDF_STATUS dp_pdev_rx_buffers_attach_simple(struct dp_soc *soc, uint32_t mac_id,
					    struct dp_srng *rxdma_srng,
					    struct rx_desc_pool *rx_desc_pool,
					    uint32_t num_req_buffers)
{
	return __dp_pdev_rx_buffers_no_map_attach(soc, mac_id,
						  rxdma_srng,
						  rx_desc_pool,
						  num_req_buffers);
}

static inline
void dp_rx_buffers_replenish_simple(struct dp_soc *soc, uint32_t mac_id,
				    struct dp_srng *rxdma_srng,
				    struct rx_desc_pool *rx_desc_pool,
				    uint32_t num_req_buffers,
				    union dp_rx_desc_list_elem_t **desc_list,
				    union dp_rx_desc_list_elem_t **tail)
{
	__dp_rx_buffers_no_map_replenish(soc, mac_id, rxdma_srng, rx_desc_pool,
					 num_req_buffers, desc_list, tail);
}

static inline
void dp_rx_buffers_lt_replenish_simple(struct dp_soc *soc, uint32_t mac_id,
				       struct dp_srng *rxdma_srng,
				       struct rx_desc_pool *rx_desc_pool,
				       uint32_t num_req_buffers,
				       union dp_rx_desc_list_elem_t **desc_list,
				       union dp_rx_desc_list_elem_t **tail)
{
	__dp_rx_buffers_no_map_lt_replenish(soc, mac_id, rxdma_srng,
					    rx_desc_pool);
}

#ifndef QCA_DP_NBUF_FAST_RECYCLE_CHECK
static inline
qdf_dma_addr_t dp_rx_nbuf_sync_no_dsb(struct dp_soc *dp_soc,
				      qdf_nbuf_t nbuf,
				      uint32_t buf_size)
{
	qdf_nbuf_dma_inv_range_no_dsb((void *)nbuf->data,
				      (void *)(nbuf->data + buf_size));

	return (qdf_dma_addr_t)qdf_mem_virt_to_phys(nbuf->data);
}
#else
#define L3_HEADER_PAD 2
static inline
qdf_dma_addr_t dp_rx_nbuf_sync_no_dsb(struct dp_soc *dp_soc,
				      qdf_nbuf_t nbuf,
				      uint32_t buf_size)
{
	if (nbuf->recycled_for_ds) {
		nbuf->recycled_for_ds = 0;
		return (qdf_dma_addr_t)qdf_mem_virt_to_phys(nbuf->data);
	}

	if (unlikely(!nbuf->fast_recycled)) {
		qdf_nbuf_dma_inv_range_no_dsb((void *)nbuf->data,
					      (void *)(nbuf->data + buf_size));
	} else {
		/*
		 * In case of fast_recycled is set we can avoid invalidating
		 * the complete buffer as it would have been invalidated
		 * by tx driver before giving to recycler.
		 *
		 * But we need to still invalidate rx_pkt_tlv_size as this
		 * area will not be invalidated in TX path
		 */
		DP_STATS_INC(dp_soc, rx.fast_recycled, 1);
		qdf_nbuf_dma_inv_range_no_dsb((void *)nbuf->data,
					      (void *)(nbuf->data +
						       dp_soc->rx_pkt_tlv_size +
						       L3_HEADER_PAD));
	}

	nbuf->fast_recycled = 0;
	return (qdf_dma_addr_t)qdf_mem_virt_to_phys(nbuf->data);
}
#endif

static inline
qdf_dma_addr_t dp_rx_nbuf_sync(struct dp_soc *dp_soc,
			       qdf_nbuf_t nbuf,
			       uint32_t buf_size)
{
	qdf_nbuf_dma_inv_range((void *)nbuf->data,
			       (void *)(nbuf->data + buf_size));

	return (qdf_dma_addr_t)qdf_mem_virt_to_phys(nbuf->data);
}

#if !defined(SPECULATIVE_READ_DISABLED)
static inline
void dp_rx_nbuf_unmap(struct dp_soc *soc,
		      struct dp_rx_desc *rx_desc,
		      uint8_t reo_ring_num)
{
	struct rx_desc_pool *rx_desc_pool;
	qdf_nbuf_t nbuf;

	rx_desc_pool = &soc->rx_desc_buf[rx_desc->pool_id];
	nbuf = rx_desc->nbuf;

	qdf_nbuf_dma_inv_range_no_dsb((void *)nbuf->data,
			       (void *)(nbuf->data + rx_desc_pool->buf_size));
}

static inline
void dp_rx_nbuf_unmap_pool(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   qdf_nbuf_t nbuf)
{
	qdf_nbuf_dma_inv_range((void *)nbuf->data,
			       (void *)(nbuf->data + rx_desc_pool->buf_size));
}

#else
static inline
void dp_rx_nbuf_unmap(struct dp_soc *soc,
		      struct dp_rx_desc *rx_desc,
		      uint8_t reo_ring_num)
{
}

static inline
void dp_rx_nbuf_unmap_pool(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   qdf_nbuf_t nbuf)
{
}
#endif

static inline
void dp_rx_per_core_stats_update(struct dp_soc *soc, uint8_t ring_id,
				 uint32_t bufs_reaped)
{
}

static inline
qdf_nbuf_t dp_rx_nbuf_alloc(struct dp_soc *soc,
			    struct rx_desc_pool *rx_desc_pool)
{
	return qdf_nbuf_alloc_simple(soc->osdev, rx_desc_pool->buf_size,
				     RX_BUFFER_RESERVATION,
				     rx_desc_pool->buf_alignment, FALSE);
}

static inline
void  dp_rx_nbuf_free(qdf_nbuf_t nbuf)
{
	qdf_nbuf_free_simple(nbuf);
}
#else
static inline
QDF_STATUS dp_pdev_rx_buffers_attach_simple(struct dp_soc *soc, uint32_t mac_id,
					    struct dp_srng *rxdma_srng,
					    struct rx_desc_pool *rx_desc_pool,
					    uint32_t num_req_buffers)
{
	return dp_pdev_rx_buffers_attach(soc, mac_id,
					 rxdma_srng,
					 rx_desc_pool,
					 num_req_buffers);
}

static inline
void dp_rx_buffers_replenish_simple(struct dp_soc *soc, uint32_t mac_id,
				    struct dp_srng *rxdma_srng,
				    struct rx_desc_pool *rx_desc_pool,
				    uint32_t num_req_buffers,
				    union dp_rx_desc_list_elem_t **desc_list,
				    union dp_rx_desc_list_elem_t **tail)
{
	dp_rx_buffers_replenish(soc, mac_id, rxdma_srng, rx_desc_pool,
				num_req_buffers, desc_list, tail, false);
}

static inline
void dp_rx_buffers_lt_replenish_simple(struct dp_soc *soc, uint32_t mac_id,
				       struct dp_srng *rxdma_srng,
				       struct rx_desc_pool *rx_desc_pool,
				       uint32_t num_req_buffers,
				       union dp_rx_desc_list_elem_t **desc_list,
				       union dp_rx_desc_list_elem_t **tail)
{
	dp_rx_buffers_replenish(soc, mac_id, rxdma_srng, rx_desc_pool,
				num_req_buffers, desc_list, tail, false);
}

static inline
qdf_dma_addr_t dp_rx_nbuf_sync_no_dsb(struct dp_soc *dp_soc,
				      qdf_nbuf_t nbuf,
				      uint32_t buf_size)
{
	return (qdf_dma_addr_t)NULL;
}

static inline
qdf_dma_addr_t dp_rx_nbuf_sync(struct dp_soc *dp_soc,
			       qdf_nbuf_t nbuf,
			       uint32_t buf_size)
{
	return (qdf_dma_addr_t)NULL;
}

static inline
void dp_rx_nbuf_unmap(struct dp_soc *soc,
		      struct dp_rx_desc *rx_desc,
		      uint8_t reo_ring_num)
{
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_buf[rx_desc->pool_id];
	dp_ipa_reo_ctx_buf_mapping_lock(soc, reo_ring_num);

	dp_audio_smmu_unmap(soc->osdev,
			    QDF_NBUF_CB_PADDR(rx_desc->nbuf),
			    rx_desc_pool->buf_size);

	dp_ipa_handle_rx_buf_smmu_mapping(soc, rx_desc->nbuf,
					  rx_desc_pool->buf_size,
					  false, __func__, __LINE__);

	qdf_nbuf_unmap_nbytes_single(soc->osdev, rx_desc->nbuf,
				     QDF_DMA_FROM_DEVICE,
				     rx_desc_pool->buf_size);

	dp_ipa_reo_ctx_buf_mapping_unlock(soc, reo_ring_num);
}

static inline
void dp_rx_nbuf_unmap_pool(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   qdf_nbuf_t nbuf)
{
	dp_audio_smmu_unmap(soc->osdev, QDF_NBUF_CB_PADDR(nbuf),
			    rx_desc_pool->buf_size);
	dp_ipa_handle_rx_buf_smmu_mapping(soc, nbuf, rx_desc_pool->buf_size,
					  false, __func__, __LINE__);
	qdf_nbuf_unmap_nbytes_single(soc->osdev, nbuf, QDF_DMA_FROM_DEVICE,
				     rx_desc_pool->buf_size);
}

static inline
void dp_rx_per_core_stats_update(struct dp_soc *soc, uint8_t ring_id,
				 uint32_t bufs_reaped)
{
	int cpu_id = qdf_get_cpu();

	DP_STATS_INC(soc, rx.ring_packets[cpu_id][ring_id], bufs_reaped);
}

static inline
qdf_nbuf_t dp_rx_nbuf_alloc(struct dp_soc *soc,
			    struct rx_desc_pool *rx_desc_pool)
{
	return qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
			      RX_BUFFER_RESERVATION,
			      rx_desc_pool->buf_alignment, FALSE);
}

static inline
void dp_rx_nbuf_free(qdf_nbuf_t nbuf)
{
	qdf_nbuf_free(nbuf);
}
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_rx_desc_reuse() - Reuse the rx descriptors to fill the rx buf ring
 * @soc: core txrx main context
 * @nbuf_list: nbuf list for delayed free
 *
 * Return: void
 */
void dp_rx_desc_reuse(struct dp_soc *soc, qdf_nbuf_t *nbuf_list);

/**
 * dp_rx_desc_delayed_free() - Delayed free of the rx descs
 *
 * @soc: core txrx main context
 *
 * Return: void
 */
void dp_rx_desc_delayed_free(struct dp_soc *soc);
#endif

/**
 * dp_rx_get_txrx_peer_and_vdev() - Get txrx peer and vdev from peer id
 * @soc: core txrx main context
 * @nbuf : pointer to the first msdu of an amsdu.
 * @peer_id : Peer id of the peer
 * @txrx_ref_handle : Buffer to save the handle for txrx peer's reference
 * @pkt_capture_offload : Flag indicating if pkt capture offload is needed
 * @vdev : Buffer to hold pointer to vdev
 * @rx_pdev : Buffer to hold pointer to rx pdev
 * @dsf : delay stats flag
 * @old_tid : Old tid
 *
 * Get txrx peer and vdev from peer id
 *
 * Return: Pointer to txrx peer
 */
static inline struct dp_txrx_peer *
dp_rx_get_txrx_peer_and_vdev(struct dp_soc *soc,
			     qdf_nbuf_t nbuf,
			     uint16_t peer_id,
			     dp_txrx_ref_handle *txrx_ref_handle,
			     bool pkt_capture_offload,
			     struct dp_vdev **vdev,
			     struct dp_pdev **rx_pdev,
			     uint32_t *dsf,
			     uint32_t *old_tid)
{
	struct dp_txrx_peer *txrx_peer = NULL;

	txrx_peer = dp_txrx_peer_get_ref_by_id(soc, peer_id, txrx_ref_handle,
					       DP_MOD_ID_RX);

	if (qdf_likely(txrx_peer)) {
		*vdev = txrx_peer->vdev;
	} else {
		nbuf->next = NULL;
		dp_rx_deliver_to_pkt_capture_no_peer(soc, nbuf,
						     pkt_capture_offload);
		if (!pkt_capture_offload)
			dp_rx_deliver_to_stack_no_peer(soc, nbuf);

		goto end;
	}

	if (qdf_unlikely(!(*vdev))) {
		qdf_nbuf_free(nbuf);
		DP_STATS_INC(soc, rx.err.invalid_vdev, 1);
		goto end;
	}

	*rx_pdev = (*vdev)->pdev;
	*dsf = (*rx_pdev)->delay_stats_flag;
	*old_tid = 0xff;

end:
	return txrx_peer;
}

static inline QDF_STATUS
dp_peer_rx_reorder_queue_setup(struct dp_soc *soc, struct dp_peer *peer,
			       int tid, uint32_t ba_window_size)
{
	return soc->arch_ops.dp_peer_rx_reorder_queue_setup(soc,
							    peer, tid,
							    ba_window_size);
}

static inline
void dp_rx_nbuf_list_deliver(struct dp_soc *soc,
			     struct dp_vdev *vdev,
			     struct dp_txrx_peer *txrx_peer,
			     uint16_t peer_id,
			     uint8_t pkt_capture_offload,
			     qdf_nbuf_t deliver_list_head,
			     qdf_nbuf_t deliver_list_tail)
{
	qdf_nbuf_t nbuf, next;

	if (qdf_likely(deliver_list_head)) {
		if (qdf_likely(txrx_peer)) {
			dp_rx_deliver_to_pkt_capture(soc, vdev->pdev, peer_id,
						     pkt_capture_offload,
						     deliver_list_head);
			if (!pkt_capture_offload)
				dp_rx_deliver_to_stack(soc, vdev, txrx_peer,
						       deliver_list_head,
						       deliver_list_tail);
		} else {
			nbuf = deliver_list_head;
			while (nbuf) {
				next = nbuf->next;
				nbuf->next = NULL;
				dp_rx_deliver_to_stack_no_peer(soc, nbuf);
				nbuf = next;
			}
		}
	}
}

#ifdef DP_TX_RX_TPUT_SIMULATE
/*
 * Change this macro value to simulate different RX T-put,
 * if OTA is 100 Mbps, to simulate 200 Mbps, then multiplication factor
 * is 2, set macro value as 1 (multiplication factor - 1).
 */
#define DP_RX_PKTS_DUPLICATE_CNT 0
static inline
void dp_rx_nbuf_list_dup_deliver(struct dp_soc *soc,
				 struct dp_vdev *vdev,
				 struct dp_txrx_peer *txrx_peer,
				 uint16_t peer_id,
				 uint8_t pkt_capture_offload,
				 qdf_nbuf_t ori_list_head,
				 qdf_nbuf_t ori_list_tail)
{
	qdf_nbuf_t new_skb = NULL;
	qdf_nbuf_t new_list_head = NULL;
	qdf_nbuf_t new_list_tail = NULL;
	qdf_nbuf_t nbuf = NULL;
	int i;

	for (i = 0; i < DP_RX_PKTS_DUPLICATE_CNT; i++) {
		nbuf = ori_list_head;
		new_list_head = NULL;
		new_list_tail = NULL;

		while (nbuf) {
			new_skb = qdf_nbuf_copy(nbuf);
			if (qdf_likely(new_skb))
				DP_RX_LIST_APPEND(new_list_head,
						  new_list_tail,
						  new_skb);
			else
				dp_err("copy skb failed");

			nbuf = qdf_nbuf_next(nbuf);
		}

		/* deliver the copied nbuf list */
		dp_rx_nbuf_list_deliver(soc, vdev, txrx_peer, peer_id,
					pkt_capture_offload,
					new_list_head,
					new_list_tail);
	}

	/* deliver the original skb_list */
	dp_rx_nbuf_list_deliver(soc, vdev, txrx_peer, peer_id,
				pkt_capture_offload,
				ori_list_head,
				ori_list_tail);
}

#define DP_RX_DELIVER_TO_STACK dp_rx_nbuf_list_dup_deliver

#else /* !DP_TX_RX_TPUT_SIMULATE */

#define DP_RX_DELIVER_TO_STACK dp_rx_nbuf_list_deliver

#endif /* DP_TX_RX_TPUT_SIMULATE */

#endif /* _DP_RX_H */

/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2018, Intel Corporation. */

#ifndef _ICE_TXRX_H_
#define _ICE_TXRX_H_

#define ICE_DFLT_IRQ_WORK	256
#define ICE_RXBUF_2048		2048
#define ICE_MAX_CHAINED_RX_BUFS	5
#define ICE_MAX_BUF_TXD		8
#define ICE_MIN_TX_LEN		17

/* The size limit for a transmit buffer in a descriptor is (16K - 1).
 * In order to align with the read requests we will align the value to
 * the nearest 4K which represents our maximum read request size.
 */
#define ICE_MAX_READ_REQ_SIZE	4096
#define ICE_MAX_DATA_PER_TXD	(16 * 1024 - 1)
#define ICE_MAX_DATA_PER_TXD_ALIGNED \
	(~(ICE_MAX_READ_REQ_SIZE - 1) & ICE_MAX_DATA_PER_TXD)

#define ICE_RX_BUF_WRITE	16	/* Must be power of 2 */
#define ICE_MAX_TXQ_PER_TXQG	128

/* Tx Descriptors needed, worst case */
#define DESC_NEEDED (MAX_SKB_FRAGS + 4)
#define ICE_DESC_UNUSED(R)	\
	((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->count) + \
	(R)->next_to_clean - (R)->next_to_use - 1)

#define ICE_TX_FLAGS_TSO	BIT(0)
#define ICE_TX_FLAGS_HW_VLAN	BIT(1)
#define ICE_TX_FLAGS_SW_VLAN	BIT(2)
#define ICE_TX_FLAGS_VLAN_M	0xffff0000
#define ICE_TX_FLAGS_VLAN_S	16

struct ice_tx_buf {
	struct ice_tx_desc *next_to_watch;
	struct sk_buff *skb;
	unsigned int bytecount;
	unsigned short gso_segs;
	u32 tx_flags;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
};

struct ice_tx_offload_params {
	u8 header_len;
	u32 td_cmd;
	u32 td_offset;
	u32 td_l2tag1;
	u16 cd_l2tag2;
	u32 cd_tunnel_params;
	u64 cd_qw1;
	struct ice_ring *tx_ring;
};

struct ice_rx_buf {
	struct sk_buff *skb;
	dma_addr_t dma;
	struct page *page;
	unsigned int page_offset;
};

struct ice_q_stats {
	u64 pkts;
	u64 bytes;
};

struct ice_txq_stats {
	u64 restart_q;
	u64 tx_busy;
	u64 tx_linearize;
};

struct ice_rxq_stats {
	u64 non_eop_descs;
	u64 alloc_page_failed;
	u64 alloc_buf_failed;
	u64 page_reuse_count;
};

/* this enum matches hardware bits and is meant to be used by DYN_CTLN
 * registers and QINT registers or more generally anywhere in the manual
 * mentioning ITR_INDX, ITR_NONE cannot be used as an index 'n' into any
 * register but instead is a special value meaning "don't update" ITR0/1/2.
 */
enum ice_dyn_idx_t {
	ICE_IDX_ITR0 = 0,
	ICE_IDX_ITR1 = 1,
	ICE_IDX_ITR2 = 2,
	ICE_ITR_NONE = 3	/* ITR_NONE must not be used as an index */
};

/* Header split modes defined by DTYPE field of Rx RLAN context */
enum ice_rx_dtype {
	ICE_RX_DTYPE_NO_SPLIT		= 0,
	ICE_RX_DTYPE_HEADER_SPLIT	= 1,
	ICE_RX_DTYPE_SPLIT_ALWAYS	= 2,
};

/* indices into GLINT_ITR registers */
#define ICE_RX_ITR	ICE_IDX_ITR0
#define ICE_TX_ITR	ICE_IDX_ITR1
#define ICE_ITR_DYNAMIC	0x8000  /* use top bit as a flag */
#define ICE_ITR_8K	0x003E

/* apply ITR HW granularity translation to program the HW registers */
#define ITR_TO_REG(val, itr_gran) (((val) & ~ICE_ITR_DYNAMIC) >> (itr_gran))

/* Legacy or Advanced Mode Queue */
#define ICE_TX_ADVANCED	0
#define ICE_TX_LEGACY	1

/* descriptor ring, associated with a VSI */
struct ice_ring {
	struct ice_ring *next;		/* pointer to next ring in q_vector */
	void *desc;			/* Descriptor ring memory */
	struct device *dev;		/* Used for DMA mapping */
	struct net_device *netdev;	/* netdev ring maps to */
	struct ice_vsi *vsi;		/* Backreference to associated VSI */
	struct ice_q_vector *q_vector;	/* Backreference to associated vector */
	u8 __iomem *tail;
	union {
		struct ice_tx_buf *tx_buf;
		struct ice_rx_buf *rx_buf;
	};
	u16 q_index;			/* Queue number of ring */
	u32 txq_teid;			/* Added Tx queue TEID */

	/* high bit set means dynamic, use accessor routines to read/write.
	 * hardware supports 2us/1us resolution for the ITR registers.
	 * these values always store the USER setting, and must be converted
	 * before programming to a register.
	 */
	u16 rx_itr_setting;
	u16 tx_itr_setting;

	u16 count;			/* Number of descriptors */
	u16 reg_idx;			/* HW register index of the ring */

	/* used in interrupt processing */
	u16 next_to_use;
	u16 next_to_clean;

	u8 ring_active;			/* is ring online or not */

	/* stats structs */
	struct ice_q_stats	stats;
	struct u64_stats_sync syncp;
	union {
		struct ice_txq_stats tx_stats;
		struct ice_rxq_stats rx_stats;
	};

	unsigned int size;		/* length of descriptor ring in bytes */
	dma_addr_t dma;			/* physical address of ring */
	struct rcu_head rcu;		/* to avoid race on free */
	u16 next_to_alloc;
} ____cacheline_internodealigned_in_smp;

enum ice_latency_range {
	ICE_LOWEST_LATENCY = 0,
	ICE_LOW_LATENCY = 1,
	ICE_BULK_LATENCY = 2,
	ICE_ULTRA_LATENCY = 3,
};

struct ice_ring_container {
	/* array of pointers to rings */
	struct ice_ring *ring;
	unsigned int total_bytes;	/* total bytes processed this int */
	unsigned int total_pkts;	/* total packets processed this int */
	enum ice_latency_range latency_range;
	u16 itr;
};

/* iterator for handling rings in ring container */
#define ice_for_each_ring(pos, head) \
	for (pos = (head).ring; pos; pos = pos->next)

bool ice_alloc_rx_bufs(struct ice_ring *rxr, u16 cleaned_count);
netdev_tx_t ice_start_xmit(struct sk_buff *skb, struct net_device *netdev);
void ice_clean_tx_ring(struct ice_ring *tx_ring);
void ice_clean_rx_ring(struct ice_ring *rx_ring);
int ice_setup_tx_ring(struct ice_ring *tx_ring);
int ice_setup_rx_ring(struct ice_ring *rx_ring);
void ice_free_tx_ring(struct ice_ring *tx_ring);
void ice_free_rx_ring(struct ice_ring *rx_ring);
int ice_napi_poll(struct napi_struct *napi, int budget);

#endif /* _ICE_TXRX_H_ */

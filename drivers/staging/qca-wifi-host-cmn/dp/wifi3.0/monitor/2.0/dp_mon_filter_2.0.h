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

#ifndef _DP_MON_FILTER_2_0_H_
#define _DP_MON_FILTER_2_0_H_

#include <dp_htt.h>

#define DEFAULT_DMA_LENGTH 7
#define DMA_LENGTH_64B 1
#define DMA_LENGTH_128B 2
#define DMA_LENGTH_256B 4

/* rx hdr tlv dma lengths */
enum dp_rx_hdr_dma_length {
	/* default dma length(128B) */
	DEFAULT_RX_HDR_DMA_LENGTH = 0,
	/* dma length 64 bytes */
	RX_HDR_DMA_LENGTH_64B = 1,
	/* dma length 128 bytes */
	RX_HDR_DMA_LENGTH_128B = 2,
	/* dma length 256 bytes */
	RX_HDR_DMA_LENGTH_256B = 3,
};

/* fwd declarations */
struct dp_mon_pdev_be;

/**
 * dp_rx_mon_enable_set() - Setup rx monitor feature
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void
dp_rx_mon_enable_set(uint32_t *msg_word,
		     struct htt_rx_ring_tlv_filter *tlv_filter);

/**
 * dp_rx_mon_hdr_length_set() - Setup rx monitor hdr tlv length
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void
dp_rx_mon_hdr_length_set(uint32_t *msg_word,
			 struct htt_rx_ring_tlv_filter *tlv_filter);

/**
 * dp_rx_mon_packet_length_set() - Setup rx monitor per packet type length
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void
dp_rx_mon_packet_length_set(uint32_t *msg_word,
			    struct htt_rx_ring_tlv_filter *tlv_filter);

/**
 * dp_rx_mon_word_mask_subscribe() - Setup rx monitor word mask subscription
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void
dp_rx_mon_word_mask_subscribe(uint32_t *msg_word,
			      struct htt_rx_ring_tlv_filter *tlv_filter);

/**
 * dp_rx_mon_enable_mpdu_logging() - Setup rx monitor per packet mpdu logging
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void
dp_rx_mon_enable_mpdu_logging(uint32_t *msg_word,
			      struct htt_rx_ring_tlv_filter *tlv_filter);

/**
 * dp_rx_mon_enable_fpmo() - Setup rx monitor fpmo mode type/subtype filters
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void
dp_rx_mon_enable_fpmo(uint32_t *msg_word,
		      struct htt_rx_ring_tlv_filter *tlv_filter);

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_filter_setup_enhanced_stats_2_0() - Setup the enhanced stats filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_enhanced_stats_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_enhanced_stats_2_0() - Reset the enhanced stats filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_enhanced_stats_2_0(struct dp_pdev *pdev);
#else
static inline void
dp_mon_filter_setup_enhanced_stats_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_enhanced_stats_2_0(struct dp_pdev *pdev)
{
}
#endif

#ifdef QCA_UNDECODED_METADATA_SUPPORT
/**
 * dp_mon_filter_setup_undecoded_metadata_capture_2_0() - Setup the filter
 * for undecoded metadata capture
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_undecoded_metadata_capture_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_undecoded_metadata_capture_2_0() - Reset the filter
 * for undecoded metadata capture
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_undecoded_metadata_capture_2_0(struct dp_pdev *pdev);
#else
static inline void
dp_mon_filter_setup_undecoded_metadata_capture_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_undecoded_metadata_capture_2_0(struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_mon_filter_setup_rx_mon_mode_2_0() - Setup the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_mon_mode_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_mon_mode_2_0() - Reset the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_mon_mode_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_tx_mon_mode_2_0() - Setup the Tx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_tx_mon_mode_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_tx_mon_mode_2_0() - Reset the Tx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_tx_mon_mode_2_0(struct dp_pdev *pdev);

#ifdef WDI_EVENT_ENABLE
/**
 * dp_mon_filter_setup_rx_pkt_log_full_2_0() - Setup the Rx pktlog full mode
 *                                             filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_full_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pkt_log_full_2_0() - Reset pktlog full mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_full_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_rx_pkt_log_lite_2_0() - Setup the Rx pktlog lite mode
 *                                             filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_lite_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pkt_log_lite_2_0() - Reset the Rx pktlog lite mode
 *                                             filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_lite_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_rx_pkt_log_cbf_2_0() - Setup the Rx pktlog cbf mode
 *                                            filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_cbf_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pktlog_cbf_2_0() - Reset the Rx pktlog cbf mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pktlog_cbf_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_pktlog_hybrid_2_0() - Setup the pktlog hybrid mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_pktlog_hybrid_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_pktlog_hybrid_2_0() - Reset pktlog hybrid mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_pktlog_hybrid_2_0(struct dp_pdev *pdev);
#else
static inline void
dp_mon_filter_setup_rx_pkt_log_full_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_rx_pkt_log_full_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_setup_rx_pkt_log_lite_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_rx_pkt_log_lite_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_setup_rx_pkt_log_cbf_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_rx_pktlog_cbf_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_setup_pktlog_hybrid_2_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_pktlog_hybrid_2_0(struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_tx_mon_filter_update_2_0() - Update monitor filter configuration
 * @pdev: physical device handle
 *
 * Return: non-zero for failure, zero for success
 */
QDF_STATUS dp_tx_mon_filter_update_2_0(struct dp_pdev *pdev);

/**
 * dp_rx_mon_filter_update_2_0() - Update monitor filter configuration
 * @pdev: physical device handle
 *
 * Return: non-zero for failure, zero for success
 */
QDF_STATUS dp_rx_mon_filter_update_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_dealloc_2_0() - free tx monitor filter memory
 * @pdev: physical device handle
 *
 * Return: non-zero for failure, zero for success
 */
void dp_mon_filter_dealloc_2_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_alloc_2_0() - tx monitor filter allocation
 * @pdev: physical device handle
 *
 * Return: non-zero for failure, zero for success
 */
QDF_STATUS dp_mon_filter_alloc_2_0(struct dp_pdev *pdev);

#ifdef QCA_SUPPORT_LITE_MONITOR
void dp_mon_filter_reset_rx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev);

void dp_mon_filter_setup_rx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev);

/**
 * dp_mon_filter_reset_tx_lite_mon() - Reset tx lite monitor filter
 * @be_mon_pdev: physical mon device handle
 *
 * Return: Null
 */
void dp_mon_filter_reset_tx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev);

/**
 * dp_mon_filter_setup_tx_lite_mon() - Setup tx lite monitor filter
 * @be_mon_pdev: physical mon device handle
 *
 * Return: Null
 */
void dp_mon_filter_setup_tx_lite_mon(struct dp_mon_pdev_be *be_mon_pdev);
#endif
#endif /* _DP_MON_FILTER_2_0_H_ */

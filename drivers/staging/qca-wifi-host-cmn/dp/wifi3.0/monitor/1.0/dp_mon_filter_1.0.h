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
#ifndef _DP_MON_FILTER_1_0_H_
#define _DP_MON_FILTER_1_0_H_

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_filter_setup_enhanced_stats_1_0() - Setup the enhanced stats filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_enhanced_stats_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_enhanced_stats_1_0() - Reset the enhanced stats filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_enhanced_stats_1_0(struct dp_pdev *pdev);
#else
static inline void dp_mon_filter_setup_enhanced_stats_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_enhanced_stats_1_0(struct dp_pdev *pdev)
{
}
#endif

#ifdef QCA_UNDECODED_METADATA_SUPPORT
/**
 * dp_mon_filter_setup_undecoded_metadata_capture_1_0() - Setup the filter
 * for undecoded metadata capture
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_undecoded_metadata_capture_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_undecoded_metadata_capture_1_0() - Reset the filter
 * for undecoded metadata capture
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_undecoded_metadata_capture_1_0(struct dp_pdev *pdev);
#else
static inline void
dp_mon_filter_setup_undecoded_metadata_capture_1_0(struct dp_pdev *pdev)
{
}

static inline void
dp_mon_filter_reset_undecoded_metadata_capture_1_0(struct dp_pdev *pdev)
{
}
#endif /* QCA_UNDECODED_METADATA_SUPPORT */

#ifdef QCA_MCOPY_SUPPORT
/**
 * dp_mon_filter_setup_mcopy_mode_1_0() - Setup the m_copy mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mcopy_mode_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_mcopy_mode_1_0() - Reset the m_copy mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mcopy_mode_1_0(struct dp_pdev *pdev);
#else
static inline void dp_mon_filter_setup_mcopy_mode_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_mcopy_mode_1_0(struct dp_pdev *pdev)
{
}
#endif

#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
/**
 * dp_mon_filter_setup_smart_monitor_1_0() - Setup the smart monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_smart_monitor_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_smart_monitor_1_0() - Reset the smart monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_smart_monitor_1_0(struct dp_pdev *pdev);
#else
static inline void dp_mon_filter_setup_smart_monitor_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_smart_monitor_1_0(struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_mon_set_reset_mon_mac_filter_1_0() - Set/Reset the monitor mode filter
 * @pdev: DP pdev handle
 * @val: Indicate set/reset filter
 */
void dp_mon_set_reset_mon_mac_filter_1_0(struct dp_pdev *pdev, bool val);

#ifdef WLAN_RX_PKT_CAPTURE_ENH
/**
 * dp_mon_filter_setup_rx_enh_capture_1_0() - Setup the Rx capture mode filters
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_enh_capture_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_enh_capture_1_0() - Reset the Rx capture mode filters
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_enh_capture_1_0(struct dp_pdev *pdev);
#else
static inline void dp_mon_filter_setup_rx_enh_capture_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_rx_enh_capture_1_0(struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_mon_filter_setup_mon_mode_1_0() - Setup the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mon_mode_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_mon_mode_1_0() - Reset the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mon_mode_1_0(struct dp_pdev *pdev);

#ifdef WDI_EVENT_ENABLE
/**
 * dp_mon_filter_setup_rx_pkt_log_full_1_0() - Setup the Rx pktlog full mode
 * filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_full_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pkt_log_full_1_0() - Reset the Rx pktlog full mode
 * filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_full_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_rx_pkt_log_lite_1_0() - Setup the Rx pktlog lite mode
 * filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_lite_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pkt_log_lite_1_0() - Reset the Rx pktlog lite mode
 * filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_lite_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_rx_pkt_log_cbf_1_0() - Setup the Rx pktlog cbf mode
 * filter in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_cbf_1_0(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pktlog_cbf_1_0() - Reset the Rx pktlog cbf mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pktlog_cbf_1_0(struct dp_pdev *pdev);
#else
static inline void dp_mon_filter_setup_rx_pkt_log_full_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_rx_pkt_log_full_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_setup_rx_pkt_log_lite_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_rx_pkt_log_lite_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_setup_rx_pkt_log_cbf_1_0(struct dp_pdev *pdev)
{
}

static inline void dp_mon_filter_reset_rx_pktlog_cbf_1_0(struct dp_pdev *pdev)
{
}
#endif

QDF_STATUS dp_mon_filter_update_1_0(struct dp_pdev *pdev);

#ifdef QCA_MAC_FILTER_FW_SUPPORT
/**
 * dp_mon_mac_filter_set() - Setup rx monitor mac filter feature
 * @msg_word: msg word
 * @tlv_filter: rx ring filter configuration
 */
void dp_mon_mac_filter_set(uint32_t *msg_word,
			   struct htt_rx_ring_tlv_filter *tlv_filter);
#else
static inline
void dp_mon_mac_filter_set(uint32_t *msg_word,
			   struct htt_rx_ring_tlv_filter *tlv_filter)
{
}
#endif

#endif /* _DP_MON_FILTER_1_0_H_ */

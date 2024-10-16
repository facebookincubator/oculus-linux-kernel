/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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

#ifndef _DP_MON_FILTER_H_
#define _DP_MON_FILTER_H_

/*
 * Accessor Macros to access the software
 * defined HTT filter htt_rx_ring_tlv_filter.
 */
#define DP_MON_FILTER_TLV_OFFSET                      0x00000000
#define DP_MON_FILTER_TLV_MASK                        0xffffffff
#define DP_MON_FILTER_TLV_LSB                         0

#define DP_MON_FILTER_FP_MGMT_OFFSET                  0x00000004
#define DP_MON_FILTER_FP_MGMT_MASK                    0x0000ffff
#define DP_MON_FILTER_FP_MGMT_LSB                     0

#define DP_MON_FILTER_MO_MGMT_OFFSET                  0x00000004
#define DP_MON_FILTER_MO_MGMT_MASK                    0xffff0000
#define DP_MON_FILTER_MO_MGMT_LSB                     16

#define DP_MON_FILTER_FP_CTRL_OFFSET                  0x00000008
#define DP_MON_FILTER_FP_CTRL_MASK                    0x0000ffff
#define DP_MON_FILTER_FP_CTRL_LSB                     0

#define DP_MON_FILTER_MO_CTRL_OFFSET                  0x00000008
#define DP_MON_FILTER_MO_CTRL_MASK                    0xffff0000
#define DP_MON_FILTER_MO_CTRL_LSB                     16

#define DP_MON_FILTER_FP_DATA_OFFSET                  0x0000000c
#define DP_MON_FILTER_FP_DATA_MASK                    0x0000ffff
#define DP_MON_FILTER_FP_DATA_LSB                     0

#define DP_MON_FILTER_MO_DATA_OFFSET                  0x0000000c
#define DP_MON_FILTER_MO_DATA_MASK                    0xffff0000
#define DP_MON_FILTER_MO_DATA_LSB                     16

#define DP_MON_FILTER_MD_DATA_OFFSET                  0x00000010
#define DP_MON_FILTER_MD_DATA_MASK                    0x0000ffff
#define DP_MON_FILTER_MD_DATA_LSB                     0

#define DP_MON_FILTER_MD_MGMT_OFFSET                  0x00000010
#define DP_MON_FILTER_MD_MGMT_MASK                    0xffff0000
#define DP_MON_FILTER_MD_MGMT_LSB                     16

#define DP_MON_FILTER_MD_CTRL_OFFSET                  0x00000014
#define DP_MON_FILTER_MD_CTRL_MASK                    0x0000ffff
#define DP_MON_FILTER_MD_CTRL_LSB                     0

#define DP_MON_FILTER_GET(src, field) \
	((*((uint32_t *)((uint8_t *)(src) + DP_MON_ ## field ## _OFFSET)) & \
	(DP_MON_ ## field ## _MASK)) >> DP_MON_ ## field ## _LSB) \

#define DP_MON_FILTER_SET(dst, field, value) \
do { \
	uint32_t *val = \
	((uint32_t *)((uint8_t *)(dst) + DP_MON_ ## field ## _OFFSET)); \
	*val &= ~(DP_MON_ ## field ## _MASK); \
	*val |= ((value) << DP_MON_ ## field ## _LSB); \
} while (0)

#define DP_MON_FILTER_PRINT(fmt, args ...) \
	QDF_TRACE(QDF_MODULE_ID_MON_FILTER, QDF_TRACE_LEVEL_DEBUG, \
		  fmt, ## args)

#define dp_mon_filter_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_MON_FILTER, params)
#define dp_mon_filter_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_MON_FILTER, params)
#define dp_mon_filter_info(params...) \
	__QDF_TRACE_FL(QDF_TRACE_LEVEL_INFO_HIGH, QDF_MODULE_ID_MON_FILTER, ## params)
#define dp_mon_filter_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_MON_FILTER, params)

/**
 * struct dp_mon_filter - Monitor TLV filter
 * @valid: enable/disable TLV filter
 * @tlv_filter: Rx ring TLV filter
 */
struct dp_mon_filter {
	bool valid;
	struct htt_rx_ring_tlv_filter tlv_filter;
};

/*
 * NB: intentionally not using kernel-doc comment because the kernel-doc
 *     script does not handle the complex conditional compilation
 * enum dp_mon_filter_mode - Different modes for SRNG filters
 * @DP_MON_FILTER_ENHACHED_STATS_MODE: PPDU enhanced stats mode
 * @DP_MON_FILTER_SMART_MONITOR_MODE: Smart monitor mode
 * @DP_MON_FILTER_MCOPY_MODE: AM copy mode
 * @DP_MON_FILTER_MONITOR_MODE: Monitor mode
 * @DP_MON_FILTER_RX_CAPTURE_MODE: Rx Capture mode
 * @DP_MON_FILTER_PKT_LOG_FULL_MODE: Packet log full mode
 * @DP_MON_FILTER_PKT_LOG_LITE_MODE: Packet log lite mode
 * @DP_MON_FILTER_PKT_LOG_CBF_MODE: Packet log cbf mode
 * @DP_MON_FILTER_PKT_LOG_HYBRID_MODE: Packet log hybrid mode
 * @DP_MON_FILTER_UNDECODED_METADATA_CAPTURE_MODE: Undecoded frame capture
 * @DP_MON_FILTER_LITE_MON_MODE:
 * @DP_MON_FILTER_MAX_MODE: max filter mode
 */
enum dp_mon_filter_mode {
#ifdef QCA_ENHANCED_STATS_SUPPORT
	DP_MON_FILTER_ENHACHED_STATS_MODE,
#endif /* QCA_ENHANCED_STATS_SUPPORT */
#ifdef QCA_MCOPY_SUPPORT
	DP_MON_FILTER_MCOPY_MODE,
#endif /* QCA_MCOPY_SUPPORT */
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
	DP_MON_FILTER_SMART_MONITOR_MODE,
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */
	DP_MON_FILTER_MONITOR_MODE,
#ifdef	WLAN_RX_PKT_CAPTURE_ENH
	DP_MON_FILTER_RX_CAPTURE_MODE,
#endif /* WLAN_RX_PKT_CAPTURE_ENH */

#ifdef WDI_EVENT_ENABLE
	DP_MON_FILTER_PKT_LOG_FULL_MODE,
	DP_MON_FILTER_PKT_LOG_LITE_MODE,
	DP_MON_FILTER_PKT_LOG_CBF_MODE,
#ifdef BE_PKTLOG_SUPPORT
	DP_MON_FILTER_PKT_LOG_HYBRID_MODE,
#endif
#endif /* WDI_EVENT_ENABLE */
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	DP_MON_FILTER_UNDECODED_METADATA_CAPTURE_MODE,
#endif
#ifdef QCA_SUPPORT_LITE_MONITOR
	DP_MON_FILTER_LITE_MON_MODE,
#endif /*QCA_SUPPORT_LITE_MONITOR*/
	DP_MON_FILTER_MAX_MODE
};

/**
 * enum dp_mon_filter_srng_type - Srng types dynamic mode filter
 * settings.
 * @DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF: RXDMA srng type
 * @DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS: RxDMA monitor status srng
 * @DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF: RxDMA destination srng
 * @DP_MON_FILTER_SRNG_TYPE_RXMON_DEST: RxMON destination srng
 * @DP_MON_FILTER_SRNG_TYPE_TXMON_DEST: TxMON destination srng
 * @DP_MON_FILTER_SRNG_TYPE_MAX: Srng max type
 */
enum dp_mon_filter_srng_type {
	DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF,
	DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS,
	DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF,
	DP_MON_FILTER_SRNG_TYPE_RXMON_DEST,
	DP_MON_FILTER_SRNG_TYPE_TXMON_DEST,
	DP_MON_FILTER_SRNG_TYPE_MAX
};

/**
 * enum dp_mon_filter_action - Action for storing the filters
 * into the radio structure.
 * @DP_MON_FILTER_CLEAR: Clears the filter for a mode
 * @DP_MON_FILTER_SET: Set the filtes for a mode
 */
enum dp_mon_filter_action {
	DP_MON_FILTER_CLEAR,
	DP_MON_FILTER_SET,
};

#ifdef QCA_UNDECODED_METADATA_SUPPORT
/**
 * enum dp_mon_fp_phy_err_buf_source - fp_phy_err_buf_src indicates the source
 * ring selection for the FP PHY ERR status tlv.
 * @WBM2RXDMA_BUF_SOURCE_RING: 0 - wbm2rxdma_buf_source_ring
 * @FW2RXDMA_BUF_SOURCE_RING: 1 - fw2rxdma_buf_source_ring
 * @SW2RXDMA_BUF_SOURCE_RING: 2 - sw2rxdma_buf_source_ring
 * @NO_BUFFER_RING: 3 - no_buffer_ring
 */
enum dp_mon_fp_phy_err_buf_source {
	WBM2RXDMA_BUF_SOURCE_RING,
	FW2RXDMA_BUF_SOURCE_RING,
	SW2RXDMA_BUF_SOURCE_RING,
	NO_BUFFER_RING
};

/**
 * enum dp_mon_fp_phy_err_buf_dest - fp_phy_err_buf_dest indicates the
 * destination ring selection for the FP PHY ERR status tlv.
 * @RXDMA_RELEASING_RING: 0 - rxdma_release_ring
 * @RXDMA2FW_RING: 1 - rxdma2fw_ring
 * @RXDMA2SW_RING: 2 - rxdma2sw_ring
 * @RXDMA2REO_RING: 3 - rxdma2reo_ring
 */
enum dp_mon_fp_phy_err_buf_dest {
	RXDMA_RELEASING_RING,
	RXDMA2FW_RING,
	RXDMA2SW_RING,
	RXDMA2REO_RING
};
#endif

/**
 * dp_mon_filters_reset() - reset all filters
 * @pdev: DP pdev handle
 */
void dp_mon_filters_reset(struct dp_pdev *pdev);

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_filter_setup_enhanced_stats() - Setup the enhanced stats filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_enhanced_stats(struct dp_pdev *pdev);

/***
 * dp_mon_filter_reset_enhanced_stats() - Reset the enhanced stats filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_enhanced_stats(struct dp_pdev *pdev);
#endif /* QCA_ENHANCED_STATS_SUPPORT */

#ifdef QCA_MCOPY_SUPPORT
/**
 * dp_mon_filter_setup_mcopy_mode() - Setup the m_copy mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mcopy_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_mcopy_mode() - Reset the m_copy mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mcopy_mode(struct dp_pdev *pdev);
#endif /* QCA_MCOPY_SUPPORT */

#ifdef QCA_UNDECODED_METADATA_SUPPORT
/**
 * dp_mon_filter_setup_undecoded_metadata_mode() - Setup the undecoded
 *  metadata capture mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_undecoded_metadata_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_undecoded_metadata_mode() - Reset the undecoded
 * metadata capture mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_undecoded_metadata_mode(struct dp_pdev *pdev);
#endif /* QCA_UNDECODED_METADATA_SUPPORT */

#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
/**
 * dp_mon_filter_setup_smart_monitor() - Setup the smart monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_smart_monitor(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_smart_monitor() - Reset the smart monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_smart_monitor(struct dp_pdev *pdev);
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */

/**
 * dp_mon_filter_set_reset_mon_mac_filter() - Setup the monitor mac filter
 * @pdev: DP pdev handle
 * @val: indicating set/reset
 */
void dp_mon_filter_set_reset_mon_mac_filter(struct dp_pdev *pdev, bool val);

#ifdef  WLAN_RX_PKT_CAPTURE_ENH
/**
 * dp_mon_filter_setup_rx_enh_capture() - Setup the Rx capture mode filters
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_enh_capture(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_enh_capture() - Reset the Rx capture mode filters
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_enh_capture(struct dp_pdev *pdev);
#endif /* WLAN_RX_PKT_CAPTURE_ENH */

/**
 * dp_mon_filter_setup_mon_mode() - Setup the Rx and Tx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mon_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_tx_mon_mode() - Setup the Tx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_tx_mon_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_mon_mode() - Reset the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mon_mode(struct dp_pdev *pdev);

#ifdef WDI_EVENT_ENABLE
/**
 * dp_mon_filter_setup_rx_pkt_log_full() - Setup the Rx pktlog full mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_full(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pkt_log_full() - Reset the Rx pktlog full mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_full(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_rx_pkt_log_lite() - Setup the Rx pktlog lite mode filter
 * in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_lite(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pkt_log_lite() - Reset the Rx pktlog lite mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_lite(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_rx_pkt_log_cbf() - Setup the Rx pktlog cbf mode filter
 * in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_cbf(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_rx_pktlog_cbf() - Reset the Rx pktlog cbf mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pktlog_cbf(struct dp_pdev *pdev);

#ifdef BE_PKTLOG_SUPPORT
/**
 * dp_mon_filter_setup_pktlog_hybrid() - Setup the pktlog hybrid mode filter
 * in the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_pktlog_hybrid(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_pktlog_hybrid() - Reset the pktlog hybrid mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_pktlog_hybrid(struct dp_pdev *pdev);
#endif
#endif /* WDI_EVENT_ENABLE */

/**
 * dp_mon_filter_set_status_cbf() - Set the cbf status filters
 * @pdev: DP pdev handle
 * @filter: Dp mon filters
 *
 * Return: void
 */
void dp_mon_filter_set_status_cbf(struct dp_pdev *pdev,
				  struct dp_mon_filter *filter);

/**
 * dp_mon_filter_update() - Setup the RX & Tx monitor filter setting for a srng
 * type
 * @pdev: DP pdev handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_mon_filter_update(struct dp_pdev *pdev);

/**
 * dp_mon_filter_dealloc() - Deallocate the filter objects to be stored in
 * the radio object.
 * @mon_pdev: monitor pdev handle
 */
void dp_mon_filter_dealloc(struct dp_mon_pdev *mon_pdev);

/**
 * dp_mon_filter_alloc() - Allocate the filter objects to be stored in
 * the radio object.
 * @mon_pdev: monitor pdev handle
 */
struct dp_mon_filter  **dp_mon_filter_alloc(struct dp_mon_pdev *mon_pdev);

/**
 * dp_mon_filter_show_filter() - Show the set filters
 * @mon_pdev: DP pdev handle
 * @mode: The filter modes
 * @filter: tlv filter
 */
void dp_mon_filter_show_filter(struct dp_mon_pdev *mon_pdev,
			       enum dp_mon_filter_mode mode,
			       struct dp_mon_filter *filter);

/**
 * dp_mon_ht2_rx_ring_cfg() - Send the tlv config to fw for a srng_type
 * based on target
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 * @srng_type: The srng type for which filter will be set
 * @tlv_filter: tlv filter
 */
QDF_STATUS
dp_mon_ht2_rx_ring_cfg(struct dp_soc *soc,
		       struct dp_pdev *pdev,
		       enum dp_mon_filter_srng_type srng_type,
		       struct htt_rx_ring_tlv_filter *tlv_filter);

/**
 * dp_mon_filter_reset_mon_srng()
 * @soc: DP SoC handle
 * @pdev: DP pdev handle
 * @mon_srng_type: Monitor srng type
 */
void
dp_mon_filter_reset_mon_srng(struct dp_soc *soc, struct dp_pdev *pdev,
			     enum dp_mon_filter_srng_type mon_srng_type);

/**
 * dp_mon_filter_set_mon_cmn() - Setp the common mon filters
 * @mon_pdev: DP pdev handle
 * @filter: DP mon filter
 *
 * Return: None
 */
void dp_mon_filter_set_mon_cmn(struct dp_pdev *pdev,
			       struct dp_mon_filter *filter);

/**
 * dp_mon_filter_set_status_cmn() - Setp the common status filters
 * @mon_pdev: DP pdev handle
 * @filter: Dp mon filters
 *
 * Return: QDF_STATUS
 */
void dp_mon_filter_set_status_cmn(struct dp_mon_pdev *mon_pdev,
				  struct dp_mon_filter *filter);

/**
 * dp_mon_filter_setup_mon_mode() - Setup the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mon_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_setup_tx_mon_mode() - Setup the Tx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_tx_mon_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_reset_mon_mode() - Reset the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mon_mode(struct dp_pdev *pdev);

/**
 * dp_mon_filter_set_cbf_cmn() - Set the common cbf mode filters
 * @pdev: DP pdev handle
 * @filter: Dp mon filters
 *
 * Return: void
 */
void dp_mon_filter_set_cbf_cmn(struct dp_pdev *pdev,
			       struct dp_mon_filter *filter);

/**
 * dp_tx_mon_filter_update() - Setup the tx monitor filter setting for a srng
 * type
 * @pdev: DP pdev handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_tx_mon_filter_update(struct dp_pdev *pdev);

/**
 * dp_mon_filter_dealloc() - Deallocate the filter objects to be stored in
 * the radio object.
 * @mon_pdev: DP pdev handle
 */
void dp_mon_filter_dealloc(struct dp_mon_pdev *mon_pdev);

/**
 * dp_mon_filter_alloc() - Allocate the filter objects to be stored in
 * the radio object.
 * @mon_pdev: DP pdev handle
 */
struct dp_mon_filter **dp_mon_filter_alloc(struct dp_mon_pdev *mon_pdev);

/**
 * dp_mon_filter_h2t_setup() - Setup filter
 * @soc: Dp soc handle
 * @pdev: pdev handle
 * @srng_type: srng type
 * @filter: filter
 */
void dp_mon_filter_h2t_setup(struct dp_soc *soc, struct dp_pdev *pdev,
			     enum dp_mon_filter_srng_type srng_type,
			     struct dp_mon_filter *filter);

/**
 * dp_mon_ht2_rx_ring_cfg() - Configure filter to HW
 * @soc: Dp soc handle
 * @pdev: Dp pdev handle
 * @srng_type: SRNG type
 * @tlv_filter: filter
 */
QDF_STATUS
dp_mon_ht2_rx_ring_cfg(struct dp_soc *soc,
		       struct dp_pdev *pdev,
		       enum dp_mon_filter_srng_type srng_type,
		       struct htt_rx_ring_tlv_filter *tlv_filter);
#endif /* #ifndef _DP_MON_FILTER_H_ */

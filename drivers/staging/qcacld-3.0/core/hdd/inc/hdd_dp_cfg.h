/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/**
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __HDD_DP_CONFIG_H
#define __HDD_DP_CONFIG_H

#define CFG_ENABLE_RX_THREAD		BIT(0)
#define CFG_ENABLE_RPS			BIT(1)
#define CFG_ENABLE_NAPI			BIT(2)
#define CFG_ENABLE_DYNAMIC_RPS		BIT(3)
#define CFG_ENABLE_DP_RX_THREADS	BIT(4)
#define CFG_RX_MODE_MAX (CFG_ENABLE_RX_THREAD | \
					  CFG_ENABLE_RPS | \
					  CFG_ENABLE_NAPI | \
					  CFG_ENABLE_DYNAMIC_RPS | \
					  CFG_ENABLE_DP_RX_THREADS)
#ifdef MDM_PLATFORM
#define CFG_RX_MODE_DEFAULT 0
#elif defined(HELIUMPLUS)
#define CFG_RX_MODE_DEFAULT CFG_ENABLE_NAPI
#endif

#ifndef CFG_RX_MODE_DEFAULT
#if defined(FEATURE_WLAN_DP_RX_THREADS)
#define CFG_RX_MODE_DEFAULT (CFG_ENABLE_DP_RX_THREADS | CFG_ENABLE_NAPI)
#else
#define CFG_RX_MODE_DEFAULT (CFG_ENABLE_RX_THREAD | CFG_ENABLE_NAPI)
#endif
#endif

/* Max # of packets to be processed in 1 tx comp loop */
#define CFG_DP_TX_COMP_LOOP_PKT_LIMIT_DEFAULT 64
#define CFG_DP_TX_COMP_LOOP_PKT_LIMIT_MAX (1024 * 1024)

/*Max # of packets to be processed in 1 rx reap loop */
#define CFG_DP_RX_REAP_LOOP_PKT_LIMIT_DEFAULT 64
#define CFG_DP_RX_REAP_LOOP_PKT_LIMIT_MAX (1024 * 1024)

/* Max # of HP OOS (out of sync) updates */
#define CFG_DP_RX_HP_OOS_UPDATE_LIMIT_DEFAULT 0
#define CFG_DP_RX_HP_OOS_UPDATE_LIMIT_MAX 1024

/* Max Yield time duration for RX Softirq */
#define CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS_DEFAULT (500 * 1000)
#define CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS_MAX (10 * 1000 * 1000)

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL

/*
 * <ini>
 * TxFlowLowWaterMark - Low watermark for pausing network queues
 *
 * @Min: 0
 * @Max: 1000
 * @Default: 300
 *
 * This ini specifies the low watermark of data packets transmitted
 * before pausing netif queues in tx flow path. It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowHighWaterMarkOffset, TxFlowMaxQueueDepth,
 *          TxLbwFlowLowWaterMark, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_FLOW_LWM \
		CFG_INI_UINT( \
		"TxFlowLowWaterMark", \
		0, \
		1000, \
		300, \
		CFG_VALUE_OR_DEFAULT, \
		"Low watermark for pausing network queues")

/*
 * <ini>
 * TxFlowHighWaterMarkOffset - High Watermark offset to unpause Netif queues
 * @Min: 0
 * @Max: 300
 * @Default: 94
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxFlowLowWaterMark. It is only applicable where legacy flow control
 * is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowMaxQueueDepth,
 *          TxLbwFlowLowWaterMark, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_FLOW_HWM_OFFSET \
		CFG_INI_UINT( \
		"TxFlowHighWaterMarkOffset", \
		0, \
		300, \
		94, \
		CFG_VALUE_OR_DEFAULT, \
		"High Watermark offset to unpause Netif queues")

/*
 * <ini>
 * TxFlowMaxQueueDepth - Max pause queue depth.
 *
 * @Min: 400
 * @Max: 3500
 * @Default: 1500
 *
 * This ini specifies the max queue pause depth.It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxLbwFlowLowWaterMark, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_FLOW_MAX_Q_DEPTH \
		CFG_INI_UINT( \
		"TxFlowMaxQueueDepth", \
		400, \
		3500, \
		1500, \
		CFG_VALUE_OR_DEFAULT, \
		"Max pause queue depth")

/*
 * <ini>
 * TxLbwFlowLowWaterMark - Low watermark for pausing network queues
 *                         in low bandwidth band
 * @Min: 0
 * @Max: 1000
 * @Default: 450
 *
 * This ini specifies the low watermark of data packets transmitted
 * before pausing netif queues in tx flow path in low bandwidth band.
 * It is only applicable where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_LBW_FLOW_LWM \
		CFG_INI_UINT( \
		"TxLbwFlowLowWaterMark", \
		0, \
		1000, \
		450, \
		CFG_VALUE_OR_DEFAULT, \
		"Low watermark for pausing network queues")

/*
 * <ini>
 * TxLbwFlowHighWaterMarkOffset - High Watermark offset to unpause Netif queues
 *                                in low bandwidth band.
 * @Min: 0
 * @Max: 300
 * @Default: 50
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxLbwFlowLowWaterMark in low bandwidth band. It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_LBW_FLOW_HWM_OFFSET \
		CFG_INI_UINT( \
		"TxLbwFlowHighWaterMarkOffset", \
		0, \
		300, \
		50, \
		CFG_VALUE_OR_DEFAULT, \
		"High Watermark offset to unpause Netif queues")

/*
 * <ini>
 * TxLbwFlowMaxQueueDepth - Max pause queue depth in low bandwidth band
 *
 * @Min: 400
 * @Max: 3500
 * @Default: 750
 *
 * This ini specifies the max queue pause depth in low bandwidth band.
 * It is only applicable where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_LBW_FLOW_MAX_Q_DEPTH \
		CFG_INI_UINT( \
		"TxLbwFlowMaxQueueDepth", \
		400, \
		3500, \
		750, \
		CFG_VALUE_OR_DEFAULT, \
		"Max pause queue depth in low bandwidth band")

/*
 * <ini>
 * TxHbwFlowLowWaterMark - Low watermark for pausing network queues
 *                         in high bandwidth band
 * @Min: 0
 * @Max: 1000
 * @Default: 406
 *
 * This ini specifies the threshold of data packets transmitted
 * before pausing netif queues.It is only applicable where
 * legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxLbwFlowMaxQueueDepth,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_HBW_FLOW_LWM \
		CFG_INI_UINT( \
		"TxHbwFlowLowWaterMark", \
		0, \
		1000, \
		406, \
		CFG_VALUE_OR_DEFAULT, \
		"Low watermark for pausing network queues")

/*
 * <ini>
 * TxHbwFlowHighWaterMarkOffset - High Watermark offset to unpause Netif queues
 *                                in high bandwidth band.
 * @Min: 0
 * @Max: 300
 * @Default: 94
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxHbwFlowLowWaterMark in high bandwidth band. It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxLbwFlowMaxQueueDepth,
 *          TxHbwFlowLowWaterMark, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_HBW_FLOW_HWM_OFFSET \
		CFG_INI_UINT( \
		"TxHbwFlowHighWaterMarkOffset", \
		0, \
		300, \
		94, \
		CFG_VALUE_OR_DEFAULT, \
		"High Watermark offset to unpause Netif queues")

/*
 * <ini>
 * TxHbwFlowMaxQueueDepth - Max pause queue depth in high bandwidth band
 * @Min: 4000
 * @Max: 3500
 * @Default: 1500
 *
 * This ini specifies the max queue pause depth in high bandwidth band.
 * It is only applicable where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxLbwFlowMaxQueueDepth,
 *          TxHbwFlowLowWaterMark, TxHbwFlowHighWaterMarkOffset
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_LL_TX_HBW_FLOW_MAX_Q_DEPTH \
		CFG_INI_UINT( \
		"TxHbwFlowMaxQueueDepth", \
		400, \
		3500, \
		1500, \
		CFG_VALUE_OR_DEFAULT, \
		"Max pause queue depth in high bandwidth band")

#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#ifdef WLAN_FEATURE_MSCS
/*
 * <ini>
 * mscs_pkt_threshold - Voice pkt count threshold
 *
 * @Min: 0
 * @Max: 10000
 * @Default: 1200
 *
 * This ini specifies the Voice pkt count threshold to
 * Send MSCS action frame to AP
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_VO_PKT_COUNT_THRESHOLD \
		CFG_INI_UINT( \
		"mscs_pkt_threshold", \
		0, \
		10000, \
		1200, \
		CFG_VALUE_OR_DEFAULT, \
		"Voice pkt count threshold")

/*
 * <ini>
 * mscs_voice_interval - mscs voice interval in sec
 *
 * @Min: 0
 * @Max: 300
 * @Default: 30
 *
 * This ini specifies the mscs voice interval to
 * monitor voice tx packet count to send MSCS action frame
 *
 * Related: mscs_pkt_threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MSCS_VOICE_INTERVAL \
		CFG_INI_UINT( \
		"mscs_voice_interval", \
		0, \
		300, \
		30, \
		CFG_VALUE_OR_DEFAULT, \
		"mscs voice interval")

#define CFG_MSCS_FEATURE_ALL \
		CFG(CFG_VO_PKT_COUNT_THRESHOLD) \
		CFG(CFG_MSCS_VOICE_INTERVAL)

#else
#define CFG_MSCS_FEATURE_ALL
#endif

/*
 * <ini>
 * NAPI_CPU_AFFINITY_MASK - CPU mask to affine NAPIs
 *
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0
 *
 * This ini is used to set NAPI IRQ CPU affinity
 *
 * Supported Feature: NAPI
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_NAPI_CE_CPU_MASK \
		CFG_INI_UINT( \
		"NAPI_CPU_AFFINITY_MASK", \
		0, \
		0xFF, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"CPU mask to affine NAPIs")

/*
 * <ini>
 * gWmiCreditCount - Credit count for WMI exchange
 * @0: Not allowed
 * @1: Serialize WMI commands, 1 command at a time
 * @Default: 2: As advertised by FW
 *
 * This ini is used to serialize the WMI commandsif required.
 *
 * Related: None
 *
 * Usage: External
 *
 * <ini>
 */
#define WLAN_CFG_WMI_CREDIT_DEFAULT	0
#define WLAN_CFG_WMI_CREDIT_MIN		1
#define WLAN_CFG_WMI_CREDIT_MAX		2

#define CFG_DP_HTC_WMI_CREDIT_CNT \
		CFG_INI_UINT("gWmiCreditCount", \
		 WLAN_CFG_WMI_CREDIT_MIN, \
		 WLAN_CFG_WMI_CREDIT_MAX, \
		 WLAN_CFG_WMI_CREDIT_DEFAULT, \
		 CFG_VALUE_OR_DEFAULT, "WMI HTC CREDIT COUNT")

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
#define CFG_HDD_DP_LEGACY_TX_FLOW \
	CFG(CFG_DP_LL_TX_FLOW_LWM) \
	CFG(CFG_DP_LL_TX_FLOW_HWM_OFFSET) \
	CFG(CFG_DP_LL_TX_FLOW_MAX_Q_DEPTH) \
	CFG(CFG_DP_LL_TX_LBW_FLOW_LWM) \
	CFG(CFG_DP_LL_TX_LBW_FLOW_HWM_OFFSET) \
	CFG(CFG_DP_LL_TX_LBW_FLOW_MAX_Q_DEPTH) \
	CFG(CFG_DP_LL_TX_HBW_FLOW_LWM) \
	CFG(CFG_DP_LL_TX_HBW_FLOW_HWM_OFFSET) \
	CFG(CFG_DP_LL_TX_HBW_FLOW_MAX_Q_DEPTH)
#else
#define CFG_HDD_DP_LEGACY_TX_FLOW
#endif

#ifdef FEATURE_ENABLE_CE_DP_IRQ_AFFINE
/*
 * <ini>
 * Enable_ce_dp_irq_affine - Enable/disable affinity on datapath CE IRQs
 *
 * @Min: 0
 * @Max: 1
 * Default: 0
 *
 * This ini param is used to enable/disable the affinity on datapath
 * Copy Engine IRQs.
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_CE_DP_IRQ_AFFINE CFG_INI_BOOL(\
			"Enable_ce_dp_irq_affine", \
			0, \
			"Enable/disable irq affinity on datapath CEs")
#define CFG_ENABLE_CE_DP_IRQ_AFFINE_ALL \
	CFG(CFG_ENABLE_CE_DP_IRQ_AFFINE)
#else
#define CFG_ENABLE_CE_DP_IRQ_AFFINE_ALL
#endif

#define CFG_HDD_DP_ALL \
	CFG(CFG_DP_NAPI_CE_CPU_MASK) \
	CFG(CFG_DP_HTC_WMI_CREDIT_CNT) \
	CFG_MSCS_FEATURE_ALL \
	CFG_HDD_DP_LEGACY_TX_FLOW \
	CFG_ENABLE_CE_DP_IRQ_AFFINE_ALL
#endif

/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef WLAN_DP_CFG_H__
#define WLAN_DP_CFG_H__

#define CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN 30

#ifdef CONFIG_DP_TRACE
/* Max length of gDptraceConfig string. e.g.- "1, 6, 1, 62" */
#define DP_TRACE_CONFIG_STRING_LENGTH		(20)

/* At max 4 DP Trace config parameters are allowed. Refer - gDptraceConfig */
#define DP_TRACE_CONFIG_NUM_PARAMS		(4)

/*
 * Default value of live mode in case it cannot be determined from cfg string
 * gDptraceConfig
 */
#define DP_TRACE_CONFIG_DEFAULT_LIVE_MODE	(1)

/*
 * Default value of thresh (packets/second) beyond which DP Trace is disabled.
 * Use this default in case the value cannot be determined from cfg string
 * gDptraceConfig
 */
#define DP_TRACE_CONFIG_DEFAULT_THRESH		(6)

/*
 * Number of intervals of BW timer to wait before enabling/disabling DP Trace.
 * Since throughput threshold to disable live logging for DP Trace is very low,
 * we calculate throughput based on # packets received in a second.
 * For example assuming bandwidth timer interval is 100ms, and if more than 6
 * prints are received in 10 * 100 ms interval, we want to disable DP Trace
 * live logging. DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT is the default
 * value, to be used in case the real value cannot be derived from
 * bw timer interval
 */
#define DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT (10)

/* Default proto bitmap in case its missing in gDptraceConfig string */
#define DP_TRACE_CONFIG_DEFAULT_BITMAP \
			(QDF_NBUF_PKT_TRAC_TYPE_EAPOL |\
			QDF_NBUF_PKT_TRAC_TYPE_DHCP |\
			QDF_NBUF_PKT_TRAC_TYPE_MGMT_ACTION |\
			QDF_NBUF_PKT_TRAC_TYPE_ARP |\
			QDF_NBUF_PKT_TRAC_TYPE_ICMP |\
			QDF_NBUF_PKT_TRAC_TYPE_ICMPv6)\

/* Default verbosity, in case its missing in gDptraceConfig string*/
#define DP_TRACE_CONFIG_DEFAULT_VERBOSTY QDF_DP_TRACE_VERBOSITY_LOW

#endif

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

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/*
 * <ini>
 * gBusBandwidthSuperHighThreshold - bus bandwidth super high threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 22000
 *
 * This ini specifies the bus bandwidth super high threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_SUPER_HIGH_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthSuperHighThreshold", \
		0, \
		4294967295UL, \
		22000, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth super high threshold")

/*
 * <ini>
 * gBusBandwidthUltraHighThreshold - bus bandwidth ultra high threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 12000
 *
 * This ini specifies the bus bandwidth very high threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_ULTRA_HIGH_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthUltraHighThreshold", \
		0, \
		4294967295UL, \
		12000, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth ultra high threshold")

/*
 * <ini>
 * gBusBandwidthVeryHighThreshold - bus bandwidth very high threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 10000
 *
 * This ini specifies the bus bandwidth very high threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_VERY_HIGH_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthVeryHighThreshold", \
		0, \
		4294967295UL, \
		10000, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth very high threshold")

/*
 * <ini>
 * gBusBandwidthMidHighThreshold - bus bandwidth high HE cases threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 0
 *
 * This ini specifies the bus bandwidth high HE cases threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_MID_HIGH_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthMidHighThreshold", \
		0, \
		4294967295UL, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth high threshold")

/*
 * <ini>
 * gBusBandwidthDBSThreshold - bus bandwidth for DBS mode threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 6000
 *
 * This ini specifies the bus bandwidth high threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_DBS_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthDBSThreshold", \
		0, \
		4294967295UL, \
		6000, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth DBS mode threshold")
/*
 * <ini>
 * gBusBandwidthHighThreshold - bus bandwidth high threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini specifies the bus bandwidth high threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_HIGH_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthHighThreshold", \
		0, \
		4294967295UL, \
		2000, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth high threshold")

/*
 * <ini>
 * gBusBandwidthMediumThreshold - bus bandwidth medium threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 500
 *
 * This ini specifies the bus bandwidth medium threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_MEDIUM_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthMediumThreshold", \
		0, \
		4294967295UL, \
		500, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth medium threshold")

/*
 * <ini>
 * gBusBandwidthLowThreshold - bus bandwidth low threshold
 *
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 150
 *
 * This ini specifies the bus bandwidth low threshold
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_LOW_THRESHOLD \
		CFG_INI_UINT( \
		"gBusBandwidthLowThreshold", \
		0, \
		4294967295UL, \
		150, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth low threshold")

/*
 * <ini>
 * gBusBandwidthComputeInterval - bus bandwidth compute interval
 *
 * @Min: 0
 * @Max: 10000
 * @Default: 100
 *
 * This ini specifies thebus bandwidth compute interval
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_BANDWIDTH_COMPUTE_INTERVAL \
		CFG_INI_UINT( \
		"gBusBandwidthComputeInterval", \
		0, \
		10000, \
		100, \
		CFG_VALUE_OR_DEFAULT, \
		"Bus bandwidth compute interval")

/*
 * <ini>
 * gTcpLimitOutputEnable - Control to enable TCP limit output byte
 * @Default: true
 *
 * This ini is used to enable dynamic configuration of TCP limit output bytes
 * tcp_limit_output_bytes param. Enabling this will let driver post message to
 * cnss-daemon, accordingly cnss-daemon will modify the tcp_limit_output_bytes.
 *
 * Supported Feature: Tcp limit output bytes
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TCP_LIMIT_OUTPUT \
		CFG_INI_BOOL( \
		"gTcpLimitOutputEnable", \
		true, \
		"Control to enable TCP limit output byte")

/*
 * <ini>
 * gTcpAdvWinScaleEnable - Control to enable  TCP adv window scaling
 * @Default: true
 *
 * This ini is used to enable dynamic configuration of TCP adv window scaling
 * system parameter.
 *
 * Supported Feature: Tcp Advance Window Scaling
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TCP_ADV_WIN_SCALE \
		CFG_INI_BOOL( \
		"gTcpAdvWinScaleEnable", \
		true, \
		"Control to enable  TCP adv window scaling")

/*
 * <ini>
 * gTcpDelAckEnable - Control to enable Dynamic Configuration of Tcp Delayed Ack
 * @Default: true
 *
 * This ini is used to enable Dynamic Configuration of Tcp Delayed Ack
 *
 * Related: gTcpDelAckThresholdHigh, gTcpDelAckThresholdLow,
 *          gTcpDelAckTimerCount
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TCP_DELACK \
		CFG_INI_BOOL( \
		"gTcpDelAckEnable", \
		true, \
		"Control to enable Dynamic Config of Tcp Delayed Ack")

/*
 * <ini>
 * gTcpDelAckThresholdHigh - High Threshold inorder to trigger TCP Del Ack
 *                                          indication
 * @Min: 0
 * @Max: 16000
 * @Default: 500
 *
 * This ini is used to mention the High Threshold inorder to trigger TCP Del Ack
 * indication i.e the threshold of packets received over a period of 100 ms.
 * i.e to have a low RX throughput requirement
 * Related: gTcpDelAckEnable, gTcpDelAckThresholdLow, gTcpDelAckTimerCount
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_TCP_DELACK_THRESHOLD_HIGH \
		CFG_INI_UINT( \
		"gTcpDelAckThresholdHigh", \
		0, \
		16000, \
		500, \
		CFG_VALUE_OR_DEFAULT, \
		"High Threshold inorder to trigger TCP Del Ack")

/*
 * <ini>
 * gTcpDelAckThresholdLow - Low Threshold inorder to trigger TCP Del Ack
 *                                          indication
 * @Min: 0
 * @Max: 10000
 * @Default: 1000
 *
 * This ini is used to mention the Low Threshold inorder to trigger TCP Del Ack
 * indication i.e the threshold of packets received over a period of 100 ms.
 * i.e to have a low RX throughput requirement
 *
 * Related: gTcpDelAckEnable, gTcpDelAckThresholdHigh, gTcpDelAckTimerCount
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_TCP_DELACK_THRESHOLD_LOW \
		CFG_INI_UINT( \
		"gTcpDelAckThresholdLow", \
		0, \
		10000, \
		1000, \
		CFG_VALUE_OR_DEFAULT, \
		"Low Threshold inorder to trigger TCP Del Ack")

/*
 * <ini>
 * gTcpDelAckTimerCount - Del Ack Timer Count inorder to trigger TCP Del Ack
 *                                      indication
 * @Min: 1
 * @Max: 1000
 * @Default: 30
 *
 * This ini is used to mention the Del Ack Timer Count inorder to
 * trigger TCP Del Ack indication i.e number of 100 ms periods
 *
 * Related: gTcpDelAckEnable, gTcpDelAckThresholdHigh, gTcpDelAckThresholdLow
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_TCP_DELACK_TIMER_COUNT \
		CFG_INI_UINT( \
		"gTcpDelAckTimerCount", \
		1, \
		1000, \
		30, \
		CFG_VALUE_OR_DEFAULT, \
		"Del Ack Timer Count inorder to trigger TCP Del Ack")

/*
 * <ini>
 * gTcpTxHighTputThreshold - High Threshold inorder to trigger High
 *                                          Tx Throughput requirement.
 * @Min: 0
 * @Max: 16000
 * @Default: 500
 *
 * This ini specifies the threshold of packets transmitted
 * over a period of 100 ms beyond which TCP can be considered to have a high
 * TX throughput requirement. The driver uses this condition to tweak TCP TX
 * specific parameters (via cnss-daemon)
 *
 * Supported Feature: To tweak TCP TX n/w parameters
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_TCP_TX_HIGH_TPUT_THRESHOLD \
		CFG_INI_UINT( \
		"gTcpTxHighTputThreshold", \
		0, \
		16000, \
		500, \
		CFG_VALUE_OR_DEFAULT, \
		"High Threshold inorder to trigger High Tx Tp")

/*
 * <ini>
 * gBusLowTputCntThreshold - Threshold count to trigger low Tput
 *			     GRO flush skip
 * @Min: 0
 * @Max: 200
 * @Default: 10
 *
 * This ini is a threshold that if count of times for bus Tput level
 * PLD_BUS_WIDTH_LOW in bus_bw_timer() >= this threshold, will enable skipping
 * GRO flush, current default threshold is 10, then will delay GRO flush-skip
 * 1 second for low Tput level.
 *
 * Supported Feature: GRO flush skip when low T-put
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_BUS_LOW_BW_CNT_THRESHOLD \
		CFG_INI_UINT( \
		"gBusLowTputCntThreshold", \
		0, \
		200, \
		10, \
		CFG_VALUE_OR_DEFAULT, \
		"Threshold to trigger GRO flush skip for low T-put")

/*
 * <ini>
 * gHandleLatencyCriticalClients - Enable the handling of latency critical
 *			     clients in bus bandwidth timer.
 * @Default: false
 *
 * This ini enables the handling of latency critical clients, eg: 11g/a
 * clients, when they are running their corresponding peak throughput.
 *
 * Supported Feature: Latency critical clients in host
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_BUS_HANDLE_LATENCY_CRITICAL_CLIENTS \
		CFG_INI_BOOL( \
		"gHandleLatencyCriticalClients", \
		false, \
		"Control to enable latency critical clients")

#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
/*
 * <ini>
 * gDriverDelAckHighThreshold - High Threshold inorder to trigger TCP
 *                              delay ack feature in the host.
 * @Min: 0
 * @Max: 70000
 * @Default: 300
 *
 * This ini specifies the threshold of RX packets transmitted
 * over a period of 100 ms beyond which TCP delay ack can be enabled
 * to improve TCP RX throughput requirement.
 *
 * Supported Feature: Tcp Delayed Ack in the host
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_DRIVER_TCP_DELACK_HIGH_THRESHOLD \
		CFG_INI_UINT( \
		"gDriverDelAckHighThreshold", \
		0, \
		70000, \
		300, \
		CFG_VALUE_OR_DEFAULT, \
		"TCP delack high threshold")

/*
 * <ini>
 * gDriverDelAckLowThreshold - Low Threshold inorder to disable TCP
 *                             delay ack feature in the host.
 * @Min: 0
 * @Max: 70000
 * @Default: 100
 *
 * This ini is used to mention the Low Threshold inorder to disable TCP Del
 * Ack feature in the host.
 *
 * Supported Feature: Tcp Delayed Ack in the host
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_DRIVER_TCP_DELACK_LOW_THRESHOLD \
		CFG_INI_UINT( \
		"gDriverDelAckLowThreshold", \
		0, \
		70000, \
		100, \
		CFG_VALUE_OR_DEFAULT, \
		"TCP delack low threshold")

/*
 * <ini>
 * gDriverDelAckTimerValue - Timeout value (ms) to send out all TCP del
 *                           ack frames
 * @Min: 1
 * @Max: 15
 * @Default: 3
 *
 * This ini specifies the time out value to send out all pending TCP delay
 * ACK frames.
 *
 * Supported Feature: Tcp Delayed Ack in the host
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_DRIVER_TCP_DELACK_TIMER_VALUE \
		CFG_INI_UINT( \
		"gDriverDelAckTimerValue", \
		1, \
		15, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"Send out all TCP Del Acks if time out")

/*
 * <ini>
 * gDriverDelAckPktCount - The maximum number of TCP delay ack frames
 * @Min: 0
 * @Max: 50
 * @Default: 20
 *
 * This ini specifies the maximum number of TCP delayed ack frames.
 *
 * Supported Feature: Tcp Delayed Ack in the host
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_DRIVER_TCP_DELACK_PKT_CNT \
		CFG_INI_UINT( \
		"gDriverDelAckPktCount", \
		0, \
		50, \
		20, \
		CFG_VALUE_OR_DEFAULT, \
		"No of TCP Del ACK count")

/*
 * <ini>
 * gDriverDelAckEnable - Control to enable Dynamic Configuration of Tcp
 *                       Delayed Ack in the host.
 * @Default: true
 *
 * This ini is used to enable Dynamic Configuration of Tcp Delayed Ack
 * in the host.
 *
 * Related: gDriverDelAckHighThreshold, gDriverDelAckLowThreshold,
 *          gDriverDelAckPktCount, gDriverDelAckTimerValue
 *
 * Supported Feature: Tcp Delayed Ack in the host
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_DRIVER_TCP_DELACK_ENABLE \
		CFG_INI_BOOL( \
		"gDriverDelAckEnable", \
		true, \
		"Enable tcp del ack in the driver")
#endif

/*
 * <ini>
 * RX_THREAD_CPU_AFFINITY_MASK - CPU mask to affine Rx_thread
 *
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0x02
 *
 * This ini is used to set Rx_thread CPU affinity
 *
 * Supported Feature: Rx_thread
 *
 * Usage: Internal
 *
 * </ini>
 */
#ifdef RX_PERFORMANCE
#define CFG_DP_RX_THREAD_CPU_MASK \
		CFG_INI_UINT( \
		"RX_THREAD_CPU_AFFINITY_MASK", \
		0, \
		0xFF, \
		0xFE, \
		CFG_VALUE_OR_DEFAULT, \
		"CPU mask to affine Rx_thread")
#else
#define CFG_DP_RX_THREAD_CPU_MASK \
		CFG_INI_UINT( \
		"RX_THREAD_CPU_AFFINITY_MASK", \
		0, \
		0xFF, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"CPU mask to affine Rx_thread")
#endif

/*
 * <ini>
 * RX_THREAD_UL_CPU_AFFINITY_MASK - CPU mask to affine Rx_thread
 *
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0x0
 *
 * This ini is used to set Rx_thread CPU affinity for uplink traffic
 *
 * Supported Feature: Rx_thread
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_THREAD_UL_CPU_MASK \
		CFG_INI_UINT( \
		"RX_THREAD_UL_CPU_AFFINITY_MASK", \
		0, \
		0xFF, \
		0x0, \
		CFG_VALUE_OR_DEFAULT, \
		"CPU mask to affine Rx_thread for uplink traffic")

/*
 * <ini>
 * rpsRxQueueCpuMapList - RPS map for different RX queues
 *
 * @Default: e
 *
 * This ini is used to set RPS map for different RX queues.
 *
 * List of RPS CPU maps for different rx queues registered by WLAN driver
 * Ref - Kernel/Documentation/networking/scaling.txt
 * RPS CPU map for a particular RX queue, selects CPU(s) for bottom half
 * processing of RX packets. For example, for a system with 4 CPUs,
 * 0xe: Use CPU1 - CPU3 and donot use CPU0.
 * 0x0: RPS is disabled, packets are processed on the interrupting CPU.
.*
 * WLAN driver registers NUM_TX_QUEUES queues for tx and rx each during
 * alloc_netdev_mq. Hence, we need to have a cpu mask for each of the rx queues.
 *
 * For example, if the NUM_TX_QUEUES is 4, a sample WLAN ini entry may look like
 * rpsRxQueueCpuMapList=a b c d
 * For a 4 CPU system (CPU0 - CPU3), this implies:
 * 0xa - (1010) use CPU1, CPU3 for rx queue 0
 * 0xb - (1011) use CPU0, CPU1 and CPU3 for rx queue 1
 * 0xc - (1100) use CPU2, CPU3 for rx queue 2
 * 0xd - (1101) use CPU0, CPU2 and CPU3 for rx queue 3

 * In practice, we may want to avoid the cores which are heavily loaded.
 *
 * Default value of rpsRxQueueCpuMapList. Different platforms may have
 * different configurations for NUM_TX_QUEUES and # of cpus, and will need to
 * configure an appropriate value via ini file. Setting default value to 'e' to
 * avoid use of CPU0 (since its heavily used by other system processes) by rx
 * queue 0, which is currently being used for rx packet processing.
 *
 * Maximum length of string used to hold a list of cpu maps for various rx
 * queues. Considering a 16 core system with 5 rx queues, a RPS CPU map
 * list may look like -
 * rpsRxQueueCpuMapList = ffff ffff ffff ffff ffff
 * (all 5 rx queues can be processed on all 16 cores)
 * max string len = 24 + 1(for '\0'). Considering 30 to be on safe side.
 *
 * Supported Feature: Rx_thread
 *
 * Usage: Internal
 * </ini>
 */
#define CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST \
		CFG_INI_STRING( \
		"rpsRxQueueCpuMapList", \
		1, \
		30, \
		"e", \
		"specify RPS map for different RX queues")

/*
 * <ini>
 * gEnableTxOrphan- Enable/Disable orphaning of Tx packets
 * @Default: false
 *
 * This ini is used to enable/disable orphaning of Tx packets.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_TX_ORPHAN_ENABLE \
		CFG_INI_BOOL( \
		"gEnableTxOrphan", \
		false, \
		"orphaning of Tx packets")

/*
 * <ini>
 * rx_mode - Control to decide rx mode for packet processing
 *
 * @Min: 0
 * @Max: (CFG_ENABLE_RX_THREAD | CFG_ENABLE_RPS | CFG_ENABLE_NAPI | \
 *	 CFG_ENABLE_DYNAMIC_RPS)
 *
 * Some possible configurations:
 * rx_mode=0 - Uses tasklets for bottom half
 * CFG_ENABLE_NAPI (rx_mode=4) - Uses NAPI for bottom half
 * CFG_ENABLE_RX_THREAD | CFG_ENABLE_NAPI (rx_mode=5) - NAPI for bottom half,
 * rx_thread for stack. Single threaded.
 * CFG_ENABLE_DP_RX_THREAD | CFG_ENABLE_NAPI (rx_mode=10) - NAPI for bottom
 * half, dp_rx_thread for stack processing. Supports multiple rx threads.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_MODE \
		CFG_INI_UINT("rx_mode", \
		0, CFG_RX_MODE_MAX, CFG_RX_MODE_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, \
		"Control to decide rx mode for packet processing")

/*
 * <ini>
 * tx_comp_loop_pkt_limit - Control to decide max # of packets to be processed
 *			    in 1 tx comp loop
 *
 * @Min: 8
 * @Max: CFG_DP_TX_COMP_LOOP_PKT_LIMIT_MAX
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_TX_COMP_LOOP_PKT_LIMIT \
		CFG_INI_UINT("tx_comp_loop_pkt_limit", \
		1, CFG_DP_TX_COMP_LOOP_PKT_LIMIT_MAX, \
		CFG_DP_TX_COMP_LOOP_PKT_LIMIT_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, \
		"Control to decide tx comp loop pkt limit")
/*
 * <ini>
 * rx_reap_loop_pkt_limit - Control to decide max # of packets to be reaped
 *			    in 1 dp_rx_process reap loop
 *
 * @Min: 8
 * @Max: CFG_DP_RX_REAP_LOOP_PKT_LIMIT_MAX
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_REAP_LOOP_PKT_LIMIT \
		CFG_INI_UINT("rx_reap_loop_pkt_limit", \
		0, CFG_DP_RX_REAP_LOOP_PKT_LIMIT_MAX, \
		CFG_DP_RX_REAP_LOOP_PKT_LIMIT_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, \
		"Control to decide rx reap loop packet limit")

/*
 * <ini>
 * rx_hp_oos_update_limit - Control to decide max # of HP OOS (out of sync)
 *			    updates
 *
 * @Min: 0
 * @Max: CFG_DP_RX_HP_OOS_UPDATE_LIMIT_MAX
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_HP_OOS_UPDATE_LIMIT \
		CFG_INI_UINT("rx_hp_oos_update_limit", \
		0, CFG_DP_RX_HP_OOS_UPDATE_LIMIT_MAX, \
		CFG_DP_RX_HP_OOS_UPDATE_LIMIT_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, \
		"Control to decide HP OOS update limit")

/*
 * <ini>
 * rx_softirq_max_yield_duration_ns - Control to decide max duration for RX
 *				      softirq
 *
 * @Min: 100 * 1000 , 100us
 * @Max: CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS_MAX
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS \
		CFG_INI_UINT("rx_softirq_max_yield_duration_ns", \
		100 * 1000, CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS_MAX, \
		CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, \
		"max yield time duration for RX Softirq")

/*
 * <ini>
 * enable_multicast_replay_filter - Enable filtering of replayed multicast
 * packets
 *
 * In a typical infrastructure setup, it is quite normal to receive
 * replayed multicast packets. These packets may cause more harm than
 * help if not handled properly. Providing a configuration option
 * to enable filtering of such packets
 *
 * </ini>
 */
#define CFG_DP_FILTER_MULTICAST_REPLAY \
	CFG_INI_BOOL("enable_multicast_replay_filter", \
	true, "Enable filtering of replayed multicast packets")

/*
 * <ini>
 * rx_wakelock_timeout - Amount of time to hold wakelock for RX unicast packets
 * @Min: 0
 * @Max: 100
 * @Default: 50
 *
 * This ini item configures the amount of time, in milliseconds, that the driver
 * should prevent system power collapse after receiving an RX unicast packet.
 * A conigured value of 0 disables the RX Wakelock feature completely.
 *
 * Related: None.
 *
 * Supported Feature: RX Wakelock
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DP_RX_WAKELOCK_TIMEOUT \
	CFG_INI_UINT("rx_wakelock_timeout", \
	0, 100, 50, CFG_VALUE_OR_DEFAULT, \
	"Amount of time to hold wakelock for RX unicast packets")

/*
 * <ini>
 * num_dp_rx_threads - Control to set the number of dp rx threads
 *
 * @Min: 1
 * @Max: 4
 * @Default: 1
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_NUM_DP_RX_THREADS \
	CFG_INI_UINT("num_dp_rx_threads", \
	1, 4, 1, CFG_VALUE_OR_DEFAULT, \
	"Control to set the number of dp rx threads")

/*
 * <ini>
 * ce_service_max_rx_ind_flush - Maximum number of HTT messages
 * to be processed per NAPI poll
 *
 * @Min: 1
 * @Max: 32
 * @Default: 1
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_CE_SERVICE_MAX_RX_IND_FLUSH \
		CFG_INI_UINT("ce_service_max_rx_ind_flush", \
		1, 32, 1, \
		CFG_VALUE_OR_DEFAULT, "Ctrl to set ce service max rx ind flsh")

/*
 * <ini>
 * ce_service_max_yield_time - Time in microseconds after which
 * a NAPI poll must yield
 *
 * @Min: 500
 * @Max: 10000
 * @Default: 500
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_CE_SERVICE_MAX_YIELD_TIME \
		CFG_INI_UINT("ce_service_max_yield_time", \
		500, 10000, 500, \
		CFG_VALUE_OR_DEFAULT, "Ctrl to set ce service max yield time")

#ifdef WLAN_FEATURE_FASTPATH
#define CFG_DP_ENABLE_FASTPATH \
		CFG_INI_BOOL("gEnableFastPath", \
		false, "Ctrl to enable fastpath feature")

#define CFG_DP_ENABLE_FASTPATH_ALL \
	CFG(CFG_DP_ENABLE_FASTPATH)
#else
#define CFG_DP_ENABLE_FASTPATH_ALL
#endif

#define CFG_DP_ENABLE_TCP_PARAM_UPDATE \
		CFG_INI_BOOL("enable_tcp_param_update", \
		false, "configure TCP param through Wi-Fi HAL")
/*
 * <ini>
 *
 * Enable/disable DPTRACE
 * Enabling this might have performance impact.
 *
 * Config DPTRACE
 * The sequence of params is important. If some param is missing, defaults are
 * considered.
 * Param 1: Enable/Disable DP Trace live mode (uint8_t)
 * Param 2: DP Trace live mode high bandwidth thresh.(uint8_t)
 *         (packets/second) beyond which DP Trace is disabled. Decimal Val.
 *          MGMT, DHCP, EAPOL, ARP pkts are not counted. ICMP and Data are.
 * Param 3: Default Verbosity (0-4)
 * Param 4: Proto Bitmap (uint8_t). Decimal Value.
 *          (decimal 62 = 0x3e)
 * e.g., to disable live mode, use the following param in the ini file.
 * gDptraceConfig = 0
 * e.g., to enable dptrace live mode and set the thresh as 6,
 * use the following param in the ini file.
 * gDptraceConfig = 1, 6
 *
 * </ini>
 */
#ifdef CONFIG_DP_TRACE
#define CFG_DP_ENABLE_DP_TRACE \
			CFG_INI_BOOL("enable_dp_trace", \
			true, "Ctrl to enable dp trace feature")

#define CFG_DP_DP_TRACE_CONFIG \
		CFG_INI_STRING( \
		"gDptraceConfig", \
		1, \
		20, \
		"1, 6, 2, 126", \
		"dp trace configuration string")

/*
 * <ini>
 * dp_proto_event_bitmap - Control for which protocol packet diag event should
 *  be sent to user space.
 * @Min: 0
 * @Max: 0x17
 * @Default: 0x6
 *
 * This ini is used to control for which protocol packet diag event should be
 * sent to user space.
 *
 * QDF_NBUF_PKT_TRAC_TYPE_DNS       0x01
 * QDF_NBUF_PKT_TRAC_TYPE_EAPOL     0x02
 * QDF_NBUF_PKT_TRAC_TYPE_DHCP      0x04
 * QDF_NBUF_PKT_TRAC_TYPE_ARP       0x10
 *
 * Related: None
 *
 * Supported Feature: STA, SAP
 *
 * Usage: Internal
 *
 * <ini>
 */
#define CFG_DP_PROTO_EVENT_BITMAP \
		CFG_INI_UINT("dp_proto_event_bitmap", \
		0, 0x17, 0x17, \
		CFG_VALUE_OR_DEFAULT, \
		"Control for which protocol type diag log should be sent")

#define CFG_DP_CONFIG_DP_TRACE_ALL \
		CFG(CFG_DP_ENABLE_DP_TRACE) \
		CFG(CFG_DP_DP_TRACE_CONFIG) \
		CFG(CFG_DP_PROTO_EVENT_BITMAP)
#else
#define CFG_DP_CONFIG_DP_TRACE_ALL
#endif

#ifdef WLAN_NUD_TRACKING
/*
 * <ini>
 * gEnableNUDTracking - Will enable or disable NUD tracking within driver
 * @Min: 0
 * @Max: 3
 * @Default: 2
 *
 * This ini is used to specify the behaviour of the driver for NUD tracking.
 * If the ini value is:-
 * 0: Driver will not track the NUD failures, and ignore the same.
 * 1: Driver will track the NUD failures and if honoured will disconnect from
 * the connected BSSID.
 * 2: Driver will track the NUD failures and if honoured will roam away from
 * the connected BSSID to a new BSSID to retain the data connectivity.
 * 3: Driver will try to roam to a new AP but if roam fails, disconnect.
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * <ini>
 */
#define CFG_DP_ENABLE_NUD_TRACKING \
		CFG_INI_UINT("gEnableNUDTracking", \
		 0, \
		 3, \
		 2, \
		 CFG_VALUE_OR_DEFAULT, "Driver NUD tracking behaviour")

#define CFG_DP_ENABLE_NUD_TRACKING_ALL \
			CFG(CFG_DP_ENABLE_NUD_TRACKING)
#else
#define CFG_DP_ENABLE_NUD_TRACKING_ALL
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE

#define CFG_DP_HL_BUNDLE_HIGH_TH \
		CFG_INI_UINT( \
		"tx_bundle_high_threashold", \
		0, \
		70000, \
		4330, \
		CFG_VALUE_OR_DEFAULT, \
		"tx bundle high threashold")

#define CFG_DP_HL_BUNDLE_LOW_TH \
		CFG_INI_UINT( \
		"tx_bundle_low_threashold", \
		0, \
		70000, \
		4000, \
		CFG_VALUE_OR_DEFAULT, \
		"tx bundle low threashold")

#define CFG_DP_HL_BUNDLE_TIMER_VALUE \
		CFG_INI_UINT( \
		"tx_bundle_timer_in_ms", \
		10, \
		10000, \
		100, \
		CFG_VALUE_OR_DEFAULT, \
		"tx bundle timer value in ms")

#define CFG_DP_HL_BUNDLE_SIZE \
		CFG_INI_UINT( \
		"tx_bundle_size", \
		0, \
		64, \
		16, \
		CFG_VALUE_OR_DEFAULT, \
		"tx bundle size")

#endif

#define WLAN_CFG_ICMP_REQ_TO_FW_MARK_ALL (-1)
#define WLAN_CFG_ICMP_REQ_TO_FW_MARK_INTERVAL 0
#define WLAN_CFG_ICMP_REQ_TO_FW_MARK_INTERVAL_MIN (-1)
#define WLAN_CFG_ICMP_REQ_TO_FW_MARK_INTERVAL_MAX 100000

/*
 * <ini>
 * icmp_req_to_fw_mark_interval - Interval to mark the ICMP Request packet
 *				  to be sent to FW.
 * @Min: -1
 * @Max:  100000
 * @Default: 0
 *
 * This ini is used to control DP Software to mark the ICMP request packets
 * to be sent to FW at certain interval (in milliseconds).
 * The value 0 is used to disable marking of ICMP requests to be sent to FW.
 * The value -1 is used to mark all the ICMP requests to be sent to FW.
 * Any value greater than zero indicates the time interval (in milliseconds)
 * at which ICMP requests are marked to be sent to FW.
 *
 * Supported modes: All modes
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_ICMP_REQ_TO_FW_MARK_INTERVAL \
	CFG_INI_INT("icmp_req_to_fw_mark_interval", \
		    WLAN_CFG_ICMP_REQ_TO_FW_MARK_INTERVAL_MIN, \
		    WLAN_CFG_ICMP_REQ_TO_FW_MARK_INTERVAL_MAX, \
		    WLAN_CFG_ICMP_REQ_TO_FW_MARK_INTERVAL, \
		    CFG_VALUE_OR_DEFAULT, \
		    "Interval to mark ICMP Request packets to be sent to FW")

/*
 * <ini>
 * enable_direct_link_ut_cmd - Enable direct link unit testing
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable direct link unit test
 *
 * Supported feature: Direct link
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_DIRECT_LINK_UT_CMD \
	CFG_INI_BOOL("enable_direct_link_ut_cmd", false, \
		     "enable/disable direct link unit test")

/*
 * <ini>
 * dp_apply_mem_profile - Apply mem profile config
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to apply DP mem profile config
 *
 * Supported feature: All modes
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_APPLY_MEM_PROFILE \
	CFG_INI_BOOL("dp_apply_mem_profile", false, \
		     "enable/disable dp mem profile")

/*TODO Flow control part to be moved to DP later*/

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#define CFG_DP_BUS_BANDWIDTH \
	CFG(CFG_DP_BUS_BANDWIDTH_SUPER_HIGH_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_ULTRA_HIGH_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_VERY_HIGH_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_MID_HIGH_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_DBS_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_HIGH_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_MEDIUM_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_LOW_THRESHOLD) \
	CFG(CFG_DP_BUS_BANDWIDTH_COMPUTE_INTERVAL) \
	CFG(CFG_DP_ENABLE_TCP_LIMIT_OUTPUT) \
	CFG(CFG_DP_ENABLE_TCP_ADV_WIN_SCALE) \
	CFG(CFG_DP_ENABLE_TCP_DELACK) \
	CFG(CFG_DP_TCP_DELACK_THRESHOLD_HIGH) \
	CFG(CFG_DP_TCP_DELACK_THRESHOLD_LOW) \
	CFG(CFG_DP_TCP_DELACK_TIMER_COUNT) \
	CFG(CFG_DP_TCP_TX_HIGH_TPUT_THRESHOLD) \
	CFG(CFG_DP_BUS_LOW_BW_CNT_THRESHOLD) \
	CFG(CFG_DP_BUS_HANDLE_LATENCY_CRITICAL_CLIENTS)

#else
#define CFG_DP_BUS_BANDWIDTH
#endif

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
#define CFG_DP_DRIVER_TCP_DELACK \
	CFG(CFG_DP_DRIVER_TCP_DELACK_HIGH_THRESHOLD) \
	CFG(CFG_DP_DRIVER_TCP_DELACK_LOW_THRESHOLD) \
	CFG(CFG_DP_DRIVER_TCP_DELACK_TIMER_VALUE) \
	CFG(CFG_DP_DRIVER_TCP_DELACK_PKT_CNT) \
	CFG(CFG_DP_DRIVER_TCP_DELACK_ENABLE)
#else
#define CFG_DP_DRIVER_TCP_DELACK
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
#define CFG_DP_HL_BUNDLE \
	CFG(CFG_DP_HL_BUNDLE_HIGH_TH) \
	CFG(CFG_DP_HL_BUNDLE_LOW_TH) \
	CFG(CFG_DP_HL_BUNDLE_TIMER_VALUE) \
	CFG(CFG_DP_HL_BUNDLE_SIZE)
#else
#define CFG_DP_HL_BUNDLE
#endif

#define CFG_DP_ALL \
	CFG(CFG_DP_RX_THREAD_CPU_MASK) \
	CFG(CFG_DP_RX_THREAD_UL_CPU_MASK) \
	CFG(CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST) \
	CFG(CFG_DP_TX_ORPHAN_ENABLE) \
	CFG(CFG_DP_RX_MODE) \
	CFG(CFG_DP_TX_COMP_LOOP_PKT_LIMIT)\
	CFG(CFG_DP_RX_REAP_LOOP_PKT_LIMIT)\
	CFG(CFG_DP_RX_HP_OOS_UPDATE_LIMIT)\
	CFG(CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS)\
	CFG(CFG_DP_CE_SERVICE_MAX_RX_IND_FLUSH) \
	CFG(CFG_DP_CE_SERVICE_MAX_YIELD_TIME) \
	CFG(CFG_DP_ENABLE_TCP_PARAM_UPDATE) \
	CFG(CFG_DP_FILTER_MULTICAST_REPLAY) \
	CFG(CFG_DP_RX_WAKELOCK_TIMEOUT) \
	CFG(CFG_DP_NUM_DP_RX_THREADS) \
	CFG(CFG_DP_ICMP_REQ_TO_FW_MARK_INTERVAL) \
	CFG(CFG_ENABLE_DIRECT_LINK_UT_CMD) \
	CFG(CFG_DP_APPLY_MEM_PROFILE) \
	CFG_DP_ENABLE_FASTPATH_ALL \
	CFG_DP_BUS_BANDWIDTH \
	CFG_DP_DRIVER_TCP_DELACK \
	CFG_DP_ENABLE_NUD_TRACKING_ALL \
	CFG_DP_CONFIG_DP_TRACE_ALL \
	CFG_DP_HL_BUNDLE

#endif /* WLAN_DP_CFG_H__ */

/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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

#if !defined(__QDF_TRACE_H)
#define __QDF_TRACE_H

/**
 *  DOC: qdf_trace
 *  QCA driver framework trace APIs
 *  Trace, logging, and debugging definitions and APIs
 */

/* Include Files */
#include  <qdf_types.h>         /* For QDF_MODULE_ID... */
#include  <qdf_status.h>
#include  <qdf_nbuf.h>
#include  <i_qdf_types.h>
#include <qdf_debugfs.h>


/* Type declarations */

#ifdef LOG_LINE_NUMBER
#define FL(x)    "%s: %d: " x, __func__, __LINE__
#else
#define FL(x)    "%s: " x, __func__
#endif

#define QDF_TRACE_BUFFER_SIZE (512)

/*
 * Extracts the 8-bit group id from the wmi command id by performing the
 * reverse operation of WMI_CMD_GRP_START_ID
 */
#define QDF_WMI_MTRACE_GRP_ID(message_id) (((message_id) >> 12) & 0xFF)
/*
 * Number of bits reserved for WMI mtrace command id
 */
 #define QDF_WMI_MTRACE_CMD_NUM_BITS 7
/*
 * Extracts the 7-bit group specific command id from the wmi command id
 */
#define QDF_WMI_MTRACE_CMD_ID(message_id) ((message_id) & 0x7F)

#ifdef QDF_TRACE_PRINT_ENABLE
#define QDF_DEFAULT_TRACE_LEVEL (1 << QDF_TRACE_LEVEL_INFO)
#endif

#define QDF_CATEGORY_INFO_U16(val) (((val >> 16) & 0x0000FFFF))
#define QDF_TRACE_LEVEL_INFO_L16(val) (val & 0x0000FFFF)

typedef int (qdf_abstract_print)(void *priv, const char *fmt, ...);

/*
 * Log levels
 */
#define QDF_DEBUG_FUNCTRACE     0x01
#define QDF_DEBUG_LEVEL0        0x02
#define QDF_DEBUG_LEVEL1        0x04
#define QDF_DEBUG_LEVEL2        0x08
#define QDF_DEBUG_LEVEL3        0x10
#define QDF_DEBUG_ERROR         0x20
#define QDF_DEBUG_CFG           0x40

/*
 * Rate limit based on pkt prototype
 */
#define QDF_MAX_DHCP_PKTS_PER_SEC       (20)
#define QDF_MAX_EAPOL_PKTS_PER_SEC      (50)
#define QDF_MAX_ARP_PKTS_PER_SEC        (5)
#define QDF_MAX_DNS_PKTS_PER_SEC        (5)
#define QDF_MAX_OTHER_PKTS_PER_SEC      (1)

/* DP Trace Implementation */
#ifdef CONFIG_DP_TRACE
#define DPTRACE(p) p
#define DPTRACE_PRINT(args...) \
	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG, args)
#else
#define DPTRACE(p)
#define DPTRACE_PRINT(args...)
#endif

/* By default Data Path module will have all log levels enabled, except debug
 * log level. Debug level will be left up to the framework or user space modules
 * to be enabled when issue is detected
 */
#define QDF_DATA_PATH_TRACE_LEVEL \
	((1 << QDF_TRACE_LEVEL_FATAL) | (1 << QDF_TRACE_LEVEL_ERROR) | \
	(1 << QDF_TRACE_LEVEL_WARN) | (1 << QDF_TRACE_LEVEL_INFO) | \
	(1 << QDF_TRACE_LEVEL_INFO_HIGH) | (1 << QDF_TRACE_LEVEL_INFO_MED) | \
	(1 << QDF_TRACE_LEVEL_INFO_LOW))

/* Preprocessor definitions and constants */
#define ASSERT_BUFFER_SIZE (512)

#ifndef MAX_QDF_TRACE_RECORDS
#define MAX_QDF_TRACE_RECORDS 4000
#endif

#define QDF_TRACE_DEFAULT_PDEV_ID 0xff
#define INVALID_QDF_TRACE_ADDR 0xffffffff
#define DEFAULT_QDF_TRACE_DUMP_COUNT 0
#define QDF_TRACE_DEFAULT_MSDU_ID 0

/*
 * first parameter to iwpriv command - dump_dp_trace
 * iwpriv wlan0 dump_dp_trace 0 0 -> dump full buffer
 * iwpriv wlan0 dump_dp_trace 1 0 -> enable live view mode
 * iwpriv wlan0 dump_dp_trace 2 0 -> clear dp trace buffer
 * iwpriv wlan0 dump_dp_trace 3 0 -> disable live view mode
 */
#define DUMP_DP_TRACE			0
#define ENABLE_DP_TRACE_LIVE_MODE	1
#define CLEAR_DP_TRACE_BUFFER		2
#define DISABLE_DP_TRACE_LIVE_MODE	3


#ifdef TRACE_RECORD

#define MTRACE(p) p

#else
#define MTRACE(p) do { } while (0)

#endif
#define NO_SESSION 0xFF

/**
 * struct qdf_trace_record_s - keep trace record
 * @qtime: qtimer ticks
 * @time: user timestamp
 * @module: module name
 * @code: hold record of code
 * @session: hold record of session
 * @data: hold data
 * @pid: hold pid of the process
 */
typedef struct qdf_trace_record_s {
	uint64_t qtime;
	char time[18];
	uint8_t module;
	uint16_t code;
	uint16_t session;
	uint32_t data;
	uint32_t pid;
} qdf_trace_record_t, *tp_qdf_trace_record;

/**
 * struct s_qdf_trace_data - MTRACE logs are stored in ring buffer
 * @head: position of first record
 * @tail: position of last record
 * @num: count of total record
 * @num_since_last_dump: count from last dump
 * @enable: config for controlling the trace
 * @dump_count: Dump after number of records reach this number
 */
typedef struct s_qdf_trace_data {
	uint32_t head;
	uint32_t tail;
	uint32_t num;
	uint16_t num_since_last_dump;
	uint8_t enable;
	uint16_t dump_count;
} t_qdf_trace_data;

#ifdef CONNECTIVITY_DIAG_EVENT
/**
 * enum diag_dp_tx_rx_status - TX/RX packet status
 * @DIAG_TX_RX_STATUS_INVALID: default invalid status
 * @DIAG_TX_RX_STATUS_OK: successfully sent + acked
 * @DIAG_TX_RX_STATUS_FW_DISCARD: queued but not sent over air
 * @DIAG_TX_RX_STATUS_NO_ACK: packet sent but no ack received
 * @DIAG_TX_RX_STATUS_DROP: packet dropped due to congestion
 * @DIAG_TX_RX_STATUS_DOWNLOAD_SUCC: packet delivered to target
 * @DIAG_TX_RX_STATUS_DEFAULT: default status
 * @DIAG_TX_RX_STATUS_MAX:
 */
enum diag_dp_tx_rx_status {
	DIAG_TX_RX_STATUS_INVALID,
	DIAG_TX_RX_STATUS_OK,
	DIAG_TX_RX_STATUS_FW_DISCARD,
	DIAG_TX_RX_STATUS_NO_ACK,
	DIAG_TX_RX_STATUS_DROP,
	DIAG_TX_RX_STATUS_DOWNLOAD_SUCC,
	DIAG_TX_RX_STATUS_DEFAULT,
	DIAG_TX_RX_STATUS_MAX
};

/**
 * enum diag_tx_status - Used by attribute
 * @DIAG_TX_STATUS_FAIL: Indicates frame is not sent over the air.
 * @DIAG_TX_STATUS_NO_ACK: Indicates packet sent but acknowledgment
 * is not received.
 * @DIAG_TX_STATUS_ACK: Indicates the frame is successfully sent and
 * acknowledged.
 */
enum diag_tx_status {
	DIAG_TX_STATUS_FAIL = 1,
	DIAG_TX_STATUS_NO_ACK = 2,
	DIAG_TX_STATUS_ACK = 3
};

/**
 * wlan_get_diag_tx_status() - Gives the diag logging specific tx status
 * @tx_status: fw specific TX status
 *
 * Returns TX status specified in enum diag_tx_status
 */
enum diag_tx_status wlan_get_diag_tx_status(enum qdf_dp_tx_rx_status tx_status);
#endif

#define CASE_RETURN_STRING(str) case ((str)): return (uint8_t *)(# str);

#ifndef MAX_QDF_DP_TRACE_RECORDS
#define MAX_QDF_DP_TRACE_RECORDS       2000
#endif

#define QDF_DP_TRACE_RECORD_SIZE       66 /* bytes */
#define INVALID_QDF_DP_TRACE_ADDR      0xffffffff
#define QDF_DP_TRACE_VERBOSITY_HIGH		4
#define QDF_DP_TRACE_VERBOSITY_MEDIUM		3
#define QDF_DP_TRACE_VERBOSITY_LOW		2
#define QDF_DP_TRACE_VERBOSITY_ULTRA_LOW	1
#define QDF_DP_TRACE_VERBOSITY_BASE		0

/**
 * enum QDF_DP_TRACE_ID - Generic ID to identify various events in data path
 * @QDF_DP_TRACE_INVALID: invalid
 * @QDF_DP_TRACE_DROP_PACKET_RECORD: record drop packet
 * @QDF_DP_TRACE_EAPOL_PACKET_RECORD: record EAPOL packet
 * @QDF_DP_TRACE_DHCP_PACKET_RECORD: record DHCP packet
 * @QDF_DP_TRACE_ARP_PACKET_RECORD: record ARP packet
 * @QDF_DP_TRACE_MGMT_PACKET_RECORD: record MGMT pacekt
 * @QDF_DP_TRACE_EVENT_RECORD: record events
 * @QDF_DP_TRACE_BASE_VERBOSITY: below this are part of base verbosity
 * @QDF_DP_TRACE_ICMP_PACKET_RECORD: record ICMP packet
 * @QDF_DP_TRACE_ICMPv6_PACKET_RECORD: record ICMPv6 packet
 * @QDF_DP_TRACE_HDD_TX_TIMEOUT: HDD tx timeout
 * @QDF_DP_TRACE_HDD_SOFTAP_TX_TIMEOUT: SOFTAP HDD tx timeout
 * @QDF_DP_TRACE_TX_CREDIT_RECORD: credit update record
 * @QDF_DP_TRACE_ULTRA_LOW_VERBOSITY: Below this is not logged for >4PPS
 * @QDF_DP_TRACE_TX_PACKET_RECORD: record 32 bytes of tx pkt at any layer
 * @QDF_DP_TRACE_RX_PACKET_RECORD: record 32 bytes of rx pkt at any layer
 * @QDF_DP_TRACE_HDD_TX_PACKET_RECORD: record 32 bytes of tx pkt at HDD
 * @QDF_DP_TRACE_HDD_RX_PACKET_RECORD: record 32 bytes of rx pkt at HDD
 * @QDF_DP_TRACE_LI_DP_TX_PACKET_RECORD: record data bytes of tx pkt at LI_DP
 * @QDF_DP_TRACE_LI_DP_RX_PACKET_RECORD: record data bytes of rx pkt at LI_DP
 * @QDF_DP_TRACE_LI_DP_FREE_PACKET_PTR_RECORD: tx completion ptr record for
 *						lithium
 * @QDF_DP_TRACE_FREE_PACKET_PTR_RECORD: tx completion ptr record
 * @QDF_DP_TRACE_LOW_VERBOSITY: below this are part of low verbosity
 * @QDF_DP_TRACE_HDD_TX_PACKET_PTR_RECORD: HDD layer ptr record
 * @QDF_DP_TRACE_TX_PACKET_PTR_RECORD: DP component Tx ptr record
 * @QDF_DP_TRACE_LI_DP_TX_PACKET_PTR_RECORD: Lithium DP layer ptr record
 * @QDF_DP_TRACE_RX_PACKET_PTR_RECORD: DP component Rx ptr record
 * @QDF_DP_TRACE_RX_HDD_PACKET_PTR_RECORD: HDD RX record
 * @QDF_DP_TRACE_CE_PACKET_PTR_RECORD: CE layer ptr record
 * @QDF_DP_TRACE_CE_FAST_PACKET_PTR_RECORD: CE fastpath ptr record
 * @QDF_DP_TRACE_CE_FAST_PACKET_ERR_RECORD: CE fastpath error record
 * @QDF_DP_TRACE_RX_HTT_PACKET_PTR_RECORD: HTT RX record
 * @QDF_DP_TRACE_RX_OFFLOAD_HTT_PACKET_PTR_RECORD: HTT RX offload record
 * @QDF_DP_TRACE_RX_LI_DP_PACKET_PTR_RECORD: Lithium DP RX record
 * @QDF_DP_TRACE_MED_VERBOSITY: below this are part of med verbosity
 * @QDF_DP_TRACE_TXRX_QUEUE_PACKET_PTR_RECORD: tx queue ptr record
 * @QDF_DP_TRACE_TXRX_PACKET_PTR_RECORD: txrx packet ptr record
 * @QDF_DP_TRACE_TXRX_FAST_PACKET_PTR_RECORD: txrx fast path record
 * @QDF_DP_TRACE_HTT_PACKET_PTR_RECORD: htt packet ptr record
 * @QDF_DP_TRACE_HTC_PACKET_PTR_RECORD: htc packet ptr record
 * @QDF_DP_TRACE_HIF_PACKET_PTR_RECORD: hif packet ptr record
 * @QDF_DP_TRACE_RX_TXRX_PACKET_PTR_RECORD: txrx packet ptr record
 * @QDF_DP_TRACE_LI_DP_NULL_RX_PACKET_RECORD:
 *		record data bytes of rx null_queue pkt at LI_DP
 * @QDF_DP_TRACE_HIGH_VERBOSITY: below this are part of high verbosity
 * @QDF_DP_TRACE_MAX: Max enumeration
 */

enum  QDF_DP_TRACE_ID {
	QDF_DP_TRACE_INVALID,
	QDF_DP_TRACE_DROP_PACKET_RECORD,
	QDF_DP_TRACE_EAPOL_PACKET_RECORD,
	QDF_DP_TRACE_DHCP_PACKET_RECORD,
	QDF_DP_TRACE_ARP_PACKET_RECORD,
	QDF_DP_TRACE_MGMT_PACKET_RECORD,
	QDF_DP_TRACE_EVENT_RECORD,
	QDF_DP_TRACE_BASE_VERBOSITY,
	QDF_DP_TRACE_ICMP_PACKET_RECORD,
	QDF_DP_TRACE_ICMPv6_PACKET_RECORD,
	QDF_DP_TRACE_HDD_TX_TIMEOUT,
	QDF_DP_TRACE_HDD_SOFTAP_TX_TIMEOUT,
	QDF_DP_TRACE_TX_CREDIT_RECORD,
	QDF_DP_TRACE_ULTRA_LOW_VERBOSITY,
	QDF_DP_TRACE_TX_PACKET_RECORD,
	QDF_DP_TRACE_RX_PACKET_RECORD,
	QDF_DP_TRACE_HDD_TX_PACKET_RECORD,
	QDF_DP_TRACE_HDD_RX_PACKET_RECORD,
	QDF_DP_TRACE_LI_DP_TX_PACKET_RECORD,
	QDF_DP_TRACE_LI_DP_RX_PACKET_RECORD,
	QDF_DP_TRACE_LI_DP_FREE_PACKET_PTR_RECORD,
	QDF_DP_TRACE_FREE_PACKET_PTR_RECORD,
	QDF_DP_TRACE_LOW_VERBOSITY,
	QDF_DP_TRACE_HDD_TX_PACKET_PTR_RECORD,
	QDF_DP_TRACE_TX_PACKET_PTR_RECORD,
	QDF_DP_TRACE_LI_DP_TX_PACKET_PTR_RECORD,
	QDF_DP_TRACE_RX_PACKET_PTR_RECORD,
	QDF_DP_TRACE_RX_HDD_PACKET_PTR_RECORD,
	QDF_DP_TRACE_CE_PACKET_PTR_RECORD,
	QDF_DP_TRACE_CE_FAST_PACKET_PTR_RECORD,
	QDF_DP_TRACE_CE_FAST_PACKET_ERR_RECORD,
	QDF_DP_TRACE_RX_HTT_PACKET_PTR_RECORD,
	QDF_DP_TRACE_RX_OFFLOAD_HTT_PACKET_PTR_RECORD,
	QDF_DP_TRACE_RX_LI_DP_PACKET_PTR_RECORD,
	QDF_DP_TRACE_MED_VERBOSITY,
	QDF_DP_TRACE_TXRX_QUEUE_PACKET_PTR_RECORD,
	QDF_DP_TRACE_TXRX_PACKET_PTR_RECORD,
	QDF_DP_TRACE_TXRX_FAST_PACKET_PTR_RECORD,
	QDF_DP_TRACE_HTT_PACKET_PTR_RECORD,
	QDF_DP_TRACE_HTC_PACKET_PTR_RECORD,
	QDF_DP_TRACE_HIF_PACKET_PTR_RECORD,
	QDF_DP_TRACE_RX_TXRX_PACKET_PTR_RECORD,
	QDF_DP_TRACE_LI_DP_NULL_RX_PACKET_RECORD,
	QDF_DP_TRACE_HIGH_VERBOSITY,
	QDF_DP_TRACE_MAX
};

/**
 * enum qdf_proto_dir - direction
 * @QDF_TX: TX direction
 * @QDF_RX: RX direction
 * @QDF_NA: not applicable
 */
enum qdf_proto_dir {
	QDF_TX,
	QDF_RX,
	QDF_NA
};

/**
 * enum QDF_CREDIT_UPDATE_SOURCE - source of credit record
 * @QDF_TX_SCHED: Tx scheduler
 * @QDF_TX_COMP: TX completion
 * @QDF_TX_CREDIT_UPDATE: credit update indication
 * @QDF_HTT_ATTACH: HTT attach
 * @QDF_TX_HTT_MSG: HTT TX message
 */
enum QDF_CREDIT_UPDATE_SOURCE {
	QDF_TX_SCHED,
	QDF_TX_COMP,
	QDF_TX_CREDIT_UPDATE,
	QDF_HTT_ATTACH,
	QDF_TX_HTT_MSG
};

/**
 * enum QDF_CREDIT_OPERATION - operation on credit
 * @QDF_CREDIT_INC: credit increment
 * @QDF_CREDIT_DEC: credit decrement
 * @QDF_CREDIT_ABS: Abosolute credit
 * @QDF_OP_NA: Not applicable
 */
enum QDF_CREDIT_OPERATION {
	QDF_CREDIT_INC,
	QDF_CREDIT_DEC,
	QDF_CREDIT_ABS,
	QDF_OP_NA
};

/**
 * struct qdf_dp_trace_ptr_buf - pointer record buffer
 * @cookie: cookie value
 * @msdu_id: msdu_id
 * @status: completion status
 */
struct qdf_dp_trace_ptr_buf {
	uint64_t cookie;
	uint16_t msdu_id;
	uint16_t status;
};

/**
 * struct qdf_dp_trace_proto_cmn - common info in proto packet
 * @vdev_id: vdev id
 * @type: packet type
 * @subtype: packet subtype
 * @proto_priv_data: protocol private data
 * can be stored in this.
 * @mpdu_seq: 802.11 MPDU sequence number
 */
struct qdf_dp_trace_proto_cmn {
	uint8_t vdev_id;
	uint8_t type;
	uint8_t subtype;
	uint32_t proto_priv_data;
	uint16_t mpdu_seq;
};

/**
 * struct qdf_dp_trace_proto_buf - proto packet buffer
 * @sa: source address
 * @da: destination address
 * @dir: direction
 * @cmn_info: common info
 */
struct qdf_dp_trace_proto_buf {
	struct qdf_mac_addr sa;
	struct qdf_mac_addr da;
	uint8_t dir;
	struct qdf_dp_trace_proto_cmn cmn_info;
};

/**
 * struct qdf_dp_trace_mgmt_buf - mgmt packet buffer
 * @vdev_id: vdev id
 * @type: packet type
 * @subtype: packet subtype
 */
struct qdf_dp_trace_mgmt_buf {
	uint8_t vdev_id;
	uint8_t type;
	uint8_t subtype;
};

/**
 * struct qdf_dp_trace_credit_record - tx credit record
 * @source: credit record source
 * @operation: credit operation
 * @delta: delta of credit
 * @total_credits: total credit
 * @g0_credit: group 0 credit
 * @g1_credit: group 1 credit
 */
struct qdf_dp_trace_credit_record {
	enum QDF_CREDIT_UPDATE_SOURCE source;
	enum QDF_CREDIT_OPERATION operation;
	int delta;
	int total_credits;
	int g0_credit;
	int g1_credit;
};

/**
 * struct qdf_dp_trace_event_buf - event buffer
 * @vdev_id: vdev id
 * @type: packet type
 * @subtype: packet subtype
 */
struct qdf_dp_trace_event_buf {
	uint8_t vdev_id;
	uint8_t type;
	uint8_t subtype;
};

/**
 * struct qdf_dp_trace_data_buf - nbuf data buffer
 * @msdu_id: msdu id
 */
struct qdf_dp_trace_data_buf {
	uint16_t msdu_id;
};

/**
 * struct qdf_dp_trace_record_s - Describes a record in DP trace
 * @time: time when it got stored
 * @code: Describes the particular event
 * @data: buffer to store data
 * @size: Length of the valid data stored in this record
 * @pid: process id which stored the data in this record
 * @pdev_id: pdev associated with the event
 */
struct qdf_dp_trace_record_s {
	uint64_t time;
	uint8_t code;
	uint8_t data[QDF_DP_TRACE_RECORD_SIZE];
	uint8_t size;
	uint32_t pid;
	uint8_t pdev_id;
};

/**
 * struct s_qdf_dp_trace_data - Parameters to configure/control DP trace
 * @head: Position of first record
 * @tail: Position of last record
 * @num:  Current index
 * @proto_bitmap: defines which protocol to be traced
 * @no_of_record: defines every nth packet to be traced
 * @num_records_to_dump: defines number of records to be dumped
 * @dump_counter: counter to track number of records dumped
 * @verbosity: defines verbosity level
 * @ini_conf_verbosity: Configured verbosity from INI
 * @enable: enable/disable DP trace
 * @count: current packet number
 * @live_mode_config: configuration as received during initialization
 * @live_mode: current live mode, enabled or disabled, can be throttled based
 *             on throughput
 * @curr_pos:
 * @saved_tail:
 * @force_live_mode: flag to enable live mode all the time for all packets.
 *                  This can be set/unset from userspace and overrides other
 *                  live mode flags.
 * @dynamic_verbosity_modify: Dynamic user configured verbosity overrides all
 * @print_pkt_cnt: count of number of packets printed in live mode
 * @high_tput_thresh: thresh beyond which live mode is turned off
 * @thresh_time_limit: max time, in terms of BW timer intervals to wait,
 *          for determining if high_tput_thresh has been crossed. ~1s
 * @tx_count: tx counter
 * @rx_count: rx counter
 * @arp_req: stats for arp reqs
 * @arp_resp: stats for arp resps
 * @icmp_req: stats for icmp reqs
 * @icmp_resp: stats for icmp resps
 * @dhcp_disc: stats for dhcp discover msgs
 * @dhcp_req: stats for dhcp req msgs
 * @dhcp_off: stats for dhcp offer msgs
 * @dhcp_ack: stats for dhcp ack msgs
 * @dhcp_nack: stats for dhcp nack msgs
 * @dhcp_others: stats for other dhcp pkts types
 * @eapol_m1: stats for eapol m1
 * @eapol_m2: stats for eapol m2
 * @eapol_m3: stats for eapol m3
 * @eapol_m4: stats for eapol m4
 * @eapol_others: stats for other eapol pkt types
 * @icmpv6_req: stats for icmpv6 reqs
 * @icmpv6_resp: stats for icmpv6 resps
 * @icmpv6_ns: stats for icmpv6 nss
 * @icmpv6_na: stats for icmpv6 nas
 * @icmpv6_rs: stats for icmpv6 rss
 * @icmpv6_ra: stats for icmpv6 ras
 * @proto_event_bitmap: defines which protocol to be diag logged.
 *  refer QDF_NBUF_PKT_TRAC_TYPE_DNS to QDF_NBUF_PKT_TRAC_TYPE_ARP
 *  for bitmap.
 */
struct s_qdf_dp_trace_data {
	uint32_t head;
	uint32_t tail;
	uint32_t num;
	uint32_t proto_bitmap;
	uint8_t no_of_record;
	uint16_t num_records_to_dump;
	uint16_t dump_counter;
	uint8_t verbosity;
	uint8_t ini_conf_verbosity;
	bool enable;
	bool live_mode_config;
	bool live_mode;
	uint32_t curr_pos;
	uint32_t saved_tail;
	bool force_live_mode;
	bool dynamic_verbosity_modify;
	uint8_t print_pkt_cnt;
	uint8_t high_tput_thresh;
	uint16_t thresh_time_limit;
	/* Stats */
	uint32_t tx_count;
	uint32_t rx_count;
	u16 arp_req;
	u16 arp_resp;
	u16 dhcp_disc;
	u16 dhcp_req;
	u16 dhcp_off;
	u16 dhcp_ack;
	u16 dhcp_nack;
	u16 dhcp_others;
	u16 eapol_m1;
	u16 eapol_m2;
	u16 eapol_m3;
	u16 eapol_m4;
	u16 eapol_others;
	u16 icmp_req;
	u16 icmp_resp;
	u16 icmpv6_req;
	u16 icmpv6_resp;
	u16 icmpv6_ns;
	u16 icmpv6_na;
	u16 icmpv6_rs;
	u16 icmpv6_ra;
	uint32_t proto_event_bitmap;
};

/**
 * enum qdf_dpt_debugfs_state - state to control read to debugfs file
 * @QDF_DPT_DEBUGFS_STATE_SHOW_STATE_INVALID: invalid state
 * @QDF_DPT_DEBUGFS_STATE_SHOW_STATE_INIT: initial state
 * @QDF_DPT_DEBUGFS_STATE_SHOW_IN_PROGRESS: read is in progress
 * @QDF_DPT_DEBUGFS_STATE_SHOW_COMPLETE:  read complete
 */

enum qdf_dpt_debugfs_state {
	QDF_DPT_DEBUGFS_STATE_SHOW_STATE_INVALID,
	QDF_DPT_DEBUGFS_STATE_SHOW_STATE_INIT,
	QDF_DPT_DEBUGFS_STATE_SHOW_IN_PROGRESS,
	QDF_DPT_DEBUGFS_STATE_SHOW_COMPLETE,
};

#define QDF_WIFI_MODULE_PARAMS_FILE "wifi_module_param.ini"

typedef void (*tp_qdf_trace_cb)(void *p_mac, tp_qdf_trace_record, uint16_t);
typedef void (*tp_qdf_state_info_cb) (char **buf, uint16_t *size);
#ifdef WLAN_FEATURE_MEMDUMP_ENABLE

/**
 * qdf_register_debugcb_init() - initializes debug callbacks
 * to NULL
 *
 * Return: None
 */
void qdf_register_debugcb_init(void);

/**
 * qdf_register_debug_callback() - stores callback handlers to print
 * state information
 * @module_id: module id of layer
 * @qdf_state_infocb: callback to be registered
 *
 * This function is used to store callback handlers to print
 * state information
 *
 * Return: None
 */
void qdf_register_debug_callback(QDF_MODULE_ID module_id,
					tp_qdf_state_info_cb qdf_state_infocb);

/**
 * qdf_state_info_dump_all() - it invokes callback of layer which registered
 * its callback to print its state information.
 * @buf:  buffer pointer to be passed
 * @size:  size of buffer to be filled
 * @driver_dump_size: actual size of buffer used
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS qdf_state_info_dump_all(char *buf, uint16_t size,
				   uint16_t *driver_dump_size);
#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static inline void qdf_register_debugcb_init(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */

#ifdef TRACE_RECORD
/**
 * qdf_trace_register() - registers the call back functions
 * @module_id: enum value of module
 * @qdf_trace_callback: call back functions to display the messages in
 * particular format.
 *
 * Registers the call back functions to display the messages in particular
 * format mentioned in these call back functions. This functions should be
 * called by interested module in their init part as we will be ready to
 * register as soon as modules are up.
 *
 * Return: None
 */
void qdf_trace_register(QDF_MODULE_ID module_id,
			tp_qdf_trace_cb qdf_trace_callback);

/**
 * qdf_trace_init() - initializes qdf trace structures and variables
 *
 * Called immediately after cds_preopen, so that we can start recording HDD
 * events ASAP.
 *
 * Return: None
 */
void qdf_trace_init(void);

/**
 * qdf_trace_deinit() - frees memory allocated dynamically
 *
 * Called from cds_deinit, so that we can free the memory and resets
 * the variables
 *
 * Return: None
 */
void qdf_trace_deinit(void);

/**
 * qdf_trace() - puts the messages in to ring-buffer
 * @module: Enum of module, basically module id.
 * @code: Code to be recorded
 * @session: Session ID of the log
 * @data: Actual message contents
 *
 * This function will be called from each module who wants record the messages
 * in circular queue. Before calling this functions make sure you have
 * registered your module with qdf through qdf_trace_register function.
 *
 * Return: None
 */
void qdf_trace(uint8_t module, uint16_t code, uint16_t session, uint32_t data);

/**
 * qdf_trace_enable() - Enable MTRACE for specific modules
 * @bitmask_of_module_id: Bitmask according to enum of the modules.
 *  32[dec] = 0010 0000 [bin] <enum of HDD is 5>
 *  64[dec] = 0100 0000 [bin] <enum of SME is 6>
 *  128[dec] = 1000 0000 [bin] <enum of PE is 7>
 * @enable: can be true or false true implies enabling MTRACE false implies
 *		disabling MTRACE.
 *
 * Enable MTRACE for specific modules whose bits are set in bitmask and enable
 * is true. if enable is false it disables MTRACE for that module. set the
 * bitmask according to enum value of the modules.
 * This functions will be called when you issue ioctl as mentioned following
 * [iwpriv wlan0 setdumplog <value> <enable>].
 * <value> - Decimal number, i.e. 64 decimal value shows only SME module,
 * 128 decimal value shows only PE module, 192 decimal value shows PE and SME.
 *
 * Return: None
 */
void qdf_trace_enable(uint32_t bitmask_of_module_id, uint8_t enable);

/**
 * qdf_trace_dump_all() - Dump data from ring buffer via call back functions
 * registered with QDF
 * @p_mac: Context of particular module
 * @code: Reason code
 * @session: Session id of log
 * @count: Number of lines to dump starting from tail to head
 * @bitmask_of_module: Bitmask according to enum of the modules.
 *
 * This function will be called up on issuing ioctl call as mentioned following
 * [iwpriv wlan0 dumplog 0 0 <n> <bitmask_of_module>]
 *
 * <n> - number lines to dump starting from tail to head.
 *
 * <bitmask_of_module> - if anybody wants to know how many messages were
 * recorded for particular module/s mentioned by setbit in bitmask from last
 * <n> messages. It is optional, if you don't provide then it will dump
 * everything from buffer.
 *
 * Return: None
 */
void qdf_trace_dump_all(void *p_mac, uint8_t code, uint8_t session,
			uint32_t count, uint32_t bitmask_of_module);

/**
 * qdf_trace_spin_lock_init() - initializes the lock variable before use
 *
 * This function will be called from cds_alloc_global_context, we will have lock
 * available to use ASAP
 *
 * Return: None
 */
QDF_STATUS qdf_trace_spin_lock_init(void);
#else
#ifndef QDF_TRACE_PRINT_ENABLE
static inline
void qdf_trace_init(void)
{
}

static inline
void qdf_trace_deinit(void)
{
}

static inline
void qdf_trace_enable(uint32_t bitmask_of_module_id, uint8_t enable)
{
}

static inline
void qdf_trace(uint8_t module, uint16_t code, uint16_t session, uint32_t data)
{
}

static inline
void qdf_trace_dump_all(void *p_mac, uint8_t code, uint8_t session,
			uint32_t count, uint32_t bitmask_of_module)
{
}

static inline
QDF_STATUS qdf_trace_spin_lock_init(void)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif

#ifdef WLAN_MAX_LOGS_PER_SEC
/**
 * qdf_detected_excessive_logging() - Excessive logging detected
 *
 * Track logging count using a quasi-tumbling window.
 * If the max logging count for a given window is exceeded,
 * return true else fails.
 *
 * Return: true/false
 */
bool qdf_detected_excessive_logging(void);

/**
 * qdf_rl_print_count_set() - set the ratelimiting print count
 * @rl_print_count: ratelimiting print count
 *
 * Return: none
 */
void qdf_rl_print_count_set(uint32_t rl_print_count);

/**
 * qdf_rl_print_time_set() - set the ratelimiting print time
 * @rl_print_time: ratelimiting print time
 *
 * Return: none
 */
void qdf_rl_print_time_set(uint32_t rl_print_time);

/**
 * qdf_rl_print_suppressed_log() - print the suppressed logs count
 *
 * Return: none
 */
void qdf_rl_print_suppressed_log(void);

/**
 * qdf_rl_print_suppressed_inc() - increment the suppressed logs count
 *
 * Return: none
 */
void qdf_rl_print_suppressed_inc(void);

#else /* WLAN_MAX_LOGS_PER_SEC */
static inline bool qdf_detected_excessive_logging(void)
{
	return false;
}
static inline void qdf_rl_print_count_set(uint32_t rl_print_count) {}
static inline void qdf_rl_print_time_set(uint32_t rl_print_time) {}
static inline void qdf_rl_print_suppressed_log(void) {}
static inline void qdf_rl_print_suppressed_inc(void) {}
#endif /* WLAN_MAX_LOGS_PER_SEC */

#ifdef ENABLE_MTRACE_LOG
/**
 * qdf_mtrace_log() - Logs a message tracepoint to DIAG
 * Infrastructure.
 * @src_module: Enum of source module (basically module id)
 * from where the message with message_id is posted.
 * @dst_module: Enum of destination module (basically module id)
 * to which the message with message_id is posted.
 * @message_id: Id of the message to be posted
 * @vdev_id: Vdev Id
 *
 * This function logs to the DIAG Infrastructure a tracepoint for a
 * message being sent from a source module to a destination module
 * with a specific ID for the benefit of a specific vdev.
 * For non-vdev messages vdev_id will be NO_SESSION
 * Return: None
 */
void qdf_mtrace_log(QDF_MODULE_ID src_module, QDF_MODULE_ID dst_module,
		    uint16_t message_id, uint8_t vdev_id);
#else
static inline
void qdf_mtrace_log(QDF_MODULE_ID src_module, QDF_MODULE_ID dst_module,
		    uint16_t message_id, uint8_t vdev_id)
{
}
#endif

#ifdef TRACE_RECORD
/**
 * qdf_mtrace() - puts the messages in to ring-buffer
 * and logs a message tracepoint to DIAG Infrastructure.
 * @src_module: Enum of source module (basically module id)
 * from where the message with message_id is posted.
 * @dst_module: Enum of destination module (basically module id)
 * to which the message with message_id is posted.
 * @message_id: Id of the message to be posted
 * @vdev_id: Vdev Id
 * @data: Actual message contents
 *
 * This function will be called from each module which wants to record the
 * messages in circular queue. Before calling this function make sure you
 * have registered your module with qdf through qdf_trace_register function.
 * In addition of the recording the messages in circular queue this function
 * will log the message tracepoint to the  DIAG infrastructure.
 * these logs will be later used by post processing script.
 *
 * Return: None
 */
void qdf_mtrace(QDF_MODULE_ID src_module, QDF_MODULE_ID dst_module,
		uint16_t message_id, uint8_t vdev_id, uint32_t data);
#else
static inline
void qdf_mtrace(QDF_MODULE_ID src_module, QDF_MODULE_ID dst_module,
		uint16_t message_id, uint8_t vdev_id, uint32_t data)
{
}
#endif

#ifdef CONFIG_DP_TRACE
/**
 * qdf_dp_set_proto_bitmap() - set dp trace proto bitmap
 * @val: unsigned bitmap to set
 *
 * Return: proto bitmap
 */
void qdf_dp_set_proto_bitmap(uint32_t val);

/**
 * qdf_dp_trace_set_verbosity() - set verbosity value
 * @val: Value to set
 *
 * Return: Null
 */
void qdf_dp_trace_set_verbosity(uint32_t val);

/**
 * qdf_dp_set_no_of_record() - set dp trace no_of_record
 * @val: unsigned no_of_record to set
 *
 * Return: null
 */
void qdf_dp_set_no_of_record(uint32_t val);

#define QDF_DP_TRACE_RECORD_INFO_LIVE (0x1)
#define QDF_DP_TRACE_RECORD_INFO_THROTTLED (0x1 << 1)

/**
 * qdf_dp_trace_log_pkt() - log packet type enabled through iwpriv
 * @vdev_id: vdev_id
 * @skb: skb pointer
 * @dir: direction
 * @pdev_id: pdev_id
 * @op_mode: Vdev Operation mode
 *
 * Return: true: some protocol was logged, false: no protocol was logged.
 */
bool qdf_dp_trace_log_pkt(uint8_t vdev_id, struct sk_buff *skb,
			  enum qdf_proto_dir dir, uint8_t pdev_id,
			  enum QDF_OPMODE op_mode);

/**
 * qdf_dp_trace_init() - enables the DP trace
 * @live_mode_config: live mode configuration
 * @thresh: high throughput threshold for disabling live mode
 * @time_limit: max time to wait before deciding if thresh is crossed
 * @verbosity: dptrace verbosity level
 * @proto_bitmap: bitmap to enable/disable specific protocols
 *
 * Called during driver load to init dptrace
 *
 * A brief note on the 'thresh' param -
 * Total # of packets received in a bandwidth timer interval beyond which
 * DP Trace logging for data packets (including ICMP) will be disabled.
 * In memory logging will still continue for these packets. Other packets for
 * which proto.bitmap is set will continue to be recorded in logs and in memory.
 *
 * Return: None
 */
void qdf_dp_trace_init(bool live_mode_config, uint8_t thresh,
		       uint16_t time_limit, uint8_t verbosity,
		       uint32_t proto_bitmap);

void qdf_dp_trace_deinit(void);

/**
 * qdf_dp_trace_spin_lock_init() - initializes the lock variable before use
 * This function will be called from cds_alloc_global_context, we will have lock
 * available to use ASAP
 *
 * Return: None
 */
void qdf_dp_trace_spin_lock_init(void);

/**
 * qdf_dp_trace_set_value() - Configure the value to control DP trace
 * @proto_bitmap: defines the protocol to be tracked
 * @no_of_records: defines the nth packet which is traced
 * @verbosity: defines the verbosity level
 *
 * Return: None
 */
void qdf_dp_trace_set_value(uint32_t proto_bitmap, uint8_t no_of_records,
			    uint8_t verbosity);

/**
 * qdf_dp_trace_set_track() - Marks whether the packet needs to be traced
 * @nbuf: defines the netbuf
 * @dir: direction
 *
 * Return: None
 */
void qdf_dp_trace_set_track(qdf_nbuf_t nbuf, enum qdf_proto_dir dir);

/**
 * qdf_dp_trace() - Stores the data in buffer
 * @nbuf: defines the netbuf
 * @code: defines the event
 * @pdev_id: pdev_id
 * @data: defines the data to be stored
 * @size: defines the size of the data record
 * @dir: direction
 *
 * Return: None
 */
void qdf_dp_trace(qdf_nbuf_t nbuf, enum QDF_DP_TRACE_ID code, uint8_t pdev_id,
		  uint8_t *data, uint8_t size, enum qdf_proto_dir dir);

/**
 * qdf_dp_trace_dump_all() - Dump data from ring buffer via call back functions
 * registered with QDF
 * @count: Number of lines to dump starting from tail to head
 * @pdev_id: pdev_id
 *
 * Return: None
 */
void qdf_dp_trace_dump_all(uint32_t count, uint8_t pdev_id);

/**
 * qdf_dpt_get_curr_pos_debugfs() - get curr position to start read
 * @file: debugfs file to read
 * @state: state to control read to debugfs file
 *
 * Return: curr pos
 */
uint32_t qdf_dpt_get_curr_pos_debugfs(qdf_debugfs_file_t file,
				enum qdf_dpt_debugfs_state state);
/**
 * qdf_dpt_dump_stats_debugfs() - dump DP Trace stats to debugfs file
 * @file: debugfs file to read
 * @curr_pos: curr position to start read
 *
 * Return: QDF_STATUS
 */
QDF_STATUS qdf_dpt_dump_stats_debugfs(qdf_debugfs_file_t file,
				      uint32_t curr_pos);

/**
 * qdf_dpt_set_value_debugfs() - set value of DP Trace debugfs params
 * @proto_bitmap: defines which protocol to be traced
 * @no_of_record: defines every nth packet to be traced
 * @verbosity: defines verbosity level
 * @num_records_to_dump: defines number of records to be dumped
 *
 * Return: none
 */
void qdf_dpt_set_value_debugfs(uint8_t proto_bitmap, uint8_t no_of_record,
			    uint8_t verbosity, uint16_t num_records_to_dump);


/**
 * qdf_dp_trace_dump_stats() - dump DP Trace stats
 *
 * Return: none
 */
void qdf_dp_trace_dump_stats(void);
typedef void (*tp_qdf_dp_trace_cb)(struct qdf_dp_trace_record_s*,
				   uint16_t, uint8_t, uint8_t info);
/**
 * qdf_dp_display_record() - Displays a record in DP trace
 * @record: pointer to a record in DP trace
 * @index: record index
 * @pdev_id: pdev id for the mgmt pkt
 * @info: info used to display pkt (live mode, throttling)
 *
 * Return: None
 */
void qdf_dp_display_record(struct qdf_dp_trace_record_s *record,
			   uint16_t index, uint8_t pdev_id,
			   uint8_t info);

/**
 * qdf_dp_display_ptr_record() - display record
 * @record: dptrace record
 * @rec_index: index
 * @pdev_id: pdev id for the mgmt pkt
 * @info: info used to display pkt (live mode, throttling)
 *
 * Return: none
 */
void qdf_dp_display_ptr_record(struct qdf_dp_trace_record_s *record,
			       uint16_t rec_index, uint8_t pdev_id,
			       uint8_t info);

/**
 * qdf_dp_display_proto_pkt() - display proto packet
 * @record: dptrace record
 * @index: index
 * @pdev_id: pdev id for the mgmt pkt
 * @info: info used to display pkt (live mode, throttling)
 *
 * Return: none
 */
void qdf_dp_display_proto_pkt(struct qdf_dp_trace_record_s *record,
			      uint16_t index, uint8_t pdev_id,
			      uint8_t info);
/**
 * qdf_dp_display_data_pkt_record() - Displays a data packet in DP trace
 * @record: pointer to a record in DP trace
 * @rec_index: record index
 * @pdev_id: pdev id
 * @info: display info regarding record
 *
 * Return: None
 */
void
qdf_dp_display_data_pkt_record(struct qdf_dp_trace_record_s *record,
			       uint16_t rec_index, uint8_t pdev_id,
			       uint8_t info);

/**
 * qdf_dp_get_status_from_htt() - Convert htt tx status to qdf dp status
 * @status: htt_tx_status which needs to be converted
 *
 * Return: the status that from qdf_dp_tx_rx_status
 */
enum qdf_dp_tx_rx_status qdf_dp_get_status_from_htt(uint8_t status);

/**
 * qdf_dp_get_status_from_a_status() - Convert A_STATUS to qdf dp status
 * @status: A_STATUS which needs to be converted
 *
 * Return: the status that from qdf_dp_tx_rx_status
 */
enum qdf_dp_tx_rx_status qdf_dp_get_status_from_a_status(uint8_t status);

/**
 * qdf_dp_trace_ptr() - record dptrace
 * @nbuf: network buffer
 * @code: dptrace code
 * @pdev_id: pdev_id
 * @data: data
 * @size: size of data
 * @msdu_id: msdu_id
 * @buf_arg_status: return status
 * @qdf_tx_status: qdf tx rx status
 * @op_mode: Vdev Operation mode
 *
 * Return: none
 */
void qdf_dp_trace_ptr(qdf_nbuf_t nbuf, enum QDF_DP_TRACE_ID code,
		      uint8_t pdev_id, uint8_t *data, uint8_t size,
		      uint16_t msdu_id, uint16_t buf_arg_status,
		      enum qdf_dp_tx_rx_status qdf_tx_status,
		      enum QDF_OPMODE op_mode);

/**
 * qdf_dp_trace_throttle_live_mode() - Throttle DP Trace live mode
 * @high_bw_request: whether this is a high BW req or not
 *
 * The function tries to prevent excessive logging into the live buffer by
 * having an upper limit on number of packets that can be logged per second.
 *
 * The intention is to allow occasional pings and data packets and really low
 * throughput levels while suppressing bursts and higher throughput levels so
 * that we donot hog the live buffer.
 *
 * If the number of packets printed in a particular second exceeds the thresh,
 * disable printing in the next second.
 *
 * Return: None
 */
void qdf_dp_trace_throttle_live_mode(bool high_bw_request);

/**
 * qdf_dp_trace_apply_tput_policy() - Change verbosity based on the TPUT
 * @is_data_traffic: Is traffic more than low TPUT threashould
 *
 * Return: None
 */
void qdf_dp_trace_apply_tput_policy(bool is_data_traffic);

/**
 * qdf_dp_trace_data_pkt() - trace data packet
 * @nbuf: nbuf which needs to be traced
 * @pdev_id: pdev_id
 * @code: QDF_DP_TRACE_ID for the packet (TX or RX)
 * @msdu_id: tx desc id for the nbuf (Only applies to TX packets)
 * @dir: TX or RX packet direction
 *
 * Return: None
 */
void qdf_dp_trace_data_pkt(qdf_nbuf_t nbuf, uint8_t pdev_id,
			   enum QDF_DP_TRACE_ID code, uint16_t msdu_id,
			   enum qdf_proto_dir dir);

/**
 * qdf_dp_get_proto_bitmap() - get dp trace proto bitmap
 *
 * Return: proto bitmap
 */
uint32_t qdf_dp_get_proto_bitmap(void);

uint8_t qdf_dp_get_verbosity(void);

/**
 * qdf_dp_get_no_of_record() - get dp trace no_of_record
 *
 * Return: number of records
 */
uint8_t qdf_dp_get_no_of_record(void);

/**
 * qdf_dp_trace_proto_pkt() - record proto packet
 * @code: dptrace code
 * @sa: source mac address
 * @da: destination mac address
 * @dir: direction
 * @pdev_id: pdev id
 * @print: to print this proto pkt or not
 * @cmn_info: common info for proto pkt
 *
 * Return: none
 */
void
qdf_dp_trace_proto_pkt(enum QDF_DP_TRACE_ID code,
		       uint8_t *sa, uint8_t *da,
		       enum qdf_proto_dir dir,
		       uint8_t pdev_id, bool print,
		       struct qdf_dp_trace_proto_cmn *cmn_info);

/**
 * qdf_dp_trace_disable_live_mode() - disable live mode for dptrace
 *
 * Return: none
 */
void qdf_dp_trace_disable_live_mode(void);

/**
 * qdf_dp_trace_enable_live_mode() - enable live mode for dptrace
 *
 * Return: none
 */
void qdf_dp_trace_enable_live_mode(void);

/**
 * qdf_dp_trace_clear_buffer() - clear dp trace buffer
 *
 * Return: none
 */
void qdf_dp_trace_clear_buffer(void);

/**
 * qdf_dp_trace_mgmt_pkt() - record mgmt packet
 * @code: dptrace code
 * @vdev_id: vdev id
 * @pdev_id: pdev_id
 * @type: proto type
 * @subtype: proto subtype
 *
 * Return: none
 */
void qdf_dp_trace_mgmt_pkt(enum QDF_DP_TRACE_ID code, uint8_t vdev_id,
			   uint8_t pdev_id, enum qdf_proto_type type,
			   enum qdf_proto_subtype subtype);

/**
 * qdf_dp_trace_credit_record() - record credit update
 * @source: source of record
 * @operation: credit operation
 * @delta: credit delta
 * @total_credits: total credit
 * @g0_credit: group 0 credit
 * @g1_credit: group 1 credit
 */
void qdf_dp_trace_credit_record(enum QDF_CREDIT_UPDATE_SOURCE source,
				enum QDF_CREDIT_OPERATION operation,
				int delta, int total_credits,
				int g0_credit, int g1_credit);

/**
 * qdf_dp_display_mgmt_pkt() - display proto packet
 * @record: dptrace record
 * @index: index
 * @pdev_id: pdev id for the mgmt pkt
 * @info: info used to display pkt (live mode, throttling)
 *
 * Return: none
 */
void qdf_dp_display_mgmt_pkt(struct qdf_dp_trace_record_s *record,
			     uint16_t index, uint8_t pdev_id, uint8_t info);

/**
 * qdf_dp_display_credit_record() - display credit record
 * @record: dptrace record
 * @index: index
 * @pdev_id: pdev id
 * @info: metadeta info
 */
void qdf_dp_display_credit_record(struct qdf_dp_trace_record_s *record,
				  uint16_t index, uint8_t pdev_id,
				  uint8_t info);

/**
 * qdf_dp_display_event_record() - display event records
 * @record: dptrace record
 * @index: index
 * @pdev_id: pdev id for the mgmt pkt
 * @info: info used to display pkt (live mode, throttling)
 *
 * Return: none
 */
void qdf_dp_display_event_record(struct qdf_dp_trace_record_s *record,
				 uint16_t index, uint8_t pdev_id, uint8_t info);

/**
 * qdf_dp_trace_record_event() - record events
 * @code: dptrace code
 * @vdev_id: vdev id
 * @pdev_id: pdev_id
 * @type: proto type
 * @subtype: proto subtype
 *
 * Return: none
 */
void qdf_dp_trace_record_event(enum QDF_DP_TRACE_ID code, uint8_t vdev_id,
			       uint8_t pdev_id, enum qdf_proto_type type,
			       enum qdf_proto_subtype subtype);

/**
 * qdf_dp_set_proto_event_bitmap() - Set the protocol event bitmap
 * @value: proto event bitmap value.
 *
 * QDF_NBUF_PKT_TRAC_TYPE_DNS       0x01
 * QDF_NBUF_PKT_TRAC_TYPE_EAPOL     0x02
 * QDF_NBUF_PKT_TRAC_TYPE_DHCP      0x04
 * QDF_NBUF_PKT_TRAC_TYPE_ARP       0x10
 *
 * Return: none
 */
void qdf_dp_set_proto_event_bitmap(uint32_t value);

/**
 * qdf_dp_log_proto_pkt_info() - Send diag log event
 * @sa: source MAC address
 * @da: destination MAC address
 * @type: pkt type
 * @subtype: pkt subtype
 * @dir: tx or rx
 * @msdu_id: msdu id
 * @status: status
 *
 * Return: none
 */
void qdf_dp_log_proto_pkt_info(uint8_t *sa, uint8_t *da, uint8_t type,
			       uint8_t subtype, uint8_t dir, uint16_t msdu_id,
			       uint8_t status);

/**
 * qdf_dp_track_noack_check() - Check if no ack count should be tracked for
 *  the configured protocol packet types
 * @nbuf: nbuf
 * @subtype: subtype of packet to be tracked
 *
 * Return: none
 */
void qdf_dp_track_noack_check(qdf_nbuf_t nbuf, enum qdf_proto_subtype *subtype);
#else
static inline
bool qdf_dp_trace_log_pkt(uint8_t vdev_id, struct sk_buff *skb,
			  enum qdf_proto_dir dir, uint8_t pdev_id,
			  enum QDF_OPMODE op_mode)
{
	return false;
}
static inline
void qdf_dp_trace_init(bool live_mode_config, uint8_t thresh,
				uint16_t time_limit, uint8_t verbosity,
				uint32_t proto_bitmap)
{
}

static inline
void qdf_dp_trace_deinit(void)
{
}

static inline
void qdf_dp_trace_set_track(qdf_nbuf_t nbuf, enum qdf_proto_dir dir)
{
}
static inline
void qdf_dp_trace_set_value(uint32_t proto_bitmap, uint8_t no_of_records,
			    uint8_t verbosity)
{
}

static inline
void qdf_dp_trace_dump_all(uint32_t count, uint8_t pdev_id)
{
}

static inline
uint32_t qdf_dpt_get_curr_pos_debugfs(qdf_debugfs_file_t file,
				      enum qdf_dpt_debugfs_state state)
{
	return 0;
}

static inline
QDF_STATUS qdf_dpt_dump_stats_debugfs(qdf_debugfs_file_t file,
				      uint32_t curr_pos)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void qdf_dpt_set_value_debugfs(uint8_t proto_bitmap, uint8_t no_of_record,
			    uint8_t verbosity, uint16_t num_records_to_dump)
{
}

static inline void qdf_dp_trace_dump_stats(void)
{
}

static inline
void qdf_dp_trace_disable_live_mode(void)
{
}

static inline
void qdf_dp_trace_enable_live_mode(void)
{
}

static inline
void qdf_dp_trace_throttle_live_mode(bool high_bw_request)
{
}

static inline
void qdf_dp_trace_clear_buffer(void)
{
}

static inline
void qdf_dp_trace_apply_tput_policy(bool is_data_traffic)
{
}

static inline
void qdf_dp_trace_data_pkt(qdf_nbuf_t nbuf, uint8_t pdev_id,
			   enum QDF_DP_TRACE_ID code, uint16_t msdu_id,
			   enum qdf_proto_dir dir)
{
}

static inline
void qdf_dp_log_proto_pkt_info(uint8_t *sa, uint8_t *da, uint8_t type,
			       uint8_t subtype, uint8_t dir, uint16_t msdu_id,
			       uint8_t status)
{
}

static inline
void qdf_dp_track_noack_check(qdf_nbuf_t nbuf, enum qdf_proto_subtype *subtype)
{
}

static inline
enum qdf_dp_tx_rx_status qdf_dp_get_status_from_htt(uint8_t status)
{
	return QDF_TX_RX_STATUS_OK;
}

static inline
enum qdf_dp_tx_rx_status qdf_dp_get_status_from_a_status(uint8_t status)
{
	return QDF_TX_RX_STATUS_OK;
}
#endif

/**
 * qdf_trace_display() - Display trace
 *
 * Return:  None
 */
void qdf_trace_display(void);

/**
 * qdf_snprintf() - wrapper function to snprintf
 * @str_buffer: string Buffer
 * @size: defines the size of the data record
 * @str_format: Format string in which the message to be logged. This format
 * string contains printf-like replacement parameters, which follow
 * this parameter in the variable argument list.
 *
 * Return: num of bytes written to buffer
 */
int __printf(3, 4) qdf_snprintf(char *str_buffer, unsigned int size,
				char *str_format, ...);

#define QDF_SNPRINTF qdf_snprintf

#ifdef TSOSEG_DEBUG

static inline void qdf_tso_seg_dbg_bug(char *msg)
{
	qdf_print("%s", msg);
	QDF_BUG(0);
};

/**
 * qdf_tso_seg_dbg_init - initialize TSO segment debug structure
 * @tsoseg: structure to initialize
 *
 * TSO segment dbg structures are attached to qdf_tso_seg_elem_t
 * structures and are allocated only of TSOSEG_DEBUG is defined.
 * When allocated, at the time of the tso_seg_pool initialization,
 * which goes with tx_desc initialization (1:1), each structure holds
 * a number of (currently 16) history entries, basically describing
 * what operation has been performed on this particular tso_seg_elem.
 * This history buffer is a circular buffer and the current index is
 * held in an atomic variable called cur. It is incremented every
 * operation. Each of these operations are added with the function
 * qdf_tso_seg_dbg_record.
 * For each segment, this initialization function MUST be called PRIOR
 * TO any _dbg_record() function calls.
 * On free, qdf_tso_seg_elem structure is cleared (using qdf_tso_seg_dbg_zero)
 * which clears the tso_desc, BUT DOES NOT CLEAR THE HISTORY element.
 *
 * Return:
 *   None
 */
static inline
void qdf_tso_seg_dbg_init(struct qdf_tso_seg_elem_t *tsoseg)
{
	tsoseg->dbg.txdesc = NULL;
	qdf_atomic_init(&tsoseg->dbg.cur); /* history empty */
}

/**
 * qdf_tso_seg_dbg_record - add a history entry to TSO debug structure
 * @tsoseg: structure to initialize
 * @id: operation ID (identifies the caller)
 *
 * Adds a history entry to the history circular buffer. Each entry
 * contains an operation id (caller, as currently each ID is used only
 * once in the source, so it directly identifies the src line that invoked
 * the recording.
 *
 * qdf_tso_seg_dbg_record CAN ONLY BE CALLED AFTER the entry is initialized
 * by qdf_tso_seg_dbg_init.
 *
 * The entry to be added is written at the location pointed by the atomic
 * variable called cur. Cur is an ever increasing atomic variable. It is
 * masked so that only the lower 4 bits are used (16 history entries).
 *
 * Return:
 *   int: the entry this record was recorded at
 */
static inline
int qdf_tso_seg_dbg_record(struct qdf_tso_seg_elem_t *tsoseg, short id)
{
	int rc = -1;
	unsigned int c;

	qdf_assert(tsoseg);

	if (id == TSOSEG_LOC_ALLOC) {
		c = qdf_atomic_read(&tsoseg->dbg.cur);
		/* dont crash on the very first alloc on the segment */
		c &= 0x0f;
		/* allow only INIT and FREE ops before ALLOC */
		if (tsoseg->dbg.h[c].id >= id)
			qdf_tso_seg_dbg_bug("Rogue TSO seg alloc");
	}
	c = qdf_atomic_inc_return(&tsoseg->dbg.cur);

	c &= 0x0f;
	tsoseg->dbg.h[c].ts = qdf_get_log_timestamp();
	tsoseg->dbg.h[c].id = id;
	rc = c;

	return rc;
};

static inline void
qdf_tso_seg_dbg_setowner(struct qdf_tso_seg_elem_t *tsoseg, void *owner)
{
	if (tsoseg)
		tsoseg->dbg.txdesc = owner;
};

static inline void
qdf_tso_seg_dbg_zero(struct qdf_tso_seg_elem_t *tsoseg)
{
	memset(tsoseg, 0, offsetof(struct qdf_tso_seg_elem_t, dbg));
	return;
};

#else
static inline
void qdf_tso_seg_dbg_init(struct qdf_tso_seg_elem_t *tsoseg)
{
};
static inline
int qdf_tso_seg_dbg_record(struct qdf_tso_seg_elem_t *tsoseg, short id)
{
	return 0;
};
static inline void qdf_tso_seg_dbg_bug(char *msg)
{
};
static inline void
qdf_tso_seg_dbg_setowner(struct qdf_tso_seg_elem_t *tsoseg, void *owner)
{
};
static inline int
qdf_tso_seg_dbg_zero(struct qdf_tso_seg_elem_t *tsoseg)
{
	memset(tsoseg, 0, sizeof(struct qdf_tso_seg_elem_t));
	return 0;
};

#endif /* TSOSEG_DEBUG */

/**
 * qdf_trace_hex_dump() - externally called hex dump function
 * @module: Module identifier a member of the QDF_MODULE_ID enumeration that
 * identifies the module issuing the trace message.
 * @level: Trace level a member of the QDF_TRACE_LEVEL enumeration indicating
 * the severity of the condition causing the trace message to be
 * issued. More severe conditions are more likely to be logged.
 * @data: The base address of the buffer to be logged.
 * @buf_len: The size of the buffer to be logged.
 *
 * Checks the level of severity and accordingly prints the trace messages
 *
 * Return:  None
 */
void qdf_trace_hex_dump(QDF_MODULE_ID module, QDF_TRACE_LEVEL level,
			void *data, int buf_len);

/**
 * qdf_trace_hex_ascii_dump() - externally called hex and ascii dump function
 * @module: Module identifier a member of the QDF_MODULE_ID enumeration that
 * identifies the module issuing the trace message.
 * @level: Trace level a member of the QDF_TRACE_LEVEL enumeration indicating
 * the severity of the condition causing the trace message to be
 * issued. More severe conditions are more likely to be logged.
 * @data: The base address of the buffer to be logged.
 * @buf_len: The size of the buffer to be logged.
 *
 * Checks the level of severity and accordingly prints the trace messages
 *
 * Return:  None
 */
void qdf_trace_hex_ascii_dump(QDF_MODULE_ID module, QDF_TRACE_LEVEL level,
			      void *data, int buf_len);

#define ERROR_CODE                      -1
#define QDF_MAX_NAME_SIZE               32
#define MAX_PRINT_CONFIG_SUPPORTED      32

#define MAX_SUPPORTED_CATEGORY QDF_MODULE_ID_MAX

/**
 * qdf_set_pidx() - Sets the global qdf_pidx.
 * @pidx: Index of print control object assigned to the module
 *
 */
void qdf_set_pidx(int pidx);

/**
 * qdf_get_pidx() - Returns the global qdf_pidx.
 *
 * Return: Current qdf print index.
 */
int qdf_get_pidx(void);
/*
 * Shared print control index
 * for converged debug framework
 */

#define QDF_PRINT_IDX_SHARED -1

/**
 * QDF_PRINT_INFO() - Generic wrapper API for logging
 * @idx: Index of print control object
 * @module: Module identifier. A member of QDF_MODULE_ID enumeration that
 *           identifies the module issuing the trace message
 * @level: Trace level. A member of QDF_TRACE_LEVEL enumeration indicating
 *          the severity of the condition causing the trace message to be
 *          issued.
 * @str_format: Format string that contains the message to be logged.
 *
 *
 * This wrapper will be used for any generic logging messages. Wrapper will
 * compile a call to converged QDF trace message API.
 *
 * Return: Nothing
 *
 */
void QDF_PRINT_INFO(unsigned int idx, QDF_MODULE_ID module,
		    QDF_TRACE_LEVEL level,
		    char *str_format, ...);

/**
 * struct category_info - Category information structure
 * @category_verbose_mask: Embeds information about category's verbose level
 */
struct category_info {
	uint16_t category_verbose_mask;
};

/**
 * struct category_name_info - Category name information structure
 * @category_name_str: Embeds information about category name
 */
struct category_name_info {
	unsigned char category_name_str[QDF_MAX_NAME_SIZE];
};

/**
 * qdf_trace_msg_cmn() - Converged logging API
 * @idx: Index of print control object assigned to the module
 * @category: Category identifier. A member of the QDF_MODULE_ID enumeration
 *            that identifies the category issuing the trace message.
 * @verbose: Verbose level. A member of the QDF_TRACE_LEVEL enumeration
 *           indicating the severity of the condition causing the trace
 *           message to be issued. More severe conditions are more likely
 *           to be logged.
 * @str_format: Format string. The message to be logged. This format string
 *              contains printf-like replacement parameters, which follow this
 *              parameter in the variable argument list.
 * @val: Variable argument list part of the log message
 *
 * Return: nothing
 *
 */
void qdf_trace_msg_cmn(unsigned int idx,
			QDF_MODULE_ID category,
			QDF_TRACE_LEVEL verbose,
			const char *str_format,
			va_list val);

/**
 * struct qdf_print_ctrl - QDF Print Control structure
 *                        Statically allocated objects of print control
 *                        structure are declared that will support maximum of
 *                        32 print control objects. Any module that needs to
 *                        register to the print control framework needs to
 *                        obtain a print control object using
 *                        qdf_print_ctrl_register API. It will have to pass
 *                        pointer to category info structure, name and
 *                        custom print function to be used if required.
 * @name:                 Optional name for the control object
 * @cat_info:             Array of category_info struct
 * @custom_print:         Custom print handler
 * @custom_ctxt:          Custom print context
 * @dbglvlmac_on:         Flag to enable/disable MAC level filtering
 * @in_use:               Boolean to indicate if control object is in use
 */
struct qdf_print_ctrl {
	char name[QDF_MAX_NAME_SIZE];
	struct category_info cat_info[MAX_SUPPORTED_CATEGORY];
	void (*custom_print)(void *ctxt, const char *fmt, va_list args);
	void *custom_ctxt;
#ifdef DBG_LVL_MAC_FILTERING
	unsigned char dbglvlmac_on;
#endif
	bool in_use;
};

/**
 * qdf_print_ctrl_register() - Allocate QDF print control object, assign
 *                             pointer to category info or print control
 *                             structure and return the index to the callee
 * @cinfo:                 Pointer to array of category info structure
 * @custom_print_handler:  Pointer to custom print handler
 * @custom_ctx:            Pointer to custom context
 * @pctrl_name:            Pointer to print control object name
 *
 * Return: Index of qdf_print_ctrl structure
 *
 */
int qdf_print_ctrl_register(const struct category_info *cinfo,
			    void *custom_print_handler,
			    void *custom_ctx,
			    const char *pctrl_name);

#ifdef QCA_WIFI_MODULE_PARAMS_FROM_INI
/**
 * qdf_initialize_module_param_from_ini() - Update qdf module params
 *
 * Read the file which has wifi module params, parse and update
 * qdf module params.
 *
 * Return: void
 */
void qdf_initialize_module_param_from_ini(void);
#else
static inline
void qdf_initialize_module_param_from_ini(void)
{
}
#endif

/**
 * qdf_shared_print_ctrl_init() - Initialize the shared print ctrl obj with
 *                                all categories set to the default level
 *
 * Return: void
 *
 */
void qdf_shared_print_ctrl_init(void);

/**
 * qdf_print_setup() - Setup default values to all the print control objects
 *
 * Register new print control object for the callee
 *
 * Return:             QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE
 *                      on failure
 */
QDF_STATUS qdf_print_setup(void);

/**
 * qdf_print_ctrl_cleanup() - Clean up a print control object
 * @idx: Index of print control object
 *
 * Cleanup the print control object for the callee
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS qdf_print_ctrl_cleanup(unsigned int idx);

/**
 * qdf_shared_print_ctrl_cleanup() - Clean up of the shared object
 *
 * Cleanup the shared print-ctrl-object
 *
 * Return: void
 */
void qdf_shared_print_ctrl_cleanup(void);

/**
 * qdf_print_set_category_verbose() - Enable/Disable category for a
 *                                    print control object with
 *                                    user provided verbose level
 * @idx: Index of the print control object assigned to callee
 * @category: Category information
 * @verbose: Verbose information
 * @is_set: Flag indicating if verbose level needs to be enabled or disabled
 *
 * Return: QDF_STATUS_SUCCESS for success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS qdf_print_set_category_verbose(unsigned int idx,
					  QDF_MODULE_ID category,
					  QDF_TRACE_LEVEL verbose,
					  bool is_set);

/**
 * qdf_log_dump_at_kernel_level() - Enable/Disable printk call
 * @enable: Indicates whether printk is enabled in QDF_TRACE
 *
 * Return: void
 */
void qdf_log_dump_at_kernel_level(bool enable);

/**
 * qdf_logging_set_flush_timer() - Set the time period in which host logs
 *                                 should be flushed out to user-space
 * @milliseconds: milliseconds after which the logs should be flushed out to
 *                 user-space
 *
 * Return: QDF_STATUS_SUCCESS for success and QDF_STATUS_E_FAILURE for failure
 */
int qdf_logging_set_flush_timer(uint32_t milliseconds);

/**
 * qdf_logging_flush_logs() - Flush out the logs to user-space one time
 *
 * Return: void
 */
void qdf_logging_flush_logs(void);

/**
 * qdf_print_get_category_verbose() - Get category verbose information for the
 *                                    print control object
 *
 * @idx: Index of print control object
 * @category: Category information
 *
 * Return: Verbose value for the particular category
 */
QDF_TRACE_LEVEL qdf_print_get_category_verbose(unsigned int idx,
					       QDF_MODULE_ID category);

/**
 * qdf_print_is_category_enabled() - Get category information for the
 *                                   print control object
 *
 * @idx: Index of print control object
 * @category: Category information
 *
 * Return: Verbose enabled(true) or disabled(false) or invalid input (false)
 */
bool qdf_print_is_category_enabled(unsigned int idx,
				   QDF_MODULE_ID category);

/**
 * qdf_print_is_verbose_enabled() - Get verbose information of a category for
 *                                  the print control object
 *
 * @idx: Index of print control object
 * @category: Category information
 * @verbose: Verbose information
 *
 * Return: Verbose enabled(true) or disabled(false) or invalid input (false)
 */
bool qdf_print_is_verbose_enabled(unsigned int idx,
				  QDF_MODULE_ID category,
				  QDF_TRACE_LEVEL verbose);

/**
 * qdf_print_clean_node_flag() - Clean up node flag for print control object
 *
 * @idx: Index of print control object
 *
 * Return: None
 */
void qdf_print_clean_node_flag(unsigned int idx);

#ifdef DBG_LVL_MAC_FILTERING

/**
 * qdf_print_set_node_flag() - Set flag to enable MAC level filtering
 *
 * @idx: Index of print control object
 * @enable: Enable/Disable bit sent by callee
 *
 * Return: QDF_STATUS_SUCCESS on Success and QDF_STATUS_E_FAILURE on Failure
 */
QDF_STATUS qdf_print_set_node_flag(unsigned int idx,
				   uint8_t enable);

/**
 * qdf_print_get_node_flag() - Get flag that controls MAC level filtering
 *
 * @idx: Index of print control object
 *
 * Return: Flag that indicates enable(1) or disable(0) or invalid(-1)
 */
bool qdf_print_get_node_flag(unsigned int idx);

#endif

#ifdef QCA_WIFI_MODULE_PARAMS_FROM_INI
/**
 * qdf_module_param_handler() - Function to store module params
 *
 * @context: NULL, unused.
 * @key: Name of the module param
 * @value: Value of the module param
 *
 * Handler function to be called from qdf_ini_parse()
 * function when a valid parameter is found in a file.
 *
 * Return: QDF_STATUS_SUCCESS on Success
 */
QDF_STATUS qdf_module_param_handler(void *context, const char *key,
				    const char *value);
#else
static inline
QDF_STATUS qdf_module_param_handler(void *context, const char *key,
				    const char *value)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * qdf_logging_init() - Initialize msg logging functionality
 *
 * Return: void
 */
void qdf_logging_init(void);

/**
 * qdf_logging_exit() - Cleanup msg logging functionality
 *
 * Return: void
 */
void qdf_logging_exit(void);

#define QDF_SYMBOL_LEN __QDF_SYMBOL_LEN

/**
 * qdf_sprint_symbol() - prints the name of a symbol into a string buffer
 * @buffer: the string buffer to print into
 * @addr: address of the symbol to lookup and print
 *
 * Return: number of characters printed
 */
int qdf_sprint_symbol(char *buffer, void *addr);

/**
 * qdf_minidump_init() - Initialize minidump functionality
 *
 *
 * Return: void
 */
static inline
void qdf_minidump_init(void)
{
	__qdf_minidump_init();
}

/**
 * qdf_minidump_deinit() - De-initialize minidump functionality
 *
 *
 * Return: void
 */
static inline
void qdf_minidump_deinit(void)
{
	__qdf_minidump_deinit();
}

/**
 * qdf_minidump_log() - Log memory address to be included in minidump
 * @start_addr: Start address of the memory to be dumped
 * @size: Size in bytes
 * @name: String to identify this entry
 */
static inline
void qdf_minidump_log(void *start_addr,
		      const size_t size, const char *name)
{
	__qdf_minidump_log(start_addr, size, name);
}

/**
 * qdf_minidump_remove() - Remove memory address from minidump
 * @start_addr: Start address of the memory previously added
 * @size: Size in bytes
 * @name: String to identify this entry
 */
static inline
void qdf_minidump_remove(void *start_addr,
			 const size_t size, const char *name)
{
	__qdf_minidump_remove(start_addr, size, name);
}

#endif /* __QDF_TRACE_H */

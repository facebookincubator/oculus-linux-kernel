/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
  * DOC: Declare various struct, macros which are used for private to DP.
  *
  * Note: This file shall not contain public API's prototype/declarations.
  *
  */

#ifndef _WLAN_DP_PRIV_STRUCT_H_
#define _WLAN_DP_PRIV_STRUCT_H_

#include "wlan_dp_public_struct.h"
#include "cdp_txrx_cmn.h"
#include "wlan_dp_cfg.h"
#include "wlan_dp_objmgr.h"
#include <cdp_txrx_misc.h>
#include <wlan_dp_rx_thread.h>
#include "qdf_periodic_work.h"
#include <cds_api.h>
#include "pld_common.h"
#include "wlan_dp_nud_tracking.h"
#include <i_qdf_net_stats.h>
#include <qdf_types.h>
#include "htc_api.h"
#include "wlan_dp_wfds.h"

#ifndef NUM_TX_RX_HISTOGRAM
#define NUM_TX_RX_HISTOGRAM 128
#endif

#define NUM_TX_RX_HISTOGRAM_MASK (NUM_TX_RX_HISTOGRAM - 1)

#if defined(WLAN_FEATURE_DP_BUS_BANDWIDTH) && defined(FEATURE_RUNTIME_PM)
/**
 * enum dp_rtpm_tput_policy_state - states to track runtime_pm tput policy
 * @DP_RTPM_TPUT_POLICY_STATE_INVALID: invalid state
 * @DP_RTPM_TPUT_POLICY_STATE_REQUIRED: state indicating runtime_pm is required
 * @DP_RTPM_TPUT_POLICY_STATE_NOT_REQUIRED: state indicating runtime_pm is NOT
 * required
 */
enum dp_rtpm_tput_policy_state {
	DP_RTPM_TPUT_POLICY_STATE_INVALID,
	DP_RTPM_TPUT_POLICY_STATE_REQUIRED,
	DP_RTPM_TPUT_POLICY_STATE_NOT_REQUIRED
};

/**
 * struct dp_rtpm_tput_policy_context - RTPM throughput policy context
 * @curr_state: current state of throughput policy (RTPM require or not)
 * @wake_lock: wakelock for QDF wake_lock acquire/release APIs
 * @rtpm_lock: lock use for QDF rutime PM prevent/allow APIs
 * @high_tput_vote: atomic variable to keep track of voting
 */
struct dp_rtpm_tput_policy_context {
	enum dp_rtpm_tput_policy_state curr_state;
	qdf_wake_lock_t wake_lock;
	qdf_runtime_lock_t rtpm_lock;
	qdf_atomic_t high_tput_vote;
};
#endif

#define FISA_FLOW_MAX_AGGR_COUNT        16 /* max flow aggregate count */

/**
 * struct wlan_dp_psoc_cfg - DP configuration parameters.
 * @tx_orphan_enable: Enable/Disable tx orphan
 * @rx_mode: rx mode for packet processing
 * @tx_comp_loop_pkt_limit: max # of packets to be processed
 * @rx_reap_loop_pkt_limit: max # of packets to be reaped
 * @rx_hp_oos_update_limit: max # of HP OOS (out of sync)
 * @rx_softirq_max_yield_duration_ns: max duration for RX softirq
 * @periodic_stats_timer_interval: Print selective stats on this specified
 * interval
 * @periodic_stats_timer_duration: duration for which periodic timer should run
 * @bus_bw_super_high_threshold: bus bandwidth super high threshold
 * @bus_bw_ultra_high_threshold: bus bandwidth ultra high threshold
 * @bus_bw_very_high_threshold: bus bandwidth very high threshold
 * @bus_bw_mid_high_threshold: bus bandwidth mid high threshold
 * @bus_bw_dbs_threshold: bus bandwidth for DBS mode threshold
 * @bus_bw_high_threshold: bus bandwidth high threshold
 * @bus_bw_medium_threshold: bandwidth threshold for medium bandwidth
 * @bus_bw_low_threshold: bandwidth threshold for low bandwidth
 * @bus_bw_compute_interval: bus bandwidth compute interval
 * @enable_tcp_delack: enable Dynamic Configuration of Tcp Delayed Ack
 * @enable_tcp_limit_output: enable TCP limit output
 * @enable_tcp_adv_win_scale: enable  TCP adv window scaling
 * @tcp_delack_thres_high: High Threshold inorder to trigger TCP Del Ack
 * indication
 * @tcp_delack_thres_low: Low Threshold inorder to trigger TCP Del Ack
 * indication
 * @tcp_tx_high_tput_thres: High Threshold inorder to trigger High Tx
 * Throughput requirement.
 * @tcp_delack_timer_count: Del Ack Timer Count inorder to trigger TCP Del Ack
 * indication
 * @enable_tcp_param_update: enable tcp parameter update
 * @bus_low_cnt_threshold: Threshold count to trigger low Tput GRO flush skip
 * @enable_latency_crit_clients: Enable the handling of latency critical clients
 * * @del_ack_enable: enable Dynamic Configuration of Tcp Delayed Ack
 * @del_ack_threshold_high: High Threshold inorder to trigger TCP delay ack
 * @del_ack_threshold_low: Low Threshold inorder to trigger TCP delay ack
 * @del_ack_timer_value: Timeout value (ms) to send out all TCP del ack frames
 * @del_ack_pkt_count: The maximum number of TCP delay ack frames
 * @rx_thread_ul_affinity_mask: CPU mask to affine Rx_thread
 * @rx_thread_affinity_mask: CPU mask to affine Rx_thread
 * @cpu_map_list: RPS map for different RX queues
 * @multicast_replay_filter: enable filtering of replayed multicast packets
 * @rx_wakelock_timeout: Amount of time to hold wakelock for RX unicast packets
 * @num_dp_rx_threads: number of dp rx threads
 * @enable_dp_trace: Enable/Disable DP trace
 * @dp_trace_config: DP trace configuration
 * @enable_nud_tracking: Enable/Disable nud tracking
 * @pkt_bundle_threshold_high: tx bundle high threshold
 * @pkt_bundle_threshold_low: tx bundle low threshold
 * @pkt_bundle_timer_value: tx bundle timer value in ms
 * @pkt_bundle_size: tx bundle size
 * @dp_proto_event_bitmap: Control for which protocol type diag log should be
 * sent
 * @fisa_enable: Enable/Disable FISA
 * @icmp_req_to_fw_mark_interval: Interval to mark the ICMP Request packet to
 * be sent to FW.
 * @lro_enable: Enable/Disable lro
 * @gro_enable: Enable/Disable gro
 * @is_rx_fisa_enabled: flag to enable/disable FISA Rx
 * @is_rx_fisa_lru_del_enabled: flag to enable/disable FST entry delete
 */
struct wlan_dp_psoc_cfg {
	bool tx_orphan_enable;

	uint32_t rx_mode;
	uint32_t tx_comp_loop_pkt_limit;
	uint32_t rx_reap_loop_pkt_limit;
	uint32_t rx_hp_oos_update_limit;
	uint64_t rx_softirq_max_yield_duration_ns;
#ifdef WLAN_FEATURE_PERIODIC_STA_STATS
	uint32_t periodic_stats_timer_interval;
	uint32_t periodic_stats_timer_duration;
#endif /* WLAN_FEATURE_PERIODIC_STA_STATS */
#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
	uint32_t bus_bw_super_high_threshold;
	uint32_t bus_bw_ultra_high_threshold;
	uint32_t bus_bw_very_high_threshold;
	uint32_t bus_bw_dbs_threshold;
	uint32_t bus_bw_mid_high_threshold;
	uint32_t bus_bw_high_threshold;
	uint32_t bus_bw_medium_threshold;
	uint32_t bus_bw_low_threshold;
	uint32_t bus_bw_compute_interval;
	uint32_t enable_tcp_delack;
	bool     enable_tcp_limit_output;
	uint32_t enable_tcp_adv_win_scale;
	uint32_t tcp_delack_thres_high;
	uint32_t tcp_delack_thres_low;
	uint32_t tcp_tx_high_tput_thres;
	uint32_t tcp_delack_timer_count;
	bool     enable_tcp_param_update;
	uint32_t bus_low_cnt_threshold;
	bool enable_latency_crit_clients;
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
	bool del_ack_enable;
	uint32_t del_ack_threshold_high;
	uint32_t del_ack_threshold_low;
	uint16_t del_ack_timer_value;
	uint16_t del_ack_pkt_count;
#endif
	uint32_t rx_thread_ul_affinity_mask;
	uint32_t rx_thread_affinity_mask;
	uint8_t cpu_map_list[CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN];
	bool multicast_replay_filter;
	uint32_t rx_wakelock_timeout;
	uint8_t num_dp_rx_threads;
#ifdef CONFIG_DP_TRACE
	bool enable_dp_trace;
	uint8_t dp_trace_config[DP_TRACE_CONFIG_STRING_LENGTH];
#endif
	uint8_t enable_nud_tracking;

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
	uint32_t pkt_bundle_threshold_high;
	uint32_t pkt_bundle_threshold_low;
	uint16_t pkt_bundle_timer_value;
	uint16_t pkt_bundle_size;
#endif
	uint32_t dp_proto_event_bitmap;
	uint32_t fisa_enable;

	int icmp_req_to_fw_mark_interval;

	bool lro_enable;
	bool gro_enable;
#ifdef WLAN_SUPPORT_RX_FISA
	bool is_rx_fisa_enabled;
	bool is_rx_fisa_lru_del_enabled;
#endif
};

/**
 * struct tx_rx_histogram: structure to keep track of tx and rx packets
 *				received over 100ms intervals
 * @interval_rx:	# of rx packets received in the last 100ms interval
 * @interval_tx:	# of tx packets received in the last 100ms interval
 * @next_vote_level:	pld_bus_width_type voting level (high or low)
 *			determined on the basis of total tx and rx packets
 *			received in the last 100ms interval
 * @next_rx_level:	pld_bus_width_type voting level (high or low)
 *			determined on the basis of rx packets received in the
 *			last 100ms interval
 * @next_tx_level:	pld_bus_width_type voting level (high or low)
 *			determined on the basis of tx packets received in the
 *			last 100ms interval
 * @is_rx_pm_qos_high: Capture rx_pm_qos voting
 * @is_tx_pm_qos_high: Capture tx_pm_qos voting
 * @qtime: timestamp when the record is added
 *
 * The structure keeps track of throughput requirements of wlan driver.
 * An entry is added if either of next_vote_level, next_rx_level or
 * next_tx_level changes. An entry is not added for every 100ms interval.
 */
struct tx_rx_histogram {
	uint64_t interval_rx;
	uint64_t interval_tx;
	uint32_t next_vote_level;
	uint32_t next_rx_level;
	uint32_t next_tx_level;
	bool is_rx_pm_qos_high;
	bool is_tx_pm_qos_high;
	uint64_t qtime;
};

/**
 * struct dp_stats - DP stats
 * @tx_rx_stats : Tx/Rx debug stats
 * @arp_stats: arp debug stats
 * @dns_stats: dns debug stats
 * @tcp_stats: tcp debug stats
 * @icmpv4_stats: icmpv4 debug stats
 * @dhcp_stats: dhcp debug stats
 * @eapol_stats: eapol debug stats
 */
struct dp_stats {
	struct dp_tx_rx_stats tx_rx_stats;
	struct dp_arp_stats arp_stats;
	struct dp_dns_stats dns_stats;
	struct dp_tcp_stats tcp_stats;
	struct dp_icmpv4_stats icmpv4_stats;
	struct dp_dhcp_stats dhcp_stats;
	struct dp_eapol_stats eapol_stats;
};

/**
 * enum dhcp_phase - Per Peer DHCP Phases
 * @DHCP_PHASE_ACK: upon receiving DHCP_ACK/NAK message in REQUEST phase or
 *         DHCP_DELINE message in OFFER phase
 * @DHCP_PHASE_DISCOVER: upon receiving DHCP_DISCOVER message in ACK phase
 * @DHCP_PHASE_OFFER: upon receiving DHCP_OFFER message in DISCOVER phase
 * @DHCP_PHASE_REQUEST: upon receiving DHCP_REQUEST message in OFFER phase or
 *         ACK phase (Renewal process)
 */
enum dhcp_phase {
	DHCP_PHASE_ACK,
	DHCP_PHASE_DISCOVER,
	DHCP_PHASE_OFFER,
	DHCP_PHASE_REQUEST
};

/**
 * enum dhcp_nego_status - Per Peer DHCP Negotiation Status
 * @DHCP_NEGO_STOP: when the peer is in ACK phase or client disassociated
 * @DHCP_NEGO_IN_PROGRESS: when the peer is in DISCOVER or REQUEST
 *         (Renewal process) phase
 */
enum dhcp_nego_status {
	DHCP_NEGO_STOP,
	DHCP_NEGO_IN_PROGRESS
};

/*
 * Pending frame type of EAP_FAILURE, bit number used in "pending_eap_frm_type"
 * of sta_info.
 */
#define DP_PENDING_TYPE_EAP_FAILURE  0

enum bss_intf_state {
	BSS_INTF_STOP,
	BSS_INTF_START,
};

struct wlan_dp_sta_info {
	struct qdf_mac_addr sta_mac;
	unsigned long pending_eap_frm_type;
	enum dhcp_phase dhcp_phase;
	enum dhcp_nego_status dhcp_nego_status;
};

struct wlan_dp_conn_info {
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peer_macaddr;
	uint8_t proxy_arp_service;
	uint8_t is_authenticated;
};

/**
 * struct link_monitoring - link speed monitoring related info
 * @enabled: Is link speed monitoring feature enabled
 * @rx_linkspeed_threshold: link speed good/bad threshold
 * @is_rx_linkspeed_good: true means rx link speed good, false means bad
 */
struct link_monitoring {
	uint8_t enabled;
	uint32_t rx_linkspeed_threshold;
	uint8_t is_rx_linkspeed_good;
};

/**
 * struct direct_link_info - direct link configuration items
 * @config_set: is the direct link config active
 * @low_latency: is low latency enabled
 */
struct direct_link_info {
	bool config_set;
	bool low_latency;
};

/**
 * struct dp_fisa_reo_mismatch_stats - reo mismatch sub-case stats for FISA
 * @allow_cce_match: packet allowed due to cce mismatch
 * @allow_fse_metdata_mismatch: packet allowed since it belongs to same flow,
 *			only fse_metadata is not same.
 * @allow_non_aggr: packet allowed due to any other reason.
 */
struct dp_fisa_reo_mismatch_stats {
	uint32_t allow_cce_match;
	uint32_t allow_fse_metdata_mismatch;
	uint32_t allow_non_aggr;
};

/**
 * struct dp_fisa_stats - FISA stats
 * @invalid_flow_index: flow index invalid from RX HW TLV
 * @update_deferred: workqueue deferred due to suspend
 * @reo_mismatch: REO ID mismatch
 * @incorrect_rdi: Incorrect REO dest indication in TLV
 *		   (typically used for RDI = 0)
 */
struct dp_fisa_stats {
	uint32_t invalid_flow_index;
	uint32_t update_deferred;
	struct dp_fisa_reo_mismatch_stats reo_mismatch;
	uint32_t incorrect_rdi;
};

/**
 * enum fisa_aggr_ret - FISA aggregation return code
 * @FISA_AGGR_DONE: FISA aggregation done
 * @FISA_AGGR_NOT_ELIGIBLE: Not eligible for FISA aggregation
 * @FISA_FLUSH_FLOW: FISA flow flushed
 */
enum fisa_aggr_ret {
	FISA_AGGR_DONE,
	FISA_AGGR_NOT_ELIGIBLE,
	FISA_FLUSH_FLOW
};

/**
 * struct fisa_pkt_hist - FISA Packet history structure
 * @tlv_hist: array of TLV history
 * @ts_hist: array of timestamps of fisa packets
 * @idx: index indicating the next location to be used in the array.
 */
struct fisa_pkt_hist {
	uint8_t *tlv_hist;
	qdf_time_t ts_hist[FISA_FLOW_MAX_AGGR_COUNT];
	uint32_t idx;
};

/**
 * struct dp_fisa_rx_sw_ft - FISA Flow table entry
 * @hw_fse: HAL Rx Flow Search Entry which matches HW definition
 * @flow_hash: Flow hash value
 * @flow_id_toeplitz: toeplitz hash value
 * @flow_id: Flow index, equivalent to hash value truncated to FST size
 * @stats: Stats tracking for this flow
 * @is_ipv4_addr_entry: Flag indicating whether flow is IPv4 address tuple
 * @is_valid: Flag indicating whether flow is valid
 * @is_populated: Flag indicating whether flow is populated
 * @is_flow_udp: Flag indicating whether flow is UDP stream
 * @is_flow_tcp: Flag indicating whether flow is TCP stream
 * @head_skb: HEAD skb where flow is aggregated
 * @cumulative_l4_checksum: Cumulative L4 checksum
 * @adjusted_cumulative_ip_length: Cumulative IP length
 * @cur_aggr: Current aggregate length of flow
 * @napi_flush_cumulative_l4_checksum: Cumulative L4 chekcsum for current
 *				       NAPI flush
 * @napi_flush_cumulative_ip_length: Cumulative IP length
 * @last_skb: The last skb aggregated in the FISA flow
 * @head_skb_ip_hdr_offset: IP header offset
 * @head_skb_l4_hdr_offset: L4 header offset
 * @rx_flow_tuple_info: RX tuple information
 * @napi_id: NAPI ID (REO ID) on which the flow is being received
 * @vdev: VDEV handle corresponding to the FLOW
 * @dp_intf: DP interface handle corresponding to the flow
 * @bytes_aggregated: Number of bytes currently aggregated
 * @flush_count: Number of Flow flushes done
 * @aggr_count: Aggregation count
 * @do_not_aggregate: Flag to indicate not to aggregate this flow
 * @hal_cumultive_ip_len: HAL cumulative IP length
 * @dp_ctx: DP component handle
 * @soc_hdl: DP SoC handle
 * @last_hal_aggr_count: last aggregate count fetched from RX PKT TLV
 * @cur_aggr_gso_size: Current aggreagtesd GSO size
 * @head_skb_udp_hdr: UDP header address for HEAD skb
 * @frags_cumulative_len:
 * @cmem_offset: CMEM offset
 * @metadata:
 * @reo_dest_indication: REO destination indication for the FLOW
 * @flow_init_ts: FLOW init timestamp
 * @last_accessed_ts: Timestamp when the flow was last accessed
 * @pkt_hist: FISA aggreagtion packets history
 * @same_mld_vdev_mismatch: Packets flushed after vdev_mismatch on same MLD
 * @add_timestamp: FISA entry created timestamp
 */
struct dp_fisa_rx_sw_ft {
	void *hw_fse;
	uint32_t flow_hash;
	uint32_t flow_id_toeplitz;
	uint32_t flow_id;
	struct cdp_flow_stats stats;
	uint8_t is_ipv4_addr_entry;
	uint8_t is_valid;
	uint8_t is_populated;
	uint8_t is_flow_udp;
	uint8_t is_flow_tcp;
	qdf_nbuf_t head_skb;
	uint16_t cumulative_l4_checksum;
	uint16_t adjusted_cumulative_ip_length;
	uint16_t cur_aggr;
	uint16_t napi_flush_cumulative_l4_checksum;
	uint16_t napi_flush_cumulative_ip_length;
	qdf_nbuf_t last_skb;
	uint32_t head_skb_ip_hdr_offset;
	uint32_t head_skb_l4_hdr_offset;
	struct cdp_rx_flow_tuple_info rx_flow_tuple_info;
	uint8_t napi_id;
	struct dp_vdev *vdev;
	struct wlan_dp_intf *dp_intf;
	uint64_t bytes_aggregated;
	uint32_t flush_count;
	uint32_t aggr_count;
	uint8_t do_not_aggregate;
	uint16_t hal_cumultive_ip_len;
	struct wlan_dp_psoc_context *dp_ctx;
	/* TODO - Only reference needed to this is to get vdev.
	 * Once that ref is removed, this field can be deleted
	 */
	struct dp_soc *soc_hdl;
	uint32_t last_hal_aggr_count;
	uint32_t cur_aggr_gso_size;
	qdf_net_udphdr_t *head_skb_udp_hdr;
	uint16_t frags_cumulative_len;
	uint32_t cmem_offset;
	uint32_t metadata;
	uint32_t reo_dest_indication;
	qdf_time_t flow_init_ts;
	qdf_time_t last_accessed_ts;
#ifdef WLAN_SUPPORT_RX_FISA_HIST
	struct fisa_pkt_hist pkt_hist;
#endif
	uint64_t same_mld_vdev_mismatch;
	uint64_t add_timestamp;
};

#define DP_RX_GET_SW_FT_ENTRY_SIZE sizeof(struct dp_fisa_rx_sw_ft)
#define MAX_FSE_CACHE_FL_HST 10
/**
 * struct fse_cache_flush_history - Debug history cache flush
 * @timestamp: Entry update timestamp
 * @flows_added: Number of flows added for this flush
 * @flows_deleted: Number of flows deleted for this flush
 */
struct fse_cache_flush_history {
	uint64_t timestamp;
	uint32_t flows_added;
	uint32_t flows_deleted;
};

/**
 * struct dp_rx_fst - FISA handle
 * @base: Software (DP) FST
 * @dp_ctx: DP component handle
 * @hal_rx_fst: Pointer to HAL FST
 * @hal_rx_fst_base_paddr: Base physical address of HAL RX HW FST
 * @max_entries: Maximum number of flows FSE supports
 * @num_entries: Num entries in flow table
 * @max_skid_length: SKID Length
 * @hash_mask: Hash mask to obtain legitimate hash entry
 * @dp_rx_fst_lock: Lock for adding/deleting entries of FST
 * @add_flow_count: Num of flows added
 * @del_flow_count: Num of flows deleted
 * @hash_collision_cnt: Num hash collisions
 * @soc_hdl: DP SoC handle
 * @fse_cache_flush_posted: Num FSE cache flush cmds posted
 * @fse_cache_flush_timer: FSE cache flush timer
 * @fse_cache_flush_allow: Flag to indicate if FSE cache flush is allowed
 * @cache_fl_rec: FSE cache flush history
 * @stats: FISA stats
 * @fst_update_work: FST CMEM update work
 * @fst_update_wq: FST CMEM update workqueue
 * @fst_update_list: List to post event to CMEM update work
 * @meta_counter:
 * @cmem_ba:
 * @dp_rx_sw_ft_lock: SW FST lock
 * @cmem_resp_event: CMEM response event indicator
 * @flow_deletion_supported: Flag to indicate if flow delete is supported
 * @fst_in_cmem: Flag to indicate if FST is stored in CMEM
 * @pm_suspended: Flag to indicate if driver is suspended
 * @fst_wq_defer:
 * @rx_hash_enabled: Flag to indicate if Hash based routing supported
 * @rx_toeplitz_hash_key: hash key
 * @rx_pkt_tlv_size: RX packet TLV size
 */
struct dp_rx_fst {
	uint8_t *base;
	struct wlan_dp_psoc_context *dp_ctx;
	struct hal_rx_fst *hal_rx_fst;
	uint64_t hal_rx_fst_base_paddr;
	uint16_t max_entries;
	uint16_t num_entries;
	uint16_t max_skid_length;
	uint32_t hash_mask;
	qdf_spinlock_t dp_rx_fst_lock;
	uint32_t add_flow_count;
	uint32_t del_flow_count;
	uint32_t hash_collision_cnt;
	struct dp_soc *soc_hdl;
	qdf_atomic_t fse_cache_flush_posted;
	qdf_timer_t fse_cache_flush_timer;
	bool fse_cache_flush_allow;
	struct fse_cache_flush_history cache_fl_rec[MAX_FSE_CACHE_FL_HST];
	struct dp_fisa_stats stats;

	/* CMEM params */
	qdf_work_t fst_update_work;
	qdf_workqueue_t *fst_update_wq;
	qdf_list_t fst_update_list;
	uint32_t meta_counter;
	uint32_t cmem_ba;
	qdf_spinlock_t dp_rx_sw_ft_lock[MAX_REO_DEST_RINGS];
	qdf_event_t cmem_resp_event;
	bool flow_deletion_supported;
	bool fst_in_cmem;
	qdf_atomic_t pm_suspended;
	bool fst_wq_defer;
	bool rx_hash_enabled;
	uint8_t *rx_toeplitz_hash_key;
	uint16_t rx_pkt_tlv_size;
};

/**
 * struct wlan_dp_intf - DP interface object related info
 * @dp_ctx: DP context reference
 * @link_monitoring: Link monitoring related info
 * @mac_addr: Device MAC address
 * @device_mode: Device Mode
 * @intf_id: Interface ID
 * @node: list node for membership in the interface list
 * @dev: netdev reference
 * @txrx_ops: Interface tx-rx ops
 * @dp_stats: Device TX/RX statistics
 * @is_sta_periodic_stats_enabled: Indicate whether to display sta periodic
 * stats
 * @periodic_stats_timer_count: count of periodic stats timer
 * @periodic_stats_timer_counter: periodic stats timer counter
 * @sta_periodic_stats_lock: sta periodic stats lock
 * @stats: netdev stats
 * @con_status: con_status value
 * @dad: dad value
 * @pkt_type_bitmap: packet type bitmap value
 * @track_arp_ip: track ARP ip
 * @dns_payload: dns payload
 * @track_dns_domain_len: dns domain length
 * @track_src_port: track source port value
 * @track_dest_port: track destination port value
 * @track_dest_ipv4: track destination ipv4 value
 * @prev_rx_packets: Rx packets received N/W interface
 * @prev_tx_packets: Tx packets transmitted on N/W interface
 * @prev_tx_bytes: Tx bytes transmitted on N/W interface
 * @prev_fwd_tx_packets: forwarded tx packets count
 * @prev_fwd_rx_packets: forwarded rx packets count
 * @nud_tracking: NUD tracking
 * @mic_work: Work to handle MIC error
 * @num_active_task: Active task count
 * @sap_tx_block_mask: SAP TX block mask
 * @gro_disallowed: GRO disallowed flag
 * @gro_flushed: GRO flushed flag
 * @fisa_disallowed: Flag to indicate fisa aggregation not to be done for a
 *		     particular rx_context
 * @fisa_force_flushed: Flag to indicate FISA flow has been flushed for a
 *			particular rx_context
 * @runtime_disable_rx_thread: Runtime Rx thread flag
 * @rx_stack: function pointer Rx packet handover
 * @tx_fn: function pointer to send Tx packet
 * @bss_state: AP BSS state
 * @qdf_sta_eap_frm_done_event: EAP frame event management
 * @traffic_end_ind: store traffic end indication info
 * @direct_link_config: direct link configuration parameters
 * @num_links: Number of links for this DP interface
 * @def_link: Pointer to default link (usually used for TX operation)
 * @dp_link_list_lock: Lock to protect dp_link_list operatiosn
 * @dp_link_list: List of dp_links for this DP interface
 * @fpm_ctx: Flow policy manager context
 * @fim_ctx: Flow identification manager context
 */
struct wlan_dp_intf {
	struct wlan_dp_psoc_context *dp_ctx;

	struct link_monitoring link_monitoring;

	struct qdf_mac_addr mac_addr;

	enum QDF_OPMODE device_mode;

	qdf_list_node_t node;

	qdf_netdev_t dev;
	struct ol_txrx_ops txrx_ops;
	struct dp_stats dp_stats;
#ifdef WLAN_FEATURE_PERIODIC_STA_STATS
	bool is_sta_periodic_stats_enabled;
	uint16_t periodic_stats_timer_count;
	uint32_t periodic_stats_timer_counter;
	qdf_mutex_t sta_periodic_stats_lock;
#endif /* WLAN_FEATURE_PERIODIC_STA_STATS */
	qdf_net_dev_stats stats;
	bool con_status;
	bool dad;
	uint32_t pkt_type_bitmap;
	uint32_t track_arp_ip;
	uint8_t dns_payload[256];
	uint32_t track_dns_domain_len;
	uint32_t track_src_port;
	uint32_t track_dest_port;
	uint32_t track_dest_ipv4;
#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
	unsigned long prev_rx_packets;
	unsigned long prev_tx_packets;
	unsigned long prev_tx_bytes;
	uint64_t prev_fwd_tx_packets;
	uint64_t prev_fwd_rx_packets;
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/
	struct dp_mic_work mic_work;
#ifdef WLAN_NUD_TRACKING
	struct dp_nud_tracking_info nud_tracking;
#endif
	qdf_atomic_t num_active_task;
	uint32_t sap_tx_block_mask;

	qdf_atomic_t gro_disallowed;
	uint8_t gro_flushed[DP_MAX_RX_THREADS];

#ifdef WLAN_SUPPORT_RX_FISA
	/*
	 * Params used for controlling the fisa aggregation dynamically
	 */
	uint8_t fisa_disallowed[MAX_REO_DEST_RINGS];
	uint8_t fisa_force_flushed[MAX_REO_DEST_RINGS];
#endif

	bool runtime_disable_rx_thread;

	enum bss_intf_state bss_state;
	qdf_event_t qdf_sta_eap_frm_done_event;
	struct dp_traffic_end_indication traffic_end_ind;
#ifdef FEATURE_DIRECT_LINK
	struct direct_link_info direct_link_config;
#endif
	uint8_t num_links;
	struct wlan_dp_link *def_link;
	qdf_spinlock_t dp_link_list_lock;
	qdf_list_t dp_link_list;
#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
	struct fpm_table *fpm_ctx;
	struct fim_vdev_ctx *fim_ctx;
#endif
};

#define WLAN_DP_LINK_MAGIC 0x5F44505F4C494E4B	/* "_DP_LINK" in ASCII */

/**
 * struct wlan_dp_link - DP link (corresponds to objmgr vdev)
 * @node: list node for membership in the DP links list
 * @magic: magic number to identify validity of dp_link
 * @link_id: ID for this DP link (Same as vdev_id)
 * @mac_addr: mac address of this link
 * @dp_intf: Parent DP interface for this DP link
 * @vdev: object manager vdev context
 * @vdev_lock: vdev spin lock
 * @conn_info: STA connection information
 * @destroyed: flag to indicate dp_link destroyed (logical delete)
 * @cdp_vdev_registered: flag to indicate if corresponding CDP vdev
 *			 is registered
 * @cdp_vdev_deleted: flag to indicate if corresponding CDP vdev is deleted
 * @inactive_list_elem: list node for membership in dp link inactive list
 */
struct wlan_dp_link {
	qdf_list_node_t node;
	uint64_t magic;
	uint8_t link_id;
	struct qdf_mac_addr mac_addr;
	struct wlan_dp_intf *dp_intf;
	struct wlan_objmgr_vdev *vdev;
	qdf_spinlock_t vdev_lock;
	struct wlan_dp_conn_info conn_info;
	uint8_t destroyed : 1,
		cdp_vdev_registered : 1,
		cdp_vdev_deleted : 1;
	TAILQ_ENTRY(wlan_dp_link) inactive_list_elem;
};

/**
 * enum RX_OFFLOAD - Receive offload modes
 * @CFG_LRO_ENABLED: Large Rx offload
 * @CFG_GRO_ENABLED: Generic Rx Offload
 */
enum RX_OFFLOAD {
	CFG_LRO_ENABLED = 1,
	CFG_GRO_ENABLED,
};

#ifdef FEATURE_DIRECT_LINK
/**
 * struct dp_direct_link_context - Datapath Direct Link context
 * @dp_ctx: pointer to DP psoc priv context
 * @lpass_ep_id: LPASS data msg service endpoint id
 * @direct_link_refill_ring_hdl: Direct Link refill ring handle
 * @dl_wfds: pointer to direct link WFDS context
 */
struct dp_direct_link_context {
	struct wlan_dp_psoc_context *dp_ctx;
	HTC_ENDPOINT_ID lpass_ep_id;
	struct dp_srng *direct_link_refill_ring_hdl;
	struct dp_direct_link_wfds_context *dl_wfds;
};
#endif

/**
 * struct wlan_dp_psoc_context - psoc related data required for DP
 * @psoc: object manager psoc context
 * @pdev: object manager pdev context
 * @qdf_dev: qdf device
 * @dp_cfg: place holder for DP configuration
 * @cdp_soc: CDP SoC handle
 * @hif_handle: HIF handle
 * @hal_soc: HAL SoC handle
 * @intf_list_lock: DP interfaces list lock
 * @intf_list: DP interfaces list
 * @rps: rps
 * @dynamic_rps: dynamic rps
 * @enable_rxthread: Enable/Disable rx thread
 * @enable_dp_rx_threads: Enable/Disable DP rx threads
 * @napi_enable: Enable/Disable napi
 * @dp_ops: DP callbacks registered from other modules
 * @sb_ops: South bound direction call backs registered in DP
 * @nb_ops: North bound direction call backs registered in DP
 * @en_tcp_delack_no_lro: Enable/Disable tcp delack no lro
 * @no_rx_offload_pkt_cnt: no of rx offload packet count
 * @no_tx_offload_pkt_cnt: no of tx offload packet count
 * @is_suspend: to check whether syetem suspend or not
 * @is_wiphy_suspended: to check whether wiphy suspend or not
 * @num_latency_critical_clients: num latency critical clients
 * @high_bus_bw_request: high bus bandwidth request
 * @bw_vote_time: bus bandwidth vote time
 * @bus_bw_work: work for periodically computing DDR bus bandwidth requirements
 * @cur_vote_level: Current vote level
 * @prev_no_rx_offload_pkts: no of previous rx offload packets
 * @prev_rx_offload_pkts: previous rx offload packets
 * @prev_no_tx_offload_pkts: no of previous tx offload packets
 * @prev_tx_offload_pkts: previous tx offload packets
 * @cur_tx_level: Current Tx level
 * @prev_tx: previous tx
 * @low_tput_gro_enable: Enable/Disable low tput gro
 * @bus_bw_lock: Bus bandwidth work lock
 * @cur_rx_level: Current Rx level
 * @bus_low_vote_cnt: bus low level count
 * @disable_rx_ol_in_concurrency: disable RX offload in concurrency scenarios
 * @disable_rx_ol_in_low_tput: disable RX offload in tput scenarios
 * @txrx_hist_idx: txrx histogram index
 * @rx_high_ind_cnt: rx high_ind count
 * @receive_offload_cb: receive offload cb
 * @dp_agg_param: DP aggregation parameter
 * @dp_agg_param.rx_aggregation:
 * @dp_agg_param.gro_force_flush:
 * @dp_agg_param.tc_based_dyn_gro:
 * @dp_agg_param.tc_ingress_prio:
 * @rtpm_tput_policy_ctx: Runtime Tput policy context
 * @txrx_hist: TxRx histogram
 * @bbm_ctx: bus bandwidth manager context
 * @dp_direct_link_lock: Direct link mutex lock
 * @dp_direct_link_ctx: DP Direct Link context
 * @arp_connectivity_map: ARP connectivity map
 * @rx_wake_lock: rx wake lock
 * @ol_enable: Enable/Disable offload
 * @rx_fst: FST handle
 * @fst_cmem_base: FST base in CMEM
 * @fst_in_cmem: Flag indicating if FST is in CMEM or not
 * @fisa_enable: Flag to indicate if FISA is enabled or not
 * @fisa_lru_del_enable: Flag to indicate if LRU flow delete is enabled
 * @fisa_dynamic_aggr_size_support: Indicate dynamic aggr size programming support
 * @skip_fisa_param: FISA skip params structure
 * @skip_fisa_param.skip_fisa: Flag to skip FISA aggr inside @skip_fisa_param
 * @skip_fisa_param.fisa_force_flush: Force flush inside @skip_fisa_param
 * @fst_cmem_size: CMEM size for FISA flow table
 * @inactive_dp_link_list: inactive DP links list
 * @dp_link_del_lock: DP link delete operation lock
 * @svc_ctx: service class context
 */
struct wlan_dp_psoc_context {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	qdf_device_t qdf_dev;
	struct wlan_dp_psoc_cfg dp_cfg;
	ol_txrx_soc_handle cdp_soc;
	struct hif_opaque_softc *hif_handle;
	void *hal_soc;

	qdf_spinlock_t intf_list_lock;
	qdf_list_t intf_list;

	bool rps;
	bool dynamic_rps;
	bool enable_rxthread;
	bool enable_dp_rx_threads;
	bool napi_enable;

	struct wlan_dp_psoc_callbacks dp_ops;
	struct wlan_dp_psoc_sb_ops sb_ops;
	struct wlan_dp_psoc_nb_ops nb_ops;

	bool en_tcp_delack_no_lro;
	uint64_t no_rx_offload_pkt_cnt;
	uint64_t no_tx_offload_pkt_cnt;
	bool is_suspend;
	bool is_wiphy_suspended;
	qdf_atomic_t num_latency_critical_clients;
	uint8_t high_bus_bw_request;
	uint64_t bw_vote_time;
#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
	struct qdf_periodic_work bus_bw_work;
	int cur_vote_level;
	qdf_spinlock_t bus_bw_lock;
	int cur_rx_level;
	uint64_t prev_no_rx_offload_pkts;
	uint64_t prev_rx_offload_pkts;
	uint64_t prev_no_tx_offload_pkts;
	uint64_t prev_tx_offload_pkts;
	int cur_tx_level;
	uint64_t prev_tx;
	qdf_atomic_t low_tput_gro_enable;
	uint32_t bus_low_vote_cnt;
#ifdef FEATURE_RUNTIME_PM
	struct dp_rtpm_tput_policy_context rtpm_tput_policy_ctx;
#endif
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/
	qdf_atomic_t disable_rx_ol_in_concurrency;
	qdf_atomic_t disable_rx_ol_in_low_tput;

	uint16_t txrx_hist_idx;
	struct tx_rx_histogram *txrx_hist;

	uint32_t rx_high_ind_cnt;
#ifdef FEATURE_BUS_BANDWIDTH_MGR
	struct bbm_context *bbm_ctx;
#endif

	QDF_STATUS(*receive_offload_cb)(struct wlan_dp_intf *, qdf_nbuf_t nbuf);

	struct {
		qdf_atomic_t rx_aggregation;
		uint8_t gro_force_flush[DP_MAX_RX_THREADS];
		bool tc_based_dyn_gro;
		uint32_t tc_ingress_prio;
	}
	dp_agg_param;

	uint32_t arp_connectivity_map;

	qdf_wake_lock_t rx_wake_lock;

	enum RX_OFFLOAD ol_enable;
#ifdef FEATURE_DIRECT_LINK
	qdf_mutex_t dp_direct_link_lock;
	struct dp_direct_link_context *dp_direct_link_ctx;
#endif
#ifdef WLAN_SUPPORT_RX_FISA
	struct dp_rx_fst *rx_fst;
	uint64_t fst_cmem_base;
	bool fst_in_cmem;
	uint8_t fisa_enable;
	uint8_t fisa_lru_del_enable;
	bool fisa_dynamic_aggr_size_support;
	/*
	 * Params used for controlling the fisa aggregation dynamically
	 */
	struct {
		qdf_atomic_t skip_fisa;
		uint8_t fisa_force_flush[MAX_REO_DEST_RINGS];
	} skip_fisa_param;

	/*
	 * CMEM address and size for FST in CMEM, This is the address
	 * shared during init time.
	 */
	uint64_t fst_cmem_size;

#endif
	TAILQ_HEAD(, wlan_dp_link) inactive_dp_link_list;
	qdf_spinlock_t dp_link_del_lock;
#ifdef WLAN_SUPPORT_SERVICE_CLASS
	struct dp_svc_ctx *svc_ctx;
#endif
};

#ifdef WLAN_DP_PROFILE_SUPPORT
/**
 * enum wlan_dp_cfg_param_type - param context type
 * @DP_TX_DESC_NUM_CFG: Number of TX desc
 * @DP_TX_EXT_DESC_NUM_CFG: Number of TX ext desc
 * @DP_TX_RING_SIZE_CFG: TX ring size
 * @DP_TX_COMPL_RING_SIZE_CFG: TX completion ring size
 * @DP_RX_SW_DESC_NUM_CFG: Number of RX S.W descriptors
 * @DP_REO_DST_RING_SIZE_CFG: RX ring size
 * @DP_RXDMA_BUF_RING_SIZE_CFG: RXDMA BUF ring size
 * @DP_RXDMA_REFILL_RING_SIZE_CFG: RXDMA refill ring size
 * @DP_RX_REFILL_POOL_NUM_CFG: Refill buffer pool size
 */
enum wlan_dp_cfg_param_type {
	DP_TX_DESC_NUM_CFG,
	DP_TX_EXT_DESC_NUM_CFG,
	DP_TX_RING_SIZE_CFG,
	DP_TX_COMPL_RING_SIZE_CFG,
	DP_RX_SW_DESC_NUM_CFG,
	DP_REO_DST_RING_SIZE_CFG,
	DP_RXDMA_BUF_RING_SIZE_CFG,
	DP_RXDMA_REFILL_RING_SIZE_CFG,
	DP_RX_REFILL_POOL_NUM_CFG,
};

/**
 * struct wlan_dp_memory_profile_ctx - element representing DP config param info
 * @param_type: DP config param type
 * @size: size/length of the param to be selected
 */
struct wlan_dp_memory_profile_ctx {
	enum wlan_dp_cfg_param_type param_type;
	uint32_t size;
};

/**
 * struct wlan_dp_memory_profile_info - Current memory profile info
 * @is_selected: profile is selected or not
 * @ctx: DP memory profile context
 * @size: size of profile
 */
struct wlan_dp_memory_profile_info {
	bool is_selected;
	struct wlan_dp_memory_profile_ctx *ctx;
	int size;
};
#endif

#endif /* end  of _WLAN_DP_PRIV_STRUCT_H_ */

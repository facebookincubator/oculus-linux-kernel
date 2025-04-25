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
 * DOC: Contains DP public data structure definitions.
 *
 */

#ifndef _WLAN_DP_PUBLIC_STRUCT_H_
#define _WLAN_DP_PUBLIC_STRUCT_H_

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_global_obj.h"
#include "qdf_status.h"
#include <wlan_nlink_common.h>
#include <qca_vendor.h>
#include <ani_system_defs.h>
#include "cdp_txrx_ops.h"
#include <qdf_defer.h>
#include <qdf_types.h>
#include <qdf_hashtable.h>
#include <qdf_notifier.h>
#include "wlan_dp_rx_thread.h"

#define DP_MAX_SUBTYPES_TRACKED	4

enum dp_rx_offld_flush_cb {
	DP_RX_FLUSH_LRO,
	DP_RX_FLUSH_THREAD,
	DP_RX_FLUSH_NAPI,
};

enum dp_nbuf_push_type {
	DP_NBUF_PUSH_NI,
	DP_NBUF_PUSH_NAPI,
	DP_NBUF_PUSH_BH_DISABLE,
	DP_NBUF_PUSH_SIMPLE,
};

/**
 * struct dp_eapol_stats - eapol debug stats count
 * @eapol_m1_count: eapol m1 count
 * @eapol_m2_count: eapol m2 count
 * @eapol_m3_count: eapol m3 count
 * @eapol_m4_count: eapol m4 count
 * @tx_dropped: no of tx frames dropped by host
 * @tx_noack_cnt: no of frames for which there is no ack
 * @rx_delivered: no. of frames delivered to network stack
 * @rx_refused: no of frames not delivered to network stack
 */
struct dp_eapol_stats {
	uint16_t eapol_m1_count;
	uint16_t eapol_m2_count;
	uint16_t eapol_m3_count;
	uint16_t eapol_m4_count;
	uint16_t tx_dropped[DP_MAX_SUBTYPES_TRACKED];
	uint16_t tx_noack_cnt[DP_MAX_SUBTYPES_TRACKED];
	uint16_t rx_delivered[DP_MAX_SUBTYPES_TRACKED];
	uint16_t rx_refused[DP_MAX_SUBTYPES_TRACKED];
};

/**
 * struct dp_dhcp_stats - dhcp debug stats count
 * @dhcp_dis_count: dhcp discovery count
 * @dhcp_off_count: dhcp offer count
 * @dhcp_req_count: dhcp request count
 * @dhcp_ack_count: dhcp ack count
 * @tx_dropped: no of tx frames dropped by host
 * @tx_noack_cnt: no of frames for which there is no ack
 * @rx_delivered: no. of frames delivered to network stack
 * @rx_refused: no of frames not delivered to network stack
 */
struct dp_dhcp_stats {
	uint16_t dhcp_dis_count;
	uint16_t dhcp_off_count;
	uint16_t dhcp_req_count;
	uint16_t dhcp_ack_count;
	uint16_t tx_dropped[DP_MAX_SUBTYPES_TRACKED];
	uint16_t tx_noack_cnt[DP_MAX_SUBTYPES_TRACKED];
	uint16_t rx_delivered[DP_MAX_SUBTYPES_TRACKED];
	uint16_t rx_refused[DP_MAX_SUBTYPES_TRACKED];
};

#ifdef TX_MULTIQ_PER_AC
#define TX_GET_QUEUE_IDX(ac, off) (((ac) * TX_QUEUES_PER_AC) + (off))
#define TX_QUEUES_PER_AC 4
#else
#define TX_GET_QUEUE_IDX(ac, off) (ac)
#define TX_QUEUES_PER_AC 1
#endif

/** Number of Tx Queues */
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || \
	defined(QCA_HL_NETDEV_FLOW_CONTROL) || \
	defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/* Only one HI_PRIO queue */
#define NUM_TX_QUEUES (4 * TX_QUEUES_PER_AC + 1)
#else
#define NUM_TX_QUEUES (4 * TX_QUEUES_PER_AC)
#endif

#ifndef NUM_CPUS
#ifdef QCA_CONFIG_SMP
#define NUM_CPUS NR_CPUS
#else
#define NUM_CPUS 1
#endif
#endif

/**
 * struct dp_arp_stats - arp debug stats count
 * @tx_arp_req_count: no. of arp req received from network stack
 * @rx_arp_rsp_count: no. of arp res received from FW
 * @tx_dropped: no. of arp req dropped at hdd layer
 * @rx_dropped: no. of arp res dropped
 * @rx_delivered: no. of arp res delivered to network stack
 * @rx_refused: no of arp rsp refused (not delivered) to network stack
 * @tx_host_fw_sent: no of arp req sent by FW OTA
 * @rx_host_drop_reorder: no of arp res dropped by host
 * @rx_fw_cnt: no of arp res received by FW
 * @tx_ack_cnt: no of arp req acked by FW
 */
struct dp_arp_stats {
	uint16_t tx_arp_req_count;
	uint16_t rx_arp_rsp_count;
	uint16_t tx_dropped;
	uint16_t rx_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_host_fw_sent;
	uint16_t rx_host_drop_reorder;
	uint16_t rx_fw_cnt;
	uint16_t tx_ack_cnt;
};

/**
 * struct dp_set_arp_stats_params - set/reset arp stats
 * @vdev_id: session id
 * @flag: enable/disable stats
 * @pkt_type: type of packet(1 - arp)
 * @ip_addr: subnet ipv4 address in case of encrypted packets
 * @pkt_type_bitmap: pkt bitmap
 * @tcp_src_port: tcp src port for pkt tracking
 * @tcp_dst_port: tcp dst port for pkt tracking
 * @icmp_ipv4: target ipv4 address to track ping packets
 * @reserved: reserved
 */
struct dp_set_arp_stats_params {
	uint32_t vdev_id;
	uint8_t flag;
	uint8_t pkt_type;
	uint32_t ip_addr;
	uint32_t pkt_type_bitmap;
	uint32_t tcp_src_port;
	uint32_t tcp_dst_port;
	uint32_t icmp_ipv4;
	uint32_t reserved;
};

/**
 * struct dp_get_arp_stats_params - get arp stats from firmware
 * @pkt_type: packet type(1 - ARP)
 * @vdev_id: session id
 */
struct dp_get_arp_stats_params {
	uint8_t pkt_type;
	uint32_t vdev_id;
};

/**
 * struct dp_dns_stats - dns debug stats count
 * @tx_dns_req_count: no. of dns query received from network stack
 * @rx_dns_rsp_count: no. of dns res received from FW
 * @tx_dropped: no. of dns query dropped at hdd layer
 * @rx_delivered: no. of dns res delivered to network stack
 * @rx_refused: no of dns res refused (not delivered) to network stack
 * @tx_host_fw_sent: no of dns query sent by FW OTA
 * @rx_host_drop: no of dns res dropped by host
 * @tx_ack_cnt: no of dns req acked by FW
 */
struct dp_dns_stats {
	uint16_t tx_dns_req_count;
	uint16_t rx_dns_rsp_count;
	uint16_t tx_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_host_fw_sent;
	uint16_t rx_host_drop;
	uint16_t tx_ack_cnt;
};

/**
 * struct dp_tcp_stats - tcp debug stats count
 * @tx_tcp_syn_count: no. of tcp syn received from network stack
 * @tx_tcp_ack_count: no. of tcp ack received from network stack
 * @rx_tcp_syn_ack_count: no. of tcp syn ack received from FW
 * @tx_tcp_syn_dropped: no. of tcp syn dropped at hdd layer
 * @tx_tcp_ack_dropped: no. of tcp ack dropped at hdd layer
 * @rx_delivered: no. of tcp syn ack delivered to network stack
 * @rx_refused: no of tcp syn ack refused (not delivered) to network stack
 * @tx_tcp_syn_host_fw_sent: no of tcp syn sent by FW OTA
 * @tx_tcp_ack_host_fw_sent: no of tcp ack sent by FW OTA
 * @rx_host_drop: no of tcp syn ack dropped by host
 * @rx_fw_cnt: no of tcp res received by FW
 * @tx_tcp_syn_ack_cnt: no of tcp syn acked by FW
 * @tx_tcp_ack_ack_cnt: no of tcp ack acked by FW
 * @is_tcp_syn_ack_rcv: flag to check tcp syn ack received or not
 * @is_tcp_ack_sent: flag to check tcp ack sent or not
 */
struct dp_tcp_stats {
	uint16_t tx_tcp_syn_count;
	uint16_t tx_tcp_ack_count;
	uint16_t rx_tcp_syn_ack_count;
	uint16_t tx_tcp_syn_dropped;
	uint16_t tx_tcp_ack_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_tcp_syn_host_fw_sent;
	uint16_t tx_tcp_ack_host_fw_sent;
	uint16_t rx_host_drop;
	uint16_t rx_fw_cnt;
	uint16_t tx_tcp_syn_ack_cnt;
	uint16_t tx_tcp_ack_ack_cnt;
	bool is_tcp_syn_ack_rcv;
	bool is_tcp_ack_sent;

};

/**
 * struct dp_icmpv4_stats - icmpv4 debug stats count
 * @tx_icmpv4_req_count: no. of icmpv4 req received from network stack
 * @rx_icmpv4_rsp_count: no. of icmpv4 res received from FW
 * @tx_dropped: no. of icmpv4 req dropped at hdd layer
 * @rx_delivered: no. of icmpv4 res delivered to network stack
 * @rx_refused: no of icmpv4 res refused (not delivered) to network stack
 * @tx_host_fw_sent: no of icmpv4 req sent by FW OTA
 * @rx_host_drop: no of icmpv4 res dropped by host
 * @rx_fw_cnt: no of icmpv4 res received by FW
 * @tx_ack_cnt: no of icmpv4 req acked by FW
 */
struct dp_icmpv4_stats {
	uint16_t tx_icmpv4_req_count;
	uint16_t rx_icmpv4_rsp_count;
	uint16_t tx_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_host_fw_sent;
	uint16_t rx_host_drop;
	uint16_t rx_fw_cnt;
	uint16_t tx_ack_cnt;
};

/**
 * struct dp_rsp_stats - arp packet stats
 * @vdev_id: session id
 * @arp_req_enqueue: fw tx count
 * @arp_req_tx_success: tx ack count
 * @arp_req_tx_failure: tx ack fail count
 * @arp_rsp_recvd: rx fw count
 * @out_of_order_arp_rsp_drop_cnt: out of order count
 * @dad_detected: dad detected
 * @connect_status: connection status
 * @ba_session_establishment_status: BA session status
 * @connect_stats_present: connectivity stats present or not
 * @tcp_ack_recvd: tcp syn ack's count
 * @icmpv4_rsp_recvd: icmpv4 responses count
 */
struct dp_rsp_stats {
	uint32_t vdev_id;
	uint32_t arp_req_enqueue;
	uint32_t arp_req_tx_success;
	uint32_t arp_req_tx_failure;
	uint32_t arp_rsp_recvd;
	uint32_t out_of_order_arp_rsp_drop_cnt;
	uint32_t dad_detected;
	uint32_t connect_status;
	uint32_t ba_session_establishment_status;
	bool connect_stats_present;
	uint32_t tcp_ack_recvd;
	uint32_t icmpv4_rsp_recvd;
};

/**
 * struct dp_txrx_soc_attach_params - SoC attach params
 * @dp_ol_if_ops: DP ol_if ops
 * @target_psoc: target psoc
 * @target_type: Target type
 */
struct dp_txrx_soc_attach_params {
	struct ol_if_ops *dp_ol_if_ops;
	void *target_psoc;
	uint32_t target_type;
};

struct dp_tx_rx_stats {
	struct {
		/* start_xmit stats */
		__u32    tx_called;
		__u32    tx_dropped;
		__u32    tx_orphaned;
		__u32    tx_classified_ac[WLAN_MAX_AC];
		__u32    tx_dropped_ac[WLAN_MAX_AC];
#ifdef TX_MULTIQ_PER_AC
		/* Neither valid socket nor skb->hash */
		uint32_t inv_sk_and_skb_hash;
		/* skb->hash already calculated */
		uint32_t qselect_existing_skb_hash;
		/* valid tx queue id in socket */
		uint32_t qselect_sk_tx_map;
		/* skb->hash calculated in select queue */
		uint32_t qselect_skb_hash_calc;
#endif
		/* rx stats */
		__u32 rx_packets;
		__u32 rx_dropped;
		__u32 rx_delivered;
		__u32 rx_refused;
	} per_cpu[NUM_CPUS];

	qdf_atomic_t rx_usolict_arp_n_mcast_drp;

	/* rx gro */
	__u32 rx_aggregated;
	__u32 rx_gro_dropped;
	__u32 rx_non_aggregated;
	__u32 rx_gro_flush_skip;
	__u32 rx_gro_low_tput_flush;

	/* txflow stats */
	bool     is_txflow_paused;
	__u32    txflow_pause_cnt;
	__u32    txflow_unpause_cnt;
	__u32    txflow_timer_cnt;

	/*tx timeout stats*/
	__u32 tx_timeout_cnt;
	__u32 cont_txtimeout_cnt;
	u64 last_txtimeout;
};

/**
 * struct dp_dhcp_ind - DHCP Start/Stop indication message
 * @dhcp_start: Is DHCP start idication
 * @device_mode: Mode of the device(ex:STA, AP)
 * @intf_mac_addr: MAC address of the interface
 * @peer_mac_addr: MAC address of the connected peer
 */
struct dp_dhcp_ind {
	bool dhcp_start;
	uint8_t device_mode;
	struct qdf_mac_addr intf_mac_addr;
	struct qdf_mac_addr peer_mac_addr;
};

/**
 * struct dp_mic_error_info - mic error info in dp
 * @ta_mac_addr: transmitter mac address
 * @multicast: Flag for multicast
 * @key_id: Key ID
 * @tsc: Sequence number
 * @vdev_id: vdev id
 *
 */
struct dp_mic_error_info {
	struct qdf_mac_addr ta_mac_addr;
	bool multicast;
	uint8_t key_id;
	uint8_t tsc[SIR_CIPHER_SEQ_CTR_SIZE];
	uint16_t vdev_id;
};

enum dp_mic_work_status {
	DP_MIC_UNINITIALIZED,
	DP_MIC_INITIALIZED,
	DP_MIC_SCHEDULED,
	DP_MIC_DISABLED
};

/**
 * struct dp_mic_work - mic work info in dp
 * @work: mic error work
 * @status: sattus of mic error work
 * @info: Pointer to mic error information
 * @lock: lock to synchronixe mic error work
 *
 */
struct dp_mic_work {
	qdf_work_t work;
	enum dp_mic_work_status status;
	struct dp_mic_error_info *info;
	qdf_spinlock_t lock;
};

enum dp_nud_state {
	DP_NUD_NONE,
	DP_NUD_INCOMPLETE,
	DP_NUD_REACHABLE,
	DP_NUD_STALE,
	DP_NUD_DELAY,
	DP_NUD_PROBE,
	DP_NUD_FAILED,
	DP_NUD_NOARP,
	DP_NUD_PERMANENT,
	DP_NUD_STATE_INVALID
};

struct opaque_hdd_callback_handle;
/*
 * typedef hdd_cb_handle - HDD Handle
 *
 * Handle to the HDD.  The HDD handle is given to the DP component from the
 * HDD during start modules.  The HDD handle is an input to all HDD function
 * calls and represents an opaque handle to the HDD instance that is
 * tied to the DP context
 *
 * The HDD must be able to derive it's internal instance structure
 * pointer through this handle.
 *
 * NOTE WELL: struct opaque_hdd_callback_handle is not defined anywhere. This
 * reference is used to help ensure that a hdd_cb_handle is never used
 * where a different handle type is expected
 */
typedef struct opaque_hdd_callback_handle *hdd_cb_handle;

/**
 * enum bus_bw_level - bus bandwidth vote levels
 *
 * @BUS_BW_LEVEL_NONE: No vote for bus bandwidth
 * @BUS_BW_LEVEL_1: vote for level-1 bus bandwidth
 * @BUS_BW_LEVEL_2: vote for level-2 bus bandwidth
 * @BUS_BW_LEVEL_3: vote for level-3 bus bandwidth
 * @BUS_BW_LEVEL_4: vote for level-4 bus bandwidth
 * @BUS_BW_LEVEL_5: vote for level-5 bus bandwidth
 * @BUS_BW_LEVEL_6: vote for level-6 bus bandwidth
 * @BUS_BW_LEVEL_7: vote for level-7 bus bandwidth
 * @BUS_BW_LEVEL_8: vote for level-8 bus bandwidth
 * @BUS_BW_LEVEL_9: vote for level-9 bus bandwidth
 * @BUS_BW_LEVEL_MAX: vote for max level bus bandwidth
 */
enum bus_bw_level {
	BUS_BW_LEVEL_NONE,
	BUS_BW_LEVEL_1,
	BUS_BW_LEVEL_2,
	BUS_BW_LEVEL_3,
	BUS_BW_LEVEL_4,
	BUS_BW_LEVEL_5,
	BUS_BW_LEVEL_6,
	BUS_BW_LEVEL_7,
	BUS_BW_LEVEL_8,
	BUS_BW_LEVEL_9,
	BUS_BW_LEVEL_MAX,
};

#define BUS_BW_LEVEL_RESUME BUS_BW_LEVEL_3

/**
 * enum tput_level - throughput levels
 *
 * @TPUT_LEVEL_NONE: No throughput
 * @TPUT_LEVEL_IDLE: idle throughtput level
 * @TPUT_LEVEL_LOW: low throughput level
 * @TPUT_LEVEL_MEDIUM: medium throughtput level
 * @TPUT_LEVEL_HIGH: high throughput level
 * @TPUT_LEVEL_MID_HIGH: mid high throughput level
 * @TPUT_LEVEL_VERY_HIGH: very high throughput level
 * @TPUT_LEVEL_ULTRA_HIGH: ultra high throughput level
 * @TPUT_LEVEL_SUPER_HIGH: super high throughput level
 * @TPUT_LEVEL_MAX: maximum throughput level
 */
enum tput_level {
	TPUT_LEVEL_NONE,
	TPUT_LEVEL_IDLE,
	TPUT_LEVEL_LOW,
	TPUT_LEVEL_MEDIUM,
	TPUT_LEVEL_HIGH,
	TPUT_LEVEL_MID_HIGH,
	TPUT_LEVEL_VERY_HIGH,
	TPUT_LEVEL_ULTRA_HIGH,
	TPUT_LEVEL_SUPER_HIGH,
	TPUT_LEVEL_MAX,
};

/**
 * enum bbm_non_per_flag - Non persistent policy related flag
 *
 * @BBM_APPS_RESUME: system resume flag
 * @BBM_APPS_SUSPEND: system suspend flag
 * @BBM_FLAG_MAX: maximum flag
 */
enum bbm_non_per_flag {
	BBM_APPS_RESUME,
	BBM_APPS_SUSPEND,
	BBM_FLAG_MAX,
};

/**
 * enum bbm_policy - BBM policy
 *
 * @BBM_DRIVER_MODE_POLICY: driver mode policy
 * @BBM_TPUT_POLICY: throughput policy
 * @BBM_USER_POLICY: user policy
 * @BBM_NON_PERSISTENT_POLICY: non persistent policy. For example, bus resume
 *  sets the bus bw level to LEVEL_3 if any adapter is connected but
 *  this is only a one time setting and is not persistent. This bus bw level
 *  is set without taking other policy vote levels into consideration.
 * @BBM_SELECT_TABLE_POLICY: policy where bus bw table is selected based on
 *  the latency level.
 * @BBM_MAX_POLICY: max policy
 */
enum bbm_policy {
	BBM_DRIVER_MODE_POLICY,
	BBM_TPUT_POLICY,
	BBM_USER_POLICY,
	BBM_NON_PERSISTENT_POLICY,
	BBM_SELECT_TABLE_POLICY,
	BBM_MAX_POLICY,
};

/**
 * enum wlm_ll_level - WLM latency levels
 *
 * @WLM_LL_NORMAL: normal latency level
 * @WLM_LL_LOW: low latency level
 * @WLM_LL_MAX: max latency level
 */
enum wlm_ll_level {
	WLM_LL_NORMAL,
	WLM_LL_LOW,
	WLM_LL_MAX,
};

/**
 * union bbm_policy_info - BBM policy specific info. Only one of the value
 *  would be valid based on the BBM policy.
 *
 * @driver_mode: global driver mode. valid for BBM_DRIVER_MODE_POLICY.
 * @flag: BBM non persistent flag. valid for BBM_NON_PERSISTENT_POLICY.
 * @tput_level: throughput level. valid for BBM_TPUT_POLICY.
 * @wlm_level: latency level. valid for BBM_WLM_POLICY.
 * @user_level: user bus bandwidth vote. valid for BBM_USER_POLICY.
 * @set: set or reset user level. valid for BBM_USER_POLICY.
 * @usr: user specific info
 */
union bbm_policy_info {
	enum QDF_GLOBAL_MODE driver_mode;
	enum bbm_non_per_flag flag;
	enum tput_level tput_level;
	enum wlm_ll_level wlm_level;
	struct {
		enum bus_bw_level user_level;
		bool set;
	} usr;
};

/**
 * struct bbm_params: BBM params
 *
 * @policy: BBM policy
 * @policy_info: policy related info
 */
struct bbm_params {
	enum bbm_policy policy;
	union bbm_policy_info policy_info;
};

/**
 * union wlan_tp_data: union of TCP msg for Tx and Rx Dir
 * @tx_tp_data: msg to TCP for Tx Dir
 * @rx_tp_data: msg to TCP for Rx Dir
 */
union wlan_tp_data {
	struct wlan_tx_tp_data tx_tp_data;
	struct wlan_rx_tp_data rx_tp_data;
};

/**
 * struct wlan_dp_psoc_callbacks - struct containing callback
 * to non-converged driver
 * @callback_ctx : Opaque callback context
 * @dp_get_netdev_by_vdev_mac: Callback to get netdev from vdev mac address
 * @dp_get_tx_flow_low_watermark: Callback to get TX flow low watermark info
 * @dp_get_tx_resource: Callback to check tx resources and take action
 * @dp_get_tsf_time: Callback to get TSF time
 * @dp_tsf_timestamp_rx: Callback to set rx packet timestamp
 * @dp_nbuf_push_pkt: Callback to push rx pkt to network
 * @dp_rx_napi_gro_flush: OS IF Callback to GRO RX/flush function.
 * @dp_rx_thread_napi_gro_flush: OS IF Callback to do gro flush
 * @dp_rx_napi_gro_receive: OS IF Callback for GRO RX receive function.
 * @dp_lro_rx_cb: OS IF Callback for LRO receive function
 * @dp_gro_rx_legacy_get_napi: Callback to get napi in legacy gro case
 * @dp_register_rx_offld_flush_cb: OS IF Callback to get rx offld flush cb
 * @dp_rx_check_qdisc_configured: OS IF Callback to check if any ingress qdisc
 * configured
 * @dp_is_gratuitous_arp_unsolicited_na: OS IF Callback to check gratuitous arp
 * unsolicited na
 * @dp_send_rx_pkt_over_nl: OS IF Callback to send rx pkt over nl
 * @dp_disable_rx_ol_for_low_tput: Callback to disable Rx offload in low TPUT
 * scenario
 * @wlan_dp_sta_get_dot11mode: Callback to get dot11 mode
 * @wlan_dp_get_ap_client_count: Callback to get client count connected to AP
 * @wlan_dp_sta_ndi_connected: Callback to get NDI connected status
 * @dp_any_adapter_connected: Callback to check if any adapter is connected
 * @dp_send_svc_nlink_msg: Callback API to send svc nlink message
 * @osif_dp_send_tcp_param_update_event: OS IF callback to send TCP param
 * @dp_send_mscs_action_frame: Callback to send MSCS action frame
 * @dp_pm_qos_add_request: Callback to send add pm qos request
 * @dp_pm_qos_remove_request: Callback to send remove pm qos request
 * @dp_pm_qos_update_request: Callback to send update pm qos request
 * @dp_pld_remove_pm_qos: Callback to send remove pld pm qos request
 * @dp_pld_request_pm_qos: Callback to send pld pm qos request
 * @dp_pktlog_enable_disable:Callback to set packet log
 * @dp_is_roaming_in_progress:Callback to check if roaming is in progress
 * @dp_is_ap_active:Callback to check if AP is active
 * @dp_napi_apply_throughput_policy:Callback to apply NAPI throughput policy
 * @wlan_dp_display_tx_multiq_stats: Callback to display Tx Mulit queue stats
 * @wlan_dp_display_netif_queue_history: Callback to display Netif queue
 * history
 * @osif_dp_process_mic_error: osif callback to process MIC error
 * @dp_is_link_adapter: Callback API to check if adapter is link adapter
 * @os_if_dp_nud_stats_info: osif callback to print nud stats info
 * @dp_get_pause_map: Callback API to get pause map count
 * @dp_nud_failure_work: Callback API to handle NUD failuire work
 * @link_monitoring_cb: Callback API to handle link speed change
 */
struct wlan_dp_psoc_callbacks {
	hdd_cb_handle callback_ctx;

	qdf_netdev_t (*dp_get_netdev_by_vdev_mac)(struct qdf_mac_addr *mac_addr);
	unsigned int (*dp_get_tx_flow_low_watermark)(hdd_cb_handle cb_ctx,
						     qdf_netdev_t netdev);
	void (*dp_get_tx_resource)(uint8_t link_id_id,
				   struct qdf_mac_addr *mac_addr);
	void (*dp_get_tsf_time)(qdf_netdev_t netdev, uint64_t input_time,
				uint64_t *tsf_time);
	void (*dp_tsf_timestamp_rx)(hdd_cb_handle ctx, qdf_nbuf_t nbuf);

	QDF_STATUS (*dp_nbuf_push_pkt)(qdf_nbuf_t nbuf,
				       enum dp_nbuf_push_type type);

	QDF_STATUS (*dp_rx_napi_gro_flush)(qdf_napi_struct *napi_to_use,
					   qdf_nbuf_t nbuf,
					   uint8_t *force_flush);
	void
	(*dp_rx_thread_napi_gro_flush)(qdf_napi_struct *napi,
				       enum dp_rx_gro_flush_code flush_code);
	QDF_STATUS (*dp_rx_napi_gro_receive)(qdf_napi_struct *napi_to_use,
					     qdf_nbuf_t nbuf);

	QDF_STATUS (*dp_lro_rx_cb)(qdf_netdev_t netdev, qdf_nbuf_t nbuf);

	qdf_napi_struct *(*dp_gro_rx_legacy_get_napi)(qdf_nbuf_t nbuf,
						      bool enable_rx_thread);

	void (*dp_register_rx_offld_flush_cb)(enum dp_rx_offld_flush_cb type);

	QDF_STATUS (*dp_rx_check_qdisc_configured)(qdf_netdev_t dev,
						   uint32_t prio);

	bool (*dp_is_gratuitous_arp_unsolicited_na)(qdf_nbuf_t nbuf);

	bool (*dp_send_rx_pkt_over_nl)(qdf_netdev_t dev, uint8_t *addr,
				       qdf_nbuf_t nbuf, bool unecrypted);
	bool
	(*wlan_dp_sta_get_dot11mode)(hdd_cb_handle context, qdf_netdev_t netdev,
				     enum qca_wlan_802_11_mode *dot11_mode);
	bool (*wlan_dp_get_ap_client_count)(hdd_cb_handle context,
					    qdf_netdev_t netdev,
					    uint16_t *client_count);
	bool (*wlan_dp_sta_ndi_connected)(hdd_cb_handle context,
					  qdf_netdev_t netdev);
	bool (*dp_any_adapter_connected)(hdd_cb_handle context);
	void (*dp_send_svc_nlink_msg)(int radio, int type, void *data, int len);

	void
	(*osif_dp_send_tcp_param_update_event)(struct wlan_objmgr_psoc *psoc,
					       union wlan_tp_data *data,
					       uint8_t dir);
	void (*dp_send_mscs_action_frame)(hdd_cb_handle context,
					  qdf_netdev_t netdev);
	void (*dp_pm_qos_add_request)(hdd_cb_handle context);
	void (*dp_pm_qos_remove_request)(hdd_cb_handle context);
	void (*dp_pm_qos_update_request)(hdd_cb_handle context,
					 cpumask_t *mask);
	void (*dp_pld_remove_pm_qos)(hdd_cb_handle context);
	void (*dp_pld_request_pm_qos)(hdd_cb_handle context);
	int (*dp_pktlog_enable_disable)(hdd_cb_handle context,
					bool enable_disable_flag,
					uint8_t user_triggered, int size);
	bool (*dp_is_roaming_in_progress)(hdd_cb_handle context);
	bool (*dp_is_ap_active)(hdd_cb_handle context, qdf_netdev_t netdev);
	void (*dp_disable_rx_ol_for_low_tput)(hdd_cb_handle context,
					      bool disable);
	int (*dp_napi_apply_throughput_policy)(hdd_cb_handle context,
					       uint64_t tx_packets,
					       uint64_t rx_packets);
	void (*wlan_dp_display_tx_multiq_stats)(hdd_cb_handle context,
						qdf_netdev_t netdev);
	void (*wlan_dp_display_netif_queue_history)(hdd_cb_handle context,
				enum qdf_stats_verbosity_level verb_lvl);
	void (*osif_dp_process_mic_error)(struct dp_mic_error_info *info,
					  struct wlan_objmgr_vdev *vdev);
	bool (*dp_is_link_adapter)(hdd_cb_handle context, uint8_t vdev_id);
	void (*os_if_dp_nud_stats_info)(struct wlan_objmgr_vdev *vdev);
	uint32_t (*dp_get_pause_map)(hdd_cb_handle context, qdf_netdev_t dev);
	void (*dp_nud_failure_work)(hdd_cb_handle context, qdf_netdev_t dev);
	void (*link_monitoring_cb)(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id,
				   bool is_link_speed_good);
};

/**
 * struct wlan_dp_psoc_sb_ops - struct containing callback
 * to south bound APIs. callbacks to call traget_if APIs
 * @dp_arp_stats_register_event_handler: Callback to register
 * arp stas WMI handle
 * @dp_arp_stats_unregister_event_handler: Callback to unregister
 * arp stas WMI handle
 * @dp_get_arp_req_stats: Callback to get arp stats
 * @dp_set_arp_req_stats: Callback to  set arp stats
 * @arp_request_ctx: ARP request context
 * @dp_lro_config_cmd: Callback to  send LRO config command
 * @dp_send_dhcp_ind: Callback to send DHCP indication
 */
struct wlan_dp_psoc_sb_ops {
	/*TODO to add target if TX ops*/
	QDF_STATUS (*dp_arp_stats_register_event_handler)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*dp_arp_stats_unregister_event_handler)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*dp_get_arp_req_stats)(struct wlan_objmgr_psoc *psoc,
					   struct dp_get_arp_stats_params *req_buf);
	QDF_STATUS (*dp_set_arp_req_stats)(struct wlan_objmgr_psoc *psoc,
					   struct dp_set_arp_stats_params *req_buf);
	void *arp_request_ctx;
	QDF_STATUS (*dp_lro_config_cmd)(struct wlan_objmgr_psoc *psoc,
					struct cdp_lro_hash_config *dp_lro_cmd);
	QDF_STATUS (*dp_send_dhcp_ind)(uint16_t vdev_id,
				       struct dp_dhcp_ind *dhcp_ind);
};

/**
 * struct wlan_dp_psoc_nb_ops - struct containing callback
 * to north bound APIs. callbacks APIs to be called by target_if APIs
 * @osif_dp_get_arp_stats_evt: Callback called on receiving arp stats event
 */
struct wlan_dp_psoc_nb_ops {
	/*TODO to add target if RX ops*/
	void (*osif_dp_get_arp_stats_evt)(struct wlan_objmgr_psoc *psoc,
					  struct dp_rsp_stats *rsp);
};

/**
 * struct wlan_dp_user_config - DP component user config
 * @ipa_enable: IPA enabled/disabled config
 * @arp_connectivity_map: ARP connectiviy map
 */
struct wlan_dp_user_config {
	bool ipa_enable;
	uint32_t arp_connectivity_map;
};

/**
 * struct dp_traffic_end_indication - Traffic end indication
 * @enabled: Feature enabled/disabled config
 * @def_dscp: Default DSCP value in regular packets in traffic
 * @spl_dscp: Special DSCP value to be used by packet to mark
 *            end of data stream
 */
struct dp_traffic_end_indication {
	bool enabled;
	uint8_t def_dscp;
	uint8_t spl_dscp;
};

#define DP_SVC_INVALID_ID 0xFF
#define DP_MAX_SVC 32
#define DP_SVC_ARRAY_SIZE DP_MAX_SVC

#define DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE   BIT(0)
#define DP_SVC_FLAGS_APP_IND_DEF_DSCP           BIT(1)
#define DP_SVC_FLAGS_APP_IND_SPL_DSCP           BIT(2)
#define DP_SVC_FLAGS_SVC_ID			BIT(3)

/* struct dp_svc_data - service class node
 * @node: list node
 * @svc_id: service class id
 * @policy_ref_count: number of policies associated
 * @buffer_latency_tolerance: buffer latency tolarence in ms
 * @app_ind_default_dscp: default dscp
 * @app_ind_special_dscp: special dscp to override with default dscp
 */
struct dp_svc_data {
	qdf_list_node_t node;
	uint8_t svc_id;
	uint8_t policy_ref_count;
	uint32_t buffer_latency_tolerance;
	uint8_t app_ind_default_dscp;
	uint8_t app_ind_special_dscp;
	uint32_t flags;
};

#define DP_FLOW_PRIO_MAX 8
#define DP_MAX_POLICY 32
#define DP_INVALID_ID 0xFF
#define DP_FLOW_HASH_MASK 0xFF
#define DP_FLOW_PRIO_DEF 3
#define MAX_TID 8

/*
 * Flow tuple related flags
 */
#define DP_FLOW_TUPLE_FLAGS_IPV4	BIT(0)
#define DP_FLOW_TUPLE_FLAGS_IPV6	BIT(1)
#define DP_FLOW_TUPLE_FLAGS_SRC_IP	BIT(2)
#define DP_FLOW_TUPLE_FLAGS_DST_IP	BIT(3)
#define DP_FLOW_TUPLE_FLAGS_SRC_PORT	BIT(4)
#define DP_FLOW_TUPLE_FLAGS_DST_PORT	BIT(5)
#define DP_FLOW_TUPLE_FLAGS_PROTO	BIT(6)

/*
 * Flow policy related flags
 */
#define DP_POLICY_TO_TID_MAP	BIT(0)
#define DP_POLICY_TO_SVC_MAP	BIT(1)
#define DP_POLICY_UPDATE_PRIO	BIT(2)

/*
 * struct flow_info - Structure used for defining flow
 * @proto: Flow proto
 * @src_port: Source port
 * @dst_port: Destination port
 * @flags: Flags indicating available attributes of a flow
 * @src_ip: Source IP (IPv4/IPv6)
 * @dst_ip: Destination IP (IPv4/IPv6)
 * @flow_label: Flow label if IPv6 is used for src_ip/dst_ip
 */
struct flow_info {
	uint8_t proto;
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t flags;
	union {
		uint32_t ipv4_addr;             /* IPV4 address */
		uint32_t ipv6_addr[4];          /* IPV6 address */
	} src_ip;
	union {
		uint32_t ipv4_addr;             /* IPV4 address */
		uint32_t ipv6_addr[4];          /* IPV6 address */
	} dst_ip;
	uint32_t flow_label;
};

/* struct dp_policy - Structure used for defining flow policy.
 * @node: dp_policy node used in constructing hlist.
 * @rcu: Protect dp_policy with rcu
 * @prio: Priority of defined flow
 * @policy_id: Unique policy ID
 * @flow: Flow tuble
 * @flags: Flags indication policy mapping
 * @target_tid: Target TID for TID override
 * @svc_id: Service class ID
 * @is_used: Is in use
 */
struct dp_policy {
	struct qdf_ht_entry node;
	qdf_rcu_head_t rcu;
	uint8_t prio;
	uint64_t policy_id;
	struct flow_info flow;
	uint32_t flags;
	uint8_t target_tid;
	uint8_t svc_id;
	bool is_used;
};

/* struct fpm_table - Flow Policy Table
 * @lock: Spin lock to protect Flow policy Table
 * @policy_tab: Policy Table
 * @policy_id_bitmap: Bitmap used to derive unique policy ID
 * @policy_count: Policy counter
 * @fpm_policy_event_notif_head: List of registered notifiers
 */
struct fpm_table {
	qdf_spinlock_t lock;
	struct qdf_ht policy_tab[DP_FLOW_PRIO_MAX];
	uint32_t policy_id_bitmap;
	uint8_t policy_count;
	qdf_atomic_notif_head fpm_policy_event_notif_head;
};

#endif /* end  of _WLAN_DP_PUBLIC_STRUCT_H_ */

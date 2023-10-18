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

/**
 * DOC: wlan_dp_ucfg_api.h
 *
 * TDLS north bound interface declaration
 */

#if !defined(_WLAN_DP_UCFG_API_H_)
#define _WLAN_DP_UCFG_API_H_

#include <scheduler_api.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include "pld_common.h"
#include <wlan_dp_public_struct.h>
#include <cdp_txrx_misc.h>
#include "wlan_dp_objmgr.h"
#include "wlan_qmi_public_struct.h"

#define DP_IGNORE_NUD_FAIL                      0
#define DP_DISCONNECT_AFTER_NUD_FAIL            1
#define DP_ROAM_AFTER_NUD_FAIL                  2
#define DP_DISCONNECT_AFTER_ROAM_FAIL           3

#ifdef WLAN_NUD_TRACKING
bool
ucfg_dp_is_roam_after_nud_enabled(struct wlan_objmgr_psoc *psoc);

bool
ucfg_dp_is_disconect_after_roam_fail(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
ucfg_dp_is_roam_after_nud_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool
ucfg_dp_is_disconect_after_roam_fail(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * ucfg_dp_update_inf_mac() - update DP interface MAC address
 * @psoc: psoc handle
 * @cur_mac: Current MAC address
 * @new_mac: new MAC address
 *
 */
void ucfg_dp_update_inf_mac(struct wlan_objmgr_psoc *psoc,
			    struct qdf_mac_addr *cur_mac,
			    struct qdf_mac_addr *new_mac);

/**
 * ucfg_dp_destroy_intf() - DP module interface deletion
 * @psoc: psoc handle
 * @intf_addr: Interface MAC address
 *
 */
QDF_STATUS ucfg_dp_destroy_intf(struct wlan_objmgr_psoc *psoc,
				struct qdf_mac_addr *intf_addr);

/**
 * ucfg_dp_create_intf() - DP module interface creation
 * @psoc: psoc handle
 * @intf_addr: Interface MAC address
 * @ndev : netdev object
 *
 */
QDF_STATUS ucfg_dp_create_intf(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *intf_addr,
			       qdf_netdev_t ndev);

/**
 * ucfg_dp_init() - DP module initialization API
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_dp_init(void);

/**
 * ucfg_dp_deinit() - DP module deinitialization API
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_dp_deinit(void);

/**
 * ucfg_dp_psoc_open() - DP component Open
 * @psoc: pointer to psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_dp_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_psoc_close() - DP component Close
 * @psoc: pointer to psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_dp_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_suspend_wlan() - update suspend state in DP component
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_dp_suspend_wlan(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_resume_wlan() - update resume state in DP component
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_dp_resume_wlan(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_update_config() - DP module config update
 * @psoc: pointer to psoc object
 * @req : user config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_dp_update_config(struct wlan_objmgr_psoc *psoc,
		      struct wlan_dp_user_config *req);
/**
 * ucfg_dp_wait_complete_tasks() - wait for DP tasks to complete
 * Called from legacy layer to wait DP tasks completion
 *
 * Return: None
 */
void
ucfg_dp_wait_complete_tasks(void);

/**
 * ucfg_dp_remove_conn_info() - Remove DP STA intf connection info
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: QDF_STATUS
 */
void
ucfg_dp_remove_conn_info(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_conn_info_set_bssid() - set BSSID info in STA intf
 * @vdev: vdev mapped to STA DP interface
 * @bssid: BSSID mac
 *
 * Return: None
 */
void ucfg_dp_conn_info_set_bssid(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *bssid);

/**
 * ucfg_dp_conn_info_set_arp_service() - set ARP service info
 * @vdev: vdev mapped to STA DP interface
 * @proxy_arp_service: ARP service info
 *
 * Return: None
 */
void ucfg_dp_conn_info_set_arp_service(struct wlan_objmgr_vdev *vdev,
				       uint8_t proxy_arp_service);

/**
 * ucfg_dp_conn_info_set_peer_authenticate() - set Peer authenticated state
 * @vdev: vdev mapped to STA DP interface
 * @is_authenticated: Peer authenticated info
 *
 * Return: None
 */
void ucfg_dp_conn_info_set_peer_authenticate(struct wlan_objmgr_vdev *vdev,
					     uint8_t is_authenticated);

/**
 * ucfg_dp_conn_info_set_peer_mac() - set peer mac info in DP intf
 * @vdev: vdev mapped to STA DP interface
 * @peer_mac: Peer MAC information
 *
 * Return: None
 */
void ucfg_dp_conn_info_set_peer_mac(struct wlan_objmgr_vdev *vdev,
				    struct qdf_mac_addr *peer_mac);

/**
 * ucfg_dp_softap_check_wait_for_tx_eap_pkt() - wait for TX EAP pkt in SAP
 * @vdev: vdev mapped to SAP DP interface
 * @mac_addr: Peer MAC address info
 *
 * Return: None
 */
void ucfg_dp_softap_check_wait_for_tx_eap_pkt(struct wlan_objmgr_vdev *vdev,
					      struct qdf_mac_addr *mac_addr);

/**
 * ucfg_dp_update_dhcp_state_on_disassoc() - update DHCP during disassoc
 * @vdev: vdev mapped to SAP DP interface
 * @mac_addr: Peer MAC address info
 *
 * Return: None
 */
void ucfg_dp_update_dhcp_state_on_disassoc(struct wlan_objmgr_vdev *vdev,
					   struct qdf_mac_addr *mac_addr);

/**
 * ucfg_dp_set_dfs_cac_tx() - update DFS CAC TX block info
 * @vdev: vdev mapped to SAP DP interface
 * @tx_block: true if TX need to be blocked
 *
 * Return: None
 */
void ucfg_dp_set_dfs_cac_tx(struct wlan_objmgr_vdev *vdev,
			    bool tx_block);

/**
 * ucfg_dp_set_bss_state_start() - update BSS state for SAP intf
 * @vdev: vdev mapped to SAP DP interface
 * @start: true if BSS state is started
 *
 * Return: None
 */
void ucfg_dp_set_bss_state_start(struct wlan_objmgr_vdev *vdev, bool start);

/**
 * ucfg_dp_lro_set_reset() - LRO set/reset in DP
 * @vdev: vdev mapped to DP interface
 * @enable_flag: Enable/disable LRO feature
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_lro_set_reset(struct wlan_objmgr_vdev *vdev,
				 uint8_t enable_flag);
/**
 * ucfg_dp_is_ol_enabled() - Get ol enable/disable info
 * @psoc: PSOC mapped to DP context
 *
 * Return: true if OL enabled
 */
bool ucfg_dp_is_ol_enabled(struct wlan_objmgr_psoc *psoc);

#ifdef RECEIVE_OFFLOAD
/**
 * ucfg_dp_rx_handle_concurrency() - Handle concurrency setting in DP
 * @psoc: PSOC mapped to DP context
 * @disable: true/false to disable/enable the Rx offload
 *
 * Return: None
 */
void ucfg_dp_rx_handle_concurrency(struct wlan_objmgr_psoc *psoc,
				   bool disable);
#else
static inline
void ucfg_dp_rx_handle_concurrency(struct wlan_objmgr_psoc *psoc,
				   bool disable) { }
#endif

/**
 * ucfg_dp_is_rx_common_thread_enabled() - Get common thread enable/disable info
 * @psoc: PSOC mapped to DP context
 *
 * Return: true if common thread enabled
 */
bool ucfg_dp_is_rx_common_thread_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_is_rx_threads_enabled() - Get RX DP threads info
 * @psoc: PSOC mapped to DP context
 *
 * Return: true if DP RX threads enabled
 */
bool ucfg_dp_is_rx_threads_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_rx_ol_init() - Initialize Rx offload mode (LRO or GRO)
 * @psoc: PSOC mapped to DP context
 * @is_wifi3_0_target: true if it wifi3.0 target
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_rx_ol_init(struct wlan_objmgr_psoc *psoc,
			      bool is_wifi3_0_target);

/**
 * ucfg_dp_init_txrx() - Initialize STA DP init TX/RX
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_init_txrx(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_deinit_txrx() - Deinitialize STA DP init TX/RX
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_deinit_txrx(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_softap_init_txrx() - Initialize SAP DP init TX/RX
 * @vdev: vdev mapped to SAP DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_softap_init_txrx(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_softap_deinit_txrx() - Deinitialize SAP DP init TX/RX
 * @vdev: vdev mapped to SAP DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_softap_deinit_txrx(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_start_xmit() - Transmit packet on STA interface
 * @nbuf: n/w buffer to transmitted
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS
ucfg_dp_start_xmit(qdf_nbuf_t nbuf, struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_rx_packet_cbk() - Receive packet on STA interface
 * @nbuf: n/w buffer to be received
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_rx_packet_cbk(struct wlan_objmgr_vdev *vdev,
				 qdf_nbuf_t nbuf);

/**
 * ucfg_dp_tx_timeout() - called during transmission timeout on STA
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: None
 */
void ucfg_dp_tx_timeout(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_softap_tx_timeout() - called during transmission timeout on SAP
 * @vdev: vdev mapped to SAP DP interface
 *
 * Return: None
 */
void ucfg_dp_softap_tx_timeout(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_softap_start_xmit() - Transmit packet on SAP interface
 * @nbuf: n/w buffer to transmitted
 * @vdev: vdev mapped to SAP DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS
ucfg_dp_softap_start_xmit(qdf_nbuf_t nbuf, struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_dev_stats() - Get netdev stats info
 * @dev: Pointer to network device
 *
 * Return: qdf_net_dev_stats info
 */
qdf_net_dev_stats *ucfg_dp_get_dev_stats(qdf_netdev_t dev);

/**
 * ucfg_dp_inc_rx_pkt_stats() - DP increment RX pkt stats
 * @vdev: VDEV mapped to DP interface
 * @pkt_len: packet length to be incremented in stats
 * @delivered: pkts delivered or not
 *
 * Return: None
 */
void ucfg_dp_inc_rx_pkt_stats(struct wlan_objmgr_vdev *vdev,
			      uint32_t pkt_len,
			      bool delivered);

/**
 * ucfg_dp_get_rx_softirq_yield_duration() - Get rx soft IRQ yield duration
 * @psoc: pointer to psoc object
 *
 * Return: soft IRQ yield duration
 */
uint64_t
ucfg_dp_get_rx_softirq_yield_duration(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_register_rx_mic_error_ind_handler : register mic error handler.
 * @soc: soc handle
 */
void ucfg_dp_register_rx_mic_error_ind_handler(void *soc);

/**
 * ucfg_dp_sta_register_txrx_ops() - Register ops for TX/RX operations in STA
 * @vdev: vdev mapped to STA DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_sta_register_txrx_ops(struct wlan_objmgr_vdev *vdev);

#ifdef FEATURE_WLAN_TDLS
/**
 * ucfg_dp_tdlsta_register_txrx_ops() - Register ops for TX/RX operations
 * @vdev: vdev mapped to TDLS STA DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_tdlsta_register_txrx_ops(struct wlan_objmgr_vdev *vdev);
#else
static inline
QDF_STATUS ucfg_dp_tdlsta_register_txrx_ops(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * ucfg_dp_ocb_register_txrx_ops() - Register ops for TX/RX operations
 * @vdev: vdev mapped to OCB DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_ocb_register_txrx_ops(struct wlan_objmgr_vdev *vdev);

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * ucfg_dp_mon_register_txrx_ops() - Register ops for TX/RX operations
 * @vdev: vdev mapped to Monitor mode DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_mon_register_txrx_ops(struct wlan_objmgr_vdev *vdev);
#else
static inline
QDF_STATUS ucfg_dp_mon_register_txrx_ops(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * ucfg_dp_softap_register_txrx_ops() - Register ops for TX/RX operations
 * @vdev: vdev mapped to SAP mode DP interface
 * @txrx_ops: Tx and Rx data transfer ops
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS ucfg_dp_softap_register_txrx_ops(struct wlan_objmgr_vdev *vdev,
					    struct ol_txrx_ops *txrx_ops);

/**
 * ucfg_dp_register_pkt_capture_callbacks() - Register ops for pkt capture operations
 * @vdev: vdev mapped to DP interface
 *
 * Return: 0 on success and non zero on failure.
 */
QDF_STATUS
ucfg_dp_register_pkt_capture_callbacks(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_bbm_context_init() - Initialize BBM context
 * @psoc: psoc handle
 *
 * Returns: error code
 */
int ucfg_dp_bbm_context_init(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_bbm_context_deinit() - De-initialize BBM context
 * @psoc: psoc handle
 *
 * Returns: None
 */
void ucfg_dp_bbm_context_deinit(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_bbm_apply_independent_policy() - Apply independent policies
 *  to set the bus bw level
 * @psoc: psoc handle
 * @params: BBM policy related params
 *
 * The function applies BBM related policies and appropriately sets the bus
 * bandwidth level.
 *
 * Returns: None
 */
void ucfg_dp_bbm_apply_independent_policy(struct wlan_objmgr_psoc *psoc,
					  struct bbm_params *params);

/**
 * ucfg_dp_periodic_sta_stats_start() - Start displaying periodic stats for STA
 * @vdev: Pointer to the vdev
 *
 * Return: none
 */
void ucfg_dp_periodic_sta_stats_start(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_periodic_sta_stats_stop() - Stop displaying periodic stats for STA
 * @vdev: Pointer to the vdev
 *
 * Return: none
 */
void ucfg_dp_periodic_sta_stats_stop(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_set_rx_mode_rps() - Enable/disable RPS in SAP mode
 * @enable: Set true to enable RPS in SAP mode
 *
 * Callback function registered with datapath
 *
 * Return: none
 */
void ucfg_dp_set_rx_mode_rps(bool enable);

/**
 * ucfg_dp_try_send_rps_ind() - send rps indication to daemon
 * @vdev: vdev handle
 *
 * If RPS feature enabled by INI, send RPS enable indication to daemon
 * Indication contents is the name of interface to find correct sysfs node
 * Should send all available interfaces
 *
 * Return: none
 */
void ucfg_dp_try_send_rps_ind(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_reg_ipa_rsp_ind() - Resiter RSP IND cb with IPA component
 * @pdev: pdev handle
 *
 * Returns: None
 */
void ucfg_dp_reg_ipa_rsp_ind(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_dp_try_set_rps_cpu_mask - set RPS CPU mask for interfaces
 * @psoc: psoc handle
 *
 * Return: none
 */
void ucfg_dp_try_set_rps_cpu_mask(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_add_latency_critical_client() - Add latency critical client
 * @vdev: vdev handle (Should not be NULL)
 * @phymode: the phymode of the connected adapter
 *
 * This function checks if the present connection is latency critical
 * and adds to the latency critical clients count and informs the
 * datapath about this connection being latency critical.
 *
 * Returns: None
 */
void ucfg_dp_add_latency_critical_client(struct wlan_objmgr_vdev *vdev,
					 enum qca_wlan_802_11_mode phymode);

/**
 * ucfg_dp_del_latency_critical_client() - Remove latency critical client
 * @vdev: vdev handle (Should not be NULL)
 * @phymode: the phymode of the connected adapter
 *
 * This function checks if the present connection was latency critical
 * and removes from the latency critical clients count and informs the
 * datapath about the removed connection being latency critical.
 *
 * Returns: None
 */
void ucfg_dp_del_latency_critical_client(struct wlan_objmgr_vdev *vdev,
					 enum qca_wlan_802_11_mode phymode);

/**
 * ucfg_dp_reset_tcp_delack() - Reset TCP delay ACK
 * level
 * @psoc: psoc handle
 *
 * Return: None
 */
void ucfg_dp_reset_tcp_delack(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_set_current_throughput_level() - update the current vote
 * level
 * @psoc: psoc handle
 * @next_vote_level: pld_bus_width_type voting level
 *
 * This function updates the current vote level to the new level
 * provided
 *
 * Return: None
 */
void
ucfg_dp_set_current_throughput_level(struct wlan_objmgr_psoc *psoc,
				     enum pld_bus_width_type next_vote_level);

/**
 * ucfg_wlan_dp_display_tx_rx_histogram() - display tx rx histogram
 * @psoc: psoc handle
 *
 * Return: none
 */
void ucfg_wlan_dp_display_tx_rx_histogram(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_wlan_dp_clear_tx_rx_histogram() - clear tx rx histogram
 * @psoc: psoc handle
 *
 * Return: none
 */
void ucfg_wlan_dp_clear_tx_rx_histogram(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_set_high_bus_bw_request() - Set high bandwidth request.
 * @psoc: psoc handle
 * @vdev_id: vdev_id
 * @high_bus_bw : High bus bandwidth requested
 *
 * Return: None.
 */
void
ucfg_dp_set_high_bus_bw_request(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				bool high_bus_bw);

/**
 * ucfg_dp_bus_bw_compute_timer_start() - start the bandwidth timer
 * @psoc: psoc handle
 *
 * Return: None
 */
void ucfg_dp_bus_bw_compute_timer_start(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_bus_bw_compute_timer_try_start() - try to start the bandwidth timer
 * @psoc: psoc handle
 *
 * This function ensures there is at least one intf in the associated state
 * before starting the bandwidth timer.
 *
 * Return: None
 */
void ucfg_dp_bus_bw_compute_timer_try_start(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_bus_bw_compute_timer_stop() - stop the bandwidth timer
 * @psoc: psoc handle
 *
 * Return: None
 */
void ucfg_dp_bus_bw_compute_timer_stop(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_bus_bw_compute_timer_try_stop() - try to stop the bandwidth timer
 * @psoc: psoc handle
 *
 * This function ensures there are no interface in the associated state before
 * stopping the bandwidth timer.
 *
 * Return: None
 */
void ucfg_dp_bus_bw_compute_timer_try_stop(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_bus_bw_compute_prev_txrx_stats() - get tx and rx stats
 * @vdev: vdev handle
 *
 * This function get the collected tx and rx stats before starting
 * the bus bandwidth timer.
 *
 * Return: None
 */
void ucfg_dp_bus_bw_compute_prev_txrx_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_bus_bw_compute_reset_prev_txrx_stats() - reset previous txrx stats
 * @vdev: vdev handle
 *
 * This function resets the adapter previous tx rx stats.
 *
 * Return: None
 */
void
ucfg_dp_bus_bw_compute_reset_prev_txrx_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_nud_set_gateway_addr() - set gateway mac address
 * @vdev: vdev handle
 * @gw_mac_addr: mac address to be set
 *
 * Return: none
 */
void ucfg_dp_nud_set_gateway_addr(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr gw_mac_addr);

/**
 * ucfg_dp_nud_event() - netevent callback
 * @netdev_mac_addr: netdev MAC addr
 * @gw_mac_addr: Gateway MAC address
 * @nud_state : NUD State
 *
 * Return: None
 */
void ucfg_dp_nud_event(struct qdf_mac_addr *netdev_mac_addr,
		       struct qdf_mac_addr *gw_mac_addr,
		       uint8_t nud_state);

/**
 * ucfg_dp_get_arp_stats_event_handler - ARP get stats event handler
 *
 * @psoc: PSOC Handle
 * @rsp : response message
 *
 * Return : 0 on success else error code.
 */

QDF_STATUS ucfg_dp_get_arp_stats_event_handler(struct wlan_objmgr_psoc *psoc,
					       struct dp_rsp_stats *rsp);

/**
 * ucfg_dp_get_arp_request_ctx - Get ARP request context
 *
 * @psoc: PSOC Handle
 *
 * Return : ARP request context
 */
void *ucfg_dp_get_arp_request_ctx(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_nud_reset_tracking() - reset NUD tracking
 * @vdev: vdev handle
 *
 * Return: None
 */
void ucfg_dp_nud_reset_tracking(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_nud_tracking_enabled - Check if NUD tracking is enabled
 *
 * @psoc: PSOC Handle
 *
 * Return : NUD tracking value.
 */
uint8_t ucfg_dp_nud_tracking_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_nud_indicate_roam() - reset NUD when roaming happens
 * @vdev: vdev handle
 *
 * Return: None
 */
void ucfg_dp_nud_indicate_roam(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_clear_arp_stats() - Clear ARP Stats
 * @vdev: vdev context
 *
 * Return: None
 */
void ucfg_dp_clear_arp_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_clear_dns_stats() - Clear DNS Stats
 * @vdev: vdev context
 *
 * Return: None
 */
void ucfg_dp_clear_dns_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_clear_tcp_stats() - Clear TCP Stats
 * @vdev: vdev context
 *
 * Return: None
 */
void ucfg_dp_clear_tcp_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_clear_icmpv4_stats() - Clear ICMPv4 Stats
 * @vdev: vdev context
 *
 * Return: None
 */
void ucfg_dp_clear_icmpv4_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_clear_dns_payload_value() - Clear DNS payload value
 * @vdev: vdev context
 *
 * Return: None
 */
void ucfg_dp_clear_dns_payload_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_set_pkt_type_bitmap_value() - Set Packet type bitmap value
 * @vdev: vdev context
 * @value: bitmap value
 *
 * Return: None
 */
void ucfg_dp_set_pkt_type_bitmap_value(struct wlan_objmgr_vdev *vdev,
				       uint32_t value);

/**
 * ucfg_dp_intf_get_pkt_type_bitmap_value() - Get packt type bitmap info
 * @intf_ctx: DP interface context
 *
 * Return: bitmap information
 */
uint32_t ucfg_dp_intf_get_pkt_type_bitmap_value(void *intf_ctx);

/**
 * ucfg_dp_set_track_dest_ipv4_value() - Set track_dest_ipv4 value
 * @vdev: vdev context
 * @value: dest ipv4 value
 *
 * Return: None
 */
void ucfg_dp_set_track_dest_ipv4_value(struct wlan_objmgr_vdev *vdev,
				       uint32_t value);

/**
 * ucfg_dp_set_track_dest_port_value() - Set track_dest_port value
 * @vdev: vdev context
 * @value: dest port value
 *
 * Return: None
 */
void ucfg_dp_set_track_dest_port_value(struct wlan_objmgr_vdev *vdev,
				       uint32_t value);

/**
 * ucfg_dp_set_track_src_port_value() - Set track_dest_port value
 * @vdev: vdev context
 * @value: src port value
 *
 * Return: None
 */
void ucfg_dp_set_track_src_port_value(struct wlan_objmgr_vdev *vdev,
				      uint32_t value);

/**
 * ucfg_dp_set_track_dns_domain_len_value() - Set track_dns_domain_len value
 * @vdev: vdev context
 * @value: dns domain len value
 *
 * Return: None
 */
void ucfg_dp_set_track_dns_domain_len_value(struct wlan_objmgr_vdev *vdev,
					    uint32_t value);

/**
 * ucfg_dp_set_track_arp_ip_value() - Set track_arp_ip value
 * @vdev: vdev context
 * @value: ARP IP value
 *
 * Return: None
 */
void ucfg_dp_set_track_arp_ip_value(struct wlan_objmgr_vdev *vdev,
				    uint32_t value);

/**
 * ucfg_dp_get_pkt_type_bitmap_value() - Get pkt_type_bitmap value
 * @vdev: vdev context
 *
 * Return: pkt_type_bitmap value
 */
uint32_t ucfg_dp_get_pkt_type_bitmap_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_dns_payload_value() - Get dns_payload value
 * @vdev: vdev context
 * @dns_query : DNS query pointer
 *
 * Return: None
 */
void ucfg_dp_get_dns_payload_value(struct wlan_objmgr_vdev *vdev,
				   uint8_t *dns_query);

/**
 * ucfg_dp_get_track_dns_domain_len_value() - Get track_dns_domain_len value
 * @vdev: vdev context
 *
 * Return: track_dns_domain_len value
 */
uint32_t ucfg_dp_get_track_dns_domain_len_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_track_dest_port_value() - Get track_dest_port value
 * @vdev: vdev context
 *
 * Return: track_dest_port value
 */
uint32_t ucfg_dp_get_track_dest_port_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_track_src_port_value() - Get track_src_port value
 * @vdev: vdev context
 *
 * Return: track_src_port value
 */
uint32_t ucfg_dp_get_track_src_port_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_track_dest_ipv4_value() - Get track_dest_ipv4 value
 * @vdev: vdev context
 *
 * Return: track_dest_ipv4 value
 */
uint32_t ucfg_dp_get_track_dest_ipv4_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_dad_value() - Get dad value
 * @vdev: vdev context
 *
 * Return: dad value
 */
bool ucfg_dp_get_dad_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_con_status_value() - Get con_status value
 * @vdev: vdev context
 *
 * Return: con_status value
 */
bool ucfg_dp_get_con_status_value(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_intf_id() - Get intf_id
 * @vdev: vdev context
 *
 * Return: intf_id
 */
uint8_t ucfg_dp_get_intf_id(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_arp_stats() - Get ARP stats
 * @vdev: vdev context
 *
 * Return: ARP Stats
 */
struct dp_arp_stats *ucfg_dp_get_arp_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_icmpv4_stats() - Get ICMPv4 stats
 * @vdev: vdev context
 *
 * Return: ICMPv4 Stats
 */
struct dp_icmpv4_stats
*ucfg_dp_get_icmpv4_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_tcp_stats() - Get TCP stats
 * @vdev: vdev context
 *
 * Return: TCP Stats
 */
struct dp_tcp_stats *ucfg_dp_get_tcp_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_get_dns_stats() - Get DNS stats
 * @vdev: vdev context
 *
 * Return: DNS Stats
 */
struct dp_dns_stats *ucfg_dp_get_dns_stats(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_set_nud_stats_cb() - Register callback with WMI
 * @psoc: psoc context
 * @cookie: callback context
 *
 * Return: None
 */

void ucfg_dp_set_nud_stats_cb(struct wlan_objmgr_psoc *psoc, void *cookie);

/**
 * ucfg_dp_clear_nud_stats_cb() - Unregister callback with WMI
 * @psoc: psoc context
 *
 * Return: None
 */
void ucfg_dp_clear_nud_stats_cb(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_set_dump_dp_trace() - set DP Trace
 * @cmd_type : command
 * @count : Number of lines to dump
 *
 * Return: None
 */
void ucfg_dp_set_dump_dp_trace(uint16_t cmd_type, uint16_t count);

/**
 * ucfg_dp_req_get_arp_stats() - Send Get ARP set request to FW
 * @psoc: psoc context
 * @params : Get ARP stats param
 *
 * Return: Status
 */
QDF_STATUS
ucfg_dp_req_get_arp_stats(struct wlan_objmgr_psoc *psoc,
			  struct dp_get_arp_stats_params *params);

/**
 * ucfg_dp_req_set_arp_stats() - Send Set ARP set request to FW
 * @psoc: psoc context
 * @params : Set ARP stats param
 *
 * Return: Status
 */
QDF_STATUS
ucfg_dp_req_set_arp_stats(struct wlan_objmgr_psoc *psoc,
			  struct dp_set_arp_stats_params *params);

/**
 * ucfg_dp_register_hdd_callbacks() - Resiter HDD callbacks with DP component
 * @psoc: psoc handle
 * @cb_obj: Callback object
 *
 * Returns: None
 */
void ucfg_dp_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				    struct wlan_dp_psoc_callbacks *cb_obj);

/**
 * ucfg_dp_register_event_handler() - Resiter event handler with DP component
 * @psoc: psoc handle
 * @cb_obj: Callback object
 *
 * Returns: None
 */
void ucfg_dp_register_event_handler(struct wlan_objmgr_psoc *psoc,
				    struct wlan_dp_psoc_nb_ops *cb_obj);

/**
 * ucfg_dp_get_bus_bw_compute_interval() - Get bus bandwidth compute interval
 * @psoc: psoc handle
 *
 * Returns: Bus bandwidth compute interval
 */
uint32_t ucfg_dp_get_bus_bw_compute_interval(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_get_current_throughput_level() - get current bandwidth level
 * @psoc: psoc handle
 *
 * Return: current bandwidth level
 */
int ucfg_dp_get_current_throughput_level(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_get_txrx_stats() - get dp txrx stats
 * @vdev: vdev handle
 * @dp_stats : dp_stats pointer
 *
 * This function update dp_stats pointer with DP component
 * txrx stats
 * Return: 0 on success
 */
QDF_STATUS ucfg_dp_get_txrx_stats(struct wlan_objmgr_vdev *vdev,
				  struct dp_tx_rx_stats *dp_stats);

/*
 * ucfg_dp_get_net_dev_stats(): Get netdev stats
 * @vdev: vdev handle
 * @stats: To hold netdev stats
 *
 * Return: None
 */
void ucfg_dp_get_net_dev_stats(struct wlan_objmgr_vdev *vdev,
			       qdf_net_dev_stats *stats);

/*
 * ucfg_dp_clear_net_dev_stats(): Clear netdev stats
 * @dev: Pointer to netdev
 *
 * Return: None
 */
void ucfg_dp_clear_net_dev_stats(qdf_netdev_t dev);

/**
 * ucfg_dp_reset_cont_txtimeout_cnt() - Reset Tx Timeout count
 * @vdev: vdev handle
 *
 * Return: None
 */
void ucfg_dp_reset_cont_txtimeout_cnt(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_dp_set_rx_thread_affinity() - Set rx thread affinity mask
 * @psoc: psoc handle
 *
 * Return: None
 */
void ucfg_dp_set_rx_thread_affinity(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_get_disable_rx_ol_val() - Get Rx OL concurrency value
 * @psoc: psoc handle
 * @disable_conc : disable rx OL concurrency value
 * @disable_low_tput : disable rx OL low tput value
 *
 * this function reads and update value in pointer variable
 * passed as arguments to function.
 *
 * Return: None
 */

void ucfg_dp_get_disable_rx_ol_val(struct wlan_objmgr_psoc *psoc,
				   uint8_t *disable_conc,
				   uint8_t *disable_low_tput);

/**
 * ucfg_dp_get_rx_aggregation_val() - Get Rx aggregation values
 * @psoc: psoc handle
 *
 * Return: Rx aggregation value
 */
uint32_t ucfg_dp_get_rx_aggregation_val(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_set_rx_aggregation_val() - Set rx aggregation value
 * @psoc: psoc handle
 * @value : value to be set
 *
 * Return: None
 */
void ucfg_dp_set_rx_aggregation_val(struct wlan_objmgr_psoc *psoc,
				    uint32_t value);

/**
 * ucfg_dp_set_tc_based_dyn_gro() - Set tc based dynamic gro
 * @psoc: psoc handle
 * @value : value to be set
 *
 * Return: None
 */
void ucfg_dp_set_tc_based_dyn_gro(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * ucfg_dp_runtime_disable_rx_thread() - Disable rx thread
 * @vdev: vdev handle
 * @value : value to be set (true/false)
 *
 * Return: None
 */
void ucfg_dp_runtime_disable_rx_thread(struct wlan_objmgr_vdev *vdev,
				       bool value);

/**
 * ucfg_dp_get_napi_enabled() - Get NAPI enabled/disabled info
 * @psoc: psoc handle mapped to DP context
 *
 * Return: true if NAPI enabled
 */
bool ucfg_dp_get_napi_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_set_tc_ingress_prio() - Set tc ingress priority
 * @psoc: psoc handle mapped to DP context
 * @value: value to be set
 *
 * Return: None
 */
void ucfg_dp_set_tc_ingress_prio(struct wlan_objmgr_psoc *psoc, uint32_t value);

/**
 * ucfg_dp_nud_fail_data_stall_evt_enabled() - Check if NUD failuire data stall
 * detection is enabled
 *
 * Return: True if the data stall event is enabled
 */
bool ucfg_dp_nud_fail_data_stall_evt_enabled(void);

/**
 * ucfg_dp_fw_data_stall_evt_enabled() - Check if Fw data stall
 * detection is enabled
 *
 * Return: data stall event mask
 */
uint32_t ucfg_dp_fw_data_stall_evt_enabled(void);

/**
 * ucfg_dp_get_bus_bw_high_threshold() - Get the bus bw high threshold
 * @psoc: psoc handle
 *
 * Return: current bus bw high threshold
 */
uint32_t ucfg_dp_get_bus_bw_high_threshold(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_event_eapol_log() - send event to wlan diag
 * @nbuf: Network buffer ptr
 * @dir: direction
 *
 * Return: None
 */
void ucfg_dp_event_eapol_log(qdf_nbuf_t nbuf, enum qdf_proto_dir dir);

/**
 * ucfg_dp_softap_inspect_dhcp_packet() - Inspect DHCP packet
 * @vdev: Vdev handle
 * @nbuf: pointer to network buffer
 * @dir: direction
 *
 * Inspect the Tx/Rx frame, and send DHCP START/STOP notification to the FW
 * through WMI message, during DHCP based IP address acquisition phase.
 *
 * Return: error number
 */
QDF_STATUS
ucfg_dp_softap_inspect_dhcp_packet(struct wlan_objmgr_vdev *vdev,
				   qdf_nbuf_t nbuf, enum qdf_proto_dir dir);

void
dp_ucfg_enable_link_monitoring(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_vdev *vdev,
			       uint32_t threshold);

void
dp_ucfg_disable_link_monitoring(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev);

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * ucfg_dp_rx_skip_fisa() - Set flags to skip fisa aggregation
 * @value: allow or skip fisa
 *
 * Return: None
 */
void ucfg_dp_rx_skip_fisa(uint32_t value);

#else
static inline
void ucfg_dp_rx_skip_fisa(uint32_t value)
{
}
#endif

#ifdef DP_TRAFFIC_END_INDICATION
/**
 * ucfg_dp_traffic_end_indication_get() - Get data end indication info
 * @vdev: vdev handle
 * @info: variable to hold stored data end indication info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_dp_traffic_end_indication_get(struct wlan_objmgr_vdev *vdev,
				   struct dp_traffic_end_indication *info);
/**
 * ucfg_dp_traffic_end_indication_set() - Store data end indication info
 * @vdev: vdev handle
 * @info: variable holding new data end indication info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_dp_traffic_end_indication_set(struct wlan_objmgr_vdev *vdev,
				   struct dp_traffic_end_indication info);
/**
 * ucfg_dp_traffic_end_indication_update_dscp() - update dscp value to default
 * @psoc: psoc handle
 * @vdev_id: vdev id
 * @dscp: dscp value to be updated
 *
 * Return: void
 */
void
ucfg_dp_traffic_end_indication_update_dscp(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   unsigned char *dscp);
#else
static inline QDF_STATUS
ucfg_dp_traffic_end_indication_get(struct wlan_objmgr_vdev *vdev,
				   struct dp_traffic_end_indication *info)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_dp_traffic_end_indication_set(struct wlan_objmgr_vdev *vdev,
				   struct dp_traffic_end_indication info)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_dp_traffic_end_indication_update_dscp(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   unsigned char *dscp)
{}
#endif

/*
 * ucfg_dp_prealloc_init() - Pre-allocate DP memory
 * @ctrl_psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS ucfg_dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc);

/*
 * ucfg_dp_prealloc_deinit() - Free pre-alloced DP memory
 *
 * Return: None
 */
void ucfg_dp_prealloc_deinit(void);

#ifdef DP_MEM_PRE_ALLOC
/**
 * ucfg_dp_prealloc_get_consistent_mem_unaligned() - gets pre-alloc unaligned
 *						     consistent memory
 * @size: total memory size
 * @base_addr: pointer to dma address
 * @ring_type: HAL ring type that requires memory
 *
 * Return: memory virtual address pointer on success, NULL on failure
 */
void *ucfg_dp_prealloc_get_consistent_mem_unaligned(qdf_size_t size,
						    qdf_dma_addr_t *base_addr,
						    uint32_t ring_type);

/**
 * ucfg_dp_prealloc_put_consistent_mem_unaligned() - puts back pre-alloc
 * unaligned consistent memory
 * @va_unaligned: memory virtual address pointer
 *
 * Return: None
 */
void ucfg_dp_prealloc_put_consistent_mem_unaligned(void *va_unaligned);
#endif

#ifdef FEATURE_DIRECT_LINK
/**
 * ucfg_dp_direct_link_init() - Initializes Direct Link datapath
 * @psoc: psoc handle
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_dp_direct_link_init(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_direct_link_deinit() - De-initializes Direct Link datapath
 * @psoc: psoc handle
 *
 * Return: None
 */
void ucfg_dp_direct_link_deinit(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_dp_wfds_handle_request_mem_ind() - Process request memory indication
 *  received from QMI server
 * @mem_msg: pointer to memory request indication message
 *
 * Return: None
 */
void
ucfg_dp_wfds_handle_request_mem_ind(struct wlan_qmi_wfds_mem_ind_msg *mem_msg);

/**
 * ucfg_dp_wfds_handle_ipcc_map_n_cfg_ind() - Process IPCC map and configure
 *  indication received from QMI server
 * @ipcc_msg: pointer to IPCC map and configure indication message
 *
 * Return: None
 */
void
ucfg_dp_wfds_handle_ipcc_map_n_cfg_ind(struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg *ipcc_msg);

/**
 * ucfg_dp_wfds_new_server() - New server callback triggered when service is up.
 *  Connect to the service as part of this call.
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_dp_wfds_new_server(void);

/**
 * ucfg_dp_wfds_del_server() - Del server callback triggered when service is
 *  down.
 *
 * Return: None
 */
void ucfg_dp_wfds_del_server(void);

/**
 * ucfg_dp_config_direct_link() - Set direct link config for vdev
 * @vdev: objmgr Vdev handle
 * @config_direct_link: Flag to enable direct link path
 * @enable_low_latency: Flag to enable low link latency
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_dp_config_direct_link(struct wlan_objmgr_vdev *vdev,
				      bool config_direct_link,
				      bool enable_low_latency);
#else
static inline
QDF_STATUS ucfg_dp_direct_link_init(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_dp_direct_link_deinit(struct wlan_objmgr_psoc *psoc)
{
}

#ifdef QMI_WFDS
static inline void
ucfg_dp_wfds_handle_request_mem_ind(struct wlan_qmi_wfds_mem_ind_msg *mem_msg)
{
}

static inline void
ucfg_dp_wfds_handle_ipcc_map_n_cfg_ind(struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg *ipcc_msg)
{
}

static inline QDF_STATUS ucfg_dp_wfds_new_server(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_dp_wfds_del_server(void)
{
}
#endif

static inline
QDF_STATUS ucfg_dp_config_direct_link(struct wlan_objmgr_vdev *vdev,
				      bool enable_direct_link,
				      bool enable_low_latency)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ucfg_dp_txrx_init() - initialize DP TXRX module
 * @soc: ol_txrx_soc_handle
 * @pdev_id: id of dp pdev handle
 * @config: configuration for DP TXRX modules
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS ucfg_dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			     struct dp_txrx_config *config);

/**
 * ucfg_dp_txrx_deinit() - de-initialize DP TXRX module
 * @soc: ol_txrx_soc_handle
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS ucfg_dp_txrx_deinit(ol_txrx_soc_handle soc);

/**
 * ucfg_dp_txrx_ext_dump_stats() - dump txrx external module stats
 * @soc: ol_txrx_soc_handle object
 * @stats_id: id  for the module whose stats are needed
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS ucfg_dp_txrx_ext_dump_stats(ol_txrx_soc_handle soc,
				       uint8_t stats_id);
/**
 * ucfg_dp_txrx_set_cpu_mask() - set CPU mask for RX threads
 * @soc: ol_txrx_soc_handle object
 * @new_mask: New CPU mask pointer
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS ucfg_dp_txrx_set_cpu_mask(ol_txrx_soc_handle soc,
				     qdf_cpu_mask *new_mask);

#endif /* _WLAN_DP_UCFGi_API_H_ */

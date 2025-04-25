/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_TX_RX_H)
#define WLAN_HDD_TX_RX_H

/**
 * DOC: wlan_hdd_tx_rx.h
 *
 * Linux HDD Tx/RX APIs
 */

#include <wlan_hdd_includes.h>
#include <cds_api.h>
#include <linux/skbuff.h>
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <qdf_tracepoint.h>
#include <qdf_pkt_add_timestamp.h>
#include "wlan_dp_public_struct.h"

struct hdd_netif_queue_history;
struct hdd_context;

#define hdd_dp_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_HDD_DATA, params)

#define hdd_dp_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_HDD_DATA, params)

#define hdd_dp_alert_rl(params...) \
			QDF_TRACE_FATAL_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_err_rl(params...) \
			QDF_TRACE_ERROR_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_warn_rl(params...) \
			QDF_TRACE_WARN_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_info_rl(params...) \
			QDF_TRACE_INFO_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_debug_rl(params...) \
			QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD_DATA, params)

#define hdd_dp_enter() hdd_dp_debug("enter")
#define hdd_dp_enter_dev(dev) hdd_dp_debug("enter(%s)", (dev)->name)
#define hdd_dp_exit() hdd_dp_debug("exit")

#define HDD_ETHERTYPE_802_1_X              0x888E
#ifdef FEATURE_WLAN_WAPI
#define HDD_ETHERTYPE_WAI                  0x88b4
#define IS_HDD_ETHERTYPE_WAI(_skb) (ntohs(_skb->protocol) == \
					HDD_ETHERTYPE_WAI)
#else
#define IS_HDD_ETHERTYPE_WAI(_skb) (false)
#endif

#define HDD_PSB_CFG_INVALID                   0xFF
#define HDD_PSB_CHANGED                       0xFF
#define SME_QOS_UAPSD_CFG_BK_CHANGED_MASK     0xF1
#define SME_QOS_UAPSD_CFG_BE_CHANGED_MASK     0xF2
#define SME_QOS_UAPSD_CFG_VI_CHANGED_MASK     0xF4
#define SME_QOS_UAPSD_CFG_VO_CHANGED_MASK     0xF8

#ifdef CFG80211_CTRL_FRAME_SRC_ADDR_TA_ADDR
#define SEND_EAPOL_OVER_NL true
#else
#define SEND_EAPOL_OVER_NL  false
#endif

netdev_tx_t hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);

/**
 * hdd_tx_timeout() - Wrapper function to protect __hdd_tx_timeout from SSR
 * @dev: pointer to net_device structure
 * @txqueue: tx queue
 *
 * Function called by OS if there is any timeout during transmission.
 * Since HDD simply enqueues packet and returns control to OS right away,
 * this would never be invoked
 *
 * Return: none
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_tx_timeout(struct net_device *dev, unsigned int txqueue);
#else
void hdd_tx_timeout(struct net_device *dev);
#endif

#ifdef WLAN_FEATURE_TSF_PLUS_SOCK_TS
/**
 * hdd_tsf_timestamp_rx() - HDD function to set rx packet timestamp
 * @ctx: pointer to HDD context
 * @netbuf: pointer to skb
 *
 * Return: None
 */
void hdd_tsf_timestamp_rx(hdd_cb_handle ctx, qdf_nbuf_t netbuf);

/**
 * hdd_get_tsf_time_cb() - HDD helper function to get TSF time
 * @netdev: netdev
 * @input_time: Input time
 * @tsf_time: time from TFS module
 *
 * Return: None
 */
void hdd_get_tsf_time_cb(qdf_netdev_t netdev, uint64_t input_time,
			 uint64_t *tsf_time);
#else
static inline
void hdd_tsf_timestamp_rx(hdd_cb_handle ctx, qdf_nbuf_t netbuf) { }

static inline
void hdd_get_tsf_time_cb(qdf_netdev_t netdev, uint64_t input_time,
			 uint64_t *tsf_time)
{
}
#endif

/**
 * hdd_legacy_gro_get_napi() - HDD function to get napi in legacy gro case
 * @nbuf: n/w buffer pointer
 * @enable_rxthread: Rx thread enabled/disabled
 *
 * Return: qdf napi struct on success, NULL on failure
 */
qdf_napi_struct
*hdd_legacy_gro_get_napi(qdf_nbuf_t nbuf, bool enable_rxthread);

/**
 * hdd_disable_rx_ol_in_concurrency() - Disable RX Offload in concurrency
 *  for rx
 * @disable: true if rx offload should be disabled in concurrency
 *
 * Return: none
 */
void hdd_disable_rx_ol_in_concurrency(bool disable);

/**
 * hdd_tx_queue_cb() - Disable/Enable the Transmit Queues
 * @hdd_handle: HDD handle
 * @vdev_id: vdev id
 * @action: Action to be taken on the Tx Queues
 * @reason: Reason for the netif action
 *
 * Return: None
 */
void hdd_tx_queue_cb(hdd_handle_t hdd_handle, uint32_t vdev_id,
		     enum netif_action_type action,
		     enum netif_reason_type reason);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void hdd_tx_resume_cb(void *adapter_context, bool tx_resume);

/**
 * hdd_tx_flow_control_is_pause() - Is TX Q paused by flow control
 * @adapter_context: pointer to vdev apdapter
 *
 * Return: true if TX Q is paused by flow control
 */
bool hdd_tx_flow_control_is_pause(void *adapter_context);

/**
 * hdd_register_tx_flow_control() - Register TX Flow control
 * @adapter: adapter handle
 * @timer_callback: timer callback
 * @flow_control_fp: txrx flow control
 * @flow_control_is_pause_fp: is txrx paused by flow control
 *
 * Return: none
 */
void hdd_register_tx_flow_control(struct hdd_adapter *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flow_control_fp,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause_fp);
void hdd_deregister_tx_flow_control(struct hdd_adapter *adapter);

/**
 * hdd_get_tx_resource() - check tx resources and take action
 * @vdev_id: vdev id mapped to HDD adapter
 * @mac_addr: mac address
 *
 * Return: none
 */
void hdd_get_tx_resource(uint8_t vdev_id,
			 struct qdf_mac_addr *mac_addr);

/**
 * hdd_get_tx_flow_low_watermark() - Get TX flow low watermark info
 * @cb_ctx: HDD opaque ctx
 * @netdev: netdev
 *
 * Return: flow low watermark value
 */
unsigned int
hdd_get_tx_flow_low_watermark(hdd_cb_handle cb_ctx, qdf_netdev_t netdev);
#else
static inline void hdd_tx_resume_cb(void *adapter_context, bool tx_resume)
{
}
static inline bool hdd_tx_flow_control_is_pause(void *adapter_context)
{
	return false;
}
static inline void hdd_register_tx_flow_control(struct hdd_adapter *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flow_control_fp,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause)
{
}
static inline void hdd_deregister_tx_flow_control(struct hdd_adapter *adapter)
{
}

/**
 * hdd_get_tx_resource() - check tx resources and take action
 * @vdev_id: vdev id mapped to HDD adapter
 * @mac_addr: mac address
 *
 * Return: none
 */
static inline
void hdd_get_tx_resource(uint8_t vdev_id,
			 struct qdf_mac_addr *mac_addr) { }

static inline unsigned int
hdd_get_tx_flow_low_watermark(hdd_cb_handle cb_ctx, qdf_netdev_t netdev)
{
	return 0;
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#if defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || \
		defined(QCA_HL_NETDEV_FLOW_CONTROL)
void hdd_tx_resume_timer_expired_handler(void *adapter_context);
#else
static inline void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
}
#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void hdd_register_hl_netdev_fc_timer(struct hdd_adapter *adapter,
				     qdf_mc_timer_callback_t timer_callback);
void hdd_deregister_hl_netdev_fc_timer(struct hdd_adapter *adapter);
#else
static inline void hdd_register_hl_netdev_fc_timer(struct hdd_adapter *adapter,
						   qdf_mc_timer_callback_t
						   timer_callback)
{}

static inline void
	hdd_deregister_hl_netdev_fc_timer(struct hdd_adapter *adapter)
{}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

const char *hdd_reason_type_to_string(enum netif_reason_type reason);
const char *hdd_action_type_to_string(enum netif_action_type action);

void wlan_hdd_netif_queue_control(struct hdd_adapter *adapter,
		enum netif_action_type action, enum netif_reason_type reason);

#ifdef FEATURE_MONITOR_MODE_SUPPORT
int hdd_set_mon_rx_cb(struct net_device *dev);
#else
static inline
int hdd_set_mon_rx_cb(struct net_device *dev)
{
	return 0;
}
#endif

/**
 * hdd_set_udp_qos_upgrade_config() - Set the threshold for UDP packet
 *				      QoS upgrade.
 * @adapter: adapter for which this configuration is to be applied
 * @priority: the threshold priority
 *
 * Returns: 0 on success, -EINVAL on failure
 */
int hdd_set_udp_qos_upgrade_config(struct hdd_adapter *adapter,
				   uint8_t priority);

/*
 * As of the 4.7 kernel, net_device->trans_start is removed. Create shims to
 * support compiling against older versions of the kernel.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
static inline void netif_trans_update(struct net_device *dev)
{
	dev->trans_start = jiffies;
}

#define TX_TIMEOUT_TRACE(dev, module_id) QDF_TRACE( \
	module_id, QDF_TRACE_LEVEL_ERROR, \
	"%s: Transmission timeout occurred jiffies %lu trans_start %lu", \
	__func__, jiffies, dev->trans_start)
#else
#define TX_TIMEOUT_TRACE(dev, module_id) QDF_TRACE( \
	module_id, QDF_TRACE_LEVEL_ERROR, \
	"%s: Transmission timeout occurred jiffies %lu", \
	__func__, jiffies)
#endif

/**
 * hdd_dp_cfg_update() - update hdd config for HDD DP INIs
 * @psoc: Pointer to psoc obj
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: None
 */
void hdd_dp_cfg_update(struct wlan_objmgr_psoc *psoc,
		       struct hdd_context *hdd_ctx);

/**
 * hdd_print_netdev_txq_status() - print netdev tx queue status
 * @dev: Pointer to network device
 *
 * This function is used to print netdev tx queue status
 *
 * Return: None
 */
void hdd_print_netdev_txq_status(struct net_device *dev);

/**
 * wlan_hdd_dump_queue_history_state() - Dump hdd queue history states
 * @q_hist: pointer to hdd queue history structure
 * @buf: buffer where the queue history string is dumped
 * @size: size of the buffer
 *
 * Dump hdd queue history states into a buffer
 *
 * Return: number of bytes written to the buffer
 */
uint32_t
wlan_hdd_dump_queue_history_state(struct hdd_netif_queue_history *q_hist,
				  char *buf, uint32_t size);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * wlan_hdd_set_tx_flow_info() - To set TX flow info
 *
 * This routine is called to set TX flow info
 *
 * Return: None
 */
void wlan_hdd_set_tx_flow_info(void);
#else
static inline void wlan_hdd_set_tx_flow_info(void)
{
}
#endif
#endif /* end #if !defined(WLAN_HDD_TX_RX_H) */

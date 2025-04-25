/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_tx_rx.c
 *
 * Linux HDD Tx/RX APIs
 */

/* denote that this file does not allow legacy hddLog */
#define HDD_DISALLOW_LEGACY_HDDLOG 1
#include "osif_sync.h"
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_napi.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/inetdevice.h>
#include <cds_sched.h>
#include <cds_utils.h>

#include <linux/wireless.h>
#include <net/cfg80211.h>
#include "sap_api.h"
#include "wlan_hdd_wmm.h"
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_flow_ctrl_v2.h>
#include <cdp_txrx_misc.h>
#include "wlan_hdd_power.h"
#include "wlan_hdd_cfg80211.h"
#include <wlan_hdd_tsf.h>
#include <net/tcp.h>

#include <ol_defines.h>
#include "cfg_ucfg_api.h"
#include "target_type.h"
#include "wlan_hdd_object_manager.h"
#include <wlan_hdd_sar_limits.h>
#include "wlan_hdd_object_manager.h"
#include "wlan_dp_ucfg_api.h"
#include "os_if_dp.h"
#include "wlan_ipa_ucfg_api.h"
#include "wlan_hdd_stats.h"

#ifdef TX_MULTIQ_PER_AC
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/*
 * Mapping Linux AC interpretation to SME AC.
 * Host has 4 queues per access category (4 AC) and 1 high priority queue.
 * 16 flow-controlled queues for regular traffic and one non-flow
 * controlled queue for high priority control traffic(EOPOL, DHCP).
 * The seventeenth queue is mapped to AC_VO to allow for proper prioritization.
 */
const uint8_t hdd_qdisc_ac_to_tl_ac[] = {
	SME_AC_VO,
	SME_AC_VO,
	SME_AC_VO,
	SME_AC_VO,
	SME_AC_VI,
	SME_AC_VI,
	SME_AC_VI,
	SME_AC_VI,
	SME_AC_BE,
	SME_AC_BE,
	SME_AC_BE,
	SME_AC_BE,
	SME_AC_BK,
	SME_AC_BK,
	SME_AC_BK,
	SME_AC_BK,
	SME_AC_VO,
};

#else
const uint8_t hdd_qdisc_ac_to_tl_ac[] = {
	SME_AC_VO,
	SME_AC_VO,
	SME_AC_VO,
	SME_AC_VO,
	SME_AC_VI,
	SME_AC_VI,
	SME_AC_VI,
	SME_AC_VI,
	SME_AC_BE,
	SME_AC_BE,
	SME_AC_BE,
	SME_AC_BE,
	SME_AC_BK,
	SME_AC_BK,
	SME_AC_BK,
	SME_AC_BK,
};

#endif
#else
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/*
 * Mapping Linux AC interpretation to SME AC.
 * Host has 5 tx queues, 4 flow-controlled queues for regular traffic and
 * one non-flow-controlled queue for high priority control traffic(EOPOL, DHCP).
 * The fifth queue is mapped to AC_VO to allow for proper prioritization.
 */
const uint8_t hdd_qdisc_ac_to_tl_ac[] = {
	SME_AC_VO,
	SME_AC_VI,
	SME_AC_BE,
	SME_AC_BK,
	SME_AC_VO,
};

#else
const uint8_t hdd_qdisc_ac_to_tl_ac[] = {
	SME_AC_VO,
	SME_AC_VI,
	SME_AC_BE,
	SME_AC_BK,
};

#endif
#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void hdd_register_hl_netdev_fc_timer(struct hdd_adapter *adapter,
				     qdf_mc_timer_callback_t timer_callback)
{
	if (!adapter->tx_flow_timer_initialized) {
		qdf_mc_timer_init(&adapter->tx_flow_control_timer,
				  QDF_TIMER_TYPE_SW, timer_callback, adapter);
		adapter->tx_flow_timer_initialized = true;
	}
}

/**
 * hdd_deregister_hl_netdev_fc_timer() - Deregister HL Flow Control Timer
 * @adapter: adapter handle
 *
 * Return: none
 */
void hdd_deregister_hl_netdev_fc_timer(struct hdd_adapter *adapter)
{
	if (adapter->tx_flow_timer_initialized) {
		qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		qdf_mc_timer_destroy(&adapter->tx_flow_control_timer);
		adapter->tx_flow_timer_initialized = false;
	}
}

/**
 * hdd_tx_resume_timer_expired_handler() - TX Q resume timer handler
 * @adapter_context: pointer to vdev adapter
 *
 * Return: None
 */
void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *)adapter_context;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	u32 p_qpaused;
	u32 np_qpaused;

	if (!adapter) {
		hdd_err("invalid adapter context");
		return;
	}

	cdp_display_stats(soc, CDP_DUMP_TX_FLOW_POOL_INFO,
			  QDF_STATS_VERBOSITY_LEVEL_LOW);
	wlan_hdd_display_adapter_netif_queue_history(adapter);
	hdd_debug("Enabling queues");
	spin_lock_bh(&adapter->pause_map_lock);
	p_qpaused = adapter->pause_map & BIT(WLAN_DATA_FLOW_CONTROL_PRIORITY);
	np_qpaused = adapter->pause_map & BIT(WLAN_DATA_FLOW_CONTROL);
	spin_unlock_bh(&adapter->pause_map_lock);

	if (p_qpaused) {
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_NETIF_PRIORITY_QUEUE_ON,
					     WLAN_DATA_FLOW_CONTROL_PRIORITY);
		cdp_hl_fc_set_os_queue_status(soc,
					      adapter->deflink->vdev_id,
					      WLAN_NETIF_PRIORITY_QUEUE_ON);
	}
	if (np_qpaused) {
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_WAKE_NON_PRIORITY_QUEUE,
					     WLAN_DATA_FLOW_CONTROL);
		cdp_hl_fc_set_os_queue_status(soc,
					      adapter->deflink->vdev_id,
					      WLAN_WAKE_NON_PRIORITY_QUEUE);
	}
}

#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_tx_resume_timer_expired_handler() - TX Q resume timer handler
 * @adapter_context: pointer to vdev adapter
 *
 * If Blocked OS Q is not resumed during timeout period, to prevent
 * permanent stall, resume OS Q forcefully.
 *
 * Return: None
 */
void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if (!adapter) {
		/* INVALID ARG */
		return;
	}

	hdd_debug("Enabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_WAKE_ALL_NETIF_QUEUE,
				     WLAN_CONTROL_PATH);
}

/**
 * hdd_tx_resume_false() - Resume OS TX Q false leads to queue disabling
 * @adapter: pointer to hdd adapter
 * @tx_resume: TX Q resume trigger
 *
 *
 * Return: None
 */
static void
hdd_tx_resume_false(struct hdd_adapter *adapter, bool tx_resume)
{
	QDF_STATUS status;
	qdf_mc_timer_t *fc_timer;

	if (true == tx_resume)
		return;

	/* Pause TX  */
	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_STOP_ALL_NETIF_QUEUE,
				     WLAN_DATA_FLOW_CONTROL);

	fc_timer = &adapter->tx_flow_control_timer;
	if (QDF_TIMER_STATE_STOPPED != qdf_mc_timer_get_current_state(fc_timer))
		goto update_stats;


	status = qdf_mc_timer_start(fc_timer,
				    WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to start tx_flow_control_timer");
	else
		adapter->deflink->hdd_stats.tx_rx_stats.txflow_timer_cnt++;

update_stats:
	adapter->deflink->hdd_stats.tx_rx_stats.txflow_pause_cnt++;
	adapter->deflink->hdd_stats.tx_rx_stats.is_txflow_paused = true;
}

/**
 * hdd_tx_resume_cb() - Resume OS TX Q.
 * @adapter_context: pointer to vdev apdapter
 * @tx_resume: TX Q resume trigger
 *
 * Q was stopped due to WLAN TX path low resource condition
 *
 * Return: None
 */
void hdd_tx_resume_cb(void *adapter_context, bool tx_resume)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;
	struct hdd_station_ctx *hdd_sta_ctx = NULL;

	if (!adapter) {
		/* INVALID ARG */
		return;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter->deflink);

	/* Resume TX  */
	if (true == tx_resume) {
		if (QDF_TIMER_STATE_STOPPED !=
		    qdf_mc_timer_get_current_state(&adapter->
						   tx_flow_control_timer)) {
			qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		}
		hdd_debug("Enabling queues");
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_WAKE_ALL_NETIF_QUEUE,
					     WLAN_DATA_FLOW_CONTROL);
		adapter->deflink->hdd_stats.tx_rx_stats.is_txflow_paused =
									false;
		adapter->deflink->hdd_stats.tx_rx_stats.txflow_unpause_cnt++;
	}
	hdd_tx_resume_false(adapter, tx_resume);
}

bool hdd_tx_flow_control_is_pause(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if ((!adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		/* INVALID ARG */
		hdd_err("invalid adapter %pK", adapter);
		return false;
	}

	return adapter->pause_map & (1 << WLAN_DATA_FLOW_CONTROL);
}

void hdd_register_tx_flow_control(struct hdd_adapter *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flow_control_fp,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause_fp)
{
	if (adapter->tx_flow_timer_initialized == false) {
		qdf_mc_timer_init(&adapter->tx_flow_control_timer,
			  QDF_TIMER_TYPE_SW,
			  timer_callback,
			  adapter);
		adapter->tx_flow_timer_initialized = true;
	}
	cdp_fc_register(cds_get_context(QDF_MODULE_ID_SOC),
		adapter->deflink->vdev_id, flow_control_fp, adapter,
		flow_control_is_pause_fp);
}

/**
 * hdd_deregister_tx_flow_control() - Deregister TX Flow control
 * @adapter: adapter handle
 *
 * Return: none
 */
void hdd_deregister_tx_flow_control(struct hdd_adapter *adapter)
{
	cdp_fc_deregister(cds_get_context(QDF_MODULE_ID_SOC),
			adapter->deflink->vdev_id);
	if (adapter->tx_flow_timer_initialized == true) {
		qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		qdf_mc_timer_destroy(&adapter->tx_flow_control_timer);
		adapter->tx_flow_timer_initialized = false;
	}
}

void hdd_get_tx_resource(uint8_t vdev_id,
			 struct qdf_mac_addr *mac_addr)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	uint16_t timer_value = WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME;
	struct wlan_hdd_link_info *link_info;
	qdf_mc_timer_t *fc_timer;

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, vdev_id);
	if (!link_info)
		return;

	adapter = link_info->adapter;
	if (adapter->device_mode == QDF_P2P_GO_MODE ||
	    adapter->device_mode == QDF_SAP_MODE)
		timer_value = WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME;

	if (cdp_fc_get_tx_resource(cds_get_context(QDF_MODULE_ID_SOC),
				   OL_TXRX_PDEV_ID, *mac_addr,
				   adapter->tx_flow_low_watermark,
				   adapter->tx_flow_hi_watermark_offset))
		return;

	hdd_debug("Disabling queues lwm %d hwm offset %d",
		  adapter->tx_flow_low_watermark,
		  adapter->tx_flow_hi_watermark_offset);
	wlan_hdd_netif_queue_control(adapter, WLAN_STOP_ALL_NETIF_QUEUE,
				     WLAN_DATA_FLOW_CONTROL);

	fc_timer = &adapter->tx_flow_control_timer;
	if ((adapter->tx_flow_timer_initialized == true) &&
	    (QDF_TIMER_STATE_STOPPED ==
	     qdf_mc_timer_get_current_state(fc_timer))) {
		qdf_mc_timer_start(fc_timer, timer_value);
		link_info->hdd_stats.tx_rx_stats.txflow_timer_cnt++;
		link_info->hdd_stats.tx_rx_stats.txflow_pause_cnt++;
		link_info->hdd_stats.tx_rx_stats.is_txflow_paused = true;
	}
}

unsigned int
hdd_get_tx_flow_low_watermark(hdd_cb_handle cb_ctx, qdf_netdev_t netdev)
{
	struct hdd_adapter *adapter;

	adapter = WLAN_HDD_GET_PRIV_PTR(netdev);
	if (!adapter)
		return 0;

	return adapter->tx_flow_low_watermark;
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#ifdef RECEIVE_OFFLOAD
qdf_napi_struct
*hdd_legacy_gro_get_napi(qdf_nbuf_t nbuf, bool enable_rxthread)
{
	struct qca_napi_info *qca_napii;
	struct qca_napi_data *napid;
	struct napi_struct *napi_to_use;

	napid = hdd_napi_get_all();
	if (unlikely(!napid))
		return NULL;

	qca_napii = hif_get_napi(QDF_NBUF_CB_RX_CTX_ID(nbuf), napid);
	if (unlikely(!qca_napii))
		return NULL;

	/*
	 * As we are breaking context in Rxthread mode, there is rx_thread NAPI
	 * corresponds each hif_napi.
	 */
	if (enable_rxthread)
		napi_to_use =  &qca_napii->rx_thread_napi;
	else
		napi_to_use = &qca_napii->napi;

	return (qdf_napi_struct *)napi_to_use;
}
#else
qdf_napi_struct
*hdd_legacy_gro_get_napi(qdf_nbuf_t nbuf, bool enable_rxthread)
{
	return NULL;
}
#endif

int hdd_set_udp_qos_upgrade_config(struct hdd_adapter *adapter,
				   uint8_t priority)
{
	if (adapter->device_mode != QDF_STA_MODE) {
		hdd_info_rl("Data priority upgrade only allowed in STA mode:%d",
			    adapter->device_mode);
		return -EINVAL;
	}

	if (priority >= QCA_WLAN_AC_ALL) {
		hdd_err_rl("Invalid data priority: %d", priority);
		return -EINVAL;
	}

	adapter->upgrade_udp_qos_threshold = priority;

	hdd_debug("UDP packets qos upgrade to: %d", priority);

	return 0;
}

#ifdef QCA_WIFI_FTM
static inline bool
hdd_drop_tx_packet_on_ftm(struct sk_buff *skb)
{
	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		kfree_skb(skb);
		return true;
	}
	return false;
}
#else
static inline bool
hdd_drop_tx_packet_on_ftm(struct sk_buff *skb)
{
	return false;
}
#endif

/**
 * __hdd_hard_start_xmit() - Transmit a frame
 * @skb: pointer to OS packet (sk_buff)
 * @dev: pointer to network device
 *
 * Function registered with the Linux OS for transmitting
 * packets. This version of the function directly passes
 * the packet to Transport Layer.
 * In case of any packet drop or error, log the error with
 * INFO HIGH/LOW/MEDIUM to avoid excessive logging in kmsg.
 *
 * Return: None
 */
static void __hdd_hard_start_xmit(struct sk_buff *skb,
				  struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_tx_rx_stats *stats =
				&adapter->deflink->hdd_stats.tx_rx_stats;
	struct hdd_station_ctx *sta_ctx = &adapter->deflink->session.station;
	int cpu = qdf_get_smp_processor_id();
	bool granted;
	sme_ac_enum_type ac;
	enum sme_qos_wmmuptype up;
	QDF_STATUS status;

	if (hdd_drop_tx_packet_on_ftm(skb))
		return;

	osif_dp_mark_pkt_type(skb);
	hdd_tx_latency_record_ingress_ts(adapter, skb);

	/* Get TL AC corresponding to Qdisc queue index/AC. */
	ac = hdd_qdisc_ac_to_tl_ac[skb->queue_mapping];

	/*
	 * user priority from IP header, which is already extracted and set from
	 * select_queue call back function
	 */
	up = skb->priority;

	++stats->per_cpu[cpu].tx_classified_ac[ac];
#ifdef HDD_WMM_DEBUG
	QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Classified as ac %d up %d", __func__, ac, up);
#endif /* HDD_WMM_DEBUG */

	if (HDD_PSB_CHANGED == adapter->psb_changed) {
		/*
		 * Function which will determine acquire admittance for a
		 * WMM AC is required or not based on psb configuration done
		 * in the framework
		 */
		hdd_wmm_acquire_access_required(adapter, ac);
	}
	/*
	 * Make sure we already have access to this access category
	 * or it is EAPOL or WAPI frame during initial authentication which
	 * can have artificially boosted higher qos priority.
	 */

	if (((adapter->psb_changed & (1 << ac)) &&
	     likely(adapter->hdd_wmm_status.ac_status[ac].
			is_access_allowed)) ||
	    ((!sta_ctx->conn_info.is_authenticated) &&
	     (QDF_NBUF_CB_PACKET_TYPE_EAPOL ==
	      QDF_NBUF_CB_GET_PACKET_TYPE(skb) ||
	      QDF_NBUF_CB_PACKET_TYPE_WAPI ==
	      QDF_NBUF_CB_GET_PACKET_TYPE(skb)))) {
		granted = true;
	} else {
		status = hdd_wmm_acquire_access(adapter, ac, &granted);
		adapter->psb_changed |= (1 << ac);
	}

	if (!granted) {
		bool is_default_ac = false;
		/*
		 * ADDTS request for this AC is sent, for now
		 * send this packet through next available lower
		 * Access category until ADDTS negotiation completes.
		 */
		while (!likely
			       (adapter->hdd_wmm_status.ac_status[ac].
			       is_access_allowed)) {
			switch (ac) {
			case SME_AC_VO:
				ac = SME_AC_VI;
				up = SME_QOS_WMM_UP_VI;
				break;
			case SME_AC_VI:
				ac = SME_AC_BE;
				up = SME_QOS_WMM_UP_BE;
				break;
			case SME_AC_BE:
				ac = SME_AC_BK;
				up = SME_QOS_WMM_UP_BK;
				break;
			default:
				ac = SME_AC_BK;
				up = SME_QOS_WMM_UP_BK;
				is_default_ac = true;
				break;
			}
			if (is_default_ac)
				break;
		}
		skb->priority = up;
		skb->queue_mapping = hdd_linux_up_to_ac_map[up];
	}

	/*
	 * vdev in link_info is directly dereferenced because this is per
	 * packet path, hdd_get_vdev_by_user() usage will be very costly
	 * as it involves lock access.
	 * Expectation here is vdev will be present during TX/RX processing
	 * and also DP internally maintaining vdev ref count
	 */
	status = ucfg_dp_start_xmit((qdf_nbuf_t)skb, adapter->deflink->vdev);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		netif_trans_update(dev);
		wlan_hdd_sar_unsolicited_timer_start(adapter->hdd_ctx);
	} else {
		++stats->per_cpu[cpu].tx_dropped_ac[ac];
	}
}

/**
 * hdd_hard_start_xmit() - Wrapper function to protect
 * __hdd_hard_start_xmit from SSR
 * @skb: pointer to OS packet
 * @net_dev: pointer to net_device structure
 *
 * Function called by OS if any packet needs to transmit.
 *
 * Return: Always returns NETDEV_TX_OK
 */
netdev_tx_t hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	__hdd_hard_start_xmit(skb, net_dev);

	return NETDEV_TX_OK;
}

/**
 * __hdd_tx_timeout() - TX timeout handler
 * @dev: pointer to network device
 *
 * This function is registered as a netdev ndo_tx_timeout method, and
 * is invoked by the kernel if the driver takes too long to transmit a
 * frame.
 *
 * Return: None
 */
static void __hdd_tx_timeout(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	struct netdev_queue *txq;
	struct wlan_objmgr_vdev *vdev;
	int i = 0;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_debug("Device is suspended, ignore WD timeout");
		return;
	}

	TX_TIMEOUT_TRACE(dev, QDF_MODULE_ID_HDD_DATA);
	DPTRACE(qdf_dp_trace(NULL, QDF_DP_TRACE_HDD_TX_TIMEOUT,
				QDF_TRACE_DEFAULT_PDEV_ID,
				NULL, 0, QDF_TX));

	/* Getting here implies we disabled the TX queues for too
	 * long. Queues are disabled either because of disassociation
	 * or low resource scenarios. In case of disassociation it is
	 * ok to ignore this. But if associated, we have do possible
	 * recovery here
	 */

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(dev, i);
		hdd_debug("Queue: %d status: %d txq->trans_start: %lu",
			  i, netif_tx_queue_stopped(txq), txq->trans_start);
	}

	hdd_debug("carrier state: %d", netif_carrier_ok(dev));

	wlan_hdd_display_adapter_netif_queue_history(adapter);

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_DP_ID);
	if (vdev) {
		ucfg_dp_tx_timeout(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_tx_timeout(struct net_device *net_dev, unsigned int txqueue)
#else
void hdd_tx_timeout(struct net_device *net_dev)
#endif
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return;

	__hdd_tx_timeout(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);
}

#ifdef RECEIVE_OFFLOAD
void hdd_disable_rx_ol_in_concurrency(bool disable)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return;

	ucfg_dp_rx_handle_concurrency(hdd_ctx->psoc, disable);
}
#else /* RECEIVE_OFFLOAD */
void hdd_disable_rx_ol_in_concurrency(bool disable)
{
}
#endif /* RECEIVE_OFFLOAD */

#ifdef WLAN_FEATURE_TSF_PLUS_SOCK_TS
void hdd_tsf_timestamp_rx(hdd_cb_handle ctx, qdf_nbuf_t netbuf)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(ctx);

	if (!hdd_tsf_is_rx_set(hdd_ctx))
		return;

	hdd_rx_timestamp(netbuf, ktime_to_us(netbuf->tstamp));
}

void hdd_get_tsf_time_cb(qdf_netdev_t netdev, uint64_t input_time,
			 uint64_t *tsf_time)
{
	struct hdd_adapter *adapter;

	adapter = WLAN_HDD_GET_PRIV_PTR(netdev);
	if (!adapter)
		return;

	hdd_get_tsf_time(adapter, input_time, tsf_time);
}
#endif

/**
 * hdd_reason_type_to_string() - return string conversion of reason type
 * @reason: reason type
 *
 * This utility function helps log string conversion of reason type.
 *
 * Return: string conversion of device mode, if match found;
 *        "Unknown" otherwise.
 */
const char *hdd_reason_type_to_string(enum netif_reason_type reason)
{
	switch (reason) {
	CASE_RETURN_STRING(WLAN_CONTROL_PATH);
	CASE_RETURN_STRING(WLAN_DATA_FLOW_CONTROL);
	CASE_RETURN_STRING(WLAN_FW_PAUSE);
	CASE_RETURN_STRING(WLAN_TX_ABORT);
	CASE_RETURN_STRING(WLAN_VDEV_STOP);
	CASE_RETURN_STRING(WLAN_PEER_UNAUTHORISED);
	CASE_RETURN_STRING(WLAN_THERMAL_MITIGATION);
	CASE_RETURN_STRING(WLAN_DATA_FLOW_CONTROL_PRIORITY);
	default:
		return "Invalid";
	}
}

/**
 * hdd_action_type_to_string() - return string conversion of action type
 * @action: action type
 *
 * This utility function helps log string conversion of action_type.
 *
 * Return: string conversion of device mode, if match found;
 *        "Unknown" otherwise.
 */
const char *hdd_action_type_to_string(enum netif_action_type action)
{

	switch (action) {
	CASE_RETURN_STRING(WLAN_STOP_ALL_NETIF_QUEUE);
	CASE_RETURN_STRING(WLAN_START_ALL_NETIF_QUEUE);
	CASE_RETURN_STRING(WLAN_WAKE_ALL_NETIF_QUEUE);
	CASE_RETURN_STRING(WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER);
	CASE_RETURN_STRING(WLAN_START_ALL_NETIF_QUEUE_N_CARRIER);
	CASE_RETURN_STRING(WLAN_NETIF_TX_DISABLE);
	CASE_RETURN_STRING(WLAN_NETIF_TX_DISABLE_N_CARRIER);
	CASE_RETURN_STRING(WLAN_NETIF_CARRIER_ON);
	CASE_RETURN_STRING(WLAN_NETIF_CARRIER_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_PRIORITY_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_PRIORITY_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_VO_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_VO_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_VI_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_VI_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_BE_BK_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_BE_BK_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_WAKE_NON_PRIORITY_QUEUE);
	CASE_RETURN_STRING(WLAN_STOP_NON_PRIORITY_QUEUE);
	default:
		return "Invalid";
	}
}

/**
 * wlan_hdd_update_queue_oper_stats - update queue operation statistics
 * @adapter: adapter handle
 * @action: action type
 * @reason: reason type
 */
static void wlan_hdd_update_queue_oper_stats(struct hdd_adapter *adapter,
	enum netif_action_type action, enum netif_reason_type reason)
{
	switch (action) {
	case WLAN_STOP_ALL_NETIF_QUEUE:
	case WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER:
	case WLAN_NETIF_BE_BK_QUEUE_OFF:
	case WLAN_NETIF_VI_QUEUE_OFF:
	case WLAN_NETIF_VO_QUEUE_OFF:
	case WLAN_NETIF_PRIORITY_QUEUE_OFF:
	case WLAN_STOP_NON_PRIORITY_QUEUE:
		adapter->queue_oper_stats[reason].pause_count++;
		break;
	case WLAN_START_ALL_NETIF_QUEUE:
	case WLAN_WAKE_ALL_NETIF_QUEUE:
	case WLAN_START_ALL_NETIF_QUEUE_N_CARRIER:
	case WLAN_NETIF_BE_BK_QUEUE_ON:
	case WLAN_NETIF_VI_QUEUE_ON:
	case WLAN_NETIF_VO_QUEUE_ON:
	case WLAN_NETIF_PRIORITY_QUEUE_ON:
	case WLAN_WAKE_NON_PRIORITY_QUEUE:
		adapter->queue_oper_stats[reason].unpause_count++;
		break;
	default:
		break;
	}
}

/**
 * hdd_netdev_queue_is_locked()
 * @txq: net device tx queue
 *
 * For SMP system, always return false and we could safely rely on
 * __netif_tx_trylock().
 *
 * Return: true locked; false not locked
 */
#ifdef QCA_CONFIG_SMP
static inline bool hdd_netdev_queue_is_locked(struct netdev_queue *txq)
{
	return false;
}
#else
static inline bool hdd_netdev_queue_is_locked(struct netdev_queue *txq)
{
	return txq->xmit_lock_owner != -1;
}
#endif

/**
 * wlan_hdd_update_txq_timestamp() - update txq timestamp
 * @dev: net device
 *
 * Return: none
 */
static void wlan_hdd_update_txq_timestamp(struct net_device *dev)
{
	struct netdev_queue *txq;
	int i;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(dev, i);

		/*
		 * On UP system, kernel will trigger watchdog bite if spinlock
		 * recursion is detected. Unfortunately recursion is possible
		 * when it is called in dev_queue_xmit() context, where stack
		 * grabs the lock before calling driver's ndo_start_xmit
		 * callback.
		 */
		if (!hdd_netdev_queue_is_locked(txq)) {
			if (__netif_tx_trylock(txq)) {
				txq_trans_update(txq);
				__netif_tx_unlock(txq);
			}
		}
	}
}

/**
 * wlan_hdd_update_unpause_time() - update unpause time
 * @adapter: adapter handle
 *
 * Return: none
 */
static void wlan_hdd_update_unpause_time(struct hdd_adapter *adapter)
{
	qdf_time_t curr_time = qdf_system_ticks();

	adapter->total_unpause_time += curr_time - adapter->last_time;
	adapter->last_time = curr_time;
}

/**
 * wlan_hdd_update_pause_time() - update pause time
 * @adapter: adapter handle
 * @temp_map: pause map
 *
 * Return: none
 */
static void wlan_hdd_update_pause_time(struct hdd_adapter *adapter,
				       uint32_t temp_map)
{
	qdf_time_t curr_time = qdf_system_ticks();
	uint8_t i;
	qdf_time_t pause_time;

	pause_time = curr_time - adapter->last_time;
	adapter->total_pause_time += pause_time;
	adapter->last_time = curr_time;

	for (i = 0; i < WLAN_REASON_TYPE_MAX; i++) {
		if (temp_map & (1 << i)) {
			adapter->queue_oper_stats[i].total_pause_time +=
								 pause_time;
			break;
		}
	}

}

uint32_t
wlan_hdd_dump_queue_history_state(struct hdd_netif_queue_history *queue_history,
				  char *buf, uint32_t size)
{
	unsigned int i;
	unsigned int index = 0;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		index += qdf_scnprintf(buf + index,
				       size - index,
				       "%u:0x%lx ",
				       i, queue_history->tx_q_state[i]);
	}

	return index;
}

/**
 * wlan_hdd_update_queue_history_state() - Save a copy of dev TX queues state
 * @dev: interface netdev
 * @q_hist: adapter queue history
 *
 * Save netdev TX queues state into adapter queue history.
 *
 * Return: None
 */
static void
wlan_hdd_update_queue_history_state(struct net_device *dev,
				    struct hdd_netif_queue_history *q_hist)
{
	unsigned int i = 0;
	uint32_t num_tx_queues = 0;
	struct netdev_queue *txq = NULL;

	num_tx_queues = qdf_min(dev->num_tx_queues, (uint32_t)NUM_TX_QUEUES);

	for (i = 0; i < num_tx_queues; i++) {
		txq = netdev_get_tx_queue(dev, i);
		q_hist->tx_q_state[i] = txq->state;
	}
}

/**
 * wlan_hdd_stop_non_priority_queue() - stop non priority queues
 * @adapter: adapter handle
 *
 * Return: None
 */
static inline void wlan_hdd_stop_non_priority_queue(struct hdd_adapter *adapter)
{
	uint8_t i;

	for (i = 0; i < TX_QUEUES_PER_AC; i++) {
		netif_stop_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_VO, i));
		netif_stop_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_VI, i));
		netif_stop_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_BE, i));
		netif_stop_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_BK, i));
	}
}

/**
 * wlan_hdd_wake_non_priority_queue() - wake non priority queues
 * @adapter: adapter handle
 *
 * Return: None
 */
static inline void wlan_hdd_wake_non_priority_queue(struct hdd_adapter *adapter)
{
	uint8_t i;

	for (i = 0; i < TX_QUEUES_PER_AC; i++) {
		netif_wake_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_VO, i));
		netif_wake_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_VI, i));
		netif_wake_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_BE, i));
		netif_wake_subqueue(adapter->dev,
				    TX_GET_QUEUE_IDX(HDD_LINUX_AC_BK, i));
	}
}

static inline
void hdd_wake_queues_for_ac(struct net_device *dev, enum hdd_wmm_linuxac ac)
{
	uint8_t i;

	for (i = 0; i < TX_QUEUES_PER_AC; i++)
		netif_wake_subqueue(dev, TX_GET_QUEUE_IDX(ac, i));
}

static inline
void hdd_stop_queues_for_ac(struct net_device *dev, enum hdd_wmm_linuxac ac)
{
	uint8_t i;

	for (i = 0; i < TX_QUEUES_PER_AC; i++)
		netif_stop_subqueue(dev, TX_GET_QUEUE_IDX(ac, i));
}

/**
 * wlan_hdd_netif_queue_control() - Use for netif_queue related actions
 * @adapter: adapter handle
 * @action: action type
 * @reason: reason type
 *
 * This is single function which is used for netif_queue related
 * actions like start/stop of network queues and on/off carrier
 * option.
 *
 * Return: None
 */
void wlan_hdd_netif_queue_control(struct hdd_adapter *adapter,
	enum netif_action_type action, enum netif_reason_type reason)
{
	uint32_t temp_map;
	uint8_t index;
	struct hdd_netif_queue_history *txq_hist_ptr;

	if ((!adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) ||
	    (!adapter->dev)) {
		hdd_err("adapter is invalid");
		return;
	}

	if (hdd_adapter_is_link_adapter(adapter))
		return;

	hdd_debug_rl("netif_control's vdev_id: %d, action: %d, reason: %d",
		     adapter->deflink->vdev_id, action, reason);

	switch (action) {

	case WLAN_NETIF_CARRIER_ON:
		netif_carrier_on(adapter->dev);
		break;

	case WLAN_NETIF_CARRIER_OFF:
		netif_carrier_off(adapter->dev);
		break;

	case WLAN_STOP_ALL_NETIF_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_tx_stop_all_queues(adapter->dev);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_STOP_NON_PRIORITY_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			wlan_hdd_stop_non_priority_queue(adapter);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_PRIORITY_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		if (reason == WLAN_DATA_FLOW_CTRL_PRI) {
			temp_map = adapter->subqueue_pause_map;
			adapter->subqueue_pause_map &= ~(1 << reason);
		} else {
			temp_map = adapter->pause_map;
			adapter->pause_map &= ~(1 << reason);
		}
		if (!adapter->pause_map) {
			netif_wake_subqueue(adapter->dev,
				HDD_LINUX_AC_HI_PRIO * TX_QUEUES_PER_AC);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_PRIORITY_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_stop_subqueue(adapter->dev,
				    HDD_LINUX_AC_HI_PRIO * TX_QUEUES_PER_AC);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		if (reason == WLAN_DATA_FLOW_CTRL_PRI)
			adapter->subqueue_pause_map |= (1 << reason);
		else
			adapter->pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_BE_BK_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			hdd_stop_queues_for_ac(adapter->dev, HDD_LINUX_AC_BK);
			hdd_stop_queues_for_ac(adapter->dev, HDD_LINUX_AC_BE);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->subqueue_pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_BE_BK_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->subqueue_pause_map;
		adapter->subqueue_pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			hdd_wake_queues_for_ac(adapter->dev, HDD_LINUX_AC_BK);
			hdd_wake_queues_for_ac(adapter->dev, HDD_LINUX_AC_BE);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VI_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			hdd_stop_queues_for_ac(adapter->dev, HDD_LINUX_AC_VI);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->subqueue_pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VI_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->subqueue_pause_map;
		adapter->subqueue_pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			hdd_wake_queues_for_ac(adapter->dev, HDD_LINUX_AC_VI);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VO_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			hdd_stop_queues_for_ac(adapter->dev, HDD_LINUX_AC_VO);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->subqueue_pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VO_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->subqueue_pause_map;
		adapter->subqueue_pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			hdd_wake_queues_for_ac(adapter->dev, HDD_LINUX_AC_VO);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_START_ALL_NETIF_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_tx_start_all_queues(adapter->dev);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_WAKE_ALL_NETIF_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_tx_wake_all_queues(adapter->dev);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_WAKE_NON_PRIORITY_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			wlan_hdd_wake_non_priority_queue(adapter);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_tx_stop_all_queues(adapter->dev);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->pause_map |= (1 << reason);
		netif_carrier_off(adapter->dev);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_START_ALL_NETIF_QUEUE_N_CARRIER:
		spin_lock_bh(&adapter->pause_map_lock);
		netif_carrier_on(adapter->dev);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_tx_start_all_queues(adapter->dev);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_ACTION_TYPE_NONE:
		break;

	default:
		hdd_err("unsupported action %d", action);
	}

	spin_lock_bh(&adapter->pause_map_lock);
	if (adapter->pause_map & (1 << WLAN_PEER_UNAUTHORISED))
		wlan_hdd_process_peer_unauthorised_pause(adapter);

	index = adapter->history_index++;
	if (adapter->history_index == WLAN_HDD_MAX_HISTORY_ENTRY)
		adapter->history_index = 0;
	spin_unlock_bh(&adapter->pause_map_lock);

	wlan_hdd_update_queue_oper_stats(adapter, action, reason);

	adapter->queue_oper_history[index].time = qdf_system_ticks();
	adapter->queue_oper_history[index].netif_action = action;
	adapter->queue_oper_history[index].netif_reason = reason;
	if (reason >= WLAN_DATA_FLOW_CTRL_BE_BK)
		adapter->queue_oper_history[index].pause_map =
			adapter->subqueue_pause_map;
	else
		adapter->queue_oper_history[index].pause_map =
			adapter->pause_map;

	txq_hist_ptr = &adapter->queue_oper_history[index];

	wlan_hdd_update_queue_history_state(adapter->dev, txq_hist_ptr);
}

void hdd_print_netdev_txq_status(struct net_device *dev)
{
	unsigned int i;

	if (!dev)
		return;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);

			hdd_debug("netdev tx queue[%u] state:0x%lx",
				  i, txq->state);
	}
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * hdd_set_mon_rx_cb() - Set Monitor mode Rx callback
 * @dev:        Pointer to net_device structure
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_set_mon_rx_cb(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx =  WLAN_HDD_GET_CTX(adapter);
	int ret;
	QDF_STATUS qdf_status;
	struct ol_txrx_desc_type sta_desc = {0};
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_objmgr_vdev *vdev;

	WLAN_ADDR_COPY(sta_desc.peer_addr.bytes, adapter->mac_addr.bytes);

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_DP_ID);
	if (!vdev) {
		hdd_err("failed to get vdev");
		return -EINVAL;
	}

	qdf_status = ucfg_dp_mon_register_txrx_ops(vdev);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("failed to register txrx ops. Status= %d [0x%08X]",
			qdf_status, qdf_status);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		goto exit;
	}
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

	/* peer is created wma_vdev_attach->wma_create_peer */
	qdf_status = cdp_peer_register(soc, OL_TXRX_PDEV_ID, &sta_desc);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("cdp_peer_register() failed to register. Status= %d [0x%08X]",
			qdf_status, qdf_status);
		goto exit;
	}

	qdf_status = sme_create_mon_session(hdd_ctx->mac_handle,
					    adapter->mac_addr.bytes,
					    adapter->deflink->vdev_id);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("sme_create_mon_session() failed to register. Status= %d [0x%08X]",
			qdf_status, qdf_status);
	}

exit:
	ret = qdf_status_to_os_return(qdf_status);
	return ret;
}
#endif

void hdd_tx_queue_cb(hdd_handle_t hdd_handle, uint32_t vdev_id,
		     enum netif_action_type action,
		     enum netif_reason_type reason)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	struct wlan_hdd_link_info *link_info;

	/*
	 * Validating the context is not required here.
	 * if there is a driver unload/SSR in progress happening in a
	 * different context and it has been scheduled to run and
	 * driver got a firmware event of sta kick out, then it is
	 * good to disable the Tx Queue to stop the influx of traffic.
	 */
	if (!hdd_ctx) {
		hdd_err("Invalid context passed");
		return;
	}

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, vdev_id);
	if (!link_info) {
		hdd_err("vdev_id %d does not exist with host", vdev_id);
		return;
	}
	hdd_debug("Tx Queue action %d on vdev %d", action, vdev_id);

	wlan_hdd_netif_queue_control(link_info->adapter, action, reason);
}

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_ini_tx_flow_control() - Initialize INIs concerned about tx flow control
 * @config: pointer to hdd config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void hdd_ini_tx_flow_control(struct hdd_config *config,
				    struct wlan_objmgr_psoc *psoc)
{
	config->tx_flow_low_watermark =
		cfg_get(psoc, CFG_DP_LL_TX_FLOW_LWM);
	config->tx_flow_hi_watermark_offset =
		cfg_get(psoc, CFG_DP_LL_TX_FLOW_HWM_OFFSET);
	config->tx_flow_max_queue_depth =
		cfg_get(psoc, CFG_DP_LL_TX_FLOW_MAX_Q_DEPTH);
	config->tx_lbw_flow_low_watermark =
		cfg_get(psoc, CFG_DP_LL_TX_LBW_FLOW_LWM);
	config->tx_lbw_flow_hi_watermark_offset =
		cfg_get(psoc, CFG_DP_LL_TX_LBW_FLOW_HWM_OFFSET);
	config->tx_lbw_flow_max_queue_depth =
		cfg_get(psoc, CFG_DP_LL_TX_LBW_FLOW_MAX_Q_DEPTH);
	config->tx_hbw_flow_low_watermark =
		cfg_get(psoc, CFG_DP_LL_TX_HBW_FLOW_LWM);
	config->tx_hbw_flow_hi_watermark_offset =
		cfg_get(psoc, CFG_DP_LL_TX_HBW_FLOW_HWM_OFFSET);
	config->tx_hbw_flow_max_queue_depth =
		cfg_get(psoc, CFG_DP_LL_TX_HBW_FLOW_MAX_Q_DEPTH);
}
#else
static void hdd_ini_tx_flow_control(struct hdd_config *config,
				    struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_FEATURE_MSCS
/**
 * hdd_ini_mscs_params() - Initialize INIs related to MSCS feature
 * @config: pointer to hdd config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void hdd_ini_mscs_params(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
	config->mscs_pkt_threshold =
		cfg_get(psoc, CFG_VO_PKT_COUNT_THRESHOLD);
	config->mscs_voice_interval =
		cfg_get(psoc, CFG_MSCS_VOICE_INTERVAL);
}

#else
static inline void hdd_ini_mscs_params(struct hdd_config *config,
				       struct wlan_objmgr_psoc *psoc)
{
}
#endif

void hdd_dp_cfg_update(struct wlan_objmgr_psoc *psoc,
		       struct hdd_context *hdd_ctx)
{
	struct hdd_config *config;

	config = hdd_ctx->config;

	config->napi_cpu_affinity_mask =
		cfg_get(psoc, CFG_DP_NAPI_CE_CPU_MASK);
	config->cfg_wmi_credit_cnt = cfg_get(psoc, CFG_DP_HTC_WMI_CREDIT_CNT);

	hdd_ini_tx_flow_control(config, psoc);
	hdd_ini_mscs_params(config, psoc);
}

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_set_tx_flow_info() - To set TX flow info
 * @adapter: pointer to adapter
 * @pre_adp_ctx: pointer to pre-adapter
 * @target_channel: target channel
 * @pre_adp_channel: pre-adapter channel
 * @dbgid: Debug IDs
 *
 * This routine is called to set TX flow info
 *
 * Return: None
 */
static void hdd_set_tx_flow_info(struct hdd_adapter *adapter,
				 struct hdd_adapter **pre_adp_ctx,
				 uint8_t target_channel,
				 uint8_t *pre_adp_channel,
				 wlan_net_dev_ref_dbgid dbgid)
{
	struct hdd_context *hdd_ctx;
	uint8_t channel24;
	uint8_t channel5;
	struct hdd_adapter *adapter2_4 = NULL;
	struct hdd_adapter *adapter5 = NULL;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	if (!target_channel)
		return;

	/*
	 * This is first adapter detected as active
	 * set as default for none concurrency case
	 */
	if (!(*pre_adp_channel)) {
		/* If IPA UC data path is enabled,
		 * target should reserve extra tx descriptors
		 * for IPA data path.
		 * Then host data path should allow less TX
		 * packet pumping in case IPA
		 * data path enabled
		 */
		if (ucfg_ipa_uc_is_enabled() &&
		    adapter->device_mode == QDF_SAP_MODE) {
			adapter->tx_flow_low_watermark =
				hdd_ctx->config->tx_flow_low_watermark +
				WLAN_TFC_IPAUC_TX_DESC_RESERVE;
		} else {
			adapter->tx_flow_low_watermark =
				hdd_ctx->config->tx_flow_low_watermark;
		}
		adapter->tx_flow_hi_watermark_offset =
			hdd_ctx->config->tx_flow_hi_watermark_offset;
		cdp_fc_ll_set_tx_pause_q_depth(soc,
				adapter->deflink->vdev_id,
				hdd_ctx->config->tx_flow_max_queue_depth);
		hdd_debug("MODE %d,CH %d,LWM %d,HWM %d,TXQDEP %d",
			  adapter->device_mode,
			  target_channel,
			  adapter->tx_flow_low_watermark,
			  adapter->tx_flow_low_watermark +
			  adapter->tx_flow_hi_watermark_offset,
			  hdd_ctx->config->tx_flow_max_queue_depth);
		*pre_adp_channel = target_channel;
		*pre_adp_ctx = adapter;
	} else {
		/*
		 * SCC, disable TX flow control for both
		 * SCC each adapter cannot reserve dedicated
		 * channel resource, as a result, if any adapter
		 * blocked OS Q by flow control,
		 * blocked adapter will lost chance to recover
		 */
		if (*pre_adp_channel == target_channel) {
			/* Current adapter */
			adapter->tx_flow_low_watermark = 0;
			adapter->tx_flow_hi_watermark_offset = 0;
			cdp_fc_ll_set_tx_pause_q_depth(soc,
				adapter->deflink->vdev_id,
				hdd_ctx->config->tx_hbw_flow_max_queue_depth);
			hdd_debug("SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
			          qdf_opmode_str(adapter->device_mode),
			          adapter->device_mode,
			          target_channel,
			          adapter->tx_flow_low_watermark,
			          adapter->tx_flow_low_watermark +
			          adapter->tx_flow_hi_watermark_offset,
			          hdd_ctx->config->tx_hbw_flow_max_queue_depth);

			if (!(*pre_adp_ctx)) {
				hdd_err("SCC: Previous adapter context NULL");
				hdd_adapter_dev_put_debug(adapter, dbgid);
				return;
			}

			/* Previous adapter */
			(*pre_adp_ctx)->tx_flow_low_watermark = 0;
			(*pre_adp_ctx)->tx_flow_hi_watermark_offset = 0;
			cdp_fc_ll_set_tx_pause_q_depth(soc,
				(*pre_adp_ctx)->deflink->vdev_id,
				hdd_ctx->config->tx_hbw_flow_max_queue_depth);
			hdd_debug("SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
				  qdf_opmode_str((*pre_adp_ctx)->device_mode),
				  (*pre_adp_ctx)->device_mode,
				  target_channel,
				  (*pre_adp_ctx)->tx_flow_low_watermark,
				  (*pre_adp_ctx)->tx_flow_low_watermark +
				  (*pre_adp_ctx)->tx_flow_hi_watermark_offset,
				 hdd_ctx->config->tx_hbw_flow_max_queue_depth);
		} else {
			/*
			 * MCC, each adapter will have dedicated
			 * resource
			 */
			/* current channel is 2.4 */
			if (target_channel <=
			    WLAN_HDD_TX_FLOW_CONTROL_MAX_24BAND_CH) {
				channel24 = target_channel;
				channel5 = *pre_adp_channel;
				adapter2_4 = adapter;
				adapter5 = *pre_adp_ctx;
			} else {
				/* Current channel is 5 */
				channel24 = *pre_adp_channel;
				channel5 = target_channel;
				adapter2_4 = *pre_adp_ctx;
				adapter5 = adapter;
			}

			if (!adapter5) {
				hdd_err("MCC: 5GHz adapter context NULL");
				hdd_adapter_dev_put_debug(adapter, dbgid);
				return;
			}
			adapter5->tx_flow_low_watermark =
				hdd_ctx->config->tx_hbw_flow_low_watermark;
			adapter5->tx_flow_hi_watermark_offset =
				hdd_ctx->config->tx_hbw_flow_hi_watermark_offset;
			cdp_fc_ll_set_tx_pause_q_depth(soc,
				adapter5->deflink->vdev_id,
				hdd_ctx->config->tx_hbw_flow_max_queue_depth);
			hdd_debug("MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
				  qdf_opmode_str(adapter5->device_mode),
				  adapter5->device_mode,
				  channel5,
				  adapter5->tx_flow_low_watermark,
				  adapter5->tx_flow_low_watermark +
				  adapter5->tx_flow_hi_watermark_offset,
				  hdd_ctx->config->tx_hbw_flow_max_queue_depth);

			if (!adapter2_4) {
				hdd_err("MCC: 2.4GHz adapter context NULL");
				hdd_adapter_dev_put_debug(adapter, dbgid);
				return;
			}
			adapter2_4->tx_flow_low_watermark =
				hdd_ctx->config->tx_lbw_flow_low_watermark;
			adapter2_4->tx_flow_hi_watermark_offset =
				hdd_ctx->config->tx_lbw_flow_hi_watermark_offset;
			cdp_fc_ll_set_tx_pause_q_depth(soc,
				adapter2_4->deflink->vdev_id,
				hdd_ctx->config->tx_lbw_flow_max_queue_depth);
			hdd_debug("MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
				  qdf_opmode_str(adapter2_4->device_mode),
				  adapter2_4->device_mode,
				  channel24,
				  adapter2_4->tx_flow_low_watermark,
				  adapter2_4->tx_flow_low_watermark +
				  adapter2_4->tx_flow_hi_watermark_offset,
				  hdd_ctx->config->tx_lbw_flow_max_queue_depth);
		}
	}
}

void wlan_hdd_set_tx_flow_info(void)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	struct hdd_ap_ctx *ap_ctx;
	struct hdd_hostapd_state *hostapd_state;
	uint8_t sta_chan = 0, ap_chan = 0;
	uint32_t chan_freq;
	struct hdd_context *hdd_ctx;
	uint8_t target_channel = 0;
	uint8_t pre_adp_channel = 0;
	struct hdd_adapter *pre_adp_ctx = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IPA_SET_TX_FLOW_INFO;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		switch (adapter->device_mode) {
		case QDF_STA_MODE:
		case QDF_P2P_CLIENT_MODE:
			sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter->deflink);
			if (hdd_cm_is_vdev_associated(adapter->deflink)) {
				chan_freq = sta_ctx->conn_info.chan_freq;
				sta_chan = wlan_reg_freq_to_chan(hdd_ctx->pdev,
								 chan_freq);
				target_channel = sta_chan;
			}
			break;
		case QDF_SAP_MODE:
		case QDF_P2P_GO_MODE:
			ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter->deflink);
			hostapd_state =
				WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter->deflink);
			if (hostapd_state->bss_state == BSS_START &&
			    hostapd_state->qdf_status == QDF_STATUS_SUCCESS) {
				chan_freq = ap_ctx->operating_chan_freq;
				ap_chan = wlan_reg_freq_to_chan(hdd_ctx->pdev,
								chan_freq);
				target_channel = ap_chan;
			}
			break;
		default:
			break;
		}

		hdd_set_tx_flow_info(adapter,
				     &pre_adp_ctx,
				     target_channel,
				     &pre_adp_channel,
				     dbgid);
		target_channel = 0;

		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

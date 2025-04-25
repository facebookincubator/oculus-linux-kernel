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

/**
 * DOC: wlan_hdd_ipa.c
 *
 * WLAN HDD and ipa interface implementation
 */

/* Include Files */
#include <wlan_hdd_includes.h>
#include <wlan_hdd_ipa.h>
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_ipa_ucfg_api.h"
#include <wlan_hdd_softap_tx_rx.h>
#include <linux/inetdevice.h>
#include <qdf_trace.h>
/* Test against msm kernel version */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) && \
	IS_ENABLED(CONFIG_SCHED_WALT)
#include <linux/sched/walt.h>
#endif
#include "wlan_hdd_object_manager.h"
#include "wlan_dp_ucfg_api.h"

#ifdef IPA_OFFLOAD

/**
 * struct hdd_ipa_connection_info - connectio info for IPA component
 * @vdev_id: vdev id
 * @ch_freq: channel frequency
 * @ch_width: channel width
 * @wlan_80211_mode: enum qca_wlan_802_11_mode
 */
struct hdd_ipa_connection_info {
	uint8_t vdev_id;
	qdf_freq_t ch_freq;
	enum phy_ch_width ch_width;
	enum qca_wlan_802_11_mode wlan_80211_mode;
};

#if (defined(QCA_CONFIG_SMP) && defined(PF_WAKE_UP_IDLE)) ||\
	IS_ENABLED(CONFIG_SCHED_WALT)
/**
 * hdd_ipa_get_wake_up_idle() - Get PF_WAKE_UP_IDLE flag in the task structure
 *
 * Get PF_WAKE_UP_IDLE flag in the task structure
 *
 * Return: 1 if PF_WAKE_UP_IDLE flag is set, 0 otherwise
 */
static uint32_t hdd_ipa_get_wake_up_idle(void)
{
	return sched_get_wake_up_idle(current);
}

/**
 * hdd_ipa_set_wake_up_idle() - Set PF_WAKE_UP_IDLE flag in the task structure
 * @wake_up_idle: Value to set PF_WAKE_UP_IDLE flag
 *
 * Set PF_WAKE_UP_IDLE flag in the task structure
 * This task and any task woken by this will be waken to idle CPU
 *
 * Return: None
 */
static void hdd_ipa_set_wake_up_idle(bool wake_up_idle)
{
	sched_set_wake_up_idle(current, wake_up_idle);
}
#else
static uint32_t hdd_ipa_get_wake_up_idle(void)
{
	return 0;
}

static void hdd_ipa_set_wake_up_idle(bool wake_up_idle)
{
}
#endif

#ifdef QCA_CONFIG_SMP
/**
 * hdd_ipa_send_to_nw_stack() - Check if IPA supports NAPI
 * polling during RX
 * @skb : data buffer sent to network stack
 *
 * If IPA LAN RX supports NAPI polling mechanism use
 * netif_receive_skb instead of netif_rx_ni to forward the skb
 * to network stack.
 *
 * Return: Return value from netif_rx_ni/netif_receive_skb
 */
static int hdd_ipa_send_to_nw_stack(qdf_nbuf_t skb)
{
	int result;

	if (qdf_ipa_get_lan_rx_napi())
		result = netif_receive_skb(skb);
	else
		result = netif_rx_ni(skb);
	return result;
}
#else
static int hdd_ipa_send_to_nw_stack(qdf_nbuf_t skb)
{
	int result;

	result = netif_rx_ni(skb);
	return result;
}
#endif

#ifdef QCA_CONFIG_SMP

/**
 * hdd_ipa_aggregated_rx_ind() - Submit aggregated packets to the stack
 * @skb: skb to be submitted to the stack
 *
 * For CONFIG_SMP systems, simply call netif_rx_ni.
 * For non CONFIG_SMP systems call netif_rx till
 * IPA_WLAN_RX_SOFTIRQ_THRESH. When threshold is reached call netif_rx_ni.
 * In this manner, UDP/TCP packets are sent in an aggregated way to the stack.
 * For IP/ICMP packets, simply call netif_rx_ni.
 *
 * Check if IPA supports NAPI polling then use netif_receive_skb
 * instead of netif_rx_ni.
 *
 * Return: return value from the netif_rx_ni/netif_rx api.
 */
static int hdd_ipa_aggregated_rx_ind(qdf_nbuf_t skb)
{
	int ret;

	ret =  hdd_ipa_send_to_nw_stack(skb);
	return ret;
}
#else
static int hdd_ipa_aggregated_rx_ind(qdf_nbuf_t skb)
{
	struct iphdr *ip_h;
	static atomic_t softirq_mitigation_cntr =
		ATOMIC_INIT(IPA_WLAN_RX_SOFTIRQ_THRESH);
	int result;

	ip_h = (struct iphdr *)(skb->data);
	if ((skb->protocol == htons(ETH_P_IP)) &&
		(ip_h->protocol == IPPROTO_ICMP)) {
		result = hdd_ipa_send_to_nw_stack(skb);
	} else {
		/* Call netif_rx_ni for every IPA_WLAN_RX_SOFTIRQ_THRESH packets
		 * to avoid excessive softirq's.
		 */
		if (atomic_dec_and_test(&softirq_mitigation_cntr)) {
			result = hdd_ipa_send_to_nw_stack(skb);
			atomic_set(&softirq_mitigation_cntr,
					IPA_WLAN_RX_SOFTIRQ_THRESH);
		} else {
			result = netif_rx(skb);
		}
	}

	return result;
}
#endif

void hdd_ipa_send_nbuf_to_network(qdf_nbuf_t nbuf, qdf_netdev_t dev)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) netdev_priv(dev);
	struct wlan_objmgr_vdev *vdev;
	int result;
	bool delivered = false;
	uint32_t enabled, len = 0;
	struct hdd_tx_rx_stats *stats;
	struct hdd_station_ctx *sta_ctx;
	bool is_eapol;
	u8 *ta_addr = NULL;

	if (hdd_validate_adapter(adapter)) {
		kfree_skb(nbuf);
		return;
	}

	if (cds_is_driver_unloading()) {
		kfree_skb(nbuf);
		return;
	}

	stats = &adapter->deflink->hdd_stats.tx_rx_stats;
	hdd_ipa_update_rx_mcbc_stats(adapter, nbuf);

	if ((adapter->device_mode == QDF_SAP_MODE) &&
	    (qdf_nbuf_is_ipv4_dhcp_pkt(nbuf) == true)) {
		/* Send DHCP Indication to FW */
		vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink,
						   WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_softap_inspect_dhcp_packet(vdev, nbuf, QDF_RX);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}
	}

	is_eapol = qdf_nbuf_is_ipv4_eapol_pkt(nbuf);

	qdf_dp_trace_set_track(nbuf, QDF_RX);

	ucfg_dp_event_eapol_log(nbuf, QDF_RX);
	qdf_dp_trace_log_pkt(adapter->deflink->vdev_id,
			     nbuf, QDF_RX, QDF_TRACE_DEFAULT_PDEV_ID,
			     adapter->device_mode);
	DPTRACE(qdf_dp_trace(nbuf,
			     QDF_DP_TRACE_RX_HDD_PACKET_PTR_RECORD,
			     QDF_TRACE_DEFAULT_PDEV_ID,
			     qdf_nbuf_data_addr(nbuf),
			     sizeof(qdf_nbuf_data(nbuf)), QDF_RX));
	DPTRACE(qdf_dp_trace_data_pkt(nbuf, QDF_TRACE_DEFAULT_PDEV_ID,
				      QDF_DP_TRACE_RX_PACKET_RECORD, 0,
				      QDF_RX));

	/*
	 * Set PF_WAKE_UP_IDLE flag in the task structure
	 * This task and any task woken by this will be waken to idle CPU
	 */
	enabled = hdd_ipa_get_wake_up_idle();
	if (!enabled)
		hdd_ipa_set_wake_up_idle(true);

	nbuf->dev = adapter->dev;
	nbuf->protocol = eth_type_trans(nbuf, nbuf->dev);
	nbuf->ip_summed = CHECKSUM_NONE;
	len = nbuf->len;

	/*
	 * Update STA RX exception packet stats.
	 * For SAP as part of IPA HW stats are updated.
	 */

	if (is_eapol && SEND_EAPOL_OVER_NL) {
		if (adapter->device_mode == QDF_SAP_MODE) {
			ta_addr = adapter->mac_addr.bytes;
		} else if (adapter->device_mode == QDF_STA_MODE) {
			sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter->deflink);
			ta_addr = (u8 *)&sta_ctx->conn_info.peer_macaddr;
		}

		if (ta_addr) {
			if (wlan_hdd_cfg80211_rx_control_port(adapter->dev,
							      ta_addr, nbuf,
							      false))
				result = NET_RX_SUCCESS;
			else
				result = NET_RX_DROP;
		} else {
			result = NET_RX_DROP;
		}

		dev_kfree_skb(nbuf);
	} else {
		result = hdd_ipa_aggregated_rx_ind(nbuf);
	}

	if (result == NET_RX_SUCCESS)
		delivered = true;
	/*
	 * adapter->vdev is directly dereferenced because this is per packet
	 * path, hdd_get_vdev_by_user() usage will be very costly as it involves
	 * lock access.
	 * Expectation here is vdev will be present during TX/RX processing
	 * and also DP internally maintaining vdev ref count
	 */
	ucfg_dp_inc_rx_pkt_stats(adapter->deflink->vdev,
				 len, delivered);
	/*
	 * Restore PF_WAKE_UP_IDLE flag in the task structure
	 */
	if (!enabled)
		hdd_ipa_set_wake_up_idle(false);
}

void hdd_ipa_set_mcc_mode(bool mcc_mode)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	ucfg_ipa_set_mcc_mode(hdd_ctx->pdev, mcc_mode);
}

#ifdef IPA_WDI3_TX_TWO_PIPES
static void
hdd_ipa_fill_sta_connection_info(struct wlan_hdd_link_info *link,
				 struct hdd_ipa_connection_info *conn)
{
	struct hdd_station_ctx *ctx = WLAN_HDD_GET_STATION_CTX_PTR(link);

	conn->ch_freq = ctx->conn_info.chan_freq;
	conn->ch_width = ctx->conn_info.ch_width;
	conn->wlan_80211_mode = hdd_convert_cfgdot11mode_to_80211mode(
			ctx->conn_info.dot11mode);
}

static void
hdd_ipa_fill_sap_connection_info(struct wlan_hdd_link_info *link,
				 struct hdd_ipa_connection_info *conn)
{
	struct hdd_ap_ctx *ctx = WLAN_HDD_GET_AP_CTX_PTR(link);

	conn->ch_freq = ctx->operating_chan_freq;
	conn->ch_width = ctx->sap_config.ch_params.ch_width;
	conn->wlan_80211_mode = hdd_convert_phymode_to_80211mode(
			ctx->sap_config.SapHw_mode);
}

static void hdd_ipa_fill_connection_info(struct wlan_hdd_link_info *link,
					 struct hdd_ipa_connection_info *conn)
{
	struct hdd_adapter *adapter = link->adapter;

	conn->vdev_id = link->vdev_id;

	if (adapter->device_mode == QDF_STA_MODE)
		hdd_ipa_fill_sta_connection_info(link, conn);
	else if (adapter->device_mode == QDF_SAP_MODE)
		hdd_ipa_fill_sap_connection_info(link, conn);
}

static QDF_STATUS
hdd_ipa_get_tx_pipe_multi_conn(struct hdd_context *hdd_ctx,
			       struct hdd_ipa_connection_info *conn,
			       bool *tx_pipe)
{
	uint32_t new_freq = conn->ch_freq;
	QDF_STATUS status;
	uint8_t vdev_id;
	bool pipe;

	if (ucfg_policy_mgr_get_vdev_same_freq_new_conn(hdd_ctx->psoc,
							new_freq,
							&vdev_id)) {
		/* Inherit the pipe selection of the connection that has
		 * same freq.
		 */
		return ucfg_ipa_get_alt_pipe(hdd_ctx->pdev, vdev_id, tx_pipe);
	} else {
		if (ucfg_policy_mgr_get_vdev_diff_freq_new_conn(hdd_ctx->psoc,
								new_freq,
								&vdev_id)) {
			status = ucfg_ipa_get_alt_pipe(hdd_ctx->pdev, vdev_id,
						       &pipe);
			if (QDF_IS_STATUS_ERROR(status))
				return QDF_STATUS_E_INVAL;

			/* Inverse the pipe selection of the connection that
			 * has different channel frequency.
			 */
			*tx_pipe = !pipe;
			return QDF_STATUS_SUCCESS;
		} else {
			return QDF_STATUS_E_INVAL;
		}
	}
}

QDF_STATUS hdd_ipa_get_tx_pipe(struct hdd_context *hdd_ctx,
			       struct wlan_hdd_link_info *link,
			       bool *tx_pipe)
{
	struct hdd_ipa_connection_info conn;
	uint32_t count;

	if (qdf_unlikely(!hdd_ctx || !link || !tx_pipe)) {
		hdd_debug("Invalid parameters");
		return QDF_STATUS_E_INVAL;
	}

	/* If SBS not capable, use legacy DBS selection */
	if (!ucfg_policy_mgr_is_hw_sbs_capable(hdd_ctx->psoc)) {
		hdd_debug("firmware is not sbs capable");
		*tx_pipe = WLAN_REG_IS_24GHZ_CH_FREQ(conn.ch_freq);
		return QDF_STATUS_SUCCESS;
	}

	hdd_ipa_fill_connection_info(link, &conn);

	/* Always select the primary pipe for connection that is EHT160 or
	 * EHT320 due to higher tput requiements.
	 */
	if (conn.wlan_80211_mode == QCA_WLAN_802_11_MODE_11BE &&
	    (conn.ch_width == CH_WIDTH_160MHZ ||
	     conn.ch_width == CH_WIDTH_320MHZ)) {
		*tx_pipe = false;
		return QDF_STATUS_SUCCESS;
	}

	count = ucfg_policy_mgr_get_connection_count(hdd_ctx->psoc);
	if (!count) {
		/* For first connection that is below EHT160, select the
		 * alternate pipe so as to reserve the primary pipe for
		 * potential connections that are above EHT160.
		 */
		*tx_pipe = true;
		return QDF_STATUS_SUCCESS;
	}

	return hdd_ipa_get_tx_pipe_multi_conn(hdd_ctx, &conn, tx_pipe);
}
#else /* !IPA_WDI3_TX_TWO_PIPES */
QDF_STATUS hdd_ipa_get_tx_pipe(struct hdd_context *hdd_ctx,
			       struct wlan_hdd_link_info *link,
			       bool *tx_pipe)
{
	if (qdf_unlikely(!tx_pipe))
		return QDF_STATUS_E_INVAL;

	/* For IPA_WDI3_TX_TWO_PIPES=n, only one tx pipe is available */
	*tx_pipe = false;

	return QDF_STATUS_SUCCESS;
}
#endif /* IPA_WDI3_TX_TWO_PIPES */

void hdd_ipa_set_perf_level_bw(enum hw_mode_bandwidth bw)
{
	struct hdd_context *hdd_ctx;
	enum wlan_ipa_bw_level lvl;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	if (bw == HW_MODE_320_MHZ)
		lvl = WLAN_IPA_BW_LEVEL_HIGH;
	else if (bw == HW_MODE_160_MHZ)
		lvl = WLAN_IPA_BW_LEVEL_MEDIUM;
	else
		lvl = WLAN_IPA_BW_LEVEL_LOW;

	hdd_debug("Vote IPA perf level to %d", lvl);
	ucfg_ipa_set_perf_level_bw(hdd_ctx->pdev, lvl);
}

#endif

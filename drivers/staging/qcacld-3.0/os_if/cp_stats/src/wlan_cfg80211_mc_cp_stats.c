/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_cfg80211_mc_cp_stats.c
 *
 * This file provide definitions to cp stats supported cfg80211 cmd handlers
 */

#include <wlan_cfg80211.h>
#include <wlan_cp_stats_ucfg_api.h>
#include <wlan_cp_stats_mc_defs.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_cfg80211_mc_cp_stats.h>
#include "wlan_osif_request_manager.h"
#include "wlan_objmgr_peer_obj.h"
#include "wlan_mlme_twt_ucfg_api.h"
#include "cds_utils.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_stats.h"


/* max time in ms, caller may wait for stats request get serviced */
#define CP_STATS_WAIT_TIME_STAT 800

#ifdef WLAN_FEATURE_MIB_STATS
/**
 * wlan_free_mib_stats() - free allocations for mib stats
 * @stats: Pointer to stats event statucture
 *
 * Return: None
 */
static void wlan_free_mib_stats(struct stats_event *stats)
{
	qdf_mem_free(stats->mib_stats);
	stats->mib_stats = NULL;
}
#else
static void wlan_free_mib_stats(struct stats_event *stats)
{
}
#endif

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS
#ifdef WLAN_SUPPORT_TWT
static void wlan_cfg80211_infra_cp_stats_twt_dealloc(void *priv)
{
	struct infra_cp_stats_event *stats = priv;

	qdf_mem_free(stats->twt_infra_cp_stats);
	stats->twt_infra_cp_stats = NULL;
}
#else
static void wlan_cfg80211_infra_cp_stats_twt_dealloc(void *priv)
{
}
#endif /* WLAN_SUPPORT_TWT */

#ifdef CONFIG_WLAN_BMISS
static void wlan_cfg80211_infra_cp_stats_bmiss_dealloc(void *priv)
{
	struct infra_cp_stats_event *stats = priv;

	qdf_mem_free(stats->bmiss_infra_cp_stats);
	stats->bmiss_infra_cp_stats = NULL;
}
#else /* CONFIG_WLAN_BMISS */
static void wlan_cfg80211_infra_cp_stats_bmiss_dealloc(void *priv)
{
}
#endif /* CONFIG_WLAN_BMISS */
/**
 * wlan_cfg80211_mc_infra_cp_stats_dealloc() - callback to free priv
 * allocations for infra cp stats
 * @priv: Pointer to priv data statucture
 *
 * Return: None
 */
static inline
void wlan_cfg80211_mc_infra_cp_stats_dealloc(void *priv)
{
	struct infra_cp_stats_event *stats = priv;

	if (!stats) {
		osif_err("infar_cp_stats is NULL");
		return;
	}
	wlan_cfg80211_infra_cp_stats_twt_dealloc(priv);
	wlan_cfg80211_infra_cp_stats_bmiss_dealloc(priv);
}
#endif /* WLAN_SUPPORT_INFRA_CTRL_PATH_STATS */

/**
 * wlan_cfg80211_mc_cp_stats_free_peer_stats_info_ext() - API to free peer stats
 * info ext structure
 * @ev: structure from where peer stats info ext needs to be freed
 *
 * Return: none
 */
static void wlan_cfg80211_mc_cp_stats_free_peer_stats_info_ext(
							struct stats_event *ev)
{
	struct peer_stats_info_ext_event *peer_stats_info =
							ev->peer_stats_info_ext;
	uint16_t i;

	if (!ev->peer_stats_info_ext) {
		ev->num_peer_stats_info_ext = 0;
		return;
	}
	for (i = 0; i < ev->num_peer_stats_info_ext; i++) {
		qdf_mem_free(peer_stats_info->tx_pkt_per_mcs);
		peer_stats_info->tx_pkt_per_mcs = NULL;
		qdf_mem_free(peer_stats_info->rx_pkt_per_mcs);
		peer_stats_info->tx_pkt_per_mcs = NULL;
		peer_stats_info++;
	}

	qdf_mem_free(ev->peer_stats_info_ext);
	ev->peer_stats_info_ext = NULL;
	ev->num_peer_stats_info_ext = 0;
}

/**
 * wlan_cfg80211_mc_cp_stats_dealloc() - callback to free priv
 * allocations for stats
 * @priv: Pointer to priv data statucture
 *
 * Return: None
 */
static void wlan_cfg80211_mc_cp_stats_dealloc(void *priv)
{
	struct stats_event *stats = priv;

	if (!stats) {
		osif_err("stats is NULL");
		return;
	}

	qdf_mem_free(stats->pdev_stats);
	qdf_mem_free(stats->pdev_extd_stats);
	qdf_mem_free(stats->peer_stats);
	qdf_mem_free(stats->cca_stats);
	qdf_mem_free(stats->vdev_summary_stats);
	qdf_mem_free(stats->vdev_chain_rssi);
	qdf_mem_free(stats->peer_adv_stats);
	wlan_cfg80211_mc_cp_stats_free_peer_stats_info_ext(stats);
	wlan_free_mib_stats(stats);
}

#define QCA_WLAN_VENDOR_ATTR_TOTAL_DRIVER_FW_LOCAL_WAKE \
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_TOTAL_DRIVER_FW_LOCAL_WAKE
#define QCA_WLAN_VENDOR_ATTR_DRIVER_FW_LOCAL_WAKE_CNT_PTR \
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_PTR
#define QCA_WLAN_VENDOR_ATTR_DRIVER_FW_LOCAL_WAKE_CNT_SZ \
	QCA_WLAN_VENDOR_ATTR_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_SZ

/**
 * wlan_cfg80211_mc_cp_stats_send_wake_lock_stats() - API to send wakelock stats
 * @wiphy: wiphy pointer
 * @stats: stats data to be sent
 *
 * Return: 0 on success, error number otherwise.
 */
static int wlan_cfg80211_mc_cp_stats_send_wake_lock_stats(struct wiphy *wiphy,
						struct wake_lock_stats *stats)
{
	struct sk_buff *skb;
	uint32_t nl_buf_len;
	uint32_t icmpv6_cnt;
	uint32_t ipv6_rx_multicast_addr_cnt;
	uint32_t total_rx_data_wake, rx_multicast_cnt;

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += QCA_WLAN_VENDOR_GET_WAKE_STATS_MAX *
				(NLMSG_HDRLEN + sizeof(uint32_t));

	skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy, nl_buf_len);

	if (!skb) {
		osif_err("wlan_cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	osif_debug("wow_ucast_wake_up_count %d",
		   stats->ucast_wake_up_count);
	osif_debug("wow_bcast_wake_up_count %d",
		   stats->bcast_wake_up_count);
	osif_debug("wow_ipv4_mcast_wake_up_count %d",
		   stats->ipv4_mcast_wake_up_count);
	osif_debug("wow_ipv6_mcast_wake_up_count %d",
		   stats->ipv6_mcast_wake_up_count);
	osif_debug("wow_ipv6_mcast_ra_stats %d",
		   stats->ipv6_mcast_ra_stats);
	osif_debug("wow_ipv6_mcast_ns_stats %d",
		   stats->ipv6_mcast_ns_stats);
	osif_debug("wow_ipv6_mcast_na_stats %d",
		   stats->ipv6_mcast_na_stats);
	osif_debug("wow_icmpv4_count %d",
		   stats->icmpv4_count);
	osif_debug("wow_icmpv6_count %d",
		   stats->icmpv6_count);
	osif_debug("wow_rssi_breach_wake_up_count %d",
		   stats->rssi_breach_wake_up_count);
	osif_debug("wow_low_rssi_wake_up_count %d",
		   stats->low_rssi_wake_up_count);
	osif_debug("wow_gscan_wake_up_count %d",
		   stats->gscan_wake_up_count);
	osif_debug("wow_pno_complete_wake_up_count %d",
		   stats->pno_complete_wake_up_count);
	osif_debug("wow_pno_match_wake_up_count %d",
		   stats->pno_match_wake_up_count);

	ipv6_rx_multicast_addr_cnt = stats->ipv6_mcast_wake_up_count;
	icmpv6_cnt = stats->icmpv6_count;
	rx_multicast_cnt = stats->ipv4_mcast_wake_up_count +
						ipv6_rx_multicast_addr_cnt;
	total_rx_data_wake = stats->ucast_wake_up_count +
			stats->bcast_wake_up_count + rx_multicast_cnt;

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_TOTAL_CMD_EVENT_WAKE,
			0) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_CMD_EVENT_WAKE_CNT_PTR,
			0) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_CMD_EVENT_WAKE_CNT_SZ,
			0) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TOTAL_DRIVER_FW_LOCAL_WAKE,
			0) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_DRIVER_FW_LOCAL_WAKE_CNT_PTR,
			0) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_DRIVER_FW_LOCAL_WAKE_CNT_SZ,
			0) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_TOTAL_RX_DATA_WAKE,
			total_rx_data_wake) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RX_UNICAST_CNT,
			stats->ucast_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RX_MULTICAST_CNT,
			rx_multicast_cnt) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RX_BROADCAST_CNT,
			stats->bcast_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP_PKT,
			stats->icmpv4_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_PKT,
			icmpv6_cnt) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_RA,
			stats->ipv6_mcast_ra_stats) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_NA,
			stats->ipv6_mcast_na_stats) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_NS,
			stats->ipv6_mcast_ns_stats) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP4_RX_MULTICAST_CNT,
			stats->ipv4_mcast_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_ICMP6_RX_MULTICAST_CNT,
			ipv6_rx_multicast_addr_cnt) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_RSSI_BREACH_CNT,
			stats->rssi_breach_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_LOW_RSSI_CNT,
			stats->low_rssi_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_GSCAN_CNT,
			stats->gscan_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_PNO_COMPLETE_CNT,
			stats->pno_complete_wake_up_count) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_WAKE_STATS_PNO_MATCH_CNT,
			stats->pno_match_wake_up_count)) {
		osif_err("nla put fail");
		goto nla_put_failure;
	}

	wlan_cfg80211_vendor_cmd_reply(skb);
	return 0;

nla_put_failure:
	wlan_cfg80211_vendor_free_skb(skb);
	return -EINVAL;
}

#undef QCA_WLAN_VENDOR_ATTR_TOTAL_DRIVER_FW_LOCAL_WAKE
#undef QCA_WLAN_VENDOR_ATTR_DRIVER_FW_LOCAL_WAKE_CNT_PTR
#undef QCA_WLAN_VENDOR_ATTR_DRIVER_FW_LOCAL_WAKE_CNT_SZ

int wlan_cfg80211_mc_cp_stats_get_wakelock_stats(struct wlan_objmgr_psoc *psoc,
						 struct wiphy *wiphy)
{
	/* refer __wlan_hdd_cfg80211_get_wakelock_stats */
	QDF_STATUS status;
	struct wake_lock_stats stats = {0};

	status = ucfg_mc_cp_stats_get_psoc_wake_lock_stats(psoc, &stats);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	return wlan_cfg80211_mc_cp_stats_send_wake_lock_stats(wiphy, &stats);
}

struct tx_power_priv {
	int dbm;
};

/**
 * get_tx_power_cb() - "Get tx power" callback function
 * @tx_power: tx_power
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void get_tx_power_cb(int tx_power, void *cookie)
{
	struct osif_request *request;
	struct tx_power_priv *priv;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->dbm = tx_power;
	osif_request_complete(request);
	osif_request_put(request);
}

int wlan_cfg80211_mc_cp_stats_get_tx_power(struct wlan_objmgr_vdev *vdev,
					   int *dbm)
{
	int ret = 0;
	void *cookie;
	QDF_STATUS status;
	struct request_info info = {0};
	struct wlan_objmgr_peer *peer;
	struct tx_power_priv *priv = NULL;
	struct osif_request *request = NULL;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = CP_STATS_WAIT_TIME_STAT,
	};

	request = osif_request_alloc(&params);
	if (!request) {
		osif_err("Request allocation failure, return cached value");
		goto fetch_tx_power;
	}

	cookie = osif_request_cookie(request);
	info.cookie = cookie;
	info.u.get_tx_power_cb = get_tx_power_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_CP_STATS_ID);
	if (!peer) {
		ret = -EINVAL;
		goto peer_is_null;
	}
	qdf_mem_copy(info.peer_mac_addr, peer->macaddr, QDF_MAC_ADDR_SIZE);

	wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	status = ucfg_mc_cp_stats_send_stats_request(vdev,
						     TYPE_CONNECTION_TX_POWER,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("wlan_mc_cp_stats_request_tx_power status: %d",
			 status);
		ret = qdf_status_to_os_return(status);
	} else {
		ret = osif_request_wait_for_response(request);
		if (ret)
			osif_err("wait failed or timed out ret: %d", ret);
		else
			priv = osif_request_priv(request);
	}

fetch_tx_power:
	if (priv) {
		*dbm = priv->dbm;
	} else {
		status = ucfg_mc_cp_stats_get_tx_power(vdev, dbm);
		if (QDF_IS_STATUS_ERROR(status)) {
			osif_err("ucfg_mc_cp_stats_get_tx_power status: %d",
				 status);
			ret = qdf_status_to_os_return(status);
		}
	}

peer_is_null:
	/*
	 * either we never sent a request, we sent a request and
	 * received a response or we sent a request and timed out.
	 * regardless we are done with the request.
	 */
	if (request)
		osif_request_put(request);

	return ret;
}

/**
 * get_peer_rssi_cb() - get_peer_rssi_cb callback function
 * @ev: peer stats buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void get_peer_rssi_cb(struct stats_event *ev, void *cookie)
{
	struct stats_event *priv;
	struct osif_request *request;
	uint32_t rssi_size;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	if (!ev->peer_stats) {
		osif_err("no peer stats");
		goto get_peer_rssi_cb_fail;
	}

	priv = osif_request_priv(request);
	rssi_size = sizeof(*ev->peer_stats) * ev->num_peer_stats;
	if (rssi_size == 0) {
		osif_err("Invalid rssi stats");
		goto get_peer_rssi_cb_fail;
	}

	priv->peer_stats = qdf_mem_malloc(rssi_size);
	if (!priv->peer_stats)
		goto get_peer_rssi_cb_fail;

	priv->num_peer_stats = ev->num_peer_stats;
	qdf_mem_copy(priv->peer_stats, ev->peer_stats, rssi_size);

get_peer_rssi_cb_fail:
	osif_request_complete(request);
	osif_request_put(request);
}

struct stats_event *
wlan_cfg80211_mc_cp_stats_get_peer_rssi(struct wlan_objmgr_vdev *vdev,
					uint8_t *mac_addr,
					int *errno)
{
	void *cookie;
	QDF_STATUS status;
	struct stats_event *priv, *out;
	struct request_info info = {0};
	struct osif_request *request = NULL;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_cp_stats_dealloc,
	};

	out = qdf_mem_malloc(sizeof(*out));
	if (!out) {
		*errno = -ENOMEM;
		return NULL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		osif_err("Request allocation failure, return cached value");
		*errno = -ENOMEM;
		qdf_mem_free(out);
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);
	info.cookie = cookie;
	info.u.get_peer_rssi_cb = get_peer_rssi_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	qdf_mem_copy(info.peer_mac_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	status = ucfg_mc_cp_stats_send_stats_request(vdev, TYPE_PEER_STATS,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("stats req failed: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_peer_rssi_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_debug("wait failed or timed out ret: %d", *errno);
		goto get_peer_rssi_fail;
	}

	if (!priv->peer_stats || priv->num_peer_stats == 0) {
		osif_err("Invalid peer stats, count %d, data %pK",
			 priv->num_peer_stats, priv->peer_stats);
		*errno = -EINVAL;
		goto get_peer_rssi_fail;
	}
	out->num_peer_stats = priv->num_peer_stats;
	out->peer_stats = priv->peer_stats;
	priv->peer_stats = NULL;
	osif_request_put(request);

	return out;

get_peer_rssi_fail:
	osif_request_put(request);
	wlan_cfg80211_mc_cp_stats_free_stats_event(out);

	return NULL;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
static void get_big_data_stats_cb(struct big_data_stats_event *ev, void *cookie)
{
	struct big_data_stats_event *priv;
	struct osif_request *request;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->tsf_out_of_sync = ev->tsf_out_of_sync;
	priv->vdev_id = ev->vdev_id;
	priv->ani_level = ev->ani_level;

	priv->last_data_tx_pwr = ev->last_data_tx_pwr;
	priv->target_power_dsss = ev->target_power_dsss;
	priv->target_power_ofdm = ev->target_power_ofdm;
	priv->last_tx_data_rix = ev->last_tx_data_rix;
	priv->last_tx_data_rate_kbps = ev->last_tx_data_rate_kbps;

	osif_request_complete(request);
	osif_request_put(request);
}
#endif

/**
 * get_station_stats_cb() - get_station_stats_cb callback function
 * @ev: station stats buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void get_station_stats_cb(struct stats_event *ev, void *cookie)
{
	struct stats_event *priv;
	struct osif_request *request;
	uint32_t summary_size, rssi_size, peer_adv_size = 0;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);

	if (!ev->vdev_summary_stats || !ev->vdev_chain_rssi) {
		osif_debug("Invalid stats");
		goto station_stats_cb_fail;
	}

	summary_size = sizeof(*ev->vdev_summary_stats) * ev->num_summary_stats;
	rssi_size = sizeof(*ev->vdev_chain_rssi) * ev->num_chain_rssi_stats;
	if (ev->peer_adv_stats && ev->num_peer_adv_stats)
		peer_adv_size =
			sizeof(*ev->peer_adv_stats) * ev->num_peer_adv_stats;

	if (summary_size == 0 || rssi_size == 0) {
		osif_err("Invalid stats, summary %d rssi %d",
			 summary_size, rssi_size);
		goto station_stats_cb_fail;
	}
	if (priv->vdev_summary_stats || priv->vdev_chain_rssi ||
	    priv->peer_adv_stats) {
		osif_err("invalid context cookie %pK request %pK",
			 cookie, request);
		goto station_stats_cb_fail;
	}

	priv->vdev_summary_stats = qdf_mem_malloc(summary_size);
	if (!priv->vdev_summary_stats)
		goto station_stats_cb_fail;

	priv->vdev_chain_rssi = qdf_mem_malloc(rssi_size);
	if (!priv->vdev_chain_rssi)
		goto station_stats_cb_fail;

	if (peer_adv_size) {
		priv->peer_adv_stats = qdf_mem_malloc(peer_adv_size);
		if (!priv->peer_adv_stats)
			goto station_stats_cb_fail;

		qdf_mem_copy(priv->peer_adv_stats, ev->peer_adv_stats,
			     peer_adv_size);
	}

	priv->num_summary_stats = ev->num_summary_stats;
	priv->num_chain_rssi_stats = ev->num_chain_rssi_stats;
	priv->tx_rate = ev->tx_rate;
	priv->rx_rate = ev->rx_rate;
	priv->tx_rate_flags = ev->tx_rate_flags;
	priv->num_peer_adv_stats = ev->num_peer_adv_stats;
	qdf_mem_copy(priv->vdev_chain_rssi, ev->vdev_chain_rssi, rssi_size);
	qdf_mem_copy(priv->vdev_summary_stats, ev->vdev_summary_stats,
		     summary_size);
	priv->bcn_protect_stats = ev->bcn_protect_stats;

station_stats_cb_fail:
	osif_request_complete(request);
	osif_request_put(request);
}

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS

#ifdef WLAN_SUPPORT_TWT
static void get_twt_infra_cp_stats(struct infra_cp_stats_event *ev,
				   struct infra_cp_stats_event *priv)

{
	priv->num_twt_infra_cp_stats = ev->num_twt_infra_cp_stats;
	priv->twt_infra_cp_stats->dialog_id = ev->twt_infra_cp_stats->dialog_id;
	priv->twt_infra_cp_stats->status = ev->twt_infra_cp_stats->status;
	priv->twt_infra_cp_stats->num_sp_cycles =
					ev->twt_infra_cp_stats->num_sp_cycles;
	priv->twt_infra_cp_stats->avg_sp_dur_us =
					ev->twt_infra_cp_stats->avg_sp_dur_us;
	priv->twt_infra_cp_stats->min_sp_dur_us =
					ev->twt_infra_cp_stats->min_sp_dur_us;
	priv->twt_infra_cp_stats->max_sp_dur_us =
					ev->twt_infra_cp_stats->max_sp_dur_us;
	priv->twt_infra_cp_stats->tx_mpdu_per_sp =
					ev->twt_infra_cp_stats->tx_mpdu_per_sp;
	priv->twt_infra_cp_stats->rx_mpdu_per_sp =
				ev->twt_infra_cp_stats->rx_mpdu_per_sp;
	priv->twt_infra_cp_stats->tx_bytes_per_sp =
				ev->twt_infra_cp_stats->tx_bytes_per_sp;
	priv->twt_infra_cp_stats->rx_bytes_per_sp =
				ev->twt_infra_cp_stats->rx_bytes_per_sp;
}

static void
wlan_cfg80211_mc_infra_cp_free_twt_stats(struct infra_cp_stats_event *stats)
{
	qdf_mem_free(stats->twt_infra_cp_stats);
}
#else
static void get_twt_infra_cp_stats(struct infra_cp_stats_event *ev,
				   struct infra_cp_stats_event *priv)
{
}

static void
wlan_cfg80211_mc_infra_cp_free_twt_stats(struct infra_cp_stats_event *stats)
{
}
#endif /* WLAN_SUPPORT_TWT */

#ifdef CONFIG_WLAN_BMISS
static void
wlan_cfg80211_mc_infra_cp_free_bmiss_stats(struct infra_cp_stats_event *stats)
{
	if (stats->bmiss_infra_cp_stats) {
		qdf_mem_free(stats->bmiss_infra_cp_stats);
		stats->bmiss_infra_cp_stats = NULL;
	}
}
#else /* CONFIG_WLAN_BMISS */
static void
wlan_cfg80211_mc_infra_cp_free_bmiss_stats(struct infra_cp_stats_event *stats)
{
}
#endif/* CONFIG_WLAN_BMISS */
static inline void
wlan_cfg80211_mc_infra_cp_stats_free_stats_event(
					struct infra_cp_stats_event *stats)
{
	if (!stats)
		return;
	wlan_cfg80211_mc_infra_cp_free_twt_stats(stats);
	wlan_cfg80211_mc_infra_cp_free_bmiss_stats(stats);
	qdf_mem_free(stats);
}

/**
 * infra_cp_stats_response_cb() - callback function to handle stats event
 * @ev: stats event buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static inline
void infra_cp_stats_response_cb(struct infra_cp_stats_event *ev,
				void *cookie)
{
	struct infra_cp_stats_event *priv;
	struct osif_request *request;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);

	priv->action = ev->action;
	priv->request_id = ev->request_id;
	priv->status = ev->status;
	get_twt_infra_cp_stats(ev, priv);

	osif_request_complete(request);
	osif_request_put(request);
}

#ifdef WLAN_SUPPORT_TWT
/*Infra limits Add comment here*/
#define MAX_TWT_STAT_VDEV_ENTRIES 1
#define MAX_TWT_STAT_MAC_ADDR_ENTRIES 1
struct infra_cp_stats_event *
wlan_cfg80211_mc_twt_get_infra_cp_stats(struct wlan_objmgr_vdev *vdev,
					uint32_t dialog_id,
					uint8_t twt_peer_mac[QDF_MAC_ADDR_SIZE],
					int *errno)
{
	void *cookie;
	QDF_STATUS status;
	struct infra_cp_stats_event *priv, *out;
	struct twt_infra_cp_stats_event *twt_event;
	struct wlan_objmgr_peer *peer;
	struct osif_request *request;
	struct infra_cp_stats_cmd_info info = {0};
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_infra_cp_stats_dealloc,
	};

	osif_debug("Enter");

	out = qdf_mem_malloc(sizeof(*out));
	if (!out) {
		*errno = -ENOMEM;
		return NULL;
	}

	out->twt_infra_cp_stats =
			qdf_mem_malloc(sizeof(*out->twt_infra_cp_stats));
	if (!out->twt_infra_cp_stats) {
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		*errno = -ENOMEM;
		goto free_stats_event;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);

	priv->twt_infra_cp_stats =
			qdf_mem_malloc(sizeof(*priv->twt_infra_cp_stats));
	if (!priv->twt_infra_cp_stats) {
		*errno = -ENOMEM;
		goto get_twt_stats_fail;
	}
	twt_event = priv->twt_infra_cp_stats;

	info.request_cookie = cookie;
	info.stats_id = TYPE_REQ_CTRL_PATH_TWT_STAT;
	info.action = ACTION_REQ_CTRL_PATH_STAT_GET;
	info.infra_cp_stats_resp_cb = infra_cp_stats_response_cb;
	info.num_pdev_ids = 0;
	info.num_vdev_ids = MAX_TWT_STAT_VDEV_ENTRIES;
	info.vdev_id[0] = wlan_vdev_get_id(vdev);
	info.num_mac_addr_list = MAX_TWT_STAT_MAC_ADDR_ENTRIES;
	qdf_mem_copy(&info.peer_mac_addr[0], twt_peer_mac, QDF_MAC_ADDR_SIZE);

	info.dialog_id = dialog_id;
	info.num_pdev_ids = 0;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_CP_STATS_ID);
	if (!peer) {
		osif_err("peer is null");
		*errno = -EINVAL;
		goto get_twt_stats_fail;
	}
	wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	status = ucfg_infra_cp_stats_register_resp_cb(wlan_vdev_get_psoc(vdev),
						      &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to register resp callback: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_twt_stats_fail;
	}

	status = ucfg_send_infra_cp_stats_request(vdev, &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send twt stats request status: %d",
			 status);
		*errno = qdf_status_to_os_return(status);
		goto get_twt_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		goto get_twt_stats_fail;
	}

	out->num_twt_infra_cp_stats = priv->num_twt_infra_cp_stats;
	out->request_id = priv->request_id;
	out->twt_infra_cp_stats->dialog_id = twt_event->dialog_id;
	out->twt_infra_cp_stats->status = twt_event->status;
	out->twt_infra_cp_stats->num_sp_cycles = twt_event->num_sp_cycles;
	out->twt_infra_cp_stats->avg_sp_dur_us = twt_event->avg_sp_dur_us;
	out->twt_infra_cp_stats->min_sp_dur_us = twt_event->min_sp_dur_us;
	out->twt_infra_cp_stats->max_sp_dur_us = twt_event->max_sp_dur_us;
	out->twt_infra_cp_stats->tx_mpdu_per_sp = twt_event->tx_mpdu_per_sp;
	out->twt_infra_cp_stats->rx_mpdu_per_sp = twt_event->rx_mpdu_per_sp;
	out->twt_infra_cp_stats->tx_bytes_per_sp = twt_event->tx_bytes_per_sp;
	out->twt_infra_cp_stats->rx_bytes_per_sp = twt_event->rx_bytes_per_sp;
	qdf_mem_copy(&out->twt_infra_cp_stats->peer_macaddr, twt_peer_mac,
		     QDF_MAC_ADDR_SIZE);
	osif_request_put(request);

	osif_debug("Exit");

	return out;

get_twt_stats_fail:
	osif_request_put(request);

free_stats_event:
	wlan_cfg80211_mc_infra_cp_stats_free_stats_event(out);

	osif_debug("Exit");

	return NULL;
}

/**
 * infra_cp_stats_reset_cb() - callback function to handle stats event
 * due to reset action
 * @ev: stats event buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void infra_cp_stats_reset_cb(struct infra_cp_stats_event *ev,
				    void *cookie)
{
	struct infra_cp_stats_event *priv;
	struct osif_request *request;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);

	osif_debug("clear stats action %d req_id %d, status %d num_cp_stats %d",
		   ev->action, ev->request_id, ev->status,
		   ev->num_twt_infra_cp_stats);

	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * wlan_cfg80211_mc_twt_clear_infra_cp_stats() - send clear twt statistics
 * request to firmware
 * @vdev: vdev id
 * @dialog_id: dialog id of the twt session.
 * @twt_peer_mac: peer mac address
 *
 * Return: 0 for success or error code for failure
 */
int
wlan_cfg80211_mc_twt_clear_infra_cp_stats(
					struct wlan_objmgr_vdev *vdev,
					uint32_t dialog_id,
					uint8_t twt_peer_mac[QDF_MAC_ADDR_SIZE])
{
	int ret;
	void *cookie;
	QDF_STATUS status;
	struct infra_cp_stats_event *priv;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	struct osif_request *request;
	struct infra_cp_stats_cmd_info info = {0};
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_infra_cp_stats_dealloc,
	};

	osif_debug("Enter");

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return -EINVAL;

	request = osif_request_alloc(&params);
	if (!request)
		return -ENOMEM;

	ucfg_mlme_set_twt_command_in_progress(psoc,
					(struct qdf_mac_addr *)twt_peer_mac,
					dialog_id,
					WLAN_TWT_CLEAR_STATISTICS);

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);

	priv->twt_infra_cp_stats =
			qdf_mem_malloc(sizeof(*priv->twt_infra_cp_stats));
	if (!priv->twt_infra_cp_stats) {
		ret = -ENOMEM;
		goto clear_twt_stats_fail;
	}

	info.request_cookie = cookie;
	info.stats_id = TYPE_REQ_CTRL_PATH_TWT_STAT;
	info.action = ACTION_REQ_CTRL_PATH_STAT_RESET;

	info.infra_cp_stats_resp_cb = infra_cp_stats_reset_cb;
	info.num_pdev_ids = 0;
	info.num_vdev_ids = MAX_TWT_STAT_VDEV_ENTRIES;
	info.vdev_id[0] = wlan_vdev_get_id(vdev);
	info.num_mac_addr_list = MAX_TWT_STAT_MAC_ADDR_ENTRIES;
	qdf_mem_copy(&info.peer_mac_addr[0], twt_peer_mac, QDF_MAC_ADDR_SIZE);

	info.dialog_id = dialog_id;
	info.num_pdev_ids = 0;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_CP_STATS_ID);
	if (!peer) {
		osif_err("peer is null");
		ret = -EINVAL;
		goto clear_twt_stats_fail;
	}
	wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	status = ucfg_infra_cp_stats_register_resp_cb(psoc, &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to register resp callback: %d", status);
		ret = qdf_status_to_os_return(status);
		goto clear_twt_stats_fail;
	}

	status = ucfg_send_infra_cp_stats_request(vdev, &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send twt stats request status: %d",
			 status);
		ret = qdf_status_to_os_return(status);
		goto clear_twt_stats_fail;
	}

	ret = osif_request_wait_for_response(request);
	if (ret)
		osif_err("wait failed or timed out ret: %d", ret);

clear_twt_stats_fail:
	ucfg_mlme_set_twt_command_in_progress(psoc,
					(struct qdf_mac_addr *)twt_peer_mac,
					dialog_id,
					WLAN_TWT_NONE);
	osif_request_put(request);
	osif_debug("Exit");

	return ret;
}
#endif
#endif /* WLAN_SUPPORT_INFRA_CTRL_PATH_STATS */

struct stats_event *
wlan_cfg80211_mc_cp_stats_get_station_stats(struct wlan_objmgr_vdev *vdev,
					    int *errno)
{
	void *cookie;
	QDF_STATUS status;
	struct stats_event *priv, *out;
	struct wlan_objmgr_peer *peer;
	struct osif_request *request;
	struct request_info info = {0};
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_cp_stats_dealloc,
	};

	osif_debug("Enter");

	out = qdf_mem_malloc(sizeof(*out));
	if (!out) {
		*errno = -ENOMEM;
		return NULL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);
	info.cookie = cookie;
	info.u.get_station_stats_cb = get_station_stats_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_CP_STATS_ID);
	if (!peer) {
		osif_err("peer is null");
		*errno = -EINVAL;
		goto get_station_stats_fail;
	}
	qdf_mem_copy(info.peer_mac_addr, peer->macaddr, QDF_MAC_ADDR_SIZE);

	wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	status = ucfg_mc_cp_stats_send_stats_request(vdev, TYPE_STATION_STATS,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send stats request status: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_station_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		goto get_station_stats_fail;
	}

	if (!priv->vdev_summary_stats || !priv->vdev_chain_rssi ||
	    priv->num_summary_stats == 0 || priv->num_chain_rssi_stats == 0) {
		osif_err("Invalid stats");
		osif_err("summary %d:%pK, rssi %d:%pK",
			 priv->num_summary_stats, priv->vdev_summary_stats,
			 priv->num_chain_rssi_stats, priv->vdev_chain_rssi);
		*errno = -EINVAL;
		goto get_station_stats_fail;
	}

	out->tx_rate = priv->tx_rate;
	out->rx_rate = priv->rx_rate;
	out->tx_rate_flags = priv->tx_rate_flags;
	out->num_summary_stats = priv->num_summary_stats;
	out->num_chain_rssi_stats = priv->num_chain_rssi_stats;
	out->vdev_summary_stats = priv->vdev_summary_stats;
	priv->vdev_summary_stats = NULL;
	out->vdev_chain_rssi = priv->vdev_chain_rssi;
	priv->vdev_chain_rssi = NULL;
	out->num_peer_adv_stats = priv->num_peer_adv_stats;
	if (priv->peer_adv_stats)
		out->peer_adv_stats = priv->peer_adv_stats;
	priv->peer_adv_stats = NULL;
	out->bcn_protect_stats = priv->bcn_protect_stats;
	osif_request_put(request);

	osif_debug("Exit");

	return out;

get_station_stats_fail:
	osif_request_put(request);
	wlan_cfg80211_mc_cp_stats_free_stats_event(out);

	osif_debug("Exit");

	return NULL;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
struct big_data_stats_event *
wlan_cfg80211_mc_cp_get_big_data_stats(struct wlan_objmgr_vdev *vdev,
				       int *errno)
{
	void *cookie;
	QDF_STATUS status;
	struct big_data_stats_event *priv, *out;
	struct hdd_context *hdd_ctx = NULL;
	struct osif_request *request;
	struct request_info info = {0};
	struct request_info last_req = {0};
	bool pending = false;

	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
	};

	osif_debug("Enter");

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (wlan_hdd_validate_context(hdd_ctx))
		return NULL;

	out = qdf_mem_malloc(sizeof(*out));
	if (!out)
		return NULL;

	request = osif_request_alloc(&params);
	if (!request) {
		qdf_mem_free(out);
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);
	info.cookie = cookie;
	info.u.get_big_data_stats_cb = get_big_data_stats_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));

	status = ucfg_send_big_data_stats_request(vdev,
						  TYPE_BIG_DATA_STATS,
						  &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send stats request status: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_station_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		ucfg_mc_cp_stats_reset_pending_req(hdd_ctx->psoc,
						   TYPE_BIG_DATA_STATS,
						   &last_req, &pending);
		goto get_station_stats_fail;
	}

	osif_debug("vdev_id: %d tsf_out_of_sync: %d ani_level: %d tx_pwr_last_data_frm: %d target_power_dsss: %d target_power_ofdm: %d rix_last_data_frm: %d tx_rate_last_data_frm: %d",
		   priv->vdev_id,
		   priv->tsf_out_of_sync, priv->ani_level,
		   priv->last_data_tx_pwr, priv->target_power_dsss,
		   priv->target_power_ofdm, priv->last_tx_data_rix,
		   priv->last_tx_data_rate_kbps);

	out->vdev_id = priv->vdev_id;
	out->tsf_out_of_sync = priv->tsf_out_of_sync;
	out->ani_level = priv->ani_level;
	out->last_data_tx_pwr = priv->last_data_tx_pwr;
	out->target_power_dsss = priv->target_power_dsss;
	out->target_power_ofdm = priv->target_power_ofdm;
	out->last_tx_data_rix = priv->last_tx_data_rix;
	out->last_tx_data_rate_kbps = priv->last_tx_data_rate_kbps;
	osif_request_put(request);

	osif_debug("Exit");

	return out;

get_station_stats_fail:
	osif_request_put(request);
	wlan_cfg80211_mc_cp_stats_free_big_data_stats_event(out);

	osif_debug("Exit");

	return NULL;
}
#endif

#ifdef WLAN_FEATURE_MIB_STATS
/**
 * get_mib_stats_cb() - get mib stats from fw callback function
 * @ev: mib stats buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void get_mib_stats_cb(struct stats_event *ev, void *cookie)
{
	struct stats_event *priv;
	struct osif_request *request;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);

	priv->mib_stats = qdf_mem_malloc(sizeof(*ev->mib_stats));
	if (!priv->mib_stats)
		goto get_mib_stats_cb_fail;

	priv->num_mib_stats = ev->num_mib_stats;
	qdf_mem_copy(priv->mib_stats, ev->mib_stats, sizeof(*ev->mib_stats));

get_mib_stats_cb_fail:
	osif_request_complete(request);
	osif_request_put(request);
}

struct stats_event *
wlan_cfg80211_mc_cp_stats_get_mib_stats(struct wlan_objmgr_vdev *vdev,
					int *errno)
{
	void *cookie;
	QDF_STATUS status;
	struct stats_event *priv, *out;
	struct wlan_objmgr_peer *peer;
	struct osif_request *request;
	struct request_info info = {0};
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_cp_stats_dealloc,
	};

	out = qdf_mem_malloc(sizeof(*out));
	if (!out) {
		*errno = -ENOMEM;
		return NULL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);
	info.cookie = cookie;
	info.u.get_mib_stats_cb = get_mib_stats_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_CP_STATS_ID);
	if (!peer) {
		osif_err("peer is null");
		*errno = -EINVAL;
		goto get_mib_stats_fail;
	}
	qdf_mem_copy(info.peer_mac_addr, peer->macaddr, QDF_MAC_ADDR_SIZE);

	osif_debug("vdev id %d, pdev id %d, peer " QDF_MAC_ADDR_FMT,
		   info.vdev_id, info.pdev_id,
		   QDF_MAC_ADDR_REF(info.peer_mac_addr));

	wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	status = ucfg_mc_cp_stats_send_stats_request(vdev, TYPE_MIB_STATS,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send stats request status: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_mib_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		goto get_mib_stats_fail;
	}

	if (!priv->mib_stats || priv->num_mib_stats == 0 ) {
		osif_err("Invalid mib stats %d:%pK",
			 priv->num_mib_stats, priv->mib_stats);
		*errno = -EINVAL;
		goto get_mib_stats_fail;
	}

	out->num_mib_stats = priv->num_mib_stats;
	out->mib_stats = priv->mib_stats;
	priv->mib_stats = NULL;

	osif_request_put(request);

	return out;

get_mib_stats_fail:
	osif_request_put(request);
	wlan_cfg80211_mc_cp_stats_free_stats_event(out);

	return NULL;
}
#endif

/**
 * copy_peer_stats_info_ext - Copy peer ext stats info from stats event to
 * destination peer stats info
 * @dst_peer_stats_info: Destination peer ext stats pointer where peer ext info
 * needs to be copied.
 * @ev: Stats event pointer from where peers stats info needs to be copied
 *
 * Return: Void
 */
static void
copy_peer_stats_info_ext(struct peer_stats_info_ext_event *dst_peer_stats_info,
			 struct stats_event *ev)
{
	uint32_t i, j;
	struct peer_stats_info_ext_event *src_peer_stats_info =
							ev->peer_stats_info_ext;
	struct peer_stats_info_ext_event *peer_stats_info = dst_peer_stats_info;

	for (i = 0; i < ev->num_peer_stats_info_ext; i++) {
		qdf_mem_copy(&peer_stats_info->peer_macaddr,
			     &src_peer_stats_info->peer_macaddr,
			     sizeof(peer_stats_info->peer_macaddr));
		peer_stats_info->tx_packets = src_peer_stats_info->tx_packets;
		peer_stats_info->tx_bytes = src_peer_stats_info->tx_bytes;
		peer_stats_info->rx_packets = src_peer_stats_info->rx_packets;
		peer_stats_info->rx_bytes = src_peer_stats_info->rx_bytes;
		peer_stats_info->tx_retries = src_peer_stats_info->tx_retries;
		peer_stats_info->tx_failed = src_peer_stats_info->tx_failed;
		peer_stats_info->tx_succeed = src_peer_stats_info->tx_succeed;
		peer_stats_info->rssi = src_peer_stats_info->rssi;
		peer_stats_info->tx_rate = src_peer_stats_info->tx_rate;
		peer_stats_info->tx_rate_code =
					src_peer_stats_info->tx_rate_code;
		peer_stats_info->rx_rate = src_peer_stats_info->rx_rate;
		peer_stats_info->rx_rate_code =
					src_peer_stats_info->rx_rate_code;
		for (j = 0; j < WMI_MAX_CHAINS; j++)
			peer_stats_info->peer_rssi_per_chain[j] =
				src_peer_stats_info->peer_rssi_per_chain[j];

		if (src_peer_stats_info->num_tx_rate_counts) {
			peer_stats_info->tx_pkt_per_mcs =
				qdf_mem_malloc(
				src_peer_stats_info->num_tx_rate_counts *
				sizeof(uint32_t));
			if (!peer_stats_info->tx_pkt_per_mcs)
				return;

			peer_stats_info->num_tx_rate_counts =
					src_peer_stats_info->num_tx_rate_counts;
			qdf_mem_copy(peer_stats_info->tx_pkt_per_mcs,
				     src_peer_stats_info->tx_pkt_per_mcs,
				     peer_stats_info->num_tx_rate_counts *
				     sizeof(uint32_t));
		}
		if (src_peer_stats_info->num_rx_rate_counts) {
			peer_stats_info->rx_pkt_per_mcs =
				qdf_mem_malloc(
				src_peer_stats_info->num_rx_rate_counts *
				sizeof(uint32_t));
			if (!peer_stats_info->rx_pkt_per_mcs)
				return;

			peer_stats_info->num_rx_rate_counts =
					src_peer_stats_info->num_rx_rate_counts;
			qdf_mem_copy(peer_stats_info->rx_pkt_per_mcs,
				     src_peer_stats_info->rx_pkt_per_mcs,
				     peer_stats_info->num_rx_rate_counts *
				     sizeof(uint32_t));
		}
		src_peer_stats_info++;
		peer_stats_info++;
	}
}

/**
 * get_peer_stats_cb() - get_peer_stats_cb callback function
 * @ev: peer stats buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void get_peer_stats_cb(struct stats_event *ev, void *cookie)
{
	struct stats_event *priv;
	struct osif_request *request;
	uint32_t peer_stats_info_size;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	peer_stats_info_size = sizeof(*ev->peer_stats_info_ext) *
			       ev->num_peer_stats_info_ext;

	if (priv->peer_stats_info_ext) {
		osif_err("invalid context cookie %pK request %pK",
			 cookie, request);
		goto peer_stats_cb_fail;
	}

	priv->peer_stats_info_ext = qdf_mem_malloc(peer_stats_info_size);
	if (!priv->peer_stats_info_ext)
		goto peer_stats_cb_fail;

	copy_peer_stats_info_ext(priv->peer_stats_info_ext, ev);

	priv->num_peer_stats_info_ext = ev->num_peer_stats_info_ext;

peer_stats_cb_fail:
	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * get_station_adv_stats_cb() - get_station_adv_stats_cb callback function
 * @ev: station stats buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static void get_station_adv_stats_cb(struct stats_event *ev, void *cookie)
{
	struct stats_event *priv;
	struct osif_request *request;
	uint32_t peer_adv_size;

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	if (!ev->peer_adv_stats || ev->num_peer_adv_stats == 0) {
		osif_debug("Invalid stats");
		goto station_adv_stats_cb_fail;
	}

	peer_adv_size = sizeof(*ev->peer_adv_stats) * ev->num_peer_adv_stats;

	if (peer_adv_size) {
		priv->peer_adv_stats = qdf_mem_malloc(peer_adv_size);
		if (!priv->peer_adv_stats)
			goto station_adv_stats_cb_fail;

		qdf_mem_copy(priv->peer_adv_stats, ev->peer_adv_stats,
			     peer_adv_size);
	}
	priv->num_peer_adv_stats = ev->num_peer_adv_stats;

station_adv_stats_cb_fail:
	osif_request_complete(request);
	osif_request_put(request);
}

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS
wlan_cfg80211_get_mlstats_vdev_params(struct wlan_objmgr_vdev *vdev,
				      struct request_info *info)
{
	struct wlan_objmgr_psoc *psoc;
	bool is_mlo_vdev;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_INVAL;

	is_mlo_vdev = wlan_vdev_mlme_get_is_mlo_vdev(psoc, info->vdev_id);
	if (is_mlo_vdev) {
		status = mlo_get_mlstats_vdev_params(psoc, &info->ml_vdev_info,
						     info->vdev_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			osif_err("unable to get vdev params for mlo stats");
			return status;
		}
	}

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS
wlan_cfg80211_get_mlstats_vdev_params(struct wlan_objmgr_vdev *vdev,
				      struct request_info *info)
{
	return QDF_STATUS_SUCCESS;
}
#endif

struct stats_event *
wlan_cfg80211_mc_cp_stats_get_peer_stats(struct wlan_objmgr_vdev *vdev,
					 const uint8_t *mac_addr,
					 int *errno)
{
	void *cookie;
	QDF_STATUS status;
	struct stats_event *priv, *out;
	struct osif_request *request;
	struct request_info info = {0};
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_cp_stats_dealloc,
	};

	osif_debug("Enter");

	out = qdf_mem_malloc(sizeof(*out));
	if (!out) {
		*errno = -ENOMEM;
		return NULL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);
	info.cookie = cookie;
	info.u.get_peer_stats_cb = get_peer_stats_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	qdf_mem_copy(info.peer_mac_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	status = ucfg_mc_cp_stats_send_stats_request(vdev,
						     TYPE_PEER_STATS_INFO_EXT,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send stats request status: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_peer_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		goto get_peer_stats_fail;
	}

	if (!priv->peer_stats_info_ext || priv->num_peer_stats_info_ext == 0) {
		osif_err("Invalid stats");
		osif_err("Peer stats info ext %d:%pK",
			 priv->num_peer_stats_info_ext,
			 priv->peer_stats_info_ext);
		*errno = -EINVAL;
		goto get_peer_stats_fail;
	}

	out->num_peer_stats_info_ext = priv->num_peer_stats_info_ext;
	out->peer_stats_info_ext = priv->peer_stats_info_ext;
	priv->peer_stats_info_ext = NULL;
	osif_request_put(request);

	request = osif_request_alloc(&params);
	if (!request) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(out);
		*errno = -ENOMEM;
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);
	info.cookie = cookie;
	info.u.get_station_stats_cb = get_station_adv_stats_cb;

	status = wlan_cfg80211_get_mlstats_vdev_params(vdev, &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		*errno = qdf_status_to_os_return(status);
		goto get_peer_stats_fail;
	}

	status = ucfg_mc_cp_stats_send_stats_request(vdev, TYPE_STATION_STATS,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send stats request status: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_peer_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		goto get_peer_stats_fail;
	}

	if (!priv->peer_adv_stats || priv->num_peer_adv_stats == 0) {
		osif_debug("Invalid stats");
		goto get_peer_stats_fail;
	}

	out->num_peer_adv_stats = priv->num_peer_adv_stats;
	out->peer_adv_stats = priv->peer_adv_stats;
	priv->peer_adv_stats = NULL;
	osif_request_put(request);
	osif_debug("Exit");
	return out;
get_peer_stats_fail:
	osif_request_put(request);
	wlan_cfg80211_mc_cp_stats_free_stats_event(out);

	osif_debug("Exit");

	return NULL;
}

void wlan_cfg80211_mc_cp_stats_free_stats_event(struct stats_event *stats)
{
	if (!stats)
		return;

	qdf_mem_free(stats->pdev_stats);
	qdf_mem_free(stats->peer_stats);
	qdf_mem_free(stats->cca_stats);
	qdf_mem_free(stats->vdev_summary_stats);
	qdf_mem_free(stats->vdev_chain_rssi);
	qdf_mem_free(stats->peer_adv_stats);
	wlan_free_mib_stats(stats);
	wlan_cfg80211_mc_cp_stats_free_peer_stats_info_ext(stats);
	qdf_mem_free(stats);
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
void
wlan_cfg80211_mc_cp_stats_free_big_data_stats_event(
					struct big_data_stats_event *stats)
{
	if (!stats)
		return;

	qdf_mem_free(stats);
}
#endif

#ifdef CONFIG_WLAN_BMISS
static void get_bmiss_infra_cp_stats(struct infra_cp_stats_event *ev,
				     struct infra_cp_stats_event *priv)

{
	int idx = 0;

	if (!ev || !ev->bmiss_infra_cp_stats) {
		osif_err("got bmiss_infra_cp_stats as NULL");
		return;
	}
	priv->bmiss_infra_cp_stats->num_pre_bmiss =
					ev->bmiss_infra_cp_stats->num_pre_bmiss;
	for (idx = 0; idx < BMISS_STATS_RSSI_SAMPLES_MAX; idx++) {
		priv->bmiss_infra_cp_stats->rssi_samples[idx].rssi =
			ev->bmiss_infra_cp_stats->rssi_samples[idx].rssi;
		priv->bmiss_infra_cp_stats->rssi_samples[idx].sample_time =
			ev->bmiss_infra_cp_stats->rssi_samples[idx].sample_time;
	}
	priv->bmiss_infra_cp_stats->rssi_sample_curr_index =
			ev->bmiss_infra_cp_stats->rssi_sample_curr_index;
	priv->bmiss_infra_cp_stats->num_first_bmiss =
			ev->bmiss_infra_cp_stats->num_first_bmiss;
	priv->bmiss_infra_cp_stats->num_final_bmiss =
			ev->bmiss_infra_cp_stats->num_final_bmiss;
	priv->bmiss_infra_cp_stats->num_null_sent_in_first_bmiss =
		ev->bmiss_infra_cp_stats->num_null_sent_in_first_bmiss;
	priv->bmiss_infra_cp_stats->num_null_failed_in_first_bmiss =
		ev->bmiss_infra_cp_stats->num_null_failed_in_first_bmiss;
	priv->bmiss_infra_cp_stats->num_null_sent_in_final_bmiss =
		ev->bmiss_infra_cp_stats->num_null_sent_in_final_bmiss;
	priv->bmiss_infra_cp_stats->num_null_failed_in_final_bmiss =
		ev->bmiss_infra_cp_stats->num_null_failed_in_final_bmiss;
	priv->bmiss_infra_cp_stats->cons_bmiss_stats.num_of_bmiss_sequences =
	ev->bmiss_infra_cp_stats->cons_bmiss_stats.num_of_bmiss_sequences;
	priv->bmiss_infra_cp_stats->cons_bmiss_stats.num_bitmask_wraparound =
	ev->bmiss_infra_cp_stats->cons_bmiss_stats.num_bitmask_wraparound;
	priv->bmiss_infra_cp_stats->cons_bmiss_stats.num_bcn_hist_lost =
	ev->bmiss_infra_cp_stats->cons_bmiss_stats.num_bcn_hist_lost;
}

/**
 * infra_cp_stats_bmiss_response_cb() - callback function to handle stats event
 * @ev: stats event buffer
 * @cookie: a cookie for the request context
 *
 * Return: None
 */
static inline
void infra_cp_stats_bmiss_response_cb(struct infra_cp_stats_event *ev,
				      void *cookie)
{
	struct infra_cp_stats_event *priv;
	struct osif_request *request;

	osif_debug("Enter");

	request = osif_request_get(cookie);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);

	priv->action = ev->action;
	priv->request_id = ev->request_id;
	priv->status = ev->status;
	get_bmiss_infra_cp_stats(ev, priv);

	osif_request_complete(request);
	osif_request_put(request);
}

#define MAX_BMISS_STAT_VDEV_ENTRIES 1
#define MAX_BMISS_STAT_MAC_ADDR_ENTRIES 1

struct infra_cp_stats_event *
wlan_cfg80211_mc_bmiss_get_infra_cp_stats(struct wlan_objmgr_vdev *vdev,
					  uint8_t *bmiss_peer_mac, int *errno)
{
	void *cookie;
	int idx = 0;
	QDF_STATUS status;
	struct infra_cp_stats_event *priv, *out;
	struct bmiss_infra_cp_stats_event *bmiss_event;
	struct osif_request *request;
	struct infra_cp_stats_cmd_info info = {0};
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 2 * CP_STATS_WAIT_TIME_STAT,
		.dealloc = wlan_cfg80211_mc_infra_cp_stats_dealloc,
	};

	osif_debug("Enter");

	out = qdf_mem_malloc(sizeof(*out));
	if (!out) {
		*errno = -ENOMEM;
		return NULL;
	}

	out->bmiss_infra_cp_stats =
			qdf_mem_malloc(sizeof(*out->bmiss_infra_cp_stats));
	if (!out->bmiss_infra_cp_stats) {
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		qdf_mem_free(out->bmiss_infra_cp_stats);
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);

	priv->bmiss_infra_cp_stats =
			qdf_mem_malloc(sizeof(*priv->bmiss_infra_cp_stats));
	if (!priv->bmiss_infra_cp_stats) {
		qdf_mem_free(out->bmiss_infra_cp_stats);
		qdf_mem_free(out);
		*errno = -ENOMEM;
		return NULL;
	}
	bmiss_event = priv->bmiss_infra_cp_stats;
	info.request_cookie = cookie;
	info.stats_id = TYPE_REQ_CTRL_PATH_BMISS_STAT;
	info.action = ACTION_REQ_CTRL_PATH_STAT_GET;
	info.infra_cp_stats_resp_cb = infra_cp_stats_bmiss_response_cb;
	info.num_pdev_ids = 0;
	info.num_vdev_ids = MAX_BMISS_STAT_VDEV_ENTRIES;
	info.vdev_id[0] = wlan_vdev_get_id(vdev);
	info.num_mac_addr_list = MAX_TWT_STAT_MAC_ADDR_ENTRIES;
	info.num_pdev_ids = 0;

	qdf_mem_copy(&info.peer_mac_addr[0], bmiss_peer_mac, QDF_MAC_ADDR_SIZE);
	status = ucfg_infra_cp_stats_register_resp_cb(wlan_vdev_get_psoc(vdev),
						      &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to register resp callback: %d", status);
		*errno = qdf_status_to_os_return(status);
		goto get_bmiss_stats_fail;
	}

	status = ucfg_send_infra_cp_stats_request(vdev, &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to send bmiss stats request status: %d",
			 status);
		*errno = qdf_status_to_os_return(status);
		goto get_bmiss_stats_fail;
	}

	*errno = osif_request_wait_for_response(request);
	if (*errno) {
		osif_err("wait failed or timed out ret: %d", *errno);
		goto get_bmiss_stats_fail;
	}

	out->request_id = priv->request_id;
	out->bmiss_infra_cp_stats->num_pre_bmiss = bmiss_event->num_pre_bmiss;
	out->bmiss_infra_cp_stats->num_pre_bmiss =
					bmiss_event->num_pre_bmiss;
	for (idx = 0; idx < BMISS_STATS_RSSI_SAMPLES_MAX; idx++) {
		out->bmiss_infra_cp_stats->rssi_samples[idx].rssi =
			bmiss_event->rssi_samples[idx].rssi;
		out->bmiss_infra_cp_stats->rssi_samples[idx].sample_time =
			bmiss_event->rssi_samples[idx].sample_time;
	}
	out->bmiss_infra_cp_stats->rssi_sample_curr_index =
					bmiss_event->rssi_sample_curr_index;
	out->bmiss_infra_cp_stats->num_first_bmiss =
					bmiss_event->num_first_bmiss;
	out->bmiss_infra_cp_stats->num_null_sent_in_first_bmiss =
				bmiss_event->num_null_sent_in_first_bmiss;
	out->bmiss_infra_cp_stats->num_null_failed_in_first_bmiss =
				bmiss_event->num_null_failed_in_first_bmiss;
	out->bmiss_infra_cp_stats->num_null_sent_in_final_bmiss =
				bmiss_event->num_null_sent_in_final_bmiss;
	out->bmiss_infra_cp_stats->num_null_failed_in_final_bmiss =
				bmiss_event->num_null_failed_in_final_bmiss;
	out->bmiss_infra_cp_stats->cons_bmiss_stats.num_of_bmiss_sequences =
			bmiss_event->cons_bmiss_stats.num_of_bmiss_sequences;
	out->bmiss_infra_cp_stats->cons_bmiss_stats.num_bitmask_wraparound =
			bmiss_event->cons_bmiss_stats.num_bitmask_wraparound;
	out->bmiss_infra_cp_stats->cons_bmiss_stats.num_bcn_hist_lost =
			bmiss_event->cons_bmiss_stats.num_bcn_hist_lost;

	qdf_mem_copy(&out->bmiss_infra_cp_stats->peer_macaddr, bmiss_peer_mac,
		     QDF_MAC_ADDR_SIZE);
	osif_request_put(request);
	osif_debug("Exit");
	return out;
get_bmiss_stats_fail:
	osif_request_put(request);
	wlan_cfg80211_mc_infra_cp_stats_free_stats_event(out);
	osif_debug("Exit");
	return NULL;
}
#endif /* CONFIG_WLAN_BMISS */


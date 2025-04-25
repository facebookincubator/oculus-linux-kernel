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
  * DOC: os_if_dp.c
  *
  *
  */
#include "os_if_dp.h"
#include "wlan_nlink_srv.h"
#include <wlan_cfg80211.h>
#include <wlan_osif_priv.h>
#include <cdp_txrx_cmn.h>
#include "qca_vendor.h"
#include "wlan_dp_ucfg_api.h"
#include "osif_vdev_sync.h"
#include "osif_sync.h"
#include <net/netevent.h>
#include "wlan_osif_request_manager.h"
#include <ol_defines.h>

/*
 * define short names for the global vendor params
 * used by wlan_hdd_cfg80211_setarp_stats_cmd()
 */
#define STATS_GET_INVALID \
	QCA_ATTR_NUD_STATS_SET_INVALID
#define COUNT_FROM_NETDEV \
	QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_FROM_NETDEV
#define COUNT_TO_LOWER_MAC \
	QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TO_LOWER_MAC
#define RX_COUNT_BY_LOWER_MAC \
	QCA_ATTR_NUD_STATS_ARP_REQ_RX_COUNT_BY_LOWER_MAC
#define COUNT_TX_SUCCESS \
	QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TX_SUCCESS
#define RSP_RX_COUNT_BY_LOWER_MAC \
	QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_LOWER_MAC
#define RSP_RX_COUNT_BY_UPPER_MAC \
	QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_UPPER_MAC
#define RSP_COUNT_TO_NETDEV \
	QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_TO_NETDEV
#define RSP_COUNT_OUT_OF_ORDER_DROP \
	QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_OUT_OF_ORDER_DROP
#define AP_LINK_ACTIVE \
	QCA_ATTR_NUD_STATS_AP_LINK_ACTIVE
#define AP_LINK_DAD \
	QCA_ATTR_NUD_STATS_IS_DAD
#define DATA_PKT_STATS \
	QCA_ATTR_NUD_STATS_DATA_PKT_STATS
#define STATS_GET_MAX \
	QCA_ATTR_NUD_STATS_GET_MAX

#define CHECK_STATS_INVALID \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_INVALID
#define CHECK_STATS_PKT_TYPE \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_TYPE
#define CHECK_STATS_PKT_DNS_DOMAIN_NAME \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_DNS_DOMAIN_NAME
#define CHECK_STATS_PKT_SRC_PORT \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_SRC_PORT
#define CHECK_STATS_PKT_DEST_PORT \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_DEST_PORT
#define CHECK_STATS_PKT_DEST_IPV4 \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_DEST_IPV4
#define CHECK_STATS_PKT_DEST_IPV6 \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_DEST_IPV6
#define CHECK_STATS_PKT_REQ_COUNT_FROM_NETDEV \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_REQ_COUNT_FROM_NETDEV
#define CHECK_STATS_PKT_REQ_COUNT_TO_LOWER_MAC \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_REQ_COUNT_TO_LOWER_MAC
#define CHECK_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC
#define CHECK_STATS_PKT_REQ_COUNT_TX_SUCCESS \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_REQ_COUNT_TX_SUCCESS
#define CHECK_STATS_PKT_RSP_RX_COUNT_BY_LOWER_MAC \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_RSP_RX_COUNT_BY_LOWER_MAC
#define CHECK_STATS_PKT_RSP_RX_COUNT_BY_UPPER_MAC \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_RSP_RX_COUNT_BY_UPPER_MAC
#define CHECK_STATS_PKT_RSP_COUNT_TO_NETDEV \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_RSP_COUNT_TO_NETDEV
#define CHECK_STATS_PKT_RSP_COUNT_OUT_OF_ORDER_DROP \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_PKT_RSP_COUNT_OUT_OF_ORDER_DROP
#define CHECK_DATA_STATS_MAX \
	QCA_ATTR_CONNECTIVITY_CHECK_DATA_STATS_MAX

#define STATS_SET_INVALID \
	QCA_ATTR_NUD_STATS_SET_INVALID
#define STATS_SET_START \
	QCA_ATTR_NUD_STATS_SET_START
#define STATS_GW_IPV4 \
	QCA_ATTR_NUD_STATS_GW_IPV4
#define STATS_SET_DATA_PKT_INFO \
		QCA_ATTR_NUD_STATS_SET_DATA_PKT_INFO
#define STATS_SET_MAX \
	QCA_ATTR_NUD_STATS_SET_MAX

const struct nla_policy
dp_set_nud_stats_policy[STATS_SET_MAX + 1] = {
	[STATS_SET_START] = {.type = NLA_FLAG },
	[STATS_GW_IPV4] = {.type = NLA_U32 },
	[STATS_SET_DATA_PKT_INFO] = {.type = NLA_NESTED },
};

/* define short names for the global vendor params */
#define CONNECTIVITY_STATS_SET_INVALID \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_SET_INVALID
#define STATS_PKT_INFO_TYPE \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_STATS_PKT_INFO_TYPE
#define STATS_DNS_DOMAIN_NAME \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_DNS_DOMAIN_NAME
#define STATS_SRC_PORT \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_SRC_PORT
#define STATS_DEST_PORT \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_DEST_PORT
#define STATS_DEST_IPV4 \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_DEST_IPV4
#define STATS_DEST_IPV6 \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_DEST_IPV6
#define CONNECTIVITY_STATS_SET_MAX \
	QCA_ATTR_CONNECTIVITY_CHECK_STATS_SET_MAX

const struct nla_policy
dp_set_connectivity_check_stats[CONNECTIVITY_STATS_SET_MAX + 1] = {
	[STATS_PKT_INFO_TYPE] = {.type = NLA_U32 },
	[STATS_DNS_DOMAIN_NAME] = {.type = NLA_NUL_STRING,
					.len = DNS_DOMAIN_NAME_MAX_LEN },
	[STATS_SRC_PORT] = {.type = NLA_U32 },
	[STATS_DEST_PORT] = {.type = NLA_U32 },
	[STATS_DEST_IPV4] = {.type = NLA_U32 },
	[STATS_DEST_IPV6] = {.type = NLA_BINARY,
			.len = ICMPV6_ADDR_LEN },
};

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * osif_dp_send_tcp_param_update_event() - Send vendor event to update
 * TCP parameter through Wi-Fi HAL
 * @psoc: Pointer to psoc context
 * @data: Parameters to update
 * @dir: Direction(tx/rx) to update
 *
 * Return: None
 */
static
void osif_dp_send_tcp_param_update_event(struct wlan_objmgr_psoc *psoc,
					 union wlan_tp_data *data,
					 uint8_t dir)
{
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev;
	struct pdev_osif_priv *os_priv;
	uint32_t event_len;
	bool tcp_limit_output = false;
	bool tcp_del_ack_ind_enabled = false;
	bool tcp_adv_win_scl_enabled = false;
	enum wlan_tp_level next_tp_level = WLAN_SVC_TP_NONE;
	enum qca_nl80211_vendor_subcmds_index index =
		QCA_NL80211_VENDOR_SUBCMD_THROUGHPUT_CHANGE_EVENT_INDEX;

	event_len = sizeof(uint8_t) + sizeof(uint8_t) + NLMSG_HDRLEN;
	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0, WLAN_DP_ID);
	if (!pdev)
		return;

	os_priv = wlan_pdev_get_ospriv(pdev);

	if (dir == 0) /*TX Flow */ {
		struct wlan_tx_tp_data *tx_tp_data =
				(struct wlan_tx_tp_data *)data;

		next_tp_level = tx_tp_data->level;

		if (tx_tp_data->tcp_limit_output) {
			/* TCP_LIMIT_OUTPUT_BYTES */
			event_len += sizeof(uint32_t);
			tcp_limit_output = true;
		}
	} else if (dir == 1) /* RX Flow */ {
		struct wlan_rx_tp_data *rx_tp_data =
				(struct wlan_rx_tp_data *)data;

		next_tp_level = rx_tp_data->level;

		if (rx_tp_data->rx_tp_flags & TCP_DEL_ACK_IND_MASK) {
			event_len += sizeof(uint32_t); /* TCP_DELACK_SEG */
			tcp_del_ack_ind_enabled = true;
		}
		if (rx_tp_data->rx_tp_flags & TCP_ADV_WIN_SCL_MASK) {
			event_len += sizeof(uint32_t); /* TCP_ADV_WIN_SCALE */
			tcp_adv_win_scl_enabled = true;
		}
	} else {
		dp_err("Invalid Direction [%d]", dir);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_DP_ID);
		return;
	}

	vendor_event = wlan_cfg80211_vendor_event_alloc(os_priv->wiphy,
							NULL, event_len,
							index, GFP_KERNEL);

	if (!vendor_event) {
		dp_err("wlan_cfg80211_vendor_event_alloc failed");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_DP_ID);
		return;
	}

	if (nla_put_u8(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_DIRECTION,
		dir))
		goto tcp_param_change_nla_failed;

	if (nla_put_u8(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_THROUGHPUT_LEVEL,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		QCA_WLAN_THROUGHPUT_LEVEL_LOW :
		QCA_WLAN_THROUGHPUT_LEVEL_HIGH)))
		goto tcp_param_change_nla_failed;

	if (tcp_limit_output &&
	    nla_put_u32(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_LIMIT_OUTPUT_BYTES,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		 TCP_LIMIT_OUTPUT_BYTES_LOW :
		 TCP_LIMIT_OUTPUT_BYTES_HI)))
		goto tcp_param_change_nla_failed;

	if (tcp_del_ack_ind_enabled &&
	    (nla_put_u32(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_DELACK_SEG,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		 TCP_DEL_ACK_LOW : TCP_DEL_ACK_HI))))
		goto tcp_param_change_nla_failed;

	if (tcp_adv_win_scl_enabled &&
	    (nla_put_u32(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_ADV_WIN_SCALE,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		 WIN_SCALE_LOW : WIN_SCALE_HI))))
		goto tcp_param_change_nla_failed;

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_DP_ID);
	return;

tcp_param_change_nla_failed:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_DP_ID);
	dp_err("nla_put api failed");
	wlan_cfg80211_vendor_free_skb(vendor_event);
}
#else
static
void osif_dp_send_tcp_param_update_event(struct wlan_objmgr_psoc *psoc,
					 union wlan_tp_data *data,
					 uint8_t dir)
{
}
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

/**
 * osif_dp_get_net_dev_from_vdev() - Get netdev object from vdev
 * @vdev: Pointer to vdev manager
 * @out_net_dev: Pointer to output netdev
 *
 * Return: 0 on success, error code on failure
 */
static int osif_dp_get_net_dev_from_vdev(struct wlan_objmgr_vdev *vdev,
					 struct net_device **out_net_dev)
{
	struct vdev_osif_priv *priv;

	if (!vdev)
		return -EINVAL;

	priv = wlan_vdev_get_ospriv(vdev);
	if (!priv || !priv->wdev || !priv->wdev->netdev)
		return -EINVAL;

	*out_net_dev = priv->wdev->netdev;

	return 0;
}

/**
 * osif_dp_process_mic_error() - Indicate mic error to supplicant
 * @info: MIC error information
 * @vdev: vdev handle
 *
 * Return: None
 */
static void
osif_dp_process_mic_error(struct dp_mic_error_info *info,
			  struct wlan_objmgr_vdev *vdev)
{
	struct net_device *dev;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_dp_get_net_dev_from_vdev(vdev, &dev);
	if (errno) {
		dp_err("failed to get netdev");
		return;
	}
	if (osif_vdev_sync_op_start(dev, &vdev_sync))
		return;

	/* inform mic failure to nl80211 */
	cfg80211_michael_mic_failure(dev,
				     (uint8_t *)&info->ta_mac_addr,
				     info->multicast ?
				     NL80211_KEYTYPE_GROUP :
				     NL80211_KEYTYPE_PAIRWISE,
				     info->key_id,
				     info->tsc,
				     GFP_KERNEL);
	osif_vdev_sync_op_stop(vdev_sync);
}


/**
 * osif_dp_get_arp_stats_event_handler() - ARP get stats event handler
 * @psoc: psoc handle
 * @rsp: Get ARP stats response
 *
 * Return: None
 */
static void osif_dp_get_arp_stats_event_handler(struct wlan_objmgr_psoc *psoc,
						struct dp_rsp_stats *rsp)
{
	struct osif_request *request = NULL;
	void *context;

	context = ucfg_dp_get_arp_request_ctx(psoc);
	if (!context)
		return;

	request = osif_request_get(context);
	if (!request)
		return;

	ucfg_dp_get_arp_stats_event_handler(psoc, rsp);

	osif_request_complete(request);
	osif_request_put(request);
}

#ifdef WLAN_NUD_TRACKING
/**
 * nud_state_osif_to_dp() - convert os_if to enum
 * @curr_state: Current NUD state
 *
 * Return: DP enum equivalent to NUD state
 */
static inline enum dp_nud_state nud_state_osif_to_dp(uint8_t curr_state)
{
	switch (curr_state) {
	case NUD_NONE:
		return DP_NUD_NONE;
	case NUD_INCOMPLETE:
		return DP_NUD_INCOMPLETE;
	case NUD_REACHABLE:
		return DP_NUD_REACHABLE;
	case NUD_STALE:
		return DP_NUD_STALE;
	case NUD_DELAY:
		return DP_NUD_DELAY;
	case NUD_PROBE:
		return DP_NUD_PROBE;
	case NUD_FAILED:
		return DP_NUD_FAILED;
	case NUD_NOARP:
		return DP_NUD_NOARP;
	case NUD_PERMANENT:
		return DP_NUD_PERMANENT;
	default:
		return DP_NUD_STATE_INVALID;
	}
}

/**
 * os_if_dp_nud_stats_info() - print NUD stats info
 * @vdev: vdev handle
 *
 * Return: None
 */
static void os_if_dp_nud_stats_info(struct wlan_objmgr_vdev *vdev)
{
	struct netdev_queue *txq;
	struct net_device *net_dev;
	int i = 0, errno;

	errno = osif_dp_get_net_dev_from_vdev(vdev, &net_dev);
	if (errno) {
		dp_err("failed to get netdev");
		return;
	}
	dp_info("carrier state: %d", netif_carrier_ok(net_dev));

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(net_dev, i);
		dp_info("Queue: %d status: %d txq->trans_start: %lu",
			i, netif_tx_queue_stopped(txq), txq->trans_start);
	}
}

/**
 * os_if_dp_nud_netevent_cb() - netevent callback
 * @nb: Pointer to notifier block
 * @event: Net Event triggered
 * @data: Pointer to neighbour struct
 *
 * Callback for netevent
 *
 * Return: 0 on success
 */
static int os_if_dp_nud_netevent_cb(struct notifier_block *nb,
				    unsigned long event,
				    void *data)
{
	struct neighbour *neighbor = data;
	struct osif_vdev_sync *vdev_sync;
	const struct net_device *netdev = neighbor->dev;
	int errno;

	errno = osif_vdev_sync_op_start(neighbor->dev, &vdev_sync);
	if (errno)
		return errno;

	switch (event) {
	case NETEVENT_NEIGH_UPDATE:
		ucfg_dp_nud_event((struct qdf_mac_addr *)netdev->dev_addr,
				  (struct qdf_mac_addr *)&neighbor->ha[0],
				  nud_state_osif_to_dp(neighbor->nud_state));
		break;
	default:
		break;
	}

	osif_vdev_sync_op_stop(vdev_sync);

	return 0;
}

static struct notifier_block wlan_netevent_nb = {
	.notifier_call = os_if_dp_nud_netevent_cb
};

int osif_dp_nud_register_netevent_notifier(struct wlan_objmgr_psoc *psoc)
{
	int ret = 0;

	if (ucfg_dp_nud_tracking_enabled(psoc)) {
		ret = register_netevent_notifier(&wlan_netevent_nb);
		if (!ret)
			dp_info("Registered netevent notifier");
	}
	return ret;
}

void osif_dp_nud_unregister_netevent_notifier(struct wlan_objmgr_psoc *psoc)
{
	int ret = 0;

	if (ucfg_dp_nud_tracking_enabled(psoc)) {
		ret = unregister_netevent_notifier(&wlan_netevent_nb);
		if (!ret)
			dp_info("Unregistered netevent notifier");
	}
}
#else
static void os_if_dp_nud_stats_info(struct wlan_objmgr_vdev *vdev)
{
}

int osif_dp_nud_register_netevent_notifier(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

void osif_dp_nud_unregister_netevent_notifier(struct wlan_objmgr_psoc *psoc)
{
}
#endif

/**
 * dp_dns_unmake_name_query() - Convert an uncompressed DNS name to a
 *			     NUL-terminated string
 * @name: DNS name
 *
 * Return: Produce a printable version of a DNS name.
 */
static inline uint8_t *dp_dns_unmake_name_query(uint8_t *name)
{
	uint8_t *p;
	unsigned int len;

	p = name;
	while ((len = *p)) {
		*(p++) = '.';
		p += len;
	}

	return name + 1;
}

/**
 * dp_dns_make_name_query() - Convert a standard NUL-terminated string
 *				to DNS name
 * @string: Name as a NUL-terminated string
 * @buf: Buffer in which to place DNS name
 * @len: BUffer length
 *
 * DNS names consist of "<length>element" pairs.
 *
 * Return: Byte following constructed DNS name
 */
static uint8_t *dp_dns_make_name_query(const uint8_t *string,
				       uint8_t *buf, uint8_t len)
{
	uint8_t *length_byte = buf++;
	uint8_t c;

	if (string[len - 1]) {
		dp_err("DNS name is not null terminated");
		return NULL;
	}

	while ((c = *(string++))) {
		if (c == '.') {
			*length_byte = buf - length_byte - 1;
			length_byte = buf;
		}
		*(buf++) = c;
	}
	*length_byte = buf - length_byte - 1;
	*(buf++) = '\0';
	return buf;
}

/**
 * osif_dp_set_clear_connectivity_check_stats_info() - set/clear stats info
 * @vdev: vdev context
 * @arp_stats_params: arp stats structure to be sent to FW
 * @tb: nl attribute
 * @is_set_stats: set/clear stats
 *
 *
 * Return: 0 on success, negative errno on failure
 */
static int osif_dp_set_clear_connectivity_check_stats_info(
		struct wlan_objmgr_vdev *vdev,
		struct dp_set_arp_stats_params *arp_stats_params,
		struct nlattr **tb, bool is_set_stats)
{
	struct nlattr *tb2[CONNECTIVITY_STATS_SET_MAX + 1];
	struct nlattr *curr_attr = NULL;
	int err = 0;
	uint32_t pkt_bitmap;
	int rem;
	uint8_t dns_payload[256];
	uint32_t pkt_type_bitmap = ucfg_dp_get_pkt_type_bitmap_value(vdev);

	/* Set NUD command for start tracking is received. */
	nla_for_each_nested(curr_attr,
			    tb[STATS_SET_DATA_PKT_INFO],
			    rem) {
		if (wlan_cfg80211_nla_parse(tb2,
				CONNECTIVITY_STATS_SET_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				dp_set_connectivity_check_stats)) {
			dp_err("nla_parse failed");
			err = -EINVAL;
			goto end;
		}

		if (tb2[STATS_PKT_INFO_TYPE]) {
			pkt_bitmap = nla_get_u32(tb2[STATS_PKT_INFO_TYPE]);
			if (!pkt_bitmap) {
				dp_err("pkt tracking bitmap is empty");
				err = -EINVAL;
				goto end;
			}

			if (is_set_stats) {
				arp_stats_params->pkt_type_bitmap = pkt_bitmap;
				arp_stats_params->flag = true;
				pkt_type_bitmap |=
					arp_stats_params->pkt_type_bitmap;
				ucfg_dp_set_pkt_type_bitmap_value(vdev,
								  pkt_type_bitmap);

				if (pkt_bitmap & CONNECTIVITY_CHECK_SET_ARP) {
					if (!tb[STATS_GW_IPV4]) {
						dp_err("GW ipv4 address is not present");
						err = -EINVAL;
						goto end;
					}
					arp_stats_params->ip_addr =
						nla_get_u32(tb[STATS_GW_IPV4]);
					arp_stats_params->pkt_type =
						WLAN_NUD_STATS_ARP_PKT_TYPE;
					ucfg_dp_set_track_arp_ip_value(vdev,
								arp_stats_params->ip_addr);
				}

				if (pkt_bitmap & CONNECTIVITY_CHECK_SET_DNS) {
					uint8_t *domain_name;

					if (!tb2[STATS_DNS_DOMAIN_NAME]) {
						dp_err("DNS domain id is not present");
						err = -EINVAL;
						goto end;
					}
					domain_name = nla_data(
						tb2[STATS_DNS_DOMAIN_NAME]);
					ucfg_dp_set_track_dns_domain_len_value(vdev,
						nla_len(tb2[STATS_DNS_DOMAIN_NAME]));
					ucfg_dp_get_dns_payload_value(vdev, dns_payload);
					if (!dp_dns_make_name_query(
						domain_name,
						dns_payload,
						ucfg_dp_get_track_dns_domain_len_value(vdev)))
						ucfg_dp_set_track_dns_domain_len_value(vdev, 0);
					/* DNStracking isn't supported in FW. */
					arp_stats_params->pkt_type_bitmap &=
						~CONNECTIVITY_CHECK_SET_DNS;
				}

				if (pkt_bitmap &
				    CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE) {
					if (!tb2[STATS_SRC_PORT] ||
					    !tb2[STATS_DEST_PORT]) {
						dp_err("Source/Dest port is not present");
						err = -EINVAL;
						goto end;
					}
					arp_stats_params->tcp_src_port =
						nla_get_u32(
							tb2[STATS_SRC_PORT]);
					arp_stats_params->tcp_dst_port =
						nla_get_u32(
							tb2[STATS_DEST_PORT]);
					ucfg_dp_set_track_src_port_value(vdev,
						arp_stats_params->tcp_src_port);
					ucfg_dp_set_track_dest_port_value(vdev,
						arp_stats_params->tcp_dst_port);
				}

				if (pkt_bitmap &
				    CONNECTIVITY_CHECK_SET_ICMPV4) {
					if (!tb2[STATS_DEST_IPV4]) {
						dp_err("destination ipv4 address to track ping packets is not present");
						err = -EINVAL;
						goto end;
					}
					arp_stats_params->icmp_ipv4 =
						nla_get_u32(
							tb2[STATS_DEST_IPV4]);
					ucfg_dp_set_track_dest_ipv4_value(vdev,
						arp_stats_params->icmp_ipv4);
				}
			} else {
				/* clear stats command received */
				arp_stats_params->pkt_type_bitmap = pkt_bitmap;
				arp_stats_params->flag = false;
				pkt_type_bitmap &=
					(~arp_stats_params->pkt_type_bitmap);
				ucfg_dp_set_pkt_type_bitmap_value(vdev, pkt_type_bitmap);

				if (pkt_bitmap & CONNECTIVITY_CHECK_SET_ARP) {
					arp_stats_params->pkt_type =
						WLAN_NUD_STATS_ARP_PKT_TYPE;
					ucfg_dp_clear_arp_stats(vdev);
					ucfg_dp_set_track_arp_ip_value(vdev, 0);
				}

				if (pkt_bitmap & CONNECTIVITY_CHECK_SET_DNS) {
					/* DNStracking isn't supported in FW. */
					arp_stats_params->pkt_type_bitmap &=
						~CONNECTIVITY_CHECK_SET_DNS;
					ucfg_dp_clear_dns_stats(vdev);
					ucfg_dp_clear_dns_payload_value(vdev);
					ucfg_dp_set_track_dns_domain_len_value(vdev, 0);
				}

				if (pkt_bitmap &
				    CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE) {
					ucfg_dp_clear_tcp_stats(vdev);
					ucfg_dp_set_track_src_port_value(vdev,
									 0);
					ucfg_dp_set_track_dest_port_value(vdev,
									  0);
				}

				if (pkt_bitmap &
				    CONNECTIVITY_CHECK_SET_ICMPV4) {
					ucfg_dp_clear_icmpv4_stats(vdev);
					ucfg_dp_set_track_dest_ipv4_value(vdev,
									  0);
				}
			}
		} else {
			dp_err("stats list empty");
			err = -EINVAL;
			goto end;
		}
	}

end:
	return err;
}

/**
 * osif_dp_populate_dns_stats_info() - populate dns stats info
 * @vdev: vdev context
 * @skb: pointer to skb
 *
 *
 * Return: An error code or 0 on success.
 */
static int osif_dp_populate_dns_stats_info(struct wlan_objmgr_vdev *vdev,
					   struct sk_buff *skb)
{
	uint8_t *dns_query;
	uint32_t track_dns_domain_len;
	struct dp_dns_stats *dns_stats = ucfg_dp_get_dns_stats(vdev);

	if (!dns_stats) {
		dp_err("Unable to get DNS stats");
		return -EINVAL;
	}

	track_dns_domain_len = ucfg_dp_get_track_dns_domain_len_value(vdev);
	dns_query = qdf_mem_malloc(track_dns_domain_len + 1);
	if (!dns_query)
		return -EINVAL;

	ucfg_dp_get_dns_payload_value(vdev, dns_query);

	if (nla_put_u16(skb, CHECK_STATS_PKT_TYPE,
			CONNECTIVITY_CHECK_SET_DNS) ||
	    nla_put(skb, CHECK_STATS_PKT_DNS_DOMAIN_NAME,
		    track_dns_domain_len,
		    dp_dns_unmake_name_query(dns_query)) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_FROM_NETDEV,
			dns_stats->tx_dns_req_count) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TO_LOWER_MAC,
			dns_stats->tx_host_fw_sent) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC,
			dns_stats->tx_host_fw_sent) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TX_SUCCESS,
			dns_stats->tx_ack_cnt) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_RX_COUNT_BY_UPPER_MAC,
			dns_stats->rx_dns_rsp_count) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_COUNT_TO_NETDEV,
			dns_stats->rx_delivered) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_COUNT_OUT_OF_ORDER_DROP,
			dns_stats->rx_host_drop)) {
		dp_err("nla put fail");
		qdf_mem_free(dns_query);
		kfree_skb(skb);
		return -EINVAL;
	}
	qdf_mem_free(dns_query);
	return 0;
}

/**
 * osif_dp_populate_tcp_stats_info() - populate tcp stats info
 * @vdev: pointer to vdev context
 * @skb: pointer to skb
 * @pkt_type: tcp pkt type
 *
 * Return: An error code or 0 on success.
 */
static int osif_dp_populate_tcp_stats_info(struct wlan_objmgr_vdev *vdev,
					   struct sk_buff *skb,
					   uint8_t pkt_type)
{
	uint32_t track_src_port = ucfg_dp_get_track_src_port_value(vdev);
	uint32_t track_dest_port = ucfg_dp_get_track_dest_port_value(vdev);
	struct dp_tcp_stats *tcp_stats = ucfg_dp_get_tcp_stats(vdev);

	if (!tcp_stats) {
		dp_err("Unable to get TCP stats");
		return -EINVAL;
	}

	switch (pkt_type) {
	case CONNECTIVITY_CHECK_SET_TCP_SYN:
		/* Fill info for tcp syn packets (tx packet) */
		if (nla_put_u16(skb, CHECK_STATS_PKT_TYPE,
				CONNECTIVITY_CHECK_SET_TCP_SYN) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_SRC_PORT,
				track_src_port) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_DEST_PORT,
				track_dest_port) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_FROM_NETDEV,
				tcp_stats->tx_tcp_syn_count) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TO_LOWER_MAC,
				tcp_stats->tx_tcp_syn_host_fw_sent) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC,
				tcp_stats->tx_tcp_syn_host_fw_sent) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TX_SUCCESS,
				tcp_stats->tx_tcp_syn_ack_cnt)) {
			dp_err("nla put fail");
			kfree_skb(skb);
			return -EINVAL;
		}
		break;
	case CONNECTIVITY_CHECK_SET_TCP_SYN_ACK:
		/* Fill info for tcp syn-ack packets (rx packet) */
		if (nla_put_u16(skb, CHECK_STATS_PKT_TYPE,
				CONNECTIVITY_CHECK_SET_TCP_SYN_ACK) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_SRC_PORT,
				track_src_port) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_DEST_PORT,
				track_dest_port) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_RSP_RX_COUNT_BY_LOWER_MAC,
				tcp_stats->rx_fw_cnt) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_RSP_RX_COUNT_BY_UPPER_MAC,
				tcp_stats->rx_tcp_syn_ack_count) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_RSP_COUNT_TO_NETDEV,
				tcp_stats->rx_delivered) ||
		    nla_put_u16(skb,
				CHECK_STATS_PKT_RSP_COUNT_OUT_OF_ORDER_DROP,
				tcp_stats->rx_host_drop)) {
			dp_err("nla put fail");
			kfree_skb(skb);
			return -EINVAL;
		}
		break;
	case CONNECTIVITY_CHECK_SET_TCP_ACK:
		/* Fill info for tcp ack packets (tx packet) */
		if (nla_put_u16(skb, CHECK_STATS_PKT_TYPE,
				CONNECTIVITY_CHECK_SET_TCP_ACK) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_SRC_PORT,
				track_src_port) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_DEST_PORT,
				track_dest_port) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_FROM_NETDEV,
				tcp_stats->tx_tcp_ack_count) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TO_LOWER_MAC,
				tcp_stats->tx_tcp_ack_host_fw_sent) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC,
				tcp_stats->tx_tcp_ack_host_fw_sent) ||
		    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TX_SUCCESS,
				tcp_stats->tx_tcp_ack_ack_cnt)) {
			dp_err("nla put fail");
			kfree_skb(skb);
			return -EINVAL;
		}
		break;
	default:
		break;
	}
	return 0;
}

/**
 * osif_dp_populate_icmpv4_stats_info() - populate icmpv4 stats
 * @vdev: pointer to vdev context
 * @skb: pointer to skb
 *
 *
 * Return: An error code or 0 on success.
 */
static int osif_dp_populate_icmpv4_stats_info(struct wlan_objmgr_vdev *vdev,
					      struct sk_buff *skb)
{
	struct dp_icmpv4_stats *icmpv4_stats = ucfg_dp_get_icmpv4_stats(vdev);
	uint32_t track_dest_ipv4 = ucfg_dp_get_track_dest_ipv4_value(vdev);

	if (!icmpv4_stats) {
		dp_err("Unable to get ICMP stats");
		return -EINVAL;
	}

	if (nla_put_u16(skb, CHECK_STATS_PKT_TYPE,
			CONNECTIVITY_CHECK_SET_ICMPV4) ||
	    nla_put_u32(skb, CHECK_STATS_PKT_DEST_IPV4,
			track_dest_ipv4) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_FROM_NETDEV,
			icmpv4_stats->tx_icmpv4_req_count) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TO_LOWER_MAC,
			icmpv4_stats->tx_host_fw_sent) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_RX_COUNT_BY_LOWER_MAC,
			icmpv4_stats->tx_host_fw_sent) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_REQ_COUNT_TX_SUCCESS,
			icmpv4_stats->tx_ack_cnt) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_RX_COUNT_BY_LOWER_MAC,
			icmpv4_stats->rx_fw_cnt) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_RX_COUNT_BY_UPPER_MAC,
			icmpv4_stats->rx_icmpv4_rsp_count) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_COUNT_TO_NETDEV,
			icmpv4_stats->rx_delivered) ||
	    nla_put_u16(skb, CHECK_STATS_PKT_RSP_COUNT_OUT_OF_ORDER_DROP,
			icmpv4_stats->rx_host_drop)) {
		dp_err("nla put fail");
		kfree_skb(skb);
		return -EINVAL;
	}
	return 0;
}

/**
 * osif_dp_populate_connectivity_check_stats_info() - Poplulate connectivity
 * stats info
 * @vdev: pointer to vdev context
 * @skb: pointer to skb
 *
 *
 * Return: An error code or 0 on success.
 */
static int
osif_dp_populate_connectivity_check_stats_info(struct wlan_objmgr_vdev *vdev,
					       struct sk_buff *skb)
{
	struct nlattr *connect_stats, *connect_info;
	uint32_t count = 0;
	uint32_t pkt_type_bitmap = ucfg_dp_get_pkt_type_bitmap_value(vdev);

	connect_stats = nla_nest_start(skb, DATA_PKT_STATS);
	if (!connect_stats) {
		dp_err("nla_nest_start failed");
		return -EINVAL;
	}

	if (pkt_type_bitmap & CONNECTIVITY_CHECK_SET_DNS) {
		connect_info = nla_nest_start(skb, count);
		if (!connect_info) {
			dp_err("nla_nest_start failed count %u", count);
			return -EINVAL;
		}

		if (osif_dp_populate_dns_stats_info(vdev, skb))
			goto put_attr_fail;
		nla_nest_end(skb, connect_info);
		count++;
	}

	if (pkt_type_bitmap & CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE) {
		connect_info = nla_nest_start(skb, count);
		if (!connect_info) {
			dp_err("nla_nest_start failed count %u", count);
			return -EINVAL;
		}
		if (osif_dp_populate_tcp_stats_info(vdev, skb,
					CONNECTIVITY_CHECK_SET_TCP_SYN))
			goto put_attr_fail;
		nla_nest_end(skb, connect_info);
		count++;

		connect_info = nla_nest_start(skb, count);
		if (!connect_info) {
			dp_err("nla_nest_start failed count %u", count);
			return -EINVAL;
		}
		if (osif_dp_populate_tcp_stats_info(vdev, skb,
					CONNECTIVITY_CHECK_SET_TCP_SYN_ACK))
			goto put_attr_fail;
		nla_nest_end(skb, connect_info);
		count++;

		connect_info = nla_nest_start(skb, count);
		if (!connect_info) {
			dp_err("nla_nest_start failed count %u", count);
			return -EINVAL;
		}
		if (osif_dp_populate_tcp_stats_info(vdev, skb,
					CONNECTIVITY_CHECK_SET_TCP_ACK))
			goto put_attr_fail;
		nla_nest_end(skb, connect_info);
		count++;
	}

	if (pkt_type_bitmap & CONNECTIVITY_CHECK_SET_ICMPV4) {
		connect_info = nla_nest_start(skb, count);
		if (!connect_info) {
			dp_err("nla_nest_start failed count %u", count);
			return -EINVAL;
		}

		if (osif_dp_populate_icmpv4_stats_info(vdev, skb))
			goto put_attr_fail;
		nla_nest_end(skb, connect_info);
		count++;
	}

	nla_nest_end(skb, connect_stats);
	return 0;

put_attr_fail:
	dp_err("QCA_WLAN_VENDOR_ATTR put fail. count %u", count);
	return -EINVAL;
}

int osif_dp_get_nud_stats(struct wiphy *wiphy,
			  struct wlan_objmgr_vdev *vdev,
			  const void *data, int data_len)
{
	int err = 0;
	struct dp_get_arp_stats_params arp_stats_params;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t pkt_type_bitmap = ucfg_dp_get_pkt_type_bitmap_value(vdev);
	struct sk_buff *skb;
	struct osif_request *request = NULL;
	struct dp_arp_stats *arp_stats;
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	static const struct osif_request_params params = {
		.priv_size = 0,
		.timeout_ms = WLAN_WAIT_TIME_NUD_STATS,
	};

	request = osif_request_alloc(&params);
	if (!request) {
		dp_err("Request allocation failure");
		return -ENOMEM;
	}

	ucfg_dp_set_nud_stats_cb(psoc, osif_request_cookie(request));

	arp_stats_params.pkt_type = WLAN_NUD_STATS_ARP_PKT_TYPE;
	arp_stats_params.vdev_id = ucfg_dp_get_link_id(vdev);

	/* send NUD failure event only when ARP tracking is enabled. */
	if (ucfg_dp_nud_fail_data_stall_evt_enabled() &&
	    !ucfg_dp_nud_tracking_enabled(psoc) &&
	    (pkt_type_bitmap & CONNECTIVITY_CHECK_SET_ARP)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "Data stall due to NUD failure");
		cdp_post_data_stall_event(soc,
					  DATA_STALL_LOG_INDICATOR_FRAMEWORK,
					  DATA_STALL_LOG_NUD_FAILURE,
					  OL_TXRX_PDEV_ID, 0XFF,
					  DATA_STALL_LOG_RECOVERY_TRIGGER_PDR);
	}

	if (QDF_STATUS_SUCCESS !=
	    ucfg_dp_req_get_arp_stats(psoc, &arp_stats_params)) {
		dp_err("Unable to sent ARP stats request");
		err = -EINVAL;
		goto exit;
	}

	err = osif_request_wait_for_response(request);
	if (err) {
		dp_err("timedout while retrieving NUD stats");
		err = -ETIMEDOUT;
		goto exit;
	}

	arp_stats = ucfg_dp_get_arp_stats(vdev);
	if (!arp_stats) {
		dp_err("Unable to get ARP stats");
		err = -EINVAL;
		goto exit;
	}

	skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						       WLAN_NUD_STATS_LEN);
	if (!skb) {
		dp_err("wlan_cfg80211_vendor_cmd_alloc_reply_skb failed");
		err = -ENOMEM;
		goto exit;
	}

	if (nla_put_u16(skb, COUNT_FROM_NETDEV,
			arp_stats->tx_arp_req_count) ||
	    nla_put_u16(skb, COUNT_TO_LOWER_MAC,
			arp_stats->tx_host_fw_sent) ||
	    nla_put_u16(skb, RX_COUNT_BY_LOWER_MAC,
			arp_stats->tx_host_fw_sent) ||
	    nla_put_u16(skb, COUNT_TX_SUCCESS,
			arp_stats->tx_ack_cnt) ||
	    nla_put_u16(skb, RSP_RX_COUNT_BY_LOWER_MAC,
			arp_stats->rx_fw_cnt) ||
	    nla_put_u16(skb, RSP_RX_COUNT_BY_UPPER_MAC,
			arp_stats->rx_arp_rsp_count) ||
	    nla_put_u16(skb, RSP_COUNT_TO_NETDEV,
			arp_stats->rx_delivered) ||
	    nla_put_u16(skb, RSP_COUNT_OUT_OF_ORDER_DROP,
			arp_stats->rx_host_drop_reorder)) {
		dp_err("nla put fail");
		wlan_cfg80211_vendor_free_skb(skb);
		err = -EINVAL;
		goto exit;
	}
	if (ucfg_dp_get_con_status_value(vdev))
		nla_put_flag(skb, AP_LINK_ACTIVE);
	if (ucfg_dp_get_dad_value(vdev))
		nla_put_flag(skb, AP_LINK_DAD);

	/* ARP tracking is done above. */
	pkt_type_bitmap &= ~CONNECTIVITY_CHECK_SET_ARP;

	if (pkt_type_bitmap) {
		if (osif_dp_populate_connectivity_check_stats_info(vdev, skb)) {
			err = -EINVAL;
			goto exit;
		}
	}

	wlan_cfg80211_vendor_cmd_reply(skb);
exit:
	ucfg_dp_clear_nud_stats_cb(psoc);
	osif_request_put(request);
	return err;
}

int osif_dp_set_nud_stats(struct wiphy *wiphy,
			  struct wlan_objmgr_vdev *vdev,
			  const void *data, int data_len)
{
	struct nlattr *tb[STATS_SET_MAX + 1];
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct dp_set_arp_stats_params arp_stats_params = {0};
	uint32_t pkt_type_bitmap = ucfg_dp_get_pkt_type_bitmap_value(vdev);
	int err = 0;

	err = wlan_cfg80211_nla_parse(tb, STATS_SET_MAX, data, data_len,
				      dp_set_nud_stats_policy);
	if (err) {
		dp_err("STATS_SET_START ATTR");
		return err;
	}

	if (tb[STATS_SET_START]) {
		/* tracking is enabled for stats other than arp. */
		if (tb[STATS_SET_DATA_PKT_INFO]) {
			err = osif_dp_set_clear_connectivity_check_stats_info(
						vdev,
						&arp_stats_params, tb, true);
			if (err)
				return -EINVAL;

			/*
			 * if only tracking dns, then don't send
			 * wmi command to FW.
			 */
			if (!arp_stats_params.pkt_type_bitmap)
				return err;
		} else {
			if (!tb[STATS_GW_IPV4]) {
				dp_err("STATS_SET_START CMD");
				return -EINVAL;
			}

			arp_stats_params.pkt_type_bitmap =
						CONNECTIVITY_CHECK_SET_ARP;
			pkt_type_bitmap |=
					arp_stats_params.pkt_type_bitmap;
			ucfg_dp_set_pkt_type_bitmap_value(vdev,
							  pkt_type_bitmap);
			arp_stats_params.flag = true;
			arp_stats_params.ip_addr =
					nla_get_u32(tb[STATS_GW_IPV4]);
			ucfg_dp_set_track_arp_ip_value(vdev,
						       arp_stats_params.ip_addr);
			arp_stats_params.pkt_type = WLAN_NUD_STATS_ARP_PKT_TYPE;
		}
	} else {
		/* clear stats command received. */
		if (tb[STATS_SET_DATA_PKT_INFO]) {
			err = osif_dp_set_clear_connectivity_check_stats_info(
						vdev,
						&arp_stats_params, tb, false);
			if (err)
				return -EINVAL;

			/*
			 * if only tracking dns, then don't send
			 * wmi command to FW.
			 */
			if (!arp_stats_params.pkt_type_bitmap)
				return err;
		} else {
			arp_stats_params.pkt_type_bitmap =
						CONNECTIVITY_CHECK_SET_ARP;
			pkt_type_bitmap &= (~arp_stats_params.pkt_type_bitmap);
			ucfg_dp_set_pkt_type_bitmap_value(vdev,
							  pkt_type_bitmap);
			arp_stats_params.flag = false;
			ucfg_dp_clear_arp_stats(vdev);
			arp_stats_params.pkt_type = WLAN_NUD_STATS_ARP_PKT_TYPE;
		}
	}

	dp_info("STATS_SET_START Received flag %d!", arp_stats_params.flag);

	arp_stats_params.vdev_id = ucfg_dp_get_link_id(vdev);

	if (QDF_STATUS_SUCCESS !=
	    ucfg_dp_req_set_arp_stats(psoc, &arp_stats_params)) {
		dp_err("Unable to set ARP stats!");
		return -EINVAL;
	}
	return err;
}

/*
 * os_if_dp_register_event_handler() - Register osif event handler
 * @psoc: psoc handle
 *
 * Return: None
 */
static void os_if_dp_register_event_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_nb_ops cb_obj = {0};

	cb_obj.osif_dp_get_arp_stats_evt =
		osif_dp_get_arp_stats_event_handler;

	ucfg_dp_register_event_handler(psoc, &cb_obj);
}

void os_if_dp_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct wlan_dp_psoc_callbacks *cb_obj)
{
	cb_obj->osif_dp_send_tcp_param_update_event =
		osif_dp_send_tcp_param_update_event;
	cb_obj->os_if_dp_nud_stats_info = os_if_dp_nud_stats_info;
	cb_obj->osif_dp_process_mic_error = osif_dp_process_mic_error;
	os_if_dp_register_txrx_callbacks(cb_obj);

	ucfg_dp_register_hdd_callbacks(psoc, cb_obj);
	os_if_dp_register_event_handler(psoc);
}

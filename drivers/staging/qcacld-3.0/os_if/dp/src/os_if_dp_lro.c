/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 *  DOC: osif_dp_lro.c
 *  This file contains DP component's LRO osif API implementation
 */
#include "os_if_dp_lro.h"
#include <wlan_objmgr_vdev_obj.h>
#include <linux/inet_lro.h>
#include <linux/list.h>
#include <linux/random.h>
#include <net/tcp.h>

#define LRO_VALID_FIELDS \
	(LRO_DESC | LRO_ELIGIBILITY_CHECKED | LRO_TCP_ACK_NUM | \
	 LRO_TCP_DATA_CSUM | LRO_TCP_SEQ_NUM | LRO_TCP_WIN)

#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750) || \
    defined(QCA_WIFI_KIWI) || defined(QCA_WIFI_WCN6450)
#ifdef WLAN_FEATURE_LRO_CTX_IN_CB
static qdf_lro_ctx_t osif_dp_get_lro_ctx(struct sk_buff *skb)
{
	return (qdf_lro_ctx_t)QDF_NBUF_CB_RX_LRO_CTX(skb);
}
#else
static qdf_lro_ctx_t osif_dp_get_lro_ctx(struct sk_buff *skb)
{
	struct hif_opaque_softc *hif_hdl =
		(struct hif_opaque_softc *)cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_hdl)
		return NULL;

	return hif_get_lro_info(QDF_NBUF_CB_RX_CTX_ID(skb), hif_hdl);
}
#endif

/**
 * osif_dp_lro_rx() - LRO receive function
 * @dev: netdev
 * @nbuf: network buffer
 *
 * Delivers LRO eligible frames to the LRO manager
 *
 * Return: QDF_STATUS_SUCCESS - frame delivered to LRO manager
 * QDF_STATUS_E_FAILURE - frame not delivered
 */
QDF_STATUS osif_dp_lro_rx(qdf_netdev_t dev, qdf_nbuf_t nbuf)
{
	qdf_lro_ctx_t ctx;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct qdf_lro_info info;
	struct net_lro_desc *lro_desc = NULL;
	struct sk_buff * skb = (struct sk_buff *)nbuf;

	if ((dev->features & NETIF_F_LRO) != NETIF_F_LRO)
		return QDF_STATUS_E_NOSUPPORT;

	ctx = osif_dp_get_lro_ctx(skb);
	if (!ctx) {
		osif_err("LRO mgr is NULL");
		return status;
	}

	info.iph = skb->data;
	info.tcph = skb->data + QDF_NBUF_CB_RX_TCP_OFFSET(skb);
	ctx->lro_mgr->dev = dev;
	if (qdf_lro_get_info(ctx, skb, &info, (void **)&lro_desc)) {
		struct net_lro_info dp_lro_info;

		dp_lro_info.valid_fields = LRO_VALID_FIELDS;

		dp_lro_info.lro_desc = lro_desc;
		dp_lro_info.lro_eligible = 1;
		dp_lro_info.tcp_ack_num = QDF_NBUF_CB_RX_TCP_ACK_NUM(skb);
		dp_lro_info.tcp_data_csum =
			 csum_unfold(htons(QDF_NBUF_CB_RX_TCP_CHKSUM(skb)));
		dp_lro_info.tcp_seq_num = QDF_NBUF_CB_RX_TCP_SEQ_NUM(skb);
		dp_lro_info.tcp_win = QDF_NBUF_CB_RX_TCP_WIN(skb);

		lro_receive_skb_ext(ctx->lro_mgr, skb, NULL,
				    &dp_lro_info);

		if (!dp_lro_info.lro_desc->active)
			qdf_lro_desc_free(ctx, lro_desc);

		status = QDF_STATUS_SUCCESS;
	} else {
		qdf_lro_flush_pkt(ctx, &info);
	}
	return status;
}

/**
 * osif_dp_lro_display_stats() - display LRO statistics
 * @vdev: vdev objmgr context
 *
 * Return: none
 */
void osif_dp_lro_display_stats(struct wlan_vdev_objmgr *vdev)
{
	osif_debug("LRO stats is broken, will fix it");
}

QDF_STATUS
osif_dp_lro_set_reset(struct wlan_vdev_objmgr *vdev, uint8_t enable_flag)
{
	struct vdev_osif_priv *osif_priv;
	struct net_device *dev;
	QDF_STATUS status;

	osif_priv  = wlan_vdev_get_ospriv(vdev);
	dev = osif_priv->wdev->netdev;

	status = ucfg_dp_lro_set_reset(vdev, enable_flag);
	if (QDF_IS_STATUS_ERROR(status))
		return 0;

	if (enable_flag)
		dev->features |= NETIF_F_LRO;
	else
		dev->features &= ~NETIF_F_LRO;

	return 0;
}

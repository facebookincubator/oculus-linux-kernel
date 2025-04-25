/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <htt.h>
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_rh_htt.h"
#include "dp_rh_rx.h"
#include "qdf_mem.h"
#include "cdp_txrx_cmn_struct.h"
#include "dp_tx_desc.h"
#include "dp_rh.h"

#define HTT_MSG_BUF_SIZE(msg_bytes) \
	((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

#define HTT_T2H_MSG_BUF_REINIT(_buf, dev)				\
	do {								\
		qdf_nbuf_push_head(_buf, (HTC_HEADER_LEN) +		\
				   HTC_HDR_ALIGNMENT_PADDING);		\
		qdf_nbuf_init_fast((_buf));				\
		qdf_mem_dma_sync_single_for_device(dev,			\
					(QDF_NBUF_CB_PADDR(_buf)),	\
					(skb_end_pointer(_buf) -	\
					(_buf)->data),			\
					PCI_DMA_FROMDEVICE);		\
	} while (0)

/**
 * dp_htt_flow_pool_map_handler_rh() - HTT_T2H_MSG_TYPE_FLOW_POOL_MAP handler
 * @soc: Handle to DP Soc structure
 * @flow_id: flow id
 * @flow_type: flow type
 * @flow_pool_id: pool id
 * @flow_pool_size: pool size
 *
 * Return: QDF_STATUS_SUCCESS - success, others - failure
 */
static QDF_STATUS
dp_htt_flow_pool_map_handler_rh(struct dp_soc *soc, uint8_t flow_id,
				uint8_t flow_type, uint8_t flow_pool_id,
				uint32_t flow_pool_size)
{
	struct dp_vdev *vdev;
	struct dp_pdev *pdev;
	QDF_STATUS status;

	if (flow_pool_id >= MAX_TXDESC_POOLS) {
		dp_err("invalid flow_pool_id %d", flow_pool_id);
		return QDF_STATUS_E_INVAL;
	}

	vdev = dp_vdev_get_ref_by_id(soc, flow_id, DP_MOD_ID_HTT);
	if (vdev) {
		pdev = vdev->pdev;
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_HTT);
	} else {
		pdev = soc->pdev_list[0];
	}

	status = dp_tx_flow_pool_map_handler(pdev, flow_id, flow_type,
					     flow_pool_id, flow_pool_size);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("failed to create tx flow pool %d", flow_pool_id);
		goto err_out;
	}

	return QDF_STATUS_SUCCESS;

err_out:
	/* TODO: is assert needed ? */
	qdf_assert_always(0);
	return status;
}

/**
 * dp_htt_flow_pool_unmap_handler_rh() - HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP handler
 * @soc: Handle to DP Soc structure
 * @flow_id: flow id
 * @flow_type: flow type
 * @flow_pool_id: pool id
 *
 * Return: none
 */
static void
dp_htt_flow_pool_unmap_handler_rh(struct dp_soc *soc, uint8_t flow_id,
				  uint8_t flow_type, uint8_t flow_pool_id)
{
	struct dp_vdev *vdev;
	struct dp_pdev *pdev;

	if (flow_pool_id >= MAX_TXDESC_POOLS) {
		dp_err("invalid flow_pool_id %d", flow_pool_id);
		return;
	}

	vdev = dp_vdev_get_ref_by_id(soc, flow_id, DP_MOD_ID_HTT);
	if (vdev) {
		pdev = vdev->pdev;
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_HTT);
	} else {
		pdev = soc->pdev_list[0];
	}

	dp_tx_flow_pool_unmap_handler(pdev, flow_id, flow_type,
				      flow_pool_id);
}

/*
 * dp_htt_h2t_send_complete_free_netbuf() - Free completed buffer
 * @soc:	SOC handle
 * @status:	Completion status
 * @netbuf:	HTT buffer
 */
static void
dp_htt_h2t_send_complete_free_netbuf(
	void *soc, A_STATUS status, qdf_nbuf_t netbuf)
{
	qdf_nbuf_free(netbuf);
}

QDF_STATUS dp_htt_h2t_rx_ring_rfs_cfg(struct htt_soc *soc)
{
	struct dp_htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	QDF_STATUS status;
	uint8_t *htt_logger_bufp;

	/*
	 * TODO check do we need ini support in Evros
	 * Receive flow steering configuration,
	 * disable gEnableFlowSteering(=0) in ini if
	 * FW doesn't support it
	 */

	/* reserve room for the HTC header */
	msg = qdf_nbuf_alloc(soc->osdev,
			     HTT_MSG_BUF_SIZE(HTT_RFS_CFG_REQ_BYTES),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     true);
	if (!msg) {
		dp_err("htt_msg alloc failed for RFS config");
		return QDF_STATUS_E_NOMEM;
	}
	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to qdf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	qdf_nbuf_put_tail(msg, HTT_RFS_CFG_REQ_BYTES);

	/* fill in the message contents */
	msg_word = (uint32_t *)qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	*msg_word = 0;
	htt_logger_bufp = (uint8_t *)msg_word;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RFS_CONFIG);
	HTT_RX_RFS_CONFIG_SET(*msg_word, 1);

	/*
	 * TODO value should be obtained from ini maxMSDUsPerRxInd
	 * currently this ini is legacy ol and available only from cds
	 * make this ini common to HL and evros DP
	 */
	*msg_word |= ((32 & 0xff) << 16);

	dp_htt_info("RFS sent to F.W: 0x%08x", *msg_word);

	/*start*/
	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt) {
		qdf_nbuf_free(msg);
		return QDF_STATUS_E_NOMEM;
	}

	pkt->soc_ctxt = NULL; /* not used during send-done callback */
	SET_HTC_PACKET_INFO_TX(
		&pkt->htc_pkt,
		dp_htt_h2t_send_complete_free_netbuf,
		qdf_nbuf_data(msg),
		qdf_nbuf_len(msg),
		soc->htc_endpoint,
		HTC_TX_PACKET_TAG_RUNTIME_PUT); /* tag for no FW response msg */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	status = DP_HTT_SEND_HTC_PKT(soc, pkt, HTT_H2T_MSG_TYPE_RFS_CONFIG,
				     htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;
}

static void
dp_htt_rx_addba_handler_rh(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t tid, uint16_t win_sz)
{
}

static QDF_STATUS
dp_htt_rx_delba_ind_handler_rh(void *soc_handle, uint16_t peer_id,
			       uint8_t tid, uint16_t win_sz)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_htt_t2h_msg_handler_fast() -  Fastpath specific message handler
 * @context: HTT context
 * @cmpl_msdus: netbuf completions
 * @num_cmpls: number of completions to be handled
 *
 * Return: None
 */
static void
dp_htt_t2h_msg_handler_fast(void *context, qdf_nbuf_t *cmpl_msdus,
			    uint32_t num_cmpls)
{
	struct htt_soc *soc = (struct htt_soc *)context;
	qdf_nbuf_t htt_t2h_msg;
	uint32_t *msg_word;
	uint32_t i;
	enum htt_t2h_msg_type msg_type;
	uint32_t msg_len;

	for (i = 0; i < num_cmpls; i++) {
		htt_t2h_msg = cmpl_msdus[i];
		msg_len = qdf_nbuf_len(htt_t2h_msg);

		/*
		 * Move the data pointer to point to HTT header
		 * past the HTC header + HTC header alignment padding
		 */
		qdf_nbuf_pull_head(htt_t2h_msg, HTC_HEADER_LEN +
				   HTC_HDR_ALIGNMENT_PADDING);

		msg_word = (uint32_t *)qdf_nbuf_data(htt_t2h_msg);
		msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);

		switch (msg_type) {
		case HTT_T2H_MSG_TYPE_RX_DATA_IND:
		{
			uint16_t vdev_id, msdu_cnt;
			uint16_t peer_id, frag_ind;

			peer_id = HTT_RX_DATA_IND_PEER_ID_GET(*msg_word);
			frag_ind = HTT_RX_DATA_IND_FRAG_GET(*(msg_word + 1));
			vdev_id = HTT_RX_DATA_IND_VDEV_ID_GET(*msg_word);

			if (qdf_unlikely(frag_ind)) {
				dp_rx_frag_indication_handler(soc->dp_soc,
							      htt_t2h_msg,
							      vdev_id, peer_id);
				break;
			}

			msdu_cnt =
				HTT_RX_DATA_IND_MSDU_CNT_GET(*(msg_word + 1));
			dp_rx_data_indication_handler(soc->dp_soc, htt_t2h_msg,
						      vdev_id, peer_id,
						      msdu_cnt);
			break;
		}
		case HTT_T2H_MSG_TYPE_SOFT_UMAC_TX_COMPL_IND:
		{
			uint32_t num_msdus;

			num_msdus = HTT_SOFT_UMAC_TX_COMP_IND_MSDU_COUNT_GET(*msg_word);

			if ((num_msdus * HTT_TX_MSDU_INFO_SIZE +
			     HTT_SOFT_UMAC_TX_COMPL_IND_SIZE) > msg_len) {
				dp_htt_err("Invalid msdu count in tx compl indication %d", num_msdus);
				break;
			}

			dp_tx_compl_handler_rh(soc->dp_soc, htt_t2h_msg);
			break;
		}
		case HTT_T2H_MSG_TYPE_RX_PN_IND:
		{
			/* TODO check and add PN IND handling */
			break;
		}
		case HTT_T2H_MSG_TYPE_RX_ADDBA:
		{
			uint16_t peer_id;
			uint8_t tid;
			uint16_t win_sz;

			/*
			 * Update REO Queue Desc with new values
			 */
			peer_id = HTT_RX_ADDBA_PEER_ID_GET(*msg_word);
			tid = HTT_RX_ADDBA_TID_GET(*msg_word);
			win_sz = HTT_RX_ADDBA_WIN_SIZE_GET(*msg_word);

			/*
			 * Window size needs to be incremented by 1
			 * since fw needs to represent a value of 256
			 * using just 8 bits
			 */
			dp_htt_rx_addba_handler_rh(soc->dp_soc, peer_id,
						   tid, win_sz + 1);
			break;
		}
		case HTT_T2H_MSG_TYPE_RX_DELBA:
		{
			uint16_t peer_id;
			uint8_t tid;
			uint8_t win_sz;
			QDF_STATUS status;

			peer_id = HTT_RX_DELBA_PEER_ID_GET(*msg_word);
			tid = HTT_RX_DELBA_TID_GET(*msg_word);
			win_sz = HTT_RX_DELBA_WIN_SIZE_GET(*msg_word);

			status = dp_htt_rx_delba_ind_handler_rh(soc->dp_soc,
								peer_id, tid,
								win_sz);

			dp_htt_info("DELBA PeerID %d BAW %d TID %d stat %d",
				    peer_id, win_sz, tid, status);
			break;
		}
		case HTT_T2H_MSG_TYPE_PPDU_STATS_IND:
		{
			qdf_nbuf_t nbuf_copy;
			HTC_PACKET htc_pkt = {0};

			nbuf_copy = qdf_nbuf_copy(htt_t2h_msg);
			if (qdf_unlikely(!nbuf_copy)) {
				dp_htt_err("NBUF copy failed for PPDU stats msg");
				break;
			}
			htc_pkt.Status = QDF_STATUS_SUCCESS;
			htc_pkt.pPktContext = (void *)nbuf_copy;
			dp_htt_t2h_msg_handler(context, &htc_pkt);
			break;
		}
		case HTT_T2H_MSG_TYPE_FLOW_POOL_MAP:
		{
			uint8_t num_flows;
			struct htt_flow_pool_map_payload_t *pool_map;

			num_flows = HTT_FLOW_POOL_MAP_NUM_FLOWS_GET(*msg_word);

			if (((HTT_FLOW_POOL_MAP_PAYLOAD_SZ /
			      HTT_FLOW_POOL_MAP_HEADER_SZ) * num_flows + 1) * sizeof(*msg_word) > msg_len) {
				dp_htt_err("Invalid flow count in flow pool map message");
				WARN_ON(1);
				break;
			}

			msg_word++;

			while (num_flows) {
				pool_map = (struct htt_flow_pool_map_payload_t *)msg_word;
				dp_htt_flow_pool_map_handler_rh(
					soc->dp_soc, pool_map->flow_id,
					pool_map->flow_type,
					pool_map->flow_pool_id,
					pool_map->flow_pool_size);

				msg_word += (HTT_FLOW_POOL_MAP_PAYLOAD_SZ /
							 HTT_FLOW_POOL_MAP_HEADER_SZ);
				num_flows--;
			}

			break;
		}
		case HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP:
		{
			struct htt_flow_pool_unmap_t *pool_unmap;

			if (msg_len < sizeof(struct htt_flow_pool_unmap_t)) {
				dp_htt_err("Invalid length in flow pool unmap message %d", msg_len);
				WARN_ON(1);
				break;
			}

			pool_unmap = (struct htt_flow_pool_unmap_t *)msg_word;
			dp_htt_flow_pool_unmap_handler_rh(
				soc->dp_soc, pool_unmap->flow_id,
				pool_unmap->flow_type,
				pool_unmap->flow_pool_id);
			break;
		}
		default:
		{
			HTC_PACKET htc_pkt = {0};

			htc_pkt.Status = QDF_STATUS_SUCCESS;
			htc_pkt.pPktContext = (void *)htt_t2h_msg;
			/*
			 * Increment user count to protect buffer
			 * from generic handler free count will be
			 * reset to 1 during MSG_BUF_REINIT
			 */
			qdf_nbuf_inc_users(htt_t2h_msg);
			dp_htt_t2h_msg_handler(context, &htc_pkt);
			break;
		}
		}

		/* Re-initialize the indication buffer */
		HTT_T2H_MSG_BUF_REINIT(htt_t2h_msg, soc->osdev);
		qdf_nbuf_set_pktlen(htt_t2h_msg, 0);
	}
}

static QDF_STATUS
dp_htt_htc_attach(struct htt_soc *soc, uint16_t service_id)
{
	struct dp_soc_rh *rh_soc = dp_get_rh_soc_from_dp_soc(soc->dp_soc);
	struct htc_service_connect_req connect;
	struct htc_service_connect_resp response;
	QDF_STATUS status;

	qdf_mem_zero(&connect, sizeof(connect));
	qdf_mem_zero(&response, sizeof(response));

	connect.pMetaData = NULL;
	connect.MetaDataLength = 0;
	connect.EpCallbacks.pContext = soc;
	connect.EpCallbacks.EpTxComplete = dp_htt_h2t_send_complete;
	connect.EpCallbacks.EpTxCompleteMultiple = NULL;
	/* fastpath handler will be used instead */
	connect.EpCallbacks.EpRecv = NULL;

	/* rx buffers currently are provided by HIF, not by EpRecvRefill */
	connect.EpCallbacks.EpRecvRefill = NULL;
	/* N/A, fill is done by HIF */
	connect.EpCallbacks.RecvRefillWaterMark = 1;

	connect.EpCallbacks.EpSendFull = dp_htt_h2t_full;
	/*
	 * Specify how deep to let a queue get before htc_send_pkt will
	 * call the EpSendFull function due to excessive send queue depth.
	 */
	connect.MaxSendQueueDepth = DP_HTT_MAX_SEND_QUEUE_DEPTH;

	/* disable flow control for HTT data message service */
	connect.ConnectionFlags |= HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;

	/* connect to control service */
	connect.service_id = service_id;

	status = htc_connect_service(soc->htc_soc, &connect, &response);

	if (status != QDF_STATUS_SUCCESS) {
		dp_htt_err("HTC connect svc failed for id:%u", service_id);
		return status;
	}

	if (service_id == HTT_DATA_MSG_SVC)
		soc->htc_endpoint = response.Endpoint;

	/* Save the EP_ID of the TX pipe that to be used during TX enqueue */
	if (service_id == HTT_DATA2_MSG_SVC)
		rh_soc->tx_endpoint = response.Endpoint;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_htt_htc_soc_attach_all(struct htt_soc *soc)
{
	struct dp_soc *dp_soc = soc->dp_soc;
	int svc_list[3] = {HTT_DATA_MSG_SVC, HTT_DATA2_MSG_SVC,
		HTT_DATA3_MSG_SVC};
	QDF_STATUS status;
	int i;

	for (i = 0; i < QDF_ARRAY_SIZE(svc_list); i++) {
		status = dp_htt_htc_attach(soc, svc_list[i]);
		if (QDF_IS_STATUS_ERROR(status))
			return status;
	}

	dp_hif_update_pipe_callback(dp_soc, (void *)soc,
				    dp_htt_hif_t2h_hp_callback,
				    DP_HTT_T2H_HP_PIPE);

	/* Register fastpath cb handlers for RX CE's */
	if (hif_ce_fastpath_cb_register(dp_soc->hif_handle,
					dp_htt_t2h_msg_handler_fast, soc)) {
		dp_htt_err("failed to register fastpath callback");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_htt_soc_initialize_rh() - SOC level HTT initialization
 * @htt_soc: Opaque htt SOC handle
 * @ctrl_psoc: Opaque ctrl SOC handle
 * @htc_soc: SOC level HTC handle
 * @hal_soc: Opaque HAL SOC handle
 * @osdev: QDF device
 *
 * Return: HTT handle on success; NULL on failure
 */
void *
dp_htt_soc_initialize_rh(struct htt_soc *htt_soc,
			 struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			 HTC_HANDLE htc_soc,
			 hal_soc_handle_t hal_soc_hdl, qdf_device_t osdev)
{
	struct htt_soc *soc = (struct htt_soc *)htt_soc;

	soc->osdev = osdev;
	soc->ctrl_psoc = ctrl_psoc;
	soc->htc_soc = htc_soc;
	soc->hal_soc = hal_soc_hdl;

	if (dp_htt_htc_soc_attach_all(soc))
		goto fail2;

	return soc;

fail2:
	return NULL;
}

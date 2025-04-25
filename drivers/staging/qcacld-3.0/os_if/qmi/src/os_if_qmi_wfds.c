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

#include "os_if_qmi.h"
#include "os_if_qmi_wifi_driver_service_v01.h"
#include <qdf_util.h>
#include "wlan_qmi_public_struct.h"
#include "wlan_dp_ucfg_api.h"

static struct qmi_handle qmi_wfds;

/*
 * os_if_ce_dir_qmi_to_wfds_type() - Convert ce direction from internal
 *  type to type used in QMI message
 * @ce_dir: internal ce direction
 *
 * Return: ce direction in QMI type
 */
static enum wifi_drv_qmi_pipe_dir_v01
os_if_ce_dir_qmi_to_wfds_type(enum wlan_qmi_wfds_pipe_dir ce_dir)
{
	switch (ce_dir) {
	case QMI_WFDS_PIPEDIR_NONE:
		return WFDS_PIPEDIR_NONE_V01;
	case QMI_WFDS_PIPEDIR_IN:
		return WFDS_PIPEDIR_IN_V01;
	case QMI_WFDS_PIPEDIR_OUT:
		return WFDS_PIPEDIR_OUT_V01;
	default:
		return WIFI_DRV_QMI_PIPE_DIR_MAX_VAL_V01;
	}
}

/*
 * os_if_srng_dir_qmi_to_wfds_type() - Convert srng direction from internal
 *  type to type used in QMI message
 * @srng_dir: internal srng direction
 *
 * Return: srng direction in QMI type
 */
static enum wifi_drv_qmi_srng_direction_v01
os_if_srng_dir_qmi_to_wfds_type(enum wlan_qmi_wfds_srng_dir srng_dir)
{
	switch (srng_dir) {
	case QMI_WFDS_SRNG_SOURCE_RING:
		return WFDS_SRNG_SOURCE_RING_V01;
	case QMI_WFDS_SRNG_DESTINATION_RING:
		return WFDS_SRNG_DESTINATION_RING_V01;
	default:
		return WIFI_DRV_QMI_SRNG_DIRECTION_MAX_VAL_V01;
	}
}

/*
 * os_if_qmi_wfds_send_config_msg() - Send config message to QMI server
 *  to QMI server
 * @src_info: source information to be filled in QMI message
 *
 * Return: QDF status
 */
static QDF_STATUS
os_if_qmi_wfds_send_config_msg(struct wlan_qmi_wfds_config_req_msg *src_info)
{
	struct wfds_config_req_msg_v01 *req;
	struct wfds_gen_resp_msg_v01 *resp;
	struct qmi_txn txn;
	QDF_STATUS status;
	uint8_t i;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_mem_free(req);
		return QDF_STATUS_E_NOMEM;
	}

	if (src_info->ce_info_len > QMI_WFDS_CE_MAX_SRNG) {
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	req->ce_info_len = src_info->ce_info_len;
	for (i = 0; i < req->ce_info_len; i++) {
		req->ce_info[i].ce_id = src_info->ce_info[i].ce_id;
		req->ce_info[i].ce_dir =
		     os_if_ce_dir_qmi_to_wfds_type(src_info->ce_info[i].ce_dir);
		req->ce_info[i].srng_info.ring_id =
			src_info->ce_info[i].srng_info.ring_id;
		req->ce_info[i].srng_info.dir =
			os_if_srng_dir_qmi_to_wfds_type(src_info->ce_info[i].srng_info.dir);
		req->ce_info[i].srng_info.num_entries =
			src_info->ce_info[i].srng_info.num_entries;
		req->ce_info[i].srng_info.entry_size =
			src_info->ce_info[i].srng_info.entry_size;
		req->ce_info[i].srng_info.ring_base_paddr =
			src_info->ce_info[i].srng_info.ring_base_paddr;
		req->ce_info[i].srng_info.hp_paddr =
			src_info->ce_info[i].srng_info.hp_paddr;
		req->ce_info[i].srng_info.tp_paddr =
			src_info->ce_info[i].srng_info.tp_paddr;
	}

	req->rx_refill_ring.ring_id = src_info->rx_refill_ring.ring_id;
	req->rx_refill_ring.dir =
		os_if_srng_dir_qmi_to_wfds_type(src_info->rx_refill_ring.dir);
	req->rx_refill_ring.num_entries = src_info->rx_refill_ring.num_entries;
	req->rx_refill_ring.entry_size = src_info->rx_refill_ring.entry_size;
	req->rx_refill_ring.ring_base_paddr =
				src_info->rx_refill_ring.ring_base_paddr;
	req->rx_refill_ring.hp_paddr = src_info->rx_refill_ring.hp_paddr;
	req->rx_refill_ring.tp_paddr = src_info->rx_refill_ring.tp_paddr;

	req->shadow_rdptr_mem_paddr = src_info->shadow_rdptr_mem_paddr;
	req->shadow_rdptr_mem_size = src_info->shadow_rdptr_mem_size;
	req->shadow_wrptr_mem_paddr = src_info->shadow_wrptr_mem_paddr;
	req->shadow_wrptr_mem_size = src_info->shadow_wrptr_mem_size;
	req->rx_pkt_tlv_len = src_info->rx_pkt_tlv_len;
	req->rx_rbm = src_info->rx_rbm;
	req->pcie_bar_pa = src_info->pcie_bar_pa;
	req->pci_slot = src_info->pci_slot;
	req->lpass_ep_id = src_info->lpass_ep_id;

	status = os_if_qmi_txn_init(&qmi_wfds, &txn, wfds_gen_resp_msg_v01_ei,
				    resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI txn init failed for WFDS config message %d",
			  status);
		goto out;
	}

	status = os_if_qmi_send_request(&qmi_wfds, NULL, &txn,
					QMI_WFDS_CONFIG_REQ_V01,
					WFDS_CONFIG_REQ_MSG_V01_MAX_MSG_LEN,
					wfds_config_req_msg_v01_ei, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI WFDS config send request failed %d", status);
		os_if_qmi_txn_cancel(&txn);
		goto out;
	}

	status = os_if_qmi_txn_wait(&txn, QMI_WFDS_TIMEOUT_JF);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("Wait for QMI WFDS config response timed out %d",
			  status);
		goto out;
	}

	qdf_assert(resp->resp.result == QMI_RESULT_SUCCESS_V01);

out:
	qdf_mem_free(resp);
	qdf_mem_free(req);

	return status;
}

/*
 * os_if_qmi_wfds_send_req_mem_msg() - Send Request Memory message to QMI server
 * @src_info: source information to be filled in QMI message
 *
 * Return: QDF status
 */
static QDF_STATUS
os_if_qmi_wfds_send_req_mem_msg(struct wlan_qmi_wfds_mem_req_msg *src_info)
{
	struct wfds_mem_req_msg_v01 *req;
	struct wfds_gen_resp_msg_v01 *resp;
	struct qmi_txn txn;
	QDF_STATUS status;
	uint8_t i;
	uint16_t j;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_mem_free(req);
		return QDF_STATUS_E_NOMEM;
	}

	if (src_info->mem_arena_page_info_len > QMI_WFDS_MEM_ARENA_MAX) {
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	req->mem_arena_page_info_len = src_info->mem_arena_page_info_len;
	for (i = 0; i < req->mem_arena_page_info_len; i++) {
		req->mem_arena_page_info[i].num_entries_per_page =
			src_info->mem_arena_page_info[i].num_entries_per_page;
		req->mem_arena_page_info[i].page_dma_addr_len =
			src_info->mem_arena_page_info[i].page_dma_addr_len;

		if (src_info->mem_arena_page_info[i].page_dma_addr_len >
		    QMI_WFDS_PAGE_INFO_ARRAY_MAX_SIZE) {
			status = QDF_STATUS_E_INVAL;
			goto out;
		}

		for (j = 0; j < req->mem_arena_page_info[i].page_dma_addr_len;
		     j++)
			req->mem_arena_page_info[i].page_dma_addr[j] =
			      src_info->mem_arena_page_info[i].page_dma_addr[j];
	}

	status = os_if_qmi_txn_init(&qmi_wfds, &txn, wfds_gen_resp_msg_v01_ei,
				    resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI txn init failed for WFDS mem req message %d",
			  status);
		goto out;
	}

	status = os_if_qmi_send_request(&qmi_wfds, NULL, &txn,
					QMI_WFDS_MEM_REQ_V01,
					WFDS_MEM_REQ_MSG_V01_MAX_MSG_LEN,
					wfds_mem_req_msg_v01_ei, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI WFDS mem request send failed %d", status);
		os_if_qmi_txn_cancel(&txn);
		goto out;
	}

	status = os_if_qmi_txn_wait(&txn, QMI_WFDS_TIMEOUT_JF);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("Wait for QMI WFDS mem response timed out %d",
			  status);
		goto out;
	}

	qdf_assert(resp->resp.result == QMI_RESULT_SUCCESS_V01);

out:
	qdf_mem_free(resp);
	qdf_mem_free(req);

	return status;
}

/*
 * os_if_qmi_wfds_send_misc_req_msg() - Send misc req message to QMI server
 * @is_ssr: true if SSR is in progress else false
 *
 * Return: QDF status
 */
static QDF_STATUS
os_if_qmi_wfds_send_misc_req_msg(bool is_ssr)
{
	struct wfds_misc_req_msg_v01 *req;
	struct wfds_gen_resp_msg_v01 *resp;
	struct qmi_txn txn;
	QDF_STATUS status;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_mem_free(req);
		return QDF_STATUS_E_NOMEM;
	}

	req->event = (is_ssr) ? WFDS_EVENT_WLAN_SSR_V01 :
			WFDS_EVENT_WLAN_HOST_RMMOD_V01;

	status = os_if_qmi_txn_init(&qmi_wfds, &txn, wfds_gen_resp_msg_v01_ei,
				    resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI txn for WFDS misc request failed %d",
			  status);
		goto out;
	}

	status = os_if_qmi_send_request(&qmi_wfds, NULL, &txn,
					QMI_WFDS_MISC_REQ_V01,
					WFDS_MISC_REQ_MSG_V01_MAX_MSG_LEN,
					wfds_misc_req_msg_v01_ei, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI WFDS misc request send failed %d", status);
		os_if_qmi_txn_cancel(&txn);
		goto out;
	}

	status = os_if_qmi_txn_wait(&txn, QMI_WFDS_TIMEOUT_JF);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("Failed to wait for WFDS misc response %d", status);
		goto out;
	}

	qdf_assert(resp->resp.result == QMI_RESULT_SUCCESS_V01);

out:
	qdf_mem_free(resp);
	qdf_mem_free(req);

	return status;
}

/*
 * os_if_srng_dir_qmi_to_wfds_type() - Convert status from internal
 *  type to type used in QMI message
 * @status: internal status
 *
 * Return: status in QMI type
 */
static uint8_t
os_if_status_qmi_to_wfds_type(enum wlan_qmi_wfds_status status)
{
	switch (status) {
	case QMI_WFDS_STATUS_SUCCESS:
		return QMI_RESULT_SUCCESS_V01;
	case QMI_WFDS_STATUS_FAILURE:
	default:
		return QMI_RESULT_FAILURE_V01;
	}
}

/*
 * os_if_qmi_wfds_ipcc_map_n_cfg_msg() - Send the IPCC map and configure message
 *  to QMI server
 * @src_info: source information to be filled in QMI message
 *
 * Return: QDF status
 */
static QDF_STATUS
os_if_qmi_wfds_ipcc_map_n_cfg_msg(struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg *src_info)
{
	struct wfds_ipcc_map_n_cfg_req_msg_v01 *req;
	struct wfds_gen_resp_msg_v01 *resp;
	struct qmi_txn txn;
	QDF_STATUS status;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_mem_free(req);
		return QDF_STATUS_E_NOMEM;
	}

	req->status = os_if_status_qmi_to_wfds_type(src_info->status);

	status = os_if_qmi_txn_init(&qmi_wfds, &txn, wfds_gen_resp_msg_v01_ei,
				    resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI txn init failed for WFDS ipcc cfg req message %d",
			  status);
		goto out;
	}

	status = os_if_qmi_send_request(&qmi_wfds, NULL, &txn,
				    QMI_WFDS_IPCC_MAP_N_CFG_REQ_V01,
				    WFDS_IPCC_MAP_N_CFG_REQ_MSG_V01_MAX_MSG_LEN,
				    wfds_ipcc_map_n_cfg_req_msg_v01_ei, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI WFDS IPCC cfg request send failed %d", status);
		os_if_qmi_txn_cancel(&txn);
		goto out;
	}

	status = os_if_qmi_txn_wait(&txn, QMI_WFDS_TIMEOUT_JF);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("Wait for QMI WFDS IPCC response timed out %d",
			  status);
		goto out;
	}

	qdf_assert(resp->resp.result == QMI_RESULT_SUCCESS_V01);

out:
	qdf_mem_free(resp);
	qdf_mem_free(req);

	return status;
}

/*
 * os_if_qmi_wfds_request_mem_ind_cb() - Process request memory indication
 *  received from QMI server
 * @qmi_hdl: QMI handle
 * @sq: pointer to QRTR sock address
 * @qmi_txn: pointer to QMI transaction
 * @data: pointer to QMI data
 *
 * Return: None
 */
static void os_if_qmi_wfds_request_mem_ind_cb(struct qmi_handle *qmi_hdl,
					      struct sockaddr_qrtr *sq,
					      struct qmi_txn *qmi_txn,
					      const void *data)
{
	struct wfds_mem_ind_msg_v01 *src_info =
				(struct wfds_mem_ind_msg_v01 *)data;
	struct wlan_qmi_wfds_mem_ind_msg mem_ind_msg = {0};
	uint8_t i;

	if (!qmi_hdl || !qmi_txn)
		return;

	if (src_info->mem_arena_info_len > QMI_WFDS_MEM_ARENA_MAX) {
		osif_info("Memory arena information array size %d exceeds max length",
			  src_info->mem_arena_info_len);
		return;
	}

	mem_ind_msg.mem_arena_info_len = src_info->mem_arena_info_len;
	for (i = 0; i < src_info->mem_arena_info_len; i++) {
		mem_ind_msg.mem_arena_info[i].entry_size =
				src_info->mem_arena_info[i].entry_size;
		mem_ind_msg.mem_arena_info[i].num_entries =
				src_info->mem_arena_info[i].num_entries;
	}

	ucfg_dp_wfds_handle_request_mem_ind(&mem_ind_msg);
}

/*
 * os_if_wfds_ipcc_map_n_cfg_ind_cb() - Process IPCC map and configure
 *  indication received from QMI server
 * @qmi_hdl: QMI handle
 * @sq: pointer to QRTR sock address
 * @qmi_txn: pointer to QMI transaction
 * @data: pointer to QMI data
 *
 * Return: None
 */
static void os_if_wfds_ipcc_map_n_cfg_ind_cb(struct qmi_handle *qmi_hdl,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *qmi_txn,
					     const void *data)
{
	struct wfds_ipcc_map_n_cfg_ind_msg_v01 *src_info =
		(struct wfds_ipcc_map_n_cfg_ind_msg_v01 *)data;
	struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg ipcc_ind_msg = {0};
	uint8_t i;

	if (!qmi_hdl || !qmi_txn)
		return;

	if (src_info->ipcc_ce_info_len > QMI_WFDS_CE_MAX_SRNG) {
		osif_info("IPCC CE information array size %d exceeds max length",
			  src_info->ipcc_ce_info_len);
		return;
	}

	ipcc_ind_msg.ipcc_ce_info_len = src_info->ipcc_ce_info_len;
	for (i = 0; i < src_info->ipcc_ce_info_len; i++) {
		ipcc_ind_msg.ipcc_ce_info[i].ce_id =
				src_info->ipcc_ce_info[i].ce_id;
		ipcc_ind_msg.ipcc_ce_info[i].ipcc_trig_addr =
				src_info->ipcc_ce_info[i].ipcc_trig_addr;
		ipcc_ind_msg.ipcc_ce_info[i].ipcc_trig_data =
				src_info->ipcc_ce_info[i].ipcc_trig_data;
	}

	ucfg_dp_wfds_handle_ipcc_map_n_cfg_ind(&ipcc_ind_msg);
}

QDF_STATUS
os_if_qmi_wfds_send_ut_cmd_req_msg(struct os_if_qmi_wfds_ut_cmd_info *cmd_info)
{
	struct wfds_ut_cmd_req_msg_v01 *req;
	struct wfds_gen_resp_msg_v01 *resp;
	struct qmi_txn txn;
	QDF_STATUS status;
	int i;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_mem_free(req);
		return QDF_STATUS_E_NOMEM;
	}

	req->cmd = (enum wifi_drv_qmi_ut_cmd_v01)cmd_info->cmd;
	req->duration = cmd_info->duration;
	req->flush_period = cmd_info->flush_period;
	req->num_pkts = cmd_info->num_pkts;
	req->buf_size = cmd_info->buf_size;
	req->ether_type = cmd_info->ether_type;
	for (i = 0; i < QDF_MAC_ADDR_SIZE; i++) {
		req->src_mac[i] = cmd_info->src_mac.bytes[i];
		req->dest_mac[i] = cmd_info->dest_mac.bytes[i];
	}

	if (cmd_info->cmd == WFDS_START_WHC) {
		for (i = 0; i < QDF_IPV4_ADDR_SIZE; i++) {
			req->src_ip_addr[i] = cmd_info->src_ip.bytes[i];
			req->dest_ip_addr[i] = cmd_info->dest_ip.bytes[i];
		}

		req->dest_port = cmd_info->dest_port;
	}

	osif_debug("cmd: %u for duration: %u s, flush period: %u ms",
		  req->cmd, req->duration, req->flush_period);

	status = os_if_qmi_txn_init(&qmi_wfds, &txn, wfds_gen_resp_msg_v01_ei,
				    resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI txn for WFDS unit test cmd init failed %d",
			  status);
		goto out;
	}

	status = os_if_qmi_send_request(&qmi_wfds, NULL, &txn,
					QMI_WFDS_UT_CMD_REQ_V01,
					WFDS_UT_CMD_REQ_MSG_V01_MAX_MSG_LEN,
					wfds_ut_cmd_req_msg_v01_ei, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("QMI WFDS UT command request send failed %d",
			  status);
		os_if_qmi_txn_cancel(&txn);
		goto out;
	}

	status = os_if_qmi_txn_wait(&txn, QMI_WFDS_TIMEOUT_JF);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_info("Wait for unit test cmd response timed out %d",
			  status);
		goto out;
	}

	qdf_assert(resp->resp.result == QMI_RESULT_SUCCESS_V01);

out:
	qdf_mem_free(resp);
	qdf_mem_free(req);

	return status;
}

/**
 * os_if_qmi_wfds_new_server() - New server callback triggered when service is
 *  up.
 * @qmi_hdl: QMI client handle
 * @qmi_svc: QMI service handle
 *
 * Returns: 0 on success else OS failure code
 */
static int
os_if_qmi_wfds_new_server(struct qmi_handle *qmi_hdl,
			  struct qmi_service *qmi_svc)
{
	QDF_STATUS status;

	status = os_if_qmi_connect_to_svc(qmi_hdl, qmi_svc);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to connect to WFDS QMI service port");
		return qdf_status_to_os_return(status);
	}

	status = ucfg_dp_wfds_new_server();

	return qdf_status_to_os_return(status);
}

/**
 * os_if_qmi_wfds_del_server() - Del server callback triggered when service is
 *  down.
 * @qmi_hdl: QMI client handle
 * @qmi_svc: QMI service handle
 *
 * Returns: None
 */
static void
os_if_qmi_wfds_del_server(struct qmi_handle *qmi_hdl,
			  struct qmi_service *qmi_svc)
{
	ucfg_dp_wfds_del_server();
}

static struct qmi_msg_handler qmi_wfds_msg_handler[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WFDS_MEM_IND_V01,
		.ei = wfds_mem_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wfds_mem_ind_msg_v01),
		.fn = os_if_qmi_wfds_request_mem_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WFDS_IPCC_MAP_N_CFG_IND_V01,
		.ei = wfds_ipcc_map_n_cfg_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wfds_ipcc_map_n_cfg_ind_msg_v01),
		.fn = os_if_wfds_ipcc_map_n_cfg_ind_cb
	},
};

static struct qmi_ops qmi_wfds_ops = {
	.new_server = os_if_qmi_wfds_new_server,
	.del_server = os_if_qmi_wfds_del_server,
};

/**
 * os_if_qmi_wfds_init() - Initialize WFDS QMI client
 *
 * Returns: QDF status
 */
static QDF_STATUS os_if_qmi_wfds_init(void)
{
	QDF_STATUS status;

	status = os_if_qmi_handle_init(&qmi_wfds, QMI_WFDS_MAX_RECV_BUF_SIZE,
				       &qmi_wfds_ops, qmi_wfds_msg_handler);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("WFDS QMI handle init failed");
		return status;
	}

	status = os_if_qmi_add_lookup(&qmi_wfds, WFDS_SERVICE_ID_V01,
				      WFDS_SERVICE_VERS_V01,
				      QMI_WFDS_SERVICE_INS_ID_V01);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("WFDS QMI add lookup failed");
		os_if_qmi_handle_release(&qmi_wfds);
		return status;
	}

	return status;
}

/**
 * os_if_qmi_wfds_deinit() - Deinitialize WFDS QMI client
 *
 * Returns: None
 */
static void os_if_qmi_wfds_deinit(void)
{
	os_if_qmi_handle_release(&qmi_wfds);
}

void os_if_qmi_wfds_register_callbacks(struct wlan_qmi_psoc_callbacks *cb_obj)
{
	cb_obj->qmi_wfds_init = os_if_qmi_wfds_init;
	cb_obj->qmi_wfds_deinit = os_if_qmi_wfds_deinit;
	cb_obj->qmi_wfds_send_config_msg = os_if_qmi_wfds_send_config_msg;
	cb_obj->qmi_wfds_send_req_mem_msg = os_if_qmi_wfds_send_req_mem_msg;
	cb_obj->qmi_wfds_send_ipcc_map_n_cfg_msg =
					os_if_qmi_wfds_ipcc_map_n_cfg_msg;
	cb_obj->qmi_wfds_send_misc_req_msg =
					os_if_qmi_wfds_send_misc_req_msg;
}

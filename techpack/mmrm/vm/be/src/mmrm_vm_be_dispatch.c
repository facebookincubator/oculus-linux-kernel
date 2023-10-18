// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "mmrm_vm_debug.h"
#include "mmrm_vm_interface.h"
#include "mmrm_vm_msgq.h"
#include "mmrm_vm_be.h"

/**
 * mmrm_vm_be_send_response - send response to FE
 * mmrm_vm: driver private data
 * msg: response message
 */

int mmrm_vm_be_send_response(struct mmrm_vm_driver_data *mmrm_vm, void *msg)
{
	struct mmrm_vm_response_msg_pkt *ppkt = (struct mmrm_vm_response_msg_pkt *)msg;
	struct mmrm_vm_msg_hdr *hdr = &ppkt->hdr;
	size_t msg_size = sizeof(*hdr) + hdr->size;
	int rc;

	hdr->version = MMRM_VM_VER_1;
	hdr->type = MMRM_VM_TYPE_DATA;
	hdr->flags = 0;

	rc = mmrm_vm_msgq_send(mmrm_vm, msg, msg_size);
	d_mpr_l("%s: size:%d rc=%d\n", __func__, msg_size, rc);
	return rc;
}

/**
 * mmrm_vm_be_client_register - call mmrm API to register client
 * mmrm_vm: driver private data
 * req: request parameters
 */
static int mmrm_vm_be_client_register(struct mmrm_vm_driver_data *mmrm_vm,
	struct mmrm_vm_api_request_msg *req)
{
	struct mmrm_client *pClient;
	int rc;
	struct mmrm_vm_response_msg_pkt pkt;
	struct mmrm_client_desc client_desc;

	// unpacketizing the call from fe on SVM
	client_desc.client_type = req->data.reg.client_type;
	memcpy(&(client_desc.client_info.desc), &(req->data.reg.desc),
				sizeof(client_desc.client_info.desc));
	client_desc.priority = req->data.reg.priority;

	d_mpr_l("%s: register type:%d priority:%d\n", __func__,
			client_desc.client_type, client_desc.priority);
	d_mpr_l("%s: domain:%d client ID:%d\n", __func__,
			client_desc.client_info.desc.client_domain,
			client_desc.client_info.desc.client_id);
	d_mpr_l("%s: clk name:%s\n", __func__, client_desc.client_info.desc.name);

	// call mmrm register function
	pClient = mmrm_client_register(&client_desc);
	if (pClient != NULL) {
		mmrm_vm->clk_client_tbl[pClient->client_uid] = pClient;
		pkt.msg.data.reg.client_id = pClient->client_uid;
	} else {
		pkt.msg.data.reg.client_id = U32_MAX;
		d_mpr_e("%s: client:%p client id:%d\n", __func__, pClient, pkt.msg.data.reg.client_id);
	}

	// prepare response packet & send to fe on SVM
	pkt.msg.hd.cmd_id = MMRM_VM_RESPONSE_REGISTER;
	pkt.msg.hd.seq_no = req->hd.seq_no;
	pkt.hdr.size = sizeof(pkt.msg.hd) + sizeof(pkt.msg.data.reg);

	d_mpr_l("%s: cmd_id:%d data size:%d\n", __func__, pkt.msg.hd.cmd_id, pkt.hdr.size);

	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt);
	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

/**
 * mmrm_vm_be_client_setvalue - call mmrm API to set client values
 * mmrm_vm: driver private data
 * req: set client value request parameters
 */
static int mmrm_vm_be_client_setvalue(struct mmrm_vm_driver_data *mmrm_vm,
	struct mmrm_vm_api_request_msg *req)
{
	struct mmrm_vm_response_msg_pkt pkt_resp;
	int rc;
	struct mmrm_vm_setvalue_request *req_param = &req->data.setval;

	// call mmrm client set value function, and fill response packet

	rc = mmrm_client_set_value(mmrm_vm->clk_client_tbl[req_param->client_id],
			&req_param->data, req_param->val);

	if (rc != 0) {
		d_mpr_e("%s: set value rc:%d client id:%d\n", __func__, rc, req_param->client_id);
	}
	// prepare response packet & send to fe on SVM
	pkt_resp.msg.hd.cmd_id = MMRM_VM_RESPONSE_SETVALUE;
	pkt_resp.msg.hd.seq_no = req->hd.seq_no;
	pkt_resp.hdr.size = sizeof(pkt_resp.msg.hd) + sizeof(pkt_resp.msg.data.setval);

	pkt_resp.msg.data.setval.val = rc;

	d_mpr_l("%s: cmd_id:%d data size:%d\n", __func__,
		pkt_resp.msg.hd.cmd_id, pkt_resp.hdr.size);

	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt_resp);

	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

/**
 * mmrm_vm_be_client_setvalue_inrange - call mmrm API to set client range values
 * mmrm_vm: driver private data
 * req: set client value request parameters
 */
static int mmrm_vm_be_client_setvalue_inrange(struct mmrm_vm_driver_data *mmrm_vm,
	struct mmrm_vm_api_request_msg *req)
{
	struct mmrm_vm_response_msg_pkt pkt;
	int rc;
	struct mmrm_vm_setvalue_inrange_request *req_param = &req->data.setval_range;

	rc = mmrm_client_set_value_in_range(mmrm_vm->clk_client_tbl[req_param->client_id],
		&req_param->data, &req_param->val);

	pkt.msg.hd.cmd_id = MMRM_VM_RESPONSE_SETVALUE_INRANGE;
	pkt.msg.hd.seq_no = req->hd.seq_no;
	pkt.msg.data.setval_range.ret_code = rc;
	pkt.hdr.size = sizeof(pkt.msg.hd) + sizeof(pkt.msg.data.setval_range);

	d_mpr_l("%s: cmd_id:%d data size:%d\n", __func__, pkt.msg.hd.cmd_id, pkt.hdr.size);

	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt);
	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

/**
 * mmrm_vm_be_client_getvalue - call mmrm API to get client values
 * mmrm_vm: driver private data
 * req: set client value request parameters
 */
static int mmrm_vm_be_client_getvalue(struct mmrm_vm_driver_data *mmrm_vm,
	struct mmrm_vm_api_request_msg *req)
{
	struct mmrm_vm_response_msg_pkt pkt;
	int rc;
	struct mmrm_vm_getvalue_request *req_param = &req->data.getval;
	struct mmrm_client_res_value val;
	struct mmrm_client_res_value *p_val = &pkt.msg.data.getval.val;

	rc = mmrm_client_get_value(mmrm_vm->clk_client_tbl[req_param->client_id], &val);

	pkt.msg.hd.cmd_id = MMRM_VM_RESPONSE_GETVALUE;
	pkt.msg.hd.seq_no = req->hd.seq_no;
	pkt.hdr.size = sizeof(pkt.msg.hd) + sizeof(pkt.msg.data.getval);

	p_val->cur = val.cur;
	p_val->max = val.max;
	p_val->min = val.min;

//	pr_err("%s: cmd_id:%d data size:%d\n", __func__, pkt.msg.hd.cmd_id, pkt.hdr.size);

	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt);
	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

/**
 * mmrm_vm_be_client_deregister - call mmrm API to deregister client
 * mmrm_vm: driver private data
 * req: set client value request parameters
 */
static int mmrm_vm_be_client_deregister(struct mmrm_vm_driver_data *mmrm_vm,
		struct mmrm_vm_api_request_msg *req)
{
	int rc;
	struct mmrm_vm_response_msg_pkt pkt;
	struct mmrm_vm_deregister_request *req_param = &req->data.dereg;

	rc = mmrm_client_deregister(mmrm_vm->clk_client_tbl[req_param->client_id]);
//	pr_err("%s: client:%d\n", __func__, req_param->client_id);

	pkt.msg.hd.cmd_id = MMRM_VM_RESPONSE_DEREGISTER;
	pkt.msg.hd.seq_no = req->hd.seq_no;

	pkt.hdr.size = sizeof(pkt.msg.hd) + sizeof(pkt.msg.data.dereg);
	pkt.msg.data.dereg.ret_code = rc;

//	pr_err("%s: cmd_id:%d data size:%d ret:%d\n", __func__,
//		pkt.msg.hd.cmd_id, pkt.hdr.size, pkt.msg.data.dereg.ret_code);

	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt);
	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

/**
 * mmrm_vm_be_client_noop - call none mmrm API to calculate msgq roundtrip time
 * mmrm_vm: driver private data
 * req: request parameters
 */
static int mmrm_vm_be_client_noop(struct mmrm_vm_driver_data *mmrm_vm,
		struct mmrm_vm_api_request_msg *req)
{
	int rc = 0;
	struct mmrm_vm_response_msg_pkt pkt;

	pkt.msg.hd.cmd_id = MMRM_VM_RESPONSE_NOOP;
	pkt.msg.hd.seq_no = req->hd.seq_no;

	pkt.hdr.size = sizeof(pkt.msg.hd) + sizeof(pkt.msg.data.lptest);
	pkt.msg.data.dereg.ret_code = rc;

	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt);
	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

/**
 * mmrm_vm_be_recv - be dispatch mmrm request to mmrm API call
 * mmrm_vm: driver private data
 * data: request message buffer pointer
 * size: request message size
 */
int mmrm_vm_be_recv(struct mmrm_vm_driver_data *mmrm_vm, void *data, size_t size)
{
	struct mmrm_vm_api_request_msg *cmd = data;
	int rc = -1;

	switch (cmd->hd.cmd_id) {
	case MMRM_VM_REQUEST_REGISTER:
		rc = mmrm_vm_be_client_register(mmrm_vm, cmd);
		break;

	case MMRM_VM_REQUEST_SETVALUE:
		rc = mmrm_vm_be_client_setvalue(mmrm_vm, cmd);
		break;

	case MMRM_VM_REQUEST_SETVALUE_INRANGE:
		rc = mmrm_vm_be_client_setvalue_inrange(mmrm_vm, cmd);
		break;

	case MMRM_VM_REQUEST_GETVALUE:
		rc = mmrm_vm_be_client_getvalue(mmrm_vm, cmd);
		break;

	case MMRM_VM_REQUEST_DEREGISTER:
		rc = mmrm_vm_be_client_deregister(mmrm_vm, cmd);
		break;
	case MMRM_VM_REQUEST_NOOP:
		rc = mmrm_vm_be_client_noop(mmrm_vm, cmd);
		break;
	default:
		pr_err("%s: cmd_id:%d unknown!!!\n", __func__, cmd->hd.cmd_id);
		break;
	}
	return rc;
}

/**
 * mmrm_vm_be_wrong_request - deal with SVM wrong request
 * mmrm_vm: driver private data
 */
int mmrm_vm_be_wrong_request(struct mmrm_vm_driver_data *mmrm_vm)
{
	int rc = 0;
	struct mmrm_vm_response_msg_pkt pkt;

	pkt.msg.hd.cmd_id = MMRM_VM_RESPONSE_INVALID_PKT;
	pkt.msg.hd.seq_no = 0;

	pkt.hdr.size = sizeof(pkt.msg.hd) + sizeof(pkt.msg.data.dereg);
	pkt.msg.data.dereg.ret_code = rc;

	d_mpr_e("%s: wrong request\n", __func__);
	rc = mmrm_vm_be_send_response(mmrm_vm, &pkt);
	if (rc != 0)
		d_mpr_e("%s: rc:%d\n", __func__, rc);
	return rc;
}

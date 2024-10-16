// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/ktime.h>


#include "mmrm_vm_fe.h"
#include "mmrm_vm_interface.h"
#include "mmrm_vm_msgq.h"
#include "mmrm_vm_debug.h"

#define get_client_handle_2_id(client) (client->client_uid)

extern struct mmrm_vm_driver_data *drv_vm_fe;

#define MAX_TIMEOUT_MS 300

#define CHECK_SKIP_MMRM_CLK_RSRC(drv_data)	\
{									\
	if (!drv_data->is_clk_scaling_supported) {	\
		d_mpr_h("%s: mmrm clk rsrc not supported\n", __func__);\
		goto skip_mmrm;				\
	}								\
}

int mmrm_fe_append_work_list(struct mmrm_vm_msg_q *msg_q, int msg_sz)
{
	struct mmrm_vm_request_msg_pkt *msg_pkt = msg_q->m_req;
	struct mmrm_vm_fe_priv *fe_data = drv_vm_fe->vm_pvt_data;
	unsigned long waited_time_ms;

	init_completion(&msg_q->complete);
	mutex_lock(&fe_data->resp_works_lock);
	list_add_tail(&msg_q->link, &fe_data->resp_works);
	mutex_unlock(&fe_data->resp_works_lock);

	mutex_lock(&fe_data->msg_send_lock);
	msg_pkt->msg.hd.seq_no = fe_data->seq_no++;
	mutex_unlock(&fe_data->msg_send_lock);

	d_mpr_w("%s: seq no:%d\n", __func__, msg_pkt->msg.hd.seq_no);

	msg_pkt->start_time_ns = ktime_get_ns();

	mmrm_vm_fe_request_send(drv_vm_fe, msg_pkt, msg_sz);

	waited_time_ms = wait_for_completion_timeout(&msg_q->complete,
		msecs_to_jiffies(MAX_TIMEOUT_MS));
	if (waited_time_ms >= MAX_TIMEOUT_MS) {
		d_mpr_e("%s: request send timeout\n", __func__);
		return -1;
	}
	return 0;
}

struct mmrm_vm_msg_q *get_msg_work(void)
{
	struct mmrm_vm_msg_q *msg_q;
	struct mmrm_vm_fe_pkt *data;

	data = kzalloc(sizeof(struct mmrm_vm_fe_pkt), GFP_KERNEL);
	if (data == NULL)
		goto err_mem_fail;

	msg_q = &data->msgq;
	msg_q->m_req = &data->req_pkt;
	msg_q->m_resp = &data->resp_pkt;

	return msg_q;

err_mem_fail:
	d_mpr_e("%s: failed to alloc msg buffer\n", __func__);
	return NULL;
}

void release_msg_work(struct mmrm_vm_msg_q *msg_q)
{
	struct mmrm_vm_fe_pkt *data;

	if (msg_q == NULL) {
		d_mpr_e("%s: release null msg ptr\n", __func__);
		return;
	}
	data = container_of(msg_q, struct mmrm_vm_fe_pkt, msgq);
	kfree(data);
}

struct mmrm_client *mmrm_client_register(struct mmrm_client_desc *desc)
{
	struct mmrm_vm_msg_q *msg_q;
	struct mmrm_vm_api_request_msg *api_msg;
	struct mmrm_vm_register_request *reg_data;
	size_t msg_size = sizeof(api_msg->hd) + sizeof(*reg_data);
	int rc = 0;
	struct mmrm_client *client = NULL;

	if (mmrm_vm_fe_clk_src_get(desc) == NULL) {
		d_mpr_e("%s: FE doesn't support clk domain=%d client id=%d\n", __func__,
			desc->client_info.desc.client_domain, desc->client_info.desc.client_id);
		goto err_clk_src;
	}

	msg_q = get_msg_work();
	if (msg_q == NULL) {
		d_mpr_e("%s: failed to alloc msg buf\n", __func__);
		goto err_no_mem;
	}
	api_msg = &msg_q->m_req->msg;
	reg_data = &api_msg->data.reg;

	api_msg->hd.cmd_id = MMRM_VM_REQUEST_REGISTER;
	reg_data->client_type = desc->client_type;
	reg_data->priority = desc->priority;
	memcpy(&reg_data->desc, &desc->client_info.desc, sizeof(reg_data->desc));

	rc = mmrm_fe_append_work_list(msg_q, msg_size);
	if (rc == 0) {
		client = mmrm_vm_fe_get_client(msg_q->m_resp->msg.data.reg.client_id);
	};

	release_msg_work(msg_q);

err_no_mem:
err_clk_src:
	return client;
}
EXPORT_SYMBOL(mmrm_client_register);

int mmrm_client_deregister(struct mmrm_client *client)
{
	int rc = -1;
	struct mmrm_vm_api_request_msg *api_msg;
	struct mmrm_vm_deregister_request *reg_data;
	struct mmrm_vm_msg_q *msg_q;

	size_t msg_size = sizeof(api_msg->hd) + sizeof(*reg_data);

	msg_q = get_msg_work();
	if (msg_q == NULL) {
		d_mpr_e("%s: failed to alloc msg buf\n", __func__);
		goto err_no_mem;
	}
	api_msg = &msg_q->m_req->msg;
	reg_data = &api_msg->data.dereg;

	api_msg->hd.cmd_id = MMRM_VM_REQUEST_DEREGISTER;
	reg_data->client_id = get_client_handle_2_id(client);


	rc = mmrm_fe_append_work_list(msg_q, msg_size);
	if (rc == 0)
		rc = msg_q->m_resp->msg.data.dereg.ret_code;

	release_msg_work(msg_q);

err_no_mem:
	return rc;
}
EXPORT_SYMBOL(mmrm_client_deregister);

int mmrm_client_set_value(struct mmrm_client *client,
	struct mmrm_client_data *client_data, unsigned long val)
{
	int rc = -1;
	struct mmrm_vm_api_request_msg *api_msg;
	struct mmrm_vm_setvalue_request *reg_data;
	struct mmrm_vm_msg_q *msg_q;

	size_t msg_size = sizeof(api_msg->hd) + sizeof(*reg_data);

	msg_q = get_msg_work();
	if (msg_q == NULL) {
		d_mpr_e("%s: failed to alloc msg buf\n", __func__);
		goto err_no_mem;
	}
	api_msg = &msg_q->m_req->msg;
	reg_data = &api_msg->data.setval;

	api_msg->hd.cmd_id = MMRM_VM_REQUEST_SETVALUE;
	reg_data->client_id = get_client_handle_2_id(client);
	reg_data->data.flags = client_data->flags;
	reg_data->data.num_hw_blocks = client_data->num_hw_blocks;
	reg_data->val = val;

	rc = mmrm_fe_append_work_list(msg_q, msg_size);
	if (rc != 0)
		return rc;

	rc = msg_q->m_resp->msg.data.setval.val;
	d_mpr_h("%s: done rc=%d\n", __func__, rc);

err_no_mem:
	return rc;
}
EXPORT_SYMBOL(mmrm_client_set_value);

int mmrm_client_set_value_in_range(struct mmrm_client *client,
	struct mmrm_client_data *client_data,
	struct mmrm_client_res_value *val)
{
	int rc = -1;
	struct mmrm_vm_api_request_msg *api_msg ;
	struct mmrm_vm_setvalue_inrange_request *reg_data;
	size_t msg_size = sizeof(api_msg->hd) + sizeof(*reg_data);
	struct mmrm_vm_msg_q *msg_q;

	msg_q = get_msg_work();
	if (msg_q == NULL) {
		d_mpr_e("%s: failed to alloc msg buf\n", __func__);
		goto err_no_mem;
	}
	api_msg = &msg_q->m_req->msg;
	reg_data = &api_msg->data.setval_range;

	api_msg->hd.cmd_id = MMRM_VM_REQUEST_SETVALUE_INRANGE;
	reg_data->client_id = get_client_handle_2_id(client);
	reg_data->data.flags = client_data->flags;
	reg_data->data.num_hw_blocks = client_data->num_hw_blocks;
	reg_data->val.cur = val->cur;
	reg_data->val.max = val->max;
	reg_data->val.min = val->min;

	rc = mmrm_fe_append_work_list(msg_q, msg_size);

err_no_mem:
	return rc;
}
EXPORT_SYMBOL(mmrm_client_set_value_in_range);


int mmrm_client_get_value(struct mmrm_client *client,
	struct mmrm_client_res_value *val)
{
	int rc = -1;
	struct mmrm_vm_api_request_msg *api_msg;
	struct mmrm_vm_getvalue_request *reg_data;
	size_t msg_size = sizeof(api_msg->hd) + sizeof(*reg_data);
	struct mmrm_vm_msg_q *msg_q;

	msg_q = get_msg_work();
	if (msg_q == NULL) {
		d_mpr_e("%s: failed to alloc msg buf\n", __func__);
		goto err_no_mem;
	}
	api_msg = &msg_q->m_req->msg;
	reg_data = &api_msg->data.getval;

	api_msg->hd.cmd_id = MMRM_VM_REQUEST_GETVALUE;
	reg_data->client_id = get_client_handle_2_id(client);


	rc = mmrm_fe_append_work_list(msg_q, msg_size);

	if (rc == 0) {
		val->cur = msg_q->m_resp->msg.data.getval.val.cur;
		val->max = msg_q->m_resp->msg.data.getval.val.max;
		val->min = msg_q->m_resp->msg.data.getval.val.min;
	}

err_no_mem:
	return rc;
}
EXPORT_SYMBOL(mmrm_client_get_value);

bool mmrm_client_check_scaling_supported(enum mmrm_client_type client_type, u32 client_domain)
{
	struct mmrm_vm_fe_priv *fe_data;

	if (drv_vm_fe == (void *)-EPROBE_DEFER) {
		d_mpr_e("%s: mmrm probe_init not done\n", __func__);
		goto err_exit;
	}

	fe_data = drv_vm_fe->vm_pvt_data;
	if (client_type == MMRM_CLIENT_CLOCK) {
		CHECK_SKIP_MMRM_CLK_RSRC(fe_data);
	}

	return true;
err_exit:
	d_mpr_e("%s: error exit\n", __func__);
skip_mmrm:
	return false;
}
EXPORT_SYMBOL(mmrm_client_check_scaling_supported);

int mmrm_client_msgq_roundtrip_measure(u32 val)
{
	int rc = 0;
	struct mmrm_vm_api_request_msg *api_msg;
	struct mmrm_vm_noop_request *reg_data;
	struct mmrm_vm_msg_q *msg_q;

	size_t msg_size = sizeof(api_msg->hd) + sizeof(*reg_data);

	msg_q = get_msg_work();
	api_msg = &msg_q->m_req->msg;
	reg_data = &api_msg->data.lptest;

	api_msg->hd.cmd_id = MMRM_VM_REQUEST_NOOP;
	reg_data->client_id = val;

	rc = mmrm_fe_append_work_list(msg_q, msg_size);

	release_msg_work(msg_q);

	return rc;
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of.h>
#include <linux/limits.h>

#include <linux/timekeeping.h>

#include "mmrm_vm_fe.h"
#include "mmrm_vm_interface.h"
#include "mmrm_vm_msgq.h"
#include "mmrm_vm_debug.h"

extern struct mmrm_vm_driver_data *drv_vm_fe;

void mmrm_vm_fe_recv(struct mmrm_vm_driver_data *mmrm_vm, void *data, size_t size)
{
	struct mmrm_vm_api_response_msg *msg = data;
	struct mmrm_vm_msg_q *node, *temp;
	int rc = -1;
	u64 kt2, interval;
	struct mmrm_vm_fe_priv *fe_data = mmrm_vm->vm_pvt_data;

	mutex_lock(&fe_data->resp_works_lock);
	list_for_each_entry_safe(node, temp, &fe_data->resp_works, link) {
		if (msg->hd.seq_no == node->m_req->msg.hd.seq_no) {
			d_mpr_w("%s: seq no:%d\n", __func__, msg->hd.seq_no);
			list_del(&node->link);
			rc = 0;
			break;
		}
	}
	mutex_unlock(&fe_data->resp_works_lock);
	if (rc != 0) {
		d_mpr_e("%s: seq no:%d wrong\n", __func__, msg->hd.seq_no);
		return;
	}

	d_mpr_w("%s: cmd:%d\n", __func__, msg->hd.cmd_id);
	switch (msg->hd.cmd_id) {
	case	MMRM_VM_RESPONSE_REGISTER:
		node->m_resp->msg.data.reg.client_id = msg->data.reg.client_id;
		d_mpr_w("%s: client_id:%u\n", __func__, msg->data.reg.client_id);
		break;
	case	MMRM_VM_RESPONSE_SETVALUE:
		node->m_resp->msg.data.setval.val = msg->data.setval.val;
		break;
	case	MMRM_VM_RESPONSE_SETVALUE_INRANGE:
		node->m_resp->msg.data.setval_range.ret_code = msg->data.setval_range.ret_code;
		break;
	case	MMRM_VM_RESPONSE_GETVALUE:
		node->m_resp->msg.data.getval.val = msg->data.getval.val;
		break;
	case	MMRM_VM_RESPONSE_DEREGISTER:
		node->m_resp->msg.data.dereg.ret_code = msg->data.dereg.ret_code;
		break;
	case	MMRM_VM_RESPONSE_NOOP:
		kt2 = ktime_get_ns();
		interval = kt2 - node->m_req->start_time_ns;
		d_mpr_w("%s: looptest start:%lu end:%lu interval:%luns\n",
			__func__,
			node->m_req->start_time_ns, kt2,
			interval);
		fe_data->msgq_rt_stats.looptest_total_us += interval;
		break;
	case	MMRM_VM_RESPONSE_INVALID_PKT:
		d_mpr_e("%s: invalid request code\n");
		break;
	default:
		d_mpr_e("wrong response\n");
		break;
	};

	complete(&node->complete);
}

int mmrm_vm_fe_request_send(struct mmrm_vm_driver_data *mmrm_vm,
		struct mmrm_vm_request_msg_pkt *msg_pkt, size_t msg_size)
{
	int  rc;

	struct mmrm_vm_msg_hdr *hdr;
	struct mmrm_vm_gh_msgq_info *pmsg_info = &mmrm_vm->msg_info;

	hdr = (struct mmrm_vm_msg_hdr *)&msg_pkt->hdr;
	hdr->version = MMRM_VM_VER_1;
	hdr->type = MMRM_VM_TYPE_DATA;
	hdr->flags = 0;
	hdr->size = msg_size;

	if (!pmsg_info->msgq_handle) {
		d_mpr_e("Failed to send msg, invalid msgq handle\n");
		return -EINVAL;
	}

	if (msg_size > GH_MSGQ_MAX_MSG_SIZE_BYTES) {
		d_mpr_e("msg size unsupported for msgq: %ld > %d\n", msg_size,
			GH_MSGQ_MAX_MSG_SIZE_BYTES);
		return -E2BIG;
	}

	rc = gh_msgq_send(pmsg_info->msgq_handle, msg_pkt,
		msg_size + sizeof(msg_pkt->hdr), GH_MSGQ_TX_PUSH);

	return rc;
}

int mmrm_vm_fe_count_clk_clients_frm_dt(struct platform_device *pdev)
{
	u32 size_clk_src = 0, num_clk_src = 0;

	of_find_property(pdev->dev.of_node, "mmrm-client-info", &size_clk_src);
	num_clk_src = size_clk_src / sizeof(struct mmrm_vm_fe_clk_client_desc);
	d_mpr_h("%s: found %d clk_srcs size %d\n",
		__func__, num_clk_src, size_clk_src);

	return num_clk_src;
}

int mmrm_vm_fe_load_clk_rsrc(struct mmrm_vm_driver_data *mmrm_vm)
{
	int rc = 0, num_clk_src = 0;
	int c = 0, size_clk_src = 0, entry_offset = 3;

	struct platform_device *pdev;
	struct mmrm_vm_fe_clk_client_desc *pclk_src;
	struct mmrm_vm_fe_priv *fe_data = mmrm_vm->vm_pvt_data;

	pdev = container_of(fe_data->dev, struct platform_device, dev);

	of_find_property(pdev->dev.of_node, "mmrm-client-info", &size_clk_src);
	if ((size_clk_src < sizeof(*fe_data->clk_src_set.clk_src_tbl)) ||
		(size_clk_src % sizeof(*fe_data->clk_src_set.clk_src_tbl))) {
		d_mpr_e("%s: invalid size(%d) of clk src table\n",
			__func__, size_clk_src);
		fe_data->clk_src_set.count = 0;
		goto err_load_clk_src_tbl;
	}

	fe_data->clk_src_set.clk_src_tbl = devm_kzalloc(&pdev->dev,
			size_clk_src, GFP_KERNEL);

	if (!fe_data->clk_src_set.clk_src_tbl) {
		d_mpr_e("%s: failed to allocate memory for clk_src_tbl\n",
				__func__);
		rc = -ENOMEM;
		goto err_load_clk_src_tbl;
	}
	num_clk_src = size_clk_src / sizeof(struct mmrm_vm_fe_clk_client_desc);
	fe_data->clk_src_set.count = num_clk_src;

	d_mpr_w("%s: found %d clk_srcs size %d\n",
			__func__, num_clk_src, size_clk_src);

	for (c = 0; c < num_clk_src; c++) {
		pclk_src = &fe_data->clk_src_set.clk_src_tbl[c];

		of_property_read_u32_index(pdev->dev.of_node,
			"mmrm-client-info", (c*entry_offset),
			&pclk_src->client_domain);
		of_property_read_u32_index(pdev->dev.of_node,
			"mmrm-client-info", (c*entry_offset+1),
			&pclk_src->client_id);
		of_property_read_u32_index(pdev->dev.of_node,
			"mmrm-client-info", (c*entry_offset+2),
			&pclk_src->num_hw_blocks);
	}

	return 0;

err_load_clk_src_tbl:
	return rc;
}

struct mmrm_vm_fe_clk_client_desc *mmrm_vm_fe_clk_src_get(struct mmrm_client_desc *desc)
{
	struct mmrm_vm_fe_priv *fe_data = drv_vm_fe->vm_pvt_data;
	int num_clk_src = fe_data->clk_src_set.count;
	struct mmrm_vm_fe_clk_client_desc *pclk_src;
	int i;

	d_mpr_l("%s: num clk src=%d domain:%d id:%d\n", __func__, num_clk_src,
		desc->client_info.desc.client_domain, desc->client_info.desc.client_id);

	pclk_src = fe_data->clk_src_set.clk_src_tbl;
	for (i = 0; i < num_clk_src; i++, pclk_src++) {
		if (pclk_src->client_domain == desc->client_info.desc.client_domain ||
			pclk_src->client_id == desc->client_info.desc.client_id) {
			return pclk_src;
		}
	}
	return NULL;
}

int mmrm_vm_fe_init_lookup_table(struct mmrm_vm_driver_data *mmrm_vm)
{
	int  i, rc = -1;
	struct platform_device *pdev;
	struct mmrm_vm_fe_priv *fe_data = mmrm_vm->vm_pvt_data;

	pdev = container_of(fe_data->dev, struct platform_device, dev);

	fe_data->client_tbl = devm_kzalloc(&pdev->dev,
		fe_data->clk_src_set.count * sizeof(struct mmrm_client), GFP_KERNEL);
	if (!fe_data->client_tbl)
		return rc;

	for (i = 0; i < fe_data->clk_src_set.count; i++) {
		fe_data->client_tbl[i].client_type = 0;
		fe_data->client_tbl[i].client_uid = U32_MAX;
	}
	return 0;
}

int mmrm_vm_fe_clk_print_info(
	struct mmrm_vm_fe_clk_src_set *clk_src_set,
	char *buf, int max_len)
{
	int left_spaces = max_len;
	int len, c;
	struct mmrm_vm_fe_clk_client_desc *pclk_src;
	int num_clk_src = clk_src_set->count;

	len = scnprintf(buf, left_spaces, "Domain  ID  Num\n");
	left_spaces -= len;
	buf += len;

	pclk_src = clk_src_set->clk_src_tbl;
	for (c = 0; c < num_clk_src; c++, pclk_src++) {
		len = scnprintf(buf, left_spaces, "%d\t%d\t%d\n",
			pclk_src->client_domain, pclk_src->client_id, pclk_src->num_hw_blocks);
		left_spaces -= len;
		buf += len;
	}

	return max_len - left_spaces;
}

struct mmrm_client *mmrm_vm_fe_get_client(u32 client_id)
{
	int i;
	struct mmrm_client *ptr;
	struct mmrm_vm_fe_priv *fe_data = drv_vm_fe->vm_pvt_data;

	if (client_id == U32_MAX)
		return NULL;

	for (i = 0, ptr = fe_data->client_tbl; i < fe_data->clk_src_set.count; i++, ptr++) {
		if (ptr->client_uid == client_id)
			return ptr;
	}

	for (i = 0, ptr = fe_data->client_tbl; i < fe_data->clk_src_set.count; i++, ptr++) {
		if (ptr->client_uid == U32_MAX) {
			ptr->client_uid = client_id;
			return ptr;
		}
	}
	return NULL;
}


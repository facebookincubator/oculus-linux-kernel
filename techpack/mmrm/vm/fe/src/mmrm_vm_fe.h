/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __MMRM_VM_FE_H__
#define __MMRM_VM_FE_H__

#include <mmrm_vm_msgq.h>
#include <mmrm_vm_interface.h>

#define MMRM_SYSFS_ENTRY_MAX_LEN     PAGE_SIZE

struct mmrm_vm_fe_clk_client_desc {
	u32 client_domain;
	u32 client_id;
	u32 num_hw_blocks;
};

struct mmrm_vm_fe_clk_src_set {
	struct mmrm_vm_fe_clk_client_desc *clk_src_tbl;
	u32 count;
};

struct mmrm_vm_fe_msgq_rt_stats {
	u64 register_total_us;
	u64 looptest_total_us;
	u32 count;
};

struct mmrm_vm_fe_priv {
	struct device *dev;

	struct mmrm_client *client_tbl;

	struct list_head resp_works;
	struct mutex resp_works_lock;

	struct mmrm_vm_fe_clk_src_set clk_src_set;
	struct mutex msg_send_lock;
	int  seq_no;
	bool is_clk_scaling_supported;

	struct mmrm_vm_fe_msgq_rt_stats msgq_rt_stats;
};

struct mmrm_vm_fe_pkt {
	struct mmrm_vm_msg_q msgq;
	struct mmrm_vm_request_msg_pkt req_pkt;
	struct mmrm_vm_response_msg_pkt resp_pkt;
};

/*
 * mmrm_vm_fe_recv_cb -- FE message receiving thread call this function
 *                       for transfer receiving packet to FE
 * @mmrm_vm: specific device driver info
 * @data: message pointer
 * @size: message size
 */
void mmrm_vm_fe_recv_cb(struct mmrm_vm_driver_data *mmrm_vm, void *data, size_t size);

/*
 * mmrm_vm_fe_request_send -- FE send mmrm request message
 * @mmrm_vm: device data, includes message handle
 * @msg_pkt: request message pointer
 * @msg_size: message size
 */
int mmrm_vm_fe_request_send(struct mmrm_vm_driver_data *mmrm_vm,
	struct mmrm_vm_request_msg_pkt *msg_pkt, size_t msg_size);

/*
 * get_client_id_2_handle -- get handle from client ID
 * @client_id: client ID
 */
struct mmrm_client *mmrm_vm_fe_get_client(u32 client_id);

/*
 * load_clk_resource_info -- get clk resource info from DT
 * @drv_priv: device data
 */
int mmrm_vm_fe_load_clk_rsrc(struct mmrm_vm_driver_data *drv_priv);

/*
 * mmrm_vm_fe_clk_src_check -- check if fe support the clk src
 * @desc: clk src description
 */
struct mmrm_vm_fe_clk_client_desc *mmrm_vm_fe_clk_src_get(struct mmrm_client_desc *desc);

/*
 * init_lookup_table -- init et clk lookup table
 * @mmrm_vm: device data
 */
int mmrm_vm_fe_init_lookup_table(struct mmrm_vm_driver_data *mmrm_vm);

/*
 * mmrm_vm_fe_clk_print_info -- output clk info through sys
 * @clk_src_set: clk info
 * @buf: received output buffer
 * @max_len: buffer length
 */
int mmrm_vm_fe_clk_print_info(
	struct mmrm_vm_fe_clk_src_set *clk_src_set,
	char *buf, int max_len);

/*
 * mmrm_vm_fe_recv -- process received response info
 * @mmrm_vm: device data
 * @data: received response info buffer
 * @size: message size
 */
void mmrm_vm_fe_recv(struct mmrm_vm_driver_data *mmrm_vm, void *data, size_t size);

/*
 * mmrm_vm_fe_count_clk_clients_frm_dt -- process received response info
 * @pdev: platform device
 */
int mmrm_vm_fe_count_clk_clients_frm_dt(struct platform_device *pdev);

#endif /* __MMRM_VM_FE_H__ */



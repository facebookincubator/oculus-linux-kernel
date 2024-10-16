/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __MMRM_VM_INTERNAL_H__
#define __MMRM_VM_INTERNAL_H__

#include <linux/mutex.h>
#include <linux/soc/qcom/msm_mmrm.h>

#include <mmrm_vm_msgq.h>

/**
 * mmrm_vm_thread_info - message listener & workqueue info
 * @msgq_listener_thread: handle to msgq listener thread that is used
 *                        to receive/send messages through gunyah interface
 * @msg_workq: message workqueue pointer
 * @msgq_work: message work, worker thread to process the messages
 * @queued_msg: message queue head
 */
struct mmrm_vm_thread_info {
	struct task_struct *msgq_listener_thread;
	struct workqueue_struct   *msg_workq;
	struct delayed_work msgq_work;
	struct mutex list_lock;
	struct list_head   queued_msg;
};

/**
 * struct mmrm_vm_data_priv -- device driver private part
 * @dev: device pointer
 * @msg_info: gunyah message info
 * @thread_info: message lister & workqueue info
 * @clk_client_tbl: index and client handler LUT
 * @debugfs_root: debug fs, /sys/kernel/debug
 * @vm_pvt_data: pointer to fe/be specific data
 */
struct mmrm_vm_driver_data {
	struct device *dev;
	struct mmrm_vm_gh_msgq_info msg_info;
	struct mmrm_vm_thread_info thread_info;
	struct mmrm_client **clk_client_tbl;

	/* debugfs */
	struct dentry *debugfs_root;
	void *vm_pvt_data;
};

/**
 * enum mmrm_vm_api_msg_id -- request/response cmd ID
 */
enum mmrm_vm_api_msg_id {
	MMRM_VM_REQUEST_REGISTER = 1,
	MMRM_VM_REQUEST_SETVALUE,
	MMRM_VM_REQUEST_SETVALUE_INRANGE,
	MMRM_VM_REQUEST_GETVALUE,
	MMRM_VM_REQUEST_DEREGISTER,
	MMRM_VM_REQUEST_NOOP, // this is for debug purpose,calculating msgq roundtrip time

	MMRM_VM_RESPONSE_REGISTER = MMRM_VM_REQUEST_REGISTER | 0x800,
	MMRM_VM_RESPONSE_SETVALUE,
	MMRM_VM_RESPONSE_SETVALUE_INRANGE,
	MMRM_VM_RESPONSE_GETVALUE,
	MMRM_VM_RESPONSE_DEREGISTER,
	MMRM_VM_RESPONSE_NOOP, // this is for debug purpose,calculating msgq roundtrip time
	MMRM_VM_RESPONSE_INVALID_PKT,
};

/**
 * struct msg_head -- message head
 * @cmd_id: mmrm API message cmd id
 * @seq_no: message sequence id
 */
struct mmrm_vm_api_msg_head {
	enum mmrm_vm_api_msg_id cmd_id;
	int  seq_no;
};

/**
 * struct register_request -- mmrm register parameters
 * @client_type: client type, definition see msm_mmrm.h
 * @priority: client priority, definition see msm_mmrm.h
 * @desc: client description, definition see msm_mmrm.h
 */
struct mmrm_vm_register_request {
	enum mmrm_client_type client_type;
	enum mmrm_client_priority priority;
	struct mmrm_clk_client_desc desc;
};

/**
 * struct deregister_request -- mmrm deregister parameters
 * @client: client registered handle
 */
struct mmrm_vm_deregister_request {
	u32 client_id;
};

/**
 * struct mmrm_vm_noop_request -- noop request parameters
 * @client: 32 bits value transfered
 */
struct mmrm_vm_noop_request {
	u32 client_id;
};

/**
 * struct setvalue_request -- mmrm setvalue parameters
 * @client: client type, definition see msm_mmrm.h
 * @data: client info, definition see msm_mmrm.h
 * @val: new clock rate value
 */
struct mmrm_vm_setvalue_request {
	u32 client_id;
	struct mmrm_client_data data;
	unsigned long val;
};

/**
 * struct mmrm_vm_setvalue_inrange_request -- mmrm setvalue_inrange parameters
 * @client: client type, definition see msm_mmrm.h
 * @data: client info, definition see msm_mmrm.h
 * @val: new clock rate value range, definition see msm_mmrm.h
 */
struct mmrm_vm_setvalue_inrange_request {
	u32 client_id;
	struct mmrm_client_data data;
	struct mmrm_client_res_value val;
};

/**
 * struct mmrm_vm_getvalue_request -- mmrm getvalue parameters
 * @client: client type, definition see msm_mmrm.h
 * @val: current clock rate value range, definition see msm_mmrm.h
 */
struct mmrm_vm_getvalue_request {
	u32 client_id;
};

/**
 * struct mmrm_vm_api_request_msg -- mmrm request API message unified data definition
 * @hd: mmrm API request message head
 * @data: parameters mmrm API needs per API message cmd id
 */
struct mmrm_vm_api_request_msg {
	struct mmrm_vm_api_msg_head hd;
	union {
		struct mmrm_vm_register_request reg;
		struct mmrm_vm_deregister_request dereg;
		struct mmrm_vm_setvalue_request setval;
		struct mmrm_vm_setvalue_inrange_request setval_range;
		struct mmrm_vm_getvalue_request getval;
		struct mmrm_vm_noop_request lptest;
	} data;
};

/**
 * struct mmrm_vm_register_response -- mmrm_client_register API response message
 * @client: handle for registered client
 */
struct mmrm_vm_register_response {
	u32 client_id;
};

/**
 * struct mmrm_vm_deregister_response -- mmrm_client_deregister API response message
 * @ret_code: indicates if the mmrm_client_deregister is successful
 */
struct mmrm_vm_deregister_response {
	int ret_code;
};

/**
 * struct mmrm_vm_noop_response -- noop request's response message
 * @ret_code: return inetger
 */
struct mmrm_vm_noop_response {
	int ret_code;
};

/**
 * struct mmrm_vm_setvalue_response -- mmrm_client_set_value API response message
 * @val: value that mmrm_client_set_value return
 */
struct mmrm_vm_setvalue_response {
	unsigned long val;
};

/**
 * struct mmrm_vm_setvalue_inrange_response -- mmrm_client_set_value_in_range API response message
 * @ret_code: value that mmrm_client_set_value_in_range return
 */
struct mmrm_vm_setvalue_inrange_response {
	int ret_code;
};

/**
 * struct mmrm_vm_getvalue_response -- mmrm_client_get_value API response message
 * @val: value that mmrm_client_get_value return
 */
struct mmrm_vm_getvalue_response {
	struct mmrm_client_res_value val;
};

/**
 * struct mmrm_vm_api_response_msg -- mmrm response message unified data
 * @hd: mmrm API response message head
 * @data: data that mmrm API return per API response message id
 */
struct mmrm_vm_api_response_msg {
	struct mmrm_vm_api_msg_head hd;
	union {
		struct mmrm_vm_register_response reg;
		struct mmrm_vm_deregister_response dereg;
		struct mmrm_vm_setvalue_response setval;
		struct mmrm_vm_setvalue_inrange_response setval_range;
		struct mmrm_vm_getvalue_response getval;
		struct mmrm_vm_noop_response lptest;
	} data;
};

/**
 * struct mmrm_vm_request_msg_pkt -- mmrm request packet that is sent through gunyah API
 * @hdr: message head for checking message valid
 * @msg: data that is needed by mmrm API
 */
struct mmrm_vm_request_msg_pkt {
	struct mmrm_vm_msg_hdr hdr;
	struct mmrm_vm_api_request_msg msg;
	u64 start_time_ns;
};

/**
 * struct mmrm_vm_response_msg_pkt -- mmrm response packet that is sent through gunyah API
 * @hdr: message head for checking message valid
 * @msg: data that is returned by mmrm API
 */
struct mmrm_vm_response_msg_pkt {
	struct mmrm_vm_msg_hdr hdr;
	struct mmrm_vm_api_response_msg msg;
};

#endif /* __MMRM_VM_INTERNAL_H__ */



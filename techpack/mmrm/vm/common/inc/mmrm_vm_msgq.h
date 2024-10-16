/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __MMRM_VM_MSGQ_H__
#define __MMRM_VM_MSGQ_H__

#include <linux/gunyah/gh_msgq.h>

#define MMRM_VM_VER_1 1       // mmrm version, for message valid check

#define MMRM_VM_MAX_PKT_SZ  1024      // mmrm max gunyah packet size

#define MMRM_VM_MSG_STATUS_NOTIFIER   0x01

/**
 * mmrm_vm_pkt_type: mmrm transfer type, for message valid check
 * @MMRM_VM_TYPE_DATA: request/response data
 */
enum mmrm_vm_pkt_type {
	MMRM_VM_TYPE_DATA = 1,
};

struct mmrm_vm_driver_data;

/**
 * struct mmrm_vm_msg_hdr - mmrm vm packet header
 * @version: protocol version
 * @type: packet type; one of MMRM_VM_TYPE_* in mmrm_vm_pkt_type
 * @flags: Reserved for future use
 * @size: length of packet, excluding this header
 */
struct mmrm_vm_msg_hdr {
	u8 version;
	u8 type;
	u8 flags;
	u8 resv;
	u32 size;
};

/**
 * mmrm_vm_msg - message that be received.
 * @link - list head
 * @msg_size - message size
 * @msg_buf - message buffer
 */
struct mmrm_vm_msg {
	struct list_head link;
	size_t msg_size;
	u8 msg_buf[GH_MSGQ_MAX_MSG_SIZE_BYTES];
};

/**
 * mmrm_vm_msgq_info - gunyah info.
 * @peer_id: notification callback check if message is from SVM
 * @msgq_handle - registered msg queue handle with gunyah api
 * @msgq_label - message queue label
 * @status: indicate init status
 * @pvt_nb - notifier info
 */
struct mmrm_vm_gh_msgq_info {
	int  peer_id;
	void *msgq_handle;
	int  msgq_label;
	int status;
	struct notifier_block pvt_nb;
};

/**
 * struct mmrm_vm_msg_q -- svm mmrm API caller queue that wait for mmrm API return
 * @link: list head
 * @m_req: request message pointer
 * @m_resp: response message buffer pointer
 * @complete: sync mmrm API response and caller
 */
struct mmrm_vm_msg_q {
	struct list_head link;
	struct mmrm_vm_request_msg_pkt *m_req;
	struct mmrm_vm_response_msg_pkt *m_resp;
	struct completion complete;
};

/**
 * mmrm_vm_msgq_init - initialize display message queue: both TX and RX
 * @mmrm_vm - handle to mmrm_vm_data_priv
 */
int mmrm_vm_msgq_init(struct mmrm_vm_driver_data *mmrm_vm);

/**
 * mmrm_vm_msgq_deinit - deinitialize display message queue: both TX and RX
 * @mmrm_vm - handle to mmrm_vm_data_priv
 */
int mmrm_vm_msgq_deinit(struct mmrm_vm_driver_data *mmrm_vm);

/**
 * mmrm_vm_msgq_send - send custom messages across VM's
 * @mmrm_vm - handle to mmrm_vm_data_priv
 * @msg - payload data
 * @msg_size - size of the payload_data
 */
int mmrm_vm_msgq_send(struct mmrm_vm_driver_data *mmrm_vm, void *msg, size_t msg_size);
#endif // __MMRM_VM_MSGQ_H__


/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __GH_RM_DRV_PRIVATE_H
#define __GH_RM_DRV_PRIVATE_H

#include <linux/types.h>

#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_common.h>

extern bool gh_rm_core_initialized;

/* Resource Manager Header */
struct gh_rm_rpc_hdr {
	u8 version:4,
		hdr_words:4;
	u8 type:2,
		fragments:6;
	u16 seq;
	u32 msg_id;
} __packed;

/* Standard reply header */
struct gh_rm_rpc_reply_hdr {
	struct gh_rm_rpc_hdr rpc_hdr;
	u32 err_code;
} __packed;

/* VM specific properties to be cached */
struct gh_vm_property {
	gh_vmid_t vmid;
	u8 *guid;
	char *uri;
	char *name;
	char *sign_auth;
};

/* RPC Header versions */
#define GH_RM_RPC_HDR_VERSION_ONE	0x1

/* RPC Header words */
#define GH_RM_RPC_HDR_WORDS		0x2

/* RPC Message types */
#define GH_RM_RPC_TYPE_CONT		0x0
#define GH_RM_RPC_TYPE_REQ		0x1
#define GH_RM_RPC_TYPE_RPLY		0x2
#define GH_RM_RPC_TYPE_NOTIF		0x3

/* RPC Message IDs */
/* Call type Message IDs that has a request/reply pattern */
/* Message IDs: Informative */
#define GH_RM_RPC_MSG_ID_CALL_GET_IDENT			0x00000001
#define GH_RM_RPC_MSG_ID_CALL_GET_FEATURES		0x00000002

/* Message IDs: Memory management */
#define GH_RM_RPC_MSG_ID_CALL_MEM_DONATE		0x51000010
#define GH_RM_RPC_MSG_ID_CALL_MEM_ACCEPT		0x51000011
#define GH_RM_RPC_MSG_ID_CALL_MEM_LEND			0x51000012
#define GH_RM_RPC_MSG_ID_CALL_MEM_SHARE			0x51000013
#define GH_RM_RPC_MSG_ID_CALL_MEM_RELEASE		0x51000014
#define GH_RM_RPC_MSG_ID_CALL_MEM_RECLAIM		0x51000015
#define GH_RM_RPC_MSG_ID_CALL_MEM_NOTIFY		0x51000017

/* Message IDs: extensions for hyp-assign */
#define GH_RM_RPC_MSG_ID_CALL_MEM_QCOM_LOOKUP_SGL	0x5100001A

/* Message IDs: VM Management */
#define GH_RM_RPC_MSG_ID_CALL_VM_ALLOCATE		0x56000001
#define GH_RM_RPC_MSG_ID_CALL_VM_DEALLOCATE		0x56000002
#define GH_RM_RPC_MSG_ID_CALL_VM_START			0x56000004
#define GH_RM_RPC_MSG_ID_CALL_VM_STOP			0x56000005
#define GH_RM_RPC_MSG_ID_CALL_VM_RESET			0x56000006

/* Message IDs: VM Query */
#define GH_RM_RPC_MSG_ID_CALL_VM_GET_ID			0x56000010
#define GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_URI		0x56000011
#define GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_GUID		0x56000012
#define GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_NAME		0x56000013
#define GH_RM_RPC_MSG_ID_CALL_VM_GET_STATE		0x56000017
#define GH_RM_RPC_MSG_ID_CALL_VM_GET_HYP_RESOURCES	0x56000020
#define GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_HYP_CAPIDS	0x56000021
#define GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_HYP_IRQS	0X56000022

/* Message IDs: VM Configuration */
#define GH_RM_RPC_MSG_ID_CALL_VM_IRQ_ACCEPT		0x56000050
#define GH_RM_RPC_MSG_ID_CALL_VM_IRQ_LEND		0x56000051
#define GH_RM_RPC_MSG_ID_CALL_VM_IRQ_RELEASE		0x56000052
#define GH_RM_RPC_MSG_ID_CALL_VM_IRQ_RECLAIM		0x56000053
#define GH_RM_RPC_MSG_ID_CALL_VM_IRQ_NOTIFY		0x56000054
#define GH_RM_RPC_MSG_ID_CALL_VM_IRQ_UNMAP		0x56000055

/* Message IDs: VM Services */
#define GH_RM_RPC_MSG_ID_CALL_VM_SET_STATUS		0x56000080
#define GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_OPEN		0x56000081
#define GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_CLOSE		0x56000082
#define GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_WRITE		0x56000083
#define GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_FLUSH		0x56000084

/* Message IDs: VM-Host Query */
#define GH_RM_RPC_MSG_ID_CALL_VM_HOST_GET_TYPE		0x560000A0

/* End Call type Message IDs */
/* End RPC Message IDs */

/* Call: VM_ALLOCATE */
struct gh_vm_allocate_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

struct gh_vm_allocate_resp_payload {
	u32 vmid;
} __packed;

/* Call: VM_DEALLOCATE */
struct gh_vm_deallocate_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

/* Call: VM_START */
struct gh_vm_start_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

struct gh_vm_start_resp_payload {
	u32 response;
} __packed;

/* Call: VM_STOP */
struct gh_vm_stop_req_payload {
	gh_vmid_t vmid;
	u8 flags;
	u8 reserved;
	u32 stop_reason;
} __packed;

/* Call: VM_RESET */
struct gh_vm_reset_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

/* Call: VM_SET_STATUS */
struct gh_vm_set_status_req_payload {
	u8 vm_status;
	u8 os_status;
	u16 app_status;
} __packed;

/* Call: VM_GET_STATE */
struct gh_vm_get_state_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

struct gh_vm_get_state_resp_payload {
	u8 vm_status;
	u8 os_status;
	u16 app_status;
} __packed;

/* Call: CONSOLE_OPEN, CONSOLE_CLOSE, CONSOLE_FLUSH */
struct gh_vm_console_common_req_payload {
	gh_vmid_t vmid;
	u16 reserved0;
} __packed;

/* Call: CONSOLE_WRITE */
struct gh_vm_console_write_req_payload {
	gh_vmid_t vmid;
	u16 num_bytes;
	u8 data[0];
} __packed;

/* Call: GET_ID */
#define GH_RM_ID_TYPE_GUID		0
#define GH_RM_ID_TYPE_URI		1
#define GH_RM_ID_TYPE_NAME		2
#define GH_RM_ID_TYPE_SIGN_AUTH		3

struct gh_vm_get_id_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

struct gh_vm_get_id_resp_entry {
	u8 id_type;
	u8 reserved;
	u16 id_size;
	char id_info[];
} __packed;

struct gh_vm_get_id_resp_payload {
	u32 n_id_entries;
	struct gh_vm_get_id_resp_entry resp_entries[];
} __packed;

enum gh_vm_lookup_type {
	GH_VM_LOOKUP_NAME,
	GH_VM_LOOKUP_URI,
	GH_VM_LOOKUP_GUID,
};

struct gh_vm_lookup_char_req_payload {
	u16 size;
	u16 reserved;
	char data[];
} __packed;

struct gh_vm_lookup_resp_entry {
	u16 vmid;
	u16 reserved;
} __packed;

struct gh_vm_lookup_resp_payload {
	u32 n_id_entries;
	struct gh_vm_lookup_resp_entry resp_entries[];
} __packed;

/* Message ID headers */
/* Call: VM_GET_HYP_RESOURCES */
#define GH_RM_RES_TYPE_DB_TX		0
#define GH_RM_RES_TYPE_DB_RX		1
#define GH_RM_RES_TYPE_MQ_TX		2
#define GH_RM_RES_TYPE_MQ_RX		3
#define GH_RM_RES_TYPE_VCPU		4
#define GH_RM_RES_TYPE_VPMGRP		5
#define GH_RM_RES_TYPE_VIRTIO_MMIO	6

struct gh_vm_get_hyp_res_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
} __packed;

struct gh_vm_get_hyp_res_resp_entry {
	u8 res_type;
	u8 reserved;
	gh_vmid_t partner_vmid;
	u32 resource_handle;
	u32 resource_label;
	u32 cap_id_low;
	u32 cap_id_high;
	u32 virq_handle;
	u32 virq;
	u32 base_low;
	u32 base_high;
	u32 size_low;
	u32 size_high;
} __packed;

struct gh_vm_get_hyp_res_resp_payload {
	u32 n_resource_entries;
	struct gh_vm_get_hyp_res_resp_entry resp_entries[];
} __packed;

/* Call: VM_IRQ_ACCEPT */
struct gh_vm_irq_accept_req_payload {
	gh_virq_handle_t virq_handle;
	s32 virq;
} __packed;

struct gh_vm_irq_accept_resp_payload {
	s32 virq;
} __packed;

/* Call: VM_IRQ_LEND */
struct gh_vm_irq_lend_req_payload {
	gh_vmid_t vmid;
	u16 reserved;
	s32 virq;
	s32 label;
} __packed;

struct gh_vm_irq_lend_resp_payload {
	gh_virq_handle_t virq;
} __packed;

/* Call: VM_IRQ_NOTIFY */
#define GH_VM_IRQ_NOTIFY_FLAGS_LENT	BIT(0)
#define GH_VM_IRQ_NOTIFY_FLAGS_RELEASED	BIT(1)
#define GH_VM_IRQ_NOTIFY_FLAGS_ACCEPTED	BIT(2)

/* Call: VM_IRQ_RELEASE */
struct gh_vm_irq_release_req_payload {
	gh_virq_handle_t virq_handle;
} __packed;

/* Call: VM_IRQ_RECLAIM */
struct gh_vm_irq_reclaim_req_payload {
	gh_virq_handle_t virq_handle;
} __packed;

struct gh_vm_irq_notify_req_payload {
	gh_virq_handle_t virq;
	u8 flags;
	u8 reserved0;
	u16 reserved1;
	struct __packed {
		u16 num_vmids;
		u16 reserved;
		struct __packed {
			gh_vmid_t vmid;
			u16 reserved;
		} vmids[0];
	} optional[0];
} __packed;

/* Call: MEM_QCOM_LOOKUP_SGL */
/*
 * Split up the whole payload into a header and several trailing structs
 * to simplify allocation and treatment of packets with multiple flexible
 * array members.
 */
struct gh_mem_qcom_lookup_sgl_req_payload_hdr {
	u32 mem_type:8;
	u32 reserved:24;
	gh_label_t label;
} __packed;

struct gh_mem_qcom_lookup_sgl_resp_payload {
	gh_memparcel_handle_t memparcel_handle;
} __packed;

/* Call: MEM_RELEASE/MEM_RECLAIM */
struct gh_mem_release_req_payload {
	gh_memparcel_handle_t memparcel_handle;
	u32 flags:8;
	u32 reserved:24;
} __packed;

/*
 * Call: MEM_ACCEPT
 *
 * Split up the whole payload into a header and several trailing structs
 * to simplify allocation and treatment of packets with multiple flexible
 * array members.
 */
struct gh_mem_accept_req_payload_hdr {
	gh_memparcel_handle_t memparcel_handle;
	u8 mem_type;
	u8 trans_type;
	u8 flags;
	u8 reserved1;
	u32 validate_label;
} __packed;

struct gh_mem_accept_resp_payload {
	u16 n_sgl_entries;
	u16 reserved;
} __packed;

/*
 * Call: MEM_LEND/MEM_SHARE
 *
 * Split up the whole payload into a header and several trailing structs
 * to simplify allocation and treatment of packets with multiple flexible
 * array members.
 */
struct gh_mem_share_req_payload_hdr {
	u8 mem_type;
	u8 reserved1;
	u8 flags;
	u8 reserved2;
	u32 label;
} __packed;

struct gh_mem_share_resp_payload {
	gh_memparcel_handle_t memparcel_handle;
} __packed;

/* Call: MEM_NOTIFY */
struct gh_mem_notify_req_payload {
	gh_memparcel_handle_t memparcel_handle;
	u32 flags:8;
	u32 reserved1:24;
	gh_label_t mem_info_tag;
} __packed;

/* End Message ID headers */

/* Common function declerations */
void gh_init_vm_prop_table(void);
int gh_update_vm_prop_table(enum gh_vm_names vm_name,
			struct gh_vm_property *vm_prop);
void *gh_rm_call(gh_rm_msgid_t message_id,
			void *req_buff, size_t req_buff_size,
			size_t *resp_buff_size, int *reply_err_code);
struct gh_vm_get_id_resp_entry *
gh_rm_vm_get_id(gh_vmid_t vmid, u32 *out_n_entries);
int gh_rm_vm_lookup(enum gh_vm_lookup_type type, const void *name,
		    size_t size, gh_vmid_t *vmid);
struct gh_vm_get_hyp_res_resp_entry *
gh_rm_vm_get_hyp_res(gh_vmid_t vmid, u32 *out_n_entries);
int gh_msgq_populate_cap_info(int label, u64 cap_id, int direction, int irq);
#endif /* __GH_RM_DRV_PRIVATE_H */

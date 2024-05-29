// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/module.h>

#include <linux/gunyah/gh_vm.h>
#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_common.h>

#include "gh_rm_drv_private.h"

#define GH_RM_MEM_RELEASE_VALID_FLAGS GH_RM_MEM_RELEASE_CLEAR
#define GH_RM_MEM_RECLAIM_VALID_FLAGS GH_RM_MEM_RECLAIM_CLEAR
#define GH_RM_MEM_ACCEPT_VALID_FLAGS\
	(GH_RM_MEM_ACCEPT_VALIDATE_SANITIZED |\
	 GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |\
	 GH_RM_MEM_ACCEPT_VALIDATE_LABEL |\
	 GH_RM_MEM_ACCEPT_MAP_IPA_CONTIGUOUS |\
	 GH_RM_MEM_ACCEPT_DONE)
#define GH_RM_MEM_SHARE_VALID_FLAGS GH_RM_MEM_SHARE_SANITIZE
#define GH_RM_MEM_LEND_VALID_FLAGS GH_RM_MEM_LEND_SANITIZE
#define GH_RM_MEM_DONATE_VALID_FLAGS GH_RM_MEM_DONATE_SANITIZE
#define GH_RM_MEM_NOTIFY_VALID_FLAGS\
	(GH_RM_MEM_NOTIFY_RECIPIENT_SHARED |\
	 GH_RM_MEM_NOTIFY_OWNER_RELEASED | GH_RM_MEM_NOTIFY_OWNER_ACCEPTED)

static DEFINE_SPINLOCK(gh_vm_table_lock);
static struct gh_vm_property gh_vm_table[GH_VM_MAX];

void gh_init_vm_prop_table(void)
{
	size_t vm_name;

	spin_lock(&gh_vm_table_lock);

	gh_vm_table[GH_SELF_VM].vmid = 0;

	for (vm_name = GH_SELF_VM + 1; vm_name < GH_VM_MAX; vm_name++) {
		gh_vm_table[vm_name].vmid = GH_VMID_INVAL;
		gh_vm_table[vm_name].guid = NULL;
		gh_vm_table[vm_name].uri = NULL;
		gh_vm_table[vm_name].name = NULL;
		gh_vm_table[vm_name].sign_auth = NULL;
	}

	spin_unlock(&gh_vm_table_lock);
}

int gh_update_vm_prop_table(enum gh_vm_names vm_name,
			struct gh_vm_property *vm_prop)
{
	if (!vm_prop)
		return -EINVAL;

	if (vm_prop->vmid < 0 || vm_name < GH_SELF_VM || vm_name > GH_VM_MAX)
		return -EINVAL;

	spin_lock(&gh_vm_table_lock);
	if (gh_vm_table[vm_name].guid || gh_vm_table[vm_name].uri ||
	    gh_vm_table[vm_name].name || gh_vm_table[vm_name].sign_auth) {
		spin_unlock(&gh_vm_table_lock);
		return -EEXIST;
	}
	if (vm_prop->vmid) {
		gh_vm_table[vm_name].vmid = vm_prop->vmid;
	}

	if (vm_prop->guid)
		gh_vm_table[vm_name].guid = vm_prop->guid;

	if (vm_prop->uri)
		gh_vm_table[vm_name].uri = vm_prop->uri;

	if (vm_prop->name)
		gh_vm_table[vm_name].name = vm_prop->name;

	if (vm_prop->sign_auth)
		gh_vm_table[vm_name].sign_auth = vm_prop->sign_auth;
	spin_unlock(&gh_vm_table_lock);

	return 0;
}

void gh_reset_vm_prop_table_entry(gh_vmid_t vmid)
{
	size_t vm_name;

	spin_lock(&gh_vm_table_lock);

	for (vm_name = GH_SELF_VM + 1; vm_name < GH_VM_MAX; vm_name++) {
		if (vmid == gh_vm_table[vm_name].vmid) {
			gh_vm_table[vm_name].vmid = GH_VMID_INVAL;
			gh_vm_table[vm_name].uri = NULL;
			gh_vm_table[vm_name].guid = NULL;
			gh_vm_table[vm_name].name = NULL;
			gh_vm_table[vm_name].sign_auth = NULL;
			break;
		}
	}

	spin_unlock(&gh_vm_table_lock);
}

/**
 * gh_rm_get_vmid: Translate VM name to vmid
 * @vm_name: VM name to lookup
 * @vmid: out pointer to store found vmid if VM is ofund
 *
 * If gh_rm_core has not yet probed, returns -EPROBE_DEFER.
 * If no VM is known to RM with the supplied name, returns -EINVAL.
 * Returns 0 on success.
 */
int gh_rm_get_vmid(enum gh_vm_names vm_name, gh_vmid_t *vmid)
{
	gh_vmid_t _vmid;
	int ret = 0;

	if (vm_name < GH_SELF_VM || vm_name > GH_VM_MAX)
		return -EINVAL;


	spin_lock(&gh_vm_table_lock);

	_vmid = gh_vm_table[vm_name].vmid;
	if (!gh_rm_core_initialized) {
		ret = -EPROBE_DEFER;
		goto out;
	}

	if (!_vmid && vm_name != GH_SELF_VM) {
		ret = -EINVAL;
		goto out;
	}

	if (vmid)
		*vmid = _vmid;

out:
	spin_unlock(&gh_vm_table_lock);
	return ret;
}
EXPORT_SYMBOL(gh_rm_get_vmid);

/**
 * gh_rm_get_vm_name: Translate vmid to vm name
 * @vmid: vmid to lookup
 * @vm_name: out pointer to store found VM name if vmid is found
 *
 * If no VM is known to RM with the supplied VMID, -EINVAL is returned.
 * 0 otherwise.
 */
int gh_rm_get_vm_name(gh_vmid_t vmid, enum gh_vm_names *vm_name)
{
	enum gh_vm_names i;

	spin_lock(&gh_vm_table_lock);

	for (i = 0; i < GH_VM_MAX; i++)
		if (gh_vm_table[i].vmid == vmid) {
			if (vm_name)
				*vm_name = i;
			spin_unlock(&gh_vm_table_lock);
			return 0;
		}

	spin_unlock(&gh_vm_table_lock);

	return -EINVAL;
}
EXPORT_SYMBOL(gh_rm_get_vm_name);

/**
 * gh_rm_get_vminfo: Obtain Vm related info with vm name
 * @vm_name: VM name to lookup
 * @vm: out pointer to store id information about VM
 *
 * If no VM is known to RM with the supplied name, -EINVAL is returned.
 * 0 otherwise.
 */
int gh_rm_get_vminfo(enum gh_vm_names vm_name, struct gh_vminfo *vm)
{
	if (!vm)
		return -EINVAL;

	spin_lock(&gh_vm_table_lock);

	vm->guid = gh_vm_table[vm_name].guid;
	vm->uri = gh_vm_table[vm_name].uri;
	vm->name = gh_vm_table[vm_name].name;
	vm->sign_auth = gh_vm_table[vm_name].sign_auth;

	spin_unlock(&gh_vm_table_lock);

	return 0;
}
EXPORT_SYMBOL(gh_rm_get_vminfo);

/**
 * gh_rm_vm_get_id: Get identification info about a VM
 * @vmid: vmid whose info is needed. Pass 0 for self
 * @n_entries: The number of the resource entries that's returned to the caller
 *
 * The function returns an array of type 'struct gh_vm_get_id_resp_entry',
 * in which each entry specifies identification info about the vm. The number
 * of entries in the array is returned by 'n_entries'. The caller must kfree
 * the returned pointer when done.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
struct gh_vm_get_id_resp_entry *
gh_rm_vm_get_id(gh_vmid_t vmid, u32 *n_entries)
{
	struct gh_vm_get_id_resp_payload *resp_payload;
	struct gh_vm_get_id_req_payload req_payload = {
		.vmid = vmid
	};
	struct gh_vm_get_id_resp_entry *resp_entries, *temp_entry;
	size_t resp_payload_size, resp_entries_size = 0;
	int err, reply_err_code, i;

	if (!n_entries)
		return ERR_PTR(-EINVAL);

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_GET_ID,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: GET_ID failed with err: %d\n",
			__func__, err);
		return ERR_PTR(err);
	}

	/* The response payload should contain all the resource entries */
	temp_entry = resp_payload->resp_entries;
	for (i = 0; i < resp_payload->n_id_entries; i++) {
		resp_entries_size +=
			sizeof(*temp_entry) + round_up(temp_entry->id_size, 4);
		temp_entry = (void *)temp_entry + sizeof(*temp_entry) +
			     round_up(temp_entry->id_size, 4);
	}
	if (resp_entries_size != resp_payload_size - sizeof(*n_entries)) {
		pr_err("%s: Invalid size received for GET_ID: %u expect %u\n",
		       __func__, resp_payload_size, resp_entries_size);
		resp_entries = ERR_PTR(-EINVAL);
		goto out;
	}
	resp_entries = kmemdup(resp_payload->resp_entries, resp_entries_size,
			       GFP_KERNEL);
	if (!resp_entries) {
		resp_entries = ERR_PTR(-ENOMEM);
		goto out;
	}

	*n_entries = resp_payload->n_id_entries;

out:
	kfree(resp_payload);
	return resp_entries;
}

static int gh_rm_vm_lookup_name_uri(gh_rm_msgid_t msg_id, const char *data,
				    size_t size, gh_vmid_t *vmid)
{
	struct gh_vm_lookup_resp_payload *resp_payload;
	struct gh_vm_lookup_char_req_payload *req_payload;
	size_t resp_payload_size, req_payload_size;
	int reply_err_code;
	int ret = 0;

	if (!data || !vmid)
		return -EINVAL;

	req_payload_size = sizeof(*req_payload) + round_up(size, 4);
	req_payload = kzalloc(req_payload_size, GFP_KERNEL);

	if (!req_payload)
		return -ENOMEM;

	req_payload->size = size;
	memcpy(req_payload->data, data, size);

	resp_payload = gh_rm_call(msg_id, req_payload, req_payload_size,
				  &resp_payload_size, &reply_err_code);

	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: lookup name/uri failed with err: %d\n", __func__, (int)ret);
		goto out;
	}

	if (resp_payload->n_id_entries == 1) {
		*vmid = resp_payload->resp_entries->vmid;
	} else if (resp_payload->n_id_entries == 0) {
		pr_err("%s: No VMID found from lookup %s\n", __func__, data);
		ret = -EINVAL;
	} else {
		pr_err("%s: More than one VMID received from lookup %s\n",
		       __func__, data);
		ret = -EINVAL;
	}

	kfree(resp_payload);
out:
	kfree(req_payload);
	return ret;
}

static int gh_rm_vm_lookup_guid(const u8 *data, gh_vmid_t *vmid)
{
	struct gh_vm_lookup_resp_payload *resp_payload;
	size_t resp_payload_size;
	int reply_err_code;
	int ret = 0;

	if (!data || !vmid)
		return -EINVAL;

	resp_payload =
		gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_GUID, (void *)data,
			   16, &resp_payload_size, &reply_err_code);

	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: lookup guid failed with err: %d\n", __func__, (int)ret);
		return ret;
	}

	if (resp_payload->n_id_entries == 1) {
		*vmid = resp_payload->resp_entries->vmid;
	} else if (resp_payload->n_id_entries == 0) {
		pr_err("%s: No VMID found from lookup %pUB\n", __func__, data);
		ret = -EINVAL;
	} else {
		pr_err("%s: More than one VMID received from lookup: %pUB\n",
		       __func__, data);
		ret = -EINVAL;
	}

	kfree(resp_payload);
	return ret;
}

/**
 * gh_rm_vm_lookup: Get vmid from name
 * @type: which type of property need to lookup
 * @data: name/uri/guid whose vmid is needed
 * @size: data size
 * @vmid: vmid return to caller
 *
 */
int gh_rm_vm_lookup(enum gh_vm_lookup_type type, const void *data, size_t size,
		    gh_vmid_t *vmid)
{
	int ret = 0;

	switch (type) {
	case GH_VM_LOOKUP_NAME:
		ret = gh_rm_vm_lookup_name_uri(
			GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_NAME,
			(const char *)data, size, vmid);
		break;
	case GH_VM_LOOKUP_URI:
		ret = gh_rm_vm_lookup_name_uri(
			GH_RM_RPC_MSG_ID_CALL_VM_LOOKUP_URI, (const char *)data,
			size, vmid);
		break;
	case GH_VM_LOOKUP_GUID:
		if (size != 16) {
			pr_err("Invalid GUID size=%d\n", size);
			ret = -EINVAL;
		} else
			ret = gh_rm_vm_lookup_guid((const u8 *)data, vmid);
		break;
	default:
		pr_err("Invalid lookup type=%d\n", type);
		break;
	}

	return ret;
}

/**
 * gh_rm_vm_get_status: Get the status of a particular VM
 * @vmid: The vmid of tehe VM. Pass 0 for self.
 *
 * The function returns a pointer to gh_vm_status containing
 * the status of the VM for the requested vmid. The caller
 * must kfree the memory when done reading the contents.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the
 * caller is responsible to check it with IS_ERR_OR_NULL().
 */
struct gh_vm_status *gh_rm_vm_get_status(gh_vmid_t vmid)
{
	struct gh_vm_get_state_req_payload req_payload = {
		.vmid = vmid,
	};
	struct gh_vm_get_state_resp_payload *resp_payload;
	struct gh_vm_status *gh_vm_status;
	int err, reply_err_code = 0;
	size_t resp_payload_size;

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_GET_STATE,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: Failed to call VM_GET_STATE: %d\n",
			__func__, err);
		if (resp_payload) {
			gh_vm_status = ERR_PTR(err);
			goto out;
		}
		return ERR_PTR(err);
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_GET_STATE: %u\n",
			__func__, resp_payload_size);
		gh_vm_status = ERR_PTR(-EINVAL);
		goto out;
	}

	gh_vm_status = kmemdup(resp_payload, resp_payload_size, GFP_KERNEL);
	if (!gh_vm_status)
		gh_vm_status = ERR_PTR(-ENOMEM);

out:
	kfree(resp_payload);
	return gh_vm_status;
}
EXPORT_SYMBOL(gh_rm_vm_get_status);

/**
 * gh_rm_vm_set_status: Set the status of this VM
 * @gh_vm_status: The status to set
 *
 * The function sets this VM's status as per gh_vm_status.
 * It returns 0 upon success and a negative error code
 * upon failure.
 */
int gh_rm_vm_set_status(struct gh_vm_status gh_vm_status)
{
	struct gh_vm_set_status_req_payload req_payload = {
		.vm_status = gh_vm_status.vm_status,
		.os_status = gh_vm_status.os_status,
		.app_status = gh_vm_status.app_status,
	};
	size_t resp_payload_size;
	int reply_err_code = 0;
	void *resp;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_SET_STATUS,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (IS_ERR(resp)) {
		pr_err("%s: Failed to call VM_SET_STATUS: %d\n",
			__func__, PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: VM_SET_STATUS returned error: %d\n",
			__func__, reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for VM_SET_STATUS: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_vm_set_status);

/**
 * gh_rm_vm_set_vm_status: Set the vm status
 * @vm_status: The vm_status to set
 *
 * The function returns 0 on success and a negative
 * error code upon failure.
 */
int gh_rm_vm_set_vm_status(u8 vm_status)
{
	int ret = 0;
	struct gh_vm_status *gh_vm_status;

	gh_vm_status = gh_rm_vm_get_status(0);
	if (IS_ERR_OR_NULL(gh_vm_status))
		return PTR_ERR(gh_vm_status);

	gh_vm_status->vm_status = vm_status;
	ret = gh_rm_vm_set_status(*gh_vm_status);

	kfree(gh_vm_status);

	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_set_vm_status);

/**
 * gh_rm_vm_set_os_status: Set the OS status. Once the VM starts booting,
 * there are various stages during the boot up that status of the
 * Operating System can be set like early_boot, boot, init, run etc.
 * @os_status: The os_status to set
 *
 * The function returns 0 on success and a negative
 * error code upon failure.
 */
int gh_rm_vm_set_os_status(u8 os_status)
{
	int ret = 0;
	struct gh_vm_status *gh_vm_status;

	gh_vm_status = gh_rm_vm_get_status(0);
	if (IS_ERR_OR_NULL(gh_vm_status))
		return PTR_ERR(gh_vm_status);

	gh_vm_status->os_status = os_status;
	ret = gh_rm_vm_set_status(*gh_vm_status);

	kfree(gh_vm_status);

	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_set_os_status);

/**
 * gh_rm_vm_set_app_status: Set the app status
 * @app_status: The app_status to set
 *
 * The function returns 0 on success and a negative
 * error code upon failure.
 */
int gh_rm_vm_set_app_status(u16 app_status)
{
	int ret = 0;
	struct gh_vm_status *gh_vm_status;

	gh_vm_status = gh_rm_vm_get_status(0);
	if (IS_ERR_OR_NULL(gh_vm_status))
		return PTR_ERR(gh_vm_status);

	gh_vm_status->app_status = app_status;
	ret = gh_rm_vm_set_status(*gh_vm_status);

	kfree(gh_vm_status);

	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_set_app_status);

/**
 * gh_rm_vm_get_hyp_res: Get info about a series of resources for this VM
 * @vmid: vmid whose info is needed. Pass 0 for self
 * @n_entries: The number of the resource entries that's returned to the caller
 *
 * The function returns an array of type 'struct gh_vm_get_hyp_res_resp_entry',
 * in which each entry specifies info about a particular resource. The number
 * of entries in the array is returned by 'n_entries'. The caller must kfree
 * the returned pointer when done.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
struct gh_vm_get_hyp_res_resp_entry *
gh_rm_vm_get_hyp_res(gh_vmid_t vmid, u32 *n_entries)
{
	struct gh_vm_get_hyp_res_resp_payload *resp_payload;
	struct gh_vm_get_hyp_res_req_payload req_payload = {
		.vmid = vmid
	};
	struct gh_vm_get_hyp_res_resp_entry *resp_entries;
	size_t resp_payload_size, resp_entries_size;
	int err, reply_err_code;

	if (!n_entries)
		return ERR_PTR(-EINVAL);

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_GET_HYP_RESOURCES,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: GET_HYP_RESOURCES failed with err: %d\n",
			__func__, err);
		return ERR_PTR(err);
	}

	/* The response payload should contain all the resource entries */
	if (resp_payload_size < sizeof(*n_entries) ||
		(sizeof(*resp_entries) &&
		(resp_payload->n_resource_entries > U32_MAX / sizeof(*resp_entries))) ||
		(sizeof(*n_entries) > (U32_MAX -
		(resp_payload->n_resource_entries * sizeof(*resp_entries)))) ||
		resp_payload_size != sizeof(*n_entries) +
		(resp_payload->n_resource_entries * sizeof(*resp_entries))) {
		pr_err("%s: Invalid size received for GET_HYP_RESOURCES: %u\n",
			__func__, resp_payload_size);
		resp_entries = ERR_PTR(-EINVAL);
		goto out;
	}

	resp_entries_size = sizeof(*resp_entries) *
				resp_payload->n_resource_entries;
	resp_entries = kmemdup(resp_payload->resp_entries, resp_entries_size,
			       GFP_KERNEL);
	if (!resp_entries) {
		resp_entries = ERR_PTR(-ENOMEM);
		goto out;
	}

	*n_entries = resp_payload->n_resource_entries;

out:
	kfree(resp_payload);
	return resp_entries;
}

/**
 * gh_rm_vm_irq_notify: Notify an IRQ to another VM
 * @vmids: VMs to notify the handle about
 * @num_vmids: number of VMs to notify the handle about
 * @flags: notification reason
 * @virq_handle: Response handle which RM will accept from the other VM to take
 *		 the lent interrupt
 */
static int gh_rm_vm_irq_notify(const gh_vmid_t *vmids, unsigned int num_vmids,
				u16 flags, gh_virq_handle_t virq_handle)
{
	void *resp;
	struct gh_vm_irq_notify_req_payload *req_payload;
	size_t resp_payload_size, req_payload_size;
	int ret = 0, reply_err_code;
	unsigned int i;


	if (!(flags & GH_VM_IRQ_NOTIFY_FLAGS_LENT) && num_vmids)
		return -EINVAL;

	if (num_vmids > U16_MAX)
		return -EINVAL;

	req_payload_size = sizeof(*req_payload);
	if (flags & GH_VM_IRQ_NOTIFY_FLAGS_LENT)
		req_payload_size += sizeof(*(req_payload->optional)) +
			(sizeof(req_payload->optional->vmids[0]) * num_vmids);
	req_payload = kzalloc(req_payload_size, GFP_KERNEL);

	if (!req_payload)
		return -ENOMEM;

	req_payload->virq = virq_handle;
	req_payload->flags = flags;
	if (flags & GH_VM_IRQ_NOTIFY_FLAGS_LENT) {
		req_payload->optional[0].num_vmids = num_vmids;
		for (i = 0; i < num_vmids; i++)
			req_payload->optional[0].vmids[i].vmid = vmids[i];
	}


	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_IRQ_NOTIFY,
			  req_payload, req_payload_size,
			  &resp_payload_size, &reply_err_code);
	kfree(req_payload);
	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send IRQ_NOTIFY to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: IRQ_NOTIFY returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for IRQ_NOTIFY: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * gh_rm_vm_irq_lend: Lend an IRQ to another VM
 * @vmid: VM to lend the interrupt to
 * @virq: Virtual IRQ number to lend
 * @label: Label to give to VM so it may know how to associate the interrupt
 * @virq_handle: Response handle which RM will accept from the other VM to take
 *		 the lent interrupt
 */
int gh_rm_vm_irq_lend(gh_vmid_t vmid, int virq, int label,
			     gh_virq_handle_t *virq_handle)
{
	struct gh_vm_irq_lend_resp_payload *resp_payload;
	struct gh_vm_irq_lend_req_payload req_payload = {0};
	size_t resp_payload_size;
	int ret = 0, reply_err_code;

	req_payload.vmid = vmid;
	req_payload.virq = virq;
	req_payload.label = label;

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_IRQ_LEND,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: VM_IRQ_LEND failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_IRQ_LEND: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
		goto out;
	}

	if (virq_handle)
		*virq_handle = resp_payload->virq;
out:
	kfree(resp_payload);
	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_irq_lend);

/**
 * gh_rm_vm_irq_lend_notify: Lend an IRQ to a VM and notify the VM about it
 * @vmid: VM to lend interrupt to
 * @virq: Virtual IRQ number to lend
 * @label: Label to give to VM so it may know how to associate the interrupt
 * @virq_handle: vIRQ handle generated by hypervisor to reperesent the interrupt
 *               which can be used later to know when the interrupt has been
 *               released
 *
 * This function performs interrupt sharing flow for "HLOS" described in
 * Resource Manager High Level Design Sec. 3.3.3.
 */
int gh_rm_vm_irq_lend_notify(gh_vmid_t vmid, gh_virq_handle_t virq_handle)
{
	return gh_rm_vm_irq_notify(&vmid, 1, GH_VM_IRQ_NOTIFY_FLAGS_LENT,
				   virq_handle);
}
EXPORT_SYMBOL(gh_rm_vm_irq_lend_notify);

/**
 * gh_rm_vm_irq_release: Return a lent IRQ
 * @virq_handle: IRQ handle to be released
 */
int gh_rm_vm_irq_release(gh_virq_handle_t virq_handle)
{
	struct gh_vm_irq_release_req_payload req_payload = {0};
	void *resp;
	int ret = 0, reply_err_code;
	size_t resp_payload_size;

	req_payload.virq_handle = virq_handle;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_IRQ_RELEASE,
			  &req_payload, sizeof(req_payload),
			  &resp_payload_size, &reply_err_code);

	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send IRQ_RELEASE to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: IRQ_RELEASE returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for IRQ_RELEASE: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_irq_release);

/**
 * gh_rm_vm_irq_release_notify: Release IRQ back to a VM and notify that it has
 * been released.
 * @vmid: VM to release interrupt to
 * @virq_handle: Virtual IRQ handle to release
 */
int gh_rm_vm_irq_release_notify(gh_vmid_t vmid, gh_virq_handle_t virq_handle)
{
	return gh_rm_vm_irq_notify(NULL, 0, GH_VM_IRQ_NOTIFY_FLAGS_RELEASED,
				   virq_handle);
}
EXPORT_SYMBOL(gh_rm_vm_irq_release_notify);

/**
 * gh_rm_vm_irq_accept: Bind the virq number to the supplied virq_handle
 * @virq_handle: The virtual IRQ handle (for example, obtained via
 *               call to gh_rm_get_hyp_resources())
 * @virq: The virtual IRQ number to bind to. Note that this is the virtual
 *        GIC IRQ number and not the linux IRQ number. Pass -1 here if the
 *        caller wants the Resource Manager VM to allocate a number
 *
 * If provided -1 for virq, the function returns the new IRQ number, else
 * the one that was already provided.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int gh_rm_vm_irq_accept(gh_virq_handle_t virq_handle, int virq)
{
	struct gh_vm_irq_accept_resp_payload *resp_payload;
	struct gh_vm_irq_accept_req_payload req_payload = {0};
	size_t resp_payload_size;
	int ret, reply_err_code;

	/* -1 is valid for virq if requesting for a new number */
	if (virq < -1)
		return -EINVAL;

	req_payload.virq_handle = virq_handle;
	req_payload.virq = virq;

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_IRQ_ACCEPT,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR_OR_NULL(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s: VM_IRQ_ACCEPT failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	if (virq == -1 && resp_payload_size != sizeof(*resp_payload)) {
		pr_err("%s: Invalid size received for VM_IRQ_ACCEPT: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
		goto out;
	}

	ret = virq == -1 ? resp_payload->virq : virq;
out:
	kfree(resp_payload);
	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_irq_accept);

/**
 * gh_rm_vm_irq_release_notify: Release IRQ back to a VM and notify that it has
 * been released.
 * @vmid: VM to release interrupt to
 * @virq_handle: Virtual IRQ handle to release
 */
int gh_rm_vm_irq_accept_notify(gh_vmid_t vmid, gh_virq_handle_t virq_handle)
{
	return gh_rm_vm_irq_notify(NULL, 0, GH_VM_IRQ_NOTIFY_FLAGS_ACCEPTED,
				   virq_handle);
}
EXPORT_SYMBOL(gh_rm_vm_irq_accept_notify);

/**
 * gh_rm_vm_irq_reclaim: Return a lent IRQ
 * @virq_handle: IRQ handle to be reclaimed
 */
int gh_rm_vm_irq_reclaim(gh_virq_handle_t virq_handle)
{
	struct gh_vm_irq_reclaim_req_payload req_payload = {0};
	void *resp;
	int ret = 0, reply_err_code;
	size_t resp_payload_size;

	req_payload.virq_handle = virq_handle;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_IRQ_RECLAIM,
			  &req_payload, sizeof(req_payload),
			  &resp_payload_size, &reply_err_code);

	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send IRQ_RELEASE to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: IRQ_RELEASE returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for IRQ_RELEASE: %u\n",
			__func__, resp_payload_size);
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(gh_rm_vm_irq_reclaim);

/**
 * gh_rm_vm_alloc_vmid: Return a vmid associated with the vm loaded into
 *			memory. This call should be called only during
			initialization.
 * @vm_name: The enum value of the vm that has been loaded.
 * @vmid: Value of vmid read from DT. If not present in DT using 0.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int gh_rm_vm_alloc_vmid(enum gh_vm_names vm_name, int *vmid)
{
	struct gh_vm_allocate_resp_payload *resp_payload;
	struct gh_vm_allocate_req_payload req_payload = {0};
	size_t resp_payload_size;
	struct gh_vm_property vm_prop = {0};
	int err, reply_err_code;

	/* Look up for the vm_name<->vmid pair if already present.
	 * If so, return.
	 */
	if (vm_name < GH_SELF_VM || vm_name > GH_VM_MAX)
		return -EINVAL;

	spin_lock(&gh_vm_table_lock);
	if (gh_vm_table[vm_name].vmid != GH_VMID_INVAL ||
		vm_name == GH_SELF_VM) {
		pr_err("%s: VM_ALLOCATE already called for this VM\n",
			__func__);
		spin_unlock(&gh_vm_table_lock);
		return -EINVAL;
	}
	spin_unlock(&gh_vm_table_lock);

	req_payload.vmid = *vmid;

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_ALLOCATE,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR(resp_payload)) {
		err = PTR_ERR(resp_payload);
		pr_err("%s: VM_ALLOCATE failed with err: %d\n",
			__func__, err);
		return err;
	}

	if (resp_payload &&
			(resp_payload_size != sizeof(*resp_payload))) {
		pr_err("%s: Invalid size received for VM_ALLOCATE: %u\n",
			__func__, resp_payload_size);
		kfree(resp_payload);
		return -EINVAL;
	}

	if (resp_payload)
		*vmid = resp_payload->vmid;

	vm_prop.vmid = *vmid;
	err = gh_update_vm_prop_table(vm_name, &vm_prop);

	if (err) {
		pr_err("%s: Invalid vmid sent for updating table: %d\n",
			__func__, vm_prop.vmid);
		return -EINVAL;
	}

	kfree(resp_payload);
	return 0;
}
EXPORT_SYMBOL(gh_rm_vm_alloc_vmid);

/**
 * gh_rm_vm_dealloc_vmid: Deallocate an already allocated vmid
 * @vmid: The vmid to deallocate.
 *
 * The function returns 0 on success and a negative error code
 * upon failure.
 */
int gh_rm_vm_dealloc_vmid(gh_vmid_t vmid)
{
	struct gh_vm_deallocate_req_payload req_payload = {
		.vmid = vmid,
	};
	size_t resp_payload_size;
	int err, reply_err_code;
	void *resp;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_DEALLOCATE,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR(resp)) {
		err = reply_err_code;
		pr_err("%s: VM_DEALLOCATE failed with err: %d\n",
			__func__, err);
		return err;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for VM_DEALLOCATE: %u\n",
			__func__, resp_payload_size);
		kfree(resp);
		return -EINVAL;
	}

	gh_reset_vm_prop_table_entry(vmid);

	return 0;
}
EXPORT_SYMBOL(gh_rm_vm_dealloc_vmid);

/**
 * gh_rm_vm_start: Send a request to Resource Manager VM to start a VM.
 * @vmid: The vmid of the vm to be started.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int gh_rm_vm_start(int vmid)
{
	struct gh_vm_start_resp_payload *resp_payload;
	struct gh_vm_start_req_payload req_payload = {0};
	size_t resp_payload_size;
	int reply_err_code = 0;

	req_payload.vmid = (gh_vmid_t) vmid;

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_START,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code) {
		pr_err("%s: VM_START failed with err: %d\n",
			__func__, reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for VM_START: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_vm_start);

/**
 * gh_rm_vm_stop: Send a request to Resource Manager VM to stop a VM.
 * @vmid: The vmid of the vm to be stopped.
 *
 * The function encodes the error codes via ERR_PTR. Hence, the caller is
 * responsible to check it with IS_ERR_OR_NULL().
 */
int gh_rm_vm_stop(gh_vmid_t vmid, u32 stop_reason, u8 flags)
{
	struct gh_vm_stop_req_payload req_payload = {0};
	size_t resp_payload_size;
	int err, reply_err_code;
	void *resp;

	if (stop_reason >= GH_VM_STOP_MAX) {
		pr_err("%s: Invalid stop reason provided for VM_STOP\n",
			__func__);
		return -EINVAL;
	}

	req_payload.vmid = vmid;
	req_payload.stop_reason = stop_reason;
	req_payload.flags = flags;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_STOP,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR(resp)) {
		err = reply_err_code;
		pr_err("%s: VM_STOP failed with err: %d\n", __func__, err);
		return err;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for VM_STOP: %u\n",
			__func__, resp_payload_size);
		kfree(resp);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_vm_stop);

/**
 * gh_rm_vm_reset: Send a request to Resource Manager VM to free up all
 * resources used by the VM.
 * @vmid: The vmid of the vm to be cleaned up.
 *
 * The function returns 0 on success and a negative error code
 * upon failure.
 */
int gh_rm_vm_reset(gh_vmid_t vmid)
{
	struct gh_vm_reset_req_payload req_payload = {
		.vmid = vmid,
	};
	size_t resp_payload_size;
	int err, reply_err_code;
	void *resp;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_RESET,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);
	if (reply_err_code || IS_ERR(resp)) {
		err = reply_err_code;
		pr_err("%s: VM_RESET failed with err: %d\n",
			__func__, err);
		return err;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for VM_RESET: %u\n",
			__func__, resp_payload_size);
		kfree(resp);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_vm_reset);

/**
 * gh_rm_console_open: Open a console with a VM
 * @vmid: The vmid of the vm to be started.
 */
int gh_rm_console_open(gh_vmid_t vmid)
{
	void *resp;
	struct gh_vm_console_common_req_payload req_payload = {0};
	size_t resp_payload_size;
	int reply_err_code = 0;

	req_payload.vmid = vmid;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_OPEN,
			  &req_payload, sizeof(req_payload),
			  &resp_payload_size, &reply_err_code);
	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send CONSOLE_OPEN to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: CONSOLE_OPEN returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for CONSOLE_OPEN: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_console_open);

/**
 * gh_rm_console_close: Close a console with a VM
 * @vmid: The vmid of the vm whose console to close.
 */
int gh_rm_console_close(gh_vmid_t vmid)
{
	void *resp;
	struct gh_vm_console_common_req_payload req_payload = {0};
	size_t resp_payload_size;
	int reply_err_code = 0;

	req_payload.vmid = vmid;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_CLOSE,
			  &req_payload, sizeof(req_payload),
			  &resp_payload_size, &reply_err_code);
	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send CONSOLE_CLOSE to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: CONSOLE_CLOSE returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for CONSOLE_CLOSE: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_console_close);

/**
 * gh_rm_console_write: Write to a VM's console
 * @vmid: The vmid of the vm whose console to write to.
 * @buf: Buffer to write to the VM's console
 * @size: Size of the buffer
 */
int gh_rm_console_write(gh_vmid_t vmid, const char *buf, size_t size)
{
	void *resp;
	struct gh_vm_console_write_req_payload *req_payload;
	size_t resp_payload_size;
	int reply_err_code = 0;
	size_t req_payload_size = sizeof(*req_payload) + size;

	if (size < 1 || size > (U32_MAX - sizeof(*req_payload)))
		return -EINVAL;

	req_payload = kzalloc(req_payload_size, GFP_KERNEL);

	if (!req_payload)
		return -ENOMEM;

	req_payload->vmid = vmid;
	req_payload->num_bytes = size;
	memcpy(req_payload->data, buf, size);

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_WRITE,
		   req_payload, req_payload_size,
		   &resp_payload_size, &reply_err_code);
	kfree(req_payload);

	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send CONSOLE_WRITE to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: CONSOLE_WRITE returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for CONSOLE_WRITE: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_console_write);

/**
 * gh_rm_console_flush: Flush a console with a VM
 * @vmid: The vmid of the vm whose console to flush
 */
int gh_rm_console_flush(gh_vmid_t vmid)
{
	void *resp;
	struct gh_vm_console_common_req_payload req_payload = {0};
	size_t resp_payload_size;
	int reply_err_code = 0;

	req_payload.vmid = vmid;

	resp = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_VM_CONSOLE_FLUSH,
				&req_payload, sizeof(req_payload),
				&resp_payload_size, &reply_err_code);

	if (IS_ERR(resp)) {
		pr_err("%s: Unable to send CONSOLE_FLUSH to RM: %d\n", __func__,
			PTR_ERR(resp));
		return PTR_ERR(resp);
	}

	if (reply_err_code) {
		pr_err("%s: CONSOLE_FLUSH returned error: %d\n", __func__,
			reply_err_code);
		return reply_err_code;
	}

	if (resp_payload_size) {
		pr_err("%s: Invalid size received for CONSOLE_FLUSH: %u\n",
			__func__, resp_payload_size);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gh_rm_console_flush);

static void gh_rm_populate_acl_desc(struct gh_acl_desc *dst_desc,
				    struct gh_acl_desc *src_desc)
{
	u32 n_acl_entries = src_desc ? src_desc->n_acl_entries : 0;
	unsigned int i;

	dst_desc->n_acl_entries = n_acl_entries;
	for (i = 0; i < n_acl_entries; i++) {
		dst_desc->acl_entries[i].vmid = src_desc->acl_entries[i].vmid;
		dst_desc->acl_entries[i].perms = src_desc->acl_entries[i].perms;
	}
}

static void gh_rm_populate_sgl_desc(struct gh_sgl_desc *dst_desc,
				    struct gh_sgl_desc *src_desc,
				    u16 reserved_param)
{
	u32 n_sgl_entries = src_desc ? src_desc->n_sgl_entries : 0;

	dst_desc->n_sgl_entries = n_sgl_entries;
	dst_desc->reserved = reserved_param;
	if (n_sgl_entries)
		memcpy(dst_desc->sgl_entries, src_desc->sgl_entries,
		       sizeof(*dst_desc->sgl_entries) * n_sgl_entries);
}

static void gh_rm_populate_mem_attr_desc(struct gh_mem_attr_desc *dst_desc,
					 struct gh_mem_attr_desc *src_desc)
{
	u32 n_mem_attr_entries = src_desc ? src_desc->n_mem_attr_entries : 0;

	dst_desc->n_mem_attr_entries = n_mem_attr_entries;
	if (n_mem_attr_entries)
		memcpy(dst_desc->attr_entries, src_desc->attr_entries,
		       sizeof(*dst_desc->attr_entries) * n_mem_attr_entries);
}

static void gh_rm_populate_mem_request(void *req_buf, u32 fn_id,
				       struct gh_acl_desc *src_acl_desc,
				       struct gh_sgl_desc *src_sgl_desc,
				       u16 reserved_param,
				       struct gh_mem_attr_desc *src_mem_attrs)
{
	struct gh_acl_desc *dst_acl_desc;
	struct gh_sgl_desc *dst_sgl_desc;
	struct gh_mem_attr_desc *dst_mem_attrs;
	size_t req_hdr_size, req_acl_size, req_sgl_size;
	u32 n_acl_entries = src_acl_desc ? src_acl_desc->n_acl_entries : 0;
	u32 n_sgl_entries = src_sgl_desc ? src_sgl_desc->n_sgl_entries : 0;

	switch (fn_id) {
	case GH_RM_RPC_MSG_ID_CALL_MEM_LEND:
	case GH_RM_RPC_MSG_ID_CALL_MEM_SHARE:
	case GH_RM_RPC_MSG_ID_CALL_MEM_DONATE:
		req_hdr_size = sizeof(struct gh_mem_share_req_payload_hdr);
		break;
	case GH_RM_RPC_MSG_ID_CALL_MEM_QCOM_LOOKUP_SGL:
		req_hdr_size =
			sizeof(struct gh_mem_qcom_lookup_sgl_req_payload_hdr);
		break;
	case GH_RM_RPC_MSG_ID_CALL_MEM_ACCEPT:
		req_hdr_size =
			sizeof(struct gh_mem_accept_req_payload_hdr);
		break;
	default:
		return;
	}

	req_acl_size = offsetof(struct gh_acl_desc, acl_entries[n_acl_entries]);
	req_sgl_size = offsetof(struct gh_sgl_desc, sgl_entries[n_sgl_entries]);

	dst_acl_desc = req_buf + req_hdr_size;
	dst_sgl_desc = req_buf + req_hdr_size + req_acl_size;
	dst_mem_attrs = req_buf + req_hdr_size + req_acl_size + req_sgl_size;

	gh_rm_populate_acl_desc(dst_acl_desc, src_acl_desc);
	gh_rm_populate_sgl_desc(dst_sgl_desc, src_sgl_desc, reserved_param);
	gh_rm_populate_mem_attr_desc(dst_mem_attrs, src_mem_attrs);
}

static void *gh_rm_alloc_mem_request_buf(u32 fn_id, size_t n_acl_entries,
					 size_t n_sgl_entries,
					 size_t n_mem_attr_entries,
					 size_t *req_payload_size_ptr)
{
	size_t req_acl_size, req_sgl_size, req_mem_attr_size, req_payload_size;
	void *req_buf;


	switch (fn_id) {
	case GH_RM_RPC_MSG_ID_CALL_MEM_LEND:
	case GH_RM_RPC_MSG_ID_CALL_MEM_SHARE:
	case GH_RM_RPC_MSG_ID_CALL_MEM_DONATE:
		req_payload_size = sizeof(struct gh_mem_share_req_payload_hdr);
		break;
	case GH_RM_RPC_MSG_ID_CALL_MEM_QCOM_LOOKUP_SGL:
		req_payload_size =
			sizeof(struct gh_mem_qcom_lookup_sgl_req_payload_hdr);
		break;
	case GH_RM_RPC_MSG_ID_CALL_MEM_ACCEPT:
		req_payload_size =
			sizeof(struct gh_mem_accept_req_payload_hdr);
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	req_acl_size = offsetof(struct gh_acl_desc, acl_entries[n_acl_entries]);
	req_sgl_size = offsetof(struct gh_sgl_desc, sgl_entries[n_sgl_entries]);
	req_mem_attr_size = offsetof(struct gh_mem_attr_desc,
				     attr_entries[n_mem_attr_entries]);
	req_payload_size += req_acl_size + req_sgl_size + req_mem_attr_size;

	req_buf = kzalloc(req_payload_size, GFP_KERNEL);
	if (!req_buf)
		return ERR_PTR(-ENOMEM);

	*req_payload_size_ptr = req_payload_size;
	return req_buf;
}

/**
 * gh_rm_mem_qcom_lookup_sgl: Look up the handle for a memparcel by its sg-list
 * @mem_type: The type of memory associated with the memparcel (i.e. normal or
 *            I/O)
 * @label: The label to assign to the memparcel
 * @acl_desc: Describes the number of ACL entries and VMID and permission pairs
 *            for the memparcel
 * @sgl_desc: Describes the number of SG-List entries and the SG-List for the
 *            memory associated with the memparcel
 * @mem_attr_desc: Describes the number of memory attribute entries and the
 *                 memory attribute and VMID pairs for the memparcel. This
 *                 parameter is currently optional, as this function is meant
 *                 to be used in conjunction with hyp_assign_[phys/table], which
 *                 does not provide memory attributes
 * @handle: Pointer to where the memparcel handle should be stored
 *
 * On success, the function will return 0 and populate the memory referenced by
 * @handle with the memparcel handle. Otherwise, a negative number will be
 * returned.
 */
int gh_rm_mem_qcom_lookup_sgl(u8 mem_type, gh_label_t label,
			      struct gh_acl_desc *acl_desc,
			      struct gh_sgl_desc *sgl_desc,
			      struct gh_mem_attr_desc *mem_attr_desc,
			      gh_memparcel_handle_t *handle)
{
	struct gh_mem_qcom_lookup_sgl_req_payload_hdr *req_payload_hdr;
	struct gh_mem_qcom_lookup_sgl_resp_payload *resp_payload;
	size_t req_payload_size, resp_size;
	void *req_buf;
	unsigned int n_mem_attr_entries = 0;
	u32 fn_id = GH_RM_RPC_MSG_ID_CALL_MEM_QCOM_LOOKUP_SGL;
	int ret = 0, gh_ret;

	if ((mem_type != GH_RM_MEM_TYPE_NORMAL &&
	     mem_type != GH_RM_MEM_TYPE_IO) || !acl_desc ||
	    !acl_desc->n_acl_entries || !sgl_desc ||
	    !sgl_desc->n_sgl_entries || !handle || (mem_attr_desc &&
	    !mem_attr_desc->n_mem_attr_entries))
		return -EINVAL;

	if (mem_attr_desc)
		n_mem_attr_entries = mem_attr_desc->n_mem_attr_entries;

	req_buf = gh_rm_alloc_mem_request_buf(fn_id, acl_desc->n_acl_entries,
					      sgl_desc->n_sgl_entries,
					      n_mem_attr_entries,
					      &req_payload_size);
	if (IS_ERR(req_buf))
		return PTR_ERR(req_buf);

	req_payload_hdr = req_buf;
	req_payload_hdr->mem_type = mem_type;
	req_payload_hdr->label = label;
	gh_rm_populate_mem_request(req_buf, fn_id, acl_desc, sgl_desc, 0,
				   mem_attr_desc);

	resp_payload = gh_rm_call(fn_id, req_buf, req_payload_size, &resp_size,
				  &gh_ret);
	if (gh_ret || IS_ERR(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s failed with err: %d\n",  __func__, ret);
		goto err_rm_call;
	}

	if (resp_size != sizeof(*resp_payload)) {
		ret = -EINVAL;
		pr_err("%s invalid size received %u\n", __func__, resp_size);
		goto err_resp_size;
	}

	*handle = resp_payload->memparcel_handle;

err_resp_size:
	kfree(resp_payload);
err_rm_call:
	kfree(req_buf);
	return ret;
}
EXPORT_SYMBOL(gh_rm_mem_qcom_lookup_sgl);

static int gh_rm_mem_release_helper(u32 fn_id, gh_memparcel_handle_t handle,
				    u8 flags)
{
	struct gh_mem_release_req_payload req_payload = {};
	void *resp;
	size_t resp_size;
	int ret, gh_ret;

	if ((fn_id == GH_RM_RPC_MSG_ID_CALL_MEM_RELEASE) &&
	    (flags & ~GH_RM_MEM_RELEASE_VALID_FLAGS))
		return -EINVAL;
	else if ((fn_id == GH_RM_RPC_MSG_ID_CALL_MEM_RECLAIM) &&
		 (flags & ~GH_RM_MEM_RECLAIM_VALID_FLAGS))
		return -EINVAL;

	req_payload.memparcel_handle = handle;
	req_payload.flags = flags;

	resp = gh_rm_call(fn_id, &req_payload, sizeof(req_payload), &resp_size,
			  &gh_ret);
	if (gh_ret) {
		ret = PTR_ERR(resp);
		pr_err("%s failed with err: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

/**
 * gh_rm_mem_release: Release a handle representing memory. This results in
 *                    the RM unmapping the associated memory from the stage-2
 *                    page-tables of the current VM
 * @handle: The memparcel handle associated with the memory
 * @flags: Bitmask of values to influence the behavior of the RM when it unmaps
 *         the memory.
 *
 * On success, the function will return 0. Otherwise, a negative number will be
 * returned.
 */
int gh_rm_mem_release(gh_memparcel_handle_t handle, u8 flags)
{
	return gh_rm_mem_release_helper(GH_RM_RPC_MSG_ID_CALL_MEM_RELEASE,
					handle, flags);
}
EXPORT_SYMBOL(gh_rm_mem_release);

/**
 * gh_rm_mem_reclaim: Reclaim a memory represented by a handle. This results in
 *                    the RM mapping the associated memory into the stage-2
 *                    page-tables of the owner VM
 * @handle: The memparcel handle associated with the memory
 * @flags: Bitmask of values to influence the behavior of the RM when it unmaps
 *         the memory.
 *
 * On success, the function will return 0. Otherwise, a negative number will be
 * returned.
 */
int gh_rm_mem_reclaim(gh_memparcel_handle_t handle, u8 flags)
{
	return gh_rm_mem_release_helper(GH_RM_RPC_MSG_ID_CALL_MEM_RECLAIM,
					handle, flags);
}
EXPORT_SYMBOL(gh_rm_mem_reclaim);

/**
 * gh_rm_mem_accept: Accept a handle representing memory. This results in
 *                   the RM mapping the associated memory from the stage-2
 *                   page-tables of a VM
 * @handle: The memparcel handle associated with the memory
 * @mem_type: The type of memory associated with the memparcel (i.e. normal or
 *            I/O)
 * @trans_type: The type of memory transfer
 * @flags: Bitmask of values to influence the behavior of the RM when it maps
 *         the memory
 * @label: The label to validate against the label maintained by the RM
 * @acl_desc: Describes the number of ACL entries and VMID and permission
 *            pairs that the resource manager should validate against for AC
 *            regarding the memparcel
 * @sgl_desc: Describes the number of SG-List entries as well as
 *            where the memory should be mapped in the IPA space of the VM
 *            denoted by @map_vmid. If this parameter is left NULL, then the
 *            RM will map the memory at an arbitrary location
 * @mem_attr_desc: Describes the number of memory attribute entries and the
 *                 memory attribute and VMID pairs that the RM should validate
 *                 against regarding the memparcel.
 * @map_vmid: The VMID which RM will map the memory for. VMID 0 corresponds
 *            to mapping the memory for the current VM
 *
 *
 * On success, the function will return a pointer to an sg-list to convey where
 * the memory has been mapped. If the @sgl_desc parameter was not NULL, then the
 * return value will be a pointer to the same SG-List. Otherwise, the return
 * value will be a pointer to a newly allocated SG-List. After the SG-List is
 * no longer needed, the caller must free the table. On a failure, a negative
 * number will be returned.
 */
struct gh_sgl_desc *gh_rm_mem_accept(gh_memparcel_handle_t handle, u8 mem_type,
				     u8 trans_type, u8 flags, gh_label_t label,
				     struct gh_acl_desc *acl_desc,
				     struct gh_sgl_desc *sgl_desc,
				     struct gh_mem_attr_desc *mem_attr_desc,
				     u16 map_vmid)
{

	struct gh_mem_accept_req_payload_hdr *req_payload_hdr;
	struct gh_sgl_desc *ret_sgl;
	struct gh_mem_accept_resp_payload *resp_payload;
	void *req_buf;
	size_t req_payload_size, resp_payload_size;
	u16 req_sgl_entries = 0, req_mem_attr_entries = 0;
	u32 req_acl_entries = 0;
	int gh_ret;
	u32 fn_id = GH_RM_RPC_MSG_ID_CALL_MEM_ACCEPT;

	if ((mem_type != GH_RM_MEM_TYPE_NORMAL &&
	     mem_type != GH_RM_MEM_TYPE_IO) ||
	    (trans_type != GH_RM_TRANS_TYPE_DONATE &&
	     trans_type != GH_RM_TRANS_TYPE_LEND &&
	     trans_type != GH_RM_TRANS_TYPE_SHARE) ||
	    (flags & ~GH_RM_MEM_ACCEPT_VALID_FLAGS))
		return ERR_PTR(-EINVAL);

	if (flags & GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS &&
	    (!acl_desc || !acl_desc->n_acl_entries) &&
	    (!mem_attr_desc || !mem_attr_desc->n_mem_attr_entries))
		return ERR_PTR(-EINVAL);

	if (flags & GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS) {
		if (acl_desc)
			req_acl_entries = acl_desc->n_acl_entries;
		if (mem_attr_desc)
			req_mem_attr_entries =
				mem_attr_desc->n_mem_attr_entries;
	}

	if (sgl_desc)
		req_sgl_entries = sgl_desc->n_sgl_entries;

	req_buf = gh_rm_alloc_mem_request_buf(fn_id, req_acl_entries,
					      req_sgl_entries,
					      req_mem_attr_entries,
					      &req_payload_size);
	if (IS_ERR(req_buf))
		return req_buf;

	req_payload_hdr = req_buf;
	req_payload_hdr->memparcel_handle = handle;
	req_payload_hdr->mem_type = mem_type;
	req_payload_hdr->trans_type = trans_type;
	req_payload_hdr->flags = flags;
	if (flags & GH_RM_MEM_ACCEPT_VALIDATE_LABEL)
		req_payload_hdr->validate_label = label;
	gh_rm_populate_mem_request(req_buf, fn_id, acl_desc, sgl_desc, map_vmid,
				   mem_attr_desc);

	resp_payload = gh_rm_call(fn_id, req_buf, req_payload_size,
				  &resp_payload_size, &gh_ret);
	if (gh_ret || IS_ERR(resp_payload)) {
		ret_sgl = ERR_CAST(resp_payload);
		pr_err("%s failed with error: %d\n", __func__,
		       PTR_ERR(resp_payload));
		goto err_rm_call;
	}


	if (sgl_desc) {
		ret_sgl = sgl_desc;
	} else {
		ret_sgl = kmemdup(resp_payload, offsetof(struct gh_sgl_desc,
				sgl_entries[resp_payload->n_sgl_entries]),
				  GFP_KERNEL);
		if (!ret_sgl)
			ret_sgl = ERR_PTR(-ENOMEM);

		kfree(resp_payload);
	}

err_rm_call:
	kfree(req_buf);
	return ret_sgl;
}
EXPORT_SYMBOL(gh_rm_mem_accept);

static int gh_rm_mem_share_lend_helper(u32 fn_id, u8 mem_type, u8 flags,
				       gh_label_t label,
				       struct gh_acl_desc *acl_desc,
				       struct gh_sgl_desc *sgl_desc,
				       struct gh_mem_attr_desc *mem_attr_desc,
				       gh_memparcel_handle_t *handle)
{
	struct gh_mem_share_req_payload_hdr *req_payload_hdr;
	struct gh_mem_share_resp_payload *resp_payload;
	void *req_buf;
	size_t req_payload_size, resp_payload_size;
	u16 req_sgl_entries, req_acl_entries, req_mem_attr_entries = 0;
	int gh_ret, ret = 0;

	if ((mem_type != GH_RM_MEM_TYPE_NORMAL &&
	     mem_type != GH_RM_MEM_TYPE_IO) ||
	    ((fn_id == GH_RM_RPC_MSG_ID_CALL_MEM_SHARE) &&
	     (flags & ~GH_RM_MEM_SHARE_VALID_FLAGS)) ||
	    ((fn_id == GH_RM_RPC_MSG_ID_CALL_MEM_LEND) &&
	     (flags & ~GH_RM_MEM_LEND_VALID_FLAGS)) ||
	    ((fn_id == GH_RM_RPC_MSG_ID_CALL_MEM_DONATE) &&
	     (flags & ~GH_RM_MEM_DONATE_VALID_FLAGS)) || !acl_desc ||
	    (acl_desc && !acl_desc->n_acl_entries) || !sgl_desc ||
	    (sgl_desc && !sgl_desc->n_sgl_entries) ||
	    (mem_attr_desc && !mem_attr_desc->n_mem_attr_entries) || !handle)
		return -EINVAL;

	req_acl_entries = acl_desc->n_acl_entries;
	req_sgl_entries = sgl_desc->n_sgl_entries;
	if (mem_attr_desc)
		req_mem_attr_entries = mem_attr_desc->n_mem_attr_entries;

	req_buf = gh_rm_alloc_mem_request_buf(fn_id, req_acl_entries,
					      req_sgl_entries,
					      req_mem_attr_entries,
					      &req_payload_size);
	if (IS_ERR(req_buf))
		return PTR_ERR(req_buf);

	req_payload_hdr = req_buf;
	req_payload_hdr->mem_type = mem_type;
	req_payload_hdr->flags = flags;
	req_payload_hdr->label = label;
	gh_rm_populate_mem_request(req_buf, fn_id, acl_desc, sgl_desc, 0,
				   mem_attr_desc);

	resp_payload = gh_rm_call(fn_id, req_buf, req_payload_size,
				  &resp_payload_size, &gh_ret);
	if (gh_ret || IS_ERR(resp_payload)) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s failed with error: %d\n", __func__,
		       PTR_ERR(resp_payload));
		goto err_rm_call;
	}

	if (resp_payload_size != sizeof(*resp_payload)) {
		ret = -EINVAL;
		goto err_resp_size;
	}

	*handle = resp_payload->memparcel_handle;

err_resp_size:
	kfree(resp_payload);
err_rm_call:
	kfree(req_buf);
	return ret;
}

/**
 * gh_rm_mem_share: Share memory with other VM(s) without excluding the owner
 * @mem_type: The type of memory being shared (i.e. normal or I/O)
 * @flags: Bitmask of values to influence the behavior of the RM when it shares
 *         the memory
 * @label: The label to assign to the memparcel that the RM will create
 * @acl_desc: Describes the number of ACL entries and VMID and permission
 *            pairs that the resource manager should consider when sharing the
 *            memory
 * @sgl_desc: Describes the number of SG-List entries as well as
 *            the location of the memory in the IPA space of the owner
 * @mem_attr_desc: Describes the number of memory attribute entries and the
 *                 memory attribute and VMID pairs that the RM should consider
 *                 when sharing the memory
 * @handle: Pointer to where the memparcel handle should be stored

 * On success, the function will return 0 and populate the memory referenced by
 * @handle with the memparcel handle. Otherwise, a negative number will be
 * returned.
 */
int gh_rm_mem_share(u8 mem_type, u8 flags, gh_label_t label,
		    struct gh_acl_desc *acl_desc, struct gh_sgl_desc *sgl_desc,
		    struct gh_mem_attr_desc *mem_attr_desc,
		    gh_memparcel_handle_t *handle)
{
	return gh_rm_mem_share_lend_helper(GH_RM_RPC_MSG_ID_CALL_MEM_SHARE,
					   mem_type, flags, label, acl_desc,
					   sgl_desc, mem_attr_desc, handle);
}
EXPORT_SYMBOL(gh_rm_mem_share);

/**
 * gh_rm_mem_lend: Lend memory to other VM(s)--excluding the owner
 * @mem_type: The type of memory being lent (i.e. normal or I/O)
 * @flags: Bitmask of values to influence the behavior of the RM when it lends
 *         the memory
 * @label: The label to assign to the memparcel that the RM will create
 * @acl_desc: Describes the number of ACL entries and VMID and permission
 *            pairs that the resource manager should consider when lending the
 *            memory
 * @sgl_desc: Describes the number of SG-List entries as well as
 *            the location of the memory in the IPA space of the owner
 * @mem_attr_desc: Describes the number of memory attribute entries and the
 *                 memory attribute and VMID pairs that the RM should consider
 *                 when lending the memory
 * @handle: Pointer to where the memparcel handle should be stored

 * On success, the function will return 0 and populate the memory referenced by
 * @handle with the memparcel handle. Otherwise, a negative number will be
 * returned.
 */
int gh_rm_mem_lend(u8 mem_type, u8 flags, gh_label_t label,
		   struct gh_acl_desc *acl_desc, struct gh_sgl_desc *sgl_desc,
		   struct gh_mem_attr_desc *mem_attr_desc,
		   gh_memparcel_handle_t *handle)
{
	return gh_rm_mem_share_lend_helper(GH_RM_RPC_MSG_ID_CALL_MEM_LEND,
					   mem_type, flags, label, acl_desc,
					   sgl_desc, mem_attr_desc, handle);
}
EXPORT_SYMBOL(gh_rm_mem_lend);

/**
 * gh_rm_mem_donate: Donate memory to a single VM.
 * @mem_type: The type of memory being lent (i.e. normal or I/O)
 * @flags: Bitmask of values to influence the behavior of the RM when it lends
 *         the memory
 * @label: The label to assign to the memparcel that the RM will create
 * @acl_desc: Describes the number of ACL entries and VMID and permission
 *            pairs that the resource manager should consider when lending the
 *            memory
 * @sgl_desc: Describes the number of SG-List entries as well as
 *            the location of the memory in the IPA space of the owner
 * @mem_attr_desc: Describes the number of memory attribute entries and the
 *                 memory attribute and VMID pairs that the RM should consider
 *                 when lending the memory
 * @handle: Pointer to where the memparcel handle should be stored

 * On success, the function will return 0 and populate the memory referenced by
 * @handle with the memparcel handle. Otherwise, a negative number will be
 * returned.
 *
 * Restrictions:
 * Only to or from HLOS.
 * non-HLOS VM must only donate memory which was previously donated to them by
 * HLOS.
 * Physically contiguous.
 * If Lend or Share operates on a sgl entry which contains memory which
 * originated from donate, that sgl entry must be entirely contained within
 * that donate operation.
 */
int gh_rm_mem_donate(u8 mem_type, u8 flags, gh_label_t label,
		   struct gh_acl_desc *acl_desc, struct gh_sgl_desc *sgl_desc,
		   struct gh_mem_attr_desc *mem_attr_desc,
		   gh_memparcel_handle_t *handle)
{
	if (sgl_desc->n_sgl_entries != 1) {
		pr_err("%s: Physically contiguous memory required\n", __func__);
		return -EINVAL;
	}

	if (acl_desc->n_acl_entries != 1) {
		pr_err("%s: Donate requires single destination VM\n", __func__);
		return -EINVAL;
	}

	if (acl_desc->acl_entries[0].perms != (GH_RM_ACL_X | GH_RM_ACL_W | GH_RM_ACL_R)) {
		pr_err("%s: Invalid permission argument\n", __func__);
		return -EINVAL;
	}

	return gh_rm_mem_share_lend_helper(GH_RM_RPC_MSG_ID_CALL_MEM_DONATE,
					   mem_type, flags, label, acl_desc,
					   sgl_desc, mem_attr_desc, handle);
}
EXPORT_SYMBOL(gh_rm_mem_donate);

/**
 * gh_rm_mem_notify: Notify VMs about a change in state with respect to a
 *                   memparcel
 * @handle: The handle of the memparcel for which a notification should be sent
 * out
 * @flags: Flags to determine if the notification is for notifying that memory
 *         has been shared to another VM, or that a VM has released memory
 * @mem_info_tag: A 32-bit value that is attached to the
 *                MEM_SHARED/MEM_RELEASED/MEM_ACCEPTED notifications to aid in
 *                distinguishing different resources from one another.
 * @vmid_desc: A list of VMIDs to notify that memory has been shared with them.
 *             This parameter should only be non-NULL if other VMs are being
 *             notified (i.e. it is invalid to specify this parameter when the
 *             operation is a release notification)
 *
 * On success, the function will return 0. Otherwise, a negative number will be
 * returned.
 */
int gh_rm_mem_notify(gh_memparcel_handle_t handle, u8 flags,
		     gh_label_t mem_info_tag,
		     struct gh_notify_vmid_desc *vmid_desc)
{
	struct gh_mem_notify_req_payload *req_payload_hdr;
	struct gh_notify_vmid_desc *dst_vmid_desc;
	void *req_buf, *resp_payload;
	size_t n_vmid_entries = 0, req_vmid_desc_size = 0, req_payload_size;
	size_t resp_size;
	unsigned int i;
	int ret = 0, gh_ret;

	if ((flags & ~GH_RM_MEM_NOTIFY_VALID_FLAGS) ||
	    ((flags & GH_RM_MEM_NOTIFY_RECIPIENT_SHARED) && (!vmid_desc ||
							     (vmid_desc &&
						!vmid_desc->n_vmid_entries))) ||
	    ((flags & (GH_RM_MEM_NOTIFY_OWNER_RELEASED |
		       GH_RM_MEM_NOTIFY_OWNER_ACCEPTED)) && vmid_desc) ||
	    (hweight8(flags) != 1))
		return -EINVAL;

	if (flags & GH_RM_MEM_NOTIFY_RECIPIENT_SHARED) {
		n_vmid_entries = vmid_desc->n_vmid_entries;
		req_vmid_desc_size = offsetof(struct gh_notify_vmid_desc,
					      vmid_entries[n_vmid_entries]);
	}

	req_payload_size = sizeof(*req_payload_hdr) + req_vmid_desc_size;
	req_buf = kzalloc(req_payload_size, GFP_KERNEL);
	if (!req_buf)
		return -ENOMEM;

	req_payload_hdr = req_buf;
	req_payload_hdr->memparcel_handle = handle;
	req_payload_hdr->flags = flags;
	req_payload_hdr->mem_info_tag = mem_info_tag;

	if (flags & GH_RM_MEM_NOTIFY_RECIPIENT_SHARED) {
		dst_vmid_desc = req_buf + sizeof(*req_payload_hdr);
		dst_vmid_desc->n_vmid_entries = n_vmid_entries;
		for (i = 0; i < n_vmid_entries; i++)
			dst_vmid_desc->vmid_entries[i].vmid =
				vmid_desc->vmid_entries[i].vmid;
	}

	resp_payload = gh_rm_call(GH_RM_RPC_MSG_ID_CALL_MEM_NOTIFY, req_buf,
				  req_payload_size, &resp_size, &gh_ret);
	if (gh_ret) {
		ret = PTR_ERR(resp_payload);
		pr_err("%s failed with err: %d\n", __func__, ret);
	}

	kfree(req_buf);
	return ret;
}
EXPORT_SYMBOL(gh_rm_mem_notify);

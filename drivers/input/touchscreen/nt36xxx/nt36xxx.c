// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (Wed, 10 Jul 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#if !defined(NVT_NT36XXX_SPI) /* NT36XXX I2C */

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

#if defined(CONFIG_DRM)
#include <linux/soc/qcom/panel_event_notifier.h>
#endif

#if defined(CONFIG_DRM_PANEL)
#include <drm/drm_panel.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx.h"
#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if defined(CONFIG_NOVATEK_TRUSTED_TOUCH)
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "linux/gunyah/gh_irq_lend.h"
#include "linux/gunyah/gh_msgq.h"
#include "linux/gunyah/gh_rm_drv.h"
#include <linux/sort.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#endif

#if NVT_TOUCH_ESD_PROTECT
static struct delayed_work nvt_esd_check_work;
static struct workqueue_struct *nvt_esd_check_wq;
static unsigned long irq_timer = 0;
uint8_t esd_check = false;
uint8_t esd_retry = 0;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
extern void nvt_mp_proc_deinit(void);
#endif

struct nvt_ts_data *ts;

#if BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_fwu_wq;
extern void Boot_Update_Firmware(struct work_struct *work);
#endif

#if defined(CONFIG_DRM)
static struct drm_panel *active_panel;
static void nvt_i2c_panel_notifier_callback(enum panel_event_notifier_tag tag,
			struct panel_event_notification *notification, void *client_data);

#elif defined(_MSM_DRM_NOTIFY_H_)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);

#else
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif

static void nvt_irq_enable(bool enable);
static irqreturn_t nvt_ts_work_func(int irq, void *data);
static int32_t nvt_ts_late_probe_sub(struct i2c_client *client,
	const struct i2c_device_id *id);

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

#if WAKEUP_GESTURE
const uint16_t gesture_key_array[] = {
	KEY_POWER,  //GESTURE_WORD_C
	KEY_POWER,  //GESTURE_WORD_W
	KEY_POWER,  //GESTURE_WORD_V
	KEY_POWER,  //GESTURE_DOUBLE_CLICK
	KEY_POWER,  //GESTURE_WORD_Z
	KEY_POWER,  //GESTURE_WORD_M
	KEY_POWER,  //GESTURE_WORD_O
	KEY_POWER,  //GESTURE_WORD_e
	KEY_POWER,  //GESTURE_WORD_S
	KEY_POWER,  //GESTURE_SLIDE_UP
	KEY_POWER,  //GESTURE_SLIDE_DOWN
	KEY_POWER,  //GESTURE_SLIDE_LEFT
	KEY_POWER,  //GESTURE_SLIDE_RIGHT
};
#endif

static uint8_t bTouchIsAwake = 0;

#if defined(CONFIG_DRM)
static void nvt_i2c_register_for_panel_events(struct device_node *dp,
		struct nvt_ts_data *ts)
{
	void *cookie = NULL;

	cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
			PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_TOUCH, active_panel,
			&nvt_i2c_panel_notifier_callback, ts);
	if (!cookie) {
		pr_err("Failed to register for panel events\n");
		return;
	}
	ts->notifier_cookie = cookie;
}
#endif

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
static void nvt_ts_trusted_touch_abort_handler(struct nvt_ts_data *ts,
						int error);
static struct gh_acl_desc *nvt_ts_vm_get_acl(enum gh_vm_names vm_name)
{
	struct gh_acl_desc *acl_desc;
	gh_vmid_t vmid;

	gh_rm_get_vmid(vm_name, &vmid);

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
	if (!acl_desc)
		return ERR_PTR(ENOMEM);

	acl_desc->n_acl_entries = 1;
	acl_desc->acl_entries[0].vmid = vmid;
	acl_desc->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	return acl_desc;
}

static struct gh_sgl_desc *nvt_ts_vm_get_sgl(
				struct trusted_touch_vm_info *vm_info)
{
	struct gh_sgl_desc *sgl_desc;
	int i;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc,
			sgl_entries[vm_info->iomem_list_size]), GFP_KERNEL);
	if (!sgl_desc)
		return ERR_PTR(ENOMEM);

	sgl_desc->n_sgl_entries = vm_info->iomem_list_size;

	for (i = 0; i < vm_info->iomem_list_size; i++) {
		sgl_desc->sgl_entries[i].ipa_base = vm_info->iomem_bases[i];
		sgl_desc->sgl_entries[i].size = vm_info->iomem_sizes[i];
	}

	return sgl_desc;
}

static int nvt_ts_populate_vm_info(struct nvt_ts_data *ts)
{
	int rc = 0, i, gpio;
	struct trusted_touch_vm_info *vm_info;
	struct device_node *np = ts->client->dev.of_node;
	int num_regs, num_gpios, list_size, num_sizes = 0;
	struct resource res;

	vm_info = kzalloc(sizeof(struct trusted_touch_vm_info), GFP_KERNEL);
	if (!vm_info) {
		rc = -ENOMEM;
		goto error;
	}

	ts->vm_info = vm_info;
	vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_PRIMARY;
	vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_PRIMARY;
	vm_info->vm_name = GH_TRUSTED_VM;

	rc = of_property_read_u32(np, "novatek,trusted-touch-spi-irq",
			&vm_info->hw_irq);
	if (rc) {
		pr_err("Failed to read trusted touch SPI irq:%d\n", rc);
		goto vm_error;
	}
	num_regs = of_property_count_u32_elems(np,
			"novatek,trusted-touch-io-bases");
	if (num_regs < 0) {
		pr_err("Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	num_sizes = of_property_count_u32_elems(np,
			"novatek,trusted-touch-io-sizes");
	if (num_sizes < 0) {
		pr_err("Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	if (num_regs != num_sizes) {
		pr_err("IO bases and sizes doe not match\n");
		rc = -EINVAL;
		goto vm_error;
	}

	num_gpios = of_gpio_named_count(np, "novatek,trusted-touch-vm-gpio-list");
	if (num_gpios < 0) {
		dev_warn(&ts->client->dev, "Ignoring invalid trusted gpio list: %d\n", num_gpios);
		num_gpios = 0;
	}

	list_size = num_regs + num_gpios;
	vm_info->iomem_list_size = list_size;

	vm_info->iomem_bases = kcalloc(list_size, sizeof(*vm_info->iomem_bases),
								GFP_KERNEL);
	if (!vm_info->iomem_bases) {
		rc = -ENOMEM;
		goto vm_error;
	}

	vm_info->iomem_sizes = kcalloc(list_size, sizeof(*vm_info->iomem_sizes),
								GFP_KERNEL);
	if (!vm_info->iomem_sizes) {
		rc = -ENOMEM;
		goto vm_error;
	}

	for (i = 0; i < num_gpios; ++i) {
		gpio = of_get_named_gpio(np, "novatek,trusted-touch-vm-gpio-list", i);
		if (gpio < 0 || !gpio_is_valid(gpio)) {
			pr_err("Invalid gpio %d at position %d\n", gpio, i);
			return gpio;
		}

		if (!msm_gpio_get_pin_address(gpio, &res)) {
			pr_err("Failed to retrieve gpio-%d resource\n", gpio);
			return -ENODATA;
		}
		vm_info->iomem_bases[i] = res.start;
		vm_info->iomem_sizes[i] = resource_size(&res);
	}

	rc = of_property_read_u32_array(np, "novatek,trusted-touch-io-bases",
			&vm_info->iomem_bases[i], list_size-i);
	if (rc) {
		pr_err("Failed to read trusted touch io bases:%d\n", rc);
		goto io_bases_error;
	}

	rc = of_property_read_u32_array(np, "novatek,trusted-touch-io-sizes",
			&vm_info->iomem_sizes[i], list_size-i);
	if (rc) {
		pr_err("Failed to read trusted touch io sizes:%d\n", rc);
		goto io_sizes_error;
	}

	return rc;

io_sizes_error:
	kfree(vm_info->iomem_sizes);
io_bases_error:
	kfree(vm_info->iomem_bases);
vm_error:
	kfree(vm_info);
error:
	return rc;
}

static void nvt_ts_destroy_vm_info(struct nvt_ts_data *ts)
{
	kfree(ts->vm_info->iomem_sizes);
	kfree(ts->vm_info->iomem_bases);
	kfree(ts->vm_info);
}

static void nvt_ts_vm_deinit(struct nvt_ts_data *ts)
{
	if (ts->vm_info->mem_cookie)
		gh_mem_notifier_unregister(ts->vm_info->mem_cookie);
	nvt_ts_destroy_vm_info(ts);
}

#ifdef CONFIG_ARCH_QTI_VM
static int nvt_ts_vm_mem_release(struct nvt_ts_data *ts);
static void nvt_ts_trusted_touch_tvm_vm_mode_disable(struct nvt_ts_data *ts);
static void nvt_ts_trusted_touch_abort_tvm(struct nvt_ts_data *ts);
static void nvt_ts_trusted_touch_event_notify(struct nvt_ts_data *ts, int event);
static int32_t nvt_ts_late_probe_tvm(struct i2c_client *client,
	const struct i2c_device_id *id);

static int nvt_ts_trusted_touch_get_tvm_driver_state(struct nvt_ts_data *ts)
{
	int state;

	mutex_lock(&ts->vm_info->tvm_state_mutex);
	state = atomic_read(&ts->vm_info->tvm_state);
	mutex_unlock(&ts->vm_info->tvm_state_mutex);

	return state;
}

static void nvt_ts_trusted_touch_set_tvm_driver_state(struct nvt_ts_data *ts,
						int state)
{
	mutex_lock(&ts->vm_info->tvm_state_mutex);
	atomic_set(&ts->vm_info->tvm_state, state);
	mutex_unlock(&ts->vm_info->tvm_state_mutex);
}

static int nvt_ts_sgl_cmp(const void *a, const void *b)
{
	struct gh_sgl_entry *left = (struct gh_sgl_entry *)a;
	struct gh_sgl_entry *right = (struct gh_sgl_entry *)b;

	return (left->ipa_base - right->ipa_base);
}

static int nvt_ts_vm_compare_sgl_desc(struct gh_sgl_desc *expected,
		struct gh_sgl_desc *received)
{
	int idx;

	if (expected->n_sgl_entries != received->n_sgl_entries)
		return -E2BIG;
	sort(received->sgl_entries, received->n_sgl_entries,
			sizeof(received->sgl_entries[0]), nvt_ts_sgl_cmp, NULL);
	sort(expected->sgl_entries, expected->n_sgl_entries,
			sizeof(expected->sgl_entries[0]), nvt_ts_sgl_cmp, NULL);

	for (idx = 0; idx < expected->n_sgl_entries; idx++) {
		struct gh_sgl_entry *left = &expected->sgl_entries[idx];
		struct gh_sgl_entry *right = &received->sgl_entries[idx];

		if ((left->ipa_base != right->ipa_base) ||
				(left->size != right->size)) {
			pr_err("sgl mismatch: base l:%d r:%d size l:%d r:%d\n",
					left->ipa_base, right->ipa_base,
					left->size, right->size);
			return -EINVAL;
		}
	}
	return 0;
}

static int nvt_ts_vm_handle_vm_hardware(struct nvt_ts_data *ts)
{
	int rc = 0;

	if (atomic_read(&ts->delayed_vm_probe_pending)) {
		rc = nvt_ts_late_probe_tvm(ts->client, ts->id);
		if (rc) {
			pr_err("Delayed probe failure on VM!\n");
			return rc;
		}
		atomic_set(&ts->delayed_vm_probe_pending, 0);
		return rc;
	}

	nvt_irq_enable(true);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_INTERRUPT_ENABLED);
	return rc;
}

static void nvt_ts_trusted_touch_tvm_vm_mode_enable(struct nvt_ts_data *ts)
{

	struct gh_sgl_desc *sgl_desc, *expected_sgl_desc;
	struct gh_acl_desc *acl_desc;
	struct irq_data *irq_data;
	int rc = 0;
	int irq = 0;

	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) !=
					TVM_ALL_RESOURCES_LENT_NOTIFIED) {
		pr_err("All lend notifications not received for touch\n");
		nvt_ts_trusted_touch_event_notify(ts,
				TRUSTED_TOUCH_EVENT_NOTIFICATIONS_PENDING);
		return;
	}

	acl_desc = nvt_ts_vm_get_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		pr_err("failed to populated acl data:rc=%d\n",
				PTR_ERR(acl_desc));
		goto accept_fail;
	}

	sgl_desc = gh_rm_mem_accept(ts->vm_info->vm_mem_handle,
			GH_RM_MEM_TYPE_IO,
			GH_RM_TRANS_TYPE_LEND,
			GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
			GH_RM_MEM_ACCEPT_VALIDATE_LABEL |
			GH_RM_MEM_ACCEPT_DONE,  TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, NULL, NULL, 0);
	if (IS_ERR_OR_NULL(sgl_desc)) {
		pr_err("failed to do mem accept :rc=%d\n",
				PTR_ERR(sgl_desc));
		goto acl_fail;
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_IOMEM_ACCEPTED);

	/* Initiate i2c session on tvm */
	rc = pm_runtime_get_sync(ts->client->adapter->dev.parent);
	if (rc < 0) {
		pr_err("failed to get sync rc:%d\n", rc);
		goto acl_fail;
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_I2C_SESSION_ACQUIRED);

	expected_sgl_desc = nvt_ts_vm_get_sgl(ts->vm_info);
	if (nvt_ts_vm_compare_sgl_desc(expected_sgl_desc, sgl_desc)) {
		pr_err("IO sg list does not match\n");
		goto sgl_cmp_fail;
	}

	kfree(expected_sgl_desc);
	kfree(acl_desc);

	irq = gh_irq_accept(ts->vm_info->irq_label, -1, IRQ_TYPE_EDGE_RISING);
	if (irq < 0) {
		pr_err("failed to accept irq\n");
		goto accept_fail;
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TVM_IRQ_ACCEPTED);


	irq_data = irq_get_irq_data(irq);
	if (!irq_data) {
		pr_err("Invalid irq data for trusted touch\n");
		goto accept_fail;
	}
	if (!irq_data->hwirq) {
		pr_err("Invalid irq in irq data\n");
		goto accept_fail;
	}
	if (irq_data->hwirq != ts->vm_info->hw_irq) {
		pr_err("Invalid irq lent\n");
		goto accept_fail;
	}

	pr_info("touch irq:returned from accept:%d\n", irq);
	ts->client->irq = irq;

	rc = nvt_ts_vm_handle_vm_hardware(ts);
	if (rc) {
		pr_err("Delayed probe failure on VM!\n");
		goto accept_fail;
	}
	atomic_set(&ts->trusted_touch_enabled, 1);
	pr_info("trusted touch enabled\n");

	return;
sgl_cmp_fail:
	kfree(expected_sgl_desc);
acl_fail:
	kfree(acl_desc);
accept_fail:
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_ACCEPT_FAILURE);
}

static void nvt_ts_vm_irq_on_lend_callback(void *data,
					unsigned long notif_type,
					enum gh_irq_label label)
{
	struct nvt_ts_data *ts = data;

	pr_debug("received touch irq lend request for label:%d\n", label);
	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) ==
		TVM_IOMEM_LENT_NOTIFIED) {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
		TVM_ALL_RESOURCES_LENT_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
			TVM_IRQ_LENT_NOTIFIED);
	}
}

static void nvt_ts_vm_mem_on_lend_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_shared_payload *payload;
	struct trusted_touch_vm_info *vm_info;
	struct nvt_ts_data *ts;

	ts = (struct nvt_ts_data *)entry_data;
	vm_info = ts->vm_info;

	if (!vm_info) {
		pr_err("Invalid vm_info\n");
		return;
	}

	if (notif_type != GH_RM_NOTIF_MEM_SHARED ||
			tag != vm_info->mem_tag) {
		pr_err("Invalid command passed from rm\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		pr_err("Invalid entry data passed from rm\n");
		return;
	}

	payload = (struct gh_rm_notif_mem_shared_payload  *)notif_msg;
	if (payload->trans_type != GH_RM_TRANS_TYPE_LEND ||
			payload->label != TRUSTED_TOUCH_MEM_LABEL) {
		pr_err("Invalid label or transaction type\n");
		return;
	}

	vm_info->vm_mem_handle = payload->mem_handle;
	pr_debug("received touch mem lend request with handle:%d\n",
		vm_info->vm_mem_handle);
	if (nvt_ts_trusted_touch_get_tvm_driver_state(ts) ==
		TVM_IRQ_LENT_NOTIFIED) {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
			TVM_ALL_RESOURCES_LENT_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
		TVM_IOMEM_LENT_NOTIFIED);
	}
}

static int nvt_ts_vm_mem_release(struct nvt_ts_data *ts)
{
	int rc = 0;

	if (!ts->vm_info->vm_mem_handle) {
		pr_err("Invalid memory handle\n");
		return -EINVAL;
	}

	rc = gh_rm_mem_release(ts->vm_info->vm_mem_handle, 0);
	if (rc)
		pr_err("touch VM mem release failed: rc=%d\n", rc);

	rc = gh_rm_mem_notify(ts->vm_info->vm_mem_handle,
				GH_RM_MEM_NOTIFY_OWNER_RELEASED,
				ts->vm_info->mem_tag, NULL);
	if (rc)
		pr_err("Failed to notify mem release to PVM: rc=%d\n");
	pr_debug("touch vm mem release succeded\n");

	ts->vm_info->vm_mem_handle = 0;
	return rc;
}

static void nvt_ts_trusted_touch_tvm_vm_mode_disable(struct nvt_ts_data *ts)
{
	int rc = 0;

	if (atomic_read(&ts->trusted_touch_abort_status)) {
		nvt_ts_trusted_touch_abort_tvm(ts);
		return;
	}

	nvt_irq_enable(false);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts,
				TVM_INTERRUPT_DISABLED);
	rc = gh_irq_release(ts->vm_info->irq_label);
	if (rc) {
		pr_err("Failed to release irq rc:%d\n", rc);
		goto error;
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_IRQ_RELEASED);
	}
	rc = gh_irq_release_notify(ts->vm_info->irq_label);
	if (rc)
		pr_err("Failed to notify release irq rc:%d\n", rc);

	pr_debug("vm irq release succeded\n");

	pm_runtime_put_sync(ts->client->adapter->dev.parent);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_I2C_SESSION_RELEASED);

	rc = nvt_ts_vm_mem_release(ts);
	if (rc) {
		pr_err("Failed to release mem rc:%d\n", rc);
		goto error;
	} else {
		nvt_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_IOMEM_RELEASED);
	}
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
	atomic_set(&ts->trusted_touch_enabled, 0);
	pr_info("trusted touch disabled\n");
	return;
error:
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_RELEASE_FAILURE);
}

static int nvt_ts_handle_trusted_touch_tvm(struct nvt_ts_data *ts, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if ((atomic_read(&ts->trusted_touch_enabled) == 0) &&
			(atomic_read(&ts->trusted_touch_abort_status) == 0)) {
			pr_err("Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			nvt_ts_trusted_touch_tvm_vm_mode_disable(ts);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&ts->trusted_touch_enabled)) {
			pr_err("Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			nvt_ts_trusted_touch_tvm_vm_mode_enable(ts);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	default:
		pr_err("unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}

	return err;
}

static void nvt_ts_trusted_touch_abort_tvm(struct nvt_ts_data *ts)
{
	int rc = 0;
	int tvm_state = nvt_ts_trusted_touch_get_tvm_driver_state(ts);

	if (tvm_state >= TRUSTED_TOUCH_TVM_STATE_MAX) {
		pr_err("invalid tvm driver state: %d\n", tvm_state);
		return;
	}

	switch (tvm_state) {
	case TVM_INTERRUPT_ENABLED:
		nvt_irq_enable(false);
	case TVM_IRQ_ACCEPTED:
	case TVM_INTERRUPT_DISABLED:
		rc = gh_irq_release(ts->vm_info->irq_label);
		if (rc)
			pr_err("Failed to release irq rc:%d\n", rc);
		rc = gh_irq_release_notify(ts->vm_info->irq_label);
		if (rc)
			pr_err("Failed to notify irq release rc:%d\n", rc);
	case TVM_I2C_SESSION_ACQUIRED:
	case TVM_IOMEM_ACCEPTED:
	case TVM_IRQ_RELEASED:
		pm_runtime_put_sync(ts->client->adapter->dev.parent);
	case TVM_I2C_SESSION_RELEASED:
		rc = nvt_ts_vm_mem_release(ts);
		if (rc)
			pr_err("Failed to release mem rc:%d\n", rc);
	case TVM_IOMEM_RELEASED:
	case TVM_ALL_RESOURCES_LENT_NOTIFIED:
	case TRUSTED_TOUCH_TVM_INIT:
	case TVM_IRQ_LENT_NOTIFIED:
	case TVM_IOMEM_LENT_NOTIFIED:
		atomic_set(&ts->trusted_touch_enabled, 0);
	}

	atomic_set(&ts->trusted_touch_abort_status, 0);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
}

#else

static void nvt_ts_bus_put(struct nvt_ts_data *ts);

static int nvt_ts_trusted_touch_get_pvm_driver_state(struct nvt_ts_data *ts)
{
	int state;

	mutex_lock(&ts->vm_info->pvm_state_mutex);
	state = atomic_read(&ts->vm_info->pvm_state);
	mutex_unlock(&ts->vm_info->pvm_state_mutex);

	return state;
}

static void nvt_ts_trusted_touch_set_pvm_driver_state(struct nvt_ts_data *ts,
							int state)
{
	mutex_lock(&ts->vm_info->pvm_state_mutex);
	atomic_set(&ts->vm_info->pvm_state, state);
	mutex_unlock(&ts->vm_info->pvm_state_mutex);
}

static void nvt_ts_trusted_touch_abort_pvm(struct nvt_ts_data *ts)
{
	int rc = 0;
	int pvm_state = nvt_ts_trusted_touch_get_pvm_driver_state(ts);

	if (pvm_state >= TRUSTED_TOUCH_PVM_STATE_MAX) {
		pr_err("Invalid driver state: %d\n", pvm_state);
		return;
	}

	switch (pvm_state) {
	case PVM_IRQ_RELEASE_NOTIFIED:
	case PVM_ALL_RESOURCES_RELEASE_NOTIFIED:
	case PVM_IRQ_LENT:
	case PVM_IRQ_LENT_NOTIFIED:
		rc = gh_irq_reclaim(ts->vm_info->irq_label);
		if (rc)
			pr_err("failed to reclaim irq on pvm rc:%d\n", rc);
	case PVM_IRQ_RECLAIMED:
	case PVM_IOMEM_LENT:
	case PVM_IOMEM_LENT_NOTIFIED:
	case PVM_IOMEM_RELEASE_NOTIFIED:
		rc = gh_rm_mem_reclaim(ts->vm_info->vm_mem_handle, 0);
		if (rc)
			pr_err("failed to reclaim iomem on pvm rc:%d\n", rc);
		ts->vm_info->vm_mem_handle = 0;
	case PVM_IOMEM_RECLAIMED:
	case PVM_INTERRUPT_DISABLED:
		nvt_irq_enable(true);
	case PVM_I2C_RESOURCE_ACQUIRED:
	case PVM_INTERRUPT_ENABLED:
		nvt_ts_bus_put(ts);
	case TRUSTED_TOUCH_PVM_INIT:
	case PVM_I2C_RESOURCE_RELEASED:
		atomic_set(&ts->trusted_touch_enabled, 0);
		atomic_set(&ts->trusted_touch_underway, 0);
	}

	atomic_set(&ts->trusted_touch_abort_status, 0);

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, TRUSTED_TOUCH_PVM_INIT);
}

static int nvt_ts_clk_prepare_enable(struct nvt_ts_data *ts)
{
	int ret;

	ret = clk_prepare_enable(ts->iface_clk);
	if (ret) {
		pr_err("error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(ts->core_clk);
	if (ret) {
		clk_disable_unprepare(ts->iface_clk);
		pr_err("error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void nvt_ts_clk_disable_unprepare(struct nvt_ts_data *ts)
{
	clk_disable_unprepare(ts->core_clk);
	clk_disable_unprepare(ts->iface_clk);
}

static int nvt_ts_bus_get(struct nvt_ts_data *ts)
{
	int rc = 0;

	if (atomic_read(&ts->suspend_resume_underway))
		wait_for_completion_interruptible(&ts->touch_suspend_resume);

	reinit_completion(&ts->trusted_touch_powerdown);
	mutex_lock(&ts->nvt_clk_io_ctrl_mutex);
	rc = pm_runtime_get_sync(ts->client->adapter->dev.parent);
	if (rc >= 0 &&  ts->core_clk != NULL &&
				ts->iface_clk != NULL) {
		rc = nvt_ts_clk_prepare_enable(ts);
		if (rc)
			pm_runtime_put_sync(
				ts->client->adapter->dev.parent);
	}
	mutex_unlock(&ts->nvt_clk_io_ctrl_mutex);
	return rc;
}

static void nvt_ts_bus_put(struct nvt_ts_data *ts)
{
	mutex_lock(&ts->nvt_clk_io_ctrl_mutex);
	if (ts->core_clk != NULL && ts->iface_clk != NULL)
		nvt_ts_clk_disable_unprepare(ts);
	pm_runtime_put_sync(ts->client->adapter->dev.parent);
	mutex_unlock(&ts->nvt_clk_io_ctrl_mutex);
	complete(&ts->trusted_touch_powerdown);
}

static struct gh_notify_vmid_desc *nvt_ts_vm_get_vmid(gh_vmid_t vmid)
{
	struct gh_notify_vmid_desc *vmid_desc;

	vmid_desc = kzalloc(offsetof(struct gh_notify_vmid_desc,
				vmid_entries[1]), GFP_KERNEL);
	if (!vmid_desc)
		return ERR_PTR(ENOMEM);

	vmid_desc->n_vmid_entries = 1;
	vmid_desc->vmid_entries[0].vmid = vmid;
	return vmid_desc;
}

static void nvt_trusted_touch_pvm_vm_mode_disable(struct nvt_ts_data *ts)
{
	int rc = 0;

	if (atomic_read(&ts->trusted_touch_abort_status)) {
		nvt_ts_trusted_touch_abort_pvm(ts);
		return;
	}

	if (nvt_ts_trusted_touch_get_pvm_driver_state(ts) !=
					PVM_ALL_RESOURCES_RELEASE_NOTIFIED)
		pr_err("all release notifications are not received yet\n");

	rc = gh_irq_reclaim(ts->vm_info->irq_label);
	if (rc) {
		pr_err("failed to reclaim irq on pvm rc:%d\n", rc);
		goto error;
	}
	nvt_ts_trusted_touch_set_pvm_driver_state(ts,
				PVM_IRQ_RECLAIMED);

	rc = gh_rm_mem_reclaim(ts->vm_info->vm_mem_handle, 0);
	if (rc) {
		pr_err("Trusted touch VM mem reclaim failed rc:%d\n", rc);
		goto error;
	}
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_RECLAIMED);
	ts->vm_info->vm_mem_handle = 0;

	nvt_irq_enable(true);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_INTERRUPT_ENABLED);
	nvt_ts_bus_put(ts);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts,
						PVM_I2C_RESOURCE_RELEASED);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts,
						TRUSTED_TOUCH_PVM_INIT);
	atomic_set(&ts->trusted_touch_enabled, 0);
	atomic_set(&ts->trusted_touch_underway, 0);
	pr_info("trusted touch disabled\n");
	return;
error:
	nvt_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_RECLAIM_FAILURE);
}

static void nvt_ts_vm_irq_on_release_callback(void *data,
					unsigned long notif_type,
					enum gh_irq_label label)
{
	struct nvt_ts_data *ts = data;

	if (notif_type != GH_RM_NOTIF_VM_IRQ_RELEASED) {
		pr_err("invalid notification type\n");
		return;
	}

	if (nvt_ts_trusted_touch_get_pvm_driver_state(ts) ==
		PVM_IOMEM_RELEASE_NOTIFIED) {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_IRQ_RELEASE_NOTIFIED);
	}
}

static void nvt_ts_vm_mem_on_release_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_released_payload *release_payload;
	struct trusted_touch_vm_info *vm_info;
	struct nvt_ts_data *ts;

	ts = (struct nvt_ts_data *)entry_data;
	vm_info = ts->vm_info;

	if (!vm_info) {
		pr_err(" Invalid vm_info\n");
		return;
	}

	if (notif_type != GH_RM_NOTIF_MEM_RELEASED) {
		pr_err(" Invalid notification type\n");
		return;
	}

	if (tag != vm_info->mem_tag) {
		pr_err(" Invalid tag\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		pr_err(" Invalid data or notification message\n");
		return;
	}

	release_payload = (struct gh_rm_notif_mem_released_payload  *)notif_msg;
	if (release_payload->mem_handle != vm_info->vm_mem_handle) {
		pr_err("Invalid mem handle detected\n");
		return;
	}

	if (nvt_ts_trusted_touch_get_pvm_driver_state(ts) ==
				PVM_IRQ_RELEASE_NOTIFIED) {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
	} else {
		nvt_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_IOMEM_RELEASE_NOTIFIED);
	}
}

static int nvt_ts_vm_mem_lend(struct nvt_ts_data *ts)
{
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	struct gh_notify_vmid_desc *vmid_desc;
	gh_memparcel_handle_t mem_handle;
	gh_vmid_t trusted_vmid;
	int rc = 0;

	acl_desc = nvt_ts_vm_get_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		pr_err("Failed to get acl of IO memories for Trusted touch\n");
		PTR_ERR(acl_desc);
		return -EINVAL;
	}

	sgl_desc = nvt_ts_vm_get_sgl(ts->vm_info);
	if (IS_ERR(sgl_desc)) {
		pr_err("Failed to get sgl of IO memories for Trusted touch\n");
		PTR_ERR(sgl_desc);
		rc = -EINVAL;
		goto sgl_error;
	}

	rc = gh_rm_mem_lend(GH_RM_MEM_TYPE_IO, 0, TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, sgl_desc, NULL, &mem_handle);
	if (rc) {
		pr_err("Failed to lend IO memories for Trusted touch rc:%d\n",
							rc);
		goto error;
	}

	pr_info("vm mem lend succeded\n");

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_LENT);

	gh_rm_get_vmid(GH_TRUSTED_VM, &trusted_vmid);

	vmid_desc = nvt_ts_vm_get_vmid(trusted_vmid);

	rc = gh_rm_mem_notify(mem_handle, GH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
			ts->vm_info->mem_tag, vmid_desc);
	if (rc) {
		pr_err("Failed to notify mem lend to hypervisor rc:%d\n", rc);
		goto vmid_error;
	}

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_LENT_NOTIFIED);

	ts->vm_info->vm_mem_handle = mem_handle;
vmid_error:
	kfree(vmid_desc);
error:
	kfree(sgl_desc);
sgl_error:
	kfree(acl_desc);

	return rc;
}

static int nvt_ts_trusted_touch_pvm_vm_mode_enable(struct nvt_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info = ts->vm_info;

	if (atomic_read(&ts->pvm_interrupt_underway))
		wait_for_completion_interruptible(&ts->trusted_touch_interrupt);

	if (!bTouchIsAwake) {
		pr_err("Invalid power state for operation\n");
		rc = -EPERM;
		return rc;
	}

	atomic_set(&ts->trusted_touch_underway, 1);

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq)
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
#endif
	/* i2c session start and resource acquire */
	if (nvt_ts_bus_get(ts) < 0) {
		pr_err("nvt_ts_bus_get failed\n");
		rc = -EIO;
		atomic_set(&ts->trusted_touch_underway, 0);
		return rc;
	}

	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_I2C_RESOURCE_ACQUIRED);
	nvt_irq_enable(false);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_INTERRUPT_DISABLED);

	rc = nvt_ts_vm_mem_lend(ts);
	if (rc) {
		pr_err("Failed to lend memory\n");
		goto error;
	}

	rc = gh_irq_lend_v2(vm_info->irq_label, vm_info->vm_name,
		ts->client->irq, &nvt_ts_vm_irq_on_release_callback, ts);
	if (rc) {
		pr_err("Failed to lend irq\n");
		goto error;
	}

	pr_info("vm irq lend succeded for irq:%d\n", ts->client->irq);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_LENT);

	rc = gh_irq_lend_notify(vm_info->irq_label);
	if (rc) {
		pr_err("Failed to notify irq\n");
		goto error;
	}
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_LENT_NOTIFIED);

	atomic_set(&ts->trusted_touch_enabled, 1);
	pr_info("trusted touch enabled\n");
	return rc;
error:
	nvt_ts_trusted_touch_abort_handler(ts, TRUSTED_TOUCH_EVENT_LEND_FAILURE);
	return rc;
}

static int nvt_ts_handle_trusted_touch_pvm(struct nvt_ts_data *ts, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if (atomic_read(&ts->trusted_touch_enabled) == 0 &&
			(atomic_read(&ts->trusted_touch_abort_status) == 0)) {
			pr_err("Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			nvt_trusted_touch_pvm_vm_mode_disable(ts);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;
	case 1:
		if (atomic_read(&ts->trusted_touch_enabled)) {
			pr_err("Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			err = nvt_ts_trusted_touch_pvm_vm_mode_enable(ts);
		} else {
			pr_err("Unsupported trusted touch mode\n");
		}
		break;

	default:
		pr_err("unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

#endif

static void nvt_ts_trusted_touch_event_notify(struct nvt_ts_data *ts, int event)
{
	atomic_set(&ts->trusted_touch_event, event);
	sysfs_notify(&ts->client->dev.kobj, NULL, "trusted_touch_event");
}

static void nvt_ts_trusted_touch_abort_handler(struct nvt_ts_data *ts, int error)
{
	atomic_set(&ts->trusted_touch_abort_status, error);
	pr_err("TUI session aborted with failure:%d\n", error);
	nvt_ts_trusted_touch_event_notify(ts, error);
}

static int nvt_ts_vm_init(struct nvt_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	void *mem_cookie;

	rc = nvt_ts_populate_vm_info(ts);
	if (rc) {
		pr_err("Cannot setup vm pipeline\n");
		rc = -EINVAL;
		goto fail;
	}

	vm_info = ts->vm_info;
#ifdef CONFIG_ARCH_QTI_VM
	mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
			nvt_ts_vm_mem_on_lend_handler, ts);
	if (!mem_cookie) {
		pr_err("Failed to register on lend mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	rc = gh_irq_wait_for_lend_v2(vm_info->irq_label, GH_PRIMARY_VM,
			&nvt_ts_vm_irq_on_lend_callback, ts);
	mutex_init(&ts->vm_info->tvm_state_mutex);
	nvt_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
#else
	mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
			nvt_ts_vm_mem_on_release_handler, ts);
	if (!mem_cookie) {
		pr_err("Failed to register on release mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	mutex_init(&ts->vm_info->pvm_state_mutex);
	nvt_ts_trusted_touch_set_pvm_driver_state(ts, TRUSTED_TOUCH_PVM_INIT);
#endif
	return rc;
init_fail:
	nvt_ts_vm_deinit(ts);
fail:
	return rc;
}

static void nvt_ts_dt_parse_trusted_touch_info(struct nvt_ts_data *ts)
{
	struct device_node *np = ts->client->dev.of_node;
	int rc = 0;
	const char *selection;
	const char *environment;

#ifdef CONFIG_ARCH_QTI_VM
	ts->touch_environment = "tvm";
#else
	ts->touch_environment = "pvm";
#endif

	rc = of_property_read_string(np, "novatek,trusted-touch-mode",
								&selection);
	if (rc) {
		dev_warn(&ts->client->dev,
			"%s: No trusted touch mode selection made\n", __func__);
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		return;
	}

	if (!strcmp(selection, "vm_mode")) {
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_VM_MODE);
		pr_err("Selected trusted touch mode to VM mode\n");
	} else {
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		pr_err("Invalid trusted_touch mode\n");
	}

	rc = of_property_read_string(np, "novatek,touch-environment",
						&environment);
	if (rc) {
		dev_warn(&ts->client->dev,
			"%s: No trusted touch mode environment\n", __func__);
	}
	ts->touch_environment = environment;
	pr_info("Trusted touch environment:%s\n",
			ts->touch_environment);
}

static void nvt_ts_trusted_touch_init(struct nvt_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_initialized, 0);
	nvt_ts_dt_parse_trusted_touch_info(ts);

	if (atomic_read(&ts->trusted_touch_mode) ==
						TRUSTED_TOUCH_MODE_NONE)
		return;

	init_completion(&ts->trusted_touch_powerdown);
	init_completion(&ts->touch_suspend_resume);
	init_completion(&ts->trusted_touch_interrupt);

	/* Get clocks */
	ts->core_clk = devm_clk_get(ts->client->dev.parent,
						"m-ahb");
	if (IS_ERR(ts->core_clk)) {
		ts->core_clk = NULL;
		dev_warn(&ts->client->dev,
				"%s: core_clk is not defined\n", __func__);
	}

	ts->iface_clk = devm_clk_get(ts->client->dev.parent,
						"se-clk");
	if (IS_ERR(ts->iface_clk)) {
		ts->iface_clk = NULL;
		dev_warn(&ts->client->dev,
			"%s: iface_clk is not defined\n", __func__);
	}

	if (atomic_read(&ts->trusted_touch_mode) ==
						TRUSTED_TOUCH_VM_MODE) {
		rc = nvt_ts_vm_init(ts);
		if (rc)
			pr_err("Failed to init VM\n");
	}
	atomic_set(&ts->trusted_touch_initialized, 1);
}

#endif

/*******************************************************
 * Description:
 *     Novatek touchscreen irq enable/disable function.
 *
 * return:
 *     n.a.
 *******************************************************/
static void nvt_irq_enable(bool enable)
{
	struct irq_desc *desc;

	if (enable) {
		if (!ts->irq_enabled) {
			enable_irq(ts->client->irq);
			ts->irq_enabled = true;
		}
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			ts->irq_enabled = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	NVT_LOG("enable=%d, desc->depth=%d\n", enable, desc->depth);
}

/*******************************************************
 * Description:
 *     Novatek touchscreen i2c read function.
 *
 * return:
 *     Executive outcomes. 2---succeed. -5---I/O error
 *******************************************************/
int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf,
		uint16_t len)
{
	struct i2c_msg msgs[2];
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = address;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = address;
	msgs[1].len   = len - 1;
	msgs[1].buf   = ts->xbuf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	memcpy(buf + 1, ts->xbuf, len - 1);

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen i2c write function.
 *
 * return:
 *     Executive outcomes. 1---succeed. -5---I/O error
 *******************************************************/
int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf,
		uint16_t len)
{
	struct i2c_msg msg;
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	msg.flags = !I2C_M_RD;
	msg.addr  = address;
	msg.len   = len;
	memcpy(ts->xbuf, buf, len);
	msg.buf   = ts->xbuf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen set index/page/addr address.
 *
 * return:
 *     Executive outcomes. 0---succeed. -5---access fail.
 ********************************************************/
int32_t nvt_set_page(uint16_t i2c_addr, uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 16) & 0xFF;
	buf[2] = (addr >> 8) & 0xFF;

	return CTP_I2C_WRITE(ts->client, i2c_addr, buf, 3);
}

/*******************************************************
 * Description:
 *     Novatek touchscreen reset MCU then into idle mode
 * function.
 *
 * return:
 *     n.a.
 *******************************************************/
void nvt_sw_reset_idle(void)
{
	uint8_t buf[4]={0};

	//---write i2c cmds to reset idle---
	buf[0]=0x00;
	buf[1]=0xA5;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

	msleep(15);
}

/*******************************************************
 * Description:
 *     Novatek touchscreen reset MCU (boot) function.
 *
 * return:
 *     n.a.
 *******************************************************/
void nvt_bootloader_reset(void)
{
	uint8_t buf[8] = {0};

	NVT_LOG("start\n");

	//---write i2c cmds to reset---
	buf[0] = 0x00;
	buf[1] = 0x69;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

	// need 35ms delay after bootloader reset
	msleep(35);

	NVT_LOG("end\n");
}

/*******************************************************
 * Description:
 *     Novatek touchscreen clear FW status function.
 *
 * return:
 *     Executive outcomes. 0---succeed. -1---fail.
 *******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
 * Description:
 *     Novatek touchscreen check FW status function.
 *
 * return:
 *     Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
 * Description:
 *     Novatek touchscreen check FW reset state function.
 *
 * return:
 *     Executive outcomes. 0---succeed. -1---failed.
 ******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	while (1) {
		usleep_range(10000, 10000);

		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > 100)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}
	}

	return ret;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen get novatek project id information
 * function.
 *
 * return:
 *     Executive outcomes. 0---success. -1---fail.
 *******************************************************/
int32_t nvt_read_pid(void)
{
	uint8_t buf[3] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];

	NVT_LOG("PID=%04X\n", ts->nvt_pid);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 17);
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->x_num = 18;
		ts->y_num = 32;
		ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ts->max_button_num = TOUCH_KEY_NUM;

		if(retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			NVT_ERR("Set default fw_ver=%d, x_num=%d, y_num=%d, "
					"abs_x_max=%d, abs_y_max=%d, max_button_num=%d!\n",
					ts->fw_ver, ts->x_num, ts->y_num,
					ts->abs_x_max, ts->abs_y_max, ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	//---Get Novatek PID---
	nvt_read_pid();

	return ret;
}

/*******************************************************
 * Create Device Node (Proc Entry)
 *******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTflash"

/*******************************************************
 * Description:
 *     Novatek touchscreen /proc/NVTflash read function.
 *
 * return:
 *     Executive outcomes. 2---succeed. -5,-14---failed.
 *******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff,
	size_t count, loff_t *offp)
{
	uint8_t str[68] = {0};
	int32_t ret = -1;
	int32_t retries = 0;
	int8_t i2c_wr = 0;

	if (count > sizeof(str)) {
		NVT_ERR("error count=%zu\n", count);
		return -EFAULT;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		return -EFAULT;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	i2c_wr = str[0] >> 7;

	if (i2c_wr == 0) {	//I2C write
		while (retries < 20) {
			ret = CTP_I2C_WRITE(ts->client, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 1)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else if (i2c_wr == 1) {	//I2C read
		while (retries < 20) {
			ret = CTP_I2C_READ(ts->client, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 2)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		// copy buff to user if i2c transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count))
				return -EFAULT;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		return -EFAULT;
	}
}

/*******************************************************
 * Description:
 *     Novatek touchscreen /proc/NVTflash open function.
 *
 * return:
 *     Executive outcomes. 0---succeed. -12---failed.
 *******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen /proc/NVTflash close function.
 *
 * return:
 *     Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	kfree(dev);

	return 0;
}

static const struct proc_ops nvt_flash_fops = {
	.proc_open = nvt_flash_open,
	.proc_release = nvt_flash_close,
	.proc_read = nvt_flash_read,
};

/*******************************************************
 * Description:
 *     Novatek touchscreen /proc/NVTflash initial function.
 *
 * return:
 *     Executive outcomes. 0---succeed. -12---failed.
 *******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("Succeeded!\n");
	}

	NVT_LOG("==========================================================\n");
	NVT_LOG("Create /proc/%s\n", DEVICE_NAME);
	NVT_LOG("==========================================================\n");

	return 0;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen /proc/NVTflash deinitial function.
 *
 * return:
 *     n.a.
 *******************************************************/
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", DEVICE_NAME);
	}
}
#endif

#if WAKEUP_GESTURE
#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24
/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE         1

/*******************************************************
 * Description:
 *     Novatek touchscreen wake up gesture key report function.
 *
 * return:
 *     n.a.
 *******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];

	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
		gesture_id = func_id;
	} else if (gesture_id > DATA_PROTOCOL) {
		NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n", gesture_id, func_type, func_id);
		return;
	}

	NVT_LOG("gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
		case GESTURE_WORD_C:
			NVT_LOG("Gesture : Word-C.\n");
			keycode = gesture_key_array[0];
			break;
		case GESTURE_WORD_W:
			NVT_LOG("Gesture : Word-W.\n");
			keycode = gesture_key_array[1];
			break;
		case GESTURE_WORD_V:
			NVT_LOG("Gesture : Word-V.\n");
			keycode = gesture_key_array[2];
			break;
		case GESTURE_DOUBLE_CLICK:
			NVT_LOG("Gesture : Double Click.\n");
			keycode = gesture_key_array[3];
			break;
		case GESTURE_WORD_Z:
			NVT_LOG("Gesture : Word-Z.\n");
			keycode = gesture_key_array[4];
			break;
		case GESTURE_WORD_M:
			NVT_LOG("Gesture : Word-M.\n");
			keycode = gesture_key_array[5];
			break;
		case GESTURE_WORD_O:
			NVT_LOG("Gesture : Word-O.\n");
			keycode = gesture_key_array[6];
			break;
		case GESTURE_WORD_e:
			NVT_LOG("Gesture : Word-e.\n");
			keycode = gesture_key_array[7];
			break;
		case GESTURE_WORD_S:
			NVT_LOG("Gesture : Word-S.\n");
			keycode = gesture_key_array[8];
			break;
		case GESTURE_SLIDE_UP:
			NVT_LOG("Gesture : Slide UP.\n");
			keycode = gesture_key_array[9];
			break;
		case GESTURE_SLIDE_DOWN:
			NVT_LOG("Gesture : Slide DOWN.\n");
			keycode = gesture_key_array[10];
			break;
		case GESTURE_SLIDE_LEFT:
			NVT_LOG("Gesture : Slide LEFT.\n");
			keycode = gesture_key_array[11];
			break;
		case GESTURE_SLIDE_RIGHT:
			NVT_LOG("Gesture : Slide RIGHT.\n");
			keycode = gesture_key_array[12];
			break;
		default:
			break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}
#endif

/*******************************************************
 * Description:
 *     Novatek touchscreen parse device tree function.
 *
 * return:
 *     n.a.
 *******************************************************/
#ifdef CONFIG_OF
static void nvt_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio_flags(np, "novatek,reset-gpio", 0, &ts->reset_flags);
	NVT_LOG("novatek,reset-gpio=%d\n", ts->reset_gpio);
#endif
	ts->irq_gpio = of_get_named_gpio_flags(np, "novatek,irq-gpio", 0, &ts->irq_flags);
	NVT_LOG("novatek,irq-gpio=%d\n", ts->irq_gpio);

}
#else
static void nvt_parse_dt(struct device *dev)
{
#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	ts->irq_gpio = NVTTOUCH_INT_PIN;
}
#endif

/*******************************************************
 * Description:
 *     Novatek touchscreen config and request gpio
 *
 * return:
 *     Executive outcomes. 0---succeed. not 0---failed.
 *******************************************************/
static int nvt_gpio_config(struct nvt_ts_data *ts)
{
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_HIGH, "NVT-tp-rst");
		if (ret) {
			NVT_ERR("Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(ts->irq_gpio)) {
		ret = gpio_request_one(ts->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			NVT_ERR("Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	return ret;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen deconfig gpio
 *
 * return:
 *     n.a.
 *******************************************************/
static void nvt_gpio_deconfig(struct nvt_ts_data *ts)
{
	if (gpio_is_valid(ts->irq_gpio))
		gpio_free(ts->irq_gpio);
#if NVT_TOUCH_SUPPORT_HW_RST
	if (gpio_is_valid(ts->reset_gpio))
		gpio_free(ts->reset_gpio);
#endif
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	/* enable/disable esd check flag */
	esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	//NVT_ERR("esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		NVT_ERR("do ESD recovery, timer = %d, retry = %d\n", timer, esd_retry);
		/* do esd recovery, bootloader reset */
		nvt_bootloader_reset();
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#define POINT_DATA_LEN 65
/*******************************************************
 * Description:
 *     Novatek touchscreen work function.
 *
 * return:
 *     n.a.
 *******************************************************/
static irqreturn_t nvt_ts_work_func(int irq, void *data)
{
	int32_t ret = -1;
	uint8_t point_data[POINT_DATA_LEN + 1] = {0};
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w = 0;
	uint32_t input_p = 0;
	uint8_t input_id = 0;
#if MT_PROTOCOL_B
	uint8_t press_id[TOUCH_MAX_FINGER_NUM] = {0};
#endif /* MT_PROTOCOL_B */
	int32_t i = 0;
	int32_t finger_cnt = 0;

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		pm_wakeup_event(&ts->input_dev->dev, 5000);
	}
#endif

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
#ifndef CONFIG_ARCH_QTI_VM
	if (atomic_read(&ts->trusted_touch_initialized)) {
		atomic_set(&ts->pvm_interrupt_underway, 1);
		reinit_completion(&ts->trusted_touch_interrupt);
	}
#endif
#endif
	mutex_lock(&ts->lock);

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
#ifndef CONFIG_ARCH_QTI_VM
	if (atomic_read(&ts->trusted_touch_initialized)) {
		if (atomic_read(&ts->trusted_touch_underway)) {
			mutex_unlock(&ts->lock);
			atomic_set(&ts->pvm_interrupt_underway, 0);
			input_report_key(ts->input_dev, BTN_TOUCH, 0);
			input_sync(ts->input_dev);
			return IRQ_HANDLED;
		}
	}
#endif
#endif

	ret = CTP_I2C_READ(ts->client, I2C_FW_Address, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		NVT_ERR("CTP_I2C_READ failed.(%d)\n", ret);
		goto XFER_ERROR;
	}

	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		input_id = (uint8_t)(point_data[1] >> 3);
		nvt_ts_wakeup_gesture_report(input_id, point_data);
		mutex_unlock(&ts->lock);
#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
#ifndef CONFIG_ARCH_QTI_VM
		if (atomic_read(&ts->trusted_touch_initialized)) {
			complete(&ts->trusted_touch_interrupt);
			atomic_set(&ts->pvm_interrupt_underway, 0);
		}
#endif
#endif
		return IRQ_HANDLED;
	}
#endif

	finger_cnt = 0;

	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3);
		if ((input_id == 0) || (input_id > ts->max_touch_num))
			continue;

		if (((point_data[position] & 0x07) == 0x01) || ((point_data[position] & 0x07) == 0x02)) {	//finger down (enter & moving)
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
			input_x = (uint32_t)(point_data[position + 1] << 4) + (uint32_t) (point_data[position + 3] >> 4);
			input_y = (uint32_t)(point_data[position + 2] << 4) + (uint32_t) (point_data[position + 3] & 0x0F);
			if ((input_x < 0) || (input_y < 0))
				continue;
			if ((input_x > ts->abs_x_max) || (input_y > ts->abs_y_max))
				continue;
			input_w = (uint32_t)(point_data[position + 4]);
			if (input_w == 0)
				input_w = 1;
			if (i < 2) {
				input_p = (uint32_t)(point_data[position + 5]) + (uint32_t)(point_data[i + 63] << 8);
				if (input_p > TOUCH_FORCE_NUM)
					input_p = TOUCH_FORCE_NUM;
			} else {
				input_p = (uint32_t)(point_data[position + 5]);
			}
			if (input_p == 0)
				input_p = 1;

#if MT_PROTOCOL_B
			press_id[input_id - 1] = 1;
			input_mt_slot(ts->input_dev, input_id - 1);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#else /* MT_PROTOCOL_B */
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, input_id - 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* MT_PROTOCOL_B */

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, input_p);

#if MT_PROTOCOL_B
#else /* MT_PROTOCOL_B */
			input_mt_sync(ts->input_dev);
#endif /* MT_PROTOCOL_B */

			finger_cnt++;
		}
	}

#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		if (press_id[i] != 1) {
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* MT_PROTOCOL_B */
	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
#endif /* MT_PROTOCOL_B */

#if TOUCH_KEY_NUM > 0
	if (point_data[61] == 0xF8) {
#if NVT_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], ((point_data[62] >> i) & 0x01));
		}
	} else {
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], 0);
		}
	}
#endif

	input_sync(ts->input_dev);

XFER_ERROR:

	mutex_unlock(&ts->lock);
#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
#ifndef CONFIG_ARCH_QTI_VM
	if (atomic_read(&ts->trusted_touch_initialized)) {
		complete(&ts->trusted_touch_interrupt);
		atomic_set(&ts->pvm_interrupt_underway, 0);
	}
#endif
#endif

	return IRQ_HANDLED;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen check and stop crc reboot loop.
 *
 * return:
 *     n.a.
*******************************************************/
void nvt_stop_crc_reboot(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;

	//read dummy buffer to check CRC fail reboot is happening or not

	//---change I2C index to prevent geting 0xFF, but not 0xFC---
	nvt_set_page(I2C_BLDR_Address, 0x1F64E);

	//---read to check if buf is 0xFC which means IC is in CRC reboot ---
	buf[0] = 0x4E;
	CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 4);

	if ((buf[1] == 0xFC) ||
		((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {

		//IC is in CRC fail reboot loop, needs to be stopped!
		for (retry = 5; retry > 0; retry--) {

			//---write i2c cmds to reset idle : 1st---
			buf[0]=0x00;
			buf[1]=0xA5;
			CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

			//---write i2c cmds to reset idle : 2rd---
			buf[0]=0x00;
			buf[1]=0xA5;
			CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
			msleep(1);

			//---clear CRC_ERR_FLAG---
			nvt_set_page(I2C_BLDR_Address, 0x3F135);

			buf[0] = 0x35;
			buf[1] = 0xA5;
			CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 2);

			//---check CRC_ERR_FLAG---
			nvt_set_page(I2C_BLDR_Address, 0x3F135);

			buf[0] = 0x35;
			buf[1] = 0x00;
			CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 2);

			if (buf[1] == 0xA5)
				break;
		}
		if (retry == 0)
			NVT_ERR("CRC auto reboot is not able to be stopped!\n");
	}

	return;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen check chip version trim function.
 *
 * return:
 *     Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(uint32_t chip_ver_trim_addr)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	nvt_bootloader_reset(); // NOT in retry loop

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {
		nvt_sw_reset_idle();

		buf[0] = 0x00;
		buf[1] = 0x35;
		CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		msleep(10);

		nvt_set_page(I2C_BLDR_Address, chip_ver_trim_addr);

		buf[0] = chip_ver_trim_addr & 0xFF;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 7);
		NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		//---Stop CRC check to prevent IC auto reboot---
		if ((buf[1] == 0xFC) ||
			((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {
			nvt_stop_crc_reboot();
			continue;
		}

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system = trim_id_table[list].hwinfo->carrier_system;
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

#if defined(CONFIG_DRM)
static int nvt_ts_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			NVT_LOG(" %s:find\n", __func__);
			return 0;
		}
	}

	NVT_ERR(" %s: not find\n", __func__);
	return -ENODEV;
}

static int nvt_ts_check_default_tp(struct device_node *dt, const char *prop)
{
	const char **active_tp = NULL;
	int count, tmp, score = 0;
	const char *active;
	int ret, i;

	count = of_property_count_strings(dt->parent, prop);
	if (count <= 0 || count > 3)
		return -ENODEV;

	active_tp = kcalloc(count, sizeof(char *),  GFP_KERNEL);
	if (!active_tp) {
		NVT_ERR("FTS alloc failed\n");
		return -ENOMEM;
	}

	ret = of_property_read_string_array(dt->parent, prop,
			active_tp, count);
	if (ret < 0) {
		NVT_ERR("fail to read %s %d\n", prop, ret);
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < count; i++) {
		active = active_tp[i];
		if (active != NULL) {
			tmp = of_device_is_compatible(dt, active);
			if (tmp > 0)
				score++;
		}
	}

	if (score <= 0) {
		NVT_ERR("not match this driver\n");
		ret = -ENODEV;
		goto out;
	}
	ret = 0;
out:
	kfree(active_tp);
	return ret;
}
#endif

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
static ssize_t trusted_touch_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nvt_ts_data *ts;

	if (!client)
		return scnprintf(buf, PAGE_SIZE, "client is null\n");

	ts = i2c_get_clientdata(client);
	if (!ts) {
		pr_err("info is null\n");
		return scnprintf(buf, PAGE_SIZE, "info is null\n");
	}

	return scnprintf(buf, PAGE_SIZE, "%d",
			atomic_read(&ts->trusted_touch_enabled));
}

static ssize_t trusted_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nvt_ts_data *info;
	unsigned long value;
	int err = 0;

	if (!client)
		return -EIO;
	info = i2c_get_clientdata(client);
	if (!info) {
		pr_err("info is null\n");
		return -EIO;
	}
	if (count > 2)
		return -EINVAL;
	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!atomic_read(&info->trusted_touch_initialized))
		return -EIO;

#ifdef CONFIG_ARCH_QTI_VM
	err = nvt_ts_handle_trusted_touch_tvm(info, value);
	if (err) {
		pr_err("Failed to handle trusted touch in tvm\n");
		return -EINVAL;
	}
#else
	err = nvt_ts_handle_trusted_touch_pvm(info, value);
	if (err) {
		pr_err("Failed to handle trusted touch in pvm\n");
		return -EINVAL;
	}
#endif
	err = count;
	return err;
}

static ssize_t trusted_touch_event_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nvt_ts_data *info;

	if (!client)
		return scnprintf(buf, PAGE_SIZE, "client is null\n");

	info = i2c_get_clientdata(client);
	if (!info) {
		NVT_ERR("info is null\n");
		return scnprintf(buf, PAGE_SIZE, "info is null\n");
	}

	return scnprintf(buf, PAGE_SIZE, "%d",
			atomic_read(&info->trusted_touch_event));
}

static ssize_t trusted_touch_event_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nvt_ts_data *info;
	unsigned long value;
	int err = 0;

	if (!client)
		return -EIO;
	info = i2c_get_clientdata(client);
	if (!info) {
		NVT_ERR("info is null\n");
		return -EIO;
	}
	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!atomic_read(&info->trusted_touch_initialized))
		return -EIO;

	if (value)
		return -EIO;

	atomic_set(&info->trusted_touch_event, value);

	return count;
}

static DEVICE_ATTR_RW(trusted_touch_enable);
static DEVICE_ATTR_RW(trusted_touch_event);

static struct attribute *nvt_attributes[] = {
	&dev_attr_trusted_touch_enable.attr,
	&dev_attr_trusted_touch_event.attr,
	NULL,
};

static struct attribute_group nvt_attribute_group = {
	.attrs = nvt_attributes
};

static int ts_create_sysfs(struct nvt_ts_data *ts)
{
	int ret = 0;

	ret = sysfs_create_group(&ts->client->dev.kobj, &nvt_attribute_group);
	if (ret) {
		pr_err("[EX]: sysfs_create_group() failed!!\n");
		sysfs_remove_group(&ts->client->dev.kobj, &nvt_attribute_group);
		return -ENOMEM;
	}

	pr_info("[EX]: sysfs_create_group() succeeded!!\n");
	return ret;
}

#ifdef CONFIG_ARCH_QTI_VM
static int32_t nvt_ts_late_probe_tvm(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t ret = 0;

	if (!atomic_read(&ts->delayed_vm_probe_pending)) {
		ts->fw_ver = 0;
		ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ret = nvt_ts_late_probe_sub(ts->client, ts->id);
		if (ret) {
			pr_err("Failed to enable resources\n");
			return ret;
		}
		atomic_set(&ts->delayed_vm_probe_pending, 1);
		return ret;
	}

	NVT_ERR("irq:%d\n", ts->client->irq);
	ret = request_threaded_irq(ts->client->irq, NULL, nvt_ts_work_func,
			IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT, NVT_I2C_NAME, ts);
	if (ret != 0) {
		NVT_ERR("request_threaded_irq failed\n");
		return ret;
	}

	ts->irq_enabled = true;
	bTouchIsAwake = 1;
	NVT_LOG("end\n");
	return 0;
}
#endif
#endif

static int32_t nvt_ts_late_probe_sub(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t ret = 0;
#if ((TOUCH_KEY_NUM > 0) || WAKEUP_GESTURE)
	int32_t retry = 0;
#endif
	//---allocate input device---
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		NVT_ERR("allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->max_touch_num = TOUCH_MAX_FINGER_NUM;

#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
#endif

	ts->int_trigger_type = INT_TRIGGER_TYPE;


	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->max_touch_num, 0);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, TOUCH_FORCE_NUM, 0, 0);    //pressure = TOUCH_FORCE_NUM

#if TOUCH_MAX_FINGER_NUM > 1
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //area = 255

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
#if MT_PROTOCOL_B
	// no need to set ABS_MT_TRACKING_ID, input_mt_init_slots() already set it
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touch_num, 0, 0);
#endif //MT_PROTOCOL_B
#endif //TOUCH_MAX_FINGER_NUM > 1

#if TOUCH_KEY_NUM > 0
	for (retry = 0; retry < ts->max_button_num; retry++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[retry]);
	}
#endif

#if WAKEUP_GESTURE
	for (retry = 0; retry < ARRAY_SIZE(gesture_key_array); retry++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[retry]);
	}
#endif

	snprintf(ts->phys, sizeof(ts->phys), "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = ts->dev;

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		NVT_ERR("register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

	return 0;

err_input_register_device_failed:
	if (ts->input_dev) {
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
	}
err_input_dev_alloc_failed:
	NVT_ERR("ret = %d\n", ret);
	return ret;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen driver probe function.
 *
 * return:
 *     Executive outcomes. 0---succeed. negative---failed
 *******************************************************/
static int32_t nvt_ts_late_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t ret = 0;

	//---request and config GPIOs---
	ret = nvt_gpio_config(ts);
	if (ret) {
		NVT_ERR("gpio config error!\n");
		goto err_gpio_config_failed;
	}

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_ADDR);
	if (ret) {
		NVT_LOG("try to check from old chip ver trim address\n");
		ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_OLD_ADDR);
		if (ret) {
			NVT_ERR("chip is not identified\n");
			ret = -EINVAL;
			goto err_chipvertrim_failed;
		}
	}

	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_INIT);
	nvt_get_fw_info();
	ret = nvt_ts_late_probe_sub(ts->client, ts->id);
	if (ret) {
		NVT_ERR("Failed to enable resources\n");
		goto err_chipvertrim_failed;
	}

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(ts->irq_gpio);
	if (client->irq) {
		NVT_LOG("int_trigger_type=%d\n", ts->int_trigger_type);
		ts->irq_enabled = true;
		ret = request_threaded_irq(client->irq, NULL, nvt_ts_work_func,
				ts->int_trigger_type | IRQF_ONESHOT, NVT_I2C_NAME, ts);
		if (ret != 0) {
			NVT_ERR("request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_irq_enable(false);
			NVT_LOG("request irq %d succeed\n", client->irq);
		}
	}

#if WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 1);
#endif

#if BOOT_UPDATE_FIRMWARE
	nvt_fwu_wq = alloc_workqueue("nvt_fwu_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!nvt_fwu_wq) {
		NVT_ERR("nvt_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	// please make sure boot update start after display reset(RESX) sequence
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(14000));
#endif

	NVT_LOG("NVT_TOUCH_ESD_PROTECT is %d\n", NVT_TOUCH_ESD_PROTECT);
#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_esd_check_work, nvt_esd_check_func);
	nvt_esd_check_wq = alloc_workqueue("nvt_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!nvt_esd_check_wq) {
		NVT_ERR("nvt_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_esd_check_wq_failed;
	}
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}
#endif

	bTouchIsAwake = 1;
	NVT_LOG("end\n");

	nvt_irq_enable(true);

	return 0;

#if NVT_TOUCH_MP
nvt_mp_proc_deinit();
err_mp_proc_init_failed:
#endif
#if NVT_TOUCH_EXT_PROC
nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
err_create_nvt_esd_check_wq_failed:
#endif
#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
err_create_nvt_fwu_wq_failed:
#endif
#if WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif
	free_irq(client->irq, ts);
err_int_request_failed:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_chipvertrim_failed:
	nvt_gpio_deconfig(ts);
err_gpio_config_failed:
	NVT_ERR("ret = %d\n", ret);
	return ret;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen driver probe function.
 *
 * return:
 *     Executive outcomes. 0---succeed. negative---failed
 *******************************************************/
static int32_t nvt_ts_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t ret = 0;
#if defined(CONFIG_DRM)
	struct device_node *dp = client->dev.of_node;
#endif

	NVT_LOG("start\n");

#if defined(CONFIG_DRM)
	if (nvt_ts_check_dt(dp)) {
		if (!nvt_ts_check_default_tp(dp, "qcom,i2c-touch-active"))
			ret = -EPROBE_DEFER;
		else
			ret = -ENODEV;

		return ret;
	}
#endif

	ts = devm_kzalloc(&client->dev, sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		NVT_ERR("failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}

	ts->client = client;
	ts->dev = &client->dev;
	i2c_set_clientdata(client, ts);

	//---parse dts---
	nvt_parse_dt(&client->dev);

	//---check i2c func.---
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		NVT_ERR("i2c_check_functionality failed. (no I2C_FUNC_I2C)\n");
		return  -ENODEV;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);

	ts->id = id;

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	nvt_ts_trusted_touch_init(ts);
	mutex_init(&(ts->nvt_clk_io_ctrl_mutex));
	ret = ts_create_sysfs(ts);
	if (ret)
		NVT_ERR("create sysfs node fail\n");
#endif

#if defined(CONFIG_DRM)
#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	if (!strcmp(ts->touch_environment, "pvm"))
		nvt_i2c_register_for_panel_events(client->dev.of_node, ts);
#else
	nvt_i2c_register_for_panel_events(client->dev.of_node, ts);
#endif
#elif defined(_MSM_DRM_NOTIFY_H_)
	ts->drm_notif.notifier_call = nvt_drm_notifier_callback;
	ret = msm_drm_register_client(&ts->drm_notif);
	if (ret) {
		NVT_ERR("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#else
	ts->fb_notif.notifier_call = nvt_fb_notifier_callback;
	ts->fb_notif.notifier_call = nvt_fb_notifier_callback;
	if (ret) {
		NVT_ERR("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#endif
	NVT_LOG("end\n");

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
	ret = nvt_ts_late_probe_tvm(ts->client, ts->id);
	if (ret)
		NVT_ERR("Failed to enable resources\n");

#endif
#endif
	return 0;
#if defined(CONFIG_DRM)

#elif defined(_MSM_DRM_NOTIFY_H_)
err_register_drm_notif_failed:
#else
err_register_fb_notif_failed:
#endif
	return -ENODEV;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen driver release function.
 *
 * return:
 *     Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_remove(struct i2c_client *client)
{
	NVT_LOG("Removing driver...\n");

#if defined(CONFIG_DRM)
	if (active_panel && ts->notifier_cookie)
		panel_event_notifier_unregister(ts->notifier_cookie);
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#endif

#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if WAKEUP_GESTURE
	if (ts->input_dev)
		device_init_wakeup(&ts->input_dev->dev, 0);
#endif

	nvt_irq_enable(false);
	free_irq(client->irq, ts);

	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	nvt_gpio_deconfig(ts);

	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}

	i2c_set_clientdata(client, NULL);

	if (ts) {
		kfree(ts);
		ts = NULL;
	}

	return 0;
}

static void nvt_ts_shutdown(struct i2c_client *client)
{
	NVT_LOG("Shutdown driver...\n");

	nvt_irq_enable(false);

#if defined(CONFIG_DRM)
	if (active_panel && ts->notifier_cookie)
		panel_event_notifier_unregister(ts->notifier_cookie);
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#endif

#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if WAKEUP_GESTURE
	if (ts->input_dev)
		device_init_wakeup(&ts->input_dev->dev, 0);
#endif
}

/*******************************************************
 * Description:
 *     Novatek touchscreen driver suspend function.
 *
 * return:
 *     Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_suspend(struct device *dev)
{
	uint8_t buf[4] = {0};
#if MT_PROTOCOL_B
	uint32_t i = 0;
#endif

	if (!bTouchIsAwake) {
		NVT_LOG("Touch is already suspend\n");
		return 0;
	}

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	if (atomic_read(&ts->trusted_touch_initialized)) {
		atomic_set(&ts->suspend_resume_underway, 1);
		reinit_completion(&ts->touch_suspend_resume);
	}
#endif

#if !WAKEUP_GESTURE
	nvt_irq_enable(false);
#endif

#if NVT_TOUCH_ESD_PROTECT
	NVT_LOG("cancel delayed work sync\n");
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	bTouchIsAwake = 0;

#if WAKEUP_GESTURE
	//---write command to enter "wakeup gesture mode"---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x13;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

	enable_irq_wake(ts->client->irq);

	NVT_LOG("Enabled touch wakeup gesture\n");

#else // WAKEUP_GESTURE
	//---write command to enter "deep sleep mode"---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x11;
#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	if (atomic_read(&ts->trusted_touch_initialized))
		if (!atomic_read(&ts->trusted_touch_underway))
			CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
#endif
#endif // WAKEUP_GESTURE

	mutex_unlock(&ts->lock);

	/* release all touches */
#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	msleep(50);
#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	if (atomic_read(&ts->trusted_touch_initialized)) {
		complete(&ts->touch_suspend_resume);
		atomic_set(&ts->suspend_resume_underway, 0);
	}
#endif
	NVT_LOG("end\n");

	return 0;
}

/*******************************************************
 * Description:
 *     Novatek touchscreen driver resume function.
 *
 * return:
 *     Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_resume(struct device *dev)
{
	NVT_LOG("start\n");

	if (bTouchIsAwake || ts->fw_ver == 0) {
		nvt_ts_late_probe(ts->client, ts->id);
		NVT_LOG("nvt_ts_late_probe\n");
		return 0;
	}

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	if (atomic_read(&ts->trusted_touch_initialized)) {
		if (atomic_read(&ts->trusted_touch_underway))
			wait_for_completion_interruptible(
				&ts->trusted_touch_powerdown);

		atomic_set(&ts->suspend_resume_underway, 1);
		reinit_completion(&ts->touch_suspend_resume);
	}
#endif
	mutex_lock(&ts->lock);

	// make sure display reset(RESX) sequence and dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	// NT36772 IC due to no boot-load when RESX/TP_RESX
	// nvt_bootloader_reset();
	if (nvt_check_fw_reset_state(RESET_STATE_REK)) {
		NVT_ERR("FW is not ready! Try to bootloader reset...\n");
		nvt_bootloader_reset();
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}

#if !WAKEUP_GESTURE
	nvt_irq_enable(true);
#endif

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	bTouchIsAwake = 1;

	mutex_unlock(&ts->lock);

#ifdef CONFIG_NOVATEK_TRUSTED_TOUCH
	if (atomic_read(&ts->trusted_touch_initialized)) {
		complete(&ts->touch_suspend_resume);
		atomic_set(&ts->suspend_resume_underway, 0);
	}
#endif
	NVT_LOG("end\n");

	return 0;
}

#if defined(CONFIG_DRM)

static void nvt_i2c_panel_notifier_callback(enum panel_event_notifier_tag tag,
		struct panel_event_notification *notification, void *client_data)
{
	struct nvt_ts_data *ts = client_data;

	if (!notification) {
		pr_err("Invalid notification\n");
		return;
	}

	NVT_LOG("Notification type:%d, early_trigger:%d",
		notification->notif_type,
		notification->notif_data.early_trigger);

	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_UNBLANK:
		if (notification->notif_data.early_trigger)
			NVT_LOG("resume notification pre commit\n");
		else
			nvt_ts_resume(&ts->client->dev);
		break;

	case DRM_PANEL_EVENT_BLANK:
		if (notification->notif_data.early_trigger)
			nvt_ts_suspend(&ts->client->dev);
		else
			NVT_LOG("suspend notification post commit\n");
		break;

	case DRM_PANEL_EVENT_BLANK_LP:
		NVT_LOG("received lp event\n");
		break;

	case DRM_PANEL_EVENT_FPS_CHANGE:
		NVT_LOG("Received fps change old fps:%d new fps:%d\n",
			notification->notif_data.old_fps,
			notification->notif_data.new_fps);
		break;
	default:
		NVT_LOG("notification serviced :%d\n",
			notification->notif_type);
		break;
	}
}

#elif defined(_MSM_DRM_NOTIFY_H_)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, drm_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (evdata->data && ts) {
		blank = evdata->data;
		if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_suspend(&ts->client->dev);
			}
		} else if (event == MSM_DRM_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_resume(&ts->client->dev);
			}
		}
	}

	return 0;
}

#else
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_suspend(&ts->client->dev);
		}
	} else if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_resume(&ts->client->dev);
		}
	}

	return 0;
}
#endif

static const struct i2c_device_id nvt_ts_id[] = {
	{ NVT_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id nvt_match_table[] = {
	{ .compatible = "novatek,NVT-ts",},
	{ },
};
#endif

static struct i2c_driver nvt_i2c_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	.shutdown	= nvt_ts_shutdown,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_I2C_NAME,
#ifdef CONFIG_OF
		.of_match_table = nvt_match_table,
#endif
	},
};

/*******************************************************
 * Description:
 *     Driver Install function.
 *
 * return:
 *     Executive Outcomes. 0---succeed. not 0---failed.
 ********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	NVT_LOG("start\n");
	//---add i2c driver---
	ret = i2c_add_driver(&nvt_i2c_driver);
	if (ret) {
		NVT_ERR("failed to add i2c driver");
		goto err_driver;
	}

	NVT_LOG("finished\n");

err_driver:
	return ret;
}

/*******************************************************
 * Description:
 *     Driver uninstall function.
 *
 * return:
 *     n.a.
 ********************************************************/
static void __exit nvt_driver_exit(void)
{
	i2c_del_driver(&nvt_i2c_driver);
}

#ifdef CONFIG_ARCH_QTI_VM
module_init(nvt_driver_init);
#else
late_initcall(nvt_driver_init);
#endif
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL v2");

#else  /* NT36XXX_SPI */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#if defined(CONFIG_DRM)
#include <linux/soc/qcom/panel_event_notifier.h>
#endif

#include "nt36xxx.h"
#if NVT_SPI_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

#if NVT_SPI_TOUCH_ESD_PROTECT
static struct delayed_work nvt_spi_esd_check_work;
static struct workqueue_struct *nvt_spi_esd_check_wq;
static unsigned long nvt_spi_irq_timer;
uint8_t nvt_spi_esd_check;
uint8_t nvt_spi_esd_retry;
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

struct nvt_spi_data_t *nvt_spi_data;

#if NVT_SPI_BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_spi_fwu_wq;
#endif

#if defined(CONFIG_DRM)
static struct drm_panel *active_spi_panel;
static void nvt_spi_panel_notifier_callback(enum panel_event_notifier_tag tag,
			struct panel_event_notification *event, void *client_data);

#elif defined(_MSM_DRM_NOTIFY_H_)
static int nvt_drm_notifier_callback(
		struct notifier_block *self, unsigned long event, void *data);
#else
static int nvt_fb_notifier_callback(
		struct notifier_block *self, unsigned long event, void *data);
#endif

static uint32_t NVT_SPI_ENG_RST_ADDR = 0x7FFF80;
uint32_t NVT_SPI_SWRST_N8_ADDR; //read from dtsi
uint32_t NVT_SPI_RD_FAST_ADDR;  //read from dtsi

#if NVT_SPI_TOUCH_KEY_NUM > 0
const uint16_t nvt_spi_touch_key_array[NVT_SPI_TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

#if NVT_SPI_WAKEUP_GESTURE
const uint16_t nvt_spi_gesture_key_array[] = {
	KEY_POWER,  //GESTURE_WORD_C
	KEY_POWER,  //GESTURE_WORD_W
	KEY_POWER,  //GESTURE_WORD_V
	KEY_POWER,  //GESTURE_DOUBLE_CLICK
	KEY_POWER,  //GESTURE_WORD_Z
	KEY_POWER,  //GESTURE_WORD_M
	KEY_POWER,  //GESTURE_WORD_O
	KEY_POWER,  //GESTURE_WORD_e
	KEY_POWER,  //GESTURE_WORD_S
	KEY_POWER,  //GESTURE_SLIDE_UP
	KEY_POWER,  //GESTURE_SLIDE_DOWN
	KEY_POWER,  //GESTURE_SLIDE_LEFT
	KEY_POWER,  //GESTURE_SLIDE_RIGHT
};
#endif

static uint8_t bTouchIsAwake;

#if defined(CONFIG_DRM)
static void nvt_spi_register_for_panel_events(struct device_node *dp,
					struct nvt_spi_data_t *ts)
{
	void *cookie = NULL;

	cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
			PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_TOUCH, active_spi_panel,
			&nvt_spi_panel_notifier_callback, ts);
	if (!cookie) {
		pr_err("Failed to register for panel events\n");
		return;
	}

	ts->notifier_cookie = cookie;
}
#endif

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen irq enable/disable function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_irq_enable(bool enable)
{
	struct irq_desc *desc;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (enable) {
		if (!ts->irq_enabled) {
			enable_irq(ts->client->irq);
			ts->irq_enabled = true;
		}
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			ts->irq_enabled = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	NVT_LOG("enable=%d, desc->depth=%d\n", enable, desc->depth);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen spi read/write core function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static inline int32_t nvt_spi_read_write(
		struct spi_device *client, uint8_t *buf, size_t len, enum NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};
	struct nvt_spi_data_t *ts = nvt_spi_data;

	memset(ts->xbuf, 0, len + NVT_SPI_DUMMY_BYTES);
	memcpy(ts->xbuf, buf, len);

	switch (rw) {
	case NVT_SPI_READ:
		t.tx_buf = ts->xbuf;
		t.rx_buf = ts->rbuf;
		t.len = (len + NVT_SPI_DUMMY_BYTES);
		break;

	case NVT_SPI_WRITE:
		t.tx_buf = ts->xbuf;
		break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(client, &m);
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen spi read function.
 *
 * return:
 *	Executive outcomes. 2---succeed. -5---I/O error
 ******************************************************
 */
int32_t nvt_spi_read(uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = NVT_SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = nvt_spi_read_write(ts->client, buf, len, NVT_SPI_READ);
		if (ret == 0)
			break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("read error, ret=%d\n", ret);
		ret = -EIO;
	} else
		memcpy((buf+1), (ts->rbuf+2), (len-1));

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen spi write function.
 *
 * return:
 *	Executive outcomes. 1---succeed. -5---I/O error
 ******************************************************
 */
int32_t nvt_spi_write(uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = NVT_SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = nvt_spi_read_write(ts->client, buf, len, NVT_SPI_WRITE);
		if (ret == 0)
			break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen set index/page/addr address.
 *
 * return:
 *	Executive outcomes. 0---succeed. -5---access fail.
 ******************************************************
 */
int32_t nvt_spi_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return nvt_spi_write(buf, 3);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen write data to specify address.
 *
 * return:
 *	Executive outcomes. 0---succeed. -5---access fail.
 *******************************************************
 */
int32_t nvt_spi_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = nvt_spi_write(buf, 3);
	if (ret) {
		NVT_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = nvt_spi_write(buf, 2);
	if (ret) {
		NVT_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen enable hw bld crc function.
 *
 * return:
 *	N/A.
 ******************************************************
 */
void nvt_spi_bld_crc_enable(void)
{
	uint8_t buf[4] = {0};
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to BLD_CRC_EN_ADDR---
	nvt_spi_set_page(ts->mmap->BLD_CRC_EN_ADDR);

	//---read data from index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = 0xFF;
	nvt_spi_read(buf, 2);

	//---write data to index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = buf[1] | (0x01 << 7);
	nvt_spi_write(buf, 2);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen clear status & enable fw crc function.
 *
 * return:
 *	N/A.
 ******************************************************
 */
void nvt_spi_fw_crc_enable(void)
{
	uint8_t buf[4] = {0};
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR);

	//---clear fw reset status---
	buf[0] = NVT_SPI_EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	nvt_spi_write(buf, 2);

	//---enable fw crc---
	buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	nvt_spi_write(buf, 2);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen set boot ready function.
 *
 * return:
 *	N/A.
 ******************************************************
 */
void nvt_spi_boot_ready(void)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---write BOOT_RDY status cmds---
	nvt_spi_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	mdelay(5);

	if (!ts->hw_crc) {
		//---write BOOT_RDY status cmds---
		nvt_spi_write_addr(ts->mmap->BOOT_RDY_ADDR, 0);

		//---write POR_CD cmds---
		nvt_spi_write_addr(ts->mmap->POR_CD_ADDR, 0xA0);
	}
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen enable auto copy mode function.
 *
 * return:
 *	N/A.
 ******************************************************
 */
void nvt_spi_tx_auto_copy_mode(void)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---write TX_AUTO_COPY_EN cmds---
	nvt_spi_write_addr(ts->mmap->TX_AUTO_COPY_EN, 0x69);

	NVT_ERR("tx auto copy mode enable\n");
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen check spi dma tx info function.
 *
 * return:
 *	N/A.
 ******************************************************
 */
int32_t nvt_spi_check_spi_dma_tx_info(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 200;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_spi_set_page(ts->mmap->SPI_DMA_TX_INFO);

		//---read fw status---
		buf[0] = ts->mmap->SPI_DMA_TX_INFO & 0x7F;
		buf[1] = 0xFF;
		nvt_spi_read(buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(1000, 1100);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -EIO;
	}

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen eng reset cmd function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_eng_reset(void)
{
	//---eng reset cmds to ENG_RST_ADDR---
	nvt_spi_write_addr(NVT_SPI_ENG_RST_ADDR, 0x5A);

	mdelay(1);	//wait tMCU_Idle2TP_REX_Hi after TP_RST
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen reset MCU function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_sw_reset(void)
{
	//---software reset cmds to SWRST_N8_ADDR---
	nvt_spi_write_addr(NVT_SPI_SWRST_N8_ADDR, 0x55);

	msleep(20);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen reset MCU then into idle mode
 *	function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_N8_ADDR---
	nvt_spi_write_addr(NVT_SPI_SWRST_N8_ADDR, 0xAA);

	msleep(20);
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen reset MCU (boot) function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_bootloader_reset(void)
{
	//---reset cmds to SWRST_N8_ADDR---
	nvt_spi_write_addr(NVT_SPI_SWRST_N8_ADDR, 0x69);

	mdelay(5);	//wait tBRST2FR after Bootload RST

	if (NVT_SPI_RD_FAST_ADDR) {
		/* disable SPI_RD_FAST */
		nvt_spi_write_addr(NVT_SPI_RD_FAST_ADDR, 0x00);
	}

	NVT_LOG("end\n");
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen clear FW status function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---fail.
 ******************************************************
 */
int32_t nvt_spi_clear_fw_status(void)
{
	uint32_t addr;
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		addr = ts->mmap->EVENT_BUF_ADDR;
		nvt_spi_set_page(addr | NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		nvt_spi_write(buf, 2);

		//---read fw status---
		buf[0] = NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		nvt_spi_read(buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 11000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -EIO;
	}

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen check FW status function.

 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 ******************************************************
 */
int32_t nvt_spi_check_fw_status(void)
{
	uint32_t addr;
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	usleep_range(20000, 21000);

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		addr = ts->mmap->EVENT_BUF_ADDR;
		nvt_spi_set_page(addr | NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = NVT_SPI_EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		nvt_spi_read(buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 11000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -EIO;
	}

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen check FW reset state function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 ******************************************************
 */
int32_t nvt_spi_check_fw_reset_state(enum NVT_SPI_RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == NVT_SPI_RESET_STATE_INIT) ? 10 : 50;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR | NVT_SPI_EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = NVT_SPI_EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		nvt_spi_read(buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= NVT_SPI_RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if (unlikely(retry > retry_max)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X,0x%02X,0x%02X,0x%02X,0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		usleep_range(10000, 11000);
	}

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen get firmware related information
 *	function.
 *
 * return:
 *	Executive outcomes. 0---success. -1---fail.
 ******************************************************
 */
int32_t nvt_spi_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_spi_set_page(ts->mmap->EVENT_BUF_ADDR | NVT_SPI_EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = NVT_SPI_EVENT_MAP_FWINFO;
	nvt_spi_read(buf, 39);
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		if (retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			ts->fw_ver = 0;
			ts->abs_x_max = NVT_SPI_TOUCH_DEFAULT_MAX_WIDTH;
			ts->abs_y_max = NVT_SPI_TOUCH_DEFAULT_MAX_HEIGHT;
			ts->max_button_num = NVT_SPI_TOUCH_KEY_NUM;
			NVT_ERR("Set default fw_ver=%d, abs_x_max=%d, ",
					ts->fw_ver, ts->abs_x_max);
			NVT_ERR("abs_y_max=%d, max_button_num=%d!\n",
					ts->abs_y_max, ts->max_button_num);
			ret = -1;
			goto out;
		}
	}
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];
	ts->nvt_pid = (uint16_t)((buf[36] << 8) | buf[35]);
	if (ts->pen_support) {
		ts->x_gang_num = buf[37];
		ts->y_gang_num = buf[38];
	}

	NVT_ERR("fw_ver=0x%02X, fw_type=0x%02X, PID=0x%04X\n", ts->fw_ver, buf[14], ts->nvt_pid);

	ret = 0;
out:

	return ret;
}

/*
 ******************************************************
 * Create Device Node (Proc Entry)
 ******************************************************
 */
#if NVT_SPI_TOUCH_PROC
static struct proc_dir_entry *nvt_spi_proc_entry;
#define DEVICE_NAME "NVTSPI"

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTSPI read function.
 *
 * return:
 *	Executive outcomes. 2---succeed. -5,-14---failed.
 ******************************************************
 */
static ssize_t nvt_spi_flash_read(
	struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_SPI_TRANSFER_LEN + 3) || (count < 3)) {
		NVT_ERR("invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = kzalloc((count), GFP_KERNEL);
	if (str == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = kzalloc((count), GFP_KERNEL | GFP_DMA);
	if (buf == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_SPI_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd
	 * check again finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_spi_esd_check_work);
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVT_SPI_WRITE) {	//SPI write
		while (retries < 20) {
			ret = nvt_spi_write(buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVT_SPI_READ) {	//SPI read
		while (retries < 20) {
			ret = nvt_spi_read(buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
	kfree(buf);
kzalloc_failed:
	return ret;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTSPI open function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---failed.
 ******************************************************
 */
static int32_t nvt_spi_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_spi_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_spi_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTSPI close function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t nvt_spi_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_spi_flash_data *dev = file->private_data;

	kfree(dev);

	return 0;
}

static const struct proc_ops nvt_flash_fops_spi = {
	.proc_open = nvt_spi_flash_open,
	.proc_release = nvt_spi_flash_close,
	.proc_read = nvt_spi_flash_read,
};

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTSPI initial function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---failed.
 ******************************************************
 */
static int32_t nvt_spi_flash_proc_init(void)
{
	nvt_spi_proc_entry = proc_create(DEVICE_NAME, 0444, NULL, &nvt_flash_fops_spi);
	if (nvt_spi_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	}

	NVT_LOG("Succeeded!\n");

	NVT_LOG("============================================================\n");
	NVT_LOG("Create /proc/%s\n", DEVICE_NAME);
	NVT_LOG("============================================================\n");

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTSPI deinitial function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_flash_proc_deinit(void)
{
	if (nvt_spi_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		nvt_spi_proc_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", DEVICE_NAME);
	}
}
#endif

#if NVT_SPI_WAKEUP_GESTURE
#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24
/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE         1

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen wake up gesture key report function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];
	struct nvt_spi_data_t *ts = nvt_spi_data;

	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE))
		gesture_id = func_id;
	else if (gesture_id > DATA_PROTOCOL) {
		NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n",
				gesture_id, func_type, func_id);
		return;
	}

	NVT_LOG("gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
	case GESTURE_WORD_C:
		NVT_LOG("Gesture : Word-C.\n");
		keycode = nvt_spi_gesture_key_array[0];
		break;
	case GESTURE_WORD_W:
		NVT_LOG("Gesture : Word-W.\n");
		keycode = nvt_spi_gesture_key_array[1];
		break;
	case GESTURE_WORD_V:
		NVT_LOG("Gesture : Word-V.\n");
		keycode = nvt_spi_gesture_key_array[2];
		break;
	case GESTURE_DOUBLE_CLICK:
		NVT_LOG("Gesture : Double Click.\n");
		keycode = nvt_spi_gesture_key_array[3];
		break;
	case GESTURE_WORD_Z:
		NVT_LOG("Gesture : Word-Z.\n");
		keycode = nvt_spi_gesture_key_array[4];
		break;
	case GESTURE_WORD_M:
		NVT_LOG("Gesture : Word-M.\n");
		keycode = nvt_spi_gesture_key_array[5];
		break;
	case GESTURE_WORD_O:
		NVT_LOG("Gesture : Word-O.\n");
		keycode = nvt_spi_gesture_key_array[6];
		break;
	case GESTURE_WORD_e:
		NVT_LOG("Gesture : Word-e.\n");
		keycode = nvt_spi_gesture_key_array[7];
		break;
	case GESTURE_WORD_S:
		NVT_LOG("Gesture : Word-S.\n");
		keycode = nvt_spi_gesture_key_array[8];
		break;
	case GESTURE_SLIDE_UP:
		NVT_LOG("Gesture : Slide UP.\n");
		keycode = nvt_spi_gesture_key_array[9];
		break;
	case GESTURE_SLIDE_DOWN:
		NVT_LOG("Gesture : Slide DOWN.\n");
		keycode = nvt_spi_gesture_key_array[10];
		break;
	case GESTURE_SLIDE_LEFT:
		NVT_LOG("Gesture : Slide LEFT.\n");
		keycode = nvt_spi_gesture_key_array[11];
		break;
	case GESTURE_SLIDE_RIGHT:
		NVT_LOG("Gesture : Slide RIGHT.\n");
		keycode = nvt_spi_gesture_key_array[12];
		break;
	default:
		break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}
#endif

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen parse device tree function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
#ifdef CONFIG_OF
static int32_t nvt_spi_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int32_t ret = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio_flags(np, "novatek,reset-gpio", 0, &ts->reset_flags);
	NVT_LOG("novatek,reset-gpio=%d\n", ts->reset_gpio);
#endif
	ts->irq_gpio = of_get_named_gpio_flags(np, "novatek,irq-gpio", 0, &ts->irq_flags);
	NVT_LOG("novatek,irq-gpio=%d\n", ts->irq_gpio);

	ts->pen_support = of_property_read_bool(np, "novatek,pen-support");
	NVT_LOG("novatek,pen-support=%d\n", ts->pen_support);

	ts->wgp_stylus = of_property_read_bool(np, "novatek,wgp-stylus");
	NVT_LOG("novatek,wgp-stylus=%d\n", ts->wgp_stylus);

	ret = of_property_read_u32(np, "novatek,swrst-n8-addr", &NVT_SPI_SWRST_N8_ADDR);
	if (ret) {
		NVT_ERR("error reading novatek,swrst-n8-addr. ret=%d\n", ret);
		return ret;
	}
	NVT_LOG("SWRST_N8_ADDR=0x%06X\n", NVT_SPI_SWRST_N8_ADDR);

	ret = of_property_read_u32(np, "novatek,spi-rd-fast-addr", &NVT_SPI_RD_FAST_ADDR);
	if (ret) {
		NVT_LOG("not support novatek,spi-rd-fast-addr\n");
		NVT_SPI_RD_FAST_ADDR = 0;
		ret = 0;
	} else
		NVT_LOG("SPI_RD_FAST_ADDR=0x%06X\n", NVT_SPI_RD_FAST_ADDR);

	return ret;
}
#else
static int32_t nvt_spi_parse_dt(struct device *dev)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	ts->irq_gpio = NVTTOUCH_INT_PIN;
	return 0;
}
#endif

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen config and request gpio
 *
 * return:
 *	Executive outcomes. 0---succeed. not 0---failed.
 ******************************************************
 */
static int nvt_spi_gpio_config(struct nvt_spi_data_t *ts)
{
	int32_t ret = 0;

#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_LOW, "NVT-tp-rst");
		if (ret) {
			NVT_ERR("Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(ts->irq_gpio)) {
		ret = gpio_request_one(ts->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			NVT_ERR("Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	return ret;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen deconfig gpio
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_gpio_deconfig(struct nvt_spi_data_t *ts)
{
	if (gpio_is_valid(ts->irq_gpio))
		gpio_free(ts->irq_gpio);
#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	if (gpio_is_valid(ts->reset_gpio))
		gpio_free(ts->reset_gpio);
#endif
}

static uint8_t nvt_spi_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i = 1; i < 7; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_SPI_TOUCH_ESD_PROTECT
void nvt_spi_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	nvt_spi_irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	nvt_spi_esd_retry = enable ? 0 : nvt_spi_esd_retry;
	/* enable/disable esd check flag */
	nvt_spi_esd_check = enable;
}

static void nvt_spi_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - nvt_spi_irq_timer);
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//NVT_LOG("esd_check = %d (retry %d)\n", esd_check, esd_retry);

	if ((timer > NVT_SPI_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		NVT_ERR("do ESD recovery, timer = %d, retry = %d\n", timer, nvt_spi_esd_retry);
		/* do esd recovery, reload fw */
		nvt_spi_update_firmware(NVT_SPI_BOOT_UPDATE_FIRMWARE_NAME);
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		nvt_spi_irq_timer = jiffies;
		/* update esd_retry counter */
		nvt_spi_esd_retry++;
	}

	queue_delayed_work(nvt_spi_esd_check_wq, &nvt_spi_esd_check_work,
			msecs_to_jiffies(NVT_SPI_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

#define NVT_SPI_PEN_DATA_LEN 14
#if NVT_SPI_CHECK_PEN_DATA_CHECKSUM
static int32_t nvt_spi_pen_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Calculate checksum
	for (i = 0; i < length - 1; i++)
		checksum += buf[i];
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length - 1]) {
		NVT_ERR("pen packet checksum not match. (buf[%d]=0x%02X, checksum=0x%02X)\n",
			length - 1, buf[length - 1], checksum);
		//--- dump pen buf ---
		for (i = 0; i < length; i++)
			NVT_ERR("%02X ", buf[i]);

		NVT_ERR("\n");

		return -EIO;
	}

	return 0;
}
#endif // #if NVT_SPI_CHECK_PEN_DATA_CHECKSUM

#if NVT_SPI_TOUCH_WDT_RECOVERY
static uint8_t nvt_spi_recovery_cnt;
static uint8_t nvt_spi_wdt_fw_recovery(uint8_t *point_data)
{
	uint32_t recovery_cnt_max = 10;
	uint8_t recovery_enable = false;
	uint8_t i = 0;

	nvt_spi_recovery_cnt++;

	/* check pattern */
	for (i = 1 ; i < 7 ; i++) {
		if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
			nvt_spi_recovery_cnt = 0;
			break;
		}
	}

	if (nvt_spi_recovery_cnt > recovery_cnt_max) {
		recovery_enable = true;
		nvt_spi_recovery_cnt = 0;
	}

	return recovery_enable;
}
#endif	/* #if NVT_SPI_TOUCH_WDT_RECOVERY */

#if NVT_SPI_POINT_DATA_CHECKSUM
static int32_t nvt_spi_point_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++)
		checksum += buf[i + 1];

	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		NVT_ERR("packet checksum not match. (point_data[%d]=0x%02X, checksum=0x%02X)\n",
			length, buf[length], checksum);

		for (i = 0; i < 10; i++)
			NVT_LOG("%02X %02X %02X %02X %02X %02X\n",
				buf[1 + i * 6], buf[2 + i * 6], buf[3 + i * 6],
				buf[4 + i * 6], buf[5 + i * 6], buf[6 + i * 6]);

		NVT_LOG("%02X %02X %02X %02X %02X\n",
			buf[61], buf[62], buf[63], buf[64], buf[65]);
		return -EIO;
	}

	return 0;
}
#endif /* NVT_SPI_POINT_DATA_CHECKSUM */

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen work function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
#define NVT_SPI_POINT_DATA_LEN 65
#define NVT_SPI_POINT (NVT_SPI_POINT_DATA_LEN + NVT_SPI_PEN_DATA_LEN + 1 + NVT_SPI_DUMMY_BYTES)
static irqreturn_t nvt_spi_work_func(int irq, void *data)
{
	int32_t ret = -1;
	uint8_t point_data[NVT_SPI_POINT] = {0};
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w = 0;
	uint32_t input_p = 0;
	uint8_t input_id = 0;
#if NVT_SPI_MT_PROTOCOL_B
	uint8_t press_id[NVT_SPI_TOUCH_MAX_FINGER_NUM] = {0};
#endif /* NVT_SPI_MT_PROTOCOL_B */
	int32_t i = 0;
	int32_t finger_cnt = 0;
	uint8_t pen_format_id = 0;
	uint32_t pen_x = 0;
	uint32_t pen_y = 0;
	uint32_t pen_pressure = 0;
	uint32_t pen_distance = 0;
	int8_t pen_tilt_x = 0;
	int8_t pen_tilt_y = 0;
	uint32_t pen_btn1 = 0;
	uint32_t pen_btn2 = 0;
	uint32_t pen_battery = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

#if NVT_SPI_WAKEUP_GESTURE
	if (bTouchIsAwake == 0)
		pm_wakeup_event(&ts->input_dev->dev, 5000);

#endif

	mutex_lock(&ts->lock);

	if (ts->pen_support)
		ret = nvt_spi_read(point_data, NVT_SPI_POINT_DATA_LEN + NVT_SPI_PEN_DATA_LEN + 1);
	else
		ret = nvt_spi_read(point_data, NVT_SPI_POINT_DATA_LEN + 1);

	if (ret < 0) {
		NVT_ERR("nvt_spi_read failed.(%d)\n", ret);
		goto XFER_ERROR;
	}

	//--- dump SPI buf ---
/*
 *	for (i = 0; i < 10; i++) {
 *		printk("%02X %02X %02X %02X %02X %02X  ",
 *			point_data[1+i*6], point_data[2+i*6], point_data[3+i*6],
 *			point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
 *	}
 *	printk("\n");
 */

#if NVT_SPI_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_spi_wdt_fw_recovery(point_data)) {
		NVT_ERR("Recover for fw reset, %02X\n", point_data[1]);
		nvt_spi_update_firmware(NVT_SPI_BOOT_UPDATE_FIRMWARE_NAME);
		goto XFER_ERROR;
	}
#endif /* #if NVT_SPI_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_spi_fw_recovery(point_data)) {
#if NVT_SPI_TOUCH_ESD_PROTECT
		nvt_spi_esd_check_enable(true);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if NVT_SPI_POINT_DATA_CHECKSUM
	if (NVT_SPI_POINT_DATA_LEN >= NVT_SPI_POINT_DATA_CHECKSUM_LEN) {
		ret = nvt_spi_point_data_checksum(point_data, NVT_SPI_POINT_DATA_CHECKSUM_LEN);
		if (ret)
			goto XFER_ERROR;
	}
#endif /* NVT_SPI_POINT_DATA_CHECKSUM */

#if NVT_SPI_WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		input_id = (uint8_t)(point_data[1] >> 3);
		nvt_spi_wakeup_gesture_report(input_id, point_data);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}
#endif

	finger_cnt = 0;

	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3);
		if ((input_id == 0) || (input_id > ts->max_touch_num))
			continue;

		if (((point_data[position] & 0x07) == 0x01)
				|| ((point_data[position] & 0x07) == 0x02)) {
			//finger down (enter & moving)
#if NVT_SPI_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			nvt_spi_irq_timer = jiffies;
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */
			input_x = (uint32_t)(point_data[position + 1] << 4);
			input_x += (uint32_t) (point_data[position + 3] >> 4);

			input_y = (uint32_t)(point_data[position + 2] << 4);
			input_y += (uint32_t) (point_data[position + 3] & 0x0F);
			if ((input_x < 0) || (input_y < 0))
				continue;
			if ((input_x > ts->abs_x_max) || (input_y > ts->abs_y_max))
				continue;
			input_w = (uint32_t)(point_data[position + 4]);
			if (input_w == 0)
				input_w = 1;
			if (i < 2) {
				input_p = (uint32_t)(point_data[position + 5]);
				input_p += (uint32_t)(point_data[i + 63] << 8);
				if (input_p > NVT_SPI_TOUCH_FORCE_NUM)
					input_p = NVT_SPI_TOUCH_FORCE_NUM;
			} else
				input_p = (uint32_t)(point_data[position + 5]);

			if (input_p == 0)
				input_p = 1;

#if NVT_SPI_MT_PROTOCOL_B
			press_id[input_id - 1] = 1;
			input_mt_slot(ts->input_dev, input_id - 1);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#else /* NVT_SPI_MT_PROTOCOL_B */
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, input_id - 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* NVT_SPI_MT_PROTOCOL_B */

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, input_p);

#if NVT_SPI_MT_PROTOCOL_B
#else /* NVT_SPI_MT_PROTOCOL_B */
			input_mt_sync(ts->input_dev);
#endif /* NVT_SPI_MT_PROTOCOL_B */

			finger_cnt++;
		}
	}

#if NVT_SPI_MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		if (press_id[i] != 1) {
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* NVT_SPI_MT_PROTOCOL_B */
	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
#endif /* NVT_SPI_MT_PROTOCOL_B */

#if NVT_SPI_TOUCH_KEY_NUM > 0
	if (point_data[61] == 0xF8) {
#if NVT_SPI_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		nvt_spi_irq_timer = jiffies;
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */
		for (i = 0; i < ts->max_button_num; i++)
			input_report_key(ts->input_dev, nvt_spi_touch_key_array[i],
					((point_data[62] >> i) & 0x01));

	} else {
		for (i = 0; i < ts->max_button_num; i++)
			input_report_key(ts->input_dev, nvt_spi_touch_key_array[i], 0);
	}
#endif

	input_sync(ts->input_dev);

	if (ts->pen_support) {

		//--- dump pen buf ---
/*
 *		printk("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
 *			point_data[66], point_data[67], point_data[68], point_data[69],
 *			point_data[70], point_data[71], point_data[72], point_data[73],
 *			point_data[74], point_data[75], point_data[76], point_data[77],
 *			point_data[78], point_data[79]);
 */
#if NVT_SPI_CHECK_PEN_DATA_CHECKSUM
		if (nvt_spi_pen_data_checksum(&point_data[66], NVT_SPI_PEN_DATA_LEN)) {
			// pen data packet checksum not match, skip it
			goto XFER_ERROR;
		}
#endif // #if NVT_SPI_CHECK_PEN_DATA_CHECKSUM

		// parse and handle pen report
		pen_format_id = point_data[66];
		if (pen_format_id != 0xFF) {
			if (pen_format_id == 0x01) {
				// report pen data
				pen_x = (uint32_t)(point_data[67] << 8);
				pen_x += (uint32_t)(point_data[68]);

				pen_y = (uint32_t)(point_data[69] << 8);
				pen_y += (uint32_t)(point_data[70]);

				pen_pressure = (uint32_t)(point_data[71] << 8);
				pen_pressure +=  (uint32_t)(point_data[72]);

				pen_tilt_x = (int32_t)point_data[73];
				pen_tilt_y = (int32_t)point_data[74];

				pen_distance = (uint32_t)(point_data[75] << 8);
				pen_distance += (uint32_t)(point_data[76]);

				pen_btn1 = (uint32_t)(point_data[77] & 0x01);
				pen_btn2 = (uint32_t)((point_data[77] >> 1) & 0x01);
				pen_battery = (uint32_t)point_data[78];
//				printk("x=%d,y=%d,p=%d,tx=%d,ty=%d,d=%d,b1=%d,b2=%d,bat=%d\n",
//					pen_x, pen_y, pen_pressure, pen_tilt_x, pen_tilt_y,
//					pen_distance, pen_btn1, pen_btn2, pen_battery);

				input_report_abs(ts->pen_input_dev, ABS_X, pen_x);
				input_report_abs(ts->pen_input_dev, ABS_Y, pen_y);
				input_report_abs(ts->pen_input_dev, ABS_PRESSURE, pen_pressure);
				input_report_key(ts->pen_input_dev, BTN_TOUCH, !!pen_pressure);
				input_report_abs(ts->pen_input_dev, ABS_TILT_X, pen_tilt_x);
				input_report_abs(ts->pen_input_dev, ABS_TILT_Y, pen_tilt_y);
				input_report_abs(ts->pen_input_dev, ABS_DISTANCE, pen_distance);
				input_report_key(ts->pen_input_dev, BTN_TOOL_PEN,
						!!pen_distance || !!pen_pressure);
				input_report_key(ts->pen_input_dev, BTN_STYLUS, pen_btn1);
				input_report_key(ts->pen_input_dev, BTN_STYLUS2, pen_btn2);
				// TBD: pen battery event report
				// NVT_LOG("pen_battery=%d\n", pen_battery);
			} else if (pen_format_id == 0xF0) {
				// report Pen ID
			} else {
				NVT_ERR("Unknown pen format id!\n");
				goto XFER_ERROR;
			}
		} else { // pen_format_id = 0xFF, i.e. no pen present
			input_report_abs(ts->pen_input_dev, ABS_X, 0);
			input_report_abs(ts->pen_input_dev, ABS_Y, 0);
			input_report_abs(ts->pen_input_dev, ABS_PRESSURE, 0);
			input_report_abs(ts->pen_input_dev, ABS_TILT_X, 0);
			input_report_abs(ts->pen_input_dev, ABS_TILT_Y, 0);
			input_report_abs(ts->pen_input_dev, ABS_DISTANCE, 0);
			input_report_key(ts->pen_input_dev, BTN_TOUCH, 0);
			input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, 0);
			input_report_key(ts->pen_input_dev, BTN_STYLUS, 0);
			input_report_key(ts->pen_input_dev, BTN_STYLUS2, 0);
		}

		input_sync(ts->pen_input_dev);
	} /* if (ts->pen_support) */

XFER_ERROR:

	mutex_unlock(&ts->lock);

	return IRQ_HANDLED;
}


/*
 *******************************************************
 * Description:
 *	Novatek touchscreen check chip version trim function.
 *
 * return:
 *	Executive outcomes. 0---NVT IC. -1---not NVT IC.
 ******************************************************
 */
static int8_t nvt_spi_check_chip_ver_trim(uint32_t chip_ver_trim_addr)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_spi_bootloader_reset();

		nvt_spi_set_page(chip_ver_trim_addr);

		buf[0] = chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		nvt_spi_read(buf, 7);
		NVT_LOG("buf[1]=0x%02X,[2]=0x%02X,[3]=0x%02X,[4]=0x%02X,[5]=0x%02X,[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < ARRAY_SIZE(nvt_spi_trim_id_table); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_SPI_ID_BYTE_MAX; i++) {
				if (nvt_spi_trim_id_table[list].mask[i]) {
					if (buf[i + 1] != nvt_spi_trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_SPI_ID_BYTE_MAX)
				found_nvt_chip = 1;

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				ts->mmap = nvt_spi_trim_id_table[list].mmap;
				ts->hw_crc = nvt_spi_trim_id_table[list].hwinfo->hw_crc;
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(20);
	}

out:
	return ret;
}

#if defined(CONFIG_DRM)
static int nvt_spi_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_spi_panel = panel;
			return 0;
		}
	}

	return PTR_ERR(panel);
}
#endif

static int32_t nvt_spi_late_probe(struct spi_device *client)
{
	int32_t ret = 0;

	//---check chip version trim---
	ret = nvt_spi_check_chip_ver_trim(NVT_SPI_CHIP_VER_TRIM_ADDR);
	if (ret) {
		NVT_LOG("try to check from old chip ver trim address\n");
		ret = nvt_spi_check_chip_ver_trim(NVT_SPI_CHIP_VER_TRIM_OLD_ADDR);
		if (ret) {
			NVT_ERR("chip is not identified\n");
			ret = -EINVAL;
			return ret;
		}
	}

	if (nvt_spi_update_firmware(NVT_SPI_BOOT_UPDATE_FIRMWARE_NAME))
		NVT_ERR("download firmware failed, ignore check fw state\n");
	else
		nvt_spi_check_fw_reset_state(NVT_SPI_RESET_STATE_REK);

	//---set device node---
#if NVT_SPI_TOUCH_PROC
	ret = nvt_spi_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_SPI_TOUCH_EXT_PROC
	ret = nvt_spi_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

#if NVT_SPI_TOUCH_MP
	ret = nvt_spi_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}
#endif

	bTouchIsAwake = 1;
	NVT_LOG("end\n");

	nvt_spi_irq_enable(true);
	return 0;

#if NVT_SPI_TOUCH_MP
	nvt_spi_mp_proc_deinit();
err_mp_proc_init_failed:
#endif
#if NVT_SPI_TOUCH_EXT_PROC
	nvt_spi_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_SPI_TOUCH_PROC
	nvt_spi_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
	return 0;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen driver probe function.
 *
 * return:
 *	Executive outcomes. 0---succeed. negative---failed
 ******************************************************
 */
static int32_t nvt_spi_probe(struct spi_device *client)
{
	int32_t ret = 0;
#if defined(CONFIG_DRM)
	struct device_node *dp = NULL;
#endif
#if ((NVT_SPI_TOUCH_KEY_NUM > 0) || NVT_SPI_WAKEUP_GESTURE)
	int32_t retry = 0;
#endif
	struct nvt_spi_data_t *ts;

#if defined(CONFIG_DRM)
	dp = client->dev.of_node;

	ret = nvt_spi_check_dt(dp);
	if (ret == -EPROBE_DEFER)
		return ret;

	if (ret) {
		ret = -ENODEV;
		return ret;
	}
#endif

	NVT_LOG("start\n");

	ts = kzalloc(sizeof(struct nvt_spi_data_t), GFP_KERNEL);
	if (ts == NULL) {
		NVT_ERR("failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}
	nvt_spi_data = ts;

	ts->xbuf = kzalloc(NVT_SPI_XBUF_LEN, GFP_KERNEL);
	if (ts->xbuf == NULL) {
		NVT_ERR("kzalloc for xbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_xbuf;
	}

	ts->rbuf = kzalloc(NVT_SPI_READ_LEN, GFP_KERNEL);
	if (ts->rbuf == NULL) {
		NVT_ERR("kzalloc for rbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_rbuf;
	}

	ts->client = client;
	spi_set_drvdata(client, ts);

	//---prepare for spi parameter---
	if (ts->client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		NVT_ERR("Full duplex not supported by master\n");
		ret = -EIO;
		goto err_ckeck_full_duplex;
	}
	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;

	ret = spi_setup(ts->client);
	if (ret < 0) {
		NVT_ERR("Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

	NVT_LOG("mode=%d, max_speed_hz=%d\n", ts->client->mode, ts->client->max_speed_hz);

	//---parse dts---
	ret = nvt_spi_parse_dt(&client->dev);
	if (ret) {
		NVT_ERR("parse dt error\n");
		goto err_spi_setup;
	}

	//---request and config GPIOs---
	ret = nvt_spi_gpio_config(ts);
	if (ret) {
		NVT_ERR("gpio config error!\n");
		goto err_gpio_config_failed;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);

	//---eng reset before TP_RESX high
	nvt_spi_eng_reset();

#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	// need 10ms delay after POR(power on reset)
	msleep(20);

	ts->abs_x_max = NVT_SPI_TOUCH_DEFAULT_MAX_WIDTH;
	ts->abs_y_max = NVT_SPI_TOUCH_DEFAULT_MAX_HEIGHT;

	//---allocate input device---
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		NVT_ERR("allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->max_touch_num = NVT_SPI_TOUCH_MAX_FINGER_NUM;

#if NVT_SPI_TOUCH_KEY_NUM > 0
	ts->max_button_num = NVT_SPI_TOUCH_KEY_NUM;
#endif

	ts->int_trigger_type = NVT_SPI_INT_TRIGGER_TYPE;


	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if NVT_SPI_MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->max_touch_num, 0);
#endif

	// pressure = NVT_SPI_TOUCH_FORCE_NUM
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, NVT_SPI_TOUCH_FORCE_NUM, 0, 0);

#if NVT_SPI_TOUCH_MAX_FINGER_NUM > 1
	// area = 255
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
#if NVT_SPI_MT_PROTOCOL_B
	// no need to set ABS_MT_TRACKING_ID, input_mt_init_slots() already set it
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touch_num, 0, 0);
#endif //NVT_SPI_MT_PROTOCOL_B
#endif //NVT_SPI_TOUCH_MAX_FINGER_NUM > 1

#if NVT_SPI_TOUCH_KEY_NUM > 0
	for (retry = 0; retry < ts->max_button_num; retry++)
		input_set_capability(ts->input_dev, EV_KEY, nvt_spi_touch_key_array[retry]);
#endif

#if NVT_SPI_WAKEUP_GESTURE
	for (retry = 0; retry < ARRAY_SIZE(nvt_spi_gesture_key_array); retry++)
		input_set_capability(ts->input_dev, EV_KEY, nvt_spi_gesture_key_array[retry]);
#endif

	snprintf(ts->phys, sizeof(ts->phys), "input/ts");
	ts->input_dev->name = NVT_SPI_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_SPI;

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		NVT_ERR("register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

	if (ts->pen_support) {
		//---allocate pen input device---
		ts->pen_input_dev = input_allocate_device();
		if (ts->pen_input_dev == NULL) {
			NVT_ERR("allocate pen input device failed\n");
			ret = -ENOMEM;
			goto err_pen_input_dev_alloc_failed;
		}

		//---set pen input device info.---
		ts->pen_input_dev->evbit[0] =
				BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_TOOL_PEN)] |= BIT_MASK(BTN_TOOL_PEN);
		//ts->pen_input_dev->keybit[BIT_WORD(BTN_TOOL_RUBBER)]
		//		|= BIT_MASK(BTN_TOOL_RUBBER);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_STYLUS)] |= BIT_MASK(BTN_STYLUS);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_STYLUS2)] |= BIT_MASK(BTN_STYLUS2);
		ts->pen_input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

		if (ts->wgp_stylus) {
			input_set_abs_params(ts->pen_input_dev, ABS_X, 0,
					ts->abs_x_max * 2, 0, 0);
			input_set_abs_params(ts->pen_input_dev, ABS_Y, 0,
					ts->abs_y_max * 2, 0, 0);
		} else {
			input_set_abs_params(ts->pen_input_dev, ABS_X, 0,
					ts->abs_x_max, 0, 0);

			input_set_abs_params(ts->pen_input_dev, ABS_Y, 0,
					ts->abs_y_max, 0, 0);
		}
		input_set_abs_params(ts->pen_input_dev, ABS_PRESSURE, 0,
				NVT_SPI_PEN_PRESSURE_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_DISTANCE, 0,
				NVT_SPI_PEN_DISTANCE_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_TILT_X,
				NVT_SPI_PEN_TILT_MIN, NVT_SPI_PEN_TILT_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_TILT_Y,
				NVT_SPI_PEN_TILT_MIN, NVT_SPI_PEN_TILT_MAX, 0, 0);

		snprintf(ts->pen_phys, sizeof(ts->pen_phys), "input/pen");
		ts->pen_input_dev->name = NVT_SPI_PEN_NAME;
		ts->pen_input_dev->phys = ts->pen_phys;
		ts->pen_input_dev->id.bustype = BUS_SPI;

		//---register pen input device---
		ret = input_register_device(ts->pen_input_dev);
		if (ret) {
			NVT_ERR("register pen input device (%s) failed. ret=%d\n",
					ts->pen_input_dev->name, ret);
			goto err_pen_input_register_device_failed;
		}
	} /* if (ts->pen_support) */

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(ts->irq_gpio);
	if (client->irq) {
		NVT_LOG("int_trigger_type=%d\n", ts->int_trigger_type);
		ts->irq_enabled = true;
		ret = request_threaded_irq(client->irq, NULL, nvt_spi_work_func,
				ts->int_trigger_type | IRQF_ONESHOT, NVT_SPI_NAME, ts);
		if (ret != 0) {
			NVT_ERR("request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_spi_irq_enable(false);
			NVT_LOG("request irq %d succeed\n", client->irq);
		}
	}

#if NVT_SPI_WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 1);
#endif

#if NVT_SPI_BOOT_UPDATE_FIRMWARE
	nvt_spi_fwu_wq = alloc_workqueue("nvt_spi_fwu_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!nvt_spi_fwu_wq) {
		NVT_ERR("nvt_spi_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_spi_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, nvt_spi_update_firmware_work);

	// please make sure boot update start after display reset(RESX) sequence
	//queue_delayed_work(nvt_spi_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(14000));
#endif

	NVT_LOG("NVT_SPI_TOUCH_ESD_PROTECT is %d\n", NVT_SPI_TOUCH_ESD_PROTECT);
#if NVT_SPI_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_spi_esd_check_work, nvt_spi_esd_check_func);
	nvt_spi_esd_check_wq = alloc_workqueue("nvt_spi_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!nvt_spi_esd_check_wq) {
		NVT_ERR("nvt_spi_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_spi_esd_check_wq_failed;
	}
	queue_delayed_work(nvt_spi_esd_check_wq, &nvt_spi_esd_check_work,
			msecs_to_jiffies(NVT_SPI_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */


#if defined(CONFIG_DRM)
	//if (!strcmp(ts->touch_environment, "pvm"))
	nvt_spi_register_for_panel_events(client->dev.of_node, ts);
#elif defined(_MSM_DRM_NOTIFY_H_)
	ts->drm_notif.notifier_call = nvt_drm_notifier_callback;
	ret = msm_drm_register_client(&ts->drm_notif);
	if (ret) {
		NVT_ERR("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#else
	ts->fb_notif.notifier_call = nvt_fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if (ret) {
		NVT_ERR("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#endif

	NVT_LOG("end\n");
	return 0;

#if defined(CONFIG_DRM)

#elif defined(_MSM_DRM_NOTIFY_H_)
err_register_drm_notif_failed:
#else
err_register_fb_notif_failed:
#endif

#if NVT_SPI_TOUCH_MP
	nvt_spi_mp_proc_deinit();
#endif
#if NVT_SPI_TOUCH_EXT_PROC
	nvt_spi_extra_proc_deinit();

#endif
#if NVT_SPI_TOUCH_PROC
	nvt_spi_flash_proc_deinit();

#endif
#if NVT_SPI_TOUCH_ESD_PROTECT
	if (nvt_spi_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_spi_esd_check_work);
		destroy_workqueue(nvt_spi_esd_check_wq);
		nvt_spi_esd_check_wq = NULL;
	}
err_create_nvt_spi_esd_check_wq_failed:
#endif
#if NVT_SPI_BOOT_UPDATE_FIRMWARE
	if (nvt_spi_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_spi_fwu_wq);
		nvt_spi_fwu_wq = NULL;
	}
err_create_nvt_spi_fwu_wq_failed:
#endif
#if NVT_SPI_WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif
	free_irq(client->irq, ts);
err_int_request_failed:
	if (ts->pen_support) {
		input_unregister_device(ts->pen_input_dev);
		ts->pen_input_dev = NULL;
	}
err_pen_input_register_device_failed:
	if (ts->pen_support) {
		if (ts->pen_input_dev) {
			input_free_device(ts->pen_input_dev);
			ts->pen_input_dev = NULL;
		}
	}
err_pen_input_dev_alloc_failed:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device_failed:
	if (ts->input_dev) {
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
	}
err_input_dev_alloc_failed:
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);
	nvt_spi_gpio_deconfig(ts);
err_gpio_config_failed:
err_spi_setup:
err_ckeck_full_duplex:
	spi_set_drvdata(client, NULL);
	kfree(ts->rbuf);
	ts->rbuf = NULL;
err_malloc_rbuf:
	kfree(ts->xbuf);
	ts->xbuf = NULL;
err_malloc_xbuf:
	kfree(ts);
	ts = NULL;

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen driver release function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t nvt_spi_remove(struct spi_device *client)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	NVT_LOG("Removing driver...\n");

#if defined(CONFIG_DRM)
	if (active_spi_panel && ts->notifier_cookie)
		panel_event_notifier_unregister(ts->notifier_cookie);
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#endif

#if NVT_SPI_TOUCH_MP
	nvt_spi_mp_proc_deinit();
#endif
#if NVT_SPI_TOUCH_EXT_PROC
	nvt_spi_extra_proc_deinit();
#endif
#if NVT_SPI_TOUCH_PROC
	nvt_spi_flash_proc_deinit();
#endif

#if NVT_SPI_TOUCH_ESD_PROTECT
	if (nvt_spi_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_spi_esd_check_work);
		nvt_spi_esd_check_enable(false);
		destroy_workqueue(nvt_spi_esd_check_wq);
		nvt_spi_esd_check_wq = NULL;
	}
#endif

#if NVT_SPI_BOOT_UPDATE_FIRMWARE
	if (nvt_spi_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_spi_fwu_wq);
		nvt_spi_fwu_wq = NULL;
	}
#endif

#if NVT_SPI_WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif

	nvt_spi_irq_enable(false);
	free_irq(client->irq, ts);

	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	nvt_spi_gpio_deconfig(ts);

	if (ts->pen_support) {
		if (ts->pen_input_dev) {
			input_unregister_device(ts->pen_input_dev);
			ts->pen_input_dev = NULL;
		}
	}

	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}

	spi_set_drvdata(client, NULL);

	kfree(ts->xbuf);
	ts->xbuf = NULL;

	kfree(ts);
	ts = NULL;

	return 0;
}

static void nvt_spi_shutdown(struct spi_device *client)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	NVT_LOG("Shutdown driver...\n");

	nvt_spi_irq_enable(false);

#if defined(CONFIG_DRM)
	if (active_spi_panel && ts->notifier_cookie)
		panel_event_notifier_unregister(ts->notifier_cookie);
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#endif

#if NVT_SPI_TOUCH_MP
	nvt_spi_mp_proc_deinit();
#endif
#if NVT_SPI_TOUCH_EXT_PROC
	nvt_spi_extra_proc_deinit();
#endif
#if NVT_SPI_TOUCH_PROC
	nvt_spi_flash_proc_deinit();
#endif

#if NVT_SPI_TOUCH_ESD_PROTECT
	if (nvt_spi_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_spi_esd_check_work);
		nvt_spi_esd_check_enable(false);
		destroy_workqueue(nvt_spi_esd_check_wq);
		nvt_spi_esd_check_wq = NULL;
	}
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

#if NVT_SPI_BOOT_UPDATE_FIRMWARE
	if (nvt_spi_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_spi_fwu_wq);
		nvt_spi_fwu_wq = NULL;
	}
#endif

#if NVT_SPI_WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen driver suspend function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t nvt_spi_suspend(struct device *dev)
{
	uint8_t buf[4] = {0};
#if NVT_SPI_MT_PROTOCOL_B
	uint32_t i = 0;
#endif
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (!bTouchIsAwake) {
		NVT_LOG("Touch is already suspend\n");
		return 0;
	}

#if !NVT_SPI_WAKEUP_GESTURE
	nvt_spi_irq_enable(false);
#endif

#if NVT_SPI_TOUCH_ESD_PROTECT
	NVT_LOG("cancel delayed work sync\n");
	cancel_delayed_work_sync(&nvt_spi_esd_check_work);
	nvt_spi_esd_check_enable(false);
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	bTouchIsAwake = 0;

#if NVT_SPI_WAKEUP_GESTURE
	//---write command to enter "wakeup gesture mode"---
	buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
	buf[1] = 0x13;
	nvt_spi_write(buf, 2);

	enable_irq_wake(ts->client->irq);

	NVT_LOG("Enabled touch wakeup gesture\n");

#else // NVT_SPI_WAKEUP_GESTURE
	//---write command to enter "deep sleep mode"---
	buf[0] = NVT_SPI_EVENT_MAP_HOST_CMD;
	buf[1] = 0x11;
	nvt_spi_write(buf, 2);
#endif // NVT_SPI_WAKEUP_GESTURE

	mutex_unlock(&ts->lock);

	/* release all touches */
#if NVT_SPI_MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !NVT_SPI_MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	/* release pen event */
	if (ts->pen_support) {
		input_report_abs(ts->pen_input_dev, ABS_X, 0);
		input_report_abs(ts->pen_input_dev, ABS_Y, 0);
		input_report_abs(ts->pen_input_dev, ABS_PRESSURE, 0);
		input_report_abs(ts->pen_input_dev, ABS_TILT_X, 0);
		input_report_abs(ts->pen_input_dev, ABS_TILT_Y, 0);
		input_report_abs(ts->pen_input_dev, ABS_DISTANCE, 0);
		input_report_key(ts->pen_input_dev, BTN_TOUCH, 0);
		input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, 0);
		input_report_key(ts->pen_input_dev, BTN_STYLUS, 0);
		input_report_key(ts->pen_input_dev, BTN_STYLUS2, 0);
		input_sync(ts->pen_input_dev);
	}

	msleep(50);

	NVT_LOG("end\n");

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen driver resume function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************
 */
static int32_t nvt_spi_resume(struct device *dev)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	if (bTouchIsAwake || ts->fw_ver == 0) {
		nvt_spi_late_probe(ts->client);
		NVT_LOG("nvt_spi_late_probe\n");
		return 0;
	}

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_SPI_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	if (nvt_spi_update_firmware(NVT_SPI_BOOT_UPDATE_FIRMWARE_NAME))
		NVT_ERR("download firmware failed, ignore check fw state\n");
	else
		nvt_spi_check_fw_reset_state(NVT_SPI_RESET_STATE_REK);

#if !NVT_SPI_WAKEUP_GESTURE
	nvt_spi_irq_enable(true);
#endif

#if NVT_SPI_TOUCH_ESD_PROTECT
	nvt_spi_esd_check_enable(false);
	queue_delayed_work(nvt_spi_esd_check_wq, &nvt_spi_esd_check_work,
			msecs_to_jiffies(NVT_SPI_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_SPI_TOUCH_ESD_PROTECT */

	bTouchIsAwake = 1;

	mutex_unlock(&ts->lock);

	NVT_LOG("end\n");

	return 0;
}

#if defined(CONFIG_DRM)

static void nvt_spi_panel_notifier_callback(enum panel_event_notifier_tag tag,
		 struct panel_event_notification *notification, void *client_data)
{
	struct nvt_spi_data_t *ts = client_data;

	if (!notification) {
		pr_err("Invalid notification\n");
		return;
	}

	NVT_LOG("Notification type:%d, early_trigger:%d",
			notification->notif_type,
			notification->notif_data.early_trigger);

	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_UNBLANK:
		if (notification->notif_data.early_trigger)
			NVT_LOG("resume notification pre commit\n");
		else
			nvt_spi_resume(&ts->client->dev);
		break;

	case DRM_PANEL_EVENT_BLANK:
		if (notification->notif_data.early_trigger)
			nvt_spi_suspend(&ts->client->dev);
		else
			NVT_LOG("suspend notification post commit\n");
		break;

	case DRM_PANEL_EVENT_BLANK_LP:
		NVT_LOG("received lp event\n");
		break;

	case DRM_PANEL_EVENT_FPS_CHANGE:
		NVT_LOG("shashank:Received fps change old fps:%d new fps:%d\n",
				notification->notif_data.old_fps,
				notification->notif_data.new_fps);
		break;
	default:
		NVT_LOG("notification serviced :%d\n",
				notification->notif_type);
		break;
	}
}

#elif defined(_MSM_DRM_NOTIFY_H_)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct nvt_spi_data_t *ts =
		container_of(self, struct nvt_spi_data_t, drm_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (evdata->data && ts) {
		blank = evdata->data;
		if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_spi_suspend(&ts->client->dev);
			}
		} else if (event == MSM_DRM_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_resume(&ts->client->dev);
			}
		}
	}

	return 0;
}
#else
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_spi_data_t *ts =
		container_of(self, struct nvt_spi_data_t, fb_notif);

	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_spi_suspend(&ts->client->dev);
		}
	} else if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_resume(&ts->client->dev);
		}
	}

	return 0;
}
#endif

static const struct spi_device_id nvt_spi_id[] = {
	{ NVT_SPI_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id nvt_spi_match_table[] = {
	{ .compatible = "novatek,NVT-ts",},
	{ },
};
#endif

static struct spi_driver nvt_spi_driver = {
	.probe		= nvt_spi_probe,
	.remove		= nvt_spi_remove,
	.shutdown	= nvt_spi_shutdown,
	.id_table	= nvt_spi_id,
	.driver = {
		.name	= NVT_SPI_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nvt_spi_match_table,
#endif
	},
};

/*
 ******************************************************
 * Description:
 *	Driver Install function.
 *
 * return:
 *	Executive Outcomes. 0---succeed. not 0---failed.
 *******************************************************
 */
static int32_t __init nvt_spi_driver_init(void)
{
	int32_t ret = 0;

	NVT_LOG("start\n");

	//---add spi driver---
	ret = spi_register_driver(&nvt_spi_driver);
	if (ret) {
		NVT_ERR("failed to add spi driver");
		goto err_driver;
	}

	NVT_LOG("finished\n");

err_driver:
	return ret;
}

/*
 ******************************************************
 * Description:
 *	Driver uninstall function.
 *
 * return:
 *	n.a.
 *******************************************************
 */
static void __exit nvt_spi_driver_exit(void)
{
	spi_unregister_driver(&nvt_spi_driver);
}

module_init(nvt_spi_driver_init);
module_exit(nvt_spi_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL v2");

#endif

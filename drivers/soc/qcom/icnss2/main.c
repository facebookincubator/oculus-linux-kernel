// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "icnss2: " fmt

#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>
#include <linux/adc-tm-clients.h>
#include <linux/iio/consumer.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/sysfs.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/ramdump.h>
#include "main.h"
#include "qmi.h"
#include "debug.h"
#include "power.h"
#include "genl.h"

#define MAX_PROP_SIZE			32
#define NUM_LOG_PAGES			10
#define NUM_LOG_LONG_PAGES		4
#define ICNSS_MAGIC			0x5abc5abc

#define ICNSS_SERVICE_LOCATION_CLIENT_NAME			"ICNSS-WLAN"
#define ICNSS_WLAN_SERVICE_NAME					"wlan/fw"
#define ICNSS_DEFAULT_FEATURE_MASK 0x01

#define ICNSS_QUIRKS_DEFAULT		BIT(FW_REJUVENATE_ENABLE)
#define ICNSS_MAX_PROBE_CNT		2

#define ICNSS_BDF_TYPE_DEFAULT         ICNSS_BDF_ELF

#define PROBE_TIMEOUT                 15000
#define WLFW_TIMEOUT			msecs_to_jiffies(3000)

static struct icnss_priv *penv;

uint64_t dynamic_feature_mask = ICNSS_DEFAULT_FEATURE_MASK;

#define ICNSS_EVENT_PENDING			2989

#define ICNSS_EVENT_SYNC			BIT(0)
#define ICNSS_EVENT_UNINTERRUPTIBLE		BIT(1)
#define ICNSS_EVENT_SYNC_UNINTERRUPTIBLE	(ICNSS_EVENT_UNINTERRUPTIBLE | \
						 ICNSS_EVENT_SYNC)


enum icnss_pdr_cause_index {
	ICNSS_FW_CRASH,
	ICNSS_ROOT_PD_CRASH,
	ICNSS_ROOT_PD_SHUTDOWN,
	ICNSS_HOST_ERROR,
};

static const char * const icnss_pdr_cause[] = {
	[ICNSS_FW_CRASH] = "FW crash",
	[ICNSS_ROOT_PD_CRASH] = "Root PD crashed",
	[ICNSS_ROOT_PD_SHUTDOWN] = "Root PD shutdown",
	[ICNSS_HOST_ERROR] = "Host error",
};

static void icnss_set_plat_priv(struct icnss_priv *priv)
{
	penv = priv;
}

static struct icnss_priv *icnss_get_plat_priv()
{
	return penv;
}

static ssize_t icnss_sysfs_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	struct icnss_priv *priv = icnss_get_plat_priv();

	atomic_set(&priv->is_shutdown, true);
	icnss_pr_dbg("Received shutdown indication");
	return count;
}

static struct kobj_attribute icnss_sysfs_attribute =
__ATTR(shutdown, 0660, NULL, icnss_sysfs_store);

static void icnss_pm_stay_awake(struct icnss_priv *priv)
{
	if (atomic_inc_return(&priv->pm_count) != 1)
		return;

	icnss_pr_vdbg("PM stay awake, state: 0x%lx, count: %d\n", priv->state,
		     atomic_read(&priv->pm_count));

	pm_stay_awake(&priv->pdev->dev);

	priv->stats.pm_stay_awake++;
}

static void icnss_pm_relax(struct icnss_priv *priv)
{
	int r = atomic_dec_return(&priv->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	icnss_pr_vdbg("PM relax, state: 0x%lx, count: %d\n", priv->state,
		     atomic_read(&priv->pm_count));

	pm_relax(&priv->pdev->dev);
	priv->stats.pm_relax++;
}

char *icnss_driver_event_to_str(enum icnss_driver_event_type type)
{
	switch (type) {
	case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case ICNSS_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case ICNSS_DRIVER_EVENT_FW_READY_IND:
		return "FW_READY";
	case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
		return "REGISTER_DRIVER";
	case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
		return "UNREGISTER_DRIVER";
	case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
		return "PD_SERVICE_DOWN";
	case ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND:
		return "FW_EARLY_CRASH_IND";
	case ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
		return "IDLE_SHUTDOWN";
	case ICNSS_DRIVER_EVENT_IDLE_RESTART:
		return "IDLE_RESTART";
	case ICNSS_DRIVER_EVENT_FW_INIT_DONE_IND:
		return "FW_INIT_DONE";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
		return "QDSS_TRACE_REQ_MEM";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
		return "QDSS_TRACE_SAVE";
	case ICNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
		return "QDSS_TRACE_FREE";
	case ICNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

char *icnss_soc_wake_event_to_str(enum icnss_soc_wake_event_type type)
{
	switch (type) {
	case ICNSS_SOC_WAKE_REQUEST_EVENT:
		return "SOC_WAKE_REQUEST";
	case ICNSS_SOC_WAKE_RELEASE_EVENT:
		return "SOC_WAKE_RELEASE";
	case ICNSS_SOC_WAKE_EVENT_MAX:
		return "SOC_EVENT_MAX";
	}

	return "UNKNOWN";
};

int icnss_driver_event_post(struct icnss_priv *priv,
			    enum icnss_driver_event_type type,
			    u32 flags, void *data)
{
	struct icnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Posting event: %s(%d), %s, flags: 0x%x, state: 0x%lx\n",
		     icnss_driver_event_to_str(type), type, current->comm,
		     flags, priv->state);

	if (type >= ICNSS_DRIVER_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (event == NULL)
		return -ENOMEM;

	icnss_pm_stay_awake(priv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = ICNSS_EVENT_PENDING;
	event->sync = !!(flags & ICNSS_EVENT_SYNC);

	spin_lock_irqsave(&priv->event_lock, irq_flags);
	list_add_tail(&event->list, &priv->event_list);
	spin_unlock_irqrestore(&priv->event_lock, irq_flags);

	priv->stats.events[type].posted++;
	queue_work(priv->event_wq, &priv->event_work);

	if (!(flags & ICNSS_EVENT_SYNC))
		goto out;

	if (flags & ICNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	icnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		     icnss_driver_event_to_str(type), type, priv->state, ret,
		     event->ret);

	spin_lock_irqsave(&priv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == ICNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&priv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&priv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	icnss_pm_relax(priv);
	return ret;
}

int icnss_soc_wake_event_post(struct icnss_priv *priv,
			      enum icnss_soc_wake_event_type type,
			      u32 flags, void *data)
{
	struct icnss_soc_wake_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!priv)
		return -ENODEV;

	icnss_pr_dbg("Posting event: %s(%d), %s, flags: 0x%x, state: 0x%lx\n",
		     icnss_soc_wake_event_to_str(type), type, current->comm,
		     flags, priv->state);

	if (type >= ICNSS_SOC_WAKE_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (!event)
		return -ENOMEM;

	icnss_pm_stay_awake(priv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = ICNSS_EVENT_PENDING;
	event->sync = !!(flags & ICNSS_EVENT_SYNC);

	spin_lock_irqsave(&priv->soc_wake_msg_lock, irq_flags);
	list_add_tail(&event->list, &priv->soc_wake_msg_list);
	spin_unlock_irqrestore(&priv->soc_wake_msg_lock, irq_flags);

	priv->stats.soc_wake_events[type].posted++;
	queue_work(priv->soc_wake_wq, &priv->soc_wake_msg_work);

	if (!(flags & ICNSS_EVENT_SYNC))
		goto out;

	if (flags & ICNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	icnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		     icnss_soc_wake_event_to_str(type), type, priv->state, ret,
		     event->ret);

	spin_lock_irqsave(&priv->soc_wake_msg_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == ICNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&priv->soc_wake_msg_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&priv->soc_wake_msg_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	icnss_pm_relax(priv);
	return ret;
}

bool icnss_is_fw_ready(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_FW_READY, &penv->state);
}
EXPORT_SYMBOL(icnss_is_fw_ready);

void icnss_block_shutdown(bool status)
{
	if (!penv)
		return;

	if (status) {
		set_bit(ICNSS_BLOCK_SHUTDOWN, &penv->state);
		reinit_completion(&penv->unblock_shutdown);
	} else {
		clear_bit(ICNSS_BLOCK_SHUTDOWN, &penv->state);
		complete(&penv->unblock_shutdown);
	}
}
EXPORT_SYMBOL(icnss_block_shutdown);

bool icnss_is_fw_down(void)
{

	struct icnss_priv *priv = icnss_get_plat_priv();

	if (!priv)
		return false;

	return test_bit(ICNSS_FW_DOWN, &priv->state) ||
		test_bit(ICNSS_PD_RESTART, &priv->state) ||
		test_bit(ICNSS_REJUVENATE, &priv->state);
}
EXPORT_SYMBOL(icnss_is_fw_down);

bool icnss_is_rejuvenate(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_REJUVENATE, &penv->state);
}
EXPORT_SYMBOL(icnss_is_rejuvenate);

bool icnss_is_pdr(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_PDR, &penv->state);
}
EXPORT_SYMBOL(icnss_is_pdr);

static irqreturn_t fw_error_fatal_handler(int irq, void *ctx)
{
	struct icnss_priv *priv = ctx;

	if (priv)
		priv->force_err_fatal = true;

	icnss_pr_err("Received force error fatal request from FW\n");

	return IRQ_HANDLED;
}

static irqreturn_t fw_crash_indication_handler(int irq, void *ctx)
{
	struct icnss_priv *priv = ctx;
	struct icnss_uevent_fw_down_data fw_down_data = {0};

	icnss_pr_err("Received early crash indication from FW\n");

	if (priv) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		if (test_bit(ICNSS_FW_READY, &priv->state)) {
			fw_down_data.crashed = true;
			icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		}
	}

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND,
				0, NULL);

	return IRQ_HANDLED;
}

static void register_fw_error_notifications(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct device_node *dev_node;
	int irq = 0, ret = 0;

	if (!priv)
		return;

	dev_node = of_find_node_by_name(NULL, "qcom,smp2p_map_wlan_1_in");
	if (!dev_node) {
		icnss_pr_err("Failed to get smp2p node for force-fatal-error\n");
		return;
	}

	icnss_pr_dbg("smp2p node->name=%s\n", dev_node->name);

	if (strcmp("qcom,smp2p_map_wlan_1_in", dev_node->name) == 0) {
		ret = irq = of_irq_get_byname(dev_node,
					      "qcom,smp2p-force-fatal-error");
		if (ret < 0) {
			icnss_pr_err("Unable to get force-fatal-error irq %d\n",
				     irq);
			return;
		}
	}

	ret = devm_request_threaded_irq(dev, irq, NULL, fw_error_fatal_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					"wlanfw-err", priv);
	if (ret < 0) {
		icnss_pr_err("Unable to register for error fatal IRQ handler %d ret = %d",
			     irq, ret);
		return;
	}
	icnss_pr_dbg("FW force error fatal handler registered irq = %d\n", irq);
	priv->fw_error_fatal_irq = irq;
}

static void register_early_crash_notifications(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct device_node *dev_node;
	int irq = 0, ret = 0;

	if (!priv)
		return;

	dev_node = of_find_node_by_name(NULL, "qcom,smp2p_map_wlan_1_in");
	if (!dev_node) {
		icnss_pr_err("Failed to get smp2p node for early-crash-ind\n");
		return;
	}

	icnss_pr_dbg("smp2p node->name=%s\n", dev_node->name);

	if (strcmp("qcom,smp2p_map_wlan_1_in", dev_node->name) == 0) {
		ret = irq = of_irq_get_byname(dev_node,
					      "qcom,smp2p-early-crash-ind");
		if (ret < 0) {
			icnss_pr_err("Unable to get early-crash-ind irq %d\n",
				     irq);
			return;
		}
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,
					fw_crash_indication_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					"wlanfw-early-crash-ind", priv);
	if (ret < 0) {
		icnss_pr_err("Unable to register for early crash indication IRQ handler %d ret = %d",
			     irq, ret);
		return;
	}
	icnss_pr_dbg("FW crash indication handler registered irq = %d\n", irq);
	priv->fw_early_crash_irq = irq;
}

int icnss_call_driver_uevent(struct icnss_priv *priv,
				    enum icnss_uevent uevent, void *data)
{
	struct icnss_uevent_data uevent_data;

	if (!priv->ops || !priv->ops->uevent)
		return 0;

	icnss_pr_dbg("Calling driver uevent state: 0x%lx, uevent: %d\n",
		     priv->state, uevent);

	uevent_data.uevent = uevent;
	uevent_data.data = data;

	return priv->ops->uevent(&priv->pdev->dev, &uevent_data);
}

static int icnss_driver_event_server_arrive(struct icnss_priv *priv,
						 void *data)
{
	int ret = 0;
	bool ignore_assert = false;

	if (!priv)
		return -ENODEV;

	set_bit(ICNSS_WLFW_EXISTS, &priv->state);
	clear_bit(ICNSS_FW_DOWN, &priv->state);
	clear_bit(ICNSS_FW_READY, &priv->state);

	icnss_ignore_fw_timeout(false);

	if (test_bit(ICNSS_WLFW_CONNECTED, &penv->state)) {
		icnss_pr_err("QMI Server already in Connected State\n");
		ICNSS_ASSERT(0);
	}

	ret = icnss_connect_to_fw_server(priv, data);
	if (ret)
		goto fail;

	set_bit(ICNSS_WLFW_CONNECTED, &priv->state);

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto clear_server;

	ret = wlfw_ind_register_send_sync_msg(priv);
	if (ret < 0) {
		if (ret == -EALREADY) {
			ret = 0;
			goto qmi_registered;
		}
		ignore_assert = true;
		goto err_power_on;
	}

	if (priv->device_id == WCN6750_DEVICE_ID) {
		ret = wlfw_host_cap_send_sync(priv);
		if (ret < 0)
			goto err_power_on;
	}

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		if (!priv->msa_va) {
			icnss_pr_err("Invalid MSA address\n");
			ret = -EINVAL;
			goto err_power_on;
		}

		ret = wlfw_msa_mem_info_send_sync_msg(priv);
		if (ret < 0) {
			ignore_assert = true;
			goto err_power_on;
		}

		ret = wlfw_msa_ready_send_sync_msg(priv);
		if (ret < 0) {
			ignore_assert = true;
			goto err_power_on;
		}
	}

	ret = wlfw_cap_send_sync_msg(priv);
	if (ret < 0) {
		ignore_assert = true;
		goto err_power_on;
	}

	if (priv->device_id == WCN6750_DEVICE_ID) {
		ret = wlfw_device_info_send_msg(priv);
		if (ret < 0) {
			ignore_assert = true;
			goto err_power_on;
		}

		priv->mem_base_va = devm_ioremap(&priv->pdev->dev,
							 priv->mem_base_pa,
							 priv->mem_base_size);
		if (!priv->mem_base_va) {
			icnss_pr_err("Ioremap failed for bar address\n");
			goto err_power_on;
		}

		icnss_pr_dbg("MEM_BASE pa: %pa, va: 0x%pK\n",
			     &priv->mem_base_pa,
			     priv->mem_base_va);

		icnss_wlfw_bdf_dnld_send_sync(priv, ICNSS_BDF_REGDB);

		ret = icnss_wlfw_bdf_dnld_send_sync(priv,
						    priv->ctrl_params.bdf_type);

	}

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		wlfw_dynamic_feature_mask_send_sync_msg(priv,
							dynamic_feature_mask);
	}

	if (!priv->fw_error_fatal_irq)
		register_fw_error_notifications(&priv->pdev->dev);

	if (!priv->fw_early_crash_irq)
		register_early_crash_notifications(&priv->pdev->dev);

	if (priv->vbatt_supported)
		icnss_init_vph_monitor(priv);

	return ret;

err_power_on:
	icnss_hw_power_off(priv);
clear_server:
	icnss_clear_server(priv);
fail:
	ICNSS_ASSERT(ignore_assert);
qmi_registered:
	return ret;
}

static int icnss_driver_event_server_exit(struct icnss_priv *priv)
{
	if (!priv)
		return -ENODEV;

	icnss_pr_info("WLAN FW Service Disconnected: 0x%lx\n", priv->state);

	icnss_clear_server(priv);

	if (priv->adc_tm_dev && priv->vbatt_supported)
		adc_tm5_disable_chan_meas(priv->adc_tm_dev,
					  &priv->vph_monitor_params);

	return 0;
}

static int icnss_call_driver_probe(struct icnss_priv *priv)
{
	int ret = 0;
	int probe_cnt = 0;

	if (!priv->ops || !priv->ops->probe)
		return 0;

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		return -EINVAL;

	icnss_pr_dbg("Calling driver probe state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	icnss_block_shutdown(true);
	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = priv->ops->probe(&priv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret < 0) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, priv->state, probe_cnt);
		icnss_block_shutdown(false);
		goto out;
	}

	icnss_block_shutdown(false);
	set_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;

out:
	icnss_hw_power_off(priv);
	return ret;
}

static int icnss_call_driver_shutdown(struct icnss_priv *priv)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	if (!priv->ops || !priv->ops->shutdown)
		goto out;

	if (test_bit(ICNSS_SHUTDOWN_DONE, &priv->state))
		goto out;

	icnss_pr_dbg("Calling driver shutdown state: 0x%lx\n", priv->state);

	priv->ops->shutdown(&priv->pdev->dev);
	set_bit(ICNSS_SHUTDOWN_DONE, &priv->state);

out:
	return 0;
}

static int icnss_pd_restart_complete(struct icnss_priv *priv)
{
	int ret;

	icnss_pm_relax(priv);

	icnss_call_driver_shutdown(priv);

	clear_bit(ICNSS_PDR, &priv->state);
	clear_bit(ICNSS_REJUVENATE, &priv->state);
	clear_bit(ICNSS_PD_RESTART, &priv->state);
	priv->early_crash_ind = false;
	priv->is_ssr = false;

	if (!priv->ops || !priv->ops->reinit)
		goto out;

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     priv->state);
		goto out;
	}

	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto call_probe;

	icnss_pr_dbg("Calling driver reinit state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	icnss_block_shutdown(true);

	ret = priv->ops->reinit(&priv->pdev->dev);
	if (ret < 0) {
		icnss_fatal_err("Driver reinit failed: %d, state: 0x%lx\n",
				ret, priv->state);
		if (!priv->allow_recursive_recovery)
			ICNSS_ASSERT(false);
		icnss_block_shutdown(false);
		goto out_power_off;
	}

out:
	icnss_block_shutdown(false);
	clear_bit(ICNSS_SHUTDOWN_DONE, &priv->state);
	return 0;

call_probe:
	return icnss_call_driver_probe(priv);

out_power_off:
	icnss_hw_power_off(priv);

	return ret;
}


static int icnss_driver_event_fw_ready_ind(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	set_bit(ICNSS_FW_READY, &priv->state);
	clear_bit(ICNSS_MODE_ON, &priv->state);

	icnss_pr_info("WLAN FW is ready: 0x%lx\n", priv->state);

	icnss_hw_power_off(priv);

	if (!priv->pdev) {
		icnss_pr_err("Device is not ready\n");
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state))
		ret = icnss_pd_restart_complete(priv);
	else
		ret = icnss_call_driver_probe(priv);

out:
	return ret;
}

static int icnss_driver_event_fw_init_done(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	icnss_pr_info("WLAN FW Initialization done: 0x%lx\n", priv->state);

	ret = wlfw_wlan_mode_send_sync_msg(priv,
			(enum wlfw_driver_mode_enum_v01)ICNSS_CALIBRATION);

	return ret;
}

int icnss_alloc_qdss_mem(struct icnss_priv *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int i, j;

	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
		if (!qdss_mem[i].va && qdss_mem[i].size) {
			qdss_mem[i].va =
				dma_alloc_coherent(&pdev->dev,
						   qdss_mem[i].size,
						   &qdss_mem[i].pa,
						   GFP_KERNEL);
			if (!qdss_mem[i].va) {
				icnss_pr_err("Failed to allocate QDSS memory for FW, size: 0x%zx, type: %u, chuck-ID: %d\n",
					     qdss_mem[i].size,
					     qdss_mem[i].type, i);
				break;
			}
		}
	}

	/* Best-effort allocation for QDSS trace */
	if (i < priv->qdss_mem_seg_len) {
		for (j = i; j < priv->qdss_mem_seg_len; j++) {
			qdss_mem[j].type = 0;
			qdss_mem[j].size = 0;
		}
		priv->qdss_mem_seg_len = i;
	}

	return 0;
}

void icnss_free_qdss_mem(struct icnss_priv *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int i;

	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
		if (qdss_mem[i].va && qdss_mem[i].size) {
			icnss_pr_dbg("Freeing memory for QDSS: pa: %pa, size: 0x%zx, type: %u\n",
				     &qdss_mem[i].pa, qdss_mem[i].size,
				     qdss_mem[i].type);
			dma_free_coherent(&pdev->dev,
					  qdss_mem[i].size, qdss_mem[i].va,
					  qdss_mem[i].pa);
			qdss_mem[i].va = NULL;
			qdss_mem[i].pa = 0;
			qdss_mem[i].size = 0;
			qdss_mem[i].type = 0;
		}
	}
	priv->qdss_mem_seg_len = 0;
}

static int icnss_qdss_trace_req_mem_hdlr(struct icnss_priv *priv)
{
	int ret = 0;

	ret = icnss_alloc_qdss_mem(priv);
	if (ret < 0)
		return ret;

	return wlfw_qdss_trace_mem_info_send_sync(priv);
}

static void *icnss_qdss_trace_pa_to_va(struct icnss_priv *priv,
				       u64 pa, u32 size, int *seg_id)
{
	int i = 0;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	u64 offset = 0;
	void *va = NULL;
	u64 local_pa;
	u32 local_size;

	for (i = 0; i < priv->qdss_mem_seg_len; i++) {
		local_pa = (u64)qdss_mem[i].pa;
		local_size = (u32)qdss_mem[i].size;
		if (pa == local_pa && size <= local_size) {
			va = qdss_mem[i].va;
			break;
		}
		if (pa > local_pa &&
		    pa < local_pa + local_size &&
		    pa + size <= local_pa + local_size) {
			offset = pa - local_pa;
			va = qdss_mem[i].va + offset;
			break;
		}
	}

	*seg_id = i;
	return va;
}

static int icnss_qdss_trace_save_hdlr(struct icnss_priv *priv,
				      void *data)
{
	struct icnss_qmi_event_qdss_trace_save_data *event_data = data;
	struct icnss_fw_mem *qdss_mem = priv->qdss_mem;
	int ret = 0;
	int i;
	void *va = NULL;
	u64 pa;
	u32 size;
	int seg_id = 0;

	if (!priv->qdss_mem_seg_len) {
		icnss_pr_err("Memory for QDSS trace is not available\n");
		return -ENOMEM;
	}

	if (event_data->mem_seg_len == 0) {
		for (i = 0; i < priv->qdss_mem_seg_len; i++) {
			ret = icnss_genl_send_msg(qdss_mem[i].va,
						  ICNSS_GENL_MSG_TYPE_QDSS,
						  event_data->file_name,
						  qdss_mem[i].size);
			if (ret < 0) {
				icnss_pr_err("Fail to save QDSS data: %d\n",
					     ret);
				break;
			}
		}
	} else {
		for (i = 0; i < event_data->mem_seg_len; i++) {
			pa = event_data->mem_seg[i].addr;
			size = event_data->mem_seg[i].size;
			va = icnss_qdss_trace_pa_to_va(priv, pa,
						       size, &seg_id);
			if (!va) {
				icnss_pr_err("Fail to find matching va for pa %pa\n",
					     &pa);
				ret = -EINVAL;
				break;
			}
			ret = icnss_genl_send_msg(va, ICNSS_GENL_MSG_TYPE_QDSS,
						  event_data->file_name, size);
			if (ret < 0) {
				icnss_pr_err("Fail to save QDSS data: %d\n",
					     ret);
				break;
			}
		}
	}

	kfree(data);
	return ret;
}

static int icnss_event_soc_wake_request(struct icnss_priv *priv, void *data)
{
	int ret = 0;

	if (!priv)
		return -ENODEV;

	ret = wlfw_send_soc_wake_msg(priv, QMI_WLFW_WAKE_REQUEST_V01);
	if (!ret)
		atomic_inc(&priv->soc_wake_ref_count);

	return ret;
}

static int icnss_event_soc_wake_release(struct icnss_priv *priv, void *data)
{
	int ret = 0;
	int count = 0;

	if (!priv)
		return -ENODEV;

	count = atomic_dec_return(&priv->soc_wake_ref_count);

	if (count) {
		icnss_pr_dbg("Wake release not called. Ref count: %d",
			     count);
		return 0;
	}

	ret = wlfw_send_soc_wake_msg(priv, QMI_WLFW_WAKE_RELEASE_V01);

	return ret;
}

static int icnss_driver_event_register_driver(struct icnss_priv *priv,
							 void *data)
{
	int ret = 0;
	int probe_cnt = 0;

	if (priv->ops)
		return -EEXIST;

	priv->ops = data;

	if (test_bit(SKIP_QMI, &priv->ctrl_params.quirks))
		set_bit(ICNSS_FW_READY, &priv->state);

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     priv->state);
		return -ENODEV;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_dbg("FW is not ready yet, state: 0x%lx\n",
			     priv->state);
		goto out;
	}

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto out;

	icnss_block_shutdown(true);
	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = priv->ops->probe(&priv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, priv->state, probe_cnt);
		icnss_block_shutdown(false);
		goto power_off;
	}

	icnss_block_shutdown(false);
	set_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;

power_off:
	icnss_hw_power_off(priv);
out:
	return ret;
}

static int icnss_driver_event_unregister_driver(struct icnss_priv *priv,
							 void *data)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		priv->ops = NULL;
		goto out;
	}

	set_bit(ICNSS_DRIVER_UNLOADING, &priv->state);

	icnss_block_shutdown(true);

	if (priv->ops)
		priv->ops->remove(&priv->pdev->dev);

	icnss_block_shutdown(false);

	clear_bit(ICNSS_DRIVER_UNLOADING, &priv->state);
	clear_bit(ICNSS_DRIVER_PROBED, &priv->state);

	priv->ops = NULL;

	icnss_hw_power_off(priv);

out:
	return 0;
}

static int icnss_call_driver_remove(struct icnss_priv *priv)
{
	icnss_pr_dbg("Calling driver remove state: 0x%lx\n", priv->state);

	clear_bit(ICNSS_FW_READY, &priv->state);

	if (test_bit(ICNSS_DRIVER_UNLOADING, &priv->state))
		return 0;

	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		return 0;

	if (!priv->ops || !priv->ops->remove)
		return 0;

	set_bit(ICNSS_DRIVER_UNLOADING, &priv->state);
	priv->ops->remove(&priv->pdev->dev);

	clear_bit(ICNSS_DRIVER_UNLOADING, &priv->state);
	clear_bit(ICNSS_DRIVER_PROBED, &priv->state);

	icnss_hw_power_off(priv);

	return 0;
}

static int icnss_fw_crashed(struct icnss_priv *priv,
			    struct icnss_event_pd_service_down_data *event_data)
{
	icnss_pr_dbg("FW crashed, state: 0x%lx\n", priv->state);

	set_bit(ICNSS_PD_RESTART, &priv->state);
	clear_bit(ICNSS_FW_READY, &priv->state);

	icnss_pm_stay_awake(priv);

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_CRASHED, NULL);

	if (event_data && event_data->fw_rejuvenate)
		wlfw_rejuvenate_ack_send_sync_msg(priv);

	return 0;
}

static int icnss_driver_event_pd_service_down(struct icnss_priv *priv,
					      void *data)
{
	struct icnss_event_pd_service_down_data *event_data = data;

	if (!test_bit(ICNSS_WLFW_EXISTS, &priv->state)) {
		icnss_ignore_fw_timeout(false);
		goto out;
	}

	if (priv->force_err_fatal)
		ICNSS_ASSERT(0);

	if (priv->early_crash_ind) {
		icnss_pr_dbg("PD Down ignored as early indication is processed: %d, state: 0x%lx\n",
			     event_data->crashed, priv->state);
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state) && event_data->crashed) {
		icnss_fatal_err("PD Down while recovery inprogress, crashed: %d, state: 0x%lx\n",
				event_data->crashed, priv->state);
		if (!priv->allow_recursive_recovery)
			ICNSS_ASSERT(0);
		goto out;
	}

	if (!test_bit(ICNSS_PD_RESTART, &priv->state))
		icnss_fw_crashed(priv, event_data);

out:
	kfree(data);

	return 0;
}

static int icnss_driver_event_early_crash_ind(struct icnss_priv *priv,
					      void *data)
{
	if (!test_bit(ICNSS_WLFW_EXISTS, &priv->state)) {
		icnss_ignore_fw_timeout(false);
		goto out;
	}

	priv->early_crash_ind = true;
	icnss_fw_crashed(priv, NULL);

out:
	kfree(data);

	return 0;
}

static int icnss_driver_event_idle_shutdown(struct icnss_priv *priv,
					    void *data)
{
	int ret = 0;

	if (!priv->ops || !priv->ops->idle_shutdown)
		return 0;

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle shutdown callback\n");
		ret = -EBUSY;
	} else {
		icnss_pr_dbg("Calling driver idle shutdown, state: 0x%lx\n",
								priv->state);
		icnss_block_shutdown(true);
		ret = priv->ops->idle_shutdown(&priv->pdev->dev);
		icnss_block_shutdown(false);
	}

	return ret;
}

static int icnss_driver_event_idle_restart(struct icnss_priv *priv,
					   void *data)
{
	int ret = 0;

	if (!priv->ops || !priv->ops->idle_restart)
		return 0;

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle restart callback\n");
		ret = -EBUSY;
	} else {
		icnss_pr_dbg("Calling driver idle restart, state: 0x%lx\n",
								priv->state);
		icnss_block_shutdown(true);
		ret = priv->ops->idle_restart(&priv->pdev->dev);
		icnss_block_shutdown(false);
	}

	return ret;
}

static int icnss_qdss_trace_free_hdlr(struct icnss_priv *priv)
{
	icnss_free_qdss_mem(priv);

	return 0;
}

static void icnss_driver_event_work(struct work_struct *work)
{
	struct icnss_priv *priv =
		container_of(work, struct icnss_priv, event_work);
	struct icnss_driver_event *event;
	unsigned long flags;
	int ret;

	icnss_pm_stay_awake(priv);

	spin_lock_irqsave(&priv->event_lock, flags);

	while (!list_empty(&priv->event_list)) {
		event = list_first_entry(&priv->event_list,
					 struct icnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&priv->event_lock, flags);

		icnss_pr_dbg("Processing event: %s%s(%d), state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type,
			     priv->state);

		switch (event->type) {
		case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = icnss_driver_event_server_arrive(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = icnss_driver_event_server_exit(priv);
			break;
		case ICNSS_DRIVER_EVENT_FW_READY_IND:
			ret = icnss_driver_event_fw_ready_ind(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = icnss_driver_event_register_driver(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = icnss_driver_event_unregister_driver(priv,
								   event->data);
			break;
		case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
			ret = icnss_driver_event_pd_service_down(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND:
			ret = icnss_driver_event_early_crash_ind(priv,
								 event->data);
			break;
		case ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
			ret = icnss_driver_event_idle_shutdown(priv,
							       event->data);
			break;
		case ICNSS_DRIVER_EVENT_IDLE_RESTART:
			ret = icnss_driver_event_idle_restart(priv,
							      event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_INIT_DONE_IND:
			ret = icnss_driver_event_fw_init_done(priv,
							      event->data);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
			ret = icnss_qdss_trace_req_mem_hdlr(priv);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_SAVE:
			ret = icnss_qdss_trace_save_hdlr(priv,
							 event->data);
			break;
		case ICNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
			ret = icnss_qdss_trace_free_hdlr(priv);
			break;
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		priv->stats.events[event->type].processed++;

		icnss_pr_dbg("Event Processed: %s%s(%d), ret: %d, state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type, ret,
			     priv->state);

		spin_lock_irqsave(&priv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&priv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&priv->event_lock, flags);
	}
	spin_unlock_irqrestore(&priv->event_lock, flags);

	icnss_pm_relax(priv);
}

static void icnss_soc_wake_msg_work(struct work_struct *work)
{
	struct icnss_priv *priv =
		container_of(work, struct icnss_priv, soc_wake_msg_work);
	struct icnss_soc_wake_event *event;
	unsigned long flags;
	int ret;

	icnss_pm_stay_awake(priv);

	spin_lock_irqsave(&priv->soc_wake_msg_lock, flags);

	while (!list_empty(&priv->soc_wake_msg_list)) {
		event = list_first_entry(&priv->soc_wake_msg_list,
					 struct icnss_soc_wake_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&priv->soc_wake_msg_lock, flags);

		icnss_pr_dbg("Processing event: %s%s(%d), state: 0x%lx\n",
			     icnss_soc_wake_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type,
			     priv->state);

		switch (event->type) {
		case ICNSS_SOC_WAKE_REQUEST_EVENT:
			ret = icnss_event_soc_wake_request(priv,
							   event->data);
			break;
		case ICNSS_SOC_WAKE_RELEASE_EVENT:
			ret = icnss_event_soc_wake_release(priv,
							   event->data);
			break;
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		priv->stats.soc_wake_events[event->type].processed++;

		icnss_pr_dbg("Event Processed: %s%s(%d), ret: %d, state: 0x%lx\n",
			     icnss_soc_wake_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type, ret,
			     priv->state);

		spin_lock_irqsave(&priv->soc_wake_msg_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&priv->soc_wake_msg_lock, flags);

		kfree(event);

		spin_lock_irqsave(&priv->soc_wake_msg_lock, flags);
	}
	spin_unlock_irqrestore(&priv->soc_wake_msg_lock, flags);

	icnss_pm_relax(priv);
}

static int icnss_msa0_ramdump(struct icnss_priv *priv)
{
	struct ramdump_segment segment;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = priv->msa_va;
	segment.size = priv->msa_mem_size;
	return do_ramdump(priv->msa0_dump_dev, &segment, 1);
}

static void icnss_update_state_send_modem_shutdown(struct icnss_priv *priv,
							void *data)
{
	struct notif_data *notif = data;
	int ret = 0;

	if (!notif->crashed) {
		if (atomic_read(&priv->is_shutdown)) {
			atomic_set(&priv->is_shutdown, false);
			if (!test_bit(ICNSS_PD_RESTART, &priv->state) &&
				!test_bit(ICNSS_SHUTDOWN_DONE, &priv->state)) {
				icnss_call_driver_remove(priv);
			}
		}

		if (test_bit(ICNSS_BLOCK_SHUTDOWN, &priv->state)) {
			if (!wait_for_completion_timeout(
					&priv->unblock_shutdown,
					msecs_to_jiffies(PROBE_TIMEOUT)))
				icnss_pr_err("modem block shutdown timeout\n");
		}

		ret = wlfw_send_modem_shutdown_msg(priv);
		if (ret < 0)
			icnss_pr_err("Fail to send modem shutdown Indication %d\n",
				     ret);
	}
}

static int icnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *data)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct notif_data *notif = data;
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       modem_ssr_nb);
	struct icnss_uevent_fw_down_data fw_down_data;

	icnss_pr_vdbg("Modem-Notify: event %lu\n", code);

	if (code == SUBSYS_AFTER_SHUTDOWN) {
		icnss_pr_info("Collecting msa0 segment dump\n");
		icnss_msa0_ramdump(priv);
		return NOTIFY_OK;
	}

	if (code != SUBSYS_BEFORE_SHUTDOWN)
		return NOTIFY_OK;

	priv->is_ssr = true;

	icnss_update_state_send_modem_shutdown(priv, data);

	if (test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		fw_down_data.crashed = !!notif->crashed;
		if (test_bit(ICNSS_FW_READY, &priv->state))
			icnss_call_driver_uevent(priv,
						 ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		return NOTIFY_OK;
	}

	icnss_pr_info("Modem went down, state: 0x%lx, crashed: %d\n",
		      priv->state, notif->crashed);

	set_bit(ICNSS_FW_DOWN, &priv->state);

	if (notif->crashed)
		priv->stats.recovery.root_pd_crash++;
	else
		priv->stats.recovery.root_pd_shutdown++;

	icnss_ignore_fw_timeout(true);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = notif->crashed;

	fw_down_data.crashed = !!notif->crashed;
	if (test_bit(ICNSS_FW_READY, &priv->state))
		icnss_call_driver_uevent(priv,
					 ICNSS_UEVENT_FW_DOWN,
					 &fw_down_data);

	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);

	return NOTIFY_OK;
}

static int icnss_modem_ssr_register_notifier(struct icnss_priv *priv)
{
	int ret = 0;

	priv->modem_ssr_nb.notifier_call = icnss_modem_notifier_nb;

	priv->modem_notify_handler =
		subsys_notif_register_notifier("modem", &priv->modem_ssr_nb);

	if (IS_ERR(priv->modem_notify_handler)) {
		ret = PTR_ERR(priv->modem_notify_handler);
		icnss_pr_err("Modem register notifier failed: %d\n", ret);
	}

	set_bit(ICNSS_SSR_REGISTERED, &priv->state);

	return ret;
}

static int icnss_modem_ssr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_SSR_REGISTERED, &priv->state))
		return 0;

	subsys_notif_unregister_notifier(priv->modem_notify_handler,
					 &priv->modem_ssr_nb);
	priv->modem_notify_handler = NULL;

	return 0;
}

static int icnss_pdr_unregister_notifier(struct icnss_priv *priv)
{
	int i;

	if (!test_and_clear_bit(ICNSS_PDR_REGISTERED, &priv->state))
		return 0;

	for (i = 0; i < priv->total_domains; i++)
		service_notif_unregister_notifier(
				priv->service_notifier[i].handle,
				&priv->service_notifier_nb);

	kfree(priv->service_notifier);

	priv->service_notifier = NULL;

	return 0;
}

static int icnss_service_notifier_notify(struct notifier_block *nb,
					 unsigned long notification, void *data)
{
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       service_notifier_nb);
	enum pd_subsys_state *state = data;
	struct icnss_event_pd_service_down_data *event_data;
	struct icnss_uevent_fw_down_data fw_down_data;
	enum icnss_pdr_cause_index cause = ICNSS_ROOT_PD_CRASH;

	icnss_pr_dbg("PD service notification: 0x%lx state: 0x%lx\n",
		     notification, priv->state);

	if (notification != SERVREG_NOTIF_SERVICE_STATE_DOWN_V01)
		goto done;

	if (!priv->is_ssr)
		set_bit(ICNSS_PDR, &priv->state);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = true;

	if (state == NULL) {
		priv->stats.recovery.root_pd_crash++;
		goto event_post;
	}

	switch (*state) {
	case ROOT_PD_WDOG_BITE:
		priv->stats.recovery.root_pd_crash++;
		break;
	case ROOT_PD_SHUTDOWN:
		cause = ICNSS_ROOT_PD_SHUTDOWN;
		priv->stats.recovery.root_pd_shutdown++;
		event_data->crashed = false;
		break;
	case USER_PD_STATE_CHANGE:
		if (test_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state)) {
			cause = ICNSS_HOST_ERROR;
			priv->stats.recovery.pdr_host_error++;
		} else {
			cause = ICNSS_FW_CRASH;
			priv->stats.recovery.pdr_fw_crash++;
		}
		break;
	default:
		priv->stats.recovery.root_pd_crash++;
		break;
	}
	icnss_pr_info("PD service down, pd_state: %d, state: 0x%lx: cause: %s\n",
		      *state, priv->state, icnss_pdr_cause[cause]);
event_post:
	if (!test_bit(ICNSS_FW_DOWN, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_fw_timeout(true);

		fw_down_data.crashed = event_data->crashed;
		if (test_bit(ICNSS_FW_READY, &priv->state))
			icnss_call_driver_uevent(priv,
						 ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
	}

	clear_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);
	icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);
done:
	if (notification == SERVREG_NOTIF_SERVICE_STATE_UP_V01)
		clear_bit(ICNSS_FW_DOWN, &priv->state);
	return NOTIFY_OK;
}

static int icnss_get_service_location_notify(struct notifier_block *nb,
					     unsigned long opcode, void *data)
{
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       get_service_nb);
	struct pd_qmi_client_data *pd = data;
	int curr_state;
	int ret;
	int i;
	int j;
	bool duplicate;
	struct service_notifier_context *notifier;

	icnss_pr_dbg("Get service notify opcode: %lu, state: 0x%lx\n", opcode,
		     priv->state);

	if (opcode != LOCATOR_UP)
		return NOTIFY_DONE;

	if (pd->total_domains == 0) {
		icnss_pr_err("Did not find any domains\n");
		ret = -ENOENT;
		goto out;
	}

	notifier = kcalloc(pd->total_domains,
				sizeof(struct service_notifier_context),
				GFP_KERNEL);
	if (!notifier) {
		ret = -ENOMEM;
		goto out;
	}

	priv->service_notifier_nb.notifier_call = icnss_service_notifier_notify;

	for (i = 0; i < pd->total_domains; i++) {
		duplicate = false;
		for (j = i + 1; j < pd->total_domains; j++) {
			if (!strcmp(pd->domain_list[i].name,
			    pd->domain_list[j].name))
				duplicate = true;
		}

		if (duplicate)
			continue;

		icnss_pr_dbg("%d: domain_name: %s, instance_id: %d\n", i,
			     pd->domain_list[i].name,
			     pd->domain_list[i].instance_id);

		notifier[i].handle =
			service_notif_register_notifier(pd->domain_list[i].name,
				pd->domain_list[i].instance_id,
				&priv->service_notifier_nb, &curr_state);
		notifier[i].instance_id = pd->domain_list[i].instance_id;
		strlcpy(notifier[i].name, pd->domain_list[i].name,
			QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);

		if (IS_ERR(notifier[i].handle)) {
			icnss_pr_err("%d: Unable to register notifier for %s(0x%x)\n",
				     i, pd->domain_list->name,
				     pd->domain_list->instance_id);
			ret = PTR_ERR(notifier[i].handle);
			goto free_handle;
		}
	}

	priv->service_notifier = notifier;
	priv->total_domains = pd->total_domains;

	set_bit(ICNSS_PDR_REGISTERED, &priv->state);

	icnss_pr_dbg("PD notification registration happened, state: 0x%lx\n",
		     priv->state);

	return NOTIFY_OK;

free_handle:
	for (i = 0; i < pd->total_domains; i++) {
		if (notifier[i].handle)
			service_notif_unregister_notifier(notifier[i].handle,
					&priv->service_notifier_nb);
	}
	kfree(notifier);

out:
	icnss_pr_err("PD restart not enabled: %d, state: 0x%lx\n", ret,
		     priv->state);

	return NOTIFY_OK;
}


static int icnss_pd_restart_enable(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(SSR_ONLY, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

	icnss_pr_dbg("Get service location, state: 0x%lx\n", priv->state);

	priv->get_service_nb.notifier_call = icnss_get_service_location_notify;
	ret = get_service_location(ICNSS_SERVICE_LOCATION_CLIENT_NAME,
				   ICNSS_WLAN_SERVICE_NAME,
				   &priv->get_service_nb);
	if (ret) {
		icnss_pr_err("Get service location failed: %d\n", ret);
		goto out;
	}

	return 0;
out:
	icnss_pr_err("Failed to enable PD restart: %d\n", ret);
	return ret;

}


static int icnss_enable_recovery(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(RECOVERY_DISABLE, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("Recovery disabled through module parameter\n");
		return 0;
	}

	if (test_bit(PDR_ONLY, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("SSR disabled through module parameter\n");
		goto enable_pdr;
	}

	priv->msa0_dump_dev = create_ramdump_device("wcss_msa0",
						    &priv->pdev->dev);
	if (!priv->msa0_dump_dev)
		return -ENOMEM;

	icnss_modem_ssr_register_notifier(priv);
	if (test_bit(SSR_ONLY, &priv->ctrl_params.quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

enable_pdr:
	ret = icnss_pd_restart_enable(priv);

	if (ret)
		return ret;

	return 0;
}

int icnss_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	struct icnss_priv *priv = icnss_get_plat_priv();
	int ret;

	if (!priv)
		return -ENODEV;

	if (!test_bit(ICNSS_WLFW_CONNECTED, &priv->state))
		return -EINVAL;

	priv->get_info_cb = cb;
	priv->get_info_cb_ctx = cb_ctx;

	ret = icnss_wlfw_get_info_send_sync(priv, type, cmd, cmd_len);
	if (ret) {
		priv->get_info_cb = NULL;
		priv->get_info_cb_ctx = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(icnss_qmi_send);

int __icnss_register_driver(struct icnss_driver_ops *ops,
			    struct module *owner, const char *mod_name)
{
	int ret = 0;
	struct icnss_priv *priv = icnss_get_plat_priv();

	if (!priv || !priv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Registering driver, state: 0x%lx\n", priv->state);

	if (priv->ops) {
		icnss_pr_err("Driver already registered\n");
		ret = -EEXIST;
		goto out;
	}

	if (!ops->probe || !ops->remove) {
		ret = -EINVAL;
		goto out;
	}

	ret = icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
				      0, ops);

	if (ret == -EINTR)
		ret = 0;

out:
	return ret;
}
EXPORT_SYMBOL(__icnss_register_driver);

int icnss_unregister_driver(struct icnss_driver_ops *ops)
{
	int ret;
	struct icnss_priv *priv = icnss_get_plat_priv();

	if (!priv || !priv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Unregistering driver, state: 0x%lx\n", priv->state);

	if (!priv->ops) {
		icnss_pr_err("Driver not registered\n");
		ret = -ENOENT;
		goto out;
	}

	ret = icnss_driver_event_post(priv,
					 ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
				      ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_driver);

static struct icnss_msi_config msi_config = {
	.total_vectors = 28,
	.total_users = 2,
	.users = (struct icnss_msi_user[]) {
		{ .name = "CE", .num_vectors = 10, .base_vector = 0 },
		{ .name = "DP", .num_vectors = 18, .base_vector = 10 },
	},
};

static int icnss_get_msi_assignment(struct icnss_priv *priv)
{
	priv->msi_config = &msi_config;

	return 0;
}

int icnss_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	struct icnss_msi_config *msi_config;
	int idx;

	if (!priv)
		return -ENODEV;

	msi_config = priv->msi_config;
	if (!msi_config) {
		icnss_pr_err("MSI is not supported.\n");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ priv->msi_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			icnss_pr_dbg("Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				    user_name, *num_vectors, *user_base_data,
				    *base_vector);

			return 0;
		}
	}

	icnss_pr_err("Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_user_msi_assignment);

int icnss_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int irq_num;

	irq_num = priv->srng_irqs[vector];
	icnss_pr_dbg("Get IRQ number %d for vector index %d\n",
		     irq_num, vector);

	return irq_num;
}
EXPORT_SYMBOL(icnss_get_msi_irq);

void icnss_get_msi_address(struct device *dev, u32 *msi_addr_low,
			   u32 *msi_addr_high)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	*msi_addr_low = lower_32_bits(priv->msi_addr_iova);
	*msi_addr_high = upper_32_bits(priv->msi_addr_iova);

}
EXPORT_SYMBOL(icnss_get_msi_address);

int icnss_ce_request_irq(struct device *dev, unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv || !priv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE request IRQ: %d, state: 0x%lx\n", ce_id, priv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = priv->ce_irqs[ce_id];
	irq_entry = &priv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		icnss_pr_err("IRQ already requested: %d, ce_id: %d\n",
			     irq, ce_id);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, flags, name, ctx);
	if (ret) {
		icnss_pr_err("IRQ request failed: %d, ce_id: %d, ret: %d\n",
			     irq, ce_id, ret);
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;

	icnss_pr_vdbg("IRQ requested: %d, ce_id: %d\n", irq, ce_id);

	penv->stats.ce_irqs[ce_id].request++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_request_irq);

int icnss_ce_free_irq(struct device *dev, unsigned int ce_id, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev || !dev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE free IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to free, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}

	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		icnss_pr_err("IRQ not requested: %d, ce_id: %d\n", irq, ce_id);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, ctx);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;

	penv->stats.ce_irqs[ce_id].free++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_free_irq);

void icnss_enable_irq(struct device *dev, unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_vdbg("Enable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
		     penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to enable IRQ, ce_id: %d\n", ce_id);
		return;
	}

	penv->stats.ce_irqs[ce_id].enable++;

	irq = penv->ce_irqs[ce_id];
	enable_irq(irq);
}
EXPORT_SYMBOL(icnss_enable_irq);

void icnss_disable_irq(struct device *dev, unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_vdbg("Disable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
		     penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to disable IRQ, ce_id: %d\n",
			     ce_id);
		return;
	}

	irq = penv->ce_irqs[ce_id];
	disable_irq(irq);

	penv->stats.ce_irqs[ce_id].disable++;
}
EXPORT_SYMBOL(icnss_disable_irq);

int icnss_get_soc_info(struct device *dev, struct icnss_soc_info *info)
{
	char *fw_build_timestamp = NULL;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	info->v_addr = priv->mem_base_va;
	info->p_addr = priv->mem_base_pa;
	info->chip_id = priv->chip_info.chip_id;
	info->chip_family = priv->chip_info.chip_family;
	info->board_id = priv->board_id;
	info->soc_id = priv->soc_id;
	info->fw_version = priv->fw_version_info.fw_version;
	fw_build_timestamp = priv->fw_version_info.fw_build_timestamp;
	fw_build_timestamp[WLFW_MAX_TIMESTAMP_LEN] = '\0';
	strlcpy(info->fw_build_timestamp,
		priv->fw_version_info.fw_build_timestamp,
		WLFW_MAX_TIMESTAMP_LEN + 1);

	return 0;
}
EXPORT_SYMBOL(icnss_get_soc_info);

int icnss_set_fw_log_mode(struct device *dev, uint8_t fw_log_mode)
{
	int ret;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!dev)
		return -ENODEV;

	if (test_bit(ICNSS_FW_DOWN, &penv->state) ||
	    !test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_err("FW down, ignoring fw_log_mode state: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	icnss_pr_dbg("FW log mode: %u\n", fw_log_mode);

	ret = wlfw_ini_send_sync_msg(priv, fw_log_mode);
	if (ret)
		icnss_pr_err("Fail to send ini, ret = %d, fw_log_mode: %u\n",
			     ret, fw_log_mode);
	return ret;
}
EXPORT_SYMBOL(icnss_set_fw_log_mode);

int icnss_force_wake_request(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int count = 0;

	if (!dev)
		return -ENODEV;

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	icnss_pr_dbg("Calling SOC Wake request");

	if (atomic_read(&priv->soc_wake_ref_count)) {
		count = atomic_inc_return(&priv->soc_wake_ref_count);
		icnss_pr_dbg("SOC already awake, Ref count: %d", count);
		return 0;
	}

	icnss_soc_wake_event_post(priv, ICNSS_SOC_WAKE_REQUEST_EVENT,
				  0, NULL);

	return 0;
}
EXPORT_SYMBOL(icnss_force_wake_request);

int icnss_force_wake_release(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!dev)
		return -ENODEV;

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	icnss_pr_dbg("Calling SOC Wake response");

	icnss_soc_wake_event_post(priv, ICNSS_SOC_WAKE_RELEASE_EVENT,
				  0, NULL);

	return 0;
}
EXPORT_SYMBOL(icnss_force_wake_release);

int icnss_is_device_awake(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!dev)
		return -ENODEV;

	if (!priv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	return atomic_read(&priv->soc_wake_ref_count);
}
EXPORT_SYMBOL(icnss_is_device_awake);

int icnss_athdiag_read(struct device *dev, uint32_t offset,
		       uint32_t mem_type, uint32_t data_len,
		       uint8_t *output)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for diag read: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!output || data_len == 0
	    || data_len > WLFW_MAX_DATA_SIZE) {
		icnss_pr_err("Invalid parameters for diag read: output %pK, data_len %u\n",
			     output, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state)) {
		icnss_pr_err("Invalid state for diag read: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	ret = wlfw_athdiag_read_send_sync_msg(priv, offset, mem_type,
					      data_len, output);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_athdiag_read);

int icnss_athdiag_write(struct device *dev, uint32_t offset,
			uint32_t mem_type, uint32_t data_len,
			uint8_t *input)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for diag write: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!input || data_len == 0
	    || data_len > WLFW_MAX_DATA_SIZE) {
		icnss_pr_err("Invalid parameters for diag write: input %pK, data_len %u\n",
			     input, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state)) {
		icnss_pr_err("Invalid state for diag write: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	ret = wlfw_athdiag_write_send_sync_msg(priv, offset, mem_type,
					       data_len, input);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_athdiag_write);

int icnss_wlan_enable(struct device *dev, struct icnss_wlan_enable_cfg *config,
		      enum icnss_driver_mode mode,
		      const char *host_version)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (test_bit(ICNSS_FW_DOWN, &priv->state) ||
	    !test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("FW down, ignoring wlan_enable state: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	if (test_bit(ICNSS_MODE_ON, &priv->state)) {
		icnss_pr_err("Already Mode on, ignoring wlan_enable state: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	return icnss_send_wlan_enable_to_fw(priv, config, mode, host_version);
}
EXPORT_SYMBOL(icnss_wlan_enable);

int icnss_wlan_disable(struct device *dev, enum icnss_driver_mode mode)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_dbg("FW down, ignoring wlan_disable state: 0x%lx\n",
			     priv->state);
		return 0;
	}

	return icnss_send_wlan_disable_to_fw(priv);
}
EXPORT_SYMBOL(icnss_wlan_disable);

bool icnss_is_qmi_disable(struct device *dev)
{
	return test_bit(SKIP_QMI, &penv->ctrl_params.quirks) ? true : false;
}
EXPORT_SYMBOL(icnss_is_qmi_disable);

int icnss_get_ce_id(struct device *dev, int irq)
{
	int i;

	if (!penv || !penv->pdev || !dev)
		return -ENODEV;

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		if (penv->ce_irqs[i] == irq)
			return i;
	}

	icnss_pr_err("No matching CE id for irq %d\n", irq);

	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_ce_id);

int icnss_get_irq(struct device *dev, int ce_id)
{
	int irq;

	if (!penv || !penv->pdev || !dev)
		return -ENODEV;

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS)
		return -EINVAL;

	irq = penv->ce_irqs[ce_id];

	return irq;
}
EXPORT_SYMBOL(icnss_get_irq);

struct iommu_domain *icnss_smmu_get_domain(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK\n", dev);
		return NULL;
	}
	return priv->iommu_domain;
}
EXPORT_SYMBOL(icnss_smmu_get_domain);

int icnss_smmu_map(struct device *dev,
		   phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	unsigned long iova;
	size_t len;
	int ret = 0;

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	if (!iova_addr) {
		icnss_pr_err("iova_addr is NULL, paddr %pa, size %zu\n",
			     &paddr, size);
		return -EINVAL;
	}

	len = roundup(size + paddr - rounddown(paddr, PAGE_SIZE), PAGE_SIZE);
	iova = roundup(priv->smmu_iova_ipa_start, PAGE_SIZE);

	if (iova >= priv->smmu_iova_ipa_start + priv->smmu_iova_ipa_len) {
		icnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			     iova,
			     &priv->smmu_iova_ipa_start,
			     priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	ret = iommu_map(priv->iommu_domain, iova,
			rounddown(paddr, PAGE_SIZE), len,
			IOMMU_READ | IOMMU_WRITE);
	if (ret) {
		icnss_pr_err("PA to IOVA mapping failed, ret %d\n", ret);
		return ret;
	}

	priv->smmu_iova_ipa_start = iova + len;
	*iova_addr = (uint32_t)(iova + paddr - rounddown(paddr, PAGE_SIZE));

	return 0;
}
EXPORT_SYMBOL(icnss_smmu_map);

unsigned int icnss_socinfo_get_serial_number(struct device *dev)
{
	return socinfo_get_serial_number();
}
EXPORT_SYMBOL(icnss_socinfo_get_serial_number);

int icnss_trigger_recovery(struct device *dev)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata: magic 0x%x\n", priv->magic);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state)) {
		icnss_pr_err("PD recovery already in progress: state: 0x%lx\n",
			     priv->state);
		ret = -EPERM;
		goto out;
	}

	if (!test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		icnss_pr_err("PD restart not enabled to trigger recovery: state: 0x%lx\n",
			     priv->state);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!priv->service_notifier || !priv->service_notifier[0].handle) {
		icnss_pr_err("Invalid handle during recovery, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	icnss_pr_warn("Initiate PD restart at WLAN FW, state: 0x%lx\n",
		      priv->state);

	/*
	 * Initiate PDR, required only for the first instance
	 */
	ret = service_notif_pd_restart(priv->service_notifier[0].name,
		priv->service_notifier[0].instance_id);

	if (!ret)
		set_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);

out:
	return ret;
}
EXPORT_SYMBOL(icnss_trigger_recovery);

int icnss_idle_shutdown(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK", dev);
		return -EINVAL;
	}

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle shutdown\n");
		return -EBUSY;
	}

	return icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN,
					ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(icnss_idle_shutdown);

int icnss_idle_restart(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK", dev);
		return -EINVAL;
	}

	if (priv->is_ssr || test_bit(ICNSS_PDR, &priv->state) ||
	    test_bit(ICNSS_REJUVENATE, &priv->state)) {
		icnss_pr_err("SSR/PDR is already in-progress during idle restart\n");
		return -EBUSY;
	}

	return icnss_driver_event_post(priv, ICNSS_DRIVER_EVENT_IDLE_RESTART,
					ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(icnss_idle_restart);

void icnss_allow_recursive_recovery(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	priv->allow_recursive_recovery = true;

	icnss_pr_info("Recursive recovery allowed for WLAN\n");
}

void icnss_disallow_recursive_recovery(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	priv->allow_recursive_recovery = false;

	icnss_pr_info("Recursive recovery disallowed for WLAN\n");
}

static void icnss_sysfs_create(struct icnss_priv *priv)
{
	struct kobject *icnss_kobject;
	int error = 0;

	atomic_set(&priv->is_shutdown, false);

	icnss_kobject = kobject_create_and_add("shutdown_wlan", kernel_kobj);
	if (!icnss_kobject) {
		icnss_pr_err("Unable to create kernel object");
		return;
	}

	priv->icnss_kobject = icnss_kobject;

	error = sysfs_create_file(icnss_kobject, &icnss_sysfs_attribute.attr);
	if (error)
		icnss_pr_err("Unable to create icnss sysfs file");
}

static void icnss_sysfs_destroy(struct icnss_priv *priv)
{
	struct kobject *icnss_kobject;

	icnss_kobject = priv->icnss_kobject;
	if (icnss_kobject)
		kobject_put(icnss_kobject);
}

static int icnss_get_vbatt_info(struct icnss_priv *priv)
{
	struct adc_tm_chip *adc_tm_dev = NULL;
	struct iio_channel *channel = NULL;
	int ret = 0;

	adc_tm_dev = get_adc_tm(&priv->pdev->dev, "icnss");
	if (PTR_ERR(adc_tm_dev) == -EPROBE_DEFER) {
		icnss_pr_err("adc_tm_dev probe defer\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(adc_tm_dev)) {
		ret = PTR_ERR(adc_tm_dev);
		icnss_pr_err("Not able to get ADC dev, VBATT monitoring is disabled: %d\n",
			     ret);
		return ret;
	}

	channel = iio_channel_get(&priv->pdev->dev, "icnss");
	if (PTR_ERR(channel) == -EPROBE_DEFER) {
		icnss_pr_err("channel probe defer\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		icnss_pr_err("Not able to get VADC dev, VBATT monitoring is disabled: %d\n",
			     ret);
		return ret;
	}

	priv->adc_tm_dev = adc_tm_dev;
	priv->channel = channel;

	return 0;
}

static int icnss_resource_parse(struct icnss_priv *priv)
{
	int ret = 0, i = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 int_prop;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,icnss-adc_tm")) {
		ret = icnss_get_vbatt_info(priv);
		if (ret == -EPROBE_DEFER)
			goto out;
		priv->vbatt_supported = true;
	}

	ret = icnss_get_vreg(priv);
	if (ret) {
		icnss_pr_err("Failed to get vreg, err = %d\n", ret);
		goto out;
	}

	ret = icnss_get_clk(priv);
	if (ret) {
		icnss_pr_err("Failed to get clocks, err = %d\n", ret);
		goto put_vreg;
	}

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "membase");
		if (!res) {
			icnss_pr_err("Memory base not found in DT\n");
			ret = -EINVAL;
			goto put_clk;
		}

		priv->mem_base_pa = res->start;
		priv->mem_base_va = devm_ioremap(dev, priv->mem_base_pa,
						 resource_size(res));
		if (!priv->mem_base_va) {
			icnss_pr_err("Memory base ioremap failed: phy addr: %pa\n",
				     &priv->mem_base_pa);
			ret = -EINVAL;
			goto put_clk;
		}
		icnss_pr_dbg("MEM_BASE pa: %pa, va: 0x%pK\n",
			     &priv->mem_base_pa,
			     priv->mem_base_va);

		for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
			res = platform_get_resource(priv->pdev,
						    IORESOURCE_IRQ, i);
			if (!res) {
				icnss_pr_err("Fail to get IRQ-%d\n", i);
				ret = -ENODEV;
				goto put_clk;
			} else {
				priv->ce_irqs[i] = res->start;
			}
		}
	} else if (priv->device_id == WCN6750_DEVICE_ID) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "msi_addr");
		if (!res) {
			icnss_pr_err("MSI address not found in DT\n");
			ret = -EINVAL;
			goto put_clk;
		}

		priv->msi_addr_pa = res->start;
		priv->msi_addr_iova = dma_map_resource(dev, priv->msi_addr_pa,
						       PAGE_SIZE,
						       DMA_FROM_DEVICE, 0);
		if (dma_mapping_error(dev, priv->msi_addr_iova)) {
			icnss_pr_err("MSI: failed to map msi address\n");
			priv->msi_addr_iova = 0;
			ret = -ENOMEM;
			goto put_clk;
		}
		icnss_pr_dbg("MSI Addr pa: %pa, iova: 0x%pK\n",
			     &priv->msi_addr_pa,
			     priv->msi_addr_iova);

		ret = of_property_read_u32_index(dev->of_node,
						 "interrupts",
						 1,
						 &int_prop);
		if (ret) {
			icnss_pr_dbg("Read interrupt prop failed");
			goto put_clk;
		}

		priv->msi_base_data = int_prop + 32;
		icnss_pr_dbg(" MSI Base Data: %d, IRQ Index: %d\n",
			     priv->msi_base_data, int_prop);

		icnss_get_msi_assignment(priv);
		for (i = 0; i < msi_config.total_vectors; i++) {
			res = platform_get_resource(priv->pdev,
						    IORESOURCE_IRQ, i);
			if (!res) {
				icnss_pr_err("Fail to get IRQ-%d\n", i);
				ret = -ENODEV;
				goto put_clk;
			} else {
				priv->srng_irqs[i] = res->start;
			}
		}
	}

	return 0;

put_clk:
	icnss_put_clk(priv);
put_vreg:
	icnss_put_vreg(priv);
out:
	return ret;
}

static int icnss_msa_dt_parse(struct icnss_priv *priv)
{
	int ret = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *np = NULL;
	u64 prop_size = 0;
	const __be32 *addrp = NULL;

	np = of_parse_phandle(dev->of_node,
			      "qcom,wlan-msa-fixed-region", 0);
	if (np) {
		addrp = of_get_address(np, 0, &prop_size, NULL);
		if (!addrp) {
			icnss_pr_err("Failed to get assigned-addresses or property\n");
			ret = -EINVAL;
			of_node_put(np);
			goto out;
		}

		priv->msa_pa = of_translate_address(np, addrp);
		if (priv->msa_pa == OF_BAD_ADDR) {
			icnss_pr_err("Failed to translate MSA PA from device-tree\n");
			ret = -EINVAL;
			of_node_put(np);
			goto out;
		}

		of_node_put(np);

		priv->msa_va = memremap(priv->msa_pa,
					(unsigned long)prop_size, MEMREMAP_WT);
		if (!priv->msa_va) {
			icnss_pr_err("MSA PA ioremap failed: phy addr: %pa\n",
				     &priv->msa_pa);
			ret = -EINVAL;
			goto out;
		}
		priv->msa_mem_size = prop_size;
	} else {
		ret = of_property_read_u32(dev->of_node, "qcom,wlan-msa-memory",
					   &priv->msa_mem_size);
		if (ret || priv->msa_mem_size == 0) {
			icnss_pr_err("Fail to get MSA Memory Size: %u ret: %d\n",
				     priv->msa_mem_size, ret);
			goto out;
		}

		priv->msa_va = dmam_alloc_coherent(&pdev->dev,
				priv->msa_mem_size, &priv->msa_pa, GFP_KERNEL);

		if (!priv->msa_va) {
			icnss_pr_err("DMA alloc failed for MSA\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	icnss_pr_dbg("MSA pa: %pa, MSA va: 0x%pK MSA Memory Size: 0x%x\n",
		     &priv->msa_pa, (void *)priv->msa_va, priv->msa_mem_size);

	return 0;

out:
	return ret;
}

static int icnss_smmu_dt_parse(struct icnss_priv *priv)
{
	int ret = 0;
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 addr_win[2];

	ret = of_property_read_u32_array(dev->of_node,
					 "qcom,iommu-dma-addr-pool",
					 addr_win,
					 ARRAY_SIZE(addr_win));

	if (ret) {
		icnss_pr_err("SMMU IOVA base not found\n");
	} else {
		priv->iommu_domain =
			iommu_get_domain_for_dev(&pdev->dev);

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "smmu_iova_ipa");
		if (!res) {
			icnss_pr_err("SMMU IOVA IPA not found\n");
		} else {
			priv->smmu_iova_ipa_start = res->start;
			priv->smmu_iova_ipa_len = resource_size(res);
			icnss_pr_dbg("SMMU IOVA IPA start: %pa, len: %zx\n",
				     &priv->smmu_iova_ipa_start,
				     priv->smmu_iova_ipa_len);
		}
	}

	return 0;
}

static const struct platform_device_id icnss_platform_id_table[] = {
	{ .name = "wcn6750", .driver_data = WCN6750_DEVICE_ID, },
	{ .name = "adrastea", .driver_data = ADRASTEA_DEVICE_ID, },
	{ },
};

static const struct of_device_id icnss_dt_match[] = {
	{
		.compatible = "qcom,wcn6750",
		.data = (void *)&icnss_platform_id_table[0]},
	{
		.compatible = "qcom,icnss",
		.data = (void *)&icnss_platform_id_table[1]},
	{ },
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static void icnss_init_control_params(struct icnss_priv *priv)
{
	priv->ctrl_params.qmi_timeout = WLFW_TIMEOUT;
	priv->ctrl_params.quirks = ICNSS_QUIRKS_DEFAULT;
	priv->ctrl_params.bdf_type = ICNSS_BDF_TYPE_DEFAULT;

	if (of_property_read_bool(priv->pdev->dev.of_node,
				  "cnss-daemon-support")) {
		priv->ctrl_params.quirks |= BIT(ENABLE_DAEMON_SUPPORT);
	}
}

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct icnss_priv *priv;
	const struct of_device_id *of_id;
	const struct platform_device_id *device_id;

	if (dev_get_drvdata(dev)) {
		icnss_pr_err("Driver is already initialized\n");
		return -EEXIST;
	}

	of_id = of_match_device(icnss_dt_match, &pdev->dev);
	if (!of_id || !of_id->data) {
		icnss_pr_err("Failed to find of match device!\n");
		ret = -ENODEV;
		goto out;
	}

	device_id = of_id->data;

	icnss_pr_dbg("Platform driver probe\n");

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->magic = ICNSS_MAGIC;
	dev_set_drvdata(dev, priv);

	priv->pdev = pdev;
	priv->device_id = device_id->driver_data;
	INIT_LIST_HEAD(&priv->vreg_list);
	INIT_LIST_HEAD(&priv->clk_list);
	icnss_allow_recursive_recovery(dev);

	icnss_init_control_params(priv);

	ret = icnss_resource_parse(priv);
	if (ret)
		goto out;

	ret = icnss_msa_dt_parse(priv);
	if (ret)
		goto out;

	ret = icnss_smmu_dt_parse(priv);
	if (ret)
		goto out;

	spin_lock_init(&priv->event_lock);
	spin_lock_init(&priv->on_off_lock);
	spin_lock_init(&priv->soc_wake_msg_lock);
	mutex_init(&priv->dev_lock);

	priv->event_wq = alloc_workqueue("icnss_driver_event", WQ_UNBOUND, 1);
	if (!priv->event_wq) {
		icnss_pr_err("Workqueue creation failed\n");
		ret = -EFAULT;
		goto smmu_cleanup;
	}

	INIT_WORK(&priv->event_work, icnss_driver_event_work);
	INIT_LIST_HEAD(&priv->event_list);

	priv->soc_wake_wq = alloc_workqueue("icnss_soc_wake_event",
					    WQ_UNBOUND, 1);
	if (!priv->soc_wake_wq) {
		icnss_pr_err("Soc wake Workqueue creation failed\n");
		ret = -EFAULT;
		goto out_destroy_wq;
	}

	INIT_WORK(&priv->soc_wake_msg_work, icnss_soc_wake_msg_work);
	INIT_LIST_HEAD(&priv->soc_wake_msg_list);

	ret = icnss_register_fw_service(priv);
	if (ret < 0) {
		icnss_pr_err("fw service registration failed: %d\n", ret);
		goto out_destroy_soc_wq;
	}

	icnss_enable_recovery(priv);

	icnss_debugfs_create(priv);

	icnss_sysfs_create(priv);

	ret = device_init_wakeup(&priv->pdev->dev, true);
	if (ret)
		icnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			     ret);

	icnss_set_plat_priv(priv);

	init_completion(&priv->unblock_shutdown);

	ret = icnss_genl_init();
	if (ret < 0)
		icnss_pr_err("ICNSS genl init failed %d\n", ret);

	icnss_pr_info("Platform driver probed successfully\n");

	return 0;

out_destroy_soc_wq:
	destroy_workqueue(priv->soc_wake_wq);
out_destroy_wq:
	destroy_workqueue(priv->event_wq);
smmu_cleanup:
	priv->iommu_domain = NULL;
out:
	dev_set_drvdata(dev, NULL);

	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	struct icnss_priv *priv = dev_get_drvdata(&pdev->dev);

	icnss_pr_info("Removing driver: state: 0x%lx\n", priv->state);

	icnss_genl_exit();

	device_init_wakeup(&priv->pdev->dev, false);

	icnss_debugfs_destroy(priv);

	icnss_sysfs_destroy(priv);

	complete_all(&priv->unblock_shutdown);

	icnss_modem_ssr_unregister_notifier(priv);

	destroy_ramdump_device(priv->msa0_dump_dev);

	icnss_pdr_unregister_notifier(priv);

	icnss_unregister_fw_service(priv);
	if (priv->event_wq)
		destroy_workqueue(priv->event_wq);

	if (priv->soc_wake_wq)
		destroy_workqueue(priv->soc_wake_wq);

	priv->iommu_domain = NULL;

	icnss_hw_power_off(priv);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int icnss_pm_suspend(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm suspend: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM Suspend, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->pm_suspend ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->pm_suspend(dev);

out:
	if (ret == 0) {
		priv->stats.pm_suspend++;
		set_bit(ICNSS_PM_SUSPEND, &priv->state);
	} else {
		priv->stats.pm_suspend_err++;
	}
	return ret;
}

static int icnss_pm_resume(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm resume: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM resume, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->pm_resume ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	if (priv->device_id == WCN6750_DEVICE_ID) {
		ret = wlfw_exit_power_save_send_msg(priv);
		if (ret) {
			priv->stats.pm_resume_err++;
			return ret;
		}
	}

	ret = priv->ops->pm_resume(dev);

out:
	if (ret == 0) {
		priv->stats.pm_resume++;
		clear_bit(ICNSS_PM_SUSPEND, &priv->state);
	} else {
		priv->stats.pm_resume_err++;
	}
	return ret;
}

static int icnss_pm_suspend_noirq(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm suspend_noirq: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM suspend_noirq, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->suspend_noirq ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->suspend_noirq(dev);

out:
	if (ret == 0) {
		priv->stats.pm_suspend_noirq++;
		set_bit(ICNSS_PM_SUSPEND_NOIRQ, &priv->state);
	} else {
		priv->stats.pm_suspend_noirq_err++;
	}
	return ret;
}

static int icnss_pm_resume_noirq(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm resume_noirq: dev %pK, data %pK, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_vdbg("PM resume_noirq, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->resume_noirq ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->resume_noirq(dev);

out:
	if (ret == 0) {
		priv->stats.pm_resume_noirq++;
		clear_bit(ICNSS_PM_SUSPEND_NOIRQ, &priv->state);
	} else {
		priv->stats.pm_resume_noirq_err++;
	}
	return ret;
}
#endif

static const struct dev_pm_ops icnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend,
				icnss_pm_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend_noirq,
				      icnss_pm_resume_noirq)
};


static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.driver = {
		.name = "icnss2",
		.pm = &icnss_pm_ops,
		.of_match_table = icnss_dt_match,
	},
};

static int __init icnss_initialize(void)
{
	icnss_debug_init();
	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
	icnss_debug_deinit();
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iWCN CORE platform driver");

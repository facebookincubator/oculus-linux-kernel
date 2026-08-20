// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * ADSP per-user-PD crash/restart debugfs hooks.
 *
 * Exposes one write-only debugfs file per ADSP user PD under
 *   /sys/kernel/debug/dsp_pd_kill/
 * Writing "1" to a node issues a QMI SERVREG_RESTART_PD_REQ for that
 * single PD via pdr_restart_pd(), leaving the ADSP root PD (and any
 * other user PDs) running.
 *
 * WARNING: may panic the AP if CONFIG_QCOM_PANIC_ON_PDR_NOTIF_TIMEOUT
 * is enabled and a PDR client fails to ack recovery. This driver is
 * intended to be installed on userdebug/eng builds only.
 */

#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/soc/qcom/pdr.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)

#define DSP_PD_KILL_DIR				 ("dsp_pd_kill")
#define DSP_PD_KILL_TRIGGER_VAL 	 (1)
#define DSP_PD_KILL_READY_TIMEOUT_MS (2000)

struct dsp_pd_kill_entry {
	const char *name;
	const char *service_name;
	const char *service_path;
	struct pdr_handle *handle;
	struct pdr_service *service;
	struct completion ready;
	bool up;
};

static struct dsp_pd_kill_entry dsp_pd_kill_entries[] = {
	{
		.name			= "audio_pd",
		.service_name	= "avs/audio",
		.service_path	= "msm/adsp/audio_pd",
	},
	{
		.name			= "sensor_pd",
		.service_name	= "tms/servreg",
		.service_path	= "msm/adsp/sensor_pd",
	},
};

static struct dentry *dsp_pd_kill_root;

static void dsp_pd_kill_status_cb(int state, char *service_path, void *priv)
{
	struct dsp_pd_kill_entry *entry = priv;

	if (!entry || !service_path)
		return;

	if (strcmp(service_path, entry->service_path) != 0)
		return;

	switch (state) {
		case SERVREG_SERVICE_STATE_UP:
			entry->up = true;
			complete(&entry->ready);
			pr_info("%s UP\n", entry->service_path);
			break;
		case SERVREG_SERVICE_STATE_DOWN:
			entry->up = false;
			pr_info("dsp_pd_kill: %s DOWN\n", entry->service_path);
			break;
		case SERVREG_SERVICE_STATE_EARLY_DOWN:
			entry->up = false;
			pr_info("dsp_pd_kill: %s EARLY_DOWN\n", entry->service_path);
			break;
		default:
			break;
	}
}

static ssize_t dsp_pd_kill_write(struct file *file,
								 const char __user *ubuf,
								 size_t count, loff_t *ppos)
{
	struct dsp_pd_kill_entry *entry = file->private_data;
	unsigned long timeout;
	u32 val;
	int ret;

	if (!entry || IS_ERR_OR_NULL(entry->handle) ||
		IS_ERR_OR_NULL(entry->service))
		return -ENODEV;

	ret = kstrtou32_from_user(ubuf, count, 0, &val);
	if (ret)
		return ret;

	if (val != DSP_PD_KILL_TRIGGER_VAL)
		return -EINVAL;

	if (!entry->up) {
		timeout = msecs_to_jiffies(DSP_PD_KILL_READY_TIMEOUT_MS);
		if (!wait_for_completion_timeout(&entry->ready, timeout)) {
			pr_warn("%s not ready; no active client?\n",
					entry->service_path);
			return -EAGAIN;
		}
	}

	pr_info("restarting %s\n", entry->service_path);
	ret = pdr_restart_pd(entry->handle, entry->service);
	if (ret) {
		pr_err("pdr_restart_pd(%s) failed: %d\n",
			   entry->service_path, ret);
		return ret;
	}

	return count;
}

static const struct file_operations dsp_pd_kill_fops = {
	.owner	= THIS_MODULE,
	.open	= simple_open,
	.write	= dsp_pd_kill_write,
	.llseek	= noop_llseek,
};

static void dsp_pd_kill_release_entries(void)
{
	struct dsp_pd_kill_entry *entry;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dsp_pd_kill_entries); i++) {
		entry = &dsp_pd_kill_entries[i];
		if (!IS_ERR_OR_NULL(entry->handle))
			pdr_handle_release(entry->handle);
		entry->handle = NULL;
		entry->service = NULL;
	}
}

static int __init dsp_pd_kill_init(void)
{
	struct dsp_pd_kill_entry *entry;
	struct dentry *file;
	size_t i;
	int ret;

	dsp_pd_kill_root = debugfs_create_dir(DSP_PD_KILL_DIR, NULL);
	if (IS_ERR(dsp_pd_kill_root)) {
		ret = PTR_ERR(dsp_pd_kill_root);
		dsp_pd_kill_root = NULL;
		pr_err("debugfs_create_dir failed: %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(dsp_pd_kill_entries); i++) {
		entry = &dsp_pd_kill_entries[i];
		init_completion(&entry->ready);
		entry->up = false;

		entry->handle = pdr_handle_alloc(dsp_pd_kill_status_cb, entry);
		if (IS_ERR(entry->handle)) {
			ret = PTR_ERR(entry->handle);
			entry->handle = NULL;
			pr_err("pdr_handle_alloc(%s) failed: %d\n",
				   entry->service_path, ret);
			goto err_release;
		}

		entry->service = pdr_add_lookup(entry->handle,
										entry->service_name,
								  entry->service_path);
		if (IS_ERR(entry->service)) {
			ret = PTR_ERR(entry->service);
			entry->service = NULL;
			pr_err("pdr_add_lookup(%s) failed: %d\n",
				   entry->service_path, ret);
			goto err_release;
		}

		file = debugfs_create_file(entry->name, 0200,
								   dsp_pd_kill_root, entry,
							 &dsp_pd_kill_fops);
		if (IS_ERR(file)) {
			ret = PTR_ERR(file);
			pr_err("debugfs_create_file(%s) failed: %d\n",
				   entry->name, ret);
			goto err_release;
		}
	}

	pr_info("ready (%zu PD entries)\n", ARRAY_SIZE(dsp_pd_kill_entries));
	return 0;

	err_release:
	dsp_pd_kill_release_entries();
	debugfs_remove_recursive(dsp_pd_kill_root);
	dsp_pd_kill_root = NULL;
	return ret;
}

static void __exit dsp_pd_kill_exit(void)
{
	debugfs_remove_recursive(dsp_pd_kill_root);
	dsp_pd_kill_root = NULL;
	dsp_pd_kill_release_entries();
}

#else /* !CONFIG_DEBUG_FS */

static int __init dsp_pd_kill_init(void)
{
	pr_info("CONFIG_DEBUG_FS disabled, skipping\n");
	return 0;
}

static void __exit dsp_pd_kill_exit(void)
{
}

#endif /* CONFIG_DEBUG_FS */

module_init(dsp_pd_kill_init);
module_exit(dsp_pd_kill_exit);

MODULE_AUTHOR("Meta Platforms, Inc.");
MODULE_DESCRIPTION("DSP per-user-PD kill/restart debugfs hooks");
MODULE_LICENSE("GPL v2");

// SPDX-License-Identifier: GPL+
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pm_wakeup.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/version.h>

#define POWERSTATE_MGR_DEVICE_NAME "powerstate_mgr0"
#define POWERSTATE_MGR_WS_NAME "powerstate-mgr"

struct powerstate_mgr_drv_data {
	struct wakeup_source *ws;
	struct miscdevice misc;
	int refcount;
	struct mutex state_mutex;
};

static struct powerstate_mgr_drv_data *powerstate_mgr;

static int powerstate_mgr_open(struct inode *inode, struct file *f)
{
	int status;
	struct powerstate_mgr_drv_data *drvdata = container_of(f->private_data, struct powerstate_mgr_drv_data, misc);

	status = mutex_lock_interruptible(&drvdata->state_mutex);
	if (status) {
		pr_warn("%s failed, refcount=%d. status=%d (pid %d)", __func__, drvdata->refcount, status, current->pid);
		return status;
	}

	if (drvdata->refcount == 0)
		__pm_stay_awake(drvdata->ws);

	++drvdata->refcount;

	pr_info("%s succeeded, refcount=%d. (pid %d)", __func__, drvdata->refcount, current->pid);

	mutex_unlock(&drvdata->state_mutex);

	return status;
}

static int powerstate_mgr_release(struct inode *inode, struct file *f)
{
	int status;
	struct powerstate_mgr_drv_data *drvdata = container_of(f->private_data, struct powerstate_mgr_drv_data, misc);

	status = mutex_lock_interruptible(&drvdata->state_mutex);
	if (status) {
		pr_warn("%s failed, refcount=%d. status=%d (pid %d)", __func__, drvdata->refcount, status, current->pid);
		return status;
	}

	--drvdata->refcount;
	if (drvdata->refcount == 0)
		__pm_relax(drvdata->ws);

	pr_info("%s succeeded, refcount=%d. (pid %d)", __func__, drvdata->refcount, current->pid);

	mutex_unlock(&drvdata->state_mutex);

	return status;
}

static const struct file_operations fops = {
	.open = powerstate_mgr_open,
	.release = powerstate_mgr_release,
};

static int __init powerstate_mgr_init(void)
{
	struct powerstate_mgr_drv_data *drvdata;
	int status = 0;

	drvdata = kzalloc(sizeof(struct powerstate_mgr_drv_data), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	mutex_init(&drvdata->state_mutex);

	drvdata->misc.name = POWERSTATE_MGR_DEVICE_NAME;
	drvdata->misc.minor = MISC_DYNAMIC_MINOR;
	drvdata->misc.fops = &fops;

	status = misc_register(&drvdata->misc);
	if (status < 0) {
		pr_err("%s failed to register misc device, error %d",
			__func__, status);
		goto drvdata_free;
	}

	drvdata->ws = wakeup_source_register(NULL, POWERSTATE_MGR_WS_NAME);
	if (drvdata->ws == NULL) {
		pr_err("%s failed to register wakesource", __func__);
		status = -ENOMEM;
		goto misc_deregister;
	}

	powerstate_mgr = drvdata;

misc_deregister:
	if (status < 0)
		misc_deregister(&drvdata->misc);
drvdata_free:
	if (status < 0) {
		mutex_destroy(&drvdata->state_mutex);
		kfree(drvdata);
	}
	return status;
}

static void __exit powerstate_mgr_exit(void)
{
	struct powerstate_mgr_drv_data *drvdata = powerstate_mgr;

	if (drvdata->refcount > 0) {
		__pm_relax(drvdata->ws);
		drvdata->refcount = 0;
	}

	wakeup_source_unregister(drvdata->ws);

	misc_deregister(&drvdata->misc);
	mutex_destroy(&drvdata->state_mutex);
	kfree(drvdata);
	powerstate_mgr = NULL;
}

module_init(powerstate_mgr_init);
module_exit(powerstate_mgr_exit);
MODULE_DESCRIPTION("POWERSTATE_MANAGER");
MODULE_LICENSE("GPL v2");

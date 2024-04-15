// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/syncboss/consumer.h>
#include <uapi/linux/syncboss.h>

#include "syncboss_nsync.h"

#define NSYNC_MISCFIFO_SIZE 256
#define NSYNC_DEVICE_NAME "syncboss_nsync0"

static irqreturn_t isr_primary_nsync(int irq, void *p)
{
	struct nsync_dev_data *devdata = (struct nsync_dev_data *)p;
	unsigned long flags;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->nsync_irq_timestamp = ktime_get_ns();
	++devdata->nsync_irq_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_nsync(int irq, void *p)
{
	struct nsync_dev_data *devdata = (struct nsync_dev_data *)p;
	struct syncboss_nsync_event event;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	event.timestamp = devdata->nsync_irq_timestamp;
	event.count = devdata->nsync_irq_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	status = miscfifo_send_buf(
			&devdata->nsync_fifo,
			(void *)&event,
			sizeof(event));
	if (status < 0)
		dev_warn_ratelimited(devdata->dev,
				"nsync fifo send failure: %d\n", status);

	return IRQ_HANDLED;
}

static int syncboss_nsync_open(struct inode *inode, struct file *f)
{
	int status;
	struct nsync_dev_data *devdata =
		container_of(f->private_data, struct nsync_dev_data,
			     misc_nsync);

	status = mutex_lock_interruptible(&devdata->miscdevice_mutex);
	if (status) {
		dev_warn(devdata->dev, "nsync open by %s (%d) borted due to signal. status=%d",
			       current->comm, current->pid, status);
		return status;
	}

	if (devdata->nsync_client_count != 0) {
		dev_err(devdata->dev, "nsync open by %s (%d) failed: nsync cannot be opened by more than one user",
				current->comm, current->pid);
		status = -EBUSY;
		goto unlock;
	}

	++devdata->nsync_client_count;

	status = miscfifo_fop_open(f, &devdata->nsync_fifo);
	if (status < 0) {
		dev_err(devdata->dev, "nsync miscfifo open by %s (%d) failed (%d)",
			current->comm, current->pid, status);
		goto decrement_ref;
	}

	devdata->nsync_irq_timestamp = 0;
	devdata->nsync_irq_count = 0;
	irq_set_status_flags(devdata->nsync_irq, IRQ_DISABLE_UNLAZY);
	status = devm_request_threaded_irq(
		devdata->dev, devdata->nsync_irq, isr_primary_nsync,
		isr_thread_nsync, IRQF_TRIGGER_RISING,
		devdata->misc_nsync.name, devdata);

	if (status < 0) {
		dev_err(devdata->dev, "nsync irq request by %s (%d) failed (%d)",
			current->comm, current->pid, status);
		goto miscfifo_release;
	}

	dev_info(devdata->dev, "nsync handle opened by %s (%d)",
		 current->comm, current->pid);

miscfifo_release:
	if (status < 0)
		miscfifo_fop_release(inode, f);
decrement_ref:
	if (status < 0)
		--devdata->nsync_client_count;
unlock:
	mutex_unlock(&devdata->miscdevice_mutex);
	return status;
}

static int syncboss_nsync_release(struct inode *inode, struct file *file)
{
	int status;
	struct miscfifo_client *client = file->private_data;
	struct nsync_dev_data *devdata =
		container_of(client->mf, struct nsync_dev_data, nsync_fifo);

	/*
	 * It is unsafe to use the interruptible variant here, as the driver can get
	 * in an inconsistent state if this lock fails. We need to make sure release
	 * handling occurs, since we can't retry releasing the file if, e.g., the
	 * file is released when a process is killed.
	 */
	mutex_lock(&devdata->miscdevice_mutex);

	--devdata->nsync_client_count;
	if (devdata->nsync_client_count == 0)
		devm_free_irq(devdata->dev, devdata->nsync_irq, devdata);

	miscfifo_clear(&devdata->nsync_fifo);

	status = miscfifo_fop_release(inode, file);
	if (status) {
		dev_err(devdata->dev, "nsync release by %s (%d) failed with status %d",
				current->comm, current->pid, status);
		goto out;
	}

	dev_info(devdata->dev, "nsync released by %s (%d)",
		 current->comm, current->pid);

out:
	mutex_unlock(&devdata->miscdevice_mutex);
	return status;
}

static const struct file_operations nsync_fops = {
	.open = syncboss_nsync_open,
	.release = syncboss_nsync_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static int syncboss_nsync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret = 0;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct nsync_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);
	devdata->dev = dev;

	mutex_init(&devdata->miscdevice_mutex);
	spin_lock_init(&devdata->nsync_lock);

	devdata->nsync_irq = platform_get_irq_byname(pdev, "nsync");
	if (devdata->nsync_irq < 0) {
		dev_err(dev, "No nsync IRQ specified");
		return -EINVAL;
	}

	devdata->nsync_fifo.config.kfifo_size = NSYNC_MISCFIFO_SIZE;
	ret = devm_miscfifo_register(dev, &devdata->nsync_fifo);
	if (ret < 0)
		dev_err(dev, "failed to set up nsync miscfifo, error %d", ret);

	devdata->misc_nsync.name = NSYNC_DEVICE_NAME;
	devdata->misc_nsync.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_nsync.fops = &nsync_fops;

	ret = misc_register(&devdata->misc_nsync);
	if (ret < 0)
		dev_err(dev, "failed to register nsync misc device, error %d", ret);

	return ret;
}

static int syncboss_nsync_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nsync_dev_data *devdata = dev_get_drvdata(dev);

	misc_deregister(&devdata->misc_nsync);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_nsync_match_table[] = {
	{ .compatible = "meta,syncboss-nsync", },
	{ },
};
#else
#define syncboss_nsync_match_table NULL
#endif

struct platform_driver syncboss_nsync_driver = {
	.driver = {
		.name = "syncboss_nsync",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_nsync_match_table
	},
	.probe = syncboss_nsync_probe,
	.remove = syncboss_nsync_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_nsync_driver,
};

static int __init syncboss_nsync_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_nsync_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_nsync_init);
module_exit(syncboss_nsync_exit);
MODULE_DESCRIPTION("Syncboss Nsync Event Driver");
MODULE_LICENSE("GPL v2");

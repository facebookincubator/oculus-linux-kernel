// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2023 The Linux Foundation. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/timekeeping.h>
#include <linux/time_namespace.h>

#define FSYNC_CIRC_BUF_SIZE 16

#define CAMFSYNC_DEV "camfsync"

#define CAMFSYNC_NUM_MINOR 2

struct cam_fsync_item {
	int64_t ts;
};

struct cam_fsync_circ_buf {
	struct cam_fsync_item buf[FSYNC_CIRC_BUF_SIZE];
	int head;
	int tail;
};

struct cam_fsync_client {
	struct cam_fsync_circ_buf circ_buf;
	struct list_head node;
	wait_queue_head_t wq;
};

struct cam_fsync_cdev {
	struct device *device;
	dev_t devno;
	struct cdev cdev;
	struct list_head client_list;
	spinlock_t client_list_lock;
};

#define circ_cnt(__c__) \
	(CIRC_CNT((__c__)->head, (__c__)->tail, FSYNC_CIRC_BUF_SIZE))

static struct class *cam_fsync_class;
static int cam_fsync_major;
static DEFINE_IDA(cam_fsync_minors);

static int cam_fsync_open(struct inode *inode, struct file *filp)
{
	struct cam_fsync_client *client;
	struct cam_fsync_cdev *fsync_cdev =
		container_of(inode->i_cdev, struct cam_fsync_cdev, cdev);

	client = kzalloc(sizeof(struct cam_fsync_client),
			GFP_KERNEL | __GFP_NOWARN);
	if (!client)
		return -ENOMEM;

	init_waitqueue_head(&client->wq);

	filp->private_data = client;

	spin_lock(&fsync_cdev->client_list_lock);
	list_add_tail_rcu(&client->node, &fsync_cdev->client_list);
	spin_unlock(&fsync_cdev->client_list_lock);

	return 0;
}

static int cam_fsync_release(struct inode *inode, struct file *filp)
{
	struct cam_fsync_client *client;
	struct cam_fsync_cdev *fsync_cdev =
		container_of(inode->i_cdev, struct cam_fsync_cdev, cdev);

	client = filp->private_data;

	if (client) {
		/* Remove client from list */
		spin_lock(&fsync_cdev->client_list_lock);
		list_del_rcu(&client->node);
		spin_unlock(&fsync_cdev->client_list_lock);
		synchronize_rcu();

		kfree(client);
	}

	return 0;
}

/* return only one item */
static ssize_t cam_fsync_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos)
{
	struct cam_fsync_client *client = filp->private_data;
	struct cam_fsync_circ_buf *circ_buf = &client->circ_buf;
	struct cam_fsync_item *item;
	int head;
	int tail;
	int ret;
	unsigned long missing;
	ssize_t status = -EFAULT;

	if (count < sizeof(struct cam_fsync_item))
		return -EFAULT;

start_read:
	head = smp_load_acquire(&circ_buf->head);
	tail = circ_buf->tail;

	if (CIRC_CNT(head, tail, FSYNC_CIRC_BUF_SIZE) == 0) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(client->wq,
					circ_cnt(circ_buf)); // TODO: use smp_load_acquire...?
		if (ret == -ERESTARTSYS)
			return -ERESTARTSYS;

		goto start_read;
	}

	item = &circ_buf->buf[circ_buf->tail];

	/* consume */
	status = sizeof(*item);
	missing = copy_to_user(buf, item, status);
	if (missing == status)
		status = -EFAULT;
	else
		status -= missing;

	smp_store_release(&circ_buf->tail,
			(tail + 1) & (FSYNC_CIRC_BUF_SIZE - 1));

	return status;
}

static unsigned int cam_fsync_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	struct cam_fsync_client *client = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &client->wq, wait);
	if (circ_cnt(&client->circ_buf))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations fsync_fops = {
	.owner = THIS_MODULE,
	.open = cam_fsync_open,
	.release = cam_fsync_release,
	.poll = cam_fsync_poll,
	.read = cam_fsync_read,
};

struct cam_fsync_ctx {
	struct device *dev;
	atomic64_t last_timestamp;
	int fsync_gpio;
	int irq;
	int configured;
	struct kernfs_node *timestamp_kn;

	bool cdev_support;
	struct cam_fsync_cdev fsync_cdev;
};

static void cam_fsync_push(struct cam_fsync_ctx *ctx, int64_t ts)
{
	struct cam_fsync_client *client;

	/* Loop the client list */
	list_for_each_entry_rcu(client, &ctx->fsync_cdev.client_list, node) {
		struct cam_fsync_circ_buf *circ_buf = &client->circ_buf;
		int head = circ_buf->head;
		int tail = READ_ONCE(circ_buf->tail);

		if (CIRC_SPACE(head, tail, FSYNC_CIRC_BUF_SIZE) >= 1) {
			struct cam_fsync_item *item = &circ_buf->buf[head];

			item->ts = ts;

			smp_store_release(&circ_buf->head,
				(head + 1) & (FSYNC_CIRC_BUF_SIZE - 1));

			wake_up(&client->wq);
		}
	}
}

static int cam_fsync_cdev_init(struct cam_fsync_ctx *ctx, const char *dev_name)
{
	int rc;
	int minor;

	minor = ida_simple_get(&cam_fsync_minors, 0, CAMFSYNC_NUM_MINOR, GFP_KERNEL);
	if (minor < 0) {
		rc = minor;
		goto error_ida;
	}

	ctx->fsync_cdev.devno = MKDEV(cam_fsync_major, minor);

	cdev_init(&ctx->fsync_cdev.cdev, &fsync_fops);
	ctx->fsync_cdev.cdev.owner = THIS_MODULE;
	rc = cdev_add(&ctx->fsync_cdev.cdev, ctx->fsync_cdev.devno, 1);
	if (rc < 0) {
		dev_err(ctx->dev, "cdev_add failed %d\n", rc);
		goto cdev_add_fail;
	}

	ctx->fsync_cdev.device = device_create(cam_fsync_class, NULL,
				ctx->fsync_cdev.devno, NULL, "%s", dev_name);

	if (IS_ERR(ctx->fsync_cdev.device)) {
		rc = PTR_ERR(ctx->fsync_cdev.device);
		dev_err(ctx->dev, "device_create failed %d\n", rc);
		goto device_create_fail;
	}

	INIT_LIST_HEAD(&ctx->fsync_cdev.client_list);
	spin_lock_init(&ctx->fsync_cdev.client_list_lock);

	dev_info(ctx->dev, "camfsync device created!\n");

	return 0;

device_create_fail:
	cdev_del(&ctx->fsync_cdev.cdev);

cdev_add_fail:
	ida_simple_remove(&cam_fsync_minors, minor);

error_ida:
	return rc;
}

static ssize_t fsync_timestamp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct cam_fsync_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			 atomic64_read(&ctx->last_timestamp));
}

static DEVICE_ATTR_RO(fsync_timestamp);

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define QTIMER_MUL_FACTOR   10000
#define QTIMER_DIV_FACTOR   192

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct cam_fsync_ctx *ctx = (struct cam_fsync_ctx *)dev_id;
	int64_t timestamp;
	uint64_t ticks;

	ticks = arch_timer_read_counter();
	if (ticks == 0) {
		dev_err(ctx->dev, "qtimer returned 0\n");
		return IRQ_HANDLED;
	}

	timestamp = mul_u64_u32_div(ticks,
			QTIMER_MUL_FACTOR, QTIMER_DIV_FACTOR);

	atomic64_set(&ctx->last_timestamp, timestamp);
	sysfs_notify_dirent(ctx->timestamp_kn);

	if (ctx->cdev_support)
		cam_fsync_push(ctx, timestamp);

	return IRQ_HANDLED;
}

static int set_input_pinctrl(struct cam_fsync_ctx *ctx)
{
	int rc = -EINVAL;
	struct device *dev = ctx->dev;
	struct pinctrl_state *pin_input;
	struct pinctrl *pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(dev, "failed to get pinctrl\n");
		return rc;
	}

	pin_input = pinctrl_lookup_state(pinctrl, "input");
	if (IS_ERR_OR_NULL(pin_input)) {
		dev_err(dev, "failed to look up input pin state\n");
		goto done;
	}

	if (pinctrl_select_state(pinctrl, pin_input)) {
		dev_err(dev, "failed to set to input pin state\n");
		goto done;
	}
	rc = 0;

done:
	devm_pinctrl_put(pinctrl);
	return rc;
}

static int configure_intr_gpio(struct cam_fsync_ctx *ctx)
{
	int rc;
	struct device *dev = ctx->dev;

	rc = devm_gpio_request(dev, ctx->fsync_gpio, "cam_fsync");
	if (rc) {
		dev_err(dev, "Failed to request GPIO rc = %d\n", rc);
		return -EINVAL;
	}

	rc = gpio_to_irq(ctx->fsync_gpio);
	if (rc < 0) {
		dev_err(dev, "Failed get IRQ number rc = %d\n", rc);
		goto free_gpio;
	}

	ctx->irq = rc;

	rc = devm_request_irq(dev, ctx->irq, (irq_handler_t)irq_handler,
			      IRQF_TRIGGER_RISING, "cam_fsync", ctx);
	if (rc) {
		dev_err(dev, "Failed request IRQ rc = %d\n", rc);
		goto free_gpio;
	}

	ctx->configured = 1;
	dev_info(dev, "cam_fsync IRQ = %d\n", ctx->irq);

	return 0;

free_gpio:
	devm_gpio_free(dev, ctx->fsync_gpio);

	return rc;
}

static int configure_input_and_intr(struct cam_fsync_ctx *ctx)
{
	int rc;

	rc = set_input_pinctrl(ctx);
	if (!rc)
		rc = configure_intr_gpio(ctx);

	return rc;
}

static ssize_t fsync_config_intr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct cam_fsync_ctx *ctx = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1) && !ctx->configured) {
		configure_input_and_intr(ctx);
	} else {
		if (ctx->configured)
			dev_err(ctx->dev,
				"The intr already configured\n");
		else
			dev_err(ctx->dev,
				"Writing 0 is not supported\n");
	}

	return count;
}

static DEVICE_ATTR_WO(fsync_config_intr);

static int cam_fsync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cam_fsync_ctx *ctx =
		devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	ctx->fsync_gpio = of_get_named_gpio(
		dev->of_node, "fsync-gpio", 0);

	if (!gpio_is_valid(ctx->fsync_gpio)) {
		dev_err(dev, "FSYNC GPIO not valid\n");
		rc = ctx->fsync_gpio;
		goto free_context;
	}

	if (of_find_property(dev->of_node, "delayed-intr-config", NULL)) {
		rc = device_create_file(dev, &dev_attr_fsync_config_intr);
		if (rc) {
			dev_err(dev, "Cannot create fsync_config_intr\n");
			goto free_context;
		}
	} else {
		rc = configure_input_and_intr(ctx);
		if (rc)
			goto free_context;
	}

	/*
	 * Initialize Character device if necessary to support
	 * circular buffered fsync which can prevent unexpected
	 * timestamp loss when there's latency on user space for
	 * reading timestamp
	 */
	ctx->cdev_support = of_property_read_bool(dev->of_node,
					"cam-fsync-cdev-support");
	if (ctx->cdev_support) {
		const char* cdev_name = NULL;

		of_property_read_string(ctx->dev->of_node,
				"cam-fsync-dev-name", &cdev_name);

		if (cdev_name == NULL)
			cdev_name = CAMFSYNC_DEV;

		rc = cam_fsync_cdev_init(ctx, cdev_name);
		if (rc)
			goto free_context;
	}

	platform_set_drvdata(pdev, ctx);

	atomic64_set(&ctx->last_timestamp, 0LL);

	rc = device_create_file(dev, &dev_attr_fsync_timestamp);
	if (rc) {
		dev_err(dev, "Cannot create fsync_timestamp\n");
		goto free_context;
	}

	ctx->timestamp_kn = sysfs_get_dirent(dev->kobj.sd, "fsync_timestamp");

	dev_info(dev, "cam_fsync probe success.\n");

	return rc;

free_context:
	devm_kfree(dev, ctx);

	return rc;
}

static int cam_fsync_remove(struct platform_device *pdev)
{
	struct cam_fsync_ctx *ctx = platform_get_drvdata(pdev);

	if (ctx->cdev_support) {
		device_destroy(cam_fsync_class, ctx->fsync_cdev.devno);
		cdev_del(&ctx->fsync_cdev.cdev);
	}

	devm_kfree(ctx->dev, ctx);

	return 0;
}

static const struct of_device_id cam_fsync_of_match[] = {
	{
		.compatible = "oculus,cam_fsync",
	},
	{},
};

static struct platform_driver cam_fsync_driver = {
	.driver = {
		.name = "oculus,cam_fsync",
		.of_match_table = cam_fsync_of_match,
	},
	.probe = cam_fsync_probe,
	.remove = cam_fsync_remove,
};

static int __init cam_fsync_driver_init(void)
{
	dev_t dev;
	int rc = 0;

	cam_fsync_class = class_create(THIS_MODULE, CAMFSYNC_DEV);
	if (IS_ERR(cam_fsync_class)) {
		rc = PTR_ERR(cam_fsync_class);
		pr_err("class_create failed %d\n", rc);
		goto class_fail;
	}

	rc = alloc_chrdev_region(&dev, 0,
			CAMFSYNC_NUM_MINOR, CAMFSYNC_DEV);
	if (rc < 0) {
		pr_err("create char device failed %d\n", rc);
		goto alloc_fail;
	}

	cam_fsync_major = MAJOR(dev);

	rc = platform_driver_register(&cam_fsync_driver);
	if (rc) {
		pr_err("Unable to register cam fsync driver:%d\n", rc);
		goto register_fail;
	}

	return 0;

register_fail:
	unregister_chrdev_region(dev, CAMFSYNC_NUM_MINOR);
alloc_fail:
	class_destroy(cam_fsync_class);
class_fail:
	return rc;
}

static void __exit cam_fsync_driver_exit(void)
{
	platform_driver_unregister(&cam_fsync_driver);
	unregister_chrdev_region(MKDEV(cam_fsync_major, 0), CAMFSYNC_NUM_MINOR);
	class_destroy(cam_fsync_class);
	ida_destroy(&cam_fsync_minors);
}

module_init(cam_fsync_driver_init);
module_exit(cam_fsync_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for Camera Fsync");

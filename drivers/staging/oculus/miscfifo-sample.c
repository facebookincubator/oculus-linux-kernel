#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/miscfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>

static struct miscdevice misc;
static struct miscfifo mf;

static struct {
	struct mutex lock;
	unsigned int sleep_us;
	struct task_struct *thread;
} thread_state;


static int threadfn(void *data)
{
	unsigned int counter = 0;
	u8 buf[80] = {};

	while (!kthread_should_stop()) {
		int n = snprintf(buf, sizeof(buf) - 1,
			"<counter %u>\n", counter++);
		usleep_range(thread_state.sleep_us, thread_state.sleep_us + 1);
		miscfifo_send_buf(&mf, buf, n);
	}

	return 0;
}

static ssize_t counter_rate(struct device *_dev,
	struct device_attribute *attr, const char *buf, size_t len)
{

	unsigned long rate;
	int rc = kstrtoul(buf, 0, &rate);
	bool thread_running;

	if (rc < 0)
		return rc;

	mutex_lock(&thread_state.lock);
	thread_running = thread_state.sleep_us > 0;
	if (!thread_running) {
		if (rate) {
			thread_state.sleep_us = 1000000 / rate;
			thread_state.thread = kthread_run(
				threadfn, NULL,
				"miscfifo-sample-counter-thread");

			if (IS_ERR(thread_state.thread)) {
				thread_state.sleep_us = 0;
				rc = PTR_ERR(thread_state.thread);
				pr_err("error creating thread: %d\n", rc);
				goto exit_unlock;
			}
		}
	} else {
		if (!rate) {
			thread_state.sleep_us = 0;
			rc = kthread_stop(thread_state.thread);
			if (rc < 0) {
				pr_err("error stopping thread: %d\n", rc);
				goto exit_unlock;
			}
		} else {
			thread_state.sleep_us = 1000000 / rate;
		}
	}

	rc = len;

exit_unlock:
	mutex_unlock(&thread_state.lock);

	return rc;
}

static ssize_t enqueue(struct device *_dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	int rc;

	rc = miscfifo_send_buf(&mf, buf, len);
	if (rc < 0) {
		pr_err("send failed: %d\n", rc);
		return rc;
	}

	return len;
}

static DEVICE_ATTR(counter_rate, S_IWUSR, NULL, counter_rate);
static DEVICE_ATTR(enqueue, S_IWUSR, NULL, enqueue);

static struct attribute *attrs[] = {
	&dev_attr_counter_rate.attr,
	&dev_attr_enqueue.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attrs
};

int mf_sample_fop_open(struct inode *inode, struct file *file)
{
	return miscfifo_fop_open(file, &mf);
}

static const struct file_operations sample_fops = {
	.owner = THIS_MODULE,
	.open = mf_sample_fop_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll,
};

static int __init mf_sample_init(void)
{
	int rc;

	misc.minor = MISC_DYNAMIC_MINOR;
	misc.name = "miscfifo_sample";
	misc.fops = &sample_fops;

	rc = misc_register(&misc);
	if (rc)
		return rc;

	mf.config.kfifo_size = 1024;
	rc = miscfifo_register(&mf);
	if (rc)
		pr_err("miscfifo_register error %d\n", rc);

	rc = sysfs_create_group(&misc.this_device->kobj, &attr_group);
	if (rc)
		pr_err("error %d registering attrs\n", rc);

	mutex_init(&thread_state.lock);

	return rc;
}

static void __exit mf_sample_exit(void)
{
	sysfs_remove_group(&misc.this_device->kobj, &attr_group);
	if (thread_state.sleep_us)
		kthread_stop(thread_state.thread);
	mutex_destroy(&thread_state.lock);
	miscfifo_destroy(&mf);
	misc_deregister(&misc);
}

module_init(mf_sample_init);
module_exit(mf_sample_exit);

MODULE_AUTHOR("Khalid Zubair <kzubair@oculus.com>");
MODULE_DESCRIPTION("Example miscfifo chardev implementation");
MODULE_LICENSE("GPL");

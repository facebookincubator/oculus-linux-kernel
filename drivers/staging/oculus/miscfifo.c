#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscfifo.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#define REC_MAX_LENGTH	0xff
#define REC_SIZE	1
#define MIN_FIFO_SIZE	(REC_MAX_LENGTH + REC_SIZE)
#define CLIENT_NAME_SIZE 64

ssize_t miscfifo_fop_read(struct file *file,
	char __user *buf, size_t len, loff_t *off)
{
	ssize_t rc;
	unsigned int copied;
	struct miscfifo_client *client = file->private_data;

	mutex_lock(&client->consumer_lock);
	if (kfifo_is_empty(&client->fifo)) {
		if (file->f_flags & O_NONBLOCK) {
			rc = -EAGAIN;
			goto exit_unlock;
		}

		mutex_unlock(&client->consumer_lock);
		rc = wait_event_interruptible(client->mf->clients.wait,
					      !kfifo_is_empty(&client->fifo));
		if (rc)
			return rc;
		mutex_lock(&client->consumer_lock);
	}

	if (kfifo_peek_len(&client->fifo) <= len) {
		rc = kfifo_to_user(&client->fifo, buf, len, &copied);
		rc = (rc == 0) ? copied : -EFAULT;
	} else
		rc = -ENOBUFS;
exit_unlock:
	mutex_unlock(&client->consumer_lock);

	return rc;
}
EXPORT_SYMBOL(miscfifo_fop_read);

ssize_t miscfifo_fop_read_many(struct file *file,
	char __user *buf, size_t len, loff_t *off)
{
	ssize_t rc;
	unsigned int total_copied = 0;
	struct miscfifo_client *client = file->private_data;

	mutex_lock(&client->consumer_lock);
	if (kfifo_is_empty(&client->fifo)) {
		if (file->f_flags & O_NONBLOCK) {
			rc = -EAGAIN;
			goto exit_unlock;
		}

		mutex_unlock(&client->consumer_lock);
		rc = wait_event_interruptible(client->mf->clients.wait,
					      !kfifo_is_empty(&client->fifo));
		if (rc)
			return rc;
		mutex_lock(&client->consumer_lock);
	}

	if (kfifo_peek_len(&client->fifo) > len) {
		rc = -ENOBUFS;
		goto exit_unlock;
	}

	while (true) {
		unsigned int copied;

		/*
		 * Check whether there is data in the queue, or whether the
		 * next record might not fit into the user buffer. If either
		 * condition is true, we will exit the loop and return the
		 * data we have copied out so far.
		 *
		 * Note: kfifo_peek_len might return a garbage value if
		 * the fifo is empty, hence we need the explicit check whether
		 * the fifo is empty, so that we don't copy out zero bytes
		 * in kfifo_to_user.
		 */
		if (kfifo_is_empty(&client->fifo) || kfifo_peek_len(&client->fifo) > len)
			break;
		rc = kfifo_to_user(&client->fifo, buf, len, &copied);
		if (rc != 0)
			break;

		total_copied += copied;
		buf += copied;
		len -= copied;
	}
	rc = total_copied;
exit_unlock:
	mutex_unlock(&client->consumer_lock);

	return rc;
}
EXPORT_SYMBOL(miscfifo_fop_read_many);

unsigned int miscfifo_fop_poll(struct file *file,
	struct poll_table_struct *pt)
{
	unsigned int mask = 0;
	struct miscfifo_client *client = file->private_data;

	if (kfifo_len(&client->fifo))
		mask |= POLLIN;

	if (!mask) {
		poll_wait(file, &client->mf->clients.wait, pt);
		if (kfifo_len(&client->fifo))
			mask |= POLLIN;
	}

	return mask;
}
EXPORT_SYMBOL(miscfifo_fop_poll);

int miscfifo_fop_release(struct inode *inode, struct file *file)
{
	struct miscfifo_client *client = file->private_data;
	struct miscfifo *mf = client->mf;

	down_write(&mf->clients.rw_lock);
	list_del(&client->node);
	up_write(&mf->clients.rw_lock);

	mutex_destroy(&client->context_lock);
	mutex_destroy(&client->producer_lock);
	mutex_destroy(&client->consumer_lock);

	kfifo_free(&client->fifo);
	kfree(client->name);
	kfree(client);

	return 0;
}
EXPORT_SYMBOL(miscfifo_fop_release);

static void get_current_client_name(char *buf, size_t size)
{
	struct mm_struct *mm;
	struct rw_semaphore *rw_sem;

	mm = current->mm;
	if (!mm)
		goto error;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0))
	rw_sem = &mm->mmap_lock;
#else
	rw_sem = &mm->mmap_sem;
#endif

	down_read(rw_sem);
	if (!mm->exe_file) {
		up_read(rw_sem);
		goto error;
	}
	snprintf(buf, size, "%s (%d)", mm->exe_file->f_path.dentry->d_name.name, current->pid);
	up_read(rw_sem);

	return;

error:
	snprintf(buf, size, "(unknown)");
}

int miscfifo_fop_open(struct file *file, struct miscfifo *mf)
{
	int rc;
	struct miscfifo_client *client = kzalloc(sizeof(*client), GFP_KERNEL);

	if (!client) {
		rc = -ENOMEM;
		goto exit;
	}

	client->mf = mf;
	rc = kfifo_alloc(&client->fifo, mf->config.kfifo_size, GFP_KERNEL);
	if (rc)
		goto exit_free_client;

	mutex_init(&client->consumer_lock);
	mutex_init(&client->producer_lock);
	mutex_init(&client->context_lock);

	client->name = kmalloc(CLIENT_NAME_SIZE, GFP_KERNEL);
	if (!client->name) {
		rc = -ENOMEM;
		goto exit_free_kfifo;
	}
	get_current_client_name(client->name, CLIENT_NAME_SIZE);

	client->file = file;
	file->private_data = client;

	down_write(&mf->clients.rw_lock);
	list_add(&client->node, &mf->clients.list);
	up_write(&mf->clients.rw_lock);

	return 0;

exit_free_kfifo:
	kfree(&client->fifo);
exit_free_client:
	kfree(client);
exit:
	return rc;
}
EXPORT_SYMBOL(miscfifo_fop_open);

int miscfifo_send_buf(struct miscfifo *mf, const u8 *buf, size_t len)
{
	struct miscfifo_client *client;
	int rc = 0;
	/* space needed in the fifo */
	size_t needed = len + REC_SIZE;

	if (WARN_ON(len == 0 || len > REC_MAX_LENGTH))
		return -EINVAL;

	down_read(&mf->clients.rw_lock);
	list_for_each_entry(client, &mf->clients.list, node) {
		bool skip;

		mutex_lock(&client->context_lock);
		skip = mf->config.filter_fn &&
			!mf->config.filter_fn(client->context, NULL, 0, buf, len);
		mutex_unlock(&client->context_lock);

		if (skip)
			continue;

		mutex_lock(&client->producer_lock);
		if (kfifo_avail(&client->fifo) >= needed) {
			client->logged_fifo_full = false;
			kfifo_in(&client->fifo, buf, len);
		} else {
			if (!client->logged_fifo_full) {
				dev_info_ratelimited(mf->dev, "miscfifo %s opened by %s is full",
						     client->file->f_path.dentry ?
						     (char *)client->file->f_path.dentry->d_name.name :
						     "unknown",
						     client->name);
				client->logged_fifo_full = true;
			}
			rc++;
		}

		mutex_unlock(&client->producer_lock);
	}
	up_read(&mf->clients.rw_lock);

	wake_up_interruptible(&mf->clients.wait);
	return rc;
}
EXPORT_SYMBOL(miscfifo_send_buf);

void miscfifo_clear(struct miscfifo *mf)
{
	struct miscfifo_client *client;

	down_read(&mf->clients.rw_lock);
	list_for_each_entry(client, &mf->clients.list, node) {
		mutex_lock(&client->consumer_lock);
		mutex_lock(&client->producer_lock);

		kfifo_reset(&client->fifo);
		client->logged_fifo_full = false;

		mutex_unlock(&client->producer_lock);
		mutex_unlock(&client->consumer_lock);
	}
	up_read(&mf->clients.rw_lock);
}
EXPORT_SYMBOL(miscfifo_clear);

static int miscfifo_register(struct device *dev, struct miscfifo *mf)
{
	/* these min sizes are required to reliably put (max length) data
	 * in to the fifo */

	if (mf->config.kfifo_size < MIN_FIFO_SIZE) {
		dev_warn(dev, "expect min fifo size: %d", MIN_FIFO_SIZE);
		mf->config.kfifo_size = MIN_FIFO_SIZE;
	}

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	mf->dev = dev;
	init_rwsem(&mf->clients.rw_lock);
	INIT_LIST_HEAD(&mf->clients.list);
	init_waitqueue_head(&mf->clients.wait);
	return 0;
}

static void devm_miscfifo_release(struct device *dev, void *res)
{
	struct miscfifo *mf = *(struct miscfifo **)res;

	BUG_ON(!list_empty(&mf->clients.list));
	module_put(THIS_MODULE);
}

int devm_miscfifo_register(struct device *dev, struct miscfifo *mf)
{
	struct miscfifo **mfp;
	int rc;

	mfp = devres_alloc(devm_miscfifo_release, sizeof(*mfp), GFP_KERNEL);
	if (!mfp)
		return -ENOMEM;

	rc = miscfifo_register(dev, mf);
	if (!rc) {
		*mfp = mf;
		devres_add(dev, mfp);
	} else {
		devres_free(mfp);
	}

	return rc;
}
EXPORT_SYMBOL(devm_miscfifo_register);

static int devm_miscfifo_match(struct device *dev, void *res, void *data)
{
	struct mf *mf = res;

	if (WARN_ON(!mf))
		return 0;

	return mf == data;
}

void devm_miscfifo_unregister(struct device *dev, struct miscfifo *mf)
{
	WARN_ON(devres_release(dev, devm_miscfifo_release, devm_miscfifo_match, mf));
}
EXPORT_SYMBOL(devm_miscfifo_unregister);

void *miscfifo_fop_xchg_context(struct file *file, void *context)
{
	struct miscfifo_client *client = file->private_data;
	void *old = client->context;

	mutex_lock(&client->context_lock);
	client->context = context;
	mutex_unlock(&client->context_lock);

	return old;
}
EXPORT_SYMBOL(miscfifo_fop_xchg_context);


MODULE_AUTHOR("Khalid Zubair <kzubair@oculus.com>");
MODULE_DESCRIPTION("General purpose chardev fifo implementation");
MODULE_LICENSE("GPL");

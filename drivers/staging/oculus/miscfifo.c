#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscfifo.h>
#include <linux/module.h>
#include <linux/slab.h>

#define REC_MAX_LENGTH	0xff
#define REC_SIZE	1
#define MIN_FIFO_SIZE	(REC_MAX_LENGTH + REC_SIZE)

ssize_t miscfifo_fop_read(struct file *file,
	char __user *buf, size_t len, loff_t *off)
{
	ssize_t rc;
	struct miscfifo_client *client = file->private_data;

	mutex_lock(&client->lock);
	if (kfifo_is_empty(&client->fifo)) {
		if (file->f_flags & O_NONBLOCK) {
			rc = -EAGAIN;
			goto exit_unlock;
		}

		mutex_unlock(&client->lock);
		rc = wait_event_interruptible(client->mf->clients.wait,
					      !kfifo_is_empty(&client->fifo));
		if (rc)
			return rc;
		mutex_lock(&client->lock);
	}

	if (client->mf->config.header_payload) {
		/* read a |header| + |payload| record packed back to
		 * back in the fifo and output in a single buffer */
		unsigned int copied1;
		unsigned int copied2;

		if (kfifo_peek_len(&client->fifo) < len) {
			rc = kfifo_to_user(&client->fifo, buf, len, &copied1);
			if (rc != 0) {
				rc = -EFAULT;
				goto exit_unlock;
			}
		} else {
			rc = -ENOBUFS;
			goto exit_unlock;
		}

		buf += copied1;
		len -= copied1;
		if (kfifo_peek_len(&client->fifo) <= len) {
			rc = kfifo_to_user(&client->fifo, buf, len, &copied2);
			if (rc != 0) {
				/* since the header was read, we have to drop the
				 * payload part */
				kfifo_skip(&client->fifo);
				rc = -EFAULT;
				goto exit_unlock;
			}
		} else {
			rc = -ENOBUFS;
			goto exit_unlock;
		}

		rc = copied1 + copied2;
	} else {
		unsigned int copied;

		if (kfifo_peek_len(&client->fifo) <= len) {
			rc = kfifo_to_user(&client->fifo, buf, len, &copied);
			rc = (rc == 0) ? copied : -EFAULT;
		} else
			rc = -ENOBUFS;

	}
exit_unlock:
	mutex_unlock(&client->lock);

	return rc;
}
EXPORT_SYMBOL(miscfifo_fop_read);

unsigned int miscfifo_fop_poll(struct file *file,
	struct poll_table_struct *pt)
{
	unsigned int mask = 0;
	struct miscfifo_client *client = file->private_data;

	mutex_lock(&client->lock);
	if (kfifo_len(&client->fifo))
		mask |= POLLIN;
	mutex_unlock(&client->lock);

	if (!mask) {
		poll_wait(file, &client->mf->clients.wait, pt);
		mutex_lock(&client->lock);
		if (kfifo_len(&client->fifo))
			mask |= POLLIN;
		mutex_unlock(&client->lock);
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

	mutex_destroy(&client->lock);
	kfifo_free(&client->fifo);
	kfree(client);

	return 0;
}
EXPORT_SYMBOL(miscfifo_fop_release);

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

	mutex_init(&client->lock);

	down_write(&mf->clients.rw_lock);
	list_add(&client->node, &mf->clients.list);
	up_write(&mf->clients.rw_lock);

	file->private_data = client;
	return 0;

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

	if (WARN_ON(mf->config.header_payload))
		return -EINVAL;

	if (WARN_ON(len == 0 || len > REC_MAX_LENGTH))
		return -EINVAL;

	down_read(&mf->clients.rw_lock);
	list_for_each_entry(client, &mf->clients.list, node) {
		mutex_lock(&client->lock);

		if (!mf->config.filter_fn ||
		    mf->config.filter_fn(client->context,
					 NULL, 0,
					 buf, len)) {

			while (kfifo_avail(&client->fifo) < needed) {
				kfifo_skip(&client->fifo);
				rc++;
			}

			kfifo_in(&client->fifo, buf, len);
		}

		mutex_unlock(&client->lock);
	}
	up_read(&mf->clients.rw_lock);

	wake_up_interruptible(&mf->clients.wait);
	return rc;
}
EXPORT_SYMBOL(miscfifo_send_buf);

int miscfifo_send_header_payload(struct miscfifo *mf,
				 const u8 *header, size_t header_len,
				 const u8 *payload, size_t payload_len)
{
	struct miscfifo_client *client;
	int rc = 0;
	/* space needed in the fifo */
	size_t needed = header_len + REC_SIZE + payload_len + REC_SIZE;

	if (WARN_ON(!mf->config.header_payload))
		return -EINVAL;

	if (WARN_ON(header_len > REC_MAX_LENGTH))
		return -EINVAL;

	if (WARN_ON(payload_len == 0 || payload_len > REC_MAX_LENGTH))
		return -EINVAL;

	down_read(&mf->clients.rw_lock);
	list_for_each_entry(client, &mf->clients.list, node) {
		mutex_lock(&client->lock);

		if (!mf->config.filter_fn ||
		    mf->config.filter_fn(client->context,
					 header, header_len,
					 payload, payload_len)) {

			/* pack a |header| + |payload| record back to back
			 * in the fifo
			 */
			while (kfifo_avail(&client->fifo) < needed) {
				kfifo_skip(&client->fifo);
				kfifo_skip(&client->fifo);
				rc++;
			}

			kfifo_in(&client->fifo, header, header_len);
			kfifo_in(&client->fifo, payload, payload_len);
		}
		mutex_unlock(&client->lock);
	}
	up_read(&mf->clients.rw_lock);

	wake_up_interruptible(&mf->clients.wait);
	return rc;
}
EXPORT_SYMBOL(miscfifo_send_header_payload);

static int miscfifo_register(struct device *dev, struct miscfifo *mf)
{
	/* these min sizes are required to reliably put (max length) data
	 * in to the fifo */
	if (!mf->config.header_payload) {
		if (mf->config.kfifo_size < MIN_FIFO_SIZE) {
			dev_warn(dev, "expect min fifo size: %d", MIN_FIFO_SIZE);
			mf->config.kfifo_size = MIN_FIFO_SIZE;
		}
	} else {
		if (mf->config.kfifo_size < MIN_FIFO_SIZE * 2) {
			dev_warn(dev, "expect min fifo size: %d", MIN_FIFO_SIZE * 2);
			mf->config.kfifo_size = MIN_FIFO_SIZE * 2;
		}
	}

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

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

void miscfifo_fop_set_context(struct file *file, void *context)
{
	struct miscfifo_client *client = file->private_data;

	mutex_lock(&client->lock);
	client->context = context;
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(miscfifo_fop_set_context);

void *miscfifo_fop_get_context(struct file *file)
{
	struct miscfifo_client *client = file->private_data;

	return client->context;
}
EXPORT_SYMBOL(miscfifo_fop_get_context);

MODULE_AUTHOR("Khalid Zubair <kzubair@oculus.com>");
MODULE_DESCRIPTION("General purpose chardev fifo implementation");
MODULE_LICENSE("GPL");

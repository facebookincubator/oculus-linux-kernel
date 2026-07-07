// SPDX-License-Identifier: GPL-2.0
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/minmax.h>
#include <linux/module.h>

#include "fwupdate_operations.h"
#include "swd.h"
#include "swd_chardev.h"

struct cdev_data {
	struct device *dev; // this very device
	struct cdev cdev;
	dev_t devt;
	atomic_t in_use;
	size_t chunk_size;
	u8 *buf; // of chunk_size
	loff_t offset;
};

static struct class *cls;

static ssize_t device_write(struct file *filp, const char __user *buffer,
			    size_t length, loff_t *offset)
{
	size_t bytes_to_write = 0;
	int ret = 0;
	struct cdev_data *cdevdata = filp->private_data;
	struct device *swd_dev = cdevdata->dev->parent;

	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);

	if (length == 0)
		return 0;

	if (!devdata->mcu_data.swd_ops.target_program_write_chunk) {
		dev_err(swd_dev, "write is not implemented");
		return -ENOSYS;
	}

	bytes_to_write = min(length, cdevdata->chunk_size);

	if (copy_from_user(cdevdata->buf, buffer, bytes_to_write)) {
		dev_err(swd_dev, "failed to copy from user!");
		return -EFAULT;
	}

	dev_dbg(swd_dev, "write offset 0x%llx sz %zu", cdevdata->offset,
		bytes_to_write);
	ret = devdata->mcu_data.swd_ops.target_program_write_chunk(
		swd_dev, cdevdata->offset, cdevdata->buf, bytes_to_write);

	if (ret < 0) {
		dev_err(swd_dev, "write failed");
		return -EIO;
	}

	*offset += bytes_to_write;
	cdevdata->offset += bytes_to_write;
	return bytes_to_write;
}

static ssize_t device_read(struct file *filp, char __user *buffer,
			   size_t length, loff_t *offset)
{
	int ret = 0;
	struct cdev_data *cdevdata = filp->private_data;
	struct device *dev = cdevdata->dev;
	struct device *swd_dev = dev->parent;
	size_t bytes_to_read = 0;

	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);

	bytes_to_read = min(length, cdevdata->chunk_size);

	if (!devdata->mcu_data.swd_ops.target_program_read) {
		dev_err(swd_dev, "read is not implemented");
		return -ENOSYS;
	}

	ret = devdata->mcu_data.swd_ops.target_program_read(
		swd_dev, cdevdata->offset, cdevdata->buf, bytes_to_read);

	if (ret < 0) {
		dev_err(swd_dev, "read failed");
		return -EIO;
	}

	ret = copy_to_user(buffer, cdevdata->buf, bytes_to_read);
	if (ret) {
		dev_err(swd_dev, "failed to copy to user!");
		return ret;
	}

	*offset += bytes_to_read;

	return bytes_to_read;
}

static int device_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct cdev_data *cdevdata = container_of(cdev, struct cdev_data, cdev);
	struct device *dev, *swd_dev;
	struct swd_dev_data *devdata = NULL;
	int ret = 0;

	if (!cdevdata) {
		dev_err(NULL, "failed to open");
		ret = -ENODEV;
		goto err;
	}

	if (atomic_cmpxchg(&cdevdata->in_use, 0, 1) != 0)
		return -EBUSY;

	dev = cdevdata->dev;
	swd_dev = dev->parent;

	dev_info(dev, "opening..");

	filp->private_data = cdevdata;

	// treat O_NONBLOCK as signal to skip initializaiton
	if (filp->f_flags & O_NONBLOCK) {
		dev_info(dev, "Skip target prepare for nonblocking release!");
		return 0;
	}

	devdata = dev_get_drvdata(swd_dev);

	if (devdata->mcu_data.swd_ops.target_prepare) {
		ret = devdata->mcu_data.swd_ops.target_prepare(swd_dev);
		if (ret) {
			dev_err(dev, "target prepare failed!");
			goto err;
		}
	} else {
		swd_init(swd_dev);
		swd_halt(swd_dev);
	}

	return 0;

err:
	if (cdevdata)
		atomic_set(&cdevdata->in_use, 0);
	return ret;
}

static int device_release(struct inode *inode, struct file *filp)
{
	int ret;
	struct cdev_data *cdevdata = filp->private_data;
	struct device *dev = cdevdata->dev;
	struct device *swd_dev = dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);

	if (filp->f_flags & O_NONBLOCK) {
		dev_info(dev, "Skip target finalize for nonblocking release!");
		goto out;
	}

	if (devdata->mcu_data.swd_ops.target_finalize) {
		ret = devdata->mcu_data.swd_ops.target_finalize(swd_dev);
		if (ret)
			dev_err(dev, "finalize failed!");
		/* ignore the failure */
	} else {
		/* reset the mcu if no finalize operation present */
		swd_reset(dev);
		swd_flush(dev);
	}

out:
	dev_info(dev, "releasing");

	atomic_set(&cdevdata->in_use, 0);

	return 0;
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct cdev_data *cdevdata = filp->private_data;
	struct device *dev = cdevdata->dev;
	struct device *swd_dev = dev->parent;
	struct swd_dev_data *devdata;
	u32 reset_time_ms = 0;

	devdata = dev_get_drvdata(swd_dev);

	switch (cmd) {
	case SWD_IOCTL_CHIP_ERASE:
		if (!devdata->mcu_data.swd_ops.target_chip_erase) {
			dev_err(dev, "chip erase not implemented!");
			ret = -ENOSYS;
			goto out;
		}
		ret = devdata->mcu_data.swd_ops.target_chip_erase(swd_dev);
		if (ret)
			dev_err(dev, "chip erase failed!");
		break;
	case SWD_IOCTL_RESET:
		if (!gpio_is_valid(devdata->gpio_reset)) {
			dev_err(dev, "reset not implemented!");
			ret = -ENOSYS;
			goto out;
		}
		reset_time_ms = (u32)arg;
		dev_info(dev, "holding reset for %d ms ..", reset_time_ms);
		gpio_set_value(devdata->gpio_reset, 0);
		msleep(reset_time_ms);
		gpio_set_value(devdata->gpio_reset, 1);
		break;
	default:
		dev_err(dev, "unhandled ioctl %d", cmd);
		ret = -EIO;
		break;
	}
out:
	return ret;
}

static loff_t noop_seek(struct file *filp, loff_t off, int whence)
{
	(void)whence;
	struct cdev_data *cdevdata = filp->private_data;

	cdevdata->offset = off;
	return off;
}

static const struct file_operations chardev_fops = {
	.owner                  = THIS_MODULE,
	.read                   = device_read,
	.write                  = device_write,
	.open                   = device_open,
	.release                = device_release,
	.unlocked_ioctl         = device_ioctl,
	.llseek                 = noop_seek,
};

int swd_driver_init_chardev(struct device *dev, const char *const flavor)
{
	int ret;
	u8 *buf = NULL;
	size_t chunk_size = 0;
	struct swd_dev_data *devdata = NULL;
	struct cdev_data *cdevdata = NULL;

	devdata = dev_get_drvdata(dev);

	if (!devdata->mcu_data.swd_ops.target_get_write_chunk_size) {
		dev_err(dev, "write chunk size not implemented");
		return -ENOSYS;
	}

	chunk_size = devdata->mcu_data.swd_ops.target_get_write_chunk_size(dev);
	if (!chunk_size) {
		dev_err(dev, "invalid chunk size");
		return -EINVAL;
	}

	cdevdata = kzalloc(sizeof(*cdevdata), GFP_KERNEL);
	buf = kzalloc(chunk_size, GFP_KERNEL);

	if (!buf || !cdevdata) {
		ret = -ENOMEM;
		goto err_free;
	}

	ret = alloc_chrdev_region(&cdevdata->devt, 0, 1, "swd_chardev");
	if (ret < 0) {
		dev_err(dev, "alloc_chrdev_region() failed");
		goto err_free;
	}

	cls = class_create(THIS_MODULE, flavor);
	if (IS_ERR(cls)) {
		ret = PTR_ERR(cls);
		dev_err(dev, "class_create() failed: %d", ret);
		cls = NULL;
		goto err_unreg_chrdev;
	}

	cdev_init(&cdevdata->cdev, &chardev_fops);
	ret = cdev_add(&cdevdata->cdev, cdevdata->devt, 1);
	if (ret) {
		dev_err(dev, "cdev_add() failed: %d", ret);
		goto err_class_destroy;
	}

	cdevdata->dev = device_create(cls, dev, cdevdata->devt, cdevdata,
				      "swd_%s", flavor);
	if (IS_ERR(cdevdata->dev)) {
		ret = PTR_ERR(cdevdata->dev);
		dev_err(dev, "device_create() failed: %d", ret);
		goto err_cdev_del;
	}

	cdevdata->buf = buf;
	cdevdata->chunk_size = chunk_size;

	devdata->chardev_data = cdevdata;

	return 0;

err_cdev_del:
	cdev_del(&cdevdata->cdev);
err_class_destroy:
	class_destroy(cls);
	cls = NULL;
err_unreg_chrdev:
	unregister_chrdev_region(cdevdata->devt, 1);
err_free:
	kfree(buf);
	kfree(cdevdata);

	return ret;
}

void swd_driver_deinit_chardev(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct cdev_data *cdevdata;

	if (!devdata || !devdata->chardev_data)
		return;

	cdevdata = devdata->chardev_data;

	device_destroy(cls, cdevdata->devt);
	cdev_del(&cdevdata->cdev);
	unregister_chrdev_region(cdevdata->devt, 1);

	if (cls) {
		class_destroy(cls);
		cls = NULL;
	}

	kfree(cdevdata->buf);
	kfree(cdevdata);
	devdata->chardev_data = NULL;
}

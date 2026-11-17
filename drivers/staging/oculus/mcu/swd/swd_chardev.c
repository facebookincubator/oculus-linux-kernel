// SPDX-License-Identifier: GPL-2.0
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

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

	/* Opened O_NONBLOCK, so the driver's own bring-up was skipped. */
	bool skipped_prepare;

	/* This fd switched the SWD bus on for a debug claim, and owes it a
	 * switch back.
	 */
	bool bus_enabled;
};

static void swd_chardev_debug_release_locked(struct cdev_data *cdevdata);

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
	cdevdata->bus_enabled = false;
	cdevdata->skipped_prepare = !!(filp->f_flags & O_NONBLOCK);

	// treat O_NONBLOCK as signal to skip initializaiton
	if (cdevdata->skipped_prepare) {
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

	/*
	 * Drop any debug claim first, so a debugger that crashed rather than
	 * releasing cannot leave the driver refusing every other path.
	 */
	mutex_lock(&devdata->state_mutex);
	swd_chardev_debug_release_locked(cdevdata);
	mutex_unlock(&devdata->state_mutex);

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

/*
 * Hand the wire to an external debugger.
 *
 * This is wire-level bring-up only: the JTAG-to-SWD switch, IDCODE read and
 * power-up handshake that swd_init() would also do are left to the debugger,
 * which drives them over SWD_IOCTL_RAW_SEQ and SWD_IOCTL_DAP_XFER.
 */
static long swd_chardev_debug_claim(struct cdev_data *cdevdata)
{
	struct device *swd_dev = cdevdata->dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);
	long ret;

	ret = mutex_lock_interruptible(&devdata->state_mutex);
	if (ret)
		return ret;

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(swd_dev, "Update in progress, refusing debug claim");
		ret = -EBUSY;
		goto out;
	}

	/*
	 * The chardev is single-open, so a second claim can only come from a
	 * client that already holds one. Refuse it rather than quietly succeed:
	 * only one debugger can own the link, and a client asking twice has lost
	 * track of that.
	 */
	if (devdata->debug_claimed) {
		dev_err(swd_dev, "SWD link is already claimed by pid %d",
			devdata->debug_claim_pid);
		ret = -EBUSY;
		goto out;
	}

	/*
	 * Where the SWD lines are muxed away from the MCU, nothing reaches the
	 * target until the mux is switched. A blocking open gets that from
	 * target_prepare, but a debugger opens O_NONBLOCK precisely to skip
	 * that path, so do it here instead.
	 */
	if (cdevdata->skipped_prepare && devdata->mcu_data.swd_ops.bus_enable) {
		ret = devdata->mcu_data.swd_ops.bus_enable(swd_dev, true);
		if (ret) {
			dev_err(swd_dev, "Could not connect the SWD bus: %ld", ret);
			goto out;
		}
		cdevdata->bus_enabled = true;
	}

	swd_wire_init(swd_dev);

	devdata->debug_claimed = true;
	devdata->debug_claim_pid = task_pid_nr(current);
	dev_info(swd_dev, "debug session claimed by pid %d", devdata->debug_claim_pid);

out:
	mutex_unlock(&devdata->state_mutex);
	return ret;
}

/* Caller must hold state_mutex. */
static void swd_chardev_debug_release_locked(struct cdev_data *cdevdata)
{
	struct device *swd_dev = cdevdata->dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);

	if (!devdata->debug_claimed)
		return;

	/*
	 * Only stop driving the lines if this fd is also the one that connected
	 * them. Otherwise the bus belongs to the open that ran target_prepare,
	 * and target_finalize still has SWD work to do on the way out.
	 */
	if (cdevdata->bus_enabled) {
		swd_deinit(swd_dev);
		devdata->mcu_data.swd_ops.bus_enable(swd_dev, false);
		cdevdata->bus_enabled = false;
	}

	devdata->debug_claimed = false;
	devdata->debug_claim_pid = 0;
	dev_info(swd_dev, "debug session released");
}

static long swd_chardev_debug_release(struct cdev_data *cdevdata)
{
	struct device *swd_dev = cdevdata->dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);
	long ret;

	ret = mutex_lock_interruptible(&devdata->state_mutex);
	if (ret)
		return ret;

	swd_chardev_debug_release_locked(cdevdata);

	mutex_unlock(&devdata->state_mutex);
	return 0;
}

/* Caller must hold state_mutex. */
static long swd_chardev_claim_held(struct cdev_data *cdevdata, const char *what)
{
	struct device *swd_dev = cdevdata->dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);

	if (devdata->debug_claimed)
		return 0;

	dev_err(swd_dev, "%s without a debug claim", what);
	return -EPERM;
}

static long swd_chardev_dap_xfer(struct cdev_data *cdevdata, unsigned long arg)
{
	struct device *swd_dev = cdevdata->dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);
	struct swd_dap_batch batch;
	struct swd_dap_xfer *xfers;
	long ret = 0;
	u32 i;

	if (copy_from_user(&batch, (void __user *)arg, sizeof(batch)))
		return -EFAULT;

	if (batch.count == 0 || batch.count > SWD_DAP_XFER_MAX)
		return -EINVAL;

	xfers = memdup_user(u64_to_user_ptr(batch.xfers),
			    array_size(batch.count, sizeof(*xfers)));
	if (IS_ERR(xfers))
		return PTR_ERR(xfers);

	/*
	 * Held across the whole batch: a concurrent DEBUG_RELEASE would float
	 * the lines mid-transfer, and a second transfer would interleave bits
	 * with this one.
	 */
	ret = mutex_lock_interruptible(&devdata->state_mutex);
	if (ret)
		goto out_free;

	ret = swd_chardev_claim_held(cdevdata, "DAP_XFER");
	if (ret)
		goto out_unlock;

	for (i = 0; i < batch.count; i++) {
		struct swd_dap_xfer *xfer = &xfers[i];

		/* Only 0x0/0x4/0x8/0xc are addressable within a bank. */
		if (xfer->reg & ~0xcU ||
		    xfer->idle_clocks > SWD_IDLE_CLOCKS_MAX) {
			ret = -EINVAL;
			goto out_unlock;
		}

		ret = swd_dpap_xfer_raw(swd_dev, !!xfer->apndp, !!xfer->rnw,
					xfer->reg, &xfer->data,
					xfer->idle_clocks, &xfer->ack);
		if (ret)
			goto out_unlock;

		/*
		 * Stop at the first non-OK acknowledge and report how far the
		 * batch got. Recovery is the debugger's to decide.
		 */
		if (xfer->ack != SWD_ACK_OK) {
			i++;
			break;
		}
	}

	batch.completed = i;
	mutex_unlock(&devdata->state_mutex);

	if (copy_to_user(u64_to_user_ptr(batch.xfers), xfers,
			 array_size(batch.count, sizeof(*xfers))) ||
	    copy_to_user((void __user *)arg, &batch, sizeof(batch)))
		ret = -EFAULT;

	kfree(xfers);
	return ret;

out_unlock:
	mutex_unlock(&devdata->state_mutex);
out_free:
	kfree(xfers);
	return ret;
}

static long swd_chardev_raw_seq(struct cdev_data *cdevdata, unsigned long arg)
{
	struct device *swd_dev = cdevdata->dev->parent;
	struct swd_dev_data *devdata = dev_get_drvdata(swd_dev);
	struct swd_raw_seq seq;
	u8 *bits;
	long ret;

	if (copy_from_user(&seq, (void __user *)arg, sizeof(seq)))
		return -EFAULT;

	if (seq.nbits == 0 || seq.nbits > SWD_RAW_SEQ_MAX_BITS)
		return -EINVAL;

	bits = memdup_user(u64_to_user_ptr(seq.bits), DIV_ROUND_UP(seq.nbits, 8));
	if (IS_ERR(bits))
		return PTR_ERR(bits);

	ret = mutex_lock_interruptible(&devdata->state_mutex);
	if (ret)
		goto out_free;

	ret = swd_chardev_claim_held(cdevdata, "RAW_SEQ");
	if (!ret)
		swd_wire_raw_seq(swd_dev, bits, seq.nbits);

	mutex_unlock(&devdata->state_mutex);

out_free:
	kfree(bits);
	return ret;
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
		ret = mutex_lock_interruptible(&devdata->state_mutex);
		if (ret)
			goto out;
		ret = swd_debug_claim_check(swd_dev);
		if (!ret) {
			ret = devdata->mcu_data.swd_ops.target_chip_erase(swd_dev);
			if (ret)
				dev_err(dev, "chip erase failed!");
		}
		mutex_unlock(&devdata->state_mutex);
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
	case SWD_IOCTL_DEBUG_CLAIM:
		ret = swd_chardev_debug_claim(cdevdata);
		break;
	case SWD_IOCTL_DEBUG_RELEASE:
		ret = swd_chardev_debug_release(cdevdata);
		break;
	case SWD_IOCTL_DAP_XFER:
		ret = swd_chardev_dap_xfer(cdevdata, arg);
		break;
	case SWD_IOCTL_RAW_SEQ:
		ret = swd_chardev_raw_seq(cdevdata, arg);
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

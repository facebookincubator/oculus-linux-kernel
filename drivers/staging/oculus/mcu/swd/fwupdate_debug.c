// SPDX-License-Identifier: GPL-2.0
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include "fwupdate_debug.h"
#include "fwupdate_operations.h"
#include "swd.h"

struct swd_debug_data {
	int swd_debug_flavors_count;
	struct dentry *swd_debug_root_entry;
};

static DEFINE_MUTEX(swd_debug_data_lock);
static struct swd_debug_data debug_data;

static ssize_t swd_debug_reset_write(struct file *fp,
					  const char __user *user_buffer,
					  size_t count, loff_t *position)
{
	char buf[2];
	int status = 0;
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_reset;
	}

	if (!gpio_is_valid(devdata->gpio_reset)) {
		dev_err(dev, "Reset gpio not valid");
		return -ENODEV;
	}

	if (copy_from_user(&buf, user_buffer, 2))
		return -EFAULT;

	if (count != 2 || (buf[0] != '1' && buf[0] != '0')) {
		dev_err(dev, "Specify `1` or `0`\n ex. `echo 0 > reset`");
		return -EINVAL;
	}

	if (buf[0] == '0')
		gpio_set_value(devdata->gpio_reset, 0);
	else
		gpio_set_value(devdata->gpio_reset, 1);

exit_debug_reset:
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static ssize_t swd_debug_erase_chip_write(struct file *fp,
					  const char __user *user_buffer,
					  size_t count, loff_t *position)
{
	int status = 0;
	static const int reset_gpio_time_ms = 5;
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_erase_chip;
	}

	if (!devdata->mcu_data.swd_ops.target_chip_erase) {
		dev_err(dev, "target_chip_erase is NULL!");
		status = -EOPNOTSUPP;
		goto exit_debug_erase_chip;
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_set_value(devdata->gpio_reset, 1);
		msleep(reset_gpio_time_ms);
	}

	swd_init(dev);
	swd_halt(dev);

	status = fwupdate_update_chip_erase(dev);
	if (status)
		dev_err(dev, "target_chip_erase failed");
	swd_deinit(dev);
	dev_info(dev, "Manual chip erase complete");

exit_debug_erase_chip:
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static ssize_t swd_debug_write_app_write(struct file *fp,
					 const char __user *user_buffer,
					 size_t count, loff_t *position)
{
	int status = 0;
	const int kMcuResetMs = 5;
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_write;
	}

	fwupdate_get_firmware_images(dev, devdata);

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_direction_output(devdata->gpio_reset, 1);
		msleep(kMcuResetMs);
	}

	swd_init(dev);
	swd_halt(dev);

	if (fwupdate_update_prepare(dev)) {
		dev_err(dev, "Failed app prepare");
		goto exit_debug_write;
	}

	if (fwupdate_update_app(dev))
		dev_err(dev, "Failed app update");

	swd_deinit(dev);

	if (gpio_is_valid(devdata->gpio_reset))
		gpio_set_value(devdata->gpio_reset, 0);

	dev_info(dev, "Manual app update complete");

exit_debug_write:
	fwupdate_release_all_firmware(dev);
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static const struct file_operations swd_debug_reset_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_reset_write,
};

static const struct file_operations swd_debug_erase_chip_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_erase_chip_write,
};

static const struct file_operations swd_debug_write_app_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_write_app_write,
};

int fwupdate_create_debugfs(struct device *dev, const char *const flavor)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct dentry *entry;

	status = mutex_lock_interruptible(&swd_debug_data_lock);
	if (status)
		return -EBUSY;

	if (debug_data.swd_debug_flavors_count == 0)
		debug_data.swd_debug_root_entry =
			debugfs_create_dir("swd_debug", NULL);

	if (!debug_data.swd_debug_root_entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	devdata->debug_entry = debugfs_create_dir(
		flavor, debug_data.swd_debug_root_entry);

	if (!devdata->debug_entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	entry = debugfs_create_file("reset", 0644, devdata->debug_entry, dev,
				&swd_debug_reset_fops);
	if (!entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	entry = debugfs_create_file("erase_chip", 0644, devdata->debug_entry, dev,
				&swd_debug_erase_chip_fops);
	if (!entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	entry = debugfs_create_file("write_app", 0644, devdata->debug_entry, dev,
			    &swd_debug_write_app_fops);
	if (!entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	debug_data.swd_debug_flavors_count++;

exit_error:
	if (status) {
		debugfs_remove_recursive(devdata->debug_entry);
		devdata->debug_entry = NULL;
		if (debug_data.swd_debug_flavors_count == 0)
			debugfs_remove_recursive(debug_data.swd_debug_root_entry);
	}

	mutex_unlock(&swd_debug_data_lock);
	return status;
}

int fwupdate_remove_debugfs(struct device *dev)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&swd_debug_data_lock);
	if (status)
		return -EBUSY;

	debugfs_remove_recursive(devdata->debug_entry);

	debug_data.swd_debug_flavors_count--;
	if (debug_data.swd_debug_flavors_count == 0)
		debugfs_remove_recursive(debug_data.swd_debug_root_entry);

	mutex_unlock(&swd_debug_data_lock);

	return status;
}

// SPDX-License-Identifier: GPL-2.0
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

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
		status =  -ENODEV;
		goto exit_debug_reset;
	}

	if (copy_from_user(&buf, user_buffer, 2)) {
		status = -EFAULT;
		goto exit_debug_reset;
    }

	if (count != 2 || (buf[0] != '1' && buf[0] != '0')) {
		dev_err(dev, "Specify `1` or `0`\n ex. `echo 0 > reset`");
		status = -EINVAL;
		goto exit_debug_reset;
	}

	if (buf[0] == '0')
		gpio_set_value(devdata->gpio_reset, 0);
	else
		gpio_set_value(devdata->gpio_reset, 1);

exit_debug_reset:
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static ssize_t swd_debug_erase_app_write(struct file *fp,
					  const char __user *user_buffer,
					  size_t count, loff_t *position)
{
	int status = 0;
	int index = 0;
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *childdata;

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_erase_app;
	}

	if (!devdata->mcu_data.swd_ops.target_erase) {
		dev_err(dev, "target_erase is NULL!");
		status = -EOPNOTSUPP;
		goto exit_debug_erase_app;
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_set_value(devdata->gpio_reset, 1);
		msleep(DEFAULT_MCU_RESET_MS);
	}

	swd_init(dev);
	swd_halt(dev);

	// If there are no children, must update the parent
	if (devdata->num_children == 0) {
		status = devdata->mcu_data.swd_ops.target_erase(dev);
		if (status) {
			dev_err(dev, "Parent app erase failed!");
			goto exit_debug_erase_app_init;
		}
	} else {
		for (index = 0; index < devdata->num_children; index++) {
			childdata = &devdata->child_mcu_data[index];
			status = childdata->swd_ops.target_erase(dev);
			if (status) {
				dev_err(dev, "Child %d app erase failed!", index);
				goto exit_debug_erase_app_init;
			}
		}
	}

	dev_info(dev, "Manual chip erase complete");

exit_debug_erase_app_init:
	swd_deinit(dev);

exit_debug_erase_app:
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static ssize_t swd_debug_erase_chip_write(struct file *fp,
					  const char __user *user_buffer,
					  size_t count, loff_t *position)
{
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
		goto exit_debug_erase_chip;
	}

	if (!devdata->mcu_data.swd_ops.target_chip_erase) {
		dev_err(dev, "target_chip_erase is NULL!");
		status = -EOPNOTSUPP;
		goto exit_debug_erase_chip;
	}

	if (!devdata->erase_all) {
		dev_err(dev, "erase_all is not set!");
		status = -EOPNOTSUPP;
		goto exit_debug_erase_chip;
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_set_value(devdata->gpio_reset, 1);
		msleep(DEFAULT_MCU_RESET_MS);
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
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	status = fwupdate_check_swd_ops(dev);
	if (status) {
		dev_err(dev, "Invalid SWD Ops");
		return status;
	}

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_write;
	}

	status = fwupdate_get_firmware_images(dev, devdata);
	if (status) {
		dev_err(dev, "Invalid firmware image");
		goto exit_debug_write;
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_direction_output(devdata->gpio_reset, 1);
		msleep(DEFAULT_MCU_RESET_MS);
	}

	swd_init(dev);
	swd_halt(dev);

	if (fwupdate_update_prepare(dev)) {
		dev_err(dev, "Failed app prepare");
		goto exit_debug_write_swd_init;
	}

	if (fwupdate_update_app(dev))
		dev_err(dev, "Failed app update");

	if (gpio_is_valid(devdata->gpio_reset))
		gpio_set_value(devdata->gpio_reset, 0);

	dev_info(dev, "Manual app update complete");

exit_debug_write_swd_init:
	swd_deinit(dev);

exit_debug_write:
	fwupdate_release_all_firmware(dev);
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static int swd_debug_flash_application(struct device *dev)
{
	int index;
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *childdata;
	const bool kForceBootloaderUpdate = true;
	const bool kForceEraseAll = true;

	if (devdata->num_children == 0) {
		status = fwupdate_update_single_app(
				dev, &devdata->mcu_data, kForceEraseAll,
				kForceBootloaderUpdate);
		if (status)
			return status;
	} else {
		for (index = 0; index < devdata->num_children; index++) {
			childdata = &devdata->child_mcu_data[index];
			status = fwupdate_update_single_app(
				dev, childdata, kForceEraseAll,
				kForceBootloaderUpdate);
			if (status)
				return status;
		}
	}

	return 0;
}

static ssize_t swd_debug_set_mcu_quirk(struct file *fp,
					 const char __user *user_buffer,
					 size_t count, loff_t *position)
{
#define MAX_MCU_QUIRK_SIZE (16)
	int status = 0;
	char str[MAX_MCU_QUIRK_SIZE];
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	size_t n = (count < (MAX_MCU_QUIRK_SIZE - 1)) ?
				count : (MAX_MCU_QUIRK_SIZE - 1);

	dev_info(dev, "manually set sdfw version");
	if (n == 0)
		return count; /* nothing to do, still report consumed */

	status = fwupdate_check_swd_ops(dev);
	if (status) {
		dev_err(dev, "Invalid SWD Ops");
		return status;
	}

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_set_mcu_quirk;
	}

	if (copy_from_user(str, user_buffer, n))
		return -EFAULT;

	str[n] = '\0';
	/* Trim trailing newline/carriage-return characters */
	while (n > 0 && (str[n - 1] == '\n' || str[n - 1] == '\r'))
		str[--n] = '\0';

	status = devdata->mcu_data.swd_ops.set_mcu_quirk(dev, str);

exit_debug_set_mcu_quirk:
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static ssize_t swd_debug_force_write_all_write(struct file *fp,
					 const char __user *user_buffer,
					 size_t count, loff_t *position)
{
	int status = 0;
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	status = fwupdate_check_swd_ops(dev);
	if (status) {
		dev_err(dev, "Invalid SWD Ops");
		return status;
	}

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_err(dev, "Failed to get state mutex");
		return -EBUSY;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev, "Update in progress, skipping");
		status = -EBUSY;
		goto exit_debug_write;
	}

	dev_info(dev, "Retrieving firmware images");
	status = fwupdate_get_firmware_images(dev, devdata);
	if (status) {
		dev_err(dev, "Invalid firmware image");
		goto exit_debug_write;
	}

	if (devdata->swd_core) {
		status = regulator_enable(devdata->swd_core);
		if (status) {
			dev_err(dev, "Regulator failed to enable");
			goto exit_debug_write_swd_init;
		}
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_direction_output(devdata->gpio_reset, 0);
		msleep(DEFAULT_MCU_RESET_MS);
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_direction_output(devdata->gpio_reset, 1);
		msleep(DEFAULT_MCU_RESET_MS);
	}

	swd_init(dev);
	swd_halt(dev);

	dev_info(dev, "Preparing to flash MCU application");
	if (fwupdate_update_prepare(dev)) {
		dev_err(dev, "Failed app prepare");
		goto exit_debug_write_swd_init;
	}

	// Intentionally ignore the device config for erase_all
	if (devdata->mcu_data.swd_ops.target_chip_erase) {
		dev_warn(dev, "Performing full chip erase!");
		status = devdata->mcu_data.swd_ops.target_chip_erase(dev);
	}

	// Log but ignore failure because at this point the chip is wiped. We might
	// still be able to flash the application. If not, the application write will
	// also fail.
	if (status)
		dev_err(dev, "target_chip_erase failed");

	// We have intentionally erased the chip, so we need to update all
	dev_info(dev, "Flashing MCU applications");
	status = swd_debug_flash_application(dev);
	if (status)
		goto exit_debug_write_swd_init;

	if (gpio_is_valid(devdata->gpio_reset))
		gpio_set_value(devdata->gpio_reset, 0);

	dev_info(dev, "Forced MCU update complete");

exit_debug_write_swd_init:
	swd_deinit(dev);

exit_debug_write:
	fwupdate_release_all_firmware(dev);
	mutex_unlock(&devdata->state_mutex);
	return status ? status : count;
}

static ssize_t swd_debug_part_number_read(struct file *fp,
					  char __user *user_buffer,
					  size_t count, loff_t *position)
{
	struct device *dev = fp->private_data;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	char str[16];
	int  s;
	int  partnum;

	if (!devdata->mcu_data.swd_ops.read_part_number) {
		dev_err(dev, "read part number not supported!");
		return -EOPNOTSUPP;
	}

	partnum = devdata->mcu_data.swd_ops.read_part_number(dev);
	s = snprintf(str, sizeof(str), "0x%x", partnum);

	if (*position >= s)
		return 0;

	s -= *position;
	s = min_t(u32, s, count);
	if (copy_to_user(user_buffer, &str[*position], s)) {
		return -EFAULT;
	}

	*position += s;

	return s;
}

static const struct file_operations swd_debug_reset_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_reset_write,
};

static const struct file_operations swd_debug_erase_app_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_erase_app_write,
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

static const struct file_operations swd_debug_force_write_all_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_force_write_all_write,
};

static const struct file_operations swd_debug_read_part_number_all_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = swd_debug_part_number_read,
};

static const struct file_operations swd_debug_set_mcu_quirk_all_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = swd_debug_set_mcu_quirk,
};

int fwupdate_create_debugfs(struct device *dev, const char *const flavor)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct dentry *entry;

	status = mutex_lock_interruptible(&swd_debug_data_lock);
	if (status) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, status);
		return status;
	}

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

	entry = debugfs_create_file("erase_app", 0644, devdata->debug_entry, dev,
				&swd_debug_erase_app_fops);
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

	entry = debugfs_create_file("force_write_all", 0644, devdata->debug_entry, dev,
			    &swd_debug_force_write_all_fops);
	if (!entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	entry = debugfs_create_file("read_part_num", 0644, devdata->debug_entry, dev,
			    &swd_debug_read_part_number_all_fops);
	if (!entry) {
		status = -ENOMEM;
		goto exit_error;
	}

	entry = debugfs_create_file("set_mcu_quirk", 0644, devdata->debug_entry,
			dev, &swd_debug_set_mcu_quirk_all_fops);
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
	if (status) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, status);
		return status;
	}

	debugfs_remove_recursive(devdata->debug_entry);

	debug_data.swd_debug_flavors_count--;
	if (debug_data.swd_debug_flavors_count == 0)
		debugfs_remove_recursive(debug_data.swd_debug_root_entry);

	mutex_unlock(&swd_debug_data_lock);

	return status;
}

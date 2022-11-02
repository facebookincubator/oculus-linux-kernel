// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>

#include "fw_helpers.h"
#include "hubert_swd_ops.h"
#include "swd.h"
#include "syncboss_swd_ops.h"
#include "safetyboss_swd_ops.h"

#define FW_UPDATE_STATE_IDLE_STR      "idle"
#define FW_UPDATE_STATE_WRITING_STR   "writing"
#define FW_UPDATE_STATE_ERROR_STR     "error"

#define RESET_GPIO_TIME_MS 5

/* SWD Operations for each supported architecture */
static struct {
	const char *flavor;
	struct swd_ops_params swd_ops;
} archs_params[] = {
	{
		.flavor = "nrf52832",
		.swd_ops = {
			.provisioning_read = syncboss_swd_provisioning_read,
			.provisioning_write = syncboss_swd_provisioning_write,
			.target_erase = syncboss_swd_erase_app,
			.target_program_write_chunk = syncboss_swd_write_chunk,
			.target_get_write_chunk_size = syncboss_get_write_chunk_size,
			.target_program_read = syncboss_swd_read,
			.target_page_is_erased = syncboss_swd_page_is_erased,
		}
	},
	{
		.flavor = "at91samd",
		.swd_ops = {
			.provisioning_read = hubert_swd_provisioning_read,
			.provisioning_write = hubert_swd_provisioning_write,
			.target_prepare = hubert_swd_prepare,
			.target_erase = hubert_swd_erase_app,
			.target_program_write_chunk = hubert_swd_write_chunk,
			.target_get_write_chunk_size = hubert_get_write_chunk_size,
			.target_program_read = hubert_swd_read,
		}
	},
	{
		.flavor = "stm32g0",
		.swd_ops = {
			.target_prepare = safetyboss_swd_prepare,
			.target_erase = safetyboss_swd_erase_app,
			.target_program_write_chunk = safetyboss_swd_write_chunk,
			.target_get_write_chunk_size = safetyboss_get_write_chunk_size,
		}
	},
};

static int check_swd_ops(struct device *dev, struct swd_ops_params *ops)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (!ops->target_erase){
		dev_err(dev, "target_erase is NULL!");
		return -ENOSYS;
	}

	if (!ops->target_program_write_chunk) {
		dev_err(dev, "target_program_write_chunk is NULL!");
		return -ENOSYS;
	}

	if (!ops->target_get_write_chunk_size) {
		dev_err(dev, "target_get_write_chunk_size is NULL!");
		return -ENOSYS;
	}

	if (devdata->flash_info.num_protected_bootloader_pages && !ops->target_page_is_erased) {
		dev_err(dev, "target_page_is_erased is NULL!");
		return -ENOSYS;
	}

	return 0;
}

static int provision_if_needed(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_ops_params *ops = &devdata->swd_ops;
	int status, addr, length;
	u8 *data;

	if (!devdata->swd_provisioning)
		return 0;

	if (!ops->provisioning_read || !ops->provisioning_write) {
		dev_err(dev, "swd provisioning support not implemented\n");
		return -EINVAL;
	}

	addr = devdata->provisioning->flash_addr;
	length = devdata->provisioning->data_length;

	data = kmalloc(length, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	status = ops->provisioning_read(dev, addr, data, length);
	if (status) {
		dev_err(dev, "Failed to read provisioning data\n");
		goto out;
	}

	if (memcmp(data, devdata->provisioning->data, length)) {
		dev_info(dev, "MCU needs provisioning. Attempting now...\n");
		status = ops->provisioning_write(dev, addr, devdata->provisioning->data, length);
		if (status) {
			dev_err(dev, "Failed to read provisioning data\n");
			goto out;
		}
		dev_info(dev, "MCU provisioning complete\n");
	} else {
		dev_info(dev, "MCU already provisioned\n");
	}

out:
	kfree(data);
	return status;
}

static int update_firmware(struct device *dev)
{
	int status;
	int iteration_ctr = 0;
	size_t bytes_to_write = 0;
	size_t pages_to_skip = 0;
	size_t bytes_written = 0;
	size_t bytes_left = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	const struct firmware *fw = devdata->fw;
	size_t chunk_size;

	status = check_swd_ops(dev, &devdata->swd_ops);
	if (status) {
		goto error;
	}

	if (devdata->swd_core) {
		status = regulator_enable(devdata->swd_core);
		if (status) {
			dev_err(dev, "Regulator failed to enable");
			goto error;
		}
	}

	if (gpio_is_valid(devdata->gpio_reset)) {
		gpio_direction_output(devdata->gpio_reset, 1);
		msleep(RESET_GPIO_TIME_MS);
	}

	dev_info(dev, "Updating firmware: Image size: %zd bytes...", fw->size);
#if defined(CONFIG_DYNAMIC_DEBUG)
	print_hex_dump_bytes("Firmware binary to write: ", DUMP_PREFIX_OFFSET,
			     fw->data, fw->size);
#endif
	swd_init(dev);
	swd_halt(dev);

	/* Configure any target-specific registers needed before the update starts */
	if (devdata->swd_ops.target_prepare) {
		status = devdata->swd_ops.target_prepare(dev);
		if (status)
			goto error;
	}

	dev_info(dev, "Erasing firmware app");
	status = devdata->swd_ops.target_erase(dev);
	if (status)
		goto error;

	while (pages_to_skip < devdata->flash_info.num_protected_bootloader_pages) {
		if (!devdata->swd_ops.target_page_is_erased(dev, pages_to_skip))
			pages_to_skip++;
		else
			break;
	}
	if (pages_to_skip > 0) {
		dev_warn(dev, "Bootloader pages are protected! Writes to them will be skipped.");
		bytes_written = pages_to_skip * devdata->flash_info.page_size;
	}

	chunk_size = devdata->swd_ops.target_get_write_chunk_size(dev);
	atomic_set(&devdata->fw_chunks_to_write, DIV_ROUND_UP(fw->size, chunk_size));

	while (bytes_written < fw->size) {
		dev_dbg(dev, "Writing chunk %d", iteration_ctr++);

		bytes_left = fw->size - bytes_written;
		bytes_to_write = min(bytes_left, chunk_size);
		status = devdata->swd_ops.target_program_write_chunk(
						  dev,
						  bytes_written,
						  &fw->data[bytes_written],
						  bytes_to_write);
		if (status)
			goto error;

		dev_dbg(dev, "Done writing chunk");

		bytes_written += bytes_to_write;
		atomic_inc(&devdata->fw_chunks_written);
	}

	status = provision_if_needed(dev);
	if (status) {
		dev_err(dev, "Provisioning failed!\n");
		goto error;
	}

	swd_reset(dev);
	swd_flush(dev);
	swd_deinit(dev);

	if (gpio_is_valid(devdata->gpio_reset)) {
		dev_info(dev, "Re-applying MCU reset");
		gpio_set_value(devdata->gpio_reset, 0);

		/*
		 * Delay since we don't know when the driver that owns this
		 * MCU will want to wake it again.
		 */
		msleep(RESET_GPIO_TIME_MS);
	}

	dev_info(dev, "Done updating firmware. ");

 error:
	kfree(devdata->provisioning);
	devdata->provisioning = NULL;
	if (devdata->swd_core && regulator_disable(devdata->swd_core))
		dev_err(dev, "Regulator failed to disable");
	atomic_set(&devdata->fw_chunks_written, 0);
	atomic_set(&devdata->fw_chunks_to_write, 0);
	devdata->fw_update_state = status ? FW_UPDATE_STATE_ERROR : FW_UPDATE_STATE_IDLE;

	return status;
}


ssize_t fwupdate_show_update_firmware(struct device *dev, char *buf)
{
	int status = 0;
	ssize_t retval = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(dev, "Failed to get state mutex: %d", status);
		return status;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_IDLE) {
		retval = scnprintf(buf, PAGE_SIZE, FW_UPDATE_STATE_IDLE_STR "\n");
	} else if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		retval = scnprintf(buf, PAGE_SIZE,
				   FW_UPDATE_STATE_WRITING_STR
				   " %i/%i\n",
				   atomic_read(&devdata->fw_chunks_written),
				   atomic_read(&devdata->fw_chunks_to_write));
	} else if (devdata->fw_update_state == FW_UPDATE_STATE_ERROR) {
		retval = scnprintf(buf, PAGE_SIZE, FW_UPDATE_STATE_ERROR_STR "\n");
	} else {
		/* In an unknown state */
		BUG_ON(1);
	}

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static void swd_workqueue_fw_update(void *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	update_firmware(dev);
	release_firmware(devdata->fw);
	devdata->fw = NULL;
}

static int populate_provisioning_data(struct device *dev, const char *buf,
				      size_t count)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct provisioning *p = (struct provisioning *)buf;

	if (count < sizeof(struct provisioning)) {
		dev_err(dev, "Failed to read provisioning data header\n");
		return -EINVAL;
	}

	if (p->format_version != 0) {
		dev_err(dev, "Unsupported provisioning format version\n");
		return -EINVAL;
	}

	if (sizeof(struct provisioning) + p->data_length != count) {
		dev_err(dev, "Unexpected provisioning data length\n");
		return -EINVAL;
	}

	devdata->provisioning = kmemdup(buf, count, GFP_KERNEL);
	if (!devdata->provisioning)
		return -ENOMEM;

	return 0;
}

/**
 * fwupdate_store_update_firmware() - handle a write to the update_firmware file
 * @dev: This device
 * @buf: Data buffer written from userspace. Expected to be opaque data starting
 *       with a 'struct provisioning' header.
 * @count: The number of bytes in buffer
 *
 * Return: @count upon success, else a negative value to indicate an error.
 */
ssize_t fwupdate_store_update_firmware(struct device *dev, const char *buf,
				       size_t count)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	size_t max_fw_size = (flash->num_pages - flash->num_retained_pages) * flash->page_size;

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_err(dev, "Failed to get state mutex: %d", status);
		return status;
	}

	if ((devdata->gpio_swdclk < 0) || (devdata->gpio_swdio < 0)) {
		dev_err(dev,
			"Cannot update firmware since swd lines were not specified");
		status = -EINVAL;
		goto error;
	}
	if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		dev_err(dev,
			"Cannot update firmware while firmware update is not in an idle state, is another fw update running?");
		status = -EINVAL;
		goto error;
	}
	if (devdata->is_busy && devdata->is_busy(dev)) {
		dev_err(dev, "Cannot update firmware while device is busy");
		status = -EBUSY;
		goto error;
	}

	status = request_firmware(&devdata->fw, devdata->fw_path, dev);
	if (status != 0) {
		if (devdata->fw_path != NULL) {
			dev_err(dev,
				"request_firmware: %d, Please ensure %s is present.",
				status, devdata->fw_path);
		} else {
			dev_err(dev,
				"request_firmware: %d, fw_path is null",
				status);
		}
		goto error;
	}

	if (devdata->fw->size > max_fw_size) {
		dev_err(dev,
			"Firmware binary size too large, provided size: %zd, max size: %zd",
			devdata->fw->size, max_fw_size);
		status = -ENOMEM;
		goto error;
	}

	if (devdata->swd_provisioning) {
		status = populate_provisioning_data(dev, buf, count);
		if (status)
			goto error;
	}

	devdata->fw_update_state = FW_UPDATE_STATE_WRITING_TO_HW;

	fw_queue_work(devdata->workqueue, dev, swd_workqueue_fw_update, NULL);

	mutex_unlock(&devdata->state_mutex);

	return count;

error:
	if (devdata->fw) {
		release_firmware(devdata->fw);
		devdata->fw = NULL;
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

int fwupdate_init_swd_ops(struct device *dev, const char *swdflavor)
{
	int i;
	struct swd_ops_params *ops = NULL;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	for (i = 0; i < ARRAY_SIZE(archs_params); i++) {
		if (!strncmp(swdflavor, archs_params[i].flavor,
			     strlen(archs_params[i].flavor))) {
			ops = &archs_params[i].swd_ops;
			break;
		}
	}

	if (!ops) {
		dev_err(dev, "No swd_ops for flavor: %s", swdflavor);
		return -EINVAL;
	}

	devdata->swd_ops = *ops;
	return 0;
}

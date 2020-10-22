#include <linux/firmware.h>
#include <linux/regulator/consumer.h>

#include "fw_helpers.h"
#include "hubert_swd_ops.h"
#include "swd.h"
#include "syncboss_swd_ops.h"

#define FW_UPDATE_STATE_IDLE_STR      "idle"
#define FW_UPDATE_STATE_WRITING_STR   "writing"

/* SWD Operations for each supported architecture */
static struct {
	const char *flavor;
	struct swd_ops_params swd_ops;
} archs_params[] = {
	{
		.flavor = "nrf52832",
		.swd_ops = {
			.target_erase = syncboss_swd_erase_app,
			.target_program_write_block = syncboss_swd_write_block,
			.target_program_cleanup = NULL,
		}
	},
	{
		.flavor = "at91samd",
		.swd_ops = {
			.target_erase = hubert_swd_erase_app,
			.target_program_write_block = hubert_swd_write_block,
			.target_program_cleanup = hubert_swd_wp_and_reset
		}
	},
};

static int update_firmware(struct device *dev)
{
	int status = 0;
	int bytes_written = 0;
	int iteration_ctr = 0;
	int bytes_to_write = 0;
	int bytes_left = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	const int block_size = flash->block_size;
	const struct firmware *fw = devdata->fw;

	if (devdata->swd_core) {
		status = regulator_enable(devdata->swd_core);
		if (status) {
			dev_err(dev, "Regulator failed to enable");
			goto error;
		}
	}

	dev_info(dev, "Updating firmware: Image size: %zd bytes...", fw->size);
#if defined(CONFIG_DYNAMIC_DEBUG)
	print_hex_dump_bytes("Firmware binary to write: ", DUMP_PREFIX_OFFSET,
			     fw->data, fw->size);
#endif
	swd_init(dev);

	dev_info(dev, "Erasing firmware app");
	if (devdata->swd_ops.target_erase)
		status = devdata->swd_ops.target_erase(dev);
	else
		dev_err(dev, "SWD target_erase is NULL!");

	if (status != 0)
		goto error;

	atomic_set(&devdata->fw_blocks_to_write,
		(fw->size + block_size - 1) / block_size);

	if (!devdata->swd_ops.target_program_write_block) {
		dev_err(dev, "SWD write_block is NULL!");
		status = -EINVAL;
		goto error;
	}

	while (bytes_written < fw->size) {
		dev_dbg(dev, "Writing block %d", iteration_ctr++);

		bytes_left = fw->size - bytes_written;
		bytes_to_write = min(bytes_left, block_size);
		status = devdata->swd_ops.target_program_write_block(
						  dev,
						  bytes_written,
						  &fw->data[bytes_written],
						  bytes_to_write);
		if (status != 0)
			goto error;

		dev_dbg(dev, "Done writing block");

		bytes_written += bytes_to_write;

		atomic_inc(&devdata->fw_blocks_written);
	}

	if (devdata->swd_ops.target_program_cleanup)
		devdata->swd_ops.target_program_cleanup(dev);

	swd_deinit(dev);

	dev_info(dev, "Done updating firmware. ");
	dev_info(dev, "Issuing sleep request");

 error:
	if (devdata->swd_core && regulator_disable(devdata->swd_core))
		dev_err(dev, "Regulator failed to disable");
	if (devdata->on_firmware_update_complete)
		devdata->on_firmware_update_complete(dev, status);
	atomic_set(&devdata->fw_blocks_written, 0);
	atomic_set(&devdata->fw_blocks_to_write, 0);
	devdata->fw_update_state = FW_UPDATE_STATE_IDLE;

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
		retval = scnprintf(buf, PAGE_SIZE,
				   FW_UPDATE_STATE_IDLE_STR "\n");
	} else if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {

		retval = scnprintf(buf, PAGE_SIZE,
				   FW_UPDATE_STATE_WRITING_STR
				   " %i/%i\n",
				   atomic_read(&devdata->fw_blocks_written),
				   atomic_read(&devdata->fw_blocks_to_write));
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

ssize_t fwupdate_store_update_firmware(struct device *dev, const char *buf,
				       size_t count)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	size_t max_fw_size = (flash->num_pages - flash->num_retained_pages)
		* flash->page_size;

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
	if (devdata->fw_update_state != FW_UPDATE_STATE_IDLE) {
		dev_err(dev,
			"Cannot update firmware while firmware update is not in the idle state, is another fw update running?");
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

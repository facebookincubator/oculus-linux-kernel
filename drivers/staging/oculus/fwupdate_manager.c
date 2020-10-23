#include "fwupdate_manager.h"

#include "syncboss_swd_ops.h"
#include "hubert_swd_ops.h"

#include "syncboss_common.h"

#define SYNCBOSS_FW_UPDATE_STATE_IDLE_STR      "idle"
#define SYNCBOSS_FW_UPDATE_STATE_WRITING_STR   "writing"

/* SWD Operations for each supported architecture */
static struct {
	const char *flavor;
	struct swd_ops_params_t swd_ops;
} archs_params[] = {
	{
		.flavor = "nrf52832",
		.swd_ops = {
			.block_size = SYNCBOSS_BLOCK_SIZE,
			.max_fw_size = (SYNCBOSS_NUM_FLASH_PAGES -
					SYNCBOSS_NUM_FLASH_PAGES_TO_RETAIN)
					* SYNCBOSS_FLASH_PAGE_SIZE,
			.target_erase = syncboss_swd_erase_app,
			.target_program_write_block = syncboss_swd_write_block,
			.target_program_cleanup = NULL,
		}
	},
	{
		.flavor = "at91samd",
		.swd_ops = {
			.block_size = AT91SAMD_BLOCK_SIZE,
			.max_fw_size = AT91SAMD_MAX_FW_SIZE,
			.target_erase = hubert_swd_erase_app,
			.target_program_write_block = hubert_swd_write_block,
			.target_program_cleanup = hubert_swd_wp_and_reset
		}
	},
};

static int update_firmware(struct fwupdate_data *devdata)
{
	int status = 0;
	int bytes_written = 0;
	int iteration_ctr = 0;
	int bytes_to_write = 0;
	struct swdhandle_t swd_handle;
	int bytes_left = 0;
	const int block_size = devdata->swd_ops.block_size;
	const struct firmware *fw = devdata->fw;

	if (fw->size > devdata->swd_ops.max_fw_size) {
		dev_err(devdata->dev,
			"Firmware binary size too large, provided size: %zd, max size: %zd",
			fw->size, devdata->swd_ops.max_fw_size);
		return -ENOMEM;
	}

	dev_info(devdata->dev,
		 "Updating firmware: Image size: %zd bytes...",
		 fw->size);
#if defined(CONFIG_DYNAMIC_DEBUG)
	print_hex_dump_bytes("Firmware binary to write: ", DUMP_PREFIX_OFFSET,
			     fw->data, fw->size);
#endif

	swd_init(&swd_handle, devdata->gpio_swdclk, devdata->gpio_swdio);

	dev_info(devdata->dev, "Erasing firmware app");
	if (devdata->swd_ops.target_erase)
		status = devdata->swd_ops.target_erase(devdata->dev,
						       &swd_handle);
	else
		dev_err(devdata->dev, "SWD target_erase is NULL!");

	if (status != 0)
		goto error;

	atomic_set(&devdata->fw_blocks_to_write,
		(fw->size + block_size - 1) / block_size);

	if (!devdata->swd_ops.target_program_write_block) {
		dev_err(devdata->dev,
			"SWD write_block is NULL!");
		return -EINVAL;
	}

	while (bytes_written < fw->size) {
		dev_dbg(devdata->dev, "Writing block %d", iteration_ctr++);

		bytes_left = fw->size - bytes_written;
		bytes_to_write = min(bytes_left, block_size);
		status = devdata->swd_ops.target_program_write_block(
						  devdata->dev,
						  &swd_handle,
						  bytes_written,
						  &fw->data[bytes_written],
						  bytes_to_write);
		if (status != 0)
			goto error;

		dev_dbg(devdata->dev, "Done writing block");

		bytes_written += bytes_to_write;

		atomic_inc(&devdata->fw_blocks_written);
	}

	if (devdata->swd_ops.target_program_cleanup)
		devdata->swd_ops.target_program_cleanup(devdata->dev,
							&swd_handle);

	swd_deinit(&swd_handle);

	dev_info(devdata->dev, "Done updating firmware. ");
	dev_info(devdata->dev, "Issuing syncboss sleep request");

 error:
	if (devdata->on_firmware_update_complete)
		devdata->on_firmware_update_complete(status, devdata->dev);
	atomic_set(&devdata->fw_blocks_written, 0);
	atomic_set(&devdata->fw_blocks_to_write, 0);
	devdata->fw_update_state = SYNCBOSS_FW_UPDATE_STATE_IDLE;

	return status;
}


ssize_t fwupdate_show_update_firmware(struct device *dev,
				    struct fwupdate_data *devdata, char *buf)
{
	int status = 0;
	ssize_t retval = 0;

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(devdata->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	if (devdata->fw_update_state == SYNCBOSS_FW_UPDATE_STATE_IDLE) {
		retval = scnprintf(buf, PAGE_SIZE,
				   SYNCBOSS_FW_UPDATE_STATE_IDLE_STR "\n");
	} else if (devdata->fw_update_state ==
		   SYNCBOSS_FW_UPDATE_STATE_WRITING_TO_HW) {

		retval = scnprintf(buf, PAGE_SIZE,
				   SYNCBOSS_FW_UPDATE_STATE_WRITING_STR
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

static void swd_workqueue_fw_update(void *data)
{
	struct fwupdate_data *devdata = data;

	update_firmware(devdata);
	release_firmware(devdata->fw);
	devdata->fw = NULL;
}

ssize_t fwupdate_store_update_firmware(struct workqueue_struct *workqueue,
						 struct device *dev,
				     struct fwupdate_data *devdata,
				     const char *buf, size_t count)
{
	int status = 0;

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_err(dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	if ((devdata->gpio_swdclk < 0) || (devdata->gpio_swdio < 0)) {
		dev_err(dev,
			"Cannot update firmware since swd lines were not specified");
		status = -EINVAL;
		goto error;
	} else if (devdata->fw_update_state != SYNCBOSS_FW_UPDATE_STATE_IDLE) {
		dev_err(dev,
			"Cannot update firmware while firmware update is not in the idle state, is another fw update running?");
		status = -EINVAL;
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

	devdata->fw_update_state = SYNCBOSS_FW_UPDATE_STATE_WRITING_TO_HW;

	syncboss_queue_work(workqueue, devdata, swd_workqueue_fw_update, NULL);
	status = count;

error:
	mutex_unlock(&devdata->state_mutex);
	return status;
}

int fwupdate_init_swd_ops(struct device *dev, struct fwupdate_data *fwudata,
				const char *swdflavor)
{
	int i;
	struct swd_ops_params_t *ops = NULL;

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

	fwudata->swd_ops = *ops;
	return 0;
}

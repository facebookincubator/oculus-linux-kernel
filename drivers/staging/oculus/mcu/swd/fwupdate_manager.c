// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>

#include "hubert_swd_ops.h"
#include "fwupdate_operations.h"
#include "swd.h"
#include "syncboss_swd_ops_nrf52xxx.h"
#include "syncboss_swd_ops_nrf5340.h"
#include "stm32g0_swd_ops.h"
#include "qm35xxx_swd_ops.h"
#include "stm32l47xxx_swd_ops.h"

#define FW_UPDATE_STATE_IDLE_STR      "idle"
#define FW_UPDATE_STATE_WRITING_STR   "writing"
#define FW_UPDATE_STATE_ERROR_STR     "error"

#define RESET_GPIO_TIME_MS 5

/* SWD Operations for each supported architecture */
static struct {
	const char *flavor;
	struct swd_ops_params swd_ops;
} archs_params[] = {
#ifdef CONFIG_META_SWD_SYNCBOSS_NRF52XXX
	{
		.flavor = "nrf52832",
		.swd_ops = {
			.target_prepare = syncboss_swd_nrf52832_prepare,

			.provisioning_read = syncboss_swd_nrf52xxx_provisioning_read,
			.provisioning_write = syncboss_swd_nrf52xxx_provisioning_write,
			.target_erase = syncboss_swd_nrf52xxx_erase_app,
			.target_chip_erase = syncboss_swd_nrf52xxx_chip_erase,
			.target_program_write_chunk = syncboss_swd_nrf52xxx_write_chunk,
			.target_get_write_chunk_size = syncboss_get_write_chunk_size,
			.target_program_read = syncboss_swd_nrf52xxx_read,
			.target_page_is_erased = syncboss_swd_nrf52xxx_page_is_erased,
			.target_finalize = syncboss_swd_nrf52xxx_finalize,
		}
	},
	{
		.flavor = "nrf52833",
		.swd_ops = {
			.target_prepare = syncboss_swd_nrf52833_prepare,
			.provisioning_read = syncboss_swd_nrf52xxx_provisioning_read,
			.provisioning_write = syncboss_swd_nrf52xxx_provisioning_write,
			.target_erase = syncboss_swd_nrf52xxx_erase_app,
			.target_chip_erase = syncboss_swd_nrf52xxx_chip_erase,
			.target_program_write_chunk = syncboss_swd_nrf52xxx_write_chunk,
			.target_get_write_chunk_size = syncboss_get_write_chunk_size,
			.target_program_read = syncboss_swd_nrf52xxx_read,
			.target_page_is_erased = syncboss_swd_nrf52xxx_page_is_erased,
			.target_finalize = syncboss_swd_nrf52xxx_finalize,
		}
	},
	{
		.flavor = "nrf52840",
		.swd_ops = {
			.provisioning_read = syncboss_swd_nrf52xxx_provisioning_read,
			.provisioning_write = syncboss_swd_nrf52xxx_provisioning_write,
			.target_erase = syncboss_swd_nrf52xxx_erase_app,
			.target_chip_erase = syncboss_swd_nrf52xxx_chip_erase,
			.target_program_write_chunk = syncboss_swd_nrf52xxx_write_chunk,
			.target_get_write_chunk_size = syncboss_get_write_chunk_size,
			.target_program_read = syncboss_swd_nrf52xxx_read,
			.target_page_is_erased = syncboss_swd_nrf52xxx_page_is_erased,
			.target_finalize = syncboss_swd_nrf52xxx_finalize,
		}
	},
#endif
#ifdef CONFIG_META_SWD_SYNCBOSS_NRF5340
	{
		.flavor = "nrf5340",
		.swd_ops = {
			.target_chip_erase = syncboss_swd_nrf5340_chip_erase,
			.target_finalize = syncboss_swd_nrf5340_finalize,
		}
	},
	{
		.flavor = "nrf5340_app",
		.swd_ops = {
			.target_erase = syncboss_swd_nrf5340_erase_app,
			.target_page_is_erased = syncboss_swd_nrf5340_page_is_erased_app,
			.target_program_write_chunk = syncboss_swd_nrf5340_app_write_chunk,
			.target_get_write_chunk_size = syncboss_nrf5340_get_app_write_chunk_size,
			.target_program_read = syncboss_swd_nrf5340_read,
		}
	},
	{
		.flavor = "nrf5340_net",
		.swd_ops = {
			.target_erase = syncboss_swd_nrf5340_erase_net,
			.target_page_is_erased = syncboss_swd_nrf5340_page_is_erased_net,
			.target_program_write_chunk = syncboss_swd_nrf5340_net_write_chunk,
			.target_get_write_chunk_size = syncboss_nrf5340_get_net_write_chunk_size,
			.target_program_read = syncboss_swd_nrf5340_read,
		}
	},
#endif
#ifdef CONFIG_META_SWD_QM35
	{
		.flavor = "qm35",
		.swd_ops = {
			.target_prepare = qm35xxx_swd_prepare,
			.target_erase = qm35xxx_swd_erase_app,
			.target_program_write_chunk = qm35xxx_swd_write_chunk,
			.target_get_write_chunk_size = qm35xxx_get_write_chunk_size,
			.target_finalize = qm35xxx_swd_finalize,
		}
	},
#endif
#ifdef CONFIG_META_SWD_HUBERT
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
#endif
#ifdef CONFIG_META_SWD_STM32G0
	{
		.flavor = "stm32g0",
		.swd_ops = {
			.target_prepare = stm32g0_swd_prepare,
			.target_erase = stm32g0_swd_erase_app,
			.target_program_write_chunk = stm32g0_swd_write_chunk,
			.target_get_write_chunk_size = stm32g0_get_write_chunk_size,
			.target_finalize = stm32g0_swd_finalize,
		}
	},
#endif
#ifdef CONFIG_META_SWD_STM32L4
	{
		.flavor = "stm32l47xxx",
		.swd_ops = {
			.provisioning_read = stm32l47xxx_swd_provisioning_read,
			.provisioning_write = stm32l47xxx_swd_provisioning_write,
			.target_prepare = stm32l47xxx_swd_prepare,
			.target_erase = stm32l47xxx_swd_erase_app,
			.target_program_write_chunk = stm32l47xxx_swd_write_chunk,
			.target_get_write_chunk_size = stm32l47xxx_get_write_chunk_size,
		}
	},
#endif
};

static int fwupdate_check_swd_ops(struct device *dev)
{
	bool parent_has_op;
	bool child_has_op = false;
	int index;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *mcudata = &devdata->mcu_data;

	parent_has_op = !!mcudata->swd_ops.target_erase;
	for (index = 0; index < devdata->num_children; index++)
		child_has_op |= !!devdata->child_mcu_data[index].swd_ops.target_erase;

	if (parent_has_op && child_has_op) {
		dev_err(dev, "Both parent and child may not have a target_erase!");
		return -EINVAL;
	}

	for (index = 0; index < devdata->num_children; index++) {
		if (!!devdata->child_mcu_data[index].swd_ops.target_chip_erase) {
			dev_err(dev, "Child nodes may not have target_chip_erase!");
			return -EINVAL;
		}
	}

	return 0;
}

static int fwupdate_provision_if_needed(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_ops_params *ops = &devdata->mcu_data.swd_ops;
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

int fwupdate_update_chip_erase(struct device *dev)
{
	int status;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (devdata->erase_all && devdata->mcu_data.swd_ops.target_chip_erase) {
		dev_warn(dev, "Performing full chip erase!");
		status = devdata->mcu_data.swd_ops.target_chip_erase(dev);
		if (status)
			return status;
	}

	return 0;
}

int fwupdate_update_prepare(struct device *dev)
{
	int status;
	int index;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (devdata->mcu_data.swd_ops.target_prepare) {
		status = devdata->mcu_data.swd_ops.target_prepare(dev);
		if (status)
			return status;
	}

	if (devdata->num_children == 0)
		return 0;

	for (index = 0; index < devdata->num_children; index++) {
		if (devdata->child_mcu_data[index].swd_ops.target_prepare) {
			status = devdata->child_mcu_data[index].swd_ops.target_prepare(dev);
			if (status)
				return status;
		}
	}

	return 0;
}

static int fwupdate_update_write_single_app(struct device *dev, struct swd_mcu_data *mcudata, int bytes_to_skip)
{
	int status;
	size_t chunk_size;
	int offset = 0;
	size_t bytes_to_write = 0;
	const struct firmware *fw = mcudata->fw;

	if (!mcudata->swd_ops.target_get_write_chunk_size ||
		!mcudata->swd_ops.target_program_write_chunk)
		return 0;

	chunk_size = mcudata->swd_ops.target_get_write_chunk_size(dev);
	BUG_ON((bytes_to_skip % chunk_size) != 0);
	atomic_set(&mcudata->fw_chunks_to_write, DIV_ROUND_UP(fw->size, chunk_size));

	// Write the chunks last to first so that an incomplete firmware image is unbootable.
	bytes_to_write = fw->size % chunk_size;
	if (bytes_to_write == 0)
		bytes_to_write = chunk_size;

	for (offset = fw->size - bytes_to_write; offset >= bytes_to_skip; offset -= chunk_size) {
		dev_dbg(dev, "Writing %lu-byte chunk @ 0x%x ", bytes_to_write, offset);
		status = mcudata->swd_ops.target_program_write_chunk(
						  dev,
						  offset,
						  &fw->data[offset],
						  bytes_to_write);
		if (status)
			return status;

		bytes_to_write = chunk_size;
		atomic_inc(&mcudata->fw_chunks_written);
	}

	return 0;
}

static int fwupdate_update_single_app(
		struct device *dev, struct swd_mcu_data *mcudata, bool erase_all,
		bool force_bootloader_update)
{
	int status;
	size_t bytes_written = 0;
	size_t pages_to_skip = 0;
	const struct firmware *fw = mcudata->fw;

	if (!fw) {
		dev_err(dev, "%s: No firmware for flavor", mcudata->fw_path);
		return -ENOENT;
	}

	dev_info(dev,
		 "Updating firmware for '%s': Image '%s'size: %zd bytes...",
		 mcudata->target_flavor, mcudata->fw_path, fw->size);
#if defined(CONFIG_DYNAMIC_DEBUG)
	print_hex_dump_bytes("Firmware binary to write: ", DUMP_PREFIX_OFFSET,
			     fw->data, fw->size);
#endif

	if (!erase_all) {
		dev_info(dev, "%s: Erasing firmware app", mcudata->fw_path);
		status = mcudata->swd_ops.target_erase(dev);
		if (status)
			return status;

		if (!force_bootloader_update) {
			while (pages_to_skip < mcudata->flash_info.num_protected_bootloader_pages) {
				if (!mcudata->swd_ops.target_page_is_erased(dev, pages_to_skip))
					pages_to_skip++;
				else
					break;
			}
			if (pages_to_skip > 0) {
				dev_warn(dev, "%s: Bootloader pages are protected! Writes to them will be skipped.", mcudata->fw_path);
				bytes_written = pages_to_skip * mcudata->flash_info.page_size;
			}
		} else if (mcudata->flash_info.num_protected_bootloader_pages > 0) {
			dev_warn(dev, "%s: Ignoring bootloader page protection!", mcudata->fw_path);
		}
	}

	return fwupdate_update_write_single_app(dev, mcudata, bytes_written);
}

int fwupdate_update_app(struct device *dev)
{
	int index;
	int status;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *childdata;

	// If there are no children, must update the parent
	if (devdata->num_children == 0) {
		status = fwupdate_update_single_app(
				dev, &devdata->mcu_data, devdata->erase_all,
				devdata->data_hdr->force_bootloader_update);
		if (status)
			return status;
	} else {
		for (index = 0; index < devdata->num_children; index++) {
			childdata = &devdata->child_mcu_data[index];
			status = fwupdate_update_single_app(
				dev, childdata, devdata->erase_all,
				devdata->data_hdr->force_bootloader_update);
			if (status)
				return status;
		}
	}

	return 0;
}

static int fwupdate_update_finalize(struct device *dev)
{
	int status;
	int index;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	// Finalize children first to maintain a reverse order from prepare
	for (index = 0; index < devdata->num_children; index++) {
		if (devdata->child_mcu_data[index].swd_ops.target_finalize) {
			status = devdata->child_mcu_data[index].swd_ops.target_finalize(dev);
			if (status)
				return status;
		}
	}

	if (devdata->mcu_data.swd_ops.target_finalize) {
		status = devdata->mcu_data.swd_ops.target_finalize(dev);
		if (status)
			return status;
	}

	return 0;
}

void fwupdate_release_all_firmware(struct device *dev)
{
	int index;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if (devdata->mcu_data.fw) {
		release_firmware(devdata->mcu_data.fw);
		devdata->mcu_data.fw = NULL;
	}

	for (index = 0; index < devdata->num_children; index++) {
		if (devdata->child_mcu_data[index].fw) {
			release_firmware(devdata->child_mcu_data[index].fw);
			devdata->child_mcu_data[index].fw = NULL;
		}
	}
}

static int fwupdate_update_firmware(struct device *dev)
{
	int status;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *mcudata = &devdata->mcu_data;

	status = fwupdate_check_swd_ops(dev);
	if (status)
		goto error;

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

	swd_init(dev);
	swd_halt(dev);

	status = fwupdate_update_prepare(dev);
	if (status)
		goto error;

	status = fwupdate_update_chip_erase(dev);
	if (status)
		goto error;

	status = fwupdate_update_app(dev);
	if (status)
		goto error;

	status = fwupdate_provision_if_needed(dev);
	if (status) {
		dev_err(dev, "Provisioning failed!\n");
		goto error;
	}

	status = fwupdate_update_finalize(dev);
	if (status)
		goto error;

	swd_reset(dev);
	swd_flush(dev);

	if (gpio_is_valid(devdata->gpio_reset)) {
		dev_info(dev, "Re-applying MCU reset");
		gpio_set_value(devdata->gpio_reset, 0);

		/*
		 * Delay since we don't know when the driver that owns this
		 * MCU will want to wake it again.
		 */
		msleep(RESET_GPIO_TIME_MS);
	}

	dev_info(dev, "Done updating firmware.");

 error:
	swd_deinit(dev);
	kfree(devdata->data_hdr);
	devdata->data_hdr = NULL;
	devdata->provisioning = NULL;
	if (devdata->swd_core && regulator_disable(devdata->swd_core))
		dev_err(dev, "Regulator failed to disable");
	atomic_set(&mcudata->fw_chunks_written, 0);
	atomic_set(&mcudata->fw_chunks_to_write, 0);

	return status;
}

ssize_t fwupdate_update_firmware_show(struct device *dev, char *buf)
{
	int status = 0;
	ssize_t retval = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, status);
		return status;
	}

	if (devdata->fw_update_state == FW_UPDATE_STATE_IDLE) {
		retval = scnprintf(buf, PAGE_SIZE, FW_UPDATE_STATE_IDLE_STR "\n");
	} else if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW) {
		retval = scnprintf(buf, PAGE_SIZE,
				   FW_UPDATE_STATE_WRITING_STR
				   " %i/%i\n",
				   atomic_read(&devdata->mcu_data.fw_chunks_written),
				   atomic_read(&devdata->mcu_data.fw_chunks_to_write));
	} else if (devdata->fw_update_state == FW_UPDATE_STATE_ERROR) {
		retval = scnprintf(buf, PAGE_SIZE, FW_UPDATE_STATE_ERROR_STR "\n");
	} else {
		/* In an unknown state */
		BUG_ON(1);
	}

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static void fwupdate_swd_workqueue_fw_update(void *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int status = fwupdate_update_firmware(dev);

	fwupdate_release_all_firmware(dev);
	if (devdata->mcu_state_locked) {
		devdata->mcu_state_unlock(dev);
		devdata->mcu_state_locked = false;
	}

	mb(); /* Ensure FW update attempt is complete before allowing another to start */
	devdata->fw_update_state = status ? FW_UPDATE_STATE_ERROR : FW_UPDATE_STATE_IDLE;
}

static int fwupdate_populate_data(struct device *dev, const char *buf,
				      size_t count)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct fwupdate_header *hdr = (struct fwupdate_header *)buf;
	struct fwupdate_provisioning *p = devdata->swd_provisioning ? hdr->provisioning : NULL;
	size_t expected_count = sizeof(struct fwupdate_header);

	if (count < sizeof(struct fwupdate_header)) {
		dev_err(dev, "Failed to read update data header\n");
		return -EINVAL;
	}

	if (p != NULL && count < offsetof(struct fwupdate_header, provisioning[1])) {
		dev_err(dev, "Failed to read provisioning data header\n");
		return -EINVAL;
	}

	if (p != NULL && p->format_version != 0) {
		dev_err(dev, "Unsupported provisioning format version\n");
		return -EINVAL;
	}

	if (p != NULL) {
		expected_count += sizeof(struct fwupdate_provisioning) + p->data_length;
		if (expected_count != count) {
			dev_err(dev, "Unexpected data length: e=%zu != c=%zu\n", expected_count, count);
			return -EINVAL;
		}
	}

	devdata->data_hdr = kmemdup(buf, expected_count, GFP_KERNEL);
	if (!devdata->data_hdr)
		return -ENOMEM;
	devdata->provisioning = (p != NULL) ? devdata->data_hdr->provisioning : NULL;

	return 0;
}

static int fwupdate_get_single_firmware_image(struct device *dev, struct swd_mcu_data *mcudata)
{
	int status;
	struct flash_info *flash = &mcudata->flash_info;
	size_t max_fw_size = (flash->num_pages * flash->bank_count - flash->num_retained_pages) * flash->page_size;

	status = request_firmware(&mcudata->fw, mcudata->fw_path, dev);
	if (status != 0) {
		if (mcudata->fw_path != NULL) {
			dev_err(dev,
				"request_firmware: %d, Please ensure %s is present.",
				status, mcudata->fw_path);
		} else {
			dev_err(dev,
				"request_firmware: %d, fw_path is null",
				status);
		}
		return status;
	}

	if (mcudata->fw->size > max_fw_size) {
		dev_err(dev,
			"%s: Firmware binary size too large, provided size: %zd, max size: %zd",
			mcudata->fw_path, mcudata->fw->size, max_fw_size);
		status = -ENOMEM;
		goto error_after_request_fw;
	}

	return 0;

error_after_request_fw:
	release_firmware(mcudata->fw);
	mcudata->fw = NULL;

	return status;
}

int fwupdate_get_firmware_images(struct device *dev, struct swd_dev_data *devdata)
{
	int index;
	int status;

	if (devdata->num_children == 0)
		return fwupdate_get_single_firmware_image(dev, &devdata->mcu_data);

	for (index = 0; index < devdata->num_children; index++) {
		status = fwupdate_get_single_firmware_image(dev, &devdata->child_mcu_data[index]);
		if (status)
			goto error_after_populated_fw;
	}

	return 0;

error_after_populated_fw:
	for (index = 0; index < devdata->num_children; index++) {
		if (devdata->child_mcu_data[index].fw) {
			release_firmware(devdata->child_mcu_data[index].fw);
			devdata->child_mcu_data[index].fw = NULL;
		}
	}

	return status;
}

/**
 * fwupdate_update_firmware_store() - handle a write to the
 * fwupdate_update_firmware file
 * @dev: This device
 * @buf: Data buffer written from userspace. Expected to be opaque data starting
 *       with a 'struct fwupdate_provisioning' header.
 * @count: The number of bytes in buffer
 *
 * Return: @count upon success, else a negative value to indicate an error.
 */
ssize_t fwupdate_update_firmware_store(struct device *dev, const char *buf,
				       size_t count)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, status);
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
	if (devdata->mcu_state_lock && devdata->get_syncboss_is_streaming) {
		devdata->mcu_state_lock(dev);
		devdata->mcu_state_locked = true;
		if (devdata->get_syncboss_is_streaming(dev)) {
			dev_err(dev, "Cannot update firmware while the MCU is busy");
			status = -EBUSY;
			goto error;
		}
	}

	status = fwupdate_get_firmware_images(dev, devdata);
	if (status)
		goto error;

	status = fwupdate_populate_data(dev, buf, count);
	if (status)
		goto error;

	devdata->fw_update_state = FW_UPDATE_STATE_WRITING_TO_HW;

	fw_queue_work(devdata->workqueue, dev, fwupdate_swd_workqueue_fw_update, NULL);

	mutex_unlock(&devdata->state_mutex);

	return count;

error:
	if (devdata->mcu_state_locked) {
		devdata->mcu_state_unlock(dev);
		devdata->mcu_state_locked = false;
	}
	mutex_unlock(&devdata->state_mutex);
	return status;
}

int fwupdate_init_swd_ops(struct device *dev, struct swd_mcu_data *mcudata)
{
	int i;
	int flavor_len = strlen(mcudata->target_flavor);
	struct swd_ops_params *ops = NULL;

	for (i = 0; i < ARRAY_SIZE(archs_params); i++) {
		if (strlen(archs_params[i].flavor) != flavor_len)
			continue;
		if (!strncmp(mcudata->target_flavor, archs_params[i].flavor, flavor_len)) {
			ops = &archs_params[i].swd_ops;
			break;
		}
	}

	if (!ops) {
		dev_err(dev, "No swd_ops for flavor: %s", mcudata->target_flavor);
		return -EINVAL;
	}

	mcudata->swd_ops = *ops;
	return 0;
}

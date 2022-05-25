// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/slab.h>

#include "swd.h"
#include "swd_registers_stm32g0.h"
#include "safetyboss_swd_ops.h"

#define DEVID_STM32G071xx	0x460

static bool safetyboss_swd_is_flash_ready(struct device *dev)
{
	return !(swd_memory_read(dev, SWD_STM32G0_FLASH_SR) & SWD_STM32G0_FLASH_SR_BUSY1);
}

static int safetyboss_swd_get_errors(struct device *dev)
{
	u32 devid = swd_memory_read(dev, SWD_STM32G0_DBG_IDCODE) & SWD_STM32G0_DBG_IDCODE_DEVID_MASK;
	if (devid != DEVID_STM32G071xx) {
		dev_err(dev, "(SafetyBoss SWD) Unrecognized devid: %u", devid);
		return -EINVAL;
	}
	return (int) (swd_memory_read(dev, SWD_STM32G0_FLASH_SR) & SWD_STM32G0_FLASH_ERR_Mask);
}

static int safetyboss_swd_wait_for_flash_ready(struct device *dev)
{
	const u64 SWD_READY_TIMEOUT_MS = 1000;
	u64 timeout_time_ns = 0;

	timeout_time_ns =
		ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (safetyboss_swd_is_flash_ready(dev))
			return 0;

		// According to datasheet, longest operation takes up to 40ms
		usleep_range(1000, 2000);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if (safetyboss_swd_is_flash_ready(dev))
		return 0;

	dev_err(dev, "(SafetyBoss SWD) Flash not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

static void safetyboss_swd_clear_errors(struct device *dev)
{
	swd_memory_write(dev, SWD_STM32G0_FLASH_SR, SWD_STM32G0_FLASH_ERR_Mask);
}

int safetyboss_swd_prepare(struct device *dev)
{
	int status = safetyboss_swd_wait_for_flash_ready(dev);

	if (status != 0)
		return status;

	/* Unlock flash */
	swd_memory_write(dev, SWD_STM32G0_FLASH_KEYR, SWD_STM32G0_FLASH_KEYR_Key1);
	swd_memory_write(dev, SWD_STM32G0_FLASH_KEYR, SWD_STM32G0_FLASH_KEYR_Key2);

	// Verify it's unlocked
	status = swd_memory_read(dev, SWD_STM32G0_FLASH_CR) & SWD_STM32G0_FLASH_CR_LOCK;
	if (status != 0)
		dev_err(dev, "(SafetyBoss SWD) Unable to unlock flash");

	return 0;
}

int safetyboss_swd_erase_app(struct device *dev)
{
	int status = 0;
	u32 cr_to_write = 0;
	u32 page_to_erase = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	const int flash_pages_to_erase = flash->num_pages - flash->num_retained_pages;

	BUG_ON(flash_pages_to_erase < 0);
	BUG_ON(flash_pages_to_erase > (SWD_STM32G0_FLASH_CR_PNB_Mask>>SWD_STM32G0_FLASH_CR_PNB_Pos));

	status = safetyboss_swd_wait_for_flash_ready(dev);
	if (status != 0)
		return status;

	safetyboss_swd_clear_errors(dev);

	/* Note: Instead of issuing an ERASEALL command, we erase each page
	 * separately.  This is to preserve the values in the emulated EEPROM region
	 * where we store some data that shouldn't be touched by firmware update.
	 */
	for (page_to_erase = 0; page_to_erase < flash_pages_to_erase; page_to_erase++) {
		cr_to_write = page_to_erase << SWD_STM32G0_FLASH_CR_PNB_Pos;
		cr_to_write |= SWD_STM32G0_FLASH_CR_STRT | SWD_STM32G0_FLASH_CR_PER;
		swd_memory_write(dev, SWD_STM32G0_FLASH_CR, cr_to_write);

		status = safetyboss_swd_wait_for_flash_ready(dev);
		if (status)
			return status;
	}

	// Flash has been wiped. Leave writing enabled until the next reset
	swd_memory_write(dev, SWD_STM32G0_FLASH_CR, SWD_STM32G0_FLASH_CR_PG);

	return safetyboss_swd_get_errors(dev);
}

size_t safetyboss_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	return devdata->flash_info.block_size;
}

int safetyboss_swd_write_chunk(struct device *dev, int addr, const u8 *data, size_t len)
{
	int bytes_left = len;
	int i = 0;
	u32 value = 0;

	BUG_ON(len == 0);
	BUG_ON(addr < 0);

	for (i = 0; i < len; i += sizeof(u32)) {
		bytes_left = len - i;
		if (bytes_left >= sizeof(u32)) {
			value = *((u32 *)&data[i]);
		} else {
			value = 0;
			memcpy(&value, &data[i], bytes_left);
		}

		if (i == 0)
			swd_memory_write(dev, SWD_STM32G0_FLASH_MEM_START_ADDR + addr + i, value);
		else
			swd_memory_write_next(dev, value);

		// Wait for tprog after every double word write, per datasheet.
		if (i % (2*sizeof(u32)) != 0)
			usleep_range(125, 140);
	}

	// Finish off the final double word write if necessary.
	if (len % (2*sizeof(u32)) != 0) {
		swd_memory_write_next(dev, 0);
		usleep_range(125, 140);
	}

	return safetyboss_swd_get_errors(dev);
}

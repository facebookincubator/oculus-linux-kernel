// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/slab.h>

#include "swd.h"
#include "swd_registers_stm32g0.h"
#include "stm32g0_swd_ops.h"

#define DEVID_STM32G071xx	0x460

static bool stm32g0_swd_is_flash_ready(struct device *dev)
{
	return !(swd_memory_read(dev, SWD_STM32G0_FLASH_SR) & SWD_STM32G0_FLASH_SR_BUSY1);
}

static int stm32g0_swd_get_errors(struct device *dev)
{
	u32 devid = swd_memory_read(dev, SWD_STM32G0_DBG_IDCODE) & SWD_STM32G0_DBG_IDCODE_DEVID_MASK;
	if (devid != DEVID_STM32G071xx) {
		dev_err(dev, "(stm32g0 SWD) Unrecognized devid: %u", devid);
		return -EINVAL;
	}
	return (int) (swd_memory_read(dev, SWD_STM32G0_FLASH_SR) & SWD_STM32G0_FLASH_ERR_Mask);
}

static int stm32g0_swd_wait_for_flash_ready(struct device *dev)
{
	const u64 SWD_READY_TIMEOUT_MS = 1000;
	u64 timeout_time_ns = 0;

	timeout_time_ns =
		ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (stm32g0_swd_is_flash_ready(dev))
			return 0;

		// According to datasheet, longest operation takes up to 40ms
		usleep_range(1000, 2000);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if (stm32g0_swd_is_flash_ready(dev))
		return 0;

	dev_err(dev, "(stm32g0 SWD) Flash not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

static void stm32g0_swd_clear_errors(struct device *dev)
{
	swd_memory_write(dev, SWD_STM32G0_FLASH_SR, SWD_STM32G0_FLASH_ERR_Mask);
}

int stm32g0_swd_prepare(struct device *dev)
{
	int status;

	// When firmware is naughty and tries to write to addr 0 (NULL), this begins a flash write
	// that blocks all FLASH_CR operations. The only way to unblock is by writing another word.
	if (swd_memory_read(dev, SWD_STM32G0_FLASH_SR) & SWD_STM32G0_FLASH_SR_CFGBSY)
		swd_memory_write(dev, 0, 0);

	// When device is already unlocked, attempting to unlock it again will cause a hardfault.
	status = swd_memory_read(dev, SWD_STM32G0_FLASH_CR);
	dev_info(dev, "FLASH_CR = 0x%x", status);
	if ((status & SWD_STM32G0_FLASH_CR_LOCK) == 0)
		return 0;

	status = stm32g0_swd_wait_for_flash_ready(dev);
	if (status != 0)
		return status;

	/* Unlock flash */
	swd_memory_write(dev, SWD_STM32G0_FLASH_KEYR, SWD_STM32G0_FLASH_KEYR_Key1);
	swd_memory_write(dev, SWD_STM32G0_FLASH_KEYR, SWD_STM32G0_FLASH_KEYR_Key2);

	// Verify it's unlocked
	status = swd_memory_read(dev, SWD_STM32G0_FLASH_CR) & SWD_STM32G0_FLASH_CR_LOCK;
	if (status != 0)
		dev_err(dev, "(stm32g0 SWD) Unable to unlock flash");

	return status;
}

int stm32g0_swd_erase_app(struct device *dev)
{
	int status = 0;
	u32 cr_to_write = 0;
	u32 page_to_erase = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->mcu_data.flash_info;
	const int flash_pages_to_erase = flash->num_pages - flash->num_retained_pages;

	BUG_ON(flash_pages_to_erase < 0);
	BUG_ON(flash_pages_to_erase > (SWD_STM32G0_FLASH_CR_PNB_Mask>>SWD_STM32G0_FLASH_CR_PNB_Pos));

	status = stm32g0_swd_wait_for_flash_ready(dev);
	if (status != 0)
		return status;

	stm32g0_swd_clear_errors(dev);

	/* Note: Instead of issuing an ERASEALL command, we erase each page
	 * separately.  This is to preserve the values in the emulated EEPROM region
	 * where we store some data that shouldn't be touched by firmware update.
	 */
	for (page_to_erase = 0; page_to_erase < flash_pages_to_erase; page_to_erase++) {
		cr_to_write = page_to_erase << SWD_STM32G0_FLASH_CR_PNB_Pos;
		cr_to_write |= SWD_STM32G0_FLASH_CR_STRT | SWD_STM32G0_FLASH_CR_PER;
		swd_memory_write(dev, SWD_STM32G0_FLASH_CR, cr_to_write);

		status = stm32g0_swd_wait_for_flash_ready(dev);
		if (status)
			return status;
	}

	// Flash has been wiped. Leave writing enabled until the next reset
	swd_memory_write(dev, SWD_STM32G0_FLASH_CR, SWD_STM32G0_FLASH_CR_PG);

	return stm32g0_swd_get_errors(dev);
}

size_t stm32g0_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	return devdata->mcu_data.flash_info.block_size;
}

int stm32g0_swd_write_chunk(struct device *dev, int addr, const u8 *data, size_t len)
{
	size_t bytes_left = len;
	int i = 0;
	u32 doubleword[2];
	const u32 tprog_us = 125;
	u32 readback;

	BUG_ON(len == 0);
	BUG_ON(addr < 0);
	BUG_ON((addr % sizeof(doubleword)) != 0);

	for (i = 0; i < len; i += sizeof(doubleword)) {
		bytes_left = len - i;
		memset(doubleword, 0, sizeof(doubleword));
		memcpy(doubleword, &data[i], min(bytes_left, sizeof(doubleword)));

		if (i == 0) {
			swd_memory_write(dev, SWD_STM32G0_FLASH_MEM_START_ADDR + addr, doubleword[0]);
			swd_memory_write_next(dev, doubleword[1]);
		} else {
			// Per datasheet, wait for tprog is necessary at the end of a double word write.
			// Accomplish this by delaying before the first word of the next double word write.
			swd_memory_write_next_delayed(dev, doubleword[0], tprog_us);
			swd_memory_write_next(dev, doubleword[1]);
		}
	}

	// Ensure final double word write is finished.
	usleep_range(tprog_us, tprog_us + 40);

	// Verify the page one word at a time.
	for (i = 0; i < len; i += sizeof(readback)) {
		if (i == 0)
			readback = swd_memory_read(dev, SWD_STM32G0_FLASH_MEM_START_ADDR + addr);
		else
			readback = swd_memory_read_next(dev);

		bytes_left = len - i;
		if (memcmp(&readback, &data[i], min(sizeof(readback), bytes_left)) != 0)
			return -EIO;
	}

	return stm32g0_swd_get_errors(dev);
}

int stm32g0_swd_finalize(struct device *dev) {
	u32 read;
	read = swd_memory_read(dev, SWD_STM32G0_FLASH_ACR);
	dev_info(dev, "FLASH_ACR = 0x%x", read);
	if (read == 0)
	{
		dev_err(dev, "FLASH_ACR returned 0, something went wrong!");
		return -1;
	}

	// now lets figure out if we need to clear the silly empty bit
	if((read & SWD_STM32G0_FLASH_ACR_EMPTY ) == 0 ) {
		return 0;
	}

	// We are clearing this because if the STM32 was reset while it had 0xFFFFFFFF in its
	// first flash address it will jump into a bootloader and continue to boot there
	// unit a POR. if we clear this empty bit that no longer happens and it will
	// boot into user code.
	dev_info(dev, "FLASH_ACR empty bit set, clearing...");
	swd_memory_write(dev, SWD_STM32G0_FLASH_ACR, read & ~SWD_STM32G0_FLASH_ACR_EMPTY);
	return 0;
}

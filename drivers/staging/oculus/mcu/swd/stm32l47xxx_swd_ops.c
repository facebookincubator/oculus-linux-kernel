// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/slab.h>

#include "swd.h"
#include "swd_registers_stm32l47xxx.h"
#include "stm32l47xxx_swd_ops.h"

static bool stm32l47xxx_swd_is_flash_ready(struct device *dev)
{
	return !(swd_memory_read(dev, SWD_STM32L47XXX_FLASH_SR) & SWD_STM32L47XXX_FLASH_SR_BSY);
}

static int stm32l47xxx_swd_get_errors(struct device *dev)
{
	u32 devid = swd_memory_read(dev, SWD_STM32L47XXX_DBG_IDCODE) & SWD_STM32L47XXX_DBG_IDCODE_DEVID_MASK;

	if (devid != DEVID_STM32L47XXX) {
		dev_err(dev, "(stm324l7xxx SWD) Unrecognized devid: %u", devid);
		return -EINVAL;
	}
	return (int) (swd_memory_read(dev, SWD_STM32L47XXX_FLASH_SR) & SWD_STM32L47XXX_FLASH_SR_ERR_Mask);
}

static int stm32l47xxx_swd_wait_for_flash_ready(struct device *dev)
{
	const u64 SWD_READY_TIMEOUT_MS = 1000;
	u64 timeout_time_ns = 0;

	timeout_time_ns =
		ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (stm32l47xxx_swd_is_flash_ready(dev))
			return 0;

		usleep_range(1000, 2000);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if (stm32l47xxx_swd_is_flash_ready(dev))
		return 0;

	dev_err(dev, "(stm32l47xxx SWD) Flash not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

static void stm32l47xxx_swd_clear_errors(struct device *dev)
{
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_SR, SWD_STM32L47XXX_FLASH_SR_ERR_Mask | SWD_STM32L47XXX_FLASH_SR_EOP);
}

// assumes that the flash has been unlocked and is ready for erasing (not busy)
// waits for the flash to be ready after erasing
static int erase_page(struct device *dev, u32 page_to_erase)
{
	int status;
	u32 cr_to_write = page_to_erase << SWD_STM32L47XXX_FLASH_CR_PNB_Pos;

	cr_to_write |= SWD_STM32L47XXX_FLASH_CR_STRT | SWD_STM32L47XXX_FLASH_CR_PER;
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_CR, cr_to_write);

	status = stm32l47xxx_swd_wait_for_flash_ready(dev);
	return status;
}

int stm32l47xxx_swd_prepare(struct device *dev)
{
	int status = stm32l47xxx_swd_wait_for_flash_ready(dev);

	if (status != 0)
		return status;

	/* Set flash control register */
	swd_memory_read(dev, SWD_STM32L47XXX_FLASH_CR);
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_CR, (SWD_STM32L47XXX_FLASH_CR_LOCK | SWD_STM32L47XXX_FLASH_CR_OPTLOCK));
	swd_memory_read(dev, SWD_STM32L47XXX_FLASH_CR);

	/* Unlock flash */
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_KEYR, SWD_STM32L47XXX_FLASH_KEYR_Key1);
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_KEYR, SWD_STM32L47XXX_FLASH_KEYR_Key2);

	/* Verify it's unlocked */
	status = swd_memory_read(dev, SWD_STM32L47XXX_FLASH_CR) & SWD_STM32L47XXX_FLASH_CR_LOCK;
	if (status != 0)
		dev_err(dev, "(stm32l47xx SWD) Unable to unlock flash");

	return 0;
}

int stm32l47xxx_swd_erase_app(struct device *dev)
{
	int status = 0;
	u32 page_to_erase = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
    struct swd_mcu_data *mcudata = &devdata->mcu_data;
	struct flash_info *flash = &mcudata->flash_info;
	const int flash_pages_to_erase = flash->num_pages * flash->bank_count - flash->num_retained_pages;

	BUG_ON(flash_pages_to_erase < 0);
	BUG_ON(flash_pages_to_erase > ((SWD_STM32L47XXX_FLASH_CR_BKER | SWD_STM32L47XXX_FLASH_CR_PNB_Mask)>>SWD_STM32L47XXX_FLASH_CR_PNB_Pos));

	status = stm32l47xxx_swd_wait_for_flash_ready(dev);
	if (status != 0)
		return status;

	stm32l47xxx_swd_clear_errors(dev);

	/* Note: Instead of issuing an ERASEALL command, we erase each page
	 * separately.  This is to preserve the values in the emulated EEPROM region
	 * where we store some data that shouldn't be touched by firmware update.
	 */
	for (page_to_erase = 0; page_to_erase < flash_pages_to_erase; page_to_erase++) {
		status = erase_page(dev, page_to_erase);
		if (status)
			return status;
	}

	/* Flash has been wiped. Leave writing enabled until the next reset */
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_CR, SWD_STM32L47XXX_FLASH_CR_PG);

	return stm32l47xxx_swd_get_errors(dev);
}

size_t stm32l47xxx_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
    struct swd_mcu_data *mcudata = &devdata->mcu_data;

	return mcudata->flash_info.block_size;
}

int stm32l47xxx_swd_write_chunk(struct device *dev, int addr, const u8 *data, size_t len)
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
			swd_memory_write(dev, SWD_STM32L47XXX_FLASH_MEM_START_ADDR + addr + i, value);
		else
			swd_memory_write_next(dev, value);

		/* Wait for tprog after every double word write, per datasheet. */
		if (i % (2*sizeof(u32)) != 0)
			usleep_range(91, 106);
	}

	/* Finish off the final double word write if necessary. */
	if (len % (2*sizeof(u32)) != 0) {
		swd_memory_write_next(dev, 0);
		usleep_range(91, 106);
		stm32l47xxx_swd_wait_for_flash_ready(dev);
	}

	return stm32l47xxx_swd_get_errors(dev);
}

int stm32l47xxx_swd_read(struct device *dev, int addr, u8 *dest,
		    size_t len)
{
	int w, words = len / sizeof(u32);
	int b, bytes = len % sizeof(u32);

	if (addr % sizeof(u32) != 0) {
		dev_err(dev, "Read start address of 0x%08X isn't word-aligned.\n",
			addr);
		return -EINVAL;
	}

	for (w = 0; w < words; w++)
		((u32 *)dest)[w] = w == 0 ? swd_memory_read(dev, addr) : swd_memory_read_next(dev);

	if (bytes) {
		int pos = w * sizeof(u32);
		u32 val = swd_memory_read(dev, addr + pos);
		for (b = 0; b < bytes; b++)
			dest[pos + b] = ((u8 *)&val)[b];
	}

	return 0;
}

int stm32l47xxx_swd_provisioning_read(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *mcudata = &devdata->mcu_data;
	struct flash_info *flash = &mcudata->flash_info;

	if (addr < SWD_STM32L47XXX_NVM_EEPROM_BASE || addr + len > SWD_STM32L47XXX_NVM_EEPROM_BASE + flash->page_size) {
		dev_err(dev, "Provisioning data addr/len is not within EEPROM region\n");
		return -EINVAL;
	}

	/* Offset data by the start of flash */
	status = stm32l47xxx_swd_read(dev, SWD_STM32L47XXX_FLASH_MEM_START_ADDR + addr, data, len);
	if (status)
		dev_err(dev, "Failed to read EEPROM region\n");

	return status;
}

int stm32l47xxx_swd_provisioning_write(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;
	u8 *whole_page = NULL;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct swd_mcu_data *mcudata = &devdata->mcu_data;
	struct flash_info *flash = &mcudata->flash_info;
	u32 page_offset = flash->num_pages * flash->bank_count - 1;
	size_t bytes_to_write = 0;
	size_t bytes_written = 0;
	size_t bytes_left = 0;
	size_t chunk_size;

	if (addr < SWD_STM32L47XXX_NVM_EEPROM_BASE || addr + len > SWD_STM32L47XXX_NVM_EEPROM_BASE + flash->page_size) {
		dev_err(dev, "Provisioning data does not fit within EEPROM region\n");
		return -EINVAL;
	}

	/* Read the whole page of flash, as not every provisioning write
	 * writes the entire region. This can result in data loss if a power
	 * outage occurs during this proces, however the data stored here can
	 * always be reflashed easily.
	 */
	whole_page = kmalloc(flash->page_size, GFP_KERNEL);
	if (!whole_page)
		return -ENOMEM;

	status = stm32l47xxx_swd_provisioning_read(dev, SWD_STM32L47XXX_NVM_EEPROM_BASE, whole_page, flash->page_size);
	if (status)
		goto error;

	memcpy(whole_page + (addr - SWD_STM32L47XXX_NVM_EEPROM_BASE), data, len);
	chunk_size = stm32l47xxx_get_write_chunk_size(dev);

	status = erase_page(dev, page_offset);
	if (status)
		goto error;

	/* enable programming */
	swd_memory_write(dev, SWD_STM32L47XXX_FLASH_CR, SWD_STM32L47XXX_FLASH_CR_PG);
	status = stm32l47xxx_swd_get_errors(dev);
	if (status)
		goto error;

	while (bytes_written < flash->page_size) {
		bytes_left = flash->page_size - bytes_written;
		bytes_to_write = min(bytes_left, chunk_size);
		status = stm32l47xxx_swd_write_chunk(dev,
			page_offset * flash->page_size + bytes_written,
			&whole_page[bytes_written],
			bytes_to_write);
		if (status)
			goto error;

		bytes_written += bytes_to_write;
	}

error:
	kfree(whole_page);

	if (status)
		dev_err(dev, "provisioning write failure 0x%08x. possible loss of data", status);

	return status;
}

// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/slab.h>

#include "swd.h"
#include "swd_registers_nrf.h"
#include "syncboss_swd_ops.h"

static int syncboss_swd_is_nvmc_ready(struct device *dev)
{
	return (swd_memory_read(dev, SWD_NRF_NVMC_READY) &
		SWD_NRF_NVMC_READY_BM) == SWD_NRF_NVMC_READY_Ready;
}

static int syncboss_swd_wait_for_nvmc_ready(struct device *dev)
{
	const u64 SWD_READY_TIMEOUT_MS = 500;
	u64 timeout_time_ns = 0;

	timeout_time_ns =
	    ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (syncboss_swd_is_nvmc_ready(dev))
			return 0;

		/*
		 * From the datasheet, page erase operations take
		 * 2.05ms to 89.7ms.
		 */
		usleep_range(1000, 2000);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if (syncboss_swd_is_nvmc_ready(dev))
		return 0;

	dev_err(dev, "SyncBoss SWD NVMC not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

bool syncboss_swd_page_is_erased(struct device *dev, u32 page)
{
	const u32 FLASH_ERASED_VALUE = 0xffffffff;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	int page_addr = page * flash->page_size;
	int w;
	u32 rd;

	for (w = 0; w < flash->page_size; w += sizeof(u32)) {
		rd = w == 0 ? swd_memory_read(dev, page_addr) : swd_memory_read_next(dev);
		if (rd != FLASH_ERASED_VALUE)
			return false;
	}
	return true;
}

int syncboss_swd_erase_app(struct device *dev)
{
	int status = 0;
	int x;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	int flash_pages_to_erase = flash->num_pages - flash->num_retained_pages;
	BUG_ON(flash_pages_to_erase < 0);

	swd_memory_write(dev, SWD_NRF_NVMC_CONFIG,
			 SWD_NRF_NVMC_CONFIG_EEN);

	/* Note: Instead of issuing an ERASEALL command, we erase each page
	 * separately. This is to preserve the UICR, bootloader, and end of flash
	 * where we store some data that shouldn't be touched by firmware update.
	 */

	for (x = flash->num_protected_bootloader_pages; x < flash_pages_to_erase; ++x) {
		swd_memory_write(dev, SWD_NRF_NVMC_ERASEPAGE,
			x * flash->page_size);
		status = syncboss_swd_wait_for_nvmc_ready(dev);
		if (status != 0)
			goto error;
	}

	// Flash has been wiped. Leave writing enabled until the next reset
	swd_memory_write(dev, SWD_NRF_NVMC_CONFIG, SWD_NRF_NVMC_CONFIG_WEN);
	return status;

error:
	swd_memory_write(dev, SWD_NRF_NVMC_CONFIG, SWD_NRF_NVMC_CONFIG_REN);
	return status;
}

size_t syncboss_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	return devdata->flash_info.block_size;
}

int syncboss_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			     size_t len)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int block_size = devdata->flash_info.block_size;
	int status = 0;
	int i = 0;
	u32 value = 0;
	u32 bytes_left = 0;

	if (addr % block_size != 0) {
		dev_err(dev, "Write start address of 0x%08X isn't on a block boundary.\n",
			addr);
		return -EINVAL;
	}

	for (i = 0; i < len; i += sizeof(u32)) {
		bytes_left = len - i;
		if (bytes_left >= sizeof(u32)) {
			value = *((u32 *)&data[i]);
		} else {
			value = 0;
			memcpy(&value, &data[i], bytes_left);
		}

		if (i == 0)
			swd_memory_write(dev, addr + i, value);
		else
			swd_memory_write_next(dev, value);

		// Per the datasheet, writes take at most 338us to complete.
		usleep_range(340, 380);
	}

	return status;
}

int syncboss_swd_read(struct device *dev, int addr, u8 *dest,
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

static int syncboss_swd_erase_uicr(struct device *dev)
{
	int status;

	swd_memory_write(dev, SWD_NRF_NVMC_CONFIG, SWD_NRF_NVMC_CONFIG_EEN);

	swd_memory_write(dev, SWD_NRF_NVMC_ERASEUICR, SWD_NRF_NVMC_ERASEUICR_Start);
	status = syncboss_swd_wait_for_nvmc_ready(dev);

	swd_memory_write(dev, SWD_NRF_NVMC_CONFIG, SWD_NRF_NVMC_CONFIG_WEN);

	return status;
}

int syncboss_swd_provisioning_read(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;

	if (addr < SWD_NRF_UICR_BASE || addr + len >= SWD_NRF_UICR_BASE + SWD_NRF_UICR_SIZE) {
		dev_err(dev, "Provisioning data addr/len is not within UICR\n");
		return -EINVAL;
	}

	status = syncboss_swd_read(dev, addr, data, len);
	if (status)
		dev_err(dev, "Failed to read provisioning data\n");

	return status;
}

int syncboss_swd_provisioning_write(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;
	u8 *uicr_data;
	int uicr_offset = addr - SWD_NRF_UICR_BASE;

	if (addr < SWD_NRF_UICR_BASE || addr + len >= SWD_NRF_UICR_BASE + SWD_NRF_UICR_SIZE) {
		dev_err(dev, "Provisioning data does not fit within UICR\n");
		return -EINVAL;
	}

	// Read-modify-write the entire UICR to satisy erase requirements...

	uicr_data = kmalloc(SWD_NRF_UICR_SIZE, GFP_KERNEL);
	if (!uicr_data)
		return -ENOMEM;

	status = syncboss_swd_read(dev, SWD_NRF_UICR_BASE, uicr_data,
				   SWD_NRF_UICR_SIZE);
	if (status) {
		dev_err(dev, "Failed to read UICR\n");
		goto error;
	}

	memcpy(uicr_data + uicr_offset, data, len);

	syncboss_swd_erase_uicr(dev);
	if (status) {
		dev_err(dev, "Failed to erase UICR\n");
		goto error;
	}

	status = syncboss_swd_write_chunk(dev, SWD_NRF_UICR_BASE, uicr_data,
					  SWD_NRF_UICR_SIZE);
	if (status) {
		dev_err(dev, "Failed to write UICR\n");
		goto error;
	}

error:
	kfree(uicr_data);
	return status;
}

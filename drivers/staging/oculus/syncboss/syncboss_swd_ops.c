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

static bool ctrlap_try_eraseall(struct device *dev)
{
	static const u32 POLLING_INTERVAL_MS = 100;
	static const u32 TIMEOUT_MS = 15000;
	int elapsed_ms;

	swd_ap_write(dev, SWD_NRF_APREG_ERASEALL, SWD_NRF_APREG_ERASEALL_Start);
	swd_ap_read(dev, SWD_NRF_APREG_ERASEALLSTATUS);
	for (elapsed_ms = 0; elapsed_ms < TIMEOUT_MS; elapsed_ms += POLLING_INTERVAL_MS) {
		if (swd_ap_read(dev, SWD_NRF_APREG_ERASEALLSTATUS) == SWD_NRF_APREG_ERASEALLSTATUS_Ready)
			break;
		msleep(POLLING_INTERVAL_MS);
	}

	swd_ap_read(dev, SWD_NRF_APREG_APPROTECTSTATUS);
	return
		swd_ap_read(dev, SWD_NRF_APREG_APPROTECTSTATUS) == SWD_NRF_APREG_APPROTECTSTATUS_Disabled;
}

static int disable_hw_approtect(struct device *dev)
{
	u32 part;
	u32 variant;
	char chip_rev;
	bool is_new_approtect;

	part = swd_memory_read(dev, SWD_NRF_FICR_PART);
	dev_info(dev, "part = %x", part);

	variant = swd_memory_read(dev, SWD_NRF_FICR_VARIANT);
	chip_rev = (char)(variant >> 8);
	dev_info(dev, "chip_rev = %c", chip_rev);

	switch (part) {
	case 0x52832:
		is_new_approtect = chip_rev >= 'G';
		break;
	case 0x52840:
		is_new_approtect = chip_rev >= 'F';
		break;
	case 0x52833:
		is_new_approtect = chip_rev >= 'B';
		break;
	default:
		dev_err(dev, "Unsupported Part!");
		return -EINVAL;
	}

	if (is_new_approtect) {
		dev_info(dev, "Disabling APPROTECT in UICR");
		swd_memory_write(dev, SWD_NRF_UICR_APPROTECT, SWD_NRF_UICR_APPROTECT_Disable);
		usleep_range(340, 380);
	}
	return 0;
}

int syncboss_swd_chip_erase(struct device *dev)
{
	/*
	 * https://infocenter.nordicsemi.com/topic/nwp_027/WP/nwp_027/nWP_027_erasing.html
	 *  Erasing all through CTRL-AP
	 *  Use the standard SWD Arm® CoreSight™ DAP protocol to erase all while the CTRL-AP is
	 *  still selected by the DP.
	 *  1. Write the value 0x00000001 to the ERASEALL register (0x004) of the CTRL-AP.
	 *     This will start the ERASEALL operation which erases all flash and RAM on the device.
	 *  2. Read the ERASEALLSTATUS register (0x008) of the CTRL-AP until the value read is 0x00
	 *     or 15 seconds from ERASEALL write has expired.
	 *  3. Write the value 0x1 to RESET register (0x000) of the CTRL-AP to issue a “soft reset”
	 *     to the device and complete the erase and unlocking of the chip.
	 *  4. Write the value 0x0 to RESET register (0x000).
	 *  5. Write the value 0x0 to the ERASEALL register (0x004) of the CTRL-AP.
	 *     This is necessary after the erase sequence is completed
	 *
	 * https://infocenter.nordicsemi.com/topic/ps_nrf52840/dif.html?cp=4_0_0_3_7_1
	 *  Access port protection is disabled by issuing an ERASEALL command via CTRL-AP. Read
	 *  CTRL-AP.APPROTECTSTATUS to ensure that access port protection is disabled, and repeat
	 *  the ERASEALL command if needed.
	 */
	swd_select_ap(dev, SWD_NRF_APSEL_CTRLAP);
	if (!ctrlap_try_eraseall(dev) && !ctrlap_try_eraseall(dev)) {
		dev_err(dev, "ERASEALL + APPROTECT disable failed!");
		return -EINVAL;
	}
	swd_ap_write(dev, SWD_NRF_APREG_RESET, SWD_NRF_APREG_RESET_Reset);
	swd_ap_write(dev, SWD_NRF_APREG_RESET, SWD_NRF_APREG_RESET_NoReset);
	swd_ap_write(dev, SWD_NRF_APREG_ERASEALL, SWD_NRF_APREG_ERASEALL_NoOperation);
	swd_select_ap(dev, SWD_NRF_APSEL_MEMAP);

	// Flash has been wiped. Leave writing enabled until the next reset
	swd_memory_write(dev, SWD_NRF_NVMC_CONFIG, SWD_NRF_NVMC_CONFIG_WEN);

	// Allow firmware to disable APPROTECT on devices supporting the new scheme.
	return disable_hw_approtect(dev);
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

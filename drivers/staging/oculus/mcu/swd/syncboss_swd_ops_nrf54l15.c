// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>

#include "swd.h"
#include "swd_registers_nrf54l15.h"
#include "syncboss_swd_ops_nrf54l15.h"
#include "syncboss_swd_common_ops.h"

// TODO: Check datasheet for range. This is based on local testing.
static const u64 RRAMC_READY_TIMEOUT_MS = 500;

static bool syncboss_swd_nrf54l15_wait_rramc_ready(struct device *dev, u64 timeout_ms)
{
	return syncboss_swd_wait_reg_value(dev, SWD_NRF54L15_RRAMC_READY,
			  SWD_NRF54L15_RRAMC_READY_Ready, timeout_ms);
}

static bool syncboss_swd_nrf54l15_verify_connection(struct device *dev)
{
	int value = 0;
	static const u64 RRAMC_READY_TIMEOUT_MS = 100;

	swd_select_ap(dev, SWD_NRF54L15_APSEL_CM33_AHBAP);
	value = swd_memory_read(dev, SWD_NRF54L15_FICR_PART);
	dev_dbg(dev, "part 0x%x", value);
	if (value != SWD_NRF54L15_FICR_PART_VALUE) {
		dev_err(dev, "part mismatch: received `0x%x`", value);
		return false;
	}

	// TODO: This is returning 0x41414141 which is not documented in the current svd files for both
	// the enga variant and production variants.
	value = swd_memory_read(dev, SWD_NRF54L15_FICR_VARIANT);
	dev_info(dev, "variant 0x%x", value);

	if (syncboss_swd_nrf54l15_wait_rramc_ready(dev, RRAMC_READY_TIMEOUT_MS))
		return -ETIMEDOUT;

	return true;
}

static void ctrlap_try_eraseall(struct device *dev)
{
	/*
	 * https://infocenter.nordicsemi.com/topic/nan_042/APP/nan_production_programming/erasing_via_ctrlap.html
	 * Use the standard Serial Wire Debug (SWD) Arm® CoreSight™ Debug Access Port (DAP) protocol to erase all
	 * while Control Access Port (CTRL-AP) is still selected by the debug port.
	 */
	static const u32 POLLING_INTERVAL_MS = 100;
	// TODO: NRF54L15 has no guidance on tpinr. NRF52xxx is 650ms and 2s seems to be stable in testing.
	static const u32 TIMEOUT_PINR_MS = 2000;
	static const u32 TIMEOUT_MS = 15000;
	int elapsed_ms;


	/*
	 * 1. Write 0x00000001 to the ERASEALL register (0x004) of CTRL-AP.
	 *    This will start the ERASEALL operation which erases all flash and RAM on the device.
	 */
	swd_ap_write(dev, SWD_NRF54L15_CTRLAP_ERASEALL,
		     SWD_NRF54L15_CTRLAP_ERASEALL_Start);

	/*
	 * 2. Read the ERASEALLSTATUS register (0008) of the CTRL-AP until the value read is 0x00 or wait 15 seconds
	 *    after the ERASEALL write has expired.
	 */
	for (elapsed_ms = 0; elapsed_ms < TIMEOUT_MS;
	     elapsed_ms += POLLING_INTERVAL_MS) {
		if (swd_ap_read(dev, SWD_NRF54L15_CTRLAP_ERASEALLSTATUS) ==
		    SWD_NRF54L15_CTRLAP_ERASEALLSTATUS_Ready)
			break;
		msleep(POLLING_INTERVAL_MS);
	}

	swd_ap_write(dev, SWD_NRF54L15_CTRLAP_RESET, SWD_NRF54L15_CTRLAP_RESET_HardReset);
	msleep(TIMEOUT_PINR_MS);
}

int syncboss_swd_nrf54l15_chip_erase(struct device *dev)
{
	// Call init in case a previous operation put the target in a bad state
	swd_init(dev);
	swd_select_ap(dev, SWD_NRF54H20_APSEL_CTRLAP);

	ctrlap_try_eraseall(dev);

	if (!syncboss_swd_nrf54l15_verify_connection(dev))
		return -ENODEV;

	// Leave the chip unlocked for writing after erase
	swd_memory_write(dev, SWD_NRF54L15_RRAMC_CONFIG, SWD_NRF54L15_RRAMC_CONFIG_WEN_Enabled);
	return syncboss_swd_nrf54l15_wait_rramc_ready(dev, RRAMC_READY_TIMEOUT_MS);
}

static int syncboss_swd_nrf54l15_erase_page(struct device *dev, int page)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->mcu_data.flash_info;
	int page_addr = page * flash->page_size;
	int w;

	// NRF54L15 does not have a concept of pages in RRAMC. Mimic this by explicitly
	// writing 0xFFFFFFFF to our logical concept of a page.
	for (w = 0; w < flash->page_size; w += sizeof(u32)) {
		w == 0 ? swd_memory_write(dev, page_addr, 0xFFFFFFFF) :
				 swd_memory_write_next(dev, 0xFFFFFFFF);
	}

	return syncboss_swd_nrf54l15_wait_rramc_ready(dev, 500);
}

int syncboss_swd_nrf54l15_erase_app(struct device *dev)
{
	int working_page = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->mcu_data.flash_info;
	int flash_pages_to_erase = flash->num_pages - flash->num_retained_pages;
	int status = 0;
	bool force_bootloader_update = false;

	if (devdata->data_hdr)
		force_bootloader_update = devdata->data_hdr->force_bootloader_update;

	BUG_ON(flash_pages_to_erase < 0);

	swd_memory_write(dev, SWD_NRF54L15_RRAMC_CONFIG, SWD_NRF54L15_RRAMC_CONFIG_WEN_Enabled);
	if (syncboss_swd_nrf54l15_wait_rramc_ready(dev, RRAMC_READY_TIMEOUT_MS))
		goto error;

	working_page = force_bootloader_update ? 0 : flash->num_protected_bootloader_pages;
	while (working_page < flash_pages_to_erase) {
		status = syncboss_swd_nrf54l15_erase_page(dev, working_page);
		if (status != 0)
			goto error;
		working_page++;
	}

	return status;
error:
	swd_memory_write(dev, SWD_NRF54L15_RRAMC_CONFIG, SWD_NRF54L15_RRAMC_CONFIG_WEN_Disabled);
	return status;
}

bool syncboss_swd_nrf54l15_page_is_erased(struct device *dev, u32 page)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->mcu_data.flash_info;
	int page_addr = page * flash->page_size;
	int w;
	u32 rd;

	for (w = 0; w < flash->page_size; w += sizeof(u32)) {
		rd = w == 0 ? swd_memory_read(dev, page_addr) :
				    swd_memory_read_next(dev);
		if (rd != SWD_NRF54L15_RRAMC_ERASEPAGE_VALUE)
			return false;
	}

	return true;
}

size_t syncboss_swd_nrf54l15_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return devdata->mcu_data.flash_info.block_size;
}

int syncboss_swd_nrf54l15_write_chunk(struct device *dev, int addr,
				     const u8 *data, size_t len)
{
	int status = 0;
	int i = 0;
	u32 value = 0;
	u32 bytes_left = 0;

	swd_select_ap(dev, SWD_NRF54L15_APSEL_CM33_AHBAP);
	status = syncboss_swd_nrf54l15_wait_rramc_ready(dev, 500);
	if (status != 0)
		return status;

	for (i = 0; i < len; i += sizeof(u32)) {
		bytes_left = len - i;
		if (bytes_left >= sizeof(u32)) {
			value = *((u32 *)&data[i]);
		} else {
			value = 0;
			memcpy(&value, &data[i], bytes_left);
		}

		if (i == 0)
			swd_memory_write(dev, addr, value);
		else
			swd_memory_write_next(dev, value);
	}

	return syncboss_swd_nrf54l15_wait_rramc_ready(dev, 500);
}

int syncboss_swd_nrf54l15_read(struct device *dev, int addr, u8 *dest,
		      size_t len)
{
	int w, words = len / sizeof(u32);
	int b, bytes = len % sizeof(u32);

	if (addr % sizeof(u32) != 0) {
		dev_err(dev, "Read start address of 0x%08X isn't word-aligned.",
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

int syncboss_swd_nrf54l15_provisioning_read(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;

	if (addr < SWD_NRF54L15_UICR_USER_BASE || addr + len >= SWD_NRF54L15_UICR_USER_BASE + SWD_NRF54L15_UICR_USER_SIZE) {
		dev_err(dev, "Provisioning data addr/len is not within UICR");
		return -EINVAL;
	}

	status = syncboss_swd_nrf54l15_read(dev, addr, data, len);
	if (status)
		dev_err(dev, "Failed to read provisioning data");

	return status;
}

int syncboss_swd_nrf54l15_provisioning_write(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;

	if (addr < SWD_NRF54L15_UICR_USER_BASE || addr + len >= SWD_NRF54L15_UICR_USER_BASE + SWD_NRF54L15_UICR_USER_SIZE) {
		dev_err(dev, "Provisioning data does not fit within UICR");
		return -EINVAL;
	}

	// We just write to the required portion of UICR as RRAMC does not need to erase blocks
	// in order to perform a write. The starting region of the UICR should remain untouched.
	status = syncboss_swd_nrf54l15_write_chunk(dev, addr, data, len);
	if (status)
		dev_err(dev, "Failed to write UICR");

	return status;
}

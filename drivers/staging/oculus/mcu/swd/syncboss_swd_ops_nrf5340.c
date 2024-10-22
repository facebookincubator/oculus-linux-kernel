// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include "swd.h"
#include "swd_registers_nrf5340.h"
#include "syncboss_swd_ops_nrf5340.h"

struct swd_core_parameters {
	int ctrl_ap;
	int mem_ap;
	int nvmc_ready_register;
	int nvmc_config_register;
};

static void ready_network_core(struct device *dev)
{
	swd_select_ap(dev, SWD_NRF5340_APSEL_APP_MEMAP);
	if (swd_memory_read(dev,
			    SWD_NRF5340_RESET_APP_SECURE_NETWORK_FORCEOFF) ==
	    SWD_NRF5340_RESET_NETWORK_FORCEOFF_Hold)
		swd_memory_write(dev,
				 SWD_NRF5340_RESET_APP_SECURE_NETWORK_FORCEOFF,
				 SWD_NRF5340_RESET_NETWORK_FORCEOFF_Release);
}

static int syncboss_swd_wait_reg_value(struct device *dev, u32 reg, u32 value,
				       u64 timeout)
{
	u64 timeout_time_ns = 0;

	timeout_time_ns = ktime_get_ns() + (timeout * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (swd_memory_read(dev, reg) == value)
			return 0;

		/*
		 * From the datasheet, page erase operations take 87.5ms.
		 */
		usleep_range(1000, 2000);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if (swd_memory_read(dev, reg) == value)
		return 0;

	dev_err(dev, "SyncBoss SWD register %08x not %08x after %llums", reg,
		value, timeout);

	return -ETIMEDOUT;
}

static int syncboss_swd_wait_for_nvmc_ready(struct device *dev, int nvmc_ready_register)
{
	static const u64 NVMC_READY_TIMEOUT_MS = 100;

	return syncboss_swd_wait_reg_value(dev, nvmc_ready_register,
					   SWD_NRF5340_NVMC_READY_Ready,
					   NVMC_READY_TIMEOUT_MS);
}

static void ctrlap_try_eraseall(struct device *dev)
{
	/*
	 * https://infocenter.nordicsemi.com/topic/nan_042/APP/nan_production_programming/erasing_via_ctrlap.html
	 * Use the standard Serial Wire Debug (SWD) Arm® CoreSight™ Debug Access Port (DAP) protocol to erase all
	 * while Control Access Port (CTRL-AP) is still selected by the debug port.
	 */
	static const u32 POLLING_INTERVAL_MS = 100;
	static const u32 TIMEOUT_MS = 15000;
	int elapsed_ms;

	/*
	 * 1. Write 0x00000001 to the ERASEALL register (0x004) of CTRL-AP.
	 *    This will start the ERASEALL operation which erases all flash and RAM on the device.
	 */
	swd_ap_write(dev, SWD_NRF5340_APREG_ERASEALL,
		     SWD_NRF5340_APREG_ERASEALL_Start);

	/*
	 * 2. Read the ERASEALLSTATUS register (0008) of the CTRL-AP until the value read is 0x00 or wait 15 seconds
	 *    after the ERASEALL write has expired.
	 */
	for (elapsed_ms = 0; elapsed_ms < TIMEOUT_MS;
	     elapsed_ms += POLLING_INTERVAL_MS) {
		if (swd_ap_read(dev, SWD_NRF5340_APREG_ERASEALLSTATUS) ==
		    SWD_NRF5340_APREG_ERASEALLSTATUS_Ready)
			break;
		msleep(POLLING_INTERVAL_MS);
	}
}

int syncboss_swd_nrf5340_chip_erase(struct device *dev)
{
	int status;

	swd_select_ap(dev, SWD_NRF5340_APSEL_APP_CTRLAP);
	ctrlap_try_eraseall(dev);

	swd_select_ap(dev, SWD_NRF5340_APSEL_NET_CTRLAP);
	ctrlap_try_eraseall(dev);

	/*
	 * https://infocenter.nordicsemi.com/topic/nan_042/APP/nan_production_programming/connecting.html
	 * Before connecting to the network core, check if it is in Force-OFF mode and not held in reset. To check
	 * if the network core is powered up, do an AHB read of AHB-AP 0 by targeting RESET.NETWORK.FORCEOFF (0x50005614)
	 * in the application core. If the readout is 0, the network core is not in Force-OFF mode. If the readout is 1,
	 * write 0 to it to exit Force-OFF mode.
	 */
	ready_network_core(dev);

	// Flash has been wiped. Leave writing enabled until the next reset
	swd_select_ap(dev, SWD_NRF5340_APSEL_APP_MEMAP);
	swd_memory_write(dev, SWD_NRF5340_NVMC_SECURE_CONFIG,
			 SWD_NRF5340_NVMC_CONFIG_WEN);
	status = syncboss_swd_wait_for_nvmc_ready(dev, SWD_NRF5340_NVMC_SECURE_READY);
	if (status) {
		dev_err(dev, "unable to wait app nvmc");
		return status;
	}

	swd_select_ap(dev, SWD_NRF5340_APSEL_NET_MEMAP);
	swd_memory_write(dev, SWD_NRF5340_NVMC_NET_CONFIG,
			 SWD_NRF5340_NVMC_CONFIG_WEN);

	status = syncboss_swd_wait_for_nvmc_ready(dev, SWD_NRF5340_NVMC_NET_READY);
	if (status) {
		dev_err(dev, "unable to wait net nvmc");
		return status;
	}

	return 0;
}

static int syncboss_nrf5340_erase_flash(struct device *dev, struct swd_mcu_data * const mcudata,
			const struct swd_core_parameters * const params)
{
	int status = 0;
	int x;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &mcudata->flash_info;
	int flash_pages_to_erase = flash->num_pages - flash->num_retained_pages;

	BUG_ON(flash_pages_to_erase < 0);

	/*
	 * https://infocenter.nordicsemi.com/topic/nan_042/APP/nan_production_programming/erase_secure_protect_not_enabled.html
	 */
	swd_select_ap(dev, params->mem_ap);
	swd_memory_write(dev, params->nvmc_config_register,
			 SWD_NRF5340_NVMC_CONFIG_EEN);

	status = syncboss_swd_wait_for_nvmc_ready(dev, params->nvmc_ready_register);
	if (status)
		goto error;

	/* Note: Instead of issuing an ERASEALL command, we erase each page
	 * separately. This is to preserve the UICR, bootloader, and end of flash
	 * where we store some data that shouldn't be touched by firmware update.
	 */
	x = devdata->data_hdr->force_bootloader_update ? 0 : flash->num_protected_bootloader_pages;
	while (x < flash_pages_to_erase) {
		swd_memory_write(dev, x * flash->page_size,
				SWD_NRF5340_NVMC_ERASEPAGE_VALUE);
		status = syncboss_swd_wait_for_nvmc_ready(dev, params->nvmc_ready_register);

		if (status != 0)
			goto error;
		x++;
	}

	// Flash has been wiped. Leave writing enabled until the next reset
	swd_memory_write(dev, params->nvmc_config_register,
			 SWD_NRF5340_NVMC_CONFIG_WEN);
	return status;
error:
	dev_err(dev, "Erase error");
	swd_memory_write(dev, SWD_NRF5340_NVMC_SECURE_CONFIG,
			 SWD_NRF5340_NVMC_CONFIG_REN);
	return status;
}

int syncboss_swd_nrf5340_erase_app(struct device *dev)
{
	struct swd_core_parameters params = {
		.ctrl_ap = SWD_NRF5340_APSEL_APP_CTRLAP,
		.mem_ap = SWD_NRF5340_APSEL_APP_MEMAP,
		.nvmc_ready_register = SWD_NRF5340_NVMC_SECURE_READY,
		.nvmc_config_register = SWD_NRF5340_NVMC_SECURE_CONFIG,
	};
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return syncboss_nrf5340_erase_flash(dev, &devdata->child_mcu_data[0], &params);
}

int syncboss_swd_nrf5340_erase_net(struct device *dev)
{
	struct swd_core_parameters params = {
		.ctrl_ap = SWD_NRF5340_APSEL_NET_CTRLAP,
		.mem_ap = SWD_NRF5340_APSEL_NET_MEMAP,
		.nvmc_ready_register = SWD_NRF5340_NVMC_NET_READY,
		.nvmc_config_register = SWD_NRF5340_NVMC_NET_CONFIG,
	};
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return syncboss_nrf5340_erase_flash(dev, &devdata->child_mcu_data[1], &params);
}

bool syncboss_swd_nrf5340_page_is_erased_app(struct device *dev, u32 page)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->child_mcu_data[0].flash_info;
	int page_addr = page * flash->page_size;
	int w;
	u32 rd;

	for (w = 0; w < flash->page_size; w += sizeof(u32)) {
		rd = w == 0 ? swd_memory_read(dev, page_addr) :
				    swd_memory_read_next(dev);
		if (rd != SWD_NRF5340_NVMC_ERASEPAGE_VALUE)
			return false;
	}

	return true;
}

bool syncboss_swd_nrf5340_page_is_erased_net(struct device *dev, u32 page)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->child_mcu_data[1].flash_info;
	int page_addr = page * flash->page_size;
	int w;
	u32 rd;

	for (w = 0; w < flash->page_size; w += sizeof(u32)) {
		rd = w == 0 ? swd_memory_read(dev, page_addr) :
				    swd_memory_read_next(dev);
		if (rd != SWD_NRF5340_NVMC_ERASEPAGE_VALUE)
			return false;
	}

	return true;
}

static int syncboss_nrf5340_write_chunk(struct device *dev, struct swd_mcu_data * const mcudata,
			const struct swd_core_parameters * const params, int addr, const u8 *data, size_t len)
{
	int block_size = mcudata->flash_info.block_size;
	int status = 0;
	int i = 0;
	u32 value = 0;
	u32 bytes_left = 0;

	swd_select_ap(dev, params->mem_ap);
	status = syncboss_swd_wait_for_nvmc_ready(dev, params->nvmc_ready_register);
	if (status != 0)
		return status;

	if (addr % block_size != 0) {
		dev_err(dev,
			"Write start address of 0x%08X isn't on a block boundary.\n",
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
			swd_memory_write(dev, addr, value);
		else
			swd_memory_write_next(dev, value);
	}

	return syncboss_swd_wait_for_nvmc_ready(dev, params->nvmc_ready_register);
}

int syncboss_swd_nrf5340_app_write_chunk(struct device *dev, int addr,
				     const u8 *data, size_t len)
{
	struct swd_core_parameters params = {
		.ctrl_ap = SWD_NRF5340_APSEL_APP_CTRLAP,
		.mem_ap = SWD_NRF5340_APSEL_APP_MEMAP,
		.nvmc_ready_register = SWD_NRF5340_NVMC_SECURE_READY,
	};
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return syncboss_nrf5340_write_chunk(dev, &devdata->child_mcu_data[0], &params, addr, data, len);
}

int syncboss_swd_nrf5340_net_write_chunk(struct device *dev, int addr,
				     const u8 *data, size_t len)
{
	struct swd_core_parameters params = {
		.ctrl_ap = SWD_NRF5340_APSEL_NET_CTRLAP,
		.mem_ap = SWD_NRF5340_APSEL_NET_MEMAP,
		.nvmc_ready_register = SWD_NRF5340_NVMC_NET_READY,
	};
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return syncboss_nrf5340_write_chunk(dev, &devdata->child_mcu_data[1], &params, SWD_NRF5340_NVMC_NET_START_ADDR + addr, data, len);
}

size_t syncboss_nrf5340_get_app_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return devdata->child_mcu_data[0].flash_info.block_size;
}

size_t syncboss_nrf5340_get_net_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return devdata->child_mcu_data[1].flash_info.block_size;
}

int syncboss_swd_nrf5340_read(struct device *dev, int addr, u8 *dest,
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

int syncboss_swd_nrf5340_finalize(struct device *dev)
{
	swd_select_ap(dev, SWD_NRF5340_APSEL_APP_MEMAP);
	swd_memory_write(dev, SWD_NRF5340_NVMC_SECURE_CONFIG,
			 SWD_NRF5340_NVMC_CONFIG_REN);

	swd_select_ap(dev, SWD_NRF5340_APSEL_NET_MEMAP);
	swd_memory_write(dev, SWD_NRF5340_NVMC_NET_CONFIG,
			 SWD_NRF5340_NVMC_CONFIG_REN);

	return 0;
}

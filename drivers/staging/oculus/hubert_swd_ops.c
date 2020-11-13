#include <linux/delay.h>

#include "hubert_swd_ops.h"
#include "swd_registers_samd.h"
#include "swd.h"

// FIXME: part of T76730771 workaround
#define USER_ROW_W0_VAL 0xD8E0C7CF
#define USER_ROW_W1_VAL 0xFFFF3F5D

/* Reading is word-aligned, this will take care of it */
static inline u8 hubert_swd_read_byte(struct device *dev, u32 address)
{
	return (swd_memory_read(dev, address) & (0xff << 8*(address % 4)))
		>> (8*(address % 4));
}

static int hubert_swd_is_nvmc_ready(struct device *dev)
{
	return (hubert_swd_read_byte(dev, SWD_SAMD_NVMCTRL_INTFLAG) &
		SWD_SAMD_NVMCTRL_INTFLAG_READY) ==
		SWD_SAMD_NVMCTRL_INTFLAG_READY;
}

static int hubert_swd_wait_for_nvmc_ready(struct device *dev)
{
	const u64 SWD_READY_TIMEOUT_MS = 500;
	u64 timeout_time_ns =
	    ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (hubert_swd_is_nvmc_ready(dev))
			return 0;
		usleep_range(50, 500);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if (hubert_swd_is_nvmc_ready(dev))
		return 0;

	dev_err(dev, "Hubert SWD NVMC not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

int hubert_swd_prepare(struct device *dev)
{
	u32 value;

	/* Enable automatic page flushing mode */
	value = swd_memory_read(dev, SWD_SAMD_NVMCTRL_CTRLB);
	value &= ~SWD_SAMD_NVMCTRL_CTRLB_MANW;
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLB, value);

	return 0;
}

int hubert_swd_erase_app(struct device *dev)
{
	int status = 0;
	int i = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	int flash_pages_to_erase = flash->num_pages - flash->num_retained_pages;
	int block_size = flash->block_size; /* also known as "row size" */
	int flash_blocks_to_erase = (flash_pages_to_erase * flash->page_size)
					/ block_size;
	BUG_ON(flash_blocks_to_erase < 0);

	// Implementation assumes rows to write are not locked (ie. reset state)...

	for (i = 0; i < flash_blocks_to_erase * block_size; i += block_size) {
		swd_memory_write(dev, SWD_SAMD_NVMCTRL_ADDR, i >> 1);
		swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
				 SWD_SAMD_NVMCTRL_CTRLA_CMD_ER);
		status = hubert_swd_wait_for_nvmc_ready(dev);
		if (status != 0)
			return status;
	}

	return status;
}

size_t hubert_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	return devdata->flash_info.page_size;
}

int hubert_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			   size_t len)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int page_size = devdata->flash_info.page_size;
	bool partial_page = (len % page_size) != 0;
	int status = 0;
	int i = 0;
	u32 value = 0;
	u32 bytes_left = 0;

	if (addr % page_size != 0) {
		dev_err(dev, "Write start address of 0x%08X isn't on a page boundary.\n",
			addr);
		return -EINVAL;
	}

	// Implementation assumes rows to write are not locked (ie. reset state)...

	// Make sure the page buffer is clear before filling it
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
			 SWD_SAMD_NVMCTRL_CTRLA_CMD_PBC);
	status = hubert_swd_wait_for_nvmc_ready(dev);
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
			swd_memory_write(dev, addr + i, value);
		else
			swd_memory_write_next(dev, value);

		/*
		 * In auto-write mode (MANW=0), only writes to the last word of the
		 * write-buffer page will initiate an NVMC operation to actually write
		 * the data back to flash. We only need to wait for NVMC ready after
		 * writing this word.
		 */
		if ((i % page_size) == (page_size - sizeof(u32))) {
			status = hubert_swd_wait_for_nvmc_ready(dev);
			if (status != 0)
				return status;
		}
	}

	/*
	 * When a full page is written to the page buffer, it will automaticlly
	 * be written back to flash. If the page buffer is not fully filled, a
	 * write must be manually performedn.
	 */
	if (partial_page) {
		swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
				 SWD_SAMD_NVMCTRL_CTRLA_CMD_WP);
		status = hubert_swd_wait_for_nvmc_ready(dev);
	}

	return status;
}

int hubert_swd_read(struct device *dev, int addr, u8 *dest,
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

static int hubert_swd_configure_eeprom(struct device *dev)
{
	int status = 0;
	u32 value_low, value_high;

	// FIXME:
	// Using USER_ROW_W0_VAL/USER_ROW_W1_VAL instead of swd_memory_read() to work
	// around T76730771.
	value_low = USER_ROW_W0_VAL;  // swd_memory_read(dev, SWD_SAMD_NVM_USER_ROW_W0);
	value_high = USER_ROW_W1_VAL; // swd_memory_read(dev, SWD_SAMD_NVM_USER_ROW_W1);

	// We must erase the old row before updating it.
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_ADDR, SWD_SAMD_NVM_USER_ROW_W0 >> 1);
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA, SWD_SAMD_NVMCTRL_CTRLA_CMD_EAR);
	status = hubert_swd_wait_for_nvmc_ready(dev);
	if (status)
		return status;

	// Make sure the page buffer is clear before filling it
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
		 SWD_SAMD_NVMCTRL_CTRLA_CMD_PBC);
	status = hubert_swd_wait_for_nvmc_ready(dev);
	if (status)
		return status;

	// Write to page buffer
	value_low &= ~SWD_SAMD_NVM_EEPROM_SIZE_MASK;
	value_low |= SWD_SAMD_NVM_EEPROM_SIZE_VAL << SWD_SAMD_NVM_EEPROM_SIZE_SHIFT;
	swd_memory_write(dev, SWD_SAMD_NVM_USER_ROW_W0, value_low);
	swd_memory_write(dev, SWD_SAMD_NVM_USER_ROW_W1, value_high);

	// Write page buffer back to flash
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_ADDR, SWD_SAMD_NVM_USER_ROW_W0 >> 1);
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
		 SWD_SAMD_NVMCTRL_CTRLA_CMD_WAP);
	status = hubert_swd_wait_for_nvmc_ready(dev);

	return status;
}

static int hubert_swd_erase_eeprom(struct device *dev)
{
	int status = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	int block_size = flash->block_size; /* also known as "row size" */
	int flash_blocks_to_erase = SWD_SAMD_NVM_EEPROM_SIZE / block_size;
	int i;

	for (i = 0; i < flash_blocks_to_erase * block_size; i += block_size) {
		swd_memory_write(dev, SWD_SAMD_NVMCTRL_ADDR, (SWD_SAMD_NVM_EEPROM_BASE + i) >> 1);
		swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
				 SWD_SAMD_NVMCTRL_CTRLA_CMD_ER);
		status = hubert_swd_wait_for_nvmc_ready(dev);
		if (status)
			return status;
	}

	return status;
}

int hubert_swd_provisioning_read(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;

	if (addr < SWD_SAMD_NVM_EEPROM_BASE || addr + len >= SWD_SAMD_NVM_EEPROM_BASE + SWD_SAMD_NVM_EEPROM_SIZE) {
		dev_err(dev, "Provisioning data addr/len is not within EEPROM region\n");
		return -EINVAL;
	}

	status = hubert_swd_read(dev, addr, data, len);
	if (status)
		dev_err(dev, "Failed to read EEPROM region\n");

	return status;
}

int hubert_swd_provisioning_write(struct device *dev, int addr, u8 *data, size_t len)
{
	int status;

	if (addr < SWD_SAMD_NVM_EEPROM_BASE || addr + len >= SWD_SAMD_NVM_EEPROM_BASE + SWD_SAMD_NVM_EEPROM_SIZE) {
		dev_err(dev, "Provisioning data does not fit within EEPROM region\n");
		return -EINVAL;
	}

	status = hubert_swd_erase_eeprom(dev);
	if (status) {
		dev_err(dev, "Failed to erase EEPROM region\n");
		return status;
	}

	status = hubert_swd_write_chunk(dev, addr, data, len);
	if (status) {
		dev_err(dev, "Failed to write EEPROM region\n");
		return status;
	}

	status = hubert_swd_configure_eeprom(dev);
	if (status) {
		dev_err(dev, "Failed to configure EEPROM region\n");
		return status;
	}

	return 0;
}

// FIXME: part of T76730771 workaround
bool hubert_swd_should_force_provision(struct device *dev)
{
	u32 value;

	value = swd_memory_read(dev, SWD_SAMD_NVM_USER_ROW_W0);
	if (value != USER_ROW_W0_VAL) {
		dev_warn(dev, "Unexpected USER_ROW_W0 value of 0x%08X. Forcing reprovision\n", value);
		return true;
	}

	value = swd_memory_read(dev, SWD_SAMD_NVM_USER_ROW_W1);
	if (value != USER_ROW_W1_VAL) {
		dev_warn(dev, "Unexpected USER_ROW_W1 value of 0x%08X. Forcing reprovision\n", value);
		return true;
	}

	return false;
}

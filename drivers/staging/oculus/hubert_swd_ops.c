#include <linux/delay.h>

#include "hubert_swd_ops.h"
#include "swd_registers_samd.h"
#include "swd.h"

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
		udelay(100);
	}

	dev_err(dev, "Hubert SWD NVMC not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

int hubert_swd_erase_app(struct device *dev)
{
	int status = 0;
	int i = 0;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	struct flash_info *flash = &devdata->flash_info;
	int flash_pages_to_erase = flash->num_pages
					- SAMD_NUM_FLASH_PAGES_TO_RETAIN;
	int block_size = flash->block_size; /* also known as "row size" */
	int flash_blocks_to_erase = (flash_pages_to_erase * flash->page_size)
					/ block_size;
	BUG_ON(flash_blocks_to_erase < 0);

	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLB, 0x00);

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

int hubert_swd_write_block(struct device *dev, int addr, const u8 *data,
			   int len)
{
	int status = 0;
	int i = 0;
	u32 value = 0;
	u32 bytes_left = 0;

	/* TODO: Parameter validation
	 * assert("Flash write address out of page",
	 *        g_nordic_page_erase == (address & ~(BLOCK_SIZE-1)));
	 * assert("Nordic flash size must be multiple of four",
	 *        (size & 3) == 0);
	 */

	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLB, 0x00);

	swd_memory_write(dev, SWD_SAMD_NVMCTRL_ADDR, addr);
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
				SWD_SAMD_NVMCTRL_CTRLA_CMD_UR);
	status = hubert_swd_wait_for_nvmc_ready(dev);
	if (status != 0)
		return status;

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
		value = le32_to_cpu(value);

		swd_memory_write(dev, addr + i, value);

		status = hubert_swd_wait_for_nvmc_ready(dev);
		if (status != 0)
			return status;
	}
	return status;
}

int hubert_swd_read_block(struct device *dev, int addr, const u8 *data,
			  int len)
{
	int status = 0;
	int i = 0;
	u32 value = 0;
	u32 bytes_left = 0;

	/* TODO: Parameter validation
	 * assert("Flash write address out of page",
	 *        g_nordic_page_erase == (address & ~(BLOCK_SIZE-1)));
	 * assert("Nordic flash size must be multiple of four",
	 *        (size & 3) == 0);
	 */

	hubert_swd_wait_for_nvmc_ready(dev);

	for (i = 0; i < len; i += sizeof(u32)) {
		bytes_left = len - i;

		value = swd_memory_read(dev, addr + i);

		status = hubert_swd_wait_for_nvmc_ready(dev);
		if (status != 0)
			return status;
	}
	return status;
}

static int hubert_swd_wp(struct device *dev)
{
	swd_memory_write(dev, SWD_SAMD_NVMCTRL_CTRLA,
			 SWD_SAMD_NVMCTRL_CTRLA_CMD_WP);
	return hubert_swd_wait_for_nvmc_ready(dev);
}

static void hubert_swd_reset(struct device *dev)
{
	swd_reset(dev);
}

int hubert_swd_wp_and_reset(struct device *dev)
{
	int status = 0;

	status = hubert_swd_wp(dev);
	hubert_swd_reset(dev);
	return status;
}

#include "hubert_swd_ops.h"

#include <linux/delay.h>

#include "swd_registers_samd.h"
#include "syncboss_swd.h"

/* Reading is word-aligned, this will take care of it */
static inline u8 hubert_swd_read_byte(struct swdhandle_t *handle, u32 address)
{
	return (swd_memory_read(handle, address) & (0xff << 8*(address % 4)))
		>> (8*(address % 4));
}

static int hubert_swd_is_nvmc_ready(struct swdhandle_t *handle)
{
	return (hubert_swd_read_byte(handle, SWD_SAMD_NVMCTRL_INTFLAG) &
		SWD_SAMD_NVMCTRL_INTFLAG_READY) ==
		SWD_SAMD_NVMCTRL_INTFLAG_READY;
}

static int hubert_swd_wait_for_nvmc_ready(struct device *dev,
					    struct swdhandle_t *handle)
{
	const u64 SWD_READY_TIMEOUT_MS = 500;
	u64 timeout_time_ns =
	    ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (hubert_swd_is_nvmc_ready(handle))
			return 0;
		udelay(100);
	}

	dev_err(dev, "Hubert SWD NVMC not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

int hubert_swd_erase_app(struct device *dev,
	struct swdhandle_t *handle)
{
	int status = 0;
	int i = 0;
	int flash_pages_to_erase =
		AT91SAMD_NUM_FW_PAGES - AT91SAMD_NUM_FLASH_PAGES_TO_RETAIN;
	int flash_rows_to_erase =
		(flash_pages_to_erase * SWD_SAMD_PAGE_SIZE) / SWD_SAMD_ROW_SIZE;

	BUILD_BUG_ON(!(AT91SAMD_NUM_FLASH_PAGES_TO_RETAIN <
		       AT91SAMD_NUM_FW_PAGES));

	swd_memory_write(handle, SWD_SAMD_NVMCTRL_CTRLB, 0x00);

	for (i = 0; i < flash_rows_to_erase * SWD_SAMD_ROW_SIZE;
					i += SWD_SAMD_ROW_SIZE) {
		swd_memory_write(handle, SWD_SAMD_NVMCTRL_ADDR, i >> 1);
		swd_memory_write(handle, SWD_SAMD_NVMCTRL_CTRLA,
				SWD_SAMD_NVMCTRL_CTRLA_CMD_ER);
		status = hubert_swd_wait_for_nvmc_ready(dev, handle);
		if (status != 0)
			return status;
	}

	return status;
}

int hubert_swd_write_block(struct device *dev,
				    struct swdhandle_t *handle, int addr,
				    const u8 *data, int len)
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

	swd_memory_write(handle, SWD_SAMD_NVMCTRL_CTRLB, 0x00);

	swd_memory_write(handle, SWD_SAMD_NVMCTRL_ADDR, addr);
	swd_memory_write(handle, SWD_SAMD_NVMCTRL_CTRLA,
				SWD_SAMD_NVMCTRL_CTRLA_CMD_UR);
	status = hubert_swd_wait_for_nvmc_ready(dev, handle);
	if (status != 0)
		return status;

	swd_memory_write(handle, SWD_SAMD_NVMCTRL_CTRLA,
				SWD_SAMD_NVMCTRL_CTRLA_CMD_PBC);
	status = hubert_swd_wait_for_nvmc_ready(dev, handle);
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

		swd_memory_write(handle, addr + i, value);

		status = hubert_swd_wait_for_nvmc_ready(dev, handle);
		if (status != 0)
			return status;
	}
	return status;
}

int hubert_swd_read_block(struct device *dev,
				    struct swdhandle_t *handle, int addr,
				    const u8 *data, int len)
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

	hubert_swd_wait_for_nvmc_ready(dev, handle);

	for (i = 0; i < len; i += sizeof(u32)) {
		bytes_left = len - i;

		value = swd_memory_read(handle, addr + i);

		status = hubert_swd_wait_for_nvmc_ready(dev, handle);
		if (status != 0)
			return status;
	}
	return status;
}

static int hubert_swd_wp(struct device *dev, struct swdhandle_t *handle)
{
	swd_memory_write(handle, SWD_SAMD_NVMCTRL_CTRLA,
				SWD_SAMD_NVMCTRL_CTRLA_CMD_WP);
	return hubert_swd_wait_for_nvmc_ready(dev, handle);
}

static void hubert_swd_reset(struct device *dev, struct swdhandle_t *handle)
{
	swd_reset(handle);
}

int hubert_swd_wp_and_reset(struct device *dev, struct swdhandle_t *handle)
{
	int status = 0;

	status = hubert_swd_wp(dev, handle);
	hubert_swd_reset(dev, handle);
	return status;
}

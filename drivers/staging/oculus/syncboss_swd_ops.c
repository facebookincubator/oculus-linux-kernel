#include "syncboss_swd_ops.h"

#include <linux/delay.h>

#include "swd_registers_nrf.h"
#include "syncboss_swd.h"

static int syncboss_swd_is_nvmc_ready(struct swdhandle_t *handle)
{
	return (swd_memory_read(handle, SWD_NRF_NVMC_READY) &
		SWD_NRF_NVMC_READY_BM) == SWD_NRF_NVMC_READY_Ready;
}

static int syncboss_swd_wait_for_nvmc_ready(struct device *dev,
					    struct swdhandle_t *handle)
{
	const u64 SWD_READY_TIMEOUT_MS = 500;
	u64 timeout_time_ns = 0;

	timeout_time_ns =
	    ktime_get_ns() + (SWD_READY_TIMEOUT_MS * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if (syncboss_swd_is_nvmc_ready(handle))
			return 0;
		udelay(1000);
	}

	dev_err(dev, "SyncBoss SWD NVMC not ready after %llums",
		SWD_READY_TIMEOUT_MS);
	return -ETIMEDOUT;
}

int syncboss_swd_erase_app(struct device *dev,
	struct swdhandle_t *handle)
{
	int status = 0;
	int x;
	int flash_pages_to_erase =
		SYNCBOSS_NUM_FLASH_PAGES - SYNCBOSS_NUM_FLASH_PAGES_TO_RETAIN;
	BUILD_BUG_ON(!(SYNCBOSS_NUM_FLASH_PAGES_TO_RETAIN <
		       SYNCBOSS_NUM_FLASH_PAGES));

	swd_halt(handle);
	swd_memory_write(handle, SWD_NRF_NVMC_CONFIG,
			 SWD_NRF_NVMC_CONFIG_EEN);

	/* Note: Instead of issuing an ERASEALL command, we erase each page
	 * separately.  This is to preserve the values in the UICR where we
	 * store some data that shouldn't be touched by firmware update.
	 */

	for (x = 0; x < flash_pages_to_erase; ++x) {
		swd_memory_write(handle, SWD_NRF_NVMC_ERASEPAGE,
			x * SYNCBOSS_FLASH_PAGE_SIZE);
		status = syncboss_swd_wait_for_nvmc_ready(dev, handle);
		if (status != 0)
			return status;
	}

	swd_memory_write(handle, SWD_NRF_NVMC_CONFIG,
			 SWD_NRF_NVMC_CONFIG_REN);
	return 0;
}

int syncboss_swd_write_block(struct device *dev,
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
	swd_memory_write(handle, SWD_NRF_NVMC_CONFIG,
			 SWD_NRF_NVMC_CONFIG_WEN);
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

		status = syncboss_swd_wait_for_nvmc_ready(dev, handle);
		if (status != 0)
			goto error;
	}

 error:
	swd_memory_write(handle, SWD_NRF_NVMC_CONFIG,
			 SWD_NRF_NVMC_CONFIG_REN);
	return status;
}

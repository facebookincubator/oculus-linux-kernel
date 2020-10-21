#ifndef HUBERT_SWD_OPS_H
#define HUBERT_SWD_OPS_H

#include "syncboss_swd.h"

#include <linux/device.h>
#include <linux/module.h>

#define AT91SAMD_BLOCK_SIZE 64
#define AT91SAMD_MAX_FW_SIZE 0x20000

/* Number of flash pages for fw image */
#define AT91SAMD_NUM_FW_PAGES (AT91SAMD_MAX_FW_SIZE / AT91SAMD_BLOCK_SIZE)

/* We reserve a few of the last flash pages for persistent config */
#define AT91SAMD_NUM_FLASH_PAGES_TO_RETAIN 64

int hubert_swd_erase_app(struct device *dev,
	struct swdhandle_t *handle);

int hubert_swd_write_block(struct device *dev,
				    struct swdhandle_t *handle, int addr,
				    const u8 *data, int len);

int hubert_swd_read_block(struct device *dev,
				    struct swdhandle_t *handle, int addr,
				    const u8 *data, int len);

int hubert_swd_wp_and_reset(struct device *dev, struct swdhandle_t *handle);

#endif

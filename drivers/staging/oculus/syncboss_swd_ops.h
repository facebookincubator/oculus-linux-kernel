#ifndef SYNCBOSS_SWD_OPS_H
#define SYNCBOSS_SWD_OPS_H

#include "syncboss_swd.h"

#include <linux/device.h>
#include <linux/module.h>


#define SYNCBOSS_BLOCK_SIZE 2048
#define SYNCBOSS_FLASH_PAGE_SIZE 0x1000
#define SYNCBOSS_NUM_FLASH_PAGES 128

/* We reserve a few of the last flash pages for persistent config */
#define SYNCBOSS_NUM_FLASH_PAGES_TO_RETAIN 3


int syncboss_swd_erase_app(struct device *dev,
	struct swdhandle_t *handle);

int syncboss_swd_write_block(struct device *dev,
				    struct swdhandle_t *handle, int addr,
				    const u8 *data, int len);

#endif

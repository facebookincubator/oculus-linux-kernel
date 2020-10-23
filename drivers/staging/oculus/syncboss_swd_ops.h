#ifndef SYNCBOSS_SWD_OPS_H
#define SYNCBOSS_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

/* We reserve a few of the last flash pages for persistent config */
#define SYNCBOSS_NUM_FLASH_PAGES_TO_RETAIN 3

int syncboss_swd_erase_app(struct device *dev);
int syncboss_swd_write_block(struct device *dev, int addr, const u8 *data,
			     int len);

#endif

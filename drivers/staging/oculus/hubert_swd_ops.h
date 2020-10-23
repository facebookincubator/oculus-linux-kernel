#ifndef HUBERT_SWD_OPS_H
#define HUBERT_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

/* We reserve a few of the last flash pages for persistent config */
#define SAMD_NUM_FLASH_PAGES_TO_RETAIN 64

int hubert_swd_erase_app(struct device *dev);
int hubert_swd_write_block(struct device *dev, int addr, const u8 *data,
			   int len);
int hubert_swd_read_block(struct device *dev, int addr, const u8 *data,
			  int len);
int hubert_swd_wp_and_reset(struct device *dev);

#endif

#ifndef HUBERT_SWD_OPS_H
#define HUBERT_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int hubert_swd_erase_app(struct device *dev);
int hubert_swd_write_block(struct device *dev, int addr, const u8 *data,
			   int len);
int hubert_swd_read_block(struct device *dev, int addr, const u8 *data,
			  int len);
int hubert_swd_wp_and_reset(struct device *dev);

#endif

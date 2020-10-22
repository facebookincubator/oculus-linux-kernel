#ifndef SYNCBOSS_SWD_OPS_H
#define SYNCBOSS_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int syncboss_swd_erase_app(struct device *dev);
int syncboss_swd_write_block(struct device *dev, int addr, const u8 *data,
			     int len);

#endif

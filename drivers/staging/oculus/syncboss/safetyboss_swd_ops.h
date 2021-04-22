#ifndef SAFETYBOSS_SWD_OPS_H
#define SAFETYBOSS_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int safetyboss_swd_prepare(struct device *dev);
int safetyboss_swd_erase_app(struct device *dev);
int safetyboss_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			   size_t len);
size_t safetyboss_get_write_chunk_size(struct device *dev);

#endif // SAFETYBOSS_SWD_OPS_H

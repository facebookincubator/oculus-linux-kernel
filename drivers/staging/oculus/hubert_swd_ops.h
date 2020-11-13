#ifndef HUBERT_SWD_OPS_H
#define HUBERT_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int hubert_swd_prepare(struct device *dev);
int hubert_swd_provisioning_read(struct device *dev, int addr, u8 *data, size_t len);
int hubert_swd_provisioning_write(struct device *dev, int addr, u8 *data, size_t len);
bool hubert_swd_should_force_provision(struct device *dev);

int hubert_swd_erase_app(struct device *dev);
int hubert_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			   size_t len);
size_t hubert_get_write_chunk_size(struct device *dev);
int hubert_swd_read(struct device *dev, int addr, u8 *dest,
		    size_t len);

#endif

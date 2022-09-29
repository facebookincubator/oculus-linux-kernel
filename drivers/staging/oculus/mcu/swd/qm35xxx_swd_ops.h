/* SPDX-License-Identifier: GPL-2.0 */
#ifndef UWB_SWD_OPS_H
#define UWB_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int qm35xxx_swd_prepare(struct device *dev);
int qm35xxx_swd_finalize(struct device *dev);
int qm35xxx_swd_erase_app(struct device *dev);
int qm35xxx_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			     size_t len);
size_t qm35xxx_get_write_chunk_size(struct device *dev);

#endif

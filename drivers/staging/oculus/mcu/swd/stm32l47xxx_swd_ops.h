/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STM32L47XXX_SWD_OPS_H
#define STM32L47XXX_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int stm32l47xxx_swd_prepare(struct device *dev);
int stm32l47xxx_swd_erase_app(struct device *dev);
int stm32l47xxx_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			   size_t len);
size_t stm32l47xxx_get_write_chunk_size(struct device *dev);
int stm32l47xxx_swd_provisioning_read(struct device *dev, int addr, u8 *data, size_t len);
int stm32l47xxx_swd_provisioning_write(struct device *dev, int addr, u8 *data, size_t len);

#endif // STM32L47XXX_SWD_OPS_H

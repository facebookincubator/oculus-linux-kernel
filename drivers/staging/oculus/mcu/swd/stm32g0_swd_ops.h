/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STM32G071_SWD_OPS_H
#define STM32G071_SWD_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int stm32g0_swd_prepare(struct device *dev);
int stm32g0_swd_erase_app(struct device *dev);
int stm32g0_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			   size_t len);
size_t stm32g0_get_write_chunk_size(struct device *dev);

#endif // STM32G0_SWD_OPS_H

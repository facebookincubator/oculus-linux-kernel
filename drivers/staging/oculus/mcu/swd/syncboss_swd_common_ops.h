/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SYNCBOSS_SWD_COMMON_OPS_H
#define SYNCBOSS_SWD_COMMON_OPS_H

#include <linux/device.h>
#include <linux/module.h>

int syncboss_swd_wait_reg_value(struct device *dev, u32 reg, u32 value,
				       u64 timeout);

#endif // SYNCBOSS_SWD_COMMON_OPS_H

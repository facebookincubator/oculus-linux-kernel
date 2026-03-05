/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SYNCBOSS_SWD_COMMON_OPS_H
#define SYNCBOSS_SWD_COMMON_OPS_H

#include <linux/device.h>
#include <linux/module.h>

int syncboss_swd_wait_reg_value_mask(struct device *dev, u32 reg, u32 value,
				     u32 mask, u64 timeout);

#define syncboss_swd_wait_reg_value(d, r, v, t) \
	syncboss_swd_wait_reg_value_mask((d), (r), (v), 0xFFFFFFFF, (t))

#endif // SYNCBOSS_SWD_COMMON_OPS_H

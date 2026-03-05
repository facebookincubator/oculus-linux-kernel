// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>

#include "swd.h"
#include "syncboss_swd_common_ops.h"

int syncboss_swd_wait_reg_value_mask(struct device *dev, u32 reg, u32 value,
				     u32 mask, u64 timeout)
{
	u64 timeout_time_ns = 0;

	value &= mask;

	timeout_time_ns = ktime_get_ns() + (timeout * NSEC_PER_MSEC);

	while (ktime_get_ns() < timeout_time_ns) {
		if ((swd_memory_read(dev, reg) & mask) == value)
			return 0;

		usleep_range(1000, 2000);
	}

	/*
	 * Try once more, just in case we were preempted at an unlucky time
	 * after calculating timeout_time_ns
	 */
	if ((swd_memory_read(dev, reg) & mask) == value)
		return 0;

	dev_err(dev, "SyncBoss SWD register %08x not %08x after %llums", reg,
		value, timeout);

	return -ETIMEDOUT;
}

// SPDX-License-Identifier: GPL-2.0
#ifndef SWD_CHARDEV_H
#define SWD_CHARDEV_H

#include <linux/device.h>
#include <linux/ioctl.h>

#define SWD_MAGIC_NUM         100
#define SWD_IOCTL_CHIP_ERASE  _IO(SWD_MAGIC_NUM, 0)
#define SWD_IOCTL_RESET       _IOW(SWD_MAGIC_NUM, 1, u32)

int swd_driver_init_chardev(struct device *dev, const char *const flavor);
void swd_driver_deinit_chardev(struct device *dev);
#endif

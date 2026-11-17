// SPDX-License-Identifier: GPL-2.0
#ifndef SWD_CHARDEV_H
#define SWD_CHARDEV_H

#include <linux/device.h>
#include <linux/ioctl.h>

#include <uapi/linux/swd_chardev.h>

int swd_driver_init_chardev(struct device *dev, const char *const flavor);
void swd_driver_deinit_chardev(struct device *dev);
#endif

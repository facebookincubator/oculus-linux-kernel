/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FWUPDATE_OPERATIONS_H
#define _FWUPDATE_OPERATIONS_H

#include <linux/platform_device.h>
#include "swd.h"

int fwupdate_preupdate_operations(struct device *dev);
void fwupdate_release_all_firmware(struct device *dev);
int fwupdate_update_app(struct device *dev);
int fwupdate_update_prepare(struct device *dev);
int fwupdate_update_chip_erase(struct device *dev);
int fwupdate_get_firmware_images(struct device *dev, struct swd_dev_data *devdata);

#endif

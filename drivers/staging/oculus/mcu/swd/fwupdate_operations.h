/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FWUPDATE_OPERATIONS_H
#define _FWUPDATE_OPERATIONS_H

#include <linux/platform_device.h>
#include "swd.h"

#define DEFAULT_MCU_RESET_MS 5

int fwupdate_check_swd_ops(struct device *dev);
int fwupdate_get_firmware_images(struct device *dev, struct swd_dev_data *devdata);
int fwupdate_preupdate_operations(struct device *dev);
void fwupdate_release_all_firmware(struct device *dev);
int fwupdate_update_app(struct device *dev);
int fwupdate_update_chip_erase(struct device *dev);
int fwupdate_update_prepare(struct device *dev);
int fwupdate_update_single_app(struct device *dev, struct swd_mcu_data *mcudata, bool erase_all, bool force_bootloader_update);

#endif

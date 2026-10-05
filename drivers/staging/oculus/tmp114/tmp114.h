// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef __TMP114_H__
#define __TMP114_H__

#define DRIVER_NAME        "tmp114"
#define DEVICE_NAME        DRIVER_NAME
#define CLASS_NAME         DRIVER_NAME
#define MAJOR_NUM          0

#define REG_TEMP           0x00


struct tmp114_dev {
    struct regmap                  *regmap;
    struct i2c_client              *client;
    struct cdev                    cdev;
    struct mutex                   mutex;
    struct thermal_zone_device     *tz;
    atomic_t                       is_enabled;
    struct regulator               *pmic;
    struct notifier_block          pmic_nb;
};

#endif

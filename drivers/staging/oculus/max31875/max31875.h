// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef __MAX31875_H__
#define __MAX31875_H__

#define DRIVER_NAME	"max31875"
#define DEVICE_NAME	DRIVER_NAME
#define CLASS_NAME	DRIVER_NAME
#define MAJOR_NUM	0

#define REG_TEMP	0x00
#define REG_CONFIG	0x01

#define CONVERSION_RATE_MASK  GENMASK(2, 1)

struct max31875_dev {
	struct regmap *regmap;
	struct i2c_client *client;
	struct cdev cdev;
	struct mutex mutex;
	struct thermal_zone_device *tz;
	char *temp_mcelsius;
	atomic_t is_enabled;
	struct regulator *pmic;
	struct notifier_block pmic_nb;
};

#endif

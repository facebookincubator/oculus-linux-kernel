// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef __RAA491901_H__
#define __RAA491901_H__

#include <linux/atomic.h>

#define DRIVER_NAME         "raa491901"
#define DEVICE_NAME         DRIVER_NAME
#define CLASS_NAME          "display-led-driver"
#define MAJOR_NUM           0

#define REG_FUNC_ENABLE         0x02
#define REG_RED_GAIN_LSB        0x13
#define REG_GRN_GAIN_LSB        0x14
#define REG_BLU_GAIN_LSB        0x15
#define REG_LED_GAIN_MSB        0x17
#define REG_LED_BLANK_VOL_LSB   0x18
#define REG_RED_STATIC_VOL_LSB  0x19
#define REG_GRN_STATIC_VOL_LSB  0x1A
#define REG_BLU_STATIC_VOL_LSB  0x1B
#define REG_LED_STATIC_VOL_MSB  0x1D
#define REG_LED_BLANK_VOL_MSB   0x1E
#define REG_RED_AUTO_VOL_LSB    0x25
#define REG_GRN_AUTO_VOL_LSB    0x26
#define REG_BLU_AUTO_VOL_LSB    0x27
#define REG_LED_AUTO_VOL_MSB    0x29

#define LED_MAX_GAIN            1023

enum raa491901_led_driver_mode {
    RAA491901_MODE_PFM,
    RAA491901_MODE_PWM,
};

enum raa491901_bit_res_setting {
    RAA491901_BIT_RES_AUTO,
    RAA491901_BIT_RES_LOW,
    RAA491901_BIT_RES_HIGH,
};

struct raa491901_dev {
    struct regmap                        *regmap;
    struct i2c_client                    *client;
    struct cdev                          cdev;
    struct mutex                         mutex;
    struct gpio_desc                     *gpio_bit_res;
    struct gpio_desc                     *gpio_led_en;
    struct gpio_desc                     *gpio_dd_en;
    int                                  dondoff_irq;
    atomic_t                             is_don;
    bool                                 userspace_takeover;
    bool                                 is_high_res;
    enum raa491901_led_driver_mode       mode;
    unsigned int                         offset_high_res_uA;
    unsigned int                         offset_low_res_uA;
    unsigned int                         slope_high_res;
    int                                  slope_low_res;
    enum raa491901_bit_res_setting       bit_res_setting;
    struct kernfs_node                   *dondoff_kn;
};

enum {
    RAA491901_LED_GAINS = 0,
    RAA491901_LED_CURRENTS,
    RAA491901_LED_MODE,
    RAA491901_VBLANK_COEFFS,
    RAA491901_LED_VOLTAGES,
    RAA491901_INTERPOLATION_COEFFS,
    RAA491901_RESOLUTION,
    RAA491901_BIT_RES_SETTING,
    RAA491901_LED_SWITCH,
};

static ssize_t raa491901_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t raa491901_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define RAA491901_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = raa491901_show_attrs,			\
	.store = raa491901_store_attrs,			\
}

#endif

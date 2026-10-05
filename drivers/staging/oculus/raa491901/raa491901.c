// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.
// raa491901 is a four channel sequential LED driver

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/gpio/consumer.h>

#include "raa491901.h"
#include "raa491901_ioctl.h"

#define REGMAP_MAX_RETRIES     5
#define REGMAP_DELAY_MS        150
#define DONDOFF_IRQF (IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_ONESHOT)
#define MAX_INT_DIGITS         21

/*
    LED msb value at register 0x17 has the format:
    Bit 7:6 -> LED1 Gain MSB (RED LED)
    Bit 5:4 -> LED2 Gain MSB (BLU LED)
    Bit 3:2 -> LED3 Gain MSB (GRN LED)
    Bit 1:0 -> LED4 Gain MSB (Unused)
*/
#define GET_RED_MSB(led_msb) ((led_msb >> 6) & 0b11)
#define GET_GRN_MSB(led_msb) ((led_msb >> 4) & 0b11)
#define GET_BLU_MSB(led_msb) ((led_msb >> 2) & 0b11)
#define GET_CMB_MSB(red_msb, grn_msb, blu_msb) ((red_msb << 6) | (grn_msb << 4) | (blu_msb << 2))

/*
    Macros to extract MSB and LSB from DAC value and vice versa
    DAC value is a 10-bit integer that has a 2-bit MSB and 8-bit LSB
*/
#define GET_LED_DAC(led_msb, led_lsb) ((led_msb << 8) | led_lsb)
#define GET_LED_MSB(led_dac) ((led_dac >> 8) & 0b11)
#define GET_LED_LSB(led_dac) (led_dac & 0xFF)

#define GET_MAX_VAL(val1, val2) ((val1 > val2) ? val1 : val2)
#define GET_MIN_VAL(val1, val2) ((val1 < val2) ? val1 : val2)

// Integer division rounds towards 0. This macro rounds to the closest integer by adding 0.5 to the result. This only works for positive integers.
#define ROUND_CLOSEST_DIV_POS_INT(num, den) (((num) + ((den) / 2)) / (den))

// To handle the case when 'current_uA < offset_uA', make sure we won't generate very big gain(1023) on the low driving current
#define REMOVE_OFFSET(current_uA, offset_uA) (((current_uA) <= (offset_uA)) ? 0 : (((current_uA) - (offset_uA)) * 1000))
/*
    Macros to convert between current in mA and gain value

    current(mA) = slope * gain + offset

    Gain value is always rounded so it falls in the valid range [0, 1023]
*/
#define GET_VALID_GAIN(gain) (GET_MIN_VAL(GET_MAX_VAL(gain, 0), 1023))
#define CURRENT_TO_GAIN(current_uA, slope, offset_uA) (GET_VALID_GAIN(ROUND_CLOSEST_DIV_POS_INT(REMOVE_OFFSET(current_uA, offset_uA), slope)))
#define GAIN_TO_CURRENT(gain, slope, offset_uA) (ROUND_CLOSEST_DIV_POS_INT(GET_VALID_GAIN(gain) * slope, 1000) + offset_uA)
#define HIGH_RES_THRESH_UA        4000

static struct class *raa491901_class = NULL;

static const char LED_IO_STRING[] = "RED:%d GREEN:%d BLUE:%d\n";
/* min possible string length with three integers (one digit) - remove six chars of %d,
                                                                null termination,
                                                                new line, and add three digits */
static const size_t LED_IO_STRING_MIN_SIZE = sizeof(LED_IO_STRING) - 5;
/* max possible string length with three integers - remove six chars of %d and three max int digits*/
static const size_t LED_IO_STRING_MAX_SIZE = sizeof(LED_IO_STRING) - 7 + 3 * MAX_INT_DIGITS;

static const char LED_PFM_MODE_IO_STRING[] = "pfm";
static const char LED_PWM_MODE_IO_STRING[] = "pwm";
static const u8 PWM_MODE_STATUS_VAL = 0xC4; // Set bits 7, 6 for slow slew rate & bit 2 for enable auto headroom
static const u8 PFM_MODE_STATUS_VAL = 0xE0; // unset bit 2 to disable auto headroom, set bit 5 for PFM


// Default linear coefficients (Y = MX + B) for calculating VBlank (DAC) given current in mA
static int vblank_red_m = 342;
static int vblank_red_b = 214000;
static int vblank_grn_m = 390;
static int vblank_grn_b = 266000;
static int vblank_blu_m = 290;
static int vblank_blu_b = 294000;
static const int vblank_low_current_threshold = 25000;
static int vblank_low_current_value = 260000;

static const char VBLANK_IO_STRING[] = "RED_M:%d RED_B:%d GREEN_M:%d GREEN_B:%d BLUE_M:%d BLUE_B:%d LOW_CURRENT_VBLANK:%d\n";
/*
    From data sheet, the formula for converting voltage to DAC is:
    DAC = (voltage - 1) * 1023 / 4.5

    Since Vblank is scaled to 10^5 for avoiding floats, the formula becomes:
    DAC = (voltage - 100000) * 2046 / 900000
*/
#define VBLANK_TO_DAC(vblank) ((vblank - 100000) * 2046 / 900000)

static const char LED_VOLTS_IO_STRING[] = "RED:%d GREEN:%d BLUE:%d BLANK:%d\n";
#define DAC_TO_MILLIVOLT(dac) (dac * 9000 / 2046 + 1000)

static const char INTERPOLATION_IO_STRING[] = "SLOPE_HIGH_RES:%d OFFSET_HIGH_RES:%d SLOPE_LOW_RES:%d OFFSET_LOW_RES:%d\n";
// Default offset values from spec
static const int default_offset_high_res_uA = 260;
static const int default_offset_low_res_uA = 3000;
static const int default_slope_high_res = 30667;
static const int default_slope_low_res = 312805;
static int max_high_res_thresh_uA = GAIN_TO_CURRENT(1023, default_slope_high_res, default_offset_high_res_uA);

static const char LED_RESOLUTION_AUTO_SETTING_STRING[] = "auto";
static const char LED_RESOLUTION_LOW_SETTING_STRING[] = "low";
static const char LED_RESOLUTION_HIGH_SETTING_STRING[] = "high";

static const struct regmap_config raa491901_regmap_config = {
    .reg_bits   = 8,
    .val_bits   = 8,
    .val_format_endian = REGMAP_ENDIAN_BIG,
    .cache_type = REGCACHE_NONE,
};

static struct device_attribute raa491901_attrs[] = {
    RAA491901_ATTR(led_gains),
    RAA491901_ATTR(led_currents),
    RAA491901_ATTR(led_mode),
    RAA491901_ATTR(vblank_coeffs),
    RAA491901_ATTR(led_voltages),
    RAA491901_ATTR(interpolation_coeffs),
    RAA491901_ATTR(resolution),
    RAA491901_ATTR(bit_res_setting),
    RAA491901_ATTR(led_switch),
};

static ssize_t dondoff_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
    struct raa491901_dev *raa491901 = i2c_get_clientdata(dev_get_drvdata(dev));

    if (!raa491901->userspace_takeover) {
        raa491901->userspace_takeover = true;
    }

    return snprintf(buf, 10, "%d\n", atomic_read(&raa491901->is_don));
}

static DEVICE_ATTR_RO(dondoff);

static int raa491901_i2c_read(struct regmap *regmap, u8 reg_addr, u8 *data, size_t data_len)
{
    int ret = 0;
    int retry = 0;

    ret = regmap_bulk_read(regmap, (unsigned int)reg_addr, data, data_len);
    while (ret < 0 && retry < REGMAP_MAX_RETRIES) {
        pr_warn("<%s>: regmap_read failed with error %d\n", __func__, ret);
        msleep(REGMAP_DELAY_MS);
        ret = regmap_bulk_read(regmap, (unsigned int)reg_addr, data, data_len);
        retry++;
    }

    if (ret < 0) {
        pr_err("<%s>: regmap_read failed with error %d after retries\n", __func__, ret);
        return ret;
    }

    return ret;
}

static int raa491901_i2c_write(struct regmap *regmap, u8 reg_addr, u8 *data, size_t data_len)
{
    int ret = 0;
    int retry = 0;

    ret = regmap_bulk_write(regmap, (unsigned int)reg_addr, data, data_len);
    while (ret < 0 && retry < REGMAP_MAX_RETRIES) {
        pr_warn("<%s>: regmap_write failed with error %d\n", __func__, ret);
        msleep(REGMAP_DELAY_MS);
        ret = regmap_bulk_write(regmap, (unsigned int)reg_addr, data, data_len);
        retry++;
    }

    if (ret < 0)
        pr_err("<%s>: regmap_write failed with error %d after retries\n", __func__, ret);

    return ret;
}

static int raa491901_read_led_gains(struct raa491901_dev *raa491901, struct raa491901_gain_cmd *led_gains)
{
    int ret = 0;
    u8 gains[5] = {0}; // REGS 0x13 to 0x17

    if (mutex_lock_killable(&raa491901->mutex))
        return -EINTR;

    ret = raa491901_i2c_read(raa491901->regmap, REG_RED_GAIN_LSB, gains, sizeof(gains));
    if (ret) {
        pr_err("<%s>: Failed to read gain registers: %d", __func__, ret);
        goto exit_read;
    }

    led_gains->red_gain = GET_LED_DAC(GET_RED_MSB(gains[4]), gains[0]);
    led_gains->green_gain = GET_LED_DAC(GET_GRN_MSB(gains[4]), gains[1]);
    led_gains->blue_gain = GET_LED_DAC(GET_BLU_MSB(gains[4]), gains[2]);

exit_read:
    mutex_unlock(&raa491901->mutex);
    return ret;
}

static int raa491901_write_led_gains(struct raa491901_dev *raa491901, struct raa491901_gain_cmd *led_gains)
{
    int ret = 0;
    u8 gains[5] = {0}; // REGS 0x13 to 0x17

    if (led_gains->red_gain > LED_MAX_GAIN
        || led_gains->blue_gain > LED_MAX_GAIN
        || led_gains->green_gain > LED_MAX_GAIN)
        return -EINVAL;

    if (mutex_lock_killable(&raa491901->mutex))
        return -EINTR;

    gains[0] = GET_LED_LSB(led_gains->red_gain);
    gains[1] = GET_LED_LSB(led_gains->green_gain);
    gains[2] = GET_LED_LSB(led_gains->blue_gain);

    gains[4] = GET_CMB_MSB(GET_LED_MSB(led_gains->red_gain),
                                GET_LED_MSB(led_gains->green_gain),
                                GET_LED_MSB(led_gains->blue_gain));

    ret = raa491901_i2c_write(raa491901->regmap, REG_RED_GAIN_LSB, gains, sizeof(gains));
    if (ret) {
        pr_err("<%s>: Failed to write to gain registers: %d", __func__, ret);
        goto exit_write;
    }

exit_write:
    mutex_unlock(&raa491901->mutex);
    return ret;
}

static int raa491901_set_default_vblank(struct raa491901_dev *raa491901,
        int red_current_uA, int green_current_uA, int blue_current_uA)
{
    int ret = 0;

    int red_vblank = (red_current_uA < vblank_low_current_threshold) ? vblank_low_current_value
                                        : vblank_red_m * red_current_uA / 1000 + vblank_red_b;
    int green_vblank = (green_current_uA < vblank_low_current_threshold) ? vblank_low_current_value
                                        : vblank_grn_m * green_current_uA / 1000 + vblank_grn_b;
    int blue_vblank = (blue_current_uA < vblank_low_current_threshold) ? vblank_low_current_value
                                        : vblank_blu_m * blue_current_uA / 1000 + vblank_blu_b;

    int max_vblank = GET_MAX_VAL(GET_MAX_VAL(red_vblank, green_vblank), blue_vblank);

    int vblank_dac = VBLANK_TO_DAC(max_vblank);
    u8 vblank_lsb = GET_LED_LSB(vblank_dac);
    u8 vblank_msb;

    if (mutex_lock_killable(&raa491901->mutex))
        return -EINTR;

    ret = raa491901_i2c_read(raa491901->regmap, REG_LED_BLANK_VOL_MSB, &vblank_msb, 1);
    if (ret) {
        pr_err("<%s>: Failed to read vblank msb register: %d", __func__, ret);
        goto exit_set_default_vblank;
    }

    ret = raa491901_i2c_write(raa491901->regmap, REG_LED_BLANK_VOL_LSB, &vblank_lsb, 1);
    if (ret) {
        pr_err("<%s>: Failed to write to vblank lsb register: %d", __func__, ret);
        goto exit_set_default_vblank;
    }

    vblank_msb = (GET_LED_MSB(vblank_dac) << 6) | (vblank_msb & 0x3F); // set bits 7 and 6
    ret = raa491901_i2c_write(raa491901->regmap, REG_LED_BLANK_VOL_MSB, &vblank_msb, 1);
    if (ret)
        pr_err("<%s>: Failed to write to vblank msb register: %d", __func__, ret);

exit_set_default_vblank:
    mutex_unlock(&raa491901->mutex);
    return ret;
}

/*
If atleast one LED current is below the high resolution threshold and
none of the currents are greater than max current supported by high resolution,
we use high resolution. Otherwise we use the low resolution.
*/
static inline bool raa491901_get_res(unsigned int red_uA,
                    unsigned int green_uA, unsigned int blue_uA)
{
    return ((red_uA < HIGH_RES_THRESH_UA || blue_uA < HIGH_RES_THRESH_UA || green_uA < HIGH_RES_THRESH_UA) &&
           (red_uA < max_high_res_thresh_uA && blue_uA < max_high_res_thresh_uA && green_uA < max_high_res_thresh_uA));
}

static void raa491901_toggle_bit_res(struct raa491901_dev *raa491901, bool is_high_res)
{
    if (raa491901->is_high_res == is_high_res)
        return;

    gpiod_set_value(raa491901->gpio_bit_res, !is_high_res);
    raa491901->is_high_res = is_high_res;
}

static int raa491901_prepare_pfm_mode_switch(struct raa491901_dev *raa491901)
{
    int ret = 0;
    int red_volt, green_volt, blue_volt, max_volt;
    u8 max_volt_msb;
    u8 led_voltages[5] = {0}; // REGS 0x25 to 0x29
    u8 led_static_voltages[7] = {0}; // REGS 0x18 to 0x1E

    // read the auto voltage registers
    ret = raa491901_i2c_read(raa491901->regmap, REG_RED_AUTO_VOL_LSB, led_voltages, sizeof(led_voltages));
    if (ret) {
        pr_err("<%s>: Failed to read auto voltage registers: %d", __func__, ret);
        return ret;
    }

    // calculate the vblank - pick the largest of the RGB voltages
    ret = raa491901_i2c_read(raa491901->regmap, REG_LED_BLANK_VOL_MSB, &max_volt_msb, 1);
    if (ret) {
        pr_err("<%s>: Failed to read vblank msb register: %d", __func__, ret);
        return ret;
    }

    red_volt = GET_LED_DAC(GET_RED_MSB(led_voltages[4]), led_voltages[0]);
    green_volt = GET_LED_DAC(GET_GRN_MSB(led_voltages[4]), led_voltages[1]);
    blue_volt = GET_LED_DAC(GET_BLU_MSB(led_voltages[4]), led_voltages[2]);
    max_volt = GET_MAX_VAL(GET_MAX_VAL(red_volt, blue_volt), green_volt);

    // set the vblank and copy the auto voltages to static voltage registers
    led_static_voltages[0] = GET_LED_LSB(max_volt);
    led_static_voltages[1] = led_voltages[0];
    led_static_voltages[2] = led_voltages[1];
    led_static_voltages[3] = led_voltages[2];
    // Do not set led_static_voltages[4] as it is for LED4 voltages which is not used
    led_static_voltages[5] = led_voltages[4];
    led_static_voltages[6] = (GET_LED_MSB(max_volt) << 6) | (max_volt_msb & 0x3F); // set bits 7 & 6 only for vblank MSB

    // write to the static voltage registers
    ret = raa491901_i2c_write(raa491901->regmap, REG_LED_BLANK_VOL_LSB, led_static_voltages, sizeof(led_static_voltages));
    if (ret) {
        pr_err("<%s>: Failed to write static voltage registers: %d", __func__, ret);
        return ret;
    }

    return ret;
}


static int raa491901_set_led_mode(struct raa491901_dev *raa491901, enum raa491901_led_driver_mode mode)
{
    int ret = 0;
    u8 mode_val;

    if (mutex_lock_killable(&raa491901->mutex))
        return -EINTR;

    switch (mode) {
        case RAA491901_MODE_PFM:
            ret = raa491901_prepare_pfm_mode_switch(raa491901);
            if (ret)
                goto error_set_led_mode;
            mode_val = PFM_MODE_STATUS_VAL;
            break;
        case RAA491901_MODE_PWM:
            mode_val = PWM_MODE_STATUS_VAL;
            break;
        default:
            ret = -EINVAL;
            goto error_set_led_mode;
    }

    ret = raa491901_i2c_write(raa491901->regmap, REG_FUNC_ENABLE, &mode_val, 1);

    if (!ret)
        raa491901->mode = mode;

error_set_led_mode:
    mutex_unlock(&raa491901->mutex);
    return ret;
}

void raa491901_toggle_led(struct raa491901_dev *raa491901, bool on)
{
    gpiod_set_value(raa491901->gpio_led_en, on?1:0);
    if (on)
    {
        usleep_range(2000, 2000+50);; // wait for the LED to turn on
    }
}

static int raa491901_open(struct inode *inode, struct file *filep)
{
    struct raa491901_dev *raa491901 = NULL;

    raa491901 = container_of(inode->i_cdev, struct raa491901_dev, cdev);
    if (raa491901 == NULL) {
        pr_err("<%s>: Failed to retrieve raa491901_dev from inode\n", __func__);
        return -ENODEV;
    }

    filep->private_data = raa491901;
    return 0;
}

static int raa491901_release(struct inode *inode, struct file *filep)
{
    return 0;
}

static long raa491901_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct raa491901_gain_cmd led_gains;
    struct raa491901_dev *raa491901;

    if (_IOC_TYPE(cmd) != IOCTL_RAA491901_MAGIC) {
        pr_info("<%s>: Invalid IOCTL magic number\n", __func__);
        return -ENOTTY;
    }

    if (_IOC_NR(cmd) > IOCTL_RAA491901_MAXCMDS) {
        pr_info("<%s>: Invalid IOCTL cmd value\n", __func__);
        return -ENOTTY;
    }

    raa491901 = filep->private_data;

    switch(cmd) {
    case IOCTL_RAA491901_IOCQGAIN:
        ret = raa491901_read_led_gains(raa491901, &led_gains);
        if (ret < 0) {
            pr_err("<%s>: raa491901_read_led_gains returned error %d\n", __func__, ret);
            return ret;
        }

        ret = copy_to_user((struct raa491901_gain_cmd *) arg, &led_gains, sizeof(struct raa491901_gain_cmd));
        if (ret) {
            pr_err("<%s>: Failed to copy %d bytes to user space\n", __func__, ret);
            return -EACCES;
        }

        break;
    case IOCTL_RAA491901_IOCSGAIN:
        ret = copy_from_user(&led_gains, (struct raa491901_gain_cmd *) arg, sizeof(struct raa491901_gain_cmd));
        if (ret) {
            pr_err("<%s>: Failed to copy %d bytes from user space\n", __func__, ret);
            return -EACCES;
        }

        ret = raa491901_write_led_gains(raa491901, &led_gains);
        if (ret < 0) {
            pr_err("<%s>: raa491901_write_led_gains returned error %d\n", __func__, ret);
            return ret;
        }
        break;
    default:
        return -ENOTTY;
    }

    return ret;
}

static const struct file_operations raa491901_fops = {
    .owner = THIS_MODULE,
    .open = raa491901_open,
    .release = raa491901_release,
    .unlocked_ioctl = raa491901_ioctl
};

static int raa491901_show_led_gains(struct raa491901_dev *raa491901, char* buf)
{
    int ret = 0;
    struct raa491901_gain_cmd led_gains;

    ret = raa491901_read_led_gains(raa491901, &led_gains);
    if (ret < 0) {
        pr_err("<%s>: raa491901_read_led_gains returned error %d\n", __func__, ret);
        return ret;
    }

    return snprintf(buf, LED_IO_STRING_MAX_SIZE, LED_IO_STRING, led_gains.red_gain, led_gains.green_gain, led_gains.blue_gain);
}

static int raa491901_show_led_currents(struct raa491901_dev* raa491901, char* buf)
{
    int ret = 0;
    unsigned int red_uA, green_uA, blue_uA, slope;
    int offset_uA;
    struct raa491901_gain_cmd led_gains;

    ret = raa491901_read_led_gains(raa491901, &led_gains);
    if (ret < 0) {
        pr_err("<%s>: raa491901_read_led_gains returned error %d\n", __func__, ret);
        return ret;
    }

    if (raa491901->is_high_res) {
        offset_uA = raa491901->offset_high_res_uA;
        slope = raa491901->slope_high_res;
    } else {
        offset_uA = raa491901->offset_low_res_uA;
        slope = raa491901->slope_low_res;
    }

    red_uA = GAIN_TO_CURRENT((unsigned int)led_gains.red_gain, slope, offset_uA);
    green_uA = GAIN_TO_CURRENT((unsigned int)led_gains.green_gain, slope, offset_uA);
    blue_uA = GAIN_TO_CURRENT((unsigned int)led_gains.blue_gain, slope, offset_uA);

    return snprintf(buf, LED_IO_STRING_MAX_SIZE, LED_IO_STRING, red_uA, green_uA, blue_uA);
}

static int raa491901_show_led_voltages(struct raa491901_dev* raa491901, char* buf)
{
    int ret = 0;
    u8 led_voltages_dac[7] = {0}; // REGS 0x18 to 0x1E
    int red_volt, green_volt, blue_volt, blank_volt;

    ret = raa491901_i2c_read(raa491901->regmap, REG_LED_BLANK_VOL_LSB,
                led_voltages_dac, sizeof(led_voltages_dac));
    if (ret) {
        pr_err("<%s>: Failed to read static voltage registers: %d", __func__, ret);
        return ret;
    }

    red_volt = GET_LED_DAC(GET_RED_MSB(led_voltages_dac[5]), led_voltages_dac[1]);
    green_volt = GET_LED_DAC(GET_GRN_MSB(led_voltages_dac[5]), led_voltages_dac[2]);
    blue_volt = GET_LED_DAC(GET_BLU_MSB(led_voltages_dac[5]), led_voltages_dac[3]);
    blank_volt = GET_LED_DAC((led_voltages_dac[6] >> 6), led_voltages_dac[0]);

    return sprintf(buf, LED_VOLTS_IO_STRING, DAC_TO_MILLIVOLT(red_volt), DAC_TO_MILLIVOLT(green_volt),
                DAC_TO_MILLIVOLT(blue_volt), DAC_TO_MILLIVOLT(blank_volt));
}

static int raa491901_show_led_switch(struct raa491901_dev* raa491901, char* buf)
{
    int val = gpiod_get_value(raa491901->gpio_led_en);
    return sprintf(buf, "%d\n", val);
}

static ssize_t raa491901_show_attrs(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int ret = 0;
    const ptrdiff_t offset = attr - raa491901_attrs;
    struct raa491901_dev *raa491901 = i2c_get_clientdata(dev_get_drvdata(dev));

    switch(offset) {
        case RAA491901_LED_GAINS:
            ret = raa491901_show_led_gains(raa491901, buf);
            break;
        case RAA491901_LED_CURRENTS:
            ret = raa491901_show_led_currents(raa491901, buf);
            break;
        case RAA491901_LED_MODE:
            ret = sprintf(buf, "%s\n", (raa491901->mode == RAA491901_MODE_PWM)
                                ? LED_PWM_MODE_IO_STRING : LED_PFM_MODE_IO_STRING);
            break;
        case RAA491901_VBLANK_COEFFS:
            ret = sprintf(buf, VBLANK_IO_STRING, vblank_red_m, vblank_red_b,
                            vblank_grn_m, vblank_grn_b, vblank_blu_m, vblank_blu_b, vblank_low_current_value);
            break;
        case RAA491901_LED_VOLTAGES:
            ret = raa491901_show_led_voltages(raa491901, buf);
            break;
        case RAA491901_INTERPOLATION_COEFFS:
            ret = sprintf(buf, INTERPOLATION_IO_STRING, raa491901->slope_high_res, raa491901->offset_high_res_uA,
                                raa491901->slope_low_res, raa491901->offset_low_res_uA);
            break;
        case RAA491901_RESOLUTION:
            ret = sprintf(buf, "%s\n", (raa491901->is_high_res) ? "high" : "low");
            break;
        case RAA491901_BIT_RES_SETTING:
            switch (raa491901->bit_res_setting) {
                case RAA491901_BIT_RES_AUTO:
                    ret = sprintf(buf, "%s\n", LED_RESOLUTION_AUTO_SETTING_STRING);
                    break;
                case RAA491901_BIT_RES_LOW:
                    ret = sprintf(buf, "%s\n", LED_RESOLUTION_LOW_SETTING_STRING);
                    break;
                case RAA491901_BIT_RES_HIGH:
                    ret = sprintf(buf, "%s\n", LED_RESOLUTION_HIGH_SETTING_STRING);
                    break;
                default:
                    return -EINVAL;
            }
            break;
        case RAA491901_LED_SWITCH:
            ret = raa491901_show_led_switch(raa491901, buf);
            break;
        default:
            return -EINVAL;
    }

    return ret;
}

static ssize_t raa491901_store_led_gains(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    int ret = 0;
    struct raa491901_gain_cmd led_gains;
    int red_gain, green_gain, blue_gain;

    if (count < LED_IO_STRING_MIN_SIZE || count > LED_IO_STRING_MAX_SIZE) {
        pr_err("<%s>: Invalid buffer size %zu\n", __func__, count);
        return -EINVAL;
    }

    ret = sscanf(buf, LED_IO_STRING, &red_gain, &green_gain, &blue_gain);
    if (ret != 3) {
        pr_err("<%s>: Failed to parse gain values from input string: %s\n", __func__, buf);
        return -EINVAL;
    }

    if (red_gain < 0 || red_gain > LED_MAX_GAIN ||
        blue_gain < 0 || blue_gain > LED_MAX_GAIN ||
        green_gain < 0 || green_gain > LED_MAX_GAIN) {
        pr_err("<%s>: Invalid gain values\n", __func__);
        return -EINVAL;
    }

    if ((red_gain == 0) && (green_gain == 0) && (blue_gain == 0)) {
        raa491901_toggle_bit_res(raa491901, true);
        return count;
    }

    led_gains.red_gain = (uint16_t) red_gain;
    led_gains.green_gain = (uint16_t) green_gain;
    led_gains.blue_gain = (uint16_t) blue_gain;
    ret = raa491901_write_led_gains(raa491901, &led_gains);
    if (ret < 0) {
        pr_err("<%s>: Failed to write led gains: %d\n", __func__, ret);
        return ret;
    }

    return count;
}

static ssize_t raa491901_store_led_currents(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    int ret = 0;
    struct raa491901_gain_cmd led_gains;
    int red_current, green_current, blue_current;
    bool is_high_res;
    int offset_uA, slope;

    if (count < LED_IO_STRING_MIN_SIZE || count > LED_IO_STRING_MAX_SIZE) {
        pr_err("<%s>: Invalid buffer size %zu\n", __func__, count);
        return -EINVAL;
    }

    ret = sscanf(buf, LED_IO_STRING, &red_current, &green_current, &blue_current);
    if (ret != 3 || red_current < 0 || blue_current < 0 || green_current < 0) {
        pr_err("<%s>: Invalid input string with current values: %s\n", __func__, buf);
        return -EINVAL;
    }

    if ((red_current == 0) && (green_current == 0) && (blue_current == 0)) {
        raa491901_toggle_bit_res(raa491901, true);
        return count;
    }

    if (raa491901->bit_res_setting == RAA491901_BIT_RES_AUTO) {
        is_high_res = raa491901_get_res(red_current, green_current, blue_current);
    } else {
        is_high_res = raa491901->bit_res_setting == RAA491901_BIT_RES_HIGH;
    }

    if (is_high_res) {
        offset_uA = raa491901->offset_high_res_uA;
        slope = raa491901->slope_high_res;
    } else {
        offset_uA = raa491901->offset_low_res_uA;
        slope = raa491901->slope_low_res;
    }

    led_gains.red_gain = CURRENT_TO_GAIN(red_current, slope, offset_uA);
    led_gains.green_gain = CURRENT_TO_GAIN(green_current, slope, offset_uA);
    led_gains.blue_gain = CURRENT_TO_GAIN(blue_current, slope, offset_uA);

    if (raa491901->mode != RAA491901_MODE_PWM) {
        ret = raa491901_set_led_mode(raa491901, RAA491901_MODE_PWM);
        if (ret != 0) {
            pr_err("<%s>: Failed to set LED mode to PWM: %d\n", __func__, ret);
            return ret;
        }
    }

    ret = raa491901_set_default_vblank(raa491901, red_current, green_current, blue_current);

    ret = raa491901_write_led_gains(raa491901, &led_gains);
    if (ret < 0) {
        pr_err("<%s>: Failed to write led gains: %d\n", __func__, ret);
        return ret;
    }

    raa491901_toggle_bit_res(raa491901, is_high_res);

    return count;
}

static size_t raa491901_store_led_mode(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    int ret = 0;

    if (strncmp(buf, LED_PFM_MODE_IO_STRING, 3) == 0) {
        ret = raa491901_set_led_mode(raa491901, RAA491901_MODE_PFM);
    } else if (strncmp(buf, LED_PWM_MODE_IO_STRING, 3) == 0) {
        ret = raa491901_set_led_mode(raa491901, RAA491901_MODE_PWM);
    } else {
        pr_err("<%s>: Invalid LED mode %s\n", __func__, buf);
        return -EINVAL;
    }

    if (ret < 0) {
        pr_err("<%s>: Failed to set LED mode: %d\n", __func__, ret);
        return ret;
    }

    return count;
}

static size_t raa491901_store_vblank_coeffs(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    int ret = 0;
    int red_m, red_b, grn_m, grn_b, blu_m, blu_b, low_current_b;

    ret = sscanf(buf, VBLANK_IO_STRING, &red_m, &red_b, &grn_m, &grn_b, &blu_m, &blu_b, &low_current_b);
    if (ret != 7) {
        pr_err("<%s>: Invalid input string with vblank coefficient values: %s\n", __func__, buf);
        return -EINVAL;
    }

    if (mutex_lock_killable(&raa491901->mutex))
        return -EINTR;

    vblank_red_m = red_m;
    vblank_red_b = red_b;
    vblank_grn_m = grn_m;
    vblank_grn_b = grn_b;
    vblank_blu_m = blu_m;
    vblank_blu_b = blu_b;
    vblank_low_current_value = low_current_b;

    mutex_unlock(&raa491901->mutex);
    return count;
}

static size_t raa491901_store_interpolation_coeffs(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    int ret = 0;
    int slope_high_res, slope_low_res, offset_high_res, offset_low_res;

    ret = sscanf(buf, INTERPOLATION_IO_STRING, &slope_high_res, &offset_high_res, &slope_low_res, &offset_low_res);
    if (ret != 4 ||
        slope_high_res < 0 ||
        slope_low_res < 0 ||
        offset_high_res < 0) {
        pr_err("<%s>: Invalid input string with offset current value: %s\n", __func__, buf);
        return -EINVAL;
    }

    if (mutex_lock_killable(&raa491901->mutex))
        return -EINTR;

    if (offset_low_res > HIGH_RES_THRESH_UA) {
        offset_low_res = HIGH_RES_THRESH_UA;
    } else if (offset_low_res < -HIGH_RES_THRESH_UA) {
        offset_low_res = -HIGH_RES_THRESH_UA;
    }

    raa491901->offset_high_res_uA = (unsigned int) offset_high_res;
    raa491901->offset_low_res_uA = offset_low_res;
    raa491901->slope_high_res = (unsigned int) slope_high_res;
    raa491901->slope_low_res = (unsigned int) slope_low_res;
    max_high_res_thresh_uA = GAIN_TO_CURRENT(1023, slope_high_res, offset_high_res);

    mutex_unlock(&raa491901->mutex);
    return count;
}

static size_t raa491901_store_bit_res_setting(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    if (strncmp(buf, LED_RESOLUTION_AUTO_SETTING_STRING, 4) == 0) {
        raa491901->bit_res_setting = RAA491901_BIT_RES_AUTO;
    } else if (strncmp(buf, LED_RESOLUTION_LOW_SETTING_STRING, 3) == 0) {
        raa491901->bit_res_setting = RAA491901_BIT_RES_LOW;
    } else if (strncmp(buf, LED_RESOLUTION_HIGH_SETTING_STRING, 4) == 0) {
        raa491901->bit_res_setting = RAA491901_BIT_RES_HIGH;
    } else {
        pr_err("<%s>: Invalid LED mode %s\n", __func__, buf);
        return -EINVAL;
    }

    return count;
}

static size_t raa491901_store_led_switch(struct raa491901_dev *raa491901, const char* buf, size_t count)
{
    if ((buf != NULL) && (buf[0] == '1')) {
        raa491901_toggle_led(raa491901, true);
    } else {
        raa491901_toggle_led(raa491901, false);
    }

    return count;
}

static ssize_t raa491901_store_attrs(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
    ssize_t ret = 0;
    const ptrdiff_t offset = attr - raa491901_attrs;
    struct raa491901_dev *raa491901 = i2c_get_clientdata(dev_get_drvdata(dev));

    switch(offset) {
        case RAA491901_LED_GAINS:
            ret = raa491901_store_led_gains(raa491901, buf, count);
            break;
        case RAA491901_LED_CURRENTS:
            ret = raa491901_store_led_currents(raa491901, buf, count);
            break;
        case RAA491901_LED_MODE:
            ret = raa491901_store_led_mode(raa491901, buf, count);
            break;
        case RAA491901_VBLANK_COEFFS:
            ret = raa491901_store_vblank_coeffs(raa491901, buf, count);
            break;
        case RAA491901_INTERPOLATION_COEFFS:
            ret = raa491901_store_interpolation_coeffs(raa491901, buf, count);
            break;
        case RAA491901_BIT_RES_SETTING:
            ret = raa491901_store_bit_res_setting(raa491901, buf, count);
            break;
        case RAA491901_LED_SWITCH:
            ret = raa491901_store_led_switch(raa491901, buf, count);
            break;
        default:
            return -EINVAL;
    }

    return ret;
}

static int raa491901_create_attrs(struct device *dev)
{
    int i, ret;

    for (i = 0; i < (int)ARRAY_SIZE(raa491901_attrs); i++) {
        ret = device_create_file(dev, &raa491901_attrs[i]);
        if (ret)
            goto create_attrs_failed;
    }

    return ret;

create_attrs_failed:
    dev_err(dev, "<%s>: Error %d when creating attr at index %d\n", __func__, ret, i);
    while (--i >= 0)
        device_remove_file(dev, &raa491901_attrs[i]);
    return ret;
}

static void raa491901_remove_attrs(struct device *dev)
{
    int i;

    for (i = 0; i < (int)ARRAY_SIZE(raa491901_attrs); i++) {
        device_remove_file(dev, &raa491901_attrs[i]);
    }
}

static void raa491901_set_interpolation_coeffs(struct raa491901_dev *raa491901)
{
    struct device_node *of_node = raa491901->client->dev.of_node;
    u32 low_res_coeffs[2] = {0};
    u32 high_res_coeffs[2] = {0};

    // Check if interpolation coefficients are defined in device tree. If not, use the default values.
    if (of_property_read_u32_array(of_node, "interpolation-coeffs-high-res", high_res_coeffs, 2)) {
        raa491901->offset_high_res_uA = default_offset_high_res_uA;
        raa491901->slope_high_res = default_slope_high_res;
    } else {
        raa491901->slope_high_res = high_res_coeffs[0];
        raa491901->offset_high_res_uA = high_res_coeffs[1];
    }

    if (of_property_read_u32_array(of_node, "interpolation-coeffs-low-res", low_res_coeffs, 2)) {
        raa491901->offset_low_res_uA = default_offset_low_res_uA;
        raa491901->slope_low_res = default_slope_low_res;
    } else {
        raa491901->slope_low_res = low_res_coeffs[0];
        raa491901->offset_low_res_uA = low_res_coeffs[1];
    }
}

static void raa491901_preboot_on(struct raa491901_dev *raa491901, bool on)
{
    u8 optical_gains[5]           = {0x2D, 0x10, 0x21, 0x00, 0x00};
    u8 voltage_blank_msb[1]       = {0x80};
    u8 voltage_blank_lsb[1]       = {0x38};
    u8 auto_headroom[1]           = {0xC4};

    if (!mutex_lock_killable(&raa491901->mutex)) {
        if (on) {
            gpiod_set_value(raa491901->gpio_led_en, 1);
            msleep(1);

            raa491901_i2c_write(raa491901->regmap, REG_RED_GAIN_LSB,
                    optical_gains, sizeof(optical_gains));
            raa491901_i2c_write(raa491901->regmap, REG_LED_BLANK_VOL_MSB,
                    voltage_blank_msb, sizeof(voltage_blank_msb));
            raa491901_i2c_write(raa491901->regmap, REG_LED_BLANK_VOL_LSB,
                    voltage_blank_lsb, sizeof(voltage_blank_lsb));
            raa491901_i2c_write(raa491901->regmap, REG_FUNC_ENABLE,
                    auto_headroom, sizeof(auto_headroom));
            raa491901_toggle_bit_res(raa491901, false);
        } else {
            gpiod_set_value(raa491901->gpio_led_en, 0);
            raa491901_toggle_bit_res(raa491901, true);
        }
        mutex_unlock(&raa491901->mutex);
    }
}

static int raa491901_preboot_on_fn(void *data)
{
	struct raa491901_dev *raa491901 = data;

    raa491901->userspace_takeover = false;
    while (!raa491901->userspace_takeover) {
        if (!gpiod_get_value(raa491901->gpio_led_en) && gpiod_get_value(raa491901->gpio_dd_en)) {
            raa491901_preboot_on(raa491901, true);
        } else if (gpiod_get_value(raa491901->gpio_led_en) && !gpiod_get_value(raa491901->gpio_dd_en)) {
            raa491901_preboot_on(raa491901, false);
        }
        msleep(10);
    }
	return 0;
}

static irqreturn_t dondoff_irq_handler(int irq, void *dev_id) {
	struct raa491901_dev *raa491901 = dev_id;

	int gpio = gpiod_get_value(raa491901->gpio_dd_en);
    atomic_set(&raa491901->is_don, gpio);
    sysfs_notify_dirent(raa491901->dondoff_kn);

	return IRQ_HANDLED;
}

static int raa491901_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct raa491901_dev *raa491901 = NULL;
    struct device *device = NULL;
    dev_t dev;

    raa491901 = kzalloc(sizeof(struct raa491901_dev), GFP_KERNEL);
    if (!raa491901)
        return -ENOMEM;
    mutex_init(&raa491901->mutex);
    raa491901->client = client;
    raa491901->is_high_res = false;
    raa491901->bit_res_setting = RAA491901_BIT_RES_AUTO;

    raa491901_set_interpolation_coeffs(raa491901);

    raa491901->regmap = devm_regmap_init_i2c(client, &raa491901_regmap_config);
    if (IS_ERR(raa491901->regmap)) {
        ret = PTR_ERR(raa491901->regmap);
        pr_err("<%s>: Error %d in initializing regmap\n", __func__, ret);
        goto error;
    }

    ret = alloc_chrdev_region(&dev, MAJOR_NUM, 1, CLASS_NAME);
    if (ret < 0) {
        pr_err("<%s>: Error %d in allocating chrdev region\n", __func__, ret);
        goto error;
    }

    cdev_init(&raa491901->cdev, &raa491901_fops);
    raa491901->cdev.owner = THIS_MODULE;

    device = device_create(raa491901_class, NULL, dev,
                NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        pr_err("<%s>: Error %d in creating device\n", __func__, ret);
        goto error_device_create;
    }

    ret = cdev_add(&raa491901->cdev, dev, 1);
    if (ret < 0) {
        pr_err("<%s>: Error %d when adding cdev\n", __func__, ret);
        goto error_cdev_add;
    }

    ret = raa491901_create_attrs(device);
    if (ret) {
        pr_err("<%s>: Failed to create attributes\n", __func__);
        goto error_cdev_add;
    }

    // Set led bit resolution to low resolution initially
    raa491901->gpio_bit_res = devm_gpiod_get(&client->dev, "ledbitres", GPIOD_OUT_HIGH);
    if (IS_ERR(raa491901->gpio_bit_res)) {
        ret = PTR_ERR(raa491901->gpio_bit_res);
        pr_err("<%s>: Error %d in getting gpio for togging bit resolution\n", __func__, ret);
        goto error_cdev_add;
    }

    raa491901->gpio_led_en = devm_gpiod_get(&client->dev, "ledenable", GPIOD_ASIS);
    if (IS_ERR(raa491901->gpio_led_en)) {
        ret = PTR_ERR(raa491901->gpio_led_en);
        pr_err("<%s>: Error %d in getting gpio for enabling led\n", __func__, ret);
        goto error_cdev_add;
    }

    raa491901->gpio_dd_en = devm_gpiod_get(&client->dev, "leddondof", GPIOD_IN);

    if (!IS_ERR(raa491901->gpio_dd_en)) {
		raa491901->dondoff_irq = gpiod_to_irq(raa491901->gpio_dd_en);
		if (raa491901->dondoff_irq < 0) {
			pr_err("failed to get dondoff_irq IRQ\n");
			goto error_cdev_add;
		}

        ret = device_create_file(device, &dev_attr_dondoff);
        if (ret) {
            pr_err("Cannot create dondoff\n");
            goto error_cdev_add;
        }
        raa491901->dondoff_kn = sysfs_get_dirent(device->kobj.sd, "dondoff");

        if (gpiod_get_value(raa491901->gpio_dd_en)) {
            atomic_set(&raa491901->is_don, 1);
        }

		ret = devm_request_threaded_irq(&client->dev, raa491901->dondoff_irq, dondoff_irq_handler,
				NULL, DONDOFF_IRQF,
				DEVICE_NAME, raa491901);
		if (ret) {
			pr_err("failed to request ID IRQ\n");
			    goto error_cdev_add;
		}

        kthread_run(raa491901_preboot_on_fn, raa491901, "raa491901_preboot_on");
    }

    i2c_set_clientdata(client, raa491901);
    dev_set_drvdata(device, client);

    pr_info("<%s>: Initialized raa491901\n", __func__);

    return 0;

error_cdev_add:
    device_destroy(raa491901_class, dev);
error_device_create:
    unregister_chrdev_region(dev, 1);
error:
    kfree(raa491901);

    return ret;
}

static int raa491901_remove(struct i2c_client *client)
{
    struct raa491901_dev *raa491901;
    dev_t devt;

    raa491901 = i2c_get_clientdata(client);
    devt = raa491901->cdev.dev;

    raa491901_remove_attrs(&client->dev);
    cdev_del(&raa491901->cdev);
    device_destroy(raa491901_class, devt);
    unregister_chrdev_region(devt, 1);
    kfree(raa491901);

    return 0;
}

static const struct i2c_device_id raa491901_id_table[] = {
    { DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, raa491901_id_table);

static const struct of_device_id raa491901_of_match[] = {
    { .compatible = "meta,raa491901-i2c"},
    { }
};
MODULE_DEVICE_TABLE(of, raa491901_of_match);

static struct i2c_driver raa491901_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = of_match_ptr(raa491901_of_match),
    },
    .probe = raa491901_probe,
    .remove = raa491901_remove,
    .id_table = raa491901_id_table,
};

static int __init raa491901_init(void)
{
    int status;

    raa491901_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(raa491901_class))
        return PTR_ERR(raa491901_class);

    status = i2c_register_driver(THIS_MODULE, &raa491901_driver);
    if (status < 0)
        class_destroy(raa491901_class);

    return status;
}
module_init(raa491901_init);

static void __exit raa491901_exit(void)
{
    i2c_del_driver(&raa491901_driver);
    class_destroy(raa491901_class);
}
module_exit(raa491901_exit);

MODULE_DESCRIPTION("raa491901 I2C driver");
MODULE_LICENSE("GPL");

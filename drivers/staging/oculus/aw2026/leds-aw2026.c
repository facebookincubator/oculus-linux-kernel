// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "leds-aw2026.h"

/* register address */
#define AW2026_REG_RESET				0x00
#define AW2026_REG_GCR					0x01
#define AW2026_REG_STATUS				0x02
#define AW2026_REG_IMAX					0x03
#define AW2026_REG_LCFG1				0x04
#define AW2026_REG_LCFG2				0x05
#define AW2026_REG_LCFG3				0x06
#define AW2026_REG_LEDEN				0x07
#define AW2026_REG_PAT_RUN				0x09
#define AW2026_REG_ILED1				0x10
#define AW2026_REG_ILED2				0x11
#define AW2026_REG_ILED3				0x12
#define AW2026_REG_PWM1					0x1C
#define AW2026_REG_PWM2					0x1D
#define AW2026_REG_PWM3					0x1E
#define AW2026_REG_PAT1_T1				0x30
#define AW2026_REG_PAT1_T2				0x31
#define AW2026_REG_PAT1_T3				0x32
#define AW2026_REG_PAT1_T4				0x33
#define AW2026_REG_PAT1_T5				0x34
#define AW2026_REG_PAT2_T1				0x35
#define AW2026_REG_PAT2_T2				0x36
#define AW2026_REG_PAT2_T3				0x37
#define AW2026_REG_PAT2_T4				0x38
#define AW2026_REG_PAT2_T5				0x39
#define AW2026_REG_PAT3_T1				0x3A
#define AW2026_REG_PAT3_T2				0x3B
#define AW2026_REG_PAT3_T3				0x3C
#define AW2026_REG_PAT3_T4				0x3D
#define AW2026_REG_PAT3_T5				0x3E

/* register bits */
#define AW2026_CHIPID					0x31
#define AW2026_LED_RESET_MASK			0x55
#define AW2026_LED_CHIP_DISABLE			0x00
#define AW2026_LED_CHIP_ENABLE_MASK		0x01
#define AW2026_LED_BREATHE_MODE_MASK	0x01
#define AW2026_LED_ON_MODE_MASK			0x00
#define AW2026_LED_BREATHE_PWM_MASK		0xFF
#define AW2026_LED_ON_PWM_MASK			0xFF
#define AW2026_LED_FADEIN_MODE_MASK		0x02
#define AW2026_LED_FADEOUT_MODE_MASK	0x04
#define AW2026_REG_LEDEN_DISABLE		0x00

#define MAX_RISE_TIME_MS				15
#define MAX_HOLD_TIME_MS				15
#define MAX_FALL_TIME_MS				15
#define MAX_OFF_TIME_MS					15
#define MAX_IMAX_VAL					3
#define MAX_LED_CURRENT_VAL				255

#define AW2026_REG_MAX AW2026_REG_PAT3_T5

struct aw2026_led {
	struct i2c_client *client;
	struct led_classdev cdev;
	struct regmap *regmap;
	struct aw2026_platform_data *pdata;
	struct work_struct brightness_work;
	struct mutex lock;
	int num_leds;
	int id;
};

static bool aw2026_readable(struct device *dev, unsigned int reg)
{
	return (reg >= AW2026_REG_RESET && reg <= AW2026_REG_MAX);
}

static bool aw2026_writeable(struct device *dev, unsigned int reg)
{
	return !(reg == AW2026_REG_STATUS);
}

static const struct regmap_config aw2026_regmap = {
	.name = "aw2026",
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AW2026_REG_MAX,
	.readable_reg = aw2026_readable,
	.writeable_reg = aw2026_writeable,

	.cache_type = REGCACHE_NONE,
	.use_single_read = true,
	.use_single_write = true,
};

static int aw2026_write(struct aw2026_led *led, u8 reg, u8 val)
{
	int ret = 0;
	ret = regmap_write(led->pdata->led->regmap, reg, val);
	if (ret < 0) {
		dev_err(&led->client->dev,
			"%s: Failed to write 0x%02x to reg 0x%02x, ret=%d\n",
			__func__, val, reg, ret);
		return ret;
	}
	return 0;
}

static int aw2026_read(struct aw2026_led *led, u8 reg, u8 *val)
{
	int ret = 0;
	unsigned int reg_val = 0;

	ret = regmap_read(led->pdata->led->regmap, reg, &reg_val);
	if (ret < 0) {
		dev_err(&led->client->dev,
			"%s: Failed to read from reg 0x%02x, ret=%d\n",
			__func__, reg, ret);
		return ret;
	}
	*val = (u8)reg_val;

	return 0;
}

static void aw2026_soft_reset(struct aw2026_led *led)
{
	aw2026_write(led, AW2026_REG_RESET, AW2026_LED_RESET_MASK);
	aw2026_write(led, AW2026_REG_LEDEN, AW2026_REG_LEDEN_DISABLE);
	usleep_range(5000, 5500);
}

static int enable_aw2026(struct aw2026_led *led, struct device * dev){
	u8 val;

	/* enable aw2026 if disabled */
	aw2026_read(led, AW2026_REG_GCR, &val);
	if (!(val&0x01)){
		aw2026_write(led, AW2026_REG_GCR, AW2026_LED_CHIP_ENABLE_MASK);
	}

	return val&0x01;
}

static int disable_aw2026(struct aw2026_led *led, struct device * dev){
	u8 val;

	/*
	 * If value in AW2026_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2026_read(led, AW2026_REG_LEDEN, &val);
	if (val == 0) {
		aw2026_write(led, AW2026_REG_GCR, AW2026_LED_CHIP_DISABLE);
	}

	return val;
}

static void aw2026_brightness_work(struct work_struct *work)
{
	struct aw2026_led *led = container_of(work, struct aw2026_led,
					brightness_work);
	u8 val;
	struct device *dev = &led->client->dev;

	mutex_lock(&led->pdata->led->lock);

	enable_aw2026(led, dev);

	if (led->cdev.brightness > 0) {
		if (led->cdev.brightness > led->cdev.max_brightness)
			led->cdev.brightness = led->cdev.max_brightness;
		aw2026_write(led, AW2026_REG_LCFG1 + led->id, AW2026_LED_ON_MODE_MASK);
		aw2026_write(led, AW2026_REG_IMAX, led->pdata->imax);
		aw2026_write(led, AW2026_REG_ILED1 + led->id, led->cdev.brightness);
		aw2026_write(led, AW2026_REG_PWM1 + led->id, AW2026_LED_ON_PWM_MASK);
		aw2026_read(led, AW2026_REG_LEDEN, &val);
		aw2026_write(led, AW2026_REG_LEDEN, val | (1 << led->id));
	} else {
		aw2026_read(led, AW2026_REG_LEDEN, &val);
		aw2026_write(led, AW2026_REG_LEDEN, val & (~(1 << led->id)));
	}

	disable_aw2026(led, dev);

	mutex_unlock(&led->pdata->led->lock);
}

static void aw2026_led_blink_set(struct aw2026_led *led, unsigned long blinking)
{
	u8 val;

	/* enable regulators if they are disabled */
	/* enable aw2026 if disabled */
	aw2026_read(led, AW2026_REG_GCR, &val);
	if (!(val&0x01))
		aw2026_write(led, AW2026_REG_GCR, AW2026_LED_CHIP_ENABLE_MASK);

	led->cdev.brightness = blinking ? led->cdev.max_brightness : 0;

	if (blinking > 0) {
		aw2026_write(led, AW2026_REG_LCFG1 + led->id, AW2026_LED_BREATHE_MODE_MASK);
		aw2026_write(led, AW2026_REG_IMAX, led->pdata->imax);
		aw2026_write(led, AW2026_REG_ILED1 + led->id, led->pdata->led_current);
		aw2026_write(led, AW2026_REG_PWM1 + led->id, AW2026_LED_BREATHE_PWM_MASK);
		aw2026_write(led, AW2026_REG_PAT1_T1 + led->id*5,
					(led->pdata->rise_time_ms << 4 | led->pdata->hold_time_ms));
		aw2026_write(led, AW2026_REG_PAT1_T2 + led->id*5,
					(led->pdata->fall_time_ms << 4 | led->pdata->off_time_ms));

		aw2026_read(led, AW2026_REG_LEDEN, &val);
		aw2026_write(led, AW2026_REG_LEDEN, val | (1 << led->id));

		aw2026_write(led, AW2026_REG_PAT_RUN, (1 << led->id));
	} else {
		aw2026_read(led, AW2026_REG_LEDEN, &val);
		aw2026_write(led, AW2026_REG_LEDEN, val & (~(1 << led->id)));
	}

	/*
	 * If value in AW2026_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2026_read(led, AW2026_REG_LEDEN, &val);
	if (val == 0) {
		aw2026_write(led, AW2026_REG_GCR, AW2026_LED_CHIP_DISABLE);
		return;
	}
}

static void aw2026_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct aw2026_led *led = container_of(cdev, struct aw2026_led, cdev);

	led->cdev.brightness = brightness;

	schedule_work(&led->brightness_work);
}

static ssize_t aw2026_blink_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led =
			container_of(led_cdev, struct aw2026_led, cdev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	mutex_lock(&led->pdata->led->lock);
	aw2026_led_blink_set(led, blinking);
	mutex_unlock(&led->pdata->led->lock);

	sysfs_notify_dirent(led->pdata->brightness_kn);

	return len;
}

static ssize_t aw2026_led_time_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led =
			container_of(led_cdev, struct aw2026_led, cdev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			led->pdata->rise_time_ms, led->pdata->hold_time_ms,
			led->pdata->fall_time_ms, led->pdata->off_time_ms);
}

static ssize_t aw2026_led_time_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led =
			container_of(led_cdev, struct aw2026_led, cdev);
	int rc, rise_time_ms, hold_time_ms, fall_time_ms, off_time_ms;

	rc = sscanf(buf, "%d %d %d %d",
			&rise_time_ms, &hold_time_ms,
			&fall_time_ms, &off_time_ms);

	mutex_lock(&led->pdata->led->lock);
	led->pdata->rise_time_ms = (rise_time_ms > MAX_RISE_TIME_MS) ?
				MAX_RISE_TIME_MS : rise_time_ms;
	led->pdata->hold_time_ms = (hold_time_ms > MAX_HOLD_TIME_MS) ?
				MAX_HOLD_TIME_MS : hold_time_ms;
	led->pdata->fall_time_ms = (fall_time_ms > MAX_FALL_TIME_MS) ?
				MAX_FALL_TIME_MS : fall_time_ms;
	led->pdata->off_time_ms = (off_time_ms > MAX_OFF_TIME_MS) ?
				MAX_OFF_TIME_MS : off_time_ms;
	mutex_unlock(&led->pdata->led->lock);
	return len;
}

static ssize_t aw2026_led_brightness_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led =
			container_of(led_cdev, struct aw2026_led, cdev);

	return snprintf(buf, PAGE_SIZE, "imax=%d, led-current=%d\n",
			led->pdata->imax, led->pdata->led_current);
}

static ssize_t aw2026_led_brightness_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led =
			container_of(led_cdev, struct aw2026_led, cdev);
	int rc, imax, led_current;

	rc = sscanf(buf, "%d %d", &imax, &led_current);

	mutex_lock(&led->pdata->led->lock);
	led->pdata->imax = (imax > MAX_IMAX_VAL) ?
				MAX_IMAX_VAL : imax;
	led->pdata->led_current = (led_current > MAX_LED_CURRENT_VAL) ?
				MAX_LED_CURRENT_VAL : ((led_current < 0) ? 0 : led_current);
	mutex_unlock(&led->pdata->led->lock);
	return len;
}

static ssize_t aw2026_led_on_show(struct device * dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led = container_of(led_cdev, struct aw2026_led, cdev);
	u8 val;

	mutex_lock(&led->pdata->led->lock);
	aw2026_read(led, AW2026_REG_LEDEN, &val);
	val = (val >> led->id) & 1;
	mutex_unlock(&led->pdata->led->lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t aw2026_led_on_store(struct device *dev,
				struct device_attribute *attr,
				const char * buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led = container_of(led_cdev, struct aw2026_led, cdev);
	int rc, val_on;
	u8 val;

	/*
	 * 1: Turn on
	 * 0: Turn off
	*/
	rc = sscanf(buf, "%d", &val_on);
	val_on = (val_on > 1) ? 1 : val_on;

	mutex_lock(&led->pdata->led->lock);

	enable_aw2026(led, dev);

	aw2026_read(led, AW2026_REG_LEDEN, &val);

	if (val_on == 1){
		aw2026_write(led, AW2026_REG_LCFG1 + led->id, AW2026_LED_ON_MODE_MASK);
		aw2026_write(led, AW2026_REG_IMAX, led->pdata->imax);
		aw2026_write(led, AW2026_REG_ILED1 + led->id, led->pdata->led_current);
		aw2026_write(led, AW2026_REG_PWM1 + led->id, AW2026_LED_ON_PWM_MASK);
		aw2026_write(led, AW2026_REG_LEDEN, val | (1 << led->id));
	}else{
		aw2026_write(led, AW2026_REG_LEDEN, val & (~(1 << led->id)));
	}

	disable_aw2026(led, dev);

	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2026_led_current_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led = container_of(led_cdev, struct aw2026_led, cdev);
	u8 val;

	mutex_lock(&led->pdata->led->lock);
	aw2026_read(led, AW2026_REG_ILED1 + led->id, &val);
	mutex_unlock(&led->pdata->led->lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t aw2026_led_current_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led = container_of(led_cdev, struct aw2026_led, cdev);
	int rc, val_current;

	rc = sscanf(buf, "%d", &val_current);

	led->pdata->led_current = (val_current > MAX_LED_CURRENT_VAL) ?
				MAX_LED_CURRENT_VAL : ((val_current < 0) ? 0 : val_current);

	mutex_lock(&led->pdata->led->lock);
	enable_aw2026(led, dev);
	aw2026_write(led, AW2026_REG_ILED1 + led->id, led->pdata->led_current);
	disable_aw2026(led, dev);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2026_led_imax_show(struct device * dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led = container_of(led_cdev, struct aw2026_led, cdev);
	u8 val;

	mutex_lock(&led->pdata->led->lock);
	aw2026_read(led, AW2026_REG_IMAX, &val);
	mutex_unlock(&led->pdata->led->lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t aw2026_led_imax_store(struct device *dev,
				struct device_attribute *attr,
				const char * buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2026_led *led = container_of(led_cdev, struct aw2026_led, cdev);
	int rc, val_imax;

	rc = sscanf(buf, "%d", &val_imax);

	led->pdata->imax = (val_imax > MAX_IMAX_VAL) ?
				MAX_IMAX_VAL : val_imax;

	mutex_lock(&led->pdata->led->lock);

	enable_aw2026(led, dev);

	aw2026_write(led, AW2026_REG_IMAX, led->pdata->imax);

	disable_aw2026(led, dev);

	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static DEVICE_ATTR_WO(aw2026_blink);
static DEVICE_ATTR_RW(aw2026_led_time);
static DEVICE_ATTR_RW(aw2026_led_brightness);
static DEVICE_ATTR_RW(aw2026_led_on);
static DEVICE_ATTR_RW(aw2026_led_current);
static DEVICE_ATTR_RW(aw2026_led_imax);

static struct attribute *aw2026_led_attributes[] = {
	&dev_attr_aw2026_blink.attr,
	&dev_attr_aw2026_led_time.attr,
	&dev_attr_aw2026_led_brightness.attr,
	&dev_attr_aw2026_led_on.attr,
	&dev_attr_aw2026_led_imax.attr,
	&dev_attr_aw2026_led_current.attr,
	NULL,
};

static struct attribute_group aw2026_led_attr_group = {
	.attrs = aw2026_led_attributes
};

static int aw2026_check_chipid(struct aw2026_led *led)
{
	u8 val;
	u8 cnt;

	for (cnt = 5; cnt > 0; cnt--) {
		aw2026_read(led, AW2026_REG_RESET, &val);
		dev_info(&led->client->dev, "AW2026 chip id %0x", val);
		if (val == AW2026_CHIPID)
			return 0;
	}

	return -EINVAL;
}

static int aw2026_led_err_handle(struct aw2026_led *led_array,
				int parsed_leds)
{
	int i;
	/*
	 * If probe fails, cannot free resource of all LEDs, only free
	 * resources of LEDs which have allocated these resource really.
	 */
	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2026_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&led_array->client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	return i;
}

static int aw2026_led_parse_child_node(struct aw2026_led *led_array,
				struct device_node *node)
{
	struct aw2026_led *led;
	struct device_node *temp;
	struct aw2026_platform_data *pdata;
	int rc = 0, parsed_leds = 0;

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->client = led_array->client;

		pdata = devm_kzalloc(&led->client->dev,
				sizeof(struct aw2026_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&led->client->dev,
				"Failed to allocate memory\n");
			goto free_err;
		}
		pdata->led = led_array;
		led->pdata = pdata;

		rc = of_property_read_string(temp, "aw2026,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,id",
			&led->id);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading id, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,imax",
			&led->pdata->imax);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading imax, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,led-current",
			&led->pdata->led_current);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led-current, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,max-brightness",
			&led->cdev.max_brightness);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading max-brightness, rc = %d\n",
				rc);
			goto free_pdata;
		}
		rc = of_property_read_u32(temp, "aw2026,rise-time-ms",
			&led->pdata->rise_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading rise-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,hold-time-ms",
			&led->pdata->hold_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading hold-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,fall-time-ms",
			&led->pdata->fall_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading fall-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2026,off-time-ms",
			&led->pdata->off_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading off-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		INIT_WORK(&led->brightness_work, aw2026_brightness_work);

		led->cdev.brightness_set = aw2026_set_brightness;

		rc = led_classdev_register(&led->client->dev, &led->cdev);
		if (rc) {
			dev_err(&led->client->dev,
				"unable to register led %d,rc=%d\n",
				led->id, rc);
			goto free_pdata;
		}

		rc = sysfs_create_group(&led->cdev.dev->kobj,
				&aw2026_led_attr_group);
		if (rc) {
			dev_err(&led->client->dev, "led sysfs rc: %d\n", rc);
			goto free_class;
		}

		led->pdata->brightness_kn = sysfs_get_dirent(led->cdev.dev->kobj.sd, "brightness");
		if(!led->pdata->brightness_kn) {
			dev_err(&led->client->dev, "failed to get brightness kernel fs node");
			goto free_class;
		}

		parsed_leds++;
	}

	return 0;

free_class:
	aw2026_led_err_handle(led_array, parsed_leds);
	led_classdev_unregister(&led_array[parsed_leds].cdev);
	cancel_work_sync(&led_array[parsed_leds].brightness_work);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	led_array[parsed_leds].pdata = NULL;
	return rc;

free_pdata:
	aw2026_led_err_handle(led_array, parsed_leds);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	return rc;

free_err:
	aw2026_led_err_handle(led_array, parsed_leds);
	return rc;
}

static int aw2026_led_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct aw2026_led *led_array;
	struct device_node *node;
	int ret = -EINVAL, num_leds = 0;

	node = client->dev.of_node;
	if (node == NULL)
		return -EINVAL;

	num_leds = of_get_child_count(node);

	if (!num_leds)
		return -EINVAL;

	led_array = devm_kzalloc(&client->dev,
			(sizeof(struct aw2026_led) * num_leds), GFP_KERNEL);
	if (!led_array)
		return -ENOMEM;

	led_array->client = client;
	led_array->num_leds = num_leds;

	led_array->regmap = devm_regmap_init_i2c(client, &aw2026_regmap);
	if (IS_ERR(led_array->regmap)) {
		ret = PTR_ERR(led_array->regmap);
		dev_err(&client->dev, "Failed to set up AW2026 register map, ret=%d\n", ret);
		goto free_led_array;
	}

	i2c_set_clientdata(client, led_array);

	mutex_init(&led_array->lock);

	ret = aw2026_led_parse_child_node(led_array, node);
	if (ret) {
		dev_err(&client->dev, "parsed node error\n");
		goto free_led_array;
	}

	ret = aw2026_check_chipid(led_array);
	if (ret) {
		dev_err(&client->dev, "Check chip id error\n");
		goto fail_parsed_node;
	}

	aw2026_soft_reset(led_array);

	return 0;

fail_parsed_node:
	aw2026_led_err_handle(led_array, num_leds);
free_led_array:
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return ret;
}

static int aw2026_led_remove(struct i2c_client *client)
{
	struct aw2026_led *led_array = i2c_get_clientdata(client);
	int i, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2026_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return 0;
}

static void aw2026_led_shutdown(struct i2c_client *client)
{
	struct aw2026_led *led = i2c_get_clientdata(client);

	aw2026_write(led, AW2026_REG_GCR, AW2026_LED_CHIP_DISABLE);
}

static const struct i2c_device_id aw2026_led_id[] = {
	{"aw2026_led", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2026_led_id);

static const struct of_device_id aw2026_match_table[] = {
	{ .compatible = "awinic,aw2026_led",},
	{ },
};

static struct i2c_driver aw2026_led_driver = {
	.probe = aw2026_led_probe,
	.remove = aw2026_led_remove,
	.shutdown = aw2026_led_shutdown,
	.driver = {
		.name = "aw2026_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw2026_match_table),
	},
	.id_table = aw2026_led_id,
};

static int __init aw2026_led_init(void)
{
	return i2c_add_driver(&aw2026_led_driver);
}
module_init(aw2026_led_init);

static void __exit aw2026_led_exit(void)
{
	i2c_del_driver(&aw2026_led_driver);
}
module_exit(aw2026_led_exit);

MODULE_DESCRIPTION("AWINIC aw2026 LED driver");
MODULE_LICENSE("GPL v2");

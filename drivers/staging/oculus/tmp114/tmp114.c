// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.
// tmp114 is a local temperature sensor with I2C/Smbus interface

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>

#include "tmp114.h"

#define REGMAP_READ_MAX_RETRIES     5
#define REGMAP_READ_DELAY_MS        150
#define MAX_INT_DIGITS_LEN          20 // Max number of digits for 64 bit integer

static struct class *tmp114_class = NULL;
static int probed_devices = 0;

static int LSB_VALUE = 128;

static const struct regmap_config tmp114_regmap_config = {
    .reg_bits   = 8,
    .val_bits   = 16,
    .val_format_endian = REGMAP_ENDIAN_BIG,
    .cache_type = REGCACHE_NONE,
};

static int tmp114_i2c_read(struct regmap *regmap, u8 reg_addr, u16 *data)
{
    int ret = 0;
    int retry = 0;
    unsigned int buf = 0;

    ret = regmap_read(regmap, (unsigned int)reg_addr, &buf);
    while (ret < 0 && retry < REGMAP_READ_MAX_RETRIES) {
        pr_warn("<%s>: regmap_read failed with error %d\n", __func__, ret);
        msleep(REGMAP_READ_DELAY_MS);
        ret = regmap_read(regmap, (unsigned int)reg_addr, &buf);
        retry++;
    }

    if (ret < 0) {
        pr_err("<%s>: regmap_read failed with error %d after retries\n", __func__, ret);
        return ret;
    }

    *data = (u16)buf;
    return ret;
}

static int tmp114_read_temp(struct regmap *regmap, int *temp)
{
    u16 temp_bytes;
    int ret;

    ret = tmp114_i2c_read(regmap, REG_TEMP, &temp_bytes);
    if (ret < 0)
        return ret;

    *temp = sign_extend32(temp_bytes, 15);

    // convert to milli celsius
    *temp = (*temp) * 1000 / LSB_VALUE;

    return 0;
}


static int tmp114_open(struct inode *inode, struct file *filep)
{
    struct tmp114_dev *tmp114 = NULL;

    tmp114 = container_of(inode->i_cdev, struct tmp114_dev, cdev);
    if (tmp114 == NULL) {
        pr_err("<%s>: Failed to retrieve tmp114_dev from inode\n", __func__);
        return -ENODEV;
    }

    filep->private_data = tmp114;
    return 0;
}

static int tmp114_release(struct inode *inode, struct file *filep)
{
    return 0;
}

static ssize_t tmp114_read(struct file *filep, char __user *buf, size_t count, loff_t *fpos)
{
    struct tmp114_dev *tmp114;
    ssize_t ret = 0, temp_size;
    int temp = 0;
    char temp_mcelsius[MAX_INT_DIGITS_LEN];

    // Don't support partial reading of the temperature
    if (*fpos > 0) {
        return 0;
    }

    tmp114 = filep->private_data;
    if (mutex_lock_killable(&tmp114->mutex))
        return -EINTR;

    temp_size = sizeof(temp_mcelsius);
    // User buffer does not have space to store the temperature value
    if (count < temp_size) {
        ret = -EINVAL;
        goto exit_read;
    }

    ret = tmp114_read_temp(tmp114->regmap, &temp);
    if (ret < 0)
        goto exit_read;

    snprintf(temp_mcelsius, temp_size, "%d", temp);
    ret = copy_to_user(buf, temp_mcelsius, temp_size);
    if (ret < 0)
        goto exit_read;

    ret = temp_size;
    *fpos += temp_size;

exit_read:
    mutex_unlock(&tmp114->mutex);
    return ret;
}


static const struct file_operations tmp114_fops = {
    .owner = THIS_MODULE,
    .open = tmp114_open,
    .release = tmp114_release,
    .read = tmp114_read
};


static int tmp114_thermal_get_temp(void *data, int *state)
{
    struct tmp114_dev *tmp114 = (struct tmp114_dev *) data;

	if (!tmp114 || !tmp114->tz || !tmp114->regmap)
		return -EINVAL;

    // Return 0 deg C when disabled to prevent thermal service from failing due to a single sensor returning error
    // TODO: Implement long term solution to return a valid temperature when sensor is disabled
	if (!atomic_read(&tmp114->is_enabled) || tmp114->tz->mode == THERMAL_DEVICE_DISABLED) {
		*state = 0;
        return 0;
    }

    if (mutex_lock_killable(&tmp114->mutex))
        return -EINTR;

    tmp114_read_temp(tmp114->regmap, state);

    mutex_unlock(&tmp114->mutex);
    return 0;
}

static const struct thermal_zone_of_device_ops tmp114_thermal_ops = {
	.get_temp	= tmp114_thermal_get_temp,
};

static void tmp114_enable(struct tmp114_dev *tmp114)
{
    u16 temp_bytes;

    // Check if tmp114 is healthy before enabling
    int ret = tmp114_i2c_read(tmp114->regmap, REG_TEMP, &temp_bytes);

    if (ret != 0) {
        pr_err("<%s>: Not enabling thermal zone since failed to read tmp114 when PMIC is enabled: %d.\n", __func__, ret);
        return;
    }

    atomic_set(&tmp114->is_enabled, 1);
    thermal_zone_device_enable(tmp114->tz);
}

static void tmp114_disable(struct tmp114_dev *tmp114)
{
    thermal_zone_device_disable(tmp114->tz);
    atomic_set(&tmp114->is_enabled, 0);
}

static int tmp114_pmic_notify(struct notifier_block *nb,
                unsigned long event, void *data)
{
    struct tmp114_dev *tmp114 = container_of(nb, struct tmp114_dev, pmic_nb);

    if (event & REGULATOR_EVENT_ENABLE) {
        tmp114_enable(tmp114);
    } else if (event & REGULATOR_EVENT_DISABLE) {
        tmp114_disable(tmp114);
    }

    return 0;
}

static const struct of_device_id tmp114_of_match[] = {
    { .compatible = "meta,tmp114-i2c"},
    { }
};
MODULE_DEVICE_TABLE(of, tmp114_of_match);

static int get_pmic_regulator(struct device *dev, struct regulator **reg)
{
    int ret;
    char *pmic_name;

    ret = of_property_read_string(dev->of_node, "pmic-supply", (const char**) &pmic_name);
    if (ret) {
        pr_debug("<%s>: Found no pmic-supply property\n", __func__);
        return -EINVAL;
    }

    *reg = devm_regulator_get(dev, pmic_name);
    if (IS_ERR(*reg)) {
        pr_debug("<%s>: Failed to get pmic-supply. Deferring probe.\n", __func__);
        return -EPROBE_DEFER;
    }

    return 0;
}

static int tmp114_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct tmp114_dev *tmp114 = NULL;
    struct device *device = NULL;
    dev_t dev;
    struct regulator *pmic_reg = NULL;

    ret = get_pmic_regulator(&client->dev, &pmic_reg);
    if (ret) {
        return ret;
    }

    tmp114 = kzalloc(sizeof(struct tmp114_dev), GFP_KERNEL);
    if (!tmp114)
        return -ENOMEM;
    mutex_init(&tmp114->mutex);
    tmp114->client = client;

    tmp114->regmap = devm_regmap_init_i2c(client, &tmp114_regmap_config);
    if (IS_ERR(tmp114->regmap)) {
        ret = PTR_ERR(tmp114->regmap);
        pr_err("<%s>: Error %d in initializing regmap\n", __func__, ret);
        goto error;
    }

    tmp114->pmic = pmic_reg;

    tmp114->pmic_nb.notifier_call = tmp114_pmic_notify;
    ret = devm_regulator_register_notifier(pmic_reg, &tmp114->pmic_nb);
    if (ret) {
        pr_err("<%s>: Error %d in registering notifier for pmic\n", __func__, ret);
        goto error;
    }

    tmp114->tz = devm_thermal_zone_of_sensor_register(&client->dev, 0,
                    tmp114, &tmp114_thermal_ops);
    if (IS_ERR(tmp114->tz)) {
        pr_err("<%s>: Error %ld in registering thermal zone device\n", __func__, PTR_ERR(tmp114->tz));
        ret = PTR_ERR(tmp114->tz);
        goto error;
    }

    // Disable the TZ by default. Check if PMIC is enabled and the temperature sensor is healthy before enabling it.
    thermal_zone_device_disable(tmp114->tz);
    atomic_set(&tmp114->is_enabled, 0);

    if (regulator_is_enabled(pmic_reg)) {
        tmp114_enable(tmp114);
    }

    ret = alloc_chrdev_region(&dev, MAJOR_NUM, 1, CLASS_NAME);
    if (ret < 0) {
        pr_err("<%s>: Error %d in allocating chrdev region\n", __func__, ret);
        goto error;
    }

    cdev_init(&tmp114->cdev, &tmp114_fops);
    tmp114->cdev.owner = THIS_MODULE;

    device = device_create(tmp114_class, NULL, dev,
                NULL, DEVICE_NAME "_%d", probed_devices);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        pr_err("<%s>: Error %d in creating device\n", __func__, ret);
        goto error_device_create;
    }
    probed_devices++;

    ret = cdev_add(&tmp114->cdev, dev, 1);
    if (ret < 0) {
        pr_err("<%s>: Error %d when adding cdev\n", __func__, ret);
        goto error_cdev_add;
    }

    i2c_set_clientdata(client, tmp114);
    pr_info("<%s>: Initialized tmp114\n", __func__);

    return 0;

error_cdev_add:
    device_destroy(tmp114_class, dev);
error_device_create:
    unregister_chrdev_region(dev, 1);
error:
    kfree(tmp114);

    return ret;
}

static int tmp114_remove(struct i2c_client *client)
{
    struct tmp114_dev *tmp114;
    dev_t devt;

    tmp114 = i2c_get_clientdata(client);
    devt = tmp114->cdev.dev;

    cdev_del(&tmp114->cdev);
    device_destroy(tmp114_class, devt);
    unregister_chrdev_region(devt, 1);
    kfree(tmp114);

    return 0;
}

static const struct i2c_device_id tmp114_id_table[] = {
    { DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, tmp114_id_table);

static struct i2c_driver tmp114_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = of_match_ptr(tmp114_of_match),
    },
    .probe = tmp114_probe,
    .remove = tmp114_remove,
    .id_table = tmp114_id_table,
};

static int __init tmp114_init(void)
{
    int status;

    tmp114_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(tmp114_class))
        return PTR_ERR(tmp114_class);

    status = i2c_register_driver(THIS_MODULE, &tmp114_driver);
    if (status < 0)
        class_destroy(tmp114_class);

    return status;
}
module_init(tmp114_init);

static void __exit tmp114_exit(void)
{
    i2c_del_driver(&tmp114_driver);
    class_destroy(tmp114_class);
}
module_exit(tmp114_exit);

MODULE_DESCRIPTION("TMP114 I2C driver");
MODULE_LICENSE("GPL");

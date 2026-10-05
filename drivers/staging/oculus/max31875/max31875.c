// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.
// MAX31875 is a local temperature sensor with I2C/Smbus interface

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

#include "max31875.h"

#define REGMAP_READ_MAX_RETRIES	5
#define REGMAP_READ_DELAY_MS	150

static struct class *max31875_class = NULL;
static int probed_devices = 0;

static int BIT_RESOLUTIONS[4] = {8, 9, 10, 12};
static int LSB_VALUES[4] = {1, 2, 4, 16};

static const struct regmap_config max31875_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.cache_type = REGCACHE_NONE,
};

static int max31875_i2c_read(struct regmap *regmap, u8 reg_addr, u16 *data)
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

static int max31875_read_temp(struct regmap *regmap, int *temp)
{
	u16 temp_bytes, config_bytes;
	u8 resolution_sel, data_format, first_used_bit;
	int ret;

	ret = max31875_i2c_read(regmap, REG_TEMP, &temp_bytes);
	if (ret < 0)
		return ret;

	ret = max31875_i2c_read(regmap, REG_CONFIG, &config_bytes);
	if (ret < 0)
		return ret;

	resolution_sel = (config_bytes >> 5) & 3;
	data_format = (config_bytes >> 7) & 1;

	/**
	 * Temperature register has different resolution settings which determine how many bits
	 * are used. We need to remove the unused bits (at the end) to convert the actual
	 * temperature value. If data_format is 1, it indicates extended format which means can
	 * read temperature values > 128 C. This adds an extra bit in the temperature value
	 * storage. See below for the register definition:
	 *
	 * Temperature Register Definition (value for each bit):
	 *
	 *	     | D15 | D14 | D13 | D12 | D11 | D10 | D9 | D8 |
	 * Extended: | Sign| 128 | 64  | 32  | 16  | 8   | 4  | 2  |
	 * Normal:   | Sign| 64  | 32  | 16  | 8   | 4   | 2  | 1  |
	 *
	 *	     | D7 | D6 | D5  | D4   | D3   | D2 | D1 | D0 |
	 * Extended: | 1  | .5 | .25 | .125 | .0625| 0  | 0  | 0  |
	 * Normal:   | .5 | .25| .125| .0625| 0    | 0  | 0  | 0  |
	 */

	first_used_bit = 16 - (BIT_RESOLUTIONS[resolution_sel] + data_format);

	// shift to remove unused bits and convert to integer
	*temp = (sign_extend32(temp_bytes, 15) >> first_used_bit);

	// convert to milli celsius
	*temp = (*temp) * 1000 / LSB_VALUES[resolution_sel];

	return 0;
}


static int max31875_open(struct inode *inode, struct file *filep)
{
	struct max31875_dev *max31875 = NULL;

	max31875 = container_of(inode->i_cdev, struct max31875_dev, cdev);
	if (max31875 == NULL) {
		pr_err("<%s>: Failed to retrieve max31875_dev from inode\n", __func__);
		return -ENODEV;
	}

	filep->private_data = max31875;
	return 0;
}

static int max31875_release(struct inode *inode, struct file *filep)
{
	return 0;
}

static ssize_t max31875_read(struct file *filep, char __user *buf, size_t count, loff_t *fpos)
{
	struct max31875_dev *max31875;
	ssize_t ret = 0, temp_size;
	int temp = 0;

	// Don't support partial reading of the temperature
	if (*fpos > 0)
		return 0;

	max31875 = filep->private_data;
	if (mutex_lock_killable(&max31875->mutex))
		return -EINTR;

	// User buffer does not have space to store the temperature value
	if (count < sizeof(max31875->temp_mcelsius)) {
		ret = -EINVAL;
		goto exit_read;
	}

	temp_size = sizeof(max31875->temp_mcelsius);

	ret = max31875_read_temp(max31875->regmap, &temp);
	if (ret < 0)
		goto exit_read;

	snprintf(max31875->temp_mcelsius, temp_size, "%d", temp);
	ret = copy_to_user(buf, max31875->temp_mcelsius, temp_size);
	if (ret < 0)
		goto exit_read;

	ret = temp_size;
	*fpos += temp_size;

exit_read:
	mutex_unlock(&max31875->mutex);
	return ret;
}


static const struct file_operations max31875_fops = {
	.owner = THIS_MODULE,
	.open = max31875_open,
	.release = max31875_release,
	.read = max31875_read
};


static int max31875_thermal_get_temp(void *data, int *state)
{
	struct max31875_dev *max31875 = (struct max31875_dev *) data;

	if (!max31875 || !max31875->tz || !max31875->regmap)
		return -EINVAL;

	/**
	 * Return 0 deg C when disabled to prevent thermal service from failing due to a single
	 * sensor returning error.
	 * TODO: Implement long term solution to return a valid temperature when sensor is disabled
	 */
	if (!atomic_read(&max31875->is_enabled) || max31875->tz->mode == THERMAL_DEVICE_DISABLED) {
		*state = 0;
		return 0;
	}

	if (mutex_lock_killable(&max31875->mutex))
		return -EINTR;

	max31875_read_temp(max31875->regmap, state);

	mutex_unlock(&max31875->mutex);
	return 0;
}

static const struct thermal_zone_of_device_ops max31875_thermal_ops = {
	.get_temp = max31875_thermal_get_temp,
};

static void max31875_enable(struct max31875_dev *max31875)
{
	u16 temp_bytes;

	// Check if MAX31875 is healthy before enabling
	int ret = max31875_i2c_read(max31875->regmap, REG_TEMP, &temp_bytes);

	if (ret != 0) {
		pr_err("<%s>: Not enabling thermal zone since reading temp failed: %d.\n",
		       __func__, ret);
		return;
	}

	atomic_set(&max31875->is_enabled, 1);
	thermal_zone_device_enable(max31875->tz);
}

static void max31875_disable(struct max31875_dev *max31875)
{
	thermal_zone_device_disable(max31875->tz);
	atomic_set(&max31875->is_enabled, 0);
}

static int max31875_pmic_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct max31875_dev *max31875 = container_of(nb, struct max31875_dev, pmic_nb);

	if (event & REGULATOR_EVENT_ENABLE)
		max31875_enable(max31875);
	else if (event & REGULATOR_EVENT_DISABLE)
		max31875_disable(max31875);

	return 0;
}

static const struct of_device_id max31875_of_match[] = {
	{ .compatible = "meta,max31875-i2c"},
	{ }
};
MODULE_DEVICE_TABLE(of, max31875_of_match);

static int get_pmic_regulator_by_name(struct device *dev, struct regulator **reg)
{
	int ret;
	char *pmic_name;

	ret = of_property_read_string(dev->of_node, "pmic-supply", (const char **) &pmic_name);
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

static int get_pmic_regulator_by_phandle(struct device *dev, struct regulator **reg)
{
	int ret;

	*reg = devm_regulator_get_optional(dev, "pmic");
	if (IS_ERR(*reg) && PTR_ERR(*reg) != -ENODEV) {
		ret = PTR_ERR(*reg);
		if (ret == -EPROBE_DEFER)
			pr_warn("<%s>: Failed to get pmic-supply. Deferring probe.\n", __func__);
		else
			pr_err("<%s>: Error %d when getting pmic-supply\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int max31875_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct max31875_dev *max31875 = NULL;
	struct device *device = NULL;
	dev_t dev;
	struct regulator *pmic_reg = NULL;
	struct regulator *vdd_reg = NULL;
	u32 rate_value = 0;
	bool is_no_always_on = false;

	ret = get_pmic_regulator_by_name(&client->dev, &pmic_reg);
	if (ret == -EINVAL)
		ret = get_pmic_regulator_by_phandle(&client->dev, &pmic_reg);
	if (ret)
		return ret;

	vdd_reg = devm_regulator_get_optional(&client->dev, "vdd");
	if (!IS_ERR_OR_NULL(vdd_reg)) {
		ret = regulator_enable(vdd_reg);
		if (ret) {
			pr_err("<%s>: Error %d when enabling vdd regulator\n", __func__, ret);
			return ret;
		}
	}

	max31875 = kzalloc(sizeof(struct max31875_dev), GFP_KERNEL);
	if (!max31875)
		return -ENOMEM;
	mutex_init(&max31875->mutex);
	max31875->client = client;

	max31875->temp_mcelsius = kzalloc(sizeof(int), GFP_KERNEL);
	if (!max31875->temp_mcelsius)
		return -ENOMEM;

	max31875->regmap = devm_regmap_init_i2c(client, &max31875_regmap_config);
	if (IS_ERR(max31875->regmap)) {
		ret = PTR_ERR(max31875->regmap);
		pr_err("<%s>: Error %d in initializing regmap\n", __func__, ret);
		goto error;
	}
	/**
	* The conversion rate bits, D2:D1, select the rate for automatic continuous conversions,
	* rates of 0.25sps(00), 1sps(01),4sps(10), and 8sps(11) are available.
	*/
	ret = of_property_read_u32(max31875->client->dev.of_node, "conver-rate-select", &rate_value);
	if (ret)
		pr_info("%s:conver-rate-select property not found\n", __func__);
	else {
		rate_value = rate_value << 1;
		ret = regmap_update_bits(max31875->regmap, REG_CONFIG, CONVERSION_RATE_MASK, rate_value);
		if (ret < 0) {
			pr_err("%s:Failed to update REG_CONFIG bit2&1 conversion rate\n", __func__);
			goto error;
		}
	}
	/**
	* If the supply for the temp sensor is controled with other modules,
	* need to disable supply  after the temp sensor configuration are complete.
	*/
	is_no_always_on = of_property_read_bool(max31875->client->dev.of_node, "no-always-power-on");

	if (!IS_ERR(pmic_reg)) {
		max31875->pmic = pmic_reg;

		max31875->pmic_nb.notifier_call = max31875_pmic_notify;
		ret = devm_regulator_register_notifier(pmic_reg, &max31875->pmic_nb);
		if (ret) {
			pr_err("<%s>: Error %d in registering notifier for pmic\n", __func__, ret);
			goto error;
		}
	}

	max31875->tz = devm_thermal_zone_of_sensor_register(&client->dev, 0, max31875,
							    &max31875_thermal_ops);
	if (IS_ERR(max31875->tz)) {
		pr_err("<%s>: Error %ld in registering thermal zone device\n", __func__,
		       PTR_ERR(max31875->tz));
		ret = PTR_ERR(max31875->tz);
		goto error;
	}

	/**
	 * Disable the TZ by default. Check if PMIC is enabled and the temperature sensor is
	 * healthy before enabling it.
	 */
	if (!IS_ERR(pmic_reg)) {
		thermal_zone_device_disable(max31875->tz);
		atomic_set(&max31875->is_enabled, 0);

		if (regulator_is_enabled(pmic_reg))
			max31875_enable(max31875);
	} else {
		max31875_enable(max31875);
	}

	ret = alloc_chrdev_region(&dev, MAJOR_NUM, 1, CLASS_NAME);
	if (ret < 0) {
		pr_err("<%s>: Error %d in allocating chrdev region\n", __func__, ret);
		goto error;
	}

	cdev_init(&max31875->cdev, &max31875_fops);
	max31875->cdev.owner = THIS_MODULE;

	device = device_create(max31875_class, NULL, dev,
			       NULL, DEVICE_NAME "_%d", probed_devices);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("<%s>: Error %d in creating device\n", __func__, ret);
		goto error_device_create;
	}
	probed_devices++;

	ret = cdev_add(&max31875->cdev, dev, 1);
	if (ret < 0) {
		pr_err("<%s>: Error %d when adding cdev\n", __func__, ret);
		goto error_cdev_add;
	}

	if (is_no_always_on && !IS_ERR_OR_NULL(vdd_reg))
		regulator_disable(vdd_reg);

	i2c_set_clientdata(client, max31875);
	pr_info("<%s>: Initialized MAX31875\n", __func__);

	return 0;

error_cdev_add:
	device_destroy(max31875_class, dev);
error_device_create:
	unregister_chrdev_region(dev, 1);
error:
	kfree(max31875);
	if (is_no_always_on && !IS_ERR_OR_NULL(vdd_reg))
		regulator_disable(vdd_reg);

	return ret;
}

static int max31875_remove(struct i2c_client *client)
{
	struct max31875_dev *max31875;
	dev_t devt;

	max31875 = i2c_get_clientdata(client);
	devt = max31875->cdev.dev;

	cdev_del(&max31875->cdev);
	device_destroy(max31875_class, devt);
	unregister_chrdev_region(devt, 1);
	kfree(max31875->temp_mcelsius);
	kfree(max31875);

	return 0;
}

static const struct i2c_device_id max31875_id_table[] = {
	{ DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max31875_id_table);

static struct i2c_driver max31875_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(max31875_of_match),
	},
	.probe = max31875_probe,
	.remove = max31875_remove,
	.id_table = max31875_id_table,
};

static int __init max31875_init(void)
{
	int status;

	max31875_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(max31875_class))
		return PTR_ERR(max31875_class);

	status = i2c_register_driver(THIS_MODULE, &max31875_driver);
	if (status < 0)
		class_destroy(max31875_class);

	return status;
}
module_init(max31875_init);

static void __exit max31875_exit(void)
{
	i2c_del_driver(&max31875_driver);
	class_destroy(max31875_class);
}
module_exit(max31875_exit);

MODULE_DESCRIPTION("MAX31875 I2C driver");
MODULE_LICENSE("GPL");

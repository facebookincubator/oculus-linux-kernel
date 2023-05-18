
/*******************************************************************************
 * @file max31875.c
 *
 * @version 0.1
 *
 * @brief Driver for the temperature sensor MAX31875
 *
 * @details Controls the MAX31875 temperature sensor via i2c
 *
 * @author Silviu Popescu
 *
 *******************************************************************************
 * Copyright (C) 2019 Facebook, Inc. and its affilates. All Rights reserved.
 ******************************************************************************/

#include "max31875.h"

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/regmap.h>
#include <linux/slab.h>


/*******************************************************************************
 * Local Constants Definition
 ******************************************************************************/
#define	DRIVER_NAME "max31875"

struct max31875
{
	struct regmap *regmap;
	u16 orig_config;
	unsigned long ready_time;
};


/* convert 12-bit MAX31875 register value to milliCelsius */
/* Use only the normal format for now.
*  The extended format will need a different function.
*/
static inline int max31875_temp_reg_to_mC(s16 val)
{
	return (val & ~0x0f) * 1000 / 256;
}

/* convert milliCelsius to left adjusted 12-bit MAX31875 register value */
/* Use only the normal format for now.
*  The extended format will need a different function.
*/
static inline u16 max31875_mC_to_temp_reg(int val)
{
	return (val * 256) / 1000;
}

static int max31875_read (struct device *dev, enum hwmon_sensor_types type,
		       				u32 attr, int channel, long *temp)
{
	struct max31875 *max31875 = dev_get_drvdata(dev);
	unsigned int regval;
	int err;

	if (type == hwmon_chip)
	{
		if (attr == hwmon_chip_update_interval)
		{
			err = regmap_read(max31875->regmap,
							MAX31875_REG_CONFIGURATION,
					  		&regval);
			if (err < 0)
				return err;

			switch (regval & MAX31875_CFG_CONV_RATE_MASK)
			{
				case MAX31875_CFG_CONV_RATE_0_25:
					*temp = 4000;
					break;

				case MAX31875_CFG_CONV_RATE_1:
					*temp = 1000;
					break;

				case MAX31875_CFG_CONV_RATE_4:
					*temp = 250;
					break;

				case MAX31875_CFG_CONV_RATE_8:
					*temp = 125;
					break;

				default:
					*temp = 4000;
					break;

			}
			return 0;
		}
		return -EOPNOTSUPP;
	}

	switch (attr)
	{
	case hwmon_temp_input:
		/* Is it too early to return a conversion ? */
		if (time_before(jiffies, max31875->ready_time)) {
			dev_dbg(dev, "%s: Conversion not ready yet...\n", __func__);
			return -EAGAIN;
		}
		err = regmap_read(max31875->regmap, MAX31875_REG_TEMPERATURE, &regval);
		if (err < 0)
			return err;
		*temp = max31875_temp_reg_to_mC(regval);
		break;

	case hwmon_temp_max:
		err = regmap_read(max31875->regmap, MAX31875_REG_TOS_HIGH_TRIP, &regval);
		if (err < 0)
			return err;
		*temp = max31875_temp_reg_to_mC(regval);
		break;

	case hwmon_temp_max_alarm:
		err = regmap_read(max31875->regmap, MAX31875_REG_CONFIGURATION, &regval);
		if (err < 0)
			return err;
		*temp = ((regval & MAX31875_CFG_OVERTEMP_MASK) ==
				MAX31875_CFG_OVERTEMP_ALARM) ? 1 : 0;
		break;

	case hwmon_temp_max_hyst:
		err = regmap_read(max31875->regmap, MAX31875_REG_THYST_LOW_TRIP, &regval);
		if (err < 0)
			return err;
		*temp = max31875_temp_reg_to_mC(regval);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}


static int max31875_write (struct device *dev, enum hwmon_sensor_types type,
						u32 attr, int channel, long temp)
{
	struct max31875 *max31875 = dev_get_drvdata(dev);
	u32 mask = 0;

	if (type == hwmon_chip)
	{
		if (attr == hwmon_chip_update_interval)
		{
			int status = 0;
			if (temp < 250)
				mask = MAX31875_CFG_CONV_RATE_8;
			else if (temp < 1000)
				mask = MAX31875_CFG_CONV_RATE_4;
			else if (temp < 4000)
				mask = MAX31875_CFG_CONV_RATE_1;
			else
				mask = MAX31875_CFG_CONV_RATE_0_25;

			status = regmap_update_bits(max31875->regmap,
						MAX31875_REG_CONFIGURATION,
						MAX31875_CFG_CONV_RATE_MASK,
						mask);

			max31875->ready_time = jiffies;
			max31875->ready_time +=
				msecs_to_jiffies(MAX31875_CONVERSION_TIME_FT_MS);

			return status;
		}
		return -EOPNOTSUPP;
	}

	switch (attr)
	{
		case hwmon_temp_max:
			temp = clamp_val(temp, MAX31875_TEMP_MIN_MC, MAX31875_TEMP_MAX_MC);
			return regmap_write(max31875->regmap,
						MAX31875_REG_TOS_HIGH_TRIP,
						max31875_mC_to_temp_reg(temp));

		case hwmon_temp_max_hyst:
			temp = clamp_val(temp, MAX31875_TEMP_MIN_MC, MAX31875_TEMP_MAX_MC);
			return regmap_write(max31875->regmap,
						MAX31875_REG_THYST_LOW_TRIP,
						max31875_mC_to_temp_reg(temp));

		default:
			return -EOPNOTSUPP;
	}
}

static umode_t max31875_is_visible (const void *data, enum hwmon_sensor_types type,
				                    u32 attr, int channel)
{
	if (type == hwmon_chip && attr == hwmon_chip_update_interval)
		return 0644;

	if (type != hwmon_temp)
		return 0;

	switch (attr)
	{
		case hwmon_temp_input:
		case hwmon_temp_max_alarm:
			return 0444;

		case hwmon_temp_max:
		case hwmon_temp_max_hyst:
			return 0644;

		default:
			return 0;
	}
}

static u32 max31875_chip_config[] = {
	HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL,
	0
};

static const struct hwmon_channel_info max31875_chip = {
	.type = hwmon_chip,
	.config = max31875_chip_config,
};

static u32 max31875_temp_config[] = {
	HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_MAX_HYST | HWMON_T_MAX_ALARM,
	0
};

static const struct hwmon_channel_info max31875_temp = {
	.type = hwmon_temp,
	.config = max31875_temp_config,
};

static const struct hwmon_channel_info *max31875_info[] = {
	&max31875_chip,
	&max31875_temp,
	NULL
};

static const struct hwmon_ops max31875_hwmon_ops = {
	.is_visible = max31875_is_visible,
	.read = max31875_read,
	.write = max31875_write,
};

static const struct hwmon_chip_info max31875_chip_info = {
	.ops = &max31875_hwmon_ops,
	.info = max31875_info,
};

static void max31875_restore_config(void *data)
{
	struct max31875 *max31875 = data;

	regmap_write(max31875->regmap, MAX31875_REG_CONFIGURATION, max31875->orig_config);
}

static bool max31875_is_writeable_reg(struct device *dev, unsigned int reg)
{
	return reg != MAX31875_REG_TEMPERATURE;
}

static bool max31875_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* Configuration register must be volatile */
	return (reg == MAX31875_REG_TEMPERATURE) || (reg == MAX31875_REG_CONFIGURATION);
}

static const struct regmap_config max31875_regmap_config = {
	.name = "max31875_regmap",
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = MAX31875_REG_MAX,
	.writeable_reg = max31875_is_writeable_reg,
	.volatile_reg = max31875_is_volatile_reg,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.cache_type = REGCACHE_RBTREE,
	.use_single_rw = true,
};

/*
* Get sensor temperature conversion time in ms.
*/
static int max31875_get_conversion_time (u32 config)
{
	u32 resolution = 0;
	int conv_time = 0;

	resolution = config & MAX31875_CFG_RESOLUTION_MASK;
	switch (resolution)
	{
		case MAX31875_CFG_RESOLUTION_8BIT:
			conv_time = MAX31875_CONVERSION_TIME_MS;
			break;

		case MAX31875_CFG_RESOLUTION_9BIT:
			conv_time = MAX31875_CONVERSION_TIME_MS;
			break;

		case MAX31875_CFG_RESOLUTION_10BIT:
			conv_time = MAX31875_CONVERSION_TIME_MS * 2;
			break;

		case MAX31875_CFG_RESOLUTION_12BIT:
			conv_time = MAX31875_CONVERSION_TIME_MS * 4;
			break;

		default:
			conv_time = MAX31875_CONVERSION_TIME_FT_MS;
			break;
	}
	return conv_time;
}

static int max31875_probe (struct i2c_client *client,
			                const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device *hwmon_dev;
	struct max31875 *max31875;
	int err;
	u32 config;

	if (!i2c_check_functionality(client->adapter,
				                I2C_FUNC_SMBUS_WORD_DATA))
    {
		dev_err(dev, "adapter doesn't support SMBus word transactions\n");
		return -ENODEV;
	}

	max31875 = devm_kzalloc(dev, sizeof(*max31875), GFP_KERNEL);
	if (!max31875)
	{
		dev_err(dev, "cannot allocate max31875 memory \n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, max31875);

	max31875->regmap = devm_regmap_init_i2c(client, &max31875_regmap_config);
	if (IS_ERR(max31875->regmap))
    {
		err = PTR_ERR(max31875->regmap);
		dev_err(dev, "regmap init failed: %d", err);
		return err;
	}

	err = regmap_read(max31875->regmap, MAX31875_REG_CONFIGURATION, &config);
	if (err < 0)
	{
		dev_err(dev, "error reading config register: %d", err);
		return err;
	}
	else
	{
		dev_info(dev, "reading config register, POR value: 0x%.4X", config);
	}
	max31875->orig_config = MAX31875_CFG_DEFAULT;

	/* set the default configuration */
	config = MAX31875_CFG_DEFAULT;

	err = regmap_write(max31875->regmap, MAX31875_REG_CONFIGURATION, config);
	if (err < 0)
	{
		dev_err(dev, "error writing config register: %d", err);
		return err;
	}

	max31875->ready_time = jiffies;
	if ((max31875->orig_config & MAX31875_CFG_OPMODE_MASK) ==
	    MAX31875_CFG_OPMODE_SHUTDOWN)
	{
		max31875->ready_time +=
			msecs_to_jiffies(MAX31875_CONVERSION_TIME_FT_MS);
	}
	else
	{
		max31875->ready_time +=
			msecs_to_jiffies(max31875_get_conversion_time(config));
	}

	err = devm_add_action_or_reset(dev, max31875_restore_config, max31875);
	if (err)
	{
		dev_err(dev, "add action or reset failed: %d", err);
		return err;
	}

	hwmon_dev = devm_hwmon_device_register_with_info(dev, client->name,
							 max31875,
							 &max31875_chip_info,
							 NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}


static int __maybe_unused max31875_suspend (struct device *dev)
{
	struct max31875 *max31875 = dev_get_drvdata(dev);
	int err;

	err = regmap_update_bits(max31875->regmap,
                             MAX31875_REG_CONFIGURATION,
				             MAX31875_CFG_OPMODE_MASK,
                             MAX31875_CFG_OPMODE_SHUTDOWN);
	return err;
}

static int __maybe_unused max31875_resume(struct device *dev)
{
	struct max31875 *max31875 = dev_get_drvdata(dev);
	int err;

	err = regmap_update_bits(max31875->regmap,
                            MAX31875_REG_CONFIGURATION,
				            MAX31875_CFG_OPMODE_MASK,
                            MAX31875_CFG_OPMODE_CONTINUOUS);

	max31875->ready_time = jiffies +
                    msecs_to_jiffies(MAX31875_CONVERSION_TIME_FT_MS);

	return err;
}


static SIMPLE_DEV_PM_OPS(max31875_dev_pm_ops, max31875_suspend, max31875_resume);

static const struct i2c_device_id max31875_i2c_ids[] = {
	{ "max31875", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, max31875_i2c_ids);


#ifdef CONFIG_OF
static const struct of_device_id max31875_of_ids[] = {
	{ .compatible = "maxim,max31875", },
	{},
};

MODULE_DEVICE_TABLE(of, max31875_of_ids);
#endif


static struct i2c_driver max31875_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = DRIVER_NAME,
		.pm	= &max31875_dev_pm_ops,
		.of_match_table = of_match_ptr(max31875_of_ids),
	},
	.probe		= max31875_probe,
	.id_table	= max31875_i2c_ids,
};

module_i2c_driver(max31875_driver);

MODULE_DESCRIPTION("MAXIM-MAX31875 temperature sensor driver");
MODULE_LICENSE("GPL");

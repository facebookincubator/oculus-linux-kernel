// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bits.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/thermal.h>

#define DRIVER_NAME	"sgm458"

/* chip registers */
#define SGM458_TEMP_MSB_REG	0x00	/* R: MSB Temperature Register */
#define SGM458_CONF_REG		0x01	/* R/W: Configuration Register */
#define SGM458_TLOW_REG		0x02	/* R/W: Low Limit */
#define SGM458_THIGH_REG	0x03	/* R/W: High Limit */
#define SGM458_TEMP_LSB_REG	0x04	/* R: LSB Temperature Register */

/* chip hidden registers for OTP */
#define SGM458_REG_09		0x09
#define SGM458_REG_0D		0x0D

/* Working mode (config bits 1:0) */
#define SGM458_CONF_MODE_MASK		GENMASK(1, 0)
#define SGM458_CONF_MODE_SHUTDOWN	0x0
#define SGM458_CONF_MODE_ONESHOT	0x1
#define SGM458_CONF_MODE_CONTINUOUS	0x2	/* M1=1, M0 don't-care */

/* Conversion rate (config bits 6:5) */
#define SGM458_CONF_CR_MASK	GENMASK(6, 5)
#define SGM458_CONF_CR_SHIFT	5

/* Conversion rate in milli-Hz, indexed by the CR1:CR0 field value */
static const unsigned int sgm458_rate_mhz[] = { 250, 1000, 4000, 8000 };

struct sgm458 {
	struct i2c_client *client;
	struct thermal_zone_device *tzd;
	struct mutex lock;
};

/* Initialize the sequence to reload the OTP */
static int sgm458_init_hardware(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, SGM458_TEMP_MSB_REG, 0xA1);
	if (ret < 0) return ret;

	ret = i2c_smbus_write_byte_data(client, SGM458_REG_0D, 0xA1);
	if (ret < 0) return ret;

	ret = i2c_smbus_write_byte_data(client, SGM458_TLOW_REG, 0x0E);
	if (ret < 0) return ret;

	ret = i2c_smbus_write_byte_data(client, SGM458_REG_09, 0x20);
	if (ret < 0) return ret;

	ret = i2c_smbus_write_byte_data(client, SGM458_REG_09, 0x00);
	if (ret < 0) return ret;

	ret = i2c_smbus_write_byte_data(client, SGM458_TEMP_MSB_REG, 0x20);
	return ret;
}

static int sgm458_update_conf(struct sgm458 *sgm458, u8 mask, u8 val)
{
	int ret;

	mutex_lock(&sgm458->lock);

	ret = i2c_smbus_read_byte_data(sgm458->client, SGM458_CONF_REG);
	if (ret < 0)
		goto out;

	ret = i2c_smbus_write_byte_data(sgm458->client, SGM458_CONF_REG,
					(ret & ~mask) | (val & mask));
out:
	mutex_unlock(&sgm458->lock);
	return ret;
}

static int sgm458_get_temp(struct thermal_zone_device *tzd, int *temp)
{
	struct sgm458 *sgm458 = tzd->devdata;
	struct i2c_client *client = sgm458->client;
	int msb, lsb;
	s16 raw;

	msb = i2c_smbus_read_byte_data(client, SGM458_TEMP_MSB_REG);
	if (msb < 0)
		return msb;

	lsb = i2c_smbus_read_byte_data(client, SGM458_TEMP_LSB_REG);
	if (lsb < 0)
		return lsb;

	/* Combine MSB and LSB registers. */
	raw = (msb << 8) | lsb;

	/* Arithmetic shift right by 4 bits to remove the unused TD[3:0] bits. */
	raw = raw >> 4;

	/* Convert the 12-bit raw ADC code into milliCelsius for Thermal Framework. */
	*temp = ((s32)raw * 125) / 2;

	return 0;
}

static const struct thermal_zone_device_ops sgm458_thermal_ops = {
	.get_temp = sgm458_get_temp,
};

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct sgm458 *sgm458 = dev_get_drvdata(dev);
	const char *mode;
	int ret;

	ret = i2c_smbus_read_byte_data(sgm458->client, SGM458_CONF_REG);
	if (ret < 0)
		return ret;

	switch (ret & SGM458_CONF_MODE_MASK) {
	case SGM458_CONF_MODE_SHUTDOWN:
		mode = "shutdown";
		break;
	case SGM458_CONF_MODE_ONESHOT:
		mode = "one-shot";
		break;
	default:
		mode = "continuous";
		break;
	}

	return sysfs_emit(buf, "%s\n", mode);
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct sgm458 *sgm458 = dev_get_drvdata(dev);
	u8 mode;
	int ret;

	if (sysfs_streq(buf, "shutdown"))
		mode = SGM458_CONF_MODE_SHUTDOWN;
	else if (sysfs_streq(buf, "one-shot"))
		mode = SGM458_CONF_MODE_ONESHOT;
	else if (sysfs_streq(buf, "continuous"))
		mode = SGM458_CONF_MODE_CONTINUOUS;
	else
		return -EINVAL;

	ret = sgm458_update_conf(sgm458, SGM458_CONF_MODE_MASK, mode);
	return ret < 0 ? ret : count;
}
static DEVICE_ATTR_RW(mode);

static ssize_t mode_available_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "shutdown one-shot continuous\n");
}
static DEVICE_ATTR_RO(mode_available);

static ssize_t conversion_rate_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sgm458 *sgm458 = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_read_byte_data(sgm458->client, SGM458_CONF_REG);
	if (ret < 0)
		return ret;

	ret = (ret & SGM458_CONF_CR_MASK) >> SGM458_CONF_CR_SHIFT;
	return sysfs_emit(buf, "%u\n", sgm458_rate_mhz[ret]);
}

static ssize_t conversion_rate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct sgm458 *sgm458 = dev_get_drvdata(dev);
	unsigned int val;
	int i, ret;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(sgm458_rate_mhz); i++)
		if (sgm458_rate_mhz[i] == val)
			break;
	if (i == ARRAY_SIZE(sgm458_rate_mhz))
		return -EINVAL;

	ret = sgm458_update_conf(sgm458, SGM458_CONF_CR_MASK,
				 i << SGM458_CONF_CR_SHIFT);
	return ret < 0 ? ret : count;
}
static DEVICE_ATTR_RW(conversion_rate);

static ssize_t conversion_rate_available_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return sysfs_emit(buf, "250 1000 4000 8000\n");
}
static DEVICE_ATTR_RO(conversion_rate_available);

static struct attribute *sgm458_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_mode_available.attr,
	&dev_attr_conversion_rate.attr,
	&dev_attr_conversion_rate_available.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sgm458);

static int sgm458_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sgm458 *sgm458;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA))
		return -EOPNOTSUPP;

	sgm458 = devm_kzalloc(dev, sizeof(*sgm458), GFP_KERNEL);
	if (!sgm458)
		return -ENOMEM;

	sgm458->client = client;
	mutex_init(&sgm458->lock);
	i2c_set_clientdata(client, sgm458);

	ret = sgm458_init_hardware(client);
	if (ret < 0) {
		dev_err(dev, "The chip initialization sequence failed to reload OTP: %d\n", ret);
		return ret;
	}

	sgm458->tzd = devm_thermal_of_zone_register(dev, 0, sgm458,
						    &sgm458_thermal_ops);
	if (IS_ERR(sgm458->tzd))
		return dev_err_probe(dev, PTR_ERR(sgm458->tzd),
				     "failed to register thermal zone\n");

	thermal_zone_device_update(sgm458->tzd, THERMAL_EVENT_UNSPECIFIED);
	return 0;
}

static const struct i2c_device_id sgm458_id[] = {
	{ "sgm458", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sgm458_id);

static const struct of_device_id sgm458_of_match[] = {
	{ .compatible = "sgmicro,sgm458" },
	{ }
};
MODULE_DEVICE_TABLE(of, sgm458_of_match);

static struct i2c_driver sgm458_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(sgm458_of_match),
		.dev_groups = sgm458_groups,
	},
	.probe_new = sgm458_probe,
	.id_table = sgm458_id,
};
module_i2c_driver(sgm458_driver);

MODULE_AUTHOR("Ilya Tsunaev");
MODULE_DESCRIPTION("SG Micro SGM458 temperature sensor driver with thermal zone support");
MODULE_LICENSE("GPL");
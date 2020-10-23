#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/delay.h>

#include <linux/regulator/consumer.h>

#include "as5510.h"

/*************************************************
 ** THE REGMAP PERMISSIONS / CACHING SETUP CODE **
 *************************************************/

static const struct reg_default as5510_reg[] = {
	{DATA1_REG, 0x00}, {DATA2_REG, 0x00}, {CONFIG_REG, 0x00},
	{OFFSET1_REG, 0x00}, {OFFSET2_REG, 0x00}, {SENSITIVITY_REG, 0x00},
};

static bool as5510_readable(struct device *dev, unsigned int reg)
{
	return (reg >= DATA1_REG && reg <= OFFSET2_REG) ||
		reg == SENSITIVITY_REG;
}

static bool as5510_volatile(struct device *dev, unsigned int reg)
{
	return reg == DATA1_REG || reg == DATA2_REG;
}

static bool as5510_writeable(struct device *dev, unsigned int reg)
{
	return reg == CONFIG_REG || reg == SENSITIVITY_REG;
}

static const struct regmap_config as5510_regmap = {
	.name = "as5510",
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SENSITIVITY_REG,
	.readable_reg = as5510_readable,
	.volatile_reg = as5510_volatile,
	.writeable_reg = as5510_writeable,

	.reg_defaults = as5510_reg,
	.num_reg_defaults = ARRAY_SIZE(as5510_reg),

	.cache_type = REGCACHE_FLAT,
	.use_single_rw = true,
};

/******************************************
 ** THE DEVICE DATA / COMMUNICATION CODE **
 ******************************************/
struct as5510_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct regulator *power_regulator;
};


static int read_reg(struct as5510_priv *client_data, u8 reg, u8 *retval)
{
	int ret;
	unsigned int regval = 0;

	ret = regmap_read(client_data->regmap, reg, &regval);
	if (ret < 0) {
		dev_err(&client_data->i2c->dev,
			"Failed reg 0x%x read with err=%d\n",
			reg, ret);
	} else {
		*retval = (u8) regval;
	}
	return ret;
}

static int write_reg(struct as5510_priv *client_data, u8 reg, u8 val)
{
	int ret = regmap_write(client_data->regmap, reg, val);

	if (ret < 0) {
		dev_err(&client_data->i2c->dev,
			"Failed reg 0x%x write with err=%d\n",
			reg, ret);
	}
	return ret;
}

/*****************************
 ** THE AS5510 SYSYFS SETUP **
 *****************************/
static ssize_t adc_raw_value_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	u8 data_reg[2] = {0};
	int expected_parity = 0;
	int calculated_parity = 0;
	int i = 0;
	int raw_adc = 0;
	struct as5510_priv *client_data;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	if (read_reg(client_data, DATA1_REG, &data_reg[0]) < 0 ||
		read_reg(client_data, DATA2_REG, &data_reg[1]) < 0)
		return 0;

	if (!DATA2_IS_COMPENSATING(data_reg[1])) {
		// on boot/resume, offset compensation runs for 0.25-1.5ms
		dev_err(dev, "Device is rebooting\n");
		return -EAGAIN;
	}

	raw_adc = DATA_REGS_TO_VAL(data_reg[0], data_reg[1]);
	expected_parity = DATA2_GET_PARITY(data_reg[1]);
	for (i = 0; i < DATA_BITS; i++)
		calculated_parity ^= (raw_adc >> i) & 1;

	if (expected_parity != calculated_parity) {
		dev_err(dev, "ADC value corrupted\n");
		return -EAGAIN;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", raw_adc);
}
static DEVICE_ATTR_RO(adc_raw_value);

static ssize_t sensitivity_value_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	u8 regval = 0;
	struct as5510_priv *client_data;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	if (read_reg(client_data, SENSITIVITY_REG, &regval) < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", SENSITIVITY_REG_TO_VAL(regval));
}
static ssize_t sensitivity_value_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	u8 newval;
	struct as5510_priv *client_data;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	if (kstrtou8(buf, 10, &newval) != 0 || // write with base 10 integers
		!IS_VALID_SENSITIVITY(newval)) {
		dev_err(dev, "Invalid sensitivity value \'%s\'\n", buf);
		return 0;
	}
	if (write_reg(client_data, SENSITIVITY_REG, newval) < 0)
		return 0;

	return count;
}
static DEVICE_ATTR_RW(sensitivity_value);

static ssize_t config_value_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	u8 regval = 0;
	struct as5510_priv *client_data;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	if (read_reg(client_data, CONFIG_REG, &regval) < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", CONFIG_REG_TO_VAL(regval));
}
static ssize_t config_value_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	u8 newval;
	struct as5510_priv *client_data;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	if (kstrtou8(buf, 10, &newval) != 0 || // write with base 10 integers
		!IS_VALID_CONFIG(newval)) {
		dev_err(dev, "Invalid config value \'%s\'\n", buf);
		return 0;
	}

	if (write_reg(client_data, CONFIG_REG, newval) < 0)
		return 0;

	return count;
}
static DEVICE_ATTR_RW(config_value);

/**********************
 ** DRIVER FUNCTIONS **
 **********************/
static int as5510_i2c_probe(struct i2c_client *i2c)
{
	struct as5510_priv *client_data;
	int retval;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "No i2c functionality exists\n");
		return -ENODEV;
	}

	client_data = devm_kzalloc(&i2c->dev, sizeof(struct as5510_priv),
	GFP_KERNEL);
	if (client_data == NULL)
		return -ENOMEM;

	// turn on power to the device
	client_data->power_regulator =
		devm_regulator_get(&i2c->dev, "power");
	if (IS_ERR(client_data->power_regulator)) {
		retval = (int) PTR_ERR(client_data->power_regulator);
		dev_err(&i2c->dev, "Failed to find power regulator (%d)\n",
			retval);
		return retval;
	}
	retval = regulator_enable(client_data->power_regulator);
	if (retval < 0) {
		dev_err(&i2c->dev, "Failed to enable power regulator (%d)\n",
			retval);
		return retval;
	}

	client_data->regmap = devm_regmap_init_i2c(i2c, &as5510_regmap);
	if (IS_ERR(client_data->regmap)) {
		dev_err(&i2c->dev, "Failed to setup driver regmap\n");
		retval = (int) PTR_ERR(client_data->regmap);

		if (regulator_disable(client_data->power_regulator) < 0)
			dev_err(&i2c->dev, "Failed to disable power regulator\n");

		return retval;
	}

	// wait for connection to be okay
	usleep_range(1600, 2000); // device takes 1.5ms to boot up

	// configure initial register values
	if (regmap_write(client_data->regmap, CONFIG_REG,
		CONFIG_VAL_TO_REG(DO_AVERAGE, POSITIVE_POLARITY, POWER_ON))
		< 0 ||
		regmap_write(client_data->regmap, SENSITIVITY_REG,
		SENSITIVITY_FINE) < 0) {
		dev_err(&i2c->dev, "Failed to initialize device registers\n");

		if (regulator_disable(client_data->power_regulator) < 0)
			dev_err(&i2c->dev, "Failed to disable power regulator\n");

		return -ENODEV;
	}

	client_data->i2c = i2c;
	i2c_set_clientdata(i2c, client_data);

	// setup sysfs files to expose device register data
	retval = device_create_file(&i2c->dev, &dev_attr_adc_raw_value);
	if (retval < 0)
		goto as5510_sysfs_fail;

	retval = device_create_file(&i2c->dev, &dev_attr_sensitivity_value);
	if (retval < 0)
		goto as5510_sysfs_fail;

	retval = device_create_file(&i2c->dev, &dev_attr_config_value);
	if (retval < 0)
		goto as5510_sysfs_fail;

	// successful termination of probe()
	return 0;

as5510_sysfs_fail:
	dev_err(&i2c->dev, "Failed to setup sysfs files\n");

	if (regulator_disable(client_data->power_regulator) < 0)
		dev_err(&i2c->dev, "Failed to disable power regulator\n");

	return retval;
}

static int as5510_i2c_remove(struct i2c_client *i2c)
{
	struct as5510_priv *client_data = i2c_get_clientdata(i2c);
	int retval;

	retval = regulator_disable(client_data->power_regulator);
	if (retval < 0) {
		dev_err(&i2c->dev, "Failed to disable regulator\n");
		return retval;
	}
	return 0;
}

static int as5510_i2c_suspend(struct device *dev, pm_message_t state)
{
	struct as5510_priv *client_data;
	int retval;
	u8 configval;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	// put device in low-power mode
	configval = CONFIG_VAL_TO_REG(DO_AVERAGE, POSITIVE_POLARITY, POWER_LOW);
	if (regmap_write(client_data->regmap, CONFIG_REG, configval) < 0) {
		dev_err(dev, "Failed to write to device registers\n");
		return -EIO;
	}

	// actually power down mode if nothing else on regulator needs power
	retval = regulator_disable(client_data->power_regulator);
	if (retval < 0) {
		dev_err(dev, "Failed to disable regulator\n");

		// try to undo what was done
		configval = CONFIG_VAL_TO_REG(DO_AVERAGE,
				POSITIVE_POLARITY, POWER_ON);
		if (regmap_write(client_data->regmap, CONFIG_REG,
			configval) < 0) {
			dev_err(dev, "FATAL: Failed to reawaken sensor\n");
			return -ENOTRECOVERABLE;
		}
		return -EIO;
	}

	return 0;
}

static int as5510_i2c_resume(struct device *dev)
{
	struct as5510_priv *client_data;
	int retval;
	u8 configval;

	client_data = (struct as5510_priv *) dev_get_drvdata(dev);

	// actually turn on power if it was disabled
	retval = regulator_enable(client_data->power_regulator);
	if (retval < 0) {
		dev_err(dev,
			"FATAL: Failed to re-enable power regulator (%d)\n",
			retval);
		return retval;
	}
	usleep_range(1600, 2000); // device takes 1.5ms to boot up

	// reinitialize register values (needed if fully shut off)
	configval = CONFIG_VAL_TO_REG(DO_AVERAGE, POSITIVE_POLARITY, POWER_ON);
	if (regmap_write(client_data->regmap, CONFIG_REG, configval) < 0 ||
		regmap_write(client_data->regmap, SENSITIVITY_REG,
		SENSITIVITY_FINE) < 0) {
		dev_err(dev, "FATAL: Failed to re-initialize device registers\n");
		return -ENOTRECOVERABLE;
	}

	return 0;
}

/**************************************************************
 ** ACTUALLY REGISTER DRIVER NOW GIVEN ALL DEFINED FUNCTIONS **
 **************************************************************/
static const struct of_device_id as5510_i2c_dt_ids[] = {
		{.compatible = "ams,as5510"}, {} };
MODULE_DEVICE_TABLE(of, as5510_i2c_dt_ids);
static const struct i2c_device_id as5510_i2c_id[] = {{"as5510", 0}, {} };
MODULE_DEVICE_TABLE(i2c, as5510_i2c_id);

static struct i2c_driver as5510_i2c_driver = {
	.driver = {
		.name = "as5510",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(as5510_i2c_dt_ids),
		.suspend = as5510_i2c_suspend,
		.resume = as5510_i2c_resume,
	},
	.probe_new = as5510_i2c_probe,
	.remove = as5510_i2c_remove,
	.id_table = as5510_i2c_id,
};

module_i2c_driver(as5510_i2c_driver);
MODULE_DESCRIPTION("Driver for AS5510 I2C Hall Effect Sensor");
MODULE_LICENSE("GPL");

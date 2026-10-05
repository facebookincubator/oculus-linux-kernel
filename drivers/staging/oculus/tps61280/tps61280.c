// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/of_regulator.h>

#define DRIVER_NAME "tps61280"

/*
 * Register definitions to all subdrivers
 */
#define TPS61280_REG_CONFIG		0x01
#define TPS61280_REG_VFLOOR		0x02
#define TPS61280_VOLTAGE_MASK		0x1F
#define TPS61280_REG_VROOF		0x03
#define TPS61280_REG_ILIM		0x04
#define TPS61280_VOUT_MIN		2850  /* mV */
#define TPS61280_VOUT_MAX		4400  /* mV */
#define TPS61280_VOUT_STEP		50    /* mV */
#define TPS61280_REG_STATUS		0x05

#define HW_CONTROL_MODE			0x00
#define PFM_AUTO_PWM_MODE		0x01
#define FORCE_PWM_MODE			0x02
#define PWM_AUTO_PFM_MODE		0x03
#define MODE_MASK			GENMASK(1, 0)
#define TPS61280_NUM_VOLTAGES		0x20
#define TPS61280_RESET_MASK		0x80

struct tps61280_chip {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
	struct gpio_desc *boost_en_gpio;
};

enum {
	BOOST_MODE_SET_PWM = 0x01,
	BOOST_MODE_DISABLE_BOOST_EN,
};

static const struct linear_range tps61280_volt_ranges[] = {
	REGULATOR_LINEAR_RANGE(2850000, 0x00, 0x1F, 50000),
};

static const unsigned int mp8892_volt_range_sel[] = {
	0x00, 0x1F
};

static int tps61280_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	int ret;
	struct tps61280_chip *data = rdev_get_drvdata(rdev);

	switch (mode) {
	case BOOST_MODE_SET_PWM:
		gpiod_set_value_cansleep(data->boost_en_gpio,1);
		msleep(50);
		ret = regmap_update_bits(rdev->regmap,TPS61280_REG_CONFIG,
			MODE_MASK, mode);
		if (ret < 0)
			return ret;
		break;
	case BOOST_MODE_DISABLE_BOOST_EN:
		gpiod_set_value_cansleep(data->boost_en_gpio,0);
		break;
	default:
		break;
	}
	return 0;
};

static const struct regulator_ops tps61280_regulator_ops = {
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_pickable_linear_range,
	.set_mode		= tps61280_set_mode,
};

static struct regulator_desc tps61280_regulator_desc = {
	.name		= "boost",
	.ops		= &tps61280_regulator_ops,
	.type		= REGULATOR_VOLTAGE,
	.id		= 0,
	.owner		= THIS_MODULE,
	.linear_range_selectors = mp8892_volt_range_sel,
	.linear_ranges	= tps61280_volt_ranges,
	.n_linear_ranges	= ARRAY_SIZE(tps61280_volt_ranges),
	.n_voltages	= TPS61280_NUM_VOLTAGES,
	.vsel_reg       = TPS61280_REG_VFLOOR,
	.vsel_mask	= TPS61280_VOLTAGE_MASK,
};

static const struct regmap_config tps61280_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = TPS61280_REG_STATUS,
	.use_single_read = true,
	.use_single_write = true,
};

static int gpio_init_helper(struct device *dev, struct gpio_desc **desc,
		const char *name, enum gpiod_flags flags)
{
	*desc = devm_gpiod_get(dev, name, flags);
	if (IS_ERR_OR_NULL(*desc)) {
		if (PTR_ERR(*desc) == -ENOENT)
			dev_warn(dev, "Could not find definition for %s gpio\n", name);
		else
			dev_err(dev, "Failed to acquire %s gpio\n", name);
		return PTR_ERR(*desc);
	}
	return 0;
};

/*
 * Registers the chip as a voltage regulator
 */
static int tps61280_regulator_probe(struct i2c_client * const client,
				const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regmap *regmap;
	struct tps61280_chip *pchip;
	int ret;
	unsigned int val;
	struct regulator_config config = { 0 };

	struct regulator_init_data init_data = {
		.constraints = {
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_FAST |
				REGULATOR_MODE_NORMAL |
				REGULATOR_MODE_IDLE,
			.min_uV = TPS61280_VOUT_MIN * 1000,
			.max_uV = TPS61280_VOUT_MAX * 1000,
		},
	};

	pchip = devm_kzalloc(dev, sizeof(struct tps61280_chip), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	i2c_set_clientdata(client, pchip);
	pchip->dev = dev;
	regmap = devm_regmap_init_i2c(client, &tps61280_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to allocate regmap!\n");
		return PTR_ERR(regmap);
	}

	if (of_property_read_bool(dev->of_node, "tps,vsel-high"))
		tps61280_regulator_desc.vsel_reg = TPS61280_REG_VROOF;

	config.dev = dev;
	config.regmap = regmap;
	config.init_data = &init_data;
	config.driver_data = pchip;
	pchip->regulator_desc = tps61280_regulator_desc;
	pchip->regulator = devm_regulator_register(pchip->dev,
			&tps61280_regulator_desc, &config);
	if (IS_ERR(pchip->regulator)) {
		ret = PTR_ERR(pchip->regulator);
		dev_err(dev, "failed to register regulator %s\n",
			tps61280_regulator_desc.name);
		return ret;
	}

	ret = gpio_init_helper(pchip->dev, &pchip->boost_en_gpio, "boost-en",
			GPIOD_OUT_LOW);
	if (ret)
		dev_err(pchip->dev, "failed to init BOOST-EN gpio: %d", ret);


	/* reset tps61280 to make it work normally */
	gpiod_set_value_cansleep(pchip->boost_en_gpio,1);
	msleep(50);
	ret = regmap_read(pchip->regulator->regmap, TPS61280_REG_CONFIG, &val);
	if (ret < 0) {
		dev_err(pchip->dev, "%s: Read TPS61280_REG_CONFIG failed: %d",
				__func__, ret);
		return ret;
	}
	val |= TPS61280_RESET_MASK;
	ret = regmap_write(pchip->regulator->regmap, TPS61280_REG_CONFIG, val);
	if (ret < 0) {
		dev_err(pchip->dev, "%s: Write TPS61280_REG_CONFIG failed: %d",
				__func__, ret);
		return ret;
	}
	msleep(50);
	gpiod_set_value_cansleep(pchip->boost_en_gpio,0);

	dev_info(pchip->dev, "tps61280 probe done\n");
	return 0;
};

static const struct i2c_device_id tps61280_i2c_id[] = {{DRIVER_NAME}, {}};
MODULE_DEVICE_TABLE(i2c, tps61280_i2c_id);

static const struct of_device_id tps61280_of_match[] = {
	{.compatible = "ti,tps61280"},
	{},
};
MODULE_DEVICE_TABLE(of, tps61280_of_match);

static struct i2c_driver tps61280_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(tps61280_of_match),
	},
	.probe = tps61280_regulator_probe,
	.id_table = tps61280_i2c_id,
};
module_i2c_driver(tps61280_driver);

MODULE_AUTHOR("Feynman Liu <feynman.liu@goertek.com>");
MODULE_DESCRIPTION("TPS61280 boost driver");
MODULE_LICENSE("GPL v2");

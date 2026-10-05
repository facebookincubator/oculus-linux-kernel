// SPDX-License-Identifier: GPL-2.0+

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include "mp28167.h"

#define DRIVER_NAME "mp28167"

struct mp28167_chip {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
	unsigned int ilim_step_ua;
	unsigned int vout_step_uv;
	unsigned int vout_min_uv;
	unsigned int vout_max_uv;
	unsigned int vout_max_steps;
	unsigned int ilim_scaling;
	struct gpio_desc *bob_enable_gpio;
	struct gpio_desc *bob_alt_gpio;
};

static int mp28167_get_status(struct regulator_dev *rdev)
{
	struct mp28167_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(pchip->regmap, MP28167_REG_STATUS, &data);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read i2c: %d @ function %s", ret,
			__func__);
		return ret;
	}
	return (data & MP28167_MASK_ST) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static int mp28167_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mp28167_chip *pchip = rdev_get_drvdata(rdev);
	int ret;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* forced pwm mode */
		ret = regmap_update_bits(pchip->regmap, MP28167_REG_CTL1,
					 MP28167_MASK_FPWM, 1);
		break;
	case REGULATOR_MODE_NORMAL:
		/* enable automatic pwm/pfm mode */
		ret = regmap_update_bits(pchip->regmap, MP28167_REG_CTL1,
					 MP28167_MASK_FPWM, 0);
		break;
	default:
		dev_err(pchip->dev, "unsupported buck mode %d @ function %s",
			mode, __func__);
		return -EINVAL;
	}

	return ret;
}

static unsigned int mp28167_get_mode(struct regulator_dev *rdev)
{
	struct mp28167_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int rval;
	int ret;

	ret = regmap_read(pchip->regmap, MP28167_REG_CTL1, &rval);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read i2c: %d @ function %s", ret,
			__func__);
		return ret;
	}

	return (rval & MP28167_MASK_FPWM) ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static int mp28167_set_voltage_regmap(struct regulator_dev *rdev,
				      int min_uV, int max_uV,
				      unsigned int *selector)
{
	struct mp28167_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int vref_h, vref_l, sel_v, vout_uv;
	int ret;

	sel_v = (min_uV - pchip->vout_min_uv) / pchip->vout_step_uv;
	vout_uv = pchip->vout_step_uv * sel_v;

	if (vout_uv > pchip->vout_max_uv) {
		dev_err(pchip->dev,
			"voltage %d uV over the limit %d uV @ function %s",
			vout_uv, pchip->vout_max_uv, __func__);
		return -EINVAL;
	}

	vref_l = sel_v & MP28167_MASK_VOUT_L;
	vref_h = sel_v >> 3;
	ret = regmap_write(pchip->regmap, MP28167_REG_VREF_L, vref_l);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to write MP28167_REG_VREF_L: %d @ function %s",
			ret, __func__);
		return ret;
	}
	ret = regmap_write(pchip->regmap, MP28167_REG_VREF_H, vref_h);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to write MP28167_REG_VREF_H: %d @ function %s",
			ret, __func__);
		return ret;
	}

	ret = regmap_update_bits(pchip->regmap, MP28167_REG_VREF_GO,
				 MP28167_MASK_GO_BIT, 1);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to write MP28167_REG_VREF_GO: %d @ function %s",
			ret, __func__);
		return ret;
	}

	return ret;
}

static int mp28167_get_voltage_regmap(struct regulator_dev *rdev)
{
	struct mp28167_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int vref_h, vref_l, vout_uv, sel_v;
	int ret;

	ret = regmap_read(pchip->regmap, MP28167_REG_VREF_L, &vref_l);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to read MP28167_REG_VREF_L: %d @ function %s",
			ret, __func__);
		return ret;
	}
	ret = regmap_read(pchip->regmap, MP28167_REG_VREF_H, &vref_h);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to read MP28167_REG_VREF_H: %d @ function %s",
			ret, __func__);
		return ret;
	}
	sel_v = (vref_l & MP28167_MASK_VOUT_L) + (vref_h << 3);
	vout_uv = pchip->vout_min_uv + sel_v * pchip->vout_step_uv;

	return ret ? ret : vout_uv;
}

static int mp28167_chg_get_current_limit(struct regulator_dev *rdev)
{
	struct mp28167_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int reg;
	int ret;

	ret = regmap_read(pchip->regmap, MP28167_REG_IOUT_LIM, &reg);
	if (ret < 0) {
		dev_err(pchip->dev,
			"failed to read MP28167_REG_IOUT_LIM: %d @ function %s",
			ret, __func__);
		return ret;
	}

	reg = (reg & MP28167_MASK_IOUT_LIM) * pchip->ilim_step_ua;
	return reg * MP28167_ILIM_SCALING_FACTOR / pchip->ilim_scaling;
}

static const struct regulator_ops mp28167_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage = mp28167_set_voltage_regmap,
	.get_voltage = mp28167_get_voltage_regmap,
	.get_current_limit = mp28167_chg_get_current_limit,
	.get_status = mp28167_get_status,
	.set_mode = mp28167_set_mode,
	.get_mode = mp28167_get_mode,
};

static const struct regmap_config mp28167_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MP28167_REG_IC_REV,
	.use_single_read = true,
	.use_single_write = true,
};

static struct regulator_desc mp28167_reg_vout = {
	.name = "vout",
	.id = 0,
	.ops = &mp28167_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.enable_mask = MP28167_MASK_CTL1_EN,
	.enable_reg = MP28167_REG_CTL1,
	.n_voltages = MP28167_MASK_VOUT + 1,
	.active_discharge_off = MP28167_AD_DISABLED,
	.active_discharge_on = MP28167_AD_ENABLED,
	.active_discharge_mask = MP28167_AD_MASK_BIT,
	.active_discharge_reg = MP28167_REG_CTL1,
};

static int gpio_store_helper(struct gpio_desc *gpio,
			     const char *buf)
{
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	else if (val < 0 || val > 1)
		return -EINVAL;

	gpiod_set_value_cansleep(gpio, val);

	return 0;
}

static ssize_t bob_enable_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct mp28167_chip *pchip = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pchip->bob_enable_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t bob_enable_store(struct device *dev,
			        struct device_attribute *attr,
			        const char *buf, size_t count)
{
	struct mp28167_chip *pchip = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pchip->bob_enable_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(bob_enable);

static ssize_t bob_alt_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct mp28167_chip *pchip = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pchip->bob_alt_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static DEVICE_ATTR_RO(bob_alt);

static int mp28167_init_params(struct mp28167_chip *pchip,
			       struct device_node *node)
{
	struct device *dev = pchip->dev;
	int ret;

	ret = of_property_read_u32(node, "vout-step-uv", &pchip->vout_step_uv);
	if (ret < 0) {
		dev_err(dev, "failed to read the vout-step-uv property %d @ function %s",
			ret, __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "vout-min-uv", &pchip->vout_min_uv);
	if (ret < 0) {
		dev_err(dev, "failed to read the vout-min-uv property %d @ function %s",
			ret, __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "vout-max-steps", &pchip->vout_max_steps);
        if (ret < 0) {
		dev_warn(dev, "failed to read the vout-max-steps property %d @ function %s Setting it to maximum value",
			ret, __func__);
		pchip->vout_max_steps = MP28167_MASK_VOUT;
	}

	pchip->vout_max_uv = pchip->vout_step_uv * pchip->vout_max_steps;

	ret = of_property_read_u32(node, "ilim-step-ua", &pchip->ilim_step_ua);
	if (ret < 0) {
		dev_err(dev, "failed to read the ilim-step-ua property %d @ function %s",
			ret, __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "ilim-scaling", &pchip->ilim_scaling);
	if (ret < 0) {
		dev_warn(dev, "failed to read the ilim-scaling property %d @ function %s."
			" Use default value %u", ret, __func__, MP28167_DEFAULT_ILIM_SCALING);
		pchip->ilim_scaling = MP28167_DEFAULT_ILIM_SCALING;
		ret = 0;
	}

	return ret;
}

static int mp28167_init_regulator(struct mp28167_chip *pchip,
				  struct device_node *node)
{
	int ret;
	struct regulator_init_data init_data = {
		.constraints = {
			.valid_ops_mask =
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
			.min_uV = pchip->vout_min_uv,
			.max_uV = pchip->vout_step_uv * pchip->vout_max_steps,
			.keep_on = true,
		},
	};
	struct regulator_config config = {};

	config.regmap = pchip->regmap;
	config.driver_data = pchip;
	config.dev = pchip->dev;
	config.init_data = &init_data;

	mp28167_reg_vout.min_uV = pchip->vout_min_uv;
	mp28167_reg_vout.uV_step = pchip->vout_step_uv;
	mp28167_reg_vout.linear_min_sel = 1;

	pchip->regulator_desc = mp28167_reg_vout;
	pchip->regulator =
		devm_regulator_register(pchip->dev, &mp28167_reg_vout, &config);

	if (IS_ERR(pchip->regulator)) {
		ret = PTR_ERR(pchip->regulator);
		return ret;
	}

	ret = devm_regulator_debug_register(pchip->dev, pchip->regulator);
	if (ret)
		dev_err(pchip->dev, "Failed to register debug regulator, rc=%d\n", ret);

	return 0;
}

static irqreturn_t bob_alt_irq_handler(int irq, void *data)
{
	struct mp28167_chip *pchip = data;

	/* Only log this event for now */
	dev_warn(pchip->dev, "Received BOB_ALT IRQ\n");

	return IRQ_HANDLED;
}

static int gpio_init_helper(struct device *dev, struct gpio_desc **desc,
			    const char *name, enum gpiod_flags flags)
{
	*desc = devm_gpiod_get(dev, name, flags);
	if (IS_ERR_OR_NULL(*desc)) {
		dev_err(dev, "Failed to acquire %s gpio\n", name);
		return PTR_ERR(*desc);
	}
	return 0;
}

static int mp28167_regulator_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct device_node *node = client->dev.of_node;
	struct device *dev = &client->dev;
	struct mp28167_chip *pchip;
	int ret, bob_alt_irq;

	pchip = devm_kzalloc(dev, sizeof(struct mp28167_chip), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	i2c_set_clientdata(client, pchip);
	pchip->dev = dev;

	pchip->regmap = devm_regmap_init_i2c(client, &mp28167_regmap_config);
	if (IS_ERR(pchip->regmap)) {
		ret = PTR_ERR(pchip->regmap);
		dev_err(dev, "failed to initialize regmap: %d", ret);
		return ret;
	}

	ret = mp28167_init_params(pchip, node);
	if (ret < 0) {
		dev_err(dev, "failed to initialize parameters: %d", ret);
		return ret;
	}

	ret = mp28167_init_regulator(pchip, node);
	if (ret < 0) {
		dev_err(dev, "failed to register regulator: %d", ret);
		return ret;
	}

	/**
	 * The output value here maps to active state and not the actual value
	 * of the pin
	 */
	ret = gpio_init_helper(dev, &pchip->bob_enable_gpio, "bob-enable",
			       GPIOD_OUT_HIGH);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pchip->bob_alt_gpio, "bob-alt",
			       GPIOD_IN);
	if (ret)
		return ret;

	bob_alt_irq = gpiod_to_irq(pchip->bob_alt_gpio);
	ret = devm_request_irq(dev, bob_alt_irq, &bob_alt_irq_handler,
			       IRQF_TRIGGER_FALLING, "bob_alt_irq", pchip);
	if (ret) {
		dev_err(dev, "Failed to request the bob_alt IRQ ret=%d\n", ret);
		return ret;
	}
	ret = enable_irq_wake(bob_alt_irq);
	if (ret) {
		dev_err(dev, "Failed to set bob_alt IRQ wake ret=%d", ret);
		return ret;
	}

	device_create_file(dev, &dev_attr_bob_enable);
	device_create_file(dev, &dev_attr_bob_alt);

	dev_info(pchip->dev, "mp28167 init done");

	return 0;
}

static int mp28167_regulator_remove(struct i2c_client *client)
{
	struct mp28167_chip *pchip;
	struct device *dev;

	pchip = i2c_get_clientdata(client);
	dev = pchip->dev;

	device_remove_file(dev, &dev_attr_bob_enable);
	device_remove_file(dev, &dev_attr_bob_alt);

	return 0;
}

static const struct i2c_device_id mp28167_i2c_id[] = {{DRIVER_NAME}, {}};
MODULE_DEVICE_TABLE(i2c, mp28167_i2c_id);

static const struct of_device_id mp28167_of_match[] = {
	{.compatible = "mps,mp28167"},
	{},
};
MODULE_DEVICE_TABLE(of, mp28167_of_match);

static struct i2c_driver mp28167_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(mp28167_of_match),
	},
	.probe = mp28167_regulator_probe,
	.remove = mp28167_regulator_remove,
	.id_table = mp28167_i2c_id,
};
module_i2c_driver(mp28167_driver);

MODULE_DESCRIPTION("Regulator Device Driver for MonolithicPower MP28167");
MODULE_LICENSE("GPL v2");

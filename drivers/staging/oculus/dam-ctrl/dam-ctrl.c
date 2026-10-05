// SPDX-License-Identifier: GPL-2.0-only
/**
 * This driver is responsible for controlling the DAM MCU on the Oatmeal
 * platform.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/pinctrl/consumer.h>

#define RESET_DELAY_US 10000
#define BOOTLOADER_DELAY_US 200000

struct dam_ctrl {
	struct device *dev;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *sbsfu_boot_gpio;
	/* BOOT0 pin */
	struct gpio_desc *rom_boot_gpio;
	struct gpio_desc *keep_awake_gpio;
	struct mutex reset_lock;

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

static ssize_t sbsfu_boot_gpio_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->sbsfu_boot_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sbsfu_boot_gpio_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pdata->sbsfu_boot_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(sbsfu_boot_gpio);

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	if (val != 1)
		return -EINVAL;

	/**
	 * Value here maps to active state and not the actual value of the pin
	 */
	mutex_lock(&pdata->reset_lock);
	gpiod_set_value(pdata->reset_gpio, 1);
	usleep_range(RESET_DELAY_US, RESET_DELAY_US + 500);
	gpiod_set_value(pdata->reset_gpio, 0);
	mutex_unlock(&pdata->reset_lock);

	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t rom_boot_gpio_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->rom_boot_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t rom_boot_gpio_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pdata->rom_boot_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(rom_boot_gpio);

static ssize_t bootloader_store_helper(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count,
				       struct gpio_desc *bl_gpio,
				       const char *bl_name,
				       unsigned long delay_us)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	if (val != 1)
		return -EINVAL;

	/**
	 * Value here maps to active state and not the actual value of the pin
	 */
	mutex_lock(&pdata->reset_lock);
	gpiod_set_value(bl_gpio, 1);
	gpiod_set_value(pdata->reset_gpio, 1);
	usleep_range(RESET_DELAY_US, RESET_DELAY_US + 500);
	gpiod_set_value(pdata->reset_gpio, 0);
	usleep_range(delay_us, delay_us + 500);
	gpiod_set_value(bl_gpio, 0);
	mutex_unlock(&pdata->reset_lock);

	dev_info(dev, "DAM entered the %s bootloader mode\n", bl_name);

	return count;
}

static ssize_t sbsfu_bootloader_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);

	return bootloader_store_helper(dev, attr, buf, count,
				       pdata->sbsfu_boot_gpio, "SBSFU",
				       BOOTLOADER_DELAY_US);
}
static DEVICE_ATTR_WO(sbsfu_bootloader);

static ssize_t rom_bootloader_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);

	return bootloader_store_helper(dev, attr, buf, count,
				       pdata->rom_boot_gpio, "ROM",
				       BOOTLOADER_DELAY_US);
}
static DEVICE_ATTR_WO(rom_bootloader);

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

static ssize_t keep_awake_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->keep_awake_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t keep_awake_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pdata->keep_awake_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(keep_awake);

/**
 * This is needed becaused the SWD driver also needs to control the reset line
 * but we cannot share the same GPIO between them.
 */
void dam_ctrl_reset(struct device *dev, int val)
{
	struct dam_ctrl *pdata = dev_get_drvdata(dev);

	mutex_lock(&pdata->reset_lock);
	gpiod_set_value(pdata->reset_gpio, val);
	mutex_unlock(&pdata->reset_lock);
}
EXPORT_SYMBOL(dam_ctrl_reset);

static int dam_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dam_ctrl *pdata;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = dev;

	mutex_init(&pdata->reset_lock);

	/**
	 * The output value here maps to active state and not the actual value
	 * of the pin.
	 */
	ret = gpio_init_helper(dev, &pdata->reset_gpio, "reset",
			       GPIOD_OUT_LOW);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pdata->sbsfu_boot_gpio, "sbsfu-boot",
			       GPIOD_OUT_LOW);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pdata->rom_boot_gpio, "rom-boot",
			       GPIOD_OUT_LOW);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pdata->keep_awake_gpio, "keep-awake",
			       GPIOD_OUT_LOW);
	if (ret)
		return ret;

	device_create_file(dev, &dev_attr_reset);
	device_create_file(dev, &dev_attr_sbsfu_boot_gpio);
	device_create_file(dev, &dev_attr_sbsfu_bootloader);
	device_create_file(dev, &dev_attr_rom_boot_gpio);
	device_create_file(dev, &dev_attr_rom_bootloader);
	device_create_file(dev, &dev_attr_keep_awake);

	dev_set_drvdata(dev, pdata);

	dev_info(dev, "DAM setup complete\n");

	return 0;
}

static int dam_ctrl_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_reset);
	device_remove_file(dev, &dev_attr_sbsfu_boot_gpio);
	device_remove_file(dev, &dev_attr_sbsfu_bootloader);
	device_remove_file(dev, &dev_attr_rom_boot_gpio);
	device_remove_file(dev, &dev_attr_rom_bootloader);
	device_remove_file(dev, &dev_attr_keep_awake);

	return 0;
}

static const struct of_device_id dam_ctrl_of_match[] = {
	{.compatible = "meta,dam-ctrl"},
	{},
};
MODULE_DEVICE_TABLE(of, dam_ctrl_of_match);

static struct platform_driver dam_ctrl_driver = {
	.driver = {
		.name = "dam_ctrl",
		.of_match_table = of_match_ptr(dam_ctrl_of_match),
	},
	.probe = dam_ctrl_probe,
	.remove = dam_ctrl_remove,
};

module_platform_driver(dam_ctrl_driver);

MODULE_DESCRIPTION("DAM control driver");
MODULE_LICENSE("GPL v2");

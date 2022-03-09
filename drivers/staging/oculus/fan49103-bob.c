/*
 * Copyright (c) Facebook, Inc.
 *
 * This file is released under the GPL v2 or later.
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/kernel.h>

#include "fan49103-bob.h"

static struct fan49103_ctx ctx;

static const char * const fan49104_reg_addr_to_name[FAN_REG_RANGE_SIZE] = {
	_FAN_REG_NAME(FAN49103_REG_CONTROL, "control"),
	_FAN_REG_NAME(FAN49103_REG_MANUFACTURER, "manufacturer_id"),
	_FAN_REG_NAME(FAN49103_REG_DEVICE, "device_id"),
};

static int fan49103_read_reg_byte(enum fan49103_reg_t reg, uint8_t *reg_val)
{
	int rc = 0;
	unsigned int read_val;

	if (!reg_val)
		return -EINVAL;

	BUG_ON(!ctx.registers);
	BUG_ON(!ctx.dev);

	dev_dbg(ctx.dev, "trying to read register " FAN_REG_PRINT_FMT "\n",
		FAN_REG_FMT_ARGS(reg));

	rc = regmap_read(ctx.registers, reg, &read_val);
	if (rc) {
		dev_err(ctx.dev,
			"error reading register " FAN_REG_PRINT_FMT ": %d\n",
			FAN_REG_FMT_ARGS(reg), rc);
		return rc;
	}

	if (read_val > U8_MAX) {
		dev_err(ctx.dev,
			"unexpected value for register " FAN_REG_PRINT_FMT
			": %d\n", FAN_REG_FMT_ARGS(reg), read_val);
		return -EINVAL;
	}

	*reg_val = read_val;

	return 0;
}

static int fan49103_write_reg_byte(enum fan49103_reg_t reg, uint8_t reg_val)
{
	int rc = 0;

	BUG_ON(!ctx.registers);
	BUG_ON(!ctx.dev);

	rc = regmap_write(ctx.registers, reg, reg_val);
	if (rc)
		dev_err(ctx.dev,
			"error writing register " FAN_REG_PRINT_FMT ": %d\n",
			FAN_REG_FMT_ARGS(reg), rc);

	return rc;
}

static const struct regmap_range fan49103_read_ranges[] = {
	regmap_reg_range(FAN49103_REG_CONTROL, FAN49103_REG_CONTROL),
	regmap_reg_range(FAN49103_REG_MANUFACTURER, FAN49103_REG_DEVICE),
};

static const struct regmap_range fan49103_write_ranges[] = {
	regmap_reg_range(FAN49103_REG_CONTROL, FAN49103_REG_CONTROL),
};

static const struct regmap_access_table fan49103_read_table = {
	.yes_ranges = fan49103_read_ranges,
	.n_yes_ranges = ARRAY_SIZE(fan49103_read_ranges),
};

static const struct regmap_access_table fan49103_write_table = {
	.yes_ranges = fan49103_write_ranges,
	.n_yes_ranges = ARRAY_SIZE(fan49103_write_ranges),
};

static const struct regmap_config fan49103_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= FAN49103_REG_MAX_REG,
	.cache_type = REGCACHE_NONE,
	.rd_table	= &fan49103_read_table,
	.wr_table	= &fan49103_write_table,
	.val_format_endian = REGMAP_ENDIAN_BIG,
};

static ssize_t control_show(struct device_driver *driver,
			    char *buf)
{
	u8 val = 0;

	fan49103_read_reg_byte(FAN49103_REG_CONTROL, &val);

	return scnprintf(buf, PAGE_SIZE, "%02x\n", val);
}

static ssize_t control_store(struct device_driver *driver,
			     const char *buf, size_t count)
{
	int rc;
	u8 val = 0;

	rc = kstrtou8(buf, 0, &val);
	if (!rc)
		fan49103_write_reg_byte(FAN49103_REG_CONTROL, val);
	else
		dev_err(ctx.dev,
			"Invalid input value for CONTROL. rc=%d buf=%s\n",
			rc, buf);

	return count;
}
static DRIVER_ATTR_RW(control);

static ssize_t manufacturer_id_show(struct device_driver *driver,
				    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02X\n", ctx.manufacturer_id);
}
static DRIVER_ATTR_RO(manufacturer_id);

static ssize_t device_id_show(struct device_driver *driver,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02X\n", ctx.device_id);
}
static DRIVER_ATTR_RO(device_id);

static struct attribute *fan49103_drv_attrs[] = {
	&driver_attr_control.attr,
	&driver_attr_manufacturer_id.attr,
	&driver_attr_device_id.attr,
	NULL
};

static struct attribute_group fan49103_dev_attr_group = {
	.name = "fan49103",
	.attrs = fan49103_drv_attrs,
};

static const struct attribute_group *fan49103_attr_groups[] = {
	&fan49103_dev_attr_group,
	NULL
};

static int bob_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct device *dev = &client->dev;

	struct regmap *registers = NULL;

	registers = devm_regmap_init_i2c(client, &fan49103_regmap_config);
	if (IS_ERR(registers)) {
		rc = PTR_ERR(registers);
		dev_err(dev, "regmap init failed: %d\n", rc);
		return rc;
	}

	ctx.registers = registers;
	ctx.dev = dev;

	if (fan49103_read_reg_byte(FAN49103_REG_MANUFACTURER,
				   &ctx.manufacturer_id))
		return -EINVAL;

	if (fan49103_read_reg_byte(FAN49103_REG_DEVICE, &ctx.device_id))
		return -EINVAL;

	return rc;
}

static const struct of_device_id bob_of_match[] = {
	{
		.compatible = "oculus,fan49103-bob",
	},
	{},
};
MODULE_DEVICE_TABLE(of, bob_of_match);

static const struct i2c_device_id bob_id[] = {
	{
		.name = "fan49103-bob",
	},
	{},
};
MODULE_DEVICE_TABLE(i2c, bob_id);

static struct i2c_driver fan49103_bob_driver = {
	.driver = {
		.name = "fan49103-bob",
		.owner = THIS_MODULE,
		.of_match_table = bob_of_match,
		.groups = fan49103_attr_groups,
	},
	.probe = bob_probe,
	.id_table = bob_id,
};

module_i2c_driver(fan49103_bob_driver);

MODULE_DESCRIPTION("FAN49103 BoB Driver");
MODULE_LICENSE("GPL v2");

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This file is released under the GPL v2 or later.
 */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#define TPS63811_REG_CONTROL		0x01
#define TPS63811_REG_STATUS		0x02
#define TPS63811_REG_DEVID		0x03
#define TPS63811_REG_VOUT1		0x04
#define TPS63811_REG_VOUT2		0x05
#define TPS63811_REG_MAX		0x05

#define TPS63811_EN_DELAY_MS		1

#define TPS63811_VOUT_MIN_MV		1800
#define TPS63811_VOUT_MAX_MV		4975
#define TPS63811_VOUT_STEP		25

#define RICHTEK_VOUT_MIN_MV		2025
#define RICHTEK_VOUT_MAX_MV		5200

#define TPS63811_VOUT_ENABLE_MASK	0x20

#define TPS63811_AUTO_SLEW_1V_PER_MS	0x0
#define TPS63811_AUTO_SLEW_2P5V_PER_MS	0x1
#define TPS63811_AUTO_SLEW_5V_PER_MS	0x2
#define TPS63811_AUTO_SLEW_10V_PER_MS	0x3

#define MANUFACTURE_TI			0x0
#define MANUFACTURE_RICHTEK		0xa

#define TPS63811_GET_MANUFACTURER(val) (((val) & 0xf0) >> 4)
#define TPS63811_GET_MAJOR(val) (((val) & 0x0c) >> 2)
#define TPS63811_GET_MINOR(val) ((val) & 0x03)

struct ctx {
	struct device *dev;
	struct regmap *rmap;
	struct regulator *rfreg;
	int on_gpio;
	u8 manufacturer;
	u8 major;
	u8 minor;
	u32 vout;
	u32 minMv;
	u32 maxMv;
};

static const struct regmap_range tps63811_read_ranges[] = {
	regmap_reg_range(TPS63811_REG_CONTROL, TPS63811_REG_VOUT2),
};

static const struct regmap_range tps63811_write_ranges[] = {
	regmap_reg_range(TPS63811_REG_CONTROL, TPS63811_REG_CONTROL),
	regmap_reg_range(TPS63811_REG_VOUT1, TPS63811_REG_VOUT2),
};

static const struct regmap_access_table tps63811_read_table = {
	.yes_ranges = tps63811_read_ranges,
	.n_yes_ranges = ARRAY_SIZE(tps63811_read_ranges),
};

static const struct regmap_access_table tps63811_write_table = {
	.yes_ranges = tps63811_write_ranges,
	.n_yes_ranges = ARRAY_SIZE(tps63811_write_ranges),
};

static const struct regmap_config tps63811_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= TPS63811_REG_MAX,
	.cache_type	= REGCACHE_NONE,
	.rd_table	= &tps63811_read_table,
	.wr_table	= &tps63811_write_table,
};

static int turn_on_gpio(struct ctx *ctx)
{
	int rc;

	rc = gpio_direction_output(ctx->on_gpio, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to set TPS63811_on GPIO : %d\n", rc);
		return rc;
	}
	msleep(TPS63811_EN_DELAY_MS);

	return 0;
}

static void turn_off_gpio(struct ctx *ctx)
{
	int rc;

	rc = gpio_direction_output(ctx->on_gpio, 0);
	if (rc)
		dev_err(ctx->dev, "Failed to unset TPS63811_on GPIO : %d\n",
			rc);
}

static int set_vout_registers(struct ctx *ctx, u32 vout)
{
	int rc;
	u8 val;

	if (ctx->vout < ctx->minMv ||
	    ctx->vout > ctx->maxMv) {
		dev_err(ctx->dev, "VOUT(%u) out of range\n", ctx->vout);
		return -EINVAL;
	}

	val = (vout - ctx->minMv) / TPS63811_VOUT_STEP;

	rc = regmap_write(ctx->rmap, TPS63811_REG_VOUT1, val);
	if (rc) {
		dev_err(ctx->dev, "Failed to write VOUT1 reg : %d", rc);
		return rc;
	}
	dev_dbg(ctx->dev, "VOUT set to %d (%02x)", vout, val);

	return 0;
}

static int vout_enable(struct ctx *ctx, u8 slew)
{
	int rc;
	u8 val = TPS63811_VOUT_ENABLE_MASK | slew;

	/* Not valid op for RICHTEC variant */
	if (ctx->manufacturer == MANUFACTURE_RICHTEK)
		return 0;

	rc = regmap_write(ctx->rmap, TPS63811_REG_CONTROL, val);
	if (rc) {
		dev_err(ctx->dev, "Failed to write CONTROL reg : %d", rc);
	}

	return rc;
}

static int vout_disable(struct ctx *ctx)
{
	int rc;

	/* Not valid op for RICHTEC variant */
	if (ctx->manufacturer == MANUFACTURE_RICHTEK)
		return 0;

	rc = regmap_write(ctx->rmap, TPS63811_REG_CONTROL, 0);
	if (rc)
		dev_err(ctx->dev, "Failed to write CONTROL reg : %d", rc);

	return rc;
}

static int turn_on_registers(struct ctx *ctx)
{
	int rc;

	if (ctx->manufacturer == MANUFACTURE_RICHTEK)
		goto set_final_vout;

	rc = set_vout_registers(ctx, ctx->minMv);
	if (rc)
		return rc;

	rc = vout_enable(ctx, TPS63811_AUTO_SLEW_1V_PER_MS);
	if (rc)
		goto error;

	rc = vout_enable(ctx, TPS63811_AUTO_SLEW_10V_PER_MS);
	if (rc)
		goto error;

set_final_vout:
	rc = set_vout_registers(ctx, ctx->vout);
	if (rc)
		goto error;

	return 0;
error:
	vout_disable(ctx);

	return rc;
}

static ssize_t on_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", gpio_get_value(ctx->on_gpio) ?
			 "on" : "off");
}

static ssize_t on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count) {
	struct ctx *ctx = dev_get_drvdata(dev);
	long res;
	int rc;

	if (!kstrtol(buf, 0, &res)) {
		if (res) {
			rc = turn_on_gpio(ctx);
			if (rc)
				goto error;

			rc = turn_on_registers(ctx);
			if (rc) {
				turn_off_gpio(ctx);
				goto error;
			}

			rc = regulator_enable(ctx->rfreg);
			if (rc) {
				dev_err(dev, "Failed to enable rf regulator");
				goto error;
			}
			dev_info(ctx->dev, "Turned on");
		} else {
			turn_off_gpio(ctx);
			rc = regulator_disable(ctx->rfreg);
			if (rc)
				dev_warn(dev, "Failed to disable rf regulator");

			dev_info(ctx->dev, "Turned off");
		}
	}

	return count;
error:
	return rc;
}

static ssize_t ctrl_reg_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ctx *ctx = dev_get_drvdata(dev);
	int rc;
	unsigned int val = 0;

	rc = regmap_read(ctx->rmap, TPS63811_REG_CONTROL, &val);
	if (rc)
		dev_err(dev, "CONTROL reg read failed: %d\n", rc);

	return scnprintf(buf, PAGE_SIZE, "%02x\n", val);
}

static ssize_t ctrl_reg_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count) {
	struct ctx *ctx = dev_get_drvdata(dev);
	int rc;
	u8 val;

	if (!kstrtou8(buf, 0, &val)) {
		rc = regmap_write(ctx->rmap, TPS63811_REG_CONTROL, val);
		if (rc) {
			dev_err(ctx->dev,
				"Failed to write CONTROL reg : %d",
				rc);
			goto error;
		}
		dev_info(ctx->dev, "CONTROL reg = %02x\n", val);
	} else {
		dev_err(dev, "Invalid value: %s\n", buf);
	}

	return count;
error:
	return rc;
}

static ssize_t vout_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ctx->vout);
}

static ssize_t vout_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count) {
	struct ctx *ctx = dev_get_drvdata(dev);
	u32 vout;
	int rc;

	if (!kstrtou32(buf, 0, &vout)) {
		ctx->vout = vout;
		rc = set_vout_registers(ctx, vout);
		if (rc)
			goto error;
		dev_info(ctx->dev, "VOUT = %u\n", vout);
	} else {
		dev_err(dev, "Invalid value: %s\n", buf);
		rc = -EINVAL;
		goto error;
	}
	return count;
error:
	return rc;
}

static ssize_t manufacturer_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ctx->manufacturer);
}

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d.%d\n", ctx->major, ctx->minor);
}

static DEVICE_ATTR_RW(on);
static DEVICE_ATTR_RW(ctrl_reg);
static DEVICE_ATTR_RW(vout);
static DEVICE_ATTR_RO(manufacturer);
static DEVICE_ATTR_RO(version);

static struct attribute *attrs[] = {
	&dev_attr_on.attr,
	&dev_attr_ctrl_reg.attr,
	&dev_attr_vout.attr,
	&dev_attr_manufacturer.attr,
	&dev_attr_version.attr,
	NULL
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
	.name = "tps63811"
};

static int parse_dt(struct device *dev, struct ctx *ctx)
{
	struct device_node *of_node = dev->of_node;

	if (of_property_read_u32(of_node, "vout_mv", &ctx->vout))
		return -EINVAL;

	ctx->on_gpio = of_get_named_gpio(dev->of_node, "on-gpio", 0);

	if (!gpio_is_valid(ctx->on_gpio)) {
		dev_err(ctx->dev, "ON GPIO not valid");
		return -EINVAL;
	}

	return 0;
}

static int tps63811_probe(struct i2c_client *client,
			  const struct i2c_device_id *client_id)
{
	struct device *dev = &client->dev;
	int rc;
	unsigned int val;
	struct ctx *ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	rc = parse_dt(dev, ctx);
	if (rc)
		return rc;

	ctx->dev = dev;

	rc = devm_gpio_request(dev, ctx->on_gpio, "tps63811_on");
	if (rc) {
		dev_err(dev, "Failed to request TPS63811_on GPIO : %d\n", rc);
		return rc;
	}

	ctx->rfreg = devm_regulator_get(dev, "rf");
	if (IS_ERR_OR_NULL(ctx->rfreg)) {
		dev_err(dev, "Failed to get rf regulator : %d\n", rc);
		return rc;
	}

	rc = turn_on_gpio(ctx);

	ctx->rmap = devm_regmap_init_i2c(client, &tps63811_regmap_config);
	if (IS_ERR(ctx->rmap)) {
		rc = PTR_ERR(ctx->rmap);
		dev_err(dev, "regmap init failed: %d\n", rc);
		goto disable;
	}

	/* Check presence of i2c device */
	rc = regmap_read(ctx->rmap, TPS63811_REG_DEVID, &val);
	if (rc) {
		dev_err(dev, "DEVID read failed: %d\n", rc);
		goto disable;
	}

	ctx->manufacturer = TPS63811_GET_MANUFACTURER(val);
	ctx->major = TPS63811_GET_MAJOR(val);
	ctx->minor = TPS63811_GET_MINOR(val);

	if (ctx->manufacturer == MANUFACTURE_RICHTEK) {
		ctx->minMv = RICHTEK_VOUT_MIN_MV;
		ctx->maxMv = RICHTEK_VOUT_MAX_MV;
	} else {
		/* default to TI config */
		ctx->minMv = TPS63811_VOUT_MIN_MV;
		ctx->maxMv = TPS63811_VOUT_MAX_MV;
	}

	rc = turn_on_registers(ctx);
	if (rc)
		goto disable;

	rc = regulator_enable(ctx->rfreg);
	if (rc) {
		dev_err(dev, "Failed to enable rf regulator");
		goto disable;
	}

	rc = sysfs_create_group(&dev->kobj, &attr_group);
	if (rc)
		dev_warn(dev, "Failed to create an attr group: %d\n", rc);

	i2c_set_clientdata(client, ctx);

	dev_info(dev, "probed success");

	return 0;

disable:
	turn_off_gpio(ctx);

	return rc;
}

static const struct i2c_device_id tps63811_id[] = {
	{.name = "tps63811-bob",},
	{},
};
MODULE_DEVICE_TABLE(i2c, tps63811_id);

static struct i2c_driver tps63811_i2c_driver = {
	.driver = {
		.name = "tps63811-bob",
	},
	.probe = tps63811_probe,
	.id_table = tps63811_id,
};

module_i2c_driver(tps63811_i2c_driver);

MODULE_DESCRIPTION("tps63811 bob configurator");
MODULE_LICENSE("GPL v2");

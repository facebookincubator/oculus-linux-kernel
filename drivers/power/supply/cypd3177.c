// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>

/* Status Registers */
#define DEVICE_MODE	0x0000
#define SILICON_ID	0x0002
#define INTERRURT	0x0006	/* Interrupt for which INTR pin */
#define PD_STATUS	0x1008
#define TYPE_C_STATUS	0x100C
#define BUS_VOLTAGE	0x100D
#define CURRENT_PDO	0x1010
#define CURRENT_RDO	0x1014
#define SWAP_RESPONSE	0x1028
#define EVENT_STATUS	0x1044
#define READ_GPIO_LEVEL	0x0082
#define SAMPLE_GPIO	0x0083

/* Command Registers */
#define RESET		0x0008
#define EVENT_MASK	0x1024
#define DM_CONTROL	0x1000
#define SELECT_SINK_PDO	0x1005
#define PD_CONTROL	0x1006
#define REQUEST		0x1050
#define SET_GPIO_MODE	0x0080
#define SET_GPIO_LEVEL	0x0081

/* Response Registers */
#define DEV_RESPONSE	0x007E
#define PD_RESPONSE	0x1400

#define OUT_EN_L_BIT		BIT(0)	/* Port Partner Connection status */
#define CYPD3177_REG_8BIT	1
#define CYPD3177_REG_16BIT	2
#define CYPD3177_REG_32BIT	4
#define VOLTAGE_UNIT		100000	/* in 100000uv units */

struct cypd3177 {
	struct device		*dev;
	struct i2c_client	*client;
	struct power_supply	*psy;
	struct dentry		*dfs_root;
};

static int cypd3177_i2c_read_reg(struct i2c_client *client, u16 data_length,
			       u16 reg, u32 *val)
{
	int r;
	struct i2c_msg msg;
	u8 data[4];

	if (!client->adapter)
		return -ENODEV;
	if (data_length != CYPD3177_REG_8BIT &&
		data_length != CYPD3177_REG_16BIT &&
		data_length != CYPD3177_REG_32BIT)
		return -EINVAL;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;

	/* low byte goes out first */
	data[0] = (u8) (reg & 0xff);
	data[1] = (u8) (reg >> 8);

	r = i2c_transfer(client->adapter, &msg, 1);
	if (r < 0)
		goto err;

	msg.len = data_length;
	msg.flags = I2C_M_RD;
	r = i2c_transfer(client->adapter, &msg, 1);
	if (r < 0)
		goto err;

	*val = 0;
	/* high byte comes first */
	if (data_length == CYPD3177_REG_8BIT)
		*val = data[0];
	else if (data_length == CYPD3177_REG_16BIT)
		*val = (data[1] << 8) | data[0];
	else
		*val = (data[3] << 24) | (data[2] << 16) | (data[1] << 8)
			| data[0];

	return 0;

err:
	dev_err(&client->dev, "read from offset 0x%x error %d\n", reg, r);

	return r;
}

static void cypd3177_i2c_create_msg(struct i2c_client *client, u16 len, u16 reg,
				  u32 val, struct i2c_msg *msg, u8 *buf)
{
	msg->addr = client->addr;
	msg->flags = 0; /* Write */
	msg->len = 2 + len;
	msg->buf = buf;

	/* low byte goes out first */
	buf[0] = (u8) (reg & 0xff);
	buf[1] = (u8) (reg >> 8);

	switch (len) {
	case CYPD3177_REG_8BIT:
		buf[2] = (u8) (val) & 0xff;
		break;
	case CYPD3177_REG_16BIT:
		buf[2] = (u8) (val) & 0xff;
		buf[3] = (u8) (val >> 8) & 0xff;
		break;
	case CYPD3177_REG_32BIT:
		buf[2] = (u8) (val) & 0xff;
		buf[3] = (u8) (val >> 8) & 0xff;
		buf[4] = (u8) (val >> 16) & 0xff;
		buf[5] = (u8) (val >> 24) & 0xff;
		break;
	default:
		dev_err(&client->dev, "CYPD3177 : %s: invalid message length.\n",
			  __func__);
		break;
	}
}

static int cypd3177_i2c_write_reg(struct i2c_client *client, u16 data_length,
				u16 reg, u32 val)
{
	int r;
	struct i2c_msg msg;
	u8 data[6];

	if (!client->adapter)
		return -ENODEV;
	if (data_length != CYPD3177_REG_8BIT &&
		data_length != CYPD3177_REG_16BIT &&
		data_length != CYPD3177_REG_32BIT)
		return -EINVAL;

	cypd3177_i2c_create_msg(client, data_length, reg, val, &msg, data);

	r = i2c_transfer(client->adapter, &msg, 1);
	if (r < 0) {
		dev_err(&client->dev,
			"wrote 0x%x to offset 0x%x error %d\n", val, reg, r);
		return r;
	}

	return 0;
}

static int cypd3177_get_online(struct cypd3177 *chip, int *val)
{
	int rc;
	int stat;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
		TYPE_C_STATUS, &stat);
	if (rc < 0) {
		*val = 0;
		dev_dbg(&chip->client->dev, "cypd3177 is offline!\n");
		return 0;
	}

	*val = stat & OUT_EN_L_BIT;
	return rc;
}

static int cypd3177_get_voltage_now(struct cypd3177 *chip, int *val)
{
	int rc;
	int raw;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT, BUS_VOLTAGE,
		&raw);
	if (rc < 0) {
		*val = 0;
		dev_dbg(&chip->client->dev, "read cypd3177 voltage error.\n");
		return 0;
	}

	*val = raw * VOLTAGE_UNIT;
	return rc;
}

static int cypd3177_get_chip_version(struct cypd3177 *chip, int *val)
{
	int rc;
	int raw;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT, SILICON_ID,
		&raw);
	if (rc < 0) {
		*val = 0;
		dev_dbg(&chip->client->dev, "read cypd3177 chip version error.\n");
		return 0;
	}

	*val = raw;
	return rc;
}

static enum power_supply_property cypd3177_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHIP_VERSION,
};

static int cypd3177_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	int rc, *val = &pval->intval;
	struct cypd3177 *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		rc = cypd3177_get_online(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = cypd3177_get_voltage_now(chip, val);
		break;
	case POWER_SUPPLY_PROP_CHIP_VERSION:
		rc = cypd3177_get_chip_version(chip, val);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("property %d unavailable: %d\n", psp, rc);
		return -ENODATA;
	}

	return rc;
}

static const struct power_supply_desc cypd3177_psy_desc = {
	.name = "wireless",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = cypd3177_psy_props,
	.num_properties = ARRAY_SIZE(cypd3177_psy_props),
	.get_property = cypd3177_get_prop,
};

static int cypd3177_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	int rc;
	struct cypd3177 *chip;
	struct power_supply_config cfg = {0};
	struct device_node *np = i2c->dev.of_node;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "I2C functionality not supported\n");
		return -ENODEV;
	}
	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = i2c;
	chip->dev = &i2c->dev;
	i2c_set_clientdata(i2c, chip);

	/* Create PSY */
	cfg.drv_data = chip;
	cfg.of_node = chip->dev->of_node;

	chip->psy = devm_power_supply_register(chip->dev, &cypd3177_psy_desc,
			&cfg);
	if (IS_ERR(chip->psy)) {
		dev_err(&i2c->dev, "psy registration failed: %d\n",
				PTR_ERR(chip->psy));
		rc = PTR_ERR(chip->psy);

		return rc;
	}

	dev_dbg(&i2c->dev, "cypd3177 probe successful\n");
	return 0;
}

static int cypd3177_remove(struct i2c_client *i2c)
{
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "cy,cypd3177", },
	{ }
};

static struct i2c_driver cypd3177_driver = {
	.driver = {
		.name = "cypd3177-driver",
		.of_match_table = match_table,
	},
	.probe =	cypd3177_probe,
	.remove =	cypd3177_remove,
};

module_i2c_driver(cypd3177_driver);

MODULE_DESCRIPTION("CYPD3177 driver");
MODULE_LICENSE("GPL");


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
#include <linux/of_gpio.h>

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

/* PD RESPONSE EVENT */
#define TYPEC_CONNECT		0x84
#define TYPEC_DISCONNECT	0x85
#define PD_CONTRACT_COMPLETE	0x86
#define HARD_RESET		0x9A
#define DEV_EVENT		1
#define PD_EVENT		2

#define OUT_EN_L_BIT		BIT(0)	/* Port Partner Connection status */
#define CLEAR_DEV_INT		BIT(0)
#define CLEAR_PD_INT		BIT(1)
#define EVENT_MASK_OPEN		(BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(11))
#define CYPD3177_REG_8BIT	1
#define CYPD3177_REG_16BIT	2
#define CYPD3177_REG_32BIT	4
#define VOLTAGE_UNIT		100000	/* in 100000uv units */
#define PDO_BIT10_BIT19		0xffc00
#define PDO_VOLTAGE_UNIT	50000
#define RDO_BIT0_BIT9		0x3ff
#define RDO_CURRENT_UNIT	10
#define INTERRUPT_MASK		0x03
#define RESET_DEV		0x152
#define VOLTAGE_9V		9000000

struct cypd3177 {
	struct device		*dev;
	struct i2c_client	*client;
	struct power_supply	*psy;
	struct dentry		*dfs_root;
	int cypd3177_hpi_gpio;
	int cypd3177_fault_gpio;
	unsigned int hpi_irq;
	unsigned int fault_irq;
	bool typec_status;
	int pd_attach;
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
	int rc = 0;
	int stat;

	if (chip->typec_status) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
			TYPE_C_STATUS, &stat);
		if (rc < 0) {
			*val = 0;
			dev_err(&chip->client->dev, "cypd3177 i2c read type-c status error: %d\n",
				rc);
			return rc;
		}
		*val = stat & OUT_EN_L_BIT;
	} else
		*val = 0;

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

static int cypd3177_get_voltage_max(struct cypd3177 *chip, int *val)
{
	int rc = 0;
	int pdo_data;

	if (chip->pd_attach) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				CURRENT_PDO, &pdo_data);
		if (rc < 0) {
			dev_dbg(&chip->client->dev, "read cypd3177 PDO data failed: %d\n",
				rc);
			return rc;
		}
		*val = ((pdo_data & PDO_BIT10_BIT19) >> 10) * PDO_VOLTAGE_UNIT;
	} else
		*val = 0;

	return rc;
}

static int cypd3177_get_current_max(struct cypd3177 *chip, int *val)
{
	int rc = 0;
	int rdo_data;

	if (chip->pd_attach) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				CURRENT_RDO, &rdo_data);
		if (rc < 0) {
			dev_dbg(&chip->client->dev, "read cypd3177 RDO data failed: %d\n",
				rc);
			return rc;
		}
		*val = (rdo_data & RDO_BIT0_BIT9) * RDO_CURRENT_UNIT;
	} else
		*val = 0;

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

static int cypd3177_dev_reset(struct cypd3177 *chip)
{
	int rc;

	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_16BIT, RESET,
				RESET_DEV);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 reset failed: %d\n", rc);

	return rc;
}

static int dev_handle_response(struct cypd3177 *chip)
{
	int raw;
	int rc;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_16BIT,
				DEV_RESPONSE, &raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read dev response failed: %d\n",
			rc);
		return rc;
	}
	/* clear device corresponding bit */
	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_8BIT, INTERRURT,
				CLEAR_DEV_INT);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 write clear dev interrupt failed: %d\n",
			rc);
		return rc;
	}
	/* open event mask bit3 bit4 bit5 bit6 bit11 */
	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_32BIT,
				EVENT_MASK, EVENT_MASK_OPEN);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 write event mask failed: %d\n",
			rc);

	return rc;
}

static int pd_handle_response(struct cypd3177 *chip)
{
	int rc;
	int pd_raw, pd_event;
	int pdo_val;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				PD_RESPONSE, &pd_raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read pd response failed: %d\n",
			rc);
		return rc;
	}
	dev_dbg(&chip->client->dev, "cypd3177 pd_raw = 0x%x\n", pd_raw);

	pd_event = pd_raw & 0xFF;
	switch (pd_event) {
	case TYPEC_CONNECT:
		chip->typec_status = true;
		break;
	case PD_CONTRACT_COMPLETE:
		chip->pd_attach = 1;
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				CURRENT_PDO, &pd_raw);
		if (rc < 0) {
			dev_err(&chip->client->dev, "cypd3177 read pdo data failed: %d\n",
			rc);
			return rc;
		}
		pdo_val = ((pd_raw & PDO_BIT10_BIT19) >> 10) * PDO_VOLTAGE_UNIT;
		if (pdo_val == VOLTAGE_9V)
			power_supply_changed(chip->psy);
		break;
	case TYPEC_DISCONNECT:
	case HARD_RESET:
		chip->typec_status = false;
		chip->pd_attach = 0;
		break;
	default:
		break;
	}
	/* clear pd corresponding bit */
	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_8BIT,
				INTERRURT, CLEAR_PD_INT);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 write clear pd interrupt failed: %d\n",
			rc);

	return rc;
}

static irqreturn_t cypd3177_hpi_irq_handler(int irq, void *dev_id)
{
	struct cypd3177 *chip = dev_id;
	int rc;
	int irq_raw;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
				INTERRURT, &irq_raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read interrupt event failed: %d\n",
			rc);
		return rc;
	}
	/* mask interrupt event bit0 bit1 */
	irq_raw = irq_raw & INTERRUPT_MASK;

	switch (irq_raw) {
	case DEV_EVENT:
		dev_handle_response(chip);
		break;
	case PD_EVENT:
		pd_handle_response(chip);
		break;
	default:
		dev_err(&chip->client->dev, "cypd3177 read interrupt event error\n");
		cypd3177_dev_reset(chip);
		break;
	}

	return IRQ_HANDLED;
}

static irqreturn_t cypd3177_fault_irq_handler(int irq, void *dev_id)
{
	struct cypd3177 *chip = dev_id;

	chip->typec_status = false;
	chip->pd_attach = 0;

	dev_err(&chip->client->dev, "cypd3177 fault interrupt event ...\n");

	return IRQ_HANDLED;
}

static enum power_supply_property cypd3177_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHIP_VERSION,
	POWER_SUPPLY_PROP_PD_ACTIVE,
};

static int cypd3177_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	int rc = 0, *val = &pval->intval;
	struct cypd3177 *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		rc = cypd3177_get_online(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = cypd3177_get_voltage_now(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = cypd3177_get_voltage_max(chip, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = cypd3177_get_current_max(chip, val);
		break;
	case POWER_SUPPLY_PROP_CHIP_VERSION:
		rc = cypd3177_get_chip_version(chip, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		*val = chip->pd_attach;
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

	/* Get the cypd3177 hpi gpio and config it */
	chip->cypd3177_hpi_gpio = of_get_named_gpio(chip->dev->of_node,
						"cy,hpi-gpio", 0);
	if (!gpio_is_valid(chip->cypd3177_hpi_gpio)) {
		dev_err(&i2c->dev, "cypd3177 hpi gpio is invalid\n");
		return -EINVAL;
	}
	rc = devm_gpio_request_one(chip->dev, chip->cypd3177_hpi_gpio,
				GPIOF_IN, "cypd3177-hpi");
	if (rc) {
		dev_err(&i2c->dev, "cypd3177 request hpi gpio failed\n");
		return rc;
	}
	rc = devm_request_threaded_irq(chip->dev,
				gpio_to_irq(chip->cypd3177_hpi_gpio),
				NULL, cypd3177_hpi_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"cypd3177_hpi_irq", chip);
	if (rc) {
		dev_err(&i2c->dev, "cypd3177 failed to request hpi interrupt\n");
		return rc;
	}
	/* Get the cypd3177 fault gpio and config it */
	chip->cypd3177_fault_gpio = of_get_named_gpio(chip->dev->of_node,
						"cy,fault-gpio", 0);
	if (!gpio_is_valid(chip->cypd3177_fault_gpio)) {
		dev_err(&i2c->dev, "cypd3177 fault gpio is invalid\n");
		return -EINVAL;
	}
	rc = devm_gpio_request_one(chip->dev, chip->cypd3177_fault_gpio,
				GPIOF_IN, "cypd3177-fault");
	if (rc) {
		dev_err(&i2c->dev, "cypd3177 request fault gpio failed\n");
		return rc;
	}
	rc = devm_request_threaded_irq(chip->dev,
				gpio_to_irq(chip->cypd3177_fault_gpio),
				NULL, cypd3177_fault_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"cypd3177_fault_irq", chip);
	if (rc) {
		dev_err(&i2c->dev, "cypd3177 failed to request fault interrupt\n");
		return rc;
	}
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

	cypd3177_dev_reset(chip);

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


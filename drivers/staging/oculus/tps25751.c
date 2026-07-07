// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 The Linux Foundation. All rights reserved.
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/minmax.h>

#include "tps25751.h"

/* Register offsets */
#define TPS_REG_MODE			0x03
#define TPS_REG_TYPE			0x04
#define TPS_REG_CUSTUSE			0x06
#define TPS_REG_CMD1			0x08
#define TPS_REG_DATA1			0x09
#define TPS_REG_DEVICE_CAPABILITIES	0x0D
#define TPS_REG_VERSION			0x0F
#define TPS_REG_INT_EVENT1		0x14
#define TPS_REG_INT_MASK1		0x16
#define TPS_REG_INT_CLEAR1		0x18
#define TPS_REG_STATUS			0x1A
#define TPS_REG_POWER_PATH_STATUS	0x26
#define TPS_REG_PORT_CONTROL		0x29
#define TPS_REG_BOOT_STATUS		0x2D
#define TPS_REG_BUILD_DESCRIPTION	0x2E
#define TPS_REG_DEVICE_INFO		0x2F
#define TPS_REG_RX_SOURCE_CAPS		0x30
#define TPS_REG_RX_SINK_CAPS		0x31
#define TPS_REG_TX_SOURCE_CAPS		0x32
#define TPS_REG_TX_SINK_CAPS		0x33
#define TPS_REG_ACTIVE_CONTRACT_PDO	0x34
#define TPS_REG_ACTIVE_CONTRACT_RDO	0x35
#define TPS_REG_POWER_STATUS		0x3F
#define TPS_REG_PD_STATUS		0x40
#define TPS_REG_TYPEC_STATE		0x69
#define TPS_REG_GPIO_STATUS		0x72
#define TPS_REG_MAX			TPS_REG_GPIO_STATUS

#define TPS_MAX_LEN			64

enum {
	TPS_MODE_APP,
	TPS_MODE_BOOT,
	TPS_MODE_PTCH,
};

static const char *const modes[] = {
	[TPS_MODE_APP]	= "APP ",
	[TPS_MODE_BOOT]	= "BOOT",
	[TPS_MODE_PTCH]	= "PTCH",
};

struct tps25751 {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct power_supply *psy;
};

static const struct regmap_config tps25751_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = TPS_REG_MAX,
};

static enum power_supply_property tps25751_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int
tps25751_block_read(struct tps25751 *tps, u8 reg, void *val, size_t len)
{
	int ret;
	u8 data[TPS_MAX_LEN + 1];

	if (len + 1 > TPS_MAX_LEN)
		return -EINVAL;

	ret = regmap_raw_read(tps->regmap, reg, data, len + 1);
	if (ret)
		return ret;

	if (data[0] < len)
		return -EIO;

	memcpy(val, &data[1], len);
	return 0;
}

static int tps25751_read16(struct tps25751 *tps, u8 reg, u16 *val)
{
	return tps25751_block_read(tps, reg, val, sizeof(u16));
}

static int tps25751_read32(struct tps25751 *tps, u8 reg, u32 *val)
{
	return tps25751_block_read(tps, reg, val, sizeof(u32));
}

static int tps25751_get_current_max(struct tps25751 *tps, int *val)
{
	int ret;
	u32 buf = 0;
	u32 current_raw = 0;

	ret = tps25751_block_read(tps, TPS_REG_ACTIVE_CONTRACT_PDO,
				  &buf, sizeof(buf));
	if (ret)
		return ret;

	current_raw = TPS_PDO_MAX_CURRENT(buf);

	/* Current in 10mA units */
	*val = current_raw * 10;
	return ret;
}

static int tps25751_get_voltage_max(struct tps25751 *tps, int *val)
{
	int ret;
	u32 buf = 0;
	u32 voltage_raw = 0;

	ret = tps25751_block_read(tps, TPS_REG_ACTIVE_CONTRACT_PDO,
				  &buf, sizeof(buf));
	if (ret)
		return ret;

	voltage_raw = TPS_PDO_MAX_VOLTAGE_(buf);

	/* Voltage in 50mV units */
	*val = voltage_raw * 50;
	return ret;
}
static int tps25751_check_mode(struct tps25751 *tps)
{
	char mode[5] = { };
	int ret;

	ret = tps25751_read32(tps, TPS_REG_MODE, (void *)mode);
	if (ret)
		return ret;

	ret = match_string(modes, ARRAY_SIZE(modes), mode);

	switch (ret) {
	case TPS_MODE_APP:
		dev_dbg(tps->dev, "APP mode\n");
		return ret;
	case TPS_MODE_PTCH:
		dev_dbg(tps->dev, "PTCH mode\n");
		return ret;
	case TPS_MODE_BOOT:
		dev_warn(tps->dev, "dead-battery condition\n");
		return ret;
	default:
		dev_err(tps->dev, "controller in unsupported mode \"%s\"\n",
			mode);
		break;
	}

	return -ENODEV;
}

static int tps25751_psy_get_prop(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *pval)
{
	struct tps25751 *tps = power_supply_get_drvdata(psy);
	u16 pwr_status;
	int ret =  -EINVAL, *val = &pval->intval;

	ret = tps25751_read16(tps, TPS_REG_POWER_STATUS, &pwr_status);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pval->intval = TPS_REG_POWER_STATUS_POWER_CONNECTION(pwr_status);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = tps25751_get_current_max(tps, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = tps25751_get_voltage_max(tps, val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct power_supply_desc tps25751_psy_desc = {
	.name = "tps25751-psy",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = tps25751_psy_props,
	.num_properties = ARRAY_SIZE(tps25751_psy_props),
	.get_property = tps25751_psy_get_prop,
};

static int tps25751_probe(struct i2c_client *client)
{
	int ret;
	struct tps25751 *tps;
	struct power_supply_config psy_cfg = { };

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EPROTO;

	tps = devm_kzalloc(&client->dev, sizeof(*tps), GFP_KERNEL);
	if (!tps)
		return -ENOMEM;

	tps->client = client;
	tps->dev = &client->dev;

	tps->regmap = devm_regmap_init_i2c(client, &tps25751_regmap_config);
	if (IS_ERR(tps->regmap))
		return PTR_ERR(tps->regmap);

	ret = tps25751_check_mode(tps);
	if (ret < 0)
		return ret;

	psy_cfg.fwnode = dev_fwnode(tps->dev);
	psy_cfg.drv_data = tps;

	tps->psy = devm_power_supply_register(tps->dev,
					      &tps25751_psy_desc,
					      &psy_cfg);
	if (IS_ERR(tps->psy)) {
		dev_err(tps->dev, "psy registration failed: %ld\n",
				PTR_ERR(tps->psy));
		ret = PTR_ERR(tps->psy);
		return ret;
	}

	i2c_set_clientdata(client, tps);

	return 0;
}

static void tps25751_remove(struct i2c_client *client)
{
	return ;
}

static const struct of_device_id tps25751_of_match[] = {
	{ .compatible = "ti,tps25751", },
	{}
};
MODULE_DEVICE_TABLE(of, tps25751_of_match);

static const struct i2c_device_id tps25751_id[] = {
	{ "tps25751" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tps25751_id);

static struct i2c_driver tps25751_i2c_driver = {
	.driver = {
		.name = "tps25751",
		.of_match_table = of_match_ptr(tps25751_of_match),
	},
	.probe_new = tps25751_probe,
	.remove = tps25751_remove,
	.id_table = tps25751_id,
};
module_i2c_driver(tps25751_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI TPS25751 Power Delivery Controller Driver");

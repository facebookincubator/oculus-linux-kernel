/*
 * STMicroelectronics hts221 i2c driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include "hts221.h"

#define I2C_AUTO_INCREMENT	0x80

static int hts221_i2c_read(struct device *dev, u8 addr, int len, u8 *data)
{
	struct i2c_msg msg[2];
	struct i2c_client *client = to_i2c_client(dev);

	if (len > 1)
		addr |= I2C_AUTO_INCREMENT;
	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &addr;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	return i2c_transfer(client->adapter, msg, 2);
}

static int hts221_i2c_write(struct device *dev, u8 addr, int len, u8 *data)
{
	u8 send[len + 1];
	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(dev);

	if (len > 1)
		addr |= I2C_AUTO_INCREMENT;
	send[0] = addr;
	memcpy(&send[1], data, len * sizeof(u8));

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len + 1;
	msg.buf = send;

	return i2c_transfer(client->adapter, &msg, 1);
}

static const struct hts221_transfer_function hts221_transfer_fn = {
	.read = hts221_i2c_read,
	.write = hts221_i2c_write,
};

static int hts221_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int err;
	struct hts221_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	i2c_set_clientdata(client, dev);
	dev->name = client->name;
	dev->dev = &client->dev;
	dev->irq = client->irq;
	dev->tf = &hts221_transfer_fn;

	err = hts221_probe(dev);
	if (err < 0) {
		kfree(dev);
		return err;
	}

	dev_info(&client->dev, "hts221 i2c sensor probed\n");

	return 0;
}

static int hts221_i2c_remove(struct i2c_client *client)
{
	int err;
	struct hts221_dev *dev = i2c_get_clientdata(client);

	err = hts221_remove(dev);
	if (err < 0)
		return err;

	dev_info(&client->dev, "hts221 i2c sensor removed\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hts221_i2c_of_match[] = {
	{ .compatible = "st,hts221", },
	{},
};
MODULE_DEVICE_TABLE(of, hts221_i2c_of_match);
#endif /* CONFIG_OF */

static const struct i2c_device_id hts221_i2c_id_table[] = {
	{ HTS221_DEV_NAME },
	{},
};
MODULE_DEVICE_TABLE(i2c, hts221_i2c_id_table);

static struct i2c_driver hts221_driver = {
	.driver = {
		.name = "hts221_i2c",
#ifdef CONFIG_OF
		.of_match_table = hts221_i2c_of_match,
#endif /* CONFIG_OF */
	},
	.probe = hts221_i2c_probe,
	.remove = hts221_i2c_remove,
	.id_table = hts221_i2c_id_table,
};
module_i2c_driver(hts221_driver);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics hts221 i2c driver");
MODULE_LICENSE("GPL v2");

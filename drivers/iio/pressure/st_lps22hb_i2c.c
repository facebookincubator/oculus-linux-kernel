/*
 * STMicroelectronics lps22hb driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Armando Visconti <armando.visconti@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/types.h>

#include "st_lps22hb_core.h"

static int lps22hb_i2c_read(struct lps22hb_data *cdata, u8 reg_addr, int len,
								u8 * data)
{
	int err = 0;
	struct i2c_msg msg[2];
	struct i2c_client *client = to_i2c_client(cdata->dev);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);

	return err;
}

static int lps22hb_i2c_write(struct lps22hb_data *cdata, u8 reg_addr, int len,
								u8 * data)
{
	int err = 0;
	u8 send[len + 1];
	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(cdata->dev);

	send[0] = reg_addr;
	memcpy(&send[1], data, len * sizeof(u8));
	len++;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = send;

	err = i2c_transfer(client->adapter, &msg, 1);

	return err;
}

static const struct lps22hb_transfer_function lps22hb_tf_i2c = {
	.write = lps22hb_i2c_write,
	.read = lps22hb_i2c_read,
};

static int lps22hb_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int err;
	struct lps22hb_data *cdata;

	cdata = kmalloc(sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	cdata->dev = &client->dev;
	cdata->name = client->name;
	cdata->tf = &lps22hb_tf_i2c;
	i2c_set_clientdata(client, cdata);

	err = lps22hb_common_probe(cdata, client->irq);
	if (err < 0)
		goto free_data;

	return 0;

free_data:
	kfree(cdata);
	return err;
}

static int lps22hb_i2c_remove(struct i2c_client *client)
{
	struct lps22hb_data *cdata = i2c_get_clientdata(client);

	lps22hb_common_remove(cdata, client->irq);
	dev_info(cdata->dev, "%s: removed\n", LPS22HB_DEV_NAME);
	kfree(cdata);
	return 0;
}

#ifdef CONFIG_PM
static int lps22hb_suspend(struct device *dev)
{
	struct lps22hb_data *cdata = i2c_get_clientdata(to_i2c_client(dev));

	return lps22hb_common_suspend(cdata);
}

static int lps22hb_resume(struct device *dev)
{
	struct lps22hb_data *cdata = i2c_get_clientdata(to_i2c_client(dev));

	return lps22hb_common_resume(cdata);
}

static const struct dev_pm_ops lps22hb_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lps22hb_suspend, lps22hb_resume)
};

#define LPS22HB_PM_OPS		(&lps22hb_pm_ops)
#else /* CONFIG_PM */
#define LPS22HB_PM_OPS		NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id lps22hb_ids[] = {
	{"lps22hb", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lps22hb_ids);

#ifdef CONFIG_OF
static const struct of_device_id lps22hb_id_table[] = {
	{.compatible = "st,lps22hb",},
	{},
};

MODULE_DEVICE_TABLE(of, lps22hb_id_table);
#endif /* CONFIG_OF */

static struct i2c_driver lps22hb_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = LPS22HB_DEV_NAME,
		   .pm = LPS22HB_PM_OPS,
#ifdef CONFIG_OF
		   .of_match_table = lps22hb_id_table,
#endif /* CONFIG_OF */
		   },
	.probe = lps22hb_i2c_probe,
	.remove = lps22hb_i2c_remove,
	.id_table = lps22hb_ids,
};

module_i2c_driver(lps22hb_i2c_driver);

MODULE_DESCRIPTION("STMicroelectronics lps22hb i2c driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_LICENSE("GPL v2");

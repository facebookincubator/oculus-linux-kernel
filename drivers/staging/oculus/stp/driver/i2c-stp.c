// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Meta Platforms, Inc. and affiliates.
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <device/stp_device.h>
#include "stp-interface.h"
#include "stp_driver.h"

#define I2C_DRV_NAME "i2c_stp"
#define I2C_ADAPTER_RETRIES 5
#define I2C_ADAPTER_TIMEOUT_MSEC 5000
#define STP_MAX_PROBE_RETRIES 5

static uint inject_bad;
module_param(inject_bad, uint, 0644);
static uint inject_bad_size = 4;
module_param(inject_bad_size, uint, 0644);

struct i2c_stp {
	struct device *dev;
	struct i2c_adapter adap;

	struct device_node *stp_interface_node;

	uint16_t bind_fail_count;
	uint16_t bind_success_count;
};

static int stp_probe_fails;

static int i2c_stp_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			int num)
{
	int ret;
	int left = num;
	int payload_len;
	struct i2c_stp *i2c;

	i2c = i2c_get_adapdata(adap);

	dev_dbg(i2c->dev, "%s: enter num=%d\n", __func__, num);

	while (left > 0) {
		int stp_msg_len;
		uint16_t reg_addr;
		uint8_t mode_read;
		struct i2c_msg *msg;
		struct stp_interface_msg_header *header;
		struct stp_interface_msg_payload *payload;

		msg = &msgs[num - left];

		/* TODO: handle 10 bits addressing */
		if (WARN_ON(msg->flags & I2C_M_TEN)) {
			ret = -EINVAL;
			break;
		}

		if (!(msg->flags & I2C_M_RD) && msg->len == 1) {
			/*
			 * This is a transaction for slave address write,
			 * which to be followed by a read. The buf contains
			 * register's address only. Gather the address only.
			 */
			if (WARN_ON(left == 0 || !(msg[1].flags & I2C_M_RD))) {
				dev_err(i2c->dev,
					"unexpected i2c_msgs, left=%d msg next flags=0x%x\n",
					left, msg[1].flags);
				ret = -EINVAL;
				break;
			}

			reg_addr = *msg->buf;
			mode_read = 1;
			left--;
			msg++;
		} else {
			/*
			 * Either write or read from the reg address noted with
			 * previous i2c_msg. A write msg contains the reg
			 * address at the first byte.
			 */
			mode_read = !!(msg->flags & I2C_M_RD);
			if (!mode_read)
				reg_addr = *msg->buf;
		}

		payload_len = sizeof(*payload);
		if (!mode_read)
			payload_len += msg->len;
		stp_msg_len = sizeof(*header) + payload_len;

		header = kmalloc(stp_msg_len, GFP_KERNEL);
		if (!header) {
			ret = -ENOMEM;
			break;
		}

		header->csum = 0;
		header->payload_len = cpu_to_le16(payload_len);

		payload = (struct stp_interface_msg_payload *)&header[1];
		payload->flags = PAYLOAD_TYPE_DATALEN;
		payload->dev_addr = cpu_to_le16(msg->addr);
		payload->reg_addr = cpu_to_le16(reg_addr);
		payload->mode_read = mode_read;
		payload->u.datalen = cpu_to_le16(msg->len);
		if (!mode_read)
			memcpy(&payload->data, msg->buf, msg->len);

		ret = stp_interface_xfer(i2c->stp_interface_node, msg->buf,
					 header, payload, stp_msg_len,
					 I2C_SMBUS_BLOCK_MAX,
					 STP_INTERFACE_I2C_MSG_MAGIC);

		if (ret) {
			goto err_exit;
		}
		left--;
	}

err_exit:
	dev_dbg(i2c->dev, "%s: exit ret=%d\n", __func__, ret);

	if (ret)
		return ret;

	return num;
}

static u32 i2c_stp_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm i2c_stp_algo = {
	.master_xfer = i2c_stp_xfer,
	.functionality = i2c_stp_functionality,
};

static int i2c_stp_probe(struct platform_device *pdev)
{
	int ret;
	struct i2c_stp *i2c;

	dev_info(&pdev->dev, "%s: i2c_stp_probe enter\n", __func__);

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c) {
		ret = -ENOMEM;
		goto err_exit_nomem;
	}

	i2c->stp_interface_node =
		of_parse_phandle(pdev->dev.of_node, "stp-interface", 0);
	if (!i2c->stp_interface_node) {
		dev_err(&pdev->dev,
			"%s: unable to get stp-interface device_node\n",
			__func__);
		ret = -EPROBE_DEFER;
		goto err_exit_no_phandle;
	}

	i2c->adap.algo = &i2c_stp_algo;
	i2c->adap.timeout = msecs_to_jiffies(I2C_ADAPTER_TIMEOUT_MSEC);
	i2c->adap.retries = I2C_ADAPTER_RETRIES;
	i2c->adap.dev.of_node = pdev->dev.of_node;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.owner = THIS_MODULE;
	strlcpy(i2c->adap.name, I2C_DRV_NAME, sizeof(i2c->adap.name));
	i2c->dev = &pdev->dev;

	i2c_set_adapdata(&i2c->adap, i2c);

	ret = i2c_add_adapter(&i2c->adap);
	if (ret) {
		dev_err(&pdev->dev, "%s: i2c adapter add failed ret=%d\n",
			 __func__, ret);
		goto err_exit;
	} else {
		dev_info(&pdev->dev, "%s: i2c adapter added\n", __func__);
	}

	platform_set_drvdata(pdev, i2c);

	stp_probe_fails = 0;
	dev_info(&pdev->dev, "%s: i2c_stp_probe exit\n", __func__);

	return 0;

err_exit:
	of_node_put(i2c->stp_interface_node);
err_exit_no_phandle:
err_exit_nomem:
	dev_err(&pdev->dev, "%s: i2c_stp_probe exit with error ret=%d\n",
		__func__, ret);

	return ret;
}

static int i2c_stp_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct i2c_stp *i2c;

	dev_info(&pdev->dev, "%s: enter i2c_stp_remove\n", __func__);

	i2c = platform_get_drvdata(pdev);

	of_node_put(i2c->stp_interface_node);

	i2c_del_adapter(&i2c->adap);

	dev_info(&pdev->dev, "%s: done i2c_stp_remove ret=%d\n", __func__, ret);

	return ret;
}

static const struct of_device_id i2c_stp_of_match[] = {
	{ .compatible = "meta,i2c-stp", .data = NULL },
	{}
};

static struct platform_driver
	i2c_stp_driver = { .probe = i2c_stp_probe,
			   .remove = i2c_stp_remove,
			   .driver = {
				   .name = I2C_DRV_NAME,
				   .of_match_table =
					   of_match_ptr(i2c_stp_of_match),
			   } };

static int __init i2c_stp_init(void)
{
	int ret;
	ret = platform_driver_register(&i2c_stp_driver);
	return ret;
}
module_init(i2c_stp_init);

static void __exit i2c_stp_exit(void)
{
	platform_driver_unregister(&i2c_stp_driver);
}
module_exit(i2c_stp_exit);

MODULE_DEVICE_TABLE(of, i2c_stp_of_match);
MODULE_LICENSE("GPL v2");

// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#include "mcu_thermistor.h"
#include "../../stp/driver/stp-interface.h"

int mcu_thermistor_get_stp_interface(struct mcu_thermistor_data *data)
{
	data->stp_interface_node = of_parse_phandle(data->dev->of_node, "stp-interface", 0);
	if (!data->stp_interface_node) {
		dev_err(data->dev, "%s: unable to get stp-interface device_node\n", __func__);
		return -ENOENT;
	}
	return 0;
}

int mcu_thermistor_xfer(struct mcu_thermistor_data *data, int *val)
{
	int ret = 0;
	int payload_len = 0;
	int stp_msg_len = 0;
	struct stp_interface_msg_header *header = NULL;
	struct stp_interface_msg_payload *payload = NULL;
	uint8_t *buf = NULL;

	payload_len = sizeof(*payload);
	stp_msg_len = sizeof(*header) + payload_len;

	if (!data->stp_interface_node) {
		ret = mcu_thermistor_get_stp_interface(data);
		if (ret)
			goto err_exit;
	}

	header = kmalloc(stp_msg_len, GFP_KERNEL);
	if (!header) {
		ret = -ENOMEM;
		goto err_exit;
	}

	buf = kmalloc(MCU_THERMISTOR_MAX_MESSAGE_SIZE_BYTES, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_exit;
	}

	header->csum = 0;
	header->payload_len = cpu_to_le16(payload_len);

	payload = (struct stp_interface_msg_payload *)&header[1];
	payload->flags = PAYLOAD_TYPE_DATALEN;
	payload->dev_addr = cpu_to_le16(0);
	payload->reg_addr = cpu_to_le16(0);
	payload->mode_read = 1;
	payload->u.datalen = cpu_to_le16(MCU_THERMISTOR_MAX_MESSAGE_SIZE_BYTES);

	ret = stp_interface_xfer(data->stp_interface_node,
				 buf,
				 header,
				 payload,
				 stp_msg_len,
				 MCU_THERMISTOR_MAX_MESSAGE_SIZE,
				 MCU_THERMISTOR_MAGIC);
	if (ret) {
		dev_err(data->dev, "stp_interface_xfer failed: %d\n", ret);
		goto err_exit_buf;
	}

	memcpy(val, buf, MCU_THERMISTOR_MAX_MESSAGE_SIZE_BYTES);

err_exit_buf:
	kfree(buf);
err_exit:
	return ret;
}

static int mcu_thermistor_get_temp_mc(void *data, int *state)
{
	int ret = 0;
	struct mcu_thermistor_tz_data *tz_data = NULL;
	struct mcu_thermistor_data *dev_data = NULL;

	if (!data || !state)
		return -EINVAL;

	tz_data = (struct mcu_thermistor_tz_data *)data;
	if (!tz_data)
		return -EINVAL;

	dev_data = (struct mcu_thermistor_data *)tz_data->data;
	if (!dev_data)
		return -EINVAL;

	ret = mcu_thermistor_xfer(dev_data, state);
	if (ret)
		return ret;

	return ret;
}

static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mcu_thermistor_data *data = dev_get_drvdata(dev);
	int temperature = 0;
	int ret = 0;

	if (!data)
		return -EINVAL;

	ret = mcu_thermistor_xfer(data, &temperature);
	if (ret) {
		dev_err(dev, "%s: Failed to read temperature: %d\n", __func__, ret);
		return ret;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", temperature);
}
static DEVICE_ATTR_RO(temperature);

static struct thermal_zone_of_device_ops thermal_ops = {
	.get_temp = mcu_thermistor_get_temp_mc,
};

static int mcu_thermistor_init_thermal_zone(struct platform_device *pdev,
					    struct mcu_thermistor_data *data)
{
	int ret = 0;
	struct mcu_thermistor_tz_data *tz_data = NULL;
	
	if (!pdev || !data)
		return -EINVAL;

	tz_data = devm_kzalloc(&pdev->dev, sizeof(*tz_data), GFP_KERNEL);
	if (!tz_data) {
		ret = -ENOMEM;
		return ret;
	}

	tz_data->data = data;

	tz_data->tzd = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, tz_data, &thermal_ops);
	if (IS_ERR(tz_data->tzd)) {
		ret = PTR_ERR(tz_data->tzd);
		dev_err(&pdev->dev, "%s: Failed to register thermal zone: %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int mcu_thermistor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mcu_thermistor_data *data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	int ret = 0;

	if (!data) {
		return -ENOMEM;
	}

	data->dev = dev;
	platform_set_drvdata(pdev, data);

	data->stp_interface_node = of_parse_phandle(pdev->dev.of_node, "stp-interface", 0);
	if (!data->stp_interface_node) {
		dev_err(&pdev->dev, "%s: unable to get stp-interface device_node\n", __func__);
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	ret = device_create_file(dev, &dev_attr_temperature);
	if (ret) {
		dev_err(dev, "%s: failed to create temperature sysfs node (%d)\n", __func__, ret);
		goto err_exit;
	}

	ret = mcu_thermistor_init_thermal_zone(pdev, data);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to init thermal zone\n", __func__);
		goto err_exit;
	}

	dev_info(dev, "mcu_thermistor successfully probed\n");
	return 0;

err_exit:
	if (data->stp_interface_node)
		of_node_put(data->stp_interface_node);
	dev_err(dev, "%s: Failed to probe mcu_thermistor: %d\n", __func__, ret);
	return ret;
}

static int mcu_thermistor_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mcu_thermistor_data *data = platform_get_drvdata(pdev);

	device_remove_file(dev, &dev_attr_temperature);
	if (data->stp_interface_node)
		of_node_put(data->stp_interface_node);
	return 0;
}

static const struct of_device_id mcu_thermistor_match_table[] = {
	{
		.compatible = "meta,mcu_thermistor",
	},
	{},
};

static struct platform_driver mcu_thermistor_driver = {
	.probe = mcu_thermistor_probe,
	.remove = mcu_thermistor_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mcu_thermistor_match_table),
	},
};

static int __init mcu_thermistor_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&mcu_thermistor_driver);
	if (rc) {
		pr_err("Failed to register mcu_thermistor driver: %d\n", rc);
	}
	return rc;
}

static void __exit mcu_thermistor_exit(void)
{
	platform_driver_unregister(&mcu_thermistor_driver);
}
module_init(mcu_thermistor_init);
module_exit(mcu_thermistor_exit);

MODULE_DESCRIPTION("MCU Thermistor Driver");
MODULE_LICENSE("GPL");

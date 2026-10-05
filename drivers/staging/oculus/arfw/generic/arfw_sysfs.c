// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_sysfs.c
 *
 * @brief Implementation of AR Firmware sysfs nodes.
 *
 * @details Generic layer sysfs nodes should be initialized and removed in this file. 
 *
 *******************************************************************************/

#include "arfw_sysfs.h"

#include <linux/module.h>
#include <linux/device.h>
#include <linux/sysfs.h>

static ssize_t released_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct arfw_char_device *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->released);
}

static ssize_t references_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct arfw_char_device *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->references);
}

static DEVICE_ATTR_RO(released);
static DEVICE_ATTR_RO(references);

int add_arfw_device_sysfs_nodes(struct arfw_char_device *device)
{
	int err;

	err = device_create_file(device->dev, &dev_attr_released);
	if (err)
		return err;

	err = device_create_file(device->dev, &dev_attr_references);
	if (err)
		goto remove_released;

	return 0;

remove_released:
	device_remove_file(device->dev, &dev_attr_released);
	return err;
}

void remove_arfw_device_sysfs_nodes(struct arfw_char_device *device)
{
	device_remove_file(device->dev, &dev_attr_released);
	device_remove_file(device->dev, &dev_attr_references);
}

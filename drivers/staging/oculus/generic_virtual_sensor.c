/*
 * Implements a generic virtual sensor using the following equation
 * temperature = offset + a0*t0 + a1*t1 + ... an*tn
 * Where offset is a static offset, an is an integer coefficient and tn
 * is the thermal zone (temperature sensor) temperature.
 *
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/idr.h>

struct sensor_zone_data {
	struct list_head entry;
	struct thermal_zone_device *tz;
	char zone_name[8];
	char coeff_name[15];
	struct device_attribute zone;
	struct device_attribute coeff;
	int coefficient;
	int id;
};

static LIST_HEAD(thermal_zone_list);

struct generic_virtual_sensor_data {
	struct device *dev;
	struct thermal_zone_device *tz;
	struct idr id;
	int offset;
};

static int get_temp(void *data, int *state)
{
	struct generic_virtual_sensor_data *vsd =
		(struct generic_virtual_sensor_data *) data;
	struct sensor_zone_data *zone = NULL;
	int ret = 0;
	int temp = 0;
	int calculation = 0;

	if (!vsd || !vsd->tz)
		return -EINVAL;

	if (vsd->tz->mode == THERMAL_DEVICE_DISABLED) {
		dev_err(vsd->dev, "thermal zone is disabled");
		return -EPERM;
	}

	list_for_each_entry(zone, &thermal_zone_list, entry) {
		ret = thermal_zone_get_temp(zone->tz, &temp);
		if (ret) {
			dev_err(vsd->dev, "couldn't read thermal zone %s", zone->tz->type);
			return -EINVAL;
		}
		dev_dbg(vsd->dev, "tz: %s, temp: %d, coeff: %d\n",
				zone->tz->type, temp, zone->coefficient);
		calculation += (zone->coefficient * temp);
	}

	calculation /= 1000;
	calculation += vsd->offset;
	*state = calculation;

	return 0;
}

static struct thermal_zone_of_device_ops generic_virtual_sensor_thermal_ops = {
	.get_temp = get_temp,
};

ssize_t zone_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_zone_data *data =
		container_of(attr, struct sensor_zone_data, zone);

	return sysfs_emit(buf, "%s\n", data->tz->type);
}

ssize_t coeff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sensor_zone_data *data =
		container_of(attr, struct sensor_zone_data, coeff);

	return sysfs_emit(buf, "%d\n", data->coefficient);
}

ssize_t coeff_store(struct device *dev, struct device_attribute *attr,
		     const char *coeff, size_t count)
{
	struct sensor_zone_data *data =
		container_of(attr, struct sensor_zone_data, coeff);
	struct generic_virtual_sensor_data *vsd = (struct generic_virtual_sensor_data *)
		dev_get_drvdata(dev);

	if (vsd->tz->mode != THERMAL_DEVICE_DISABLED) {
		dev_err(vsd->dev, "thermal zone is not disabled");
		return -EPERM;
	}

	if (kstrtoint(coeff, 10, &data->coefficient)) {
		dev_err(dev, "couldn't parse coefficient");
		return -EINVAL;
	}

	return count;
}

static int add_zone(struct generic_virtual_sensor_data *data, const char* name,
		int coefficient)
{
	struct thermal_zone_device *tz = NULL;
	struct sensor_zone_data *new_zone = NULL;

	tz = thermal_zone_get_zone_by_name(name);
	if (IS_ERR(tz)) {
		dev_err(data->dev, "thermal zone %s not found", name);
		return -EINVAL;
	}

	new_zone = kzalloc(sizeof(*new_zone), GFP_KERNEL);
	if (!new_zone)
		return -ENOMEM;

	new_zone->tz = tz;
	new_zone->id = idr_alloc(&data->id, new_zone, 0, 50, GFP_KERNEL);
	new_zone->coefficient = coefficient;
	list_add_tail(&new_zone->entry, &thermal_zone_list);

	// update sysfs
	sprintf(new_zone->coeff_name, "coefficient_%d", new_zone->id);
	sysfs_attr_init(&new_zone->coeff.attr);
	new_zone->coeff.attr.name = new_zone->coeff_name;
	new_zone->coeff.attr.mode = 0644;
	new_zone->coeff.show = coeff_show;
	new_zone->coeff.store = coeff_store;

	sprintf(new_zone->zone_name, "zone_%d", new_zone->id);
	sysfs_attr_init(&new_zone->zone.attr);
	new_zone->zone.attr.name = new_zone->zone_name;
	new_zone->zone.attr.mode = 0444;
	new_zone->zone.show = zone_show;

	if (device_create_file(data->dev, &new_zone->coeff)) {
		dev_err(data->dev, "couldn't create device file");
		list_del(&new_zone->entry);
		kfree(new_zone);
		return -EINVAL;
	}

	if (device_create_file(data->dev, &new_zone->zone)) {
		dev_err(data->dev, "couldn't create device file");
		list_del(&new_zone->entry);
		kfree(new_zone);
		return -EINVAL;
	}

	return 0;
}


static void remove_zone(struct generic_virtual_sensor_data *data, struct thermal_zone_device *tz)
{
	struct list_head *p, *n;
	struct sensor_zone_data *temp;

	list_for_each_safe(p, n, &thermal_zone_list) {
		temp = list_entry(p, struct sensor_zone_data, entry);
		if (temp->tz == tz) {
			device_remove_file(data->dev, &temp->zone);
			device_remove_file(data->dev, &temp->coeff);
			list_del(&temp->entry);
			idr_remove(&data->id, temp->id);
			kfree(temp);
			return;
		}
	}
}

static ssize_t add_zone_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *name, size_t count)
{
	struct generic_virtual_sensor_data *vsd = (struct generic_virtual_sensor_data *)
		dev_get_drvdata(dev);

	if (vsd->tz->mode != THERMAL_DEVICE_DISABLED) {
		dev_err(vsd->dev, "thermal zone is not disabled");
		return -EPERM;
	}

	if (count > THERMAL_NAME_LENGTH) {
		dev_err(vsd->dev, "thermal zone %s invalid", name);
		return -EINVAL;
	}

	if (add_zone(vsd, name, 0)) {
		dev_err(vsd->dev, "couldn't add thermal zone");
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_WO(add_zone);

static ssize_t remove_zone_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *name, size_t count)
{
	struct generic_virtual_sensor_data *vsd = (struct generic_virtual_sensor_data *)
		dev_get_drvdata(dev);
	struct thermal_zone_device *tz = NULL;

	if (vsd->tz->mode != THERMAL_DEVICE_DISABLED) {
		dev_err(vsd->dev, "thermal zone is not disabled");
		return -EPERM;
	}

	if (count > THERMAL_NAME_LENGTH) {
		dev_err(vsd->dev, "thermal zone %s invalid", name);
		return -EINVAL;
	}

	tz = thermal_zone_get_zone_by_name(name);
	if (IS_ERR(tz)) {
		dev_err(vsd->dev, "thermal zone %s not found", name);
		return -EINVAL;
	}

	remove_zone(vsd, tz);

	return count;
}
static DEVICE_ATTR_WO(remove_zone);


static ssize_t offset_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct generic_virtual_sensor_data *vsd = (struct generic_virtual_sensor_data *)
		dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", vsd->offset);
}
static ssize_t offset_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *offset, size_t count)
{
	struct generic_virtual_sensor_data *vsd = (struct generic_virtual_sensor_data *)
		dev_get_drvdata(dev);

	if (vsd->tz->mode != THERMAL_DEVICE_DISABLED) {
		dev_err(vsd->dev, "thermal zone is not disabled");
		return -EPERM;
	}

	if (kstrtoint(offset, 10, &vsd->offset)) {
		dev_err(vsd->dev, "couldn't read input");
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(offset);

static struct attribute *generic_virtual_sensor_attrs[] = {
	&dev_attr_add_zone.attr,
	&dev_attr_remove_zone.attr,
	&dev_attr_offset.attr,
	NULL,
};
ATTRIBUTE_GROUPS(generic_virtual_sensor);

static const struct of_device_id generic_virtual_sensor_table[] = {
	{ .compatible = "oculus,generic-virtual-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, generic_virtual_sensor_table);

static int populate_from_device_tree(struct device_node *np,
		struct generic_virtual_sensor_data *vsd)
{
	struct device_node *child = of_get_child_by_name(np, "zones");
	struct device_node *iter = NULL;
	const char *thermal_zone_name;
	int offset = 0;
	int ret = 0;

	if (!child) {
		ret = -EINVAL;
		goto error;
	}

	ret = of_property_read_s32(np, "offset", &offset);
	if (!ret) {
		vsd->offset = offset;
	} else {
		dev_err(vsd->dev, "invalid offset");
		goto error;
	}

	dev_info(vsd->dev, "offset is: %d\n", vsd->offset);

	for_each_child_of_node(child, iter) {
		int32_t coeff = 0;

		ret = of_property_read_string(iter, "type",
				&thermal_zone_name);
		if (ret) {
			dev_err(vsd->dev, "invalid thermal zone name");
			goto error;
		}

		ret = of_property_read_s32(iter, "coefficient", &coeff);
		if (ret) {
			dev_err(vsd->dev, "invalid coefficient for %s",
					thermal_zone_name);
			goto error;
		}

		ret = add_zone(vsd, thermal_zone_name, coeff);
		if (ret) {
			dev_err(vsd->dev, "couldn't add thermal zone %s",
					thermal_zone_name);
			goto error;
		}
		dev_info(vsd->dev, "coeff for %s name is: %d\n",
				thermal_zone_name, coeff);
	}

error:
	of_node_put(iter);
	of_node_put(child);
	of_node_put(np);

	return ret;
}

static int generic_virtual_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct generic_virtual_sensor_data *vsd = NULL;
	struct thermal_zone_device *tz = NULL;

	vsd = devm_kzalloc(&pdev->dev, sizeof(*vsd), GFP_KERNEL);
	if (!vsd) {
		dev_err(&pdev->dev, "power state failed to allocate vs\n");
		return -ENOMEM;
	}

	vsd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, vsd);
	tz = thermal_zone_of_sensor_register(&pdev->dev,
					      0,
					      vsd,
					      &generic_virtual_sensor_thermal_ops);
	if (IS_ERR(tz)) {
		dev_err(&pdev->dev, "failed to register thermal zone\n");
		return PTR_ERR(tz);
	}
	vsd->tz = tz;
	ret = sysfs_create_groups(&pdev->dev.kobj,
			generic_virtual_sensor_groups);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to create sysfs files\n");
		goto therm_err;
	}

	// link the virtual sensor sysfs to thermal zone for convenience
	ret = sysfs_create_link(&tz->device.kobj,
			&pdev->dev.kobj, pdev->name);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to link sysfs files\n");
		goto sysfs_err;
	}

	// init ids used for zone_x identifiers
	idr_init(&vsd->id);

	populate_from_device_tree(pdev->dev.of_node, vsd);

	dev_info(&pdev->dev, "probe complete\n");

	return ret;

sysfs_err:
	sysfs_remove_groups(&pdev->dev.kobj,
			    generic_virtual_sensor_groups);
therm_err:
	thermal_zone_of_sensor_unregister(&pdev->dev, vsd->tz);

	return ret;
}

static int generic_virtual_sensor_remove(struct platform_device *pdev)
{
	struct generic_virtual_sensor_data *vsd =
		(struct generic_virtual_sensor_data *)
		dev_get_drvdata(&pdev->dev);

	struct list_head *p, *n;
	struct sensor_zone_data *temp;

	list_for_each_safe(p, n, &thermal_zone_list) {
		temp = list_entry(p, struct sensor_zone_data, entry);
		device_remove_file(vsd->dev, &temp->zone);
		device_remove_file(vsd->dev, &temp->coeff);
		list_del(&temp->entry);
		idr_remove(&vsd->id, temp->id);
		kfree(temp);
	}
	sysfs_remove_link(&vsd->tz->device.kobj, pdev->name);
	sysfs_remove_groups(&pdev->dev.kobj,
			    generic_virtual_sensor_groups);
	thermal_zone_of_sensor_unregister(&pdev->dev, vsd->tz);

	return 0;
}

static struct platform_driver generic_virtual_sensor_driver = {
	.probe = generic_virtual_sensor_probe,
	.remove = generic_virtual_sensor_remove,
	.driver = {
		.name = "generic-virtual-sensor",
		.of_match_table = generic_virtual_sensor_table,
	},
};

static int __init generic_virtual_sensor_init(void)
{
	return platform_driver_register(&generic_virtual_sensor_driver);
}
late_initcall(generic_virtual_sensor_init);

static void __exit generic_virtual_sensor_deinit(void)
{
	platform_driver_unregister(&generic_virtual_sensor_driver);
}
module_exit(generic_virtual_sensor_deinit);

MODULE_ALIAS("generic_virtual_sensor");
MODULE_LICENSE("Proprietary");

// SPDX-License-Identifier: GPL-2.0-only
/*******************************************************************************
 * @file power_state_virtual_sensor.c
 *
 * @brief Implements a peak power state driver as a virtual sensor for
 *        integration with thermal HAL in hammerhead.
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include <linux/err.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/types.h>

#include "power_state_virtual_sensor.h"

struct power_state_virtual_sensor_data {
	struct device *dev;
	struct thermal_zone_device *tzd;
	struct power_supply *sn_battery;
	TempPowerPolicy *policyTable;
};

static int last_battery_capacity = 0;  // dd charge level
static int last_battery_temp = 0; // temp in tenths of degrees C (100 = 10.0)
static int last_power_state = HH_POWER_OK;

// used for power management support
static atomic_t in_suspend;

static int get_battery_temp(struct power_state_virtual_sensor_data *vs,
			    int *temp)
{
	int ret = 0;
	union power_supply_propval val;

	dev_dbg(vs->dev, "power state: battery temp\n");

	ret = power_supply_get_property(vs->sn_battery,
		POWER_SUPPLY_PROP_TEMP,
		&val);
	if (ret < 0) {
		dev_err(vs->dev, "Error getting battery temp\n");
		return ret;
	}

	last_battery_temp = val.intval / 10;
	*temp = last_battery_temp;
	dev_dbg(vs->dev, "power state: battery temp done %d\n",
		 last_battery_temp);

	return ret;
}

static int get_battery_capacity(struct power_state_virtual_sensor_data *vs,
			   int *capacity)
{
	int ret = 0;

	union power_supply_propval val;

	dev_dbg(vs->dev, "power state: battery capacity\n");

	ret = power_supply_get_property(vs->sn_battery,
		POWER_SUPPLY_PROP_CAPACITY,
		&val);
	if (ret < 0) {
		dev_err(vs->dev, "Error getting battery capacity\n");
		return ret;
	}

	// capacity is reported in whole %ages
	last_battery_capacity = val.intval;
	*capacity = last_battery_capacity;
	dev_dbg(vs->dev, "power state: battery capacity done %d!\n",
		 last_battery_capacity);

	return ret;
}

static int get_temp(void *data, int *state)
{
	int ret = 0;
	struct power_state_virtual_sensor_data *vsdata =
		(struct power_state_virtual_sensor_data *)data;
	int current_temp = 0;
	int current_capacity = 0;
	int tindex, pindex;
	TempPowerPolicy *policy;
	int *policyBuckets;

	if (!vsdata || !vsdata->dev) {
		return -EINVAL;
	}

	if (!vsdata->sn_battery)
		vsdata->sn_battery = power_supply_get_by_name("max17332-battery");

	// handle suspend state by returning last value
	if (atomic_read(&in_suspend) || !vsdata->sn_battery) {
		*state = last_power_state;
		return ret;
	}

	dev_dbg(vsdata->dev, "power state: get_temp\n");

	// Check whether we support this platform
	if (of_property_read_bool(vsdata->dev->of_node, "meta,pmanagement-disable")) {
		dev_dbg(vsdata->dev, "%s: Suspend disable property is set \n",
			__func__);
		*state = HH_POWER_OK;
		return 0;
	}

	if ((ret = get_battery_temp(vsdata, &current_temp)) < 0) {
		dev_err(vsdata->dev,
			"power state: error getting battery temp\n");
		return ret;
	}

	if ((ret = get_battery_capacity(vsdata, &current_capacity)) < 0) {
		dev_err(vsdata->dev,
			"power state: error getting battery capacity\n");
		return ret;
	}

	// default is OK
	*state = HH_POWER_OK;
	// below 0 we use 0, higher than 25, we use 25
	if (current_temp < 0) {
	  current_temp = 0;
	}
	policy = vsdata->policyTable;

	for(tindex=0; tindex<NUM_TEMP_BUCKETS; tindex++) {
		if (current_temp >= policy[tindex].policyArray[0]) {
			for(pindex=1; pindex<2*(policy[tindex].numPolicyBuckets); pindex+=2) {
				policyBuckets = policy[tindex].policyArray;
				if(current_capacity <= policyBuckets[pindex]) {
					*state = policyBuckets[pindex+1];
					break;
				}
			}
			break;
		}
	}

	last_power_state = *state;
	dev_dbg(vsdata->dev, "power state get_temp returning %d\n", *state);

	return ret;
}

static struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
	.get_temp = get_temp,
};

// sysfs support

static ssize_t last_capacity_read(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", last_battery_capacity);
}

static ssize_t last_capacity_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf,
				  unsigned long count)
{
	int ret = 0;
	dev_info(NULL, "power state store with value=%s count=%lu\n", buf, count);

	ret = kstrtoint(buf, 10, &last_battery_capacity);
	if (ret) {
		dev_err(NULL, "power state: couldn't parse store value");
		return ret;
	}
	return count;
}

static ssize_t last_temp_read(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", last_battery_temp);
}

static ssize_t last_temp_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				unsigned long count)
{
	int ret = 0;
	dev_info(NULL, "power state store with value=%s count=%lu\n", buf, count);

	ret = kstrtoint(buf, 10, &last_battery_temp);
	if (ret) {
		dev_err(NULL, "power state: couldn't parse store value");
		return ret;
	}

	return count;
}

static ssize_t last_state_read(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", last_power_state);
}

static ssize_t last_state_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf,
				  unsigned long count)
{
	int ret = 0;
	dev_info(NULL, "power state store with value=%s count=%lu\n", buf, count);

	ret = kstrtoint(buf, 10, &last_power_state);
	if (ret) {
		dev_err(NULL, "power state: couldn't parse store value");
		return ret;
	}
	return count;
}

static struct kobj_attribute last_battery_capacity_attr =
	__ATTR(last_battery_capacity, 0644, last_capacity_read, last_capacity_store);

static struct kobj_attribute last_battery_temp_attr =
	__ATTR(last_battery_temp, 0644, last_temp_read, last_temp_store);

static struct kobj_attribute last_power_state_attr =
	__ATTR(last_power_state, 0644, last_state_read, last_state_store);

static struct attribute *power_state_virtual_sensor_attrs[] = {
	&last_battery_capacity_attr.attr,
	&last_battery_temp_attr.attr,
	&last_power_state_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(power_state_virtual_sensor);

static int power_state_pm_notify(struct notifier_block *nb,
				 unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		atomic_set(&in_suspend, 1);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&in_suspend, 0);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block power_state_pm_nb = {
	.notifier_call = power_state_pm_notify,
};


static int virtual_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct power_state_virtual_sensor_data *vs = NULL;
	struct thermal_zone_device *tzd = NULL;

	dev_dbg(&pdev->dev, "power-state probing\n");

	vs = devm_kzalloc(&pdev->dev, sizeof(*vs), GFP_KERNEL);
	if (!vs) {
		dev_err(&pdev->dev, "power state failed to allocate vs\n");
		return -ENOMEM;
	}
	dev_dbg(&pdev->dev, "power state allocated local data\n");

	// Initialize power policy table
	vs->policyTable = devm_kzalloc(&pdev->dev,
				       NUM_TEMP_BUCKETS * sizeof(TempPowerPolicy),
				       GFP_KERNEL);
	if (!vs->policyTable) {
		dev_err(&pdev->dev, "power state, failed to create policy table\n");
		return -ENOMEM;
	}
	// Initialize the buckets
	vs->policyTable[0].numPolicyBuckets = sizeof(PeakPowerPolicy25)/(2*sizeof(int));
	vs->policyTable[0].policyArray = PeakPowerPolicy25;
	vs->policyTable[1].numPolicyBuckets = sizeof(PeakPowerPolicy15)/(2*sizeof(int));
	vs->policyTable[1].policyArray = PeakPowerPolicy15;
	vs->policyTable[2].numPolicyBuckets = sizeof(PeakPowerPolicy10)/(2*sizeof(int));
	vs->policyTable[2].policyArray = PeakPowerPolicy10;
	vs->policyTable[3].numPolicyBuckets = sizeof(PeakPowerPolicy5)/(2*sizeof(int));
	vs->policyTable[3].policyArray = PeakPowerPolicy5;
	vs->policyTable[4].numPolicyBuckets = sizeof(PeakPowerPolicy0)/(2*sizeof(int));
	vs->policyTable[4].policyArray = PeakPowerPolicy0;

	vs->dev = &pdev->dev;
	// save the device data structure
	dev_set_drvdata(&pdev->dev, vs);

	// Register the thermal zone
	tzd = thermal_zone_of_sensor_register(&pdev->dev,
					      0,
					      vs,
					      &virtual_sensor_thermal_ops);
	if (IS_ERR(tzd)) {
		dev_err(&pdev->dev, "Failed to register power state device\n");
		return PTR_ERR(tzd);
	}

	dev_dbg(&pdev->dev, "power state: thermal zone registered\n");

	// store the thermal zone device in data
	vs->tzd = tzd;

	// Create sysfs groups
	ret = sysfs_create_groups(&pdev->dev.kobj,
				  power_state_virtual_sensor_groups);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"power state Failed to create sysfs files\n");
		return ret;
	}

	// configure power management support
	atomic_set(&in_suspend, 0);
	register_pm_notifier(&power_state_pm_nb);

	dev_info(&pdev->dev, "power-state probe done successfully!\n");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct power_state_virtual_sensor_data *vs =
		(struct power_state_virtual_sensor_data *)
		dev_get_drvdata(&pdev->dev);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);
	sysfs_remove_groups(&pdev->dev.kobj,
			    power_state_virtual_sensor_groups);

	unregister_pm_notifier(&power_state_pm_nb);
	/* vs doesn't need to be freed since we use devm_kzalloc */

	return 0;
}

static const struct of_device_id virtual_sensor_table[] = {
	{ .compatible = "oculus,power-state-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, virtual_sensor_table);

static struct platform_driver virtual_sensor_driver = {
	.probe = virtual_sensor_probe,
	.remove = virtual_sensor_remove,
	.driver = {
		.name = "power-state-sensor",
		.of_match_table = virtual_sensor_table,
	},
};

static int __init virtual_sensor_init(void)
{
	return platform_driver_register(&virtual_sensor_driver);
}
late_initcall(virtual_sensor_init);

static void __exit virtual_sensor_deinit(void)
{
	platform_driver_unregister(&virtual_sensor_driver);
}
module_exit(virtual_sensor_deinit);

MODULE_ALIAS("power_state_virtual_sensor");
MODULE_LICENSE("GPL v2");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/types.h>

static const struct virtual_sensor_data virtual_sensors_kona[] = {
	{
		.virt_zone_name = "vrapi-front-panel",
		.num_sensors = 2,
		.sensor_names = {"cpuss-0-usr", "cpuss-1-usr"},
		.logic = VIRT_WEIGHTED_AVG,
		.coefficient_ct = 2,
		.coefficients = {50, 50},
		.avg_denominator = 100,
	},
};

static int front_panel_sensor_probe(struct platform_device *pdev)
{
	int idx = 0;
	struct thermal_zone_device *tz = NULL;

	dev_dbg(&pdev->dev, "probing");

	/* TODO(T65363888): Make this configurable based on OF match data */
	for (idx = 0; idx < ARRAY_SIZE(virtual_sensors_kona); idx++) {
		tz = devm_thermal_of_virtual_sensor_register(&pdev->dev,
				&virtual_sensors_kona[idx]);
		if (IS_ERR(tz))
			dev_err(&pdev->dev, "sensor %s register error: %ld\n",
				virtual_sensors_kona[idx].virt_zone_name,
				PTR_ERR(tz));
		else
			dev_info(&pdev->dev, "sensor %s registered\n",
				virtual_sensors_kona[idx].virt_zone_name);
	}

	return 0;
}

static int front_panel_sensor_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id front_panel_sensor_table[] = {
	{ .compatible = "oculus,front-panel-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, front_panel_sensor_table);

static struct platform_driver front_panel_sensor_driver = {
	.probe = front_panel_sensor_probe,
	.remove = front_panel_sensor_remove,
	.driver = {
		.name = "front-panel-sensor",
		.of_match_table = front_panel_sensor_table,
	},
};

static int __init front_panel_sensor_init(void)
{
	pr_info("%s: Initializing\n", __func__);
	return platform_driver_register(&front_panel_sensor_driver);
}
subsys_initcall(front_panel_sensor_init);

static void __exit front_panel_sensor_deinit(void)
{
	platform_driver_unregister(&front_panel_sensor_driver);
}
module_exit(front_panel_sensor_deinit);

MODULE_ALIAS("front_panel_sensor");
MODULE_LICENSE("GPL v2");

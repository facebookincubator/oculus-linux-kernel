// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/scmi_protocol.h>
#include <linux/scmi_bwprof.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

extern int cpucp_bwprof_init(struct scmi_device *sdev);

static int scmi_bwprof_probe(struct scmi_device *sdev)
{
	if (!sdev)
		return -ENODEV;

	return cpucp_bwprof_init(sdev);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ .protocol_id = SCMI_PROTOCOL_BWPROF, .name = "scmi_protocol_bwprof" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_bwprof_drv = {
	.name		= "scmi-bwprof-driver",
	.probe		= scmi_bwprof_probe,
	.id_table	= scmi_id_table,
};
module_scmi_driver(scmi_bwprof_drv);

MODULE_SOFTDEP("pre: bwprof_vendor");
MODULE_DESCRIPTION("ARM SCMI bwprof driver");
MODULE_LICENSE("GPL");

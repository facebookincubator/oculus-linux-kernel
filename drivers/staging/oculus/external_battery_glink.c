// SPDX-License-Identifier: GPL-2.0

#include <linux/of.h>
#include <linux/of_platform.h>

#include "external_battery.h"
#include "vdm_glink.h"

void *vdm_glink_dev;

int external_battery_register_svid_handler(struct ext_batt_pd *pd)
{
	struct device_node *dev_node;
	struct platform_device *pdev;

	if (!pd || !pd->dev || !pd->dev->of_node) {
		dev_err(pd->dev, "invalid arg\n");
		return -EINVAL;
	}

	dev_node = of_parse_phandle(pd->dev->of_node, "vdm-glink", 0);
	if (!dev_node) {
		dev_err(pd->dev, "failed to get glink node\n");
		return -ENXIO;
	}

	pdev = of_find_device_by_node(dev_node);
	of_node_put(dev_node);
	if (!pdev) {
		dev_err(pd->dev, "failed to get pdev");
		return -EPROBE_DEFER;
	}

	vdm_glink_dev = dev_get_drvdata(&pdev->dev);

	if (!vdm_glink_dev) {
		dev_err(pd->dev, "failed to get vdm_glink_dev");
		return -ENXIO;
	}

	return vdm_glink_register_handler(vdm_glink_dev, &(pd->vdm_handler));
}

int external_battery_unregister_svid_handler(struct ext_batt_pd *pd)
{
	(void) pd;
	return vdm_glink_unregister_handler(vdm_glink_dev);
}

int external_battery_send_vdm(struct ext_batt_pd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	if (!vdm_glink_dev) {
		dev_err(pd->dev, "error sending vdm, vdm_glink_dev undefined");
		return -EINVAL;
	}

	return vdm_glink_send_vdm(vdm_glink_dev, vdm_hdr, vdos, num_vdos);
}

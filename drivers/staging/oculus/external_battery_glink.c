// SPDX-License-Identifier: GPL-2.0

#include <linux/of.h>
#include <linux/of_platform.h>

#include "external_battery.h"
#include "vdm_glink.h"

struct ext_batt_pd *ext_batt_pd;
struct glink_svid_handler vdm_handler;
struct vdm_glink_dev *vdm_glink_dev;

static void vdm_connect(struct glink_svid_handler *hdlr, bool usb_comm);
static void vdm_disconnect(struct glink_svid_handler *hdlr);
static void vdm_received(struct glink_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos);

static int get_vdm_glink_dev(struct ext_batt_pd *pd,
		struct vdm_glink_dev **udev);

int external_battery_register_svid_handler(struct ext_batt_pd *pd)
{
	int result;
	struct vdm_glink_dev *udev;

	if (!pd || !pd->dev)
		return -EINVAL;
	ext_batt_pd = pd;

	result = get_vdm_glink_dev(pd, &udev);
	if (result)
		return result;

	vdm_handler.svid = pd->svid;
	vdm_handler.connect = vdm_connect;
	vdm_handler.disconnect = vdm_disconnect;
	vdm_handler.vdm_received = vdm_received;

	return vdm_glink_register_handler(udev, &vdm_handler);
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

/* SVID Handler Callbacks */

static void vdm_connect(struct glink_svid_handler *hdlr, bool usb_comm)
{
	ext_batt_vdm_connect(ext_batt_pd, usb_comm);
}

static void vdm_disconnect(struct glink_svid_handler *hdlr)
{
	ext_batt_vdm_disconnect(ext_batt_pd);
}

static void vdm_received(struct glink_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	ext_batt_vdm_received(ext_batt_pd, vdm_hdr, vdos, num_vdos);
}

/* Helper functions */

static int get_vdm_glink_dev(struct ext_batt_pd *pd,
		struct vdm_glink_dev **udev)
{
	struct device_node *dev_node;
	struct platform_device *pdev;

	if (vdm_glink_dev) {
		*udev = vdm_glink_dev;
		return 0;
	}

	if (!pd->dev->of_node) {
		dev_err(pd->dev, "missing of_node");
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
		dev_err_ratelimited(pd->dev, "failed to get pdev");
		return -EPROBE_DEFER;
	}

	*udev = dev_get_drvdata(&pdev->dev);
	if (!(*udev)) {
		dev_err(pd->dev, "failed to get vdm_glink_dev");
		return -EPROBE_DEFER;
	}

	vdm_glink_dev = *udev;
	return 0;
}

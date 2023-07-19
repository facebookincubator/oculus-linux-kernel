// SPDX-License-Identifier: GPL-2.0

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/usb/usbpd.h>

#include "external_battery.h"

/* Data specific to the usbpd platform */
struct ext_batt_usbpd_pd {
	/* usbpd protocol engine handle */
	struct usbpd *upd;
};

static struct ext_batt_usbpd_pd pd_usbpd;

int external_battery_register_svid_handler(struct ext_batt_pd *pd)
{
	if (!pd || !pd->dev) {
		dev_err(pd->dev, "invalid arg\n");
		return -EINVAL;
	}

	pd_usbpd.upd = devm_usbpd_get_by_phandle(pd->dev, "battery-usbpd");
	if (IS_ERR_OR_NULL(pd_usbpd.upd))
		return -EPROBE_DEFER;

	return usbpd_register_svid(pd_usbpd.upd, &pd->vdm_handler);
}

int external_battery_unregister_svid_handler(struct ext_batt_pd *pd)
{
	if (pd_usbpd.upd != NULL)
		usbpd_unregister_svid(pd_usbpd.upd, &pd->vdm_handler);

	return 0;
}

int external_battery_send_vdm(struct ext_batt_pd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	(void) pd;
	return usbpd_send_vdm(pd_usbpd.upd, vdm_hdr, vdos, num_vdos);
}

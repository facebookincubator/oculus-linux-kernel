// SPDX-License-Identifier: GPL-2.0

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/usb/usbpd.h>

#include "external_battery.h"

/* Data specific to the usbpd platform */
struct ext_batt_usbpd_pd {
	/* Data needed for the usbpd kernel API */
	struct usbpd *upd;
	struct usbpd_svid_handler vdm_handler;

	struct ext_batt_pd *pd;
};

static struct ext_batt_usbpd_pd pd_usbpd;

static void vdm_connect(struct usbpd_svid_handler *hdlr, bool usb_comm);
static void vdm_disconnect(struct usbpd_svid_handler *hdlr);
static void vdm_received(struct usbpd_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos);

int external_battery_register_svid_handler(struct ext_batt_pd *pd)
{
	if (!pd || !pd->dev)
		return -EINVAL;
	pd_usbpd.pd = pd;

	pd_usbpd.upd = devm_usbpd_get_by_phandle(pd->dev, "battery-usbpd");
	if (IS_ERR_OR_NULL(pd_usbpd.upd))
		return -EPROBE_DEFER;

	pd_usbpd.vdm_handler.svid = pd->svid;
	pd_usbpd.vdm_handler.connect = vdm_connect;
	pd_usbpd.vdm_handler.disconnect = vdm_disconnect;
	pd_usbpd.vdm_handler.vdm_received = vdm_received;

	return usbpd_register_svid(pd_usbpd.upd, &pd_usbpd.vdm_handler);
}

int external_battery_unregister_svid_handler(struct ext_batt_pd *pd)
{
	if (pd_usbpd.upd != NULL)
		usbpd_unregister_svid(pd_usbpd.upd, &pd_usbpd.vdm_handler);

	return 0;
}

int external_battery_send_vdm(struct ext_batt_pd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	(void) pd;
	return usbpd_send_vdm(pd_usbpd.upd, vdm_hdr, vdos, num_vdos);
}

/* SVID Handler Callbacks */

static void vdm_connect(struct usbpd_svid_handler *hdlr, bool usb_comm)
{
	ext_batt_vdm_connect(pd_usbpd.pd, usb_comm);
}

static void vdm_disconnect(struct usbpd_svid_handler *hdlr)
{
	ext_batt_vdm_disconnect(pd_usbpd.pd);
}

static void vdm_received(struct usbpd_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	ext_batt_vdm_received(pd_usbpd.pd, vdm_hdr, vdos, num_vdos);
}

/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VDM_GLINK_H__
#define _VDM_GLINK_H__

#include <linux/usb/usbpd.h>

#if IS_ENABLED(CONFIG_OCULUS_VDM_GLINK)

int vdm_glink_send_vdm(void *udev, u32 vdm_hdr, const u32 *vdos, u32 num_vdos);
int vdm_glink_register_handler(void *udev, struct usbpd_svid_handler *handler);
int vdm_glink_unregister_handler(void *udev);

#else

static int vdm_glink_send_vdm(void *udev, u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	return -EINVAL;
}

static int vdm_glink_register_handler(void *udev, struct usbpd_svid_handler *handler)
{
	return -EINVAL;
}
static int vdm_glink_unregister_handler(void *udev)
{
	return -EINVAL;
}

#endif

#endif

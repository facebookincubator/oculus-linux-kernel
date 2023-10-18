/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VDM_GLINK_H__
#define _VDM_GLINK_H__

/*
 * Implemented by client
 *
 * Loosely mimics struct usbpd_svid_handler
 */
struct glink_svid_handler {
	u16 svid;
	u16 pid;

	/* Notified when VDM session established/reset; must be implemented */
	void (*connect)(struct glink_svid_handler *hdlr,
			bool supports_usb_comm);
	void (*disconnect)(struct glink_svid_handler *hdlr);

	/* Unstructured VDM */
	void (*vdm_received)(struct glink_svid_handler *hdlr, u32 vdm_hdr,
			const u32 *vdos, int num_vdos);

	/* client should leave these blank; private members used by Glink driver */
	struct list_head entry;
};

struct vdm_glink_dev;

#if IS_ENABLED(CONFIG_OCULUS_VDM_GLINK)

int vdm_glink_send_vdm(struct vdm_glink_dev *udev,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos);
int vdm_glink_register_handler(struct vdm_glink_dev *udev,
		struct glink_svid_handler *handler);
void vdm_glink_unregister_handler(struct vdm_glink_dev *udev,
		struct glink_svid_handler *hdlr);

struct vdm_glink_dev *devm_vdm_glink_get_by_phandle(struct device *dev,
		const char *phandle);

#else

static int vdm_glink_send_vdm(struct vdm_glink_dev *udev,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	return -EINVAL;
}

static int vdm_glink_register_handler(struct vdm_glink_dev *udev,
		struct glink_svid_handler *handler)
{
	return -EINVAL;
}
static void vdm_glink_unregister_handler(struct vdm_glink_dev *udev,
		struct glink_svid_handler *hdlr)
{
	return;
}

static struct vdm_glink_dev *devm_vdm_glink_get_by_phandle(struct device *dev,
		const char *phandle)
{
	return ERR_PTR(-EINVAL);
}

#endif

#endif

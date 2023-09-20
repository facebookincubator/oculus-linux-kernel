/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __USBVDM_H
#define __USBVDM_H

/* Vendor Defined Object Section */
#define VDOS_MAX_BYTES 16

/* Vendor Defined Objects Macros */
#define VDO_ACK_STATUS(v) (v & 0xFF)

/* Vendor Defined Message Header Macros */
#define VDMH_SVID(h) ((h) >> 16)
#define VDMH_PARAMETER(h) (h & 0xFF)
#define VDMH_SIZE(h) ((h & 0x700) >> 8)
#define VDMH_HIGH(h) ((h & 0x800) >> 11)
#define VDMH_PROTOCOL(h) ((h & 0x3000) >> 12)
#define VDMH_ACK(h) ((h & 0x4000) >> 14)
#define VDMH_CONSTRUCT(svid, ack, proto, high, size, param) \
	((0xFFFF0000 & (svid << 16)) | \
	(0X4000 & (ack << 14)) | (0X3000 & (proto << 12)) | \
	(0X0800 & (high << 11)) | (0X0700 & (size << 8)) | \
	(0xFF & param))

/* VDM protocol types */
enum vdm_protocol_type {
	VDM_BROADCAST = 0,
	VDM_REQUEST,
	VDM_RESPONSE,
};

static const size_t vdm_size_bytes[] = { 1, 2, 4, 8, 16, 32 };

/* Product categories of first-party accessories (with UPAs considered docks) */
enum vdm_product_type {
	VDM_PRODUCT_TYPE_CHARGING_DOCK,
	VDM_PRODUCT_TYPE_EXTERNAL_BATTERY,
};

/* USB Vendor IDs used by first-party devices */
enum vdm_svid {
	VDM_SVID_FACEBOOK = 0x2ec6,
	VDM_SVID_META = 0x2833,
};

/* USB Product IDs used by first-party accessories */
enum vdm_pid {
	/* Charging docks */
	VDM_PID_BURU = 0x2202,
	VDM_PID_SKELLIG = 0x0093,
	VDM_PID_MAUI = 0x5000,
	VDM_PID_UPA_18W = 0x2200,
	VDM_PID_UPA_45W = 0x2103,
	/* External batteries */
	VDM_PID_MOLOKINI = 0xfc60,
	VDM_PID_LEHUA = 0x5001,
};

#endif /* __USBVDM_H */

/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __USBVDM_H
#define __USBVDM_H

/* Vendor Defined Object Section */
#define VDOS_MAX_BYTES 16

/* Vendor Defined Objects Macros */
#define VDO_ACK_STATUS(v) (v & 0xFF)

/* Vendor Defined Message Header Macros */
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

#endif /* __USBVDM_H */

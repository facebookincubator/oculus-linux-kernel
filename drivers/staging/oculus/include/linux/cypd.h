/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_CYPD_H__
#define _LINUX_CYPD_H__

#include <linux/device.h>

struct cypd;
struct device;

/* Structured VDM Commands */
#define CYPD_SVDM_DISCOVER_IDENTITY	0x1
#define CYPD_SVDM_DISCOVER_SVIDS	0x2
#define CYPD_SVDM_DISCOVER_MODES	0x3
#define CYPD_SVDM_ENTER_MODE		0x4
#define CYPD_SVDM_EXIT_MODE		0x5
#define CYPD_SVDM_ATTENTION		0x6

/* Structured VDM Command Type */
enum cypd_svdm_cmd_type {
	SVDM_CMD_TYPE_INITIATOR,
	SVDM_CMD_TYPE_RESP_ACK,
	SVDM_CMD_TYPE_RESP_NAK,
	SVDM_CMD_TYPE_RESP_BUSY,
};

/*
 * Implemented by client
 */
struct cypd_svid_handler {
	u16 svid;

	/* Notified when VDM session established/reset; must be implemented */
	void (*connect)(struct cypd_svid_handler *hdlr);
	void (*disconnect)(struct cypd_svid_handler *hdlr);

	/* Unstructured VDM */
	void (*vdm_received)(struct cypd_svid_handler *hdlr, u32 vdm_hdr,
			const u32 *vdos, int num_vdos);

	/* Structured VDM */
	void (*svdm_received)(struct cypd_svid_handler *hdlr, u8 cmd,
			enum cypd_svdm_cmd_type cmd_type, const u32 *vdos,
			int num_vdos);

	/* client should leave these blank; private members used by PD driver */
	struct list_head entry;
	bool discovered;
};

#if IS_ENABLED(CONFIG_CYPD_POLICY_ENGINE)
struct cypd *devm_cypd_get_by_phandle(struct device *dev,
		const char *phandle);
/*
 * Transmit a VDM message.
 */
int cypd_send_vdm(struct cypd *pd, u32 vdm_hdr, const u32 *vdos,
		int num_vdos);
/*
 * Transmit a Structured VDM message.
 */
int cypd_send_svdm(struct cypd *pd, u16 svid, u8 cmd,
		enum cypd_svdm_cmd_type cmd_type, int obj_pos,
		const u32 *vdos, int num_vdos);
/*
 * Invoked by client to handle specific SVID messages.
 * Specify callback functions in the cypd_svid_handler argument
 */
int cypd_register_svid(struct cypd *pd, struct cypd_svid_handler *hdlr);
void cypd_unregister_svid(struct cypd *pd, struct cypd_svid_handler *hdlr);
#else
static struct cypd *devm_cypd_get_by_phandle(struct device *dev,
		const char *phandle)
{
	return ERR_PTR(-ENODEV);
}
static int cypd_send_vdm(struct cypd *pd, u32 vdm_hdr, const u32 *vdos,
		int num_vdos)
{
	return -EINVAL;
}
static int cypd_send_svdm(struct cypd *pd, u16 svid, u8 cmd,
		enum cypd_svdm_cmd_type cmd_type, int obj_pos,
		const u32 *vdos, int num_vdos)
{
	return -EINVAL;
}
static int cypd_register_svid(struct cypd *pd,
		struct cypd_svid_handler *hdlr)
{
	return -EINVAL;
}
static void cypd_unregister_svid(struct cypd *pd,
		struct cypd_svid_handler *hdlr)
{
}
#endif
#endif /* _LINUX_CYPD_H__ */

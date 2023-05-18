/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _CYPD_H__
#define _CYPD_H__

#include <linux/device.h>

struct cypd;
struct device;

/* Standard IDs */
#define CYPD_SID			0xff00

/* Structured VDM Commands */
#define CYPD_SVDM_DISCOVER_IDENTITY	0x1
#define CYPD_SVDM_DISCOVER_SVIDS	0x2
#define CYPD_SVDM_DISCOVER_MODES	0x3
#define CYPD_SVDM_ENTER_MODE		0x4
#define CYPD_SVDM_EXIT_MODE		0x5
#define CYPD_SVDM_ATTENTION		0x6

enum data_role {
	DR_NONE = -1,
	DR_UFP = 0,
	DR_DFP = 1,
};

enum power_role {
	PR_NONE = -1,
	PR_SINK = 0,
	PR_SRC = 1,
};

enum pd_spec_rev {
	PD_REV_20 = 1,
	PD_REV_30 = 2,
};

enum pd_sop_type {
	SOP_MSG = 0,
	SOPI_MSG,
	SOPII_MSG,
};

enum cypd3177_iio_prop {
	CYPD_IIO_PROP_CHIP_VERSION = 0,
	CYPD_IIO_PROP_PD_ACTIVE,
	CYPD_IIO_PROP_MAX
};

struct cypd_phy_params {
	void	(*msg_rx_cb)(struct cypd *pd, enum pd_sop_type sop,
					u8 *buf, size_t len);
};

#if IS_ENABLED(CONFIG_CYPD_POLICY_ENGINE)
struct cypd *cypd_create(struct device *parent);
void cypd_destroy(struct cypd *pd);
#else
static struct cypd *cypd_create(struct device *parent)
{
	return ERR_PTR(-ENODEV);
}
static void cypd_destroy(struct cypd *pd) { }
#endif

#if IS_ENABLED(CONFIG_CHARGER_CYPD3177)
int cypd_phy_open(struct cypd_phy_params *params);
void cypd_phy_close(void);
int cypd_phy_write(u16 hdr, const u8 *data, size_t data_len,
		enum pd_sop_type sop);
#else
static int cypd_phy_write(u16 hdr, const u8 *data, size_t data_len,
		enum pd_sop_type sop)
{
	return -ENODEV;
}
static int cypd_phy_open(struct cypd_phy_params *params)
{
	return -ENODEV;
}
static void cypd_phy_close(void)
{
}
#endif
#endif /* _CYPD_H__ */

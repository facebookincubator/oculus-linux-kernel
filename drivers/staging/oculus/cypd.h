/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _CYPD_H__
#define _CYPD_H__

#include <linux/device.h>

struct cypd;
struct device;

enum pd_sop_type {
	SOP_MSG = 0,
	SOPI_MSG,
	SOPII_MSG,
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

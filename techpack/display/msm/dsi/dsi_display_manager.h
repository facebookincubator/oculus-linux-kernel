/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DSI_DISPLAY_MANAGER_H_
#define _DSI_DISPLAY_MANAGER_H_

#include <linux/list.h>

#include "dsi_display.h"

struct dsi_display_manager {
	struct list_head display_list;
	struct mutex disp_mgr_mutex;
	bool init;
};

enum dsi_display_mgr_ctrl_type {
	 DSI_DISPLAY_MGR_PHY_PWR = 0,
	 DSI_DISPLAY_MGR_PHY_IDLE,
};

static struct dsi_display_manager disp_mgr;

void dsi_display_manager_register(struct dsi_display *display);
void dsi_display_manager_unregister(struct dsi_display *display);

int dsi_display_mgr_phy_enable(struct dsi_display *display);
int dsi_display_mgr_phy_disable(struct dsi_display *display);
int dsi_display_mgr_phy_idle_on(struct dsi_display *display);
int dsi_display_mgr_phy_idle_off(struct dsi_display *display);
int dsi_display_mgr_phy_configure(void *priv, bool commit);
int dsi_display_mgr_phy_pll_toggle(void *priv, bool enable);
int dsi_display_mgr_panel_pre_prepare(struct dsi_display *display);
int dsi_display_mgr_panel_post_unprepare(struct dsi_display *display);
int dsi_display_config_mgr_for_cont_splash(struct dsi_display *display);
#endif /* _DSI_DISPLAY_MANAGER_H_ */

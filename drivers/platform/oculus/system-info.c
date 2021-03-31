/*
 * Copyright (c) 2017, Oculus VR. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "../../video/fbdev/msm/mdss.h"
#include "../../video/fbdev/msm/mdss_panel.h"

static u32 android_serialno_bin = 0;
static int __init parse_serialno(char *str)
{
	int i, pos;
	for (i = 0; i < strlen(str); i++) {
		/* pos tracks from end of string to beginning */
		pos = strlen(str) - i - 1;
		/* essentially multiplies the value by pow(16, i) */
		android_serialno_bin += hex_to_bin(str[pos]) * (0x1 << (i * 4));
	}
	return 0;
}
__setup("androidboot.serialno=", parse_serialno);

u32 get_android_serialno(void) { return android_serialno_bin; }
EXPORT_SYMBOL(get_android_serialno);

int get_panel_name(char *panel_name)
{
	int rc;
	int len, i = 0;
	char panel_cfg[256] = "";
	char ctrl_id_stream[3] =  "0:";
	char *str1 = NULL, *str2 = NULL;
	struct mdss_util_intf *util;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!panel_name)
		return MDSS_PANEL_INTF_INVALID;

	util = mdss_get_util_intf();
	if (util == NULL) {
		pr_err("%s: Failed to get mdss utility functions\n", __func__);
		return MDSS_PANEL_INTF_INVALID;
	}

	pan_cfg = util->panel_intf_type(0);
	if (IS_ERR_OR_NULL(pan_cfg)) {
		rc = PTR_ERR(pan_cfg);
		pr_err("WLED pan_cfg NULL or ERROR\n");
		goto end;
	} else {
		rc = strlcpy(panel_cfg, pan_cfg->arg_cfg,
				sizeof(pan_cfg->arg_cfg));
	}

	len = strlen(panel_cfg);
	if (!len) {
		/* no panel cfg chg, parse dt */
		pr_debug("%s:%d: no cmd line cfg present\n",
				__func__, __LINE__);
		goto end;
	} else {
		strlcpy(ctrl_id_stream, "1:", 3);

		/* get controller number */
		str1 = strnstr(panel_cfg, ctrl_id_stream, len);
		if (!str1) {
			pr_err("%s: controller %s is not present in %s\n",
					__func__, ctrl_id_stream, panel_cfg);
			goto end;
		}
		if ((str1 != panel_cfg) && (*(str1-1) != ':')) {
			str1 += 2;
			pr_debug("false match with config node name in \"%s\". search again in \"%s\"\n",
					panel_cfg, str1);
			str1 = strnstr(str1, ctrl_id_stream, len);
			if (!str1) {
				pr_err("%s: 2. controller %s is not present in %s\n",
						__func__, ctrl_id_stream, str1);
				goto end;
			}
		}
		str1 += 7;

		/* get panel name */
		str2 = strnchr(str1, strlen(str1), ':');
		if (!str2) {
			strlcpy(panel_name, str1, 256);
		} else {
			for (i = 0; (str1 + i) < str2; i++)
				panel_name[i] = *(str1 + i);
			panel_name[i] = 0;
		}
		pr_info("%s: cmdline:%s panel_name:%s\n",
				__func__, panel_cfg, panel_name);

		if (!strcmp(panel_name, "none")) {
			pr_err("%s: panel name = NONE_PANEL\n", __func__);
			goto end;
		}
	}
end:
	return rc;
}
EXPORT_SYMBOL(get_panel_name);

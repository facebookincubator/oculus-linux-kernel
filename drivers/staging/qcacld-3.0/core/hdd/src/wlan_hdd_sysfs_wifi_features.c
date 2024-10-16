/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_sysfs_wifi_features.c
 *
 * Wifi Feature sysfs implementation
 */

#include <linux/kobject.h>

#include "wlan_hdd_includes.h"
#include "wlan_hdd_sysfs_wifi_features.h"
#include "wlan_hdd_sysfs.h"
#include "osif_sync.h"

static ssize_t  __hdd_sysfs_feature_set_show(struct hdd_context *hdd_ctx,
					     struct kobj_attribute *attr,
					     char *buf)
{
	ssize_t ret_val = 0;
	uint8_t i = 0;
	char const *solution_provider = "QCT";

	if (!hdd_ctx->oem_data_len) {
		hdd_debug("Feature info is not available");
		return 0;
	}

	for (i = 0; i < hdd_ctx->oem_data_len; i++) {
		/* The Solution Provider Info is from index 2 to 4 */
		if (i == 2) {
			ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val,
					     "%s", solution_provider);
			i = i + 2;
			continue;
		}
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val, "%.2X",
				     hdd_ctx->oem_data[i]);
	}

	buf[ret_val] = '\n';

	return ret_val;
}

static ssize_t hdd_sysfs_feature_set_show(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	ssize_t errno_size;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	errno_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					     &psoc_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_feature_set_show(hdd_ctx, attr, buf);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static struct kobj_attribute feature_set_attribute =
	__ATTR(feature, 0660, hdd_sysfs_feature_set_show, NULL);

void hdd_sysfs_create_wifi_feature_interface(struct kobject *wifi_kobject)
{
	int error;

	if (!wifi_kobject) {
		hdd_err("could not get wifi kobject!");
		return;
	}

	error = sysfs_create_file(wifi_kobject,
				  &feature_set_attribute.attr);
	if (error)
		hdd_err("could not create dump in progress sysfs file");
}

void hdd_sysfs_destroy_wifi_feature_interface(struct kobject *wifi_kobject)
{
	if (!wifi_kobject) {
		hdd_err("could not get wifi kobject!");
		return;
	}

	sysfs_remove_file(wifi_kobject,
			  &feature_set_attribute.attr);
}

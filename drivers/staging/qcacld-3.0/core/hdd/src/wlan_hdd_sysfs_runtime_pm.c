/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved..
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

#include <wlan_hdd_includes.h>
#include "osif_psoc_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_runtime_pm.h>
#include "hif.h"
#include "hif_runtime_pm.h"

static ssize_t hdd_sysfs_runtime_pm_show(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 char *buf)
{
	return hif_rtpm_log_debug_stats(buf, HIF_RTPM_FILL_TYPE_SYSFS);
}

static struct kobj_attribute runtime_pm_attribute =
	__ATTR(runtime_pm, 0440, hdd_sysfs_runtime_pm_show,
	       NULL);

int hdd_sysfs_runtime_pm_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &runtime_pm_attribute.attr);
	if (error)
		hdd_err("could not create runtime_pm sysfs file");

	return error;
}

void
hdd_sysfs_runtime_pm_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}
	sysfs_remove_file(driver_kobject, &runtime_pm_attribute.attr);
}

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

#ifndef _WLAN_HDD_SYSFS_RUNTIME_PM_H
#define _WLAN_HDD_SYSFS_RUNTIME_PM_H

#if defined(WLAN_SYSFS) && defined(FEATURE_RUNTIME_PM)

/**
 * hdd_sysfs_runtime_pm_create(): create runtime pm WoW stats sysfs node
 * @driver_kobject: pointer to driver kobject
 *
 * Return: 0 for success and non zero error code for failure
 *
 */
int hdd_sysfs_runtime_pm_create(struct kobject *driver_kobject);

/**
 * hdd_sysfs_runtime_pm_destroy(): destroy runtime pm WoW stats sysfs node
 * @driver_kobject: pointer to driver kobject
 *
 * Return: void
 *
 */
void
hdd_sysfs_runtime_pm_destroy(struct kobject *driver_kobject);

#else

static inline int
hdd_sysfs_runtime_pm_create(struct kobject *driver_kobject)
{
	return 0;
}

static inline void
hdd_sysfs_runtime_pm_destroy(struct kobject *driver_kobject)
{
}
#endif
#endif

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

#if !defined(WLAN_HDD_SYSFS_WIFI_FEATURES_H)
#define WLAN_HDD_SYSFS_WIFI_FEATURES_H
/**
 * DOC: wlan_hdd_sysfs_wifi_features.h
 *
 * Wifi features declarations
 */

#if defined(WLAN_SYSFS) && defined(FEATURE_SET)
/**
 * hdd_sysfs_create_wifi_feature_interface() - API to create
 * wifi feature sysfs file
 * @wifi_kobject: sysfs wifi kobject
 *
 * Return: None
 */
void hdd_sysfs_create_wifi_feature_interface(struct kobject *wifi_kobject);

/**
 * hdd_sysfs_destroy_wifi_feature_interface() - API to destroy
 * wifi feature sysfs file
 * @wifi_kobject: sysfs wifi kobject
 *
 * Return: None
 */
void hdd_sysfs_destroy_wifi_feature_interface(struct kobject *wifi_kobject);
#else
static inline void
hdd_sysfs_create_wifi_feature_interface(struct kobject *wifi_kobject)
{
}

static inline void
hdd_sysfs_destroy_wifi_feature_interface(struct kobject *wifi_kobject)
{
}
#endif
#endif

/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_bitrates.h
 *
 * implementation for creating sysfs file bitrates
 * file path: /sys/class/net/wlanxx/sta_bitrates
 * file path: /sys/class/net/wlanxx/sap_bitrates
 *
 * usage:
 *      echo [arg_0] [arg_1] > sta_bitrates
 *      echo [arg_0] [arg_1] > sap_bitrates
 */

#ifndef _WLAN_HDD_SYSFS_BITRATES_H
#define _WLAN_HDD_SYSFS_BITRATES_H

#define SET_11N_RATES 0
#define SET_11AC_RATES 1
#define SET_11AX_RATES 2

#if defined(WLAN_SYSFS) && defined(CONFIG_WLAN_SYSFS_BITRATES)
/**
 * hdd_sysfs_sta_bitrates_create() - API to create sta_bitrates
 * @adapter: hdd adapter
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_sta_bitrates_create(struct hdd_adapter *adapter);
/**
 * hdd_sysfs_sap_bitrates_create() - API to create sap_bitrates
 * @adapter: hdd adapter
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_sap_bitrates_create(struct hdd_adapter *adapter);
/**
 * hdd_sysfs_sta_bitrates_destroy() - API to destroy sta_bitrates
 * @adapter: hdd adapter
 *
 * Return: none
 */
void
hdd_sysfs_sta_bitrates_destroy(struct hdd_adapter *adapter);
/**
 * hdd_sysfs_sap_bitrates_destroy() - API to destroy sap_bitrates
 * @adapter: hdd adapter
 *
 * Return: none
 */
void
hdd_sysfs_sap_bitrates_destroy(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_sta_bitrates_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline int
hdd_sysfs_sap_bitrates_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_sta_bitrates_destroy(struct hdd_adapter *adapter)
{
}

static inline void
hdd_sysfs_sap_bitrates_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_BITRATES_H */

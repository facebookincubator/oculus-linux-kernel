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

#ifndef _WLAN_HDD_SYSFS_DIRECT_LINK_UT_CMD_H
#define _WLAN_HDD_SYSFS_DIRECT_LINK_UT_CMD_H

#if defined(WLAN_SYSFS) && defined(FEATURE_DIRECT_LINK)
/**
 * hdd_sysfs_direct_link_ut_cmd_create() - API to create direct link unit test
 * command sysfs entry
 * @adapter: HDD adapter
 *
 * file path: /sys/class/net/wlanxx/direct_link_ut_cmd
 * This sysfs entry is created per adapter
 *
 * usage:
 *   echo [0/1] <duration> <flush> <pkts> <size> <ether> <dest_mac> > file path
 *
 * Return: 0 on success
 */
int hdd_sysfs_direct_link_ut_cmd_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_direct_link_ut_destroy() - API to destroy direct link unit test
 * command sysfs entry
 * @adapter: HDD adapter
 *
 * Return: None
 */
void hdd_sysfs_direct_link_ut_destroy(struct hdd_adapter *adapter);
#else
static inline
int hdd_sysfs_direct_link_ut_cmd_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline
void hdd_sysfs_direct_link_ut_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif

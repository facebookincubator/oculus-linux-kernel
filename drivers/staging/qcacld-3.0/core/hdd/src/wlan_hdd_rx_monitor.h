/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __WLAN_HDD_RX_MONITOR_H
#define __WLAN_HDD_RX_MONITOR_H

struct ol_txrx_ops;

#ifdef FEATURE_MONITOR_MODE_SUPPORT
int hdd_enable_monitor_mode(struct net_device *dev);

/**
 * hdd_disable_monitor_mode() - Disable monitor mode
 *
 * This function invokes cdp interface API to disable
 * monitor mode configuration on the hardware. In this
 * case sends HTT messages to FW to reset hardware rings
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_disable_monitor_mode(void);
#else
static inline int hdd_enable_monitor_mode(struct net_device *dev)
{
	return 0;
}

static inline int hdd_disable_monitor_mode(void)
{
	return 0;
}

#endif /* FEATURE_MONITOR_MODE_SUPPORT */

#endif /* __WLAN_HDD_RX_MONITOR_H */


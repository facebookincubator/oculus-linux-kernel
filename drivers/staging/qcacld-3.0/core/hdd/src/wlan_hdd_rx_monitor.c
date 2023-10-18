/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_hdd_includes.h"
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <cds_sched.h>
#include <cds_utils.h>
#include "wlan_hdd_rx_monitor.h"
#include "ol_txrx.h"
#include "cdp_txrx_mon.h"

int hdd_enable_monitor_mode(struct net_device *dev)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t vdev_id;

	hdd_enter_dev(dev);

	vdev_id = cdp_get_mon_vdev_from_pdev(soc, OL_TXRX_PDEV_ID);
	if (vdev_id < 0)
		return -EINVAL;

	return cdp_set_monitor_mode(soc, vdev_id, false);
}

int hdd_disable_monitor_mode(void)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	return cdp_reset_monitor_mode(soc, OL_TXRX_PDEV_ID, false);
}

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_eht_rate.c
 *
 * implementation for creating sysfs file 11be_rate
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_eht.h"
#include "wlan_hdd_sysfs.h"
#include "wlan_hdd_sysfs_eht_rate.h"
#include "osif_sync.h"

static ssize_t
__hdd_sysfs_set_11be_fixed_rate(struct net_device *net_dev, char const *buf,
				size_t count)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	int ret;
	uint16_t rate_code;
	char *sptr, *token;

	if (hdd_validate_adapter(adapter)) {
		hdd_err_rl("invalid adapter");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret) {
		hdd_err_rl("invalid hdd context");
		return ret;
	}

	if (!wlan_hdd_validate_modules_state(hdd_ctx)) {
		hdd_err_rl("invalid module state");
		return -EINVAL;
	}

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	token = strsep(&sptr, " ");
	if (!token || kstrtou16(token, 0, &rate_code)) {
		hdd_err_rl("invalid input");
		return -EINVAL;
	}

	hdd_set_11be_rate_code(adapter, rate_code);

	return count;
}

static ssize_t hdd_sysfs_set_11be_fixed_rate(
			     struct device *dev, struct device_attribute *attr,
			     char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_set_11be_fixed_rate(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(11be_rate, 0220, NULL, hdd_sysfs_set_11be_fixed_rate);

void hdd_sysfs_11be_rate_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_11be_rate);
	if (error)
		hdd_err("could not create sysfs file to set 11be rate");
}

void hdd_sysfs_11be_rate_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_11be_rate);
}


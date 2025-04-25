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
 * DOC: wlan_hdd_sysfs_dfsnol.c
 *
 * Implementation for creating sysfs file dfsnol
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_dfs_utils_api.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_dfsnol.h>

static ssize_t
__hdd_sysfs_dfsnol_show(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_pdev *pdev;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	pdev = hdd_ctx->pdev;
	if (!pdev) {
		hdd_err("null pdev");
		return -EINVAL;
	}

	utils_dfs_print_nol_channels(pdev);
	return scnprintf(buf, PAGE_SIZE, "DFS NOL Info written to dmesg log\n");
}

static ssize_t
hdd_sysfs_dfsnol_show(struct device *dev,
		      struct device_attribute *attr, char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dfsnol_show(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dfsnol_store(struct net_device *net_dev,
			 char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	struct sap_context *sap_ctx;
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	eSapDfsNolType set_value;
	int ret;
	QDF_STATUS status;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter->deflink);
	if (!sap_ctx) {
		hdd_err_rl("Null SAP Context");
		return -EINVAL;
	}

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	hdd_nofl_debug("set_dfsnol: count %zu buf_local:(%s) net_devname %s",
		       count, buf_local, net_dev->name);

	/* Get set_value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &set_value))
		return -EINVAL;

	status = wlansap_set_dfs_nol(sap_ctx, set_value);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("Unable to set_dfsnol val %d", set_value);
		return -EINVAL;
	}

	return count;
}

static ssize_t
hdd_sysfs_dfsnol_store(struct device *dev, struct device_attribute *attr,
		       char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dfsnol_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(dfsnol, 0660, hdd_sysfs_dfsnol_show,
		   hdd_sysfs_dfsnol_store);

int hdd_sysfs_dfsnol_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dfsnol);
	if (error)
		hdd_err_rl("could not create dfsnol sysfs file");

	return error;
}

void hdd_sysfs_dfsnol_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dfsnol);
}

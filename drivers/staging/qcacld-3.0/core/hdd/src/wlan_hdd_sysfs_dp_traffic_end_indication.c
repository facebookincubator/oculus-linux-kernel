/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_dp_traffic_end_indication.c
 *
 * implementation for creating sysfs files:
 *
 * dp_traffic_end_indication
 */
#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_object_manager.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_dp_traffic_end_indication.h>
#include <wlan_dp_ucfg_api.h>

static ssize_t
__hdd_sysfs_dp_traffic_end_indication_show(struct net_device *net_dev,
					   char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct wlan_objmgr_vdev *vdev;
	struct dp_traffic_end_indication info = {0};
	QDF_STATUS status;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	ret = wlan_hdd_validate_context(adapter->hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(adapter->hdd_ctx))
		return -EINVAL;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_DP_ID);
	if (!vdev)
		return -EINVAL;

	status = ucfg_dp_traffic_end_indication_get(vdev, &info);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

	if (!QDF_IS_STATUS_SUCCESS(status))
		return -EINVAL;

	hdd_debug("vdev_id:%u traffic end indication:%u defdscp:%u spldscp:%u",
		  adapter->deflink->vdev_id, info.enabled,
		  info.def_dscp, info.spl_dscp);

	ret = scnprintf(buf, PAGE_SIZE, "%u %u %u\n",
			info.enabled, info.def_dscp, info.def_dscp);
	return ret;
}

static ssize_t
hdd_sysfs_dp_traffic_end_indication_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_dp_traffic_end_indication_show(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static ssize_t
__hdd_sysfs_dp_traffic_end_indication_store(struct net_device *net_dev,
					    const char *buf,
					    size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct dp_traffic_end_indication info = {0};
	struct wlan_objmgr_vdev *vdev;
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	uint8_t value, defdscp, spldscp;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	ret = wlan_hdd_validate_context(adapter->hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(adapter->hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err("invalid input");
		return ret;
	}

	sptr = buf_local;
	/* Enable/disable traffic end indication*/
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou8(token, 0, &value))
		return -EINVAL;

	/* Default DSCP Value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou8(token, 0, &defdscp))
		return -EINVAL;

	/* Special DSCP Value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou8(token, 0, &spldscp))
		return -EINVAL;

	if ((defdscp > 63) || (spldscp > 63)) {
		hdd_err("invalid dscp value");
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_DP_ID);
	if (!vdev)
		return -EINVAL;

	info.enabled = !!value;
	adapter->traffic_end_ind_en = info.enabled;
	if (info.enabled) {
		info.def_dscp = defdscp;
		info.spl_dscp = spldscp;
	} else {
		info.def_dscp = 0;
		info.spl_dscp = 0;
	}

	hdd_debug("vdev_id:%u traffic end indication:%u defdscp:%u spldscp:%u",
		  adapter->deflink->vdev_id, value, defdscp, spldscp);

	ucfg_dp_traffic_end_indication_set(vdev, info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID);

	return count;
}

static ssize_t
hdd_sysfs_dp_traffic_end_indication_store(struct device *dev,
					  struct device_attribute *attr,
					  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dp_traffic_end_indication_store(net_dev,
								 buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(dp_traffic_end_indication, 0660,
		   hdd_sysfs_dp_traffic_end_indication_show,
		   hdd_sysfs_dp_traffic_end_indication_store);

int hdd_sysfs_dp_traffic_end_indication_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_dp_traffic_end_indication);
	if (error)
		hdd_err("could not create traffic_end_indication sysfs file");

	return error;
}

void
hdd_sysfs_dp_traffic_end_indication_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev,
			   &dev_attr_dp_traffic_end_indication);
}

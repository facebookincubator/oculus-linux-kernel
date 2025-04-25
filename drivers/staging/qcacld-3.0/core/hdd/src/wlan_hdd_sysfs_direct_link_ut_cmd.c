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

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "os_if_qmi.h"
#include "cfg_ucfg_api.h"
#include "wlan_hdd_object_manager.h"
#include <wlan_hdd_sysfs.h>
#include "wlan_hdd_sysfs_direct_link_ut_cmd.h"

#define MAX_SYSFS_DIRECT_LNK_UT_USER_COMMAND_LENGTH 512

static ssize_t __hdd_sysfs_direct_link_ut_cmd_store(struct net_device *net_dev,
						    char const *buf,
						    size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct os_if_qmi_wfds_ut_cmd_info cmd_info;
	char buf_local[MAX_SYSFS_DIRECT_LNK_UT_USER_COMMAND_LENGTH + 1];
	char *sptr, *token;
	int ret;
	QDF_STATUS status;

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

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, (uint32_t *)&cmd_info.cmd))
		return -EINVAL;

	if (cmd_info.cmd >= WFDS_CMD_MAX)
		return -EINVAL;

	if (cmd_info.cmd == WFDS_STOP_TRAFFIC || cmd_info.cmd == WFDS_GET_STATS)
		goto send_request;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &cmd_info.duration))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &cmd_info.flush_period))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &cmd_info.num_pkts))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &cmd_info.buf_size))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou16(token, 0, &cmd_info.ether_type))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	qdf_mac_parse(token, &cmd_info.dest_mac);

	if (cmd_info.cmd == WFDS_START_WHC) {
		token = strsep(&sptr, " ");
		if (!token)
			return -EINVAL;
		qdf_mac_parse(token, &cmd_info.src_mac);

		token = strsep(&sptr, " ");
		if (!token)
			return -EINVAL;
		qdf_ipv4_parse(token, &cmd_info.dest_ip);

		token = strsep(&sptr, " ");
		if (!token)
			return -EINVAL;
		qdf_ipv4_parse(token, &cmd_info.src_ip);

		token = strsep(&sptr, " ");
		if (!token)
			return -EINVAL;
		if (kstrtou16(token, 0, &cmd_info.dest_port))
			return -EINVAL;
	} else if (cmd_info.cmd == WFDS_START_TRAFFIC) {
		qdf_copy_macaddr(&cmd_info.src_mac, &adapter->mac_addr);
	}

send_request:
	status = os_if_qmi_wfds_send_ut_cmd_req_msg(&cmd_info);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("failed to send command %d", status);

	return count;
}

static ssize_t hdd_sysfs_direct_link_ut_cmd_store(struct device *dev,
						  struct device_attribute *attr,
						  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_direct_link_ut_cmd_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(direct_link_ut_cmd, 0660, NULL,
		   hdd_sysfs_direct_link_ut_cmd_store);

int hdd_sysfs_direct_link_ut_cmd_create(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	int error;

	if (!hdd_ctx) {
		hdd_err("HDD context is NULL!");
		return -EINVAL;
	}

	if (cfg_get(hdd_ctx->psoc, CFG_ENABLE_DIRECT_LINK_UT_CMD) == false)
		return -EINVAL;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_direct_link_ut_cmd);
	if (error)
		hdd_err("could not create traffic_end_indication sysfs file");

	return error;
}

void hdd_sysfs_direct_link_ut_destroy(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;

	if (!hdd_ctx) {
		hdd_err("HDD context is NULL!");
		return;
	}

	if (cfg_get(hdd_ctx->psoc, CFG_ENABLE_DIRECT_LINK_UT_CMD) == false)
		return;

	device_remove_file(&adapter->dev->dev,
			   &dev_attr_direct_link_ut_cmd);
}

/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: defines driver functions interfacing with linux kernel
 */

#include "wlan_cfg80211.h"
#include "wlan_osif_request_manager.h"
#include "wlan_fwol_ucfg_api.h"
#include "os_if_fwol.h"

#ifdef WLAN_FEATURE_ELNA
#define WLAN_WAIT_TIME_GET_ELNA_BYPASS 1500

int os_if_fwol_set_elna_bypass(struct wlan_objmgr_vdev *vdev,
			       const struct nlattr *attr)
{
	struct set_elna_bypass_request req;
	QDF_STATUS status;

	req.vdev_id = vdev->vdev_objmgr.vdev_id;
	req.elna_mode = nla_get_u8(attr);
	if (req.elna_mode > EXTLNA_MODE_FIRMWARE_DEFAULT) {
		osif_err("Invalid elna_bypass value %d", req.elna_mode);
		return -EINVAL;
	}

	status = ucfg_fwol_set_elna_bypass(vdev, &req);
	if (!QDF_IS_STATUS_SUCCESS(status))
		osif_err("Failed to set ELNA BYPASS, %d", status);

	return qdf_status_to_os_return(status);
}

struct osif_get_elna_bypass_priv {
	enum fwol_extlna_mode elna_mode;
};

/**
 * os_if_fwol_get_elna_bypass_callback() - Get eLNA bypass callback
 * @context: Call context
 * @response: Pointer to response structure
 *
 * Return: void
 */
static void
os_if_fwol_get_elna_bypass_callback(void *context,
				    struct get_elna_bypass_response *response)
{
	struct osif_request *request;
	struct osif_get_elna_bypass_priv *priv;

	request = osif_request_get(context);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->elna_mode = response->elna_mode;

	osif_request_complete(request);
	osif_request_put(request);
}

int os_if_fwol_get_elna_bypass(struct wlan_objmgr_vdev *vdev,
			       struct sk_buff *skb,
			       const struct nlattr *attr)
{
	struct get_elna_bypass_request req;
	void *cookie;
	struct osif_request *request;
	struct osif_get_elna_bypass_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_GET_ELNA_BYPASS,
	};
	QDF_STATUS status;
	int ret = 0;

	req.vdev_id = vdev->vdev_objmgr.vdev_id;

	request = osif_request_alloc(&params);
	if (!request) {
		osif_err("Request allocation failure");
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);

	status = ucfg_fwol_get_elna_bypass(vdev, &req,
					   os_if_fwol_get_elna_bypass_callback,
					   cookie);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		osif_err("Failed to get ELNA BYPASS, %d", status);
		ret = qdf_status_to_os_return(status);
		goto end;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		osif_err("Operation timed out");
		goto end;
	}

	priv = osif_request_priv(request);
	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_CONFIG_ELNA_BYPASS,
		       priv->elna_mode)) {
		osif_err("put fail with elna_mode:%d", priv->elna_mode);
		ret = -EINVAL;
	}

end:
	osif_request_put(request);
	return ret;
}
#endif /* #ifdef WLAN_FEATURE_ELNA */

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
int os_if_fwol_send_dscp_up_map_to_fw(struct wlan_objmgr_vdev *vdev,
				     uint32_t *dscp_to_up_map)
{
	QDF_STATUS status;

	status = ucfg_fwol_send_dscp_up_map_to_fw(vdev, dscp_to_up_map);
	if (!QDF_IS_STATUS_SUCCESS(status))
		osif_err("Failed to send dscp_up_map to FW, %d", status);

	return qdf_status_to_os_return(status);
}
#endif

#ifdef WLAN_FEATURE_MDNS_OFFLOAD
int os_if_fwol_enable_mdns_offload(struct wlan_objmgr_psoc *psoc,
				   struct mdns_config_info *mdns_info)
{
	int ret = 0;
	QDF_STATUS status;

	if (!psoc) {
		osif_err("Null pointer for psoc");
		return -EINVAL;
	}

	if (!mdns_info) {
		osif_err("Invalid mDNS config");
		return -EINVAL;
	}

	status = ucfg_fwol_set_mdns_config(psoc, mdns_info);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		osif_err("Failed to set mDNS Config");
		ret = qdf_status_to_os_return(status);
	}

	return ret;
}

int os_if_fwol_disable_mdns_offload(struct wlan_objmgr_psoc *psoc)
{
	struct mdns_config_info *mdns_info = NULL;
	int ret = 0;
	QDF_STATUS status;

	if (!psoc) {
		osif_err("Null pointer for psoc");
		return -EINVAL;
	}

	mdns_info = qdf_mem_malloc(sizeof(*mdns_info));
	if (!mdns_info) {
		ret = -ENOMEM;
		goto out;
	}

	mdns_info->enable = false;
	status = ucfg_fwol_set_mdns_config(psoc, mdns_info);
	if (!QDF_IS_STATUS_SUCCESS(status))
		osif_err("Failed to set mDNS Config");
	ret = qdf_status_to_os_return(status);
out:
	qdf_mem_free(mdns_info);
	return ret;
}
#endif /* WLAN_FEATURE_MDNS_OFFLOAD */

#ifdef THERMAL_STATS_SUPPORT
int os_if_fwol_get_thermal_stats_req(struct wlan_objmgr_psoc *psoc,
				     enum thermal_stats_request_type req,
				     void (*callback)(void *context,
				     struct thermal_throttle_info *response),
				     void *context)
{
	QDF_STATUS status;


	status = ucfg_fwol_send_get_thermal_stats_cmd(psoc, req, callback,
						      context);
	if (!QDF_IS_STATUS_SUCCESS(status))
		osif_err("Failed to send get thermal stats cmd to FW, %d",
			 status);

	return qdf_status_to_os_return(status);
}
#endif

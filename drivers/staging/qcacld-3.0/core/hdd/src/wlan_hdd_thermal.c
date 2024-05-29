/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: wlan_hdd_thermal.c
 *
 * WLAN Host Device Driver implementation for thermal mitigation handling
 */

#include <wlan_hdd_includes.h>
#include <net/cfg80211.h>
#include "wlan_osif_priv.h"
#include "qdf_trace.h"
#include "wlan_hdd_main.h"
#include "osif_sync.h"
#include <linux/limits.h>
#include <wlan_hdd_object_manager.h>
#include "sme_api.h"
#include "wlan_hdd_thermal.h"
#include "wlan_hdd_cfg80211.h"
#include <qca_vendor.h>
#include "wlan_fwol_ucfg_api.h"
#include <pld_common.h>
#include "wlan_hdd_stats.h"
#include "os_if_fwol.h"
#include "wlan_osif_request_manager.h"
#include "wlan_fwol_public_structs.h"

#define DC_OFF_PERCENT_WPPS 50
#define WLAN_WAIT_TIME_GET_THERM_LVL 1000

const struct nla_policy
	wlan_hdd_thermal_mitigation_policy
	[QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_MAX + 1] = {
		[QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_VALUE] = {.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_THERMAL_COMPLETION_WINDOW] = {
						.type = NLA_U32},
		[QCA_WLAN_VENDOR_ATTR_THERMAL_STATS] = {.type = NLA_NESTED},
};

#ifdef FEATURE_WPSS_THERMAL_MITIGATION
void
hdd_thermal_fill_clientid_priority(struct hdd_context *hdd_ctx, uint8_t mon_id,
				   uint8_t priority_apps, uint8_t priority_wpps,
				   struct thermal_mitigation_params *params)
{
	if (hdd_ctx->multi_client_thermal_mitigation) {
		if (mon_id == THERMAL_MONITOR_APPS) {
			params->priority  = priority_apps;
			params->client_id = mon_id;
			hdd_debug("Thermal client:%d priority_apps: %d", mon_id,
				  priority_apps);
		} else if (mon_id == THERMAL_MONITOR_WPSS) {
			params->priority = priority_wpps;
			params->client_id = mon_id;
			/* currently hardcoded,
			 * can be changed based on requirement.
			 */
			params->levelconf[0].dcoffpercent = DC_OFF_PERCENT_WPPS;
			hdd_debug("Thermal client:%d priority_wpps: %d", mon_id,
				  priority_wpps);
		}
	}
}
#endif

QDF_STATUS
hdd_send_thermal_mitigation_val(struct hdd_context *hdd_ctx, uint32_t level,
				uint8_t mon_id)
{
	uint32_t dc, dc_off_percent;
	uint32_t prio = 0, target_temp = 0;
	struct wlan_fwol_thermal_temp thermal_temp = {0};
	QDF_STATUS status;
	bool enable = true;
	struct thermal_mitigation_params therm_cfg_params = {0};

	status = ucfg_fwol_get_thermal_temp(hdd_ctx->psoc, &thermal_temp);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("Failed to get fwol thermal obj");
		return status;
	}

	switch (level) {
	case QCA_WLAN_VENDOR_THERMAL_LEVEL_EMERGENCY:
		dc_off_percent = thermal_temp.throttle_dutycycle_level[5];
		break;
	case QCA_WLAN_VENDOR_THERMAL_LEVEL_CRITICAL:
		dc_off_percent = thermal_temp.throttle_dutycycle_level[4];
		break;
	case QCA_WLAN_VENDOR_THERMAL_LEVEL_SEVERE:
		dc_off_percent = thermal_temp.throttle_dutycycle_level[3];
		break;
	case QCA_WLAN_VENDOR_THERMAL_LEVEL_MODERATE:
		dc_off_percent = thermal_temp.throttle_dutycycle_level[2];
		break;
	case QCA_WLAN_VENDOR_THERMAL_LEVEL_LIGHT:
		dc_off_percent = thermal_temp.throttle_dutycycle_level[1];
		break;
	case QCA_WLAN_VENDOR_THERMAL_LEVEL_NONE:
		enable = false;
		dc_off_percent = thermal_temp.throttle_dutycycle_level[0];
		break;
	default:
		hdd_debug("Invalid thermal state");
		return QDF_STATUS_E_INVAL;
	}

	dc = thermal_temp.thermal_sampling_time;
	therm_cfg_params.enable = enable;
	therm_cfg_params.dc = dc;
	therm_cfg_params.levelconf[0].dcoffpercent = dc_off_percent;
	therm_cfg_params.levelconf[0].priority = prio;
	therm_cfg_params.levelconf[0].tmplwm = target_temp;
	therm_cfg_params.num_thermal_conf = 1;
	therm_cfg_params.pdev_id = 0;

	hdd_thermal_fill_clientid_priority(hdd_ctx, mon_id,
					   thermal_temp.priority_apps,
					   thermal_temp.priority_wpps,
					   &therm_cfg_params);

	hdd_debug("dc %d dc_off_per %d", dc, dc_off_percent);

	status = sme_set_thermal_throttle_cfg(hdd_ctx->mac_handle,
					      &therm_cfg_params);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err_rl("Failed to set throttle configuration %d", status);

	else
		/*
		 * After SSR, the thermal mitigation level is lost.
		 * As SSR is hidden from userland, this command will not come
		 * from userspace after a SSR. To restore this configuration,
		 * save this in hdd context and restore after re-init.
		 */
		hdd_ctx->dutycycle_off_percent = dc_off_percent;

	return QDF_STATUS_SUCCESS;
}

/**
 * convert_level_to_vendor_thermal_level() - convert internal thermal level
 *  to vendor command attribute enum qca_wlan_vendor_thermal_level
 * @level: driver internal thermal level
 *
 * Return: vendor thermal level
 */
static enum qca_wlan_vendor_thermal_level
convert_level_to_vendor_thermal_level(enum thermal_throttle_level level)
{
	if (level == THERMAL_FULLPERF)
		return QCA_WLAN_VENDOR_THERMAL_LEVEL_NONE;
	else if (level == THERMAL_MITIGATION)
		return QCA_WLAN_VENDOR_THERMAL_LEVEL_MODERATE;
	else if (level == THERMAL_SHUTOFF)
		return QCA_WLAN_VENDOR_THERMAL_LEVEL_CRITICAL;
	else
		return QCA_WLAN_VENDOR_THERMAL_LEVEL_EMERGENCY;
}

/**
 * hdd_get_curr_thermal_throttle_level_val() - Indicate current target
 *  thermal throttle level to upper layer upon query level
 * @hdd_ctx: hdd context
 *
 * Return: 0 for success
 */
static int
hdd_get_curr_thermal_throttle_level_val(struct hdd_context *hdd_ctx)
{
	struct sk_buff *reply_skb;
	uint32_t data_len;
	enum thermal_throttle_level level = THERMAL_FULLPERF;
	enum qca_wlan_vendor_thermal_level vendor_level;
	QDF_STATUS status;

	status = ucfg_fwol_thermal_get_target_level(hdd_ctx->psoc, &level);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("get_thermal level: fail get target level");
		return -EINVAL;
	}
	vendor_level = convert_level_to_vendor_thermal_level(level);
	data_len = NLMSG_HDRLEN + nla_total_size(sizeof(uint32_t));
	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
							     data_len);
	if (!reply_skb) {
		hdd_err("get_thermal level: buffer alloc fail");
		return -ENOMEM;
	}
	if (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL,
			vendor_level)) {
		hdd_err("get_thermal level: nla put fail");
		wlan_cfg80211_vendor_free_skb(reply_skb);
		return -EINVAL;
	}
	hdd_debug("get_thermal level: %d vendor level %d", level,
		  vendor_level);

	return wlan_cfg80211_vendor_cmd_reply(reply_skb);
}

/**
 * hdd_get_curr_thermal_temperature_val() - Indicate current target
 *  thermal temperature to upper layer when handing temperature
 *  query vendor command
 * @hdd_ctx: hdd context
 * @adapter: adapter context
 *
 * Return: 0 for success
 */
static int
hdd_get_curr_thermal_temperature_val(struct hdd_context *hdd_ctx,
				     struct hdd_adapter *adapter)
{
	struct sk_buff *reply_skb;
	int ret;
	uint32_t data_len;
	int temperature = 0;

	ret = wlan_hdd_get_temperature(adapter, &temperature);
	if (ret)
		return ret;
	data_len = NLMSG_HDRLEN + nla_total_size(sizeof(uint32_t));
	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
							     data_len);
	if (!reply_skb) {
		hdd_err("get_thermal temperature: buffer alloc fail");
		return -ENOMEM;
	}
	if (nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_THERMAL_GET_TEMPERATURE_DATA,
			temperature)) {
		hdd_err("get_thermal temperature: nla put fail");
		wlan_cfg80211_vendor_free_skb(reply_skb);
		return -EINVAL;
	}
	hdd_debug("get_thermal temperature: %d", temperature);

	return wlan_cfg80211_vendor_cmd_reply(reply_skb);
}

#ifdef THERMAL_STATS_SUPPORT
QDF_STATUS
hdd_send_get_thermal_stats_cmd(struct hdd_context *hdd_ctx,
			       enum thermal_stats_request_type request_type,
			       void (*callback)(void *context,
			       struct thermal_throttle_info *response),
			       void *context)
{
	int ret;

	if (!hdd_ctx->psoc) {
		hdd_err_rl("NULL pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}


	/* Send Get Thermal Stats cmd to FW */
	ret = os_if_fwol_get_thermal_stats_req(hdd_ctx->psoc, request_type,
					       callback, context);
	if (ret)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_get_thermal_stats_cb() - Get thermal stats callback
 * @context: Call context
 * @response: Pointer to response structure
 *
 * Return: void
 */
static void
hdd_get_thermal_stats_cb(void *context,
			 struct thermal_throttle_info *response)
{
	struct osif_request *request;
	struct thermal_throttle_info *priv;

	request = osif_request_get(context);
	if (!request) {
		osif_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	qdf_mem_copy(priv, response, sizeof(struct thermal_throttle_info));

	osif_request_complete(request);
	osif_request_put(request);
}

#define THERMAL_MIN_TEMP QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MIN_TEMPERATURE
#define THERMAL_MAX_TEMP QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_MAX_TEMPERATURE
#define THERMAL_DWELL_TIME QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_DWELL_TIME
#define THERMAL_LVL_COUNT QCA_WLAN_VENDOR_ATTR_THERMAL_STATS_TEMP_LEVEL_COUNTER

/**
 * hdd_get_curr_thermal_stats_val() - Indicate thermal stats
 *  to upper layer when query vendor command
 * @wiphy: Pointer to wireless phy
 * @hdd_ctx: hdd context
 *
 * Return: 0 for success
 */
static int
hdd_get_curr_thermal_stats_val(struct wiphy *wiphy,
			       struct hdd_context *hdd_ctx)
{
	int ret = 0;
	uint8_t i = 0;
	struct osif_request *request = NULL;
	int skb_len = 0;
	struct thermal_throttle_info *priv;
	struct thermal_throttle_info *get_tt_stats = NULL;
	struct sk_buff *skb = NULL;
	void *cookie;
	struct nlattr *therm_attr;
	struct nlattr *tt_levels;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_GET_THERM_LVL,
		.dealloc = NULL,
	};

	if (hdd_ctx->is_therm_stats_in_progress) {
		hdd_err("request already in progress");
		return -EINVAL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("request allocation failure");
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);
	hdd_ctx->is_therm_stats_in_progress = true;
	ret = hdd_send_get_thermal_stats_cmd(hdd_ctx, thermal_stats_req,
					     hdd_get_thermal_stats_cb,
					     cookie);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("Failure while sending command to fw");
		ret = -EAGAIN;
		goto completed;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("Timed out while retrieving thermal stats");
		ret = -EAGAIN;
		goto completed;
	}

	get_tt_stats = osif_request_priv(request);
	if (!get_tt_stats) {
		hdd_err("invalid get_tt_stats");
		ret = -EINVAL;
		goto completed;
	}

	skb_len = NLMSG_HDRLEN + (get_tt_stats->therm_throt_levels) *
		  (NLA_HDRLEN + (NLA_HDRLEN +
		     sizeof(get_tt_stats->level_info[i].start_temp_level) +
		     NLA_HDRLEN +
		     sizeof(get_tt_stats->level_info[i].end_temp_level) +
		     NLA_HDRLEN +
		     sizeof(get_tt_stats->level_info[i].total_time_ms_lo) +
		     NLA_HDRLEN +
		     sizeof(get_tt_stats->level_info[i].num_entry)));

	skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						       skb_len);
	if (!skb) {
		hdd_err_rl("wlan_cfg80211_vendor_cmd_alloc_reply_skb failed");
		ret = -ENOMEM;
		goto completed;
	}

	therm_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_THERMAL_STATS);
	if (!therm_attr) {
		hdd_err_rl("nla_nest_start failed for attr failed");
		ret = -EINVAL;
		goto nla_failed;
	}

	for (i = 0; i < get_tt_stats->therm_throt_levels; i++) {
		tt_levels = nla_nest_start(skb, i);
		if (!tt_levels) {
			hdd_err_rl("nla_nest_start failed for thermal level %d",
				   i);
			ret = -EINVAL;
			goto nla_failed;
		}

		hdd_debug("level %d, Temp Range: %d - %d, Dwell time %d, Counter %d",
			  i, get_tt_stats->level_info[i].start_temp_level,
			  get_tt_stats->level_info[i].end_temp_level,
			  get_tt_stats->level_info[i].total_time_ms_lo,
			  get_tt_stats->level_info[i].num_entry);

		if (nla_put_u32(skb, THERMAL_MIN_TEMP,
				get_tt_stats->level_info[i].start_temp_level) ||
		    nla_put_u32(skb, THERMAL_MAX_TEMP,
				get_tt_stats->level_info[i].end_temp_level) ||
		    nla_put_u32(skb, THERMAL_DWELL_TIME,
				(get_tt_stats->level_info[i].total_time_ms_lo)) ||
		    nla_put_u32(skb, THERMAL_LVL_COUNT,
				get_tt_stats->level_info[i].num_entry)) {
			hdd_err("nla put failure");
			ret =  -EINVAL;
			goto nla_failed;
		}
		nla_nest_end(skb, tt_levels);
	}
	nla_nest_end(skb, therm_attr);
	wlan_cfg80211_vendor_cmd_reply(skb);
	goto completed;

nla_failed:
	wlan_cfg80211_vendor_free_skb(skb);
completed:
	hdd_ctx->is_therm_stats_in_progress = false;
	osif_request_put(request);

	return ret;
}

#undef THERMAL_MIN_TEMP
#undef THERMAL_MAX_TEMP
#undef THERMAL_DWELL_TIME
#undef THERMAL_LVL_COUNT

static QDF_STATUS
hdd_send_thermal_stats_clear_cmd(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	status = hdd_send_get_thermal_stats_cmd(hdd_ctx,
					     thermal_stats_clear, NULL,
					     NULL);

	return status;
}
#else
static int
hdd_get_curr_thermal_stats_val(struct wiphy *wiphy,
			       struct hdd_context *hdd_ctx)
{
	return -EINVAL;
}

static QDF_STATUS
hdd_send_thermal_stats_clear_cmd(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* THERMAL_STATS_SUPPORT */

/**
 * __wlan_hdd_cfg80211_set_thermal_mitigation_policy() - Set the thermal policy
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_set_thermal_mitigation_policy(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_MAX + 1];
	uint32_t level, cmd_type;
	QDF_STATUS status;
	int ret;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return -EINVAL;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err_rl("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_MAX,
				    (struct nlattr *)data, data_len,
				    wlan_hdd_thermal_mitigation_policy)) {
		hdd_err_rl("Invalid attribute");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_VALUE]) {
		hdd_err_rl("attr thermal cmd value failed");
		return -EINVAL;
	}

	cmd_type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_VALUE]);
	switch (cmd_type) {
	case QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_SET_LEVEL:
		if (!tb[QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL]) {
			hdd_err_rl("attr thermal throttle set failed");
			return -EINVAL;
		}
		level =
		    nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_THERMAL_LEVEL]);

		hdd_debug("thermal mitigation level from userspace %d", level);
		status = hdd_send_thermal_mitigation_val(hdd_ctx, level,
							 THERMAL_MONITOR_APPS);
		ret = qdf_status_to_os_return(status);
		break;
	case QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_LEVEL:
		ret = hdd_get_curr_thermal_throttle_level_val(hdd_ctx);
		break;
	case QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_TEMPERATURE:
		ret = hdd_get_curr_thermal_temperature_val(hdd_ctx, adapter);
		break;
	case QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_GET_THERMAL_STATS:
		ret = hdd_get_curr_thermal_stats_val(wiphy, hdd_ctx);
		break;
	case QCA_WLAN_VENDOR_ATTR_THERMAL_CMD_TYPE_CLEAR_THERMAL_STATS:
		status = hdd_send_thermal_stats_clear_cmd(hdd_ctx);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failure while sending command to fw");
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}

	hdd_exit();
	return ret;
}

/**
 * wlan_hdd_cfg80211_set_thermal_mitigation_policy() - set thermal
 * mitigation policy
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * Return: 0 on success; error number otherwise.
 */
int
wlan_hdd_cfg80211_set_thermal_mitigation_policy(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_thermal_mitigation_policy(wiphy, wdev,
								  data,
								  data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

bool wlan_hdd_thermal_config_support(void)
{
	return true;
}

QDF_STATUS hdd_restore_thermal_mitigation_config(struct hdd_context *hdd_ctx)
{
	bool enable = true;
	uint32_t dc, dc_off_percent = 0;
	uint32_t prio = 0, target_temp = 0;
	struct wlan_fwol_thermal_temp thermal_temp = {0};
	QDF_STATUS status;
	struct thermal_mitigation_params therm_cfg_params = {0};

	status = ucfg_fwol_get_thermal_temp(hdd_ctx->psoc, &thermal_temp);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("Failed to get fwol thermal obj");
		return status;
	}

	dc_off_percent = hdd_ctx->dutycycle_off_percent;
	dc = thermal_temp.thermal_sampling_time;

	if (!dc_off_percent)
		enable = false;

	therm_cfg_params.enable = enable;
	therm_cfg_params.dc = dc;
	therm_cfg_params.levelconf[0].dcoffpercent = dc_off_percent;
	therm_cfg_params.levelconf[0].priority = prio;
	therm_cfg_params.levelconf[0].tmplwm = target_temp;
	therm_cfg_params.num_thermal_conf = 1;
	therm_cfg_params.client_id = THERMAL_MONITOR_APPS;
	therm_cfg_params.priority = 0;

	hdd_debug("dc %d dc_off_per %d enable %d", dc, dc_off_percent, enable);

	status = sme_set_thermal_throttle_cfg(hdd_ctx->mac_handle,
					      &therm_cfg_params);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err_rl("Failed to set throttle configuration %d", status);

	return status;
}

static int
__wlan_hdd_pld_set_thermal_mitigation(struct device *dev, unsigned long state,
				      int mon_id)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	QDF_STATUS status;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED)
		return -EINVAL;

	status = hdd_send_thermal_mitigation_val(hdd_ctx, state, mon_id);

	return qdf_status_to_os_return(status);
}

int wlan_hdd_pld_set_thermal_mitigation(struct device *dev, unsigned long state,
					int mon_id)
{
	struct osif_psoc_sync *psoc_sync;
	int ret;

	hdd_enter();

	ret = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (ret)
		return ret;

	ret =  __wlan_hdd_pld_set_thermal_mitigation(dev, state, mon_id);

	osif_psoc_sync_op_stop(psoc_sync);
	hdd_exit();

	return ret;
}

#ifdef FEATURE_WPSS_THERMAL_MITIGATION
inline void hdd_thermal_mitigation_register_wpps(struct hdd_context *hdd_ctx,
						 struct device *dev)
{
	if (hdd_ctx->multi_client_thermal_mitigation)
		pld_thermal_register(dev, HDD_THERMAL_STATE_LIGHT,
				     THERMAL_MONITOR_WPSS);
}

inline void hdd_thermal_mitigation_unregister_wpps(struct hdd_context *hdd_ctx,
						   struct device *dev)
{
	if (hdd_ctx->multi_client_thermal_mitigation)
		pld_thermal_unregister(dev, THERMAL_MONITOR_WPSS);
}
#else
static inline
void hdd_thermal_mitigation_register_wpps(struct hdd_context *hdd_ctx,
					  struct device *dev)
{
}

static inline
void hdd_thermal_mitigation_unregister_wpps(struct hdd_context *hdd_ctx,
					    struct device *dev)
{
}
#endif
void hdd_thermal_mitigation_register(struct hdd_context *hdd_ctx,
				     struct device *dev)
{
	pld_thermal_register(dev, HDD_THERMAL_STATE_EMERGENCY,
			     THERMAL_MONITOR_APPS);
	hdd_thermal_mitigation_register_wpps(hdd_ctx, dev);
}

void hdd_thermal_mitigation_unregister(struct hdd_context *hdd_ctx,
				       struct device *dev)
{
	hdd_thermal_mitigation_unregister_wpps(hdd_ctx, dev);
	pld_thermal_unregister(dev, THERMAL_MONITOR_APPS);
}

#ifdef FW_THERMAL_THROTTLE_SUPPORT
/**
 * hdd_notify_thermal_throttle_handler() - Thermal throttle event handler
 * @psoc: psoc object
 * @info: thermal throttle information from target
 *
 * Return: QDF_STATUS_SUCCESS for success.
 */
static QDF_STATUS
hdd_notify_thermal_throttle_handler(struct wlan_objmgr_psoc *psoc,
				    struct thermal_throttle_info *info)
{
	uint32_t data_len;
	struct sk_buff *vendor_event;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;
	enum qca_wlan_vendor_thermal_level level;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return QDF_STATUS_E_FAILURE;

	/* TX will be throttled completely if above MITIGATION level.
	 * So report additional DIAG event to notify user-space explicitly.
	 */
	if (info->level == THERMAL_SHUTOFF ||
	    info->level == THERMAL_SHUTDOWN_TARGET)
		host_log_device_status(WLAN_STATUS_DEVICE_TEMPERATURE_HIGH);

	data_len = NLMSG_HDRLEN + nla_total_size(sizeof(uint32_t));
	vendor_event = wlan_cfg80211_vendor_event_alloc(
				hdd_ctx->wiphy, NULL, data_len,
				QCA_NL80211_VENDOR_SUBCMD_THERMAL_INDEX,
				GFP_KERNEL);
	if (!vendor_event) {
		hdd_err("wlan_cfg80211_vendor_event_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	level = convert_level_to_vendor_thermal_level(info->level);
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_THERMAL_EVENT_LEVEL,
			level)) {
		wlan_cfg80211_vendor_free_skb(vendor_event);
		return QDF_STATUS_E_INVAL;
	}
	hdd_debug("thermal_throttle:level %d vendor level %d", info->level,
		  level);
	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return QDF_STATUS_SUCCESS;
}

void hdd_thermal_register_callbacks(struct hdd_context *hdd_ctx)
{
	struct fwol_thermal_callbacks cb_obj = {0};

	cb_obj.notify_thermal_throttle_handler =
		hdd_notify_thermal_throttle_handler;
	ucfg_fwol_thermal_register_callbacks(hdd_ctx->psoc, &cb_obj);
}

void hdd_thermal_unregister_callbacks(struct hdd_context *hdd_ctx)
{
	ucfg_fwol_thermal_unregister_callbacks(hdd_ctx->psoc);
}
#endif

/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <osdep.h>
#include <wmi.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_concurrency_api.h>
#ifdef WLAN_FEATURE_MCC_QUOTA
#include <wlan_p2p_mcc_quota_public_struct.h>
#endif

/**
 * send_set_enable_disable_mcc_adaptive_scheduler_cmd_tlv() -enable/disable
 *							     mcc scheduler
 * @wmi_handle: wmi handle
 * @mcc_adaptive_scheduler: enable/disable
 * @pdev_id: pdev identifier
 *
 * This function enable/disable mcc adaptive scheduler in fw.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS send_set_enable_disable_mcc_adaptive_scheduler_cmd_tlv(
		wmi_unified_t wmi_handle, uint32_t mcc_adaptive_scheduler,
		uint32_t pdev_id)
{
	QDF_STATUS ret;
	wmi_buf_t buf = 0;
	wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param *cmd = NULL;
	uint16_t len =
		sizeof(wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param *)
		wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param));
	cmd->enable = mcc_adaptive_scheduler;
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								pdev_id);

	wmi_mtrace(WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send enable/disable MCC"
			" adaptive scheduler command");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_set_mcc_channel_time_latency_cmd_tlv() -set MCC channel time latency
 * @wmi_handle: wmi handle
 * @mcc_channel_freq: mcc channel frequency in MHz
 * @mcc_channel_time_latency: MCC channel time latency.
 *
 * Currently used to set time latency for an MCC vdev/adapter using operating
 * channel of it and channel number. The info is provided run time using
 * iwpriv command: iwpriv <wlan0 | p2p0> setMccLatency <latency in ms>.
 *
 * Return: QDF status
 */
static QDF_STATUS send_set_mcc_channel_time_latency_cmd_tlv(
					wmi_unified_t wmi_handle,
					uint32_t mcc_channel_freq,
					uint32_t mcc_channel_time_latency)
{
	QDF_STATUS ret;
	wmi_buf_t buf = 0;
	wmi_resmgr_set_chan_latency_cmd_fixed_param *cmdTL = NULL;
	uint16_t len = 0;
	uint8_t *buf_ptr = NULL;
	wmi_resmgr_chan_latency chan_latency;
	/* Note: we only support MCC time latency for a single channel */
	uint32_t num_channels = 1;
	uint32_t chan1_freq = mcc_channel_freq;
	uint32_t latency_chan1 = mcc_channel_time_latency;

	/* If 0ms latency is provided, then FW will set to a default.
	 * Otherwise, latency must be at least 30ms.
	 */
	if ((latency_chan1 > 0) &&
	    (latency_chan1 < WMI_MCC_MIN_NON_ZERO_CHANNEL_LATENCY)) {
		wmi_err("Invalid time latency for Channel #1 = %dms "
			 "Minimum is 30ms (or 0 to use default value by "
			 "firmware)", latency_chan1);
		return QDF_STATUS_E_INVAL;
	}

	/*   Set WMI CMD for channel time latency here */
	len = sizeof(wmi_resmgr_set_chan_latency_cmd_fixed_param) +
	      WMI_TLV_HDR_SIZE +  /*Place holder for chan_time_latency array */
	      num_channels * sizeof(wmi_resmgr_chan_latency);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		return QDF_STATUS_E_NOMEM;
	}
	buf_ptr = (uint8_t *) wmi_buf_data(buf);
	cmdTL = (wmi_resmgr_set_chan_latency_cmd_fixed_param *)
		wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmdTL->tlv_header,
		WMITLV_TAG_STRUC_wmi_resmgr_set_chan_latency_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_resmgr_set_chan_latency_cmd_fixed_param));
	cmdTL->num_chans = num_channels;
	/* Update channel time latency information for home channel(s) */
	buf_ptr += sizeof(*cmdTL);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       num_channels * sizeof(wmi_resmgr_chan_latency));
	buf_ptr += WMI_TLV_HDR_SIZE;
	chan_latency.chan_mhz = chan1_freq;
	chan_latency.latency = latency_chan1;
	qdf_mem_copy(buf_ptr, &chan_latency, sizeof(chan_latency));
	wmi_mtrace(WMI_RESMGR_SET_CHAN_LATENCY_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_RESMGR_SET_CHAN_LATENCY_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send MCC Channel Time Latency command");
		wmi_buf_free(buf);
		QDF_ASSERT(0);
	}

	return ret;
}

/**
 * send_set_mcc_channel_time_quota_cmd_tlv() -set MCC channel time quota
 * @wmi_handle: wmi handle
 * @adapter_1_chan_freq: adapter 1 channel frequency in MHz
 * @adapter_1_quota: adapter 1 quota
 * @adapter_2_chan_freq: adapter 2 channel frequency in MHz
 *
 * Return: QDF status
 */
static QDF_STATUS send_set_mcc_channel_time_quota_cmd_tlv(
					wmi_unified_t wmi_handle,
					uint32_t adapter_1_chan_freq,
					uint32_t adapter_1_quota,
					uint32_t adapter_2_chan_freq)
{
	QDF_STATUS ret;
	wmi_buf_t buf = 0;
	uint16_t len = 0;
	uint8_t *buf_ptr = NULL;
	wmi_resmgr_set_chan_time_quota_cmd_fixed_param *cmdTQ = NULL;
	wmi_resmgr_chan_time_quota chan_quota;
	uint32_t quota_chan1 = adapter_1_quota;
	/* Knowing quota of 1st chan., derive quota for 2nd chan. */
	uint32_t quota_chan2 = 100 - quota_chan1;
	/* Note: setting time quota for MCC requires info for 2 channels */
	uint32_t num_channels = 2;
	uint32_t chan1_freq = adapter_1_chan_freq;
	uint32_t chan2_freq = adapter_2_chan_freq;

	wmi_debug("freq1:%dMHz, Quota1:%dms, freq2:%dMHz, Quota2:%dms",
		 chan1_freq, quota_chan1, chan2_freq, quota_chan2);

	/*
	 * Perform sanity check on time quota values provided.
	 */
	if (quota_chan1 < WMI_MCC_MIN_CHANNEL_QUOTA ||
	    quota_chan1 > WMI_MCC_MAX_CHANNEL_QUOTA) {
		wmi_err("Invalid time quota for Chan #1=%dms. Min: %dms, Max: %dms",
			quota_chan1, WMI_MCC_MIN_CHANNEL_QUOTA,
			WMI_MCC_MAX_CHANNEL_QUOTA);
		return QDF_STATUS_E_INVAL;
	}
	/* Set WMI CMD for channel time quota here */
	len = sizeof(wmi_resmgr_set_chan_time_quota_cmd_fixed_param) +
	      WMI_TLV_HDR_SIZE +       /* Place holder for chan_time_quota array */
	      num_channels * sizeof(wmi_resmgr_chan_time_quota);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		return QDF_STATUS_E_NOMEM;
	}
	buf_ptr = (uint8_t *) wmi_buf_data(buf);
	cmdTQ = (wmi_resmgr_set_chan_time_quota_cmd_fixed_param *)
		wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmdTQ->tlv_header,
		       WMITLV_TAG_STRUC_wmi_resmgr_set_chan_time_quota_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_resmgr_set_chan_time_quota_cmd_fixed_param));
	cmdTQ->num_chans = num_channels;

	/* Update channel time quota information for home channel(s) */
	buf_ptr += sizeof(*cmdTQ);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       num_channels * sizeof(wmi_resmgr_chan_time_quota));
	buf_ptr += WMI_TLV_HDR_SIZE;
	chan_quota.chan_mhz = chan1_freq;
	chan_quota.channel_time_quota = quota_chan1;
	qdf_mem_copy(buf_ptr, &chan_quota, sizeof(chan_quota));
	/* Construct channel and quota record for the 2nd MCC mode. */
	buf_ptr += sizeof(chan_quota);
	chan_quota.chan_mhz = chan2_freq;
	chan_quota.channel_time_quota = quota_chan2;
	qdf_mem_copy(buf_ptr, &chan_quota, sizeof(chan_quota));

	wmi_mtrace(WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send MCC Channel Time Quota command");
		wmi_buf_free(buf);
		QDF_ASSERT(0);
	}

	return ret;
}

#ifdef WLAN_FEATURE_MCC_QUOTA
/**
 * convert_to_host_quota_type() - convert wmi quota type to host quota type
 * @quota_type: wmi target quota type
 *
 * Return: enum mcc_quota_type
 */
static enum mcc_quota_type convert_to_host_quota_type(uint32_t quota_type)
{
	switch (quota_type) {
	case WMI_RESMGR_QUOTA_TYPE_CLEAR:
		return QUOTA_TYPE_CLEAR;
	case WMI_RESMGR_QUOTA_TYPE_FIXED:
		return QUOTA_TYPE_FIXED;
	case WMI_RESMGR_QUOTA_TYPE_DYNAMIC:
		return QUOTA_TYPE_DYNAMIC;
	default:
		wmi_err("mcc quota unknown quota type %d", quota_type);
		return QUOTA_TYPE_UNKNOWN;
	}
}

/**
 * extract_mcc_quota_ev_param_tlv() - extract mcc quota information from wmi
 *    event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @param: Pointer to hold mcc quota info
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_mcc_quota_ev_param_tlv(wmi_unified_t wmi_handle,
			       void *evt_buf, struct mcc_quota_info *param)
{
	WMI_RESMGR_CHAN_TIME_QUOTA_CHANGED_EVENTID_param_tlvs *param_tlvs;
	wmi_resmgr_chan_time_quota_changed_event_fixed_param *fixed_param;
	uint8_t i;
	wmi_resmgr_chan_time_quota_tlv *wmi_mcc_quota_info;

	if (!param) {
		wmi_err("mcc quota information param is null");
		return QDF_STATUS_E_INVAL;
	}

	param_tlvs = evt_buf;
	if (!param_tlvs || !param_tlvs->fixed_param) {
		wmi_err("Invalid mcc quota event buffer");
		return QDF_STATUS_E_INVAL;
	}
	fixed_param = param_tlvs->fixed_param;

	wmi_debug("mcc quota type %d, num %d",
		  fixed_param->quota_type, param_tlvs->num_chan_quota);

	param->type = convert_to_host_quota_type(fixed_param->quota_type);
	if (param->type == QUOTA_TYPE_UNKNOWN)
		return QDF_STATUS_E_INVAL;

	if (!param_tlvs->chan_quota) {
		param->num_chan_quota = 0;
		return QDF_STATUS_SUCCESS;
	}

	if (param_tlvs->num_chan_quota > MAX_MCC_QUOTA_CH_NUM)
		wmi_warn("mcc quota num %d unexpected",
			 param_tlvs->num_chan_quota);
	param->num_chan_quota = qdf_min(param_tlvs->num_chan_quota,
					(uint32_t)MAX_MCC_QUOTA_CH_NUM);
	wmi_mcc_quota_info = param_tlvs->chan_quota;
	for (i = 0; i < param->num_chan_quota; i++) {
		param->chan_quota[i].chan_mhz =
			wmi_mcc_quota_info[i].chan_time_quota.chan_mhz;
		param->chan_quota[i].channel_time_quota =
			wmi_mcc_quota_info[i].chan_time_quota.channel_time_quota;
		wmi_debug("mcc quota [%d] chan %d, quota %d",
			  i, param->chan_quota[i].chan_mhz,
			  param->chan_quota[i].channel_time_quota);
	}

	return QDF_STATUS_SUCCESS;
}

static void wmi_mcc_quota_evt_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->extract_mcc_quota_ev_param = extract_mcc_quota_ev_param_tlv;
}
#else
static inline void wmi_mcc_quota_evt_attach_tlv(wmi_unified_t wmi_handle)
{
}
#endif

void wmi_concurrency_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_set_enable_disable_mcc_adaptive_scheduler_cmd =
		send_set_enable_disable_mcc_adaptive_scheduler_cmd_tlv;
	ops->send_set_mcc_channel_time_latency_cmd =
		send_set_mcc_channel_time_latency_cmd_tlv;
	ops->send_set_mcc_channel_time_quota_cmd =
		send_set_mcc_channel_time_quota_cmd_tlv;
	wmi_mcc_quota_evt_attach_tlv(wmi_handle);
}

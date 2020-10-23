/*
 * Copyright (c) 2013-2018 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 *  DOC:    wma_utis.c
 *  This file contains utilities and stats related functions.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"
#include "cfg_api.h"

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"
#include "ol_txrx_peer_find.h"

#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"

#include "cds_utils.h"

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"

#include "dfs.h"
#include "wma_internal.h"
#include "cds_concurrency.h"
#include "wmi_unified_param.h"
#include "linux/ieee80211.h"
#include "cds_reg_service.h"

/* MCS Based rate table */
/* HT MCS parameters with Nss = 1 */
static struct index_data_rate_type mcs_nss1[] = {
	/* MCS L20  S20   L40   S40 */
	{0,  {65,  72},  {135,  150 } },
	{1,  {130, 144}, {270,  300 } },
	{2,  {195, 217}, {405,  450 } },
	{3,  {260, 289}, {540,  600 } },
	{4,  {390, 433}, {815,  900 } },
	{5,  {520, 578}, {1080, 1200} },
	{6,  {585, 650}, {1215, 1350} },
	{7,  {650, 722}, {1350, 1500} }
};

/* HT MCS parameters with Nss = 2 */
static struct index_data_rate_type mcs_nss2[] = {
	/* MCS L20  S20    L40   S40 */
	{0,  {130,  144},  {270,  300 } },
	{1,  {260,  289},  {540,  600 } },
	{2,  {390,  433},  {810,  900 } },
	{3,  {520,  578},  {1080, 1200} },
	{4,  {780,  867},  {1620, 1800} },
	{5,  {1040, 1156}, {2160, 2400} },
	{6,  {1170, 1300}, {2430, 2700} },
	{7,  {1300, 1440}, {2700, 3000} }
};

/* MCS Based VHT rate table */
/* MCS parameters with Nss = 1*/
static struct index_vht_data_rate_type vht_mcs_nss1[] = {
	/* MCS L20  S20    L40   S40    L80   S80 */
	{0,  {65,   72 }, {135,  150},  {293,  325} },
	{1,  {130,  144}, {270,  300},  {585,  650} },
	{2,  {195,  217}, {405,  450},  {878,  975} },
	{3,  {260,  289}, {540,  600},  {1170, 1300} },
	{4,  {390,  433}, {810,  900},  {1755, 1950} },
	{5,  {520,  578}, {1080, 1200}, {2340, 2600} },
	{6,  {585,  650}, {1215, 1350}, {2633, 2925} },
	{7,  {650,  722}, {1350, 1500}, {2925, 3250} },
	{8,  {780,  867}, {1620, 1800}, {3510, 3900} },
	{9,  {865,  960}, {1800, 2000}, {3900, 4333} }
};

/*MCS parameters with Nss = 2*/
static struct index_vht_data_rate_type vht_mcs_nss2[] = {
	/* MCS L20  S20    L40    S40    L80    S80 */
	{0,  {130,  144},  {270,  300},  { 585,  650} },
	{1,  {260,  289},  {540,  600},  {1170, 1300} },
	{2,  {390,  433},  {810,  900},  {1755, 1950} },
	{3,  {520,  578},  {1080, 1200}, {2340, 2600} },
	{4,  {780,  867},  {1620, 1800}, {3510, 3900} },
	{5,  {1040, 1156}, {2160, 2400}, {4680, 5200} },
	{6,  {1170, 1300}, {2430, 2700}, {5265, 5850} },
	{7,  {1300, 1444}, {2700, 3000}, {5850, 6500} },
	{8,  {1560, 1733}, {3240, 3600}, {7020, 7800} },
	{9,  {1730, 1920}, {3600, 4000}, {7800, 8667} }
};

#ifdef BIG_ENDIAN_HOST

/* ############# function definitions ############ */

/**
 * wma_swap_bytes() - swap bytes
 * @pv: buffer
 * @n: swap bytes
 *
 * Return: none
 */
void wma_swap_bytes(void *pv, uint32_t n)
{
	int32_t no_words;
	int32_t i;
	uint32_t *word_ptr;

	no_words = n / sizeof(uint32_t);
	word_ptr = (uint32_t *) pv;
	for (i = 0; i < no_words; i++) {
		*(word_ptr + i) = __cpu_to_le32(*(word_ptr + i));
	}
}

#define SWAPME(x, len) wma_swap_bytes(&x, len);
#endif /* BIG_ENDIAN_HOST */

/**
 * wma_mcs_rate_match() - find the match mcs rate
 * @match_rate:	the rate to look up
 * @is_sgi:	return if the SGI rate is found
 * @nss:	the nss in use
 * @nss1_rate:	the nss1 rate
 * @nss1_srate:	the nss1 SGI rate
 * @nss2_rate:	the nss2 rate
 * @nss2_srate:	the nss2 SGI rate
 *
 * This is a helper function to find the match of the tx_rate
 * in terms of the nss1/nss2 rate with non-SGI/SGI.
 *
 * Return: the found rate or 0 otherwise
 */
static inline uint16_t wma_mcs_rate_match(uint16_t match_rate, bool *is_sgi,
					  uint8_t nss, uint16_t nss1_rate,
					  uint16_t nss1_srate,
					  uint16_t nss2_rate,
					  uint16_t nss2_srate)
{
	WMA_LOGD("%s match_rate: %d, %d %d %d %d",
		__func__, match_rate, nss1_rate, nss1_srate, nss2_rate,
		nss2_srate);

	if (match_rate == nss1_rate) {
		return nss1_rate;
	} else if (match_rate == nss1_srate) {
		*is_sgi = true;
		return nss1_srate;
	} else if (nss == 2 && match_rate == nss2_rate)
		return nss2_rate;
	else if (nss == 2 && match_rate == nss2_srate) {
		*is_sgi = true;
		return nss2_srate;
	} else
		return 0;
}

/**
 * wma_get_mcs_idx() - get mcs index
 * @maxRate: max rate
 * @rate_flags: rate flags
 * @nss: number of nss
 * @mcsRateFlag: mcs rate flag
 *
 * Return: return mcs index
 */
static uint8_t wma_get_mcs_idx(uint16_t maxRate, uint8_t rate_flags,
			       uint8_t nss, uint8_t *mcsRateFlag)
{
	uint8_t  index = 0;
	uint16_t match_rate = 0;
	bool is_sgi = false;

	WMA_LOGD("%s rate:%d rate_flgs: 0x%x, nss: %d",
		 __func__, maxRate, rate_flags, nss);

	*mcsRateFlag = rate_flags;
	*mcsRateFlag &= ~eHAL_TX_RATE_SGI;
	for (index = 0; index < MAX_VHT_MCS_IDX; index++) {
		if (rate_flags & eHAL_TX_RATE_VHT80) {
			/* check for vht80 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(maxRate, &is_sgi, nss,
					vht_mcs_nss1[index].ht80_rate[0],
					vht_mcs_nss1[index].ht80_rate[1],
					vht_mcs_nss2[index].ht80_rate[0],
					vht_mcs_nss2[index].ht80_rate[1]);
			if (match_rate)
				goto rate_found;
		}
		if ((rate_flags & eHAL_TX_RATE_VHT40) |
		    (rate_flags & eHAL_TX_RATE_VHT80)) {
			/* check for vht40 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(maxRate, &is_sgi, nss,
					vht_mcs_nss1[index].ht40_rate[0],
					vht_mcs_nss1[index].ht40_rate[1],
					vht_mcs_nss2[index].ht40_rate[0],
					vht_mcs_nss2[index].ht40_rate[1]);
			if (match_rate) {
				*mcsRateFlag &= ~eHAL_TX_RATE_VHT80;
				goto rate_found;
			}
		}
		if ((rate_flags & eHAL_TX_RATE_VHT20) |
		    (rate_flags & eHAL_TX_RATE_VHT40) |
		    (rate_flags & eHAL_TX_RATE_VHT80)) {
			/* check for vht20 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(maxRate, &is_sgi, nss,
					vht_mcs_nss1[index].ht20_rate[0],
					vht_mcs_nss1[index].ht20_rate[1],
					vht_mcs_nss2[index].ht20_rate[0],
					vht_mcs_nss2[index].ht20_rate[1]);
			if (match_rate) {
				*mcsRateFlag &= ~(eHAL_TX_RATE_VHT80 |
						eHAL_TX_RATE_VHT40);
				goto rate_found;
			}
		}
	}
	for (index = 0; index < MAX_HT_MCS_IDX; index++) {
		if (rate_flags & eHAL_TX_RATE_HT40) {
			/* check for ht40 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(maxRate, &is_sgi, nss,
					mcs_nss1[index].ht40_rate[0],
					mcs_nss1[index].ht40_rate[1],
					mcs_nss2[index].ht40_rate[0],
					mcs_nss2[index].ht40_rate[1]);
			if (match_rate) {
				*mcsRateFlag = eHAL_TX_RATE_HT40;
				goto rate_found;
			}
		}
		if ((rate_flags & eHAL_TX_RATE_HT20) ||
			(rate_flags & eHAL_TX_RATE_HT40)) {
			/* check for ht20 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(maxRate, &is_sgi, nss,
					mcs_nss1[index].ht20_rate[0],
					mcs_nss1[index].ht20_rate[1],
					mcs_nss2[index].ht20_rate[0],
					mcs_nss2[index].ht20_rate[1]);
			if (match_rate) {
				*mcsRateFlag = eHAL_TX_RATE_HT20;
				goto rate_found;
			}
		}
	}

rate_found:
	/* set SGI flag only if this is SGI rate */
	if (match_rate && is_sgi == true)
		*mcsRateFlag |= eHAL_TX_RATE_SGI;

	WMA_LOGD("%s - match_rate: %d index: %d rate_flag: 0x%x is_sgi: %d",
		 __func__, match_rate, index, *mcsRateFlag, is_sgi);

	return match_rate ? index : INVALID_MCS_IDX;
}

/**
 * wma_peek_vdev_req() - peek what request message is queued for response.
 *			 the function does not delete the node after found
 * @wma: WMA handle
 * @vdev_id: vdev ID
 * @type: request message type
 *
 * Return: the request message found
 */
static struct wma_target_req *wma_peek_vdev_req(tp_wma_handle wma,
						uint8_t vdev_id, uint8_t type)
{
	struct wma_target_req *req_msg = NULL;
	bool found = false;
	qdf_list_node_t *node1 = NULL, *node2 = NULL;

	qdf_spin_lock_bh(&wma->vdev_respq_lock);
	if (QDF_STATUS_SUCCESS != qdf_list_peek_front(&wma->vdev_resp_queue,
							&node2)) {
		qdf_spin_unlock_bh(&wma->vdev_respq_lock);
		return NULL;
	}

	do {
		node1 = node2;
		req_msg = qdf_container_of(node1, struct wma_target_req, node);
		if (req_msg->vdev_id != vdev_id)
			continue;
		if (req_msg->type != type)
			continue;

		found = true;
		break;
	} while (QDF_STATUS_SUCCESS == qdf_list_peek_next(&wma->vdev_resp_queue,
							  node1, &node2));
	qdf_spin_unlock_bh(&wma->vdev_respq_lock);
	if (!found) {
		WMA_LOGP(FL("target request not found for vdev_id %d type %d"),
			 vdev_id, type);
		return NULL;
	}
	WMA_LOGD(FL("target request found for vdev id: %d type %d msg %d"),
		 vdev_id, type, req_msg->msg_type);
	return req_msg;
}

void wma_lost_link_info_handler(tp_wma_handle wma, uint32_t vdev_id,
					int32_t rssi)
{
	struct sir_lost_link_info *lost_link_info;
	QDF_STATUS qdf_status;
	cds_msg_t sme_msg = {0};

	/* report lost link information only for STA mode */
	if (wma->interfaces[vdev_id].vdev_up &&
	    (WMI_VDEV_TYPE_STA == wma->interfaces[vdev_id].type) &&
	    (0 == wma->interfaces[vdev_id].sub_type)) {
		lost_link_info = qdf_mem_malloc(sizeof(*lost_link_info));
		if (NULL == lost_link_info) {
			WMA_LOGE("%s: failed to allocate memory", __func__);
			return;
		}
		lost_link_info->vdev_id = vdev_id;
		lost_link_info->rssi = rssi;
		sme_msg.type = eWNI_SME_LOST_LINK_INFO_IND;
		sme_msg.bodyptr = lost_link_info;
		sme_msg.bodyval = 0;
		WMA_LOGI("%s: post msg to SME, bss_idx %d, rssi %d",  __func__,
			 lost_link_info->vdev_id, lost_link_info->rssi);

		qdf_status = cds_mq_post_message(QDF_MODULE_ID_SME, &sme_msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			WMA_LOGE("%s: fail to post msg to SME", __func__);
			qdf_mem_free(lost_link_info);
		}
	}
}

/**
 * host_map_smps_mode() - map fw smps mode to tSmpsModeValue
 * @fw_smps_mode: fw smps mode
 *
 * Return: return tSmpsModeValue
 */
tSmpsModeValue host_map_smps_mode(A_UINT32 fw_smps_mode)
{
	tSmpsModeValue smps_mode = SMPS_MODE_DISABLED;
	switch (fw_smps_mode) {
	case WMI_SMPS_FORCED_MODE_STATIC:
		smps_mode = STATIC_SMPS_MODE;
		break;
	case WMI_SMPS_FORCED_MODE_DYNAMIC:
		smps_mode = DYNAMIC_SMPS_MODE;
		break;
	default:
		smps_mode = SMPS_MODE_DISABLED;
	}

	return smps_mode;
}

/**
 * wma_smps_mode_to_force_mode_param() - Map smps mode to force
 * mode commmand param
 * @smps_mode: SMPS mode according to the protocol
 *
 * Return: int > 0 for success else failure
 */
int wma_smps_mode_to_force_mode_param(uint8_t smps_mode)
{
	int param = -EINVAL;

	switch (smps_mode) {
	case STATIC_SMPS_MODE:
		param = WMI_SMPS_FORCED_MODE_STATIC;
		break;
	case DYNAMIC_SMPS_MODE:
		param = WMI_SMPS_FORCED_MODE_DYNAMIC;
		break;
	case SMPS_MODE_DISABLED:
		param = WMI_SMPS_FORCED_MODE_DISABLED;
		break;
	default:
		WMA_LOGE(FL("smps mode cannot be mapped :%d "),
			 smps_mode);
	}
	return param;
}

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * wma_stats_ext_event_handler() - extended stats event handler
 * @handle:     wma handle
 * @event_buf:  event buffer received from fw
 * @len:        length of data
 *
 * Return: 0 for success or error code
 */
int wma_stats_ext_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	WMI_STATS_EXT_EVENTID_param_tlvs *param_buf;
	tSirStatsExtEvent *stats_ext_event;
	wmi_stats_ext_event_fixed_param *stats_ext_info;
	QDF_STATUS status;
	cds_msg_t cds_msg;
	uint8_t *buf_ptr;
	uint32_t alloc_len;

	WMA_LOGD("%s: Posting stats ext event to SME", __func__);

	param_buf = (WMI_STATS_EXT_EVENTID_param_tlvs *) event_buf;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid stats ext event buf", __func__);
		return -EINVAL;
	}

	stats_ext_info = param_buf->fixed_param;
	buf_ptr = (uint8_t *) stats_ext_info;

	alloc_len = sizeof(tSirStatsExtEvent);
	alloc_len += stats_ext_info->data_len;

	if (stats_ext_info->data_len > (WMI_SVC_MSG_MAX_SIZE -
	    sizeof(*stats_ext_info)) || stats_ext_info->data_len >
	    param_buf->num_data) {
		WMA_LOGE("Excess data_len:%d, num_data:%d",
			stats_ext_info->data_len, param_buf->num_data);
		return -EINVAL;
	}
	stats_ext_event = (tSirStatsExtEvent *) qdf_mem_malloc(alloc_len);
	if (NULL == stats_ext_event) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return -ENOMEM;
	}

	buf_ptr += sizeof(wmi_stats_ext_event_fixed_param) + WMI_TLV_HDR_SIZE;

	stats_ext_event->vdev_id = stats_ext_info->vdev_id;
	stats_ext_event->event_data_len = stats_ext_info->data_len;
	qdf_mem_copy(stats_ext_event->event_data,
		     buf_ptr, stats_ext_event->event_data_len);

	cds_msg.type = eWNI_SME_STATS_EXT_EVENT;
	cds_msg.bodyptr = (void *)stats_ext_event;
	cds_msg.bodyval = 0;

	status = cds_mq_post_message(CDS_MQ_ID_SME, &cds_msg);
	if (status != QDF_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to post stats ext event to SME", __func__);
		qdf_mem_free(stats_ext_event);
		return -EFAULT;
	}

	WMA_LOGD("%s: stats ext event Posted to SME", __func__);
	return 0;
}
#endif /* WLAN_FEATURE_STATS_EXT */


/**
 * wma_profile_data_report_event_handler() - fw profiling handler
 * @handle:     wma handle
 * @event_buf:  event buffer received from fw
 * @len:        length of data
 *
 * Return: 0 for success or error code
 */
int wma_profile_data_report_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	WMI_WLAN_PROFILE_DATA_EVENTID_param_tlvs *param_buf;
	wmi_wlan_profile_ctx_t *profile_ctx;
	wmi_wlan_profile_t *profile_data;
	uint32_t i = 0;
	uint32_t entries;
	uint8_t *buf_ptr;
	char temp_str[150];
	param_buf = (WMI_WLAN_PROFILE_DATA_EVENTID_param_tlvs *) event_buf;

	if (!param_buf) {
		WMA_LOGE("%s: Invalid profile data event buf", __func__);
		return -EINVAL;
	}
	profile_ctx = param_buf->profile_ctx;
	buf_ptr = (uint8_t *)profile_ctx;
	buf_ptr = buf_ptr + sizeof(wmi_wlan_profile_ctx_t) + WMI_TLV_HDR_SIZE;
	profile_data = (wmi_wlan_profile_t *) buf_ptr;
	entries = profile_ctx->bin_count;

	if (entries > param_buf->num_profile_data) {
		WMA_LOGE("FW bin count %d more than data %d in TLV hdr",
			 entries,
			 param_buf->num_profile_data);
		return -EINVAL;
	}

	QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
				"Profile data stats\n");
	QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
		"TOT: %d\n"
		"tx_msdu_cnt: %d\n"
		"tx_mpdu_cnt: %d\n"
		"tx_ppdu_cnt: %d\n"
		"rx_msdu_cnt: %d\n"
		"rx_mpdu_cnt: %d\n"
		"bin_count: %d\n",
		profile_ctx->tot,
		profile_ctx->tx_msdu_cnt,
		profile_ctx->tx_mpdu_cnt,
		profile_ctx->tx_ppdu_cnt,
		profile_ctx->rx_msdu_cnt,
		profile_ctx->rx_mpdu_cnt,
		profile_ctx->bin_count);

	QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
		  "Profile ID: Count: TOT: Min: Max: hist_intvl: hist[0]: hist[1]:hist[2]");

	for (i = 0; i < entries; i++) {
		if (i == WMI_WLAN_PROFILE_MAX_BIN_CNT)
			break;
		snprintf(temp_str, sizeof(temp_str),
			 " %d : %d : %d : %d : %d : %d : %d : %d : %d",
			profile_data[i].id,
			profile_data[i].cnt,
			profile_data[i].tot,
			profile_data[i].min,
			profile_data[i].max,
			profile_data[i].hist_intvl,
			profile_data[i].hist[0],
			profile_data[i].hist[1],
			profile_data[i].hist[2]);
		QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
			"%s", temp_str);
	}

	return 0;
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

/**
 * wma_unified_link_peer_stats_event_handler() - peer stats event handler
 * @handle:          wma handle
 * @cmd_param_info:  data received with event from fw
 * @len:             length of data
 *
 * Return: 0 for success or error code
 */
static int wma_unified_link_peer_stats_event_handler(void *handle,
						     uint8_t *cmd_param_info,
						     uint32_t len)
{
	WMI_PEER_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_peer_stats_event_fixed_param *fixed_param;
	wmi_peer_link_stats *peer_stats, *temp_peer_stats;
	wmi_rate_stats *rate_stats;
	tSirLLStatsResults *link_stats_results;
	uint8_t *results, *t_peer_stats, *t_rate_stats;
	uint32_t count, rate_cnt;
	uint32_t total_num_rates = 0;
	uint32_t next_res_offset, next_peer_offset, next_rate_offset;
	size_t peer_info_size, peer_stats_size, rate_stats_size;
	size_t link_stats_results_size;
	bool excess_data = false;
	uint32_t buf_len;

	tpAniSirGlobal pMac = cds_get_context(QDF_MODULE_ID_PE);

	if (!pMac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!pMac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	param_tlvs = (WMI_PEER_LINK_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}
	/*
	 * cmd_param_info contains
	 * wmi_peer_stats_event_fixed_param fixed_param;
	 * num_peers * size of(struct wmi_peer_link_stats)
	 * total_num_rates * size of(struct wmi_rate_stats)
	 * total_num_rates is the sum of the rates of all the peers.
	 */
	fixed_param = param_tlvs->fixed_param;
	peer_stats = param_tlvs->peer_stats;
	rate_stats = param_tlvs->peer_rate_stats;

	if (!fixed_param || !peer_stats ||
	    (peer_stats->num_rates && !rate_stats)) {
		WMA_LOGA("%s: Invalid param_tlvs for Peer Stats", __func__);
		return -EINVAL;
	}

	do {
		if (fixed_param->num_peers >
		    WMI_SVC_MSG_MAX_SIZE/sizeof(wmi_peer_link_stats) ||
		    fixed_param->num_peers > param_tlvs->num_peer_stats) {
			excess_data = true;
			break;
		} else {
			buf_len = fixed_param->num_peers *
				sizeof(wmi_peer_link_stats);
		}
		temp_peer_stats = (wmi_peer_link_stats *) peer_stats;
		for (count = 0; count < fixed_param->num_peers; count++) {
			if (temp_peer_stats->num_rates >
			    WMI_SVC_MSG_MAX_SIZE / sizeof(wmi_rate_stats)) {
			excess_data = true;
			break;
		} else {
				total_num_rates += temp_peer_stats->num_rates;
				if (total_num_rates >
				    WMI_SVC_MSG_MAX_SIZE /
				    sizeof(wmi_rate_stats) || total_num_rates >
				    param_tlvs->num_peer_rate_stats) {
					excess_data = true;
					break;
				}
				buf_len += temp_peer_stats->num_rates *
					sizeof(wmi_rate_stats);
			}
			temp_peer_stats++;
		}
	} while (0);

	if (excess_data ||
		(sizeof(*fixed_param) > WMI_SVC_MSG_MAX_SIZE - buf_len)) {
		WMA_LOGE("excess wmi buffer: rates:%d, peers:%d",
			peer_stats->num_rates, fixed_param->num_peers);
		return -EINVAL;
	}

	peer_stats_size = sizeof(tSirWifiPeerStat);
	peer_info_size = sizeof(tSirWifiPeerInfo);
	rate_stats_size = sizeof(tSirWifiRateStat);
	link_stats_results_size =
		sizeof(*link_stats_results) + peer_stats_size +
		(fixed_param->num_peers * peer_info_size) +
		(total_num_rates * rate_stats_size);

	link_stats_results = qdf_mem_malloc(link_stats_results_size);
	if (NULL == link_stats_results) {
		WMA_LOGD("%s: could not allocate mem for stats results-len %zu",
			 __func__, link_stats_results_size);
		return -ENOMEM;
	}

	qdf_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId = WMI_LINK_STATS_ALL_PEER;
	link_stats_results->rspId = fixed_param->request_id;
	link_stats_results->ifaceId = 0;
	link_stats_results->num_peers = fixed_param->num_peers;
	link_stats_results->peer_event_number = fixed_param->peer_event_number;
	link_stats_results->moreResultToFollow = fixed_param->more_data;

	qdf_mem_copy(link_stats_results->results,
		     &fixed_param->num_peers, peer_stats_size);

	results = (uint8_t *) link_stats_results->results;
	t_peer_stats = (uint8_t *) peer_stats;
	t_rate_stats = (uint8_t *) rate_stats;
	next_res_offset = peer_stats_size;
	next_peer_offset = WMI_TLV_HDR_SIZE;
	next_rate_offset = WMI_TLV_HDR_SIZE;
	for (rate_cnt = 0; rate_cnt < fixed_param->num_peers; rate_cnt++) {
		qdf_mem_copy(results + next_res_offset,
			     t_peer_stats + next_peer_offset, peer_info_size);
		next_res_offset += peer_info_size;

		/* Copy rate stats associated with this peer */
		for (count = 0; count < peer_stats->num_rates; count++) {
			rate_stats++;

			qdf_mem_copy(results + next_res_offset,
				     t_rate_stats + next_rate_offset,
				     rate_stats_size);
			next_res_offset += rate_stats_size;
			next_rate_offset += sizeof(*rate_stats);
		}
		next_peer_offset += sizeof(*peer_stats);
		peer_stats++;
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	pMac->sme.pLinkLayerStatsIndCallback(pMac->hHdd,
					     WMA_LINK_LAYER_STATS_RESULTS_RSP,
					     link_stats_results);
	qdf_mem_free(link_stats_results);

	return 0;
}

/**
 * wma_unified_radio_tx_mem_free() - Free radio tx power stats memory
 * @handle: WMI handle
 *
 * Return: 0 on success, error number otherwise.
 */
static int wma_unified_radio_tx_mem_free(void *handle)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	tSirWifiRadioStat *rs_results;
	uint32_t i = 0;

	if (!wma_handle->link_stats_results)
		return 0;

	rs_results = (tSirWifiRadioStat *)&wma_handle->link_stats_results->results[0];
	for (i = 0; i < wma_handle->link_stats_results->num_radio; i++) {
		rs_results += i;
		if (rs_results->tx_time_per_power_level) {
			qdf_mem_free(rs_results->tx_time_per_power_level);
			rs_results->tx_time_per_power_level = NULL;
		}

		if (rs_results->channels) {
			qdf_mem_free(rs_results->channels);
			rs_results->channels = NULL;
		}
	}

	qdf_mem_free(wma_handle->link_stats_results);
	wma_handle->link_stats_results = NULL;

	return 0;
}

/**
 * wma_unified_radio_tx_power_level_stats_event_handler() - tx power level stats
 * @handle: WMI handle
 * @cmd_param_info: command param info
 * @len: Length of @cmd_param_info
 *
 * This is the WMI event handler function to receive radio stats tx
 * power level stats.
 *
 * Return: 0 on success, error number otherwise.
*/
static int wma_unified_radio_tx_power_level_stats_event_handler(void *handle,
			u_int8_t *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_tx_power_level_stats_evt_fixed_param *fixed_param;
	uint8_t *tx_power_level_values;
	tSirLLStatsResults *link_stats_results;
	tSirWifiRadioStat *rs_results;
	uint32_t max_total_num_tx_power_levels = MAX_TPC_LEVELS * NUM_OF_BANDS *
						MAX_SPATIAL_STREAM_ANY_V3;

	tpAniSirGlobal mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!mac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	param_tlvs = (WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid tx power level stats event", __func__);
		return -EINVAL;
	}

	fixed_param = param_tlvs->fixed_param;
	if (!fixed_param) {
		WMA_LOGA("%s: Invalid param_tlvs for Radio tx_power level Stats", __func__);
		return -EINVAL;
	}

	link_stats_results = wma_handle->link_stats_results;
	if (!link_stats_results) {
		WMA_LOGA("%s: link_stats_results is NULL", __func__);
		return -EINVAL;
	}

	WMA_LOGD("%s: tot_num_tx_pwr_lvls: %u num_tx_pwr_lvls: %u pwr_lvl_offset: %u radio_id: %u",
			__func__, fixed_param->total_num_tx_power_levels,
			 fixed_param->num_tx_power_levels,
			 fixed_param->power_level_offset,
			 fixed_param->radio_id);

	if (fixed_param->num_tx_power_levels > ((WMI_SVC_MSG_MAX_SIZE -
	    sizeof(*fixed_param)) / sizeof(uint32_t)) ||
	    fixed_param->num_tx_power_levels >
	    param_tlvs->num_tx_time_per_power_level) {
		WMA_LOGE("%s: excess tx_power buffers:%d, num_tx_time_per_power_level:%d",
			__func__, fixed_param->num_tx_power_levels,
			param_tlvs->num_tx_time_per_power_level);
		return -EINVAL;
	}

	if (fixed_param->radio_id > link_stats_results->num_radio) {
		WMA_LOGD("%s: Invalid radio_id %d num_radio %d",
			 __func__, fixed_param->radio_id,
			 link_stats_results->num_radio);
		return -EINVAL;
	}

	if (fixed_param->total_num_tx_power_levels >
	    max_total_num_tx_power_levels) {
		WMA_LOGD("Invalid total_num_tx_power_levels %d",
			 fixed_param->total_num_tx_power_levels);
		return -EINVAL;
	}

	rs_results = (tSirWifiRadioStat *) &link_stats_results->results[0] +
							 fixed_param->radio_id;
	tx_power_level_values = (uint8_t *) param_tlvs->tx_time_per_power_level;

	rs_results->total_num_tx_power_levels =
				fixed_param->total_num_tx_power_levels;
	if (!rs_results->total_num_tx_power_levels) {
		link_stats_results->nr_received++;
		goto post_stats;
	}

	if ((fixed_param->power_level_offset >
	    rs_results->total_num_tx_power_levels) ||
	    (fixed_param->num_tx_power_levels >
	    rs_results->total_num_tx_power_levels -
	    fixed_param->power_level_offset)) {
		WMA_LOGE("%s: Invalid offset %d total_num %d num %d",
			 __func__, fixed_param->power_level_offset,
			 rs_results->total_num_tx_power_levels,
			 fixed_param->num_tx_power_levels);
		return -EINVAL;
	}

	if (!rs_results->tx_time_per_power_level) {
		rs_results->tx_time_per_power_level = qdf_mem_malloc(
				sizeof(uint32_t) *
				rs_results->total_num_tx_power_levels);
		if (!rs_results->tx_time_per_power_level) {
			WMA_LOGA("%s: Mem alloc failed for tx power level stats", __func__);
			/* In error case, atleast send the radio stats without
			 * tx_power_level stats */
			rs_results->total_num_tx_power_levels = 0;
			link_stats_results->nr_received++;
			goto post_stats;
		}
	}
	qdf_mem_copy(&rs_results->tx_time_per_power_level[fixed_param->power_level_offset],
		tx_power_level_values,
		sizeof(uint32_t) * fixed_param->num_tx_power_levels);
	if (rs_results->total_num_tx_power_levels ==
	   (fixed_param->num_tx_power_levels + fixed_param->power_level_offset)) {
		link_stats_results->moreResultToFollow = 0;
		link_stats_results->nr_received++;
	}

	WMA_LOGD("%s: moreResultToFollow: %u nr: %u nr_received: %u",
			__func__, link_stats_results->moreResultToFollow,
			link_stats_results->num_radio,
			link_stats_results->nr_received);

	/* If still data to receive, return from here */
	if (link_stats_results->moreResultToFollow)
		return 0;

post_stats:
	if (link_stats_results->num_radio != link_stats_results->nr_received) {
		/* Not received all radio stats yet, don't post yet */
		return 0;
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	mac->sme.pLinkLayerStatsIndCallback(mac->hHdd,
		WMA_LINK_LAYER_STATS_RESULTS_RSP,
		link_stats_results);
	wma_unified_radio_tx_mem_free(handle);

	return 0;
}

/**
 * wma_unified_link_radio_stats_event_handler() - radio link stats event handler
 * @handle:          wma handle
 * @cmd_param_info:  data received with event from fw
 * @len:             length of data
 *
 * Return: 0 for success or error code
 */
static int wma_unified_link_radio_stats_event_handler(void *handle,
						      uint8_t *cmd_param_info,
						      uint32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_RADIO_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_radio_link_stats_event_fixed_param *fixed_param;
	wmi_radio_link_stats *radio_stats;
	wmi_channel_stats *channel_stats;
	tSirLLStatsResults *link_stats_results;
	uint8_t *results, *t_radio_stats, *t_channel_stats;
	uint32_t next_chan_offset, count;
	size_t radio_stats_size, chan_stats_size;
	size_t link_stats_results_size;
	tSirWifiRadioStat *rs_results;
	tSirWifiChannelStats *chn_results;

	tpAniSirGlobal pMac = cds_get_context(QDF_MODULE_ID_PE);

	if (!pMac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!pMac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	param_tlvs = (WMI_RADIO_LINK_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	/*
	 * cmd_param_info contains
	 * wmi_radio_link_stats_event_fixed_param fixed_param;
	 * size of(struct wmi_radio_link_stats);
	 * num_channels * size of(struct wmi_channel_stats)
	 */
	fixed_param = param_tlvs->fixed_param;
	radio_stats = param_tlvs->radio_stats;
	channel_stats = param_tlvs->channel_stats;

	if (!fixed_param || !radio_stats ||
	    (radio_stats->num_channels && !channel_stats)) {
		WMA_LOGA("%s: Invalid param_tlvs for Radio Stats", __func__);
		return -EINVAL;
	}
	if (radio_stats->num_channels >
		(NUM_24GHZ_CHANNELS + NUM_5GHZ_CHANNELS) ||
		radio_stats->num_channels > param_tlvs->num_channel_stats) {
		WMA_LOGE("%s: Too many channels %d",
			__func__, radio_stats->num_channels);
		return -EINVAL;
	}

	radio_stats_size = sizeof(tSirWifiRadioStat);
	chan_stats_size = sizeof(tSirWifiChannelStats);
	if (fixed_param->num_radio >
		(UINT_MAX - sizeof(*link_stats_results))/radio_stats_size) {
		WMA_LOGE("excess num_radio %d is leading to int overflow",
			fixed_param->num_radio);
		return -EINVAL;
	}
	link_stats_results_size = sizeof(*link_stats_results) +
				  fixed_param->num_radio * radio_stats_size;

	if (!wma_handle->link_stats_results) {
		wma_handle->link_stats_results = qdf_mem_malloc(link_stats_results_size);
		if (NULL == wma_handle->link_stats_results) {
			WMA_LOGD("%s: could not allocate mem for stats results-len %zu",
				 __func__, link_stats_results_size);
			return -ENOMEM;
		}
	}
	link_stats_results = wma_handle->link_stats_results;

	WMA_LOGD("Radio stats Fixed Param:");
	WMA_LOGD("req_id: %u num_radio: %u more_radio_events: %u",
		 fixed_param->request_id, fixed_param->num_radio,
		 fixed_param->more_radio_events);

	WMA_LOGD("Radio Info: radio_id: %u on_time: %u tx_time: %u rx_time: %u on_time_scan: %u "
		 "on_time_nbd: %u on_time_gscan: %u on_time_roam_scan: %u "
		 "on_time_pno_scan: %u on_time_hs20: %u num_channels: %u",
		 radio_stats->radio_id, radio_stats->on_time,
		 radio_stats->tx_time, radio_stats->rx_time,
		 radio_stats->on_time_scan, radio_stats->on_time_nbd,
		 radio_stats->on_time_gscan,
		 radio_stats->on_time_roam_scan,
		 radio_stats->on_time_pno_scan,
		 radio_stats->on_time_hs20, radio_stats->num_channels);

	link_stats_results->paramId = WMI_LINK_STATS_RADIO;
	link_stats_results->rspId = fixed_param->request_id;
	link_stats_results->ifaceId = 0;
	link_stats_results->num_radio = fixed_param->num_radio;
	link_stats_results->peer_event_number = 0;

	/*
	 * Backward compatibility:
	 * There are firmware(s) which will send Radio stats only with
	 * more_radio_events set to 0 and firmware which sends Radio stats
	 * followed by tx_power level stats with more_radio_events set to 1.
	 * if more_radio_events is set to 1, buffer the radio stats and
	 * wait for tx_power_level stats.
	 */
	link_stats_results->moreResultToFollow = fixed_param->more_radio_events;

	results = (uint8_t *) link_stats_results->results;
	t_radio_stats = (uint8_t *) radio_stats;
	t_channel_stats = (uint8_t *) channel_stats;

	rs_results = (tSirWifiRadioStat *) &results[0] + radio_stats->radio_id;
	rs_results->radio = radio_stats->radio_id;
	rs_results->onTime = radio_stats->on_time;
	rs_results->txTime = radio_stats->tx_time;
	rs_results->rxTime = radio_stats->rx_time;
	rs_results->onTimeScan = radio_stats->on_time_scan;
	rs_results->onTimeNbd = radio_stats->on_time_nbd;
	rs_results->onTimeGscan = radio_stats->on_time_gscan;
	rs_results->onTimeRoamScan = radio_stats->on_time_roam_scan;
	rs_results->onTimePnoScan = radio_stats->on_time_pno_scan;
	rs_results->onTimeHs20 = radio_stats->on_time_hs20;
	rs_results->total_num_tx_power_levels = 0;
	rs_results->tx_time_per_power_level = NULL;
	rs_results->numChannels = radio_stats->num_channels;
	rs_results->channels = NULL;

	if (rs_results->numChannels) {
		rs_results->channels = (tSirWifiChannelStats *) qdf_mem_malloc(
					radio_stats->num_channels *
					chan_stats_size);
		if (rs_results->channels == NULL) {
			WMA_LOGD("%s: could not allocate mem for channel stats (size=%zu)",
				 __func__, radio_stats->num_channels * chan_stats_size);
			wma_unified_radio_tx_mem_free(handle);
			return -ENOMEM;
		}

		chn_results = (tSirWifiChannelStats *) &rs_results->channels[0];
		next_chan_offset = WMI_TLV_HDR_SIZE;
		WMA_LOGD("Channel Stats Info");
		for (count = 0; count < radio_stats->num_channels; count++) {
			WMA_LOGD("channel_width %u center_freq %u center_freq0 %u "
				 "center_freq1 %u radio_awake_time %u cca_busy_time %u",
				 channel_stats->channel_width,
				 channel_stats->center_freq,
				 channel_stats->center_freq0,
				 channel_stats->center_freq1,
				 channel_stats->radio_awake_time,
				 channel_stats->cca_busy_time);
			channel_stats++;

			qdf_mem_copy(chn_results,
				     t_channel_stats + next_chan_offset,
				     chan_stats_size);
			chn_results++;
			next_chan_offset += sizeof(*channel_stats);
		}
	}

	if (link_stats_results->moreResultToFollow) {
		/* More results coming, don't post yet */
		return 0;
	} else {
		link_stats_results->nr_received++;
	}

	if (link_stats_results->num_radio != link_stats_results->nr_received) {
		/* Not received all radio stats yet, don't post yet */
		return 0;
	}

	pMac->sme.pLinkLayerStatsIndCallback(pMac->hHdd,
					     WMA_LINK_LAYER_STATS_RESULTS_RSP,
					     link_stats_results);
	wma_unified_radio_tx_mem_free(handle);

	return 0;
}

/**
 * wma_register_ll_stats_event_handler() - register link layer stats related
 *                                         event handler
 * @wma_handle: wma handle
 *
 * Return: none
 */
void wma_register_ll_stats_event_handler(tp_wma_handle wma_handle)
{
	if (NULL == wma_handle) {
		WMA_LOGE("%s: wma_handle is NULL", __func__);
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_IFACE_LINK_STATS_EVENTID,
				wma_unified_link_iface_stats_event_handler,
				WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_PEER_LINK_STATS_EVENTID,
				wma_unified_link_peer_stats_event_handler,
				WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_RADIO_LINK_STATS_EVENTID,
				wma_unified_link_radio_stats_event_handler,
				WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID,
			wma_unified_radio_tx_power_level_stats_event_handler,
			WMA_RX_SERIALIZER_CTX);

	return;
}


/**
 * wma_process_ll_stats_clear_req() - clear link layer stats
 * @wma: wma handle
 * @clearReq: ll stats clear request command params
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_process_ll_stats_clear_req
	(tp_wma_handle wma, const tpSirLLStatsClearReq clearReq)
{
	struct ll_stats_clear_params cmd = {0};
	int ret;

	if (!clearReq || !wma) {
		WMA_LOGE("%s: input pointer is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	cmd.stop_req = clearReq->stopReq;
	cmd.sta_id = clearReq->staId;
	cmd.stats_clear_mask = clearReq->statsClearReqMask;

	ret = wmi_unified_process_ll_stats_clear_cmd(wma->wmi_handle, &cmd,
				   wma->interfaces[clearReq->staId].addr);
	if (ret) {
		WMA_LOGE("%s: Failed to send clear link stats req", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_process_ll_stats_set_req() - link layer stats set request
 * @wma:       wma handle
 * @setReq:  ll stats set request command params
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_process_ll_stats_set_req
	(tp_wma_handle wma, const tpSirLLStatsSetReq setReq)
{
	struct ll_stats_set_params cmd = {0};
	int ret;

	if (!setReq || !wma) {
		WMA_LOGE("%s: input pointer is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	cmd.mpdu_size_threshold = setReq->mpduSizeThreshold;
	cmd.aggressive_statistics_gathering =
		setReq->aggressiveStatisticsGathering;

	ret = wmi_unified_process_ll_stats_set_cmd(wma->wmi_handle,
					 &cmd);
	if (ret) {
		WMA_LOGE("%s: Failed to send set link stats request", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_process_ll_stats_get_req() - link layer stats get request
 * @wma:wma handle
 * @getReq:ll stats get request command params
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_process_ll_stats_get_req
	(tp_wma_handle wma, const tpSirLLStatsGetReq getReq)
{
	struct ll_stats_get_params cmd = {0};
	int ret;

	if (!getReq || !wma) {
		WMA_LOGE("%s: input pointer is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (!wma->interfaces[getReq->staId].vdev_active) {
		WMA_LOGE("%s: vdev not created yet", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	cmd.req_id = getReq->reqId;
	cmd.param_id_mask = getReq->paramIdMask;
	cmd.sta_id = getReq->staId;

	ret = wmi_unified_process_ll_stats_get_cmd(wma->wmi_handle, &cmd,
				   wma->interfaces[getReq->staId].addr);
	if (ret) {
		WMA_LOGE("%s: Failed to send get link stats request", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_unified_link_iface_stats_event_handler() - link iface stats event handler
 * @wma:wma handle
 * @cmd_param_info: data from event
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_unified_link_iface_stats_event_handler(void *handle,
					       uint8_t *cmd_param_info,
					       uint32_t len)
{
	WMI_IFACE_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_iface_link_stats_event_fixed_param *fixed_param;
	wmi_iface_link_stats *link_stats;
	wmi_wmm_ac_stats *ac_stats;
	tSirLLStatsResults *link_stats_results;
	uint8_t *results, *t_link_stats, *t_ac_stats;
	uint32_t next_res_offset, next_ac_offset, count;
	uint32_t roaming_offset, roaming_size;
	size_t link_stats_size, ac_stats_size, iface_info_size;
	size_t link_stats_results_size;

	tpAniSirGlobal pMac = cds_get_context(QDF_MODULE_ID_PE);

	if (!pMac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!pMac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	param_tlvs = (WMI_IFACE_LINK_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	/*
	 * cmd_param_info contains
	 * wmi_iface_link_stats_event_fixed_param fixed_param;
	 * wmi_iface_link_stats iface_link_stats;
	 * iface_link_stats->num_ac * size of(struct wmi_wmm_ac_stats)
	 */
	fixed_param = param_tlvs->fixed_param;
	link_stats = param_tlvs->iface_link_stats;
	ac_stats = param_tlvs->ac;

	if (!fixed_param || !link_stats || (link_stats->num_ac && !ac_stats)) {
		WMA_LOGA("%s: Invalid param_tlvs for Iface Stats", __func__);
		return -EINVAL;
	}
	if (link_stats->num_ac > WIFI_AC_MAX || link_stats->num_ac >
	    param_tlvs->num_ac) {
		WMA_LOGE("%s: Excess data received from firmware num_ac %d, param_tlvs->num_ac %d",
			 __func__, link_stats->num_ac, param_tlvs->num_ac);
		return -EINVAL;
	}
	if (fixed_param->num_offload_stats > WMI_OFFLOAD_STATS_TYPE_MAX ||
	    fixed_param->num_offload_stats >
	    param_tlvs->num_iface_offload_stats) {
		WMA_LOGE("%s: Excess num offload stats recvd from fw: %d, um_iface_offload_stats: %d",
			__func__, fixed_param->num_offload_stats,
			param_tlvs->num_iface_offload_stats);
		return -EINVAL;
	}

	link_stats_size = sizeof(tSirWifiIfaceStat);
	iface_info_size = sizeof(tSirWifiInterfaceInfo);
	ac_stats_size = sizeof(tSirWifiWmmAcStat);
	link_stats_results_size = sizeof(*link_stats_results) + link_stats_size;

	link_stats_results = qdf_mem_malloc(link_stats_results_size);
	if (!link_stats_results) {
		WMA_LOGD("%s: could not allocate mem for stats results-len %zu",
			 __func__, link_stats_results_size);
		return -ENOMEM;
	}

	qdf_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId = WMI_LINK_STATS_IFACE;
	link_stats_results->rspId = fixed_param->request_id;
	link_stats_results->ifaceId = fixed_param->vdev_id;
	link_stats_results->num_peers = link_stats->num_peers;
	link_stats_results->peer_event_number = 0;
	link_stats_results->moreResultToFollow = 0;

	results = (uint8_t *) link_stats_results->results;
	t_link_stats = (uint8_t *) link_stats;
	t_ac_stats = (uint8_t *) ac_stats;

	/* Copy roaming state */
	roaming_offset = offsetof(tSirWifiInterfaceInfo, roaming);
	roaming_size = member_size(tSirWifiInterfaceInfo, roaming);

	qdf_mem_copy(results + roaming_offset, &link_stats->roam_state,
		     roaming_size);

	qdf_mem_copy(results + iface_info_size,
		     t_link_stats + WMI_TLV_HDR_SIZE,
		     link_stats_size - iface_info_size -
		     WIFI_AC_MAX * ac_stats_size);

	next_res_offset = link_stats_size - WIFI_AC_MAX * ac_stats_size;
	next_ac_offset = WMI_TLV_HDR_SIZE;

	for (count = 0; count < link_stats->num_ac; count++) {
		ac_stats++;

		qdf_mem_copy(results + next_res_offset,
			     t_ac_stats + next_ac_offset, ac_stats_size);
		next_res_offset += ac_stats_size;
		next_ac_offset += sizeof(*ac_stats);
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	pMac->sme.pLinkLayerStatsIndCallback(pMac->hHdd,
					     WMA_LINK_LAYER_STATS_RESULTS_RSP,
					     link_stats_results);
	qdf_mem_free(link_stats_results);

	return 0;
}

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

/**
 * wma_update_pdev_stats() - update pdev stats
 * @wma: wma handle
 * @pdev_stats: pdev stats
 *
 * Return: none
 */
static void wma_update_pdev_stats(tp_wma_handle wma,
				  wmi_pdev_stats *pdev_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	uint32_t temp_mask;
	uint8_t *stats_buf;
	tCsrGlobalClassAStatsInfo *classa_stats = NULL;
	struct wma_txrx_node *node;
	uint8_t i;

	for (i = 0; i < wma->max_bssid; i++) {
		node = &wma->interfaces[i];
		stats_rsp_params = node->stats_rsp;
		if (stats_rsp_params) {
			node->fw_stats_set |= FW_PDEV_STATS_SET;
			WMA_LOGD("<---FW PDEV STATS received for vdevId:%d", i);
			stats_buf = (uint8_t *) (stats_rsp_params + 1);
			temp_mask = stats_rsp_params->statsMask;
			if (temp_mask & (1 << eCsrSummaryStats))
				stats_buf += sizeof(tCsrSummaryStatsInfo);

			if (temp_mask & (1 << eCsrGlobalClassAStats)) {
				classa_stats =
					(tCsrGlobalClassAStatsInfo *) stats_buf;
				classa_stats->max_pwr = pdev_stats->chan_tx_pwr;
			}
		}
	}
}

/**
 * wma_vdev_stats_lost_link_helper() - helper function to extract
 * lost link information from vdev statistics event while deleting BSS.
 * @wma: WMA handle
 * @vdev_stats: statistics information from firmware
 *
 * This is for informing HDD to collect lost link information while
 * disconnection. Following conditions to check
 * 1. vdev is up
 * 2. bssid is zero. When handling DELETE_BSS request message, it sets bssid to
 * zero, hence add the check here to indicate the event comes during deleting
 * BSS
 * 3. DELETE_BSS is the request message queued. Put this condition check on the
 * last one as it consumes more resource searching entries in the  list
 *
 * Return: none
 */
static void wma_vdev_stats_lost_link_helper(tp_wma_handle wma,
					    wmi_vdev_stats *vdev_stats)
{
	struct wma_txrx_node *node;
	int32_t rssi;
	struct wma_target_req *req_msg;
	static const uint8_t zero_mac[QDF_MAC_ADDR_SIZE] = {0};
	int32_t bcn_snr, dat_snr;

	node = &wma->interfaces[vdev_stats->vdev_id];
	if (node->vdev_up &&
	    !qdf_mem_cmp(node->bssid, zero_mac, QDF_MAC_ADDR_SIZE)) {
		req_msg = wma_peek_vdev_req(wma, vdev_stats->vdev_id,
					    WMA_TARGET_REQ_TYPE_VDEV_STOP);
		if ((NULL == req_msg) ||
		    (WMA_DELETE_BSS_REQ != req_msg->msg_type)) {
			WMA_LOGD(FL("cannot find DELETE_BSS request message"));
			return;
		 }
		bcn_snr = vdev_stats->vdev_snr.bcn_snr;
		dat_snr = vdev_stats->vdev_snr.dat_snr;
		WMA_LOGD(FL("get vdev id %d, beancon snr %d, data snr %d"),
			vdev_stats->vdev_id, bcn_snr, dat_snr);

		if (WMA_TGT_IS_VALID_SNR(bcn_snr))
			rssi = bcn_snr;
		else if (WMA_TGT_IS_VALID_SNR(dat_snr))
			rssi = dat_snr;
		else
			rssi = WMA_TGT_INVALID_SNR;

		/* Get the absolute rssi value from the current rssi value */
		rssi = rssi + WMA_TGT_NOISE_FLOOR_DBM;
		wma_lost_link_info_handler(wma, vdev_stats->vdev_id, rssi);
	}
}

/**
 * wma_update_vdev_stats() - update vdev stats
 * @wma: wma handle
 * @vdev_stats: vdev stats
 *
 * Return: none
 */
static void wma_update_vdev_stats(tp_wma_handle wma,
				  wmi_vdev_stats *vdev_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	tCsrSummaryStatsInfo *summary_stats = NULL;
	uint8_t *stats_buf;
	struct wma_txrx_node *node;
	uint8_t i;
	int32_t rssi = 0;
	QDF_STATUS qdf_status;
	tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq *) wma->pGetRssiReq;
	cds_msg_t sme_msg = { 0 };
	int32_t bcn_snr, dat_snr;

	bcn_snr = vdev_stats->vdev_snr.bcn_snr;
	dat_snr = vdev_stats->vdev_snr.dat_snr;
	WMA_LOGD("vdev id %d beancon snr %d data snr %d",
		 vdev_stats->vdev_id, bcn_snr, dat_snr);

	node = &wma->interfaces[vdev_stats->vdev_id];
	stats_rsp_params = node->stats_rsp;
	if (stats_rsp_params) {
		stats_buf = (uint8_t *) (stats_rsp_params + 1);
		node->fw_stats_set |= FW_VDEV_STATS_SET;
		WMA_LOGD("<---FW VDEV STATS received for vdevId:%d",
			 vdev_stats->vdev_id);
		if (stats_rsp_params->statsMask & (1 << eCsrSummaryStats)) {
			summary_stats = (tCsrSummaryStatsInfo *) stats_buf;
			for (i = 0; i < 4; i++) {
				summary_stats->tx_frm_cnt[i] =
					vdev_stats->tx_frm_cnt[i];
				summary_stats->fail_cnt[i] =
					vdev_stats->fail_cnt[i];
				summary_stats->multiple_retry_cnt[i] =
					vdev_stats->multiple_retry_cnt[i];
			}

			summary_stats->rx_frm_cnt = vdev_stats->rx_frm_cnt;
			summary_stats->rx_error_cnt = vdev_stats->rx_err_cnt;
			summary_stats->rx_discard_cnt =
				vdev_stats->rx_discard_cnt;
			summary_stats->ack_fail_cnt = vdev_stats->ack_fail_cnt;
			summary_stats->rts_succ_cnt = vdev_stats->rts_succ_cnt;
			summary_stats->rts_fail_cnt = vdev_stats->rts_fail_cnt;
			/* Update SNR and RSSI in SummaryStats */
			if (WMA_TGT_IS_VALID_SNR(bcn_snr)) {
				summary_stats->snr = bcn_snr;
				summary_stats->rssi =
					bcn_snr + WMA_TGT_NOISE_FLOOR_DBM;
			} else if (WMA_TGT_IS_VALID_SNR(dat_snr)) {
				summary_stats->snr = dat_snr;
				summary_stats->rssi =
					dat_snr + WMA_TGT_NOISE_FLOOR_DBM;
			} else {
				summary_stats->snr = WMA_TGT_INVALID_SNR;
				summary_stats->rssi = 0;
			}
		}
	}

	if (pGetRssiReq && pGetRssiReq->sessionId == vdev_stats->vdev_id) {
		if (WMA_TGT_IS_VALID_SNR(bcn_snr)) {
			rssi = bcn_snr;
			rssi = rssi + WMA_TGT_NOISE_FLOOR_DBM;
		} else if (WMA_TGT_IS_VALID_SNR(dat_snr)) {
			rssi = dat_snr;
			rssi = rssi + WMA_TGT_NOISE_FLOOR_DBM;
		} else {
			/*
			 * Firmware sends invalid snr till it sees
			 * Beacon/Data after connection since after
			 * vdev up fw resets the snr to invalid.
			 * In this duartion Host will return the last know
			 * rssi during connection.
			 */
			WMA_LOGE("Invalid SNR from firmware");
		}

		WMA_LOGD("Average Rssi = %d, vdev id= %d", rssi,
			 pGetRssiReq->sessionId);

		/* update the average rssi value to UMAC layer */
		if (NULL != pGetRssiReq->rssiCallback) {
			((tCsrRssiCallback) (pGetRssiReq->rssiCallback))(rssi,
						pGetRssiReq->staId,
						pGetRssiReq->pDevContext);
		}

		qdf_mem_free(pGetRssiReq);
		wma->pGetRssiReq = NULL;
	}

	if (node->psnr_req) {
		tAniGetSnrReq *p_snr_req = node->psnr_req;

		if (WMA_TGT_IS_VALID_SNR(bcn_snr))
			p_snr_req->snr = bcn_snr;
		else if (WMA_TGT_IS_VALID_SNR(dat_snr))
			p_snr_req->snr = dat_snr;
		else
			p_snr_req->snr = WMA_TGT_INVALID_SNR;

		sme_msg.type = eWNI_SME_SNR_IND;
		sme_msg.bodyptr = p_snr_req;
		sme_msg.bodyval = 0;

		qdf_status = cds_mq_post_message(QDF_MODULE_ID_SME, &sme_msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			WMA_LOGE("%s: Fail to post snr ind msg", __func__);
			qdf_mem_free(p_snr_req);
		}

		node->psnr_req = NULL;
	}
	wma_vdev_stats_lost_link_helper(wma, vdev_stats);
}

/**
 * wma_post_stats() - update stats to PE
 * @wma: wma handle
 * @node: txrx node
 *
 * Return: none
 */
static void wma_post_stats(tp_wma_handle wma, struct wma_txrx_node *node)
{
	/* send response to UMAC */
	wma_send_msg(wma, WMA_GET_STATISTICS_RSP, node->stats_rsp, 0);
	node->stats_rsp = NULL;
	node->fw_stats_set = 0;
}

/**
 * wma_update_peer_stats() - update peer stats
 * @wma: wma handle
 * @peer_stats: peer stats
 *
 * Return: none
 */
static void wma_update_peer_stats(tp_wma_handle wma,
				  wmi_peer_stats *peer_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	tCsrGlobalClassAStatsInfo *classa_stats = NULL;
	struct wma_txrx_node *node;
	uint8_t *stats_buf, vdev_id, macaddr[IEEE80211_ADDR_LEN], mcsRateFlags;
	uint32_t temp_mask;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&peer_stats->peer_macaddr, &macaddr[0]);
	if (!wma_find_vdev_by_bssid(wma, macaddr, &vdev_id))
		return;

	node = &wma->interfaces[vdev_id];
	stats_rsp_params = (tAniGetPEStatsRsp *) node->stats_rsp;
	if (stats_rsp_params) {
		node->fw_stats_set |= FW_PEER_STATS_SET;
		WMA_LOGD("<-- FW PEER STATS received for vdevId:%d", vdev_id);
		stats_buf = (uint8_t *) (stats_rsp_params + 1);
		temp_mask = stats_rsp_params->statsMask;
		if (temp_mask & (1 << eCsrSummaryStats))
			stats_buf += sizeof(tCsrSummaryStatsInfo);

		if (temp_mask & (1 << eCsrGlobalClassAStats)) {
			classa_stats = (tCsrGlobalClassAStatsInfo *) stats_buf;
			WMA_LOGD("peer tx rate:%d", peer_stats->peer_tx_rate);
			/*The linkspeed returned by fw is in kbps so convert
			 *it in to units of 500kbps which is expected by UMAC*/
			if (peer_stats->peer_tx_rate) {
				classa_stats->tx_rate =
					peer_stats->peer_tx_rate / 500;
			}

			classa_stats->tx_rate_flags = node->rate_flags;
			if (!(node->rate_flags & eHAL_TX_RATE_LEGACY)) {
				classa_stats->mcs_index =
					wma_get_mcs_idx((peer_stats->peer_tx_rate /
							 100), node->rate_flags,
							node->nss, &mcsRateFlags);
				classa_stats->nss = node->nss;
				classa_stats->mcs_rate_flags = mcsRateFlags;
			}
			/* FW returns tx power in intervals of 0.5 dBm
			   Convert it back to intervals of 1 dBm */
			classa_stats->max_pwr =
				roundup(classa_stats->max_pwr, 2) >> 1;
		}
	}
}

/**
 * wma_post_link_status() - post link status to SME
 * @pGetLinkStatus: SME Link status
 * @link_status: Link status
 *
 * Return: none
 */
void wma_post_link_status(tAniGetLinkStatus *pGetLinkStatus,
			  uint8_t link_status)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	cds_msg_t sme_msg = { 0 };

	pGetLinkStatus->linkStatus = link_status;
	sme_msg.type = eWNI_SME_LINK_STATUS_IND;
	sme_msg.bodyptr = pGetLinkStatus;
	sme_msg.bodyval = 0;

	qdf_status = cds_mq_post_message(QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		WMA_LOGE("%s: Fail to post link status ind msg", __func__);
		qdf_mem_free(pGetLinkStatus);
	}
}

/**
 * wma_update_per_chain_rssi_stats() - to store per chain rssi stats
 * @wma: wma handle
 * @rssi_stats: rssi stats
 * @rssi_per_chain_stats: buffer where rssi stats to be stored
 *
 * This function stores per chain rssi stats received from fw for all vdevs for
 * which the stats were requested into a csr stats structure.
 *
 * Return: void
 */
static void wma_update_per_chain_rssi_stats(tp_wma_handle wma,
		wmi_rssi_stats *rssi_stats,
		struct csr_per_chain_rssi_stats_info *rssi_per_chain_stats)
{
	int i;
	int32_t bcn_snr, dat_snr;

	for (i = 0; i < NUM_CHAINS_MAX; i++) {
		bcn_snr = rssi_stats->rssi_avg_beacon[i];
		dat_snr = rssi_stats->rssi_avg_data[i];
		WMA_LOGD("chain %d beacon snr %d data snr %d",
			i, bcn_snr, dat_snr);

		if (WMA_TGT_IS_VALID_SNR(bcn_snr))
			rssi_per_chain_stats->rssi[i] = bcn_snr;
		else if (WMA_TGT_IS_VALID_SNR(dat_snr))
			rssi_per_chain_stats->rssi[i] = dat_snr;
		else
			/*
			 * Firmware sends invalid snr till it sees
			 * Beacon/Data after connection since after
			 * vdev up fw resets the snr to invalid.
			 * In this duartion Host will return an invalid rssi
			 * value.
			 */
			rssi_per_chain_stats->rssi[i] = WMA_TGT_INVALID_SNR;

		/*
		 * Get the absolute rssi value from the current rssi value the
		 * sinr value is hardcoded into 0 in the qcacld-new/CORE stack
		 */
		rssi_per_chain_stats->rssi[i] += WMA_TGT_NOISE_FLOOR_DBM;
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&(rssi_stats->peer_macaddr),
			rssi_per_chain_stats->peer_mac_addr);
	}
}

/**
 * wma_update_rssi_stats() - to update rssi stats for all vdevs
 *         for which the stats were requested.
 * @wma: wma handle
 * @rssi_stats: rssi stats
 *
 * This function updates the rssi stats for all vdevs for which
 * the stats were requested.
 *
 * Return: void
 */
static void wma_update_rssi_stats(tp_wma_handle wma,
			wmi_rssi_stats *rssi_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	struct csr_per_chain_rssi_stats_info *rssi_per_chain_stats = NULL;
	struct wma_txrx_node *node;
	uint8_t *stats_buf;
	uint32_t temp_mask;
	uint8_t vdev_id;

	vdev_id = rssi_stats->vdev_id;
	node = &wma->interfaces[vdev_id];
	stats_rsp_params = (tAniGetPEStatsRsp *) node->stats_rsp;
	if (stats_rsp_params) {
		node->fw_stats_set |=  FW_RSSI_PER_CHAIN_STATS_SET;
		WMA_LOGD("<-- FW RSSI PER CHAIN STATS received for vdevId:%d",
				vdev_id);
		stats_buf = (uint8_t *) (stats_rsp_params + 1);
		temp_mask = stats_rsp_params->statsMask;

		if (temp_mask & (1 << eCsrSummaryStats))
			stats_buf += sizeof(tCsrSummaryStatsInfo);
		if (temp_mask & (1 << eCsrGlobalClassAStats))
			stats_buf += sizeof(tCsrGlobalClassAStatsInfo);
		if (temp_mask & (1 << eCsrGlobalClassBStats))
			stats_buf += sizeof(tCsrGlobalClassBStatsInfo);
		if (temp_mask & (1 << eCsrGlobalClassCStats))
			stats_buf += sizeof(tCsrGlobalClassCStatsInfo);
		if (temp_mask & (1 << eCsrGlobalClassDStats))
			stats_buf += sizeof(tCsrGlobalClassDStatsInfo);
		if (temp_mask & (1 << eCsrPerStaStats))
			stats_buf += sizeof(tCsrPerStaStatsInfo);

		if (temp_mask & (1 << csr_per_chain_rssi_stats)) {
			rssi_per_chain_stats =
			     (struct csr_per_chain_rssi_stats_info *)stats_buf;
			wma_update_per_chain_rssi_stats(wma, rssi_stats,
					rssi_per_chain_stats);
		}
	}
}


/**
 * wma_link_status_event_handler() - link status event handler
 * @handle: wma handle
 * @cmd_param_info: data from event
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_link_status_event_handler(void *handle, uint8_t *cmd_param_info,
				  uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_UPDATE_VDEV_RATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_vdev_rate_stats_event_fixed_param *event;
	wmi_vdev_rate_ht_info *ht_info;
	struct wma_txrx_node *intr = wma->interfaces;
	uint8_t link_status = LINK_STATUS_LEGACY;
	uint32_t i;

	param_buf =
	      (WMI_UPDATE_VDEV_RATE_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	event = (wmi_vdev_rate_stats_event_fixed_param *) param_buf->fixed_param;
	ht_info = (wmi_vdev_rate_ht_info *) param_buf->ht_info;

	WMA_LOGD("num_vdev_stats: %d", event->num_vdev_stats);

	if (event->num_vdev_stats > ((WMI_SVC_MSG_MAX_SIZE -
	    sizeof(*event)) / sizeof(*ht_info)) ||
	    event->num_vdev_stats > param_buf->num_ht_info) {
		WMA_LOGE("%s: excess vdev_stats buffers:%d, num_ht_info:%d",
			__func__, event->num_vdev_stats,
			param_buf->num_ht_info);
		return -EINVAL;
	}
	for (i = 0; (i < event->num_vdev_stats) && ht_info; i++) {
		WMA_LOGD("%s vdevId:%d  tx_nss:%d rx_nss:%d tx_preamble:%d rx_preamble:%d",
			__func__, ht_info->vdevid, ht_info->tx_nss,
			ht_info->rx_nss, ht_info->tx_preamble,
			ht_info->rx_preamble);
		if (ht_info->vdevid < wma->max_bssid
		    && intr[ht_info->vdevid].plink_status_req) {
			if (ht_info->tx_nss || ht_info->rx_nss)
				link_status = LINK_STATUS_MIMO;

			if ((ht_info->tx_preamble == LINK_RATE_VHT) ||
			    (ht_info->rx_preamble == LINK_RATE_VHT))
				link_status |= LINK_STATUS_VHT;

			if (intr[ht_info->vdevid].nss == 2)
				link_status |= LINK_SUPPORT_MIMO;

			if (intr[ht_info->vdevid].rate_flags &
				(eHAL_TX_RATE_VHT20 | eHAL_TX_RATE_VHT40 |
				eHAL_TX_RATE_VHT80))
				link_status |= LINK_SUPPORT_VHT;

			wma_post_link_status(intr[ht_info->vdevid].plink_status_req,
							 link_status);
			intr[ht_info->vdevid].plink_status_req = NULL;
			link_status = LINK_STATUS_LEGACY;
		}

		ht_info++;
	}

	return 0;
}

int wma_rso_cmd_status_event_handler(wmi_roam_event_fixed_param *wmi_event)
{
	struct rso_cmd_status *rso_status;
	cds_msg_t sme_msg;
	QDF_STATUS qdf_status;

	rso_status = qdf_mem_malloc(sizeof(*rso_status));
	if (!rso_status) {
		WMA_LOGE("%s: malloc fails for rso cmd status", __func__);
		return -ENOMEM;
	}

	rso_status->vdev_id = wmi_event->vdev_id;
	if (WMI_ROAM_NOTIF_SCAN_MODE_SUCCESS == wmi_event->notif)
		rso_status->status = true;
	else if (WMI_ROAM_NOTIF_SCAN_MODE_FAIL == wmi_event->notif)
		rso_status->status = false;
	sme_msg.type = eWNI_SME_RSO_CMD_STATUS_IND;
	sme_msg.bodyptr = rso_status;
	sme_msg.bodyval = 0;
	WMA_LOGI("%s: Post RSO cmd status to SME",  __func__);

	qdf_status = cds_mq_post_message(QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		WMA_LOGE("%s: fail to post RSO cmd status to SME", __func__);
		qdf_mem_free(rso_status);
	}
	return 0;
}

/**
 * wma_handle_sta_peer_info() - handle peer information in
 * peer stats
 * @num_peer_stats: peer number
 * @peer_stats: peer stats received from firmware
 * @peer_macaddr: the specified mac address
 * @sapaddr: sap mac address
 *
 * This function will send eWNI_SME_GET_PEER_INFO_IND
 * to sme with stations' information
 *
 */
static void wma_handle_sta_peer_info(uint32_t num_peer_stats,
					wmi_peer_stats *peer_stats,
					struct qdf_mac_addr peer_macaddr,
					uint8_t *sapaddr)
{
	QDF_STATUS qdf_status;
	wmi_mac_addr temp_addr;
	struct sir_peer_info_resp *peer_info;
	cds_msg_t sme_msg = {0};
	uint32_t  i;
	uint32_t  j = 0;

	if (!qdf_is_macaddr_broadcast(&peer_macaddr)) {
		WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_macaddr.bytes, &temp_addr);
		for (i = 0; i < num_peer_stats; i++) {
			if ((((temp_addr.mac_addr47to32) & 0x0000ffff) ==
				((peer_stats->peer_macaddr.mac_addr47to32) &
								0x0000ffff))
				&& (temp_addr.mac_addr31to0 ==
				peer_stats->peer_macaddr.mac_addr31to0)) {

				break;
			}
			peer_stats = peer_stats + 1;
		}
		peer_info = qdf_mem_malloc(sizeof(*peer_info) +
				sizeof(peer_info->info[0]));
		if (NULL == peer_info) {
			WMA_LOGE("%s: Memory allocation failed.", __func__);
			return;
		}
		if (i < num_peer_stats) {
			peer_info->count = 1;
			WMI_MAC_ADDR_TO_CHAR_ARRAY(&(peer_stats->peer_macaddr),
				peer_info->info[0].peer_macaddr.bytes);
			peer_info->info[0].rssi = peer_stats->peer_rssi;
			peer_info->info[0].tx_rate = peer_stats->peer_tx_rate;
			peer_info->info[0].rx_rate = peer_stats->peer_rx_rate;
			WMA_LOGD("%s peer %pM rssi %d tx_rate %d rx_rate %d",
					 __func__,
					 peer_info->info[0].peer_macaddr.bytes,
					 peer_stats->peer_rssi,
					 peer_stats->peer_tx_rate,
					 peer_stats->peer_rx_rate);
		} else {
			WMA_LOGE("%s: no match mac address", __func__);
			peer_info->count = 0;
		}
	} else {
		peer_info = qdf_mem_malloc(sizeof(*peer_info) +
				num_peer_stats * sizeof(peer_info->info[0]));
		if (NULL == peer_info) {
			WMA_LOGE("%s: Memory allocation failed.", __func__);
			return;
		}
		peer_info->count = num_peer_stats;

		for (i = 0; i < num_peer_stats; i++) {
			WMI_MAC_ADDR_TO_CHAR_ARRAY(&(peer_stats->peer_macaddr),
					peer_info->info[j].peer_macaddr.bytes);
			peer_info->info[j].rssi = peer_stats->peer_rssi;
			peer_info->info[j].tx_rate = peer_stats->peer_tx_rate;
			peer_info->info[j].rx_rate = peer_stats->peer_rx_rate;
			WMA_LOGD("%s peer %pM rssi %d tx_rate %d rx_rate %d",
					__func__,
					peer_info->info[j].peer_macaddr.bytes,
					peer_stats->peer_rssi,
					peer_stats->peer_tx_rate,
					peer_stats->peer_rx_rate);
			if (!qdf_mem_cmp(peer_info->info[j].peer_macaddr.bytes,
					sapaddr, QDF_MAC_ADDR_SIZE)) {
				peer_info->count = peer_info->count - 1;
			} else {
				j++;
			}
			peer_stats = peer_stats + 1;
		}
		WMA_LOGD("WDA send peer num %d", peer_info->count);
	}

	sme_msg.type = eWNI_SME_GET_PEER_INFO_IND;
	sme_msg.bodyptr = peer_info;
	sme_msg.bodyval = 0;

	qdf_status = cds_mq_post_message(QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		WMA_LOGE("%s: Fail to post get rssi msg", __func__);
		qdf_mem_free(peer_info);
	}

	return;
}

/**
 * wma_stats_event_handler() - stats event handler
 * @handle: wma handle
 * @cmd_param_info: data from event
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_stats_event_handler(void *handle, uint8_t *cmd_param_info,
			    uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *event;
	wmi_pdev_stats *pdev_stats;
	wmi_vdev_stats *vdev_stats;
	wmi_peer_stats *peer_stats;
	wmi_rssi_stats *rssi_stats;
	wmi_per_chain_rssi_stats *rssi_event;
	struct wma_txrx_node *node;
	uint8_t *temp;
	uint32_t i;
	uint32_t buf_len = 0;
	bool excess_data = false;
	wmi_congestion_stats *congestion_stats;
	tpAniSirGlobal mac;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	temp = (uint8_t *) param_buf->data;
	if ((event->num_pdev_stats + event->num_vdev_stats +
	     event->num_peer_stats) > param_buf->num_data) {
		WMA_LOGE("%s: Invalid num_pdev_stats:%d or num_vdev_stats:%d or num_peer_stats:%d",
			__func__, event->num_pdev_stats, event->num_vdev_stats,
			event->num_peer_stats);
		return -EINVAL;
	}

	do {
		if (event->num_pdev_stats > ((WMI_SVC_MSG_MAX_SIZE -
		    sizeof(*event)) / sizeof(*pdev_stats))) {
			excess_data = true;
			break;
		} else {
			buf_len += event->num_pdev_stats * sizeof(*pdev_stats);
		}

		if (event->num_vdev_stats > ((WMI_SVC_MSG_MAX_SIZE -
		    sizeof(*event)) / sizeof(*vdev_stats))) {
			excess_data = true;
			break;
		} else {
			buf_len += event->num_vdev_stats * sizeof(*vdev_stats);
		}

		if (event->num_peer_stats > ((WMI_SVC_MSG_MAX_SIZE -
		    sizeof(*event)) / sizeof(*peer_stats))) {
			excess_data = true;
			break;
		} else {
			buf_len += event->num_peer_stats * sizeof(*peer_stats);
		}

		rssi_event =
			(wmi_per_chain_rssi_stats *) param_buf->chain_stats;
		if (rssi_event && (rssi_event->num_per_chain_rssi_stats >
		    ((WMI_SVC_MSG_MAX_SIZE - sizeof(*event)) /
		    sizeof(*peer_stats)))) {
			excess_data = true;
			break;
		} else {
			buf_len += rssi_event->num_per_chain_rssi_stats *
						sizeof(*peer_stats);
		}
	} while (0);

	if (excess_data ||
		(sizeof(*event) > WMI_SVC_MSG_MAX_SIZE - buf_len)) {
		WMA_LOGE("excess wmi buffer: stats pdev %d vdev %d peer %d",
			 event->num_pdev_stats, event->num_vdev_stats,
			 event->num_peer_stats);
		QDF_ASSERT(0);
		return -EINVAL;
	}

	if (event->num_pdev_stats > 0) {
		for (i = 0; i < event->num_pdev_stats; i++) {
			pdev_stats = (wmi_pdev_stats *) temp;
			wma_update_pdev_stats(wma, pdev_stats);
			temp += sizeof(wmi_pdev_stats);
		}
	}

	if (event->num_vdev_stats > 0) {
		for (i = 0; i < event->num_vdev_stats; i++) {
			vdev_stats = (wmi_vdev_stats *) temp;
			wma_update_vdev_stats(wma, vdev_stats);
			temp += sizeof(wmi_vdev_stats);
		}
	}

	if (event->num_peer_stats > 0) {
		if (wma->get_sta_peer_info == true) {
			wma_handle_sta_peer_info(event->num_peer_stats,
				(wmi_peer_stats *)temp,
				wma->peer_macaddr,
				wma->myaddr);
		} else {
		for (i = 0; i < event->num_peer_stats; i++) {
			peer_stats = (wmi_peer_stats *) temp;
			wma_update_peer_stats(wma, peer_stats);
			temp += sizeof(wmi_peer_stats);
		}
	}
	}

	rssi_event = (wmi_per_chain_rssi_stats *) param_buf->chain_stats;
	if (rssi_event) {
		if (rssi_event->num_per_chain_rssi_stats >
		    param_buf->num_rssi_stats) {
			WMA_LOGE("%s: Invalid num_per_chain_rssi_stats:%d",
				__func__, rssi_event->num_per_chain_rssi_stats);
			return -EINVAL;
		}
		if (((rssi_event->tlv_header & 0xFFFF0000) >> 16 ==
			  WMITLV_TAG_STRUC_wmi_per_chain_rssi_stats) &&
			  ((rssi_event->tlv_header & 0x0000FFFF) ==
			  WMITLV_GET_STRUCT_TLVLEN(wmi_per_chain_rssi_stats))) {
			if (rssi_event->num_per_chain_rssi_stats > 0) {
				temp = (uint8_t *) rssi_event;
				temp += sizeof(*rssi_event);

				/* skip past struct array tlv header */
				temp += WMI_TLV_HDR_SIZE;

				for (i = 0;
				     i < rssi_event->num_per_chain_rssi_stats;
				     i++) {
					rssi_stats = (wmi_rssi_stats *)temp;
					wma_update_rssi_stats(wma, rssi_stats);
					temp += sizeof(wmi_rssi_stats);
				}
			}
		}
	}

	congestion_stats = (wmi_congestion_stats *) param_buf->congestion_stats;
	if (congestion_stats) {
		if (((congestion_stats->tlv_header & 0xFFFF0000) >> 16 ==
			  WMITLV_TAG_STRUC_wmi_congestion_stats) &&
			  ((congestion_stats->tlv_header & 0x0000FFFF) ==
			  WMITLV_GET_STRUCT_TLVLEN(wmi_congestion_stats))) {
			mac = cds_get_context(QDF_MODULE_ID_PE);
			if (!mac) {
				WMA_LOGE("%s: Invalid mac", __func__);
				return -EINVAL;
			}
			if (!mac->sme.congestion_cb) {
				WMA_LOGE("%s: Callback not registered",
					__func__);
				return -EINVAL;
			}
			WMA_LOGI("%s: congestion %d", __func__,
				congestion_stats->congestion);
			mac->sme.congestion_cb(mac->hHdd,
				congestion_stats->congestion,
				congestion_stats->vdev_id);
		}
	}

	for (i = 0; i < wma->max_bssid; i++) {
		node = &wma->interfaces[i];
		if (node->fw_stats_set & FW_PEER_STATS_SET) {
			wma_post_stats(wma, node);
		}
	}

	WMA_LOGI("%s: Exit", __func__);
	return 0;
}

/**
 * dump_peer_stats_info() - dump wmi peer info struct
 * @event: wmi peer info fixed param pointer
 * @peer_stats: wmi peer stats info pointer
 *
 * This function will dump wmi peer info struct
 *
 * Return: None
 */
static void dump_peer_stats_info(wmi_peer_stats_info_event_fixed_param *event,
		wmi_peer_stats_info *peer_stats)
{
	int i;
	wmi_peer_stats_info *stats = peer_stats;
	u_int8_t mac[6];

	WMA_LOGI("%s vdev_id %d, num_peers %d more_data %d",
			__func__, event->vdev_id,
			event->num_peers, event->more_data);

	for (i = 0; i < event->num_peers; i++) {
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&stats->peer_macaddr, mac);
		WMA_LOGI("%s mac %pM", __func__, mac);
		WMA_LOGI("%s tx_bytes %d %d tx_packets %d %d",
				__func__,
				stats->tx_bytes.low_32,
				stats->tx_bytes.high_32,
				stats->tx_packets.low_32,
				stats->tx_packets.high_32);
		WMA_LOGI("%s rx_bytes %d %d rx_packets %d %d",
				__func__,
				stats->rx_bytes.low_32,
				stats->rx_bytes.high_32,
				stats->rx_packets.low_32,
				stats->rx_packets.high_32);
		WMA_LOGI("%s tx_retries %d tx_failed %d",
				__func__, stats->tx_retries, stats->tx_failed);
		WMA_LOGI("%s tx_rate_code %x rx_rate_code %x",
				__func__,
				stats->last_tx_rate_code,
				stats->last_rx_rate_code);
		WMA_LOGI("%s tx_rate %x rx_rate %x",
				__func__,
				stats->last_tx_bitrate_kbps,
				stats->last_rx_bitrate_kbps);
		WMA_LOGI("%s peer_rssi %d", __func__, stats->peer_rssi);
		stats++;
	}
}

int wma_peer_info_event_handler(void *handle, u_int8_t *cmd_param_info,
				   u_int32_t len)
{
	WMI_PEER_STATS_INFO_EVENTID_param_tlvs *param_buf;
	wmi_peer_stats_info_event_fixed_param *event;
	u_int32_t buf_size;
	u_int8_t *buf;

	param_buf =
		(WMI_PEER_STATS_INFO_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	WMA_LOGI("%s Recv WMI_PEER_STATS_INFO_EVENTID", __func__);
	event = param_buf->fixed_param;
	if (event->num_peers >
	    ((WMI_SVC_MSG_MAX_SIZE -
	      sizeof(wmi_peer_stats_info_event_fixed_param))/
	      sizeof(wmi_peer_stats_info)) || event->num_peers >
	      param_buf->num_peer_stats_info) {
		WMA_LOGE("Excess num of peers from fw: %d, num_peer_stats_info:%d",
			event->num_peers, param_buf->num_peer_stats_info);
		return -EINVAL;
	}
	buf_size = sizeof(wmi_peer_stats_info_event_fixed_param) +
		sizeof(wmi_peer_stats_info) * event->num_peers;
	buf = qdf_mem_malloc(buf_size);
	if (!buf) {
		WMA_LOGE("%s: Failed alloc memory for buf", __func__);
		return -ENOMEM;
	}

	qdf_mem_copy(buf, param_buf->fixed_param,
			sizeof(wmi_peer_stats_info_event_fixed_param));
	qdf_mem_copy((buf + sizeof(wmi_peer_stats_info_event_fixed_param)),
			param_buf->peer_stats_info,
			sizeof(wmi_peer_stats_info) * event->num_peers);
	WMA_LOGI("%s dump peer stats info", __func__);
	dump_peer_stats_info(event, param_buf->peer_stats_info);

	qdf_mem_free(buf);

	return 0;
}

/**
 * wma_send_link_speed() - send link speed to SME
 * @link_speed: link speed
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_send_link_speed(uint32_t link_speed)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tpAniSirGlobal mac_ctx;
	tSirLinkSpeedInfo *ls_ind =
		(tSirLinkSpeedInfo *) qdf_mem_malloc(sizeof(tSirLinkSpeedInfo));

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		WMA_LOGD("%s: NULL mac_ctx ptr. Exiting", __func__);
		return QDF_STATUS_E_INVAL;
	}

	if (!ls_ind) {
		WMA_LOGE("%s: Memory allocation failed.", __func__);
		qdf_status = QDF_STATUS_E_NOMEM;
	} else {
		ls_ind->estLinkSpeed = link_speed;
		if (mac_ctx->sme.pLinkSpeedIndCb)
			mac_ctx->sme.pLinkSpeedIndCb(ls_ind,
					mac_ctx->sme.pLinkSpeedCbContext);
		else
			WMA_LOGD("%s: pLinkSpeedIndCb is null", __func__);
		qdf_mem_free(ls_ind);
	}

	return qdf_status;
}

/**
 * wma_link_speed_event_handler() - link speed event handler
 * @handle: wma handle
 * @cmd_param_info: event data
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_link_speed_event_handler(void *handle, uint8_t *cmd_param_info,
				 uint32_t len)
{
	WMI_PEER_ESTIMATED_LINKSPEED_EVENTID_param_tlvs *param_buf;
	wmi_peer_estimated_linkspeed_event_fixed_param *event;
	QDF_STATUS qdf_status;

	param_buf =
		(WMI_PEER_ESTIMATED_LINKSPEED_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid linkspeed event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	qdf_status = wma_send_link_speed(event->est_linkspeed_kbps);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		return -EINVAL;
	}
	return 0;
}

/**
 * wma_wni_cfg_dnld() - cfg download request
 * @handle: wma handle
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_wni_cfg_dnld(tp_wma_handle wma_handle)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	void *mac = cds_get_context(QDF_MODULE_ID_PE);

	WMA_LOGD("%s: Enter", __func__);

	if (NULL == mac) {
		WMA_LOGP("%s: Invalid context", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}

	process_cfg_download_req(mac);

	WMA_LOGD("%s: Exit", __func__);
	return qdf_status;
}

#define BIG_ENDIAN_MAX_DEBUG_BUF   500
/**
 * wma_unified_debug_print_event_handler() - debug print event handler
 * @handle: wma handle
 * @datap: data pointer
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_unified_debug_print_event_handler(void *handle, uint8_t *datap,
					  uint32_t len)
{
	WMI_DEBUG_PRINT_EVENTID_param_tlvs *param_buf;
	uint8_t *data;
	uint32_t datalen;

	param_buf = (WMI_DEBUG_PRINT_EVENTID_param_tlvs *) datap;
	if (!param_buf) {
		WMA_LOGE("Get NULL point message from FW");
		return -ENOMEM;
	}
	data = param_buf->data;
	datalen = param_buf->num_data;

#ifdef BIG_ENDIAN_HOST
	{
		if (datalen > BIG_ENDIAN_MAX_DEBUG_BUF) {
			WMA_LOGE("%s Invalid data len %d, limiting to max",
				 __func__, datalen);
			datalen = BIG_ENDIAN_MAX_DEBUG_BUF;
		}
		char dbgbuf[BIG_ENDIAN_MAX_DEBUG_BUF] = { 0 };

		memcpy(dbgbuf, data, datalen);
		SWAPME(dbgbuf, datalen);
		WMA_LOGD("FIRMWARE:%s", dbgbuf);
		return 0;
	}
#else
	WMA_LOGD("FIRMWARE:%s", data);
	return 0;
#endif /* BIG_ENDIAN_HOST */
}

/**
 * wma_is_sap_active() - check sap is active or not
 * @handle: wma handle
 *
 * Return: true/false
 */
bool wma_is_sap_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_AP &&
		    wma_handle->interfaces[i].sub_type == 0)
			return true;
	}
	return false;
}

/**
 * wma_is_p2p_go_active() - check p2p go is active or not
 * @handle: wma handle
 *
 * Return: true/false
 */
bool wma_is_p2p_go_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_AP &&
		    wma_handle->interfaces[i].sub_type ==
		    WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO)
			return true;
	}
	return false;
}

/**
 * wma_is_p2p_cli_active() - check p2p cli is active or not
 * @handle: wma handle
 *
 * Return: true/false
 */
bool wma_is_p2p_cli_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_STA &&
		    wma_handle->interfaces[i].sub_type ==
		    WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT)
			return true;
	}
	return false;
}

/**
 * wma_is_sta_active() - check sta is active or not
 * @handle: wma handle
 *
 * Return: true/false
 */
bool wma_is_sta_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_STA &&
		    wma_handle->interfaces[i].sub_type == 0)
			return true;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_IBSS)
			return true;
	}
	return false;
}

/**
 * wma_peer_phymode() - get phymode
 * @nw_type: nw type
 * @sta_type: sta type
 * @is_ht: is ht supported
 * @is_cw40: is channel width 40 supported
 * @is_vht: is vht supported
 * @is_cw_vht: is channel width 80 supported
 *
 * Return: WLAN_PHY_MODE
 */
WLAN_PHY_MODE wma_peer_phymode(tSirNwType nw_type, uint8_t sta_type,
			       uint8_t is_ht, uint8_t ch_width,
			       uint8_t is_vht)
{
	WLAN_PHY_MODE phymode = MODE_UNKNOWN;

	switch (nw_type) {
	case eSIR_11B_NW_TYPE:
#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == sta_type) {
		if (is_vht) {
			if (CH_WIDTH_80MHZ == ch_width)
				phymode = MODE_11AC_VHT80;
			else
				phymode = (CH_WIDTH_40MHZ == ch_width) ?
					  MODE_11AC_VHT40 :
					  MODE_11AC_VHT20;
		} else if (is_ht) {
			phymode = (CH_WIDTH_40MHZ == ch_width) ?
				  MODE_11NG_HT40 : MODE_11NG_HT20;
		} else
			phymode = MODE_11B;
	} else
#endif /* FEATURE_WLAN_TDLS */
	{
		phymode = MODE_11B;
		if (is_ht || is_vht)
			WMA_LOGE("HT/VHT is enabled with 11B NW type");
	}
		break;
	case eSIR_11G_NW_TYPE:
		if (!(is_ht || is_vht)) {
			phymode = MODE_11G;
			break;
		}
		if (CH_WIDTH_40MHZ < ch_width)
			WMA_LOGE("80/160 MHz BW sent in 11G, configured 40MHz");
		if (ch_width)
			phymode = (is_vht) ?
				MODE_11AC_VHT40_2G : MODE_11NG_HT40;
		else
			phymode = (is_vht) ?
				MODE_11AC_VHT20_2G : MODE_11NG_HT20;
		break;
	case eSIR_11A_NW_TYPE:
		if (!(is_ht || is_vht)) {
			phymode = MODE_11A;
			break;
		}
		if (is_vht) {
			if (ch_width == CH_WIDTH_160MHZ)
				phymode = MODE_11AC_VHT160;
			else if (ch_width == CH_WIDTH_80P80MHZ)
				phymode = MODE_11AC_VHT80_80;
			else if (ch_width == CH_WIDTH_80MHZ)
				phymode = MODE_11AC_VHT80;
			else
				phymode = (ch_width) ?
					  MODE_11AC_VHT40 : MODE_11AC_VHT20;
		} else
			phymode = (ch_width) ? MODE_11NA_HT40 : MODE_11NA_HT20;
		break;
	default:
		WMA_LOGP("%s: Invalid nw type %d", __func__, nw_type);
		break;
	}
	WMA_LOGD("%s: nw_type %d is_ht %d ch_width %d is_vht %d phymode %d",
		  __func__, nw_type, is_ht, ch_width, is_vht, phymode);

	return phymode;
}

/**
 * wma_txrx_fw_stats_reset() - reset txrx fw statistics
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: 0 for success or return error
 */
int32_t wma_txrx_fw_stats_reset(tp_wma_handle wma_handle,
				uint8_t vdev_id, uint32_t value)
{
	struct ol_txrx_stats_req req;
	ol_txrx_vdev_handle vdev;

	vdev = wma_find_vdev_by_id(wma_handle, vdev_id);
	if (!vdev) {
		WMA_LOGE("%s:Invalid vdev handle", __func__);
		return -EINVAL;
	}
	qdf_mem_zero(&req, sizeof(req));
	req.stats_type_reset_mask = value;
	ol_txrx_fw_stats_get(vdev, &req, false, false);

	return 0;
}

#ifdef HELIUMPLUS
#define SET_UPLOAD_MASK(_mask, _rate_info) 	\
	((_mask) = 1 << (_rate_info ## _V2))
#else  /* !HELIUMPLUS */
#define SET_UPLOAD_MASK(_mask, _rate_info) 	\
	((_mask) = 1 << (_rate_info))
#endif

#ifdef HELIUMPLUS
static bool wma_is_valid_fw_stats_cmd(uint32_t value)
{
	if (value > (HTT_DBG_NUM_STATS + 1) ||
		value == (HTT_DBG_STATS_RX_RATE_INFO + 1) ||
		value == (HTT_DBG_STATS_TX_RATE_INFO + 1) ||
		value == (HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT + 1)) {
		WMA_LOGE("%s: Not supported", __func__);
		return false;
	}
	return true;
}
#else
static bool wma_is_valid_fw_stats_cmd(uint32_t value)
{
	if (value > (HTT_DBG_NUM_STATS + 1) ||
		value == (HTT_DBG_STATS_RX_RATE_INFO_V2 + 1) ||
		value == (HTT_DBG_STATS_TX_RATE_INFO_V2 + 1) ||
		value == (HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT + 1)) {
		WMA_LOGE("%s: Not supported", __func__);
		return false;
	}
	return true;
}
#endif

/**
 * wma_set_txrx_fw_stats_level() - set txrx fw stats level
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: 0 for success or return error
 */
int32_t wma_set_txrx_fw_stats_level(tp_wma_handle wma_handle,
				    uint8_t vdev_id, uint32_t value)
{
	struct ol_txrx_stats_req req;
	ol_txrx_vdev_handle vdev;
	uint32_t l_up_mask;

	vdev = wma_find_vdev_by_id(wma_handle, vdev_id);
	if (!vdev) {
		WMA_LOGE("%s:Invalid vdev handle", __func__);
		return -EINVAL;
	}

	if (wma_is_valid_fw_stats_cmd(value) == false)
		return -EINVAL;

	qdf_mem_zero(&req, sizeof(req));
	req.print.verbose = 1;

	/* TODO: Need to check how to avoid mem leak*/
	l_up_mask = 1 << (value - 1);
	req.stats_type_upload_mask = l_up_mask;

	ol_txrx_fw_stats_get(vdev, &req, false, true);

	return 0;
}

/**
 * wma_get_stats_rsp_buf() - fill get stats response buffer
 * @get_stats_param: get stats parameters
 *
 * Return: stats response buffer
 */
static tAniGetPEStatsRsp *wma_get_stats_rsp_buf
			(tAniGetPEStatsReq *get_stats_param)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	uint32_t len, temp_mask, counter = 0;

	len = sizeof(tAniGetPEStatsRsp);
	temp_mask = get_stats_param->statsMask;

	while (temp_mask) {
		if (temp_mask & 1) {
			switch (counter) {
			case eCsrSummaryStats:
				len += sizeof(tCsrSummaryStatsInfo);
				break;
			case eCsrGlobalClassAStats:
				len += sizeof(tCsrGlobalClassAStatsInfo);
				break;
			case eCsrGlobalClassBStats:
				len += sizeof(tCsrGlobalClassBStatsInfo);
				break;
			case eCsrGlobalClassCStats:
				len += sizeof(tCsrGlobalClassCStatsInfo);
				break;
			case eCsrGlobalClassDStats:
				len += sizeof(tCsrGlobalClassDStatsInfo);
				break;
			case eCsrPerStaStats:
				len += sizeof(tCsrPerStaStatsInfo);
				break;
			case csr_per_chain_rssi_stats:
				len +=
				   sizeof(struct csr_per_chain_rssi_stats_info);
				break;
			}
		}

		counter++;
		temp_mask >>= 1;
	}

	stats_rsp_params = (tAniGetPEStatsRsp *) qdf_mem_malloc(len);
	if (!stats_rsp_params) {
		WMA_LOGE("memory allocation failed for tAniGetPEStatsRsp");
		QDF_ASSERT(0);
		return NULL;
	}

	qdf_mem_zero(stats_rsp_params, len);
	stats_rsp_params->staId = get_stats_param->staId;
	stats_rsp_params->statsMask = get_stats_param->statsMask;
	stats_rsp_params->msgType = WMA_GET_STATISTICS_RSP;
	stats_rsp_params->msgLen = len - sizeof(tAniGetPEStatsRsp);
	stats_rsp_params->rc = QDF_STATUS_SUCCESS;
	return stats_rsp_params;
}

/**
 * wma_get_stats_req() - get stats request
 * @handle: wma handle
 * @get_stats_param: stats params
 *
 * Return: none
 */
void wma_get_stats_req(WMA_HANDLE handle,
		       tAniGetPEStatsReq *get_stats_param)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct wma_txrx_node *node;
	struct pe_stats_req  cmd = {0};
	tAniGetPEStatsRsp *pGetPEStatsRspParams;


	WMA_LOGD("%s: Enter", __func__);
	node = &wma_handle->interfaces[get_stats_param->sessionId];
	if (node->stats_rsp) {
		pGetPEStatsRspParams = node->stats_rsp;
		if (pGetPEStatsRspParams->staId == get_stats_param->staId &&
		    pGetPEStatsRspParams->statsMask ==
		    get_stats_param->statsMask) {
			WMA_LOGI("Stats for staId %d with stats mask %d "
				 "is pending.... ignore new request",
				 get_stats_param->staId,
				 get_stats_param->statsMask);
			goto end;
		} else {
			qdf_mem_free(node->stats_rsp);
			node->stats_rsp = NULL;
			node->fw_stats_set = 0;
		}
	}

	pGetPEStatsRspParams = wma_get_stats_rsp_buf(get_stats_param);
	if (!pGetPEStatsRspParams)
		goto end;

	node->fw_stats_set = 0;
	if (node->stats_rsp) {
		WMA_LOGD(FL("stats_rsp is not null, prev_value: %pK"),
			node->stats_rsp);
		qdf_mem_free(node->stats_rsp);
		node->stats_rsp = NULL;
	}
	node->stats_rsp = pGetPEStatsRspParams;
	wma_handle->get_sta_peer_info = false;
	WMA_LOGD("stats_rsp allocated: %pK, sta_id: %d, mask: %d, vdev_id: %d",
		node->stats_rsp, node->stats_rsp->staId,
		node->stats_rsp->statsMask, get_stats_param->sessionId);

	cmd.session_id = get_stats_param->sessionId;
	if (wmi_unified_get_stats_cmd(wma_handle->wmi_handle, &cmd,
				 node->bssid)) {

		WMA_LOGE("%s: Failed to send WMI_REQUEST_STATS_CMDID",
			 __func__);
		goto failed;
	}

	goto end;
failed:

	pGetPEStatsRspParams->rc = QDF_STATUS_E_FAILURE;
	/* send response to UMAC */
	wma_send_msg(wma_handle, WMA_GET_STATISTICS_RSP, pGetPEStatsRspParams,
		     0);
	node->stats_rsp = NULL;
end:
	qdf_mem_free(get_stats_param);
	WMA_LOGD("%s: Exit", __func__);
	return;
}

/**
 * wma_get_cca_stats() - send request to fw to get CCA
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS wma_get_cca_stats(tp_wma_handle wma_handle,
				uint8_t vdev_id)
{
	if (wmi_unified_congestion_request_cmd(wma_handle->wmi_handle,
			vdev_id)) {
		WMA_LOGE("Failed to congestion request to fw");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_beacon_buffer_by_vdev_id() - get the beacon buffer from vdev ID
 * @vdev_id: vdev id
 * @buffer_size: size of buffer
 *
 * Return: none
 */
void *wma_get_beacon_buffer_by_vdev_id(uint8_t vdev_id, uint32_t *buffer_size)
{
	tp_wma_handle wma;
	struct beacon_info *beacon;
	uint8_t *buf;
	uint32_t buf_size;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %u", __func__, vdev_id);
		return NULL;
	}

	if (!wma_is_vdev_in_ap_mode(wma, vdev_id)) {
		WMA_LOGE("%s: vdevid %d is not in AP mode", __func__, vdev_id);
		return NULL;
	}

	beacon = wma->interfaces[vdev_id].beacon;

	if (!beacon) {
		WMA_LOGE("%s: beacon invalid", __func__);
		return NULL;
	}

	qdf_spin_lock_bh(&beacon->lock);

	buf_size = qdf_nbuf_len(beacon->buf);
	buf = qdf_mem_malloc(buf_size);

	if (!buf) {
		qdf_spin_unlock_bh(&beacon->lock);
		WMA_LOGE("%s: alloc failed for beacon buf", __func__);
		return NULL;
	}

	qdf_mem_copy(buf, qdf_nbuf_data(beacon->buf), buf_size);

	qdf_spin_unlock_bh(&beacon->lock);

	if (buffer_size)
		*buffer_size = buf_size;

	return buf;
}

/**
 * wma_get_vdev_address_by_vdev_id() - lookup MAC address from vdev ID
 * @vdev_id: vdev id
 *
 * Return: mac address
 */
uint8_t *wma_get_vdev_address_by_vdev_id(uint8_t vdev_id)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %u", __func__, vdev_id);
		return NULL;
	}

	return wma->interfaces[vdev_id].addr;
}

/**
 * wma_get_interface_by_vdev_id() - lookup interface entry using vdev ID
 * @vdev_id: vdev id
 *
 * Return: entry from vdev table
 */
struct wma_txrx_node  *wma_get_interface_by_vdev_id(uint8_t vdev_id)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %u", __func__, vdev_id);
		return NULL;
	}

	return &wma->interfaces[vdev_id];
}

/**
 * wma_is_vdev_up() - return whether a vdev is up
 * @vdev_id: vdev id
 *
 * Return: true if the vdev is up, false otherwise
 */
bool wma_is_vdev_up(uint8_t vdev_id)
{
	struct wma_txrx_node *vdev = wma_get_interface_by_vdev_id(vdev_id);
	if (vdev)
		return vdev->vdev_up;
	else
		return false;
}

#if defined(QCA_WIFI_FTM)
/**
 * wma_utf_rsp() - utf response
 * @wma_handle: wma handle
 * @payload: payload
 * @len: length of payload
 *
 * Return: 0 for success or error code
 */
static int wma_utf_rsp(tp_wma_handle wma_handle, uint8_t **payload,
		       uint32_t *len)
{
	int ret = -1;
	uint32_t payload_len;

	payload_len = wma_handle->utf_event_info.length;
	if (payload_len) {
		ret = 0;

		/*
		 * The first 4 bytes holds the payload size
		 * and the actual payload sits next to it
		 */
		*payload = (uint8_t *) qdf_mem_malloc((uint32_t) payload_len
						      + sizeof(A_UINT32));
		*(A_UINT32 *) &(*payload[0]) =
			wma_handle->utf_event_info.length;
		memcpy(*payload + sizeof(A_UINT32),
		       wma_handle->utf_event_info.data, payload_len);
		wma_handle->utf_event_info.length = 0;
		*len = payload_len;
	}

	return ret;
}

/**
 * wma_post_ftm_response() - post ftm response to upper layer
 * @wma_handle: wma handle
 *
 * Return: none
 */
static void wma_post_ftm_response(tp_wma_handle wma_handle)
{
	int ret;
	uint8_t *payload;
	uint32_t data_len;
	cds_msg_t msg = { 0 };
	QDF_STATUS status;

	ret = wma_utf_rsp(wma_handle, &payload, &data_len);

	if (ret) {
		return;
	}

	sys_build_message_header(SYS_MSG_ID_FTM_RSP, &msg);
	msg.bodyptr = payload;
	msg.bodyval = 0;

	status = cds_mq_post_message(CDS_MQ_ID_SYS, &msg);

	if (status != QDF_STATUS_SUCCESS) {
		WMA_LOGE("failed to post ftm response to SYS");
		qdf_mem_free(payload);
	}
}

/**
 * wma_process_utf_event() - process utf event
 * @handle: wma handle
 * @datap: data buffer
 * @dataplen: data length
 *
 * Return: 0 for success or error code
 */
static int
wma_process_utf_event(WMA_HANDLE handle, uint8_t *datap, uint32_t dataplen)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct seg_hdr_info segHdrInfo;
	uint8_t totalNumOfSegments, currentSeq;
	WMI_PDEV_UTF_EVENTID_param_tlvs *param_buf;
	uint8_t *data;
	uint32_t datalen;

	param_buf = (WMI_PDEV_UTF_EVENTID_param_tlvs *) datap;
	if (!param_buf) {
		WMA_LOGE("Get NULL point message from FW");
		return -EINVAL;
	}
	data = param_buf->data;
	datalen = param_buf->num_data;

	if (datalen < sizeof(segHdrInfo)) {
		WMA_LOGE("message size %d is smaller than struct seg_hdr_info",
			 datalen);
		return -EINVAL;
	}

	segHdrInfo = *(struct seg_hdr_info *) &(data[0]);

	wma_handle->utf_event_info.currentSeq = (segHdrInfo.segmentInfo & 0xF);

	currentSeq = (segHdrInfo.segmentInfo & 0xF);
	totalNumOfSegments = (segHdrInfo.segmentInfo >> 4) & 0xF;

	datalen = datalen - sizeof(segHdrInfo);

	if (currentSeq == 0) {
		wma_handle->utf_event_info.expectedSeq = 0;
		wma_handle->utf_event_info.offset = 0;
	} else {
		if (wma_handle->utf_event_info.expectedSeq != currentSeq)
			WMA_LOGE("Mismatch in expecting seq expected"
				 " Seq %d got seq %d",
				 wma_handle->utf_event_info.expectedSeq,
				 currentSeq);
	}

	memcpy(&wma_handle->utf_event_info.
	       data[wma_handle->utf_event_info.offset],
	       &data[sizeof(segHdrInfo)], datalen);
	wma_handle->utf_event_info.offset =
		wma_handle->utf_event_info.offset + datalen;
	wma_handle->utf_event_info.expectedSeq++;

	if (wma_handle->utf_event_info.expectedSeq == totalNumOfSegments) {
		if (wma_handle->utf_event_info.offset != segHdrInfo.len)
			WMA_LOGE("All segs received total len mismatch.."
				 " len %zu total len %d",
				 wma_handle->utf_event_info.offset,
				 segHdrInfo.len);

		wma_handle->utf_event_info.length =
			wma_handle->utf_event_info.offset;
	}

	wma_post_ftm_response(wma_handle);

	return 0;
}

/**
 * wma_utf_detach() - utf detach
 * @wma_handle: wma handle
 *
 * Return: none
 */
void wma_utf_detach(tp_wma_handle wma_handle)
{
	if (wma_handle->utf_event_info.data) {
		qdf_mem_free(wma_handle->utf_event_info.data);
		wma_handle->utf_event_info.data = NULL;
		wma_handle->utf_event_info.length = 0;
		wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
						     WMI_PDEV_UTF_EVENTID);
	}
}

/**
 * wma_utf_attach() - utf attach
 * @wma_handle: wma handle
 *
 * Return: none
 */
void wma_utf_attach(tp_wma_handle wma_handle)
{
	int ret;

	wma_handle->utf_event_info.data = (unsigned char *)
					  qdf_mem_malloc(MAX_UTF_EVENT_LENGTH);
	wma_handle->utf_event_info.length = 0;

	ret = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						 WMI_PDEV_UTF_EVENTID,
						 wma_process_utf_event,
						 WMA_RX_SERIALIZER_CTX);

	if (ret)
		WMA_LOGP("%s: Failed to register UTF event callback", __func__);
}

/**
 * wma_utf_cmd() - utf command
 * @wma_handle: wma handle
 * @data: data
 * @len: length
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS wma_utf_cmd(tp_wma_handle wma_handle, uint8_t *data,
			      uint16_t len)
{
	struct pdev_utf_params param = {0};

	wma_handle->utf_event_info.length = 0;
	param.utf_payload = data;
	param.len = len;

	return wmi_unified_pdev_utf_cmd_send(wma_handle->wmi_handle, &param,
						WMA_WILDCARD_PDEV_ID);
}

/**
 * wma_process_ftm_command() - process ftm command
 * @wma_handle: wma handle
 * @msg_buffer: message buffer
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
wma_process_ftm_command(tp_wma_handle wma_handle,
			struct ar6k_testmode_cmd_data *msg_buffer)
{
	uint8_t *data = NULL;
	uint16_t len = 0;
	int ret;

	if (!msg_buffer)
		return QDF_STATUS_E_INVAL;

	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
		WMA_LOGE("FTM command issued in non-FTM mode");
		qdf_mem_free(msg_buffer->data);
		qdf_mem_free(msg_buffer);
		return QDF_STATUS_E_NOSUPPORT;
	}

	data = msg_buffer->data;
	len = msg_buffer->len;

	ret = wma_utf_cmd(wma_handle, data, len);

	qdf_mem_free(msg_buffer->data);
	qdf_mem_free(msg_buffer);

	if (ret)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_WIFI_FTM */

/**
 * wma_get_wcnss_software_version() - get wcnss software version
 * @p_cds_gctx: cds context
 * @pVersion: version pointer
 * @versionBufferSize: buffer size
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_get_wcnss_software_version(void *p_cds_gctx,
					  uint8_t *pVersion,
					  uint32_t versionBufferSize)
{
	tp_wma_handle wma_handle;
	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (NULL == wma_handle) {
		WMA_LOGE("%s: Failed to get wma", __func__);
		return QDF_STATUS_E_FAULT;
	}

	snprintf(pVersion, versionBufferSize, "%x",
		 (unsigned int)wma_handle->target_fw_version);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_tx_rx_ss_from_config() - Get Tx/Rx spatial stream from HW mode config
 * @mac_ss: Config which indicates the HW mode as per 'hw_mode_ss_config'
 * @tx_ss: Contains the Tx spatial stream
 * @rx_ss: Contains the Rx spatial stream
 *
 * Returns the number of spatial streams of Tx and Rx
 *
 * Return: None
 */
static void wma_get_tx_rx_ss_from_config(enum hw_mode_ss_config mac_ss,
					 uint32_t *tx_ss, uint32_t *rx_ss)
{
	switch (mac_ss) {
	case HW_MODE_SS_0x0:
		*tx_ss = 0;
		*rx_ss = 0;
		break;
	case HW_MODE_SS_1x1:
		*tx_ss = 1;
		*rx_ss = 1;
		break;
	case HW_MODE_SS_2x2:
		*tx_ss = 2;
		*rx_ss = 2;
		break;
	case HW_MODE_SS_3x3:
		*tx_ss = 3;
		*rx_ss = 3;
		break;
	case HW_MODE_SS_4x4:
		*tx_ss = 4;
		*rx_ss = 4;
		break;
	default:
		*tx_ss = 0;
		*rx_ss = 0;
	}
}

/**
 * wma_get_matching_hw_mode_index() - Get matching HW mode index
 * @wma: WMA handle
 * @mac0_tx_ss: Number of tx spatial streams of MAC0
 * @mac0_rx_ss: Number of rx spatial streams of MAC0
 * @mac0_bw: Bandwidth of MAC0 of type 'hw_mode_bandwidth'
 * @mac1_tx_ss: Number of tx spatial streams of MAC1
 * @mac1_rx_ss: Number of rx spatial streams of MAC1
 * @mac1_bw: Bandwidth of MAC1 of type 'hw_mode_bandwidth'
 * @dbs: DBS capability of type 'hw_mode_dbs_capab'
 * @dfs: Agile DFS capability of type 'hw_mode_agile_dfs_capab'
 * @sbs: SBS capability of type 'hw_mode_sbs_capab'
 *
 * Fetches the HW mode index corresponding to the HW mode provided
 *
 * Return: Positive hw mode index in case a match is found or a negative
 * value, otherwise
 */
static int8_t wma_get_matching_hw_mode_index(tp_wma_handle wma,
				uint32_t mac0_tx_ss, uint32_t mac0_rx_ss,
				enum hw_mode_bandwidth mac0_bw,
				uint32_t mac1_tx_ss, uint32_t mac1_rx_ss,
				enum hw_mode_bandwidth mac1_bw,
				enum hw_mode_dbs_capab dbs,
				enum hw_mode_agile_dfs_capab dfs,
				enum hw_mode_sbs_capab sbs)
{
	uint32_t i;
	uint32_t t_mac0_tx_ss, t_mac0_rx_ss, t_mac0_bw;
	uint32_t t_mac1_tx_ss, t_mac1_rx_ss, t_mac1_bw;
	uint32_t dbs_mode, agile_dfs_mode, sbs_mode;
	int8_t found = -EINVAL;

	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return found;
	}

	for (i = 0; i < wma->num_dbs_hw_modes; i++) {
		t_mac0_tx_ss = WMA_HW_MODE_MAC0_TX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (t_mac0_tx_ss != mac0_tx_ss)
			continue;

		t_mac0_rx_ss = WMA_HW_MODE_MAC0_RX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (t_mac0_rx_ss != mac0_rx_ss)
			continue;

		t_mac0_bw = WMA_HW_MODE_MAC0_BANDWIDTH_GET(
				wma->hw_mode.hw_mode_list[i]);
		/*
		 * Firmware advertises max bw capability as CBW 80+80
		 * for single MAC. Thus CBW 20/40/80 should also be
		 * supported, if CBW 80+80 is supported.
		 */
		if (t_mac0_bw < mac0_bw)
			continue;

		t_mac1_tx_ss = WMA_HW_MODE_MAC1_TX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (t_mac1_tx_ss != mac1_tx_ss)
			continue;

		t_mac1_rx_ss = WMA_HW_MODE_MAC1_RX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (t_mac1_rx_ss != mac1_rx_ss)
			continue;

		t_mac1_bw = WMA_HW_MODE_MAC1_BANDWIDTH_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (t_mac1_bw < mac1_bw)
			continue;

		dbs_mode = WMA_HW_MODE_DBS_MODE_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (dbs_mode != dbs)
			continue;

		agile_dfs_mode = WMA_HW_MODE_AGILE_DFS_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (agile_dfs_mode != dfs)
			continue;

		sbs_mode = WMA_HW_MODE_SBS_MODE_GET(
				wma->hw_mode.hw_mode_list[i]);
		if (sbs_mode != sbs)
			continue;

		found = i;
		WMA_LOGI("%s: hw_mode index %d found",
				__func__, i);
		break;
	}
	return found;
}

/**
 * wma_get_hw_mode_from_dbs_hw_list() - Get hw_mode index
 * @mac0_ss: MAC0 spatial stream configuration
 * @mac0_bw: MAC0 bandwidth configuration
 * @mac1_ss: MAC1 spatial stream configuration
 * @mac1_bw: MAC1 bandwidth configuration
 * @dbs: HW DBS capability
 * @dfs: HW Agile DFS capability
 * @sbs: HW SBS capability
 *
 * Get the HW mode index corresponding to the HW modes spatial stream,
 * bandwidth, DBS, Agile DFS and SBS capability
 *
 * Return: Index number if a match is found or -negative value if not found
 */
int8_t wma_get_hw_mode_idx_from_dbs_hw_list(enum hw_mode_ss_config mac0_ss,
					    enum hw_mode_bandwidth mac0_bw,
					    enum hw_mode_ss_config mac1_ss,
					    enum hw_mode_bandwidth mac1_bw,
					    enum hw_mode_dbs_capab dbs,
					    enum hw_mode_agile_dfs_capab dfs,
					    enum hw_mode_sbs_capab sbs)
{
	tp_wma_handle wma;
	uint32_t mac0_tx_ss, mac0_rx_ss;
	uint32_t mac1_tx_ss, mac1_rx_ss;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return -EINVAL;
	}

	wma_get_tx_rx_ss_from_config(mac0_ss, &mac0_tx_ss, &mac0_rx_ss);
	wma_get_tx_rx_ss_from_config(mac1_ss, &mac1_tx_ss, &mac1_rx_ss);

	WMA_LOGI("%s: MAC0: TxSS=%d, RxSS=%d, BW=%d",
		__func__, mac0_tx_ss, mac0_rx_ss, mac0_bw);
	WMA_LOGI("%s: MAC1: TxSS=%d, RxSS=%d, BW=%d",
		__func__, mac1_tx_ss, mac1_rx_ss, mac1_bw);
	WMA_LOGI("%s: DBS=%d, Agile DFS=%d, SBS=%d",
		__func__, dbs, dfs, sbs);

	return wma_get_matching_hw_mode_index(wma, mac0_tx_ss, mac0_rx_ss,
						mac0_bw,
						mac1_tx_ss, mac1_rx_ss,
						mac1_bw,
						dbs, dfs, sbs);
}

/**
 * wma_get_hw_mode_from_idx() - Get HW mode based on index
 * @idx: HW mode index
 * @hw_mode: HW mode params
 *
 * Fetches the HW mode parameters
 *
 * Return: Success if hw mode is obtained and the hw mode params
 */
QDF_STATUS wma_get_hw_mode_from_idx(uint32_t idx,
		struct sir_hw_mode_params *hw_mode)
{
	tp_wma_handle wma;
	uint32_t param;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (idx > wma->num_dbs_hw_modes) {
		WMA_LOGE("%s: Invalid index", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (!wma->num_dbs_hw_modes) {
		WMA_LOGE("%s: No dbs hw modes available", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	param = wma->hw_mode.hw_mode_list[idx];

	hw_mode->mac0_tx_ss = WMA_HW_MODE_MAC0_TX_STREAMS_GET(param);
	hw_mode->mac0_rx_ss = WMA_HW_MODE_MAC0_RX_STREAMS_GET(param);
	hw_mode->mac0_bw = WMA_HW_MODE_MAC0_BANDWIDTH_GET(param);
	hw_mode->mac1_tx_ss = WMA_HW_MODE_MAC1_TX_STREAMS_GET(param);
	hw_mode->mac1_rx_ss = WMA_HW_MODE_MAC1_RX_STREAMS_GET(param);
	hw_mode->mac1_bw = WMA_HW_MODE_MAC1_BANDWIDTH_GET(param);
	hw_mode->dbs_cap = WMA_HW_MODE_DBS_MODE_GET(param);
	hw_mode->agile_dfs_cap = WMA_HW_MODE_AGILE_DFS_GET(param);
	hw_mode->sbs_cap = WMA_HW_MODE_SBS_MODE_GET(param);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_num_dbs_hw_modes() - Get number of HW mode
 *
 * Fetches the number of DBS HW modes returned by the FW
 *
 * Return: Negative value on error or returns the number of DBS HW modes
 */
int8_t wma_get_num_dbs_hw_modes(void)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return -EINVAL;
	}
	return wma->num_dbs_hw_modes;
}

/**
 * wma_is_hw_dbs_capable() - Check if HW is DBS capable
 *
 * Checks if the HW is DBS capable
 *
 * Return: true if the HW is DBS capable
 */
bool wma_is_hw_dbs_capable(void)
{
	tp_wma_handle wma;
	uint32_t param, i, found = 0;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return false;
	}

	if (!wma_is_dbs_enable()) {
		WMA_LOGI("%s: DBS is disabled", __func__);
		return false;
	}

	WMA_LOGI("%s: DBS service bit map: %d", __func__,
		WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
		WMI_SERVICE_DUAL_BAND_SIMULTANEOUS_SUPPORT));

	/* The agreement with FW is that: To know if the target is DBS
	 * capable, DBS needs to be supported both in the HW mode list
	 * and in the service ready event
	 */
	if (!(WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_DUAL_BAND_SIMULTANEOUS_SUPPORT)))
		return false;

	for (i = 0; i < wma->num_dbs_hw_modes; i++) {
		param = wma->hw_mode.hw_mode_list[i];
		WMA_LOGI("%s: HW param: %x", __func__, param);
		if (WMA_HW_MODE_DBS_MODE_GET(param)) {
			WMA_LOGI("%s: HW (%d) is DBS capable", __func__, i);
			found = 1;
			break;
		}
	}

	if (found)
		return true;

	return false;
}

/**
 * wma_get_cds_hw_mode_change_from_hw_mode_index - Returns value in terms of
 * cds_hw_mode_change enums derived from hw_mode_index.
 *
 * Returns cds_hw_mode_change value derived from hw_mode_index.
 *
 * Return: value in terms of cds_hw_mode_change enums.
 */
enum cds_hw_mode_change
wma_get_cds_hw_mode_change_from_hw_mode_index(uint32_t hw_mode_index)
{
	tp_wma_handle wma;
	uint32_t param = 0;
	enum cds_hw_mode_change value = CDS_HW_MODE_NOT_IN_PROGRESS;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		goto ret_value;
	}

	WMA_LOGI("%s: HW param: %x", __func__, param);

	param = wma->hw_mode.hw_mode_list[hw_mode_index];
	if (WMA_HW_MODE_DBS_MODE_GET(param)) {
		WMA_LOGI("%s: DBS is requested with HW (%d)", __func__,
			 hw_mode_index);
		value = CDS_DBS_IN_PROGRESS;
		goto ret_value;
	}

	if (WMA_HW_MODE_SBS_MODE_GET(param)) {
		WMA_LOGI("%s: SBS is requested with HW (%d)", __func__,
			 hw_mode_index);
		value = CDS_SBS_IN_PROGRESS;
		goto ret_value;
	}

	value = CDS_SMM_IN_PROGRESS;
	WMA_LOGI("%s: SMM is requested with HW (%d)", __func__,
		 hw_mode_index);
ret_value:
	return value;
}

/**
 * wma_get_mac_id_of_vdev() - Get MAC id corresponding to a vdev
 * @vdev_id: VDEV whose MAC ID is required
 *
 * Get MAC id corresponding to a vdev id from the WMA structure
 *
 * Return: Negative value on failure and MAC id on success
 */
int8_t wma_get_mac_id_of_vdev(uint32_t vdev_id)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return -EINVAL;
	}

	if (wma->interfaces)
		return wma->interfaces[vdev_id].mac_id;

	return -EINVAL;
}

/**
 * wma_get_old_and_new_hw_index() - Get the old and new HW index
 * @old_hw_mode_index: Value at this pointer contains the old HW mode index
 * Default value when not configured is WMA_DEFAULT_HW_MODE_INDEX
 * @new_hw_mode_index: Value at this pointer contains the new HW mode index
 * Default value when not configured is WMA_DEFAULT_HW_MODE_INDEX
 *
 * Get the old and new HW index configured in the driver
 *
 * Return: Failure in case the HW mode indices cannot be fetched and Success
 * otherwise. When no HW mode transition has happened the values of
 * old_hw_mode_index and new_hw_mode_index will be the same.
 */
QDF_STATUS wma_get_old_and_new_hw_index(uint32_t *old_hw_mode_index,
				    uint32_t *new_hw_mode_index)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return QDF_STATUS_E_INVAL;
	}

	*old_hw_mode_index = wma->old_hw_mode_index;
	*new_hw_mode_index = wma->new_hw_mode_index;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_update_intf_hw_mode_params() - Update WMA params
 * @vdev_id: VDEV id whose params needs to be updated
 * @mac_id: MAC id to be updated
 * @cfgd_hw_mode_index: HW mode index from which Tx and Rx SS will be updated
 *
 * Updates the MAC id, tx spatial stream, rx spatial stream in WMA
 *
 * Return: None
 */
void wma_update_intf_hw_mode_params(uint32_t vdev_id, uint32_t mac_id,
				    uint32_t cfgd_hw_mode_index)
{
	tp_wma_handle wma;
	uint32_t param;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return;
	}

	if (!wma->interfaces) {
		WMA_LOGE("%s: Interface is NULL", __func__);
		return;
	}

	if (cfgd_hw_mode_index > wma->num_dbs_hw_modes) {
		WMA_LOGE("%s: Invalid index", __func__);
		return;
	}

	param = wma->hw_mode.hw_mode_list[cfgd_hw_mode_index];
	wma->interfaces[vdev_id].mac_id = mac_id;
	if (mac_id == 0) {
		wma->interfaces[vdev_id].tx_streams =
			WMA_HW_MODE_MAC0_TX_STREAMS_GET(param);
		wma->interfaces[vdev_id].rx_streams =
			WMA_HW_MODE_MAC0_RX_STREAMS_GET(param);
	} else {
		wma->interfaces[vdev_id].tx_streams =
			WMA_HW_MODE_MAC1_TX_STREAMS_GET(param);
		wma->interfaces[vdev_id].rx_streams =
			WMA_HW_MODE_MAC1_RX_STREAMS_GET(param);
	}
}

/**
 * wma_get_dbs_hw_modes() - Get the DBS HW modes for userspace
 * @one_by_one_dbs: 1x1 DBS capability of HW
 * @two_by_two_dbs: 2x2 DBS capability of HW
 *
 * Provides the DBS HW mode capability such as whether
 * 1x1 DBS, 2x2 DBS is supported by the HW or not.
 *
 * Return: Failure in case of error and 0 on success
 *         one_by_one_dbs/two_by_two_dbs will be false,
 *         if they are not supported.
 *         one_by_one_dbs/two_by_two_dbs will be true,
 *         if they are supported.
 *         false values of one_by_one_dbs/two_by_two_dbs,
 *         indicate DBS is disabled
 */
QDF_STATUS wma_get_dbs_hw_modes(bool *one_by_one_dbs, bool *two_by_two_dbs)
{
	tp_wma_handle wma;
	uint32_t i;
	int8_t found_one_by_one = -EINVAL, found_two_by_two = -EINVAL;
	uint32_t conf1_tx_ss, conf1_rx_ss;
	uint32_t conf2_tx_ss, conf2_rx_ss;

	*one_by_one_dbs = false;
	*two_by_two_dbs = false;

	if (wma_is_hw_dbs_capable() == false) {
		WMA_LOGE("%s: HW is not DBS capable", __func__);
		/* Caller will understand that DBS is disabled */
		return QDF_STATUS_SUCCESS;

	}

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	/* To check 1x1 capability */
	wma_get_tx_rx_ss_from_config(HW_MODE_SS_1x1,
			&conf1_tx_ss, &conf1_rx_ss);
	/* To check 2x2 capability */
	wma_get_tx_rx_ss_from_config(HW_MODE_SS_2x2,
			&conf2_tx_ss, &conf2_rx_ss);

	for (i = 0; i < wma->num_dbs_hw_modes; i++) {
		uint32_t t_conf0_tx_ss, t_conf0_rx_ss;
		uint32_t t_conf1_tx_ss, t_conf1_rx_ss;
		uint32_t dbs_mode;

		t_conf0_tx_ss = WMA_HW_MODE_MAC0_TX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		t_conf0_rx_ss = WMA_HW_MODE_MAC0_RX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		t_conf1_tx_ss = WMA_HW_MODE_MAC1_TX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		t_conf1_rx_ss = WMA_HW_MODE_MAC1_RX_STREAMS_GET(
				wma->hw_mode.hw_mode_list[i]);
		dbs_mode = WMA_HW_MODE_DBS_MODE_GET(
				wma->hw_mode.hw_mode_list[i]);

		if (((((t_conf0_tx_ss == conf1_tx_ss) &&
		    (t_conf0_rx_ss == conf1_rx_ss)) ||
		    ((t_conf1_tx_ss == conf1_tx_ss) &&
		    (t_conf1_rx_ss == conf1_rx_ss))) &&
		    (dbs_mode == HW_MODE_DBS)) &&
		    (found_one_by_one < 0)) {
			found_one_by_one = i;
			WMA_LOGI("%s: 1x1 hw_mode index %d found",
					__func__, i);
			/* Once an entry is found, need not check for 1x1
			 * again
			 */
			continue;
		}

		if (((((t_conf0_tx_ss == conf2_tx_ss) &&
		    (t_conf0_rx_ss == conf2_rx_ss)) ||
		    ((t_conf1_tx_ss == conf2_tx_ss) &&
		    (t_conf1_rx_ss == conf2_rx_ss))) &&
		    (dbs_mode == HW_MODE_DBS)) &&
		    (found_two_by_two < 0)) {
			found_two_by_two = i;
			WMA_LOGI("%s: 2x2 hw_mode index %d found",
					__func__, i);
			/* Once an entry is found, need not check for 2x2
			 * again
			 */
			continue;
		}
	}

	if (found_one_by_one >= 0)
		*one_by_one_dbs = true;
	if (found_two_by_two >= 0)
		*two_by_two_dbs = true;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_current_hw_mode() - Get current HW mode params
 * @hw_mode: HW mode parameters
 *
 * Provides the current HW mode parameters if the HW mode is initialized
 * in the driver
 *
 * Return: Success if the current HW mode params are successfully populated
 */
QDF_STATUS wma_get_current_hw_mode(struct sir_hw_mode_params *hw_mode)
{
	QDF_STATUS status;
	uint32_t old_hw_index = 0, new_hw_index = 0;

	WMA_LOGI("%s: Get the current hw mode", __func__);

	status = wma_get_old_and_new_hw_index(&old_hw_index,
			&new_hw_index);
	if (QDF_STATUS_SUCCESS != status) {
		WMA_LOGE("%s: Failed to get HW mode index", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (new_hw_index == WMA_DEFAULT_HW_MODE_INDEX) {
		WMA_LOGE("%s: HW mode is not yet initialized", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	status = wma_get_hw_mode_from_idx(new_hw_index, hw_mode);
	if (QDF_STATUS_SUCCESS != status) {
		WMA_LOGE("%s: Failed to get HW mode index", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_is_current_hwmode_dbs() - Check if current hw mode is DBS
 *
 * Checks if current hardware mode of the system is DBS or no
 *
 * Return: true or false
 */
bool wma_is_current_hwmode_dbs(void)
{
	struct sir_hw_mode_params hw_mode;

	if (!wma_is_hw_dbs_capable())
		return false;
	if (QDF_STATUS_SUCCESS != wma_get_current_hw_mode(&hw_mode))
		return false;
	if (hw_mode.dbs_cap)
		return true;
	return false;
}

/**
 * wma_is_dbs_enable() - Check if master DBS control is enabled
 *
 * Checks if the master DBS control is enabled. This will be used
 * to override any other DBS capability
 *
 * Return: True if master DBS control is enabled
 */
bool wma_is_dbs_enable(void)
{
	tp_wma_handle wma;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return false;
	}

	WMA_LOGD("%s: DBS=%d", __func__,
	    WMI_DBS_FW_MODE_CFG_DBS_GET(wma->dual_mac_cfg.cur_fw_mode_config));

	if (WMI_DBS_FW_MODE_CFG_DBS_GET(wma->dual_mac_cfg.cur_fw_mode_config))
		return true;

	return false;
}

/**
 * wma_get_updated_scan_config() - Get the updated scan configuration
 * @scan_config: Pointer containing the updated scan config
 * @dbs_scan: 0 or 1 indicating if DBS scan needs to be enabled/disabled
 * @dbs_plus_agile_scan: 0 or 1 indicating if DBS plus agile scan needs to be
 * enabled/disabled
 * @single_mac_scan_with_dfs: 0 or 1 indicating if single MAC scan with DFS
 * needs to be enabled/disabled
 *
 * Takes the current scan configuration and set the necessary scan config
 * bits to either 0/1 and provides the updated value to the caller who
 * can use this to pass it on to the FW
 *
 * Return: 0 on success
 */
QDF_STATUS wma_get_updated_scan_config(uint32_t *scan_config,
		bool dbs_scan,
		bool dbs_plus_agile_scan,
		bool single_mac_scan_with_dfs)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	*scan_config = wma->dual_mac_cfg.cur_scan_config;

	WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_SET(*scan_config, dbs_scan);
	WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_SET(*scan_config,
			dbs_plus_agile_scan);
	WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_SET(*scan_config,
			single_mac_scan_with_dfs);

	WMA_LOGD("%s: *scan_config:%x ", __func__, *scan_config);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_updated_fw_mode_config() - Get the updated fw mode configuration
 * @fw_mode_config: Pointer containing the updated fw mode config
 * @dbs: 0 or 1 indicating if DBS needs to be enabled/disabled
 * @agile_dfs: 0 or 1 indicating if agile DFS needs to be enabled/disabled
 *
 * Takes the current fw mode configuration and set the necessary fw mode config
 * bits to either 0/1 and provides the updated value to the caller who
 * can use this to pass it on to the FW
 *
 * Return: 0 on success
 */
QDF_STATUS wma_get_updated_fw_mode_config(uint32_t *fw_mode_config,
		bool dbs,
		bool agile_dfs)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	*fw_mode_config = wma->dual_mac_cfg.cur_fw_mode_config;

	WMI_DBS_FW_MODE_CFG_DBS_SET(*fw_mode_config, dbs);
	WMI_DBS_FW_MODE_CFG_AGILE_DFS_SET(*fw_mode_config, agile_dfs);

	WMA_LOGD("%s: *fw_mode_config:%x ", __func__, *fw_mode_config);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_dbs_config() - Get DBS bit
 *
 * Gets the DBS bit of fw_mode_config_bits
 *
 * Return: 0 or 1 to indicate the DBS bit
 */
bool wma_get_dbs_config(void)
{
	tp_wma_handle wma;
	uint32_t fw_mode_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	fw_mode_config = wma->dual_mac_cfg.cur_fw_mode_config;

	return WMI_DBS_FW_MODE_CFG_DBS_GET(fw_mode_config);
}

/**
 * wma_get_agile_dfs_config() - Get Agile DFS bit
 *
 * Gets the Agile DFS bit of fw_mode_config_bits
 *
 * Return: 0 or 1 to indicate the Agile DFS bit
 */
bool wma_get_agile_dfs_config(void)
{
	tp_wma_handle wma;
	uint32_t fw_mode_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	fw_mode_config = wma->dual_mac_cfg.cur_fw_mode_config;

	return WMI_DBS_FW_MODE_CFG_AGILE_DFS_GET(fw_mode_config);
}

/**
 * wma_get_dbs_scan_config() - Get DBS scan bit
 *
 * Gets the DBS scan bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the DBS scan bit
 */
bool wma_get_dbs_scan_config(void)
{
	tp_wma_handle wma;
	uint32_t scan_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = wma->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_GET(scan_config);
}

/**
 * wma_get_dbs_plus_agile_scan_config() - Get DBS plus agile scan bit
 *
 * Gets the DBS plus agile scan bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the DBS plus agile scan bit
 */
bool wma_get_dbs_plus_agile_scan_config(void)
{
	tp_wma_handle wma;
	uint32_t scan_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = wma->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_GET(scan_config);
}

/**
 * wma_get_single_mac_scan_with_dfs_config() - Get Single MAC scan with DFS bit
 *
 * Gets the Single MAC scan with DFS bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the Single MAC scan with DFS bit
 */
bool wma_get_single_mac_scan_with_dfs_config(void)
{
	tp_wma_handle wma;
	uint32_t scan_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = wma->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_GET(scan_config);
}

/**
 * wma_is_dual_mac_disabled_in_ini() - Check if dual mac is disabled in INI
 *
 * Checks if the dual mac feature is disabled in INI
 *
 * Return: true if the dual mac feature is disabled from INI
 */
bool wma_is_dual_mac_disabled_in_ini(void)
{
	tpAniSirGlobal mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		WMA_LOGE("%s: Invalid mac pointer", __func__);
		return true;
	}

	if (mac->dual_mac_feature_disable)
		return true;

	return false;
}

/**
 * wma_get_prev_dbs_config() - Get prev DBS bit
 *
 * Gets the previous DBS bit of fw_mode_config_bits
 *
 * Return: 0 or 1 to indicate the DBS bit
 */
bool wma_get_prev_dbs_config(void)
{
	tp_wma_handle wma;
	uint32_t fw_mode_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	fw_mode_config = wma->dual_mac_cfg.prev_fw_mode_config;

	return WMI_DBS_FW_MODE_CFG_DBS_GET(fw_mode_config);
}

/**
 * wma_get_prev_agile_dfs_config() - Get prev Agile DFS bit
 *
 * Gets the previous Agile DFS bit of fw_mode_config_bits
 *
 * Return: 0 or 1 to indicate the Agile DFS bit
 */
bool wma_get_prev_agile_dfs_config(void)
{
	tp_wma_handle wma;
	uint32_t fw_mode_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	fw_mode_config = wma->dual_mac_cfg.prev_fw_mode_config;

	return WMI_DBS_FW_MODE_CFG_AGILE_DFS_GET(fw_mode_config);
}

/**
 * wma_get_prev_dbs_scan_config() - Get prev DBS scan bit
 *
 * Gets the previous DBS scan bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the DBS scan bit
 */
bool wma_get_prev_dbs_scan_config(void)
{
	tp_wma_handle wma;
	uint32_t scan_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = wma->dual_mac_cfg.prev_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_GET(scan_config);
}

/**
 * wma_get_prev_dbs_plus_agile_scan_config() - Get prev DBS plus agile scan bit
 *
 * Gets the previous DBS plus agile scan bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the DBS plus agile scan bit
 */
bool wma_get_prev_dbs_plus_agile_scan_config(void)
{
	tp_wma_handle wma;
	uint32_t scan_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = wma->dual_mac_cfg.prev_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_GET(scan_config);
}

/**
 * wma_get_prev_single_mac_scan_with_dfs_config() - Get prev Single MAC scan
 * with DFS bit
 *
 * Gets the previous Single MAC scan with DFS bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the Single MAC scan with DFS bit
 */
bool wma_get_prev_single_mac_scan_with_dfs_config(void)
{
	tp_wma_handle wma;
	uint32_t scan_config;

	if (wma_is_dual_mac_disabled_in_ini())
		return false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = wma->dual_mac_cfg.prev_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_GET(scan_config);
}

/**
 * wma_is_scan_simultaneous_capable() - Check if scan parallelization is
 * supported or not
 *
 * currently scan parallelization feature support is dependent on DBS but
 * it can be independent in future.
 *
 * Return: True if master DBS control is enabled
 */
bool wma_is_scan_simultaneous_capable(void)
{
	if (wma_is_hw_dbs_capable())
		return true;

	return false;
}

/**
 * wma_get_vht_ch_width - return vht channel width
 *
 * Return: return vht channel width
 */
uint32_t wma_get_vht_ch_width(void)
{
	uint32_t fw_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
	tp_wma_handle wm_hdl = cds_get_context(QDF_MODULE_ID_WMA);

	if (NULL == wm_hdl)
		return fw_ch_wd;

	if (wm_hdl->vht_cap_info & WMI_VHT_CAP_CH_WIDTH_80P80_160MHZ)
		fw_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ;
	else if (wm_hdl->vht_cap_info & WMI_VHT_CAP_CH_WIDTH_160MHZ)
		fw_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;

	return fw_ch_wd;
}

/**
 * wma_get_num_of_setbits_from_bitmask() - to get num of setbits from bitmask
 * @mask: given bitmask
 *
 * This helper function should return number of setbits from bitmask
 *
 * Return: number of setbits from bitmask
 */
uint32_t wma_get_num_of_setbits_from_bitmask(uint32_t mask)
{
	uint32_t num_of_setbits = 0;

	while (mask) {
		mask &= (mask - 1);
		num_of_setbits++;
	}
	return num_of_setbits;
}

/**
 * wma_config_debug_module_cmd - set debug log config
 * @wmi_handle: wmi layer handle
 * @param: debug log parameter
 * @val: debug log value
 * @module_id_bitmap: debug module id bitmap
 * @bitmap_len:  debug module bitmap length
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
wma_config_debug_module_cmd(wmi_unified_t wmi_handle, A_UINT32 param,
			    A_UINT32 val, A_UINT32 *module_id_bitmap,
			    A_UINT32 bitmap_len)
{
	struct dbglog_params dbg_param;
	dbg_param.param = param;
	dbg_param.val = val;
	dbg_param.module_id_bitmap = module_id_bitmap;
	dbg_param.bitmap_len = bitmap_len;

	return wmi_unified_dbglog_cmd_send(wmi_handle, &dbg_param);
}

/**
 * wma_is_p2p_lo_capable() - if driver is capable of p2p listen offload
 *
 * This function checks if driver is capable of p2p listen offload
 *    true: capable of p2p offload
 *    false: not capable
 *
 * Return: true - capable, false - not capable
 */
bool wma_is_p2p_lo_capable(void)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return false;
	}

	if (WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_P2P_LISTEN_OFFLOAD_SUPPORT))
		return true;

	return false;
}

QDF_STATUS wma_get_rcpi_req(WMA_HANDLE handle,
			    struct sme_rcpi_req *rcpi_request)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct rcpi_req  cmd = {0};
	struct wma_txrx_node *iface;
	struct sme_rcpi_req *node_rcpi_req = NULL;

	WMA_LOGD("%s: Enter", __func__);
	iface = &wma_handle->interfaces[rcpi_request->session_id];
	/* command is in progress */
	if (iface->rcpi_req != NULL) {
		WMA_LOGE("%s : previous rcpi request is pending", __func__);
		return QDF_STATUS_SUCCESS;
	}

	node_rcpi_req = qdf_mem_malloc(sizeof(*node_rcpi_req));
	if (!node_rcpi_req) {
		WMA_LOGE("Failed to allocate memory for rcpi_request");
		return QDF_STATUS_E_NOMEM;
	}

	node_rcpi_req->session_id = rcpi_request->session_id;
	node_rcpi_req->measurement_type = rcpi_request->measurement_type;
	node_rcpi_req->rcpi_callback = rcpi_request->rcpi_callback;
	node_rcpi_req->rcpi_context = rcpi_request->rcpi_context;
	node_rcpi_req->mac_addr = rcpi_request->mac_addr;
	iface->rcpi_req = node_rcpi_req;

	cmd.vdev_id = rcpi_request->session_id;
	qdf_mem_copy(cmd.mac_addr, &rcpi_request->mac_addr, QDF_MAC_ADDR_SIZE);

	switch (rcpi_request->measurement_type) {

	case RCPI_MEASUREMENT_TYPE_AVG_MGMT:
		cmd.measurement_type = WMI_RCPI_MEASUREMENT_TYPE_AVG_MGMT;
		break;

	case RCPI_MEASUREMENT_TYPE_AVG_DATA:
		cmd.measurement_type = WMI_RCPI_MEASUREMENT_TYPE_AVG_DATA;
		break;

	case RCPI_MEASUREMENT_TYPE_LAST_MGMT:
		cmd.measurement_type = WMI_RCPI_MEASUREMENT_TYPE_LAST_MGMT;
		break;

	case RCPI_MEASUREMENT_TYPE_LAST_DATA:
		cmd.measurement_type = WMI_RCPI_MEASUREMENT_TYPE_LAST_DATA;
		break;

	default:
		/*
		 * invalid rcpi measurement type, fall back to
		 * RCPI_MEASUREMENT_TYPE_AVG_MGMT
		 */
		cmd.measurement_type = WMI_RCPI_MEASUREMENT_TYPE_AVG_MGMT;
		break;
	}

	if (wmi_unified_get_rcpi_cmd(wma_handle->wmi_handle, &cmd)) {
		WMA_LOGE("%s: Failed to send WMI_REQUEST_RCPI_CMDID",
			 __func__);
		iface->rcpi_req = NULL;
		qdf_mem_free(node_rcpi_req);
		return QDF_STATUS_E_FAILURE;
	}

	WMA_LOGD("%s: Exit", __func__);

	return QDF_STATUS_SUCCESS;
}

int wma_rcpi_event_handler(void *handle, uint8_t *cmd_param_info,
			    uint32_t len)
{
	WMI_UPDATE_RCPI_EVENTID_param_tlvs *param_buf;
	wmi_update_rcpi_event_fixed_param *event;
	struct sme_rcpi_req *rcpi_req;
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE];
	struct qdf_mac_addr qdf_mac;
	struct wma_txrx_node *iface;
	enum rcpi_measurement_type msmt_type;
	QDF_STATUS rcpi_status = QDF_STATUS_SUCCESS;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma = (tp_wma_handle)handle;

	param_buf = (WMI_UPDATE_RCPI_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid rcpi event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	if (event->vdev_id >= wma->max_bssid) {
		WMA_LOGE("%s: received invalid vdev_id %d",
			 __func__, event->vdev_id);
		return -EINVAL;
	}
	iface = &wma->interfaces[event->vdev_id];

	if (!iface->rcpi_req) {
		WMI_LOGE("rcpi_req buffer not available");
		return 0;
	}

	rcpi_req = iface->rcpi_req;
	if (!rcpi_req->rcpi_callback) {
		iface->rcpi_req = NULL;
		qdf_mem_free(rcpi_req);
		return 0;
	}
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->peer_macaddr, mac_addr);

	switch (event->measurement_type) {
	case WMI_RCPI_MEASUREMENT_TYPE_AVG_MGMT:
		msmt_type = RCPI_MEASUREMENT_TYPE_AVG_MGMT;
		break;

	case WMI_RCPI_MEASUREMENT_TYPE_AVG_DATA:
		msmt_type = RCPI_MEASUREMENT_TYPE_AVG_DATA;
		break;

	case WMI_RCPI_MEASUREMENT_TYPE_LAST_MGMT:
		msmt_type = RCPI_MEASUREMENT_TYPE_LAST_MGMT;
		break;

	case WMI_RCPI_MEASUREMENT_TYPE_LAST_DATA:
		msmt_type = RCPI_MEASUREMENT_TYPE_LAST_DATA;
		break;

	default:
		WMI_LOGE("Invalid rcpi measurement type from firmware");
		rcpi_status = QDF_STATUS_E_INVAL;
		break;
	}


	if (!QDF_IS_STATUS_SUCCESS(rcpi_status) ||
	    (event->vdev_id != rcpi_req->session_id) ||
	    (msmt_type != rcpi_req->measurement_type) ||
	    (qdf_mem_cmp(mac_addr, &rcpi_req->mac_addr, QDF_MAC_ADDR_SIZE))) {
		WMI_LOGE("invalid rcpi_req for specified rcpi_req");
		iface->rcpi_req = NULL;
		qdf_mem_free(rcpi_req);
		return 0;
	}

	qdf_mem_copy(&qdf_mac, mac_addr, QDF_MAC_ADDR_SIZE);

	if (event->status)
		status = QDF_STATUS_E_FAILURE;
	else
		status = QDF_STATUS_SUCCESS;

	(rcpi_req->rcpi_callback)(rcpi_req->rcpi_context, qdf_mac, event->rcpi,
				  status);
	iface->rcpi_req = NULL;
	qdf_mem_free(rcpi_req);

	return 0;
}

/**
 * wma_next_peer_log_index() - atomically increment and wrap around index value
 * @index: address of index to increment
 * @size: wrap around this value
 *
 * Return: new value of index
 */
static int wma_next_peer_log_index(qdf_atomic_t *index, int size)
{
	int i = qdf_atomic_inc_return(index);

	if (i == WMA_PEER_DEBUG_MAX_REC)
		qdf_atomic_sub(WMA_PEER_DEBUG_MAX_REC, index);
	while (i >= size)
		i -= WMA_PEER_DEBUG_MAX_REC;

	return i;
}

/**
 * wma_peer_debug_log() - Add a debug log entry into peer debug records
 * @vdev_id: vdev identifier
 * @op: operation identifier
 * @peer_id: peer id
 * @mac_addr: mac address of peer, can be NULL
 * @peer_obj: peer object address, can be NULL
 * @arg1: extra argument #1
 * @arg2: extra argument #2
 *
 * Return: none
 */
void wma_peer_debug_log(uint8_t vdev_id, uint8_t op,
			uint16_t peer_id, void *mac_addr,
			void *peer_obj, uint32_t arg1, uint32_t arg2)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	uint32_t i;
	struct peer_debug_rec *rec;

	if (!wma) {
		WMA_LOGE("%s: WMA handle NULL. Exiting", __func__);
		return;
	}

	i = wma_next_peer_log_index(&wma->peer_dbg->index,
				    WMA_PEER_DEBUG_MAX_REC);
	rec = &wma->peer_dbg->rec[i];
	rec->time = qdf_get_log_timestamp();
	rec->operation = op;
	rec->vdev_id = vdev_id;
	rec->peer_id = peer_id;
	if (mac_addr)
		qdf_mem_copy(rec->mac_addr.bytes, mac_addr,
			     IEEE80211_ADDR_LEN);
	else
		qdf_mem_zero(rec->mac_addr.bytes,
			     IEEE80211_ADDR_LEN);
	rec->peer_obj = peer_obj;
	rec->arg1 = arg1;
	rec->arg2 = arg2;
}

/**
 * wma_peer_debug_string() - convert operation value to printable string
 * @op: operation identifier
 *
 * Return: printable string for the operation
 */
static char *wma_peer_debug_string(uint32_t op)
{
	switch (op) {
	case DEBUG_PEER_CREATE_SEND:
		return "peer create send";
	case DEBUG_PEER_CREATE_RESP:
		return "peer create resp_event";
	case DEBUG_PEER_DELETE_SEND:
		return "peer delete send";
	case DEBUG_PEER_DELETE_RESP:
		return "peer delete resp_event";
	case DEBUG_PEER_MAP_EVENT:
		return "peer map event";
	case DEBUG_PEER_UNMAP_EVENT:
		return "peer unmap event";
	case DEBUG_PEER_UNREF_DELETE:
		return "peer unref delete";
	case DEBUG_DELETING_PEER_OBJ:
		return "peer obj deleted";
	case DEBUG_ROAM_SYNCH_IND:
		return "roam synch ind event";
	case DEBUG_ROAM_SYNCH_CNF:
		return "roam sync conf sent";
	case DEBUG_ROAM_SYNCH_FAIL:
		return "roam sync fail event";
	case DEBUG_ROAM_EVENT:
		return "roam event";
	case DEBUG_WOW_ROAM_EVENT:
		return "wow wakeup roam event";
	case DEBUG_BUS_SUSPEND:
		return "host suspend";
	case DEBUG_BUS_RESUME:
		return "host wakeup";
	case DEBUG_WOW_REASON:
		return "wow wakeup reason";
	default:
		return "unknown";
	}
}

/**
 * wma_peer_debug_dump() - Print the peer debug log records
 * print all the valid debug records in the order of timestamp
 *
 * Return: none
 */
void wma_peer_debug_dump(void)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	uint32_t i;
	uint32_t current_index;
	struct peer_debug_rec *dbg_rec;
	uint64_t startt = 0;
	uint32_t delta;

	if (!wma) {
		WMA_LOGE("%s: WMA handle NULL. Exiting", __func__);
		return;
	}

	if (!wma) {
		WMA_LOGD("%s: WMA handle NULL. Exiting", __func__);
		return;
	}

#define DEBUG_CLOCK_TICKS_PER_MSEC 19200

	current_index = qdf_atomic_read(&wma->peer_dbg->index);
	if (current_index < 0) {
		WMA_LOGE("%s: No records to dump", __func__);
		return;
	} else {
		WMA_LOGD("%s: Dumping all records. current index %d",
			 __func__, current_index);
	}

	i = current_index;
	do {
		/* wrap around */
		i = (i + 1) % WMA_PEER_DEBUG_MAX_REC;
		dbg_rec = &wma->peer_dbg->rec[i];
		/* skip unused entry */
		if (dbg_rec->time == 0)
			continue;
		if (startt == 0)
			startt = dbg_rec->time;

		/*
		 * Divide by 19200 == right shift 8 bits, then divide by 75
		 * 32 bit computation keeps both 32 and 64 bit compilers happy.
		 * The value will roll over after approx. 33554 seconds.
		 */
		delta = (uint32_t) (((dbg_rec->time - startt) >> 8) &
				    0xffffffff);
		delta = delta / (DEBUG_CLOCK_TICKS_PER_MSEC >> 8);

		WMA_LOGD("index = %5d timestamp = 0x%016llx delta ms = %-12u "
			 "info = %-24s vdev_id = %-3d mac addr = %pM "
			 "peer obj = 0x%p peer_id = %-4d "
			 "arg1 = 0x%-8x arg2 = 0x%-8x",
			 i,
			 dbg_rec->time,
			 delta,
			 wma_peer_debug_string(dbg_rec->operation),
			 (int8_t) dbg_rec->vdev_id,
			 dbg_rec->mac_addr.bytes,
			 dbg_rec->peer_obj,
			 (int8_t) dbg_rec->peer_id,
			 dbg_rec->arg1,
			 dbg_rec->arg2);
	} while (i != current_index);
}

void wma_acquire_wakelock(qdf_wake_lock_t *wl, uint32_t msec)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	cds_host_diag_log_work(wl, msec, WIFI_POWER_EVENT_WAKELOCK_WMI_CMD_RSP);
	qdf_wake_lock_timeout_acquire(wl, msec);
	qdf_runtime_pm_prevent_suspend(&wma->wmi_cmd_rsp_runtime_lock);
}

void wma_release_wakelock(qdf_wake_lock_t *wl)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	qdf_wake_lock_release(wl, WIFI_POWER_EVENT_WAKELOCK_WMI_CMD_RSP);
	qdf_runtime_pm_allow_suspend(&wma->wmi_cmd_rsp_runtime_lock);
}

QDF_STATUS
wma_send_vdev_start_to_fw(t_wma_handle *wma, struct vdev_start_params *params)
{
	QDF_STATUS status;
	struct wma_txrx_node *vdev = &wma->interfaces[params->vdev_id];

	wma_acquire_wakelock(&vdev->vdev_start_wakelock,
			     WMA_VDEV_START_REQUEST_TIMEOUT);
	status = wmi_unified_vdev_start_send(wma->wmi_handle, params);
	if (QDF_IS_STATUS_ERROR(status))
		wma_release_wakelock(&vdev->vdev_start_wakelock);

	return status;
}

QDF_STATUS wma_send_vdev_stop_to_fw(t_wma_handle *wma, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct wma_txrx_node *vdev = &wma->interfaces[vdev_id];

	wma_acquire_wakelock(&vdev->vdev_stop_wakelock,
			     WMA_VDEV_STOP_REQUEST_TIMEOUT);
	status = wmi_unified_vdev_stop_send(wma->wmi_handle, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		wma_release_wakelock(&vdev->vdev_stop_wakelock);

	return status;
}

QDF_STATUS wma_send_vdev_up_to_fw(t_wma_handle *wma,
				  struct vdev_up_params *params,
				  uint8_t bssid[IEEE80211_ADDR_LEN])
{
	QDF_STATUS status;
	struct wma_txrx_node *vdev = &wma->interfaces[params->vdev_id];

	status = wmi_unified_vdev_up_send(wma->wmi_handle, bssid, params);
	wma_release_wakelock(&vdev->vdev_start_wakelock);

	return status;
}

QDF_STATUS wma_send_vdev_down_to_fw(t_wma_handle *wma, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct wma_txrx_node *vdev = &wma->interfaces[vdev_id];

	status = wmi_unified_vdev_down_send(wma->wmi_handle, vdev_id);
	wma_release_wakelock(&vdev->vdev_start_wakelock);

	return status;
}

int wma_chip_power_save_failure_detected_handler(void *handle,
						 uint8_t  *cmd_param_info,
						 uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID_param_tlvs *param_buf;
	wmi_chip_power_save_failure_detected_fixed_param  *event;
	struct chip_pwr_save_fail_detected_params  pwr_save_fail_params;
	tpAniSirGlobal mac = (tpAniSirGlobal)cds_get_context(
						QDF_MODULE_ID_PE);
	if (NULL == wma) {
		WMA_LOGE("%s: wma_handle is NULL", __func__);
		return -EINVAL;
	}
	if (!mac) {
		WMA_LOGE("%s: Invalid mac context", __func__);
		return -EINVAL;
	}
	if (!mac->sme.chip_power_save_fail_cb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
	}

	param_buf =
	(WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID_param_tlvs *)
	cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid pwr_save_fail_params breached event",
			 __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	pwr_save_fail_params.failure_reason_code =
				event->power_save_failure_reason_code;
	pwr_save_fail_params.wake_lock_bitmap[0] =
				event->protocol_wake_lock_bitmap[0];
	pwr_save_fail_params.wake_lock_bitmap[1] =
				event->protocol_wake_lock_bitmap[1];
	pwr_save_fail_params.wake_lock_bitmap[2] =
				event->protocol_wake_lock_bitmap[2];
	pwr_save_fail_params.wake_lock_bitmap[3] =
				event->protocol_wake_lock_bitmap[3];
	mac->sme.chip_power_save_fail_cb(mac->hHdd,
				&pwr_save_fail_params);

	WMA_LOGD("%s: Invoke HDD pwr_save_fail callback", __func__);
	return 0;
}
/**
 * wma_get_event_bitmap_idx() - get indices for extended wow bitmaps
 * @event: wow event
 * @wow_bitmap_size: WOW bitmap size
 * @bit_idx: bit index
 * @idx: byte index
 *
 * Return: none
 */
static inline void wma_get_event_bitmap_idx(WOW_WAKE_EVENT_TYPE event,
			      uint32_t wow_bitmap_size,
			      uint32_t *bit_idx,
			      uint32_t *idx)
{

	if (!bit_idx || !idx || wow_bitmap_size == 0)
		return;
	if (event == 0)
		*idx = *bit_idx = 0;
	else {
		*idx = event / (wow_bitmap_size * 8);
		*bit_idx = event % (wow_bitmap_size * 8);
	}
}

/**
 * wma_set_vc_mode_config() - set voltage corner mode config to FW.
 * @wma_handle:	pointer to wma handle.
 * @vc_bitmap:	value needs to set to firmware.
 *
 * At the time of driver startup, set operating voltage corner mode
 * for differenet phymode and bw configurations.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_vc_mode_config(void *wma_handle,
		uint32_t vc_bitmap)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle)wma_handle;
	struct pdev_params pdevparam;

	pdevparam.param_id = WMI_PDEV_UPDATE_WDCVS_ALGO;
	pdevparam.param_value = vc_bitmap;

	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
			&pdevparam,
			WMA_WILDCARD_PDEV_ID);
	if (ret) {
		WMA_LOGE("Fail to Set Voltage Corner config (0x%x)",
			vc_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	WMA_LOGD("Successfully Set Voltage Corner config (0x%x)",
		vc_bitmap);

	return QDF_STATUS_SUCCESS;
}


void wma_set_wow_event_bitmap(WOW_WAKE_EVENT_TYPE event,
			      uint32_t wow_bitmap_size,
			      uint32_t *bitmask)
{
	uint32_t bit_idx = 0, idx = 0;

	if (!bitmask || wow_bitmap_size < WMI_WOW_MAX_EVENT_BM_LEN) {
		WMA_LOGE("%s: wow bitmask length shorter than %d",
			 __func__, WMI_WOW_MAX_EVENT_BM_LEN);
		return;
	}
	wma_get_event_bitmap_idx(event, wow_bitmap_size, &bit_idx, &idx);
	bitmask[idx] |= 1 << bit_idx;

	WMA_LOGI("%s: bitmask updated %x%x%x%x",
		 __func__, bitmask[0], bitmask[1], bitmask[2], bitmask[3]);
}

inline bool wma_is_wow_bitmask_zero(uint32_t *bitmask,
				    uint32_t wow_bitmap_size)
{
	int i = 0;

	if (!bitmask || wow_bitmap_size < WMI_WOW_MAX_EVENT_BM_LEN)
		WMA_LOGE("%s: wow bitmask length shorter than %d",
			 __func__, WMI_WOW_MAX_EVENT_BM_LEN);
	else {
		for (; i < WMI_WOW_MAX_EVENT_BM_LEN; i++) {
			if (bitmask[i] != 0)
				return false;
		}
	}
	return true;
}

void wma_set_sta_wow_bitmask(uint32_t *bitmask, uint32_t wow_bitmap_size)
{

	if (!bitmask || wow_bitmap_size < WMI_WOW_MAX_EVENT_BM_LEN) {
		WMA_LOGE("%s: wow bitmask length shorter than %d",
			 __func__, WMI_WOW_MAX_EVENT_BM_LEN);
		return;
	}
	wma_set_wow_event_bitmap(WOW_CSA_IE_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_CLIENT_KICKOUT_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_PATTERN_MATCH_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_MAGIC_PKT_RECVD_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_DEAUTH_RECVD_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_DISASSOC_RECVD_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_BMISS_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_GTK_ERR_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_BETTER_AP_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_HTT_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_RA_MATCH_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_NLO_DETECTED_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_EXTSCAN_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_OEM_RESPONSE_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_TDLS_CONN_TRACKER_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	/* Add further STA wakeup events above this line. */
}

void wma_set_sap_wow_bitmask(uint32_t *bitmask, uint32_t wow_bitmap_size)
{

	if (!bitmask || wow_bitmap_size < WMI_WOW_MAX_EVENT_BM_LEN) {
		WMA_LOGE("%s: wow bitmask length shorter than %d",
			 __func__, WMI_WOW_MAX_EVENT_BM_LEN);
		return;
	}

	wma_set_wow_event_bitmap(WOW_PROBE_REQ_WPS_IE_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_PATTERN_MATCH_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_AUTH_REQ_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_ASSOC_REQ_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_DEAUTH_RECVD_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_DISASSOC_RECVD_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);
	wma_set_wow_event_bitmap(WOW_HTT_EVENT,
			     WMI_WOW_MAX_EVENT_BM_LEN,
			     bitmask);

	wma_set_wow_event_bitmap(WOW_CLIENT_KICKOUT_EVENT,
			WMI_WOW_MAX_EVENT_BM_LEN,
			bitmask);

	/* Add further SAP wakeup events above this line. */
}

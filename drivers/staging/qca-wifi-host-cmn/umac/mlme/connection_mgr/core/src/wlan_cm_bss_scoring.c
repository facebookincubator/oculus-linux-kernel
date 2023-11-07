/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains bss scoring logic
 */

#ifdef WLAN_POLICY_MGR_ENABLE
#include "wlan_policy_mgr_api.h"
#endif
#include <include/wlan_psoc_mlme.h>
#include "wlan_psoc_mlme_api.h"
#include "cfg_ucfg_api.h"
#include "wlan_cm_bss_score_param.h"
#include "wlan_scan_api.h"
#include "wlan_crypto_global_api.h"
#include "wlan_mgmt_txrx_utils_api.h"
#ifdef CONN_MGR_ADV_FEATURE
#include "wlan_mlme_api.h"
#include "wlan_wfa_tgt_if_tx_api.h"
#endif

#define CM_PCL_RSSI_THRESHOLD -75

#define CM_BAND_2G_INDEX                   0
#define CM_BAND_5G_INDEX                   1
#define CM_BAND_6G_INDEX                   2
/* 3 is reserved */
#define CM_MAX_BAND_INDEX                  4

#define CM_SCORE_INDEX_0                   0
#define CM_SCORE_INDEX_3                   3
#define CM_SCORE_INDEX_7                   7
#define CM_SCORE_OFFSET_INDEX_7_4          4
#define CM_SCORE_INDEX_11                  11
#define CM_SCORE_OFFSET_INDEX_11_8         8
#define CM_SCORE_MAX_INDEX                 15
#define CM_SCORE_OFFSET_INDEX_15_12        12

#define CM_MAX_OCE_WAN_DL_CAP 16

#define CM_MAX_CHANNEL_WEIGHT 100
#define CM_MAX_CHANNEL_UTILIZATION 100
#define CM_MAX_ESTIMATED_AIR_TIME_FRACTION 255
#define CM_MAX_AP_LOAD 255

#define CM_MAX_WEIGHT_OF_PCL_CHANNELS 255
#define CM_PCL_GROUPS_WEIGHT_DIFFERENCE 20

/* Congestion threshold (channel load %) to consider band and OCE WAN score */
#define CM_CONGESTION_THRSHOLD_FOR_BAND_OCE_SCORE 75

#define CM_RSSI_WEIGHTAGE 20
#define CM_HT_CAPABILITY_WEIGHTAGE 2
#define CM_VHT_CAP_WEIGHTAGE 1
#define CM_HE_CAP_WEIGHTAGE 2
#define CM_CHAN_WIDTH_WEIGHTAGE 12
#define CM_CHAN_BAND_WEIGHTAGE 2
#define CM_NSS_WEIGHTAGE 20
#define CM_SECURITY_WEIGHTAGE 4
#define CM_BEAMFORMING_CAP_WEIGHTAGE 2
#define CM_PCL_WEIGHT 10
#define CM_CHANNEL_CONGESTION_WEIGHTAGE 5
#define CM_OCE_WAN_WEIGHTAGE 2
#define CM_OCE_AP_TX_POWER_WEIGHTAGE 5
#define CM_OCE_SUBNET_ID_WEIGHTAGE 3
#define CM_SAE_PK_AP_WEIGHTAGE 30
#define CM_BEST_CANDIDATE_MAX_WEIGHT 200
#define CM_MAX_PCT_SCORE 100
#define CM_MAX_INDEX_PER_INI 4
#define CM_SLO_CONGESTION_MAX_SCORE 80

/*
 * This macro give percentage value of security_weightage to be used as per
 * security Eg if AP security is WPA 10% will be given for AP.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): WPA - Def 25%
 *     1 Index (BITS 8-15): WPA2- Def 50%
 *     2 Index (BITS 16-23): WPA3- Def 100%
 *     3 Index (BITS 24-31): reserved
 *
 * if AP security is Open/WEP 0% will be given for AP
 * These percentage values are stored in HEX. For any index max value, can be 64
 */
#define CM_SECURITY_INDEX_WEIGHTAGE 0x00643219

#define CM_BEST_CANDIDATE_MAX_BSS_SCORE (CM_BEST_CANDIDATE_MAX_WEIGHT * 100)
#define CM_AVOID_CANDIDATE_MIN_SCORE 1

#define CM_GET_SCORE_PERCENTAGE(value32, bw_index) \
	QDF_GET_BITS(value32, (8 * (bw_index)), 8)
#define CM_SET_SCORE_PERCENTAGE(value32, score_pcnt, bw_index) \
	QDF_SET_BITS(value32, (8 * (bw_index)), 8, score_pcnt)

#ifdef CONN_MGR_ADV_FEATURE
/* 3.2 us + 0.8 us(GI) */
#define PPDU_PAYLOAD_SYMBOL_DUR_US 4
/* 12.8 us + (0.8 + 1.6)/2 us(GI) */
#define HE_PPDU_PAYLOAD_SYMBOL_DUR_US 14
#define MAC_HEADER_LEN 26
/* Minimum snrDb supported by LUT */
#define SNR_DB_TO_BIT_PER_TONE_LUT_MIN -10
/* Maximum snrDb supported by LUT */
#define SNR_DB_TO_BIT_PER_TONE_LUT_MAX 9
#define DB_NUM 20
/*
 * A fudge factor to represent HW implementation margin in dB.
 * Predicted throughput matches pretty well with OTA throughput with this
 * fudge factor.
 */
#define SNR_MARGIN_DB 16
#define TWO_IN_DB 3
static int32_t
SNR_DB_TO_BIT_PER_TONE_LUT[DB_NUM] = {0, 171, 212, 262, 323, 396, 484,
586, 706, 844, 1000, 1176, 1370, 1583, 1812, 2058, 2317, 2588, 2870, 3161};
#endif

/* MLO link types */
enum MLO_TYPE {
	SLO,
	MLSR,
	MLMR,
	MLO_TYPE_MAX
};

static bool cm_is_better_bss(struct scan_cache_entry *bss1,
			     struct scan_cache_entry *bss2)
{
	if (bss1->bss_score > bss2->bss_score)
		return true;
	else if (bss1->bss_score == bss2->bss_score)
		if (bss1->rssi_raw > bss2->rssi_raw)
			return true;

	return false;
}

/**
 * cm_get_rssi_pcnt_for_slot() - calculate rssi % score based on the slot
 * index between the high rssi and low rssi threshold
 * @high_rssi_threshold: High rssi of the window
 * @low_rssi_threshold: low rssi of the window
 * @high_rssi_pcnt: % score for the high rssi
 * @low_rssi_pcnt: %score for the low rssi
 * @bucket_size: bucket size of the window
 * @bss_rssi: Input rssi for which value need to be calculated
 *
 * Return: rssi pct to use for the given rssi
 */
static inline
int8_t cm_get_rssi_pcnt_for_slot(int32_t high_rssi_threshold,
				 int32_t low_rssi_threshold,
				 uint32_t high_rssi_pcnt,
				 uint32_t low_rssi_pcnt,
				 uint32_t bucket_size, int8_t bss_rssi)
{
	int8_t slot_index, slot_size, rssi_diff, num_slot, rssi_pcnt;

	num_slot = ((high_rssi_threshold -
		     low_rssi_threshold) / bucket_size) + 1;
	slot_size = ((high_rssi_pcnt - low_rssi_pcnt) +
		     (num_slot / 2)) / (num_slot);
	rssi_diff = high_rssi_threshold - bss_rssi;
	slot_index = (rssi_diff / bucket_size) + 1;
	rssi_pcnt = high_rssi_pcnt - (slot_size * slot_index);
	if (rssi_pcnt < low_rssi_pcnt)
		rssi_pcnt = low_rssi_pcnt;

	mlme_debug("Window %d -> %d pcnt range %d -> %d bucket_size %d bss_rssi %d num_slot %d slot_size %d rssi_diff %d slot_index %d rssi_pcnt %d",
		   high_rssi_threshold, low_rssi_threshold, high_rssi_pcnt,
		   low_rssi_pcnt, bucket_size, bss_rssi, num_slot, slot_size,
		   rssi_diff, slot_index, rssi_pcnt);

	return rssi_pcnt;
}

/**
 * cm_calculate_rssi_score() - Calculate RSSI score based on AP RSSI
 * @score_param: rssi score params
 * @rssi: rssi of the AP
 * @rssi_weightage: rssi_weightage out of total weightage
 *
 * Return: rssi score
 */
static int32_t cm_calculate_rssi_score(struct rssi_config_score *score_param,
				       int32_t rssi, uint8_t rssi_weightage)
{
	int8_t rssi_pcnt;
	int32_t total_rssi_score;
	int32_t best_rssi_threshold;
	int32_t good_rssi_threshold;
	int32_t bad_rssi_threshold;
	uint32_t good_rssi_pcnt;
	uint32_t bad_rssi_pcnt;
	uint32_t good_bucket_size;
	uint32_t bad_bucket_size;

	best_rssi_threshold = score_param->best_rssi_threshold * (-1);
	good_rssi_threshold = score_param->good_rssi_threshold * (-1);
	bad_rssi_threshold = score_param->bad_rssi_threshold * (-1);
	good_rssi_pcnt = score_param->good_rssi_pcnt;
	bad_rssi_pcnt = score_param->bad_rssi_pcnt;
	good_bucket_size = score_param->good_rssi_bucket_size;
	bad_bucket_size = score_param->bad_rssi_bucket_size;

	total_rssi_score = (CM_MAX_PCT_SCORE * rssi_weightage);

	/*
	 * If RSSI is better than the best rssi threshold then it return full
	 * score.
	 */
	if (rssi > best_rssi_threshold)
		return total_rssi_score;
	/*
	 * If RSSI is less or equal to bad rssi threshold then it return
	 * least score.
	 */
	if (rssi <= bad_rssi_threshold)
		return (total_rssi_score * bad_rssi_pcnt) / 100;

	/* RSSI lies between best to good rssi threshold */
	if (rssi > good_rssi_threshold)
		rssi_pcnt = cm_get_rssi_pcnt_for_slot(best_rssi_threshold,
				good_rssi_threshold, 100, good_rssi_pcnt,
				good_bucket_size, rssi);
	else
		rssi_pcnt = cm_get_rssi_pcnt_for_slot(good_rssi_threshold,
				bad_rssi_threshold, good_rssi_pcnt,
				bad_rssi_pcnt, bad_bucket_size,
				rssi);

	return (total_rssi_score * rssi_pcnt) / 100;
}

/**
 * cm_rssi_is_same_bucket() - check if both rssi fall in same bucket
 * @rssi_top_thresh: high rssi threshold of the the window
 * @rssi_ref1: rssi ref one
 * @rssi_ref2: rssi ref two
 * @bucket_size: bucket size of the window
 *
 * Return: true if both fall in same window
 */
static inline bool cm_rssi_is_same_bucket(int8_t rssi_top_thresh,
					  int8_t rssi_ref1, int8_t rssi_ref2,
					  int8_t bucket_size)
{
	int8_t rssi_diff1 = 0;
	int8_t rssi_diff2 = 0;

	rssi_diff1 = rssi_top_thresh - rssi_ref1;
	rssi_diff2 = rssi_top_thresh - rssi_ref2;

	return (rssi_diff1 / bucket_size) == (rssi_diff2 / bucket_size);
}

/**
 * cm_get_rssi_prorate_pct() - Calculate prorated RSSI score
 * based on AP RSSI. This will be used to determine HT VHT score
 * @score_param: rssi score params
 * @rssi: bss rssi
 * @rssi_weightage: rssi_weightage out of total weightage
 *
 * If rssi is greater than good threshold return 100, if less than bad return 0,
 * if between good and bad, return prorated rssi score for the index.
 *
 * Return: rssi prorated score
 */
static int8_t
cm_get_rssi_prorate_pct(struct rssi_config_score *score_param,
			int32_t rssi, uint8_t rssi_weightage)
{
	int32_t good_rssi_threshold;
	int32_t bad_rssi_threshold;
	int8_t rssi_pref_5g_rssi_thresh;
	bool same_bucket;

	good_rssi_threshold = score_param->good_rssi_threshold * (-1);
	bad_rssi_threshold = score_param->bad_rssi_threshold * (-1);
	rssi_pref_5g_rssi_thresh = score_param->rssi_pref_5g_rssi_thresh * (-1);

	/* If RSSI is greater than good rssi return full weight */
	if (rssi > good_rssi_threshold)
		return CM_MAX_PCT_SCORE;

	same_bucket = cm_rssi_is_same_bucket(good_rssi_threshold, rssi,
					     rssi_pref_5g_rssi_thresh,
					     score_param->bad_rssi_bucket_size);
	if (same_bucket || (rssi < rssi_pref_5g_rssi_thresh))
		return 0;
	/* If RSSI is less or equal to bad rssi threshold then it return 0 */
	if (rssi <= bad_rssi_threshold)
		return 0;

	/* If RSSI is between good and bad threshold */
	return cm_get_rssi_pcnt_for_slot(good_rssi_threshold,
					 bad_rssi_threshold,
					 score_param->good_rssi_pcnt,
					 score_param->bad_rssi_pcnt,
					 score_param->bad_rssi_bucket_size,
					 rssi);
}

/**
 * cm_get_score_for_index() - get score for the given index
 * @index: index for which we need the score
 * @weightage: weigtage for the param
 * @score: per slot score
 *
 * Return: score for the index
 */
static int32_t cm_get_score_for_index(uint8_t index,
				      uint8_t weightage,
				      struct per_slot_score *score)
{
	if (index <= CM_SCORE_INDEX_3)
		return weightage * CM_GET_SCORE_PERCENTAGE(
				   score->score_pcnt3_to_0,
				   index);
	else if (index <= CM_SCORE_INDEX_7)
		return weightage * CM_GET_SCORE_PERCENTAGE(
				   score->score_pcnt7_to_4,
				   index - CM_SCORE_OFFSET_INDEX_7_4);
	else if (index <= CM_SCORE_INDEX_11)
		return weightage * CM_GET_SCORE_PERCENTAGE(
				   score->score_pcnt11_to_8,
				   index - CM_SCORE_OFFSET_INDEX_11_8);
	else
		return weightage * CM_GET_SCORE_PERCENTAGE(
				   score->score_pcnt15_to_12,
				   index - CM_SCORE_OFFSET_INDEX_15_12);
}

/**
 * cm_get_congestion_pct() - Calculate congestion pct from esp/qbss load
 * @entry: bss information
 *
 * Return: congestion pct
 */
static int32_t cm_get_congestion_pct(struct scan_cache_entry *entry)
{
	uint32_t ap_load = 0;
	uint32_t est_air_time_percentage = 0;
	uint32_t congestion = 0;

	if (entry->air_time_fraction) {
		/* Convert 0-255 range to percentage */
		est_air_time_percentage = entry->air_time_fraction *
							CM_MAX_CHANNEL_WEIGHT;
		est_air_time_percentage = qdf_do_div(est_air_time_percentage,
					   CM_MAX_ESTIMATED_AIR_TIME_FRACTION);
		/*
		 * Calculate channel congestion from estimated air time
		 * fraction.
		 */
		congestion = CM_MAX_CHANNEL_UTILIZATION -
					est_air_time_percentage;
		if (!congestion)
			congestion = 1;
	} else if (util_scan_entry_qbssload(entry)) {
		ap_load = (entry->qbss_chan_load * CM_MAX_PCT_SCORE);
		/*
		 * Calculate ap_load in % from qbss channel load from
		 * 0-255 range
		 */
		congestion = qdf_do_div(ap_load, CM_MAX_AP_LOAD);
		if (!congestion)
			congestion = 1;
	}

	return congestion;
}

/**
 * cm_calculate_congestion_score() - Calculate congestion score
 * @entry: bss information
 * @score_params: bss score params
 * @congestion_pct: congestion pct
 * @rssi_bad_zone:
 *
 * Return: congestion score
 */
static int32_t cm_calculate_congestion_score(struct scan_cache_entry *entry,
					     struct scoring_cfg *score_params,
					     uint32_t *congestion_pct,
					     bool rssi_bad_zone)
{
	uint32_t window_size;
	uint8_t index;
	int32_t good_rssi_threshold;
	uint8_t chan_congestion_weight;

	chan_congestion_weight =
		score_params->weight_config.channel_congestion_weightage;

	if (!entry)
		return chan_congestion_weight *
			   CM_GET_SCORE_PERCENTAGE(
			   score_params->esp_qbss_scoring.score_pcnt3_to_0,
			   CM_SCORE_INDEX_0);

	*congestion_pct = cm_get_congestion_pct(entry);

	if (!score_params->esp_qbss_scoring.num_slot)
		return 0;

	if (score_params->esp_qbss_scoring.num_slot >
	    CM_SCORE_MAX_INDEX)
		score_params->esp_qbss_scoring.num_slot =
			CM_SCORE_MAX_INDEX;

	good_rssi_threshold =
		score_params->rssi_score.good_rssi_threshold * (-1);

	/* For bad zone rssi get score from last index */
	if (rssi_bad_zone || entry->rssi_raw <= good_rssi_threshold)
		return cm_get_score_for_index(
			score_params->esp_qbss_scoring.num_slot,
			chan_congestion_weight,
			&score_params->esp_qbss_scoring);

	if (!*congestion_pct)
		return chan_congestion_weight *
			   CM_GET_SCORE_PERCENTAGE(
			   score_params->esp_qbss_scoring.score_pcnt3_to_0,
			   CM_SCORE_INDEX_0);

	window_size = CM_MAX_PCT_SCORE /
			score_params->esp_qbss_scoring.num_slot;

	/* Desired values are from 1 to 15, as 0 is for not present. so do +1 */
	index = qdf_do_div(*congestion_pct, window_size) + 1;

	if (index > score_params->esp_qbss_scoring.num_slot)
		index = score_params->esp_qbss_scoring.num_slot;

	return cm_get_score_for_index(index,
				      chan_congestion_weight,
				      &score_params->esp_qbss_scoring);
}

/**
 * cm_calculate_nss_score() - Calculate congestion score
 * @psoc: psoc ptr
 * @score_config: scoring config
 * @ap_nss: ap nss
 * @prorated_pct: prorated % to return dependent on RSSI
 * @sta_nss: Sta NSS
 *
 * Return: nss score
 */
static int32_t cm_calculate_nss_score(struct wlan_objmgr_psoc *psoc,
				      struct scoring_cfg *score_config,
				      uint8_t ap_nss, uint8_t prorated_pct,
				      uint32_t sta_nss)
{
	uint8_t nss;
	uint8_t score_pct;

	nss = ap_nss;
	if (sta_nss < nss)
		nss = sta_nss;

	if (nss == 8)
		score_pct = CM_MAX_PCT_SCORE;
	if (nss == 4)
		score_pct = CM_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index[0],
				CM_NSS_4x4_INDEX);
	else if (nss == 3)
		score_pct = CM_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index[0],
				CM_NSS_3x3_INDEX);
	else if (nss == 2)
		score_pct = CM_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index[0],
				CM_NSS_2x2_INDEX);
	else
		score_pct = CM_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index[0],
				CM_NSS_1x1_INDEX);

	return (score_config->weight_config.nss_weightage * score_pct *
		prorated_pct) / CM_MAX_PCT_SCORE;
}

static int32_t cm_calculate_security_score(struct scoring_cfg *score_config,
					   struct security_info neg_sec_info)
{
	uint32_t authmode, key_mgmt, ucastcipherset;
	uint8_t score_pct = 0;

	authmode = neg_sec_info.authmodeset;
	key_mgmt = neg_sec_info.key_mgmt;
	ucastcipherset = neg_sec_info.ucastcipherset;

	if (QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_FILS_SK) ||
	    QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_SAE) ||
	    QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_CCKM) ||
	    QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_RSNA) ||
	    QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_8021X)) {
		if (QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_SAE) ||
		    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_SAE) ||
		    QDF_HAS_PARAM(key_mgmt,
				  WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B) ||
		    QDF_HAS_PARAM(key_mgmt,
				  WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192) ||
		    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FILS_SHA256) ||
		    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FILS_SHA384) ||
		    QDF_HAS_PARAM(key_mgmt,
				  WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256) ||
		    QDF_HAS_PARAM(key_mgmt,
				  WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384) ||
		    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_OWE) ||
		    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_DPP) ||
		    QDF_HAS_PARAM(key_mgmt,
				  WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X_SHA384) ||
		    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_SAE_EXT_KEY) ||
		    QDF_HAS_PARAM(key_mgmt,
				  WLAN_CRYPTO_KEY_MGMT_FT_SAE_EXT_KEY)) {
			/*If security is WPA3, consider score_pct = 100%*/
			score_pct = CM_GET_SCORE_PERCENTAGE(
					score_config->security_weight_per_index,
					CM_SECURITY_WPA3_INDEX);
		} else if (QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_PSK) ||
			   QDF_HAS_PARAM(key_mgmt,
					 WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X) ||
			   QDF_HAS_PARAM(key_mgmt,
					 WLAN_CRYPTO_KEY_MGMT_FT_PSK) ||
			   QDF_HAS_PARAM(key_mgmt,
				WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256) ||
			   QDF_HAS_PARAM(key_mgmt,
					 WLAN_CRYPTO_KEY_MGMT_PSK_SHA256)) {
			/*If security is WPA2, consider score_pct = 50%*/
			score_pct = CM_GET_SCORE_PERCENTAGE(
				score_config->security_weight_per_index,
				CM_SECURITY_WPA2_INDEX);
		}
	} else if (QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_SHARED) ||
		   QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_WPA) ||
		   QDF_HAS_PARAM(authmode, WLAN_CRYPTO_AUTH_WAPI)) {
		/*If security is WPA, consider score_pct = 25%*/
		score_pct = CM_GET_SCORE_PERCENTAGE(
				score_config->security_weight_per_index,
				CM_SECURITY_WPA_INDEX);
	}

	return (score_config->weight_config.security_weightage * score_pct) /
			CM_MAX_PCT_SCORE;
}

#ifdef WLAN_POLICY_MGR_ENABLE
static uint32_t cm_get_sta_nss(struct wlan_objmgr_psoc *psoc,
			       qdf_freq_t bss_channel_freq,
			       uint8_t vdev_nss_2g, uint8_t vdev_nss_5g)
{
	/*
	 * If station support nss as 2*2 but AP support NSS as 1*1,
	 * this AP will be given half weight compare to AP which are having
	 * NSS as 2*2.
	 */

	if (policy_mgr_is_chnl_in_diff_band(
	    psoc, bss_channel_freq) &&
	    policy_mgr_is_hw_dbs_capable(psoc) &&
	    !(policy_mgr_is_hw_dbs_2x2_capable(psoc)))
		return 1;

	return (WLAN_REG_IS_24GHZ_CH_FREQ(bss_channel_freq) ?
		vdev_nss_2g :
		vdev_nss_5g);
}
#else
static uint32_t cm_get_sta_nss(struct wlan_objmgr_psoc *psoc,
			       qdf_freq_t bss_channel_freq,
			       uint8_t vdev_nss_2g, uint8_t vdev_nss_5g)
{
	return (WLAN_REG_IS_24GHZ_CH_FREQ(bss_channel_freq) ?
		vdev_nss_2g :
		vdev_nss_5g);
}
#endif

#ifdef CONN_MGR_ADV_FEATURE
static bool
cm_get_pcl_weight_of_channel(uint32_t chan_freq,
			     struct pcl_freq_weight_list *pcl_lst,
			     int *pcl_chan_weight)
{
	int i;
	bool found = false;

	if (!pcl_lst)
		return found;

	for (i = 0; i < pcl_lst->num_of_pcl_channels; i++) {
		if (pcl_lst->pcl_freq_list[i] == chan_freq) {
			*pcl_chan_weight = pcl_lst->pcl_weight_list[i];
			found = true;
			break;
		}
	}

	return found;
}

/**
 * cm_calculate_pcl_score() - Calculate PCL score based on PCL weightage
 * @psoc: psoc ptr
 * @pcl_chan_weight: pcl weight of BSS channel
 * @pcl_weightage: PCL _weightage out of total weightage
 *
 * Return: pcl score
 */
static int32_t cm_calculate_pcl_score(struct wlan_objmgr_psoc *psoc,
				      int pcl_chan_weight,
				      uint8_t pcl_weightage)
{
	int32_t pcl_score = 0;
	int32_t temp_pcl_chan_weight = 0;

	/*
	 * Don’t consider pcl weightage for STA connection,
	 * if primary interface is configured.
	 */
	if (!policy_mgr_is_pcl_weightage_required(psoc))
		return 0;

	if (pcl_chan_weight) {
		temp_pcl_chan_weight =
			(CM_MAX_WEIGHT_OF_PCL_CHANNELS - pcl_chan_weight);
		temp_pcl_chan_weight = qdf_do_div(
					temp_pcl_chan_weight,
					CM_PCL_GROUPS_WEIGHT_DIFFERENCE);
		pcl_score = pcl_weightage - temp_pcl_chan_weight;
		if (pcl_score < 0)
			pcl_score = 0;
	}

	return pcl_score * CM_MAX_PCT_SCORE;
}

/**
 * cm_calculate_oce_wan_score() - Calculate oce wan score
 * @entry: bss information
 * @score_params: bss score params
 *
 * Return: oce wan score
 */
static int32_t cm_calculate_oce_wan_score(
	struct scan_cache_entry *entry,
	struct scoring_cfg *score_params)
{
	uint32_t window_size;
	uint8_t index;
	struct oce_reduced_wan_metrics wan_metrics;
	uint8_t *mbo_oce_ie;

	if (!score_params->oce_wan_scoring.num_slot)
		return 0;

	if (score_params->oce_wan_scoring.num_slot >
	    CM_SCORE_MAX_INDEX)
		score_params->oce_wan_scoring.num_slot =
			CM_SCORE_MAX_INDEX;

	window_size = CM_SCORE_MAX_INDEX /
			score_params->oce_wan_scoring.num_slot;
	mbo_oce_ie = util_scan_entry_mbo_oce(entry);
	if (wlan_parse_oce_reduced_wan_metrics_ie(mbo_oce_ie, &wan_metrics)) {
		mlme_err("downlink_av_cap %d", wan_metrics.downlink_av_cap);
		/* if capacity is 0 return 0 score */
		if (!wan_metrics.downlink_av_cap)
			return 0;
		/* Desired values are from 1 to WLAN_SCORE_MAX_INDEX */
		index = qdf_do_div(wan_metrics.downlink_av_cap,
				   window_size);
	} else {
		index = CM_SCORE_INDEX_0;
	}

	if (index > score_params->oce_wan_scoring.num_slot)
		index = score_params->oce_wan_scoring.num_slot;

	return cm_get_score_for_index(index,
			score_params->weight_config.oce_wan_weightage,
			&score_params->oce_wan_scoring);
}

/**
 * cm_calculate_oce_subnet_id_weightage() - Calculate oce subnet id weightage
 * @entry: bss entry
 * @score_params: bss score params
 * @oce_subnet_id_present: check if subnet id subelement is present in OCE IE
 *
 * Return: oce subnet id score
 */
static uint32_t
cm_calculate_oce_subnet_id_weightage(struct scan_cache_entry *entry,
				     struct scoring_cfg *score_params,
				     bool *oce_subnet_id_present)
{
	uint32_t score = 0;
	uint8_t *mbo_oce_ie;

	mbo_oce_ie = util_scan_entry_mbo_oce(entry);
	*oce_subnet_id_present = wlan_parse_oce_subnet_id_ie(mbo_oce_ie);

	/* Consider 50% weightage if subnet id sub element is present */
	if (*oce_subnet_id_present)
		score  = score_params->weight_config.oce_subnet_id_weightage *
				(CM_MAX_PCT_SCORE / 2);

	return score;
}

/**
 * cm_calculate_sae_pk_ap_weightage() - Calculate SAE-PK AP weightage
 * @entry: bss entry
 * @score_params: bss score params
 * @sae_pk_cap_present: sae_pk cap presetn in RSNXE capability field
 *
 * Return: SAE-PK AP weightage score
 */
static uint32_t
cm_calculate_sae_pk_ap_weightage(struct scan_cache_entry *entry,
				 struct scoring_cfg *score_params,
				 bool *sae_pk_cap_present)
{
	const uint8_t *rsnxe_ie;
	const uint8_t *rsnxe_cap;
	uint8_t cap_len;

	rsnxe_ie = util_scan_entry_rsnxe(entry);

	rsnxe_cap = wlan_crypto_parse_rsnxe_ie(rsnxe_ie, &cap_len);

	if (!rsnxe_cap)
		return 0;

	*sae_pk_cap_present = *rsnxe_cap & WLAN_CRYPTO_RSNX_CAP_SAE_PK;
	if (*sae_pk_cap_present)
		return score_params->weight_config.sae_pk_ap_weightage *
			CM_MAX_PCT_SCORE;

	return 0;
}

/**
 * cm_calculate_oce_ap_tx_pwr_weightage() - Calculate oce ap tx pwr weightage
 * @entry: bss entry
 * @score_params: bss score params
 * @ap_tx_pwr_dbm: pointer to hold ap tx power
 *
 * Return: oce ap tx power score
 */
static uint32_t
cm_calculate_oce_ap_tx_pwr_weightage(struct scan_cache_entry *entry,
				     struct scoring_cfg *score_params,
				     int8_t *ap_tx_pwr_dbm)
{
	uint8_t *mbo_oce_ie, ap_tx_pwr_factor;
	struct rssi_config_score *rssi_score_param;
	int32_t best_rssi_threshold, good_rssi_threshold, bad_rssi_threshold;
	uint32_t good_rssi_pcnt, bad_rssi_pcnt, good_bucket_size;
	uint32_t score, normalized_ap_tx_pwr, bad_bucket_size;
	bool ap_tx_pwr_cap_present = true;

	mbo_oce_ie = util_scan_entry_mbo_oce(entry);
	if (!wlan_parse_oce_ap_tx_pwr_ie(mbo_oce_ie, ap_tx_pwr_dbm)) {
		ap_tx_pwr_cap_present = false;
		/* If no OCE AP TX pwr, consider Uplink RSSI = Downlink RSSI */
		normalized_ap_tx_pwr = entry->rssi_raw;
	} else {
		/*
		 * Normalized ap_tx_pwr =
		 * Uplink RSSI = (STA TX Power - * (AP TX power - RSSI)) in dBm.
		 * Currently assuming STA Tx Power to be 20dBm, though later it
		 * need to fetched from hal-phy API.
		 */
		normalized_ap_tx_pwr =
			(20 - (*ap_tx_pwr_dbm - entry->rssi_raw));
	}

	rssi_score_param = &score_params->rssi_score;

	best_rssi_threshold = rssi_score_param->best_rssi_threshold * (-1);
	good_rssi_threshold = rssi_score_param->good_rssi_threshold * (-1);
	bad_rssi_threshold = rssi_score_param->bad_rssi_threshold * (-1);
	good_rssi_pcnt = rssi_score_param->good_rssi_pcnt;
	bad_rssi_pcnt = rssi_score_param->bad_rssi_pcnt;
	good_bucket_size = rssi_score_param->good_rssi_bucket_size;
	bad_bucket_size = rssi_score_param->bad_rssi_bucket_size;

	/* Uplink RSSI is better than best rssi threshold */
	if (normalized_ap_tx_pwr > best_rssi_threshold) {
		ap_tx_pwr_factor = CM_MAX_PCT_SCORE;
	} else if (normalized_ap_tx_pwr <= bad_rssi_threshold) {
		/* Uplink RSSI is less or equal to bad rssi threshold */
		ap_tx_pwr_factor = rssi_score_param->bad_rssi_pcnt;
	} else if (normalized_ap_tx_pwr > good_rssi_threshold) {
		/* Uplink RSSI lies between best to good rssi threshold */
		ap_tx_pwr_factor =
			cm_get_rssi_pcnt_for_slot(
					best_rssi_threshold,
					good_rssi_threshold, 100,
					good_rssi_pcnt,
					good_bucket_size, normalized_ap_tx_pwr);
	} else {
		/* Uplink RSSI lies between good to best rssi threshold */
		ap_tx_pwr_factor =
			cm_get_rssi_pcnt_for_slot(
					good_rssi_threshold,
					bad_rssi_threshold, good_rssi_pcnt,
					bad_rssi_pcnt, bad_bucket_size,
					normalized_ap_tx_pwr);
	}

	score  = score_params->weight_config.oce_ap_tx_pwr_weightage *
			ap_tx_pwr_factor;

	return score;
}

static bool cm_is_assoc_allowed(struct psoc_mlme_obj *mlme_psoc_obj,
				struct scan_cache_entry *entry)
{
	uint8_t reason;
	uint8_t *mbo_oce;
	bool check_assoc_disallowed;

	mbo_oce = util_scan_entry_mbo_oce(entry);

	check_assoc_disallowed =
	   mlme_psoc_obj->psoc_cfg.score_config.check_assoc_disallowed;

	if (check_assoc_disallowed &&
	    wlan_parse_oce_assoc_disallowed_ie(mbo_oce, &reason)) {
		mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): rssi %d, assoc disallowed set in MBO/OCE IE reason %d",
				QDF_MAC_ADDR_REF(entry->bssid.bytes),
				entry->channel.chan_freq,
				entry->rssi_raw, reason);
		return false;
	}

	return true;
}

void wlan_cm_set_check_assoc_disallowed(struct wlan_objmgr_psoc *psoc,
					bool value)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	mlme_psoc_obj->psoc_cfg.score_config.check_assoc_disallowed = value;
}

void wlan_cm_get_check_assoc_disallowed(struct wlan_objmgr_psoc *psoc,
					bool *value)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj) {
		*value = false;
		return;
	}

	*value = mlme_psoc_obj->psoc_cfg.score_config.check_assoc_disallowed;
}

static enum phy_ch_width
cm_calculate_bandwidth(struct scan_cache_entry *entry,
		       struct psoc_phy_config *phy_config)
{
	uint8_t bw_above_20 = 0;
	bool is_vht = false;
	enum phy_ch_width ch_width;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq)) {
		bw_above_20 = phy_config->bw_above_20_24ghz;
		if (phy_config->vht_24G_cap)
			is_vht = true;
	} else if (phy_config->vht_cap) {
		is_vht = true;
		bw_above_20 = phy_config->bw_above_20_5ghz;
	}

	if (IS_WLAN_PHYMODE_160MHZ(entry->phy_mode))
		ch_width = CH_WIDTH_160MHZ;
	else if (IS_WLAN_PHYMODE_80MHZ(entry->phy_mode))
		ch_width = CH_WIDTH_80MHZ;
	else if (IS_WLAN_PHYMODE_40MHZ(entry->phy_mode))
		ch_width = CH_WIDTH_40MHZ;
	else
		ch_width = CH_WIDTH_20MHZ;

	if (!phy_config->ht_cap &&
	    ch_width >= CH_WIDTH_20MHZ)
		ch_width = CH_WIDTH_20MHZ;

	if (!is_vht && ch_width > CH_WIDTH_40MHZ)
		ch_width = CH_WIDTH_40MHZ;

	if (!bw_above_20)
		ch_width = CH_WIDTH_20MHZ;

	return ch_width;
}

static uint8_t cm_etp_get_ba_win_size_from_esp(uint8_t esp_ba_win_size)
{
	/*
	 * BA Window Size subfield is three bits in length and indicates the
	 * size of the Block Ack window that is.
	 * 802.11-2016.pdf Table 9-262 BA Window Size subfield encoding
	 */
	switch (esp_ba_win_size) {
	case 1: return 2;
	case 2: return 4;
	case 3: return 6;
	case 4: return 8;
	case 5: return 16;
	case 6: return 32;
	case 7: return 64;
	default: return 1;
	}
}

static uint16_t cm_get_etp_ntone(bool is_ht, bool is_vht,
				 enum phy_ch_width ch_width)
{
	uint16_t n_sd = 52, n_seg = 1;

	if (is_vht) {
		/* Refer Table 21-5 in IEEE80211-2016 Spec */
		if (ch_width == CH_WIDTH_20MHZ)
			n_sd = 52;
		else if (ch_width == CH_WIDTH_40MHZ)
			n_sd = 108;
		else if (ch_width == CH_WIDTH_80MHZ)
			n_sd = 234;
		else if (ch_width == CH_WIDTH_80P80MHZ)
			n_sd = 234, n_seg = 2;
		else if (ch_width == CH_WIDTH_160MHZ)
			n_sd = 468;
	} else if (is_ht) {
		/* Refer Table 19-6 in IEEE80211-2016 Spec */
		if (ch_width == CH_WIDTH_20MHZ)
			n_sd = 52;
		if (ch_width == CH_WIDTH_40MHZ)
			n_sd = 108;
	} else {
		n_sd = 48;
	}

	return (n_sd * n_seg);
}

/* Refer Table 27-64 etc in Draft P802.11ax_D7.0.txt */
static uint16_t cm_get_etp_he_ntone(enum phy_ch_width ch_width)
{
	uint16_t n_sd = 234, n_seg = 1;

	if (ch_width == CH_WIDTH_20MHZ)
		n_sd = 234;
	else if (ch_width == CH_WIDTH_40MHZ)
		n_sd = 468;
	else if (ch_width == CH_WIDTH_80MHZ)
		n_sd = 980;
	else if (ch_width == CH_WIDTH_80P80MHZ)
		n_sd = 980, n_seg = 2;
	else if (ch_width == CH_WIDTH_160MHZ)
		n_sd = 1960;

	return (n_sd * n_seg);
}

static uint16_t cm_get_etp_phy_header_dur_us(bool is_ht, bool is_vht,
					     uint8_t nss)
{
	uint16_t dur_us = 0;

	if (is_vht) {
		/*
		 * Refer Figure 21-4 in 80211-2016 Spec
		 * 8 (L-STF) + 8 (L-LTF) + 4 (L-SIG) +
		 * 8 (VHT-SIG-A) + 4 (VHT-STF) + 4 (VHT-SIG-B)
		 */
		dur_us = 36;
		/* (nss * VHT-LTF) = (nss * 4) */
		dur_us += (nss << 2);
	} else if (is_ht) {
		/*
		 * Refer Figure 19-1 in 80211-2016 Spec
		 * 8 (L-STF) + 8 (L-LTF) + 4 (L-SIG) + 8 (HT-SIG) +
		 * 4 (HT-STF)
		 */
		dur_us = 32;
		/* (nss * HT-LTF = nss * 4) */
		dur_us += (nss << 2);
	} else {
		/*
		 * non-HT
		 * Refer Figure 19-1 in 80211-2016 Spec
		 * 8 (L-STF) + 8 (L-LTF) + 4 (L-SIG)
		 */
		dur_us = 20;
	}
	return dur_us;
}

static uint32_t
cm_get_etp_max_bits_per_sc_1000x_for_nss(struct wlan_objmgr_psoc *psoc,
					 struct scan_cache_entry *entry,
					 uint8_t nss,
					 struct psoc_phy_config *phy_config)
{
	uint32_t max_bits_per_sc_1000x = 5000; /* 5 * 1000 */
	uint8_t mcs_map;
	struct wlan_ie_vhtcaps *bss_vht_cap;
	struct wlan_ie_hecaps *bss_he_cap;
	uint32_t self_rx_mcs_map;
	QDF_STATUS status;

	bss_vht_cap = (struct wlan_ie_vhtcaps *)util_scan_entry_vhtcap(entry);
	bss_he_cap = (struct wlan_ie_hecaps *)util_scan_entry_hecap(entry);
	if (!phy_config->vht_cap || !bss_vht_cap) {
		mlme_err("vht unsupported");
		return max_bits_per_sc_1000x;
	}

	status = wlan_mlme_cfg_get_vht_rx_mcs_map(psoc, &self_rx_mcs_map);
	if (QDF_IS_STATUS_ERROR(status))
		return max_bits_per_sc_1000x;

	if (nss == 4) {
		mcs_map = (self_rx_mcs_map & 0xC0) >> 6;
		mcs_map = QDF_MIN(mcs_map,
				  (bss_vht_cap->rx_mcs_map & 0xC0) >> 6);
	} else if (nss == 3) {
		mcs_map = (self_rx_mcs_map & 0x30) >> 4;
		mcs_map = QDF_MIN(mcs_map,
				  (bss_vht_cap->rx_mcs_map & 0x30) >> 4);
	} else if (nss == 2) {
		mcs_map = (self_rx_mcs_map & 0x0C) >> 2;
		mcs_map = QDF_MIN(mcs_map,
				  (bss_vht_cap->rx_mcs_map & 0x0C) >> 2);
	} else {
		mcs_map = (self_rx_mcs_map & 0x03);
		mcs_map = QDF_MIN(mcs_map, (bss_vht_cap->rx_mcs_map & 0x03));
	}
	if (bss_he_cap) {
		if (mcs_map == 2)
			max_bits_per_sc_1000x = 8333; /* 10 *5/6 * 1000 */
		else if (mcs_map == 1)
			max_bits_per_sc_1000x = 7500; /* 10 * 3/4 * 1000 */
	} else {
		if (mcs_map == 2)
			max_bits_per_sc_1000x = 6667; /* 8 * 5/6 * 1000 */
		else if (mcs_map == 1)
			max_bits_per_sc_1000x = 6000; /* 8 * 3/4 * 1000 */
	}
	return max_bits_per_sc_1000x;
}

/* Refer Table 9-163 in 80211-2016 Spec */
static uint32_t cm_etp_get_min_mpdu_ss_us_100x(struct htcap_cmn_ie *htcap)
{
	tSirMacHTParametersInfo *ampdu_param;
	uint8_t ampdu_density;

	ampdu_param = (tSirMacHTParametersInfo *)&htcap->ampdu_param;
	ampdu_density = ampdu_param->mpduDensity;

	if (ampdu_density == 1)
		return 25; /* (1/4) * 100 */
	else if (ampdu_density == 2)
		return 50; /* (1/2) * 100 */
	else if (ampdu_density == 3)
		return 100; /* 1 * 100 */
	else if (ampdu_density == 4)
		return 200; /* 2 * 100 */
	else if (ampdu_density == 5)
		return 400; /* 4 * 100 */
	else if (ampdu_density == 6)
		return 800; /* 8 * 100 */
	else if (ampdu_density == 7)
		return 1600; /* 16 * 100 */
	else
		return 100;
}

/* Refer Table 9-162 in 80211-2016 Spec */
static uint32_t cm_etp_get_max_amsdu_len(struct wlan_objmgr_psoc *psoc,
					 struct htcap_cmn_ie *htcap)
{
	uint8_t bss_max_amsdu;
	uint32_t bss_max_amsdu_len;
	QDF_STATUS status;

	status = wlan_mlme_get_max_amsdu_num(psoc, &bss_max_amsdu);
	if (QDF_IS_STATUS_ERROR(status))
		bss_max_amsdu_len = 3839;
	else if (bss_max_amsdu == 1)
		bss_max_amsdu_len =  7935;
	else
		bss_max_amsdu_len = 3839;

	return bss_max_amsdu_len;
}

   // Calculate the number of bits per tone based on the input of SNR in dB
    // The output is scaled up by BIT_PER_TONE_SCALE for integer representation
static uint32_t
calculate_bit_per_tone(int32_t rssi, enum phy_ch_width ch_width)
{
	int32_t noise_floor_db_boost;
	int32_t noise_floor_dbm;
	int32_t snr_db;
	int32_t bit_per_tone;
	int32_t lut_in_idx;

	noise_floor_db_boost = TWO_IN_DB * ch_width;
	noise_floor_dbm = WLAN_NOISE_FLOOR_DBM_DEFAULT + noise_floor_db_boost +
			SNR_MARGIN_DB;
	snr_db = rssi - noise_floor_dbm;
	if (snr_db <= SNR_DB_TO_BIT_PER_TONE_LUT_MAX) {
		lut_in_idx = QDF_MAX(snr_db, SNR_DB_TO_BIT_PER_TONE_LUT_MIN)
			- SNR_DB_TO_BIT_PER_TONE_LUT_MIN;
		lut_in_idx = QDF_MIN(lut_in_idx, DB_NUM - 1);
		bit_per_tone = SNR_DB_TO_BIT_PER_TONE_LUT[lut_in_idx];
	} else {
		/*
		 * SNR_tone = 10^(SNR/10)
		 * log2(1+SNR_tone) ~= log2(SNR_tone) =
		 * log10(SNR_tone)/log10(2) = log10(10^(SNR/10)) / 0.3
		 * = (SNR/10) / 0.3 = SNR/3
		 * So log2(1+SNR_tone) = SNR/3. 1000x for this is SNR*334
		 */
		bit_per_tone = snr_db * 334;
	}

	return bit_per_tone;
}

static uint32_t
cm_calculate_etp(struct wlan_objmgr_psoc *psoc,
		 struct scan_cache_entry *entry,
		 struct etp_params  *etp_param,
		 uint8_t max_nss, enum phy_ch_width ch_width,
		 bool is_ht, bool is_vht, bool is_he,
		 int8_t rssi,
		 struct psoc_phy_config *phy_config)
{
	uint16_t ntone;
	uint16_t phy_hdr_dur_us, max_amsdu_len = 1500, min_mpdu_ss_us_100x = 0;
	uint32_t max_bits_per_sc_1000x, log_2_snr_tone_1000x;
	uint32_t ppdu_payload_dur_us = 0, mpdu_per_ampdu, mpdu_per_ppdu;
	uint32_t single_ppdu_dur_us, estimated_throughput_mbps, data_rate_kbps;
	struct htcap_cmn_ie *htcap;

	htcap = (struct htcap_cmn_ie *)util_scan_entry_htcap(entry);
	if (ch_width > CH_WIDTH_160MHZ)
		return CM_AVOID_CANDIDATE_MIN_SCORE;

	if (is_he)
		ntone = cm_get_etp_he_ntone(ch_width);
	else
		ntone = cm_get_etp_ntone(is_ht, is_vht, ch_width);
	phy_hdr_dur_us = cm_get_etp_phy_header_dur_us(is_ht, is_vht, max_nss);

	max_bits_per_sc_1000x =
		cm_get_etp_max_bits_per_sc_1000x_for_nss(psoc, entry,
							 max_nss, phy_config);
	if (rssi < WLAN_NOISE_FLOOR_DBM_DEFAULT)
		return CM_AVOID_CANDIDATE_MIN_SCORE;

	log_2_snr_tone_1000x = calculate_bit_per_tone(rssi, ch_width);

	/* Eq. R-2 Pg:3508 in 80211-2016 Spec */
	if (is_he)
		data_rate_kbps =
			QDF_MIN(log_2_snr_tone_1000x, max_bits_per_sc_1000x) *
			(max_nss * ntone) / HE_PPDU_PAYLOAD_SYMBOL_DUR_US;
	else
		data_rate_kbps =
			QDF_MIN(log_2_snr_tone_1000x, max_bits_per_sc_1000x) *
			(max_nss * ntone) / PPDU_PAYLOAD_SYMBOL_DUR_US;
	mlme_debug("data_rate_kbps: %d", data_rate_kbps);
	if (data_rate_kbps < 1000) {
		/* Return ETP as 1 since datarate is not even 1 Mbps */
		return CM_AVOID_CANDIDATE_MIN_SCORE;
	}
	/* compute MPDU_p_PPDU */
	if (is_ht) {
		min_mpdu_ss_us_100x =
			cm_etp_get_min_mpdu_ss_us_100x(htcap);
		max_amsdu_len =
			cm_etp_get_max_amsdu_len(psoc, htcap);
		ppdu_payload_dur_us =
			etp_param->data_ppdu_dur_target_us - phy_hdr_dur_us;
		mpdu_per_ampdu =
			QDF_MIN(qdf_ceil(ppdu_payload_dur_us * 100,
					 min_mpdu_ss_us_100x),
				qdf_ceil(ppdu_payload_dur_us *
					 (data_rate_kbps / 1000),
					 (MAC_HEADER_LEN + max_amsdu_len) * 8));
		mpdu_per_ppdu = QDF_MIN(etp_param->ba_window_size,
					QDF_MAX(1, mpdu_per_ampdu));
	} else {
		mpdu_per_ppdu = 1;
	}

	/* compute PPDU_Dur */
	single_ppdu_dur_us =
		qdf_ceil((MAC_HEADER_LEN + max_amsdu_len) * mpdu_per_ppdu * 8,
			 (data_rate_kbps / 1000) * PPDU_PAYLOAD_SYMBOL_DUR_US);
	single_ppdu_dur_us *= PPDU_PAYLOAD_SYMBOL_DUR_US;
	single_ppdu_dur_us += phy_hdr_dur_us;

	estimated_throughput_mbps =
		qdf_ceil(mpdu_per_ppdu * max_amsdu_len * 8, single_ppdu_dur_us);
	estimated_throughput_mbps =
		(estimated_throughput_mbps *
		 etp_param->airtime_fraction) /
		 CM_MAX_ESTIMATED_AIR_TIME_FRACTION;

	if (estimated_throughput_mbps < CM_AVOID_CANDIDATE_MIN_SCORE)
		estimated_throughput_mbps = CM_AVOID_CANDIDATE_MIN_SCORE;
	if (estimated_throughput_mbps > CM_BEST_CANDIDATE_MAX_BSS_SCORE)
		estimated_throughput_mbps = CM_BEST_CANDIDATE_MAX_BSS_SCORE;

	mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): rssi %d HT %d VHT %d HE %d ATF: %d NSS %d, ch_width: %d",
			QDF_MAC_ADDR_REF(entry->bssid.bytes),
			entry->channel.chan_freq,
			entry->rssi_raw, is_ht, is_vht, is_he,
			etp_param->airtime_fraction,
			entry->nss, ch_width);
	if (is_ht)
		mlme_nofl_debug("min_mpdu_ss_us_100x = %d, max_amsdu_len = %d, ppdu_payload_dur_us = %d, mpdu_per_ampdu = %d, mpdu_per_ppdu = %d, ba_window_size = %d",
				min_mpdu_ss_us_100x, max_amsdu_len,
				ppdu_payload_dur_us, mpdu_per_ampdu,
				mpdu_per_ppdu, etp_param->ba_window_size);
	mlme_nofl_debug("ETP score params: ntone: %d, phy_hdr_dur_us: %d, max_bits_per_sc_1000x: %d, log_2_snr_tone_1000x: %d mpdu_p_ppdu = %d, max_amsdu_len = %d, ppdu_dur_us = %d, total score = %d",
			ntone, phy_hdr_dur_us, max_bits_per_sc_1000x,
			log_2_snr_tone_1000x, mpdu_per_ppdu, max_amsdu_len,
			single_ppdu_dur_us, estimated_throughput_mbps);

	return estimated_throughput_mbps;
}

static uint32_t
cm_calculate_etp_score(struct wlan_objmgr_psoc *psoc,
		       struct scan_cache_entry *entry,
		       struct psoc_phy_config *phy_config)
{
	enum phy_ch_width ch_width;
	uint32_t nss;
	bool is_he_intersect = false;
	bool is_vht_intersect = false;
	bool is_ht_intersect = false;
	struct wlan_esp_info *esp;
	struct wlan_esp_ie *esp_ie;
	struct etp_params etp_param;

	if (phy_config->he_cap && entry->ie_list.hecap)
		is_he_intersect = true;
	if ((phy_config->vht_cap || phy_config->vht_24G_cap) &&
	    (entry->ie_list.vhtcap ||
	     WLAN_REG_IS_6GHZ_CHAN_FREQ(entry->channel.chan_freq)))
		is_vht_intersect = true;
	if (phy_config->ht_cap && entry->ie_list.htcap)
		is_ht_intersect = true;
	nss = cm_get_sta_nss(psoc, entry->channel.chan_freq,
			     phy_config->vdev_nss_24g,
			     phy_config->vdev_nss_5g);
	nss = QDF_MIN(nss, entry->nss);
	ch_width = cm_calculate_bandwidth(entry, phy_config);

	/* Initialize default ETP params */
	etp_param.airtime_fraction = 255 / 2;
	etp_param.ba_window_size = 32;
	etp_param.data_ppdu_dur_target_us = 5000; /* 5 msec */

	if (entry->air_time_fraction) {
		etp_param.airtime_fraction = entry->air_time_fraction;
		esp_ie = (struct wlan_esp_ie *)
			util_scan_entry_esp_info(entry);
		if (esp_ie) {
			esp = &esp_ie->esp_info_AC_BE;
			etp_param.ba_window_size =
				cm_etp_get_ba_win_size_from_esp(esp->ba_window_size);
			etp_param.data_ppdu_dur_target_us =
					50 * esp->ppdu_duration;
			mlme_debug("esp ba_window_size: %d, ppdu_duration: %d",
				   esp->ba_window_size, esp->ppdu_duration);
		}
	} else if (entry->qbss_chan_load) {
		mlme_debug("qbss_chan_load: %d", entry->qbss_chan_load);
		etp_param.airtime_fraction =
			CM_MAX_ESTIMATED_AIR_TIME_FRACTION -
			entry->qbss_chan_load;
	}
	/* If ini vendor_roam_score_algorithm=1, just calculate ETP of all
	 * bssid of ssid selected by high layer, and try to connect AP by
	 * order of ETP, legacy algorithm with following Parameters/Weightage
	 * becomes useless. ETP should be [1Mbps, 20000Mbps],matches score
	 * range: [1, 20000]
	 */
	return cm_calculate_etp(psoc, entry,
				  &etp_param,
				  nss,
				  ch_width,
				  is_ht_intersect,
				  is_vht_intersect,
				  is_he_intersect,
				  entry->rssi_raw,
				  phy_config);
}
#else
static bool
cm_get_pcl_weight_of_channel(uint32_t chan_freq,
			     struct pcl_freq_weight_list *pcl_lst,
			     int *pcl_chan_weight)
{
	return false;
}

static int32_t cm_calculate_pcl_score(struct wlan_objmgr_psoc *psoc,
				      int pcl_chan_weight,
				      uint8_t pcl_weightage)
{
	return 0;
}

static int32_t cm_calculate_oce_wan_score(struct scan_cache_entry *entry,
					  struct scoring_cfg *score_params)
{
	return 0;
}

static uint32_t
cm_calculate_oce_subnet_id_weightage(struct scan_cache_entry *entry,
				     struct scoring_cfg *score_params,
				     bool *oce_subnet_id_present)
{
	return 0;
}

static uint32_t
cm_calculate_sae_pk_ap_weightage(struct scan_cache_entry *entry,
				 struct scoring_cfg *score_params,
				 bool *sae_pk_cap_present)
{
	return 0;
}

static uint32_t
cm_calculate_oce_ap_tx_pwr_weightage(struct scan_cache_entry *entry,
				     struct scoring_cfg *score_params,
				     int8_t *ap_tx_pwr_dbm)
{
	return 0;
}

static inline bool cm_is_assoc_allowed(struct psoc_mlme_obj *mlme_psoc_obj,
				       struct scan_cache_entry *entry)
{
	return true;
}

static uint32_t
cm_calculate_etp_score(struct wlan_objmgr_psoc *psoc,
		       struct scan_cache_entry *entry,
		       struct psoc_phy_config *phy_config)
{
	return 0;
}
#endif

/**
 * cm_get_band_score() - Get band preference weightage
 * @freq: Operating frequency of the AP
 * @score_config: Score configuration
 *
 * Return: Band score for AP.
 */
static int
cm_get_band_score(uint32_t freq, struct scoring_cfg *score_config)
{
	uint8_t band_index;
	struct weight_cfg *weight_config;

	weight_config = &score_config->weight_config;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(freq))
		band_index = CM_BAND_5G_INDEX;
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(freq))
		band_index = CM_BAND_2G_INDEX;
	else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(freq))
		band_index = CM_BAND_6G_INDEX;
	else
		return 0;

	return weight_config->chan_band_weightage *
	       CM_GET_SCORE_PERCENTAGE(score_config->band_weight_per_index,
				       band_index);
}

#ifdef WLAN_FEATURE_11BE
static int cm_calculate_eht_score(struct scan_cache_entry *entry,
				  struct scoring_cfg *score_config,
				  struct psoc_phy_config *phy_config,
				  uint8_t prorated_pcnt)
{
	uint32_t eht_caps_score;
	struct weight_cfg *weight_config;

	if (!phy_config->eht_cap || !entry->ie_list.ehtcap)
		return 0;

	weight_config = &score_config->weight_config;
	eht_caps_score = prorated_pcnt * weight_config->eht_caps_weightage;

	return eht_caps_score;
}

/**
 * cm_get_puncture_bw() - Get puncture band width
 * @entry: Bss scan entry
 *
 * Return: Total bandwidth of punctured subchannels (unit: MHz)
 */
static uint16_t cm_get_puncture_bw(struct scan_cache_entry *entry)
{
	uint16_t puncture_bitmap;
	uint8_t num_puncture_bw = 0;

	puncture_bitmap = entry->channel.puncture_bitmap;
	while (puncture_bitmap) {
		if (puncture_bitmap & 1)
			++num_puncture_bw;
		puncture_bitmap >>= 1;
	}
	return num_puncture_bw * 20;
}

static bool cm_get_su_beam_former(struct scan_cache_entry *entry)
{
	struct wlan_ie_ehtcaps *eht_cap;
	struct wlan_eht_cap_info *eht_cap_info;

	eht_cap = (struct wlan_ie_ehtcaps *)util_scan_entry_ehtcap(entry);
	if (eht_cap) {
		eht_cap_info = (struct wlan_eht_cap_info *)eht_cap->eht_mac_cap;
		if (eht_cap_info->su_beamformer)
			return true;
	}

	return false;
}
#else
static int cm_calculate_eht_score(struct scan_cache_entry *entry,
				  struct scoring_cfg *score_config,
				  struct psoc_phy_config *phy_config,
				uint8_t prorated_pcnt)
{
	return 0;
}

static uint16_t cm_get_puncture_bw(struct scan_cache_entry *entry)
{
	return 0;
}

static bool cm_get_su_beam_former(struct scan_cache_entry *entry)
{
	return false;
}
#endif

#define CM_BAND_WIDTH_NUM 16
#define CM_BAND_WIDTH_UNIT 20
uint16_t link_bw_score[CM_BAND_WIDTH_NUM] = {
9, 18, 27, 35, 44, 53, 56, 67, 74, 80, 86, 90, 93, 96, 98, 100};

static uint32_t cm_get_bw_score(uint8_t bw_weightage, uint16_t bw,
				uint8_t prorated_pcnt)
{
	uint32_t score;
	uint8_t index;

	index = bw / CM_BAND_WIDTH_UNIT - 1;
	if (index >= CM_BAND_WIDTH_NUM)
		index = CM_BAND_WIDTH_NUM - 1;
	score = bw_weightage * link_bw_score[index]
		* prorated_pcnt / CM_MAX_PCT_SCORE;

	return score;
}

/**
 * cm_get_ch_width() - Get channel width of bss scan entry
 * @entry: Bss scan entry
 * @phy_config: Phy config
 *
 * Return: Channel width (unit: MHz)
 */
static uint16_t cm_get_ch_width(struct scan_cache_entry *entry,
				struct psoc_phy_config *phy_config)
{
	uint16_t bw, total_bw = 0;
	uint8_t bw_above_20 = 0;
	bool is_vht = false;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq)) {
		bw_above_20 = phy_config->bw_above_20_24ghz;
		if (phy_config->vht_24G_cap)
			is_vht = true;
	} else if (phy_config->vht_cap) {
		is_vht = true;
		bw_above_20 = phy_config->bw_above_20_5ghz;
	}
	if (IS_WLAN_PHYMODE_320MHZ(entry->phy_mode))
		bw = 320;
	else if (IS_WLAN_PHYMODE_160MHZ(entry->phy_mode))
		bw = 160;
	else if (IS_WLAN_PHYMODE_80MHZ(entry->phy_mode))
		bw = 80;
	else if (IS_WLAN_PHYMODE_40MHZ(entry->phy_mode))
		bw = 40;
	else
		bw = 20;
	if (!phy_config->ht_cap && bw > 20)
		bw = 20;

	if (!is_vht && bw > 40)
		bw = 40;

	total_bw = bw - cm_get_puncture_bw(entry);

	return total_bw;
}

#ifdef WLAN_FEATURE_11BE_MLO
#define CM_MLO_BAD_RSSI_PCT 61
#define CM_MLO_CONGESTION_PCT_BAD_RSSI 6

static uint8_t mlo_boost_pct[MLO_TYPE_MAX] = {0, 10, CM_MAX_PCT_SCORE};

/**
 * struct mlo_rssi_pct: MLO AP rssi joint factor and score percent
 * @joint_factor: rssi joint factor (0 - 100)
 * @rssi_pcnt: Rssi score percent (0 - 100)
 * @prorate_pcnt: RSSI prorated percent
 */
struct mlo_rssi_pct {
	uint16_t joint_factor;
	uint16_t rssi_pcnt;
	uint16_t prorate_pcnt;
};

#define CM_RSSI_BUCKET_NUM 7
static struct mlo_rssi_pct mlo_rssi_pcnt[CM_RSSI_BUCKET_NUM] = {
{80, 100, 100}, {60, 87, 100}, {44, 74, 100}, {30, 61, 100}, {20, 48, 54},
{10, 35, 28}, {0, 22, 1} };

/**
 * cm_get_mlo_rssi_score() - Calculate joint rssi score for MLO AP
 * @rssi_weightage: rssi weightage
 * @link1_rssi: link1 rssi
 * @link2_rssi: link2 rssi
 * @prorate_pcnt: pointer to store RSSI prorated percent
 *
 * Return: MLO AP joint rssi score
 */
static uint32_t cm_get_mlo_rssi_score(uint8_t rssi_weightage, int8_t link1_rssi,
				      int8_t link2_rssi, uint16_t *prorate_pcnt)
{
	int8_t link1_factor = 0, link2_factor = 0;
	int32_t joint_factor = 0;
	int16_t rssi_pcnt = 0;
	int8_t i;

	/* Calculate RSSI score -- using joint rssi, but limit to 2 links */
	link1_factor = QDF_MAX(QDF_MIN(link1_rssi, -50), -95) + 95;
	link2_factor = QDF_MAX(QDF_MIN(link2_rssi, -50), -95) + 95;
	joint_factor = QDF_MIN((link1_factor * link1_factor +
			    link2_factor * link2_factor) * 100 / (2 * 45 * 45),
			    100);
	for (i = 0; i < CM_RSSI_BUCKET_NUM; i++)
		if (joint_factor > mlo_rssi_pcnt[i].joint_factor) {
			rssi_pcnt = mlo_rssi_pcnt[i].rssi_pcnt;
			*prorate_pcnt = mlo_rssi_pcnt[i].prorate_pcnt;
			break;
		}

	return (rssi_weightage * rssi_pcnt);
}

static inline int cm_calculate_emlsr_score(struct weight_cfg *weight_config)
{
	return weight_config->emlsr_weightage * mlo_boost_pct[MLSR];
}

/**
 * cm_get_entry() - Get bss scan entry by link mac address
 * @scan_list: Scan entry list of bss candidates after filtering
 * @link_addr: link mac address
 *
 * Return: Pointer to bss scan entry
 */
static struct scan_cache_entry *cm_get_entry(qdf_list_t *scan_list,
					     struct qdf_mac_addr *link_addr)
{
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct scan_cache_node *curr_entry = NULL;

	qdf_list_peek_front(scan_list, &cur_node);
	while (cur_node) {
		curr_entry = qdf_container_of(cur_node, struct scan_cache_node,
					      node);
		if (!qdf_mem_cmp(&curr_entry->entry->mac_addr,
				 link_addr, QDF_MAC_ADDR_SIZE))
			return curr_entry->entry;

		qdf_list_peek_next(scan_list, cur_node, &next_node);
		cur_node = next_node;
		next_node = NULL;
	}

	return NULL;
}

#ifdef CONN_MGR_ADV_FEATURE
static uint8_t cm_get_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_get_sta_mlo_conn_max_num(psoc);
}

static bool is_freq_dbs_or_sbs(struct wlan_objmgr_psoc *psoc,
			       qdf_freq_t freq_1,
			       qdf_freq_t freq_2)
{
	if ((policy_mgr_is_hw_sbs_capable(psoc) &&
	     policy_mgr_are_sbs_chan(psoc, freq_1, freq_2)) ||
	    (policy_mgr_is_hw_dbs_capable(psoc) &&
	     !wlan_reg_is_same_band_freqs(freq_1, freq_2))) {
		return true;
	}

	return false;
}

#else
static inline
uint8_t cm_get_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc)
{
	return WLAN_UMAC_MLO_MAX_DEV;
}

static inline bool is_freq_dbs_or_sbs(struct wlan_objmgr_psoc *psoc,
				      qdf_freq_t freq_1,
				      qdf_freq_t freq_2)
{
	return false;
}
#endif

/**
 * cm_bss_mlo_type() - Get mlo type of bss scan entry
 * @psoc: Pointer of psoc object
 * @entry: Bss scan entry
 * @scan_list:
 *
 * Return: MLO AP type: SLO, MLMR or EMLSR.
 */
static enum MLO_TYPE  cm_bss_mlo_type(struct wlan_objmgr_psoc *psoc,
				      struct scan_cache_entry *entry,
				      qdf_list_t *scan_list)
{
	uint8_t mlo_link_num;
	uint8_t i;
	uint32_t freq_entry;
	uint32_t freq[MLD_MAX_LINKS - 1];
	struct scan_cache_entry *entry_partner[MLD_MAX_LINKS - 1];
	bool multi_link = false;

	mlo_link_num = cm_get_sta_mlo_conn_max_num(psoc);
	if (!entry->ie_list.multi_link_bv)
		return SLO;
	else if (!entry->ml_info.num_links)
		return SLO;
	else if (mlo_link_num == 1)
		return SLO;

	for (i = 0; i < entry->ml_info.num_links; i++) {
		if (!entry->ml_info.link_info[i].is_valid_link)
			continue;
		multi_link = true;
		freq_entry = entry->channel.chan_freq;
		freq[i] = entry->ml_info.link_info[i].freq;
		entry_partner[i] =
			cm_get_entry(scan_list,
				     &entry->ml_info.link_info[i].link_addr);
		if (entry_partner[i])
			freq[i] = entry_partner[i]->channel.chan_freq;
		if (is_freq_dbs_or_sbs(psoc, freq[i], freq_entry))
			return MLMR;
	}

	if (multi_link)
		return MLSR;
	else
		return SLO;
}

/**
 * cm_get_mlo_congestion_score() - Get mlo jointer congestion percent
 * @bw1: channel width of link1
 * @bw2: channel width of link2
 * @congestion_score1: congestion score of link1
 * @congestion_score2: congestion score of link2
 * @score_params: score param
 *
 * Return: Mlo jointer congestion percent
 */
static uint32_t
cm_get_mlo_congestion_score(uint16_t bw1,
			    uint16_t bw2,
			    uint32_t congestion_score1,
			    uint32_t congestion_score2,
			    struct scoring_cfg *score_params)
{
	uint32_t congestion_best;
	uint32_t congestion_worst;
	uint32_t congestion_weight;

	congestion_weight =
		score_params->weight_config.channel_congestion_weightage;
	if (congestion_score1 > congestion_score2) {
		congestion_best = congestion_score1;
		congestion_worst = congestion_score2 * bw1 / (bw1 + bw2);
	} else if (congestion_score1 < congestion_score2) {
		congestion_best = congestion_score2;
		congestion_worst = congestion_score1 * bw2 / (bw1 + bw2);
	} else {
		congestion_best = congestion_score1;
		congestion_worst = congestion_score2 / 2;
	}
	congestion_best = congestion_best * CM_SLO_CONGESTION_MAX_SCORE /
			 CM_MAX_PCT_SCORE;
	congestion_worst = congestion_worst * CM_SLO_CONGESTION_MAX_SCORE /
			 CM_MAX_PCT_SCORE;
	congestion_worst = QDF_MIN(congestion_worst, 20 * congestion_weight);

	return congestion_best + congestion_worst;
}

/**
 * cm_estimate_rssi() - Get estimated rssi by frequency
 * @rssi_entry: Rssi of bss scan entry
 * @freq_entry: Frequency of bss scan entry
 * @freq_partner: Frequency of partner link of MLO
 *
 * Estimated equation: RSSI(2G) = RSSI(5G) + 7 = RSSI(6G) + 8
 *
 * Return: Estimated rssi of partner link of MLO
 */
static int8_t cm_estimate_rssi(int8_t rssi_entry, uint32_t freq_entry,
			       uint32_t freq_partner)
{
	if (wlan_reg_is_24ghz_ch_freq(freq_entry)) {
		if (wlan_reg_is_5ghz_ch_freq(freq_partner))
			return rssi_entry - 7;
		else if (wlan_reg_is_6ghz_chan_freq(freq_partner))
			return rssi_entry - 8;
	} else if (wlan_reg_is_5ghz_ch_freq(freq_entry)) {
		if (wlan_reg_is_24ghz_ch_freq(freq_partner))
			return rssi_entry + 7;
		else if (wlan_reg_is_6ghz_chan_freq(freq_partner))
			return rssi_entry - 1;
	} else if (wlan_reg_is_6ghz_chan_freq(freq_entry)) {
		if (wlan_reg_is_24ghz_ch_freq(freq_partner))
			return rssi_entry + 8;
		else if (wlan_reg_is_5ghz_ch_freq(freq_partner))
			return rssi_entry + 1;
	}

	return rssi_entry;
}

/**
 * cm_calculate_mlo_bss_score() - Calculate mlo bss score
 * @psoc: Pointer to psoc object
 * @entry: Bss scan entry
 * @score_params: score parameters
 * @phy_config: Phy config
 * @scan_list: Scan entry list of bss candidates after filtering
 * @rssi_prorated_pct: Rssi prorated percent
 *
 * For MLMR case, besides adding MLMR boost score,
 * calculate joint RSSI/band width/congestion score for combination of
 * scan entry + each partner link, select highest total score as candidate
 * combination, only activate that partner link.
 *
 * Return: MLO AP joint total score
 */
static int cm_calculate_mlo_bss_score(struct wlan_objmgr_psoc *psoc,
				      struct scan_cache_entry *entry,
				      struct scoring_cfg *score_params,
				      struct psoc_phy_config *phy_config,
				      qdf_list_t *scan_list,
				      uint8_t *rssi_prorated_pct)
{
	struct scan_cache_entry *entry_partner[MLD_MAX_LINKS - 1];
	int32_t rssi[MLD_MAX_LINKS - 1];
	uint32_t rssi_score[MLD_MAX_LINKS - 1] = {};
	uint16_t prorated_pct[MLD_MAX_LINKS - 1] = {};
	uint32_t freq[MLD_MAX_LINKS - 1];
	uint16_t ch_width[MLD_MAX_LINKS - 1];
	uint32_t bandwidth_score[MLD_MAX_LINKS - 1] = {};
	uint32_t congestion_pct[MLD_MAX_LINKS - 1] = {};
	uint32_t congestion_score[MLD_MAX_LINKS - 1] = {};
	uint32_t cong_total_score[MLD_MAX_LINKS - 1] = {};
	uint32_t total_score[MLD_MAX_LINKS - 1] = {};
	uint8_t i, j;
	uint16_t chan_width;
	uint32_t best_total_score = 0;
	uint8_t best_partner_index = 0;
	uint32_t cong_pct = 0;
	uint32_t cong_score = 0;
	uint32_t freq_entry;
	struct weight_cfg *weight_config;
	struct partner_link_info *link;
	struct wlan_objmgr_pdev *pdev;
	bool rssi_bad_zone;
	bool eht_capab;
	struct partner_link_info tmp_link_info;
	uint32_t tmp_total_score = 0;

	wlan_psoc_mlme_get_11be_capab(psoc, &eht_capab);
	if (!eht_capab)
		return 0;

	weight_config = &score_params->weight_config;
	freq_entry = entry->channel.chan_freq;
	chan_width = cm_get_ch_width(entry, phy_config);
	cong_score = cm_calculate_congestion_score(entry,
						   score_params,
						   &cong_pct, false);
	link = &entry->ml_info.link_info[0];
	for (i = 0; i < entry->ml_info.num_links; i++) {
		if (!link[i].is_valid_link)
			continue;
		entry_partner[i] = cm_get_entry(scan_list, &link[i].link_addr);
		if (entry_partner[i])
			freq[i] = entry_partner[i]->channel.chan_freq;
		else
			freq[i] = link[i].freq;
		if (!is_freq_dbs_or_sbs(psoc, freq[i], freq_entry)) {
			mlme_nofl_debug("freq %d and %d can't be MLMR",
					freq[i], freq_entry);
			continue;
		}
		if (entry_partner[i]) {
			rssi[i] = entry_partner[i]->rssi_raw;
			ch_width[i] = cm_get_ch_width(entry_partner[i],
						      phy_config);
		} else {
			rssi[i] = cm_estimate_rssi(entry->rssi_raw,
						   freq_entry,
						   freq[i]);
			pdev = psoc->soc_objmgr.wlan_pdev_list[0];
			ch_width[i] =
				wlan_reg_get_op_class_width(pdev,
							    link[i].op_class,
							    true);
			mlme_nofl_debug("No entry for partner, estimate with rnr");
		}
		rssi_score[i] =
			cm_get_mlo_rssi_score(weight_config->rssi_weightage,
					      entry->rssi_raw, rssi[i],
					      &prorated_pct[i]);

		bandwidth_score[i] =
			cm_get_bw_score(weight_config->chan_width_weightage,
					chan_width + ch_width[i],
					prorated_pct[i]);

		rssi_bad_zone = prorated_pct[i] < CM_MAX_PCT_SCORE;
		congestion_score[i] =
			cm_calculate_congestion_score(entry_partner[i],
						      score_params,
						      &congestion_pct[i],
						      rssi_bad_zone);
		cong_total_score[i] =
			cm_get_mlo_congestion_score(chan_width,
						    ch_width[i],
						    cong_score,
						    congestion_score[i],
						    score_params);

		total_score[i] = rssi_score[i] + bandwidth_score[i] +
				 cong_total_score[i];
		if (total_score[i] > best_total_score) {
			best_total_score = total_score[i];
			best_partner_index = i;
		}
		mlme_nofl_debug("ML score: link index %u rssi %d %d rssi score %u pror %u freq %u %u bw %u %u, bw score %u congest score %u %u %u, total score %u",
				i, entry->rssi_raw,  rssi[i], rssi_score[i],
				prorated_pct[i], freq_entry, freq[i],
				chan_width, ch_width[i], bandwidth_score[i],
				cong_score, congestion_score[i],
				cong_total_score[i], total_score[i]);
	}

	*rssi_prorated_pct = prorated_pct[best_partner_index];

	/* reorder the link idx per score */
	for (j = 0; j < entry->ml_info.num_links; j++) {
		tmp_total_score = total_score[j];
		best_partner_index = j;
		for (i = j + 1; i < entry->ml_info.num_links; i++) {
			if (tmp_total_score < total_score[i]) {
				tmp_total_score = total_score[i];
				best_partner_index = i;
			}
		}

		if (best_partner_index != j) {
			tmp_link_info = entry->ml_info.link_info[j];
			entry->ml_info.link_info[j] =
				entry->ml_info.link_info[best_partner_index];
			entry->ml_info.link_info[best_partner_index] =
							tmp_link_info;
			total_score[best_partner_index] = total_score[j];
		}
		total_score[j] = 0;
	}

	best_total_score += weight_config->mlo_weightage *
			    mlo_boost_pct[MLMR];
	entry->ml_info.ml_bss_score = best_total_score;

	return best_total_score;
}
#else
static inline int cm_calculate_emlsr_score(struct weight_cfg *weight_config)
{
	return 0;
}

static enum MLO_TYPE cm_bss_mlo_type(struct wlan_objmgr_psoc *psoc,
				     struct scan_cache_entry *entry,
				     qdf_list_t *scan_list)
{
	return SLO;
}

static int cm_calculate_mlo_bss_score(struct wlan_objmgr_psoc *psoc,
				      struct scan_cache_entry *entry,
				      struct scoring_cfg *score_params,
				      struct psoc_phy_config *phy_config,
				      qdf_list_t *scan_list,
				      uint8_t *rssi_prorated_pct)
{
	return 0;
}
#endif

static int cm_calculate_bss_score(struct wlan_objmgr_psoc *psoc,
				  struct scan_cache_entry *entry,
				  int pcl_chan_weight,
				  struct qdf_mac_addr *bssid_hint,
				  qdf_list_t *scan_list)
{
	int32_t score = 0;
	int32_t rssi_score = 0;
	int32_t pcl_score = 0;
	int32_t ht_score = 0;
	int32_t vht_score = 0;
	int32_t he_score = 0;
	int32_t bandwidth_score = 0;
	int32_t beamformee_score = 0;
	int32_t band_score = 0;
	int32_t nss_score = 0;
	int32_t security_score = 0;
	int32_t congestion_score = 0;
	int32_t congestion_pct = 0;
	int32_t oce_wan_score = 0;
	uint8_t oce_ap_tx_pwr_score = 0;
	uint8_t oce_subnet_id_score = 0;
	uint32_t sae_pk_score = 0;
	bool oce_subnet_id_present = 0;
	bool sae_pk_cap_present = 0;
	int8_t ap_tx_pwr_dbm = 0;
	uint8_t prorated_pcnt;
	bool is_vht = false;
	int8_t good_rssi_threshold;
	int8_t rssi_pref_5g_rssi_thresh;
	bool same_bucket = false;
	bool ap_su_beam_former = false;
	struct wlan_ie_vhtcaps *vht_cap;
	struct wlan_ie_hecaps *he_cap;
	struct scoring_cfg *score_config;
	struct weight_cfg *weight_config;
	uint32_t sta_nss;
	struct psoc_mlme_obj *mlme_psoc_obj;
	struct psoc_phy_config *phy_config;
	uint32_t eht_score;
	enum MLO_TYPE bss_mlo_type;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return 0;

	phy_config = &mlme_psoc_obj->psoc_cfg.phy_config;
	score_config = &mlme_psoc_obj->psoc_cfg.score_config;
	weight_config = &score_config->weight_config;

	if (score_config->is_bssid_hint_priority && bssid_hint &&
	    qdf_is_macaddr_equal(bssid_hint, &entry->bssid)) {
		entry->bss_score = CM_BEST_CANDIDATE_MAX_BSS_SCORE;
		mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): rssi %d BSSID hint given, give max score %d",
				QDF_MAC_ADDR_REF(entry->bssid.bytes),
				entry->channel.chan_freq,
				entry->rssi_raw,
				CM_BEST_CANDIDATE_MAX_BSS_SCORE);
		return CM_BEST_CANDIDATE_MAX_BSS_SCORE;
	}
	if (score_config->vendor_roam_score_algorithm) {
		score = cm_calculate_etp_score(psoc, entry, phy_config);
		entry->bss_score = score;
		return score;
	}

	bss_mlo_type = cm_bss_mlo_type(psoc, entry, scan_list);
	if (bss_mlo_type == SLO || bss_mlo_type == MLSR) {
		rssi_score =
			cm_calculate_rssi_score(&score_config->rssi_score,
						entry->rssi_raw,
						weight_config->rssi_weightage);
		prorated_pcnt =
			cm_get_rssi_prorate_pct(&score_config->rssi_score,
						entry->rssi_raw,
						weight_config->rssi_weightage);
		score += rssi_score;
		bandwidth_score =
			cm_get_bw_score(weight_config->chan_width_weightage,
					cm_get_ch_width(entry, phy_config),
					prorated_pcnt);
		score += bandwidth_score;

		congestion_score =
			cm_calculate_congestion_score(entry,
						      score_config,
						      &congestion_pct, 0);
		score += congestion_score * CM_SLO_CONGESTION_MAX_SCORE /
			 CM_MAX_PCT_SCORE;
		if (bss_mlo_type == MLSR)
			score += cm_calculate_emlsr_score(weight_config);
	} else {
		score += cm_calculate_mlo_bss_score(psoc, entry, score_config,
						    phy_config, scan_list,
						    &prorated_pcnt);
	}

	pcl_score = cm_calculate_pcl_score(psoc, pcl_chan_weight,
					   weight_config->pcl_weightage);
	score += pcl_score;

	/*
	 * Add HT weight if HT is supported by the AP. In case
	 * of 6 GHZ AP, HT and VHT won't be supported so that
	 * these weightage to the same by default to match
	 * with 2.4/5 GHZ APs where HT, VHT is supported
	 */
	if (phy_config->ht_cap && (entry->ie_list.htcap ||
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(entry->channel.chan_freq)))
		ht_score = prorated_pcnt *
				weight_config->ht_caps_weightage;
	score += ht_score;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq)) {
		if (phy_config->vht_24G_cap)
			is_vht = true;
	} else if (phy_config->vht_cap) {
		is_vht = true;
	}

	/* Add VHT score to 6 GHZ AP to match with 2.4/5 GHZ APs */
	if (is_vht && (entry->ie_list.vhtcap ||
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(entry->channel.chan_freq)))
		vht_score = prorated_pcnt *
				 weight_config->vht_caps_weightage;
	score += vht_score;

	if (phy_config->he_cap && entry->ie_list.hecap)
		he_score = prorated_pcnt *
			   weight_config->he_caps_weightage;
	score += he_score;

	good_rssi_threshold =
		score_config->rssi_score.good_rssi_threshold * (-1);
	rssi_pref_5g_rssi_thresh =
		score_config->rssi_score.rssi_pref_5g_rssi_thresh * (-1);
	if (entry->rssi_raw < good_rssi_threshold)
		same_bucket = cm_rssi_is_same_bucket(good_rssi_threshold,
				entry->rssi_raw, rssi_pref_5g_rssi_thresh,
				score_config->rssi_score.bad_rssi_bucket_size);

	vht_cap = (struct wlan_ie_vhtcaps *)util_scan_entry_vhtcap(entry);
	he_cap = (struct wlan_ie_hecaps *)util_scan_entry_hecap(entry);

	if (vht_cap && vht_cap->su_beam_former) {
		ap_su_beam_former = true;
	} else if (he_cap && QDF_GET_BITS(*(he_cap->he_phy_cap.phy_cap_bytes +
		   WLAN_HE_PHYCAP_SU_BFER_OFFSET), WLAN_HE_PHYCAP_SU_BFER_IDX,
		   WLAN_HE_PHYCAP_SU_BFER_BITS)) {
		ap_su_beam_former = true;
	} else {
		ap_su_beam_former = cm_get_su_beam_former(entry);
	}

	if (phy_config->beamformee_cap && is_vht &&
	    ap_su_beam_former &&
	    (entry->rssi_raw > rssi_pref_5g_rssi_thresh) && !same_bucket)
		beamformee_score = CM_MAX_PCT_SCORE *
				weight_config->beamforming_cap_weightage;
	score += beamformee_score;

	/*
	 * Consider OCE WAN score and band preference score only if
	 * congestion_pct is greater than CONGESTION_THRSHOLD_FOR_BAND_OCE_SCORE
	 */
	if (congestion_pct < CM_CONGESTION_THRSHOLD_FOR_BAND_OCE_SCORE) {
		/*
		 * If AP is on 5/6 GHZ channel , extra weigtage is added to BSS
		 * score. if RSSI is greater than 5g rssi threshold or fall in
		 * same bucket else give weigtage to 2.4 GHZ AP.
		 */
		if ((entry->rssi_raw > rssi_pref_5g_rssi_thresh) &&
		    !same_bucket) {
			if (!WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq))
				band_score = cm_get_band_score(
						entry->channel.chan_freq,
						score_config);
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			   entry->channel.chan_freq)) {
			band_score = cm_get_band_score(entry->channel.chan_freq,
						       score_config);
		}
		score += band_score;

		oce_wan_score = cm_calculate_oce_wan_score(entry, score_config);
		score += oce_wan_score;
	}

	oce_ap_tx_pwr_score =
		cm_calculate_oce_ap_tx_pwr_weightage(entry, score_config,
						     &ap_tx_pwr_dbm);
	score += oce_ap_tx_pwr_score;

	oce_subnet_id_score = cm_calculate_oce_subnet_id_weightage(entry,
						score_config,
						&oce_subnet_id_present);
	score += oce_subnet_id_score;

	sae_pk_score = cm_calculate_sae_pk_ap_weightage(entry, score_config,
							&sae_pk_cap_present);
	score += sae_pk_score;

	sta_nss = cm_get_sta_nss(psoc, entry->channel.chan_freq,
				 phy_config->vdev_nss_24g,
				 phy_config->vdev_nss_5g);

	/*
	 * If station support nss as 2*2 but AP support NSS as 1*1,
	 * this AP will be given half weight compare to AP which are having
	 * NSS as 2*2.
	 */
	nss_score = cm_calculate_nss_score(psoc, score_config, entry->nss,
					   prorated_pcnt, sta_nss);
	score += nss_score;

	/*
	 * Since older FW will stick to the single AKM for roaming,
	 * no need to check the fw capability.
	 */
	security_score = cm_calculate_security_score(score_config,
						     entry->neg_sec_info);
	score += security_score;

	eht_score = cm_calculate_eht_score(entry, score_config, phy_config,
					   prorated_pcnt);
	score += eht_score;

	mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): rssi %d HT %d VHT %d HE %d EHT %d su bfer %d phy %d  air time frac %d qbss %d cong_pct %d NSS %d ap_tx_pwr_dbm %d oce_subnet_id_present %d sae_pk_cap_present %d prorated_pcnt %d keymgmt 0x%x mlo type %d",
			QDF_MAC_ADDR_REF(entry->bssid.bytes),
			entry->channel.chan_freq,
			entry->rssi_raw, util_scan_entry_htcap(entry) ? 1 : 0,
			util_scan_entry_vhtcap(entry) ? 1 : 0,
			util_scan_entry_hecap(entry) ? 1 : 0,
			util_scan_entry_ehtcap(entry) ? 1 : 0,
			ap_su_beam_former,
			entry->phy_mode, entry->air_time_fraction,
			entry->qbss_chan_load, congestion_pct, entry->nss,
			ap_tx_pwr_dbm, oce_subnet_id_present,
			sae_pk_cap_present, prorated_pcnt,
			entry->neg_sec_info.key_mgmt, bss_mlo_type);

	mlme_nofl_debug("Scores: rssi %d pcl %d ht %d vht %d he %d bfee %d bw %d band %d congestion %d nss %d oce wan %d oce ap tx pwr %d subnet %d sae_pk %d eht %d security %d TOTAL %d",
			rssi_score, pcl_score, ht_score,
			vht_score, he_score, beamformee_score, bandwidth_score,
			band_score, congestion_score, nss_score, oce_wan_score,
			oce_ap_tx_pwr_score, oce_subnet_id_score,
			sae_pk_score, eht_score, security_score, score);

	entry->bss_score = score;

	return score;
}

static void cm_list_insert_sorted(qdf_list_t *scan_list,
				  struct scan_cache_node *scan_entry)
{
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct scan_cache_node *curr_entry;

	qdf_list_peek_front(scan_list, &cur_node);
	while (cur_node) {
		curr_entry = qdf_container_of(cur_node, struct scan_cache_node,
					      node);
		if (cm_is_better_bss(scan_entry->entry, curr_entry->entry)) {
			qdf_list_insert_before(scan_list, &scan_entry->node,
					       &curr_entry->node);
			break;
		}
		qdf_list_peek_next(scan_list, cur_node, &next_node);
		cur_node = next_node;
		next_node = NULL;
	}

	if (!cur_node)
		qdf_list_insert_back(scan_list, &scan_entry->node);
}

#ifdef CONN_MGR_ADV_FEATURE
/**
 * cm_is_bad_rssi_entry() - check the entry have rssi value, if rssi is lower
 * than threshold limit, then it is considered ad bad rssi value.
 * @scan_entry: pointer to scan cache entry
 * @score_config: pointer to score config structure
 * @bssid_hint: bssid hint
 *
 * Return: true if rssi is lower than threshold
 */
static
bool cm_is_bad_rssi_entry(struct scan_cache_entry *scan_entry,
			  struct scoring_cfg *score_config,
			  struct qdf_mac_addr *bssid_hint)
{
	int8_t rssi_threshold =
		score_config->rssi_score.con_non_hint_target_rssi_threshold;

	 /* do not need to consider BSSID hint if it is invalid entry(zero) */
	if (qdf_is_macaddr_zero(bssid_hint))
		return false;

	if (score_config->is_bssid_hint_priority &&
	    !qdf_is_macaddr_equal(bssid_hint, &scan_entry->bssid) &&
	    scan_entry->rssi_raw < rssi_threshold) {
		mlme_nofl_debug("Candidate(  " QDF_MAC_ADDR_FMT "  freq %d): remove entry, rssi %d lower than rssi_threshold %d",
				QDF_MAC_ADDR_REF(scan_entry->bssid.bytes),
				scan_entry->channel.chan_freq,
				scan_entry->rssi_raw, rssi_threshold);
		return true;
	}

	return false;
}

/**
 * cm_update_bss_score_for_mac_addr_matching() - boost score based on mac
 * address matching
 * @scan_entry: pointer to scan cache entry
 * @self_mac: pointer to bssid to be matched
 *
 * Some IOT APs only allow to connect if last 3 bytes of BSSID
 * and self MAC is same. They create a new bssid on receiving
 * unicast probe/auth req from STA and allow STA to connect to
 * this matching BSSID only. So boost the matching BSSID to try
 * to connect to this BSSID.
 *
 * Return: void
 */
static void
cm_update_bss_score_for_mac_addr_matching(struct scan_cache_node *scan_entry,
					  struct qdf_mac_addr *self_mac)
{
	struct qdf_mac_addr *scan_entry_bssid;

	if (!self_mac)
		return;
	scan_entry_bssid = &scan_entry->entry->bssid;
	if (QDF_IS_LAST_3_BYTES_OF_MAC_SAME(
		self_mac, scan_entry_bssid)) {
		mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): boost bss score due to same last 3 byte match",
				QDF_MAC_ADDR_REF(
				scan_entry_bssid->bytes),
				scan_entry->entry->channel.chan_freq);
		scan_entry->entry->bss_score =
			CM_BEST_CANDIDATE_MAX_BSS_SCORE;
	}
}
#else
static inline
bool cm_is_bad_rssi_entry(struct scan_cache_entry *scan_entry,
			  struct scoring_cfg *score_config,
			  struct qdf_mac_addr *bssid_hint)

{
	return false;
}

static void
cm_update_bss_score_for_mac_addr_matching(struct scan_cache_node *scan_entry,
					  struct qdf_mac_addr *self_mac)
{
}
#endif

void wlan_cm_calculate_bss_score(struct wlan_objmgr_pdev *pdev,
				 struct pcl_freq_weight_list *pcl_lst,
				 qdf_list_t *scan_list,
				 struct qdf_mac_addr *bssid_hint,
				 struct qdf_mac_addr *self_mac)
{
	struct scan_cache_node *scan_entry;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct psoc_mlme_obj *mlme_psoc_obj;
	struct scoring_cfg *score_config;
	int pcl_chan_weight;
	QDF_STATUS status;
	struct psoc_phy_config *config;
	enum cm_denylist_action denylist_action;
	struct wlan_objmgr_psoc *psoc;
	bool assoc_allowed;
	struct scan_cache_node *force_connect_candidate = NULL;
	bool are_all_candidate_denylisted = true;
	bool is_rssi_bad = false;

	psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc) {
		mlme_err("psoc NULL");
		return;
	}
	if (!scan_list) {
		mlme_err("Scan list NULL");
		return;
	}

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	score_config = &mlme_psoc_obj->psoc_cfg.score_config;
	config = &mlme_psoc_obj->psoc_cfg.phy_config;

	mlme_nofl_debug("Self caps: HT %d VHT %d HE %d EHT %d VHT_24Ghz %d BF cap %d bw_above_20_24ghz %d bw_above_20_5ghz %d 2.4G NSS %d 5G NSS %d",
			config->ht_cap, config->vht_cap,
			config->he_cap, config->eht_cap, config->vht_24G_cap,
			config->beamformee_cap, config->bw_above_20_24ghz,
			config->bw_above_20_5ghz, config->vdev_nss_24g,
			config->vdev_nss_5g);

	/* calculate score for each AP */
	if (qdf_list_peek_front(scan_list, &cur_node) != QDF_STATUS_SUCCESS) {
		mlme_err("failed to peer front of scan list");
		return;
	}

	while (cur_node) {
		qdf_list_peek_next(scan_list, cur_node, &next_node);
		pcl_chan_weight = 0;
		scan_entry = qdf_container_of(cur_node, struct scan_cache_node,
					      node);

		is_rssi_bad = cm_is_bad_rssi_entry(scan_entry->entry,
						   score_config, bssid_hint);

		assoc_allowed = cm_is_assoc_allowed(mlme_psoc_obj,
						    scan_entry->entry);

		if (assoc_allowed && !is_rssi_bad)
			denylist_action = wlan_denylist_action_on_bssid(pdev,
							scan_entry->entry);
		else
			denylist_action = CM_DLM_FORCE_REMOVE;

		if (denylist_action == CM_DLM_NO_ACTION ||
		    denylist_action == CM_DLM_AVOID)
			are_all_candidate_denylisted = false;

		if (denylist_action == CM_DLM_NO_ACTION &&
		    pcl_lst && pcl_lst->num_of_pcl_channels &&
		    scan_entry->entry->rssi_raw > CM_PCL_RSSI_THRESHOLD &&
		    score_config->weight_config.pcl_weightage) {
			if (cm_get_pcl_weight_of_channel(
					scan_entry->entry->channel.chan_freq,
					pcl_lst, &pcl_chan_weight)) {
				mlme_debug("pcl freq %d pcl_chan_weight %d",
					   scan_entry->entry->channel.chan_freq,
					   pcl_chan_weight);
			}
		}

		if (denylist_action == CM_DLM_NO_ACTION ||
		    (are_all_candidate_denylisted && denylist_action ==
		     CM_DLM_REMOVE)) {
			cm_calculate_bss_score(psoc, scan_entry->entry,
					       pcl_chan_weight, bssid_hint,
					       scan_list);
		} else if (denylist_action == CM_DLM_AVOID) {
			/* add min score so that it is added back in the end */
			scan_entry->entry->bss_score =
					CM_AVOID_CANDIDATE_MIN_SCORE;
			mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): rssi %d, is in Avoidlist, give min score %d",
					QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes),
					scan_entry->entry->channel.chan_freq,
					scan_entry->entry->rssi_raw,
					scan_entry->entry->bss_score);
		} else {
			mlme_nofl_debug("Candidate("QDF_MAC_ADDR_FMT" freq %d): denylist_action %d",
					QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes),
					scan_entry->entry->channel.chan_freq,
					denylist_action);
		}

		cm_update_bss_score_for_mac_addr_matching(scan_entry, self_mac);
		/*
		 * The below logic is added to select the best candidate
		 * amongst the denylisted candidates. This is done to
		 * handle a case where all the BSSIDs become denylisted
		 * and hence there are continuous connection failures.
		 * With the below logic if the action on BSSID is to remove
		 * then we keep a backup node and restore the candidate
		 * list.
		 */
		if (denylist_action == CM_DLM_REMOVE &&
		    are_all_candidate_denylisted) {
			if (!force_connect_candidate) {
				force_connect_candidate =
					qdf_mem_malloc(
					   sizeof(*force_connect_candidate));
				if (!force_connect_candidate)
					return;
				force_connect_candidate->entry =
					util_scan_copy_cache_entry(scan_entry->entry);
				if (!force_connect_candidate->entry)
					return;
			} else if (cm_is_better_bss(
				   scan_entry->entry,
				   force_connect_candidate->entry)) {
				util_scan_free_cache_entry(
					force_connect_candidate->entry);
				force_connect_candidate->entry =
				  util_scan_copy_cache_entry(scan_entry->entry);
				if (!force_connect_candidate->entry)
					return;
			}
		}

		/* Remove node from current location to add node back sorted */
		status = qdf_list_remove_node(scan_list, cur_node);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err("failed to remove node for BSS "QDF_MAC_ADDR_FMT" from scan list",
				 QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes));
			return;
		}

		/*
		 * If CM_DLM_REMOVE ie denylisted or assoc not allowed then
		 * free the entry else add back to the list sorted
		 */
		if (denylist_action == CM_DLM_REMOVE ||
		    denylist_action == CM_DLM_FORCE_REMOVE) {
			if (assoc_allowed && !is_rssi_bad)
				mlme_nofl_debug("Candidate( " QDF_MAC_ADDR_FMT " freq %d): rssi %d, dlm action %d is in Denylist, remove entry",
					QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes),
					scan_entry->entry->channel.chan_freq,
					scan_entry->entry->rssi_raw,
					denylist_action);
			util_scan_free_cache_entry(scan_entry->entry);
			qdf_mem_free(scan_entry);
		} else {
			cm_list_insert_sorted(scan_list, scan_entry);
		}

		cur_node = next_node;
		next_node = NULL;
	}

	if (are_all_candidate_denylisted && force_connect_candidate) {
		mlme_nofl_debug("All candidates in denylist, Candidate( " QDF_MAC_ADDR_FMT " freq %d): rssi %d, selected for connection",
			QDF_MAC_ADDR_REF(force_connect_candidate->entry->bssid.bytes),
			force_connect_candidate->entry->channel.chan_freq,
			force_connect_candidate->entry->rssi_raw);
		cm_list_insert_sorted(scan_list, force_connect_candidate);
	} else if (force_connect_candidate) {
		util_scan_free_cache_entry(force_connect_candidate->entry);
		qdf_mem_free(force_connect_candidate);
	}
}

#ifdef CONFIG_BAND_6GHZ
static bool cm_check_h2e_support(const uint8_t *rsnxe)
{
	const uint8_t *rsnxe_cap;
	uint8_t cap_len;

	rsnxe_cap = wlan_crypto_parse_rsnxe_ie(rsnxe, &cap_len);
	if (!rsnxe_cap) {
		mlme_debug("RSNXE caps not present");
		return false;
	}

	if (*rsnxe_cap & WLAN_CRYPTO_RSNX_CAP_SAE_H2E)
		return true;

	mlme_debug("RSNXE caps %x dont have H2E support", *rsnxe_cap);

	return false;
}

#ifdef CONN_MGR_ADV_FEATURE
static bool wlan_cm_wfa_get_test_feature_flags(struct wlan_objmgr_psoc *psoc)
{
	return wlan_wfa_get_test_feature_flags(psoc, WFA_TEST_IGNORE_RSNXE);
}
#else
static bool wlan_cm_wfa_get_test_feature_flags(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

bool wlan_cm_6ghz_allowed_for_akm(struct wlan_objmgr_psoc *psoc,
				  uint32_t key_mgmt, uint16_t rsn_caps,
				  const uint8_t *rsnxe, uint8_t sae_pwe,
				  bool is_wps)
{
	struct psoc_mlme_obj *mlme_psoc_obj;
	struct scoring_cfg *config;

	/* Allow connection for WPS security */
	if (is_wps)
		return true;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return false;

	config = &mlme_psoc_obj->psoc_cfg.score_config;
	/*
	 * if check_6ghz_security is not set check if key_mgmt_mask_6ghz is set
	 * if key_mgmt_mask_6ghz is set check if AKM matches the user configured
	 * 6Ghz security
	 */
	if (!config->check_6ghz_security) {
		if (!config->key_mgmt_mask_6ghz)
			return true;
		/*
		 * Check if any AKM is allowed as per user 6Ghz allowed AKM mask
		 */
		if (!(config->key_mgmt_mask_6ghz & key_mgmt)) {
			mlme_debug("user configured mask %x didn't match AKM %x",
				   config->key_mgmt_mask_6ghz , key_mgmt);
			return false;
		}

		return true;
	}

	/* Check if any AKM is allowed as per the 6Ghz allowed AKM mask */
	if (!(key_mgmt & ALLOWED_KEYMGMT_6G_MASK)) {
		mlme_debug("AKM 0x%x didn't match with allowed 6ghz AKM 0x%x",
			   key_mgmt, ALLOWED_KEYMGMT_6G_MASK);
		return false;
	}

	/* if check_6ghz_security is set validate all checks for 6Ghz */
	if (!(rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED)) {
		mlme_debug("PMF not enabled for 6GHz AP");
		return false;
	}

	/* for SAE we need to check H2E support */
	if (!(QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_SAE) ||
	    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_SAE) ||
	    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_SAE_EXT_KEY) ||
	    QDF_HAS_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_SAE_EXT_KEY)))
		return true;

	return (cm_check_h2e_support(rsnxe) ||
		wlan_cm_wfa_get_test_feature_flags(psoc));
}

void wlan_cm_set_check_6ghz_security(struct wlan_objmgr_psoc *psoc,
				     bool value)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	mlme_debug("6ghz security check val %x", value);
	mlme_psoc_obj->psoc_cfg.score_config.check_6ghz_security = value;
}

void wlan_cm_reset_check_6ghz_security(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	mlme_psoc_obj->psoc_cfg.score_config.check_6ghz_security =
					cfg_get(psoc, CFG_CHECK_6GHZ_SECURITY);
}

bool wlan_cm_get_check_6ghz_security(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return false;

	return mlme_psoc_obj->psoc_cfg.score_config.check_6ghz_security;
}

void wlan_cm_set_standard_6ghz_conn_policy(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	mlme_debug("6ghz standard connection policy val %x", value);
	mlme_psoc_obj->psoc_cfg.score_config.standard_6ghz_conn_policy = value;
}

bool wlan_cm_get_standard_6ghz_conn_policy(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return false;

	return mlme_psoc_obj->psoc_cfg.score_config.standard_6ghz_conn_policy;
}

void wlan_cm_set_disable_vlp_sta_conn_to_sp_ap(struct wlan_objmgr_psoc *psoc,
					       bool value)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	mlme_debug("disable_vlp_sta_conn_to_sp_ap val %x", value);
	mlme_psoc_obj->psoc_cfg.score_config.disable_vlp_sta_conn_to_sp_ap = value;
}

bool wlan_cm_get_disable_vlp_sta_conn_to_sp_ap(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return false;

	return mlme_psoc_obj->psoc_cfg.score_config.disable_vlp_sta_conn_to_sp_ap;
}

void wlan_cm_set_6ghz_key_mgmt_mask(struct wlan_objmgr_psoc *psoc,
				     uint32_t value)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return;

	mlme_debug("key_mgmt_mask_6ghz %x", value);
	mlme_psoc_obj->psoc_cfg.score_config.key_mgmt_mask_6ghz = value;
}

uint32_t wlan_cm_get_6ghz_key_mgmt_mask(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *mlme_psoc_obj;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (!mlme_psoc_obj)
		return DEFAULT_KEYMGMT_6G_MASK;

	return mlme_psoc_obj->psoc_cfg.score_config.key_mgmt_mask_6ghz;
}

static void cm_fill_6ghz_params(struct wlan_objmgr_psoc *psoc,
				struct scoring_cfg *score_cfg)
{
	/* Allow all security in 6Ghz by default */
	score_cfg->check_6ghz_security = cfg_get(psoc, CFG_CHECK_6GHZ_SECURITY);
	score_cfg->key_mgmt_mask_6ghz =
				cfg_get(psoc, CFG_6GHZ_ALLOWED_AKM_MASK);
}
#else
static inline void cm_fill_6ghz_params(struct wlan_objmgr_psoc *psoc,
				       struct scoring_cfg *score_cfg)
{
}
#endif

static uint32_t
cm_limit_max_per_index_score(uint32_t per_index_score)
{
	uint8_t i, score;

	for (i = 0; i < CM_MAX_INDEX_PER_INI; i++) {
		score = CM_GET_SCORE_PERCENTAGE(per_index_score, i);
		if (score > CM_MAX_PCT_SCORE)
			CM_SET_SCORE_PERCENTAGE(per_index_score,
						CM_MAX_PCT_SCORE, i);
	}

	return per_index_score;
}

#ifdef WLAN_FEATURE_11BE_MLO

#define CM_EHT_CAP_WEIGHTAGE 2
#define CM_MLO_WEIGHTAGE 3
#define CM_WLM_INDICATION_WEIGHTAGE 2
#define CM_EMLSR_WEIGHTAGE 3
static void cm_init_mlo_score_config(struct wlan_objmgr_psoc *psoc,
				     struct scoring_cfg *score_cfg,
				     uint32_t *total_weight)
{
	score_cfg->weight_config.eht_caps_weightage =
		cfg_get(psoc, CFG_SCORING_EHT_CAPS_WEIGHTAGE);

	score_cfg->weight_config.mlo_weightage =
		cfg_get(psoc, CFG_SCORING_MLO_WEIGHTAGE);

	score_cfg->weight_config.wlm_indication_weightage =
		cfg_get(psoc, CFG_SCORING_WLM_INDICATION_WEIGHTAGE);

	score_cfg->weight_config.joint_rssi_alpha =
				cfg_get(psoc, CFG_SCORING_JOINT_RSSI_ALPHA);

	score_cfg->weight_config.low_band_rssi_boost =
				cfg_get(psoc, CFG_SCORING_LOW_BAND_RSSI_BOOST);

	score_cfg->weight_config.joint_esp_alpha =
				cfg_get(psoc, CFG_SCORING_JOINT_ESP_ALPHA);

	score_cfg->weight_config.low_band_esp_boost =
				cfg_get(psoc, CFG_SCORING_LOW_BAND_ESP_BOOST);

	score_cfg->weight_config.joint_oce_alpha =
				cfg_get(psoc, CFG_SCORING_JOINT_OCE_ALPHA);

	score_cfg->weight_config.low_band_oce_boost =
				cfg_get(psoc, CFG_SCORING_LOW_BAND_OCE_BOOST);

	score_cfg->weight_config.emlsr_weightage =
		cfg_get(psoc, CFG_SCORING_EMLSR_WEIGHTAGE);

	score_cfg->mlsr_link_selection =
		cfg_get(psoc, CFG_SCORING_MLSR_LINK_SELECTION);

	*total_weight += score_cfg->weight_config.eht_caps_weightage +
			 score_cfg->weight_config.mlo_weightage +
			 score_cfg->weight_config.wlm_indication_weightage +
			 score_cfg->weight_config.emlsr_weightage;
}

static void cm_set_default_mlo_weights(struct scoring_cfg *score_cfg)
{
	score_cfg->weight_config.eht_caps_weightage = CM_EHT_CAP_WEIGHTAGE;
	score_cfg->weight_config.mlo_weightage = CM_MLO_WEIGHTAGE;
	score_cfg->weight_config.wlm_indication_weightage =
						CM_WLM_INDICATION_WEIGHTAGE;
	score_cfg->weight_config.emlsr_weightage = CM_EMLSR_WEIGHTAGE;
}

static void cm_init_bw_weight_per_index(struct wlan_objmgr_psoc *psoc,
					struct scoring_cfg *score_cfg)
{
	score_cfg->bandwidth_weight_per_index[0] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX));

	score_cfg->bandwidth_weight_per_index[1] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_4_TO_7));

	score_cfg->bandwidth_weight_per_index[2] =
		cm_limit_max_per_index_score(
		     cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_8_TO_11));

	score_cfg->bandwidth_weight_per_index[3] =
		cm_limit_max_per_index_score(
		    cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_12_TO_15));

	score_cfg->bandwidth_weight_per_index[4] =
		cm_limit_max_per_index_score(
		    cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_16_TO_19));

	score_cfg->bandwidth_weight_per_index[5] =
		cm_limit_max_per_index_score(
		    cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_20_TO_23));

	score_cfg->bandwidth_weight_per_index[6] =
		cm_limit_max_per_index_score(
		    cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_24_TO_27));

	score_cfg->bandwidth_weight_per_index[7] =
		cm_limit_max_per_index_score(
		    cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_28_TO_31));

	score_cfg->bandwidth_weight_per_index[8] =
		cm_limit_max_per_index_score(
		    cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_32_TO_35));
}

static void cm_init_nss_weight_per_index(struct wlan_objmgr_psoc *psoc,
					 struct scoring_cfg *score_cfg)
{
	score_cfg->nss_weight_per_index[0] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_NSS_WEIGHT_PER_IDX));

	score_cfg->nss_weight_per_index[1] =
		cm_limit_max_per_index_score(
		      cfg_get(psoc, CFG_SCORING_ML_NSS_WEIGHT_PER_IDX_4_TO_7));
}
#else
static void cm_init_mlo_score_config(struct wlan_objmgr_psoc *psoc,
				     struct scoring_cfg *score_cfg,
				     uint32_t *total_weight)
{
}

static void cm_set_default_mlo_weights(struct scoring_cfg *score_cfg)
{
}

#ifdef WLAN_FEATURE_11BE
static void cm_init_bw_weight_per_index(struct wlan_objmgr_psoc *psoc,
					struct scoring_cfg *score_cfg)
{
	score_cfg->bandwidth_weight_per_index[0] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX));

	score_cfg->bandwidth_weight_per_index[1] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_4_TO_7));

	score_cfg->bandwidth_weight_per_index[2] =
		cm_limit_max_per_index_score(
		     cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX_8_TO_11));
}
#else
static void cm_init_bw_weight_per_index(struct wlan_objmgr_psoc *psoc,
					struct scoring_cfg *score_cfg)
{
	score_cfg->bandwidth_weight_per_index[0] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_BW_WEIGHT_PER_IDX));
}
#endif

static void cm_init_nss_weight_per_index(struct wlan_objmgr_psoc *psoc,
					 struct scoring_cfg *score_cfg)
{
	score_cfg->nss_weight_per_index[0] =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_NSS_WEIGHT_PER_IDX));
}
#endif

void wlan_cm_init_score_config(struct wlan_objmgr_psoc *psoc,
			       struct scoring_cfg *score_cfg)
{
	uint32_t total_weight;

	score_cfg->weight_config.rssi_weightage =
		cfg_get(psoc, CFG_SCORING_RSSI_WEIGHTAGE);
	score_cfg->weight_config.ht_caps_weightage =
		cfg_get(psoc, CFG_SCORING_HT_CAPS_WEIGHTAGE);
	score_cfg->weight_config.vht_caps_weightage =
		cfg_get(psoc, CFG_SCORING_VHT_CAPS_WEIGHTAGE);
	score_cfg->weight_config.he_caps_weightage =
		cfg_get(psoc, CFG_SCORING_HE_CAPS_WEIGHTAGE);
	score_cfg->weight_config.chan_width_weightage =
		cfg_get(psoc, CFG_SCORING_CHAN_WIDTH_WEIGHTAGE);
	score_cfg->weight_config.chan_band_weightage =
		cfg_get(psoc, CFG_SCORING_CHAN_BAND_WEIGHTAGE);
	score_cfg->weight_config.nss_weightage =
		cfg_get(psoc, CFG_SCORING_NSS_WEIGHTAGE);
	score_cfg->weight_config.beamforming_cap_weightage =
		cfg_get(psoc, CFG_SCORING_BEAMFORM_CAP_WEIGHTAGE);
	score_cfg->weight_config.pcl_weightage =
		cfg_get(psoc, CFG_SCORING_PCL_WEIGHTAGE);
	score_cfg->weight_config.channel_congestion_weightage =
		cfg_get(psoc, CFG_SCORING_CHAN_CONGESTION_WEIGHTAGE);
	score_cfg->weight_config.oce_wan_weightage =
		cfg_get(psoc, CFG_SCORING_OCE_WAN_WEIGHTAGE);
	score_cfg->weight_config.oce_ap_tx_pwr_weightage =
				cfg_get(psoc, CFG_OCE_AP_TX_PWR_WEIGHTAGE);
	score_cfg->weight_config.oce_subnet_id_weightage =
				cfg_get(psoc, CFG_OCE_SUBNET_ID_WEIGHTAGE);
	score_cfg->weight_config.sae_pk_ap_weightage =
				cfg_get(psoc, CFG_SAE_PK_AP_WEIGHTAGE);
	score_cfg->weight_config.security_weightage = CM_SECURITY_WEIGHTAGE;

	total_weight =  score_cfg->weight_config.rssi_weightage +
			score_cfg->weight_config.ht_caps_weightage +
			score_cfg->weight_config.vht_caps_weightage +
			score_cfg->weight_config.he_caps_weightage +
			score_cfg->weight_config.chan_width_weightage +
			score_cfg->weight_config.chan_band_weightage +
			score_cfg->weight_config.nss_weightage +
			score_cfg->weight_config.beamforming_cap_weightage +
			score_cfg->weight_config.pcl_weightage +
			score_cfg->weight_config.channel_congestion_weightage +
			score_cfg->weight_config.oce_wan_weightage +
			score_cfg->weight_config.oce_ap_tx_pwr_weightage +
			score_cfg->weight_config.oce_subnet_id_weightage +
			score_cfg->weight_config.sae_pk_ap_weightage +
			score_cfg->weight_config.security_weightage;

	cm_init_mlo_score_config(psoc, score_cfg, &total_weight);

	/*
	 * If configured weights are greater than max weight,
	 * fallback to default weights
	 */
	if (total_weight > CM_BEST_CANDIDATE_MAX_WEIGHT) {
		mlme_err("Total weight greater than %d, using default weights",
			 CM_BEST_CANDIDATE_MAX_WEIGHT);
		score_cfg->weight_config.rssi_weightage = CM_RSSI_WEIGHTAGE;
		score_cfg->weight_config.ht_caps_weightage =
						CM_HT_CAPABILITY_WEIGHTAGE;
		score_cfg->weight_config.vht_caps_weightage =
						CM_VHT_CAP_WEIGHTAGE;
		score_cfg->weight_config.he_caps_weightage =
						CM_HE_CAP_WEIGHTAGE;
		score_cfg->weight_config.chan_width_weightage =
						CM_CHAN_WIDTH_WEIGHTAGE;
		score_cfg->weight_config.chan_band_weightage =
						CM_CHAN_BAND_WEIGHTAGE;
		score_cfg->weight_config.nss_weightage = CM_NSS_WEIGHTAGE;
		score_cfg->weight_config.beamforming_cap_weightage =
						CM_BEAMFORMING_CAP_WEIGHTAGE;
		score_cfg->weight_config.pcl_weightage = CM_PCL_WEIGHT;
		score_cfg->weight_config.channel_congestion_weightage =
						CM_CHANNEL_CONGESTION_WEIGHTAGE;
		score_cfg->weight_config.oce_wan_weightage =
						CM_OCE_WAN_WEIGHTAGE;
		score_cfg->weight_config.oce_ap_tx_pwr_weightage =
						CM_OCE_AP_TX_POWER_WEIGHTAGE;
		score_cfg->weight_config.oce_subnet_id_weightage =
						CM_OCE_SUBNET_ID_WEIGHTAGE;
		score_cfg->weight_config.sae_pk_ap_weightage =
						CM_SAE_PK_AP_WEIGHTAGE;
		cm_set_default_mlo_weights(score_cfg);
	}

	score_cfg->rssi_score.best_rssi_threshold =
		cfg_get(psoc, CFG_SCORING_BEST_RSSI_THRESHOLD);
	score_cfg->rssi_score.good_rssi_threshold =
		cfg_get(psoc, CFG_SCORING_GOOD_RSSI_THRESHOLD);
	score_cfg->rssi_score.bad_rssi_threshold =
		cfg_get(psoc, CFG_SCORING_BAD_RSSI_THRESHOLD);

	score_cfg->rssi_score.good_rssi_pcnt =
		cfg_get(psoc, CFG_SCORING_GOOD_RSSI_PERCENT);
	score_cfg->rssi_score.bad_rssi_pcnt =
		cfg_get(psoc, CFG_SCORING_BAD_RSSI_PERCENT);

	score_cfg->rssi_score.good_rssi_bucket_size =
		cfg_get(psoc, CFG_SCORING_GOOD_RSSI_BUCKET_SIZE);
	score_cfg->rssi_score.bad_rssi_bucket_size =
		cfg_get(psoc, CFG_SCORING_BAD_RSSI_BUCKET_SIZE);

	score_cfg->rssi_score.rssi_pref_5g_rssi_thresh =
		cfg_get(psoc, CFG_SCORING_RSSI_PREF_5G_THRESHOLD);

	score_cfg->rssi_score.con_non_hint_target_rssi_threshold =
		cfg_get(psoc, CFG_CON_NON_HINT_TARGET_MIN_RSSI);

	score_cfg->esp_qbss_scoring.num_slot =
		cfg_get(psoc, CFG_SCORING_NUM_ESP_QBSS_SLOTS);
	score_cfg->esp_qbss_scoring.score_pcnt3_to_0 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_ESP_QBSS_SCORE_IDX_3_TO_0));
	score_cfg->esp_qbss_scoring.score_pcnt7_to_4 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_ESP_QBSS_SCORE_IDX_7_TO_4));
	score_cfg->esp_qbss_scoring.score_pcnt11_to_8 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_ESP_QBSS_SCORE_IDX_11_TO_8));
	score_cfg->esp_qbss_scoring.score_pcnt15_to_12 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_ESP_QBSS_SCORE_IDX_15_TO_12));

	score_cfg->oce_wan_scoring.num_slot =
		cfg_get(psoc, CFG_SCORING_NUM_OCE_WAN_SLOTS);
	score_cfg->oce_wan_scoring.score_pcnt3_to_0 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_OCE_WAN_SCORE_IDX_3_TO_0));
	score_cfg->oce_wan_scoring.score_pcnt7_to_4 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_OCE_WAN_SCORE_IDX_7_TO_4));
	score_cfg->oce_wan_scoring.score_pcnt11_to_8 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_OCE_WAN_SCORE_IDX_11_TO_8));
	score_cfg->oce_wan_scoring.score_pcnt15_to_12 =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_OCE_WAN_SCORE_IDX_15_TO_12));

	score_cfg->band_weight_per_index =
		cm_limit_max_per_index_score(
			cfg_get(psoc, CFG_SCORING_BAND_WEIGHT_PER_IDX));
	score_cfg->is_bssid_hint_priority =
			cfg_get(psoc, CFG_IS_BSSID_HINT_PRIORITY);
	score_cfg->vendor_roam_score_algorithm =
			cfg_get(psoc, CFG_VENDOR_ROAM_SCORE_ALGORITHM);
	score_cfg->check_assoc_disallowed = true;
	cm_fill_6ghz_params(psoc, score_cfg);

	cm_init_bw_weight_per_index(psoc, score_cfg);
	cm_init_nss_weight_per_index(psoc, score_cfg);
	score_cfg->security_weight_per_index = CM_SECURITY_INDEX_WEIGHTAGE;
}

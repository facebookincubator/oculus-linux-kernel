/*
 * Copyright (c) 2011-2012, 2014-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(__SMERRMINTERNAL_H)
#define __SMERRMINTERNAL_H

/**
 * \file  sme_rrm_internal.h
 *
 * \brief prototype for SME RRM APIs
 */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "qdf_lock.h"
#include "qdf_trace.h"
#include "qdf_mem.h"
#include "qdf_types.h"
#include "rrm_global.h"

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct sRrmNeighborReportDesc {
	tListElem List;
	tSirNeighborBssDescription *pNeighborBssDescription;
	uint32_t roamScore;
	uint8_t sessionId;
} tRrmNeighborReportDesc, *tpRrmNeighborReportDesc;

typedef void (*NeighborReportRspCallback)(void *context,
		QDF_STATUS qdf_status);

typedef struct sRrmNeighborRspCallbackInfo {
	uint32_t timeout;       /* in ms.. min value is 10 (10ms) */
	NeighborReportRspCallback neighborRspCallback;
	void *neighborRspCallbackContext;
} tRrmNeighborRspCallbackInfo, *tpRrmNeighborRspCallbackInfo;

typedef struct sRrmNeighborRequestControlInfo {
	/* To check whether a neighbor req is already sent & response pending */
	bool isNeighborRspPending;
	qdf_mc_timer_t neighborRspWaitTimer;
	tRrmNeighborRspCallbackInfo neighborRspCallbackInfo;
} tRrmNeighborRequestControlInfo, *tpRrmNeighborRequestControlInfo;

/**
 * enum rrm_measurement_type: measurement type
 * @RRM_CHANNEL_LOAD: measurement type for channel load req
 * @RRM_BEACON_REPORT: measurement type for beacon report request
 */
enum rrm_measurement_type {
	RRM_CHANNEL_LOAD = 0,
	RRM_BEACON_REPORT = 1,
};

/**
 * enum channel_load_req_info: channel load request info
 * @channel: channel for which the host receives the channel load req from AP
 * @req_chan_width: channel width for which the host receives the channel load
 * req from AP
 * @rrm_scan_tsf: to store jiffies for RRM scan to process chan load req
 * @bw_ind: Contains info for Bandwidth Indication IE
 * @wide_bw: Contains info for Wide Bandwidth Channel IE
 */
struct channel_load_req_info {
	uint8_t channel;
	qdf_freq_t req_freq;
	enum phy_ch_width req_chan_width;
	qdf_time_t rrm_scan_tsf;
	struct bw_ind_element bw_ind;
	struct wide_bw_chan_switch wide_bw;
};

typedef struct sRrmSMEContext {
	uint16_t token;
	struct qdf_mac_addr sessionBssId;
	uint8_t regClass;
	uint8_t measurement_idx;
	enum rrm_measurement_type measurement_type;
	struct channel_load_req_info chan_load_req_info;
	/* list of all channels to be measured. */
	tCsrChannelInfo channelList;
	uint8_t currentIndex;
	/* SSID used in the measuring beacon report. */
	tAniSSID ssId;
	tSirMacAddr bssId;      /* bssid used for beacon report measurement. */
	/* Randomization interval to be used in subsequent measurements. */
	uint16_t randnIntvl;
	uint16_t duration[SIR_ESE_MAX_MEAS_IE_REQS];
	uint8_t measMode[SIR_ESE_MAX_MEAS_IE_REQS];
	uint32_t scan_id;
	tDblLinkList neighborReportCache;
	tRrmNeighborRequestControlInfo neighborReqControlInfo;

#ifdef FEATURE_WLAN_ESE
	tCsrEseBeaconReq eseBcnReqInfo;
	bool eseBcnReqInProgress;
#endif /* FEATURE_WLAN_ESE */
	tRrmMsgReqSource msgSource;
	wlan_scan_requester req_id;
} tRrmSMEContext, *tpRrmSMEContext;

typedef struct sRrmNeighborReq {
	uint8_t no_ssid;
	tSirMacSSid ssid;
	bool neighbor_report_offload;
} tRrmNeighborReq, *tpRrmNeighborReq;

/**
 * sme_rrm_issue_scan_req() - To issue rrm scan request
 * @mac_ctx: pointer to mac context
 *
 * This routine is called to issue rrm scan request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_rrm_issue_scan_req(struct mac_context *mac_ctx, uint8_t idx);

#endif /* #if !defined( __SMERRMINTERNAL_H ) */

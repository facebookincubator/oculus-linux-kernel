/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/* Include Files */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_stats.h>
#include "wlan_hdd_trace.h"
#include "wlan_hdd_ioctl.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_regulatory.h"
#include "wlan_osif_request_manager.h"
#include "wlan_hdd_driver_ops.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_hdd_hostapd.h"
#include "scheduler_api.h"
#include "wlan_reg_ucfg_api.h"
#include "wlan_hdd_p2p.h"
#include <linux/ctype.h>
#include "wma.h"
#include "wlan_hdd_napi.h"
#include "wlan_mlme_ucfg_api.h"

#ifdef FEATURE_WLAN_ESE
#include <sme_api.h>
#include <sir_api.h>
#endif
#include "hif.h"
#include "qdf_func_tracker.h"

#if defined(LINUX_QCMBR)
#define SIOCIOCTLTX99 (SIOCDEVPRIVATE+13)
#endif

/*
 * Size of Driver command strings from upper layer
 */
#define SIZE_OF_SETROAMMODE             11      /* size of SETROAMMODE */
#define SIZE_OF_GETROAMMODE             11      /* size of GETROAMMODE */
#define SIZE_OF_SETSUSPENDMODE          14

/*
 * Size of GETCOUNTRYREV output = (sizeof("GETCOUNTRYREV") = 14) + one (space) +
 *				  (sizeof("country_code") = 3) +
 *                                one (NULL terminating character)
 */
#define SIZE_OF_GETCOUNTRYREV_OUTPUT 20

/*
 * Ibss prop IE from command will be of size:
 * size  = sizeof(oui) + sizeof(oui_data) + 1(Element ID) + 1(EID Length)
 * OUI_DATA should be at least 3 bytes long
 */
#define WLAN_HDD_IBSS_MIN_OUI_DATA_LENGTH (3)

#ifdef FEATURE_WLAN_ESE
#define TID_MIN_VALUE 0
#define TID_MAX_VALUE 15
#endif /* FEATURE_WLAN_ESE */

/*
 * Maximum buffer size used for returning the data back to user space
 */
#define WLAN_MAX_BUF_SIZE 1024
#define WLAN_PRIV_DATA_MAX_LEN    8192

/*
 * Driver miracast parameters 0-Disabled
 * 1-Source, 2-Sink
 */
#define WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL 0
#define WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL 2

/*
 * When ever we need to print IBSSPEERINFOALL for more than 16 STA
 * we will split the printing.
 */
#define NUM_OF_STA_DATA_TO_PRINT 16

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/**
 * struct enable_ext_wow_priv - Private data structure for ext wow
 * @ext_wow_should_suspend: Suspend status of ext wow
 */
struct enable_ext_wow_priv {
	bool ext_wow_should_suspend;
};
#endif

/*
 * Android DRIVER command structures
 */
struct android_wifi_reassoc_params {
	unsigned char bssid[18];
	int channel;
};

#define ANDROID_WIFI_ACTION_FRAME_SIZE 1040
struct android_wifi_af_params {
	unsigned char bssid[18];
	int channel;
	int dwell_time;
	int len;
	unsigned char data[ANDROID_WIFI_ACTION_FRAME_SIZE];
};

/*
 * Define HDD driver command handling entry, each contains a command
 * string and the handler.
 */
typedef int (*hdd_drv_cmd_handler_t)(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx,
				     uint8_t *cmd,
				     uint8_t cmd_name_len,
				     struct hdd_priv_data *priv_data);

/**
 * struct hdd_drv_cmd - Structure to store ioctl command handling info
 * @cmd: Name of the command
 * @handler: Command handler to be invoked
 * @args: Set to true if command expects input parameters
 */
struct hdd_drv_cmd {
	const char *cmd;
	hdd_drv_cmd_handler_t handler;
	bool args;
};

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
#define WLAN_WAIT_TIME_READY_TO_EXTWOW   2000
#define WLAN_HDD_MAX_TCP_PORT            65535
#endif

static uint16_t cesium_pid;

/**
 * drv_cmd_validate() - Validates for space in hdd driver command
 * @command: pointer to input data (its a NULL terminated string)
 * @len: length of command name
 *
 * This function checks for space after command name and if no space
 * is found returns error.
 *
 * Return: 0 for success non-zero for failure
 */
static int drv_cmd_validate(uint8_t *command, int len)
{
	if (command[len] != ' ')
		return -EINVAL;

	return 0;
}

#ifdef FEATURE_WLAN_ESE
struct tsm_priv {
	tAniTrafStrmMetrics tsm_metrics;
};

static void hdd_get_tsm_stats_cb(tAniTrafStrmMetrics tsm_metrics,
				 const uint32_t staId, void *context)
{
	struct osif_request *request;
	struct tsm_priv *priv;

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}
	priv = osif_request_priv(request);
	priv->tsm_metrics = tsm_metrics;
	osif_request_complete(request);
	osif_request_put(request);
	hdd_exit();

}

static int hdd_get_tsm_stats(struct hdd_adapter *adapter,
			     const uint8_t tid,
			     tAniTrafStrmMetrics *tsm_metrics)
{
	struct hdd_context *hdd_ctx;
	struct hdd_station_ctx *hdd_sta_ctx;
	QDF_STATUS status;
	int ret;
	void *cookie;
	struct osif_request *request;
	struct tsm_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_STATS,
	};

	if (NULL == adapter) {
		hdd_err("adapter is NULL");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);

	status = sme_get_tsm_stats(hdd_ctx->mac_handle, hdd_get_tsm_stats_cb,
				   hdd_sta_ctx->conn_info.staId[0],
				   hdd_sta_ctx->conn_info.bssId,
				   cookie, tid);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Unable to retrieve tsm statistics");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("SME timed out while retrieving tsm statistics");
		goto cleanup;
	}

	priv = osif_request_priv(request);
	*tsm_metrics = priv->tsm_metrics;

 cleanup:
	osif_request_put(request);

	return ret;
}
#endif /*FEATURE_WLAN_ESE */

/* Function header is left blank intentionally */
static int hdd_parse_setrmcenable_command(uint8_t *pValue,
					  uint8_t *pRmcEnable)
{
	uint8_t *inPtr = pValue;
	int tempInt;
	int v = 0;
	char buf[32];
	*pRmcEnable = 0;

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

	if (NULL == inPtr)
		return 0;
	else if (SPACE_ASCII_VALUE != *inPtr)
		return 0;

	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	if ('\0' == *inPtr)
		return 0;

	v = sscanf(inPtr, "%31s ", buf);
	if (1 != v)
		return -EINVAL;

	v = kstrtos32(buf, 10, &tempInt);
	if (v < 0)
		return -EINVAL;

	*pRmcEnable = tempInt;

	hdd_debug("ucRmcEnable: %d", *pRmcEnable);

	return 0;
}

/* Function header is left blank intentionally */
static int hdd_parse_setrmcactionperiod_command(uint8_t *pValue,
						uint32_t *pActionPeriod)
{
	uint8_t *inPtr = pValue;
	int tempInt;
	int v = 0;
	char buf[32];
	*pActionPeriod = 0;

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

	if (NULL == inPtr)
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *inPtr)
		return -EINVAL;

	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	if ('\0' == *inPtr)
		return 0;

	v = sscanf(inPtr, "%31s ", buf);
	if (1 != v)
		return -EINVAL;

	v = kstrtos32(buf, 10, &tempInt);
	if (v < 0)
		return -EINVAL;

	if ((tempInt < WNI_CFG_RMC_ACTION_PERIOD_FREQUENCY_STAMIN) ||
	    (tempInt > WNI_CFG_RMC_ACTION_PERIOD_FREQUENCY_STAMAX))
		return -EINVAL;

	*pActionPeriod = tempInt;

	hdd_debug("uActionPeriod: %d", *pActionPeriod);

	return 0;
}

/* Function header is left blank intentionally */
static int hdd_parse_setrmcrate_command(uint8_t *pValue,
					uint32_t *pRate,
					enum tx_rate_info *pTxFlags)
{
	uint8_t *inPtr = pValue;
	int tempInt;
	int v = 0;
	char buf[32];
	*pRate = 0;
	*pTxFlags = 0;

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);

	if (NULL == inPtr)
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *inPtr)
		return -EINVAL;

	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	if ('\0' == *inPtr)
		return 0;

	v = sscanf(inPtr, "%31s ", buf);
	if (1 != v)
		return -EINVAL;

	v = kstrtos32(buf, 10, &tempInt);
	if (v < 0)
		return -EINVAL;

	switch (tempInt) {
	default:
		hdd_warn("Unsupported rate: %d", tempInt);
		return -EINVAL;
	case 0:
	case 6:
	case 9:
	case 12:
	case 18:
	case 24:
	case 36:
	case 48:
	case 54:
		*pTxFlags = TX_RATE_LEGACY;
		*pRate = tempInt * 10;
		break;
	case 65:
		*pTxFlags = TX_RATE_HT20;
		*pRate = tempInt * 10;
		break;
	case 72:
		*pTxFlags = TX_RATE_HT20 | TX_RATE_SGI;
		*pRate = 722;
		break;
	}

	hdd_debug("Rate: %d", *pRate);

	return 0;
}

/**
 * hdd_get_ibss_peer_info_cb() - IBSS peer Info request callback
 * @UserData: Adapter private data
 * @pPeerInfoRsp: Peer info response
 *
 * This is an asynchronous callback function from SME when the peer info
 * is received
 *
 * Return: 0 for success non-zero for failure
 */
void
hdd_get_ibss_peer_info_cb(void *pUserData,
				tSirPeerInfoRspParams *pPeerInfo)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) pUserData;
	struct hdd_station_ctx *pStaCtx;
	uint8_t i;

	if ((NULL == adapter) ||
	    (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		hdd_err("invalid adapter or adapter has invalid magic");
		return;
	}

	pStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (NULL != pPeerInfo && QDF_STATUS_SUCCESS == pPeerInfo->status) {
		/* validate number of peers */
		if (pPeerInfo->numPeers > SIR_MAX_NUM_STA_IN_IBSS) {
			hdd_warn("Limiting num_peers %u to %u",
				pPeerInfo->numPeers, SIR_MAX_NUM_STA_IN_IBSS);
			pPeerInfo->numPeers = SIR_MAX_NUM_STA_IN_IBSS;
		}
		pStaCtx->ibss_peer_info.status = pPeerInfo->status;
		pStaCtx->ibss_peer_info.numPeers = pPeerInfo->numPeers;

		for (i = 0; i < pPeerInfo->numPeers; i++)
			pStaCtx->ibss_peer_info.peerInfoParams[i] =
				pPeerInfo->peerInfoParams[i];
	} else {
		hdd_debug("peerInfo %s: status %u, numPeers %u",
			pPeerInfo ? "valid" : "null",
			pPeerInfo ? pPeerInfo->status : QDF_STATUS_E_FAILURE,
			pPeerInfo ? pPeerInfo->numPeers : 0);
		pStaCtx->ibss_peer_info.numPeers = 0;
		pStaCtx->ibss_peer_info.status = QDF_STATUS_E_FAILURE;
	}

	complete(&adapter->ibss_peer_info_comp);
}

/**
 * hdd_cfg80211_get_ibss_peer_info_all() - get ibss peers' info
 * @adapter:	Adapter context
 *
 * Request function to get IBSS peer info from lower layers
 *
 * Return: 0 for success non-zero for failure
 */
static
QDF_STATUS hdd_cfg80211_get_ibss_peer_info_all(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	unsigned long rc;

	INIT_COMPLETION(adapter->ibss_peer_info_comp);

	status = sme_request_ibss_peer_info(adapter->hdd_ctx->mac_handle,
					    adapter,
					    hdd_get_ibss_peer_info_cb,
					    true, 0xFF);

	if (QDF_STATUS_SUCCESS == status) {
		rc = wait_for_completion_timeout
			     (&adapter->ibss_peer_info_comp,
			     msecs_to_jiffies(IBSS_PEER_INFO_REQ_TIMOEUT));

		/* status will be 0 if timed out */
		if (!rc) {
			hdd_warn("Warning: IBSS_PEER_INFO_TIMEOUT");
			status = QDF_STATUS_E_FAILURE;
		}
	} else {
		hdd_warn("Warning: sme_request_ibss_peer_info Request failed");
	}

	return status;
}

/**
 * hdd_cfg80211_get_ibss_peer_info() - get ibss peer info
 * @adapter:	Adapter context
 * @staIdx:	Sta index for which the peer info is requested
 *
 * Request function to get IBSS peer info from lower layers
 *
 * Return: 0 for success non-zero for failure
 */
static QDF_STATUS
hdd_cfg80211_get_ibss_peer_info(struct hdd_adapter *adapter, uint8_t staIdx)
{
	unsigned long rc;
	QDF_STATUS status;

	INIT_COMPLETION(adapter->ibss_peer_info_comp);

	status = sme_request_ibss_peer_info(adapter->hdd_ctx->mac_handle,
					    adapter,
					    hdd_get_ibss_peer_info_cb,
					    false, staIdx);

	if (QDF_STATUS_SUCCESS == status) {
		rc = wait_for_completion_timeout(
				&adapter->ibss_peer_info_comp,
				msecs_to_jiffies(IBSS_PEER_INFO_REQ_TIMOEUT));

		/* status = 0 on timeout */
		if (!rc) {
			hdd_warn("Warning: IBSS_PEER_INFO_TIMEOUT");
			status = QDF_STATUS_E_FAILURE;
		}
	} else {
		hdd_warn("Warning: sme_request_ibss_peer_info Request failed");
	}

	return status;
}

/* Function header is left blank intentionally */
static QDF_STATUS
hdd_parse_get_ibss_peer_info(uint8_t *pValue, struct qdf_mac_addr *pPeerMacAddr)
{
	uint8_t *inPtr = pValue;
	size_t in_ptr_len = strlen(pValue);

	inPtr = strnchr(pValue, in_ptr_len, SPACE_ASCII_VALUE);

	if (NULL == inPtr)
		return QDF_STATUS_E_FAILURE;
	else if (SPACE_ASCII_VALUE != *inPtr)
		return QDF_STATUS_E_FAILURE;

	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	if ('\0' == *inPtr)
		return QDF_STATUS_E_FAILURE;

	in_ptr_len -= (inPtr - pValue);
	if (in_ptr_len < 17)
		return QDF_STATUS_E_FAILURE;

	if (inPtr[2] != ':' || inPtr[5] != ':' || inPtr[8] != ':' ||
	    inPtr[11] != ':' || inPtr[14] != ':')
		return QDF_STATUS_E_FAILURE;

	sscanf(inPtr, "%2x:%2x:%2x:%2x:%2x:%2x",
	       (unsigned int *)&pPeerMacAddr->bytes[0],
	       (unsigned int *)&pPeerMacAddr->bytes[1],
	       (unsigned int *)&pPeerMacAddr->bytes[2],
	       (unsigned int *)&pPeerMacAddr->bytes[3],
	       (unsigned int *)&pPeerMacAddr->bytes[4],
	       (unsigned int *)&pPeerMacAddr->bytes[5]);

	return QDF_STATUS_SUCCESS;
}

static void hdd_get_band_helper(struct hdd_context *hdd_ctx, int *ui_band)
{
	enum band_info band = -1;

	sme_get_freq_band(hdd_ctx->mac_handle, &band);
	switch (band) {
	case BAND_ALL:
		*ui_band = WLAN_HDD_UI_BAND_AUTO;
		break;

	case BAND_2G:
		*ui_band = WLAN_HDD_UI_BAND_2_4_GHZ;
		break;

	case BAND_5G:
		*ui_band = WLAN_HDD_UI_BAND_5_GHZ;
		break;

	default:
		hdd_warn("Invalid Band %d", band);
		*ui_band = -1;
		break;
	}
}

/**
 * _hdd_parse_bssid_and_chan() - helper function to parse bssid and channel
 * @data:            input data
 * @target_ap_bssid: pointer to bssid (output parameter)
 * @channel:         pointer to channel (output parameter)
 *
 * Return: 0 if parsing is successful; -EINVAL otherwise
 */
static int _hdd_parse_bssid_and_chan(const uint8_t **data,
				     uint8_t *bssid,
				     uint8_t *channel)
{
	const uint8_t *in_ptr;
	int            v = 0;
	int            temp_int;
	uint8_t        temp_buf[32];

	/* 12 hexa decimal digits, 5 ':' and '\0' */
	uint8_t        mac_addr[18];

	if (!data || !*data)
		return -EINVAL;

	in_ptr = *data;

	in_ptr = strnchr(in_ptr, strlen(in_ptr), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == in_ptr)
		goto error;
	/* no space after the command */
	else if (SPACE_ASCII_VALUE != *in_ptr)
		goto error;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *in_ptr) && ('\0' != *in_ptr))
		in_ptr++;

	/* no argument followed by spaces */
	if ('\0' == *in_ptr)
		goto error;

	v = sscanf(in_ptr, "%17s", mac_addr);
	if (!((1 == v) && hdd_is_valid_mac_address(mac_addr))) {
		hdd_err("Invalid MAC address or All hex inputs are not read (%d)",
			 v);
		goto error;
	}

	bssid[0] = hex_to_bin(mac_addr[0]) << 4 |
			hex_to_bin(mac_addr[1]);
	bssid[1] = hex_to_bin(mac_addr[3]) << 4 |
			hex_to_bin(mac_addr[4]);
	bssid[2] = hex_to_bin(mac_addr[6]) << 4 |
			hex_to_bin(mac_addr[7]);
	bssid[3] = hex_to_bin(mac_addr[9]) << 4 |
			hex_to_bin(mac_addr[10]);
	bssid[4] = hex_to_bin(mac_addr[12]) << 4 |
			hex_to_bin(mac_addr[13]);
	bssid[5] = hex_to_bin(mac_addr[15]) << 4 |
			hex_to_bin(mac_addr[16]);

	/* point to the next argument */
	in_ptr = strnchr(in_ptr, strlen(in_ptr), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == in_ptr)
		goto error;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *in_ptr) && ('\0' != *in_ptr))
		in_ptr++;

	/* no argument followed by spaces */
	if ('\0' == *in_ptr)
		goto error;

	/* get the next argument ie the channel number */
	v = sscanf(in_ptr, "%31s ", temp_buf);
	if (1 != v)
		goto error;

	v = kstrtos32(temp_buf, 10, &temp_int);
	if ((v < 0) || (temp_int < 0) ||
	    (temp_int > WNI_CFG_CURRENT_CHANNEL_STAMAX))
		return -EINVAL;

	*channel = temp_int;
	*data = in_ptr;
	return 0;
error:
	*data = in_ptr;
	return -EINVAL;
}

/**
 * hdd_parse_send_action_frame_data() - HDD Parse send action frame data
 * @pValue:         Pointer to input data
 * @pTargetApBssid: Pointer to target Ap bssid
 * @pChannel:       Pointer to the Target AP channel
 * @pDwellTime:     Pointer to the time to stay off-channel
 *                  after transmitting action frame
 * @pBuf:           Pointer to data
 * @pBufLen:        Pointer to data length
 *
 * This function parses the send action frame data passed in the format
 * SENDACTIONFRAME<space><bssid><space><channel><space><dwelltime><space><data>
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_send_action_frame_v1_data(const uint8_t *pValue,
				    uint8_t *pTargetApBssid,
				    uint8_t *pChannel, uint8_t *pDwellTime,
				    uint8_t **pBuf, uint8_t *pBufLen)
{
	const uint8_t *inPtr = pValue;
	const uint8_t *dataEnd;
	int tempInt;
	int j = 0;
	int i = 0;
	int v = 0;
	uint8_t tempBuf[32];
	uint8_t tempByte = 0;

	if (_hdd_parse_bssid_and_chan(&inPtr, pTargetApBssid, pChannel))
		return -EINVAL;

	/* point to the next argument */
	inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == inPtr)
		return -EINVAL;
	/* removing empty spaces */
	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	/* no argument followed by spaces */
	if ('\0' == *inPtr)
		return -EINVAL;

	/* getting the next argument ie the dwell time */
	v = sscanf(inPtr, "%31s ", tempBuf);
	if (1 != v)
		return -EINVAL;

	v = kstrtos32(tempBuf, 10, &tempInt);
	if (v < 0 || tempInt < 0)
		return -EINVAL;

	*pDwellTime = tempInt;

	/* point to the next argument */
	inPtr = strnchr(inPtr, strlen(inPtr), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == inPtr)
		return -EINVAL;
	/* removing empty spaces */
	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	/* no argument followed by spaces */
	if ('\0' == *inPtr)
		return -EINVAL;

	/* find the length of data */
	dataEnd = inPtr;
	while (('\0' != *dataEnd))
		dataEnd++;

	*pBufLen = dataEnd - inPtr;
	if (*pBufLen <= 0)
		return -EINVAL;

	/*
	 * Allocate the number of bytes based on the number of input characters
	 * whether it is even or odd.
	 * if the number of input characters are even, then we need N/2 byte.
	 * if the number of input characters are odd, then we need do (N+1)/2
	 * to compensate rounding off.
	 * For example, if N = 18, then (18 + 1)/2 = 9 bytes are enough.
	 * If N = 19, then we need 10 bytes, hence (19 + 1)/2 = 10 bytes
	 */
	*pBuf = qdf_mem_malloc((*pBufLen + 1) / 2);
	if (NULL == *pBuf) {
		hdd_err("qdf_mem_malloc failed");
		return -ENOMEM;
	}

	/* the buffer received from the upper layer is character buffer,
	 * we need to prepare the buffer taking 2 characters in to a U8 hex
	 * decimal number for example 7f0000f0...form a buffer to contain 7f
	 * in 0th location, 00 in 1st and f0 in 3rd location
	 */
	for (i = 0, j = 0; j < *pBufLen; j += 2) {
		if (j + 1 == *pBufLen) {
			tempByte = hex_to_bin(inPtr[j]);
		} else {
			tempByte =
				(hex_to_bin(inPtr[j]) << 4) |
				(hex_to_bin(inPtr[j + 1]));
		}
		(*pBuf)[i++] = tempByte;
	}
	*pBufLen = i;
	return 0;
}

/**
 * hdd_parse_reassoc_command_data() - HDD Parse reassoc command data
 * @pValue:         Pointer to input data (its a NULL terminated string)
 * @pTargetApBssid: Pointer to target Ap bssid
 * @pChannel:       Pointer to the Target AP channel
 *
 * This function parses the reasoc command data passed in the format
 * REASSOC<space><bssid><space><channel>
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_reassoc_command_v1_data(const uint8_t *pValue,
					     uint8_t *pTargetApBssid,
					     uint8_t *pChannel)
{
	const uint8_t *inPtr = pValue;

	if (_hdd_parse_bssid_and_chan(&inPtr, pTargetApBssid, pChannel))
		return -EINVAL;

	return 0;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS hdd_wma_send_fastreassoc_cmd(struct hdd_adapter *adapter,
					const tSirMacAddr bssid,
					int channel)
{
	struct hdd_station_ctx *hdd_sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct csr_roam_profile *roam_profile;
	tSirMacAddr connected_bssid;

	roam_profile = hdd_roam_profile(adapter);
	qdf_mem_copy(connected_bssid, hdd_sta_ctx->conn_info.bssId.bytes,
		     ETH_ALEN);
	return sme_fast_reassoc(adapter->hdd_ctx->mac_handle, roam_profile,
				bssid, channel, adapter->session_id,
				connected_bssid);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
/**
 * hdd_is_fast_reassoc_allowed  - check if roam offload is enabled on the given
 * vdev else, don't allow roam invoke to be triggered.
 * @mac_handle: Opaque mac handle
 * @vdev_id: vdev_id
 *
 * This API should return true if kernel version is less than 4.9, because
 * the earlier versions don't have the fix to handle reassociation failure.
 *
 * Return: true if roaming module initialization is done else false
 */
static bool
hdd_is_fast_reassoc_allowed(mac_handle_t mac_handle, uint8_t vdev_id)
{
	return sme_is_fast_reassoc_allowed(mac_handle, vdev_id);
}
#else
static inline bool
hdd_is_fast_reassoc_allowed(mac_handle_t mac_handle, uint8_t vdev_id)
{
	return true;
}
#endif

/**
 * hdd_reassoc() - perform a userspace-directed reassoc
 * @adapter:    Adapter upon which the command was received
 * @bssid:      BSSID with which to reassociate
 * @channel:    channel upon which to reassociate
 * @src:        The source for the trigger of this action
 *
 * This function performs a userspace-directed reassoc operation
 *
 * Return: 0 for success non-zero for failure
 */
int hdd_reassoc(struct hdd_adapter *adapter, const uint8_t *bssid,
		uint8_t channel, const handoff_src src)
{
	struct hdd_station_ctx *sta_ctx;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret = 0;
	QDF_STATUS status;

	if (hdd_ctx == NULL) {
		hdd_err("Invalid hdd ctx");
		return -EINVAL;
	}

	if (QDF_STA_MODE != adapter->device_mode) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	/*
	 * pHddStaCtx->conn_info.connState is set to disconnected only
	 * after the disconnect done indication from SME. If the SME is
	 * in the process of disconnecting, the SME Connection state is
	 * set to disconnected and the pHddStaCtx->conn_info.connState
	 * will still be associated till the disconnect is done.
	 * So check both the HDD state and SME state here.
	 * If not associated, no need to proceed with reassoc
	 */
	if ((eConnectionState_Associated != sta_ctx->conn_info.connState) ||
	    (!sme_is_conn_state_connected(hdd_ctx->mac_handle,
	    adapter->session_id))) {
		hdd_warn("Not associated");
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * if the target bssid is same as currently associated AP,
	 * use the current connections's channel.
	 */
	if (!memcmp(bssid, sta_ctx->conn_info.bssId.bytes,
			QDF_MAC_ADDR_SIZE)) {
		hdd_warn("Reassoc BSSID is same as currently associated AP bssid");
		channel = sta_ctx->conn_info.operationChannel;
	}

	/* Check channel number is a valid channel number */
	if (QDF_STATUS_SUCCESS !=
	    wlan_hdd_validate_operation_channel(adapter, channel)) {
		hdd_err("Invalid Channel: %d", channel);
		ret = -EINVAL;
		goto exit;
	}

	/* Proceed with reassoc */
	if (roaming_offload_enabled(hdd_ctx)) {
		if (!hdd_is_fast_reassoc_allowed(adapter->hdd_ctx->mac_handle,
						 adapter->session_id)) {
			hdd_err("LFR3: vdev[%d] RSO is not enabled",
				adapter->session_id);
			ret = -EPERM;
			goto exit;
		}
		status = hdd_wma_send_fastreassoc_cmd(adapter,
						      bssid, (int)channel);
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("Failed to send fast reassoc cmd");
			ret = -EINVAL;
		}
	} else {
		tCsrHandoffRequest handoff;

		handoff.channel = channel;
		handoff.src = src;
		qdf_mem_copy(handoff.bssid.bytes, bssid, QDF_MAC_ADDR_SIZE);
		sme_handoff_request(hdd_ctx->mac_handle, adapter->session_id,
				    &handoff);
	}
exit:
	return ret;
}

/**
 * hdd_parse_reassoc_v1() - parse version 1 of the REASSOC command
 * @adapter:	Adapter upon which the command was received
 * @command:	ASCII text command that was received
 *
 * This function parses the v1 REASSOC command with the format
 *
 *    REASSOC xx:xx:xx:xx:xx:xx CH
 *
 * Where "xx:xx:xx:xx:xx:xx" is the Hex-ASCII representation of the
 * BSSID and CH is the ASCII representation of the channel.  For
 * example
 *
 *    REASSOC 00:0a:0b:11:22:33 48
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_reassoc_v1(struct hdd_adapter *adapter, const char *command)
{
	uint8_t channel = 0;
	tSirMacAddr bssid;
	int ret;

	ret = hdd_parse_reassoc_command_v1_data(command, bssid, &channel);
	if (ret)
		hdd_err("Failed to parse reassoc command data");
	else
		ret = hdd_reassoc(adapter, bssid, channel, REASSOC);

	return ret;
}

/**
 * hdd_parse_reassoc_v2() - parse version 2 of the REASSOC command
 * @adapter:	Adapter upon which the command was received
 * @command:	Command that was received, ASCII command
 *		followed by binary data
 * @total_len:  Total length of the command received
 *
 * This function parses the v2 REASSOC command with the format
 *
 *    REASSOC <android_wifi_reassoc_params>
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_reassoc_v2(struct hdd_adapter *adapter,
				const char *command,
				int total_len)
{
	struct android_wifi_reassoc_params params;
	tSirMacAddr bssid;
	int ret;

	if (total_len < sizeof(params) + 8) {
		hdd_err("Invalid command length");
		return -EINVAL;
	}

	/* The params are located after "REASSOC " */
	memcpy(&params, command + 8, sizeof(params));

	if (!mac_pton(params.bssid, (u8 *) &bssid)) {
		hdd_err("MAC address parsing failed");
		ret = -EINVAL;
	} else {
		ret = hdd_reassoc(adapter, bssid, params.channel, REASSOC);
	}
	return ret;
}

/**
 * hdd_parse_reassoc() - parse the REASSOC command
 * @adapter:	Adapter upon which the command was received
 * @command:	Command that was received
 * @total_len:  Total length of the command received
 *
 * There are two different versions of the REASSOC command.  Version 1
 * of the command contains a parameter list that is ASCII characters
 * whereas version 2 contains a combination of ASCII and binary
 * payload.  Determine if a version 1 or a version 2 command is being
 * parsed by examining the parameters, and then dispatch the parser
 * that is appropriate for the command.
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_reassoc(struct hdd_adapter *adapter, const char *command,
			     int total_len)
{
	int ret;

	/* both versions start with "REASSOC "
	 * v1 has a bssid and channel # as an ASCII string
	 *    REASSOC xx:xx:xx:xx:xx:xx CH
	 * v2 has a C struct
	 *    REASSOC <binary c struct>
	 *
	 * The first field in the v2 struct is also the bssid in ASCII.
	 * But in the case of a v2 message the BSSID is NUL-terminated.
	 * Hence we can peek at that offset to see if this is V1 or V2
	 * REASSOC xx:xx:xx:xx:xx:xx*
	 *           1111111111222222
	 * 01234567890123456789012345
	 */

	if (total_len < 26) {
		hdd_err("Invalid command, total_len = %d", total_len);
		return -EINVAL;
	}

	if (command[25])
		ret = hdd_parse_reassoc_v1(adapter, command);
	else
		ret = hdd_parse_reassoc_v2(adapter, command, total_len);

	return ret;
}

/**
 * hdd_sendactionframe() - send a userspace-supplied action frame
 * @adapter:	Adapter upon which the command was received
 * @bssid:	BSSID target of the action frame
 * @channel:	Channel upon which to send the frame
 * @dwell_time:	Amount of time to dwell when the frame is sent
 * @payload_len:Length of the payload
 * @payload:	Payload of the frame
 *
 * This function sends a userspace-supplied action frame
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_sendactionframe(struct hdd_adapter *adapter, const uint8_t *bssid,
		    const uint8_t channel, const uint8_t dwell_time,
		    const int payload_len, const uint8_t *payload)
{
	struct ieee80211_channel chan;
	int frame_len, ret = 0;
	uint8_t *frame;
	struct ieee80211_hdr_3addr *hdr;
	u64 cookie;
	struct hdd_station_ctx *sta_ctx;
	struct hdd_context *hdd_ctx;
	tpSirMacVendorSpecificFrameHdr pVendorSpecific =
		(tpSirMacVendorSpecificFrameHdr) payload;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	struct cfg80211_mgmt_tx_params params;
#endif

	if (QDF_STA_MODE != adapter->device_mode) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	/* if not associated, no need to send action frame */
	if (eConnectionState_Associated != sta_ctx->conn_info.connState) {
		hdd_warn("Not associated");
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * if the target bssid is different from currently associated AP,
	 * then no need to send action frame
	 */
	if (memcmp(bssid, sta_ctx->conn_info.bssId.bytes,
			QDF_MAC_ADDR_SIZE)) {
		hdd_warn("STA is not associated to this AP");
		ret = -EINVAL;
		goto exit;
	}

	chan.center_freq = sme_chn_to_freq(channel);
	/* Check if it is specific action frame */
	if (pVendorSpecific->category ==
	    SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY) {
		static const uint8_t Oui[] = { 0x00, 0x00, 0xf0 };

		if (!qdf_mem_cmp(pVendorSpecific->Oui, (void *)Oui, 3)) {
			/*
			 * if the channel number is different from operating
			 * channel then no need to send action frame
			 */
			if (channel != 0) {
				if (channel !=
				    sta_ctx->conn_info.operationChannel) {
					hdd_warn("channel(%d) is different from operating channel(%d)",
						  channel,
						  sta_ctx->conn_info.
						  operationChannel);
					ret = -EINVAL;
					goto exit;
				}
				/*
				 * If channel number is specified and same
				 * as home channel, ensure that action frame
				 * is sent immediately by cancelling
				 * roaming scans. Otherwise large dwell times
				 * may cause long delays in sending action
				 * frames.
				 */
				sme_abort_roam_scan(hdd_ctx->mac_handle,
						    adapter->session_id);
			} else {
				/*
				 * 0 is accepted as current home channel,
				 * delayed transmission of action frame is ok.
				 */
				chan.center_freq =
					sme_chn_to_freq(sta_ctx->conn_info.
							operationChannel);
			}
		}
	}
	if (chan.center_freq == 0) {
		hdd_err("Invalid channel number: %d", channel);
		ret = -EINVAL;
		goto exit;
	}

	frame_len = payload_len + 24;
	frame = qdf_mem_malloc(frame_len);
	if (!frame) {
		hdd_err("memory allocation failed");
		ret = -ENOMEM;
		goto exit;
	}

	hdr = (struct ieee80211_hdr_3addr *)frame;
	hdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION);
	qdf_mem_copy(hdr->addr1, bssid, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr->addr2, adapter->mac_addr.bytes,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr->addr3, bssid, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr + 1, payload, payload_len);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	params.chan = &chan;
	params.offchan = 0;
	params.wait = dwell_time;
	params.buf = frame;
	params.len = frame_len;
	params.no_cck = 1;
	params.dont_wait_for_ack = 1;
	ret = wlan_hdd_mgmt_tx(NULL, &adapter->wdev, &params, &cookie);
#else
	ret = wlan_hdd_mgmt_tx(NULL,
			       &(adapter->wdev),
			       &chan, 0,

			       dwell_time, frame, frame_len, 1, 1, &cookie);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */

	qdf_mem_free(frame);
exit:
	return ret;
}

/**
 * hdd_parse_sendactionframe_v1() - parse version 1 of the
 *       SENDACTIONFRAME command
 * @adapter:	Adapter upon which the command was received
 * @command:	ASCII text command that was received
 *
 * This function parses the v1 SENDACTIONFRAME command with the format
 *
 *    SENDACTIONFRAME xx:xx:xx:xx:xx:xx CH DW xxxxxx
 *
 * Where "xx:xx:xx:xx:xx:xx" is the Hex-ASCII representation of the
 * BSSID, CH is the ASCII representation of the channel, DW is the
 * ASCII representation of the dwell time, and xxxxxx is the Hex-ASCII
 * payload.  For example
 *
 *    SENDACTIONFRAME 00:0a:0b:11:22:33 48 40 aabbccddee
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_sendactionframe_v1(struct hdd_adapter *adapter, const char *command)
{
	uint8_t channel = 0;
	uint8_t dwell_time = 0;
	uint8_t payload_len = 0;
	uint8_t *payload = NULL;
	tSirMacAddr bssid;
	int ret;

	ret = hdd_parse_send_action_frame_v1_data(command, bssid, &channel,
						  &dwell_time, &payload,
						  &payload_len);
	if (ret) {
		hdd_err("Failed to parse send action frame data");
	} else {
		ret = hdd_sendactionframe(adapter, bssid, channel,
					  dwell_time, payload_len, payload);
		qdf_mem_free(payload);
	}

	return ret;
}

/**
 * hdd_parse_sendactionframe_v2() - parse version 2 of the
 *       SENDACTIONFRAME command
 * @adapter:	Adapter upon which the command was received
 * @command:	Command that was received, ASCII command
 *		followed by binary data
 *
 * This function parses the v2 SENDACTIONFRAME command with the format
 *
 *    SENDACTIONFRAME <android_wifi_af_params>
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_sendactionframe_v2(struct hdd_adapter *adapter,
			     const char *command, int total_len)
{
	struct android_wifi_af_params *params;
	tSirMacAddr bssid;
	int ret;
	int len_wo_payload = 0;

	/* The params are located after "SENDACTIONFRAME " */
	total_len -= 16;
	len_wo_payload = sizeof(*params) - ANDROID_WIFI_ACTION_FRAME_SIZE;
	if (total_len <= len_wo_payload) {
		hdd_err("Invalid command len");
		return -EINVAL;
	}

	params = (struct android_wifi_af_params *)(command + 16);

	if (params->len <= 0 || params->len > ANDROID_WIFI_ACTION_FRAME_SIZE ||
		(params->len > (total_len - len_wo_payload))) {
		hdd_err("Invalid payload length: %d", params->len);
		return -EINVAL;
	}

	if (!mac_pton(params->bssid, (u8 *)&bssid)) {
		hdd_err("MAC address parsing failed");
		return -EINVAL;
	}

	if (params->channel < 0 ||
	    params->channel > WNI_CFG_CURRENT_CHANNEL_STAMAX) {
		hdd_err("Invalid channel: %d", params->channel);
		return -EINVAL;
	}

	if (params->dwell_time < 0) {
		hdd_err("Invalid dwell_time: %d", params->dwell_time);
		return -EINVAL;
	}

	ret = hdd_sendactionframe(adapter, bssid, params->channel,
				params->dwell_time, params->len, params->data);

	return ret;
}

/**
 * hdd_parse_sendactionframe() - parse the SENDACTIONFRAME command
 * @adapter:	Adapter upon which the command was received
 * @command:	Command that was received
 *
 * There are two different versions of the SENDACTIONFRAME command.
 * Version 1 of the command contains a parameter list that is ASCII
 * characters whereas version 2 contains a combination of ASCII and
 * binary payload.  Determine if a version 1 or a version 2 command is
 * being parsed by examining the parameters, and then dispatch the
 * parser that is appropriate for the version of the command.
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_sendactionframe(struct hdd_adapter *adapter, const char *command,
			  int total_len)
{
	int ret;

	/*
	 * both versions start with "SENDACTIONFRAME "
	 * v1 has a bssid and other parameters as an ASCII string
	 *    SENDACTIONFRAME xx:xx:xx:xx:xx:xx CH DWELL LEN FRAME
	 * v2 has a C struct
	 *    SENDACTIONFRAME <binary c struct>
	 *
	 * The first field in the v2 struct is also the bssid in ASCII.
	 * But in the case of a v2 message the BSSID is NUL-terminated.
	 * Hence we can peek at that offset to see if this is V1 or V2
	 * SENDACTIONFRAME xx:xx:xx:xx:xx:xx*
	 *           111111111122222222223333
	 * 0123456789012345678901234567890123
	 * For both the commands, a valid command must have atleast
	 * first 34 length of data.
	 */
	if (total_len < 34) {
		hdd_err("Invalid command (total_len=%d)", total_len);
		return -EINVAL;
	}

	if (command[33])
		ret = hdd_parse_sendactionframe_v1(adapter, command);
	else
		ret = hdd_parse_sendactionframe_v2(adapter, command, total_len);

	return ret;
}

/**
 * hdd_parse_channellist() - HDD Parse channel list
 * @pValue:		Pointer to input channel list
 * @ChannelList:	Pointer to local output array to record
 *			channel list
 * @pNumChannels:	Pointer to number of roam scan channels
 *
 * This function parses the channel list passed in the format
 * SETROAMSCANCHANNELS<space><Number of channels><space>Channel 1<space>
 * Channel 2<space>Channel N
 * if the Number of channels (N) does not match with the actual number
 * of channels passed then take the minimum of N and count of
 * (Ch1, Ch2, ...Ch M). For example, if SETROAMSCANCHANNELS 3 36 40 44 48,
 * only 36, 40 and 44 shall be taken. If SETROAMSCANCHANNELS 5 36 40 44 48,
 * ignore 5 and take 36, 40, 44 and 48. This function does not take care of
 * removing duplicate channels from the list
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_channellist(const uint8_t *pValue, uint8_t *pChannelList,
		      uint8_t *pNumChannels)
{
	const uint8_t *inPtr = pValue;
	int tempInt;
	int j = 0;
	int v = 0;
	char buf[32];

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == inPtr)
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *inPtr) /* no space after the command */
		return -EINVAL;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	/* no argument followed by spaces */
	if ('\0' == *inPtr)
		return -EINVAL;

	/* get the first argument ie the number of channels */
	v = sscanf(inPtr, "%31s ", buf);
	if (1 != v)
		return -EINVAL;

	v = kstrtos32(buf, 10, &tempInt);
	if ((v < 0) ||
	    (tempInt <= 0) || (tempInt > WNI_CFG_VALID_CHANNEL_LIST_LEN))
		return -EINVAL;

	*pNumChannels = tempInt;

	hdd_debug("Number of channels are: %d", *pNumChannels);

	for (j = 0; j < (*pNumChannels); j++) {
		/*
		 * inPtr pointing to the beginning of first space after number
		 * of channels
		 */
		inPtr = strpbrk(inPtr, " ");
		/* no channel list after the number of channels argument */
		if (NULL == inPtr) {
			if ((j != 0) && (j == *pNumChannels))
				return 0;
			else
				goto cnt_mismatch;
		}

		/* remove empty space */
		while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
			inPtr++;

		/*
		 * no channel list after the number of channels
		 * argument and spaces
		 */
		if ('\0' == *inPtr) {
			if ((j != 0) && (j == *pNumChannels))
				return 0;
			else
				goto cnt_mismatch;
		}

		v = sscanf(inPtr, "%31s ", buf);
		if (1 != v)
			return -EINVAL;

		v = kstrtos32(buf, 10, &tempInt);
		if ((v < 0) ||
		    (tempInt <= 0) ||
		    (tempInt > WNI_CFG_CURRENT_CHANNEL_STAMAX)) {
			return -EINVAL;
		}
		pChannelList[j] = tempInt;

		hdd_debug("Channel %d added to preferred channel list",
			  pChannelList[j]);
	}

	return 0;

cnt_mismatch:
	hdd_debug("Mismatch in ch cnt: %d and num of ch: %d", *pNumChannels, j);
	*pNumChannels = 0;
	return -EINVAL;
}

/**
 * hdd_parse_set_roam_scan_channels_v1() - parse version 1 of the
 * SETROAMSCANCHANNELS command
 * @adapter:	Adapter upon which the command was received
 * @command:	ASCII text command that was received
 *
 * This function parses the v1 SETROAMSCANCHANNELS command with the format
 *
 *    SETROAMSCANCHANNELS N C1 C2 ... Cn
 *
 * Where "N" is the ASCII representation of the number of channels and
 * C1 thru Cn is the ASCII representation of the channels.  For example
 *
 *    SETROAMSCANCHANNELS 4 36 40 44 48
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_set_roam_scan_channels_v1(struct hdd_adapter *adapter,
				    const char *command)
{
	uint8_t channel_list[WNI_CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t num_chan = 0;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;
	mac_handle_t mac_handle;

	ret = hdd_parse_channellist(command, channel_list, &num_chan);
	if (ret) {
		hdd_err("Failed to parse channel list information");
		goto exit;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_SETROAMSCANCHANNELS_IOCTL,
		   adapter->session_id, num_chan);

	if (num_chan > WNI_CFG_VALID_CHANNEL_LIST_LEN) {
		hdd_err("number of channels (%d) supported exceeded max (%d)",
			 num_chan, WNI_CFG_VALID_CHANNEL_LIST_LEN);
		ret = -EINVAL;
		goto exit;
	}

	mac_handle = hdd_ctx->mac_handle;
	if (!sme_validate_channel_list(mac_handle, channel_list, num_chan)) {
		hdd_err("List contains invalid channel(s)");
		ret = -EINVAL;
		goto exit;
	}

	status = sme_change_roam_scan_channel_list(mac_handle,
						   adapter->session_id,
						   channel_list, num_chan);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Failed to update channel list information");
		ret = -EINVAL;
		goto exit;
	}
exit:
	return ret;
}

/**
 * hdd_parse_set_roam_scan_channels_v2() - parse version 2 of the
 * SETROAMSCANCHANNELS command
 * @adapter:	Adapter upon which the command was received
 * @command:	Command that was received, ASCII command
 *		followed by binary data
 *
 * This function parses the v2 SETROAMSCANCHANNELS command with the format
 *
 *    SETROAMSCANCHANNELS [N][C1][C2][Cn]
 *
 * The command begins with SETROAMSCANCHANNELS followed by a space, but
 * what follows the space is an array of u08 parameters.  For example
 *
 *    SETROAMSCANCHANNELS [0x04 0x24 0x28 0x2c 0x30]
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_set_roam_scan_channels_v2(struct hdd_adapter *adapter,
				    const char *command)
{
	const uint8_t *value;
	uint8_t channel_list[WNI_CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t channel;
	uint8_t num_chan;
	int i;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;
	int ret = 0;
	mac_handle_t mac_handle;

	/* array of values begins after "SETROAMSCANCHANNELS " */
	value = command + 20;

	num_chan = *value++;
	if (num_chan > WNI_CFG_VALID_CHANNEL_LIST_LEN) {
		hdd_err("number of channels (%d) supported exceeded max (%d)",
			  num_chan, WNI_CFG_VALID_CHANNEL_LIST_LEN);
		ret = -EINVAL;
		goto exit;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_SETROAMSCANCHANNELS_IOCTL,
		   adapter->session_id, num_chan);


	for (i = 0; i < num_chan; i++) {
		channel = *value++;
		if (!channel) {
			hdd_err("Channels end at index %d, expected %d",
				i, num_chan);
			ret = -EINVAL;
			goto exit;
		}

		if (channel > WNI_CFG_CURRENT_CHANNEL_STAMAX) {
			hdd_err("index %d invalid channel %d",
				  i, channel);
			ret = -EINVAL;
			goto exit;
		}
		channel_list[i] = channel;
	}

	mac_handle = hdd_ctx->mac_handle;
	if (!sme_validate_channel_list(mac_handle, channel_list, num_chan)) {
		hdd_err("List contains invalid channel(s)");
		ret = -EINVAL;
		goto exit;
	}

	status = sme_change_roam_scan_channel_list(mac_handle,
						   adapter->session_id,
						   channel_list, num_chan);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Failed to update channel list information");
		ret = -EINVAL;
		goto exit;
	}
exit:
	return ret;
}

/**
 * hdd_parse_set_roam_scan_channels() - parse the
 * SETROAMSCANCHANNELS command
 * @adapter:	Adapter upon which the command was received
 * @command:	Command that was received
 *
 * There are two different versions of the SETROAMSCANCHANNELS command.
 * Version 1 of the command contains a parameter list that is ASCII
 * characters whereas version 2 contains a binary payload.  Determine
 * if a version 1 or a version 2 command is being parsed by examining
 * the parameters, and then dispatch the parser that is appropriate for
 * the command.
 *
 * Return: 0 for success non-zero for failure
 */
static int
hdd_parse_set_roam_scan_channels(struct hdd_adapter *adapter, const char *command)
{
	const char *cursor;
	char ch;
	bool v1;
	int ret;

	/* start after "SETROAMSCANCHANNELS " */
	cursor = command + 20;

	/* assume we have a version 1 command until proven otherwise */
	v1 = true;

	/* v1 params will only contain ASCII digits and space */
	while ((ch = *cursor++) && v1) {
		if (!(isdigit(ch) || isspace(ch)))
			v1 = false;
	}

	if (v1)
		ret = hdd_parse_set_roam_scan_channels_v1(adapter, command);
	else
		ret = hdd_parse_set_roam_scan_channels_v2(adapter, command);

	return ret;
}

#ifdef FEATURE_WLAN_ESE
/**
 * hdd_parse_plm_cmd() - HDD Parse Plm command
 * @pValue:	Pointer to input data
 * @pPlmRequest:Pointer to output struct tpSirPlmReq
 *
 * This function parses the plm command passed in the format
 * CCXPLMREQ<space><enable><space><dialog_token><space>
 * <meas_token><space><num_of_bursts><space><burst_int><space>
 * <measu duration><space><burst_len><space><desired_tx_pwr>
 * <space><multcast_addr><space><number_of_channels>
 * <space><channel_numbers>
 *
 * Return: 0 for success non-zero for failure
 */
static QDF_STATUS hdd_parse_plm_cmd(uint8_t *pValue, tSirPlmReq *pPlmRequest)
{
	uint8_t *cmdPtr = NULL;
	int count, content = 0, ret = 0;
	char buf[32];

	/* move to argument list */
	cmdPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
	if (NULL == cmdPtr)
		return QDF_STATUS_E_FAILURE;

	/* no space after the command */
	if (SPACE_ASCII_VALUE != *cmdPtr)
		return QDF_STATUS_E_FAILURE;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
		cmdPtr++;

	/* START/STOP PLM req */
	ret = sscanf(cmdPtr, "%31s ", buf);
	if (1 != ret)
		return QDF_STATUS_E_FAILURE;

	ret = kstrtos32(buf, 10, &content);
	if (ret < 0)
		return QDF_STATUS_E_FAILURE;

	pPlmRequest->enable = content;
	cmdPtr = strpbrk(cmdPtr, " ");

	if (NULL == cmdPtr)
		return QDF_STATUS_E_FAILURE;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
		cmdPtr++;

	/* Dialog token of radio meas req containing meas reqIE */
	ret = sscanf(cmdPtr, "%31s ", buf);
	if (1 != ret)
		return QDF_STATUS_E_FAILURE;

	ret = kstrtos32(buf, 10, &content);
	if (ret < 0)
		return QDF_STATUS_E_FAILURE;

	pPlmRequest->diag_token = content;
	hdd_debug("diag token %d", pPlmRequest->diag_token);
	cmdPtr = strpbrk(cmdPtr, " ");

	if (NULL == cmdPtr)
		return QDF_STATUS_E_FAILURE;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
		cmdPtr++;

	/* measurement token of meas req IE */
	ret = sscanf(cmdPtr, "%31s ", buf);
	if (1 != ret)
		return QDF_STATUS_E_FAILURE;

	ret = kstrtos32(buf, 10, &content);
	if (ret < 0)
		return QDF_STATUS_E_FAILURE;

	pPlmRequest->meas_token = content;
	hdd_debug("meas token %d", pPlmRequest->meas_token);

	hdd_debug("PLM req %s", pPlmRequest->enable ? "START" : "STOP");
	if (pPlmRequest->enable) {

		cmdPtr = strpbrk(cmdPtr, " ");

		if (NULL == cmdPtr)
			return QDF_STATUS_E_FAILURE;

		/* remove empty spaces */
		while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
			cmdPtr++;

		/* total number of bursts after which STA stops sending */
		ret = sscanf(cmdPtr, "%31s ", buf);
		if (1 != ret)
			return QDF_STATUS_E_FAILURE;

		ret = kstrtos32(buf, 10, &content);
		if (ret < 0)
			return QDF_STATUS_E_FAILURE;

		if (content < 0)
			return QDF_STATUS_E_FAILURE;

		pPlmRequest->numBursts = content;
		hdd_debug("num burst %d", pPlmRequest->numBursts);
		cmdPtr = strpbrk(cmdPtr, " ");

		if (NULL == cmdPtr)
			return QDF_STATUS_E_FAILURE;

		/* remove empty spaces */
		while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
			cmdPtr++;

		/* burst interval in seconds */
		ret = sscanf(cmdPtr, "%31s ", buf);
		if (1 != ret)
			return QDF_STATUS_E_FAILURE;

		ret = kstrtos32(buf, 10, &content);
		if (ret < 0)
			return QDF_STATUS_E_FAILURE;

		if (content <= 0)
			return QDF_STATUS_E_FAILURE;

		pPlmRequest->burstInt = content;
		hdd_debug("burst Int %d", pPlmRequest->burstInt);
		cmdPtr = strpbrk(cmdPtr, " ");

		if (NULL == cmdPtr)
			return QDF_STATUS_E_FAILURE;

		/* remove empty spaces */
		while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
			cmdPtr++;

		/* Meas dur in TU's,STA goes off-ch and transmit PLM bursts */
		ret = sscanf(cmdPtr, "%31s ", buf);
		if (1 != ret)
			return QDF_STATUS_E_FAILURE;

		ret = kstrtos32(buf, 10, &content);
		if (ret < 0)
			return QDF_STATUS_E_FAILURE;

		if (content <= 0)
			return QDF_STATUS_E_FAILURE;

		pPlmRequest->measDuration = content;
		hdd_debug("measDur %d", pPlmRequest->measDuration);
		cmdPtr = strpbrk(cmdPtr, " ");

		if (NULL == cmdPtr)
			return QDF_STATUS_E_FAILURE;

		/* remove empty spaces */
		while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
			cmdPtr++;

		/* burst length of PLM bursts */
		ret = sscanf(cmdPtr, "%31s ", buf);
		if (1 != ret)
			return QDF_STATUS_E_FAILURE;

		ret = kstrtos32(buf, 10, &content);
		if (ret < 0)
			return QDF_STATUS_E_FAILURE;

		if (content <= 0)
			return QDF_STATUS_E_FAILURE;

		pPlmRequest->burstLen = content;
		hdd_debug("burstLen %d", pPlmRequest->burstLen);
		cmdPtr = strpbrk(cmdPtr, " ");

		if (NULL == cmdPtr)
			return QDF_STATUS_E_FAILURE;

		/* remove empty spaces */
		while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
			cmdPtr++;

		/* desired tx power for transmission of PLM bursts */
		ret = sscanf(cmdPtr, "%31s ", buf);
		if (1 != ret)
			return QDF_STATUS_E_FAILURE;

		ret = kstrtos32(buf, 10, &content);
		if (ret < 0)
			return QDF_STATUS_E_FAILURE;

		if (content <= 0)
			return QDF_STATUS_E_FAILURE;

		pPlmRequest->desiredTxPwr = content;
		hdd_debug("desiredTxPwr %d",
			   pPlmRequest->desiredTxPwr);

		for (count = 0; count < QDF_MAC_ADDR_SIZE; count++) {
			cmdPtr = strpbrk(cmdPtr, " ");

			if (NULL == cmdPtr)
				return QDF_STATUS_E_FAILURE;

			/* remove empty spaces */
			while ((SPACE_ASCII_VALUE == *cmdPtr)
			       && ('\0' != *cmdPtr))
				cmdPtr++;

			ret = sscanf(cmdPtr, "%31s ", buf);
			if (1 != ret)
				return QDF_STATUS_E_FAILURE;

			ret = kstrtos32(buf, 16, &content);
			if (ret < 0)
				return QDF_STATUS_E_FAILURE;

			pPlmRequest->mac_addr.bytes[count] = content;
		}

		hdd_debug("MC addr " MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(pPlmRequest->mac_addr.bytes));

		cmdPtr = strpbrk(cmdPtr, " ");

		if (NULL == cmdPtr)
			return QDF_STATUS_E_FAILURE;

		/* remove empty spaces */
		while ((SPACE_ASCII_VALUE == *cmdPtr) && ('\0' != *cmdPtr))
			cmdPtr++;

		/* number of channels */
		ret = sscanf(cmdPtr, "%31s ", buf);
		if (1 != ret)
			return QDF_STATUS_E_FAILURE;

		ret = kstrtos32(buf, 10, &content);
		if (ret < 0)
			return QDF_STATUS_E_FAILURE;

		if (content < 0)
			return QDF_STATUS_E_FAILURE;

		content = QDF_MIN(content, WNI_CFG_VALID_CHANNEL_LIST_LEN);
		pPlmRequest->plmNumCh = content;
		hdd_debug("numch: %d", pPlmRequest->plmNumCh);

		/* Channel numbers */
		for (count = 0; count < pPlmRequest->plmNumCh; count++) {
			cmdPtr = strpbrk(cmdPtr, " ");

			if (NULL == cmdPtr)
				return QDF_STATUS_E_FAILURE;

			/* remove empty spaces */
			while ((SPACE_ASCII_VALUE == *cmdPtr)
			       && ('\0' != *cmdPtr))
				cmdPtr++;

			ret = sscanf(cmdPtr, "%31s ", buf);
			if (1 != ret)
				return QDF_STATUS_E_FAILURE;

			ret = kstrtos32(buf, 10, &content);
			if (ret < 0 || content <= 0 ||
			    content > WNI_CFG_CURRENT_CHANNEL_STAMAX)
				return QDF_STATUS_E_FAILURE;

			pPlmRequest->plmChList[count] = content;
			hdd_debug(" ch- %d", pPlmRequest->plmChList[count]);
		}
	}
	/* If PLM START */
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/**
 * wlan_hdd_ready_to_extwow() - Callback function for enable ext wow
 * @cookie: callback context
 * @is_success: suspend status of ext wow
 *
 * Return: none
 */
static void wlan_hdd_ready_to_extwow(void *cookie, bool is_success)
{
	struct osif_request *request = NULL;
	struct enable_ext_wow_priv *priv = NULL;

	request = osif_request_get(cookie);
	if (!request) {
		hdd_err("Obselete request");
		return;
	}
	priv = osif_request_priv(request);
	priv->ext_wow_should_suspend = is_success;

	osif_request_complete(request);
	osif_request_put(request);
}

static int hdd_enable_ext_wow(struct hdd_adapter *adapter,
			      tpSirExtWoWParams arg_params)
{
	tSirExtWoWParams params;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int rc;
	struct enable_ext_wow_priv *priv = NULL;
	struct osif_request *request = NULL;
	void *cookie = NULL;
	struct osif_request_params hdd_params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_READY_TO_EXTWOW,
	};

	qdf_mem_copy(&params, arg_params, sizeof(params));

	request = osif_request_alloc(&hdd_params);
	if (!request) {
		hdd_err("Request Allocation Failure");
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);

	status = sme_configure_ext_wow(hdd_ctx->mac_handle, &params,
				       &wlan_hdd_ready_to_extwow,
				       cookie);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("sme_configure_ext_wow returned failure %d",
			 status);
		rc = -EPERM;
		goto exit;
	}

	rc = osif_request_wait_for_response(request);
	if (rc) {
		hdd_err("Failed to get ready to extwow");
		rc = -EPERM;
		goto exit;
	}

	priv = osif_request_priv(request);
	if (!priv->ext_wow_should_suspend) {
		hdd_err("Received ready to ExtWoW failure");
		rc = -EPERM;
		goto exit;
	}

	if (hdd_ctx->config->extWowGotoSuspend) {
		hdd_info("Received ready to ExtWoW. Going to suspend");

		rc = wlan_hdd_cfg80211_suspend_wlan(hdd_ctx->wiphy, NULL);
		if (rc < 0) {
			hdd_err("wlan_hdd_cfg80211_suspend_wlan failed, error = %d",
				rc);
			goto exit;
		}
		rc = wlan_hdd_bus_suspend();
		if (rc) {
			hdd_err("wlan_hdd_bus_suspend failed, status = %d",
				rc);
			wlan_hdd_cfg80211_resume_wlan(hdd_ctx->wiphy);
			goto exit;
		}
	}
exit:
	osif_request_put(request);
	return rc;
}

static int hdd_enable_ext_wow_parser(struct hdd_adapter *adapter, int vdev_id,
				     int value)
{
	tSirExtWoWParams params;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int rc;

	rc = wlan_hdd_validate_context(hdd_ctx);
	if (rc)
		return rc;

	if (value < EXT_WOW_TYPE_APP_TYPE1 ||
	    value > EXT_WOW_TYPE_APP_TYPE1_2) {
		hdd_err("Invalid type: %d", value);
		return -EINVAL;
	}

	if (value == EXT_WOW_TYPE_APP_TYPE1 &&
	    hdd_ctx->is_extwow_app_type1_param_set)
		params.type = value;
	else if (value == EXT_WOW_TYPE_APP_TYPE2 &&
		 hdd_ctx->is_extwow_app_type2_param_set)
		params.type = value;
	else if (value == EXT_WOW_TYPE_APP_TYPE1_2 &&
		 hdd_ctx->is_extwow_app_type1_param_set &&
		 hdd_ctx->is_extwow_app_type2_param_set)
		params.type = value;
	else {
		hdd_err("Set app params before enable it value %d",
			 value);
		return -EINVAL;
	}

	params.vdev_id = vdev_id;
	params.wakeup_pin_num = hdd_ctx->config->extWowApp1WakeupPinNumber |
				(hdd_ctx->config->extWowApp2WakeupPinNumber
					<< 8);

	return hdd_enable_ext_wow(adapter, &params);
}

static int hdd_set_app_type1_params(mac_handle_t mac_handle,
				    tpSirAppType1Params arg_params)
{
	tSirAppType1Params params;
	QDF_STATUS status;

	qdf_mem_copy(&params, arg_params, sizeof(params));

	status = sme_configure_app_type1_params(mac_handle, &params);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("sme_configure_app_type1_params returned failure %d",
			 status);
		return -EPERM;
	}

	return 0;
}

static int hdd_set_app_type1_parser(struct hdd_adapter *adapter,
				    char *arg, int len)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	char id[20], password[20];
	tSirAppType1Params params;
	int rc;

	rc = wlan_hdd_validate_context(hdd_ctx);
	if (rc)
		return rc;

	if (2 != sscanf(arg, "%8s %16s", id, password)) {
		hdd_err("Invalid Number of arguments");
		return -EINVAL;
	}

	memset(&params, 0, sizeof(tSirAppType1Params));
	params.vdev_id = adapter->session_id;
	qdf_copy_macaddr(&params.wakee_mac_addr, &adapter->mac_addr);

	params.id_length = strlen(id);
	qdf_mem_copy(params.identification_id, id, params.id_length);
	params.pass_length = strlen(password);
	qdf_mem_copy(params.password, password, params.pass_length);

	hdd_debug("%d %pM %.8s %u %.16s %u",
		  params.vdev_id, params.wakee_mac_addr.bytes,
		  params.identification_id, params.id_length,
		  params.password, params.pass_length);

	return hdd_set_app_type1_params(hdd_ctx->mac_handle, &params);
}

static int hdd_set_app_type2_params(mac_handle_t mac_handle,
				    tpSirAppType2Params arg_params)
{
	tSirAppType2Params params;
	QDF_STATUS status;

	qdf_mem_copy(&params, arg_params, sizeof(params));

	status = sme_configure_app_type2_params(mac_handle, &params);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("sme_configure_app_type2_params returned failure %d",
			 status);
		return -EPERM;
	}

	return 0;
}

static int hdd_set_app_type2_parser(struct hdd_adapter *adapter,
				    char *arg, int len)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	char mac_addr[20], rc4_key[20];
	unsigned int gateway_mac[QDF_MAC_ADDR_SIZE];
	tSirAppType2Params params;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	memset(&params, 0, sizeof(tSirAppType2Params));

	ret = sscanf(arg, "%17s %16s %x %x %x %u %u %hu %hu %u %u %u %u %u %u",
		     mac_addr, rc4_key, (unsigned int *)&params.ip_id,
		     (unsigned int *)&params.ip_device_ip,
		     (unsigned int *)&params.ip_server_ip,
		     (unsigned int *)&params.tcp_seq,
		     (unsigned int *)&params.tcp_ack_seq,
		     (uint16_t *)&params.tcp_src_port,
		     (uint16_t *)&params.tcp_dst_port,
		     (unsigned int *)&params.keepalive_init,
		     (unsigned int *)&params.keepalive_min,
		     (unsigned int *)&params.keepalive_max,
		     (unsigned int *)&params.keepalive_inc,
		     (unsigned int *)&params.tcp_tx_timeout_val,
		     (unsigned int *)&params.tcp_rx_timeout_val);

	if (ret != 15 && ret != 7) {
		hdd_err("Invalid Number of arguments");
		return -EINVAL;
	}

	if (6 != sscanf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
			&gateway_mac[0], &gateway_mac[1], &gateway_mac[2],
			&gateway_mac[3], &gateway_mac[4], &gateway_mac[5])) {
		hdd_err("Invalid MacAddress Input %s", mac_addr);
		return -EINVAL;
	}

	if (params.tcp_src_port > WLAN_HDD_MAX_TCP_PORT ||
	    params.tcp_dst_port > WLAN_HDD_MAX_TCP_PORT) {
		hdd_err("Invalid TCP Port Number");
		return -EINVAL;
	}

	qdf_mem_copy(&params.gateway_mac.bytes, (uint8_t *) &gateway_mac,
			QDF_MAC_ADDR_SIZE);

	params.rc4_key_len = strlen(rc4_key);
	qdf_mem_copy(params.rc4_key, rc4_key, params.rc4_key_len);

	params.vdev_id = adapter->session_id;
	params.tcp_src_port = (params.tcp_src_port != 0) ?
		params.tcp_src_port : hdd_ctx->config->extWowApp2TcpSrcPort;
	params.tcp_dst_port = (params.tcp_dst_port != 0) ?
		params.tcp_dst_port : hdd_ctx->config->extWowApp2TcpDstPort;
	params.keepalive_init = (params.keepalive_init != 0) ?
		params.keepalive_init : hdd_ctx->config->
						extWowApp2KAInitPingInterval;
	params.keepalive_min =
		(params.keepalive_min != 0) ?
			params.keepalive_min :
			hdd_ctx->config->extWowApp2KAMinPingInterval;
	params.keepalive_max =
		(params.keepalive_max != 0) ?
			params.keepalive_max :
			hdd_ctx->config->extWowApp2KAMaxPingInterval;
	params.keepalive_inc =
		(params.keepalive_inc != 0) ?
			params.keepalive_inc :
			hdd_ctx->config->extWowApp2KAIncPingInterval;
	params.tcp_tx_timeout_val =
		(params.tcp_tx_timeout_val != 0) ?
			params.tcp_tx_timeout_val :
			hdd_ctx->config->extWowApp2TcpTxTimeout;
	params.tcp_rx_timeout_val =
		(params.tcp_rx_timeout_val != 0) ?
			params.tcp_rx_timeout_val :
			hdd_ctx->config->extWowApp2TcpRxTimeout;

	hdd_debug("%pM %.16s %u %u %u %u %u %u %u %u %u %u %u %u %u",
		  gateway_mac, rc4_key, params.ip_id,
		  params.ip_device_ip, params.ip_server_ip, params.tcp_seq,
		  params.tcp_ack_seq, params.tcp_src_port, params.tcp_dst_port,
		  params.keepalive_init, params.keepalive_min,
		  params.keepalive_max, params.keepalive_inc,
		  params.tcp_tx_timeout_val, params.tcp_rx_timeout_val);

	return hdd_set_app_type2_params(hdd_ctx->mac_handle, &params);
}
#endif /* WLAN_FEATURE_EXTWOW_SUPPORT */

/**
 * hdd_parse_setmaxtxpower_command() - HDD Parse MAXTXPOWER command
 * @pValue:	Pointer to MAXTXPOWER command
 * @pDbm:	Pointer to tx power
 *
 * This function parses the MAXTXPOWER command passed in the format
 * MAXTXPOWER<space>X(Tx power in dbm)
 *
 * For example input commands:
 * 1) MAXTXPOWER -8 -> This is translated into set max TX power to -8 dbm
 * 2) MAXTXPOWER -23 -> This is translated into set max TX power to -23 dbm
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_setmaxtxpower_command(uint8_t *pValue, int *pTxPower)
{
	uint8_t *inPtr = pValue;
	int tempInt;
	int v = 0;
	*pTxPower = 0;

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == inPtr)
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *inPtr) /* no space after the command */
		return -EINVAL;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	/* no argument followed by spaces */
	if ('\0' == *inPtr)
		return 0;

	v = kstrtos32(inPtr, 10, &tempInt);

	/* Range checking for passed parameter */
	if ((tempInt < HDD_MIN_TX_POWER) || (tempInt > HDD_MAX_TX_POWER))
		return -EINVAL;

	*pTxPower = tempInt;

	hdd_debug("SETMAXTXPOWER: %d", *pTxPower);

	return 0;
} /* End of hdd_parse_setmaxtxpower_command */

static int hdd_get_dwell_time(struct hdd_context *hdd_ctx,  uint8_t *command,
			      char *extra, uint8_t n, uint8_t *len)
{
	uint32_t val = 0;
	struct hdd_config *pCfg = hdd_ctx->config;

	if (!pCfg || !command || !extra || !len) {
		hdd_err("argument passed for GETDWELLTIME is incorrect");
		return -EINVAL;
	}

	if (strncmp(command, "GETDWELLTIME ACTIVE MAX", 23) == 0) {
		*len = scnprintf(extra, n, "GETDWELLTIME ACTIVE MAX %u\n",
				 (int)pCfg->nActiveMaxChnTime);
		return 0;
	}
	if (strncmp(command, "GETDWELLTIME ACTIVE MIN", 23) == 0) {
		*len = scnprintf(extra, n, "GETDWELLTIME ACTIVE MIN %u\n",
				 (int)pCfg->nActiveMinChnTime);
		return 0;
	}
	if (strncmp(command, "GETDWELLTIME PASSIVE MAX", 24) == 0) {
		*len = scnprintf(extra, n, "GETDWELLTIME PASSIVE MAX %u\n",
				 (int)pCfg->nPassiveMaxChnTime);
		return 0;
	}
	if (strncmp(command, "GETDWELLTIME PASSIVE MIN", 24) == 0) {
		*len = scnprintf(extra, n, "GETDWELLTIME PASSIVE MIN %u\n",
				 (int)pCfg->nPassiveMinChnTime);
		return 0;
	}
	if (strncmp(command, "GETDWELLTIME 2G MAX", 19) == 0) {
		ucfg_scan_cfg_get_active_2g_dwelltime(hdd_ctx->psoc, &val);
		*len = scnprintf(extra, n, "GETDWELLTIME 2G MAX %u\n",
				 val);
		return 0;
	}
	if (strncmp(command, "GETDWELLTIME", 12) == 0) {
		*len = scnprintf(extra, n, "GETDWELLTIME %u\n",
				 (int)pCfg->nActiveMaxChnTime);
		return 0;
	}

	return -EINVAL;
}

static int hdd_set_dwell_time(struct hdd_adapter *adapter, uint8_t *command)
{
	mac_handle_t mac_handle = hdd_adapter_get_mac_handle(adapter);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_config *pCfg;
	uint8_t *value = command;
	tSmeConfigParams *sme_config;
	int val = 0, temp = 0;
	int retval = 0;

	pCfg = (WLAN_HDD_GET_CTX(adapter))->config;
	if (!pCfg || !mac_handle) {
		hdd_err("argument passed for SETDWELLTIME is incorrect");
		return -EINVAL;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (retval)
		return retval;

	sme_config = qdf_mem_malloc(sizeof(*sme_config));
	if (!sme_config) {
		hdd_err("failed to allocate memory for sme_config");
		return -ENOMEM;
	}
	qdf_mem_zero(sme_config, sizeof(*sme_config));
	sme_get_config_param(mac_handle, sme_config);

	if (strncmp(command, "SETDWELLTIME ACTIVE MAX", 23) == 0) {
		if (drv_cmd_validate(command, 23)) {
			retval = -EINVAL;
			goto free;
		}
		value = value + 24;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
		    val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX) {
			hdd_err("argument passed for SETDWELLTIME ACTIVE MAX is incorrect");
			retval = -EFAULT;
			goto free;
		}
		pCfg->nActiveMaxChnTime = val;
		sme_config->csrConfig.nActiveMaxChnTime = val;
		sme_update_config(mac_handle, sme_config);
		ucfg_scan_cfg_set_active_dwelltime(hdd_ctx->psoc, val);
	} else if (strncmp(command, "SETDWELLTIME ACTIVE MIN", 23) == 0) {
		if (drv_cmd_validate(command, 23)) {
			retval = -EINVAL;
			goto free;
		}

		value = value + 24;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_ACTIVE_MIN_CHANNEL_TIME_MIN ||
		    val > CFG_ACTIVE_MIN_CHANNEL_TIME_MAX) {
			hdd_err("argument passed for SETDWELLTIME ACTIVE MIN is incorrect");
			retval = -EFAULT;
			goto free;
		}
		pCfg->nActiveMinChnTime = val;
		sme_config->csrConfig.nActiveMinChnTime = val;
		sme_update_config(mac_handle, sme_config);
	} else if (strncmp(command, "SETDWELLTIME PASSIVE MAX", 24) == 0) {
		if (drv_cmd_validate(command, 24)) {
			retval = -EINVAL;
			goto free;
		}

		value = value + 25;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_PASSIVE_MAX_CHANNEL_TIME_MIN ||
		    val > CFG_PASSIVE_MAX_CHANNEL_TIME_MAX) {
			hdd_err("argument passed for SETDWELLTIME PASSIVE MAX is incorrect");
			retval = -EFAULT;
			goto free;
		}
		pCfg->nPassiveMaxChnTime = val;
		sme_config->csrConfig.nPassiveMaxChnTime = val;
		sme_update_config(mac_handle, sme_config);
		ucfg_scan_cfg_set_passive_dwelltime(hdd_ctx->psoc, val);
	} else if (strncmp(command, "SETDWELLTIME PASSIVE MIN", 24) == 0) {
		if (drv_cmd_validate(command, 24)) {
			retval = -EINVAL;
			goto free;
		}

		value = value + 25;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_PASSIVE_MIN_CHANNEL_TIME_MIN ||
		    val > CFG_PASSIVE_MIN_CHANNEL_TIME_MAX) {
			hdd_err("argument passed for SETDWELLTIME PASSIVE MIN is incorrect");
			retval = -EFAULT;
			goto free;
		}
		pCfg->nPassiveMinChnTime = val;
		sme_config->csrConfig.nPassiveMinChnTime = val;
		sme_update_config(mac_handle, sme_config);
	} else if (strncmp(command, "SETDWELLTIME 2G MAX", 19) == 0) {
		if (drv_cmd_validate(command, 19)) {
			retval = -EINVAL;
			goto free;
		}

		value = value + 20;
		temp = kstrtou32(value, 10, &val);
		if (temp ||  val < CFG_ACTIVE_MAX_2G_CHANNEL_TIME_MIN ||
		    val > CFG_ACTIVE_MAX_2G_CHANNEL_TIME_MAX) {
			hdd_err_rl("argument passed for SETDWELLTIME 2G MAX is incorrect");
			return -EFAULT;
		}
		ucfg_scan_cfg_set_active_2g_dwelltime(hdd_ctx->psoc, val);
	}  else if (strncmp(command, "SETDWELLTIME", 12) == 0) {
		if (drv_cmd_validate(command, 12)) {
			retval = -EINVAL;
			goto free;
		}

		value = value + 13;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_MIN ||
		    val > CFG_ACTIVE_MAX_CHANNEL_TIME_MAX) {
			hdd_err("argument passed for SETDWELLTIME is incorrect");
			retval = -EFAULT;
			goto free;
		}
		pCfg->nActiveMaxChnTime = val;
		sme_config->csrConfig.nActiveMaxChnTime = val;
		sme_update_config(mac_handle, sme_config);
		ucfg_scan_cfg_set_active_dwelltime(hdd_ctx->psoc, val);
	} else {
		retval = -EINVAL;
		goto free;
	}

free:
	qdf_mem_free(sme_config);
	return retval;
}

struct link_status_priv {
	uint8_t link_status;
};

/**
 * hdd_conc_set_dwell_time() - Set Concurrent dwell time parameters
 * @adapter: Adapter upon which the command was received
 * @command: ASCII text command that is received
 *
 * Driver commands:
 * wpa_cli DRIVER CONCSETDWELLTIME ACTIVE MAX <value>
 * wpa_cli DRIVER CONCSETDWELLTIME ACTIVE MIN <value>
 * wpa_cli DRIVER CONCSETDWELLTIME PASSIVE MAX <value>
 * wpa_cli DRIVER CONCSETDWELLTIME PASSIVE MIN <value>
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_conc_set_dwell_time(struct hdd_adapter *adapter,
				   uint8_t *command)
{
	mac_handle_t mac_handle = hdd_adapter_get_mac_handle(adapter);
	struct hdd_config *p_cfg;
	u8 *value = command;
	tSmeConfigParams *sme_config;
	int val = 0, temp = 0;
	int retval = 0;

	p_cfg = (WLAN_HDD_GET_CTX(adapter))->config;
	if (!p_cfg || !mac_handle) {
		hdd_err("Argument passed for CONCSETDWELLTIME is incorrect");
		return -EINVAL;
	}

	sme_config = qdf_mem_malloc(sizeof(*sme_config));
	if (!sme_config) {
		hdd_err("Failed to allocate memory for sme_config");
		return -ENOMEM;
	}

	qdf_mem_zero(sme_config, sizeof(*sme_config));
	sme_get_config_param(mac_handle, sme_config);

	if (strncmp(command, "CONCSETDWELLTIME ACTIVE MAX", 27) == 0) {
		if (drv_cmd_validate(command, 27)) {
			hdd_err("Invalid driver command");
			retval = -EINVAL;
			goto sme_config_free;
		}

		value = value + 28;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MIN ||
		    val > CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MAX) {
			hdd_err("Argument passed for CONCSETDWELLTIME ACTIVE MAX is incorrect");
			retval = -EFAULT;
			goto sme_config_free;
		}

		p_cfg->nActiveMaxChnTimeConc = val;
		sme_config->csrConfig.nActiveMaxChnTimeConc = val;
		sme_update_config(mac_handle, sme_config);
		ucfg_scan_cfg_set_conc_active_dwelltime(
				(WLAN_HDD_GET_CTX(adapter))->psoc, val);
	} else if (strncmp(command, "CONCSETDWELLTIME ACTIVE MIN", 27) == 0) {
		if (drv_cmd_validate(command, 27)) {
			hdd_err("Invalid driver command");
			retval = -EINVAL;
			goto sme_config_free;
		}

		value = value + 28;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MIN ||
		    val > CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MAX) {
			hdd_err("argument passed for CONCSETDWELLTIME ACTIVE MIN is incorrect");
			retval = -EFAULT;
			goto sme_config_free;
		}

		p_cfg->nActiveMinChnTimeConc = val;
		sme_config->csrConfig.nActiveMinChnTimeConc = val;
		sme_update_config(mac_handle, sme_config);
	} else if (strncmp(command, "CONCSETDWELLTIME PASSIVE MAX", 28) == 0) {
		if (drv_cmd_validate(command, 28)) {
			hdd_err("Invalid driver command");
			retval = -EINVAL;
			goto sme_config_free;
		}

		value = value + 29;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MIN ||
		    val > CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MAX) {
			hdd_err("Argument passed for CONCSETDWELLTIME PASSIVE MAX is incorrect");
			retval = -EFAULT;
			goto sme_config_free;
		}

		p_cfg->nPassiveMaxChnTimeConc = val;
		sme_config->csrConfig.nPassiveMaxChnTimeConc = val;
		sme_update_config(mac_handle, sme_config);
		ucfg_scan_cfg_set_conc_passive_dwelltime(
				(WLAN_HDD_GET_CTX(adapter))->psoc, val);
	} else if (strncmp(command, "CONCSETDWELLTIME PASSIVE MIN", 28) == 0) {
		if (drv_cmd_validate(command, 28)) {
			hdd_err("Invalid driver command");
			retval = -EINVAL;
			goto sme_config_free;
		}

		value = value + 29;
		temp = kstrtou32(value, 10, &val);
		if (temp != 0 || val < CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MIN ||
		    val > CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MAX) {
			hdd_err("argument passed for SETDWELLTIME PASSIVE MIN is incorrect");
			retval = -EFAULT;
			goto sme_config_free;
		}

		p_cfg->nPassiveMinChnTimeConc = val;
		sme_config->csrConfig.nPassiveMinChnTimeConc = val;
		sme_update_config(mac_handle, sme_config);
	} else {
		retval = -EINVAL;
	}

sme_config_free:
	qdf_mem_free(sme_config);
	return retval;
}

static void hdd_get_link_status_cb(uint8_t status, void *context)
{
	struct osif_request *request;
	struct link_status_priv *priv;

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->link_status = status;
	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * wlan_hdd_get_link_status() - get link status
 * @adapter:     pointer to the adapter
 *
 * This function sends a request to query the link status and waits
 * on a timer to invoke the callback. if the callback is invoked then
 * latest link status shall be returned or otherwise cached value
 * will be returned.
 *
 * Return: On success, link status shall be returned.
 *         On error or not associated, link status 0 will be returned.
 */
static int wlan_hdd_get_link_status(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;
	QDF_STATUS status;
	int ret;
	void *cookie;
	struct osif_request *request;
	struct link_status_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_LINK_STATUS,
	};

	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		hdd_warn("Recovery in Progress. State: 0x%x Ignore!!!",
			 cds_get_driver_state());
		return 0;
	}

	if ((QDF_STA_MODE != adapter->device_mode) &&
	    (QDF_P2P_CLIENT_MODE != adapter->device_mode)) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return 0;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (eConnectionState_Associated != sta_ctx->conn_info.connState) {
		/* If not associated, then expected link status return
		 * value is 0
		 */
		hdd_warn("Not associated!");
		return 0;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return 0;
	}
	cookie = osif_request_cookie(request);

	status = sme_get_link_status(adapter->hdd_ctx->mac_handle,
				     hdd_get_link_status_cb,
				     cookie, adapter->session_id);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Unable to retrieve link status");
		/* return a cached value */
	} else {
		/* request is sent -- wait for the response */
		ret = osif_request_wait_for_response(request);
		if (ret) {
			hdd_err("SME timed out while retrieving link status");
			/* return a cached value */
		} else {
			/* update the adapter with the fresh results */
			priv = osif_request_priv(request);
			adapter->link_status = priv->link_status;
		}
	}

	/*
	 * either we never sent a request, we sent a request and
	 * received a response or we sent a request and timed out.
	 * regardless we are done with the request.
	 */
	osif_request_put(request);

	/* either callback updated adapter stats or it has cached data */
	return adapter->link_status;
}

static void hdd_tx_fail_ind_callback(uint8_t *MacAddr, uint8_t seqNo)
{
	int payload_len;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	uint8_t *data;

	payload_len = ETH_ALEN;

	if (0 == cesium_pid || cesium_nl_srv_sock == NULL) {
		hdd_err("cesium process not registered");
		return;
	}

	skb = nlmsg_new(payload_len, GFP_ATOMIC);
	if (skb == NULL) {
		hdd_err("nlmsg_new() failed for msg size[%d]",
			 NLMSG_SPACE(payload_len));
		return;
	}

	nlh = nlmsg_put(skb, cesium_pid, seqNo, 0, payload_len, NLM_F_REQUEST);

	if (NULL == nlh) {
		hdd_err("nlmsg_put() failed for msg size[%d]",
			 NLMSG_SPACE(payload_len));

		kfree_skb(skb);
		return;
	}

	data = nlmsg_data(nlh);
	memcpy(data, MacAddr, ETH_ALEN);

	if (nlmsg_unicast(cesium_nl_srv_sock, skb, cesium_pid) < 0) {
		hdd_err("nlmsg_unicast() failed for msg size[%d]",
			 NLMSG_SPACE(payload_len));
	}
}


/**
 * hdd_ParseuserParams - return a pointer to the next argument
 * @pValue:	Input argument string
 * @ppArg:	Output pointer to the next argument
 *
 * This function parses argument stream and finds the pointer
 * to the next argument
 *
 * Return: 0 if the next argument found; -EINVAL otherwise
 */
static int hdd_parse_user_params(uint8_t *pValue, uint8_t **ppArg)
{
	uint8_t *pVal;

	pVal = strnchr(pValue, strlen(pValue), ' ');

	if (NULL == pVal) /* no argument remains */
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *pVal)/* no space after the current arg */
		return -EINVAL;

	pVal++;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *pVal) && ('\0' != *pVal))
		pVal++;

	/* no argument followed by spaces */
	if ('\0' == *pVal)
		return -EINVAL;

	*ppArg = pVal;

	return 0;
}

/**
 * hdd_parse_ibsstx_fail_event_params - Parse params
 *                                             for SETIBSSTXFAILEVENT
 * @pValue:		Input ibss tx fail event argument
 * @tx_fail_count:	(Output parameter) Tx fail counter
 * @pid:		(Output parameter) PID
 *
 * Return: 0 if the parsing succeeds; -EINVAL otherwise
 */
static int hdd_parse_ibsstx_fail_event_params(uint8_t *pValue,
					      uint8_t *tx_fail_count,
					      uint16_t *pid)
{
	uint8_t *param = NULL;
	int ret;

	ret = hdd_parse_user_params(pValue, &param);

	if (0 == ret && NULL != param) {
		if (1 != sscanf(param, "%hhu", tx_fail_count)) {
			ret = -EINVAL;
			goto done;
		}
	} else {
		goto done;
	}

	if (0 == *tx_fail_count) {
		*pid = 0;
		goto done;
	}

	pValue = param;
	pValue++;

	ret = hdd_parse_user_params(pValue, &param);

	if (0 == ret) {
		if (1 != sscanf(param, "%hu", pid)) {
			ret = -EINVAL;
			goto done;
		}
	} else {
		goto done;
	}

done:
	return ret;
}

#ifdef FEATURE_WLAN_ESE
/**
 * hdd_parse_ese_beacon_req() - Parse ese beacon request
 * @pValue:	Pointer to data
 * @pEseBcnReq:	Output pointer to store parsed ie information
 *
 * This function parses the ese beacon request passed in the format
 * CCXBEACONREQ<space><Number of fields><space><Measurement token>
 * <space>Channel 1<space>Scan Mode <space>Meas Duration<space>Channel N
 * <space>Scan Mode N<space>Meas Duration N
 *
 * If the Number of bcn req fields (N) does not match with the
 * actual number of fields passed then take N.
 * <Meas Token><Channel><Scan Mode> and <Meas Duration> are treated
 * as one pair. For example, CCXBEACONREQ 2 1 1 1 30 2 44 0 40.
 * This function does not take care of removing duplicate channels from the
 * list
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_ese_beacon_req(uint8_t *pValue,
					tCsrEseBeaconReq *pEseBcnReq)
{
	uint8_t *inPtr = pValue;
	uint8_t input = 0;
	uint32_t tempInt = 0;
	int j = 0, i = 0, v = 0;
	char buf[32];

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
	if (NULL == inPtr) /* no argument after the command */
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *inPtr) /* no space after the command */
		return -EINVAL;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;

	/* no argument followed by spaces */
	if ('\0' == *inPtr)
		return -EINVAL;

	/* Getting the first argument ie Number of IE fields */
	v = sscanf(inPtr, "%31s ", buf);
	if (1 != v)
		return -EINVAL;

	v = kstrtou8(buf, 10, &input);
	if (v < 0)
		return -EINVAL;

	input = QDF_MIN(input, SIR_ESE_MAX_MEAS_IE_REQS);
	pEseBcnReq->numBcnReqIe = input;

	hdd_debug("Number of Bcn Req Ie fields: %d", pEseBcnReq->numBcnReqIe);

	for (j = 0; j < (pEseBcnReq->numBcnReqIe); j++) {
		for (i = 0; i < 4; i++) {
			/*
			 * inPtr pointing to the beginning of 1st space
			 * after number of ie fields
			 */
			inPtr = strpbrk(inPtr, " ");
			/* no ie data after the number of ie fields argument */
			if (NULL == inPtr)
				return -EINVAL;

			/* remove empty space */
			while ((SPACE_ASCII_VALUE == *inPtr)
			       && ('\0' != *inPtr))
				inPtr++;

			/*
			 * no ie data after the number of ie fields
			 * argument and spaces
			 */
			if ('\0' == *inPtr)
				return -EINVAL;

			v = sscanf(inPtr, "%31s ", buf);
			if (1 != v)
				return -EINVAL;

			v = kstrtou32(buf, 10, &tempInt);
			if (v < 0)
				return -EINVAL;

			switch (i) {
			case 0: /* Measurement token */
				if (!tempInt) {
					hdd_err("Invalid Measurement Token: %u",
						  tempInt);
					return -EINVAL;
				}
				pEseBcnReq->bcnReq[j].measurementToken =
					tempInt;
				break;

			case 1: /* Channel number */
				if (!tempInt ||
				    (tempInt >
				     WNI_CFG_CURRENT_CHANNEL_STAMAX)) {
					hdd_err("Invalid Channel Number: %u",
						  tempInt);
					return -EINVAL;
				}
				pEseBcnReq->bcnReq[j].channel = tempInt;
				break;

			case 2: /* Scan mode */
				if ((tempInt < eSIR_PASSIVE_SCAN)
				    || (tempInt > eSIR_BEACON_TABLE)) {
					hdd_err("Invalid Scan Mode: %u Expected{0|1|2}",
						  tempInt);
					return -EINVAL;
				}
				pEseBcnReq->bcnReq[j].scanMode = tempInt;
				break;

			case 3: /* Measurement duration */
				if ((!tempInt
				     && (pEseBcnReq->bcnReq[j].scanMode !=
					 eSIR_BEACON_TABLE)) ||
				    (pEseBcnReq->bcnReq[j].scanMode ==
						eSIR_BEACON_TABLE)) {
					hdd_err("Invalid Measurement Duration: %u",
						  tempInt);
					return -EINVAL;
				}
				pEseBcnReq->bcnReq[j].measurementDuration =
					tempInt;
				break;
			}
		}
	}

	for (j = 0; j < pEseBcnReq->numBcnReqIe; j++) {
		hdd_debug("Index: %d Measurement Token: %u Channel: %u Scan Mode: %u Measurement Duration: %u",
			  j,
			  pEseBcnReq->bcnReq[j].measurementToken,
			  pEseBcnReq->bcnReq[j].channel,
			  pEseBcnReq->bcnReq[j].scanMode,
			  pEseBcnReq->bcnReq[j].measurementDuration);
	}

	return 0;
}

/**
 * hdd_parse_get_cckm_ie() - HDD Parse and fetch the CCKM IE
 * @pValue:	Pointer to input data
 * @pCckmIe:	Pointer to output cckm Ie
 * @pCckmIeLen:	Pointer to output cckm ie length
 *
 * This function parses the SETCCKM IE command
 * SETCCKMIE<space><ie data>
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_get_cckm_ie(uint8_t *pValue, uint8_t **pCckmIe,
				 uint8_t *pCckmIeLen)
{
	uint8_t *inPtr = pValue;
	uint8_t *dataEnd;
	int j = 0;
	int i = 0;
	uint8_t tempByte = 0;

	inPtr = strnchr(pValue, strlen(pValue), SPACE_ASCII_VALUE);
	/* no argument after the command */
	if (NULL == inPtr)
		return -EINVAL;
	else if (SPACE_ASCII_VALUE != *inPtr) /* no space after the command */
		return -EINVAL;

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *inPtr) && ('\0' != *inPtr))
		inPtr++;
	/* no argument followed by spaces */
	if ('\0' == *inPtr)
		return -EINVAL;

	/* find the length of data */
	dataEnd = inPtr;
	while (('\0' != *dataEnd)) {
		dataEnd++;
		++(*pCckmIeLen);
	}
	if (*pCckmIeLen <= 0)
		return -EINVAL;
	/*
	 * Allocate the number of bytes based on the number of input characters
	 * whether it is even or odd.
	 * if the number of input characters are even, then we need N / 2 byte.
	 * if the number of input characters are odd, then we need do
	 * (N + 1) / 2 to compensate rounding off.
	 * For example, if N = 18, then (18 + 1) / 2 = 9 bytes are enough.
	 * If N = 19, then we need 10 bytes, hence (19 + 1) / 2 = 10 bytes
	 */
	*pCckmIe = qdf_mem_malloc((*pCckmIeLen + 1) / 2);
	if (NULL == *pCckmIe) {
		hdd_err("qdf_mem_malloc failed");
		return -ENOMEM;
	}
	/*
	 * the buffer received from the upper layer is character buffer,
	 * we need to prepare the buffer taking 2 characters in to a U8 hex
	 * decimal number for example 7f0000f0...form a buffer to contain
	 * 7f in 0th location, 00 in 1st and f0 in 3rd location
	 */
	for (i = 0, j = 0; j < *pCckmIeLen; j += 2) {
		tempByte = (hex_to_bin(inPtr[j]) << 4) |
			   (hex_to_bin(inPtr[j + 1]));
		(*pCckmIe)[i++] = tempByte;
	}
	*pCckmIeLen = i;
	return 0;
}
#endif /* FEATURE_WLAN_ESE */

int wlan_hdd_set_mc_rate(struct hdd_adapter *adapter, int targetRate)
{
	tSirRateUpdateInd rateUpdate = {0};
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_config *pConfig;

	if (hdd_ctx == NULL) {
		hdd_err("HDD context is null");
		return -EINVAL;
	}
	if ((QDF_IBSS_MODE != adapter->device_mode) &&
	    (QDF_SAP_MODE != adapter->device_mode) &&
	    (QDF_STA_MODE != adapter->device_mode)) {
		hdd_err("Received SETMCRATE cmd in invalid mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		hdd_err("SETMCRATE cmd is allowed only in STA, IBSS or SOFTAP mode");
		return -EINVAL;
	}
	pConfig = hdd_ctx->config;
	rateUpdate.nss = (pConfig->enable2x2 == 0) ? 0 : 1;
	rateUpdate.dev_mode = adapter->device_mode;
	rateUpdate.mcastDataRate24GHz = targetRate;
	rateUpdate.mcastDataRate24GHzTxFlag = 1;
	rateUpdate.mcastDataRate5GHz = targetRate;
	rateUpdate.bcastDataRate = -1;
	qdf_copy_macaddr(&rateUpdate.bssid, &adapter->mac_addr);
	hdd_debug("MC Target rate %d, mac = %pM, dev_mode %s(%d)",
		  rateUpdate.mcastDataRate24GHz, rateUpdate.bssid.bytes,
		  hdd_device_mode_to_string(adapter->device_mode),
		  adapter->device_mode);
	status = sme_send_rate_update_ind(hdd_ctx->mac_handle, &rateUpdate);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("SETMCRATE failed");
		return -EFAULT;
	}
	return 0;
}

static int drv_cmd_p2p_dev_addr(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	struct qdf_mac_addr *addr = &hdd_ctx->p2p_device_address;
	size_t user_size = qdf_min(sizeof(addr->bytes),
				   (size_t)priv_data->total_len);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_P2P_DEV_ADDR_IOCTL,
		   adapter->session_id,
		   (unsigned int)(*(addr->bytes + 2) << 24 |
				*(addr->bytes + 3) << 16 |
				*(addr->bytes + 4) << 8 |
				*(addr->bytes + 5)));


	if (copy_to_user(priv_data->buf, addr->bytes, user_size)) {
		hdd_err("failed to copy data to user buffer");
		return -EFAULT;
	}

	return 0;
}

/**
 * drv_cmd_p2p_set_noa() - Handler for P2P_SET_NOA driver command
 * @adapter: Adapter on which the command was received
 * @hdd_ctx: HDD global context
 * @command: Entire driver command received from userspace
 * @command_len: Length of @command
 * @priv_data: Pointer to ioctl private data structure
 *
 * This is a trivial command handler function which simply forwards the
 * command to the actual command processor within the P2P module.
 *
 * Return: 0 on success, non-zero on failure
 */
static int drv_cmd_p2p_set_noa(struct hdd_adapter *adapter,
			       struct hdd_context *hdd_ctx,
			       uint8_t *command,
			       uint8_t command_len,
			       struct hdd_priv_data *priv_data)
{
	return hdd_set_p2p_noa(adapter->dev, command);
}

/**
 * drv_cmd_p2p_set_ps() - Handler for P2P_SET_PS driver command
 * @adapter: Adapter on which the command was received
 * @hdd_ctx: HDD global context
 * @command: Entire driver command received from userspace
 * @command_len: Length of @command
 * @priv_data: Pointer to ioctl private data structure
 *
 * This is a trivial command handler function which simply forwards the
 * command to the actual command processor within the P2P module.
 *
 * Return: 0 on success, non-zero on failure
 */
static int drv_cmd_p2p_set_ps(struct hdd_adapter *adapter,
			      struct hdd_context *hdd_ctx,
			      uint8_t *command,
			      uint8_t command_len,
			      struct hdd_priv_data *priv_data)
{
	return hdd_set_p2p_opps(adapter->dev, command);
}

static int drv_cmd_set_band(struct hdd_adapter *adapter,
			    struct hdd_context *hdd_ctx,
			    uint8_t *command,
			    uint8_t command_len,
			    struct hdd_priv_data *priv_data)
{
	int err;
	uint8_t band;

	/*
	 * Parse the band value passed from userspace. The first 8 bytes
	 * should be "SETBAND " and the 9th byte should be a UI band value
	 */
	err = kstrtou8(command + command_len + 1, 10, &band);
	if (err) {
		hdd_err("error %d parsing userspace band parameter", err);
		return err;
	}

	return hdd_reg_set_band(adapter->dev, band);
}

static int drv_cmd_set_wmmps(struct hdd_adapter *adapter,
			     struct hdd_context *hdd_ctx,
			     uint8_t *command,
			     uint8_t command_len,
			     struct hdd_priv_data *priv_data)
{
	return hdd_wmmps_helper(adapter, command);
}

static inline int drv_cmd_country(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	char *country_code;

	country_code = strnchr(command, strlen(command), ' ');
	/* no argument after the command */
	if (!country_code)
		return -EINVAL;

	/* no space after the command */
	if (*country_code != SPACE_ASCII_VALUE)
		return -EINVAL;

	country_code++;

	/* removing empty spaces */
	while ((*country_code == SPACE_ASCII_VALUE) &&
	       (*country_code != '\0'))
		country_code++;

	/* no or less than 2  arguments followed by spaces */
	if (*country_code == '\0' || *(country_code + 1) == '\0')
		return -EINVAL;

	return hdd_reg_set_country(hdd_ctx, country_code);
}

/**
 * drv_cmd_get_country() - Helper function to get current county code
 * @adapter: pointer to adapter on which request is received
 * @hdd_ctx: pointer to hdd context
 * @command: command name
 * @command_len: command buffer length
 * @priv_data: output pointer to hold current country code
 *
 * Return: On success 0, negative value on error.
 */
static int drv_cmd_get_country(struct hdd_adapter *adapter,
			       struct hdd_context *hdd_ctx,
			       uint8_t *command, uint8_t command_len,
			       struct hdd_priv_data *priv_data)
{
	uint8_t buf[SIZE_OF_GETCOUNTRYREV_OUTPUT] = {0};
	uint8_t cc[REG_ALPHA2_LEN + 1];
	int ret = 0, len;

	qdf_mem_copy(cc, hdd_ctx->reg.alpha2, REG_ALPHA2_LEN);
	cc[REG_ALPHA2_LEN] = '\0';

	len = scnprintf(buf, sizeof(buf), "%s %s",
			"GETCOUNTRYREV", cc);
	hdd_debug("buf = %s", buf);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, buf, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_trigger(struct hdd_adapter *adapter,
				    struct hdd_context *hdd_ctx,
				    uint8_t *command,
				    uint8_t command_len,
				    struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	int8_t rssi = 0;
	uint8_t lookUpThreshold = CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Move pointer to ahead of SETROAMTRIGGER<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtos8(value, 10, &rssi);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed Input value may be out of range[%d - %d]",
			CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
			CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX);
		ret = -EINVAL;
		goto exit;
	}

	lookUpThreshold = abs(rssi);

	if ((lookUpThreshold < CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN) ||
	    (lookUpThreshold > CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX)) {
		hdd_err("Neighbor lookup threshold value %d is out of range (Min: %d Max: %d)",
			  lookUpThreshold,
			  CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
			  CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX);
		ret = -EINVAL;
		goto exit;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_SETROAMTRIGGER_IOCTL,
		   adapter->session_id, lookUpThreshold);

	hdd_debug("Received Command to Set Roam trigger (Neighbor lookup threshold) = %d",
		  lookUpThreshold);

	hdd_ctx->config->nNeighborLookupRssiThreshold = lookUpThreshold;
	status = sme_set_neighbor_lookup_rssi_threshold(hdd_ctx->mac_handle,
							adapter->session_id,
							lookUpThreshold);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Failed to set roam trigger, try again");
		ret = -EPERM;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_get_roam_trigger(struct hdd_adapter *adapter,
				    struct hdd_context *hdd_ctx,
				    uint8_t *command,
				    uint8_t command_len,
				    struct hdd_priv_data *priv_data)
{
	int ret = 0;
	char extra[32];
	uint8_t lookup_threshold;
	int rssi;
	uint8_t len = 0;
	QDF_STATUS status;

	status = sme_get_neighbor_lookup_rssi_threshold(hdd_ctx->mac_handle,
							adapter->session_id,
							&lookup_threshold);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_GETROAMTRIGGER_IOCTL,
		   adapter->session_id, lookup_threshold);

	hdd_debug("vdev_id: %u, lookup_threshold: %u",
		  adapter->session_id, lookup_threshold);

	rssi = (-1) * lookup_threshold;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, rssi);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_scan_period(struct hdd_adapter *adapter,
					struct hdd_context *hdd_ctx,
					uint8_t *command,
					uint8_t command_len,
					struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t roamScanPeriod = 0;
	uint16_t neighborEmptyScanRefreshPeriod =
		CFG_EMPTY_SCAN_REFRESH_PERIOD_DEFAULT;

	/* input refresh period is in terms of seconds */

	/* Move pointer to ahead of SETROAMSCANPERIOD<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &roamScanPeriod);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed Input value may be out of range[%d - %d]",
			(CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN / 1000),
			(CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX / 1000));
		ret = -EINVAL;
		goto exit;
	}

	if ((roamScanPeriod < (CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN / 1000))
	    || (roamScanPeriod > (CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX / 1000))) {
		hdd_err("Roam scan period value %d is out of range (Min: %d Max: %d)",
			roamScanPeriod,
			(CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN / 1000),
			(CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX / 1000));
		ret = -EINVAL;
		goto exit;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_SETROAMSCANPERIOD_IOCTL,
		   adapter->session_id, roamScanPeriod);
	neighborEmptyScanRefreshPeriod = roamScanPeriod * 1000;

	hdd_debug("Received Command to Set roam scan period (Empty Scan refresh period) = %d",
		  roamScanPeriod);

	hdd_ctx->config->nEmptyScanRefreshPeriod =
		neighborEmptyScanRefreshPeriod;
	sme_update_empty_scan_refresh_period(hdd_ctx->mac_handle,
					     adapter->session_id,
					     neighborEmptyScanRefreshPeriod);

exit:
	return ret;
}

static int drv_cmd_get_roam_scan_period(struct hdd_adapter *adapter,
					struct hdd_context *hdd_ctx,
					uint8_t *command,
					uint8_t command_len,
					struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t empty_scan_refresh_period;
	char extra[32];
	uint8_t len;
	QDF_STATUS status;

	status = sme_get_empty_scan_refresh_period(hdd_ctx->mac_handle,
						   adapter->session_id,
						   &empty_scan_refresh_period);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	hdd_debug("vdev_id: %u, empty_scan_refresh_period: %u",
		  adapter->session_id, empty_scan_refresh_period);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_GETROAMSCANPERIOD_IOCTL,
		   adapter->session_id,
		   empty_scan_refresh_period);

	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETROAMSCANPERIOD",
			(empty_scan_refresh_period / 1000));
	/* Returned value is in units of seconds */
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_scan_refresh_period(struct hdd_adapter *adapter,
						struct hdd_context *hdd_ctx,
						uint8_t *command,
						uint8_t command_len,
						struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	uint8_t roamScanRefreshPeriod = 0;
	uint16_t neighborScanRefreshPeriod =
		CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT;

	/* input refresh period is in terms of seconds */
	/* Move pointer to ahead of SETROAMSCANREFRESHPERIOD<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &roamScanRefreshPeriod);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed Input value may be out of range[%d - %d]",
			CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN / 1000,
			CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX / 1000);
		ret = -EINVAL;
		goto exit;
	}

	if ((roamScanRefreshPeriod <
		(CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN / 1000)) ||
	    (roamScanRefreshPeriod >
		(CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX / 1000))) {
		hdd_err("Neighbor scan results refresh period value %d is out of range (Min: %d Max: %d)",
			roamScanRefreshPeriod,
			(CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN
			 / 1000),
			(CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX
			 / 1000));
		ret = -EINVAL;
		goto exit;
	}
	neighborScanRefreshPeriod = roamScanRefreshPeriod * 1000;

	hdd_debug("Received Command to Set roam scan refresh period (Scan refresh period) = %d",
		  roamScanRefreshPeriod);

	hdd_ctx->config->nNeighborResultsRefreshPeriod =
				neighborScanRefreshPeriod;
	sme_set_neighbor_scan_refresh_period(hdd_ctx->mac_handle,
					     adapter->session_id,
					     neighborScanRefreshPeriod);

exit:
	return ret;
}

static int drv_cmd_get_roam_scan_refresh_period(struct hdd_adapter *adapter,
						struct hdd_context *hdd_ctx,
						uint8_t *command,
						uint8_t command_len,
						struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t value =
		sme_get_neighbor_scan_refresh_period(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len;

	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETROAMSCANREFRESHPERIOD",
			(value / 1000));
	/* Returned value is in units of seconds */
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_mode(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	mac_handle_t mac_handle;
	int ret;
	uint8_t *value = command;
	uint8_t roamMode = CFG_LFR_FEATURE_ENABLED_DEFAULT;

	/* Move pointer to ahead of SETROAMMODE<delimiter> */
	value = value + SIZE_OF_SETROAMMODE + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &roamMode);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			CFG_LFR_FEATURE_ENABLED_MIN,
			CFG_LFR_FEATURE_ENABLED_MAX);
		ret = -EINVAL;
		goto exit;
	}
	if ((roamMode < CFG_LFR_FEATURE_ENABLED_MIN) ||
	    (roamMode > CFG_LFR_FEATURE_ENABLED_MAX)) {
		hdd_err("Roam Mode value %d is out of range (Min: %d Max: %d)",
			roamMode,
			CFG_LFR_FEATURE_ENABLED_MIN,
			CFG_LFR_FEATURE_ENABLED_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set Roam Mode = %d",
		  roamMode);
	/*
	 * Note that
	 *     SETROAMMODE 0 is to enable LFR while
	 *     SETROAMMODE 1 is to disable LFR, but
	 *     notify_is_fast_roam_ini_feature_enabled 0/1 is to
	 *     enable/disable. So, we have to invert the value
	 *     to call sme_update_is_fast_roam_ini_feature_enabled.
	 */
	if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
		roamMode = CFG_LFR_FEATURE_ENABLED_MAX;  /* Roam enable */
	else
		roamMode = CFG_LFR_FEATURE_ENABLED_MIN;  /* Roam disable */

	hdd_ctx->config->isFastRoamIniFeatureEnabled = roamMode;
	mac_handle = hdd_ctx->mac_handle;
	if (roamMode) {
		hdd_ctx->config->isRoamOffloadScanEnabled = roamMode;
		sme_update_roam_scan_offload_enabled(mac_handle, roamMode);
		sme_update_is_fast_roam_ini_feature_enabled(mac_handle,
							    adapter->session_id,
							    roamMode);
	} else {
		sme_update_is_fast_roam_ini_feature_enabled(mac_handle,
							    adapter->session_id,
							    roamMode);
		hdd_ctx->config->isRoamOffloadScanEnabled = roamMode;
		sme_update_roam_scan_offload_enabled(mac_handle, roamMode);
	}


exit:
	return ret;
}

static int drv_cmd_set_suspend_mode(struct hdd_adapter *adapter,
				    struct hdd_context *hdd_ctx,
				    uint8_t *command,
				    uint8_t command_len,
				    struct hdd_priv_data *priv_data)
{
	int errno;
	uint8_t *value = command;
	QDF_STATUS status;
	uint8_t idle_monitor;

	if (QDF_STA_MODE != adapter->device_mode) {
		hdd_debug("Non-STA interface");
		return 0;
	}

	/* Move pointer to ahead of SETSUSPENDMODE<delimiter> */
	value = value + SIZE_OF_SETSUSPENDMODE + 1;

	/* Convert the value from ascii to integer */
	errno = kstrtou8(value, 10, &idle_monitor);
	if (errno < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("Range validation failed");
		return -EINVAL;
	}

	hdd_debug("idle_monitor:%d", idle_monitor);
	status = ucfg_pmo_tgt_psoc_send_idle_roam_suspend_mode(hdd_ctx->psoc,
							       idle_monitor);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("Send suspend mode to fw failed");
		return -EINVAL;
	}
	return 0;
}

static int drv_cmd_get_roam_mode(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	bool roamMode = sme_get_is_lfr_feature_enabled(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len;

	/*
	 * roamMode value shall be inverted because the sementics is different.
	 */
	if (CFG_LFR_FEATURE_ENABLED_MIN == roamMode)
		roamMode = CFG_LFR_FEATURE_ENABLED_MAX;
	else
		roamMode = CFG_LFR_FEATURE_ENABLED_MIN;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, roamMode);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_delta(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	uint8_t roamRssiDiff = CFG_ROAM_RSSI_DIFF_DEFAULT;

	/* Move pointer to ahead of SETROAMDELTA<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &roamRssiDiff);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			CFG_ROAM_RSSI_DIFF_MIN,
			CFG_ROAM_RSSI_DIFF_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((roamRssiDiff < CFG_ROAM_RSSI_DIFF_MIN) ||
	    (roamRssiDiff > CFG_ROAM_RSSI_DIFF_MAX)) {
		hdd_err("Roam rssi diff value %d is out of range (Min: %d Max: %d)",
			roamRssiDiff,
			CFG_ROAM_RSSI_DIFF_MIN,
			CFG_ROAM_RSSI_DIFF_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set roam rssi diff = %d",
		  roamRssiDiff);

	hdd_ctx->config->RoamRssiDiff = roamRssiDiff;
	sme_update_roam_rssi_diff(hdd_ctx->mac_handle,
				  adapter->session_id,
				  roamRssiDiff);

exit:
	return ret;
}

static int drv_cmd_get_roam_delta(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t rssi_diff;
	char extra[32];
	uint8_t len;
	QDF_STATUS status;

	status = sme_get_roam_rssi_diff(hdd_ctx->mac_handle,
					adapter->session_id,
					&rssi_diff);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	hdd_debug("vdev_id: %u, rssi_diff: %u", adapter->session_id, rssi_diff);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_GETROAMDELTA_IOCTL,
		   adapter->session_id, rssi_diff);

	len = scnprintf(extra, sizeof(extra), "%s %d",
			command, rssi_diff);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_get_band(struct hdd_adapter *adapter,
			    struct hdd_context *hdd_ctx,
			    uint8_t *command,
			    uint8_t command_len,
			    struct hdd_priv_data *priv_data)
{
	int ret = 0;
	int band = -1;
	char extra[32];
	uint8_t len = 0;

	hdd_get_band_helper(hdd_ctx, &band);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_GETBAND_IOCTL,
		   adapter->session_id, band);

	len = scnprintf(extra, sizeof(extra), "%s %d", command, band);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_scan_channels(struct hdd_adapter *adapter,
					  struct hdd_context *hdd_ctx,
					  uint8_t *command,
					  uint8_t command_len,
					  struct hdd_priv_data *priv_data)
{
	return hdd_parse_set_roam_scan_channels(adapter, command);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static bool is_roam_ch_from_fw_supported(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->roam_ch_from_fw_supported;
}

struct roam_ch_priv {
	struct roam_scan_ch_resp roam_ch;
};

void hdd_get_roam_scan_ch_cb(hdd_handle_t hdd_handle,
			     struct roam_scan_ch_resp *roam_ch,
			     void *context)
{
	struct osif_request *request;
	struct roam_ch_priv *priv;
	uint8_t *event = NULL, i = 0;
	uint32_t  *freq = NULL, len;
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);

	hdd_debug("roam scan ch list event received : vdev_id:%d command resp: %d",
		  roam_ch->vdev_id, roam_ch->command_resp);
	/**
	 * If command response is set in the response message, then it is
	 * getroamscanchannels command response else this event is asyncronous
	 * event raised by firmware.
	 */
	if (!roam_ch->command_resp) {
		len = roam_ch->num_channels * sizeof(roam_ch->chan_list[0]);
		if (!len) {
			hdd_err("Invalid len");
			return;
		}
		event = (uint8_t *)qdf_mem_malloc(len);
		if (!event) {
			hdd_err("Failed to alloc event response buf vdev_id: %d",
				roam_ch->vdev_id);
			return;
		}
		freq = (uint32_t *)event;
		for (i = 0; i < roam_ch->num_channels &&
		     i < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++) {
			freq[i] = roam_ch->chan_list[i];
		}

		hdd_send_roam_scan_ch_list_event(hdd_ctx, roam_ch->vdev_id,
						 len, event);
		qdf_mem_free(event);
		return;
	}

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}
	priv = osif_request_priv(request);

	priv->roam_ch.num_channels = roam_ch->num_channels;
	for (i = 0; i < priv->roam_ch.num_channels &&
	     i < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++)
		priv->roam_ch.chan_list[i] = roam_ch->chan_list[i];

	osif_request_complete(request);
	osif_request_put(request);
}

static uint32_t
hdd_get_roam_chan_from_fw(struct hdd_adapter *adapter,
			  uint8_t *chan_list, uint8_t *num_channels)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct hdd_context *hdd_ctx;
	int ret, i;
	void *cookie;
	struct osif_request *request;
	struct roam_ch_priv *priv;
	struct roam_scan_ch_resp *p_roam_ch;
	struct hdd_station_ctx *hdd_sta_ctx;
	static const struct osif_request_params params = {
			.priv_size = sizeof(*priv) +
				     sizeof(priv->roam_ch.chan_list[0]) *
				     WNI_CFG_VALID_CHANNEL_LIST_LEN,
			.timeout_ms = WLAN_WAIT_TIME_STATS,
	};

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}

	priv = osif_request_priv(request);
	p_roam_ch = &priv->roam_ch;
	/** channel list starts after response structure*/
	priv->roam_ch.chan_list = (uint32_t *)(p_roam_ch + 1);
	cookie = osif_request_cookie(request);
	status = sme_get_roam_scan_ch(hdd_ctx->mac_handle,
				      adapter->session_id, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to retrieve roam channels");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("SME timed out while retrieving raom channels");
		goto cleanup;
	}

	*num_channels = priv->roam_ch.num_channels;
	for (i = 0; i < *num_channels &&
	     i < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++)
		chan_list[i] = wlan_reg_freq_to_chan(
				hdd_ctx->pdev, priv->roam_ch.chan_list[i]);

cleanup:
	osif_request_put(request);

	return ret;
}
#else
static bool is_roam_ch_from_fw_supported(struct hdd_context *hdd_ctx)
{
	return false;
}

static uint32_t
hdd_get_roam_chan_from_fw(struct hdd_adapter *adapter,
			  uint8_t *chan_list, uint8_t *num_channels)
{
	return QDF_STATUS_E_INVAL;
}
#endif

static int drv_cmd_get_roam_scan_channels(struct hdd_adapter *adapter,
					  struct hdd_context *hdd_ctx,
					  uint8_t *command,
					  uint8_t command_len,
					  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t numChannels = 0;
	uint8_t j = 0;
	char extra[128] = { 0 };
	int len;

	if (is_roam_ch_from_fw_supported(hdd_ctx)) {
		ret = hdd_get_roam_chan_from_fw(adapter, ChannelList,
						&numChannels);
		if (ret == QDF_STATUS_SUCCESS) {
			goto fill_ch_resp;
		} else {
			hdd_err("failed to get roam scan channel list from FW");
			ret = -EFAULT;
			goto exit;
		}
	}

	if (QDF_STATUS_SUCCESS !=
		sme_get_roam_scan_channel_list(hdd_ctx->mac_handle,
					       ChannelList,
					       &numChannels,
					       adapter->session_id)) {
		hdd_err("failed to get roam scan channel list");
		ret = -EFAULT;
		goto exit;
	}

fill_ch_resp:
	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_GETROAMSCANCHANNELS_IOCTL,
		   adapter->session_id, numChannels);

	/*
	 * output channel list is of the format
	 * [Number of roam scan channels][Channel1][Channel2]...
	 * copy the number of channels in the 0th index
	 */
	len = scnprintf(extra, sizeof(extra), "%s %d", command,
			numChannels);
	for (j = 0; (j < numChannels) && len <= sizeof(extra); j++)
		len += scnprintf(extra + len, sizeof(extra) - len,
				 " %d", ChannelList[j]);

	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_get_ccx_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	mac_handle_t mac_handle = hdd_ctx->mac_handle;
	bool eseMode = sme_get_is_ese_feature_enabled(mac_handle);
	char extra[32];
	uint8_t len = 0;
	struct pmkid_mode_bits pmkid_modes;

	hdd_get_pmkid_modes(hdd_ctx, &pmkid_modes);
	/*
	 * Check if the features PMKID/ESE/11R are supported simultaneously,
	 * then this operation is not permitted (return FAILURE)
	 */
	if (eseMode &&
	    (pmkid_modes.fw_okc || pmkid_modes.fw_pmksa_cache) &&
	    sme_get_is_ft_feature_enabled(mac_handle)) {
		hdd_warn("PMKID/ESE/11R are supported simultaneously hence this operation is not permitted!");
		ret = -EPERM;
		goto exit;
	}

	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETCCXMODE", eseMode);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_get_okc_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	struct pmkid_mode_bits pmkid_modes;
	char extra[32];
	uint8_t len = 0;
	mac_handle_t mac_handle = hdd_ctx->mac_handle;

	hdd_get_pmkid_modes(hdd_ctx, &pmkid_modes);
	/*
	 * Check if the features OKC/ESE/11R are supported simultaneously,
	 * then this operation is not permitted (return FAILURE)
	 */
	if (pmkid_modes.fw_okc &&
	    sme_get_is_ese_feature_enabled(mac_handle) &&
	    sme_get_is_ft_feature_enabled(mac_handle)) {
		hdd_warn("PMKID/ESE/11R are supported simultaneously hence this operation is not permitted!");
		ret = -EPERM;
		goto exit;
	}

	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETOKCMODE", pmkid_modes.fw_okc);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_get_fast_roam(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	bool lfrMode = sme_get_is_lfr_feature_enabled(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETFASTROAM", lfrMode);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_get_fast_transition(struct hdd_adapter *adapter,
				       struct hdd_context *hdd_ctx,
				       uint8_t *command,
				       uint8_t command_len,
				       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	bool ft = sme_get_is_ft_feature_enabled(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d",
					"GETFASTTRANSITION", ft);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_scan_channel_min_time(struct hdd_adapter *adapter,
						  struct hdd_context *hdd_ctx,
						  uint8_t *command,
						  uint8_t command_len,
						  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t minTime = CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT;

	/* Move pointer to ahead of SETROAMSCANCHANNELMINTIME<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &minTime);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
			CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((minTime < CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN) ||
	    (minTime > CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX)) {
		hdd_err("scan min channel time value %d is out of range (Min: %d Max: %d)",
			minTime,
			CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
			CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_SETROAMSCANCHANNELMINTIME_IOCTL,
		   adapter->session_id, minTime);
	hdd_debug("Received Command to change channel min time = %d",
		  minTime);

	hdd_ctx->config->nNeighborScanMinChanTime = minTime;
	sme_set_neighbor_scan_min_chan_time(hdd_ctx->mac_handle,
					    minTime,
					    adapter->session_id);

exit:
	return ret;
}

static int drv_cmd_send_action_frame(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx,
				     uint8_t *command,
				     uint8_t command_len,
				     struct hdd_priv_data *priv_data)
{
	return hdd_parse_sendactionframe(adapter, command,
					 priv_data->total_len);
}

static int drv_cmd_get_roam_scan_channel_min_time(struct hdd_adapter *adapter,
						  struct hdd_context *hdd_ctx,
						  uint8_t *command,
						  uint8_t command_len,
						  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t val = sme_get_neighbor_scan_min_chan_time(hdd_ctx->mac_handle,
							   adapter->session_id);
	char extra[32];
	uint8_t len = 0;

	/* value is interms of msec */
	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETROAMSCANCHANNELMINTIME", val);
	len = QDF_MIN(priv_data->total_len, len + 1);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_GETROAMSCANCHANNELMINTIME_IOCTL,
		   adapter->session_id, val);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_scan_channel_time(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint16_t maxTime = CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT;

	/* Move pointer to ahead of SETSCANCHANNELTIME<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou16(value, 10, &maxTime);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou16 failed range [%d - %d]",
			CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
			CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((maxTime < CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN) ||
	    (maxTime > CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX)) {
		hdd_err("lfr mode value %d is out of range (Min: %d Max: %d)",
			maxTime,
			CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
			CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to change channel max time = %d",
		  maxTime);

	hdd_ctx->config->nNeighborScanMaxChanTime = maxTime;
	sme_set_neighbor_scan_max_chan_time(hdd_ctx->mac_handle,
					    adapter->session_id,
					    maxTime);

exit:
	return ret;
}

static int drv_cmd_get_scan_channel_time(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t val = sme_get_neighbor_scan_max_chan_time(hdd_ctx->mac_handle,
							   adapter->session_id);
	char extra[32];
	uint8_t len = 0;

	/* value is interms of msec */
	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETSCANCHANNELTIME", val);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_scan_home_time(struct hdd_adapter *adapter,
				      struct hdd_context *hdd_ctx,
				      uint8_t *command,
				      uint8_t command_len,
				      struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint16_t val = CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT;

	/* Move pointer to ahead of SETSCANHOMETIME<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou16(value, 10, &val);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou16 failed range [%d - %d]",
			CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
			CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((val < CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN) ||
	    (val > CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX)) {
		hdd_err("scan home time value %d is out of range (Min: %d Max: %d)",
			val,
			CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
			CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to change scan home time = %d",
		  val);

	hdd_ctx->config->nNeighborScanPeriod = val;
	sme_set_neighbor_scan_period(hdd_ctx->mac_handle,
				     adapter->session_id, val);

exit:
	return ret;
}

static int drv_cmd_get_scan_home_time(struct hdd_adapter *adapter,
				      struct hdd_context *hdd_ctx,
				      uint8_t *command,
				      uint8_t command_len,
				      struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t val = sme_get_neighbor_scan_period(hdd_ctx->mac_handle,
						    adapter->session_id);
	char extra[32];
	uint8_t len = 0;

	/* value is interms of msec */
	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETSCANHOMETIME", val);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_intra_band(struct hdd_adapter *adapter,
				       struct hdd_context *hdd_ctx,
				       uint8_t *command,
				       uint8_t command_len,
				       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t val = CFG_ROAM_INTRA_BAND_DEFAULT;

	/* Move pointer to ahead of SETROAMINTRABAND<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &val);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			CFG_ROAM_INTRA_BAND_MIN,
			CFG_ROAM_INTRA_BAND_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((val < CFG_ROAM_INTRA_BAND_MIN) ||
	    (val > CFG_ROAM_INTRA_BAND_MAX)) {
		hdd_err("intra band mode value %d is out of range (Min: %d Max: %d)",
			val,
			CFG_ROAM_INTRA_BAND_MIN,
			CFG_ROAM_INTRA_BAND_MAX);
		ret = -EINVAL;
		goto exit;
	}
	hdd_debug("Received Command to change intra band = %d",
		  val);

	hdd_ctx->config->nRoamIntraBand = val;
	sme_set_roam_intra_band(hdd_ctx->mac_handle, val);
	policy_mgr_set_pcl_for_existing_combo(
					hdd_ctx->psoc,
					PM_STA_MODE);

exit:
	return ret;
}

static int drv_cmd_get_roam_intra_band(struct hdd_adapter *adapter,
				       struct hdd_context *hdd_ctx,
				       uint8_t *command,
				       uint8_t command_len,
				       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t val = sme_get_roam_intra_band(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	/* value is interms of msec */
	len = scnprintf(extra, sizeof(extra), "%s %d",
			"GETROAMINTRABAND", val);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_scan_n_probes(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx,
				     uint8_t *command,
				     uint8_t command_len,
				     struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t nProbes = CFG_ROAM_SCAN_N_PROBES_DEFAULT;

	/* Move pointer to ahead of SETSCANNPROBES<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &nProbes);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			CFG_ROAM_SCAN_N_PROBES_MIN,
			CFG_ROAM_SCAN_N_PROBES_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((nProbes < CFG_ROAM_SCAN_N_PROBES_MIN) ||
	    (nProbes > CFG_ROAM_SCAN_N_PROBES_MAX)) {
		hdd_err("NProbes value %d is out of range (Min: %d Max: %d)",
			nProbes,
			CFG_ROAM_SCAN_N_PROBES_MIN,
			CFG_ROAM_SCAN_N_PROBES_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set nProbes = %d",
		  nProbes);

	hdd_ctx->config->nProbes = nProbes;
	sme_update_roam_scan_n_probes(hdd_ctx->mac_handle,
				      adapter->session_id, nProbes);

exit:
	return ret;
}

static int drv_cmd_get_scan_n_probes(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx,
				     uint8_t *command,
				     uint8_t command_len,
				     struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t val;
	char extra[32];
	uint8_t len = 0;
	QDF_STATUS status;

	status = sme_get_roam_scan_n_probes(hdd_ctx->mac_handle,
					    adapter->session_id,
					    &val);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	hdd_debug("vdev_id: %u, scan_n_probes: %u",
		  adapter->session_id, val);

	len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_scan_home_away_time(struct hdd_adapter *adapter,
					   struct hdd_context *hdd_ctx,
					   uint8_t *command,
					   uint8_t command_len,
					   struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint16_t homeAwayTime = CFG_ROAM_SCAN_HOME_AWAY_TIME_DEFAULT;

	/* input value is in units of msec */

	/* Move pointer to ahead of SETSCANHOMEAWAYTIME<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou16(value, 10, &homeAwayTime);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
			CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((homeAwayTime < CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN) ||
	    (homeAwayTime > CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX)) {
		hdd_err("homeAwayTime value %d is out of range (Min: %d Max: %d)",
			  homeAwayTime,
			  CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
			  CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set scan away time = %d",
		  homeAwayTime);

	sme_update_roam_scan_home_away_time(hdd_ctx->mac_handle,
					    adapter->session_id,
					    homeAwayTime,
					    true);

exit:
	return ret;
}

static int drv_cmd_get_scan_home_away_time(struct hdd_adapter *adapter,
					   struct hdd_context *hdd_ctx,
					   uint8_t *command,
					   uint8_t command_len,
					   struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint16_t val;
	char extra[32] = {0};
	uint8_t len = 0;
	QDF_STATUS status;

	status = sme_get_roam_scan_home_away_time(hdd_ctx->mac_handle,
						  adapter->session_id,
						  &val);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	hdd_debug("vdev_id: %u, scan home away time: %u",
		  adapter->session_id, val);
	len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_reassoc(struct hdd_adapter *adapter,
			   struct hdd_context *hdd_ctx,
			   uint8_t *command,
			   uint8_t command_len,
			   struct hdd_priv_data *priv_data)
{
	return hdd_parse_reassoc(adapter, command, priv_data->total_len);
}

static int drv_cmd_set_wes_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t wesMode = CFG_ENABLE_WES_MODE_NAME_DEFAULT;

	/* Move pointer to ahead of SETWESMODE<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &wesMode);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			  CFG_ENABLE_WES_MODE_NAME_MIN,
			  CFG_ENABLE_WES_MODE_NAME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((wesMode < CFG_ENABLE_WES_MODE_NAME_MIN) ||
	    (wesMode > CFG_ENABLE_WES_MODE_NAME_MAX)) {
		hdd_err("WES Mode value %d is out of range (Min: %d Max: %d)",
			  wesMode,
			  CFG_ENABLE_WES_MODE_NAME_MIN,
			  CFG_ENABLE_WES_MODE_NAME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set WES Mode rssi diff = %d",
		  wesMode);

	hdd_ctx->config->isWESModeEnabled = wesMode;
	sme_update_wes_mode(hdd_ctx->mac_handle, wesMode, adapter->session_id);

exit:
	return ret;
}

static int drv_cmd_get_wes_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	bool wesMode = sme_get_wes_mode(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, wesMode);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_opportunistic_rssi_diff(struct hdd_adapter *adapter,
					       struct hdd_context *hdd_ctx,
					       uint8_t *command,
					       uint8_t command_len,
					       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t nOpportunisticThresholdDiff =
		CFG_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF_DEFAULT;

	/* Move pointer to ahead of SETOPPORTUNISTICRSSIDIFF<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &nOpportunisticThresholdDiff);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed");
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set Opportunistic Threshold diff = %d",
		  nOpportunisticThresholdDiff);

	sme_set_roam_opportunistic_scan_threshold_diff(hdd_ctx->mac_handle,
				adapter->session_id,
				nOpportunisticThresholdDiff);

exit:
	return ret;
}

static int drv_cmd_get_opportunistic_rssi_diff(struct hdd_adapter *adapter,
					       struct hdd_context *hdd_ctx,
					       uint8_t *command,
					       uint8_t command_len,
					       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	mac_handle_t mac_handle = hdd_ctx->mac_handle;
	int8_t val = sme_get_roam_opportunistic_scan_threshold_diff(mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_roam_rescan_rssi_diff(struct hdd_adapter *adapter,
					     struct hdd_context *hdd_ctx,
					     uint8_t *command,
					     uint8_t command_len,
					     struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t nRoamRescanRssiDiff = CFG_ROAM_RESCAN_RSSI_DIFF_DEFAULT;

	/* Move pointer to ahead of SETROAMRESCANRSSIDIFF<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &nRoamRescanRssiDiff);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed");
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set Roam Rescan RSSI Diff = %d",
		  nRoamRescanRssiDiff);

	sme_set_roam_rescan_rssi_diff(hdd_ctx->mac_handle,
				      adapter->session_id,
				      nRoamRescanRssiDiff);

exit:
	return ret;
}

static int drv_cmd_get_roam_rescan_rssi_diff(struct hdd_adapter *adapter,
					     struct hdd_context *hdd_ctx,
					     uint8_t *command,
					     uint8_t command_len,
					     struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t val = sme_get_roam_rescan_rssi_diff(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, val);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_set_fast_roam(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t lfrMode = CFG_LFR_FEATURE_ENABLED_DEFAULT;

	/* Move pointer to ahead of SETFASTROAM<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &lfrMode);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			  CFG_LFR_FEATURE_ENABLED_MIN,
			  CFG_LFR_FEATURE_ENABLED_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((lfrMode < CFG_LFR_FEATURE_ENABLED_MIN) ||
	    (lfrMode > CFG_LFR_FEATURE_ENABLED_MAX)) {
		hdd_err("lfr mode value %d is out of range (Min: %d Max: %d)",
			  lfrMode,
			  CFG_LFR_FEATURE_ENABLED_MIN,
			  CFG_LFR_FEATURE_ENABLED_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to change lfr mode = %d",
		  lfrMode);

	hdd_ctx->config->isFastRoamIniFeatureEnabled = lfrMode;
	sme_update_is_fast_roam_ini_feature_enabled(hdd_ctx->mac_handle,
						    adapter->session_id,
						    lfrMode);

exit:
	return ret;
}

static int drv_cmd_set_fast_transition(struct hdd_adapter *adapter,
				       struct hdd_context *hdd_ctx,
				       uint8_t *command,
				       uint8_t command_len,
				       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t ft = CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT;

	/* Move pointer to ahead of SETFASTROAM<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &ft);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			  CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
			  CFG_FAST_TRANSITION_ENABLED_NAME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((ft < CFG_FAST_TRANSITION_ENABLED_NAME_MIN) ||
	    (ft > CFG_FAST_TRANSITION_ENABLED_NAME_MAX)) {
		hdd_err("ft mode value %d is out of range (Min: %d Max: %d)",
			  ft,
			  CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
			  CFG_FAST_TRANSITION_ENABLED_NAME_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to change ft mode = %d", ft);

	hdd_ctx->config->isFastTransitionEnabled = ft;
	sme_update_fast_transition_enabled(hdd_ctx->mac_handle, ft);

exit:
	return ret;
}

static int drv_cmd_fast_reassoc(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t channel = 0;
	tSirMacAddr targetApBssid;
	uint32_t roamId = INVALID_ROAM_ID;
	tCsrRoamModifyProfileFields modProfileFields;
	tCsrHandoffRequest handoffInfo;
	struct hdd_station_ctx *sta_ctx;
	mac_handle_t mac_handle;

	if (QDF_STA_MODE != adapter->device_mode) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	/* if not associated, no need to proceed with reassoc */
	if (eConnectionState_Associated != sta_ctx->conn_info.connState) {
		hdd_warn("Not associated!");
		ret = -EINVAL;
		goto exit;
	}

	ret = hdd_parse_reassoc_command_v1_data(value, targetApBssid,
						&channel);
	if (ret) {
		hdd_err("Failed to parse reassoc command data");
		goto exit;
	}

	mac_handle = hdd_ctx->mac_handle;

	/*
	 * if the target bssid is same as currently associated AP,
	 * issue reassoc to same AP
	 */
	if (!qdf_mem_cmp(targetApBssid,
				    sta_ctx->conn_info.bssId.bytes,
				    QDF_MAC_ADDR_SIZE)) {
		hdd_warn("Reassoc BSSID is same as currently associated AP bssid");
		if (roaming_offload_enabled(hdd_ctx)) {
			hdd_wma_send_fastreassoc_cmd(adapter,
				targetApBssid,
				sta_ctx->conn_info.operationChannel);
		} else {
			sme_get_modify_profile_fields(mac_handle,
				adapter->session_id,
				&modProfileFields);
			sme_roam_reassoc(mac_handle, adapter->session_id,
				NULL, modProfileFields, &roamId, 1);
		}
		return 0;
	}

	/* Check channel number is a valid channel number */
	if (channel && (QDF_STATUS_SUCCESS !=
		wlan_hdd_validate_operation_channel(adapter, channel))) {
		hdd_err("Invalid Channel [%d]", channel);
		return -EINVAL;
	}

	if (roaming_offload_enabled(hdd_ctx)) {
		hdd_wma_send_fastreassoc_cmd(adapter,
					targetApBssid, (int)channel);
		goto exit;
	}
	/* Proceed with reassoc */
	handoffInfo.channel = channel;
	handoffInfo.src = FASTREASSOC;
	qdf_mem_copy(handoffInfo.bssid.bytes, targetApBssid,
		     sizeof(tSirMacAddr));
	sme_handoff_request(mac_handle, adapter->session_id,
			    &handoffInfo);
exit:
	return ret;
}

static int drv_cmd_set_roam_scan_control(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t roamScanControl = 0;

	/* Move pointer to ahead of SETROAMSCANCONTROL<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &roamScanControl);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed");
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set roam scan control = %d",
		  roamScanControl);

	if (0 != roamScanControl) {
		ret = 0; /* return success but ignore param value "true" */
		goto exit;
	}

	sme_set_roam_scan_control(hdd_ctx->mac_handle,
				  adapter->session_id,
				  roamScanControl);

exit:
	return ret;
}

static int drv_cmd_set_okc_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint32_t okc_mode;
	struct pmkid_mode_bits pmkid_modes;
	mac_handle_t mac_handle;

	hdd_get_pmkid_modes(hdd_ctx, &pmkid_modes);

	/*
	 * Check if the features PMKID/ESE/11R are supported simultaneously,
	 * then this operation is not permitted (return FAILURE)
	 */
	mac_handle = hdd_ctx->mac_handle;
	if (sme_get_is_ese_feature_enabled(mac_handle) &&
	    pmkid_modes.fw_okc &&
	    sme_get_is_ft_feature_enabled(mac_handle)) {
		hdd_warn("PMKID/ESE/11R are supported simultaneously hence this operation is not permitted!");
		ret = -EPERM;
		goto exit;
	}

	/* Move pointer to ahead of SETOKCMODE<delimiter> */
	value = value + command_len + 1;

	/* get the current configured value */
	okc_mode = hdd_ctx->config->pmkid_modes & CFG_PMKID_MODES_OKC;

	/* Convert the value from ascii to integer */
	ret = kstrtou32(value, 10, &okc_mode);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("value out of range [0 - 1]");
		ret = -EINVAL;
		goto exit;
	}

	if ((okc_mode < 0) ||
	    (okc_mode > 1)) {
		hdd_err("Okc mode value %d is out of range (Min: 0 Max: 1)",
			  okc_mode);
		ret = -EINVAL;
		goto exit;
	}
	hdd_debug("Received Command to change okc mode = %d",
		  okc_mode);

	if (okc_mode)
		hdd_ctx->config->pmkid_modes |= CFG_PMKID_MODES_OKC;
	else
		hdd_ctx->config->pmkid_modes &= ~CFG_PMKID_MODES_OKC;

exit:
	return ret;
}

static int drv_cmd_get_roam_scan_control(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	bool roamScanControl = sme_get_roam_scan_control(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d",
			command, roamScanControl);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_bt_coex_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	char *bcMode;

	bcMode = command + 11;
	if ('1' == *bcMode) {
		hdd_debug("BTCOEXMODE %d", *bcMode);
		hdd_ctx->bt_coex_mode_set = true;
		ret = wlan_hdd_scan_abort(adapter);
		if (ret < 0) {
			hdd_err("Failed to abort existing scan status: %d",
				ret);
		}
	} else if ('2' == *bcMode) {
		hdd_debug("BTCOEXMODE %d", *bcMode);
		hdd_ctx->bt_coex_mode_set = false;
	}

	return ret;
}

static int drv_cmd_scan_active(struct hdd_adapter *adapter,
			       struct hdd_context *hdd_ctx,
			       uint8_t *command,
			       uint8_t command_len,
			       struct hdd_priv_data *priv_data)
{
	hdd_ctx->ioctl_scan_mode = eSIR_ACTIVE_SCAN;
	return 0;
}

static int drv_cmd_scan_passive(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	hdd_ctx->ioctl_scan_mode = eSIR_PASSIVE_SCAN;
	return 0;
}

static int drv_cmd_get_dwell_time(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	char extra[32];
	uint8_t len = 0;

	memset(extra, 0, sizeof(extra));
	ret = hdd_get_dwell_time(hdd_ctx, command, extra, sizeof(extra), &len);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (ret != 0 || copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
		goto exit;
	}
	ret = len;
exit:
	return ret;
}

static int drv_cmd_set_dwell_time(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	return hdd_set_dwell_time(adapter, command);
}

static int drv_cmd_conc_set_dwell_time(struct hdd_adapter *adapter,
				       struct hdd_context *hdd_ctx,
				       u8 *command,
				       u8 command_len,
				       struct hdd_priv_data *priv_data)
{
	return hdd_conc_set_dwell_time(adapter, command);
}

static int drv_cmd_miracast(struct hdd_adapter *adapter,
			    struct hdd_context *hdd_ctx,
			    uint8_t *command,
			    uint8_t command_len,
			    struct hdd_priv_data *priv_data)
{
	QDF_STATUS ret_status;
	int ret = 0;
	uint8_t filterType = 0;
	uint8_t *value;

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	value = command + 9;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &filterType);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range");
		ret = -EINVAL;
		goto exit;
	}
	if ((filterType < WLAN_HDD_DRIVER_MIRACAST_CFG_MIN_VAL)
	    || (filterType >
		WLAN_HDD_DRIVER_MIRACAST_CFG_MAX_VAL)) {
		hdd_err("Accepted Values are 0 to 2. 0-Disabled, 1-Source, 2-Sink");
		ret = -EINVAL;
		goto exit;
	}
	/* Filtertype value should be either 0-Disabled, 1-Source, 2-sink */
	hdd_ctx->miracast_value = filterType;

	ret_status = sme_set_miracast(hdd_ctx->mac_handle, filterType);
	if (QDF_STATUS_SUCCESS != ret_status) {
		hdd_err("Failed to set miracast");
		return -EBUSY;
	}
	ret_status = ucfg_scan_set_miracast(hdd_ctx->psoc,
					    filterType ? true : false);
	if (QDF_IS_STATUS_ERROR(ret_status)) {
		hdd_err("Failed to set miracastn scan");
		return -EBUSY;
	}

	if (policy_mgr_is_mcc_in_24G(hdd_ctx->psoc))
		return wlan_hdd_set_mas(adapter, filterType);

exit:
	return ret;
}

/* Function header is left blank intentionally */
static int hdd_parse_set_ibss_oui_data_command(uint8_t *command, uint8_t *ie,
					int32_t *oui_length, int32_t limit)
{
	uint8_t len;
	uint8_t data;

	while ((SPACE_ASCII_VALUE == *command) && ('\0' != *command)) {
		command++;
		limit--;
	}

	len = 2;

	while ((SPACE_ASCII_VALUE != *command) && ('\0' != *command) &&
		(limit > 1)) {
		sscanf(command, "%02x", (unsigned int *)&data);
		ie[len++] = data;
		command += 2;
		limit -= 2;
	}

	*oui_length = len - 2;

	while ((SPACE_ASCII_VALUE == *command) && ('\0' != *command)) {
		command++;
		limit--;
	}

	while ((SPACE_ASCII_VALUE != *command) && ('\0' != *command) &&
		(limit > 1)) {
		sscanf(command, "%02x", (unsigned int *)&data);
		ie[len++] = data;
		command += 2;
		limit -= 2;
	}

	ie[0] = IE_EID_VENDOR;
	ie[1] = len - 2;

	return len;
}

/**
 * drv_cmd_set_ibss_beacon_oui_data() - set ibss oui data command
 * @adapter: Pointer to adapter
 * @hdd_ctx: Pointer to HDD context
 * @command: Pointer to command string
 * @command_len : Command length
 * @priv_data : Pointer to priv data
 *
 * Return:
 *      int status code
 */
static int drv_cmd_set_ibss_beacon_oui_data(struct hdd_adapter *adapter,
					    struct hdd_context *hdd_ctx,
					    uint8_t *command,
					    uint8_t command_len,
					    struct hdd_priv_data *priv_data)
{
	int i = 0;
	int status;
	int ret = 0;
	uint8_t *ibss_ie;
	int32_t oui_length = 0;
	uint32_t ibss_ie_length;
	uint8_t *value = command;
	tSirModifyIE ibssModifyIE;
	struct csr_roam_profile *roam_profile;
	mac_handle_t mac_handle;

	if (QDF_IBSS_MODE != adapter->device_mode) {
		hdd_debug("Device_mode %s(%d) not IBSS",
			  hdd_device_mode_to_string(adapter->device_mode),
			  adapter->device_mode);
		return ret;
	}

	hdd_debug("received command %s", ((char *)value));

	/* validate argument of command */
	if (strlen(value) <= command_len) {
		hdd_err("No arguments in command length %zu",
			 strlen(value));
		ret = -EFAULT;
		goto exit;
	}

	/* moving to arguments of commands */
	value = value + command_len;
	command_len = strlen(value);

	/* oui_data can't be less than 3 bytes */
	if (command_len < (2 * WLAN_HDD_IBSS_MIN_OUI_DATA_LENGTH)) {
		hdd_err("Invalid SETIBSSBEACONOUIDATA command length %d",
			 command_len);
		ret = -EFAULT;
		goto exit;
	}

	ibss_ie = qdf_mem_malloc(command_len);
	if (!ibss_ie) {
		hdd_err("Could not allocate memory for command length %d",
			 command_len);
		ret = -ENOMEM;
		goto exit;
	}

	ibss_ie_length = hdd_parse_set_ibss_oui_data_command(value, ibss_ie,
							     &oui_length,
							     command_len);
	if (ibss_ie_length <= (2 * WLAN_HDD_IBSS_MIN_OUI_DATA_LENGTH)) {
		hdd_err("Could not parse command %s return length %d",
			 value, ibss_ie_length);
		ret = -EFAULT;
		qdf_mem_free(ibss_ie);
		goto exit;
	}

	roam_profile = hdd_roam_profile(adapter);

	qdf_copy_macaddr(&ibssModifyIE.bssid,
		     roam_profile->BSSIDs.bssid);

	ibssModifyIE.smeSessionId = adapter->session_id;
	ibssModifyIE.notify = true;
	ibssModifyIE.ieID = IE_EID_VENDOR;
	ibssModifyIE.ieIDLen = ibss_ie_length;
	ibssModifyIE.ieBufferlength = ibss_ie_length;
	ibssModifyIE.pIEBuffer = ibss_ie;
	ibssModifyIE.oui_length = oui_length;

	hdd_warn("ibss_ie length %d oui_length %d ibss_ie:",
		 ibss_ie_length, oui_length);
	while (i < ibssModifyIE.ieBufferlength)
		hdd_warn("0x%x", ibss_ie[i++]);

	/* Probe Bcn modification */
	mac_handle = hdd_ctx->mac_handle;
	sme_modify_add_ie(mac_handle, &ibssModifyIE, eUPDATE_IE_PROBE_BCN);

	/* Populating probe resp frame */
	sme_modify_add_ie(mac_handle, &ibssModifyIE, eUPDATE_IE_PROBE_RESP);

	qdf_mem_free(ibss_ie);

	status = sme_send_cesium_enable_ind(mac_handle,
					    adapter->session_id);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Could not send cesium enable indication %d",
			  status);
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_set_rmc_enable(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t ucRmcEnable = 0;
	int status;
	mac_handle_t mac_handle;

	if ((QDF_IBSS_MODE != adapter->device_mode) &&
	    (QDF_SAP_MODE != adapter->device_mode)) {
		hdd_err("Received SETRMCENABLE cmd in invalid mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		hdd_err("SETRMCENABLE cmd is allowed only in IBSS/SOFTAP mode");
		ret = -EINVAL;
		goto exit;
	}

	status = hdd_parse_setrmcenable_command(value, &ucRmcEnable);
	if (status) {
		hdd_err("Invalid SETRMCENABLE command");
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("ucRmcEnable %d", ucRmcEnable);
	mac_handle = hdd_ctx->mac_handle;

	if (true == ucRmcEnable) {
		status = sme_enable_rmc(mac_handle, adapter->session_id);
	} else if (false == ucRmcEnable) {
		status = sme_disable_rmc(mac_handle, adapter->session_id);
	} else {
		hdd_err("Invalid SETRMCENABLE command %d", ucRmcEnable);
		ret = -EINVAL;
		goto exit;
	}

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("SETRMC %d failed status %d", ucRmcEnable, status);
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_set_rmc_action_period(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint32_t uActionPeriod = 0;
	int status;
	mac_handle_t mac_handle;

	if ((QDF_IBSS_MODE != adapter->device_mode) &&
	    (QDF_SAP_MODE != adapter->device_mode)) {
		hdd_err("Received SETRMC cmd in invalid mode %s(%d)",
			hdd_device_mode_to_string(adapter->device_mode),
			adapter->device_mode);
		hdd_err("SETRMC cmd is allowed only in IBSS/SOFTAP mode");
		ret = -EINVAL;
		goto exit;
	}

	status = hdd_parse_setrmcactionperiod_command(value, &uActionPeriod);
	if (status) {
		hdd_err("Invalid SETRMCACTIONPERIOD command");
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("uActionPeriod %d", uActionPeriod);
	mac_handle = hdd_ctx->mac_handle;

	if (sme_cfg_set_int(mac_handle,
			    WNI_CFG_RMC_ACTION_PERIOD_FREQUENCY,
			    uActionPeriod)) {
		hdd_err("Could not set SETRMCACTIONPERIOD %d",
			  uActionPeriod);
		ret = -EINVAL;
		goto exit;
	}

	status = sme_send_rmc_action_period(mac_handle,
					    adapter->session_id);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Could not send cesium enable indication %d",
			  status);
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_get_ibss_peer_info_all(struct hdd_adapter *adapter,
					  struct hdd_context *hdd_ctx,
					  uint8_t *command,
					  uint8_t command_len,
					  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	int status = QDF_STATUS_SUCCESS;
	struct hdd_station_ctx *sta_ctx = NULL;
	char *extra = NULL;
	int idx = 0;
	int length = 0;
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE];
	uint32_t numOfBytestoPrint = 0;

	if (QDF_IBSS_MODE != adapter->device_mode) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	hdd_debug("Received GETIBSSPEERINFOALL Command");

	/* Handle the command */
	status = hdd_cfg80211_get_ibss_peer_info_all(adapter);
	if (QDF_STATUS_SUCCESS == status) {
		size_t user_size = qdf_min(WLAN_MAX_BUF_SIZE,
					   priv_data->total_len);

		/*
		 * The variable extra needed to be allocated on the heap since
		 * amount of memory required to copy the data for 32 devices
		 * exceeds the size of 1024 bytes of default stack size. On
		 * 64 bit devices, the default max stack size of 2048 bytes
		 */
		extra = qdf_mem_malloc(user_size);

		if (NULL == extra) {
			hdd_err("memory allocation failed");
			ret = -ENOMEM;
			goto exit;
		}

		/* Copy number of stations */
		length = scnprintf(extra, user_size, "%d ",
				   sta_ctx->ibss_peer_info.numPeers);
		numOfBytestoPrint = length;
		for (idx = 0; idx < sta_ctx->ibss_peer_info.numPeers;
								idx++) {
			int8_t rssi;
			uint32_t tx_rate;

			qdf_mem_copy(mac_addr,
				sta_ctx->ibss_peer_info.peerInfoParams[idx].
				mac_addr, sizeof(mac_addr));

			tx_rate =
				sta_ctx->ibss_peer_info.peerInfoParams[idx].
									txRate;
			/*
			 * Only lower 3 bytes are rate info. Mask of the MSByte
			 */
			tx_rate &= 0x00FFFFFF;

			rssi = sta_ctx->ibss_peer_info.peerInfoParams[idx].
									rssi;

			length += scnprintf(extra + length,
				user_size - length,
				"%02x:%02x:%02x:%02x:%02x:%02x %d %d ",
				mac_addr[0], mac_addr[1], mac_addr[2],
				mac_addr[3], mac_addr[4], mac_addr[5],
				tx_rate, rssi);
			/*
			 * cdf_trace_msg has limitation of 512 bytes for the
			 * print buffer. Hence printing the data in two chunks.
			 * The first chunk will have the data for 16 devices
			 * and the second chunk will have the rest.
			 */
			if (idx < NUM_OF_STA_DATA_TO_PRINT)
				numOfBytestoPrint = length;
		}

		/*
		 * Copy the data back into buffer, if the data to copy is
		 * more than 512 bytes than we will split the data and do
		 * it in two shots
		 */
		if (copy_to_user(priv_data->buf, extra, numOfBytestoPrint)) {
			hdd_err("Copy into user data buffer failed");
			ret = -EFAULT;
			goto mem_free;
		}

		/* This overwrites the last space, which we already copied */
		extra[numOfBytestoPrint - 1] = '\0';
		hdd_debug("%s", extra);

		if (length > numOfBytestoPrint) {
			if (copy_to_user
				    (priv_data->buf + numOfBytestoPrint,
				    extra + numOfBytestoPrint,
				    length - numOfBytestoPrint + 1)) {
				hdd_err("Copy into user data buffer failed");
				ret = -EFAULT;
				goto mem_free;
			}
			hdd_debug("%s", &extra[numOfBytestoPrint]);
		}
	} else {
		/* Command failed, log error */
		hdd_err("GETIBSSPEERINFOALL command failed with status code %d",
			  status);
		ret = -EINVAL;
		goto exit;
	}
	ret = 0;

mem_free:
	qdf_mem_free(extra);
exit:
	return ret;
}

/* Peer Info <Peer Addr> command */
static int drv_cmd_get_ibss_peer_info(struct hdd_adapter *adapter,
				      struct hdd_context *hdd_ctx,
				      uint8_t *command,
				      uint8_t command_len,
				      struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	QDF_STATUS status;
	struct hdd_station_ctx *sta_ctx = NULL;
	char extra[128] = { 0 };
	uint32_t length = 0;
	uint8_t staIdx = 0;
	struct qdf_mac_addr peerMacAddr;

	if (QDF_IBSS_MODE != adapter->device_mode) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	hdd_debug("Received GETIBSSPEERINFO Command");

	/* if there are no peers, no need to continue with the command */
	if (eConnectionState_IbssConnected !=
	    sta_ctx->conn_info.connState) {
		hdd_err("No IBSS Peers coalesced");
		ret = -EINVAL;
		goto exit;
	}

	/* Parse the incoming command buffer */
	status = hdd_parse_get_ibss_peer_info(value, &peerMacAddr);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Invalid GETIBSSPEERINFO command");
		ret = -EINVAL;
		goto exit;
	}

	/* Get station index for the peer mac address and sanitize it */
	hdd_get_peer_sta_id(sta_ctx, &peerMacAddr, &staIdx);

	if (staIdx > MAX_PEERS) {
		hdd_err("Invalid StaIdx %d returned", staIdx);
		ret = -EINVAL;
		goto exit;
	}

	/* Handle the command */
	status = hdd_cfg80211_get_ibss_peer_info(adapter, staIdx);
	if (QDF_STATUS_SUCCESS == status) {
		uint32_t txRate =
			sta_ctx->ibss_peer_info.peerInfoParams[0].txRate;
		/* Only lower 3 bytes are rate info. Mask of the MSByte */
		txRate &= 0x00FFFFFF;

		length = scnprintf(extra, sizeof(extra), "%d %d",
				(int)txRate,
				(int)sta_ctx->ibss_peer_info.
				peerInfoParams[0].rssi);
		length = QDF_MIN(priv_data->total_len, length + 1);

		/* Copy the data back into buffer */
		if (copy_to_user(priv_data->buf, &extra, length)) {
			hdd_err("copy data to user buffer failed GETIBSSPEERINFO command");
			ret = -EFAULT;
			goto exit;
		}
	} else {
		/* Command failed, log error */
		hdd_err("GETIBSSPEERINFO command failed with status code %d",
			  status);
		ret = -EINVAL;
		goto exit;
	}

	/* Success ! */
	hdd_debug("%s", extra);
	ret = 0;

exit:
	return ret;
}

static int drv_cmd_set_rmc_tx_rate(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   uint8_t *command,
				   uint8_t command_len,
				   struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint32_t uRate = 0;
	enum tx_rate_info txFlags = 0;
	tSirRateUpdateInd rateUpdateParams = {0};
	int status;
	struct hdd_config *pConfig = hdd_ctx->config;

	if ((QDF_IBSS_MODE != adapter->device_mode) &&
	    (QDF_SAP_MODE != adapter->device_mode)) {
		hdd_err("Received SETRMCTXRATE cmd in invalid mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		hdd_err("SETRMCTXRATE cmd is allowed only in IBSS/SOFTAP mode");
		ret = -EINVAL;
		goto exit;
	}

	status = hdd_parse_setrmcrate_command(value, &uRate, &txFlags);
	if (status) {
		hdd_err("Invalid SETRMCTXRATE command");
		ret = -EINVAL;
		goto exit;
	}
	hdd_debug("uRate %d", uRate);
	/* -1 implies ignore this param */
	rateUpdateParams.ucastDataRate = -1;

	/*
	 * Fill the user specifieed RMC rate param
	 * and the derived tx flags.
	 */
	rateUpdateParams.nss = (pConfig->enable2x2 == 0) ? 0 : 1;
	rateUpdateParams.reliableMcastDataRate = uRate;
	rateUpdateParams.reliableMcastDataRateTxFlag = txFlags;
	rateUpdateParams.dev_mode = adapter->device_mode;
	rateUpdateParams.bcastDataRate = -1;
	memcpy(rateUpdateParams.bssid.bytes,
	       adapter->mac_addr.bytes,
	       sizeof(rateUpdateParams.bssid));
	status = sme_send_rate_update_ind(hdd_ctx->mac_handle,
					  &rateUpdateParams);

exit:
	return ret;
}

static int drv_cmd_set_ibss_tx_fail_event(struct hdd_adapter *adapter,
					  struct hdd_context *hdd_ctx,
					  uint8_t *command,
					  uint8_t command_len,
					  struct hdd_priv_data *priv_data)
{
	int ret = 0;
	char *value;
	uint8_t tx_fail_count = 0;
	uint16_t pid = 0;
	mac_handle_t mac_handle;

	value = command;

	ret = hdd_parse_ibsstx_fail_event_params(value, &tx_fail_count, &pid);

	if (0 != ret) {
		hdd_err("Failed to parse SETIBSSTXFAILEVENT arguments");
		goto exit;
	}

	hdd_debug("tx_fail_cnt=%hhu, pid=%hu", tx_fail_count, pid);
	mac_handle = hdd_ctx->mac_handle;

	if (0 == tx_fail_count) {
		/* Disable TX Fail Indication */
		if (QDF_STATUS_SUCCESS ==
		    sme_tx_fail_monitor_start_stop_ind(mac_handle,
						       tx_fail_count,
						       NULL)) {
			cesium_pid = 0;
		} else {
			hdd_err("failed to disable TX Fail Event");
			ret = -EINVAL;
		}
	} else {
		if (QDF_STATUS_SUCCESS ==
		    sme_tx_fail_monitor_start_stop_ind(mac_handle,
				tx_fail_count,
				(void *)hdd_tx_fail_ind_callback)) {
			cesium_pid = pid;
			hdd_debug("Registered Cesium pid %u",
				  cesium_pid);
		} else {
			hdd_err("Failed to enable TX Fail Monitoring");
			ret = -EINVAL;
		}
	}

exit:
	return ret;
}

#ifdef FEATURE_WLAN_ESE
static int drv_cmd_set_ccx_roam_scan_channels(struct hdd_adapter *adapter,
					      struct hdd_context *hdd_ctx,
					      uint8_t *command,
					      uint8_t command_len,
					      struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t numChannels = 0;
	QDF_STATUS status;
	mac_handle_t mac_handle;

	ret = hdd_parse_channellist(value, ChannelList, &numChannels);
	if (ret) {
		hdd_err("Failed to parse channel list information");
		goto exit;
	}
	if (numChannels > WNI_CFG_VALID_CHANNEL_LIST_LEN) {
		hdd_err("number of channels (%d) supported exceeded max (%d)",
			  numChannels,
			  WNI_CFG_VALID_CHANNEL_LIST_LEN);
		ret = -EINVAL;
		goto exit;
	}

	mac_handle = hdd_ctx->mac_handle;
	if (!sme_validate_channel_list(mac_handle, ChannelList, numChannels)) {
		hdd_err("List contains invalid channel(s)");
		ret = -EINVAL;
		goto exit;
	}

	status = sme_set_ese_roam_scan_channel_list(mac_handle,
						    adapter->session_id,
						    ChannelList,
						    numChannels);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Failed to update channel list information");
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_get_tsm_stats(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	char extra[128] = { 0 };
	int len = 0;
	uint8_t tid = 0;
	struct hdd_station_ctx *sta_ctx;
	tAniTrafStrmMetrics tsm_metrics = {0};

	if ((QDF_STA_MODE != adapter->device_mode) &&
	    (QDF_P2P_CLIENT_MODE != adapter->device_mode)) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	/* if not associated, return error */
	if (eConnectionState_Associated != sta_ctx->conn_info.connState) {
		hdd_err("Not associated!");
		ret = -EINVAL;
		goto exit;
	}

	/* Move pointer to ahead of GETTSMSTATS<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &tid);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			  TID_MIN_VALUE,
			  TID_MAX_VALUE);
		ret = -EINVAL;
		goto exit;
	}
	if ((tid < TID_MIN_VALUE) || (tid > TID_MAX_VALUE)) {
		hdd_err("tid value %d is out of range (Min: %d Max: %d)",
			  tid, TID_MIN_VALUE, TID_MAX_VALUE);
		ret = -EINVAL;
		goto exit;
	}
	hdd_debug("Received Command to get tsm stats tid = %d",
		 tid);
	ret = hdd_get_tsm_stats(adapter, tid, &tsm_metrics);
	if (ret) {
		hdd_err("failed to get tsm stats");
		goto exit;
	}
	hdd_debug(
		"UplinkPktQueueDly(%d) UplinkPktQueueDlyHist[0](%d) UplinkPktQueueDlyHist[1](%d) UplinkPktQueueDlyHist[2](%d) UplinkPktQueueDlyHist[3](%d) UplinkPktTxDly(%u) UplinkPktLoss(%d) UplinkPktCount(%d) RoamingCount(%d) RoamingDly(%d)",
		  tsm_metrics.UplinkPktQueueDly,
		  tsm_metrics.UplinkPktQueueDlyHist[0],
		  tsm_metrics.UplinkPktQueueDlyHist[1],
		  tsm_metrics.UplinkPktQueueDlyHist[2],
		  tsm_metrics.UplinkPktQueueDlyHist[3],
		  tsm_metrics.UplinkPktTxDly,
		  tsm_metrics.UplinkPktLoss,
		  tsm_metrics.UplinkPktCount,
		  tsm_metrics.RoamingCount,
		  tsm_metrics.RoamingDly);
	/*
	 * Output TSM stats is of the format
	 * GETTSMSTATS [PktQueueDly]
	 * [PktQueueDlyHist[0]]:[PktQueueDlyHist[1]] ...[RoamingDly]
	 * eg., GETTSMSTATS 10 1:0:0:161 20 1 17 8 39800
	 */
	len = scnprintf(extra,
			sizeof(extra),
			"%s %d %d:%d:%d:%d %u %d %d %d %d",
			command,
			tsm_metrics.UplinkPktQueueDly,
			tsm_metrics.UplinkPktQueueDlyHist[0],
			tsm_metrics.UplinkPktQueueDlyHist[1],
			tsm_metrics.UplinkPktQueueDlyHist[2],
			tsm_metrics.UplinkPktQueueDlyHist[3],
			tsm_metrics.UplinkPktTxDly,
			tsm_metrics.UplinkPktLoss,
			tsm_metrics.UplinkPktCount,
			tsm_metrics.RoamingCount,
			tsm_metrics.RoamingDly);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int drv_cmd_set_cckm_ie(struct hdd_adapter *adapter,
			       struct hdd_context *hdd_ctx,
			       uint8_t *command,
			       uint8_t command_len,
			       struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	uint8_t *cckmIe = NULL;
	uint8_t cckmIeLen = 0;

	ret = hdd_parse_get_cckm_ie(value, &cckmIe, &cckmIeLen);
	if (ret) {
		hdd_err("Failed to parse cckm ie data");
		goto exit;
	}

	if (cckmIeLen > DOT11F_IE_RSN_MAX_LEN) {
		hdd_err("CCKM Ie input length is more than max[%d]",
			  DOT11F_IE_RSN_MAX_LEN);
		if (NULL != cckmIe) {
			qdf_mem_free(cckmIe);
			cckmIe = NULL;
		}
		ret = -EINVAL;
		goto exit;
	}

	sme_set_cckm_ie(hdd_ctx->mac_handle, adapter->session_id,
			cckmIe, cckmIeLen);
	if (NULL != cckmIe) {
		qdf_mem_free(cckmIe);
		cckmIe = NULL;
	}

exit:
	return ret;
}

static int drv_cmd_ccx_beacon_req(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	tCsrEseBeaconReq eseBcnReq = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (QDF_STA_MODE != adapter->device_mode) {
		hdd_warn("Unsupported in mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);
		return -EINVAL;
	}

	ret = hdd_parse_ese_beacon_req(value, &eseBcnReq);
	if (ret) {
		hdd_err("Failed to parse ese beacon req");
		goto exit;
	}

	if (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_debug("Not associated");

		if (!eseBcnReq.numBcnReqIe)
			return -EINVAL;

		hdd_indicate_ese_bcn_report_no_results(adapter,
			eseBcnReq.bcnReq[0].measurementToken,
			0x02, /* BIT(1) set for measurement done */
			0);   /* no BSS */
		goto exit;
	}

	status = sme_set_ese_beacon_request(hdd_ctx->mac_handle,
					    adapter->session_id,
					    &eseBcnReq);

	if (QDF_STATUS_E_RESOURCES == status) {
		hdd_err("sme_set_ese_beacon_request failed (%d), a request already in progress",
			  status);
		ret = -EBUSY;
		goto exit;
	} else if (QDF_STATUS_SUCCESS != status) {
		hdd_err("sme_set_ese_beacon_request failed (%d)",
			status);
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

/**
 * drv_cmd_ccx_plm_req() - Set ESE PLM request
 * @adapter:     Pointer to the HDD adapter
 * @hdd_ctx:     Pointer to the HDD context
 * @command:     Driver command string
 * @command_len: Driver command string length
 * @priv_data:   Private data coming with the driver command. Unused here
 *
 * This function handles driver command that sets the ESE PLM request
 *
 * Return: 0 on success; negative errno otherwise
 */
static int drv_cmd_ccx_plm_req(struct hdd_adapter *adapter,
			       struct hdd_context *hdd_ctx,
			       uint8_t *command,
			       uint8_t command_len,
			       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpSirPlmReq pPlmRequest = NULL;

	pPlmRequest = qdf_mem_malloc(sizeof(tSirPlmReq));
	if (NULL == pPlmRequest) {
		ret = -ENOMEM;
		goto exit;
	}

	status = hdd_parse_plm_cmd(value, pPlmRequest);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_mem_free(pPlmRequest);
		pPlmRequest = NULL;
		ret = -EINVAL;
		goto exit;
	}
	pPlmRequest->sessionId = adapter->session_id;

	status = sme_set_plm_request(hdd_ctx->mac_handle, pPlmRequest);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_mem_free(pPlmRequest);
		pPlmRequest = NULL;
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

/**
 * drv_cmd_set_ccx_mode() - Set ESE mode
 * @adapter:     Pointer to the HDD adapter
 * @hdd_ctx:     Pointer to the HDD context
 * @command:     Driver command string
 * @command_len: Driver command string length
 * @priv_data:   Private data coming with the driver command. Unused here
 *
 * This function handles driver command that sets ESE mode
 *
 * Return: 0 on success; negative errno otherwise
 */
static int drv_cmd_set_ccx_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t eseMode = CFG_ESE_FEATURE_ENABLED_DEFAULT;
	struct pmkid_mode_bits pmkid_modes;
	mac_handle_t mac_handle;

	hdd_get_pmkid_modes(hdd_ctx, &pmkid_modes);
	mac_handle = hdd_ctx->mac_handle;
	/*
	 * Check if the features OKC/ESE/11R are supported simultaneously,
	 * then this operation is not permitted (return FAILURE)
	 */
	if (sme_get_is_ese_feature_enabled(mac_handle) &&
	    pmkid_modes.fw_okc &&
	    sme_get_is_ft_feature_enabled(mac_handle)) {
		hdd_warn("OKC/ESE/11R are supported simultaneously hence this operation is not permitted!");
		ret = -EPERM;
		goto exit;
	}

	/* Move pointer to ahead of SETCCXMODE<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &eseMode);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			  CFG_ESE_FEATURE_ENABLED_MIN,
			  CFG_ESE_FEATURE_ENABLED_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((eseMode < CFG_ESE_FEATURE_ENABLED_MIN) ||
	    (eseMode > CFG_ESE_FEATURE_ENABLED_MAX)) {
		hdd_err("Ese mode value %d is out of range (Min: %d Max: %d)",
			  eseMode,
			  CFG_ESE_FEATURE_ENABLED_MIN,
			  CFG_ESE_FEATURE_ENABLED_MAX);
		ret = -EINVAL;
		goto exit;
	}
	hdd_debug("Received Command to change ese mode = %d", eseMode);

	hdd_ctx->config->isEseIniFeatureEnabled = eseMode;
	sme_update_is_ese_feature_enabled(mac_handle,
					  adapter->session_id,
					  eseMode);

exit:
	return ret;
}
#endif /* FEATURE_WLAN_ESE */

static int drv_cmd_set_mc_rate(struct hdd_adapter *adapter,
			       struct hdd_context *hdd_ctx,
			       uint8_t *command,
			       uint8_t command_len,
			       struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	int target_rate = 0;

	/* input value is in units of hundred kbps */

	/* Move pointer to ahead of SETMCRATE<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer, decimal base */
	ret = kstrtouint(value, 10, &target_rate);

	ret = wlan_hdd_set_mc_rate(adapter, target_rate);
	return ret;
}

static int drv_cmd_max_tx_power(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int ret;
	int tx_power;
	QDF_STATUS status;
	uint8_t *value = command;
	struct qdf_mac_addr bssid = QDF_MAC_ADDR_BCAST_INIT;
	struct qdf_mac_addr selfmac = QDF_MAC_ADDR_BCAST_INIT;

	ret = hdd_parse_setmaxtxpower_command(value, &tx_power);
	if (ret) {
		hdd_err("Invalid MAXTXPOWER command");
		return ret;
	}

	hdd_for_each_adapter(hdd_ctx, adapter) {
		/* Assign correct self MAC address */
		qdf_copy_macaddr(&bssid,
				 &adapter->mac_addr);
		qdf_copy_macaddr(&selfmac,
				 &adapter->mac_addr);

		hdd_debug("Device mode %d max tx power %d selfMac: "
			  MAC_ADDRESS_STR " bssId: " MAC_ADDRESS_STR,
			  adapter->device_mode, tx_power,
			  MAC_ADDR_ARRAY(selfmac.bytes),
			  MAC_ADDR_ARRAY(bssid.bytes));

		status = sme_set_max_tx_power(hdd_ctx->mac_handle,
					      bssid, selfmac, tx_power);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("Set max tx power failed");
			ret = -EINVAL;
			goto exit;
		}
		hdd_debug("Set max tx power success");
	}

exit:
	return ret;
}

static int drv_cmd_set_dfs_scan_mode(struct hdd_adapter *adapter,
				    struct hdd_context *hdd_ctx,
				    uint8_t *command,
				    uint8_t command_len,
				    struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t *value = command;
	uint8_t dfsScanMode = CFG_ROAMING_DFS_CHANNEL_DEFAULT;

	/* Move pointer to ahead of SETDFSSCANMODE<delimiter> */
	value = value + command_len + 1;

	/* Convert the value from ascii to integer */
	ret = kstrtou8(value, 10, &dfsScanMode);
	if (ret < 0) {
		/*
		 * If the input value is greater than max value of datatype,
		 * then also kstrtou8 fails
		 */
		hdd_err("kstrtou8 failed range [%d - %d]",
			  CFG_ROAMING_DFS_CHANNEL_MIN,
			  CFG_ROAMING_DFS_CHANNEL_MAX);
		ret = -EINVAL;
		goto exit;
	}

	if ((dfsScanMode < CFG_ROAMING_DFS_CHANNEL_MIN) ||
	    (dfsScanMode > CFG_ROAMING_DFS_CHANNEL_MAX)) {
		hdd_err("dfsScanMode value %d is out of range (Min: %d Max: %d)",
			  dfsScanMode,
			  CFG_ROAMING_DFS_CHANNEL_MIN,
			  CFG_ROAMING_DFS_CHANNEL_MAX);
		ret = -EINVAL;
		goto exit;
	}

	hdd_debug("Received Command to Set DFS Scan Mode = %d",
		  dfsScanMode);

	/* When DFS scanning is disabled, the DFS channels need to be
	 * removed from the operation of device.
	 */
	ret = wlan_hdd_enable_dfs_chan_scan(hdd_ctx,
			dfsScanMode != CFG_ROAMING_DFS_CHANNEL_DISABLED);
	if (ret < 0) {
		/* Some conditions prevented it from disabling DFS channels */
		hdd_err("disable/enable DFS channel request was denied");
		goto exit;
	}

	hdd_ctx->config->allowDFSChannelRoam = dfsScanMode;
	sme_update_dfs_scan_mode(hdd_ctx->mac_handle, adapter->session_id,
				 dfsScanMode);

exit:
	return ret;
}

static int drv_cmd_get_dfs_scan_mode(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx,
				     uint8_t *command,
				     uint8_t command_len,
				     struct hdd_priv_data *priv_data)
{
	int ret = 0;
	uint8_t dfsScanMode = sme_get_dfs_scan_mode(hdd_ctx->mac_handle);
	char extra[32];
	uint8_t len = 0;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, dfsScanMode);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_get_link_status(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   uint8_t *command,
				   uint8_t command_len,
				   struct hdd_priv_data *priv_data)
{
	int ret = 0;
	int value = wlan_hdd_get_link_status(adapter);
	char extra[32];
	uint8_t len;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, value);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
static int drv_cmd_enable_ext_wow(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx,
				  uint8_t *command,
				  uint8_t command_len,
				  struct hdd_priv_data *priv_data)
{
	uint8_t *value = command;
	int set_value;

	/* Move pointer to ahead of ENABLEEXTWOW */
	value = value + command_len;

	if (!(sscanf(value, "%d", &set_value))) {
		hdd_info("No input identified");
		return -EINVAL;
	}

	return hdd_enable_ext_wow_parser(adapter,
					 adapter->session_id,
					 set_value);
}

static int drv_cmd_set_app1_params(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   uint8_t *command,
				   uint8_t command_len,
				   struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;

	/* Move pointer to ahead of SETAPP1PARAMS */
	value = value + command_len;

	ret = hdd_set_app_type1_parser(adapter,
				       value, strlen(value));
	if (ret >= 0)
		hdd_ctx->is_extwow_app_type1_param_set = true;

	return ret;
}

static int drv_cmd_set_app2_params(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   uint8_t *command,
				   uint8_t command_len,
				   struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;

	/* Move pointer to ahead of SETAPP2PARAMS */
	value = value + command_len;

	ret = hdd_set_app_type2_parser(adapter, value, strlen(value));
	if (ret >= 0)
		hdd_ctx->is_extwow_app_type2_param_set = true;

	return ret;
}
#endif /* WLAN_FEATURE_EXTWOW_SUPPORT */

#ifdef FEATURE_WLAN_TDLS
/**
 * drv_cmd_tdls_secondary_channel_offset() - secondary tdls off channel offset
 * @adapter:     Pointer to the HDD adapter
 * @hdd_ctx:     Pointer to the HDD context
 * @command:     Driver command string
 * @command_len: Driver command string length
 * @priv_data:   Private data coming with the driver command. Unused here
 *
 * This function handles driver command that sets the secondary tdls off channel
 * offset
 *
 * Return: 0 on success; negative errno otherwise
 */
static int drv_cmd_tdls_secondary_channel_offset(struct hdd_adapter *adapter,
						 struct hdd_context *hdd_ctx,
						 uint8_t *command,
						 uint8_t command_len,
						 struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	int set_value;

	/* Move pointer to point the string */
	value += command_len;

	ret = sscanf(value, "%d", &set_value);
	if (ret != 1)
		return -EINVAL;

	hdd_debug("Tdls offchannel offset:%d", set_value);

	ret = hdd_set_tdls_secoffchanneloffset(hdd_ctx, adapter, set_value);

	return ret;
}

/**
 * drv_cmd_tdls_off_channel_mode() - set tdls off channel mode
 * @adapter:     Pointer to the HDD adapter
 * @hdd_ctx:     Pointer to the HDD context
 * @command:     Driver command string
 * @command_len: Driver command string length
 * @priv_data:   Private data coming with the driver command. Unused here
 *
 * This function handles driver command that sets tdls off channel mode
 *
 * Return: 0 on success; negative errno otherwise
 */
static int drv_cmd_tdls_off_channel_mode(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	int set_value;

	/* Move pointer to point the string */
	value += command_len;

	ret = sscanf(value, "%d", &set_value);
	if (ret != 1)
		return -EINVAL;

	hdd_debug("Tdls offchannel mode:%d", set_value);

	ret = hdd_set_tdls_offchannelmode(hdd_ctx, adapter, set_value);

	return ret;
}

/**
 * drv_cmd_tdls_off_channel() - set tdls off channel number
 * @adapter:     Pointer to the HDD adapter
 * @hdd_ctx:     Pointer to the HDD context
 * @command:     Driver command string
 * @command_len: Driver command string length
 * @priv_data:   Private data coming with the driver command. Unused here
 *
 * This function handles driver command that sets tdls off channel number
 *
 * Return: 0 on success; negative errno otherwise
 */
static int drv_cmd_tdls_off_channel(struct hdd_adapter *adapter,
				    struct hdd_context *hdd_ctx,
				    uint8_t *command,
				    uint8_t command_len,
				    struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	int channel;
	enum channel_state reg_state;

	/* Move pointer to point the string */
	value += command_len;

	ret = sscanf(value, "%d", &channel);
	if (ret != 1)
		return -EINVAL;
	reg_state = wlan_reg_get_channel_state(hdd_ctx->pdev, channel);

	if (reg_state == CHANNEL_STATE_DFS ||
		reg_state == CHANNEL_STATE_DISABLE ||
		reg_state == CHANNEL_STATE_INVALID) {
		hdd_err("reg state of the  channel %d is %d and not supported",
			channel, reg_state);
		return -EINVAL;
	}

	hdd_debug("Tdls offchannel num: %d", channel);

	ret = hdd_set_tdls_offchannel(hdd_ctx, adapter, channel);

	return ret;
}

/**
 * drv_cmd_tdls_scan() - set tdls scan type
 * @adapter:     Pointer to the HDD adapter
 * @hdd_ctx:     Pointer to the HDD context
 * @command:     Driver command string
 * @command_len: Driver command string length
 * @priv_data:   Private data coming with the driver command. Unused here
 *
 * This function handles driver command that sets tdls scan type
 *
 * Return: 0 on success; negative errno otherwise
 */
static int drv_cmd_tdls_scan(struct hdd_adapter *adapter,
				    struct hdd_context *hdd_ctx,
				    uint8_t *command,
				    uint8_t command_len,
				    struct hdd_priv_data *priv_data)
{
	int ret;
	uint8_t *value = command;
	int set_value;

	/* Move pointer to point the string */
	value += command_len;

	ret = sscanf(value, "%d", &set_value);
	if (ret != 1)
		return -EINVAL;

	hdd_debug("Tdls scan type val: %d", set_value);

	ret = hdd_set_tdls_scan_type(hdd_ctx, set_value);

	return ret;
}
#endif

static int drv_cmd_get_rssi(struct hdd_adapter *adapter,
			    struct hdd_context *hdd_ctx,
			    uint8_t *command,
			    uint8_t command_len,
			    struct hdd_priv_data *priv_data)
{
	int ret = 0;
	int8_t rssi = 0;
	char extra[32];

	uint8_t len = 0;

	wlan_hdd_get_rssi(adapter, &rssi);

	len = scnprintf(extra, sizeof(extra), "%s %d", command, rssi);
	len = QDF_MIN(priv_data->total_len, len + 1);

	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("Failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

static int drv_cmd_get_linkspeed(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	int ret;
	uint32_t link_speed = 0;
	char extra[32];
	uint8_t len = 0;

	ret = wlan_hdd_get_link_speed(adapter, &link_speed);
	if (0 != ret)
		return ret;

	len = scnprintf(extra, sizeof(extra), "%s %d", command, link_speed);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("Failed to copy data to user buffer");
		ret = -EFAULT;
	}

	return ret;
}

/**
 * hdd_set_rx_filter() - set RX filter
 * @adapter: Pointer to adapter
 * @action: Filter action
 * @pattern: Address pattern
 *
 * Address pattern is most significant byte of address for example
 * 0x01 for IPV4 multicast address
 * 0x33 for IPV6 multicast address
 * 0xFF for broadcast address
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_set_rx_filter(struct hdd_adapter *adapter, bool action,
			uint8_t pattern)
{
	int ret;
	uint8_t i, j;
	tSirRcvFltMcAddrList *filter;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	mac_handle_t mac_handle;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle) {
		hdd_err("MAC Handle is NULL");
		return -EINVAL;
	}

	if (!hdd_ctx->config->fEnableMCAddrList) {
		hdd_warn("mc addr ini is disabled");
		return -EINVAL;
	}

	/*
	 * If action is false it means start dropping packets
	 * Set addr_filter_pattern which will be used when sending
	 * MC/BC address list to target
	 */
	if (!action)
		adapter->addr_filter_pattern = pattern;
	else
		adapter->addr_filter_pattern = 0;

	if (((adapter->device_mode == QDF_STA_MODE) ||
		(adapter->device_mode == QDF_P2P_CLIENT_MODE)) &&
		adapter->mc_addr_list.mc_cnt &&
		hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {


		filter = qdf_mem_malloc(sizeof(*filter));
		if (NULL == filter) {
			hdd_err("Could not allocate Memory");
			return -ENOMEM;
		}
		filter->action = action;
		for (i = 0, j = 0; i < adapter->mc_addr_list.mc_cnt; i++) {
			if (!memcmp(adapter->mc_addr_list.addr[i],
				&pattern, 1)) {
				memcpy(filter->multicastAddr[j].bytes,
					adapter->mc_addr_list.addr[i],
					sizeof(adapter->mc_addr_list.addr[i]));

				hdd_debug("%s RX filter : addr ="
				    MAC_ADDRESS_STR,
				    action ? "setting" : "clearing",
				    MAC_ADDR_ARRAY(filter->multicastAddr[j].bytes));
				j++;
			}
			if (j == SIR_MAX_NUM_MULTICAST_ADDRESS)
				break;
		}
		filter->ulMulticastAddrCnt = j;
		/* Set rx filter */
		sme_8023_multicast_list(mac_handle, adapter->session_id,
					filter);
		qdf_mem_free(filter);
	} else {
		hdd_debug("mode %d mc_cnt %d",
			  adapter->device_mode, adapter->mc_addr_list.mc_cnt);
	}

	return 0;
}

/**
 * hdd_driver_rxfilter_command_handler() - RXFILTER driver command handler
 * @command: Pointer to input string driver command
 * @adapter: Pointer to adapter
 * @action: Action to enable/disable filtering
 *
 * If action == false
 * Start filtering out data packets based on type
 * RXFILTER-REMOVE 0 -> Start filtering out unicast data packets
 * RXFILTER-REMOVE 1 -> Start filtering out broadcast data packets
 * RXFILTER-REMOVE 2 -> Start filtering out IPV4 mcast data packets
 * RXFILTER-REMOVE 3 -> Start filtering out IPV6 mcast data packets
 *
 * if action == true
 * Stop filtering data packets based on type
 * RXFILTER-ADD 0 -> Stop filtering unicast data packets
 * RXFILTER-ADD 1 -> Stop filtering broadcast data packets
 * RXFILTER-ADD 2 -> Stop filtering IPV4 mcast data packets
 * RXFILTER-ADD 3 -> Stop filtering IPV6 mcast data packets
 *
 * Current implementation only supports IPV4 address filtering by
 * selectively allowing IPV4 multicast data packest based on
 * address list received in .ndo_set_rx_mode
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_driver_rxfilter_command_handler(uint8_t *command,
						struct hdd_adapter *adapter,
						bool action)
{
	int ret = 0;
	uint8_t *value;
	uint8_t type;

	value = command;
	/* Skip space after RXFILTER-REMOVE OR RXFILTER-ADD based on action */
	if (!action)
		value = command + 16;
	else
		value = command + 13;
	ret = kstrtou8(value, 10, &type);
	if (ret < 0) {
		hdd_err("kstrtou8 failed invalid input value");
		return -EINVAL;
	}

	switch (type) {
	case 2:
		/* Set rx filter for IPV4 multicast data packets */
		ret = hdd_set_rx_filter(adapter, action, 0x01);
		break;
	default:
		hdd_debug("Unsupported RXFILTER type %d", type);
		break;
	}

	return ret;
}

/**
 * drv_cmd_rx_filter_remove() - RXFILTER REMOVE driver command handler
 * @adapter: Pointer to network adapter
 * @hdd_ctx: Pointer to hdd context
 * @command: Pointer to input command
 * @command_len: Command length
 * @priv_data: Pointer to private data in command
 */
static int drv_cmd_rx_filter_remove(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	return hdd_driver_rxfilter_command_handler(command, adapter, false);
}

/**
 * drv_cmd_rx_filter_add() - RXFILTER ADD driver command handler
 * @adapter: Pointer to network adapter
 * @hdd_ctx: Pointer to hdd context
 * @command: Pointer to input command
 * @command_len: Command length
 * @priv_data: Pointer to private data in command
 */
static int drv_cmd_rx_filter_add(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	return hdd_driver_rxfilter_command_handler(command, adapter, true);
}

/**
 * hdd_parse_setantennamode_command() - HDD Parse SETANTENNAMODE
 * command
 * @value: Pointer to SETANTENNAMODE command
 * @mode: Pointer to antenna mode
 * @reason: Pointer to reason for set antenna mode
 *
 * This function parses the SETANTENNAMODE command passed in the format
 * SETANTENNAMODE<space>mode
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_parse_setantennamode_command(const uint8_t *value)
{
	const uint8_t *in_ptr = value;
	int tmp, v;
	char arg1[32];

	in_ptr = strnchr(value, strlen(value), SPACE_ASCII_VALUE);

	/* no argument after the command */
	if (NULL == in_ptr) {
		hdd_err("No argument after the command");
		return -EINVAL;
	}

	/* no space after the command */
	if (SPACE_ASCII_VALUE != *in_ptr) {
		hdd_err("No space after the command");
		return -EINVAL;
	}

	/* remove empty spaces */
	while ((SPACE_ASCII_VALUE == *in_ptr) && ('\0' != *in_ptr))
		in_ptr++;

	/* no argument followed by spaces */
	if ('\0' == *in_ptr) {
		hdd_err("No argument followed by spaces");
		return -EINVAL;
	}

	/* get the argument i.e. antenna mode */
	v = sscanf(in_ptr, "%31s ", arg1);
	if (1 != v) {
		hdd_err("argument retrieval from cmd string failed");
		return -EINVAL;
	}

	v = kstrtos32(arg1, 10, &tmp);
	if (v < 0) {
		hdd_err("argument string to int conversion failed");
		return -EINVAL;
	}

	return tmp;
}

/**
 * hdd_is_supported_chain_mask_2x2() - Verify if supported chain
 * mask is 2x2 mode
 * @hdd_ctx: Pointer to hdd contex
 *
 * Return: true if supported chain mask 2x2 else false
 */
static bool hdd_is_supported_chain_mask_2x2(struct hdd_context *hdd_ctx)
{
	/*
	 * Revisit and the update logic to determine the number
	 * of TX/RX chains supported in the system when
	 * antenna sharing per band chain mask support is
	 * brought in
	 */
	return (hdd_ctx->config->enable2x2 == 0x01) ? true : false;
}

/**
 * hdd_is_supported_chain_mask_1x1() - Verify if the supported
 * chain mask is 1x1
 * @hdd_ctx: Pointer to hdd contex
 *
 * Return: true if supported chain mask 1x1 else false
 */
static bool hdd_is_supported_chain_mask_1x1(struct hdd_context *hdd_ctx)
{
	/*
	 * Revisit and update the logic to determine the number
	 * of TX/RX chains supported in the system when
	 * antenna sharing per band chain mask support is
	 * brought in
	 */
	return (!hdd_ctx->config->enable2x2) ? true : false;
}

QDF_STATUS hdd_update_smps_antenna_mode(struct hdd_context *hdd_ctx, int mode)
{
	QDF_STATUS status;
	uint8_t smps_mode;
	uint8_t smps_enable;
	mac_handle_t mac_handle;

	/* Update SME SMPS config */
	if (HDD_ANTENNA_MODE_1X1 == mode) {
		smps_enable = true;
		smps_mode = HDD_SMPS_MODE_STATIC;
	} else {
		smps_enable = false;
		smps_mode = HDD_SMPS_MODE_DISABLED;
	}

	hdd_debug("Update SME SMPS enable: %d mode: %d",
		 smps_enable, smps_mode);
	mac_handle = hdd_ctx->mac_handle;
	status = sme_update_mimo_power_save(mac_handle, smps_enable,
					    smps_mode, false);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Update SMPS config failed enable: %d mode: %d status: %d",
			smps_enable, smps_mode, status);
		return QDF_STATUS_E_FAILURE;
	}

	hdd_ctx->current_antenna_mode = mode;
	/*
	 * Update the user requested nss in the mac context.
	 * This will be used in tdls protocol engine to form tdls
	 * Management frames.
	 */
	sme_update_user_configured_nss(mac_handle,
				       hdd_ctx->current_antenna_mode);

	hdd_debug("Successfully switched to mode: %d x %d",
		 hdd_ctx->current_antenna_mode,
		 hdd_ctx->current_antenna_mode);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_soc_set_antenna_mode_cb() - Callback for set antenna mode
 * @status: Status of set antenna mode
 * @context: callback context
 *
 * Callback on setting antenna mode
 *
 * Return: None
 */
static void
wlan_hdd_soc_set_antenna_mode_cb(enum set_antenna_mode_status status,
				 void *context)
{
	struct osif_request *request = NULL;

	hdd_debug("Status: %d", status);

	request = osif_request_get(context);
	if (!request) {
		hdd_err("obselete request");
		return;
	}

	/* Signal the completion of set dual mac config */
	osif_request_complete(request);
	osif_request_put(request);
}

static QDF_STATUS
hdd_populate_vdev_chains(struct mlme_nss_chains *nss_chains_cfg,
			 uint8_t tx_chains,
			 uint8_t rx_chains,
			 enum nss_chains_band_info band,
			 struct wlan_objmgr_vdev *vdev)
{
	struct mlme_nss_chains *dynamic_cfg;

	nss_chains_cfg->num_rx_chains[band] = rx_chains;
	nss_chains_cfg->num_tx_chains[band] = tx_chains;

	dynamic_cfg = ucfg_mlme_get_dynamic_vdev_config(vdev);

	if (!dynamic_cfg) {
		hdd_err("nss chain dynamic config NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * If user gives any nss value, then chains will be adjusted based on
	 * nss (in SME func sme_validate_user_nss_chain_params).
	 * If Chains are not suitable as per current NSS then, we need to
	 * return, and the below logic is added for the same.
	 */

	if ((dynamic_cfg->rx_nss[band] > rx_chains) ||
	    (dynamic_cfg->tx_nss[band] > tx_chains)) {
		hdd_err("Chains less than nss, configure correct nss first.");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static int
hdd_set_dynamic_antenna_mode(struct hdd_adapter *adapter,
			     uint8_t num_rx_chains,
			     uint8_t num_tx_chains)
{
	enum nss_chains_band_info band;
	struct mlme_nss_chains user_cfg;
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle) {
		hdd_err("NULL MAC handle");
		return -EINVAL;
	}

	if (!hdd_is_vdev_in_conn_state(adapter)) {
		hdd_debug("Vdev (id %d) not in connected/started state, cannot accept command",
			  adapter->session_id);
		return -EINVAL;
	}

	qdf_mem_zero(&user_cfg, sizeof(user_cfg));
	for (band = NSS_CHAINS_BAND_2GHZ; band < NSS_CHAINS_BAND_MAX; band++) {
		status = hdd_populate_vdev_chains(&user_cfg,
						  num_rx_chains,
						  num_tx_chains, band,
						  adapter->vdev);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;
	}
	status = sme_nss_chains_update(mac_handle,
				       &user_cfg,
				       adapter->session_id);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	return 0;
}

int hdd_set_antenna_mode(struct hdd_adapter *adapter,
				  struct hdd_context *hdd_ctx, int mode)
{

	struct sir_antenna_mode_param params;
	QDF_STATUS status;
	int ret = 0;
	struct osif_request *request = NULL;
	static const struct osif_request_params request_params = {
		.priv_size = 0,
		.timeout_ms = WLAN_WAIT_TIME_ANTENNA_MODE_REQ,
	};

	switch (mode) {
	case HDD_ANTENNA_MODE_1X1:
		params.num_rx_chains = 1;
		params.num_tx_chains = 1;
		break;
	case HDD_ANTENNA_MODE_2X2:
		params.num_rx_chains = 2;
		params.num_tx_chains = 2;
		break;
	default:
		hdd_err("unsupported antenna mode");
		ret = -EINVAL;
		goto exit;
	}

	if (hdd_ctx->dynamic_nss_chains_support)
		return hdd_set_dynamic_antenna_mode(adapter,
						    params.num_rx_chains,
						    params.num_tx_chains);

	if (hdd_ctx->current_antenna_mode == mode) {
		hdd_err("System already in the requested mode");
		return 0;
	}

	if ((HDD_ANTENNA_MODE_2X2 == mode) &&
	    (!hdd_is_supported_chain_mask_2x2(hdd_ctx))) {
		hdd_err("System does not support 2x2 mode");
		ret = -EPERM;
		goto exit;
	}

	if ((HDD_ANTENNA_MODE_1X1 == mode) &&
	    hdd_is_supported_chain_mask_1x1(hdd_ctx)) {
		hdd_err("System only supports 1x1 mode");
		goto exit;
	}

	/* Check TDLS status and update antenna mode */
	if ((QDF_STA_MODE == adapter->device_mode) &&
	    policy_mgr_is_sta_active_connection_exists(hdd_ctx->psoc)) {
		ret = wlan_hdd_tdls_antenna_switch(hdd_ctx, adapter, mode);
		if (0 != ret)
			goto exit;
	}

	request = osif_request_alloc(&request_params);
	if (!request) {
		hdd_err("Request Allocation Failure");
		ret = -ENOMEM;
		goto exit;
	}

	params.set_antenna_mode_ctx = osif_request_cookie(request);
	params.set_antenna_mode_resp = (void *)wlan_hdd_soc_set_antenna_mode_cb;
	hdd_debug("Set antenna mode rx chains: %d tx chains: %d",
		 params.num_rx_chains,
		 params.num_tx_chains);

	status = sme_soc_set_antenna_mode(hdd_ctx->mac_handle, &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("set antenna mode failed status : %d", status);
		ret = -EFAULT;
		goto request_put;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("send set antenna mode timed out");
		goto request_put;
	}

	status = hdd_update_smps_antenna_mode(hdd_ctx, mode);
	if (QDF_STATUS_SUCCESS != status) {
		ret = -EFAULT;
		goto request_put;
	}
	ret = 0;
request_put:
	osif_request_put(request);
exit:
	hdd_debug("Set antenna status: %d current mode: %d",
		 ret, hdd_ctx->current_antenna_mode);

	return ret;
}
/**
 * drv_cmd_set_antenna_mode() - SET ANTENNA MODE driver command
 * handler
 * @adapter: Pointer to network adapter
 * @hdd_ctx: Pointer to hdd context
 * @command: Pointer to input command
 * @command_len: Command length
 * @priv_data: Pointer to private data in command
 */
static int drv_cmd_set_antenna_mode(struct hdd_adapter *adapter,
				struct hdd_context *hdd_ctx,
				uint8_t *command,
				uint8_t command_len,
				struct hdd_priv_data *priv_data)
{
	int mode;
	uint8_t *value = command;

	mode = hdd_parse_setantennamode_command(value);
	if (mode < 0) {
		hdd_err("Invalid SETANTENNA command");
		return mode;
	}

	hdd_debug("Processing antenna mode switch to: %d", mode);

	return hdd_set_antenna_mode(adapter, hdd_ctx, mode);
}

/**
 * hdd_get_dynamic_antenna_mode() -get the dynamic antenna mode for vdev
 * @antenna_mode: pointer to antenna mode of vdev
 * @dynamic_nss_chains_support: feature support for dynamic nss chains update
 * @vdev: Pointer to vdev
 *
 * This API will check for the num of chains configured for the vdev, and fill
 * that info in the antenna mode if the dynamic chains per vdev are supported.
 *
 * Return: None
 */
static void
hdd_get_dynamic_antenna_mode(uint32_t *antenna_mode,
			     bool dynamic_nss_chains_support,
			     struct wlan_objmgr_vdev *vdev)
{
	struct mlme_nss_chains *nss_chains_dynamic_cfg;

	if (!dynamic_nss_chains_support)
		return;

	nss_chains_dynamic_cfg = ucfg_mlme_get_dynamic_vdev_config(vdev);
	if (!nss_chains_dynamic_cfg) {
		hdd_err("nss chain dynamic config NULL");
		return;
	}
	/*
	 * At present, this command doesn't include band, so by default
	 * it will return the band 2ghz present rf chains.
	 */
	*antenna_mode =
		nss_chains_dynamic_cfg->num_rx_chains[NSS_CHAINS_BAND_2GHZ];
}

/**
 * drv_cmd_get_antenna_mode() - GET ANTENNA MODE driver command
 * handler
 * @adapter: Pointer to hdd adapter
 * @hdd_ctx: Pointer to hdd context
 * @command: Pointer to input command
 * @command_len: length of the command
 * @priv_data: private data coming with the driver command
 *
 * Return: 0 for success non-zero for failure
 */
static inline int drv_cmd_get_antenna_mode(struct hdd_adapter *adapter,
					   struct hdd_context *hdd_ctx,
					   uint8_t *command,
					   uint8_t command_len,
					   struct hdd_priv_data *priv_data)
{
	uint32_t antenna_mode = 0;
	char extra[32];
	uint8_t len = 0;

	antenna_mode = hdd_ctx->current_antenna_mode;
	/* Overwrite this antenna mode if dynamic vdev chains are supported */
	hdd_get_dynamic_antenna_mode(&antenna_mode,
				     hdd_ctx->dynamic_nss_chains_support,
				     adapter->vdev);
	len = scnprintf(extra, sizeof(extra), "%s %d", command,
			antenna_mode);
	len = QDF_MIN(priv_data->total_len, len + 1);
	if (copy_to_user(priv_data->buf, &extra, len)) {
		hdd_err("Failed to copy data to user buffer");
		return -EFAULT;
	}

	hdd_debug("Get antenna mode: %d", antenna_mode);

	return 0;
}

/*
 * dummy (no-op) hdd driver command handler
 */
static int drv_cmd_dummy(struct hdd_adapter *adapter,
			 struct hdd_context *hdd_ctx,
			 uint8_t *command,
			 uint8_t command_len,
			 struct hdd_priv_data *priv_data)
{
	hdd_debug("%s: Ignoring driver command \"%s\"",
		 adapter->dev->name, command);
	return 0;
}

/*
 * handler for any unsupported wlan hdd driver command
 */
static int drv_cmd_invalid(struct hdd_adapter *adapter,
			   struct hdd_context *hdd_ctx,
			   uint8_t *command,
			   uint8_t command_len,
			   struct hdd_priv_data *priv_data)
{
	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_UNSUPPORTED_IOCTL,
		   adapter->session_id, 0);

	hdd_warn("%s: Unsupported driver command \"%s\"",
		 adapter->dev->name, command);

	return -ENOTSUPP;
}

/**
 * drv_cmd_set_fcc_channel() - Handle fcc constraint request
 * @adapter: HDD adapter
 * @hdd_ctx: HDD context
 * @command: command ptr, SET_FCC_CHANNEL 0/-1 is the command
 * @command_len: command len
 * @priv_data: private data
 *
 * Return: status
 */
static int drv_cmd_set_fcc_channel(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   uint8_t *command,
				   uint8_t command_len,
				   struct hdd_priv_data *priv_data)
{
	QDF_STATUS status;
	int8_t input_value;
	bool fcc_constraint;
	int err;

	/*
	 * This command would be called by user-space when it detects WLAN
	 * ON after airplane mode is set. When APM is set, WLAN turns off.
	 * But it can be turned back on. Otherwise; when APM is turned back
	 * off, WLAN would turn back on. So at that point the command is
	 * expected to come down. 0 means reduce power as per fcc constraint
	 * and -1 means remove constraint.
	 */

	err = kstrtos8(command + command_len + 1, 10, &input_value);
	if (err) {
		hdd_err("error %d parsing userspace fcc parameter", err);
		return err;
	}

	fcc_constraint = input_value ? false : true;
	hdd_debug("input_value = %d && fcc_constraint = %u",
		  input_value, fcc_constraint);

	status = ucfg_reg_set_fcc_constraint(hdd_ctx->pdev, fcc_constraint);

	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to %s tx power for channels 12/13",
			fcc_constraint ? "restore" : "reduce");

	return qdf_status_to_os_return(status);
}

/**
 * hdd_parse_set_channel_switch_command() - Parse and validate CHANNEL_SWITCH
 * command
 * @value: Pointer to the command
 * @chan_number: Pointer to the channel number
 * @chan_bw: Pointer to the channel bandwidth
 *
 * Parses and provides the channel number and channel width from the input
 * command which is expected to be of the format: CHANNEL_SWITCH <CH> <BW>
 * <CH> is channel number to move (where 1 = channel 1, 149 = channel 149, ...)
 * <BW> is bandwidth to move (where 20 = BW 20, 40 = BW 40, 80 = BW 80)
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_parse_set_channel_switch_command(uint8_t *value,
					 uint32_t *chan_number,
					 uint32_t *chan_bw)
{
	const uint8_t *in_ptr = value;
	int ret;

	in_ptr = strnchr(value, strlen(value), SPACE_ASCII_VALUE);

	/* no argument after the command */
	if (NULL == in_ptr) {
		hdd_err("No argument after the command");
		return -EINVAL;
	}

	/* no space after the command */
	if (SPACE_ASCII_VALUE != *in_ptr) {
		hdd_err("No space after the command ");
		return -EINVAL;
	}

	/* remove empty spaces and move the next argument */
	while ((SPACE_ASCII_VALUE == *in_ptr) && ('\0' != *in_ptr))
		in_ptr++;

	/* no argument followed by spaces */
	if ('\0' == *in_ptr) {
		hdd_err("No argument followed by spaces");
		return -EINVAL;
	}

	/* get the two arguments: channel number and bandwidth */
	ret = sscanf(in_ptr, "%u %u", chan_number, chan_bw);
	if (ret != 2) {
		hdd_err("Arguments retrieval from cmd string failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * drv_cmd_set_channel_switch() - Switch SAP/P2P-GO operating channel
 * @adapter: HDD adapter
 * @hdd_ctx: HDD context
 * @command: Pointer to the input command CHANNEL_SWITCH
 * @command_len: Command len
 * @priv_data: Private data
 *
 * Handles private IOCTL CHANNEL_SWITCH command to switch the operating channel
 * of SAP/P2P-GO
 *
 * Return: 0 for success, non-zero for failure
 */
static int drv_cmd_set_channel_switch(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   uint8_t *command,
				   uint8_t command_len,
				   struct hdd_priv_data *priv_data)
{
	struct net_device *dev = adapter->dev;
	int status;
	uint32_t chan_number = 0, chan_bw = 0;
	uint8_t *value = command;
	enum phy_ch_width width;

	if ((adapter->device_mode != QDF_P2P_GO_MODE) &&
		(adapter->device_mode != QDF_SAP_MODE)) {
		hdd_err("IOCTL CHANNEL_SWITCH not supported for mode %d",
			adapter->device_mode);
		return -EINVAL;
	}

	status = hdd_parse_set_channel_switch_command(value,
							&chan_number, &chan_bw);
	if (status) {
		hdd_err("Invalid CHANNEL_SWITCH command");
		return status;
	}

	if ((chan_bw != 20) && (chan_bw != 40) && (chan_bw != 80)) {
		hdd_err("BW %d is not allowed for CHANNEL_SWITCH", chan_bw);
		return -EINVAL;
	}

	if (chan_bw == 80)
		width = CH_WIDTH_80MHZ;
	else if (chan_bw == 40)
		width = CH_WIDTH_40MHZ;
	else
		width = CH_WIDTH_20MHZ;

	hdd_debug("CH:%d BW:%d", chan_number, chan_bw);

	wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, adapter->session_id,
				    CSA_REASON_USER_INITIATED);
	status = hdd_softap_set_channel_change(dev, chan_number, width, true);
	if (status) {
		hdd_err("Set channel change fail");
		return status;
	}

	return 0;
}

#ifdef DISABLE_CHANNEL_LIST
void wlan_hdd_free_cache_channels(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	if (!hdd_ctx->original_channels)
		return;

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);
	hdd_ctx->original_channels->num_channels = 0;
	if (hdd_ctx->original_channels->channel_info) {
		qdf_mem_free(hdd_ctx->original_channels->channel_info);
		hdd_ctx->original_channels->channel_info = NULL;
	}
	qdf_mem_free(hdd_ctx->original_channels);
	hdd_ctx->original_channels = NULL;
	qdf_mutex_release(&hdd_ctx->cache_channel_lock);

	hdd_exit();
}

/**
 * hdd_alloc_chan_cache() - Allocate the memory to cache the channel
 * info for the channels received in command SET_DISABLE_CHANNEL_LIST
 * @hdd_ctx: Pointer to HDD context
 * @num_chan: Number of channels for which memory needs to
 * be allocated
 *
 * Return: 0 on success and error code on failure
 */
static int hdd_alloc_chan_cache(struct hdd_context *hdd_ctx, int num_chan)
{
	hdd_ctx->original_channels =
			qdf_mem_malloc(sizeof(struct hdd_cache_channels));
	if (!hdd_ctx->original_channels) {
		hdd_err("QDF_MALLOC_ERR");
		return -ENOMEM;
	}
	hdd_ctx->original_channels->num_channels = num_chan;
	hdd_ctx->original_channels->channel_info =
					qdf_mem_malloc(num_chan *
					sizeof(struct hdd_cache_channel_info));
	if (!hdd_ctx->original_channels->channel_info) {
		hdd_err("QDF_MALLOC_ERR");
		hdd_ctx->original_channels->num_channels = 0;
		qdf_mem_free(hdd_ctx->original_channels);
		hdd_ctx->original_channels = NULL;
		return -ENOMEM;
	}
	return 0;
}

/**
 * hdd_parse_disable_chan_cmd() - Parse the channel list received
 * in command.
 * @adapter: pointer to hdd adapter
 * @ptr: Pointer to the command string
 *
 * This function parses the channel list received in the command.
 * command should be a string having format
 * SET_DISABLE_CHANNEL_LIST <num of channels>
 * <channels separated by spaces>.
 * If the command comes multiple times than this function will compare
 * the channels received in the command with the channles cached in the
 * first command, if the channel list matches with the cached channles,
 * it returns success otherwise returns failure.
 *
 * Return: 0 on success, Error code on failure
 */

static int hdd_parse_disable_chan_cmd(struct hdd_adapter *adapter, uint8_t *ptr)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	uint8_t *param;
	int j, i, temp_int, ret = 0, num_channels;
	uint32_t parsed_channels[MAX_CHANNEL];
	bool is_command_repeated = false;

	if (NULL == hdd_ctx) {
		hdd_err("HDD Context is NULL");
		return -EINVAL;
	}

	param = strnchr(ptr, strlen(ptr), ' ');
	/*no argument after the command*/
	if (NULL == param)
		return -EINVAL;

	/*no space after the command*/
	else if (SPACE_ASCII_VALUE != *param)
		return -EINVAL;

	param++;

	/*removing empty spaces*/
	while ((SPACE_ASCII_VALUE  == *param) && ('\0' !=  *param))
		param++;

	/*no argument followed by spaces*/
	if ('\0' == *param)
		return -EINVAL;

	/*getting the first argument ie the number of channels*/
	if (sscanf(param, "%d ", &temp_int) != 1) {
		hdd_err("Cannot get number of channels from input");
		return -EINVAL;
	}

	if (temp_int < 0 || temp_int > MAX_CHANNEL) {
		hdd_err("Invalid Number of channel received");
		return -EINVAL;
	}

	hdd_debug("Number of channel to disable are: %d", temp_int);

	if (!temp_int) {
		/*
		 * Restore and Free the cache channels when the command is
		 * received with num channels as 0
		 */
		wlan_hdd_restore_channels(hdd_ctx, false);
		return 0;
	}

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);

	if (!hdd_ctx->original_channels) {
		if (hdd_alloc_chan_cache(hdd_ctx, temp_int)) {
			ret = -ENOMEM;
			goto mem_alloc_failed;
		}
	} else if (hdd_ctx->original_channels->num_channels != temp_int) {
		hdd_err("Invalid Number of channels");
		ret = -EINVAL;
		is_command_repeated = true;
		goto parse_failed;
	} else {
		is_command_repeated = true;
	}
	num_channels = temp_int;
	for (j = 0; j < num_channels; j++) {
		/*
		 * param pointing to the beginning of first space
		 * after number of channels
		 */
		param = strpbrk(param, " ");
		/*no channel list after the number of channels argument*/
		if (NULL == param) {
			hdd_err("Invalid No of channel provided in the list");
			ret = -EINVAL;
			goto parse_failed;
		}

		param++;

		/*removing empty space*/
		while ((SPACE_ASCII_VALUE == *param) && ('\0' != *param))
			param++;

		if ('\0' == *param) {
			hdd_err("No channel is provided in the list");
			ret = -EINVAL;
			goto parse_failed;
		}

		if (sscanf(param, "%d ", &temp_int) != 1) {
			hdd_err("Cannot read channel number");
			ret = -EINVAL;
			goto parse_failed;
		}

		if (!IS_CHANNEL_VALID(temp_int)) {
			hdd_err("Invalid channel number received");
			ret = -EINVAL;
			goto parse_failed;
		}

		hdd_debug("channel[%d] = %d", j, temp_int);
		parsed_channels[j] = temp_int;
	}

	/*extra arguments check*/
	param = strpbrk(param, " ");
	if (NULL != param) {
		while ((SPACE_ASCII_VALUE == *param) && ('\0' != *param))
			param++;

		if ('\0' !=  *param) {
			hdd_err("Invalid argument received");
			ret = -EINVAL;
			goto parse_failed;
		}
	}

	/*
	 * If command is received first time, cache the channels to
	 * be disabled else compare the channels received in the
	 * command with the cached channels, if channel list matches
	 * return success otherewise return failure.
	 */
	if (!is_command_repeated) {
		for (j = 0; j < num_channels; j++)
			hdd_ctx->original_channels->
					channel_info[j].channel_num =
							parsed_channels[j];

		/* Cache the channel list in regulatory also */
		ucfg_reg_cache_channel_state(hdd_ctx->pdev, parsed_channels,
					     num_channels);
	} else {
		for (i = 0; i < num_channels; i++) {
			for (j = 0; j < num_channels; j++)
				if (hdd_ctx->original_channels->
					channel_info[i].channel_num ==
							parsed_channels[j])
					break;
			if (j == num_channels) {
				ret = -EINVAL;
				goto parse_failed;
			}
		}
		ret = 0;
	}
mem_alloc_failed:

	qdf_mutex_release(&hdd_ctx->cache_channel_lock);
	/* Disable the channels received in command SET_DISABLE_CHANNEL_LIST */
	if (!is_command_repeated && hdd_ctx->original_channels) {
		wlan_hdd_disable_channels(hdd_ctx);
		hdd_check_and_disconnect_sta_on_invalid_channel(hdd_ctx,
			eSIR_MAC_OPER_CHANNEL_USER_DISABLED);
	}

	hdd_exit();

	return ret;

parse_failed:
	if (!is_command_repeated)
		wlan_hdd_free_cache_channels(hdd_ctx);

	qdf_mutex_release(&hdd_ctx->cache_channel_lock);
	hdd_exit();

	return ret;
}

static int drv_cmd_set_disable_chan_list(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	return hdd_parse_disable_chan_cmd(adapter, command);
}

/**
 * hdd_get_disable_ch_list() - get disable channel list
 * @hdd_ctx: hdd context
 * @buf: buffer to hold disable channel list
 * @buf_len: buffer length
 *
 * Return: length of data copied to buf
 */
static int hdd_get_disable_ch_list(struct hdd_context *hdd_ctx, uint8_t *buf,
				   uint32_t buf_len)
{
	struct hdd_cache_channel_info *ch_list;
	unsigned char i, num_ch;
	int len = 0;

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);
	if (hdd_ctx->original_channels &&
	    hdd_ctx->original_channels->num_channels &&
	    hdd_ctx->original_channels->channel_info) {
		num_ch = hdd_ctx->original_channels->num_channels;

		len = scnprintf(buf, buf_len, "%s %hhu",
				"GET_DISABLE_CHANNEL_LIST", num_ch);
		ch_list = hdd_ctx->original_channels->channel_info;
		for (i = 0; (i < num_ch) && (len < buf_len - 1); i++) {
			len += scnprintf(buf + len, buf_len - len,
					 " %d", ch_list[i].channel_num);
		}
	}
	qdf_mutex_release(&hdd_ctx->cache_channel_lock);

	return len;
}

static int drv_cmd_get_disable_chan_list(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	char extra[512] = {0};
	int max_len, copied_length;

	hdd_debug("Received Command to get disable Channels list");

	max_len = QDF_MIN(priv_data->total_len, sizeof(extra));
	copied_length = hdd_get_disable_ch_list(hdd_ctx, extra, max_len);
	if (copied_length == 0) {
		hdd_err("disable channel list is not yet programmed");
		return -EINVAL;
	}

	if (copy_to_user(priv_data->buf, &extra, copied_length + 1)) {
		hdd_err("failed to copy data to user buffer");
		return -EFAULT;
	}

	hdd_debug("data:%s", extra);
	return 0;
}
#else

static int drv_cmd_set_disable_chan_list(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	return 0;
}

void wlan_hdd_free_cache_channels(struct hdd_context *hdd_ctx)
{
}

static int drv_cmd_get_disable_chan_list(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	return 0;
}
#endif

#ifdef FEATURE_ANI_LEVEL_REQUEST
static int drv_cmd_get_ani_level(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	char *extra;
	int copied_length = 0, j, temp_int, ret = 0;
	uint8_t *param, num_freqs, num_recv_channels;
	uint32_t parsed_freqs[MAX_NUM_FREQS_FOR_ANI_LEVEL];
	struct wmi_host_ani_level_event ani[MAX_NUM_FREQS_FOR_ANI_LEVEL];
	size_t user_size = priv_data->total_len;

	hdd_debug("Received Command to get ANI level");

	param = strnchr(command, strlen(command), ' ');

	/* No argument after the command */
	if (!param)
		return -EINVAL;

	/* No space after the command */
	else if (SPACE_ASCII_VALUE != *param)
		return -EINVAL;

	param++;

	/* Removing empty spaces*/
	while ((SPACE_ASCII_VALUE  == *param) && ('\0' !=  *param))
		param++;

	/*no argument followed by spaces */
	if ('\0' == *param)
		return -EINVAL;

	/* Getting the first argument ie the number of channels */
	if (sscanf(param, "%d ", &temp_int) != 1) {
		hdd_err("Cannot get number of freq from input");
		return -EINVAL;
	}

	if (temp_int < 0 || temp_int > MAX_NUM_FREQS_FOR_ANI_LEVEL) {
		hdd_err("Invalid Number of channel received");
		return -EINVAL;
	}

	hdd_debug("Number of freq to fetch ANI level are: %d", temp_int);

	if (!temp_int)
		return 0;

	num_freqs = temp_int;

	for (j = 0; j < num_freqs; j++) {
		/*
		 * Param pointing to the beginning of first space
		 * after number of channels.
		 */
		param = strpbrk(param, " ");
		/*no channel list after the number of channels argument*/
		if (!param) {
			hdd_err("Invalid No of freq provided in the list");
			ret = -EINVAL;
			goto parse_failed;
		}

		param++;

		/* Removing empty space */
		while ((SPACE_ASCII_VALUE == *param) && ('\0' != *param))
			param++;

		if ('\0' == *param) {
			hdd_err("No freq is provided in the list");
			ret = -EINVAL;
			goto parse_failed;
		}

		if (sscanf(param, "%d ", &temp_int) != 1) {
			hdd_err("Cannot read freq number");
			ret = -EINVAL;
			goto parse_failed;
		}

		hdd_debug("channel_freq[%d] = %d", j, temp_int);
		parsed_freqs[j] = temp_int;
	}

	/* Extra arguments check */
	param = strpbrk(param, " ");
	if (param) {
		while ((SPACE_ASCII_VALUE == *param) && ('\0' != *param))
			param++;

		if ('\0' !=  *param) {
			hdd_err("Invalid argument received");
			ret = -EINVAL;
			goto parse_failed;
		}
	}

	qdf_mem_zero(ani, sizeof(ani));
	hdd_debug("num_freq: %d", num_freqs);
	if (QDF_IS_STATUS_ERROR(wlan_hdd_get_ani_level(adapter, ani,
						       parsed_freqs,
						       num_freqs))) {
		hdd_err("Unable to retrieve ani level");
		return -EINVAL;
	}

	extra = qdf_mem_malloc(user_size);
	if (!extra) {
		hdd_err("memory allocation failed");
		ret = -ENOMEM;
		goto parse_failed;
	}

	/*
	 * Find the number of channels that are populated. If freq is not
	 * filled then stop count there
	 */
	for (num_recv_channels = 0;
	     (num_recv_channels < num_freqs &&
	     ani[num_recv_channels].chan_freq); num_recv_channels++)
		;

	for (j = 0; j < num_recv_channels; j++) {
		/* Sanity check for ANI level validity */
		if (ani[j].ani_level > MAX_ANI_LEVEL)
			continue;

		copied_length += scnprintf(extra + copied_length,
					   user_size - copied_length, "%d:%d\n",
					   ani[j].chan_freq, ani[j].ani_level);
	}

	if (copied_length == 0) {
		hdd_err("ANI level not fetched");
		ret = -EINVAL;
		goto free;
	}

	hdd_debug("data: %s", extra);

	if (copy_to_user(priv_data->buf, extra, copied_length + 1)) {
		hdd_err("failed to copy data to user buffer");
		ret = -EFAULT;
		goto free;
	}

free:
	qdf_mem_free(extra);

parse_failed:
	return ret;
}

#else
static int drv_cmd_get_ani_level(struct hdd_adapter *adapter,
				 struct hdd_context *hdd_ctx,
				 uint8_t *command,
				 uint8_t command_len,
				 struct hdd_priv_data *priv_data)
{
	return 0;
}
#endif

#ifdef FUNC_CALL_MAP
static int drv_cmd_get_function_call_map(struct hdd_adapter *adapter,
					 struct hdd_context *hdd_ctx,
					 uint8_t *command,
					 uint8_t command_len,
					 struct hdd_priv_data *priv_data)
{
	char *cc_buf = qdf_mem_malloc(QDF_FUNCTION_CALL_MAP_BUF_LEN);
	uint8_t *param;
	int temp_int;

	param = strnchr(command, strlen(command), ' ');
	/*no argument after the command*/
	if (NULL == param)
		return -EINVAL;

	/*no space after the command*/
	else if (SPACE_ASCII_VALUE != *param)
		return -EINVAL;

	param++;

	/*removing empty spaces*/
	while ((SPACE_ASCII_VALUE  == *param) && ('\0' !=  *param))
		param++;

	/*no argument followed by spaces*/
	if ('\0' == *param)
		return -EINVAL;

	/*getting the first argument */
	if (sscanf(param, "%d ", &temp_int) != 1) {
		hdd_err("No option given");
		return -EINVAL;
	}

	if (temp_int < 0 || temp_int > 1) {
		hdd_err("Invalid option given");
		return -EINVAL;
	}

	/* Read the buffer */
	if (temp_int) {
	/*
	 * These logs are required as these indicates the start and end of the
	 * dump for the auto script to parse
	 */
		hdd_info("Function call map dump start");
		qdf_get_func_call_map(cc_buf);
		qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   cc_buf, QDF_FUNCTION_CALL_MAP_BUF_LEN);
		hdd_info("Function call map dump end");
	} else {
		qdf_clear_func_call_map();
		hdd_info("Function call map clear");
	}
	qdf_mem_free(cc_buf);

	return 0;
}
#endif

/*
 * The following table contains all supported WLAN HDD
 * IOCTL driver commands and the handler for each of them.
 */
static const struct hdd_drv_cmd hdd_drv_cmds[] = {
	{"P2P_DEV_ADDR",              drv_cmd_p2p_dev_addr, false},
	{"P2P_SET_NOA",               drv_cmd_p2p_set_noa, true},
	{"P2P_SET_PS",                drv_cmd_p2p_set_ps, true},
	{"SETBAND",                   drv_cmd_set_band, true},
	{"SETWMMPS",                  drv_cmd_set_wmmps, true},
	{"COUNTRY",                   drv_cmd_country, true},
	{"SETCOUNTRYREV",             drv_cmd_country, true},
	{"GETCOUNTRYREV",             drv_cmd_get_country, false},
	{"SETSUSPENDMODE",            drv_cmd_set_suspend_mode, true},
	{"SET_AP_WPS_P2P_IE",         drv_cmd_dummy, false},
	{"SETROAMTRIGGER",            drv_cmd_set_roam_trigger, true},
	{"GETROAMTRIGGER",            drv_cmd_get_roam_trigger, false},
	{"SETROAMSCANPERIOD",         drv_cmd_set_roam_scan_period, true},
	{"GETROAMSCANPERIOD",         drv_cmd_get_roam_scan_period, false},
	{"SETROAMSCANREFRESHPERIOD",  drv_cmd_set_roam_scan_refresh_period,
	 true},
	{"GETROAMSCANREFRESHPERIOD",  drv_cmd_get_roam_scan_refresh_period,
	 false},
	{"SETROAMMODE",               drv_cmd_set_roam_mode, true},
	{"GETROAMMODE",               drv_cmd_get_roam_mode, false},
	{"SETROAMDELTA",              drv_cmd_set_roam_delta, true},
	{"GETROAMDELTA",              drv_cmd_get_roam_delta, false},
	{"GETBAND",                   drv_cmd_get_band, false},
	{"SETROAMSCANCHANNELS",       drv_cmd_set_roam_scan_channels, true},
	{"GETROAMSCANCHANNELS",       drv_cmd_get_roam_scan_channels, false},
	{"GETCCXMODE",                drv_cmd_get_ccx_mode, false},
	{"GETOKCMODE",                drv_cmd_get_okc_mode, false},
	{"GETFASTROAM",               drv_cmd_get_fast_roam, false},
	{"GETFASTTRANSITION",         drv_cmd_get_fast_transition, false},
	{"SETROAMSCANCHANNELMINTIME", drv_cmd_set_roam_scan_channel_min_time,
	 true},
	{"SENDACTIONFRAME",           drv_cmd_send_action_frame, true},
	{"GETROAMSCANCHANNELMINTIME", drv_cmd_get_roam_scan_channel_min_time,
	 false},
	{"SETSCANCHANNELTIME",        drv_cmd_set_scan_channel_time, true},
	{"GETSCANCHANNELTIME",        drv_cmd_get_scan_channel_time, false},
	{"SETSCANHOMETIME",           drv_cmd_set_scan_home_time, true},
	{"GETSCANHOMETIME",           drv_cmd_get_scan_home_time, false},
	{"SETROAMINTRABAND",          drv_cmd_set_roam_intra_band, true},
	{"GETROAMINTRABAND",          drv_cmd_get_roam_intra_band, false},
	{"SETSCANNPROBES",            drv_cmd_set_scan_n_probes, true},
	{"GETSCANNPROBES",            drv_cmd_get_scan_n_probes, false},
	{"SETSCANHOMEAWAYTIME",       drv_cmd_set_scan_home_away_time, true},
	{"GETSCANHOMEAWAYTIME",       drv_cmd_get_scan_home_away_time, false},
	{"REASSOC",                   drv_cmd_reassoc, true},
	{"SETWESMODE",                drv_cmd_set_wes_mode, true},
	{"GETWESMODE",                drv_cmd_get_wes_mode, false},
	{"SETOPPORTUNISTICRSSIDIFF",  drv_cmd_set_opportunistic_rssi_diff,
	 true},
	{"GETOPPORTUNISTICRSSIDIFF",  drv_cmd_get_opportunistic_rssi_diff,
	 false},
	{"SETROAMRESCANRSSIDIFF",     drv_cmd_set_roam_rescan_rssi_diff, true},
	{"GETROAMRESCANRSSIDIFF",     drv_cmd_get_roam_rescan_rssi_diff, false},
	{"SETFASTROAM",               drv_cmd_set_fast_roam, true},
	{"SETFASTTRANSITION",         drv_cmd_set_fast_transition, true},
	{"FASTREASSOC",               drv_cmd_fast_reassoc, true},
	{"SETROAMSCANCONTROL",        drv_cmd_set_roam_scan_control, true},
	{"SETOKCMODE",                drv_cmd_set_okc_mode, true},
	{"GETROAMSCANCONTROL",        drv_cmd_get_roam_scan_control, false},
	{"BTCOEXMODE",                drv_cmd_bt_coex_mode, true},
	{"SCAN-ACTIVE",               drv_cmd_scan_active, false},
	{"SCAN-PASSIVE",              drv_cmd_scan_passive, false},
	{"CONCSETDWELLTIME",          drv_cmd_conc_set_dwell_time, true},
	{"GETDWELLTIME",              drv_cmd_get_dwell_time, false},
	{"SETDWELLTIME",              drv_cmd_set_dwell_time, true},
	{"MIRACAST",                  drv_cmd_miracast, true},
	{"SETIBSSBEACONOUIDATA",      drv_cmd_set_ibss_beacon_oui_data, true},
	{"SETRMCENABLE",              drv_cmd_set_rmc_enable, true},
	{"SETRMCACTIONPERIOD",        drv_cmd_set_rmc_action_period, true},
	{"GETIBSSPEERINFOALL",        drv_cmd_get_ibss_peer_info_all, false},
	{"GETIBSSPEERINFO",           drv_cmd_get_ibss_peer_info, true},
	{"SETRMCTXRATE",              drv_cmd_set_rmc_tx_rate, true},
	{"SETIBSSTXFAILEVENT",        drv_cmd_set_ibss_tx_fail_event, true},
#ifdef FEATURE_WLAN_ESE
	{"SETCCXROAMSCANCHANNELS",    drv_cmd_set_ccx_roam_scan_channels, true},
	{"GETTSMSTATS",               drv_cmd_get_tsm_stats, true},
	{"SETCCKMIE",                 drv_cmd_set_cckm_ie, true},
	{"CCXBEACONREQ",	      drv_cmd_ccx_beacon_req, true},
	{"CCXPLMREQ",                 drv_cmd_ccx_plm_req, true},
	{"SETCCXMODE",                drv_cmd_set_ccx_mode, true},
#endif /* FEATURE_WLAN_ESE */
	{"SETMCRATE",                 drv_cmd_set_mc_rate, true},
	{"MAXTXPOWER",                drv_cmd_max_tx_power, true},
	{"SETDFSSCANMODE",            drv_cmd_set_dfs_scan_mode, true},
	{"GETDFSSCANMODE",            drv_cmd_get_dfs_scan_mode, false},
	{"GETLINKSTATUS",             drv_cmd_get_link_status, false},
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	{"ENABLEEXTWOW",              drv_cmd_enable_ext_wow, true},
	{"SETAPP1PARAMS",             drv_cmd_set_app1_params, true},
	{"SETAPP2PARAMS",             drv_cmd_set_app2_params, true},
#endif
#ifdef FEATURE_WLAN_TDLS
	{"TDLSSECONDARYCHANNELOFFSET", drv_cmd_tdls_secondary_channel_offset,
	 true},
	{"TDLSOFFCHANNELMODE",        drv_cmd_tdls_off_channel_mode, true},
	{"TDLSOFFCHANNEL",            drv_cmd_tdls_off_channel, true},
	{"TDLSSCAN",                  drv_cmd_tdls_scan, true},
#endif
	{"RSSI",                      drv_cmd_get_rssi, false},
	{"LINKSPEED",                 drv_cmd_get_linkspeed, false},
	{"RXFILTER-REMOVE",           drv_cmd_rx_filter_remove, true},
	{"RXFILTER-ADD",              drv_cmd_rx_filter_add, true},
	{"SET_FCC_CHANNEL",           drv_cmd_set_fcc_channel, true},
	{"CHANNEL_SWITCH",            drv_cmd_set_channel_switch, true},
	{"SETANTENNAMODE",            drv_cmd_set_antenna_mode, true},
	{"GETANTENNAMODE",            drv_cmd_get_antenna_mode, false},
	{"SET_DISABLE_CHANNEL_LIST",  drv_cmd_set_disable_chan_list, true},
	{"GET_DISABLE_CHANNEL_LIST",  drv_cmd_get_disable_chan_list, false},
	{"GET_ANI_LEVEL",             drv_cmd_get_ani_level, false},
#ifdef FUNC_CALL_MAP
	{"GET_FUNCTION_CALL_MAP",     drv_cmd_get_function_call_map, true},
#endif
	{"STOP",                      drv_cmd_dummy, false},
	/* Deprecated commands */
	{"RXFILTER-START",            drv_cmd_dummy, false},
	{"RXFILTER-STOP",             drv_cmd_dummy, false},
	{"BTCOEXSCAN-START",          drv_cmd_dummy, false},
	{"BTCOEXSCAN-STOP",           drv_cmd_dummy, false},
};

/**
 * hdd_drv_cmd_process() - chooses and runs the proper
 *                                handler based on the input command
 * @adapter:	Pointer to the hdd adapter
 * @cmd:	Pointer to the driver command
 * @priv_data:	Pointer to the data associated with the command
 *
 * This function parses the input hdd driver command and runs
 * the proper handler
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_drv_cmd_process(struct hdd_adapter *adapter,
			       uint8_t *cmd,
			       struct hdd_priv_data *priv_data)
{
	struct hdd_context *hdd_ctx;
	int i;
	const int cmd_num_total = ARRAY_SIZE(hdd_drv_cmds);
	uint8_t *cmd_i = NULL;
	hdd_drv_cmd_handler_t handler = NULL;
	int len = 0, cmd_len = 0;
	uint8_t *ptr;
	bool args;

	if (!adapter || !cmd || !priv_data) {
		hdd_err("at least 1 param is NULL");
		return -EINVAL;
	}

	/* Calculate length of the first word */
	ptr = strchrnul(cmd, ' ');
	cmd_len = ptr - cmd;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	for (i = 0; i < cmd_num_total; i++) {

		cmd_i = (uint8_t *)hdd_drv_cmds[i].cmd;
		handler = hdd_drv_cmds[i].handler;
		len = strlen(cmd_i);
		args = hdd_drv_cmds[i].args;

		if (!handler) {
			hdd_err("no. %d handler is NULL", i);
			return -EINVAL;
		}

		if (len == cmd_len && strncasecmp(cmd, cmd_i, len) == 0) {
			if (args && drv_cmd_validate(cmd, len))
				return -EINVAL;

			return handler(adapter, hdd_ctx,
				       cmd, len, priv_data);
		}
	}

	return drv_cmd_invalid(adapter, hdd_ctx, cmd, len, priv_data);
}

/**
 * hdd_driver_command() - top level wlan hdd driver command handler
 * @adapter:	Pointer to the hdd adapter
 * @priv_data:	Pointer to the raw command data
 *
 * This function is the top level wlan hdd driver command handler. It
 * handles the command with the help of hdd_drv_cmd_process()
 *
 * Return: 0 for success non-zero for failure
 */
static int hdd_driver_command(struct hdd_adapter *adapter,
			      struct hdd_priv_data *priv_data)
{
	uint8_t *command = NULL;
	int ret = 0;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_enter();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_err("Driver module is closed; command can not be processed");
		return -EINVAL;
	}

	/*
	 * Note that valid pointers are provided by caller
	 */

	/* copy to local struct to avoid numerous changes to legacy code */
	if (priv_data->total_len <= 0 ||
	    priv_data->total_len > WLAN_PRIV_DATA_MAX_LEN) {
		hdd_warn("Invalid priv_data.total_len: %d!!!",
			  priv_data->total_len);
		ret = -EINVAL;
		goto exit;
	}

	/* Allocate +1 for '\0' */
	command = qdf_mem_malloc(priv_data->total_len + 1);
	if (!command) {
		hdd_err("failed to allocate memory");
		ret = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(command, priv_data->buf, priv_data->total_len)) {
		ret = -EFAULT;
		goto exit;
	}

	/* Make sure the command is NUL-terminated */
	command[priv_data->total_len] = '\0';

	hdd_debug("%s: %s", adapter->dev->name, command);
	ret = hdd_drv_cmd_process(adapter, command, priv_data);

exit:
	if (command)
		qdf_mem_free(command);
	hdd_exit();
	return ret;
}

#ifdef CONFIG_COMPAT
static int hdd_driver_compat_ioctl(struct hdd_adapter *adapter, struct ifreq *ifr)
{
	struct {
		compat_uptr_t buf;
		int used_len;
		int total_len;
	} compat_priv_data;
	struct hdd_priv_data priv_data;
	int ret = 0;

	/*
	 * Note that adapter and ifr have already been verified by caller,
	 * and HDD context has also been validated
	 */
	if (copy_from_user(&compat_priv_data, ifr->ifr_data,
			   sizeof(compat_priv_data))) {
		ret = -EFAULT;
		goto exit;
	}
	priv_data.buf = compat_ptr(compat_priv_data.buf);
	priv_data.used_len = compat_priv_data.used_len;
	priv_data.total_len = compat_priv_data.total_len;
	ret = hdd_driver_command(adapter, &priv_data);
exit:
	return ret;
}
#else /* CONFIG_COMPAT */
static int hdd_driver_compat_ioctl(struct hdd_adapter *adapter, struct ifreq *ifr)
{
	/* will never be invoked */
	return 0;
}
#endif /* CONFIG_COMPAT */

static int hdd_driver_ioctl(struct hdd_adapter *adapter, struct ifreq *ifr)
{
	struct hdd_priv_data priv_data;
	int ret = 0;

	/*
	 * Note that adapter and ifr have already been verified by caller,
	 * and HDD context has also been validated
	 */
	if (copy_from_user(&priv_data, ifr->ifr_data, sizeof(priv_data)))
		ret = -EFAULT;
	else
		ret = hdd_driver_command(adapter, &priv_data);

	return ret;
}

/**
 * __hdd_ioctl() - ioctl handler for wlan network interfaces
 * @dev: device upon which the ioctl was received
 * @ifr: ioctl request information
 * @cmd: ioctl command
 *
 * This function does initial processing of wlan device ioctls.
 * Currently two flavors of ioctls are supported.  The primary ioctl
 * that is supported is the (SIOCDEVPRIVATE + 1) ioctl which is used
 * for Android "DRIVER" commands.  The other ioctl that is
 * conditionally supported is the SIOCIOCTLTX99 ioctl which is used
 * for FTM on some platforms.  This function simply verifies that the
 * driver is in a sane state, and that the ioctl is one of the
 * supported flavors, in which case flavor-specific handlers are
 * dispatched.
 *
 * Return: 0 on success, non-zero on error
 */
static int __hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	int ret;

	hdd_enter_dev(dev);

	if (dev != adapter->dev) {
		hdd_err("HDD adapter/dev inconsistency");
		ret = -ENODEV;
		goto exit;
	}

	if ((!ifr) || (!ifr->ifr_data)) {
		hdd_err("invalid data cmd: %d", cmd);
		ret = -EINVAL;
		goto exit;
	}
#if  defined(QCA_WIFI_FTM) && defined(LINUX_QCMBR)
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		if (SIOCIOCTLTX99 == cmd) {
			ret = wlan_hdd_qcmbr_unified_ioctl(adapter, ifr);
			goto exit;
		}
	}
#endif

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		goto exit;

	switch (cmd) {
	case (SIOCDEVPRIVATE + 1):
		if (in_compat_syscall())
			ret = hdd_driver_compat_ioctl(adapter, ifr);
		else
			ret = hdd_driver_ioctl(adapter, ifr);
		break;
	default:
		hdd_warn("unknown ioctl %d", cmd);
		ret = -EINVAL;
		break;
	}
exit:
	hdd_exit();
	return ret;
}

/**
 * hdd_ioctl() - ioctl handler (wrapper) for wlan network interfaces
 * @dev: device upon which the ioctl was received
 * @ifr: ioctl request information
 * @cmd: ioctl command
 *
 * This function acts as an SSR-protecting wrapper to __hdd_ioctl()
 * which is where the ioctls are really handled.
 *
 * Return: 0 on success, non-zero on error
 */
int hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_ioctl(dev, ifr, cmd);
	cds_ssr_unprotect(__func__);
	return ret;
}

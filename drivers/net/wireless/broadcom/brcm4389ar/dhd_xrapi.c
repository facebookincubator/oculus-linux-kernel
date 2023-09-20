/**
* @file Broadcom Dongle Host Driver (DHD), xrapi handler
*
* handles xrapi reqeust, messages*
* Copyright (C) 2022, Broadcom.
*
*      Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2 (the "GPL"),
* available at http://www.broadcom.com/licenses/GPLv2.php, with the
* following added to such license:
*
*      As a special exception, the copyright holders of this software give you
* permission to link this software with independent modules, and to copy and
* distribute the resulting executable under terms of your choice, provided that
* you also meet, for each linked independent module, the terms and conditions of
* the license of that module.  An independent module is a module which is not
* derived from this software.  The special exception does not apply to any
* modifications of the software.
*
*
* <<Broadcom-WL-IPTag/Vendor1284:>>
*/

#include <typedefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmevent.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <wlioctl.h>
#include <dhd_xrapi.h>
#include <wl_android.h>
#ifdef CONFIG_XRPS_DHD_HOOKS
#include "xrps.h"
#endif /* CONFIG_XRPS_DHD_HOOKS */

#if defined(DHD_MAGIC_PKT_FILTER)
/* Make sure PKT_FILTER_SUPPORT is not defined, it's exclusive */
#if defined(PKT_FILTER_SUPPORT)
#error "PKT_FILTER_SUPPORT not allowed"
#endif /* PKT_FILTER_SUPPORT */

/* Wake filter */
#define XRAPI_WAKE_PATTERN_SIZE		(ETHER_ADDR_LEN*17)
#define XRAPI_WAKE_MASK_SIZE		(ETHER_ADDR_LEN*17)
#define XRAPI_WAKE_PKT_FILTER_MIN_ID	128
#define XRAPI_WAKE_PKT_PATTERN_ITER	16u
#endif /* DHD_MAGIC_PKT_FILTER */

#ifdef TSF_GSYNC
int dhd_tsf_gsync_handler(dhd_pub_t *dhd, const wl_event_msg_t *event, void *event_data)
{
	int ret = BCME_OK;
	uint32 status;
	tsf_gsync_result_t *result = (tsf_gsync_result_t *)event_data;

	status = ntoh32(event->status);

	switch (status) {
	case WLC_E_STATUS_SUCCESS:
	{
#ifdef XRAPI_IN_USER_SPACE
		wl_android_xrapi_notify_time_sync_event(
			dhd_linux_get_primary_netdev(dhd), event_data,
			sizeof(tsf_gsync_result_t));
#endif //XRAPI_IN_USER_SPACE
		if (result == NULL) {
			return -EINVAL;
		}

		DHD_ERROR(("%s: ReqID=%d TSF=0x%016llx TS_BCN=0x%016llx LOCAL_TSF_BCN=0x%016llx\n",
			__func__, result->req_id,
			((uint64)result->tsf.tsf_h << 32u)|(result->tsf.tsf_l),
			((uint64)result->ts_bcn.tsf_h << 32u)|(result->ts_bcn.tsf_l),
			((uint64)result->ltsf_bcn.tsf_h << 32u)|(result->ltsf_bcn.tsf_l)));
		break;
	}

	case WLC_E_STATUS_FAIL:
	{
		uint32 err_reason = ntoh32(event->reason);

		DHD_ERROR(("%s: TSF read fail. request ID: %d err: %d\n",
			__FUNCTION__, result->req_id, err_reason));
		break;
	}
	default:
		DHD_ERROR(("%s: Unknown Event\n", __FUNCTION__));
	}

	return ret;
}
#endif /* TSF_GSYNC */

/* EOT frame handler */
void dhd_xrapi_eot_handler(dhd_pub_t *dhdp, void * pktbuf)
{
	DHD_TRACE(("dhd_xrapi_eot_handler: Enter\n"));
#ifdef CONFIG_XRPS_DHD_HOOKS
	if (xrps_is_init())
		dhdp->xrps_intf->rx_eot_cb(pktbuf);
#endif /* CONFIG_XRPS_DHD_HOOKS */
	PKTFREE(dhdp->osh, pktbuf, FALSE);
	return;
}

#ifdef QFLUSH_LOG
/* detach for free and clean up */
int dhd_xrapi_qflushlog_detach(dhd_pub_t *dhdp)
{
	int result = BCME_ERROR;

	if (dhdp != NULL) {
		dhdp->txs_histo.idx = 0;
		dhdp->txs_histo.trigger_cnt = 0;

		if (dhdp->txs_histo.txstatus_xrapi != NULL) {
			MFREE(dhdp->osh, dhdp->txs_histo.txstatus_xrapi,
				MAX_TXSTATUS_HISTORY *
				sizeof(*(dhdp->txs_histo.txstatus_xrapi)));
		}
		dhdp->txs_histo.txstatus_xrapi = NULL;

		result = BCME_OK;
	}

	return result;
}

/* attach for alloc and init */
int dhd_xrapi_qflushlog_attach(dhd_pub_t *dhdp)
{
	int result = BCME_ERROR;

	if (dhdp != NULL) {
		dhdp->txs_histo.idx = 0;
		dhdp->txs_histo.trigger_cnt = 0;
		dhdp->txs_histo.txstatus_xrapi = (txstatus_xrapi_t*)MALLOCZ(dhdp->osh,
			MAX_TXSTATUS_HISTORY * sizeof(*(dhdp->txs_histo.txstatus_xrapi)));

		if (dhdp->txs_histo.txstatus_xrapi != NULL) {
			result = BCME_OK;
		}
	}

	if (result != BCME_OK) {
		DHD_ERROR(("%s:\tMALLOC of qflushlog failed\n",
			__FUNCTION__));
	}

	return result;
}

/*
 * This function will log txstatus history
 * when flush flowring is requested to verify
 * the match of pktid and seq num.
 */
void dhd_msgbuf_txstatus_histo(dhd_pub_t *dhdp, host_txbuf_cmpl_t *txstatus)
{
	int idx = 0;
	uint16 wlfc_status = ltoh16(txstatus->compl_hdr.status) & WLFC_CTL_PKTFLAG_MASK;

	if (dhdp->txs_histo.idx > MAX_TXSTATUS_HISTORY - 1) {
		dhdp->txs_histo.idx = 0;
	}
	// logging to circular buffer for txstatus
	dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].wlfc_status = wlfc_status;
	dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].ringid =
		txstatus->compl_hdr.flow_ring_id;
	dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].pktid = txstatus->cmn_hdr.request_id;
	dhdp->txs_histo.idx++;

	if (wlfc_status	== WLFC_CTL_PKTFLAG_DROPPED ||
		wlfc_status == WLFC_CTL_PKTFLAG_EXPIRED) {
		dhdp->txs_histo.trigger_cnt++;
	}
	else if (dhdp->txs_histo.trigger_cnt > 0) { /* triggered already then prints */
		dhdp->txs_histo.trigger_cnt = MAX_TXSTATUS_NG;
	}

	if (dhdp->txs_histo.trigger_cnt >= MAX_TXSTATUS_NG) {
		dhdp->flush_logging = FALSE;

		/* print MAX_TXSTATUS_HISTORY(20) history of txstatus */
		dhdp->txs_histo.trigger_cnt = 0;
		idx = dhdp->txs_histo.idx;
		DHD_ERROR(("[Qflush - DHD] ----------------- txstatus stats ---------------\n"));
		while (dhdp->txs_histo.idx < MAX_TXSTATUS_HISTORY) {
			DHD_ERROR(("[Qflush - DHD] [%d]: wlfc_status=%d, ringid=%u, pktid=%u\n",
				dhdp->txs_histo.idx,
				dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].wlfc_status,
				dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].ringid,
				dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].pktid));
			dhdp->txs_histo.idx++;
		}
		dhdp->txs_histo.idx = 0;
		while (dhdp->txs_histo.idx < idx) {
			DHD_ERROR(("[Qflush - DHD] [%d]: wlfc_status=%d, ringid=%u, pktid=%u\n",
				dhdp->txs_histo.idx,
				dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].wlfc_status,
				dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].ringid,
				dhdp->txs_histo.txstatus_xrapi[dhdp->txs_histo.idx].pktid));
			dhdp->txs_histo.idx++;
		}
		DHD_ERROR(("[Qflush - DHD] ------------------------------------------------\n"));
	}
}
#endif /* QFLUSH_LOG */

int dhd_xrapi_softap_psmode_handler(dhd_pub_t *dhd, const wl_event_msg_t *event)
{
	int ret = BCME_OK;
	uint32 status;
	int reason;

	DHD_TRACE(("%s: Enter\n", __FUNCTION__));

	status = ntoh32(event->status);
	reason = ntoh32(event->reason);

	switch (status) {
	case WLC_E_XR_SOFTAP_PSMODE_SLEEP:
	case WLC_E_XR_SOFTAP_PSMODE_AWAKE:
	{
		if (reason == BCME_OK) {
			DHD_ERROR(("%s: SoftAP PSmode change as %d(%s)\n",
				__FUNCTION__, status,
				(status == WLC_E_XR_SOFTAP_PSMODE_SLEEP) ? "SLEEP" : "AWAKE"));
		} else {
			DHD_ERROR(("%s: Failed to change SoftAP PSmode as %d(%s) - reason %d\n",
				__FUNCTION__, status,
				(status == WLC_E_XR_SOFTAP_PSMODE_SLEEP) ? "SLEEP" : "AWAKE",
				reason));
			ret = reason;
		}
#ifdef XRAPI_IN_USER_SPACE
		wl_android_xrapi_notify_psmode_event(
			dhd_linux_get_primary_netdev(dhd), &status,
			sizeof(uint32));
#endif //XRAPI_IN_USER_SPACE
		break;
	}
	default:
		DHD_ERROR(("%s: Unknown Event\n", __FUNCTION__));
		ret = BCME_NOTFOUND;
	}

	return ret;
}

#if defined(DHD_MAGIC_PKT_FILTER)
int dhd_xrapi_validate_pkt_filter(dhd_pub_t *dhd)
{
	char iovbuf[WLC_IOCTL_SMLEN];
	struct ether_addr cur_mac;
	int ret;

	if (!dhd) {
		DHD_ERROR(("%s: dhdp is NULL\n", __FUNCTION__));
		return BCME_BADARG;
	}

	/* Read the current mac address */
	ret = dhd_iovar(dhd, 0, "cur_etheraddr", NULL, 0, iovbuf, sizeof(iovbuf), FALSE);
	if (ret < 0) {
		DHD_ERROR(("%s: Can't get the default MAC address\n", __FUNCTION__));
		return BCME_NOTUP;
	}

	/* Update the current MAC address */
	memcpy_s(&cur_mac, ETHER_ADDR_LEN, iovbuf, ETHER_ADDR_LEN);

	/* Check if cur mac is updated */
	if (memcmp(&cur_mac, &(dhd->filter_mac), ETHER_ADDR_LEN)) {
		DHD_ERROR(("%s: MAC is updated, change the filter_mac\n", __FUNCTION__));
		/* Delete installed magic filter */
		dhd_wl_ioctl_set_intiovar(dhd, "pkt_filter_delete",
			XRAPI_WAKE_PKT_FILTER_MIN_ID, WLC_SET_VAR, TRUE, 0);
		/* Re-install the magic filter */
		return dhd_xrapi_install_wake_pkt_filter(dhd);
	}

	return BCME_OK;
}

int dhd_xrapi_install_wake_pkt_filter(dhd_pub_t *dhd)
{
	char iovbuf[WLC_IOCTL_SMLEN];
	struct ether_addr *filter_mac;
	wl_pkt_filter_t *pkt_filterp;
	uint8 *mask, *pattern;
	uint8 buf[sizeof(wl_pkt_filter_t) + XRAPI_WAKE_MASK_SIZE + XRAPI_WAKE_PATTERN_SIZE];
	int ret, i;

	if (!dhd) {
		DHD_ERROR(("%s: dhdp is NULL\n", __FUNCTION__));
		return BCME_BADARG;
	}

	filter_mac = &dhd->filter_mac;
	/* Read current MAC address */
	ret = dhd_iovar(dhd, 0, "cur_etheraddr", NULL, 0, iovbuf, sizeof(iovbuf), FALSE);
	if (ret < 0) {
		DHD_ERROR(("%s: Can't get the default MAC address\n", __FUNCTION__));
		return BCME_NOTUP;
	}

	/* Update the default filter_mac address */
	memcpy_s(filter_mac, ETHER_ADDR_LEN, iovbuf, ETHER_ADDR_LEN);

	/* Update filter */
	pkt_filterp = (wl_pkt_filter_t *) buf;
	bzero(buf, sizeof(buf));

	pkt_filterp->id = XRAPI_WAKE_PKT_FILTER_MIN_ID;
	pkt_filterp->negate_match = 0;
	pkt_filterp->type = WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH;
	pkt_filterp->u.pattern.offset = ETHER_HDR_LEN;
	pkt_filterp->u.pattern.size_bytes = XRAPI_WAKE_PATTERN_SIZE;

	mask = pkt_filterp->u.pattern.mask_and_pattern;
	memset(mask, 0xFF, XRAPI_WAKE_MASK_SIZE);
	pattern = mask + XRAPI_WAKE_MASK_SIZE;
	memset(pattern, 0xFF, ETHER_ADDR_LEN);

	for (i = 0u; i < XRAPI_WAKE_PKT_PATTERN_ITER; i++) {
		(void)memcpy_s(&pattern[ETHER_ADDR_LEN + i * ETHER_ADDR_LEN], ETHER_ADDR_LEN,
			(uint8*)filter_mac, ETHER_ADDR_LEN);
	}

	/* Install packet filter */
	ret = dhd_iovar(dhd, 0, "pkt_filter_add", buf, WL_PKT_FILTER_FIXED_LEN +
			WL_PKT_FILTER_PATTERN_FIXED_LEN + XRAPI_WAKE_MASK_SIZE +
			XRAPI_WAKE_PATTERN_SIZE, NULL, 0, TRUE);

	if (ret == BCME_OK) {
		DHD_INFO(("%s: installed\n", __FUNCTION__));
	} else {
		DHD_ERROR(("%s: pkt_filter_add failed, ret %d\n", __FUNCTION__, ret));
	}
	return ret;
}

int dhd_xrapi_enable_wake_pkt_filter(dhd_pub_t *dhd, bool enable)
{
	wl_pkt_filter_enable_t pkt_flt_en;
	int ret;

	pkt_flt_en.id = XRAPI_WAKE_PKT_FILTER_MIN_ID;
	pkt_flt_en.enable = enable;

	/* PKT Filter is not allowed if SoftAP activated */
	if ((dhd->op_mode & DHD_FLAG_HOSTAP_MODE) && enable) {
		DHD_ERROR(("%s: DHD_FLAG_HOSTAP_MODE\n", __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	/* PKT filter is not allowed if DHD is not in STA support mode */
	if (enable && (!dhd_support_sta_mode(dhd))) {
		DHD_ERROR(("%s: packet filter NOT allowed\n", __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	/* Verify the installed filter_mac is correct */
	if (enable) {
		ret = dhd_xrapi_validate_pkt_filter(dhd);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s: pkt_filter_verification failed ret %d\n",
				__FUNCTION__, ret));
			return ret;
		}
	}
	ret = dhd_iovar(dhd, 0, "pkt_filter_enable", (char *)&pkt_flt_en,
			sizeof(pkt_flt_en), NULL, 0, TRUE);

	if (ret == BCME_OK) {
		DHD_INFO(("%s: %s\n", __FUNCTION__, enable ? "enabled" : "disabled"));
	} else {
		DHD_ERROR(("%s: pkt_filter_%s failed ret %d\n", __FUNCTION__,
			enable ? "enable" : "disable", ret));
	}

	return ret;
}
#endif /* DHD_MAGIC_PKT_FILTER */

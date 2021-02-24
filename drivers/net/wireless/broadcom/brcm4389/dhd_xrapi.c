/**
* @file Broadcom Dongle Host Driver (DHD), xrapi handler
*
* handles xrapi reqeust, messages*
* Copyright (C) 2020, Broadcom.
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
		uint64 tsf64 = 0;

		tsf64 = result->tsf.tsf_h;
		tsf64 <<= 32u;
		tsf64 |= result->tsf.tsf_l;
		DHD_ERROR(("%s: Request ID: %d TSF_H=0x%08x TSF_L=0x%08x, TSF=0x%016llx\n",
			__FUNCTION__, result->req_id, result->tsf.tsf_h, result->tsf.tsf_l, tsf64));
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
	// TBD by customer
	PKTFREE(dhdp->osh, pktbuf, FALSE);
	return;
}

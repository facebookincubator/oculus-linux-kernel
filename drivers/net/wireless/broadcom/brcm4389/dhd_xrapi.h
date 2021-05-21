/*
* Header file describing the xrapi functionality
*
* Provides type definitions and function prototypes used to handle xrapi functionality.
*
* Copyright (C) 2021, Broadcom.
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

#ifndef _dhd_xrapi_h_
#define _dhd_xrapi_h_

#ifdef TSF_GSYNC
extern int
dhd_tsf_gsync_handler(dhd_pub_t *dhd, const wl_event_msg_t *event, void *event_data);
#endif /* TSF_GSYNC */
extern void
dhd_xrapi_eot_handler(dhd_pub_t *dhdp, void * pktbuf);

#ifdef QFLUSH_LOG

#include <bcmmsgbuf.h>
#define MAX_TXSTATUS_HISTORY 30u
#define MAX_TXSTATUS_NG 27u

extern int
dhd_xrapi_qflushlog_detach(dhd_pub_t *dhdp);
extern int
dhd_xrapi_qflushlog_attach(dhd_pub_t *dhdp);
extern void
dhd_msgbuf_txstatus_histo(dhd_pub_t *dhd, host_txbuf_cmpl_t * txstatus);
#endif /* QFLUSH_LOG */

extern int
dhd_xrapi_softap_psmode_handler(dhd_pub_t *dhd, const wl_event_msg_t *event);
#if defined(DHD_MAGIC_PKT_FILTER)
extern int
dhd_xrapi_install_wake_pkt_filter(dhd_pub_t *dhd);
extern int
dhd_xrapi_enable_wake_pkt_filter(dhd_pub_t *dhd, bool enable);
extern int
dhd_xrapi_validate_pkt_filter(dhd_pub_t *dhd);
#endif /* DHD_MAGIC_PKT_FILTER */

#endif  /* _dhd_xrapi_h_ */

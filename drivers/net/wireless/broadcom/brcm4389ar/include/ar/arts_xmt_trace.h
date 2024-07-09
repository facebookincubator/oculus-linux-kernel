/*
 * (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
 */

/***************************************************************************************************
 *
 * @file arts_xmt_trace.h
 *
 * @brief This is Meta specific custom header for tracing ARTS TX packets
 *
 * @details
 *
 **************************************************************************************************/

#ifndef _meta_arts_xmt_trace_h_
#define _meta_arts_xmt_trace_h_
/***************************************************************************************************
 * Includes
 **************************************************************************************************/
#include "ar/include/ARTS_header.h"
#include "ar/arts_common.h"

#include <dhd.h>
#include <dhd_dbg.h>

/***************************************************************************************************
 * Macro Definitions
 **************************************************************************************************/
/* Time taken to pick up packet from qdisc queue to when it is queued to flowring for transmission */
#define ARTS_HIGH_TX_START_THRESHOLD_US  1E4   /* 10ms */
/* Time taken to send packet from flowring to device i.e. when complete is called */
#define ARTS_HIGH_TX_XMIT_THRESHOLD_US   2E4   /* 20ms */
/***************************************************************************************************
 * Type Definitions
 **************************************************************************************************/

/***************************************************************************************************
 * Constant Definitions
 **************************************************************************************************/

/***************************************************************************************************
 * Exported Variable Declarations
 **************************************************************************************************/

/***************************************************************************************************
 * Exported Function Prototypes
 **************************************************************************************************/
inline static ARTS_header_t *arts_check_get_header(uint8 *pktdata) {
    uint8 *arts_pktbody = NULL;
    if (pktdata == NULL || !AR_IS_ARTS_PACKET((struct ether_header *)pktdata)) {
        return NULL;
    }
    arts_pktbody = (uint8 *)pktdata + sizeof(struct ether_header);
    if (*((uint16 *)(arts_pktbody)) != hton16(ARTS_ETH_HEAD_MAGIC)) {
        return NULL;
    }
    return (ARTS_header_t *)arts_pktbody;
}

inline static void arts_xmt_start(void *pkt, uint8 *pktdata) {
    (void)pktdata;
#if defined(AR_ARTS_TX_PKT_MONITOR)
	/* set the time when pkt is picked for xmit from qdisc */
	DHD_PKT_SET_QTIME(pkt, OSL_SYSUPTIME_US());
#else  /* AR_ARTS_TX_PKT_MONITOR */
    (void)pkt;
#endif /* AR_ARTS_TX_PKT_MONITOR */
}

inline static void arts_xmt_send(void *pkt, uint8 *pktdata) {
#if defined(AR_ARTS_TX_PKT_MONITOR)
    uint64 ts =  OSL_SYSUPTIME_US();
    uint64 elapsed = ts - DHD_PKT_GET_QTIME(pkt);
    ARTS_header_t *arts_header = arts_check_get_header(pktdata);
    if (arts_header) {
        if (arts_header->timestamp == 0) {
            // Fill in timestamp if not already set by WIPC. The ARTS header version would still be 1.
            // This will help determine latency in Acro WIPC.
            arts_header->timestamp = (uint32_t)ts;
        }
        if (elapsed > ARTS_HIGH_TX_START_THRESHOLD_US) {
            DHD_ERROR((
                "WiFi:INFO TX: START took %llu us @%llu for 0x%X->0x%X seqnum=%hhu size=%hu offset=%u tsize=%u ts=%u cpu=%d [%p]\n",
                elapsed,
                ts,
                arts_header->src,
                arts_header->dst,
                arts_header->seq_number,
                arts_header->segment_size,
                arts_header->segment_offset,
                arts_header->transfer_size,
                arts_header->timestamp,
                get_cpu(),
                pkt));
        }
    }
#else  /* AR_ARTS_TX_PKT_MONITOR */
    (void)pkt;
    (void)pktdata;
#endif /* AR_ARTS_TX_PKT_MONITOR */
}

inline static void arts_xmt_complete(void *pkt, uint8 *pktdata, bool success) {
    ARTS_header_t *arts_header = NULL;
    uint64 elapsed = 0;
#if defined(AR_ARTS_TX_PKT_MONITOR)
    uint64 ts =  OSL_SYSUPTIME_US();
    elapsed = ts - DHD_PKT_GET_QTIME(pkt);
    if (elapsed > ARTS_HIGH_TX_XMIT_THRESHOLD_US) {
        arts_header = arts_check_get_header(pktdata);
        if (arts_header) {
            DHD_ERROR((
                "WiFi:INFO TX: XMIT took %llu us @%llu for 0x%X->0x%X seqnum=%hhu size=%hu offset=%u tsize=%u ts=%u cpu=%d [%p]\n",
                elapsed,
                ts,
                arts_header->src,
                arts_header->dst,
                arts_header->seq_number,
                arts_header->segment_size,
                arts_header->segment_offset,
                arts_header->transfer_size,
                arts_header->timestamp,
                get_cpu(),
                pkt));
        }
    }
#endif /* AR_ARTS_TX_PKT_MONITOR */
    if (success) {
        return;
    }
    if (!arts_header) {
        arts_header = arts_check_get_header(pktdata);
    }
    if (arts_header) {
        DHD_ERROR((
            "WiFi:ERROR TX ARTS pkt dropped after %llu us. 0x%X->0x%X seqnum=%hhu size=%hu offset=%u tsize=%u ts=%u [%p]\n",
            elapsed,
            arts_header->src,
            arts_header->dst,
            arts_header->seq_number,
            arts_header->segment_size,
            arts_header->segment_offset,
            arts_header->transfer_size,
            arts_header->timestamp,
            pkt));
    }
}

#endif /*_meta_arts_xmt_trace_h_*/

/*
 * Copyright Meta Platforms, Inc. and its affiliates.
 *
 * NOTICE OF CONFIDENTIAL AND PROPRIETARY INFORMATION & TECHNOLOGY:
 * The information and technology contained herein (including the accompanying binary code)
 * is the confidential information of Meta Platforms, Inc. and its affiliates (collectively,
 * "Meta"). It is protected by applicable copyright and trade secret law, and may be claimed
 * in one or more U.S. or foreign patents or pending patent applications. Meta retains all right,
 * title and interest (including all intellectual property rights) in such information and
 * technology, and no licenses are hereby granted by Meta. Unauthorized use, reproduction, or
 * dissemination is a violation of Meta's rights and is strictly prohibited.
 */

#ifndef _performance_custom_h_
#define _performance_custom_h_

#include <typedefs.h>

#ifndef ETHERTYPE_FACEBOOK_USB
#define ETHERTYPE_FACEBOOK_USB 0x88B5
#endif
/***************************************************************************************************
 * Type Definitions
 **************************************************************************************************/
typedef enum {
	DHD_START_XMIT,
	DHD_SEND_PKT,
	DHD_BUS_TX_DATA,
	DHD_FLOW_ENQUEUE,
	DHD_FLOW_DEQUEUE,
	DHD_PROT_TXDATA,
	DHD_TXCOMPLETE,
	DHD_PROT_MSGBUF_RXCMP,
	DHD_RX_FRAME_EXIT,
	// New entries above this line
	NUM_LATENCY_EVENT,
} latency_event_t;

/***************************************************************************************************
 * Exported Function Prototypes
 **************************************************************************************************/
#ifdef ENABLE_PERFORMANCE_DEBUG
/**
 * @brief Mark a specific event has ocurred:
 * capture the timestamp of the event and store it in internal buffers which can later be queried to
 * detemine the runtime of operations between events.
 *
 * @param event the latency_event_t which has occurred, and should be captured
 * @param pktdata the packet pointer
 */
extern void latency_event_mark(latency_event_t event, uint8_t *pktdata);

#endif /*ENABLE_PERFORMANCE_DEBUG*/

#endif /*_performance_custom_h_*/

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

/***************************************************************************************************
 *
 * @file performance_custom.c
 *
 * @brief This module provides API for tracing the latency on the tx/rx data path in wifi driver.
 * Will also be used to trace other performance statistics, such as packet status, throughput.
 *
 **************************************************************************************************/
#include <performance_custom.h>
#include <dhd_dbg.h>
#include <linux/ktime.h>

#define CURRENT_TIME_NS() ktime_to_ns(ktime_get_boottime())
#define JANUS_STD_MSG_ID_ECHO (0xFE03)
#define JANUS_STD_MSG_ID_ACK (0xFE00)
#define JANUS_STD_MSG_ID_NACK (0xFE01)
// Reference from WipcUtils.h
#define CONFIG_ETH_DESG_HEADROOM 16

// Those defines needs to match the ARTS msg format
typedef struct {
	uint16_t stream_id;
	uint16_t seq_number;
	uint16_t segment_size;
	uint16_t indirect_flag : 1; // Bit 0: Indirect/direct message type
	uint16_t reliability_flag : 1; // Bit 1: reliable/best effort message type
	uint16_t reserved : 14;
	uint32_t transfer_size;
	uint32_t segment_offset;
} ARTS_header_t;

// After ARTS header, there is ipcHeaderBuffer,
// which holds 12-byte IPC header and 4-byte payload size field
/* 4-byte */
typedef struct {
	uint8_t protocol; /* support multiple header format */
	uint8_t flag; /* bitmask for flags */
	uint16_t hdr_size; /* reserved */
} ipc_hdr_fmt_t;

/* 4-byte */
/* Defined by Comm stack */
typedef struct {
	uint16_t dst; /* dst logic endpoint id */
	uint16_t src; /* src logic endpoint id */
} ipc_ep_pair_t;

/* Used and defined by FAST IPC */
typedef struct {
	uint8_t dst; /* dst core id */
	uint8_t ch; /* channel id  */
	uint8_t src; /* src core id */
	uint8_t qos; /* quality of service parameter */
} ipc_core_def_t;

/* 12-byte */
typedef struct {
	ipc_hdr_fmt_t fmt;
	union {
		ipc_ep_pair_t ep_pair;
		ipc_core_def_t core_def;
	} route;
	uint8_t track_num; /* tracking number, for request-reply */
	uint8_t seq_num; /* sequence number */
	uint16_t id; /* message id */
} ipc_msg_hdr_t;

#define ECHO_STRING_SIZE 15

// The buffer size of the most recent events
#define LATENCY_EVENT_HISTORY 256

typedef struct latency_event_param {
	uint64_t event_time;
} latency_event_param_t;

typedef struct latency_event {
	latency_event_param_t event_param[LATENCY_EVENT_HISTORY];
	uint64_t current_time; //last saved timestamp
	uint32_t index; //next available indx
} latency_event_buffer_t;

latency_event_buffer_t g_latency_event[NUM_LATENCY_EVENT];

// Needs to match enum class TimeStampEntry in WifiIntralinkLatencyCli
enum {
	WifiDriverTxStart = 0,
	WifiDriverRxComplete,
	TotalEntry,
};

/* Local Functions */
static void latency_insert_current_timestamp_in_echo_packet(
	uint64_t *current_time, uint8_t *pktdata, uint8_t offset)
{
	ARTS_header_t *arts = NULL;
	ipc_msg_hdr_t *ipc_msg_hdr = NULL;
	uint8_t *echo_payload = NULL;

	if (pktdata == NULL) {
		DHD_ERROR(("%s : Invalid pktdata\n", __func__));
		return;
	}

	arts = (ARTS_header_t *)(pktdata + sizeof(struct ether_header) +
				 CONFIG_ETH_DESG_HEADROOM);
	if (arts == NULL || arts->segment_offset != 0) {
		DHD_ERROR(("%s : Wrong Segment\n", __func__));
		return;
	}

	ipc_msg_hdr = (ipc_msg_hdr_t *)((uint8_t *)arts + sizeof(ARTS_header_t));
	if (ipc_msg_hdr && (ipc_msg_hdr->id == JANUS_STD_MSG_ID_ECHO ||
			    ipc_msg_hdr->id == JANUS_STD_MSG_ID_ACK ||
			    ipc_msg_hdr->id == JANUS_STD_MSG_ID_NACK)) {
		// 12-byte IPC header and 4-byte payload size field
		echo_payload = (uint8_t *)ipc_msg_hdr + sizeof(ipc_msg_hdr_t) +
			       sizeof(uint32_t);
		memcpy(echo_payload + ECHO_STRING_SIZE +
			       offset * sizeof(uint64_t),
		       current_time, sizeof(uint64_t));
	}
}

/***************************************************************************************************
 * Public Function Definitions
 **************************************************************************************************/

void latency_event_mark(latency_event_t event, uint8_t *pktdata)
{
	latency_event_buffer_t *current_event_buffer = NULL;
	latency_event_buffer_t *dhd_start_xmit_buffer = NULL;
	latency_event_buffer_t *dhd_start_rx_buffer = NULL;
	uint64_t txlatency = 0;
	uint64_t rxlatency = 0;
	uint64_t totallatency = 0;
	uint32_t current_index = 0;
	uint64_t event_time = 0;

	if (event >= NUM_LATENCY_EVENT) {
		DHD_ERROR(("%s : Invalid Event %d\n", __FUNCTION__, event));
		return;
	}

	current_event_buffer = &g_latency_event[event];
	current_index = current_event_buffer->index;
	event_time = CURRENT_TIME_NS();
	current_event_buffer->event_param[current_index].event_time =
		event_time;
	current_event_buffer->current_time = event_time;
	if (event == DHD_START_XMIT) {
		latency_insert_current_timestamp_in_echo_packet(
			&event_time, pktdata, WifiDriverTxStart);
	} else if (event == DHD_TXCOMPLETE) {
		// txlatency is the tx processing time in wifi driver:
		// between starting xmiting echo request to tx complete.
		dhd_start_xmit_buffer = &g_latency_event[DHD_START_XMIT];
		txlatency = event_time - dhd_start_xmit_buffer->current_time;
	} else if (event == DHD_RX_FRAME_EXIT) {
		dhd_start_rx_buffer = &g_latency_event[DHD_PROT_MSGBUF_RXCMP];
		dhd_start_xmit_buffer = &g_latency_event[DHD_START_XMIT];
		// rxlatency is the rx processing time in wifi driver:
		// between receiving rx interrupt from dongle to rx pkt sending to net skb.
		rxlatency = event_time - dhd_start_rx_buffer->current_time;
		// total latnecy is the total wifi driver latency including echo request xmit, airtime, and echo response rx.
		// Between starting xmiting echo request to echo response sending to net skb.
		totallatency = event_time - dhd_start_xmit_buffer->current_time;
		latency_insert_current_timestamp_in_echo_packet(
			&event_time, pktdata, WifiDriverRxComplete);
	}

	DHD_ERROR((
		"Event %d 0x%016llx txlatency = %llu  rxlatency = %llu totallatency = %llu \n",
		event, event_time, txlatency, rxlatency, totallatency));
	current_event_buffer->index++;
	if (current_event_buffer->index == NUM_LATENCY_EVENT) {
		current_event_buffer->index = 0;
	}
}

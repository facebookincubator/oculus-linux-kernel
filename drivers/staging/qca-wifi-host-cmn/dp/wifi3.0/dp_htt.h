/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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

#ifndef _DP_HTT_H_
#define _DP_HTT_H_

#include <qdf_types.h>
#include <qdf_lock.h>
#include <qdf_nbuf.h>
#include <htc_api.h>

#include "cdp_txrx_cmn_struct.h"
#include "dp_types.h"

#define HTT_TX_MUTEX_TYPE qdf_spinlock_t

#define HTT_TX_MUTEX_INIT(_mutex)				\
	qdf_spinlock_create(_mutex)

#define HTT_TX_MUTEX_ACQUIRE(_mutex)			\
	qdf_spin_lock_bh(_mutex)

#define HTT_TX_MUTEX_RELEASE(_mutex)			\
	qdf_spin_unlock_bh(_mutex)

#define HTT_TX_MUTEX_DESTROY(_mutex)			\
	qdf_spinlock_destroy(_mutex)

#define DP_HTT_MAX_SEND_QUEUE_DEPTH 64

#ifndef HTT_MAC_ADDR_LEN
#define HTT_MAC_ADDR_LEN 6
#endif

#define HTT_FRAMECTRL_TYPE_MASK 0x0C
#define HTT_GET_FRAME_CTRL_TYPE(_val)	\
		(((_val) & HTT_FRAMECTRL_TYPE_MASK) >> 2)
#define FRAME_CTRL_TYPE_MGMT	0x0
#define FRAME_CTRL_TYPE_CTRL	0x1
#define FRAME_CTRL_TYPE_DATA	0x2
#define FRAME_CTRL_TYPE_RESV	0x3

#define HTT_FRAMECTRL_DATATYPE 0x08
#define HTT_PPDU_DESC_MAX_DEPTH 16
#define DP_SCAN_PEER_ID 0xFFFF

#define HTT_RX_DELBA_WIN_SIZE_M    0x0000FC00
#define HTT_RX_DELBA_WIN_SIZE_S    10

#define HTT_RX_DELBA_WIN_SIZE_GET(word)	\
	(((word) & HTT_RX_DELBA_WIN_SIZE_M) >> HTT_RX_DELBA_WIN_SIZE_S)

/*
 * Set the base misclist size to HTT copy engine source ring size
 * to guarantee that a packet on the misclist wont be freed while it
 * is sitting in the copy engine.
 */
#define DP_HTT_HTC_PKT_MISCLIST_SIZE          2048
#define HTT_T2H_MAX_MSG_SIZE 2048

#define HTT_T2H_EXT_STATS_TLV_START_OFFSET    3

struct dp_htt_htc_pkt {
	void *soc_ctxt;
	qdf_dma_addr_t nbuf_paddr;
	HTC_PACKET htc_pkt;
};

struct dp_htt_htc_pkt_union {
	union {
		struct dp_htt_htc_pkt pkt;
		struct dp_htt_htc_pkt_union *next;
	} u;
};

struct htt_soc {
	void *ctrl_psoc;
	void *dp_soc;
	void *hal_soc;
	HTC_HANDLE htc_soc;
	qdf_device_t osdev;
	HTC_ENDPOINT_ID htc_endpoint;
	struct dp_htt_htc_pkt_union *htt_htc_pkt_freelist;
	struct dp_htt_htc_pkt_union *htt_htc_pkt_misclist;
	struct {
		u_int8_t major;
		u_int8_t minor;
	} tgt_ver;
	struct {
		u_int8_t major;
		u_int8_t minor;
	} wifi_ip_ver;

	struct {
		int htc_err_cnt;
		int htc_pkt_free;
	} stats;

	HTT_TX_MUTEX_TYPE htt_tx_mutex;
};

/**
 * struct htt_rx_ring_tlv_filter - Rx ring TLV filter
 * enable/disable.
 * @mpdu_start: enable/disable MPDU start TLV
 * @msdu_start: enable/disable MSDU start TLV
 * @packet: enable/disable PACKET TLV
 * @msdu_end: enable/disable MSDU end TLV
 * @mpdu_end: enable/disable MPDU end TLV
 * @packet_header: enable/disable PACKET header TLV
 * @attention: enable/disable ATTENTION TLV
 * @ppdu_start: enable/disable PPDU start TLV
 * @ppdu_end: enable/disable PPDU end TLV
 * @ppdu_end_user_stats: enable/disable PPDU user stats TLV
 * @ppdu_end_user_stats_ext: enable/disable PPDU user stats ext TLV
 * @ppdu_end_status_done: enable/disable PPDU end status done TLV
 * @enable_fp: enable/disable FP packet
 * @enable_md: enable/disable MD packet
 * @enable_mo: enable/disable MO packet
 * @enable_mgmt: enable/disable MGMT packet
 * @enable_ctrl: enable/disable CTRL packet
 * @enable_data: enable/disable DATA packet
 * @offset_valid: Flag to indicate if below offsets are valid
 * @rx_packet_offset: Offset of packet payload
 * @rx_header_offset: Offset of rx_header tlv
 * @rx_mpdu_end_offset: Offset of rx_mpdu_end tlv
 * @rx_mpdu_start_offset: Offset of rx_mpdu_start tlv
 * @rx_msdu_end_offset: Offset of rx_msdu_end tlv
 * @rx_msdu_start_offset: Offset of rx_msdu_start tlv
 * @rx_attn_offset: Offset of rx_attention tlv
 */
struct htt_rx_ring_tlv_filter {
	u_int32_t mpdu_start:1,
		msdu_start:1,
		packet:1,
		msdu_end:1,
		mpdu_end:1,
		packet_header:1,
		attention:1,
		ppdu_start:1,
		ppdu_end:1,
		ppdu_end_user_stats:1,
		ppdu_end_user_stats_ext:1,
		ppdu_end_status_done:1,
		header_per_msdu:1,
		enable_fp:1,
		enable_md:1,
		enable_mo:1;
	u_int32_t fp_mgmt_filter:16,
		mo_mgmt_filter:16;
	u_int32_t fp_ctrl_filter:16,
		mo_ctrl_filter:16;
	u_int32_t fp_data_filter:16,
		mo_data_filter:16;
	u_int16_t md_data_filter;
	u_int16_t md_mgmt_filter;
	u_int16_t md_ctrl_filter;
	bool offset_valid;
	uint16_t rx_packet_offset;
	uint16_t rx_header_offset;
	uint16_t rx_mpdu_end_offset;
	uint16_t rx_mpdu_start_offset;
	uint16_t rx_msdu_end_offset;
	uint16_t rx_msdu_start_offset;
	uint16_t rx_attn_offset;
};

/*
 * htt_soc_initialize() - SOC level HTT initialization
 * @htt_soc: Opaque htt SOC handle
 * @ctrl_psoc: Opaque ctrl SOC handle
 * @htc_soc: SOC level HTC handle
 * @hal_soc: Opaque HAL SOC handle
 * @osdev: QDF device
 *
 * Return: HTT handle on success; NULL on failure
 */
void *
htt_soc_initialize(void *htt_soc, void *ctrl_psoc, HTC_HANDLE htc_soc,
		   void *hal_soc, qdf_device_t osdev);

/*
 * htt_soc_htc_dealloc() - HTC memory de-alloc
 * @htt_soc: SOC level HTT handle
 *
 * Return: None
 */
void htt_soc_htc_dealloc(struct htt_soc *htt_handle);

/*
 * htt_soc_htc_prealloc() - HTC memory prealloc
 * @htt_soc: SOC level HTT handle
 *
 * Return: QDF_STATUS_SUCCESS on success or
 * QDF_STATUS_E_NO_MEM on allocation failure
 */
QDF_STATUS htt_soc_htc_prealloc(struct htt_soc *htt_soc);

void htt_soc_detach(void *soc);

int htt_srng_setup(void *htt_soc, int pdev_id, void *hal_srng,
	int hal_ring_type);

int htt_soc_attach_target(void *htt_soc);

/*
 * htt_h2t_rx_ring_cfg() - Send SRNG packet and TLV filter
 * config message to target
 * @htt_soc:	HTT SOC handle
 * @pdev_id:	PDEV Id
 * @hal_srng:	Opaque HAL SRNG pointer
 * @hal_ring_type:	SRNG ring type
 * @ring_buf_size:	SRNG buffer size
 * @htt_tlv_filter:	Rx SRNG TLV and filter setting
 *
 * Return: 0 on success; error code on failure
 */
int htt_h2t_rx_ring_cfg(void *htt_soc, int pdev_id, void *hal_srng,
	int hal_ring_type, int ring_buf_size,
	struct htt_rx_ring_tlv_filter *htt_tlv_filter);

/*
 * htt_t2h_stats_handler() - target to host stats work handler
 * @context:	context (dp soc context)
 *
 * Return: void
 */
void htt_t2h_stats_handler(void *context);

/**
 * struct htt_stats_context - htt stats information
 * @soc: Size of each descriptor in the pool
 * @msg: T2H Ext stats message queue
 * @msg_len: T2H Ext stats message length
 */
struct htt_stats_context {
	struct dp_soc *soc;
	qdf_nbuf_queue_t msg;
	uint32_t msg_len;
};

int
dp_htt_get_ppdu_sniffer_ampdu_tlv_bitmap(uint32_t bitmap);

/**
 * dp_ppdu_desc_user_stats_update(): Function to update TX user stats
 * @pdev: DP pdev handle
 * @ppdu_info: per PPDU TLV descriptor
 *
 * return: void
 */
void
dp_ppdu_desc_user_stats_update(struct dp_pdev *pdev,
			       struct ppdu_info *ppdu_info);

#endif /* _DP_HTT_H_ */

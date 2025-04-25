/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#ifndef _DP_TX_MON_2_0_H_
#define _DP_TX_MON_2_0_H_

#include <qdf_nbuf_frag.h>
#include <hal_be_api_mon.h>

struct dp_mon_desc;

/**
 * struct dp_tx_mon_desc_list - structure to store descriptor linked list
 * @desc_list: descriptor list
 * @tail: descriptor list tail
 * @tx_mon_reap_cnt: tx monitor reap count
 */
struct dp_tx_mon_desc_list {
	union dp_mon_desc_list_elem_t *desc_list;
	union dp_mon_desc_list_elem_t *tail;
	uint32_t tx_mon_reap_cnt;
};

/**
 * dp_tx_mon_buf_desc_pool_deinit() - deinit tx monitor descriptor pool
 * @soc: dp soc handle
 *
 */
void
dp_tx_mon_buf_desc_pool_deinit(struct dp_soc *soc);

/**
 * dp_tx_mon_buf_desc_pool_init() - init tx monitor descriptor pool
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS
dp_tx_mon_buf_desc_pool_init(struct dp_soc *soc);

/**
 * dp_tx_mon_buf_desc_pool_free() - free tx monitor descriptor pool
 * @soc: dp soc handle
 *
 */
void dp_tx_mon_buf_desc_pool_free(struct dp_soc *soc);

/**
 * dp_tx_mon_update_end_reason() - API to update end reason
 *
 * @mon_pdev: DP_MON_PDEV handle
 * @ppdu_id: ppdu_id
 * @end_reason: monitor destination descriptor end reason
 *
 * Return: void
 */
void dp_tx_mon_update_end_reason(struct dp_mon_pdev *mon_pdev,
				 int ppdu_id, int end_reason);

/**
 * dp_tx_mon_status_free_packet_buf() - API to free packet buffer
 * @pdev: pdev Handle
 * @status_frag: status frag
 * @end_offset: status fragment end offset
 * @mon_desc_list_ref: tx monitor descriptor list reference
 *
 * Return: void
 */
void
dp_tx_mon_status_free_packet_buf(struct dp_pdev *pdev,
				 qdf_frag_t status_frag,
				 uint32_t end_offset,
				 struct dp_tx_mon_desc_list *mon_desc_list_ref);

#if defined(WLAN_TX_PKT_CAPTURE_ENH_BE) && defined(WLAN_PKT_CAPTURE_TX_2_0) && \
	defined(BE_PKTLOG_SUPPORT)
/**
 * dp_tx_process_pktlog_be() - process pktlog
 * @soc: dp soc handle
 * @pdev: dp pdev handle
 * @status_frag: frag pointer which needs to be added to nbuf
 * @end_offset: Offset in frag to be added to nbuf_frags
 *
 * Return:
 * * 0             - OK to runtime suspend the device
 * * -EINVAL - invalid argument
 * * -ENOMEM - out of memory
 */
QDF_STATUS
dp_tx_process_pktlog_be(struct dp_soc *soc, struct dp_pdev *pdev,
			void *status_frag, uint32_t end_offset);
#else
static inline QDF_STATUS
dp_tx_process_pktlog_be(struct dp_soc *soc, struct dp_pdev *pdev,
			void *status_frag, uint32_t end_offset)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_tx_mon_process_status_tlv() - API to processed TLV
 * invoked from interrupt handler
 *
 * @soc: DP_SOC handle
 * @pdev: DP_PDEV handle
 * @mon_ring_desc: descriptor status info
 * @status_frag: status buffer frag address
 * @end_offset: end offset of buffer that has valid buffer
 * @mon_desc_list_ref: tx monitor descriptor list reference
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_tx_mon_process_status_tlv(struct dp_soc *soc,
			     struct dp_pdev *pdev,
			     struct hal_mon_desc *mon_ring_desc,
			     qdf_frag_t status_frag,
			     uint32_t end_offset,
			     struct dp_tx_mon_desc_list *mon_desc_list_ref);

/**
 * dp_tx_mon_process_2_0() - tx monitor interrupt process
 * @soc: dp soc handle
 * @int_ctx: interrupt context
 * @mac_id: mac id
 * @quota: quota to process
 *
 */
uint32_t
dp_tx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota);

/**
 * dp_tx_mon_print_ring_stat_2_0() - Print monitor ring stats
 * @pdev: dp pdev handle
 *
 */
void
dp_tx_mon_print_ring_stat_2_0(struct dp_pdev *pdev);

/* The maximum buffer length allocated for radiotap for monitor status buffer */
#define MAX_MONITOR_HEADER (512)
#define MAX_DUMMY_FRM_BODY (128)

#define MAX_STATUS_BUFFER_IN_PPDU (128)
#define TXMON_NO_BUFFER_SZ (64)

#define DP_BA_ACK_FRAME_SIZE (sizeof(struct ieee80211_ctlframe_addr2) + 36)
#define DP_ACK_FRAME_SIZE (sizeof(struct ieee80211_frame_min_one))
#define DP_CTS_FRAME_SIZE (sizeof(struct ieee80211_frame_min_one))
#define DP_ACKNOACK_FRAME_SIZE (sizeof(struct ieee80211_frame) + 16)

#define DP_IEEE80211_BAR_CTL_TID_S 12
#define DP_IEEE80211_BAR_CTL_TID_M 0xf
#define DP_IEEE80211_BAR_CTL_POLICY_S 0
#define DP_IEEE80211_BAR_CTL_POLICY_M 0x1
#define DP_IEEE80211_BA_S_SEQ_S 4
#define DP_IEEE80211_BAR_CTL_COMBA 0x0004

#define TXMON_PPDU(ppdu_info, field)	ppdu_info->field
#define TXMON_PPDU_USR(ppdu_info, user_index, field)	\
			ppdu_info->hal_txmon.rx_user_status[user_index].field
#define TXMON_PPDU_COM(ppdu_info, field) ppdu_info->hal_txmon.rx_status.field
#define TXMON_PPDU_HAL(ppdu_info, field) ppdu_info->hal_txmon.field

#define HE_DATA_CNT	6

#define INITIATOR_WINDOW 0
#define RESPONSE_WINDOW 1

#ifdef WLAN_PKT_CAPTURE_TX_2_0
/**
 * enum bf_type -  tx monitor supported Beamformed type
 * @NO_BF:
 * @LEGACY_BF:
 * @SU_BF:
 * @MU_BF:
 */
enum bf_type {
	NO_BF = 0,
	LEGACY_BF,
	SU_BF,
	MU_BF
};

/**
 * enum dot11b_preamble_type - tx monitor supported 11b preamble type
 * @SHORT_PREAMBLE:
 * @LONG_PREAMBLE:
 */
enum dot11b_preamble_type {
	SHORT_PREAMBLE = 0,
	LONG_PREAMBLE,
};

/**
 * enum bw_type - tx monitor supported bandwidth type
 * @TXMON_BW_20_MHZ:
 * @TXMON_BW_40_MHZ:
 * @TXMON_BW_80_MHZ:
 * @TXMON_BW_160_MHZ:
 * @TXMON_BW_240_MHZ:
 * @TXMON_BW_320_MHZ:
 */
enum bw_type {
	TXMON_BW_20_MHZ = 0,
	TXMON_BW_40_MHZ,
	TXMON_BW_80_MHZ,
	TXMON_BW_160_MHZ,
	TXMON_BW_240_MHZ,
	TXMON_BW_320_MHZ
};

/**
 * enum ppdu_start_reason - tx monitor supported PPDU start reason type
 * @TXMON_FES_PROTECTION_FRAME:
 * @TXMON_FES_AFTER_PROTECTION:
 * @TXMON_FES_ONLY:
 * @TXMON_RESPONSE_FRAME:
 * @TXMON_TRIG_RESPONSE_FRAME:
 * @TXMON_DYNAMIC_PROTECTION_FES_ONLY:
 */
enum ppdu_start_reason {
	TXMON_FES_PROTECTION_FRAME,
	TXMON_FES_AFTER_PROTECTION,
	TXMON_FES_ONLY,
	TXMON_RESPONSE_FRAME,
	TXMON_TRIG_RESPONSE_FRAME,
	TXMON_DYNAMIC_PROTECTION_FES_ONLY
};

/**
 * enum guard_interval - tx monitor supported Guard interval type
 * @TXMON_GI_0_8_US:
 * @TXMON_GI_0_4_US:
 * @TXMON_GI_1_6_US:
 * @TXMON_GI_3_2_US:
 */
enum guard_interval {
	TXMON_GI_0_8_US = 0,
	TXMON_GI_0_4_US,
	TXMON_GI_1_6_US,
	TXMON_GI_3_2_US
};

/**
 * enum RU_size_start - tx monitor supported RU size start type
 * @TXMON_RU_26:
 * @TXMON_RU_52:
 * @TXMON_RU_106:
 * @TXMON_RU_242:
 * @TXMON_RU_484:
 * @TXMON_RU_996:
 * @TXMON_RU_1992:
 * @TXMON_RU_FULLBW_240:
 * @TXMON_RU_FULLBW_320:
 * @TXMON_RU_MULTI_LARGE:
 * @TXMON_RU_78:
 * @TXMON_RU_132:
 */
enum RU_size_start {
	TXMON_RU_26 = 0,
	TXMON_RU_52,
	TXMON_RU_106,
	TXMON_RU_242,
	TXMON_RU_484,
	TXMON_RU_996,
	TXMON_RU_1992,
	TXMON_RU_FULLBW_240,
	TXMON_RU_FULLBW_320,
	TXMON_RU_MULTI_LARGE,
	TXMON_RU_78,
	TXMON_RU_132
};

/**
 * enum response_type_expected - expected response type
 * @TXMON_RESP_NO_RESP:
 * @TXMON_RESP_ACK:
 * @TXMON_RESP_BA_64_BITMAP:
 * @TXMON_RESP_BA_256:
 * @TXMON_RESP_ACTIONNOACK:
 * @TXMON_RESP_ACK_BA:
 * @TXMON_RESP_CTS:
 * @TXMON_RESP_ACK_DATA:
 * @TXMON_RESP_NDP_ACK:
 * @TXMON_RESP_NDP_MODIFIED_ACK:
 * @TXMON_RESP_NDP_BA:
 * @TXMON_RESP_NDP_CTS:
 * @TXMON_RESP_NDP_ACK_OR_NDP_MODIFIED_ACK:
 * @TXMON_RESP_UL_MU_BA:
 * @TXMON_RESP_UL_MU_BA_AND_DATA:
 * @TXMON_RESP_UL_MU_CBF:
 * @TXMON_RESP_UL_MU_FRAMES:
 * @TXMON_RESP_ANY_RESP_TO_DEVICE:
 * @TXMON_RESP_ANY_RESP_ACCEPTED:
 * @TXMON_RESP_FRAMELESS_PHYRX_RESP_ACCEPTED:
 * @TXMON_RESP_RANGING_NDP_AND_LMR:
 * @TXMON_RESP_BA_512:
 * @TXMON_RESP_BA_1024:
 * @TXMON_RESP_UL_MU_RANGING_CTS2S:
 * @TXMON_RESP_UL_MU_RANGING_NDP:
 * @TXMON_RESP_UL_MU_RANGING_LMR:
 */
enum response_type_expected {
	TXMON_RESP_NO_RESP = 0,
	TXMON_RESP_ACK,
	TXMON_RESP_BA_64_BITMAP,
	TXMON_RESP_BA_256,
	TXMON_RESP_ACTIONNOACK,
	TXMON_RESP_ACK_BA,
	TXMON_RESP_CTS,
	TXMON_RESP_ACK_DATA,
	TXMON_RESP_NDP_ACK,
	TXMON_RESP_NDP_MODIFIED_ACK,
	TXMON_RESP_NDP_BA,
	TXMON_RESP_NDP_CTS,
	TXMON_RESP_NDP_ACK_OR_NDP_MODIFIED_ACK,
	TXMON_RESP_UL_MU_BA,
	TXMON_RESP_UL_MU_BA_AND_DATA,
	TXMON_RESP_UL_MU_CBF,
	TXMON_RESP_UL_MU_FRAMES,
	TXMON_RESP_ANY_RESP_TO_DEVICE,
	TXMON_RESP_ANY_RESP_ACCEPTED,
	TXMON_RESP_FRAMELESS_PHYRX_RESP_ACCEPTED,
	TXMON_RESP_RANGING_NDP_AND_LMR,
	TXMON_RESP_BA_512,
	TXMON_RESP_BA_1024,
	TXMON_RESP_UL_MU_RANGING_CTS2S,
	TXMON_RESP_UL_MU_RANGING_NDP,
	TXMON_RESP_UL_MU_RANGING_LMR
};

/**
 * enum resposne_to_respone - tx monitor supported response to response type
 * @TXMON_RESP_TO_RESP_NONE:
 * @TXMON_RESP_TO_RESP_SU_BA:
 * @TXMON_RESP_TO_RESP_MU_BA:
 * @TXMON_RESP_TO_RESP_CMD:
 */
enum resposne_to_respone {
	TXMON_RESP_TO_RESP_NONE = 0,
	TXMON_RESP_TO_RESP_SU_BA,
	TXMON_RESP_TO_RESP_MU_BA,
	TXMON_RESP_TO_RESP_CMD
};

/**
 * enum medium_protection_type - tx monitor supported protection type
 * @TXMON_MEDIUM_NO_PROTECTION:
 * @TXMON_MEDIUM_RTS_LEGACY:
 * @TXMON_MEDIUM_RTS_11AC_STATIC_BW:
 * @TXMON_MEDIUM_RTS_11AC_DYNAMIC_BW:
 * @TXMON_MEDIUM_CTS2SELF:
 * @TXMON_MEDIUM_QOS_NULL_NO_ACK_3ADDR:
 * @TXMON_MEDIUM_QOS_NULL_NO_ACK_4ADDR:
 */
enum medium_protection_type {
	TXMON_MEDIUM_NO_PROTECTION,
	TXMON_MEDIUM_RTS_LEGACY,
	TXMON_MEDIUM_RTS_11AC_STATIC_BW,
	TXMON_MEDIUM_RTS_11AC_DYNAMIC_BW,
	TXMON_MEDIUM_CTS2SELF,
	TXMON_MEDIUM_QOS_NULL_NO_ACK_3ADDR,
	TXMON_MEDIUM_QOS_NULL_NO_ACK_4ADDR,
};

/**
 * enum ndp_frame - tx monitor supported ndp frame type
 * @TXMON_NO_NDP_TRANSMISSION:
 * @TXMON_BEAMFORMING_NDP:
 * @TXMON_HE_RANGING_NDP:
 * @TXMON_HE_FEEDBACK_NDP:
 */
enum ndp_frame {
	TXMON_NO_NDP_TRANSMISSION,
	TXMON_BEAMFORMING_NDP,
	TXMON_HE_RANGING_NDP,
	TXMON_HE_FEEDBACK_NDP,
};

/**
 * enum tx_ppdu_info_type - tx monitor supported ppdu type
 * @TX_PROT_PPDU_INFO:
 * @TX_DATA_PPDU_INFO:
 */
enum tx_ppdu_info_type {
	TX_PROT_PPDU_INFO,
	TX_DATA_PPDU_INFO,
};

/**
 * struct dp_tx_ppdu_info - structure to store tx ppdu info
 * @ppdu_id: current ppdu info ppdu id
 * @frame_type: ppdu info frame type
 * @cur_usr_idx: current user index of ppdu info
 * @ulist: union of linked lists
 * @tx_ppdu_info_dlist_elem: support adding to double linked list
 * @tx_ppdu_info_slist_elem: support adding to single linked list
 * @hal_txmon: hal tx monitor info for that ppdu
 */
struct dp_tx_ppdu_info {
	uint32_t ppdu_id;
	uint8_t frame_type;
	uint8_t cur_usr_idx;

	union {
		TAILQ_ENTRY(dp_tx_ppdu_info) tx_ppdu_info_dlist_elem;
		STAILQ_ENTRY(dp_tx_ppdu_info) tx_ppdu_info_slist_elem;
	} ulist;

	#define tx_ppdu_info_list_elem ulist.tx_ppdu_info_dlist_elem
	#define tx_ppdu_info_queue_elem ulist.tx_ppdu_info_slist_elem

	struct hal_tx_ppdu_info hal_txmon;
};

/**
 * struct dp_tx_monitor_drop_stats - structure to store tx monitor drop
 *                                   statistics
 * @ppdu_drop_cnt: ppdu drop counter
 * @mpdu_drop_cnt: mpdu drop counter
 * @tlv_drop_cnt: tlv drop counter
 * @pkt_buf_recv: tx monitor packet buffer received
 * @pkt_buf_free: tx monitor packet buffer free
 * @pkt_buf_processed: tx monitor packet buffer processed
 * @pkt_buf_to_stack: tx monitor packet buffer send to stack
 * @status_buf_recv: tx monitor status buffer received
 * @status_buf_free: tx monitor status buffer free
 * @totat_tx_mon_replenish_cnt: tx monitor replenish count
 * @total_tx_mon_reap_cnt: tx monitor reap count
 * @tx_mon_stuck: tx monitor stuck count
 * @total_tx_mon_stuck: tx monitor stuck count
 * @ppdu_info_drop_th: count ppdu info been dropped due threshold reached
 * @ppdu_info_drop_flush: count ppdu info been dropped due to flush detected
 * @ppdu_info_drop_trunc: count ppdu info been dropped due to truncated
 */
struct dp_tx_monitor_drop_stats {
	uint64_t ppdu_drop_cnt;
	uint64_t mpdu_drop_cnt;
	uint64_t tlv_drop_cnt;

	uint64_t pkt_buf_recv;
	uint64_t pkt_buf_free;
	uint64_t pkt_buf_processed;
	uint64_t pkt_buf_to_stack;

	uint64_t status_buf_recv;
	uint64_t status_buf_free;

	uint64_t totat_tx_mon_replenish_cnt;
	uint64_t total_tx_mon_reap_cnt;
	uint8_t tx_mon_stuck;
	uint32_t total_tx_mon_stuck;

	uint64_t ppdu_info_drop_th;
	uint64_t ppdu_info_drop_flush;
	uint64_t ppdu_info_drop_trunc;
};

/**
 * enum dp_tx_monitor_mode - tx monitor supported mode
 * @TX_MON_BE_DISABLE: tx monitor disable
 * @TX_MON_BE_FULL_CAPTURE: tx monitor mode to capture full packet
 * @TX_MON_BE_PEER_FILTER: tx monitor mode to capture peer filter
 */
enum dp_tx_monitor_mode {
	TX_MON_BE_DISABLE,
	TX_MON_BE_FULL_CAPTURE,
	TX_MON_BE_PEER_FILTER,
};

/**
 * enum dp_tx_monitor_framework_mode - tx monitor framework mode
 * @TX_MON_BE_FRM_WRK_DISABLE: tx monitor frame work disable
 * @TX_MON_BE_FRM_WRK_FULL_CAPTURE: tx monitor frame work full capture
 * @TX_MON_BE_FRM_WRK_128B_CAPTURE: tx monitor frame work 128B capture
 */
enum dp_tx_monitor_framework_mode {
	TX_MON_BE_FRM_WRK_DISABLE,
	TX_MON_BE_FRM_WRK_FULL_CAPTURE,
	TX_MON_BE_FRM_WRK_128B_CAPTURE,
};

#define TX_TAILQ_INSERT_TAIL(pdev, tx_ppdu_info)			\
	do {								\
		STAILQ_INSERT_TAIL(&pdev->tx_ppdu_info_list,		\
				   tx_ppdu_info, tx_ppdu_info_list_elem);\
		pdev->tx_ppdu_info_queue_depth++;			\
	} while (0)

#define TX_TAILQ_REMOVE(pdev, tx_ppdu_info)				\
	do {								\
		TAILQ_REMOVE(&pdev->tx_ppdu_info_list, tx_ppdu_info,	\
			     tx_ppdu_info_list_elem);			\
		pdev->tx_ppdu_info_queue_depth--;			\
	} while (0)

#define TX_TAILQ_FIRST(pdev) TAILQ_FIRST(&pdev->tx_ppdu_info_list)

#define TX_TAILQ_FOREACH_SAFE(pdev, tx_ppdu_info)			\
	do {								\
		struct dp_tx_ppdu_info *tx_ppdu_info_next = NULL;	\
		TAILQ_FOREACH_SAFE(tx_ppdu_info,			\
				   &pdev->tx_ppdu_info_list,		\
				   tx_ppdu_info_list_elem,		\
				   tx_ppdu_info_next);			\
	} while (0)

#ifdef WLAN_TX_MON_CORE_DEBUG
/**
 * struct dp_pdev_tx_monitor_be - info to store tx capture information in pdev
 * @be_ppdu_id: current ppdu id
 * @mode: tx monitor core framework current mode
 * @stats: tx monitor drop stats for that mac
 *
 */
struct dp_pdev_tx_monitor_be {
	uint32_t be_ppdu_id;
	uint32_t mode;
	struct dp_tx_monitor_drop_stats stats;
};

/**
 * struct dp_peer_tx_capture_be - Tx monitor peer structure
 *
 * This is a dummy structure
 */
struct dp_peer_tx_capture_be {
};
#endif

#ifdef WLAN_TX_PKT_CAPTURE_ENH_BE
/**
 * struct dp_txmon_frag_vec - a contiguous range of physical memory address
 * @frag_buf: frag buffer address
 * @end_offset: byte offset within the frag buffer where valid data resides
 */
struct dp_txmon_frag_vec {
	qdf_frag_t frag_buf;
	uint32_t end_offset;
};

/*
 * NB: intentionally not using kernel-doc comment because the kernel-doc
 *     script does not handle the STAILQ_HEAD macro
 * struct dp_pdev_tx_monitor_be - info to store tx capture information in pdev
 * @be_ppdu_id: current ppdu id
 * @be_end_reason_bitmap: current end reason bitmap
 * @mode: tx monitor current mode
 * @tx_mon_list_lock: spinlock protection to list
 * @post_ppdu_workqueue: tx monitor workqueue representation
 * @post_ppdu_work: tx monitor post ppdu work
 * @tx_ppdu_info_list_depth: list depth counter
 * @tx_ppdu_info_list: ppdu info list to hold ppdu
 * @defer_ppdu_info_list_depth: defer ppdu list depth counter
 * @defer_ppdu_info_list: defer ppdu info list to hold defer ppdu
 * @stats: tx monitor drop stats for that mac
 * @tx_prot_ppdu_info: tx monitor protection ppdu info
 * @tx_data_ppdu_info: tx monitor data ppdu info
 * @last_prot_ppdu_info: last tx monitor protection ppdu info
 * @last_data_ppdu_info: last tx monitor data ppdu info
 * @prot_status_info: protection status info
 * @data_status_info: data status info
 * @last_tsft: last received tsft
 * @last_ppdu_timestamp: last received ppdu_timestamp
 * @last_frag_q_idx: last index of frag buffer
 * @cur_frag_q_idx: current index of frag buffer
 * @status_frag_queue: array of status frag queue to hold 64 status buffer
 */
struct dp_pdev_tx_monitor_be {
	uint32_t be_ppdu_id;
	uint32_t be_end_reason_bitmap;
	uint32_t mode;

	qdf_spinlock_t tx_mon_list_lock;

	qdf_work_t post_ppdu_work;
	qdf_workqueue_t *post_ppdu_workqueue;

	uint32_t tx_ppdu_info_list_depth;

	STAILQ_HEAD(, dp_tx_ppdu_info) tx_ppdu_info_queue;

	uint32_t defer_ppdu_info_list_depth;

	STAILQ_HEAD(, dp_tx_ppdu_info) defer_tx_ppdu_info_queue;

	struct dp_tx_monitor_drop_stats stats;

	struct dp_tx_ppdu_info *tx_prot_ppdu_info;
	struct dp_tx_ppdu_info *tx_data_ppdu_info;

	struct dp_tx_ppdu_info *last_prot_ppdu_info;
	struct dp_tx_ppdu_info *last_data_ppdu_info;

	struct hal_tx_status_info prot_status_info;
	struct hal_tx_status_info data_status_info;

	uint64_t last_tsft;
	uint32_t last_ppdu_timestamp;

	uint8_t last_frag_q_idx;
	uint8_t cur_frag_q_idx;
	struct dp_txmon_frag_vec frag_q_vec[MAX_STATUS_BUFFER_IN_PPDU];
};

/**
 * struct dp_peer_tx_capture_be - Tx monitor peer structure
 *
 * need to be added here
 */
struct dp_peer_tx_capture_be {
};

/**
 * dp_tx_mon_free_usr_mpduq() - API to free user mpduq
 * @tx_ppdu_info: pointer to tx_ppdu_info
 * @usr_idx: user index
 * @tx_mon_be: pointer to tx monitor be
 *
 * Return: void
 */
void dp_tx_mon_free_usr_mpduq(struct dp_tx_ppdu_info *tx_ppdu_info,
			      uint8_t usr_idx,
			      struct dp_pdev_tx_monitor_be *tx_mon_be);

#endif /* WLAN_TX_PKT_CAPTURE_ENH_BE */

/**
 * dp_tx_mon_ppdu_info_free() - API to free dp_tx_ppdu_info
 * @tx_ppdu_info: pointer to tx_ppdu_info
 *
 * Return: void
 */
void dp_tx_mon_ppdu_info_free(struct dp_tx_ppdu_info *tx_ppdu_info);

/**
 * dp_tx_mon_free_ppdu_info() - API to free dp_tx_ppdu_info
 * @tx_ppdu_info: pointer to tx_ppdu_info
 * @tx_mon_be: pointer to tx monitor be
 *
 * Return: void
 */
void dp_tx_mon_free_ppdu_info(struct dp_tx_ppdu_info *tx_ppdu_info,
			      struct dp_pdev_tx_monitor_be *tx_mon_be);

/**
 * dp_tx_mon_get_ppdu_info() - API to allocate dp_tx_ppdu_info
 * @pdev: pdev handle
 * @type: type of ppdu_info data or protection
 * @num_user: number user in a ppdu_info
 * @ppdu_id: ppdu_id number
 *
 * Return: pointer to dp_tx_ppdu_info
 */
struct dp_tx_ppdu_info *dp_tx_mon_get_ppdu_info(struct dp_pdev *pdev,
						enum tx_ppdu_info_type type,
						uint8_t num_user,
						uint32_t ppdu_id);

#endif /* WLAN_PKT_CAPTURE_TX_2_0 */

#if (defined(WIFI_MONITOR_SUPPORT) && defined(WLAN_TX_PKT_CAPTURE_ENH_BE))
/**
 * dp_config_enh_tx_monitor_2_0()- API to validate tx monitor feature
 * @pdev: DP_PDEV handle
 * @val: user provided value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_config_enh_tx_monitor_2_0(struct dp_pdev *pdev, uint8_t val);
#endif

#ifdef WLAN_TX_MON_CORE_DEBUG
/**
 * dp_config_enh_tx_core_monitor_2_0()- API to validate core framework
 * @pdev: DP_PDEV handle
 * @val: user provided value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_config_enh_tx_core_monitor_2_0(struct dp_pdev *pdev, uint8_t val);
#endif

#ifdef WLAN_PKT_CAPTURE_TX_2_0
QDF_STATUS dp_tx_mon_pdev_htt_srng_setup_2_0(struct dp_soc *soc,
					     struct dp_pdev *pdev,
					     int mac_id,
					     int mac_for_pdev);
QDF_STATUS dp_tx_mon_soc_htt_srng_setup_2_0(struct dp_soc *soc,
					    int mac_id);
QDF_STATUS dp_tx_mon_pdev_rings_alloc_2_0(struct dp_pdev *pdev, uint32_t lmac_id);
void dp_tx_mon_pdev_rings_free_2_0(struct dp_pdev *pdev, uint32_t lmac_id);
QDF_STATUS dp_tx_mon_pdev_rings_init_2_0(struct dp_pdev *pdev, uint32_t lmac_id);
void dp_tx_mon_pdev_rings_deinit_2_0(struct dp_pdev *pdev, uint32_t lmac_id);
QDF_STATUS dp_tx_mon_soc_init_2_0(struct dp_soc *soc);
QDF_STATUS dp_tx_mon_soc_attach_2_0(struct dp_soc *soc, uint32_t lmac_id);
QDF_STATUS dp_tx_mon_soc_detach_2_0(struct dp_soc *soc, uint32_t lmac_id);
void dp_tx_mon_soc_deinit_2_0(struct dp_soc *soc, uint32_t lmac_id);
void dp_print_pdev_tx_monitor_stats_2_0(struct dp_pdev *pdev);
QDF_STATUS
dp_tx_mon_buffers_alloc(struct dp_soc *soc, uint32_t size);
void
dp_tx_mon_buffers_free(struct dp_soc *soc);
QDF_STATUS
dp_tx_mon_buf_desc_pool_alloc(struct dp_soc *soc);
void dp_tx_ppdu_stats_attach_2_0(struct dp_pdev *pdev);
QDF_STATUS dp_config_enh_tx_monitor_2_0(struct dp_pdev *pdev, uint8_t val);
QDF_STATUS dp_peer_set_tx_capture_enabled_2_0(struct dp_pdev *pdev_handle,
					      struct dp_peer *peer_handle,
					      uint8_t is_tx_pkt_cap_enable,
					      uint8_t *peer_mac);
void dp_tx_ppdu_stats_detach_2_0(struct dp_pdev *pdev);
#else
static inline
QDF_STATUS dp_tx_mon_pdev_htt_srng_setup_2_0(struct dp_soc *soc,
					     struct dp_pdev *pdev,
					     int mac_id,
					     int mac_for_pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_tx_mon_soc_htt_srng_setup_2_0(struct dp_soc *soc,
					    int mac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_tx_mon_pdev_rings_alloc_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_tx_mon_pdev_rings_free_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
}

static inline
QDF_STATUS dp_tx_mon_pdev_rings_init_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_tx_mon_pdev_rings_deinit_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
}

static inline
QDF_STATUS dp_tx_mon_soc_init_2_0(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_tx_mon_soc_attach_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_tx_mon_soc_detach_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_tx_mon_soc_deinit_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
}

static inline void
dp_print_pdev_tx_monitor_stats_2_0(struct dp_pdev *pdev)
{
}

static inline QDF_STATUS
dp_tx_mon_buffers_alloc(struct dp_soc *soc, uint32_t size)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_tx_mon_buffers_free(struct dp_soc *soc)
{
}

static inline QDF_STATUS
dp_tx_mon_buf_desc_pool_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_tx_ppdu_stats_attach_2_0(struct dp_pdev *pdev)
{
}

static inline QDF_STATUS
dp_config_enh_tx_monitor_2_0(struct dp_pdev *pdev, uint8_t val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_peer_set_tx_capture_enabled_2_0(struct dp_pdev *pdev_handle,
				   struct dp_peer *peer_handle,
				   uint8_t is_tx_pkt_cap_enable,
				   uint8_t *peer_mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_tx_ppdu_stats_detach_2_0(struct dp_pdev *pdev)
{
}
#endif

#endif /* _DP_TX_MON_2_0_H_ */

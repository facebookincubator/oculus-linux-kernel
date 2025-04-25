/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <qdf_types.h>
#include <qdf_lock.h>
#include <hal_hw_headers.h>
#include "dp_htt.h"
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_peer.h"
#include "dp_rx_defrag.h"
#include "dp_rx.h"
#include <hal_api.h>
#include <hal_reo.h>
#include <cdp_txrx_handle.h>
#include <wlan_cfg.h>
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#include <qdf_module.h>
#ifdef QCA_PEER_EXT_STATS
#include "dp_hist.h"
#endif
#ifdef BYPASS_OL_OPS
#include <target_if_dp.h>
#endif

#ifdef REO_QDESC_HISTORY
#define REO_QDESC_HISTORY_SIZE 512
uint64_t reo_qdesc_history_idx;
struct reo_qdesc_event reo_qdesc_history[REO_QDESC_HISTORY_SIZE];
#endif

#ifdef REO_QDESC_HISTORY
static inline void
dp_rx_reo_qdesc_history_add(struct reo_desc_list_node *free_desc,
			    enum reo_qdesc_event_type type)
{
	struct reo_qdesc_event *evt;
	struct dp_rx_tid *rx_tid = &free_desc->rx_tid;
	uint32_t idx;

	reo_qdesc_history_idx++;
	idx = (reo_qdesc_history_idx & (REO_QDESC_HISTORY_SIZE - 1));

	evt = &reo_qdesc_history[idx];

	qdf_mem_copy(evt->peer_mac, free_desc->peer_mac, QDF_MAC_ADDR_SIZE);
	evt->qdesc_addr = rx_tid->hw_qdesc_paddr;
	evt->ts = qdf_get_log_timestamp();
	evt->type = type;
}

#ifdef WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
static inline void
dp_rx_reo_qdesc_deferred_evt_add(struct reo_desc_deferred_freelist_node *desc,
				 enum reo_qdesc_event_type type)
{
	struct reo_qdesc_event *evt;
	uint32_t idx;

	reo_qdesc_history_idx++;
	idx = (reo_qdesc_history_idx & (REO_QDESC_HISTORY_SIZE - 1));

	evt = &reo_qdesc_history[idx];

	qdf_mem_copy(evt->peer_mac, desc->peer_mac, QDF_MAC_ADDR_SIZE);
	evt->qdesc_addr = desc->hw_qdesc_paddr;
	evt->ts = qdf_get_log_timestamp();
	evt->type = type;
}

#define DP_RX_REO_QDESC_DEFERRED_FREE_EVT(desc) \
	dp_rx_reo_qdesc_deferred_evt_add((desc), REO_QDESC_FREE)

#define DP_RX_REO_QDESC_DEFERRED_GET_MAC(desc, freedesc) \
	qdf_mem_copy((desc)->peer_mac, (freedesc)->peer_mac, QDF_MAC_ADDR_SIZE)
#endif /* WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY */

#define DP_RX_REO_QDESC_GET_MAC(freedesc, peer) \
	qdf_mem_copy((freedesc)->peer_mac, (peer)->mac_addr.raw, QDF_MAC_ADDR_SIZE)

#define DP_RX_REO_QDESC_UPDATE_EVT(free_desc) \
	dp_rx_reo_qdesc_history_add((free_desc), REO_QDESC_UPDATE_CB)

#define DP_RX_REO_QDESC_FREE_EVT(free_desc) \
	dp_rx_reo_qdesc_history_add((free_desc), REO_QDESC_FREE)

#else
#define DP_RX_REO_QDESC_GET_MAC(freedesc, peer)

#define DP_RX_REO_QDESC_UPDATE_EVT(free_desc)

#define DP_RX_REO_QDESC_FREE_EVT(free_desc)

#define DP_RX_REO_QDESC_DEFERRED_FREE_EVT(desc)

#define DP_RX_REO_QDESC_DEFERRED_GET_MAC(desc, freedesc)
#endif

static inline void
dp_set_ssn_valid_flag(struct hal_reo_cmd_params *params,
		      uint8_t valid)
{
	params->u.upd_queue_params.update_svld = 1;
	params->u.upd_queue_params.svld = valid;
	dp_peer_debug("Setting SSN valid bit to %d",
		      valid);
}

#ifdef IPA_OFFLOAD
void dp_peer_update_tid_stats_from_reo(struct dp_soc *soc, void *cb_ctxt,
				       union hal_reo_status *reo_status)
{
	struct dp_peer *peer = NULL;
	struct dp_rx_tid *rx_tid = NULL;
	unsigned long comb_peer_id_tid;
	struct hal_reo_queue_status *queue_status = &reo_status->queue_status;
	uint16_t tid;
	uint16_t peer_id;

	if (queue_status->header.status != HAL_REO_CMD_SUCCESS) {
		dp_err("REO stats failure %d",
		       queue_status->header.status);
		return;
	}
	comb_peer_id_tid = (unsigned long)cb_ctxt;
	tid = DP_PEER_GET_REO_STATS_TID(comb_peer_id_tid);
	peer_id = DP_PEER_GET_REO_STATS_PEER_ID(comb_peer_id_tid);
	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_GENERIC_STATS);
	if (!peer)
		return;
	rx_tid  = &peer->rx_tid[tid];

	if (!rx_tid) {
		dp_peer_unref_delete(peer, DP_MOD_ID_GENERIC_STATS);
		return;
	}

	rx_tid->rx_msdu_cnt.bytes += queue_status->total_cnt;
	rx_tid->rx_msdu_cnt.num += queue_status->msdu_frms_cnt;
	dp_peer_unref_delete(peer, DP_MOD_ID_GENERIC_STATS);
}

qdf_export_symbol(dp_peer_update_tid_stats_from_reo);
#endif

void dp_rx_tid_stats_cb(struct dp_soc *soc, void *cb_ctxt,
			union hal_reo_status *reo_status)
{
	struct dp_rx_tid *rx_tid = (struct dp_rx_tid *)cb_ctxt;
	struct hal_reo_queue_status *queue_status = &reo_status->queue_status;

	if (queue_status->header.status == HAL_REO_CMD_DRAIN)
		return;

	if (queue_status->header.status != HAL_REO_CMD_SUCCESS) {
		DP_PRINT_STATS("REO stats failure %d for TID %d",
			       queue_status->header.status, rx_tid->tid);
		return;
	}

	DP_PRINT_STATS("REO queue stats (TID: %d):\n"
		       "ssn: %d\n"
		       "curr_idx  : %d\n"
		       "pn_31_0   : %08x\n"
		       "pn_63_32  : %08x\n"
		       "pn_95_64  : %08x\n"
		       "pn_127_96 : %08x\n"
		       "last_rx_enq_tstamp : %08x\n"
		       "last_rx_deq_tstamp : %08x\n"
		       "rx_bitmap_31_0     : %08x\n"
		       "rx_bitmap_63_32    : %08x\n"
		       "rx_bitmap_95_64    : %08x\n"
		       "rx_bitmap_127_96   : %08x\n"
		       "rx_bitmap_159_128  : %08x\n"
		       "rx_bitmap_191_160  : %08x\n"
		       "rx_bitmap_223_192  : %08x\n"
		       "rx_bitmap_255_224  : %08x\n",
		       rx_tid->tid,
		       queue_status->ssn, queue_status->curr_idx,
		       queue_status->pn_31_0, queue_status->pn_63_32,
		       queue_status->pn_95_64, queue_status->pn_127_96,
		       queue_status->last_rx_enq_tstamp,
		       queue_status->last_rx_deq_tstamp,
		       queue_status->rx_bitmap_31_0,
		       queue_status->rx_bitmap_63_32,
		       queue_status->rx_bitmap_95_64,
		       queue_status->rx_bitmap_127_96,
		       queue_status->rx_bitmap_159_128,
		       queue_status->rx_bitmap_191_160,
		       queue_status->rx_bitmap_223_192,
		       queue_status->rx_bitmap_255_224);

	DP_PRINT_STATS(
		       "curr_mpdu_cnt      : %d\n"
		       "curr_msdu_cnt      : %d\n"
		       "fwd_timeout_cnt    : %d\n"
		       "fwd_bar_cnt        : %d\n"
		       "dup_cnt            : %d\n"
		       "frms_in_order_cnt  : %d\n"
		       "bar_rcvd_cnt       : %d\n"
		       "mpdu_frms_cnt      : %d\n"
		       "msdu_frms_cnt      : %d\n"
		       "total_byte_cnt     : %d\n"
		       "late_recv_mpdu_cnt : %d\n"
		       "win_jump_2k        : %d\n"
		       "hole_cnt           : %d\n",
		       queue_status->curr_mpdu_cnt,
		       queue_status->curr_msdu_cnt,
		       queue_status->fwd_timeout_cnt,
		       queue_status->fwd_bar_cnt,
		       queue_status->dup_cnt,
		       queue_status->frms_in_order_cnt,
		       queue_status->bar_rcvd_cnt,
		       queue_status->mpdu_frms_cnt,
		       queue_status->msdu_frms_cnt,
		       queue_status->total_cnt,
		       queue_status->late_recv_mpdu_cnt,
		       queue_status->win_jump_2k,
		       queue_status->hole_cnt);

	DP_PRINT_STATS("Addba Req          : %d\n"
			"Addba Resp         : %d\n"
			"Addba Resp success : %d\n"
			"Addba Resp failed  : %d\n"
			"Delba Req received : %d\n"
			"Delba Tx success   : %d\n"
			"Delba Tx Fail      : %d\n"
			"BA window size     : %d\n"
			"Pn size            : %d\n",
			rx_tid->num_of_addba_req,
			rx_tid->num_of_addba_resp,
			rx_tid->num_addba_rsp_success,
			rx_tid->num_addba_rsp_failed,
			rx_tid->num_of_delba_req,
			rx_tid->delba_tx_success_cnt,
			rx_tid->delba_tx_fail_cnt,
			rx_tid->ba_win_size,
			rx_tid->pn_size);
}

static void dp_rx_tid_update_cb(struct dp_soc *soc, void *cb_ctxt,
				union hal_reo_status *reo_status)
{
	struct dp_rx_tid *rx_tid = (struct dp_rx_tid *)cb_ctxt;

	if ((reo_status->rx_queue_status.header.status !=
		HAL_REO_CMD_SUCCESS) &&
		(reo_status->rx_queue_status.header.status !=
		HAL_REO_CMD_DRAIN)) {
		/* Should not happen normally. Just print error for now */
		dp_peer_err("%pK: Rx tid HW desc update failed(%d): tid %d",
			    soc, reo_status->rx_queue_status.header.status,
			    rx_tid->tid);
	}
}

bool dp_get_peer_vdev_roaming_in_progress(struct dp_peer *peer)
{
	struct ol_if_ops *ol_ops = NULL;
	bool is_roaming = false;
	uint8_t vdev_id = -1;
	struct cdp_soc_t *soc;

	if (!peer) {
		dp_peer_info("Peer is NULL. No roaming possible");
		return false;
	}

	soc = dp_soc_to_cdp_soc_t(peer->vdev->pdev->soc);
	ol_ops = peer->vdev->pdev->soc->cdp_soc.ol_ops;

	if (ol_ops && ol_ops->is_roam_inprogress) {
		dp_get_vdevid(soc, peer->mac_addr.raw, &vdev_id);
		is_roaming = ol_ops->is_roam_inprogress(vdev_id);
	}

	dp_peer_info("peer: " QDF_MAC_ADDR_FMT ", vdev_id: %d, is_roaming: %d",
		     QDF_MAC_ADDR_REF(peer->mac_addr.raw), vdev_id, is_roaming);

	return is_roaming;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_rx_tid_setup_allow() - check if rx_tid and reo queue desc
 *			     setup is necessary
 * @peer: DP peer handle
 *
 * Return: true - allow, false - disallow
 */
static inline
bool dp_rx_tid_setup_allow(struct dp_peer *peer)
{
	if (IS_MLO_DP_LINK_PEER(peer) && !peer->first_link)
		return false;

	return true;
}

/**
 * dp_rx_tid_update_allow() - check if rx_tid update needed
 * @peer: DP peer handle
 *
 * Return: true - allow, false - disallow
 */
static inline
bool dp_rx_tid_update_allow(struct dp_peer *peer)
{
	/* not as expected for MLO connection link peer */
	if (IS_MLO_DP_LINK_PEER(peer)) {
		QDF_BUG(0);
		return false;
	}

	return true;
}
#else
static inline
bool dp_rx_tid_setup_allow(struct dp_peer *peer)
{
	return true;
}

static inline
bool dp_rx_tid_update_allow(struct dp_peer *peer)
{
	return true;
}
#endif

QDF_STATUS
dp_rx_tid_update_wifi3(struct dp_peer *peer, int tid, uint32_t ba_window_size,
		       uint32_t start_seq, bool bar_update)
{
	struct dp_rx_tid *rx_tid = &peer->rx_tid[tid];
	struct dp_soc *soc = peer->vdev->pdev->soc;
	struct hal_reo_cmd_params params;

	if (!dp_rx_tid_update_allow(peer)) {
		dp_peer_err("skip tid update for peer:" QDF_MAC_ADDR_FMT,
			    QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&params, sizeof(params));

	params.std.need_status = 1;
	params.std.addr_lo = rx_tid->hw_qdesc_paddr & 0xffffffff;
	params.std.addr_hi = (uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
	params.u.upd_queue_params.update_ba_window_size = 1;
	params.u.upd_queue_params.ba_window_size = ba_window_size;

	if (start_seq < IEEE80211_SEQ_MAX) {
		params.u.upd_queue_params.update_ssn = 1;
		params.u.upd_queue_params.ssn = start_seq;
	} else {
	    dp_set_ssn_valid_flag(&params, 0);
	}

	if (dp_reo_send_cmd(soc, CMD_UPDATE_RX_REO_QUEUE, &params,
			    dp_rx_tid_update_cb, rx_tid)) {
		dp_err_log("failed to send reo cmd CMD_UPDATE_RX_REO_QUEUE");
		DP_STATS_INC(soc, rx.err.reo_cmd_send_fail, 1);
	}

	rx_tid->ba_win_size = ba_window_size;

	if (dp_get_peer_vdev_roaming_in_progress(peer))
		return QDF_STATUS_E_PERM;

	if (!bar_update)
		dp_peer_rx_reorder_queue_setup(soc, peer,
					       BIT(tid), ba_window_size);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
/**
 * dp_reo_desc_defer_free_enqueue() - enqueue REO QDESC to be freed into
 *                                    the deferred list
 * @soc: Datapath soc handle
 * @freedesc: REO DESC reference that needs to be freed
 *
 * Return: true if enqueued, else false
 */
static bool dp_reo_desc_defer_free_enqueue(struct dp_soc *soc,
					   struct reo_desc_list_node *freedesc)
{
	struct reo_desc_deferred_freelist_node *desc;

	if (!qdf_atomic_read(&soc->cmn_init_done))
		return false;

	desc = qdf_mem_malloc(sizeof(*desc));
	if (!desc)
		return false;

	desc->hw_qdesc_paddr = freedesc->rx_tid.hw_qdesc_paddr;
	desc->hw_qdesc_alloc_size = freedesc->rx_tid.hw_qdesc_alloc_size;
	desc->hw_qdesc_vaddr_unaligned =
			freedesc->rx_tid.hw_qdesc_vaddr_unaligned;
	desc->free_ts = qdf_get_system_timestamp();
	DP_RX_REO_QDESC_DEFERRED_GET_MAC(desc, freedesc);

	qdf_spin_lock_bh(&soc->reo_desc_deferred_freelist_lock);
	if (!soc->reo_desc_deferred_freelist_init) {
		qdf_mem_free(desc);
		qdf_spin_unlock_bh(&soc->reo_desc_deferred_freelist_lock);
		return false;
	}
	qdf_list_insert_back(&soc->reo_desc_deferred_freelist,
			     (qdf_list_node_t *)desc);
	qdf_spin_unlock_bh(&soc->reo_desc_deferred_freelist_lock);

	return true;
}

/**
 * dp_reo_desc_defer_free() - free the REO QDESC in the deferred list
 *                            based on time threshold
 * @soc: Datapath soc handle
 *
 * Return: true if enqueued, else false
 */
static void dp_reo_desc_defer_free(struct dp_soc *soc)
{
	struct reo_desc_deferred_freelist_node *desc;
	unsigned long curr_ts = qdf_get_system_timestamp();

	qdf_spin_lock_bh(&soc->reo_desc_deferred_freelist_lock);

	while ((qdf_list_peek_front(&soc->reo_desc_deferred_freelist,
	       (qdf_list_node_t **)&desc) == QDF_STATUS_SUCCESS) &&
	       (curr_ts > (desc->free_ts + REO_DESC_DEFERRED_FREE_MS))) {
		qdf_list_remove_front(&soc->reo_desc_deferred_freelist,
				      (qdf_list_node_t **)&desc);

		DP_RX_REO_QDESC_DEFERRED_FREE_EVT(desc);

		qdf_mem_unmap_nbytes_single(soc->osdev,
					    desc->hw_qdesc_paddr,
					    QDF_DMA_BIDIRECTIONAL,
					    desc->hw_qdesc_alloc_size);
		qdf_mem_free(desc->hw_qdesc_vaddr_unaligned);
		qdf_mem_free(desc);

		curr_ts = qdf_get_system_timestamp();
	}

	qdf_spin_unlock_bh(&soc->reo_desc_deferred_freelist_lock);
}
#else
static inline bool
dp_reo_desc_defer_free_enqueue(struct dp_soc *soc,
			       struct reo_desc_list_node *freedesc)
{
	return false;
}

static void dp_reo_desc_defer_free(struct dp_soc *soc)
{
}
#endif /* !WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY */

void check_free_list_for_invalid_flush(struct dp_soc *soc)
{
	uint32_t i;
	uint32_t *addr_deref_val;
	unsigned long curr_ts = qdf_get_system_timestamp();
	uint32_t max_list_size;

	max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	if (max_list_size == 0)
		return;

	for (i = 0; i < soc->free_addr_list_idx; i++) {
		addr_deref_val = (uint32_t *)
			    soc->list_qdesc_addr_free[i].hw_qdesc_vaddr_unalign;

		if (*addr_deref_val == 0xDDBEEF84 ||
		    *addr_deref_val == 0xADBEEF84 ||
		    *addr_deref_val == 0xBDBEEF84 ||
		    *addr_deref_val == 0xCDBEEF84) {
			if (soc->list_qdesc_addr_free[i].ts_hw_flush_back == 0)
				soc->list_qdesc_addr_free[i].ts_hw_flush_back =
									curr_ts;
		}
	}
}

/**
 * dp_reo_desc_free() - Callback free reo descriptor memory after
 * HW cache flush
 *
 * @soc: DP SOC handle
 * @cb_ctxt: Callback context
 * @reo_status: REO command status
 */
static void dp_reo_desc_free(struct dp_soc *soc, void *cb_ctxt,
			     union hal_reo_status *reo_status)
{
	struct reo_desc_list_node *freedesc =
		(struct reo_desc_list_node *)cb_ctxt;
	struct dp_rx_tid *rx_tid = &freedesc->rx_tid;
	unsigned long curr_ts = qdf_get_system_timestamp();

	if ((reo_status->fl_cache_status.header.status !=
		HAL_REO_CMD_SUCCESS) &&
		(reo_status->fl_cache_status.header.status !=
		HAL_REO_CMD_DRAIN)) {
		dp_peer_err("%pK: Rx tid HW desc flush failed(%d): tid %d",
			    soc, reo_status->rx_queue_status.header.status,
			    freedesc->rx_tid.tid);
	}
	dp_peer_info("%pK: %lu hw_qdesc_paddr: %pK, tid:%d", soc,
		     curr_ts, (void *)(rx_tid->hw_qdesc_paddr),
		     rx_tid->tid);

	/* REO desc is enqueued to be freed at a later point
	 * in time, just free the freedesc alone and return
	 */
	if (dp_reo_desc_defer_free_enqueue(soc, freedesc))
		goto out;

	DP_RX_REO_QDESC_FREE_EVT(freedesc);
	add_entry_free_list(soc, rx_tid);

	hal_reo_shared_qaddr_cache_clear(soc->hal_soc);
	qdf_mem_unmap_nbytes_single(soc->osdev,
				    rx_tid->hw_qdesc_paddr,
				    QDF_DMA_BIDIRECTIONAL,
				    rx_tid->hw_qdesc_alloc_size);
	check_free_list_for_invalid_flush(soc);

	*(uint32_t *)rx_tid->hw_qdesc_vaddr_unaligned = 0;
	qdf_mem_free(rx_tid->hw_qdesc_vaddr_unaligned);
out:
	qdf_mem_free(freedesc);
}

#if defined(CONFIG_WIFI_EMULATION_WIFI_3_0) && defined(BUILD_X86)
/* Hawkeye emulation requires bus address to be >= 0x50000000 */
static inline int dp_reo_desc_addr_chk(qdf_dma_addr_t dma_addr)
{
	if (dma_addr < 0x50000000)
		return QDF_STATUS_E_FAILURE;
	else
		return QDF_STATUS_SUCCESS;
}
#else
static inline int dp_reo_desc_addr_chk(qdf_dma_addr_t dma_addr)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static inline void
dp_rx_tid_setup_error_process(uint32_t tid_bitmap, struct dp_peer *peer)
{
	struct dp_rx_tid *rx_tid;
	int tid;
	struct dp_soc *soc = peer->vdev->pdev->soc;

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		if (!(BIT(tid) & tid_bitmap))
			continue;

		rx_tid = &peer->rx_tid[tid];
		if (!rx_tid->hw_qdesc_vaddr_unaligned)
			continue;

		if (dp_reo_desc_addr_chk(rx_tid->hw_qdesc_paddr) ==
		    QDF_STATUS_SUCCESS)
			qdf_mem_unmap_nbytes_single(
				soc->osdev,
				rx_tid->hw_qdesc_paddr,
				QDF_DMA_BIDIRECTIONAL,
				rx_tid->hw_qdesc_alloc_size);
		qdf_mem_free(rx_tid->hw_qdesc_vaddr_unaligned);
		rx_tid->hw_qdesc_vaddr_unaligned = NULL;
		rx_tid->hw_qdesc_paddr = 0;
	}
}

static QDF_STATUS
dp_single_rx_tid_setup(struct dp_peer *peer, int tid,
		       uint32_t ba_window_size, uint32_t start_seq)
{
	struct dp_rx_tid *rx_tid = &peer->rx_tid[tid];
	struct dp_vdev *vdev = peer->vdev;
	struct dp_soc *soc = vdev->pdev->soc;
	uint32_t hw_qdesc_size;
	uint32_t hw_qdesc_align;
	int hal_pn_type;
	void *hw_qdesc_vaddr;
	uint32_t alloc_tries = 0, ret;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_txrx_peer *txrx_peer;

	rx_tid->delba_tx_status = 0;
	rx_tid->ppdu_id_2k = 0;
	rx_tid->num_of_addba_req = 0;
	rx_tid->num_of_delba_req = 0;
	rx_tid->num_of_addba_resp = 0;
	rx_tid->num_addba_rsp_failed = 0;
	rx_tid->num_addba_rsp_success = 0;
	rx_tid->delba_tx_success_cnt = 0;
	rx_tid->delba_tx_fail_cnt = 0;
	rx_tid->statuscode = 0;

	/* TODO: Allocating HW queue descriptors based on max BA window size
	 * for all QOS TIDs so that same descriptor can be used later when
	 * ADDBA request is received. This should be changed to allocate HW
	 * queue descriptors based on BA window size being negotiated (0 for
	 * non BA cases), and reallocate when BA window size changes and also
	 * send WMI message to FW to change the REO queue descriptor in Rx
	 * peer entry as part of dp_rx_tid_update.
	 */
	hw_qdesc_size = hal_get_reo_qdesc_size(soc->hal_soc,
					       ba_window_size, tid);

	hw_qdesc_align = hal_get_reo_qdesc_align(soc->hal_soc);
	/* To avoid unnecessary extra allocation for alignment, try allocating
	 * exact size and see if we already have aligned address.
	 */
	rx_tid->hw_qdesc_alloc_size = hw_qdesc_size;

try_desc_alloc:
	rx_tid->hw_qdesc_vaddr_unaligned =
		qdf_mem_malloc(rx_tid->hw_qdesc_alloc_size);

	if (!rx_tid->hw_qdesc_vaddr_unaligned) {
		dp_peer_err("%pK: Rx tid HW desc alloc failed: tid %d",
			    soc, tid);
		return QDF_STATUS_E_NOMEM;
	}

	if ((unsigned long)(rx_tid->hw_qdesc_vaddr_unaligned) %
		hw_qdesc_align) {
		/* Address allocated above is not aligned. Allocate extra
		 * memory for alignment
		 */
		qdf_mem_free(rx_tid->hw_qdesc_vaddr_unaligned);
		rx_tid->hw_qdesc_vaddr_unaligned =
			qdf_mem_malloc(rx_tid->hw_qdesc_alloc_size +
					hw_qdesc_align - 1);

		if (!rx_tid->hw_qdesc_vaddr_unaligned) {
			dp_peer_err("%pK: Rx tid HW desc alloc failed: tid %d",
				    soc, tid);
			return QDF_STATUS_E_NOMEM;
		}

		hw_qdesc_vaddr = (void *)qdf_align((unsigned long)
			rx_tid->hw_qdesc_vaddr_unaligned,
			hw_qdesc_align);

		dp_peer_debug("%pK: Total Size %d Aligned Addr %pK",
			      soc, rx_tid->hw_qdesc_alloc_size,
			      hw_qdesc_vaddr);

	} else {
		hw_qdesc_vaddr = rx_tid->hw_qdesc_vaddr_unaligned;
	}
	rx_tid->hw_qdesc_vaddr_aligned = hw_qdesc_vaddr;

	txrx_peer = dp_get_txrx_peer(peer);

	/* TODO: Ensure that sec_type is set before ADDBA is received.
	 * Currently this is set based on htt indication
	 * HTT_T2H_MSG_TYPE_SEC_IND from target
	 */
	switch (txrx_peer->security[dp_sec_ucast].sec_type) {
	case cdp_sec_type_tkip_nomic:
	case cdp_sec_type_aes_ccmp:
	case cdp_sec_type_aes_ccmp_256:
	case cdp_sec_type_aes_gcmp:
	case cdp_sec_type_aes_gcmp_256:
		hal_pn_type = HAL_PN_WPA;
		break;
	case cdp_sec_type_wapi:
		if (vdev->opmode == wlan_op_mode_ap)
			hal_pn_type = HAL_PN_WAPI_EVEN;
		else
			hal_pn_type = HAL_PN_WAPI_UNEVEN;
		break;
	default:
		hal_pn_type = HAL_PN_NONE;
		break;
	}

	hal_reo_qdesc_setup(soc->hal_soc, tid, ba_window_size, start_seq,
		hw_qdesc_vaddr, rx_tid->hw_qdesc_paddr, hal_pn_type,
		vdev->vdev_stats_id);

	ret = qdf_mem_map_nbytes_single(soc->osdev, hw_qdesc_vaddr,
					QDF_DMA_BIDIRECTIONAL,
					rx_tid->hw_qdesc_alloc_size,
					&rx_tid->hw_qdesc_paddr);

	if (!ret)
		add_entry_alloc_list(soc, rx_tid, peer, hw_qdesc_vaddr);

	if (dp_reo_desc_addr_chk(rx_tid->hw_qdesc_paddr) !=
			QDF_STATUS_SUCCESS || ret) {
		if (alloc_tries++ < 10) {
			qdf_mem_free(rx_tid->hw_qdesc_vaddr_unaligned);
			rx_tid->hw_qdesc_vaddr_unaligned = NULL;
			goto try_desc_alloc;
		} else {
			dp_peer_err("%pK: Rx tid %d desc alloc fail (lowmem)",
				    soc, tid);
			status = QDF_STATUS_E_NOMEM;
			goto error;
		}
	}

	return QDF_STATUS_SUCCESS;

error:
	dp_rx_tid_setup_error_process(1 << tid, peer);

	return status;
}

QDF_STATUS dp_rx_tid_setup_wifi3(struct dp_peer *peer,
				 uint32_t tid_bitmap,
				 uint32_t ba_window_size,
				 uint32_t start_seq)
{
	QDF_STATUS status;
	int tid;
	struct dp_rx_tid *rx_tid;
	struct dp_vdev *vdev = peer->vdev;
	struct dp_soc *soc = vdev->pdev->soc;
	uint8_t setup_fail_cnt = 0;

	if (!qdf_atomic_read(&peer->is_default_route_set))
		return QDF_STATUS_E_FAILURE;

	if (!dp_rx_tid_setup_allow(peer)) {
		dp_peer_info("skip rx tid setup for peer" QDF_MAC_ADDR_FMT,
			     QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		goto send_wmi_reo_cmd;
	}

	dp_peer_info("tid_bitmap 0x%x, ba_window_size %d, start_seq %d",
		     tid_bitmap, ba_window_size, start_seq);

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		if (!(BIT(tid) & tid_bitmap))
			continue;

		rx_tid = &peer->rx_tid[tid];
		rx_tid->ba_win_size = ba_window_size;
		if (rx_tid->hw_qdesc_vaddr_unaligned) {
			status = dp_rx_tid_update_wifi3(peer, tid,
					ba_window_size, start_seq, false);
			if (QDF_IS_STATUS_ERROR(status)) {
				/* Not continue to update other tid(s) and
				 * return even if they have not been set up.
				 */
				dp_peer_err("Update tid %d fail", tid);
				return status;
			}

			dp_peer_info("Update tid %d", tid);
			tid_bitmap &= ~BIT(tid);
			continue;
		}

		status = dp_single_rx_tid_setup(peer, tid,
						ba_window_size, start_seq);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_peer_err("Set up tid %d fail, status=%d",
				    tid, status);
			tid_bitmap &= ~BIT(tid);
			setup_fail_cnt++;
			continue;
		}
	}

	/* tid_bitmap == 0 means there is no tid(s) for further setup */
	if (!tid_bitmap) {
		dp_peer_info("tid_bitmap=0, no tid setup, setup_fail_cnt %d",
			     setup_fail_cnt);

		/*  If setup_fail_cnt==0, all tid(s) has been
		 * successfully updated, so we return success.
		 */
		if (!setup_fail_cnt)
			return QDF_STATUS_SUCCESS;
		else
			return QDF_STATUS_E_FAILURE;
	}

send_wmi_reo_cmd:
	if (dp_get_peer_vdev_roaming_in_progress(peer)) {
		status = QDF_STATUS_E_PERM;
		goto error;
	}

	dp_peer_info("peer %pK, tids 0x%x, multi_reo %d, s_seq %d, w_size %d",
		      peer, tid_bitmap,
		      soc->features.multi_rx_reorder_q_setup_support,
		      start_seq, ba_window_size);

	status = dp_peer_rx_reorder_queue_setup(soc, peer,
						tid_bitmap,
						ba_window_size);
	if (QDF_IS_STATUS_SUCCESS(status))
		return status;

error:
	dp_rx_tid_setup_error_process(tid_bitmap, peer);

	return status;
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
static
void dp_peer_rst_tids(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	int tid;

	for (tid = 0; tid < (DP_MAX_TIDS - 1); tid++) {
		struct dp_rx_tid *rx_tid = &peer->rx_tid[tid];
		void *vaddr = rx_tid->hw_qdesc_vaddr_aligned;

		if (vaddr)
			dp_reset_rx_reo_tid_queue(soc, vaddr,
						  rx_tid->hw_qdesc_alloc_size);
	}
}

void dp_reset_tid_q_setup(struct dp_soc *soc)
{
	dp_soc_iterate_peer(soc, dp_peer_rst_tids, NULL, DP_MOD_ID_UMAC_RESET);
}
#endif
#ifdef REO_DESC_DEFER_FREE
/**
 * dp_reo_desc_clean_up() - If cmd to flush base desc fails add
 * desc back to freelist and defer the deletion
 *
 * @soc: DP SOC handle
 * @desc: Base descriptor to be freed
 * @reo_status: REO command status
 */
static void dp_reo_desc_clean_up(struct dp_soc *soc,
				 struct reo_desc_list_node *desc,
				 union hal_reo_status *reo_status)
{
	desc->free_ts = qdf_get_system_timestamp();
	DP_STATS_INC(soc, rx.err.reo_cmd_send_fail, 1);
	qdf_list_insert_back(&soc->reo_desc_freelist,
			     (qdf_list_node_t *)desc);
}

/**
 * dp_reo_limit_clean_batch_sz() - Limit number REO CMD queued to cmd
 * ring in avoid of REO hang
 *
 * @list_size: REO desc list size to be cleaned
 */
static inline void dp_reo_limit_clean_batch_sz(uint32_t *list_size)
{
	unsigned long curr_ts = qdf_get_system_timestamp();

	if ((*list_size) > REO_DESC_FREELIST_SIZE) {
		dp_err_log("%lu:freedesc number %d in freelist",
			   curr_ts, *list_size);
		/* limit the batch queue size */
		*list_size = REO_DESC_FREELIST_SIZE;
	}
}
#else
/**
 * dp_reo_desc_clean_up() - If send cmd to REO inorder to flush
 * cache fails free the base REO desc anyway
 *
 * @soc: DP SOC handle
 * @desc: Base descriptor to be freed
 * @reo_status: REO command status
 */
static void dp_reo_desc_clean_up(struct dp_soc *soc,
				 struct reo_desc_list_node *desc,
				 union hal_reo_status *reo_status)
{
	if (reo_status) {
		qdf_mem_zero(reo_status, sizeof(*reo_status));
		reo_status->fl_cache_status.header.status = 0;
		dp_reo_desc_free(soc, (void *)desc, reo_status);
	}
}

/**
 * dp_reo_limit_clean_batch_sz() - Limit number REO CMD queued to cmd
 * ring in avoid of REO hang
 *
 * @list_size: REO desc list size to be cleaned
 */
static inline void dp_reo_limit_clean_batch_sz(uint32_t *list_size)
{
}
#endif

/**
 * dp_resend_update_reo_cmd() - Resend the UPDATE_REO_QUEUE
 * cmd and re-insert desc into free list if send fails.
 *
 * @soc: DP SOC handle
 * @desc: desc with resend update cmd flag set
 * @rx_tid: Desc RX tid associated with update cmd for resetting
 * valid field to 0 in h/w
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_resend_update_reo_cmd(struct dp_soc *soc,
			 struct reo_desc_list_node *desc,
			 struct dp_rx_tid *rx_tid)
{
	struct hal_reo_cmd_params params;

	qdf_mem_zero(&params, sizeof(params));
	params.std.need_status = 1;
	params.std.addr_lo =
		rx_tid->hw_qdesc_paddr & 0xffffffff;
	params.std.addr_hi =
		(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
	params.u.upd_queue_params.update_vld = 1;
	params.u.upd_queue_params.vld = 0;
	desc->resend_update_reo_cmd = false;
	/*
	 * If the cmd send fails then set resend_update_reo_cmd flag
	 * and insert the desc at the end of the free list to retry.
	 */
	if (dp_reo_send_cmd(soc,
			    CMD_UPDATE_RX_REO_QUEUE,
			    &params,
			    dp_rx_tid_delete_cb,
			    (void *)desc)
	    != QDF_STATUS_SUCCESS) {
		desc->resend_update_reo_cmd = true;
		desc->free_ts = qdf_get_system_timestamp();
		qdf_list_insert_back(&soc->reo_desc_freelist,
				     (qdf_list_node_t *)desc);
		dp_err_log("failed to send reo cmd CMD_UPDATE_RX_REO_QUEUE");
		DP_STATS_INC(soc, rx.err.reo_cmd_send_fail, 1);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void dp_rx_tid_delete_cb(struct dp_soc *soc, void *cb_ctxt,
			 union hal_reo_status *reo_status)
{
	struct reo_desc_list_node *freedesc =
		(struct reo_desc_list_node *)cb_ctxt;
	uint32_t list_size;
	struct reo_desc_list_node *desc = NULL;
	unsigned long curr_ts = qdf_get_system_timestamp();
	uint32_t desc_size, tot_desc_size;
	struct hal_reo_cmd_params params;
	bool flush_failure = false;

	DP_RX_REO_QDESC_UPDATE_EVT(freedesc);

	if (reo_status->rx_queue_status.header.status == HAL_REO_CMD_DRAIN) {
		qdf_mem_zero(reo_status, sizeof(*reo_status));
		reo_status->fl_cache_status.header.status = HAL_REO_CMD_DRAIN;
		dp_reo_desc_free(soc, (void *)freedesc, reo_status);
		DP_STATS_INC(soc, rx.err.reo_cmd_send_drain, 1);
		return;
	} else if (reo_status->rx_queue_status.header.status !=
		HAL_REO_CMD_SUCCESS) {
		/* Should not happen normally. Just print error for now */
		dp_info_rl("Rx tid HW desc deletion failed(%d): tid %d",
			   reo_status->rx_queue_status.header.status,
			   freedesc->rx_tid.tid);
	}

	dp_peer_info("%pK: rx_tid: %d status: %d",
		     soc, freedesc->rx_tid.tid,
		     reo_status->rx_queue_status.header.status);

	qdf_spin_lock_bh(&soc->reo_desc_freelist_lock);
	freedesc->free_ts = curr_ts;
	qdf_list_insert_back_size(&soc->reo_desc_freelist,
				  (qdf_list_node_t *)freedesc, &list_size);

	/* MCL path add the desc back to reo_desc_freelist when REO FLUSH
	 * failed. it may cause the number of REO queue pending  in free
	 * list is even larger than REO_CMD_RING max size and lead REO CMD
	 * flood then cause REO HW in an unexpected condition. So it's
	 * needed to limit the number REO cmds in a batch operation.
	 */
	dp_reo_limit_clean_batch_sz(&list_size);

	while ((qdf_list_peek_front(&soc->reo_desc_freelist,
		(qdf_list_node_t **)&desc) == QDF_STATUS_SUCCESS) &&
		((list_size >= REO_DESC_FREELIST_SIZE) ||
		(curr_ts > (desc->free_ts + REO_DESC_FREE_DEFER_MS)) ||
		(desc->resend_update_reo_cmd && list_size))) {
		struct dp_rx_tid *rx_tid;

		qdf_list_remove_front(&soc->reo_desc_freelist,
				      (qdf_list_node_t **)&desc);
		list_size--;
		rx_tid = &desc->rx_tid;

		/* First process descs with resend_update_reo_cmd set */
		if (desc->resend_update_reo_cmd) {
			if (dp_resend_update_reo_cmd(soc, desc, rx_tid) !=
			    QDF_STATUS_SUCCESS)
				break;
			else
				continue;
		}

		/* Flush and invalidate REO descriptor from HW cache: Base and
		 * extension descriptors should be flushed separately
		 */
		if (desc->pending_ext_desc_size)
			tot_desc_size = desc->pending_ext_desc_size;
		else
			tot_desc_size = rx_tid->hw_qdesc_alloc_size;
		/* Get base descriptor size by passing non-qos TID */
		desc_size = hal_get_reo_qdesc_size(soc->hal_soc, 0,
						   DP_NON_QOS_TID);

		/* Flush reo extension descriptors */
		while ((tot_desc_size -= desc_size) > 0) {
			qdf_mem_zero(&params, sizeof(params));
			params.std.addr_lo =
				((uint64_t)(rx_tid->hw_qdesc_paddr) +
				tot_desc_size) & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;

			if (QDF_STATUS_SUCCESS !=
			    dp_reo_send_cmd(soc, CMD_FLUSH_CACHE, &params,
					    NULL, NULL)) {
				dp_info_rl("fail to send CMD_CACHE_FLUSH:"
					   "tid %d desc %pK", rx_tid->tid,
					   (void *)(rx_tid->hw_qdesc_paddr));
				desc->pending_ext_desc_size = tot_desc_size +
								      desc_size;
				dp_reo_desc_clean_up(soc, desc, reo_status);
				flush_failure = true;
				break;
			}
		}

		if (flush_failure)
			break;

		desc->pending_ext_desc_size = desc_size;

		/* Flush base descriptor */
		qdf_mem_zero(&params, sizeof(params));
		params.std.need_status = 1;
		params.std.addr_lo =
			(uint64_t)(rx_tid->hw_qdesc_paddr) & 0xffffffff;
		params.std.addr_hi = (uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
		if (rx_tid->ba_win_size > 256)
			params.u.fl_cache_params.flush_q_1k_desc = 1;
		params.u.fl_cache_params.fwd_mpdus_in_queue = 1;

		if (QDF_STATUS_SUCCESS != dp_reo_send_cmd(soc,
							  CMD_FLUSH_CACHE,
							  &params,
							  dp_reo_desc_free,
							  (void *)desc)) {
			union hal_reo_status reo_status;
			/*
			 * If dp_reo_send_cmd return failure, related TID queue desc
			 * should be unmapped. Also locally reo_desc, together with
			 * TID queue desc also need to be freed accordingly.
			 *
			 * Here invoke desc_free function directly to do clean up.
			 *
			 * In case of MCL path add the desc back to the free
			 * desc list and defer deletion.
			 */
			dp_info_rl("fail to send REO cmd to flush cache: tid %d",
				   rx_tid->tid);
			dp_reo_desc_clean_up(soc, desc, &reo_status);
			DP_STATS_INC(soc, rx.err.reo_cmd_send_fail, 1);
			break;
		}
	}
	qdf_spin_unlock_bh(&soc->reo_desc_freelist_lock);

	dp_reo_desc_defer_free(soc);
}

/**
 * dp_rx_tid_delete_wifi3() - Delete receive TID queue
 * @peer: Datapath peer handle
 * @tid: TID
 *
 * Return: 0 on success, error code on failure
 */
static int dp_rx_tid_delete_wifi3(struct dp_peer *peer, int tid)
{
	struct dp_rx_tid *rx_tid = &peer->rx_tid[tid];
	struct dp_soc *soc = peer->vdev->pdev->soc;
	union hal_reo_status reo_status;
	struct hal_reo_cmd_params params;
	struct reo_desc_list_node *freedesc =
		qdf_mem_malloc(sizeof(*freedesc));

	if (!freedesc) {
		dp_peer_err("%pK: malloc failed for freedesc: tid %d",
			    soc, tid);
		qdf_assert(0);
		return -ENOMEM;
	}

	freedesc->rx_tid = *rx_tid;
	freedesc->resend_update_reo_cmd = false;

	qdf_mem_zero(&params, sizeof(params));

	DP_RX_REO_QDESC_GET_MAC(freedesc, peer);

	reo_status.rx_queue_status.header.status = HAL_REO_CMD_SUCCESS;
	dp_rx_tid_delete_cb(soc, freedesc, &reo_status);

	rx_tid->hw_qdesc_vaddr_unaligned = NULL;
	rx_tid->hw_qdesc_alloc_size = 0;
	rx_tid->hw_qdesc_paddr = 0;

	return 0;
}

#ifdef DP_LFR
static void dp_peer_setup_remaining_tids(struct dp_peer *peer)
{
	int tid;
	uint32_t tid_bitmap = 0;

	for (tid = 1; tid < DP_MAX_TIDS-1; tid++)
		tid_bitmap |= BIT(tid);

	dp_peer_info("Sett up tid_bitmap 0x%x for peer %pK peer->local_id %d",
		     tid_bitmap, peer, peer->local_id);
	dp_rx_tid_setup_wifi3(peer, tid_bitmap, 1, 0);
}

#else
static void dp_peer_setup_remaining_tids(struct dp_peer *peer) {};
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_peer_rx_tids_init() - initialize each tids in peer
 * @peer: peer pointer
 *
 * Return: None
 */
static void dp_peer_rx_tids_init(struct dp_peer *peer)
{
	int tid;
	struct dp_rx_tid *rx_tid;
	struct dp_rx_tid_defrag *rx_tid_defrag;

	if (!IS_MLO_DP_LINK_PEER(peer)) {
		for (tid = 0; tid < DP_MAX_TIDS; tid++) {
			rx_tid_defrag = &peer->txrx_peer->rx_tid[tid];

			rx_tid_defrag->array = &rx_tid_defrag->base;
			rx_tid_defrag->defrag_timeout_ms = 0;
			rx_tid_defrag->defrag_waitlist_elem.tqe_next = NULL;
			rx_tid_defrag->defrag_waitlist_elem.tqe_prev = NULL;
			rx_tid_defrag->base.head = NULL;
			rx_tid_defrag->base.tail = NULL;
			rx_tid_defrag->tid = tid;
			rx_tid_defrag->defrag_peer = peer->txrx_peer;
		}
	}

	/* if not first assoc link peer,
	 * not to initialize rx_tids again.
	 */
	if (IS_MLO_DP_LINK_PEER(peer) && !peer->first_link)
		return;

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		rx_tid = &peer->rx_tid[tid];
		rx_tid->tid = tid;
		rx_tid->ba_win_size = 0;
		rx_tid->ba_status = DP_RX_BA_INACTIVE;
	}
}
#else
static void dp_peer_rx_tids_init(struct dp_peer *peer)
{
	int tid;
	struct dp_rx_tid *rx_tid;
	struct dp_rx_tid_defrag *rx_tid_defrag;

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		rx_tid = &peer->rx_tid[tid];

		rx_tid_defrag = &peer->txrx_peer->rx_tid[tid];
		rx_tid->tid = tid;
		rx_tid->ba_win_size = 0;
		rx_tid->ba_status = DP_RX_BA_INACTIVE;

		rx_tid_defrag->base.head = NULL;
		rx_tid_defrag->base.tail = NULL;
		rx_tid_defrag->tid = tid;
		rx_tid_defrag->array = &rx_tid_defrag->base;
		rx_tid_defrag->defrag_timeout_ms = 0;
		rx_tid_defrag->defrag_waitlist_elem.tqe_next = NULL;
		rx_tid_defrag->defrag_waitlist_elem.tqe_prev = NULL;
		rx_tid_defrag->defrag_peer = peer->txrx_peer;
	}
}
#endif

void dp_peer_rx_tid_setup(struct dp_peer *peer)
{
	struct dp_soc *soc = peer->vdev->pdev->soc;
	struct dp_txrx_peer *txrx_peer = dp_get_txrx_peer(peer);
	struct dp_vdev *vdev = peer->vdev;

	dp_peer_rx_tids_init(peer);

	/* Setup default (non-qos) rx tid queue */
	dp_rx_tid_setup_wifi3(peer, BIT(DP_NON_QOS_TID), 1, 0);

	/* Setup rx tid queue for TID 0.
	 * Other queues will be setup on receiving first packet, which will cause
	 * NULL REO queue error. For Mesh peer, if on one of the mesh AP the
	 * mesh peer is not deleted, the new addition of mesh peer on other mesh AP
	 * doesn't do BA negotiation leading to mismatch in BA windows.
	 * To avoid this send max BA window during init.
	 */
	if (qdf_unlikely(vdev->mesh_vdev) ||
	    qdf_unlikely(txrx_peer->nawds_enabled))
		dp_rx_tid_setup_wifi3(
				peer, BIT(0),
				hal_get_rx_max_ba_window(soc->hal_soc, 0),
				0);
	else
		dp_rx_tid_setup_wifi3(peer, BIT(0), 1, 0);

	/*
	 * Setup the rest of TID's to handle LFR
	 */
	dp_peer_setup_remaining_tids(peer);
}

void dp_peer_rx_cleanup(struct dp_vdev *vdev, struct dp_peer *peer)
{
	int tid;
	uint32_t tid_delete_mask = 0;

	if (!peer->txrx_peer)
		return;

	dp_info("Remove tids for peer: %pK", peer);

	for (tid = 0; tid < DP_MAX_TIDS; tid++) {
		struct dp_rx_tid *rx_tid = &peer->rx_tid[tid];
		struct dp_rx_tid_defrag *defrag_rx_tid =
				&peer->txrx_peer->rx_tid[tid];

		qdf_spin_lock_bh(&defrag_rx_tid->defrag_tid_lock);
		if (!peer->bss_peer || peer->vdev->opmode == wlan_op_mode_sta) {
			/* Cleanup defrag related resource */
			dp_rx_defrag_waitlist_remove(peer->txrx_peer, tid);
			dp_rx_reorder_flush_frag(peer->txrx_peer, tid);
		}
		qdf_spin_unlock_bh(&defrag_rx_tid->defrag_tid_lock);

		qdf_spin_lock_bh(&rx_tid->tid_lock);
		if (peer->rx_tid[tid].hw_qdesc_vaddr_unaligned) {
			dp_rx_tid_delete_wifi3(peer, tid);

			tid_delete_mask |= (1 << tid);
		}
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
	}
#ifdef notyet /* See if FW can remove queues as part of peer cleanup */
	if (soc->ol_ops->peer_rx_reorder_queue_remove) {
		soc->ol_ops->peer_rx_reorder_queue_remove(soc->ctrl_psoc,
			peer->vdev->pdev->pdev_id,
			peer->vdev->vdev_id, peer->mac_addr.raw,
			tid_delete_mask);
	}
#endif
}

/**
 * dp_teardown_256_ba_sessions() - Teardown sessions using 256
 *                                window size when a request with
 *                                64 window size is received.
 *                                This is done as a WAR since HW can
 *                                have only one setting per peer (64 or 256).
 *                                For HKv2, we use per tid buffersize setting
 *                                for 0 to per_tid_basize_max_tid. For tid
 *                                more than per_tid_basize_max_tid we use HKv1
 *                                method.
 * @peer: Datapath peer
 *
 * Return: void
 */
static void dp_teardown_256_ba_sessions(struct dp_peer *peer)
{
	uint8_t delba_rcode = 0;
	int tid;
	struct dp_rx_tid *rx_tid = NULL;

	tid = peer->vdev->pdev->soc->per_tid_basize_max_tid;
	for (; tid < DP_MAX_TIDS; tid++) {
		rx_tid = &peer->rx_tid[tid];
		qdf_spin_lock_bh(&rx_tid->tid_lock);

		if (rx_tid->ba_win_size <= 64) {
			qdf_spin_unlock_bh(&rx_tid->tid_lock);
			continue;
		} else {
			if (rx_tid->ba_status == DP_RX_BA_ACTIVE ||
			    rx_tid->ba_status == DP_RX_BA_IN_PROGRESS) {
				/* send delba */
				if (!rx_tid->delba_tx_status) {
					rx_tid->delba_tx_retry++;
					rx_tid->delba_tx_status = 1;
					rx_tid->delba_rcode =
					IEEE80211_REASON_QOS_SETUP_REQUIRED;
					delba_rcode = rx_tid->delba_rcode;

					qdf_spin_unlock_bh(&rx_tid->tid_lock);
					if (peer->vdev->pdev->soc->cdp_soc.ol_ops->send_delba)
						peer->vdev->pdev->soc->cdp_soc.ol_ops->send_delba(
							peer->vdev->pdev->soc->ctrl_psoc,
							peer->vdev->vdev_id,
							peer->mac_addr.raw,
							tid, delba_rcode,
							CDP_DELBA_REASON_NONE);
				} else {
					qdf_spin_unlock_bh(&rx_tid->tid_lock);
				}
			} else {
				qdf_spin_unlock_bh(&rx_tid->tid_lock);
			}
		}
	}
}

int dp_addba_resp_tx_completion_wifi3(struct cdp_soc_t *cdp_soc,
				      uint8_t *peer_mac,
				      uint16_t vdev_id,
				      uint8_t tid, int status)
{
	struct dp_peer *peer = dp_peer_get_tgt_peer_hash_find(
					(struct dp_soc *)cdp_soc,
					peer_mac, 0, vdev_id,
					DP_MOD_ID_CDP);
	struct dp_rx_tid *rx_tid = NULL;

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		goto fail;
	}
	rx_tid = &peer->rx_tid[tid];
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	if (status) {
		rx_tid->num_addba_rsp_failed++;
		if (rx_tid->hw_qdesc_vaddr_unaligned)
			dp_rx_tid_update_wifi3(peer, tid, 1,
					       IEEE80211_SEQ_MAX, false);
		rx_tid->ba_status = DP_RX_BA_INACTIVE;
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
		dp_err("RxTid- %d addba rsp tx completion failed", tid);

		goto success;
	}

	rx_tid->num_addba_rsp_success++;
	if (rx_tid->ba_status == DP_RX_BA_INACTIVE) {
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
		dp_peer_err("%pK: Rx Tid- %d hw qdesc is not in IN_PROGRESS",
			    cdp_soc, tid);
		goto fail;
	}

	if (!qdf_atomic_read(&peer->is_default_route_set)) {
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
		dp_peer_debug("%pK: default route is not set for peer: " QDF_MAC_ADDR_FMT,
			      cdp_soc, QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		goto fail;
	}

	if (dp_rx_tid_update_wifi3(peer, tid,
				   rx_tid->ba_win_size,
				   rx_tid->startseqnum,
				   false)) {
		dp_err("Failed update REO SSN");
	}

	dp_info("tid %u window_size %u start_seq_num %u",
		tid, rx_tid->ba_win_size,
		rx_tid->startseqnum);

	/* First Session */
	if (peer->active_ba_session_cnt == 0) {
		if (rx_tid->ba_win_size > 64 && rx_tid->ba_win_size <= 256)
			peer->hw_buffer_size = 256;
		else if (rx_tid->ba_win_size <= 1024 &&
			 rx_tid->ba_win_size > 256)
			peer->hw_buffer_size = 1024;
		else
			peer->hw_buffer_size = 64;
	}

	rx_tid->ba_status = DP_RX_BA_ACTIVE;

	peer->active_ba_session_cnt++;

	qdf_spin_unlock_bh(&rx_tid->tid_lock);

	/* Kill any session having 256 buffer size
	 * when 64 buffer size request is received.
	 * Also, latch on to 64 as new buffer size.
	 */
	if (peer->kill_256_sessions) {
		dp_teardown_256_ba_sessions(peer);
		peer->kill_256_sessions = 0;
	}

success:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;

fail:
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_addba_responsesetup_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			     uint16_t vdev_id, uint8_t tid,
			     uint8_t *dialogtoken, uint16_t *statuscode,
			     uint16_t *buffersize, uint16_t *batimeout)
{
	struct dp_rx_tid *rx_tid = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)cdp_soc,
						       peer_mac, 0, vdev_id,
						       DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		return QDF_STATUS_E_FAILURE;
	}
	rx_tid = &peer->rx_tid[tid];
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	rx_tid->num_of_addba_resp++;
	/* setup ADDBA response parameters */
	*dialogtoken = rx_tid->dialogtoken;
	*statuscode = rx_tid->statuscode;
	*buffersize = rx_tid->ba_win_size;
	*batimeout  = 0;
	qdf_spin_unlock_bh(&rx_tid->tid_lock);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

/**
 * dp_check_ba_buffersize() - Check buffer size in request
 *                            and latch onto this size based on
 *                            size used in first active session.
 * @peer: Datapath peer
 * @tid: Tid
 * @buffersize: Block ack window size
 *
 * Return: void
 */
static void dp_check_ba_buffersize(struct dp_peer *peer,
				   uint16_t tid,
				   uint16_t buffersize)
{
	struct dp_rx_tid *rx_tid = NULL;
	struct dp_soc *soc = peer->vdev->pdev->soc;
	uint16_t max_ba_window;

	max_ba_window = hal_get_rx_max_ba_window(soc->hal_soc, tid);
	dp_info("Input buffersize %d, max dp allowed %d",
		buffersize, max_ba_window);
	/* Adjust BA window size, restrict it to max DP allowed */
	buffersize = QDF_MIN(buffersize, max_ba_window);

	dp_info(QDF_MAC_ADDR_FMT" per_tid_basize_max_tid %d tid %d buffersize %d hw_buffer_size %d",
		QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		soc->per_tid_basize_max_tid, tid, buffersize,
		peer->hw_buffer_size);

	rx_tid = &peer->rx_tid[tid];
	if (soc->per_tid_basize_max_tid &&
	    tid < soc->per_tid_basize_max_tid) {
		rx_tid->ba_win_size = buffersize;
		goto out;
	} else {
		if (peer->active_ba_session_cnt == 0) {
			rx_tid->ba_win_size = buffersize;
		} else {
			if (peer->hw_buffer_size == 64) {
				if (buffersize <= 64)
					rx_tid->ba_win_size = buffersize;
				else
					rx_tid->ba_win_size = peer->hw_buffer_size;
			} else if (peer->hw_buffer_size == 256) {
				if (buffersize > 64) {
					rx_tid->ba_win_size = buffersize;
				} else {
					rx_tid->ba_win_size = buffersize;
					peer->hw_buffer_size = 64;
					peer->kill_256_sessions = 1;
				}
			} else if (buffersize <= 1024) {
				/*
				 * Above checks are only for HK V2
				 * Set incoming buffer size for others
				 */
				rx_tid->ba_win_size = buffersize;
			} else {
				dp_err("Invalid buffer size %d", buffersize);
				qdf_assert_always(0);
			}
		}
	}

out:
	dp_info("rx_tid->ba_win_size %d peer->hw_buffer_size %d peer->kill_256_sessions %d",
		rx_tid->ba_win_size,
		peer->hw_buffer_size,
		peer->kill_256_sessions);
}

QDF_STATUS dp_rx_tid_update_ba_win_size(struct cdp_soc_t *cdp_soc,
					uint8_t *peer_mac, uint16_t vdev_id,
					uint8_t tid, uint16_t buffersize)
{
	struct dp_rx_tid *rx_tid = NULL;
	struct dp_peer *peer;

	peer = dp_peer_get_tgt_peer_hash_find((struct dp_soc *)cdp_soc,
					      peer_mac, 0, vdev_id,
					      DP_MOD_ID_CDP);
	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		return QDF_STATUS_E_FAILURE;
	}

	rx_tid = &peer->rx_tid[tid];

	qdf_spin_lock_bh(&rx_tid->tid_lock);
	rx_tid->ba_win_size = buffersize;
	qdf_spin_unlock_bh(&rx_tid->tid_lock);

	dp_info("peer "QDF_MAC_ADDR_FMT", tid %d, update BA win size to %d",
		QDF_MAC_ADDR_REF(peer->mac_addr.raw), tid, buffersize);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

#define DP_RX_BA_SESSION_DISABLE  1

int dp_addba_requestprocess_wifi3(struct cdp_soc_t *cdp_soc,
				  uint8_t *peer_mac,
				  uint16_t vdev_id,
				  uint8_t dialogtoken,
				  uint16_t tid, uint16_t batimeout,
				  uint16_t buffersize,
				  uint16_t startseqnum)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_rx_tid *rx_tid = NULL;
	struct dp_peer *peer;

	peer = dp_peer_get_tgt_peer_hash_find((struct dp_soc *)cdp_soc,
					      peer_mac,
					      0, vdev_id,
					      DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		return QDF_STATUS_E_FAILURE;
	}
	rx_tid = &peer->rx_tid[tid];
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	rx_tid->num_of_addba_req++;
	if ((rx_tid->ba_status == DP_RX_BA_ACTIVE &&
	     rx_tid->hw_qdesc_vaddr_unaligned)) {
		dp_rx_tid_update_wifi3(peer, tid, 1, IEEE80211_SEQ_MAX, false);
		rx_tid->ba_status = DP_RX_BA_INACTIVE;
		peer->active_ba_session_cnt--;
		dp_peer_debug("%pK: Rx Tid- %d hw qdesc is already setup",
			      cdp_soc, tid);
	}

	if (rx_tid->ba_status == DP_RX_BA_IN_PROGRESS) {
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}

	if (rx_tid->rx_ba_win_size_override == DP_RX_BA_SESSION_DISABLE) {
		dp_peer_info("%pK: disable BA session",
			     cdp_soc);

		buffersize = 1;
	} else if (rx_tid->rx_ba_win_size_override) {
		dp_peer_info("%pK: override BA win to %d", cdp_soc,
			     rx_tid->rx_ba_win_size_override);

		buffersize = rx_tid->rx_ba_win_size_override;
	} else {
		dp_peer_info("%pK: restore BA win %d based on addba req", cdp_soc,
			     buffersize);
	}

	dp_check_ba_buffersize(peer, tid, buffersize);

	if (dp_rx_tid_setup_wifi3(peer, BIT(tid),
	    rx_tid->ba_win_size, startseqnum)) {
		rx_tid->ba_status = DP_RX_BA_INACTIVE;
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}
	rx_tid->ba_status = DP_RX_BA_IN_PROGRESS;

	rx_tid->dialogtoken = dialogtoken;
	rx_tid->startseqnum = startseqnum;

	if (rx_tid->userstatuscode != IEEE80211_STATUS_SUCCESS)
		rx_tid->statuscode = rx_tid->userstatuscode;
	else
		rx_tid->statuscode = IEEE80211_STATUS_SUCCESS;

	if (rx_tid->rx_ba_win_size_override == DP_RX_BA_SESSION_DISABLE)
		rx_tid->statuscode = IEEE80211_STATUS_REFUSED;

	qdf_spin_unlock_bh(&rx_tid->tid_lock);

fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

QDF_STATUS
dp_set_addba_response(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
		      uint16_t vdev_id, uint8_t tid, uint16_t statuscode)
{
	struct dp_peer *peer = dp_peer_get_tgt_peer_hash_find(
					(struct dp_soc *)cdp_soc,
					peer_mac, 0, vdev_id,
					DP_MOD_ID_CDP);
	struct dp_rx_tid *rx_tid;

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		return QDF_STATUS_E_FAILURE;
	}

	rx_tid = &peer->rx_tid[tid];
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	rx_tid->userstatuscode = statuscode;
	qdf_spin_unlock_bh(&rx_tid->tid_lock);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

int dp_delba_process_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			   uint16_t vdev_id, int tid, uint16_t reasoncode)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_rx_tid *rx_tid;
	struct dp_peer *peer = dp_peer_get_tgt_peer_hash_find(
					(struct dp_soc *)cdp_soc,
					peer_mac, 0, vdev_id,
					DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		return QDF_STATUS_E_FAILURE;
	}
	rx_tid = &peer->rx_tid[tid];
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	if (rx_tid->ba_status == DP_RX_BA_INACTIVE ||
	    rx_tid->ba_status == DP_RX_BA_IN_PROGRESS) {
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}
	/* TODO: See if we can delete the existing REO queue descriptor and
	 * replace with a new one without queue extension descript to save
	 * memory
	 */
	rx_tid->delba_rcode = reasoncode;
	rx_tid->num_of_delba_req++;
	dp_rx_tid_update_wifi3(peer, tid, 1, IEEE80211_SEQ_MAX, false);

	rx_tid->ba_status = DP_RX_BA_INACTIVE;
	peer->active_ba_session_cnt--;
	qdf_spin_unlock_bh(&rx_tid->tid_lock);
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

int dp_delba_tx_completion_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
				 uint16_t vdev_id,
				 uint8_t tid, int status)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct dp_rx_tid *rx_tid = NULL;
	struct dp_peer *peer = dp_peer_get_tgt_peer_hash_find(
					(struct dp_soc *)cdp_soc,
					peer_mac, 0, vdev_id,
					DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", cdp_soc);
		return QDF_STATUS_E_FAILURE;
	}
	rx_tid = &peer->rx_tid[tid];
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	if (status) {
		rx_tid->delba_tx_fail_cnt++;
		if (rx_tid->delba_tx_retry >= DP_MAX_DELBA_RETRY) {
			rx_tid->delba_tx_retry = 0;
			rx_tid->delba_tx_status = 0;
			qdf_spin_unlock_bh(&rx_tid->tid_lock);
		} else {
			rx_tid->delba_tx_retry++;
			rx_tid->delba_tx_status = 1;
			qdf_spin_unlock_bh(&rx_tid->tid_lock);
			if (peer->vdev->pdev->soc->cdp_soc.ol_ops->send_delba)
				peer->vdev->pdev->soc->cdp_soc.ol_ops->send_delba(
					peer->vdev->pdev->soc->ctrl_psoc,
					peer->vdev->vdev_id,
					peer->mac_addr.raw, tid,
					rx_tid->delba_rcode,
					CDP_DELBA_REASON_NONE);
		}
		goto end;
	} else {
		rx_tid->delba_tx_success_cnt++;
		rx_tid->delba_tx_retry = 0;
		rx_tid->delba_tx_status = 0;
	}
	if (rx_tid->ba_status == DP_RX_BA_ACTIVE) {
		dp_rx_tid_update_wifi3(peer, tid, 1, IEEE80211_SEQ_MAX, false);
		rx_tid->ba_status = DP_RX_BA_INACTIVE;
		peer->active_ba_session_cnt--;
	}
	if (rx_tid->ba_status == DP_RX_BA_IN_PROGRESS) {
		dp_rx_tid_update_wifi3(peer, tid, 1, IEEE80211_SEQ_MAX, false);
		rx_tid->ba_status = DP_RX_BA_INACTIVE;
	}
	qdf_spin_unlock_bh(&rx_tid->tid_lock);

end:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return ret;
}

QDF_STATUS
dp_set_pn_check_wifi3(struct cdp_soc_t *soc_t, uint8_t vdev_id,
		      uint8_t *peer_mac, enum cdp_sec_type sec_type,
		      uint32_t *rx_pn)
{
	struct dp_pdev *pdev;
	int i;
	uint8_t pn_size;
	struct hal_reo_cmd_params params;
	struct dp_peer *peer = NULL;
	struct dp_vdev *vdev = NULL;
	struct dp_soc *soc = NULL;

	peer = dp_peer_get_tgt_peer_hash_find((struct dp_soc *)soc_t,
					      peer_mac, 0, vdev_id,
					      DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("%pK: Peer is NULL!", soc);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = peer->vdev;

	if (!vdev) {
		dp_peer_debug("%pK: VDEV is NULL!", soc);
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	pdev = vdev->pdev;
	soc = pdev->soc;
	qdf_mem_zero(&params, sizeof(params));

	params.std.need_status = 1;
	params.u.upd_queue_params.update_pn_valid = 1;
	params.u.upd_queue_params.update_pn_size = 1;
	params.u.upd_queue_params.update_pn = 1;
	params.u.upd_queue_params.update_pn_check_needed = 1;
	params.u.upd_queue_params.update_svld = 1;
	params.u.upd_queue_params.svld = 0;

	switch (sec_type) {
	case cdp_sec_type_tkip_nomic:
	case cdp_sec_type_aes_ccmp:
	case cdp_sec_type_aes_ccmp_256:
	case cdp_sec_type_aes_gcmp:
	case cdp_sec_type_aes_gcmp_256:
		params.u.upd_queue_params.pn_check_needed = 1;
		params.u.upd_queue_params.pn_size = PN_SIZE_48;
		pn_size = 48;
		break;
	case cdp_sec_type_wapi:
		params.u.upd_queue_params.pn_check_needed = 1;
		params.u.upd_queue_params.pn_size = PN_SIZE_128;
		pn_size = 128;
		if (vdev->opmode == wlan_op_mode_ap) {
			params.u.upd_queue_params.pn_even = 1;
			params.u.upd_queue_params.update_pn_even = 1;
		} else {
			params.u.upd_queue_params.pn_uneven = 1;
			params.u.upd_queue_params.update_pn_uneven = 1;
		}
		break;
	default:
		params.u.upd_queue_params.pn_check_needed = 0;
		pn_size = 0;
		break;
	}

	for (i = 0; i < DP_MAX_TIDS; i++) {
		struct dp_rx_tid *rx_tid = &peer->rx_tid[i];

		qdf_spin_lock_bh(&rx_tid->tid_lock);
		if (rx_tid->hw_qdesc_vaddr_unaligned) {
			params.std.addr_lo =
				rx_tid->hw_qdesc_paddr & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;

			if (pn_size) {
				dp_peer_info("%pK: PN set for TID:%d pn:%x:%x:%x:%x",
					     soc, i, rx_pn[3], rx_pn[2],
					     rx_pn[1], rx_pn[0]);
				params.u.upd_queue_params.update_pn_valid = 1;
				params.u.upd_queue_params.pn_31_0 = rx_pn[0];
				params.u.upd_queue_params.pn_63_32 = rx_pn[1];
				params.u.upd_queue_params.pn_95_64 = rx_pn[2];
				params.u.upd_queue_params.pn_127_96 = rx_pn[3];
			}
			rx_tid->pn_size = pn_size;
			if (dp_reo_send_cmd(soc,
					    CMD_UPDATE_RX_REO_QUEUE,
					    &params, dp_rx_tid_update_cb,
					    rx_tid)) {
				dp_err_log("fail to send CMD_UPDATE_RX_REO_QUEUE"
					   "tid %d desc %pK", rx_tid->tid,
					   (void *)(rx_tid->hw_qdesc_paddr));
				DP_STATS_INC(soc,
					     rx.err.reo_cmd_send_fail, 1);
			}
		} else {
			dp_peer_info("%pK: PN Check not setup for TID :%d ", soc, i);
		}
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_rx_delba_ind_handler(void *soc_handle, uint16_t peer_id,
			uint8_t tid, uint16_t win_sz)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;
	struct dp_peer *peer;
	struct dp_rx_tid *rx_tid;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_HTT);

	if (!peer) {
		dp_peer_err("%pK: Couldn't find peer from ID %d",
			    soc, peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_assert_always(tid < DP_MAX_TIDS);

	rx_tid = &peer->rx_tid[tid];

	if (rx_tid->hw_qdesc_vaddr_unaligned) {
		if (!rx_tid->delba_tx_status) {
			dp_peer_info("%pK: PEER_ID: %d TID: %d, BA win: %d ",
				     soc, peer_id, tid, win_sz);

			qdf_spin_lock_bh(&rx_tid->tid_lock);

			rx_tid->delba_tx_status = 1;

			rx_tid->rx_ba_win_size_override =
			    qdf_min((uint16_t)63, win_sz);

			rx_tid->delba_rcode =
			    IEEE80211_REASON_QOS_SETUP_REQUIRED;

			qdf_spin_unlock_bh(&rx_tid->tid_lock);

			if (soc->cdp_soc.ol_ops->send_delba)
				soc->cdp_soc.ol_ops->send_delba(
					peer->vdev->pdev->soc->ctrl_psoc,
					peer->vdev->vdev_id,
					peer->mac_addr.raw,
					tid,
					rx_tid->delba_rcode,
					CDP_DELBA_REASON_NONE);
		}
	} else {
		dp_peer_err("%pK: BA session is not setup for TID:%d ",
			    soc, tid);
		status = QDF_STATUS_E_FAILURE;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_HTT);

	return status;
}

#ifdef IPA_OFFLOAD
int dp_peer_get_rxtid_stats_ipa(struct dp_peer *peer,
				dp_rxtid_stats_cmd_cb dp_stats_cmd_cb)
{
	struct dp_soc *soc = peer->vdev->pdev->soc;
	struct hal_reo_cmd_params params;
	int i;
	int stats_cmd_sent_cnt = 0;
	QDF_STATUS status;
	uint16_t peer_id = peer->peer_id;
	unsigned long comb_peer_id_tid;
	struct dp_rx_tid *rx_tid;

	if (!dp_stats_cmd_cb)
		return stats_cmd_sent_cnt;

	qdf_mem_zero(&params, sizeof(params));
	for (i = 0; i < DP_MAX_TIDS; i++) {
		if ((i >= CDP_DATA_TID_MAX) && (i != CDP_DATA_NON_QOS_TID))
			continue;

		rx_tid = &peer->rx_tid[i];
		if (rx_tid->hw_qdesc_vaddr_unaligned) {
			params.std.need_status = 1;
			params.std.addr_lo =
				rx_tid->hw_qdesc_paddr & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
			params.u.stats_params.clear = 1;
			comb_peer_id_tid = ((i << DP_PEER_REO_STATS_TID_SHIFT)
					    | peer_id);
			status = dp_reo_send_cmd(soc, CMD_GET_QUEUE_STATS,
						 &params, dp_stats_cmd_cb,
						 (void *)comb_peer_id_tid);
			if (QDF_IS_STATUS_SUCCESS(status))
				stats_cmd_sent_cnt++;

			/* Flush REO descriptor from HW cache to update stats
			 * in descriptor memory. This is to help debugging
			 */
			qdf_mem_zero(&params, sizeof(params));
			params.std.need_status = 0;
			params.std.addr_lo =
				rx_tid->hw_qdesc_paddr & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
			params.u.fl_cache_params.flush_no_inval = 1;
			dp_reo_send_cmd(soc, CMD_FLUSH_CACHE, &params, NULL,
					NULL);
		}
	}

	return stats_cmd_sent_cnt;
}

qdf_export_symbol(dp_peer_get_rxtid_stats_ipa);

#endif
int dp_peer_rxtid_stats(struct dp_peer *peer,
			dp_rxtid_stats_cmd_cb dp_stats_cmd_cb,
			void *cb_ctxt)
{
	struct dp_soc *soc = peer->vdev->pdev->soc;
	struct hal_reo_cmd_params params;
	int i;
	int stats_cmd_sent_cnt = 0;
	QDF_STATUS status;
	struct dp_rx_tid *rx_tid;

	if (!dp_stats_cmd_cb)
		return stats_cmd_sent_cnt;

	qdf_mem_zero(&params, sizeof(params));
	for (i = 0; i < DP_MAX_TIDS; i++) {
		if ((i >= CDP_DATA_TID_MAX) && (i != CDP_DATA_NON_QOS_TID))
			continue;

		rx_tid = &peer->rx_tid[i];
		if (rx_tid->hw_qdesc_vaddr_unaligned) {
			params.std.need_status = 1;
			params.std.addr_lo =
				rx_tid->hw_qdesc_paddr & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;

			if (cb_ctxt) {
				status = dp_reo_send_cmd(
						soc, CMD_GET_QUEUE_STATS,
						&params, dp_stats_cmd_cb,
						cb_ctxt);
			} else {
				status = dp_reo_send_cmd(
						soc, CMD_GET_QUEUE_STATS,
						&params, dp_stats_cmd_cb,
						rx_tid);
			}

			if (QDF_IS_STATUS_SUCCESS(status))
				stats_cmd_sent_cnt++;

			/* Flush REO descriptor from HW cache to update stats
			 * in descriptor memory. This is to help debugging
			 */
			qdf_mem_zero(&params, sizeof(params));
			params.std.need_status = 0;
			params.std.addr_lo =
				rx_tid->hw_qdesc_paddr & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
			params.u.fl_cache_params.flush_no_inval = 1;
			dp_reo_send_cmd(soc, CMD_FLUSH_CACHE, &params, NULL,
					NULL);
		}
	}

	return stats_cmd_sent_cnt;
}

QDF_STATUS dp_peer_rx_tids_create(struct dp_peer *peer)
{
	uint8_t i;

	if (IS_MLO_DP_MLD_PEER(peer)) {
		dp_peer_info("skip for mld peer");
		return QDF_STATUS_SUCCESS;
	}

	if (peer->rx_tid) {
		QDF_BUG(0);
		dp_peer_err("peer rx_tid mem already exist");
		return QDF_STATUS_E_FAILURE;
	}

	peer->rx_tid = qdf_mem_malloc(DP_MAX_TIDS *
			sizeof(struct dp_rx_tid));

	if (!peer->rx_tid) {
		dp_err("fail to alloc tid for peer" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_zero(peer->rx_tid, DP_MAX_TIDS * sizeof(struct dp_rx_tid));
	for (i = 0; i < DP_MAX_TIDS; i++)
		qdf_spinlock_create(&peer->rx_tid[i].tid_lock);

	return QDF_STATUS_SUCCESS;
}

void dp_peer_rx_tids_destroy(struct dp_peer *peer)
{
	uint8_t i;

	if (!IS_MLO_DP_LINK_PEER(peer)) {
		for (i = 0; i < DP_MAX_TIDS; i++)
			qdf_spinlock_destroy(&peer->rx_tid[i].tid_lock);

		qdf_mem_free(peer->rx_tid);
	}

	peer->rx_tid = NULL;
}

#ifdef DUMP_REO_QUEUE_INFO_IN_DDR
void dp_dump_rx_reo_queue_info(
	struct dp_soc *soc, void *cb_ctxt, union hal_reo_status *reo_status)
{
	struct dp_rx_tid *rx_tid = (struct dp_rx_tid *)cb_ctxt;

	if (!rx_tid)
		return;

	if (reo_status->fl_cache_status.header.status !=
		HAL_REO_CMD_SUCCESS) {
		dp_err_rl("Rx tid REO HW desc flush failed(%d)",
			  reo_status->rx_queue_status.header.status);
		return;
	}
	qdf_spin_lock_bh(&rx_tid->tid_lock);
	hal_dump_rx_reo_queue_desc(rx_tid->hw_qdesc_vaddr_aligned);
	qdf_spin_unlock_bh(&rx_tid->tid_lock);
}

void dp_send_cache_flush_for_rx_tid(
	struct dp_soc *soc, struct dp_peer *peer)
{
	int i;
	struct dp_rx_tid *rx_tid;
	struct hal_reo_cmd_params params;

	if (!peer) {
		dp_err_rl("Peer is NULL");
		return;
	}

	for (i = 0; i < DP_MAX_TIDS; i++) {
		rx_tid = &peer->rx_tid[i];
		if (!rx_tid)
			continue;
		qdf_spin_lock_bh(&rx_tid->tid_lock);
		if (rx_tid->hw_qdesc_vaddr_aligned) {
			qdf_mem_zero(&params, sizeof(params));
			params.std.need_status = 1;
			params.std.addr_lo =
				rx_tid->hw_qdesc_paddr & 0xffffffff;
			params.std.addr_hi =
				(uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
			params.u.fl_cache_params.flush_no_inval = 0;

			if (rx_tid->ba_win_size > 256)
				params.u.fl_cache_params.flush_q_1k_desc = 1;
			params.u.fl_cache_params.fwd_mpdus_in_queue = 1;

			if (QDF_STATUS_SUCCESS !=
				dp_reo_send_cmd(
					soc, CMD_FLUSH_CACHE,
					&params, dp_dump_rx_reo_queue_info,
					(void *)rx_tid)) {
				dp_err_rl("cache flush send failed tid %d",
					  rx_tid->tid);
				qdf_spin_unlock_bh(&rx_tid->tid_lock);
				break;
			}
		}
		qdf_spin_unlock_bh(&rx_tid->tid_lock);
	}
}

void dp_get_rx_reo_queue_info(
	struct cdp_soc_t *soc_hdl, uint8_t vdev_id)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_GENERIC_STATS);
	struct dp_peer *peer = NULL;

	if (!vdev) {
		dp_err_rl("vdev is null for vdev_id: %u", vdev_id);
		goto failed;
	}

	peer = dp_vdev_bss_peer_ref_n_get(soc, vdev, DP_MOD_ID_GENERIC_STATS);

	if (!peer) {
		dp_err_rl("Peer is NULL");
		goto failed;
	}
	dp_send_cache_flush_for_rx_tid(soc, peer);
failed:
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_GENERIC_STATS);
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_GENERIC_STATS);
}
#endif /* DUMP_REO_QUEUE_INFO_IN_DDR */


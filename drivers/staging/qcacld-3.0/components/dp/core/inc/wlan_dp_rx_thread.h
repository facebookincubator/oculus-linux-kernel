/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(__WLAN_DP_RX_THREAD_H)
#define __WLAN_DP_RX_THREAD_H

#include <qdf_lock.h>
#include <qdf_event.h>
#include <qdf_threads.h>
#include <wlan_objmgr_vdev_obj.h>
#include "cfg_dp.h"
#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>
#include "wlan_cfg.h"
#include "qdf_nbuf.h"
#include "qdf_threads.h"
#include "qdf_net_if.h"

/* Maximum number of REO rings supported (for stats tracking) */
#define DP_RX_TM_MAX_REO_RINGS WLAN_CFG_NUM_REO_DEST_RING
/* Number of DP RX threads supported */
#define DP_MAX_RX_THREADS WLAN_CFG_NUM_REO_DEST_RING

/*
 * struct dp_rx_tm_handle_cmn - Opaque handle for rx_threads to store
 * rx_tm_handle. This handle will be common for all the threads.
 * Individual threads should not be accessing
 * elements from dp_rx_tm_handle. It should be via an API.
 */
struct dp_rx_tm_handle_cmn;

/**
 * struct dp_rx_thread_stats - structure holding stats for DP RX thread
 * @nbuf_queued: packets queued into the thread per reo ring
 * @nbuf_queued_total: packets queued into the thread for all reo rings
 * @nbuf_dequeued: packets de-queued from the thread
 * @nbuf_sent_to_stack: packets sent to the stack. some dequeued packets may be
 *			dropped due to no peer or vdev, hence this stat.
 * @gro_flushes: number of GRO flushes
 * @gro_flushes_by_vdev_del: number of GRO flushes triggered by vdev del.
 * @nbufq_max_len: maximum number of nbuf_lists queued for the thread
 * @dropped_invalid_vdev: packets(nbuf_list) dropped due to no vdev
 * @rx_flushed: packets flushed after vdev delete
 * @dropped_invalid_peer: packets(nbuf_list) dropped due to no peer
 * @dropped_invalid_os_rx_handles: packets(nbuf_list) dropped due to no os rx
 * handles
 * @dropped_others: packets dropped due to other reasons
 * @dropped_enq_fail: packets dropped due to pending queue full
 * @rx_nbufq_loop_yield: rx loop yield counter
 */
struct dp_rx_thread_stats {
	unsigned int nbuf_queued[DP_RX_TM_MAX_REO_RINGS];
	unsigned int nbuf_queued_total;
	unsigned int nbuf_dequeued;
	unsigned int nbuf_sent_to_stack;
	unsigned int gro_flushes;
	unsigned int gro_flushes_by_vdev_del;
	unsigned int nbufq_max_len;
	unsigned int dropped_invalid_vdev;
	unsigned int rx_flushed;
	unsigned int dropped_invalid_peer;
	unsigned int dropped_invalid_os_rx_handles;
	unsigned int dropped_others;
	unsigned int dropped_enq_fail;
	unsigned int rx_nbufq_loop_yield;
};

/**
 * enum dp_rx_refill_thread_state - enum to keep track of rx refill thread state
 * @DP_RX_REFILL_THREAD_INVALID: initial invalid state
 * @DP_RX_REFILL_THREAD_RUNNING: rx refill thread functional(NOT suspended,
 *                      processing packets or waiting on a wait_queue)
 * @DP_RX_REFILL_THREAD_SUSPENDING: rx refill thread is suspending
 * @DP_RX_REFILL_THREAD_SUSPENDED: rx refill_thread suspended
 */
enum dp_rx_refill_thread_state {
	DP_RX_REFILL_THREAD_INVALID,
	DP_RX_REFILL_THREAD_RUNNING,
	DP_RX_REFILL_THREAD_SUSPENDING,
	DP_RX_REFILL_THREAD_SUSPENDED
};

/**
 * struct dp_rx_thread - structure holding variables for a single DP RX thread
 * @id: id of the dp_rx_thread (0 or 1 or 2..DP_MAX_RX_THREADS - 1)
 * @task: task structure corresponding to the thread
 * @start_event: handle of Event for DP Rx thread to signal startup
 * @suspend_event: handle of Event for DP Rx thread to signal suspend
 * @resume_event: handle of Event for DP Rx thread to signal resume
 * @shutdown_event: handle of Event for DP Rx thread to signal shutdown
 * @vdev_del_event: handle of Event for vdev del thread to signal completion
 *		    for gro flush
 * @gro_flush_ind: gro flush indication for DP Rx thread
 * @event_flag: event flag to post events to DP Rx thread
 * @nbuf_queue:nbuf queue used to store RX packets
 * @nbufq_len: length of the nbuf queue
 * @aff_mask: cuurent affinity mask of the DP Rx thread
 * @stats: per thread stats
 * @rtm_handle_cmn: abstract RX TM handle. This allows access to the dp_rx_tm
 *		    structures via APIs.
 * @napi: napi to deliver packet to stack via GRO
 * @wait_q: wait queue to conditionally wait on events for DP Rx thread
 * @netdev: dummy netdev to initialize the napi structure with
 */
struct dp_rx_thread {
	uint8_t id;
	qdf_thread_t *task;
	qdf_event_t start_event;
	qdf_event_t suspend_event;
	qdf_event_t resume_event;
	qdf_event_t shutdown_event;
	qdf_event_t vdev_del_event;
	qdf_atomic_t gro_flush_ind;
	unsigned long event_flag;
	qdf_nbuf_queue_head_t nbuf_queue;
	unsigned long aff_mask;
	struct dp_rx_thread_stats stats;
	struct dp_rx_tm_handle_cmn *rtm_handle_cmn;
	qdf_napi_struct napi;
	qdf_wait_queue_head_t wait_q;
	qdf_dummy_netdev_t netdev;
};

/**
 * struct dp_rx_refill_thread - structure holding info of DP Rx refill thread
 * @task: task structure corresponding to the thread
 * @start_event: handle of Event for DP Rx refill thread to signal startup
 * @suspend_event: handle of Event for DP Rx refill thread to signal suspend
 * @resume_event: handle of Event for DP Rx refill thread to signal resume
 * @shutdown_event: handle of Event for DP Rx refill thread to signal shutdown
 * @event_flag: event flag to post events to DP Rx refill thread
 * @wait_q: wait queue to conditionally wait on events for DP Rx refill thread
 * @enabled: flag to check whether DP Rx refill thread is enabled
 * @soc: abstract DP soc reference used in internal API's
 * @state: state of DP Rx refill thread
 */
struct dp_rx_refill_thread {
	qdf_thread_t *task;
	qdf_event_t start_event;
	qdf_event_t suspend_event;
	qdf_event_t resume_event;
	qdf_event_t shutdown_event;
	unsigned long event_flag;
	qdf_wait_queue_head_t wait_q;
	bool enabled;
	void *soc;
	enum dp_rx_refill_thread_state state;
};

/**
 * enum dp_rx_thread_state - enum to keep track of the state of the rx threads
 * @DP_RX_THREADS_INVALID: initial invalid state
 * @DP_RX_THREADS_RUNNING: rx threads functional(NOT suspended, processing
 *			  packets or waiting on a wait_queue)
 * @DP_RX_THREADS_SUSPENDING: rx thread is suspending
 * @DP_RX_THREADS_SUSPENDED: rx_threads suspended from cfg8011 suspend
 */
enum dp_rx_thread_state {
	DP_RX_THREADS_INVALID,
	DP_RX_THREADS_RUNNING,
	DP_RX_THREADS_SUSPENDING,
	DP_RX_THREADS_SUSPENDED
};

/**
 * struct dp_rx_tm_handle - DP RX thread infrastructure handle
 * @num_dp_rx_threads: number of DP RX threads initialized
 * @txrx_handle_cmn: opaque txrx handle to get to pdev and soc
 * @state: state of the rx_threads. All of them should be in the same state.
 * @rx_thread: array of pointers of type struct dp_rx_thread
 * @allow_dropping: flag to indicate frame dropping is enabled
 */
struct dp_rx_tm_handle {
	uint8_t num_dp_rx_threads;
	struct dp_txrx_handle_cmn *txrx_handle_cmn;
	enum dp_rx_thread_state state;
	struct dp_rx_thread **rx_thread;
	qdf_atomic_t allow_dropping;
};

/**
 * enum dp_rx_gro_flush_code - enum differentiate different GRO flushes
 * @DP_RX_GRO_NOT_FLUSH: not fush indication
 * @DP_RX_GRO_NORMAL_FLUSH: Regular full flush
 * @DP_RX_GRO_LOW_TPUT_FLUSH: Flush during low tput level
 */
enum dp_rx_gro_flush_code {
	DP_RX_GRO_NOT_FLUSH = 0,
	DP_RX_GRO_NORMAL_FLUSH,
	DP_RX_GRO_LOW_TPUT_FLUSH
};

/**
 * struct dp_txrx_config - dp txrx configuration passed to dp txrx modules
 * @enable_rx_threads: DP rx threads or not
 */
struct dp_txrx_config {
	bool enable_rx_threads;
};

struct dp_txrx_handle_cmn;

/**
 * struct dp_txrx_handle - main dp txrx container handle
 * @pdev: cdp_pdev pdev handle
 * @soc: ol_txrx_soc_handle soc handle
 * @refill_thread: rx refill thread infra handle
 * @rx_tm_hdl: rx thread infrastructure handle
 * @config: configuration for DP TXRX modules
 */
struct dp_txrx_handle {
	ol_txrx_soc_handle soc;
	struct cdp_pdev *pdev;
	struct dp_rx_tm_handle rx_tm_hdl;
	struct dp_rx_refill_thread refill_thread;
	struct dp_txrx_config config;
};

/**
 * dp_rx_refill_thread_init() - Initialize DP Rx refill threads
 * @refill_thread: Contains over all rx refill thread info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_refill_thread_init(struct dp_rx_refill_thread *refill_thread);

/**
 * dp_rx_refill_thread_deinit() - De-initialize DP Rx refill threads
 * @refill_thread: Contains over all rx refill thread info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_rx_refill_thread_deinit(struct dp_rx_refill_thread *refill_thread);

/**
 * dp_rx_tm_init() - initialize DP Rx thread infrastructure
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread infrastructure
 * @num_dp_rx_threads: number of DP Rx threads to be initialized
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS dp_rx_tm_init(struct dp_rx_tm_handle *rx_tm_hdl,
			 uint8_t num_dp_rx_threads);

/**
 * dp_rx_tm_deinit() - de-initialize DP Rx thread infrastructure
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread infrastructure
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_rx_tm_deinit(struct dp_rx_tm_handle *rx_tm_hdl);

/**
 * dp_rx_tm_enqueue_pkt() - enqueue RX packet into RXTI
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread infrastructure
 * @nbuf_list: single or a list of nbufs to be enqueued into RXTI
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS dp_rx_tm_enqueue_pkt(struct dp_rx_tm_handle *rx_tm_hdl,
				qdf_nbuf_t nbuf_list);

/**
 * dp_rx_tm_gro_flush_ind() - flush GRO packets for a RX Context Id
 * @rx_tm_handle: dp_rx_tm_handle containing the overall thread infrastructure
 * @rx_ctx_id: RX Thread Context Id for which GRO flush needs to be done
 * @flush_code: flush code to differentiate low TPUT flush
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS dp_rx_tm_gro_flush_ind(struct dp_rx_tm_handle *rx_tm_handle,
				  int rx_ctx_id,
				  enum dp_rx_gro_flush_code flush_code);
/**
 * dp_rx_refill_thread_suspend() - Suspend RX refill thread
 * @refill_thread: pointer to dp_rx_refill_thread object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS
dp_rx_refill_thread_suspend(struct dp_rx_refill_thread *refill_thread);

/**
 * dp_rx_tm_suspend() - suspend all threads in RXTI
 * @rx_tm_handle: pointer to dp_rx_tm_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_rx_tm_suspend(struct dp_rx_tm_handle *rx_tm_handle);

/**
 * dp_rx_tm_flush_by_vdev_id() - flush rx packets by vdev_id in all
 * rx thread queues
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *             infrastructure
 * @vdev_id: vdev id for which packets are to be flushed
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS dp_rx_tm_flush_by_vdev_id(struct dp_rx_tm_handle *rx_tm_hdl,
				     uint8_t vdev_id);

/**
 * dp_rx_refill_thread_resume() - Resume RX refill thread
 * @refill_thread: pointer to dp_rx_refill_thread
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS
dp_rx_refill_thread_resume(struct dp_rx_refill_thread *refill_thread);

/**
 * dp_rx_tm_resume() - resume all threads in RXTI
 * @rx_tm_handle: pointer to dp_rx_tm_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_rx_tm_resume(struct dp_rx_tm_handle *rx_tm_handle);

/**
 * dp_rx_tm_dump_stats() - dump stats for all threads in RXTI
 * @rx_tm_handle: pointer to dp_rx_tm_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_rx_tm_dump_stats(struct dp_rx_tm_handle *rx_tm_handle);

/**
 * dp_rx_thread_get_txrx_handle() - get txrx handle from rx_tm_handle_cmn
 * @rx_tm_handle_cmn: opaque pointer to dp_rx_tm_handle_cmn struct
 *
 * Return: pointer to dp_txrx_handle_cmn handle
 */
static inline struct dp_txrx_handle_cmn*
dp_rx_thread_get_txrx_handle(struct dp_rx_tm_handle_cmn *rx_tm_handle_cmn)
{
	return (((struct dp_rx_tm_handle *)rx_tm_handle_cmn)->txrx_handle_cmn);
}

/**
 * dp_rx_tm_get_napi_context() - get NAPI context for a RX CTX ID
 * @rx_ctx_id: RX context ID (RX thread ID) corresponding to which NAPI is
 *             needed
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *             infrastructure
 *
 * Return: NULL on failure, else pointer to NAPI corresponding to rx_ctx_id
 */
qdf_napi_struct *dp_rx_tm_get_napi_context(struct dp_rx_tm_handle *rx_tm_hdl,
					   uint8_t rx_ctx_id);

/**
 * dp_rx_tm_set_cpu_mask() - set CPU mask for RX threads
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *             infrastructure
 * @new_mask: New CPU mask pointer
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_rx_tm_set_cpu_mask(struct dp_rx_tm_handle *rx_tm_hdl,
				 qdf_cpu_mask *new_mask);

#ifdef FEATURE_WLAN_DP_RX_THREADS
/**
 * dp_txrx_get_cmn_hdl_frm_ext_hdl() - conversion func ext_hdl->txrx_handle_cmn
 * @dp_ext_hdl: pointer to dp_txrx_handle structure
 *
 * Return: typecasted pointer of type - struct dp_txrx_handle_cmn
 */
static inline struct dp_txrx_handle_cmn *
dp_txrx_get_cmn_hdl_frm_ext_hdl(struct dp_txrx_handle *dp_ext_hdl)
{
	return (struct dp_txrx_handle_cmn *)dp_ext_hdl;
}

/**
 * dp_txrx_get_ext_hdl_frm_cmn_hdl() - conversion func txrx_handle_cmn->ext_hdl
 * @txrx_cmn_hdl: pointer to dp_txrx_handle_cmn structure
 *
 * Return: typecasted pointer of type - struct dp_txrx_handle
 */
static inline struct dp_txrx_handle *
dp_txrx_get_ext_hdl_frm_cmn_hdl(struct dp_txrx_handle_cmn *txrx_cmn_hdl)
{
	return (struct dp_txrx_handle *)txrx_cmn_hdl;
}

static inline ol_txrx_soc_handle
dp_txrx_get_soc_from_ext_handle(struct dp_txrx_handle_cmn *txrx_cmn_hdl)
{
	struct dp_txrx_handle *dp_ext_hdl;

	dp_ext_hdl = dp_txrx_get_ext_hdl_frm_cmn_hdl(txrx_cmn_hdl);

	return dp_ext_hdl->soc;
}

/**
 * dp_txrx_init() - initialize DP TXRX module
 * @soc: ol_txrx_soc_handle
 * @pdev_id: id of dp pdev handle
 * @config: configuration for DP TXRX modules
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			struct dp_txrx_config *config);

/**
 * dp_txrx_deinit() - de-initialize DP TXRX module
 * @soc: ol_txrx_soc_handle
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_txrx_deinit(ol_txrx_soc_handle soc);

/**
 * dp_txrx_flush_pkts_by_vdev_id() - flush rx packets for a vdev_id
 * @soc: ol_txrx_soc_handle object
 * @vdev_id: vdev_id for which rx packets are to be flushed
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_flush_pkts_by_vdev_id(ol_txrx_soc_handle soc,
						       uint8_t vdev_id)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_flush_by_vdev_id(&dp_ext_hdl->rx_tm_hdl, vdev_id);
ret:
	return qdf_status;
}

/**
 * dp_txrx_resume() - resume all threads
 * @soc: ol_txrx_soc_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_resume(ol_txrx_soc_handle soc)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_rx_refill_thread *refill_thread;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	refill_thread = &dp_ext_hdl->refill_thread;
	if (refill_thread->enabled) {
		qdf_status = dp_rx_refill_thread_resume(refill_thread);
		if (qdf_status != QDF_STATUS_SUCCESS)
			return qdf_status;
	}

	qdf_status = dp_rx_tm_resume(&dp_ext_hdl->rx_tm_hdl);
ret:
	return qdf_status;
}

/**
 * dp_txrx_suspend() - suspend all threads
 * @soc: ol_txrx_soc_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_suspend(ol_txrx_soc_handle soc)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_rx_refill_thread *refill_thread;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	refill_thread = &dp_ext_hdl->refill_thread;
	if (refill_thread->enabled) {
		qdf_status = dp_rx_refill_thread_suspend(refill_thread);
		if (qdf_status != QDF_STATUS_SUCCESS)
			return qdf_status;
	}

	qdf_status = dp_rx_tm_suspend(&dp_ext_hdl->rx_tm_hdl);
	if (QDF_IS_STATUS_ERROR(qdf_status) && refill_thread->enabled)
		dp_rx_refill_thread_resume(refill_thread);

ret:
	return qdf_status;
}

/**
 * dp_rx_enqueue_pkt() - enqueue packet(s) into the thread
 * @soc: ol_txrx_soc_handle object
 * @nbuf_list: list of packets to be queued into the rx_thread
 *
 * The function accepts a list of skbs connected by the skb->next pointer and
 * queues them into a RX thread to be sent to the stack.
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline
QDF_STATUS dp_rx_enqueue_pkt(ol_txrx_soc_handle soc, qdf_nbuf_t nbuf_list)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc || !nbuf_list) {
		qdf_status = QDF_STATUS_E_INVAL;
		dp_err("invalid input params soc %pK nbuf %pK"
		       , soc, nbuf_list);
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_enqueue_pkt(&dp_ext_hdl->rx_tm_hdl, nbuf_list);
ret:
	return qdf_status;
}

/**
 * dp_rx_gro_flush_ind() - Flush GRO packets for a given RX CTX Id
 * @soc: ol_txrx_soc_handle object
 * @rx_ctx_id: Context Id (Thread for which GRO packets need to be flushed)
 * @flush_code: flush_code differentiating normal_flush from low_tput_flush
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline
QDF_STATUS dp_rx_gro_flush_ind(ol_txrx_soc_handle soc, int rx_ctx_id,
			       enum dp_rx_gro_flush_code flush_code)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		dp_err("invalid input param soc %pK", soc);
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_gro_flush_ind(&dp_ext_hdl->rx_tm_hdl, rx_ctx_id,
					    flush_code);
ret:
	return qdf_status;
}

/**
 * dp_txrx_ext_dump_stats() - dump txrx external module stats
 * @soc: ol_txrx_soc_handle object
 * @stats_id: id  for the module whose stats are needed
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_ext_dump_stats(ol_txrx_soc_handle soc,
						uint8_t stats_id)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status;

	if (!soc) {
		dp_err("invalid input params soc %pK", soc);
		return QDF_STATUS_E_INVAL;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl)
		return QDF_STATUS_E_FAULT;

	if (stats_id == CDP_DP_RX_THREAD_STATS)
		qdf_status = dp_rx_tm_dump_stats(&dp_ext_hdl->rx_tm_hdl);
	else
		qdf_status = QDF_STATUS_E_INVAL;

	return qdf_status;
}

/**
 * dp_rx_get_napi_context() - get NAPI context for a RX CTX ID
 * @soc: ol_txrx_soc_handle object
 * @rx_ctx_id: RX context ID (RX thread ID) corresponding to which NAPI is
 *             needed
 *
 * Return: NULL on failure, else pointer to NAPI corresponding to rx_ctx_id
 */
static inline
qdf_napi_struct *dp_rx_get_napi_context(ol_txrx_soc_handle soc,
					uint8_t rx_ctx_id)
{
	struct dp_txrx_handle *dp_ext_hdl;

	if (!soc) {
		dp_err("soc in NULL!");
		return NULL;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		dp_err("dp_ext_hdl in NULL!");
		return NULL;
	}

	return dp_rx_tm_get_napi_context(&dp_ext_hdl->rx_tm_hdl, rx_ctx_id);
}

/**
 * dp_txrx_set_cpu_mask() - set CPU mask for RX threads
 * @soc: ol_txrx_soc_handle object
 * @new_mask: New CPU mask pointer
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline
QDF_STATUS dp_txrx_set_cpu_mask(ol_txrx_soc_handle soc, qdf_cpu_mask *new_mask)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_set_cpu_mask(&dp_ext_hdl->rx_tm_hdl, new_mask);

ret:
	return qdf_status;
}

#else

static inline
QDF_STATUS dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			struct dp_txrx_config *config)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_deinit(ol_txrx_soc_handle soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_flush_pkts_by_vdev_id(ol_txrx_soc_handle soc,
						       uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_resume(ol_txrx_soc_handle soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_suspend(ol_txrx_soc_handle soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_enqueue_pkt(ol_txrx_soc_handle soc, qdf_nbuf_t nbuf_list)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_gro_flush_ind(ol_txrx_soc_handle soc, int rx_ctx_id,
			       enum dp_rx_gro_flush_code flush_code)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_ext_dump_stats(ol_txrx_soc_handle soc,
						uint8_t stats_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
qdf_napi_struct *dp_rx_get_napi_context(ol_txrx_soc_handle soc,
					uint8_t rx_ctx_id)
{
	return NULL;
}

static inline
QDF_STATUS dp_txrx_set_cpu_mask(ol_txrx_soc_handle soc, qdf_cpu_mask *new_mask)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* FEATURE_WLAN_DP_RX_THREADS */

/**
 * dp_rx_tm_get_pending() - get number of frame in thread
 * nbuf queue pending
 * @soc: ol_txrx_soc_handle object
 *
 * Return: number of frames
 */
int dp_rx_tm_get_pending(ol_txrx_soc_handle soc);
#endif /* __WLAN_DP_RX_THREAD_H */

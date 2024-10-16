/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _DP_UMAC_RESET_H_
#define _DP_UMAC_RESET_H_

#include <qdf_types.h>
struct dp_soc;

/**
 * enum umac_reset_action - Actions supported by the UMAC reset
 * @UMAC_RESET_ACTION_DO_PRE_RESET: DO_PRE_RESET
 * @UMAC_RESET_ACTION_DO_POST_RESET_START: DO_POST_RESET_START
 * @UMAC_RESET_ACTION_DO_POST_RESET_COMPLETE: DO_POST_RESET_COMPLETE
 * @UMAC_RESET_ACTION_MAX: Maximum actions
 */
enum umac_reset_action {
	UMAC_RESET_ACTION_DO_PRE_RESET = 0,
	UMAC_RESET_ACTION_DO_POST_RESET_START = 1,
	UMAC_RESET_ACTION_DO_POST_RESET_COMPLETE = 2,
	UMAC_RESET_ACTION_MAX
};

#ifdef DP_UMAC_HW_RESET_SUPPORT

#define dp_umac_reset_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_DP_UMAC_RESET, params)
#define dp_umac_reset_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_DP_UMAC_RESET, params)
#define dp_umac_reset_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_DP_UMAC_RESET, params)
#define dp_umac_reset_notice(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_DP_UMAC_RESET, params)
#define dp_umac_reset_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_DP_UMAC_RESET, params)
#define dp_umac_reset_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_DP_UMAC_RESET, params)

#define DP_UMAC_RESET_SHMEM_ALIGN 8
#define DP_UMAC_RESET_SHMEM_MAGIC_NUM (0xDEADBEEF)

/**
 * enum umac_reset_state - States required by the UMAC reset state machine
 * @UMAC_RESET_STATE_WAIT_FOR_DO_PRE_RESET: Waiting for the DO_PRE_RESET event
 * @UMAC_RESET_STATE_DO_PRE_RESET_RECEIVED: Received the DO_PRE_RESET event
 * @UMAC_RESET_STATE_HOST_PRE_RESET_DONE: Host has completed handling the
 * PRE_RESET event
 * @UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_START: Waiting for the
 * DO_POST_RESET_START event
 * @UMAC_RESET_STATE_DO_POST_RESET_START_RECEIVED: Received the
 * DO_POST_RESET_START event
 * @UMAC_RESET_STATE_HOST_POST_RESET_START_DONE: Host has completed handling the
 * POST_RESET_START event
 * @UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_COMPLETE: Waiting for the
 * DO_POST_RESET_COMPLETE event
 * @UMAC_RESET_STATE_DO_POST_RESET_COMPLETE_RECEIVED: Received the
 * DO_POST_RESET_COMPLETE event
 * @UMAC_RESET_STATE_HOST_POST_RESET_COMPLETE_DONE: Host has completed handling
 * the DO_POST_RESET_COMPLETE event
 */
enum umac_reset_state {
	UMAC_RESET_STATE_WAIT_FOR_DO_PRE_RESET = 0,
	UMAC_RESET_STATE_DO_PRE_RESET_RECEIVED,
	UMAC_RESET_STATE_HOST_PRE_RESET_DONE,

	UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_START,
	UMAC_RESET_STATE_DO_POST_RESET_START_RECEIVED,
	UMAC_RESET_STATE_HOST_POST_RESET_START_DONE,

	UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_COMPLETE,
	UMAC_RESET_STATE_DO_POST_RESET_COMPLETE_RECEIVED,
	UMAC_RESET_STATE_HOST_POST_RESET_COMPLETE_DONE,
};

/**
 * enum umac_reset_rx_event - Rx events deduced by the UMAC reset
 * @UMAC_RESET_RX_EVENT_NONE: No event
 * @UMAC_RESET_RX_EVENT_DO_PRE_RESET: DO_PRE_RESET event
 * @UMAC_RESET_RX_EVENT_DO_POST_RESET_START: DO_POST_RESET_START event
 * @UMAC_RESET_RX_EVENT_DO_POST_RESET_COMPELTE: DO_POST_RESET_COMPELTE event
 * @UMAC_RESET_RX_EVENT_ERROR: Error while processing the Rx event
 */
enum umac_reset_rx_event {
	UMAC_RESET_RX_EVENT_NONE = 0x0,
	UMAC_RESET_RX_EVENT_DO_PRE_RESET = 0x1,
	UMAC_RESET_RX_EVENT_DO_POST_RESET_START = 0x2,
	UMAC_RESET_RX_EVENT_DO_POST_RESET_COMPELTE = 0x4,

	UMAC_RESET_RX_EVENT_ERROR = 0xFFFFFFFF,
};

/**
 * enum umac_reset_tx_cmd: UMAC reset Tx command
 * @UMAC_RESET_TX_CMD_PRE_RESET_DONE: PRE_RESET_DONE
 * @UMAC_RESET_TX_CMD_POST_RESET_START_DONE: POST_RESET_START_DONE
 * @UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE: POST_RESET_COMPLETE_DONE
 */
enum umac_reset_tx_cmd {
	UMAC_RESET_TX_CMD_PRE_RESET_DONE,
	UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
	UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE,
};

/**
 * struct umac_reset_rx_actions - callbacks for handling UMAC reset actions
 * @cb: Array of pointers where each pointer contains callback for each UMAC
 * reset action for that index
 */
struct umac_reset_rx_actions {
	QDF_STATUS (*cb[UMAC_RESET_ACTION_MAX])(struct dp_soc *soc);
};

/**
 * struct reset_ts - timestamps of for umac reset events for debug
 * @pre_reset_start: Umac prereset start event timestamp
 * @pre_reset_done: Umac prereset done timestamp
 * @post_reset_start: Umac postreset start event timestamp
 * @post_reset_done: Umac postreset done timestamp
 * @post_reset_complete_start: Umac postreset complete event timestamp
 * @post_reset_complete_done: Umac postreset complete done timestamp
 */
struct reset_ts {
	uint64_t pre_reset_start;
	uint64_t pre_reset_done;
	uint64_t post_reset_start;
	uint64_t post_reset_done;
	uint64_t post_reset_complete_start;
	uint64_t post_reset_complete_done;
};

/**
 * struct dp_soc_umac_reset_ctx - UMAC reset context at soc level
 * @shmem_paddr_unaligned: Physical address of the shared memory (unaligned)
 * @shmem_vaddr_unaligned: Virtual address of the shared memory (unaligned)
 * @shmem_paddr_aligned: Physical address of the shared memory (aligned)
 * @shmem_vaddr_aligned: Virtual address of the shared memory (aligned)
 * @shmem_size: Size of the shared memory
 * @intr_offset: Offset of the UMAC reset interrupt w.r.t DP base interrupt
 * @current_state: current state of the UMAC reset state machine
 * @shmem_exp_magic_num: Expected magic number in the shared memory
 * @rx_actions: callbacks for handling UMAC reset actions
 * @intr_ctx_bkp: DP Interrupts ring masks backup
 * @nbuf_list: skb list for delayed free
 * @skel_enable: Enable skeleton code for umac reset
 * @ts: timestamps debug
 */
struct dp_soc_umac_reset_ctx {
	qdf_dma_addr_t shmem_paddr_unaligned;
	void *shmem_vaddr_unaligned;
	qdf_dma_addr_t shmem_paddr_aligned;
	htt_umac_hang_recovery_msg_shmem_t *shmem_vaddr_aligned;
	size_t shmem_size;
	int intr_offset;
	enum umac_reset_state current_state;
	uint32_t shmem_exp_magic_num;
	struct umac_reset_rx_actions rx_actions;
	struct dp_intr_bkp *intr_ctx_bkp;
	qdf_nbuf_t nbuf_list;
	bool skel_enable;
	struct reset_ts ts;
};

/**
 * dp_soc_umac_reset_init() - Initialize UMAC reset context
 * @soc: DP soc object
 *
 * Return: QDF status of operation
 */
QDF_STATUS dp_soc_umac_reset_init(struct dp_soc *soc);

/**
 * dp_soc_umac_reset_deinit() - De-initialize UMAC reset context
 * @txrx_soc: DP soc object
 *
 * Return: QDF status of operation
 */
QDF_STATUS dp_soc_umac_reset_deinit(struct cdp_soc_t *txrx_soc);

/**
 * dp_umac_reset_interrupt_attach() - Register handlers for UMAC reset interrupt
 * @soc: DP soc object
 *
 * Return: QDF status of operation
 */
QDF_STATUS dp_umac_reset_interrupt_attach(struct dp_soc *soc);

/**
 * dp_umac_reset_interrupt_detach() - Unregister UMAC reset interrupt handlers
 * @soc: DP soc object
 *
 * Return: QDF status of operation
 */
QDF_STATUS dp_umac_reset_interrupt_detach(struct dp_soc *soc);

/**
 * dp_umac_reset_register_rx_action_callback() - Register a callback for a given
 * UMAC reset action
 * @soc: DP soc object
 * @handler: callback handler to be registered
 * @action: UMAC reset action for which @handler needs to be registered
 *
 * Return: QDF status of operation
 */
QDF_STATUS dp_umac_reset_register_rx_action_callback(
			struct dp_soc *soc,
			QDF_STATUS (*handler)(struct dp_soc *soc),
			enum umac_reset_action action);

/**
 * dp_umac_reset_notify_action_completion() - Notify that a given action has
 * been completed
 * @soc: DP soc object
 * @action: UMAC reset action that got completed
 *
 * Return: QDF status of operation
 */
QDF_STATUS dp_umac_reset_notify_action_completion(
			struct dp_soc *soc,
			enum umac_reset_action action);
#else
static inline
QDF_STATUS dp_soc_umac_reset_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_soc_umac_reset_deinit(struct cdp_soc_t *txrx_soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_umac_reset_register_rx_action_callback(
			struct dp_soc *soc,
			QDF_STATUS (*handler)(struct dp_soc *soc),
			enum umac_reset_action action)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_umac_reset_notify_action_completion(
		struct dp_soc *soc,
		enum umac_reset_action action)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* DP_UMAC_HW_RESET_SUPPORT */
#endif /* _DP_UMAC_RESET_H_ */

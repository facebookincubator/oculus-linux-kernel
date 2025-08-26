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
#include <dp_types.h>
#include <wlan_cfg.h>
#include <hif.h>
#include <dp_htt.h>

/**
 * dp_get_umac_reset_intr_ctx() - Get the interrupt context to be used by
 * UMAC reset feature
 * @soc: DP soc object
 * @intr_ctx: Interrupt context variable to be populated by this API
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS dp_get_umac_reset_intr_ctx(struct dp_soc *soc, int *intr_ctx)
{
	int umac_reset_mask, i;

	/**
	 * Go over all the contexts and check which interrupt context has
	 * the UMAC reset mask set.
	 */
	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		umac_reset_mask = wlan_cfg_get_umac_reset_intr_mask(
					soc->wlan_cfg_ctx, i);

		if (umac_reset_mask) {
			*intr_ctx = i;
			return QDF_STATUS_SUCCESS;
		}
	}

	*intr_ctx = -1;
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_umac_reset_send_setup_cmd(): Send the UMAC reset setup command
 * @soc: dp soc object
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS
dp_umac_reset_send_setup_cmd(struct dp_soc *soc)
{
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;
	int msi_vector_count, ret;
	uint32_t msi_base_data, msi_vector_start;
	struct dp_htt_umac_reset_setup_cmd_params params;

	umac_reset_ctx = &soc->umac_reset_ctx;
	ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
					  &msi_vector_count, &msi_base_data,
					  &msi_vector_start);
	if (ret)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_zero(&params, sizeof(params));
	params.msi_data = (umac_reset_ctx->intr_offset % msi_vector_count) +
				msi_base_data;
	params.shmem_addr_low =
		qdf_get_lower_32_bits(umac_reset_ctx->shmem_paddr_aligned);
	params.shmem_addr_high =
		qdf_get_upper_32_bits(umac_reset_ctx->shmem_paddr_aligned);

	return dp_htt_umac_reset_send_setup_cmd(soc, &params);
}

QDF_STATUS dp_soc_umac_reset_init(struct dp_soc *soc)
{
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;
	size_t alloc_size;
	QDF_STATUS status;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!soc->features.umac_hw_reset_support) {
		dp_umac_reset_info("Target doesn't support the UMAC HW reset feature");
		return QDF_STATUS_E_NOSUPPORT;
	}

	umac_reset_ctx = &soc->umac_reset_ctx;
	qdf_mem_zero(umac_reset_ctx, sizeof(*umac_reset_ctx));

	umac_reset_ctx->current_state = UMAC_RESET_STATE_WAIT_FOR_DO_PRE_RESET;
	umac_reset_ctx->shmem_exp_magic_num = DP_UMAC_RESET_SHMEM_MAGIC_NUM;

	status = dp_get_umac_reset_intr_ctx(soc, &umac_reset_ctx->intr_offset);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_umac_reset_err("No interrupt assignment");
		return status;
	}

	alloc_size = sizeof(htt_umac_hang_recovery_msg_shmem_t) +
			DP_UMAC_RESET_SHMEM_ALIGN - 1;
	umac_reset_ctx->shmem_vaddr_unaligned =
	    qdf_mem_alloc_consistent(soc->osdev, soc->osdev->dev,
				     alloc_size,
				     &umac_reset_ctx->shmem_paddr_unaligned);
	if (!umac_reset_ctx->shmem_vaddr_unaligned) {
		dp_umac_reset_err("shmem allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	umac_reset_ctx->shmem_vaddr_aligned = (void *)(uintptr_t)qdf_roundup(
		(uint64_t)(uintptr_t)umac_reset_ctx->shmem_vaddr_unaligned,
		DP_UMAC_RESET_SHMEM_ALIGN);
	umac_reset_ctx->shmem_paddr_aligned = qdf_roundup(
		(uint64_t)umac_reset_ctx->shmem_paddr_unaligned,
		DP_UMAC_RESET_SHMEM_ALIGN);
	umac_reset_ctx->shmem_size = alloc_size;

	/* Write the magic number to the shared memory */
	umac_reset_ctx->shmem_vaddr_aligned->magic_num =
		DP_UMAC_RESET_SHMEM_MAGIC_NUM;

	/* Attach the interrupts */
	status = dp_umac_reset_interrupt_attach(soc);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_umac_reset_err("Interrupt attach failed");
		qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
					umac_reset_ctx->shmem_size,
					umac_reset_ctx->shmem_vaddr_unaligned,
					umac_reset_ctx->shmem_paddr_unaligned,
					0);
		return status;
	}

	/* Send the setup cmd to the target */
	return dp_umac_reset_send_setup_cmd(soc);
}

QDF_STATUS dp_soc_umac_reset_deinit(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!soc->features.umac_hw_reset_support) {
		dp_umac_reset_info("Target doesn't support the UMAC HW reset feature");
		return QDF_STATUS_E_NOSUPPORT;
	}

	dp_umac_reset_interrupt_detach(soc);

	umac_reset_ctx = &soc->umac_reset_ctx;
	qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
				umac_reset_ctx->shmem_size,
				umac_reset_ctx->shmem_vaddr_unaligned,
				umac_reset_ctx->shmem_paddr_unaligned,
				0);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_get_rx_event() - Extract the Rx event from the shared memory
 * @umac_reset_ctx: UMAC reset context
 *
 * Return: Extracted Rx event in the form of enumeration umac_reset_rx_event
 */
static enum umac_reset_rx_event
dp_umac_reset_get_rx_event_from_shmem(
	struct dp_soc_umac_reset_ctx *umac_reset_ctx)
{
	htt_umac_hang_recovery_msg_shmem_t *shmem_vaddr;
	uint32_t t2h_msg;
	uint8_t num_events = 0;
	enum umac_reset_rx_event rx_event;

	shmem_vaddr = umac_reset_ctx->shmem_vaddr_aligned;
	if (!shmem_vaddr) {
		dp_umac_reset_err("Shared memory address is NULL");
		goto err;
	}

	if (shmem_vaddr->magic_num != umac_reset_ctx->shmem_exp_magic_num) {
		dp_umac_reset_err("Shared memory got corrupted");
		goto err;
	}

	/* Read the shared memory into a local variable */
	t2h_msg = shmem_vaddr->t2h_msg;

	/* Clear the shared memory right away */
	shmem_vaddr->t2h_msg = 0;

	dp_umac_reset_debug("shmem value - t2h_msg: 0x%x", t2h_msg);

	rx_event = UMAC_RESET_RX_EVENT_NONE;

	if (HTT_UMAC_HANG_RECOVERY_MSG_SHMEM_DO_PRE_RESET_GET(t2h_msg)) {
		rx_event |= UMAC_RESET_RX_EVENT_DO_PRE_RESET;
		num_events++;
	}

	if (HTT_UMAC_HANG_RECOVERY_MSG_SHMEM_DO_POST_RESET_START_GET(t2h_msg)) {
		rx_event |= UMAC_RESET_RX_EVENT_DO_POST_RESET_START;
		num_events++;
	}

	if (HTT_UMAC_HANG_RECOVERY_MSG_SHMEM_DO_POST_RESET_COMPLETE_GET(t2h_msg)) {
		rx_event |= UMAC_RESET_RX_EVENT_DO_POST_RESET_COMPELTE;
		num_events++;
	}

	dp_umac_reset_debug("deduced rx event: 0x%x", rx_event);
	/* There should not be more than 1 event */
	if (num_events > 1) {
		dp_umac_reset_err("Multiple events(0x%x) got posted", rx_event);
		goto err;
	}

	return rx_event;
err:
	qdf_assert_always(0);
	return UMAC_RESET_RX_EVENT_ERROR;
}

/**
 * dp_umac_reset_get_rx_event() - Extract the Rx event
 * @umac_reset_ctx: UMAC reset context
 *
 * Return: Extracted Rx event in the form of enumeration umac_reset_rx_event
 */
static inline enum umac_reset_rx_event
dp_umac_reset_get_rx_event(struct dp_soc_umac_reset_ctx *umac_reset_ctx)
{
	return dp_umac_reset_get_rx_event_from_shmem(umac_reset_ctx);
}

/**
 * dp_umac_reset_validate_n_update_state_machine_on_rx() - Validate the state
 * machine for a given rx event and update the state machine
 * @umac_reset_ctx: UMAC reset context
 * @rx_event: Rx event
 * @current_exp_state: Expected state
 * @next_state: The state to which the state machine needs to be updated
 *
 * Return: QDF_STATUS of operation
 */
static QDF_STATUS
dp_umac_reset_validate_n_update_state_machine_on_rx(
	struct dp_soc_umac_reset_ctx *umac_reset_ctx,
	enum umac_reset_rx_event rx_event,
	enum umac_reset_state current_exp_state,
	enum umac_reset_state next_state)
{
	if (umac_reset_ctx->current_state != current_exp_state) {
		dp_umac_reset_err("state machine validation failed on rx event: %d, current state is %d",
				  rx_event,
				  umac_reset_ctx->current_state);
		qdf_assert_always(0);
		return QDF_STATUS_E_FAILURE;
	}

	/* Update the state */
	umac_reset_ctx->current_state = next_state;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_rx_event_handler() - Main Rx event handler for UMAC reset
 * @dp_ctx: Interrupt context corresponding to UMAC reset
 *
 * Return: 0 incase of success, else failure
 */
static int dp_umac_reset_rx_event_handler(void *dp_ctx)
{
	struct dp_intr *int_ctx = (struct dp_intr *)dp_ctx;
	struct dp_soc *soc = int_ctx->soc;
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;
	enum umac_reset_rx_event rx_event;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	enum umac_reset_action action;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		goto exit;
	}

	umac_reset_ctx = &soc->umac_reset_ctx;

	dp_umac_reset_debug("enter");
	rx_event = dp_umac_reset_get_rx_event(umac_reset_ctx);

	switch (rx_event) {
	case UMAC_RESET_RX_EVENT_NONE:
		/* This interrupt is not meant for us, so exit */
		dp_umac_reset_debug("Not a UMAC reset event");
		status = QDF_STATUS_SUCCESS;
		goto exit;

	case UMAC_RESET_RX_EVENT_DO_PRE_RESET:
		umac_reset_ctx->ts.pre_reset_start =
						qdf_get_log_timestamp_usecs();
		status = dp_umac_reset_validate_n_update_state_machine_on_rx(
			umac_reset_ctx, rx_event,
			UMAC_RESET_STATE_WAIT_FOR_DO_PRE_RESET,
			UMAC_RESET_STATE_DO_PRE_RESET_RECEIVED);

		action = UMAC_RESET_ACTION_DO_PRE_RESET;
		break;

	case UMAC_RESET_RX_EVENT_DO_POST_RESET_START:
		umac_reset_ctx->ts.post_reset_start =
						qdf_get_log_timestamp_usecs();
		status = dp_umac_reset_validate_n_update_state_machine_on_rx(
			umac_reset_ctx, rx_event,
			UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_START,
			UMAC_RESET_STATE_DO_POST_RESET_START_RECEIVED);

		action = UMAC_RESET_ACTION_DO_POST_RESET_START;
		break;

	case UMAC_RESET_RX_EVENT_DO_POST_RESET_COMPELTE:
		umac_reset_ctx->ts.post_reset_complete_start =
						qdf_get_log_timestamp_usecs();
		status = dp_umac_reset_validate_n_update_state_machine_on_rx(
			umac_reset_ctx, rx_event,
			UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_COMPLETE,
			UMAC_RESET_STATE_DO_POST_RESET_COMPLETE_RECEIVED);

		action = UMAC_RESET_ACTION_DO_POST_RESET_COMPLETE;
		break;

	case UMAC_RESET_RX_EVENT_ERROR:
		dp_umac_reset_err("Error Rx event");
		goto exit;

	default:
		dp_umac_reset_err("Invalid value(%u) for Rx event", rx_event);
		goto exit;
	}

	/* Call the handler for this event */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (!umac_reset_ctx->rx_actions.cb[action]) {
			dp_umac_reset_err("rx callback is NULL");
			goto exit;
		}

		status = umac_reset_ctx->rx_actions.cb[action](soc);
	}

exit:
	return qdf_status_to_os_return(status);
}

QDF_STATUS dp_umac_reset_interrupt_attach(struct dp_soc *soc)
{
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;
	int msi_vector_count, ret;
	uint32_t msi_base_data, msi_vector_start;
	uint32_t umac_reset_vector, umac_reset_irq;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!soc->features.umac_hw_reset_support) {
		dp_umac_reset_info("Target doesn't support the UMAC HW reset feature");
		return QDF_STATUS_SUCCESS;
	}

	umac_reset_ctx = &soc->umac_reset_ctx;

	if (pld_get_enable_intx(soc->osdev->dev)) {
		dp_umac_reset_err("UMAC reset is not supported in legacy interrupt mode");
		return QDF_STATUS_E_FAILURE;
	}

	ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
					  &msi_vector_count, &msi_base_data,
					  &msi_vector_start);
	if (ret) {
		dp_umac_reset_err("UMAC reset is only supported in MSI interrupt mode");
		return QDF_STATUS_E_FAILURE;
	}

	if (umac_reset_ctx->intr_offset < 0 ||
	    umac_reset_ctx->intr_offset >= WLAN_CFG_INT_NUM_CONTEXTS) {
		dp_umac_reset_err("Invalid interrupt offset");
		return QDF_STATUS_E_FAILURE;
	}

	umac_reset_vector = msi_vector_start +
			       (umac_reset_ctx->intr_offset % msi_vector_count);

	/* Get IRQ number */
	umac_reset_irq = pld_get_msi_irq(soc->osdev->dev, umac_reset_vector);

	/* Finally register to this IRQ from HIF layer */
	return hif_register_umac_reset_handler(
				soc->hif_handle,
				dp_umac_reset_rx_event_handler,
				&soc->intr_ctx[umac_reset_ctx->intr_offset],
				umac_reset_irq);
}

QDF_STATUS dp_umac_reset_interrupt_detach(struct dp_soc *soc)
{
	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!soc->features.umac_hw_reset_support) {
		dp_umac_reset_info("Target doesn't support the UMAC HW reset feature");
		return QDF_STATUS_SUCCESS;
	}

	return hif_unregister_umac_reset_handler(soc->hif_handle);
}

QDF_STATUS dp_umac_reset_register_rx_action_callback(
			struct dp_soc *soc,
			QDF_STATUS (*handler)(struct dp_soc *soc),
			enum umac_reset_action action)
{
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!soc->features.umac_hw_reset_support) {
		dp_umac_reset_info("Target doesn't support the UMAC HW reset feature");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (action >= UMAC_RESET_ACTION_MAX) {
		dp_umac_reset_err("invalid action: %d", action);
		return QDF_STATUS_E_INVAL;
	}

	umac_reset_ctx = &soc->umac_reset_ctx;

	umac_reset_ctx->rx_actions.cb[action] = handler;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_post_tx_cmd_via_shmem() - Post Tx command using shared memory
 * @umac_reset_ctx: UMAC reset context
 * @tx_cmd: Tx command to be posted
 *
 * Return: QDF status of operation
 */
static QDF_STATUS
dp_umac_reset_post_tx_cmd_via_shmem(
	struct dp_soc_umac_reset_ctx *umac_reset_ctx,
	enum umac_reset_tx_cmd tx_cmd)
{
	htt_umac_hang_recovery_msg_shmem_t *shmem_vaddr;

	shmem_vaddr = umac_reset_ctx->shmem_vaddr_aligned;
	if (!shmem_vaddr) {
		dp_umac_reset_err("Shared memory address is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	switch (tx_cmd) {
	case UMAC_RESET_TX_CMD_PRE_RESET_DONE:
		HTT_UMAC_HANG_RECOVERY_MSG_SHMEM_PRE_RESET_DONE_SET(
			shmem_vaddr->h2t_msg, 1);

		umac_reset_ctx->ts.pre_reset_done =
						qdf_get_log_timestamp_usecs();
		break;

	case UMAC_RESET_TX_CMD_POST_RESET_START_DONE:
		HTT_UMAC_HANG_RECOVERY_MSG_SHMEM_POST_RESET_START_DONE_SET(
			shmem_vaddr->h2t_msg, 1);

		umac_reset_ctx->ts.post_reset_done =
						qdf_get_log_timestamp_usecs();
		break;

	case UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE:
		HTT_UMAC_HANG_RECOVERY_MSG_SHMEM_POST_RESET_COMPLETE_DONE_SET(
			shmem_vaddr->h2t_msg, 1);

		umac_reset_ctx->ts.post_reset_complete_done =
						qdf_get_log_timestamp_usecs();
		break;

	default:
		dp_umac_reset_err("Invalid tx cmd: %d", tx_cmd);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_notify_target() - Notify the target about completion of action.
 * @umac_reset_ctx: UMAC reset context
 *
 * This API figures out the Tx command that needs to be posted based on the
 * current state in the state machine. Also, updates the state machine once the
 * Tx command has been posted.
 *
 * Return: QDF status of operation
 */
static QDF_STATUS
dp_umac_reset_notify_target(struct dp_soc_umac_reset_ctx *umac_reset_ctx)
{
	enum umac_reset_state next_state;
	enum umac_reset_tx_cmd tx_cmd;
	QDF_STATUS status;

	switch (umac_reset_ctx->current_state) {
	case UMAC_RESET_STATE_HOST_PRE_RESET_DONE:
		tx_cmd = UMAC_RESET_TX_CMD_PRE_RESET_DONE;
		next_state = UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_START;
		break;

	case UMAC_RESET_STATE_HOST_POST_RESET_START_DONE:
		tx_cmd = UMAC_RESET_TX_CMD_POST_RESET_START_DONE;
		next_state = UMAC_RESET_STATE_WAIT_FOR_DO_POST_RESET_COMPLETE;
		break;

	case UMAC_RESET_STATE_HOST_POST_RESET_COMPLETE_DONE:
		tx_cmd = UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE;
		next_state = UMAC_RESET_STATE_WAIT_FOR_DO_PRE_RESET;
		break;

	default:
		dp_umac_reset_err("Invalid state(%d) during Tx",
				  umac_reset_ctx->current_state);
		qdf_assert_always(0);
		return QDF_STATUS_E_FAILURE;
	}

	status = dp_umac_reset_post_tx_cmd_via_shmem(umac_reset_ctx, tx_cmd);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_umac_reset_err("Couldn't post Tx cmd");
		qdf_assert_always(0);
		return status;
	}

	/* Update the state machine */
	umac_reset_ctx->current_state = next_state;

	return status;
}

/**
 * dp_umac_reset_notify_completion() - Notify that a given action has been
 * completed
 * @soc: DP soc object
 * @next_state: The state to which the state machine needs to be updated due to
 * this completion
 *
 * Return: QDF status of operation
 */
static QDF_STATUS dp_umac_reset_notify_completion(
		struct dp_soc *soc,
		enum umac_reset_state next_state)
{
	struct dp_soc_umac_reset_ctx *umac_reset_ctx;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	umac_reset_ctx = &soc->umac_reset_ctx;

	/* Update the state first */
	umac_reset_ctx->current_state = next_state;

	return dp_umac_reset_notify_target(umac_reset_ctx);
}

QDF_STATUS dp_umac_reset_notify_action_completion(
		struct dp_soc *soc,
		enum umac_reset_action action)
{
	enum umac_reset_state next_state;

	if (!soc) {
		dp_umac_reset_err("DP SOC is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!soc->features.umac_hw_reset_support) {
		dp_umac_reset_info("Target doesn't support the UMAC HW reset feature");
		return QDF_STATUS_E_NOSUPPORT;
	}

	switch (action) {
	case UMAC_RESET_ACTION_DO_PRE_RESET:
		next_state = UMAC_RESET_STATE_HOST_PRE_RESET_DONE;
		break;

	case UMAC_RESET_ACTION_DO_POST_RESET_START:
		next_state = UMAC_RESET_STATE_HOST_POST_RESET_START_DONE;
		break;

	case UMAC_RESET_ACTION_DO_POST_RESET_COMPLETE:
		next_state = UMAC_RESET_STATE_HOST_POST_RESET_COMPLETE_DONE;
		break;

	default:
		dp_umac_reset_err("Invalid action");
		return QDF_STATUS_E_FAILURE;
	}

	return dp_umac_reset_notify_completion(soc, next_state);
}

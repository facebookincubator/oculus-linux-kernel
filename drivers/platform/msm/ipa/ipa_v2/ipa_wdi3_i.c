/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "ipa_i.h"
#include <linux/ipa_wdi3.h>

#define IPA_HW_WDI3_RX_MBOX_START_INDEX 48
#define IPA_HW_WDI3_TX_MBOX_START_INDEX 50

static int ipa_send_wdi3_setup_pipe_cmd(
	u8 is_smmu_enabled, struct ipa_wdi_pipe_setup_info *info,
	struct ipa_wdi_pipe_setup_info_smmu *info_smmu, u8 dir)
{
	int ipa_ep_idx;
	int result = 0, len;
	unsigned long va;
	struct ipa_mem_buffer cmd;
	struct IpaHwWdi3SetUpCmdData_t *wdi3_params;
	struct IpaHwOffloadSetUpCmdData_t *cmd_data;

	if (info == NULL || info_smmu == NULL) {
		IPAERR("invalid input\n");
		return -EINVAL;
	}

	cmd.size = sizeof(*cmd_data);
	cmd.base = dma_alloc_coherent(ipa_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -ENOMEM;
	}

	cmd_data = (struct IpaHwOffloadSetUpCmdData_t *)cmd.base;
	cmd_data->protocol = IPA_HW_FEATURE_WDI3;

	if (!is_smmu_enabled) {
		ipa_ep_idx = ipa_get_ep_mapping(info->client);
		if (ipa_ep_idx == -1) {
			IPAERR("fail to get ep idx.\n");
			return -EFAULT;
		}

		IPADBG("client=%d ep=%d\n", info->client, ipa_ep_idx);
		IPADBG("ring_base_pa = 0x%pad\n", &info->transfer_ring_base_pa);
		IPADBG("ring_size = %hu\n", info->transfer_ring_size);
		IPADBG("ring_db_pa = 0x%pad\n",
			&info->transfer_ring_doorbell_pa);
		IPADBG("evt_ring_base_pa = 0x%pad\n",
			&info->event_ring_base_pa);
		IPADBG("evt_ring_size = %hu\n", info->event_ring_size);
		IPADBG("evt_ring_db_pa = 0x%pad\n",
			&info->event_ring_doorbell_pa);
		IPADBG("num_pkt_buffers = %hu\n", info->num_pkt_buffers);
		IPADBG("pkt_offset = %d\n", info->pkt_offset);

		wdi3_params = &cmd_data->SetupCh_params.Wdi3SetupCh_params;
		wdi3_params->transfer_ring_base_pa =
			(u32)info->transfer_ring_base_pa;
		wdi3_params->transfer_ring_base_pa_hi =
			(u32)((u64)info->transfer_ring_base_pa >> 32);
		wdi3_params->transfer_ring_size = info->transfer_ring_size;
		wdi3_params->transfer_ring_doorbell_pa =
			(u32)info->transfer_ring_doorbell_pa;
		wdi3_params->transfer_ring_doorbell_pa_hi =
			(u32)((u64)info->transfer_ring_doorbell_pa >> 32);
		wdi3_params->event_ring_base_pa = (u32)info->event_ring_base_pa;
		wdi3_params->event_ring_base_pa_hi =
			(u32)((u64)info->event_ring_base_pa >> 32);
		wdi3_params->event_ring_size = info->event_ring_size;
		wdi3_params->event_ring_doorbell_pa =
			(u32)info->event_ring_doorbell_pa;
		wdi3_params->event_ring_doorbell_pa_hi =
			(u32)((u64)info->event_ring_doorbell_pa >> 32);
		wdi3_params->num_pkt_buffers = info->num_pkt_buffers;
		wdi3_params->ipa_pipe_number = ipa_ep_idx;
		wdi3_params->dir = dir;
		wdi3_params->pkt_offset = info->pkt_offset;
		memcpy(wdi3_params->desc_format_template,
			info->desc_format_template,
			sizeof(wdi3_params->desc_format_template));
	} else {
		ipa_ep_idx = ipa_get_ep_mapping(info_smmu->client);
		if (ipa_ep_idx == -1) {
			IPAERR("fail to get ep idx\n");
			return -EFAULT;
		}

		IPADBG("client=%d ep=%d\n", info_smmu->client, ipa_ep_idx);
		IPADBG("ring_size = %hu\n", info_smmu->transfer_ring_size);
		IPADBG("ring_db_pa = 0x%pad\n",
			&info_smmu->transfer_ring_doorbell_pa);
		IPADBG("evt_ring_size = %hu\n", info_smmu->event_ring_size);
		IPADBG("evt_ring_db_pa = 0x%pad\n",
			&info_smmu->event_ring_doorbell_pa);
		IPADBG("num_pkt_buffers = %hu\n", info_smmu->num_pkt_buffers);
		IPADBG("pkt_offset = %d\n", info_smmu->pkt_offset);

		wdi3_params = &cmd_data->SetupCh_params.Wdi3SetupCh_params;

		if (dir == IPA_WDI3_TX_DIR) {
			len = info_smmu->transfer_ring_size;
			if (ipa2_create_uc_smmu_mapping(IPA_WDI_TX_RING_RES,
				true, info->transfer_ring_base_pa,
				&info_smmu->transfer_ring_base, len,
				false, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->transfer_ring_base_pa = (u32)va;
			wdi3_params->transfer_ring_base_pa_hi =
				(u32)((u64)va >> 32);
			wdi3_params->transfer_ring_size = len;

			if (ipa2_create_uc_smmu_mapping(IPA_WDI_TX_DB_RES,
				true, info_smmu->transfer_ring_doorbell_pa,
				NULL, 4, true, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->transfer_ring_doorbell_pa =
				(u32)va;
			wdi3_params->transfer_ring_doorbell_pa_hi =
				(u32)((u64)va >> 32);

			len = info_smmu->event_ring_size;
			if (ipa2_create_uc_smmu_mapping(IPA_WDI_CE_RING_RES,
				true, info->event_ring_base_pa,
				&info_smmu->event_ring_base, len,
				false, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->event_ring_base_pa = (u32)va;
			wdi3_params->event_ring_base_pa_hi =
				(u32)((u64)va >> 32);
			wdi3_params->event_ring_size = len;

			if (ipa2_create_uc_smmu_mapping(IPA_WDI_CE_DB_RES,
				true, info_smmu->event_ring_doorbell_pa,
				NULL, 4, true, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->event_ring_doorbell_pa =
				(u32)va;
			wdi3_params->event_ring_doorbell_pa_hi =
				(u32)((u64)va >> 32);
		} else {
			len = info_smmu->transfer_ring_size;
			if (ipa2_create_uc_smmu_mapping(IPA_WDI_RX_RING_RES,
				true, info->transfer_ring_base_pa,
				&info_smmu->transfer_ring_base, len,
				false, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->transfer_ring_base_pa = (u32)va;
			wdi3_params->transfer_ring_base_pa_hi =
				(u32)((u64)va >> 32);
			wdi3_params->transfer_ring_size = len;

			if (ipa2_create_uc_smmu_mapping(IPA_WDI_RX_RING_RP_RES,
				true, info_smmu->transfer_ring_doorbell_pa,
				NULL, 4, true, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->transfer_ring_doorbell_pa =
				(u32)va;
			wdi3_params->transfer_ring_doorbell_pa_hi =
				(u32)((u64)va >> 32);

			len = info_smmu->event_ring_size;
			if (ipa2_create_uc_smmu_mapping(
				IPA_WDI_RX_COMP_RING_RES, true,
				info->event_ring_base_pa,
				&info_smmu->event_ring_base, len,
				false, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->event_ring_base_pa = (u32)va;
			wdi3_params->event_ring_base_pa_hi =
				(u32)((u64)va >> 32);
			wdi3_params->event_ring_size = len;

			if (ipa2_create_uc_smmu_mapping(
				IPA_WDI_RX_COMP_RING_WP_RES, true,
				info_smmu->event_ring_doorbell_pa,
				NULL, 4, true, &va)) {
				IPAERR("failed to get smmu mapping\n");
				return -EFAULT;
			}
			wdi3_params->event_ring_doorbell_pa =
				(u32)va;
			wdi3_params->event_ring_doorbell_pa_hi =
				(u32)((u64)va >> 32);
		}
		wdi3_params->num_pkt_buffers = info_smmu->num_pkt_buffers;
		wdi3_params->ipa_pipe_number = ipa_ep_idx;
		wdi3_params->dir = dir;
		wdi3_params->pkt_offset = info_smmu->pkt_offset;
		memcpy(wdi3_params->desc_format_template,
			info_smmu->desc_format_template,
			sizeof(wdi3_params->desc_format_template));
	}

	result = ipa_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_CHANNEL_SET_UP,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result) {
		IPAERR("uc setup channel cmd failed: %d\n", result);
		result = -EFAULT;
	}

	dma_free_coherent(ipa_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	return result;
}

int ipa2_conn_wdi3_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out,
	ipa_wdi_meter_notifier_cb wdi_notify)
{
	enum ipa_client_type rx_client;
	enum ipa_client_type tx_client;
	struct ipa_ep_context *ep_rx;
	struct ipa_ep_context *ep_tx;
	int ipa_ep_idx_rx;
	int ipa_ep_idx_tx;
	int result = 0;

	if (in == NULL || out == NULL) {
		IPAERR("invalid input\n");
		return -EINVAL;
	}

	if (in->is_smmu_enabled == false) {
		rx_client = in->u_rx.rx.client;
		tx_client = in->u_tx.tx.client;
	} else {
		rx_client = in->u_rx.rx_smmu.client;
		tx_client = in->u_tx.tx_smmu.client;
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(rx_client);
	ipa_ep_idx_tx = ipa_get_ep_mapping(tx_client);

	if (ipa_ep_idx_rx == -1 || ipa_ep_idx_tx == -1) {
		IPAERR("fail to alloc EP.\n");
		return -EFAULT;
	}
	if (ipa_ep_idx_rx >= IPA_MAX_NUM_PIPES ||
		ipa_ep_idx_tx >= IPA_MAX_NUM_PIPES) {
		IPAERR("ep out of range.\n");
		return -EFAULT;
	}

	ep_rx = &ipa_ctx->ep[ipa_ep_idx_rx];
	ep_tx = &ipa_ctx->ep[ipa_ep_idx_tx];

	if (ep_rx->valid || ep_tx->valid) {
		IPAERR("EP already allocated.\n");
		return -EFAULT;
	}

	memset(ep_rx, 0, offsetof(struct ipa_ep_context, sys));
	memset(ep_tx, 0, offsetof(struct ipa_ep_context, sys));

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (wdi_notify)
		ipa_ctx->uc_wdi_ctx.stats_notify = wdi_notify;
	else
		IPADBG("wdi_notify is null\n");

	/* setup rx ep cfg */
	ep_rx->valid = 1;
	ep_rx->client = rx_client;
	result = ipa_disable_data_path(ipa_ep_idx_rx);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx_rx);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return -EFAULT;
	}
	ep_rx->client_notify = in->notify;
	ep_rx->priv = in->priv;

	if (in->is_smmu_enabled == false)
		memcpy(&ep_rx->cfg, &in->u_rx.rx.ipa_ep_cfg,
			sizeof(ep_rx->cfg));
	else
		memcpy(&ep_rx->cfg, &in->u_rx.rx_smmu.ipa_ep_cfg,
			sizeof(ep_rx->cfg));

	if (ipa_cfg_ep(ipa_ep_idx_rx, &ep_rx->cfg)) {
		IPAERR("fail to setup rx pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	if (ipa_send_wdi3_setup_pipe_cmd(in->is_smmu_enabled,
		&in->u_rx.rx, &in->u_rx.rx_smmu, IPA_WDI3_RX_DIR)) {
		IPAERR("fail to send cmd to uc for rx pipe\n");
		result = -EFAULT;
		goto fail;
	}
	ipa_install_dflt_flt_rules(ipa_ep_idx_rx);
	out->rx_uc_db_pa = ipa_ctx->ipa_wrapper_base +
		IPA_REG_BASE_OFST_v2_5 +
		IPA_UC_MAILBOX_m_n_OFFS_v2_5(
		IPA_HW_WDI3_RX_MBOX_START_INDEX/32,
		IPA_HW_WDI3_RX_MBOX_START_INDEX % 32);

	IPADBG("client %d (ep: %d) connected\n", rx_client,
		ipa_ep_idx_rx);

	/* setup tx ep cfg */
	ep_tx->valid = 1;
	ep_tx->client = tx_client;
	result = ipa_disable_data_path(ipa_ep_idx_tx);
	if (result) {
		IPAERR("disable data path failed res=%d ep=%d.\n", result,
			ipa_ep_idx_tx);
		result = -EFAULT;
		goto fail;
	}

	if (in->is_smmu_enabled == false)
		memcpy(&ep_tx->cfg, &in->u_tx.tx.ipa_ep_cfg,
			sizeof(ep_tx->cfg));
	else
		memcpy(&ep_tx->cfg, &in->u_tx.tx_smmu.ipa_ep_cfg,
			sizeof(ep_tx->cfg));

	if (ipa_cfg_ep(ipa_ep_idx_tx, &ep_tx->cfg)) {
		IPAERR("fail to setup tx pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	if (ipa_send_wdi3_setup_pipe_cmd(in->is_smmu_enabled,
		&in->u_tx.tx, &in->u_tx.tx_smmu, IPA_WDI3_TX_DIR)) {
		IPAERR("fail to send cmd to uc for tx pipe\n");
		result = -EFAULT;
		goto fail;
	}
	out->tx_uc_db_pa = ipa_ctx->ipa_wrapper_base +
		IPA_REG_BASE_OFST_v2_5 +
		IPA_UC_MAILBOX_m_n_OFFS_v2_5(
		IPA_HW_WDI3_TX_MBOX_START_INDEX/32,
		IPA_HW_WDI3_TX_MBOX_START_INDEX % 32);
	IPADBG("client %d (ep: %d) connected\n", tx_client,
		ipa_ep_idx_tx);

fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

static int ipa_send_wdi3_common_ch_cmd(int ipa_ep_idx, int command)
{
	struct ipa_mem_buffer cmd;
	struct IpaHwOffloadCommonChCmdData_t *cmd_data;
	union IpaHwWdi3CommonChCmdData_t *wdi3;
	int result = 0;

	cmd.size = sizeof(*cmd_data);
	cmd.base = dma_alloc_coherent(ipa_ctx->uc_pdev, cmd.size,
		&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -ENOMEM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	/* enable the TX pipe */
	cmd_data = (struct IpaHwOffloadCommonChCmdData_t *)cmd.base;
	cmd_data->protocol = IPA_HW_FEATURE_WDI3;

	wdi3 = &cmd_data->CommonCh_params.Wdi3CommonCh_params;
	wdi3->params.ipa_pipe_number = ipa_ep_idx;
	result = ipa_uc_send_cmd((u32)(cmd.phys_base), command,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result) {
		result = -EFAULT;
		goto fail;
	}

fail:
	dma_free_coherent(ipa_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

int ipa2_disconn_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	struct ipa_ep_context *ep_tx, *ep_rx;
	int result = 0;

	IPADBG("ep_tx = %d\n", ipa_ep_idx_tx);
	IPADBG("ep_rx = %d\n", ipa_ep_idx_rx);

	if (ipa_ep_idx_tx < 0 || ipa_ep_idx_tx >= IPA_MAX_NUM_PIPES ||
		ipa_ep_idx_rx < 0 || ipa_ep_idx_rx >= IPA_MAX_NUM_PIPES) {
		IPAERR("invalid ipa ep index\n");
		return -EINVAL;
	}

	ep_tx = &ipa_ctx->ep[ipa_ep_idx_tx];
	ep_rx = &ipa_ctx->ep[ipa_ep_idx_rx];

	/* tear down tx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_tx,
		IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN)) {
		IPAERR("fail to tear down tx pipe\n");
		result = -EFAULT;
		goto fail;
	}
	ipa_disable_data_path(ipa_ep_idx_tx);
	memset(ep_tx, 0, sizeof(struct ipa_ep_context));
	IPADBG("tx client (ep: %d) disconnected\n", ipa_ep_idx_tx);

	/* tear down rx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_rx,
		IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN)) {
		IPAERR("fail to tear down rx pipe\n");
		result = -EFAULT;
		goto fail;
	}
	ipa_disable_data_path(ipa_ep_idx_rx);
	ipa_delete_dflt_flt_rules(ipa_ep_idx_rx);
	memset(ep_rx, 0, sizeof(struct ipa_ep_context));
	IPADBG("rx client (ep: %d) disconnected\n", ipa_ep_idx_rx);

fail:
	return result;
}

int ipa2_enable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	struct ipa_ep_context *ep_tx, *ep_rx;
	int result = 0;

	IPADBG("ep_tx = %d\n", ipa_ep_idx_tx);
	IPADBG("ep_rx = %d\n", ipa_ep_idx_rx);

	ep_tx = &ipa_ctx->ep[ipa_ep_idx_tx];
	ep_rx = &ipa_ctx->ep[ipa_ep_idx_rx];

	/* enable tx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_tx,
		IPA_CPU_2_HW_CMD_OFFLOAD_ENABLE)) {
		IPAERR("fail to enable tx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* resume tx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_tx,
		IPA_CPU_2_HW_CMD_OFFLOAD_RESUME)) {
		IPAERR("fail to resume tx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* enable rx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_rx,
		IPA_CPU_2_HW_CMD_OFFLOAD_ENABLE)) {
		IPAERR("fail to enable rx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* resume rx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_rx,
		IPA_CPU_2_HW_CMD_OFFLOAD_RESUME)) {
		IPAERR("fail to resume rx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* enable data path */
	result = ipa_enable_data_path(ipa_ep_idx_rx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx_rx);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return -EFAULT;
	}

	result = ipa_enable_data_path(ipa_ep_idx_tx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx_tx);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return -EFAULT;
	}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

fail:
	return result;
}

int ipa2_disable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	struct ipa_ep_context *ep_tx, *ep_rx;
	int result = 0;

	IPADBG("ep_tx = %d\n", ipa_ep_idx_tx);
	IPADBG("ep_rx = %d\n", ipa_ep_idx_rx);

	ep_tx = &ipa_ctx->ep[ipa_ep_idx_tx];
	ep_rx = &ipa_ctx->ep[ipa_ep_idx_rx];

	/* suspend tx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_tx,
		IPA_CPU_2_HW_CMD_OFFLOAD_SUSPEND)) {
		IPAERR("fail to suspend tx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* disable tx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_tx,
		IPA_CPU_2_HW_CMD_OFFLOAD_DISABLE)) {
		IPAERR("fail to disable tx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* suspend rx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_rx,
		IPA_CPU_2_HW_CMD_OFFLOAD_SUSPEND)) {
		IPAERR("fail to suspend rx pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* disable rx pipe */
	if (ipa_send_wdi3_common_ch_cmd(ipa_ep_idx_rx,
		IPA_CPU_2_HW_CMD_OFFLOAD_DISABLE)) {
		IPAERR("fail to disable rx pipe\n");
		result = -EFAULT;
		goto fail;
	}

fail:
	return result;
}

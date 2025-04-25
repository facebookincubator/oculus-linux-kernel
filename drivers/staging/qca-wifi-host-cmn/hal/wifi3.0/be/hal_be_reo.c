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

#include "qdf_module.h"
#include "hal_hw_headers.h"
#include "hal_be_hw_headers.h"
#include "hal_reo.h"
#include "hal_be_reo.h"
#include "hal_be_api.h"

uint32_t hal_get_reo_reg_base_offset_be(void)
{
	return REO_REG_REG_BASE;
}

void hal_reo_qdesc_setup_be(hal_soc_handle_t hal_soc_hdl, int tid,
			    uint32_t ba_window_size,
			    uint32_t start_seq, void *hw_qdesc_vaddr,
			    qdf_dma_addr_t hw_qdesc_paddr,
			    int pn_type, uint8_t vdev_stats_id)
{
	uint32_t *reo_queue_desc = (uint32_t *)hw_qdesc_vaddr;
	uint32_t *reo_queue_ext_desc;
	uint32_t reg_val;
	uint32_t pn_enable;
	uint32_t pn_size = 0;

	qdf_mem_zero(hw_qdesc_vaddr, sizeof(struct rx_reo_queue));

	hal_uniform_desc_hdr_setup(reo_queue_desc, HAL_DESC_REO_OWNED,
				   HAL_REO_QUEUE_DESC);
	/* Fixed pattern in reserved bits for debugging */
	HAL_DESC_SET_FIELD(reo_queue_desc, UNIFORM_DESCRIPTOR_HEADER,
			   RESERVED_0A, 0xDDBEEF);

	/* This a just a SW meta data and will be copied to REO destination
	 * descriptors indicated by hardware.
	 * TODO: Setting TID in this field. See if we should set something else.
	 */
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
			   RECEIVE_QUEUE_NUMBER, tid);
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
			   VLD, 1);
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
			   ASSOCIATED_LINK_DESCRIPTOR_COUNTER,
			   HAL_RX_LINK_DESC_CNTR);

	/*
	 * Fields DISABLE_DUPLICATE_DETECTION and SOFT_REORDER_ENABLE will be 0
	 */

	reg_val = TID_TO_WME_AC(tid);
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, AC, reg_val);

	if (ba_window_size < 1)
		ba_window_size = 1;

	/* WAR to get 2k exception in Non BA case.
	 * Setting window size to 2 to get 2k jump exception
	 * when we receive aggregates in Non BA case
	 */
	ba_window_size = hal_update_non_ba_win_size(tid, ba_window_size);

	/* Set RTY bit for non-BA case. Duplicate detection is currently not
	 * done by HW in non-BA case if RTY bit is not set.
	 * TODO: This is a temporary War and should be removed once HW fix is
	 * made to check and discard duplicates even if RTY bit is not set.
	 */
	if (ba_window_size == 1)
		HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, RTY, 1);

	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, BA_WINDOW_SIZE,
			   ba_window_size - 1);

	switch (pn_type) {
	case HAL_PN_WPA:
		pn_enable = 1;
		pn_size = PN_SIZE_48;
		break;
	case HAL_PN_WAPI_EVEN:
	case HAL_PN_WAPI_UNEVEN:
		pn_enable = 1;
		pn_size = PN_SIZE_128;
		break;
	default:
		pn_enable = 0;
		break;
	}

	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, PN_CHECK_NEEDED,
			   pn_enable);

	if (pn_type == HAL_PN_WAPI_EVEN)
		HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
				   PN_SHALL_BE_EVEN, 1);
	else if (pn_type == HAL_PN_WAPI_UNEVEN)
		HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
				   PN_SHALL_BE_UNEVEN, 1);

	/*
	 *  TODO: Need to check if PN handling in SW needs to be enabled
	 *  So far this is not a requirement
	 */

	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, PN_SIZE,
			   pn_size);

	/* TODO: Check if RX_REO_QUEUE_IGNORE_AMPDU_FLAG need to be set
	 * based on BA window size and/or AMPDU capabilities
	 */
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
			   IGNORE_AMPDU_FLAG, 1);

	if (start_seq <= 0xfff)
		HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, SSN,
				   start_seq);

	/* TODO: SVLD should be set to 1 if a valid SSN is received in ADDBA,
	 * but REO is not delivering packets if we set it to 1. Need to enable
	 * this once the issue is resolved
	 */
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE, SVLD, 0);

	hal_update_stats_counter_index(reo_queue_desc, vdev_stats_id);

	/* TODO: Check if we should set start PN for WAPI */

	/* TODO: HW queue descriptors are currently allocated for max BA
	 * window size for all QOS TIDs so that same descriptor can be used
	 * later when ADDBA request is received. This should be changed to
	 * allocate HW queue descriptors based on BA window size being
	 * negotiated (0 for non BA cases), and reallocate when BA window
	 * size changes and also send WMI message to FW to change the REO
	 * queue descriptor in Rx peer entry as part of dp_rx_tid_update.
	 */
	if (tid == HAL_NON_QOS_TID)
		return;

	reo_queue_ext_desc = (uint32_t *)
		(((struct rx_reo_queue *)reo_queue_desc) + 1);
	qdf_mem_zero(reo_queue_ext_desc, 3 *
		     sizeof(struct rx_reo_queue_ext));
	/* Initialize first reo queue extension descriptor */
	hal_uniform_desc_hdr_setup(reo_queue_ext_desc,
				   HAL_DESC_REO_OWNED,
				   HAL_REO_QUEUE_EXT_DESC);
	/* Fixed pattern in reserved bits for debugging */
	HAL_DESC_SET_FIELD(reo_queue_ext_desc,
			   UNIFORM_DESCRIPTOR_HEADER, RESERVED_0A,
			   0xADBEEF);
	/* Initialize second reo queue extension descriptor */
	reo_queue_ext_desc = (uint32_t *)
		(((struct rx_reo_queue_ext *)reo_queue_ext_desc) + 1);
	hal_uniform_desc_hdr_setup(reo_queue_ext_desc,
				   HAL_DESC_REO_OWNED,
				   HAL_REO_QUEUE_EXT_DESC);
	/* Fixed pattern in reserved bits for debugging */
	HAL_DESC_SET_FIELD(reo_queue_ext_desc,
			   UNIFORM_DESCRIPTOR_HEADER, RESERVED_0A,
			   0xBDBEEF);
	/* Initialize third reo queue extension descriptor */
	reo_queue_ext_desc = (uint32_t *)
		(((struct rx_reo_queue_ext *)reo_queue_ext_desc) + 1);
	hal_uniform_desc_hdr_setup(reo_queue_ext_desc,
				   HAL_DESC_REO_OWNED,
				   HAL_REO_QUEUE_EXT_DESC);
	/* Fixed pattern in reserved bits for debugging */
	HAL_DESC_SET_FIELD(reo_queue_ext_desc,
			   UNIFORM_DESCRIPTOR_HEADER, RESERVED_0A,
			   0xCDBEEF);
}

qdf_export_symbol(hal_reo_qdesc_setup_be);

static void
hal_reo_cmd_set_descr_addr_be(uint32_t *reo_desc,
			      enum hal_reo_cmd_type type,
			      uint32_t paddr_lo,
			      uint8_t paddr_hi)
{
	struct reo_get_queue_stats *reo_get_queue_stats;
	struct reo_flush_queue *reo_flush_queue;
	struct reo_flush_cache *reo_flush_cache;
	struct reo_update_rx_reo_queue *reo_update_rx_reo_queue;

	switch (type) {
	case CMD_GET_QUEUE_STATS:
		reo_get_queue_stats = (struct reo_get_queue_stats *)reo_desc;
		reo_get_queue_stats->rx_reo_queue_desc_addr_31_0 = paddr_lo;
		reo_get_queue_stats->rx_reo_queue_desc_addr_39_32 = paddr_hi;
		break;
	case CMD_FLUSH_QUEUE:
		reo_flush_queue = (struct reo_flush_queue *)reo_desc;
		reo_flush_queue->flush_desc_addr_31_0 = paddr_lo;
		reo_flush_queue->flush_desc_addr_39_32 = paddr_hi;
		break;
	case CMD_FLUSH_CACHE:
		reo_flush_cache = (struct reo_flush_cache *)reo_desc;
		reo_flush_cache->flush_addr_31_0 = paddr_lo;
		reo_flush_cache->flush_addr_39_32 = paddr_hi;
		break;
	case CMD_UPDATE_RX_REO_QUEUE:
		reo_update_rx_reo_queue =
				(struct reo_update_rx_reo_queue *)reo_desc;
		reo_update_rx_reo_queue->rx_reo_queue_desc_addr_31_0 = paddr_lo;
		reo_update_rx_reo_queue->rx_reo_queue_desc_addr_39_32 =
								paddr_hi;
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid REO command type", __func__);
		break;
	}
}

static int
hal_reo_cmd_queue_stats_be(hal_ring_handle_t  hal_ring_hdl,
			   hal_soc_handle_t hal_soc_hdl,
			   struct hal_reo_cmd_params *cmd)
{
	uint32_t *reo_desc, val;
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct reo_get_queue_stats *reo_get_queue_stats;

	hal_srng_access_start(hal_soc_hdl, hal_ring_hdl);
	reo_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);
	if (!reo_desc) {
		hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
		hal_warn_rl("Out of cmd ring entries");
		return -EBUSY;
	}

	HAL_SET_TLV_HDR(reo_desc, WIFIREO_GET_QUEUE_STATS_E,
			sizeof(struct reo_get_queue_stats));

	/*
	 * Offsets of descriptor fields defined in HW headers start from
	 * the field after TLV header
	 */
	reo_desc += (sizeof(struct tlv_32_hdr) >> 2);
	qdf_mem_zero((reo_desc + NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER),
		     sizeof(struct reo_get_queue_stats) -
		     (NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER << 2));

	reo_get_queue_stats = (struct reo_get_queue_stats *)reo_desc;
	reo_get_queue_stats->cmd_header.reo_status_required =
							cmd->std.need_status;

	hal_reo_cmd_set_descr_addr_be(reo_desc, CMD_GET_QUEUE_STATS,
				      cmd->std.addr_lo,
				      cmd->std.addr_hi);

	reo_get_queue_stats->clear_stats = cmd->u.stats_params.clear;

	hal_srng_access_end_v1(hal_soc_hdl, hal_ring_hdl,
			       HIF_RTPM_ID_HAL_REO_CMD);

	val = reo_desc[CMD_HEADER_DW_OFFSET];
	return HAL_GET_FIELD(UNIFORM_REO_CMD_HEADER, REO_CMD_NUMBER,
				     val);
}

static int
hal_reo_cmd_flush_queue_be(hal_ring_handle_t hal_ring_hdl,
			   hal_soc_handle_t hal_soc_hdl,
			   struct hal_reo_cmd_params *cmd)
{
	uint32_t *reo_desc, val;
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct reo_flush_queue *reo_flush_queue;

	hal_srng_access_start(hal_soc_hdl, hal_ring_hdl);
	reo_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);
	if (!reo_desc) {
		hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
		hal_warn_rl("Out of cmd ring entries");
		return -EBUSY;
	}

	HAL_SET_TLV_HDR(reo_desc, WIFIREO_FLUSH_QUEUE_E,
			sizeof(struct reo_flush_queue));

	/*
	 * Offsets of descriptor fields defined in HW headers start from
	 * the field after TLV header
	 */
	reo_desc += (sizeof(struct tlv_32_hdr) >> 2);
	qdf_mem_zero((reo_desc + NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER),
		     sizeof(struct reo_flush_queue) -
		     (NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER << 2));

	reo_flush_queue = (struct reo_flush_queue *)reo_desc;
	reo_flush_queue->cmd_header.reo_status_required = cmd->std.need_status;

	hal_reo_cmd_set_descr_addr_be(reo_desc, CMD_FLUSH_QUEUE,
				      cmd->std.addr_lo, cmd->std.addr_hi);

	reo_flush_queue->block_desc_addr_usage_after_flush =
				cmd->u.fl_queue_params.block_use_after_flush;

	if (cmd->u.fl_queue_params.block_use_after_flush)
		reo_flush_queue->block_resource_index =
						cmd->u.fl_queue_params.index;

	hal_srng_access_end_v1(hal_soc_hdl, hal_ring_hdl,
			       HIF_RTPM_ID_HAL_REO_CMD);

	val = reo_desc[CMD_HEADER_DW_OFFSET];
	return HAL_GET_FIELD(UNIFORM_REO_CMD_HEADER, REO_CMD_NUMBER,
				     val);
}

static int
hal_reo_cmd_flush_cache_be(hal_ring_handle_t hal_ring_hdl,
			   hal_soc_handle_t hal_soc_hdl,
			   struct hal_reo_cmd_params *cmd)
{
	uint32_t *reo_desc, val;
	struct hal_reo_cmd_flush_cache_params *cp;
	uint8_t index = 0;
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct reo_flush_cache *reo_flush_cache;

	cp = &cmd->u.fl_cache_params;

	hal_srng_access_start(hal_soc_hdl, hal_ring_hdl);

	/* We need a cache block resource for this operation, and REO HW has
	 * only 4 such blocking resources. These resources are managed using
	 * reo_res_bitmap, and we return failure if none is available.
	 */
	if (cp->block_use_after_flush) {
		index = hal_find_zero_bit(hal_soc->reo_res_bitmap);
		if (index > 3) {
			hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
			hal_warn_rl("No blocking resource available!");
			return -EBUSY;
		}
		hal_soc->index = index;
	}

	reo_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);
	if (!reo_desc) {
		hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
		hal_srng_dump(hal_ring_handle_to_hal_srng(hal_ring_hdl));
		return -EBUSY;
	}

	HAL_SET_TLV_HDR(reo_desc, WIFIREO_FLUSH_CACHE_E,
			sizeof(struct reo_flush_cache));

	/*
	 * Offsets of descriptor fields defined in HW headers start from
	 * the field after TLV header
	 */
	reo_desc += (sizeof(struct tlv_32_hdr) >> 2);
	qdf_mem_zero((reo_desc + NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER),
		     sizeof(struct reo_flush_cache) -
		     (NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER << 2));

	reo_flush_cache = (struct reo_flush_cache *)reo_desc;
	reo_flush_cache->cmd_header.reo_status_required = cmd->std.need_status;

	hal_reo_cmd_set_descr_addr_be(reo_desc, CMD_FLUSH_CACHE,
				      cmd->std.addr_lo, cmd->std.addr_hi);

	reo_flush_cache->forward_all_mpdus_in_queue = cp->fwd_mpdus_in_queue;

	/* set it to 0 for now */
	cp->rel_block_index = 0;
	reo_flush_cache->release_cache_block_index = cp->rel_block_index;

	if (cp->block_use_after_flush) {
		reo_flush_cache->cache_block_resource_index = index;
	}

	reo_flush_cache->flush_without_invalidate = cp->flush_no_inval;
	reo_flush_cache->flush_queue_1k_desc = cp->flush_q_1k_desc;
	reo_flush_cache->block_cache_usage_after_flush =
						cp->block_use_after_flush;
	reo_flush_cache->flush_entire_cache = cp->flush_entire_cache;

	hal_srng_access_end_v1(hal_soc_hdl, hal_ring_hdl,
			       HIF_RTPM_ID_HAL_REO_CMD);

	val = reo_desc[CMD_HEADER_DW_OFFSET];
	return HAL_GET_FIELD(UNIFORM_REO_CMD_HEADER, REO_CMD_NUMBER,
				     val);
}

static int
hal_reo_cmd_unblock_cache_be(hal_ring_handle_t hal_ring_hdl,
			     hal_soc_handle_t hal_soc_hdl,
			     struct hal_reo_cmd_params *cmd)

{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t *reo_desc, val;
	uint8_t index = 0;
	struct reo_unblock_cache *reo_unblock_cache;

	hal_srng_access_start(hal_soc_hdl, hal_ring_hdl);

	if (cmd->u.unblk_cache_params.type == UNBLOCK_RES_INDEX) {
		index = hal_find_one_bit(hal_soc->reo_res_bitmap);
		if (index > 3) {
			hal_srng_access_end(hal_soc, hal_ring_hdl);
			qdf_print("No blocking resource to unblock!");
			return -EBUSY;
		}
	}

	reo_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);
	if (!reo_desc) {
		hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
		hal_warn_rl("Out of cmd ring entries");
		return -EBUSY;
	}

	HAL_SET_TLV_HDR(reo_desc, WIFIREO_UNBLOCK_CACHE_E,
			sizeof(struct reo_unblock_cache));

	/*
	 * Offsets of descriptor fields defined in HW headers start from
	 * the field after TLV header
	 */
	reo_desc += (sizeof(struct tlv_32_hdr) >> 2);
	qdf_mem_zero((reo_desc + NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER),
		     sizeof(struct reo_unblock_cache) -
		     (NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER << 2));

	reo_unblock_cache = (struct reo_unblock_cache *)reo_desc;
	reo_unblock_cache->cmd_header.reo_status_required =
							cmd->std.need_status;
	reo_unblock_cache->unblock_type = cmd->u.unblk_cache_params.type;

	if (cmd->u.unblk_cache_params.type == UNBLOCK_RES_INDEX)
		reo_unblock_cache->cache_block_resource_index =
						cmd->u.unblk_cache_params.index;

	hal_srng_access_end(hal_soc, hal_ring_hdl);
	val = reo_desc[CMD_HEADER_DW_OFFSET];
	return HAL_GET_FIELD(UNIFORM_REO_CMD_HEADER, REO_CMD_NUMBER,
				     val);
}

static int
hal_reo_cmd_flush_timeout_list_be(hal_ring_handle_t hal_ring_hdl,
				  hal_soc_handle_t hal_soc_hdl,
				  struct hal_reo_cmd_params *cmd)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t *reo_desc, val;
	struct reo_flush_timeout_list *reo_flush_timeout_list;

	hal_srng_access_start(hal_soc_hdl, hal_ring_hdl);
	reo_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);
	if (!reo_desc) {
		hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
		hal_warn_rl("Out of cmd ring entries");
		return -EBUSY;
	}

	HAL_SET_TLV_HDR(reo_desc, WIFIREO_FLUSH_TIMEOUT_LIST_E,
			sizeof(struct reo_flush_timeout_list));

	/*
	 * Offsets of descriptor fields defined in HW headers start from
	 * the field after TLV header
	 */
	reo_desc += (sizeof(struct tlv_32_hdr) >> 2);
	qdf_mem_zero((reo_desc + NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER),
		     sizeof(struct reo_flush_timeout_list) -
		     (NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER << 2));

	reo_flush_timeout_list = (struct reo_flush_timeout_list *)reo_desc;
	reo_flush_timeout_list->cmd_header.reo_status_required =
							cmd->std.need_status;
	reo_flush_timeout_list->ac_timout_list =
					cmd->u.fl_tim_list_params.ac_list;
	reo_flush_timeout_list->minimum_release_desc_count =
					cmd->u.fl_tim_list_params.min_rel_desc;
	reo_flush_timeout_list->minimum_forward_buf_count =
					cmd->u.fl_tim_list_params.min_fwd_buf;

	hal_srng_access_end(hal_soc, hal_ring_hdl);
	val = reo_desc[CMD_HEADER_DW_OFFSET];
	return HAL_GET_FIELD(UNIFORM_REO_CMD_HEADER, REO_CMD_NUMBER,
				     val);
}

static int
hal_reo_cmd_update_rx_queue_be(hal_ring_handle_t hal_ring_hdl,
			       hal_soc_handle_t hal_soc_hdl,
			       struct hal_reo_cmd_params *cmd)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t *reo_desc, val;
	struct hal_reo_cmd_update_queue_params *p;
	struct reo_update_rx_reo_queue *reo_update_rx_reo_queue;

	p = &cmd->u.upd_queue_params;

	hal_srng_access_start(hal_soc_hdl, hal_ring_hdl);
	reo_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);
	if (!reo_desc) {
		hal_srng_access_end_reap(hal_soc, hal_ring_hdl);
		hal_warn_rl("Out of cmd ring entries");
		return -EBUSY;
	}

	HAL_SET_TLV_HDR(reo_desc, WIFIREO_UPDATE_RX_REO_QUEUE_E,
			sizeof(struct reo_update_rx_reo_queue));

	/*
	 * Offsets of descriptor fields defined in HW headers start from
	 * the field after TLV header
	 */
	reo_desc += (sizeof(struct tlv_32_hdr) >> 2);
	qdf_mem_zero((reo_desc + NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER),
		     sizeof(struct reo_update_rx_reo_queue) -
		     (NUM_OF_DWORDS_UNIFORM_REO_CMD_HEADER << 2));

	reo_update_rx_reo_queue = (struct reo_update_rx_reo_queue *)reo_desc;
	reo_update_rx_reo_queue->cmd_header.reo_status_required =
							cmd->std.need_status;

	hal_reo_cmd_set_descr_addr_be(reo_desc, CMD_UPDATE_RX_REO_QUEUE,
				      cmd->std.addr_lo, cmd->std.addr_hi);

	reo_update_rx_reo_queue->update_receive_queue_number =
							p->update_rx_queue_num;
	reo_update_rx_reo_queue->update_vld = p->update_vld;
	reo_update_rx_reo_queue->update_associated_link_descriptor_counter =
						p->update_assoc_link_desc;
	reo_update_rx_reo_queue->update_disable_duplicate_detection =
						p->update_disable_dup_detect;
	reo_update_rx_reo_queue->update_soft_reorder_enable =
						p->update_soft_reorder_enab;
	reo_update_rx_reo_queue->update_ac = p->update_ac;
	reo_update_rx_reo_queue->update_bar = p->update_bar;
	reo_update_rx_reo_queue->update_rty = p->update_rty;
	reo_update_rx_reo_queue->update_chk_2k_mode = p->update_chk_2k_mode;
	reo_update_rx_reo_queue->update_oor_mode = p->update_oor_mode;
	reo_update_rx_reo_queue->update_ba_window_size =
						p->update_ba_window_size;
	reo_update_rx_reo_queue->update_pn_check_needed =
						p->update_pn_check_needed;
	reo_update_rx_reo_queue->update_pn_shall_be_even = p->update_pn_even;
	reo_update_rx_reo_queue->update_pn_shall_be_uneven =
							p->update_pn_uneven;
	reo_update_rx_reo_queue->update_pn_handling_enable =
							p->update_pn_hand_enab;
	reo_update_rx_reo_queue->update_pn_size = p->update_pn_size;
	reo_update_rx_reo_queue->update_ignore_ampdu_flag =
							p->update_ignore_ampdu;
	reo_update_rx_reo_queue->update_svld = p->update_svld;
	reo_update_rx_reo_queue->update_ssn = p->update_ssn;
	reo_update_rx_reo_queue->update_seq_2k_error_detected_flag =
						p->update_seq_2k_err_detect;
	reo_update_rx_reo_queue->update_pn_valid = p->update_pn_valid;
	reo_update_rx_reo_queue->update_pn = p->update_pn;
	reo_update_rx_reo_queue->receive_queue_number = p->rx_queue_num;
	reo_update_rx_reo_queue->vld = p->vld;
	reo_update_rx_reo_queue->associated_link_descriptor_counter =
							p->assoc_link_desc;
	reo_update_rx_reo_queue->disable_duplicate_detection =
							p->disable_dup_detect;
	reo_update_rx_reo_queue->soft_reorder_enable = p->soft_reorder_enab;
	reo_update_rx_reo_queue->ac = p->ac;
	reo_update_rx_reo_queue->bar = p->bar;
	reo_update_rx_reo_queue->chk_2k_mode = p->chk_2k_mode;
	reo_update_rx_reo_queue->rty = p->rty;
	reo_update_rx_reo_queue->oor_mode = p->oor_mode;
	reo_update_rx_reo_queue->pn_check_needed = p->pn_check_needed;
	reo_update_rx_reo_queue->pn_shall_be_even = p->pn_even;
	reo_update_rx_reo_queue->pn_shall_be_uneven = p->pn_uneven;
	reo_update_rx_reo_queue->pn_handling_enable = p->pn_hand_enab;
	reo_update_rx_reo_queue->ignore_ampdu_flag = p->ignore_ampdu;

	if (p->ba_window_size < 1)
		p->ba_window_size = 1;
	/*
	 * WAR to get 2k exception in Non BA case.
	 * Setting window size to 2 to get 2k jump exception
	 * when we receive aggregates in Non BA case
	 */
	if (p->ba_window_size == 1)
		p->ba_window_size++;

	reo_update_rx_reo_queue->ba_window_size = p->ba_window_size - 1;
	reo_update_rx_reo_queue->pn_size = p->pn_size;
	reo_update_rx_reo_queue->svld = p->svld;
	reo_update_rx_reo_queue->ssn = p->ssn;
	reo_update_rx_reo_queue->seq_2k_error_detected_flag =
							p->seq_2k_err_detect;
	reo_update_rx_reo_queue->pn_error_detected_flag = p->pn_err_detect;
	reo_update_rx_reo_queue->pn_31_0 = p->pn_31_0;
	reo_update_rx_reo_queue->pn_63_32 = p->pn_63_32;
	reo_update_rx_reo_queue->pn_95_64 = p->pn_95_64;
	reo_update_rx_reo_queue->pn_127_96 = p->pn_127_96;

	hal_srng_access_end_v1(hal_soc_hdl, hal_ring_hdl,
			       HIF_RTPM_ID_HAL_REO_CMD);

	val = reo_desc[CMD_HEADER_DW_OFFSET];
	return HAL_GET_FIELD(UNIFORM_REO_CMD_HEADER, REO_CMD_NUMBER,
				     val);
}

int hal_reo_send_cmd_be(hal_soc_handle_t hal_soc_hdl,
			hal_ring_handle_t  hal_ring_hdl,
			enum hal_reo_cmd_type cmd,
			void *params)
{
	struct hal_reo_cmd_params *cmd_params =
			(struct hal_reo_cmd_params *)params;
	int num = 0;

	switch (cmd) {
	case CMD_GET_QUEUE_STATS:
		num = hal_reo_cmd_queue_stats_be(hal_ring_hdl,
						 hal_soc_hdl, cmd_params);
		break;
	case CMD_FLUSH_QUEUE:
		num = hal_reo_cmd_flush_queue_be(hal_ring_hdl,
						 hal_soc_hdl, cmd_params);
		break;
	case CMD_FLUSH_CACHE:
		num = hal_reo_cmd_flush_cache_be(hal_ring_hdl,
						 hal_soc_hdl, cmd_params);
		break;
	case CMD_UNBLOCK_CACHE:
		num = hal_reo_cmd_unblock_cache_be(hal_ring_hdl,
						   hal_soc_hdl, cmd_params);
		break;
	case CMD_FLUSH_TIMEOUT_LIST:
		num = hal_reo_cmd_flush_timeout_list_be(hal_ring_hdl,
							hal_soc_hdl,
							cmd_params);
		break;
	case CMD_UPDATE_RX_REO_QUEUE:
		num = hal_reo_cmd_update_rx_queue_be(hal_ring_hdl,
						     hal_soc_hdl, cmd_params);
		break;
	default:
		hal_err("Invalid REO command type: %d", cmd);
		return -EINVAL;
	};

	return num;
}

void
hal_reo_queue_stats_status_be(hal_ring_desc_t ring_desc,
			      void *st_handle,
			      hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_queue_status *st =
		(struct hal_reo_queue_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;
	uint64_t val;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc, HAL_REO_QUEUE_STATS_STATUS_TLV,
				  &(st->header), hal_soc);

	/* SSN */
	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS, SSN)];
	st->ssn = HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS, SSN, val);

	/* current index */
	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 CURRENT_INDEX)];
	st->curr_idx =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      CURRENT_INDEX, val);

	/* PN bits */
	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 PN_31_0)];
	st->pn_31_0 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      PN_31_0, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 PN_63_32)];
	st->pn_63_32 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      PN_63_32, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 PN_95_64)];
	st->pn_95_64 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      PN_95_64, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 PN_127_96)];
	st->pn_127_96 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      PN_127_96, val);

	/* timestamps */
	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 LAST_RX_ENQUEUE_TIMESTAMP)];
	st->last_rx_enq_tstamp =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      LAST_RX_ENQUEUE_TIMESTAMP, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 LAST_RX_DEQUEUE_TIMESTAMP)];
	st->last_rx_deq_tstamp =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      LAST_RX_DEQUEUE_TIMESTAMP, val);

	/* rx bitmap */
	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_31_0)];
	st->rx_bitmap_31_0 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_31_0, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_63_32)];
	st->rx_bitmap_63_32 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_63_32, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_95_64)];
	st->rx_bitmap_95_64 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_95_64, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_127_96)];
	st->rx_bitmap_127_96 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_127_96, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_159_128)];
	st->rx_bitmap_159_128 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_159_128, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_191_160)];
	st->rx_bitmap_191_160 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_191_160, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_223_192)];
	st->rx_bitmap_223_192 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_223_192, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 RX_BITMAP_255_224)];
	st->rx_bitmap_255_224 =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      RX_BITMAP_255_224, val);

	/* various counts */
	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 CURRENT_MPDU_COUNT)];
	st->curr_mpdu_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      CURRENT_MPDU_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 CURRENT_MSDU_COUNT)];
	st->curr_msdu_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      CURRENT_MSDU_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 TIMEOUT_COUNT)];
	st->fwd_timeout_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      TIMEOUT_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 FORWARD_DUE_TO_BAR_COUNT)];
	st->fwd_bar_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      FORWARD_DUE_TO_BAR_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 DUPLICATE_COUNT)];
	st->dup_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      DUPLICATE_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 FRAMES_IN_ORDER_COUNT)];
	st->frms_in_order_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      FRAMES_IN_ORDER_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 BAR_RECEIVED_COUNT)];
	st->bar_rcvd_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      BAR_RECEIVED_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 MPDU_FRAMES_PROCESSED_COUNT)];
	st->mpdu_frms_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      MPDU_FRAMES_PROCESSED_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 MSDU_FRAMES_PROCESSED_COUNT)];
	st->msdu_frms_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      MSDU_FRAMES_PROCESSED_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 TOTAL_PROCESSED_BYTE_COUNT)];
	st->total_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      TOTAL_PROCESSED_BYTE_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 LATE_RECEIVE_MPDU_COUNT)];
	st->late_recv_mpdu_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      LATE_RECEIVE_MPDU_COUNT, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 WINDOW_JUMP_2K)];
	st->win_jump_2k =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      WINDOW_JUMP_2K, val);

	val = reo_desc[HAL_OFFSET_QW(REO_GET_QUEUE_STATS_STATUS,
					 HOLE_COUNT)];
	st->hole_cnt =
		HAL_GET_FIELD(REO_GET_QUEUE_STATS_STATUS,
			      HOLE_COUNT, val);
}

void
hal_reo_flush_queue_status_be(hal_ring_desc_t ring_desc,
			      void *st_handle,
			      hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_flush_queue_status *st =
			(struct hal_reo_flush_queue_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;
	uint64_t val;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc, HAL_REO_FLUSH_QUEUE_STATUS_TLV,
				  &(st->header), hal_soc);

	/* error bit */
	val = reo_desc[HAL_OFFSET(REO_FLUSH_QUEUE_STATUS,
					 ERROR_DETECTED)];
	st->error = HAL_GET_FIELD(REO_FLUSH_QUEUE_STATUS, ERROR_DETECTED,
				  val);
}

void
hal_reo_flush_cache_status_be(hal_ring_desc_t ring_desc,
			      void *st_handle,
			      hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_flush_cache_status *st =
			(struct hal_reo_flush_cache_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;
	uint64_t val;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc, HAL_REO_FLUSH_CACHE_STATUS_TLV,
				  &(st->header), hal_soc);

	/* error bit */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
					 ERROR_DETECTED)];
	st->error = HAL_GET_FIELD(REO_FLUSH_QUEUE_STATUS, ERROR_DETECTED,
				  val);

	/* block error */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
					 BLOCK_ERROR_DETAILS)];
	st->block_error = HAL_GET_FIELD(REO_FLUSH_CACHE_STATUS,
					BLOCK_ERROR_DETAILS,
					val);
	if (!st->block_error)
		qdf_set_bit(hal_soc->index,
			    (unsigned long *)&hal_soc->reo_res_bitmap);

	/* cache flush status */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
					 CACHE_CONTROLLER_FLUSH_STATUS_HIT)];
	st->cache_flush_status = HAL_GET_FIELD(REO_FLUSH_CACHE_STATUS,
					CACHE_CONTROLLER_FLUSH_STATUS_HIT,
					val);

	/* cache flush descriptor type */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
				  CACHE_CONTROLLER_FLUSH_STATUS_DESC_TYPE)];
	st->cache_flush_status_desc_type =
		HAL_GET_FIELD(REO_FLUSH_CACHE_STATUS,
			      CACHE_CONTROLLER_FLUSH_STATUS_DESC_TYPE,
			      val);

	/* cache flush count */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_CACHE_STATUS,
				  CACHE_CONTROLLER_FLUSH_COUNT)];
	st->cache_flush_cnt =
		HAL_GET_FIELD(REO_FLUSH_CACHE_STATUS,
			      CACHE_CONTROLLER_FLUSH_COUNT,
			      val);
}

void
hal_reo_unblock_cache_status_be(hal_ring_desc_t ring_desc,
				hal_soc_handle_t hal_soc_hdl,
				void *st_handle)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_unblk_cache_status *st =
			(struct hal_reo_unblk_cache_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;
	uint64_t val;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc, HAL_REO_UNBLK_CACHE_STATUS_TLV,
				  &st->header, hal_soc);

	/* error bit */
	val = reo_desc[HAL_OFFSET_QW(REO_UNBLOCK_CACHE_STATUS,
				  ERROR_DETECTED)];
	st->error = HAL_GET_FIELD(REO_UNBLOCK_CACHE_STATUS,
				  ERROR_DETECTED,
				  val);

	/* unblock type */
	val = reo_desc[HAL_OFFSET_QW(REO_UNBLOCK_CACHE_STATUS,
				  UNBLOCK_TYPE)];
	st->unblock_type = HAL_GET_FIELD(REO_UNBLOCK_CACHE_STATUS,
					 UNBLOCK_TYPE,
					 val);

	if (!st->error && (st->unblock_type == UNBLOCK_RES_INDEX))
		qdf_clear_bit(hal_soc->index,
			      (unsigned long *)&hal_soc->reo_res_bitmap);
}

void hal_reo_flush_timeout_list_status_be(hal_ring_desc_t ring_desc,
					  void *st_handle,
					  hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_flush_timeout_list_status *st =
			(struct hal_reo_flush_timeout_list_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;
	uint64_t val;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc, HAL_REO_TIMOUT_LIST_STATUS_TLV,
				  &(st->header), hal_soc);

	/* error bit */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_TIMEOUT_LIST_STATUS,
					 ERROR_DETECTED)];
	st->error = HAL_GET_FIELD(REO_FLUSH_TIMEOUT_LIST_STATUS,
				  ERROR_DETECTED,
				  val);

	/* list empty */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_TIMEOUT_LIST_STATUS,
					 TIMOUT_LIST_EMPTY)];
	st->list_empty = HAL_GET_FIELD(REO_FLUSH_TIMEOUT_LIST_STATUS,
				       TIMOUT_LIST_EMPTY,
				       val);

	/* release descriptor count */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_TIMEOUT_LIST_STATUS,
					 RELEASE_DESC_COUNT)];
	st->rel_desc_cnt = HAL_GET_FIELD(REO_FLUSH_TIMEOUT_LIST_STATUS,
					 RELEASE_DESC_COUNT,
					 val);

	/* forward buf count */
	val = reo_desc[HAL_OFFSET_QW(REO_FLUSH_TIMEOUT_LIST_STATUS,
					 FORWARD_BUF_COUNT)];
	st->fwd_buf_cnt = HAL_GET_FIELD(REO_FLUSH_TIMEOUT_LIST_STATUS,
					FORWARD_BUF_COUNT,
					val);
}

void hal_reo_desc_thres_reached_status_be(hal_ring_desc_t ring_desc,
					  void *st_handle,
					  hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_desc_thres_reached_status *st =
			(struct hal_reo_desc_thres_reached_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;
	uint64_t val;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc,
				  HAL_REO_DESC_THRES_STATUS_TLV,
				  &(st->header), hal_soc);

	/* threshold index */
	val = reo_desc[HAL_OFFSET_QW(
				 REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				 THRESHOLD_INDEX)];
	st->thres_index = HAL_GET_FIELD(
				REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				THRESHOLD_INDEX,
				val);

	/* link desc counters */
	val = reo_desc[HAL_OFFSET_QW(
				 REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				 LINK_DESCRIPTOR_COUNTER0)];
	st->link_desc_counter0 = HAL_GET_FIELD(
				REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				LINK_DESCRIPTOR_COUNTER0,
				val);

	val = reo_desc[HAL_OFFSET_QW(
				 REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				 LINK_DESCRIPTOR_COUNTER1)];
	st->link_desc_counter1 = HAL_GET_FIELD(
				REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				LINK_DESCRIPTOR_COUNTER1,
				val);

	val = reo_desc[HAL_OFFSET_QW(
				 REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				 LINK_DESCRIPTOR_COUNTER2)];
	st->link_desc_counter2 = HAL_GET_FIELD(
				REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				LINK_DESCRIPTOR_COUNTER2,
				val);

	val = reo_desc[HAL_OFFSET_QW(
				 REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				 LINK_DESCRIPTOR_COUNTER_SUM)];
	st->link_desc_counter_sum = HAL_GET_FIELD(
				REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS,
				LINK_DESCRIPTOR_COUNTER_SUM,
				val);
}

void
hal_reo_rx_update_queue_status_be(hal_ring_desc_t ring_desc,
				  void *st_handle,
				  hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	struct hal_reo_update_rx_queue_status *st =
			(struct hal_reo_update_rx_queue_status *)st_handle;
	uint64_t *reo_desc = (uint64_t *)ring_desc;

	/*
	 * Offsets of descriptor fields defined in HW headers start
	 * from the field after TLV header
	 */
	reo_desc += HAL_GET_NUM_QWORDS(sizeof(struct tlv_32_hdr));

	/* header */
	hal_reo_status_get_header(ring_desc,
				  HAL_REO_UPDATE_RX_QUEUE_STATUS_TLV,
				  &(st->header), hal_soc);
}

uint8_t hal_get_tlv_hdr_size_be(void)
{
	return sizeof(struct tlv_32_hdr);
}

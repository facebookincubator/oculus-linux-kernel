/*
 * Copyright (c) 2017-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _HAL_REO_BE_H_
#define _HAL_REO_BE_H_

#include "hal_be_hw_headers.h"
#include "hal_rx.h"
#include "hal_reo.h"

#define HAL_REO_QUEUE_EXT_DESC 10
#define HAL_MAX_REO2SW_RINGS 8
#define HAL_NUM_RX_RING_PER_IX_MAP 8

#if defined(QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT) && \
	defined(RX_REO_QUEUE_STATISTICS_COUNTER_INDEX_MASK)
static inline void hal_update_stats_counter_index(uint32_t *reo_queue_desc,
						  uint8_t vdev_stats_id)
{
	HAL_DESC_SET_FIELD(reo_queue_desc, RX_REO_QUEUE,
			   STATISTICS_COUNTER_INDEX, vdev_stats_id);
}
#else
static inline void hal_update_stats_counter_index(uint32_t *reo_queue_desc,
						  uint8_t vdev_stats_id)
{
}
#endif

/* Proto-types */
uint32_t hal_get_reo_reg_base_offset_be(void);

int hal_reo_send_cmd_be(hal_soc_handle_t hal_soc_hdl,
			hal_ring_handle_t  hal_ring_hdl,
			enum hal_reo_cmd_type cmd,
			void *params);

/* REO status ring routines */
void
hal_reo_queue_stats_status_be(hal_ring_desc_t ring_desc,
			      void *st_handle,
			      hal_soc_handle_t hal_soc_hdl);
void
hal_reo_flush_queue_status_be(hal_ring_desc_t ring_desc,
			      void *st_handle,
			      hal_soc_handle_t hal_soc_hdl);
void
hal_reo_flush_cache_status_be(hal_ring_desc_t ring_desc,
			      void *st_handle,
			      hal_soc_handle_t hal_soc_hdl);
void
hal_reo_unblock_cache_status_be(hal_ring_desc_t ring_desc,
				hal_soc_handle_t hal_soc_hdl,
				void *st_handle);
void hal_reo_flush_timeout_list_status_be(hal_ring_desc_t ring_desc,
					  void *st_handle,
					  hal_soc_handle_t hal_soc_hdl);
void hal_reo_desc_thres_reached_status_be(hal_ring_desc_t ring_desc,
					  void *st_handle,
					  hal_soc_handle_t hal_soc_hdl);
void
hal_reo_rx_update_queue_status_be(hal_ring_desc_t ring_desc,
				  void *st_handle,
				  hal_soc_handle_t hal_soc_hdl);

/**
 * hal_reo_init_cmd_ring_be() - Initialize descriptors of REO command SRNG
 * with command number
 * @hal_soc: Handle to HAL SoC structure
 * @hal_ring: Handle to HAL SRNG structure
 *
 * Return: none
 */
void hal_reo_init_cmd_ring_be(hal_soc_handle_t hal_soc_hdl,
			      hal_ring_handle_t hal_ring_hdl);
uint8_t hal_get_tlv_hdr_size_be(void);
#endif /* _HAL_REO_BE_H_ */

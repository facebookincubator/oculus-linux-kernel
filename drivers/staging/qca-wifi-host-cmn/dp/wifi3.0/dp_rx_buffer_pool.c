/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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

#include "dp_rx_buffer_pool.h"
#include "dp_ipa.h"

#ifndef DP_RX_BUFFER_POOL_SIZE
#define DP_RX_BUFFER_POOL_SIZE 128
#endif

#ifndef DP_RX_REFILL_BUFF_POOL_SIZE
#define DP_RX_REFILL_BUFF_POOL_SIZE 2048
#endif

#ifndef DP_RX_REFILL_BUFF_POOL_BURST
#define DP_RX_REFILL_BUFF_POOL_BURST 64
#endif

#ifndef DP_RX_BUFF_POOL_ALLOC_THRES
#define DP_RX_BUFF_POOL_ALLOC_THRES 1
#endif

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
bool dp_rx_buffer_pool_refill(struct dp_soc *soc, qdf_nbuf_t nbuf, u8 mac_id)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	struct rx_buff_pool *bufpool = &soc->rx_buff_pool[mac_id];
	qdf_nbuf_t next_nbuf, first_nbuf, refill_nbuf;
	bool consumed = false;

	if (!bufpool->is_initialized || !pdev)
		return consumed;

	/* process only buffers of RXDMA ring */
	if (qdf_unlikely(rx_desc_pool !=
			 dp_rx_get_mon_desc_pool(soc, mac_id, pdev->pdev_id)))
		return consumed;

	first_nbuf = nbuf;

	while (nbuf) {
		next_nbuf = qdf_nbuf_next(nbuf);

		if (qdf_likely(qdf_nbuf_queue_head_qlen(&bufpool->emerg_nbuf_q) >=
		    DP_RX_BUFFER_POOL_SIZE))
			break;

		refill_nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
					     RX_BUFFER_RESERVATION,
					     rx_desc_pool->buf_alignment,
					     FALSE);

		/* Failed to allocate new nbuf, reset and place it back
		 * in to the pool.
		 */
		if (!refill_nbuf) {
			DP_STATS_INC(pdev,
				     rx_buffer_pool.num_bufs_consumed, 1);
			consumed = true;
			break;
		}

		/* Successful allocation!! */
		DP_STATS_INC(pdev,
			     rx_buffer_pool.num_bufs_alloc_success, 1);
		qdf_nbuf_queue_head_enqueue_tail(&bufpool->emerg_nbuf_q,
						 refill_nbuf);
		nbuf = next_nbuf;
	}

	nbuf = first_nbuf;
	if (consumed) {
		/* Free the MSDU/scattered MSDU */
		while (nbuf) {
			next_nbuf = qdf_nbuf_next(nbuf);
			dp_rx_buffer_pool_nbuf_free(soc, nbuf, mac_id);
			nbuf = next_nbuf;
		}
	}

	return consumed;
}

void dp_rx_buffer_pool_nbuf_free(struct dp_soc *soc, qdf_nbuf_t nbuf, u8 mac_id)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct rx_desc_pool *rx_desc_pool;
	struct rx_buff_pool *buff_pool;

	if (!wlan_cfg_per_pdev_lmac_ring(soc->wlan_cfg_ctx))
		mac_id = dp_pdev->lmac_id;

	rx_desc_pool = &soc->rx_desc_buf[mac_id];
	buff_pool = &soc->rx_buff_pool[mac_id];

	if (qdf_likely(qdf_nbuf_queue_head_qlen(&buff_pool->emerg_nbuf_q) >=
		       DP_RX_BUFFER_POOL_SIZE) ||
	    !buff_pool->is_initialized)
		return qdf_nbuf_free(nbuf);

	qdf_nbuf_reset(nbuf, RX_BUFFER_RESERVATION,
		       rx_desc_pool->buf_alignment);
	qdf_nbuf_queue_head_enqueue_tail(&buff_pool->emerg_nbuf_q, nbuf);
}

void dp_rx_refill_buff_pool_enqueue(struct dp_soc *soc)
{
	struct rx_desc_pool *rx_desc_pool;
	struct rx_refill_buff_pool *buff_pool;
	struct dp_pdev *dp_pdev;
	qdf_nbuf_t nbuf;
	QDF_STATUS ret;
	int count, i;
	qdf_nbuf_t nbuf_head;
	qdf_nbuf_t nbuf_tail;
	uint32_t num_req_refill;

	if (!soc)
		return;

	buff_pool = &soc->rx_refill_buff_pool;
	if (!buff_pool->is_initialized)
		return;

	rx_desc_pool = &soc->rx_desc_buf[0];
	dp_pdev = dp_get_pdev_for_lmac_id(soc, 0);

	num_req_refill = buff_pool->max_bufq_len - buff_pool->bufq_len;

	while (num_req_refill) {
		if (num_req_refill > DP_RX_REFILL_BUFF_POOL_BURST)
			num_req_refill = DP_RX_REFILL_BUFF_POOL_BURST;

		count = 0;
		nbuf_head = NULL;
		nbuf_tail = NULL;
		for (i = 0; i < num_req_refill; i++) {
			nbuf = qdf_nbuf_alloc(soc->osdev,
					      rx_desc_pool->buf_size,
					      RX_BUFFER_RESERVATION,
					      rx_desc_pool->buf_alignment,
					      FALSE);
			if (!nbuf)
				continue;

			ret = qdf_nbuf_map_nbytes_single(soc->osdev, nbuf,
							 QDF_DMA_FROM_DEVICE,
							 rx_desc_pool->buf_size);
			if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
				qdf_nbuf_free(nbuf);
				continue;
			}

			DP_RX_LIST_APPEND(nbuf_head, nbuf_tail, nbuf);
			count++;
		}
		if (count) {
			qdf_spin_lock_bh(&buff_pool->bufq_lock);
			DP_RX_MERGE_TWO_LIST(buff_pool->buf_head,
					     buff_pool->buf_tail,
					     nbuf_head, nbuf_tail);
			buff_pool->bufq_len += count;

			num_req_refill = buff_pool->max_bufq_len -
				buff_pool->bufq_len;
			qdf_spin_unlock_bh(&buff_pool->bufq_lock);

			DP_STATS_INC(dp_pdev,
				     rx_refill_buff_pool.num_bufs_refilled,
				     count);
		}
	}
}

static inline qdf_nbuf_t dp_rx_refill_buff_pool_dequeue_nbuf(struct dp_soc *soc)
{
	qdf_nbuf_t nbuf = NULL;
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;

	if (!buff_pool->in_rx_refill_lock || !buff_pool->bufq_len)
		return nbuf;

	nbuf = buff_pool->buf_head;
	buff_pool->buf_head = qdf_nbuf_next(buff_pool->buf_head);
	qdf_nbuf_set_next(nbuf, NULL);
	buff_pool->bufq_len--;

	return nbuf;
}

qdf_nbuf_t
dp_rx_buffer_pool_nbuf_alloc(struct dp_soc *soc, uint32_t mac_id,
			     struct rx_desc_pool *rx_desc_pool,
			     uint32_t num_available_buffers)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct rx_buff_pool *buff_pool;
	struct dp_srng *dp_rxdma_srng;
	qdf_nbuf_t nbuf;

	nbuf = dp_rx_refill_buff_pool_dequeue_nbuf(soc);
	if (qdf_likely(nbuf)) {
		DP_STATS_INC(dp_pdev,
			     rx_refill_buff_pool.num_bufs_allocated, 1);
		return nbuf;
	}

	if (!wlan_cfg_per_pdev_lmac_ring(soc->wlan_cfg_ctx))
		mac_id = dp_pdev->lmac_id;

	buff_pool = &soc->rx_buff_pool[mac_id];
	dp_rxdma_srng = &soc->rx_refill_buf_ring[mac_id];

	nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
			      RX_BUFFER_RESERVATION,
			      rx_desc_pool->buf_alignment,
			      FALSE);

	if (!buff_pool->is_initialized)
		return nbuf;

	if (qdf_likely(nbuf)) {
		buff_pool->nbuf_fail_cnt = 0;
		return nbuf;
	}

	buff_pool->nbuf_fail_cnt++;

	/* Allocate buffer from the buffer pool */
	if (buff_pool->nbuf_fail_cnt >= DP_RX_BUFFER_POOL_ALLOC_THRES ||
	    (num_available_buffers < dp_rxdma_srng->num_entries / 10)) {
		nbuf = qdf_nbuf_queue_head_dequeue(&buff_pool->emerg_nbuf_q);
		if (nbuf)
			DP_STATS_INC(dp_pdev,
				     rx_buffer_pool.num_pool_bufs_replenish, 1);
	}

	return nbuf;
}

QDF_STATUS
dp_rx_buffer_pool_nbuf_map(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (!QDF_NBUF_CB_PADDR((nbuf_frag_info_t->virt_addr).nbuf))
		ret = qdf_nbuf_map_nbytes_single(soc->osdev,
						 (nbuf_frag_info_t->virt_addr).nbuf,
						 QDF_DMA_FROM_DEVICE,
						 rx_desc_pool->buf_size);

	return ret;
}

static void dp_rx_refill_buff_pool_init(struct dp_soc *soc, u8 mac_id)
{
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	qdf_nbuf_t nbuf;
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;
	QDF_STATUS ret;
	int i;

	if (!wlan_cfg_is_rx_refill_buffer_pool_enabled(soc->wlan_cfg_ctx)) {
		dp_err("RX refill buffer pool support is disabled");
		buff_pool->is_initialized = false;
		return;
	}

	buff_pool->bufq_len = 0;
	buff_pool->buf_head = NULL;
	buff_pool->buf_tail = NULL;
	buff_pool->max_bufq_len = DP_RX_REFILL_BUFF_POOL_SIZE;
	qdf_spinlock_create(&buff_pool->bufq_lock);

	for (i = 0; i < buff_pool->max_bufq_len; i++) {
		nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
				      RX_BUFFER_RESERVATION,
				      rx_desc_pool->buf_alignment, FALSE);
		if (!nbuf)
			continue;

		ret = qdf_nbuf_map_nbytes_single(soc->osdev, nbuf,
						 QDF_DMA_FROM_DEVICE,
						 rx_desc_pool->buf_size);
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
			qdf_nbuf_free(nbuf);
			continue;
		}

		DP_RX_LIST_APPEND(buff_pool->buf_head,
				  buff_pool->buf_tail, nbuf);
		buff_pool->bufq_len++;
	}

	dp_info("RX refill buffer pool required allocation: %u actual allocation: %u",
		buff_pool->max_bufq_len,
		buff_pool->bufq_len);

	buff_pool->is_initialized = true;
}

void dp_rx_buffer_pool_init(struct dp_soc *soc, u8 mac_id)
{
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	struct rx_buff_pool *buff_pool = &soc->rx_buff_pool[mac_id];
	qdf_nbuf_t nbuf;
	int i;

	dp_rx_refill_buff_pool_init(soc, mac_id);

	if (!wlan_cfg_is_rx_buffer_pool_enabled(soc->wlan_cfg_ctx)) {
		dp_err("RX buffer pool support is disabled");
		buff_pool->is_initialized = false;
		return;
	}

	if (buff_pool->is_initialized)
		return;

	qdf_nbuf_queue_head_init(&buff_pool->emerg_nbuf_q);

	for (i = 0; i < DP_RX_BUFFER_POOL_SIZE; i++) {
		nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
				      RX_BUFFER_RESERVATION,
				      rx_desc_pool->buf_alignment, FALSE);
		if (!nbuf)
			continue;
		qdf_nbuf_queue_head_enqueue_tail(&buff_pool->emerg_nbuf_q,
						 nbuf);
	}

	dp_info("RX buffer pool required allocation: %u actual allocation: %u",
		DP_RX_BUFFER_POOL_SIZE,
		qdf_nbuf_queue_head_qlen(&buff_pool->emerg_nbuf_q));

	buff_pool->is_initialized = true;
}

static void dp_rx_refill_buff_pool_deinit(struct dp_soc *soc, u8 mac_id)
{
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	qdf_nbuf_t nbuf;
	uint32_t count = 0;

	if (!buff_pool->is_initialized)
		return;

	buff_pool->in_rx_refill_lock  = true;
	while ((nbuf = dp_rx_refill_buff_pool_dequeue_nbuf(soc))) {
		qdf_nbuf_unmap_nbytes_single(soc->osdev, nbuf,
					     QDF_DMA_BIDIRECTIONAL,
					     rx_desc_pool->buf_size);
		qdf_nbuf_free(nbuf);
		count++;
	}
	buff_pool->in_rx_refill_lock  = false;

	dp_info("Rx refill buffers freed during deinit %u qlen: %u",
		count, buff_pool->bufq_len);

	qdf_spinlock_destroy(&buff_pool->bufq_lock);
	buff_pool->is_initialized = false;
}

void dp_rx_buffer_pool_deinit(struct dp_soc *soc, u8 mac_id)
{
	struct rx_buff_pool *buff_pool = &soc->rx_buff_pool[mac_id];
	qdf_nbuf_t nbuf;

	dp_rx_refill_buff_pool_deinit(soc, mac_id);

	if (!buff_pool->is_initialized)
		return;

	dp_info("buffers in the RX buffer pool during deinit: %u",
		qdf_nbuf_queue_head_qlen(&buff_pool->emerg_nbuf_q));

	while ((nbuf = qdf_nbuf_queue_head_dequeue(&buff_pool->emerg_nbuf_q)))
		qdf_nbuf_free(nbuf);

	buff_pool->is_initialized = false;
}
#endif /* WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL */

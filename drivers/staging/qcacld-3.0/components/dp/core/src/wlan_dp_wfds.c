/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "wlan_dp_wfds.h"
#include "hif.h"
#include "hal_api.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "pld_common.h"
#include "wlan_objmgr_psoc_obj.h"
#include <qdf_mem.h>
#include "wlan_dp_prealloc.h"
#include <htc_api.h>

static struct dp_direct_link_wfds_context *gp_dl_wfds_ctx;

/**
 * dp_wfds_send_config_msg() - Send config message to WFDS QMI server
 * @dl_wfds: Direct Link WFDS context
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_wfds_send_config_msg(struct dp_direct_link_wfds_context *dl_wfds)
{
	struct dp_direct_link_context *direct_link_ctx =
					dl_wfds->direct_link_ctx;
	struct wlan_qmi_wfds_config_req_msg *info;
	struct dp_soc *dp_soc =
		wlan_psoc_get_dp_handle(dl_wfds->direct_link_ctx->dp_ctx->psoc);
	struct hif_opaque_softc *hif_ctx;
	qdf_device_t qdf_dev;
	void *hal_soc;
	struct hal_mem_info mem_info = {0};
	struct hif_direct_link_ce_info ce_info[QMI_WFDS_CE_MAX_SRNG] = {0};
	QDF_STATUS status;
	struct hif_ce_ring_info *srng_info;
	struct hal_srng_params srng_params = {0};
	hal_ring_handle_t refill_ring;
	uint8_t i;

	qdf_dev = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;

	if (!dp_soc || !dp_soc->hif_handle || !qdf_dev)
		return QDF_STATUS_E_FAILURE;

	hif_ctx = dp_soc->hif_handle;

	hal_soc = hif_get_hal_handle(hif_ctx);
	if (!hal_soc)
		return QDF_STATUS_E_FAILURE;

	hal_get_meminfo(hal_soc, &mem_info);

	info = qdf_mem_malloc(sizeof(*info));
	if (!info)
		return QDF_STATUS_E_NOMEM;

	info->shadow_rdptr_mem_paddr =
				(uint64_t)mem_info.shadow_rdptr_mem_paddr;
	info->shadow_rdptr_mem_size = sizeof(uint32_t) * HAL_SRNG_ID_MAX;
	info->shadow_wrptr_mem_paddr =
				(uint64_t)mem_info.shadow_wrptr_mem_paddr;
	info->shadow_wrptr_mem_size = sizeof(uint32_t) * HAL_MAX_LMAC_RINGS;
	info->pcie_bar_pa = (uint64_t)mem_info.dev_base_paddr;

	dl_wfds->iommu_cfg.shadow_rdptr_paddr = info->shadow_rdptr_mem_paddr;
	dl_wfds->iommu_cfg.shadow_rdptr_map_size = info->shadow_rdptr_mem_size;
	dl_wfds->iommu_cfg.shadow_wrptr_paddr = info->shadow_wrptr_mem_paddr;
	dl_wfds->iommu_cfg.shadow_wrptr_map_size = info->shadow_wrptr_mem_size;

	pld_audio_smmu_map(qdf_dev->dev,
			   qdf_mem_paddr_from_dmaaddr(qdf_dev,
						      info->shadow_rdptr_mem_paddr),
			   info->shadow_rdptr_mem_paddr,
			   info->shadow_rdptr_mem_size);
	pld_audio_smmu_map(qdf_dev->dev,
			   qdf_mem_paddr_from_dmaaddr(qdf_dev,
						      info->shadow_wrptr_mem_paddr),
			   info->shadow_wrptr_mem_paddr,
			   info->shadow_wrptr_mem_size);

	info->ce_info_len = QMI_WFDS_CE_MAX_SRNG;
	status = hif_get_direct_link_ce_srng_info(hif_ctx, ce_info,
						  QMI_WFDS_CE_MAX_SRNG);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Direct Link CE srng info get failed");
		qdf_mem_free(info);
		return status;
	}

	for (i = 0; i < QMI_WFDS_CE_MAX_SRNG; i++) {
		info->ce_info[i].ce_id = ce_info[i].ce_id;
		info->ce_info[i].ce_dir = ce_info[i].pipe_dir;

		srng_info = &ce_info[i].ring_info;
		info->ce_info[i].srng_info.ring_id = srng_info->ring_id;
		info->ce_info[i].srng_info.dir = srng_info->ring_dir;
		info->ce_info[i].srng_info.num_entries = srng_info->num_entries;
		info->ce_info[i].srng_info.entry_size = srng_info->entry_size;
		info->ce_info[i].srng_info.ring_base_paddr =
						     srng_info->ring_base_paddr;
		info->ce_info[i].srng_info.hp_paddr = srng_info->hp_paddr;
		info->ce_info[i].srng_info.tp_paddr = srng_info->tp_paddr;

		dl_wfds->iommu_cfg.direct_link_srng_ring_base_paddr[i] =
			srng_info->ring_base_paddr;
		dl_wfds->iommu_cfg.direct_link_srng_ring_map_size[i] =
			srng_info->entry_size * srng_info->num_entries * 4;

		pld_audio_smmu_map(qdf_dev->dev,
				   qdf_mem_paddr_from_dmaaddr(qdf_dev,
							      srng_info->ring_base_paddr),
				   srng_info->ring_base_paddr,
				   dl_wfds->iommu_cfg.direct_link_srng_ring_map_size[i]);
	}

	refill_ring = direct_link_ctx->direct_link_refill_ring_hdl->hal_srng;
	hal_get_srng_params(hal_soc, refill_ring, &srng_params);
	info->rx_refill_ring.ring_id = srng_params.ring_id;
	info->rx_refill_ring.dir =
		(srng_params.ring_dir == HAL_SRNG_SRC_RING) ?
		QMI_WFDS_SRNG_SOURCE_RING : QMI_WFDS_SRNG_DESTINATION_RING;
	info->rx_refill_ring.num_entries = srng_params.num_entries;
	info->rx_refill_ring.entry_size = srng_params.entry_size;
	info->rx_refill_ring.ring_base_paddr = srng_params.ring_base_paddr;

	dl_wfds->iommu_cfg.direct_link_refill_ring_base_paddr = srng_params.ring_base_paddr;
	dl_wfds->iommu_cfg.direct_link_refill_ring_map_size =
		srng_params.entry_size * srng_params.num_entries * 4;

	pld_audio_smmu_map(qdf_dev->dev,
			   qdf_mem_paddr_from_dmaaddr(qdf_dev,
						      srng_params.ring_base_paddr),
			   srng_params.ring_base_paddr,
			   dl_wfds->iommu_cfg.direct_link_refill_ring_map_size);

	info->rx_refill_ring.hp_paddr =
				hal_srng_get_hp_addr(hal_soc, refill_ring);
	info->rx_refill_ring.tp_paddr =
				hal_srng_get_tp_addr(hal_soc, refill_ring);

	info->rx_pkt_tlv_len = dp_soc->rx_pkt_tlv_size;
	info->rx_rbm = dp_rx_get_rx_bm_id(dp_soc);
	info->pci_slot = pld_get_pci_slot(qdf_dev->dev);
	qdf_assert(info.pci_slot >= 0);
	info->lpass_ep_id = direct_link_ctx->lpass_ep_id;

	status = wlan_qmi_wfds_send_config_msg(direct_link_ctx->dp_ctx->psoc,
					       info);
	qdf_mem_free(info);

	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Configuration message send failed %d", status);
		return status;
	}

	qdf_atomic_set(&dl_wfds->wfds_state,
		       DP_WFDS_SVC_CONFIG_DONE);

	return status;
}

/**
 * dp_wfds_req_mem_msg() - Send Request Memory message to QMI server
 * @dl_wfds: Direct Link QMI context
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_wfds_req_mem_msg(struct dp_direct_link_wfds_context *dl_wfds)
{
	struct wlan_qmi_wfds_mem_req_msg *info;
	struct dp_soc *dp_soc =
		wlan_psoc_get_dp_handle(dl_wfds->direct_link_ctx->dp_ctx->psoc);
	struct hif_opaque_softc *hif_ctx;
	qdf_device_t qdf_dev;
	QDF_STATUS status;
	struct qdf_mem_multi_page_t *pages;
	uint16_t num_pages;
	uint8_t i;

	qdf_dev = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;

	if (!dl_wfds || !dp_soc || !dp_soc->hif_handle || !qdf_dev)
		return QDF_STATUS_E_NOSUPPORT;

	hif_ctx = dp_soc->hif_handle;

	info = qdf_mem_malloc(sizeof(*info));
	if (!info)
		return QDF_STATUS_E_NOMEM;

	info->mem_arena_page_info_len = dl_wfds->num_mem_arenas;
	for (i = 0; i < dl_wfds->num_mem_arenas; i++) {
		if (i == QMI_WFDS_MEM_ARENA_CE_RX_MSG_BUFFERS) {
			uint64_t *dma_addr = NULL;
			uint32_t buf_size;

			num_pages =
			    hif_get_direct_link_ce_dest_srng_buffers(hif_ctx,
								     &dma_addr,
								     &buf_size);
			qdf_assert(dma_addr);

			info->mem_arena_page_info[i].num_entries_per_page =
					qdf_page_size / buf_size;
			info->mem_arena_page_info[i].page_dma_addr_len =
								      num_pages;
			while (num_pages--) {
				info->mem_arena_page_info[i].page_dma_addr[num_pages] =
							dma_addr[num_pages];
				pld_audio_smmu_map(qdf_dev->dev,
						   qdf_mem_paddr_from_dmaaddr(qdf_dev, dma_addr[num_pages]),
						   dma_addr[num_pages],
						   buf_size);
			}

			qdf_mem_free(dma_addr);
			continue;
		}

		num_pages = dl_wfds->mem_arena_pages[i].num_pages;
		pages = &dl_wfds->mem_arena_pages[i];

		info->mem_arena_page_info[i].num_entries_per_page =
		       dl_wfds->mem_arena_pages[i].num_element_per_page;
		info->mem_arena_page_info[i].page_dma_addr_len = num_pages;

		while (num_pages--) {
			info->mem_arena_page_info[i].page_dma_addr[num_pages] =
					pages->dma_pages[num_pages].page_p_addr;

			pld_audio_smmu_map(qdf_dev->dev,
					qdf_mem_paddr_from_dmaaddr(qdf_dev, pages->dma_pages[num_pages].page_p_addr),
					pages->dma_pages[num_pages].page_p_addr,
					pages->page_size);
		}
	}

	status = wlan_qmi_wfds_send_req_mem_msg(
					dl_wfds->direct_link_ctx->dp_ctx->psoc,
					info);
	qdf_mem_free(info);

	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Memory request message send failed %d", status);
		return status;
	}

	qdf_atomic_set(&dl_wfds->wfds_state,
		       DP_WFDS_SVC_MEM_CONFIG_DONE);

	return status;
}

/**
 * dp_wfds_ipcc_map_n_cfg_msg() - Send the IPCC map and configure message
 *  to QMI server
 * @dlink_wfds: Direct Link QMI context
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_wfds_ipcc_map_n_cfg_msg(struct dp_direct_link_wfds_context *dlink_wfds)
{
	struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg info = {0};
	QDF_STATUS status;

	info.status = QMI_WFDS_STATUS_SUCCESS;

	status = wlan_qmi_wfds_ipcc_map_n_cfg_msg(
				dlink_wfds->direct_link_ctx->dp_ctx->psoc,
				&info);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("IPCC map n cfg message send failed %d", status);
		return status;
	}

	qdf_atomic_set(&dlink_wfds->wfds_state,
		       DP_WFDS_SVC_IPCC_MAP_N_CFG_DONE);

	return status;
}

/**
 * dp_wfds_work() - DP WFDS work handler
 * @arg: direct link QMI context
 *
 * Return: None
 */
static void dp_wfds_work(void *arg)
{
	struct dp_direct_link_wfds_context *dl_wfds = arg;
	struct dp_wfds_event *wfds_evt;

	dp_debug("entry");

	qdf_spinlock_acquire(&dl_wfds->wfds_event_list_lock);
	while ((wfds_evt =
		qdf_list_first_entry_or_null(&dl_wfds->wfds_event_list,
					     struct dp_wfds_event,
					     list_node))) {
		qdf_list_remove_node(&dl_wfds->wfds_event_list,
				     &wfds_evt->list_node);
		qdf_spinlock_release(&dl_wfds->wfds_event_list_lock);

		switch (wfds_evt->wfds_evt_type) {
		case DP_WFDS_NEW_SERVER:
			dp_wfds_send_config_msg(dl_wfds);
			break;
		case DP_WFDS_MEM_REQ:
			dp_wfds_req_mem_msg(dl_wfds);
			break;
		case DP_WFDS_IPCC_MAP_N_CFG:
			dp_wfds_ipcc_map_n_cfg_msg(dl_wfds);
			break;
		default:
			break;
		}

		qdf_mem_free(wfds_evt);

		qdf_spinlock_acquire(&dl_wfds->wfds_event_list_lock);
	}
	qdf_spinlock_release(&dl_wfds->wfds_event_list_lock);

	dp_debug("exit");
}

/**
 * dp_wfds_event_post() - Post WFDS event to be processed in worker context
 * @dl_wfds: Direct Link QMI context
 * @wfds_evt_type: QMI event type
 * @data: pointer to QMI data
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_wfds_event_post(struct dp_direct_link_wfds_context *dl_wfds,
		   enum dp_wfds_event_type wfds_evt_type, void *data)
{
	struct dp_wfds_event *wfds_evt;

	wfds_evt = qdf_mem_malloc(sizeof(*wfds_evt));
	if (!wfds_evt)
		return QDF_STATUS_E_NOMEM;

	wfds_evt->wfds_evt_type = wfds_evt_type;
	wfds_evt->data = data;

	qdf_spinlock_acquire(&dl_wfds->wfds_event_list_lock);
	qdf_list_insert_back(&dl_wfds->wfds_event_list,
			     &wfds_evt->list_node);
	qdf_spinlock_release(&dl_wfds->wfds_event_list_lock);

	qdf_queue_work(0, dl_wfds->wfds_wq, &dl_wfds->wfds_work);

	return QDF_STATUS_SUCCESS;
}

#ifdef DP_MEM_PRE_ALLOC
/**
 * dp_wfds_get_desc_type_from_mem_arena() - Get descriptor type for the memory
 *  arena
 * @mem_arena: memory arena
 *
 * Return: DP descriptor type
 */
static uint32_t
dp_wfds_get_desc_type_from_mem_arena(enum wlan_qmi_wfds_mem_arenas mem_arena)
{
	switch (mem_arena) {
	case QMI_WFDS_MEM_ARENA_TX_BUFFERS:
		return QDF_DP_TX_DIRECT_LINK_BUF_TYPE;
	case QMI_WFDS_MEM_ARENA_CE_TX_MSG_BUFFERS:
		return QDF_DP_TX_DIRECT_LINK_CE_BUF_TYPE;
	default:
		dp_debug("No desc type for mem arena %d", mem_arena);
		qdf_assert(0);
		return QDF_DP_DESC_TYPE_MAX;
	}
}

/**
 * dp_wfds_alloc_mem_arena() - Allocate memory for the arena
 * @dl_wfds: Direct Link QMI context
 * @mem_arena: memory arena
 * @entry_size: element size
 * @num_entries: number of elements
 *
 * Return: None
 */
static void
dp_wfds_alloc_mem_arena(struct dp_direct_link_wfds_context *dl_wfds,
			enum wlan_qmi_wfds_mem_arenas mem_arena,
			uint16_t entry_size, uint16_t num_entries)
{
	qdf_device_t qdf_ctx = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;
	uint32_t desc_type;

	desc_type = dp_wfds_get_desc_type_from_mem_arena(mem_arena);

	if (desc_type != QDF_DP_DESC_TYPE_MAX)
		dp_prealloc_get_multi_pages(desc_type, entry_size,
					    num_entries,
					    &dl_wfds->mem_arena_pages[mem_arena],
					    false);

	if (!dl_wfds->mem_arena_pages[mem_arena].num_pages)
		qdf_mem_multi_pages_alloc(qdf_ctx,
					  &dl_wfds->mem_arena_pages[mem_arena],
					  entry_size, num_entries, 0, false);
}

/**
 * dp_wfds_free_mem_arena() - Free memory for the arena
 * @dl_wfds: Direct Link QMI context
 * @mem_arena: memory arena
 *
 * Return: None
 */
static void
dp_wfds_free_mem_arena(struct dp_direct_link_wfds_context *dl_wfds,
		       enum wlan_qmi_wfds_mem_arenas mem_arena)
{
	qdf_device_t qdf_ctx = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;
	uint32_t desc_type;

	if (dl_wfds->mem_arena_pages[mem_arena].is_mem_prealloc) {
		desc_type = dp_wfds_get_desc_type_from_mem_arena(mem_arena);
		dp_prealloc_put_multi_pages(desc_type,
					  &dl_wfds->mem_arena_pages[mem_arena]);
	} else {
		qdf_mem_multi_pages_free(qdf_ctx,
					 &dl_wfds->mem_arena_pages[mem_arena],
					 0, false);
	}
}
#else
static void
dp_wfds_alloc_mem_arena(struct dp_direct_link_wfds_context *dl_wfds,
			enum wlan_qmi_wfds_mem_arenas mem_arena,
			uint16_t entry_size, uint16_t num_entries)
{
	qdf_device_t qdf_ctx = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;

	qdf_mem_multi_pages_alloc(qdf_ctx,
				  &dl_wfds->mem_arena_pages[mem_arena],
				  entry_size, num_entries, 0, false);
}

static void
dp_wfds_free_mem_arena(struct dp_direct_link_wfds_context *dl_wfds,
		       enum wlan_qmi_wfds_mem_arenas mem_arena)
{
	qdf_device_t qdf_ctx = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;

	qdf_mem_multi_pages_free(qdf_ctx,
				 &dl_wfds->mem_arena_pages[mem_arena],
				 0, false);
}
#endif

void
dp_wfds_handle_request_mem_ind(struct wlan_qmi_wfds_mem_ind_msg *mem_msg)
{
	struct dp_direct_link_wfds_context *dl_wfds = gp_dl_wfds_ctx;
	uint8_t i;

	if (!dl_wfds)
		return;

	dp_debug("Received request mem indication from QMI server");

	dl_wfds->num_mem_arenas = mem_msg->mem_arena_info_len;
	dl_wfds->mem_arena_pages =
		qdf_mem_malloc(sizeof(*dl_wfds->mem_arena_pages) *
			       mem_msg->mem_arena_info_len);
	if (!dl_wfds->mem_arena_pages)
		return;

	for (i = 0; i < dl_wfds->num_mem_arenas; i++) {
		if (i == QMI_WFDS_MEM_ARENA_CE_RX_MSG_BUFFERS)
			continue;

		dp_wfds_alloc_mem_arena(dl_wfds, i,
					mem_msg->mem_arena_info[i].entry_size,
					mem_msg->mem_arena_info[i].num_entries);

		if (!dl_wfds->mem_arena_pages[i].num_pages) {
			while (--i >= 0 &&
			       dl_wfds->mem_arena_pages[i].num_pages)
				dp_wfds_free_mem_arena(dl_wfds, i);

			qdf_mem_free(dl_wfds->mem_arena_pages);
			dl_wfds->mem_arena_pages = NULL;
			dl_wfds->num_mem_arenas = 0;

			return;
		}
	}

	dp_wfds_event_post(dl_wfds, DP_WFDS_MEM_REQ, NULL);
}

void
dp_wfds_handle_ipcc_map_n_cfg_ind(struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg *ipcc_msg)
{
	struct dp_direct_link_wfds_context *dl_wfds = gp_dl_wfds_ctx;
	qdf_device_t qdf_ctx = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;
	struct dp_soc *dp_soc =
		wlan_psoc_get_dp_handle(dl_wfds->direct_link_ctx->dp_ctx->psoc);
	struct hif_opaque_softc *hif_ctx;
	uint8_t i;

	if (!dl_wfds || !qdf_ctx || !dp_soc || !dp_soc->hif_handle)
		return;

	hif_ctx = dp_soc->hif_handle;

	dp_debug("Received IPCC map n cfg indication from QMI server");

	/*
	 * IPCC Address for all the CE srngs will be the same and only the
	 * IPCC data will differ.
	 */
	pld_smmu_map(qdf_ctx->dev, ipcc_msg->ipcc_ce_info[0].ipcc_trig_addr,
		     &dl_wfds->ipcc_dma_addr, sizeof(uint32_t));

	dl_wfds->ipcc_ce_id_len = ipcc_msg->ipcc_ce_info_len;

	for (i = 0; i < ipcc_msg->ipcc_ce_info_len; i++) {
		dl_wfds->ipcc_ce_id[i] = ipcc_msg->ipcc_ce_info[i].ce_id;
		hif_set_irq_config_by_ceid(hif_ctx,
				      ipcc_msg->ipcc_ce_info[i].ce_id,
				      dl_wfds->ipcc_dma_addr,
				      ipcc_msg->ipcc_ce_info[i].ipcc_trig_data);
	}

	dp_wfds_event_post(dl_wfds, DP_WFDS_IPCC_MAP_N_CFG, NULL);
}

QDF_STATUS dp_wfds_new_server(void)
{
	struct dp_direct_link_wfds_context *dl_wfds = gp_dl_wfds_ctx;
	void *htc_handle = cds_get_context(QDF_MODULE_ID_HTC);

	if (!dl_wfds || !htc_handle)
		return QDF_STATUS_E_INVAL;

	qdf_atomic_set(&dl_wfds->wfds_state, DP_WFDS_SVC_CONNECTED);

	htc_vote_link_up(htc_handle, HTC_LINK_VOTE_DIRECT_LINK_USER_ID);
	dp_debug("Connected to WFDS QMI service, state: 0x%x",
		 qdf_atomic_read(&dl_wfds->wfds_state));

	return dp_wfds_event_post(dl_wfds, DP_WFDS_NEW_SERVER, NULL);
}

void dp_wfds_del_server(void)
{
	struct dp_direct_link_wfds_context *dl_wfds = gp_dl_wfds_ctx;
	qdf_device_t qdf_ctx = dl_wfds->direct_link_ctx->dp_ctx->qdf_dev;
	void *htc_handle = cds_get_context(QDF_MODULE_ID_HTC);
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	enum dp_wfds_state dl_wfds_state;
	uint8_t i;
	uint16_t page_idx;

	if (!dl_wfds || !qdf_ctx || !hif_ctx || !htc_handle)
		return;

	dp_debug("WFDS QMI server exiting");

	dl_wfds_state = qdf_atomic_read(&dl_wfds->wfds_state);
	qdf_atomic_set(&dl_wfds->wfds_state,
		       DP_WFDS_SVC_DISCONNECTED);

	if (dl_wfds_state >= DP_WFDS_SVC_IPCC_MAP_N_CFG_DONE &&
	    dl_wfds->ipcc_dma_addr) {
		for (i = 0; i < dl_wfds->ipcc_ce_id_len; i++)
			hif_set_irq_config_by_ceid(hif_ctx,
						   dl_wfds->ipcc_ce_id[i],
						   0, 0);

		pld_smmu_unmap(qdf_ctx->dev, dl_wfds->ipcc_dma_addr,
			       sizeof(uint32_t));
	}

	if (dl_wfds_state >= DP_WFDS_SVC_MEM_CONFIG_DONE) {
		uint64_t *dma_addr = NULL;
		uint16_t num_pages;
		uint32_t buf_size;

		for (i = 0; i < dl_wfds->num_mem_arenas; i++) {
			struct qdf_mem_multi_page_t *mp_info;

			if (!dl_wfds->mem_arena_pages[i].num_pages)
				continue;

			mp_info = &dl_wfds->mem_arena_pages[i];
			for (page_idx = 0; page_idx < mp_info->num_pages;
			     page_idx++)
				pld_audio_smmu_unmap(qdf_ctx->dev,
				       mp_info->dma_pages[page_idx].page_p_addr,
				       mp_info->page_size);

			dp_wfds_free_mem_arena(dl_wfds, i);
		}

		qdf_mem_free(dl_wfds->mem_arena_pages);
		dl_wfds->mem_arena_pages = NULL;
		dl_wfds->num_mem_arenas = 0;

		num_pages = hif_get_direct_link_ce_dest_srng_buffers(hif_ctx,
								     &dma_addr,
								     &buf_size);
		qdf_assert(dma_addr);

		while (num_pages--)
			pld_audio_smmu_unmap(qdf_ctx->dev, dma_addr[num_pages],
					     buf_size);

		qdf_mem_free(dma_addr);
	}

	if (dl_wfds_state >= DP_WFDS_SVC_CONFIG_DONE) {
		pld_audio_smmu_unmap(qdf_ctx->dev,
				     dl_wfds->iommu_cfg.shadow_rdptr_paddr,
				     dl_wfds->iommu_cfg.shadow_rdptr_map_size);
		pld_audio_smmu_unmap(qdf_ctx->dev,
				     dl_wfds->iommu_cfg.shadow_wrptr_paddr,
				     dl_wfds->iommu_cfg.shadow_wrptr_map_size);

		for (i = 0; i < QMI_WFDS_CE_MAX_SRNG; i++)
			pld_audio_smmu_unmap(qdf_ctx->dev,
				dl_wfds->iommu_cfg.direct_link_srng_ring_base_paddr[i],
				dl_wfds->iommu_cfg.direct_link_srng_ring_map_size[i]);

		pld_audio_smmu_unmap(qdf_ctx->dev,
			dl_wfds->iommu_cfg.direct_link_refill_ring_base_paddr,
			dl_wfds->iommu_cfg.direct_link_refill_ring_map_size);
	}

	htc_vote_link_down(htc_handle, HTC_LINK_VOTE_DIRECT_LINK_USER_ID);
}

QDF_STATUS dp_wfds_init(struct dp_direct_link_context *dp_direct_link_ctx)
{
	struct dp_direct_link_wfds_context *dl_wfds;
	QDF_STATUS status;

	dl_wfds = qdf_mem_malloc(sizeof(*dl_wfds));
	if (!dl_wfds) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	qdf_spinlock_create(&dl_wfds->wfds_event_list_lock);
	qdf_list_create(&dl_wfds->wfds_event_list, 0);

	status = qdf_create_work(0, &dl_wfds->wfds_work,
				 dp_wfds_work, dl_wfds);
	if (status != QDF_STATUS_SUCCESS) {
		dp_err("DP QMI work create failed");
		goto wfds_work_create_fail;
	}

	dl_wfds->wfds_wq = qdf_alloc_unbound_workqueue("dp_wfds_wq");
	if (!dl_wfds->wfds_wq) {
		dp_err("DP QMI workqueue allocate failed");
		goto wfds_wq_alloc_fail;
	}

	qdf_atomic_set(&dl_wfds->wfds_state,
		       DP_WFDS_SVC_DISCONNECTED);

	status = wlan_qmi_wfds_init(dp_direct_link_ctx->dp_ctx->psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("WFDS QMI initialization failed %d", status);
		goto qmi_wfds_init_fail;
	}

	dp_direct_link_ctx->dl_wfds = dl_wfds;
	dl_wfds->direct_link_ctx = dp_direct_link_ctx;
	gp_dl_wfds_ctx = dl_wfds;
	dp_debug("WFDS QMI init successful");

	return status;

qmi_wfds_init_fail:
	qdf_flush_workqueue(0, dl_wfds->wfds_wq);
	qdf_destroy_workqueue(0, dl_wfds->wfds_wq);

wfds_wq_alloc_fail:
	qdf_flush_work(&dl_wfds->wfds_work);
	qdf_destroy_work(0, &dl_wfds->wfds_work);

wfds_work_create_fail:
	qdf_spinlock_destroy(&dl_wfds->wfds_event_list_lock);
	qdf_list_destroy(&dl_wfds->wfds_event_list);
	qdf_mem_free(dl_wfds);

out:
	return status;
}

void dp_wfds_deinit(struct dp_direct_link_context *dp_direct_link_ctx,
		    bool is_ssr)
{
	struct dp_direct_link_wfds_context *dl_wfds;

	if (!dp_direct_link_ctx)
		return;

	dl_wfds = dp_direct_link_ctx->dl_wfds;

	dp_debug("WFDS QMI deinit");

	qdf_flush_workqueue(0, dl_wfds->wfds_wq);
	qdf_destroy_workqueue(0, dl_wfds->wfds_wq);

	qdf_flush_work(&dl_wfds->wfds_work);
	qdf_destroy_work(0, &dl_wfds->wfds_work);

	qdf_spinlock_destroy(&dl_wfds->wfds_event_list_lock);
	qdf_list_destroy(&dl_wfds->wfds_event_list);

	if (qdf_atomic_read(&dl_wfds->wfds_state) !=
	    DP_WFDS_SVC_DISCONNECTED)
		wlan_qmi_wfds_send_misc_req_msg(dp_direct_link_ctx->dp_ctx->psoc,
						is_ssr);

	wlan_qmi_wfds_deinit(dp_direct_link_ctx->dp_ctx->psoc);
	gp_dl_wfds_ctx = NULL;

	qdf_mem_free(dl_wfds);
}

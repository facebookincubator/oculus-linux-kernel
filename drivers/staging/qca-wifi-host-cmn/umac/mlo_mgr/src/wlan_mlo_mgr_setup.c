/* Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains MLO manager ap related functionality
 */
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_mlo_mgr_main.h"
#ifdef WLAN_MLO_MULTI_CHIP
#include "wlan_lmac_if_def.h"
#include <cdp_txrx_mlo.h>
#endif
#include <wlan_mgmt_txrx_rx_reo_utils_api.h>

#ifdef WLAN_MLO_MULTI_CHIP
static inline
bool mlo_psoc_get_index_id(struct wlan_objmgr_psoc *psoc,
			   uint8_t grp_id,
			   uint8_t *index,
			   bool teardown)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id;

	if (!mlo_ctx)
		return false;

	if (!psoc)
		return false;

	if (!index)
		return false;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return false;
	}

	for (id = 0; id < mlo_ctx->setup_info[grp_id].tot_socs; id++)
		if (mlo_ctx->setup_info[grp_id].curr_soc_list[id] == psoc) {
			*index = id;
			return true;
		}

	if (teardown)
		return false;

	for (id = 0; id < mlo_ctx->setup_info[grp_id].tot_socs; id++)
		if (!mlo_ctx->setup_info[grp_id].curr_soc_list[id]) {
			*index = id;
			return true;
		}

	return false;
}

bool mlo_psoc_get_grp_id(struct wlan_objmgr_psoc *psoc, uint8_t *ret_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t grp_id;
	uint8_t tot_socs;
	uint8_t id;

	if (!mlo_ctx)
		return false;

	if (!psoc)
		return false;

	if (!ret_id)
		return false;

	for (grp_id = 0; grp_id < mlo_ctx->total_grp; grp_id++) {
		tot_socs = mlo_ctx->setup_info[grp_id].tot_socs;
		for (id = 0; id < tot_socs; id++)
			if (mlo_ctx->setup_info[grp_id].soc_list[id] == psoc) {
				*ret_id = grp_id;
				return true;
			}
	}

	return false;
}

qdf_export_symbol(mlo_psoc_get_grp_id);

bool mlo_is_ml_soc(struct wlan_objmgr_psoc *psoc, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id;

	if (!mlo_ctx)
		return false;

	if (!psoc)
		return false;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return false;
	}

	for (id = 0; id < mlo_ctx->setup_info[grp_id].tot_socs; id++)
		if (mlo_ctx->setup_info[grp_id].curr_soc_list[id] == psoc)
			return true;

	return false;
}

qdf_export_symbol(mlo_is_ml_soc);

static void mlo_set_soc_list(uint8_t grp_id, struct wlan_objmgr_psoc *psoc)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t idx;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	for (idx = 0; idx < mlo_ctx->setup_info[grp_id].tot_socs; idx++) {
		if (mlo_ctx->setup_info[grp_id].soc_id_list[idx] ==
				psoc->soc_objmgr.psoc_id) {
			mlo_ctx->setup_info[grp_id].soc_list[idx] = psoc;
			mlo_wsi_link_info_update_soc(psoc, grp_id);
		}
	}
}

void mlo_get_soc_list(struct wlan_objmgr_psoc **soc_list,
		      uint8_t grp_id,
		      uint8_t total_socs,
		      enum MLO_SOC_LIST curr)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t chip_idx;

	if (!mlo_ctx)
		goto err_case;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		goto err_case;
	}

	if (total_socs != mlo_ctx->setup_info[grp_id].tot_socs) {
		mlo_err("Mismatch in number of socs in the grp id %d, expected %d observed %d",
			grp_id, total_socs,
			mlo_ctx->setup_info[grp_id].tot_socs);
		goto err_case;
	}

	if (curr == WLAN_MLO_GROUP_CURRENT_SOC_LIST) {
		for (chip_idx = 0; chip_idx < total_socs; chip_idx++)
			soc_list[chip_idx] =
			mlo_ctx->setup_info[grp_id].curr_soc_list[chip_idx];
	} else {
		for (chip_idx = 0; chip_idx < total_socs; chip_idx++)
			soc_list[chip_idx] =
				mlo_ctx->setup_info[grp_id].soc_list[chip_idx];
	}

	return;

err_case:
		for (chip_idx = 0; chip_idx < total_socs; chip_idx++)
			soc_list[chip_idx] = NULL;

		return;
}

qdf_export_symbol(mlo_get_soc_list);

void mlo_cleanup_asserted_soc_setup_info(struct wlan_objmgr_psoc *psoc,
					 uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t link_idx;
	struct wlan_objmgr_pdev *pdev;
	struct mlo_setup_info *setup_info;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->num_links)
		return;

	if (!psoc) {
		mlo_info("NULL psoc");
		return;
	}

	for (link_idx = 0; link_idx < MAX_MLO_LINKS; link_idx++) {
		pdev = setup_info->pdev_list[link_idx];
		if (pdev) {
			if (wlan_pdev_get_psoc(pdev) == psoc) {
				setup_info->pdev_list[link_idx] = NULL;
				setup_info->state[link_idx] = MLO_LINK_TEARDOWN;
				setup_info->num_links--;
			}
		}
	}
}

qdf_export_symbol(mlo_cleanup_asserted_soc_setup_info);

void mlo_setup_update_soc_id_list(uint8_t grp_id, uint8_t *soc_id_list)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint32_t tot_socs;
	uint32_t num_soc;
	uint8_t *soc_list;

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	tot_socs = mlo_ctx->setup_info[grp_id].tot_socs;
	soc_list = mlo_ctx->setup_info[grp_id].soc_id_list;

	for (num_soc = 0; num_soc < tot_socs; num_soc++)
		soc_list[num_soc] = soc_id_list[num_soc];
}

qdf_export_symbol(mlo_setup_update_soc_id_list);

uint8_t mlo_setup_get_total_socs(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return 0;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return 0;
	}

	return mlo_ctx->setup_info[grp_id].tot_socs;
}

qdf_export_symbol(mlo_setup_get_total_socs);

void mlo_setup_update_total_socs(uint8_t grp_id, uint8_t tot_socs)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	mlo_ctx->setup_info[grp_id].tot_socs = tot_socs;
	mlo_ctx->setup_info[grp_id].ml_grp_id = grp_id;
	mlo_ctx->setup_info[grp_id].tot_links = 0;
	qdf_info("Grp_id %d Total MLO socs = %d links = %d",
		 grp_id, mlo_ctx->setup_info[grp_id].tot_socs,
		 mlo_ctx->setup_info[grp_id].tot_links);
}

qdf_export_symbol(mlo_setup_update_total_socs);

static QDF_STATUS mlo_find_pdev_idx(struct wlan_objmgr_pdev *pdev,
				    uint8_t *link_idx, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t idx;

	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	if (!link_idx)
		return QDF_STATUS_E_FAILURE;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return QDF_STATUS_E_FAILURE;
	}

	for (idx = 0; idx < mlo_ctx->setup_info[grp_id].tot_links; idx++) {
		if (mlo_ctx->setup_info[grp_id].pdev_list[idx] == pdev) {
			*link_idx = idx;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

bool mlo_check_start_stop_inprogress(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return true;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return true;
	}

	return qdf_atomic_test_and_set_bit(
			START_STOP_INPROGRESS_BIT,
			&mlo_ctx->setup_info[grp_id].start_stop_inprogress);
}

qdf_export_symbol(mlo_check_start_stop_inprogress);

void mlo_clear_start_stop_inprogress(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	qdf_atomic_clear_bit(
			START_STOP_INPROGRESS_BIT,
			&mlo_ctx->setup_info[grp_id].start_stop_inprogress);
}

qdf_export_symbol(mlo_clear_start_stop_inprogress);

#define WLAN_SOC_ID_NOT_INITIALIZED -1
bool mlo_vdevs_check_single_soc(struct wlan_objmgr_vdev **wlan_vdev_list,
				uint8_t vdev_count)
{
	int i;
	uint8_t soc_id = WLAN_SOC_ID_NOT_INITIALIZED;

	for (i = 0; i < vdev_count; i++) {
		uint8_t vdev_soc_id = wlan_vdev_get_psoc_id(wlan_vdev_list[i]);

		if (i == 0)
			soc_id = vdev_soc_id;
		else if (soc_id != vdev_soc_id)
			return false;
	}

	return true;
}

qdf_export_symbol(mlo_vdevs_check_single_soc);

static void mlo_check_state(struct wlan_objmgr_psoc *psoc,
			    void *obj, void *args)
{
	struct wlan_objmgr_pdev *pdev;
	uint8_t link_idx;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_state_params *params = (struct mlo_state_params *)args;

	uint8_t grp_id = params->grp_id;
	pdev = (struct wlan_objmgr_pdev *)obj;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) != QDF_STATUS_SUCCESS) {
		mlo_info("Failed to find pdev");
		return;
	}

	if (mlo_ctx->setup_info[grp_id].state[link_idx] != params->check_state)
		params->link_state_fail = 1;
}

QDF_STATUS mlo_check_all_pdev_state(struct wlan_objmgr_psoc *psoc,
				    uint8_t grp_id,
				    enum MLO_LINK_STATE state)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct mlo_state_params params = {0};

	params.check_state = state;
	params.grp_id = grp_id;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
				     mlo_check_state, &params,
				     0, WLAN_MLME_NB_ID);

	if (params.link_state_fail)
		status = QDF_STATUS_E_INVAL;
	else
		status = QDF_STATUS_SUCCESS;

	return status;
}

void mlo_setup_init(uint8_t total_grp)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t id;

	if (!mlo_ctx)
		return;

	if (!total_grp && total_grp > WLAN_MAX_MLO_GROUPS) {
		mlo_err("Total number of groups (%d) is greater than MAX (%d), MLD Setup failed!!",
			total_grp, WLAN_MAX_MLO_GROUPS);
		return;
	}

	mlo_ctx->total_grp = total_grp;
	setup_info = qdf_mem_malloc(sizeof(struct mlo_setup_info) *
					      total_grp);

	if (!setup_info)
		return;

	mlo_ctx->setup_info = setup_info;
	mlo_ctx->setup_info[0].ml_grp_id = 0;
	for (id = 0; id < total_grp; id++) {
		mlo_ctx->setup_info[id].tsf_sync_enabled = true;
		mlo_ctx->setup_info[id].wsi_stats_info_support = 0xff;

		if (qdf_event_create(&mlo_ctx->setup_info[id].event) !=
							QDF_STATUS_SUCCESS)
			mlo_err("Unable to create teardown event");
	}
}

qdf_export_symbol(mlo_setup_init);

void mlo_setup_deinit(void)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t id;

	if (!mlo_ctx)
		return;

	if (!mlo_ctx->setup_info)
		return;

	for (id = 0; id < mlo_ctx->total_grp; id++)
		qdf_event_destroy(&mlo_ctx->setup_info[id].event);

	qdf_mem_free(mlo_ctx->setup_info);
	mlo_ctx->setup_info = NULL;
}

qdf_export_symbol(mlo_setup_deinit);

void mlo_setup_update_chip_info(struct wlan_objmgr_psoc *psoc,
				uint8_t chip_id, uint8_t *adj_chip_id)
{
	uint8_t psoc_id, i;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_chip_info *chip_info;

	if (!mlo_ctx)
		return;

	chip_info = &mlo_ctx->setup_info->chip_info;
	/* get psoc_id of a soc */
	psoc_id = wlan_psoc_get_id(psoc);

	if (psoc_id >= MAX_MLO_CHIPS)
		return;
	/* chip id & psoc id need not be same, assign here based on psoc index*/
	chip_info->chip_id[psoc_id] = chip_id;

	/* For a particular psoc id populate the adjacent chip id's */
	for (i = 0; i < MAX_ADJ_CHIPS; i++)
		chip_info->adj_chip_ids[psoc_id][i] = adj_chip_id[i];

	chip_info->info_valid = 1;
}

qdf_export_symbol(mlo_setup_update_chip_info);

QDF_STATUS mlo_chip_adjacent(uint8_t psoc_id_1, uint8_t psoc_id_2,
			     uint8_t *is_adjacent)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t chip_id2, i;
	struct mlo_chip_info *chip_info;

	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	if ((psoc_id_1 >= MAX_MLO_CHIPS) || (psoc_id_2 >= MAX_MLO_CHIPS)) {
		mlo_err("psoc id's greater then max limit of %d",
			MAX_MLO_CHIPS);
		return QDF_STATUS_E_FAILURE;
	}

	chip_info = &mlo_ctx->setup_info->chip_info;

	/* default case is adjacent */
	*is_adjacent = 1;

	if (!chip_info->info_valid) {
		/* This is default (non-ini), they are adjacent */
		return QDF_STATUS_SUCCESS;
	}

	if (psoc_id_1 == psoc_id_2) {
		/* this is probably a single soc case, they are adjacent */
		return QDF_STATUS_SUCCESS;
	}
	/* get chip id from psoc */
	chip_id2 = chip_info->chip_id[psoc_id_2];
	for (i = 0; i < MAX_ADJ_CHIPS; i++) {
		if (chip_info->adj_chip_ids[psoc_id_1][i] == chip_id2)
			return QDF_STATUS_SUCCESS;
	}

	*is_adjacent = 0;
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(mlo_chip_adjacent);

void mlo_setup_update_num_links(struct wlan_objmgr_psoc *psoc,
				uint8_t grp_id, uint8_t num_links)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	mlo_ctx->setup_info[grp_id].tot_links += num_links;
	qdf_info("Grp_id %d Total MLO links = %d",
		 grp_id, mlo_ctx->setup_info[grp_id].tot_links);
}

qdf_export_symbol(mlo_setup_update_num_links);

void mlo_setup_update_soc_ready(struct wlan_objmgr_psoc *psoc, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t chip_idx, tot_socs;
	struct cdp_mlo_ctxt *dp_mlo_ctxt = NULL;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->tot_socs)
		return;

	tot_socs = setup_info->tot_socs;
	if (!mlo_psoc_get_index_id(psoc, grp_id, &chip_idx, 0))  {
		mlo_err("Unable to fetch chip idx for psoc id %d grp id %d",
			psoc->soc_objmgr.psoc_id,
			grp_id);
		return;
	}

	if (!(chip_idx < tot_socs)) {
		mlo_err("Invalid chip index, SoC setup failed");
		return;
	}

	setup_info->curr_soc_list[chip_idx] = psoc;
	mlo_set_soc_list(grp_id, psoc);
	setup_info->num_soc++;

	mlo_debug("SoC updated to mld grp %d , chip idx %d num soc %d",
		  grp_id, chip_idx, setup_info->num_soc);

	if (setup_info->num_soc != tot_socs)
		return;

	dp_mlo_ctxt = wlan_objmgr_get_dp_mlo_ctx(grp_id);

	if (!dp_mlo_ctxt) {
		dp_mlo_ctxt = cdp_mlo_ctxt_attach(
				wlan_psoc_get_dp_handle(psoc),
				(struct cdp_ctrl_mlo_mgr *)mlo_ctx);
		wlan_objmgr_set_dp_mlo_ctx(dp_mlo_ctxt, grp_id);
	}

	for (chip_idx = 0; chip_idx < tot_socs; chip_idx++) {
		struct wlan_objmgr_psoc *tmp_soc =
			setup_info->curr_soc_list[chip_idx];
		if (tmp_soc)
			cdp_soc_mlo_soc_setup(wlan_psoc_get_dp_handle(tmp_soc),
					      setup_info->dp_handle);
	}

	cdp_mlo_setup_complete(wlan_psoc_get_dp_handle(psoc),
			       setup_info->dp_handle);
}

qdf_export_symbol(mlo_setup_update_soc_ready);

void mlo_setup_link_ready(struct wlan_objmgr_pdev *pdev, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;
	uint16_t link_id;

	if (!mlo_ctx)
		return;

	if (!pdev)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->tot_links) {
		mlo_err("Setup info total links %d for grp id %d",
			setup_info->tot_links, grp_id);
		return;
	}

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) == QDF_STATUS_SUCCESS) {
		mlo_debug("Pdev already part of list link idx %d", link_idx);
		return;
	}

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (!setup_info->pdev_list[link_idx])
			break;

	if (link_idx >= setup_info->tot_links) {
		mlo_err("Exceeding max total mld links");
		return;
	}

	setup_info->pdev_list[link_idx] = pdev;
	setup_info->state[link_idx] = MLO_LINK_SETUP_INIT;
	setup_info->num_links++;

	link_id = wlan_mlo_get_pdev_hw_link_id(pdev);
	if (link_id == INVALID_HW_LINK_ID) {
		mlo_err("Invalid HW link id for the pdev");
		return;
	}
	setup_info->valid_link_bitmap |= (1 << link_id);

	qdf_info("Pdev updated to Grp id %d mld link %d num_links %d  hw link id %d Valid link bitmap %d",
		 grp_id, link_idx, setup_info->num_links,
		 link_id, setup_info->valid_link_bitmap);

	qdf_assert_always(link_idx < MAX_MLO_LINKS);

	if (setup_info->num_links == setup_info->tot_links &&
	    setup_info->num_soc == setup_info->tot_socs) {
		struct wlan_objmgr_psoc *psoc;
		struct wlan_lmac_if_tx_ops *tx_ops;
		QDF_STATUS status;

		psoc = wlan_pdev_get_psoc(pdev);
		tx_ops = wlan_psoc_get_lmac_if_txops(psoc);

		status = wlan_mgmt_rx_reo_validate_mlo_link_info(psoc);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("Failed to validate MLO HW link info");
			qdf_assert_always(0);
		}

		qdf_info("Trigger MLO Setup request");
		if (tx_ops && tx_ops->mops.target_if_mlo_setup_req) {
			tx_ops->mops.target_if_mlo_setup_req(
					setup_info->pdev_list,
					setup_info->num_links,
					grp_id);
		}
	}
}

qdf_export_symbol(mlo_setup_link_ready);

void mlo_link_setup_complete(struct wlan_objmgr_pdev *pdev, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;

	if (!mlo_ctx)
		return;

	if (!pdev)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->pdev_list[link_idx] == pdev) {
			setup_info->state[link_idx] =
							MLO_LINK_SETUP_DONE;
			break;
		}

	mlo_debug("Setup complete for pdev id %d mlo group %d",
		  pdev->pdev_objmgr.wlan_pdev_id, grp_id);

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->state[link_idx] == MLO_LINK_SETUP_DONE)
			continue;
		else
			break;

	if (link_idx == setup_info->tot_links) {
		struct wlan_objmgr_psoc *psoc;
		struct wlan_lmac_if_tx_ops *tx_ops;

		psoc = wlan_pdev_get_psoc(pdev);
		tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
		mlo_debug("Trigger MLO ready");
		if (tx_ops && tx_ops->mops.target_if_mlo_ready) {
			tx_ops->mops.target_if_mlo_ready(
					setup_info->pdev_list,
					setup_info->num_links);
		}
	}
}

qdf_export_symbol(mlo_link_setup_complete);

static void mlo_setup_link_down(struct wlan_objmgr_psoc *psoc,
				void *obj, void *args)
{
	struct wlan_objmgr_pdev *pdev;
	uint8_t link_idx;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint16_t link_id;
	uint8_t grp_id = *(uint8_t *)args;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	pdev = (struct wlan_objmgr_pdev *)obj;

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) != QDF_STATUS_SUCCESS) {
		mlo_info("Failed to find pdev");
		return;
	}

	if (setup_info->pdev_list[link_idx]) {
		setup_info->pdev_list[link_idx] = NULL;
		setup_info->state[link_idx] = MLO_LINK_UNINITIALIZED;
		setup_info->num_links--;

		link_id = wlan_mlo_get_pdev_hw_link_id(pdev);
		if (link_id == INVALID_HW_LINK_ID) {
			mlo_err("Invalid HW link id for the pdev");
			return;
		}
		setup_info->valid_link_bitmap &= ~(1 << link_id);
	}

	mlo_debug("Pdev link down grp_id %d link_idx %d num_links %d",
		  grp_id, link_idx, setup_info->num_links);
}

static void mlo_dp_ctxt_detach(struct wlan_objmgr_psoc *psoc,
			       uint8_t grp_id,
			       struct cdp_mlo_ctxt *dp_mlo_ctxt)
{
	if (!psoc)
		return;

	wlan_objmgr_set_dp_mlo_ctx(NULL, grp_id);
	if (dp_mlo_ctxt)
		cdp_mlo_ctxt_detach(wlan_psoc_get_dp_handle(psoc), dp_mlo_ctxt);
}

void mlo_setup_update_soc_down(struct wlan_objmgr_psoc *psoc, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t chip_idx;
	struct wlan_objmgr_psoc *soc;

	if (!mlo_ctx)
		return;

	if (!psoc)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (setup_info->num_links) {
		wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
					     mlo_setup_link_down, &grp_id,
					     0, WLAN_MLME_NB_ID);
	}

	if (!mlo_psoc_get_index_id(psoc, grp_id, &chip_idx, 1)) {
		mlo_err("Unable to fetch chip idx for psoc id %d grp id %d",
			psoc->soc_objmgr.psoc_id,
			grp_id);
		return;
	}

	if (!(chip_idx < MAX_MLO_CHIPS)) {
		mlo_err("Invalid chip index, SoC setup down failed");
		return;
	}

	if (setup_info->curr_soc_list[chip_idx]) {
		soc = setup_info->curr_soc_list[chip_idx];
		cdp_soc_mlo_soc_teardown(wlan_psoc_get_dp_handle(soc),
					 setup_info->dp_handle, false);

		setup_info->curr_soc_list[chip_idx] = NULL;
		setup_info->num_soc--;

		if (!setup_info->num_soc)
			mlo_dp_ctxt_detach(soc, grp_id, setup_info->dp_handle);
	}

	mlo_debug("Soc down, mlo group %d num soc %d num links %d",
		  grp_id, setup_info->num_soc,
		  setup_info->num_links);
}

qdf_export_symbol(mlo_setup_update_soc_down);

void mlo_link_teardown_complete(struct wlan_objmgr_pdev *pdev, uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;

	if (!mlo_ctx)
		return;

	if (!pdev)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	if (!setup_info->num_links) {
		mlo_err("Delayed response ignore");
		return;
	}

	if (mlo_find_pdev_idx(pdev, &link_idx, grp_id) != QDF_STATUS_SUCCESS) {
		mlo_info("Failed to find pdev");
		return;
	}

	mlo_debug("Teardown link idx = %d", link_idx);
	setup_info->state[link_idx] = MLO_LINK_TEARDOWN;

	/* Waiting for teardown on other links */
	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->state[link_idx] != MLO_LINK_TEARDOWN)
			return;

	qdf_info("Teardown complete");

	setup_info->trigger_umac_reset = false;

	qdf_event_set(&setup_info->event);
}

qdf_export_symbol(mlo_link_teardown_complete);

static void mlo_force_teardown(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		setup_info->state[link_idx] = MLO_LINK_TEARDOWN;
}

static void mlo_send_teardown_req(struct wlan_objmgr_psoc *psoc,
				  uint8_t grp_id, uint32_t reason)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct wlan_lmac_if_tx_ops *tx_ops;
	struct wlan_objmgr_pdev *temp_pdev;
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;
	uint8_t tot_links;
	bool umac_reset = 0;

	if (!mlo_ctx)
		return;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return;
	}

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		mlo_err("Tx Ops is null for the psoc id %d",
			wlan_psoc_get_id(psoc));
		return;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];
	tot_links = setup_info->tot_links;

	if (reason == WMI_HOST_MLO_TEARDOWN_REASON_MODE1_SSR ||
	    reason == WMI_HOST_MLO_TEARDOWN_REASON_STANDBY) {
		for (link_idx = 0; link_idx < tot_links; link_idx++) {
			umac_reset = 0;
			temp_pdev = setup_info->pdev_list[link_idx];
			if (!temp_pdev)
				continue;

			if (!setup_info->trigger_umac_reset) {
				if (psoc == wlan_pdev_get_psoc(temp_pdev)) {
					umac_reset = 1;
					setup_info->trigger_umac_reset = 1;
				}
			}

			if (tx_ops && tx_ops->mops.target_if_mlo_teardown_req) {
				mlo_info(
				"Trigger Teardown with Pdev id: %d Psoc id: %d link idx: %d Umac reset: %d Standby Active: %d",
				wlan_objmgr_pdev_get_pdev_id(temp_pdev),
				wlan_psoc_get_id(wlan_pdev_get_psoc(temp_pdev)),
				link_idx, umac_reset,
				temp_pdev->standby_active);
				tx_ops->mops.target_if_mlo_teardown_req(
						setup_info->pdev_list[link_idx],
						reason, umac_reset,
						temp_pdev->standby_active);
			}
		}
	} else {
		for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
			if (tx_ops && tx_ops->mops.target_if_mlo_teardown_req) {
				if (!setup_info->pdev_list[link_idx])
					continue;
				tx_ops->mops.target_if_mlo_teardown_req(
						setup_info->pdev_list[link_idx],
						reason, 0, 0);
			}
	}
}

static bool mlo_grp_in_teardown(uint8_t grp_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *setup_info;
	uint8_t link_idx;

	if (!mlo_ctx) {
		mlo_err("MLO ctx null, teardown failed");
		return true;
	}

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return true;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	for (link_idx = 0; link_idx < setup_info->tot_links; link_idx++)
		if (setup_info->pdev_list[link_idx] &&
		    (setup_info->state[link_idx] == MLO_LINK_TEARDOWN))
			return true;

	return false;
}

#define MLO_MGR_TEARDOWN_TIMEOUT 3000
QDF_STATUS mlo_link_teardown_link(struct wlan_objmgr_psoc *psoc,
				  uint8_t grp_id,
				  uint32_t reason)
{
	QDF_STATUS status;
	struct mlo_setup_info *setup_info;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if (!mlo_ctx)
		return QDF_STATUS_E_FAILURE;

	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	if (grp_id >= mlo_ctx->total_grp) {
		mlo_err("Invalid grp id %d, total no of groups %d",
			grp_id, mlo_ctx->total_grp);
		return QDF_STATUS_E_INVAL;
	}

	setup_info = &mlo_ctx->setup_info[grp_id];

	mlo_debug("Teardown req with grp_id %d num_soc %d num_link %d",
		  grp_id, setup_info->num_soc, setup_info->num_links);

	if (!setup_info->num_soc)
		return QDF_STATUS_SUCCESS;

	if (mlo_grp_in_teardown(grp_id)) {
		mlo_debug("Skip teardown: as teardown sent already, grp_id %d num_soc %d num_link %d",
			  grp_id, setup_info->num_soc, setup_info->num_links);
		return QDF_STATUS_SUCCESS;
	}

	/* Trigger MLO teardown */
	mlo_send_teardown_req(psoc, grp_id, reason);

	if (reason == WMI_HOST_MLO_TEARDOWN_REASON_SSR) {
		/* do not wait for teardown event completion here for SSR */
		return QDF_STATUS_SUCCESS;
	}

	status = qdf_wait_for_event_completion(
			&setup_info->event,
			MLO_MGR_TEARDOWN_TIMEOUT);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_info("Teardown timeout");
		mlo_force_teardown(grp_id);
	}

	return status;
}

qdf_export_symbol(mlo_link_teardown_link);

void mlo_update_wsi_stats_info_support(struct wlan_objmgr_psoc *psoc,
				       bool wsi_stats_info_support)
{
	uint8_t ml_grp_id;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *mlo_setup;

	ml_grp_id = wlan_mlo_get_psoc_group_id(psoc);
	if ((ml_grp_id ==  WLAN_MLO_GROUP_INVALID) ||
	    (ml_grp_id < 0)) {
		mlo_err("Invalid ML Grp ID %d", ml_grp_id);
		return;
	}

	mlo_setup = &mlo_ctx->setup_info[ml_grp_id];
	if (mlo_setup->wsi_stats_info_support == 0xFF)
		mlo_setup->wsi_stats_info_support = wsi_stats_info_support;
	else
		mlo_setup->wsi_stats_info_support &= wsi_stats_info_support;
}

qdf_export_symbol(mlo_update_wsi_stats_info_support);

uint8_t mlo_get_wsi_stats_info_support(struct wlan_objmgr_psoc *psoc)
{
	uint8_t ml_grp_id;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *mlo_setup;

	ml_grp_id = wlan_mlo_get_psoc_group_id(psoc);
	if ((ml_grp_id ==  WLAN_MLO_GROUP_INVALID) ||
	    (ml_grp_id < 0)) {
		mlo_err("Invalid ML Grp ID %d", ml_grp_id);
		return 0;
	}

	mlo_setup = &mlo_ctx->setup_info[ml_grp_id];
	if (mlo_setup->wsi_stats_info_support == 0xFF)
		return 0;

	return mlo_setup->wsi_stats_info_support;
}

void mlo_update_tsf_sync_support(struct wlan_objmgr_psoc *psoc,
				 bool tsf_sync_enab)
{
	uint8_t ml_grp_id;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct mlo_setup_info *mlo_setup;

	ml_grp_id = wlan_mlo_get_psoc_group_id(psoc);
	if (ml_grp_id < 0) {
		mlo_err("Invalid ML Grp ID %d", ml_grp_id);
		return;
	}

	mlo_setup = &mlo_ctx->setup_info[ml_grp_id];
	mlo_setup->tsf_sync_enabled &= tsf_sync_enab;
}

qdf_export_symbol(mlo_update_tsf_sync_support);

bool mlo_pdev_derive_bridge_link_pdevs(struct wlan_objmgr_pdev *pdev,
				       struct wlan_objmgr_pdev **pdev_list)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_psoc *grp_soc_list[MAX_MLO_CHIPS];
	struct wlan_objmgr_pdev *tmp_pdev;
	uint8_t tot_grp_socs;
	uint8_t grp_id;
	uint8_t psoc_id, tmp_psoc_id;
	uint8_t idx;
	uint8_t is_adjacent;
	QDF_STATUS status;

	psoc = wlan_pdev_get_psoc(pdev);

	/* Initialize pdev list to NULL by default */
	for (idx = 0; idx < MLO_MAX_BRIDGE_LINKS_PER_MLD; idx++)
		pdev_list[idx] = NULL;

	if (!mlo_psoc_get_grp_id(psoc, &grp_id)) {
		qdf_err("Unable to get group id");
		return false;
	}

	/* Get the total SOCs in the MLO group */
	tot_grp_socs = mlo_setup_get_total_socs(grp_id);
	if (!tot_grp_socs || tot_grp_socs > MAX_MLO_CHIPS) {
		qdf_err("Unable to get total SOCs");
		return false;
	}
	qdf_info("Total SOCs in MLO group%d: %d", grp_id, tot_grp_socs);

	/* Get the SOC list in the MLO group */
	mlo_get_soc_list(grp_soc_list, grp_id, tot_grp_socs,
			 WLAN_MLO_GROUP_DEFAULT_SOC_LIST);

	psoc_id = wlan_psoc_get_id(psoc);

	/*
	 * Check the current pdev for num bridge links created and
	 * add to the pdev list if possible otherwise find opposite pdev
	 */
	if (wlan_pdev_get_mlo_bridge_vdev_count(pdev)
	    < MLO_MAX_BRIDGE_LINKS_PER_RADIO)
		pdev_list[0] = pdev;

	/*
	 * Iterate over the MLO group SOC list
	 * and get the pdevs for bridge links
	 */
	for (idx = 0; idx < tot_grp_socs; idx++) {
		if (!grp_soc_list[idx])
			continue;

		if (grp_soc_list[idx] == psoc)
			continue;

		/* Skip the pdev if bridge link quota is over */
		tmp_pdev = grp_soc_list[idx]->soc_objmgr.wlan_pdev_list[0];

		if (wlan_pdev_get_mlo_bridge_vdev_count(tmp_pdev)
		    >= MLO_MAX_BRIDGE_LINKS_PER_RADIO)
			continue;

		tmp_psoc_id = wlan_psoc_get_id(grp_soc_list[idx]);

		qdf_info("Checking adjacency of soc %d and %d",
			 psoc_id, tmp_psoc_id);
		status = mlo_chip_adjacent(psoc_id, tmp_psoc_id, &is_adjacent);
		if (status != QDF_STATUS_SUCCESS) {
			qdf_info("Check adjacency failed");
			return false;
		}

		if (is_adjacent) {
			if (!pdev_list[1])
				pdev_list[1] = tmp_pdev;
		} else if (!pdev_list[0]) {
			pdev_list[0] = tmp_pdev;
		}

		if (pdev_list[0] && pdev_list[1])
			return true;
	}

	return false;
}

qdf_export_symbol(mlo_pdev_derive_bridge_link_pdevs);
#endif /*WLAN_MLO_MULTI_CHIP*/

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_mlo_mgr_main.h"
#include "qdf_types.h"
#include "wlan_cmn.h"
#include "wlan_mlo_mgr_peer.h"
#include <wlan_mlo_mgr_ap.h>
#include <wlan_mlo_mgr_setup.h>
#include <wlan_utility.h>
#include <wlan_reg_services_api.h>
#include <wlan_mlo_mgr_sta.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mgmt_txrx_rx_reo_utils_api.h>
/**
 * struct mlpeer_data: PSOC peers MLO data
 * @total_rssi:  sum of RSSI of all ML peers
 * @num_ml_peers: Number of ML peer's with this PSOC as TQM
 * @max_ml_peers: Max ML peers can have this PSOC as TQM
 *                (it is to distribute peers across all PSOCs)
 * @num_non_ml_peers: Non MLO peers of this PSOC
 */
struct mlpeer_data {
	int32_t total_rssi;
	uint16_t num_ml_peers;
	uint16_t max_ml_peers;
	uint16_t num_non_ml_peers;
};

/**
 * struct mlo_all_link_rssi: structure to collect TQM params for all PSOCs
 * @psoc_tqm_parms:  It collects peer data for all PSOCs
 * @num_psocs:       Number of PSOCs in the system
 * @current_psoc_id: current psoc id, it is for iterator
 */
struct mlo_all_link_rssi {
	struct mlpeer_data psoc_tqm_parms[WLAN_OBJMGR_MAX_DEVICES];
	uint8_t num_psocs;
	uint8_t current_psoc_id;
};

/* Invalid TQM/PSOC ID */
#define ML_INVALID_PRIMARY_TQM   0xff
/* Congestion value */
#define ML_PRIMARY_TQM_CONGESTION 30
/* PTQM migration timeout value in ms */
#define ML_PRIMARY_TQM_MIGRATRION_TIMEOUT 4000
/* Link ID used for WDS Bridge*/
#define WDS_BRIDGE_VDEV_LINK_ID (WLAN_LINK_ID_INVALID - 1)

static void wlan_mlo_peer_get_rssi(struct wlan_objmgr_psoc *psoc,
				   void *obj, void *args)
{
	struct wlan_mlo_peer_context *mlo_peer_ctx;
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
	struct mlo_all_link_rssi *rssi_data = (struct mlo_all_link_rssi *)args;
	struct mlpeer_data *tqm_params = NULL;
	uint8_t index;

	mlo_peer_ctx = peer->mlo_peer_ctx;
	index = rssi_data->current_psoc_id;
	tqm_params = &rssi_data->psoc_tqm_parms[index];

	if (!wlan_peer_is_mlo(peer) && !mlo_peer_ctx) {
		if (wlan_peer_get_peer_type(peer) == WLAN_PEER_STA)
			tqm_params->num_non_ml_peers += 1;
		return;
	}

	if (!mlo_peer_ctx)
		return;

	/* If this psoc is new primary UMAC after migration,
	 * account RSSI on new link
	 */
	if (mlo_peer_ctx->migrate_primary_umac_psoc_id ==
			rssi_data->current_psoc_id) {
		tqm_params->total_rssi += mlo_peer_ctx->avg_link_rssi;
		tqm_params->num_ml_peers += 1;
		return;
	}

	/* If this psoc is not primary UMAC or if TQM migration is happening
	 * from current primary psoc, don't account RSSI
	 */
	if (mlo_peer_ctx->primary_umac_psoc_id == rssi_data->current_psoc_id &&
	    mlo_peer_ctx->migrate_primary_umac_psoc_id ==
	    ML_INVALID_PRIMARY_TQM) {
		tqm_params->total_rssi += mlo_peer_ctx->avg_link_rssi;
		tqm_params->num_ml_peers += 1;
	}
}

static void wlan_get_rssi_data_each_psoc(struct wlan_objmgr_psoc *psoc,
					 void *arg, uint8_t index)
{
	struct mlo_all_link_rssi *rssi_data = (struct mlo_all_link_rssi *)arg;
	struct mlpeer_data *tqm_params = NULL;

	tqm_params = &rssi_data->psoc_tqm_parms[index];

	tqm_params->total_rssi = 0;
	tqm_params->num_ml_peers = 0;
	tqm_params->num_non_ml_peers = 0;
	tqm_params->max_ml_peers = MAX_MLO_PEER;

	rssi_data->current_psoc_id = index;
	rssi_data->num_psocs++;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PEER_OP,
				     wlan_mlo_peer_get_rssi, rssi_data, 0,
				     WLAN_MLO_MGR_ID);
}

static QDF_STATUS mld_get_link_rssi(struct mlo_all_link_rssi *rssi_data)
{
	rssi_data->num_psocs = 0;

	wlan_objmgr_iterate_psoc_list(wlan_get_rssi_data_each_psoc,
				      rssi_data, WLAN_MLO_MGR_ID);

	return QDF_STATUS_SUCCESS;
}

uint8_t
wlan_mld_get_best_primary_umac_w_rssi(struct wlan_mlo_peer_context *ml_peer,
				      struct wlan_objmgr_vdev *link_vdevs[],
				      bool allow_all_links)
{
	struct mlo_all_link_rssi rssi_data;
	uint8_t i;
	int32_t avg_rssi[WLAN_OBJMGR_MAX_DEVICES] = {0};
	int32_t diff_rssi[WLAN_OBJMGR_MAX_DEVICES] = {0};
	int32_t diff_low;
	bool mld_sta_links[WLAN_OBJMGR_MAX_DEVICES] = {0};
	bool mld_no_sta[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint8_t prim_link, id, prim_link_hi;
	uint8_t num_psocs;
	struct mlpeer_data *tqm_params = NULL;
	struct wlan_channel *channel;
	enum phy_ch_width sec_hi_bw, hi_bw;
	uint8_t cong = ML_PRIMARY_TQM_CONGESTION;
	uint16_t mld_ml_sta_count[WLAN_OBJMGR_MAX_DEVICES] = {0};
	enum phy_ch_width mld_ch_width[WLAN_OBJMGR_MAX_DEVICES];
	uint8_t psoc_w_nosta;
	uint16_t ml_sta_count = 0;
	uint32_t total_cap, cap;
	uint16_t bw;
	bool group_full[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint16_t group_size[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint16_t grp_size = 0;
	uint16_t group_full_count = 0;

	mld_get_link_rssi(&rssi_data);

	for (i = 0; i < rssi_data.num_psocs; i++) {
		tqm_params = &rssi_data.psoc_tqm_parms[i];

		if (tqm_params->num_ml_peers)
			avg_rssi[i] = (tqm_params->total_rssi /
				       tqm_params->num_ml_peers);
	}

	/**
	 * If MLD STA associated to a set of links, choose primary UMAC
	 * from those links only
	 */
	num_psocs = 0;
	psoc_w_nosta = 0;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++)
		mld_ch_width[i] = CH_WIDTH_INVALID;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!link_vdevs[i])
			continue;

		id = wlan_vdev_get_psoc_id(link_vdevs[i]);
		if (id >= WLAN_OBJMGR_MAX_DEVICES)
			continue;

		if (!allow_all_links && wlan_vdev_skip_pumac(link_vdevs[i])) {
			mlo_err("Skip Radio for Primary MLO umac");
			mld_sta_links[id] = false;
			continue;
		}

		tqm_params = &rssi_data.psoc_tqm_parms[id];
		mld_sta_links[id] = true;

		channel = wlan_vdev_mlme_get_bss_chan(link_vdevs[i]);
		mld_ch_width[id] = channel->ch_width;

		if ((tqm_params->num_ml_peers +
		     tqm_params->num_non_ml_peers) == 0) {
			/* If this PSOC has no stations */
			mld_no_sta[id] = true;
			psoc_w_nosta++;
		}

		mld_ml_sta_count[id] = tqm_params->num_ml_peers;
		/* Update total MLO STA count */
		ml_sta_count += tqm_params->num_ml_peers;

		num_psocs++;

		/* If no stations are associated, derive diff rssi
		 * based on psoc id {0-20, 20-40, 40 } so that
		 * stations are distributed across TQMs
		 */
		if (!avg_rssi[id]) {
			diff_rssi[id] = (id * 20);
			continue;
		}
		diff_rssi[id] = (ml_peer->avg_link_rssi >= avg_rssi[id]) ?
				(ml_peer->avg_link_rssi - avg_rssi[id]) :
				(avg_rssi[id] - ml_peer->avg_link_rssi);

	}

	prim_link = ML_INVALID_PRIMARY_TQM;

	/* If one of the PSOCs doesn't have any station select that PSOC as
	 * primary TQM. If more than one PSOC have no stations as Primary TQM
	 * the vdev with less bw needs to be selected as Primary TQM
	 */
	if (psoc_w_nosta == 1) {
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			if (mld_no_sta[i]) {
				prim_link = i;
				break;
			}
		}
	} else if (psoc_w_nosta > 1) {
		hi_bw = CH_WIDTH_INVALID;
		sec_hi_bw = CH_WIDTH_INVALID;
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			if (!mld_no_sta[i])
				continue;

			if (hi_bw == CH_WIDTH_INVALID) {
				prim_link_hi = i;
				hi_bw = mld_ch_width[i];
				continue;
			}
			/* if bw is 320MHZ mark it as highest ch width */
			if (mld_ch_width[i] == CH_WIDTH_320MHZ) {
				prim_link = prim_link_hi;
				sec_hi_bw = hi_bw;
				hi_bw = mld_ch_width[i];
				prim_link_hi = i;
			}
			/* If bw is less than or equal to 160 MHZ
			 * and chwidth is greater than than other link
			 * Mark this link as primary link
			 */
			if (mld_ch_width[i] <= CH_WIDTH_160MHZ) {
				if (hi_bw < mld_ch_width[i]) {
					/* move high bw to second high bw */
					prim_link = prim_link_hi;
					sec_hi_bw = hi_bw;

					hi_bw = mld_ch_width[i];
					prim_link_hi = i;
				} else if ((sec_hi_bw == CH_WIDTH_INVALID) ||
					   (sec_hi_bw < mld_ch_width[i])) {
					/* update sec high bw */
					sec_hi_bw = mld_ch_width[i];
					prim_link = i;
				}
			}
		}
	} else {
		total_cap = 0;
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			bw = wlan_reg_get_bw_value(mld_ch_width[i]);
			total_cap += bw * (100 - cong);
		}

		group_full_count = 0;
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			if (!mld_sta_links[i])
				continue;

			bw = wlan_reg_get_bw_value(mld_ch_width[i]);
			cap = bw * (100 - cong);
			grp_size = (ml_sta_count) * ((cap * 100) / total_cap);
			group_size[i] = grp_size / 100;
			if (group_size[i] <=  mld_ml_sta_count[i]) {
				group_full[i] = true;
				group_full_count++;
			}
		}

		if ((num_psocs - group_full_count) == 1) {
			for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
				if (!mld_sta_links[i])
					continue;

				if (group_full[i])
					continue;

				prim_link = i;
				break;
			}
		} else {
			diff_low = 0;
			/* find min diff, based on it, allocate primary umac */
			for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
				if (!mld_sta_links[i])
					continue;

				/* First iteration */
				if (diff_low == 0) {
					diff_low = diff_rssi[i];
					prim_link = i;
				} else if (diff_low > diff_rssi[i]) {
					diff_low = diff_rssi[i];
					prim_link = i;
				}
			}
		}
	}

	if (prim_link != ML_INVALID_PRIMARY_TQM)
		return prim_link;

	/* If primary link id is not found, return id of 1st available link */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!link_vdevs[i])
			continue;

		if (!allow_all_links && wlan_vdev_skip_pumac(link_vdevs[i])) {
			mlo_debug("Skip Radio for Primary MLO umac");
			continue;
		}
		id = wlan_vdev_get_psoc_id(link_vdevs[i]);
		if (id >= WLAN_OBJMGR_MAX_DEVICES)
			continue;

		return wlan_vdev_get_psoc_id(link_vdevs[i]);
	}

	return ML_INVALID_PRIMARY_TQM;
}

void mlo_peer_assign_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_mlo_link_peer_entry *peer_entry)
{
	struct wlan_mlo_link_peer_entry *peer_ent_iter;
	uint8_t i;
	uint8_t primary_umac_set = 0;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	bool is_central_primary = false;
	uint8_t bridge_umac_id = -1;
	uint8_t link_peer_psoc_id;
	struct wlan_mlo_dev_context *ml_dev = NULL;
#endif

	/* If MLD is within single SOC, then assoc link becomes
	 * primary umac
	 */
	if (ml_peer->primary_umac_psoc_id == ML_PRIMARY_UMAC_ID_INVAL) {
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
		ml_dev = ml_peer->ml_dev;

		if (!ml_dev) {
			mlo_err("ML dev ctx is NULL");
			return;
		}
		if (ml_dev->bridge_sta_ctx) {
			is_central_primary = ml_dev->bridge_sta_ctx->is_force_central_primary;
			bridge_umac_id = ml_dev->bridge_sta_ctx->bridge_umac_id;
		}
		link_peer_psoc_id = wlan_peer_get_psoc_id(peer_entry->link_peer);
		if (is_central_primary) {
			if (link_peer_psoc_id == bridge_umac_id) {
				peer_entry->is_primary = true;
				ml_peer->primary_umac_psoc_id = bridge_umac_id;
			} else {
				peer_entry->is_primary = false;
				ml_peer->primary_umac_psoc_id = bridge_umac_id;
			}

		} else {

#endif
			if (wlan_peer_mlme_is_assoc_peer(peer_entry->link_peer)) {
				peer_entry->is_primary = true;
				ml_peer->primary_umac_psoc_id =
					wlan_peer_get_psoc_id(peer_entry->link_peer);
			} else {
				peer_entry->is_primary = false;
			}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
		}
#endif
	} else {
		if ((wlan_peer_mlme_is_assoc_peer(peer_entry->link_peer)) &&
		    (ml_peer->max_links > 1) &&
		    (mlo_ctx->force_non_assoc_prim_umac)) {
			peer_entry->is_primary = false;
			return;
		}

		/* If this peer PSOC is not derived as Primary PSOC,
		 * mark is_primary as false
		 */
		if (wlan_peer_get_psoc_id(peer_entry->link_peer) !=
				ml_peer->primary_umac_psoc_id) {
			peer_entry->is_primary = false;
			return;
		}

		/* For single SOC, check whether is_primary is set for
		 * other partner peer, then mark is_primary false for this peer
		 */
		for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
			peer_ent_iter = &ml_peer->peer_list[i];

			if (!peer_ent_iter->link_peer)
				continue;

			/* Check for other link peers */
			if (peer_ent_iter == peer_entry)
				continue;

			if (wlan_peer_get_psoc_id(peer_ent_iter->link_peer) !=
					ml_peer->primary_umac_psoc_id)
				continue;

			if (peer_ent_iter->is_primary)
				primary_umac_set = 1;
		}

		if (primary_umac_set)
			peer_entry->is_primary = false;
		else
			peer_entry->is_primary = true;
	}
}

static int8_t wlan_vdev_derive_link_rssi(struct wlan_objmgr_vdev *vdev,
					 struct wlan_objmgr_vdev *assoc_vdev,
					 int8_t rssi)
{
	struct wlan_channel *channel, *assoc_channel;
	uint16_t ch_freq, assoc_freq;
	uint8_t tx_pow, assoc_tx_pow;
	int8_t diff_txpow;
	struct wlan_objmgr_pdev *pdev, *assoc_pdev;
	uint8_t log10_freq;
	uint8_t derived_rssi;
	int16_t ten_derived_rssi;
	int8_t ten_diff_pl = 0;

	pdev = wlan_vdev_get_pdev(vdev);
	assoc_pdev = wlan_vdev_get_pdev(assoc_vdev);

	channel = wlan_vdev_get_active_channel(vdev);
	if (channel)
		ch_freq = channel->ch_freq;
	else
		ch_freq = 1;

	assoc_channel = wlan_vdev_get_active_channel(assoc_vdev);
	if (assoc_channel)
		assoc_freq = assoc_channel->ch_freq;
	else
		assoc_freq = 1;

	/*
	 *  diff of path loss (of two links) = log10(freq1) - log10(freq2)
	 *                       (since distance is constant)
	 *  since log10 is not available, we cameup with approximate ranges
	 */
	log10_freq = (ch_freq * 10) / assoc_freq;
	if ((log10_freq >= 20) && (log10_freq < 30))
		ten_diff_pl = 4;  /* 0.4 *10 */
	else if ((log10_freq >= 11) && (log10_freq < 20))
		ten_diff_pl = 1;  /* 0.1 *10 */
	else if ((log10_freq >= 8) && (log10_freq < 11))
		ten_diff_pl = 0; /* 0 *10 */
	else if ((log10_freq >= 4) && (log10_freq < 8))
		ten_diff_pl = -1; /* -0.1 * 10 */
	else if ((log10_freq >= 1) && (log10_freq < 4))
		ten_diff_pl = -4;  /* -0.4 * 10 */

	assoc_tx_pow = wlan_reg_get_channel_reg_power_for_freq(assoc_pdev,
							       assoc_freq);
	tx_pow = wlan_reg_get_channel_reg_power_for_freq(pdev, ch_freq);

	diff_txpow = tx_pow -  assoc_tx_pow;

	ten_derived_rssi = (diff_txpow * 10) - ten_diff_pl + (rssi * 10);
	derived_rssi = ten_derived_rssi / 10;

	return derived_rssi;
}

static void mlo_peer_calculate_avg_rssi(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer,
		int8_t rssi,
		struct wlan_objmgr_vdev *assoc_vdev)
{
	int32_t total_rssi = 0;
	uint8_t num_psocs = 0;
	uint8_t i;
	struct wlan_objmgr_vdev *vdev;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev = ml_dev->wlan_vdev_list[i];
		if (!vdev)
			continue;

		num_psocs++;
		if (vdev == assoc_vdev)
			total_rssi += rssi;
		else
			total_rssi += wlan_vdev_derive_link_rssi(vdev,
								 assoc_vdev,
								 rssi);
	}

	if (!num_psocs)
		return;

	ml_peer->avg_link_rssi = total_rssi / num_psocs;
}

#ifdef WLAN_MLO_MULTI_CHIP
int8_t mlo_get_central_umac_id(
		uint8_t *psoc_ids)
{
	uint8_t prim_psoc_id = -1;
	uint8_t adjacent = 0;

	/* Some 3 link RDPs have restriction on the primary umac.
	 * Only the link that is adjacent to both the links can be
	 * a primary umac.
	 * Note: it means umac migration is also restricted.
	 */
	mlo_chip_adjacent(psoc_ids[0], psoc_ids[1], &adjacent);
	if (!adjacent) {
		prim_psoc_id = psoc_ids[2];
	} else {
		mlo_chip_adjacent(psoc_ids[0], psoc_ids[2], &adjacent);
		if (!adjacent) {
			prim_psoc_id = psoc_ids[1];
		} else {
			/* If all links are adjacent to each other,
			 * no need to restrict the primary umac.
			 * return failure the caller will handle.
			 */
			mlo_chip_adjacent(psoc_ids[1], psoc_ids[2],
					  &adjacent);
			if (!adjacent)
				prim_psoc_id = psoc_ids[0];
			else
				return prim_psoc_id;
		}
	}

	return prim_psoc_id;
}

QDF_STATUS mlo_check_topology(struct wlan_objmgr_pdev *pdev,
			      struct wlan_objmgr_vdev *vdev,
			      uint8_t aplinks)
{
	struct wlan_mlo_dev_context *ml_dev = vdev->mlo_dev_ctx;
	struct wlan_objmgr_vdev *vdev_iter = NULL;
	struct wlan_objmgr_vdev *tmp_vdev = NULL;
	uint8_t psoc_ids[WLAN_UMAC_MLO_MAX_VDEVS];
	uint8_t i, idx = 0;
	uint8_t bridge_umac;
	uint8_t adjacent = -1;
	uint8_t max_soc;
	uint8_t link_id;
	bool is_mlo_vdev;

	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	/* Do topology check for STA mode for other modes return Success */
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return QDF_STATUS_SUCCESS;

	max_soc = mlo_get_total_links(pdev);

	if (max_soc != WLAN_UMAC_MLO_MAX_VDEVS) {
		/* For Devices which has no topology dependency return Success */
		return QDF_STATUS_SUCCESS;
	}

	if (!ml_dev->bridge_sta_ctx) {
		mlo_err("Bridge STA context Null");
		return QDF_STATUS_E_FAILURE;
	}

	/* Incase of 4-LINK RDP in 3-LINK NON-AP MLD mode there is
	 * restriction to have the primary umac as central in topology.
	 * Note: It also means restriction on umac migration
	 */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_iter = vdev->mlo_dev_ctx->wlan_vdev_list[i];
		if (!vdev_iter)
			continue;
		/* Store the psoc_ids of the links */
		psoc_ids[idx] = wlan_vdev_get_psoc_id(vdev_iter);
		idx++;
	}
	/* If number of links in AP are greater or equal to STA */
	if (aplinks >= idx) {
		/* Station has 2 links enabled */
		/* Check if the primary umac and assoc links can be different*/
		if (idx == (WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY - 1)) {
			mlo_chip_adjacent(psoc_ids[0], psoc_ids[1], &adjacent);
			if (adjacent == 1) {
				mlo_info("pri umac & assoc link can be diff as chips are adj");
				return QDF_STATUS_SUCCESS;
			} else {
				mlo_info("pri umac & assoc link can be diff but need bridge");
				return QDF_STATUS_E_FAILURE;
			}
		}
		/* Check if the primary umac and assoc links are same for 3 link sta*/
		if (idx == WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY) {
			bridge_umac = mlo_get_central_umac_id(psoc_ids);

			tmp_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev,
								  bridge_umac,
								  false);

			if (!tmp_vdev)
				return QDF_STATUS_E_FAILURE;

			link_id = tmp_vdev->vdev_mlme.mlo_link_id;
			if (bridge_umac != -1) {
				if (wlan_vdev_get_psoc_id(vdev) != bridge_umac) {
					mlo_err("Central LINK %d Force central as primary umac!! ",
						bridge_umac);
					tmp_vdev->vdev_objmgr.mlo_central_vdev = true;
					ml_dev->bridge_sta_ctx->is_force_central_primary = true;
					ml_dev->bridge_sta_ctx->bridge_umac_id = bridge_umac;
					ml_dev->bridge_sta_ctx->bridge_link_id = link_id;
					wlan_objmgr_vdev_release_ref(tmp_vdev, WLAN_MLO_MGR_ID);
					return QDF_STATUS_SUCCESS;
				}
			}
		}
	} else {
		/* If # of links in AP < then link on Station check for bridge vap */
		/* Check case when AP MLD is 2 link and NON-AP MLD is 3 link capable*/
		if (idx == WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY &&
		    (aplinks == (WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY - 1))) {
			bridge_umac = mlo_get_central_umac_id(psoc_ids);
			tmp_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev,
								  bridge_umac,
								  false);

			if (!tmp_vdev)
				return QDF_STATUS_E_FAILURE;

			link_id = tmp_vdev->vdev_mlme.mlo_link_id;
			if (bridge_umac != -1) {
				if (wlan_vdev_get_psoc_id(vdev) != bridge_umac) {
					is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(tmp_vdev);
					if (is_mlo_vdev) {
						mlo_err("Central Link %d partipating in Assoc!! ",
							bridge_umac);
					} else {
						mlo_err("Central %d not part of Assoc create bridge!!",
							bridge_umac);
						tmp_vdev->vdev_objmgr.mlo_central_vdev = true;
						ml_dev->bridge_sta_ctx->is_force_central_primary = true;
						ml_dev->bridge_sta_ctx->bridge_umac_id = bridge_umac;
						ml_dev->bridge_sta_ctx->bridge_vap_exists = true;
						ml_dev->bridge_sta_ctx->bridge_link_id = WDS_BRIDGE_VDEV_LINK_ID;
					}
				}
			}
		}
	}
	if (tmp_vdev)
		wlan_objmgr_vdev_release_ref(tmp_vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

void mlo_update_partner_bridge_info(struct wlan_mlo_dev_context *ml_dev,
				    struct mlo_partner_info *partner_info)
{
	struct wlan_objmgr_vdev *bridge_vdev = NULL;
	uint8_t bridge_umac_id = -1;
	uint8_t bridge_index = partner_info->num_partner_links;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return;

	bridge_umac_id = ml_dev->bridge_sta_ctx->bridge_umac_id;
	bridge_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev, bridge_umac_id, false);
	if (bridge_vdev) {
		partner_info->partner_link_info[bridge_index].link_id = bridge_vdev->vdev_mlme.mlo_link_id;
		qdf_mem_copy(&partner_info->partner_link_info[bridge_index].link_addr,
			     wlan_vdev_mlme_get_macaddr(bridge_vdev), sizeof(struct qdf_mac_addr));
		/* Account for bridge peer here */
		partner_info->num_partner_links++;
		wlan_objmgr_vdev_release_ref(bridge_vdev, WLAN_MLO_MGR_ID);
	}
}

bool mlo_is_sta_bridge_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev)
		return false;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return false;

	if (vdev->vdev_objmgr.mlo_central_vdev &&
	    ml_dev->bridge_sta_ctx->bridge_vap_exists)
		return true;

	return false;
}

qdf_export_symbol(mlo_is_sta_bridge_vdev);

bool mlo_sta_bridge_exists(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev)
		return false;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return false;

	if (ml_dev->bridge_sta_ctx->bridge_vap_exists)
		return true;

	return false;
}

qdf_export_symbol(mlo_sta_bridge_exists);

bool mlo_is_force_central_primary(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev)
		return false;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return false;

	if (ml_dev->bridge_sta_ctx->is_force_central_primary)
		return true;

	return false;
}

qdf_export_symbol(mlo_is_force_central_primary);

uint8_t mlo_get_total_links(struct wlan_objmgr_pdev *pdev)
{
	uint8_t ml_grp_id;

	ml_grp_id = wlan_get_mlo_grp_id_from_pdev(pdev);
	return mlo_setup_get_total_socs(ml_grp_id);
}

static QDF_STATUS mlo_set_3_link_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[])
{
	uint8_t psoc_ids[WLAN_UMAC_MLO_MAX_VDEVS];
	int8_t central_umac_id;

	if (ml_peer->max_links != 3)
		return QDF_STATUS_E_FAILURE;

	/* Some 3 link RDPs have restriction on the primary umac.
	 * Only the link that is adjacent to both the links can be
	 * a primary umac.
	 * Note: it means umac migration is also restricted.
	 */
	psoc_ids[0] = wlan_vdev_get_psoc_id(link_vdevs[0]);
	psoc_ids[1] = wlan_vdev_get_psoc_id(link_vdevs[1]);
	psoc_ids[2] = wlan_vdev_get_psoc_id(link_vdevs[2]);

	central_umac_id = mlo_get_central_umac_id(psoc_ids);
	if (central_umac_id != -1)
		ml_peer->primary_umac_psoc_id = central_umac_id;
	else
		return QDF_STATUS_E_FAILURE;

	mlo_peer_assign_primary_umac(ml_peer,
				     &ml_peer->peer_list[0]);

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS mlo_set_3_link_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[])
{
	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS mlo_peer_allocate_primary_umac(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[])
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *assoc_peer = NULL;
	int32_t rssi;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t first_link_id = 0;
	bool primary_umac_set = false;
	uint8_t i, psoc_id;

	peer_entry = &ml_peer->peer_list[0];
	assoc_peer = peer_entry->link_peer;
	if (!assoc_peer)
		return QDF_STATUS_E_FAILURE;

	/* For Station mode, assign assoc peer as primary umac */
	if (wlan_peer_get_peer_type(assoc_peer) == WLAN_PEER_AP) {
		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " primary umac soc %d ",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	/* Select assoc peer's PSOC as primary UMAC in Multi-chip solution,
	 * 1) for single link MLO connection
	 * 2) if MLD is single chip MLO
	 */
	if ((mlo_ctx->force_non_assoc_prim_umac) &&
	    (ml_peer->max_links >= 1)) {
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!link_vdevs[i])
				continue;

			if (wlan_peer_get_vdev(assoc_peer) == link_vdevs[i])
				continue;
			psoc_id = wlan_vdev_get_psoc_id(link_vdevs[i]);
			ml_peer->primary_umac_psoc_id = psoc_id;
			break;
		}

		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT
			 " primary umac soc %d ", ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if ((ml_peer->max_links == 1) ||
	    (mlo_vdevs_check_single_soc(link_vdevs, ml_peer->max_links))) {
		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d Assoc peer " QDF_MAC_ADDR_FMT
			 " primary umac soc %d ", ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if (mlo_set_3_link_primary_umac(ml_peer, link_vdevs) ==
	    QDF_STATUS_SUCCESS) {
		/* If success then the primary umac is restricted and assigned.
		 * if not, there is no restriction, so just fallthrough
		 */
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT
			 " center primary umac soc %d ",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if (mlo_ctx->mlo_is_force_primary_umac) {
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!link_vdevs[i])
				continue;

			psoc_id = wlan_vdev_get_psoc_id(link_vdevs[i]);
			if (!first_link_id)
				first_link_id = psoc_id;

			if (psoc_id == mlo_ctx->mlo_forced_primary_umac_id) {
				ml_peer->primary_umac_psoc_id = psoc_id;
				primary_umac_set = true;
				break;
			}
		}

		if (!primary_umac_set)
			ml_peer->primary_umac_psoc_id = first_link_id;

		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " primary umac soc %d ",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	rssi = wlan_peer_get_rssi(assoc_peer);
	mlo_peer_calculate_avg_rssi(ml_dev, ml_peer, rssi,
				    wlan_peer_get_vdev(assoc_peer));

	ml_peer->primary_umac_psoc_id =
		wlan_mld_get_best_primary_umac_w_rssi(ml_peer, link_vdevs, false);

	mlo_peer_assign_primary_umac(ml_peer, peer_entry);

	mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " avg RSSI %d primary umac soc %d ",
		 ml_dev->mld_id,
		 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		 ml_peer->avg_link_rssi, ml_peer->primary_umac_psoc_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlo_peer_free_primary_umac(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
void wlan_objmgr_mlo_update_primary_info(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer = NULL;
	struct wlan_mlo_link_peer_entry *peer_ent_iter;
	uint8_t i;

	ml_peer = peer->mlo_peer_ctx;
	wlan_mlo_peer_wsi_link_delete(ml_peer);
	ml_peer->primary_umac_psoc_id = wlan_peer_get_psoc_id(peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_ent_iter = &ml_peer->peer_list[i];

		if (!peer_ent_iter->link_peer)
			continue;

		if (peer_ent_iter->is_primary)
			peer_ent_iter->is_primary = false;

		if (peer_ent_iter->link_peer == peer)
			peer_ent_iter->is_primary = true;
	}
	wlan_mlo_peer_wsi_link_add(ml_peer);
}

qdf_export_symbol(wlan_objmgr_mlo_update_primary_info);

void mlo_mlme_ptqm_migrate_timer_cb(void *arg)
{
	struct wlan_mlo_dev_context *ml_dev = (struct wlan_mlo_dev_context *)arg;
	struct wlan_mlo_peer_context *ml_peer = NULL;
	uint16_t i = 0;

	if (!ml_dev)
		return;

	/* Check for pending bitmaps and issue disconnect */
	for (i = 0; i < MAX_MLO_PEER_ID; i++) {
		if (qdf_test_bit(i, ml_dev->mlo_peer_id_bmap)) {
			ml_peer = wlan_mlo_get_mlpeer_by_ml_peerid(ml_dev, i);
			if (ml_peer && ml_peer->primary_umac_migration_in_progress) {
				ml_peer->primary_umac_migration_in_progress = false;
				mlo_err("Issue disconnect for ml peer with ml peer id:%d", i);
				wlan_mlo_peer_deauth_init(ml_peer,
							  NULL, 0);
			}
			qdf_clear_bit(i, ml_dev->mlo_peer_id_bmap);
		}
	}
}

/**
 * mlo_ptqm_list_peek_head() - Returns the head of linked list
 *
 * @ptqm_list: Pointer to the list of peer ptqm migrate entries
 *
 * API to retrieve the head from the list of peer ptqm migrate entries
 *
 * Return: Pointer to peer ptqm migrate entry
 */
static
struct peer_ptqm_migrate_list_entry *mlo_ptqm_list_peek_head(
					qdf_list_t *ptqm_list)
{
	struct peer_ptqm_migrate_list_entry *peer_entry;
	qdf_list_node_t *peer_node = NULL;

	if (qdf_list_peek_front(ptqm_list, &peer_node) != QDF_STATUS_SUCCESS)
		return NULL;

	peer_entry = qdf_container_of(peer_node,
				      struct peer_ptqm_migrate_list_entry,
				      node);

	return peer_entry;
}

/**
 * mlo_get_next_peer_ctx() - Return next peer ptqm entry from the list
 *
 * @peer_list:  Pointer to the list of peer ptqm migrate entries
 * @peer_cur: Pointer to the current peer ptqm entry
 *
 * API to retrieve the next node from the list of peer ptqm migrate entries
 *
 * Return: Pointer to peer ptqm migrate entry
 */
static
struct peer_ptqm_migrate_list_entry *mlo_get_next_peer_ctx(
				qdf_list_t *peer_list,
				struct peer_ptqm_migrate_list_entry *peer_cur)
{
	struct peer_ptqm_migrate_list_entry *peer_next;
	qdf_list_node_t *node = &peer_cur->node;
	qdf_list_node_t *next_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (!node)
		return NULL;

	if (qdf_list_peek_next(peer_list, node, &next_node) !=
						QDF_STATUS_SUCCESS)
		return NULL;

	peer_next = qdf_container_of(next_node,
				     struct peer_ptqm_migrate_list_entry,
				     node);
	return peer_next;
}

/**
 * wlan_mlo_send_ptqm_migrate_cmd() - API to send WMI to trigger ptqm migration
 * @vdev: objmgr vdev object
 * @list: peer list to be migrated
 * @num_peers_failed: number of peers for which wmi cmd is failed.
 * This value is expected to be used only in case failure is returned by WMI
 *
 * API to send WMI to trigger ptqm migration
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_send_ptqm_migrate_cmd(struct wlan_objmgr_vdev *vdev,
			       struct peer_migrate_ptqm_multi_entries *list,
			       uint16_t *num_peers_failed)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct peer_ptqm_migrate_params param = {0};
	struct peer_ptqm_migrate_entry *peer_list = NULL;
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;
	struct wlan_mlo_dev_context *ml_dev = NULL;
	uint16_t i = 0;

	ml_dev = vdev->mlo_dev_ctx;
	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		mlo_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;
	if (!mlo_tx_ops || !mlo_tx_ops->peer_ptqm_migrate_send) {
		mlo_err("mlo_tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	param.vdev_id = wlan_vdev_get_id(vdev);
	param.num_peers = list->num_entries;

	param.peer_list = qdf_mem_malloc(sizeof(struct peer_ptqm_migrate_entry) *
					 list->num_entries);
	if (!param.peer_list) {
		mlo_err("Failed to allocate memory for ptqm migration command");
		return QDF_STATUS_E_FAILURE;
	}

	peer_list = param.peer_list;

	peer_entry = mlo_ptqm_list_peek_head(&list->peer_list);
	while (peer_entry) {
		peer_list[i].ml_peer_id = peer_entry->mlo_peer_id;
		peer_list[i].hw_link_id = peer_entry->new_hw_link_id;

		qdf_set_bit(peer_entry->mlo_peer_id,
			    ml_dev->mlo_peer_id_bmap);

		mlo_debug("idx:%d, ml_peer_id:%d, hw_link_id:%d",
			  i, peer_list[i].ml_peer_id,
			  peer_list[i].hw_link_id);
		i++;
		next_entry = mlo_get_next_peer_ctx(&list->peer_list,
						   peer_entry);
		peer_entry = next_entry;
	}

	status = mlo_tx_ops->peer_ptqm_migrate_send(vdev, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Failed to send WMI for ptqm migration");
		*num_peers_failed = param.num_peers_failed;
		qdf_mem_free(param.peer_list);
		return QDF_STATUS_E_FAILURE;
	}

	/* Set timeout equal to peer delete timeout as requested by FW.
	 * Timeout value to be optimized later. Timeout value will be
	 * updated later based on stress testings.
	 */
	qdf_timer_mod(&ml_dev->ptqm_migrate_timer,
		      ML_PRIMARY_TQM_MIGRATRION_TIMEOUT);

	qdf_mem_free(param.peer_list);
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlo_get_new_ptqm_id() - API to get new ptqm ID
 * @curr_vdev: objmgr vdev object for current primary link
 * @ml_peer: ml peer object
 * @new_primary_link_id: new primary link id
 * @new_hw_link_id: hw link id for new primary TQM
 * @force_mig: allow migration to vdevs which are disabled to be pumac
 * using primary_umac_skip ini
 *
 * API to get new ptqm ID
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_get_new_ptqm_id(struct wlan_objmgr_vdev *curr_vdev,
			 struct wlan_mlo_peer_context *ml_peer,
			 uint8_t new_primary_link_id,
			 uint16_t *new_hw_link_id,
			 bool force_mig)
{
	uint8_t current_primary_link_id = WLAN_LINK_ID_INVALID;
	struct wlan_objmgr_vdev *tmp_vdev = NULL;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_objmgr_vdev *tmp_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint8_t psoc_ids[WLAN_UMAC_MLO_MAX_VDEVS];
	struct wlan_objmgr_vdev *link_vdev = NULL;
	struct wlan_objmgr_peer *curr_peer = NULL;
	QDF_STATUS status;
	uint8_t i = 0, idx = 0, j = 0, tmp_cnt = 0;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry || !peer_entry->link_peer)
			continue;

		if (peer_entry->is_primary) {
			curr_peer = peer_entry->link_peer;
			break;
		}
	}

	if (!curr_peer) {
		mlo_err("ML peer " QDF_MAC_ADDR_FMT " current primary link not found",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_mlme_get_opmode(curr_vdev) == QDF_SAP_MODE &&
	    wlan_peer_mlme_get_state(curr_peer) != WLAN_CONNECTED_STATE) {
		mlo_err("ML peer " QDF_MAC_ADDR_FMT " is not authorized",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	*new_hw_link_id = INVALID_HW_LINK_ID;
	current_primary_link_id =
		wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
	if (current_primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_err("ML peer " QDF_MAC_ADDR_FMT "current primary link id is invalid",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (current_primary_link_id == new_primary_link_id) {
		mlo_err("current and requested link_id are same");
		return QDF_STATUS_E_INVAL;
	}

	if (new_primary_link_id != WLAN_LINK_ID_INVALID) {
		link_vdev = mlo_get_vdev_by_link_id(curr_vdev,
						    new_primary_link_id,
						    WLAN_MLO_MGR_ID);
		if (!link_vdev) {
			mlo_err("links vdev not found for link id %d",
				new_primary_link_id);
			return QDF_STATUS_E_INVAL;
		}

		if (wlan_vdev_read_skip_pumac_cnt(link_vdev) > 0) {
			mlo_err("Selected new ptqm link not allowed for migration");
			mlo_release_vdev_ref(link_vdev);
			return QDF_STATUS_E_PERM;
		}
		mlo_release_vdev_ref(link_vdev);
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry || !peer_entry->link_peer)
			continue;

		if (wlan_peer_get_peer_type(peer_entry->link_peer) ==
					WLAN_PEER_MLO_BRIDGE)
			goto exit;

		psoc_ids[j++] = wlan_vdev_get_psoc_id(
				wlan_peer_get_vdev(peer_entry->link_peer));
	}

	/* For some 3 link RDPs, there is restriction on primary umac */
	if (j == 3) {
		if (mlo_get_central_umac_id(psoc_ids) != -1) {
			mlo_err("ML peer " QDF_MAC_ADDR_FMT "migration not supported",
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
			goto exit;
		}
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry || !peer_entry->link_peer)
			continue;

		tmp_vdev = wlan_peer_get_vdev(peer_entry->link_peer);
		if (!tmp_vdev || tmp_vdev == curr_vdev)
			continue;

		status = wlan_objmgr_vdev_try_get_ref(tmp_vdev,
						      WLAN_MLME_SB_ID);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("failed to get vdev ref");
			continue;
		}

		if (wlan_vdev_read_skip_pumac_cnt(tmp_vdev) > 0) {
			mlo_debug("Vdev not allowed for migration, skip this vdev");
			wlan_objmgr_vdev_release_ref(tmp_vdev,
						     WLAN_MLME_SB_ID);
			continue;
		}

		/* Store vdevs which cannot be selected as primary in a temp
		 * list. force_mig flag will be used to allow migration to vdevs
		 * which are not allowed to be selected as primary by using the
		 * primary_umac_skip ini config. This will be helpful in scenarios
		 * where if the current primary link is going down and peer ptqm
		 * needs to be migrated but the partner links ofthat mld are
		 * the user disabled links for ptqm.
		 */
		if (wlan_vdev_skip_pumac(tmp_vdev)) {
			mlo_debug("Vdev cannot be selected as primary");
			tmp_vdev_list[tmp_cnt++] = tmp_vdev;
			continue;
		}
		wlan_vdev_list[idx++] = tmp_vdev;
	}

	if (new_primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_debug("Invalid link id provided, select new link id");
		/* If there are no vdevs present in wlan_vdev_list, means none
		 * of the partner vdevs of current MLD are eligible to become
		 * primary umac. In that case if user has requested force
		 * migration and there are some vdevs present in temp_vdev_list,
		 * select new primary umac from those vdev. If no vdevs are
		 * prenset in any of the list, return failure.
		 */
		if (idx == 0) {
			if (!force_mig || tmp_cnt == 0) {
				mlo_err("No link available to be selected as primary");
				goto exit;
			} else {
				ml_peer->migrate_primary_umac_psoc_id =
					wlan_mld_get_best_primary_umac_w_rssi(
							ml_peer,
							tmp_vdev_list,
							true);
				if (ml_peer->migrate_primary_umac_psoc_id ==
						ML_PRIMARY_UMAC_ID_INVAL) {
					mlo_err("Unable to fetch new primary link id for ml peer " QDF_MAC_ADDR_FMT,
						QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
					goto exit;
				}

				for (i = 0; i < tmp_cnt; i++) {
					if (ml_peer->migrate_primary_umac_psoc_id ==
							wlan_vdev_get_psoc_id(tmp_vdev_list[i])) {
						*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
								wlan_vdev_get_pdev(tmp_vdev_list[i]));
						break;
					}
				}
			}
		} else {
			ml_peer->migrate_primary_umac_psoc_id =
				wlan_mld_get_best_primary_umac_w_rssi(
							ml_peer,
							wlan_vdev_list,
							false);
			if (ml_peer->migrate_primary_umac_psoc_id ==
					ML_PRIMARY_UMAC_ID_INVAL) {
				mlo_err("Unable to fetch new primary link id for ml peer " QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
				goto exit;
			}
			for (i = 0; i < idx; i++) {
				if (ml_peer->migrate_primary_umac_psoc_id ==
						wlan_vdev_get_psoc_id(wlan_vdev_list[i])) {
					*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
							wlan_vdev_get_pdev(wlan_vdev_list[i]));
					break;
				}
			}
		}
	} else {
		/* check if provided link id is part of current ml peer links */
		for (i = 0; i < idx; i++) {
			if (new_primary_link_id == wlan_vdev_get_link_id(wlan_vdev_list[i])) {
				*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
							wlan_vdev_get_pdev(wlan_vdev_list[i]));
				ml_peer->migrate_primary_umac_psoc_id =
						wlan_vdev_get_psoc_id(wlan_vdev_list[i]);
				break;
			}
		}
		if (*new_hw_link_id == INVALID_HW_LINK_ID && force_mig) {
			for (i = 0; i < tmp_cnt; i++) {
				if (new_primary_link_id ==
				    wlan_vdev_get_link_id(tmp_vdev_list[i])) {
					*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
								wlan_vdev_get_pdev(tmp_vdev_list[i]));
					ml_peer->migrate_primary_umac_psoc_id =
							wlan_vdev_get_psoc_id(tmp_vdev_list[i]);
					break;
				}
			}
		}
	}

	if (*new_hw_link_id == INVALID_HW_LINK_ID) {
		mlo_err("New primary link id not found for ml peer " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		goto exit;
	}

	for (i = 0; i < idx; i++)
		wlan_objmgr_vdev_release_ref(wlan_vdev_list[i],
					     WLAN_MLME_SB_ID);

	for (i = 0; i < tmp_cnt; i++)
		wlan_objmgr_vdev_release_ref(tmp_vdev_list[i],
					     WLAN_MLME_SB_ID);
	return QDF_STATUS_SUCCESS;

exit:
	ml_peer->migrate_primary_umac_psoc_id = ML_PRIMARY_UMAC_ID_INVAL;

	for (i = 0; i < idx; i++)
		wlan_objmgr_vdev_release_ref(wlan_vdev_list[i],
					     WLAN_MLME_SB_ID);

	for (i = 0; i < tmp_cnt; i++)
		wlan_objmgr_vdev_release_ref(tmp_vdev_list[i],
					     WLAN_MLME_SB_ID);
	return QDF_STATUS_E_FAILURE;
}

/**
 * wlan_mlo_free_ptqm_migrate_list() - API to free peer ptqm migration list
 * @list: peer ptqm migration list
 *
 * API to free peer ptqm migration list
 *
 * Return: void
 */
static void wlan_mlo_free_ptqm_migrate_list(
			struct peer_migrate_ptqm_multi_entries *list)
{
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;

	peer_entry = mlo_ptqm_list_peek_head(&list->peer_list);
	while (peer_entry) {
		list->num_entries--;
		next_entry = mlo_get_next_peer_ctx(&list->peer_list,
						   peer_entry);
		if (peer_entry->peer)
			wlan_objmgr_peer_release_ref(peer_entry->peer,
						     WLAN_MLME_SB_ID);
		qdf_list_remove_node(&list->peer_list, &peer_entry->node);
		qdf_mem_free(peer_entry);
		peer_entry = next_entry;
	}
	qdf_list_destroy(&list->peer_list);
}

/**
 * wlan_mlo_reset_ptqm_migrate_list() - API to reset peer ptqm migration list
 * @ml_dev: MLO dev context
 * @list: peer ptqm migration list
 * @num_peers_failed: number of peers for which wmi cmd is failed.
 *
 * API to reset peer ptqm migration list
 *
 * Return: void
 */
static void wlan_mlo_reset_ptqm_migrate_list(
			struct wlan_mlo_dev_context *ml_dev,
			struct peer_migrate_ptqm_multi_entries *list,
			uint16_t num_peers_failed)
{
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;
	uint16_t count = 0;

	if (!ml_dev)
		return;

	peer_entry = mlo_ptqm_list_peek_head(&list->peer_list);
	while (peer_entry) {
		/* Reset the flags only for entries for which wmi
		 * command trigger is failed
		 */
		if (count < list->num_entries - num_peers_failed) {
			count++;
			next_entry = mlo_get_next_peer_ctx(&list->peer_list,
							   peer_entry);
			peer_entry = next_entry;
			continue;
		}
		if (peer_entry->peer) {
			qdf_clear_bit(peer_entry->mlo_peer_id, ml_dev->mlo_peer_id_bmap);
			peer_entry->peer->mlo_peer_ctx->primary_umac_migration_in_progress = false;
			peer_entry->peer->mlo_peer_ctx->migrate_primary_umac_psoc_id =
							ML_PRIMARY_UMAC_ID_INVAL;
		}
		next_entry = mlo_get_next_peer_ctx(&list->peer_list,
						   peer_entry);
		peer_entry = next_entry;
	}
}

/**
 * wlan_mlo_build_ptqm_migrate_list() - API to build peer ptqm migration list
 * @vdev: objmgr vdev list
 * @object: peer object
 * @arg: list pointer
 *
 * API to build peer ptqm migration list
 *
 * Return: void
 */
static void wlan_mlo_build_ptqm_migrate_list(struct wlan_objmgr_vdev *vdev,
					     void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct peer_migrate_ptqm_multi_entries *list =
				(struct peer_migrate_ptqm_multi_entries *)arg;
	struct peer_ptqm_migrate_list_entry *peer_entry;
	struct wlan_mlo_peer_context *ml_peer;
	uint16_t new_hw_link_id = INVALID_HW_LINK_ID;
	uint8_t current_primary_link_id = WLAN_LINK_ID_INVALID;
	QDF_STATUS status;

	if (!wlan_peer_is_mlo(peer) || !peer->mlo_peer_ctx)
		return;

	ml_peer = peer->mlo_peer_ctx;

	if (ml_peer->link_peer_cnt == 1)
		return;

	if (ml_peer->primary_umac_migration_in_progress) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " primary umac migration already in progress",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	current_primary_link_id = wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
	if (current_primary_link_id == WLAN_LINK_ID_INVALID ||
	    current_primary_link_id != wlan_vdev_get_link_id(vdev)) {
		mlo_debug("peer " QDF_MAC_ADDR_FMT " not having primary on current vdev",
			  QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	status = wlan_mlo_get_new_ptqm_id(vdev, ml_peer,
					  WLAN_LINK_ID_INVALID,
					  &new_hw_link_id, true);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " unable to get new ptqm id",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}
	ml_peer->primary_umac_migration_in_progress = true;

	peer_entry = (struct peer_ptqm_migrate_list_entry *)
			qdf_mem_malloc(sizeof(struct peer_ptqm_migrate_list_entry));
	if (!peer_entry) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " unable to allocate peer entry",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	status = wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID);
	peer_entry->peer = peer;
	peer_entry->new_hw_link_id = new_hw_link_id;
	peer_entry->mlo_peer_id = ml_peer->mlo_peer_id;
	qdf_list_insert_back(&list->peer_list, &peer_entry->node);
	list->num_entries++;
}

/**
 * wlan_mlo_trigger_link_ptqm_migration() - API to trigger ptqm migration
 * for a link
 * @vdev: objmgr vdev object
 *
 * API to trigger ptqm migration of all peers having primary on given link
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_trigger_link_ptqm_migration(
				struct wlan_objmgr_vdev *vdev)
{
	struct peer_migrate_ptqm_multi_entries migrate_list = {0};
	QDF_STATUS status;
	uint16_t num_peers_failed = 0;

	qdf_list_create(&migrate_list.peer_list, MAX_MLO_PEER_ID);
	wlan_objmgr_iterate_peerobj_list(vdev,
					 wlan_mlo_build_ptqm_migrate_list,
					 &migrate_list, WLAN_MLME_NB_ID);

	/* trigger WMI */
	if (migrate_list.num_entries == 0) {
		mlo_err("No peer found");
		return QDF_STATUS_SUCCESS;
	}

	status = wlan_mlo_send_ptqm_migrate_cmd(vdev, &migrate_list,
						&num_peers_failed);
	if (QDF_IS_STATUS_ERROR(status))
		wlan_mlo_reset_ptqm_migrate_list(vdev->mlo_dev_ctx,
						 &migrate_list,
						 num_peers_failed);
	wlan_mlo_free_ptqm_migrate_list(&migrate_list);
	return status;
}

QDF_STATUS wlan_mlo_set_ptqm_migration(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlo_peer_context *ml_peer,
				       bool link_migration,
				       uint32_t link_id, bool force_mig)
{
	uint16_t new_hw_link_id = INVALID_HW_LINK_ID;
	struct peer_migrate_ptqm_multi_entries migrate_list = {0};
	struct peer_ptqm_migrate_list_entry *peer_entry;
	struct wlan_objmgr_vdev *curr_vdev = NULL;
	uint8_t current_primary_link_id = WLAN_LINK_ID_INVALID;
	uint16_t num_peers_failed = 0;
	QDF_STATUS status;

	if (!vdev) {
		mlo_err("Vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (link_migration == false && !ml_peer) {
		mlo_err("ML peer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (link_migration) {
		mlo_info("Trigger migration for full link");
		// trigger full link migration
		status = wlan_mlo_trigger_link_ptqm_migration(vdev);
		if (QDF_IS_STATUS_ERROR(status))
			mlo_err("Failed to trigger link migration");
		return status;
	}

	if (ml_peer->link_peer_cnt == 1) {
		mlo_info("peer " QDF_MAC_ADDR_FMT " is SLO",
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_FAILURE;
	}

	if (ml_peer->primary_umac_migration_in_progress) {
		mlo_info("peer " QDF_MAC_ADDR_FMT " primary umac migration already in progress",
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_FAILURE;
	}

	current_primary_link_id = wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
	if (current_primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_err("Current primary link id is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	curr_vdev = mlo_get_vdev_by_link_id(vdev, current_primary_link_id,
					    WLAN_MLO_MGR_ID);
	if (!curr_vdev) {
		mlo_err("Unable to get current primary vdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = wlan_mlo_get_new_ptqm_id(curr_vdev, ml_peer,
					  link_id, &new_hw_link_id, force_mig);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " unable to get new ptqm id",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		goto exit;
	}
	ml_peer->primary_umac_migration_in_progress = true;

	peer_entry = (struct peer_ptqm_migrate_list_entry *)
				qdf_mem_malloc(sizeof(struct peer_ptqm_migrate_list_entry));
	if (!peer_entry) {
		mlo_err("Failed to allocate peer entry");
		status = QDF_STATUS_E_NULL_VALUE;
		goto exit;
	}

	peer_entry->new_hw_link_id = new_hw_link_id;
	peer_entry->mlo_peer_id = ml_peer->mlo_peer_id;
	qdf_list_create(&migrate_list.peer_list, MAX_MLO_PEER_ID);
	qdf_list_insert_back(&migrate_list.peer_list, &peer_entry->node);
	migrate_list.num_entries = 1;

	//trigger WMI
	status = wlan_mlo_send_ptqm_migrate_cmd(curr_vdev, &migrate_list,
						&num_peers_failed);
	if (QDF_IS_STATUS_ERROR(status))
		wlan_mlo_reset_ptqm_migrate_list(curr_vdev->mlo_dev_ctx,
						 &migrate_list,
						 num_peers_failed);
	wlan_mlo_free_ptqm_migrate_list(&migrate_list);

exit:
	if (curr_vdev)
		mlo_release_vdev_ref(curr_vdev);

	return status;
}
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */

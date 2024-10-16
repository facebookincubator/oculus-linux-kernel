/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

	if (!wlan_peer_is_mlo(peer) || !mlo_peer_ctx) {
		if (wlan_peer_get_peer_type(peer) == WLAN_PEER_STA)
			tqm_params->num_non_ml_peers += 1;
		return;
	}

	/* If this psoc is not primary UMAC, don't account RSSI */
	if (mlo_peer_ctx->primary_umac_psoc_id != rssi_data->current_psoc_id)
		return;

	tqm_params->total_rssi += mlo_peer_ctx->avg_link_rssi;
	tqm_params->num_ml_peers += 1;
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

static void
mld_get_best_primary_umac_w_rssi(struct wlan_mlo_peer_context *ml_peer,
				 struct wlan_objmgr_vdev *link_vdevs[])
{
	struct mlo_all_link_rssi rssi_data;
	uint8_t i;
	int32_t avg_rssi[WLAN_OBJMGR_MAX_DEVICES] = {0};
	int32_t diff_rssi[WLAN_OBJMGR_MAX_DEVICES] = {0};
	int32_t diff_low;
	bool mld_sta_links[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint8_t num_psocs_w_no_sta = 0;
	struct wlan_objmgr_peer *assoc_peer = NULL;
	uint8_t prim_link, id;
	uint8_t num_psocs;
	struct mlpeer_data *tqm_params = NULL;

	mld_get_link_rssi(&rssi_data);

	for (i = 0; i < rssi_data.num_psocs; i++) {
		tqm_params = &rssi_data.psoc_tqm_parms[i];

		if (tqm_params->num_ml_peers)
			avg_rssi[i] = (tqm_params->total_rssi /
				       tqm_params->num_ml_peers);
		else
			num_psocs_w_no_sta++;
	}

	assoc_peer = wlan_mlo_peer_get_assoc_peer(ml_peer);
	if (!assoc_peer) {
		mlo_err("Assoc peer of ML Peer " QDF_MAC_ADDR_FMT " is invalid",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		QDF_BUG(0);
		return;
	}

	/**
	 * If this is first station, then assign primary umac to
	 * assoc peer's psoc
	 */
	if (num_psocs_w_no_sta == rssi_data.num_psocs) {
		ml_peer->primary_umac_psoc_id =
			wlan_peer_get_psoc_id(assoc_peer);
		return;
	}

	/**
	 * If MLD STA associated to a set of links, choose primary UMAC
	 * from those links only
	 */
	num_psocs = 0;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!link_vdevs[i])
			continue;

		id = wlan_vdev_get_psoc_id(link_vdevs[i]);
		if (id >= WLAN_OBJMGR_MAX_DEVICES)
			continue;

		tqm_params = &rssi_data.psoc_tqm_parms[id];
		mld_sta_links[id] = true;

		/* If this PSOC has exceeded limit, skip it */
		if ((tqm_params->num_ml_peers +
		     tqm_params->num_non_ml_peers) >=
		     tqm_params->max_ml_peers) {
			mld_sta_links[id] = false;
			continue;
		}

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

	if (prim_link != 0xff)
		ml_peer->primary_umac_psoc_id = prim_link;
	else
		ml_peer->primary_umac_psoc_id =
			wlan_peer_get_psoc_id(assoc_peer);
}

void mlo_peer_assign_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_mlo_link_peer_entry *peer_entry)
{
	struct wlan_mlo_link_peer_entry *peer_ent_iter;
	uint8_t i;
	uint8_t primary_umac_set = 0;

	/* If MLD is within single SOC, then assoc link becomes
	 * primary umac
	 */
	if (ml_peer->primary_umac_psoc_id == ML_PRIMARY_UMAC_ID_INVAL) {
		if (wlan_peer_mlme_is_assoc_peer(peer_entry->link_peer)) {
			peer_entry->is_primary = true;
			ml_peer->primary_umac_psoc_id =
				wlan_peer_get_psoc_id(peer_entry->link_peer);
		} else {
			peer_entry->is_primary = false;
		}
	} else {
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
	if ((ml_peer->max_links == 1) ||
	    (mlo_vdevs_check_single_soc(link_vdevs, ml_peer->max_links))) {
		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d Assoc peer " QDF_MAC_ADDR_FMT " primary umac soc %d ",
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

	mld_get_best_primary_umac_w_rssi(ml_peer, link_vdevs);

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

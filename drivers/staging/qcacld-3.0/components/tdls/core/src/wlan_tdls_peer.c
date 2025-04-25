/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: wlan_tdls_peer.c
 *
 * TDLS peer basic operations
 */
#include "wlan_tdls_main.h"
#include "wlan_tdls_peer.h"
#include <wlan_reg_services_api.h>
#include <wlan_utility.h>
#include <wlan_policy_mgr_api.h>
#include "wlan_reg_ucfg_api.h"
#include <host_diag_core_event.h>
#include "wlan_policy_mgr_api.h"

static uint8_t calculate_hash_key(const uint8_t *macaddr)
{
	uint8_t i, key;

	for (i = 0, key = 0; i < 6; i++)
		key ^= macaddr[i];

	return key % WLAN_TDLS_PEER_LIST_SIZE;
}

struct tdls_peer *tdls_find_peer(struct tdls_vdev_priv_obj *vdev_obj,
				 const uint8_t *macaddr)
{
	uint8_t key;
	QDF_STATUS status;
	struct tdls_peer *peer;
	qdf_list_t *head;
	qdf_list_node_t *p_node;

	key = calculate_hash_key(macaddr);
	head = &vdev_obj->peer_list[key];

	status = qdf_list_peek_front(head, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		peer = qdf_container_of(p_node, struct tdls_peer, node);
		if (WLAN_ADDR_EQ(&peer->peer_mac, macaddr)
		    == QDF_STATUS_SUCCESS) {
			return peer;
		}
		status = qdf_list_peek_next(head, p_node, &p_node);
	}

	tdls_debug("no tdls peer " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(macaddr));
	return NULL;
}

/**
 * tdls_find_peer_handler() - helper function for tdls_find_all_peer
 * @psoc: soc object
 * @obj: vdev object
 * @arg: used to keep search peer parameters
 *
 * Return: None.
 */
static void
tdls_find_peer_handler(struct wlan_objmgr_psoc *psoc, void *obj, void *arg)
{
	struct wlan_objmgr_vdev *vdev = obj;
	struct tdls_search_peer_param *tdls_param = arg;
	struct tdls_vdev_priv_obj *vdev_obj;

	if (tdls_param->peer)
		return;

	if (!vdev) {
		tdls_err("invalid vdev");
		return;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE &&
	    wlan_vdev_mlme_get_opmode(vdev) != QDF_P2P_CLIENT_MODE)
		return;

	vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							 WLAN_UMAC_COMP_TDLS);
	if (!vdev_obj)
		return;

	tdls_param->peer = tdls_find_peer(vdev_obj, tdls_param->macaddr);
}

struct tdls_peer *
tdls_find_all_peer(struct tdls_soc_priv_obj *soc_obj, const uint8_t *macaddr)
{
	struct tdls_search_peer_param tdls_search_param;
	struct wlan_objmgr_psoc *psoc;

	if (!soc_obj) {
		tdls_err("tdls soc object is NULL");
		return NULL;
	}

	psoc = soc_obj->soc;
	if (!psoc) {
		tdls_err("psoc is NULL");
		return NULL;
	}
	tdls_search_param.macaddr = macaddr;
	tdls_search_param.peer = NULL;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     tdls_find_peer_handler,
				     &tdls_search_param, 0, WLAN_TDLS_NB_ID);

	return tdls_search_param.peer;
}

uint8_t tdls_find_opclass(struct wlan_objmgr_psoc *psoc, uint8_t channel,
				 uint8_t bw_offset)
{
	char country[REG_ALPHA2_LEN + 1];
	QDF_STATUS status;

	if (!psoc) {
		tdls_err("psoc is NULL");
		return 0;
	}

	status = wlan_reg_read_default_country(psoc, country);
	if (QDF_IS_STATUS_ERROR(status))
		return 0;

	return wlan_reg_dmn_get_opclass_from_channel(country, channel,
						     bw_offset);
}

#ifdef WLAN_FEATURE_11AX
qdf_freq_t tdls_get_offchan_freq(struct wlan_objmgr_vdev *vdev,
				 struct tdls_soc_priv_obj *soc_obj)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	qdf_freq_t pref_freq, pref_6g_freq;
	uint8_t pref_non6g_ch;

	if (!pdev) {
		tdls_err("pdev is NULL");
		return 0;
	}

	pref_6g_freq = soc_obj->tdls_configs.tdls_pre_off_chan_freq_6g;
	pref_non6g_ch = soc_obj->tdls_configs.tdls_pre_off_chan_num;

	/*
	 * Fill preferred offchannel frequency here. If TDLS on 6 GHz is
	 * allowed then fill pref 6 GHz frequency
	 * Otherwise, fill 5 GHz preferred frequency
	 */
	if (pref_6g_freq && tdls_is_6g_freq_allowed(vdev, pref_6g_freq))
		pref_freq = pref_6g_freq;
	else
		pref_freq = wlan_reg_legacy_chan_to_freq(pdev, pref_non6g_ch);

	return pref_freq;
}

#else
qdf_freq_t tdls_get_offchan_freq(struct wlan_objmgr_vdev *vdev,
				 struct tdls_soc_priv_obj *soc_obj)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	uint32_t pref_leg_chan = soc_obj->tdls_configs.tdls_pre_off_chan_num;

	if (!pdev) {
		tdls_err("pdev is NULL");
		return 0;
	}

	return wlan_reg_legacy_chan_to_freq(pdev, pref_leg_chan);
}
#endif

uint32_t tdls_get_offchan_bw(struct tdls_soc_priv_obj *soc_obj,
			     qdf_freq_t off_chan_freq)
{
	uint32_t pre_off_chan_bw;

	if (wlan_reg_is_5ghz_ch_freq(off_chan_freq) &&
	    CHECK_BIT(soc_obj->tdls_configs.tdls_pre_off_chan_bw,
		      BW_160_OFFSET_BIT))
		pre_off_chan_bw = soc_obj->tdls_configs.tdls_pre_off_chan_bw &
						~(1 << BW_160_OFFSET_BIT);
	else
		pre_off_chan_bw = soc_obj->tdls_configs.tdls_pre_off_chan_bw;

	return pre_off_chan_bw;
}

static void tdls_fill_pref_off_chan_info(struct tdls_vdev_priv_obj *vdev_obj,
					 struct tdls_soc_priv_obj *soc_obj,
					 struct tdls_peer *peer)
{
	peer->pref_off_chan_freq = tdls_get_offchan_freq(vdev_obj->vdev,
							 soc_obj);
	peer->pref_off_chan_width = tdls_get_offchan_bw(soc_obj,
						      peer->pref_off_chan_freq);
	tdls_debug("Pref off channel freq %d chan width %d",
		   peer->pref_off_chan_freq, peer->pref_off_chan_width);
}

static QDF_STATUS
tdls_remove_first_idle_peer(qdf_list_t *head) {
	QDF_STATUS status;
	qdf_list_node_t *p_node;
	struct tdls_peer *peer;

	status = qdf_list_peek_front(head, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		peer = qdf_container_of(p_node, struct tdls_peer, node);
		if (peer && peer->link_status == TDLS_LINK_IDLE) {
			if (peer->is_peer_idle_timer_initialised) {
				tdls_debug(QDF_MAC_ADDR_FMT
					": destroy  idle timer ",
					QDF_MAC_ADDR_REF(
					peer->peer_mac.bytes));
				qdf_mc_timer_stop(&peer->peer_idle_timer);
				qdf_mc_timer_destroy(&peer->peer_idle_timer);
			}

			tdls_debug(QDF_MAC_ADDR_FMT ": free peer",
				   QDF_MAC_ADDR_REF(peer->peer_mac.bytes));
			qdf_list_remove_node(head, p_node);
			qdf_mem_free(peer);

			return status;
		}
		status = qdf_list_peek_next(head, p_node, &p_node);
	}

	return QDF_STATUS_E_INVAL;
}

/**
 * tdls_add_peer() - add TDLS peer in TDLS vdev object
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of peer
 *
 * Allocate memory for the new peer, and add it to hash table.
 *
 * Return: new added TDLS peer, NULL if failed.
 */
static struct tdls_peer *tdls_add_peer(struct tdls_vdev_priv_obj *vdev_obj,
				       const uint8_t *macaddr)
{
	struct tdls_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	uint8_t key = 0;
	qdf_list_t *head;
	uint8_t reg_bw_offset;

	peer = qdf_mem_malloc(sizeof(*peer));
	if (!peer)
		return NULL;

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
	if (!soc_obj) {
		tdls_err("NULL tdls soc object");
		return NULL;
	}

	key = calculate_hash_key(macaddr);
	head = &vdev_obj->peer_list[key];

	qdf_mem_copy(&peer->peer_mac, macaddr, sizeof(peer->peer_mac));
	peer->vdev_priv = vdev_obj;

	tdls_fill_pref_off_chan_info(vdev_obj, soc_obj, peer);
	peer->op_class_for_pref_off_chan =
		tdls_get_opclass_from_bandwidth(
				vdev_obj->vdev, peer->pref_off_chan_freq,
				peer->pref_off_chan_width,
				&reg_bw_offset);

	peer->valid_entry = false;

	if (qdf_list_size(head) >= qdf_list_max_size(head)) {
		if (QDF_IS_STATUS_ERROR(tdls_remove_first_idle_peer(head))) {
			tdls_err("list size exceed max and remove idle peer failed, key %d",
				 key);
			qdf_mem_free(peer);
			return NULL;
		}
	}

	qdf_list_insert_back(head, &peer->node);

	tdls_debug("add tdls peer: " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(macaddr));
	return peer;
}

struct tdls_peer *tdls_get_peer(struct tdls_vdev_priv_obj *vdev_obj,
				const uint8_t *macaddr)
{
	struct tdls_peer *peer;

	peer = tdls_find_peer(vdev_obj, macaddr);
	if (!peer)
		peer = tdls_add_peer(vdev_obj, macaddr);

	return peer;
}

static struct tdls_peer *
tdls_find_progress_peer_in_list(qdf_list_t *head,
				const uint8_t *macaddr, uint8_t skip_self)
{
	QDF_STATUS status;
	struct tdls_peer *peer;
	qdf_list_node_t *p_node;

	status = qdf_list_peek_front(head, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		peer = qdf_container_of(p_node, struct tdls_peer, node);
		if (skip_self && macaddr &&
		    WLAN_ADDR_EQ(&peer->peer_mac, macaddr)
		    == QDF_STATUS_SUCCESS) {
			status = qdf_list_peek_next(head, p_node, &p_node);
			continue;
		} else if (TDLS_LINK_CONNECTING == peer->link_status) {
			tdls_debug(QDF_MAC_ADDR_FMT " TDLS_LINK_CONNECTING",
				   QDF_MAC_ADDR_REF(peer->peer_mac.bytes));
			return peer;
		}
		status = qdf_list_peek_next(head, p_node, &p_node);
	}

	return NULL;
}

/**
 * tdls_find_progress_peer() - find the peer with ongoing TDLS progress
 *                             on present vdev
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of peer, if NULL check for all the peer list
 * @skip_self: If true, skip this macaddr. Otherwise, check all the peer list.
 *             if macaddr is NULL, this argument is ignored, and check for all
 *             the peer list.
 *
 * Return: Pointer to tdls_peer if TDLS is ongoing. Otherwise return NULL.
 */
static struct tdls_peer *
tdls_find_progress_peer(struct tdls_vdev_priv_obj *vdev_obj,
			const uint8_t *macaddr, uint8_t skip_self)
{
	uint8_t i;
	struct tdls_peer *peer;
	qdf_list_t *head;

	if (!vdev_obj) {
		tdls_err("invalid tdls vdev object");
		return NULL;
	}

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &vdev_obj->peer_list[i];

		peer = tdls_find_progress_peer_in_list(head, macaddr,
						       skip_self);
		if (peer)
			return peer;
	}

	return NULL;
}

/**
 * tdls_find_progress_peer_handler() - helper function for tdls_is_progress
 * @psoc: soc object
 * @obj: vdev object
 * @arg: used to keep search peer parameters
 *
 * Return: None.
 */
static void
tdls_find_progress_peer_handler(struct wlan_objmgr_psoc *psoc,
				void *obj, void *arg)
{
	struct wlan_objmgr_vdev *vdev = obj;
	struct tdls_search_progress_param *tdls_progress = arg;
	struct tdls_vdev_priv_obj *vdev_obj;

	if (tdls_progress->peer)
		return;

	if (!vdev) {
		tdls_err("invalid vdev");
		return;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE &&
	    wlan_vdev_mlme_get_opmode(vdev) != QDF_P2P_CLIENT_MODE)
		return;

	vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							 WLAN_UMAC_COMP_TDLS);

	tdls_progress->peer = tdls_find_progress_peer(vdev_obj,
						      tdls_progress->macaddr,
						      tdls_progress->skip_self);
}

struct tdls_peer *tdls_is_progress(struct tdls_vdev_priv_obj *vdev_obj,
				   const uint8_t *macaddr, uint8_t skip_self)
{
	struct tdls_search_progress_param tdls_progress;
	struct wlan_objmgr_psoc *psoc;

	if (!vdev_obj) {
		tdls_err("invalid tdls vdev object");
		return NULL;
	}

	psoc = wlan_vdev_get_psoc(vdev_obj->vdev);
	if (!psoc) {
		tdls_err("invalid psoc");
		return NULL;
	}
	tdls_progress.macaddr = macaddr;
	tdls_progress.skip_self = skip_self;
	tdls_progress.peer = NULL;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     tdls_find_progress_peer_handler,
				     &tdls_progress, 0, WLAN_TDLS_NB_ID);

	return tdls_progress.peer;
}

struct tdls_peer *
tdls_find_first_connected_peer(struct tdls_vdev_priv_obj *vdev_obj)
{
	uint16_t i;
	struct tdls_peer *peer;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	if (!vdev_obj) {
		tdls_err("invalid tdls vdev object");
		return NULL;
	}

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &vdev_obj->peer_list[i];

		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			peer = qdf_container_of(p_node, struct tdls_peer, node);

			if (peer && TDLS_LINK_CONNECTED == peer->link_status) {
				tdls_debug(QDF_MAC_ADDR_FMT
					   " TDLS_LINK_CONNECTED",
					   QDF_MAC_ADDR_REF(
						   peer->peer_mac.bytes));
				return peer;
			}
			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}

	return NULL;
}

/**
 * tdls_determine_channel_opclass() - determine channel and opclass
 * @soc_obj: TDLS soc object
 * @vdev_obj: TDLS vdev object
 * @peer: TDLS peer
 * @channel: pointer to channel
 * @opclass: pointer to opclass
 *
 * Function determines the channel and operating class
 *
 * Return: None.
 */
static void tdls_determine_channel_opclass(struct tdls_soc_priv_obj *soc_obj,
					   struct tdls_vdev_priv_obj *vdev_obj,
					   struct tdls_peer *peer,
					   uint32_t *channel, uint32_t *opclass)
{
	uint32_t vdev_id;
	enum QDF_OPMODE opmode;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	enum policy_mgr_con_mode mode;
	/*
	 * If tdls offchannel is not enabled then we provide base channel
	 * and in that case pass opclass as 0 since opclass is mainly needed
	 * for offchannel cases.
	 */
	if (!(TDLS_IS_OFF_CHANNEL_ENABLED(
		      soc_obj->tdls_configs.tdls_feature_flags)) ||
	      soc_obj->tdls_fw_off_chan_mode != ENABLE_CHANSWITCH) {
		vdev_id = wlan_vdev_get_id(vdev_obj->vdev);
		opmode = wlan_vdev_mlme_get_opmode(vdev_obj->vdev);
		pdev = wlan_vdev_get_pdev(vdev_obj->vdev);
		psoc = wlan_pdev_get_psoc(pdev);

		mode = policy_mgr_qdf_opmode_to_pm_con_mode(psoc, opmode,
							    vdev_id);
		*channel = wlan_reg_freq_to_chan(pdev, policy_mgr_get_channel(
						 soc_obj->soc,
						 mode,
						 &vdev_id));
		*opclass = 0;
	} else {
		*channel = wlan_reg_freq_to_chan(pdev,
						 peer->pref_off_chan_freq);
		*opclass = peer->op_class_for_pref_off_chan;
	}
	tdls_debug("channel:%d opclass:%d", *channel, *opclass);
}

/**
 * tdls_get_wifi_hal_state() - get TDLS wifi hal state on current peer
 * @peer: TDLS peer
 * @state: output parameter to store the TDLS wifi hal state
 * @reason: output parameter to store the reason of the current peer
 *
 * Return: None.
 */
static void tdls_get_wifi_hal_state(struct tdls_peer *peer, uint32_t *state,
				    int32_t *reason)
{
	struct wlan_objmgr_vdev *vdev;
	struct tdls_soc_priv_obj *soc_obj;

	vdev = peer->vdev_priv->vdev;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!soc_obj) {
		tdls_err("can't get tdls object");
		return;
	}

	*reason = peer->reason;

	switch (peer->link_status) {
	case TDLS_LINK_IDLE:
	case TDLS_LINK_DISCOVERED:
	case TDLS_LINK_DISCOVERING:
	case TDLS_LINK_CONNECTING:
		*state = QCA_WIFI_HAL_TDLS_S_ENABLED;
		break;
	case TDLS_LINK_CONNECTED:
		if ((TDLS_IS_OFF_CHANNEL_ENABLED(
			     soc_obj->tdls_configs.tdls_feature_flags)) &&
		     (soc_obj->tdls_fw_off_chan_mode == ENABLE_CHANSWITCH))
			*state = QCA_WIFI_HAL_TDLS_S_ESTABLISHED_OFF_CHANNEL;
		else
			*state = QCA_WIFI_HAL_TDLS_S_ENABLED;
		break;
	case TDLS_LINK_TEARING:
		*state = QCA_WIFI_HAL_TDLS_S_DROPPED;
		break;
	}
}

#ifdef WLAN_FEATURE_TDLS_CONCURRENCIES
/**
 * tdls_get_allowed_off_channel_for_concurrency() - Get allowed off-channel
 * frequency based on current concurrency. Return 0 if all frequencies are
 * allowed
 * @pdev: Pointer to PDEV object
 * @vdev: Pointer to vdev object
 *
 * Return: Frequency
 */
static inline qdf_freq_t
tdls_get_allowed_off_channel_for_concurrency(struct wlan_objmgr_pdev *pdev,
					     struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	qdf_freq_t freq = 0;

	if (!psoc)
		return 0;

	if (!wlan_psoc_nif_fw_ext2_cap_get(psoc,
					   WLAN_TDLS_CONCURRENCIES_SUPPORT))
		return 0;

	if (!policy_mgr_get_allowed_tdls_offchannel_freq(psoc, vdev, &freq)) {
		tdls_debug("off channel not allowed for current concurrency");
		return 0;
	}

	return freq;
}
#else
static inline qdf_freq_t
tdls_get_allowed_off_channel_for_concurrency(struct wlan_objmgr_pdev *pdev,
					     struct wlan_objmgr_vdev *vdev)
{
	return 0;
}
#endif

/**
 * tdls_extract_peer_state_param() - extract peer update params from TDLS peer
 * @peer_param: output peer update params
 * @peer: TDLS peer
 *
 * This is used when enable TDLS link
 *
 * Return: None.
 */
void tdls_extract_peer_state_param(struct tdls_peer_update_state *peer_param,
				   struct tdls_peer *peer)
{
	uint16_t i, num;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_soc_priv_obj *soc_obj;
	enum channel_state ch_state;
	struct wlan_objmgr_pdev *pdev;
	uint32_t cur_band;
	qdf_freq_t ch_freq, allowed_freq;
	uint32_t tx_power = 0;

	vdev_obj = peer->vdev_priv;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
	pdev = wlan_vdev_get_pdev(vdev_obj->vdev);
	if (!soc_obj || !pdev) {
		tdls_err("soc_obj: %pK, pdev: %pK", soc_obj, pdev);
		return;
	}

	qdf_mem_zero(peer_param, sizeof(*peer_param));
	peer_param->vdev_id = wlan_vdev_get_id(vdev_obj->vdev);

	qdf_mem_copy(peer_param->peer_macaddr,
		     peer->peer_mac.bytes, QDF_MAC_ADDR_SIZE);
	peer_param->peer_state = TDLS_PEER_STATE_CONNECTED;
	peer_param->peer_cap.is_peer_responder = peer->is_responder;
	peer_param->peer_cap.peer_uapsd_queue = peer->uapsd_queues;
	peer_param->peer_cap.peer_max_sp = peer->max_sp;
	peer_param->peer_cap.peer_buff_sta_support = peer->buf_sta_capable;
	peer_param->peer_cap.peer_off_chan_support =
		peer->off_channel_capable;
	peer_param->peer_cap.peer_curr_operclass = 0;
	peer_param->peer_cap.self_curr_operclass =
			peer->op_class_for_pref_off_chan;
	peer_param->peer_cap.pref_off_channum = wlan_reg_freq_to_chan(pdev,
						      peer->pref_off_chan_freq);
	peer_param->peer_cap.pref_off_chan_bandwidth =
						peer->pref_off_chan_width;
	peer_param->peer_cap.opclass_for_prefoffchan =
		peer->op_class_for_pref_off_chan;
	peer_param->peer_cap.pref_offchan_freq = peer->pref_off_chan_freq;

	if (QDF_STATUS_SUCCESS != ucfg_reg_get_band(pdev, &cur_band)) {
		tdls_err("not able get the current frequency band");
		return;
	}

	if (BIT(REG_BAND_2G) == cur_band) {
		tdls_err("sending the offchannel value as 0 as only 2g is supported");
		peer_param->peer_cap.pref_off_channum = 0;
		peer_param->peer_cap.opclass_for_prefoffchan = 0;
		peer_param->peer_cap.pref_offchan_freq = 0;
	}

	ch_freq = peer->pref_off_chan_freq;
	if (wlan_reg_is_dfs_for_freq(pdev, ch_freq)) {
		/*
		 * If pref_off_chan_freq is DFS frequency, that means it is 5Ghz
		 * case. So, reset to default 5 GHz frequency
		 */
		tdls_err("Resetting TDLS off-channel from %d to %d",
			 peer_param->peer_cap.pref_off_channum,
			 WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_DEF);
		peer_param->peer_cap.pref_off_channum =
			WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_DEF;
		peer_param->peer_cap.pref_offchan_freq =
			WLAN_TDLS_PREFERRED_OFF_CHANNEL_FRQ_DEF;
	}

	num = 0;
	allowed_freq =
		tdls_get_allowed_off_channel_for_concurrency(pdev,
							     vdev_obj->vdev);
	tdls_debug("allowed freq:%u", allowed_freq);

	for (i = 0; i < peer->supported_channels_len; i++) {
		ch_freq = peer->supported_chan_freq[i];
		if (allowed_freq && allowed_freq != ch_freq)
			continue;

		ch_state = wlan_reg_get_channel_state_for_pwrmode(
							pdev, ch_freq,
							REG_CURRENT_PWR_MODE);

		if (CHANNEL_STATE_INVALID != ch_state &&
		    !wlan_reg_is_dfs_for_freq(pdev, ch_freq) &&
		    !wlan_reg_is_dsrc_freq(ch_freq)) {
			peer_param->peer_cap.peer_chan[num].ch_freq = ch_freq;
			if (!wlan_reg_is_6ghz_chan_freq(ch_freq)) {
				tx_power =
				wlan_reg_get_channel_reg_power_for_freq(pdev,
								       ch_freq);
			} else {
				tx_power =
				tdls_get_6g_pwr_for_power_type(vdev_obj->vdev,
							       ch_freq,
							       REG_CLI_DEF_VLP);
			}
			peer_param->peer_cap.peer_chan[num].pwr = tx_power;
			peer_param->peer_cap.peer_chan[num].dfs_set = false;
			peer_param->peer_cap.peer_chanlen++;
			num++;
		}
	}

	peer_param->peer_cap.peer_oper_classlen =
		peer->supported_oper_classes_len;
	for (i = 0; i < peer->supported_oper_classes_len; i++)
		peer_param->peer_cap.peer_oper_class[i] =
			peer->supported_oper_classes[i];
}

#ifdef TDLS_WOW_ENABLED
/**
 * tdls_prevent_suspend(): Prevent suspend for TDLS
 * @tdls_soc: TDLS soc object
 *
 * Acquire wake lock and prevent suspend for TDLS
 *
 * Return None
 */
static void tdls_prevent_suspend(struct tdls_soc_priv_obj *tdls_soc)
{
	if (tdls_soc->is_prevent_suspend)
		return;

	qdf_wake_lock_acquire(&tdls_soc->wake_lock,
			      WIFI_POWER_EVENT_WAKELOCK_TDLS);
	qdf_runtime_pm_prevent_suspend(&tdls_soc->runtime_lock);
	tdls_soc->is_prevent_suspend = true;
	tdls_debug("Acquire WIFI_POWER_EVENT_WAKELOCK_TDLS");
}

/**
 * tdls_allow_suspend(): Allow suspend for TDLS
 * @tdls_soc: TDLS soc object
 *
 * Release wake lock and allow suspend for TDLS
 *
 * Return None
 */
static void tdls_allow_suspend(struct tdls_soc_priv_obj *tdls_soc)
{
	if (!tdls_soc->is_prevent_suspend)
		return;

	qdf_wake_lock_release(&tdls_soc->wake_lock,
			      WIFI_POWER_EVENT_WAKELOCK_TDLS);
	qdf_runtime_pm_allow_suspend(&tdls_soc->runtime_lock);
	tdls_soc->is_prevent_suspend = false;
	tdls_debug("Release WIFI_POWER_EVENT_WAKELOCK_TDLS");
}

/**
 * tdls_update_pmo_status() - Update PMO status by TDLS status
 * @tdls_vdev: TDLS vdev object
 * @old_status: old link status
 * @new_status: new link status
 *
 * Return: None.
 */
static void tdls_update_pmo_status(struct tdls_vdev_priv_obj *tdls_vdev,
				   enum tdls_link_state old_status,
				   enum tdls_link_state new_status)
{
	struct tdls_soc_priv_obj *tdls_soc;

	tdls_soc = wlan_vdev_get_tdls_soc_obj(tdls_vdev->vdev);
	if (!tdls_soc) {
		tdls_err("NULL psoc object");
		return;
	}

	if (tdls_soc->is_drv_supported)
		return;

	if ((old_status < TDLS_LINK_CONNECTING) &&
	    (new_status == TDLS_LINK_CONNECTING))
		tdls_prevent_suspend(tdls_soc);

	if ((old_status > TDLS_LINK_IDLE) &&
	    (new_status == TDLS_LINK_IDLE) &&
	    (!tdls_soc->connected_peer_count) &&
	    (!tdls_is_progress(tdls_vdev, NULL, 0)))
		tdls_allow_suspend(tdls_soc);
}
#else
static void tdls_update_pmo_status(struct tdls_vdev_priv_obj *tdls_vdev,
				   enum tdls_link_state old_status,
				   enum tdls_link_state new_status)
{
}
#endif

void tdls_set_link_status(struct tdls_vdev_priv_obj *vdev_obj,
			  const uint8_t *mac,
			  enum tdls_link_state link_status,
			  enum tdls_link_state_reason link_reason)
{
	uint32_t state = 0;
	int32_t res = 0;
	uint32_t op_class = 0;
	uint32_t channel = 0;
	struct tdls_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	enum tdls_link_state old_status;

	peer = tdls_find_peer(vdev_obj, mac);
	if (!peer) {
		tdls_err("peer is NULL, can't set link status %d, reason %d",
			 link_status, link_reason);
		return;
	}

	old_status = peer->link_status;
	peer->link_status = link_status;
	tdls_update_pmo_status(vdev_obj, old_status, link_status);

	if (link_status >= TDLS_LINK_DISCOVERED)
		peer->discovery_attempt = 0;

	if (peer->is_forced_peer && peer->state_change_notification) {
		peer->reason = link_reason;

		soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
		if (!soc_obj) {
			tdls_err("NULL psoc object");
			return;
		}

		tdls_determine_channel_opclass(soc_obj, vdev_obj,
					       peer, &channel, &op_class);
		tdls_get_wifi_hal_state(peer, &state, &res);
		peer->state_change_notification(mac, op_class, channel,
						state, res, soc_obj->soc);
	}
}

static inline char *
tdls_link_status_str(enum tdls_link_state link_status)
{
	switch (link_status) {
	CASE_RETURN_STRING(TDLS_LINK_IDLE);
	CASE_RETURN_STRING(TDLS_LINK_DISCOVERING);
	CASE_RETURN_STRING(TDLS_LINK_DISCOVERED);
	CASE_RETURN_STRING(TDLS_LINK_CONNECTING);
	CASE_RETURN_STRING(TDLS_LINK_CONNECTED);
	CASE_RETURN_STRING(TDLS_LINK_TEARING);
	default:
		return "UNKNOWN";
	}
}

void tdls_set_peer_link_status(struct tdls_peer *peer,
			       enum tdls_link_state link_status,
			       enum tdls_link_state_reason link_reason)
{
	uint32_t state = 0;
	int32_t res = 0;
	uint32_t op_class = 0;
	uint32_t channel = 0;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	enum tdls_link_state old_status;

	vdev_obj = peer->vdev_priv;

	old_status = peer->link_status;
	peer->link_status = link_status;
	tdls_debug("vdev:%d new state: %s old state:%s reason %d peer:" QDF_MAC_ADDR_FMT,
		   wlan_vdev_get_id(vdev_obj->vdev),
		   tdls_link_status_str(link_status),
		   tdls_link_status_str(old_status), link_reason,
		   QDF_MAC_ADDR_REF(peer->peer_mac.bytes));
	tdls_update_pmo_status(vdev_obj, old_status, link_status);

	if (link_status >= TDLS_LINK_DISCOVERED)
		peer->discovery_attempt = 0;

	if (peer->is_forced_peer && peer->state_change_notification) {
		peer->reason = link_reason;

		soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
		if (!soc_obj) {
			tdls_err("NULL psoc object");
			return;
		}

		tdls_determine_channel_opclass(soc_obj, vdev_obj,
					       peer, &channel, &op_class);
		tdls_get_wifi_hal_state(peer, &state, &res);
		peer->state_change_notification(peer->peer_mac.bytes,
						op_class, channel, state,
						res, soc_obj->soc);
	}
}

static void
tdls_fill_peer_pref_offchan_bw(struct tdls_peer *peer,
			       uint16_t bw)
{
	if (bw < BW_160_MHZ)
		peer->pref_off_chan_width &= ~(1 << BW_160_OFFSET_BIT);

	if (bw < BW_80_MHZ)
		peer->pref_off_chan_width &= ~(1 << BW_80_OFFSET_BIT);

	if (bw < BW_40_MHZ)
		peer->pref_off_chan_width &= ~(1 << BW_40_OFFSET_BIT);
}

static void tdls_update_off_chan_peer_caps(struct tdls_vdev_priv_obj *vdev_obj,
					   struct tdls_soc_priv_obj *soc_obj,
					   struct tdls_peer *peer)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev_obj->vdev);
	qdf_freq_t ini_pref_6g_freq, ini_pref_non6g_freq, peer_freq;
	enum channel_enum peer_chan;
	qdf_freq_t peer_5g_freq = 0, peer_6g_freq = 0;
	bool is_6g_support = false;
	bool peer_6g_supportd = false;
	bool peer_5g_supportd = false;
	uint8_t i;
	uint16_t temp_bw, max_pref_width, peer_supportd_max_bw = 0;
	uint8_t reg_bw_offset;

	if (!pdev) {
		tdls_err("pdev is NULL");
		return;
	}

	/*
	 * Update Pref Offcahnnel BW such that:
	 * 1. If 6 GHz is supported then select the ini preferred 6 GHz channel
	 *    frequency.
	 * 2. If 6 GHz is supported and peer doesn't support the ini preferred
	 *    channel frequency then select the very first 6 GHz channel which
	 *    peer supports as prefferd offchannel.
	 * 3. If peer doesn't support 6 GHz, then select ini preferred 5 GHz
	 *    off channel frequency, given that peer should also support it
	 * 4. If peer doesn support 6 GHz and also doesn't support ini preferred
	 *    5 GHz offcahnnel, then select the very first 5 GHz channel it
	 *    supports.
	 */
	ini_pref_6g_freq = soc_obj->tdls_configs.tdls_pre_off_chan_freq_6g;
	ini_pref_non6g_freq = wlan_reg_legacy_chan_to_freq(pdev,
				soc_obj->tdls_configs.tdls_pre_off_chan_num);

	if (ini_pref_6g_freq == peer->pref_off_chan_freq)
		is_6g_support = true;

	for (i = 0; i < peer->supported_channels_len; i++) {
		peer_freq = peer->supported_chan_freq[i];
		peer_chan = wlan_reg_get_chan_enum_for_freq(peer_freq);

		if (!wlan_reg_is_freq_idx_enabled(pdev, peer_chan,
						  REG_CLI_DEF_VLP))
			continue;

		if (peer->pref_off_chan_freq == peer_freq)
			break;

		if (ini_pref_non6g_freq == peer_freq) {
			peer_5g_supportd = true;
			peer_5g_freq = ini_pref_non6g_freq;
		}
		if (!peer_5g_supportd &&
		    wlan_reg_is_5ghz_ch_freq(peer_freq)) {
			peer_5g_freq = peer_freq;
			peer_5g_supportd = true;
		}
		if (!peer_6g_supportd &&
		    wlan_reg_is_6ghz_chan_freq(peer_freq)) {
			peer_6g_freq = peer_freq;
			peer_6g_supportd = true;
		}
	}

	if (peer->pref_off_chan_freq == peer->supported_chan_freq[i])
		goto bw_check;

	if (is_6g_support && peer_6g_freq)
		peer->pref_off_chan_freq = peer_6g_freq;
	else if (peer_5g_freq)
		peer->pref_off_chan_freq = peer_5g_freq;
	else
		peer->pref_off_chan_freq = 0;

bw_check:
	max_pref_width = wlan_reg_get_max_chwidth(pdev,
						  peer->pref_off_chan_freq);
	for (i = 0; i < peer->supported_oper_classes_len; i++) {
		temp_bw = wlan_reg_get_op_class_width(pdev,
						peer->supported_oper_classes[i],
						false);
		if (temp_bw > peer_supportd_max_bw)
			peer_supportd_max_bw = temp_bw;
	}

	peer_supportd_max_bw = (peer_supportd_max_bw > max_pref_width) ?
				max_pref_width : peer_supportd_max_bw;
	if (wlan_reg_is_6ghz_chan_freq(peer->pref_off_chan_freq) &&
	    peer_supportd_max_bw < BW_160_MHZ)
		tdls_fill_peer_pref_offchan_bw(peer, peer_supportd_max_bw);
	else if (wlan_reg_is_5ghz_ch_freq(peer->pref_off_chan_freq) &&
		 peer_supportd_max_bw < BW_80_MHZ)
		tdls_fill_peer_pref_offchan_bw(peer, peer_supportd_max_bw);

	if (wlan_reg_is_5ghz_ch_freq(peer->pref_off_chan_freq) &&
	    CHECK_BIT(peer->pref_off_chan_width, BW_160_OFFSET_BIT))
		peer->pref_off_chan_width &= ~(1 << BW_160_OFFSET_BIT);

	peer->op_class_for_pref_off_chan =
		tdls_get_opclass_from_bandwidth(
				vdev_obj->vdev, peer->pref_off_chan_freq,
				peer->pref_off_chan_width,
				&reg_bw_offset);

	tdls_debug("Updated preff offchannel freq %d width %d opclass %d",
		   peer->pref_off_chan_freq, peer->pref_off_chan_width,
		   peer->op_class_for_pref_off_chan);
}

void tdls_set_peer_caps(struct tdls_vdev_priv_obj *vdev_obj,
			const uint8_t *macaddr,
			struct tdls_update_peer_params  *req_info)
{
	uint8_t is_buffer_sta = 0;
	uint8_t is_off_channel_supported = 0;
	uint8_t is_qos_wmm_sta = 0;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_peer *curr_peer;
	uint32_t feature;

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
	if (!soc_obj) {
		tdls_err("NULL psoc object");
		return;
	}

	curr_peer = tdls_find_peer(vdev_obj, macaddr);
	if (!curr_peer) {
		tdls_err("NULL tdls peer");
		return;
	}

	feature = soc_obj->tdls_configs.tdls_feature_flags;
	if ((1 << 4) & req_info->extn_capability[3])
		is_buffer_sta = 1;

	if ((1 << 6) & req_info->extn_capability[3])
		is_off_channel_supported = 1;

	if (TDLS_IS_WMM_ENABLED(feature) && req_info->is_qos_wmm_sta)
		is_qos_wmm_sta = 1;

	curr_peer->uapsd_queues = req_info->uapsd_queues;
	curr_peer->max_sp = req_info->max_sp;
	curr_peer->buf_sta_capable = is_buffer_sta;
	curr_peer->off_channel_capable = is_off_channel_supported;

	qdf_mem_copy(curr_peer->supported_chan_freq,
		     req_info->supported_chan_freq,
		     sizeof(qdf_freq_t) * req_info->supported_channels_len);

	curr_peer->supported_channels_len = req_info->supported_channels_len;

	qdf_mem_copy(curr_peer->supported_oper_classes,
		     req_info->supported_oper_classes,
		     req_info->supported_oper_classes_len);

	curr_peer->supported_oper_classes_len =
		req_info->supported_oper_classes_len;

	curr_peer->qos = is_qos_wmm_sta;

	tdls_update_off_chan_peer_caps(vdev_obj, soc_obj, curr_peer);
}

QDF_STATUS tdls_set_valid(struct tdls_vdev_priv_obj *vdev_obj,
			   const uint8_t *macaddr)
{
	struct tdls_peer *peer;

	peer = tdls_find_peer(vdev_obj, macaddr);
	if (!peer) {
		tdls_err("peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	peer->valid_entry = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_set_force_peer(struct tdls_vdev_priv_obj *vdev_obj,
			       const uint8_t *macaddr, bool forcepeer)
{
	struct tdls_peer *peer;

	peer = tdls_find_peer(vdev_obj, macaddr);
	if (!peer) {
		tdls_err("peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	peer->is_forced_peer = forcepeer;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_set_callback(struct tdls_peer *peer,
			     tdls_state_change_callback callback)
{
	if (!peer) {
		tdls_err("peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	peer->state_change_notification = callback;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_set_extctrl_param(struct tdls_peer *peer, qdf_freq_t ch_freq,
				  uint32_t max_latency, uint32_t op_class,
				  uint32_t min_bandwidth)
{
	if (!peer) {
		tdls_err("peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	peer->op_class_for_pref_off_chan = (uint8_t)op_class;
	peer->pref_off_chan_freq = ch_freq;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_reset_peer(struct tdls_vdev_priv_obj *vdev_obj,
			   const uint8_t *macaddr)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_peer *curr_peer;
	struct tdls_user_config *config;
	uint8_t reg_bw_offset;

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
	if (!soc_obj) {
		tdls_err("NULL psoc object");
		return QDF_STATUS_E_FAILURE;
	}

	curr_peer = tdls_find_peer(vdev_obj, macaddr);
	if (!curr_peer) {
		tdls_err("NULL tdls peer");
		return QDF_STATUS_E_FAILURE;
	}

	if (!curr_peer->is_forced_peer) {
		config = &soc_obj->tdls_configs;
		tdls_fill_pref_off_chan_info(vdev_obj, soc_obj, curr_peer);
		curr_peer->op_class_for_pref_off_chan =
			tdls_get_opclass_from_bandwidth(
				vdev_obj->vdev, curr_peer->pref_off_chan_freq,
				curr_peer->pref_off_chan_width,
				&reg_bw_offset);
	}

	if (curr_peer->is_peer_idle_timer_initialised) {
		tdls_debug(QDF_MAC_ADDR_FMT ": destroy  idle timer ",
			   QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));
		qdf_mc_timer_stop(&curr_peer->peer_idle_timer);
		qdf_mc_timer_destroy(&curr_peer->peer_idle_timer);
		curr_peer->is_peer_idle_timer_initialised = false;
	}

	tdls_set_peer_link_status(curr_peer, TDLS_LINK_IDLE,
				  TDLS_LINK_UNSPECIFIED);
	curr_peer->valid_entry = false;

	return QDF_STATUS_SUCCESS;
}

void tdls_peer_idle_timers_destroy(struct tdls_vdev_priv_obj *vdev_obj)
{
	uint16_t i;
	struct tdls_peer *peer;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	if (!vdev_obj) {
		tdls_err("NULL tdls vdev object");
		return;
	}

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &vdev_obj->peer_list[i];

		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			peer = qdf_container_of(p_node, struct tdls_peer, node);
			if (peer && peer->is_peer_idle_timer_initialised) {
				tdls_debug(QDF_MAC_ADDR_FMT
					   ": destroy  idle timer ",
					   QDF_MAC_ADDR_REF(
						   peer->peer_mac.bytes));
				qdf_mc_timer_stop(&peer->peer_idle_timer);
				qdf_mc_timer_destroy(&peer->peer_idle_timer);
			}
			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}
}

void tdls_free_peer_list(struct tdls_vdev_priv_obj *vdev_obj)
{
	uint16_t i;
	struct tdls_peer *peer;
	qdf_list_t *head;
	qdf_list_node_t *p_node;

	if (!vdev_obj) {
		tdls_err("NULL tdls vdev object");
		return;
	}

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &vdev_obj->peer_list[i];

		while (QDF_IS_STATUS_SUCCESS(
			       qdf_list_remove_front(head, &p_node))) {
			peer = qdf_container_of(p_node, struct tdls_peer, node);
			qdf_mem_free(peer);
		}
		qdf_list_destroy(head);
	}
}

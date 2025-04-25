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

#ifndef _DP_TXRX_WDS_H_
#define _DP_TXRX_WDS_H_

#ifdef WIFI_MONITOR_SUPPORT
#include "dp_htt.h"
#include "dp_mon.h"
#endif

/* host managed flag */
#define DP_AST_FLAGS_HM 0x0020

/* WDS AST entry aging timer value */
#define DP_WDS_AST_AGING_TIMER_DEFAULT_MS	120000
#define DP_WDS_AST_AGING_TIMER_CNT \
((DP_WDS_AST_AGING_TIMER_DEFAULT_MS / DP_AST_AGING_TIMER_DEFAULT_MS) - 1)

/**
 * dp_soc_wds_attach() - Setup WDS timer and AST table
 * @soc: Datapath SOC handle
 *
 * Return: None
 */
void dp_soc_wds_attach(struct dp_soc *soc);

/**
 * dp_soc_wds_detach() - Detach WDS data structures and timers
 * @soc: DP SOC handle
 *
 * Return: None
 */
void dp_soc_wds_detach(struct dp_soc *soc);
#ifdef QCA_PEER_MULTIQ_SUPPORT
/**
 * dp_peer_find_ast_index_by_flowq_id() - API to get ast idx for a given flowid
 * @soc: soc handle
 * @vdev_id: vdev ID
 * @peer_mac_addr: mac address of the peer
 * @flow_id: flow id to find ast index
 * @tid: TID
 *
 * Return: ast index for a given flow id, -1 for fail cases
 */
int dp_peer_find_ast_index_by_flowq_id(struct cdp_soc_t *soc,
	       uint16_t vdev_id, uint8_t *peer_mac_addr,
	       uint8_t flow_id, uint8_t tid);
#endif

/**
 * dp_rx_da_learn() - Add AST entry based on DA lookup
 *			This is a WAR for HK 1.0 and will
 *			be removed in HK 2.0
 *
 * @soc: core txrx main context
 * @rx_tlv_hdr: start address of rx tlvs
 * @ta_peer: Transmitter peer entry
 * @nbuf: nbuf to retrieve destination mac for which AST will be added
 *
 */
void
dp_rx_da_learn(struct dp_soc *soc,
	       uint8_t *rx_tlv_hdr,
	       struct dp_txrx_peer *ta_peer,
	       qdf_nbuf_t nbuf);

/**
 * dp_tx_mec_handler() - Tx  MEC Notify Handler
 * @vdev: pointer to dp dev handler
 * @status : Tx completion status from HTT descriptor
 *
 * Handles MEC notify event sent from fw to Host
 *
 * Return: none
 */
void dp_tx_mec_handler(struct dp_vdev *vdev, uint8_t *status);
#ifdef FEATURE_WDS
#ifdef FEATURE_MCL_REPEATER
static inline bool dp_tx_da_search_override(struct dp_vdev *vdev)
{
	if (vdev->mec_enabled)
		return true;

	return false;
}
#else
static inline bool dp_tx_da_search_override(struct dp_vdev *vdev)
{
	struct dp_soc *soc = vdev->pdev->soc;

	/*
	 * If AST index override support is available (HKv2 etc),
	 * DA search flag be enabled always
	 *
	 * If AST index override support is not available (HKv1),
	 * DA search flag should be used for all modes except QWRAP
	 */
	if (soc->ast_override_support || !vdev->proxysta_vdev)
		return true;

	return false;
}
#endif /* FEATURE_MCL_REPEATER */
#endif /* FEATURE_WDS */
#ifdef WDS_VENDOR_EXTENSION

/**
 * dp_txrx_peer_wds_tx_policy_update() - API to set tx wds policy
 *
 * @soc: DP soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @wds_tx_ucast: policy for unicast transmission
 * @wds_tx_mcast: policy for multicast transmission
 *
 * Return: void
 */
QDF_STATUS
dp_txrx_peer_wds_tx_policy_update(struct cdp_soc_t *soc,  uint8_t vdev_id,
				  uint8_t *peer_mac, int wds_tx_ucast,
				  int wds_tx_mcast);

/**
 * dp_txrx_set_wds_rx_policy() - API to store datapath
 *                            config parameters
 * @soc_hdl: datapath soc handle
 * @vdev_id: id of datapath vdev handle
 * @val: WDS rx policy value
 *
 * Return: status
 */
QDF_STATUS
dp_txrx_set_wds_rx_policy(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			  u_int32_t val);
#endif

/**
 * dp_hmwds_ast_add_notify() - schedules hmwds ast add status work
 * @peer: DP peer
 * @mac_addr: ast mac addr
 * @type: ast type
 * @err: QDF_STATUS error code
 * @is_peer_map: notify is from peer map
 *
 * Return: void
 */
void dp_hmwds_ast_add_notify(struct dp_peer *peer,
			     uint8_t *mac_addr,
			     enum cdp_txrx_ast_entry_type type,
			     QDF_STATUS err,
			     bool is_peer_map);

#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * dp_wds_ext_peer_learn() - function to send event to control
 * path on receiving 1st 4-address frame from backhaul.
 * @soc: DP soc
 * @ta_peer: WDS repeater peer
 *
 * Return: void
 */
static inline void dp_wds_ext_peer_learn(struct dp_soc *soc,
					 struct dp_peer *ta_peer)
{
	uint8_t wds_ext_src_mac[QDF_MAC_ADDR_SIZE];

	if (ta_peer->vdev->wds_ext_enabled &&
	    !qdf_atomic_test_and_set_bit(WDS_EXT_PEER_INIT_BIT,
					 &ta_peer->txrx_peer->wds_ext.init)) {
		qdf_mem_copy(wds_ext_src_mac, &ta_peer->mac_addr.raw[0],
			     QDF_MAC_ADDR_SIZE);
		soc->cdp_soc.ol_ops->rx_wds_ext_peer_learn(
						soc->ctrl_psoc,
						ta_peer->peer_id,
						ta_peer->vdev->vdev_id,
						wds_ext_src_mac);
	}
}
#else
static inline void dp_wds_ext_peer_learn(struct dp_soc *soc,
					 struct dp_peer *ta_peer)
{
}
#endif

/**
 * dp_rx_wds_add_or_update_ast() - Add or update the ast entry.
 *
 * @soc: core txrx main context
 * @ta_peer: WDS repeater txrx peer
 * @nbuf: network buffer
 * @is_ad4_valid: 4-address valid flag
 * @is_sa_valid: source address valid flag
 * @is_chfrag_start: frag start flag
 * @sa_idx: source-address index for peer
 * @sa_sw_peer_id: software source-address peer-id
 *
 * Return: void:
 */
static inline void
dp_rx_wds_add_or_update_ast(struct dp_soc *soc,
			    struct dp_txrx_peer *ta_peer,
			    qdf_nbuf_t nbuf, uint8_t is_ad4_valid,
			    uint8_t is_sa_valid, uint8_t is_chfrag_start,
			    uint16_t sa_idx, uint16_t sa_sw_peer_id)
{
	struct dp_peer *sa_peer;
	struct dp_ast_entry *ast;
	uint32_t flags = DP_AST_FLAGS_HM;
	struct dp_pdev *pdev = ta_peer->vdev->pdev;
	uint8_t wds_src_mac[QDF_MAC_ADDR_SIZE];
	struct dp_peer *ta_base_peer;


	if (!(is_chfrag_start && is_ad4_valid))
		return;

	if (qdf_unlikely(!is_sa_valid)) {
		qdf_mem_copy(wds_src_mac,
			     (qdf_nbuf_data(nbuf) + QDF_MAC_ADDR_SIZE),
			     QDF_MAC_ADDR_SIZE);

		ta_base_peer = dp_peer_get_ref_by_id(soc, ta_peer->peer_id,
						     DP_MOD_ID_RX);
		if (ta_base_peer) {
			if (ta_peer->vdev->opmode == wlan_op_mode_ap)
				dp_wds_ext_peer_learn(soc, ta_base_peer);

			dp_peer_add_ast(soc, ta_base_peer, wds_src_mac,
					CDP_TXRX_AST_TYPE_WDS, flags);

			dp_peer_unref_delete(ta_base_peer, DP_MOD_ID_RX);
		}
		return;
	}

	qdf_spin_lock_bh(&soc->ast_lock);
	ast = soc->ast_table[sa_idx];

	if (!ast) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		/*
		 * In HKv1, it is possible that HW retains the AST entry in
		 * GSE cache on 1 radio , even after the AST entry is deleted
		 * (on another radio).
		 *
		 * Due to this, host might still get sa_is_valid indications
		 * for frames with SA not really present in AST table.
		 *
		 * So we go ahead and send an add_ast command to FW in such
		 * cases where sa is reported still as valid, so that FW will
		 * invalidate this GSE cache entry and new AST entry gets
		 * cached.
		 */
		if (!soc->ast_override_support) {
			qdf_mem_copy(wds_src_mac,
				     (qdf_nbuf_data(nbuf) + QDF_MAC_ADDR_SIZE),
				     QDF_MAC_ADDR_SIZE);

			ta_base_peer = dp_peer_get_ref_by_id(soc,
							     ta_peer->peer_id,
							     DP_MOD_ID_RX);

			if (ta_base_peer) {
				dp_peer_add_ast(soc, ta_base_peer,
						wds_src_mac,
						CDP_TXRX_AST_TYPE_WDS,
						flags);

				dp_peer_unref_delete(ta_base_peer,
						     DP_MOD_ID_RX);
			}
			return;
		} else {
			/* In HKv2 smart monitor case, when NAC client is
			 * added first and this client roams within BSS to
			 * connect to RE, since we have an AST entry for
			 * NAC we get sa_is_valid bit set. So we check if
			 * smart monitor is enabled and send add_ast command
			 * to FW.
			 */
			ta_base_peer = dp_peer_get_ref_by_id(soc,
							     ta_peer->peer_id,
							     DP_MOD_ID_RX);
			if (ta_base_peer) {
				dp_monitor_neighbour_peer_add_ast(pdev,
								  ta_base_peer,
								  wds_src_mac,
								  nbuf,
								  flags);
				dp_peer_unref_delete(ta_base_peer,
						     DP_MOD_ID_RX);
			}
			return;
		}
	}

	/*
	 * Ensure we are updating the right AST entry by
	 * validating ast_idx.
	 * There is a possibility we might arrive here without
	 * AST MAP event , so this check is mandatory
	 */
	if (ast->is_mapped && (ast->ast_idx == sa_idx))
		ast->is_active = TRUE;

	if (ast->peer_id != ta_peer->peer_id) {
		if ((ast->type == CDP_TXRX_AST_TYPE_WDS_HM) ||
		    (ast->type == CDP_TXRX_AST_TYPE_WDS_HM_SEC)) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			return;
		}

		if ((ast->type != CDP_TXRX_AST_TYPE_STATIC) &&
		    (ast->type != CDP_TXRX_AST_TYPE_SELF) &&
		    (ast->type != CDP_TXRX_AST_TYPE_STA_BSS)) {
			if (ast->pdev_id != ta_peer->vdev->pdev->pdev_id) {
				/* This case is when a STA roams from one
				 * repeater to another repeater, but these
				 * repeaters are connected to root AP on
				 * different radios.
				 * Ex: rptr1 connected to ROOT AP over 5G
				 * and rptr2 connected to ROOT AP over 2G
				 * radio
				 */
				dp_peer_del_ast(soc, ast);
				qdf_spin_unlock_bh(&soc->ast_lock);
				return;
			} else {
				/* this case is when a STA roams from one
				 * reapter to another repeater, but inside
				 * same radio.
				 */
				/* For HKv2 do not update the AST entry if
				 * new ta_peer is on STA vap as SRC port
				 * learning is disable on STA vap
				 */
				if (soc->ast_override_support &&
				    (ta_peer->vdev->opmode == wlan_op_mode_sta)) {
					dp_peer_del_ast(soc, ast);
					qdf_spin_unlock_bh(&soc->ast_lock);
					return;
				} else {
					ta_base_peer =
					dp_peer_get_ref_by_id(soc,
							      ta_peer->peer_id,
							      DP_MOD_ID_RX);
					if (ta_base_peer) {
						dp_wds_ext_peer_learn(soc,
								ta_base_peer);
						dp_peer_update_ast(soc,
								   ta_base_peer,
								   ast, flags);

						dp_peer_unref_delete(ta_base_peer,
								DP_MOD_ID_RX);
					}
				}
				qdf_spin_unlock_bh(&soc->ast_lock);
				return;
			}
		}
		/*
		 * Do not kickout STA if it belongs to a different radio.
		 * For DBDC repeater, it is possible to arrive here
		 * for multicast loopback frames originated from connected
		 * clients and looped back (intrabss) by Root AP
		 */
		if (ast->pdev_id != ta_peer->vdev->pdev->pdev_id) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			return;
		}

		qdf_spin_unlock_bh(&soc->ast_lock);
		/*
		 * Kickout, when direct associated peer(SA) roams
		 * to another AP and reachable via TA peer
		 */
		sa_peer = dp_peer_get_ref_by_id(soc, ast->peer_id,
						DP_MOD_ID_RX);
		if (!sa_peer)
			return;

		if ((sa_peer->vdev->opmode == wlan_op_mode_ap) &&
		    !sa_peer->delete_in_progress) {
			qdf_mem_copy(wds_src_mac,
				     (qdf_nbuf_data(nbuf) + QDF_MAC_ADDR_SIZE),
				     QDF_MAC_ADDR_SIZE);
			sa_peer->delete_in_progress = true;
			if (soc->cdp_soc.ol_ops->peer_sta_kickout) {
				soc->cdp_soc.ol_ops->peer_sta_kickout(
					soc->ctrl_psoc,
					sa_peer->vdev->pdev->pdev_id,
					wds_src_mac);
			}
		}
		dp_peer_unref_delete(sa_peer, DP_MOD_ID_RX);
		return;
	}

	qdf_spin_unlock_bh(&soc->ast_lock);
}

/**
 * dp_rx_wds_srcport_learn() - Add or update the STA PEER which
 *				is behind the WDS repeater.
 * @soc: core txrx main context
 * @rx_tlv_hdr: base address of RX TLV header
 * @ta_peer: WDS repeater peer
 * @nbuf: rx pkt
 * @msdu_end_info:
 *
 * Return: void:
 */
static inline void
dp_rx_wds_srcport_learn(struct dp_soc *soc,
			uint8_t *rx_tlv_hdr,
			struct dp_txrx_peer *ta_peer,
			qdf_nbuf_t nbuf,
			struct hal_rx_msdu_metadata msdu_end_info)
{
	uint8_t sa_is_valid = qdf_nbuf_is_sa_valid(nbuf);
	uint8_t is_chfrag_start = 0;
	uint8_t is_ad4_valid = 0;

	if (qdf_unlikely(!ta_peer))
		return;

	is_chfrag_start = qdf_nbuf_is_rx_chfrag_start(nbuf);
	if (is_chfrag_start)
		is_ad4_valid = hal_rx_get_mpdu_mac_ad4_valid(soc->hal_soc, rx_tlv_hdr);


	/*
	 * Get the AST entry from HW SA index and mark it as active
	 */
	dp_rx_wds_add_or_update_ast(soc, ta_peer, nbuf, is_ad4_valid,
				    sa_is_valid, is_chfrag_start,
				    msdu_end_info.sa_idx, msdu_end_info.sa_sw_peer_id);
}

#ifdef IPA_WDS_EASYMESH_FEATURE
/**
 * dp_rx_ipa_wds_srcport_learn() - Add or update the STA PEER which
 *				is behind the WDS repeater.
 *
 * @soc: core txrx main context
 * @ta_peer: WDS repeater peer
 * @nbuf: rx pkt
 * @msdu_end_info: msdu end info
 * @ad4_valid: address4 valid bit
 * @chfrag_start: Msdu start bit
 *
 * Return: void
 */
static inline void
dp_rx_ipa_wds_srcport_learn(struct dp_soc *soc,
			    struct dp_peer *ta_peer, qdf_nbuf_t nbuf,
			    struct hal_rx_msdu_metadata msdu_end_info,
			    bool ad4_valid, bool chfrag_start)
{
	uint8_t sa_is_valid = qdf_nbuf_is_sa_valid(nbuf);
	uint8_t is_chfrag_start = (uint8_t)chfrag_start;
	uint8_t is_ad4_valid = (uint8_t)ad4_valid;
	struct dp_txrx_peer *peer = (struct dp_txrx_peer *)ta_peer;

	if (qdf_unlikely(!ta_peer))
		return;

	/*
	 * Get the AST entry from HW SA index and mark it as active
	 */
	dp_rx_wds_add_or_update_ast(soc, peer, nbuf, is_ad4_valid,
				    sa_is_valid, is_chfrag_start,
				    msdu_end_info.sa_idx,
				    msdu_end_info.sa_sw_peer_id);
}
#endif

/**
 * dp_rx_ast_set_active() - set the active flag of the astentry
 *				    corresponding to a hw index.
 * @soc: core txrx main context
 * @sa_idx: hw idx
 * @is_active: active flag
 *
 */
static inline QDF_STATUS dp_rx_ast_set_active(struct dp_soc *soc,
					      uint16_t sa_idx, bool is_active)
{
	struct dp_ast_entry *ast;

	qdf_spin_lock_bh(&soc->ast_lock);
	ast = soc->ast_table[sa_idx];

	if (!ast) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!ast->is_mapped) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * Ensure we are updating the right AST entry by
	 * validating ast_idx.
	 * There is a possibility we might arrive here without
	 * AST MAP event , so this check is mandatory
	 */
	if (ast->ast_idx == sa_idx) {
		ast->is_active = is_active;
		qdf_spin_unlock_bh(&soc->ast_lock);
		return QDF_STATUS_SUCCESS;
	}

	qdf_spin_unlock_bh(&soc->ast_lock);
	return QDF_STATUS_E_FAILURE;
}
#endif /* DP_TXRX_WDS*/

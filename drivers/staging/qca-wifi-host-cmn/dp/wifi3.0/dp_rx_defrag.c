/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "hal_hw_headers.h"
#ifndef RX_DEFRAG_DO_NOT_REINJECT
#ifndef DP_BE_WAR
#include "li/hal_li_rx.h"
#endif
#endif
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_peer.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "qdf_nbuf.h"
#include "dp_internal.h"
#include "dp_rx_defrag.h"
#include <enet.h>	/* LLC_SNAP_HDR_LEN */
#include "dp_rx_defrag.h"
#include "dp_ipa.h"
#include "dp_rx_buffer_pool.h"

const struct dp_rx_defrag_cipher dp_f_ccmp = {
	"AES-CCM",
	IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
	IEEE80211_WEP_MICLEN,
	0,
};

const struct dp_rx_defrag_cipher dp_f_tkip = {
	"TKIP",
	IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
	IEEE80211_WEP_CRCLEN,
	IEEE80211_WEP_MICLEN,
};

const struct dp_rx_defrag_cipher dp_f_wep = {
	"WEP",
	IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN,
	IEEE80211_WEP_CRCLEN,
	0,
};

/*
 * The header and mic length are same for both
 * GCMP-128 and GCMP-256.
 */
const struct dp_rx_defrag_cipher dp_f_gcmp = {
	"AES-GCMP",
	WLAN_IEEE80211_GCMP_HEADERLEN,
	WLAN_IEEE80211_GCMP_MICLEN,
	WLAN_IEEE80211_GCMP_MICLEN,
};

/*
 * dp_rx_defrag_frames_free(): Free fragment chain
 * @frames: Fragment chain
 *
 * Iterates through the fragment chain and frees them
 * Returns: None
 */
static void dp_rx_defrag_frames_free(qdf_nbuf_t frames)
{
	qdf_nbuf_t next, frag = frames;

	while (frag) {
		next = qdf_nbuf_next(frag);
		dp_rx_nbuf_free(frag);
		frag = next;
	}
}

/*
 * dp_rx_clear_saved_desc_info(): Clears descriptor info
 * @txrx peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Saves MPDU descriptor info and MSDU link pointer from REO
 * ring descriptor. The cache is created per peer, per TID
 *
 * Returns: None
 */
static void dp_rx_clear_saved_desc_info(struct dp_txrx_peer *txrx_peer,
					unsigned int tid)
{
	if (txrx_peer->rx_tid[tid].dst_ring_desc)
		qdf_mem_free(txrx_peer->rx_tid[tid].dst_ring_desc);

	txrx_peer->rx_tid[tid].dst_ring_desc = NULL;
	txrx_peer->rx_tid[tid].head_frag_desc = NULL;
}

static void dp_rx_return_head_frag_desc(struct dp_txrx_peer *txrx_peer,
					unsigned int tid)
{
	struct dp_soc *soc;
	struct dp_pdev *pdev;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	union dp_rx_desc_list_elem_t *head = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	uint8_t pool_id;

	pdev = txrx_peer->vdev->pdev;
	soc = pdev->soc;

	if (txrx_peer->rx_tid[tid].head_frag_desc) {
		pool_id = txrx_peer->rx_tid[tid].head_frag_desc->pool_id;
		dp_rxdma_srng = &soc->rx_refill_buf_ring[pool_id];
		rx_desc_pool = &soc->rx_desc_buf[pool_id];

		dp_rx_add_to_free_desc_list(&head, &tail,
					    txrx_peer->rx_tid[tid].head_frag_desc);
		dp_rx_buffers_replenish(soc, 0, dp_rxdma_srng, rx_desc_pool,
					1, &head, &tail, false);
	}

	if (txrx_peer->rx_tid[tid].dst_ring_desc) {
		if (dp_rx_link_desc_return(soc,
					   txrx_peer->rx_tid[tid].dst_ring_desc,
					   HAL_BM_ACTION_PUT_IN_IDLE_LIST) !=
		    QDF_STATUS_SUCCESS)
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  "%s: Failed to return link desc", __func__);
	}
}

/*
 * dp_rx_reorder_flush_frag(): Flush the frag list
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Flush the per-TID frag list
 *
 * Returns: None
 */
void dp_rx_reorder_flush_frag(struct dp_txrx_peer *txrx_peer,
			      unsigned int tid)
{
	dp_info_rl("Flushing TID %d", tid);

	if (!txrx_peer) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
					"%s: NULL peer", __func__);
		return;
	}

	dp_rx_return_head_frag_desc(txrx_peer, tid);
	dp_rx_defrag_cleanup(txrx_peer, tid);
}

/*
 * dp_rx_defrag_waitlist_flush(): Flush SOC defrag wait list
 * @soc: DP SOC
 *
 * Flush fragments of all waitlisted TID's
 *
 * Returns: None
 */
void dp_rx_defrag_waitlist_flush(struct dp_soc *soc)
{
	struct dp_rx_tid_defrag *waitlist_elem = NULL;
	struct dp_rx_tid_defrag *tmp;
	uint32_t now_ms = qdf_system_ticks_to_msecs(qdf_system_ticks());
	TAILQ_HEAD(, dp_rx_tid_defrag) temp_list;
	dp_txrx_ref_handle txrx_ref_handle = NULL;

	TAILQ_INIT(&temp_list);

	dp_debug("Current time  %u", now_ms);

	qdf_spin_lock_bh(&soc->rx.defrag.defrag_lock);
	TAILQ_FOREACH_SAFE(waitlist_elem, &soc->rx.defrag.waitlist,
			   defrag_waitlist_elem, tmp) {
		uint32_t tid;

		if (waitlist_elem->defrag_timeout_ms > now_ms)
			break;

		tid = waitlist_elem->tid;
		if (tid >= DP_MAX_TIDS) {
			qdf_assert(0);
			continue;
		}

		TAILQ_REMOVE(&soc->rx.defrag.waitlist, waitlist_elem,
			     defrag_waitlist_elem);
		DP_STATS_DEC(soc, rx.rx_frag_wait, 1);

		/* Move to temp list and clean-up later */
		TAILQ_INSERT_TAIL(&temp_list, waitlist_elem,
				  defrag_waitlist_elem);
	}
	if (waitlist_elem) {
		soc->rx.defrag.next_flush_ms =
			waitlist_elem->defrag_timeout_ms;
	} else {
		soc->rx.defrag.next_flush_ms =
			now_ms + soc->rx.defrag.timeout_ms;
	}

	qdf_spin_unlock_bh(&soc->rx.defrag.defrag_lock);

	TAILQ_FOREACH_SAFE(waitlist_elem, &temp_list,
			   defrag_waitlist_elem, tmp) {
		struct dp_txrx_peer *txrx_peer, *temp_peer = NULL;

		qdf_spin_lock_bh(&waitlist_elem->defrag_tid_lock);
		TAILQ_REMOVE(&temp_list, waitlist_elem,
			     defrag_waitlist_elem);
		/* get address of current peer */
		txrx_peer = waitlist_elem->defrag_peer;
		qdf_spin_unlock_bh(&waitlist_elem->defrag_tid_lock);

		temp_peer = dp_txrx_peer_get_ref_by_id(soc, txrx_peer->peer_id,
						       &txrx_ref_handle,
						       DP_MOD_ID_RX_ERR);
		if (temp_peer == txrx_peer) {
			qdf_spin_lock_bh(&waitlist_elem->defrag_tid_lock);
			dp_rx_reorder_flush_frag(txrx_peer, waitlist_elem->tid);
			qdf_spin_unlock_bh(&waitlist_elem->defrag_tid_lock);
		}

		if (temp_peer)
			dp_txrx_peer_unref_delete(txrx_ref_handle,
						  DP_MOD_ID_RX_ERR);

	}
}

/*
 * dp_rx_defrag_waitlist_add(): Update per-PDEV defrag wait list
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Appends per-tid fragments to global fragment wait list
 *
 * Returns: None
 */
static void dp_rx_defrag_waitlist_add(struct dp_txrx_peer *txrx_peer,
				      unsigned int tid)
{
	struct dp_soc *psoc = txrx_peer->vdev->pdev->soc;
	struct dp_rx_tid_defrag *waitlist_elem = &txrx_peer->rx_tid[tid];

	dp_debug("Adding TID %u to waitlist for peer %pK with peer_id = %d ",
		 tid, txrx_peer, txrx_peer->peer_id);

	/* TODO: use LIST macros instead of TAIL macros */
	qdf_spin_lock_bh(&psoc->rx.defrag.defrag_lock);
	if (TAILQ_EMPTY(&psoc->rx.defrag.waitlist))
		psoc->rx.defrag.next_flush_ms =
			waitlist_elem->defrag_timeout_ms;

	TAILQ_INSERT_TAIL(&psoc->rx.defrag.waitlist, waitlist_elem,
			  defrag_waitlist_elem);
	DP_STATS_INC(psoc, rx.rx_frag_wait, 1);
	qdf_spin_unlock_bh(&psoc->rx.defrag.defrag_lock);
}

/*
 * dp_rx_defrag_waitlist_remove(): Remove fragments from waitlist
 * @txrx peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Remove fragments from waitlist
 *
 * Returns: None
 */
void dp_rx_defrag_waitlist_remove(struct dp_txrx_peer *txrx_peer,
				  unsigned int tid)
{
	struct dp_pdev *pdev = txrx_peer->vdev->pdev;
	struct dp_soc *soc = pdev->soc;
	struct dp_rx_tid_defrag *waitlist_elm;
	struct dp_rx_tid_defrag *tmp;

	dp_debug("Removing TID %u to waitlist for peer %pK peer_id = %d ",
		 tid, txrx_peer, txrx_peer->peer_id);

	if (tid >= DP_MAX_TIDS) {
		dp_err("TID out of bounds: %d", tid);
		qdf_assert_always(0);
	}

	qdf_spin_lock_bh(&soc->rx.defrag.defrag_lock);
	TAILQ_FOREACH_SAFE(waitlist_elm, &soc->rx.defrag.waitlist,
			   defrag_waitlist_elem, tmp) {
		struct dp_txrx_peer *peer_on_waitlist;

		/* get address of current peer */
		peer_on_waitlist = waitlist_elm->defrag_peer;

		/* Ensure it is TID for same peer */
		if (peer_on_waitlist == txrx_peer && waitlist_elm->tid == tid) {
			TAILQ_REMOVE(&soc->rx.defrag.waitlist,
				     waitlist_elm, defrag_waitlist_elem);
			DP_STATS_DEC(soc, rx.rx_frag_wait, 1);
		}
	}
	qdf_spin_unlock_bh(&soc->rx.defrag.defrag_lock);
}

/*
 * dp_rx_defrag_fraglist_insert(): Create a per-sequence fragment list
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 * @head_addr: Pointer to head list
 * @tail_addr: Pointer to tail list
 * @frag: Incoming fragment
 * @all_frag_present: Flag to indicate whether all fragments are received
 *
 * Build a per-tid, per-sequence fragment list.
 *
 * Returns: Success, if inserted
 */
static QDF_STATUS
dp_rx_defrag_fraglist_insert(struct dp_txrx_peer *txrx_peer, unsigned int tid,
			     qdf_nbuf_t *head_addr, qdf_nbuf_t *tail_addr,
			     qdf_nbuf_t frag, uint8_t *all_frag_present)
{
	struct dp_soc *soc = txrx_peer->vdev->pdev->soc;
	qdf_nbuf_t next;
	qdf_nbuf_t prev = NULL;
	qdf_nbuf_t cur;
	uint16_t head_fragno, cur_fragno, next_fragno;
	uint8_t last_morefrag = 1, count = 0;
	struct dp_rx_tid_defrag *rx_tid = &txrx_peer->rx_tid[tid];
	uint8_t *rx_desc_info;

	qdf_assert(frag);
	qdf_assert(head_addr);
	qdf_assert(tail_addr);

	*all_frag_present = 0;
	rx_desc_info = qdf_nbuf_data(frag);
	cur_fragno = dp_rx_frag_get_mpdu_frag_number(soc, rx_desc_info);

	dp_debug("cur_fragno %d\n", cur_fragno);
	/* If this is the first fragment */
	if (!(*head_addr)) {
		*head_addr = *tail_addr = frag;
		qdf_nbuf_set_next(*tail_addr, NULL);
		rx_tid->curr_frag_num = cur_fragno;

		goto insert_done;
	}

	/* In sequence fragment */
	if (cur_fragno > rx_tid->curr_frag_num) {
		qdf_nbuf_set_next(*tail_addr, frag);
		*tail_addr = frag;
		qdf_nbuf_set_next(*tail_addr, NULL);
		rx_tid->curr_frag_num = cur_fragno;
	} else {
		/* Out of sequence fragment */
		cur = *head_addr;
		rx_desc_info = qdf_nbuf_data(cur);
		head_fragno = dp_rx_frag_get_mpdu_frag_number(soc,
							      rx_desc_info);

		if (cur_fragno == head_fragno) {
			dp_rx_nbuf_free(frag);
			goto insert_fail;
		} else if (head_fragno > cur_fragno) {
			qdf_nbuf_set_next(frag, cur);
			cur = frag;
			*head_addr = frag; /* head pointer to be updated */
		} else {
			while ((cur_fragno > head_fragno) && cur) {
				prev = cur;
				cur = qdf_nbuf_next(cur);
				if (cur) {
					rx_desc_info = qdf_nbuf_data(cur);
					head_fragno =
						dp_rx_frag_get_mpdu_frag_number(
								soc,
								rx_desc_info);
				}
			}

			if (cur_fragno == head_fragno) {
				dp_rx_nbuf_free(frag);
				goto insert_fail;
			}

			qdf_nbuf_set_next(prev, frag);
			qdf_nbuf_set_next(frag, cur);
		}
	}

	next = qdf_nbuf_next(*head_addr);

	rx_desc_info = qdf_nbuf_data(*tail_addr);
	last_morefrag = dp_rx_frag_get_more_frag_bit(soc, rx_desc_info);

	/* TODO: optimize the loop */
	if (!last_morefrag) {
		/* Check if all fragments are present */
		do {
			rx_desc_info = qdf_nbuf_data(next);
			next_fragno =
				dp_rx_frag_get_mpdu_frag_number(soc,
								rx_desc_info);
			count++;

			if (next_fragno != count)
				break;

			next = qdf_nbuf_next(next);
		} while (next);

		if (!next) {
			*all_frag_present = 1;
			return QDF_STATUS_SUCCESS;
		} else {
			/* revisit */
		}
	}

insert_done:
	return QDF_STATUS_SUCCESS;

insert_fail:
	return QDF_STATUS_E_FAILURE;
}


/*
 * dp_rx_defrag_tkip_decap(): decap tkip encrypted fragment
 * @msdu: Pointer to the fragment
 * @hdrlen: 802.11 header length (mostly useful in 4 addr frames)
 *
 * decap tkip encrypted fragment
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS
dp_rx_defrag_tkip_decap(struct dp_soc *soc,
			qdf_nbuf_t msdu, uint16_t hdrlen)
{
	uint8_t *ivp, *orig_hdr;
	int rx_desc_len = soc->rx_pkt_tlv_size;

	/* start of 802.11 header info */
	orig_hdr = (uint8_t *)(qdf_nbuf_data(msdu) + rx_desc_len);

	/* TKIP header is located post 802.11 header */
	ivp = orig_hdr + hdrlen;
	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"IEEE80211_WEP_EXTIV is missing in TKIP fragment");
		return QDF_STATUS_E_DEFRAG_ERROR;
	}

	qdf_nbuf_trim_tail(msdu, dp_f_tkip.ic_trailer);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag_ccmp_demic(): Remove MIC information from CCMP fragment
 * @nbuf: Pointer to the fragment buffer
 * @hdrlen: 802.11 header length (mostly useful in 4 addr frames)
 *
 * Remove MIC information from CCMP fragment
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS
dp_rx_defrag_ccmp_demic(struct dp_soc *soc, qdf_nbuf_t nbuf, uint16_t hdrlen)
{
	uint8_t *ivp, *orig_hdr;
	int rx_desc_len = soc->rx_pkt_tlv_size;

	/* start of the 802.11 header */
	orig_hdr = (uint8_t *)(qdf_nbuf_data(nbuf) + rx_desc_len);

	/* CCMP header is located after 802.11 header */
	ivp = orig_hdr + hdrlen;
	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV))
		return QDF_STATUS_E_DEFRAG_ERROR;

	qdf_nbuf_trim_tail(nbuf, dp_f_ccmp.ic_trailer);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag_ccmp_decap(): decap CCMP encrypted fragment
 * @nbuf: Pointer to the fragment
 * @hdrlen: length of the header information
 *
 * decap CCMP encrypted fragment
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS
dp_rx_defrag_ccmp_decap(struct dp_soc *soc, qdf_nbuf_t nbuf, uint16_t hdrlen)
{
	uint8_t *ivp, *origHdr;
	int rx_desc_len = soc->rx_pkt_tlv_size;

	origHdr = (uint8_t *) (qdf_nbuf_data(nbuf) + rx_desc_len);
	ivp = origHdr + hdrlen;

	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV))
		return QDF_STATUS_E_DEFRAG_ERROR;

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag_wep_decap(): decap WEP encrypted fragment
 * @msdu: Pointer to the fragment
 * @hdrlen: length of the header information
 *
 * decap WEP encrypted fragment
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS
dp_rx_defrag_wep_decap(struct dp_soc *soc, qdf_nbuf_t msdu, uint16_t hdrlen)
{
	uint8_t *origHdr;
	int rx_desc_len = soc->rx_pkt_tlv_size;

	origHdr = (uint8_t *) (qdf_nbuf_data(msdu) + rx_desc_len);
	qdf_mem_move(origHdr + dp_f_wep.ic_header, origHdr, hdrlen);

	qdf_nbuf_trim_tail(msdu, dp_f_wep.ic_trailer);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag_hdrsize(): Calculate the header size of the received fragment
 * @soc: soc handle
 * @nbuf: Pointer to the fragment
 *
 * Calculate the header size of the received fragment
 *
 * Returns: header size (uint16_t)
 */
static uint16_t dp_rx_defrag_hdrsize(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	uint16_t size = sizeof(struct ieee80211_frame);
	uint16_t fc = 0;
	uint32_t to_ds, fr_ds;
	uint8_t frm_ctrl_valid;
	uint16_t frm_ctrl_field;

	to_ds = hal_rx_mpdu_get_to_ds(soc->hal_soc, rx_tlv_hdr);
	fr_ds = hal_rx_mpdu_get_fr_ds(soc->hal_soc, rx_tlv_hdr);
	frm_ctrl_valid =
		hal_rx_get_mpdu_frame_control_valid(soc->hal_soc,
						    rx_tlv_hdr);
	frm_ctrl_field = hal_rx_get_frame_ctrl_field(soc->hal_soc, rx_tlv_hdr);

	if (to_ds && fr_ds)
		size += QDF_MAC_ADDR_SIZE;

	if (frm_ctrl_valid) {
		fc = frm_ctrl_field;

		/* use 1-st byte for validation */
		if (DP_RX_DEFRAG_IEEE80211_QOS_HAS_SEQ(fc & 0xff)) {
			size += sizeof(uint16_t);
			/* use 2-nd byte for validation */
			if (((fc & 0xff00) >> 8) & IEEE80211_FC1_ORDER)
				size += sizeof(struct ieee80211_htc);
		}
	}

	return size;
}

/*
 * dp_rx_defrag_michdr(): Calculate a pseudo MIC header
 * @wh0: Pointer to the wireless header of the fragment
 * @hdr: Array to hold the pseudo header
 *
 * Calculate a pseudo MIC header
 *
 * Returns: None
 */
static void dp_rx_defrag_michdr(const struct ieee80211_frame *wh0,
				uint8_t hdr[])
{
	const struct ieee80211_frame_addr4 *wh =
		(const struct ieee80211_frame_addr4 *)wh0;

	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr1); /* DA */
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr3); /* DA */
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr1); /* DA */
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr3);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr3); /* DA */
		DP_RX_DEFRAG_IEEE80211_ADDR_COPY(hdr + QDF_MAC_ADDR_SIZE,
					   wh->i_addr4);
		break;
	}

	/*
	 * Bit 7 is QDF_IEEE80211_FC0_SUBTYPE_QOS for data frame, but
	 * it could also be set for deauth, disassoc, action, etc. for
	 * a mgt type frame. It comes into picture for MFP.
	 */
	if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
		if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
				IEEE80211_FC1_DIR_DSTODS) {
			const struct ieee80211_qosframe_addr4 *qwh =
				(const struct ieee80211_qosframe_addr4 *)wh;
			hdr[12] = qwh->i_qos[0] & IEEE80211_QOS_TID;
		} else {
			const struct ieee80211_qosframe *qwh =
				(const struct ieee80211_qosframe *)wh;
			hdr[12] = qwh->i_qos[0] & IEEE80211_QOS_TID;
		}
	} else {
		hdr[12] = 0;
	}

	hdr[13] = hdr[14] = hdr[15] = 0;	/* reserved */
}

/*
 * dp_rx_defrag_mic(): Calculate MIC header
 * @key: Pointer to the key
 * @wbuf: fragment buffer
 * @off: Offset
 * @data_len: Data length
 * @mic: Array to hold MIC
 *
 * Calculate a pseudo MIC header
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS dp_rx_defrag_mic(struct dp_soc *soc, const uint8_t *key,
				   qdf_nbuf_t wbuf, uint16_t off,
				   uint16_t data_len, uint8_t mic[])
{
	uint8_t hdr[16] = { 0, };
	uint32_t l, r;
	const uint8_t *data;
	uint32_t space;
	int rx_desc_len = soc->rx_pkt_tlv_size;

	dp_rx_defrag_michdr((struct ieee80211_frame *)(qdf_nbuf_data(wbuf)
		+ rx_desc_len), hdr);

	l = dp_rx_get_le32(key);
	r = dp_rx_get_le32(key + 4);

	/* Michael MIC pseudo header: DA, SA, 3 x 0, Priority */
	l ^= dp_rx_get_le32(hdr);
	dp_rx_michael_block(l, r);
	l ^= dp_rx_get_le32(&hdr[4]);
	dp_rx_michael_block(l, r);
	l ^= dp_rx_get_le32(&hdr[8]);
	dp_rx_michael_block(l, r);
	l ^= dp_rx_get_le32(&hdr[12]);
	dp_rx_michael_block(l, r);

	/* first buffer has special handling */
	data = (uint8_t *)qdf_nbuf_data(wbuf) + off;
	space = qdf_nbuf_len(wbuf) - off;

	for (;; ) {
		if (space > data_len)
			space = data_len;

		/* collect 32-bit blocks from current buffer */
		while (space >= sizeof(uint32_t)) {
			l ^= dp_rx_get_le32(data);
			dp_rx_michael_block(l, r);
			data += sizeof(uint32_t);
			space -= sizeof(uint32_t);
			data_len -= sizeof(uint32_t);
		}
		if (data_len < sizeof(uint32_t))
			break;

		wbuf = qdf_nbuf_next(wbuf);
		if (!wbuf)
			return QDF_STATUS_E_DEFRAG_ERROR;

		if (space != 0) {
			const uint8_t *data_next;
			/*
			 * Block straddles buffers, split references.
			 */
			data_next =
				(uint8_t *)qdf_nbuf_data(wbuf) + off;
			if ((qdf_nbuf_len(wbuf)) <
				sizeof(uint32_t) - space) {
				return QDF_STATUS_E_DEFRAG_ERROR;
			}
			switch (space) {
			case 1:
				l ^= dp_rx_get_le32_split(data[0],
					data_next[0], data_next[1],
					data_next[2]);
				data = data_next + 3;
				space = (qdf_nbuf_len(wbuf) - off) - 3;
				break;
			case 2:
				l ^= dp_rx_get_le32_split(data[0], data[1],
						    data_next[0], data_next[1]);
				data = data_next + 2;
				space = (qdf_nbuf_len(wbuf) - off) - 2;
				break;
			case 3:
				l ^= dp_rx_get_le32_split(data[0], data[1],
					data[2], data_next[0]);
				data = data_next + 1;
				space = (qdf_nbuf_len(wbuf) - off) - 1;
				break;
			}
			dp_rx_michael_block(l, r);
			data_len -= sizeof(uint32_t);
		} else {
			/*
			 * Setup for next buffer.
			 */
			data = (uint8_t *)qdf_nbuf_data(wbuf) + off;
			space = qdf_nbuf_len(wbuf) - off;
		}
	}
	/* Last block and padding (0x5a, 4..7 x 0) */
	switch (data_len) {
	case 0:
		l ^= dp_rx_get_le32_split(0x5a, 0, 0, 0);
		break;
	case 1:
		l ^= dp_rx_get_le32_split(data[0], 0x5a, 0, 0);
		break;
	case 2:
		l ^= dp_rx_get_le32_split(data[0], data[1], 0x5a, 0);
		break;
	case 3:
		l ^= dp_rx_get_le32_split(data[0], data[1], data[2], 0x5a);
		break;
	}
	dp_rx_michael_block(l, r);
	dp_rx_michael_block(l, r);
	dp_rx_put_le32(mic, l);
	dp_rx_put_le32(mic + 4, r);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag_tkip_demic(): Remove MIC header from the TKIP frame
 * @key: Pointer to the key
 * @msdu: fragment buffer
 * @hdrlen: Length of the header information
 *
 * Remove MIC information from the TKIP frame
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS dp_rx_defrag_tkip_demic(struct dp_soc *soc,
					  const uint8_t *key,
					  qdf_nbuf_t msdu, uint16_t hdrlen)
{
	QDF_STATUS status;
	uint32_t pktlen = 0, prev_data_len;
	uint8_t mic[IEEE80211_WEP_MICLEN];
	uint8_t mic0[IEEE80211_WEP_MICLEN];
	qdf_nbuf_t prev = NULL, prev0, next;
	uint8_t len0 = 0;

	next = msdu;
	prev0 = msdu;
	while (next) {
		pktlen += (qdf_nbuf_len(next) - hdrlen);
		prev = next;
		dp_debug("pktlen %u",
			 (uint32_t)(qdf_nbuf_len(next) - hdrlen));
		next = qdf_nbuf_next(next);
		if (next && !qdf_nbuf_next(next))
			prev0 = prev;
	}

	if (!prev) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s Defrag chaining failed !\n", __func__);
		return QDF_STATUS_E_DEFRAG_ERROR;
	}

	prev_data_len = qdf_nbuf_len(prev) - hdrlen;
	if (prev_data_len < dp_f_tkip.ic_miclen) {
		if (prev0 == prev) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  "%s Fragments don't have MIC header !\n", __func__);
			return QDF_STATUS_E_DEFRAG_ERROR;
		}
		len0 = dp_f_tkip.ic_miclen - (uint8_t)prev_data_len;
		qdf_nbuf_copy_bits(prev0, qdf_nbuf_len(prev0) - len0, len0,
				   (caddr_t)mic0);
		qdf_nbuf_trim_tail(prev0, len0);
	}

	qdf_nbuf_copy_bits(prev, (qdf_nbuf_len(prev) -
			   (dp_f_tkip.ic_miclen - len0)),
			   (dp_f_tkip.ic_miclen - len0),
			   (caddr_t)(&mic0[len0]));
	qdf_nbuf_trim_tail(prev, (dp_f_tkip.ic_miclen - len0));
	pktlen -= dp_f_tkip.ic_miclen;

	if (((qdf_nbuf_len(prev) - hdrlen) == 0) && prev != msdu) {
		dp_rx_nbuf_free(prev);
		qdf_nbuf_set_next(prev0, NULL);
	}

	status = dp_rx_defrag_mic(soc, key, msdu, hdrlen,
				  pktlen, mic);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (qdf_mem_cmp(mic, mic0, dp_f_tkip.ic_miclen))
		return QDF_STATUS_E_DEFRAG_ERROR;

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_frag_pull_hdr(): Pulls the RXTLV & the 802.11 headers
 * @nbuf: buffer pointer
 * @hdrsize: size of the header to be pulled
 *
 * Pull the RXTLV & the 802.11 headers
 *
 * Returns: None
 */
static void dp_rx_frag_pull_hdr(struct dp_soc *soc,
				qdf_nbuf_t nbuf, uint16_t hdrsize)
{
	hal_rx_print_pn(soc->hal_soc, qdf_nbuf_data(nbuf));

	qdf_nbuf_pull_head(nbuf, soc->rx_pkt_tlv_size + hdrsize);

	dp_debug("final pktlen %d .11len %d",
		 (uint32_t)qdf_nbuf_len(nbuf), hdrsize);
}

/*
 * dp_rx_defrag_pn_check(): Check the PN of current fragmented with prev PN
 * @msdu: msdu to get the current PN
 * @cur_pn128: PN extracted from current msdu
 * @prev_pn128: Prev PN
 *
 * Returns: 0 on success, non zero on failure
 */
static int dp_rx_defrag_pn_check(struct dp_soc *soc, qdf_nbuf_t msdu,
				 uint64_t *cur_pn128, uint64_t *prev_pn128)
{
	int out_of_order = 0;

	hal_rx_tlv_get_pn_num(soc->hal_soc, qdf_nbuf_data(msdu), cur_pn128);

	if (cur_pn128[1] == prev_pn128[1])
		out_of_order = (cur_pn128[0] - prev_pn128[0] != 1);
	else
		out_of_order = (cur_pn128[1] - prev_pn128[1] != 1);

	return out_of_order;
}

/*
 * dp_rx_construct_fraglist(): Construct a nbuf fraglist
 * @txrx peer: Pointer to the txrx peer
 * @head: Pointer to list of fragments
 * @hdrsize: Size of the header to be pulled
 *
 * Construct a nbuf fraglist
 *
 * Returns: None
 */
static int
dp_rx_construct_fraglist(struct dp_txrx_peer *txrx_peer, int tid,
			 qdf_nbuf_t head,
			 uint16_t hdrsize)
{
	struct dp_soc *soc = txrx_peer->vdev->pdev->soc;
	qdf_nbuf_t msdu = qdf_nbuf_next(head);
	qdf_nbuf_t rx_nbuf = msdu;
	struct dp_rx_tid_defrag *rx_tid = &txrx_peer->rx_tid[tid];
	uint32_t len = 0;
	uint64_t cur_pn128[2] = {0, 0}, prev_pn128[2];
	int out_of_order = 0;
	int index;
	int needs_pn_check = 0;
	enum cdp_sec_type sec_type;

	prev_pn128[0] = rx_tid->pn128[0];
	prev_pn128[1] = rx_tid->pn128[1];

	index = hal_rx_msdu_is_wlan_mcast(soc->hal_soc, msdu) ? dp_sec_mcast :
				dp_sec_ucast;
	sec_type = txrx_peer->security[index].sec_type;

	if (!(sec_type == cdp_sec_type_none || sec_type == cdp_sec_type_wep128 ||
	      sec_type == cdp_sec_type_wep104 || sec_type == cdp_sec_type_wep40))
		needs_pn_check = 1;

	while (msdu) {
		if (qdf_likely(needs_pn_check))
			out_of_order = dp_rx_defrag_pn_check(soc, msdu,
							     &cur_pn128[0],
							     &prev_pn128[0]);

		if (qdf_unlikely(out_of_order)) {
			dp_info_rl("cur_pn128[0] 0x%llx cur_pn128[1] 0x%llx prev_pn128[0] 0x%llx prev_pn128[1] 0x%llx",
				   cur_pn128[0], cur_pn128[1],
				   prev_pn128[0], prev_pn128[1]);
			return QDF_STATUS_E_FAILURE;
		}

		prev_pn128[0] = cur_pn128[0];
		prev_pn128[1] = cur_pn128[1];

		/*
		 * Broadcast and multicast frames should never be fragmented.
		 * Iterating through all msdus and dropping fragments if even
		 * one of them has mcast/bcast destination address.
		 */
		if (hal_rx_msdu_is_wlan_mcast(soc->hal_soc, msdu)) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
				  "Dropping multicast/broadcast fragments");
			return QDF_STATUS_E_FAILURE;
		}

		dp_rx_frag_pull_hdr(soc, msdu, hdrsize);
		len += qdf_nbuf_len(msdu);
		msdu = qdf_nbuf_next(msdu);
	}

	qdf_nbuf_append_ext_list(head, rx_nbuf, len);
	qdf_nbuf_set_next(head, NULL);
	qdf_nbuf_set_is_frag(head, 1);

	dp_debug("head len %d ext len %d data len %d ",
		 (uint32_t)qdf_nbuf_len(head),
		 (uint32_t)qdf_nbuf_len(rx_nbuf),
		 (uint32_t)(head->data_len));

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_defrag_err() - rx err handler
 * @pdev: handle to pdev object
 * @vdev_id: vdev id
 * @peer_mac_addr: peer mac address
 * @tid: TID
 * @tsf32: TSF
 * @err_type: error type
 * @rx_frame: rx frame
 * @pn: PN Number
 * @key_id: key id
 *
 * This function handles rx error and send MIC error notification
 *
 * Return: None
 */
static void dp_rx_defrag_err(struct dp_vdev *vdev, qdf_nbuf_t nbuf)
{
	struct ol_if_ops *tops = NULL;
	struct dp_pdev *pdev = vdev->pdev;
	int rx_desc_len = pdev->soc->rx_pkt_tlv_size;
	uint8_t *orig_hdr;
	struct ieee80211_frame *wh;
	struct cdp_rx_mic_err_info mic_failure_info;

	orig_hdr = (uint8_t *)(qdf_nbuf_data(nbuf) + rx_desc_len);
	wh = (struct ieee80211_frame *)orig_hdr;

	qdf_copy_macaddr((struct qdf_mac_addr *)&mic_failure_info.da_mac_addr,
			 (struct qdf_mac_addr *)&wh->i_addr1);
	qdf_copy_macaddr((struct qdf_mac_addr *)&mic_failure_info.ta_mac_addr,
			 (struct qdf_mac_addr *)&wh->i_addr2);
	mic_failure_info.key_id = 0;
	mic_failure_info.multicast =
		IEEE80211_IS_MULTICAST(wh->i_addr1);
	qdf_mem_zero(mic_failure_info.tsc, MIC_SEQ_CTR_SIZE);
	mic_failure_info.frame_type = cdp_rx_frame_type_802_11;
	mic_failure_info.data = (uint8_t *)wh;
	mic_failure_info.vdev_id = vdev->vdev_id;

	tops = pdev->soc->cdp_soc.ol_ops;
	if (tops->rx_mic_error)
		tops->rx_mic_error(pdev->soc->ctrl_psoc, pdev->pdev_id,
				   &mic_failure_info);
}


/*
 * dp_rx_defrag_nwifi_to_8023(): Transcap 802.11 to 802.3
 * @soc: dp soc handle
 * @txrx_peer: txrx_peer handle
 * @nbuf: Pointer to the fragment buffer
 * @hdrsize: Size of headers
 *
 * Transcap the fragment from 802.11 to 802.3
 *
 * Returns: None
 */
static void
dp_rx_defrag_nwifi_to_8023(struct dp_soc *soc, struct dp_txrx_peer *txrx_peer,
			   int tid, qdf_nbuf_t nbuf, uint16_t hdrsize)
{
	struct llc_snap_hdr_t *llchdr;
	struct ethernet_hdr_t *eth_hdr;
	uint8_t ether_type[2];
	uint16_t fc = 0;
	union dp_align_mac_addr mac_addr;
	uint8_t *rx_desc_info = qdf_mem_malloc(soc->rx_pkt_tlv_size);
	struct dp_rx_tid_defrag *rx_tid = &txrx_peer->rx_tid[tid];
	struct ieee80211_frame_addr4 wh = {0};

	hal_rx_tlv_get_pn_num(soc->hal_soc, qdf_nbuf_data(nbuf), rx_tid->pn128);

	hal_rx_print_pn(soc->hal_soc, qdf_nbuf_data(nbuf));

	if (!rx_desc_info) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			"%s: Memory alloc failed ! ", __func__);
		QDF_ASSERT(0);
		return;
	}

	qdf_mem_zero(&wh, sizeof(struct ieee80211_frame_addr4));
	if (hal_rx_get_mpdu_mac_ad4_valid(soc->hal_soc, qdf_nbuf_data(nbuf)))
		qdf_mem_copy(&wh, qdf_nbuf_data(nbuf) + soc->rx_pkt_tlv_size,
			     hdrsize);

	qdf_mem_copy(rx_desc_info, qdf_nbuf_data(nbuf), soc->rx_pkt_tlv_size);

	llchdr = (struct llc_snap_hdr_t *)(qdf_nbuf_data(nbuf) +
					soc->rx_pkt_tlv_size + hdrsize);
	qdf_mem_copy(ether_type, llchdr->ethertype, 2);

	qdf_nbuf_pull_head(nbuf, (soc->rx_pkt_tlv_size + hdrsize +
				  sizeof(struct llc_snap_hdr_t) -
				  sizeof(struct ethernet_hdr_t)));

	eth_hdr = (struct ethernet_hdr_t *)(qdf_nbuf_data(nbuf));

	if (hal_rx_get_mpdu_frame_control_valid(soc->hal_soc,
						rx_desc_info))
		fc = hal_rx_get_frame_ctrl_field(soc->hal_soc, rx_desc_info);

	dp_debug("Frame control type: 0x%x", fc);

	switch (((fc & 0xff00) >> 8) & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		hal_rx_mpdu_get_addr1(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->dest_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		hal_rx_mpdu_get_addr2(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->src_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		break;
	case IEEE80211_FC1_DIR_TODS:
		hal_rx_mpdu_get_addr3(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->dest_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		hal_rx_mpdu_get_addr2(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->src_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		hal_rx_mpdu_get_addr1(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->dest_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		hal_rx_mpdu_get_addr3(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->src_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		break;

	case IEEE80211_FC1_DIR_DSTODS:
		hal_rx_mpdu_get_addr3(soc->hal_soc, rx_desc_info,
				      &mac_addr.raw[0]);
		qdf_mem_copy(eth_hdr->dest_addr, &mac_addr.raw[0],
			QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(eth_hdr->src_addr, &wh.i_addr4[0],
			     QDF_MAC_ADDR_SIZE);
		break;

	default:
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		"%s: Unknown frame control type: 0x%x", __func__, fc);
	}

	qdf_mem_copy(eth_hdr->ethertype, ether_type,
			sizeof(ether_type));

	qdf_nbuf_push_head(nbuf, soc->rx_pkt_tlv_size);
	qdf_mem_copy(qdf_nbuf_data(nbuf), rx_desc_info, soc->rx_pkt_tlv_size);
	qdf_mem_free(rx_desc_info);
}

#ifdef RX_DEFRAG_DO_NOT_REINJECT
/*
 * dp_rx_defrag_deliver(): Deliver defrag packet to stack
 * @peer: Pointer to the peer
 * @tid: Transmit Identifier
 * @head: Nbuf to be delivered
 *
 * Returns: None
 */
static inline void dp_rx_defrag_deliver(struct dp_txrx_peer *txrx_peer,
					unsigned int tid,
					qdf_nbuf_t head)
{
	struct dp_vdev *vdev = txrx_peer->vdev;
	struct dp_soc *soc = vdev->pdev->soc;
	qdf_nbuf_t deliver_list_head = NULL;
	qdf_nbuf_t deliver_list_tail = NULL;
	uint8_t *rx_tlv_hdr;

	rx_tlv_hdr = qdf_nbuf_data(head);

	QDF_NBUF_CB_RX_VDEV_ID(head) = vdev->vdev_id;
	qdf_nbuf_set_tid_val(head, tid);
	qdf_nbuf_pull_head(head, soc->rx_pkt_tlv_size);

	DP_RX_LIST_APPEND(deliver_list_head, deliver_list_tail,
			  head);
	dp_rx_deliver_to_stack(soc, vdev, txrx_peer, deliver_list_head,
			       deliver_list_tail);
}

/*
 * dp_rx_defrag_reo_reinject(): Reinject the fragment chain back into REO
 * @txrx peer: Pointer to the peer
 * @tid: Transmit Identifier
 * @head: Buffer to be reinjected back
 *
 * Reinject the fragment chain back into REO
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS dp_rx_defrag_reo_reinject(struct dp_txrx_peer *txrx_peer,
					    unsigned int tid, qdf_nbuf_t head)
{
	struct dp_rx_reorder_array_elem *rx_reorder_array_elem;

	rx_reorder_array_elem = txrx_peer->rx_tid[tid].array;

	dp_rx_defrag_deliver(txrx_peer, tid, head);
	rx_reorder_array_elem->head = NULL;
	rx_reorder_array_elem->tail = NULL;
	dp_rx_return_head_frag_desc(txrx_peer, tid);

	return QDF_STATUS_SUCCESS;
}
#else
#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
/**
 * dp_rx_reinject_ring_record_entry() - Record reinject ring history
 * @soc: Datapath soc structure
 * @paddr: paddr of the buffer reinjected to SW2REO ring
 * @sw_cookie: SW cookie of the buffer reinjected to SW2REO ring
 * @rbm: Return buffer manager of the buffer reinjected to SW2REO ring
 *
 * Returns: None
 */
static inline void
dp_rx_reinject_ring_record_entry(struct dp_soc *soc, uint64_t paddr,
				 uint32_t sw_cookie, uint8_t rbm)
{
	struct dp_buf_info_record *record;
	uint32_t idx;

	if (qdf_unlikely(!soc->rx_reinject_ring_history))
		return;

	idx = dp_history_get_next_index(&soc->rx_reinject_ring_history->index,
					DP_RX_REINJECT_HIST_MAX);

	/* No NULL check needed for record since its an array */
	record = &soc->rx_reinject_ring_history->entry[idx];

	record->timestamp = qdf_get_log_timestamp();
	record->hbi.paddr = paddr;
	record->hbi.sw_cookie = sw_cookie;
	record->hbi.rbm = rbm;
}
#else
static inline void
dp_rx_reinject_ring_record_entry(struct dp_soc *soc, uint64_t paddr,
				 uint32_t sw_cookie, uint8_t rbm)
{
}
#endif

/*
 * dp_rx_defrag_reo_reinject(): Reinject the fragment chain back into REO
 * @txrx_peer: Pointer to the txrx_peer
 * @tid: Transmit Identifier
 * @head: Buffer to be reinjected back
 *
 * Reinject the fragment chain back into REO
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS dp_rx_defrag_reo_reinject(struct dp_txrx_peer *txrx_peer,
					    unsigned int tid, qdf_nbuf_t head)
{
	struct dp_pdev *pdev = txrx_peer->vdev->pdev;
	struct dp_soc *soc = pdev->soc;
	struct hal_buf_info buf_info;
	struct hal_buf_info temp_buf_info;
	void *link_desc_va;
	void *msdu0, *msdu_desc_info;
	void *ent_ring_desc, *ent_mpdu_desc_info, *ent_qdesc_addr;
	void *dst_mpdu_desc_info;
	uint64_t dst_qdesc_addr;
	qdf_dma_addr_t paddr;
	uint32_t nbuf_len, seq_no, dst_ind;
	uint32_t ret, cookie;
	hal_ring_desc_t dst_ring_desc =
		txrx_peer->rx_tid[tid].dst_ring_desc;
	hal_ring_handle_t hal_srng = soc->reo_reinject_ring.hal_srng;
	struct dp_rx_desc *rx_desc = txrx_peer->rx_tid[tid].head_frag_desc;
	struct dp_rx_reorder_array_elem *rx_reorder_array_elem =
						txrx_peer->rx_tid[tid].array;
	qdf_nbuf_t nbuf_head;
	struct rx_desc_pool *rx_desc_pool = NULL;
	void *buf_addr_info = HAL_RX_REO_BUF_ADDR_INFO_GET(dst_ring_desc);
	uint8_t rx_defrag_rbm_id = dp_rx_get_defrag_bm_id(soc);

	/* do duplicate link desc address check */
	dp_rx_link_desc_refill_duplicate_check(
				soc,
				&soc->last_op_info.reo_reinject_link_desc,
				buf_addr_info);

	nbuf_head = dp_ipa_handle_rx_reo_reinject(soc, head);
	if (qdf_unlikely(!nbuf_head)) {
		dp_err_rl("IPA RX REO reinject failed");
		return QDF_STATUS_E_FAILURE;
	}

	/* update new allocated skb in case IPA is enabled */
	if (nbuf_head != head) {
		head = nbuf_head;
		rx_desc->nbuf = head;
		rx_reorder_array_elem->head = head;
	}

	ent_ring_desc = hal_srng_src_get_next(soc->hal_soc, hal_srng);
	if (!ent_ring_desc) {
		dp_err_rl("HAL src ring next entry NULL");
		return QDF_STATUS_E_FAILURE;
	}

	hal_rx_reo_buf_paddr_get(soc->hal_soc, dst_ring_desc, &buf_info);

	/* buffer_addr_info is the first element of ring_desc */
	hal_rx_buf_cookie_rbm_get(soc->hal_soc, (uint32_t *)dst_ring_desc,
				  &buf_info);

	link_desc_va = dp_rx_cookie_2_link_desc_va(soc, &buf_info);

	qdf_assert_always(link_desc_va);

	msdu0 = hal_rx_msdu0_buffer_addr_lsb(soc->hal_soc, link_desc_va);
	nbuf_len = qdf_nbuf_len(head) - soc->rx_pkt_tlv_size;

	HAL_RX_UNIFORM_HDR_SET(link_desc_va, OWNER, UNI_DESC_OWNER_SW);
	HAL_RX_UNIFORM_HDR_SET(link_desc_va, BUFFER_TYPE,
			UNI_DESC_BUF_TYPE_RX_MSDU_LINK);

	/* msdu reconfig */
	msdu_desc_info = hal_rx_msdu_desc_info_ptr_get(soc->hal_soc, msdu0);

	dst_ind = hal_rx_msdu_reo_dst_ind_get(soc->hal_soc, link_desc_va);

	qdf_mem_zero(msdu_desc_info, sizeof(struct rx_msdu_desc_info));

	hal_msdu_desc_info_set(soc->hal_soc, msdu_desc_info, dst_ind, nbuf_len);

	/* change RX TLV's */
	hal_rx_tlv_msdu_len_set(soc->hal_soc, qdf_nbuf_data(head), nbuf_len);

	hal_rx_buf_cookie_rbm_get(soc->hal_soc, (uint32_t *)msdu0,
				  &temp_buf_info);

	cookie = temp_buf_info.sw_cookie;
	rx_desc_pool = &soc->rx_desc_buf[pdev->lmac_id];

	/* map the nbuf before reinject it into HW */
	ret = qdf_nbuf_map_nbytes_single(soc->osdev, head,
					 QDF_DMA_FROM_DEVICE,
					 rx_desc_pool->buf_size);
	if (qdf_unlikely(ret == QDF_STATUS_E_FAILURE)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				"%s: nbuf map failed !", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	dp_ipa_handle_rx_buf_smmu_mapping(soc, head, rx_desc_pool->buf_size,
					  true, __func__, __LINE__);

	dp_audio_smmu_map(soc->osdev,
			  qdf_mem_paddr_from_dmaaddr(soc->osdev,
						     QDF_NBUF_CB_PADDR(head)),
			  QDF_NBUF_CB_PADDR(head), rx_desc_pool->buf_size);

	/*
	 * As part of rx frag handler buffer was unmapped and rx desc
	 * unmapped is set to 1. So again for defrag reinject frame reset
	 * it back to 0.
	 */
	rx_desc->unmapped = 0;

	paddr = qdf_nbuf_get_frag_paddr(head, 0);

	ret = dp_check_paddr(soc, &head, &paddr, rx_desc_pool);

	if (ret == QDF_STATUS_E_FAILURE) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				"%s: x86 check failed !", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	hal_rxdma_buff_addr_info_set(soc->hal_soc, msdu0, paddr, cookie,
				     rx_defrag_rbm_id);

	/* Lets fill entrance ring now !!! */
	if (qdf_unlikely(hal_srng_access_start(soc->hal_soc, hal_srng))) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
		"HAL RING Access For REO entrance SRNG Failed: %pK",
		hal_srng);

		return QDF_STATUS_E_FAILURE;
	}

	dp_rx_reinject_ring_record_entry(soc, paddr, cookie,
					 rx_defrag_rbm_id);
	paddr = (uint64_t)buf_info.paddr;
	/* buf addr */
	hal_rxdma_buff_addr_info_set(soc->hal_soc, ent_ring_desc, paddr,
				     buf_info.sw_cookie,
				     soc->idle_link_bm_id);
	/* mpdu desc info */
	ent_mpdu_desc_info = hal_ent_mpdu_desc_info(soc->hal_soc,
						    ent_ring_desc);
	dst_mpdu_desc_info = hal_dst_mpdu_desc_info(soc->hal_soc,
						    dst_ring_desc);

	qdf_mem_copy(ent_mpdu_desc_info, dst_mpdu_desc_info,
				sizeof(struct rx_mpdu_desc_info));
	qdf_mem_zero(ent_mpdu_desc_info, sizeof(uint32_t));

	seq_no = hal_rx_get_rx_sequence(soc->hal_soc, rx_desc->rx_buf_start);

	hal_mpdu_desc_info_set(soc->hal_soc, ent_ring_desc, ent_mpdu_desc_info,
			       seq_no);
	/* qdesc addr */
	ent_qdesc_addr = hal_get_reo_ent_desc_qdesc_addr(soc->hal_soc,
						(uint8_t *)ent_ring_desc);

	dst_qdesc_addr = soc->arch_ops.get_reo_qdesc_addr(
						soc->hal_soc,
						(uint8_t *)dst_ring_desc,
						qdf_nbuf_data(head),
						txrx_peer, tid);

	qdf_mem_copy(ent_qdesc_addr, &dst_qdesc_addr, 5);

	hal_set_reo_ent_desc_reo_dest_ind(soc->hal_soc,
					  (uint8_t *)ent_ring_desc, dst_ind);

	hal_srng_access_end(soc->hal_soc, hal_srng);

	DP_STATS_INC(soc, rx.reo_reinject, 1);
	dp_debug("reinjection done !");
	return QDF_STATUS_SUCCESS;
}
#endif

/*
 * dp_rx_defrag_gcmp_demic(): Remove MIC information from GCMP fragment
 * @soc: Datapath soc structure
 * @nbuf: Pointer to the fragment buffer
 * @hdrlen: 802.11 header length
 *
 * Remove MIC information from GCMP fragment
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS dp_rx_defrag_gcmp_demic(struct dp_soc *soc, qdf_nbuf_t nbuf,
					  uint16_t hdrlen)
{
	uint8_t *ivp, *orig_hdr;
	int rx_desc_len = soc->rx_pkt_tlv_size;

	/* start of the 802.11 header */
	orig_hdr = (uint8_t *)(qdf_nbuf_data(nbuf) + rx_desc_len);

	/*
	 * GCMP header is located after 802.11 header and EXTIV
	 * field should always be set to 1 for GCMP protocol.
	 */
	ivp = orig_hdr + hdrlen;
	if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV))
		return QDF_STATUS_E_DEFRAG_ERROR;

	qdf_nbuf_trim_tail(nbuf, dp_f_gcmp.ic_trailer);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag(): Defragment the fragment chain
 * @txrx peer: Pointer to the peer
 * @tid: Transmit Identifier
 * @frag_list_head: Pointer to head list
 * @frag_list_tail: Pointer to tail list
 *
 * Defragment the fragment chain
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS dp_rx_defrag(struct dp_txrx_peer *txrx_peer, unsigned int tid,
			       qdf_nbuf_t frag_list_head,
			       qdf_nbuf_t frag_list_tail)
{
	qdf_nbuf_t tmp_next;
	qdf_nbuf_t cur = frag_list_head, msdu;
	uint32_t index, tkip_demic = 0;
	uint16_t hdr_space;
	uint8_t key[DEFRAG_IEEE80211_KEY_LEN];
	struct dp_vdev *vdev = txrx_peer->vdev;
	struct dp_soc *soc = vdev->pdev->soc;
	uint8_t status = 0;

	if (!cur)
		return QDF_STATUS_E_DEFRAG_ERROR;

	hdr_space = dp_rx_defrag_hdrsize(soc, cur);
	index = hal_rx_msdu_is_wlan_mcast(soc->hal_soc, cur) ?
		dp_sec_mcast : dp_sec_ucast;

	/* Remove FCS from all fragments */
	while (cur) {
		tmp_next = qdf_nbuf_next(cur);
		qdf_nbuf_set_next(cur, NULL);
		qdf_nbuf_trim_tail(cur, DEFRAG_IEEE80211_FCS_LEN);
		qdf_nbuf_set_next(cur, tmp_next);
		cur = tmp_next;
	}
	cur = frag_list_head;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: index %d Security type: %d", __func__,
		  index, txrx_peer->security[index].sec_type);

	switch (txrx_peer->security[index].sec_type) {
	case cdp_sec_type_tkip:
		tkip_demic = 1;
		fallthrough;
	case cdp_sec_type_tkip_nomic:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (dp_rx_defrag_tkip_decap(soc, cur, hdr_space)) {

				QDF_TRACE(QDF_MODULE_ID_TXRX,
					QDF_TRACE_LEVEL_ERROR,
					"dp_rx_defrag: TKIP decap failed");

				return QDF_STATUS_E_DEFRAG_ERROR;
			}
			cur = tmp_next;
		}

		/* If success, increment header to be stripped later */
		hdr_space += dp_f_tkip.ic_header;
		break;

	case cdp_sec_type_aes_ccmp:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (dp_rx_defrag_ccmp_demic(soc, cur, hdr_space)) {

				QDF_TRACE(QDF_MODULE_ID_TXRX,
					QDF_TRACE_LEVEL_ERROR,
					"dp_rx_defrag: CCMP demic failed");

				return QDF_STATUS_E_DEFRAG_ERROR;
			}
			if (dp_rx_defrag_ccmp_decap(soc, cur, hdr_space)) {

				QDF_TRACE(QDF_MODULE_ID_TXRX,
					QDF_TRACE_LEVEL_ERROR,
					"dp_rx_defrag: CCMP decap failed");

				return QDF_STATUS_E_DEFRAG_ERROR;
			}
			cur = tmp_next;
		}

		/* If success, increment header to be stripped later */
		hdr_space += dp_f_ccmp.ic_header;
		break;

	case cdp_sec_type_wep40:
	case cdp_sec_type_wep104:
	case cdp_sec_type_wep128:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (dp_rx_defrag_wep_decap(soc, cur, hdr_space)) {

				QDF_TRACE(QDF_MODULE_ID_TXRX,
					QDF_TRACE_LEVEL_ERROR,
					"dp_rx_defrag: WEP decap failed");

				return QDF_STATUS_E_DEFRAG_ERROR;
			}
			cur = tmp_next;
		}

		/* If success, increment header to be stripped later */
		hdr_space += dp_f_wep.ic_header;
		break;
	case cdp_sec_type_aes_gcmp:
	case cdp_sec_type_aes_gcmp_256:
		while (cur) {
			tmp_next = qdf_nbuf_next(cur);
			if (dp_rx_defrag_gcmp_demic(soc, cur, hdr_space)) {
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "dp_rx_defrag: GCMP demic failed");

				return QDF_STATUS_E_DEFRAG_ERROR;
			}
			cur = tmp_next;
		}

		hdr_space += dp_f_gcmp.ic_header;
		break;
	default:
		break;
	}

	if (tkip_demic) {
		msdu = frag_list_head;
		qdf_mem_copy(key,
			     &txrx_peer->security[index].michael_key[0],
			     IEEE80211_WEP_MICLEN);
		status = dp_rx_defrag_tkip_demic(soc, key, msdu,
						 soc->rx_pkt_tlv_size +
						 hdr_space);

		if (status) {
			dp_rx_defrag_err(vdev, frag_list_head);

			QDF_TRACE(QDF_MODULE_ID_TXRX,
				  QDF_TRACE_LEVEL_ERROR,
				  "%s: TKIP demic failed status %d",
				   __func__, status);

			return QDF_STATUS_E_DEFRAG_ERROR;
		}
	}

	/* Convert the header to 802.3 header */
	dp_rx_defrag_nwifi_to_8023(soc, txrx_peer, tid, frag_list_head,
				   hdr_space);
	if (qdf_nbuf_next(frag_list_head)) {
		if (dp_rx_construct_fraglist(txrx_peer, tid, frag_list_head,
					     hdr_space))
			return QDF_STATUS_E_DEFRAG_ERROR;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_defrag_cleanup(): Clean up activities
 * @txrx_peer: Pointer to the peer
 * @tid: Transmit Identifier
 *
 * Returns: None
 */
void dp_rx_defrag_cleanup(struct dp_txrx_peer *txrx_peer, unsigned int tid)
{
	struct dp_rx_reorder_array_elem *rx_reorder_array_elem =
				txrx_peer->rx_tid[tid].array;

	if (rx_reorder_array_elem) {
		/* Free up nbufs */
		dp_rx_defrag_frames_free(rx_reorder_array_elem->head);
		rx_reorder_array_elem->head = NULL;
		rx_reorder_array_elem->tail = NULL;
	} else {
		dp_info("Cleanup self peer %pK and TID %u",
			txrx_peer, tid);
	}

	/* Free up saved ring descriptors */
	dp_rx_clear_saved_desc_info(txrx_peer, tid);

	txrx_peer->rx_tid[tid].defrag_timeout_ms = 0;
	txrx_peer->rx_tid[tid].curr_frag_num = 0;
	txrx_peer->rx_tid[tid].curr_seq_num = 0;
}

/*
 * dp_rx_defrag_save_info_from_ring_desc(): Save info from REO ring descriptor
 * @soc: Pointer to the SOC data structure
 * @ring_desc: Pointer to the dst ring descriptor
 * @txrx_peer: Pointer to the peer
 * @tid: Transmit Identifier
 *
 * Returns: None
 */
static QDF_STATUS
dp_rx_defrag_save_info_from_ring_desc(struct dp_soc *soc,
				      hal_ring_desc_t ring_desc,
				      struct dp_rx_desc *rx_desc,
				      struct dp_txrx_peer *txrx_peer,
				      unsigned int tid)
{
	void *dst_ring_desc;

	dst_ring_desc = qdf_mem_malloc(hal_srng_get_entrysize(soc->hal_soc,
							      REO_DST));

	if (!dst_ring_desc) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			"%s: Memory alloc failed !", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_copy(dst_ring_desc, ring_desc,
		     hal_srng_get_entrysize(soc->hal_soc, REO_DST));

	txrx_peer->rx_tid[tid].dst_ring_desc = dst_ring_desc;
	txrx_peer->rx_tid[tid].head_frag_desc = rx_desc;

	return QDF_STATUS_SUCCESS;
}

#ifdef DP_RX_DEFRAG_ADDR1_CHECK_WAR
#ifdef WLAN_FEATURE_11BE_MLO
/*
 * dp_rx_defrag_vdev_mac_addr_cmp() - function to check whether mac address
 *				matches VDEV mac
 * @vdev: dp_vdev object of the VDEV on which this data packet is received
 * @mac_addr: Address to compare
 *
 * Return: 1 if the mac matching,
 *         0 if this frame is not correctly destined to this VDEV/MLD
 */
static int dp_rx_defrag_vdev_mac_addr_cmp(struct dp_vdev *vdev,
					  uint8_t *mac_addr)
{
	return ((qdf_mem_cmp(mac_addr, &vdev->mac_addr.raw[0],
			     QDF_MAC_ADDR_SIZE) == 0) ||
		(qdf_mem_cmp(mac_addr, &vdev->mld_mac_addr.raw[0],
			     QDF_MAC_ADDR_SIZE) == 0));
}

#else
static int dp_rx_defrag_vdev_mac_addr_cmp(struct dp_vdev *vdev,
					  uint8_t *mac_addr)
{
	return (qdf_mem_cmp(mac_addr, &vdev->mac_addr.raw[0],
			    QDF_MAC_ADDR_SIZE) == 0);
}
#endif

static bool dp_rx_defrag_addr1_check(struct dp_soc *soc,
				     struct dp_vdev *vdev,
				     uint8_t *rx_tlv_hdr)
{
	union dp_align_mac_addr mac_addr;

	/* If address1 is not valid discard the fragment */
	if (hal_rx_mpdu_get_addr1(soc->hal_soc, rx_tlv_hdr,
				  &mac_addr.raw[0]) != QDF_STATUS_SUCCESS) {
		DP_STATS_INC(soc, rx.err.defrag_ad1_invalid, 1);
		return false;
	}

	/* WAR suggested by HW team to avoid crashing incase of packet
	 * corruption issue
	 *
	 * recipe is to compare VDEV mac or MLD mac address with ADDR1
	 * in case of mismatch consider it as corrupted packet and do
	 * not process further
	 */
	if (!dp_rx_defrag_vdev_mac_addr_cmp(vdev,
					    &mac_addr.raw[0])) {
		DP_STATS_INC(soc, rx.err.defrag_ad1_invalid, 1);
		return false;
	}

	return true;
}
#else
static inline bool dp_rx_defrag_addr1_check(struct dp_soc *soc,
					    struct dp_vdev *vdev,
					    uint8_t *rx_tlv_hdr)
{

	return true;
}
#endif

/*
 * dp_rx_defrag_store_fragment(): Store incoming fragments
 * @soc: Pointer to the SOC data structure
 * @ring_desc: Pointer to the ring descriptor
 * @mpdu_desc_info: MPDU descriptor info
 * @tid: Traffic Identifier
 * @rx_desc: Pointer to rx descriptor
 * @rx_bfs: Number of bfs consumed
 *
 * Returns: QDF_STATUS
 */
static QDF_STATUS
dp_rx_defrag_store_fragment(struct dp_soc *soc,
			    hal_ring_desc_t ring_desc,
			    union dp_rx_desc_list_elem_t **head,
			    union dp_rx_desc_list_elem_t **tail,
			    struct hal_rx_mpdu_desc_info *mpdu_desc_info,
			    unsigned int tid, struct dp_rx_desc *rx_desc,
			    uint32_t *rx_bfs)
{
	struct dp_rx_reorder_array_elem *rx_reorder_array_elem;
	struct dp_pdev *pdev;
	struct dp_txrx_peer *txrx_peer = NULL;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	uint16_t peer_id;
	uint8_t fragno, more_frag, all_frag_present = 0;
	uint16_t rxseq = mpdu_desc_info->mpdu_seq;
	QDF_STATUS status;
	struct dp_rx_tid_defrag *rx_tid;
	uint8_t mpdu_sequence_control_valid;
	uint8_t mpdu_frame_control_valid;
	qdf_nbuf_t frag = rx_desc->nbuf;
	uint32_t msdu_len;

	if (qdf_nbuf_len(frag) > 0) {
		dp_info("Dropping unexpected packet with skb_len: %d,"
			"data len: %d, cookie: %d",
			(uint32_t)qdf_nbuf_len(frag), frag->data_len,
			rx_desc->cookie);
		DP_STATS_INC(soc, rx.rx_frag_err_len_error, 1);
		goto discard_frag;
	}

	if (dp_rx_buffer_pool_refill(soc, frag, rx_desc->pool_id)) {
		/* fragment queued back to the pool, free the link desc */
		goto err_free_desc;
	}

	msdu_len = hal_rx_msdu_start_msdu_len_get(soc->hal_soc,
						  rx_desc->rx_buf_start);

	qdf_nbuf_set_pktlen(frag, (msdu_len + soc->rx_pkt_tlv_size));
	qdf_nbuf_append_ext_list(frag, NULL, 0);

	/* Check if the packet is from a valid peer */
	peer_id = dp_rx_peer_metadata_peer_id_get(soc,
					       mpdu_desc_info->peer_meta_data);
	txrx_peer = dp_txrx_peer_get_ref_by_id(soc, peer_id, &txrx_ref_handle,
					       DP_MOD_ID_RX_ERR);

	if (!txrx_peer) {
		/* We should not receive anything from unknown peer
		 * however, that might happen while we are in the monitor mode.
		 * We don't need to handle that here
		 */
		dp_info_rl("Unknown peer with peer_id %d, dropping fragment",
			   peer_id);
		DP_STATS_INC(soc, rx.rx_frag_err_no_peer, 1);
		goto discard_frag;
	}

	if (tid >= DP_MAX_TIDS) {
		dp_info("TID out of bounds: %d", tid);
		qdf_assert_always(0);
		goto discard_frag;
	}

	if (!dp_rx_defrag_addr1_check(soc, txrx_peer->vdev,
				      rx_desc->rx_buf_start)) {
		dp_info("Invalid address 1");
		goto discard_frag;
	}

	mpdu_sequence_control_valid =
		hal_rx_get_mpdu_sequence_control_valid(soc->hal_soc,
						       rx_desc->rx_buf_start);

	/* Invalid MPDU sequence control field, MPDU is of no use */
	if (!mpdu_sequence_control_valid) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"Invalid MPDU seq control field, dropping MPDU");

		qdf_assert(0);
		goto discard_frag;
	}

	mpdu_frame_control_valid =
		hal_rx_get_mpdu_frame_control_valid(soc->hal_soc,
						    rx_desc->rx_buf_start);

	/* Invalid frame control field */
	if (!mpdu_frame_control_valid) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"Invalid frame control field, dropping MPDU");

		qdf_assert(0);
		goto discard_frag;
	}

	/* Current mpdu sequence */
	more_frag = dp_rx_frag_get_more_frag_bit(soc, rx_desc->rx_buf_start);

	/* HW does not populate the fragment number as of now
	 * need to get from the 802.11 header
	 */
	fragno = dp_rx_frag_get_mpdu_frag_number(soc, rx_desc->rx_buf_start);

	pdev = txrx_peer->vdev->pdev;
	rx_tid = &txrx_peer->rx_tid[tid];

	dp_rx_err_send_pktlog(soc, pdev, mpdu_desc_info, frag,
			      QDF_TX_RX_STATUS_OK, false);

	qdf_spin_lock_bh(&rx_tid->defrag_tid_lock);
	rx_reorder_array_elem = txrx_peer->rx_tid[tid].array;
	if (!rx_reorder_array_elem) {
		dp_err_rl("Rcvd Fragmented pkt before tid setup for peer %pK",
			  txrx_peer);
		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
		goto discard_frag;
	}

	/*
	 * !more_frag: no more fragments to be delivered
	 * !frag_no: packet is not fragmented
	 * !rx_reorder_array_elem->head: no saved fragments so far
	 */
	if ((!more_frag) && (!fragno) && (!rx_reorder_array_elem->head)) {
		/* We should not get into this situation here.
		 * It means an unfragmented packet with fragment flag
		 * is delivered over the REO exception ring.
		 * Typically it follows normal rx path.
		 */
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"Rcvd unfragmented pkt on REO Err srng, dropping");

		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
		qdf_assert(0);
		goto discard_frag;
	}

	/* Check if the fragment is for the same sequence or a different one */
	dp_debug("rx_tid %d", tid);
	if (rx_reorder_array_elem->head) {
		dp_debug("rxseq %d\n", rxseq);
		if (rxseq != rx_tid->curr_seq_num) {

			dp_debug("mismatch cur_seq %d rxseq %d\n",
				 rx_tid->curr_seq_num, rxseq);
			/* Drop stored fragments if out of sequence
			 * fragment is received
			 */
			dp_rx_reorder_flush_frag(txrx_peer, tid);

			DP_STATS_INC(soc, rx.rx_frag_oor, 1);

			dp_debug("cur rxseq %d\n", rxseq);
			/*
			 * The sequence number for this fragment becomes the
			 * new sequence number to be processed
			 */
			rx_tid->curr_seq_num = rxseq;
		}
	} else {
		/* Check if we are processing first fragment if it is
		 * not first fragment discard fragment.
		 */
		if (fragno) {
			qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
			goto discard_frag;
		}
		dp_debug("cur rxseq %d\n", rxseq);
		/* Start of a new sequence */
		dp_rx_defrag_cleanup(txrx_peer, tid);
		rx_tid->curr_seq_num = rxseq;
		/* store PN number also */
	}

	/*
	 * If the earlier sequence was dropped, this will be the fresh start.
	 * Else, continue with next fragment in a given sequence
	 */
	status = dp_rx_defrag_fraglist_insert(txrx_peer, tid,
					      &rx_reorder_array_elem->head,
					      &rx_reorder_array_elem->tail,
					      frag, &all_frag_present);

	/*
	 * Currently, we can have only 6 MSDUs per-MPDU, if the current
	 * packet sequence has more than 6 MSDUs for some reason, we will
	 * have to use the next MSDU link descriptor and chain them together
	 * before reinjection.
	 * ring_desc is validated in dp_rx_err_process.
	 */
	if ((fragno == 0) && (status == QDF_STATUS_SUCCESS) &&
			(rx_reorder_array_elem->head == frag)) {

		status = dp_rx_defrag_save_info_from_ring_desc(soc, ring_desc,
							       rx_desc,
							       txrx_peer, tid);

		if (status != QDF_STATUS_SUCCESS) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				"%s: Unable to store ring desc !", __func__);
			qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
			goto discard_frag;
		}
	} else {
		dp_rx_add_to_free_desc_list(head, tail, rx_desc);
		(*rx_bfs)++;

		/* Return the non-head link desc */
		if (dp_rx_link_desc_return(soc, ring_desc,
					   HAL_BM_ACTION_PUT_IN_IDLE_LIST) !=
		    QDF_STATUS_SUCCESS)
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  "%s: Failed to return link desc", __func__);

	}

	if (pdev->soc->rx.flags.defrag_timeout_check)
		dp_rx_defrag_waitlist_remove(txrx_peer, tid);

	/* Yet to receive more fragments for this sequence number */
	if (!all_frag_present) {
		uint32_t now_ms =
			qdf_system_ticks_to_msecs(qdf_system_ticks());

		txrx_peer->rx_tid[tid].defrag_timeout_ms =
			now_ms + pdev->soc->rx.defrag.timeout_ms;

		dp_rx_defrag_waitlist_add(txrx_peer, tid);
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX_ERR);
		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);

		return QDF_STATUS_SUCCESS;
	}

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "All fragments received for sequence: %d", rxseq);

	/* Process the fragments */
	status = dp_rx_defrag(txrx_peer, tid, rx_reorder_array_elem->head,
			      rx_reorder_array_elem->tail);
	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"Fragment processing failed");

		dp_rx_add_to_free_desc_list(head, tail,
				txrx_peer->rx_tid[tid].head_frag_desc);
		(*rx_bfs)++;

		if (dp_rx_link_desc_return(soc,
					txrx_peer->rx_tid[tid].dst_ring_desc,
					HAL_BM_ACTION_PUT_IN_IDLE_LIST) !=
				QDF_STATUS_SUCCESS)
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
					"%s: Failed to return link desc",
					__func__);
		dp_rx_defrag_cleanup(txrx_peer, tid);
		qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);
		goto end;
	}

	/* Re-inject the fragments back to REO for further processing */
	status = dp_rx_defrag_reo_reinject(txrx_peer, tid,
					   rx_reorder_array_elem->head);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		rx_reorder_array_elem->head = NULL;
		rx_reorder_array_elem->tail = NULL;
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  "Fragmented sequence successfully reinjected");
	} else {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
		"Fragmented sequence reinjection failed");
		dp_rx_return_head_frag_desc(txrx_peer, tid);
	}

	dp_rx_defrag_cleanup(txrx_peer, tid);
	qdf_spin_unlock_bh(&rx_tid->defrag_tid_lock);

	dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX_ERR);

	return QDF_STATUS_SUCCESS;

discard_frag:
	dp_rx_nbuf_free(frag);
err_free_desc:
	dp_rx_add_to_free_desc_list(head, tail, rx_desc);
	if (dp_rx_link_desc_return(soc, ring_desc,
				   HAL_BM_ACTION_PUT_IN_IDLE_LIST) !=
	    QDF_STATUS_SUCCESS)
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: Failed to return link desc", __func__);
	(*rx_bfs)++;

end:
	if (txrx_peer)
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX_ERR);

	DP_STATS_INC(soc, rx.rx_frag_err, 1);
	return QDF_STATUS_E_DEFRAG_ERROR;
}

/**
 * dp_rx_frag_handle() - Handles fragmented Rx frames
 *
 * @soc: core txrx main context
 * @ring_desc: opaque pointer to the REO error ring descriptor
 * @mpdu_desc_info: MPDU descriptor information from ring descriptor
 * @head: head of the local descriptor free-list
 * @tail: tail of the local descriptor free-list
 * @quota: No. of units (packets) that can be serviced in one shot.
 *
 * This function implements RX 802.11 fragmentation handling
 * The handling is mostly same as legacy fragmentation handling.
 * If required, this function can re-inject the frames back to
 * REO ring (with proper setting to by-pass fragmentation check
 * but use duplicate detection / re-ordering and routing these frames
 * to a different core.
 *
 * Return: uint32_t: No. of elements processed
 */
uint32_t dp_rx_frag_handle(struct dp_soc *soc, hal_ring_desc_t ring_desc,
			   struct hal_rx_mpdu_desc_info *mpdu_desc_info,
			   struct dp_rx_desc *rx_desc,
			   uint8_t *mac_id,
			   uint32_t quota)
{
	uint32_t rx_bufs_used = 0;
	qdf_nbuf_t msdu = NULL;
	uint32_t tid;
	uint32_t rx_bfs = 0;
	struct dp_pdev *pdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct rx_desc_pool *rx_desc_pool;

	qdf_assert(soc);
	qdf_assert(mpdu_desc_info);
	qdf_assert(rx_desc);

	dp_debug("Number of MSDUs to process, num_msdus: %d",
		 mpdu_desc_info->msdu_count);


	if (qdf_unlikely(mpdu_desc_info->msdu_count == 0)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"Not sufficient MSDUs to process");
		return rx_bufs_used;
	}

	/* all buffers in MSDU link belong to same pdev */
	pdev = dp_get_pdev_for_lmac_id(soc, rx_desc->pool_id);
	if (!pdev) {
		dp_nofl_debug("pdev is null for pool_id = %d",
			      rx_desc->pool_id);
		return rx_bufs_used;
	}

	*mac_id = rx_desc->pool_id;

	msdu = rx_desc->nbuf;

	rx_desc_pool = &soc->rx_desc_buf[rx_desc->pool_id];

	if (rx_desc->unmapped)
		return rx_bufs_used;

	dp_ipa_rx_buf_smmu_mapping_lock(soc);
	dp_rx_nbuf_unmap_pool(soc, rx_desc_pool, rx_desc->nbuf);
	rx_desc->unmapped = 1;
	dp_ipa_rx_buf_smmu_mapping_unlock(soc);

	rx_desc->rx_buf_start = qdf_nbuf_data(msdu);

	tid = hal_rx_mpdu_start_tid_get(soc->hal_soc, rx_desc->rx_buf_start);

	/* Process fragment-by-fragment */
	status = dp_rx_defrag_store_fragment(soc, ring_desc,
					     &pdev->free_list_head,
					     &pdev->free_list_tail,
					     mpdu_desc_info,
					     tid, rx_desc, &rx_bfs);

	if (rx_bfs)
		rx_bufs_used += rx_bfs;

	if (!QDF_IS_STATUS_SUCCESS(status))
		dp_info_rl("Rx Defrag err seq#:0x%x msdu_count:%d flags:%d",
			   mpdu_desc_info->mpdu_seq,
			   mpdu_desc_info->msdu_count,
			   mpdu_desc_info->mpdu_flags);

	return rx_bufs_used;
}

QDF_STATUS dp_rx_defrag_add_last_frag(struct dp_soc *soc,
				      struct dp_txrx_peer *txrx_peer,
				      uint16_t tid,
				      uint16_t rxseq, qdf_nbuf_t nbuf)
{
	struct dp_rx_tid_defrag *rx_tid = &txrx_peer->rx_tid[tid];
	struct dp_rx_reorder_array_elem *rx_reorder_array_elem;
	uint8_t all_frag_present;
	uint32_t msdu_len;
	QDF_STATUS status;

	rx_reorder_array_elem = txrx_peer->rx_tid[tid].array;

	/*
	 * HW may fill in unexpected peer_id in RX PKT TLV,
	 * if this peer_id related peer is valid by coincidence,
	 * but actually this peer won't do dp_peer_rx_init(like SAP vdev
	 * self peer), then invalid access to rx_reorder_array_elem happened.
	 */
	if (!rx_reorder_array_elem) {
		dp_verbose_debug(
			"peer id:%d drop rx frame!",
			txrx_peer->peer_id);
		DP_STATS_INC(soc, rx.err.defrag_peer_uninit, 1);
		dp_rx_nbuf_free(nbuf);
		goto fail;
	}

	if (rx_reorder_array_elem->head &&
	    rxseq != rx_tid->curr_seq_num) {
		/* Drop stored fragments if out of sequence
		 * fragment is received
		 */
		dp_rx_reorder_flush_frag(txrx_peer, tid);

		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: No list found for TID %d Seq# %d",
				__func__, tid, rxseq);
		dp_rx_nbuf_free(nbuf);
		goto fail;
	}

	msdu_len = hal_rx_msdu_start_msdu_len_get(soc->hal_soc,
						  qdf_nbuf_data(nbuf));

	qdf_nbuf_set_pktlen(nbuf, (msdu_len + soc->rx_pkt_tlv_size));

	status = dp_rx_defrag_fraglist_insert(txrx_peer, tid,
					      &rx_reorder_array_elem->head,
			&rx_reorder_array_elem->tail, nbuf,
			&all_frag_present);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s Fragment insert failed", __func__);

		goto fail;
	}

	if (soc->rx.flags.defrag_timeout_check)
		dp_rx_defrag_waitlist_remove(txrx_peer, tid);

	if (!all_frag_present) {
		uint32_t now_ms =
			qdf_system_ticks_to_msecs(qdf_system_ticks());

		txrx_peer->rx_tid[tid].defrag_timeout_ms =
			now_ms + soc->rx.defrag.timeout_ms;

		dp_rx_defrag_waitlist_add(txrx_peer, tid);

		return QDF_STATUS_SUCCESS;
	}

	status = dp_rx_defrag(txrx_peer, tid, rx_reorder_array_elem->head,
			      rx_reorder_array_elem->tail);

	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s Fragment processing failed", __func__);

		dp_rx_return_head_frag_desc(txrx_peer, tid);
		dp_rx_defrag_cleanup(txrx_peer, tid);

		goto fail;
	}

	/* Re-inject the fragments back to REO for further processing */
	status = dp_rx_defrag_reo_reinject(txrx_peer, tid,
					   rx_reorder_array_elem->head);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		rx_reorder_array_elem->head = NULL;
		rx_reorder_array_elem->tail = NULL;
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
			  "%s: Frag seq successfully reinjected",
			__func__);
	} else {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Frag seq reinjection failed", __func__);
		dp_rx_return_head_frag_desc(txrx_peer, tid);
	}

	dp_rx_defrag_cleanup(txrx_peer, tid);
	return QDF_STATUS_SUCCESS;

fail:
	return QDF_STATUS_E_DEFRAG_ERROR;
}

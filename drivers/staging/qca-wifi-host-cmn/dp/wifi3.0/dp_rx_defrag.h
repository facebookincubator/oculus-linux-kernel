/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _DP_RX_DEFRAG_H
#define _DP_RX_DEFRAG_H

#include "hal_rx.h"

#define DEFRAG_IEEE80211_KEY_LEN	8
#define DEFRAG_IEEE80211_FCS_LEN	4

#define DP_RX_DEFRAG_IEEE80211_ADDR_COPY(dst, src) \
	qdf_mem_copy(dst, src, QDF_MAC_ADDR_SIZE)

#define DP_RX_DEFRAG_IEEE80211_QOS_HAS_SEQ(wh) \
	(((wh) & \
	(IEEE80211_FC0_TYPE_MASK | QDF_IEEE80211_FC0_SUBTYPE_QOS)) == \
	(IEEE80211_FC0_TYPE_DATA | QDF_IEEE80211_FC0_SUBTYPE_QOS))

#define UNI_DESC_OWNER_SW 0x1
#define UNI_DESC_BUF_TYPE_RX_MSDU_LINK 0x6
/**
 * struct dp_rx_defrag_cipher: structure to indicate cipher header
 * @ic_name: Name
 * @ic_header: header length
 * @ic_trailer: trail length
 * @ic_miclen: MIC length
 */
struct dp_rx_defrag_cipher {
	const char *ic_name;
	uint16_t ic_header;
	uint8_t ic_trailer;
	uint8_t ic_miclen;
};

#ifndef WLAN_SOFTUMAC_SUPPORT /* WLAN_SOFTUMAC_SUPPORT */
/**
 * dp_rx_frag_handle() - Handles fragmented Rx frames
 *
 * @soc: core txrx main context
 * @ring_desc: opaque pointer to the REO error ring descriptor
 * @mpdu_desc_info: MPDU descriptor information from ring descriptor
 * @rx_desc:
 * @mac_id:
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
uint32_t dp_rx_frag_handle(struct dp_soc *soc, hal_ring_desc_t  ring_desc,
			   struct hal_rx_mpdu_desc_info *mpdu_desc_info,
			   struct dp_rx_desc *rx_desc,
			   uint8_t *mac_id,
			   uint32_t quota);
#endif /* WLAN_SOFTUMAC_SUPPORT */

/**
 * dp_rx_frag_get_mac_hdr() - Return pointer to the mac hdr
 * @soc: DP SOC
 * @rx_desc_info: Pointer to the pkt_tlvs in the
 * nbuf (pkt_tlvs->mac_hdr->data)
 *
 * It is inefficient to peek into the packet for received
 * frames but these APIs are required to get to some of
 * 802.11 fields that hardware does not populate in the
 * rx meta data.
 *
 * Return: pointer to ieee80211_frame
 */
static inline struct ieee80211_frame *
dp_rx_frag_get_mac_hdr(struct dp_soc *soc, uint8_t *rx_desc_info)
{
	int rx_desc_len = soc->rx_pkt_tlv_size;
	return (struct ieee80211_frame *)(rx_desc_info + rx_desc_len);
}

/**
 * dp_rx_frag_get_mpdu_seq_number() - Get mpdu sequence number
 * @soc: DP SOC
 * @rx_desc_info: Pointer to the pkt_tlvs in the
 * nbuf (pkt_tlvs->mac_hdr->data)
 *
 * Return: uint16_t, rx sequence number
 */
static inline uint16_t
dp_rx_frag_get_mpdu_seq_number(struct dp_soc *soc, int8_t *rx_desc_info)
{
	struct ieee80211_frame *mac_hdr;
	mac_hdr = dp_rx_frag_get_mac_hdr(soc, rx_desc_info);

	return qdf_le16_to_cpu(*(uint16_t *) mac_hdr->i_seq) >>
		IEEE80211_SEQ_SEQ_SHIFT;
}

/**
 * dp_rx_frag_get_mpdu_frag_number() - Get mpdu fragment number
 * @soc: DP SOC
 * @rx_desc_info: Pointer to the pkt_tlvs in the
 * nbuf (pkt_tlvs->mac_hdr->data)
 *
 * Return: uint8_t, receive fragment number
 */
static inline uint8_t
dp_rx_frag_get_mpdu_frag_number(struct dp_soc *soc, uint8_t *rx_desc_info)
{
	struct ieee80211_frame *mac_hdr;
	mac_hdr = dp_rx_frag_get_mac_hdr(soc, rx_desc_info);

	return qdf_le16_to_cpu(*(uint16_t *) mac_hdr->i_seq) &
		IEEE80211_SEQ_FRAG_MASK;
}

/**
 * dp_rx_frag_get_more_frag_bit() - Get more fragment bit
 * @soc: DP SOC
 * @rx_desc_info: Pointer to the pkt_tlvs in the
 * nbuf (pkt_tlvs->mac_hdr->data)
 *
 * Return: uint8_t, get more fragment bit
 */
static inline
uint8_t dp_rx_frag_get_more_frag_bit(struct dp_soc *soc, uint8_t *rx_desc_info)
{
	struct ieee80211_frame *mac_hdr;
	mac_hdr = dp_rx_frag_get_mac_hdr(soc, rx_desc_info);

	return (mac_hdr->i_fc[1] & IEEE80211_FC1_MORE_FRAG) >> 2;
}

static inline
uint8_t dp_rx_get_pkt_dir(struct dp_soc *soc, uint8_t *rx_desc_info)
{
	struct ieee80211_frame *mac_hdr;
	mac_hdr = dp_rx_frag_get_mac_hdr(soc, rx_desc_info);

	return mac_hdr->i_fc[1] & IEEE80211_FC1_DIR_MASK;
}

/**
 * dp_rx_defrag_fraglist_insert() - Create a per-sequence fragment list
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 * @head_addr: Pointer to head list
 * @tail_addr: Pointer to tail list
 * @frag: Incoming fragment
 * @all_frag_present: Flag to indicate whether all fragments are received
 *
 * Build a per-tid, per-sequence fragment list.
 *
 * Return: Success, if inserted
 */
QDF_STATUS
dp_rx_defrag_fraglist_insert(struct dp_txrx_peer *txrx_peer, unsigned int tid,
			     qdf_nbuf_t *head_addr, qdf_nbuf_t *tail_addr,
			     qdf_nbuf_t frag, uint8_t *all_frag_present);

/**
 * dp_rx_defrag_waitlist_add() - Update per-PDEV defrag wait list
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Appends per-tid fragments to global fragment wait list
 *
 * Return: None
 */
void dp_rx_defrag_waitlist_add(struct dp_txrx_peer *txrx_peer,
			       unsigned int tid);

/**
 * dp_rx_defrag() - Defragment the fragment chain
 * @txrx_peer: Pointer to the peer
 * @tid: Transmit Identifier
 * @frag_list_head: Pointer to head list
 * @frag_list_tail: Pointer to tail list
 *
 * Defragment the fragment chain
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_defrag(struct dp_txrx_peer *txrx_peer, unsigned int tid,
			qdf_nbuf_t frag_list_head,
			qdf_nbuf_t frag_list_tail);

/**
 * dp_rx_defrag_waitlist_flush() - Flush SOC defrag wait list
 * @soc: DP SOC
 *
 * Flush fragments of all waitlisted TID's
 *
 * Return: None
 */
void dp_rx_defrag_waitlist_flush(struct dp_soc *soc);

/**
 * dp_rx_reorder_flush_frag() - Flush the frag list
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Flush the per-TID frag list
 *
 * Return: None
 */
void dp_rx_reorder_flush_frag(struct dp_txrx_peer *txrx_peer,
			      unsigned int tid);

/**
 * dp_rx_defrag_waitlist_remove() - Remove fragments from waitlist
 * @txrx_peer: Pointer to the peer data structure
 * @tid: Transmit ID (TID)
 *
 * Remove fragments from waitlist
 *
 * Return: None
 */
void dp_rx_defrag_waitlist_remove(struct dp_txrx_peer *txrx_peer,
				  unsigned int tid);

/**
 * dp_rx_defrag_cleanup() - Clean up activities
 * @txrx_peer: Pointer to the peer
 * @tid: Transmit Identifier
 *
 * Return: None
 */
void dp_rx_defrag_cleanup(struct dp_txrx_peer *txrx_peer, unsigned int tid);

QDF_STATUS dp_rx_defrag_add_last_frag(struct dp_soc *soc,
				      struct dp_txrx_peer *peer, uint16_t tid,
				      uint16_t rxseq, qdf_nbuf_t nbuf);
#endif /* _DP_RX_DEFRAG_H */

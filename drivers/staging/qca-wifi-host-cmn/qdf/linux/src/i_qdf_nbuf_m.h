/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: i_qdf_nbuf_m.h
 *
 * This file provides platform specific nbuf API's.
 * Included by i_qdf_nbuf.h and should not be included
 * directly from other files.
 */

#ifndef _I_QDF_NBUF_M_H
#define _I_QDF_NBUF_M_H
/**
 * struct qdf_nbuf_cb - network buffer control block contents (skb->cb)
 *                    - data passed between layers of the driver.
 *
 * Notes:
 *   1. Hard limited to 48 bytes. Please count your bytes
 *   2. The size of this structure has to be easily calculable and
 *      consistently so: do not use any conditional compile flags
 *   3. Split into a common part followed by a tx/rx overlay
 *   4. There is only one extra frag, which represents the HTC/HTT header
 *   5. "ext_cb_pt" must be the first member in both TX and RX unions
 *      for the priv_cb_w since it must be at same offset for both
 *      TX and RX union
 *   6. "ipa.owned" bit must be first member in both TX and RX unions
 *      for the priv_cb_m since it must be at same offset for both
 *      TX and RX union.
 *
 * @paddr   : physical addressed retrieved by dma_map of nbuf->data
 * @u: union of rx and tx data
 * @u.rx: rx data
 * @u.rx.dev: union of priv_cb_w and priv_cb_m
 *
 * @u.rx.dev.priv_cb_w:
 * @u.rx.dev.priv_cb_w.ext_cb_ptr: extended cb pointer
 * @u.rx.dev.priv_cb_w.fctx: ctx to handle special pkts defined by ftype
 * @u.rx.dev.priv_cb_w.msdu_len: length of RX packet
 * @u.rx.dev.priv_cb_w.flag_intra_bss: flag to indicate this is intra bss packet
 * @u.rx.dev.priv_cb_w.ipa_smmu_map: do IPA smmu map
 * @u.rx.dev.priv_cb_w.peer_id: peer_id for RX packet
 * @u.rx.dev.priv_cb_w.protocol_tag: protocol tag set by app for rcvd packet
 *                                   type
 * @u.rx.dev.priv_cb_w.flow_tag: flow tag set by application for 5 tuples rcvd
 *
 * @u.rx.dev.priv_cb_m:
 * @u.rx.dev.priv_cb_m.ipa.owned: packet owned by IPA
 * @u.rx.dev.priv_cb_m.peer_cached_buf_frm: peer cached buffer
 * @u.rx.dev.priv_cb_m.flush_ind: flush indication
 * @u.rx.dev.priv_cb_m.packet_buf_pool:  packet buff bool
 * @u.rx.dev.priv_cb_m.l3_hdr_pad: L3 header padding offset
 * @u.rx.dev.priv_cb_m.exc_frm: exception frame
 * @u.rx.dev.priv_cb_m.ipa_smmu_map: do IPA smmu map
 * @u.rx.dev.priv_cb_m.reo_dest_ind_or_sw_excpt: reo destination indication or
 *					     sw exception bit from ring desc
 * @u.rx.dev.priv_cb_m.lmac_id: lmac id for RX packet
 * @u.rx.dev.priv_cb_m.fr_ds: from DS bit in RX packet
 * @u.rx.dev.priv_cb_m.to_ds: to DS bit in RX packet
 * @u.rx.dev.priv_cb_m.logical_link_id: link id of RX packet
 * @u.rx.dev.priv_cb_m.reserved1: reserved bits
 * @u.rx.dev.priv_cb_m.dp_ext: Union of tcp and ext structs
 * @u.rx.dev.priv_cb_m.dp_ext.tcp: TCP structs
 * @u.rx.dev.priv_cb_m.dp_ext.tcp.tcp_seq_num: TCP sequence number
 * @u.rx.dev.priv_cb_m.dp_ext.tcp.tcp_ack_num: TCP ACK number
 * @u.rx.dev.priv_cb_m.dp_ext.ext: Extension struct for other usage
 * @u.rx.dev.priv_cb_m.dp_ext.ext.mpdu_seq: wifi MPDU sequence number
 * @u.rx.dev.priv_cb_m.dp: Union of wifi3 and wifi2 structs
 * @u.rx.dev.priv_cb_m.dp.wifi3: wifi3 data
 * @u.rx.dev.priv_cb_m.dp.wifi3.msdu_len: length of RX packet
 * @u.rx.dev.priv_cb_m.dp.wifi3.peer_id:  peer_id for RX packet
 * @u.rx.dev.priv_cb_m.dp.wifi2: wifi2 data
 * @u.rx.dev.priv_cb_m.dp.wifi2.map_index:
 * @u.rx.dev.priv_cb_m.lro_ctx: LRO context
 *
 * @u.rx.lro_eligible: flag to indicate whether the MSDU is LRO eligible
 * @u.rx.tcp_proto: L4 protocol is TCP
 * @u.rx.tcp_pure_ack: A TCP ACK packet with no payload
 * @u.rx.ipv6_proto: L3 protocol is IPV6
 * @u.rx.ip_offset: offset to IP header
 * @u.rx.tcp_offset: offset to TCP header
 * @u.rx.rx_ctx_id: Rx context id
 * @u.rx.fcs_err: FCS error
 * @u.rx.is_raw_frame: RAW frame
 * @u.rx.num_elements_in_list: number of elements in the nbuf list
 *
 * @u.rx.tcp_udp_chksum: L4 payload checksum
 * @u.rx.tcp_win: TCP window size
 *
 * @u.rx.flow_id: 32bit flow id
 *
 * @u.rx.flag_chfrag_start: first MSDU in an AMSDU
 * @u.rx.flag_chfrag_cont: middle or part of MSDU in an AMSDU
 * @u.rx.flag_chfrag_end: last MSDU in an AMSDU
 * @u.rx.flag_retry: flag to indicate MSDU is retried
 * @u.rx.flag_da_mcbc: flag to indicate mulicast or broadcast packets
 * @u.rx.flag_da_valid: flag to indicate DA is valid for RX packet
 * @u.rx.flag_sa_valid: flag to indicate SA is valid for RX packet
 * @u.rx.flag_is_frag: flag to indicate skb has frag list
 *
 * @u.rx.trace: combined structure for DP and protocol trace
 * @u.rx.trace.packet_state: {NBUF_TX_PKT_[(HDD)|(TXRX_ENQUEUE)|(TXRX_DEQUEUE)|
 *                       +          (TXRX)|(HTT)|(HTC)|(HIF)|(CE)|(FREE)]
 * @u.rx.trace.dp_trace: flag (Datapath trace)
 * @u.rx.trace.packet_track: RX_DATA packet
 * @u.rx.trace.rsrvd: enable packet logging
 *
 * @u.rx.vdev_id: vdev_id for RX pkt
 * @u.rx.tid_val: tid value
 * @u.rx.ftype: mcast2ucast, TSO, SG, MESH
 *
 * @u.tx: tx data
 * @u.tx.dev: union of priv_cb_w and priv_cb_m
 *
 * @u.tx.dev.priv_cb_w:
 * @u.tx.dev.priv_cb_w.ext_cb_ptr: extended cb pointer
 * @u.tx.dev.priv_cb_w.fctx: ctx to handle special pkts defined by ftype
 *
 * @u.tx.dev.priv_cb_m:
 * @u.tx.dev.priv_cb_m:ipa: IPA-specific data
 * @u.tx.dev.priv_cb_m.ipa.ipa.owned: packet owned by IPA
 * @u.tx.dev.priv_cb_m.ipa.ipa.priv: private data, used by IPA
 * @u.tx.dev.priv_cb_m.data_attr: value that is programmed in CE descr, includes
 *                 + (1) CE classification enablement bit
 *                 + (2) packet type (802.3 or Ethernet type II)
 *                 + (3) packet offset (usually length of HTC/HTT descr)
 * @u.tx.dev.priv_cb_m.desc_id: tx desc id, used to sync between host and fw
 * @u.tx.dev.priv_cb_m.dma_option: DMA options
 * @u.tx.dev.priv_cb_m.dma_option.mgmt_desc_id: mgmt descriptor for tx
 *                                              completion cb
 * @u.tx.dev.priv_cb_m.dma_option.dma_option.bi_map: flag to do bi-direction
 *                                                   dma map
 * @u.tx.dev.priv_cb_m.dma_option.dma_option.reserved: reserved bits for future
 *                                                     use
 * @u.tx.dev.priv_cb_m.flag_notify_comp: reserved
 * @u.tx.dev.priv_cb_m.flag_ts_valid: flag to indicate field
 * u.tx.pa_ts.ts_value is available, it must be cleared before fragment mapping
 * @u.tx.dev.priv_cb_m.rsvd: reserved
 * @u.tx.dev.priv_cb_m.reserved: reserved
 *
 * @u.tx.ftype: mcast2ucast, TSO, SG, MESH
 * @u.tx.vdev_id: vdev (for protocol trace)
 * @u.tx.len: length of efrag pointed by the above pointers
 *
 * @u.tx.flags: union of flag representations
 * @u.tx.flags.bits: flags represent as individual bitmasks
 * @u.tx.flags.bits.flag_efrag: flag, efrag payload to be swapped (wordstream)
 * @u.tx.flags.bits.num: number of extra frags ( 0 or 1)
 * @u.tx.flags.bits.nbuf: flag, nbuf payload to be swapped (wordstream)
 * @u.tx.flags.bits.flag_chfrag_start: first MSDU in an AMSDU
 * @u.tx.flags.bits.flag_chfrag_cont: middle or part of MSDU in an AMSDU
 * @u.tx.flags.bits.flag_chfrag_end: last MSDU in an AMSDU
 * @u.tx.flags.bits.flag_ext_header: extended flags
 * @u.tx.flags.bits.is_critical: flag indicating a critical frame
 * @u.tx.flags.u8: flags as a single u8
 * @u.tx.trace: combined structure for DP and protocol trace
 * @u.tx.trace.packet_stat: {NBUF_TX_PKT_[(HDD)|(TXRX_ENQUEUE)|(TXRX_DEQUEUE)|
 *                       +          (TXRX)|(HTT)|(HTC)|(HIF)|(CE)|(FREE)]
 * @u.tx.trace.is_packet_priv:
 * @u.tx.trace.packet_track: {NBUF_TX_PKT_[(DATA)|(MGMT)]_TRACK}
 * @u.tx.trace.to_fw: Flag to indicate send this packet to FW
 * @u.tx.trace.htt2_frm: flag (high-latency path only)
 * @u.tx.trace.proto_type: bitmap of NBUF_PKT_TRAC_TYPE[(EAPOL)|(DHCP)|
 *                          + (MGMT_ACTION)] - 4 bits
 * @u.tx.trace.dp_trace: flag (Datapath trace)
 * @u.tx.trace.is_bcast: flag (Broadcast packet)
 * @u.tx.trace.is_mcast: flag (Multicast packet)
 * @u.tx.trace.packet_type: flag (Packet type)
 * @u.tx.trace.print: enable packet logging
 *
 * @u.tx.vaddr: virtual address of ~
 * @u.tx.pa_ts.paddr: physical/DMA address of ~
 * @u.tx.pa_ts.ts_value: driver ingress timestamp, it must be cleared before
 * fragment mapping
 */
struct qdf_nbuf_cb {
	/* common */
	qdf_paddr_t paddr; /* of skb->data */
	/* valid only in one direction */
	union {
		/* Note: MAX: 40 bytes */
		struct {
			union {
				struct {
					void *ext_cb_ptr;
					void *fctx;
					uint16_t msdu_len : 14,
						 flag_intra_bss : 1,
						 ipa_smmu_map : 1;
					uint16_t peer_id;
					uint16_t protocol_tag;
					uint16_t flow_tag;
				} priv_cb_w;
				struct {
					/* ipa_owned bit is common between rx
					 * control block and tx control block.
					 * Do not change location of this bit.
					 */
					uint32_t ipa_owned:1,
						 peer_cached_buf_frm:1,
						 flush_ind:1,
						 packet_buf_pool:1,
						 l3_hdr_pad:3,
						 /* exception frame flag */
						 exc_frm:1,
						 ipa_smmu_map:1,
						 reo_dest_ind_or_sw_excpt:5,
						 lmac_id:2,
						 fr_ds:1,
						 to_ds:1,
						 logical_link_id:4,
						 band:3,
						 reserved1:7;
					union {
						struct {
							uint32_t tcp_seq_num;
							uint32_t tcp_ack_num;
						} tcp;
						struct {
							uint32_t mpdu_seq:12,
								 reserved:20;
							uint32_t reserved1;
						} ext;
					} dp_ext;
					union {
						struct {
							uint16_t msdu_len;
							uint16_t peer_id;
						} wifi3;
						struct {
							uint32_t map_index;
						} wifi2;
					} dp;
					unsigned char *lro_ctx;
				} priv_cb_m;
			} dev;
			uint32_t lro_eligible:1,
				tcp_proto:1,
				tcp_pure_ack:1,
				ipv6_proto:1,
				ip_offset:7,
				tcp_offset:7,
				rx_ctx_id:4,
				fcs_err:1,
				is_raw_frame:1,
				num_elements_in_list:8;
			uint32_t tcp_udp_chksum:16,
				 tcp_win:16;
			uint32_t flow_id;
			uint8_t flag_chfrag_start:1,
				flag_chfrag_cont:1,
				flag_chfrag_end:1,
				flag_retry:1,
				flag_da_mcbc:1,
				flag_da_valid:1,
				flag_sa_valid:1,
				flag_is_frag:1;
			union {
				uint8_t packet_state;
				uint8_t dp_trace:1,
					packet_track:3,
					rsrvd:4;
			} trace;
			uint16_t vdev_id:8,
				 tid_val:4,
				 ftype:4;
		} rx;

		/* Note: MAX: 40 bytes */
		struct {
			union {
				struct {
					void *ext_cb_ptr;
					void *fctx;
				} priv_cb_w;
				struct {
					/* ipa_owned bit is common between rx
					 * control block and tx control block.
					 * Do not change location of this bit.
					 */
					struct {
						uint32_t owned:1,
							priv:31;
					} ipa;
					uint32_t data_attr;
					uint16_t desc_id;
					uint16_t mgmt_desc_id;
					struct {
						uint8_t bi_map:1,
							reserved:7;
					} dma_option;
					uint8_t flag_notify_comp:1,
						band:3,
						flag_ts_valid:1,
						rsvd:3;
					uint8_t reserved[2];
				} priv_cb_m;
			} dev;
			uint8_t ftype;
			uint8_t vdev_id;
			uint16_t len;
			union {
				struct {
					uint8_t flag_efrag:1,
						flag_nbuf:1,
						num:1,
						flag_chfrag_start:1,
						flag_chfrag_cont:1,
						flag_chfrag_end:1,
						flag_ext_header:1,
						is_critical:1;
				} bits;
				uint8_t u8;
			} flags;
			struct {
				uint8_t packet_state:7,
					is_packet_priv:1;
				uint8_t packet_track:3,
					to_fw:1,
					htt2_frm:1,
					proto_type:3;
				uint8_t dp_trace:1,
					is_bcast:1,
					is_mcast:1,
					packet_type:4,
					print:1;
			} trace;
			unsigned char *vaddr;
			union {
				qdf_paddr_t paddr;
				qdf_ktime_t ts_value;
			} pa_ts;
		} tx;
	} u;
}; /* struct qdf_nbuf_cb: MAX 48 bytes */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0))
QDF_COMPILE_TIME_ASSERT(qdf_nbuf_cb_size,
			(sizeof(struct qdf_nbuf_cb)) <=
			sizeof_field(struct sk_buff, cb));
#else
QDF_COMPILE_TIME_ASSERT(qdf_nbuf_cb_size,
			(sizeof(struct qdf_nbuf_cb)) <=
			FIELD_SIZEOF(struct sk_buff, cb));
#endif

/*
 *  access macros to qdf_nbuf_cb
 *  Note: These macros can be used as L-values as well as R-values.
 *        When used as R-values, they effectively function as "get" macros
 *        When used as L_values, they effectively function as "set" macros
 */

#define QDF_NBUF_CB_PADDR(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->paddr.dma_addr)

#define QDF_NBUF_CB_RX_LRO_ELIGIBLE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.lro_eligible)
#define QDF_NBUF_CB_RX_TCP_PROTO(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.tcp_proto)
#define QDF_NBUF_CB_RX_TCP_PURE_ACK(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.tcp_pure_ack)
#define QDF_NBUF_CB_RX_IPV6_PROTO(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.ipv6_proto)
#define QDF_NBUF_CB_RX_IP_OFFSET(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.ip_offset)
#define QDF_NBUF_CB_RX_TCP_OFFSET(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.tcp_offset)
#define QDF_NBUF_CB_RX_CTX_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.rx_ctx_id)
#define QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(skb) \
		(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.num_elements_in_list)

#define QDF_NBUF_CB_RX_TCP_CHKSUM(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.tcp_udp_chksum)
#define QDF_NBUF_CB_RX_TCP_WIN(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.tcp_win)

#define QDF_NBUF_CB_RX_FLOW_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.flow_id)

#define QDF_NBUF_CB_RX_PACKET_STATE(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.trace.packet_state)
#define QDF_NBUF_CB_RX_DP_TRACE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.trace.dp_trace)

#define QDF_NBUF_CB_RX_FTYPE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.ftype)

#define QDF_NBUF_CB_RX_VDEV_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.vdev_id)

#define QDF_NBUF_CB_RX_CHFRAG_START(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_chfrag_start)
#define QDF_NBUF_CB_RX_CHFRAG_CONT(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_chfrag_cont)
#define QDF_NBUF_CB_RX_CHFRAG_END(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.rx.flag_chfrag_end)

#define QDF_NBUF_CB_RX_DA_MCBC(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_da_mcbc)

#define QDF_NBUF_CB_RX_DA_VALID(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_da_valid)

#define QDF_NBUF_CB_RX_SA_VALID(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_sa_valid)

#define QDF_NBUF_CB_RX_RETRY_FLAG(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_retry)

#define QDF_NBUF_CB_RX_RAW_FRAME(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.is_raw_frame)

#define QDF_NBUF_CB_RX_FROM_DS(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.dev.priv_cb_m.fr_ds)

#define QDF_NBUF_CB_RX_TO_DS(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.dev.priv_cb_m.to_ds)

#define QDF_NBUF_CB_RX_TID_VAL(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.tid_val)

#define QDF_NBUF_CB_RX_IS_FRAG(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.flag_is_frag)

#define QDF_NBUF_CB_RX_FCS_ERR(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.fcs_err)

#define QDF_NBUF_UPDATE_TX_PKT_COUNT(skb, PACKET_STATE) \
	qdf_nbuf_set_state(skb, PACKET_STATE)

#define QDF_NBUF_CB_TX_DATA_ATTR(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.data_attr)

#define QDF_NBUF_CB_TX_FTYPE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.ftype)

#define QDF_NBUF_CB_TX_EXTRA_FRAG_LEN(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.len)
#define QDF_NBUF_CB_TX_VDEV_CTX(skb) \
		(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.vdev_id)

/* Tx Flags Accessor Macros*/
#define QDF_NBUF_CB_TX_EXTRA_FRAG_WORDSTR_EFRAG(skb) \
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.flags.bits.flag_efrag)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_WORDSTR_NBUF(skb) \
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.flags.bits.flag_nbuf)
#define QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.flags.bits.num)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_CHFRAG_START(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.tx.flags.bits.flag_chfrag_start)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_CHFRAG_CONT(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.tx.flags.bits.flag_chfrag_cont)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_CHFRAG_END(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.flags.bits.flag_chfrag_end)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_EXT_HEADER(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.flags.bits.flag_ext_header)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_WORDSTR_FLAGS(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.flags.u8)

#define QDF_NBUF_CB_TX_EXTRA_IS_CRITICAL(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.flags.bits.is_critical)
/* End of Tx Flags Accessor Macros */

/* Tx trace accessor macros */
#define QDF_NBUF_CB_TX_PACKET_STATE(skb)\
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.packet_state)

#define QDF_NBUF_CB_TX_IS_PACKET_PRIV(skb) \
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.is_packet_priv)

#define QDF_NBUF_CB_TX_PACKET_TRACK(skb)\
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.packet_track)

#define QDF_NBUF_CB_TX_PACKET_TO_FW(skb)\
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.to_fw)

#define QDF_NBUF_CB_RX_PACKET_TRACK(skb)\
		(((struct qdf_nbuf_cb *) \
			((skb)->cb))->u.rx.trace.packet_track)

#define QDF_NBUF_CB_TX_PROTO_TYPE(skb)\
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.proto_type)

#define QDF_NBUF_CB_TX_DP_TRACE(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.trace.dp_trace)

#define QDF_NBUF_CB_DP_TRACE_PRINT(skb)	\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.trace.print)

#define QDF_NBUF_CB_TX_HL_HTT2_FRM(skb)	\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.trace.htt2_frm)

#define QDF_NBUF_CB_GET_IS_BCAST(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.trace.is_bcast)

#define QDF_NBUF_CB_GET_IS_MCAST(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.trace.is_mcast)

#define QDF_NBUF_CB_GET_PACKET_TYPE(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.trace.packet_type)

#define QDF_NBUF_CB_SET_BCAST(skb) \
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.is_bcast = true)

#define QDF_NBUF_CB_SET_MCAST(skb) \
	(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.tx.trace.is_mcast = true)
/* End of Tx trace accessor macros */

#define QDF_NBUF_CB_TX_EXTRA_FRAG_VADDR(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.vaddr)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_PADDR(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.pa_ts.paddr.dma_addr)

#define QDF_NBUF_CB_TX_TS_VALID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.flag_ts_valid)
#define QDF_NBUF_CB_TX_TS_VALUE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.pa_ts.ts_value)

/* assume the OS provides a single fragment */
#define __qdf_nbuf_get_num_frags(skb)		   \
	(QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb) + 1)

#define __qdf_nbuf_reset_num_frags(skb) \
	(QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb) = 0)

#define QDF_NBUF_CB_RX_TCP_SEQ_NUM(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 dp_ext.tcp.tcp_seq_num)
#define QDF_NBUF_CB_RX_TCP_ACK_NUM(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 dp_ext.tcp.tcp_ack_num)
#define QDF_NBUF_CB_RX_MPDU_SEQ_NUM(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 dp_ext.ext.mpdu_seq)

#define QDF_NBUF_CB_RX_LRO_CTX(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m.lro_ctx)

#define QDF_NBUF_CB_TX_IPA_OWNED(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.ipa.owned)
#define QDF_NBUF_CB_TX_IPA_PRIV(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.ipa.priv)
#define QDF_NBUF_CB_TX_DESC_ID(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.desc_id)
#define QDF_NBUF_CB_MGMT_TXRX_DESC_ID(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.mgmt_desc_id)
#define QDF_NBUF_CB_TX_DMA_BI_MAP(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m. \
	dma_option.bi_map)
#define QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m. \
	flag_notify_comp)

#define QDF_NBUF_CB_TX_BAND(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m. \
	band)

#define QDF_NBUF_CB_RX_PEER_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m.dp. \
	wifi3.peer_id)

#define QDF_NBUF_CB_RX_PKT_LEN(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m.dp. \
	wifi3.msdu_len)

#define QDF_NBUF_CB_RX_MAP_IDX(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m.dp. \
	wifi2.map_index)

#define  QDF_NBUF_CB_RX_PEER_CACHED_FRM(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 peer_cached_buf_frm)

#define  QDF_NBUF_CB_RX_FLUSH_IND(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m.flush_ind)

#define  QDF_NBUF_CB_RX_PACKET_BUFF_POOL(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 packet_buf_pool)

#define  QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 l3_hdr_pad)

#define  QDF_NBUF_CB_RX_PACKET_EXC_FRAME(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 exc_frm)

#define  QDF_NBUF_CB_RX_PACKET_IPA_SMMU_MAP(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	 ipa_smmu_map)

#define  QDF_NBUF_CB_RX_PACKET_REO_DEST_IND_OR_SW_EXCPT(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	reo_dest_ind_or_sw_excpt)

#define  QDF_NBUF_CB_RX_PACKET_LMAC_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	lmac_id)

#define QDF_NBUF_CB_RX_LOGICAL_LINK_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	logical_link_id)

#define QDF_NBUF_CB_RX_BAND(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.dev.priv_cb_m. \
	band)

#define __qdf_nbuf_ipa_owned_get(skb) \
	QDF_NBUF_CB_TX_IPA_OWNED(skb)

#define __qdf_nbuf_ipa_owned_set(skb) \
	(QDF_NBUF_CB_TX_IPA_OWNED(skb) = 1)

#define __qdf_nbuf_ipa_owned_clear(skb) \
	(QDF_NBUF_CB_TX_IPA_OWNED(skb) = 0)

#define __qdf_nbuf_ipa_priv_get(skb)	\
	QDF_NBUF_CB_TX_IPA_PRIV(skb)

#define __qdf_nbuf_ipa_priv_set(skb, priv) \
	(QDF_NBUF_CB_TX_IPA_PRIV(skb) = (priv))

#define QDF_NBUF_CB_TX_DATA_ATTR(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.dev.priv_cb_m.data_attr)

#define __qdf_nbuf_data_attr_get(skb)		\
	QDF_NBUF_CB_TX_DATA_ATTR(skb)
#define __qdf_nbuf_data_attr_set(skb, data_attr) \
	(QDF_NBUF_CB_TX_DATA_ATTR(skb) = (data_attr))

#define __qdf_nbuf_set_tx_ts(skb, ts) \
	do { \
		QDF_NBUF_CB_TX_TS_VALUE(skb) = (ts); \
		QDF_NBUF_CB_TX_TS_VALID(skb) = 1; \
	} while (0)

#define __qdf_nbuf_clear_tx_ts(skb) \
	do { \
		QDF_NBUF_CB_TX_TS_VALUE(skb) = 0; \
		QDF_NBUF_CB_TX_TS_VALID(skb) = 0; \
	} while (0)

#define __qdf_nbuf_get_tx_ts(skb) \
	(QDF_NBUF_CB_TX_TS_VALID(skb) ? \
	 QDF_NBUF_CB_TX_TS_VALUE(skb) : 0)

/**
 * __qdf_nbuf_map_nbytes_single() - map nbytes
 * @osdev: os device
 * @buf: buffer
 * @dir: direction
 * @nbytes: number of bytes
 *
 * Return: QDF_STATUS
 */
#ifdef A_SIMOS_DEVHOST
static inline QDF_STATUS __qdf_nbuf_map_nbytes_single(
		qdf_device_t osdev, struct sk_buff *buf,
		qdf_dma_dir_t dir, int nbytes)
{
	qdf_dma_addr_t paddr;

	QDF_NBUF_CB_PADDR(buf) = paddr = buf->data;
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS __qdf_nbuf_map_nbytes_single(
		qdf_device_t osdev, struct sk_buff *buf,
		qdf_dma_dir_t dir, int nbytes)
{
	qdf_dma_addr_t paddr;
	QDF_STATUS ret;

	/* assume that the OS only provides a single fragment */
	QDF_NBUF_CB_PADDR(buf) = paddr =
		dma_map_single(osdev->dev, buf->data,
			       nbytes, __qdf_dma_dir_to_os(dir));
	ret =  dma_mapping_error(osdev->dev, paddr) ?
		QDF_STATUS_E_FAULT : QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(ret))
		__qdf_record_nbuf_nbytes(__qdf_nbuf_get_end_offset(buf),
					 dir, true);
	return ret;
}
#endif
/**
 * __qdf_nbuf_unmap_nbytes_single() - unmap nbytes
 * @osdev: os device
 * @buf: buffer
 * @dir: direction
 * @nbytes: number of bytes
 *
 * Return: none
 */
#if defined(A_SIMOS_DEVHOST)
static inline void
__qdf_nbuf_unmap_nbytes_single(qdf_device_t osdev, struct sk_buff *buf,
			       qdf_dma_dir_t dir, int nbytes)
{
}

#else
static inline void
__qdf_nbuf_unmap_nbytes_single(qdf_device_t osdev, struct sk_buff *buf,
			       qdf_dma_dir_t dir, int nbytes)
{
	qdf_dma_addr_t paddr = QDF_NBUF_CB_PADDR(buf);

	if (qdf_likely(paddr)) {
		__qdf_record_nbuf_nbytes(
			__qdf_nbuf_get_end_offset(buf), dir, false);
		dma_unmap_single(osdev->dev, paddr, nbytes,
				 __qdf_dma_dir_to_os(dir));
		return;
	}
}
#endif

/**
 * __qdf_nbuf_reset() - reset the buffer data and pointer
 * @skb: Network buf instance
 * @reserve: reserve
 * @align: align
 *
 * Return: none
 */
static inline void
__qdf_nbuf_reset(struct sk_buff *skb, int reserve, int align)
{
	int offset;

	skb_push(skb, skb_headroom(skb));
	skb_put(skb, skb_tailroom(skb));
	memset(skb->data, 0x0, skb->len);
	skb_trim(skb, 0);
	skb_reserve(skb, NET_SKB_PAD);
	memset(skb->cb, 0x0, sizeof(skb->cb));

	/*
	 * The default is for netbuf fragments to be interpreted
	 * as wordstreams rather than bytestreams.
	 */
	QDF_NBUF_CB_TX_EXTRA_FRAG_WORDSTR_EFRAG(skb) = 1;
	QDF_NBUF_CB_TX_EXTRA_FRAG_WORDSTR_NBUF(skb) = 1;

	/*
	 * Align & make sure that the tail & data are adjusted properly
	 */

	if (align) {
		offset = ((unsigned long)skb->data) % align;
		if (offset)
			skb_reserve(skb, align - offset);
	}

	skb_reserve(skb, reserve);
}

/**
 * qdf_nbuf_cb_update_vdev_id() - update vdev id in skb cb
 * @skb: skb pointer whose cb is updated with vdev id information
 * @vdev_id: vdev id to be updated in cb
 *
 * Return: void
 */
static inline void
qdf_nbuf_cb_update_vdev_id(struct sk_buff *skb, uint8_t vdev_id)
{
	QDF_NBUF_CB_RX_VDEV_ID(skb) = vdev_id;
}

/**
 * __qdf_nbuf_init_replenish_timer() - Initialize the alloc replenish timer
 *
 * This function initializes the nbuf alloc fail replenish timer.
 *
 * Return: void
 */
void __qdf_nbuf_init_replenish_timer(void);

/**
 * __qdf_nbuf_deinit_replenish_timer() - Deinitialize the alloc replenish timer
 *
 * This function deinitializes the nbuf alloc fail replenish timer.
 *
 * Return: void
 */
void __qdf_nbuf_deinit_replenish_timer(void);

/**
 * __qdf_nbuf_len() - return the amount of valid data in the skb
 * @skb: Pointer to network buffer
 *
 * This API returns the amount of valid data in the skb, If there are frags
 * then it returns total length.
 *
 * Return: network buffer length
 */
static inline size_t __qdf_nbuf_len(struct sk_buff *skb)
{
	int i, extra_frag_len = 0;

	i = QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb);
	if (i > 0)
		extra_frag_len = QDF_NBUF_CB_TX_EXTRA_FRAG_LEN(skb);

	return extra_frag_len + skb->len;
}

/**
 * __qdf_nbuf_num_frags_init() - init extra frags
 * @skb: sk buffer
 *
 * Return: none
 */
static inline
void __qdf_nbuf_num_frags_init(struct sk_buff *skb)
{
	QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb) = 0;
}

/**
 * __qdf_nbuf_push_head() - Push data in the front
 * @skb: Pointer to network buffer
 * @size: size to be pushed
 *
 * Return: New data pointer of this buf after data has been pushed,
 *         or NULL if there is not enough room in this buf.
 */
static inline uint8_t *__qdf_nbuf_push_head(struct sk_buff *skb, size_t size)
{
	if (QDF_NBUF_CB_PADDR(skb))
		QDF_NBUF_CB_PADDR(skb) -= size;

	return skb_push(skb, size);
}


/**
 * __qdf_nbuf_pull_head() - pull data out from the front
 * @skb: Pointer to network buffer
 * @size: size to be popped
 *
 * Return: New data pointer of this buf after data has been popped,
 *	   or NULL if there is not sufficient data to pull.
 */
static inline uint8_t *__qdf_nbuf_pull_head(struct sk_buff *skb, size_t size)
{
	if (QDF_NBUF_CB_PADDR(skb))
		QDF_NBUF_CB_PADDR(skb) += size;

	return skb_pull(skb, size);
}

/**
 * qdf_nbuf_is_intra_bss() - get intra bss bit
 * @buf: Network buffer
 *
 * Return: integer value - 0/1
 */
static inline int qdf_nbuf_is_intra_bss(struct sk_buff *buf)
{
	return 0;
}

/**
 * qdf_nbuf_set_intra_bss() - set intra bss bit
 * @buf: Network buffer
 * @val: 0/1
 *
 * Return: void
 */
static inline void qdf_nbuf_set_intra_bss(struct sk_buff *buf, uint8_t val)
{
}

/**
 * qdf_nbuf_init_replenish_timer - Initialize the alloc replenish timer
 *
 * This function initializes the nbuf alloc fail replenish timer.
 *
 * Return: void
 */
static inline void
qdf_nbuf_init_replenish_timer(void)
{
	__qdf_nbuf_init_replenish_timer();
}

/**
 * qdf_nbuf_deinit_replenish_timer - Deinitialize the alloc replenish timer
 *
 * This function deinitializes the nbuf alloc fail replenish timer.
 *
 * Return: void
 */
static inline void
qdf_nbuf_deinit_replenish_timer(void)
{
	__qdf_nbuf_deinit_replenish_timer();
}

static inline void
__qdf_nbuf_dma_inv_range(const void *buf_start, const void *buf_end) {}

static inline void
__qdf_nbuf_dma_inv_range_no_dsb(const void *buf_start, const void *buf_end) {}

static inline void
__qdf_nbuf_dma_clean_range_no_dsb(const void *buf_start, const void *buf_end) {}

static inline void
__qdf_dsb(void) {}

static inline void
__qdf_nbuf_dma_clean_range(const void *buf_start, const void *buf_end) {}

#endif /*_I_QDF_NBUF_M_H */

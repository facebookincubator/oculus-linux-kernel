/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: i_qdf_nbuf_w.h
 *
 * This file provides platform specific nbuf API's.
 * Included by i_qdf_nbuf.h and should not be included
 * directly from other files.
 */

#ifndef _I_QDF_NBUF_W_H
#define _I_QDF_NBUF_W_H

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
 * @u: union of TX and RX member elements
 * @u.rx.ext_cb_ptr: extended cb pointer
 * @u.rx.fctx: ctx to handle special pkts defined by ftype
 * @u.rx.rx_ctx_id: RX ring id
 * @u.rx.fcs_err: fcs error in RX packet
 * @u.rx.ipa_smmu_map: do IPA smmu map
 * @u.rx.flow_idx_valid: is flow_idx_valid flag
 * @u.rx.flow_idx_timeout: is flow_idx_timeout flag
 * @u.rx.rsvd8: reserved bits
 * @u.rx.num_elements_in_list: num of elements (nbufs) in the list
 * @u.rx.trace: combined structure for DP and protocol trace
 * @u.rx.trace.packet_stat: {NBUF_TX_PKT_[(HDD)|(TXRX_ENQUEUE)|(TXRX_DEQUEUE)|
 *                       +          (TXRX)|(HTT)|(HTC)|(HIF)|(CE)|(FREE)]
 * @u.rx.trace.dp_trace: flag (Datapath trace)
 * @u.rx.trace.packet_track: RX_DATA packet
 * @u.rx.trace.rsrvd: enable packet logging
 *
 * @u.rx.protocol_tag: protocol tag set by app for rcvd packet type
 * @u.rx.flow_tag: flow tag set by application for 5 tuples rcvd
 *
 * @u.rx.hw_info: combined structure for HW info fields
 * @u.rx.hw_info.desc_tlv_members.msdu_count: num of msdus
 * @u.rx.hw_info.desc_tlv_members.fragment_flag: is this fragment mpdu
 * @u.rx.hw_info.desc_tlv_members.flag_retry: is mpdu retry flag set
 * @u.rx.hw_info.desc_tlv_members.flag_is_ampdu: is ampdu flag set
 * @u.rx.hw_info.desc_tlv_members.bar_frame: is this bar frame
 * @u.rx.hw_info.desc_tlv_members.pn_fields_contain_valid_info: pn valid
 * @u.rx.hw_info.desc_tlv_members.is_raw_frame: is this raw frame
 * @u.rx.hw_info.desc_tlv_members.more_fragment_flag: more fragment flag
 * @u.rx.hw_info.desc_tlv_members.src_info: PPE VP number
 * @u.rx.hw_info.desc_tlv_members.mpdu_qos_control_valid: is qos ctrl valid
 * @u.rx.hw_info.desc_tlv_members.tid_val: tid value
 *
 * @u.rx.hw_info.desc_tlv_members.peer_id: peer id
 * @u.rx.hw_info.desc_tlv_members.ml_peer_valid: is ml peer valid
 * @u.rx.hw_info.desc_tlv_members.vdev_id: vdev id
 * @u.rx.hw_info.desc_tlv_members.hw_link_id: link id of RX packet
 * @u.rx.hw_info.desc_tlv_members.chip_id: chip id
 * @u.rx.hw_info.desc_tlv_members.reserved2: reserved
 *
 * @u.rx.hw_info.desc_tlv_members.flag_chfrag_start: first fragment of msdu
 * @u.rx.hw_info.desc_tlv_members.flag_chfrag_end: last fragment of msdu
 * @u.rx.hw_info.desc_tlv_members.flag_chfrag_cont: msdu frag is continued
 * @u.rx.hw_info.desc_tlv_members.msdu_len: msdu length
 * @u.rx.hw_info.desc_tlv_members.flag_is_frag: msdu is frag
 * @u.rx.hw_info.desc_tlv_members.flag_sa_valid: source address is valid
 * @u.rx.hw_info.desc_tlv_members.flag_da_valid: dest address is valid
 * @u.rx.hw_info.desc_tlv_members.flag_da_mcbc: is mcast/bcast msdu
 * @u.rx.hw_info.desc_tlv_members.l3_hdr_pad_msb: l3 pad bytes
 * @u.rx.hw_info.desc_tlv_members.tcp_udp_chksum_fail: tcp/udp checksum failed
 * @u.rx.hw_info.desc_tlv_members.ip_chksum_fail: ip checksum failed
 * @u.rx.hw_info.desc_tlv_members.fr_ds: FROM DS bit is set
 * @u.rx.hw_info.desc_tlv_members.to_ds: TO DS bit is set
 * @u.rx.hw_info.desc_tlv_members.intra_bss: this is intra-bss msdu
 * @u.rx.hw_info.desc_tlv_members.rsvd4: reserved
 *
 * @u.rx.hw_info.desc_tlv_members.release_source_module: release source
 * @u.rx.hw_info.desc_tlv_members.bm_action: bm action
 * @u.rx.hw_info.desc_tlv_members.buffer_or_desc_type: buffer or desc
 * @u.rx.hw_info.desc_tlv_members.return_buffer_manager: rbm value
 * @u.rx.hw_info.desc_tlv_members.reserved_2a: reserved
 * @u.rx.hw_info.desc_tlv_members.cache_id: cache_id
 * @u.rx.hw_info.desc_tlv_members.cookie_conversion_status: cc status
 * @u.rx.hw_info.desc_tlv_members.rxdma_push_reason: rxdma push reason
 * @u.rx.hw_info.desc_tlv_members.rxdma_error_code: rxdma error code
 * @u.rx.hw_info.desc_tlv_members.reo_push_reason: reo push reason
 * @u.rx.hw_info.desc_tlv_members.reo_error_code: reo error code
 * @u.rx.hw_info.desc_tlv_members.wbm_internal_error: wbm internal error
 *
 * @u.rx.hw_info.desc_info.mpdu_desc_info[2]: reo destination mpdu desc info
 * @u.rx.hw_info.desc_info.msdu_desc_info: reo destination msdu desc info
 * @u.rx.hw_info.desc_info.rx_error_code: wbm error codes
 *
 *
 * @u.tx.ext_cb_ptr: extended cb pointer
 * @u.tx.fctx: ctx to handle special pkts defined by ftype
 * @u.tx.ftype: ftype
 * @u.tx.xmit_type: xmit type of packet (MLD/Legacy)
 * @u.tx.reserved: unused
 * @u.tx.vdev_id: vdev_id
 * @u.tx.len: len
 * @u.tx.flags:
 * @u.tx.flags.bits.flag_efrag:
 * @u.tx.flags.bits.flag_nbuf:
 * @u.tx.flags.bits.num:
 * @u.tx.flags.bits.flag_chfrag_start:
 * @u.tx.flags.bits.flag_chfrag_cont:
 * @u.tx.flags.bits.flag_chfrag_end:
 * @u.tx.flags.bits.flag_ext_header:
 * @u.tx.flags.bits.is_critical:
 * @u.tx.flags.u8:
 * @u.tx.trace.packet_state:  {NBUF_TX_PKT_[(HDD)|(TXRX_ENQUEUE)|(TXRX_DEQUEUE)|
 *			+          (TXRX)|(HTT)|(HTC)|(HIF)|(CE)|(FREE)]
 * @u.tx.trace.is_packet_priv:
 * @u.tx.trace.packet_track: {NBUF_TX_PKT_[(DATA)|(MGMT)]_TRACK}
 * @u.tx.trace.to_fw: Flag to indicate send this packet to FW
 * @u.tx.trace.htt2_frm: flag (high-latency path only)
 * @u.tx.trace.proto_type: bitmap of NBUF_PKT_TRAC_TYPE[(EAPOL)|(DHCP)|
 *			+ (MGMT_ACTION)] - 4 bits
 * @u.tx.trace.dp_trace: flag (Datapath trace)
 * @u.tx.trace.is_bcast: flag (Broadcast packet)
 * @u.tx.trace.is_mcast: flag (Multicast packet)
 * @u.tx.trace.packet_type: flag (Packet type)
 * @u.tx.trace.print: enable packet logging
 *
 * @u.tx.vaddr: virtual address of ~
 * @u.tx.paddr: physical/DMA address of ~
 */

struct qdf_nbuf_cb {
	/* common */
	qdf_paddr_t paddr; /* of skb->data */
	/* valid only in one direction */
	union {
		/* Note: MAX: 40 bytes */
		struct {
			void *ext_cb_ptr;
			void *fctx;
			uint16_t ftype:4,
				 rx_ctx_id:4,
				 fcs_err:1,
				 ipa_smmu_map:1,
				 flow_idx_valid:1,
				 flow_idx_timeout:1,
				 rsvd8:4;
			uint8_t num_elements_in_list;
			union {
				uint8_t packet_state;
				uint8_t dp_trace:1,
					packet_track:3,
					rsrvd:4;
			} trace;
			uint16_t protocol_tag;
			uint16_t flow_tag;
			union {
				struct {
					/* do not re-arrange the fields in
					 * the below 3 uint32_t words as
					 * they map exactly to the desc info
					 */
#ifndef BIG_ENDIAN_HOST
					/* 1st word rx_mpdu_desc_info */
					uint32_t msdu_count:8,
						 fragment_flag:1,
						 flag_retry:1,
						 flag_is_ampdu:1,
						 bar_frame:1,
						 pn_fields_contain_valid_info:1,
						 is_raw_frame:1,
						 more_fragment_flag:1,
						 src_info:12,
						 mpdu_qos_control_valid:1,
						 tid_val:4;
#else
					uint32_t tid_val:4,
						 mpdu_qos_control_valid:1,
						 src_info:12,
						 more_fragment_flag:1,
						 is_raw_frame:1,
						 pn_fields_contain_valid_info:1,
						 bar_frame:1,
						 flag_is_ampdu:1,
						 flag_retry:1,
						 fragment_flag:1,
						 msdu_count:8;
#endif
					/* 2nd word rx_mpdu_desc_info */
					uint32_t peer_id:14,
						 vdev_id:8,
						 hw_link_id:4,
						 chip_id:3,
						 reserved2:3;
#ifndef BIG_ENDIAN_HOST
					/* 1st word of rx_msdu_desc_info */
					uint32_t flag_chfrag_start:1,
						 flag_chfrag_end:1,
						 flag_chfrag_cont:1,
						 msdu_len:14,
						 flag_is_frag:1,
						 flag_sa_valid:1,
						 flag_da_valid:1,
						 flag_da_mcbc:1,
						 l3_hdr_pad_msb:1,
						 tcp_udp_chksum_fail:1,
						 ip_chksum_fail:1,
						 fr_ds:1,
						 to_ds:1,
						 intra_bss:1,
						 rsvd4:5;
					uint32_t release_source_module:3,
						 bm_action:3,
						 buffer_or_desc_type:3,
						 return_buffer_manager:4,
						 reserved_2a:2,
						 cache_id:1,
						 cookie_conversion_status:1,
						 rxdma_push_reason:2,
						 rxdma_error_code:5,
						 reo_push_reason:2,
						 reo_error_code:5,
						 wbm_internal_error:1;
#else
					uint32_t rsvd4:5,
						 intra_bss:1,
						 to_ds:1,
						 fr_ds:1,
						 ip_chksum_fail:1,
						 tcp_udp_chksum_fail:1,
						 l3_hdr_pad_msb:1,
						 flag_da_mcbc:1,
						 flag_da_valid:1,
						 flag_sa_valid:1,
						 flag_is_frag:1,
						 msdu_len:14,
						 flag_chfrag_cont:1,
						 flag_chfrag_end:1,
						 flag_chfrag_start:1;
					uint32_t wbm_internal_error:1,
						 reo_error_code:5,
						 reo_push_reason:2,
						 rxdma_error_code:5,
						 rxdma_push_reason:2,
						 cookie_conversion_status:1,
						 cache_id:1,
						 reserved_2a:2,
						 return_buffer_manager:4,
						 buffer_or_desc_type:3,
						 bm_action:3,
						 release_source_module:3;
#endif
				} desc_tlv_members;
				struct {
					uint32_t mpdu_desc_info[2];
					uint32_t msdu_desc_info;
					uint32_t rx_error_codes;
				} desc_info;
			} hw_info;
		} rx;

		/* Note: MAX: 40 bytes */
		struct {
			void *ext_cb_ptr;
			void *fctx;
			uint8_t ftype:4,
				xmit_type:1,
				reserved:3;
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
					/* used only for hl */
					htt2_frm:1,
					proto_type:3;
				uint8_t dp_trace:1,
					is_bcast:1,
					is_mcast:1,
					packet_type:4,
					print:1;
			} trace;
			unsigned char *vaddr;
			qdf_paddr_t paddr;
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

#define QDF_NBUF_CB_RX_CTX_ID(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.rx_ctx_id)
#define QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(skb) \
		(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.num_elements_in_list)

#define QDF_NBUF_CB_RX_PACKET_STATE(skb)\
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.trace.packet_state)
#define QDF_NBUF_CB_RX_DP_TRACE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.trace.dp_trace)

#define QDF_NBUF_CB_RX_FTYPE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.ftype)

#define QDF_NBUF_CB_PKT_XMIT_TYPE(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.xmit_type)

#define QDF_NBUF_CB_RX_CHFRAG_START(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_chfrag_start)
#define QDF_NBUF_CB_RX_CHFRAG_CONT(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_chfrag_cont)
#define QDF_NBUF_CB_RX_CHFRAG_END(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_chfrag_end)

#define QDF_NBUF_CB_RX_DA_MCBC(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_da_mcbc)

#define QDF_NBUF_CB_RX_L3_PAD_MSB(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.l3_hdr_pad_msb)

#define QDF_NBUF_CB_RX_DA_VALID(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_da_valid)

#define QDF_NBUF_CB_RX_SA_VALID(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_sa_valid)

#define QDF_NBUF_CB_RX_RETRY_FLAG(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_retry)

#define QDF_NBUF_CB_RX_RAW_FRAME(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.is_raw_frame)

#define QDF_NBUF_CB_RX_FROM_DS(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.fr_ds)

#define QDF_NBUF_CB_RX_TO_DS(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.to_ds)

#define QDF_NBUF_CB_RX_IS_FRAG(skb) \
	(((struct qdf_nbuf_cb *) \
	((skb)->cb))->u.rx.hw_info.desc_tlv_members.flag_is_frag)

#define QDF_NBUF_CB_RX_FCS_ERR(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.fcs_err)

#define QDF_NBUF_CB_RX_MSDU_DESC_INFO(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_info.msdu_desc_info)

#define QDF_NBUF_CB_RX_MPDU_DESC_INFO(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_info.mpdu_desc_info)

#define QDF_NBUF_CB_RX_MPDU_DESC_INFO_1(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_info.mpdu_desc_info[0])

#define QDF_NBUF_CB_RX_MPDU_DESC_INFO_2(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_info.mpdu_desc_info[1])

#define QDF_NBUF_CB_RX_ERROR_CODE_INFO(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_info.rx_error_codes)

#define QDF_NBUF_CB_RX_PEER_ID(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.peer_id)

#define QDF_NBUF_CB_RX_VDEV_ID(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.vdev_id)

#define QDF_NBUF_CB_RX_PKT_LEN(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.msdu_len)

#define QDF_NBUF_CB_RX_TID_VAL(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.tid_val)

#define QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.num_elements_in_list)

#define QDF_NBUF_UPDATE_TX_PKT_COUNT(skb, PACKET_STATE) \
	qdf_nbuf_set_state(skb, PACKET_STATE)

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
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.paddr.dma_addr)

/* assume the OS provides a single fragment */
#define __qdf_nbuf_get_num_frags(skb)		   \
	(QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb) + 1)

#define __qdf_nbuf_reset_num_frags(skb) \
	(QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(skb) = 0)

/* ext_cb accessor macros and internal API's */
#define QDF_NBUF_CB_EXT_CB(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.ext_cb_ptr)

#define __qdf_nbuf_set_ext_cb(skb, ref) \
	do { \
		QDF_NBUF_CB_EXT_CB((skb)) = (ref); \
	} while (0)

#define __qdf_nbuf_get_ext_cb(skb) \
	QDF_NBUF_CB_EXT_CB((skb))

/* fctx accessor macros and internal API's*/
#define QDF_NBUF_CB_RX_FCTX(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.fctx)

#define QDF_NBUF_CB_TX_FCTX(skb) \
	(((struct qdf_nbuf_cb *)((skb)->cb))->u.tx.fctx)

#define QDF_NBUF_CB_RX_INTRA_BSS(skb) \
	(((struct qdf_nbuf_cb *) \
	  ((skb)->cb))->u.rx.hw_info.desc_tlv_members.intra_bss)

#define __qdf_nbuf_set_rx_fctx_type(skb, ctx, type) \
	do { \
		QDF_NBUF_CB_RX_FCTX((skb)) = (ctx); \
		QDF_NBUF_CB_RX_FTYPE((skb)) = (type); \
	} while (0)

#define __qdf_nbuf_get_rx_fctx(skb) \
		 QDF_NBUF_CB_RX_FCTX((skb))

#define __qdf_nbuf_set_intra_bss(skb, val) \
	((QDF_NBUF_CB_RX_INTRA_BSS((skb))) = val)

#define __qdf_nbuf_is_intra_bss(skb) \
	(QDF_NBUF_CB_RX_INTRA_BSS((skb)))

#define __qdf_nbuf_set_tx_fctx_type(skb, ctx, type) \
	do { \
		QDF_NBUF_CB_TX_FCTX((skb)) = (ctx); \
		QDF_NBUF_CB_TX_FTYPE((skb)) = (type); \
	} while (0)

#define __qdf_nbuf_get_tx_fctx(skb) \
		 QDF_NBUF_CB_TX_FCTX((skb))

#define QDF_NBUF_CB_RX_PROTOCOL_TAG(skb) \
		(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.protocol_tag)

#define __qdf_nbuf_set_rx_protocol_tag(skb, val) \
		((QDF_NBUF_CB_RX_PROTOCOL_TAG((skb))) = val)

#define __qdf_nbuf_get_rx_protocol_tag(skb) \
		(QDF_NBUF_CB_RX_PROTOCOL_TAG((skb)))

#define QDF_NBUF_CB_RX_FLOW_TAG(skb) \
		(((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.flow_tag)

#define __qdf_nbuf_set_rx_flow_tag(skb, val) \
		((QDF_NBUF_CB_RX_FLOW_TAG((skb))) = val)

#define __qdf_nbuf_get_rx_flow_tag(skb) \
		(QDF_NBUF_CB_RX_FLOW_TAG((skb)))

#define QDF_NBUF_CB_RX_FLOW_IDX_VALID(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.rx.flow_idx_valid)

#define __qdf_nbuf_set_rx_flow_idx_valid(skb, val) \
		((QDF_NBUF_CB_RX_FLOW_IDX_VALID((skb))) = val)

#define __qdf_nbuf_get_rx_flow_idx_valid(skb) \
		(QDF_NBUF_CB_RX_FLOW_IDX_VALID((skb)))

#define QDF_NBUF_CB_RX_FLOW_IDX_TIMEOUT(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.rx.flow_idx_timeout)

#define QDF_NBUF_CB_RX_HW_LINK_ID(skb) \
		(((struct qdf_nbuf_cb *) \
		((skb)->cb))->u.rx.hw_info.desc_tlv_members.hw_link_id)

#define __qdf_nbuf_set_rx_flow_idx_timeout(skb, val) \
		((QDF_NBUF_CB_RX_FLOW_IDX_TIMEOUT((skb))) = val)

#define __qdf_nbuf_get_rx_flow_idx_timeout(skb) \
		(QDF_NBUF_CB_RX_FLOW_IDX_TIMEOUT((skb)))

#define  QDF_NBUF_CB_RX_PACKET_IPA_SMMU_MAP(skb) \
	 (((struct qdf_nbuf_cb *)((skb)->cb))->u.rx.ipa_smmu_map)

#define __qdf_nbuf_data_attr_get(skb) (0)
#define __qdf_nbuf_data_attr_set(skb, data_attr)

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
 * qdf_nbuf_cb_update_vdev_id() - update vdev id in skb cb
 * @skb: skb pointer whose cb is updated with vdev id information
 * @vdev_id: vdev id to be updated in cb
 *
 * Return: void
 */
static inline void
qdf_nbuf_cb_update_vdev_id(struct sk_buff *skb, uint8_t vdev_id)
{
	/* Does not apply to WIN */
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
	return skb_push(skb, size);
}

/**
 * __qdf_nbuf_pull_head() - pull data out from the front
 * @skb: Pointer to network buffer
 * @size: size to be popped
 *
 * Return: New data pointer of this buf after data has been popped,
 * or NULL if there is not sufficient data to pull.
 */
static inline uint8_t *__qdf_nbuf_pull_head(struct sk_buff *skb, size_t size)
{
	return skb_pull(skb, size);
}

static inline void qdf_nbuf_init_replenish_timer(void) {}
static inline void qdf_nbuf_deinit_replenish_timer(void) {}

/**
 * __qdf_nbuf_dma_inv_range() - nbuf invalidate
 * @buf_start: from
 * @buf_end: to address to invalidate
 *
 * Return: none
 */
#if (defined(__LINUX_ARM_ARCH__) && !defined(DP_NO_CACHE_DESC_SUPPORT))
static inline void
__qdf_nbuf_dma_inv_range(const void *buf_start, const void *buf_end)
{
	dmac_inv_range(buf_start, buf_end);
}

static inline void
__qdf_nbuf_dma_inv_range_no_dsb(const void *buf_start, const void *buf_end)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 89)
	dmac_inv_range_no_dsb(buf_start, buf_end);
#else
	dmac_inv_range(buf_start, buf_end);
#endif
}

static inline void
__qdf_nbuf_dma_clean_range_no_dsb(const void *buf_start, const void *buf_end)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 89)
	dmac_clean_range_no_dsb(buf_start, buf_end);
#else
	dmac_clean_range(buf_start, buf_end);
#endif
}

static inline void
__qdf_dsb(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 89)
	dsb(st);
#endif
}

static inline void
__qdf_nbuf_dma_clean_range(const void *buf_start, const void *buf_end)
{
	dmac_clean_range(buf_start, buf_end);
}
#elif defined(__LINUX_MIPS32_ARCH__) || defined(__LINUX_MIPS64_ARCH__)
static inline void
__qdf_nbuf_dma_inv_range(const void *buf_start, const void *buf_end)
{
	dma_cache_inv((unsigned long)buf_start,
		      (unsigned long)(buf_end - buf_start));
}

static inline void
__qdf_nbuf_dma_inv_range_no_dsb(const void *buf_start, const void *buf_end)
{
	dma_cache_inv((unsigned long)buf_start,
		      (unsigned long)(buf_end - buf_start));
}

static inline void
__qdf_nbuf_dma_clean_range_no_dsb(const void *buf_start, const void *buf_end)
{
	dmac_cache_wback((unsigned long)buf_start,
			 (unsigned long)(buf_end - buf_start));
}

static inline void
__qdf_dsb(void)
{
}

static inline void
__qdf_nbuf_dma_clean_range(const void *buf_start, const void *buf_end)
{
	dma_cache_wback((unsigned long)buf_start,
			(unsigned long)(buf_end - buf_start));
}
#else
static inline void
__qdf_nbuf_dma_inv_range(const void *buf_start, const void *buf_end)
{
}

static inline void
__qdf_nbuf_dma_inv_range_no_dsb(const void *buf_start, const void *buf_end)
{
}

static inline void
__qdf_nbuf_dma_clean_range_no_dsb(const void *buf_start, const void *buf_end)
{
}

static inline void
__qdf_dsb(void)
{
}

static inline void
__qdf_nbuf_dma_clean_range(const void *buf_start, const void *buf_end)
{
}
#endif
#endif /*_I_QDF_NBUF_W_H */

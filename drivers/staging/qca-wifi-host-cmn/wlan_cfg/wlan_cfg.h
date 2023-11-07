/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_CFG_H
#define __WLAN_CFG_H

#include <wlan_init_cfg.h>

/* DP process status */
#if defined(MAX_PDEV_CNT) && (MAX_PDEV_CNT == 1)
#define CONFIG_PROCESS_RX_STATUS 1
#define CONFIG_PROCESS_TX_STATUS 1
#else
#define CONFIG_PROCESS_RX_STATUS 0
#define CONFIG_PROCESS_TX_STATUS 0
#endif

/* Miscellaneous configuration */
#define MAX_IDLE_SCATTER_BUFS 16
#define DP_MAX_IRQ_PER_CONTEXT 12
#define MAX_HTT_METADATA_LEN 32
#define DP_MAX_TIDS 17
#define DP_NON_QOS_TID 16
#define DP_NULL_DATA_TID 17

#ifdef CONFIG_BERYLLIUM
#define WLAN_CFG_RX_FST_MAX_SEARCH 16
#else
#define WLAN_CFG_RX_FST_MAX_SEARCH 2
#endif
#define WLAN_CFG_RX_FST_TOEPLITZ_KEYLEN 40

#define INVALID_PDEV_ID 0xFF

#define WLAN_CFG_RX_RING_MASK_0 0x1
#define WLAN_CFG_RX_RING_MASK_1 0x2
#define WLAN_CFG_RX_RING_MASK_2 0x4
#define WLAN_CFG_RX_RING_MASK_3 0x8
#define WLAN_CFG_RX_RING_MASK_4 0x10
#define WLAN_CFG_RX_RING_MASK_5 0x20
#define WLAN_CFG_RX_RING_MASK_6 0x40
#define WLAN_CFG_RX_RING_MASK_7 0x80

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
#ifdef IPA_OFFLOAD
#define WLAN_CFG_RX_NEAR_FULL_IRQ_MASK_1 (WLAN_CFG_RX_RING_MASK_0 |	\
					  WLAN_CFG_RX_RING_MASK_1 |	\
					  WLAN_CFG_RX_RING_MASK_2)

#define WLAN_CFG_RX_NEAR_FULL_IRQ_MASK_2 (WLAN_CFG_RX_RING_MASK_4 |	\
					  WLAN_CFG_RX_RING_MASK_5 |	\
					  WLAN_CFG_RX_RING_MASK_6)

#define WLAN_CFG_TX_RING_NEAR_FULL_IRQ_MASK (WLAN_CFG_TX_RING_MASK_0 | \
					     WLAN_CFG_TX_RING_MASK_4 | \
					     WLAN_CFG_TX_RING_MASK_2)

#else
#define WLAN_CFG_RX_NEAR_FULL_IRQ_MASK_1 (WLAN_CFG_RX_RING_MASK_0 |	\
					  WLAN_CFG_RX_RING_MASK_1 |	\
					  WLAN_CFG_RX_RING_MASK_2 |	\
					  WLAN_CFG_RX_RING_MASK_3)

#define WLAN_CFG_RX_NEAR_FULL_IRQ_MASK_2 (WLAN_CFG_RX_RING_MASK_4 |	\
					  WLAN_CFG_RX_RING_MASK_5 |	\
					  WLAN_CFG_RX_RING_MASK_6 |	\
					  WLAN_CFG_RX_RING_MASK_7)

#ifdef QCA_WIFI_KIWI_V2
#define WLAN_CFG_TX_RING_NEAR_FULL_IRQ_MASK (WLAN_CFG_TX_RING_MASK_0 | \
					     WLAN_CFG_TX_RING_MASK_4 | \
					     WLAN_CFG_TX_RING_MASK_2 | \
					     WLAN_CFG_TX_RING_MASK_5 | \
					     WLAN_CFG_TX_RING_MASK_6)
#else /* !QCA_WIFI_KIWI_V2 */
#define WLAN_CFG_TX_RING_NEAR_FULL_IRQ_MASK (WLAN_CFG_TX_RING_MASK_0 | \
					     WLAN_CFG_TX_RING_MASK_4 | \
					     WLAN_CFG_TX_RING_MASK_2 | \
					     WLAN_CFG_TX_RING_MASK_6 | \
					     WLAN_CFG_TX_RING_MASK_7)
#endif /* QCA_WIFI_KIWI_V2 */
#endif
#endif

/* Max number of chips that can participate in MLO */
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
#define WLAN_MAX_MLO_CHIPS 4
#define WLAN_MAX_MLO_LINKS_PER_SOC 2
#else
#define WLAN_MAX_MLO_CHIPS 1
#endif

struct wlan_cfg_dp_pdev_ctxt;

/**
 * struct wlan_cfg_tcl_wbm_ring_num_map - TCL WBM Ring number mapping
 * @tcl_ring_num: TCL Ring number
 * @wbm_ring_num: WBM Ring number
 * @wbm_rbm_id: WBM RBM ID to be used when enqueuing to TCL
 * @for_ipa: whether this TCL/WBM for IPA use or not
 */
struct wlan_cfg_tcl_wbm_ring_num_map {
	uint8_t tcl_ring_num;
	uint8_t wbm_ring_num;
	uint8_t wbm_rbm_id;
	uint8_t for_ipa;
};

/**
 * struct wlan_srng_cfg - Per ring configuration parameters
 * @timer_threshold: Config to control interrupts based on timer duration
 * @batch_count_threshold: Config to control interrupts based on
 * number of packets in the ring
 * @low_threshold: Config to control low threshold interrupts for SRC rings
 */
struct wlan_srng_cfg {
	uint32_t timer_threshold;
	uint32_t batch_count_threshold;
	uint32_t low_threshold;
};

/**
 * struct wlan_cfg_dp_soc_ctxt - Configuration parameters for SoC (core TxRx)
 * @num_int_ctxts: Number of NAPI/Interrupt contexts to be registered for DP
 * @max_clients: Maximum number of peers/stations supported by device
 * @max_alloc_size: Maximum allocation size for any dynamic memory
 *			allocation request for this device
 * @per_pdev_tx_ring: 0: TCL ring is not mapped per radio
 *		       1: Each TCL ring is mapped to one radio/pdev
 * @num_tx_comp_rings: Number of Tx comp rings supported by device
 * @num_tcl_data_rings: Number of TCL Data rings supported by device
 * @num_nss_tcl_data_rings:
 * @per_pdev_rx_ring: 0: REO ring is not mapped per radio
 *		       1: Each REO ring is mapped to one radio/pdev
 * @per_pdev_lmac_ring:
 * @num_reo_dest_rings:
 * @num_nss_reo_dest_rings:
 * @num_tx_desc_pool: Number of Tx Descriptor pools
 * @num_tx_ext_desc_pool: Number of Tx MSDU extension Descriptor pools
 * @num_tx_desc: Number of Tx Descriptors per pool
 * @num_tx_spl_desc: Number of Tx Descriptors per pool to handle special frames
 * @min_tx_desc: Minimum number of Tx Descriptors per pool
 * @num_tx_ext_desc: Number of Tx MSDU extension Descriptors per pool
 * @max_peer_id: Maximum value of peer id that FW can assign for a client
 * @htt_packet_type: Default 802.11 encapsulation type for any VAP created
 * @int_tx_ring_mask: Bitmap of Tx interrupts mapped to each NAPI/Intr context
 * @int_rx_ring_mask: Bitmap of Rx interrupts mapped to each NAPI/Intr context
 * @int_batch_threshold_ppe2tcl:
 * @int_timer_threshold_ppe2tcl:
 * @int_batch_threshold_tx:
 * @int_timer_threshold_tx:
 * @int_batch_threshold_rx:
 * @int_timer_threshold_rx:
 * @int_batch_threshold_other:
 * @int_timer_threshold_other:
 * @int_timer_threshold_mon:
 * @tx_ring_size:
 * @time_control_bp:
 * @tx_comp_ring_size:
 * @tx_comp_ring_size_nss:
 * @int_rx_mon_ring_mask: Bitmap of Rx monitor ring interrupts mapped to each
 *			  NAPI/Intr context
 * @int_tx_mon_ring_mask: Bitmap of Tx monitor ring interrupts mapped to each
 *			  NAPI/Intr context
 * @int_host2rxdma_mon_ring_mask:
 * @int_rxdma2host_mon_ring_mask:
 * @int_ce_ring_mask: Bitmap of CE interrupts mapped to each NAPI/Intr context
 * @int_rx_err_ring_mask: Bitmap of Rx err ring interrupts mapped to each
 *			  NAPI/Intr context
 * @int_rx_wbm_rel_ring_mask: Bitmap of wbm rel ring interrupts mapped to each
 *			      NAPI/Intr context
 * @int_reo_status_ring_mask: Bitmap of reo status ring interrupts mapped to
 *                            each NAPI/Intr context
 * @int_rxdma2host_ring_mask:
 * @int_host2rxdma_ring_mask:
 * @int_rx_ring_near_full_irq_1_mask: Bitmap of REO DST ring near full interrupt
 *				mapped to each NAPI/INTR context
 * @int_rx_ring_near_full_irq_2_mask: Bitmap of REO DST ring near full interrupt
 *				mapped to each NAPI/INTR context
 * @int_tx_ring_near_full_irq_mask: Bitmap of Tx completion ring near full
 *				interrupt mapped to each NAPI/INTR context
 * @int_host2txmon_ring_mask: Bitmap of Tx monitor source ring interrupt
 *				mapped to each NAPI/INTR context
 * @int_ppeds_wbm_release_ring_mask:
 * @int_ppe2tcl_ring_mask:
 * @int_reo2ppe_ring_mask:
 * @int_umac_reset_intr_mask: Bitmap of UMAC reset interrupt mapped to each
 * NAPI/INTR context
 * @hw_macid:
 * @hw_macid_pdev_id_map:
 * @base_hw_macid:
 * @rx_hash: Enable hash based steering of rx packets
 * @tso_enabled: enable/disable tso feature
 * @lro_enabled: enable/disable LRO feature
 * @sg_enabled: enable disable scatter gather feature
 * @gro_enabled: enable disable GRO feature
 * @tc_based_dynamic_gro: enable/disable tc based dynamic gro
 * @tc_ingress_prio: ingress prio to be checked for dynamic gro
 * @ipa_enabled: Flag indicating if IPA is enabled
 * @ol_tx_csum_enabled: Flag indicating if TX csum is enabled
 * @ol_rx_csum_enabled: Flag indicating if Rx csum is enabled
 * @rawmode_enabled: Flag indicating if RAW mode is enabled
 * @peer_flow_ctrl_enabled: Flag indicating if peer flow control is enabled
 * @napi_enabled: enable/disable interrupt mode for reaping tx and rx packets
 * @p2p_tcp_udp_checksumoffload: enable/disable checksum offload for P2P mode
 * @nan_tcp_udp_checksumoffload: enable/disable checksum offload for NAN mode
 * @tcp_udp_checksumoffload: enable/disable checksum offload
 * @legacy_mode_checksumoffload_disable:
 * @defrag_timeout_check:
 * @nss_cfg: nss configuration
 * @tx_flow_stop_queue_threshold:
 * @tx_flow_start_queue_offset:
 * @rx_defrag_min_timeout: rx defrag minimum timeout
 * @reo_dst_ring_size:
 * @wbm_release_ring: wbm release ring size
 * @tcl_cmd_credit_ring: tcl command/credit ring size
 * @tcl_status_ring: tcl status ring size
 * @reo_reinject_ring: reo reinject ring
 * @rx_release_ring: rx release ring size
 * @reo_exception_ring: reo exception ring size
 * @reo_cmd_ring: reo cmd ring size
 * @reo_status_ring: reo status ting size
 * @rxdma_refill_ring: rxdma refill ring size
 * @rxdma_refill_lt_disable: rxdma refill low threshold disable
 * @rxdma_err_dst_ring: rxdma error destination ring size
 * @per_pkt_trace:
 * @raw_mode_war: enable/disable raw mode war
 * @enable_data_stall_detection: enable/disable specific data stall detection
 * @disable_intra_bss_fwd: flag to disable intra bss forwarding
 * @rxdma1_enable: flag to indicate if rxdma1 is enabled
 * @delay_mon_replenish: delay monitor buffer replenish
 * @max_ast_idx:
 * @tx_desc_limit_0: tx_desc limit for 5 GHz High
 * @tx_desc_limit_1: tx_desc limit for 2 GHz
 * @tx_desc_limit_2: tx_desc limit for 5 GHz Low
 * @tx_device_limit: tx device limit
 * @tx_spl_device_limit: tx device limit for special frames
 * @tx_sw_internode_queue: tx sw internode queue
 * @mon_drop_thresh:
 * @tx_comp_loop_pkt_limit: Max # of packets to be processed in 1 tx comp loop
 * @rx_reap_loop_pkt_limit: Max # of packets to be processed in 1 rx reap loop
 * @rx_hp_oos_update_limit: Max # of HP OOS (out of sync) updates
 * @rx_enable_eol_data_check: flag to enable check for more ring data at end of
 *                            dp_rx_process loop
 * @tx_comp_enable_eol_data_check: flag to enable/disable checking for more data
 *                                at end of tx_comp_handler loop.
 * @rx_sw_desc_weight: rx sw descriptor weight configuration
 * @rx_sw_desc_num:
 * @is_rx_mon_protocol_flow_tag_enabled: flag to enable/disable RX protocol or
 *                                       flow tagging in monitor/mon-lite mode
 * @is_rx_flow_tag_enabled: flag to enable/disable RX flow tagging using FSE
 * @is_rx_flow_search_table_per_pdev: flag to indicate if a per-SOC or per-pdev
 *                                    table should be used
 * @rx_flow_search_table_size: indicates the number of flows in the flow search
 *                             table
 * @rx_flow_max_search: max skid length for each hash entry
 * @rx_toeplitz_hash_key: toeplitz key pointer used for hash computation over
 *                        5 tuple flow entry
 * @pktlog_buffer_size: packet log buffer size
 * @is_rx_fisa_enabled: flag to enable/disable FISA Rx
 * @is_rx_fisa_lru_del_enabled:
 * @is_tso_desc_attach_defer:
 * @delayed_replenish_entries:
 * @reo_rings_mapping:
 * @pext_stats_enabled: Flag to enable and disabled peer extended stats
 * @is_rx_buff_pool_enabled: flag to enable/disable emergency RX buffer
 *                           pool support
 * @is_rx_refill_buff_pool_enabled: flag to enable/disable RX refill buffer
 *                           pool support
 * @rx_pending_high_threshold: threshold of starting pkt drop
 * @rx_pending_low_threshold: threshold of stopping pkt drop
 * @is_poll_mode_enabled:
 * @is_swlm_enabled: flag to enable/disable SWLM
 * @fst_in_cmem:
 * @tx_per_pkt_vdev_id_check: Enable tx perpkt vdev id check
 * @radio0_rx_default_reo:
 * @radio1_rx_default_reo:
 * @radio2_rx_default_reo:
 * @wow_check_rx_pending_enable: Enable RX frame pending check in WoW
 * @jitter_stats_enabled: true if jitter stats are enabled
 * @ipa_tx_ring_size: IPA tx ring size
 * @ipa_tx_comp_ring_size: IPA tx completion ring size
 * @ipa_tx_alt_ring_size: IPA tx alt ring size
 * @ipa_tx_alt_comp_ring_size: IPA tx alt completion ring size
 * @hw_cc_enabled: cookie conversion enabled
 * @tcl_wbm_map_array: TCL-WBM map array
 * @ppeds_enable: Enable PPE Direct Switch feature
 * @reo2ppe_ring: REO2PPE ring size
 * @ppe2tcl_ring: PPE2TCL ring size
 * @ppeds_num_tx_desc: Number of tx descs for PPE DS
 * @ppeds_tx_comp_napi_budget: Napi budget for tx completions
 * @pkt_capture_mode: Packet capture mode config
 * @rx_mon_buf_ring_size: Rx monitor buf ring size
 * @tx_mon_buf_ring_size: Tx monitor buf ring size
 * @rx_rel_wbm2sw_ring_id: Rx WBM2SW ring id
 * @tx_rings_grp_bitmap: bitmap of group intr contexts which have
 *                       non-zero tx ring mask
 * @mlo_chip_rx_ring_map: map of chip_id to rx ring map
 * @vdev_stats_hw_offload_config: HW vdev stats config
 * @vdev_stats_hw_offload_timer: HW vdev stats timer duration
 * @num_rxdma_dst_rings_per_pdev: Number of Rx DMA rings per pdev
 * @txmon_hw_support: TxMON HW support
 * @txmon_sw_peer_filtering: TxMON sw peer filtering support
 * @num_rxdma_status_rings_per_pdev: Num RXDMA status rings
 * @tx_capt_max_mem_allowed: Max memory for Tx packet capture
 * @sawf_enabled:  Is SAWF enabled
 * @sawf_stats: SAWF Statistics
 * @mpdu_retry_threshold_1: MPDU retry threshold 1 to increment tx bad count
 * @mpdu_retry_threshold_2: MPDU retry threshold 2 to increment tx bad count
 * @napi_scale_factor: scaling factor to be used for napi polls
 * @notify_frame_support: flag indicating capability to mark notify frames
 * @is_handle_invalid_decap_type_disabled: flag to indicate if invalid decap
 *                                         type handling is disabled
 * @tx_pkt_inspect_for_ilp: flag to indicate if TX packet inspection for HW
 *			    based ILP feature is enabled
 * @pointer_timer_threshold_rx: RX REO2SW ring pointer update timer threshold
 * @pointer_num_threshold_rx: RX REO2SW ring pointer update entries threshold
 * @special_frame_msk: Special frame mask
 */
struct wlan_cfg_dp_soc_ctxt {
	int num_int_ctxts;
	int max_clients;
	int max_alloc_size;
	int per_pdev_tx_ring;
	int num_tx_comp_rings;
	int num_tcl_data_rings;
	int num_nss_tcl_data_rings;
	int per_pdev_rx_ring;
	int per_pdev_lmac_ring;
	int num_reo_dest_rings;
	int num_nss_reo_dest_rings;
	int num_tx_desc_pool;
	int num_tx_ext_desc_pool;
	int num_tx_desc;
	int num_tx_spl_desc;
	int min_tx_desc;
	int num_tx_ext_desc;
	int max_peer_id;
	int htt_packet_type;
	int int_batch_threshold_ppe2tcl;
	int int_timer_threshold_ppe2tcl;
	int int_batch_threshold_tx;
	int int_timer_threshold_tx;
	int int_batch_threshold_rx;
	int int_timer_threshold_rx;
	int int_batch_threshold_other;
	int int_timer_threshold_other;
	int int_timer_threshold_mon;
	int tx_ring_size;
	int time_control_bp;
	int tx_comp_ring_size;
	int tx_comp_ring_size_nss;
	uint8_t int_tx_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rx_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rx_mon_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_tx_mon_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_host2rxdma_mon_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rxdma2host_mon_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_ce_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rx_err_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rx_wbm_rel_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_reo_status_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rxdma2host_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_host2rxdma_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rx_ring_near_full_irq_1_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_rx_ring_near_full_irq_2_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_tx_ring_near_full_irq_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_host2txmon_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_ppeds_wbm_release_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_ppe2tcl_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_reo2ppe_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	uint8_t int_umac_reset_intr_mask[WLAN_CFG_INT_NUM_CONTEXTS];
	int hw_macid[MAX_PDEV_CNT];
	int hw_macid_pdev_id_map[MAX_NUM_LMAC_HW];
	int base_hw_macid;
	bool rx_hash;
	bool tso_enabled;
	bool lro_enabled;
	bool sg_enabled;
	bool gro_enabled;
	bool tc_based_dynamic_gro;
	uint32_t tc_ingress_prio;
	bool ipa_enabled;
	bool ol_tx_csum_enabled;
	bool ol_rx_csum_enabled;
	bool rawmode_enabled;
	bool peer_flow_ctrl_enabled;
	bool napi_enabled;
	bool p2p_tcp_udp_checksumoffload;
	bool nan_tcp_udp_checksumoffload;
	bool tcp_udp_checksumoffload;
	bool legacy_mode_checksumoffload_disable;
	bool defrag_timeout_check;
	int nss_cfg;
	uint32_t tx_flow_stop_queue_threshold;
	uint32_t tx_flow_start_queue_offset;
	int rx_defrag_min_timeout;
	int reo_dst_ring_size;
	int wbm_release_ring;
	int tcl_cmd_credit_ring;
	int tcl_status_ring;
	int reo_reinject_ring;
	int rx_release_ring;
	int reo_exception_ring;
	int reo_cmd_ring;
	int reo_status_ring;
	int rxdma_refill_ring;
	bool rxdma_refill_lt_disable;
	int rxdma_err_dst_ring;
	uint32_t per_pkt_trace;
	bool raw_mode_war;
	uint32_t enable_data_stall_detection;
	bool disable_intra_bss_fwd;
	bool rxdma1_enable;
	bool delay_mon_replenish;
	int max_ast_idx;
	int tx_desc_limit_0;
	int tx_desc_limit_1;
	int tx_desc_limit_2;
	int tx_device_limit;
	int tx_spl_device_limit;
	int tx_sw_internode_queue;
	int mon_drop_thresh;
#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
	uint32_t tx_comp_loop_pkt_limit;
	uint32_t rx_reap_loop_pkt_limit;
	uint32_t rx_hp_oos_update_limit;
	bool rx_enable_eol_data_check;
	bool tx_comp_enable_eol_data_check;
#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */
	int rx_sw_desc_weight;
	int rx_sw_desc_num;
	bool is_rx_mon_protocol_flow_tag_enabled;
	bool is_rx_flow_tag_enabled;
	bool is_rx_flow_search_table_per_pdev;
	uint16_t rx_flow_search_table_size;
	uint16_t rx_flow_max_search;
	uint8_t *rx_toeplitz_hash_key;
	uint8_t pktlog_buffer_size;
	uint8_t is_rx_fisa_enabled;
	bool is_rx_fisa_lru_del_enabled;
	bool is_tso_desc_attach_defer;
	uint32_t delayed_replenish_entries;
	uint32_t reo_rings_mapping;
	bool pext_stats_enabled;
	bool is_rx_buff_pool_enabled;
	bool is_rx_refill_buff_pool_enabled;
	uint32_t rx_pending_high_threshold;
	uint32_t rx_pending_low_threshold;
	bool is_poll_mode_enabled;
	uint8_t is_swlm_enabled;
	bool fst_in_cmem;
	bool tx_per_pkt_vdev_id_check;
	uint8_t radio0_rx_default_reo;
	uint8_t radio1_rx_default_reo;
	uint8_t radio2_rx_default_reo;
	bool wow_check_rx_pending_enable;
	bool jitter_stats_enabled;
#ifdef IPA_OFFLOAD
	uint32_t ipa_tx_ring_size;
	uint32_t ipa_tx_comp_ring_size;
#ifdef IPA_WDI3_TX_TWO_PIPES
	int ipa_tx_alt_ring_size;
	int ipa_tx_alt_comp_ring_size;
#endif /* IPA_WDI3_TX_TWO_PIPES */
#endif /* IPA_OFFLOAD */
	bool hw_cc_enabled;
	struct wlan_cfg_tcl_wbm_ring_num_map *tcl_wbm_map_array;
#ifdef WLAN_SUPPORT_PPEDS
	bool ppeds_enable;
	int reo2ppe_ring;
	int ppe2tcl_ring;
	int ppeds_num_tx_desc;
	int ppeds_tx_comp_napi_budget;
#endif
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
	uint32_t pkt_capture_mode;
#endif
	uint32_t rx_mon_buf_ring_size;
	uint32_t tx_mon_buf_ring_size;
	uint8_t rx_rel_wbm2sw_ring_id;
	uint32_t tx_rings_grp_bitmap;
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	uint8_t mlo_chip_rx_ring_map;
#endif
#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
	bool vdev_stats_hw_offload_config;
	int vdev_stats_hw_offload_timer;
#endif
	uint8_t num_rxdma_dst_rings_per_pdev;
	bool txmon_hw_support;
	bool txmon_sw_peer_filtering;
	uint8_t num_rxdma_status_rings_per_pdev;
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	uint32_t tx_capt_max_mem_allowed;
#endif
#ifdef CONFIG_SAWF
	bool sawf_enabled;
#endif
#ifdef CONFIG_SAWF_STATS
	uint8_t sawf_stats;
#endif
	uint8_t mpdu_retry_threshold_1;
	uint8_t mpdu_retry_threshold_2;
	uint8_t napi_scale_factor;
	uint8_t notify_frame_support;
	bool is_handle_invalid_decap_type_disabled;
#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
	bool tx_pkt_inspect_for_ilp;
#endif
	uint16_t pointer_timer_threshold_rx;
	uint8_t pointer_num_threshold_rx;
	uint32_t special_frame_msk;
};

/**
 * struct wlan_cfg_dp_pdev_ctxt - Configuration parameters for pdev (radio)
 * @rx_dma_buf_ring_size: Size of RxDMA buffer ring
 * @dma_mon_buf_ring_size: Size of RxDMA Monitor buffer ring
 * @dma_rx_mon_dest_ring_size: Size of RxDMA Monitor Destination ring
 * @dma_tx_mon_dest_ring_size: Size of Tx Monitor Destination ring
 * @dma_mon_status_ring_size: Size of RxDMA Monitor Status ring
 * @rxdma_monitor_desc_ring: rxdma monitor desc ring size
 * @num_mac_rings: Number of mac rings
 * @nss_enabled: 1 - NSS enabled, 0 - NSS disabled
 * @dma_tx_mon_buf_ring_size: Tx monitor BUF Ring size
 */
struct wlan_cfg_dp_pdev_ctxt {
	int rx_dma_buf_ring_size;
	int dma_mon_buf_ring_size;
	int dma_rx_mon_dest_ring_size;
	int dma_tx_mon_dest_ring_size;
	int dma_mon_status_ring_size;
	int rxdma_monitor_desc_ring;
	int num_mac_rings;
	int nss_enabled;
	int dma_tx_mon_buf_ring_size;
};

/**
 * struct wlan_dp_prealloc_cfg - DP prealloc related config
 * @num_tx_ring_entries: num of tcl data ring entries
 * @num_tx_comp_ring_entries: num of tx comp ring entries
 * @num_wbm_rel_ring_entries: num of wbm err ring entries
 * @num_rxdma_err_dst_ring_entries: num of rxdma err ring entries
 * @num_reo_exception_ring_entries: num of rx exception ring entries
 * @num_tx_desc: num of tx descriptors
 * @num_tx_ext_desc: num of tx ext descriptors
 * @num_reo_dst_ring_entries: Number of entries in REO destination ring
 * @num_rxdma_buf_ring_entries: Number of entries in rxdma buf ring
 * @num_rxdma_refill_ring_entries: Number of entries in rxdma refill ring
 * @num_reo_status_ring_entries: Number of entries in REO status ring
 * @num_mon_status_ring_entries: Number of entries in monitor status ring
 */
struct wlan_dp_prealloc_cfg {
	int num_tx_ring_entries;
	int num_tx_comp_ring_entries;
	int num_wbm_rel_ring_entries;
	int num_rxdma_err_dst_ring_entries;
	int num_reo_exception_ring_entries;
	int num_tx_desc;
	int num_tx_ext_desc;
	int num_reo_dst_ring_entries;
	int num_rxdma_buf_ring_entries;
	int num_rxdma_refill_ring_entries;
	int num_reo_status_ring_entries;
	int num_mon_status_ring_entries;
};

/**
 * wlan_cfg_soc_attach() - Attach configuration interface for SoC
 * @psoc: PSOC object
 *
 * Allocates context for Soc configuration parameters,
 * Read configuration information from device tree/ini file and
 * returns back handle
 *
 * Return: Handle to configuration context
 */
struct wlan_cfg_dp_soc_ctxt *
wlan_cfg_soc_attach(struct cdp_ctrl_objmgr_psoc *psoc);

/**
 * wlan_cfg_soc_detach() - Detach soc configuration handle
 * @wlan_cfg_ctx: soc configuration handle
 *
 * De-allocates memory allocated for SoC configuration
 *
 * Return:none
 */
void wlan_cfg_soc_detach(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_pdev_attach() - Attach configuration interface for pdev
 * @ctrl_obj: PSOC object
 *
 * Allocates context for pdev configuration parameters,
 * Read configuration information from device tree/ini file and
 * returns back handle
 *
 * Return: Handle to configuration context
 */
struct wlan_cfg_dp_pdev_ctxt *
wlan_cfg_pdev_attach(struct cdp_ctrl_objmgr_psoc *ctrl_obj);

/**
 * wlan_cfg_pdev_detach() - Detach and free pdev configuration handle
 * @wlan_cfg_pdev_ctx: PDEV Configuration Handle
 *
 * Return: void
 */
void wlan_cfg_pdev_detach(struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

void wlan_cfg_set_num_contexts(struct wlan_cfg_dp_soc_ctxt *cfg, int num);
void wlan_cfg_set_tx_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
			       int context, int mask);
void wlan_cfg_set_rx_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
			       int context, int mask);
void wlan_cfg_set_rx_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				   int context, int mask);
void wlan_cfg_set_ce_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
			       int context, int mask);
void wlan_cfg_set_rxbuf_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg, int context,
				  int mask);
void wlan_cfg_set_max_peer_id(struct wlan_cfg_dp_soc_ctxt *cfg, uint32_t val);
void wlan_cfg_set_max_ast_idx(struct wlan_cfg_dp_soc_ctxt *cfg, uint32_t val);
int wlan_cfg_get_max_ast_idx(struct wlan_cfg_dp_soc_ctxt *cfg);
int wlan_cfg_get_mon_drop_thresh(struct wlan_cfg_dp_soc_ctxt *cfg);
int wlan_cfg_set_rx_err_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				int context, int mask);
int wlan_cfg_set_rx_wbm_rel_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context, int mask);
int wlan_cfg_set_reo_status_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context, int mask);

/**
 * wlan_cfg_set_mon_delayed_replenish_entries() - number of buffers to replenish
 *				for monitor buffer ring at initialization
 * @wlan_cfg_ctx: Configuration Handle
 * @replenish_entries: number of entries to replenish at initialization
 *
 */
void wlan_cfg_set_mon_delayed_replenish_entries(struct wlan_cfg_dp_soc_ctxt
						*wlan_cfg_ctx,
						uint32_t replenish_entries);

/**
 * wlan_cfg_get_mon_delayed_replenish_entries() - get num of buffer to replenish
 *				for monitor buffer ring at initialization
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: delayed_replenish_entries;
 */
int wlan_cfg_get_mon_delayed_replenish_entries(struct wlan_cfg_dp_soc_ctxt
					       *wlan_cfg_ctx);
/**
 * wlan_cfg_get_num_contexts() - Number of interrupt contexts to be registered
 * @wlan_cfg_ctx: Configuration Handle
 *
 * For WIN,  DP_NUM_INTERRUPT_CONTEXTS will be equal to  number of CPU cores.
 * Each context (for linux it is a NAPI context) will have a tx_ring_mask,
 * rx_ring_mask ,and rx_monitor_ring mask  to indicate the rings
 * that are processed by the handler.
 *
 * Return: num_contexts
 */
int wlan_cfg_get_num_contexts(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_get_tx_ring_mask() - Return Tx interrupt mask mapped to an
 *				 interrupt context
 * @wlan_cfg_ctx: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_tx_ring_mask[context]
 */
int wlan_cfg_get_tx_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_get_tcl_wbm_ring_num_for_index() - Get TCL/WBM ring number for index
 * @wlan_cfg_ctx: Configuration Handle
 * @index: index for which TCL/WBM ring numbers are needed
 * @tcl: pointer to TCL ring number, to be filled
 * @wbm: pointer to WBM ring number to be filled
 *
 * The function fills in tcl/wbm input pointers with TCL/WBM ring numbers for a
 * given index corresponding to soc->tcl_data_ring or soc->tx_comp_ring. This
 * is needed since WBM/TCL rings may not be sequentially available for HOST
 * to use. The function returns values as stored in tcl_wbm_map_array global
 * array.
 *
 * Return: None
 */
static inline
void wlan_cfg_get_tcl_wbm_ring_num_for_index(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
					     int index, int *tcl, int *wbm)
{
	*tcl = wlan_cfg_ctx->tcl_wbm_map_array[index].tcl_ring_num;
	*wbm = wlan_cfg_ctx->tcl_wbm_map_array[index].wbm_ring_num;
}

/**
 * wlan_cfg_get_wbm_ring_num_for_index() - Get WBM ring number for index
 * @wlan_cfg_ctx: Configuration Handle
 * @index: index for which WBM ring numbers is needed
 *
 * Return: WBM Ring number for the index
 */
static inline
int wlan_cfg_get_wbm_ring_num_for_index(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
					int index)
{
	return wlan_cfg_ctx->tcl_wbm_map_array[index].wbm_ring_num;
}

/**
 * wlan_cfg_get_rbm_id_for_index() - Get WBM RBM ID for TX ring index
 * @wlan_cfg_ctx: Configuration Handle
 * @index: TCL index for which WBM rbm value is needed
 *
 * The function fills in wbm rbm value corresponding to a TX ring index in
 * soc->tcl_data_ring. This is needed since WBM ring numbers donot map
 * sequentially to wbm rbm values.
 * The function returns rbm id values as stored in tcl_wbm_map_array global
 * array.
 *
 * Return: WBM rbm value corresnponding to TX ring index
 */
static inline
int wlan_cfg_get_rbm_id_for_index(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx, int index)
{
	return wlan_cfg_ctx->tcl_wbm_map_array[index].wbm_rbm_id;
}

/**
 * wlan_cfg_get_rx_ring_mask() - Return Rx interrupt mask mapped to an
 *				 interrupt context
 * @wlan_cfg_ctx: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rx_ring_mask[context]
 */
int wlan_cfg_get_rx_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_get_rx_mon_ring_mask() - Return Rx monitor ring interrupt mask
 *				   mapped to an interrupt context
 * @wlan_cfg_ctx: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rx_mon_ring_mask[context]
 */
int wlan_cfg_get_rx_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_set_tx_mon_ring_mask() - Set Tx monitor ring interrupt mask
 *				   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 * @mask: Interrupt mask
 *
 * Return: None
 */
void wlan_cfg_set_tx_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				   int context, int mask);

/**
 * wlan_cfg_get_tx_mon_ring_mask() - Return Tx monitor ring interrupt mask
 *				   mapped to an interrupt context
 * @wlan_cfg_ctx: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_tx_mon_ring_mask[context]
 */
int wlan_cfg_get_tx_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
				  int context);

/**
 * wlan_cfg_set_rxdma2host_ring_mask() - Set rxdma2host ring interrupt mask
 *				   for the given interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 * @mask: Interrupt mask
 *
 */
void wlan_cfg_set_rxdma2host_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				       int context, int mask);

/**
 * wlan_cfg_get_rxdma2host_ring_mask() - Return rxdma2host ring interrupt mask
 *				   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rxdma2host_ring_mask[context]
 */
int wlan_cfg_get_rxdma2host_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				      int context);

/**
 * wlan_cfg_set_host2rxdma_ring_mask() - Set host2rxdma ring interrupt mask
 *				   for the given interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 * @mask: Interrupt mask
 *
 */
void wlan_cfg_set_host2rxdma_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				       int context, int mask);

/**
 * wlan_cfg_get_host2rxdma_ring_mask() - Return host2rxdma ring interrupt mask
 *				   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_host2rxdma_ring_mask[context]
 */
int wlan_cfg_get_host2rxdma_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				      int context);

/**
 * wlan_cfg_get_rx_near_full_grp_1_mask() - Return REO near full interrupt mask
 *					mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: REO near full interrupt mask[context]
 */
int wlan_cfg_get_rx_near_full_grp_1_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					 int context);

/**
 * wlan_cfg_get_rx_near_full_grp_2_mask() - Return REO near full interrupt mask
 *					mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: REO near full interrupt mask[context]
 */
int wlan_cfg_get_rx_near_full_grp_2_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					 int context);

/**
 * wlan_cfg_get_tx_ring_near_full_mask() - Return tx completion ring near full
 *				interrupt mask mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: tx completion near full interrupt mask[context]
 */
int wlan_cfg_get_tx_ring_near_full_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context);
/**
 * wlan_cfg_set_host2rxdma_mon_ring_mask() - Set host2rxdma monitor ring
 *                                interrupt mask for the given interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 * @mask: Interrupt mask
 *
 */
void wlan_cfg_set_host2rxdma_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					   int context, int mask);

/**
 * wlan_cfg_get_host2rxdma_mon_ring_mask() - Return host2rxdma monitoe ring
 *                               interrupt mask mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_host2rxdma_mon_ring_mask[context]
 */
int wlan_cfg_get_host2rxdma_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					  int context);

/**
 * wlan_cfg_set_rxdma2host_mon_ring_mask() - Set rxdma2host monitor
 *				   destination ring interrupt mask
 *				   for the given interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 * @mask: Interrupt mask
 *
 */
void wlan_cfg_set_rxdma2host_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					   int context, int mask);

/**
 * wlan_cfg_get_rxdma2host_mon_ring_mask() - Return rxdma2host monitor
 *				   destination ring interrupt mask
 *				   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rxdma2host_mon_ring_mask[context]
 */
int wlan_cfg_get_rxdma2host_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					  int context);

/**
 * wlan_cfg_set_hw_mac_idx() - Set HW MAC Idx for the given PDEV index
 *
 * @cfg: Configuration Handle
 * @pdev_idx: Index of SW PDEV
 * @hw_macid: HW MAC Id
 *
 */
void wlan_cfg_set_hw_mac_idx(struct wlan_cfg_dp_soc_ctxt *cfg,
			     int pdev_idx, int hw_macid);

/**
 * wlan_cfg_get_hw_mac_idx() - Get 0 based HW MAC index for the given
 * PDEV index
 *
 * @cfg: Configuration Handle
 * @pdev_idx: Index of SW PDEV
 *
 * Return: HW MAC index
 */
int wlan_cfg_get_hw_mac_idx(struct wlan_cfg_dp_soc_ctxt *cfg, int pdev_idx);

/**
 * wlan_cfg_get_target_pdev_id() - Get target PDEV ID for HW MAC ID
 *
 * @cfg: Configuration Handle
 * @hw_macid: Index of hw mac
 *
 * Return: PDEV ID
 */
int
wlan_cfg_get_target_pdev_id(struct wlan_cfg_dp_soc_ctxt *cfg, int hw_macid);

/**
 * wlan_cfg_set_pdev_idx() - Set 0 based host PDEV index for the given
 * hw mac index
 *
 * @cfg: Configuration Handle
 * @pdev_idx: Index of SW PDEV
 * @hw_macid: Index of hw mac
 *
 * Return: PDEV index
 */
void wlan_cfg_set_pdev_idx
	(struct wlan_cfg_dp_soc_ctxt *cfg, int pdev_idx, int hw_macid);

/**
 * wlan_cfg_get_pdev_idx() - Get 0 based PDEV index for the given
 * hw mac index
 *
 * @cfg: Configuration Handle
 * @hw_macid: Index of hw mac
 *
 * Return: PDEV index
 */
int wlan_cfg_get_pdev_idx(struct wlan_cfg_dp_soc_ctxt *cfg, int hw_macid);

/**
 * wlan_cfg_get_rx_err_ring_mask() - Return Rx monitor ring interrupt mask
 *					   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rx_err_ring_mask[context]
 */
int wlan_cfg_get_rx_err_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg, int
				  context);

/**
 * wlan_cfg_get_rx_wbm_rel_ring_mask() - Return Rx monitor ring interrupt mask
 *					   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_wbm_rel_ring_mask[context]
 */
int wlan_cfg_get_rx_wbm_rel_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg, int
				      context);

/**
 * wlan_cfg_get_reo_status_ring_mask() - Return Rx monitor ring interrupt mask
 *					   mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_reo_status_ring_mask[context]
 */
int wlan_cfg_get_reo_status_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg, int
				      context);

/**
 * wlan_cfg_get_ce_ring_mask() - Return CE ring interrupt mask
 *				mapped to an interrupt context
 * @wlan_cfg_ctx: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_ce_ring_mask[context]
 */
int wlan_cfg_get_ce_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_get_umac_reset_intr_mask() - Get UMAC reset interrupt mask
 * mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_umac_reset_intr_mask[context]
 */
int wlan_cfg_get_umac_reset_intr_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				      int context);
/**
 * wlan_cfg_get_max_clients() - Return maximum number of peers/stations
 *				supported by device
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: max_clients
 */
uint32_t wlan_cfg_get_max_clients(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_max_alloc_size() - Return Maximum allocation size for any dynamic
 *			    memory allocation request for this device
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: max_alloc_size
 */
uint32_t wlan_cfg_max_alloc_size(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_per_pdev_tx_ring() - Return true if Tx rings are mapped as
 *			       one per radio
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: per_pdev_tx_ring
 */
int wlan_cfg_per_pdev_tx_ring(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_num_tx_comp_rings() - Number of Tx comp rings (HOST mode)
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_comp_rings
 */
int wlan_cfg_num_tx_comp_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_num_tcl_data_rings() - Number of TCL Data rings (HOST mode)
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tcl_data_rings
 */
int wlan_cfg_num_tcl_data_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_num_nss_tcl_data_rings() - Number of TCL Data rings (NSS offload)
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tcl_data_rings
 */
int wlan_cfg_num_nss_tcl_data_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_per_pdev_rx_ring() - Return true if Rx rings are mapped as
 *                              one per radio
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: per_pdev_rx_ring
 */
int wlan_cfg_per_pdev_rx_ring(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_per_pdev_lmac_ring() - Return true if error rings are mapped as
 *                              one per radio
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: return 1 if per pdev error ring else 0
 */
int wlan_cfg_per_pdev_lmac_ring(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_num_reo_dest_rings() - Number of REO Data rings (HOST mode)
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_reo_dest_rings
 */
int wlan_cfg_num_reo_dest_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_num_nss_reo_dest_rings() - Number of REO Data rings (NSS offload)
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_reo_dest_rings
 */
int wlan_cfg_num_nss_reo_dest_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_pkt_type() - Default 802.11 encapsulation type
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: htt_pkt_type_ethernet
 */
int wlan_cfg_pkt_type(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_get_num_tx_desc_pool() - Number of Tx Descriptor pools for the
 *					device
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_desc_pool
 */
int wlan_cfg_get_num_tx_desc_pool(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_set_num_tx_desc_pool() - Set the number of Tx Descriptor pools for the
 *					device
 * @cfg: Configuration Handle
 * @num_pool: Number of pool
 */
void wlan_cfg_set_num_tx_desc_pool(struct wlan_cfg_dp_soc_ctxt *cfg, int num_pool);

/**
 * wlan_cfg_get_num_tx_ext_desc_pool() -  Number of Tx MSDU ext Descriptor
 *					pools
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_ext_desc_pool
 */
int wlan_cfg_get_num_tx_ext_desc_pool(
		struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_get_reo_dst_ring_size() - Get REO destination ring size
 *
 * @cfg: Configuration Handle
 *
 * Return: reo_dst_ring_size
 */
int wlan_cfg_get_reo_dst_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_reo_dst_ring_size() - Set the REO Destination ring size
 *
 * @cfg: Configuration Handle
 * @reo_dst_ring_size: REO Destination ring size
 */
void wlan_cfg_set_reo_dst_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg,
				    int reo_dst_ring_size);

/**
 * wlan_cfg_set_raw_mode_war() - Set raw mode war configuration
 *
 * @cfg: Configuration Handle
 * @raw_mode_war: raw mode war configuration
 */
void wlan_cfg_set_raw_mode_war(struct wlan_cfg_dp_soc_ctxt *cfg,
			       bool raw_mode_war);

/**
 * wlan_cfg_get_raw_mode_war() - Get raw mode war configuration
 *
 * @cfg: Configuration Handle
 *
 * Return: reo_dst_ring_size
 */
bool wlan_cfg_get_raw_mode_war(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_num_tx_ext_desc_pool() -  Set the number of Tx MSDU ext Descriptor
 *					pools
 * @cfg: Configuration Handle
 * @num_pool: Number of pool
 */
void wlan_cfg_set_num_tx_ext_desc_pool(struct wlan_cfg_dp_soc_ctxt *cfg, int num_pool);

/**
 * wlan_cfg_get_num_tx_desc() - Number of Tx Descriptors per pool
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_desc
 */
int wlan_cfg_get_num_tx_desc(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_get_num_tx_spl_desc() - Number of Tx Descriptors for special
 *				    frames per pool
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_desc
 */
int wlan_cfg_get_num_tx_spl_desc(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_get_min_tx_desc() - Minimum number of Tx Descriptors per pool
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_desc
 */
int wlan_cfg_get_min_tx_desc(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_set_num_tx_desc() - Set the number of Tx Descriptors per pool
 *
 * @cfg: Configuration Handle
 * @num_desc: Number of descriptor
 */
void wlan_cfg_set_num_tx_desc(struct wlan_cfg_dp_soc_ctxt *cfg, int num_desc);

/**
 * wlan_cfg_get_num_tx_ext_desc() - Number of Tx MSDU extension Descriptors
 *					per pool
 * @wlan_cfg_ctx: Configuration Handle
 *
 * Return: num_tx_ext_desc
 */
int wlan_cfg_get_num_tx_ext_desc(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_set_num_tx_ext_desc() - Set the number of Tx MSDU extension
 *                                  Descriptors per pool
 * @cfg: Configuration Handle
 * @num_ext_desc: Number of descriptor
 */
void wlan_cfg_set_num_tx_ext_desc(struct wlan_cfg_dp_soc_ctxt *cfg,
				  int num_ext_desc);

/**
 * wlan_cfg_max_peer_id() - Get maximum peer ID
 * @cfg: Configuration Handle
 *
 * Return: maximum peer ID
 */
uint32_t wlan_cfg_max_peer_id(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dma_mon_buf_ring_size(): Return Size of monitor buffer ring
 * @wlan_cfg_pdev_ctx: pdev configuration context
 *
 * Return: dma_mon_buf_ring_size
 */
int wlan_cfg_get_dma_mon_buf_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/**
 * wlan_cfg_get_dma_mon_stat_ring_size() - Return size of Monitor Status ring
 * @wlan_cfg_pdev_ctx: pdev configuration context
 *
 * Return: dma_mon_stat_ring_size
 */
int wlan_cfg_get_dma_mon_stat_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/**
 * wlan_cfg_get_dma_mon_desc_ring_size - Get rxdma monitor size
 * @cfg: soc configuration context
 *
 * Return: rxdma monitor desc ring size
 */
int
wlan_cfg_get_dma_mon_desc_ring_size(struct wlan_cfg_dp_pdev_ctxt *cfg);

/**
 * wlan_cfg_get_rx_dma_buf_ring_size() - Return Size of RxDMA buffer ring
 * @wlan_cfg_pdev_ctx: pdev configuration context
 *
 * Return: rx_dma_buf_ring_size
 */
int wlan_cfg_get_rx_dma_buf_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/**
 * wlan_cfg_rx_pending_hl_threshold() - Return high threshold of rx pending
 * @cfg: pdev configuration context
 *
 * Return: rx_pending_high_threshold
 */
uint32_t
wlan_cfg_rx_pending_hl_threshold(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_rx_pending_lo_threshold() - Return low threshold of rx pending
 * @cfg: soc configuration context
 *
 * Return: rx_pending_low_threshold
 */
uint32_t
wlan_cfg_rx_pending_lo_threshold(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_num_mac_rings() - Return the number of MAC RX DMA rings
 * per pdev
 * @cfg: pdev configuration context
 *
 * Return: number of mac DMA rings per pdev
 */
int wlan_cfg_get_num_mac_rings(struct wlan_cfg_dp_pdev_ctxt *cfg);

/**
 * wlan_cfg_is_lro_enabled - Return LRO enabled/disabled
 * @cfg: soc configuration context
 *
 * Return: true - LRO enabled false - LRO disabled
 */
bool wlan_cfg_is_lro_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_gro_enabled - Return GRO enabled/disabled
 * @cfg: soc configuration context
 *
 * Return: true - GRO enabled false - GRO disabled
 */
bool wlan_cfg_is_gro_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_rx_hash_enabled - Return RX hash enabled/disabled
 * @cfg: soc configuration context
 *
 * Return: true - enabled false - disabled
 */
bool wlan_cfg_is_rx_hash_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_ipa_enabled - Return IPA enabled/disabled
 * @cfg: soc configuration context
 *
 * Return: true - enabled false - disabled
 */
bool wlan_cfg_is_ipa_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_rx_hash - set rx hash enabled/disabled
 * @cfg: soc configuration context
 * @rx_hash: true - enabled false - disabled
 */
void wlan_cfg_set_rx_hash(struct wlan_cfg_dp_soc_ctxt *cfg, bool rx_hash);

/**
 * wlan_cfg_get_dp_pdev_nss_enabled - Return pdev nss enabled/disabled
 * @cfg: pdev configuration context
 *
 * Return: 1 - enabled 0 - disabled
 */
int wlan_cfg_get_dp_pdev_nss_enabled(struct wlan_cfg_dp_pdev_ctxt *cfg);

/**
 * wlan_cfg_set_dp_pdev_nss_enabled - set pdev nss enabled/disabled
 * @cfg: pdev configuration context
 * @nss_enabled: 1 - enabled 0 - disabled
 */
void wlan_cfg_set_dp_pdev_nss_enabled(struct wlan_cfg_dp_pdev_ctxt *cfg,
				      int nss_enabled);

/**
 * wlan_cfg_get_dp_soc_nss_cfg - Return soc nss config
 * @cfg: soc configuration context
 *
 * Return: nss_cfg
 */
int wlan_cfg_get_dp_soc_nss_cfg(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_dp_soc_nss_cfg - set soc nss config
 * @cfg: soc configuration context
 * @nss_cfg: NSS configuration
 *
 */
void wlan_cfg_set_dp_soc_nss_cfg(struct wlan_cfg_dp_soc_ctxt *cfg, int nss_cfg);

/**
 * wlan_cfg_get_int_timer_threshold_ppe2tcl - Get intr mitigation for ppe2tcl
 * @cfg: soc configuration context
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_ppe2tcl(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_batch_threshold_ppe2tcl - Get intr mitigation for ppe2tcl
 * @cfg: soc configuration context
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_ppe2tcl(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_batch_threshold_tx - Get interrupt mitigation cfg for Tx
 * @cfg: soc configuration context
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_tx(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_timer_threshold_tx - Get interrupt mitigation cfg for Tx
 * @cfg: soc configuration context
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_tx(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_batch_threshold_rx - Get interrupt mitigation cfg for Rx
 * @cfg: soc configuration context
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_rx(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_timer_threshold_rx - Get interrupt mitigation cfg for Rx
 * @cfg: soc configuration context
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_rx(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_batch_threshold_other - Get interrupt mitigation cfg for
 *                                          other srngs
 * @cfg: soc configuration context
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_other(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_timer_threshold_other - Get interrupt mitigation cfg for
 *                                          other srngs
 * @cfg: soc configuration context
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_other(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_int_timer_threshold_mon - Get int mitigation cfg for mon srngs
 * @cfg: soc configuration context
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_mon(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_checksum_offload - Get checksum offload enable or disable status
 * @cfg: soc configuration context
 *
 * Return: Checksum offload enable or disable
 */
int wlan_cfg_get_checksum_offload(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_nan_checksum_offload - Get checksum offload enable/disable val
 * @cfg: soc configuration context
 *
 * Return: Checksum offload enable or disable value for NAN mode
 */
int wlan_cfg_get_nan_checksum_offload(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_p2p_checksum_offload - Get checksum offload enable/disable val
 * @cfg: soc configuration context
 *
 * Return: Checksum offload enable or disable value for P2P mode
 */
int wlan_cfg_get_p2p_checksum_offload(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_tx_ring_size - Get Tx DMA ring size (TCL Data Ring)
 * @cfg: soc configuration context
 *
 * Return: Tx Ring Size
 */
int wlan_cfg_tx_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_time_control_bp - Get time for interval in bp prints
 * @cfg: soc configuration context
 *
 * Return: interval time
 */
int wlan_cfg_time_control_bp(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_tx_comp_ring_size - Get Tx completion ring size (WBM Ring)
 * @cfg: soc configuration context
 *
 * Return: Tx Completion ring size
 */
int wlan_cfg_tx_comp_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_wbm_release_ring_size - Get wbm_release_ring size
 * @cfg: soc configuration context
 *
 * Return: wbm_release_ring size
 */
int
wlan_cfg_get_dp_soc_wbm_release_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tcl_cmd_credit_ring_size - Get command/credit ring size
 * @cfg: soc configuration context
 *
 * Return: tcl_cmd_credit_ring size
 */
int
wlan_cfg_get_dp_soc_tcl_cmd_credit_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tcl_status_ring_size - Get tcl_status_ring size
 * @cfg: soc configuration context
 *
 * Return: tcl_status_ring size
 */
int
wlan_cfg_get_dp_soc_tcl_status_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_reo_reinject_ring_size - Get reo_reinject_ring size
 * @cfg: soc configuration context
 *
 * Return: reo_reinject_ring size
 */
int
wlan_cfg_get_dp_soc_reo_reinject_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_rx_release_ring_size - Get rx_release_ring size
 * @cfg: soc configuration context
 *
 * Return: rx_release_ring size
 */
int
wlan_cfg_get_dp_soc_rx_release_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_reo_exception_ring_size - Get reo_exception_ring size
 * @cfg: soc configuration context
 *
 * Return: reo_exception_ring size
 */
int
wlan_cfg_get_dp_soc_reo_exception_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_reo_cmd_ring_size - Get reo_cmd_ring size
 * @cfg: soc configuration context
 *
 * Return: reo_cmd_ring size
 */
int
wlan_cfg_get_dp_soc_reo_cmd_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_reo_status_ring_size - Get reo_status_ring size
 * @cfg: soc configuration context
 *
 * Return: reo_status_ring size
 */
int
wlan_cfg_get_dp_soc_reo_status_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_desc_limit_0 - Get tx desc limit for 5G H
 * @cfg: soc configuration context
 *
 * Return: tx desc limit for 5G H
 */
int
wlan_cfg_get_dp_soc_tx_desc_limit_0(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_desc_limit_1 - Get tx desc limit for 2G
 * @cfg: soc configuration context
 *
 * Return: tx desc limit for 2G
 */
int
wlan_cfg_get_dp_soc_tx_desc_limit_1(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_desc_limit_2 - Get tx desc limit for 5G L
 * @cfg: soc configuration context
 *
 * Return: tx desc limit for 5G L
 */
int
wlan_cfg_get_dp_soc_tx_desc_limit_2(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_device_limit - Get tx device limit
 * @cfg: soc configuration context
 *
 * Return: tx device limit
 */
int
wlan_cfg_get_dp_soc_tx_device_limit(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_spl_device_limit - Get tx device limit for special
 *					     frames
 * @cfg: Configuration Handle
 *
 * Return: tx device limit for special frames
 */
int
wlan_cfg_get_dp_soc_tx_spl_device_limit(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_sw_internode_queue - Get tx sw internode queue
 * @cfg: soc configuration context
 *
 * Return: tx sw internode queue
 */
int
wlan_cfg_get_dp_soc_tx_sw_internode_queue(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_rxdma_refill_ring_size - Get rxdma refill ring size
 * @cfg: soc configuration context
 *
 * Return: rxdma refill ring size
 */
int
wlan_cfg_get_dp_soc_rxdma_refill_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_rxdma_refill_lt_disable - Get RxDMA refill LT status
 * @cfg: soc configuration context
 *
 * Return: true if Low threshold disable else false
 */
bool
wlan_cfg_get_dp_soc_rxdma_refill_lt_disable(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_rxdma_err_dst_ring_size - Get rxdma dst ring size
 * @cfg: soc configuration context
 *
 * Return: rxdma error dst ring size
 */
int
wlan_cfg_get_dp_soc_rxdma_err_dst_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_rx_sw_desc_weight - Get rx sw desc weight
 * @cfg: soc configuration context
 *
 * Return: rx_sw_desc_weight
 */
int
wlan_cfg_get_dp_soc_rx_sw_desc_weight(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_rx_sw_desc_num - Get rx sw desc num
 * @cfg: soc configuration context
 *
 * Return: rx_sw_desc_num
 */
int
wlan_cfg_get_dp_soc_rx_sw_desc_num(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_caps - Get dp capabilities
 * @cfg: soc configuration context
 * @dp_caps: enum for dp capabilities
 *
 * Return: bool if a dp capabilities is enabled
 */
bool
wlan_cfg_get_dp_caps(struct wlan_cfg_dp_soc_ctxt *cfg,
		     enum cdp_capabilities dp_caps);

/**
 * wlan_set_srng_cfg() - Fill per ring specific
 * configuration parameters
 * @wlan_cfg: global srng configuration table
 *
 * Return: None
 */
void wlan_set_srng_cfg(struct wlan_srng_cfg **wlan_cfg);

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
int wlan_cfg_get_tx_flow_stop_queue_th(struct wlan_cfg_dp_soc_ctxt *cfg);

int wlan_cfg_get_tx_flow_start_queue_offset(struct wlan_cfg_dp_soc_ctxt *cfg);
#endif /* QCA_LL_TX_FLOW_CONTROL_V2 */
int wlan_cfg_get_rx_defrag_min_timeout(struct wlan_cfg_dp_soc_ctxt *cfg);

int wlan_cfg_get_defrag_timeout_check(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_rx_flow_search_table_size() - Return the size of Rx FST
 *                                            in number of entries
 *
 * @cfg: soc configuration context
 *
 * Return: rx_fst_size
 */
uint16_t
wlan_cfg_get_rx_flow_search_table_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_rx_fst_get_max_search() - Return the max skid length for FST search
 *
 * @cfg: soc configuration context
 *
 * Return: max_search
 */
uint8_t wlan_cfg_rx_fst_get_max_search(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_rx_fst_get_hash_key() - Return Toeplitz Hash Key used for FST
 *                                  search
 *
 * @cfg: soc configuration context
 *
 * Return: 320-bit Hash Key
 */
uint8_t *wlan_cfg_rx_fst_get_hash_key(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_rx_flow_tag_enabled() - set rx flow tag enabled flag in
 *                                      DP soc context
 * @cfg: soc configuration context
 * @val: Rx flow tag feature flag value
 *
 * Return: None
 */
void wlan_cfg_set_rx_flow_tag_enabled(struct wlan_cfg_dp_soc_ctxt *cfg,
				      bool val);

/**
 * wlan_cfg_is_rx_flow_tag_enabled() - get rx flow tag enabled flag from
 *                                     DP soc context
 * @cfg: soc configuration context
 *
 * Return: true if feature is enabled, else false
 */
bool wlan_cfg_is_rx_flow_tag_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_rx_flow_search_table_per_pdev() - Set flag to indicate that
 *                                                Rx FST is per pdev
 * @cfg: soc configuration context
 * @val: boolean flag indicating Rx FST per pdev or per SOC
 *
 * Return: None
 */
void
wlan_cfg_set_rx_flow_search_table_per_pdev(struct wlan_cfg_dp_soc_ctxt *cfg,
					   bool val);

/**
 * wlan_cfg_is_rx_flow_search_table_per_pdev() - get RX FST flag for per pdev
 * @cfg: soc configuration context
 *
 * Return: true if Rx FST is per pdev, else false
 */
bool
wlan_cfg_is_rx_flow_search_table_per_pdev(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_rx_flow_search_table_size() - set RX FST size in DP SoC context
 * @cfg: soc configuration context
 * @val: Rx FST size in number of entries
 *
 * Return: None
 */
void
wlan_cfg_set_rx_flow_search_table_size(struct wlan_cfg_dp_soc_ctxt *cfg,
				       uint16_t val);

/**
 * wlan_cfg_set_rx_mon_protocol_flow_tag_enabled() - set mon rx tag enabled flag
 *                                                   in DP soc context
 * @cfg: soc configuration context
 * @val: Rx protocol or flow tag feature flag value in monitor mode from INI
 *
 * Return: None
 */
void
wlan_cfg_set_rx_mon_protocol_flow_tag_enabled(struct wlan_cfg_dp_soc_ctxt *cfg,
					      bool val);

/**
 * wlan_cfg_is_rx_mon_protocol_flow_tag_enabled() - get mon rx tag enabled flag
 *                                                  from DP soc context
 * @cfg: soc configuration context
 *
 * Return: true if feature is enabled in monitor mode for protocol or flow
 * tagging in INI, false otherwise
 */
bool
wlan_cfg_is_rx_mon_protocol_flow_tag_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_tx_per_pkt_vdev_id_check() - set flag to enable perpkt
 *                                              vdev id check in tx.
 * @cfg: soc configuration context
 * @val: feature flag value
 *
 * Return: None
 */
void
wlan_cfg_set_tx_per_pkt_vdev_id_check(struct wlan_cfg_dp_soc_ctxt *cfg,
				      bool val);

/**
 * wlan_cfg_is_tx_per_pkt_vdev_id_check_enabled() - get flag to check if
 *                              perpkt vdev id check is enabled in tx.
 * @cfg: soc configuration context
 *
 * Return: true if feature is enabled, false otherwise
 */
bool
wlan_cfg_is_tx_per_pkt_vdev_id_check_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_fill_interrupt_mask() - set interrupt mask
 *
 * @wlan_cfg_ctx: soc configuration context
 * @num_dp_msi: Number of DP interrupts available (0 for integrated)
 * @interrupt_mode: Type of interrupt
 * @is_monitor_mode: is monitor mode enabled
 *
 * Return: void
 */
void wlan_cfg_fill_interrupt_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
				  int num_dp_msi, int interrupt_mode,
				  bool is_monitor_mode);

/**
 * wlan_cfg_is_rx_fisa_enabled() - Get Rx FISA enabled flag
 *
 *
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool wlan_cfg_is_rx_fisa_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_rx_fisa_lru_del_enabled() - Get Rx FISA LRU del enabled flag
 *
 *
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool wlan_cfg_is_rx_fisa_lru_del_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_rx_buffer_pool_enabled() - Get RX buffer pool enabled flag
 *
 *
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool wlan_cfg_is_rx_buffer_pool_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_rx_refill_buffer_pool_enabled() - Get RX refill buffer pool enabled flag
 *
 *
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool wlan_cfg_is_rx_refill_buffer_pool_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);


void wlan_cfg_set_tso_desc_attach_defer(struct wlan_cfg_dp_soc_ctxt *cfg,
					bool val);

bool wlan_cfg_is_tso_desc_attach_defer(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_reo_rings_mapping() - Get Reo destination ring bitmap
 *
 *
 * @cfg: soc configuration context
 *
 * Return: reo ring bitmap.
 */
uint32_t wlan_cfg_get_reo_rings_mapping(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_peer_ext_stats() - set peer extended stats
 *
 * @cfg: soc configuration context
 * @val: Flag value read from INI
 *
 * Return: void
 */
void
wlan_cfg_set_peer_ext_stats(struct wlan_cfg_dp_soc_ctxt *cfg,
			    bool val);

/**
 * wlan_cfg_set_peer_jitter_stats() - set peer jitter stats
 *
 * @cfg: soc configuration context
 * @val: Flag value read from INI
 *
 * Return: bool
 */
void
wlan_cfg_set_peer_jitter_stats(struct wlan_cfg_dp_soc_ctxt *cfg,
			       bool val);

/**
 * wlan_cfg_is_peer_ext_stats_enabled() - Check if peer extended
 *                                        stats are enabled
 *
 * @cfg: soc configuration context
 *
 * Return: bool
 */
bool
wlan_cfg_is_peer_ext_stats_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_peer_jitter_stats_enabled() - check if jitter stats are enabled
 *
 * @cfg: soc configuration context
 *
 * Return: bool
 */
bool
wlan_cfg_is_peer_jitter_stats_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_poll_mode_enabled() - Check if poll mode is enabled
 *
 * @cfg: soc configuration context
 *
 * Return: bool
 */

bool wlan_cfg_is_poll_mode_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_fst_in_cmem_enabled() - Check if FST in CMEM is enabled
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool wlan_cfg_is_fst_in_cmem_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_is_swlm_enabled() - Get SWLMenabled flag
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool wlan_cfg_is_swlm_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

#ifdef IPA_OFFLOAD
/**
 * wlan_cfg_ipa_tx_ring_size - Get Tx DMA ring size (TCL Data Ring)
 * @cfg: dp cfg context
 *
 * Return: IPA Tx Ring Size
 */
uint32_t wlan_cfg_ipa_tx_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_ipa_tx_comp_ring_size - Get Tx completion ring size (WBM Ring)
 * @cfg: dp cfg context
 *
 * Return: IPA Tx Completion ring size
 */
uint32_t wlan_cfg_ipa_tx_comp_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_ipa_tx_alt_ring_size - Get Tx alt DMA ring size (TCL Data Ring)
 * @cfg: dp cfg context
 *
 * Return: IPA Tx alt Ring Size
 */
int wlan_cfg_ipa_tx_alt_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_ipa_tx_alt_comp_ring_size - Get Tx alt comp DMA ring size
 *  (TCL Data Ring)
 * @cfg: dp cfg context
 *
 * Return: IPA Tx alt comp Ring Size
 */
int
wlan_cfg_ipa_tx_alt_comp_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

#else
static inline
uint32_t wlan_cfg_ipa_tx_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}

static inline
uint32_t wlan_cfg_ipa_tx_comp_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}
#endif

/**
 * wlan_cfg_radio0_default_reo_get -  Get Radio0 default REO
 * @cfg: soc configuration context
 *
 * Return: None
 */
uint8_t wlan_cfg_radio0_default_reo_get(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_radio1_default_reo_get - Get Radio1 default REO
 * @cfg: soc configuration context
 *
 * Return: None
 */
uint8_t wlan_cfg_radio1_default_reo_get(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_radio2_default_reo_get() - Get Radio2 default REO
 * @cfg: soc configuration context
 *
 * Return: None
 */
uint8_t wlan_cfg_radio2_default_reo_get(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_rxdma1_enable() - Enable rxdma1
 * @wlan_cfg_ctx: soc configuration context
 *
 * Return: None
 */
void wlan_cfg_set_rxdma1_enable(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_is_delay_mon_replenish() - Get if delayed monitor replenish
 * is enabled
 * @cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
bool
wlan_cfg_is_delay_mon_replenish(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_delay_mon_replenish() - Set delayed monitor replenish
 * @cfg: soc configuration context
 * @val: val to set
 *
 * Return: None
 */
void
wlan_cfg_set_delay_mon_replenish(struct wlan_cfg_dp_soc_ctxt *cfg, bool val);

/**
 * wlan_cfg_dp_soc_ctx_dump() - Dump few DP cfg soc parameters
 * @cfg: soc configuration context
 *
 * Return:
 */
void wlan_cfg_dp_soc_ctx_dump(struct wlan_cfg_dp_soc_ctxt *cfg);

#ifdef WLAN_SUPPORT_PPEDS
/**
 * wlan_cfg_get_dp_soc_is_ppeds_enabled() - API to get ppe enable flag
 * @cfg: Configuration Handle
 *
 * Return: true if ppe is enabled else return false
 */
bool
wlan_cfg_get_dp_soc_is_ppeds_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_reo2ppe_ring_size() - get ppe rx ring size
 * @cfg: Configuration Handle
 *
 * Return: size of reo2ppe ring
 */
int
wlan_cfg_get_dp_soc_reo2ppe_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_ppe2tcl_ring_size() - get ppe tx ring size
 * @cfg: Configuration Handle
 *
 * Return: size of ppe2tcl ring
 */
int
wlan_cfg_get_dp_soc_ppe2tcl_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_ppeds_num_tx_desc() - Number of ppeds tx Descriptors
 * @cfg: Configuration Handle
 *
 * Return: num_tx_desc
 */
int
wlan_cfg_get_dp_soc_ppeds_num_tx_desc(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_ppeds_tx_comp_napi_budget() - ppeds Tx comp napi budget
 * @cfg: Configuration Handle
 *
 * Return: napi budget
 */
int
wlan_cfg_get_dp_soc_ppeds_tx_comp_napi_budget(struct wlan_cfg_dp_soc_ctxt *cfg);
#else
static inline bool
wlan_cfg_get_dp_soc_is_ppeds_enabled(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return false;
}

static inline int
wlan_cfg_get_dp_soc_reo2ppe_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}

static inline int
wlan_cfg_get_dp_soc_ppe2tcl_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}

static inline int
wlan_cfg_get_dp_soc_ppeds_num_tx_desc(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}

static inline int
wlan_cfg_get_dp_soc_ppeds_tx_comp_napi_budget(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}
#endif

/**
 * wlan_cfg_get_prealloc_cfg() - Get dp prealloc related cfg param
 * @ctrl_psoc: PSOC object
 * @cfg: cfg ctx where values will be populated
 *
 * Return: None
 */
void
wlan_cfg_get_prealloc_cfg(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			  struct wlan_dp_prealloc_cfg *cfg);
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * wlan_cfg_get_pkt_capture_mode() - Get packet capture mode config
 * @cfg: config context
 *
 * Return: value of packet capture mode
 */
uint32_t wlan_cfg_get_pkt_capture_mode(struct wlan_cfg_dp_soc_ctxt *cfg);
#else
static inline
uint32_t wlan_cfg_get_pkt_capture_mode(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return 0;
}
#endif

/**
 * wlan_cfg_get_dp_soc_rx_mon_buf_ring_size() - Rx MON buf ring size
 * @cfg:  Configuration Handle
 *
 * Return: Size of Rx MON buf ring size
 */
uint32_t
wlan_cfg_get_dp_soc_rx_mon_buf_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dp_soc_tx_mon_buf_ring_size() - Tx MON buf ring size
 * @cfg:  Configuration Handle
 *
 * Return: Size of Tx MON buf ring size
 */
uint32_t
wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_dma_rx_mon_dest_ring_size() - Rx MON dest ring size
 * @cfg:  Configuration Handle
 *
 * Return: Size of Rx MON dest ring size
 */
int wlan_cfg_get_dma_rx_mon_dest_ring_size(struct wlan_cfg_dp_pdev_ctxt *cfg);

/**
 * wlan_cfg_get_dma_tx_mon_dest_ring_size() - Tx MON dest ring size
 * @cfg:  Configuration Handle
 *
 * Return: Size of Tx MON dest ring size
 */
int wlan_cfg_get_dma_tx_mon_dest_ring_size(struct wlan_cfg_dp_pdev_ctxt *cfg);

/**
 * wlan_cfg_get_rx_rel_ring_id() - get wbm2sw ring id for Rx release ring
 * @cfg: Configuration Handle
 *
 * Return: wbm2sw ring id
 */
uint8_t
wlan_cfg_get_rx_rel_ring_id(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_rx_rel_ring_id() - set wbm2sw ring id for Rx release ring
 * @cfg: soc configuration context
 * @wbm2sw_ring_id: wbm2sw ring id
 *
 * Return: None
 */
void
wlan_cfg_set_rx_rel_ring_id(struct wlan_cfg_dp_soc_ctxt *cfg,
			    uint8_t wbm2sw_ring_id);

/**
 * wlan_cfg_set_vdev_stats_hw_offload_config() - Set hw vdev stats offload
 *						 config
 * @cfg: config context
 * @value: value to be set
 *
 * Return: none
 */
void
wlan_cfg_set_vdev_stats_hw_offload_config(struct wlan_cfg_dp_soc_ctxt *cfg,
					  bool value);

/**
 * wlan_cfg_get_vdev_stats_hw_offload_config() - Get hw vdev stats offload
 *						 config
 * @cfg: config context
 *
 * Return: value of hw vdev stats config
 */
bool
wlan_cfg_get_vdev_stats_hw_offload_config(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_vdev_stats_hw_offload_timer()- Get hw vdev stats timer duration
 * @cfg: config context
 *
 * Return: value of hw vdev stats timer duration
 */
int wlan_cfg_get_vdev_stats_hw_offload_timer(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_sawf_config() - Set SAWF config enable/disable
 * @cfg: config context
 * @value: value to be set
 *
 * Return: none
 */
void
wlan_cfg_set_sawf_config(struct wlan_cfg_dp_soc_ctxt *cfg, bool value);

/**
 * wlan_cfg_get_sawf_config() - Get SAWF config enable/disable
 * @cfg: config context
 *
 * Return: true or false
 */
bool
wlan_cfg_get_sawf_config(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_set_sawf_stats_config() - Set SAWF stats config
 * @cfg: config context
 * @value: value to be set
 *
 * Return: void
 */
void
wlan_cfg_set_sawf_stats_config(struct wlan_cfg_dp_soc_ctxt *cfg, uint8_t value);

/**
 * wlan_cfg_get_sawf_stats_config() - Get SAWF stats config
 * @cfg: config context
 *
 * Return: value for sawf_stats_config
 */
uint8_t
wlan_cfg_get_sawf_stats_config(struct wlan_cfg_dp_soc_ctxt *cfg);

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
/**
 * wlan_cfg_mlo_rx_ring_map_get() - get rx ring map
 * @cfg: soc configuration context
 *
 * Return: rx_ring_map
 */
uint8_t
wlan_cfg_mlo_rx_ring_map_get(struct wlan_cfg_dp_soc_ctxt *cfg);
#endif

/**
 * wlan_cfg_set_host2txmon_ring_mask() - Set host2txmon ring
 *                               interrupt mask mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: interrupt context
 * @mask: interrupt mask
 *
 * Return: None
 */
void wlan_cfg_set_host2txmon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				       int context, int mask);
/**
 * wlan_cfg_get_host2txmon_ring_mask() - Return host2txmon ring
 *                               interrupt mask mapped to an interrupt context
 * @cfg: Configuration Handle
 * @context: Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_host2txmon_ring_mask[context]
 */
int wlan_cfg_get_host2txmon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				      int context);
/**
 * wlan_cfg_set_txmon_hw_support () - Set txmon hw support
 * @cfg:  Configuration Handle
 * @txmon_hw_support: value to set
 *
 * Return: None
 */
void wlan_cfg_set_txmon_hw_support(struct wlan_cfg_dp_soc_ctxt *cfg,
				   bool txmon_hw_support);

/**
 * wlan_cfg_get_txmon_hw_support () - Get txmon hw support
 * @cfg:  Configuration Handle
 *
 * Return: txmon_hw_support
 */
bool wlan_cfg_get_txmon_hw_support(struct wlan_cfg_dp_soc_ctxt *cfg);

void wlan_cfg_set_txmon_sw_peer_filtering(struct wlan_cfg_dp_soc_ctxt *cfg,
					  bool txmon_sw_peer_filtering);
bool wlan_cfg_get_txmon_sw_peer_filtering(struct wlan_cfg_dp_soc_ctxt *cfg);

#ifdef WLAN_TX_PKT_CAPTURE_ENH
/**
 * wlan_cfg_get_tx_capt_max_mem - Get max memory allowed for TX capture feature
 * @cfg: Configuration Handle
 *
 * Return: user given size in bytes
 */
static inline int
wlan_cfg_get_tx_capt_max_mem(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return cfg->tx_capt_max_mem_allowed;
}
#endif /* WLAN_TX_PKT_CAPTURE_ENH */

#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
/**
 * wlan_cfg_get_tx_ilp_inspect_config() - Get TX ILP configuration
 * @cfg: Configuration Handle
 *
 * Return: TX ILP enable or not
 */
static inline bool
wlan_cfg_get_tx_ilp_inspect_config(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return cfg->tx_pkt_inspect_for_ilp;
}
#endif

/**
 * wlan_cfg_get_napi_scale_factor() - Get napi scale factor
 * @cfg: soc configuration context
 *
 * Return: napi scale factor
 */
uint8_t wlan_cfg_get_napi_scale_factor(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_soc_update_tgt_params() - Update band specific params
 * @wlan_cfg_ctx: SOC cfg context
 * @ctrl_obj: PSOC object
 *
 * Return: void
 */
void
wlan_cfg_soc_update_tgt_params(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
			       struct cdp_ctrl_objmgr_psoc *ctrl_obj);

/**
 * wlan_cfg_get_pointer_timer_threshold_rx() - Get timer threshold for RX
 *                                             pointer update
 * @cfg: soc configuration context
 *
 * Return: timer threshold for RX REO Dest ring  pointer update
 */
uint16_t
wlan_cfg_get_pointer_timer_threshold_rx(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_pointer_num_threshold_rx() - Get number threshold for RX
 *                                           pointer update
 * @cfg: soc configuration context
 *
 * Return: entries number threshold for RX REO Dest ring  pointer update
 */
uint8_t
wlan_cfg_get_pointer_num_threshold_rx(struct wlan_cfg_dp_soc_ctxt *cfg);

/**
 * wlan_cfg_get_special_frame_cfg() - Get special frame mask
 * @cfg: soc configuration context
 *
 * Return: frame mask
 */
uint32_t
wlan_cfg_get_special_frame_cfg(struct wlan_cfg_dp_soc_ctxt *cfg);
#endif /*__WLAN_CFG_H*/

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

#ifndef _DP_TYPES_H_
#define _DP_TYPES_H_

#include <qdf_types.h>
#include <qdf_nbuf.h>
#include <qdf_lock.h>
#include <qdf_atomic.h>
#include <qdf_util.h>
#include <qdf_list.h>
#include <qdf_lro.h>
#include <queue.h>
#include <htt_common.h>
#include <htt.h>
#include <htt_stats.h>
#include <cdp_txrx_cmn.h>
#ifdef DP_MOB_DEFS
#include <cds_ieee80211_common.h>
#endif
#include <wdi_event_api.h>    /* WDI subscriber event list */

#include "hal_hw_headers.h"
#include <hal_tx.h>
#include <hal_reo.h>
#include "wlan_cfg.h"
#include "hal_rx.h"
#include <hal_api.h>
#include <hal_api_mon.h>
#include "hal_rx.h"
//#include "hal_rx_flow.h"

#define MAX_BW 8
#define MAX_RETRIES 4
#define MAX_RECEPTION_TYPES 4

#define MINIDUMP_STR_SIZE 25
#ifndef REMOVE_PKT_LOG
#include <pktlog.h>
#endif
#include <dp_umac_reset.h>

//#include "dp_tx.h"

#define REPT_MU_MIMO 1
#define REPT_MU_OFDMA_MIMO 3
#define DP_VO_TID 6
 /** MAX TID MAPS AVAILABLE PER PDEV */
#define DP_MAX_TID_MAPS 16
/** pad DSCP_TID_MAP_MAX with 6 to fix oob issue */
#define DSCP_TID_MAP_MAX (64 + 6)
#define DP_IP_DSCP_SHIFT 2
#define DP_IP_DSCP_MASK 0x3f
#define DP_FC0_SUBTYPE_QOS 0x80
#define DP_QOS_TID 0x0f
#define DP_IPV6_PRIORITY_SHIFT 20
#define MAX_MON_LINK_DESC_BANKS 2
#define DP_VDEV_ALL 0xff

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define WLAN_DP_RESET_MON_BUF_RING_FILTER
#define MAX_TXDESC_POOLS 6
#else
#define MAX_TXDESC_POOLS 4
#endif

/* Max no of descriptors to handle special frames like EAPOL */
#define MAX_TX_SPL_DESC 1024

#define MAX_RXDESC_POOLS 4
#define MAX_PPE_TXDESC_POOLS 1

/* Max no. of VDEV per PSOC */
#ifdef WLAN_PSOC_MAX_VDEVS
#define MAX_VDEV_CNT WLAN_PSOC_MAX_VDEVS
#else
#define MAX_VDEV_CNT 51
#endif

/* Max no. of VDEVs, a PDEV can support */
#ifdef WLAN_PDEV_MAX_VDEVS
#define DP_PDEV_MAX_VDEVS WLAN_PDEV_MAX_VDEVS
#else
#define DP_PDEV_MAX_VDEVS 17
#endif

#define EXCEPTION_DEST_RING_ID 0
#define MAX_IDLE_SCATTER_BUFS 16
#define DP_MAX_IRQ_PER_CONTEXT 12
#define DEFAULT_HW_PEER_ID 0xffff

#define MAX_AST_AGEOUT_COUNT 128

#ifdef TX_ADDR_INDEX_SEARCH
#define DP_TX_ADDR_SEARCH_ADDR_POLICY HAL_TX_ADDR_INDEX_SEARCH
#else
#define DP_TX_ADDR_SEARCH_ADDR_POLICY HAL_TX_ADDR_SEARCH_DEFAULT
#endif

#define WBM_INT_ERROR_ALL 0
#define WBM_INT_ERROR_REO_NULL_BUFFER 1
#define WBM_INT_ERROR_REO_NULL_LINK_DESC 2
#define WBM_INT_ERROR_REO_NULL_MSDU_BUFF 3
#define WBM_INT_ERROR_REO_BUFF_REAPED 4
#define MAX_WBM_INT_ERROR_REASONS 5

#define MAX_TX_HW_QUEUES MAX_TCL_DATA_RINGS
/* Maximum retries for Delba per tid per peer */
#define DP_MAX_DELBA_RETRY 3

#ifdef AST_OFFLOAD_ENABLE
#define AST_OFFLOAD_ENABLE_STATUS 1
#else
#define AST_OFFLOAD_ENABLE_STATUS 0
#endif

#ifdef FEATURE_MEC_OFFLOAD
#define FW_MEC_FW_OFFLOAD_ENABLED 1
#else
#define FW_MEC_FW_OFFLOAD_ENABLED 0
#endif

#define PCP_TID_MAP_MAX 8
#define MAX_MU_USERS 37

#define REO_CMD_EVENT_HIST_MAX 64

#define DP_MAX_SRNGS 64

/* 2G PHYB */
#define PHYB_2G_LMAC_ID 2
#define PHYB_2G_TARGET_PDEV_ID 2

/* Flags for skippig s/w tid classification */
#define DP_TX_HW_DSCP_TID_MAP_VALID 0x1
#define DP_TXRX_HLOS_TID_OVERRIDE_ENABLED 0x2
#define DP_TX_MESH_ENABLED 0x4
#define DP_TX_INVALID_QOS_TAG 0xf

#ifdef WLAN_SUPPORT_RX_FISA
#define FISA_FLOW_MAX_AGGR_COUNT        16 /* max flow aggregate count */
#endif

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
#define DP_RX_REFILL_BUFF_POOL_SIZE  2048
#define DP_RX_REFILL_BUFF_POOL_BURST 64
#define DP_RX_REFILL_THRD_THRESHOLD  512
#endif

#ifdef WLAN_VENDOR_SPECIFIC_BAR_UPDATE
#define DP_SKIP_BAR_UPDATE_TIMEOUT 5000
#endif

#define DP_TX_MAGIC_PATTERN_INUSE	0xABCD1234
#define DP_TX_MAGIC_PATTERN_FREE	0xDEADBEEF

#ifdef IPA_OFFLOAD
#define DP_PEER_REO_STATS_TID_SHIFT 16
#define DP_PEER_REO_STATS_TID_MASK 0xFFFF0000
#define DP_PEER_REO_STATS_PEER_ID_MASK 0x0000FFFF
#define DP_PEER_GET_REO_STATS_TID(comb_peer_id_tid) \
	((comb_peer_id_tid & DP_PEER_REO_STATS_TID_MASK) >> \
	DP_PEER_REO_STATS_TID_SHIFT)
#define DP_PEER_GET_REO_STATS_PEER_ID(comb_peer_id_tid) \
	(comb_peer_id_tid & DP_PEER_REO_STATS_PEER_ID_MASK)
#endif

enum rx_pktlog_mode {
	DP_RX_PKTLOG_DISABLED = 0,
	DP_RX_PKTLOG_FULL,
	DP_RX_PKTLOG_LITE,
};

/* enum m_copy_mode - Available mcopy mode
 *
 */
enum m_copy_mode {
	M_COPY_DISABLED = 0,
	M_COPY = 2,
	M_COPY_EXTENDED = 4,
};

struct msdu_list {
	qdf_nbuf_t head;
	qdf_nbuf_t tail;
	uint32_t sum_len;
};

struct dp_soc_cmn;
struct dp_pdev;
struct dp_vdev;
struct dp_tx_desc_s;
struct dp_soc;
union dp_rx_desc_list_elem_t;
struct cdp_peer_rate_stats_ctx;
struct cdp_soc_rate_stats_ctx;
struct dp_rx_fst;
struct dp_mon_filter;
struct dp_mon_mpdu;
#ifdef BE_PKTLOG_SUPPORT
struct dp_mon_filter_be;
#endif
struct dp_peer;
struct dp_txrx_peer;

/**
 * enum dp_peer_state - DP peer states
 * @DP_PEER_STATE_NONE:
 * @DP_PEER_STATE_INIT:
 * @DP_PEER_STATE_ACTIVE:
 * @DP_PEER_STATE_LOGICAL_DELETE:
 * @DP_PEER_STATE_INACTIVE:
 * @DP_PEER_STATE_FREED:
 * @DP_PEER_STATE_INVALID:
 */
enum dp_peer_state {
	DP_PEER_STATE_NONE,
	DP_PEER_STATE_INIT,
	DP_PEER_STATE_ACTIVE,
	DP_PEER_STATE_LOGICAL_DELETE,
	DP_PEER_STATE_INACTIVE,
	DP_PEER_STATE_FREED,
	DP_PEER_STATE_INVALID,
};

/**
 * enum dp_mod_id - DP module IDs
 * @DP_MOD_ID_TX_RX:
 * @DP_MOD_ID_TX_COMP:
 * @DP_MOD_ID_RX:
 * @DP_MOD_ID_HTT_COMP:
 * @DP_MOD_ID_RX_ERR:
 * @DP_MOD_ID_TX_PPDU_STATS:
 * @DP_MOD_ID_RX_PPDU_STATS:
 * @DP_MOD_ID_CDP:
 * @DP_MOD_ID_GENERIC_STATS:
 * @DP_MOD_ID_TX_MULTIPASS:
 * @DP_MOD_ID_TX_CAPTURE:
 * @DP_MOD_ID_NSS_OFFLOAD:
 * @DP_MOD_ID_CONFIG:
 * @DP_MOD_ID_HTT:
 * @DP_MOD_ID_IPA:
 * @DP_MOD_ID_AST:
 * @DP_MOD_ID_MCAST2UCAST:
 * @DP_MOD_ID_CHILD:
 * @DP_MOD_ID_MESH:
 * @DP_MOD_ID_TX_EXCEPTION:
 * @DP_MOD_ID_TDLS:
 * @DP_MOD_ID_MISC:
 * @DP_MOD_ID_MSCS:
 * @DP_MOD_ID_TX:
 * @DP_MOD_ID_SAWF:
 * @DP_MOD_ID_REINJECT:
 * @DP_MOD_ID_SCS:
 * @DP_MOD_ID_UMAC_RESET:
 * @DP_MOD_ID_TX_MCAST:
 * @DP_MOD_ID_DS:
 * @DP_MOD_ID_MAX:
 */
enum dp_mod_id {
	DP_MOD_ID_TX_RX,
	DP_MOD_ID_TX_COMP,
	DP_MOD_ID_RX,
	DP_MOD_ID_HTT_COMP,
	DP_MOD_ID_RX_ERR,
	DP_MOD_ID_TX_PPDU_STATS,
	DP_MOD_ID_RX_PPDU_STATS,
	DP_MOD_ID_CDP,
	DP_MOD_ID_GENERIC_STATS,
	DP_MOD_ID_TX_MULTIPASS,
	DP_MOD_ID_TX_CAPTURE,
	DP_MOD_ID_NSS_OFFLOAD,
	DP_MOD_ID_CONFIG,
	DP_MOD_ID_HTT,
	DP_MOD_ID_IPA,
	DP_MOD_ID_AST,
	DP_MOD_ID_MCAST2UCAST,
	DP_MOD_ID_CHILD,
	DP_MOD_ID_MESH,
	DP_MOD_ID_TX_EXCEPTION,
	DP_MOD_ID_TDLS,
	DP_MOD_ID_MISC,
	DP_MOD_ID_MSCS,
	DP_MOD_ID_TX,
	DP_MOD_ID_SAWF,
	DP_MOD_ID_REINJECT,
	DP_MOD_ID_SCS,
	DP_MOD_ID_UMAC_RESET,
	DP_MOD_ID_TX_MCAST,
	DP_MOD_ID_DS,
	DP_MOD_ID_MAX,
};

#define DP_PDEV_ITERATE_VDEV_LIST(_pdev, _vdev) \
	TAILQ_FOREACH((_vdev), &(_pdev)->vdev_list, vdev_list_elem)

#define DP_VDEV_ITERATE_PEER_LIST(_vdev, _peer) \
	TAILQ_FOREACH((_peer), &(_vdev)->peer_list, peer_list_elem)

#define DP_PEER_ITERATE_ASE_LIST(_peer, _ase, _temp_ase) \
	TAILQ_FOREACH_SAFE((_ase), &peer->ast_entry_list, ase_list_elem, (_temp_ase))

#define DP_MUTEX_TYPE qdf_spinlock_t

#define DP_FRAME_IS_MULTICAST(_a)  (*(_a) & 0x01)
#define DP_FRAME_IS_IPV4_MULTICAST(_a)  (*(_a) == 0x01)

#define DP_FRAME_IS_IPV6_MULTICAST(_a)         \
    ((_a)[0] == 0x33 &&                         \
     (_a)[1] == 0x33)

#define DP_FRAME_IS_BROADCAST(_a)              \
    ((_a)[0] == 0xff &&                         \
     (_a)[1] == 0xff &&                         \
     (_a)[2] == 0xff &&                         \
     (_a)[3] == 0xff &&                         \
     (_a)[4] == 0xff &&                         \
     (_a)[5] == 0xff)
#define DP_FRAME_IS_SNAP(_llc) ((_llc)->llc_dsap == 0xaa && \
		(_llc)->llc_ssap == 0xaa && \
		(_llc)->llc_un.type_snap.control == 0x3)
#define DP_FRAME_IS_LLC(typeorlen) ((typeorlen) >= 0x600)
#define DP_FRAME_FC0_TYPE_MASK 0x0c
#define DP_FRAME_FC0_TYPE_DATA 0x08
#define DP_FRAME_IS_DATA(_frame) \
	(((_frame)->i_fc[0] & DP_FRAME_FC0_TYPE_MASK) == DP_FRAME_FC0_TYPE_DATA)

/*
 * macros to convert hw mac id to sw mac id:
 * mac ids used by hardware start from a value of 1 while
 * those in host software start from a value of 0. Use the
 * macros below to convert between mac ids used by software and
 * hardware
 */
#define DP_SW2HW_MACID(id) ((id) + 1)
#define DP_HW2SW_MACID(id) ((id) > 0 ? ((id) - 1) : 0)

/*
 * Number of Tx Queues
 * enum and macro to define how many threshold levels is used
 * for the AC based flow control
 */
#ifdef QCA_AC_BASED_FLOW_CONTROL
enum dp_fl_ctrl_threshold {
	DP_TH_BE_BK = 0,
	DP_TH_VI,
	DP_TH_VO,
	DP_TH_HI,
};

#define FL_TH_MAX (4)
#define FL_TH_VI_PERCENTAGE (80)
#define FL_TH_VO_PERCENTAGE (60)
#define FL_TH_HI_PERCENTAGE (40)
#endif

/**
 * enum dp_intr_mode
 * @DP_INTR_INTEGRATED: Line interrupts
 * @DP_INTR_MSI: MSI interrupts
 * @DP_INTR_POLL: Polling
 * @DP_INTR_LEGACY_VIRTUAL_IRQ:
 */
enum dp_intr_mode {
	DP_INTR_INTEGRATED = 0,
	DP_INTR_MSI,
	DP_INTR_POLL,
	DP_INTR_LEGACY_VIRTUAL_IRQ,
};

/**
 * enum dp_tx_frm_type
 * @dp_tx_frm_std: Regular frame, no added header fragments
 * @dp_tx_frm_tso: TSO segment, with a modified IP header added
 * @dp_tx_frm_sg: SG segment
 * @dp_tx_frm_audio: Audio frames, a custom LLC/SNAP header added
 * @dp_tx_frm_me: Multicast to Unicast Converted frame
 * @dp_tx_frm_raw: Raw Frame
 * @dp_tx_frm_rmnet:
 */
enum dp_tx_frm_type {
	dp_tx_frm_std = 0,
	dp_tx_frm_tso,
	dp_tx_frm_sg,
	dp_tx_frm_audio,
	dp_tx_frm_me,
	dp_tx_frm_raw,
	dp_tx_frm_rmnet,
};

/**
 * enum dp_ast_type
 * @dp_ast_type_wds: WDS peer AST type
 * @dp_ast_type_static: static ast entry type
 * @dp_ast_type_mec: Multicast echo ast entry type
 */
enum dp_ast_type {
	dp_ast_type_wds = 0,
	dp_ast_type_static,
	dp_ast_type_mec,
};

/**
 * enum dp_nss_cfg
 * @dp_nss_cfg_default: No radios are offloaded
 * @dp_nss_cfg_first_radio: First radio offloaded
 * @dp_nss_cfg_second_radio: Second radio offloaded
 * @dp_nss_cfg_dbdc: Dual radios offloaded
 * @dp_nss_cfg_dbtc: Three radios offloaded
 * @dp_nss_cfg_max: max value
 */
enum dp_nss_cfg {
	dp_nss_cfg_default = 0x0,
	dp_nss_cfg_first_radio = 0x1,
	dp_nss_cfg_second_radio = 0x2,
	dp_nss_cfg_dbdc = 0x3,
	dp_nss_cfg_dbtc = 0x7,
	dp_nss_cfg_max
};

#ifdef WLAN_TX_PKT_CAPTURE_ENH
#define DP_CPU_RING_MAP_1 1
#endif

/**
 * enum dp_cpu_ring_map_types - dp tx cpu ring map
 * @DP_NSS_DEFAULT_MAP: Default mode with no NSS offloaded
 * @DP_NSS_FIRST_RADIO_OFFLOADED_MAP: Only First Radio is offloaded
 * @DP_NSS_SECOND_RADIO_OFFLOADED_MAP: Only second radio is offloaded
 * @DP_NSS_DBDC_OFFLOADED_MAP: Both radios are offloaded
 * @DP_NSS_DBTC_OFFLOADED_MAP: All three radios are offloaded
 * @DP_SINGLE_TX_RING_MAP: to avoid out of order all cpu mapped to single ring
 * @DP_NSS_CPU_RING_MAP_MAX: Max cpu ring map val
 */
enum dp_cpu_ring_map_types {
	DP_NSS_DEFAULT_MAP,
	DP_NSS_FIRST_RADIO_OFFLOADED_MAP,
	DP_NSS_SECOND_RADIO_OFFLOADED_MAP,
	DP_NSS_DBDC_OFFLOADED_MAP,
	DP_NSS_DBTC_OFFLOADED_MAP,
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	DP_SINGLE_TX_RING_MAP,
#endif
	DP_NSS_CPU_RING_MAP_MAX
};

/**
 * struct dp_rx_nbuf_frag_info - Hold vaddr and paddr for a buffer
 *
 * @paddr: Physical address of buffer allocated.
 * @virt_addr: union of virtual address representations
 * @nbuf: Allocated nbuf in case of nbuf approach.
 * @vaddr: Virtual address of frag allocated in case of frag approach.
 */
struct dp_rx_nbuf_frag_info {
	qdf_dma_addr_t paddr;
	union {
		qdf_nbuf_t nbuf;
		qdf_frag_t vaddr;
	} virt_addr;
};

/**
 * enum dp_ctxt_type - context type
 * @DP_PDEV_TYPE: PDEV context
 * @DP_RX_RING_HIST_TYPE: Datapath rx ring history
 * @DP_RX_ERR_RING_HIST_TYPE: Datapath rx error ring history
 * @DP_RX_REINJECT_RING_HIST_TYPE: Datapath reinject ring history
 * @DP_TX_TCL_HIST_TYPE:
 * @DP_TX_COMP_HIST_TYPE:
 * @DP_FISA_RX_FT_TYPE:
 * @DP_RX_REFILL_RING_HIST_TYPE: Datapath rx refill ring history
 * @DP_TX_HW_DESC_HIST_TYPE: Datapath TX HW descriptor history
 * @DP_MON_SOC_TYPE: Datapath monitor soc context
 * @DP_MON_PDEV_TYPE: Datapath monitor pdev context
 * @DP_MON_STATUS_BUF_HIST_TYPE: DP monitor status buffer history
 * @DP_CFG_EVENT_HIST_TYPE: DP config events history
 */
enum dp_ctxt_type {
	DP_PDEV_TYPE,
	DP_RX_RING_HIST_TYPE,
	DP_RX_ERR_RING_HIST_TYPE,
	DP_RX_REINJECT_RING_HIST_TYPE,
	DP_TX_TCL_HIST_TYPE,
	DP_TX_COMP_HIST_TYPE,
	DP_FISA_RX_FT_TYPE,
	DP_RX_REFILL_RING_HIST_TYPE,
	DP_TX_HW_DESC_HIST_TYPE,
	DP_MON_SOC_TYPE,
	DP_MON_PDEV_TYPE,
	DP_MON_STATUS_BUF_HIST_TYPE,
	DP_CFG_EVENT_HIST_TYPE,
};

/**
 * enum dp_desc_type - source type for multiple pages allocation
 * @DP_TX_DESC_TYPE: DP SW TX descriptor
 * @DP_TX_PPEDS_DESC_TYPE: DP PPE-DS Tx descriptor
 * @DP_TX_EXT_DESC_TYPE: DP TX msdu extension descriptor
 * @DP_TX_EXT_DESC_LINK_TYPE: DP link descriptor for msdu ext_desc
 * @DP_TX_TSO_DESC_TYPE: DP TX TSO descriptor
 * @DP_TX_TSO_NUM_SEG_TYPE: DP TX number of segments
 * @DP_RX_DESC_BUF_TYPE: DP RX SW descriptor
 * @DP_RX_DESC_STATUS_TYPE: DP RX SW descriptor for monitor status
 * @DP_HW_LINK_DESC_TYPE: DP HW link descriptor
 * @DP_HW_CC_SPT_PAGE_TYPE: DP pages for HW CC secondary page table
 */
enum dp_desc_type {
	DP_TX_DESC_TYPE,
	DP_TX_PPEDS_DESC_TYPE,
	DP_TX_EXT_DESC_TYPE,
	DP_TX_EXT_DESC_LINK_TYPE,
	DP_TX_TSO_DESC_TYPE,
	DP_TX_TSO_NUM_SEG_TYPE,
	DP_RX_DESC_BUF_TYPE,
	DP_RX_DESC_STATUS_TYPE,
	DP_HW_LINK_DESC_TYPE,
	DP_HW_CC_SPT_PAGE_TYPE,
};

/**
 * struct rx_desc_pool
 * @pool_size: number of RX descriptor in the pool
 * @elem_size: Element size
 * @desc_pages: Multi page descriptors
 * @array: pointer to array of RX descriptor
 * @freelist: pointer to free RX descriptor link list
 * @lock: Protection for the RX descriptor pool
 * @owner: owner for nbuf
 * @buf_size: Buffer size
 * @buf_alignment: Buffer alignment
 * @rx_mon_dest_frag_enable: Enable frag processing for mon dest buffer
 * @desc_type: type of desc this pool serves
 */
struct rx_desc_pool {
	uint32_t pool_size;
#ifdef RX_DESC_MULTI_PAGE_ALLOC
	uint16_t elem_size;
	struct qdf_mem_multi_page_t desc_pages;
#else
	union dp_rx_desc_list_elem_t *array;
#endif
	union dp_rx_desc_list_elem_t *freelist;
	qdf_spinlock_t lock;
	uint8_t owner;
	uint16_t buf_size;
	uint8_t buf_alignment;
	bool rx_mon_dest_frag_enable;
	enum dp_desc_type desc_type;
};

/**
 * struct dp_tx_ext_desc_elem_s
 * @next: next extension descriptor pointer
 * @vaddr: hlos virtual address pointer
 * @paddr: physical address pointer for descriptor
 * @flags: mark features for extension descriptor
 * @me_buffer: Pointer to ME buffer - store this so that it can be freed on
 *		Tx completion of ME packet
 * @tso_desc: Pointer to Tso desc
 * @tso_num_desc: Pointer to tso_num_desc
 */
struct dp_tx_ext_desc_elem_s {
	struct dp_tx_ext_desc_elem_s *next;
	void *vaddr;
	qdf_dma_addr_t paddr;
	uint16_t flags;
	struct dp_tx_me_buf_t *me_buffer;
	struct qdf_tso_seg_elem_t *tso_desc;
	struct qdf_tso_num_seg_elem_t *tso_num_desc;
};

/*
 * NB: intentionally not using kernel-doc comment because the kernel-doc
 *     script does not handle the qdf_dma_mem_context macro
 * struct dp_tx_ext_desc_pool_s - Tx Extension Descriptor Pool
 * @elem_count: Number of descriptors in the pool
 * @elem_size: Size of each descriptor
 * @num_free: Number of free descriptors
 * @desc_pages: multiple page allocation information for actual descriptors
 * @link_elem_size: size of the link descriptor in cacheable memory used for
 * 		    chaining the extension descriptors
 * @desc_link_pages: multiple page allocation information for link descriptors
 * @freelist:
 * @lock:
 * @memctx:
 */
struct dp_tx_ext_desc_pool_s {
	uint16_t elem_count;
	int elem_size;
	uint16_t num_free;
	struct qdf_mem_multi_page_t desc_pages;
	int link_elem_size;
	struct qdf_mem_multi_page_t desc_link_pages;
	struct dp_tx_ext_desc_elem_s *freelist;
	qdf_spinlock_t lock;
	qdf_dma_mem_context(memctx);
};

/**
 * struct dp_tx_desc_s - Tx Descriptor
 * @next: Next in the chain of descriptors in freelist or in the completion list
 * @nbuf: Buffer Address
 * @length:
 * @magic:
 * @timestamp_tick:
 * @flags: Flags to track the state of descriptor and special frame handling
 * @id: Descriptor ID
 * @dma_addr:
 * @vdev_id: vdev_id of vdev over which the packet was transmitted
 * @tx_status:
 * @peer_id:
 * @pdev: Handle to pdev
 * @tx_encap_type: Transmit encap type (i.e. Raw, Native Wi-Fi, Ethernet).
 * 		   This is maintained in descriptor to allow more efficient
 * 		   processing in completion event processing code.
 * 		   This field is filled in with the htt_pkt_type enum.
 * @buffer_src: buffer source TQM, REO, FW etc.
 * @reserved:
 * @frm_type: Frame Type - ToDo check if this is redundant
 * @pkt_offset: Offset from which the actual packet data starts
 * @pool_id: Pool ID - used when releasing the descriptor
 * @shinfo_addr:
 * @msdu_ext_desc: MSDU extension descriptor
 * @timestamp:
 * @comp:
 */
struct dp_tx_desc_s {
	struct dp_tx_desc_s *next;
	qdf_nbuf_t nbuf;
	uint16_t length;
#ifdef DP_TX_TRACKING
	uint32_t magic;
	uint64_t timestamp_tick;
#endif
	uint16_t flags;
	uint32_t id;
	qdf_dma_addr_t dma_addr;
	uint8_t vdev_id;
	uint8_t tx_status;
	uint16_t peer_id;
	struct dp_pdev *pdev;
	uint8_t tx_encap_type:2,
		buffer_src:3,
		reserved:3;
	uint8_t frm_type;
	uint8_t pkt_offset;
	uint8_t  pool_id;
	unsigned char *shinfo_addr;
	struct dp_tx_ext_desc_elem_s *msdu_ext_desc;
	qdf_ktime_t timestamp;
	struct hal_tx_desc_comp_s comp;
};

#ifdef QCA_AC_BASED_FLOW_CONTROL
/**
 * enum flow_pool_status - flow pool status
 * @FLOW_POOL_ACTIVE_UNPAUSED : pool is active (can take/put descriptors)
 *				and network queues are unpaused
 * @FLOW_POOL_ACTIVE_PAUSED: pool is active (can take/put descriptors)
 *			   and network queues are paused
 * @FLOW_POOL_BE_BK_PAUSED:
 * @FLOW_POOL_VI_PAUSED:
 * @FLOW_POOL_VO_PAUSED:
 * @FLOW_POOL_INVALID: pool is invalid (put descriptor)
 * @FLOW_POOL_INACTIVE: pool is inactive (pool is free)
 * @FLOW_POOL_ACTIVE_UNPAUSED_REATTACH: pool is reattached but network
 *					queues are not paused
 */
enum flow_pool_status {
	FLOW_POOL_ACTIVE_UNPAUSED = 0,
	FLOW_POOL_ACTIVE_PAUSED = 1,
	FLOW_POOL_BE_BK_PAUSED = 2,
	FLOW_POOL_VI_PAUSED = 3,
	FLOW_POOL_VO_PAUSED = 4,
	FLOW_POOL_INVALID = 5,
	FLOW_POOL_INACTIVE = 6,
	FLOW_POOL_ACTIVE_UNPAUSED_REATTACH = 7,
};

#else
/**
 * enum flow_pool_status - flow pool status
 * @FLOW_POOL_ACTIVE_UNPAUSED : pool is active (can take/put descriptors)
 *				and network queues are unpaused
 * @FLOW_POOL_ACTIVE_PAUSED: pool is active (can take/put descriptors)
 *			   and network queues are paused
 * @FLOW_POOL_BE_BK_PAUSED:
 * @FLOW_POOL_VI_PAUSED:
 * @FLOW_POOL_VO_PAUSED:
 * @FLOW_POOL_INVALID: pool is invalid (put descriptor)
 * @FLOW_POOL_INACTIVE: pool is inactive (pool is free)
 */
enum flow_pool_status {
	FLOW_POOL_ACTIVE_UNPAUSED = 0,
	FLOW_POOL_ACTIVE_PAUSED = 1,
	FLOW_POOL_BE_BK_PAUSED = 2,
	FLOW_POOL_VI_PAUSED = 3,
	FLOW_POOL_VO_PAUSED = 4,
	FLOW_POOL_INVALID = 5,
	FLOW_POOL_INACTIVE = 6,
};

#endif

/**
 * struct dp_tx_tso_seg_pool_s
 * @pool_size: total number of pool elements
 * @num_free: free element count
 * @freelist: first free element pointer
 * @desc_pages: multiple page allocation information for actual descriptors
 * @lock: lock for accessing the pool
 */
struct dp_tx_tso_seg_pool_s {
	uint16_t pool_size;
	uint16_t num_free;
	struct qdf_tso_seg_elem_t *freelist;
	struct qdf_mem_multi_page_t desc_pages;
	qdf_spinlock_t lock;
};

/**
 * struct dp_tx_tso_num_seg_pool_s - TSO Num seg pool
 * @num_seg_pool_size: total number of pool elements
 * @num_free: free element count
 * @freelist: first free element pointer
 * @desc_pages: multiple page allocation information for actual descriptors
 * @lock: lock for accessing the pool
 */

struct dp_tx_tso_num_seg_pool_s {
	uint16_t num_seg_pool_size;
	uint16_t num_free;
	struct qdf_tso_num_seg_elem_t *freelist;
	struct qdf_mem_multi_page_t desc_pages;
	/*tso mutex */
	qdf_spinlock_t lock;
};

/**
 * struct dp_tx_desc_pool_s - Tx Descriptor pool information
 * @elem_size: Size of each descriptor in the pool
 * @num_allocated: Number of used descriptors
 * @freelist: Chain of free descriptors
 * @desc_pages: multiple page allocation information for actual descriptors
 * @pool_size: Total number of descriptors in the pool
 * @flow_pool_id:
 * @num_invalid_bin: Deleted pool with pending Tx completions.
 * @avail_desc:
 * @status:
 * @flow_type:
 * @stop_th:
 * @start_th:
 * @max_pause_time:
 * @latest_pause_time:
 * @pkt_drop_no_desc:
 * @flow_pool_lock:
 * @pool_create_cnt:
 * @pool_owner_ctx:
 * @elem_count:
 * @num_free: Number of free descriptors
 * @lock: Lock for descriptor allocation/free from/to the pool
 */
struct dp_tx_desc_pool_s {
	uint16_t elem_size;
	uint32_t num_allocated;
	struct dp_tx_desc_s *freelist;
	struct qdf_mem_multi_page_t desc_pages;
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
	uint16_t pool_size;
	uint8_t flow_pool_id;
	uint8_t num_invalid_bin;
	uint16_t avail_desc;
	enum flow_pool_status status;
	enum htt_flow_type flow_type;
#ifdef QCA_AC_BASED_FLOW_CONTROL
	uint16_t stop_th[FL_TH_MAX];
	uint16_t start_th[FL_TH_MAX];
	qdf_time_t max_pause_time[FL_TH_MAX];
	qdf_time_t latest_pause_time[FL_TH_MAX];
#else
	uint16_t stop_th;
	uint16_t start_th;
#endif
	uint16_t pkt_drop_no_desc;
	qdf_spinlock_t flow_pool_lock;
	uint8_t pool_create_cnt;
	void *pool_owner_ctx;
#else
	uint16_t elem_count;
	uint32_t num_free;
	qdf_spinlock_t lock;
#endif
};

/**
 * struct dp_txrx_pool_stats - flow pool related statistics
 * @pool_map_count: flow pool map received
 * @pool_unmap_count: flow pool unmap received
 * @pkt_drop_no_pool: packets dropped due to unavailablity of pool
 */
struct dp_txrx_pool_stats {
	uint16_t pool_map_count;
	uint16_t pool_unmap_count;
	uint16_t pkt_drop_no_pool;
};

/**
 * struct dp_srng - DP srng structure
 * @hal_srng: hal_srng handle
 * @base_vaddr_unaligned: un-aligned virtual base address of the srng ring
 * @base_vaddr_aligned: aligned virtual base address of the srng ring
 * @base_paddr_unaligned: un-aligned physical base address of the srng ring
 * @base_paddr_aligned: aligned physical base address of the srng ring
 * @alloc_size: size of the srng ring
 * @cached: is the srng ring memory cached or un-cached memory
 * @irq: irq number of the srng ring
 * @num_entries: number of entries in the srng ring
 * @is_mem_prealloc: Is this srng memory pre-allocated
 * @crit_thresh: Critical threshold for near-full processing of this srng
 * @safe_thresh: Safe threshold for near-full processing of this srng
 * @near_full: Flag to indicate srng is near-full
 */
struct dp_srng {
	hal_ring_handle_t hal_srng;
	void *base_vaddr_unaligned;
	void *base_vaddr_aligned;
	qdf_dma_addr_t base_paddr_unaligned;
	qdf_dma_addr_t base_paddr_aligned;
	uint32_t alloc_size;
	uint8_t cached;
	int irq;
	uint32_t num_entries;
#ifdef DP_MEM_PRE_ALLOC
	uint8_t is_mem_prealloc;
#endif
#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
	uint16_t crit_thresh;
	uint16_t safe_thresh;
	qdf_atomic_t near_full;
#endif
};

struct dp_rx_reorder_array_elem {
	qdf_nbuf_t head;
	qdf_nbuf_t tail;
};

#define DP_RX_BA_INACTIVE 0
#define DP_RX_BA_ACTIVE 1
#define DP_RX_BA_IN_PROGRESS 2
struct dp_reo_cmd_info {
	uint16_t cmd;
	enum hal_reo_cmd_type cmd_type;
	void *data;
	void (*handler)(struct dp_soc *, void *, union hal_reo_status *);
	TAILQ_ENTRY(dp_reo_cmd_info) reo_cmd_list_elem;
};

struct dp_peer_delay_stats {
	struct cdp_delay_tid_stats delay_tid_stats[CDP_MAX_DATA_TIDS]
						  [CDP_MAX_TXRX_CTX];
};

/* Rx TID defrag*/
struct dp_rx_tid_defrag {
	/* TID */
	int tid;

	/* only used for defrag right now */
	TAILQ_ENTRY(dp_rx_tid_defrag) defrag_waitlist_elem;

	/* Store dst desc for reinjection */
	hal_ring_desc_t dst_ring_desc;
	struct dp_rx_desc *head_frag_desc;

	/* Sequence and fragments that are being processed currently */
	uint32_t curr_seq_num;
	uint32_t curr_frag_num;

	/* TODO: Check the following while adding defragmentation support */
	struct dp_rx_reorder_array_elem *array;
	/* base - single rx reorder element used for non-aggr cases */
	struct dp_rx_reorder_array_elem base;
	/* rx_tid lock */
	qdf_spinlock_t defrag_tid_lock;

	/* head PN number */
	uint64_t pn128[2];

	uint32_t defrag_timeout_ms;

	/* defrag usage only, dp_peer pointer related with this tid */
	struct dp_txrx_peer *defrag_peer;
};

/* Rx TID */
struct dp_rx_tid {
	/* TID */
	int tid;

	/* Num of addba requests */
	uint32_t num_of_addba_req;

	/* Num of addba responses */
	uint32_t num_of_addba_resp;

	/* Num of delba requests */
	uint32_t num_of_delba_req;

	/* Num of addba responses successful */
	uint32_t num_addba_rsp_success;

	/* Num of addba responses failed */
	uint32_t num_addba_rsp_failed;

	/* pn size */
	uint8_t pn_size;
	/* REO TID queue descriptors */
	void *hw_qdesc_vaddr_unaligned;
	void *hw_qdesc_vaddr_aligned;
	qdf_dma_addr_t hw_qdesc_paddr_unaligned;
	qdf_dma_addr_t hw_qdesc_paddr;
	uint32_t hw_qdesc_alloc_size;

	/* RX ADDBA session state */
	int ba_status;

	/* RX BA window size */
	uint16_t ba_win_size;

	/* Starting sequence number in Addba request */
	uint16_t startseqnum;
	uint16_t dialogtoken;
	uint16_t statuscode;
	/* user defined ADDBA response status code */
	uint16_t userstatuscode;

	/* rx_tid lock */
	qdf_spinlock_t tid_lock;

	/* Store ppdu_id when 2k exception is received */
	uint32_t ppdu_id_2k;

	/* Delba Tx completion status */
	uint8_t delba_tx_status;

	/* Delba Tx retry count */
	uint8_t delba_tx_retry;

	/* Delba stats */
	uint32_t delba_tx_success_cnt;
	uint32_t delba_tx_fail_cnt;

	/* Delba reason code for retries */
	uint8_t delba_rcode;

	/* Coex Override preserved windows size 1 based */
	uint16_t rx_ba_win_size_override;
#ifdef IPA_OFFLOAD
	/* rx msdu count per tid */
	struct cdp_pkt_info rx_msdu_cnt;
#endif

};

/**
 * struct dp_intr_stats - DP Interrupt Stats for an interrupt context
 * @num_tx_ring_masks: interrupts with tx_ring_mask set
 * @num_rx_ring_masks: interrupts with rx_ring_mask set
 * @num_rx_mon_ring_masks: interrupts with rx_mon_ring_mask set
 * @num_rx_err_ring_masks: interrupts with rx_err_ring_mask set
 * @num_rx_wbm_rel_ring_masks: interrupts with rx_wbm_rel_ring_mask set
 * @num_reo_status_ring_masks: interrupts with reo_status_ring_mask set
 * @num_rxdma2host_ring_masks: interrupts with rxdma2host_ring_mask set
 * @num_host2rxdma_ring_masks: interrupts with host2rxdma_ring_mask set
 * @num_host2rxdma_mon_ring_masks: interrupts with host2rxdma_ring_mask set
 * @num_rx_ring_near_full_masks: Near-full interrupts for REO DST ring
 * @num_tx_comp_ring_near_full_masks: Near-full interrupts for TX completion
 * @num_rx_wbm_rel_ring_near_full_masks: total number of times the wbm rel ring
 *                                       near full interrupt was received
 * @num_reo_status_ring_near_full_masks: total number of times the reo status
 *                                       near full interrupt was received
 * @num_near_full_masks: total number of times the near full interrupt
 *                       was received
 * @num_masks: total number of times the interrupt was received
 * @num_host2txmon_ring__masks: interrupts with host2txmon_ring_mask set
 * @num_near_full_masks: total number of times the interrupt was received
 * @num_masks: total number of times the near full interrupt was received
 * @num_tx_mon_ring_masks: interrupts with num_tx_mon_ring_masks set
 *
 * Counter for individual masks are incremented only if there are any packets
 * on that ring.
 */
struct dp_intr_stats {
	uint32_t num_tx_ring_masks[MAX_TCL_DATA_RINGS];
	uint32_t num_rx_ring_masks[MAX_REO_DEST_RINGS];
	uint32_t num_rx_mon_ring_masks;
	uint32_t num_rx_err_ring_masks;
	uint32_t num_rx_wbm_rel_ring_masks;
	uint32_t num_reo_status_ring_masks;
	uint32_t num_rxdma2host_ring_masks;
	uint32_t num_host2rxdma_ring_masks;
	uint32_t num_host2rxdma_mon_ring_masks;
	uint32_t num_rx_ring_near_full_masks[MAX_REO_DEST_RINGS];
	uint32_t num_tx_comp_ring_near_full_masks[MAX_TCL_DATA_RINGS];
	uint32_t num_rx_wbm_rel_ring_near_full_masks;
	uint32_t num_reo_status_ring_near_full_masks;
	uint32_t num_host2txmon_ring__masks;
	uint32_t num_near_full_masks;
	uint32_t num_masks;
	uint32_t num_tx_mon_ring_masks;
};

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * struct dp_intr_bkp - DP per interrupt context ring masks old state
 * @tx_ring_mask: WBM Tx completion rings (0-2) associated with this napi ctxt
 * @rx_ring_mask: Rx REO rings (0-3) associated with this interrupt context
 * @rx_mon_ring_mask: Rx monitor ring mask (0-2)
 * @rx_err_ring_mask: REO Exception Ring
 * @rx_wbm_rel_ring_mask: WBM2SW Rx Release Ring
 * @reo_status_ring_mask: REO command response ring
 * @rxdma2host_ring_mask: RXDMA to host destination ring
 * @host2rxdma_ring_mask: Host to RXDMA buffer ring
 * @host2rxdma_mon_ring_mask: Host to RXDMA monitor  buffer ring
 * @host2txmon_ring_mask: Tx monitor buffer ring
 * @tx_mon_ring_mask: Tx monitor ring mask (0-2)
 *
 */
struct dp_intr_bkp {
	uint8_t tx_ring_mask;
	uint8_t rx_ring_mask;
	uint8_t rx_mon_ring_mask;
	uint8_t rx_err_ring_mask;
	uint8_t rx_wbm_rel_ring_mask;
	uint8_t reo_status_ring_mask;
	uint8_t rxdma2host_ring_mask;
	uint8_t host2rxdma_ring_mask;
	uint8_t host2rxdma_mon_ring_mask;
	uint8_t host2txmon_ring_mask;
	uint8_t tx_mon_ring_mask;
};
#endif

/* per interrupt context  */
struct dp_intr {
	uint8_t tx_ring_mask;   /* WBM Tx completion rings (0-2)
				associated with this napi context */
	uint8_t rx_ring_mask;   /* Rx REO rings (0-3) associated
				with this interrupt context */
	uint8_t rx_mon_ring_mask;  /* Rx monitor ring mask (0-2) */
	uint8_t rx_err_ring_mask; /* REO Exception Ring */
	uint8_t rx_wbm_rel_ring_mask; /* WBM2SW Rx Release Ring */
	uint8_t reo_status_ring_mask; /* REO command response ring */
	uint8_t rxdma2host_ring_mask; /* RXDMA to host destination ring */
	uint8_t host2rxdma_ring_mask; /* Host to RXDMA buffer ring */
	/* Host to RXDMA monitor  buffer ring */
	uint8_t host2rxdma_mon_ring_mask;
	/* RX REO rings near full interrupt mask */
	uint8_t rx_near_full_grp_1_mask;
	/* RX REO rings near full interrupt mask */
	uint8_t rx_near_full_grp_2_mask;
	/* WBM TX completion rings near full interrupt mask */
	uint8_t tx_ring_near_full_mask;
	uint8_t host2txmon_ring_mask; /* Tx monitor buffer ring */
	uint8_t tx_mon_ring_mask;  /* Tx monitor ring mask (0-2) */
	struct dp_soc *soc;    /* Reference to SoC structure ,
				to get DMA ring handles */
	qdf_lro_ctx_t lro_ctx;
	uint8_t dp_intr_id;

	/* Interrupt Stats for individual masks */
	struct dp_intr_stats intr_stats;
	uint8_t umac_reset_intr_mask;  /* UMAC reset interrupt mask */
};

#define REO_DESC_FREELIST_SIZE 64
#define REO_DESC_FREE_DEFER_MS 1000
struct reo_desc_list_node {
	qdf_list_node_t node;
	unsigned long free_ts;
	struct dp_rx_tid rx_tid;
	bool resend_update_reo_cmd;
	uint32_t pending_ext_desc_size;
#ifdef REO_QDESC_HISTORY
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
#endif
};

#ifdef WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
#define REO_DESC_DEFERRED_FREELIST_SIZE 256
#define REO_DESC_DEFERRED_FREE_MS 30000

struct reo_desc_deferred_freelist_node {
	qdf_list_node_t node;
	unsigned long free_ts;
	void *hw_qdesc_vaddr_unaligned;
	qdf_dma_addr_t hw_qdesc_paddr;
	uint32_t hw_qdesc_alloc_size;
#ifdef REO_QDESC_HISTORY
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
#endif /* REO_QDESC_HISTORY */
};
#endif /* WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY */

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
/**
 * struct reo_cmd_event_record: Elements to record for each reo command
 * @cmd_type: reo command type
 * @cmd_return_status: reo command post status
 * @timestamp: record timestamp for the reo command
 */
struct reo_cmd_event_record {
	enum hal_reo_cmd_type cmd_type;
	uint8_t cmd_return_status;
	uint64_t timestamp;
};

/**
 * struct reo_cmd_event_history: Account for reo cmd events
 * @index: record number
 * @cmd_record: list of records
 */
struct reo_cmd_event_history {
	qdf_atomic_t index;
	struct reo_cmd_event_record cmd_record[REO_CMD_EVENT_HIST_MAX];
};
#endif /* WLAN_FEATURE_DP_EVENT_HISTORY */

/* SoC level data path statistics */
struct dp_soc_stats {
	struct {
		uint32_t added;
		uint32_t deleted;
		uint32_t aged_out;
		uint32_t map_err;
		uint32_t ast_mismatch;
	} ast;

	struct {
		uint32_t added;
		uint32_t deleted;
	} mec;

	/* SOC level TX stats */
	struct {
		/* Total packets transmitted */
		struct cdp_pkt_info egress[MAX_TCL_DATA_RINGS];
		/* Enqueues per tcl ring */
		uint32_t tcl_enq[MAX_TCL_DATA_RINGS];
		/* packets dropped on tx because of no peer */
		struct cdp_pkt_info tx_invalid_peer;
		/* descriptors in each tcl ring */
		uint32_t tcl_ring_full[MAX_TCL_DATA_RINGS];
		/* Descriptors in use at soc */
		uint32_t desc_in_use;
		/* tqm_release_reason == FW removed */
		uint32_t dropped_fw_removed;
		/* tx completion release_src != TQM or FW */
		uint32_t invalid_release_source;
		/* TX descriptor from completion ring Desc is not valid */
		uint32_t invalid_tx_comp_desc;
		/* tx completion wbm_internal_error */
		uint32_t wbm_internal_error[MAX_WBM_INT_ERROR_REASONS];
		/* tx completion non_wbm_internal_error */
		uint32_t non_wbm_internal_err;
		/* TX Comp loop packet limit hit */
		uint32_t tx_comp_loop_pkt_limit_hit;
		/* Head pointer Out of sync at the end of dp_tx_comp_handler */
		uint32_t hp_oos2;
		/* tx desc freed as part of vdev detach */
		uint32_t tx_comp_exception;
		/* TQM drops after/during peer delete */
		uint64_t tqm_drop_no_peer;
		/* Number of tx completions reaped per WBM2SW release ring */
		uint32_t tx_comp[MAX_TCL_DATA_RINGS];
		/* Number of tx completions force freed */
		uint32_t tx_comp_force_freed;
		/* Tx completion ring near full */
		uint32_t near_full;
	} tx;

	/* SOC level RX stats */
	struct {
		/* Total rx packets count */
		struct cdp_pkt_info ingress;
		/* Rx errors */
		/* Total Packets in Rx Error ring */
		uint32_t err_ring_pkts;
		/* No of Fragments */
		uint32_t rx_frags;
		/* No of incomplete fragments in waitlist */
		uint32_t rx_frag_wait;
		/* Fragments dropped due to errors */
		uint32_t rx_frag_err;
		/* Fragments received OOR causing sequence num mismatch */
		uint32_t rx_frag_oor;
		/* Fragments dropped due to len errors in skb */
		uint32_t rx_frag_err_len_error;
		/* Fragments dropped due to no peer found */
		uint32_t rx_frag_err_no_peer;
		/* No of reinjected packets */
		uint32_t reo_reinject;
		/* Reap loop packet limit hit */
		uint32_t reap_loop_pkt_limit_hit;
		/* Head pointer Out of sync at the end of dp_rx_process */
		uint32_t hp_oos2;
		/* Rx ring near full */
		uint32_t near_full;
		/* Break ring reaping as not all scattered msdu received */
		uint32_t msdu_scatter_wait_break;
		/* Number of bar frames received */
		uint32_t bar_frame;
		/* Number of frames routed from rxdma */
		uint32_t rxdma2rel_route_drop;
		/* Number of frames routed from reo*/
		uint32_t reo2rel_route_drop;
		uint64_t fast_recycled;
		/* Number of hw stats requested */
		uint32_t rx_hw_stats_requested;
		/* Number of hw stats request timeout */
		uint32_t rx_hw_stats_timeout;

		struct {
			/* Invalid RBM error count */
			uint32_t invalid_rbm;
			/* Invalid VDEV Error count */
			uint32_t invalid_vdev;
			/* Invalid PDEV error count */
			uint32_t invalid_pdev;

			/* Packets delivered to stack that no related peer */
			uint32_t pkt_delivered_no_peer;
			/* Defrag peer uninit error count */
			uint32_t defrag_peer_uninit;
			/* Invalid sa_idx or da_idx*/
			uint32_t invalid_sa_da_idx;
			/* MSDU DONE failures */
			uint32_t msdu_done_fail;
			/* Invalid PEER Error count */
			struct cdp_pkt_info rx_invalid_peer;
			/* Invalid PEER ID count */
			struct cdp_pkt_info rx_invalid_peer_id;
			/* Invalid packet length */
			struct cdp_pkt_info rx_invalid_pkt_len;
			/* HAL ring access Fail error count */
			uint32_t hal_ring_access_fail;
			/* HAL ring access full Fail error count */
			uint32_t hal_ring_access_full_fail;
			/* RX DMA error count */
			uint32_t rxdma_error[HAL_RXDMA_ERR_MAX];
			/* RX REO DEST Desc Invalid Magic count */
			uint32_t rx_desc_invalid_magic;
			/* REO Error count */
			uint32_t reo_error[HAL_REO_ERR_MAX];
			/* HAL REO ERR Count */
			uint32_t hal_reo_error[MAX_REO_DEST_RINGS];
			/* HAL REO DEST Duplicate count */
			uint32_t hal_reo_dest_dup;
			/* HAL WBM RELEASE Duplicate count */
			uint32_t hal_wbm_rel_dup;
			/* HAL RXDMA error Duplicate count */
			uint32_t hal_rxdma_err_dup;
			/* ipa smmu map duplicate count */
			uint32_t ipa_smmu_map_dup;
			/* ipa smmu unmap duplicate count */
			uint32_t ipa_smmu_unmap_dup;
			/* ipa smmu unmap while ipa pipes is disabled */
			uint32_t ipa_unmap_no_pipe;
			/* REO cmd send fail/requeue count */
			uint32_t reo_cmd_send_fail;
			/* REO cmd send drain count */
			uint32_t reo_cmd_send_drain;
			/* RX msdu drop count due to scatter */
			uint32_t scatter_msdu;
			/* RX msdu drop count due to invalid cookie */
			uint32_t invalid_cookie;
			/* Count of stale cookie read in RX path */
			uint32_t stale_cookie;
			/* Delba sent count due to RX 2k jump */
			uint32_t rx_2k_jump_delba_sent;
			/* RX 2k jump msdu indicated to stack count */
			uint32_t rx_2k_jump_to_stack;
			/* RX 2k jump msdu dropped count */
			uint32_t rx_2k_jump_drop;
			/* REO ERR msdu buffer received */
			uint32_t reo_err_msdu_buf_rcved;
			/* REO ERR msdu buffer with invalid coookie received */
			uint32_t reo_err_msdu_buf_invalid_cookie;
			/* REO OOR msdu drop count */
			uint32_t reo_err_oor_drop;
			/* REO OOR msdu indicated to stack count */
			uint32_t reo_err_oor_to_stack;
			/* REO OOR scattered msdu count */
			uint32_t reo_err_oor_sg_count;
			/* RX msdu rejected count on delivery to vdev stack_fn*/
			uint32_t rejected;
			/* Incorrect msdu count in MPDU desc info */
			uint32_t msdu_count_mismatch;
			/* RX raw frame dropped count */
			uint32_t raw_frm_drop;
			/* Stale link desc cookie count*/
			uint32_t invalid_link_cookie;
			/* Nbuf sanity failure */
			uint32_t nbuf_sanity_fail;
			/* Duplicate link desc refilled */
			uint32_t dup_refill_link_desc;
			/* Incorrect msdu continuation bit in MSDU desc */
			uint32_t msdu_continuation_err;
			/* count of start sequence (ssn) updates */
			uint32_t ssn_update_count;
			/* count of bar handling fail */
			uint32_t bar_handle_fail_count;
			/* EAPOL drop count in intrabss scenario */
			uint32_t intrabss_eapol_drop;
			/* PN check failed for 2K-jump or OOR error */
			uint32_t pn_in_dest_check_fail;
			/* MSDU len err count */
			uint32_t msdu_len_err;
			/* Rx flush count */
			uint32_t rx_flush_count;
			/* Rx invalid tid count */
			uint32_t rx_invalid_tid_err;
			/* Invalid address1 in defrag path*/
			uint32_t defrag_ad1_invalid;
			/* decrypt error drop */
			uint32_t decrypt_err_drop;
		} err;

		/* packet count per core - per ring */
		uint64_t ring_packets[NR_CPUS][MAX_REO_DEST_RINGS];
	} rx;

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
	struct reo_cmd_event_history cmd_event_history;
#endif /* WLAN_FEATURE_DP_EVENT_HISTORY */
};

union dp_align_mac_addr {
	uint8_t raw[QDF_MAC_ADDR_SIZE];
	struct {
		uint16_t bytes_ab;
		uint16_t bytes_cd;
		uint16_t bytes_ef;
	} align2;
	struct {
		uint32_t bytes_abcd;
		uint16_t bytes_ef;
	} align4;
	struct __attribute__((__packed__)) {
		uint16_t bytes_ab;
		uint32_t bytes_cdef;
	} align4_2;
};

/**
 * struct dp_ast_free_cb_params - HMWDS free callback cookie
 * @mac_addr: ast mac address
 * @peer_mac_addr: mac address of peer
 * @type: ast entry type
 * @vdev_id: vdev_id
 * @flags: ast flags
 */
struct dp_ast_free_cb_params {
	union dp_align_mac_addr mac_addr;
	union dp_align_mac_addr peer_mac_addr;
	enum cdp_txrx_ast_entry_type type;
	uint8_t vdev_id;
	uint32_t flags;
};

/**
 * struct dp_ast_entry - AST entry
 *
 * @ast_idx: Hardware AST Index
 * @peer_id: Next Hop peer_id (for non-WDS nodes, this will be point to
 *           associated peer with this MAC address)
 * @mac_addr:  MAC Address for this AST entry
 * @next_hop: Set to 1 if this is for a WDS node
 * @is_active: flag to indicate active data traffic on this node
 *             (used for aging out/expiry)
 * @ase_list_elem: node in peer AST list
 * @is_bss: flag to indicate if entry corresponds to bss peer
 * @is_mapped: flag to indicate that we have mapped the AST entry
 *             in ast_table
 * @pdev_id: pdev ID
 * @vdev_id: vdev ID
 * @ast_hash_value: hast value in HW
 * @ref_cnt: reference count
 * @type: flag to indicate type of the entry(static/WDS/MEC)
 * @delete_in_progress: Flag to indicate that delete commands send to FW
 *                      and host is waiting for response from FW
 * @callback: ast free/unmap callback
 * @cookie: argument to callback
 * @hash_list_elem: node in soc AST hash list (mac address used as hash)
 */
struct dp_ast_entry {
	uint16_t ast_idx;
	uint16_t peer_id;
	union dp_align_mac_addr mac_addr;
	bool next_hop;
	bool is_active;
	bool is_mapped;
	uint8_t pdev_id;
	uint8_t vdev_id;
	uint16_t ast_hash_value;
	qdf_atomic_t ref_cnt;
	enum cdp_txrx_ast_entry_type type;
	bool delete_in_progress;
	txrx_ast_free_cb callback;
	void *cookie;
	TAILQ_ENTRY(dp_ast_entry) ase_list_elem;
	TAILQ_ENTRY(dp_ast_entry) hash_list_elem;
};

/**
 * struct dp_mec_entry - MEC entry
 *
 * @mac_addr:  MAC Address for this MEC entry
 * @is_active: flag to indicate active data traffic on this node
 *             (used for aging out/expiry)
 * @pdev_id: pdev ID
 * @vdev_id: vdev ID
 * @hash_list_elem: node in soc MEC hash list (mac address used as hash)
 */
struct dp_mec_entry {
	union dp_align_mac_addr mac_addr;
	bool is_active;
	uint8_t pdev_id;
	uint8_t vdev_id;

	TAILQ_ENTRY(dp_mec_entry) hash_list_elem;
};

/* SOC level htt stats */
struct htt_t2h_stats {
	/* lock to protect htt_stats_msg update */
	qdf_spinlock_t lock;

	/* work queue to process htt stats */
	qdf_work_t work;

	/* T2H Ext stats message queue */
	qdf_nbuf_queue_t msg;

	/* number of completed stats in htt_stats_msg */
	uint32_t num_stats;
};

struct link_desc_bank {
	void *base_vaddr_unaligned;
	void *base_vaddr;
	qdf_dma_addr_t base_paddr_unaligned;
	qdf_dma_addr_t base_paddr;
	uint32_t size;
};

struct rx_buff_pool {
	qdf_nbuf_queue_head_t emerg_nbuf_q;
	uint32_t nbuf_fail_cnt;
	bool is_initialized;
};

struct rx_refill_buff_pool {
	bool is_initialized;
	uint16_t head;
	uint16_t tail;
	struct dp_pdev *dp_pdev;
	uint16_t max_bufq_len;
	qdf_nbuf_t buf_elem[2048];
};

#ifdef DP_TX_HW_DESC_HISTORY
#define DP_TX_HW_DESC_HIST_MAX 6144
#define DP_TX_HW_DESC_HIST_PER_SLOT_MAX 2048
#define DP_TX_HW_DESC_HIST_MAX_SLOTS 3
#define DP_TX_HW_DESC_HIST_SLOT_SHIFT 11

struct dp_tx_hw_desc_evt {
	uint8_t tcl_desc[HAL_TX_DESC_LEN_BYTES];
	uint8_t tcl_ring_id;
	uint64_t posted;
	uint32_t hp;
	uint32_t tp;
};

/* struct dp_tx_hw_desc_history - TX HW desc hisotry
 * @index: Index where the last entry is written
 * @entry: history entries
 */
struct dp_tx_hw_desc_history {
	qdf_atomic_t index;
	uint16_t num_entries_per_slot;
	uint16_t allocated;
	struct dp_tx_hw_desc_evt *entry[DP_TX_HW_DESC_HIST_MAX_SLOTS];
};
#endif

/**
 * enum dp_mon_status_process_event - Events for monitor status buffer record
 * @DP_MON_STATUS_BUF_REAP: Monitor status buffer is reaped from ring
 * @DP_MON_STATUS_BUF_ENQUEUE: Status buffer is enqueued to local queue
 * @DP_MON_STATUS_BUF_DEQUEUE: Status buffer is dequeued from local queue
 */
enum dp_mon_status_process_event {
	DP_MON_STATUS_BUF_REAP,
	DP_MON_STATUS_BUF_ENQUEUE,
	DP_MON_STATUS_BUF_DEQUEUE,
};

#ifdef WLAN_FEATURE_DP_MON_STATUS_RING_HISTORY
#define DP_MON_STATUS_HIST_MAX	2048

/**
 * struct dp_mon_stat_info_record - monitor stat ring buffer info
 * @hbi: HW ring buffer info
 * @timestamp: timestamp when this entry was recorded
 * @event: event
 * @rx_desc: RX descriptor corresponding to the received buffer
 * @nbuf: buffer attached to rx_desc, if event is REAP, else the buffer
 *	  which was enqueued or dequeued.
 * @rx_desc_nbuf_data: nbuf data pointer.
 */
struct dp_mon_stat_info_record {
	struct hal_buf_info hbi;
	uint64_t timestamp;
	enum dp_mon_status_process_event event;
	void *rx_desc;
	qdf_nbuf_t nbuf;
	uint8_t *rx_desc_nbuf_data;
};

/* struct dp_rx_history - rx ring hisotry
 * @index: Index where the last entry is written
 * @entry: history entries
 */
struct dp_mon_status_ring_history {
	qdf_atomic_t index;
	struct dp_mon_stat_info_record entry[DP_MON_STATUS_HIST_MAX];
};
#endif

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
/*
 * The logic for get current index of these history is dependent on this
 * value being power of 2.
 */
#define DP_RX_HIST_MAX 2048
#define DP_RX_ERR_HIST_MAX 2048
#define DP_RX_REINJECT_HIST_MAX 1024
#define DP_RX_REFILL_HIST_MAX 2048

QDF_COMPILE_TIME_ASSERT(rx_history_size,
			(DP_RX_HIST_MAX &
			 (DP_RX_HIST_MAX - 1)) == 0);
QDF_COMPILE_TIME_ASSERT(rx_err_history_size,
			(DP_RX_ERR_HIST_MAX &
			 (DP_RX_ERR_HIST_MAX - 1)) == 0);
QDF_COMPILE_TIME_ASSERT(rx_reinject_history_size,
			(DP_RX_REINJECT_HIST_MAX &
			 (DP_RX_REINJECT_HIST_MAX - 1)) == 0);
QDF_COMPILE_TIME_ASSERT(rx_refill_history_size,
			(DP_RX_REFILL_HIST_MAX &
			(DP_RX_REFILL_HIST_MAX - 1)) == 0);


/**
 * struct dp_buf_info_record - ring buffer info
 * @hbi: HW ring buffer info
 * @timestamp: timestamp when this entry was recorded
 */
struct dp_buf_info_record {
	struct hal_buf_info hbi;
	uint64_t timestamp;
};

/**
 * struct dp_refill_info_record - ring refill buffer info
 * @hp: HP value after refill
 * @tp: cached tail value during refill
 * @num_req: number of buffers requested to refill
 * @num_refill: number of buffers refilled to ring
 * @timestamp: timestamp when this entry was recorded
 */
struct dp_refill_info_record {
	uint32_t hp;
	uint32_t tp;
	uint32_t num_req;
	uint32_t num_refill;
	uint64_t timestamp;
};

/**
 * struct dp_rx_history - rx ring hisotry
 * @index: Index where the last entry is written
 * @entry: history entries
 */
struct dp_rx_history {
	qdf_atomic_t index;
	struct dp_buf_info_record entry[DP_RX_HIST_MAX];
};

/**
 * struct dp_rx_err_history - rx err ring hisotry
 * @index: Index where the last entry is written
 * @entry: history entries
 */
struct dp_rx_err_history {
	qdf_atomic_t index;
	struct dp_buf_info_record entry[DP_RX_ERR_HIST_MAX];
};

/**
 * struct dp_rx_reinject_history - rx reinject ring hisotry
 * @index: Index where the last entry is written
 * @entry: history entries
 */
struct dp_rx_reinject_history {
	qdf_atomic_t index;
	struct dp_buf_info_record entry[DP_RX_REINJECT_HIST_MAX];
};

/**
 * struct dp_rx_refill_history - rx buf refill hisotry
 * @index: Index where the last entry is written
 * @entry: history entries
 */
struct dp_rx_refill_history {
	qdf_atomic_t index;
	struct dp_refill_info_record entry[DP_RX_REFILL_HIST_MAX];
};

#endif

/**
 * enum dp_cfg_event_type - Datapath config events type
 * @DP_CFG_EVENT_VDEV_ATTACH: vdev attach
 * @DP_CFG_EVENT_VDEV_DETACH: vdev detach
 * @DP_CFG_EVENT_VDEV_UNREF_DEL: vdev memory free after last ref is released
 * @DP_CFG_EVENT_PEER_CREATE: peer create
 * @DP_CFG_EVENT_PEER_DELETE: peer delete
 * @DP_CFG_EVENT_PEER_UNREF_DEL: peer memory free after last ref is released
 * @DP_CFG_EVENT_PEER_SETUP: peer setup
 * @DP_CFG_EVENT_MLO_ADD_LINK: add link peer to mld peer
 * @DP_CFG_EVENT_MLO_DEL_LINK: delete link peer from mld peer
 * @DP_CFG_EVENT_MLO_SETUP: MLO peer setup
 * @DP_CFG_EVENT_MLO_SETUP_VDEV_UPDATE: MLD peer vdev update
 * @DP_CFG_EVENT_PEER_MAP: peer map
 * @DP_CFG_EVENT_PEER_UNMAP: peer unmap
 * @DP_CFG_EVENT_MLO_PEER_MAP: MLD peer map
 * @DP_CFG_EVENT_MLO_PEER_UNMAP: MLD peer unmap
 */
enum dp_cfg_event_type {
	DP_CFG_EVENT_VDEV_ATTACH,
	DP_CFG_EVENT_VDEV_DETACH,
	DP_CFG_EVENT_VDEV_UNREF_DEL,
	DP_CFG_EVENT_PEER_CREATE,
	DP_CFG_EVENT_PEER_DELETE,
	DP_CFG_EVENT_PEER_UNREF_DEL,
	DP_CFG_EVENT_PEER_SETUP,
	DP_CFG_EVENT_MLO_ADD_LINK,
	DP_CFG_EVENT_MLO_DEL_LINK,
	DP_CFG_EVENT_MLO_SETUP,
	DP_CFG_EVENT_MLO_SETUP_VDEV_UPDATE,
	DP_CFG_EVENT_PEER_MAP,
	DP_CFG_EVENT_PEER_UNMAP,
	DP_CFG_EVENT_MLO_PEER_MAP,
	DP_CFG_EVENT_MLO_PEER_UNMAP,
};

#ifdef WLAN_FEATURE_DP_CFG_EVENT_HISTORY
/* Size must be in 2 power, for bitwise index rotation */
#define DP_CFG_EVT_HISTORY_SIZE 0x800
#define DP_CFG_EVT_HIST_PER_SLOT_MAX 256
#define DP_CFG_EVT_HIST_MAX_SLOTS 8
#define DP_CFG_EVT_HIST_SLOT_SHIFT 8

/**
 * struct dp_vdev_attach_detach_desc - vdev ops descriptor
 * @vdev: DP vdev handle
 * @mac_addr: vdev mac address
 * @vdev_id: vdev id
 * @ref_count: vdev ref count
 */
struct dp_vdev_attach_detach_desc {
	struct dp_vdev *vdev;
	union dp_align_mac_addr mac_addr;
	uint8_t vdev_id;
	int32_t ref_count;
};

/**
 * struct dp_peer_cmn_ops_desc - peer events descriptor
 * @vdev_id: vdev_id of the vdev on which peer exists
 * @is_reuse: indicates if its a peer reuse case, during peer create
 * @peer: DP peer handle
 * @vdev: DP vdev handle on which peer exists
 * @mac_addr: peer mac address
 * @vdev_mac_addr: vdev mac address
 * @vdev_ref_count: vdev ref count
 * @peer_ref_count: peer ref count
 */
struct dp_peer_cmn_ops_desc {
	uint8_t vdev_id : 5,
		is_reuse : 1;
	struct dp_peer *peer;
	struct dp_vdev *vdev;
	union dp_align_mac_addr mac_addr;
	union dp_align_mac_addr vdev_mac_addr;
	int32_t vdev_ref_count;
	int32_t peer_ref_count;
};

/**
 * struct dp_mlo_add_del_link_desc - MLO add/del link event descriptor
 * @idx: index at which link peer got added in MLD peer's list
 * @num_links: num links added in the MLD peer's list
 * @action_result: add/del was success or not
 * @link_peer: link peer handle
 * @mld_peer: MLD peer handle
 * @link_mac_addr: link peer mac address
 * @mld_mac_addr: MLD peer mac address
 */
struct dp_mlo_add_del_link_desc {
	uint8_t idx : 3,
		num_links : 3,
		action_result : 1,
		reserved : 1;
	struct dp_peer *link_peer;
	struct dp_peer *mld_peer;
	union dp_align_mac_addr link_mac_addr;
	union dp_align_mac_addr mld_mac_addr;
};

/**
 * struct dp_mlo_setup_vdev_update_desc - MLD peer vdev update event desc
 * @mld_peer: MLD peer handle
 * @prev_vdev: previous vdev handle
 * @new_vdev: new vdev handle
 */
struct dp_mlo_setup_vdev_update_desc {
	struct dp_peer *mld_peer;
	struct dp_vdev *prev_vdev;
	struct dp_vdev *new_vdev;
};

/**
 * struct dp_rx_peer_map_unmap_desc - peer map/unmap event descriptor
 * @peer_id: peer id
 * @ml_peer_id: ML peer id, if its an MLD peer
 * @hw_peer_id: hw peer id
 * @vdev_id: vdev id of the peer
 * @is_ml_peer: is this MLD peer
 * @mac_addr: mac address of the peer
 * @peer: peer handle
 */
struct dp_rx_peer_map_unmap_desc {
	uint16_t peer_id;
	uint16_t ml_peer_id;
	uint16_t hw_peer_id;
	uint8_t vdev_id;
	uint8_t is_ml_peer;
	union dp_align_mac_addr mac_addr;
	struct dp_peer *peer;
};

/**
 * struct dp_peer_setup_desc - peer setup event descriptor
 * @peer: DP peer handle
 * @vdev: vdev handle on which peer exists
 * @vdev_ref_count: vdev ref count
 * @mac_addr: peer mac address
 * @mld_mac_addr: MLD mac address
 * @is_first_link: is the current link the first link created
 * @is_primary_link: is the current link primary link
 * @vdev_id: vdev id of the vdev on which the current link peer exists
 */
struct dp_peer_setup_desc {
	struct dp_peer *peer;
	struct dp_vdev *vdev;
	int32_t vdev_ref_count;
	union dp_align_mac_addr mac_addr;
	union dp_align_mac_addr mld_mac_addr;
	uint8_t is_first_link : 1,
		is_primary_link : 1,
		vdev_id : 5,
		reserved : 1;
};

/**
 * union dp_cfg_event_desc - DP config event descriptor
 * @vdev_evt: vdev events desc
 * @peer_cmn_evt: common peer events desc
 * @peer_setup_evt: peer setup event desc
 * @mlo_link_delink_evt: MLO link/delink event desc
 * @mlo_setup_vdev_update: MLD peer vdev update event desc
 * @peer_map_unmap_evt: peer map/unmap event desc
 */
union dp_cfg_event_desc {
	struct dp_vdev_attach_detach_desc vdev_evt;
	struct dp_peer_cmn_ops_desc peer_cmn_evt;
	struct dp_peer_setup_desc peer_setup_evt;
	struct dp_mlo_add_del_link_desc mlo_link_delink_evt;
	struct dp_mlo_setup_vdev_update_desc mlo_setup_vdev_update;
	struct dp_rx_peer_map_unmap_desc peer_map_unmap_evt;
};

/**
 * struct dp_cfg_event - DP config event descriptor
 * @timestamp: timestamp at which event was recorded
 * @type: event type
 * @event_desc: event descriptor
 */
struct dp_cfg_event {
	uint64_t timestamp;
	enum dp_cfg_event_type type;
	union dp_cfg_event_desc event_desc;
};

/**
 * struct dp_cfg_event_history - DP config event history
 * @index: current index
 * @num_entries_per_slot: number of entries per slot
 * @allocated: Is the history allocated or not
 * @entry: event history descriptors
 */
struct dp_cfg_event_history {
	qdf_atomic_t index;
	uint16_t num_entries_per_slot;
	uint16_t allocated;
	struct dp_cfg_event *entry[DP_CFG_EVT_HIST_MAX_SLOTS];
};
#endif

enum dp_tx_event_type {
	DP_TX_DESC_INVAL_EVT = 0,
	DP_TX_DESC_MAP,
	DP_TX_DESC_COOKIE,
	DP_TX_DESC_FLUSH,
	DP_TX_DESC_UNMAP,
	DP_TX_COMP_UNMAP,
	DP_TX_COMP_UNMAP_ERR,
	DP_TX_COMP_MSDU_EXT,
};

#ifdef WLAN_FEATURE_DP_TX_DESC_HISTORY
/* Size must be in 2 power, for bitwise index rotation */
#define DP_TX_TCL_HISTORY_SIZE 0x4000
#define DP_TX_TCL_HIST_PER_SLOT_MAX 2048
#define DP_TX_TCL_HIST_MAX_SLOTS 8
#define DP_TX_TCL_HIST_SLOT_SHIFT 11

/* Size must be in 2 power, for bitwise index rotation */
#define DP_TX_COMP_HISTORY_SIZE 0x4000
#define DP_TX_COMP_HIST_PER_SLOT_MAX 2048
#define DP_TX_COMP_HIST_MAX_SLOTS 8
#define DP_TX_COMP_HIST_SLOT_SHIFT 11

struct dp_tx_desc_event {
	qdf_nbuf_t skb;
	dma_addr_t paddr;
	uint32_t sw_cookie;
	enum dp_tx_event_type type;
	uint64_t ts;
};

struct dp_tx_tcl_history {
	qdf_atomic_t index;
	uint16_t num_entries_per_slot;
	uint16_t allocated;
	struct dp_tx_desc_event *entry[DP_TX_TCL_HIST_MAX_SLOTS];
};

struct dp_tx_comp_history {
	qdf_atomic_t index;
	uint16_t num_entries_per_slot;
	uint16_t allocated;
	struct dp_tx_desc_event *entry[DP_TX_COMP_HIST_MAX_SLOTS];
};
#endif /* WLAN_FEATURE_DP_TX_DESC_HISTORY */

/* structure to record recent operation related variable */
struct dp_last_op_info {
	/* last link desc buf info through WBM release ring */
	struct hal_buf_info wbm_rel_link_desc;
	/* last link desc buf info through REO reinject ring */
	struct hal_buf_info reo_reinject_link_desc;
};

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR

/**
 * struct dp_swlm_tcl_data - params for tcl register write coalescing
 *			     decision making
 * @nbuf: TX packet
 * @tid: tid for transmitting the current packet
 * @num_ll_connections: Number of low latency connections on this vdev
 * @ring_id: TCL ring id
 * @pkt_len: Packet length
 *
 * This structure contains the information required by the software
 * latency manager to decide on whether to coalesce the current TCL
 * register write or not.
 */
struct dp_swlm_tcl_data {
	qdf_nbuf_t nbuf;
	uint8_t tid;
	uint8_t num_ll_connections;
	uint8_t ring_id;
	uint32_t pkt_len;
};

/**
 * union swlm_data - SWLM query data
 * @tcl_data: data for TCL query in SWLM
 */
union swlm_data {
	struct dp_swlm_tcl_data *tcl_data;
};

/**
 * struct dp_swlm_ops - SWLM ops
 * @tcl_wr_coalesce_check: handler to check if the current TCL register
 *			   write can be coalesced or not
 */
struct dp_swlm_ops {
	int (*tcl_wr_coalesce_check)(struct dp_soc *soc,
				     struct dp_swlm_tcl_data *tcl_data);
};

/**
 * struct dp_swlm_stats - Stats for Software Latency manager.
 * @tcl: TCL stats
 * @tcl.timer_flush_success: Num TCL HP writes success from timer context
 * @tcl.timer_flush_fail: Num TCL HP writes failure from timer context
 * @tcl.tid_fail: Num TCL register write coalescing skips, since the pkt
 *		 was being transmitted on a TID above coalescing threshold
 * @tcl.sp_frames: Num TCL register write coalescing skips, since the pkt
 *		  being transmitted was a special frame
 * @tcl.ll_connection: Num TCL register write coalescing skips, since the
 *		       vdev has low latency connections
 * @tcl.bytes_thresh_reached: Num TCL HP writes flush after the coalescing
 *			     bytes threshold was reached
 * @tcl.time_thresh_reached: Num TCL HP writes flush after the coalescing
 *			    session time expired
 * @tcl.tput_criteria_fail: Num TCL HP writes coalescing fails, since the
 *			   throughput did not meet session threshold
 * @tcl.coalesce_success: Num of TCL HP writes coalesced successfully.
 * @tcl.coalesce_fail: Num of TCL HP writes coalesces failed
 */
struct dp_swlm_stats {
	struct {
		uint32_t timer_flush_success;
		uint32_t timer_flush_fail;
		uint32_t tid_fail;
		uint32_t sp_frames;
		uint32_t ll_connection;
		uint32_t bytes_thresh_reached;
		uint32_t time_thresh_reached;
		uint32_t tput_criteria_fail;
		uint32_t coalesce_success;
		uint32_t coalesce_fail;
	} tcl[MAX_TCL_DATA_RINGS];
};

/**
 * struct dp_swlm_tcl_params: Parameters based on TCL for different modules
 *			      in the Software latency manager.
 * @soc: DP soc reference
 * @ring_id: TCL ring id
 * @flush_timer: Timer for flushing the coalesced TCL HP writes
 * @sampling_session_tx_bytes: Num bytes transmitted in the sampling time
 * @bytes_flush_thresh: Bytes threshold to flush the TCL HP register write
 * @coalesce_end_time: End timestamp for current coalescing session
 * @bytes_coalesced: Num bytes coalesced in the current session
 * @prev_tx_packets: Previous TX packets accounted
 * @prev_tx_bytes: Previous TX bytes accounted
 * @prev_rx_bytes: Previous RX bytes accounted
 * @expire_time: expiry time for sample
 * @tput_pass_cnt: threshold throughput pass counter
 */
struct dp_swlm_tcl_params {
	struct dp_soc *soc;
	uint32_t ring_id;
	qdf_timer_t flush_timer;
	uint32_t sampling_session_tx_bytes;
	uint32_t bytes_flush_thresh;
	uint64_t coalesce_end_time;
	uint32_t bytes_coalesced;
	uint32_t prev_tx_packets;
	uint32_t prev_tx_bytes;
	uint32_t prev_rx_bytes;
	uint64_t expire_time;
	uint32_t tput_pass_cnt;
};

/**
 * struct dp_swlm_params: Parameters for different modules in the
 *			  Software latency manager.
 * @rx_traffic_thresh: Threshold for RX traffic, to begin TCL register
 *			   write coalescing
 * @tx_traffic_thresh: Threshold for TX traffic, to begin TCL register
 *			   write coalescing
 * @sampling_time: Sampling time to test the throughput threshold
 * @time_flush_thresh: Time threshold to flush the TCL HP register write
 * @tx_thresh_multiplier: Multiplier to deduce the bytes threshold after
 *			      which the TCL HP register is written, thereby
 *			      ending the coalescing.
 * @tx_pkt_thresh: Threshold for TX packet count, to begin TCL register
 *		       write coalescing
 * @tcl: TCL ring specific params
 */

struct dp_swlm_params {
	uint32_t rx_traffic_thresh;
	uint32_t tx_traffic_thresh;
	uint32_t sampling_time;
	uint32_t time_flush_thresh;
	uint32_t tx_thresh_multiplier;
	uint32_t tx_pkt_thresh;
	struct dp_swlm_tcl_params tcl[MAX_TCL_DATA_RINGS];
};

/**
 * struct dp_swlm - Software latency manager context
 * @ops: SWLM ops pointers
 * @is_enabled: SWLM enabled/disabled
 * @is_init: SWLM module initialized
 * @stats: SWLM stats
 * @params: SWLM SRNG params
 * @tcl_flush_timer: flush timer for TCL register writes
 */
struct dp_swlm {
	struct dp_swlm_ops *ops;
	uint8_t is_enabled:1,
		is_init:1;
	struct dp_swlm_stats stats;
	struct dp_swlm_params params;
};
#endif

#ifdef IPA_OFFLOAD
/* IPA uC datapath offload Wlan Tx resources */
struct ipa_dp_tx_rsc {
	/* Resource info to be passed to IPA */
	qdf_dma_addr_t ipa_tcl_ring_base_paddr;
	void *ipa_tcl_ring_base_vaddr;
	uint32_t ipa_tcl_ring_size;
	qdf_dma_addr_t ipa_tcl_hp_paddr;
	uint32_t alloc_tx_buf_cnt;

	qdf_dma_addr_t ipa_wbm_ring_base_paddr;
	void *ipa_wbm_ring_base_vaddr;
	uint32_t ipa_wbm_ring_size;
	qdf_dma_addr_t ipa_wbm_tp_paddr;
	/* WBM2SW HP shadow paddr */
	qdf_dma_addr_t ipa_wbm_hp_shadow_paddr;

	/* TX buffers populated into the WBM ring */
	void **tx_buf_pool_vaddr_unaligned;
	qdf_dma_addr_t *tx_buf_pool_paddr_unaligned;
};

/* IPA uC datapath offload Wlan Rx resources */
struct ipa_dp_rx_rsc {
	/* Resource info to be passed to IPA */
	qdf_dma_addr_t ipa_reo_ring_base_paddr;
	void *ipa_reo_ring_base_vaddr;
	uint32_t ipa_reo_ring_size;
	qdf_dma_addr_t ipa_reo_tp_paddr;

	/* Resource info to be passed to firmware and IPA */
	qdf_dma_addr_t ipa_rx_refill_buf_ring_base_paddr;
	void *ipa_rx_refill_buf_ring_base_vaddr;
	uint32_t ipa_rx_refill_buf_ring_size;
	qdf_dma_addr_t ipa_rx_refill_buf_hp_paddr;
};
#endif

struct dp_tx_msdu_info_s;
/**
 * enum dp_context_type- DP Context Type
 * @DP_CONTEXT_TYPE_SOC: Context type DP SOC
 * @DP_CONTEXT_TYPE_PDEV: Context type DP PDEV
 * @DP_CONTEXT_TYPE_VDEV: Context type DP VDEV
 * @DP_CONTEXT_TYPE_PEER: Context type DP PEER
 * @DP_CONTEXT_TYPE_MON_SOC: Context type DP MON SOC
 * @DP_CONTEXT_TYPE_MON_PDEV: Context type DP MON PDEV
 *
 * Helper enums to be used to retrieve the size of the corresponding
 * data structure by passing the type.
 */
enum dp_context_type {
	DP_CONTEXT_TYPE_SOC,
	DP_CONTEXT_TYPE_PDEV,
	DP_CONTEXT_TYPE_VDEV,
	DP_CONTEXT_TYPE_PEER,
	DP_CONTEXT_TYPE_MON_SOC,
	DP_CONTEXT_TYPE_MON_PDEV
};

/**
 * struct dp_arch_ops - DP target specific arch ops
 * @txrx_soc_attach:
 * @txrx_soc_detach:
 * @txrx_soc_init:
 * @txrx_soc_deinit:
 * @txrx_soc_srng_alloc:
 * @txrx_soc_srng_init:
 * @txrx_soc_srng_deinit:
 * @txrx_soc_srng_free:
 * @txrx_pdev_attach:
 * @txrx_pdev_detach:
 * @txrx_vdev_attach:
 * @txrx_vdev_detach:
 * @txrx_peer_map_attach:
 * @txrx_peer_map_detach:
 * @dp_rxdma_ring_sel_cfg:
 * @soc_cfg_attach:
 * @txrx_peer_setup:
 * @peer_get_reo_hash:
 * @reo_remap_config:
 * @tx_hw_enqueue: enqueue TX data to HW
 * @tx_comp_get_params_from_hal_desc: get software tx descriptor and release
 * 				      source from HAL desc for wbm release ring
 * @dp_tx_process_htt_completion:
 * @dp_rx_process:
 * @dp_tx_send_fast:
 * @dp_tx_desc_pool_init:
 * @dp_tx_desc_pool_deinit:
 * @dp_rx_desc_pool_init:
 * @dp_rx_desc_pool_deinit:
 * @dp_wbm_get_rx_desc_from_hal_desc:
 * @dp_rx_intrabss_mcast_handler:
 * @dp_rx_word_mask_subscribe:
 * @dp_rx_desc_cookie_2_va:
 * @dp_service_near_full_srngs: Handler for servicing the near full IRQ
 * @tx_implicit_rbm_set:
 * @dp_rx_peer_metadata_peer_id_get:
 * @dp_rx_chain_msdus:
 * @txrx_set_vdev_param: target specific ops while setting vdev params
 * @txrx_get_vdev_mcast_param: target specific ops for getting vdev
 *			       params related to multicast
 * @txrx_get_context_size:
 * @txrx_get_mon_context_size:
 * @dp_srng_test_and_update_nf_params: Check if the srng is in near full state
 *				and set the near-full params.
 * @dp_tx_mcast_handler:
 * @dp_rx_mcast_handler:
 * @dp_tx_is_mcast_primary:
 * @dp_soc_get_by_idle_bm_id:
 * @mlo_peer_find_hash_detach:
 * @mlo_peer_find_hash_attach:
 * @mlo_peer_find_hash_add:
 * @mlo_peer_find_hash_remove:
 * @mlo_peer_find_hash_find:
 * @get_reo_qdesc_addr:
 * @get_rx_hash_key:
 * @dp_set_rx_fst:
 * @dp_get_rx_fst:
 * @dp_rx_fst_deref:
 * @dp_rx_fst_ref:
 * @txrx_print_peer_stats:
 * @dp_peer_rx_reorder_queue_setup: Dp peer reorder queue setup
 * @dp_find_peer_by_destmac:
 * @dp_bank_reconfig:
 * @dp_rx_replenish_soc_get:
 * @dp_soc_get_num_soc:
 * @dp_reconfig_tx_vdev_mcast_ctrl:
 * @dp_cc_reg_cfg_init:
 * @dp_tx_compute_hw_delay:
 * @print_mlo_ast_stats:
 * @dp_partner_chips_map:
 * @dp_partner_chips_unmap:
 * @ipa_get_bank_id: Get TCL bank id used by IPA
 * @dp_txrx_ppeds_rings_status:
 * @dp_tx_ppeds_inuse_desc:
 * @dp_tx_ppeds_cfg_astidx_cache_mapping:
 * @txrx_soc_ppeds_start:
 * @txrx_soc_ppeds_stop:
 * @dp_register_ppeds_interrupts:
 * @dp_free_ppeds_interrupts:
 */
struct dp_arch_ops {
	/* INIT/DEINIT Arch Ops */
	QDF_STATUS (*txrx_soc_attach)(struct dp_soc *soc,
				      struct cdp_soc_attach_params *params);
	QDF_STATUS (*txrx_soc_detach)(struct dp_soc *soc);
	QDF_STATUS (*txrx_soc_init)(struct dp_soc *soc);
	QDF_STATUS (*txrx_soc_deinit)(struct dp_soc *soc);
	QDF_STATUS (*txrx_soc_srng_alloc)(struct dp_soc *soc);
	QDF_STATUS (*txrx_soc_srng_init)(struct dp_soc *soc);
	void (*txrx_soc_srng_deinit)(struct dp_soc *soc);
	void (*txrx_soc_srng_free)(struct dp_soc *soc);
	QDF_STATUS (*txrx_pdev_attach)(struct dp_pdev *pdev,
				       struct cdp_pdev_attach_params *params);
	QDF_STATUS (*txrx_pdev_detach)(struct dp_pdev *pdev);
	QDF_STATUS (*txrx_vdev_attach)(struct dp_soc *soc,
				       struct dp_vdev *vdev);
	QDF_STATUS (*txrx_vdev_detach)(struct dp_soc *soc,
				       struct dp_vdev *vdev);
	QDF_STATUS (*txrx_peer_map_attach)(struct dp_soc *soc);
	void (*txrx_peer_map_detach)(struct dp_soc *soc);
	QDF_STATUS (*dp_rxdma_ring_sel_cfg)(struct dp_soc *soc);
	void (*soc_cfg_attach)(struct dp_soc *soc);
	QDF_STATUS (*txrx_peer_setup)(struct dp_soc *soc,
				      struct dp_peer *peer);
	void (*peer_get_reo_hash)(struct dp_vdev *vdev,
				  struct cdp_peer_setup_info *setup_info,
				  enum cdp_host_reo_dest_ring *reo_dest,
				  bool *hash_based,
				  uint8_t *lmac_peer_id_msb);
	 bool (*reo_remap_config)(struct dp_soc *soc, uint32_t *remap0,
				  uint32_t *remap1, uint32_t *remap2);

	/* TX RX Arch Ops */
	QDF_STATUS (*tx_hw_enqueue)(struct dp_soc *soc, struct dp_vdev *vdev,
				    struct dp_tx_desc_s *tx_desc,
				    uint16_t fw_metadata,
				    struct cdp_tx_exception_metadata *metadata,
				    struct dp_tx_msdu_info_s *msdu_info);

	void (*tx_comp_get_params_from_hal_desc)(struct dp_soc *soc,
						 void *tx_comp_hal_desc,
						 struct dp_tx_desc_s **desc);
	void (*dp_tx_process_htt_completion)(struct dp_soc *soc,
					     struct dp_tx_desc_s *tx_desc,
					     uint8_t *status,
					     uint8_t ring_id);

	uint32_t (*dp_rx_process)(struct dp_intr *int_ctx,
				  hal_ring_handle_t hal_ring_hdl,
				  uint8_t reo_ring_num, uint32_t quota);

	qdf_nbuf_t (*dp_tx_send_fast)(struct cdp_soc_t *soc_hdl,
				      uint8_t vdev_id,
				      qdf_nbuf_t nbuf);

	QDF_STATUS (*dp_tx_desc_pool_init)(struct dp_soc *soc,
					   uint32_t num_elem,
					   uint8_t pool_id);
	void (*dp_tx_desc_pool_deinit)(
				struct dp_soc *soc,
				struct dp_tx_desc_pool_s *tx_desc_pool,
				uint8_t pool_id);

	QDF_STATUS (*dp_rx_desc_pool_init)(struct dp_soc *soc,
					   struct rx_desc_pool *rx_desc_pool,
					   uint32_t pool_id);
	void (*dp_rx_desc_pool_deinit)(struct dp_soc *soc,
				       struct rx_desc_pool *rx_desc_pool,
				       uint32_t pool_id);

	QDF_STATUS (*dp_wbm_get_rx_desc_from_hal_desc)(
						struct dp_soc *soc,
						void *ring_desc,
						struct dp_rx_desc **r_rx_desc);

	bool
	(*dp_rx_intrabss_mcast_handler)(struct dp_soc *soc,
					struct dp_txrx_peer *ta_txrx_peer,
					qdf_nbuf_t nbuf_copy,
					struct cdp_tid_rx_stats *tid_stats);

	void (*dp_rx_word_mask_subscribe)(
				struct dp_soc *soc,
				uint32_t *msg_word,
				void *rx_filter);

	struct dp_rx_desc *(*dp_rx_desc_cookie_2_va)(struct dp_soc *soc,
						     uint32_t cookie);
	uint32_t (*dp_service_near_full_srngs)(struct dp_soc *soc,
					       struct dp_intr *int_ctx,
					       uint32_t dp_budget);
	void (*tx_implicit_rbm_set)(struct dp_soc *soc, uint8_t tx_ring_id,
				    uint8_t bm_id);
	uint16_t (*dp_rx_peer_metadata_peer_id_get)(struct dp_soc *soc,
						    uint32_t peer_metadata);
	bool (*dp_rx_chain_msdus)(struct dp_soc *soc, qdf_nbuf_t nbuf,
				  uint8_t *rx_tlv_hdr, uint8_t mac_id);
	/* Control Arch Ops */
	QDF_STATUS (*txrx_set_vdev_param)(struct dp_soc *soc,
					  struct dp_vdev *vdev,
					  enum cdp_vdev_param_type param,
					  cdp_config_param_type val);

	QDF_STATUS (*txrx_get_vdev_mcast_param)(struct dp_soc *soc,
						struct dp_vdev *vdev,
						cdp_config_param_type *val);

	/* Misc Arch Ops */
	qdf_size_t (*txrx_get_context_size)(enum dp_context_type);
#ifdef WIFI_MONITOR_SUPPORT
	qdf_size_t (*txrx_get_mon_context_size)(enum dp_context_type);
#endif
	int (*dp_srng_test_and_update_nf_params)(struct dp_soc *soc,
						 struct dp_srng *dp_srng,
						 int *max_reap_limit);

	/* MLO ops */
#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_MCAST_MLO
	void (*dp_tx_mcast_handler)(struct dp_soc *soc, struct dp_vdev *vdev,
				    qdf_nbuf_t nbuf);
	bool (*dp_rx_mcast_handler)(struct dp_soc *soc, struct dp_vdev *vdev,
				    struct dp_txrx_peer *peer, qdf_nbuf_t nbuf);
	bool (*dp_tx_is_mcast_primary)(struct dp_soc *soc,
				       struct dp_vdev *vdev);
#endif
	struct dp_soc * (*dp_soc_get_by_idle_bm_id)(struct dp_soc *soc,
						    uint8_t bm_id);

	void (*mlo_peer_find_hash_detach)(struct dp_soc *soc);
	QDF_STATUS (*mlo_peer_find_hash_attach)(struct dp_soc *soc);
	void (*mlo_peer_find_hash_add)(struct dp_soc *soc,
				       struct dp_peer *peer);
	void (*mlo_peer_find_hash_remove)(struct dp_soc *soc,
					  struct dp_peer *peer);

	struct dp_peer *(*mlo_peer_find_hash_find)(struct dp_soc *soc,
						   uint8_t *peer_mac_addr,
						   int mac_addr_is_aligned,
						   enum dp_mod_id mod_id,
						   uint8_t vdev_id);
#endif
	uint64_t (*get_reo_qdesc_addr)(hal_soc_handle_t hal_soc_hdl,
				       uint8_t *dst_ring_desc,
				       uint8_t *buf,
				       struct dp_txrx_peer *peer,
				       unsigned int tid);
	void (*get_rx_hash_key)(struct dp_soc *soc,
				struct cdp_lro_hash_config *lro_hash);
	void (*dp_set_rx_fst)(struct dp_soc *soc, struct dp_rx_fst *fst);
	struct dp_rx_fst *(*dp_get_rx_fst)(struct dp_soc *soc);
	uint8_t (*dp_rx_fst_deref)(struct dp_soc *soc);
	void (*dp_rx_fst_ref)(struct dp_soc *soc);
	void (*txrx_print_peer_stats)(struct cdp_peer_stats *peer_stats,
				      enum peer_stats_type stats_type);
	QDF_STATUS (*dp_peer_rx_reorder_queue_setup)(struct dp_soc *soc,
						     struct dp_peer *peer,
						     int tid,
						     uint32_t ba_window_size);
	struct dp_peer *(*dp_find_peer_by_destmac)(struct dp_soc *soc,
						   uint8_t *dest_mac_addr,
						   uint8_t vdev_id);
	void (*dp_bank_reconfig)(struct dp_soc *soc, struct dp_vdev *vdev);

	struct dp_soc * (*dp_rx_replenish_soc_get)(struct dp_soc *soc,
						   uint8_t chip_id);

	uint8_t (*dp_soc_get_num_soc)(struct dp_soc *soc);
	void (*dp_reconfig_tx_vdev_mcast_ctrl)(struct dp_soc *soc,
					       struct dp_vdev *vdev);

	void (*dp_cc_reg_cfg_init)(struct dp_soc *soc, bool is_4k_align);

	QDF_STATUS
	(*dp_tx_compute_hw_delay)(struct dp_soc *soc,
				  struct dp_vdev *vdev,
				  struct hal_tx_completion_status *ts,
				  uint32_t *delay_us);
	void (*print_mlo_ast_stats)(struct dp_soc *soc);
	void (*dp_partner_chips_map)(struct dp_soc *soc,
				     struct dp_peer *peer,
				     uint16_t peer_id);
	void (*dp_partner_chips_unmap)(struct dp_soc *soc,
				       uint16_t peer_id);

#ifdef IPA_OFFLOAD
	int8_t (*ipa_get_bank_id)(struct dp_soc *soc);
#endif
#ifdef WLAN_SUPPORT_PPEDS
	void (*dp_txrx_ppeds_rings_status)(struct dp_soc *soc);
	void (*dp_tx_ppeds_inuse_desc)(struct dp_soc *soc);
	void (*dp_tx_ppeds_cfg_astidx_cache_mapping)(struct dp_soc *soc,
						     struct dp_vdev *vdev,
						     bool peer_map);
#endif
	QDF_STATUS (*txrx_soc_ppeds_start)(struct dp_soc *soc);
	void (*txrx_soc_ppeds_stop)(struct dp_soc *soc);
	int (*dp_register_ppeds_interrupts)(struct dp_soc *soc,
					    struct dp_srng *srng, int vector,
					    int ring_type, int ring_num);
	void (*dp_free_ppeds_interrupts)(struct dp_soc *soc,
					 struct dp_srng *srng, int ring_type,
					 int ring_num);
};

/**
 * struct dp_soc_features: Data structure holding the SOC level feature flags.
 * @pn_in_reo_dest: PN provided by hardware in the REO destination ring.
 * @dmac_cmn_src_rxbuf_ring_enabled: Flag to indicate DMAC mode common Rx
 *				     buffer source rings
 * @rssi_dbm_conv_support: Rssi dbm conversion support param.
 * @umac_hw_reset_support: UMAC HW reset support
 * @wds_ext_ast_override_enable:
 */
struct dp_soc_features {
	uint8_t pn_in_reo_dest:1,
		dmac_cmn_src_rxbuf_ring_enabled:1;
	bool rssi_dbm_conv_support;
	bool umac_hw_reset_support;
	bool wds_ext_ast_override_enable;
};

enum sysfs_printing_mode {
	PRINTING_MODE_DISABLED = 0,
	PRINTING_MODE_ENABLED
};

/**
 * typedef notify_pre_reset_fw_callback() - pre-reset callback
 * @soc: DP SoC
 */
typedef void (*notify_pre_reset_fw_callback)(struct dp_soc *soc);

#ifdef WLAN_SYSFS_DP_STATS
/**
 * struct sysfs_stats_config: Data structure holding stats sysfs config.
 * @rw_stats_lock: Lock to read and write to stat_type and pdev_id.
 * @sysfs_read_lock: Lock held while another stat req is being executed.
 * @sysfs_write_user_buffer: Lock to change buff len, max buf len
 * and *buf.
 * @sysfs_txrx_fw_request_done: Event to wait for firmware response.
 * @stat_type_requested: stat type requested.
 * @mac_id: mac id for which stat type are requested.
 * @printing_mode: Should a print go through.
 * @process_id: Process allowed to write to buffer.
 * @curr_buffer_length: Curr length of buffer written
 * @max_buffer_length: Max buffer length.
 * @buf: Sysfs buffer.
 */
struct sysfs_stats_config {
	/* lock held to read stats */
	qdf_spinlock_t rw_stats_lock;
	qdf_mutex_t sysfs_read_lock;
	qdf_spinlock_t sysfs_write_user_buffer;
	qdf_event_t sysfs_txrx_fw_request_done;
	uint32_t stat_type_requested;
	uint32_t mac_id;
	enum sysfs_printing_mode printing_mode;
	int process_id;
	uint16_t curr_buffer_length;
	uint16_t max_buffer_length;
	char *buf;
};
#endif

/* SOC level structure for data path */
struct dp_soc {
	/**
	 * re-use memory section starts
	 */

	/* Common base structure - Should be the first member */
	struct cdp_soc_t cdp_soc;

	/* SoC Obj */
	struct cdp_ctrl_objmgr_psoc *ctrl_psoc;

	/* OS device abstraction */
	qdf_device_t osdev;

	/*cce disable*/
	bool cce_disable;

	/* WLAN config context */
	struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx;

	/* HTT handle for host-fw interaction */
	struct htt_soc *htt_handle;

	/* Commint init done */
	qdf_atomic_t cmn_init_done;

	/* Opaque hif handle */
	struct hif_opaque_softc *hif_handle;

	/* PDEVs on this SOC */
	struct dp_pdev *pdev_list[MAX_PDEV_CNT];

	/* Ring used to replenish rx buffers (maybe to the firmware of MAC) */
	struct dp_srng rx_refill_buf_ring[MAX_PDEV_CNT];

	struct dp_srng rxdma_mon_desc_ring[MAX_NUM_LMAC_HW];

	/* RXDMA error destination ring */
	struct dp_srng rxdma_err_dst_ring[MAX_NUM_LMAC_HW];

	/* RXDMA monitor buffer replenish ring */
	struct dp_srng rxdma_mon_buf_ring[MAX_NUM_LMAC_HW];

	/* RXDMA monitor destination ring */
	struct dp_srng rxdma_mon_dst_ring[MAX_NUM_LMAC_HW];

	/* RXDMA monitor status ring. TBD: Check format of this ring */
	struct dp_srng rxdma_mon_status_ring[MAX_NUM_LMAC_HW];

	/* Number of PDEVs */
	uint8_t pdev_count;

	/*ast override support in HW*/
	bool ast_override_support;

	/*number of hw dscp tid map*/
	uint8_t num_hw_dscp_tid_map;

	/* HAL SOC handle */
	hal_soc_handle_t hal_soc;

	/* rx monitor pkt tlv size */
	uint16_t rx_mon_pkt_tlv_size;
	/* rx pkt tlv size */
	uint16_t rx_pkt_tlv_size;
	/* rx pkt tlv size in current operation mode */
	uint16_t curr_rx_pkt_tlv_size;

	struct dp_arch_ops arch_ops;

	/* Device ID coming from Bus sub-system */
	uint32_t device_id;

	/* Link descriptor pages */
	struct qdf_mem_multi_page_t link_desc_pages;

	/* total link descriptors for regular RX and TX */
	uint32_t total_link_descs;

	/* Link descriptor Idle list for HW internal use (SRNG mode) */
	struct dp_srng wbm_idle_link_ring;

	/* Link descriptor Idle list for HW internal use (scatter buffer mode)
	 */
	qdf_dma_addr_t wbm_idle_scatter_buf_base_paddr[MAX_IDLE_SCATTER_BUFS];
	void *wbm_idle_scatter_buf_base_vaddr[MAX_IDLE_SCATTER_BUFS];
	uint32_t num_scatter_bufs;

	/* Tx SW descriptor pool */
	struct dp_tx_desc_pool_s tx_desc[MAX_TXDESC_POOLS];

	/* Tx MSDU Extension descriptor pool */
	struct dp_tx_ext_desc_pool_s tx_ext_desc[MAX_TXDESC_POOLS];

	/* Tx TSO descriptor pool */
	struct dp_tx_tso_seg_pool_s tx_tso_desc[MAX_TXDESC_POOLS];

	/* Tx TSO Num of segments pool */
	struct dp_tx_tso_num_seg_pool_s tx_tso_num_seg[MAX_TXDESC_POOLS];

	/* REO destination rings */
	struct dp_srng reo_dest_ring[MAX_REO_DEST_RINGS];

	/* REO exception ring - See if should combine this with reo_dest_ring */
	struct dp_srng reo_exception_ring;

	/* REO reinjection ring */
	struct dp_srng reo_reinject_ring;

	/* REO command ring */
	struct dp_srng reo_cmd_ring;

	/* REO command status ring */
	struct dp_srng reo_status_ring;

	/* WBM Rx release ring */
	struct dp_srng rx_rel_ring;

	/* TCL data ring */
	struct dp_srng tcl_data_ring[MAX_TCL_DATA_RINGS];

	/* Number of Tx comp rings */
	uint8_t num_tx_comp_rings;

	/* Number of TCL data rings */
	uint8_t num_tcl_data_rings;

	/* TCL CMD_CREDIT ring */
	bool init_tcl_cmd_cred_ring;

	/* It is used as credit based ring on QCN9000 else command ring */
	struct dp_srng tcl_cmd_credit_ring;

	/* TCL command status ring */
	struct dp_srng tcl_status_ring;

	/* WBM Tx completion rings */
	struct dp_srng tx_comp_ring[MAX_TCL_DATA_RINGS];

	/* Common WBM link descriptor release ring (SW to WBM) */
	struct dp_srng wbm_desc_rel_ring;

	/* DP Interrupts */
	struct dp_intr intr_ctx[WLAN_CFG_INT_NUM_CONTEXTS];

	/* Monitor mode mac id to dp_intr_id map */
	int mon_intr_id_lmac_map[MAX_NUM_LMAC_HW];
	/* Rx SW descriptor pool for RXDMA monitor buffer */
	struct rx_desc_pool rx_desc_mon[MAX_RXDESC_POOLS];

	/* Rx SW descriptor pool for RXDMA status buffer */
	struct rx_desc_pool rx_desc_status[MAX_RXDESC_POOLS];

	/* Rx SW descriptor pool for RXDMA buffer */
	struct rx_desc_pool rx_desc_buf[MAX_RXDESC_POOLS];

	/* Number of REO destination rings */
	uint8_t num_reo_dest_rings;

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
	/* lock to control access to soc TX descriptors */
	qdf_spinlock_t flow_pool_array_lock;

	/* pause callback to pause TX queues as per flow control */
	tx_pause_callback pause_cb;

	/* flow pool related statistics */
	struct dp_txrx_pool_stats pool_stats;
#endif /* !QCA_LL_TX_FLOW_CONTROL_V2 */

	notify_pre_reset_fw_callback notify_fw_callback;

	unsigned long service_rings_running;

	uint32_t wbm_idle_scatter_buf_size;

	/* VDEVs on this SOC */
	struct dp_vdev *vdev_id_map[MAX_VDEV_CNT];

	/* Tx H/W queues lock */
	qdf_spinlock_t tx_queue_lock[MAX_TX_HW_QUEUES];

	/* Tx ring map for interrupt processing */
	uint8_t tx_ring_map[WLAN_CFG_INT_NUM_CONTEXTS];

	/* Rx ring map for interrupt processing */
	uint8_t rx_ring_map[WLAN_CFG_INT_NUM_CONTEXTS];

	/* peer ID to peer object map (array of pointers to peer objects) */
	struct dp_peer **peer_id_to_obj_map;

	struct {
		unsigned mask;
		unsigned idx_bits;
		TAILQ_HEAD(, dp_peer) * bins;
	} peer_hash;

	/* rx defrag state – TBD: do we need this per radio? */
	struct {
		struct {
			TAILQ_HEAD(, dp_rx_tid_defrag) waitlist;
			uint32_t timeout_ms;
			uint32_t next_flush_ms;
			qdf_spinlock_t defrag_lock;
		} defrag;
		struct {
			int defrag_timeout_check;
			int dup_check;
		} flags;
		TAILQ_HEAD(, dp_reo_cmd_info) reo_cmd_list;
		qdf_spinlock_t reo_cmd_lock;
	} rx;

	/* optional rx processing function */
	void (*rx_opt_proc)(
		struct dp_vdev *vdev,
		struct dp_peer *peer,
		unsigned tid,
		qdf_nbuf_t msdu_list);

	/* pool addr for mcast enhance buff */
	struct {
		int size;
		uint32_t paddr;
		uint32_t *vaddr;
		struct dp_tx_me_buf_t *freelist;
		int buf_in_use;
		qdf_dma_mem_context(memctx);
	} me_buf;

	/* Protect peer hash table */
	DP_MUTEX_TYPE peer_hash_lock;
	/* Protect peer_id_to_objmap */
	DP_MUTEX_TYPE peer_map_lock;

	/* maximum number of suppoerted peers */
	uint32_t max_peers;
	/* maximum value for peer_id */
	uint32_t max_peer_id;

#ifdef DP_USE_REDUCED_PEER_ID_FIELD_WIDTH
	uint32_t peer_id_shift;
	uint32_t peer_id_mask;
#endif

	/* SoC level data path statistics */
	struct dp_soc_stats stats;
#ifdef WLAN_SYSFS_DP_STATS
	/* sysfs config for DP stats */
	struct sysfs_stats_config *sysfs_config;
#endif
	/* timestamp to keep track of msdu buffers received on reo err ring */
	uint64_t rx_route_err_start_pkt_ts;

	/* Num RX Route err in a given window to keep track of rate of errors */
	uint32_t rx_route_err_in_window;

	/* Enable processing of Tx completion status words */
	bool process_tx_status;
	bool process_rx_status;
	struct dp_ast_entry **ast_table;
	struct {
		unsigned mask;
		unsigned idx_bits;
		TAILQ_HEAD(, dp_ast_entry) * bins;
	} ast_hash;

#ifdef DP_TX_HW_DESC_HISTORY
	struct dp_tx_hw_desc_history tx_hw_desc_history;
#endif

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
	struct dp_rx_history *rx_ring_history[MAX_REO_DEST_RINGS];
	struct dp_rx_refill_history *rx_refill_ring_history[MAX_PDEV_CNT];
	struct dp_rx_err_history *rx_err_ring_history;
	struct dp_rx_reinject_history *rx_reinject_ring_history;
#endif

#ifdef WLAN_FEATURE_DP_MON_STATUS_RING_HISTORY
	struct dp_mon_status_ring_history *mon_status_ring_history;
#endif

#ifdef WLAN_FEATURE_DP_TX_DESC_HISTORY
	struct dp_tx_tcl_history tx_tcl_history;
	struct dp_tx_comp_history tx_comp_history;
#endif

#ifdef WLAN_FEATURE_DP_CFG_EVENT_HISTORY
	struct dp_cfg_event_history cfg_event_history;
#endif

	qdf_spinlock_t ast_lock;
	/*Timer for AST entry ageout maintenance */
	qdf_timer_t ast_aging_timer;

	/*Timer counter for WDS AST entry ageout*/
	uint8_t wds_ast_aging_timer_cnt;
	bool pending_ageout;
	bool ast_offload_support;
	bool host_ast_db_enable;
	uint32_t max_ast_ageout_count;
	uint8_t eapol_over_control_port;

	uint8_t sta_mode_search_policy;
	qdf_timer_t lmac_reap_timer;
	uint8_t lmac_timer_init;
	qdf_timer_t int_timer;
	uint8_t intr_mode;
	uint8_t lmac_polled_mode;

	qdf_list_t reo_desc_freelist;
	qdf_spinlock_t reo_desc_freelist_lock;

	/* htt stats */
	struct htt_t2h_stats htt_stats;

	void *external_txrx_handle; /* External data path handle */
#ifdef IPA_OFFLOAD
	struct ipa_dp_tx_rsc ipa_uc_tx_rsc;
#ifdef IPA_WDI3_TX_TWO_PIPES
	/* Resources for the alternative IPA TX pipe */
	struct ipa_dp_tx_rsc ipa_uc_tx_rsc_alt;
#endif

	struct ipa_dp_rx_rsc ipa_uc_rx_rsc;
#ifdef IPA_WDI3_VLAN_SUPPORT
	struct ipa_dp_rx_rsc ipa_uc_rx_rsc_alt;
#endif
	qdf_atomic_t ipa_pipes_enabled;
	bool ipa_first_tx_db_access;
	qdf_spinlock_t ipa_rx_buf_map_lock;
	bool ipa_rx_buf_map_lock_initialized;
	uint8_t ipa_reo_ctx_lock_required[MAX_REO_DEST_RINGS];
#endif

#ifdef WLAN_FEATURE_STATS_EXT
	struct {
		uint32_t rx_mpdu_received;
		uint32_t rx_mpdu_missed;
	} ext_stats;
	qdf_event_t rx_hw_stats_event;
	qdf_spinlock_t rx_hw_stats_lock;
	bool is_last_stats_ctx_init;
#endif /* WLAN_FEATURE_STATS_EXT */

	/* Indicates HTT map/unmap versions*/
	uint8_t peer_map_unmap_versions;
	/* Per peer per Tid ba window size support */
	uint8_t per_tid_basize_max_tid;
	/* Soc level flag to enable da_war */
	uint8_t da_war_enabled;
	/* number of active ast entries */
	uint32_t num_ast_entries;
	/* peer extended rate statistics context at soc level*/
	struct cdp_soc_rate_stats_ctx *rate_stats_ctx;
	/* peer extended rate statistics control flag */
	bool peerstats_enabled;

	/* 8021p PCP-TID map values */
	uint8_t pcp_tid_map[PCP_TID_MAP_MAX];
	/* TID map priority value */
	uint8_t tidmap_prty;
	/* Pointer to global per ring type specific configuration table */
	struct wlan_srng_cfg *wlan_srng_cfg;
	/* Num Tx outstanding on device */
	qdf_atomic_t num_tx_outstanding;
	/* Num Tx exception on device */
	qdf_atomic_t num_tx_exception;
	/* Num Tx allowed */
	uint32_t num_tx_allowed;
	/* Num Regular Tx allowed */
	uint32_t num_reg_tx_allowed;
	/* Num Tx allowed for special frames*/
	uint32_t num_tx_spl_allowed;
	/* Preferred HW mode */
	uint8_t preferred_hw_mode;

	/**
	 * Flag to indicate whether WAR to address single cache entry
	 * invalidation bug is enabled or not
	 */
	bool is_rx_fse_full_cache_invalidate_war_enabled;
#if defined(WLAN_SUPPORT_RX_FLOW_TAG) || defined(WLAN_SUPPORT_RX_FISA)
	/**
	 * Pointer to DP RX Flow FST at SOC level if
	 * is_rx_flow_search_table_per_pdev is false
	 * TBD: rx_fst[num_macs] if we decide to have per mac FST
	 */
	struct dp_rx_fst *rx_fst;
#ifdef WLAN_SUPPORT_RX_FISA
	uint8_t fisa_enable;
	uint8_t fisa_lru_del_enable;
	/**
	 * Params used for controlling the fisa aggregation dynamically
	 */
	struct {
		qdf_atomic_t skip_fisa;
		uint8_t fisa_force_flush[MAX_REO_DEST_RINGS];
	} skip_fisa_param;

	/**
	 * CMEM address and size for FST in CMEM, This is the address
	 * shared during init time.
	 */
	uint64_t fst_cmem_base;
	uint64_t fst_cmem_size;
#endif
#endif /* WLAN_SUPPORT_RX_FLOW_TAG || WLAN_SUPPORT_RX_FISA */
	/* SG supported for msdu continued packets from wbm release ring */
	bool wbm_release_desc_rx_sg_support;
	bool peer_map_attach_success;
	/* Flag to disable mac1 ring interrupts */
	bool disable_mac1_intr;
	/* Flag to disable mac2 ring interrupts */
	bool disable_mac2_intr;

	struct {
		/* 1st msdu in sg for msdu continued packets in wbm rel ring */
		bool wbm_is_first_msdu_in_sg;
		/* Wbm sg list head */
		qdf_nbuf_t wbm_sg_nbuf_head;
		/* Wbm sg list tail */
		qdf_nbuf_t wbm_sg_nbuf_tail;
		uint32_t wbm_sg_desc_msdu_len;
	} wbm_sg_param;
	/* Number of msdu exception descriptors */
	uint32_t num_msdu_exception_desc;

	/* RX buffer params */
	struct rx_buff_pool rx_buff_pool[MAX_PDEV_CNT];
	struct rx_refill_buff_pool rx_refill_buff_pool;
	/* Save recent operation related variable */
	struct dp_last_op_info last_op_info;
	TAILQ_HEAD(, dp_peer) inactive_peer_list;
	qdf_spinlock_t inactive_peer_list_lock;
	TAILQ_HEAD(, dp_vdev) inactive_vdev_list;
	qdf_spinlock_t inactive_vdev_list_lock;
	/* lock to protect vdev_id_map table*/
	qdf_spinlock_t vdev_map_lock;

	/* Flow Search Table is in CMEM */
	bool fst_in_cmem;

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
	struct dp_swlm swlm;
#endif

#ifdef FEATURE_RUNTIME_PM
	/* DP Rx timestamp */
	qdf_time_t rx_last_busy;
	/* Dp runtime refcount */
	qdf_atomic_t dp_runtime_refcount;
	/* Dp tx pending count in RTPM */
	qdf_atomic_t tx_pending_rtpm;
#endif
	/* Invalid buffer that allocated for RX buffer */
	qdf_nbuf_queue_t invalid_buf_queue;

#ifdef FEATURE_MEC
	/** @mec_lock: spinlock for MEC table */
	qdf_spinlock_t mec_lock;
	/** @mec_cnt: number of active mec entries */
	qdf_atomic_t mec_cnt;
	struct {
		/** @mask: mask bits */
		uint32_t mask;
		/** @idx_bits: index to shift bits */
		uint32_t idx_bits;
		/** @bins: MEC table */
		TAILQ_HEAD(, dp_mec_entry) * bins;
	} mec_hash;
#endif

#ifdef WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
	qdf_list_t reo_desc_deferred_freelist;
	qdf_spinlock_t reo_desc_deferred_freelist_lock;
	bool reo_desc_deferred_freelist_init;
#endif
	/* BM id for first WBM2SW  ring */
	uint32_t wbm_sw0_bm_id;

	/* Store arch_id from device_id */
	uint16_t arch_id;

	/* link desc ID start per device type */
	uint32_t link_desc_id_start;

	/* CMEM buffer target reserved for host usage */
	uint64_t cmem_base;
	/* CMEM size in bytes */
	uint64_t cmem_total_size;
	/* CMEM free size in bytes */
	uint64_t cmem_avail_size;

	/* SOC level feature flags */
	struct dp_soc_features features;

#ifdef WIFI_MONITOR_SUPPORT
	struct dp_mon_soc *monitor_soc;
#endif
	uint8_t rxdma2sw_rings_not_supported:1,
		wbm_sg_last_msdu_war:1,
		mec_fw_offload:1,
		multi_peer_grp_cmd_supported:1;

	/* Number of Rx refill rings */
	uint8_t num_rx_refill_buf_rings;
#ifdef FEATURE_RUNTIME_PM
	/* flag to indicate vote for runtime_pm for high tput castt*/
	qdf_atomic_t rtpm_high_tput_flag;
#endif
	/* Buffer manager ID for idle link descs */
	uint8_t idle_link_bm_id;
	qdf_atomic_t ref_count;

	unsigned long vdev_stats_id_map;
	bool txmon_hw_support;

#ifdef DP_UMAC_HW_RESET_SUPPORT
	struct dp_soc_umac_reset_ctx umac_reset_ctx;
#endif
	/* PPDU to link_id mapping parameters */
	uint8_t link_id_offset;
	uint8_t link_id_bits;
#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
	/* A flag using to decide the switch of rx link speed  */
	bool high_throughput;
#endif
	bool is_tx_pause;

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
	/* number of IPv4 flows inserted */
	qdf_atomic_t ipv4_fse_cnt;
	/* number of IPv6 flows inserted */
	qdf_atomic_t ipv6_fse_cnt;
#endif
	/* Reo queue ref table items */
	struct reo_queue_ref_table reo_qref;
};

#ifdef IPA_OFFLOAD
/**
 * struct dp_ipa_resources - Resources needed for IPA
 * @tx_ring:
 * @tx_num_alloc_buffer:
 * @tx_comp_ring:
 * @rx_rdy_ring:
 * @rx_refill_ring:
 * @tx_comp_doorbell_paddr: IPA UC doorbell registers paddr
 * @tx_comp_doorbell_vaddr:
 * @rx_ready_doorbell_paddr:
 * @is_db_ddr_mapped:
 * @tx_alt_ring:
 * @tx_alt_ring_num_alloc_buffer:
 * @tx_alt_comp_ring:
 * @tx_alt_comp_doorbell_paddr: IPA UC doorbell registers paddr
 * @tx_alt_comp_doorbell_vaddr:
 * @rx_alt_rdy_ring:
 * @rx_alt_refill_ring:
 * @rx_alt_ready_doorbell_paddr:
 */
struct dp_ipa_resources {
	qdf_shared_mem_t tx_ring;
	uint32_t tx_num_alloc_buffer;

	qdf_shared_mem_t tx_comp_ring;
	qdf_shared_mem_t rx_rdy_ring;
	qdf_shared_mem_t rx_refill_ring;

	/* IPA UC doorbell registers paddr */
	qdf_dma_addr_t tx_comp_doorbell_paddr;
	uint32_t *tx_comp_doorbell_vaddr;
	qdf_dma_addr_t rx_ready_doorbell_paddr;

	bool is_db_ddr_mapped;

#ifdef IPA_WDI3_TX_TWO_PIPES
	qdf_shared_mem_t tx_alt_ring;
	uint32_t tx_alt_ring_num_alloc_buffer;
	qdf_shared_mem_t tx_alt_comp_ring;

	/* IPA UC doorbell registers paddr */
	qdf_dma_addr_t tx_alt_comp_doorbell_paddr;
	uint32_t *tx_alt_comp_doorbell_vaddr;
#endif
#ifdef IPA_WDI3_VLAN_SUPPORT
	qdf_shared_mem_t rx_alt_rdy_ring;
	qdf_shared_mem_t rx_alt_refill_ring;
	qdf_dma_addr_t rx_alt_ready_doorbell_paddr;
#endif
};
#endif

#define MAX_RX_MAC_RINGS 2
/* Same as NAC_MAX_CLENT */
#define DP_NAC_MAX_CLIENT  24

/*
 * 24 bits cookie size
 * 10 bits page id 0 ~ 1023 for MCL
 * 3 bits page id 0 ~ 7 for WIN
 * WBM Idle List Desc size = 128,
 * Num descs per page = 4096/128 = 32 for MCL
 * Num descs per page = 2MB/128 = 16384 for WIN
 */
/*
 * Macros to setup link descriptor cookies - for link descriptors, we just
 * need first 3 bits to store bank/page ID for WIN. The
 * remaining bytes will be used to set a unique ID, which will
 * be useful in debugging
 */
#ifdef MAX_ALLOC_PAGE_SIZE
#if PAGE_SIZE == 4096
#define LINK_DESC_PAGE_ID_MASK  0x007FE0
#define LINK_DESC_ID_SHIFT      5
#define LINK_DESC_ID_START_21_BITS_COOKIE 0x8000
#elif PAGE_SIZE == 65536
#define LINK_DESC_PAGE_ID_MASK  0x007E00
#define LINK_DESC_ID_SHIFT      9
#define LINK_DESC_ID_START_21_BITS_COOKIE 0x800
#else
#error "Unsupported kernel PAGE_SIZE"
#endif
#define LINK_DESC_COOKIE(_desc_id, _page_id, _desc_id_start) \
	((((_page_id) + (_desc_id_start)) << LINK_DESC_ID_SHIFT) | (_desc_id))
#define LINK_DESC_COOKIE_PAGE_ID(_cookie) \
	(((_cookie) & LINK_DESC_PAGE_ID_MASK) >> LINK_DESC_ID_SHIFT)
#else
#define LINK_DESC_PAGE_ID_MASK  0x7
#define LINK_DESC_ID_SHIFT      3
#define LINK_DESC_COOKIE(_desc_id, _page_id, _desc_id_start) \
	((((_desc_id) + (_desc_id_start)) << LINK_DESC_ID_SHIFT) | (_page_id))
#define LINK_DESC_COOKIE_PAGE_ID(_cookie) \
	((_cookie) & LINK_DESC_PAGE_ID_MASK)
#define LINK_DESC_ID_START_21_BITS_COOKIE 0x8000
#endif
#define LINK_DESC_ID_START_20_BITS_COOKIE 0x4000

/* same as ieee80211_nac_param */
enum dp_nac_param_cmd {
	/* IEEE80211_NAC_PARAM_ADD */
	DP_NAC_PARAM_ADD = 1,
	/* IEEE80211_NAC_PARAM_DEL */
	DP_NAC_PARAM_DEL,
	/* IEEE80211_NAC_PARAM_LIST */
	DP_NAC_PARAM_LIST,
};

/**
 * struct dp_neighbour_peer - neighbour peer list type for smart mesh
 * @neighbour_peers_macaddr: neighbour peer's mac address
 * @vdev: associated vdev
 * @ast_entry: ast_entry for neighbour peer
 * @rssi: rssi value
 * @neighbour_peer_list_elem: neighbour peer list TAILQ element
 */
struct dp_neighbour_peer {
	union dp_align_mac_addr neighbour_peers_macaddr;
	struct dp_vdev *vdev;
	struct dp_ast_entry *ast_entry;
	uint8_t rssi;
	TAILQ_ENTRY(dp_neighbour_peer) neighbour_peer_list_elem;
};

#ifdef WLAN_TX_PKT_CAPTURE_ENH
#define WLAN_TX_PKT_CAPTURE_ENH 1
#define DP_TX_PPDU_PROC_THRESHOLD 8
#define DP_TX_PPDU_PROC_TIMEOUT 10
#endif

/**
 * struct ppdu_info - PPDU Status info descriptor
 * @ppdu_id: Unique ppduid assigned by firmware for every tx packet
 * @sched_cmdid: schedule command id, which will be same in a burst
 * @max_ppdu_id: wrap around for ppdu id
 * @tsf_l32:
 * @tlv_bitmap:
 * @last_tlv_cnt: Keep track for missing ppdu tlvs
 * @last_user: last ppdu processed for user
 * @is_ampdu: set if Ampdu aggregate
 * @nbuf: ppdu descriptor payload
 * @ppdu_desc: ppdu descriptor
 * @ulist: Union of lists
 * @ppdu_info_dlist_elem: linked list of ppdu tlvs
 * @ppdu_info_slist_elem: Singly linked list (queue) of ppdu tlvs
 * @ppdu_info_list_elem: linked list of ppdu tlvs
 * @ppdu_info_queue_elem: Singly linked list (queue) of ppdu tlvs
 * @compltn_common_tlv: Successful tlv counter from COMPLTN COMMON tlv
 * @ack_ba_tlv: Successful tlv counter from ACK BA tlv
 * @done:
 */
struct ppdu_info {
	uint32_t ppdu_id;
	uint32_t sched_cmdid;
	uint32_t max_ppdu_id;
	uint32_t tsf_l32;
	uint16_t tlv_bitmap;
	uint16_t last_tlv_cnt;
	uint16_t last_user:8,
		 is_ampdu:1;
	qdf_nbuf_t nbuf;
	struct cdp_tx_completion_ppdu *ppdu_desc;
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	union {
		TAILQ_ENTRY(ppdu_info) ppdu_info_dlist_elem;
		STAILQ_ENTRY(ppdu_info) ppdu_info_slist_elem;
	} ulist;
#define ppdu_info_list_elem ulist.ppdu_info_dlist_elem
#define ppdu_info_queue_elem ulist.ppdu_info_slist_elem
#else
	TAILQ_ENTRY(ppdu_info) ppdu_info_list_elem;
#endif
	uint8_t compltn_common_tlv;
	uint8_t ack_ba_tlv;
	bool done;
};

/**
 * struct msdu_completion_info - wbm msdu completion info
 * @ppdu_id: Unique ppduid assigned by firmware for every tx packet
 * @peer_id: peer_id
 * @tid: tid which used during transmit
 * @first_msdu: first msdu indication
 * @last_msdu: last msdu indication
 * @msdu_part_of_amsdu: msdu part of amsdu
 * @transmit_cnt: retried count
 * @status: transmit status
 * @tsf: timestamp which it transmitted
 */
struct msdu_completion_info {
	uint32_t ppdu_id;
	uint16_t peer_id;
	uint8_t tid;
	uint8_t first_msdu:1,
		last_msdu:1,
		msdu_part_of_amsdu:1;
	uint8_t transmit_cnt;
	uint8_t status;
	uint32_t tsf;
};

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
struct rx_protocol_tag_map {
	/* This is the user configured tag for the said protocol type */
	uint16_t tag;
};

#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
/**
 * struct rx_protocol_tag_stats - protocol statistics
 * @tag_ctr: number of rx msdus matching this tag
 */
struct rx_protocol_tag_stats {
	uint32_t tag_ctr;
};
#endif /* WLAN_SUPPORT_RX_TAG_STATISTICS */

#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

#ifdef WLAN_RX_PKT_CAPTURE_ENH
/* Template data to be set for Enhanced RX Monitor packets */
#define RX_MON_CAP_ENH_TRAILER 0xdeadc0dedeadda7a

/**
 * struct dp_rx_mon_enh_trailer_data - Data structure to set a known pattern
 * at end of each MSDU in monitor-lite mode
 * @reserved1: reserved for future use
 * @reserved2: reserved for future use
 * @flow_tag: flow tag value read from skb->cb
 * @protocol_tag: protocol tag value read from skb->cb
 */
struct dp_rx_mon_enh_trailer_data {
	uint16_t reserved1;
	uint16_t reserved2;
	uint16_t flow_tag;
	uint16_t protocol_tag;
};
#endif /* WLAN_RX_PKT_CAPTURE_ENH */

#ifdef HTT_STATS_DEBUGFS_SUPPORT
/* Number of debugfs entries created for HTT stats */
#define PDEV_HTT_STATS_DBGFS_SIZE HTT_DBG_NUM_EXT_STATS

/**
 * struct pdev_htt_stats_dbgfs_priv - Structure to maintain debugfs information
 * of HTT stats
 * @pdev: dp pdev of debugfs entry
 * @stats_id: stats id of debugfs entry
 */
struct pdev_htt_stats_dbgfs_priv {
	struct dp_pdev *pdev;
	uint16_t stats_id;
};

/**
 * struct pdev_htt_stats_dbgfs_cfg - PDEV level data structure for debugfs
 * support for HTT stats
 * @debugfs_entry: qdf_debugfs directory entry
 * @m: qdf debugfs file handler
 * @pdev_htt_stats_dbgfs_ops: File operations of entry created
 * @priv: HTT stats debugfs private object
 * @htt_stats_dbgfs_event: HTT stats event for debugfs support
 * @lock: HTT stats debugfs lock
 * @htt_stats_dbgfs_msg_process: Function callback to print HTT stats
 */
struct pdev_htt_stats_dbgfs_cfg {
	qdf_dentry_t debugfs_entry[PDEV_HTT_STATS_DBGFS_SIZE];
	qdf_debugfs_file_t m;
	struct qdf_debugfs_fops
			pdev_htt_stats_dbgfs_ops[PDEV_HTT_STATS_DBGFS_SIZE - 1];
	struct pdev_htt_stats_dbgfs_priv priv[PDEV_HTT_STATS_DBGFS_SIZE - 1];
	qdf_event_t htt_stats_dbgfs_event;
	qdf_mutex_t lock;
	void (*htt_stats_dbgfs_msg_process)(void *data, A_INT32 len);
};
#endif /* HTT_STATS_DEBUGFS_SUPPORT */

struct dp_srng_ring_state {
	enum hal_ring_type ring_type;
	uint32_t sw_head;
	uint32_t sw_tail;
	uint32_t hw_head;
	uint32_t hw_tail;

};

struct dp_soc_srngs_state {
	uint32_t seq_num;
	uint32_t max_ring_id;
	struct dp_srng_ring_state ring_state[DP_MAX_SRNGS];
	TAILQ_ENTRY(dp_soc_srngs_state) list_elem;
};

#ifdef WLAN_FEATURE_11BE_MLO
/* struct dp_mlo_sync_timestamp - PDEV level data structure for storing
 * MLO timestamp received via HTT msg.
 * msg_type: This would be set to HTT_T2H_MSG_TYPE_MLO_TIMESTAMP_OFFSET_IND
 * pdev_id: pdev_id
 * chip_id: chip_id
 * mac_clk_freq: mac clock frequency of the mac HW block in MHz
 * sync_tstmp_lo_us: lower 32 bits of the WLAN global time stamp (in us) at
 *                   which last sync interrupt was received
 * sync_tstmp_hi_us: upper 32 bits of the WLAN global time stamp (in us) at
 *                   which last sync interrupt was received
 * mlo_offset_lo_us: lower 32 bits of the MLO time stamp offset in us
 * mlo_offset_hi_us: upper 32 bits of the MLO time stamp offset in us
 * mlo_offset_clks:  MLO time stamp offset in clock ticks for sub us
 * mlo_comp_us:      MLO time stamp compensation applied in us
 * mlo_comp_clks:    MLO time stamp compensation applied in clock ticks
 *                   for sub us resolution
 * mlo_comp_timer:   period of MLO compensation timer at which compensation
 *                   is applied, in us
 */
struct dp_mlo_sync_timestamp {
	uint32_t msg_type:8,
		 pdev_id:2,
		 chip_id:2,
		 rsvd1:4,
		 mac_clk_freq:16;
	uint32_t sync_tstmp_lo_us;
	uint32_t sync_tstmp_hi_us;
	uint32_t mlo_offset_lo_us;
	uint32_t mlo_offset_hi_us;
	uint32_t mlo_offset_clks;
	uint32_t mlo_comp_us:16,
		 mlo_comp_clks:10,
		 rsvd2:6;
	uint32_t mlo_comp_timer:22,
		 rsvd3:10;
};
#endif

/* PDEV level structure for data path */
struct dp_pdev {
	/**
	 * Re-use Memory Section Starts
	 */

	/* PDEV Id */
	uint8_t pdev_id;

	/* LMAC Id */
	uint8_t lmac_id;

	/* Target pdev  Id */
	uint8_t target_pdev_id;

	bool pdev_deinit;

	/* TXRX SOC handle */
	struct dp_soc *soc;

	/* pdev status down or up required to handle dynamic hw
	 * mode switch between DBS and DBS_SBS.
	 * 1 = down
	 * 0 = up
	 */
	bool is_pdev_down;

	/* Enhanced Stats is enabled */
	bool enhanced_stats_en;

	/* Flag to indicate fast RX */
	bool rx_fast_flag;

	/* Second ring used to replenish rx buffers */
	struct dp_srng rx_refill_buf_ring2;
#ifdef IPA_WDI3_VLAN_SUPPORT
	/* Third ring used to replenish rx buffers */
	struct dp_srng rx_refill_buf_ring3;
#endif

#ifdef FEATURE_DIRECT_LINK
	/* Fourth ring used to replenish rx buffers */
	struct dp_srng rx_refill_buf_ring4;
#endif

	/* Empty ring used by firmware to post rx buffers to the MAC */
	struct dp_srng rx_mac_buf_ring[MAX_RX_MAC_RINGS];

	int ch_band_lmac_id_mapping[REG_BAND_UNKNOWN];

	/* wlan_cfg pdev ctxt*/
	 struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_ctx;

	/**
	 * TODO: See if we need a ring map here for LMAC rings.
	 * 1. Monitor rings are currently planning to be processed on receiving
	 * PPDU end interrupts and hence won't need ring based interrupts.
	 * 2. Rx buffer rings will be replenished during REO destination
	 * processing and doesn't require regular interrupt handling - we will
	 * only handle low water mark interrupts which is not expected
	 * frequently
	 */

	/* VDEV list */
	TAILQ_HEAD(, dp_vdev) vdev_list;

	/* vdev list lock */
	qdf_spinlock_t vdev_list_lock;

	/* Number of vdevs this device have */
	uint16_t vdev_count;

	/* PDEV transmit lock */
	qdf_spinlock_t tx_lock;

	/*tx_mutex for me*/
	DP_MUTEX_TYPE tx_mutex;

	/* msdu chain head & tail */
	qdf_nbuf_t invalid_peer_head_msdu;
	qdf_nbuf_t invalid_peer_tail_msdu;

	/* Band steering  */
	/* TBD */

	/* PDEV level data path statistics */
	struct cdp_pdev_stats stats;

	/* Global RX decap mode for the device */
	enum htt_pkt_type rx_decap_mode;

	qdf_atomic_t num_tx_outstanding;
	int32_t tx_descs_max;

	qdf_atomic_t num_tx_exception;

	/* MCL specific local peer handle */
	struct {
		uint8_t pool[OL_TXRX_NUM_LOCAL_PEER_IDS + 1];
		uint8_t freelist;
		qdf_spinlock_t lock;
		struct dp_peer *map[OL_TXRX_NUM_LOCAL_PEER_IDS];
	} local_peer_ids;

	/* dscp_tid_map_*/
	uint8_t dscp_tid_map[DP_MAX_TID_MAPS][DSCP_TID_MAP_MAX];

	/* operating channel */
	struct {
		uint8_t num;
		uint8_t band;
		uint16_t freq;
	} operating_channel;

	/* pool addr for mcast enhance buff */
	struct {
		int size;
		uint32_t paddr;
		char *vaddr;
		struct dp_tx_me_buf_t *freelist;
		int buf_in_use;
		qdf_dma_mem_context(memctx);
	} me_buf;

	bool hmmc_tid_override_en;
	uint8_t hmmc_tid;

	/* Number of VAPs with mcast enhancement enabled */
	qdf_atomic_t mc_num_vap_attached;

	qdf_atomic_t stats_cmd_complete;

#ifdef IPA_OFFLOAD
	ipa_uc_op_cb_type ipa_uc_op_cb;
	void *usr_ctxt;
	struct dp_ipa_resources ipa_resource;
#endif

	/* TBD */

	/* map this pdev to a particular Reo Destination ring */
	enum cdp_host_reo_dest_ring reo_dest;

	/* WDI event handlers */
	struct wdi_event_subscribe_t **wdi_event_list;

	bool cfr_rcc_mode;

	/* enable time latency check for tx completion */
	bool latency_capture_enable;

	/* enable calculation of delay stats*/
	bool delay_stats_flag;
	void *dp_txrx_handle; /* Advanced data path handle */
	uint32_t ppdu_id;
	bool first_nbuf;
	/* Current noise-floor reading for the pdev channel */
	int16_t chan_noise_floor;

	/*
	 * For multiradio device, this flag indicates if
	 * this radio is primary or secondary.
	 *
	 * For HK 1.0, this is used for WAR for the AST issue.
	 * HK 1.x mandates creation of only 1 AST entry with same MAC address
	 * across 2 radios. is_primary indicates the radio on which DP should
	 * install HW AST entry if there is a request to add 2 AST entries
	 * with same MAC address across 2 radios
	 */
	uint8_t is_primary;
	struct cdp_tx_sojourn_stats sojourn_stats;
	qdf_nbuf_t sojourn_buf;

	union dp_rx_desc_list_elem_t *free_list_head;
	union dp_rx_desc_list_elem_t *free_list_tail;
	/* Cached peer_id from htt_peer_details_tlv */
	uint16_t fw_stats_peer_id;

	/* qdf_event for fw_peer_stats */
	qdf_event_t fw_peer_stats_event;

	/* qdf_event for fw_stats */
	qdf_event_t fw_stats_event;

	/* qdf_event for fw__obss_stats */
	qdf_event_t fw_obss_stats_event;

	/* To check if request is already sent for obss stats */
	bool pending_fw_obss_stats_response;

	/* User configured max number of tx buffers */
	uint32_t num_tx_allowed;

	/*
	 * User configured max num of tx buffers excluding the
	 * number of buffers reserved for handling special frames
	 */
	uint32_t num_reg_tx_allowed;

	/* User configured max number of tx buffers for the special frames*/
	uint32_t num_tx_spl_allowed;

	/* unique cookie required for peer session */
	uint32_t next_peer_cookie;

	/*
	 * Run time enabled when the first protocol tag is added,
	 * run time disabled when the last protocol tag is deleted
	 */
	bool  is_rx_protocol_tagging_enabled;

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
	/*
	 * The protocol type is used as array index to save
	 * user provided tag info
	 */
	struct rx_protocol_tag_map rx_proto_tag_map[RX_PROTOCOL_TAG_MAX];

#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
	/*
	 * Track msdus received from each reo ring separately to avoid
	 * simultaneous writes from different core
	 */
	struct rx_protocol_tag_stats
		reo_proto_tag_stats[MAX_REO_DEST_RINGS][RX_PROTOCOL_TAG_MAX];
	/* Track msdus received from exception ring separately */
	struct rx_protocol_tag_stats
		rx_err_proto_tag_stats[RX_PROTOCOL_TAG_MAX];
	struct rx_protocol_tag_stats
		mon_proto_tag_stats[RX_PROTOCOL_TAG_MAX];
#endif /* WLAN_SUPPORT_RX_TAG_STATISTICS */
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
	/**
	 * Pointer to DP Flow FST at SOC level if
	 * is_rx_flow_search_table_per_pdev is true
	 */
	struct dp_rx_fst *rx_fst;
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */

#ifdef FEATURE_TSO_STATS
	/* TSO Id to index into TSO packet information */
	qdf_atomic_t tso_idx;
#endif /* FEATURE_TSO_STATS */

#ifdef WLAN_SUPPORT_DATA_STALL
	data_stall_detect_cb data_stall_detect_callback;
#endif /* WLAN_SUPPORT_DATA_STALL */

	/* flag to indicate whether LRO hash command has been sent to FW */
	uint8_t is_lro_hash_configured;

#ifdef HTT_STATS_DEBUGFS_SUPPORT
	/* HTT stats debugfs params */
	struct pdev_htt_stats_dbgfs_cfg *dbgfs_cfg;
#endif
	struct {
		qdf_work_t work;
		qdf_workqueue_t *work_queue;
		uint32_t seq_num;
		uint8_t queue_depth;
		qdf_spinlock_t list_lock;

		TAILQ_HEAD(, dp_soc_srngs_state) list;
	} bkp_stats;
#ifdef WIFI_MONITOR_SUPPORT
	struct dp_mon_pdev *monitor_pdev;
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	struct dp_mlo_sync_timestamp timestamp;
#endif
	/* Is isolation mode enabled */
	bool  isolation;
#ifdef WLAN_FEATURE_MARK_FIRST_WAKEUP_PACKET
	uint8_t is_first_wakeup_packet;
#endif
#ifdef CONNECTIVITY_PKTLOG
	/* packetdump callback functions */
	ol_txrx_pktdump_cb dp_tx_packetdump_cb;
	ol_txrx_pktdump_cb dp_rx_packetdump_cb;
#endif

	/* Firmware Stats for TLV received from Firmware */
	uint64_t fw_stats_tlv_bitmap_rcvd;

	/* For Checking Pending Firmware Response */
	bool pending_fw_stats_response;
};

struct dp_peer;

#ifdef DP_RX_UDP_OVER_PEER_ROAM
#define WLAN_ROAM_PEER_AUTH_STATUS_NONE 0x0
/*
 * This macro is equivalent to macro ROAM_AUTH_STATUS_AUTHENTICATED used
 * in connection mgr
 */
#define WLAN_ROAM_PEER_AUTH_STATUS_AUTHENTICATED 0x2
#endif

/* VDEV structure for data path state */
struct dp_vdev {
	/* OS device abstraction */
	qdf_device_t osdev;

	/* physical device that is the parent of this virtual device */
	struct dp_pdev *pdev;

	/* VDEV operating mode */
	enum wlan_op_mode opmode;

	/* VDEV subtype */
	enum wlan_op_subtype subtype;

	/* Tx encapsulation type for this VAP */
	enum htt_cmn_pkt_type tx_encap_type;

	/* Rx Decapsulation type for this VAP */
	enum htt_cmn_pkt_type rx_decap_type;

	/* WDS enabled */
	bool wds_enabled;

	/* MEC enabled */
	bool mec_enabled;

#ifdef QCA_SUPPORT_WDS_EXTENDED
	bool wds_ext_enabled;
	bool drop_tx_mcast;
#endif /* QCA_SUPPORT_WDS_EXTENDED */
	bool drop_3addr_mcast;
#ifdef WLAN_VENDOR_SPECIFIC_BAR_UPDATE
	bool skip_bar_update;
	unsigned long skip_bar_update_last_ts;
#endif
	/* WDS Aging timer period */
	uint32_t wds_aging_timer_val;

	/* NAWDS enabled */
	bool nawds_enabled;

	/* Multicast enhancement enabled */
	uint8_t mcast_enhancement_en;

	/* IGMP multicast enhancement enabled */
	uint8_t igmp_mcast_enhanc_en;

	/* vdev_id - ID used to specify a particular vdev to the target */
	uint8_t vdev_id;

	/* Default HTT meta data for this VDEV */
	/* TBD: check alignment constraints */
	uint16_t htt_tcl_metadata;

	/* vdev lmac_id */
	uint8_t lmac_id;

	/* vdev bank_id */
	uint8_t bank_id;

	/* Mesh mode vdev */
	uint32_t mesh_vdev;

	/* Mesh mode rx filter setting */
	uint32_t mesh_rx_filter;

	/* DSCP-TID mapping table ID */
	uint8_t dscp_tid_map_id;

	/* Address search type to be set in TX descriptor */
	uint8_t search_type;

	/*
	 * Flag to indicate if s/w tid classification should be
	 * skipped
	 */
	uint8_t skip_sw_tid_classification;

	/* Flag to enable peer authorization */
	uint8_t peer_authorize;

	/* AST hash value for BSS peer in HW valid for STA VAP*/
	uint16_t bss_ast_hash;

	/* AST hash index for BSS peer in HW valid for STA VAP*/
	uint16_t bss_ast_idx;

	bool multipass_en;

	/* Address search flags to be configured in HAL descriptor */
	uint8_t hal_desc_addr_search_flags;

	/* Handle to the OS shim SW's virtual device */
	ol_osif_vdev_handle osif_vdev;

	/* MAC address */
	union dp_align_mac_addr mac_addr;

#ifdef WLAN_FEATURE_11BE_MLO
	/* MLO MAC address corresponding to vdev */
	union dp_align_mac_addr mld_mac_addr;
#if defined(WLAN_MLO_MULTI_CHIP) && defined(WLAN_MCAST_MLO)
	bool mlo_vdev;
#endif
#endif

	/* node in the pdev's list of vdevs */
	TAILQ_ENTRY(dp_vdev) vdev_list_elem;

	/* dp_peer list */
	TAILQ_HEAD(, dp_peer) peer_list;
	/* to protect peer_list */
	DP_MUTEX_TYPE peer_list_lock;

	/* RX call back function to flush GRO packets*/
	ol_txrx_rx_gro_flush_ind_fp osif_gro_flush;
	/* default RX call back function called by dp */
	ol_txrx_rx_fp osif_rx;
#ifdef QCA_SUPPORT_EAPOL_OVER_CONTROL_PORT
	/* callback to receive eapol frames */
	ol_txrx_rx_fp osif_rx_eapol;
#endif
	/* callback to deliver rx frames to the OS */
	ol_txrx_rx_fp osif_rx_stack;
	/* Callback to handle rx fisa frames */
	ol_txrx_fisa_rx_fp osif_fisa_rx;
	ol_txrx_fisa_flush_fp osif_fisa_flush;

	/* call back function to flush out queued rx packets*/
	ol_txrx_rx_flush_fp osif_rx_flush;
	ol_txrx_rsim_rx_decap_fp osif_rsim_rx_decap;
	ol_txrx_get_key_fp osif_get_key;
	ol_txrx_tx_free_ext_fp osif_tx_free_ext;

#ifdef notyet
	/* callback to check if the msdu is an WAI (WAPI) frame */
	ol_rx_check_wai_fp osif_check_wai;
#endif

	/* proxy arp function */
	ol_txrx_proxy_arp_fp osif_proxy_arp;

	ol_txrx_mcast_me_fp me_convert;

	/* completion function used by this vdev*/
	ol_txrx_completion_fp tx_comp;

	ol_txrx_get_tsf_time get_tsf_time;

	/* callback to classify critical packets */
	ol_txrx_classify_critical_pkt_fp tx_classify_critical_pkt_cb;

	/* deferred vdev deletion state */
	struct {
		/* VDEV delete pending */
		int pending;
		/*
		* callback and a context argument to provide a
		* notification for when the vdev is deleted.
		*/
		ol_txrx_vdev_delete_cb callback;
		void *context;
	} delete;

	/* tx data delivery notification callback function */
	struct {
		ol_txrx_data_tx_cb func;
		void *ctxt;
	} tx_non_std_data_callback;


	/* safe mode control to bypass the encrypt and decipher process*/
	uint32_t safemode;

	/* rx filter related */
	uint32_t drop_unenc;
#ifdef notyet
	privacy_exemption privacy_filters[MAX_PRIVACY_FILTERS];
	uint32_t filters_num;
#endif
	/* TDLS Link status */
	bool tdls_link_connected;
	bool is_tdls_frame;

	/* per vdev rx nbuf queue */
	qdf_nbuf_queue_t rxq;

	uint8_t tx_ring_id;
	struct dp_tx_desc_pool_s *tx_desc;
	struct dp_tx_ext_desc_pool_s *tx_ext_desc;

	/* Capture timestamp of previous tx packet enqueued */
	uint64_t prev_tx_enq_tstamp;

	/* Capture timestamp of previous rx packet delivered */
	uint64_t prev_rx_deliver_tstamp;

	/* VDEV Stats */
	struct cdp_vdev_stats stats;

	/* Is this a proxySTA VAP */
	uint8_t proxysta_vdev : 1, /* Is this a proxySTA VAP */
		wrap_vdev : 1, /* Is this a QWRAP AP VAP */
		isolation_vdev : 1, /* Is this a QWRAP AP VAP */
		reserved : 5; /* Reserved */

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
	struct dp_tx_desc_pool_s *pool;
#endif
	/* AP BRIDGE enabled */
	bool ap_bridge_enabled;

	enum cdp_sec_type  sec_type;

	/* SWAR for HW: Enable WEP bit in the AMSDU frames for RAW mode */
	bool raw_mode_war;


	/* 8021p PCP-TID mapping table ID */
	uint8_t tidmap_tbl_id;

	/* 8021p PCP-TID map values */
	uint8_t pcp_tid_map[PCP_TID_MAP_MAX];

	/* TIDmap priority */
	uint8_t tidmap_prty;

#ifdef QCA_MULTIPASS_SUPPORT
	uint16_t *iv_vlan_map;

	/* dp_peer special list */
	TAILQ_HEAD(, dp_txrx_peer) mpass_peer_list;
	DP_MUTEX_TYPE mpass_peer_mutex;
#endif
	/* Extended data path handle */
	struct cdp_ext_vdev *vdev_dp_ext_handle;
#ifdef VDEV_PEER_PROTOCOL_COUNT
	/*
	 * Rx-Ingress and Tx-Egress are in the lower level DP layer
	 * Rx-Egress and Tx-ingress are handled in osif layer for DP
	 * So
	 * Rx-Egress and Tx-ingress mask definitions are in OSIF layer
	 * Rx-Ingress and Tx-Egress definitions are here below
	 */
#define VDEV_PEER_PROTOCOL_RX_INGRESS_MASK 1
#define VDEV_PEER_PROTOCOL_TX_INGRESS_MASK 2
#define VDEV_PEER_PROTOCOL_RX_EGRESS_MASK 4
#define VDEV_PEER_PROTOCOL_TX_EGRESS_MASK 8
	bool peer_protocol_count_track;
	int peer_protocol_count_dropmask;
#endif
	/* callback to collect connectivity stats */
	ol_txrx_stats_rx_fp stats_cb;
	uint32_t num_peers;
	/* entry to inactive_list*/
	TAILQ_ENTRY(dp_vdev) inactive_list_elem;

#ifdef WLAN_SUPPORT_RX_FISA
	/**
	 * Params used for controlling the fisa aggregation dynamically
	 */
	uint8_t fisa_disallowed[MAX_REO_DEST_RINGS];
	uint8_t fisa_force_flushed[MAX_REO_DEST_RINGS];
#endif
	/*
	 * Refcount for VDEV currently incremented when
	 * peer is created for VDEV
	 */
	qdf_atomic_t ref_cnt;
	qdf_atomic_t mod_refs[DP_MOD_ID_MAX];
	uint8_t num_latency_critical_conn;
#ifdef WLAN_SUPPORT_MESH_LATENCY
	uint8_t peer_tid_latency_enabled;
	/* tid latency configuration parameters */
	struct {
		uint32_t service_interval;
		uint32_t burst_size;
		uint8_t latency_tid;
	} mesh_tid_latency_config;
#endif
#ifdef WIFI_MONITOR_SUPPORT
	struct dp_mon_vdev *monitor_vdev;
#endif
#if defined(WLAN_FEATURE_TSF_UPLINK_DELAY) || defined(WLAN_CONFIG_TX_DELAY)
	/* Delta between TQM clock and TSF clock */
	uint32_t delta_tsf;
#endif
#ifdef WLAN_FEATURE_TSF_UPLINK_DELAY
	/* Indicate if uplink delay report is enabled or not */
	qdf_atomic_t ul_delay_report;
	/* accumulative delay for every TX completion */
	qdf_atomic_t ul_delay_accum;
	/* accumulative number of packets delay has accumulated */
	qdf_atomic_t ul_pkts_accum;
#endif /* WLAN_FEATURE_TSF_UPLINK_DELAY */

	/* vdev_stats_id - ID used for stats collection by FW from HW*/
	uint8_t vdev_stats_id;
#ifdef HW_TX_DELAY_STATS_ENABLE
	/* hw tx delay stats enable */
	uint8_t hw_tx_delay_stats_enabled;
#endif
#ifdef DP_RX_UDP_OVER_PEER_ROAM
	uint32_t roaming_peer_status;
	union dp_align_mac_addr roaming_peer_mac;
#endif
#ifdef DP_TRAFFIC_END_INDICATION
	/* per vdev feature enable/disable status */
	bool traffic_end_ind_en;
	/* per vdev nbuf queue for traffic end indication packets */
	qdf_nbuf_queue_t end_ind_pkt_q;
#endif
#ifdef FEATURE_DIRECT_LINK
	/* Flag to indicate if to_fw should be set for tx pkts on this vdev */
	bool to_fw;
#endif
};

enum {
	dp_sec_mcast = 0,
	dp_sec_ucast
};

#ifdef WDS_VENDOR_EXTENSION
typedef struct {
	uint8_t	wds_tx_mcast_4addr:1,
		wds_tx_ucast_4addr:1,
		wds_rx_filter:1,      /* enforce rx filter */
		wds_rx_ucast_4addr:1, /* when set, accept 4addr unicast frames    */
		wds_rx_mcast_4addr:1;  /* when set, accept 4addr multicast frames  */

} dp_ecm_policy;
#endif

/**
 * struct dp_peer_cached_bufq - cached_bufq to enqueue rx packets
 * @cached_bufq: nbuff list to enqueue rx packets
 * @bufq_lock: spinlock for nbuff list access
 * @thresh: maximum threshold for number of rx buff to enqueue
 * @entries: number of entries
 * @dropped: number of packets dropped
 */
struct dp_peer_cached_bufq {
	qdf_list_t cached_bufq;
	qdf_spinlock_t bufq_lock;
	uint32_t thresh;
	uint32_t entries;
	uint32_t dropped;
};

/**
 * enum dp_peer_ast_flowq
 * @DP_PEER_AST_FLOWQ_HI_PRIO: Hi Priority flow queue
 * @DP_PEER_AST_FLOWQ_LOW_PRIO: Low priority flow queue
 * @DP_PEER_AST_FLOWQ_UDP: flow queue type is UDP
 * @DP_PEER_AST_FLOWQ_NON_UDP: flow queue type is Non UDP
 * @DP_PEER_AST_FLOWQ_MAX: max value
 */
enum dp_peer_ast_flowq {
	DP_PEER_AST_FLOWQ_HI_PRIO,
	DP_PEER_AST_FLOWQ_LOW_PRIO,
	DP_PEER_AST_FLOWQ_UDP,
	DP_PEER_AST_FLOWQ_NON_UDP,
	DP_PEER_AST_FLOWQ_MAX,
};

/**
 * struct dp_ast_flow_override_info - ast override info
 * @ast_idx: ast indexes in peer map message
 * @ast_valid_mask: ast valid mask for each ast index
 * @ast_flow_mask: ast flow mask for each ast index
 * @tid_valid_low_pri_mask: per tid mask for low priority flow
 * @tid_valid_hi_pri_mask: per tid mask for hi priority flow
 */
struct dp_ast_flow_override_info {
	uint16_t ast_idx[DP_PEER_AST_FLOWQ_MAX];
	uint8_t ast_valid_mask;
	uint8_t ast_flow_mask[DP_PEER_AST_FLOWQ_MAX];
	uint8_t tid_valid_low_pri_mask;
	uint8_t tid_valid_hi_pri_mask;
};

/**
 * struct dp_peer_ast_params - ast parameters for a msdu flow-queue
 * @ast_idx: ast index populated by FW
 * @is_valid: ast flow valid mask
 * @valid_tid_mask: per tid mask for this ast index
 * @flowQ: flow queue id associated with this ast index
 */
struct dp_peer_ast_params {
	uint16_t ast_idx;
	uint8_t is_valid;
	uint8_t valid_tid_mask;
	uint8_t flowQ;
};

#define DP_MLO_FLOW_INFO_MAX	3

/**
 * struct dp_mlo_flow_override_info - Flow override info
 * @ast_idx: Primary TCL AST Index
 * @ast_idx_valid: Is AST index valid
 * @chip_id: CHIP ID
 * @tidmask: tidmask
 * @cache_set_num: Cache set number
 */
struct dp_mlo_flow_override_info {
	uint16_t ast_idx;
	uint8_t ast_idx_valid;
	uint8_t chip_id;
	uint8_t tidmask;
	uint8_t cache_set_num;
};

/**
 * struct dp_mlo_link_info - Link info
 * @peer_chip_id: Peer Chip ID
 * @vdev_id: Vdev ID
 */
struct dp_mlo_link_info {
	uint8_t peer_chip_id;
	uint8_t vdev_id;
};

#ifdef WLAN_SUPPORT_MSCS
/*MSCS Procedure based macros */
#define IEEE80211_MSCS_MAX_ELEM_SIZE    5
#define IEEE80211_TCLAS_MASK_CLA_TYPE_4  4
/**
 * struct dp_peer_mscs_parameter - MSCS database obtained from
 * MSCS Request and Response in the control path. This data is used
 * by the AP to find out what priority to set based on the tuple
 * classification during packet processing.
 * @user_priority_bitmap: User priority bitmap obtained during
 * handshake
 * @user_priority_limit: User priority limit obtained during
 * handshake
 * @classifier_mask: params to be compared during processing
 */
struct dp_peer_mscs_parameter {
	uint8_t user_priority_bitmap;
	uint8_t user_priority_limit;
	uint8_t classifier_mask;
};
#endif

#ifdef QCA_SUPPORT_WDS_EXTENDED
#define WDS_EXT_PEER_INIT_BIT 0

/**
 * struct dp_wds_ext_peer - wds ext peer structure
 * This is used when wds extended feature is enabled
 * both compile time and run time. It is created
 * when 1st 4 address frame is received from
 * wds backhaul.
 * @osif_peer: Handle to the OS shim SW's virtual device
 * @init: wds ext netdev state
 */
struct dp_wds_ext_peer {
	ol_osif_peer_handle osif_peer;
	unsigned long init;
};
#endif /* QCA_SUPPORT_WDS_EXTENDED */

#ifdef WLAN_SUPPORT_MESH_LATENCY
/*Advanced Mesh latency feature based macros */

/**
 * struct dp_peer_mesh_latency_parameter - Mesh latency related
 * parameters. This data is updated per peer per TID based on
 * the flow tuple classification in external rule database
 * during packet processing.
 * @service_interval_dl: Service interval associated with TID in DL
 * @burst_size_dl: Burst size additive over multiple flows in DL
 * @service_interval_ul: Service interval associated with TID in UL
 * @burst_size_ul: Burst size additive over multiple flows in UL
 * @ac: custom ac derived from service interval
 * @msduq: MSDU queue number within TID
 */
struct dp_peer_mesh_latency_parameter {
	uint32_t service_interval_dl;
	uint32_t burst_size_dl;
	uint32_t service_interval_ul;
	uint32_t burst_size_ul;
	uint8_t ac;
	uint8_t msduq;
};
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/* Max number of links for MLO connection */
#define DP_MAX_MLO_LINKS 3

/**
 * struct dp_peer_link_info - link peer information for MLO
 * @mac_addr: Mac address
 * @vdev_id: Vdev ID for current link peer
 * @is_valid: flag for link peer info valid or not
 * @chip_id: chip id
 */
struct dp_peer_link_info {
	union dp_align_mac_addr mac_addr;
	uint8_t vdev_id;
	uint8_t is_valid;
	uint8_t chip_id;
};

/**
 * struct dp_mld_link_peers - this structure is used to get link peers
 *			      pointer from mld peer
 * @link_peers: link peers pointer array
 * @num_links: number of link peers fetched
 */
struct dp_mld_link_peers {
	struct dp_peer *link_peers[DP_MAX_MLO_LINKS];
	uint8_t num_links;
};
#endif

typedef void *dp_txrx_ref_handle;

/**
 * struct dp_peer_per_pkt_tx_stats- Peer Tx stats updated in per pkt
 *				Tx completion path
 * @ucast: Unicast Packet Count
 * @mcast: Multicast Packet Count
 * @bcast: Broadcast Packet Count
 * @nawds_mcast: NAWDS Multicast Packet Count
 * @tx_success: Successful Tx Packets
 * @nawds_mcast_drop: NAWDS Multicast Drop Count
 * @ofdma: Total Packets as ofdma
 * @non_amsdu_cnt: Number of MSDUs with no MSDU level aggregation
 * @amsdu_cnt: Number of MSDUs part of AMSDU
 * @dropped: Dropped packet statistics
 * @dropped.fw_rem: Discarded by firmware
 * @dropped.fw_rem_notx: firmware_discard_untransmitted
 * @dropped.fw_rem_tx: firmware_discard_transmitted
 * @dropped.age_out: aged out in mpdu/msdu queues
 * @dropped.fw_reason1: discarded by firmware reason 1
 * @dropped.fw_reason2: discarded by firmware reason 2
 * @dropped.fw_reason3: discarded by firmware reason  3
 * @dropped.fw_rem_no_match: dropped due to fw no match command
 * @dropped.drop_threshold: dropped due to HW threshold
 * @dropped.drop_link_desc_na: dropped due resource not available in HW
 * @dropped.invalid_drop: Invalid msdu drop
 * @dropped.mcast_vdev_drop: MCAST drop configured for VDEV in HW
 * @dropped.invalid_rr: Invalid TQM release reason
 * @failed_retry_count: packets failed due to retry above 802.11 retry limit
 * @retry_count: packets successfully send after one or more retry
 * @multiple_retry_count: packets successfully sent after more than one retry
 * @no_ack_count: no ack pkt count for different protocols
 * @tx_success_twt: Successful Tx Packets in TWT session
 * @last_tx_ts: last timestamp in jiffies when tx comp occurred
 * @avg_sojourn_msdu: Avg sojourn msdu stat
 * @protocol_trace_cnt: per-peer protocol counter
 * @release_src_not_tqm: Counter to keep track of release source is not TQM
 *			 in TX completion status processing
 */
struct dp_peer_per_pkt_tx_stats {
	struct cdp_pkt_info ucast;
	struct cdp_pkt_info mcast;
	struct cdp_pkt_info bcast;
	struct cdp_pkt_info nawds_mcast;
	struct cdp_pkt_info tx_success;
	uint32_t nawds_mcast_drop;
	uint32_t ofdma;
	uint32_t non_amsdu_cnt;
	uint32_t amsdu_cnt;
	struct {
		struct cdp_pkt_info fw_rem;
		uint32_t fw_rem_notx;
		uint32_t fw_rem_tx;
		uint32_t age_out;
		uint32_t fw_reason1;
		uint32_t fw_reason2;
		uint32_t fw_reason3;
		uint32_t fw_rem_queue_disable;
		uint32_t fw_rem_no_match;
		uint32_t drop_threshold;
		uint32_t drop_link_desc_na;
		uint32_t invalid_drop;
		uint32_t mcast_vdev_drop;
		uint32_t invalid_rr;
	} dropped;
	uint32_t failed_retry_count;
	uint32_t retry_count;
	uint32_t multiple_retry_count;
	uint32_t no_ack_count[QDF_PROTO_SUBTYPE_MAX];
	struct cdp_pkt_info tx_success_twt;
	unsigned long last_tx_ts;
	qdf_ewma_tx_lag avg_sojourn_msdu[CDP_DATA_TID_MAX];
#ifdef VDEV_PEER_PROTOCOL_COUNT
	struct protocol_trace_count protocol_trace_cnt[CDP_TRACE_MAX];
#endif
	uint32_t release_src_not_tqm;
};

/**
 * struct dp_peer_extd_tx_stats - Peer Tx stats updated in either
 *	per pkt Tx completion path when macro QCA_ENHANCED_STATS_SUPPORT is
 *	disabled or in HTT Tx PPDU completion path when macro is enabled
 * @stbc: Packets in STBC
 * @ldpc: Packets in LDPC
 * @retries: Packet retries
 * @pkt_type: pkt count for different .11 modes
 * @wme_ac_type: Wireless Multimedia type Count
 * @excess_retries_per_ac: Wireless Multimedia type Count
 * @ampdu_cnt: completion of aggregation
 * @non_ampdu_cnt: tx completion not aggregated
 * @num_ppdu_cookie_valid: no. of valid ppdu cookies rcvd from FW
 * @tx_ppdus: ppdus in tx
 * @tx_mpdus_success: mpdus successful in tx
 * @tx_mpdus_tried: mpdus tried in tx
 * @tx_rate: Tx Rate in kbps
 * @last_tx_rate: Last tx rate for unicast packets
 * @last_tx_rate_mcs: Tx rate mcs for unicast packets
 * @mcast_last_tx_rate: Last tx rate for multicast packets
 * @mcast_last_tx_rate_mcs: Last tx rate mcs for multicast
 * @rnd_avg_tx_rate: Rounded average tx rate
 * @avg_tx_rate: Average TX rate
 * @tx_ratecode: Tx rate code of last frame
 * @pream_punct_cnt: Preamble Punctured count
 * @sgi_count: SGI count
 * @nss: Packet count for different num_spatial_stream values
 * @bw: Packet Count for different bandwidths
 * @ru_start: RU start index
 * @ru_tones: RU tones size
 * @ru_loc: pkt info for RU location 26/ 52/ 106/ 242/ 484 counter
 * @transmit_type: pkt info for tx transmit type
 * @mu_group_id: mumimo mu group id
 * @last_ack_rssi: RSSI of last acked packet
 * @nss_info: NSS 1,2, ...8
 * @mcs_info: MCS index
 * @bw_info: Bandwidth
 *       <enum 0 bw_20_MHz>
 *       <enum 1 bw_40_MHz>
 *       <enum 2 bw_80_MHz>
 *       <enum 3 bw_160_MHz>
 * @gi_info: <enum 0     0_8_us_sgi > Legacy normal GI
 *       <enum 1     0_4_us_sgi > Legacy short GI
 *       <enum 2     1_6_us_sgi > HE related GI
 *       <enum 3     3_2_us_sgi > HE
 * @preamble_info: preamble
 * @tx_ucast_total: total ucast count
 * @tx_ucast_success: total ucast success count
 * @retries_mpdu: mpdu number of successfully transmitted after retries
 * @mpdu_success_with_retries: mpdu retry count in case of successful tx
 * @su_be_ppdu_cnt: SU Tx packet count for 11BE
 * @mu_be_ppdu_cnt: MU Tx packet count for 11BE
 * @punc_bw: MSDU count for punctured bw
 * @rts_success: RTS success count
 * @rts_failure: RTS failure count
 * @bar_cnt: Block ACK Request frame count
 * @ndpa_cnt: NDP announcement frame count
 * @wme_ac_type_bytes: Wireless Multimedia bytes Count
 */
struct dp_peer_extd_tx_stats {
	uint32_t stbc;
	uint32_t ldpc;
	uint32_t retries;
	struct cdp_pkt_type pkt_type[DOT11_MAX];
	uint32_t wme_ac_type[WME_AC_MAX];
	uint32_t excess_retries_per_ac[WME_AC_MAX];
	uint32_t ampdu_cnt;
	uint32_t non_ampdu_cnt;
	uint32_t num_ppdu_cookie_valid;
	uint32_t tx_ppdus;
	uint32_t tx_mpdus_success;
	uint32_t tx_mpdus_tried;

	uint32_t tx_rate;
	uint32_t last_tx_rate;
	uint32_t last_tx_rate_mcs;
	uint32_t mcast_last_tx_rate;
	uint32_t mcast_last_tx_rate_mcs;
	uint64_t rnd_avg_tx_rate;
	uint64_t avg_tx_rate;
	uint16_t tx_ratecode;

	uint32_t sgi_count[MAX_GI];
	uint32_t pream_punct_cnt;
	uint32_t nss[SS_COUNT];
	uint32_t bw[MAX_BW];
	uint32_t ru_start;
	uint32_t ru_tones;
	struct cdp_tx_pkt_info ru_loc[MAX_RU_LOCATIONS];

	struct cdp_tx_pkt_info transmit_type[MAX_TRANSMIT_TYPES];
	uint32_t mu_group_id[MAX_MU_GROUP_ID];

	uint32_t last_ack_rssi;

	uint32_t nss_info:4,
		 mcs_info:4,
		 bw_info:4,
		 gi_info:4,
		 preamble_info:4;

	uint32_t retries_mpdu;
	uint32_t mpdu_success_with_retries;
	struct cdp_pkt_info tx_ucast_total;
	struct cdp_pkt_info tx_ucast_success;
#ifdef WLAN_FEATURE_11BE
	struct cdp_pkt_type su_be_ppdu_cnt;
	struct cdp_pkt_type mu_be_ppdu_cnt[TXRX_TYPE_MU_MAX];
	uint32_t punc_bw[MAX_PUNCTURED_MODE];
#endif
	uint32_t rts_success;
	uint32_t rts_failure;
	uint32_t bar_cnt;
	uint32_t ndpa_cnt;
	uint64_t wme_ac_type_bytes[WME_AC_MAX];
};

/**
 * struct dp_peer_per_pkt_rx_stats - Peer Rx stats updated in per pkt Rx path
 * @rcvd_reo: Packets received on the reo ring
 * @rx_lmac: Packets received on each lmac
 * @unicast: Total unicast packets
 * @multicast: Total multicast packets
 * @bcast:  Broadcast Packet Count
 * @raw: Raw Pakets received
 * @nawds_mcast_drop: Total NAWDS multicast packets dropped
 * @mec_drop: Total MEC packets dropped
 * @last_rx_ts: last timestamp in jiffies when RX happened
 * @intra_bss: Intra BSS statistics
 * @intra_bss.pkts: Intra BSS packets received
 * @intra_bss.fail: Intra BSS packets failed
 * @intra_bss.mdns_no_fws: Intra BSS MDNS packets not forwarded
 * @err: error counters
 * @err.mic_err: Rx MIC errors CCMP
 * @err.decrypt_err: Rx Decryption Errors CRC
 * @err.fcserr: rx MIC check failed (CCMP)
 * @err.pn_err: pn check failed
 * @err.oor_err: Rx OOR errors
 * @err.jump_2k_err: 2k jump errors
 * @err.rxdma_wifi_parse_err: rxdma wifi parse errors
 * @non_amsdu_cnt: Number of MSDUs with no MSDU level aggregation
 * @amsdu_cnt: Number of MSDUs part of AMSDU
 * @rx_retries: retries of packet in rx
 * @multipass_rx_pkt_drop: Dropped multipass rx pkt
 * @peer_unauth_rx_pkt_drop: Unauth rx packet drops
 * @policy_check_drop: policy check drops
 * @to_stack_twt: Total packets sent up the stack in TWT session
 * @protocol_trace_cnt: per-peer protocol counters
 * @mcast_3addr_drop:
 * @rx_total: total rx count
 */
struct dp_peer_per_pkt_rx_stats {
	struct cdp_pkt_info rcvd_reo[CDP_MAX_RX_RINGS];
	struct cdp_pkt_info rx_lmac[CDP_MAX_LMACS];
	struct cdp_pkt_info unicast;
	struct cdp_pkt_info multicast;
	struct cdp_pkt_info bcast;
	struct cdp_pkt_info raw;
	uint32_t nawds_mcast_drop;
	struct cdp_pkt_info mec_drop;
	unsigned long last_rx_ts;
	struct {
		struct cdp_pkt_info pkts;
		struct cdp_pkt_info fail;
		uint32_t mdns_no_fwd;
	} intra_bss;
	struct {
		uint32_t mic_err;
		uint32_t decrypt_err;
		uint32_t fcserr;
		uint32_t pn_err;
		uint32_t oor_err;
		uint32_t jump_2k_err;
		uint32_t rxdma_wifi_parse_err;
	} err;
	uint32_t non_amsdu_cnt;
	uint32_t amsdu_cnt;
	uint32_t rx_retries;
	uint32_t multipass_rx_pkt_drop;
	uint32_t peer_unauth_rx_pkt_drop;
	uint32_t policy_check_drop;
	struct cdp_pkt_info to_stack_twt;
#ifdef VDEV_PEER_PROTOCOL_COUNT
	struct protocol_trace_count protocol_trace_cnt[CDP_TRACE_MAX];
#endif
	uint32_t mcast_3addr_drop;
#ifdef IPA_OFFLOAD
	struct cdp_pkt_info rx_total;
#endif
};

/**
 * struct dp_peer_extd_rx_stats - Peer Rx stats updated in either
 *	per pkt Rx path when macro QCA_ENHANCED_STATS_SUPPORT is disabled or in
 *	Rx monitor patch when macro is enabled
 * @pkt_type: pkt counter for different .11 modes
 * @wme_ac_type: Wireless Multimedia type Count
 * @mpdu_cnt_fcs_ok: SU Rx success mpdu count
 * @mpdu_cnt_fcs_err: SU Rx fail mpdu count
 * @non_ampdu_cnt: Number of MSDUs with no MPDU level aggregation
 * @ampdu_cnt: Number of MSDUs part of AMSPU
 * @rx_mpdus: mpdu in rx
 * @rx_ppdus: ppdu in rx
 * @su_ax_ppdu_cnt: SU Rx packet count for .11ax
 * @rx_mu: Rx MU stats
 * @reception_type: Reception type of packets
 * @ppdu_cnt: PPDU packet count in reception type
 * @sgi_count: sgi count
 * @nss: packet count in spatiel Streams
 * @ppdu_nss: PPDU packet count in spatial streams
 * @bw: Packet Count in different bandwidths
 * @rx_mpdu_cnt: rx mpdu count per MCS rate
 * @rx_rate: Rx rate
 * @last_rx_rate: Previous rx rate
 * @rnd_avg_rx_rate: Rounded average rx rate
 * @avg_rx_rate: Average Rx rate
 * @rx_ratecode: Rx rate code of last frame
 * @avg_snr: Average snr
 * @rx_snr_measured_time: Time at which snr is measured
 * @snr: SNR of received signal
 * @last_snr: Previous snr
 * @nss_info: NSS 1,2, ...8
 * @mcs_info: MCS index
 * @bw_info: Bandwidth
 *       <enum 0 bw_20_MHz>
 *       <enum 1 bw_40_MHz>
 *       <enum 2 bw_80_MHz>
 *       <enum 3 bw_160_MHz>
 * @gi_info: <enum 0     0_8_us_sgi > Legacy normal GI
 *       <enum 1     0_4_us_sgi > Legacy short GI
 *       <enum 2     1_6_us_sgi > HE related GI
 *       <enum 3     3_2_us_sgi > HE
 * @preamble_info: preamble
 * @mpdu_retry_cnt: retries of mpdu in rx
 * @su_be_ppdu_cnt: SU Rx packet count for BE
 * @mu_be_ppdu_cnt: MU rx packet count for BE
 * @punc_bw: MSDU count for punctured bw
 * @bar_cnt: Block ACK Request frame count
 * @ndpa_cnt: NDP announcement frame count
 * @wme_ac_type_bytes: Wireless Multimedia type Bytes Count
 */
struct dp_peer_extd_rx_stats {
	struct cdp_pkt_type pkt_type[DOT11_MAX];
	uint32_t wme_ac_type[WME_AC_MAX];
	uint32_t mpdu_cnt_fcs_ok;
	uint32_t mpdu_cnt_fcs_err;
	uint32_t non_ampdu_cnt;
	uint32_t ampdu_cnt;
	uint32_t rx_mpdus;
	uint32_t rx_ppdus;

	struct cdp_pkt_type su_ax_ppdu_cnt;
	struct cdp_rx_mu rx_mu[TXRX_TYPE_MU_MAX];
	uint32_t reception_type[MAX_RECEPTION_TYPES];
	uint32_t ppdu_cnt[MAX_RECEPTION_TYPES];

	uint32_t sgi_count[MAX_GI];
	uint32_t nss[SS_COUNT];
	uint32_t ppdu_nss[SS_COUNT];
	uint32_t bw[MAX_BW];
	uint32_t rx_mpdu_cnt[MAX_MCS];

	uint32_t rx_rate;
	uint32_t last_rx_rate;
	uint32_t rnd_avg_rx_rate;
	uint32_t avg_rx_rate;
	uint32_t rx_ratecode;

	uint32_t avg_snr;
	unsigned long rx_snr_measured_time;
	uint8_t snr;
	uint8_t last_snr;

	uint32_t nss_info:4,
		 mcs_info:4,
		 bw_info:4,
		 gi_info:4,
		 preamble_info:4;

	uint32_t mpdu_retry_cnt;
#ifdef WLAN_FEATURE_11BE
	struct cdp_pkt_type su_be_ppdu_cnt;
	struct cdp_pkt_type mu_be_ppdu_cnt[TXRX_TYPE_MU_MAX];
	uint32_t punc_bw[MAX_PUNCTURED_MODE];
#endif
	uint32_t bar_cnt;
	uint32_t ndpa_cnt;
	uint64_t wme_ac_type_bytes[WME_AC_MAX];
};

/**
 * struct dp_peer_per_pkt_stats - Per pkt stats for peer
 * @tx: Per pkt Tx stats
 * @rx: Per pkt Rx stats
 */
struct dp_peer_per_pkt_stats {
	struct dp_peer_per_pkt_tx_stats tx;
	struct dp_peer_per_pkt_rx_stats rx;
};

/**
 * struct dp_peer_extd_stats - Stats from extended path for peer
 * @tx: Extended path tx stats
 * @rx: Extended path rx stats
 */
struct dp_peer_extd_stats {
	struct dp_peer_extd_tx_stats tx;
	struct dp_peer_extd_rx_stats rx;
};

/**
 * struct dp_peer_stats - Peer stats
 * @per_pkt_stats: Per packet path stats
 * @extd_stats: Extended path stats
 */
struct dp_peer_stats {
	struct dp_peer_per_pkt_stats per_pkt_stats;
#ifndef QCA_ENHANCED_STATS_SUPPORT
	struct dp_peer_extd_stats extd_stats;
#endif
};

/**
 * struct dp_txrx_peer: DP txrx_peer structure used in per pkt path
 * @vdev: VDEV to which this peer is associated
 * @peer_id: peer ID for this peer
 * @authorize: Set when authorized
 * @in_twt: in TWT session
 * @hw_txrx_stats_en: Indicate HW offload vdev stats
 * @mld_peer:1: MLD peer
 * @tx_failed: Total Tx failure
 * @comp_pkt: Pkt Info for which completions were received
 * @to_stack: Total packets sent up the stack
 * @stats: Peer stats
 * @delay_stats: Peer delay stats
 * @jitter_stats: Peer jitter stats
 * @security: Security credentials
 * @nawds_enabled: NAWDS flag
 * @bss_peer: set for bss peer
 * @isolation: enable peer isolation for this peer
 * @wds_enabled: WDS peer
 * @wds_ecm:
 * @flush_in_progress:
 * @bufq_info:
 * @mpass_peer_list_elem: node in the special peer list element
 * @vlan_id: vlan id for key
 * @wds_ext:
 * @osif_rx:
 * @rx_tid:
 * @sawf_stats:
 * @bw: bandwidth of peer connection
 * @mpdu_retry_threshold: MPDU retry threshold to increment tx bad count
 */
struct dp_txrx_peer {
	struct dp_vdev *vdev;
	uint16_t peer_id;
	uint8_t authorize:1,
		in_twt:1,
		hw_txrx_stats_en:1,
		mld_peer:1;
	uint32_t tx_failed;
	struct cdp_pkt_info comp_pkt;
	struct cdp_pkt_info to_stack;

	struct dp_peer_stats stats;

	struct dp_peer_delay_stats *delay_stats;

	struct cdp_peer_tid_stats *jitter_stats;

	struct {
		enum cdp_sec_type sec_type;
		u_int32_t michael_key[2]; /* relevant for TKIP */
	} security[2]; /* 0 -> multicast, 1 -> unicast */

	uint16_t nawds_enabled:1,
		bss_peer:1,
		isolation:1,
		wds_enabled:1;
#ifdef WDS_VENDOR_EXTENSION
	dp_ecm_policy wds_ecm;
#endif
#ifdef PEER_CACHE_RX_PKTS
	qdf_atomic_t flush_in_progress;
	struct dp_peer_cached_bufq bufq_info;
#endif
#ifdef QCA_MULTIPASS_SUPPORT
	TAILQ_ENTRY(dp_txrx_peer) mpass_peer_list_elem;
	uint16_t vlan_id;
#endif
#ifdef QCA_SUPPORT_WDS_EXTENDED
	struct dp_wds_ext_peer wds_ext;
	ol_txrx_rx_fp osif_rx;
#endif
	struct dp_rx_tid_defrag rx_tid[DP_MAX_TIDS];
#ifdef CONFIG_SAWF
	struct dp_peer_sawf_stats *sawf_stats;
#endif
#ifdef DP_PEER_EXTENDED_API
	enum cdp_peer_bw bw;
	uint8_t mpdu_retry_threshold;
#endif
};

/* Peer structure for data path state */
struct dp_peer {
	struct dp_txrx_peer *txrx_peer;
#ifdef WIFI_MONITOR_SUPPORT
	struct dp_mon_peer *monitor_peer;
#endif
	/* peer ID for this peer */
	uint16_t peer_id;

	/* VDEV to which this peer is associated */
	struct dp_vdev *vdev;

	struct dp_ast_entry *self_ast_entry;

	qdf_atomic_t ref_cnt;

	union dp_align_mac_addr mac_addr;

	/* node in the vdev's list of peers */
	TAILQ_ENTRY(dp_peer) peer_list_elem;
	/* node in the hash table bin's list of peers */
	TAILQ_ENTRY(dp_peer) hash_list_elem;

	/* TID structures pointer */
	struct dp_rx_tid *rx_tid;

	/* TBD: No transmit TID state required? */

	struct {
		enum cdp_sec_type sec_type;
		u_int32_t michael_key[2]; /* relevant for TKIP */
	} security[2]; /* 0 -> multicast, 1 -> unicast */

	/* NAWDS Flag and Bss Peer bit */
	uint16_t bss_peer:1, /* set for bss peer */
		authorize:1, /* Set when authorized */
		valid:1, /* valid bit */
		delete_in_progress:1, /* Indicate kickout sent */
		sta_self_peer:1, /* Indicate STA self peer */
		is_tdls_peer:1; /* Indicate TDLS peer */

#ifdef WLAN_FEATURE_11BE_MLO
	uint8_t first_link:1, /* first link peer for MLO */
		primary_link:1; /* primary link for MLO */
#endif

	/* MCL specific peer local id */
	uint16_t local_id;
	enum ol_txrx_peer_state state;
	qdf_spinlock_t peer_info_lock;

	/* Peer calibrated stats */
	struct cdp_calibr_stats stats;

	TAILQ_HEAD(, dp_ast_entry) ast_entry_list;
	/* TBD */

	/* Active Block ack sessions */
	uint16_t active_ba_session_cnt;

	/* Current HW buffersize setting */
	uint16_t hw_buffer_size;

	/*
	 * Flag to check if sessions with 256 buffersize
	 * should be terminated.
	 */
	uint8_t kill_256_sessions;
	qdf_atomic_t is_default_route_set;

#ifdef QCA_PEER_MULTIQ_SUPPORT
	struct dp_peer_ast_params peer_ast_flowq_idx[DP_PEER_AST_FLOWQ_MAX];
#endif
	/* entry to inactive_list*/
	TAILQ_ENTRY(dp_peer) inactive_list_elem;

	qdf_atomic_t mod_refs[DP_MOD_ID_MAX];

	uint8_t peer_state;
	qdf_spinlock_t peer_state_lock;
#ifdef WLAN_SUPPORT_MSCS
	struct dp_peer_mscs_parameter mscs_ipv4_parameter, mscs_ipv6_parameter;
	bool mscs_active;
#endif
#ifdef WLAN_SUPPORT_MESH_LATENCY
	struct dp_peer_mesh_latency_parameter mesh_latency_params[DP_MAX_TIDS];
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	/* peer type */
	enum cdp_peer_type peer_type;
	/*---------for link peer---------*/
	struct dp_peer *mld_peer;
	/*---------for mld peer----------*/
	struct dp_peer_link_info link_peers[DP_MAX_MLO_LINKS];
	uint8_t num_links;
	DP_MUTEX_TYPE link_peers_info_lock;
#endif
#ifdef CONFIG_SAWF_DEF_QUEUES
	struct dp_peer_sawf *sawf;
#endif
	/* AST hash index for peer in HW */
	uint16_t ast_idx;

	/* AST hash value for peer in HW */
	uint16_t ast_hash;
};

/**
 * struct dp_invalid_peer_msg - Invalid peer message
 * @nbuf: data buffer
 * @wh: 802.11 header
 * @vdev_id: id of vdev
 */
struct dp_invalid_peer_msg {
	qdf_nbuf_t nbuf;
	struct ieee80211_frame *wh;
	uint8_t vdev_id;
};

/**
 * struct dp_tx_me_buf_t - ME buffer
 * @next: pointer to next buffer
 * @data: Destination Mac address
 * @paddr_macbuf: physical address for dest_mac
 */
struct dp_tx_me_buf_t {
	/* Note: ME buf pool initialization logic expects next pointer to
	 * be the first element. Dont add anything before next */
	struct dp_tx_me_buf_t *next;
	uint8_t data[QDF_MAC_ADDR_SIZE];
	qdf_dma_addr_t paddr_macbuf;
};

#if defined(WLAN_SUPPORT_RX_FLOW_TAG) || defined(WLAN_SUPPORT_RX_FISA)
struct hal_rx_fst;

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
struct dp_rx_fse {
	/* HAL Rx Flow Search Entry which matches HW definition */
	void *hal_rx_fse;
	/* Toeplitz hash value */
	uint32_t flow_hash;
	/* Flow index, equivalent to hash value truncated to FST size */
	uint32_t flow_id;
	/* Stats tracking for this flow */
	struct cdp_flow_stats stats;
	/* Flag indicating whether flow is IPv4 address tuple */
	uint8_t is_ipv4_addr_entry;
	/* Flag indicating whether flow is valid */
	uint8_t is_valid;
};

struct dp_rx_fst {
	/* Software (DP) FST */
	uint8_t *base;
	/* Pointer to HAL FST */
	struct hal_rx_fst *hal_rx_fst;
	/* Base physical address of HAL RX HW FST */
	uint64_t hal_rx_fst_base_paddr;
	/* Maximum number of flows FSE supports */
	uint16_t max_entries;
	/* Num entries in flow table */
	uint16_t num_entries;
	/* SKID Length */
	uint16_t max_skid_length;
	/* Hash mask to obtain legitimate hash entry */
	uint32_t hash_mask;
	/* Timer for bundling of flows */
	qdf_timer_t cache_invalidate_timer;
	/**
	 * Flag which tracks whether cache update
	 * is needed on timer expiry
	 */
	qdf_atomic_t is_cache_update_pending;
	/* Flag to indicate completion of FSE setup in HW/FW */
	bool fse_setup_done;
};

#define DP_RX_GET_SW_FT_ENTRY_SIZE sizeof(struct dp_rx_fse)
#elif WLAN_SUPPORT_RX_FISA

/**
 * struct dp_fisa_reo_mismatch_stats - reo mismatch sub-case stats for FISA
 * @allow_cce_match: packet allowed due to cce mismatch
 * @allow_fse_metdata_mismatch: packet allowed since it belongs to same flow,
 *			only fse_metadata is not same.
 * @allow_non_aggr: packet allowed due to any other reason.
 */
struct dp_fisa_reo_mismatch_stats {
	uint32_t allow_cce_match;
	uint32_t allow_fse_metdata_mismatch;
	uint32_t allow_non_aggr;
};

struct dp_fisa_stats {
	/* flow index invalid from RX HW TLV */
	uint32_t invalid_flow_index;
	/* workqueue deferred due to suspend */
	uint32_t update_deferred;
	struct dp_fisa_reo_mismatch_stats reo_mismatch;
};

enum fisa_aggr_ret {
	FISA_AGGR_DONE,
	FISA_AGGR_NOT_ELIGIBLE,
	FISA_FLUSH_FLOW
};

/**
 * struct fisa_pkt_hist - FISA Packet history structure
 * @tlv_hist: array of TLV history
 * @ts_hist: array of timestamps of fisa packets
 * @idx: index indicating the next location to be used in the array.
 */
struct fisa_pkt_hist {
	uint8_t *tlv_hist;
	qdf_time_t ts_hist[FISA_FLOW_MAX_AGGR_COUNT];
	uint32_t idx;
};

struct dp_fisa_rx_sw_ft {
	/* HAL Rx Flow Search Entry which matches HW definition */
	void *hw_fse;
	/* hash value */
	uint32_t flow_hash;
	/* toeplitz hash value*/
	uint32_t flow_id_toeplitz;
	/* Flow index, equivalent to hash value truncated to FST size */
	uint32_t flow_id;
	/* Stats tracking for this flow */
	struct cdp_flow_stats stats;
	/* Flag indicating whether flow is IPv4 address tuple */
	uint8_t is_ipv4_addr_entry;
	/* Flag indicating whether flow is valid */
	uint8_t is_valid;
	uint8_t is_populated;
	uint8_t is_flow_udp;
	uint8_t is_flow_tcp;
	qdf_nbuf_t head_skb;
	uint16_t cumulative_l4_checksum;
	uint16_t adjusted_cumulative_ip_length;
	uint16_t cur_aggr;
	uint16_t napi_flush_cumulative_l4_checksum;
	uint16_t napi_flush_cumulative_ip_length;
	qdf_nbuf_t last_skb;
	uint32_t head_skb_ip_hdr_offset;
	uint32_t head_skb_l4_hdr_offset;
	struct cdp_rx_flow_tuple_info rx_flow_tuple_info;
	uint8_t napi_id;
	struct dp_vdev *vdev;
	uint64_t bytes_aggregated;
	uint32_t flush_count;
	uint32_t aggr_count;
	uint8_t do_not_aggregate;
	uint16_t hal_cumultive_ip_len;
	struct dp_soc *soc_hdl;
	/* last aggregate count fetched from RX PKT TLV */
	uint32_t last_hal_aggr_count;
	uint32_t cur_aggr_gso_size;
	qdf_net_udphdr_t *head_skb_udp_hdr;
	uint16_t frags_cumulative_len;
	/* CMEM parameters */
	uint32_t cmem_offset;
	uint32_t metadata;
	uint32_t reo_dest_indication;
	qdf_time_t flow_init_ts;
	qdf_time_t last_accessed_ts;
#ifdef WLAN_SUPPORT_RX_FISA_HIST
	struct fisa_pkt_hist pkt_hist;
#endif
};

#define DP_RX_GET_SW_FT_ENTRY_SIZE sizeof(struct dp_fisa_rx_sw_ft)
#define MAX_FSE_CACHE_FL_HST 10
/**
 * struct fse_cache_flush_history - Debug history cache flush
 * @timestamp: Entry update timestamp
 * @flows_added: Number of flows added for this flush
 * @flows_deleted: Number of flows deleted for this flush
 */
struct fse_cache_flush_history {
	uint64_t timestamp;
	uint32_t flows_added;
	uint32_t flows_deleted;
};

struct dp_rx_fst {
	/* Software (DP) FST */
	uint8_t *base;
	/* Pointer to HAL FST */
	struct hal_rx_fst *hal_rx_fst;
	/* Base physical address of HAL RX HW FST */
	uint64_t hal_rx_fst_base_paddr;
	/* Maximum number of flows FSE supports */
	uint16_t max_entries;
	/* Num entries in flow table */
	uint16_t num_entries;
	/* SKID Length */
	uint16_t max_skid_length;
	/* Hash mask to obtain legitimate hash entry */
	uint32_t hash_mask;
	/* Lock for adding/deleting entries of FST */
	qdf_spinlock_t dp_rx_fst_lock;
	uint32_t add_flow_count;
	uint32_t del_flow_count;
	uint32_t hash_collision_cnt;
	struct dp_soc *soc_hdl;
	qdf_atomic_t fse_cache_flush_posted;
	qdf_timer_t fse_cache_flush_timer;
	/* Allow FSE cache flush cmd to FW */
	bool fse_cache_flush_allow;
	struct fse_cache_flush_history cache_fl_rec[MAX_FSE_CACHE_FL_HST];
	/* FISA DP stats */
	struct dp_fisa_stats stats;

	/* CMEM params */
	qdf_work_t fst_update_work;
	qdf_workqueue_t *fst_update_wq;
	qdf_list_t fst_update_list;
	uint32_t meta_counter;
	uint32_t cmem_ba;
	qdf_spinlock_t dp_rx_sw_ft_lock[MAX_REO_DEST_RINGS];
	qdf_event_t cmem_resp_event;
	bool flow_deletion_supported;
	bool fst_in_cmem;
	qdf_atomic_t pm_suspended;
	bool fst_wq_defer;
};

#endif /* WLAN_SUPPORT_RX_FISA */
#endif /* WLAN_SUPPORT_RX_FLOW_TAG || WLAN_SUPPORT_RX_FISA */

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * struct dp_req_rx_hw_stats_t - RX peer HW stats query structure
 * @pending_tid_stats_cnt: pending tid stats count which waits for REO status
 * @is_query_timeout: flag to show is stats query timeout
 */
struct dp_req_rx_hw_stats_t {
	qdf_atomic_t pending_tid_stats_cnt;
	bool is_query_timeout;
};
#endif
/* soc level structure to declare arch specific ops for DP */

/**
 * dp_hw_link_desc_pool_banks_free() - Free h/w link desc pool banks
 * @soc: DP SOC handle
 * @mac_id: mac id
 *
 * Return: none
 */
void dp_hw_link_desc_pool_banks_free(struct dp_soc *soc, uint32_t mac_id);

/**
 * dp_hw_link_desc_pool_banks_alloc() - Allocate h/w link desc pool banks
 * @soc: DP SOC handle
 * @mac_id: mac id
 *
 * Allocates memory pages for link descriptors, the page size is 4K for
 * MCL and 2MB for WIN. if the mac_id is invalid link descriptor pages are
 * allocated for regular RX/TX and if the there is a proper mac_id link
 * descriptors are allocated for RX monitor mode.
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *	   QDF_STATUS_E_FAILURE: Failure
 */
QDF_STATUS dp_hw_link_desc_pool_banks_alloc(struct dp_soc *soc,
					    uint32_t mac_id);

/**
 * dp_link_desc_ring_replenish() - Replenish hw link desc rings
 * @soc: DP SOC handle
 * @mac_id: mac id
 *
 * Return: None
 */
void dp_link_desc_ring_replenish(struct dp_soc *soc, uint32_t mac_id);

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
void dp_rx_refill_buff_pool_enqueue(struct dp_soc *soc);
#else
static inline void dp_rx_refill_buff_pool_enqueue(struct dp_soc *soc) {}
#endif

/**
 * dp_srng_alloc() - Allocate memory for SRNG
 * @soc  : Data path soc handle
 * @srng : SRNG pointer
 * @ring_type : Ring Type
 * @num_entries: Number of entries
 * @cached: cached flag variable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_srng_alloc(struct dp_soc *soc, struct dp_srng *srng,
			 int ring_type, uint32_t num_entries,
			 bool cached);

/**
 * dp_srng_free() - Free SRNG memory
 * @soc: Data path soc handle
 * @srng: SRNG pointer
 *
 * Return: None
 */
void dp_srng_free(struct dp_soc *soc, struct dp_srng *srng);

/**
 * dp_srng_init() - Initialize SRNG
 * @soc  : Data path soc handle
 * @srng : SRNG pointer
 * @ring_type : Ring Type
 * @ring_num: Ring number
 * @mac_id: mac_id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_srng_init(struct dp_soc *soc, struct dp_srng *srng,
			int ring_type, int ring_num, int mac_id);

/**
 * dp_srng_init_idx() - Initialize SRNG
 * @soc  : Data path soc handle
 * @srng : SRNG pointer
 * @ring_type : Ring Type
 * @ring_num: Ring number
 * @mac_id: mac_id
 * @idx: ring index
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_srng_init_idx(struct dp_soc *soc, struct dp_srng *srng,
			    int ring_type, int ring_num, int mac_id,
			    uint32_t idx);

/**
 * dp_srng_deinit() - Internal function to deinit SRNG rings used by data path
 * @soc: DP SOC handle
 * @srng: source ring structure
 * @ring_type: type of ring
 * @ring_num: ring number
 *
 * Return: None
 */
void dp_srng_deinit(struct dp_soc *soc, struct dp_srng *srng,
		    int ring_type, int ring_num);

void dp_print_peer_txrx_stats_be(struct cdp_peer_stats *peer_stats,
				 enum peer_stats_type stats_type);
void dp_print_peer_txrx_stats_li(struct cdp_peer_stats *peer_stats,
				 enum peer_stats_type stats_type);

/**
 * dp_should_timer_irq_yield() - Decide if the bottom half should yield
 * @soc: DP soc handle
 * @work_done: work done in softirq context
 * @start_time: start time for the softirq
 *
 * Return: enum with yield code
 */
enum timer_yield_status
dp_should_timer_irq_yield(struct dp_soc *soc, uint32_t work_done,
			  uint64_t start_time);

/**
 * dp_vdev_get_default_reo_hash() - get reo dest ring and hash values for a vdev
 * @vdev: Datapath VDEV handle
 * @reo_dest: pointer to default reo_dest ring for vdev to be populated
 * @hash_based: pointer to hash value (enabled/disabled) to be populated
 *
 * Return: None
 */
void dp_vdev_get_default_reo_hash(struct dp_vdev *vdev,
				  enum cdp_host_reo_dest_ring *reo_dest,
				  bool *hash_based);

/**
 * dp_reo_remap_config() - configure reo remap register value based
 *                         nss configuration.
 * @soc: DP soc handle
 * @remap0: output parameter indicates reo remap 0 register value
 * @remap1: output parameter indicates reo remap 1 register value
 * @remap2: output parameter indicates reo remap 2 register value
 *
 * based on offload_radio value below remap configuration
 * get applied.
 *	0 - both Radios handled by host (remap rings 1, 2, 3 & 4)
 *	1 - 1st Radio handled by NSS (remap rings 2, 3 & 4)
 *	2 - 2nd Radio handled by NSS (remap rings 1, 2 & 4)
 *	3 - both Radios handled by NSS (remap not required)
 *	4 - IPA OFFLOAD enabled (remap rings 1,2 & 3)
 *
 * Return: bool type, true if remap is configured else false.
 */

bool dp_reo_remap_config(struct dp_soc *soc, uint32_t *remap0,
			 uint32_t *remap1, uint32_t *remap2);

#ifdef QCA_DP_TX_HW_SW_NBUF_DESC_PREFETCH
/**
 * dp_tx_comp_get_prefetched_params_from_hal_desc() - Get prefetched TX desc
 * @soc: DP soc handle
 * @tx_comp_hal_desc: HAL TX Comp Descriptor
 * @r_tx_desc: SW Tx Descriptor retrieved from HAL desc.
 *
 * Return: None
 */
void dp_tx_comp_get_prefetched_params_from_hal_desc(
					struct dp_soc *soc,
					void *tx_comp_hal_desc,
					struct dp_tx_desc_s **r_tx_desc);
#endif
#endif /* _DP_TYPES_H_ */

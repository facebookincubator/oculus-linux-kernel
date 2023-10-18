/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
#ifndef __DP_BE_H
#define __DP_BE_H

#include <dp_types.h>
#include <hal_be_tx.h>
#ifdef WLAN_MLO_MULTI_CHIP
#include "mlo/dp_mlo.h"
#else
#include <dp_peer.h>
#endif
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif

enum CMEM_MEM_CLIENTS {
	COOKIE_CONVERSION,
	FISA_FST,
};

/* maximum number of entries in one page of secondary page table */
#define DP_CC_SPT_PAGE_MAX_ENTRIES 512

/* maximum number of entries in one page of secondary page table */
#define DP_CC_SPT_PAGE_MAX_ENTRIES_MASK (DP_CC_SPT_PAGE_MAX_ENTRIES - 1)

/* maximum number of entries in primary page table */
#define DP_CC_PPT_MAX_ENTRIES \
	DP_CC_PPT_MEM_SIZE / DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED

/* cookie conversion required CMEM offset from CMEM pool */
#define DP_CC_MEM_OFFSET_IN_CMEM 0

/* cookie conversion primary page table size 4K */
#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define DP_CC_PPT_MEM_SIZE 4096
#else
#define DP_CC_PPT_MEM_SIZE 8192
#endif

/* FST required CMEM offset from CMEM pool */
#define DP_FST_MEM_OFFSET_IN_CMEM \
	(DP_CC_MEM_OFFSET_IN_CMEM + DP_CC_PPT_MEM_SIZE)

/* CMEM size for FISA FST 16K */
#define DP_CMEM_FST_SIZE 16384

/* lower 9 bits in Desc ID for offset in page of SPT */
#define DP_CC_DESC_ID_SPT_VA_OS_SHIFT 0

#define DP_CC_DESC_ID_SPT_VA_OS_MASK 0x1FF

#define DP_CC_DESC_ID_SPT_VA_OS_LSB 0

#define DP_CC_DESC_ID_SPT_VA_OS_MSB 8

/* higher 11 bits in Desc ID for offset in CMEM of PPT */
#define DP_CC_DESC_ID_PPT_PAGE_OS_LSB 9

#define DP_CC_DESC_ID_PPT_PAGE_OS_MSB 19

#define DP_CC_DESC_ID_PPT_PAGE_OS_SHIFT 9

#define DP_CC_DESC_ID_PPT_PAGE_OS_MASK 0xFFE00

/*
 * page 4K unaligned case, single SPT page physical address
 * need 8 bytes in PPT
 */
#define DP_CC_PPT_ENTRY_SIZE_4K_UNALIGNED 8
/*
 * page 4K aligned case, single SPT page physical address
 * need 4 bytes in PPT
 */
#define DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED 4

/* 4K aligned case, number of bits HW append for one PPT entry value */
#define DP_CC_PPT_ENTRY_HW_APEND_BITS_4K_ALIGNED 12

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
/* WBM2SW ring id for rx release */
#define WBM2SW_REL_ERR_RING_NUM 3
#else
/* WBM2SW ring id for rx release */
#define WBM2SW_REL_ERR_RING_NUM 5
#endif

#ifdef WLAN_SUPPORT_PPEDS
#define DP_PPEDS_STAMODE_ASTIDX_MAP_REG_IDX 1
/* The MAX PPE PRI2TID */
#define DP_TX_INT_PRI2TID_MAX 15

#define DP_TX_PPEDS_POOL_ID 0

/* size of CMEM needed for a ppeds tx desc pool */
#define DP_TX_PPEDS_DESC_POOL_CMEM_SIZE \
	((WLAN_CFG_NUM_PPEDS_TX_DESC_MAX / DP_CC_SPT_PAGE_MAX_ENTRIES) * \
	 DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED)

/* Offset of ppeds tx descripotor pool */
#define DP_TX_PPEDS_DESC_CMEM_OFFSET 0

#define PEER_ROUTING_USE_PPE 1
#define PEER_ROUTING_ENABLED 1
#define DP_PPE_INTR_STRNG_LEN 32
#define DP_PPE_INTR_MAX 3

#else
#define DP_TX_PPEDS_DESC_CMEM_OFFSET 0
#define DP_TX_PPEDS_DESC_POOL_CMEM_SIZE 0

#define DP_PPE_INTR_STRNG_LEN 0
#define DP_PPE_INTR_MAX 0
#endif

/* tx descriptor are programmed at start of CMEM region*/
#define DP_TX_DESC_CMEM_OFFSET \
	(DP_TX_PPEDS_DESC_CMEM_OFFSET + DP_TX_PPEDS_DESC_POOL_CMEM_SIZE)

/* size of CMEM needed for a tx desc pool*/
#define DP_TX_DESC_POOL_CMEM_SIZE \
	((WLAN_CFG_NUM_TX_DESC_MAX / DP_CC_SPT_PAGE_MAX_ENTRIES) * \
	 DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED)

/* Offset of rx descripotor pool */
#define DP_RX_DESC_CMEM_OFFSET \
	DP_TX_DESC_CMEM_OFFSET + (MAX_TXDESC_POOLS * DP_TX_DESC_POOL_CMEM_SIZE)

/* size of CMEM needed for a rx desc pool */
#define DP_RX_DESC_POOL_CMEM_SIZE \
	((WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX / DP_CC_SPT_PAGE_MAX_ENTRIES) * \
	 DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED)

/* get ppt_id from CMEM_OFFSET */
#define DP_CMEM_OFFSET_TO_PPT_ID(offset) \
	((offset) / DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED)

/**
 * struct dp_spt_page_desc - secondary page table page descriptors
 * @next: pointer to next linked SPT page Desc
 * @page_v_addr: page virtual address
 * @page_p_addr: page physical address
 * @ppt_index: entry index in primary page table where this page physical
		address stored
 * @avail_entry_index: index for available entry that store TX/RX Desc VA
 */
struct dp_spt_page_desc {
	uint8_t *page_v_addr;
	qdf_dma_addr_t page_p_addr;
	uint32_t ppt_index;
};

/**
 * struct dp_hw_cookie_conversion_t - main context for HW cookie conversion
 * @cmem_offset: CMEM offset from base address for primary page table setup
 * @total_page_num: total DDR page allocated
 * @page_desc_freelist: available page Desc list
 * @page_desc_base: page Desc buffer base address.
 * @page_pool: DDR pages pool
 * @cc_lock: locks for page acquiring/free
 */
struct dp_hw_cookie_conversion_t {
	uint32_t cmem_offset;
	uint32_t total_page_num;
	struct dp_spt_page_desc *page_desc_base;
	struct qdf_mem_multi_page_t page_pool;
	qdf_spinlock_t cc_lock;
};

/**
 * struct dp_spt_page_desc_list - containor of SPT page desc list info
 * @spt_page_list_head: head of SPT page descriptor list
 * @spt_page_list_tail: tail of SPT page descriptor list
 * @num_spt_pages: number of SPT page descriptor allocated
 */
struct dp_spt_page_desc_list {
	struct dp_spt_page_desc *spt_page_list_head;
	struct dp_spt_page_desc *spt_page_list_tail;
	uint16_t num_spt_pages;
};

/* HW reading 8 bytes for VA */
#define DP_CC_HW_READ_BYTES 8
#define DP_CC_SPT_PAGE_UPDATE_VA(_page_base_va, _index, _desc_va) \
	{ *((uintptr_t *)((_page_base_va) + (_index) * DP_CC_HW_READ_BYTES)) \
	= (uintptr_t)(_desc_va); }

/**
 * struct dp_tx_bank_profile - DP wrapper for TCL banks
 * @is_configured: flag indicating if this bank is configured
 * @ref_count: ref count indicating number of users of the bank
 * @bank_config: HAL TX bank configuration
 */
struct dp_tx_bank_profile {
	uint8_t is_configured;
	qdf_atomic_t  ref_count;
	union hal_tx_bank_config bank_config;
};

#ifdef WLAN_SUPPORT_PPEDS
/**
 * struct dp_ppe_vp_tbl_entry - PPE Virtual table entry
 * @is_configured: Boolean that the entry is configured.
 */
struct dp_ppe_vp_tbl_entry {
	bool is_configured;
};

/**
 * struct dp_ppe_vp_search_idx_tbl_entry - PPE Virtual search table entry
 * @is_configured: Boolean that the entry is configured.
 */
struct dp_ppe_vp_search_idx_tbl_entry {
	bool is_configured;
};

/**
 * struct dp_ppe_vp_profile - PPE direct switch profiler per vdev
 * @vp_num: Virtual port number
 * @ppe_vp_num_idx: Index to the PPE VP table entry
 * @search_idx_reg_num: Address search Index register number
 * @drop_prec_enable: Drop precedance enable
 * @to_fw: To FW exception enable/disable.
 * @use_ppe_int_pri: Use PPE INT_PRI to TID mapping table
 */
struct dp_ppe_vp_profile {
	uint8_t vp_num;
	uint8_t ppe_vp_num_idx;
	uint8_t search_idx_reg_num;
	uint8_t drop_prec_enable;
	uint8_t to_fw;
	uint8_t use_ppe_int_pri;
};

/**
 * struct dp_ppeds_tx_desc_pool_s - PPEDS Tx Descriptor Pool
 * @elem_size: Size of each descriptor
 * @num_allocated: Number of used descriptors
 * @freelist: Chain of free descriptors
 * @desc_pages: multiple page allocation information for actual descriptors
 * @elem_count: Number of descriptors in the pool
 * @num_free: Number of free descriptors
 * @lock- Lock for descriptor allocation/free from/to the pool
 */
struct dp_ppeds_tx_desc_pool_s {
	uint16_t elem_size;
	uint32_t num_allocated;
	struct dp_tx_desc_s *freelist;
	struct qdf_mem_multi_page_t desc_pages;
	uint16_t elem_count;
	uint32_t num_free;
	qdf_spinlock_t lock;
};
#endif

/**
 * struct dp_ppeds_napi - napi parameters for ppe ds
 * @napi: napi structure to register with napi infra
 * @ndev: net_dev structure
 */
struct dp_ppeds_napi {
	struct napi_struct napi;
	struct net_device ndev;
};

/**
 * struct dp_soc_be - Extended DP soc for BE targets
 * @soc: dp soc structure
 * @num_bank_profiles: num TX bank profiles
 * @bank_profiles: bank profiles for various TX banks
 * @cc_cmem_base: cmem offset reserved for CC
 * @tx_cc_ctx: Cookie conversion context for tx desc pools
 * @rx_cc_ctx: Cookie conversion context for rx desc pools
 * @monitor_soc_be: BE specific monitor object
 * @mlo_enabled: Flag to indicate MLO is enabled or not
 * @mlo_chip_id: MLO chip_id
 * @ml_ctxt: pointer to global ml_context
 * @delta_tqm: delta_tqm
 * @mlo_tstamp_offset: mlo timestamp offset
 * @mld_peer_hash: peer hash table for ML peers
 *           Associated peer with this MAC address)
 * @mld_peer_hash_lock: lock to protect mld_peer_hash
 * @ppe_ds_int_mode_enabled: PPE DS interrupt mode enabled
 * @reo2ppe_ring: REO2PPE ring
 * @ppe2tcl_ring: PPE2TCL ring
 * @ppe_vp_tbl: PPE VP table
 * @ppe_vp_search_idx_tbl: PPE VP search idx table
 * @ppe_vp_tbl_lock: PPE VP table lock
 * @num_ppe_vp_entries : Number of PPE VP entries
 * @ipa_bank_id: TCL bank id used by IPA
 * @ppeds_tx_cc_ctx: Cookie conversion context for ppeds tx desc pool
 * @ppeds_tx_desc: PPEDS tx desc pool
 * @ppeds_handle: PPEDS soc instance handle
 * @ppe_vp_tbl_lock: PPEDS VP table lock
 * @num_ppe_vp_entries: PPEDS number of VP entries
 * @num_ppe_vp_search_idx_entries: PPEDS VP search idx entries
 * @irq_name: PPEDS VP irq names
 */
struct dp_soc_be {
	struct dp_soc soc;
	uint8_t num_bank_profiles;
#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
	qdf_mutex_t tx_bank_lock;
#else
	qdf_spinlock_t tx_bank_lock;
#endif
	struct dp_tx_bank_profile *bank_profiles;
	struct dp_spt_page_desc *page_desc_base;
	uint32_t cc_cmem_base;
	struct dp_hw_cookie_conversion_t tx_cc_ctx[MAX_TXDESC_POOLS];
	struct dp_hw_cookie_conversion_t rx_cc_ctx[MAX_RXDESC_POOLS];
#ifdef WLAN_SUPPORT_PPEDS
	uint8_t ppeds_int_mode_enabled:1,
		ppeds_stopped:1;
	struct dp_srng reo2ppe_ring;
	struct dp_srng ppe2tcl_ring;
	struct dp_srng ppeds_wbm_release_ring;
	struct dp_ppe_vp_tbl_entry *ppe_vp_tbl;
	struct dp_ppe_vp_search_idx_tbl_entry *ppe_vp_search_idx_tbl;
	struct dp_hw_cookie_conversion_t ppeds_tx_cc_ctx;
	struct dp_ppeds_tx_desc_pool_s ppeds_tx_desc;
	struct dp_ppeds_napi ppeds_napi_ctxt;
	void *ppeds_handle;
	qdf_mutex_t ppe_vp_tbl_lock;
	uint8_t num_ppe_vp_entries;
	uint8_t num_ppe_vp_search_idx_entries;
	char irq_name[DP_PPE_INTR_MAX][DP_PPE_INTR_STRNG_LEN];
#endif
#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_MLO_MULTI_CHIP
	uint8_t mlo_enabled;
	uint8_t mlo_chip_id;
	struct dp_mlo_ctxt *ml_ctxt;
	uint64_t delta_tqm;
	uint64_t mlo_tstamp_offset;
#else
	/* Protect mld peer hash table */
	DP_MUTEX_TYPE mld_peer_hash_lock;
	struct {
		uint32_t mask;
		uint32_t idx_bits;

		TAILQ_HEAD(, dp_peer) * bins;
	} mld_peer_hash;
#endif
#endif
#ifdef IPA_OFFLOAD
	int8_t ipa_bank_id;
#endif
};

/* convert struct dp_soc_be pointer to struct dp_soc pointer */
#define DP_SOC_BE_GET_SOC(be_soc) ((struct dp_soc *)be_soc)

/**
 * struct dp_pdev_be - Extended DP pdev for BE targets
 * @pdev: dp pdev structure
 * @monitor_pdev_be: BE specific monitor object
 * @mlo_link_id: MLO link id for PDEV
 * @delta_tsf2: delta_tsf2
 */
struct dp_pdev_be {
	struct dp_pdev pdev;
#ifdef WLAN_MLO_MULTI_CHIP
	uint8_t mlo_link_id;
	uint64_t delta_tsf2;
#endif
};

/**
 * struct dp_vdev_be - Extended DP vdev for BE targets
 * @vdev: dp vdev structure
 * @bank_id: bank_id to be used for TX
 * @vdev_id_check_en: flag if HW vdev_id check is enabled for vdev
 * @ppe_vp_enabled: flag to check if PPE VP is enabled for vdev
 * @ppe_vp_profile: PPE VP profile
 */
struct dp_vdev_be {
	struct dp_vdev vdev;
	int8_t bank_id;
	uint8_t vdev_id_check_en;
#ifdef WLAN_MLO_MULTI_CHIP
	/* partner list used for Intra-BSS */
	uint8_t partner_vdev_list[WLAN_MAX_MLO_CHIPS][WLAN_MAX_MLO_LINKS_PER_SOC];
#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_MCAST_MLO
	/* DP MLO seq number */
	uint16_t seq_num;
	/* MLO Mcast primary vdev */
	bool mcast_primary;
#endif
#endif
#endif
	unsigned long ppe_vp_enabled;
#ifdef WLAN_SUPPORT_PPEDS
	struct dp_ppe_vp_profile ppe_vp_profile;
#endif
};

/**
 * struct dp_peer_be - Extended DP peer for BE targets
 * @dp_peer: dp peer structure
 */
struct dp_peer_be {
	struct dp_peer peer;
#ifdef WLAN_SUPPORT_PPEDS
	uint8_t priority_valid;
#endif
};

/**
 * dp_get_soc_context_size_be() - get context size for target specific DP soc
 *
 * Return: value in bytes for BE specific soc structure
 */
qdf_size_t dp_get_soc_context_size_be(void);

/**
 * dp_initialize_arch_ops_be() - initialize BE specific arch ops
 * @arch_ops: arch ops pointer
 *
 * Return: none
 */
void dp_initialize_arch_ops_be(struct dp_arch_ops *arch_ops);

/**
 * dp_get_context_size_be() - get BE specific size for peer/vdev/pdev/soc
 * @arch_ops: arch ops pointer
 *
 * Return: size in bytes for the context_type
 */
qdf_size_t dp_get_context_size_be(enum dp_context_type context_type);

/**
 * dp_get_be_soc_from_dp_soc() - get dp_soc_be from dp_soc
 * @soc: dp_soc pointer
 *
 * Return: dp_soc_be pointer
 */
static inline struct dp_soc_be *dp_get_be_soc_from_dp_soc(struct dp_soc *soc)
{
	return (struct dp_soc_be *)soc;
}

#ifdef WLAN_MLO_MULTI_CHIP
typedef struct dp_mlo_ctxt *dp_mld_peer_hash_obj_t;

/*
 * dp_mlo_get_peer_hash_obj() - return the container struct of MLO hash table
 *
 * @soc: soc handle
 *
 * return: MLD peer hash object
 */
static inline dp_mld_peer_hash_obj_t
dp_mlo_get_peer_hash_obj(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	return be_soc->ml_ctxt;
}

void  dp_clr_mlo_ptnr_list(struct dp_soc *soc, struct dp_vdev *vdev);

#if defined(WLAN_FEATURE_11BE_MLO)
/**
 * dp_mlo_partner_chips_map() - Map MLO peers to partner SOCs
 * @soc: Soc handle
 * @peer: DP peer handle for ML peer
 * @peer_id: peer_id
 * Return: None
 */
void dp_mlo_partner_chips_map(struct dp_soc *soc,
			      struct dp_peer *peer,
			      uint16_t peer_id);

/**
 * dp_mlo_partner_chips_unmap() - Unmap MLO peers to partner SOCs
 * @soc: Soc handle
 * @peer_id: peer_id
 * Return: None
 */
void dp_mlo_partner_chips_unmap(struct dp_soc *soc,
				uint16_t peer_id);

#ifdef WLAN_MCAST_MLO
typedef void dp_ptnr_vdev_iter_func(struct dp_vdev_be *be_vdev,
				    struct dp_vdev *ptnr_vdev,
				    void *arg);
typedef void dp_ptnr_soc_iter_func(struct dp_soc *ptnr_soc,
				   void *arg);
/*
 * dp_mcast_mlo_iter_ptnr_vdev - API to iterate through ptnr vdev list
 * @be_soc: dp_soc_be pointer
 * @be_vdev: dp_vdev_be pointer
 * @func        : function to be called for each peer
 * @arg         : argument need to be passed to func
 * @mod_id: module id
 *
 * Return: None
 */
void dp_mcast_mlo_iter_ptnr_vdev(struct dp_soc_be *be_soc,
				 struct dp_vdev_be *be_vdev,
				 dp_ptnr_vdev_iter_func func,
				 void *arg,
				 enum dp_mod_id mod_id);
/*
 * dp_mcast_mlo_iter_ptnr_soc - API to iterate through ptnr soc list
 * @be_soc: dp_soc_be pointer
 * @func        : function to be called for each peer
 * @arg         : argument need to be passed to func
 *
 * Return: None
 */
void dp_mcast_mlo_iter_ptnr_soc(struct dp_soc_be *be_soc,
				dp_ptnr_soc_iter_func func,
				void *arg);
/*
 * dp_mlo_get_mcast_primary_vdev- get ref to mcast primary vdev
 * @be_soc: dp_soc_be pointer
 * @be_vdev: dp_vdev_be pointer
 * @mod_id: module id
 *
 * Return: mcast primary DP VDEV handle on success, NULL on failure
 */
struct dp_vdev *dp_mlo_get_mcast_primary_vdev(struct dp_soc_be *be_soc,
					      struct dp_vdev_be *be_vdev,
					      enum dp_mod_id mod_id);
#endif
#endif

#else
typedef struct dp_soc_be *dp_mld_peer_hash_obj_t;

static inline dp_mld_peer_hash_obj_t
dp_mlo_get_peer_hash_obj(struct dp_soc *soc)
{
	return dp_get_be_soc_from_dp_soc(soc);
}

static inline void  dp_clr_mlo_ptnr_list(struct dp_soc *soc,
					 struct dp_vdev *vdev)
{
}
#endif

/*
 * dp_mlo_peer_find_hash_attach_be() - API to initialize ML peer hash table
 *
 * @mld_hash_obj: Peer has object
 * @hash_elems: number of entries in hash table
 *
 * return: QDF_STATUS_SUCCESS when attach is success else QDF_STATUS_FAILURE
 */
QDF_STATUS
dp_mlo_peer_find_hash_attach_be(dp_mld_peer_hash_obj_t mld_hash_obj,
				int hash_elems);

/*
 * dp_mlo_peer_find_hash_detach_be() - API to de-initialize ML peer hash table
 *
 * @mld_hash_obj: Peer has object
 *
 * return: void
 */
void dp_mlo_peer_find_hash_detach_be(dp_mld_peer_hash_obj_t mld_hash_obj);

/**
 * dp_get_be_pdev_from_dp_pdev() - get dp_pdev_be from dp_pdev
 * @pdev: dp_pdev pointer
 *
 * Return: dp_pdev_be pointer
 */
static inline
struct dp_pdev_be *dp_get_be_pdev_from_dp_pdev(struct dp_pdev *pdev)
{
	return (struct dp_pdev_be *)pdev;
}

/**
 * dp_get_be_vdev_from_dp_vdev() - get dp_vdev_be from dp_vdev
 * @vdev: dp_vdev pointer
 *
 * Return: dp_vdev_be pointer
 */
static inline
struct dp_vdev_be *dp_get_be_vdev_from_dp_vdev(struct dp_vdev *vdev)
{
	return (struct dp_vdev_be *)vdev;
}

/**
 * dp_get_be_peer_from_dp_peer() - get dp_peer_be from dp_peer
 * @peer: dp_peer pointer
 *
 * Return: dp_peer_be pointer
 */
static inline
struct dp_peer_be *dp_get_be_peer_from_dp_peer(struct dp_peer *peer)
{
	return (struct dp_peer_be *)peer;
}

void dp_ppeds_disable_irq(struct dp_soc *soc, struct dp_srng *srng);
void dp_ppeds_enable_irq(struct dp_soc *soc, struct dp_srng *srng);

QDF_STATUS
dp_hw_cookie_conversion_attach(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx,
			       uint32_t num_descs,
			       enum dp_desc_type desc_type,
			       uint8_t desc_pool_id);

void dp_reo_shared_qaddr_detach(struct dp_soc *soc);

QDF_STATUS
dp_hw_cookie_conversion_detach(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx);
QDF_STATUS
dp_hw_cookie_conversion_init(struct dp_soc_be *be_soc,
			     struct dp_hw_cookie_conversion_t *cc_ctx);
QDF_STATUS
dp_hw_cookie_conversion_deinit(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx);
/**
 * dp_cc_spt_page_desc_alloc() - allocate SPT DDR page descriptor from pool
 * @be_soc: beryllium soc handler
 * @list_head: pointer to page desc head
 * @list_tail: pointer to page desc tail
 * @num_desc: number of TX/RX Descs required for SPT pages
 *
 * Return: number of SPT page Desc allocated
 */
uint16_t dp_cc_spt_page_desc_alloc(struct dp_soc_be *be_soc,
				   struct dp_spt_page_desc **list_head,
				   struct dp_spt_page_desc **list_tail,
				   uint16_t num_desc);
/**
 * dp_cc_spt_page_desc_free() - free SPT DDR page descriptor to pool
 * @be_soc: beryllium soc handler
 * @list_head: pointer to page desc head
 * @list_tail: pointer to page desc tail
 * @page_nums: number of page desc freed back to pool
 */
void dp_cc_spt_page_desc_free(struct dp_soc_be *be_soc,
			      struct dp_spt_page_desc **list_head,
			      struct dp_spt_page_desc **list_tail,
			      uint16_t page_nums);

/**
 * dp_cc_desc_id_generate() - generate SW cookie ID according to
				DDR page 4K aligned or not
 * @ppt_index: offset index in primary page table
 * @spt_index: offset index in sceondary DDR page
 *
 * Generate SW cookie ID to match as HW expected
 *
 * Return: cookie ID
 */
static inline uint32_t dp_cc_desc_id_generate(uint32_t ppt_index,
					      uint16_t spt_index)
{
	/*
	 * for 4k aligned case, cmem entry size is 4 bytes,
	 * HW index from bit19~bit10 value = ppt_index / 2, high 32bits flag
	 * from bit9 value = ppt_index % 2, then bit 19 ~ bit9 value is
	 * exactly same with original ppt_index value.
	 * for 4k un-aligned case, cmem entry size is 8 bytes.
	 * bit19 ~ bit9 will be HW index value, same as ppt_index value.
	 */
	return ((((uint32_t)ppt_index) << DP_CC_DESC_ID_PPT_PAGE_OS_SHIFT) |
		spt_index);
}

/**
 * dp_cc_desc_va_find() - find TX/RX Descs virtual address by ID
 * @be_soc: be soc handle
 * @desc_id: TX/RX Dess ID
 *
 * Return: TX/RX Desc virtual address
 */
static inline uintptr_t dp_cc_desc_find(struct dp_soc *soc,
					uint32_t desc_id)
{
	struct dp_soc_be *be_soc;
	uint16_t ppt_page_id, spt_va_id;
	uint8_t *spt_page_va;

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	ppt_page_id = (desc_id & DP_CC_DESC_ID_PPT_PAGE_OS_MASK) >>
			DP_CC_DESC_ID_PPT_PAGE_OS_SHIFT;

	spt_va_id = (desc_id & DP_CC_DESC_ID_SPT_VA_OS_MASK) >>
			DP_CC_DESC_ID_SPT_VA_OS_SHIFT;

	/*
	 * ppt index in cmem is same order where the page in the
	 * page desc array during initialization.
	 * entry size in DDR page is 64 bits, for 32 bits system,
	 * only lower 32 bits VA value is needed.
	 */
	spt_page_va = be_soc->page_desc_base[ppt_page_id].page_v_addr;

	return (*((uintptr_t *)(spt_page_va  +
				spt_va_id * DP_CC_HW_READ_BYTES)));
}

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * enum dp_srng_near_full_levels - SRNG Near FULL levels
 * @DP_SRNG_THRESH_SAFE: SRNG level safe for yielding the near full mode
 *		of processing the entries in SRNG
 * @DP_SRNG_THRESH_NEAR_FULL: SRNG level enters the near full mode
 *		of processing the entries in SRNG
 * @DP_SRNG_THRESH_CRITICAL: SRNG level enters the critical level of full
 *		condition and drastic steps need to be taken for processing
 *		the entries in SRNG
 */
enum dp_srng_near_full_levels {
	DP_SRNG_THRESH_SAFE,
	DP_SRNG_THRESH_NEAR_FULL,
	DP_SRNG_THRESH_CRITICAL,
};

/**
 * dp_srng_check_ring_near_full() - Check if SRNG is marked as near-full from
 *				its corresponding near-full irq handler
 * @soc: Datapath SoC handle
 * @dp_srng: datapath handle for this SRNG
 *
 * Return: 1, if the srng was marked as near-full
 *	   0, if the srng was not marked as near-full
 */
static inline int dp_srng_check_ring_near_full(struct dp_soc *soc,
					       struct dp_srng *dp_srng)
{
	return qdf_atomic_read(&dp_srng->near_full);
}

/**
 * dp_srng_get_near_full_level() - Check the num available entries in the
 *			consumer srng and return the level of the srng
 *			near full state.
 * @soc: Datapath SoC Handle [To be validated by the caller]
 * @hal_ring_hdl: SRNG handle
 *
 * Return: near-full level
 */
static inline int
dp_srng_get_near_full_level(struct dp_soc *soc, struct dp_srng *dp_srng)
{
	uint32_t num_valid;

	num_valid = hal_srng_dst_num_valid_nolock(soc->hal_soc,
						  dp_srng->hal_srng,
						  true);

	if (num_valid > dp_srng->crit_thresh)
		return DP_SRNG_THRESH_CRITICAL;
	else if (num_valid < dp_srng->safe_thresh)
		return DP_SRNG_THRESH_SAFE;
	else
		return DP_SRNG_THRESH_NEAR_FULL;
}

#define DP_SRNG_PER_LOOP_NF_REAP_MULTIPLIER	2

/**
 * dp_srng_test_and_update_nf_params() - Test the near full level and update
 *			the reap_limit and flags to reflect the state.
 * @soc: Datapath soc handle
 * @srng: Datapath handle for the srng
 * @max_reap_limit: [Output Param] Buffer to set the map_reap_limit as
 *			per the near-full state
 *
 * Return: 1, if the srng is near full
 *	   0, if the srng is not near full
 */
static inline int
_dp_srng_test_and_update_nf_params(struct dp_soc *soc,
				   struct dp_srng *srng,
				   int *max_reap_limit)
{
	int ring_near_full = 0, near_full_level;

	if (dp_srng_check_ring_near_full(soc, srng)) {
		near_full_level = dp_srng_get_near_full_level(soc, srng);
		switch (near_full_level) {
		case DP_SRNG_THRESH_CRITICAL:
			/* Currently not doing anything special here */
			fallthrough;
		case DP_SRNG_THRESH_NEAR_FULL:
			ring_near_full = 1;
			*max_reap_limit *= DP_SRNG_PER_LOOP_NF_REAP_MULTIPLIER;
			break;
		case DP_SRNG_THRESH_SAFE:
			qdf_atomic_set(&srng->near_full, 0);
			ring_near_full = 0;
			break;
		default:
			qdf_assert(0);
			break;
		}
	}

	return ring_near_full;
}
#else
static inline int
_dp_srng_test_and_update_nf_params(struct dp_soc *soc,
				   struct dp_srng *srng,
				   int *max_reap_limit)
{
	return 0;
}
#endif

static inline
uint32_t dp_desc_pool_get_cmem_base(uint8_t chip_id, uint8_t desc_pool_id,
				    enum dp_desc_type desc_type)
{
	switch (desc_type) {
	case DP_TX_DESC_TYPE:
		return (DP_TX_DESC_CMEM_OFFSET +
			(desc_pool_id * DP_TX_DESC_POOL_CMEM_SIZE));
	case DP_RX_DESC_BUF_TYPE:
		return (DP_RX_DESC_CMEM_OFFSET +
			((chip_id * MAX_RXDESC_POOLS) + desc_pool_id) *
			DP_RX_DESC_POOL_CMEM_SIZE);
	case DP_TX_PPEDS_DESC_TYPE:
		return DP_TX_PPEDS_DESC_CMEM_OFFSET;
	default:
			QDF_BUG(0);
	}
	return 0;
}

#ifndef WLAN_MLO_MULTI_CHIP
static inline
void dp_soc_mlo_fill_params(struct dp_soc *soc,
			    struct cdp_soc_attach_params *params)
{
}

static inline
void dp_pdev_mlo_fill_params(struct dp_pdev *pdev,
			     struct cdp_pdev_attach_params *params)
{
}

static inline
void dp_mlo_update_link_to_pdev_map(struct dp_soc *soc, struct dp_pdev *pdev)
{
}

static inline
void dp_mlo_update_link_to_pdev_unmap(struct dp_soc *soc, struct dp_pdev *pdev)
{
}
#endif
#endif

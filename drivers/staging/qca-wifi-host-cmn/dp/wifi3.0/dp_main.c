/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <wlan_ipa_obj_mgmt_api.h>
#include <qdf_types.h>
#include <qdf_lock.h>
#include <qdf_net_types.h>
#include <qdf_lro.h>
#include <qdf_module.h>
#include <hal_hw_headers.h>
#include <hal_api.h>
#include <hif.h>
#include <htt.h>
#include <wdi_event.h>
#include <queue.h>
#include "dp_types.h"
#include "dp_rings.h"
#include "dp_internal.h"
#include "dp_tx.h"
#include "dp_tx_desc.h"
#include "dp_rx.h"
#ifdef DP_RATETABLE_SUPPORT
#include "dp_ratetable.h"
#endif
#include <cdp_txrx_handle.h>
#include <wlan_cfg.h>
#include <wlan_utility.h>
#include "cdp_txrx_cmn_struct.h"
#include "cdp_txrx_stats_struct.h"
#include "cdp_txrx_cmn_reg.h"
#include <qdf_util.h>
#include "dp_peer.h"
#include "htt_stats.h"
#include "dp_htt.h"
#ifdef WLAN_SUPPORT_RX_FISA
#include <wlan_dp_fisa_rx.h>
#endif
#include "htt_ppdu_stats.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "cfg_ucfg_api.h"
#include <wlan_module_ids.h>
#ifdef QCA_MULTIPASS_SUPPORT
#include <enet.h>
#endif
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
#include "cdp_txrx_flow_ctrl_v2.h"
#else

static inline void
cdp_dump_flow_pool_info(struct cdp_soc_t *soc)
{
	return;
}
#endif
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#include "dp_ipa.h"
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#ifdef WLAN_SUPPORT_MSCS
#include "dp_mscs.h"
#endif
#ifdef WLAN_SUPPORT_MESH_LATENCY
#include "dp_mesh_latency.h"
#endif
#ifdef WLAN_SUPPORT_SCS
#include "dp_scs.h"
#endif
#ifdef ATH_SUPPORT_IQUE
#include "dp_txrx_me.h"
#endif
#if defined(DP_CON_MON)
#ifndef REMOVE_PKT_LOG
#include <pktlog_ac_api.h>
#include <pktlog_ac.h>
#endif
#endif
#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
#include <wlan_dp_swlm.h>
#endif
#ifdef WLAN_DP_PROFILE_SUPPORT
#include <wlan_dp_main.h>
#endif
#ifdef CONFIG_SAWF_DEF_QUEUES
#include "dp_sawf.h"
#endif
#ifdef WLAN_SUPPORT_RX_FLOW_TAG
#include "dp_rx_tag.h"
#endif
#ifdef WLAN_FEATURE_PEER_TXQ_FLUSH_CONF
#include <target_if_dp.h>
#endif
#include "qdf_ssr_driver_dump.h"
#ifdef WLAN_SUPPORT_LAPB
#include <wlan_dp_lapb_flow.h>
#endif

#ifdef WLAN_SUPPORT_DPDK
#include <dp_dpdk.h>
#endif

#ifdef QCA_DP_ENABLE_TX_COMP_RING4
#define TXCOMP_RING4_NUM 3
#else
#define TXCOMP_RING4_NUM WBM2SW_TXCOMP_RING4_NUM
#endif

#if defined(DP_PEER_EXTENDED_API) || defined(WLAN_DP_PENDING_MEM_FLUSH)
#define SET_PEER_REF_CNT_ONE(_peer) \
	qdf_atomic_set(&(_peer)->ref_cnt, 1)
#else
#define SET_PEER_REF_CNT_ONE(_peer)
#endif

#ifdef WLAN_SYSFS_DP_STATS
/* sysfs event wait time for firmware stat request unit milliseconds */
#define WLAN_SYSFS_STAT_REQ_WAIT_MS 3000
#endif

#ifdef QCA_DP_TX_FW_METADATA_V2
#define DP_TX_TCL_METADATA_PDEV_ID_SET(_var, _val) \
		HTT_TX_TCL_METADATA_V2_PDEV_ID_SET(_var, _val)
#else
#define DP_TX_TCL_METADATA_PDEV_ID_SET(_var, _val) \
		HTT_TX_TCL_METADATA_PDEV_ID_SET(_var, _val)
#endif
#define MLD_MODE_INVALID 0xFF

QDF_COMPILE_TIME_ASSERT(max_rx_rings_check,
			MAX_REO_DEST_RINGS == CDP_MAX_RX_RINGS);

QDF_COMPILE_TIME_ASSERT(max_tx_rings_check,
			MAX_TCL_DATA_RINGS == CDP_MAX_TX_COMP_RINGS);

void dp_configure_arch_ops(struct dp_soc *soc);
qdf_size_t dp_get_soc_context_size(uint16_t device_id);

/*
 * The max size of cdp_peer_stats_param_t is limited to 16 bytes.
 * If the buffer size is exceeding this size limit,
 * dp_txrx_get_peer_stats is to be used instead.
 */
QDF_COMPILE_TIME_ASSERT(cdp_peer_stats_param_t_max_size,
			(sizeof(cdp_peer_stats_param_t) <= 16));

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
/*
 * If WLAN_CFG_INT_NUM_CONTEXTS is changed, HIF_NUM_INT_CONTEXTS
 * also should be updated accordingly
 */
QDF_COMPILE_TIME_ASSERT(num_intr_grps,
			HIF_NUM_INT_CONTEXTS == WLAN_CFG_INT_NUM_CONTEXTS);

/*
 * HIF_EVENT_HIST_MAX should always be power of 2
 */
QDF_COMPILE_TIME_ASSERT(hif_event_history_size,
			(HIF_EVENT_HIST_MAX & (HIF_EVENT_HIST_MAX - 1)) == 0);
#endif /* WLAN_FEATURE_DP_EVENT_HISTORY */

/*
 * If WLAN_CFG_INT_NUM_CONTEXTS is changed,
 * WLAN_CFG_INT_NUM_CONTEXTS_MAX should also be updated
 */
QDF_COMPILE_TIME_ASSERT(wlan_cfg_num_int_ctxs,
			WLAN_CFG_INT_NUM_CONTEXTS_MAX >=
			WLAN_CFG_INT_NUM_CONTEXTS);

static void dp_soc_unset_qref_debug_list(struct dp_soc *soc);
static QDF_STATUS dp_sysfs_deinitialize_stats(struct dp_soc *soc_hdl);
static QDF_STATUS dp_sysfs_initialize_stats(struct dp_soc *soc_hdl);

static void dp_pdev_srng_deinit(struct dp_pdev *pdev);
static QDF_STATUS dp_pdev_srng_init(struct dp_pdev *pdev);
static void dp_pdev_srng_free(struct dp_pdev *pdev);
static QDF_STATUS dp_pdev_srng_alloc(struct dp_pdev *pdev);

static inline
QDF_STATUS dp_pdev_attach_wifi3(struct cdp_soc_t *txrx_soc,
				struct cdp_pdev_attach_params *params);

static int dp_pdev_post_attach_wifi3(struct cdp_soc_t *psoc, uint8_t pdev_id);

static QDF_STATUS
dp_pdev_init_wifi3(struct cdp_soc_t *txrx_soc,
		   HTC_HANDLE htc_handle,
		   qdf_device_t qdf_osdev,
		   uint8_t pdev_id);

static QDF_STATUS
dp_pdev_deinit_wifi3(struct cdp_soc_t *psoc, uint8_t pdev_id, int force);

static void dp_soc_detach_wifi3(struct cdp_soc_t *txrx_soc);
static void dp_soc_deinit_wifi3(struct cdp_soc_t *txrx_soc);

static void dp_pdev_detach(struct cdp_pdev *txrx_pdev, int force);
static QDF_STATUS dp_pdev_detach_wifi3(struct cdp_soc_t *psoc,
				       uint8_t pdev_id,
				       int force);
static struct dp_soc *
dp_soc_attach(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
	      struct cdp_soc_attach_params *params);
static inline QDF_STATUS dp_peer_create_wifi3(struct cdp_soc_t *soc_hdl,
					      uint8_t vdev_id,
					      uint8_t *peer_mac_addr,
					      enum cdp_peer_type peer_type);
static QDF_STATUS dp_peer_delete_wifi3(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *peer_mac, uint32_t bitmap,
				       enum cdp_peer_type peer_type);
static void dp_vdev_flush_peers(struct cdp_vdev *vdev_handle,
				bool unmap_only,
				bool mlo_peers_only);
#ifdef ENABLE_VERBOSE_DEBUG
bool is_dp_verbose_debug_enabled;
#endif

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
static bool dp_get_cfr_rcc(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
static void dp_set_cfr_rcc(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			   bool enable);
static inline void
dp_get_cfr_dbg_stats(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		     struct cdp_cfr_rcc_stats *cfr_rcc_stats);
static inline void
dp_clear_cfr_dbg_stats(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
static QDF_STATUS dp_umac_reset_action_trigger_recovery(struct dp_soc *soc);
static QDF_STATUS dp_umac_reset_handle_pre_reset(struct dp_soc *soc);
static QDF_STATUS dp_umac_reset_handle_post_reset(struct dp_soc *soc);
static QDF_STATUS dp_umac_reset_handle_post_reset_complete(struct dp_soc *soc);
#endif

#define MON_VDEV_TIMER_INIT 0x1
#define MON_VDEV_TIMER_RUNNING 0x2

#define DP_MCS_LENGTH (6*MAX_MCS)

#define DP_CURR_FW_STATS_AVAIL 19
#define DP_HTT_DBG_EXT_STATS_MAX 256
#define DP_MAX_SLEEP_TIME 100
#ifndef QCA_WIFI_3_0_EMU
#define SUSPEND_DRAIN_WAIT 500
#else
#define SUSPEND_DRAIN_WAIT 3000
#endif

#ifdef IPA_OFFLOAD
/* Exclude IPA rings from the interrupt context */
#define TX_RING_MASK_VAL	0xb
#define RX_RING_MASK_VAL	0x7
#else
#define TX_RING_MASK_VAL	0xF
#define RX_RING_MASK_VAL	0xF
#endif

#define STR_MAXLEN	64

#define RNG_ERR		"SRNG setup failed for"

/**
 * enum dp_stats_type - Select the type of statistics
 * @STATS_FW: Firmware-based statistic
 * @STATS_HOST: Host-based statistic
 * @STATS_TYPE_MAX: maximum enumeration
 */
enum dp_stats_type {
	STATS_FW = 0,
	STATS_HOST = 1,
	STATS_TYPE_MAX = 2,
};

/**
 * enum dp_fw_stats - General Firmware statistics options
 * @TXRX_FW_STATS_INVALID: statistic is not available
 */
enum dp_fw_stats {
	TXRX_FW_STATS_INVALID	= -1,
};

/*
 * dp_stats_mapping_table - Firmware and Host statistics
 * currently supported
 */
#ifndef WLAN_SOFTUMAC_SUPPORT
const int dp_stats_mapping_table[][STATS_TYPE_MAX] = {
	{HTT_DBG_EXT_STATS_RESET, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_RX, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_HWQ, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_SCHED, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_ERROR, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TQM, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TQM_CMDQ, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_DE_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_RATE, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_RX_RATE, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_SELFGEN_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_MU_HWQ, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_RING_IF_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_SRNG_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_SFM_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_MU, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_ACTIVE_PEERS_LIST, TXRX_HOST_STATS_INVALID},
	/* Last ENUM for HTT FW STATS */
	{DP_HTT_DBG_EXT_STATS_MAX, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_CLEAR_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_RX_RATE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_TX_RATE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_TX_HOST_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_RX_HOST_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_AST_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SRNG_PTR_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_RX_MON_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_REO_QUEUE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_CFG_PARAMS},
	{TXRX_FW_STATS_INVALID, TXRX_PDEV_CFG_PARAMS},
	{TXRX_FW_STATS_INVALID, TXRX_NAPI_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_INTERRUPT_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_FSE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_HAL_REG_WRITE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_REO_HW_DESC_DUMP},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_WBM_IDLE_HPTP_DUMP},
	{TXRX_FW_STATS_INVALID, TXRX_SRNG_USAGE_WM_STATS},
	{HTT_DBG_EXT_STATS_PDEV_RX_RATE_EXT, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_SOUNDING_INFO, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_PEER_STATS},
};
#else
const int dp_stats_mapping_table[][STATS_TYPE_MAX] = {
	{HTT_DBG_EXT_STATS_RESET, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_RX, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_HWQ, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_SCHED, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_ERROR, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TQM, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TQM_CMDQ, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_DE_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_RATE, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_RX_RATE, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_SELFGEN_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_MU_HWQ, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_RING_IF_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_SRNG_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_SFM_INFO, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_TX_MU, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_ACTIVE_PEERS_LIST, TXRX_HOST_STATS_INVALID},
	/* Last ENUM for HTT FW STATS */
	{DP_HTT_DBG_EXT_STATS_MAX, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_CLEAR_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_RX_RATE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_TX_RATE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_TX_HOST_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_RX_HOST_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_AST_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SRNG_PTR_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_RX_MON_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_CFG_PARAMS},
	{TXRX_FW_STATS_INVALID, TXRX_PDEV_CFG_PARAMS},
	{TXRX_FW_STATS_INVALID, TXRX_NAPI_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_INTERRUPT_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_SOC_FSE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_HAL_REG_WRITE_STATS},
	{TXRX_FW_STATS_INVALID, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_HOST_STATS_INVALID},
	{TXRX_FW_STATS_INVALID, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_PDEV_RX_RATE_EXT, TXRX_HOST_STATS_INVALID},
	{HTT_DBG_EXT_STATS_TX_SOUNDING_INFO, TXRX_HOST_STATS_INVALID}
};
#endif

/* MCL specific functions */
#if defined(DP_CON_MON)

#ifdef IPA_OFFLOAD
/**
 * dp_get_num_rx_contexts() - get number of RX contexts
 * @soc_hdl: cdp opaque soc handle
 *
 * Return: number of RX contexts
 */
static int dp_get_num_rx_contexts(struct cdp_soc_t *soc_hdl)
{
	int num_rx_contexts;
	uint32_t reo_ring_map;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	reo_ring_map = wlan_cfg_get_reo_rings_mapping(soc->wlan_cfg_ctx);

	switch (soc->arch_id) {
	case CDP_ARCH_TYPE_BE:
		/* 2 REO rings are used for IPA */
		reo_ring_map &=  ~(BIT(3) | BIT(7));

		break;
	case CDP_ARCH_TYPE_LI:
		/* 1 REO ring is used for IPA */
		reo_ring_map &=  ~BIT(3);
		break;
	default:
		dp_err("unknown arch_id 0x%x", soc->arch_id);
		QDF_BUG(0);
	}
	/*
	 * qdf_get_hweight32 prefer over qdf_get_hweight8 in case map is scaled
	 * in future
	 */
	num_rx_contexts = qdf_get_hweight32(reo_ring_map);

	return num_rx_contexts;
}
#else
#ifdef WLAN_SOFTUMAC_SUPPORT
static int dp_get_num_rx_contexts(struct cdp_soc_t *soc_hdl)
{
	uint32_t rx_rings_config;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	rx_rings_config = wlan_cfg_get_rx_rings_mapping(soc->wlan_cfg_ctx);
	/*
	 * qdf_get_hweight32 prefer over qdf_get_hweight8 in case map is scaled
	 * in future
	 */
	return qdf_get_hweight32(rx_rings_config);
}
#else
static int dp_get_num_rx_contexts(struct cdp_soc_t *soc_hdl)
{
	int num_rx_contexts;
	uint32_t reo_config;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	reo_config = wlan_cfg_get_reo_rings_mapping(soc->wlan_cfg_ctx);
	/*
	 * qdf_get_hweight32 prefer over qdf_get_hweight8 in case map is scaled
	 * in future
	 */
	num_rx_contexts = qdf_get_hweight32(reo_config);

	return num_rx_contexts;
}
#endif /* WLAN_SOFTUMAC_SUPPORT */
#endif

#endif

#ifdef FEATURE_MEC
void dp_peer_mec_flush_entries(struct dp_soc *soc)
{
	unsigned int index;
	struct dp_mec_entry *mecentry, *mecentry_next;

	TAILQ_HEAD(, dp_mec_entry) free_list;
	TAILQ_INIT(&free_list);

	if (!soc->mec_hash.mask)
		return;

	if (!soc->mec_hash.bins)
		return;

	if (!qdf_atomic_read(&soc->mec_cnt))
		return;

	qdf_spin_lock_bh(&soc->mec_lock);
	for (index = 0; index <= soc->mec_hash.mask; index++) {
		if (!TAILQ_EMPTY(&soc->mec_hash.bins[index])) {
			TAILQ_FOREACH_SAFE(mecentry, &soc->mec_hash.bins[index],
					   hash_list_elem, mecentry_next) {
			    dp_peer_mec_detach_entry(soc, mecentry, &free_list);
			}
		}
	}
	qdf_spin_unlock_bh(&soc->mec_lock);

	dp_peer_mec_free_list(soc, &free_list);
}

/**
 * dp_print_mec_stats() - Dump MEC entries in table
 * @soc: Datapath soc handle
 *
 * Return: none
 */
static void dp_print_mec_stats(struct dp_soc *soc)
{
	int i;
	uint32_t index;
	struct dp_mec_entry *mecentry = NULL, *mec_list;
	uint32_t num_entries = 0;

	DP_PRINT_STATS("MEC Stats:");
	DP_PRINT_STATS("   Entries Added   = %d", soc->stats.mec.added);
	DP_PRINT_STATS("   Entries Deleted = %d", soc->stats.mec.deleted);

	if (!qdf_atomic_read(&soc->mec_cnt))
		return;

	mec_list = qdf_mem_malloc(sizeof(*mecentry) * DP_PEER_MAX_MEC_ENTRY);
	if (!mec_list) {
		dp_peer_warn("%pK: failed to allocate mec_list", soc);
		return;
	}

	DP_PRINT_STATS("MEC Table:");
	for (index = 0; index <= soc->mec_hash.mask; index++) {
		qdf_spin_lock_bh(&soc->mec_lock);
		if (TAILQ_EMPTY(&soc->mec_hash.bins[index])) {
			qdf_spin_unlock_bh(&soc->mec_lock);
			continue;
		}

		TAILQ_FOREACH(mecentry, &soc->mec_hash.bins[index],
			      hash_list_elem) {
			qdf_mem_copy(&mec_list[num_entries], mecentry,
				     sizeof(*mecentry));
			num_entries++;
		}
		qdf_spin_unlock_bh(&soc->mec_lock);
	}

	if (!num_entries) {
		qdf_mem_free(mec_list);
		return;
	}

	for (i = 0; i < num_entries; i++) {
		DP_PRINT_STATS("%6d mac_addr = " QDF_MAC_ADDR_FMT
			       " is_active = %d pdev_id = %d vdev_id = %d",
			       i,
			       QDF_MAC_ADDR_REF(mec_list[i].mac_addr.raw),
			       mec_list[i].is_active,
			       mec_list[i].pdev_id,
			       mec_list[i].vdev_id);
	}
	qdf_mem_free(mec_list);
}
#else
static void dp_print_mec_stats(struct dp_soc *soc)
{
}
#endif

static int dp_peer_add_ast_wifi3(struct cdp_soc_t *soc_hdl,
				 uint8_t vdev_id,
				 uint8_t *peer_mac,
				 uint8_t *mac_addr,
				 enum cdp_txrx_ast_entry_type type,
				 uint32_t flags)
{
	int ret = -1;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)soc_hdl,
						       peer_mac, 0, vdev_id,
						       DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("Peer is NULL!");
		return ret;
	}

	status = dp_peer_add_ast((struct dp_soc *)soc_hdl,
				 peer,
				 mac_addr,
				 type,
				 flags);
	if ((status == QDF_STATUS_SUCCESS) ||
	    (status == QDF_STATUS_E_ALREADY) ||
	    (status == QDF_STATUS_E_AGAIN))
		ret = 0;

	dp_hmwds_ast_add_notify(peer, mac_addr,
				type, status, false);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return ret;
}

static int dp_peer_update_ast_wifi3(struct cdp_soc_t *soc_hdl,
						uint8_t vdev_id,
						uint8_t *peer_mac,
						uint8_t *wds_macaddr,
						uint32_t flags)
{
	int status = -1;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_ast_entry  *ast_entry = NULL;
	struct dp_peer *peer;

	if (soc->ast_offload_support)
		return status;

	peer = dp_peer_find_hash_find((struct dp_soc *)soc_hdl,
				      peer_mac, 0, vdev_id,
				      DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_debug("Peer is NULL!");
		return status;
	}

	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, wds_macaddr,
						    peer->vdev->pdev->pdev_id);

	if (ast_entry) {
		status = dp_peer_update_ast(soc,
					    peer,
					    ast_entry, flags);
	}
	qdf_spin_unlock_bh(&soc->ast_lock);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

/**
 * dp_peer_reset_ast_entries() - Deletes all HMWDS entries for a peer
 * @soc:		Datapath SOC handle
 * @peer:		DP peer
 * @arg:		callback argument
 *
 * Return: None
 */
static void
dp_peer_reset_ast_entries(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	struct dp_ast_entry *ast_entry = NULL;
	struct dp_ast_entry *tmp_ast_entry;

	DP_PEER_ITERATE_ASE_LIST(peer, ast_entry, tmp_ast_entry) {
		if ((ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM) ||
		    (ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM_SEC))
			dp_peer_del_ast(soc, ast_entry);
	}
}

/**
 * dp_wds_reset_ast_wifi3() - Reset the is_active param for ast entry
 * @soc_hdl:		Datapath SOC handle
 * @wds_macaddr:	WDS entry MAC Address
 * @peer_mac_addr:	WDS entry MAC Address
 * @vdev_id:		id of vdev handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_wds_reset_ast_wifi3(struct cdp_soc_t *soc_hdl,
					 uint8_t *wds_macaddr,
					 uint8_t *peer_mac_addr,
					 uint8_t vdev_id)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_ast_entry *ast_entry = NULL;
	struct dp_peer *peer;
	struct dp_pdev *pdev;
	struct dp_vdev *vdev;

	if (soc->ast_offload_support)
		return QDF_STATUS_E_FAILURE;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	pdev = vdev->pdev;

	if (peer_mac_addr) {
		peer = dp_peer_find_hash_find(soc, peer_mac_addr,
					      0, vdev->vdev_id,
					      DP_MOD_ID_CDP);
		if (!peer) {
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
			return QDF_STATUS_E_FAILURE;
		}

		qdf_spin_lock_bh(&soc->ast_lock);
		dp_peer_reset_ast_entries(soc, peer, NULL);
		qdf_spin_unlock_bh(&soc->ast_lock);
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	} else if (wds_macaddr) {
		qdf_spin_lock_bh(&soc->ast_lock);
		ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, wds_macaddr,
							    pdev->pdev_id);

		if (ast_entry) {
			if ((ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM) ||
			    (ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM_SEC))
				dp_peer_del_ast(soc, ast_entry);
		}
		qdf_spin_unlock_bh(&soc->ast_lock);
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_wds_reset_ast_table_wifi3() - Reset the is_active param for all ast entry
 * @soc_hdl:		Datapath SOC handle
 * @vdev_id:		id of vdev object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_wds_reset_ast_table_wifi3(struct cdp_soc_t  *soc_hdl,
			     uint8_t vdev_id)
{
	struct dp_soc *soc = (struct dp_soc *) soc_hdl;

	if (soc->ast_offload_support)
		return QDF_STATUS_SUCCESS;

	qdf_spin_lock_bh(&soc->ast_lock);

	dp_soc_iterate_peer(soc, dp_peer_reset_ast_entries, NULL,
			    DP_MOD_ID_CDP);
	qdf_spin_unlock_bh(&soc->ast_lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_peer_flush_ast_entries() - Delete all wds and hmwds ast entries of a peer
 * @soc:		Datapath SOC
 * @peer:		Datapath peer
 * @arg:		arg to callback
 *
 * Return: None
 */
static void
dp_peer_flush_ast_entries(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	struct dp_ast_entry *ase = NULL;
	struct dp_ast_entry *temp_ase;

	DP_PEER_ITERATE_ASE_LIST(peer, ase, temp_ase) {
		if ((ase->type ==
			CDP_TXRX_AST_TYPE_STATIC) ||
			(ase->type ==
			 CDP_TXRX_AST_TYPE_SELF) ||
			(ase->type ==
			 CDP_TXRX_AST_TYPE_STA_BSS))
			continue;
		dp_peer_del_ast(soc, ase);
	}
}

/**
 * dp_wds_flush_ast_table_wifi3() - Delete all wds and hmwds ast entry
 * @soc_hdl:		Datapath SOC handle
 *
 * Return: None
 */
static void dp_wds_flush_ast_table_wifi3(struct cdp_soc_t  *soc_hdl)
{
	struct dp_soc *soc = (struct dp_soc *) soc_hdl;

	qdf_spin_lock_bh(&soc->ast_lock);

	dp_soc_iterate_peer(soc, dp_peer_flush_ast_entries, NULL,
			    DP_MOD_ID_CDP);

	qdf_spin_unlock_bh(&soc->ast_lock);
	dp_peer_mec_flush_entries(soc);
}

#if defined(IPA_WDS_EASYMESH_FEATURE) && defined(FEATURE_AST)
/**
 * dp_peer_send_wds_disconnect() - Send Disconnect event to IPA for each peer
 * @soc: Datapath SOC
 * @peer: Datapath peer
 *
 * Return: None
 */
static void
dp_peer_send_wds_disconnect(struct dp_soc *soc, struct dp_peer *peer)
{
	struct dp_ast_entry *ase = NULL;
	struct dp_ast_entry *temp_ase;

	DP_PEER_ITERATE_ASE_LIST(peer, ase, temp_ase) {
		if (ase->type == CDP_TXRX_AST_TYPE_WDS) {
			soc->cdp_soc.ol_ops->peer_send_wds_disconnect(soc->ctrl_psoc,
								      ase->mac_addr.raw,
								      ase->vdev_id);
		}
	}
}
#elif defined(FEATURE_AST)
static void
dp_peer_send_wds_disconnect(struct dp_soc *soc, struct dp_peer *peer)
{
}
#endif

/**
 * dp_peer_check_ast_offload() - check ast offload support is enable or not
 * @soc: soc handle
 *
 * Return: false in case of IPA and true/false in IPQ case
 *
 */
#if defined(IPA_OFFLOAD) && defined(QCA_WIFI_QCN9224)
static inline bool dp_peer_check_ast_offload(struct dp_soc *soc)
{
	return false;
}
#else
static inline bool dp_peer_check_ast_offload(struct dp_soc *soc)
{
	if (soc->ast_offload_support)
		return true;

	return false;
}
#endif

/**
 * dp_peer_get_ast_info_by_soc_wifi3() - search the soc AST hash table
 *                                       and return ast entry information
 *                                       of first ast entry found in the
 *                                       table with given mac address
 * @soc_hdl: data path soc handle
 * @ast_mac_addr: AST entry mac address
 * @ast_entry_info: ast entry information
 *
 * Return: true if ast entry found with ast_mac_addr
 *          false if ast entry not found
 */
static bool dp_peer_get_ast_info_by_soc_wifi3
	(struct cdp_soc_t *soc_hdl,
	 uint8_t *ast_mac_addr,
	 struct cdp_ast_entry_info *ast_entry_info)
{
	struct dp_ast_entry *ast_entry = NULL;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_peer *peer = NULL;

	if (dp_peer_check_ast_offload(soc))
		return false;

	qdf_spin_lock_bh(&soc->ast_lock);

	ast_entry = dp_peer_ast_hash_find_soc(soc, ast_mac_addr);
	if ((!ast_entry) ||
	    (ast_entry->delete_in_progress && !ast_entry->callback)) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	}

	peer = dp_peer_get_ref_by_id(soc, ast_entry->peer_id,
				     DP_MOD_ID_AST);
	if (!peer) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	}

	ast_entry_info->type = ast_entry->type;
	ast_entry_info->pdev_id = ast_entry->pdev_id;
	ast_entry_info->vdev_id = ast_entry->vdev_id;
	ast_entry_info->peer_id = ast_entry->peer_id;
	qdf_mem_copy(&ast_entry_info->peer_mac_addr[0],
		     &peer->mac_addr.raw[0],
		     QDF_MAC_ADDR_SIZE);
	dp_peer_unref_delete(peer, DP_MOD_ID_AST);
	qdf_spin_unlock_bh(&soc->ast_lock);
	return true;
}

/**
 * dp_peer_get_ast_info_by_pdevid_wifi3() - search the soc AST hash table
 *                                          and return ast entry information
 *                                          if mac address and pdev_id matches
 * @soc_hdl: data path soc handle
 * @ast_mac_addr: AST entry mac address
 * @pdev_id: pdev_id
 * @ast_entry_info: ast entry information
 *
 * Return: true if ast entry found with ast_mac_addr
 *          false if ast entry not found
 */
static bool dp_peer_get_ast_info_by_pdevid_wifi3
		(struct cdp_soc_t *soc_hdl,
		 uint8_t *ast_mac_addr,
		 uint8_t pdev_id,
		 struct cdp_ast_entry_info *ast_entry_info)
{
	struct dp_ast_entry *ast_entry;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_peer *peer = NULL;

	if (soc->ast_offload_support)
		return false;

	qdf_spin_lock_bh(&soc->ast_lock);

	ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, ast_mac_addr,
						    pdev_id);

	if ((!ast_entry) ||
	    (ast_entry->delete_in_progress && !ast_entry->callback)) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	}

	peer = dp_peer_get_ref_by_id(soc, ast_entry->peer_id,
				     DP_MOD_ID_AST);
	if (!peer) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	}

	ast_entry_info->type = ast_entry->type;
	ast_entry_info->pdev_id = ast_entry->pdev_id;
	ast_entry_info->vdev_id = ast_entry->vdev_id;
	ast_entry_info->peer_id = ast_entry->peer_id;
	qdf_mem_copy(&ast_entry_info->peer_mac_addr[0],
		     &peer->mac_addr.raw[0],
		     QDF_MAC_ADDR_SIZE);
	dp_peer_unref_delete(peer, DP_MOD_ID_AST);
	qdf_spin_unlock_bh(&soc->ast_lock);
	return true;
}

/**
 * dp_peer_ast_entry_del_by_soc() - delete the ast entry from soc AST hash table
 *                            with given mac address
 * @soc_handle: data path soc handle
 * @mac_addr: AST entry mac address
 * @callback: callback function to called on ast delete response from FW
 * @cookie: argument to be passed to callback
 *
 * Return: QDF_STATUS_SUCCESS if ast entry found with ast_mac_addr and delete
 *          is sent
 *          QDF_STATUS_E_INVAL false if ast entry not found
 */
static QDF_STATUS dp_peer_ast_entry_del_by_soc(struct cdp_soc_t *soc_handle,
					       uint8_t *mac_addr,
					       txrx_ast_free_cb callback,
					       void *cookie)

{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;
	struct dp_ast_entry *ast_entry = NULL;
	txrx_ast_free_cb cb = NULL;
	void *arg = NULL;

	if (soc->ast_offload_support)
		return -QDF_STATUS_E_INVAL;

	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_soc(soc, mac_addr);
	if (!ast_entry) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return -QDF_STATUS_E_INVAL;
	}

	if (ast_entry->callback) {
		cb = ast_entry->callback;
		arg = ast_entry->cookie;
	}

	ast_entry->callback = callback;
	ast_entry->cookie = cookie;

	/*
	 * if delete_in_progress is set AST delete is sent to target
	 * and host is waiting for response should not send delete
	 * again
	 */
	if (!ast_entry->delete_in_progress)
		dp_peer_del_ast(soc, ast_entry);

	qdf_spin_unlock_bh(&soc->ast_lock);
	if (cb) {
		cb(soc->ctrl_psoc,
		   dp_soc_to_cdp_soc(soc),
		   arg,
		   CDP_TXRX_AST_DELETE_IN_PROGRESS);
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_peer_ast_entry_del_by_pdev() - delete the ast entry from soc AST hash
 *                                   table if mac address and pdev_id matches
 * @soc_handle: data path soc handle
 * @mac_addr: AST entry mac address
 * @pdev_id: pdev id
 * @callback: callback function to called on ast delete response from FW
 * @cookie: argument to be passed to callback
 *
 * Return: QDF_STATUS_SUCCESS if ast entry found with ast_mac_addr and delete
 *          is sent
 *          QDF_STATUS_E_INVAL false if ast entry not found
 */

static QDF_STATUS dp_peer_ast_entry_del_by_pdev(struct cdp_soc_t *soc_handle,
						uint8_t *mac_addr,
						uint8_t pdev_id,
						txrx_ast_free_cb callback,
						void *cookie)

{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;
	struct dp_ast_entry *ast_entry;
	txrx_ast_free_cb cb = NULL;
	void *arg = NULL;

	if (soc->ast_offload_support)
		return -QDF_STATUS_E_INVAL;

	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, mac_addr, pdev_id);

	if (!ast_entry) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return -QDF_STATUS_E_INVAL;
	}

	if (ast_entry->callback) {
		cb = ast_entry->callback;
		arg = ast_entry->cookie;
	}

	ast_entry->callback = callback;
	ast_entry->cookie = cookie;

	/*
	 * if delete_in_progress is set AST delete is sent to target
	 * and host is waiting for response should not sent delete
	 * again
	 */
	if (!ast_entry->delete_in_progress)
		dp_peer_del_ast(soc, ast_entry);

	qdf_spin_unlock_bh(&soc->ast_lock);

	if (cb) {
		cb(soc->ctrl_psoc,
		   dp_soc_to_cdp_soc(soc),
		   arg,
		   CDP_TXRX_AST_DELETE_IN_PROGRESS);
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_peer_HMWDS_ast_entry_del() - delete the ast entry from soc AST hash
 *                                 table if HMWDS rem-addr command is issued
 *
 * @soc_handle: data path soc handle
 * @vdev_id: vdev id
 * @wds_macaddr: AST entry mac address to delete
 * @type: cdp_txrx_ast_entry_type to send to FW
 * @delete_in_fw: flag to indicate AST entry deletion in FW
 *
 * Return: QDF_STATUS_SUCCESS if ast entry found with ast_mac_addr and delete
 *         is sent
 *         QDF_STATUS_E_INVAL false if ast entry not found
 */
static QDF_STATUS dp_peer_HMWDS_ast_entry_del(struct cdp_soc_t *soc_handle,
					      uint8_t vdev_id,
					      uint8_t *wds_macaddr,
					      uint8_t type,
					      uint8_t delete_in_fw)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	if (soc->ast_offload_support) {
		dp_del_wds_entry_wrapper(soc, vdev_id, wds_macaddr, type,
					 delete_in_fw);
		return QDF_STATUS_SUCCESS;
	}

	return -QDF_STATUS_E_INVAL;
}

#ifdef FEATURE_AST
/**
 * dp_print_mlo_ast_stats() - Print AST stats for MLO peers
 *
 * @soc: core DP soc context
 *
 * Return: void
 */
static void dp_print_mlo_ast_stats(struct dp_soc *soc)
{
	if (soc->arch_ops.print_mlo_ast_stats)
		soc->arch_ops.print_mlo_ast_stats(soc);
}

void
dp_print_peer_ast_entries(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	struct dp_ast_entry *ase, *tmp_ase;
	uint32_t num_entries = 0;
	char type[CDP_TXRX_AST_TYPE_MAX][10] = {
			"NONE", "STATIC", "SELF", "WDS", "HMWDS", "BSS",
			"DA", "HMWDS_SEC", "MLD"};

	DP_PEER_ITERATE_ASE_LIST(peer, ase, tmp_ase) {
	    DP_PRINT_STATS("%6d mac_addr = "QDF_MAC_ADDR_FMT
		    " peer_mac_addr = "QDF_MAC_ADDR_FMT
		    " peer_id = %u"
		    " type = %s"
		    " next_hop = %d"
		    " is_active = %d"
		    " ast_idx = %d"
		    " ast_hash = %d"
		    " delete_in_progress = %d"
		    " pdev_id = %d"
		    " vdev_id = %d",
		    ++num_entries,
		    QDF_MAC_ADDR_REF(ase->mac_addr.raw),
		    QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		    ase->peer_id,
		    type[ase->type],
		    ase->next_hop,
		    ase->is_active,
		    ase->ast_idx,
		    ase->ast_hash_value,
		    ase->delete_in_progress,
		    ase->pdev_id,
		    ase->vdev_id);
	}
}

void dp_print_ast_stats(struct dp_soc *soc)
{
	DP_PRINT_STATS("AST Stats:");
	DP_PRINT_STATS("	Entries Added   = %d", soc->stats.ast.added);
	DP_PRINT_STATS("	Entries Deleted = %d", soc->stats.ast.deleted);
	DP_PRINT_STATS("	Entries Agedout = %d", soc->stats.ast.aged_out);
	DP_PRINT_STATS("	Entries MAP ERR  = %d", soc->stats.ast.map_err);
	DP_PRINT_STATS("	Entries Mismatch ERR  = %d",
		       soc->stats.ast.ast_mismatch);

	DP_PRINT_STATS("AST Table:");

	qdf_spin_lock_bh(&soc->ast_lock);

	dp_soc_iterate_peer(soc, dp_print_peer_ast_entries, NULL,
			    DP_MOD_ID_GENERIC_STATS);

	qdf_spin_unlock_bh(&soc->ast_lock);

	dp_print_mlo_ast_stats(soc);
}
#else
void dp_print_ast_stats(struct dp_soc *soc)
{
	DP_PRINT_STATS("AST Stats not available.Enable FEATURE_AST");
	return;
}
#endif

/**
 * dp_print_peer_info() - Dump peer info
 * @soc: Datapath soc handle
 * @peer: Datapath peer handle
 * @arg: argument to iter function
 *
 * Return: void
 */
static void
dp_print_peer_info(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	struct dp_txrx_peer *txrx_peer = NULL;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer)
		return;

	DP_PRINT_STATS(" peer id = %d"
		       " peer_mac_addr = "QDF_MAC_ADDR_FMT
		       " nawds_enabled = %d"
		       " bss_peer = %d"
		       " wds_enabled = %d"
		       " tx_cap_enabled = %d"
		       " rx_cap_enabled = %d",
		       peer->peer_id,
		       QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		       txrx_peer->nawds_enabled,
		       txrx_peer->bss_peer,
		       txrx_peer->wds_enabled,
		       dp_monitor_is_tx_cap_enabled(peer),
		       dp_monitor_is_rx_cap_enabled(peer));
}

/**
 * dp_print_peer_table() - Dump all Peer stats
 * @vdev: Datapath Vdev handle
 *
 * Return: void
 */
static void dp_print_peer_table(struct dp_vdev *vdev)
{
	DP_PRINT_STATS("Dumping Peer Table  Stats:");
	dp_vdev_iterate_peer(vdev, dp_print_peer_info, NULL,
			     DP_MOD_ID_GENERIC_STATS);
}

#ifdef DP_MEM_PRE_ALLOC

void *dp_context_alloc_mem(struct dp_soc *soc, enum dp_ctxt_type ctxt_type,
			   size_t ctxt_size)
{
	void *ctxt_mem;

	if (!soc->cdp_soc.ol_ops->dp_prealloc_get_context) {
		dp_warn("dp_prealloc_get_context null!");
		goto dynamic_alloc;
	}

	ctxt_mem = soc->cdp_soc.ol_ops->dp_prealloc_get_context(ctxt_type,
								ctxt_size);

	if (ctxt_mem)
		goto end;

dynamic_alloc:
	dp_info("switch to dynamic-alloc for type %d, size %zu",
		ctxt_type, ctxt_size);
	ctxt_mem = qdf_mem_malloc(ctxt_size);
end:
	return ctxt_mem;
}

void dp_context_free_mem(struct dp_soc *soc, enum dp_ctxt_type ctxt_type,
			 void *vaddr)
{
	QDF_STATUS status;

	if (soc->cdp_soc.ol_ops->dp_prealloc_put_context) {
		status = soc->cdp_soc.ol_ops->dp_prealloc_put_context(
								ctxt_type,
								vaddr);
	} else {
		dp_warn("dp_prealloc_put_context null!");
		status = QDF_STATUS_E_NOSUPPORT;
	}

	if (QDF_IS_STATUS_ERROR(status)) {
		dp_info("Context type %d not pre-allocated", ctxt_type);
		qdf_mem_free(vaddr);
	}
}

static inline
void *dp_srng_aligned_mem_alloc_consistent(struct dp_soc *soc,
					   struct dp_srng *srng,
					   uint32_t ring_type)
{
	void *mem;

	qdf_assert(!srng->is_mem_prealloc);

	if (!soc->cdp_soc.ol_ops->dp_prealloc_get_consistent) {
		dp_warn("dp_prealloc_get_consistent is null!");
		goto qdf;
	}

	mem =
		soc->cdp_soc.ol_ops->dp_prealloc_get_consistent
						(&srng->alloc_size,
						 &srng->base_vaddr_unaligned,
						 &srng->base_paddr_unaligned,
						 &srng->base_paddr_aligned,
						 DP_RING_BASE_ALIGN, ring_type);

	if (mem) {
		srng->is_mem_prealloc = true;
		goto end;
	}
qdf:
	mem =  qdf_aligned_mem_alloc_consistent(soc->osdev, &srng->alloc_size,
						&srng->base_vaddr_unaligned,
						&srng->base_paddr_unaligned,
						&srng->base_paddr_aligned,
						DP_RING_BASE_ALIGN);
end:
	dp_info("%s memory %pK dp_srng %pK ring_type %d alloc_size %d num_entries %d",
		srng->is_mem_prealloc ? "pre-alloc" : "dynamic-alloc", mem,
		srng, ring_type, srng->alloc_size, srng->num_entries);
	return mem;
}

static inline void dp_srng_mem_free_consistent(struct dp_soc *soc,
					       struct dp_srng *srng)
{
	if (srng->is_mem_prealloc) {
		if (!soc->cdp_soc.ol_ops->dp_prealloc_put_consistent) {
			dp_warn("dp_prealloc_put_consistent is null!");
			QDF_BUG(0);
			return;
		}
		soc->cdp_soc.ol_ops->dp_prealloc_put_consistent
						(srng->alloc_size,
						 srng->base_vaddr_unaligned,
						 srng->base_paddr_unaligned);

	} else {
		qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
					srng->alloc_size,
					srng->base_vaddr_unaligned,
					srng->base_paddr_unaligned, 0);
	}
}

void dp_desc_multi_pages_mem_alloc(struct dp_soc *soc,
				   enum qdf_dp_desc_type desc_type,
				   struct qdf_mem_multi_page_t *pages,
				   size_t element_size,
				   uint32_t element_num,
				   qdf_dma_context_t memctxt,
				   bool cacheable)
{
	if (!soc->cdp_soc.ol_ops->dp_get_multi_pages) {
		dp_warn("dp_get_multi_pages is null!");
		goto qdf;
	}

	pages->num_pages = 0;
	pages->is_mem_prealloc = 0;
	soc->cdp_soc.ol_ops->dp_get_multi_pages(desc_type,
						element_size,
						element_num,
						pages,
						cacheable);
	if (pages->num_pages)
		goto end;

qdf:
	qdf_mem_multi_pages_alloc(soc->osdev, pages, element_size,
				  element_num, memctxt, cacheable);
end:
	dp_info("%s desc_type %d element_size %d element_num %d cacheable %d",
		pages->is_mem_prealloc ? "pre-alloc" : "dynamic-alloc",
		desc_type, (int)element_size, element_num, cacheable);
}

void dp_desc_multi_pages_mem_free(struct dp_soc *soc,
				  enum qdf_dp_desc_type desc_type,
				  struct qdf_mem_multi_page_t *pages,
				  qdf_dma_context_t memctxt,
				  bool cacheable)
{
	if (pages->is_mem_prealloc) {
		if (!soc->cdp_soc.ol_ops->dp_put_multi_pages) {
			dp_warn("dp_put_multi_pages is null!");
			QDF_BUG(0);
			return;
		}

		soc->cdp_soc.ol_ops->dp_put_multi_pages(desc_type, pages);
		qdf_mem_zero(pages, sizeof(*pages));
	} else {
		qdf_mem_multi_pages_free(soc->osdev, pages,
					 memctxt, cacheable);
	}
}

#else

static inline
void *dp_srng_aligned_mem_alloc_consistent(struct dp_soc *soc,
					   struct dp_srng *srng,
					   uint32_t ring_type)

{
	void *mem;

	mem = qdf_aligned_mem_alloc_consistent(soc->osdev, &srng->alloc_size,
					       &srng->base_vaddr_unaligned,
					       &srng->base_paddr_unaligned,
					       &srng->base_paddr_aligned,
					       DP_RING_BASE_ALIGN);
	if (mem)
		qdf_mem_set(srng->base_vaddr_unaligned, 0, srng->alloc_size);

	return mem;
}

static inline void dp_srng_mem_free_consistent(struct dp_soc *soc,
					       struct dp_srng *srng)
{
	qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
				srng->alloc_size,
				srng->base_vaddr_unaligned,
				srng->base_paddr_unaligned, 0);
}

#endif /* DP_MEM_PRE_ALLOC */

#ifdef QCA_SUPPORT_WDS_EXTENDED
bool dp_vdev_is_wds_ext_enabled(struct dp_vdev *vdev)
{
	return vdev->wds_ext_enabled;
}
#else
bool dp_vdev_is_wds_ext_enabled(struct dp_vdev *vdev)
{
	return false;
}
#endif

void dp_pdev_update_fast_rx_flag(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_vdev *vdev = NULL;
	uint8_t rx_fast_flag = true;

	/* Check if protocol tagging enable */
	if (pdev->is_rx_protocol_tagging_enabled) {
		rx_fast_flag = false;
		goto update_flag;
	}

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		/* Check if any VDEV has NAWDS enabled */
		if (vdev->nawds_enabled) {
			rx_fast_flag = false;
			break;
		}

		/* Check if any VDEV has multipass enabled */
		if (vdev->multipass_en) {
			rx_fast_flag = false;
			break;
		}

		/* Check if any VDEV has mesh enabled */
		if (vdev->mesh_vdev) {
			rx_fast_flag = false;
			break;
		}
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);

update_flag:
	dp_init_info("Updated Rx fast flag to %u", rx_fast_flag);
	pdev->rx_fast_flag = rx_fast_flag;
}

void dp_soc_set_interrupt_mode(struct dp_soc *soc)
{
	uint32_t msi_base_data, msi_vector_start;
	int msi_vector_count, ret;

	soc->intr_mode = DP_INTR_INTEGRATED;

	if (!(soc->wlan_cfg_ctx->napi_enabled) ||
	    (dp_is_monitor_mode_using_poll(soc) &&
	     soc->cdp_soc.ol_ops->get_con_mode &&
	     soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_MONITOR_MODE)) {
		soc->intr_mode = DP_INTR_POLL;
	} else {
		ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
						  &msi_vector_count,
						  &msi_base_data,
						  &msi_vector_start);
		if (ret)
			return;

		soc->intr_mode = DP_INTR_MSI;
	}
}

static int dp_srng_calculate_msi_group(struct dp_soc *soc,
				       enum hal_ring_type ring_type,
				       int ring_num,
				       int *reg_msi_grp_num,
				       bool nf_irq_support,
				       int *nf_msi_grp_num)
{
	struct wlan_cfg_dp_soc_ctxt *cfg_ctx = soc->wlan_cfg_ctx;
	uint8_t *grp_mask, *nf_irq_mask = NULL;
	bool nf_irq_enabled = false;
	uint8_t wbm2_sw_rx_rel_ring_id;

	switch (ring_type) {
	case WBM2SW_RELEASE:
		wbm2_sw_rx_rel_ring_id =
			wlan_cfg_get_rx_rel_ring_id(cfg_ctx);
		if (ring_num == wbm2_sw_rx_rel_ring_id) {
			/* dp_rx_wbm_err_process - soc->rx_rel_ring */
			grp_mask = &cfg_ctx->int_rx_wbm_rel_ring_mask[0];
			ring_num = 0;
		} else if (ring_num == WBM2_SW_PPE_REL_RING_ID) {
			grp_mask = &cfg_ctx->int_ppeds_wbm_release_ring_mask[0];
			ring_num = 0;
		}  else { /* dp_tx_comp_handler - soc->tx_comp_ring */
			grp_mask = &soc->wlan_cfg_ctx->int_tx_ring_mask[0];
			nf_irq_mask = dp_srng_get_near_full_irq_mask(soc,
								     ring_type,
								     ring_num);
			if (nf_irq_mask)
				nf_irq_enabled = true;

			/*
			 * Using ring 4 as 4th tx completion ring since ring 3
			 * is Rx error ring
			 */
			if (ring_num == WBM2SW_TXCOMP_RING4_NUM)
				ring_num = TXCOMP_RING4_NUM;
		}
	break;

	case REO_EXCEPTION:
		/* dp_rx_err_process - &soc->reo_exception_ring */
		grp_mask = &soc->wlan_cfg_ctx->int_rx_err_ring_mask[0];
	break;

	case REO_DST:
		/* dp_rx_process - soc->reo_dest_ring */
		grp_mask = &soc->wlan_cfg_ctx->int_rx_ring_mask[0];
		nf_irq_mask = dp_srng_get_near_full_irq_mask(soc, ring_type,
							     ring_num);
		if (nf_irq_mask)
			nf_irq_enabled = true;
	break;

	case REO_STATUS:
		/* dp_reo_status_ring_handler - soc->reo_status_ring */
		grp_mask = &soc->wlan_cfg_ctx->int_reo_status_ring_mask[0];
	break;

	/* dp_rx_mon_status_srng_process - pdev->rxdma_mon_status_ring*/
	case RXDMA_MONITOR_STATUS:
	/* dp_rx_mon_dest_process - pdev->rxdma_mon_dst_ring */
	case RXDMA_MONITOR_DST:
		/* dp_mon_process */
		grp_mask = &soc->wlan_cfg_ctx->int_rx_mon_ring_mask[0];
	break;
	case TX_MONITOR_DST:
		/* dp_tx_mon_process */
		grp_mask = &soc->wlan_cfg_ctx->int_tx_mon_ring_mask[0];
	break;
	case RXDMA_DST:
		/* dp_rxdma_err_process */
		grp_mask = &soc->wlan_cfg_ctx->int_rxdma2host_ring_mask[0];
	break;

	case RXDMA_BUF:
		grp_mask = &soc->wlan_cfg_ctx->int_host2rxdma_ring_mask[0];
	break;

	case RXDMA_MONITOR_BUF:
		grp_mask = &soc->wlan_cfg_ctx->int_host2rxdma_mon_ring_mask[0];
	break;

	case TX_MONITOR_BUF:
		grp_mask = &soc->wlan_cfg_ctx->int_host2txmon_ring_mask[0];
	break;

	case REO2PPE:
		grp_mask = &soc->wlan_cfg_ctx->int_reo2ppe_ring_mask[0];
	break;

	case PPE2TCL:
		grp_mask = &soc->wlan_cfg_ctx->int_ppe2tcl_ring_mask[0];
	break;

	case TCL_DATA:
	/* CMD_CREDIT_RING is used as command in 8074 and credit in 9000 */
	case TCL_CMD_CREDIT:
	case REO_CMD:
	case SW2WBM_RELEASE:
	case WBM_IDLE_LINK:
		/* normally empty SW_TO_HW rings */
		return -QDF_STATUS_E_NOENT;
	break;

	case TCL_STATUS:
	case REO_REINJECT:
		/* misc unused rings */
		return -QDF_STATUS_E_NOENT;
	break;

	case CE_SRC:
	case CE_DST:
	case CE_DST_STATUS:
		/* CE_rings - currently handled by hif */
	default:
		return -QDF_STATUS_E_NOENT;
	break;
	}

	*reg_msi_grp_num = dp_srng_find_ring_in_mask(ring_num, grp_mask);

	if (nf_irq_support && nf_irq_enabled) {
		*nf_msi_grp_num = dp_srng_find_ring_in_mask(ring_num,
							    nf_irq_mask);
	}

	return QDF_STATUS_SUCCESS;
}

#if defined(IPA_OFFLOAD) && defined(IPA_WDI3_VLAN_SUPPORT)
static void
dp_ipa_vlan_srng_msi_setup(struct hal_srng_params *ring_params, int ring_type,
			   int ring_num)
{
	if (wlan_ipa_is_vlan_enabled()) {
		if ((ring_type == REO_DST) &&
		    (ring_num == IPA_ALT_REO_DEST_RING_IDX)) {
			ring_params->msi_addr = 0;
			ring_params->msi_data = 0;
			ring_params->flags &= ~HAL_SRNG_MSI_INTR;
		}
	}
}
#else
static inline void
dp_ipa_vlan_srng_msi_setup(struct hal_srng_params *ring_params, int ring_type,
			   int ring_num)
{
}
#endif

void dp_srng_msi_setup(struct dp_soc *soc, struct dp_srng *srng,
		       struct hal_srng_params *ring_params,
		       int ring_type, int ring_num)
{
	int reg_msi_grp_num;
	/*
	 * nf_msi_grp_num needs to be initialized with negative value,
	 * to avoid configuring near-full msi for WBM2SW3 ring
	 */
	int nf_msi_grp_num = -1;
	int msi_data_count;
	int ret;
	uint32_t msi_data_start, msi_irq_start, addr_low, addr_high;
	bool nf_irq_support;
	int vector;

	ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
					  &msi_data_count, &msi_data_start,
					  &msi_irq_start);

	if (ret)
		return;

	nf_irq_support = hal_srng_is_near_full_irq_supported(soc->hal_soc,
							     ring_type,
							     ring_num);
	ret = dp_srng_calculate_msi_group(soc, ring_type, ring_num,
					  &reg_msi_grp_num,
					  nf_irq_support,
					  &nf_msi_grp_num);
	if (ret < 0) {
		dp_init_info("%pK: ring not part of an ext_group; ring_type: %d,ring_num %d",
			     soc, ring_type, ring_num);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		dp_srng_set_msi2_ring_params(soc, ring_params, 0, 0);
		return;
	}

	if (reg_msi_grp_num < 0) {
		dp_init_info("%pK: ring not part of an ext_group; ring_type: %d,ring_num %d",
			     soc, ring_type, ring_num);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		goto configure_msi2;
	}

	if (dp_is_msi_group_number_invalid(soc, reg_msi_grp_num,
					   msi_data_count)) {
		dp_init_warn("%pK: 2 msi_groups will share an msi; msi_group_num %d",
			     soc, reg_msi_grp_num);
		QDF_ASSERT(0);
	}

	pld_get_msi_address(soc->osdev->dev, &addr_low, &addr_high);

	ring_params->msi_addr = addr_low;
	ring_params->msi_addr |= (qdf_dma_addr_t)(((uint64_t)addr_high) << 32);
	ring_params->msi_data = (reg_msi_grp_num % msi_data_count)
		+ msi_data_start;
	ring_params->flags |= HAL_SRNG_MSI_INTR;

	dp_ipa_vlan_srng_msi_setup(ring_params, ring_type, ring_num);

	dp_debug("ring type %u ring_num %u msi->data %u msi_addr %llx",
		 ring_type, ring_num, ring_params->msi_data,
		 (uint64_t)ring_params->msi_addr);

	vector = msi_irq_start + (reg_msi_grp_num % msi_data_count);

	/*
	 * During umac reset ppeds interrupts free is not called.
	 * Avoid registering interrupts again.
	 *
	 */
	if (dp_check_umac_reset_in_progress(soc))
		goto configure_msi2;

	if (soc->arch_ops.dp_register_ppeds_interrupts)
		if (soc->arch_ops.dp_register_ppeds_interrupts(soc, srng,
							       vector,
							       ring_type,
							       ring_num))
			return;

configure_msi2:
	if (!nf_irq_support) {
		dp_srng_set_msi2_ring_params(soc, ring_params, 0, 0);
		return;
	}

	dp_srng_msi2_setup(soc, ring_params, ring_type, ring_num,
			   nf_msi_grp_num);
}

#ifdef WLAN_DP_PER_RING_TYPE_CONFIG
/**
 * dp_srng_configure_interrupt_thresholds() - Retrieve interrupt
 * threshold values from the wlan_srng_cfg table for each ring type
 * @soc: device handle
 * @ring_params: per ring specific parameters
 * @ring_type: Ring type
 * @ring_num: Ring number for a given ring type
 * @num_entries: number of entries to fill
 *
 * Fill the ring params with the interrupt threshold
 * configuration parameters available in the per ring type wlan_srng_cfg
 * table.
 *
 * Return: None
 */
void
dp_srng_configure_interrupt_thresholds(struct dp_soc *soc,
				       struct hal_srng_params *ring_params,
				       int ring_type, int ring_num,
				       int num_entries)
{
	uint8_t wbm2_sw_rx_rel_ring_id;

	wbm2_sw_rx_rel_ring_id = wlan_cfg_get_rx_rel_ring_id(soc->wlan_cfg_ctx);

	if (ring_type == REO_DST) {
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_rx(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
			wlan_cfg_get_int_batch_threshold_rx(soc->wlan_cfg_ctx);
	} else if (ring_type == WBM2SW_RELEASE &&
		   (ring_num == wbm2_sw_rx_rel_ring_id)) {
		ring_params->intr_timer_thres_us =
				wlan_cfg_get_int_timer_threshold_other(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
				wlan_cfg_get_int_batch_threshold_other(soc->wlan_cfg_ctx);
	} else {
		ring_params->intr_timer_thres_us =
				soc->wlan_srng_cfg[ring_type].timer_threshold;
		ring_params->intr_batch_cntr_thres_entries =
				soc->wlan_srng_cfg[ring_type].batch_count_threshold;
	}
	ring_params->low_threshold =
			soc->wlan_srng_cfg[ring_type].low_threshold;
	if (ring_params->low_threshold)
		ring_params->flags |= HAL_SRNG_LOW_THRES_INTR_ENABLE;

	dp_srng_configure_nf_interrupt_thresholds(soc, ring_params, ring_type);
}
#else
void
dp_srng_configure_interrupt_thresholds(struct dp_soc *soc,
				       struct hal_srng_params *ring_params,
				       int ring_type, int ring_num,
				       int num_entries)
{
	uint8_t wbm2_sw_rx_rel_ring_id;
	bool rx_refill_lt_disable;

	wbm2_sw_rx_rel_ring_id = wlan_cfg_get_rx_rel_ring_id(soc->wlan_cfg_ctx);

	if (ring_type == REO_DST || ring_type == REO2PPE) {
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_rx(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
			wlan_cfg_get_int_batch_threshold_rx(soc->wlan_cfg_ctx);
	} else if (ring_type == WBM2SW_RELEASE &&
		   (ring_num < wbm2_sw_rx_rel_ring_id ||
		   ring_num == WBM2SW_TXCOMP_RING4_NUM ||
		   ring_num == WBM2_SW_PPE_REL_RING_ID)) {
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_tx(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
			wlan_cfg_get_int_batch_threshold_tx(soc->wlan_cfg_ctx);
	} else if (ring_type == RXDMA_BUF) {
		rx_refill_lt_disable =
			wlan_cfg_get_dp_soc_rxdma_refill_lt_disable
							(soc->wlan_cfg_ctx);
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_rx(soc->wlan_cfg_ctx);

		if (!rx_refill_lt_disable) {
			ring_params->low_threshold = num_entries >> 3;
			ring_params->flags |= HAL_SRNG_LOW_THRES_INTR_ENABLE;
			ring_params->intr_batch_cntr_thres_entries = 0;
		}
	} else {
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_other(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
			wlan_cfg_get_int_batch_threshold_other(soc->wlan_cfg_ctx);
	}

	/* These rings donot require interrupt to host. Make them zero */
	switch (ring_type) {
	case REO_REINJECT:
	case REO_CMD:
	case TCL_DATA:
	case TCL_CMD_CREDIT:
	case TCL_STATUS:
	case WBM_IDLE_LINK:
	case SW2WBM_RELEASE:
	case SW2RXDMA_NEW:
		ring_params->intr_timer_thres_us = 0;
		ring_params->intr_batch_cntr_thres_entries = 0;
		break;
	case PPE2TCL:
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_ppe2tcl(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
			wlan_cfg_get_int_batch_threshold_ppe2tcl(soc->wlan_cfg_ctx);
		break;
	case RXDMA_MONITOR_DST:
		ring_params->intr_timer_thres_us =
		  wlan_cfg_get_int_timer_threshold_mon_dest(soc->wlan_cfg_ctx);
		ring_params->intr_batch_cntr_thres_entries =
		  wlan_cfg_get_int_batch_threshold_mon_dest(soc->wlan_cfg_ctx);
		break;
	}

	/* Enable low threshold interrupts for rx buffer rings (regular and
	 * monitor buffer rings.
	 * TODO: See if this is required for any other ring
	 */
	if ((ring_type == RXDMA_MONITOR_BUF) ||
	    (ring_type == RXDMA_MONITOR_STATUS ||
	    (ring_type == TX_MONITOR_BUF))) {
		/* TODO: Setting low threshold to 1/8th of ring size
		 * see if this needs to be configurable
		 */
		ring_params->low_threshold = num_entries >> 3;
		ring_params->intr_timer_thres_us =
			wlan_cfg_get_int_timer_threshold_rx(soc->wlan_cfg_ctx);
		ring_params->flags |= HAL_SRNG_LOW_THRES_INTR_ENABLE;
		ring_params->intr_batch_cntr_thres_entries = 0;
	}

	/* During initialisation monitor rings are only filled with
	 * MON_BUF_MIN_ENTRIES entries. So low threshold needs to be set to
	 * a value less than that. Low threshold value is reconfigured again
	 * to 1/8th of the ring size when monitor vap is created.
	 */
	if (ring_type == RXDMA_MONITOR_BUF)
		ring_params->low_threshold = MON_BUF_MIN_ENTRIES >> 1;

	/* In case of PCI chipsets, we dont have PPDU end interrupts,
	 * so MONITOR STATUS ring is reaped by receiving MSI from srng.
	 * Keep batch threshold as 8 so that interrupt is received for
	 * every 4 packets in MONITOR_STATUS ring
	 */
	if ((ring_type == RXDMA_MONITOR_STATUS) &&
	    (soc->intr_mode == DP_INTR_MSI))
		ring_params->intr_batch_cntr_thres_entries = 4;
}
#endif

static int dp_process_rxdma_dst_ring(struct dp_soc *soc,
				     struct dp_intr *int_ctx,
				     int mac_for_pdev,
				     int total_budget)
{
	uint32_t target_type;

	target_type = hal_get_target_type(soc->hal_soc);
	if (target_type == TARGET_TYPE_QCN9160)
		return dp_monitor_process(soc, int_ctx,
					  mac_for_pdev, total_budget);
	else
		return dp_rxdma_err_process(int_ctx, soc, mac_for_pdev,
					    total_budget);
}

/**
 * dp_process_lmac_rings() - Process LMAC rings
 * @int_ctx: interrupt context
 * @total_budget: budget of work which can be done
 *
 * Return: work done
 */
int dp_process_lmac_rings(struct dp_intr *int_ctx, int total_budget)
{
	struct dp_intr_stats *intr_stats = &int_ctx->intr_stats;
	struct dp_soc *soc = int_ctx->soc;
	uint32_t remaining_quota = total_budget;
	struct dp_pdev *pdev = NULL;
	uint32_t work_done  = 0;
	int budget = total_budget;
	int ring = 0;
	bool rx_refill_lt_disable;

	rx_refill_lt_disable =
		wlan_cfg_get_dp_soc_rxdma_refill_lt_disable(soc->wlan_cfg_ctx);

	/* Process LMAC interrupts */
	for  (ring = 0 ; ring < MAX_NUM_LMAC_HW; ring++) {
		int mac_for_pdev = ring;

		pdev = dp_get_pdev_for_lmac_id(soc, mac_for_pdev);
		if (!pdev)
			continue;
		if (int_ctx->rx_mon_ring_mask & (1 << mac_for_pdev)) {
			work_done = dp_monitor_process(soc, int_ctx,
						       mac_for_pdev,
						       remaining_quota);
			if (work_done)
				intr_stats->num_rx_mon_ring_masks++;
			budget -= work_done;
			if (budget <= 0)
				goto budget_done;
			remaining_quota = budget;
		}

		if (int_ctx->tx_mon_ring_mask & (1 << mac_for_pdev)) {
			work_done = dp_tx_mon_process(soc, int_ctx,
						      mac_for_pdev,
						      remaining_quota);
			if (work_done)
				intr_stats->num_tx_mon_ring_masks++;
			budget -= work_done;
			if (budget <= 0)
				goto budget_done;
			remaining_quota = budget;
		}

		if (int_ctx->rxdma2host_ring_mask &
				(1 << mac_for_pdev)) {
			work_done = dp_process_rxdma_dst_ring(soc, int_ctx,
							      mac_for_pdev,
							      remaining_quota);
			if (work_done)
				intr_stats->num_rxdma2host_ring_masks++;
			budget -=  work_done;
			if (budget <= 0)
				goto budget_done;
			remaining_quota = budget;
		}

		if (int_ctx->host2rxdma_ring_mask & (1 << mac_for_pdev)) {
			struct dp_srng *rx_refill_buf_ring;
			struct rx_desc_pool *rx_desc_pool;

			rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];
			if (wlan_cfg_per_pdev_lmac_ring(soc->wlan_cfg_ctx))
				rx_refill_buf_ring =
					&soc->rx_refill_buf_ring[mac_for_pdev];
			else
				rx_refill_buf_ring =
					&soc->rx_refill_buf_ring[pdev->lmac_id];

			intr_stats->num_host2rxdma_ring_masks++;

			if (!rx_refill_lt_disable)
				dp_rx_buffers_lt_replenish_simple
							(soc, mac_for_pdev,
							 rx_refill_buf_ring,
							 rx_desc_pool,
							 false);
		}
	}

	if (int_ctx->host2rxdma_mon_ring_mask)
		dp_rx_mon_buf_refill(int_ctx);

	if (int_ctx->host2txmon_ring_mask)
		dp_tx_mon_buf_refill(int_ctx);

budget_done:
	return total_budget - budget;
}

uint32_t dp_service_srngs_wrapper(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	struct dp_intr *int_ctx = (struct dp_intr *)dp_ctx;
	struct dp_soc *soc = int_ctx->soc;

	return soc->arch_ops.dp_service_srngs(dp_ctx, dp_budget, cpu);
}

#ifdef QCA_SUPPORT_LEGACY_INTERRUPTS
/**
 * dp_soc_interrupt_map_calculate_wifi3_pci_legacy() -
 * Calculate interrupt map for legacy interrupts
 * @soc: DP soc handle
 * @intr_ctx_num: Interrupt context number
 * @irq_id_map: IRQ map
 * @num_irq_r: Number of interrupts assigned for this context
 *
 * Return: void
 */
static void dp_soc_interrupt_map_calculate_wifi3_pci_legacy(struct dp_soc *soc,
							    int intr_ctx_num,
							    int *irq_id_map,
							    int *num_irq_r)
{
	int j;
	int num_irq = 0;
	int tx_mask = wlan_cfg_get_tx_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_mask = wlan_cfg_get_rx_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_mon_mask = wlan_cfg_get_rx_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_err_ring_mask = wlan_cfg_get_rx_err_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_wbm_rel_ring_mask = wlan_cfg_get_rx_wbm_rel_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int reo_status_ring_mask = wlan_cfg_get_reo_status_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rxdma2host_ring_mask = wlan_cfg_get_rxdma2host_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2rxdma_ring_mask = wlan_cfg_get_host2rxdma_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2rxdma_mon_ring_mask = wlan_cfg_get_host2rxdma_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2txmon_ring_mask = wlan_cfg_get_host2txmon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int txmon2host_mon_ring_mask = wlan_cfg_get_tx_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	soc->intr_mode = DP_INTR_LEGACY_VIRTUAL_IRQ;
	for (j = 0; j < HIF_MAX_GRP_IRQ; j++) {
		if (tx_mask & (1 << j))
			irq_id_map[num_irq++] = (wbm2sw0_release - j);
		if (rx_mask & (1 << j))
			irq_id_map[num_irq++] = (reo2sw1_intr - j);
		if (rx_mon_mask & (1 << j))
			irq_id_map[num_irq++] = (rxmon2sw_p0_dest0 - j);
		if (rx_err_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (reo2sw0_intr - j);
		if (rx_wbm_rel_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (wbm2sw5_release - j);
		if (reo_status_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (reo_status - j);
		if (rxdma2host_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (rxdma2sw_dst_ring0 - j);
		if (host2rxdma_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (sw2rxdma_0 - j);
		if (host2rxdma_mon_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (sw2rxmon_src_ring - j);
		if (host2txmon_ring_mask & (1 << j))
			irq_id_map[num_irq++] = sw2txmon_src_ring;
		if (txmon2host_mon_ring_mask & (1 << j))
			irq_id_map[num_irq++] = (txmon2sw_p0_dest0 - j);
	}
	*num_irq_r = num_irq;
}
#else
static void dp_soc_interrupt_map_calculate_wifi3_pci_legacy(struct dp_soc *soc,
							    int intr_ctx_num,
							    int *irq_id_map,
							    int *num_irq_r)
{
}
#endif

static void
dp_soc_interrupt_map_calculate_integrated(struct dp_soc *soc, int intr_ctx_num,
					  int *irq_id_map, int *num_irq_r)
{
	int j;
	int num_irq = 0;

	int tx_mask =
		wlan_cfg_get_tx_ring_mask(soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_mask =
		wlan_cfg_get_rx_ring_mask(soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_mon_mask =
		wlan_cfg_get_rx_mon_ring_mask(soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_err_ring_mask = wlan_cfg_get_rx_err_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_wbm_rel_ring_mask = wlan_cfg_get_rx_wbm_rel_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int reo_status_ring_mask = wlan_cfg_get_reo_status_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rxdma2host_ring_mask = wlan_cfg_get_rxdma2host_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2rxdma_ring_mask = wlan_cfg_get_host2rxdma_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2rxdma_mon_ring_mask = wlan_cfg_get_host2rxdma_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2txmon_ring_mask = wlan_cfg_get_host2txmon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int txmon2host_mon_ring_mask = wlan_cfg_get_tx_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);

	soc->intr_mode = DP_INTR_INTEGRATED;

	for (j = 0; j < HIF_MAX_GRP_IRQ; j++) {
		if (tx_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				(wbm2host_tx_completions_ring1 - j);
		}

		if (rx_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				(reo2host_destination_ring1 - j);
		}

		if (rxdma2host_ring_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				rxdma2host_destination_ring_mac1 - j;
		}

		if (host2rxdma_ring_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				host2rxdma_host_buf_ring_mac1 -	j;
		}

		if (host2rxdma_mon_ring_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				host2rxdma_monitor_ring1 - j;
		}

		if (rx_mon_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				ppdu_end_interrupts_mac1 - j;
			irq_id_map[num_irq++] =
				rxdma2host_monitor_status_ring_mac1 - j;
			irq_id_map[num_irq++] =
				rxdma2host_monitor_destination_mac1 - j;
		}

		if (rx_wbm_rel_ring_mask & (1 << j))
			irq_id_map[num_irq++] = wbm2host_rx_release;

		if (rx_err_ring_mask & (1 << j))
			irq_id_map[num_irq++] = reo2host_exception;

		if (reo_status_ring_mask & (1 << j))
			irq_id_map[num_irq++] = reo2host_status;

		if (host2txmon_ring_mask & (1 << j))
			irq_id_map[num_irq++] = host2tx_monitor_ring1;

		if (txmon2host_mon_ring_mask & (1 << j)) {
			irq_id_map[num_irq++] =
				(txmon2host_monitor_destination_mac1 - j);
		}
	}
	*num_irq_r = num_irq;
}

static void
dp_soc_interrupt_map_calculate_msi(struct dp_soc *soc, int intr_ctx_num,
				   int *irq_id_map, int *num_irq_r,
				   int msi_vector_count, int msi_vector_start)
{
	int tx_mask = wlan_cfg_get_tx_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_mask = wlan_cfg_get_rx_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_mon_mask = wlan_cfg_get_rx_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int tx_mon_mask = wlan_cfg_get_tx_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_err_ring_mask = wlan_cfg_get_rx_err_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_wbm_rel_ring_mask = wlan_cfg_get_rx_wbm_rel_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int reo_status_ring_mask = wlan_cfg_get_reo_status_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rxdma2host_ring_mask = wlan_cfg_get_rxdma2host_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2rxdma_ring_mask = wlan_cfg_get_host2rxdma_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int host2rxdma_mon_ring_mask = wlan_cfg_get_host2rxdma_mon_ring_mask(
					soc->wlan_cfg_ctx, intr_ctx_num);
	int rx_near_full_grp_1_mask =
		wlan_cfg_get_rx_near_full_grp_1_mask(soc->wlan_cfg_ctx,
						     intr_ctx_num);
	int rx_near_full_grp_2_mask =
		wlan_cfg_get_rx_near_full_grp_2_mask(soc->wlan_cfg_ctx,
						     intr_ctx_num);
	int tx_ring_near_full_mask =
		wlan_cfg_get_tx_ring_near_full_mask(soc->wlan_cfg_ctx,
						    intr_ctx_num);

	int host2txmon_ring_mask =
		wlan_cfg_get_host2txmon_ring_mask(soc->wlan_cfg_ctx,
						  intr_ctx_num);
	unsigned int vector =
		(intr_ctx_num % msi_vector_count) + msi_vector_start;
	int num_irq = 0;

	soc->intr_mode = DP_INTR_MSI;

	if (tx_mask | rx_mask | rx_mon_mask | tx_mon_mask | rx_err_ring_mask |
	    rx_wbm_rel_ring_mask | reo_status_ring_mask | rxdma2host_ring_mask |
	    host2rxdma_ring_mask | host2rxdma_mon_ring_mask |
	    rx_near_full_grp_1_mask | rx_near_full_grp_2_mask |
	    tx_ring_near_full_mask | host2txmon_ring_mask)
		irq_id_map[num_irq++] =
			pld_get_msi_irq(soc->osdev->dev, vector);

	*num_irq_r = num_irq;
}

void dp_soc_interrupt_map_calculate(struct dp_soc *soc, int intr_ctx_num,
				    int *irq_id_map, int *num_irq)
{
	int msi_vector_count, ret;
	uint32_t msi_base_data, msi_vector_start;

	if (pld_get_enable_intx(soc->osdev->dev)) {
		return dp_soc_interrupt_map_calculate_wifi3_pci_legacy(soc,
				intr_ctx_num, irq_id_map, num_irq);
	}

	ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
					  &msi_vector_count,
					  &msi_base_data,
					  &msi_vector_start);
	if (ret)
		return dp_soc_interrupt_map_calculate_integrated(soc,
				intr_ctx_num, irq_id_map, num_irq);

	else
		dp_soc_interrupt_map_calculate_msi(soc,
				intr_ctx_num, irq_id_map, num_irq,
				msi_vector_count, msi_vector_start);
}

void dp_srng_free(struct dp_soc *soc, struct dp_srng *srng)
{
	if (srng->alloc_size && srng->base_vaddr_unaligned) {
		if (!srng->cached) {
			dp_srng_mem_free_consistent(soc, srng);
		} else {
			qdf_mem_free(srng->base_vaddr_unaligned);
		}
		srng->alloc_size = 0;
		srng->base_vaddr_unaligned = NULL;
	}
	srng->hal_srng = NULL;
}

qdf_export_symbol(dp_srng_free);

QDF_STATUS dp_srng_init(struct dp_soc *soc, struct dp_srng *srng, int ring_type,
			int ring_num, int mac_id)
{
	return soc->arch_ops.txrx_srng_init(soc, srng, ring_type,
					    ring_num, mac_id);
}

qdf_export_symbol(dp_srng_init);

QDF_STATUS dp_srng_alloc(struct dp_soc *soc, struct dp_srng *srng,
			 int ring_type, uint32_t num_entries,
			 bool cached)
{
	hal_soc_handle_t hal_soc = soc->hal_soc;
	uint32_t entry_size = hal_srng_get_entrysize(hal_soc, ring_type);
	uint32_t max_entries = hal_srng_max_entries(hal_soc, ring_type);

	if (srng->base_vaddr_unaligned) {
		dp_init_err("%pK: Ring type: %d, is already allocated",
			    soc, ring_type);
		return QDF_STATUS_SUCCESS;
	}

	num_entries = (num_entries > max_entries) ? max_entries : num_entries;
	srng->hal_srng = NULL;
	srng->alloc_size = num_entries * entry_size;
	srng->num_entries = num_entries;
	srng->cached = cached;

	if (!cached) {
		srng->base_vaddr_aligned =
		    dp_srng_aligned_mem_alloc_consistent(soc,
							 srng,
							 ring_type);
	} else {
		srng->base_vaddr_aligned = qdf_aligned_malloc(
					&srng->alloc_size,
					&srng->base_vaddr_unaligned,
					&srng->base_paddr_unaligned,
					&srng->base_paddr_aligned,
					DP_RING_BASE_ALIGN);
	}

	if (!srng->base_vaddr_aligned)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_srng_alloc);

void dp_srng_deinit(struct dp_soc *soc, struct dp_srng *srng,
		    int ring_type, int ring_num)
{
	if (!srng->hal_srng) {
		dp_init_err("%pK: Ring type: %d, num:%d not setup",
			    soc, ring_type, ring_num);
		return;
	}

	if (dp_check_umac_reset_in_progress(soc))
		goto srng_cleanup;

	if (soc->arch_ops.dp_free_ppeds_interrupts)
		soc->arch_ops.dp_free_ppeds_interrupts(soc, srng, ring_type,
						       ring_num);

srng_cleanup:
	hal_srng_cleanup(soc->hal_soc, srng->hal_srng,
			 dp_check_umac_reset_in_progress(soc));
	srng->hal_srng = NULL;
}

qdf_export_symbol(dp_srng_deinit);

/* TODO: Need this interface from HIF */
void *hif_get_hal_handle(struct hif_opaque_softc *hif_handle);

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
int dp_srng_access_start(struct dp_intr *int_ctx, struct dp_soc *dp_soc,
			 hal_ring_handle_t hal_ring_hdl)
{
	hal_soc_handle_t hal_soc = dp_soc->hal_soc;
	uint32_t hp, tp;
	uint8_t ring_id;

	if (!int_ctx)
		return dp_hal_srng_access_start(hal_soc, hal_ring_hdl);

	hal_get_sw_hptp(hal_soc, hal_ring_hdl, &tp, &hp);
	ring_id = hal_srng_ring_id_get(hal_ring_hdl);

	hif_record_event(dp_soc->hif_handle, int_ctx->dp_intr_id,
			 ring_id, hp, tp, HIF_EVENT_SRNG_ACCESS_START);

	return dp_hal_srng_access_start(hal_soc, hal_ring_hdl);
}

void dp_srng_access_end(struct dp_intr *int_ctx, struct dp_soc *dp_soc,
			hal_ring_handle_t hal_ring_hdl)
{
	hal_soc_handle_t hal_soc = dp_soc->hal_soc;
	uint32_t hp, tp;
	uint8_t ring_id;

	if (!int_ctx)
		return dp_hal_srng_access_end(hal_soc, hal_ring_hdl);

	hal_get_sw_hptp(hal_soc, hal_ring_hdl, &tp, &hp);
	ring_id = hal_srng_ring_id_get(hal_ring_hdl);

	hif_record_event(dp_soc->hif_handle, int_ctx->dp_intr_id,
			 ring_id, hp, tp, HIF_EVENT_SRNG_ACCESS_END);

	return dp_hal_srng_access_end(hal_soc, hal_ring_hdl);
}

static inline void dp_srng_record_timer_entry(struct dp_soc *dp_soc,
					      uint8_t hist_group_id)
{
	hif_record_event(dp_soc->hif_handle, hist_group_id,
			 0, 0, 0, HIF_EVENT_TIMER_ENTRY);
}

static inline void dp_srng_record_timer_exit(struct dp_soc *dp_soc,
					     uint8_t hist_group_id)
{
	hif_record_event(dp_soc->hif_handle, hist_group_id,
			 0, 0, 0, HIF_EVENT_TIMER_EXIT);
}
#else

static inline void dp_srng_record_timer_entry(struct dp_soc *dp_soc,
					      uint8_t hist_group_id)
{
}

static inline void dp_srng_record_timer_exit(struct dp_soc *dp_soc,
					     uint8_t hist_group_id)
{
}

#endif /* WLAN_FEATURE_DP_EVENT_HISTORY */

enum timer_yield_status
dp_should_timer_irq_yield(struct dp_soc *soc, uint32_t work_done,
			  uint64_t start_time)
{
	uint64_t cur_time = qdf_get_log_timestamp();

	if (!work_done)
		return DP_TIMER_WORK_DONE;

	if (cur_time - start_time > DP_MAX_TIMER_EXEC_TIME_TICKS)
		return DP_TIMER_TIME_EXHAUST;

	return DP_TIMER_NO_YIELD;
}

qdf_export_symbol(dp_should_timer_irq_yield);

void dp_interrupt_timer(void *arg)
{
	struct dp_soc *soc = (struct dp_soc *) arg;
	struct dp_pdev *pdev = soc->pdev_list[0];
	enum timer_yield_status yield = DP_TIMER_NO_YIELD;
	uint32_t work_done  = 0, total_work_done = 0;
	int budget = 0xffff, i;
	uint32_t remaining_quota = budget;
	uint64_t start_time;
	uint32_t lmac_id = DP_MON_INVALID_LMAC_ID;
	uint8_t dp_intr_id = wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx);
	uint32_t lmac_iter;
	int max_mac_rings = wlan_cfg_get_num_mac_rings(pdev->wlan_cfg_ctx);
	enum reg_wifi_band mon_band;
	int cpu = dp_srng_get_cpu();

	/*
	 * this logic makes all data path interfacing rings (UMAC/LMAC)
	 * and Monitor rings polling mode when NSS offload is disabled
	 */
	if (wlan_cfg_is_poll_mode_enabled(soc->wlan_cfg_ctx) &&
	    !wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx)) {
		if (qdf_atomic_read(&soc->cmn_init_done)) {
			for (i = 0; i < wlan_cfg_get_num_contexts(
						soc->wlan_cfg_ctx); i++)
				soc->arch_ops.dp_service_srngs(&soc->intr_ctx[i], 0xffff,
							       cpu);

			qdf_timer_mod(&soc->int_timer, DP_INTR_POLL_TIMER_MS);
		}
		return;
	}

	if (!qdf_atomic_read(&soc->cmn_init_done))
		return;

	if (dp_monitor_is_chan_band_known(pdev)) {
		mon_band = dp_monitor_get_chan_band(pdev);
		lmac_id = pdev->ch_band_lmac_id_mapping[mon_band];
		if (qdf_likely(lmac_id != DP_MON_INVALID_LMAC_ID)) {
			dp_intr_id = soc->mon_intr_id_lmac_map[lmac_id];
			dp_srng_record_timer_entry(soc, dp_intr_id);
		}
	}

	start_time = qdf_get_log_timestamp();
	dp_update_num_mac_rings_for_dbs(soc, &max_mac_rings);

	while (yield == DP_TIMER_NO_YIELD) {
		for (lmac_iter = 0; lmac_iter < max_mac_rings; lmac_iter++) {
			if (lmac_iter == lmac_id)
				work_done = dp_monitor_process(soc,
						&soc->intr_ctx[dp_intr_id],
						lmac_iter, remaining_quota);
			else
				work_done =
					dp_monitor_drop_packets_for_mac(pdev,
							     lmac_iter,
							     remaining_quota);
			if (work_done) {
				budget -=  work_done;
				if (budget <= 0) {
					yield = DP_TIMER_WORK_EXHAUST;
					goto budget_done;
				}
				remaining_quota = budget;
				total_work_done += work_done;
			}
		}

		yield = dp_should_timer_irq_yield(soc, total_work_done,
						  start_time);
		total_work_done = 0;
	}

budget_done:
	if (yield == DP_TIMER_WORK_EXHAUST ||
	    yield == DP_TIMER_TIME_EXHAUST)
		qdf_timer_mod(&soc->int_timer, 1);
	else
		qdf_timer_mod(&soc->int_timer, DP_INTR_POLL_TIMER_MS);

	if (lmac_id != DP_MON_INVALID_LMAC_ID)
		dp_srng_record_timer_exit(soc, dp_intr_id);
}

/**
 * dp_soc_interrupt_detach_wrapper() - wrapper function for interrupt detach
 * @txrx_soc: DP SOC handle
 *
 * Return: None
 */
static void dp_soc_interrupt_detach_wrapper(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	return soc->arch_ops.dp_soc_interrupt_detach(txrx_soc);
}

#if defined(DP_INTR_POLL_BOTH)
/**
 * dp_soc_interrupt_attach_wrapper() - Register handlers for DP interrupts
 * @txrx_soc: DP SOC handle
 *
 * Call the appropriate attach function based on the mode of operation.
 * This is a WAR for enabling monitor mode.
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS dp_soc_interrupt_attach_wrapper(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	if (!(soc->wlan_cfg_ctx->napi_enabled) ||
	    (dp_is_monitor_mode_using_poll(soc) &&
	     soc->cdp_soc.ol_ops->get_con_mode &&
	     soc->cdp_soc.ol_ops->get_con_mode() ==
	     QDF_GLOBAL_MONITOR_MODE)) {
		dp_info("Poll mode");
		return soc->arch_ops.dp_soc_attach_poll(txrx_soc);
	} else {
		dp_info("Interrupt  mode");
		return soc->arch_ops.dp_soc_interrupt_attach(txrx_soc);
	}
}
#else
#if defined(DP_INTR_POLL_BASED) && DP_INTR_POLL_BASED
static QDF_STATUS dp_soc_interrupt_attach_wrapper(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	return soc->arch_ops.dp_soc_attach_poll(txrx_soc);
}
#else
static QDF_STATUS dp_soc_interrupt_attach_wrapper(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	if (wlan_cfg_is_poll_mode_enabled(soc->wlan_cfg_ctx))
		return soc->arch_ops.dp_soc_attach_poll(txrx_soc);
	else
		return soc->arch_ops.dp_soc_interrupt_attach(txrx_soc);
}
#endif
#endif

void dp_link_desc_ring_replenish(struct dp_soc *soc, uint32_t mac_id)
{
	uint32_t cookie = 0;
	uint32_t page_idx = 0;
	struct qdf_mem_multi_page_t *pages;
	struct qdf_mem_dma_page_t *dma_pages;
	uint32_t offset = 0;
	uint32_t count = 0;
	uint32_t desc_id = 0;
	void *desc_srng;
	int link_desc_size = hal_get_link_desc_size(soc->hal_soc);
	uint32_t *total_link_descs_addr;
	uint32_t total_link_descs;
	uint32_t scatter_buf_num;
	uint32_t num_entries_per_buf = 0;
	uint32_t rem_entries;
	uint32_t num_descs_per_page;
	uint32_t num_scatter_bufs = 0;
	uint8_t *scatter_buf_ptr;
	void *desc;

	num_scatter_bufs = soc->num_scatter_bufs;

	if (mac_id == WLAN_INVALID_PDEV_ID) {
		pages = &soc->link_desc_pages;
		total_link_descs = soc->total_link_descs;
		desc_srng = soc->wbm_idle_link_ring.hal_srng;
	} else {
		pages = dp_monitor_get_link_desc_pages(soc, mac_id);
		/* dp_monitor_get_link_desc_pages returns NULL only
		 * if monitor SOC is  NULL
		 */
		if (!pages) {
			dp_err("can not get link desc pages");
			QDF_ASSERT(0);
			return;
		}
		total_link_descs_addr =
				dp_monitor_get_total_link_descs(soc, mac_id);
		total_link_descs = *total_link_descs_addr;
		desc_srng = dp_monitor_get_link_desc_ring(soc, mac_id);
	}

	dma_pages = pages->dma_pages;
	do {
		qdf_mem_zero(dma_pages[page_idx].page_v_addr_start,
			     pages->page_size);
		page_idx++;
	} while (page_idx < pages->num_pages);

	if (desc_srng) {
		hal_srng_access_start_unlocked(soc->hal_soc, desc_srng);
		page_idx = 0;
		count = 0;
		offset = 0;

		qdf_assert(pages->num_element_per_page != 0);
		while ((desc = hal_srng_src_get_next(soc->hal_soc,
						     desc_srng)) &&
			(count < total_link_descs)) {
			page_idx = count / pages->num_element_per_page;
			if (desc_id == pages->num_element_per_page)
				desc_id = 0;

			offset = count % pages->num_element_per_page;
			cookie = LINK_DESC_COOKIE(desc_id, page_idx,
						  soc->link_desc_id_start);

			hal_set_link_desc_addr(soc->hal_soc, desc, cookie,
					       dma_pages[page_idx].page_p_addr
					       + (offset * link_desc_size),
					       soc->idle_link_bm_id);
			count++;
			desc_id++;
		}
		hal_srng_access_end_unlocked(soc->hal_soc, desc_srng);
	} else {
		/* Populate idle list scatter buffers with link descriptor
		 * pointers
		 */
		scatter_buf_num = 0;
		num_entries_per_buf = hal_idle_scatter_buf_num_entries(
					soc->hal_soc,
					soc->wbm_idle_scatter_buf_size);

		scatter_buf_ptr = (uint8_t *)(
			soc->wbm_idle_scatter_buf_base_vaddr[scatter_buf_num]);
		rem_entries = num_entries_per_buf;
		page_idx = 0; count = 0;
		offset = 0;
		num_descs_per_page = pages->num_element_per_page;

		qdf_assert(num_descs_per_page != 0);
		while (count < total_link_descs) {
			page_idx = count / num_descs_per_page;
			offset = count % num_descs_per_page;
			if (desc_id == pages->num_element_per_page)
				desc_id = 0;

			cookie = LINK_DESC_COOKIE(desc_id, page_idx,
						  soc->link_desc_id_start);
			hal_set_link_desc_addr(soc->hal_soc,
					       (void *)scatter_buf_ptr,
					       cookie,
					       dma_pages[page_idx].page_p_addr +
					       (offset * link_desc_size),
					       soc->idle_link_bm_id);
			rem_entries--;
			if (rem_entries) {
				scatter_buf_ptr += link_desc_size;
			} else {
				rem_entries = num_entries_per_buf;
				scatter_buf_num++;
				if (scatter_buf_num >= num_scatter_bufs) {
					scatter_buf_num--;
					break;
				}

				scatter_buf_ptr = (uint8_t *)
					(soc->wbm_idle_scatter_buf_base_vaddr[
					 scatter_buf_num]);
			}
			count++;
			desc_id++;
		}
		/* Setup link descriptor idle list in HW */
		hal_setup_link_idle_list(soc->hal_soc,
			soc->wbm_idle_scatter_buf_base_paddr,
			soc->wbm_idle_scatter_buf_base_vaddr,
			num_scatter_bufs, soc->wbm_idle_scatter_buf_size,
			(uint32_t)(scatter_buf_ptr -
			(uint8_t *)(soc->wbm_idle_scatter_buf_base_vaddr[
			scatter_buf_num])), total_link_descs);
	}
}

qdf_export_symbol(dp_link_desc_ring_replenish);

/**
 * dp_soc_ppeds_stop() - Stop PPE DS processing
 * @soc_handle: DP SOC handle
 *
 * Return: none
 */
static void dp_soc_ppeds_stop(struct cdp_soc_t *soc_handle)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	if (soc->arch_ops.txrx_soc_ppeds_stop)
		soc->arch_ops.txrx_soc_ppeds_stop(soc);
}

#ifdef ENABLE_VERBOSE_DEBUG
void dp_enable_verbose_debug(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	if (soc_cfg_ctx->per_pkt_trace & dp_verbose_debug_mask)
		is_dp_verbose_debug_enabled = true;

	if (soc_cfg_ctx->per_pkt_trace & hal_verbose_debug_mask)
		hal_set_verbose_debug(true);
	else
		hal_set_verbose_debug(false);
}
#else
void dp_enable_verbose_debug(struct dp_soc *soc)
{
}
#endif

static QDF_STATUS dp_lro_hash_setup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct cdp_lro_hash_config lro_hash;
	QDF_STATUS status;

	if (!wlan_cfg_is_lro_enabled(soc->wlan_cfg_ctx) &&
	    !wlan_cfg_is_gro_enabled(soc->wlan_cfg_ctx) &&
	    !wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx)) {
		dp_err("LRO, GRO and RX hash disabled");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&lro_hash, sizeof(lro_hash));

	if (wlan_cfg_is_lro_enabled(soc->wlan_cfg_ctx) ||
	    wlan_cfg_is_gro_enabled(soc->wlan_cfg_ctx)) {
		lro_hash.lro_enable = 1;
		lro_hash.tcp_flag = QDF_TCPHDR_ACK;
		lro_hash.tcp_flag_mask = QDF_TCPHDR_FIN | QDF_TCPHDR_SYN |
			 QDF_TCPHDR_RST | QDF_TCPHDR_ACK | QDF_TCPHDR_URG |
			 QDF_TCPHDR_ECE | QDF_TCPHDR_CWR;
	}

	soc->arch_ops.get_rx_hash_key(soc, &lro_hash);

	qdf_assert(soc->cdp_soc.ol_ops->lro_hash_config);

	if (!soc->cdp_soc.ol_ops->lro_hash_config) {
		QDF_BUG(0);
		dp_err("lro_hash_config not configured");
		return QDF_STATUS_E_FAILURE;
	}

	status = soc->cdp_soc.ol_ops->lro_hash_config(soc->ctrl_psoc,
						      pdev->pdev_id,
						      &lro_hash);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("failed to send lro_hash_config to FW %u", status);
		return status;
	}

	dp_info("LRO CMD config: lro_enable: 0x%x tcp_flag 0x%x tcp_flag_mask 0x%x",
		lro_hash.lro_enable, lro_hash.tcp_flag,
		lro_hash.tcp_flag_mask);

	dp_info("toeplitz_hash_ipv4:");
	qdf_trace_hex_dump(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   lro_hash.toeplitz_hash_ipv4,
			   (sizeof(lro_hash.toeplitz_hash_ipv4[0]) *
			   LRO_IPV4_SEED_ARR_SZ));

	dp_info("toeplitz_hash_ipv6:");
	qdf_trace_hex_dump(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   lro_hash.toeplitz_hash_ipv6,
			   (sizeof(lro_hash.toeplitz_hash_ipv6[0]) *
			   LRO_IPV6_SEED_ARR_SZ));

	return status;
}

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
/**
 * dp_reap_timer_init() - initialize the reap timer
 * @soc: data path SoC handle
 *
 * Return: void
 */
static void dp_reap_timer_init(struct dp_soc *soc)
{
	/*
	 * Timer to reap rxdma status rings.
	 * Needed until we enable ppdu end interrupts
	 */
	dp_monitor_reap_timer_init(soc);
	dp_monitor_vdev_timer_init(soc);
}

/**
 * dp_reap_timer_deinit() - de-initialize the reap timer
 * @soc: data path SoC handle
 *
 * Return: void
 */
static void dp_reap_timer_deinit(struct dp_soc *soc)
{
	dp_monitor_reap_timer_deinit(soc);
}
#else
/* WIN use case */
static void dp_reap_timer_init(struct dp_soc *soc)
{
	/* Configure LMAC rings in Polled mode */
	if (soc->lmac_polled_mode) {
		/*
		 * Timer to reap lmac rings.
		 */
		qdf_timer_init(soc->osdev, &soc->lmac_reap_timer,
			       dp_service_lmac_rings, (void *)soc,
			       QDF_TIMER_TYPE_WAKE_APPS);
		soc->lmac_timer_init = 1;
		qdf_timer_mod(&soc->lmac_reap_timer, DP_INTR_POLL_TIMER_MS);
	}
}

static void dp_reap_timer_deinit(struct dp_soc *soc)
{
	if (soc->lmac_timer_init) {
		qdf_timer_stop(&soc->lmac_reap_timer);
		qdf_timer_free(&soc->lmac_reap_timer);
		soc->lmac_timer_init = 0;
	}
}
#endif

#ifdef QCA_HOST2FW_RXBUF_RING
/**
 * dp_rxdma_ring_alloc() - allocate the RXDMA rings
 * @soc: data path SoC handle
 * @pdev: Physical device handle
 *
 * Return: 0 - success, > 0 - failure
 */
static int dp_rxdma_ring_alloc(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct wlan_cfg_dp_pdev_ctxt *pdev_cfg_ctx;
	int max_mac_rings;
	int i;
	int ring_size;

	pdev_cfg_ctx = pdev->wlan_cfg_ctx;
	max_mac_rings = wlan_cfg_get_num_mac_rings(pdev_cfg_ctx);
	ring_size =  wlan_cfg_get_rx_dma_buf_ring_size(pdev_cfg_ctx);

	for (i = 0; i < max_mac_rings; i++) {
		dp_verbose_debug("pdev_id %d mac_id %d", pdev->pdev_id, i);
		if (dp_srng_alloc(soc, &pdev->rx_mac_buf_ring[i],
				  RXDMA_BUF, ring_size, 0)) {
			dp_init_err("%pK: failed rx mac ring setup", soc);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rxdma_ring_setup() - configure the RXDMA rings
 * @soc: data path SoC handle
 * @pdev: Physical device handle
 *
 * Return: 0 - success, > 0 - failure
 */
static int dp_rxdma_ring_setup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct wlan_cfg_dp_pdev_ctxt *pdev_cfg_ctx;
	int max_mac_rings;
	int i;

	pdev_cfg_ctx = pdev->wlan_cfg_ctx;
	max_mac_rings = wlan_cfg_get_num_mac_rings(pdev_cfg_ctx);

	for (i = 0; i < max_mac_rings; i++) {
		dp_verbose_debug("pdev_id %d mac_id %d", pdev->pdev_id, i);
		if (dp_srng_init(soc, &pdev->rx_mac_buf_ring[i],
				 RXDMA_BUF, 1, i)) {
			dp_init_err("%pK: failed rx mac ring setup", soc);
			return QDF_STATUS_E_FAILURE;
		}
		dp_ssr_dump_srng_register("rx_mac_buf_ring",
					  &pdev->rx_mac_buf_ring[i], i);
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rxdma_ring_cleanup() - Deinit the RXDMA rings and reap timer
 * @soc: data path SoC handle
 * @pdev: Physical device handle
 *
 * Return: void
 */
static void dp_rxdma_ring_cleanup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	int i;

	for (i = 0; i < MAX_RX_MAC_RINGS; i++) {
		dp_ssr_dump_srng_unregister("rx_mac_buf_ring", i);
		dp_srng_deinit(soc, &pdev->rx_mac_buf_ring[i], RXDMA_BUF, 1);
	}

	dp_reap_timer_deinit(soc);
}

/**
 * dp_rxdma_ring_free() - Free the RXDMA rings
 * @pdev: Physical device handle
 *
 * Return: void
 */
static void dp_rxdma_ring_free(struct dp_pdev *pdev)
{
	int i;

	for (i = 0; i < MAX_RX_MAC_RINGS; i++)
		dp_srng_free(pdev->soc, &pdev->rx_mac_buf_ring[i]);
}

#else
static int dp_rxdma_ring_alloc(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static int dp_rxdma_ring_setup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_rxdma_ring_cleanup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	dp_reap_timer_deinit(soc);
}

static void dp_rxdma_ring_free(struct dp_pdev *pdev)
{
}
#endif

#ifdef IPA_OFFLOAD
/**
 * dp_setup_ipa_rx_refill_buf_ring - Setup second Rx refill buffer ring
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_RESOURCES: Error return
 */
static int dp_setup_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					   struct dp_pdev *pdev)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	int entries;

	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		soc_cfg_ctx = soc->wlan_cfg_ctx;
		entries =
			wlan_cfg_get_dp_soc_rxdma_refill_ring_size(soc_cfg_ctx);

		/* Setup second Rx refill buffer ring */
		if (dp_srng_alloc(soc, &pdev->rx_refill_buf_ring2, RXDMA_BUF,
				  entries, 0)) {
			dp_init_err("%pK: dp_srng_alloc failed second"
				    "rx refill ring", soc);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_WDI3_VLAN_SUPPORT
static int dp_setup_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	int entries;

	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx) &&
	    wlan_ipa_is_vlan_enabled()) {
		soc_cfg_ctx = soc->wlan_cfg_ctx;
		entries =
			wlan_cfg_get_dp_soc_rxdma_refill_ring_size(soc_cfg_ctx);

		/* Setup second Rx refill buffer ring */
		if (dp_srng_alloc(soc, &pdev->rx_refill_buf_ring3, RXDMA_BUF,
				  entries, 0)) {
			dp_init_err("%pK: alloc failed for 3rd rx refill ring",
				    soc);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static int dp_init_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					      struct dp_pdev *pdev)
{
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx) &&
	    wlan_ipa_is_vlan_enabled()) {
		if (dp_srng_init(soc, &pdev->rx_refill_buf_ring3, RXDMA_BUF,
				 IPA_RX_ALT_REFILL_BUF_RING_IDX,
				 pdev->pdev_id)) {
			dp_init_err("%pK: init failed for 3rd rx refill ring",
				    soc);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static void dp_deinit_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
						 struct dp_pdev *pdev)
{
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx) &&
	    wlan_ipa_is_vlan_enabled())
		dp_srng_deinit(soc, &pdev->rx_refill_buf_ring3, RXDMA_BUF, 0);
}

static void dp_free_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx) &&
	    wlan_ipa_is_vlan_enabled())
		dp_srng_free(soc, &pdev->rx_refill_buf_ring3);
}
#else
static int dp_setup_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static int dp_init_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					      struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_deinit_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
						 struct dp_pdev *pdev)
{
}

static void dp_free_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_deinit_ipa_rx_refill_buf_ring - deinit second Rx refill buffer ring
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * Return: void
 */
static void dp_deinit_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					     struct dp_pdev *pdev)
{
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		dp_srng_deinit(soc, &pdev->rx_refill_buf_ring2, RXDMA_BUF, 0);
}

/**
 * dp_init_ipa_rx_refill_buf_ring - Init second Rx refill buffer ring
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_RESOURCES: Error return
 */
static int dp_init_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					  struct dp_pdev *pdev)
{
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		if (dp_srng_init(soc, &pdev->rx_refill_buf_ring2, RXDMA_BUF,
				 IPA_RX_REFILL_BUF_RING_IDX, pdev->pdev_id)) {
			dp_init_err("%pK: dp_srng_init failed second"
				    "rx refill ring", soc);
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (dp_init_ipa_rx_alt_refill_buf_ring(soc, pdev)) {
		dp_deinit_ipa_rx_refill_buf_ring(soc, pdev);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_free_ipa_rx_refill_buf_ring - free second Rx refill buffer ring
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * Return: void
 */
static void dp_free_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					   struct dp_pdev *pdev)
{
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		dp_srng_free(soc, &pdev->rx_refill_buf_ring2);
}
#else
static int dp_setup_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					   struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static int dp_init_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					  struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_deinit_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					     struct dp_pdev *pdev)
{
}

static void dp_free_ipa_rx_refill_buf_ring(struct dp_soc *soc,
					   struct dp_pdev *pdev)
{
}

static int dp_setup_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_deinit_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
						 struct dp_pdev *pdev)
{
}

static void dp_free_ipa_rx_alt_refill_buf_ring(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
}
#endif

#ifdef WLAN_FEATURE_DP_CFG_EVENT_HISTORY

/**
 * dp_soc_cfg_history_attach() - Allocate and attach datapath config events
 *				 history
 * @soc: DP soc handle
 *
 * Return: None
 */
static void dp_soc_cfg_history_attach(struct dp_soc *soc)
{
	dp_soc_frag_history_attach(soc, &soc->cfg_event_history,
				   DP_CFG_EVT_HIST_MAX_SLOTS,
				   DP_CFG_EVT_HIST_PER_SLOT_MAX,
				   sizeof(struct dp_cfg_event),
				   true, DP_CFG_EVENT_HIST_TYPE);
}

/**
 * dp_soc_cfg_history_detach() - Detach and free DP config events history
 * @soc: DP soc handle
 *
 * Return: none
 */
static void dp_soc_cfg_history_detach(struct dp_soc *soc)
{
	dp_soc_frag_history_detach(soc, &soc->cfg_event_history,
				   DP_CFG_EVT_HIST_MAX_SLOTS,
				   true, DP_CFG_EVENT_HIST_TYPE);
}

#else
static void dp_soc_cfg_history_attach(struct dp_soc *soc)
{
}

static void dp_soc_cfg_history_detach(struct dp_soc *soc)
{
}
#endif

#ifdef DP_TX_HW_DESC_HISTORY
/**
 * dp_soc_tx_hw_desc_history_attach - Attach TX HW descriptor history
 *
 * @soc: DP soc handle
 *
 * Return: None
 */
static void dp_soc_tx_hw_desc_history_attach(struct dp_soc *soc)
{
	dp_soc_frag_history_attach(soc, &soc->tx_hw_desc_history,
				   DP_TX_HW_DESC_HIST_MAX_SLOTS,
				   DP_TX_HW_DESC_HIST_PER_SLOT_MAX,
				   sizeof(struct dp_tx_hw_desc_evt),
				   true, DP_TX_HW_DESC_HIST_TYPE);
}

static void dp_soc_tx_hw_desc_history_detach(struct dp_soc *soc)
{
	dp_soc_frag_history_detach(soc, &soc->tx_hw_desc_history,
				   DP_TX_HW_DESC_HIST_MAX_SLOTS,
				   true, DP_TX_HW_DESC_HIST_TYPE);
}

#else /* DP_TX_HW_DESC_HISTORY */
static inline void
dp_soc_tx_hw_desc_history_attach(struct dp_soc *soc)
{
}

static inline void
dp_soc_tx_hw_desc_history_detach(struct dp_soc *soc)
{
}
#endif /* DP_TX_HW_DESC_HISTORY */

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
#ifndef RX_DEFRAG_DO_NOT_REINJECT
/**
 * dp_soc_rx_reinject_ring_history_attach - Attach the reo reinject ring
 *					    history.
 * @soc: DP soc handle
 *
 * Return: None
 */
static void dp_soc_rx_reinject_ring_history_attach(struct dp_soc *soc)
{
	soc->rx_reinject_ring_history =
		dp_context_alloc_mem(soc, DP_RX_REINJECT_RING_HIST_TYPE,
				     sizeof(struct dp_rx_reinject_history));
	if (soc->rx_reinject_ring_history)
		qdf_atomic_init(&soc->rx_reinject_ring_history->index);
}
#else /* RX_DEFRAG_DO_NOT_REINJECT */
static inline void
dp_soc_rx_reinject_ring_history_attach(struct dp_soc *soc)
{
}
#endif /* RX_DEFRAG_DO_NOT_REINJECT */

/**
 * dp_soc_rx_history_attach() - Attach the ring history record buffers
 * @soc: DP soc structure
 *
 * This function allocates the memory for recording the rx ring, rx error
 * ring and the reinject ring entries. There is no error returned in case
 * of allocation failure since the record function checks if the history is
 * initialized or not. We do not want to fail the driver load in case of
 * failure to allocate memory for debug history.
 *
 * Return: None
 */
static void dp_soc_rx_history_attach(struct dp_soc *soc)
{
	int i;
	uint32_t rx_ring_hist_size;
	uint32_t rx_refill_ring_hist_size;

	rx_ring_hist_size = sizeof(*soc->rx_ring_history[0]);
	rx_refill_ring_hist_size = sizeof(*soc->rx_refill_ring_history[0]);

	for (i = 0; i < MAX_REO_DEST_RINGS; i++) {
		soc->rx_ring_history[i] = dp_context_alloc_mem(
				soc, DP_RX_RING_HIST_TYPE, rx_ring_hist_size);
		if (soc->rx_ring_history[i])
			qdf_atomic_init(&soc->rx_ring_history[i]->index);
	}

	soc->rx_err_ring_history = dp_context_alloc_mem(
			soc, DP_RX_ERR_RING_HIST_TYPE, rx_ring_hist_size);
	if (soc->rx_err_ring_history)
		qdf_atomic_init(&soc->rx_err_ring_history->index);

	dp_soc_rx_reinject_ring_history_attach(soc);

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		soc->rx_refill_ring_history[i] = dp_context_alloc_mem(
						soc,
						DP_RX_REFILL_RING_HIST_TYPE,
						rx_refill_ring_hist_size);

		if (soc->rx_refill_ring_history[i])
			qdf_atomic_init(&soc->rx_refill_ring_history[i]->index);
	}
}

static void dp_soc_rx_history_detach(struct dp_soc *soc)
{
	int i;

	for (i = 0; i < MAX_REO_DEST_RINGS; i++)
		dp_context_free_mem(soc, DP_RX_RING_HIST_TYPE,
				    soc->rx_ring_history[i]);

	dp_context_free_mem(soc, DP_RX_ERR_RING_HIST_TYPE,
			    soc->rx_err_ring_history);

	/*
	 * No need for a featurized detach since qdf_mem_free takes
	 * care of NULL pointer.
	 */
	dp_context_free_mem(soc, DP_RX_REINJECT_RING_HIST_TYPE,
			    soc->rx_reinject_ring_history);

	for (i = 0; i < MAX_PDEV_CNT; i++)
		dp_context_free_mem(soc, DP_RX_REFILL_RING_HIST_TYPE,
				    soc->rx_refill_ring_history[i]);
}

#else
static inline void dp_soc_rx_history_attach(struct dp_soc *soc)
{
}

static inline void dp_soc_rx_history_detach(struct dp_soc *soc)
{
}
#endif

#ifdef WLAN_FEATURE_DP_MON_STATUS_RING_HISTORY
/**
 * dp_soc_mon_status_ring_history_attach() - Attach the monitor status
 *					     buffer record history.
 * @soc: DP soc handle
 *
 * This function allocates memory to track the event for a monitor
 * status buffer, before its parsed and freed.
 *
 * Return: None
 */
static void dp_soc_mon_status_ring_history_attach(struct dp_soc *soc)
{
	soc->mon_status_ring_history = dp_context_alloc_mem(soc,
				DP_MON_STATUS_BUF_HIST_TYPE,
				sizeof(struct dp_mon_status_ring_history));
	if (!soc->mon_status_ring_history) {
		dp_err("Failed to alloc memory for mon status ring history");
		return;
	}
}

/**
 * dp_soc_mon_status_ring_history_detach() - Detach the monitor status buffer
 *					     record history.
 * @soc: DP soc handle
 *
 * Return: None
 */
static void dp_soc_mon_status_ring_history_detach(struct dp_soc *soc)
{
	dp_context_free_mem(soc, DP_MON_STATUS_BUF_HIST_TYPE,
			    soc->mon_status_ring_history);
}
#else
static void dp_soc_mon_status_ring_history_attach(struct dp_soc *soc)
{
}

static void dp_soc_mon_status_ring_history_detach(struct dp_soc *soc)
{
}
#endif

#ifdef WLAN_FEATURE_DP_TX_DESC_HISTORY
/**
 * dp_soc_tx_history_attach() - Attach the ring history record buffers
 * @soc: DP soc structure
 *
 * This function allocates the memory for recording the tx tcl ring and
 * the tx comp ring entries. There is no error returned in case
 * of allocation failure since the record function checks if the history is
 * initialized or not. We do not want to fail the driver load in case of
 * failure to allocate memory for debug history.
 *
 * Return: None
 */
static void dp_soc_tx_history_attach(struct dp_soc *soc)
{
	dp_soc_frag_history_attach(soc, &soc->tx_tcl_history,
				   DP_TX_TCL_HIST_MAX_SLOTS,
				   DP_TX_TCL_HIST_PER_SLOT_MAX,
				   sizeof(struct dp_tx_desc_event),
				   true, DP_TX_TCL_HIST_TYPE);
	dp_soc_frag_history_attach(soc, &soc->tx_comp_history,
				   DP_TX_COMP_HIST_MAX_SLOTS,
				   DP_TX_COMP_HIST_PER_SLOT_MAX,
				   sizeof(struct dp_tx_desc_event),
				   true, DP_TX_COMP_HIST_TYPE);
}

/**
 * dp_soc_tx_history_detach() - Detach the ring history record buffers
 * @soc: DP soc structure
 *
 * This function frees the memory for recording the tx tcl ring and
 * the tx comp ring entries.
 *
 * Return: None
 */
static void dp_soc_tx_history_detach(struct dp_soc *soc)
{
	dp_soc_frag_history_detach(soc, &soc->tx_tcl_history,
				   DP_TX_TCL_HIST_MAX_SLOTS,
				   true, DP_TX_TCL_HIST_TYPE);
	dp_soc_frag_history_detach(soc, &soc->tx_comp_history,
				   DP_TX_COMP_HIST_MAX_SLOTS,
				   true, DP_TX_COMP_HIST_TYPE);
}

#else
static inline void dp_soc_tx_history_attach(struct dp_soc *soc)
{
}

static inline void dp_soc_tx_history_detach(struct dp_soc *soc)
{
}
#endif /* WLAN_FEATURE_DP_TX_DESC_HISTORY */

#ifdef DP_RX_MSDU_DONE_FAIL_HISTORY
static void dp_soc_msdu_done_fail_history_attach(struct dp_soc *soc)
{
	soc->msdu_done_fail_hist =
		qdf_mem_malloc(sizeof(struct dp_msdu_done_fail_history));
	if (soc->msdu_done_fail_hist)
		qdf_atomic_init(&soc->msdu_done_fail_hist->index);
}

static void dp_soc_msdu_done_fail_history_detach(struct dp_soc *soc)
{
	if (soc->msdu_done_fail_hist)
		qdf_mem_free(soc->msdu_done_fail_hist);
}
#else
static inline void dp_soc_msdu_done_fail_history_attach(struct dp_soc *soc)
{
}

static inline void dp_soc_msdu_done_fail_history_detach(struct dp_soc *soc)
{
}
#endif

#ifdef DP_RX_PEEK_MSDU_DONE_WAR
static void dp_soc_msdu_done_fail_desc_list_attach(struct dp_soc *soc)
{
	qdf_atomic_init(&soc->msdu_done_fail_desc_list.index);
	qdf_atomic_set(&soc->msdu_done_fail_desc_list.index,
		       DP_MSDU_DONE_FAIL_DESCS_MAX - 1);
}
#else
static void dp_soc_msdu_done_fail_desc_list_attach(struct dp_soc *soc)
{
}
#endif

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
QDF_STATUS
dp_rx_fst_attach_wrapper(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *rx_fst = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	/* for Lithium the below API is not registered
	 * hence fst attach happens for each pdev
	 */
	if (!soc->arch_ops.dp_get_rx_fst)
		return dp_rx_fst_attach(soc, pdev);

	rx_fst = soc->arch_ops.dp_get_rx_fst();

	/* for BE the FST attach is called only once per
	 * ML context. if rx_fst is already registered
	 * increase the ref count and return.
	 */
	if (rx_fst) {
		soc->rx_fst = rx_fst;
		pdev->rx_fst = rx_fst;
		soc->arch_ops.dp_rx_fst_ref();
	} else {
		ret = dp_rx_fst_attach(soc, pdev);
		if ((ret != QDF_STATUS_SUCCESS) &&
		    (ret != QDF_STATUS_E_NOSUPPORT))
			return ret;

		soc->arch_ops.dp_set_rx_fst(soc->rx_fst);
		soc->arch_ops.dp_rx_fst_ref();
	}
	return ret;
}

void
dp_rx_fst_detach_wrapper(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *rx_fst = NULL;

	/* for Lithium the below API is not registered
	 * hence fst detach happens for each pdev
	 */
	if (!soc->arch_ops.dp_get_rx_fst) {
		dp_rx_fst_detach(soc, pdev);
		return;
	}

	rx_fst = soc->arch_ops.dp_get_rx_fst();

	/* for BE the FST detach is called only when last
	 * ref count reaches 1.
	 */
	if (rx_fst) {
		if (soc->arch_ops.dp_rx_fst_deref() == 1)
			dp_rx_fst_detach(soc, pdev);
	}
	pdev->rx_fst = NULL;
}
#else
QDF_STATUS
dp_rx_fst_attach_wrapper(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

void
dp_rx_fst_detach_wrapper(struct dp_soc *soc, struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_pdev_attach_wifi3() - attach txrx pdev
 * @txrx_soc: Datapath SOC handle
 * @params: Params for PDEV attach
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS dp_pdev_attach_wifi3(struct cdp_soc_t *txrx_soc,
				struct cdp_pdev_attach_params *params)
{
	qdf_size_t pdev_context_size;
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	struct dp_pdev *pdev = NULL;
	uint8_t pdev_id = params->pdev_id;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	int nss_cfg;
	QDF_STATUS ret;

	pdev_context_size =
		soc->arch_ops.txrx_get_context_size(DP_CONTEXT_TYPE_PDEV);
	if (pdev_context_size)
		pdev = dp_context_alloc_mem(soc, DP_PDEV_TYPE,
					    pdev_context_size);

	if (!pdev) {
		dp_init_err("%pK: DP PDEV memory allocation failed",
			    soc);
		goto fail0;
	}
	wlan_minidump_log(pdev, sizeof(*pdev), soc->ctrl_psoc,
			  WLAN_MD_DP_PDEV, "dp_pdev");

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	pdev->wlan_cfg_ctx = wlan_cfg_pdev_attach(soc->ctrl_psoc);

	if (!pdev->wlan_cfg_ctx) {
		dp_init_err("%pK: pdev cfg_attach failed", soc);
		goto fail1;
	}

	pdev->soc = soc;
	pdev->pdev_id = pdev_id;
	soc->pdev_list[pdev_id] = pdev;
	pdev->lmac_id = wlan_cfg_get_hw_mac_idx(soc->wlan_cfg_ctx, pdev_id);
	soc->pdev_count++;

	dp_ssr_dump_pdev_register(pdev, pdev_id);

	/*sync DP pdev cfg items with profile support after cfg_pdev_attach*/
	wlan_dp_pdev_cfg_sync_profile((struct cdp_soc_t *)soc, pdev_id);

	/*
	 * set nss pdev config based on soc config
	 */
	nss_cfg = wlan_cfg_get_dp_soc_nss_cfg(soc_cfg_ctx);
	wlan_cfg_set_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx,
					 (nss_cfg & (1 << pdev_id)));

	/* Allocate memory for pdev srng rings */
	if (dp_pdev_srng_alloc(pdev)) {
		dp_init_err("%pK: dp_pdev_srng_alloc failed", soc);
		goto fail2;
	}

	/* Setup second Rx refill buffer ring */
	if (dp_setup_ipa_rx_refill_buf_ring(soc, pdev)) {
		dp_init_err("%pK: dp_srng_alloc failed rxrefill2 ring",
			    soc);
		goto fail3;
	}

	/* Allocate memory for pdev rxdma rings */
	if (dp_rxdma_ring_alloc(soc, pdev)) {
		dp_init_err("%pK: dp_rxdma_ring_alloc failed", soc);
		goto fail4;
	}

	/* Rx specific init */
	if (dp_rx_pdev_desc_pool_alloc(pdev)) {
		dp_init_err("%pK: dp_rx_pdev_attach failed", soc);
		goto fail4;
	}

	if (dp_monitor_pdev_attach(pdev)) {
		dp_init_err("%pK: dp_monitor_pdev_attach failed", soc);
		goto fail5;
	}

	soc->arch_ops.txrx_pdev_attach(pdev, params);

	/* Setup third Rx refill buffer ring */
	if (dp_setup_ipa_rx_alt_refill_buf_ring(soc, pdev)) {
		dp_init_err("%pK: dp_srng_alloc failed rxrefill3 ring",
			    soc);
		goto fail6;
	}

	ret = dp_rx_fst_attach_wrapper(soc, pdev);
	if ((ret != QDF_STATUS_SUCCESS) && (ret != QDF_STATUS_E_NOSUPPORT)) {
		dp_init_err("%pK: RX FST attach failed: pdev %d err %d",
			    soc, pdev_id, ret);
		goto fail7;
	}

	return QDF_STATUS_SUCCESS;

fail7:
	dp_free_ipa_rx_alt_refill_buf_ring(soc, pdev);
fail6:
	dp_monitor_pdev_detach(pdev);
fail5:
	dp_rx_pdev_desc_pool_free(pdev);
fail4:
	dp_rxdma_ring_free(pdev);
	dp_free_ipa_rx_refill_buf_ring(soc, pdev);
fail3:
	dp_pdev_srng_free(pdev);
fail2:
	wlan_cfg_pdev_detach(pdev->wlan_cfg_ctx);
fail1:
	soc->pdev_list[pdev_id] = NULL;
	qdf_mem_free(pdev);
fail0:
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_pdev_flush_pending_vdevs() - Flush all delete pending vdevs in pdev
 * @pdev: Datapath PDEV handle
 *
 * This is the last chance to flush all pending dp vdevs/peers,
 * some peer/vdev leak case like Non-SSR + peer unmap missing
 * will be covered here.
 *
 * Return: None
 */
static void dp_pdev_flush_pending_vdevs(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_vdev *vdev_arr[MAX_VDEV_CNT] = {0};
	uint32_t i = 0;
	uint32_t num_vdevs = 0;
	struct dp_vdev *vdev = NULL;

	if (TAILQ_EMPTY(&soc->inactive_vdev_list))
		return;

	qdf_spin_lock_bh(&soc->inactive_vdev_list_lock);
	TAILQ_FOREACH(vdev, &soc->inactive_vdev_list,
		      inactive_list_elem) {
		if (vdev->pdev != pdev)
			continue;

		vdev_arr[num_vdevs] = vdev;
		num_vdevs++;
		/* take reference to free */
		dp_vdev_get_ref(soc, vdev, DP_MOD_ID_CDP);
	}
	qdf_spin_unlock_bh(&soc->inactive_vdev_list_lock);

	for (i = 0; i < num_vdevs; i++) {
		dp_vdev_flush_peers((struct cdp_vdev *)vdev_arr[i], 0, 0);
		dp_vdev_unref_delete(soc, vdev_arr[i], DP_MOD_ID_CDP);
	}
}

#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
/**
 * dp_vdev_stats_hw_offload_target_config() - Send HTT command to FW
 *                                          for enable/disable of HW vdev stats
 * @soc: Datapath soc handle
 * @pdev_id: INVALID_PDEV_ID for all pdevs or 0,1,2 for individual pdev
 * @enable: flag to represent enable/disable of hw vdev stats
 *
 * Return: none
 */
static void dp_vdev_stats_hw_offload_target_config(struct dp_soc *soc,
						   uint8_t pdev_id,
						   bool enable)
{
	/* Check SOC level config for HW offload vdev stats support */
	if (!wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx)) {
		dp_debug("%pK: HW vdev offload stats is disabled", soc);
		return;
	}

	/* Send HTT command to FW for enable of stats */
	dp_h2t_hw_vdev_stats_config_send(soc, pdev_id, enable, false, 0);
}

/**
 * dp_vdev_stats_hw_offload_target_clear() - Clear HW vdev stats on target
 * @soc: Datapath soc handle
 * @pdev_id: pdev_id (0,1,2)
 * @vdev_id_bitmask: bitmask with vdev_id(s) for which stats are to be
 *                   cleared on HW
 *
 * Return: none
 */
static
void dp_vdev_stats_hw_offload_target_clear(struct dp_soc *soc, uint8_t pdev_id,
					   uint64_t vdev_id_bitmask)
{
	/* Check SOC level config for HW offload vdev stats support */
	if (!wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx)) {
		dp_debug("%pK: HW vdev offload stats is disabled", soc);
		return;
	}

	/* Send HTT command to FW for reset of stats */
	dp_h2t_hw_vdev_stats_config_send(soc, pdev_id, true, true,
					 vdev_id_bitmask);
}
#else
static void
dp_vdev_stats_hw_offload_target_config(struct dp_soc *soc, uint8_t pdev_id,
				       bool enable)
{
}

static
void dp_vdev_stats_hw_offload_target_clear(struct dp_soc *soc, uint8_t pdev_id,
					   uint64_t vdev_id_bitmask)
{
}
#endif /*QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT */

/**
 * dp_pdev_deinit() - Deinit txrx pdev
 * @txrx_pdev: Datapath PDEV handle
 * @force: Force deinit
 *
 * Return: None
 */
static void dp_pdev_deinit(struct cdp_pdev *txrx_pdev, int force)
{
	struct dp_pdev *pdev = (struct dp_pdev *)txrx_pdev;
	qdf_nbuf_t curr_nbuf, next_nbuf;

	if (pdev->pdev_deinit)
		return;

	dp_tx_me_exit(pdev);
	dp_rx_pdev_buffers_free(pdev);
	dp_rx_pdev_desc_pool_deinit(pdev);
	dp_pdev_bkp_stats_detach(pdev);
	qdf_event_destroy(&pdev->fw_peer_stats_event);
	qdf_event_destroy(&pdev->fw_stats_event);
	qdf_event_destroy(&pdev->fw_obss_stats_event);
	if (pdev->sojourn_buf)
		qdf_nbuf_free(pdev->sojourn_buf);

	dp_pdev_flush_pending_vdevs(pdev);
	dp_tx_desc_flush(pdev, NULL, true);

	qdf_spinlock_destroy(&pdev->tx_mutex);
	qdf_spinlock_destroy(&pdev->vdev_list_lock);

	dp_monitor_pdev_deinit(pdev);

	dp_pdev_srng_deinit(pdev);

	dp_ipa_uc_detach(pdev->soc, pdev);
	dp_deinit_ipa_rx_alt_refill_buf_ring(pdev->soc, pdev);
	dp_deinit_ipa_rx_refill_buf_ring(pdev->soc, pdev);
	dp_rxdma_ring_cleanup(pdev->soc, pdev);

	curr_nbuf = pdev->invalid_peer_head_msdu;
	while (curr_nbuf) {
		next_nbuf = qdf_nbuf_next(curr_nbuf);
		dp_rx_nbuf_free(curr_nbuf);
		curr_nbuf = next_nbuf;
	}
	pdev->invalid_peer_head_msdu = NULL;
	pdev->invalid_peer_tail_msdu = NULL;

	dp_wdi_event_detach(pdev);
	pdev->pdev_deinit = 1;
}

/**
 * dp_pdev_deinit_wifi3() - Deinit txrx pdev
 * @psoc: Datapath psoc handle
 * @pdev_id: Id of datapath PDEV handle
 * @force: Force deinit
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_pdev_deinit_wifi3(struct cdp_soc_t *psoc, uint8_t pdev_id,
		     int force)
{
	struct dp_pdev *txrx_pdev;

	txrx_pdev = dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)psoc,
						       pdev_id);

	if (!txrx_pdev)
		return QDF_STATUS_E_FAILURE;

	dp_pdev_deinit((struct cdp_pdev *)txrx_pdev, force);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_pdev_post_attach() - Do post pdev attach after dev_alloc_name
 * @txrx_pdev: Datapath PDEV handle
 *
 * Return: None
 */
static void dp_pdev_post_attach(struct cdp_pdev *txrx_pdev)
{
	struct dp_pdev *pdev = (struct dp_pdev *)txrx_pdev;

	dp_monitor_tx_capture_debugfs_init(pdev);

	if (dp_pdev_htt_stats_dbgfs_init(pdev)) {
		dp_init_err("%pK: Failed to initialize pdev HTT stats debugfs", pdev->soc);
	}
}

/**
 * dp_pdev_post_attach_wifi3() - attach txrx pdev post
 * @soc: Datapath soc handle
 * @pdev_id: pdev id of pdev
 *
 * Return: QDF_STATUS
 */
static int dp_pdev_post_attach_wifi3(struct cdp_soc_t *soc,
				     uint8_t pdev_id)
{
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						  pdev_id);

	if (!pdev) {
		dp_init_err("%pK: DP PDEV is Null for pdev id %d",
			    (struct dp_soc *)soc, pdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	dp_pdev_post_attach((struct cdp_pdev *)pdev);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_pdev_detach() - Complete rest of pdev detach
 * @txrx_pdev: Datapath PDEV handle
 * @force: Force deinit
 *
 * Return: None
 */
static void dp_pdev_detach(struct cdp_pdev *txrx_pdev, int force)
{
	struct dp_pdev *pdev = (struct dp_pdev *)txrx_pdev;
	struct dp_soc *soc = pdev->soc;

	dp_rx_fst_detach_wrapper(soc, pdev);
	dp_pdev_htt_stats_dbgfs_deinit(pdev);
	dp_rx_pdev_desc_pool_free(pdev);
	dp_monitor_pdev_detach(pdev);
	dp_rxdma_ring_free(pdev);
	dp_free_ipa_rx_refill_buf_ring(soc, pdev);
	dp_free_ipa_rx_alt_refill_buf_ring(soc, pdev);
	dp_pdev_srng_free(pdev);

	soc->pdev_count--;
	soc->pdev_list[pdev->pdev_id] = NULL;

	wlan_cfg_pdev_detach(pdev->wlan_cfg_ctx);
	wlan_minidump_remove(pdev, sizeof(*pdev), soc->ctrl_psoc,
			     WLAN_MD_DP_PDEV, "dp_pdev");
	dp_context_free_mem(soc, DP_PDEV_TYPE, pdev);
}

/**
 * dp_pdev_detach_wifi3() - detach txrx pdev
 * @psoc: Datapath soc handle
 * @pdev_id: pdev id of pdev
 * @force: Force detach
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_pdev_detach_wifi3(struct cdp_soc_t *psoc, uint8_t pdev_id,
				       int force)
{
	struct dp_pdev *pdev;
	struct dp_soc *soc = (struct dp_soc *)psoc;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)psoc,
						  pdev_id);

	if (!pdev) {
		dp_init_err("%pK: DP PDEV is Null for pdev id %d",
			    (struct dp_soc *)psoc, pdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	dp_ssr_dump_pdev_unregister(pdev_id);

	soc->arch_ops.txrx_pdev_detach(pdev);

	dp_pdev_detach((struct cdp_pdev *)pdev, force);
	return QDF_STATUS_SUCCESS;
}

void dp_soc_print_inactive_objects(struct dp_soc *soc)
{
	struct dp_peer *peer = NULL;
	struct dp_peer *tmp_peer = NULL;
	struct dp_vdev *vdev = NULL;
	struct dp_vdev *tmp_vdev = NULL;
	int i = 0;
	uint32_t count;

	if (TAILQ_EMPTY(&soc->inactive_peer_list) &&
	    TAILQ_EMPTY(&soc->inactive_vdev_list))
		return;

	TAILQ_FOREACH_SAFE(peer, &soc->inactive_peer_list,
			   inactive_list_elem, tmp_peer) {
		for (i = 0; i < DP_MOD_ID_MAX; i++) {
			count = qdf_atomic_read(&peer->mod_refs[i]);
			if (count)
				DP_PRINT_STATS("peer %pK Module id %u ==> %u",
					       peer, i, count);
		}
	}

	TAILQ_FOREACH_SAFE(vdev, &soc->inactive_vdev_list,
			   inactive_list_elem, tmp_vdev) {
		for (i = 0; i < DP_MOD_ID_MAX; i++) {
			count = qdf_atomic_read(&vdev->mod_refs[i]);
			if (count)
				DP_PRINT_STATS("vdev %pK Module id %u ==> %u",
					       vdev, i, count);
		}
	}
	QDF_BUG(0);
}

/**
 * dp_soc_deinit_wifi3() - Deinitialize txrx SOC
 * @txrx_soc: Opaque DP SOC handle
 *
 * Return: None
 */
static void dp_soc_deinit_wifi3(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	soc->arch_ops.txrx_soc_deinit(soc);
}

/**
 * dp_soc_detach() - Detach rest of txrx SOC
 * @txrx_soc: DP SOC handle, struct cdp_soc_t is first element of struct dp_soc.
 *
 * Return: None
 */
static void dp_soc_detach(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	soc->arch_ops.txrx_soc_detach(soc);

	qdf_ssr_driver_dump_unregister_region("wlan_cfg_ctx");
	qdf_ssr_driver_dump_unregister_region("dp_soc");
	qdf_ssr_driver_dump_unregister_region("tcl_wbm_map_array");
	qdf_nbuf_ssr_unregister_region();

	dp_runtime_deinit();

	dp_soc_unset_qref_debug_list(soc);
	dp_sysfs_deinitialize_stats(soc);
	wlan_dp_lapb_flow_detach(soc);
	dp_soc_swlm_detach(soc);
	dp_soc_tx_desc_sw_pools_free(soc);
	dp_soc_srng_free(soc);
	dp_hw_link_desc_ring_free(soc);
	dp_hw_link_desc_pool_banks_free(soc, WLAN_INVALID_PDEV_ID);
	wlan_cfg_soc_detach(soc->wlan_cfg_ctx);
	dp_soc_tx_hw_desc_history_detach(soc);
	dp_soc_tx_history_detach(soc);
	dp_soc_mon_status_ring_history_detach(soc);
	dp_soc_rx_history_detach(soc);
	dp_soc_cfg_history_detach(soc);
	dp_soc_msdu_done_fail_history_detach(soc);

	if (!dp_monitor_modularized_enable()) {
		dp_mon_soc_detach_wrapper(soc);
	}

	qdf_mem_free(soc->cdp_soc.ops);
	qdf_mem_common_free(soc);
}

/**
 * dp_soc_detach_wifi3() - Detach txrx SOC
 * @txrx_soc: DP SOC handle, struct cdp_soc_t is first element of struct dp_soc.
 *
 * Return: None
 */
static void dp_soc_detach_wifi3(struct cdp_soc_t *txrx_soc)
{
	dp_soc_detach(txrx_soc);
}

#ifdef QCA_HOST2FW_RXBUF_RING
#ifdef IPA_WDI3_VLAN_SUPPORT
static inline
void dp_rxdma_setup_refill_ring3(struct dp_soc *soc,
				 struct dp_pdev *pdev,
				 uint8_t idx)
{
	if (pdev->rx_refill_buf_ring3.hal_srng)
		htt_srng_setup(soc->htt_handle, idx,
			       pdev->rx_refill_buf_ring3.hal_srng,
			       RXDMA_BUF);
}
#else
static inline
void dp_rxdma_setup_refill_ring3(struct dp_soc *soc,
				 struct dp_pdev *pdev,
				 uint8_t idx)
{ }
#endif

#ifdef WIFI_MONITOR_SUPPORT
static inline QDF_STATUS dp_lpc_tx_config(struct dp_pdev *pdev)
{
	return dp_local_pkt_capture_tx_config(pdev);
}
#else
static inline QDF_STATUS dp_lpc_tx_config(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_rxdma_ring_config() - configure the RX DMA rings
 * @soc: data path SoC handle
 *
 * This function is used to configure the MAC rings.
 * On MCL host provides buffers in Host2FW ring
 * FW refills (copies) buffers to the ring and updates
 * ring_idx in register
 *
 * Return: zero on success, non-zero on failure
 */
static QDF_STATUS dp_rxdma_ring_config(struct dp_soc *soc)
{
	int i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev = soc->pdev_list[i];

		if (pdev) {
			int mac_id;
			int max_mac_rings =
				 wlan_cfg_get_num_mac_rings
				(pdev->wlan_cfg_ctx);
			int lmac_id = dp_get_lmac_id_for_pdev_id(soc, 0, i);

			htt_srng_setup(soc->htt_handle, i,
				       soc->rx_refill_buf_ring[lmac_id]
				       .hal_srng,
				       RXDMA_BUF);

			if (pdev->rx_refill_buf_ring2.hal_srng)
				htt_srng_setup(soc->htt_handle, i,
					       pdev->rx_refill_buf_ring2
					       .hal_srng,
					       RXDMA_BUF);

			dp_rxdma_setup_refill_ring3(soc, pdev, i);

			dp_update_num_mac_rings_for_dbs(soc, &max_mac_rings);
			dp_lpc_tx_config(pdev);
			dp_info("pdev_id %d max_mac_rings %d",
			       pdev->pdev_id, max_mac_rings);

			for (mac_id = 0; mac_id < max_mac_rings; mac_id++) {
				int mac_for_pdev =
					dp_get_mac_id_for_pdev(mac_id,
							       pdev->pdev_id);
				/*
				 * Obtain lmac id from pdev to access the LMAC
				 * ring in soc context
				 */
				lmac_id =
				dp_get_lmac_id_for_pdev_id(soc,
							   mac_id,
							   pdev->pdev_id);
				dp_info("mac_id %d", mac_for_pdev);

				htt_srng_setup(soc->htt_handle, mac_for_pdev,
					 pdev->rx_mac_buf_ring[mac_id]
						.hal_srng,
					 RXDMA_BUF);

				if (!soc->rxdma2sw_rings_not_supported)
					dp_htt_setup_rxdma_err_dst_ring(soc,
						mac_for_pdev, lmac_id);

				/* Configure monitor mode rings */
				status = dp_monitor_htt_srng_setup(soc, pdev,
								   lmac_id,
								   mac_for_pdev);
				if (status != QDF_STATUS_SUCCESS) {
					dp_err("Failed to send htt monitor messages to target");
					return status;
				}

			}
		}
	}

	dp_reap_timer_init(soc);
	return status;
}
#else
/* This is only for WIN */
static QDF_STATUS dp_rxdma_ring_config(struct dp_soc *soc)
{
	int i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	int mac_for_pdev;
	int lmac_id;

	/* Configure monitor mode rings */
	dp_monitor_soc_htt_srng_setup(soc);

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev =  soc->pdev_list[i];

		if (!pdev)
			continue;

		mac_for_pdev = i;
		lmac_id = dp_get_lmac_id_for_pdev_id(soc, 0, i);

		if (soc->rx_refill_buf_ring[lmac_id].hal_srng)
			htt_srng_setup(soc->htt_handle, mac_for_pdev,
				       soc->rx_refill_buf_ring[lmac_id].
				       hal_srng, RXDMA_BUF);

		/* Configure monitor mode rings */
		dp_monitor_htt_srng_setup(soc, pdev,
					  lmac_id,
					  mac_for_pdev);
		if (!soc->rxdma2sw_rings_not_supported)
			htt_srng_setup(soc->htt_handle, mac_for_pdev,
				       soc->rxdma_err_dst_ring[lmac_id].hal_srng,
				       RXDMA_DST);
	}

	dp_reap_timer_init(soc);
	return status;
}
#endif

/**
 * dp_rx_target_fst_config() - configure the RXOLE Flow Search Engine
 *
 * This function is used to configure the FSE HW block in RX OLE on a
 * per pdev basis. Here, we will be programming parameters related to
 * the Flow Search Table.
 *
 * @soc: data path SoC handle
 *
 * Return: zero on success, non-zero on failure
 */
#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static QDF_STATUS
dp_rx_target_fst_config(struct dp_soc *soc)
{
	int i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev = soc->pdev_list[i];

		/* Flow search is not enabled if NSS offload is enabled */
		if (pdev &&
		    !wlan_cfg_get_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx)) {
			status = dp_rx_flow_send_fst_fw_setup(pdev->soc, pdev);
			if (status != QDF_STATUS_SUCCESS)
				break;
		}
	}
	return status;
}
#else
static inline QDF_STATUS dp_rx_target_fst_config(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifndef WLAN_DP_FEATURE_SW_LATENCY_MGR
static inline QDF_STATUS dp_print_swlm_stats(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* !WLAN_DP_FEATURE_SW_LATENCY_MGR */

#ifdef WLAN_SUPPORT_PPEDS
/**
 * dp_soc_target_ppe_rxole_rxdma_cfg() - Configure the RxOLe and RxDMA for PPE
 * @soc: DP Tx/Rx handle
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS dp_soc_target_ppe_rxole_rxdma_cfg(struct dp_soc *soc)
{
	struct dp_htt_rxdma_rxole_ppe_config htt_cfg = {0};
	QDF_STATUS status;

	/*
	 * Program RxDMA to override the reo destination indication
	 * with REO2PPE_DST_IND, when use_ppe is set to 1 in RX_MSDU_END,
	 * thereby driving the packet to REO2PPE ring.
	 * If the MSDU is spanning more than 1 buffer, then this
	 * override is not done.
	 */
	htt_cfg.override = 1;
	htt_cfg.reo_destination_indication = REO2PPE_DST_IND;
	htt_cfg.multi_buffer_msdu_override_en = 0;

	/*
	 * Override use_ppe to 0 in RxOLE for the following
	 * cases.
	 */
	htt_cfg.intra_bss_override = 1;
	htt_cfg.decap_raw_override = 1;
	htt_cfg.decap_nwifi_override = 1;
	htt_cfg.ip_frag_override = 1;

	status = dp_htt_rxdma_rxole_ppe_cfg_set(soc, &htt_cfg);
	if (status != QDF_STATUS_SUCCESS)
		dp_err("RxOLE and RxDMA PPE config failed %d", status);

	return status;
}

#else
static inline
QDF_STATUS dp_soc_target_ppe_rxole_rxdma_cfg(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_SUPPORT_PPEDS */

#ifdef DP_UMAC_HW_RESET_SUPPORT
static void dp_register_umac_reset_handlers(struct dp_soc *soc)
{
	dp_umac_reset_register_rx_action_callback(soc,
					dp_umac_reset_action_trigger_recovery,
					UMAC_RESET_ACTION_DO_TRIGGER_RECOVERY);

	dp_umac_reset_register_rx_action_callback(soc,
		dp_umac_reset_handle_pre_reset, UMAC_RESET_ACTION_DO_PRE_RESET);

	dp_umac_reset_register_rx_action_callback(soc,
					dp_umac_reset_handle_post_reset,
					UMAC_RESET_ACTION_DO_POST_RESET_START);

	dp_umac_reset_register_rx_action_callback(soc,
				dp_umac_reset_handle_post_reset_complete,
				UMAC_RESET_ACTION_DO_POST_RESET_COMPLETE);

}
#else
static void dp_register_umac_reset_handlers(struct dp_soc *soc)
{
}
#endif
/**
 * dp_soc_attach_target_wifi3() - SOC initialization in the target
 * @cdp_soc: Opaque Datapath SOC handle
 *
 * Return: zero on success, non-zero on failure
 */
static QDF_STATUS
dp_soc_attach_target_wifi3(struct cdp_soc_t *cdp_soc)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hal_reo_params reo_params;

	htt_soc_attach_target(soc->htt_handle);

	status = dp_soc_target_ppe_rxole_rxdma_cfg(soc);
	if (status != QDF_STATUS_SUCCESS) {
		dp_err("Failed to send htt RxOLE and RxDMA messages to target");
		return status;
	}

	status = dp_rxdma_ring_config(soc);
	if (status != QDF_STATUS_SUCCESS) {
		dp_err("Failed to send htt srng setup messages to target");
		return status;
	}

	status = soc->arch_ops.dp_rxdma_ring_sel_cfg(soc);
	if (status != QDF_STATUS_SUCCESS) {
		dp_err("Failed to send htt ring config message to target");
		return status;
	}

	status = dp_soc_umac_reset_init(cdp_soc);
	if (status != QDF_STATUS_SUCCESS &&
	    status != QDF_STATUS_E_NOSUPPORT) {
		dp_err("Failed to initialize UMAC reset");
		return status;
	}

	dp_register_umac_reset_handlers(soc);

	status = dp_rx_target_fst_config(soc);
	if (status != QDF_STATUS_SUCCESS &&
	    status != QDF_STATUS_E_NOSUPPORT) {
		dp_err("Failed to send htt fst setup config message to target");
		return status;
	}

	DP_STATS_INIT(soc);

	dp_runtime_init(soc);

	/* Enable HW vdev offload stats if feature is supported */
	dp_vdev_stats_hw_offload_target_config(soc, INVALID_PDEV_ID, true);

	/* initialize work queue for stats processing */
	qdf_create_work(0, &soc->htt_stats.work, htt_t2h_stats_handler, soc);

	wlan_cfg_soc_update_tgt_params(soc->wlan_cfg_ctx,
				       soc->ctrl_psoc);
	/* Setup HW REO */
	qdf_mem_zero(&reo_params, sizeof(reo_params));

	if (wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx)) {
		/*
		 * Reo ring remap is not required if both radios
		 * are offloaded to NSS
		 */

		if (soc->arch_ops.reo_remap_config(soc, &reo_params.remap0,
						   &reo_params.remap1,
						   &reo_params.remap2))
			reo_params.rx_hash_enabled = true;
		else
			reo_params.rx_hash_enabled = false;
	}

	/*
	 * set the fragment destination ring
	 */
	dp_reo_frag_dst_set(soc, &reo_params.frag_dst_ring);

	if (wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx))
		reo_params.alt_dst_ind_0 = REO_REMAP_RELEASE;

	reo_params.reo_qref = &soc->reo_qref;
	hal_reo_setup(soc->hal_soc, &reo_params, 1);

	hal_reo_set_err_dst_remap(soc->hal_soc);

	soc->features.pn_in_reo_dest = hal_reo_enable_pn_in_dest(soc->hal_soc);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_vdev_id_map_tbl_add() - Add vdev into vdev_id table
 * @soc: SoC handle
 * @vdev: vdev handle
 * @vdev_id: vdev_id
 *
 * Return: None
 */
static void dp_vdev_id_map_tbl_add(struct dp_soc *soc,
				   struct dp_vdev *vdev,
				   uint8_t vdev_id)
{
	QDF_ASSERT(vdev_id <= MAX_VDEV_CNT);

	qdf_spin_lock_bh(&soc->vdev_map_lock);

	if (dp_vdev_get_ref(soc, vdev, DP_MOD_ID_CONFIG) !=
			QDF_STATUS_SUCCESS) {
		dp_vdev_info("%pK: unable to get vdev reference at MAP vdev %pK vdev_id %u",
			     soc, vdev, vdev_id);
		qdf_spin_unlock_bh(&soc->vdev_map_lock);
		return;
	}

	if (!soc->vdev_id_map[vdev_id])
		soc->vdev_id_map[vdev_id] = vdev;
	else
		QDF_ASSERT(0);

	qdf_spin_unlock_bh(&soc->vdev_map_lock);
}

/**
 * dp_vdev_id_map_tbl_remove() - remove vdev from vdev_id table
 * @soc: SoC handle
 * @vdev: vdev handle
 *
 * Return: None
 */
static void dp_vdev_id_map_tbl_remove(struct dp_soc *soc,
				      struct dp_vdev *vdev)
{
	qdf_spin_lock_bh(&soc->vdev_map_lock);
	QDF_ASSERT(soc->vdev_id_map[vdev->vdev_id] == vdev);

	soc->vdev_id_map[vdev->vdev_id] = NULL;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CONFIG);
	qdf_spin_unlock_bh(&soc->vdev_map_lock);
}

/**
 * dp_vdev_pdev_list_add() - add vdev into pdev's list
 * @soc: soc handle
 * @pdev: pdev handle
 * @vdev: vdev handle
 *
 * Return: none
 */
static void dp_vdev_pdev_list_add(struct dp_soc *soc,
				  struct dp_pdev *pdev,
				  struct dp_vdev *vdev)
{
	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	if (dp_vdev_get_ref(soc, vdev, DP_MOD_ID_CONFIG) !=
			QDF_STATUS_SUCCESS) {
		dp_vdev_info("%pK: unable to get vdev reference at MAP vdev %pK",
			     soc, vdev);
		qdf_spin_unlock_bh(&pdev->vdev_list_lock);
		return;
	}
	/* add this vdev into the pdev's list */
	TAILQ_INSERT_TAIL(&pdev->vdev_list, vdev, vdev_list_elem);
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);
}

/**
 * dp_vdev_pdev_list_remove() - remove vdev from pdev's list
 * @soc: SoC handle
 * @pdev: pdev handle
 * @vdev: VDEV handle
 *
 * Return: none
 */
static void dp_vdev_pdev_list_remove(struct dp_soc *soc,
				     struct dp_pdev *pdev,
				     struct dp_vdev *vdev)
{
	uint8_t found = 0;
	struct dp_vdev *tmpvdev = NULL;

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	TAILQ_FOREACH(tmpvdev, &pdev->vdev_list, vdev_list_elem) {
		if (tmpvdev == vdev) {
			found = 1;
			break;
		}
	}

	if (found) {
		TAILQ_REMOVE(&pdev->vdev_list, vdev, vdev_list_elem);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CONFIG);
	} else {
		dp_vdev_debug("%pK: vdev:%pK not found in pdev:%pK vdevlist:%pK",
			      soc, vdev, pdev, &pdev->vdev_list);
		QDF_ASSERT(0);
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);
}

#ifdef QCA_SUPPORT_EAPOL_OVER_CONTROL_PORT
/**
 * dp_vdev_init_rx_eapol() - initializing osif_rx_eapol
 * @vdev: Datapath VDEV handle
 *
 * Return: None
 */
static inline void dp_vdev_init_rx_eapol(struct dp_vdev *vdev)
{
	vdev->osif_rx_eapol = NULL;
}

/**
 * dp_vdev_register_rx_eapol() - Register VDEV operations for rx_eapol
 * @vdev: DP vdev handle
 * @txrx_ops: Tx and Rx operations
 *
 * Return: None
 */
static inline void dp_vdev_register_rx_eapol(struct dp_vdev *vdev,
					     struct ol_txrx_ops *txrx_ops)
{
	vdev->osif_rx_eapol = txrx_ops->rx.rx_eapol;
}
#else
static inline void dp_vdev_init_rx_eapol(struct dp_vdev *vdev)
{
}

static inline void dp_vdev_register_rx_eapol(struct dp_vdev *vdev,
					     struct ol_txrx_ops *txrx_ops)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
static inline void dp_vdev_save_mld_addr(struct dp_vdev *vdev,
					 struct cdp_vdev_info *vdev_info)
{
	if (vdev_info->mld_mac_addr)
		qdf_mem_copy(&vdev->mld_mac_addr.raw[0],
			     vdev_info->mld_mac_addr, QDF_MAC_ADDR_SIZE);
}

#ifdef WLAN_MLO_MULTI_CHIP
static inline void
dp_vdev_update_bridge_vdev_param(struct dp_vdev *vdev,
				 struct cdp_vdev_info *vdev_info)
{
	if (vdev_info->is_bridge_vap)
		vdev->is_bridge_vdev = 1;

	dp_info("is_bridge_link = %d vdev id = %d chip id = %d",
		vdev->is_bridge_vdev, vdev->vdev_id,
		dp_get_chip_id(vdev->pdev->soc));
}
#else
static inline void
dp_vdev_update_bridge_vdev_param(struct dp_vdev *vdev,
				 struct cdp_vdev_info *vdev_info)
{
}
#endif /* WLAN_MLO_MULTI_CHIP */

#else
static inline void dp_vdev_save_mld_addr(struct dp_vdev *vdev,
					 struct cdp_vdev_info *vdev_info)
{

}

static inline void
dp_vdev_update_bridge_vdev_param(struct dp_vdev *vdev,
				 struct cdp_vdev_info *vdev_info)
{
}
#endif

#ifdef DP_TRAFFIC_END_INDICATION
/**
 * dp_tx_vdev_traffic_end_indication_attach() - Initialize data end indication
 *                                              related members in VDEV
 * @vdev: DP vdev handle
 *
 * Return: None
 */
static inline void
dp_tx_vdev_traffic_end_indication_attach(struct dp_vdev *vdev)
{
	qdf_nbuf_queue_init(&vdev->end_ind_pkt_q);
}

/**
 * dp_tx_vdev_traffic_end_indication_detach() - De-init data end indication
 *                                              related members in VDEV
 * @vdev: DP vdev handle
 *
 * Return: None
 */
static inline void
dp_tx_vdev_traffic_end_indication_detach(struct dp_vdev *vdev)
{
	qdf_nbuf_t nbuf;

	while ((nbuf = qdf_nbuf_queue_remove(&vdev->end_ind_pkt_q)) != NULL)
		qdf_nbuf_free(nbuf);
}
#else
static inline void
dp_tx_vdev_traffic_end_indication_attach(struct dp_vdev *vdev)
{}

static inline void
dp_tx_vdev_traffic_end_indication_detach(struct dp_vdev *vdev)
{}
#endif

#ifdef WLAN_DP_VDEV_NO_SELF_PEER
static inline bool dp_vdev_self_peer_required(struct dp_soc *soc,
					      struct dp_vdev *vdev)
{
	return false;
}
#else
static inline bool dp_vdev_self_peer_required(struct dp_soc *soc,
					      struct dp_vdev *vdev)
{
	if (wlan_op_mode_sta == vdev->opmode)
		return true;

	return false;
}
#endif

/**
 * dp_vdev_attach_wifi3() - attach txrx vdev
 * @cdp_soc: CDP SoC context
 * @pdev_id: PDEV ID for vdev creation
 * @vdev_info: parameters used for vdev creation
 *
 * Return: status
 */
static QDF_STATUS dp_vdev_attach_wifi3(struct cdp_soc_t *cdp_soc,
				       uint8_t pdev_id,
				       struct cdp_vdev_info *vdev_info)
{
	int i = 0;
	qdf_size_t vdev_context_size;
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	struct dp_vdev *vdev;
	uint8_t *vdev_mac_addr = vdev_info->vdev_mac_addr;
	uint8_t vdev_id = vdev_info->vdev_id;
	enum wlan_op_mode op_mode = vdev_info->op_mode;
	enum wlan_op_subtype subtype = vdev_info->subtype;
	enum QDF_OPMODE qdf_opmode = vdev_info->qdf_opmode;
	uint8_t vdev_stats_id = vdev_info->vdev_stats_id;

	vdev_context_size =
		soc->arch_ops.txrx_get_context_size(DP_CONTEXT_TYPE_VDEV);
	vdev = qdf_mem_malloc(vdev_context_size);

	if (!pdev) {
		dp_init_err("%pK: DP PDEV is Null for pdev id %d",
			    cdp_soc, pdev_id);
		qdf_mem_free(vdev);
		goto fail0;
	}

	if (!vdev) {
		dp_init_err("%pK: DP VDEV memory allocation failed",
			    cdp_soc);
		goto fail0;
	}

	wlan_minidump_log(vdev, sizeof(*vdev), soc->ctrl_psoc,
			  WLAN_MD_DP_VDEV, "dp_vdev");

	vdev->pdev = pdev;
	vdev->vdev_id = vdev_id;
	vdev->vdev_stats_id = vdev_stats_id;
	vdev->opmode = op_mode;
	vdev->subtype = subtype;
	vdev->qdf_opmode = qdf_opmode;
	vdev->osdev = soc->osdev;

	vdev->osif_rx = NULL;
	vdev->osif_rsim_rx_decap = NULL;
	vdev->osif_get_key = NULL;
	vdev->osif_tx_free_ext = NULL;
	vdev->osif_vdev = NULL;

	vdev->delete.pending = 0;
	vdev->safemode = 0;
	vdev->drop_unenc = 1;
	vdev->sec_type = cdp_sec_type_none;
	vdev->multipass_en = false;
	vdev->wrap_vdev = false;
	dp_vdev_init_rx_eapol(vdev);
	qdf_atomic_init(&vdev->ref_cnt);
	for (i = 0; i < DP_MOD_ID_MAX; i++)
		qdf_atomic_init(&vdev->mod_refs[i]);

	/* Take one reference for create*/
	qdf_atomic_inc(&vdev->ref_cnt);
	qdf_atomic_inc(&vdev->mod_refs[DP_MOD_ID_CONFIG]);
	vdev->num_peers = 0;
#ifdef notyet
	vdev->filters_num = 0;
#endif
	vdev->lmac_id = pdev->lmac_id;

	qdf_mem_copy(&vdev->mac_addr.raw[0], vdev_mac_addr, QDF_MAC_ADDR_SIZE);

	dp_vdev_update_bridge_vdev_param(vdev, vdev_info);
	dp_vdev_save_mld_addr(vdev, vdev_info);

	/* TODO: Initialize default HTT meta data that will be used in
	 * TCL descriptors for packets transmitted from this VDEV
	 */

	qdf_spinlock_create(&vdev->peer_list_lock);
	TAILQ_INIT(&vdev->peer_list);
	dp_peer_multipass_list_init(vdev);
	if ((soc->intr_mode == DP_INTR_POLL) &&
	    wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx) != 0) {
		if ((pdev->vdev_count == 0) ||
		    (wlan_op_mode_monitor == vdev->opmode))
			qdf_timer_mod(&soc->int_timer, DP_INTR_POLL_TIMER_MS);
	} else if (dp_soc_get_con_mode(soc) == QDF_GLOBAL_MISSION_MODE &&
		   soc->intr_mode == DP_INTR_MSI &&
		   wlan_op_mode_monitor == vdev->opmode &&
		   !dp_mon_mode_local_pkt_capture(soc)) {
		/* Timer to reap status ring in mission mode */
		dp_monitor_vdev_timer_start(soc);
	}

	dp_vdev_id_map_tbl_add(soc, vdev, vdev_id);

	if (wlan_op_mode_monitor == vdev->opmode) {
		if (dp_monitor_vdev_attach(vdev) == QDF_STATUS_SUCCESS) {
			dp_monitor_pdev_set_mon_vdev(vdev);
			return dp_monitor_vdev_set_monitor_mode_buf_rings(pdev);
		}
		return QDF_STATUS_E_FAILURE;
	}

	vdev->tx_encap_type = wlan_cfg_pkt_type(soc->wlan_cfg_ctx);
	vdev->rx_decap_type = wlan_cfg_pkt_type(soc->wlan_cfg_ctx);
	vdev->dscp_tid_map_id = 0;
	vdev->mcast_enhancement_en = 0;
	vdev->igmp_mcast_enhanc_en = 0;
	vdev->raw_mode_war = wlan_cfg_get_raw_mode_war(soc->wlan_cfg_ctx);
	vdev->prev_tx_enq_tstamp = 0;
	vdev->prev_rx_deliver_tstamp = 0;
	vdev->skip_sw_tid_classification = DP_TX_HW_DSCP_TID_MAP_VALID;
	dp_tx_vdev_traffic_end_indication_attach(vdev);

	dp_vdev_pdev_list_add(soc, pdev, vdev);
	pdev->vdev_count++;

	if (wlan_op_mode_sta != vdev->opmode &&
	    wlan_op_mode_ndi != vdev->opmode)
		vdev->ap_bridge_enabled = true;
	else
		vdev->ap_bridge_enabled = false;
	dp_init_info("%pK: wlan_cfg_ap_bridge_enabled %d",
		     cdp_soc, vdev->ap_bridge_enabled);

	dp_tx_vdev_attach(vdev);

	dp_monitor_vdev_attach(vdev);
	if (!pdev->is_lro_hash_configured) {
		if (QDF_IS_STATUS_SUCCESS(dp_lro_hash_setup(soc, pdev)))
			pdev->is_lro_hash_configured = true;
		else
			dp_err("LRO hash setup failure!");
	}

	dp_cfg_event_record_vdev_evt(soc, DP_CFG_EVENT_VDEV_ATTACH, vdev);
	dp_info("Created vdev %pK ("QDF_MAC_ADDR_FMT") vdev_id %d", vdev,
		QDF_MAC_ADDR_REF(vdev->mac_addr.raw), vdev->vdev_id);
	DP_STATS_INIT(vdev);

	if (QDF_IS_STATUS_ERROR(soc->arch_ops.txrx_vdev_attach(soc, vdev)))
		goto fail0;

	if (dp_vdev_self_peer_required(soc, vdev))
		dp_peer_create_wifi3((struct cdp_soc_t *)soc, vdev_id,
				     vdev->mac_addr.raw, CDP_LINK_PEER_TYPE);

	dp_pdev_update_fast_rx_flag(soc, pdev);

	return QDF_STATUS_SUCCESS;

fail0:
	return QDF_STATUS_E_FAILURE;
}

#ifndef QCA_HOST_MODE_WIFI_DISABLED
/**
 * dp_vdev_fetch_tx_handler() - Fetch Tx handlers
 * @vdev: struct dp_vdev *
 * @soc: struct dp_soc *
 * @ctx: struct ol_txrx_hardtart_ctxt *
 */
static inline void dp_vdev_fetch_tx_handler(struct dp_vdev *vdev,
					    struct dp_soc *soc,
					    struct ol_txrx_hardtart_ctxt *ctx)
{
	/* Enable vdev_id check only for ap, if flag is enabled */
	if (vdev->mesh_vdev)
		ctx->tx = dp_tx_send_mesh;
	else if ((wlan_cfg_is_tx_per_pkt_vdev_id_check_enabled(soc->wlan_cfg_ctx)) &&
		 (vdev->opmode == wlan_op_mode_ap)) {
		ctx->tx = dp_tx_send_vdev_id_check;
		ctx->tx_fast = dp_tx_send_vdev_id_check;
	} else {
		ctx->tx = dp_tx_send;
		ctx->tx_fast = soc->arch_ops.dp_tx_send_fast;
	}

	/* Avoid check in regular exception Path */
	if ((wlan_cfg_is_tx_per_pkt_vdev_id_check_enabled(soc->wlan_cfg_ctx)) &&
	    (vdev->opmode == wlan_op_mode_ap))
		ctx->tx_exception = dp_tx_send_exception_vdev_id_check;
	else
		ctx->tx_exception = dp_tx_send_exception;
}

/**
 * dp_vdev_register_tx_handler() - Register Tx handler
 * @vdev: struct dp_vdev *
 * @soc: struct dp_soc *
 * @txrx_ops: struct ol_txrx_ops *
 */
static inline void dp_vdev_register_tx_handler(struct dp_vdev *vdev,
					       struct dp_soc *soc,
					       struct ol_txrx_ops *txrx_ops)
{
	struct ol_txrx_hardtart_ctxt ctx = {0};

	dp_vdev_fetch_tx_handler(vdev, soc, &ctx);

	txrx_ops->tx.tx = ctx.tx;
	txrx_ops->tx.tx_fast = ctx.tx_fast;
	txrx_ops->tx.tx_exception = ctx.tx_exception;

	dp_info("Configure tx_vdev_id_chk_handler Feature Flag: %d and mode:%d for vdev_id:%d",
		wlan_cfg_is_tx_per_pkt_vdev_id_check_enabled(soc->wlan_cfg_ctx),
		vdev->opmode, vdev->vdev_id);
}
#else /* QCA_HOST_MODE_WIFI_DISABLED */
static inline void dp_vdev_register_tx_handler(struct dp_vdev *vdev,
					       struct dp_soc *soc,
					       struct ol_txrx_ops *txrx_ops)
{
}

static inline void dp_vdev_fetch_tx_handler(struct dp_vdev *vdev,
					    struct dp_soc *soc,
					    struct ol_txrx_hardtart_ctxt *ctx)
{
}
#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_vdev_register_wifi3() - Register VDEV operations from osif layer
 * @soc_hdl: Datapath soc handle
 * @vdev_id: id of Datapath VDEV handle
 * @osif_vdev: OSIF vdev handle
 * @txrx_ops: Tx and Rx operations
 *
 * Return: DP VDEV handle on success, NULL on failure
 */
static QDF_STATUS dp_vdev_register_wifi3(struct cdp_soc_t *soc_hdl,
					 uint8_t vdev_id,
					 ol_osif_vdev_handle osif_vdev,
					 struct ol_txrx_ops *txrx_ops)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev =	dp_vdev_get_ref_by_id(soc, vdev_id,
						      DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	vdev->osif_vdev = osif_vdev;
	vdev->osif_rx = txrx_ops->rx.rx;
	vdev->osif_rx_stack = txrx_ops->rx.rx_stack;
	vdev->osif_rx_flush = txrx_ops->rx.rx_flush;
	vdev->osif_gro_flush = txrx_ops->rx.rx_gro_flush;
	vdev->osif_rsim_rx_decap = txrx_ops->rx.rsim_rx_decap;
	vdev->osif_fisa_rx = txrx_ops->rx.osif_fisa_rx;
	vdev->osif_fisa_flush = txrx_ops->rx.osif_fisa_flush;
	vdev->osif_get_key = txrx_ops->get_key;
	dp_monitor_vdev_register_osif(vdev, txrx_ops);
	vdev->osif_tx_free_ext = txrx_ops->tx.tx_free_ext;
	vdev->tx_comp = txrx_ops->tx.tx_comp;
	vdev->stats_cb = txrx_ops->rx.stats_rx;
	vdev->tx_classify_critical_pkt_cb =
		txrx_ops->tx.tx_classify_critical_pkt_cb;
#ifdef notyet
#if ATH_SUPPORT_WAPI
	vdev->osif_check_wai = txrx_ops->rx.wai_check;
#endif
#endif
#ifdef UMAC_SUPPORT_PROXY_ARP
	vdev->osif_proxy_arp = txrx_ops->proxy_arp;
#endif
	vdev->me_convert = txrx_ops->me_convert;
	vdev->get_tsf_time = txrx_ops->get_tsf_time;
	vdev->vdev_del_notify = txrx_ops->vdev_del_notify;

	dp_vdev_register_rx_eapol(vdev, txrx_ops);

	dp_vdev_register_tx_handler(vdev, soc, txrx_ops);

	dp_init_info("%pK: DP Vdev Register success", soc);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
void dp_peer_delete(struct dp_soc *soc,
		    struct dp_peer *peer,
		    void *arg)
{
	if (!peer->valid)
		return;

	dp_peer_delete_wifi3((struct cdp_soc_t *)soc,
			     peer->vdev->vdev_id,
			     peer->mac_addr.raw, 0,
			     peer->peer_type);
}
#else
void dp_peer_delete(struct dp_soc *soc,
		    struct dp_peer *peer,
		    void *arg)
{
	if (!peer->valid)
		return;

	dp_peer_delete_wifi3((struct cdp_soc_t *)soc,
			     peer->vdev->vdev_id,
			     peer->mac_addr.raw, 0,
			     CDP_LINK_PEER_TYPE);
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
static uint8_t
dp_mlo_get_num_link_peer(struct dp_soc *soc, struct dp_peer *peer)
{
	if (soc->cdp_soc.ol_ops->peer_get_num_mlo_links)
		return soc->cdp_soc.ol_ops->peer_get_num_mlo_links(
				soc->ctrl_psoc,
				peer->vdev->vdev_id,
				peer->mac_addr.raw,
				IS_MLO_DP_MLD_PEER(peer));

	return 0;
}

void dp_mlo_peer_delete(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	if (!peer->valid)
		return;

	/* skip deleting the SLO peers */
	if (dp_mlo_get_num_link_peer(soc, peer) == 1)
		return;

	if (IS_MLO_DP_LINK_PEER(peer))
		dp_peer_delete_wifi3((struct cdp_soc_t *)soc,
				     peer->vdev->vdev_id,
				     peer->mac_addr.raw, 0,
				     CDP_LINK_PEER_TYPE);
}

/**
 * dp_mlo_link_peer_flush() - flush all the link peers
 * @soc: Datapath soc handle
 * @peer: DP peer handle to be checked
 *
 * Return: None
 */
static void dp_mlo_link_peer_flush(struct dp_soc *soc, struct dp_peer *peer)
{
	int cnt = 0;
	struct dp_peer *link_peer = NULL;
	struct dp_mld_link_peers link_peers_info = {NULL};

	if (!IS_MLO_DP_MLD_PEER(peer))
		return;

	/* get link peers with reference */
	dp_get_link_peers_ref_from_mld_peer(soc, peer, &link_peers_info,
					    DP_MOD_ID_CDP);
	for (cnt = 0; cnt < link_peers_info.num_links; cnt++) {
		link_peer = link_peers_info.link_peers[cnt];
		if (!link_peer)
			continue;

		/* delete all the link peers */
		dp_mlo_peer_delete(link_peer->vdev->pdev->soc, link_peer, NULL);
		/* unmap all the link peers */
		dp_rx_peer_unmap_handler(link_peer->vdev->pdev->soc,
					 link_peer->peer_id,
					 link_peer->vdev->vdev_id,
					 link_peer->mac_addr.raw, 0,
					 DP_PEER_WDS_COUNT_INVALID);
	}
	dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
}
#else
static uint8_t
dp_mlo_get_num_link_peer(struct dp_soc *soc, struct dp_peer *peer)
{
	return 0;
}

void dp_mlo_peer_delete(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
}

static void dp_mlo_link_peer_flush(struct dp_soc *soc, struct dp_peer *peer)
{
}
#endif
/**
 * dp_vdev_flush_peers() - Forcibily Flush peers of vdev
 * @vdev_handle: Datapath VDEV handle
 * @unmap_only: Flag to indicate "only unmap"
 * @mlo_peers_only: true if only MLO peers should be flushed
 *
 * Return: void
 */
static void dp_vdev_flush_peers(struct cdp_vdev *vdev_handle,
				bool unmap_only,
				bool mlo_peers_only)
{
	struct dp_vdev *vdev = (struct dp_vdev *)vdev_handle;
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_soc *soc = pdev->soc;
	struct dp_peer *peer;
	uint32_t i = 0;


	if (!unmap_only) {
		if (!mlo_peers_only)
			dp_vdev_iterate_peer_lock_safe(vdev,
						       dp_peer_delete,
						       NULL,
						       DP_MOD_ID_CDP);
		else
			dp_vdev_iterate_peer_lock_safe(vdev,
						       dp_mlo_peer_delete,
						       NULL,
						       DP_MOD_ID_CDP);
	}

	for (i = 0; i < soc->max_peer_id ; i++) {
		peer = __dp_peer_get_ref_by_id(soc, i, DP_MOD_ID_CDP);

		if (!peer)
			continue;

		if (peer->vdev != vdev) {
			dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
			continue;
		}

		if (!mlo_peers_only) {
			dp_info("peer: " QDF_MAC_ADDR_FMT " is getting unmap",
				QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			dp_mlo_link_peer_flush(soc, peer);
			dp_rx_peer_unmap_handler(soc, i,
						 vdev->vdev_id,
						 peer->mac_addr.raw, 0,
						 DP_PEER_WDS_COUNT_INVALID);
			if (!IS_MLO_DP_MLD_PEER(peer))
				SET_PEER_REF_CNT_ONE(peer);
		} else if (IS_MLO_DP_LINK_PEER(peer) ||
			   IS_MLO_DP_MLD_PEER(peer)) {
			dp_info("peer: " QDF_MAC_ADDR_FMT " is getting unmap",
				QDF_MAC_ADDR_REF(peer->mac_addr.raw));

			/* skip deleting the SLO peers */
			if (dp_mlo_get_num_link_peer(soc, peer) ==  1) {
				dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
				continue;
			}

			dp_mlo_link_peer_flush(soc, peer);
			dp_rx_peer_unmap_handler(soc, i,
						 vdev->vdev_id,
						 peer->mac_addr.raw, 0,
						 DP_PEER_WDS_COUNT_INVALID);
			if (!IS_MLO_DP_MLD_PEER(peer))
				SET_PEER_REF_CNT_ONE(peer);
		}

		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	}
}

#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
/**
 * dp_txrx_alloc_vdev_stats_id()- Allocate vdev_stats_id
 * @soc_hdl: Datapath soc handle
 * @vdev_stats_id: Address of vdev_stats_id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_txrx_alloc_vdev_stats_id(struct cdp_soc_t *soc_hdl,
					      uint8_t *vdev_stats_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	uint8_t id = 0;

	if (!wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx)) {
		*vdev_stats_id = CDP_INVALID_VDEV_STATS_ID;
		return QDF_STATUS_E_FAILURE;
	}

	while (id < CDP_MAX_VDEV_STATS_ID) {
		if (!qdf_atomic_test_and_set_bit(id, &soc->vdev_stats_id_map)) {
			*vdev_stats_id = id;
			return QDF_STATUS_SUCCESS;
		}
		id++;
	}

	*vdev_stats_id = CDP_INVALID_VDEV_STATS_ID;
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_txrx_reset_vdev_stats_id() - Reset vdev_stats_id in dp_soc
 * @soc_hdl: Datapath soc handle
 * @vdev_stats_id: vdev_stats_id to reset in dp_soc
 *
 * Return: none
 */
static void dp_txrx_reset_vdev_stats_id(struct cdp_soc_t *soc_hdl,
					uint8_t vdev_stats_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if ((!wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx)) ||
	    (vdev_stats_id >= CDP_MAX_VDEV_STATS_ID))
		return;

	qdf_atomic_clear_bit(vdev_stats_id, &soc->vdev_stats_id_map);
}
#else
static void dp_txrx_reset_vdev_stats_id(struct cdp_soc_t *soc,
					uint8_t vdev_stats_id)
{}
#endif
/**
 * dp_vdev_detach_wifi3() - Detach txrx vdev
 * @cdp_soc: Datapath soc handle
 * @vdev_id: VDEV Id
 * @callback: Callback OL_IF on completion of detach
 * @cb_context:	Callback context
 *
 */
static QDF_STATUS dp_vdev_detach_wifi3(struct cdp_soc_t *cdp_soc,
				       uint8_t vdev_id,
				       ol_txrx_vdev_delete_cb callback,
				       void *cb_context)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_pdev *pdev;
	struct dp_neighbour_peer *peer = NULL;
	struct dp_peer *vap_self_peer = NULL;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	soc->arch_ops.txrx_vdev_detach(soc, vdev);

	pdev = vdev->pdev;

	vap_self_peer = dp_sta_vdev_self_peer_ref_n_get(soc, vdev,
							DP_MOD_ID_CONFIG);
	if (vap_self_peer) {
		qdf_spin_lock_bh(&soc->ast_lock);
		if (vap_self_peer->self_ast_entry) {
			dp_peer_del_ast(soc, vap_self_peer->self_ast_entry);
			vap_self_peer->self_ast_entry = NULL;
		}
		qdf_spin_unlock_bh(&soc->ast_lock);

		dp_peer_delete_wifi3((struct cdp_soc_t *)soc, vdev->vdev_id,
				     vap_self_peer->mac_addr.raw, 0,
				     CDP_LINK_PEER_TYPE);
		dp_peer_unref_delete(vap_self_peer, DP_MOD_ID_CONFIG);
	}

	/*
	 * If Target is hung, flush all peers before detaching vdev
	 * this will free all references held due to missing
	 * unmap commands from Target
	 */
	if (!hif_is_target_ready(HIF_GET_SOFTC(soc->hif_handle)))
		dp_vdev_flush_peers((struct cdp_vdev *)vdev, false, false);
	else if (hif_get_target_status(soc->hif_handle) == TARGET_STATUS_RESET)
		dp_vdev_flush_peers((struct cdp_vdev *)vdev, true, false);

	/* indicate that the vdev needs to be deleted */
	vdev->delete.pending = 1;
	dp_rx_vdev_detach(vdev);
	/*
	 * move it after dp_rx_vdev_detach(),
	 * as the call back done in dp_rx_vdev_detach()
	 * still need to get vdev pointer by vdev_id.
	 */
	dp_vdev_id_map_tbl_remove(soc, vdev);

	dp_monitor_neighbour_peer_list_remove(pdev, vdev, peer);

	dp_txrx_reset_vdev_stats_id(cdp_soc, vdev->vdev_stats_id);

	dp_tx_vdev_multipass_deinit(vdev);
	dp_tx_vdev_traffic_end_indication_detach(vdev);

	if (vdev->vdev_dp_ext_handle) {
		qdf_mem_free(vdev->vdev_dp_ext_handle);
		vdev->vdev_dp_ext_handle = NULL;
	}
	vdev->delete.callback = callback;
	vdev->delete.context = cb_context;

	if (vdev->opmode != wlan_op_mode_monitor)
		dp_vdev_pdev_list_remove(soc, pdev, vdev);

	pdev->vdev_count--;
	/* release reference taken above for find */
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	qdf_spin_lock_bh(&soc->inactive_vdev_list_lock);
	TAILQ_INSERT_TAIL(&soc->inactive_vdev_list, vdev, inactive_list_elem);
	qdf_spin_unlock_bh(&soc->inactive_vdev_list_lock);

	dp_cfg_event_record_vdev_evt(soc, DP_CFG_EVENT_VDEV_DETACH, vdev);
	dp_info("detach vdev %pK id %d pending refs %d",
		vdev, vdev->vdev_id, qdf_atomic_read(&vdev->ref_cnt));

	/* release reference taken at dp_vdev_create */
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CONFIG);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * is_dp_peer_can_reuse() - check if the dp_peer match condition to be reused
 * @vdev: Target DP vdev handle
 * @peer: DP peer handle to be checked
 * @peer_mac_addr: Target peer mac address
 * @peer_type: Target peer type
 *
 * Return: true - if match, false - not match
 */
static inline
bool is_dp_peer_can_reuse(struct dp_vdev *vdev,
			  struct dp_peer *peer,
			  uint8_t *peer_mac_addr,
			  enum cdp_peer_type peer_type)
{
	if (peer->bss_peer && (peer->vdev == vdev) &&
	    (peer->peer_type == peer_type) &&
	    (qdf_mem_cmp(peer_mac_addr, peer->mac_addr.raw,
			 QDF_MAC_ADDR_SIZE) == 0))
		return true;

	return false;
}
#else
static inline
bool is_dp_peer_can_reuse(struct dp_vdev *vdev,
			  struct dp_peer *peer,
			  uint8_t *peer_mac_addr,
			  enum cdp_peer_type peer_type)
{
	if (peer->bss_peer && (peer->vdev == vdev) &&
	    (qdf_mem_cmp(peer_mac_addr, peer->mac_addr.raw,
			 QDF_MAC_ADDR_SIZE) == 0))
		return true;

	return false;
}
#endif

static inline struct dp_peer *dp_peer_can_reuse(struct dp_vdev *vdev,
						uint8_t *peer_mac_addr,
						enum cdp_peer_type peer_type)
{
	struct dp_peer *peer;
	struct dp_soc *soc = vdev->pdev->soc;

	qdf_spin_lock_bh(&soc->inactive_peer_list_lock);
	TAILQ_FOREACH(peer, &soc->inactive_peer_list,
		      inactive_list_elem) {

		/* reuse bss peer only when vdev matches*/
		if (is_dp_peer_can_reuse(vdev, peer,
					 peer_mac_addr, peer_type)) {
			/* increment ref count for cdp_peer_create*/
			if (dp_peer_get_ref(soc, peer, DP_MOD_ID_CONFIG) ==
						QDF_STATUS_SUCCESS) {
				TAILQ_REMOVE(&soc->inactive_peer_list, peer,
					     inactive_list_elem);
				qdf_spin_unlock_bh
					(&soc->inactive_peer_list_lock);
				return peer;
			}
		}
	}

	qdf_spin_unlock_bh(&soc->inactive_peer_list_lock);
	return NULL;
}

#ifdef FEATURE_AST
static inline void dp_peer_ast_handle_roam_del(struct dp_soc *soc,
					       struct dp_pdev *pdev,
					       uint8_t *peer_mac_addr)
{
	struct dp_ast_entry *ast_entry;

	if (soc->ast_offload_support)
		return;

	qdf_spin_lock_bh(&soc->ast_lock);
	if (soc->ast_override_support)
		ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, peer_mac_addr,
							    pdev->pdev_id);
	else
		ast_entry = dp_peer_ast_hash_find_soc(soc, peer_mac_addr);

	if (ast_entry && ast_entry->next_hop && !ast_entry->delete_in_progress)
		dp_peer_del_ast(soc, ast_entry);

	qdf_spin_unlock_bh(&soc->ast_lock);
}
#else
static inline void dp_peer_ast_handle_roam_del(struct dp_soc *soc,
					       struct dp_pdev *pdev,
					       uint8_t *peer_mac_addr)
{
}
#endif

#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
/**
 * dp_peer_hw_txrx_stats_init() - Initialize hw_txrx_stats_en in dp_peer
 * @soc: Datapath soc handle
 * @txrx_peer: Datapath peer handle
 *
 * Return: none
 */
static inline
void dp_peer_hw_txrx_stats_init(struct dp_soc *soc,
				struct dp_txrx_peer *txrx_peer)
{
	txrx_peer->hw_txrx_stats_en =
		wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx);
}
#else
static inline
void dp_peer_hw_txrx_stats_init(struct dp_soc *soc,
				struct dp_txrx_peer *txrx_peer)
{
	txrx_peer->hw_txrx_stats_en = 0;
}
#endif

static QDF_STATUS dp_txrx_peer_detach(struct dp_soc *soc, struct dp_peer *peer)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_pdev *pdev;
	struct cdp_txrx_peer_params_update params = {0};

	/* dp_txrx_peer exists for mld peer and legacy peer */
	if (peer->txrx_peer) {
		txrx_peer = peer->txrx_peer;
		peer->txrx_peer = NULL;
		pdev = txrx_peer->vdev->pdev;

		if ((peer->vdev->opmode != wlan_op_mode_sta) &&
		    !peer->bss_peer) {
			params.vdev_id = peer->vdev->vdev_id;
			params.peer_mac = peer->mac_addr.raw;

			dp_wdi_event_handler(WDI_EVENT_PEER_DELETE, soc,
					     (void *)&params, peer->peer_id,
					     WDI_NO_VAL, pdev->pdev_id);
		}

		dp_peer_defrag_rx_tids_deinit(txrx_peer);
		/*
		 * Deallocate the extended stats contenxt
		 */
		dp_peer_delay_stats_ctx_dealloc(soc, txrx_peer);
		dp_peer_rx_bufq_resources_deinit(txrx_peer);
		dp_peer_jitter_stats_ctx_dealloc(pdev, txrx_peer);
		dp_peer_sawf_stats_ctx_free(soc, txrx_peer);

		qdf_mem_free(txrx_peer);
	}

	return QDF_STATUS_SUCCESS;
}

static inline
uint8_t dp_txrx_peer_calculate_stats_size(struct dp_soc *soc,
					  struct dp_peer *peer)
{
	if ((wlan_cfg_is_peer_link_stats_enabled(soc->wlan_cfg_ctx)) &&
	    IS_MLO_DP_MLD_PEER(peer)) {
		return (DP_MAX_MLO_LINKS + 1);
	}
	return 1;
}

static QDF_STATUS dp_txrx_peer_attach(struct dp_soc *soc, struct dp_peer *peer)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_pdev *pdev;
	struct cdp_txrx_peer_params_update params = {0};
	uint8_t stats_arr_size = 0;

	stats_arr_size = dp_txrx_peer_calculate_stats_size(soc, peer);

	txrx_peer = (struct dp_txrx_peer *)qdf_mem_malloc(sizeof(*txrx_peer) +
							  (stats_arr_size *
							   sizeof(struct dp_peer_stats)));

	if (!txrx_peer)
		return QDF_STATUS_E_NOMEM; /* failure */

	txrx_peer->peer_id = HTT_INVALID_PEER;
	/* initialize the peer_id */
	txrx_peer->vdev = peer->vdev;
	pdev = peer->vdev->pdev;
	txrx_peer->stats_arr_size = stats_arr_size;

	DP_TXRX_PEER_STATS_INIT(txrx_peer,
				(txrx_peer->stats_arr_size *
				sizeof(struct dp_peer_stats)));

	if (!IS_DP_LEGACY_PEER(peer))
		txrx_peer->is_mld_peer = 1;

	dp_wds_ext_peer_init(txrx_peer);
	dp_peer_rx_bufq_resources_init(txrx_peer);
	dp_peer_hw_txrx_stats_init(soc, txrx_peer);
	/*
	 * Allocate peer extended stats context. Fall through in
	 * case of failure as its not an implicit requirement to have
	 * this object for regular statistics updates.
	 */
	if (dp_peer_delay_stats_ctx_alloc(soc, txrx_peer) !=
					  QDF_STATUS_SUCCESS)
		dp_warn("peer delay_stats ctx alloc failed");

	/*
	 * Alloctate memory for jitter stats. Fall through in
	 * case of failure as its not an implicit requirement to have
	 * this object for regular statistics updates.
	 */
	if (dp_peer_jitter_stats_ctx_alloc(pdev, txrx_peer) !=
					   QDF_STATUS_SUCCESS)
		dp_warn("peer jitter_stats ctx alloc failed");

	dp_set_peer_isolation(txrx_peer, false);

	dp_peer_defrag_rx_tids_init(txrx_peer);

	if (dp_peer_sawf_stats_ctx_alloc(soc, txrx_peer) != QDF_STATUS_SUCCESS)
		dp_warn("peer sawf stats alloc failed");

	dp_txrx_peer_attach_add(soc, peer, txrx_peer);

	if ((peer->vdev->opmode == wlan_op_mode_sta) || peer->bss_peer)
		return QDF_STATUS_SUCCESS;

	params.peer_mac = peer->mac_addr.raw;
	params.vdev_id = peer->vdev->vdev_id;
	params.chip_id = dp_get_chip_id(soc);
	params.pdev_id = peer->vdev->pdev->pdev_id;

	dp_wdi_event_handler(WDI_EVENT_TXRX_PEER_CREATE, soc,
			     (void *)&params, peer->peer_id,
			     WDI_NO_VAL, params.pdev_id);

	return QDF_STATUS_SUCCESS;
}

static inline
void dp_txrx_peer_stats_clr(struct dp_txrx_peer *txrx_peer)
{
	if (!txrx_peer)
		return;

	txrx_peer->tx_failed = 0;
	txrx_peer->comp_pkt.num = 0;
	txrx_peer->comp_pkt.bytes = 0;
	txrx_peer->to_stack.num = 0;
	txrx_peer->to_stack.bytes = 0;

	DP_TXRX_PEER_STATS_CLR(txrx_peer,
			       (txrx_peer->stats_arr_size *
			       sizeof(struct dp_peer_stats)));
	dp_peer_delay_stats_ctx_clr(txrx_peer);
	dp_peer_jitter_stats_ctx_clr(txrx_peer);
}

#if defined WLAN_FEATURE_11BE_MLO && defined DP_MLO_LINK_STATS_SUPPORT
/**
 * dp_txrx_peer_reset_local_link_id() - Reset local link id
 * @txrx_peer: txrx peer handle
 *
 * Return: None
 */
static inline void
dp_txrx_peer_reset_local_link_id(struct dp_txrx_peer *txrx_peer)
{
	int i;

	for (i = 0; i <= DP_MAX_MLO_LINKS; i++)
		txrx_peer->ll_band[i] = DP_BAND_INVALID;
}
#else
static inline void
dp_txrx_peer_reset_local_link_id(struct dp_txrx_peer *txrx_peer)
{
}
#endif

/**
 * dp_peer_create_wifi3() - attach txrx peer
 * @soc_hdl: Datapath soc handle
 * @vdev_id: id of vdev
 * @peer_mac_addr: Peer MAC address
 * @peer_type: link or MLD peer type
 *
 * Return: 0 on success, -1 on failure
 */
static QDF_STATUS
dp_peer_create_wifi3(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		     uint8_t *peer_mac_addr, enum cdp_peer_type peer_type)
{
	struct dp_peer *peer;
	int i;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev;
	enum cdp_txrx_ast_entry_type ast_type = CDP_TXRX_AST_TYPE_STATIC;
	struct dp_vdev *vdev = NULL;

	if (!peer_mac_addr)
		return QDF_STATUS_E_FAILURE;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	pdev = vdev->pdev;
	soc = pdev->soc;

	/*
	 * If a peer entry with given MAC address already exists,
	 * reuse the peer and reset the state of peer.
	 */
	peer = dp_peer_can_reuse(vdev, peer_mac_addr, peer_type);

	if (peer) {
		qdf_atomic_init(&peer->is_default_route_set);
		dp_peer_cleanup(vdev, peer);

		dp_peer_vdev_list_add(soc, vdev, peer);
		dp_peer_find_hash_add(soc, peer);

		if (dp_peer_rx_tids_create(peer) != QDF_STATUS_SUCCESS) {
			dp_alert("RX tid alloc fail for peer %pK (" QDF_MAC_ADDR_FMT ")",
				 peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
			return QDF_STATUS_E_FAILURE;
		}

		if (IS_MLO_DP_MLD_PEER(peer))
			dp_mld_peer_init_link_peers_info(peer);

		qdf_spin_lock_bh(&soc->ast_lock);
		dp_peer_delete_ast_entries(soc, peer);
		qdf_spin_unlock_bh(&soc->ast_lock);

		if ((vdev->opmode == wlan_op_mode_sta) &&
		    !qdf_mem_cmp(peer_mac_addr, &vdev->mac_addr.raw[0],
		     QDF_MAC_ADDR_SIZE)) {
			ast_type = CDP_TXRX_AST_TYPE_SELF;
		}
		dp_peer_add_ast(soc, peer, peer_mac_addr, ast_type, 0);

		peer->valid = 1;
		peer->is_tdls_peer = false;
		dp_local_peer_id_alloc(pdev, peer);

		qdf_spinlock_create(&peer->peer_info_lock);

		DP_STATS_INIT(peer);

		/*
		 * In tx_monitor mode, filter may be set for unassociated peer
		 * when unassociated peer get associated peer need to
		 * update tx_cap_enabled flag to support peer filter.
		 */
		if (!IS_MLO_DP_MLD_PEER(peer)) {
			dp_monitor_peer_tx_capture_filter_check(pdev, peer);
			dp_monitor_peer_reset_stats(soc, peer);
		}

		if (peer->txrx_peer) {
			dp_peer_rx_bufq_resources_init(peer->txrx_peer);
			dp_txrx_peer_stats_clr(peer->txrx_peer);
			dp_set_peer_isolation(peer->txrx_peer, false);
			dp_wds_ext_peer_init(peer->txrx_peer);
			dp_peer_hw_txrx_stats_init(soc, peer->txrx_peer);
			dp_txrx_peer_reset_local_link_id(peer->txrx_peer);
		}

		dp_cfg_event_record_peer_evt(soc, DP_CFG_EVENT_PEER_CREATE,
					     peer, vdev, 1);
		dp_info("vdev %pK Reused peer %pK ("QDF_MAC_ADDR_FMT
			") vdev_ref_cnt "
			"%d peer_ref_cnt: %d",
			vdev, peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
			qdf_atomic_read(&vdev->ref_cnt),
			qdf_atomic_read(&peer->ref_cnt));
			dp_peer_update_state(soc, peer, DP_PEER_STATE_INIT);

		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_SUCCESS;
	} else {
		/*
		 * When a STA roams from RPTR AP to ROOT AP and vice versa, we
		 * need to remove the AST entry which was earlier added as a WDS
		 * entry.
		 * If an AST entry exists, but no peer entry exists with a given
		 * MAC addresses, we could deduce it as a WDS entry
		 */
		dp_peer_ast_handle_roam_del(soc, pdev, peer_mac_addr);
	}

#ifdef notyet
	peer = (struct dp_peer *)qdf_mempool_alloc(soc->osdev,
		soc->mempool_ol_ath_peer);
#else
	peer = (struct dp_peer *)qdf_mem_malloc(sizeof(*peer));
#endif
	wlan_minidump_log(peer,
			  sizeof(*peer),
			  soc->ctrl_psoc,
			  WLAN_MD_DP_PEER, "dp_peer");
	if (!peer) {
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE; /* failure */
	}

	qdf_mem_zero(peer, sizeof(struct dp_peer));

	/* store provided params */
	peer->vdev = vdev;

	/* initialize the peer_id */
	peer->peer_id = HTT_INVALID_PEER;

	qdf_mem_copy(
		&peer->mac_addr.raw[0], peer_mac_addr, QDF_MAC_ADDR_SIZE);

	DP_PEER_SET_TYPE(peer, peer_type);
	if (IS_MLO_DP_MLD_PEER(peer)) {
		if (dp_txrx_peer_attach(soc, peer) !=
				QDF_STATUS_SUCCESS)
			goto fail; /* failure */

		dp_mld_peer_init_link_peers_info(peer);
	}

	if (dp_monitor_peer_attach(soc, peer) != QDF_STATUS_SUCCESS)
		dp_warn("peer monitor ctx alloc failed");

	TAILQ_INIT(&peer->ast_entry_list);

	/* get the vdev reference for new peer */
	dp_vdev_get_ref(soc, vdev, DP_MOD_ID_CHILD);

	if ((vdev->opmode == wlan_op_mode_sta) &&
	    !qdf_mem_cmp(peer_mac_addr, &vdev->mac_addr.raw[0],
			 QDF_MAC_ADDR_SIZE)) {
		ast_type = CDP_TXRX_AST_TYPE_SELF;
	}
	qdf_spinlock_create(&peer->peer_state_lock);
	dp_peer_add_ast(soc, peer, peer_mac_addr, ast_type, 0);
	qdf_spinlock_create(&peer->peer_info_lock);

	/* reset the ast index to flowid table */
	dp_peer_reset_flowq_map(peer);

	qdf_atomic_init(&peer->ref_cnt);

	for (i = 0; i < DP_MOD_ID_MAX; i++)
		qdf_atomic_init(&peer->mod_refs[i]);

	/* keep one reference for attach */
	qdf_atomic_inc(&peer->ref_cnt);
	qdf_atomic_inc(&peer->mod_refs[DP_MOD_ID_CONFIG]);

	dp_peer_vdev_list_add(soc, vdev, peer);

	/* TODO: See if hash based search is required */
	dp_peer_find_hash_add(soc, peer);

	/* Initialize the peer state */
	peer->state = OL_TXRX_PEER_STATE_DISC;

	dp_cfg_event_record_peer_evt(soc, DP_CFG_EVENT_PEER_CREATE,
				     peer, vdev, 0);
	dp_info("vdev %pK created peer %pK ("QDF_MAC_ADDR_FMT") vdev_ref_cnt "
		"%d peer_ref_cnt: %d",
		vdev, peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		qdf_atomic_read(&vdev->ref_cnt),
		qdf_atomic_read(&peer->ref_cnt));
	/*
	 * For every peer MAp message search and set if bss_peer
	 */
	if (qdf_mem_cmp(peer->mac_addr.raw, vdev->mac_addr.raw,
			QDF_MAC_ADDR_SIZE) == 0 &&
			(wlan_op_mode_sta != vdev->opmode)) {
		dp_info("vdev bss_peer!!");
		peer->bss_peer = 1;
		if (peer->txrx_peer)
			peer->txrx_peer->bss_peer = 1;
	}

	if (wlan_op_mode_sta == vdev->opmode &&
	    qdf_mem_cmp(peer->mac_addr.raw, vdev->mac_addr.raw,
			QDF_MAC_ADDR_SIZE) == 0) {
		peer->sta_self_peer = 1;
	}

	if (dp_peer_rx_tids_create(peer) != QDF_STATUS_SUCCESS) {
		dp_alert("RX tid alloc fail for peer %pK (" QDF_MAC_ADDR_FMT ")",
			 peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		goto fail;
	}

	peer->valid = 1;
	dp_local_peer_id_alloc(pdev, peer);
	DP_STATS_INIT(peer);

	if (dp_peer_sawf_ctx_alloc(soc, peer) != QDF_STATUS_SUCCESS)
		dp_warn("peer sawf context alloc failed");

	dp_peer_update_state(soc, peer, DP_PEER_STATE_INIT);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
fail:
	qdf_mem_free(peer);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS dp_peer_legacy_setup(struct dp_soc *soc, struct dp_peer *peer)
{
	/* txrx_peer might exist already in peer reuse case */
	if (peer->txrx_peer)
		return QDF_STATUS_SUCCESS;

	if (dp_txrx_peer_attach(soc, peer) !=
				QDF_STATUS_SUCCESS) {
		dp_err("peer txrx ctx alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS dp_mld_peer_change_vdev(struct dp_soc *soc,
					  struct dp_peer *mld_peer,
					  uint8_t new_vdev_id)
{
	struct dp_vdev *prev_vdev;

	prev_vdev = mld_peer->vdev;
	/* release the ref to original dp_vdev */
	dp_vdev_unref_delete(soc, mld_peer->vdev,
			     DP_MOD_ID_CHILD);
	/*
	 * get the ref to new dp_vdev,
	 * increase dp_vdev ref_cnt
	 */
	mld_peer->vdev = dp_vdev_get_ref_by_id(soc, new_vdev_id,
					       DP_MOD_ID_CHILD);
	mld_peer->txrx_peer->vdev = mld_peer->vdev;

	dp_info("Change vdev for ML peer " QDF_MAC_ADDR_FMT
		" old vdev %pK id %d new vdev %pK id %d",
		QDF_MAC_ADDR_REF(mld_peer->mac_addr.raw),
		prev_vdev, prev_vdev->vdev_id, mld_peer->vdev, new_vdev_id);

	dp_cfg_event_record_mlo_setup_vdev_update_evt(
			soc, mld_peer, prev_vdev,
			mld_peer->vdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_peer_mlo_setup(
			struct dp_soc *soc,
			struct dp_peer *peer,
			uint8_t vdev_id,
			struct cdp_peer_setup_info *setup_info)
{
	struct dp_peer *mld_peer = NULL;
	struct cdp_txrx_peer_params_update params = {0};

	/* Non-MLO connection */
	if (!setup_info || !setup_info->mld_peer_mac) {
		/* To handle downgrade scenarios */
		if (peer->vdev->opmode == wlan_op_mode_sta) {
			struct cdp_txrx_peer_params_update params = {0};

			params.chip_id = dp_get_chip_id(soc);
			params.pdev_id = peer->vdev->pdev->pdev_id;
			params.vdev_id = peer->vdev->vdev_id;

			dp_wdi_event_handler(
					WDI_EVENT_STA_PRIMARY_UMAC_UPDATE,
					soc,
					(void *)&params, peer->peer_id,
					WDI_NO_VAL, params.pdev_id);
		}
		return QDF_STATUS_SUCCESS;
	}

	dp_cfg_event_record_peer_setup_evt(soc, DP_CFG_EVENT_MLO_SETUP,
					   peer, NULL, vdev_id, setup_info);

	/* if this is the first link peer */
	if (setup_info->is_first_link)
		/* create MLD peer */
		dp_peer_create_wifi3((struct cdp_soc_t *)soc,
				     vdev_id,
				     setup_info->mld_peer_mac,
				     CDP_MLD_PEER_TYPE);

	if (peer->vdev->opmode == wlan_op_mode_sta &&
	    setup_info->is_primary_link) {
		struct cdp_txrx_peer_params_update params = {0};

		params.chip_id = dp_get_chip_id(soc);
		params.pdev_id = peer->vdev->pdev->pdev_id;
		params.vdev_id = peer->vdev->vdev_id;

		dp_wdi_event_handler(
				WDI_EVENT_STA_PRIMARY_UMAC_UPDATE,
				soc,
				(void *)&params, peer->peer_id,
				WDI_NO_VAL, params.pdev_id);
	}

	peer->first_link = setup_info->is_first_link;
	peer->primary_link = setup_info->is_primary_link;
	mld_peer = dp_mld_peer_find_hash_find(soc,
					      setup_info->mld_peer_mac,
					      0, vdev_id, DP_MOD_ID_CDP);

	dp_info("Peer %pK MAC " QDF_MAC_ADDR_FMT " mld peer %pK MAC "
		QDF_MAC_ADDR_FMT " first_link %d, primary_link %d", peer,
		QDF_MAC_ADDR_REF(peer->mac_addr.raw), mld_peer,
		QDF_MAC_ADDR_REF(setup_info->mld_peer_mac),
		peer->first_link,
		peer->primary_link);

	if (mld_peer) {
		if (setup_info->is_first_link) {
			/* assign rx_tid to mld peer */
			mld_peer->rx_tid = peer->rx_tid;
			/* no cdp_peer_setup for MLD peer,
			 * set it for addba processing
			 */
			qdf_atomic_set(&mld_peer->is_default_route_set, 1);
		} else {
			/* free link peer original rx_tids mem */
			dp_peer_rx_tids_destroy(peer);
			/* assign mld peer rx_tid to link peer */
			peer->rx_tid = mld_peer->rx_tid;
		}

		if (setup_info->is_primary_link &&
		    !setup_info->is_first_link) {
			/*
			 * if first link is not the primary link,
			 * then need to change mld_peer->vdev as
			 * primary link dp_vdev is not same one
			 * during mld peer creation.
			 */
			dp_info("Primary link is not the first link. vdev: %pK "
				"vdev_id %d vdev_ref_cnt %d",
				mld_peer->vdev, vdev_id,
				qdf_atomic_read(&mld_peer->vdev->ref_cnt));

			dp_mld_peer_change_vdev(soc, mld_peer, vdev_id);

			params.vdev_id = peer->vdev->vdev_id;
			params.peer_mac = mld_peer->mac_addr.raw;
			params.chip_id = dp_get_chip_id(soc);
			params.pdev_id = peer->vdev->pdev->pdev_id;

			dp_wdi_event_handler(
					WDI_EVENT_PEER_PRIMARY_UMAC_UPDATE,
					soc, (void *)&params, peer->peer_id,
					WDI_NO_VAL, params.pdev_id);
		}

		/* associate mld and link peer */
		dp_link_peer_add_mld_peer(peer, mld_peer);
		dp_mld_peer_add_link_peer(mld_peer, peer, setup_info->is_bridge_peer);

		mld_peer->txrx_peer->is_mld_peer = 1;
		dp_peer_unref_delete(mld_peer, DP_MOD_ID_CDP);
	} else {
		peer->mld_peer = NULL;
		dp_err("mld peer" QDF_MAC_ADDR_FMT "not found!",
		       QDF_MAC_ADDR_REF(setup_info->mld_peer_mac));
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mlo_peer_authorize() - authorize MLO peer
 * @soc: soc handle
 * @peer: pointer to link peer
 *
 * Return: void
 */
static void dp_mlo_peer_authorize(struct dp_soc *soc,
				  struct dp_peer *peer)
{
	int i;
	struct dp_peer *link_peer = NULL;
	struct dp_peer *mld_peer = peer->mld_peer;
	struct dp_mld_link_peers link_peers_info;

	if (!mld_peer)
		return;

	/* get link peers with reference */
	dp_get_link_peers_ref_from_mld_peer(soc, mld_peer,
					    &link_peers_info,
					    DP_MOD_ID_CDP);

	for (i = 0; i < link_peers_info.num_links; i++) {
		link_peer = link_peers_info.link_peers[i];

		if (!link_peer->authorize) {
			dp_release_link_peers_ref(&link_peers_info,
						  DP_MOD_ID_CDP);
			mld_peer->authorize = false;
			return;
		}
	}

	/* if we are here all link peers are authorized,
	 * authorize ml_peer also
	 */
	mld_peer->authorize = true;

	/* release link peers reference */
	dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
}
#endif

/**
 * dp_peer_setup_wifi3_wrapper() - initialize the peer
 * @soc_hdl: soc handle object
 * @vdev_id : vdev_id of vdev object
 * @peer_mac: Peer's mac address
 * @setup_info: peer setup info for MLO
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_peer_setup_wifi3_wrapper(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			    uint8_t *peer_mac,
			    struct cdp_peer_setup_info *setup_info)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	return soc->arch_ops.txrx_peer_setup(soc_hdl, vdev_id,
					     peer_mac, setup_info);
}

/**
 * dp_cp_peer_del_resp_handler() - Handle the peer delete response
 * @soc_hdl: Datapath SOC handle
 * @vdev_id: id of virtual device object
 * @mac_addr: Mac address of the peer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_cp_peer_del_resp_handler(struct cdp_soc_t *soc_hdl,
					      uint8_t vdev_id,
					      uint8_t *mac_addr)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_ast_entry  *ast_entry = NULL;
	txrx_ast_free_cb cb = NULL;
	void *cookie;

	if (soc->ast_offload_support)
		return QDF_STATUS_E_INVAL;

	qdf_spin_lock_bh(&soc->ast_lock);

	ast_entry =
		dp_peer_ast_hash_find_by_vdevid(soc, mac_addr,
						vdev_id);

	/* in case of qwrap we have multiple BSS peers
	 * with same mac address
	 *
	 * AST entry for this mac address will be created
	 * only for one peer hence it will be NULL here
	 */
	if ((!ast_entry || !ast_entry->delete_in_progress) ||
	    (ast_entry->peer_id != HTT_INVALID_PEER)) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return QDF_STATUS_E_FAILURE;
	}

	if (ast_entry->is_mapped)
		soc->ast_table[ast_entry->ast_idx] = NULL;

	DP_STATS_INC(soc, ast.deleted, 1);
	dp_peer_ast_hash_remove(soc, ast_entry);

	cb = ast_entry->callback;
	cookie = ast_entry->cookie;
	ast_entry->callback = NULL;
	ast_entry->cookie = NULL;

	soc->num_ast_entries--;
	qdf_spin_unlock_bh(&soc->ast_lock);

	if (cb) {
		cb(soc->ctrl_psoc,
		   dp_soc_to_cdp_soc(soc),
		   cookie,
		   CDP_TXRX_AST_DELETED);
	}
	qdf_mem_free(ast_entry);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_MSCS
/**
 * dp_record_mscs_params() - Record MSCS parameters sent by the STA in
 * the MSCS Request to the AP.
 * @soc_hdl: Datapath soc handle
 * @peer_mac: STA Mac address
 * @vdev_id: ID of the vdev handle
 * @mscs_params: Structure having MSCS parameters obtained
 * from handshake
 * @active: Flag to set MSCS active/inactive
 *
 * The AP makes a note of these parameters while comparing the MSDUs
 * sent by the STA, to send the downlink traffic with correct User
 * priority.
 *
 * Return: QDF_STATUS - Success/Invalid
 */
static QDF_STATUS
dp_record_mscs_params(struct cdp_soc_t *soc_hdl, uint8_t *peer_mac,
		      uint8_t vdev_id, struct cdp_mscs_params *mscs_params,
		      bool active)
{
	struct dp_peer *peer;
	struct dp_peer *tgt_peer;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	peer = dp_peer_find_hash_find(soc, peer_mac, 0, vdev_id,
				      DP_MOD_ID_CDP);

	if (!peer) {
		dp_err("Peer is NULL!");
		goto fail;
	}

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	if (!tgt_peer)
		goto fail;

	if (!active) {
		dp_info("MSCS Procedure is terminated");
		tgt_peer->mscs_active = active;
		goto fail;
	}

	if (mscs_params->classifier_type == IEEE80211_TCLAS_MASK_CLA_TYPE_4) {
		/* Populate entries inside IPV4 database first */
		tgt_peer->mscs_ipv4_parameter.user_priority_bitmap =
			mscs_params->user_pri_bitmap;
		tgt_peer->mscs_ipv4_parameter.user_priority_limit =
			mscs_params->user_pri_limit;
		tgt_peer->mscs_ipv4_parameter.classifier_mask =
			mscs_params->classifier_mask;

		/* Populate entries inside IPV6 database */
		tgt_peer->mscs_ipv6_parameter.user_priority_bitmap =
			mscs_params->user_pri_bitmap;
		tgt_peer->mscs_ipv6_parameter.user_priority_limit =
			mscs_params->user_pri_limit;
		tgt_peer->mscs_ipv6_parameter.classifier_mask =
			mscs_params->classifier_mask;
		tgt_peer->mscs_active = 1;
		dp_info("\n\tMSCS Procedure request based parameters for "QDF_MAC_ADDR_FMT"\n"
			"\tClassifier_type = %d\tUser priority bitmap = %x\n"
			"\tUser priority limit = %x\tClassifier mask = %x",
			QDF_MAC_ADDR_REF(peer_mac),
			mscs_params->classifier_type,
			tgt_peer->mscs_ipv4_parameter.user_priority_bitmap,
			tgt_peer->mscs_ipv4_parameter.user_priority_limit,
			tgt_peer->mscs_ipv4_parameter.classifier_mask);
	}

	status = QDF_STATUS_SUCCESS;
fail:
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return status;
}
#endif

/**
 * dp_get_sec_type() - Get the security type
 * @soc: soc handle
 * @vdev_id: id of dp handle
 * @peer_mac: mac of datapath PEER handle
 * @sec_idx:    Security id (mcast, ucast)
 *
 * return sec_type: Security type
 */
static int dp_get_sec_type(struct cdp_soc_t *soc, uint8_t vdev_id,
			   uint8_t *peer_mac, uint8_t sec_idx)
{
	int sec_type = 0;
	struct dp_peer *peer =
			dp_peer_get_tgt_peer_hash_find((struct dp_soc *)soc,
						       peer_mac, 0, vdev_id,
						       DP_MOD_ID_CDP);

	if (!peer) {
		dp_cdp_err("%pK: Peer is NULL!", (struct dp_soc *)soc);
		return sec_type;
	}

	if (!peer->txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		dp_peer_debug("%pK: txrx peer is NULL!", soc);
		return sec_type;
	}
	sec_type = peer->txrx_peer->security[sec_idx].sec_type;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return sec_type;
}

/**
 * dp_peer_authorize() - authorize txrx peer
 * @soc_hdl: soc handle
 * @vdev_id: id of dp handle
 * @peer_mac: mac of datapath PEER handle
 * @authorize:
 *
 * Return: QDF_STATUS
 *
 */
static QDF_STATUS
dp_peer_authorize(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		  uint8_t *peer_mac, uint32_t authorize)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_peer *peer = dp_peer_get_tgt_peer_hash_find(soc, peer_mac,
							      0, vdev_id,
							      DP_MOD_ID_CDP);

	if (!peer) {
		dp_cdp_debug("%pK: Peer is NULL!", soc);
		status = QDF_STATUS_E_FAILURE;
	} else {
		peer->authorize = authorize ? 1 : 0;
		if (peer->txrx_peer)
			peer->txrx_peer->authorize = peer->authorize;

		if (!peer->authorize)
			dp_peer_flush_frags(soc_hdl, vdev_id, peer_mac);

		dp_mlo_peer_authorize(soc, peer);
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	}

	return status;
}

/**
 * dp_peer_get_authorize() - get peer authorize status
 * @soc_hdl: soc handle
 * @vdev_id: id of dp handle
 * @peer_mac: mac of datapath PEER handle
 *
 * Return: true is peer is authorized, false otherwise
 */
static bool
dp_peer_get_authorize(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		      uint8_t *peer_mac)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	bool authorize = false;
	struct dp_peer *peer = dp_peer_find_hash_find(soc, peer_mac,
						      0, vdev_id,
						      DP_MOD_ID_CDP);

	if (!peer) {
		dp_cdp_debug("%pK: Peer is NULL!", soc);
		return authorize;
	}

	authorize = peer->authorize;
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return authorize;
}

void dp_vdev_unref_delete(struct dp_soc *soc, struct dp_vdev *vdev,
			  enum dp_mod_id mod_id)
{
	ol_txrx_vdev_delete_cb vdev_delete_cb = NULL;
	void *vdev_delete_context = NULL;
	ol_txrx_vdev_delete_cb vdev_del_notify = NULL;
	void *vdev_del_noitfy_ctx = NULL;
	uint8_t vdev_id = vdev->vdev_id;
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_vdev *tmp_vdev = NULL;
	uint8_t found = 0;

	QDF_ASSERT(qdf_atomic_dec_return(&vdev->mod_refs[mod_id]) >= 0);

	/* Return if this is not the last reference*/
	if (!qdf_atomic_dec_and_test(&vdev->ref_cnt))
		return;

	/*
	 * This should be set as last reference need to released
	 * after cdp_vdev_detach() is called
	 *
	 * if this assert is hit there is a ref count issue
	 */
	QDF_ASSERT(vdev->delete.pending);

	vdev_delete_cb = vdev->delete.callback;
	vdev_delete_context = vdev->delete.context;

	vdev_del_notify = vdev->vdev_del_notify;
	vdev_del_noitfy_ctx = vdev->osif_vdev;

	dp_info("deleting vdev object %pK (" QDF_MAC_ADDR_FMT ")%s",
		vdev, QDF_MAC_ADDR_REF(vdev->mac_addr.raw),
		vdev_del_notify ? " with del_notify" : "");

	if (wlan_op_mode_monitor == vdev->opmode) {
		dp_monitor_vdev_delete(soc, vdev);
		goto free_vdev;
	}

	/* all peers are gone, go ahead and delete it */
	dp_tx_flow_pool_unmap_handler(pdev, vdev_id,
			FLOW_TYPE_VDEV, vdev_id);
	dp_tx_vdev_detach(vdev);
	dp_monitor_vdev_detach(vdev);

free_vdev:
	qdf_spinlock_destroy(&vdev->peer_list_lock);

	qdf_spin_lock_bh(&soc->inactive_vdev_list_lock);
	TAILQ_FOREACH(tmp_vdev, &soc->inactive_vdev_list,
		      inactive_list_elem) {
		if (tmp_vdev == vdev) {
			found = 1;
			break;
		}
	}
	if (found)
		TAILQ_REMOVE(&soc->inactive_vdev_list, vdev,
			     inactive_list_elem);
	/* delete this peer from the list */
	qdf_spin_unlock_bh(&soc->inactive_vdev_list_lock);

	dp_cfg_event_record_vdev_evt(soc, DP_CFG_EVENT_VDEV_UNREF_DEL,
				     vdev);
	wlan_minidump_remove(vdev, sizeof(*vdev), soc->ctrl_psoc,
			     WLAN_MD_DP_VDEV, "dp_vdev");
	qdf_mem_free(vdev);
	vdev = NULL;

	if (vdev_delete_cb)
		vdev_delete_cb(vdev_delete_context);

	if (vdev_del_notify)
		vdev_del_notify(vdev_del_noitfy_ctx);
}

qdf_export_symbol(dp_vdev_unref_delete);

void dp_peer_unref_delete(struct dp_peer *peer, enum dp_mod_id mod_id)
{
	struct dp_vdev *vdev = peer->vdev;
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_soc *soc = pdev->soc;
	uint16_t peer_id;
	struct dp_peer *tmp_peer;
	bool found = false;

	if (mod_id > DP_MOD_ID_RX)
		QDF_ASSERT(qdf_atomic_dec_return(&peer->mod_refs[mod_id]) >= 0);

	/*
	 * Hold the lock all the way from checking if the peer ref count
	 * is zero until the peer references are removed from the hash
	 * table and vdev list (if the peer ref count is zero).
	 * This protects against a new HL tx operation starting to use the
	 * peer object just after this function concludes it's done being used.
	 * Furthermore, the lock needs to be held while checking whether the
	 * vdev's list of peers is empty, to make sure that list is not modified
	 * concurrently with the empty check.
	 */
	if (qdf_atomic_dec_and_test(&peer->ref_cnt)) {
		peer_id = peer->peer_id;

		/*
		 * Make sure that the reference to the peer in
		 * peer object map is removed
		 */
		QDF_ASSERT(peer_id == HTT_INVALID_PEER);

		dp_peer_info("Deleting peer %pK ("QDF_MAC_ADDR_FMT")", peer,
			     QDF_MAC_ADDR_REF(peer->mac_addr.raw));

		dp_peer_sawf_ctx_free(soc, peer);

		wlan_minidump_remove(peer, sizeof(*peer), soc->ctrl_psoc,
				     WLAN_MD_DP_PEER, "dp_peer");

		qdf_spin_lock_bh(&soc->inactive_peer_list_lock);
		TAILQ_FOREACH(tmp_peer, &soc->inactive_peer_list,
			      inactive_list_elem) {
			if (tmp_peer == peer) {
				found = 1;
				break;
			}
		}
		if (found)
			TAILQ_REMOVE(&soc->inactive_peer_list, peer,
				     inactive_list_elem);
		/* delete this peer from the list */
		qdf_spin_unlock_bh(&soc->inactive_peer_list_lock);
		DP_AST_ASSERT(TAILQ_EMPTY(&peer->ast_entry_list));
		dp_peer_update_state(soc, peer, DP_PEER_STATE_FREED);

		/* cleanup the peer data */
		dp_peer_cleanup(vdev, peer);

		dp_monitor_peer_detach(soc, peer);

		qdf_spinlock_destroy(&peer->peer_state_lock);

		dp_txrx_peer_detach(soc, peer);
		dp_cfg_event_record_peer_evt(soc, DP_CFG_EVENT_PEER_UNREF_DEL,
					     peer, vdev, 0);
		qdf_mem_free(peer);

		/*
		 * Decrement ref count taken at peer create
		 */
		dp_peer_info("Deleted peer. Unref vdev %pK, vdev_ref_cnt %d",
			     vdev, qdf_atomic_read(&vdev->ref_cnt));
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CHILD);
	}
}

qdf_export_symbol(dp_peer_unref_delete);

void dp_txrx_peer_unref_delete(dp_txrx_ref_handle handle,
			       enum dp_mod_id mod_id)
{
	dp_peer_unref_delete((struct dp_peer *)handle, mod_id);
}

qdf_export_symbol(dp_txrx_peer_unref_delete);

/**
 * dp_peer_delete_wifi3() - Delete txrx peer
 * @soc_hdl: soc handle
 * @vdev_id: id of dp handle
 * @peer_mac: mac of datapath PEER handle
 * @bitmap: bitmap indicating special handling of request.
 * @peer_type: peer type (link or MLD)
 *
 */
static QDF_STATUS dp_peer_delete_wifi3(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *peer_mac, uint32_t bitmap,
				       enum cdp_peer_type peer_type)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer;
	struct cdp_peer_info peer_info = { 0 };
	struct dp_vdev *vdev = NULL;

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac,
				 false, peer_type);
	peer = dp_peer_hash_find_wrapper(soc, &peer_info, DP_MOD_ID_CDP);

	/* Peer can be null for monitor vap mac address */
	if (!peer) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid peer\n", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (!peer->valid) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		dp_err("Invalid peer: "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac));
		return QDF_STATUS_E_ALREADY;
	}

	vdev = peer->vdev;

	if (!vdev) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	peer->valid = 0;

	dp_cfg_event_record_peer_evt(soc, DP_CFG_EVENT_PEER_DELETE, peer,
				     vdev, 0);
	dp_init_info("%pK: peer %pK (" QDF_MAC_ADDR_FMT ") pending-refs %d",
		     soc, peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		     qdf_atomic_read(&peer->ref_cnt));

	dp_peer_rx_reo_shared_qaddr_delete(soc, peer);

	dp_local_peer_id_free(peer->vdev->pdev, peer);

	/* Drop all rx packets before deleting peer */
	dp_clear_peer_internal(soc, peer);

	qdf_spinlock_destroy(&peer->peer_info_lock);
	dp_peer_multipass_list_remove(peer);

	/* remove the reference to the peer from the hash table */
	dp_peer_find_hash_remove(soc, peer);

	dp_peer_vdev_list_remove(soc, vdev, peer);

	dp_peer_mlo_delete(peer);

	qdf_spin_lock_bh(&soc->inactive_peer_list_lock);
	TAILQ_INSERT_TAIL(&soc->inactive_peer_list, peer,
			  inactive_list_elem);
	qdf_spin_unlock_bh(&soc->inactive_peer_list_lock);

	/*
	 * Remove the reference added during peer_attach.
	 * The peer will still be left allocated until the
	 * PEER_UNMAP message arrives to remove the other
	 * reference, added by the PEER_MAP message.
	 */
	dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
	/*
	 * Remove the reference taken above
	 */
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

#ifdef DP_RX_UDP_OVER_PEER_ROAM
static QDF_STATUS dp_update_roaming_peer_wifi3(struct cdp_soc_t *soc_hdl,
					       uint8_t vdev_id,
					       uint8_t *peer_mac,
					       uint32_t auth_status)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	vdev->roaming_peer_status = auth_status;
	qdf_mem_copy(vdev->roaming_peer_mac.raw, peer_mac,
		     QDF_MAC_ADDR_SIZE);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}
#endif
/**
 * dp_get_vdev_mac_addr_wifi3() - Detach txrx peer
 * @soc_hdl: Datapath soc handle
 * @vdev_id: virtual interface id
 *
 * Return: MAC address on success, NULL on failure.
 *
 */
static uint8_t *dp_get_vdev_mac_addr_wifi3(struct cdp_soc_t *soc_hdl,
					   uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	uint8_t *mac = NULL;

	if (!vdev)
		return NULL;

	mac = vdev->mac_addr.raw;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return mac;
}

/**
 * dp_vdev_set_wds() - Enable per packet stats
 * @soc_hdl: DP soc handle
 * @vdev_id: id of DP VDEV handle
 * @val: value
 *
 * Return: none
 */
static int dp_vdev_set_wds(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   uint32_t val)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev =
		dp_vdev_get_ref_by_id((struct dp_soc *)soc, vdev_id,
				      DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	vdev->wds_enabled = val;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

static int dp_get_opmode(struct cdp_soc_t *soc_hdl, uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	int opmode;

	if (!vdev) {
		dp_err_rl("vdev for id %d is NULL", vdev_id);
		return -EINVAL;
	}
	opmode = vdev->opmode;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return opmode;
}

/**
 * dp_get_os_rx_handles_from_vdev_wifi3() - Get os rx handles for a vdev
 * @soc_hdl: ol_txrx_soc_handle handle
 * @vdev_id: vdev id for which os rx handles are needed
 * @stack_fn_p: pointer to stack function pointer
 * @osif_vdev_p: pointer to ol_osif_vdev_handle
 *
 * Return: void
 */
static
void dp_get_os_rx_handles_from_vdev_wifi3(struct cdp_soc_t *soc_hdl,
					  uint8_t vdev_id,
					  ol_txrx_rx_fp *stack_fn_p,
					  ol_osif_vdev_handle *osif_vdev_p)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (qdf_unlikely(!vdev)) {
		*stack_fn_p = NULL;
		*osif_vdev_p = NULL;
		return;
	}
	*stack_fn_p = vdev->osif_rx_stack;
	*osif_vdev_p = vdev->osif_vdev;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}

/**
 * dp_get_ctrl_pdev_from_vdev_wifi3() - Get control pdev of vdev
 * @soc_hdl: datapath soc handle
 * @vdev_id: virtual device/interface id
 *
 * Return: Handle to control pdev
 */
static struct cdp_cfg *dp_get_ctrl_pdev_from_vdev_wifi3(
						struct cdp_soc_t *soc_hdl,
						uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_pdev *pdev;

	if (!vdev)
		return NULL;

	pdev = vdev->pdev;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return pdev ? (struct cdp_cfg *)pdev->wlan_cfg_ctx : NULL;
}

int32_t dp_get_tx_pending(struct cdp_pdev *pdev_handle)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;

	return qdf_atomic_read(&pdev->num_tx_outstanding);
}

/**
 * dp_get_peer_mac_from_peer_id() - get peer mac
 * @soc: CDP SoC handle
 * @peer_id: Peer ID
 * @peer_mac: MAC addr of PEER
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_get_peer_mac_from_peer_id(struct cdp_soc_t *soc,
					       uint32_t peer_id,
					       uint8_t *peer_mac)
{
	struct dp_peer *peer;

	if (soc && peer_mac) {
		peer = dp_peer_get_ref_by_id((struct dp_soc *)soc,
					     (uint16_t)peer_id,
					     DP_MOD_ID_CDP);
		if (peer) {
			qdf_mem_copy(peer_mac, peer->mac_addr.raw,
				     QDF_MAC_ADDR_SIZE);
			dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

#ifdef MESH_MODE_SUPPORT
static
void dp_vdev_set_mesh_mode(struct cdp_vdev *vdev_hdl, uint32_t val)
{
	struct dp_vdev *vdev = (struct dp_vdev *)vdev_hdl;

	dp_cdp_info("%pK: val %d", vdev->pdev->soc, val);
	vdev->mesh_vdev = val;
	if (val)
		vdev->skip_sw_tid_classification |=
			DP_TX_MESH_ENABLED;
	else
		vdev->skip_sw_tid_classification &=
			~DP_TX_MESH_ENABLED;
}

/**
 * dp_vdev_set_mesh_rx_filter() - to set the mesh rx filter
 * @vdev_hdl: virtual device object
 * @val: value to be set
 *
 * Return: void
 */
static
void dp_vdev_set_mesh_rx_filter(struct cdp_vdev *vdev_hdl, uint32_t val)
{
	struct dp_vdev *vdev = (struct dp_vdev *)vdev_hdl;

	dp_cdp_info("%pK: val %d", vdev->pdev->soc, val);
	vdev->mesh_rx_filter = val;
}
#endif

/**
 * dp_vdev_set_hlos_tid_override() - to set hlos tid override
 * @vdev: virtual device object
 * @val: value to be set
 *
 * Return: void
 */
static
void dp_vdev_set_hlos_tid_override(struct dp_vdev *vdev, uint32_t val)
{
	dp_cdp_info("%pK: val %d", vdev->pdev->soc, val);
	if (val)
		vdev->skip_sw_tid_classification |=
			DP_TXRX_HLOS_TID_OVERRIDE_ENABLED;
	else
		vdev->skip_sw_tid_classification &=
			~DP_TXRX_HLOS_TID_OVERRIDE_ENABLED;
}

/**
 * dp_vdev_get_hlos_tid_override() - to get hlos tid override flag
 * @vdev_hdl: virtual device object
 *
 * Return: 1 if this flag is set
 */
static
uint8_t dp_vdev_get_hlos_tid_override(struct cdp_vdev *vdev_hdl)
{
	struct dp_vdev *vdev = (struct dp_vdev *)vdev_hdl;

	return !!(vdev->skip_sw_tid_classification &
			DP_TXRX_HLOS_TID_OVERRIDE_ENABLED);
}

#ifdef VDEV_PEER_PROTOCOL_COUNT
static void dp_enable_vdev_peer_protocol_count(struct cdp_soc_t *soc_hdl,
					       int8_t vdev_id,
					       bool enable)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return;

	dp_info("enable %d vdev_id %d", enable, vdev_id);
	vdev->peer_protocol_count_track = enable;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}

static void dp_enable_vdev_peer_protocol_drop_mask(struct cdp_soc_t *soc_hdl,
						   int8_t vdev_id,
						   int drop_mask)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return;

	dp_info("drop_mask %d vdev_id %d", drop_mask, vdev_id);
	vdev->peer_protocol_count_dropmask = drop_mask;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}

static int dp_is_vdev_peer_protocol_count_enabled(struct cdp_soc_t *soc_hdl,
						  int8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;
	int peer_protocol_count_track;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return 0;

	dp_info("enable %d vdev_id %d", vdev->peer_protocol_count_track,
		vdev_id);
	peer_protocol_count_track =
		vdev->peer_protocol_count_track;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return peer_protocol_count_track;
}

static int dp_get_vdev_peer_protocol_drop_mask(struct cdp_soc_t *soc_hdl,
					       int8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;
	int peer_protocol_count_dropmask;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return 0;

	dp_info("drop_mask %d vdev_id %d", vdev->peer_protocol_count_dropmask,
		vdev_id);
	peer_protocol_count_dropmask =
		vdev->peer_protocol_count_dropmask;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return peer_protocol_count_dropmask;
}

#endif

bool dp_check_pdev_exists(struct dp_soc *soc, struct dp_pdev *data)
{
	uint8_t pdev_count;

	for (pdev_count = 0; pdev_count < MAX_PDEV_CNT; pdev_count++) {
		if (soc->pdev_list[pdev_count] &&
		    soc->pdev_list[pdev_count] == data)
			return true;
	}
	return false;
}

void dp_aggregate_vdev_stats(struct dp_vdev *vdev,
			     struct cdp_vdev_stats *vdev_stats,
			     enum dp_pkt_xmit_type xmit_type)
{
	if (!vdev || !vdev->pdev)
		return;

	dp_update_vdev_ingress_stats(vdev);

	dp_copy_vdev_stats_to_tgt_buf(vdev_stats,
					    &vdev->stats, xmit_type);
	dp_vdev_iterate_peer(vdev, dp_update_vdev_stats, vdev_stats,
			     DP_MOD_ID_GENERIC_STATS);

	dp_update_vdev_rate_stats(vdev_stats, &vdev->stats);

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
	dp_wdi_event_handler(WDI_EVENT_UPDATE_DP_STATS, vdev->pdev->soc,
			     vdev_stats, vdev->vdev_id,
			     UPDATE_VDEV_STATS, vdev->pdev->pdev_id);
#endif
}

void dp_aggregate_pdev_stats(struct dp_pdev *pdev)
{
	struct dp_vdev *vdev = NULL;
	struct dp_soc *soc;
	struct cdp_vdev_stats *vdev_stats =
			qdf_mem_malloc_atomic(sizeof(struct cdp_vdev_stats));

	if (!vdev_stats) {
		dp_cdp_err("%pK: DP alloc failure - unable to get alloc vdev stats",
			   pdev->soc);
		return;
	}

	soc = pdev->soc;

	qdf_mem_zero(&pdev->stats.tx, sizeof(pdev->stats.tx));
	qdf_mem_zero(&pdev->stats.rx, sizeof(pdev->stats.rx));
	qdf_mem_zero(&pdev->stats.tx_i, sizeof(pdev->stats.tx_i));
	qdf_mem_zero(&pdev->stats.rx_i, sizeof(pdev->stats.rx_i));

	if (dp_monitor_is_enable_mcopy_mode(pdev))
		dp_monitor_invalid_peer_update_pdev_stats(soc, pdev);

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {

		dp_aggregate_vdev_stats(vdev, vdev_stats, DP_XMIT_TOTAL);
		dp_update_pdev_stats(pdev, vdev_stats);
		dp_update_pdev_ingress_stats(pdev, vdev);
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);
	qdf_mem_free(vdev_stats);

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
	dp_wdi_event_handler(WDI_EVENT_UPDATE_DP_STATS, pdev->soc, &pdev->stats,
			     pdev->pdev_id, UPDATE_PDEV_STATS, pdev->pdev_id);
#endif
}

/**
 * dp_vdev_getstats() - get vdev packet level stats
 * @vdev_handle: Datapath VDEV handle
 * @stats: cdp network device stats structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_vdev_getstats(struct cdp_vdev *vdev_handle,
				   struct cdp_dev_stats *stats)
{
	struct dp_vdev *vdev = (struct dp_vdev *)vdev_handle;
	struct dp_pdev *pdev;
	struct dp_soc *soc;
	struct cdp_vdev_stats *vdev_stats;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	pdev = vdev->pdev;
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	soc = pdev->soc;

	vdev_stats = qdf_mem_malloc_atomic(sizeof(struct cdp_vdev_stats));

	if (!vdev_stats) {
		dp_err("%pK: DP alloc failure - unable to get alloc vdev stats",
		       soc);
		return QDF_STATUS_E_FAILURE;
	}

	dp_aggregate_vdev_stats(vdev, vdev_stats, DP_XMIT_LINK);

	stats->tx_packets = vdev_stats->tx.comp_pkt.num;
	stats->tx_bytes = vdev_stats->tx.comp_pkt.bytes;

	stats->tx_errors = vdev_stats->tx.tx_failed;
	stats->tx_dropped = vdev_stats->tx_i.dropped.dropped_pkt.num +
			    vdev_stats->tx_i.sg.dropped_host.num +
			    vdev_stats->tx_i.mcast_en.dropped_map_error +
			    vdev_stats->tx_i.mcast_en.dropped_self_mac +
			    vdev_stats->tx_i.mcast_en.dropped_send_fail +
			    vdev_stats->tx.nawds_mcast_drop;

	if (!wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx)) {
		stats->rx_packets = vdev_stats->rx.to_stack.num;
		stats->rx_bytes = vdev_stats->rx.to_stack.bytes;
	} else {
		stats->rx_packets = vdev_stats->rx_i.reo_rcvd_pkt.num +
				    vdev_stats->rx_i.null_q_desc_pkt.num +
				    vdev_stats->rx_i.routed_eapol_pkt.num;
		stats->rx_bytes = vdev_stats->rx_i.reo_rcvd_pkt.bytes +
				  vdev_stats->rx_i.null_q_desc_pkt.bytes +
				  vdev_stats->rx_i.routed_eapol_pkt.bytes;
	}

	stats->rx_errors = vdev_stats->rx.err.mic_err +
			   vdev_stats->rx.err.decrypt_err +
			   vdev_stats->rx.err.fcserr +
			   vdev_stats->rx.err.pn_err +
			   vdev_stats->rx.err.oor_err +
			   vdev_stats->rx.err.jump_2k_err +
			   vdev_stats->rx.err.rxdma_wifi_parse_err;

	stats->rx_dropped = vdev_stats->rx.mec_drop.num +
			    vdev_stats->rx.multipass_rx_pkt_drop +
			    vdev_stats->rx.peer_unauth_rx_pkt_drop +
			    vdev_stats->rx.policy_check_drop +
			    vdev_stats->rx.nawds_mcast_drop +
			    vdev_stats->rx.mcast_3addr_drop +
			    vdev_stats->rx.ppeds_drop.num;

	qdf_mem_free(vdev_stats);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_pdev_getstats() - get pdev packet level stats
 * @pdev_handle: Datapath PDEV handle
 * @stats: cdp network device stats structure
 *
 * Return: QDF_STATUS
 */
static void dp_pdev_getstats(struct cdp_pdev *pdev_handle,
			     struct cdp_dev_stats *stats)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;

	dp_aggregate_pdev_stats(pdev);

	stats->tx_packets = pdev->stats.tx.comp_pkt.num;
	stats->tx_bytes = pdev->stats.tx.comp_pkt.bytes;

	stats->tx_errors = pdev->stats.tx.tx_failed;
	stats->tx_dropped = pdev->stats.tx_i.dropped.dropped_pkt.num +
			    pdev->stats.tx_i.sg.dropped_host.num +
			    pdev->stats.tx_i.mcast_en.dropped_map_error +
			    pdev->stats.tx_i.mcast_en.dropped_self_mac +
			    pdev->stats.tx_i.mcast_en.dropped_send_fail +
			    pdev->stats.tx.nawds_mcast_drop +
			    pdev->stats.tso_stats.dropped_host.num;

	if (!wlan_cfg_get_vdev_stats_hw_offload_config(pdev->soc->wlan_cfg_ctx)) {
		stats->rx_packets = pdev->stats.rx.to_stack.num;
		stats->rx_bytes = pdev->stats.rx.to_stack.bytes;
	} else {
		stats->rx_packets = pdev->stats.rx_i.reo_rcvd_pkt.num +
				    pdev->stats.rx_i.null_q_desc_pkt.num +
				    pdev->stats.rx_i.routed_eapol_pkt.num;
		stats->rx_bytes = pdev->stats.rx_i.reo_rcvd_pkt.bytes +
				  pdev->stats.rx_i.null_q_desc_pkt.bytes +
				  pdev->stats.rx_i.routed_eapol_pkt.bytes;
	}

	stats->rx_errors = pdev->stats.err.ip_csum_err +
		pdev->stats.err.tcp_udp_csum_err +
		pdev->stats.rx.err.mic_err +
		pdev->stats.rx.err.decrypt_err +
		pdev->stats.rx.err.fcserr +
		pdev->stats.rx.err.pn_err +
		pdev->stats.rx.err.oor_err +
		pdev->stats.rx.err.jump_2k_err +
		pdev->stats.rx.err.rxdma_wifi_parse_err;
	stats->rx_dropped = pdev->stats.dropped.msdu_not_done +
		pdev->stats.dropped.mec +
		pdev->stats.dropped.mesh_filter +
		pdev->stats.dropped.wifi_parse +
		pdev->stats.dropped.mon_rx_drop +
		pdev->stats.dropped.mon_radiotap_update_err +
		pdev->stats.rx.mec_drop.num +
		pdev->stats.rx.ppeds_drop.num +
		pdev->stats.rx.multipass_rx_pkt_drop +
		pdev->stats.rx.peer_unauth_rx_pkt_drop +
		pdev->stats.rx.policy_check_drop +
		pdev->stats.rx.nawds_mcast_drop +
		pdev->stats.rx.mcast_3addr_drop;
}

/**
 * dp_get_device_stats() - get interface level packet stats
 * @soc_hdl: soc handle
 * @id: vdev_id or pdev_id based on type
 * @stats: cdp network device stats structure
 * @type: device type pdev/vdev
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_get_device_stats(struct cdp_soc_t *soc_hdl, uint8_t id,
				      struct cdp_dev_stats *stats,
				      uint8_t type)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct dp_vdev *vdev;

	switch (type) {
	case UPDATE_VDEV_STATS:
		vdev = dp_vdev_get_ref_by_id(soc, id, DP_MOD_ID_CDP);

		if (vdev) {
			status = dp_vdev_getstats((struct cdp_vdev *)vdev,
						  stats);
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		}
		return status;
	case UPDATE_PDEV_STATS:
		{
			struct dp_pdev *pdev =
				dp_get_pdev_from_soc_pdev_id_wifi3(
						(struct dp_soc *)soc,
						 id);
			if (pdev) {
				dp_pdev_getstats((struct cdp_pdev *)pdev,
						 stats);
				return QDF_STATUS_SUCCESS;
			}
		}
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"apstats cannot be updated for this input "
			"type %d", type);
		break;
	}

	return QDF_STATUS_E_FAILURE;
}

const
char *dp_srng_get_str_from_hal_ring_type(enum hal_ring_type ring_type)
{
	switch (ring_type) {
	case REO_DST:
		return "Reo_dst";
	case REO_EXCEPTION:
		return "Reo_exception";
	case REO_CMD:
		return "Reo_cmd";
	case REO_REINJECT:
		return "Reo_reinject";
	case REO_STATUS:
		return "Reo_status";
	case WBM2SW_RELEASE:
		return "wbm2sw_release";
	case TCL_DATA:
		return "tcl_data";
	case TCL_CMD_CREDIT:
		return "tcl_cmd_credit";
	case TCL_STATUS:
		return "tcl_status";
	case SW2WBM_RELEASE:
		return "sw2wbm_release";
	case RXDMA_BUF:
		return "Rxdma_buf";
	case RXDMA_DST:
		return "Rxdma_dst";
	case RXDMA_MONITOR_BUF:
		return "Rxdma_monitor_buf";
	case RXDMA_MONITOR_DESC:
		return "Rxdma_monitor_desc";
	case RXDMA_MONITOR_STATUS:
		return "Rxdma_monitor_status";
	case RXDMA_MONITOR_DST:
		return "Rxdma_monitor_destination";
	case WBM_IDLE_LINK:
		return "WBM_hw_idle_link";
	case PPE2TCL:
		return "PPE2TCL";
	case REO2PPE:
		return "REO2PPE";
	case TX_MONITOR_DST:
		return "tx_monitor_destination";
	case TX_MONITOR_BUF:
		return "tx_monitor_buf";
	default:
		dp_err("Invalid ring type: %u", ring_type);
		break;
	}
	return "Invalid";
}

void dp_print_napi_stats(struct dp_soc *soc)
{
	hif_print_napi_stats(soc->hif_handle);
}

/**
 * dp_txrx_host_peer_stats_clr() - Reinitialize the txrx peer stats
 * @soc: Datapath soc
 * @peer: Datatpath peer
 * @arg: argument to iter function
 *
 * Return: QDF_STATUS
 */
static inline void
dp_txrx_host_peer_stats_clr(struct dp_soc *soc,
			    struct dp_peer *peer,
			    void *arg)
{
	struct dp_txrx_peer *txrx_peer = NULL;
	struct dp_peer *tgt_peer = NULL;
	struct cdp_interface_peer_stats peer_stats_intf = {0};

	peer_stats_intf.rx_avg_snr = CDP_INVALID_SNR;

	DP_STATS_CLR(peer);
	/* Clear monitor peer stats */
	dp_monitor_peer_reset_stats(soc, peer);

	/* Clear MLD peer stats only when link peer is primary */
	if (dp_peer_is_primary_link_peer(peer)) {
		tgt_peer = dp_get_tgt_peer_from_peer(peer);
		if (tgt_peer) {
			DP_STATS_CLR(tgt_peer);
			txrx_peer = tgt_peer->txrx_peer;
			dp_txrx_peer_stats_clr(txrx_peer);
		}
	}

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
	dp_wdi_event_handler(WDI_EVENT_UPDATE_DP_STATS, peer->vdev->pdev->soc,
			     &peer_stats_intf,  peer->peer_id,
			     UPDATE_PEER_STATS, peer->vdev->pdev->pdev_id);
#endif
}

#ifdef WLAN_DP_SRNG_USAGE_WM_TRACKING
static inline void dp_srng_clear_ring_usage_wm_stats(struct dp_soc *soc)
{
	int ring;

	for (ring = 0; ring < soc->num_reo_dest_rings; ring++)
		hal_srng_clear_ring_usage_wm_locked(soc->hal_soc,
					    soc->reo_dest_ring[ring].hal_srng);

	for (ring = 0; ring < soc->num_tcl_data_rings; ring++) {
		if (wlan_cfg_get_wbm_ring_num_for_index(
					soc->wlan_cfg_ctx, ring) ==
		    INVALID_WBM_RING_NUM)
			continue;

		hal_srng_clear_ring_usage_wm_locked(soc->hal_soc,
					soc->tx_comp_ring[ring].hal_srng);
	}
}
#else
static inline void dp_srng_clear_ring_usage_wm_stats(struct dp_soc *soc)
{
}
#endif

#ifdef WLAN_SUPPORT_PPEDS
static void dp_clear_tx_ppeds_stats(struct dp_soc *soc)
{
	if (soc->arch_ops.dp_ppeds_clear_stats)
		soc->arch_ops.dp_ppeds_clear_stats(soc);
}

static void dp_ppeds_clear_ring_util_stats(struct dp_soc *soc)
{
	if (soc->arch_ops.dp_txrx_ppeds_clear_rings_stats)
		soc->arch_ops.dp_txrx_ppeds_clear_rings_stats(soc);
}
#else
static void dp_clear_tx_ppeds_stats(struct dp_soc *soc)
{
}

static void dp_ppeds_clear_ring_util_stats(struct dp_soc *soc)
{
}
#endif

/**
 * dp_txrx_host_stats_clr() - Reinitialize the txrx stats
 * @vdev: DP_VDEV handle
 * @soc: DP_SOC handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_txrx_host_stats_clr(struct dp_vdev *vdev, struct dp_soc *soc)
{
	struct dp_vdev *var_vdev = NULL;

	if (!vdev || !vdev->pdev)
		return QDF_STATUS_E_FAILURE;

	/*
	 * if NSS offload is enabled, then send message
	 * to NSS FW to clear the stats. Once NSS FW clears the statistics
	 * then clear host statistics.
	 */
	if (wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx)) {
		if (soc->cdp_soc.ol_ops->nss_stats_clr)
			soc->cdp_soc.ol_ops->nss_stats_clr(soc->ctrl_psoc,
							   vdev->vdev_id);
	}

	dp_vdev_stats_hw_offload_target_clear(soc, vdev->pdev->pdev_id,
					      (1 << vdev->vdev_id));

	DP_STATS_CLR(vdev->pdev);
	DP_STATS_CLR(vdev->pdev->soc);

	dp_clear_tx_ppeds_stats(soc);
	dp_ppeds_clear_ring_util_stats(soc);

	hif_clear_napi_stats(vdev->pdev->soc->hif_handle);

	TAILQ_FOREACH(var_vdev, &vdev->pdev->vdev_list, vdev_list_elem) {
		DP_STATS_CLR(var_vdev);
		dp_vdev_iterate_peer(var_vdev, dp_txrx_host_peer_stats_clr,
				     NULL, DP_MOD_ID_GENERIC_STATS);
	}

	dp_srng_clear_ring_usage_wm_stats(soc);

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
	dp_wdi_event_handler(WDI_EVENT_UPDATE_DP_STATS, vdev->pdev->soc,
			     &vdev->stats,  vdev->vdev_id,
			     UPDATE_VDEV_STATS, vdev->pdev->pdev_id);
#endif
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_get_peer_calibr_stats()- Get peer calibrated stats
 * @peer: Datapath peer
 * @peer_stats: buffer for peer stats
 *
 * Return: none
 */
static inline
void dp_get_peer_calibr_stats(struct dp_peer *peer,
			      struct cdp_peer_stats *peer_stats)
{
	struct dp_peer *tgt_peer;

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	if (!tgt_peer)
		return;

	peer_stats->tx.last_per = tgt_peer->stats.tx.last_per;
	peer_stats->tx.tx_bytes_success_last =
				tgt_peer->stats.tx.tx_bytes_success_last;
	peer_stats->tx.tx_data_success_last =
					tgt_peer->stats.tx.tx_data_success_last;
	peer_stats->tx.tx_byte_rate = tgt_peer->stats.tx.tx_byte_rate;
	peer_stats->tx.tx_data_rate = tgt_peer->stats.tx.tx_data_rate;
	peer_stats->tx.tx_data_ucast_last =
					tgt_peer->stats.tx.tx_data_ucast_last;
	peer_stats->tx.tx_data_ucast_rate =
					tgt_peer->stats.tx.tx_data_ucast_rate;
	peer_stats->tx.inactive_time = tgt_peer->stats.tx.inactive_time;
	peer_stats->rx.rx_bytes_success_last =
				tgt_peer->stats.rx.rx_bytes_success_last;
	peer_stats->rx.rx_data_success_last =
				tgt_peer->stats.rx.rx_data_success_last;
	peer_stats->rx.rx_byte_rate = tgt_peer->stats.rx.rx_byte_rate;
	peer_stats->rx.rx_data_rate = tgt_peer->stats.rx.rx_data_rate;
}

/**
 * dp_get_peer_basic_stats()- Get peer basic stats
 * @peer: Datapath peer
 * @peer_stats: buffer for peer stats
 *
 * Return: none
 */
static inline
void dp_get_peer_basic_stats(struct dp_peer *peer,
			     struct cdp_peer_stats *peer_stats)
{
	struct dp_txrx_peer *txrx_peer;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer)
		return;

	peer_stats->tx.comp_pkt.num += txrx_peer->comp_pkt.num;
	peer_stats->tx.comp_pkt.bytes += txrx_peer->comp_pkt.bytes;
	peer_stats->tx.tx_failed += txrx_peer->tx_failed;
	peer_stats->rx.to_stack.num += txrx_peer->to_stack.num;
	peer_stats->rx.to_stack.bytes += txrx_peer->to_stack.bytes;
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_get_peer_per_pkt_stats()- Get peer per pkt stats
 * @peer: Datapath peer
 * @peer_stats: buffer for peer stats
 *
 * Return: none
 */
static inline
void dp_get_peer_per_pkt_stats(struct dp_peer *peer,
			       struct cdp_peer_stats *peer_stats)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_per_pkt_stats *per_pkt_stats;
	uint8_t inx = 0, link_id = 0;
	struct dp_pdev *pdev;
	struct dp_soc *soc;
	uint8_t stats_arr_size;

	txrx_peer = dp_get_txrx_peer(peer);
	pdev = peer->vdev->pdev;

	if (!txrx_peer)
		return;

	if (!IS_MLO_DP_LINK_PEER(peer)) {
		stats_arr_size = txrx_peer->stats_arr_size;
		for (inx = 0; inx < stats_arr_size; inx++) {
			per_pkt_stats = &txrx_peer->stats[inx].per_pkt_stats;
			DP_UPDATE_PER_PKT_STATS(peer_stats, per_pkt_stats);
		}
	} else {
		soc = pdev->soc;
		link_id = dp_get_peer_hw_link_id(soc, pdev);
		per_pkt_stats =
			&txrx_peer->stats[link_id].per_pkt_stats;
		DP_UPDATE_PER_PKT_STATS(peer_stats, per_pkt_stats);
	}
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_get_peer_extd_stats()- Get peer extd stats
 * @peer: Datapath peer
 * @peer_stats: buffer for peer stats
 *
 * Return: none
 */
static inline
void dp_get_peer_extd_stats(struct dp_peer *peer,
			    struct cdp_peer_stats *peer_stats)
{
	struct dp_soc *soc = peer->vdev->pdev->soc;

	if (IS_MLO_DP_MLD_PEER(peer)) {
		uint8_t i;
		struct dp_peer *link_peer;
		struct dp_soc *link_peer_soc;
		struct dp_mld_link_peers link_peers_info;

		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_CDP);
		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			link_peer_soc = link_peer->vdev->pdev->soc;
			dp_monitor_peer_get_stats(link_peer_soc, link_peer,
						  peer_stats,
						  UPDATE_PEER_STATS);
		}
		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
	} else {
		dp_monitor_peer_get_stats(soc, peer, peer_stats,
					  UPDATE_PEER_STATS);
	}
}
#else
static inline
void dp_get_peer_extd_stats(struct dp_peer *peer,
			    struct cdp_peer_stats *peer_stats)
{
	struct dp_soc *soc = peer->vdev->pdev->soc;

	dp_monitor_peer_get_stats(soc, peer, peer_stats, UPDATE_PEER_STATS);
}
#endif
#else
#if defined WLAN_FEATURE_11BE_MLO && defined DP_MLO_LINK_STATS_SUPPORT
static inline
void dp_get_peer_per_pkt_stats(struct dp_peer *peer,
			       struct cdp_peer_stats *peer_stats)
{
	uint8_t i, index;
	struct dp_mld_link_peers link_peers_info;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_per_pkt_stats *per_pkt_stats;
	struct dp_soc *soc = peer->vdev->pdev->soc;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer)
		return;

	if (IS_MLO_DP_MLD_PEER(peer)) {
		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_GENERIC_STATS);
		for (i = 0; i < link_peers_info.num_links; i++) {
			if (i > txrx_peer->stats_arr_size)
				break;
			per_pkt_stats = &txrx_peer->stats[i].per_pkt_stats;
			DP_UPDATE_PER_PKT_STATS(peer_stats, per_pkt_stats);
		}
		dp_release_link_peers_ref(&link_peers_info,
					  DP_MOD_ID_GENERIC_STATS);
	} else {
		index = dp_get_peer_link_id(peer);
		per_pkt_stats = &txrx_peer->stats[index].per_pkt_stats;
		DP_UPDATE_PER_PKT_STATS(peer_stats, per_pkt_stats);
		qdf_mem_copy(&peer_stats->mac_addr,
			     &peer->mac_addr.raw[0],
			     QDF_MAC_ADDR_SIZE);
	}
}

static inline
void dp_get_peer_extd_stats(struct dp_peer *peer,
			    struct cdp_peer_stats *peer_stats)
{
	uint8_t i, index;
	struct dp_mld_link_peers link_peers_info;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_extd_stats *extd_stats;
	struct dp_soc *soc = peer->vdev->pdev->soc;

	txrx_peer = dp_get_txrx_peer(peer);
	if (qdf_unlikely(!txrx_peer)) {
		dp_err_rl("txrx_peer NULL for peer MAC: " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		return;
	}

	if (IS_MLO_DP_MLD_PEER(peer)) {
		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_GENERIC_STATS);
		for (i = 0; i < link_peers_info.num_links; i++) {
			if (i > txrx_peer->stats_arr_size)
				break;
			extd_stats = &txrx_peer->stats[i].extd_stats;
			/* Return aggregated stats for MLD peer */
			DP_UPDATE_EXTD_STATS(peer_stats, extd_stats);
		}
		dp_release_link_peers_ref(&link_peers_info,
					  DP_MOD_ID_GENERIC_STATS);
	} else {
		index = dp_get_peer_link_id(peer);
		extd_stats = &txrx_peer->stats[index].extd_stats;
		DP_UPDATE_EXTD_STATS(peer_stats, extd_stats);
		qdf_mem_copy(&peer_stats->mac_addr,
			     &peer->mac_addr.raw[0],
			     QDF_MAC_ADDR_SIZE);
	}
}
#else
static inline
void dp_get_peer_per_pkt_stats(struct dp_peer *peer,
			       struct cdp_peer_stats *peer_stats)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_per_pkt_stats *per_pkt_stats;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer)
		return;

	per_pkt_stats = &txrx_peer->stats[0].per_pkt_stats;
	DP_UPDATE_PER_PKT_STATS(peer_stats, per_pkt_stats);
}

static inline
void dp_get_peer_extd_stats(struct dp_peer *peer,
			    struct cdp_peer_stats *peer_stats)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_extd_stats *extd_stats;

	txrx_peer = dp_get_txrx_peer(peer);
	if (qdf_unlikely(!txrx_peer)) {
		dp_err_rl("txrx_peer NULL");
		return;
	}

	extd_stats = &txrx_peer->stats[0].extd_stats;
	DP_UPDATE_EXTD_STATS(peer_stats, extd_stats);
}
#endif
#endif

/**
 * dp_get_peer_tx_per()- Get peer packet error ratio
 * @peer_stats: buffer for peer stats
 *
 * Return: none
 */
static inline
void dp_get_peer_tx_per(struct cdp_peer_stats *peer_stats)
{
	if (peer_stats->tx.tx_success.num + peer_stats->tx.retries > 0)
		peer_stats->tx.per = qdf_do_div((peer_stats->tx.retries * 100),
				  (peer_stats->tx.tx_success.num +
				   peer_stats->tx.retries));
	else
		peer_stats->tx.per = 0;
}

void dp_get_peer_stats(struct dp_peer *peer, struct cdp_peer_stats *peer_stats)
{
	dp_get_peer_calibr_stats(peer, peer_stats);

	dp_get_peer_basic_stats(peer, peer_stats);

	dp_get_peer_per_pkt_stats(peer, peer_stats);

	dp_get_peer_extd_stats(peer, peer_stats);

	dp_get_peer_tx_per(peer_stats);
}

/**
 * dp_get_host_peer_stats()- function to print peer stats
 * @soc: dp_soc handle
 * @mac_addr: mac address of the peer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_get_host_peer_stats(struct cdp_soc_t *soc, uint8_t *mac_addr)
{
	struct dp_peer *peer = NULL;
	struct cdp_peer_stats *peer_stats = NULL;
	struct cdp_peer_info peer_info = { 0 };

	if (!mac_addr) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: NULL peer mac addr\n", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	DP_PEER_INFO_PARAMS_INIT(&peer_info, DP_VDEV_ALL, mac_addr, false,
				 CDP_WILD_PEER_TYPE);

	peer = dp_peer_hash_find_wrapper((struct dp_soc *)soc, &peer_info,
					 DP_MOD_ID_CDP);
	if (!peer) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid peer\n", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	peer_stats = qdf_mem_malloc(sizeof(struct cdp_peer_stats));
	if (!peer_stats) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Memory allocation failed for cdp_peer_stats\n",
			  __func__);
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_zero(peer_stats, sizeof(struct cdp_peer_stats));

	dp_get_peer_stats(peer, peer_stats);
	dp_print_peer_stats(peer, peer_stats);

	dp_peer_rxtid_stats(dp_get_tgt_peer_from_peer(peer),
			    dp_rx_tid_stats_cb, NULL);

	qdf_mem_free(peer_stats);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_txrx_stats_help() - Helper function for Txrx_Stats
 *
 * Return: None
 */
static void dp_txrx_stats_help(void)
{
	dp_info("Command: iwpriv wlan0 txrx_stats <stats_option> <mac_id>");
	dp_info("stats_option:");
	dp_info("  1 -- HTT Tx Statistics");
	dp_info("  2 -- HTT Rx Statistics");
	dp_info("  3 -- HTT Tx HW Queue Statistics");
	dp_info("  4 -- HTT Tx HW Sched Statistics");
	dp_info("  5 -- HTT Error Statistics");
	dp_info("  6 -- HTT TQM Statistics");
	dp_info("  7 -- HTT TQM CMDQ Statistics");
	dp_info("  8 -- HTT TX_DE_CMN Statistics");
	dp_info("  9 -- HTT Tx Rate Statistics");
	dp_info(" 10 -- HTT Rx Rate Statistics");
	dp_info(" 11 -- HTT Peer Statistics");
	dp_info(" 12 -- HTT Tx SelfGen Statistics");
	dp_info(" 13 -- HTT Tx MU HWQ Statistics");
	dp_info(" 14 -- HTT RING_IF_INFO Statistics");
	dp_info(" 15 -- HTT SRNG Statistics");
	dp_info(" 16 -- HTT SFM Info Statistics");
	dp_info(" 17 -- HTT PDEV_TX_MU_MIMO_SCHED INFO Statistics");
	dp_info(" 18 -- HTT Peer List Details");
	dp_info(" 20 -- Clear Host Statistics");
	dp_info(" 21 -- Host Rx Rate Statistics");
	dp_info(" 22 -- Host Tx Rate Statistics");
	dp_info(" 23 -- Host Tx Statistics");
	dp_info(" 24 -- Host Rx Statistics");
	dp_info(" 25 -- Host AST Statistics");
	dp_info(" 26 -- Host SRNG PTR Statistics");
	dp_info(" 27 -- Host Mon Statistics");
	dp_info(" 28 -- Host REO Queue Statistics");
	dp_info(" 29 -- Host Soc cfg param Statistics");
	dp_info(" 30 -- Host pdev cfg param Statistics");
	dp_info(" 31 -- Host NAPI stats");
	dp_info(" 32 -- Host Interrupt stats");
	dp_info(" 33 -- Host FISA stats");
	dp_info(" 34 -- Host Register Work stats");
	dp_info(" 35 -- HW REO Queue stats");
	dp_info(" 36 -- Host WBM IDLE link desc ring HP/TP");
	dp_info(" 37 -- Host SRNG usage watermark stats");
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_umac_rst_skel_enable_update() - Update skel dbg flag for umac reset
 * @soc: dp soc handle
 * @en: ebable/disable
 *
 * Return: void
 */
static void dp_umac_rst_skel_enable_update(struct dp_soc *soc, bool en)
{
	soc->umac_reset_ctx.skel_enable = en;
	dp_cdp_debug("UMAC HW reset debug skeleton code enabled :%u",
		     soc->umac_reset_ctx.skel_enable);
}

/**
 * dp_umac_rst_skel_enable_get() - Get skel dbg flag for umac reset
 * @soc: dp soc handle
 *
 * Return: enable/disable flag
 */
static bool dp_umac_rst_skel_enable_get(struct dp_soc *soc)
{
	return soc->umac_reset_ctx.skel_enable;
}
#else
static void dp_umac_rst_skel_enable_update(struct dp_soc *soc, bool en)
{
}

static bool dp_umac_rst_skel_enable_get(struct dp_soc *soc)
{
	return false;
}
#endif

#ifndef WLAN_SOFTUMAC_SUPPORT
static void dp_print_reg_write_stats(struct dp_soc *soc)
{
	hal_dump_reg_write_stats(soc->hal_soc);
	hal_dump_reg_write_srng_stats(soc->hal_soc);
}
#else
static void dp_print_reg_write_stats(struct dp_soc *soc)
{
	hif_print_reg_write_stats(soc->hif_handle);
}
#endif

/**
 * dp_print_host_stats()- Function to print the stats aggregated at host
 * @vdev: DP_VDEV handle
 * @req: host stats type
 * @soc: dp soc handler
 *
 * Return: 0 on success, print error message in case of failure
 */
static int
dp_print_host_stats(struct dp_vdev *vdev,
		    struct cdp_txrx_stats_req *req,
		    struct dp_soc *soc)
{
	struct dp_pdev *pdev = (struct dp_pdev *)vdev->pdev;
	enum cdp_host_txrx_stats type =
			dp_stats_mapping_table[req->stats][STATS_HOST];

	dp_aggregate_pdev_stats(pdev);

	switch (type) {
	case TXRX_CLEAR_STATS:
		dp_txrx_host_stats_clr(vdev, soc);
		break;
	case TXRX_RX_RATE_STATS:
		dp_print_rx_rates(vdev);
		break;
	case TXRX_TX_RATE_STATS:
		dp_print_tx_rates(vdev);
		break;
	case TXRX_TX_HOST_STATS:
		dp_print_pdev_tx_stats(pdev);
		dp_print_soc_tx_stats(pdev->soc);
		dp_print_global_desc_count();
		dp_print_vdev_mlo_mcast_tx_stats(vdev);
		break;
	case TXRX_RX_HOST_STATS:
		dp_print_pdev_rx_stats(pdev);
		dp_print_soc_rx_stats(pdev->soc);
		break;
	case TXRX_AST_STATS:
		dp_print_ast_stats(pdev->soc);
		dp_print_mec_stats(pdev->soc);
		dp_print_peer_table(vdev);
		if (soc->arch_ops.dp_mlo_print_ptnr_info)
			soc->arch_ops.dp_mlo_print_ptnr_info(vdev);
		break;
	case TXRX_SRNG_PTR_STATS:
		dp_print_ring_stats(pdev);
		break;
	case TXRX_RX_MON_STATS:
		dp_monitor_print_pdev_rx_mon_stats(pdev);
		break;
	case TXRX_REO_QUEUE_STATS:
		dp_get_host_peer_stats((struct cdp_soc_t *)pdev->soc,
				       req->peer_addr);
		break;
	case TXRX_SOC_CFG_PARAMS:
		dp_print_soc_cfg_params(pdev->soc);
		break;
	case TXRX_PDEV_CFG_PARAMS:
		dp_print_pdev_cfg_params(pdev);
		break;
	case TXRX_NAPI_STATS:
		dp_print_napi_stats(pdev->soc);
		break;
	case TXRX_SOC_INTERRUPT_STATS:
		dp_print_soc_interrupt_stats(pdev->soc);
		break;
	case TXRX_SOC_FSE_STATS:
		if (soc->cdp_soc.ol_ops->dp_print_fisa_stats)
			soc->cdp_soc.ol_ops->dp_print_fisa_stats(
						CDP_FISA_STATS_ID_DUMP_HW_FST);
		break;
	case TXRX_HAL_REG_WRITE_STATS:
		dp_print_reg_write_stats(pdev->soc);
		break;
	case TXRX_SOC_REO_HW_DESC_DUMP:
		dp_get_rx_reo_queue_info((struct cdp_soc_t *)pdev->soc,
					 vdev->vdev_id);
		break;
	case TXRX_SOC_WBM_IDLE_HPTP_DUMP:
		dp_dump_wbm_idle_hptp(pdev->soc, pdev);
		break;
	case TXRX_SRNG_USAGE_WM_STATS:
		/* Dump usage watermark stats for all SRNGs */
		dp_dump_srng_high_wm_stats(soc, DP_SRNG_WM_MASK_ALL);
		break;
	case TXRX_PEER_STATS:
		dp_print_per_link_stats((struct cdp_soc_t *)pdev->soc,
					vdev->vdev_id);
		break;
	default:
		dp_info("Wrong Input For TxRx Host Stats");
		dp_txrx_stats_help();
		break;
	}
	return 0;
}

/**
 * dp_pdev_tid_stats_ingress_inc() - increment ingress_stack counter
 * @pdev: pdev handle
 * @val: increase in value
 *
 * Return: void
 */
static void
dp_pdev_tid_stats_ingress_inc(struct dp_pdev *pdev, uint32_t val)
{
	pdev->stats.tid_stats.ingress_stack += val;
}

/**
 * dp_pdev_tid_stats_osif_drop() - increment osif_drop counter
 * @pdev: pdev handle
 * @val: increase in value
 *
 * Return: void
 */
static void
dp_pdev_tid_stats_osif_drop(struct dp_pdev *pdev, uint32_t val)
{
	pdev->stats.tid_stats.osif_drop += val;
}

/**
 * dp_get_fw_peer_stats()- function to print peer stats
 * @soc: soc handle
 * @pdev_id: id of the pdev handle
 * @mac_addr: mac address of the peer
 * @cap: Type of htt stats requested
 * @is_wait: if set, wait on completion from firmware response
 *
 * Currently Supporting only MAC ID based requests Only
 *	1: HTT_PEER_STATS_REQ_MODE_NO_QUERY
 *	2: HTT_PEER_STATS_REQ_MODE_QUERY_TQM
 *	3: HTT_PEER_STATS_REQ_MODE_FLUSH_TQM
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_get_fw_peer_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
		     uint8_t *mac_addr,
		     uint32_t cap, uint32_t is_wait)
{
	int i;
	uint32_t config_param0 = 0;
	uint32_t config_param1 = 0;
	uint32_t config_param2 = 0;
	uint32_t config_param3 = 0;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	HTT_DBG_EXT_STATS_PEER_INFO_IS_MAC_ADDR_SET(config_param0, 1);
	config_param0 |= (1 << (cap + 1));

	for (i = 0; i < HTT_PEER_STATS_MAX_TLV; i++) {
		config_param1 |= (1 << i);
	}

	config_param2 |= (mac_addr[0] & 0x000000ff);
	config_param2 |= ((mac_addr[1] << 8) & 0x0000ff00);
	config_param2 |= ((mac_addr[2] << 16) & 0x00ff0000);
	config_param2 |= ((mac_addr[3] << 24) & 0xff000000);

	config_param3 |= (mac_addr[4] & 0x000000ff);
	config_param3 |= ((mac_addr[5] << 8) & 0x0000ff00);

	if (is_wait) {
		qdf_event_reset(&pdev->fw_peer_stats_event);
		dp_h2t_ext_stats_msg_send(pdev, HTT_DBG_EXT_STATS_PEER_INFO,
					  config_param0, config_param1,
					  config_param2, config_param3,
					  0, DBG_STATS_COOKIE_DP_STATS, 0);
		qdf_wait_single_event(&pdev->fw_peer_stats_event,
				      DP_FW_PEER_STATS_CMP_TIMEOUT_MSEC);
	} else {
		dp_h2t_ext_stats_msg_send(pdev, HTT_DBG_EXT_STATS_PEER_INFO,
					  config_param0, config_param1,
					  config_param2, config_param3,
					  0, DBG_STATS_COOKIE_DEFAULT, 0);
	}

	return QDF_STATUS_SUCCESS;

}

/* This struct definition will be removed from here
 * once it get added in FW headers*/
struct httstats_cmd_req {
    uint32_t    config_param0;
    uint32_t    config_param1;
    uint32_t    config_param2;
    uint32_t    config_param3;
    int cookie;
    u_int8_t    stats_id;
};

/**
 * dp_get_htt_stats: function to process the httstas request
 * @soc: DP soc handle
 * @pdev_id: id of pdev handle
 * @data: pointer to request data
 * @data_len: length for request data
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_get_htt_stats(struct cdp_soc_t *soc, uint8_t pdev_id, void *data,
		 uint32_t data_len)
{
	struct httstats_cmd_req *req = (struct httstats_cmd_req *)data;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	QDF_ASSERT(data_len == sizeof(struct httstats_cmd_req));
	dp_h2t_ext_stats_msg_send(pdev, req->stats_id,
				req->config_param0, req->config_param1,
				req->config_param2, req->config_param3,
				req->cookie, DBG_STATS_COOKIE_DEFAULT, 0);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_set_pdev_tidmap_prty_wifi3() - update tidmap priority in pdev
 * @pdev: DP_PDEV handle
 * @prio: tidmap priority value passed by the user
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS dp_set_pdev_tidmap_prty_wifi3(struct dp_pdev *pdev,
						uint8_t prio)
{
	struct dp_soc *soc = pdev->soc;

	soc->tidmap_prty = prio;

	hal_tx_set_tidmap_prty(soc->hal_soc, prio);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_get_peer_param: function to get parameters in peer
 * @cdp_soc: DP soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @param: parameter type to be set
 * @val: address of buffer
 *
 * Return: val
 */
static QDF_STATUS dp_get_peer_param(struct cdp_soc_t *cdp_soc,  uint8_t vdev_id,
				    uint8_t *peer_mac,
				    enum cdp_peer_param_type param,
				    cdp_config_param_type *val)
{
	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
static inline void
dp_check_map_link_id_band(struct dp_peer *peer)
{
	if (peer->link_id_valid)
		dp_map_link_id_band(peer);
}

/**
 * dp_map_local_link_id_band() - map local link id band
 * @peer: dp peer handle
 *
 * Return: None
 */
static inline
void dp_map_local_link_id_band(struct dp_peer *peer)
{
	struct dp_txrx_peer *txrx_peer = NULL;
	enum dp_bands band;

	txrx_peer = dp_get_txrx_peer(peer);
	if (txrx_peer && peer->local_link_id) {
		band = dp_freq_to_band(peer->freq);
		txrx_peer->ll_band[peer->local_link_id] = band;
	} else {
		dp_info("txrx_peer NULL or local link id not set: %u "
			QDF_MAC_ADDR_FMT, peer->local_link_id,
			QDF_MAC_ADDR_REF(peer->mac_addr.raw));
	}
}
#else
static inline void
dp_check_map_link_id_band(struct dp_peer *peer)
{
}

static inline
void dp_map_local_link_id_band(struct dp_peer *peer)
{
}
#endif

/**
 * dp_set_peer_freq() - Set peer frequency
 * @cdp_soc: DP soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @param: parameter type to be set
 * @val: value of parameter to be set
 *
 * Return: QDF_STATUS_SUCCESS for success. error code for failure.
 */
static inline QDF_STATUS
dp_set_peer_freq(struct cdp_soc_t *cdp_soc,  uint8_t vdev_id,
		 uint8_t *peer_mac, enum cdp_peer_param_type param,
		 cdp_config_param_type val)
{
	struct dp_peer *peer = NULL;
	struct cdp_peer_info peer_info = { 0 };

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac,
				 false, CDP_LINK_PEER_TYPE);

	peer = dp_peer_hash_find_wrapper((struct dp_soc *)cdp_soc,
					 &peer_info, DP_MOD_ID_CDP);
	if (!peer) {
		dp_err("peer NULL,MAC " QDF_MAC_ADDR_FMT ", vdev_id %u",
		       QDF_MAC_ADDR_REF(peer_mac), vdev_id);

		return QDF_STATUS_E_FAILURE;
	}

	peer->freq = val.cdp_peer_param_freq;
	dp_check_map_link_id_band(peer);
	dp_map_local_link_id_band(peer);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	dp_info("Peer " QDF_MAC_ADDR_FMT " vdev_id %u, frequency %u",
		QDF_MAC_ADDR_REF(peer_mac), vdev_id,
		peer->freq);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_set_peer_param: function to set parameters in peer
 * @cdp_soc: DP soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @param: parameter type to be set
 * @val: value of parameter to be set
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS dp_set_peer_param(struct cdp_soc_t *cdp_soc,  uint8_t vdev_id,
				    uint8_t *peer_mac,
				    enum cdp_peer_param_type param,
				    cdp_config_param_type val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_peer *peer =
			dp_peer_get_tgt_peer_hash_find((struct dp_soc *)cdp_soc,
						       peer_mac, 0, vdev_id,
						       DP_MOD_ID_CDP);
	struct dp_txrx_peer *txrx_peer;

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	txrx_peer = peer->txrx_peer;
	if (!txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	switch (param) {
	case CDP_CONFIG_NAWDS:
		txrx_peer->nawds_enabled = val.cdp_peer_param_nawds;
		break;
	case CDP_CONFIG_ISOLATION:
		dp_info("Peer " QDF_MAC_ADDR_FMT " vdev_id %d, isolation %d",
			QDF_MAC_ADDR_REF(peer_mac), vdev_id,
			val.cdp_peer_param_isolation);
		dp_set_peer_isolation(txrx_peer, val.cdp_peer_param_isolation);
		break;
	case CDP_CONFIG_IN_TWT:
		txrx_peer->in_twt = !!(val.cdp_peer_param_in_twt);
		break;
	case CDP_CONFIG_PEER_FREQ:
		status =  dp_set_peer_freq(cdp_soc, vdev_id,
					   peer_mac, param, val);
		break;
	default:
		break;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_set_mld_peer_param: function to set parameters in MLD peer
 * @cdp_soc: DP soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @param: parameter type to be set
 * @val: value of parameter to be set
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS dp_set_mld_peer_param(struct cdp_soc_t *cdp_soc,
					uint8_t vdev_id,
					uint8_t *peer_mac,
					enum cdp_peer_param_type param,
					cdp_config_param_type val)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(cdp_soc);
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	peer = dp_mld_peer_find_hash_find(soc, peer_mac, 0, vdev_id,
					  DP_MOD_ID_CDP);
	if (!peer)
		return QDF_STATUS_E_FAILURE;

	txrx_peer = peer->txrx_peer;
	if (!txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	switch (param) {
	case CDP_CONFIG_MLD_PEER_VDEV:
		status = dp_mld_peer_change_vdev(soc, peer, val.new_vdev_id);
		break;
	default:
		break;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

/**
 * dp_set_peer_param_wrapper: wrapper function to set parameters in
 *			      legacy/link/MLD peer
 * @cdp_soc: DP soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @param: parameter type to be set
 * @val: value of parameter to be set
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS
dp_set_peer_param_wrapper(struct cdp_soc_t *cdp_soc,  uint8_t vdev_id,
			  uint8_t *peer_mac, enum cdp_peer_param_type param,
			  cdp_config_param_type val)
{
	QDF_STATUS status;

	switch (param) {
	case CDP_CONFIG_MLD_PEER_VDEV:
		status = dp_set_mld_peer_param(cdp_soc, vdev_id, peer_mac,
					       param, val);
		break;
	default:
		status = dp_set_peer_param(cdp_soc, vdev_id, peer_mac,
					   param, val);
		break;
	}

	return status;
}
#endif

/**
 * dp_get_pdev_param() - function to get parameters from pdev
 * @cdp_soc: DP soc handle
 * @pdev_id: id of pdev handle
 * @param: parameter type to be get
 * @val: buffer for value
 *
 * Return: status
 */
static QDF_STATUS dp_get_pdev_param(struct cdp_soc_t *cdp_soc, uint8_t pdev_id,
				    enum cdp_pdev_param_type param,
				    cdp_config_param_type *val)
{
	struct cdp_pdev *pdev = (struct cdp_pdev *)
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)cdp_soc,
						   pdev_id);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	switch (param) {
	case CDP_CONFIG_VOW:
		val->cdp_pdev_param_cfg_vow =
				((struct dp_pdev *)pdev)->vow_stats;
		break;
	case CDP_TX_PENDING:
		val->cdp_pdev_param_tx_pending = dp_get_tx_pending(pdev);
		break;
	case CDP_FILTER_MCAST_DATA:
		val->cdp_pdev_param_fltr_mcast =
				dp_monitor_pdev_get_filter_mcast_data(pdev);
		break;
	case CDP_FILTER_NO_DATA:
		val->cdp_pdev_param_fltr_none =
				dp_monitor_pdev_get_filter_non_data(pdev);
		break;
	case CDP_FILTER_UCAST_DATA:
		val->cdp_pdev_param_fltr_ucast =
				dp_monitor_pdev_get_filter_ucast_data(pdev);
		break;
	case CDP_MONITOR_CHANNEL:
		val->cdp_pdev_param_monitor_chan =
			dp_monitor_get_chan_num((struct dp_pdev *)pdev);
		break;
	case CDP_MONITOR_FREQUENCY:
		val->cdp_pdev_param_mon_freq =
			dp_monitor_get_chan_freq((struct dp_pdev *)pdev);
		break;
	case CDP_CONFIG_RXDMA_BUF_RING_SIZE:
		val->cdp_rxdma_buf_ring_size =
			wlan_cfg_get_rx_dma_buf_ring_size(((struct dp_pdev *)pdev)->wlan_cfg_ctx);
		break;
	case CDP_CONFIG_DELAY_STATS:
		val->cdp_pdev_param_cfg_delay_stats =
				((struct dp_pdev *)pdev)->delay_stats_flag;
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_set_pdev_param() - function to set parameters in pdev
 * @cdp_soc: DP soc handle
 * @pdev_id: id of pdev handle
 * @param: parameter type to be set
 * @val: value of parameter to be set
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS dp_set_pdev_param(struct cdp_soc_t *cdp_soc, uint8_t pdev_id,
				    enum cdp_pdev_param_type param,
				    cdp_config_param_type val)
{
	int target_type;
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)cdp_soc,
						   pdev_id);
	enum reg_wifi_band chan_band;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_QCA6750:
	case TARGET_TYPE_WCN6450:
		pdev->ch_band_lmac_id_mapping[REG_BAND_2G] = DP_MAC0_LMAC_ID;
		pdev->ch_band_lmac_id_mapping[REG_BAND_5G] = DP_MAC0_LMAC_ID;
		pdev->ch_band_lmac_id_mapping[REG_BAND_6G] = DP_MAC0_LMAC_ID;
		break;
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
	case TARGET_TYPE_PEACH:
		pdev->ch_band_lmac_id_mapping[REG_BAND_2G] = DP_MAC0_LMAC_ID;
		pdev->ch_band_lmac_id_mapping[REG_BAND_5G] = DP_MAC0_LMAC_ID;
		pdev->ch_band_lmac_id_mapping[REG_BAND_6G] = DP_MAC0_LMAC_ID;
		break;
	default:
		pdev->ch_band_lmac_id_mapping[REG_BAND_2G] = DP_MAC1_LMAC_ID;
		pdev->ch_band_lmac_id_mapping[REG_BAND_5G] = DP_MAC0_LMAC_ID;
		pdev->ch_band_lmac_id_mapping[REG_BAND_6G] = DP_MAC0_LMAC_ID;
		break;
	}

	switch (param) {
	case CDP_CONFIG_TX_CAPTURE:
		return dp_monitor_config_debug_sniffer(pdev,
						val.cdp_pdev_param_tx_capture);
	case CDP_CONFIG_DEBUG_SNIFFER:
		return dp_monitor_config_debug_sniffer(pdev,
						val.cdp_pdev_param_dbg_snf);
	case CDP_CONFIG_BPR_ENABLE:
		return dp_monitor_set_bpr_enable(pdev,
						 val.cdp_pdev_param_bpr_enable);
	case CDP_CONFIG_PRIMARY_RADIO:
		pdev->is_primary = val.cdp_pdev_param_primary_radio;
		break;
	case CDP_CONFIG_CAPTURE_LATENCY:
		pdev->latency_capture_enable = val.cdp_pdev_param_cptr_latcy;
		break;
	case CDP_INGRESS_STATS:
		dp_pdev_tid_stats_ingress_inc(pdev,
					      val.cdp_pdev_param_ingrs_stats);
		break;
	case CDP_OSIF_DROP:
		dp_pdev_tid_stats_osif_drop(pdev,
					    val.cdp_pdev_param_osif_drop);
		break;
	case CDP_CONFIG_ENH_RX_CAPTURE:
		return dp_monitor_config_enh_rx_capture(pdev,
						val.cdp_pdev_param_en_rx_cap);
	case CDP_CONFIG_ENH_TX_CAPTURE:
		return dp_monitor_config_enh_tx_capture(pdev,
						val.cdp_pdev_param_en_tx_cap);
	case CDP_CONFIG_HMMC_TID_OVERRIDE:
		pdev->hmmc_tid_override_en = val.cdp_pdev_param_hmmc_tid_ovrd;
		break;
	case CDP_CONFIG_HMMC_TID_VALUE:
		pdev->hmmc_tid = val.cdp_pdev_param_hmmc_tid;
		break;
	case CDP_CHAN_NOISE_FLOOR:
		pdev->chan_noise_floor = val.cdp_pdev_param_chn_noise_flr;
		break;
	case CDP_TIDMAP_PRTY:
		dp_set_pdev_tidmap_prty_wifi3(pdev,
					      val.cdp_pdev_param_tidmap_prty);
		break;
	case CDP_FILTER_NEIGH_PEERS:
		dp_monitor_set_filter_neigh_peers(pdev,
					val.cdp_pdev_param_fltr_neigh_peers);
		break;
	case CDP_MONITOR_CHANNEL:
		dp_monitor_set_chan_num(pdev, val.cdp_pdev_param_monitor_chan);
		break;
	case CDP_MONITOR_FREQUENCY:
		chan_band = wlan_reg_freq_to_band(val.cdp_pdev_param_mon_freq);
		dp_monitor_set_chan_freq(pdev, val.cdp_pdev_param_mon_freq);
		dp_monitor_set_chan_band(pdev, chan_band);
		break;
	case CDP_CONFIG_BSS_COLOR:
		dp_monitor_set_bsscolor(pdev, val.cdp_pdev_param_bss_color);
		break;
	case CDP_SET_ATF_STATS_ENABLE:
		dp_monitor_set_atf_stats_enable(pdev,
					val.cdp_pdev_param_atf_stats_enable);
		break;
	case CDP_CONFIG_SPECIAL_VAP:
		dp_monitor_pdev_config_scan_spcl_vap(pdev,
					val.cdp_pdev_param_config_special_vap);
		dp_monitor_vdev_set_monitor_mode_buf_rings(pdev);
		break;
	case CDP_RESET_SCAN_SPCL_VAP_STATS_ENABLE:
		dp_monitor_pdev_reset_scan_spcl_vap_stats_enable(pdev,
				val.cdp_pdev_param_reset_scan_spcl_vap_stats_enable);
		break;
	case CDP_CONFIG_ENHANCED_STATS_ENABLE:
		pdev->enhanced_stats_en = val.cdp_pdev_param_enhanced_stats_enable;
		break;
	case CDP_ISOLATION:
		pdev->isolation = val.cdp_pdev_param_isolation;
		break;
	case CDP_CONFIG_UNDECODED_METADATA_CAPTURE_ENABLE:
		return dp_monitor_config_undecoded_metadata_capture(pdev,
				val.cdp_pdev_param_undecoded_metadata_enable);
		break;
	case CDP_CONFIG_RXDMA_BUF_RING_SIZE:
		wlan_cfg_set_rx_dma_buf_ring_size(pdev->wlan_cfg_ctx,
						  val.cdp_rxdma_buf_ring_size);
		break;
	case CDP_CONFIG_VOW:
		pdev->vow_stats = val.cdp_pdev_param_cfg_vow;
		break;
	default:
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_UNDECODED_METADATA_SUPPORT
static
QDF_STATUS dp_set_pdev_phyrx_error_mask(struct cdp_soc_t *cdp_soc,
					uint8_t pdev_id, uint32_t mask,
					uint32_t mask_cont)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)cdp_soc,
						   pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	return dp_monitor_config_undecoded_metadata_phyrx_error_mask(pdev,
				mask, mask_cont);
}

static
QDF_STATUS dp_get_pdev_phyrx_error_mask(struct cdp_soc_t *cdp_soc,
					uint8_t pdev_id, uint32_t *mask,
					uint32_t *mask_cont)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)cdp_soc,
						   pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	return dp_monitor_get_undecoded_metadata_phyrx_error_mask(pdev,
				mask, mask_cont);
}
#endif

#ifdef QCA_PEER_EXT_STATS
static void dp_rx_update_peer_delay_stats(struct dp_soc *soc,
					  qdf_nbuf_t nbuf)
{
	struct dp_peer *peer = NULL;
	uint16_t peer_id, ring_id;
	uint8_t tid = qdf_nbuf_get_tid_val(nbuf);
	struct dp_peer_delay_stats *delay_stats = NULL;

	peer_id = QDF_NBUF_CB_RX_PEER_ID(nbuf);
	if (peer_id > soc->max_peer_id)
		return;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_CDP);
	if (qdf_unlikely(!peer))
		return;

	if (qdf_unlikely(!peer->txrx_peer)) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return;
	}

	if (qdf_likely(peer->txrx_peer->delay_stats)) {
		delay_stats = peer->txrx_peer->delay_stats;
		ring_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
		dp_rx_compute_tid_delay(&delay_stats->delay_tid_stats[tid][ring_id],
					nbuf);
	}
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
}
#else
static inline void dp_rx_update_peer_delay_stats(struct dp_soc *soc,
						 qdf_nbuf_t nbuf)
{
}
#endif

/**
 * dp_calculate_delay_stats() - function to get rx delay stats
 * @cdp_soc: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @nbuf: skb
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_calculate_delay_stats(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
			 qdf_nbuf_t nbuf)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(cdp_soc);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_SUCCESS;

	if (vdev->pdev->delay_stats_flag)
		dp_rx_compute_delay(vdev, nbuf);
	else
		dp_rx_update_peer_delay_stats(soc, nbuf);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_get_vdev_param() - function to get parameters from vdev
 * @cdp_soc: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @param: parameter type to get value
 * @val: buffer address
 *
 * Return: status
 */
static QDF_STATUS dp_get_vdev_param(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
				    enum cdp_vdev_param_type param,
				    cdp_config_param_type *val)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(cdp_soc);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	switch (param) {
	case CDP_ENABLE_WDS:
		val->cdp_vdev_param_wds = vdev->wds_enabled;
		break;
	case CDP_ENABLE_MEC:
		val->cdp_vdev_param_mec = vdev->mec_enabled;
		break;
	case CDP_ENABLE_DA_WAR:
		val->cdp_vdev_param_da_war = vdev->pdev->soc->da_war_enabled;
		break;
	case CDP_ENABLE_IGMP_MCAST_EN:
		val->cdp_vdev_param_igmp_mcast_en = vdev->igmp_mcast_enhanc_en;
		break;
	case CDP_ENABLE_MCAST_EN:
		val->cdp_vdev_param_mcast_en = vdev->mcast_enhancement_en;
		break;
	case CDP_ENABLE_HLOS_TID_OVERRIDE:
		val->cdp_vdev_param_hlos_tid_override =
			    dp_vdev_get_hlos_tid_override((struct cdp_vdev *)vdev);
		break;
	case CDP_ENABLE_PEER_AUTHORIZE:
		val->cdp_vdev_param_peer_authorize =
			    vdev->peer_authorize;
		break;
	case CDP_TX_ENCAP_TYPE:
		val->cdp_vdev_param_tx_encap = vdev->tx_encap_type;
		break;
	case CDP_ENABLE_CIPHER:
		val->cdp_vdev_param_cipher_en = vdev->sec_type;
		break;
#ifdef WLAN_SUPPORT_MESH_LATENCY
	case CDP_ENABLE_PEER_TID_LATENCY:
		val->cdp_vdev_param_peer_tid_latency_enable =
			vdev->peer_tid_latency_enabled;
		break;
	case CDP_SET_VAP_MESH_TID:
		val->cdp_vdev_param_mesh_tid =
				vdev->mesh_tid_latency_config.latency_tid;
		break;
#endif
	case CDP_DROP_3ADDR_MCAST:
		val->cdp_drop_3addr_mcast = vdev->drop_3addr_mcast;
		break;
	case CDP_SET_MCAST_VDEV:
		soc->arch_ops.txrx_get_vdev_mcast_param(soc, vdev, val);
		break;
#ifdef QCA_SUPPORT_WDS_EXTENDED
	case CDP_DROP_TX_MCAST:
		val->cdp_drop_tx_mcast = vdev->drop_tx_mcast;
		break;
#endif

#ifdef MESH_MODE_SUPPORT
	case CDP_MESH_RX_FILTER:
		val->cdp_vdev_param_mesh_rx_filter = vdev->mesh_rx_filter;
		break;
	case CDP_MESH_MODE:
		val->cdp_vdev_param_mesh_mode = vdev->mesh_vdev;
		break;
#endif
	case CDP_ENABLE_NAWDS:
		val->cdp_vdev_param_nawds = vdev->nawds_enabled;
		break;

	case CDP_ENABLE_WRAP:
		val->cdp_vdev_param_wrap = vdev->wrap_vdev;
		break;

#ifdef DP_TRAFFIC_END_INDICATION
	case CDP_ENABLE_TRAFFIC_END_INDICATION:
		val->cdp_vdev_param_traffic_end_ind = vdev->traffic_end_ind_en;
		break;
#endif

	default:
		dp_cdp_err("%pK: param value %d is wrong",
			   soc, param);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_set_vdev_param() - function to set parameters in vdev
 * @cdp_soc: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @param: parameter type to get value
 * @val: value
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_set_vdev_param(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
		  enum cdp_vdev_param_type param, cdp_config_param_type val)
{
	struct dp_soc *dsoc = (struct dp_soc *)cdp_soc;
	struct dp_vdev *vdev =
		dp_vdev_get_ref_by_id(dsoc, vdev_id, DP_MOD_ID_CDP);
	uint32_t var = 0;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	switch (param) {
	case CDP_ENABLE_WDS:
		dp_cdp_err("%pK: wds_enable %d for vdev(%pK) id(%d)",
			   dsoc, val.cdp_vdev_param_wds, vdev, vdev->vdev_id);
		vdev->wds_enabled = val.cdp_vdev_param_wds;
		break;
	case CDP_ENABLE_MEC:
		dp_cdp_err("%pK: mec_enable %d for vdev(%pK) id(%d)",
			   dsoc, val.cdp_vdev_param_mec, vdev, vdev->vdev_id);
		vdev->mec_enabled = val.cdp_vdev_param_mec;
		break;
	case CDP_ENABLE_DA_WAR:
		dp_cdp_err("%pK: da_war_enable %d for vdev(%pK) id(%d)",
			   dsoc, val.cdp_vdev_param_da_war, vdev, vdev->vdev_id);
		vdev->pdev->soc->da_war_enabled = val.cdp_vdev_param_da_war;
		dp_wds_flush_ast_table_wifi3(((struct cdp_soc_t *)
					     vdev->pdev->soc));
		break;
	case CDP_ENABLE_NAWDS:
		vdev->nawds_enabled = val.cdp_vdev_param_nawds;
		break;
	case CDP_ENABLE_MCAST_EN:
		vdev->mcast_enhancement_en = val.cdp_vdev_param_mcast_en;
		break;
	case CDP_ENABLE_IGMP_MCAST_EN:
		vdev->igmp_mcast_enhanc_en = val.cdp_vdev_param_igmp_mcast_en;
		break;
	case CDP_ENABLE_PROXYSTA:
		vdev->proxysta_vdev = val.cdp_vdev_param_proxysta;
		break;
	case CDP_UPDATE_TDLS_FLAGS:
		vdev->tdls_link_connected = val.cdp_vdev_param_tdls_flags;
		break;
	case CDP_CFG_WDS_AGING_TIMER:
		var = val.cdp_vdev_param_aging_tmr;
		if (!var)
			qdf_timer_stop(&vdev->pdev->soc->ast_aging_timer);
		else if (var != vdev->wds_aging_timer_val)
			qdf_timer_mod(&vdev->pdev->soc->ast_aging_timer, var);

		vdev->wds_aging_timer_val = var;
		break;
	case CDP_ENABLE_AP_BRIDGE:
		if (wlan_op_mode_sta != vdev->opmode)
			vdev->ap_bridge_enabled = val.cdp_vdev_param_ap_brdg_en;
		else
			vdev->ap_bridge_enabled = false;
		break;
	case CDP_ENABLE_CIPHER:
		vdev->sec_type = val.cdp_vdev_param_cipher_en;
		break;
	case CDP_ENABLE_QWRAP_ISOLATION:
		vdev->isolation_vdev = val.cdp_vdev_param_qwrap_isolation;
		break;
	case CDP_UPDATE_MULTIPASS:
		vdev->multipass_en = val.cdp_vdev_param_update_multipass;
		dp_info("vdev %d Multipass enable %d", vdev_id,
			vdev->multipass_en);
		break;
	case CDP_TX_ENCAP_TYPE:
		vdev->tx_encap_type = val.cdp_vdev_param_tx_encap;
		break;
	case CDP_RX_DECAP_TYPE:
		vdev->rx_decap_type = val.cdp_vdev_param_rx_decap;
		break;
	case CDP_TID_VDEV_PRTY:
		vdev->tidmap_prty = val.cdp_vdev_param_tidmap_prty;
		break;
	case CDP_TIDMAP_TBL_ID:
		vdev->tidmap_tbl_id = val.cdp_vdev_param_tidmap_tbl_id;
		break;
#ifdef MESH_MODE_SUPPORT
	case CDP_MESH_RX_FILTER:
		dp_vdev_set_mesh_rx_filter((struct cdp_vdev *)vdev,
					   val.cdp_vdev_param_mesh_rx_filter);
		break;
	case CDP_MESH_MODE:
		dp_vdev_set_mesh_mode((struct cdp_vdev *)vdev,
				      val.cdp_vdev_param_mesh_mode);
		break;
#endif
	case CDP_ENABLE_HLOS_TID_OVERRIDE:
		dp_info("vdev_id %d enable hlod tid override %d", vdev_id,
			val.cdp_vdev_param_hlos_tid_override);
		dp_vdev_set_hlos_tid_override(vdev,
				val.cdp_vdev_param_hlos_tid_override);
		break;
#ifdef QCA_SUPPORT_WDS_EXTENDED
	case CDP_CFG_WDS_EXT:
		if (vdev->opmode == wlan_op_mode_ap)
			vdev->wds_ext_enabled = val.cdp_vdev_param_wds_ext;
		break;
	case CDP_DROP_TX_MCAST:
		dp_info("vdev_id %d drop tx mcast :%d", vdev_id,
			val.cdp_drop_tx_mcast);
		vdev->drop_tx_mcast = val.cdp_drop_tx_mcast;
		break;
#endif
	case CDP_ENABLE_PEER_AUTHORIZE:
		vdev->peer_authorize = val.cdp_vdev_param_peer_authorize;
		break;
#ifdef WLAN_SUPPORT_MESH_LATENCY
	case CDP_ENABLE_PEER_TID_LATENCY:
		dp_info("vdev_id %d enable peer tid latency %d", vdev_id,
			val.cdp_vdev_param_peer_tid_latency_enable);
		vdev->peer_tid_latency_enabled =
			val.cdp_vdev_param_peer_tid_latency_enable;
		break;
	case CDP_SET_VAP_MESH_TID:
		dp_info("vdev_id %d enable peer tid latency %d", vdev_id,
			val.cdp_vdev_param_mesh_tid);
		vdev->mesh_tid_latency_config.latency_tid
				= val.cdp_vdev_param_mesh_tid;
		break;
#endif
#ifdef WLAN_VENDOR_SPECIFIC_BAR_UPDATE
	case CDP_SKIP_BAR_UPDATE_AP:
		dp_info("vdev_id %d skip BAR update: %u", vdev_id,
			val.cdp_skip_bar_update);
		vdev->skip_bar_update = val.cdp_skip_bar_update;
		vdev->skip_bar_update_last_ts = 0;
		break;
#endif
	case CDP_DROP_3ADDR_MCAST:
		dp_info("vdev_id %d drop 3 addr mcast :%d", vdev_id,
			val.cdp_drop_3addr_mcast);
		vdev->drop_3addr_mcast = val.cdp_drop_3addr_mcast;
		break;
	case CDP_ENABLE_WRAP:
		vdev->wrap_vdev = val.cdp_vdev_param_wrap;
		break;
#ifdef DP_TRAFFIC_END_INDICATION
	case CDP_ENABLE_TRAFFIC_END_INDICATION:
		vdev->traffic_end_ind_en = val.cdp_vdev_param_traffic_end_ind;
		break;
#endif
#ifdef FEATURE_DIRECT_LINK
	case CDP_VDEV_TX_TO_FW:
		dp_info("vdev_id %d to_fw :%d", vdev_id, val.cdp_vdev_tx_to_fw);
		vdev->to_fw = val.cdp_vdev_tx_to_fw;
		break;
#endif
	case CDP_VDEV_SET_MAC_ADDR:
		dp_info("set mac addr, old mac addr" QDF_MAC_ADDR_FMT
			" new mac addr: " QDF_MAC_ADDR_FMT " for vdev %d",
			QDF_MAC_ADDR_REF(vdev->mac_addr.raw),
			QDF_MAC_ADDR_REF(val.mac_addr), vdev->vdev_id);
		qdf_mem_copy(&vdev->mac_addr.raw[0], val.mac_addr,
			     QDF_MAC_ADDR_SIZE);
		break;
	default:
		break;
	}

	dp_tx_vdev_update_search_flags((struct dp_vdev *)vdev);
	dsoc->arch_ops.txrx_set_vdev_param(dsoc, vdev, param, val);

	/* Update PDEV flags as VDEV flags are updated */
	dp_pdev_update_fast_rx_flag(dsoc, vdev->pdev);
	dp_vdev_unref_delete(dsoc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

#if defined(FEATURE_WLAN_TDLS) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * dp_update_mlo_vdev_for_tdls() - update mlo vdev configuration
 *                                 for TDLS
 * @cdp_soc: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @param: parameter type for vdev
 * @val: value
 *
 * If TDLS connection is from secondary vdev, then copy osif_vdev from
 * primary vdev to support RX, update TX bank register info for primary
 * vdev as well.
 * If TDLS connection is from primary vdev, same as before.
 *
 * Return: None
 */
static void
dp_update_mlo_vdev_for_tdls(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
			    enum cdp_vdev_param_type param,
			    cdp_config_param_type val)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_peer *peer;
	struct dp_peer *tmp_peer;
	struct dp_peer *mld_peer;
	struct dp_vdev *vdev = NULL;
	struct dp_vdev *pri_vdev = NULL;
	uint8_t pri_vdev_id = CDP_INVALID_VDEV_ID;

	if (param != CDP_UPDATE_TDLS_FLAGS)
		return;

	dp_info("update TDLS flag for vdev_id %d, val %d",
		vdev_id, val.cdp_vdev_param_tdls_flags);
	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_MISC);
	/* only check for STA mode vdev */
	if (!vdev || vdev->opmode != wlan_op_mode_sta) {
		dp_info("vdev is not as expected for TDLS");
		goto comp_ret;
	}

	/* Find primary vdev_id */
	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH_SAFE(peer, &vdev->peer_list,
			   peer_list_elem,
			   tmp_peer) {
		if (dp_peer_get_ref(soc, peer, DP_MOD_ID_CONFIG) ==
					QDF_STATUS_SUCCESS) {
			/* do check only if MLO link peer exist */
			if (IS_MLO_DP_LINK_PEER(peer)) {
				mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
				pri_vdev_id = mld_peer->vdev->vdev_id;
				dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
				break;
			}
			dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
		}
	}
	qdf_spin_unlock_bh(&vdev->peer_list_lock);

	if (pri_vdev_id != CDP_INVALID_VDEV_ID)
		pri_vdev = dp_vdev_get_ref_by_id(soc, pri_vdev_id,
						 DP_MOD_ID_MISC);

	/* If current vdev is not same as primary vdev */
	if (pri_vdev && pri_vdev != vdev) {
		dp_info("primary vdev [%d] %pK different with vdev [%d] %pK",
			pri_vdev->vdev_id, pri_vdev,
			vdev->vdev_id, vdev);
		/* update osif_vdev to support RX for vdev */
		vdev->osif_vdev = pri_vdev->osif_vdev;
		dp_set_vdev_param(cdp_soc, pri_vdev->vdev_id,
				  CDP_UPDATE_TDLS_FLAGS, val);
	}

comp_ret:
	if (pri_vdev)
		dp_vdev_unref_delete(soc, pri_vdev, DP_MOD_ID_MISC);
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_MISC);
}

static QDF_STATUS
dp_set_vdev_param_wrapper(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
			  enum cdp_vdev_param_type param,
			  cdp_config_param_type val)
{
	dp_update_mlo_vdev_for_tdls(cdp_soc, vdev_id, param, val);

	return dp_set_vdev_param(cdp_soc, vdev_id, param, val);
}
#else
static QDF_STATUS
dp_set_vdev_param_wrapper(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
			  enum cdp_vdev_param_type param,
			  cdp_config_param_type val)
{
	return dp_set_vdev_param(cdp_soc, vdev_id, param, val);
}
#endif

/**
 * dp_rx_peer_metadata_ver_update() - update rx peer metadata version and
 *                                    corresponding filed shift and mask
 * @soc: Handle to DP Soc structure
 * @peer_md_ver: RX peer metadata version value
 *
 * Return: None
 */
static void
dp_rx_peer_metadata_ver_update(struct dp_soc *soc, uint8_t peer_md_ver)
{
	dp_info("rx_peer_metadata version %d", peer_md_ver);

	switch (peer_md_ver) {
	case 0: /* htt_rx_peer_metadata_v0 */
		soc->htt_peer_id_s = HTT_RX_PEER_META_DATA_V0_PEER_ID_S;
		soc->htt_peer_id_m = HTT_RX_PEER_META_DATA_V0_PEER_ID_M;
		soc->htt_vdev_id_s = HTT_RX_PEER_META_DATA_V0_VDEV_ID_S;
		soc->htt_vdev_id_m = HTT_RX_PEER_META_DATA_V0_VDEV_ID_M;
		break;
	case 1: /* htt_rx_peer_metadata_v1 */
		soc->htt_peer_id_s = HTT_RX_PEER_META_DATA_V1_PEER_ID_S;
		soc->htt_peer_id_m = HTT_RX_PEER_META_DATA_V1_PEER_ID_M;
		soc->htt_vdev_id_s = HTT_RX_PEER_META_DATA_V1_VDEV_ID_S;
		soc->htt_vdev_id_m = HTT_RX_PEER_META_DATA_V1_VDEV_ID_M;
		soc->htt_mld_peer_valid_s =
				HTT_RX_PEER_META_DATA_V1_ML_PEER_VALID_S;
		soc->htt_mld_peer_valid_m =
				HTT_RX_PEER_META_DATA_V1_ML_PEER_VALID_M;
		break;
	case 2: /* htt_rx_peer_metadata_v1a */
		soc->htt_peer_id_s = HTT_RX_PEER_META_DATA_V1A_PEER_ID_S;
		soc->htt_peer_id_m = HTT_RX_PEER_META_DATA_V1A_PEER_ID_M;
		soc->htt_vdev_id_s = HTT_RX_PEER_META_DATA_V1A_VDEV_ID_S;
		soc->htt_vdev_id_m = HTT_RX_PEER_META_DATA_V1A_VDEV_ID_M;
		soc->htt_mld_peer_valid_s =
				HTT_RX_PEER_META_DATA_V1A_ML_PEER_VALID_S;
		soc->htt_mld_peer_valid_m =
				HTT_RX_PEER_META_DATA_V1A_ML_PEER_VALID_M;
		break;
	case 3: /* htt_rx_peer_metadata_v1b */
		soc->htt_peer_id_s = HTT_RX_PEER_META_DATA_V1B_PEER_ID_S;
		soc->htt_peer_id_m = HTT_RX_PEER_META_DATA_V1B_PEER_ID_M;
		soc->htt_vdev_id_s = HTT_RX_PEER_META_DATA_V1B_VDEV_ID_S;
		soc->htt_vdev_id_m = HTT_RX_PEER_META_DATA_V1B_VDEV_ID_M;
		soc->htt_mld_peer_valid_s =
				HTT_RX_PEER_META_DATA_V1B_ML_PEER_VALID_S;
		soc->htt_mld_peer_valid_m =
				HTT_RX_PEER_META_DATA_V1B_ML_PEER_VALID_M;
		break;
	default:
		dp_err("invliad rx_peer_metadata version %d", peer_md_ver);
		break;
	}

	soc->rx_peer_metadata_ver = peer_md_ver;
}

/**
 * dp_set_psoc_param: function to set parameters in psoc
 * @cdp_soc: DP soc handle
 * @param: parameter type to be set
 * @val: value of parameter to be set
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_set_psoc_param(struct cdp_soc_t *cdp_soc,
		  enum cdp_psoc_param_type param, cdp_config_param_type val)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx = soc->wlan_cfg_ctx;

	switch (param) {
	case CDP_ENABLE_RATE_STATS:
		soc->peerstats_enabled = val.cdp_psoc_param_en_rate_stats;
		break;
	case CDP_SET_NSS_CFG:
		wlan_cfg_set_dp_soc_nss_cfg(wlan_cfg_ctx,
					    val.cdp_psoc_param_en_nss_cfg);
		/*
		 * TODO: masked out based on the per offloaded radio
		 */
		switch (val.cdp_psoc_param_en_nss_cfg) {
		case dp_nss_cfg_default:
			break;
		case dp_nss_cfg_first_radio:
		/*
		 * This configuration is valid for single band radio which
		 * is also NSS offload.
		 */
		case dp_nss_cfg_dbdc:
		case dp_nss_cfg_dbtc:
			wlan_cfg_set_num_tx_desc_pool(wlan_cfg_ctx, 0);
			wlan_cfg_set_num_tx_ext_desc_pool(wlan_cfg_ctx, 0);
			wlan_cfg_set_num_tx_desc(wlan_cfg_ctx, 0);
			wlan_cfg_set_num_tx_spl_desc(soc->wlan_cfg_ctx, 0);
			wlan_cfg_set_num_tx_ext_desc(wlan_cfg_ctx, 0);
			break;
		default:
			dp_cdp_err("%pK: Invalid offload config %d",
				   soc, val.cdp_psoc_param_en_nss_cfg);
		}

			dp_cdp_err("%pK: nss-wifi<0> nss config is enabled"
				   , soc);
		break;
	case CDP_SET_PREFERRED_HW_MODE:
		soc->preferred_hw_mode = val.cdp_psoc_param_preferred_hw_mode;
		break;
	case CDP_IPA_ENABLE:
		soc->wlan_cfg_ctx->ipa_enabled = val.cdp_ipa_enabled;
		break;
	case CDP_CFG_VDEV_STATS_HW_OFFLOAD:
		wlan_cfg_set_vdev_stats_hw_offload_config(wlan_cfg_ctx,
				val.cdp_psoc_param_vdev_stats_hw_offload);
		break;
	case CDP_SAWF_ENABLE:
		wlan_cfg_set_sawf_config(wlan_cfg_ctx, val.cdp_sawf_enabled);
		break;
	case CDP_UMAC_RST_SKEL_ENABLE:
		dp_umac_rst_skel_enable_update(soc, val.cdp_umac_rst_skel);
		break;
	case CDP_UMAC_RESET_STATS:
		dp_umac_reset_stats_print(soc);
		break;
	case CDP_SAWF_STATS:
		wlan_cfg_set_sawf_stats_config(wlan_cfg_ctx,
					       val.cdp_sawf_stats);
		break;
	case CDP_CFG_RX_PEER_METADATA_VER:
		dp_rx_peer_metadata_ver_update(
				soc, val.cdp_peer_metadata_ver);
		break;
	case CDP_CFG_TX_DESC_NUM:
		wlan_cfg_set_num_tx_desc(wlan_cfg_ctx,
					 val.cdp_tx_desc_num);
		break;
	case CDP_CFG_TX_EXT_DESC_NUM:
		wlan_cfg_set_num_tx_ext_desc(wlan_cfg_ctx,
					     val.cdp_tx_ext_desc_num);
		break;
	case CDP_CFG_TX_RING_SIZE:
		wlan_cfg_set_tx_ring_size(wlan_cfg_ctx,
					  val.cdp_tx_ring_size);
		break;
	case CDP_CFG_TX_COMPL_RING_SIZE:
		wlan_cfg_set_tx_comp_ring_size(wlan_cfg_ctx,
					       val.cdp_tx_comp_ring_size);
		break;
	case CDP_CFG_RX_SW_DESC_NUM:
		wlan_cfg_set_dp_soc_rx_sw_desc_num(wlan_cfg_ctx,
						   val.cdp_rx_sw_desc_num);
		break;
	case CDP_CFG_REO_DST_RING_SIZE:
		wlan_cfg_set_reo_dst_ring_size(wlan_cfg_ctx,
					       val.cdp_reo_dst_ring_size);
		break;
	case CDP_CFG_RXDMA_REFILL_RING_SIZE:
		wlan_cfg_set_dp_soc_rxdma_refill_ring_size(wlan_cfg_ctx,
					val.cdp_rxdma_refill_ring_size);
		break;
#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
	case CDP_CFG_RX_REFILL_POOL_NUM:
		wlan_cfg_set_rx_refill_buf_pool_size(wlan_cfg_ctx,
						val.cdp_rx_refill_buf_pool_size);
		break;
#endif
	case CDP_CFG_AST_INDICATION_DISABLE:
		wlan_cfg_set_ast_indication_disable
			(wlan_cfg_ctx, val.cdp_ast_indication_disable);
		break;
	case CDP_CONFIG_DP_DEBUG_LOG:
		soc->dp_debug_log_en = val.cdp_psoc_param_dp_debug_log;
		break;
	case CDP_MONITOR_FLAG:
		soc->mon_flags = val.cdp_monitor_flag;
		dp_info("monior interface flags: 0x%x", soc->mon_flags);
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
/**
 * dp_get_mldev_mode: function to get mlo operation mode
 * @soc: soc structure for data path
 *
 * Return: uint8_t
 */
static uint8_t dp_get_mldev_mode(struct dp_soc *soc)
{
	return soc->mld_mode_ap;
}
#else
static uint8_t dp_get_mldev_mode(struct dp_soc *cdp_soc)
{
	return MLD_MODE_INVALID;
}
#endif

/**
 * dp_get_psoc_param: function to get parameters in soc
 * @cdp_soc: DP soc handle
 * @param: parameter type to be get
 * @val: address of buffer
 *
 * Return: status
 */
static QDF_STATUS dp_get_psoc_param(struct cdp_soc_t *cdp_soc,
				    enum cdp_psoc_param_type param,
				    cdp_config_param_type *val)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx;

	if (!soc)
		return QDF_STATUS_E_FAILURE;

	wlan_cfg_ctx = soc->wlan_cfg_ctx;

	switch (param) {
	case CDP_ENABLE_RATE_STATS:
		val->cdp_psoc_param_en_rate_stats = soc->peerstats_enabled;
		break;
	case CDP_CFG_PEER_EXT_STATS:
		val->cdp_psoc_param_pext_stats =
			wlan_cfg_is_peer_ext_stats_enabled(wlan_cfg_ctx);
		break;
	case CDP_CFG_VDEV_STATS_HW_OFFLOAD:
		val->cdp_psoc_param_vdev_stats_hw_offload =
			wlan_cfg_get_vdev_stats_hw_offload_config(wlan_cfg_ctx);
		break;
	case CDP_UMAC_RST_SKEL_ENABLE:
		val->cdp_umac_rst_skel = dp_umac_rst_skel_enable_get(soc);
		break;
	case CDP_TXRX_HAL_SOC_HDL:
		val->hal_soc_hdl = soc->hal_soc;
		break;
	case CDP_CFG_TX_DESC_NUM:
		val->cdp_tx_desc_num = wlan_cfg_get_num_tx_desc(wlan_cfg_ctx);
		break;
	case CDP_CFG_TX_EXT_DESC_NUM:
		val->cdp_tx_ext_desc_num =
			wlan_cfg_get_num_tx_ext_desc(wlan_cfg_ctx);
		break;
	case CDP_CFG_TX_RING_SIZE:
		val->cdp_tx_ring_size = wlan_cfg_tx_ring_size(wlan_cfg_ctx);
		break;
	case CDP_CFG_TX_COMPL_RING_SIZE:
		val->cdp_tx_comp_ring_size =
			wlan_cfg_tx_comp_ring_size(wlan_cfg_ctx);
		break;
	case CDP_CFG_RX_SW_DESC_NUM:
		val->cdp_rx_sw_desc_num =
			wlan_cfg_get_dp_soc_rx_sw_desc_num(wlan_cfg_ctx);
		break;
	case CDP_CFG_REO_DST_RING_SIZE:
		val->cdp_reo_dst_ring_size =
			wlan_cfg_get_reo_dst_ring_size(wlan_cfg_ctx);
		break;
	case CDP_CFG_RXDMA_REFILL_RING_SIZE:
		val->cdp_rxdma_refill_ring_size =
			wlan_cfg_get_dp_soc_rxdma_refill_ring_size(wlan_cfg_ctx);
		break;
#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
	case CDP_CFG_RX_REFILL_POOL_NUM:
		val->cdp_rx_refill_buf_pool_size =
			wlan_cfg_get_rx_refill_buf_pool_size(wlan_cfg_ctx);
		break;
#endif
	case CDP_CFG_FISA_PARAMS:
		val->fisa_params.fisa_fst_size = wlan_cfg_get_rx_flow_search_table_size(soc->wlan_cfg_ctx);
		val->fisa_params.rx_flow_max_search =
			wlan_cfg_rx_fst_get_max_search(soc->wlan_cfg_ctx);
		val->fisa_params.rx_toeplitz_hash_key =
			wlan_cfg_rx_fst_get_hash_key(soc->wlan_cfg_ctx);
		break;
	case CDP_RX_PKT_TLV_SIZE:
		val->rx_pkt_tlv_size = soc->rx_pkt_tlv_size;
		break;
	case CDP_CFG_GET_MLO_OPER_MODE:
		val->cdp_psoc_param_mlo_oper_mode = dp_get_mldev_mode(soc);
		break;
	case CDP_CFG_PEER_JITTER_STATS:
		val->cdp_psoc_param_jitter_stats =
			wlan_cfg_is_peer_jitter_stats_enabled(soc->wlan_cfg_ctx);
		break;
	case CDP_CONFIG_DP_DEBUG_LOG:
		val->cdp_psoc_param_dp_debug_log = soc->dp_debug_log_en;
		break;
	default:
		dp_warn("Invalid param: %u", param);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_set_vdev_dscp_tid_map_wifi3() - Update Map ID selected for particular vdev
 * @cdp_soc: CDP SOC handle
 * @vdev_id: id of DP_VDEV handle
 * @map_id:ID of map that needs to be updated
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_set_vdev_dscp_tid_map_wifi3(ol_txrx_soc_handle cdp_soc,
						 uint8_t vdev_id,
						 uint8_t map_id)
{
	cdp_config_param_type val;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(cdp_soc);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	if (vdev) {
		vdev->dscp_tid_map_id = map_id;
		val.cdp_vdev_param_dscp_tid_map_id = map_id;
		soc->arch_ops.txrx_set_vdev_param(soc,
						  vdev,
						  CDP_UPDATE_DSCP_TO_TID_MAP,
						  val);
		/* Update flag for transmit tid classification */
		if (vdev->dscp_tid_map_id < soc->num_hw_dscp_tid_map)
			vdev->skip_sw_tid_classification |=
				DP_TX_HW_DSCP_TID_MAP_VALID;
		else
			vdev->skip_sw_tid_classification &=
				~DP_TX_HW_DSCP_TID_MAP_VALID;
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

#ifdef DP_RATETABLE_SUPPORT
static int dp_txrx_get_ratekbps(int preamb, int mcs,
				int htflag, int gintval)
{
	uint32_t rix;
	uint16_t ratecode;
	enum cdp_punctured_modes punc_mode = NO_PUNCTURE;

	return dp_getrateindex((uint32_t)gintval, (uint16_t)mcs, 1,
			       (uint8_t)preamb, 1, punc_mode,
			       &rix, &ratecode);
}
#else
static int dp_txrx_get_ratekbps(int preamb, int mcs,
				int htflag, int gintval)
{
	return 0;
}
#endif

/**
 * dp_txrx_get_pdev_stats() - Returns cdp_pdev_stats
 * @soc: DP soc handle
 * @pdev_id: id of DP pdev handle
 * @pdev_stats: buffer to copy to
 *
 * Return: status success/failure
 */
static QDF_STATUS
dp_txrx_get_pdev_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
		       struct cdp_pdev_stats *pdev_stats)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	dp_aggregate_pdev_stats(pdev);

	qdf_mem_copy(pdev_stats, &pdev->stats, sizeof(struct cdp_pdev_stats));
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_txrx_update_vdev_me_stats() - Update vdev ME stats sent from CDP
 * @vdev: DP vdev handle
 * @buf: buffer containing specific stats structure
 * @xmit_type: xmit type of packet - MLD/Link
 *
 * Return: void
 */
static void dp_txrx_update_vdev_me_stats(struct dp_vdev *vdev,
					 void *buf, uint8_t xmit_type)
{
	struct cdp_tx_ingress_stats *host_stats = NULL;

	if (!buf) {
		dp_cdp_err("%pK: Invalid host stats buf", vdev->pdev->soc);
		return;
	}
	host_stats = (struct cdp_tx_ingress_stats *)buf;

	DP_STATS_INC_PKT(vdev, tx_i[xmit_type].mcast_en.mcast_pkt,
			 host_stats->mcast_en.mcast_pkt.num,
			 host_stats->mcast_en.mcast_pkt.bytes);
	DP_STATS_INC(vdev, tx_i[xmit_type].mcast_en.dropped_map_error,
		     host_stats->mcast_en.dropped_map_error);
	DP_STATS_INC(vdev, tx_i[xmit_type].mcast_en.dropped_self_mac,
		     host_stats->mcast_en.dropped_self_mac);
	DP_STATS_INC(vdev, tx_i[xmit_type].mcast_en.dropped_send_fail,
		     host_stats->mcast_en.dropped_send_fail);
	DP_STATS_INC(vdev, tx_i[xmit_type].mcast_en.ucast,
		     host_stats->mcast_en.ucast);
	DP_STATS_INC(vdev, tx_i[xmit_type].mcast_en.fail_seg_alloc,
		     host_stats->mcast_en.fail_seg_alloc);
	DP_STATS_INC(vdev, tx_i[xmit_type].mcast_en.clone_fail,
		     host_stats->mcast_en.clone_fail);
}

/**
 * dp_txrx_update_vdev_igmp_me_stats() - Update vdev IGMP ME stats sent from CDP
 * @vdev: DP vdev handle
 * @buf: buffer containing specific stats structure
 * @xmit_type: xmit type of packet -  MLD/Link
 *
 * Return: void
 */
static void dp_txrx_update_vdev_igmp_me_stats(struct dp_vdev *vdev,
					      void *buf, uint8_t xmit_type)
{
	struct cdp_tx_ingress_stats *host_stats = NULL;

	if (!buf) {
		dp_cdp_err("%pK: Invalid host stats buf", vdev->pdev->soc);
		return;
	}
	host_stats = (struct cdp_tx_ingress_stats *)buf;

	DP_STATS_INC(vdev, tx_i[xmit_type].igmp_mcast_en.igmp_rcvd,
		     host_stats->igmp_mcast_en.igmp_rcvd);
	DP_STATS_INC(vdev, tx_i[xmit_type].igmp_mcast_en.igmp_ucast_converted,
		     host_stats->igmp_mcast_en.igmp_ucast_converted);
}

/**
 * dp_txrx_update_vdev_host_stats() - Update stats sent through CDP
 * @soc_hdl: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @buf: buffer containing specific stats structure
 * @stats_id: stats type
 * @xmit_type: xmit type of packet - MLD/Link
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_txrx_update_vdev_host_stats(struct cdp_soc_t *soc_hdl,
						 uint8_t vdev_id,
						 void *buf,
						 uint16_t stats_id,
						 uint8_t xmit_type)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev) {
		dp_cdp_err("%pK: Invalid vdev handle", soc);
		return QDF_STATUS_E_FAILURE;
	}

	switch (stats_id) {
	case DP_VDEV_STATS_PKT_CNT_ONLY:
		break;
	case DP_VDEV_STATS_TX_ME:
		dp_txrx_update_vdev_me_stats(vdev, buf, xmit_type);
		dp_txrx_update_vdev_igmp_me_stats(vdev, buf, xmit_type);
		break;
	default:
		qdf_info("Invalid stats_id %d", stats_id);
		break;
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_txrx_get_peer_stats_wrapper() - will get cdp_peer_stats
 * @soc: soc handle
 * @peer_stats: destination buffer to copy to
 * @peer_info: peer info
 *
 * Return: status success/failure
 */
static QDF_STATUS
dp_txrx_get_peer_stats_wrapper(struct cdp_soc_t *soc,
			       struct cdp_peer_stats *peer_stats,
			       struct cdp_peer_info peer_info)
{
	struct dp_peer *peer = NULL;

	peer = dp_peer_hash_find_wrapper((struct dp_soc *)soc, &peer_info,
					 DP_MOD_ID_CDP);

	qdf_mem_zero(peer_stats, sizeof(struct cdp_peer_stats));

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	dp_get_peer_stats(peer, peer_stats);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_txrx_get_peer_stats() - will get cdp_peer_stats
 * @soc: soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address of DP_PEER handle
 * @peer_stats: destination buffer to copy to
 *
 * Return: status success/failure
 */
static QDF_STATUS
dp_txrx_get_peer_stats(struct cdp_soc_t *soc, uint8_t vdev_id,
		       uint8_t *peer_mac, struct cdp_peer_stats *peer_stats)
{
	struct cdp_peer_info peer_info = { 0 };

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac, false,
				 CDP_WILD_PEER_TYPE);

	return dp_txrx_get_peer_stats_wrapper(soc, peer_stats, peer_info);
}

/**
 * dp_txrx_get_peer_stats_based_on_peer_type() - get peer stats based on the
 * peer type
 * @soc: soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: mac of DP_PEER handle
 * @peer_stats: buffer to copy to
 * @peer_type: type of peer
 *
 * Return: status success/failure
 */
static QDF_STATUS
dp_txrx_get_peer_stats_based_on_peer_type(struct cdp_soc_t *soc, uint8_t vdev_id,
					  uint8_t *peer_mac,
					  struct cdp_peer_stats *peer_stats,
					  enum cdp_peer_type peer_type)
{
	struct cdp_peer_info peer_info = { 0 };

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac, false,
				 peer_type);

	return dp_txrx_get_peer_stats_wrapper(soc, peer_stats, peer_info);
}

#if defined WLAN_FEATURE_11BE_MLO && defined DP_MLO_LINK_STATS_SUPPORT
/**
 * dp_get_per_link_peer_stats() - Get per link stats
 * @peer: DP peer
 * @peer_stats: buffer to copy to
 * @peer_type: Peer type
 * @num_link: Number of ML links
 *
 * Return: status success/failure
 */
QDF_STATUS dp_get_per_link_peer_stats(struct dp_peer *peer,
				      struct cdp_peer_stats *peer_stats,
				      enum cdp_peer_type peer_type,
				      uint8_t num_link)
{
	uint8_t i, min_num_links;
	struct dp_peer *link_peer;
	struct dp_mld_link_peers link_peers_info;
	struct dp_soc *soc = peer->vdev->pdev->soc;

	dp_get_peer_calibr_stats(peer, peer_stats);
	dp_get_peer_basic_stats(peer, peer_stats);
	dp_get_peer_tx_per(peer_stats);

	if (IS_MLO_DP_MLD_PEER(peer)) {
		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_GENERIC_STATS);
		if (link_peers_info.num_links > num_link)
			dp_info("Req stats of %d link. less than total link %d",
				num_link, link_peers_info.num_links);

		min_num_links = num_link < link_peers_info.num_links ?
				num_link : link_peers_info.num_links;
		for (i = 0; i < min_num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			if (qdf_unlikely(!link_peer))
				continue;
			dp_get_peer_per_pkt_stats(link_peer, peer_stats);
			dp_get_peer_extd_stats(link_peer, peer_stats);
		}
		dp_release_link_peers_ref(&link_peers_info,
					  DP_MOD_ID_GENERIC_STATS);
	} else {
		dp_get_peer_per_pkt_stats(peer, peer_stats);
		dp_get_peer_extd_stats(peer, peer_stats);
	}
	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS dp_get_per_link_peer_stats(struct dp_peer *peer,
				      struct cdp_peer_stats *peer_stats,
				      enum cdp_peer_type peer_type,
				      uint8_t num_link)
{
	dp_err("Per link stats not supported");
	return QDF_STATUS_E_INVAL;
}
#endif

/**
 * dp_txrx_get_per_link_peer_stats() - Get per link peer stats
 * @soc: soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: peer mac address
 * @peer_stats: buffer to copy to
 * @peer_type: Peer type
 * @num_link: Number of ML links
 *
 * NOTE: For peer_type = CDP_MLD_PEER_TYPE peer_stats should point to
 *       buffer of size = (sizeof(*peer_stats) * num_link)
 *
 * Return: status success/failure
 */
static QDF_STATUS
dp_txrx_get_per_link_peer_stats(struct cdp_soc_t *soc, uint8_t vdev_id,
				uint8_t *peer_mac,
				struct cdp_peer_stats *peer_stats,
				enum cdp_peer_type peer_type, uint8_t num_link)
{
	QDF_STATUS status;
	struct dp_peer *peer = NULL;
	struct cdp_peer_info peer_info = { 0 };

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac, false,
				 peer_type);

	peer = dp_peer_hash_find_wrapper((struct dp_soc *)soc, &peer_info,
					 DP_MOD_ID_GENERIC_STATS);
	if (!peer)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_zero(peer_stats, sizeof(struct cdp_peer_stats));

	status = dp_get_per_link_peer_stats(peer, peer_stats, peer_type,
					    num_link);

	dp_peer_unref_delete(peer, DP_MOD_ID_GENERIC_STATS);

	return status;
}

/**
 * dp_txrx_get_peer_stats_param() - will return specified cdp_peer_stats
 * @soc: soc handle
 * @vdev_id: vdev_id of vdev object
 * @peer_mac: mac address of the peer
 * @type: enum of required stats
 * @buf: buffer to hold the value
 *
 * Return: status success/failure
 */
static QDF_STATUS
dp_txrx_get_peer_stats_param(struct cdp_soc_t *soc, uint8_t vdev_id,
			     uint8_t *peer_mac, enum cdp_peer_stats_type type,
			     cdp_peer_stats_param_t *buf)
{
	QDF_STATUS ret;
	struct dp_peer *peer = NULL;
	struct cdp_peer_info peer_info = { 0 };

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac, false,
				 CDP_WILD_PEER_TYPE);

	peer = dp_peer_hash_find_wrapper((struct dp_soc *)soc, &peer_info,
				         DP_MOD_ID_CDP);

	if (!peer) {
		dp_peer_err("%pK: Invalid Peer for Mac " QDF_MAC_ADDR_FMT,
			    soc, QDF_MAC_ADDR_REF(peer_mac));
		return QDF_STATUS_E_FAILURE;
	}

	if (type >= cdp_peer_per_pkt_stats_min &&
	    type < cdp_peer_per_pkt_stats_max) {
		ret = dp_txrx_get_peer_per_pkt_stats_param(peer, type, buf);
	} else if (type >= cdp_peer_extd_stats_min &&
		   type < cdp_peer_extd_stats_max) {
		ret = dp_txrx_get_peer_extd_stats_param(peer, type, buf);
	} else {
		dp_err("%pK: Invalid stat type requested", soc);
		ret = QDF_STATUS_E_FAILURE;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return ret;
}

/**
 * dp_txrx_reset_peer_stats() - reset cdp_peer_stats for particular peer
 * @soc_hdl: soc handle
 * @vdev_id: id of vdev handle
 * @peer_mac: mac of DP_PEER handle
 *
 * Return: QDF_STATUS
 */
#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS
dp_txrx_reset_peer_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			 uint8_t *peer_mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_peer *peer =
			dp_peer_get_tgt_peer_hash_find(soc, peer_mac, 0,
						       vdev_id, DP_MOD_ID_CDP);

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	DP_STATS_CLR(peer);
	dp_txrx_peer_stats_clr(peer->txrx_peer);

	if (IS_MLO_DP_MLD_PEER(peer)) {
		uint8_t i;
		struct dp_peer *link_peer;
		struct dp_soc *link_peer_soc;
		struct dp_mld_link_peers link_peers_info;

		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_CDP);
		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			link_peer_soc = link_peer->vdev->pdev->soc;

			DP_STATS_CLR(link_peer);
			dp_monitor_peer_reset_stats(link_peer_soc, link_peer);
		}

		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
	} else {
		dp_monitor_peer_reset_stats(soc, peer);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}
#else
static QDF_STATUS
dp_txrx_reset_peer_stats(struct cdp_soc_t *soc, uint8_t vdev_id,
			 uint8_t *peer_mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)soc,
						      peer_mac, 0, vdev_id,
						      DP_MOD_ID_CDP);

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	DP_STATS_CLR(peer);
	dp_txrx_peer_stats_clr(peer->txrx_peer);
	dp_monitor_peer_reset_stats((struct dp_soc *)soc, peer);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}
#endif

/**
 * dp_txrx_get_vdev_stats() - Update buffer with cdp_vdev_stats
 * @soc_hdl: CDP SoC handle
 * @vdev_id: vdev Id
 * @buf: buffer for vdev stats
 * @is_aggregate: are aggregate stats being collected
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_txrx_get_vdev_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		       void *buf, bool is_aggregate)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct cdp_vdev_stats *vdev_stats;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_RESOURCES;

	vdev_stats = (struct cdp_vdev_stats *)buf;

	if (is_aggregate) {
		dp_aggregate_vdev_stats(vdev, buf, DP_XMIT_LINK);
	} else {
		dp_copy_vdev_stats_to_tgt_buf(vdev_stats,
						    &vdev->stats, DP_XMIT_LINK);
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_get_total_per() - get total per
 * @soc: DP soc handle
 * @pdev_id: id of DP_PDEV handle
 *
 * Return: % error rate using retries per packet and success packets
 */
static int dp_get_total_per(struct cdp_soc_t *soc, uint8_t pdev_id)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (!pdev)
		return 0;

	dp_aggregate_pdev_stats(pdev);
	if ((pdev->stats.tx.tx_success.num + pdev->stats.tx.retries) == 0)
		return 0;
	return qdf_do_div((pdev->stats.tx.retries * 100),
		((pdev->stats.tx.tx_success.num) + (pdev->stats.tx.retries)));
}

/**
 * dp_txrx_stats_publish() - publish pdev stats into a buffer
 * @soc: DP soc handle
 * @pdev_id: id of DP_PDEV handle
 * @buf: to hold pdev_stats
 *
 * Return: int
 */
static int
dp_txrx_stats_publish(struct cdp_soc_t *soc, uint8_t pdev_id,
		      struct cdp_stats_extd *buf)
{
	struct cdp_txrx_stats_req req = {0,};
	QDF_STATUS status;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (!pdev)
		return TXRX_STATS_LEVEL_OFF;

	if (pdev->pending_fw_stats_response) {
		dp_warn("pdev%d: prev req pending\n", pdev->pdev_id);
		return TXRX_STATS_LEVEL_OFF;
	}

	dp_aggregate_pdev_stats(pdev);

	pdev->pending_fw_stats_response = true;
	req.stats = (enum cdp_stats)HTT_DBG_EXT_STATS_PDEV_TX;
	req.cookie_val = DBG_STATS_COOKIE_DP_STATS;
	pdev->fw_stats_tlv_bitmap_rcvd = 0;
	qdf_event_reset(&pdev->fw_stats_event);
	status = dp_h2t_ext_stats_msg_send(pdev, req.stats, req.param0,
					   req.param1, req.param2, req.param3, 0,
					   req.cookie_val, 0);

	if (status != QDF_STATUS_SUCCESS) {
		dp_warn("pdev%d: tx stats req failed\n", pdev->pdev_id);
		pdev->pending_fw_stats_response = false;
		return TXRX_STATS_LEVEL_OFF;
	}

	req.stats = (enum cdp_stats)HTT_DBG_EXT_STATS_PDEV_RX;
	req.cookie_val = DBG_STATS_COOKIE_DP_STATS;
	status = dp_h2t_ext_stats_msg_send(pdev, req.stats, req.param0,
					   req.param1, req.param2, req.param3, 0,
					   req.cookie_val, 0);
	if (status != QDF_STATUS_SUCCESS) {
		dp_warn("pdev%d: rx stats req failed\n", pdev->pdev_id);
		pdev->pending_fw_stats_response = false;
		return TXRX_STATS_LEVEL_OFF;
	}

	/* The event may have already been signaled. Wait only if it's pending */
	if (!pdev->fw_stats_event.done) {
		status =
			qdf_wait_single_event(&pdev->fw_stats_event,
					      DP_MAX_SLEEP_TIME);

		if (status != QDF_STATUS_SUCCESS) {
			if (status == QDF_STATUS_E_TIMEOUT)
				dp_warn("pdev%d: fw stats timeout. TLVs rcvd 0x%llx\n",
					     pdev->pdev_id,
					     pdev->fw_stats_tlv_bitmap_rcvd);
			pdev->pending_fw_stats_response = false;
			return TXRX_STATS_LEVEL_OFF;
		}
	}

	qdf_mem_copy(buf, &pdev->stats, sizeof(struct cdp_pdev_stats));
	pdev->pending_fw_stats_response = false;

	return TXRX_STATS_LEVEL;
}

/**
 * dp_get_obss_stats() - Get Pdev OBSS stats from Fw
 * @soc: DP soc handle
 * @pdev_id: id of DP_PDEV handle
 * @buf: to hold pdev obss stats
 * @req: Pointer to CDP TxRx stats
 *
 * Return: status
 */
static QDF_STATUS
dp_get_obss_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
		  struct cdp_pdev_obss_pd_stats_tlv *buf,
		  struct cdp_txrx_stats_req *req)
{
	QDF_STATUS status;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	if (pdev->pending_fw_obss_stats_response)
		return QDF_STATUS_E_AGAIN;

	pdev->pending_fw_obss_stats_response = true;
	req->stats = (enum cdp_stats)HTT_DBG_EXT_STATS_PDEV_OBSS_PD_STATS;
	req->cookie_val = DBG_STATS_COOKIE_HTT_OBSS;
	qdf_event_reset(&pdev->fw_obss_stats_event);
	status = dp_h2t_ext_stats_msg_send(pdev, req->stats, req->param0,
					   req->param1, req->param2,
					   req->param3, 0, req->cookie_val,
					   req->mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		pdev->pending_fw_obss_stats_response = false;
		return status;
	}
	status =
		qdf_wait_single_event(&pdev->fw_obss_stats_event,
				      DP_MAX_SLEEP_TIME);

	if (status != QDF_STATUS_SUCCESS) {
		if (status == QDF_STATUS_E_TIMEOUT)
			qdf_debug("TIMEOUT_OCCURS");
		pdev->pending_fw_obss_stats_response = false;
		return QDF_STATUS_E_TIMEOUT;
	}
	qdf_mem_copy(buf, &pdev->stats.htt_tx_pdev_stats.obss_pd_stats_tlv,
		     sizeof(struct cdp_pdev_obss_pd_stats_tlv));
	pdev->pending_fw_obss_stats_response = false;
	return status;
}

/**
 * dp_clear_pdev_obss_pd_stats() - Clear pdev obss stats
 * @soc: DP soc handle
 * @pdev_id: id of DP_PDEV handle
 * @req: Pointer to CDP TxRx stats request mac_id will be
 *	 pre-filled and should not be overwritten
 *
 * Return: status
 */
static QDF_STATUS
dp_clear_pdev_obss_pd_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
			    struct cdp_txrx_stats_req *req)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	uint32_t cookie_val = DBG_STATS_COOKIE_DEFAULT;

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	/*
	 * For HTT_DBG_EXT_STATS_RESET command, FW need to config
	 * from param0 to param3 according to below rule:
	 *
	 * PARAM:
	 *   - config_param0 : start_offset (stats type)
	 *   - config_param1 : stats bmask from start offset
	 *   - config_param2 : stats bmask from start offset + 32
	 *   - config_param3 : stats bmask from start offset + 64
	 */
	req->stats = (enum cdp_stats)HTT_DBG_EXT_STATS_RESET;
	req->param0 = HTT_DBG_EXT_STATS_PDEV_OBSS_PD_STATS;
	req->param1 = 0x00000001;

	return dp_h2t_ext_stats_msg_send(pdev, req->stats, req->param0,
				  req->param1, req->param2, req->param3, 0,
				cookie_val, req->mac_id);
}

/**
 * dp_set_pdev_dscp_tid_map_wifi3() - update dscp tid map in pdev
 * @soc_handle: soc handle
 * @pdev_id: id of DP_PDEV handle
 * @map_id: ID of map that needs to be updated
 * @tos: index value in map
 * @tid: tid value passed by the user
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_set_pdev_dscp_tid_map_wifi3(struct cdp_soc_t *soc_handle,
			       uint8_t pdev_id,
			       uint8_t map_id,
			       uint8_t tos, uint8_t tid)
{
	uint8_t dscp;
	struct dp_soc *soc = (struct dp_soc *)soc_handle;
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	dscp = (tos >> DP_IP_DSCP_SHIFT) & DP_IP_DSCP_MASK;
	pdev->dscp_tid_map[map_id][dscp] = tid;

	if (map_id < soc->num_hw_dscp_tid_map)
		hal_tx_update_dscp_tid(soc->hal_soc, tid,
				       map_id, dscp);
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SYSFS_DP_STATS
/**
 * dp_sysfs_event_trigger() - Trigger event to wait for firmware
 * stats request response.
 * @soc: soc handle
 * @cookie_val: cookie value
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_sysfs_event_trigger(struct dp_soc *soc, uint32_t cookie_val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	/* wait for firmware response for sysfs stats request */
	if (cookie_val == DBG_SYSFS_STATS_COOKIE) {
		if (!soc) {
			dp_cdp_err("soc is NULL");
			return QDF_STATUS_E_FAILURE;
		}
		/* wait for event completion */
		status = qdf_wait_single_event(&soc->sysfs_config->sysfs_txrx_fw_request_done,
					       WLAN_SYSFS_STAT_REQ_WAIT_MS);
		if (status == QDF_STATUS_SUCCESS)
			dp_cdp_info("sysfs_txrx_fw_request_done event completed");
		else if (status == QDF_STATUS_E_TIMEOUT)
			dp_cdp_warn("sysfs_txrx_fw_request_done event expired");
		else
			dp_cdp_warn("sysfs_txrx_fw_request_done event error code %d", status);
	}

	return status;
}
#else /* WLAN_SYSFS_DP_STATS */
static QDF_STATUS
dp_sysfs_event_trigger(struct dp_soc *soc, uint32_t cookie_val)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_SYSFS_DP_STATS */

/**
 * dp_fw_stats_process() - Process TXRX FW stats request.
 * @vdev: DP VDEV handle
 * @req: stats request
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_fw_stats_process(struct dp_vdev *vdev,
		    struct cdp_txrx_stats_req *req)
{
	struct dp_pdev *pdev = NULL;
	struct dp_soc *soc = NULL;
	uint32_t stats = req->stats;
	uint8_t mac_id = req->mac_id;
	uint32_t cookie_val = DBG_STATS_COOKIE_DEFAULT;

	if (!vdev) {
		DP_TRACE(NONE, "VDEV not found");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = vdev->pdev;
	if (!pdev) {
		DP_TRACE(NONE, "PDEV not found");
		return QDF_STATUS_E_FAILURE;
	}

	soc = pdev->soc;
	if (!soc) {
		DP_TRACE(NONE, "soc not found");
		return QDF_STATUS_E_FAILURE;
	}

	/* In case request is from host sysfs for displaying stats on console */
	if (req->cookie_val == DBG_SYSFS_STATS_COOKIE)
		cookie_val = DBG_SYSFS_STATS_COOKIE;

	/*
	 * For HTT_DBG_EXT_STATS_RESET command, FW need to config
	 * from param0 to param3 according to below rule:
	 *
	 * PARAM:
	 *   - config_param0 : start_offset (stats type)
	 *   - config_param1 : stats bmask from start offset
	 *   - config_param2 : stats bmask from start offset + 32
	 *   - config_param3 : stats bmask from start offset + 64
	 */
	if (req->stats == CDP_TXRX_STATS_0) {
		req->param0 = HTT_DBG_EXT_STATS_PDEV_TX;
		req->param1 = 0xFFFFFFFF;
		req->param2 = 0xFFFFFFFF;
		req->param3 = 0xFFFFFFFF;
	} else if (req->stats == (uint8_t)HTT_DBG_EXT_STATS_PDEV_TX_MU) {
		req->param0 = HTT_DBG_EXT_STATS_SET_VDEV_MASK(vdev->vdev_id);
	}

	if (req->stats == (uint8_t)HTT_DBG_EXT_STATS_PDEV_RX_RATE_EXT) {
		dp_h2t_ext_stats_msg_send(pdev,
					  HTT_DBG_EXT_STATS_PDEV_RX_RATE_EXT,
					  req->param0, req->param1, req->param2,
					  req->param3, 0, cookie_val,
					  mac_id);
	} else {
		dp_h2t_ext_stats_msg_send(pdev, stats, req->param0,
					  req->param1, req->param2, req->param3,
					  0, cookie_val, mac_id);
	}

	dp_sysfs_event_trigger(soc, cookie_val);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_txrx_stats_request - function to map to firmware and host stats
 * @soc_handle: soc handle
 * @vdev_id: virtual device ID
 * @req: stats request
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS dp_txrx_stats_request(struct cdp_soc_t *soc_handle,
				 uint8_t vdev_id,
				 struct cdp_txrx_stats_req *req)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_handle);
	int host_stats;
	int fw_stats;
	enum cdp_stats stats;
	int num_stats;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (!vdev || !req) {
		dp_cdp_err("%pK: Invalid vdev/req instance", soc);
		status = QDF_STATUS_E_INVAL;
		goto fail0;
	}

	if (req->mac_id >= WLAN_CFG_MAC_PER_TARGET) {
		dp_err("Invalid mac_id: %u request", req->mac_id);
		status = QDF_STATUS_E_INVAL;
		goto fail0;
	}

	stats = req->stats;
	if (stats >= CDP_TXRX_MAX_STATS) {
		status = QDF_STATUS_E_INVAL;
		goto fail0;
	}

	/*
	 * DP_CURR_FW_STATS_AVAIL: no of FW stats currently available
	 *			has to be updated if new FW HTT stats added
	 */
	if (stats > CDP_TXRX_STATS_HTT_MAX)
		stats = stats + DP_CURR_FW_STATS_AVAIL - DP_HTT_DBG_EXT_STATS_MAX;

	num_stats  = QDF_ARRAY_SIZE(dp_stats_mapping_table);

	if (stats >= num_stats) {
		dp_cdp_err("%pK : Invalid stats option: %d", soc, stats);
		status = QDF_STATUS_E_INVAL;
		goto fail0;
	}

	req->stats = stats;
	fw_stats = dp_stats_mapping_table[stats][STATS_FW];
	host_stats = dp_stats_mapping_table[stats][STATS_HOST];

	dp_info("stats: %u fw_stats_type: %d host_stats: %d",
		stats, fw_stats, host_stats);

	if (fw_stats != TXRX_FW_STATS_INVALID) {
		/* update request with FW stats type */
		req->stats = fw_stats;
		status = dp_fw_stats_process(vdev, req);
	} else if ((host_stats != TXRX_HOST_STATS_INVALID) &&
			(host_stats <= TXRX_HOST_STATS_MAX))
		status = dp_print_host_stats(vdev, req, soc);
	else
		dp_cdp_info("%pK: Wrong Input for TxRx Stats", soc);
fail0:
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return status;
}

/**
 * dp_soc_notify_asserted_soc() - API to notify asserted soc info
 * @psoc: CDP soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_soc_notify_asserted_soc(struct cdp_soc_t *psoc)
{
	struct dp_soc *soc = (struct dp_soc *)psoc;

	if (!soc) {
		dp_cdp_err("%pK: soc is NULL", soc);
		return QDF_STATUS_E_INVAL;
	}

	return dp_umac_reset_notify_asserted_soc(soc);
}

/**
 * dp_txrx_dump_stats() -  Dump statistics
 * @psoc: CDP soc handle
 * @value: Statistics option
 * @level: verbosity level
 */
static QDF_STATUS dp_txrx_dump_stats(struct cdp_soc_t *psoc, uint16_t value,
				     enum qdf_stats_verbosity_level level)
{
	struct dp_soc *soc =
		(struct dp_soc *)psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!soc) {
		dp_cdp_err("%pK: soc is NULL", soc);
		return QDF_STATUS_E_INVAL;
	}

	switch (value) {
	case CDP_TXRX_PATH_STATS:
		dp_txrx_path_stats(soc);
		dp_print_soc_interrupt_stats(soc);
		dp_print_reg_write_stats(soc);
		dp_pdev_print_tx_delay_stats(soc);
		/* Dump usage watermark stats for core TX/RX SRNGs */
		dp_dump_srng_high_wm_stats(soc,
					   DP_SRNG_WM_MASK_REO_DST |
					   DP_SRNG_WM_MASK_TX_COMP);
		if (soc->cdp_soc.ol_ops->dp_print_fisa_stats)
			soc->cdp_soc.ol_ops->dp_print_fisa_stats(
						CDP_FISA_STATS_ID_ERR_STATS);
		break;

	case CDP_RX_RING_STATS:
		dp_print_per_ring_stats(soc);
		break;

	case CDP_TXRX_TSO_STATS:
		dp_print_tso_stats(soc, level);
		break;

	case CDP_DUMP_TX_FLOW_POOL_INFO:
		if (level == QDF_STATS_VERBOSITY_LEVEL_HIGH)
			cdp_dump_flow_pool_info((struct cdp_soc_t *)soc);
		else
			dp_tx_dump_flow_pool_info_compact(soc);
		break;

	case CDP_DP_NAPI_STATS:
		dp_print_napi_stats(soc);
		break;

	case CDP_TXRX_DESC_STATS:
		/* TODO: NOT IMPLEMENTED */
		break;

	case CDP_DP_RX_FISA_STATS:
		if (soc->cdp_soc.ol_ops->dp_print_fisa_stats)
			soc->cdp_soc.ol_ops->dp_print_fisa_stats(
						CDP_FISA_STATS_ID_DUMP_SW_FST);
		break;

	case CDP_DP_SWLM_STATS:
		dp_print_swlm_stats(soc);
		break;

	case CDP_DP_TX_HW_LATENCY_STATS:
		dp_pdev_print_tx_delay_stats(soc);
		break;

	default:
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;

}

#ifdef WLAN_SYSFS_DP_STATS
static
void dp_sysfs_get_stat_type(struct dp_soc *soc, uint32_t *mac_id,
			    uint32_t *stat_type)
{
	qdf_spinlock_acquire(&soc->sysfs_config->rw_stats_lock);
	*stat_type = soc->sysfs_config->stat_type_requested;
	*mac_id   = soc->sysfs_config->mac_id;

	qdf_spinlock_release(&soc->sysfs_config->rw_stats_lock);
}

static
void dp_sysfs_update_config_buf_params(struct dp_soc *soc,
				       uint32_t curr_len,
				       uint32_t max_buf_len,
				       char *buf)
{
	qdf_spinlock_acquire(&soc->sysfs_config->sysfs_write_user_buffer);
	/* set sysfs_config parameters */
	soc->sysfs_config->buf = buf;
	soc->sysfs_config->curr_buffer_length = curr_len;
	soc->sysfs_config->max_buffer_length = max_buf_len;
	qdf_spinlock_release(&soc->sysfs_config->sysfs_write_user_buffer);
}

static
QDF_STATUS dp_sysfs_fill_stats(ol_txrx_soc_handle soc_hdl,
			       char *buf, uint32_t buf_size)
{
	uint32_t mac_id = 0;
	uint32_t stat_type = 0;
	uint32_t fw_stats = 0;
	uint32_t host_stats = 0;
	enum cdp_stats stats;
	struct cdp_txrx_stats_req req;
	uint32_t num_stats;
	struct dp_soc *soc = NULL;

	if (!soc_hdl) {
		dp_cdp_err("%pK: soc_hdl is NULL", soc_hdl);
		return QDF_STATUS_E_INVAL;
	}

	soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!soc) {
		dp_cdp_err("%pK: soc is NULL", soc);
		return QDF_STATUS_E_INVAL;
	}

	dp_sysfs_get_stat_type(soc, &mac_id, &stat_type);

	stats = stat_type;
	if (stats >= CDP_TXRX_MAX_STATS) {
		dp_cdp_info("sysfs stat type requested is invalid");
		return QDF_STATUS_E_INVAL;
	}
	/*
	 * DP_CURR_FW_STATS_AVAIL: no of FW stats currently available
	 *			has to be updated if new FW HTT stats added
	 */
	if (stats > CDP_TXRX_MAX_STATS)
		stats = stats + DP_CURR_FW_STATS_AVAIL - DP_HTT_DBG_EXT_STATS_MAX;

	num_stats = QDF_ARRAY_SIZE(dp_stats_mapping_table);

	if (stats >= num_stats) {
		dp_cdp_err("%pK : Invalid stats option: %d, max num stats: %d",
				soc, stats, num_stats);
		return QDF_STATUS_E_INVAL;
	}

	/* build request */
	fw_stats = dp_stats_mapping_table[stats][STATS_FW];
	host_stats = dp_stats_mapping_table[stats][STATS_HOST];

	req.stats = stat_type;
	req.mac_id = mac_id;
	/* request stats to be printed */
	qdf_mutex_acquire(&soc->sysfs_config->sysfs_read_lock);

	if (fw_stats != TXRX_FW_STATS_INVALID) {
		/* update request with FW stats type */
		req.cookie_val = DBG_SYSFS_STATS_COOKIE;
	} else if ((host_stats != TXRX_HOST_STATS_INVALID) &&
			(host_stats <= TXRX_HOST_STATS_MAX)) {
		req.cookie_val = DBG_STATS_COOKIE_DEFAULT;
		soc->sysfs_config->process_id = qdf_get_current_pid();
		soc->sysfs_config->printing_mode = PRINTING_MODE_ENABLED;
	}

	dp_sysfs_update_config_buf_params(soc, 0, buf_size, buf);

	dp_txrx_stats_request(soc_hdl, mac_id, &req);
	soc->sysfs_config->process_id = 0;
	soc->sysfs_config->printing_mode = PRINTING_MODE_DISABLED;

	dp_sysfs_update_config_buf_params(soc, 0, 0, NULL);

	qdf_mutex_release(&soc->sysfs_config->sysfs_read_lock);
	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_sysfs_set_stat_type(ol_txrx_soc_handle soc_hdl,
				  uint32_t stat_type, uint32_t mac_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!soc_hdl) {
		dp_cdp_err("%pK: soc is NULL", soc);
		return QDF_STATUS_E_INVAL;
	}

	qdf_spinlock_acquire(&soc->sysfs_config->rw_stats_lock);

	soc->sysfs_config->stat_type_requested = stat_type;
	soc->sysfs_config->mac_id = mac_id;

	qdf_spinlock_release(&soc->sysfs_config->rw_stats_lock);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_sysfs_initialize_stats(struct dp_soc *soc_hdl)
{
	struct dp_soc *soc;
	QDF_STATUS status;

	if (!soc_hdl) {
		dp_cdp_err("%pK: soc_hdl is NULL", soc_hdl);
		return QDF_STATUS_E_INVAL;
	}

	soc = soc_hdl;

	soc->sysfs_config = qdf_mem_malloc(sizeof(struct sysfs_stats_config));
	if (!soc->sysfs_config) {
		dp_cdp_err("failed to allocate memory for sysfs_config no memory");
		return QDF_STATUS_E_NOMEM;
	}

	status = qdf_event_create(&soc->sysfs_config->sysfs_txrx_fw_request_done);
	/* create event for fw stats request from sysfs */
	if (status != QDF_STATUS_SUCCESS) {
		dp_cdp_err("failed to create event sysfs_txrx_fw_request_done");
		qdf_mem_free(soc->sysfs_config);
		soc->sysfs_config = NULL;
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spinlock_create(&soc->sysfs_config->rw_stats_lock);
	qdf_mutex_create(&soc->sysfs_config->sysfs_read_lock);
	qdf_spinlock_create(&soc->sysfs_config->sysfs_write_user_buffer);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_sysfs_deinitialize_stats(struct dp_soc *soc_hdl)
{
	struct dp_soc *soc;
	QDF_STATUS status;

	if (!soc_hdl) {
		dp_cdp_err("%pK: soc_hdl is NULL", soc_hdl);
		return QDF_STATUS_E_INVAL;
	}

	soc = soc_hdl;
	if (!soc->sysfs_config) {
		dp_cdp_err("soc->sysfs_config is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_event_destroy(&soc->sysfs_config->sysfs_txrx_fw_request_done);
	if (status != QDF_STATUS_SUCCESS)
		dp_cdp_err("Failed to destroy event sysfs_txrx_fw_request_done");

	qdf_mutex_destroy(&soc->sysfs_config->sysfs_read_lock);
	qdf_spinlock_destroy(&soc->sysfs_config->rw_stats_lock);
	qdf_spinlock_destroy(&soc->sysfs_config->sysfs_write_user_buffer);

	qdf_mem_free(soc->sysfs_config);

	return QDF_STATUS_SUCCESS;
}

#else /* WLAN_SYSFS_DP_STATS */

static
QDF_STATUS dp_sysfs_deinitialize_stats(struct dp_soc *soc_hdl)
{
	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_sysfs_initialize_stats(struct dp_soc *soc_hdl)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_SYSFS_DP_STATS */

/**
 * dp_txrx_clear_dump_stats() - clear dumpStats
 * @soc_hdl: soc handle
 * @pdev_id: pdev ID
 * @value: stats option
 *
 * Return: 0 - Success, non-zero - failure
 */
static
QDF_STATUS dp_txrx_clear_dump_stats(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				    uint8_t value)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!soc) {
		dp_err("soc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	switch (value) {
	case CDP_TXRX_TSO_STATS:
		dp_txrx_clear_tso_stats(soc);
		break;

	case CDP_DP_TX_HW_LATENCY_STATS:
		dp_pdev_clear_tx_delay_stats(soc);
		break;

	default:
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

static QDF_STATUS
dp_txrx_get_interface_stats(struct cdp_soc_t *soc_hdl,
			    uint8_t vdev_id,
			    void *buf,
			    bool is_aggregate)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (soc && soc->arch_ops.dp_get_interface_stats)
		return soc->arch_ops.dp_get_interface_stats(soc_hdl,
							    vdev_id,
							    buf,
							    is_aggregate);
	return QDF_STATUS_E_FAILURE;
}

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * dp_update_flow_control_parameters() - API to store datapath
 *                            config parameters
 * @soc: soc handle
 * @params: ini parameter handle
 *
 * Return: void
 */
static inline
void dp_update_flow_control_parameters(struct dp_soc *soc,
				struct cdp_config_params *params)
{
	soc->wlan_cfg_ctx->tx_flow_stop_queue_threshold =
					params->tx_flow_stop_queue_threshold;
	soc->wlan_cfg_ctx->tx_flow_start_queue_offset =
					params->tx_flow_start_queue_offset;
}
#else
static inline
void dp_update_flow_control_parameters(struct dp_soc *soc,
				struct cdp_config_params *params)
{
}
#endif

#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
/* Max packet limit for TX Comp packet loop (dp_tx_comp_handler) */
#define DP_TX_COMP_LOOP_PKT_LIMIT_MAX 1024

/* Max packet limit for RX REAP Loop (dp_rx_process) */
#define DP_RX_REAP_LOOP_PKT_LIMIT_MAX 1024

static
void dp_update_rx_soft_irq_limit_params(struct dp_soc *soc,
					struct cdp_config_params *params)
{
	soc->wlan_cfg_ctx->tx_comp_loop_pkt_limit =
				params->tx_comp_loop_pkt_limit;

	if (params->tx_comp_loop_pkt_limit < DP_TX_COMP_LOOP_PKT_LIMIT_MAX)
		soc->wlan_cfg_ctx->tx_comp_enable_eol_data_check = true;
	else
		soc->wlan_cfg_ctx->tx_comp_enable_eol_data_check = false;

	soc->wlan_cfg_ctx->rx_reap_loop_pkt_limit =
				params->rx_reap_loop_pkt_limit;

	if (params->rx_reap_loop_pkt_limit < DP_RX_REAP_LOOP_PKT_LIMIT_MAX)
		soc->wlan_cfg_ctx->rx_enable_eol_data_check = true;
	else
		soc->wlan_cfg_ctx->rx_enable_eol_data_check = false;

	soc->wlan_cfg_ctx->rx_hp_oos_update_limit =
				params->rx_hp_oos_update_limit;

	dp_info("tx_comp_loop_pkt_limit %u tx_comp_enable_eol_data_check %u rx_reap_loop_pkt_limit %u rx_enable_eol_data_check %u rx_hp_oos_update_limit %u",
		soc->wlan_cfg_ctx->tx_comp_loop_pkt_limit,
		soc->wlan_cfg_ctx->tx_comp_enable_eol_data_check,
		soc->wlan_cfg_ctx->rx_reap_loop_pkt_limit,
		soc->wlan_cfg_ctx->rx_enable_eol_data_check,
		soc->wlan_cfg_ctx->rx_hp_oos_update_limit);
}

#else
static inline
void dp_update_rx_soft_irq_limit_params(struct dp_soc *soc,
					struct cdp_config_params *params)
{ }

#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */

/**
 * dp_update_config_parameters() - API to store datapath
 *                            config parameters
 * @psoc: soc handle
 * @params: ini parameter handle
 *
 * Return: status
 */
static
QDF_STATUS dp_update_config_parameters(struct cdp_soc *psoc,
				struct cdp_config_params *params)
{
	struct dp_soc *soc = (struct dp_soc *)psoc;

	if (!(soc)) {
		dp_cdp_err("%pK: Invalid handle", soc);
		return QDF_STATUS_E_INVAL;
	}

	soc->wlan_cfg_ctx->tso_enabled = params->tso_enable;
	soc->wlan_cfg_ctx->lro_enabled = params->lro_enable;
	soc->wlan_cfg_ctx->rx_hash = params->flow_steering_enable;
	soc->wlan_cfg_ctx->p2p_tcp_udp_checksumoffload =
				params->p2p_tcp_udp_checksumoffload;
	soc->wlan_cfg_ctx->nan_tcp_udp_checksumoffload =
				params->nan_tcp_udp_checksumoffload;
	soc->wlan_cfg_ctx->tcp_udp_checksumoffload =
				params->tcp_udp_checksumoffload;
	soc->wlan_cfg_ctx->napi_enabled = params->napi_enable;
	soc->wlan_cfg_ctx->ipa_enabled = params->ipa_enable;
	soc->wlan_cfg_ctx->gro_enabled = params->gro_enable;

	dp_update_rx_soft_irq_limit_params(soc, params);
	dp_update_flow_control_parameters(soc, params);

	return QDF_STATUS_SUCCESS;
}

static struct cdp_wds_ops dp_ops_wds = {
	.vdev_set_wds = dp_vdev_set_wds,
#ifdef WDS_VENDOR_EXTENSION
	.txrx_set_wds_rx_policy = dp_txrx_set_wds_rx_policy,
	.txrx_wds_peer_tx_policy_update = dp_txrx_peer_wds_tx_policy_update,
#endif
};

/**
 * dp_txrx_data_tx_cb_set() - set the callback for non standard tx
 * @soc_hdl: datapath soc handle
 * @vdev_id: virtual interface id
 * @callback: callback function
 * @ctxt: callback context
 *
 */
static void
dp_txrx_data_tx_cb_set(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		       ol_txrx_data_tx_cb callback, void *ctxt)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev)
		return;

	vdev->tx_non_std_data_callback.func = callback;
	vdev->tx_non_std_data_callback.ctxt = ctxt;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}

/**
 * dp_pdev_get_dp_txrx_handle() - get dp handle from pdev
 * @soc: datapath soc handle
 * @pdev_id: id of datapath pdev handle
 *
 * Return: opaque pointer to dp txrx handle
 */
static void *dp_pdev_get_dp_txrx_handle(struct cdp_soc_t *soc, uint8_t pdev_id)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	if (qdf_unlikely(!pdev))
		return NULL;

	return pdev->dp_txrx_handle;
}

/**
 * dp_pdev_set_dp_txrx_handle() - set dp handle in pdev
 * @soc: datapath soc handle
 * @pdev_id: id of datapath pdev handle
 * @dp_txrx_hdl: opaque pointer for dp_txrx_handle
 *
 * Return: void
 */
static void
dp_pdev_set_dp_txrx_handle(struct cdp_soc_t *soc, uint8_t pdev_id,
			   void *dp_txrx_hdl)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (!pdev)
		return;

	pdev->dp_txrx_handle = dp_txrx_hdl;
}

/**
 * dp_vdev_get_dp_ext_handle() - get dp handle from vdev
 * @soc_hdl: datapath soc handle
 * @vdev_id: vdev id
 *
 * Return: opaque pointer to dp txrx handle
 */
static void *dp_vdev_get_dp_ext_handle(ol_txrx_soc_handle soc_hdl,
				       uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	void *dp_ext_handle;

	if (!vdev)
		return NULL;
	dp_ext_handle = vdev->vdev_dp_ext_handle;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return dp_ext_handle;
}

/**
 * dp_vdev_set_dp_ext_handle() - set dp handle in vdev
 * @soc_hdl: datapath soc handle
 * @vdev_id: vdev id
 * @size: size of advance dp handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_vdev_set_dp_ext_handle(ol_txrx_soc_handle soc_hdl, uint8_t vdev_id,
			  uint16_t size)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	void *dp_ext_handle;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	dp_ext_handle = qdf_mem_malloc(size);

	if (!dp_ext_handle) {
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	vdev->vdev_dp_ext_handle = dp_ext_handle;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_vdev_inform_ll_conn() - Inform vdev to add/delete a latency critical
 *			      connection for this vdev
 * @soc_hdl: CDP soc handle
 * @vdev_id: vdev ID
 * @action: Add/Delete action
 *
 * Return: QDF_STATUS.
 */
static QDF_STATUS
dp_vdev_inform_ll_conn(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		       enum vdev_ll_conn_actions action)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev) {
		dp_err("LL connection action for invalid vdev %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	switch (action) {
	case CDP_VDEV_LL_CONN_ADD:
		vdev->num_latency_critical_conn++;
		break;

	case CDP_VDEV_LL_CONN_DEL:
		vdev->num_latency_critical_conn--;
		break;

	default:
		dp_err("LL connection action invalid %d", action);
		break;
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
/**
 * dp_soc_set_swlm_enable() - Enable/Disable SWLM if initialized.
 * @soc_hdl: CDP Soc handle
 * @value: Enable/Disable value
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_soc_set_swlm_enable(struct cdp_soc_t *soc_hdl,
					 uint8_t value)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!soc->swlm.is_init) {
		dp_err("SWLM is not initialized");
		return QDF_STATUS_E_FAILURE;
	}

	soc->swlm.is_enabled = !!value;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_soc_is_swlm_enabled() - Check if SWLM is enabled.
 * @soc_hdl: CDP Soc handle
 *
 * Return: QDF_STATUS
 */
static uint8_t dp_soc_is_swlm_enabled(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	return soc->swlm.is_enabled;
}
#endif

/**
 * dp_soc_get_dp_txrx_handle() - get context for external-dp from dp soc
 * @soc_handle: datapath soc handle
 *
 * Return: opaque pointer to external dp (non-core DP)
 */
static void *dp_soc_get_dp_txrx_handle(struct cdp_soc *soc_handle)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	return soc->external_txrx_handle;
}

/**
 * dp_soc_set_dp_txrx_handle() - set external dp handle in soc
 * @soc_handle: datapath soc handle
 * @txrx_handle: opaque pointer to external dp (non-core DP)
 *
 * Return: void
 */
static void
dp_soc_set_dp_txrx_handle(struct cdp_soc *soc_handle, void *txrx_handle)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	soc->external_txrx_handle = txrx_handle;
}

/**
 * dp_soc_map_pdev_to_lmac() - Save pdev_id to lmac_id mapping
 * @soc_hdl: datapath soc handle
 * @pdev_id: id of the datapath pdev handle
 * @lmac_id: lmac id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_soc_map_pdev_to_lmac
	(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	 uint32_t lmac_id)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	wlan_cfg_set_hw_mac_idx(soc->wlan_cfg_ctx,
				pdev_id,
				lmac_id);

	/*Set host PDEV ID for lmac_id*/
	wlan_cfg_set_pdev_idx(soc->wlan_cfg_ctx,
			      pdev_id,
			      lmac_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_soc_handle_pdev_mode_change() - Update pdev to lmac mapping
 * @soc_hdl: datapath soc handle
 * @pdev_id: id of the datapath pdev handle
 * @lmac_id: lmac id
 *
 * In the event of a dynamic mode change, update the pdev to lmac mapping
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_soc_handle_pdev_mode_change
	(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	 uint32_t lmac_id)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_vdev *vdev = NULL;
	uint8_t hw_pdev_id, mac_id;
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc,
								  pdev_id);
	int nss_config = wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx);

	if (qdf_unlikely(!pdev))
		return QDF_STATUS_E_FAILURE;

	pdev->lmac_id = lmac_id;
	pdev->target_pdev_id =
		dp_calculate_target_pdev_id_from_host_pdev_id(soc, pdev_id);
	dp_info("mode change %d %d", pdev->pdev_id, pdev->lmac_id);

	/*Set host PDEV ID for lmac_id*/
	wlan_cfg_set_pdev_idx(soc->wlan_cfg_ctx,
			      pdev->pdev_id,
			      lmac_id);

	hw_pdev_id =
		dp_get_target_pdev_id_for_host_pdev_id(soc,
						       pdev->pdev_id);

	/*
	 * When NSS offload is enabled, send pdev_id->lmac_id
	 * and pdev_id to hw_pdev_id to NSS FW
	 */
	if (nss_config) {
		mac_id = pdev->lmac_id;
		if (soc->cdp_soc.ol_ops->pdev_update_lmac_n_target_pdev_id)
			soc->cdp_soc.ol_ops->
				pdev_update_lmac_n_target_pdev_id(
				soc->ctrl_psoc,
				&pdev_id, &mac_id, &hw_pdev_id);
	}

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		DP_TX_TCL_METADATA_PDEV_ID_SET(vdev->htt_tcl_metadata,
					       hw_pdev_id);
		vdev->lmac_id = pdev->lmac_id;
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_soc_set_pdev_status_down() - set pdev down/up status
 * @soc: datapath soc handle
 * @pdev_id: id of datapath pdev handle
 * @is_pdev_down: pdev down/up status
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_soc_set_pdev_status_down(struct cdp_soc_t *soc, uint8_t pdev_id,
			    bool is_pdev_down)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	pdev->is_pdev_down = is_pdev_down;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_get_cfg_capabilities() - get dp capabilities
 * @soc_handle: datapath soc handle
 * @dp_caps: enum for dp capabilities
 *
 * Return: bool to determine if dp caps is enabled
 */
static bool
dp_get_cfg_capabilities(struct cdp_soc_t *soc_handle,
			enum cdp_capabilities dp_caps)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	return wlan_cfg_get_dp_caps(soc->wlan_cfg_ctx, dp_caps);
}

#ifdef FEATURE_AST
static QDF_STATUS
dp_peer_teardown_wifi3(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		       uint8_t *peer_mac)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_peer *peer =
			dp_peer_find_hash_find(soc, peer_mac, 0, vdev_id,
					       DP_MOD_ID_CDP);

	/* Peer can be null for monitor vap mac address */
	if (!peer) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid peer\n", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	dp_peer_update_state(soc, peer, DP_PEER_STATE_LOGICAL_DELETE);

	qdf_spin_lock_bh(&soc->ast_lock);
	dp_peer_send_wds_disconnect(soc, peer);
	dp_peer_delete_ast_entries(soc, peer);
	qdf_spin_unlock_bh(&soc->ast_lock);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return status;
}
#endif

#ifndef WLAN_SUPPORT_RX_TAG_STATISTICS
/**
 * dp_dump_pdev_rx_protocol_tag_stats - dump the number of packets tagged for
 * given protocol type (RX_PROTOCOL_TAG_ALL indicates for all protocol)
 * @soc: cdp_soc handle
 * @pdev_id: id of cdp_pdev handle
 * @protocol_type: protocol type for which stats should be displayed
 *
 * Return: none
 */
static inline void
dp_dump_pdev_rx_protocol_tag_stats(struct cdp_soc_t  *soc, uint8_t pdev_id,
				   uint16_t protocol_type)
{
}
#endif /* WLAN_SUPPORT_RX_TAG_STATISTICS */

#ifndef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
/**
 * dp_update_pdev_rx_protocol_tag() - Add/remove a protocol tag that should be
 * applied to the desired protocol type packets
 * @soc: soc handle
 * @pdev_id: id of cdp_pdev handle
 * @enable_rx_protocol_tag: bitmask that indicates what protocol types
 * are enabled for tagging. zero indicates disable feature, non-zero indicates
 * enable feature
 * @protocol_type: new protocol type for which the tag is being added
 * @tag: user configured tag for the new protocol
 *
 * Return: Success
 */
static inline QDF_STATUS
dp_update_pdev_rx_protocol_tag(struct cdp_soc_t  *soc, uint8_t pdev_id,
			       uint32_t enable_rx_protocol_tag,
			       uint16_t protocol_type,
			       uint16_t tag)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

#ifndef WLAN_SUPPORT_RX_FLOW_TAG
/**
 * dp_set_rx_flow_tag() - add/delete a flow
 * @cdp_soc: CDP soc handle
 * @pdev_id: id of cdp_pdev handle
 * @flow_info: flow tuple that is to be added to/deleted from flow search table
 *
 * Return: Success
 */
static inline QDF_STATUS
dp_set_rx_flow_tag(struct cdp_soc_t *cdp_soc, uint8_t pdev_id,
		   struct cdp_rx_flow_info *flow_info)
{
	return QDF_STATUS_SUCCESS;
}
/**
 * dp_dump_rx_flow_tag_stats() - dump the number of packets tagged for
 * given flow 5-tuple
 * @cdp_soc: soc handle
 * @pdev_id: id of cdp_pdev handle
 * @flow_info: flow 5-tuple for which stats should be displayed
 *
 * Return: Success
 */
static inline QDF_STATUS
dp_dump_rx_flow_tag_stats(struct cdp_soc_t *cdp_soc, uint8_t pdev_id,
			  struct cdp_rx_flow_info *flow_info)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */

static QDF_STATUS dp_peer_map_attach_wifi3(struct cdp_soc_t  *soc_hdl,
					   uint32_t max_peers,
					   uint32_t max_ast_index,
					   uint8_t peer_map_unmap_versions)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	QDF_STATUS status;

	soc->max_peers = max_peers;

	wlan_cfg_set_max_ast_idx(soc->wlan_cfg_ctx, max_ast_index);

	status = soc->arch_ops.txrx_peer_map_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("failure in allocating peer tables");
		return QDF_STATUS_E_FAILURE;
	}

	dp_info("max_peers %u, calculated max_peers %u max_ast_index: %u",
		max_peers, soc->max_peer_id, max_ast_index);

	status = dp_peer_find_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("Peer find attach failure");
		goto fail;
	}

	soc->peer_map_unmap_versions = peer_map_unmap_versions;
	soc->peer_map_attach_success = TRUE;

	return QDF_STATUS_SUCCESS;
fail:
	soc->arch_ops.txrx_peer_map_detach(soc);

	return status;
}

static QDF_STATUS dp_soc_set_param(struct cdp_soc_t  *soc_hdl,
				   enum cdp_soc_param_t param,
				   uint32_t value)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	switch (param) {
	case DP_SOC_PARAM_MSDU_EXCEPTION_DESC:
		soc->num_msdu_exception_desc = value;
		dp_info("num_msdu exception_desc %u",
			value);
		break;
	case DP_SOC_PARAM_CMEM_FSE_SUPPORT:
		if (wlan_cfg_is_fst_in_cmem_enabled(soc->wlan_cfg_ctx))
			soc->fst_in_cmem = !!value;
		dp_info("FW supports CMEM FSE %u", value);
		break;
	case DP_SOC_PARAM_MAX_AST_AGEOUT:
		soc->max_ast_ageout_count = value;
		dp_info("Max ast ageout count %u", soc->max_ast_ageout_count);
		break;
	case DP_SOC_PARAM_EAPOL_OVER_CONTROL_PORT:
		soc->eapol_over_control_port = value;
		dp_info("Eapol over control_port:%d",
			soc->eapol_over_control_port);
		break;
	case DP_SOC_PARAM_MULTI_PEER_GRP_CMD_SUPPORT:
		soc->multi_peer_grp_cmd_supported = value;
		dp_info("Multi Peer group command support:%d",
			soc->multi_peer_grp_cmd_supported);
		break;
	case DP_SOC_PARAM_RSSI_DBM_CONV_SUPPORT:
		soc->features.rssi_dbm_conv_support = value;
		dp_info("Rssi dbm conversion support:%u",
			soc->features.rssi_dbm_conv_support);
		break;
	case DP_SOC_PARAM_UMAC_HW_RESET_SUPPORT:
		soc->features.umac_hw_reset_support = value;
		dp_info("UMAC HW reset support :%u",
			soc->features.umac_hw_reset_support);
		break;
	case DP_SOC_PARAM_MULTI_RX_REORDER_SETUP_SUPPORT:
		soc->features.multi_rx_reorder_q_setup_support = value;
		dp_info("Multi rx reorder queue setup support: %u",
			soc->features.multi_rx_reorder_q_setup_support);
		break;
	default:
		dp_info("not handled param %d ", param);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

static void dp_soc_set_rate_stats_ctx(struct cdp_soc_t *soc_handle,
				      void *stats_ctx)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	soc->rate_stats_ctx = (struct cdp_soc_rate_stats_ctx *)stats_ctx;
}

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
/**
 * dp_peer_flush_rate_stats_req() - Flush peer rate stats
 * @soc: Datapath SOC handle
 * @peer: Datapath peer
 * @arg: argument to iter function
 *
 * Return: QDF_STATUS
 */
static void
dp_peer_flush_rate_stats_req(struct dp_soc *soc, struct dp_peer *peer,
			     void *arg)
{
	/* Skip self peer */
	if (!qdf_mem_cmp(peer->mac_addr.raw, peer->vdev->mac_addr.raw,
			 QDF_MAC_ADDR_SIZE))
		return;

	dp_wdi_event_handler(
		WDI_EVENT_FLUSH_RATE_STATS_REQ,
		soc, dp_monitor_peer_get_peerstats_ctx(soc, peer),
		peer->peer_id,
		WDI_NO_VAL, peer->vdev->pdev->pdev_id);
}

/**
 * dp_flush_rate_stats_req() - Flush peer rate stats in pdev
 * @soc_hdl: Datapath SOC handle
 * @pdev_id: pdev_id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_flush_rate_stats_req(struct cdp_soc_t *soc_hdl,
					  uint8_t pdev_id)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	dp_pdev_iterate_peer(pdev, dp_peer_flush_rate_stats_req, NULL,
			     DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_flush_rate_stats_req(struct cdp_soc_t *soc_hdl,
			uint8_t pdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_get_peer_extd_rate_link_stats() - function to get peer
 *				extended rate and link stats
 * @soc_hdl: dp soc handler
 * @mac_addr: mac address of peer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_get_peer_extd_rate_link_stats(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr)
{
	uint8_t i;
	struct dp_peer *link_peer;
	struct dp_soc *link_peer_soc;
	struct dp_mld_link_peers link_peers_info;
	struct dp_peer *peer = NULL;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct cdp_peer_info peer_info = { 0 };

	if (!mac_addr) {
		dp_err("NULL peer mac addr");
		return QDF_STATUS_E_FAILURE;
	}

	DP_PEER_INFO_PARAMS_INIT(&peer_info, DP_VDEV_ALL, mac_addr, false,
				 CDP_WILD_PEER_TYPE);

	peer = dp_peer_hash_find_wrapper(soc, &peer_info, DP_MOD_ID_CDP);
	if (!peer) {
		dp_err("Peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (IS_MLO_DP_MLD_PEER(peer)) {
		dp_get_link_peers_ref_from_mld_peer(soc, peer,
						    &link_peers_info,
						    DP_MOD_ID_CDP);
		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			link_peer_soc = link_peer->vdev->pdev->soc;
			dp_wdi_event_handler(WDI_EVENT_FLUSH_RATE_STATS_REQ,
					     link_peer_soc,
					     dp_monitor_peer_get_peerstats_ctx
					     (link_peer_soc, link_peer),
					     link_peer->peer_id,
					     WDI_NO_VAL,
					     link_peer->vdev->pdev->pdev_id);
		}
		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_CDP);
	} else {
		dp_wdi_event_handler(
				WDI_EVENT_FLUSH_RATE_STATS_REQ, soc,
				dp_monitor_peer_get_peerstats_ctx(soc, peer),
				peer->peer_id,
				WDI_NO_VAL, peer->vdev->pdev->pdev_id);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS
dp_get_peer_extd_rate_link_stats(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr)
{
	struct dp_peer *peer = NULL;
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	if (!mac_addr) {
		dp_err("NULL peer mac addr");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_find_hash_find(soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_err("Peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dp_wdi_event_handler(
			WDI_EVENT_FLUSH_RATE_STATS_REQ, soc,
			dp_monitor_peer_get_peerstats_ctx(soc, peer),
			peer->peer_id,
			WDI_NO_VAL, peer->vdev->pdev->pdev_id);

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}
#endif
#else
static inline QDF_STATUS
dp_get_peer_extd_rate_link_stats(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static void *dp_peer_get_peerstats_ctx(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *mac_addr)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_peer *peer;
	void *peerstats_ctx = NULL;

	if (mac_addr) {
		peer = dp_peer_find_hash_find(soc, mac_addr,
					      0, vdev_id,
					      DP_MOD_ID_CDP);
		if (!peer)
			return NULL;

		if (!IS_MLO_DP_MLD_PEER(peer))
			peerstats_ctx = dp_monitor_peer_get_peerstats_ctx(soc,
									  peer);

		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	}

	return peerstats_ctx;
}

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
static QDF_STATUS dp_peer_flush_rate_stats(struct cdp_soc_t *soc,
					   uint8_t pdev_id,
					   void *buf)
{
	 dp_wdi_event_handler(WDI_EVENT_PEER_FLUSH_RATE_STATS,
			      (struct dp_soc *)soc, buf, HTT_INVALID_PEER,
			      WDI_NO_VAL, pdev_id);
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_peer_flush_rate_stats(struct cdp_soc_t *soc,
			 uint8_t pdev_id,
			 void *buf)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static void *dp_soc_get_rate_stats_ctx(struct cdp_soc_t *soc_handle)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	return soc->rate_stats_ctx;
}

/**
 * dp_get_cfg() - get dp cfg
 * @soc: cdp soc handle
 * @cfg: cfg enum
 *
 * Return: cfg value
 */
static uint32_t dp_get_cfg(struct cdp_soc_t *soc, enum cdp_dp_cfg cfg)
{
	struct dp_soc *dpsoc = (struct dp_soc *)soc;
	uint32_t value = 0;

	switch (cfg) {
	case cfg_dp_enable_data_stall:
		value = dpsoc->wlan_cfg_ctx->enable_data_stall_detection;
		break;
	case cfg_dp_enable_p2p_ip_tcp_udp_checksum_offload:
		value = dpsoc->wlan_cfg_ctx->p2p_tcp_udp_checksumoffload;
		break;
	case cfg_dp_enable_nan_ip_tcp_udp_checksum_offload:
		value = dpsoc->wlan_cfg_ctx->nan_tcp_udp_checksumoffload;
		break;
	case cfg_dp_enable_ip_tcp_udp_checksum_offload:
		value = dpsoc->wlan_cfg_ctx->tcp_udp_checksumoffload;
		break;
	case cfg_dp_disable_legacy_mode_csum_offload:
		value = dpsoc->wlan_cfg_ctx->
					legacy_mode_checksumoffload_disable;
		break;
	case cfg_dp_tso_enable:
		value = dpsoc->wlan_cfg_ctx->tso_enabled;
		break;
	case cfg_dp_lro_enable:
		value = dpsoc->wlan_cfg_ctx->lro_enabled;
		break;
	case cfg_dp_gro_enable:
		value = dpsoc->wlan_cfg_ctx->gro_enabled;
		break;
	case cfg_dp_tc_based_dyn_gro_enable:
		value = dpsoc->wlan_cfg_ctx->tc_based_dynamic_gro;
		break;
	case cfg_dp_tc_ingress_prio:
		value = dpsoc->wlan_cfg_ctx->tc_ingress_prio;
		break;
	case cfg_dp_sg_enable:
		value = dpsoc->wlan_cfg_ctx->sg_enabled;
		break;
	case cfg_dp_tx_flow_start_queue_offset:
		value = dpsoc->wlan_cfg_ctx->tx_flow_start_queue_offset;
		break;
	case cfg_dp_tx_flow_stop_queue_threshold:
		value = dpsoc->wlan_cfg_ctx->tx_flow_stop_queue_threshold;
		break;
	case cfg_dp_disable_intra_bss_fwd:
		value = dpsoc->wlan_cfg_ctx->disable_intra_bss_fwd;
		break;
	case cfg_dp_pktlog_buffer_size:
		value = dpsoc->wlan_cfg_ctx->pktlog_buffer_size;
		break;
	case cfg_dp_wow_check_rx_pending:
		value = dpsoc->wlan_cfg_ctx->wow_check_rx_pending_enable;
		break;
	case cfg_dp_local_pkt_capture:
		value = wlan_cfg_get_local_pkt_capture(dpsoc->wlan_cfg_ctx);
		break;
	default:
		value =  0;
	}

	return value;
}

#ifdef PEER_FLOW_CONTROL
/**
 * dp_tx_flow_ctrl_configure_pdev() - Configure flow control params
 * @soc_handle: datapath soc handle
 * @pdev_id: id of datapath pdev handle
 * @param: ol ath params
 * @value: value of the flag
 * @buff: Buffer to be passed
 *
 * Implemented this function same as legacy function. In legacy code, single
 * function is used to display stats and update pdev params.
 *
 * Return: 0 for success. nonzero for failure.
 */
static uint32_t dp_tx_flow_ctrl_configure_pdev(struct cdp_soc_t *soc_handle,
					       uint8_t pdev_id,
					       enum _dp_param_t param,
					       uint32_t value, void *buff)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);

	if (qdf_unlikely(!pdev))
		return 1;

	soc = pdev->soc;
	if (!soc)
		return 1;

	switch (param) {
#ifdef QCA_ENH_V3_STATS_SUPPORT
	case DP_PARAM_VIDEO_DELAY_STATS_FC:
		if (value)
			pdev->delay_stats_flag = true;
		else
			pdev->delay_stats_flag = false;
		break;
	case DP_PARAM_VIDEO_STATS_FC:
		qdf_print("------- TID Stats ------\n");
		dp_pdev_print_tid_stats(pdev);
		qdf_print("------ Delay Stats ------\n");
		dp_pdev_print_delay_stats(pdev);
		qdf_print("------ Rx Error Stats ------\n");
		dp_pdev_print_rx_error_stats(pdev);
		break;
#endif
	case DP_PARAM_TOTAL_Q_SIZE:
		{
			uint32_t tx_min, tx_max;

			tx_min = wlan_cfg_get_min_tx_desc(soc->wlan_cfg_ctx);
			tx_max = wlan_cfg_get_num_tx_desc(soc->wlan_cfg_ctx);

			if (!buff) {
				if ((value >= tx_min) && (value <= tx_max)) {
					pdev->num_tx_allowed = value;
				} else {
					dp_tx_info("%pK: Failed to update num_tx_allowed, Q_min = %d Q_max = %d",
						   soc, tx_min, tx_max);
					break;
				}
			} else {
				*(int *)buff = pdev->num_tx_allowed;
			}
		}
		break;
	default:
		dp_tx_info("%pK: not handled param %d ", soc, param);
		break;
	}

	return 0;
}
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_reset_interrupt_ring_masks() - Reset rx interrupt masks
 * @soc: dp soc handle
 *
 * Return: void
 */
static void dp_reset_interrupt_ring_masks(struct dp_soc *soc)
{
	struct dp_intr_bkp *intr_bkp;
	struct dp_intr *intr_ctx;
	int num_ctxt = wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx);
	int i;

	intr_bkp =
	(struct dp_intr_bkp *)qdf_mem_malloc_atomic(sizeof(struct dp_intr_bkp) *
			num_ctxt);

	qdf_assert_always(intr_bkp);

	soc->umac_reset_ctx.intr_ctx_bkp = intr_bkp;
	for (i = 0; i < num_ctxt; i++) {
		intr_ctx = &soc->intr_ctx[i];

		intr_bkp->tx_ring_mask = intr_ctx->tx_ring_mask;
		intr_bkp->rx_ring_mask = intr_ctx->rx_ring_mask;
		intr_bkp->rx_mon_ring_mask = intr_ctx->rx_mon_ring_mask;
		intr_bkp->rx_err_ring_mask = intr_ctx->rx_err_ring_mask;
		intr_bkp->rx_wbm_rel_ring_mask = intr_ctx->rx_wbm_rel_ring_mask;
		intr_bkp->reo_status_ring_mask = intr_ctx->reo_status_ring_mask;
		intr_bkp->rxdma2host_ring_mask = intr_ctx->rxdma2host_ring_mask;
		intr_bkp->host2rxdma_ring_mask = intr_ctx->host2rxdma_ring_mask;
		intr_bkp->host2rxdma_mon_ring_mask =
					intr_ctx->host2rxdma_mon_ring_mask;
		intr_bkp->tx_mon_ring_mask = intr_ctx->tx_mon_ring_mask;

		intr_ctx->tx_ring_mask = 0;
		intr_ctx->rx_ring_mask = 0;
		intr_ctx->rx_mon_ring_mask = 0;
		intr_ctx->rx_err_ring_mask = 0;
		intr_ctx->rx_wbm_rel_ring_mask = 0;
		intr_ctx->reo_status_ring_mask = 0;
		intr_ctx->rxdma2host_ring_mask = 0;
		intr_ctx->host2rxdma_ring_mask = 0;
		intr_ctx->host2rxdma_mon_ring_mask = 0;
		intr_ctx->tx_mon_ring_mask = 0;

		intr_bkp++;
	}
}

/**
 * dp_restore_interrupt_ring_masks() - Restore rx interrupt masks
 * @soc: dp soc handle
 *
 * Return: void
 */
static void dp_restore_interrupt_ring_masks(struct dp_soc *soc)
{
	struct dp_intr_bkp *intr_bkp = soc->umac_reset_ctx.intr_ctx_bkp;
	struct dp_intr_bkp *intr_bkp_base = intr_bkp;
	struct dp_intr *intr_ctx;
	int num_ctxt = wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx);
	int i;

	if (!intr_bkp)
		return;

	for (i = 0; i < num_ctxt; i++) {
		intr_ctx = &soc->intr_ctx[i];

		intr_ctx->tx_ring_mask = intr_bkp->tx_ring_mask;
		intr_ctx->rx_ring_mask = intr_bkp->rx_ring_mask;
		intr_ctx->rx_mon_ring_mask = intr_bkp->rx_mon_ring_mask;
		intr_ctx->rx_err_ring_mask = intr_bkp->rx_err_ring_mask;
		intr_ctx->rx_wbm_rel_ring_mask = intr_bkp->rx_wbm_rel_ring_mask;
		intr_ctx->reo_status_ring_mask = intr_bkp->reo_status_ring_mask;
		intr_ctx->rxdma2host_ring_mask = intr_bkp->rxdma2host_ring_mask;
		intr_ctx->host2rxdma_ring_mask = intr_bkp->host2rxdma_ring_mask;
		intr_ctx->host2rxdma_mon_ring_mask =
			intr_bkp->host2rxdma_mon_ring_mask;
		intr_ctx->tx_mon_ring_mask = intr_bkp->tx_mon_ring_mask;

		intr_bkp++;
	}

	qdf_mem_free(intr_bkp_base);
	soc->umac_reset_ctx.intr_ctx_bkp = NULL;
}

/**
 * dp_resume_tx_hardstart() - Restore the old Tx hardstart functions
 * @soc: dp soc handle
 *
 * Return: void
 */
static void dp_resume_tx_hardstart(struct dp_soc *soc)
{
	struct dp_vdev *vdev;
	struct ol_txrx_hardtart_ctxt ctxt = {0};
	struct cdp_ctrl_objmgr_psoc *psoc = soc->ctrl_psoc;
	int i;

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev = soc->pdev_list[i];

		if (!pdev)
			continue;

		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			uint8_t vdev_id = vdev->vdev_id;

			dp_vdev_fetch_tx_handler(vdev, soc, &ctxt);
			soc->cdp_soc.ol_ops->dp_update_tx_hardstart(psoc,
								    vdev_id,
								    &ctxt);
		}
	}
}

/**
 * dp_pause_tx_hardstart() - Register Tx hardstart functions to drop packets
 * @soc: dp soc handle
 *
 * Return: void
 */
static void dp_pause_tx_hardstart(struct dp_soc *soc)
{
	struct dp_vdev *vdev;
	struct ol_txrx_hardtart_ctxt ctxt;
	struct cdp_ctrl_objmgr_psoc *psoc = soc->ctrl_psoc;
	int i;

	ctxt.tx = &dp_tx_drop;
	ctxt.tx_fast = &dp_tx_drop;
	ctxt.tx_exception = &dp_tx_exc_drop;

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev = soc->pdev_list[i];

		if (!pdev)
			continue;

		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			uint8_t vdev_id = vdev->vdev_id;

			soc->cdp_soc.ol_ops->dp_update_tx_hardstart(psoc,
								    vdev_id,
								    &ctxt);
		}
	}
}

/**
 * dp_unregister_notify_umac_pre_reset_fw_callback() - unregister notify_fw_cb
 * @soc: dp soc handle
 *
 * Return: void
 */
static inline
void dp_unregister_notify_umac_pre_reset_fw_callback(struct dp_soc *soc)
{
	soc->notify_fw_callback = NULL;
}

/**
 * dp_check_n_notify_umac_prereset_done() - Send pre reset done to firmware
 * @soc: dp soc handle
 *
 * Return: void
 */
static inline
void dp_check_n_notify_umac_prereset_done(struct dp_soc *soc)
{
	/* Some Cpu(s) is processing the umac rings*/
	if (soc->service_rings_running)
		return;

	/* Unregister the callback */
	dp_unregister_notify_umac_pre_reset_fw_callback(soc);

	/* Check if notify was already sent by any other thread */
	if (qdf_atomic_test_and_set_bit(DP_UMAC_RESET_NOTIFY_DONE,
					&soc->service_rings_running))
		return;

	/* Notify the firmware that Umac pre reset is complete */
	dp_umac_reset_notify_action_completion(soc,
					       UMAC_RESET_ACTION_DO_PRE_RESET);
}

/**
 * dp_register_notify_umac_pre_reset_fw_callback() - register notify_fw_cb
 * @soc: dp soc handle
 *
 * Return: void
 */
static inline
void dp_register_notify_umac_pre_reset_fw_callback(struct dp_soc *soc)
{
	soc->notify_fw_callback = dp_check_n_notify_umac_prereset_done;
}

#ifdef DP_UMAC_HW_HARD_RESET
/**
 * dp_set_umac_regs() - Reinitialize host umac registers
 * @soc: dp soc handle
 *
 * Return: void
 */
static void dp_set_umac_regs(struct dp_soc *soc)
{
	int i;
	struct hal_reo_params reo_params;

	qdf_mem_zero(&reo_params, sizeof(reo_params));

	if (wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx)) {
		if (soc->arch_ops.reo_remap_config(soc, &reo_params.remap0,
						   &reo_params.remap1,
						   &reo_params.remap2))
			reo_params.rx_hash_enabled = true;
		else
			reo_params.rx_hash_enabled = false;
	}

	reo_params.reo_qref = &soc->reo_qref;
	hal_reo_setup(soc->hal_soc, &reo_params, 0);

	soc->arch_ops.dp_cc_reg_cfg_init(soc, true);

	for (i = 0; i < PCP_TID_MAP_MAX; i++)
		hal_tx_update_pcp_tid_map(soc->hal_soc, soc->pcp_tid_map[i], i);

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_vdev *vdev = NULL;
		struct dp_pdev *pdev = soc->pdev_list[i];

		if (!pdev)
			continue;

		for (i = 0; i < soc->num_hw_dscp_tid_map; i++)
			hal_tx_set_dscp_tid_map(soc->hal_soc,
						pdev->dscp_tid_map[i], i);

		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			soc->arch_ops.dp_bank_reconfig(soc, vdev);
			soc->arch_ops.dp_reconfig_tx_vdev_mcast_ctrl(soc,
								      vdev);
		}
	}
}
#else
static void dp_set_umac_regs(struct dp_soc *soc)
{
}
#endif

/**
 * dp_reinit_rings() - Reinitialize host managed rings
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS
 */
static void dp_reinit_rings(struct dp_soc *soc)
{
	unsigned long end;

	dp_soc_srng_deinit(soc);
	dp_hw_link_desc_ring_deinit(soc);

	/* Busy wait for 2 ms to make sure the rings are in idle state
	 * before we enable them again
	 */
	end = jiffies + msecs_to_jiffies(2);
	while (time_before(jiffies, end))
		;

	dp_hw_link_desc_ring_init(soc);
	dp_link_desc_ring_replenish(soc, WLAN_INVALID_PDEV_ID);
	dp_soc_srng_init(soc);
}

/**
 * dp_umac_reset_action_trigger_recovery() - Handle FW Umac recovery trigger
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_umac_reset_action_trigger_recovery(struct dp_soc *soc)
{
	enum umac_reset_action action = UMAC_RESET_ACTION_DO_TRIGGER_RECOVERY;

	return dp_umac_reset_notify_action_completion(soc, action);
}

#ifdef WLAN_SUPPORT_PPEDS
/**
 * dp_umac_reset_service_handle_n_notify_done()
 *	Handle Umac pre reset for direct switch
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_umac_reset_service_handle_n_notify_done(struct dp_soc *soc)
{
	if (!soc->arch_ops.txrx_soc_ppeds_enabled_check ||
	    !soc->arch_ops.txrx_soc_ppeds_service_status_update ||
	    !soc->arch_ops.txrx_soc_ppeds_interrupt_stop)
		goto non_ppeds;

	/*
	 * Check if ppeds is enabled on SoC.
	 */
	if (!soc->arch_ops.txrx_soc_ppeds_enabled_check(soc))
		goto non_ppeds;

	/*
	 * Start the UMAC pre reset done service.
	 */
	soc->arch_ops.txrx_soc_ppeds_service_status_update(soc, true);

	dp_register_notify_umac_pre_reset_fw_callback(soc);

	soc->arch_ops.txrx_soc_ppeds_interrupt_stop(soc);

	dp_soc_ppeds_stop((struct cdp_soc_t *)soc);

	/*
	 * UMAC pre reset service complete
	 */
	soc->arch_ops.txrx_soc_ppeds_service_status_update(soc, false);

	soc->umac_reset_ctx.nbuf_list = NULL;
	return QDF_STATUS_SUCCESS;

non_ppeds:
	dp_register_notify_umac_pre_reset_fw_callback(soc);
	dp_umac_reset_trigger_pre_reset_notify_cb(soc);
	soc->umac_reset_ctx.nbuf_list = NULL;
	return QDF_STATUS_SUCCESS;
}

static inline void dp_umac_reset_ppeds_txdesc_pool_reset(struct dp_soc *soc,
							 qdf_nbuf_t *nbuf_list)
{
	if (!soc->arch_ops.txrx_soc_ppeds_enabled_check ||
	    !soc->arch_ops.txrx_soc_ppeds_txdesc_pool_reset)
		return;

	/*
	 * Deinit of PPEDS Tx desc rings.
	 */
	if (soc->arch_ops.txrx_soc_ppeds_enabled_check(soc))
		soc->arch_ops.txrx_soc_ppeds_txdesc_pool_reset(soc, nbuf_list);
}

static inline void dp_umac_reset_ppeds_start(struct dp_soc *soc)
{
	if (!soc->arch_ops.txrx_soc_ppeds_enabled_check ||
	    !soc->arch_ops.txrx_soc_ppeds_start ||
	    !soc->arch_ops.txrx_soc_ppeds_interrupt_start)
		return;

	/*
	 * Start PPEDS node and enable interrupt.
	 */
	if (soc->arch_ops.txrx_soc_ppeds_enabled_check(soc)) {
		soc->arch_ops.txrx_soc_ppeds_start(soc);
		soc->arch_ops.txrx_soc_ppeds_interrupt_start(soc);
	}
}
#else
static QDF_STATUS dp_umac_reset_service_handle_n_notify_done(struct dp_soc *soc)
{
	dp_register_notify_umac_pre_reset_fw_callback(soc);
	dp_umac_reset_trigger_pre_reset_notify_cb(soc);
	soc->umac_reset_ctx.nbuf_list = NULL;
	return QDF_STATUS_SUCCESS;
}

static inline void dp_umac_reset_ppeds_txdesc_pool_reset(struct dp_soc *soc,
							 qdf_nbuf_t *nbuf_list)
{
}

static inline void dp_umac_reset_ppeds_start(struct dp_soc *soc)
{
}
#endif

/**
 * dp_umac_reset_handle_pre_reset() - Handle Umac prereset interrupt from FW
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_umac_reset_handle_pre_reset(struct dp_soc *soc)
{
	dp_reset_interrupt_ring_masks(soc);

	dp_pause_tx_hardstart(soc);
	dp_pause_reo_send_cmd(soc);
	dp_umac_reset_service_handle_n_notify_done(soc);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_handle_post_reset() - Handle Umac postreset interrupt from FW
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_umac_reset_handle_post_reset(struct dp_soc *soc)
{
	if (!soc->umac_reset_ctx.skel_enable) {
		bool cleanup_needed;
		qdf_nbuf_t *nbuf_list = &soc->umac_reset_ctx.nbuf_list;

		dp_set_umac_regs(soc);

		dp_reinit_rings(soc);

		dp_rx_desc_reuse(soc, nbuf_list);

		dp_cleanup_reo_cmd_module(soc);

		dp_umac_reset_ppeds_txdesc_pool_reset(soc, nbuf_list);

		cleanup_needed = dp_get_global_tx_desc_cleanup_flag(soc);

		dp_tx_desc_pool_cleanup(soc, nbuf_list, cleanup_needed);

		dp_reset_tid_q_setup(soc);
	}

	return dp_umac_reset_notify_action_completion(soc,
					UMAC_RESET_ACTION_DO_POST_RESET_START);
}

/**
 * dp_umac_reset_handle_post_reset_complete() - Handle Umac postreset_complete
 *						interrupt from FW
 * @soc: dp soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_umac_reset_handle_post_reset_complete(struct dp_soc *soc)
{
	QDF_STATUS status;
	qdf_nbuf_t nbuf_list = soc->umac_reset_ctx.nbuf_list;
	uint8_t mac_id;

	soc->umac_reset_ctx.nbuf_list = NULL;

	soc->service_rings_running = 0;

	dp_resume_reo_send_cmd(soc);

	dp_umac_reset_ppeds_start(soc);

	dp_restore_interrupt_ring_masks(soc);

	dp_resume_tx_hardstart(soc);

	dp_reset_global_tx_desc_cleanup_flag(soc);

	status = dp_umac_reset_notify_action_completion(soc,
				UMAC_RESET_ACTION_DO_POST_RESET_COMPLETE);

	while (nbuf_list) {
		qdf_nbuf_t nbuf = nbuf_list->next;

		qdf_nbuf_free(nbuf_list);
		nbuf_list = nbuf;
	}

	/*
	 * at pre-reset if in_use descriptors are not sufficient we replenish
	 * only 1/3 of the ring. Try to replenish full ring here.
	 */
	for (mac_id = 0; mac_id < MAX_PDEV_CNT; mac_id++) {
		struct dp_srng *dp_rxdma_srng =
					&soc->rx_refill_buf_ring[mac_id];
		struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];

		dp_rx_buffers_lt_replenish_simple(soc, mac_id, dp_rxdma_srng,
						  rx_desc_pool, true);
	}

	dp_umac_reset_info("Umac reset done on soc %pK\n trigger start : %u us "
			   "trigger done : %u us prereset : %u us\n"
			   "postreset : %u us \n postreset complete: %u us \n",
			   soc,
			   soc->umac_reset_ctx.ts.trigger_done -
			   soc->umac_reset_ctx.ts.trigger_start,
			   soc->umac_reset_ctx.ts.pre_reset_done -
			   soc->umac_reset_ctx.ts.pre_reset_start,
			   soc->umac_reset_ctx.ts.post_reset_done -
			   soc->umac_reset_ctx.ts.post_reset_start,
			   soc->umac_reset_ctx.ts.post_reset_complete_done -
			   soc->umac_reset_ctx.ts.post_reset_complete_start);

	return status;
}
#endif
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
static void
dp_set_pkt_capture_mode(struct cdp_soc_t *soc_handle, bool val)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;

	soc->wlan_cfg_ctx->pkt_capture_mode = val;
}
#endif

#ifdef HW_TX_DELAY_STATS_ENABLE
/**
 * dp_enable_disable_vdev_tx_delay_stats() - Start/Stop tx delay stats capture
 * @soc_hdl: DP soc handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: None
 */
static void
dp_enable_disable_vdev_tx_delay_stats(struct cdp_soc_t *soc_hdl,
				      uint8_t vdev_id,
				      uint8_t value)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = NULL;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return;

	vdev->hw_tx_delay_stats_enabled = value;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}

/**
 * dp_check_vdev_tx_delay_stats_enabled() - check the feature is enabled or not
 * @soc_hdl: DP soc handle
 * @vdev_id: vdev id
 *
 * Return: 1 if enabled, 0 if disabled
 */
static uint8_t
dp_check_vdev_tx_delay_stats_enabled(struct cdp_soc_t *soc_hdl,
				     uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;
	uint8_t ret_val = 0;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return ret_val;

	ret_val = vdev->hw_tx_delay_stats_enabled;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return ret_val;
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
static void
dp_recovery_vdev_flush_peers(struct cdp_soc_t *cdp_soc,
			     uint8_t vdev_id,
			     bool mlo_peers_only)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_vdev *vdev;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);

	if (!vdev)
		return;

	dp_vdev_flush_peers((struct cdp_vdev *)vdev, false, mlo_peers_only);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}
#endif
#ifdef QCA_GET_TSF_VIA_REG
/**
 * dp_get_tsf_time() - get tsf time
 * @soc_hdl: Datapath soc handle
 * @tsf_id: TSF identifier
 * @mac_id: mac_id
 * @tsf: pointer to update tsf value
 * @tsf_sync_soc_time: pointer to update tsf sync time
 *
 * Return: None.
 */
static inline void
dp_get_tsf_time(struct cdp_soc_t *soc_hdl, uint32_t tsf_id, uint32_t mac_id,
		uint64_t *tsf, uint64_t *tsf_sync_soc_time)
{
	hal_get_tsf_time(((struct dp_soc *)soc_hdl)->hal_soc, tsf_id, mac_id,
			 tsf, tsf_sync_soc_time);
}
#else
static inline void
dp_get_tsf_time(struct cdp_soc_t *soc_hdl, uint32_t tsf_id, uint32_t mac_id,
		uint64_t *tsf, uint64_t *tsf_sync_soc_time)
{
}
#endif

/**
 * dp_get_tsf2_scratch_reg() - get tsf2 offset from the scratch register
 * @soc_hdl: Datapath soc handle
 * @mac_id: mac_id
 * @value: pointer to update tsf2 offset value
 *
 * Return: None.
 */
static inline void
dp_get_tsf2_scratch_reg(struct cdp_soc_t *soc_hdl, uint8_t mac_id,
			uint64_t *value)
{
	hal_get_tsf2_offset(((struct dp_soc *)soc_hdl)->hal_soc, mac_id, value);
}

/**
 * dp_get_tqm_scratch_reg() - get tqm offset from the scratch register
 * @soc_hdl: Datapath soc handle
 * @value: pointer to update tqm offset value
 *
 * Return: None.
 */
static inline void
dp_get_tqm_scratch_reg(struct cdp_soc_t *soc_hdl, uint64_t *value)
{
	hal_get_tqm_offset(((struct dp_soc *)soc_hdl)->hal_soc, value);
}

/**
 * dp_set_tx_pause() - Pause or resume tx path
 * @soc_hdl: Datapath soc handle
 * @flag: set or clear is_tx_pause
 *
 * Return: None.
 */
static inline
void dp_set_tx_pause(struct cdp_soc_t *soc_hdl, bool flag)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	soc->is_tx_pause = flag;
}

static inline uint64_t dp_rx_fisa_get_cmem_base(struct cdp_soc_t *soc_hdl,
						uint64_t size)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (soc->arch_ops.dp_get_fst_cmem_base)
		return soc->arch_ops.dp_get_fst_cmem_base(soc, size);

	return 0;
}

#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
/**
 * dp_evaluate_update_tx_ilp_config() - Evaluate and update DP TX
 *                                      ILP configuration
 * @soc_hdl: CDP SOC handle
 * @num_msdu_idx_map: Number of HTT msdu index to qtype map in array
 * @msdu_idx_map_arr: Pointer to HTT msdu index to qtype map array
 *
 * This function will check: (a) TX ILP INI configuration,
 * (b) index 3 value in array same as HTT_MSDU_QTYPE_LATENCY_TOLERANT,
 * only if both (a) and (b) condition is met, then TX ILP feature is
 * considered to be enabled.
 *
 * Return: Final updated TX ILP enable result in dp_soc,
 *         true is enabled, false is not
 */
static
bool dp_evaluate_update_tx_ilp_config(struct cdp_soc_t *soc_hdl,
				      uint8_t num_msdu_idx_map,
				      uint8_t *msdu_idx_map_arr)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	bool enable_tx_ilp = false;

	/**
	 * Check INI configuration firstly, if it's disabled,
	 * then keep feature disabled.
	 */
	if (!wlan_cfg_get_tx_ilp_inspect_config(soc->wlan_cfg_ctx)) {
		dp_info("TX ILP INI is disabled already");
		goto update_tx_ilp;
	}

	/* Check if the msdu index to qtype map table is valid */
	if (num_msdu_idx_map != HTT_MSDUQ_MAX_INDEX || !msdu_idx_map_arr) {
		dp_info("Invalid msdu_idx qtype map num: 0x%x, arr_addr %pK",
			num_msdu_idx_map, msdu_idx_map_arr);
		goto update_tx_ilp;
	}

	dp_info("msdu_idx_map_arr idx 0x%x value 0x%x",
		HTT_MSDUQ_INDEX_CUSTOM_PRIO_1,
		msdu_idx_map_arr[HTT_MSDUQ_INDEX_CUSTOM_PRIO_1]);

	if (HTT_MSDU_QTYPE_USER_SPECIFIED ==
	    msdu_idx_map_arr[HTT_MSDUQ_INDEX_CUSTOM_PRIO_1])
		enable_tx_ilp = true;

update_tx_ilp:
	soc->tx_ilp_enable = enable_tx_ilp;
	dp_info("configure tx ilp enable %d", soc->tx_ilp_enable);

	return soc->tx_ilp_enable;
}
#endif

static struct cdp_cmn_ops dp_ops_cmn = {
	.txrx_soc_attach_target = dp_soc_attach_target_wifi3,
	.txrx_vdev_attach = dp_vdev_attach_wifi3,
	.txrx_vdev_detach = dp_vdev_detach_wifi3,
	.txrx_pdev_attach = dp_pdev_attach_wifi3,
	.txrx_pdev_post_attach = dp_pdev_post_attach_wifi3,
	.txrx_pdev_detach = dp_pdev_detach_wifi3,
	.txrx_pdev_deinit = dp_pdev_deinit_wifi3,
	.txrx_peer_create = dp_peer_create_wifi3,
	.txrx_peer_setup = dp_peer_setup_wifi3_wrapper,
#ifdef FEATURE_AST
	.txrx_peer_teardown = dp_peer_teardown_wifi3,
#else
	.txrx_peer_teardown = NULL,
#endif
	.txrx_peer_add_ast = dp_peer_add_ast_wifi3,
	.txrx_peer_update_ast = dp_peer_update_ast_wifi3,
	.txrx_peer_get_ast_info_by_soc = dp_peer_get_ast_info_by_soc_wifi3,
	.txrx_peer_get_ast_info_by_pdev =
		dp_peer_get_ast_info_by_pdevid_wifi3,
	.txrx_peer_ast_delete_by_soc =
		dp_peer_ast_entry_del_by_soc,
	.txrx_peer_ast_delete_by_pdev =
		dp_peer_ast_entry_del_by_pdev,
	.txrx_peer_HMWDS_ast_delete = dp_peer_HMWDS_ast_entry_del,
	.txrx_peer_delete = dp_peer_delete_wifi3,
#ifdef DP_RX_UDP_OVER_PEER_ROAM
	.txrx_update_roaming_peer = dp_update_roaming_peer_wifi3,
#endif
	.txrx_vdev_register = dp_vdev_register_wifi3,
	.txrx_soc_detach = dp_soc_detach_wifi3,
	.txrx_soc_deinit = dp_soc_deinit_wifi3,
	.txrx_soc_init = dp_soc_init_wifi3,
#ifndef QCA_HOST_MODE_WIFI_DISABLED
	.txrx_tso_soc_attach = dp_tso_soc_attach,
	.txrx_tso_soc_detach = dp_tso_soc_detach,
	.tx_send = dp_tx_send,
	.tx_send_exc = dp_tx_send_exception,
#endif
	.set_tx_pause = dp_set_tx_pause,
	.txrx_pdev_init = dp_pdev_init_wifi3,
	.txrx_get_vdev_mac_addr = dp_get_vdev_mac_addr_wifi3,
	.txrx_get_ctrl_pdev_from_vdev = dp_get_ctrl_pdev_from_vdev_wifi3,
	.txrx_ath_getstats = dp_get_device_stats,
#ifndef WLAN_SOFTUMAC_SUPPORT
	.addba_requestprocess = dp_addba_requestprocess_wifi3,
	.addba_responsesetup = dp_addba_responsesetup_wifi3,
	.addba_resp_tx_completion = dp_addba_resp_tx_completion_wifi3,
	.delba_process = dp_delba_process_wifi3,
	.set_addba_response = dp_set_addba_response,
	.flush_cache_rx_queue = NULL,
	.tid_update_ba_win_size = dp_rx_tid_update_ba_win_size,
#endif
	/* TODO: get API's for dscp-tid need to be added*/
	.set_vdev_dscp_tid_map = dp_set_vdev_dscp_tid_map_wifi3,
	.set_pdev_dscp_tid_map = dp_set_pdev_dscp_tid_map_wifi3,
	.txrx_get_total_per = dp_get_total_per,
	.txrx_stats_request = dp_txrx_stats_request,
	.txrx_get_peer_mac_from_peer_id = dp_get_peer_mac_from_peer_id,
	.display_stats = dp_txrx_dump_stats,
	.notify_asserted_soc = dp_soc_notify_asserted_soc,
	.txrx_intr_attach = dp_soc_interrupt_attach_wrapper,
	.txrx_intr_detach = dp_soc_interrupt_detach_wrapper,
	.txrx_ppeds_stop = dp_soc_ppeds_stop,
	.set_key_sec_type = dp_set_key_sec_type_wifi3,
	.update_config_parameters = dp_update_config_parameters,
	/* TODO: Add other functions */
	.txrx_data_tx_cb_set = dp_txrx_data_tx_cb_set,
	.get_dp_txrx_handle = dp_pdev_get_dp_txrx_handle,
	.set_dp_txrx_handle = dp_pdev_set_dp_txrx_handle,
	.get_vdev_dp_ext_txrx_handle = dp_vdev_get_dp_ext_handle,
	.set_vdev_dp_ext_txrx_handle = dp_vdev_set_dp_ext_handle,
	.get_soc_dp_txrx_handle = dp_soc_get_dp_txrx_handle,
	.set_soc_dp_txrx_handle = dp_soc_set_dp_txrx_handle,
	.map_pdev_to_lmac = dp_soc_map_pdev_to_lmac,
	.handle_mode_change = dp_soc_handle_pdev_mode_change,
	.set_pdev_status_down = dp_soc_set_pdev_status_down,
	.txrx_peer_reset_ast = dp_wds_reset_ast_wifi3,
	.txrx_peer_reset_ast_table = dp_wds_reset_ast_table_wifi3,
	.txrx_peer_flush_ast_table = dp_wds_flush_ast_table_wifi3,
	.txrx_peer_map_attach = dp_peer_map_attach_wifi3,
	.set_soc_param = dp_soc_set_param,
	.txrx_get_os_rx_handles_from_vdev =
					dp_get_os_rx_handles_from_vdev_wifi3,
#ifndef WLAN_SOFTUMAC_SUPPORT
	.set_pn_check = dp_set_pn_check_wifi3,
	.txrx_set_ba_aging_timeout = dp_set_ba_aging_timeout,
	.txrx_get_ba_aging_timeout = dp_get_ba_aging_timeout,
	.delba_tx_completion = dp_delba_tx_completion_wifi3,
	.set_pdev_pcp_tid_map = dp_set_pdev_pcp_tid_map_wifi3,
	.set_vdev_pcp_tid_map = dp_set_vdev_pcp_tid_map_wifi3,
#endif
	.get_dp_capabilities = dp_get_cfg_capabilities,
	.txrx_get_cfg = dp_get_cfg,
	.set_rate_stats_ctx = dp_soc_set_rate_stats_ctx,
	.get_rate_stats_ctx = dp_soc_get_rate_stats_ctx,
	.txrx_peer_flush_rate_stats = dp_peer_flush_rate_stats,
	.txrx_flush_rate_stats_request = dp_flush_rate_stats_req,
	.txrx_peer_get_peerstats_ctx = dp_peer_get_peerstats_ctx,

	.txrx_cp_peer_del_response = dp_cp_peer_del_resp_handler,
#ifdef QCA_MULTIPASS_SUPPORT
	.set_vlan_groupkey = dp_set_vlan_groupkey,
#endif
	.get_peer_mac_list = dp_get_peer_mac_list,
	.get_peer_id = dp_get_peer_id,
#ifdef QCA_SUPPORT_WDS_EXTENDED
	.set_wds_ext_peer_rx = dp_wds_ext_set_peer_rx,
	.get_wds_ext_peer_osif_handle = dp_wds_ext_get_peer_osif_handle,
	.set_wds_ext_peer_bit = dp_wds_ext_set_peer_bit,
#endif /* QCA_SUPPORT_WDS_EXTENDED */

#if defined(FEATURE_RUNTIME_PM) || defined(DP_POWER_SAVE)
	.txrx_drain = dp_drain_txrx,
#endif
#if defined(FEATURE_RUNTIME_PM)
	.set_rtpm_tput_policy = dp_set_rtpm_tput_policy_requirement,
#endif
#ifdef WLAN_SYSFS_DP_STATS
	.txrx_sysfs_fill_stats = dp_sysfs_fill_stats,
	.txrx_sysfs_set_stat_type = dp_sysfs_set_stat_type,
#endif /* WLAN_SYSFS_DP_STATS */
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
	.set_pkt_capture_mode = dp_set_pkt_capture_mode,
#endif
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	.txrx_recovery_vdev_flush_peers = dp_recovery_vdev_flush_peers,
#endif
	.txrx_umac_reset_deinit = dp_soc_umac_reset_deinit,
	.txrx_umac_reset_init = dp_soc_umac_reset_init,
	.txrx_get_tsf_time = dp_get_tsf_time,
	.txrx_get_tsf2_offset = dp_get_tsf2_scratch_reg,
	.txrx_get_tqm_offset = dp_get_tqm_scratch_reg,
#ifdef WLAN_SUPPORT_RX_FISA
	.get_fst_cmem_base = dp_rx_fisa_get_cmem_base,
#endif
#ifdef WLAN_SUPPORT_DPDK
	.dpdk_get_ring_info = dp_dpdk_get_ring_info,
	.cfgmgr_get_soc_info = dp_cfgmgr_get_soc_info,
	.cfgmgr_get_vdev_info = dp_cfgmgr_get_vdev_info,
	.cfgmgr_get_peer_info = dp_cfgmgr_get_peer_info,
	.cfgmgr_get_vdev_create_evt_info = dp_cfgmgr_get_vdev_create_evt_info,
	.cfgmgr_get_peer_create_evt_info = dp_cfgmgr_get_peer_create_evt_info,
#endif
};

static struct cdp_ctrl_ops dp_ops_ctrl = {
	.txrx_peer_authorize = dp_peer_authorize,
	.txrx_peer_get_authorize = dp_peer_get_authorize,
#ifdef VDEV_PEER_PROTOCOL_COUNT
	.txrx_enable_peer_protocol_count = dp_enable_vdev_peer_protocol_count,
	.txrx_set_peer_protocol_drop_mask =
		dp_enable_vdev_peer_protocol_drop_mask,
	.txrx_is_peer_protocol_count_enabled =
		dp_is_vdev_peer_protocol_count_enabled,
	.txrx_get_peer_protocol_drop_mask = dp_get_vdev_peer_protocol_drop_mask,
#endif
	.txrx_set_vdev_param = dp_set_vdev_param_wrapper,
	.txrx_set_psoc_param = dp_set_psoc_param,
	.txrx_get_psoc_param = dp_get_psoc_param,
#ifndef WLAN_SOFTUMAC_SUPPORT
	.txrx_set_pdev_reo_dest = dp_set_pdev_reo_dest,
	.txrx_get_pdev_reo_dest = dp_get_pdev_reo_dest,
#endif
	.txrx_get_sec_type = dp_get_sec_type,
	.txrx_wdi_event_sub = dp_wdi_event_sub,
	.txrx_wdi_event_unsub = dp_wdi_event_unsub,
	.txrx_set_pdev_param = dp_set_pdev_param,
	.txrx_get_pdev_param = dp_get_pdev_param,
#ifdef WLAN_FEATURE_11BE_MLO
	.txrx_set_peer_param = dp_set_peer_param_wrapper,
#else
	.txrx_set_peer_param = dp_set_peer_param,
#endif
	.txrx_get_peer_param = dp_get_peer_param,
#ifdef VDEV_PEER_PROTOCOL_COUNT
	.txrx_peer_protocol_cnt = dp_peer_stats_update_protocol_cnt,
#endif
#ifdef WLAN_SUPPORT_MSCS
	.txrx_record_mscs_params = dp_record_mscs_params,
#endif
	.set_key = dp_set_michael_key,
	.txrx_get_vdev_param = dp_get_vdev_param,
	.calculate_delay_stats = dp_calculate_delay_stats,
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
	.txrx_update_pdev_rx_protocol_tag = dp_update_pdev_rx_protocol_tag,
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
	.txrx_dump_pdev_rx_protocol_tag_stats =
				dp_dump_pdev_rx_protocol_tag_stats,
#endif /* WLAN_SUPPORT_RX_TAG_STATISTICS */
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */
#ifdef WLAN_SUPPORT_RX_FLOW_TAG
	.txrx_set_rx_flow_tag = dp_set_rx_flow_tag,
	.txrx_dump_rx_flow_tag_stats = dp_dump_rx_flow_tag_stats,
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */
#ifdef QCA_MULTIPASS_SUPPORT
	.txrx_peer_set_vlan_id = dp_peer_set_vlan_id,
#endif /*QCA_MULTIPASS_SUPPORT*/
#if defined(WLAN_FEATURE_TSF_AUTO_REPORT) || defined(WLAN_CONFIG_TX_DELAY)
	.txrx_set_delta_tsf = dp_set_delta_tsf,
#endif
#ifdef WLAN_FEATURE_TSF_UPLINK_DELAY
	.txrx_set_tsf_ul_delay_report = dp_set_tsf_ul_delay_report,
	.txrx_get_uplink_delay = dp_get_uplink_delay,
#endif
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	.txrx_set_pdev_phyrx_error_mask = dp_set_pdev_phyrx_error_mask,
	.txrx_get_pdev_phyrx_error_mask = dp_get_pdev_phyrx_error_mask,
#endif
	.txrx_peer_flush_frags = dp_peer_flush_frags,
#ifdef DP_UMAC_HW_RESET_SUPPORT
	.get_umac_reset_in_progress_state = dp_get_umac_reset_in_progress_state,
#endif
#ifdef WLAN_SUPPORT_RX_FISA
	.txrx_fisa_config = dp_fisa_config,
#endif
};

static struct cdp_me_ops dp_ops_me = {
#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef ATH_SUPPORT_IQUE
	.tx_me_alloc_descriptor = dp_tx_me_alloc_descriptor,
	.tx_me_free_descriptor = dp_tx_me_free_descriptor,
	.tx_me_convert_ucast = dp_tx_me_send_convert_ucast,
#endif
#endif
};

static struct cdp_host_stats_ops dp_ops_host_stats = {
	.txrx_per_peer_stats = dp_get_host_peer_stats,
	.get_fw_peer_stats = dp_get_fw_peer_stats,
	.get_htt_stats = dp_get_htt_stats,
	.txrx_stats_publish = dp_txrx_stats_publish,
	.txrx_get_vdev_stats  = dp_txrx_get_vdev_stats,
	.txrx_get_peer_stats = dp_txrx_get_peer_stats,
	.txrx_get_peer_stats_based_on_peer_type =
			dp_txrx_get_peer_stats_based_on_peer_type,
	.txrx_get_soc_stats = dp_txrx_get_soc_stats,
	.txrx_get_peer_stats_param = dp_txrx_get_peer_stats_param,
	.txrx_get_per_link_stats = dp_txrx_get_per_link_peer_stats,
	.txrx_reset_peer_stats = dp_txrx_reset_peer_stats,
	.txrx_get_pdev_stats = dp_txrx_get_pdev_stats,
#if defined(IPA_OFFLOAD) && defined(QCA_ENHANCED_STATS_SUPPORT)
	.txrx_get_peer_stats = dp_ipa_txrx_get_peer_stats,
	.txrx_get_vdev_stats  = dp_ipa_txrx_get_vdev_stats,
	.txrx_get_pdev_stats = dp_ipa_txrx_get_pdev_stats,
#endif
	.txrx_get_ratekbps = dp_txrx_get_ratekbps,
	.txrx_update_vdev_stats = dp_txrx_update_vdev_host_stats,
	.txrx_get_peer_delay_stats = dp_txrx_get_peer_delay_stats,
	.txrx_get_peer_jitter_stats = dp_txrx_get_peer_jitter_stats,
#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
	.txrx_alloc_vdev_stats_id = dp_txrx_alloc_vdev_stats_id,
	.txrx_reset_vdev_stats_id = dp_txrx_reset_vdev_stats_id,
#endif
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	.get_peer_tx_capture_stats = dp_peer_get_tx_capture_stats,
	.get_pdev_tx_capture_stats = dp_pdev_get_tx_capture_stats,
#endif /* WLAN_TX_PKT_CAPTURE_ENH */
#ifdef HW_TX_DELAY_STATS_ENABLE
	.enable_disable_vdev_tx_delay_stats =
				dp_enable_disable_vdev_tx_delay_stats,
	.is_tx_delay_stats_enabled = dp_check_vdev_tx_delay_stats_enabled,
#endif
	.txrx_get_pdev_tid_stats = dp_pdev_get_tid_stats,
#ifdef WLAN_CONFIG_TELEMETRY_AGENT
	.txrx_pdev_telemetry_stats = dp_get_pdev_telemetry_stats,
	.txrx_peer_telemetry_stats = dp_get_peer_telemetry_stats,
	.txrx_pdev_deter_stats = dp_get_pdev_deter_stats,
	.txrx_peer_deter_stats = dp_get_peer_deter_stats,
	.txrx_update_pdev_chan_util_stats = dp_update_pdev_chan_util_stats,
#endif
	.txrx_get_peer_extd_rate_link_stats =
					dp_get_peer_extd_rate_link_stats,
	.get_pdev_obss_stats = dp_get_obss_stats,
	.clear_pdev_obss_pd_stats = dp_clear_pdev_obss_pd_stats,
	.txrx_get_interface_stats  = dp_txrx_get_interface_stats,
#ifdef WLAN_FEATURE_TX_LATENCY_STATS
	.tx_latency_stats_fetch = dp_tx_latency_stats_fetch,
	.tx_latency_stats_config = dp_tx_latency_stats_config,
	.tx_latency_stats_register_cb = dp_tx_latency_stats_register_cb,
#endif
	/* TODO */
};

static struct cdp_raw_ops dp_ops_raw = {
	/* TODO */
};

#ifdef PEER_FLOW_CONTROL
static struct cdp_pflow_ops dp_ops_pflow = {
	dp_tx_flow_ctrl_configure_pdev,
};
#endif

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
static struct cdp_cfr_ops dp_ops_cfr = {
	.txrx_get_cfr_rcc = dp_get_cfr_rcc,
	.txrx_set_cfr_rcc = dp_set_cfr_rcc,
	.txrx_get_cfr_dbg_stats = dp_get_cfr_dbg_stats,
	.txrx_clear_cfr_dbg_stats = dp_clear_cfr_dbg_stats,
};
#endif

#ifdef WLAN_SUPPORT_MSCS
static struct cdp_mscs_ops dp_ops_mscs = {
	.mscs_peer_lookup_n_get_priority = dp_mscs_peer_lookup_n_get_priority,
};
#endif

#ifdef WLAN_SUPPORT_MESH_LATENCY
static struct cdp_mesh_latency_ops dp_ops_mesh_latency = {
	.mesh_latency_update_peer_parameter =
		dp_mesh_latency_update_peer_parameter,
};
#endif

#ifdef WLAN_SUPPORT_SCS
static struct cdp_scs_ops dp_ops_scs = {
	.scs_peer_lookup_n_rule_match = dp_scs_peer_lookup_n_rule_match,
};
#endif

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static struct cdp_fse_ops dp_ops_fse = {
	.fse_rule_add = dp_rx_sfe_add_flow_entry,
	.fse_rule_delete = dp_rx_sfe_delete_flow_entry,
};
#endif

#ifdef CONFIG_SAWF_DEF_QUEUES
static struct cdp_sawf_ops dp_ops_sawf = {
	.sawf_def_queues_map_req = dp_sawf_def_queues_map_req,
	.sawf_def_queues_unmap_req = dp_sawf_def_queues_unmap_req,
	.sawf_def_queues_get_map_report =
		dp_sawf_def_queues_get_map_report,
#ifdef CONFIG_SAWF_STATS
	.sawf_get_peer_msduq_info = dp_sawf_get_peer_msduq_info,
	.txrx_get_peer_sawf_delay_stats = dp_sawf_get_peer_delay_stats,
	.txrx_get_peer_sawf_tx_stats = dp_sawf_get_peer_tx_stats,
	.sawf_mpdu_stats_req = dp_sawf_mpdu_stats_req,
	.sawf_mpdu_details_stats_req = dp_sawf_mpdu_details_stats_req,
	.txrx_sawf_set_mov_avg_params = dp_sawf_set_mov_avg_params,
	.txrx_sawf_set_sla_params = dp_sawf_set_sla_params,
	.txrx_sawf_init_telemtery_params = dp_sawf_init_telemetry_params,
	.telemetry_get_throughput_stats = dp_sawf_get_tx_stats,
	.telemetry_get_mpdu_stats = dp_sawf_get_mpdu_sched_stats,
	.telemetry_get_drop_stats = dp_sawf_get_drop_stats,
	.peer_config_ul = dp_sawf_peer_config_ul,
	.swaf_peer_sla_configuration = dp_swaf_peer_sla_configuration,
	.sawf_peer_flow_count = dp_sawf_peer_flow_count,
#endif
#ifdef WLAN_FEATURE_11BE_MLO_3_LINK_TX
	.get_peer_msduq = dp_sawf_get_peer_msduq,
	.sawf_3_link_peer_flow_count = dp_sawf_3_link_peer_flow_count,
#endif
};
#endif

#ifdef DP_TX_TRACKING

#define DP_TX_COMP_MAX_LATENCY_MS 60000

static bool dp_check_pending_tx(struct dp_soc *soc)
{
	hal_soc_handle_t hal_soc = soc->hal_soc;
	uint32_t hp, tp, i;

	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		if (dp_ipa_is_ring_ipa_tx(soc, i))
			continue;

		hal_get_sw_hptp(hal_soc, soc->tcl_data_ring[i].hal_srng,
				&tp, &hp);

		if (hp != tp) {
			dp_info_rl("Pending transactions in TCL DATA Ring[%d] hp=0x%x, tp=0x%x",
				   i, hp, tp);
			return true;
		}

		if (wlan_cfg_get_wbm_ring_num_for_index(soc->wlan_cfg_ctx, i) ==
		    INVALID_WBM_RING_NUM)
			continue;

		hal_get_sw_hptp(hal_soc, soc->tx_comp_ring[i].hal_srng,
				&tp, &hp);

		if (hp != tp) {
			dp_info_rl("Pending transactions in TX comp Ring[%d] hp=0x%x, tp=0x%x",
				   i, hp, tp);
			return true;
		}
	}

	return false;
}

/**
 * dp_tx_comp_delay_check() - calculate time latency for tx completion per pkt
 * @tx_desc: tx descriptor
 *
 * Calculate time latency for tx completion per pkt and trigger self recovery
 * when the delay is more than threshold value.
 *
 * Return: True if delay is more than threshold
 */
static bool dp_tx_comp_delay_check(struct dp_tx_desc_s *tx_desc)
{
	uint64_t time_latency, timestamp_tick = tx_desc->timestamp_tick;
	qdf_ktime_t current_time = qdf_ktime_real_get();
	qdf_ktime_t timestamp = tx_desc->timestamp;

	if (dp_tx_pkt_tracepoints_enabled()) {
		if (!timestamp)
			return false;

		time_latency = qdf_ktime_to_ms(current_time) -
				qdf_ktime_to_ms(timestamp);
		if (time_latency >= DP_TX_COMP_MAX_LATENCY_MS) {
			dp_err_rl("enqueued: %llu ms, current : %llu ms",
				  timestamp, current_time);
			return true;
		}
	} else {
		if (!timestamp_tick)
			return false;

		current_time = qdf_system_ticks();
		time_latency = qdf_system_ticks_to_msecs(current_time -
							 timestamp_tick);
		if (time_latency >= DP_TX_COMP_MAX_LATENCY_MS) {
			dp_err_rl("enqueued: %u ms, current : %u ms",
				  qdf_system_ticks_to_msecs(timestamp_tick),
				  qdf_system_ticks_to_msecs(current_time));
			return true;
		}
	}

	return false;
}

void dp_find_missing_tx_comp(struct dp_soc *soc)
{
	uint8_t i;
	uint32_t j;
	uint32_t num_desc, page_id, offset;
	uint16_t num_desc_per_page;
	struct dp_tx_desc_s *tx_desc = NULL;
	struct dp_tx_desc_pool_s *tx_desc_pool = NULL;

	if (dp_check_pending_tx(soc))
		return;

	for (i = 0; i < MAX_TXDESC_POOLS; i++) {
		tx_desc_pool = &soc->tx_desc[i];
		if (!(tx_desc_pool->pool_size) ||
		    IS_TX_DESC_POOL_STATUS_INACTIVE(tx_desc_pool) ||
		    !(tx_desc_pool->desc_pages.cacheable_pages))
			continue;

		num_desc = tx_desc_pool->pool_size;
		num_desc_per_page =
			tx_desc_pool->desc_pages.num_element_per_page;
		for (j = 0; j < num_desc; j++) {
			page_id = j / num_desc_per_page;
			offset = j % num_desc_per_page;

			if (qdf_unlikely(!(tx_desc_pool->
					 desc_pages.cacheable_pages)))
				break;

			tx_desc = dp_tx_desc_find(soc, i, page_id, offset,
						  false);
			if (tx_desc->magic == DP_TX_MAGIC_PATTERN_FREE) {
				continue;
			} else if (tx_desc->magic ==
				   DP_TX_MAGIC_PATTERN_INUSE) {
				if (dp_tx_comp_delay_check(tx_desc)) {
					dp_err_rl("Tx completion not rcvd for id: %u",
						  tx_desc->id);
					if (tx_desc->vdev_id == DP_INVALID_VDEV_ID) {
						tx_desc->flags |= DP_TX_DESC_FLAG_FLUSH;
						dp_err_rl("Freed tx_desc %u",
							  tx_desc->id);
						dp_tx_comp_free_buf(soc,
								    tx_desc,
								    false);
						dp_tx_desc_release(soc, tx_desc,
								   i);
						DP_STATS_INC(soc,
							     tx.tx_comp_force_freed, 1);
					}
				}
			} else {
				dp_err_rl("tx desc %u corrupted, flags: 0x%x",
					  tx_desc->id, tx_desc->flags);
			}
		}
	}
}
#else
inline void dp_find_missing_tx_comp(struct dp_soc *soc)
{
}
#endif

#ifdef FEATURE_RUNTIME_PM
/**
 * dp_runtime_suspend() - ensure DP is ready to runtime suspend
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 *
 * DP is ready to runtime suspend if there are no pending TX packets.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_runtime_suspend(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;
	int32_t tx_pending;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Abort if there are any pending TX packets */
	tx_pending = dp_get_tx_pending(dp_pdev_to_cdp_pdev(pdev));
	if (tx_pending) {
		dp_info_rl("%pK: Abort suspend due to pending TX packets %d",
			   soc, tx_pending);
		dp_find_missing_tx_comp(soc);
		/* perform a force flush if tx is pending */
		soc->arch_ops.dp_update_ring_hptp(soc, true);
		qdf_atomic_set(&soc->tx_pending_rtpm, 0);

		return QDF_STATUS_E_AGAIN;
	}

	if (dp_runtime_get_refcount(soc)) {
		dp_init_info("refcount: %d", dp_runtime_get_refcount(soc));

		return QDF_STATUS_E_AGAIN;
	}

	if (soc->intr_mode == DP_INTR_POLL)
		qdf_timer_stop(&soc->int_timer);

	return QDF_STATUS_SUCCESS;
}

#define DP_FLUSH_WAIT_CNT 10
#define DP_RUNTIME_SUSPEND_WAIT_MS 10
/**
 * dp_runtime_resume() - ensure DP is ready to runtime resume
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 *
 * Resume DP for runtime PM.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_runtime_resume(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	int suspend_wait = 0;

	if (soc->intr_mode == DP_INTR_POLL)
		qdf_timer_mod(&soc->int_timer, DP_INTR_POLL_TIMER_MS);

	/*
	 * Wait until dp runtime refcount becomes zero or time out, then flush
	 * pending tx for runtime suspend.
	 */
	while (dp_runtime_get_refcount(soc) &&
	       suspend_wait < DP_FLUSH_WAIT_CNT) {
		qdf_sleep(DP_RUNTIME_SUSPEND_WAIT_MS);
		suspend_wait++;
	}

	soc->arch_ops.dp_update_ring_hptp(soc, false);
	qdf_atomic_set(&soc->tx_pending_rtpm, 0);

	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_RUNTIME_PM */

/**
 * dp_tx_get_success_ack_stats() - get tx success completion count
 * @soc_hdl: Datapath soc handle
 * @vdev_id: vdev identifier
 *
 * Return: tx success ack count
 */
static uint32_t dp_tx_get_success_ack_stats(struct cdp_soc_t *soc_hdl,
					    uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct cdp_vdev_stats *vdev_stats = NULL;
	uint32_t tx_success;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev) {
		dp_cdp_err("%pK: Invalid vdev id %d", soc, vdev_id);
		return 0;
	}

	vdev_stats = qdf_mem_malloc_atomic(sizeof(struct cdp_vdev_stats));
	if (!vdev_stats) {
		dp_cdp_err("%pK: DP alloc failure - unable to get alloc vdev stats", soc);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return 0;
	}

	dp_aggregate_vdev_stats(vdev, vdev_stats, DP_XMIT_TOTAL);

	tx_success = vdev_stats->tx.tx_success.num;
	qdf_mem_free(vdev_stats);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return tx_success;
}

#ifdef WLAN_SUPPORT_DATA_STALL
/**
 * dp_register_data_stall_detect_cb() - register data stall callback
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @data_stall_detect_callback: data stall callback function
 *
 * Return: QDF_STATUS Enumeration
 */
static
QDF_STATUS dp_register_data_stall_detect_cb(
			struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			data_stall_detect_cb data_stall_detect_callback)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev NULL!");
		return QDF_STATUS_E_INVAL;
	}

	pdev->data_stall_detect_callback = data_stall_detect_callback;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_deregister_data_stall_detect_cb() - de-register data stall callback
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @data_stall_detect_callback: data stall callback function
 *
 * Return: QDF_STATUS Enumeration
 */
static
QDF_STATUS dp_deregister_data_stall_detect_cb(
			struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			data_stall_detect_cb data_stall_detect_callback)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev NULL!");
		return QDF_STATUS_E_INVAL;
	}

	pdev->data_stall_detect_callback = NULL;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_txrx_post_data_stall_event() - post data stall event
 * @soc_hdl: Datapath soc handle
 * @indicator: Module triggering data stall
 * @data_stall_type: data stall event type
 * @pdev_id: pdev id
 * @vdev_id_bitmap: vdev id bitmap
 * @recovery_type: data stall recovery type
 *
 * Return: None
 */
static void
dp_txrx_post_data_stall_event(struct cdp_soc_t *soc_hdl,
			      enum data_stall_log_event_indicator indicator,
			      enum data_stall_log_event_type data_stall_type,
			      uint32_t pdev_id, uint32_t vdev_id_bitmap,
			      enum data_stall_log_recovery_type recovery_type)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct data_stall_event_info data_stall_info;
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev NULL!");
		return;
	}

	if (!pdev->data_stall_detect_callback) {
		dp_err("data stall cb not registered!");
		return;
	}

	dp_info("data_stall_type: %x pdev_id: %d",
		data_stall_type, pdev_id);

	data_stall_info.indicator = indicator;
	data_stall_info.data_stall_type = data_stall_type;
	data_stall_info.vdev_id_bitmap = vdev_id_bitmap;
	data_stall_info.pdev_id = pdev_id;
	data_stall_info.recovery_type = recovery_type;

	pdev->data_stall_detect_callback(&data_stall_info);
}
#endif /* WLAN_SUPPORT_DATA_STALL */

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * dp_txrx_ext_stats_request() - request dp txrx extended stats request
 * @soc_hdl: soc handle
 * @pdev_id: pdev id
 * @req: stats request
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_txrx_ext_stats_request(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			  struct cdp_txrx_ext_stats *req)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	int i = 0;
	int tcl_ring_full = 0;

	if (!pdev) {
		dp_err("pdev is null");
		return QDF_STATUS_E_INVAL;
	}

	dp_aggregate_pdev_stats(pdev);

	for(i = 0 ; i < MAX_TCL_DATA_RINGS; i++)
		tcl_ring_full += soc->stats.tx.tcl_ring_full[i];

	req->tx_msdu_enqueue = pdev->stats.tx_i.processed.num;
	req->tx_msdu_overflow = tcl_ring_full;
	/* Error rate at LMAC */
	req->rx_mpdu_received = soc->ext_stats.rx_mpdu_received +
				pdev->stats.err.fw_reported_rxdma_error;
	/* only count error source from RXDMA */
	req->rx_mpdu_error = pdev->stats.err.fw_reported_rxdma_error;

	/* Error rate at above the MAC */
	req->rx_mpdu_delivered = soc->ext_stats.rx_mpdu_received;
	req->rx_mpdu_missed = pdev->stats.err.reo_error;

	dp_info("ext stats: tx_msdu_enq = %u, tx_msdu_overflow = %u, "
		"rx_mpdu_receive = %u, rx_mpdu_delivered = %u, "
		"rx_mpdu_missed = %u, rx_mpdu_error = %u",
		req->tx_msdu_enqueue,
		req->tx_msdu_overflow,
		req->rx_mpdu_received,
		req->rx_mpdu_delivered,
		req->rx_mpdu_missed,
		req->rx_mpdu_error);

	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_STATS_EXT */

#ifdef WLAN_FEATURE_MARK_FIRST_WAKEUP_PACKET
/**
 * dp_mark_first_wakeup_packet() - set flag to indicate that
 *    fw is compatible for marking first packet after wow wakeup
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @value: 1 for enabled/ 0 for disabled
 *
 * Return: None
 */
static void dp_mark_first_wakeup_packet(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id, uint8_t value)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL");
		return;
	}

	pdev->is_first_wakeup_packet = value;
}
#endif

#ifdef WLAN_FEATURE_PEER_TXQ_FLUSH_CONF
/**
 * dp_set_peer_txq_flush_config() - Set the peer txq flush configuration
 * @soc_hdl: Opaque handle to the DP soc object
 * @vdev_id: VDEV identifier
 * @mac: MAC address of the peer
 * @ac: access category mask
 * @tid: TID mask
 * @policy: Flush policy
 *
 * Return: 0 on success, errno on failure
 */
static int dp_set_peer_txq_flush_config(struct cdp_soc_t *soc_hdl,
					uint8_t vdev_id, uint8_t *mac,
					uint8_t ac, uint32_t tid,
					enum cdp_peer_txq_flush_policy policy)
{
	struct dp_soc *soc;

	if (!soc_hdl) {
		dp_err("soc is null");
		return -EINVAL;
	}
	soc = cdp_soc_t_to_dp_soc(soc_hdl);
	return target_if_peer_txq_flush_config(soc->ctrl_psoc, vdev_id,
					       mac, ac, tid, policy);
}
#endif

#ifdef CONNECTIVITY_PKTLOG
/**
 * dp_register_packetdump_callback() - registers
 *  tx data packet, tx mgmt. packet and rx data packet
 *  dump callback handler.
 *
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @dp_tx_packetdump_cb: tx packetdump cb
 * @dp_rx_packetdump_cb: rx packetdump cb
 *
 * This function is used to register tx data pkt, tx mgmt.
 * pkt and rx data pkt dump callback
 *
 * Return: None
 *
 */
static inline
void dp_register_packetdump_callback(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				     ol_txrx_pktdump_cb dp_tx_packetdump_cb,
				     ol_txrx_pktdump_cb dp_rx_packetdump_cb)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL!");
		return;
	}

	pdev->dp_tx_packetdump_cb = dp_tx_packetdump_cb;
	pdev->dp_rx_packetdump_cb = dp_rx_packetdump_cb;
}

/**
 * dp_deregister_packetdump_callback() - deregidters
 *  tx data packet, tx mgmt. packet and rx data packet
 *  dump callback handler
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 *
 * This function is used to deregidter tx data pkt.,
 * tx mgmt. pkt and rx data pkt. dump callback
 *
 * Return: None
 *
 */
static inline
void dp_deregister_packetdump_callback(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL!");
		return;
	}

	pdev->dp_tx_packetdump_cb = NULL;
	pdev->dp_rx_packetdump_cb = NULL;
}
#endif

#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * dp_set_bus_vote_lvl_high() - Take a vote on bus bandwidth from dp
 * @soc_hdl: Datapath soc handle
 * @high: whether the bus bw is high or not
 *
 * Return: void
 */
static void
dp_set_bus_vote_lvl_high(ol_txrx_soc_handle soc_hdl, bool high)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	soc->high_throughput = high;
}

/**
 * dp_get_bus_vote_lvl_high() - get bus bandwidth vote to dp
 * @soc_hdl: Datapath soc handle
 *
 * Return: bool
 */
static bool
dp_get_bus_vote_lvl_high(ol_txrx_soc_handle soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	return soc->high_throughput;
}
#endif

#ifdef DP_PEER_EXTENDED_API
static struct cdp_misc_ops dp_ops_misc = {
#ifdef FEATURE_WLAN_TDLS
	.tx_non_std = dp_tx_non_std,
#endif /* FEATURE_WLAN_TDLS */
	.get_opmode = dp_get_opmode,
#ifdef FEATURE_RUNTIME_PM
	.runtime_suspend = dp_runtime_suspend,
	.runtime_resume = dp_runtime_resume,
#endif /* FEATURE_RUNTIME_PM */
	.get_num_rx_contexts = dp_get_num_rx_contexts,
	.get_tx_ack_stats = dp_tx_get_success_ack_stats,
#ifdef WLAN_SUPPORT_DATA_STALL
	.txrx_data_stall_cb_register = dp_register_data_stall_detect_cb,
	.txrx_data_stall_cb_deregister = dp_deregister_data_stall_detect_cb,
	.txrx_post_data_stall_event = dp_txrx_post_data_stall_event,
#endif

#ifdef WLAN_FEATURE_STATS_EXT
	.txrx_ext_stats_request = dp_txrx_ext_stats_request,
#ifndef WLAN_SOFTUMAC_SUPPORT
	.request_rx_hw_stats = dp_request_rx_hw_stats,
	.reset_rx_hw_ext_stats = dp_reset_rx_hw_ext_stats,
#endif
#endif /* WLAN_FEATURE_STATS_EXT */
	.vdev_inform_ll_conn = dp_vdev_inform_ll_conn,
#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
	.set_swlm_enable = dp_soc_set_swlm_enable,
	.is_swlm_enabled = dp_soc_is_swlm_enabled,
#endif
	.display_txrx_hw_info = dp_display_srng_info,
#ifndef WLAN_SOFTUMAC_SUPPORT
	.get_tx_rings_grp_bitmap = dp_get_tx_rings_grp_bitmap,
#endif
#ifdef WLAN_FEATURE_MARK_FIRST_WAKEUP_PACKET
	.mark_first_wakeup_packet = dp_mark_first_wakeup_packet,
#endif
#ifdef WLAN_FEATURE_PEER_TXQ_FLUSH_CONF
	.set_peer_txq_flush_config = dp_set_peer_txq_flush_config,
#endif
#ifdef CONNECTIVITY_PKTLOG
	.register_pktdump_cb = dp_register_packetdump_callback,
	.unregister_pktdump_cb = dp_deregister_packetdump_callback,
#endif
#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
	.set_bus_vote_lvl_high = dp_set_bus_vote_lvl_high,
	.get_bus_vote_lvl_high = dp_get_bus_vote_lvl_high,
#endif
#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
	.evaluate_update_tx_ilp_cfg = dp_evaluate_update_tx_ilp_config,
#endif
};
#endif

#ifdef DP_FLOW_CTL
static struct cdp_flowctl_ops dp_ops_flowctl = {
	/* WIFI 3.0 DP implement as required. */
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
#ifndef WLAN_SOFTUMAC_SUPPORT
	.flow_pool_map_handler = dp_tx_flow_pool_map,
	.flow_pool_unmap_handler = dp_tx_flow_pool_unmap,
#endif /*WLAN_SOFTUMAC_SUPPORT */
	.register_pause_cb = dp_txrx_register_pause_cb,
	.dump_flow_pool_info = dp_tx_dump_flow_pool_info,
	.tx_desc_thresh_reached = dp_tx_desc_thresh_reached,
#endif /* QCA_LL_TX_FLOW_CONTROL_V2 */
};

static struct cdp_lflowctl_ops dp_ops_l_flowctl = {
	/* WIFI 3.0 DP NOT IMPLEMENTED YET */
};
#endif

#ifdef IPA_OFFLOAD
static struct cdp_ipa_ops dp_ops_ipa = {
	.ipa_get_resource = dp_ipa_get_resource,
	.ipa_set_doorbell_paddr = dp_ipa_set_doorbell_paddr,
	.ipa_iounmap_doorbell_vaddr = dp_ipa_iounmap_doorbell_vaddr,
	.ipa_op_response = dp_ipa_op_response,
	.ipa_register_op_cb = dp_ipa_register_op_cb,
	.ipa_deregister_op_cb = dp_ipa_deregister_op_cb,
	.ipa_get_stat = dp_ipa_get_stat,
	.ipa_tx_data_frame = dp_tx_send_ipa_data_frame,
	.ipa_enable_autonomy = dp_ipa_enable_autonomy,
	.ipa_disable_autonomy = dp_ipa_disable_autonomy,
	.ipa_setup = dp_ipa_setup,
	.ipa_cleanup = dp_ipa_cleanup,
	.ipa_setup_iface = dp_ipa_setup_iface,
	.ipa_cleanup_iface = dp_ipa_cleanup_iface,
	.ipa_enable_pipes = dp_ipa_enable_pipes,
	.ipa_disable_pipes = dp_ipa_disable_pipes,
	.ipa_set_perf_level = dp_ipa_set_perf_level,
	.ipa_rx_intrabss_fwd = dp_ipa_rx_intrabss_fwd,
	.ipa_tx_buf_smmu_mapping = dp_ipa_tx_buf_smmu_mapping,
	.ipa_tx_buf_smmu_unmapping = dp_ipa_tx_buf_smmu_unmapping,
	.ipa_rx_buf_smmu_pool_mapping = dp_ipa_rx_buf_pool_smmu_mapping,
	.ipa_set_smmu_mapped = dp_ipa_set_smmu_mapped,
	.ipa_get_smmu_mapped = dp_ipa_get_smmu_mapped,
#ifdef QCA_SUPPORT_WDS_EXTENDED
	.ipa_rx_wdsext_iface = dp_ipa_rx_wdsext_iface,
#endif
#ifdef QCA_ENHANCED_STATS_SUPPORT
	.ipa_update_peer_rx_stats = dp_ipa_update_peer_rx_stats,
#endif
#ifdef IPA_OPT_WIFI_DP
	.ipa_rx_super_rule_setup = dp_ipa_rx_super_rule_setup,
	.ipa_pcie_link_up = dp_ipa_pcie_link_up,
	.ipa_pcie_link_down = dp_ipa_pcie_link_down,
#endif
#ifdef IPA_WDS_EASYMESH_FEATURE
	.ipa_ast_create = dp_ipa_ast_create,
#endif
	.ipa_get_wdi_version = dp_ipa_get_wdi_version,
};
#endif

#ifdef DP_POWER_SAVE
static QDF_STATUS dp_bus_suspend(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	int timeout = SUSPEND_DRAIN_WAIT;
	int drain_wait_delay = 50; /* 50 ms */
	int32_t tx_pending;

	if (qdf_unlikely(!pdev)) {
		dp_err("pdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Abort if there are any pending TX packets */
	while ((tx_pending = dp_get_tx_pending((struct cdp_pdev *)pdev))) {
		qdf_sleep(drain_wait_delay);
		if (timeout <= 0) {
			dp_info("TX frames are pending %d, abort suspend",
				tx_pending);
			dp_find_missing_tx_comp(soc);
			return QDF_STATUS_E_TIMEOUT;
		}
		timeout = timeout - drain_wait_delay;
	}

	if (soc->intr_mode == DP_INTR_POLL)
		qdf_timer_stop(&soc->int_timer);

	/* Stop monitor reap timer and reap any pending frames in ring */
	dp_monitor_reap_timer_suspend(soc);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_bus_resume(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (qdf_unlikely(!pdev)) {
		dp_err("pdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (soc->intr_mode == DP_INTR_POLL)
		qdf_timer_mod(&soc->int_timer, DP_INTR_POLL_TIMER_MS);

	/* Start monitor reap timer */
	dp_monitor_reap_timer_start(soc, CDP_MON_REAP_SOURCE_ANY);

	soc->arch_ops.dp_update_ring_hptp(soc, false);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_process_wow_ack_rsp() - process wow ack response
 * @soc_hdl: datapath soc handle
 * @pdev_id: data path pdev handle id
 *
 * Return: none
 */
static void dp_process_wow_ack_rsp(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (qdf_unlikely(!pdev)) {
		dp_err("pdev is NULL");
		return;
	}

	/*
	 * As part of wow enable FW disables the mon status ring and in wow ack
	 * response from FW reap mon status ring to make sure no packets pending
	 * in the ring.
	 */
	dp_monitor_reap_timer_suspend(soc);
}

/**
 * dp_process_target_suspend_req() - process target suspend request
 * @soc_hdl: datapath soc handle
 * @pdev_id: data path pdev handle id
 *
 * Return: none
 */
static void dp_process_target_suspend_req(struct cdp_soc_t *soc_hdl,
					  uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (qdf_unlikely(!pdev)) {
		dp_err("pdev is NULL");
		return;
	}

	/* Stop monitor reap timer and reap any pending frames in ring */
	dp_monitor_reap_timer_suspend(soc);
}

static struct cdp_bus_ops dp_ops_bus = {
	.bus_suspend = dp_bus_suspend,
	.bus_resume = dp_bus_resume,
	.process_wow_ack_rsp = dp_process_wow_ack_rsp,
	.process_target_suspend_req = dp_process_target_suspend_req
};
#endif

#ifdef DP_FLOW_CTL
static struct cdp_throttle_ops dp_ops_throttle = {
	/* WIFI 3.0 DP NOT IMPLEMENTED YET */
};

static struct cdp_cfg_ops dp_ops_cfg = {
	/* WIFI 3.0 DP NOT IMPLEMENTED YET */
};
#endif

#ifdef DP_PEER_EXTENDED_API
static struct cdp_ocb_ops dp_ops_ocb = {
	/* WIFI 3.0 DP NOT IMPLEMENTED YET */
};

static struct cdp_mob_stats_ops dp_ops_mob_stats = {
	.clear_stats = dp_txrx_clear_dump_stats,
};

static struct cdp_peer_ops dp_ops_peer = {
	.register_peer = dp_register_peer,
	.clear_peer = dp_clear_peer,
	.find_peer_exist = dp_find_peer_exist,
	.find_peer_exist_on_vdev = dp_find_peer_exist_on_vdev,
	.find_peer_exist_on_other_vdev = dp_find_peer_exist_on_other_vdev,
	.peer_state_update = dp_peer_state_update,
	.get_vdevid = dp_get_vdevid,
	.get_vdev_by_peer_addr = dp_get_vdev_by_peer_addr,
	.peer_get_peer_mac_addr = dp_peer_get_peer_mac_addr,
	.get_peer_state = dp_get_peer_state,
	.peer_flush_frags = dp_peer_flush_frags,
	.set_peer_as_tdls_peer = dp_set_peer_as_tdls_peer,
};
#endif

static void dp_soc_txrx_ops_attach(struct dp_soc *soc)
{
	soc->cdp_soc.ops->cmn_drv_ops = &dp_ops_cmn;
	soc->cdp_soc.ops->ctrl_ops = &dp_ops_ctrl;
	soc->cdp_soc.ops->me_ops = &dp_ops_me;
	soc->cdp_soc.ops->host_stats_ops = &dp_ops_host_stats;
	soc->cdp_soc.ops->wds_ops = &dp_ops_wds;
	soc->cdp_soc.ops->raw_ops = &dp_ops_raw;
#ifdef PEER_FLOW_CONTROL
	soc->cdp_soc.ops->pflow_ops = &dp_ops_pflow;
#endif /* PEER_FLOW_CONTROL */
#ifdef DP_PEER_EXTENDED_API
	soc->cdp_soc.ops->misc_ops = &dp_ops_misc;
	soc->cdp_soc.ops->ocb_ops = &dp_ops_ocb;
	soc->cdp_soc.ops->peer_ops = &dp_ops_peer;
	soc->cdp_soc.ops->mob_stats_ops = &dp_ops_mob_stats;
#endif
#ifdef DP_FLOW_CTL
	soc->cdp_soc.ops->cfg_ops = &dp_ops_cfg;
	soc->cdp_soc.ops->flowctl_ops = &dp_ops_flowctl;
	soc->cdp_soc.ops->l_flowctl_ops = &dp_ops_l_flowctl;
	soc->cdp_soc.ops->throttle_ops = &dp_ops_throttle;
#endif
#ifdef IPA_OFFLOAD
	soc->cdp_soc.ops->ipa_ops = &dp_ops_ipa;
#endif
#ifdef DP_POWER_SAVE
	soc->cdp_soc.ops->bus_ops = &dp_ops_bus;
#endif
#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
	soc->cdp_soc.ops->cfr_ops = &dp_ops_cfr;
#endif
#ifdef WLAN_SUPPORT_MSCS
	soc->cdp_soc.ops->mscs_ops = &dp_ops_mscs;
#endif
#ifdef WLAN_SUPPORT_MESH_LATENCY
	soc->cdp_soc.ops->mesh_latency_ops = &dp_ops_mesh_latency;
#endif
#ifdef CONFIG_SAWF_DEF_QUEUES
	soc->cdp_soc.ops->sawf_ops = &dp_ops_sawf;
#endif
#ifdef WLAN_SUPPORT_SCS
	soc->cdp_soc.ops->scs_ops = &dp_ops_scs;
#endif
#ifdef WLAN_SUPPORT_RX_FLOW_TAG
	soc->cdp_soc.ops->fse_ops = &dp_ops_fse;
#endif
};

#if defined(QCA_WIFI_QCA8074) || defined(QCA_WIFI_QCA6018) || \
	defined(QCA_WIFI_QCA5018) || defined(QCA_WIFI_QCA9574) || \
	defined(QCA_WIFI_QCA5332)

/**
 * dp_soc_attach_wifi3() - Attach txrx SOC
 * @ctrl_psoc: Opaque SOC handle from control plane
 * @params: SOC attach params
 *
 * Return: DP SOC handle on success, NULL on failure
 */
struct cdp_soc_t *
dp_soc_attach_wifi3(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
		    struct cdp_soc_attach_params *params)
{
	struct dp_soc *dp_soc = NULL;

	dp_soc = dp_soc_attach(ctrl_psoc, params);

	return dp_soc_to_cdp_soc_t(dp_soc);
}

static inline void dp_soc_set_def_pdev(struct dp_soc *soc)
{
	int lmac_id;

	for (lmac_id = 0; lmac_id < MAX_NUM_LMAC_HW; lmac_id++) {
		/*Set default host PDEV ID for lmac_id*/
		wlan_cfg_set_pdev_idx(soc->wlan_cfg_ctx,
				      INVALID_PDEV_ID, lmac_id);
	}
}

static void dp_soc_unset_qref_debug_list(struct dp_soc *soc)
{
	uint32_t max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	if (max_list_size == 0)
		return;

	qdf_mem_free(soc->list_shared_qaddr_del);
	qdf_mem_free(soc->reo_write_list);
	qdf_mem_free(soc->list_qdesc_addr_free);
	qdf_mem_free(soc->list_qdesc_addr_alloc);
}

static void dp_soc_set_qref_debug_list(struct dp_soc *soc)
{
	uint32_t max_list_size = soc->wlan_cfg_ctx->qref_control_size;

	if (max_list_size == 0)
		return;

	soc->list_shared_qaddr_del =
			(struct test_qaddr_del *)
				qdf_mem_malloc(sizeof(struct test_qaddr_del) *
					       max_list_size);
	soc->reo_write_list =
			(struct test_qaddr_del *)
				qdf_mem_malloc(sizeof(struct test_qaddr_del) *
					       max_list_size);
	soc->list_qdesc_addr_free =
			(struct test_mem_free *)
				qdf_mem_malloc(sizeof(struct test_mem_free) *
					       max_list_size);
	soc->list_qdesc_addr_alloc =
			(struct test_mem_free *)
				qdf_mem_malloc(sizeof(struct test_mem_free) *
					       max_list_size);
}

static uint32_t
dp_get_link_desc_id_start(uint16_t arch_id)
{
	switch (arch_id) {
	case CDP_ARCH_TYPE_LI:
	case CDP_ARCH_TYPE_RH:
		return LINK_DESC_ID_START_21_BITS_COOKIE;
	case CDP_ARCH_TYPE_BE:
		return LINK_DESC_ID_START_20_BITS_COOKIE;
	default:
		dp_err("unknown arch_id 0x%x", arch_id);
		QDF_BUG(0);
		return LINK_DESC_ID_START_21_BITS_COOKIE;
	}
}

#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
static inline
void dp_soc_init_tx_ilp(struct dp_soc *soc)
{
	soc->tx_ilp_enable = false;
}
#else
static inline
void dp_soc_init_tx_ilp(struct dp_soc *soc)
{
}
#endif

/**
 * dp_soc_attach() - Attach txrx SOC
 * @ctrl_psoc: Opaque SOC handle from control plane
 * @params: SOC attach params
 *
 * Return: DP SOC handle on success, NULL on failure
 */
static struct dp_soc *
dp_soc_attach(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
	      struct cdp_soc_attach_params *params)
{
	struct dp_soc *soc =  NULL;
	uint16_t arch_id;
	struct hif_opaque_softc *hif_handle = params->hif_handle;
	qdf_device_t qdf_osdev = params->qdf_osdev;
	struct ol_if_ops *ol_ops = params->ol_ops;
	uint16_t device_id = params->device_id;

	if (!hif_handle) {
		dp_err("HIF handle is NULL");
		goto fail0;
	}
	arch_id = cdp_get_arch_type_from_devid(device_id);
	soc = qdf_mem_common_alloc(dp_get_soc_context_size(device_id));
	if (!soc) {
		dp_err("DP SOC memory allocation failed");
		goto fail0;
	}

	dp_info("soc memory allocated %pK", soc);
	soc->hif_handle = hif_handle;
	soc->hal_soc = hif_get_hal_handle(soc->hif_handle);
	if (!soc->hal_soc)
		goto fail1;

	hif_get_cmem_info(soc->hif_handle,
			  &soc->cmem_base,
			  &soc->cmem_total_size);
	soc->cmem_avail_size = soc->cmem_total_size;
	soc->device_id = device_id;
	soc->cdp_soc.ops =
		(struct cdp_ops *)qdf_mem_malloc(sizeof(struct cdp_ops));
	if (!soc->cdp_soc.ops)
		goto fail1;

	dp_soc_txrx_ops_attach(soc);
	soc->cdp_soc.ol_ops = ol_ops;
	soc->ctrl_psoc = ctrl_psoc;
	soc->osdev = qdf_osdev;
	soc->num_hw_dscp_tid_map = HAL_MAX_HW_DSCP_TID_MAPS;
	dp_soc_init_tx_ilp(soc);
	hal_rx_get_tlv_size(soc->hal_soc, &soc->rx_pkt_tlv_size,
			    &soc->rx_mon_pkt_tlv_size);
	soc->idle_link_bm_id = hal_get_idle_link_bm_id(soc->hal_soc,
						       params->mlo_chip_id);
	soc->features.dmac_cmn_src_rxbuf_ring_enabled =
		hal_dmac_cmn_src_rxbuf_ring_get(soc->hal_soc);
	soc->arch_id = arch_id;
	soc->link_desc_id_start =
			dp_get_link_desc_id_start(soc->arch_id);
	dp_configure_arch_ops(soc);

	/* Reset wbm sg list and flags */
	dp_rx_wbm_sg_list_reset(soc);

	dp_soc_cfg_history_attach(soc);
	dp_soc_tx_hw_desc_history_attach(soc);
	dp_soc_rx_history_attach(soc);
	dp_soc_mon_status_ring_history_attach(soc);
	dp_soc_tx_history_attach(soc);
	dp_soc_msdu_done_fail_desc_list_attach(soc);
	dp_soc_msdu_done_fail_history_attach(soc);
	wlan_set_srng_cfg(&soc->wlan_srng_cfg);
	soc->wlan_cfg_ctx = wlan_cfg_soc_attach(soc->ctrl_psoc);
	if (!soc->wlan_cfg_ctx) {
		dp_err("wlan_cfg_ctx failed");
		goto fail2;
	}

	qdf_ssr_driver_dump_register_region("wlan_cfg_ctx", soc->wlan_cfg_ctx,
					    sizeof(*soc->wlan_cfg_ctx));

	/*sync DP soc cfg items with profile support after cfg_soc_attach*/
	wlan_dp_soc_cfg_sync_profile((struct cdp_soc_t *)soc);

	soc->arch_ops.soc_cfg_attach(soc);

	qdf_ssr_driver_dump_register_region("tcl_wbm_map_array",
					    &soc->wlan_cfg_ctx->tcl_wbm_map_array,
					    sizeof(struct wlan_cfg_tcl_wbm_ring_num_map));

	if (dp_hw_link_desc_pool_banks_alloc(soc, WLAN_INVALID_PDEV_ID)) {
		dp_err("failed to allocate link desc pool banks");
		goto fail3;
	}

	if (dp_hw_link_desc_ring_alloc(soc)) {
		dp_err("failed to allocate link_desc_ring");
		goto fail4;
	}

	if (!QDF_IS_STATUS_SUCCESS(soc->arch_ops.txrx_soc_attach(soc,
								 params))) {
		dp_err("unable to do target specific attach");
		goto fail5;
	}

	if (dp_soc_srng_alloc(soc)) {
		dp_err("failed to allocate soc srng rings");
		goto fail6;
	}

	if (dp_soc_tx_desc_sw_pools_alloc(soc)) {
		dp_err("dp_soc_tx_desc_sw_pools_alloc failed");
		goto fail7;
	}

	if (!dp_monitor_modularized_enable()) {
		if (dp_mon_soc_attach_wrapper(soc)) {
			dp_err("failed to attach monitor");
			goto fail8;
		}
	}

	if (hal_reo_shared_qaddr_setup((hal_soc_handle_t)soc->hal_soc,
				       &soc->reo_qref)
	    != QDF_STATUS_SUCCESS) {
		dp_err("unable to setup reo shared qaddr");
		goto fail9;
	}

	if (dp_sysfs_initialize_stats(soc) != QDF_STATUS_SUCCESS) {
		dp_err("failed to initialize dp stats sysfs file");
		dp_sysfs_deinitialize_stats(soc);
	}

	wlan_dp_lapb_flow_attach(soc);
	dp_soc_swlm_attach(soc);
	dp_soc_set_interrupt_mode(soc);
	dp_soc_set_def_pdev(soc);
	dp_soc_set_qref_debug_list(soc);
	qdf_ssr_driver_dump_register_region("dp_soc", soc, sizeof(*soc));
	qdf_nbuf_ssr_register_region();

	dp_info("Mem stats: DMA = %u HEAP = %u SKB = %u",
		qdf_dma_mem_stats_read(),
		qdf_heap_mem_stats_read(),
		qdf_skb_total_mem_stats_read());

	return soc;
fail9:
	if (!dp_monitor_modularized_enable())
		dp_mon_soc_detach_wrapper(soc);
fail8:
	dp_soc_tx_desc_sw_pools_free(soc);
fail7:
	dp_soc_srng_free(soc);
fail6:
	soc->arch_ops.txrx_soc_detach(soc);
fail5:
	dp_hw_link_desc_ring_free(soc);
fail4:
	dp_hw_link_desc_pool_banks_free(soc, WLAN_INVALID_PDEV_ID);
fail3:
	wlan_cfg_soc_detach(soc->wlan_cfg_ctx);
fail2:
	dp_soc_msdu_done_fail_history_detach(soc);
	qdf_mem_free(soc->cdp_soc.ops);
fail1:
	qdf_mem_common_free(soc);
fail0:
	return NULL;
}

void *dp_soc_init_wifi3(struct cdp_soc_t *cdp_soc,
			struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			struct hif_opaque_softc *hif_handle,
			HTC_HANDLE htc_handle, qdf_device_t qdf_osdev,
			struct ol_if_ops *ol_ops, uint16_t device_id)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;

	return soc->arch_ops.txrx_soc_init(soc, htc_handle, hif_handle);
}

#endif

void *dp_get_pdev_for_mac_id(struct dp_soc *soc, uint32_t mac_id)
{
	if (wlan_cfg_per_pdev_lmac_ring(soc->wlan_cfg_ctx))
		return (mac_id < MAX_PDEV_CNT) ? soc->pdev_list[mac_id] : NULL;

	/* Typically for MCL as there only 1 PDEV*/
	return soc->pdev_list[0];
}

void dp_update_num_mac_rings_for_dbs(struct dp_soc *soc,
				     int *max_mac_rings)
{
	bool dbs_enable = false;

	if (soc->cdp_soc.ol_ops->is_hw_dbs_capable)
		dbs_enable = soc->cdp_soc.ol_ops->
				is_hw_dbs_capable((void *)soc->ctrl_psoc);

	*max_mac_rings = dbs_enable ? (*max_mac_rings) : 1;
	dp_info("dbs_enable %d, max_mac_rings %d",
		dbs_enable, *max_mac_rings);
}

qdf_export_symbol(dp_update_num_mac_rings_for_dbs);

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
/**
 * dp_get_cfr_rcc() - get cfr rcc config
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of objmgr pdev
 *
 * Return: true/false based on cfr mode setting
 */
static
bool dp_get_cfr_rcc(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = NULL;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL");
		return false;
	}

	return pdev->cfr_rcc_mode;
}

/**
 * dp_set_cfr_rcc() - enable/disable cfr rcc config
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of objmgr pdev
 * @enable: Enable/Disable cfr rcc mode
 *
 * Return: none
 */
static
void dp_set_cfr_rcc(struct cdp_soc_t *soc_hdl, uint8_t pdev_id, bool enable)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = NULL;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL");
		return;
	}

	pdev->cfr_rcc_mode = enable;
}

/**
 * dp_get_cfr_dbg_stats - Get the debug statistics for CFR
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @cfr_rcc_stats: CFR RCC debug statistics buffer
 *
 * Return: none
 */
static inline void
dp_get_cfr_dbg_stats(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		     struct cdp_cfr_rcc_stats *cfr_rcc_stats)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("pdev is NULL");
		return;
	}

	qdf_mem_copy(cfr_rcc_stats, &pdev->stats.rcc,
		     sizeof(struct cdp_cfr_rcc_stats));
}

/**
 * dp_clear_cfr_dbg_stats - Clear debug statistics for CFR
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 *
 * Return: none
 */
static void dp_clear_cfr_dbg_stats(struct cdp_soc_t *soc_hdl,
				   uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("dp pdev is NULL");
		return;
	}

	qdf_mem_zero(&pdev->stats.rcc, sizeof(pdev->stats.rcc));
}
#endif

/**
 * dp_bucket_index() - Return index from array
 *
 * @delay: delay measured
 * @array: array used to index corresponding delay
 * @delay_in_us: flag to indicate whether the delay in ms or us
 *
 * Return: index
 */
static uint8_t
dp_bucket_index(uint32_t delay, uint16_t *array, bool delay_in_us)
{
	uint8_t i = CDP_DELAY_BUCKET_0;
	uint32_t thr_low, thr_high;

	for (; i < CDP_DELAY_BUCKET_MAX - 1; i++) {
		thr_low = array[i];
		thr_high = array[i + 1];

		if (delay_in_us) {
			thr_low = thr_low * USEC_PER_MSEC;
			thr_high = thr_high * USEC_PER_MSEC;
		}
		if (delay >= thr_low && delay <= thr_high)
			return i;
	}
	return (CDP_DELAY_BUCKET_MAX - 1);
}

/*
 * cdp_fw_to_hw_delay_range
 * Fw to hw delay ranges in milliseconds
 */
static uint16_t cdp_fw_to_hw_delay[CDP_DELAY_BUCKET_MAX] = {
	0, 2, 4, 6, 8, 10, 20, 30, 40, 50, 100, 250, 500};

/*
 * cdp_sw_enq_delay_range
 * Software enqueue delay ranges in milliseconds
 */
static uint16_t cdp_sw_enq_delay[CDP_DELAY_BUCKET_MAX] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

/*
 * cdp_intfrm_delay_range
 * Interframe delay ranges in milliseconds
 */
static uint16_t cdp_intfrm_delay[CDP_DELAY_BUCKET_MAX] = {
	0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60};

#ifdef HW_TX_DELAY_STATS_ENABLE
static inline
void dp_check_bucket_list_overflow(struct cdp_delay_stats *delay_stat,
				   uint8_t delay_index)
{
	if (qdf_likely(delay_stat->delay_bucket[delay_index] < UINT_MAX))
		return;

	dp_debug_rl("delay_stat overflow detected: bin:%u", delay_index);
	qdf_mem_zero(delay_stat, sizeof(struct cdp_delay_stats));
}
#else
static inline
void dp_check_bucket_list_overflow(struct cdp_delay_stats *delay_stat,
				   uint8_t delay_index)
{
}
#endif

/**
 * dp_fill_delay_buckets() - Fill delay statistics bucket for each
 *				type of delay
 * @tstats: tid tx stats
 * @rstats: tid rx stats
 * @delay: delay in ms
 * @tid: tid value
 * @mode: type of tx delay mode
 * @ring_id: ring number
 * @delay_in_us: flag to indicate whether the delay in ms or us
 *
 * Return: pointer to cdp_delay_stats structure
 */
static struct cdp_delay_stats *
dp_fill_delay_buckets(struct cdp_tid_tx_stats *tstats,
		      struct cdp_tid_rx_stats *rstats, uint32_t delay,
		      uint8_t tid, uint8_t mode, uint8_t ring_id,
		      bool delay_in_us)
{
	uint8_t delay_index = 0;
	struct cdp_delay_stats *stats = NULL;

	/*
	 * Update delay stats in proper bucket
	 */
	switch (mode) {
	/* Software Enqueue delay ranges */
	case CDP_DELAY_STATS_SW_ENQ:
		if (!tstats)
			break;

		delay_index = dp_bucket_index(delay, cdp_sw_enq_delay,
					      delay_in_us);
		tstats->swq_delay.delay_bucket[delay_index]++;
		stats = &tstats->swq_delay;
		break;

	/* Tx Completion delay ranges */
	case CDP_DELAY_STATS_FW_HW_TRANSMIT:
		if (!tstats)
			break;

		delay_index = dp_bucket_index(delay, cdp_fw_to_hw_delay,
					      delay_in_us);
		stats = &tstats->hwtx_delay;
		dp_check_bucket_list_overflow(stats, delay_index);
		tstats->hwtx_delay.delay_bucket[delay_index]++;
		break;

	/* Interframe tx delay ranges */
	case CDP_DELAY_STATS_TX_INTERFRAME:
		if (!tstats)
			break;

		delay_index = dp_bucket_index(delay, cdp_intfrm_delay,
					      delay_in_us);
		tstats->intfrm_delay.delay_bucket[delay_index]++;
		stats = &tstats->intfrm_delay;
		break;

	/* Interframe rx delay ranges */
	case CDP_DELAY_STATS_RX_INTERFRAME:
		if (!rstats)
			break;

		delay_index = dp_bucket_index(delay, cdp_intfrm_delay,
					      delay_in_us);
		rstats->intfrm_delay.delay_bucket[delay_index]++;
		stats = &rstats->intfrm_delay;
		break;

	/* Ring reap to indication to network stack */
	case CDP_DELAY_STATS_REAP_STACK:
		if (!rstats)
			break;

		delay_index = dp_bucket_index(delay, cdp_intfrm_delay,
					      delay_in_us);
		rstats->to_stack_delay.delay_bucket[delay_index]++;
		stats = &rstats->to_stack_delay;
		break;
	default:
		dp_debug("Incorrect delay mode: %d", mode);
	}

	return stats;
}

void dp_update_delay_stats(struct cdp_tid_tx_stats *tstats,
			   struct cdp_tid_rx_stats *rstats, uint32_t delay,
			   uint8_t tid, uint8_t mode, uint8_t ring_id,
			   bool delay_in_us)
{
	struct cdp_delay_stats *dstats = NULL;

	/*
	 * Delay ranges are different for different delay modes
	 * Get the correct index to update delay bucket
	 */
	dstats = dp_fill_delay_buckets(tstats, rstats, delay, tid, mode,
				       ring_id, delay_in_us);
	if (qdf_unlikely(!dstats))
		return;

	if (delay != 0) {
		/*
		 * Compute minimum,average and maximum
		 * delay
		 */
		if (!dstats->min_delay || (delay < dstats->min_delay))
			dstats->min_delay = delay;

		if (delay > dstats->max_delay)
			dstats->max_delay = delay;

		/*
		 * Average over delay measured till now
		 */
		if (!dstats->avg_delay)
			dstats->avg_delay = delay;
		else
			dstats->avg_delay = ((delay + dstats->avg_delay) >> 1);
	}
}

uint16_t dp_get_peer_mac_list(ol_txrx_soc_handle soc, uint8_t vdev_id,
			      u_int8_t newmac[][QDF_MAC_ADDR_SIZE],
			      u_int16_t mac_cnt, bool limit)
{
	struct dp_soc *dp_soc = (struct dp_soc *)soc;
	struct dp_vdev *vdev =
		dp_vdev_get_ref_by_id(dp_soc, vdev_id, DP_MOD_ID_CDP);
	struct dp_peer *peer;
	uint16_t new_mac_cnt = 0;

	if (!vdev)
		return new_mac_cnt;

	if (limit && (vdev->num_peers > mac_cnt)) {
		dp_vdev_unref_delete(dp_soc, vdev, DP_MOD_ID_CDP);
		return 0;
	}

	qdf_spin_lock_bh(&vdev->peer_list_lock);
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		if (peer->bss_peer)
			continue;
		if (new_mac_cnt < mac_cnt) {
			WLAN_ADDR_COPY(newmac[new_mac_cnt], peer->mac_addr.raw);
			new_mac_cnt++;
		}
	}
	qdf_spin_unlock_bh(&vdev->peer_list_lock);
	dp_vdev_unref_delete(dp_soc, vdev, DP_MOD_ID_CDP);
	return new_mac_cnt;
}

uint16_t dp_get_peer_id(ol_txrx_soc_handle soc, uint8_t vdev_id, uint8_t *mac)
{
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)soc,
						       mac, 0, vdev_id,
						       DP_MOD_ID_CDP);
	uint16_t peer_id = HTT_INVALID_PEER;

	if (!peer) {
		dp_cdp_debug("%pK: Peer is NULL!", (struct dp_soc *)soc);
		return peer_id;
	}

	peer_id = peer->peer_id;
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return peer_id;
}

#ifdef QCA_SUPPORT_WDS_EXTENDED
QDF_STATUS dp_wds_ext_set_peer_rx(ol_txrx_soc_handle soc,
				  uint8_t vdev_id,
				  uint8_t *mac,
				  ol_txrx_rx_fp rx,
				  ol_osif_peer_handle osif_peer)
{
	struct dp_txrx_peer *txrx_peer = NULL;
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)soc,
						       mac, 0, vdev_id,
						       DP_MOD_ID_CDP);
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (!peer) {
		dp_cdp_debug("%pK: Peer is NULL!", (struct dp_soc *)soc);
		return status;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return status;
	}

	if (rx) {
		if (txrx_peer->osif_rx) {
			status = QDF_STATUS_E_ALREADY;
		} else {
			txrx_peer->osif_rx = rx;
			status = QDF_STATUS_SUCCESS;
		}
	} else {
		if (txrx_peer->osif_rx) {
			txrx_peer->osif_rx = NULL;
			status = QDF_STATUS_SUCCESS;
		} else {
			status = QDF_STATUS_E_ALREADY;
		}
	}

	txrx_peer->wds_ext.osif_peer = osif_peer;
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

QDF_STATUS dp_wds_ext_get_peer_osif_handle(
				ol_txrx_soc_handle soc,
				uint8_t vdev_id,
				uint8_t *mac,
				ol_osif_peer_handle *osif_peer)
{
	struct dp_soc *dp_soc = (struct dp_soc *)soc;
	struct dp_txrx_peer *txrx_peer = NULL;
	struct dp_peer *peer = dp_peer_find_hash_find(dp_soc,
						      mac, 0, vdev_id,
						      DP_MOD_ID_CDP);

	if (!peer) {
		dp_cdp_debug("%pK: Peer is NULL!", dp_soc);
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_cdp_debug("%pK: TXRX Peer is NULL!", dp_soc);
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_INVAL;
	}

	*osif_peer = txrx_peer->wds_ext.osif_peer;
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_wds_ext_set_peer_bit(ol_txrx_soc_handle soc, uint8_t *mac)
{
	struct dp_txrx_peer *txrx_peer = NULL;
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)soc,
						       mac, 0, DP_VDEV_ALL,
						       DP_MOD_ID_IPA);
	if (!peer) {
		dp_cdp_debug("%pK: Peer is NULL!\n", (struct dp_soc *)soc);
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_IPA);
		return QDF_STATUS_E_INVAL;
	}
	qdf_atomic_test_and_set_bit(WDS_EXT_PEER_INIT_BIT,
				    &txrx_peer->wds_ext.init);
	dp_peer_unref_delete(peer, DP_MOD_ID_IPA);

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_SUPPORT_WDS_EXTENDED */

/**
 * dp_pdev_srng_deinit() - de-initialize all pdev srng ring including
 *			   monitor rings
 * @pdev: Datapath pdev handle
 *
 */
static void dp_pdev_srng_deinit(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t i;

	if (!soc->features.dmac_cmn_src_rxbuf_ring_enabled)
		dp_srng_deinit(soc, &soc->rx_refill_buf_ring[pdev->lmac_id],
			       RXDMA_BUF,
			       pdev->lmac_id);

	if (!soc->rxdma2sw_rings_not_supported) {
		for (i = 0;
		     i < soc->wlan_cfg_ctx->num_rxdma_dst_rings_per_pdev; i++) {
			int lmac_id = dp_get_lmac_id_for_pdev_id(soc, i,
								 pdev->pdev_id);

			wlan_minidump_remove(soc->rxdma_err_dst_ring[lmac_id].
							base_vaddr_unaligned,
					     soc->rxdma_err_dst_ring[lmac_id].
								alloc_size,
					     soc->ctrl_psoc,
					     WLAN_MD_DP_SRNG_RXDMA_ERR_DST,
					     "rxdma_err_dst");
			dp_srng_deinit(soc, &soc->rxdma_err_dst_ring[lmac_id],
				       RXDMA_DST, lmac_id);
		}
	}


}

/**
 * dp_pdev_srng_init() - initialize all pdev srng rings including
 *			   monitor rings
 * @pdev: Datapath pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_NOMEM on failure
 */
static QDF_STATUS dp_pdev_srng_init(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	uint32_t i;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	if (!soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		if (dp_srng_init(soc, &soc->rx_refill_buf_ring[pdev->lmac_id],
				 RXDMA_BUF, 0, pdev->lmac_id)) {
			dp_init_err("%pK: dp_srng_init failed rx refill ring",
				    soc);
			goto fail1;
		}
	}

	/* LMAC RxDMA to SW Rings configuration */
	if (!wlan_cfg_per_pdev_lmac_ring(soc_cfg_ctx))
		/* Only valid for MCL */
		pdev = soc->pdev_list[0];

	if (!soc->rxdma2sw_rings_not_supported) {
		for (i = 0;
		     i < soc->wlan_cfg_ctx->num_rxdma_dst_rings_per_pdev; i++) {
			int lmac_id = dp_get_lmac_id_for_pdev_id(soc, i,
								 pdev->pdev_id);
			struct dp_srng *srng =
				&soc->rxdma_err_dst_ring[lmac_id];

			if (srng->hal_srng)
				continue;

			if (dp_srng_init(soc, srng, RXDMA_DST, 0, lmac_id)) {
				dp_init_err("%pK:" RNG_ERR "rxdma_err_dst_ring",
					    soc);
				goto fail1;
			}
			wlan_minidump_log(soc->rxdma_err_dst_ring[lmac_id].
						base_vaddr_unaligned,
					  soc->rxdma_err_dst_ring[lmac_id].
						alloc_size,
					  soc->ctrl_psoc,
					  WLAN_MD_DP_SRNG_RXDMA_ERR_DST,
					  "rxdma_err_dst");
		}
	}
	return QDF_STATUS_SUCCESS;

fail1:
	dp_pdev_srng_deinit(pdev);
	return QDF_STATUS_E_NOMEM;
}

/**
 * dp_pdev_srng_free() - free all pdev srng rings including monitor rings
 * @pdev: Datapath pdev handle
 *
 */
static void dp_pdev_srng_free(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t i;

	if (!soc->features.dmac_cmn_src_rxbuf_ring_enabled)
		dp_srng_free(soc, &soc->rx_refill_buf_ring[pdev->lmac_id]);

	if (!soc->rxdma2sw_rings_not_supported) {
		for (i = 0;
		     i < soc->wlan_cfg_ctx->num_rxdma_dst_rings_per_pdev; i++) {
			int lmac_id = dp_get_lmac_id_for_pdev_id(soc, i,
								 pdev->pdev_id);

			dp_srng_free(soc, &soc->rxdma_err_dst_ring[lmac_id]);
		}
	}
}

/**
 * dp_pdev_srng_alloc() - allocate memory for all pdev srng rings including
 *			  monitor rings
 * @pdev: Datapath pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_NOMEM on failure
 */
static QDF_STATUS dp_pdev_srng_alloc(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	uint32_t ring_size;
	uint32_t i;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	ring_size = wlan_cfg_get_dp_soc_rxdma_refill_ring_size(soc_cfg_ctx);
	if (!soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		if (dp_srng_alloc(soc, &soc->rx_refill_buf_ring[pdev->lmac_id],
				  RXDMA_BUF, ring_size, 0)) {
			dp_init_err("%pK: dp_srng_alloc failed rx refill ring",
				    soc);
			goto fail1;
		}
	}

	ring_size = wlan_cfg_get_dp_soc_rxdma_err_dst_ring_size(soc_cfg_ctx);
	/* LMAC RxDMA to SW Rings configuration */
	if (!wlan_cfg_per_pdev_lmac_ring(soc_cfg_ctx))
		/* Only valid for MCL */
		pdev = soc->pdev_list[0];

	if (!soc->rxdma2sw_rings_not_supported) {
		for (i = 0;
		     i < soc->wlan_cfg_ctx->num_rxdma_dst_rings_per_pdev; i++) {
			int lmac_id = dp_get_lmac_id_for_pdev_id(soc, i,
								 pdev->pdev_id);
			struct dp_srng *srng =
				&soc->rxdma_err_dst_ring[lmac_id];

			if (srng->base_vaddr_unaligned)
				continue;

			if (dp_srng_alloc(soc, srng, RXDMA_DST, ring_size, 0)) {
				dp_init_err("%pK:" RNG_ERR "rxdma_err_dst_ring",
					    soc);
				goto fail1;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
fail1:
	dp_pdev_srng_free(pdev);
	return QDF_STATUS_E_NOMEM;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(DP_MLO_LINK_STATS_SUPPORT)
/**
 * dp_init_link_peer_stats_enabled() - Init link_peer_stats as per config
 * @pdev: DP pdev
 *
 * Return: None
 */
static inline void
dp_init_link_peer_stats_enabled(struct dp_pdev *pdev)
{
	pdev->link_peer_stats = wlan_cfg_is_peer_link_stats_enabled(
						pdev->soc->wlan_cfg_ctx);
}
#else
static inline void
dp_init_link_peer_stats_enabled(struct dp_pdev *pdev)
{
}
#endif

static QDF_STATUS dp_pdev_init(struct cdp_soc_t *txrx_soc,
				      HTC_HANDLE htc_handle,
				      qdf_device_t qdf_osdev,
				      uint8_t pdev_id)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	int nss_cfg;
	void *sojourn_buf;

	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	struct dp_pdev *pdev = soc->pdev_list[pdev_id];

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	pdev->soc = soc;
	pdev->pdev_id = pdev_id;

	/*
	 * Variable to prevent double pdev deinitialization during
	 * radio detach execution .i.e. in the absence of any vdev.
	 */
	pdev->pdev_deinit = 0;

	if (dp_wdi_event_attach(pdev)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "dp_wdi_evet_attach failed");
		goto fail0;
	}

	if (dp_pdev_srng_init(pdev)) {
		dp_init_err("%pK: Failed to initialize pdev srng rings", soc);
		goto fail1;
	}

	/* Initialize descriptors in TCL Rings used by IPA */
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		hal_tx_init_data_ring(soc->hal_soc,
				      soc->tcl_data_ring[IPA_TCL_DATA_RING_IDX].hal_srng);
		dp_ipa_hal_tx_init_alt_data_ring(soc);
	}

	/*
	 * Initialize command/credit ring descriptor
	 * Command/CREDIT ring also used for sending DATA cmds
	 */
	dp_tx_init_cmd_credit_ring(soc);

	dp_tx_pdev_init(pdev);

	/*
	 * set nss pdev config based on soc config
	 */
	nss_cfg = wlan_cfg_get_dp_soc_nss_cfg(soc_cfg_ctx);
	wlan_cfg_set_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx,
					 (nss_cfg & (1 << pdev_id)));
	pdev->target_pdev_id =
		dp_calculate_target_pdev_id_from_host_pdev_id(soc, pdev_id);

	if (soc->preferred_hw_mode == WMI_HOST_HW_MODE_2G_PHYB &&
	    pdev->lmac_id == PHYB_2G_LMAC_ID) {
		pdev->target_pdev_id = PHYB_2G_TARGET_PDEV_ID;
	}

	/* Reset the cpu ring map if radio is NSS offloaded */
	if (wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx)) {
		dp_soc_reset_cpu_ring_map(soc);
		dp_soc_reset_intr_mask(soc);
	}

	/* Reset the ring interrupt mask if DPDK is enabled */
	if (wlan_cfg_get_dp_soc_dpdk_cfg(soc->ctrl_psoc)) {
		dp_soc_reset_dpdk_intr_mask(soc);
	}
	/* Reset the cpu ring map if radio is NSS offloaded */
	dp_soc_reset_ipa_vlan_intr_mask(soc);

	TAILQ_INIT(&pdev->vdev_list);
	qdf_spinlock_create(&pdev->vdev_list_lock);
	pdev->vdev_count = 0;
	pdev->is_lro_hash_configured = 0;

	qdf_spinlock_create(&pdev->tx_mutex);
	pdev->ch_band_lmac_id_mapping[REG_BAND_2G] = DP_MON_INVALID_LMAC_ID;
	pdev->ch_band_lmac_id_mapping[REG_BAND_5G] = DP_MON_INVALID_LMAC_ID;
	pdev->ch_band_lmac_id_mapping[REG_BAND_6G] = DP_MON_INVALID_LMAC_ID;

	DP_STATS_INIT(pdev);

	dp_local_peer_id_pool_init(pdev);

	dp_dscp_tid_map_setup(pdev);
	dp_pcp_tid_map_setup(pdev);

	/* set the reo destination during initialization */
	dp_pdev_set_default_reo(pdev);

	qdf_mem_zero(&pdev->sojourn_stats, sizeof(struct cdp_tx_sojourn_stats));

	pdev->sojourn_buf = qdf_nbuf_alloc(pdev->soc->osdev,
			      sizeof(struct cdp_tx_sojourn_stats), 0, 4,
			      TRUE);

	if (!pdev->sojourn_buf) {
		dp_init_err("%pK: Failed to allocate sojourn buf", soc);
		goto fail2;
	}
	sojourn_buf = qdf_nbuf_data(pdev->sojourn_buf);
	qdf_mem_zero(sojourn_buf, sizeof(struct cdp_tx_sojourn_stats));

	qdf_event_create(&pdev->fw_peer_stats_event);
	qdf_event_create(&pdev->fw_stats_event);
	qdf_event_create(&pdev->fw_obss_stats_event);

	pdev->num_tx_allowed = wlan_cfg_get_num_tx_desc(soc->wlan_cfg_ctx);
	pdev->num_tx_spl_allowed =
		wlan_cfg_get_num_tx_spl_desc(soc->wlan_cfg_ctx);
	pdev->num_reg_tx_allowed =
		pdev->num_tx_allowed - pdev->num_tx_spl_allowed;
	if (dp_rxdma_ring_setup(soc, pdev)) {
		dp_init_err("%pK: RXDMA ring config failed", soc);
		goto fail3;
	}

	if (dp_init_ipa_rx_refill_buf_ring(soc, pdev))
		goto fail3;

	if (dp_ipa_ring_resource_setup(soc, pdev))
		goto fail4;

	if (dp_ipa_uc_attach(soc, pdev) != QDF_STATUS_SUCCESS) {
		dp_init_err("%pK: dp_ipa_uc_attach failed", soc);
		goto fail4;
	}

	if (dp_pdev_bkp_stats_attach(pdev) != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  FL("dp_pdev_bkp_stats_attach failed"));
		goto fail5;
	}

	if (dp_monitor_pdev_init(pdev)) {
		dp_init_err("%pK: dp_monitor_pdev_init failed", soc);
		goto fail6;
	}

	/* initialize sw rx descriptors */
	dp_rx_pdev_desc_pool_init(pdev);
	/* allocate buffers and replenish the RxDMA ring */
	dp_rx_pdev_buffers_alloc(pdev);

	dp_init_tso_stats(pdev);
	dp_init_link_peer_stats_enabled(pdev);

	/* Initialize dp tx fast path flag */
	pdev->tx_fast_flag = DP_TX_DESC_FLAG_SIMPLE;
	if (soc->hw_txrx_stats_en)
		pdev->tx_fast_flag |= DP_TX_DESC_FLAG_FASTPATH_SIMPLE;

	pdev->rx_fast_flag = false;
	dp_info("Mem stats: DMA = %u HEAP = %u SKB = %u",
		qdf_dma_mem_stats_read(),
		qdf_heap_mem_stats_read(),
		qdf_skb_total_mem_stats_read());

	return QDF_STATUS_SUCCESS;
fail6:
	dp_pdev_bkp_stats_detach(pdev);
fail5:
	dp_ipa_uc_detach(soc, pdev);
fail4:
	dp_deinit_ipa_rx_refill_buf_ring(soc, pdev);
fail3:
	dp_rxdma_ring_cleanup(soc, pdev);
	qdf_nbuf_free(pdev->sojourn_buf);
fail2:
	qdf_spinlock_destroy(&pdev->tx_mutex);
	qdf_spinlock_destroy(&pdev->vdev_list_lock);
	dp_pdev_srng_deinit(pdev);
fail1:
	dp_wdi_event_detach(pdev);
fail0:
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_pdev_init_wifi3() - Init txrx pdev
 * @txrx_soc:
 * @htc_handle: HTC handle for host-target interface
 * @qdf_osdev: QDF OS device
 * @pdev_id: pdev Id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_pdev_init_wifi3(struct cdp_soc_t *txrx_soc,
				     HTC_HANDLE htc_handle,
				     qdf_device_t qdf_osdev,
				     uint8_t pdev_id)
{
	return dp_pdev_init(txrx_soc, htc_handle, qdf_osdev, pdev_id);
}

#ifdef FEATURE_DIRECT_LINK
struct dp_srng *dp_setup_direct_link_refill_ring(struct cdp_soc_t *soc_hdl,
						 uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("DP pdev is NULL");
		return NULL;
	}

	if (dp_srng_alloc(soc, &pdev->rx_refill_buf_ring4,
			  RXDMA_BUF, DIRECT_LINK_REFILL_RING_ENTRIES, false)) {
		dp_err("SRNG alloc failed for rx_refill_buf_ring4");
		return NULL;
	}

	if (dp_srng_init(soc, &pdev->rx_refill_buf_ring4,
			 RXDMA_BUF, DIRECT_LINK_REFILL_RING_IDX, 0)) {
		dp_err("SRNG init failed for rx_refill_buf_ring4");
		dp_srng_free(soc, &pdev->rx_refill_buf_ring4);
		return NULL;
	}

	if (htt_srng_setup(soc->htt_handle, pdev_id,
			   pdev->rx_refill_buf_ring4.hal_srng, RXDMA_BUF)) {
		dp_srng_deinit(soc, &pdev->rx_refill_buf_ring4, RXDMA_BUF,
			       DIRECT_LINK_REFILL_RING_IDX);
		dp_srng_free(soc, &pdev->rx_refill_buf_ring4);
		return NULL;
	}

	return &pdev->rx_refill_buf_ring4;
}

void dp_destroy_direct_link_refill_ring(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("DP pdev is NULL");
		return;
	}

	dp_srng_deinit(soc, &pdev->rx_refill_buf_ring4, RXDMA_BUF, 0);
	dp_srng_free(soc, &pdev->rx_refill_buf_ring4);
}
#endif

#ifdef QCA_MULTIPASS_SUPPORT
QDF_STATUS dp_set_vlan_groupkey(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				uint16_t vlan_id, uint16_t group_key)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_TX_MULTIPASS);
	QDF_STATUS status;

	dp_info("Try: vdev_id %d, vdev %pK, multipass_en %d, vlan_id %d, group_key %d",
		vdev_id, vdev, vdev ? vdev->multipass_en : 0, vlan_id,
		group_key);
	if (!vdev || !vdev->multipass_en) {
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	if (!vdev->iv_vlan_map) {
		uint16_t vlan_map_size = (sizeof(uint16_t)) * DP_MAX_VLAN_IDS;

		vdev->iv_vlan_map = (uint16_t *)qdf_mem_malloc(vlan_map_size);
		if (!vdev->iv_vlan_map) {
			QDF_TRACE_ERROR(QDF_MODULE_ID_DP, "iv_vlan_map");
			status = QDF_STATUS_E_NOMEM;
			goto fail;
		}

		/*
		 * 0 is invalid group key.
		 * Initilalize array with invalid group keys.
		 */
		qdf_mem_zero(vdev->iv_vlan_map, vlan_map_size);
	}

	if (vlan_id >= DP_MAX_VLAN_IDS) {
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	dp_info("Successful setting: vdev_id %d, vlan_id %d, group_key %d",
		vdev_id, vlan_id, group_key);
	vdev->iv_vlan_map[vlan_id] = group_key;
	status = QDF_STATUS_SUCCESS;
fail:
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_TX_MULTIPASS);
	return status;
}

void dp_tx_remove_vlan_tag(struct dp_vdev *vdev, qdf_nbuf_t nbuf)
{
	struct vlan_ethhdr veth_hdr;
	struct vlan_ethhdr *veh = (struct vlan_ethhdr *)nbuf->data;

	/*
	 * Extract VLAN header of 4 bytes:
	 * Frame Format : {dst_addr[6], src_addr[6], 802.1Q header[4],
	 *		   EtherType[2], Payload}
	 * Before Removal : xx xx xx xx xx xx xx xx xx xx xx xx 81 00 00 02
	 *		    08 00 45 00 00...
	 * After Removal  : xx xx xx xx xx xx xx xx xx xx xx xx 08 00 45 00
	 *		    00...
	 */
	qdf_mem_copy(&veth_hdr, veh, sizeof(veth_hdr));
	qdf_nbuf_pull_head(nbuf, ETHERTYPE_VLAN_LEN);
	veh = (struct vlan_ethhdr *)nbuf->data;
	qdf_mem_copy(veh, &veth_hdr, 2 * QDF_MAC_ADDR_SIZE);
}

void dp_tx_vdev_multipass_deinit(struct dp_vdev *vdev)
{
	struct dp_txrx_peer *txrx_peer = NULL;

	qdf_spin_lock_bh(&vdev->mpass_peer_mutex);
	TAILQ_FOREACH(txrx_peer, &vdev->mpass_peer_list, mpass_peer_list_elem)
		qdf_err("Peers present in mpass list : %d", txrx_peer->peer_id);
	qdf_spin_unlock_bh(&vdev->mpass_peer_mutex);

	if (vdev->iv_vlan_map) {
		qdf_mem_free(vdev->iv_vlan_map);
		vdev->iv_vlan_map = NULL;
	}

	qdf_spinlock_destroy(&vdev->mpass_peer_mutex);
}

void dp_peer_multipass_list_init(struct dp_vdev *vdev)
{
	/*
	 * vdev->iv_vlan_map is allocated when the first configuration command
	 * is issued to avoid unnecessary allocation for regular mode VAP.
	 */
	TAILQ_INIT(&vdev->mpass_peer_list);
	qdf_spinlock_create(&vdev->mpass_peer_mutex);
}
#endif /* QCA_MULTIPASS_SUPPORT */

#ifdef WLAN_FEATURE_SSR_DRIVER_DUMP
#define MAX_STR_LEN 50
#define MAX_SRNG_STR_LEN 30

void dp_ssr_dump_srng_register(char *region_name, struct dp_srng *srng, int num)
{
	char ring[MAX_SRNG_STR_LEN], ring_handle[MAX_STR_LEN];

	if (num >= 0)
		qdf_snprint(ring, MAX_SRNG_STR_LEN, "%s%s%d",
			    region_name, "_", num);
	else
		qdf_snprint(ring, MAX_SRNG_STR_LEN, "%s", region_name);

	qdf_snprint(ring_handle, MAX_STR_LEN, "%s%s", ring, "_handle");

	qdf_ssr_driver_dump_register_region(ring_handle, srng->hal_srng,
					    sizeof(struct hal_srng));
	qdf_ssr_driver_dump_register_region(ring,
					    srng->base_vaddr_aligned,
					    srng->alloc_size);
}

void dp_ssr_dump_srng_unregister(char *region_name, int num)
{
	char ring[MAX_SRNG_STR_LEN], ring_handle[MAX_STR_LEN];

	if (num >= 0)
		qdf_snprint(ring, MAX_SRNG_STR_LEN, "%s%s%d",
			    region_name, "_", num);
	else
		qdf_snprint(ring, MAX_SRNG_STR_LEN, "%s", region_name);

	qdf_snprint(ring_handle, MAX_STR_LEN, "%s%s", ring, "_handle");

	qdf_ssr_driver_dump_unregister_region(ring);
	qdf_ssr_driver_dump_unregister_region(ring_handle);
}

void dp_ssr_dump_pdev_register(struct dp_pdev *pdev, uint8_t pdev_id)
{
	char pdev_str[MAX_STR_LEN];

	qdf_snprint(pdev_str, MAX_STR_LEN, "%s%s%d", "dp_pdev", "_", pdev_id);
	qdf_ssr_driver_dump_register_region(pdev_str, pdev, sizeof(*pdev));
}

void dp_ssr_dump_pdev_unregister(uint8_t pdev_id)
{
	char pdev_str[MAX_STR_LEN];

	qdf_snprint(pdev_str, MAX_STR_LEN, "%s%s%d", "dp_pdev", "_", pdev_id);
	qdf_ssr_driver_dump_unregister_region(pdev_str);
}
#endif

/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DP_MON_H_
#define _DP_MON_H_

#include "qdf_trace.h"
#include "dp_internal.h"
#include "dp_types.h"
#include "dp_htt.h"

#include <dp_mon_filter.h>
#ifdef WLAN_TX_PKT_CAPTURE_ENH
#include "dp_tx_capture.h"
#endif

#ifdef QCA_SUPPORT_LITE_MONITOR
#include "dp_lite_mon.h"
#endif

#define DP_INTR_POLL_TIMER_MS	5

#define MON_VDEV_TIMER_INIT 0x1
#define MON_VDEV_TIMER_RUNNING 0x2

/* Budget to reap monitor status ring */
#define DP_MON_REAP_BUDGET 1024
#define MON_BUF_MIN_ENTRIES 64

/* 40MHZ BW 2 20MHZ sub bands */
#define SUB40BW 2
/* 80MHZ BW 4 20MHZ sub bands */
#define SUB80BW 4
/* 160MHZ BW 8 20MHZ sub bands */
#define SUB160BW 8
/* 320MHZ BW 16 20MHZ sub bands */
#define SUB320BW 16

#define RNG_ERR		"SRNG setup failed for"
#define dp_mon_info(params...) \
	__QDF_TRACE_FL(QDF_TRACE_LEVEL_INFO_HIGH, QDF_MODULE_ID_MON, ## params)
#define dp_mon_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_MON, params)
#define dp_mon_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_MON, params)
#define dp_mon_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_MON, params)

#define dp_mon_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_MON, params)
#define dp_mon_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_MON, params)
#define dp_mon_info_rl(params...) \
	__QDF_TRACE_RL(QDF_TRACE_LEVEL_INFO_HIGH, QDF_MODULE_ID_MON, ## params)

#ifdef QCA_ENHANCED_STATS_SUPPORT
typedef struct dp_peer_extd_tx_stats dp_mon_peer_tx_stats;
typedef struct dp_peer_extd_rx_stats dp_mon_peer_rx_stats;

#define DP_UPDATE_MON_STATS(_tgtobj, _srcobj) \
	DP_UPDATE_EXTD_STATS(_tgtobj, _srcobj)
#endif

#ifndef WLAN_TX_PKT_CAPTURE_ENH
struct dp_pdev_tx_capture {
};

struct dp_peer_tx_capture {
};
#endif

#ifndef WLAN_TX_PKT_CAPTURE_ENH
static inline void
dp_process_ppdu_stats_update_failed_bitmap(struct dp_pdev *pdev,
					   void *data,
					   uint32_t ppdu_id,
					   uint32_t size)
{
}
#endif

#ifdef DP_CON_MON_MSI_ENABLED
static inline bool dp_is_monitor_mode_using_poll(struct dp_soc *soc)
{
	return false;
}
#else
static inline bool dp_is_monitor_mode_using_poll(struct dp_soc *soc)
{
	return true;
}
#endif

/**
 * dp_mon_soc_attach() - DP monitor soc attach
 * @soc: Datapath SOC handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_NOMEM: Error
 */
QDF_STATUS dp_mon_soc_attach(struct dp_soc *soc);

/**
 * dp_mon_soc_detach() - DP monitor soc detach
 * @soc: Datapath SOC handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_soc_detach(struct dp_soc *soc);

/**
 * dp_mon_soc_cfg_init() - DP monitor soc config init
 * @soc: Datapath SOC handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_soc_cfg_init(struct dp_soc *soc);

/**
 * dp_mon_pdev_attach() - DP monitor pdev attach
 * @pdev: Datapath pdev handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_NOMEM: Error
 */
QDF_STATUS dp_mon_pdev_attach(struct dp_pdev *pdev);

/**
 * dp_mon_pdev_detach() - DP monitor pdev detach
 * @pdev: Datapath pdev handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_pdev_detach(struct dp_pdev *pdev);

/**
 * dp_mon_pdev_init() - DP monitor pdev init
 * @pdev: Datapath pdev handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_pdev_init(struct dp_pdev *pdev);

/**
 * dp_mon_pdev_deinit() - DP monitor pdev deinit
 * @pdev: Datapath pdev handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_pdev_deinit(struct dp_pdev *pdev);

/**
 * dp_mon_vdev_attach() - DP monitor vdev attach
 * @vdev: Datapath vdev handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_NOMEM: Error
 */
QDF_STATUS dp_mon_vdev_attach(struct dp_vdev *vdev);

/**
 * dp_mon_vdev_detach() - DP monitor vdev detach
 * @vdev: Datapath vdev handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_vdev_detach(struct dp_vdev *vdev);

/**
 * dp_mon_peer_attach() - DP monitor peer attach
 * @peer: Datapath peer handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_NOMEM: Error
 */
#if defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(FEATURE_PERPKT_INFO)
QDF_STATUS dp_mon_peer_attach(struct dp_peer *peer);
#else
static inline
QDF_STATUS dp_mon_peer_attach(struct dp_peer *peer)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_mon_peer_detach() - DP monitor peer detach
 * @peer: Datapath peer handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_peer_detach(struct dp_peer *peer);

/**
 * dp_mon_peer_get_peerstats_ctx() - Get peer stats context from monitor peer
 * @peer: Datapath peer handle
 *
 * Return: peerstats_ctx
 */
struct cdp_peer_rate_stats_ctx *dp_mon_peer_get_peerstats_ctx(struct
							      dp_peer *peer);

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_peer_reset_stats() - Reset monitor peer stats
 * @peer: Datapath peer handle
 *
 * Return: none
 */
void dp_mon_peer_reset_stats(struct dp_peer *peer);

/**
 * dp_mon_peer_get_stats() - Get monitor peer stats
 *
 * @peer: Datapath peer handle
 * @arg: Pointer to stats struct
 * @type: Update type
 *
 * Return: none
 */
void dp_mon_peer_get_stats(struct dp_peer *peer, void *arg,
			   enum cdp_stat_update_type type);

/**
 * dp_mon_invalid_peer_update_pdev_stats() - Update pdev stats from
 *					invalid monitor peer
 * @pdev: Datapath pdev handle
 *
 * Return: none
 */
void dp_mon_invalid_peer_update_pdev_stats(struct dp_pdev *pdev);

/**
 * dp_mon_peer_get_stats_param() - Get stats param value from monitor peer
 * @peer: Datapath peer handle
 * @type: Stats type requested
 * @buf: Pointer to buffer for stats param
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_mon_peer_get_stats_param(struct dp_peer *peer,
				       enum cdp_peer_stats_type type,
				       cdp_peer_stats_param_t *buf);
#else
static inline void dp_mon_peer_reset_stats(struct dp_peer *peer)
{
}

static inline
void dp_mon_peer_get_stats(struct dp_peer *peer, void *arg,
			   enum cdp_stat_update_type type)
{
}

static inline void dp_mon_invalid_peer_update_pdev_stats(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_mon_peer_get_stats_param(struct dp_peer *peer,
				       enum cdp_peer_stats_type type,
				       cdp_peer_stats_param_t *buf)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * dp_mon_cdp_ops_register() - Register monitor cdp ops
 * @soc: Datapath soc handle
 *
 */
void dp_mon_cdp_ops_register(struct dp_soc *soc);

/**
 * dp_mon_cdp_ops_deregister() - deregister monitor cdp ops
 * @soc: Datapath soc handle
 *
 */
void dp_mon_cdp_ops_deregister(struct dp_soc *soc);

/**
 * dp_mon_intr_ops_deregister() - deregister monitor interrupt ops
 * @soc: Datapath soc handle
 *
 */
void dp_mon_intr_ops_deregister(struct dp_soc *soc);

/**
 * dp_mon_feature_ops_deregister() - deregister monitor feature ops
 * @soc: Datapath soc handle
 *
 */
void dp_mon_feature_ops_deregister(struct dp_soc *soc);

/**
 * dp_mon_ops_free() - free monitor ops
 * @soc: Datapath soc handle
 *
 */
void dp_mon_ops_free(struct dp_soc *soc);

/**
 * dp_mon_ops_register() - Register monitor ops
 * @soc: Datapath soc handle
 *
 */
void dp_mon_ops_register(struct dp_soc *soc);

#ifndef DISABLE_MON_CONFIG
void dp_mon_register_intr_ops(struct dp_soc *soc);
#else
static inline void dp_mon_register_intr_ops(struct dp_soc *soc)
{}
#endif

/**
 * dp_mon_htt_srng_setup() - DP mon htt srng setup
 * @soc: Datapath soc handle
 * @pdev: Datapath pdev handle
 * @mac_id: mac id
 * @mac_for_pdev: mac id mapped pdev
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_FAILURE: Error
 */
QDF_STATUS dp_mon_htt_srng_setup(struct dp_soc *soc,
				 struct dp_pdev *pdev,
				 int mac_id,
				 int mac_for_pdev);

/**
 * dp_config_debug_sniffer()- API to enable/disable debug sniffer
 * @pdev: DP_PDEV handle
 * @val: user provided value
 *
 * Return: 0 for success. nonzero for failure.
 */
#if defined(QCA_MCOPY_SUPPORT) || defined(QCA_TX_CAPTURE_SUPPORT)
QDF_STATUS
dp_config_debug_sniffer(struct dp_pdev *pdev, int val);
#else
static inline QDF_STATUS
dp_config_debug_sniffer(struct dp_pdev *pdev, int val) {
	return QDF_STATUS_E_INVAL;
}
#endif /* QCA_MCOPY_SUPPORT || QCA_TX_CAPTURE_SUPPORT */

/**
 * dp_mon_config_undecoded_metadata_capture()- API to enable/disable undecoded
 *                                             metadata capture
 * @pdev: DP_PDEV handle
 * @val: user provided value
 *
 * Return: 0 for success. nonzero for failure.
 */
#ifdef QCA_UNDECODED_METADATA_SUPPORT
QDF_STATUS
dp_mon_config_undecoded_metadata_capture(struct dp_pdev *pdev, int val);
#else
static inline QDF_STATUS
dp_mon_config_undecoded_metadata_capture(struct dp_pdev *pdev, int val) {
	return QDF_STATUS_E_INVAL;
}
#endif /* QCA_UNDECODED_METADATA_SUPPORT */

/**
 * dp_htt_ppdu_stats_attach() - attach resources for HTT PPDU stats processing
 * @pdev: Datapath PDEV handle
 *
 * Return: QDF_STATUS_SUCCESS: Success
 *         QDF_STATUS_E_NOMEM: Error
 */
QDF_STATUS dp_htt_ppdu_stats_attach(struct dp_pdev *pdev);

/**
 * dp_htt_ppdu_stats_detach() - detach stats resources
 * @pdev: Datapath PDEV handle
 *
 * Return: void
 */
void dp_htt_ppdu_stats_detach(struct dp_pdev *pdev);

/**
 * dp_set_bpr_enable() - API to enable/disable bpr feature
 * @pdev: DP_PDEV handle.
 * @val: Provided value.
 *
 * Return: 0 for success. nonzero for failure.
 */
#ifdef QCA_SUPPORT_BPR
QDF_STATUS
dp_set_bpr_enable(struct dp_pdev *pdev, int val);
#endif

#ifdef ATH_SUPPORT_NAC
int dp_set_filter_neigh_peers(struct dp_pdev *pdev,
				     bool val);
#endif /* ATH_SUPPORT_NAC */

#ifdef WLAN_ATF_ENABLE
void dp_set_atf_stats_enable(struct dp_pdev *pdev, bool value);
#endif

/**
 * dp_mon_set_bsscolor() - sets bsscolor for tx capture
 * @pdev: Datapath PDEV handle
 * @bsscolor: new bsscolor
 */
void
dp_mon_set_bsscolor(struct dp_pdev *pdev, uint8_t bsscolor);

/**
 * dp_pdev_get_filter_ucast_data() - get DP PDEV monitor ucast filter
 * @pdev_handle: Datapath PDEV handle
 *
 * Return: true on ucast filter flag set
 */
bool dp_pdev_get_filter_ucast_data(struct cdp_pdev *pdev_handle);

/**
 * dp_pdev_get_filter_mcast_data() - get DP PDEV monitor mcast filter
 * @pdev_handle: Datapath PDEV handle
 *
 * Return: true on mcast filter flag set
 */
bool dp_pdev_get_filter_mcast_data(struct cdp_pdev *pdev_handle);

/**
 * dp_pdev_get_filter_non_data() - get DP PDEV monitor non_data filter
 * @pdev_handle: Datapath PDEV handle
 *
 * Return: true on non data filter flag set
 */
bool dp_pdev_get_filter_non_data(struct cdp_pdev *pdev_handle);

/**
 * dp_set_pktlog_wifi3() - attach txrx vdev
 * @pdev: Datapath PDEV handle
 * @event: which event's notifications are being subscribed to
 * @enable: WDI event subscribe or not. (True or False)
 *
 * Return: Success, NULL on failure
 */
#ifdef WDI_EVENT_ENABLE
int dp_set_pktlog_wifi3(struct dp_pdev *pdev, uint32_t event,
			bool enable);
#endif

/* MCL specific functions */
#if defined(DP_CON_MON) && !defined(REMOVE_PKT_LOG)
/**
 * dp_pktlogmod_exit() - API to cleanup pktlog info
 * @pdev: Pdev handle
 *
 * Return: none
 */
void dp_pktlogmod_exit(struct dp_pdev *pdev);
#else
static inline
void dp_pktlogmod_exit(struct dp_pdev *handle)
{
}
#endif

#ifdef QCA_MONITOR_PKT_SUPPORT
/**
 * dp_vdev_set_monitor_mode_buf_rings() - set monitor mode buf rings
 * @pdev: DP pdev object
 *
 * Allocate SW descriptor pool, buffers, link descriptor memory
 * Initialize monitor related SRNGs
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_vdev_set_monitor_mode_buf_rings(struct dp_pdev *pdev);

/**
 * dp_vdev_set_monitor_mode_rings() - set monitor mode rings
 * @pdev: DP pdev object
 * @delayed_replenish:
 *
 * Allocate SW descriptor pool, buffers, link descriptor memory
 * Initialize monitor related SRNGs
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_vdev_set_monitor_mode_rings(struct dp_pdev *pdev,
					  uint8_t delayed_replenish);

#else
static inline QDF_STATUS
dp_vdev_set_monitor_mode_buf_rings(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_vdev_set_monitor_mode_rings(struct dp_pdev *pdev,
			       uint8_t delayed_replenish)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WDI_EVENT_ENABLE) &&\
	(defined(QCA_ENHANCED_STATS_SUPPORT) || !defined(REMOVE_PKT_LOG))
/**
 * dp_ppdu_stats_ind_handler() - PPDU stats msg handler
 * @soc:	 HTT SOC handle
 * @msg_word:    Pointer to payload
 * @htt_t2h_msg: HTT msg nbuf
 *
 * Return: True if buffer should be freed by caller.
 */
bool dp_ppdu_stats_ind_handler(struct htt_soc *soc,
			       uint32_t *msg_word,
			       qdf_nbuf_t htt_t2h_msg);
#endif

#if defined(QCA_ENHANCED_STATS_SUPPORT) && \
	(!defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(QCA_MONITOR_2_0_SUPPORT))
/**
 * dp_ppdu_desc_deliver(): Function to deliver Tx PPDU status descriptor
 * to upper layer
 * @pdev: DP pdev handle
 * @ppdu_info: per PPDU TLV descriptor
 *
 * return: void
 */
void dp_ppdu_desc_deliver(struct dp_pdev *pdev, struct ppdu_info *ppdu_info);
#endif

#ifdef QCA_RSSI_DB2DBM
/**
 * dp_mon_pdev_params_rssi_dbm_conv() --> to set rssi in dbm conversion
 *						params into monitor pdev.
 * @cdp_soc: dp soc handle.
 * @params: cdp_rssi_db2dbm_param_dp structure value.
 *
 * Return: QDF_STATUS_SUCCESS if value set successfully
 *         QDF_STATUS_E_INVAL false if error
 */
QDF_STATUS
dp_mon_pdev_params_rssi_dbm_conv(struct cdp_soc_t *cdp_soc,
				 struct cdp_rssi_db2dbm_param_dp *params);
#else
static inline QDF_STATUS
dp_mon_pdev_params_rssi_dbm_conv(struct cdp_soc_t *cdp_soc,
				 struct cdp_rssi_db2dbm_param_dp *params)
{
	return QDF_STATUS_E_INVAL;
}
#endif /* QCA_RSSI_DB2DBM */

struct dp_mon_ops {
	QDF_STATUS (*mon_soc_cfg_init)(struct dp_soc *soc);
	QDF_STATUS (*mon_soc_attach)(struct dp_soc *soc);
	QDF_STATUS (*mon_soc_detach)(struct dp_soc *soc);
	QDF_STATUS (*mon_pdev_alloc)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_soc_init)(struct dp_soc *soc);
	void (*mon_soc_deinit)(struct dp_soc *soc);
	void (*mon_pdev_free)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_pdev_attach)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_pdev_detach)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_pdev_init)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_pdev_deinit)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_vdev_attach)(struct dp_vdev *vdev);
	QDF_STATUS (*mon_vdev_detach)(struct dp_vdev *vdev);
	QDF_STATUS (*mon_peer_attach)(struct dp_peer *peer);
	QDF_STATUS (*mon_peer_detach)(struct dp_peer *peer);
	struct cdp_peer_rate_stats_ctx *(*mon_peer_get_peerstats_ctx)(struct
								dp_peer *peer);
	void (*mon_peer_reset_stats)(struct dp_peer *peer);
	void (*mon_peer_get_stats)(struct dp_peer *peer, void *arg,
				   enum cdp_stat_update_type type);
	void (*mon_invalid_peer_update_pdev_stats)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_peer_get_stats_param)(struct dp_peer *peer,
					       enum cdp_peer_stats_type type,
					       cdp_peer_stats_param_t *buf);
	QDF_STATUS (*mon_config_debug_sniffer)(struct dp_pdev *pdev, int val);
	void (*mon_flush_rings)(struct dp_soc *soc);
#if !defined(DISABLE_MON_CONFIG)
	QDF_STATUS (*mon_pdev_htt_srng_setup)(struct dp_soc *soc,
					      struct dp_pdev *pdev,
					      int mac_id,
					      int mac_for_pdev);
	QDF_STATUS (*mon_soc_htt_srng_setup)(struct dp_soc *soc);
#endif
#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_MAC)
	uint32_t (*mon_drop_packets_for_mac)(struct dp_pdev *pdev,
					     uint32_t mac_id,
					     uint32_t quota);
#endif
#if defined(DP_CON_MON)
	void (*mon_service_rings)(struct  dp_soc *soc, uint32_t quota);
#endif
#ifndef DISABLE_MON_CONFIG
	uint32_t (*mon_rx_process)(struct dp_soc *soc,
				   struct dp_intr *int_ctx,
				   uint32_t mac_id,
				   uint32_t quota);
	uint32_t (*mon_tx_process)(struct dp_soc *soc,
				   struct dp_intr *int_ctx,
				   uint32_t mac_id,
				   uint32_t quota);
	void (*print_txmon_ring_stat)(struct dp_pdev *pdev);
#endif
	void (*mon_peer_tx_init)(struct dp_pdev *pdev, struct dp_peer *peer);
	void (*mon_peer_tx_cleanup)(struct dp_vdev *vdev,
				    struct dp_peer *peer);
#ifdef WIFI_MONITOR_SUPPORT
	void (*mon_peer_tid_peer_id_update)(struct dp_peer *peer,
					    uint16_t peer_id);
	void (*mon_tx_ppdu_stats_attach)(struct dp_pdev *pdev);
	void (*mon_tx_ppdu_stats_detach)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_tx_capture_debugfs_init)(struct dp_pdev *pdev);
	void (*mon_peer_tx_capture_filter_check)(struct dp_pdev *pdev,
						 struct dp_peer *peer);
	QDF_STATUS (*mon_tx_add_to_comp_queue)(struct dp_soc *soc,
					       struct dp_tx_desc_s *desc,
					       struct hal_tx_completion_status *ts,
					       uint16_t peer_id);
	QDF_STATUS (*mon_update_msdu_to_list)(struct dp_soc *soc,
					      struct dp_pdev *pdev,
					      struct dp_peer *peer,
					      struct hal_tx_completion_status *ts,
					      qdf_nbuf_t netbuf);
	QDF_STATUS
	(*mon_peer_tx_capture_get_stats)(struct dp_peer *peer,
					 struct cdp_peer_tx_capture_stats *sts);
	QDF_STATUS
	(*mon_pdev_tx_capture_get_stats)(struct dp_pdev *pdev,
					 struct cdp_pdev_tx_capture_stats *sts);
#endif
#if defined(WDI_EVENT_ENABLE) &&\
	(defined(QCA_ENHANCED_STATS_SUPPORT) || !defined(REMOVE_PKT_LOG))
	bool (*mon_ppdu_stats_ind_handler)(struct htt_soc *soc,
					   uint32_t *msg_word,
					   qdf_nbuf_t htt_t2h_msg);
#endif
	QDF_STATUS (*mon_htt_ppdu_stats_attach)(struct dp_pdev *pdev);
	void (*mon_htt_ppdu_stats_detach)(struct dp_pdev *pdev);
	void (*mon_print_pdev_rx_mon_stats)(struct dp_pdev *pdev);

#ifdef WIFI_MONITOR_SUPPORT
	void (*mon_print_pdev_tx_capture_stats)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_config_enh_tx_capture)(struct dp_pdev *pdev,
						uint8_t val);
	QDF_STATUS (*mon_tx_peer_filter)(struct dp_pdev *pdev_handle,
					 struct dp_peer *peer_handle,
					 uint8_t is_tx_pkt_cap_enable,
					 uint8_t *peer_mac);
#endif
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	QDF_STATUS (*mon_config_enh_rx_capture)(struct dp_pdev *pdev,
						uint8_t val);
#endif
#ifdef QCA_SUPPORT_BPR
	QDF_STATUS (*mon_set_bpr_enable)(struct dp_pdev *pdev, int val);
#endif
#ifdef ATH_SUPPORT_NAC
	int (*mon_set_filter_neigh_peers)(struct dp_pdev *pdev, bool val);
#endif
#ifdef WLAN_ATF_ENABLE
	void (*mon_set_atf_stats_enable)(struct dp_pdev *pdev, bool value);
#endif
	void (*mon_set_bsscolor)(struct dp_pdev *pdev, uint8_t bsscolor);
	bool (*mon_pdev_get_filter_ucast_data)(struct cdp_pdev *pdev_handle);
	bool (*mon_pdev_get_filter_non_data)(struct cdp_pdev *pdev_handle);
	bool (*mon_pdev_get_filter_mcast_data)(struct cdp_pdev *pdev_handle);
#ifdef WDI_EVENT_ENABLE
	int (*mon_set_pktlog_wifi3)(struct dp_pdev *pdev, uint32_t event,
				    bool enable);
#endif
#if defined(DP_CON_MON) && !defined(REMOVE_PKT_LOG)
	void (*mon_pktlogmod_exit)(struct dp_pdev *pdev);
#endif
	QDF_STATUS (*mon_vdev_set_monitor_mode_buf_rings)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_vdev_set_monitor_mode_rings)(struct dp_pdev *pdev,
						      uint8_t delayed_replenish);
	void (*mon_neighbour_peers_detach)(struct dp_pdev *pdev);
#ifdef FEATURE_NAC_RSSI
	QDF_STATUS (*mon_filter_neighbour_peer)(struct dp_pdev *pdev,
						uint8_t *rx_pkt_hdr);
#endif
	void (*mon_vdev_timer_init)(struct dp_soc *soc);
	void (*mon_vdev_timer_start)(struct dp_soc *soc);
	bool (*mon_vdev_timer_stop)(struct dp_soc *soc);
	void (*mon_vdev_timer_deinit)(struct dp_soc *soc);
	void (*mon_reap_timer_init)(struct dp_soc *soc);
	bool (*mon_reap_timer_start)(struct dp_soc *soc,
				     enum cdp_mon_reap_source source);
	bool (*mon_reap_timer_stop)(struct dp_soc *soc,
				    enum cdp_mon_reap_source source);
	void (*mon_reap_timer_deinit)(struct dp_soc *soc);
#ifdef QCA_MCOPY_SUPPORT
	QDF_STATUS (*mon_mcopy_check_deliver)(struct dp_pdev *pdev,
					      uint16_t peer_id,
					      uint32_t ppdu_id,
					      uint8_t first_msdu);
#endif
	void (*mon_neighbour_peer_add_ast)(struct dp_pdev *pdev,
					   struct dp_peer *ta_peer,
					   uint8_t *mac_addr,
					   qdf_nbuf_t nbuf,
					   uint32_t flags);
#ifdef QCA_ENHANCED_STATS_SUPPORT
	void (*mon_filter_setup_enhanced_stats)(struct dp_pdev *pdev);
	void (*mon_filter_reset_enhanced_stats)(struct dp_pdev *pdev);
	void (*mon_tx_stats_update)(struct dp_mon_peer *mon_peer,
				    struct cdp_tx_completion_ppdu_user *ppdu);
	void (*mon_tx_enable_enhanced_stats)(struct dp_pdev *pdev);
	void (*mon_tx_disable_enhanced_stats)(struct dp_pdev *pdev);
	void (*mon_ppdu_desc_deliver)(struct dp_pdev *pdev,
				      struct ppdu_info *ppdu_info);
	bool (*mon_ppdu_stats_feat_enable_check)(struct dp_pdev *pdev);
	void (*mon_ppdu_desc_notify)(struct dp_pdev *pdev, qdf_nbuf_t nbuf);
#endif
#ifdef QCA_MCOPY_SUPPORT
	void (*mon_filter_setup_mcopy_mode)(struct dp_pdev *pdev);
	void (*mon_filter_reset_mcopy_mode)(struct dp_pdev *pdev);
#endif
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
	void (*mon_filter_setup_smart_monitor)(struct dp_pdev *pdev);
	void (*mon_filter_reset_smart_monitor)(struct dp_pdev *pdev);
#endif
	void (*mon_filter_set_reset_mon_mac_filter)(struct dp_pdev *pdev,
						    bool val);
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	void (*mon_filter_setup_rx_enh_capture)(struct dp_pdev *pdev);
	void (*mon_filter_reset_rx_enh_capture)(struct dp_pdev *pdev);
#endif
	void (*mon_filter_setup_rx_mon_mode)(struct dp_pdev *pdev);
	void (*mon_filter_reset_rx_mon_mode)(struct dp_pdev *pdev);
	void (*mon_filter_setup_tx_mon_mode)(struct dp_pdev *pdev);
	void (*mon_filter_reset_tx_mon_mode)(struct dp_pdev *pdev);
#ifdef WDI_EVENT_ENABLE
	void (*mon_filter_setup_rx_pkt_log_full)(struct dp_pdev *pdev);
	void (*mon_filter_reset_rx_pkt_log_full)(struct dp_pdev *pdev);
	void (*mon_filter_setup_rx_pkt_log_lite)(struct dp_pdev *pdev);
	void (*mon_filter_reset_rx_pkt_log_lite)(struct dp_pdev *pdev);
	void (*mon_filter_setup_rx_pkt_log_cbf)(struct dp_pdev *pdev);
	void (*mon_filter_reset_rx_pkt_log_cbf)(struct dp_pdev *pdev);
#ifdef BE_PKTLOG_SUPPORT
	void (*mon_filter_setup_pktlog_hybrid)(struct dp_pdev *pdev);
	void (*mon_filter_reset_pktlog_hybrid)(struct dp_pdev *pdev);
#endif
#endif
	QDF_STATUS (*rx_mon_filter_update)(struct dp_pdev *pdev);
	QDF_STATUS (*tx_mon_filter_update)(struct dp_pdev *pdev);
	QDF_STATUS (*set_mon_mode_buf_rings_tx)(struct dp_pdev *pdev,
						uint16_t num_buf);

	QDF_STATUS (*tx_mon_filter_alloc)(struct dp_pdev *pdev);
	void (*tx_mon_filter_dealloc)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_rings_alloc)(struct dp_pdev *pdev);
	void (*mon_rings_free)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_rings_init)(struct dp_pdev *pdev);
	void (*mon_rings_deinit)(struct dp_pdev *pdev);

	QDF_STATUS (*rx_mon_buffers_alloc)(struct dp_pdev *pdev);
	void (*rx_mon_buffers_free)(struct dp_pdev *pdev);
	void (*rx_mon_desc_pool_init)(struct dp_pdev *pdev);
	void (*rx_mon_desc_pool_deinit)(struct dp_pdev *pdev);
	QDF_STATUS (*rx_mon_desc_pool_alloc)(struct dp_pdev *pdev);
	void (*rx_mon_desc_pool_free)(struct dp_pdev *pdev);
	void (*tx_mon_desc_pool_init)(struct dp_pdev *pdev);
	void (*tx_mon_desc_pool_deinit)(struct dp_pdev *pdev);
	QDF_STATUS (*tx_mon_desc_pool_alloc)(struct dp_pdev *pdev);
	void (*tx_mon_desc_pool_free)(struct dp_pdev *pdev);
	void (*rx_mon_enable)(uint32_t *msg_word,
			      struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_hdr_length_set)(uint32_t *msg_word,
				  struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_packet_length_set)(uint32_t *msg_word,
				     struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_wmask_subscribe)(uint32_t *msg_word,
				   struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_enable_mpdu_logging)(uint32_t *msg_word,
				       struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_enable_fpmo)(uint32_t *msg_word,
			       struct htt_rx_ring_tlv_filter *tlv_filter);
#ifndef DISABLE_MON_CONFIG
	void (*mon_register_intr_ops)(struct dp_soc *soc);
#endif
	void (*mon_register_feature_ops)(struct dp_soc *soc);
#ifdef QCA_ENHANCED_STATS_SUPPORT
	void (*mon_rx_stats_update)(struct dp_mon_peer *mon_peer,
				    struct cdp_rx_indication_ppdu *ppdu,
				    struct cdp_rx_stats_ppdu_user *ppdu_user);
	void (*mon_rx_populate_ppdu_usr_info)(struct mon_rx_user_status *rx_user_status,
					      struct cdp_rx_stats_ppdu_user *ppdu_user);
	void (*mon_rx_populate_ppdu_info)(struct hal_rx_ppdu_info *hal_ppdu_info,
					  struct cdp_rx_indication_ppdu *ppdu);
#endif
	QDF_STATUS (*rx_mon_refill_buf_ring)(struct dp_intr *int_ctx);
	QDF_STATUS (*tx_mon_refill_buf_ring)(struct dp_intr *int_ctx);
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	QDF_STATUS (*mon_config_undecoded_metadata_capture)
	    (struct dp_pdev *pdev, int val);
	void (*mon_filter_setup_undecoded_metadata_capture)
	    (struct dp_pdev *pdev);
	void (*mon_filter_reset_undecoded_metadata_capture)
	    (struct dp_pdev *pdev);
#endif
	QDF_STATUS (*mon_pdev_ext_init)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_pdev_ext_deinit)(struct dp_pdev *pdev);
	QDF_STATUS (*mon_lite_mon_alloc)(struct dp_pdev *pdev);
	void (*mon_lite_mon_dealloc)(struct dp_pdev *pdev);
	void (*mon_lite_mon_vdev_delete)(struct dp_pdev *pdev,
					 struct dp_vdev *vdev);
	void (*mon_lite_mon_disable_rx)(struct dp_pdev *pdev);
	bool (*mon_lite_mon_is_rx_adv_filter_enable)(struct dp_pdev *pdev);
	/* Print advanced monitor stats */
	void (*mon_rx_print_advanced_stats)
		(struct dp_soc *soc, struct dp_pdev *pdev);
	QDF_STATUS (*mon_rx_ppdu_info_cache_create)(struct dp_pdev *pdev);
	void (*mon_rx_ppdu_info_cache_destroy)(struct dp_pdev *pdev);
	void (*mon_mac_filter_set)(uint32_t *msg_word,
				   struct htt_rx_ring_tlv_filter *tlv_filter);
};

/**
 * struct dp_mon_soc_stats - monitor stats
 * @frag_alloc: Number of frags allocated
 * @frag_free: Number of frags freed
 */
struct dp_mon_soc_stats {
	uint32_t frag_alloc;
	uint32_t frag_free;
};

struct dp_mon_soc {
	/* Holds all monitor related fields extracted from dp_soc */
	/* Holds pointer to monitor ops */
	/* monitor link descriptor pages */
	struct qdf_mem_multi_page_t mon_link_desc_pages[MAX_NUM_LMAC_HW];

	/* total link descriptors for monitor mode for each radio */
	uint32_t total_mon_link_descs[MAX_NUM_LMAC_HW];

	 /* Monitor Link descriptor memory banks */
	struct link_desc_bank
		mon_link_desc_banks[MAX_NUM_LMAC_HW][MAX_MON_LINK_DESC_BANKS];
	uint32_t num_mon_link_desc_banks[MAX_NUM_LMAC_HW];
	/* Smart monitor capability for HKv2 */
	uint8_t hw_nac_monitor_support;

	/* Full monitor mode support */
	bool full_mon_mode;

	/*interrupt timer*/
	qdf_timer_t mon_reap_timer;
	uint8_t reap_timer_init;

	qdf_spinlock_t reap_timer_lock;

	/* Bitmap to record trigger sources of the reap timer */
	qdf_bitmap(mon_reap_src_bitmap, CDP_MON_REAP_SOURCE_NUM);

	qdf_timer_t mon_vdev_timer;
	uint8_t mon_vdev_timer_state;

	struct dp_mon_ops *mon_ops;
	bool monitor_mode_v2;
#ifndef DISABLE_MON_CONFIG
	uint32_t (*mon_rx_process)(struct dp_soc *soc,
				   struct dp_intr *int_ctx,
				   uint32_t mac_id,
				   uint32_t quota);
#endif

#ifdef WLAN_TX_PKT_CAPTURE_ENH
	struct dp_soc_tx_capture dp_soc_tx_capt;
#endif
	/* monitor stats */
	struct dp_mon_soc_stats stats;
};

#ifdef WLAN_TELEMETRY_STATS_SUPPORT
struct dp_mon_peer_airtime_consumption {
	uint32_t consumption;
	uint32_t avg_consumption_per_sec;
};
#endif

/**
 * struct dp_mon_peer_stats - Monitor peer stats
 * @tx: tx stats
 * @rx: rx stats
 * @airtime_consumption: airtime consumption per access category
 */
struct dp_mon_peer_stats {
#ifdef QCA_ENHANCED_STATS_SUPPORT
	dp_mon_peer_tx_stats tx;
	dp_mon_peer_rx_stats rx;
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
	struct dp_mon_peer_airtime_consumption airtime_consumption[WME_AC_MAX];
#endif
#endif
};

struct dp_mon_peer {
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	struct dp_peer_tx_capture tx_capture;
#endif
#ifdef FEATURE_PERPKT_INFO
	/* delayed ba ppdu stats handling */
	struct cdp_delayed_tx_completion_ppdu_user delayed_ba_ppdu_stats;
	/* delayed ba flag */
	bool last_delayed_ba;
	/* delayed ba ppdu id */
	uint32_t last_delayed_ba_ppduid;
#endif
	uint8_t tx_cap_enabled:1, /* Peer's tx-capture is enabled */
		rx_cap_enabled:1; /* Peer's rx-capture is enabled */

	/* Peer level flag to check peer based pktlog enabled or
	 * disabled
	 */
	uint8_t peer_based_pktlog_filter;

	/* Monitor peer stats */
	struct dp_mon_peer_stats stats;

	/* peer extended statistics context */
	struct cdp_peer_rate_stats_ctx *peerstats_ctx;
};

struct dp_rx_mon_rssi_offset {
	/* Temperature based rssi offset */
	int32_t rssi_temp_offset;
	/* Low noise amplifier bypass offset */
	int32_t xlna_bypass_offset;
	/* Low noise amplifier bypass threshold */
	int32_t xlna_bypass_threshold;
	/* 3 Bytes of xbar_config are used for RF to BB mapping */
	uint32_t xbar_config;
	/* min noise floor in active chains per channel */
	int8_t min_nf_dbm;
	/* this value is sum of temp_oofset + min_nf*/
	int32_t rssi_offset;
};

struct  dp_mon_pdev {
	/* monitor */
	bool monitor_configured;

	struct dp_mon_filter **filter;	/* Monitor Filter pointer */

	/* advance filter mode and type*/
	uint8_t mon_filter_mode;
	uint16_t fp_mgmt_filter;
	uint16_t fp_ctrl_filter;
	uint16_t fp_data_filter;
	uint16_t mo_mgmt_filter;
	uint16_t mo_ctrl_filter;
	uint16_t mo_data_filter;
	uint16_t md_data_filter;

#ifdef WLAN_TX_PKT_CAPTURE_ENH
	struct dp_pdev_tx_capture tx_capture;
	bool stop_tx_capture_work_q_timer;
#endif

	/* tx packet capture enhancement */
	enum cdp_tx_enh_capture_mode tx_capture_enabled;
	/* Stuck count on monitor destination ring MPDU process */
	uint32_t mon_dest_ring_stuck_cnt;
	/* monitor mode lock */
	qdf_spinlock_t mon_lock;

	/* Monitor mode operation channel */
	int mon_chan_num;

	/* Monitor mode operation frequency */
	qdf_freq_t mon_chan_freq;

	/* Monitor mode band */
	enum reg_wifi_band mon_chan_band;

	uint32_t mon_ppdu_status;
	/* monitor mode status/destination ring PPDU and MPDU count */
	struct cdp_pdev_mon_stats rx_mon_stats;
	/* Monitor mode interface and status storage */
	struct dp_vdev *mvdev;
	struct cdp_mon_status rx_mon_recv_status;
	/* to track duplicate link descriptor indications by HW for a WAR */
	uint64_t mon_last_linkdesc_paddr;
	/* to track duplicate buffer indications by HW for a WAR */
	uint32_t mon_last_buf_cookie;

#ifdef QCA_SUPPORT_FULL_MON
	/* List to maintain all MPDUs for a PPDU in monitor mode */
	TAILQ_HEAD(, dp_mon_mpdu) mon_mpdu_q;

	/* TODO: define per-user mpdu list
	 * struct dp_mon_mpdu_list mpdu_list[MAX_MU_USERS];
	 */
	struct hal_rx_mon_desc_info *mon_desc;
#endif
	/* Flag to hold on to monitor destination ring */
	bool hold_mon_dest_ring;

	/* Flag to inidicate monitor rings are initialized */
	uint8_t pdev_mon_init;
#ifndef REMOVE_PKT_LOG
	bool pkt_log_init;
	struct pktlog_dev_t *pl_dev; /* Pktlog pdev */
#endif /* #ifndef REMOVE_PKT_LOG */

	/* Smart Mesh */
	bool filter_neighbour_peers;

	/*flag to indicate neighbour_peers_list not empty */
	bool neighbour_peers_added;
	/* smart mesh mutex */
	qdf_spinlock_t neighbour_peer_mutex;
	/* Neighnour peer list */
	TAILQ_HEAD(, dp_neighbour_peer) neighbour_peers_list;
	/* Enhanced Stats is enabled */
	bool enhanced_stats_en;
	qdf_nbuf_queue_t rx_status_q;

	/* 128 bytes mpdu header queue per user for ppdu */
	qdf_nbuf_queue_t mpdu_q[MAX_MU_USERS];

	/* is this a mpdu header TLV and not msdu header TLV */
	bool is_mpdu_hdr[MAX_MU_USERS];

	/* per user 128 bytes msdu header list for MPDU */
	struct msdu_list msdu_list[MAX_MU_USERS];

	/* RX enhanced capture mode */
	uint8_t rx_enh_capture_mode;
	/* Rx per peer enhanced capture mode */
	bool rx_enh_capture_peer;
	struct dp_vdev *rx_enh_monitor_vdev;
	/* RX enhanced capture trailer enable/disable flag */
	bool is_rx_enh_capture_trailer_enabled;
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	/* RX per MPDU/PPDU information */
	struct cdp_rx_indication_mpdu mpdu_ind;
#endif

	/* Packet log mode */
	uint8_t rx_pktlog_mode;
	/* Enable pktlog logging cbf */
	bool rx_pktlog_cbf;

#ifdef BE_PKTLOG_SUPPORT
	/* Enable pktlog logging hybrid */
	bool pktlog_hybrid_mode;
#endif
	bool tx_sniffer_enable;
	/* mirror copy mode */
	enum m_copy_mode mcopy_mode;
	bool bpr_enable;
	/* Pdev level flag to check peer based pktlog enabled or
	 * disabled
	 */
	uint8_t dp_peer_based_pktlog;

#ifdef WLAN_ATF_ENABLE
	/* ATF stats enable */
	bool dp_atf_stats_enable;
#endif

	/* Maintains first status buffer's paddr of a PPDU */
	uint64_t status_buf_addr;
	struct hal_rx_ppdu_info ppdu_info;

	/* ppdu_id of last received HTT TX stats */
	uint32_t last_ppdu_id;
	struct {
		uint8_t last_user;
		qdf_nbuf_t buf;
	} tx_ppdu_info;

	struct {
		uint32_t tx_ppdu_id;
		uint16_t tx_peer_id;
		uint32_t rx_ppdu_id;
	} m_copy_id;

	/* To check if PPDU Tx stats are enabled for Pktlog */
	bool pktlog_ppdu_stats;

#ifdef ATH_SUPPORT_NAC_RSSI
	bool nac_rssi_filtering;
#endif

	/* ppdu_stats lock for queue concurrency between cores*/
	qdf_spinlock_t ppdu_stats_lock;

	/* list of ppdu tlvs */
	TAILQ_HEAD(, ppdu_info) ppdu_info_list;
	TAILQ_HEAD(, ppdu_info) sched_comp_ppdu_list;

	uint32_t sched_comp_list_depth;
	uint16_t delivered_sched_cmdid;
	uint16_t last_sched_cmdid;
	uint32_t tlv_count;
	uint32_t list_depth;

	struct {
		qdf_nbuf_t last_nbuf; /*Ptr to mgmt last buf */
		uint8_t *mgmt_buf; /* Ptr to mgmt. payload in HTT ppdu stats */
		uint32_t mgmt_buf_len; /* Len of mgmt. payload in ppdu stats */
		uint32_t ppdu_id;
	} mgmtctrl_frm_info;
	/* Context of cal client timer */
	struct cdp_cal_client *cal_client_ctx;
	uint32_t *ppdu_tlv_buf; /* Buffer to hold HTT ppdu stats TLVs*/

	qdf_nbuf_t mcopy_status_nbuf;
	bool is_dp_mon_pdev_initialized;
	/* indicates if spcl vap is configured */
	bool scan_spcl_vap_configured;
	bool undecoded_metadata_capture;
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	uint32_t phyrx_error_mask;
	uint32_t phyrx_error_mask_cont;
#endif
#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
	/* enable spcl vap stats reset on ch change */
	bool reset_scan_spcl_vap_stats_enable;
#endif
	bool is_tlv_hdr_64_bit;

	/* Invalid monitor peer to account for stats in mcopy mode */
	struct dp_mon_peer *invalid_mon_peer;

	bool rssi_dbm_conv_support;
	struct dp_rx_mon_rssi_offset rssi_offsets;
};

struct  dp_mon_vdev {
	/* callback to hand rx monitor 802.11 MPDU to the OS shim */
	ol_txrx_rx_mon_fp osif_rx_mon;
#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
	struct cdp_scan_spcl_vap_stats *scan_spcl_vap_stats;
#endif
};

#if defined(QCA_TX_CAPTURE_SUPPORT) || defined(QCA_ENHANCED_STATS_SUPPORT)
void dp_deliver_mgmt_frm(struct dp_pdev *pdev, qdf_nbuf_t nbuf);
#else
static inline
void dp_deliver_mgmt_frm(struct dp_pdev *pdev, qdf_nbuf_t nbuf)
{
}
#endif

#if defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) ||\
	defined(WLAN_SUPPORT_RX_FLOW_TAG)
/**
 * dp_rx_mon_update_protocol_flow_tag() - Performs necessary checks for monitor
 *                                       mode and then tags appropriate packets
 * @soc: core txrx main context
 * @dp_pdev: pdev on which packet is received
 * @msdu: QDF packet buffer on which the protocol tag should be set
 * @rx_desc: base address where the RX TLVs start
 *
 * Return: void
 */
void dp_rx_mon_update_protocol_flow_tag(struct dp_soc *soc,
					struct dp_pdev *dp_pdev,
					qdf_nbuf_t msdu, void *rx_desc);
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG || WLAN_SUPPORT_RX_FLOW_TAG */

#if !defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) &&\
	!defined(WLAN_SUPPORT_RX_FLOW_TAG)
/**
 * dp_rx_mon_update_protocol_flow_tag() - Performs necessary checks for monitor
 *                                       mode and then tags appropriate packets
 * @soc: core txrx main context
 * @dp_pdev: pdev on which packet is received
 * @msdu: QDF packet buffer on which the protocol tag should be set
 * @rx_desc: base address where the RX TLVs start
 *
 * Return: void
 */
static inline
void dp_rx_mon_update_protocol_flow_tag(struct dp_soc *soc,
					struct dp_pdev *dp_pdev,
					qdf_nbuf_t msdu, void *rx_desc)
{
}
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG || WLAN_SUPPORT_RX_FLOW_TAG */

#ifndef WLAN_TX_PKT_CAPTURE_ENH
/**
 * dp_peer_tid_queue_init() - Initialize ppdu stats queue per TID
 * @peer: Datapath peer
 *
 */
static inline void dp_peer_tid_queue_init(struct dp_peer *peer)
{
}

/**
 * dp_peer_tid_queue_cleanup() - remove ppdu stats queue per TID
 * @peer: Datapath peer
 *
 */
static inline void dp_peer_tid_queue_cleanup(struct dp_peer *peer)
{
}

/**
 * dp_peer_update_80211_hdr() - dp peer update 80211 hdr
 * @vdev: Datapath vdev
 * @peer: Datapath peer
 *
 */
static inline void
dp_peer_update_80211_hdr(struct dp_vdev *vdev, struct dp_peer *peer)
{
}

/**
 * dp_get_peer_tx_capture_stats() - to get peer tx capture stats
 * @peer: DP PEER handle
 * @stats: pointor to peer tx capture stats
 *
 * return: QDF_STATUS
 */
static inline QDF_STATUS
dp_get_peer_tx_capture_stats(struct dp_peer *peer,
			     struct cdp_peer_tx_capture_stats *stats)
{
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_get_pdev_tx_capture_stats() - to get pdev tx capture stats
 * @pdev: DP PDEV handle
 * @stats: pointor to pdev tx capture stats
 *
 * return: QDF_STATUS
 */
static inline QDF_STATUS
dp_get_pdev_tx_capture_stats(struct dp_pdev *pdev,
			     struct cdp_pdev_tx_capture_stats *stats)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef WLAN_TX_PKT_CAPTURE_ENH
extern uint8_t
dp_cpu_ring_map[DP_NSS_CPU_RING_MAP_MAX][WLAN_CFG_INT_NUM_CONTEXTS_MAX];
#endif

/**
 * dp_htt_get_ppdu_sniffer_ampdu_tlv_bitmap() - Get ppdu stats tlv
 * bitmap for sniffer mode
 * @bitmap: received bitmap
 *
 * Return: expected bitmap value, returns zero if doesn't match with
 * either 64-bit Tx window or 256-bit window tlv bitmap
 */
int
dp_htt_get_ppdu_sniffer_ampdu_tlv_bitmap(uint32_t bitmap);

#if (defined(DP_CON_MON) || defined(WDI_EVENT_ENABLE)) &&\
	(!defined(REMOVE_PKT_LOG))
/**
 * dp_pkt_log_init() - API to initialize packet log
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @scn: HIF context
 *
 * Return: none
 */
void dp_pkt_log_init(struct cdp_soc_t *soc_hdl, uint8_t pdev_id, void *scn);
#else
static inline void
dp_pkt_log_init(struct cdp_soc_t *soc_hdl, uint8_t pdev_id, void *scn)
{
}
#endif

#if defined(WDI_EVENT_ENABLE) && defined(QCA_ENHANCED_STATS_SUPPORT)
QDF_STATUS dp_peer_stats_notify(struct dp_pdev *pdev, struct dp_peer *peer);
#else
static inline QDF_STATUS dp_peer_stats_notify(struct dp_pdev *pdev,
					      struct dp_peer *peer)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(FEATURE_PERPKT_INFO) && defined(WDI_EVENT_ENABLE)
void dp_send_stats_event(struct dp_pdev *pdev, struct dp_peer *peer,
			 uint16_t peer_id);
#else
static inline
void dp_send_stats_event(struct dp_pdev *pdev, struct dp_peer *peer,
			 uint16_t peer_id)
{
}
#endif

#ifndef WLAN_TX_PKT_CAPTURE_ENH
/**
 * dp_tx_ppdu_stats_process - Deferred PPDU stats handler
 * @context: Opaque work context (PDEV)
 *
 * Return: none
 */
static  inline void dp_tx_ppdu_stats_process(void *context)
{
}

/**
 * dp_tx_capture_htt_frame_counter: increment counter for htt_frame_type
 * @pdev: DP pdev handle
 * @htt_frame_type: htt frame type received from fw
 *
 * return: void
 */
static inline
void dp_tx_capture_htt_frame_counter(struct dp_pdev *pdev,
				     uint32_t htt_frame_type)
{
}

#endif

/**
 * dp_rx_cookie_2_mon_link_desc_va() - Converts cookie to a virtual address of
 *				   the MSDU Link Descriptor
 * @pdev: core txrx pdev context
 * @buf_info: buf_info includes cookie that used to lookup virtual address of
 * link descriptor. Normally this is just an index into a per pdev array.
 * @mac_id: mac id
 *
 * This is the VA of the link descriptor in monitor mode destination ring,
 * that HAL layer later uses to retrieve the list of MSDU's for a given MPDU.
 *
 * Return: void *: Virtual Address of the Rx descriptor
 */
static inline
void *dp_rx_cookie_2_mon_link_desc_va(struct dp_pdev *pdev,
				      struct hal_buf_info *buf_info,
				      int mac_id)
{
	void *link_desc_va;
	struct qdf_mem_multi_page_t *pages;
	uint16_t page_id = LINK_DESC_COOKIE_PAGE_ID(buf_info->sw_cookie);
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc)
		return NULL;

	pages = &mon_soc->mon_link_desc_pages[mac_id];
	if (!pages)
		return NULL;

	if (qdf_unlikely(page_id >= pages->num_pages))
		return NULL;

	link_desc_va = pages->dma_pages[page_id].page_v_addr_start +
		(buf_info->paddr - pages->dma_pages[page_id].page_p_addr);

	return link_desc_va;
}

/**
 * dp_soc_is_full_mon_enable() - Return if full monitor mode is enabled
 * @pdev: point to dp pdev
 *
 * Return: Full monitor mode status
 */
static inline bool dp_soc_is_full_mon_enable(struct dp_pdev *pdev)
{
	return (pdev->soc->monitor_soc->full_mon_mode &&
		pdev->monitor_pdev->monitor_configured) ? true : false;
}

/**
 * dp_monitor_is_enable_mcopy_mode() - check if mcopy mode is enabled
 * @pdev: point to dp pdev
 *
 * Return: true if mcopy mode is enabled
 */
static inline bool dp_monitor_is_enable_mcopy_mode(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return false;

	return pdev->monitor_pdev->mcopy_mode;
}

/**
 * dp_monitor_is_enable_tx_sniffer() - check if tx sniffer is enabled
 * @pdev: point to dp pdev
 *
 * Return: true if tx sniffer is enabled
 */
static inline bool dp_monitor_is_enable_tx_sniffer(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return false;

	return pdev->monitor_pdev->tx_sniffer_enable;
}

/**
 * dp_monitor_is_configured() - check if monitor configured is set
 * @pdev: point to dp pdev
 *
 * Return: true if monitor configured is set
 */
static inline bool dp_monitor_is_configured(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return false;

	return pdev->monitor_pdev->monitor_configured;
}

/**
 * dp_monitor_check_com_info_ppdu_id() - check if msdu ppdu_id matches with
 * com info ppdu_id
 * @pdev: point to dp pdev
 * @rx_desc: point to rx_desc
 *
 * Return: success if ppdu_id matches
 */
static inline QDF_STATUS dp_monitor_check_com_info_ppdu_id(struct dp_pdev *pdev,
							   void *rx_desc)
{
	struct cdp_mon_status *rs;
	struct dp_mon_pdev *mon_pdev;
	uint32_t msdu_ppdu_id = 0;

	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;
	if (qdf_likely(1 != mon_pdev->ppdu_info.rx_status.rxpcu_filter_pass))
		return QDF_STATUS_E_FAILURE;

	rs = &pdev->monitor_pdev->rx_mon_recv_status;
	if (!rs || rs->cdp_rs_rxdma_err)
		return QDF_STATUS_E_FAILURE;

	msdu_ppdu_id = hal_rx_get_ppdu_id(pdev->soc->hal_soc, rx_desc);
	if (msdu_ppdu_id != mon_pdev->ppdu_info.com_info.ppdu_id) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_ERROR,
			  "msdu_ppdu_id=%x,com_info.ppdu_id=%x",
			  msdu_ppdu_id,
			  mon_pdev->ppdu_info.com_info.ppdu_id);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_monitor_get_rx_status() - get rx status
 * @pdev: point to dp pdev
 *
 * Return: return rx status pointer
 */
static inline struct mon_rx_status*
dp_monitor_get_rx_status(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return NULL;

	return &pdev->monitor_pdev->ppdu_info.rx_status;
}

/**
 * dp_monitor_is_chan_band_known() - check if monitor chan band known
 * @pdev: point to dp pdev
 *
 * Return: true if chan band known
 */
static inline bool dp_monitor_is_chan_band_known(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return false;

	if (pdev->monitor_pdev->mon_chan_band != REG_BAND_UNKNOWN)
		return true;

	return false;
}

/**
 * dp_monitor_get_chan_band() - get chan band
 * @pdev: point to dp pdev
 *
 * Return: wifi channel band
 */
static inline enum reg_wifi_band
dp_monitor_get_chan_band(struct dp_pdev *pdev)
{
	return pdev->monitor_pdev->mon_chan_band;
}

/**
 * dp_monitor_print_tx_stats() - print tx stats from monitor pdev
 * @pdev: point to dp pdev
 *
 */
static inline void dp_monitor_print_tx_stats(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return;

	DP_PRINT_STATS("ppdu info schedule completion list depth: %d",
		       pdev->monitor_pdev->sched_comp_list_depth);
	DP_PRINT_STATS("delivered sched cmdid: %d",
		       pdev->monitor_pdev->delivered_sched_cmdid);
	DP_PRINT_STATS("cur sched cmdid: %d",
		       pdev->monitor_pdev->last_sched_cmdid);
	DP_PRINT_STATS("ppdu info list depth: %d",
		       pdev->monitor_pdev->list_depth);
}

/**
 * dp_monitor_set_chan_num() - set channel number
 * @pdev: point to dp pdev
 * @chan_num: channel number
 *
 */
static inline void dp_monitor_set_chan_num(struct dp_pdev *pdev, int chan_num)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return;

	pdev->monitor_pdev->mon_chan_num = chan_num;
}

/**
 * dp_monitor_get_chan_num() - get channel number
 * @pdev: DP pdev handle
 *
 * Return: channel number
 */
static inline int dp_monitor_get_chan_num(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return 0;

	return pdev->monitor_pdev->mon_chan_num;
}

/**
 * dp_monitor_set_chan_freq() - set channel frequency
 * @pdev: point to dp pdev
 * @chan_freq: channel frequency
 *
 */
static inline void
dp_monitor_set_chan_freq(struct dp_pdev *pdev, qdf_freq_t chan_freq)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return;

	pdev->monitor_pdev->mon_chan_freq = chan_freq;
}

/**
 * dp_monitor_get_chan_freq() - get channel frequency
 * @pdev: DP pdev handle
 *
 * Return: channel frequency
 */
static inline qdf_freq_t
dp_monitor_get_chan_freq(struct dp_pdev *pdev)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return 0;

	return pdev->monitor_pdev->mon_chan_freq;
}

/**
 * dp_monitor_set_chan_band() - set channel band
 * @pdev: point to dp pdev
 * @chan_band: channel band
 *
 */
static inline void
dp_monitor_set_chan_band(struct dp_pdev *pdev, enum reg_wifi_band chan_band)
{
	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return;

	pdev->monitor_pdev->mon_chan_band = chan_band;
}

/**
 * dp_monitor_get_mpdu_status() - get mpdu status
 * @pdev: point to dp pdev
 * @soc: point to dp soc
 * @rx_tlv_hdr: point to rx tlv header
 *
 */
static inline void dp_monitor_get_mpdu_status(struct dp_pdev *pdev,
					      struct dp_soc *soc,
					      uint8_t *rx_tlv_hdr)
{
	struct dp_mon_pdev *mon_pdev;

	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return;

	mon_pdev = pdev->monitor_pdev;
	hal_rx_mon_hw_desc_get_mpdu_status(soc->hal_soc, rx_tlv_hdr,
					   &mon_pdev->ppdu_info.rx_status);
}

#ifdef FEATURE_NAC_RSSI
/**
 * dp_monitor_drop_inv_peer_pkts() - drop invalid peer pkts
 * @vdev: point to dp vdev
 *
 * Return: success if sta mode and filter for neighbour peers enabled
 */
static inline QDF_STATUS dp_monitor_drop_inv_peer_pkts(struct dp_vdev *vdev)
{
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_soc *soc = pdev->soc;

	if (!soc->monitor_soc)
		return QDF_STATUS_E_FAILURE;

	if (!soc->monitor_soc->hw_nac_monitor_support &&
	    pdev->monitor_pdev->filter_neighbour_peers &&
	    vdev->opmode == wlan_op_mode_sta)
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}
#else
static inline QDF_STATUS dp_monitor_drop_inv_peer_pkts(struct dp_vdev *vdev)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * dp_peer_ppdu_delayed_ba_init() - Initialize ppdu in peer
 * @peer: Datapath peer
 *
 * return: void
 */
#ifdef FEATURE_PERPKT_INFO
static inline void dp_peer_ppdu_delayed_ba_init(struct dp_peer *peer)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;

	if (!mon_peer)
		return;

	qdf_mem_zero(&mon_peer->delayed_ba_ppdu_stats,
		     sizeof(struct cdp_delayed_tx_completion_ppdu_user));
	mon_peer->last_delayed_ba = false;
	mon_peer->last_delayed_ba_ppduid = 0;
}
#else
static inline void dp_peer_ppdu_delayed_ba_init(struct dp_peer *peer)
{
}
#endif

/**
 * dp_monitor_vdev_register_osif() - Register osif rx mon
 * @vdev: point to vdev
 * @txrx_ops: point to ol txrx ops
 *
 * Return: void
 */
static inline void dp_monitor_vdev_register_osif(struct dp_vdev *vdev,
						 struct ol_txrx_ops *txrx_ops)
{
	if (!vdev->monitor_vdev)
		return;

	vdev->monitor_vdev->osif_rx_mon = txrx_ops->rx.mon;
}

/**
 * dp_monitor_get_monitor_vdev_from_pdev() - Get monitor vdev
 * @pdev: point to pdev
 *
 * Return: pointer to vdev
 */
static inline struct dp_vdev*
dp_monitor_get_monitor_vdev_from_pdev(struct dp_pdev *pdev)
{
	if (!pdev || !pdev->monitor_pdev || !pdev->monitor_pdev->mvdev)
		return NULL;

	return pdev->monitor_pdev->mvdev;
}

/**
 * dp_monitor_is_vdev_timer_running() - Get vdev timer status
 * @soc: point to soc
 *
 * Return: true if timer running
 */
static inline bool dp_monitor_is_vdev_timer_running(struct dp_soc *soc)
{
	if (qdf_unlikely(!soc || !soc->monitor_soc))
		return false;

	return !!(soc->monitor_soc->mon_vdev_timer_state &
		  MON_VDEV_TIMER_RUNNING);
}

/**
 * dp_monitor_get_link_desc_pages() - Get link desc pages
 * @soc: point to soc
 * @mac_id: mac id
 *
 * Return: return point to link desc pages
 */
static inline struct qdf_mem_multi_page_t*
dp_monitor_get_link_desc_pages(struct dp_soc *soc, uint32_t mac_id)
{
	if (qdf_unlikely(!soc || !soc->monitor_soc))
		return NULL;

	return &soc->monitor_soc->mon_link_desc_pages[mac_id];
}

/**
 * dp_monitor_get_total_link_descs() - Get total link descs
 * @soc: point to soc
 * @mac_id: mac id
 *
 * Return: return point total link descs
 */
static inline uint32_t *
dp_monitor_get_total_link_descs(struct dp_soc *soc, uint32_t mac_id)
{
	return &soc->monitor_soc->total_mon_link_descs[mac_id];
}

/**
 * dp_monitor_pdev_attach() - Monitor pdev attach
 * @pdev: point to pdev
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_pdev_attach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	/*
	 * mon_soc uninitialized modular support enabled
	 * monitor related attach/detach/init/deinit
	 * will be done while monitor insmod
	 */
	if (!mon_soc)
		return QDF_STATUS_SUCCESS;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_attach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_pdev_attach(pdev);
}

/**
 * dp_monitor_pdev_detach() - Monitor pdev detach
 * @pdev: point to pdev
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_pdev_detach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	/*
	 * mon_soc uninitialized modular support enabled
	 * monitor related attach/detach/init/deinit
	 * will be done while monitor insmod
	 */
	if (!mon_soc)
		return QDF_STATUS_SUCCESS;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_detach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_pdev_detach(pdev);
}

/**
 * dp_monitor_vdev_attach() - Monitor vdev attach
 * @vdev: point to vdev
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_vdev_attach(struct dp_vdev *vdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = vdev->pdev->soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_attach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_vdev_attach(vdev);
}

/**
 * dp_monitor_vdev_detach() - Monitor vdev detach
 * @vdev: point to vdev
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_vdev_detach(struct dp_vdev *vdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = vdev->pdev->soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_detach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_vdev_detach(vdev);
}

/**
 * dp_monitor_peer_attach() - Monitor peer attach
 * @soc: point to soc
 * @peer: point to peer
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_peer_attach(struct dp_soc *soc,
						struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_attach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_peer_attach(peer);
}

/**
 * dp_monitor_peer_detach() - Monitor peer detach
 * @soc: point to soc
 * @peer: point to peer
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_peer_detach(struct dp_soc *soc,
						struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_detach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_peer_detach(peer);
}

/**
 * dp_monitor_peer_get_peerstats_ctx() - Get peerstats context from monitor peer
 * @soc: Datapath soc handle
 * @peer: Datapath peer handle
 *
 * Return: peer stats context
 */
static inline struct cdp_peer_rate_stats_ctx*
dp_monitor_peer_get_peerstats_ctx(struct dp_soc *soc, struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return NULL;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_get_peerstats_ctx) {
		dp_mon_debug("callback not registered");
		return NULL;
	}

	return monitor_ops->mon_peer_get_peerstats_ctx(peer);
}

/**
 * dp_monitor_peer_reset_stats() - Reset monitor peer stats
 * @soc: Datapath soc handle
 * @peer: Datapath peer handle
 *
 * Return: none
 */
static inline void dp_monitor_peer_reset_stats(struct dp_soc *soc,
					       struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_reset_stats) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_peer_reset_stats(peer);
}

/**
 * dp_monitor_peer_get_stats() - Get monitor peer stats
 * @soc: Datapath soc handle
 * @peer: Datapath peer handle
 * @arg: Pointer to stats struct
 * @type: Update type
 *
 * Return: none
 */
static inline
void dp_monitor_peer_get_stats(struct dp_soc *soc, struct dp_peer *peer,
			       void *arg, enum cdp_stat_update_type type)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_get_stats) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_peer_get_stats(peer, arg, type);
}

/**
 * dp_monitor_invalid_peer_update_pdev_stats() - Update pdev stats from
 *						invalid monitor peer
 * @soc: Datapath soc handle
 * @pdev: Datapath pdev handle
 *
 * Return: none
 */
static inline
void dp_monitor_invalid_peer_update_pdev_stats(struct dp_soc *soc,
					       struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_invalid_peer_update_pdev_stats) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_invalid_peer_update_pdev_stats(pdev);
}

/**
 * dp_monitor_peer_get_stats_param() - Get stats param value from monitor peer
 * @soc: Datapath soc handle
 * @peer: Datapath peer handle
 * @type: Stats type requested
 * @buf: Pointer to buffer for stats param
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_monitor_peer_get_stats_param(struct dp_soc *soc, struct dp_peer *peer,
				enum cdp_peer_stats_type type,
				cdp_peer_stats_param_t *buf)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_get_stats_param) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_peer_get_stats_param(peer, type, buf);
}

/**
 * dp_monitor_pdev_init() - Monitor pdev init
 * @pdev: point to pdev
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_pdev_init(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	/*
	 * mon_soc uninitialized when modular support enabled
	 * monitor related attach/detach/init/deinit
	 * will be done while monitor insmod
	 */
	if (!mon_soc)
		return QDF_STATUS_SUCCESS;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_init) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_pdev_init(pdev);
}

/**
 * dp_monitor_pdev_deinit() - Monitor pdev deinit
 * @pdev: point to pdev
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_pdev_deinit(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	/*
	 * mon_soc uninitialized modular when support enabled
	 * monitor related attach/detach/init/deinit
	 * will be done while monitor insmod
	 */
	if (!mon_soc)
		return QDF_STATUS_SUCCESS;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_deinit) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_pdev_deinit(pdev);
}

/**
 * dp_monitor_soc_cfg_init() - Monitor sco cfg init
 * @soc: point to soc
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_soc_cfg_init(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	/*
	 * this API is getting call from dp_soc_init,
	 * mon_soc will be uninitialized when monitor support enabled
	 * So returning QDF_STATUS_SUCCESS.
	 * soc cfg init will be done while monitor insmod.
	 */
	if (!mon_soc)
		return QDF_STATUS_SUCCESS;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_soc_cfg_init) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_soc_cfg_init(soc);
}

/**
 * dp_monitor_config_debug_sniffer() - Monitor config debug sniffer
 * @pdev: point to pdev
 * @val: val
 *
 * Return: return QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_config_debug_sniffer(struct dp_pdev *pdev,
							 int val)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_config_debug_sniffer) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_config_debug_sniffer(pdev, val);
}

/**
 * dp_monitor_flush_rings() - Flush monitor rings
 * @soc: point to soc
 *
 * Return: None
 */
static inline void dp_monitor_flush_rings(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_flush_rings) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_flush_rings(soc);
}

/**
 * dp_monitor_config_undecoded_metadata_capture() - Monitor config
 * undecoded metadata capture
 * @pdev: point to pdev
 * @val: val
 *
 * Return: return QDF_STATUS
 */
#ifdef QCA_UNDECODED_METADATA_SUPPORT
static inline
QDF_STATUS dp_monitor_config_undecoded_metadata_capture(struct dp_pdev *pdev,
							int val)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops ||
	    !monitor_ops->mon_config_undecoded_metadata_capture) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_config_undecoded_metadata_capture(pdev, val);
}

static inline QDF_STATUS
dp_monitor_config_undecoded_metadata_phyrx_error_mask(struct dp_pdev *pdev,
						      int mask, int mask_cont)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc)
		return QDF_STATUS_E_FAILURE;

	if (!mon_pdev)
		return QDF_STATUS_E_FAILURE;

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops ||
	    !monitor_ops->mon_config_undecoded_metadata_capture) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	if (!mon_pdev->undecoded_metadata_capture) {
		qdf_info("mask:0x%x mask_cont:0x%x", mask, mask_cont);
		return QDF_STATUS_SUCCESS;
	}

	mon_pdev->phyrx_error_mask = mask;
	mon_pdev->phyrx_error_mask_cont = mask_cont;

	return monitor_ops->mon_config_undecoded_metadata_capture(pdev, 1);
}

static inline QDF_STATUS
dp_monitor_get_undecoded_metadata_phyrx_error_mask(struct dp_pdev *pdev,
						   int *mask, int *mask_cont)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev)
		return QDF_STATUS_E_FAILURE;

	*mask = mon_pdev->phyrx_error_mask;
	*mask_cont = mon_pdev->phyrx_error_mask_cont;

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS dp_monitor_config_undecoded_metadata_capture(struct dp_pdev *pdev,
							int val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_monitor_config_undecoded_metadata_phyrx_error_mask(struct dp_pdev *pdev,
						      int mask1, int mask2)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_monitor_get_undecoded_metadata_phyrx_error_mask(struct dp_pdev *pdev,
						   int *mask, int *mask_cont)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_UNDECODED_METADATA_SUPPORT */

/**
 * dp_monitor_htt_srng_setup() - Setup htt srng
 * @soc: point to soc
 * @pdev: point to pdev
 * @mac_id: lmac id
 * @mac_for_pdev: pdev id
 *
 * Return: QDF_STATUS
 */
#if !defined(DISABLE_MON_CONFIG)
static inline QDF_STATUS dp_monitor_htt_srng_setup(struct dp_soc *soc,
						   struct dp_pdev *pdev,
						   int mac_id,
						   int mac_for_pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_SUCCESS;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_htt_srng_setup) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_pdev_htt_srng_setup(soc, pdev, mac_id,
						    mac_for_pdev);
}

static inline QDF_STATUS dp_monitor_soc_htt_srng_setup(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_SUCCESS;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_soc_htt_srng_setup) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_soc_htt_srng_setup(soc);
}
#else
static inline QDF_STATUS dp_monitor_htt_srng_setup(struct dp_soc *soc,
						   struct dp_pdev *pdev,
						   int mac_id,
						   int mac_for_pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_monitor_service_mon_rings() - service monitor rings
 * @soc: point to soc
 * @quota: reap budget
 *
 * Return: None
 */
#if defined(DP_CON_MON)
static inline
void dp_monitor_service_mon_rings(struct dp_soc *soc, uint32_t quota)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_service_rings) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_service_rings(soc, quota);
}
#endif

/**
 * dp_monitor_process() - Process monitor
 * @soc: point to soc
 * @int_ctx: interrupt ctx
 * @mac_id: lma
 * @quota:
 *
 * Return: None
 */
#ifndef DISABLE_MON_CONFIG
static inline
uint32_t dp_monitor_process(struct dp_soc *soc, struct dp_intr *int_ctx,
			       uint32_t mac_id, uint32_t quota)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	if (!mon_soc->mon_rx_process) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return mon_soc->mon_rx_process(soc, int_ctx, mac_id, quota);
}

static inline
uint32_t dp_tx_mon_process(struct dp_soc *soc, struct dp_intr *int_ctx,
			   uint32_t mac_id, uint32_t quota)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_tx_process) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return monitor_ops->mon_tx_process(soc, int_ctx, mac_id, quota);
}

static inline
uint32_t dp_tx_mon_buf_refill(struct dp_intr *int_ctx)
{
	struct dp_soc *soc = int_ctx->soc;
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->tx_mon_refill_buf_ring) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return monitor_ops->tx_mon_refill_buf_ring(int_ctx);
}

static inline
uint32_t dp_rx_mon_buf_refill(struct dp_intr *int_ctx)
{
	struct dp_soc *soc = int_ctx->soc;
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_mon_refill_buf_ring) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return monitor_ops->rx_mon_refill_buf_ring(int_ctx);
}

static inline
void dp_print_txmon_ring_stat_from_hal(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->print_txmon_ring_stat) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->print_txmon_ring_stat(pdev);
}

#else
static inline
uint32_t dp_monitor_process(struct dp_soc *soc, struct dp_intr *int_ctx,
			       uint32_t mac_id, uint32_t quota)
{
	return 0;
}

static inline uint32_t
dp_tx_mon_process(struct dp_soc *soc, struct dp_intr *int_ctx,
		  uint32_t mac_id, uint32_t quota)
{
	return 0;
}

static inline
uint32_t dp_tx_mon_buf_refill(struct dp_intr *int_ctx)
{
	return 0;
}

static inline
uint32_t dp_rx_mon_buf_refill(struct dp_intr *int_ctx)
{
	return 0;
}

static inline
void dp_print_txmon_ring_stat_from_hal(struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_monitor_drop_packets_for_mac() - monitor_drop_packets_for_mac
 * @pdev: point to pdev
 * @mac_id:
 * @quota:
 *
 * Return:
 */
#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_MAC)
static inline
uint32_t dp_monitor_drop_packets_for_mac(struct dp_pdev *pdev,
					 uint32_t mac_id, uint32_t quota)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_drop_packets_for_mac) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return monitor_ops->mon_drop_packets_for_mac(pdev,
						     mac_id, quota);
}
#else
static inline
uint32_t dp_monitor_drop_packets_for_mac(struct dp_pdev *pdev,
					 uint32_t mac_id, uint32_t quota)
{
	return 0;
}
#endif

/**
 * dp_monitor_peer_tx_init() - peer tx init
 * @pdev: point to pdev
 * @peer: point to peer
 *
 * Return: None
 */
static inline void dp_monitor_peer_tx_init(struct dp_pdev *pdev,
					   struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_tx_init) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_peer_tx_init(pdev, peer);
}

/**
 * dp_monitor_peer_tx_cleanup() - peer tx cleanup
 * @vdev: point to vdev
 * @peer: point to peer
 *
 * Return: None
 */
static inline void dp_monitor_peer_tx_cleanup(struct dp_vdev *vdev,
					      struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = vdev->pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_tx_cleanup) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_peer_tx_cleanup(vdev, peer);
}

#ifdef WIFI_MONITOR_SUPPORT
/**
 * dp_monitor_peer_tid_peer_id_update() - peer tid update
 * @soc: point to soc
 * @peer: point to peer
 * @peer_id: peer id
 *
 * Return: None
 */
static inline
void dp_monitor_peer_tid_peer_id_update(struct dp_soc *soc,
					struct dp_peer *peer,
					uint16_t peer_id)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_tid_peer_id_update) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_peer_tid_peer_id_update(peer, peer_id);
}

/**
 * dp_monitor_tx_ppdu_stats_attach() - Attach tx ppdu stats
 * @pdev: point to pdev
 *
 * Return: None
 */
static inline void dp_monitor_tx_ppdu_stats_attach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_tx_ppdu_stats_attach) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_tx_ppdu_stats_attach(pdev);
}

/**
 * dp_monitor_tx_ppdu_stats_detach() - Detach tx ppdu stats
 * @pdev: point to pdev
 *
 * Return: None
 */
static inline void dp_monitor_tx_ppdu_stats_detach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_tx_ppdu_stats_detach) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_tx_ppdu_stats_detach(pdev);
}

/**
 * dp_monitor_tx_capture_debugfs_init() - Init tx capture debugfs
 * @pdev: point to pdev
 *
 * Return: QDF_STATUS_SUCCESS
 */
static inline
QDF_STATUS dp_monitor_tx_capture_debugfs_init(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_tx_capture_debugfs_init) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_tx_capture_debugfs_init(pdev);
}

/**
 * dp_monitor_peer_tx_capture_filter_check() - Check tx capture filter
 * @pdev: point to pdev
 * @peer: point to peer
 *
 * Return: None
 */
static inline void dp_monitor_peer_tx_capture_filter_check(struct dp_pdev *pdev,
							   struct dp_peer *peer)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_tx_capture_filter_check) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_peer_tx_capture_filter_check(pdev, peer);
}

/**
 * dp_monitor_tx_add_to_comp_queue() - add completion msdu to queue
 *
 * This API returns QDF_STATUS_SUCCESS in case where buffer is added
 * to txmonitor queue successfully caller will not free the buffer in
 * this case. In other cases this API return QDF_STATUS_E_FAILURE and
 * caller frees the buffer
 *
 * @soc: point to soc
 * @desc: point to tx desc
 * @ts: Tx completion status from HAL/HTT descriptor
 * @peer_id: DP peer id
 *
 * Return: QDF_STATUS
 *
 */
static inline
QDF_STATUS dp_monitor_tx_add_to_comp_queue(struct dp_soc *soc,
					   struct dp_tx_desc_s *desc,
					   struct hal_tx_completion_status *ts,
					   uint16_t peer_id)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_tx_add_to_comp_queue) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_tx_add_to_comp_queue(soc, desc, ts, peer_id);
}

static inline
QDF_STATUS monitor_update_msdu_to_list(struct dp_soc *soc,
				       struct dp_pdev *pdev,
				       struct dp_peer *peer,
				       struct hal_tx_completion_status *ts,
				       qdf_nbuf_t netbuf)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_SUCCESS;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_update_msdu_to_list) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_update_msdu_to_list(soc, pdev,
						    peer, ts, netbuf);
}

/**
 * dp_monitor_peer_tx_capture_get_stats - to get Peer Tx Capture stats
 * @soc: DP SOC handle
 * @peer: DP PEER handle
 * @stats: Pointer Peer tx capture stats
 *
 * Return: QDF_STATUS_E_FAILURE or QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
dp_monitor_peer_tx_capture_get_stats(struct dp_soc *soc, struct dp_peer *peer,
				     struct cdp_peer_tx_capture_stats *stats)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_peer_tx_capture_get_stats) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_peer_tx_capture_get_stats(peer, stats);
}

/**
 * dp_monitor_pdev_tx_capture_get_stats - to get pdev tx capture stats
 * @soc: DP SOC handle
 * @pdev: DP PDEV handle
 * @stats: Pointer to pdev tx capture stats
 *
 * Return: QDF_STATUS_E_FAILURE or QDF_STATUS_SUCCESS
 */
static inline QDF_STATUS
dp_monitor_pdev_tx_capture_get_stats(struct dp_soc *soc, struct dp_pdev *pdev,
				     struct cdp_pdev_tx_capture_stats *stats)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_tx_capture_get_stats) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_pdev_tx_capture_get_stats(pdev, stats);
}
#else
static inline
void dp_monitor_peer_tid_peer_id_update(struct dp_soc *soc,
					struct dp_peer *peer,
					uint16_t peer_id)
{
}

static inline void dp_monitor_tx_ppdu_stats_attach(struct dp_pdev *pdev)
{
}

static inline void dp_monitor_tx_ppdu_stats_detach(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_monitor_tx_capture_debugfs_init(struct dp_pdev *pdev)
{
	return QDF_STATUS_E_FAILURE;
}

static inline void dp_monitor_peer_tx_capture_filter_check(struct dp_pdev *pdev,
							   struct dp_peer *peer)
{
}

static inline
QDF_STATUS dp_monitor_tx_add_to_comp_queue(struct dp_soc *soc,
					   struct dp_tx_desc_s *desc,
					   struct hal_tx_completion_status *ts,
					   uint16_t peer_id)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
QDF_STATUS monitor_update_msdu_to_list(struct dp_soc *soc,
				       struct dp_pdev *pdev,
				       struct dp_peer *peer,
				       struct hal_tx_completion_status *ts,
				       qdf_nbuf_t netbuf)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
dp_monitor_peer_tx_capture_get_stats(struct dp_soc *soc, struct dp_peer *peer,
				     struct cdp_peer_tx_capture_stats *stats)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
dp_monitor_pdev_tx_capture_get_stats(struct dp_soc *soc, struct dp_pdev *pdev,
				     struct cdp_pdev_tx_capture_stats *stats)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * dp_monitor_ppdu_stats_ind_handler() - PPDU stats msg handler
 * @soc:     HTT SOC handle
 * @msg_word:    Pointer to payload
 * @htt_t2h_msg: HTT msg nbuf
 *
 * Return: True if buffer should be freed by caller.
 */
#if defined(WDI_EVENT_ENABLE) &&\
	(defined(QCA_ENHANCED_STATS_SUPPORT) || !defined(REMOVE_PKT_LOG))
static inline bool dp_monitor_ppdu_stats_ind_handler(struct htt_soc *soc,
						     uint32_t *msg_word,
						     qdf_nbuf_t htt_t2h_msg)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->dp_soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return true;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_ppdu_stats_ind_handler) {
		dp_mon_debug("callback not registered");
		return true;
	}

	return monitor_ops->mon_ppdu_stats_ind_handler(soc, msg_word,
						       htt_t2h_msg);
}
#else
static inline bool dp_monitor_ppdu_stats_ind_handler(struct htt_soc *soc,
						     uint32_t *msg_word,
						     qdf_nbuf_t htt_t2h_msg)
{
	return true;
}
#endif

/**
 * dp_monitor_htt_ppdu_stats_attach() - attach resources for HTT PPDU
 * stats processing
 * @pdev: Datapath PDEV handle
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS dp_monitor_htt_ppdu_stats_attach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_SUCCESS;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_htt_ppdu_stats_attach) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_htt_ppdu_stats_attach(pdev);
}

/**
 * dp_monitor_htt_ppdu_stats_detach() - detach stats resources
 * @pdev: Datapath PDEV handle
 *
 * Return: void
 */
static inline void dp_monitor_htt_ppdu_stats_detach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_htt_ppdu_stats_detach) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_htt_ppdu_stats_detach(pdev);
}

/**
 * dp_monitor_print_pdev_rx_mon_stats() - print rx mon stats
 * @pdev: Datapath PDEV handle
 *
 * Return: void
 */
static inline void dp_monitor_print_pdev_rx_mon_stats(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_print_pdev_rx_mon_stats) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_print_pdev_rx_mon_stats(pdev);
}

#ifdef WIFI_MONITOR_SUPPORT
/**
 * dp_monitor_print_pdev_tx_capture_stats() - print tx capture stats
 * @pdev: Datapath PDEV handle
 *
 * Return: void
 */
static inline void dp_monitor_print_pdev_tx_capture_stats(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_print_pdev_tx_capture_stats) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_print_pdev_tx_capture_stats(pdev);
}

/**
 * dp_monitor_config_enh_tx_capture() - configure tx capture
 * @pdev: Datapath PDEV handle
 * @val: mode
 *
 * Return: status
 */
static inline QDF_STATUS dp_monitor_config_enh_tx_capture(struct dp_pdev *pdev,
							  uint32_t val)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_config_enh_tx_capture) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_config_enh_tx_capture(pdev, val);
}

/**
 * dp_monitor_tx_peer_filter() -  add tx monitor peer filter
 * @pdev: Datapath PDEV handle
 * @peer: Datapath PEER handle
 * @is_tx_pkt_cap_enable: flag for tx capture enable/disable
 * @peer_mac: peer mac address
 *
 * Return: status
 */
static inline QDF_STATUS dp_monitor_tx_peer_filter(struct dp_pdev *pdev,
						   struct dp_peer *peer,
						   uint8_t is_tx_pkt_cap_enable,
						   uint8_t *peer_mac)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		qdf_err("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_tx_peer_filter) {
		qdf_err("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_tx_peer_filter(pdev, peer, is_tx_pkt_cap_enable,
					       peer_mac);
}
#endif

#ifdef WLAN_RX_PKT_CAPTURE_ENH
static inline QDF_STATUS dp_monitor_config_enh_rx_capture(struct dp_pdev *pdev,
							  uint32_t val)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_config_enh_rx_capture) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_config_enh_rx_capture(pdev, val);
}
#else
static inline QDF_STATUS dp_monitor_config_enh_rx_capture(struct dp_pdev *pdev,
							  uint32_t val)
{
	return QDF_STATUS_E_INVAL;
}
#endif

#ifdef QCA_SUPPORT_BPR
static inline QDF_STATUS dp_monitor_set_bpr_enable(struct dp_pdev *pdev,
						   uint32_t val)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_set_bpr_enable) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_set_bpr_enable(pdev, val);
}
#else
static inline QDF_STATUS dp_monitor_set_bpr_enable(struct dp_pdev *pdev,
						   uint32_t val)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef ATH_SUPPORT_NAC
static inline
int dp_monitor_set_filter_neigh_peers(struct dp_pdev *pdev, bool val)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_set_filter_neigh_peers) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return monitor_ops->mon_set_filter_neigh_peers(pdev, val);
}
#else
static inline
int dp_monitor_set_filter_neigh_peers(struct dp_pdev *pdev, bool val)
{
	return 0;
}
#endif

#ifdef WLAN_ATF_ENABLE
static inline
void dp_monitor_set_atf_stats_enable(struct dp_pdev *pdev, bool value)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_set_atf_stats_enable) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_set_atf_stats_enable(pdev, value);
}
#else
static inline
void dp_monitor_set_atf_stats_enable(struct dp_pdev *pdev, bool value)
{
}
#endif

static inline
void dp_monitor_set_bsscolor(struct dp_pdev *pdev, uint8_t bsscolor)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_set_bsscolor) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_set_bsscolor(pdev, bsscolor);
}

static inline
bool dp_monitor_pdev_get_filter_mcast_data(struct cdp_pdev *pdev_handle)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_get_filter_mcast_data) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_pdev_get_filter_mcast_data(pdev_handle);
}

static inline
bool dp_monitor_pdev_get_filter_non_data(struct cdp_pdev *pdev_handle)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_get_filter_non_data) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_pdev_get_filter_non_data(pdev_handle);
}

static inline
bool dp_monitor_pdev_get_filter_ucast_data(struct cdp_pdev *pdev_handle)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pdev_get_filter_ucast_data) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_pdev_get_filter_ucast_data(pdev_handle);
}

#ifdef WDI_EVENT_ENABLE
static inline
int dp_monitor_set_pktlog_wifi3(struct dp_pdev *pdev, uint32_t event,
				bool enable)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return 0;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_set_pktlog_wifi3) {
		dp_mon_debug("callback not registered");
		return 0;
	}

	return monitor_ops->mon_set_pktlog_wifi3(pdev, event, enable);
}
#else
static inline
int dp_monitor_set_pktlog_wifi3(struct dp_pdev *pdev, uint32_t event,
				bool enable)
{
	return 0;
}
#endif

#if defined(DP_CON_MON) && !defined(REMOVE_PKT_LOG)
static inline void dp_monitor_pktlogmod_exit(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_pktlogmod_exit) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_pktlogmod_exit(pdev);
}
#else
static inline void dp_monitor_pktlogmod_exit(struct dp_pdev *pdev) {}
#endif

static inline
QDF_STATUS dp_monitor_vdev_set_monitor_mode_buf_rings(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_set_monitor_mode_buf_rings) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_vdev_set_monitor_mode_buf_rings(pdev);
}

static inline
void dp_monitor_neighbour_peers_detach(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_neighbour_peers_detach) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_neighbour_peers_detach(pdev);
}

#ifdef FEATURE_NAC_RSSI
static inline QDF_STATUS dp_monitor_filter_neighbour_peer(struct dp_pdev *pdev,
							  uint8_t *rx_pkt_hdr)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_filter_neighbour_peer) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_filter_neighbour_peer(pdev, rx_pkt_hdr);
}
#else
static inline QDF_STATUS dp_monitor_filter_neighbour_peer(struct dp_pdev *pdev,
							  uint8_t *rx_pkt_hdr)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

static inline
void dp_monitor_reap_timer_init(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_reap_timer_init) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_reap_timer_init(soc);
}

static inline
void dp_monitor_reap_timer_deinit(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_reap_timer_deinit) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_reap_timer_deinit(soc);
}

/**
 * dp_monitor_reap_timer_start() - start reap timer of monitor status ring
 * @soc: point to soc
 * @source: trigger source
 *
 * Return: true if timer-start is performed, false otherwise.
 */
static inline bool
dp_monitor_reap_timer_start(struct dp_soc *soc,
			    enum cdp_mon_reap_source source)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_reap_timer_start) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_reap_timer_start(soc, source);
}

/**
 * dp_monitor_reap_timer_stop() - stop reap timer of monitor status ring
 * @soc: point to soc
 * @source: trigger source
 *
 * Return: true if timer-stop is performed, false otherwise.
 */
static inline bool
dp_monitor_reap_timer_stop(struct dp_soc *soc,
			   enum cdp_mon_reap_source source)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_reap_timer_stop) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_reap_timer_stop(soc, source);
}

static inline
void dp_monitor_vdev_timer_init(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_timer_init) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_vdev_timer_init(soc);
}

static inline
void dp_monitor_vdev_timer_deinit(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_timer_deinit) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_vdev_timer_deinit(soc);
}

static inline
void dp_monitor_vdev_timer_start(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_timer_start) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_vdev_timer_start(soc);
}

static inline
bool dp_monitor_vdev_timer_stop(struct dp_soc *soc)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_vdev_timer_stop) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_vdev_timer_stop(soc);
}

#ifdef QCA_MCOPY_SUPPORT
static inline
QDF_STATUS dp_monitor_mcopy_check_deliver(struct dp_pdev *pdev,
					  uint16_t peer_id, uint32_t ppdu_id,
					  uint8_t first_msdu)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_mcopy_check_deliver) {
		dp_mon_debug("callback not registered");
		return QDF_STATUS_E_FAILURE;
	}

	return monitor_ops->mon_mcopy_check_deliver(pdev, peer_id,
						    ppdu_id, first_msdu);
}
#else
static inline
QDF_STATUS dp_monitor_mcopy_check_deliver(struct dp_pdev *pdev,
					  uint16_t peer_id, uint32_t ppdu_id,
					  uint8_t first_msdu)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_monitor_neighbour_peer_add_ast() - Add ast entry
 * @pdev: point to dp pdev
 * @ta_peer: point to peer
 * @mac_addr: mac address
 * @nbuf: point to nbuf
 * @flags: flags
 *
 * Return: void
 */
static inline void
dp_monitor_neighbour_peer_add_ast(struct dp_pdev *pdev,
				  struct dp_peer *ta_peer,
				  uint8_t *mac_addr,
				  qdf_nbuf_t nbuf,
				  uint32_t flags)
{
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_neighbour_peer_add_ast) {
		dp_mon_debug("callback not registered");
		return;
	}

	return monitor_ops->mon_neighbour_peer_add_ast(pdev, ta_peer, mac_addr,
						       nbuf, flags);
}

/**
 * dp_monitor_vdev_delete() - delete monitor vdev
 * @soc: point to dp soc
 * @vdev: point to dp vdev
 *
 * Return: void
 */
static inline void dp_monitor_vdev_delete(struct dp_soc *soc,
					  struct dp_vdev *vdev)
{
	if (soc->intr_mode == DP_INTR_POLL) {
		qdf_timer_sync_cancel(&soc->int_timer);
		dp_monitor_flush_rings(soc);
	} else if (soc->intr_mode == DP_INTR_MSI) {
		if (dp_monitor_vdev_timer_stop(soc))
			dp_monitor_flush_rings(soc);
	}

	dp_monitor_vdev_detach(vdev);
}

#ifdef DP_POWER_SAVE
/**
 * dp_monitor_reap_timer_suspend() - Stop monitor reap timer
 * and reap any pending frames in ring
 * @soc: DP soc context
 *
 * Return: void
 */
static inline void
dp_monitor_reap_timer_suspend(struct dp_soc *soc)
{
	if (dp_monitor_reap_timer_stop(soc, CDP_MON_REAP_SOURCE_ANY))
		dp_monitor_service_mon_rings(soc, DP_MON_REAP_BUDGET);
}

#endif

/**
 * dp_monitor_neighbour_peer_list_remove() - remove neighbour peer list
 * @pdev: point to dp pdev
 * @vdev: point to dp vdev
 * @peer: point to dp_neighbour_peer
 *
 * Return: void
 */
static inline
void dp_monitor_neighbour_peer_list_remove(struct dp_pdev *pdev,
					   struct dp_vdev *vdev,
					   struct dp_neighbour_peer *peer)
{
	struct dp_mon_pdev *mon_pdev;
	struct dp_neighbour_peer *temp_peer = NULL;

	if (qdf_unlikely(!pdev || !pdev->monitor_pdev))
		return;

	mon_pdev = pdev->monitor_pdev;
	qdf_spin_lock_bh(&mon_pdev->neighbour_peer_mutex);
	if (!pdev->soc->monitor_soc->hw_nac_monitor_support) {
		TAILQ_FOREACH(peer, &mon_pdev->neighbour_peers_list,
			      neighbour_peer_list_elem) {
				QDF_ASSERT(peer->vdev != vdev);
			}
	} else {
		TAILQ_FOREACH_SAFE(peer, &mon_pdev->neighbour_peers_list,
				   neighbour_peer_list_elem, temp_peer) {
			if (peer->vdev == vdev) {
				TAILQ_REMOVE(&mon_pdev->neighbour_peers_list,
					     peer,
					     neighbour_peer_list_elem);
				qdf_mem_free(peer);
			}
		}
	}
	qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);
}

static inline
void dp_monitor_pdev_set_mon_vdev(struct dp_vdev *vdev)
{
	if (!vdev->pdev->monitor_pdev)
		return;

	vdev->pdev->monitor_pdev->mvdev = vdev;
}

static inline
void dp_monitor_pdev_config_scan_spcl_vap(struct dp_pdev *pdev, bool val)
{
	if (!pdev || !pdev->monitor_pdev)
		return;

	pdev->monitor_pdev->scan_spcl_vap_configured = val;
}

#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
static inline
void dp_monitor_pdev_reset_scan_spcl_vap_stats_enable(struct dp_pdev *pdev,
						      bool val)
{
	if (!pdev || !pdev->monitor_pdev)
		return;

	pdev->monitor_pdev->reset_scan_spcl_vap_stats_enable = val;
}
#else
static inline
void dp_monitor_pdev_reset_scan_spcl_vap_stats_enable(struct dp_pdev *pdev,
						      bool val)
{
}
#endif

static inline void
dp_mon_rx_wmask_subscribe(struct dp_soc *soc, uint32_t *msg_word,
			  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;

	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_wmask_subscribe) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->rx_wmask_subscribe(msg_word, tlv_filter);
}

static inline void
dp_mon_rx_enable_mpdu_logging(struct dp_soc *soc, uint32_t *msg_word,
			      struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;

	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_enable_mpdu_logging) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->rx_enable_mpdu_logging(msg_word, tlv_filter);
}

/**
 * dp_mon_rx_enable_fpmo() - set fpmo filters
 * @soc: dp soc handle
 * @msg_word: msg word
 * @tlv_filter: rx fing filter config
 *
 * Return: void
 */
static inline void
dp_mon_rx_enable_fpmo(struct dp_soc *soc, uint32_t *msg_word,
		      struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_enable_fpmo) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->rx_enable_fpmo(msg_word, tlv_filter);
}

/**
 * dp_mon_rx_hdr_length_set() - set rx hdr tlv length
 * @soc: dp soc handle
 * @msg_word: msg word
 * @tlv_filter: rx fing filter config
 *
 * Return: void
 */
static inline void
dp_mon_rx_hdr_length_set(struct dp_soc *soc, uint32_t *msg_word,
			 struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_hdr_length_set) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->rx_hdr_length_set(msg_word, tlv_filter);
}

static inline void
dp_mon_rx_packet_length_set(struct dp_soc *soc, uint32_t *msg_word,
			    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;

	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_packet_length_set) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->rx_packet_length_set(msg_word, tlv_filter);
}

static inline void
dp_rx_mon_enable(struct dp_soc *soc, uint32_t *msg_word,
		 struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->rx_mon_enable) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->rx_mon_enable(msg_word, tlv_filter);
}

static inline void
dp_mon_rx_mac_filter_set(struct dp_soc *soc, uint32_t *msg_word,
			 struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops || !monitor_ops->mon_mac_filter_set) {
		dp_mon_debug("callback not registered");
		return;
	}

	monitor_ops->mon_mac_filter_set(msg_word, tlv_filter);
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
QDF_STATUS dp_peer_qos_stats_notify(struct dp_pdev *dp_pdev,
				    struct cdp_rx_stats_ppdu_user *ppdu_user);
#endif

/**
 * dp_print_pdev_rx_mon_stats() - print rx mon stats
 * @pdev: device object
 *
 * Return: void
 */
void
dp_print_pdev_rx_mon_stats(struct dp_pdev *pdev);

/**
 * dp_mcopy_check_deliver() - mcopy check deliver
 * @pdev: DP pdev handle
 * @peer_id: peer id
 * @ppdu_id: ppdu
 * @first_msdu: flag to indicate first msdu of ppdu
 * Return: 0 on success, not 0 on failure
 */
QDF_STATUS dp_mcopy_check_deliver(struct dp_pdev *pdev,
				  uint16_t peer_id,
				  uint32_t ppdu_id,
				  uint8_t first_msdu);

/**
 * dp_pdev_set_advance_monitor_filter() - Set DP PDEV monitor filter
 * @soc_hdl: soc handle
 * @pdev_id: id of Datapath PDEV handle
 * @filter_val: Flag to select Filter for monitor mode
 *
 * Return: 0 on success, not 0 on failure
 */
#ifdef QCA_ADVANCE_MON_FILTER_SUPPORT
QDF_STATUS
dp_pdev_set_advance_monitor_filter(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				   struct cdp_monitor_filter *filter_val);
#else
static inline QDF_STATUS
dp_pdev_set_advance_monitor_filter(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				   struct cdp_monitor_filter *filter_val)
{
	return QDF_STATUS_E_INVAL;
}
#endif /* QCA_ADVANCE_MON_FILTER_SUPPORT */

/**
 * dp_deliver_tx_mgmt() - Deliver mgmt frame for tx capture
 * @cdp_soc : data path soc handle
 * @pdev_id : pdev_id
 * @nbuf: Management frame buffer
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS
dp_deliver_tx_mgmt(struct cdp_soc_t *cdp_soc, uint8_t pdev_id, qdf_nbuf_t nbuf);

/**
 * dp_filter_neighbour_peer() - API to filter neighbour peer
 * @pdev : DP pdev handle
 * @rx_pkt_hdr : packet header
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAILURE on failure
 */
#ifdef FEATURE_NAC_RSSI
QDF_STATUS dp_filter_neighbour_peer(struct dp_pdev *pdev,
				    uint8_t *rx_pkt_hdr);
#else
static inline
QDF_STATUS dp_filter_neighbour_peer(struct dp_pdev *pdev,
				    uint8_t *rx_pkt_hdr)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_NAC_RSSI */

/**
 * dp_neighbour_peers_detach() - Detach neighbour peers(nac clients)
 * @pdev: device object
 *
 * Return: void
 */
void dp_neighbour_peers_detach(struct dp_pdev *pdev);

/**
 * dp_reset_monitor_mode() - Disable monitor mode
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of datapath PDEV handle
 * @smart_monitor: smart monitor flag
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_reset_monitor_mode(struct cdp_soc_t *soc_hdl,
				 uint8_t pdev_id,
				 uint8_t smart_monitor);

static inline
struct dp_mon_ops *dp_mon_ops_get(struct dp_soc *soc)
{
	if (soc && soc->monitor_soc)
		return soc->monitor_soc->mon_ops;

	return NULL;
}

static inline
struct cdp_mon_ops *dp_mon_cdp_ops_get(struct dp_soc *soc)
{
	struct cdp_ops *ops = soc->cdp_soc.ops;

	return ops->mon_ops;
}

/**
 * dp_monitor_soc_init() - Monitor SOC init
 * @soc: DP soc handle
 *
 * Return: void
 */
static inline void dp_monitor_soc_init(struct dp_soc *soc)
{
	struct dp_mon_ops *mon_ops;

	mon_ops = dp_mon_ops_get(soc);

	if (mon_ops && mon_ops->mon_soc_init)
		mon_ops->mon_soc_init(soc);
}

/**
 * dp_monitor_soc_deinit() - Monitor SOC deinit
 * @soc: DP soc handle
 *
 * Return: void
 */
static inline void dp_monitor_soc_deinit(struct dp_soc *soc)
{
	struct dp_mon_ops *mon_ops;

	mon_ops = dp_mon_ops_get(soc);

	if (mon_ops && mon_ops->mon_soc_deinit)
		mon_ops->mon_soc_deinit(soc);
}

/**
 * dp_ppdu_desc_user_stats_update(): Function to update TX user stats
 * @pdev: DP pdev handle
 * @ppdu_info: per PPDU TLV descriptor
 *
 * Return: void
 */
#ifdef QCA_ENHANCED_STATS_SUPPORT
void
dp_ppdu_desc_user_stats_update(struct dp_pdev *pdev,
			       struct ppdu_info *ppdu_info);
#else
static inline void
dp_ppdu_desc_user_stats_update(struct dp_pdev *pdev,
			       struct ppdu_info *ppdu_info)
{
}
#endif /* QCA_ENHANCED_STATS_SUPPORT */

/**
 * dp_mon_ops_register_1_0(): register legacy monitor ops
 * @mon_soc: monitor soc handle
 *
 * return: void
 */
void dp_mon_ops_register_1_0(struct dp_mon_soc *mon_soc);

/**
 * dp_mon_cdp_ops_register_1_0(): register legacy monitor cdp ops
 * @ops: cdp ops handle
 *
 * return: void
 */
void dp_mon_cdp_ops_register_1_0(struct cdp_ops *ops);

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
/**
 * dp_cfr_filter_register_1_0(): register cfr filter setting API
 * @ops: cdp ops handle
 *
 * return: void
 */
void dp_cfr_filter_register_1_0(struct cdp_ops *ops);
#endif

#ifdef QCA_MONITOR_2_0_SUPPORT
/**
 * dp_mon_ops_register_2_0(): register monitor ops
 * @mon_soc: monitor soc handle
 *
 * return: void
 */
void dp_mon_ops_register_2_0(struct dp_mon_soc *mon_soc);

/**
 * dp_mon_cdp_ops_register_2_0(): register monitor cdp ops
 * @ops: cdp ops handle
 *
 * return: void
 */
void dp_mon_cdp_ops_register_2_0(struct cdp_ops *ops);

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
/**
 * dp_cfr_filter_register_2_0(): register cfr filter setting API
 * @ops: cdp ops handle
 *
 * return: void
 */
void dp_cfr_filter_register_2_0(struct cdp_ops *ops);
#endif
#endif /* QCA_MONITOR_2_0_SUPPORT */

/**
 * dp_mon_register_feature_ops(): Register mon feature ops
 * @soc: Datapath soc context
 *
 * return: void
 */
static inline
void dp_mon_register_feature_ops(struct dp_soc *soc)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);
	if (!mon_ops) {
		dp_mon_err("Monitor ops is NULL");
		return;
	}
	if (mon_ops->mon_register_feature_ops)
		mon_ops->mon_register_feature_ops(soc);
}

/**
 * dp_pdev_get_rx_mon_stats(): Get pdev Rx monitor stats
 * @soc_hdl: soc handle
 * @pdev_id: id of pdev handle
 * @stats: User allocated stats buffer
 *
 * return: status success/failure
 */
QDF_STATUS dp_pdev_get_rx_mon_stats(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				    struct cdp_pdev_mon_stats *stats);

/**
 * dp_enable_mon_reap_timer() - enable/disable reap timer
 * @soc_hdl: Datapath soc handle
 * @source: trigger source of the timer
 * @enable: Enable/Disable reap timer of monitor status ring
 *
 * Return: true if a timer-start/stop is performed, false otherwise.
 */
bool dp_enable_mon_reap_timer(struct cdp_soc_t *soc_hdl,
			      enum cdp_mon_reap_source source, bool enable);

/**
 * dp_monitor_lite_mon_disable_rx() - disables rx lite mon
 * @pdev: dp pdev
 *
 * Return: void
 */
static inline void
dp_monitor_lite_mon_disable_rx(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return;
}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops ||
	    !monitor_ops->mon_lite_mon_disable_rx) {
		dp_mon_debug("callback not registered");
		return;
}

	return monitor_ops->mon_lite_mon_disable_rx(pdev);
}

/*
 * dp_monitor_lite_mon_is_rx_adv_filter_enable()
 *   - check if advance mon filter is already applied
 * @pdev: dp pdev
 *
 * Return: bool
 */
static inline bool
dp_monitor_lite_mon_is_rx_adv_filter_enable(struct dp_pdev *pdev)
{
	struct dp_mon_ops *monitor_ops;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;

	if (!mon_soc) {
		dp_mon_debug("monitor soc is NULL");
		return false;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops ||
	    !monitor_ops->mon_lite_mon_disable_rx) {
		dp_mon_debug("callback not registered");
		return false;
	}

	return monitor_ops->mon_lite_mon_is_rx_adv_filter_enable(pdev);
}

#ifndef QCA_SUPPORT_LITE_MONITOR
static inline void
dp_lite_mon_disable_rx(struct dp_pdev *pdev)
{
}

static inline void
dp_lite_mon_disable_tx(struct dp_pdev *pdev)
{
}

static inline bool
dp_lite_mon_is_rx_adv_filter_enable(struct dp_pdev *pdev)
{
	return false;
}

static inline bool
dp_lite_mon_get_filter_ucast_data(struct cdp_pdev *pdev_handle)
{
	return false;
}

static inline bool
dp_lite_mon_get_filter_mcast_data(struct cdp_pdev *pdev_handle)
{
	return false;
}

static inline bool
dp_lite_mon_get_filter_non_data(struct cdp_pdev *pdev_handle)
{
	return false;
}

static inline int
dp_lite_mon_is_level_msdu(struct dp_mon_pdev *mon_pdev)
{
	return 0;
}

static inline int
dp_lite_mon_is_rx_enabled(struct dp_mon_pdev *mon_pdev)
{
	return 0;
}

static inline int
dp_lite_mon_is_tx_enabled(struct dp_mon_pdev *mon_pdev)
{
	return 0;
}

static inline QDF_STATUS
dp_lite_mon_alloc(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_lite_mon_dealloc(struct dp_pdev *pdev)
{
}

static inline void
dp_lite_mon_vdev_delete(struct dp_pdev *pdev, struct dp_vdev *vdev)
{
}

static inline int
dp_lite_mon_config_nac_peer(struct cdp_soc_t *soc_hdl,
			    uint8_t vdev_id,
			    uint32_t cmd, uint8_t *macaddr)
{
	return 0;
}

static inline QDF_STATUS
dp_lite_mon_config_nac_rssi_peer(struct cdp_soc_t *soc_hdl,
				 uint8_t vdev_id,
				 enum cdp_nac_param_cmd cmd,
				 char *bssid, char *macaddr,
				 uint8_t chan_num)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
dp_lite_mon_get_nac_peer_rssi(struct cdp_soc_t *soc_hdl,
			      uint8_t vdev_id, char *macaddr,
			      uint8_t *rssi)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
dp_lite_mon_rx_mpdu_process(struct dp_pdev *pdev,
			    struct hal_rx_ppdu_info *ppdu_info,
			    qdf_nbuf_t mon_mpdu, uint16_t mpdu_id,
			    uint8_t user)
{
	return QDF_STATUS_E_FAILURE;
}

static inline int
dp_lite_mon_get_legacy_feature_enabled(struct cdp_soc_t *soc,
				       uint8_t pdev_id,
				       uint8_t direction)
{
	return 0;
}
#endif

#ifdef WLAN_TELEMETRY_STATS_SUPPORT
static inline
void dp_monitor_peer_telemetry_stats(struct dp_peer *peer,
				     struct cdp_peer_telemetry_stats *stats)
{
	struct dp_mon_peer_stats *mon_peer_stats = NULL;
	uint8_t ac;

	if (qdf_unlikely(!peer->monitor_peer))
		return;

	mon_peer_stats = &peer->monitor_peer->stats;
	for (ac = 0; ac < WME_AC_MAX; ac++) {
		/* consumption is in micro seconds, convert it to seconds and
		 * then calculate %age per sec
		 */
		stats->airtime_consumption[ac] =
			((mon_peer_stats->airtime_consumption[ac].avg_consumption_per_sec * 100) /
			(1000000));
	}
	stats->tx_mpdu_retried = mon_peer_stats->tx.retries;
	stats->tx_mpdu_total = mon_peer_stats->tx.tx_mpdus_tried;
	stats->rx_mpdu_retried = mon_peer_stats->rx.mpdu_retry_cnt;
	stats->rx_mpdu_total = mon_peer_stats->rx.rx_mpdus;
	stats->snr = CDP_SNR_OUT(mon_peer_stats->rx.avg_snr);
}
#endif

/**
 * dp_monitor_is_tx_cap_enabled() - get tx-cature enabled/disabled
 * @peer: DP peer handle
 *
 * Return: true if tx-capture is enabled
 */
static inline bool dp_monitor_is_tx_cap_enabled(struct dp_peer *peer)
{
	return peer->monitor_peer ? peer->monitor_peer->tx_cap_enabled : 0;
}

/**
 * dp_monitor_is_rx_cap_enabled() - get rx-cature enabled/disabled
 * @peer: DP peer handle
 *
 * Return: true if rx-capture is enabled
 */
static inline bool dp_monitor_is_rx_cap_enabled(struct dp_peer *peer)
{
	return peer->monitor_peer ? peer->monitor_peer->rx_cap_enabled : 0;
}

#if !(!defined(DISABLE_MON_CONFIG) && defined(QCA_MONITOR_2_0_SUPPORT))
/**
 * dp_mon_get_context_size_be() - get BE specific size for mon pdev/soc
 * @context_type: context type for which the size is needed
 *
 * Return: size in bytes for the context_type
 */
static inline
qdf_size_t dp_mon_get_context_size_be(enum dp_context_type context_type)
{
	switch (context_type) {
	case DP_CONTEXT_TYPE_MON_SOC:
		return sizeof(struct dp_mon_soc);
	case DP_CONTEXT_TYPE_MON_PDEV:
		return sizeof(struct dp_mon_pdev);
	default:
		return 0;
	}
}
#endif

/**
 * dp_mon_rx_print_advanced_stats() - print advanced monitor stats
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 *
 * Return: void
 */
static inline void
dp_mon_rx_print_advanced_stats(struct dp_soc *soc,
			       struct dp_pdev *pdev)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *monitor_ops;

	if (!mon_soc) {
		dp_mon_debug("mon soc is NULL");
		return;
	}

	monitor_ops = mon_soc->mon_ops;
	if (!monitor_ops ||
	    !monitor_ops->mon_rx_print_advanced_stats) {
		dp_mon_debug("callback not registered");
		return;
	}
	return monitor_ops->mon_rx_print_advanced_stats(soc, pdev);
}
#endif /* _DP_MON_H_ */

/*
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
  * DOC: wlan_dp_main.c
  *
  *
  */

#include "wlan_dp_main.h"
#include "wlan_dp_public_struct.h"
#include "cfg_ucfg_api.h"
#include "wlan_dp_bus_bandwidth.h"
#include <wlan_objmgr_psoc_obj_i.h>
#include <wlan_nlink_common.h>
#include <qdf_net_types.h>
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_cm_api.h"
#include "wlan_dp_nud_tracking.h"
#include "target_if_dp_comp.h"
#include "wlan_dp_txrx.h"
#include "init_deinit_lmac.h"
#include <hif.h>
#include <htc_api.h>
#if defined(WLAN_DP_PROFILE_SUPPORT) || defined(FEATURE_DIRECT_LINK)
#include "cdp_txrx_ctrl.h"
#endif
#ifdef FEATURE_DIRECT_LINK
#include "dp_internal.h"
#endif
#include "wlan_dp_svc.h"

#ifdef WLAN_DP_PROFILE_SUPPORT
/* Memory profile table based on supported caps */
static struct wlan_dp_memory_profile_ctx wlan_dp_1x1_he80_1kqam[] = {
	{DP_TX_DESC_NUM_CFG, 1024},
	{DP_TX_EXT_DESC_NUM_CFG, 1024},
	{DP_TX_RING_SIZE_CFG, 1024},
	{DP_TX_COMPL_RING_SIZE_CFG, 1024},
	{DP_RX_SW_DESC_NUM_CFG, 1024},
	{DP_REO_DST_RING_SIZE_CFG, 1024},
	{DP_RXDMA_BUF_RING_SIZE_CFG, 1024},
	{DP_RXDMA_REFILL_RING_SIZE_CFG, 1024},
	{DP_RX_REFILL_POOL_NUM_CFG, 1024},
};

/* Global data structure to save profile info */
static struct wlan_dp_memory_profile_info g_dp_profile_info;
#endif

/* Global DP context */
static struct wlan_dp_psoc_context *gp_dp_ctx;

QDF_STATUS dp_allocate_ctx(void)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = qdf_mem_malloc(sizeof(*dp_ctx));
	if (!dp_ctx) {
		dp_err("Failed to create DP context");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_spinlock_create(&dp_ctx->intf_list_lock);
	qdf_list_create(&dp_ctx->intf_list, 0);

	dp_attach_ctx(dp_ctx);

	return QDF_STATUS_SUCCESS;
}

void dp_free_ctx(void)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx =  dp_get_context();

	qdf_spinlock_destroy(&dp_ctx->intf_list_lock);
	qdf_list_destroy(&dp_ctx->intf_list);
	dp_detach_ctx();
	qdf_mem_free(dp_ctx);
}

QDF_STATUS dp_get_front_intf_no_lock(struct wlan_dp_psoc_context *dp_ctx,
				     struct wlan_dp_intf **out_intf)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	*out_intf = NULL;

	status = qdf_list_peek_front(&dp_ctx->intf_list, &node);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_intf = qdf_container_of(node, struct wlan_dp_intf, node);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_get_next_intf_no_lock(struct wlan_dp_psoc_context *dp_ctx,
				    struct wlan_dp_intf *cur_intf,
				    struct wlan_dp_intf **out_intf)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	if (!cur_intf)
		return QDF_STATUS_E_INVAL;

	*out_intf = NULL;

	status = qdf_list_peek_next(&dp_ctx->intf_list,
				    &cur_intf->node,
				    &node);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_intf = qdf_container_of(node, struct wlan_dp_intf, node);

	return status;
}

struct wlan_dp_intf*
dp_get_intf_by_macaddr(struct wlan_dp_psoc_context *dp_ctx,
		       struct qdf_mac_addr *addr)
{
	struct wlan_dp_intf *dp_intf;

	qdf_spin_lock_bh(&dp_ctx->intf_list_lock);
	for (dp_get_front_intf_no_lock(dp_ctx, &dp_intf); dp_intf;
	     dp_get_next_intf_no_lock(dp_ctx, dp_intf, &dp_intf)) {
		if (qdf_is_macaddr_equal(&dp_intf->mac_addr, addr)) {
			qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);
			return dp_intf;
		}
	}
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);

	return NULL;
}

struct wlan_dp_intf*
dp_get_intf_by_netdev(struct wlan_dp_psoc_context *dp_ctx, qdf_netdev_t dev)
{
	struct wlan_dp_intf *dp_intf;

	qdf_spin_lock_bh(&dp_ctx->intf_list_lock);
	for (dp_get_front_intf_no_lock(dp_ctx, &dp_intf); dp_intf;
	     dp_get_next_intf_no_lock(dp_ctx, dp_intf, &dp_intf)) {
		if (dp_intf->dev == dev) {
			qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);
			return dp_intf;
		}
	}
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock);

	return NULL;
}

/**
 * validate_interface_id() - Check if interface ID is valid
 * @intf_id: interface ID
 *
 * Return: 0 on success, error code on failure
 */
static int validate_interface_id(uint8_t intf_id)
{
	if (intf_id == WLAN_UMAC_VDEV_ID_MAX) {
		dp_err("Interface is not up: %ps", QDF_RET_IP);
		return -EINVAL;
	}
	if (intf_id >= WLAN_MAX_VDEVS) {
		dp_err("Bad interface id:%u", intf_id);
		return -EINVAL;
	}
	return 0;
}

int is_dp_intf_valid(struct wlan_dp_intf *dp_intf)
{
	if (!dp_intf) {
		dp_err("Interface is NULL");
		return -EINVAL;
	}

	if (!dp_intf->dev) {
		dp_err("DP interface net_device is null");
		return -EINVAL;
	}

	if (!(dp_intf->dev->flags & IFF_UP)) {
		dp_info_rl("DP interface '%s' is not up %ps",
			   dp_intf->dev->name, QDF_RET_IP);
		return -EAGAIN;
	}

	return validate_interface_id(dp_intf->intf_id);
}

static QDF_STATUS
dp_intf_wait_for_task_complete(struct wlan_dp_intf *dp_intf)
{
	int count = DP_TASK_MAX_WAIT_CNT;
	int r;

	while (count) {
		r = atomic_read(&dp_intf->num_active_task);

		if (!r)
			return QDF_STATUS_SUCCESS;

		if (--count) {
			dp_err_rl("Waiting for DP task to complete: %d", count);
			qdf_sleep(DP_TASK_WAIT_TIME);
		}
	}

	dp_err("Timed-out waiting for DP task completion");
	return QDF_STATUS_E_TIMEOUT;
}

void dp_wait_complete_tasks(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_intf *dp_intf, *dp_intf_next = NULL;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		/*
		 * If timeout happens for one interface better to bail out
		 * instead of waiting for other intefaces task completion
		 */
		if (qdf_atomic_read(&dp_intf->num_active_task))
			if (dp_intf_wait_for_task_complete(dp_intf))
				break;
	}
}

#ifdef CONFIG_DP_TRACE
/**
 * dp_convert_string_to_array() - used to convert string into u8 array
 * @str: String to be converted
 * @array: Array where converted value is stored
 * @len: Length of the populated array
 * @array_max_len: Maximum length of the array
 * @to_hex: true, if conversion required for hex string
 *
 * This API is called to convert string (each byte separated by
 * a comma) into an u8 array
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_convert_string_to_array(char *str, uint8_t *array,
					     uint8_t *len,
					     uint16_t array_max_len,
					     bool to_hex)
{
	char *format, *s = str;

	if (!str || !array || !len)
		return QDF_STATUS_E_INVAL;

	format = (to_hex) ? "%02x" : "%d";

	*len = 0;
	while ((s) && (*len < array_max_len)) {
		int val;
		/* Increment length only if sscanf successfully extracted
		 * one element. Any other return value means error.
		 * Ignore it.
		 */
		if (sscanf(s, format, &val) == 1) {
			array[*len] = (uint8_t)val;
			*len += 1;
		}

		s = strpbrk(s, ",");
		if (s)
			s++;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_string_to_u8_array() - used to convert string into u8 array
 * @str: String to be converted
 * @array: Array where converted value is stored
 * @len: Length of the populated array
 * @array_max_len: Maximum length of the array
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS dp_string_to_u8_array(char *str, uint8_t *array,
				 uint8_t *len, uint16_t array_max_len)
{
	return dp_convert_string_to_array(str, array, len,
					  array_max_len, false);
}

void dp_trace_init(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_dp_psoc_cfg *config;
	bool live_mode = DP_TRACE_CONFIG_DEFAULT_LIVE_MODE;
	uint8_t thresh = DP_TRACE_CONFIG_DEFAULT_THRESH;
	uint16_t thresh_time_limit = DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT;
	uint8_t verbosity = DP_TRACE_CONFIG_DEFAULT_VERBOSTY;
	uint32_t proto_bitmap = DP_TRACE_CONFIG_DEFAULT_BITMAP;
	uint8_t config_params[DP_TRACE_CONFIG_NUM_PARAMS];
	uint8_t num_entries = 0;
	uint32_t bw_compute_interval;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	config = &dp_ctx->dp_cfg;

	qdf_dp_set_proto_event_bitmap(config->dp_proto_event_bitmap);

	if (!config->enable_dp_trace) {
		dp_err("dp trace is disabled from ini");
		return;
	}

	dp_string_to_u8_array(config->dp_trace_config, config_params,
			      &num_entries, sizeof(config_params));

	/* calculating, num bw timer intervals in a second (1000ms) */
	bw_compute_interval = DP_BUS_BW_CFG(config->bus_bw_compute_interval);

	if (bw_compute_interval <= 1000 && bw_compute_interval > 0) {
		thresh_time_limit = 1000 / bw_compute_interval;
	} else if (bw_compute_interval > 1000) {
		dp_err("busBandwidthComputeInterval > 1000, using 1000");
		thresh_time_limit = 1;
	} else {
		dp_err("busBandwidthComputeInterval is 0, using defaults");
	}

	switch (num_entries) {
	case 4:
		proto_bitmap = config_params[3];
		fallthrough;
	case 3:
		verbosity = config_params[2];
		fallthrough;
	case 2:
		thresh = config_params[1];
		fallthrough;
	case 1:
		live_mode = config_params[0];
		fallthrough;
	default:
		dp_debug("live_mode %u thresh %u time_limit %u verbosity %u bitmap 0x%x",
			 live_mode, thresh, thresh_time_limit,
			 verbosity, proto_bitmap);
	};

	qdf_dp_trace_init(live_mode, thresh, thresh_time_limit,
			  verbosity, proto_bitmap);
}

void dp_set_dump_dp_trace(uint16_t cmd_type, uint16_t count)
{
	dp_debug("DUMP_DP_TRACE_LEVEL: %d %d",
		 cmd_type, count);
	if (cmd_type == DUMP_DP_TRACE)
		qdf_dp_trace_dump_all(count, QDF_TRACE_DEFAULT_PDEV_ID);
	else if (cmd_type == ENABLE_DP_TRACE_LIVE_MODE)
		qdf_dp_trace_enable_live_mode();
	else if (cmd_type == CLEAR_DP_TRACE_BUFFER)
		qdf_dp_trace_clear_buffer();
	else if (cmd_type == DISABLE_DP_TRACE_LIVE_MODE)
		qdf_dp_trace_disable_live_mode();
}
#else
void dp_trace_init(struct wlan_objmgr_psoc *psoc)
{
}

void dp_set_dump_dp_trace(uint16_t cmd_type, uint16_t count)
{
}
#endif
#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * dp_ini_bus_bandwidth() - Initialize INIs concerned about bus bandwidth
 * @config: pointer to dp config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void dp_ini_bus_bandwidth(struct wlan_dp_psoc_cfg *config,
				 struct wlan_objmgr_psoc *psoc)
{
	config->bus_bw_super_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_SUPER_HIGH_THRESHOLD);
	config->bus_bw_ultra_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_ULTRA_HIGH_THRESHOLD);
	config->bus_bw_very_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_VERY_HIGH_THRESHOLD);
	config->bus_bw_dbs_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_DBS_THRESHOLD);
	config->bus_bw_mid_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_MID_HIGH_THRESHOLD);
	config->bus_bw_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_HIGH_THRESHOLD);
	config->bus_bw_medium_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_MEDIUM_THRESHOLD);
	config->bus_bw_low_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_LOW_THRESHOLD);
	config->bus_bw_compute_interval =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_COMPUTE_INTERVAL);
	config->bus_low_cnt_threshold =
		cfg_get(psoc, CFG_DP_BUS_LOW_BW_CNT_THRESHOLD);
	config->enable_latency_crit_clients =
		cfg_get(psoc, CFG_DP_BUS_HANDLE_LATENCY_CRITICAL_CLIENTS);
}

/**
 * dp_ini_tcp_settings() - Initialize INIs concerned about tcp settings
 * @config: pointer to dp config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void dp_ini_tcp_settings(struct wlan_dp_psoc_cfg *config,
				struct wlan_objmgr_psoc *psoc)
{
	config->enable_tcp_limit_output =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_LIMIT_OUTPUT);
	config->enable_tcp_adv_win_scale =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_ADV_WIN_SCALE);
	config->enable_tcp_delack =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_DELACK);
	config->tcp_delack_thres_high =
		cfg_get(psoc, CFG_DP_TCP_DELACK_THRESHOLD_HIGH);
	config->tcp_delack_thres_low =
		cfg_get(psoc, CFG_DP_TCP_DELACK_THRESHOLD_LOW);
	config->tcp_delack_timer_count =
		cfg_get(psoc, CFG_DP_TCP_DELACK_TIMER_COUNT);
	config->tcp_tx_high_tput_thres =
		cfg_get(psoc, CFG_DP_TCP_TX_HIGH_TPUT_THRESHOLD);
	config->enable_tcp_param_update =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_PARAM_UPDATE);
}

#else
static void dp_ini_bus_bandwidth(struct wlan_dp_psoc_cfg *config,
				 struct wlan_objmgr_psoc *psoc)
{
}

static void dp_ini_tcp_settings(struct wlan_dp_psoc_cfg *config,
				struct wlan_objmgr_psoc *psoc)
{
}
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

#ifdef CONFIG_DP_TRACE
/**
 * dp_trace_cfg_update() - initialize DP Trace config
 * @config : Configuration parameters
 * @psoc: psoc handle
 */
static void
dp_trace_cfg_update(struct wlan_dp_psoc_cfg *config,
		    struct wlan_objmgr_psoc *psoc)
{
	qdf_size_t array_out_size;

	config->enable_dp_trace = cfg_get(psoc, CFG_DP_ENABLE_DP_TRACE);
	qdf_uint8_array_parse(cfg_get(psoc, CFG_DP_DP_TRACE_CONFIG),
			      config->dp_trace_config,
			      sizeof(config->dp_trace_config), &array_out_size);
	config->dp_proto_event_bitmap = cfg_get(psoc,
						CFG_DP_PROTO_EVENT_BITMAP);
}
#else
static void
dp_trace_cfg_update(struct wlan_dp_psoc_cfg *config,
		    struct wlan_objmgr_psoc *psoc)
{
}
#endif
#ifdef WLAN_NUD_TRACKING
/**
 * dp_nud_tracking_cfg_update() - initialize NUD Tracking config
 * @config : Configuration parameters
 * @psoc: psoc handle
 */
static void
dp_nud_tracking_cfg_update(struct wlan_dp_psoc_cfg *config,
			   struct wlan_objmgr_psoc *psoc)
{
	config->enable_nud_tracking = cfg_get(psoc, CFG_DP_ENABLE_NUD_TRACKING);
}
#else
static void
dp_nud_tracking_cfg_update(struct wlan_dp_psoc_cfg *config,
			   struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
/**
 * dp_ini_tcp_del_ack_settings() - initialize TCP delack config
 * @config : Configuration parameters
 * @psoc: psoc handle
 */
static void dp_ini_tcp_del_ack_settings(struct wlan_dp_psoc_cfg *config,
					struct wlan_objmgr_psoc *psoc)
{
	config->del_ack_threshold_high =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_HIGH_THRESHOLD);
	config->del_ack_threshold_low =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_LOW_THRESHOLD);
	config->del_ack_enable =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_ENABLE);
	config->del_ack_pkt_count =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_PKT_CNT);
	config->del_ack_timer_value =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_TIMER_VALUE);
}
#else
static void dp_ini_tcp_del_ack_settings(struct wlan_dp_psoc_cfg *config,
					struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
/**
 * dp_hl_bundle_cfg_update() - initialize TxRx HL bundle config
 * @config : Configuration parameters
 * @psoc: psoc handle
 */
static void dp_hl_bundle_cfg_update(struct wlan_dp_psoc_cfg *config,
				    struct wlan_objmgr_psoc *psoc)
{
	config->pkt_bundle_threshold_high =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_HIGH_TH);
	config->pkt_bundle_threshold_low =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_LOW_TH);
	config->pkt_bundle_timer_value =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_TIMER_VALUE);
	config->pkt_bundle_size =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_SIZE);
}
#else
static void dp_hl_bundle_cfg_update(struct wlan_dp_psoc_cfg *config,
				    struct wlan_objmgr_psoc *psoc)
{
}
#endif

/**
 * dp_set_rx_mode_value() - set rx_mode values
 * @dp_ctx: DP context
 *
 * Return: none
 */
static void dp_set_rx_mode_value(struct wlan_dp_psoc_context *dp_ctx)
{
	uint32_t rx_mode = dp_ctx->dp_cfg.rx_mode;
	enum QDF_GLOBAL_MODE con_mode = 0;

	con_mode = cds_get_conparam();

	/* RPS has higher priority than dynamic RPS when both bits are set */
	if (rx_mode & CFG_ENABLE_RPS && rx_mode & CFG_ENABLE_DYNAMIC_RPS)
		rx_mode &= ~CFG_ENABLE_DYNAMIC_RPS;

	if (rx_mode & CFG_ENABLE_RX_THREAD && rx_mode & CFG_ENABLE_RPS) {
		dp_warn("rx_mode wrong configuration. Make it default");
		rx_mode = CFG_RX_MODE_DEFAULT;
	}

	if (rx_mode & CFG_ENABLE_RX_THREAD) {
		dp_ctx->enable_rxthread = true;
	} else if (rx_mode & CFG_ENABLE_DP_RX_THREADS) {
		if (con_mode == QDF_GLOBAL_MONITOR_MODE)
			dp_ctx->enable_dp_rx_threads = false;
		else
			dp_ctx->enable_dp_rx_threads = true;
	}

	if (rx_mode & CFG_ENABLE_RPS)
		dp_ctx->rps = true;

	if (rx_mode & CFG_ENABLE_NAPI)
		dp_ctx->napi_enable = true;

	if (rx_mode & CFG_ENABLE_DYNAMIC_RPS)
		dp_ctx->dynamic_rps = true;

	dp_info("rx_mode:%u dp_rx_threads:%u rx_thread:%u napi:%u rps:%u dynamic rps %u",
		rx_mode, dp_ctx->enable_dp_rx_threads,
		dp_ctx->enable_rxthread, dp_ctx->napi_enable,
		dp_ctx->rps, dp_ctx->dynamic_rps);
}

/**
 * dp_cfg_init() - initialize target specific configuration
 * @ctx: dp context handle
 */
static void dp_cfg_init(struct wlan_dp_psoc_context *ctx)
{
	struct wlan_dp_psoc_cfg *config = &ctx->dp_cfg;
	struct wlan_objmgr_psoc *psoc = ctx->psoc;
	uint16_t cfg_len;

	cfg_len = qdf_str_len(cfg_get(psoc, CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST))
		  + 1;
	dp_ini_bus_bandwidth(config, psoc);
	dp_ini_tcp_settings(config, psoc);

	dp_ini_tcp_del_ack_settings(config, psoc);

	dp_hl_bundle_cfg_update(config, psoc);

	config->rx_thread_ul_affinity_mask =
		cfg_get(psoc, CFG_DP_RX_THREAD_UL_CPU_MASK);
	config->rx_thread_affinity_mask =
		cfg_get(psoc, CFG_DP_RX_THREAD_CPU_MASK);
	config->fisa_enable = cfg_get(psoc, CFG_DP_RX_FISA_ENABLE);
	if (cfg_len < CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN) {
		qdf_str_lcopy(config->cpu_map_list,
			      cfg_get(psoc, CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST),
			      cfg_len);
	} else {
		dp_err("ini string length greater than max size %d",
		       CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN);
		cfg_len = qdf_str_len(cfg_default(CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST));
		qdf_str_lcopy(config->cpu_map_list,
			      cfg_default(CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST),
			      cfg_len);
	}
	config->tx_orphan_enable = cfg_get(psoc, CFG_DP_TX_ORPHAN_ENABLE);
	config->rx_mode = cfg_get(psoc, CFG_DP_RX_MODE);
	dp_set_rx_mode_value(ctx);
	config->multicast_replay_filter =
		cfg_get(psoc, CFG_DP_FILTER_MULTICAST_REPLAY);
	config->rx_wakelock_timeout =
		cfg_get(psoc, CFG_DP_RX_WAKELOCK_TIMEOUT);
	config->num_dp_rx_threads = cfg_get(psoc, CFG_DP_NUM_DP_RX_THREADS);
	config->icmp_req_to_fw_mark_interval =
		cfg_get(psoc, CFG_DP_ICMP_REQ_TO_FW_MARK_INTERVAL);

	config->rx_softirq_max_yield_duration_ns =
		cfg_get(psoc,
			CFG_DP_RX_SOFTIRQ_MAX_YIELD_TIME_NS);

	dp_trace_cfg_update(config, psoc);
	dp_nud_tracking_cfg_update(config, psoc);
	dp_trace_cfg_update(config, psoc);
}

/**
 * __dp_process_mic_error() - Indicate mic error to supplicant
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
static void
__dp_process_mic_error(struct wlan_dp_intf *dp_intf)
{
	struct wlan_dp_psoc_callbacks *ops = &dp_intf->dp_ctx->dp_ops;
	struct wlan_objmgr_vdev *vdev;

	vdev = dp_objmgr_get_vdev_by_user(dp_intf, WLAN_DP_ID);
	if (!vdev) {
		return;
	}

	if ((dp_intf->device_mode == QDF_STA_MODE ||
	     dp_intf->device_mode == QDF_P2P_CLIENT_MODE) &&
	    wlan_cm_is_vdev_active(vdev))
		ops->osif_dp_process_mic_error(dp_intf->mic_work.info,
						   vdev);
	else if (dp_intf->device_mode == QDF_SAP_MODE ||
		 dp_intf->device_mode == QDF_P2P_GO_MODE)
		ops->osif_dp_process_mic_error(dp_intf->mic_work.info,
						   vdev);
	else
		dp_err("Invalid interface type:%d", dp_intf->device_mode);

	dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
}

/**
 * dp_process_mic_error() - process mic error work
 * @data: void pointer to dp interface
 *
 * Return: None
 */
static void
dp_process_mic_error(void *data)
{
	struct wlan_dp_intf *dp_intf = data;

	if (is_dp_intf_valid(dp_intf))
		goto exit;

	__dp_process_mic_error(dp_intf);

exit:
	qdf_spin_lock_bh(&dp_intf->mic_work.lock);
	if (dp_intf->mic_work.info) {
		qdf_mem_free(dp_intf->mic_work.info);
		dp_intf->mic_work.info = NULL;
	}
	if (dp_intf->mic_work.status == DP_MIC_SCHEDULED)
		dp_intf->mic_work.status = DP_MIC_INITIALIZED;
	qdf_spin_unlock_bh(&dp_intf->mic_work.lock);
}

void
dp_rx_mic_error_ind(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
		    struct cdp_rx_mic_err_info *mic_failure_info)
{
	struct dp_mic_error_info *dp_mic_info;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_dp_intf *dp_intf;

	if (!psoc)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc((struct wlan_objmgr_psoc *)psoc,
						    mic_failure_info->vdev_id,
						    WLAN_DP_ID);
	if (!vdev)
		return;
	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (!dp_intf) {
		dp_comp_vdev_put_ref(vdev);
		return;
	}

	dp_mic_info = qdf_mem_malloc(sizeof(*dp_mic_info));
	if (!dp_mic_info) {
		dp_comp_vdev_put_ref(vdev);
		return;
	}

	qdf_copy_macaddr(&dp_mic_info->ta_mac_addr,
			 &mic_failure_info->ta_mac_addr);
	dp_mic_info->multicast = mic_failure_info->multicast;
	dp_mic_info->key_id = mic_failure_info->key_id;
	qdf_mem_copy(&dp_mic_info->tsc, &mic_failure_info->tsc,
		     SIR_CIPHER_SEQ_CTR_SIZE);
	dp_mic_info->vdev_id = mic_failure_info->vdev_id;

	qdf_spin_lock_bh(&dp_intf->mic_work.lock);
	if (dp_intf->mic_work.status != DP_MIC_INITIALIZED) {
		qdf_spin_unlock_bh(&dp_intf->mic_work.lock);
		qdf_mem_free(dp_mic_info);
		dp_comp_vdev_put_ref(vdev);
		return;
	}
	/*
	 * Store mic error info pointer in dp_intf
	 * for freeing up the allocated memory in case
	 * the work scheduled below is flushed or deinitialized.
	 */
	dp_intf->mic_work.status = DP_MIC_SCHEDULED;
	dp_intf->mic_work.info = dp_mic_info;
	qdf_sched_work(0, &dp_intf->mic_work.work);
	qdf_spin_unlock_bh(&dp_intf->mic_work.lock);
	dp_comp_vdev_put_ref(vdev);
}

/**
 * dp_mic_flush_work() - disable and flush pending mic work
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
static void
dp_mic_flush_work(struct wlan_dp_intf *dp_intf)
{
	dp_info("Flush the MIC error work");

	qdf_spin_lock_bh(&dp_intf->mic_work.lock);
	if (dp_intf->mic_work.status != DP_MIC_SCHEDULED) {
		qdf_spin_unlock_bh(&dp_intf->mic_work.lock);
		return;
	}
	dp_intf->mic_work.status = DP_MIC_DISABLED;
	qdf_spin_unlock_bh(&dp_intf->mic_work.lock);

	qdf_flush_work(&dp_intf->mic_work.work);
}

/**
 * dp_mic_enable_work() - enable mic error work
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
static void dp_mic_enable_work(struct wlan_dp_intf *dp_intf)
{
	dp_info("Enable the MIC error work");

	qdf_spin_lock_bh(&dp_intf->mic_work.lock);
	if (dp_intf->mic_work.status == DP_MIC_DISABLED)
		dp_intf->mic_work.status = DP_MIC_INITIALIZED;
	qdf_spin_unlock_bh(&dp_intf->mic_work.lock);
}

void dp_mic_deinit_work(struct wlan_dp_intf *dp_intf)
{
	dp_info("DeInitialize the MIC error work");

	if (dp_intf->mic_work.status != DP_MIC_UNINITIALIZED) {
		qdf_destroy_work(NULL, &dp_intf->mic_work.work);

		qdf_spin_lock_bh(&dp_intf->mic_work.lock);
		dp_intf->mic_work.status = DP_MIC_UNINITIALIZED;
		if (dp_intf->mic_work.info) {
			qdf_mem_free(dp_intf->mic_work.info);
			dp_intf->mic_work.info = NULL;
		}
		qdf_spin_unlock_bh(&dp_intf->mic_work.lock);
		qdf_spinlock_destroy(&dp_intf->mic_work.lock);
	}
}

void dp_mic_init_work(struct wlan_dp_intf *dp_intf)
{
	qdf_spinlock_create(&dp_intf->mic_work.lock);
	qdf_create_work(0, &dp_intf->mic_work.work,
			dp_process_mic_error, dp_intf);
	dp_intf->mic_work.status = DP_MIC_INITIALIZED;
	dp_intf->mic_work.info = NULL;
}

QDF_STATUS
dp_peer_obj_create_notification(struct wlan_objmgr_peer *peer, void *arg)
{
	struct wlan_dp_sta_info *sta_info;
	QDF_STATUS status;

	sta_info = qdf_mem_malloc(sizeof(*sta_info));
	if (!sta_info)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_peer_component_obj_attach(peer, WLAN_COMP_DP,
						       sta_info,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("DP peer attach failed");
		qdf_mem_free(sta_info);
		return status;
	}

	qdf_mem_copy(sta_info->sta_mac.bytes, peer->macaddr,
		     QDF_MAC_ADDR_SIZE);
	sta_info->pending_eap_frm_type = 0;
	sta_info->dhcp_phase = DHCP_PHASE_ACK;
	sta_info->dhcp_nego_status = DHCP_NEGO_STOP;

	dp_info("sta info created mac:" QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));

	return status;
}

QDF_STATUS
dp_peer_obj_destroy_notification(struct wlan_objmgr_peer *peer, void *arg)
{
	struct wlan_dp_sta_info *sta_info;
	QDF_STATUS status;

	sta_info = dp_get_peer_priv_obj(peer);
	if (!sta_info) {
		dp_err("DP_peer_obj is NULL");
		return QDF_STATUS_E_FAULT;
	}

	status = wlan_objmgr_peer_component_obj_detach(peer, WLAN_COMP_DP,
						       sta_info);
	if (QDF_IS_STATUS_ERROR(status))
		dp_err("DP peer detach failed");

	qdf_mem_free(sta_info);

	return status;
}

QDF_STATUS
dp_vdev_obj_create_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_dp_intf *dp_intf;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr *mac_addr;
	qdf_netdev_t dev;

	dp_info("DP VDEV OBJ create notification");

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		dp_err("Failed to get psoc");
		return QDF_STATUS_E_INVAL;
	}

	dp_ctx =  dp_psoc_get_priv(psoc);
	mac_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_macaddr(vdev);

	dev = dp_ctx->dp_ops.dp_get_netdev_by_vdev_mac(mac_addr);
	if (!dev) {
		dp_err("Failed to get intf mac:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(mac_addr->bytes));
		return QDF_STATUS_E_INVAL;
	}

	dp_intf = dp_get_intf_by_netdev(dp_ctx, dev);
	if (!dp_intf) {
		dp_err("Failed to get dp intf dev: %s",
		       qdf_netdev_get_devname(dev));

		return QDF_STATUS_E_INVAL;
	}

	dp_intf->device_mode = wlan_vdev_mlme_get_opmode(vdev);
	qdf_spin_lock_bh(&dp_intf->vdev_lock);
	dp_intf->intf_id = vdev->vdev_objmgr.vdev_id;
	dp_intf->vdev = vdev;
	qdf_spin_unlock_bh(&dp_intf->vdev_lock);
	qdf_atomic_init(&dp_intf->num_active_task);

	if (dp_intf->device_mode == QDF_SAP_MODE ||
	    dp_intf->device_mode == QDF_P2P_GO_MODE) {
		dp_intf->sap_tx_block_mask = DP_TX_FN_CLR | DP_TX_SAP_STOP;

		status = qdf_event_create(&dp_intf->qdf_sta_eap_frm_done_event);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			dp_err("eap frm done event init failed!!");
			return status;
		}
	}

	status = wlan_objmgr_vdev_component_obj_attach(vdev,
						       WLAN_COMP_DP,
						       (void *)dp_intf,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to attach dp_intf with vdev");
		return status;
	}

	dp_nud_ignore_tracking(dp_intf, false);
	dp_mic_enable_work(dp_intf);
	dp_flow_priortization_init(dp_intf);

	return status;
}

QDF_STATUS
dp_vdev_obj_destroy_notification(struct wlan_objmgr_vdev *vdev, void *arg)

{
	struct wlan_dp_intf *dp_intf;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	dp_info("DP VDEV OBJ destroy notification");

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (!dp_intf) {
		dp_err("Failed to get DP interface obj");
		return QDF_STATUS_E_INVAL;
	}

	dp_flow_priortization_deinit(dp_intf);
	dp_nud_ignore_tracking(dp_intf, true);
	dp_nud_reset_tracking(dp_intf);
	dp_nud_flush_work(dp_intf);
	dp_mic_flush_work(dp_intf);

	status = dp_intf_wait_for_task_complete(dp_intf);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (dp_intf->device_mode == QDF_SAP_MODE ||
	    dp_intf->device_mode == QDF_P2P_GO_MODE) {
		status = qdf_event_destroy(&dp_intf->qdf_sta_eap_frm_done_event);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			dp_err("eap frm done event destroy failed!!");
			return status;
		}
	}
	qdf_mem_zero(&dp_intf->conn_info, sizeof(struct wlan_dp_conn_info));
	dp_intf->intf_id = WLAN_UMAC_VDEV_ID_MAX;
	qdf_spin_lock_bh(&dp_intf->vdev_lock);
	dp_intf->vdev = NULL;
	qdf_spin_unlock_bh(&dp_intf->vdev_lock);
	status = wlan_objmgr_vdev_component_obj_detach(vdev,
						       WLAN_COMP_DP,
						       (void *)dp_intf);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to detach dp_intf with vdev");
		return status;
	}

	return status;
}

QDF_STATUS
dp_pdev_obj_create_notification(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_dp_psoc_context *dp_ctx;
	QDF_STATUS status;

	dp_info("DP PDEV OBJ create notification");
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		obj_mgr_err("psoc is NULL in pdev");
		return QDF_STATUS_E_FAILURE;
	}
	dp_ctx =  dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("Failed to get dp_ctx from psoc");
		return QDF_STATUS_E_FAILURE;
	}
	status = wlan_objmgr_pdev_component_obj_attach(pdev,
						       WLAN_COMP_DP,
						       (void *)dp_ctx,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to attach dp_ctx to pdev");
		return status;
	}

	dp_ctx->pdev = pdev;
	return status;
}

QDF_STATUS
dp_pdev_obj_destroy_notification(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_dp_psoc_context *dp_ctx;
	QDF_STATUS status;

	dp_info("DP PDEV OBJ destroy notification");
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		obj_mgr_err("psoc is NULL in pdev");
		return QDF_STATUS_E_FAILURE;
	}

	dp_ctx = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_COMP_DP);
	if (!dp_ctx) {
		dp_err("Failed to get dp_ctx from pdev");
		return QDF_STATUS_E_FAILURE;
	}
	status = wlan_objmgr_pdev_component_obj_detach(pdev,
						       WLAN_COMP_DP,
						       dp_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to detach dp_ctx from pdev");
		return status;
	}
	if (!dp_ctx->pdev)
		dp_err("DP Pdev is NULL");

	dp_ctx->pdev = NULL;
	return status;
}

QDF_STATUS
dp_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_dp_psoc_context *dp_ctx = gp_dp_ctx;
	QDF_STATUS status;

	status = wlan_objmgr_psoc_component_obj_attach(
				psoc, WLAN_COMP_DP,
				dp_ctx, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to attach psoc component obj");
		return status;
	}

	dp_ctx->psoc = psoc;
	dp_cfg_init(dp_ctx);
	target_if_dp_register_tx_ops(&dp_ctx->sb_ops);
	target_if_dp_register_rx_ops(&dp_ctx->nb_ops);

	return status;
}

QDF_STATUS
dp_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_dp_psoc_context *dp_ctx;
	QDF_STATUS status;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("psoc priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(
					psoc, WLAN_COMP_DP,
					dp_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to detach psoc component obj");
		return status;
	}

	dp_reset_all_intfs_connectivity_stats(dp_ctx);

	return status;
}

void dp_attach_ctx(struct wlan_dp_psoc_context *dp_ctx)
{
	if (gp_dp_ctx)
		dp_debug("already attached global dp ctx");
	gp_dp_ctx = dp_ctx;
}

void dp_detach_ctx(void)
{
	if (!gp_dp_ctx) {
		dp_err("global dp ctx is already detached");
		return;
	}
	gp_dp_ctx = NULL;
}

struct wlan_dp_psoc_context *dp_get_context(void)
{
	return gp_dp_ctx;
}

/**
 * dp_hex_string_to_u16_array() - convert a hex string to a uint16 array
 * @str: input string
 * @int_array: pointer to input array of type uint16
 * @len: pointer to number of elements which the function adds to the array
 * @int_array_max_len: maximum number of elements in input uint16 array
 *
 * This function is used to convert a space separated hex string to an array of
 * uint16_t. For example, an input string str = "a b c d" would be converted to
 * a unint16 array, int_array = {0xa, 0xb, 0xc, 0xd}, *len = 4.
 * This assumes that input value int_array_max_len >= 4.
 *
 * Return: QDF_STATUS_SUCCESS - if the conversion is successful
 *         non zero value     - if the conversion is a failure
 */
static QDF_STATUS
dp_hex_string_to_u16_array(char *str, uint16_t *int_array, uint8_t *len,
			   uint8_t int_array_max_len)
{
	char *s = str;
	uint32_t val = 0;

	if (!str || !int_array || !len)
		return QDF_STATUS_E_INVAL;

	dp_debug("str %pK intArray %pK intArrayMaxLen %d",
		 s, int_array, int_array_max_len);

	*len = 0;

	while ((s) && (*len < int_array_max_len)) {
		/*
		 * Increment length only if sscanf successfully extracted one
		 * element. Any other return value means error. Ignore it.
		 */
		if (sscanf(s, "%x", &val) == 1) {
			int_array[*len] = (uint16_t)val;
			dp_debug("s %pK val %x intArray[%d]=0x%x",
				 s, val, *len, int_array[*len]);
			*len += 1;
		}
		s = strpbrk(s, " ");
		if (s)
			s++;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_get_interface() - to get dp interface matching the mode
 * @dp_ctx: dp context
 * @mode: interface mode
 *
 * This routine will return the pointer to dp interface matching
 * with the passed mode.
 *
 * Return: pointer to interface or null
 */
static struct
wlan_dp_intf *dp_get_interface(struct wlan_dp_psoc_context *dp_ctx,
			       enum QDF_OPMODE mode)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_intf *dp_intf_next;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		if (!dp_intf)
			continue;

		if (dp_intf->device_mode == mode)
			return dp_intf;
	}

	return NULL;
}

void dp_send_rps_ind(struct wlan_dp_intf *dp_intf)
{
	int i;
	uint8_t cpu_map_list_len = 0;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct wlan_rps_data rps_data;
	struct cds_config_info *cds_cfg;

	cds_cfg = cds_get_ini_config();
	if (!cds_cfg) {
		dp_err("cds_cfg is NULL");
		return;
	}

	rps_data.num_queues = NUM_RX_QUEUES;

	dp_info("cpu_map_list '%s'", dp_ctx->dp_cfg.cpu_map_list);

	/* in case no cpu map list is provided, simply return */
	if (!strlen(dp_ctx->dp_cfg.cpu_map_list)) {
		dp_info("no cpu map list found");
		goto err;
	}

	if (QDF_STATUS_SUCCESS !=
		dp_hex_string_to_u16_array(dp_ctx->dp_cfg.cpu_map_list,
					   rps_data.cpu_map_list,
					   &cpu_map_list_len,
					   WLAN_SVC_IFACE_NUM_QUEUES)) {
		dp_err("invalid cpu map list");
		goto err;
	}

	rps_data.num_queues =
		(cpu_map_list_len < rps_data.num_queues) ?
				cpu_map_list_len : rps_data.num_queues;

	for (i = 0; i < rps_data.num_queues; i++) {
		dp_info("cpu_map_list[%d] = 0x%x",
			i, rps_data.cpu_map_list[i]);
	}

	strlcpy(rps_data.ifname, qdf_netdev_get_devname(dp_intf->dev),
		sizeof(rps_data.ifname));
	dp_ctx->dp_ops.dp_send_svc_nlink_msg(cds_get_radio_index(),
					     WLAN_SVC_RPS_ENABLE_IND,
					     &rps_data, sizeof(rps_data));

	cds_cfg->rps_enabled = true;

	return;

err:
	dp_info("Wrong RPS configuration. enabling rx_thread");
	cds_cfg->rps_enabled = false;
}

void dp_try_send_rps_ind(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_dp_intf *dp_intf = dp_get_vdev_priv_obj(vdev);

	if (!dp_intf) {
		dp_err("dp interface is NULL");
		return;
	}
	if (dp_intf->dp_ctx->rps)
		dp_send_rps_ind(dp_intf);
}

void dp_send_rps_disable_ind(struct wlan_dp_intf *dp_intf)
{
	struct wlan_rps_data rps_data;
	struct cds_config_info *cds_cfg;

	cds_cfg = cds_get_ini_config();

	if (!cds_cfg) {
		dp_err("cds_cfg is NULL");
		return;
	}

	rps_data.num_queues = NUM_RX_QUEUES;

	dp_info("Set cpu_map_list 0");

	qdf_mem_zero(&rps_data.cpu_map_list, sizeof(rps_data.cpu_map_list));

	strlcpy(rps_data.ifname, qdf_netdev_get_devname(dp_intf->dev),
		sizeof(rps_data.ifname));
	dp_intf->dp_ctx->dp_ops.dp_send_svc_nlink_msg(cds_get_radio_index(),
				    WLAN_SVC_RPS_ENABLE_IND,
				    &rps_data, sizeof(rps_data));

	cds_cfg->rps_enabled = false;
}

#ifdef QCA_CONFIG_RPS
void dp_set_rps(uint8_t vdev_id, bool enable)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_dp_intf *dp_intf;

	dp_ctx = dp_get_context();
	if (!dp_ctx)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(dp_ctx->psoc,
						    vdev_id, WLAN_DP_ID);
	if (!vdev)
		return;

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (!dp_intf) {
		dp_comp_vdev_put_ref(vdev);
		dp_err_rl("DP interface not found for vdev_id: %d", vdev_id);
		return;
	}

	dp_info("Set RPS to %d for vdev_id %d", enable, vdev_id);
	if (!dp_ctx->rps) {
		if (enable)
			dp_send_rps_ind(dp_intf);
		else
			dp_send_rps_disable_ind(dp_intf);
	}
	dp_comp_vdev_put_ref(vdev);
}
#endif

void dp_set_rx_mode_rps(bool enable)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct wlan_dp_intf *dp_intf;
	struct cds_config_info *cds_cfg;

	dp_ctx = dp_get_context();
	cds_cfg = cds_get_ini_config();
	if (!dp_ctx || !cds_cfg)
		return;

	dp_intf = dp_get_interface(dp_ctx, QDF_SAP_MODE);
	if (!dp_intf)
		return;

	if (!dp_intf->dp_ctx->rps && cds_cfg->uc_offload_enabled) {
		if (enable && !cds_cfg->rps_enabled)
			dp_send_rps_ind(dp_intf);
		else if (!enable && cds_cfg->rps_enabled)
			dp_send_rps_disable_ind(dp_intf);
	}
}

void dp_set_rps_cpu_mask(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_intf *dp_intf_next;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		if (!dp_intf)
			continue;

		dp_send_rps_ind(dp_intf);
	}
}

void dp_try_set_rps_cpu_mask(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("dp context is NULL");
		return;
	}

	if (dp_ctx->dynamic_rps)
		dp_set_rps_cpu_mask(dp_ctx);
}

void dp_clear_rps_cpu_mask(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_intf *dp_intf_next;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		if (!dp_intf)
			continue;

		dp_send_rps_disable_ind(dp_intf);
	}
}

QDF_STATUS dp_get_arp_stats_event_handler(struct wlan_objmgr_psoc *psoc,
					  struct dp_rsp_stats *rsp)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    rsp->vdev_id,
						    WLAN_DP_ID);
	if (!vdev) {
		dp_err("Can't get vdev by vdev_id:%d", rsp->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	dp_intf = dp_get_vdev_priv_obj(vdev);
	if (!dp_intf) {
		dp_err("Unable to get DP interface");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID);
		return QDF_STATUS_E_INVAL;
	}

	dp_info("rsp->arp_req_enqueue :%x", rsp->arp_req_enqueue);
	dp_info("rsp->arp_req_tx_success :%x", rsp->arp_req_tx_success);
	dp_info("rsp->arp_req_tx_failure :%x", rsp->arp_req_tx_failure);
	dp_info("rsp->arp_rsp_recvd :%x", rsp->arp_rsp_recvd);
	dp_info("rsp->out_of_order_arp_rsp_drop_cnt :%x",
		rsp->out_of_order_arp_rsp_drop_cnt);
	dp_info("rsp->dad_detected :%x", rsp->dad_detected);
	dp_info("rsp->connect_status :%x", rsp->connect_status);
	dp_info("rsp->ba_session_establishment_status :%x",
		rsp->ba_session_establishment_status);

	dp_intf->dp_stats.arp_stats.rx_fw_cnt = rsp->arp_rsp_recvd;
	dp_intf->dad |= rsp->dad_detected;
	dp_intf->con_status = rsp->connect_status;

	/* Flag true indicates connectivity check stats present. */
	if (rsp->connect_stats_present) {
		dp_info("rsp->tcp_ack_recvd :%x", rsp->tcp_ack_recvd);
		dp_info("rsp->icmpv4_rsp_recvd :%x", rsp->icmpv4_rsp_recvd);
		dp_intf->dp_stats.tcp_stats.rx_fw_cnt = rsp->tcp_ack_recvd;
		dp_intf->dp_stats.icmpv4_stats.rx_fw_cnt =
							rsp->icmpv4_rsp_recvd;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_DP_ID);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_OBJMGR_REF_ID_TRACE
struct wlan_objmgr_vdev *
__dp_objmgr_get_vdev_by_user(struct wlan_dp_intf *dp_intf,
			     wlan_objmgr_ref_dbgid id,
			     const char *func, int line)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	if (!dp_intf) {
		dp_err("dp_intf is NULL (via %s, id %d)", func, id);
		return NULL;
	}

	qdf_spin_lock_bh(&dp_intf->vdev_lock);
	vdev = dp_intf->vdev;
	if (vdev) {
		status = wlan_objmgr_vdev_try_get_ref_debug(vdev, id, func,
							    line);
		if (QDF_IS_STATUS_ERROR(status))
			vdev = NULL;
	}
	qdf_spin_unlock_bh(&dp_intf->vdev_lock);

	if (!vdev)
		dp_debug("VDEV is NULL (via %s, id %d)", func, id);

	return vdev;
}

void
__dp_objmgr_put_vdev_by_user(struct wlan_objmgr_vdev *vdev,
			     wlan_objmgr_ref_dbgid id, const char *func,
			     int line)
{
	if (!vdev) {
		dp_err("VDEV is NULL (via %s, id %d)", func, id);
		return;
	}

	wlan_objmgr_vdev_release_ref_debug(vdev, id, func, line);
}
#else
struct wlan_objmgr_vdev *
__dp_objmgr_get_vdev_by_user(struct wlan_dp_intf *dp_intf,
			     wlan_objmgr_ref_dbgid id,
			     const char *func)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	if (!dp_intf) {
		dp_err("dp_intf is NULL (via %s, id %d)", func, id);
		return NULL;
	}

	qdf_spin_lock_bh(&dp_intf->vdev_lock);
	vdev = dp_intf->vdev;
	if (vdev) {
		status = wlan_objmgr_vdev_try_get_ref(vdev, id);
		if (QDF_IS_STATUS_ERROR(status))
			vdev = NULL;
	}
	qdf_spin_unlock_bh(&dp_intf->vdev_lock);

	if (!vdev)
		dp_debug("VDEV is NULL (via %s, id %d)", func, id);

	return vdev;
}

void
__dp_objmgr_put_vdev_by_user(struct wlan_objmgr_vdev *vdev,
			     wlan_objmgr_ref_dbgid id, const char *func)
{
	if (!vdev) {
		dp_err("VDEV is NULL (via %s, id %d)", func, id);
		return;
	}

	wlan_objmgr_vdev_release_ref(vdev, id);
}
#endif /* WLAN_OBJMGR_REF_ID_TRACE */

bool dp_is_data_stall_event_enabled(uint32_t evt)
{
	uint32_t bitmap = cdp_cfg_get(cds_get_context(QDF_MODULE_ID_SOC),
				      cfg_dp_enable_data_stall);

	if (bitmap & DP_DATA_STALL_ENABLE || bitmap & evt)
		return true;

	return false;
}

#ifdef FEATURE_DIRECT_LINK
/**
 * dp_lpass_h2t_tx_complete() - Copy completion handler for LPASS data
 * message service
 * @ctx: DP Direct Link context
 * @pkt: htc packet
 *
 * Return: None
 */
static void dp_lpass_h2t_tx_complete(void *ctx, HTC_PACKET *pkt)
{
	dp_info("Unexpected lpass tx complete trigger");
	qdf_assert(0);
}

/**
 * dp_lpass_t2h_msg_handler() - target to host message handler for LPASS data
 * message service
 * @ctx: DP Direct Link context
 * @pkt: htc packet
 *
 * Return: None
 */
static void dp_lpass_t2h_msg_handler(void *ctx, HTC_PACKET *pkt)
{
	dp_info("Unexpected receive msg trigger for lpass service");
	qdf_assert(0);
}

/**
 * dp_lpass_connect_htc_service() - Connect lpass data message htc service
 * @dp_direct_link_ctx: DP Direct Link context
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_lpass_connect_htc_service(struct dp_direct_link_context *dp_direct_link_ctx)
{
	struct htc_service_connect_req connect = {0};
	struct htc_service_connect_resp response = {0};
	HTC_HANDLE htc_handle = cds_get_context(QDF_MODULE_ID_HTC);
	QDF_STATUS status;

	if (!htc_handle)
		return QDF_STATUS_E_FAILURE;

	connect.EpCallbacks.pContext = dp_direct_link_ctx;
	connect.EpCallbacks.EpTxComplete = dp_lpass_h2t_tx_complete;
	connect.EpCallbacks.EpRecv = dp_lpass_t2h_msg_handler;

	/* disable flow control for LPASS data message service */
	connect.ConnectionFlags |= HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
	connect.service_id = LPASS_DATA_MSG_SVC;

	status = htc_connect_service(htc_handle, &connect, &response);

	if (status != QDF_STATUS_SUCCESS) {
		dp_err("LPASS_DATA_MSG connect service failed");
		return status;
	}

	dp_direct_link_ctx->lpass_ep_id = response.Endpoint;

	dp_err("LPASS_DATA_MSG connect service successful");

	return status;
}

/**
 * dp_direct_link_refill_ring_init() - Initialize refill ring that would be used
 *  for Direct Link DP
 * @direct_link_ctx: DP Direct Link context
 *
 * Return: QDF status
 */
static QDF_STATUS
dp_direct_link_refill_ring_init(struct dp_direct_link_context *direct_link_ctx)
{
	struct cdp_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t pdev_id;

	if (!soc)
		return QDF_STATUS_E_FAILURE;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(direct_link_ctx->dp_ctx->pdev);

	direct_link_ctx->direct_link_refill_ring_hdl =
				dp_setup_direct_link_refill_ring(soc,
								 pdev_id);
	if (!direct_link_ctx->direct_link_refill_ring_hdl) {
		dp_err("Refill ring init for Direct Link failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_direct_link_refill_ring_deinit() - De-initialize refill ring that would be
 *  used for Direct Link DP
 * @dlink_ctx: DP Direct Link context
 *
 * Return: None
 */
static void
dp_direct_link_refill_ring_deinit(struct dp_direct_link_context *dlink_ctx)
{
	struct cdp_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t pdev_id;

	if (!soc)
		return;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(dlink_ctx->dp_ctx->pdev);
	dp_destroy_direct_link_refill_ring(soc, pdev_id);
	dlink_ctx->direct_link_refill_ring_hdl = NULL;
}

QDF_STATUS dp_direct_link_init(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_direct_link_context *dp_direct_link_ctx;
	QDF_STATUS status;

	if (!pld_is_direct_link_supported(dp_ctx->qdf_dev->dev)) {
		dp_info("FW does not support Direct Link");
		return QDF_STATUS_SUCCESS;
	}

	dp_direct_link_ctx = qdf_mem_malloc(sizeof(*dp_direct_link_ctx));
	if (!dp_direct_link_ctx) {
		dp_err("Failed to allocate memory for DP Direct Link context");
		return QDF_STATUS_E_NOMEM;
	}

	dp_direct_link_ctx->dp_ctx = dp_ctx;

	status = dp_lpass_connect_htc_service(dp_direct_link_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to connect to LPASS data msg service");
		qdf_mem_free(dp_direct_link_ctx);
		return status;
	}

	status = dp_direct_link_refill_ring_init(dp_direct_link_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(dp_direct_link_ctx);
		return status;
	}

	status = dp_wfds_init(dp_direct_link_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to initialize QMI for Direct Link");
		dp_direct_link_refill_ring_deinit(dp_ctx->dp_direct_link_ctx);
		qdf_mem_free(dp_direct_link_ctx);
		return status;
	}
	qdf_mutex_create(&dp_ctx->dp_direct_link_lock);

	dp_ctx->dp_direct_link_ctx = dp_direct_link_ctx;

	return status;
}

void dp_direct_link_deinit(struct wlan_dp_psoc_context *dp_ctx, bool is_ssr)
{
	struct wlan_dp_intf *dp_intf;

	if (!pld_is_direct_link_supported(dp_ctx->qdf_dev->dev))
		return;

	if (!dp_ctx->dp_direct_link_ctx)
		return;

	for (dp_get_front_intf_no_lock(dp_ctx, &dp_intf); dp_intf;
	     dp_get_next_intf_no_lock(dp_ctx, dp_intf, &dp_intf)) {
		if (dp_intf->device_mode == QDF_SAP_MODE)
			dp_config_direct_link(dp_intf, false, false);
	}

	dp_wfds_deinit(dp_ctx->dp_direct_link_ctx, is_ssr);
	dp_direct_link_refill_ring_deinit(dp_ctx->dp_direct_link_ctx);
	qdf_mutex_destroy(&dp_ctx->dp_direct_link_lock);
	qdf_mem_free(dp_ctx->dp_direct_link_ctx);
	dp_ctx->dp_direct_link_ctx = NULL;
}

QDF_STATUS dp_config_direct_link(struct wlan_dp_intf *dp_intf,
				 bool config_direct_link,
				 bool enable_low_latency)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct direct_link_info *config = &dp_intf->direct_link_config;
	void *htc_handle;
	bool prev_ll, update_ll, vote_link;
	cdp_config_param_type vdev_param = {0};
	QDF_STATUS status;

	if (!dp_ctx || !dp_ctx->psoc) {
		dp_err("DP Handle is NULL");
		return QDF_STATUS_E_CANCELED;
	}

	if (!dp_ctx->dp_direct_link_ctx) {
		dp_err("Direct link not enabled");
		return QDF_STATUS_SUCCESS;
	}

	htc_handle = lmac_get_htc_hdl(dp_ctx->psoc);
	if (!htc_handle) {
		dp_err("HTC handle is NULL");
		return QDF_STATUS_E_EMPTY;
	}

	qdf_mutex_acquire(&dp_ctx->dp_direct_link_lock);
	prev_ll = config->low_latency;
	update_ll = config_direct_link ? enable_low_latency : prev_ll;
	vote_link = config->config_set ^ config_direct_link;
	config->config_set = config_direct_link;
	config->low_latency = enable_low_latency;
	vdev_param.cdp_vdev_tx_to_fw = config_direct_link;
	status = cdp_txrx_set_vdev_param(wlan_psoc_get_dp_handle(dp_ctx->psoc),
					 dp_intf->intf_id, CDP_VDEV_TX_TO_FW,
					 vdev_param);

	if (config_direct_link) {
		if (vote_link)
			htc_vote_link_up(htc_handle,
					 HTC_LINK_VOTE_DIRECT_LINK_USER_ID);
		if (update_ll)
			hif_prevent_link_low_power_states(
						htc_get_hif_device(htc_handle));
		else if (prev_ll)
			hif_allow_link_low_power_states(
						htc_get_hif_device(htc_handle));
		dp_info("Direct link config set. Low link latency enabled: %d",
			enable_low_latency);
	} else {
		if (vote_link)
			htc_vote_link_down(htc_handle,
					   HTC_LINK_VOTE_DIRECT_LINK_USER_ID);
		if (update_ll)
			hif_allow_link_low_power_states(
						htc_get_hif_device(htc_handle));
		dp_info("Direct link config cleared.");
	}
	qdf_mutex_release(&dp_ctx->dp_direct_link_lock);

	return status;
}
#endif

#ifdef WLAN_DP_PROFILE_SUPPORT
struct wlan_dp_memory_profile_info *
wlan_dp_get_profile_info(void)
{
	return &g_dp_profile_info;
}

QDF_STATUS wlan_dp_select_profile_cfg(struct wlan_objmgr_psoc *psoc)
{
	struct pld_soc_info info = {0};
	struct pld_wlan_hw_cap_info *hw_cap_info;
	qdf_device_t qdf_dev;
	bool apply_profile = false;
	int ret;

	apply_profile = cfg_get(psoc,
				CFG_DP_APPLY_MEM_PROFILE);
	if (!apply_profile)
		return QDF_STATUS_E_NOSUPPORT;

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!qdf_dev)
		return QDF_STATUS_E_FAILURE;

	ret = pld_get_soc_info(qdf_dev->dev, &info);
	if (ret) {
		dp_err("profile selection failed unable to H.W caps reason:%u",
		       qdf_status_from_os_return(ret));
		return qdf_status_from_os_return(ret);
	}

	hw_cap_info = &info.hw_cap_info;
	/* Based on supported H.W caps select required memory profile */
	if (hw_cap_info->nss == PLD_WLAN_HW_CAP_NSS_1x1 &&
	    hw_cap_info->bw == PLD_WLAN_HW_CHANNEL_BW_80MHZ &&
	    hw_cap_info->qam == PLD_WLAN_HW_QAM_1K) {
		g_dp_profile_info.is_selected = true;
		g_dp_profile_info.ctx = wlan_dp_1x1_he80_1kqam;
		g_dp_profile_info.size = QDF_ARRAY_SIZE(wlan_dp_1x1_he80_1kqam);
		dp_info("DP profile selected is 1x1_HE80_1KQAM based");
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
static void
wlan_dp_rx_refill_pool_cfg_sync_profile(struct cdp_soc_t *cdp_soc,
				struct wlan_dp_memory_profile_ctx *profile_ctx)
{
	cdp_config_param_type val;
	QDF_STATUS status;
	int cur_val;

	status = cdp_txrx_get_psoc_param(cdp_soc, CDP_CFG_RX_REFILL_POOL_NUM,
					 &val);
	if (QDF_IS_STATUS_SUCCESS(status) &&
	    val.cdp_rx_refill_buf_pool_size != profile_ctx->size) {
		cur_val = val.cdp_rx_refill_buf_pool_size;
		val.cdp_rx_refill_buf_pool_size = profile_ctx->size;
		if (cdp_txrx_set_psoc_param(cdp_soc,
					    CDP_CFG_RX_REFILL_POOL_NUM, val)) {
			dp_err("unable to sync param type:%u", profile_ctx->param_type);
			return;
		}
		dp_info("current Rx refill pool size:%u synced with profile:%u",
			cur_val, profile_ctx->size);
	}
}
#else
static inline void
wlan_dp_rx_refill_pool_cfg_sync_profile(struct cdp_soc_t *cdp_soc,
				struct wlan_dp_memory_profile_ctx *profile_ctx)
{
}
#endif

void wlan_dp_soc_cfg_sync_profile(struct cdp_soc_t *cdp_soc)
{
	struct wlan_dp_memory_profile_info *profile_info;
	struct wlan_dp_memory_profile_ctx *profile_ctx;
	cdp_config_param_type val = {0};
	QDF_STATUS status;
	int cur_val, i;

	profile_info = wlan_dp_get_profile_info();
	if (!profile_info->is_selected)
		return;

	for (i = 0; i < profile_info->size; i++) {
		profile_ctx = &profile_info->ctx[i];
	       qdf_mem_zero(&val, sizeof(cdp_config_param_type));

		switch (profile_ctx->param_type) {
		case DP_TX_DESC_NUM_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_TX_DESC_NUM, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_tx_desc_num != profile_ctx->size) {
				cur_val = val.cdp_tx_desc_num;
				val.cdp_tx_desc_num = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
						CDP_CFG_TX_DESC_NUM, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current Tx desc num:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_TX_EXT_DESC_NUM_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_TX_EXT_DESC_NUM, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_tx_ext_desc_num != profile_ctx->size) {
				cur_val = val.cdp_tx_ext_desc_num;
				val.cdp_tx_ext_desc_num = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
						CDP_CFG_TX_EXT_DESC_NUM, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current Ext Tx desc num:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_TX_RING_SIZE_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_TX_RING_SIZE, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_tx_ring_size != profile_ctx->size) {
				cur_val = val.cdp_tx_ring_size;
				val.cdp_tx_ring_size = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
						CDP_CFG_TX_RING_SIZE, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current Tx Ring size:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_TX_COMPL_RING_SIZE_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_TX_COMPL_RING_SIZE, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_tx_comp_ring_size != profile_ctx->size) {
				cur_val = val.cdp_tx_comp_ring_size;
				val.cdp_tx_comp_ring_size = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
							    CDP_CFG_TX_COMPL_RING_SIZE, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current Tx Comp Ring size:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_RX_SW_DESC_NUM_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_RX_SW_DESC_NUM, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_rx_sw_desc_num != profile_ctx->size) {
				cur_val = val.cdp_rx_sw_desc_num;
				val.cdp_rx_sw_desc_num = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
							    CDP_CFG_RX_SW_DESC_NUM, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current Rx desc num:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_REO_DST_RING_SIZE_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_REO_DST_RING_SIZE, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_reo_dst_ring_size != profile_ctx->size) {
				cur_val = val.cdp_reo_dst_ring_size;
				val.cdp_reo_dst_ring_size = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
							    CDP_CFG_REO_DST_RING_SIZE, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current Rx Ring size:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_RXDMA_REFILL_RING_SIZE_CFG:
			status = cdp_txrx_get_psoc_param(cdp_soc,
						CDP_CFG_RXDMA_REFILL_RING_SIZE, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_rxdma_refill_ring_size != profile_ctx->size) {
				cur_val = val.cdp_rxdma_refill_ring_size;
				val.cdp_rxdma_refill_ring_size = profile_ctx->size;
				if (cdp_txrx_set_psoc_param(cdp_soc,
							    CDP_CFG_RXDMA_REFILL_RING_SIZE, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					break;
				}
				dp_info("current RXDMA refill ring size:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			break;
		case DP_RX_REFILL_POOL_NUM_CFG:
			wlan_dp_rx_refill_pool_cfg_sync_profile(cdp_soc,
								profile_ctx);
			break;
		default:
			dp_debug("Unknown profile param type:%u", profile_ctx->param_type);
			break;
		}
	}
}

void wlan_dp_pdev_cfg_sync_profile(struct cdp_soc_t *cdp_soc, uint8_t pdev_id)
{
	struct wlan_dp_memory_profile_info *profile_info;
	struct wlan_dp_memory_profile_ctx *profile_ctx;
	cdp_config_param_type val = {0};
	QDF_STATUS status;
	int cur_val, i;

	profile_info = wlan_dp_get_profile_info();
	if (!profile_info->is_selected)
		return;

	for (i = 0; i < profile_info->size; i++) {
		profile_ctx = &profile_info->ctx[i];
		if (profile_ctx->param_type == DP_RXDMA_BUF_RING_SIZE_CFG) {
			status = cdp_txrx_get_pdev_param(cdp_soc, pdev_id,
					CDP_CONFIG_RXDMA_BUF_RING_SIZE, &val);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    val.cdp_rxdma_buf_ring_size != profile_ctx->size) {
				cur_val = val.cdp_rxdma_buf_ring_size;
				val.cdp_rxdma_buf_ring_size = profile_ctx->size;
				if (cdp_txrx_set_pdev_param(cdp_soc, pdev_id,
							    CDP_CONFIG_RXDMA_BUF_RING_SIZE, val)) {
					dp_err("unable to sync param type:%u", profile_ctx->param_type);
					return;
				}
				dp_info("current RXDMA buf ring size:%u synced with profile:%u", cur_val, profile_ctx->size);
			}
			return;
		}
	}

	dp_err("pdev based config item not found in profile table");
}
#endif

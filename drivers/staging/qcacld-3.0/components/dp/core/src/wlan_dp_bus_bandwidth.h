/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(WLAN_DP_BUS_BANDWIDTH_H)
#define WLAN_DP_BUS_BANDWIDTH_H
/**
 * DOC: wlan_dp_bus_bandwidth.h
 *
 * Bus Bandwidth Manager implementation
 */

#include "wlan_dp_priv.h"
#include <qdf_types.h>
#include <qca_vendor.h>
#include <wlan_objmgr_psoc_obj.h>
#include "wlan_dp_public_struct.h"
#include "wlan_dp_priv.h"
#include <cdp_txrx_misc.h>

typedef const enum bus_bw_level
	bus_bw_table_type[QCA_WLAN_802_11_MODE_INVALID][TPUT_LEVEL_MAX];

/**
 * struct bbm_context: Bus Bandwidth Manager context
 *
 * @curr_bus_bw_lookup_table: current bus bw lookup table
 * @curr_vote_level: current vote level
 * @per_policy_vote: per BBM policy related vote
 * @bbm_lock: BBM API lock
 */
struct bbm_context {
	bus_bw_table_type *curr_bus_bw_lookup_table;
	enum bus_bw_level curr_vote_level;
	enum bus_bw_level per_policy_vote[BBM_MAX_POLICY];
	qdf_mutex_t bbm_lock;
};

#ifdef FEATURE_BUS_BANDWIDTH_MGR
/**
 * dp_bbm_context_init() - Initialize BBM context
 * @psoc: psoc Handle
 *
 * Returns: error code
 */
int dp_bbm_context_init(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bbm_context_deinit() - De-initialize BBM context
 * @psoc: psoc Handle
 *
 * Returns: None
 */
void dp_bbm_context_deinit(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bbm_apply_independent_policy() - Function to apply independent policies
 *  to set the bus bw level
 * @psoc: psoc Handle
 * @params: BBM policy related params
 *
 * The function applies BBM related policies and appropriately sets the bus
 * bandwidth level.
 *
 * Returns: None
 */
void dp_bbm_apply_independent_policy(struct wlan_objmgr_psoc *psoc,
				     struct bbm_params *params);
#else
static inline int dp_bbm_context_init(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline void dp_bbm_context_deinit(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bbm_apply_independent_policy(struct wlan_objmgr_psoc *psoc,
				     struct bbm_params *params)
{
}
#endif /* FEATURE_BUS_BANDWIDTH_MGR */

#if defined(WLAN_FEATURE_DP_BUS_BANDWIDTH) && defined(FEATURE_RUNTIME_PM)
/**
 * dp_rtpm_tput_policy_init() - Initialize RTPM tput policy
 * @psoc: psoc handle
 *
 * Returns: None
 */
void dp_rtpm_tput_policy_init(struct wlan_objmgr_psoc *psoc);

/**
 * dp_rtpm_tput_policy_deinit() - Deinitialize RTPM tput policy
 * @psoc: psoc handle
 *
 * Returns: None
 */
void dp_rtpm_tput_policy_deinit(struct wlan_objmgr_psoc *psoc);

/**
 * dp_rtpm_tput_policy_apply() - Apply RTPM tput policy
 * @dp_ctx: dp_ctx handle
 * @tput_level : Tput level
 *
 * Returns: None
 */
void dp_rtpm_tput_policy_apply(struct wlan_dp_psoc_context *dp_ctx,
			       enum tput_level tput_level);

/**
 * dp_rtpm_tput_policy_get_vote() - Get RTPM tput policy vote
 * @dp_ctx: dp_ctx handle
 *
 * Returns: Current vote
 */
int dp_rtpm_tput_policy_get_vote(struct wlan_dp_psoc_context *dp_ctx);
#else
static inline
void dp_rtpm_tput_policy_init(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_rtpm_tput_policy_deinit(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_rtpm_tput_policy_apply(struct wlan_dp_psoc_context *dp_ctx,
			       enum tput_level tput_level)
{
}

static inline int
dp_rtpm_tput_policy_get_vote(struct wlan_dp_psoc_context *dp_ctx)
{
	return -EINVAL;
}
#endif /* WLAN_FEATURE_DP_BUS_BANDWIDTH && FEATURE_RUNTIME_PM */

/**
 * dp_set_high_bus_bw_request() - Set High Bandwidth request value
 * @psoc: psoc handle
 * @vdev_id: Vdev ID
 * @high_bus_bw : Flag to set or clear high bandwidth request
 *
 * Return: None
 */
static inline void dp_set_high_bus_bw_request(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id,
					      bool high_bus_bw)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (high_bus_bw)
		dp_ctx->high_bus_bw_request |= (1 << vdev_id);
	else
		dp_ctx->high_bus_bw_request &= ~(1 << vdev_id);
}

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * dp_reset_tcp_delack() - Reset tcp delack value to default
 * @psoc: psoc handle
 *
 * Function used to reset TCP delack value to its default value
 *
 * Return: None
 */
void dp_reset_tcp_delack(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_dp_update_tcp_rx_param() - update TCP param in RX dir
 * @dp_ctx: Pointer to DP context
 * @data: Parameters to update
 *
 * Return: None
 */
void wlan_dp_update_tcp_rx_param(struct wlan_dp_psoc_context *dp_ctx,
				 struct wlan_rx_tp_data *data);

#ifdef RX_PERFORMANCE
/**
 * dp_is_current_high_throughput() - Check if vote level is high
 * @dp_ctx: Pointer to DP context
 *
 * Function used to check if vote level is high
 *
 * Return: True if vote level is high
 */
bool dp_is_current_high_throughput(struct wlan_dp_psoc_context *dp_ctx);
#else
static inline
bool dp_is_current_high_throughput(struct wlan_dp_psoc_context *dp_ctx)
{
	return false;
}
#endif /* RX_PERFORMANCE */

/**
 * dp_reset_tcp_delack() - Reset TCP delack
 * @psoc: psoc handle
 *
 * Return: None
 */
void dp_reset_tcp_delack(struct wlan_objmgr_psoc *psoc);

/**
 * dp_get_current_throughput_level() - Get the current vote
 * level
 * @dp_ctx: DP Context handle
 *
 * Return: current vote level
 */
static inline enum pld_bus_width_type
dp_get_current_throughput_level(struct wlan_dp_psoc_context *dp_ctx)
{
	return dp_ctx->cur_vote_level;
}

/**
 * dp_set_current_throughput_level() - update the current vote
 * level
 * @psoc: psoc object
 * @next_vote_level: pld_bus_width_type voting level
 *
 * This function updates the current vote level to the new level
 * provided
 *
 * Return: None
 */
static inline void
dp_set_current_throughput_level(struct wlan_objmgr_psoc *psoc,
				enum pld_bus_width_type next_vote_level)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	dp_ctx->cur_vote_level = next_vote_level;
}

/**
 * wlan_dp_display_tx_rx_histogram() - display tx rx histogram
 * @psoc: psoc handle
 *
 * Return: none
 */
void wlan_dp_display_tx_rx_histogram(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_dp_clear_tx_rx_histogram() - clear tx rx histogram
 * @psoc: psoc handle
 *
 * Return: none
 */
void wlan_dp_clear_tx_rx_histogram(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bandwidth_init() - Initialize bus bandwidth data structures.
 * @psoc: psoc handle
 *
 * Initialize bus bandwidth related data structures like spinlock and timer.
 *
 * Return: None.
 */
int dp_bus_bandwidth_init(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bandwidth_deinit() - De-initialize bus bandwidth data structures.
 * @psoc: psoc handle
 *
 * De-initialize bus bandwidth related data structures like timer.
 *
 * Return: None.
 */
void dp_bus_bandwidth_deinit(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bw_compute_timer_start() - start the bandwidth timer
 * @psoc: psoc handle
 *
 * Return: None
 */
void dp_bus_bw_compute_timer_start(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bw_compute_timer_try_start() - try to start the bandwidth timer
 * @psoc: psoc handle
 *
 * This function ensures there is at least one interface in the assoc state
 * before starting the bandwidth timer.
 *
 * Return: None
 */
void dp_bus_bw_compute_timer_try_start(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bw_compute_timer_stop() - stop the bandwidth timer
 * @psoc: psoc handle
 *
 * Return: None
 */
void dp_bus_bw_compute_timer_stop(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bw_compute_timer_try_stop() - try to stop the bandwidth timer
 * @psoc: psoc handle
 *
 * This function ensures there are no interface in the assoc state before
 * stopping the bandwidth timer.
 *
 * Return: None
 */
void dp_bus_bw_compute_timer_try_stop(struct wlan_objmgr_psoc *psoc);

/**
 * dp_bus_bw_compute_prev_txrx_stats() - get tx and rx stats
 * @vdev: vdev handle
 *
 * This function get the collected tx and rx stats before starting
 * the bus bandwidth timer.
 *
 * Return: None
 */
void dp_bus_bw_compute_prev_txrx_stats(struct wlan_objmgr_vdev *vdev);

/**
 * dp_bus_bw_compute_reset_prev_txrx_stats() - reset previous tx and rx stats
 * @vdev: vdev handle
 *
 * This function resets the previous tx rx stats.
 *
 * Return: None
 */
void dp_bus_bw_compute_reset_prev_txrx_stats(struct wlan_objmgr_vdev *vdev);

/**
 * dp_get_bus_bw_high_threshold() - Get the bus bw high threshold
 * level
 * @dp_ctx: DP Context handle
 *
 * Return: bus bw high threshold
 */
static inline uint32_t
dp_get_bus_bw_high_threshold(struct wlan_dp_psoc_context *dp_ctx)
{
	return dp_ctx->dp_cfg.bus_bw_high_threshold;
}

#else
static inline
void dp_reset_tcp_delack(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void wlan_dp_update_tcp_rx_param(struct wlan_dp_psoc_context *dp_ctx,
				 struct wlan_rx_tp_data *data)
{
}

static inline
bool dp_is_current_high_throughput(struct wlan_dp_psoc_context *dp_ctx)
{
	return false;
}

static inline enum pld_bus_width_type
dp_get_current_throughput_level(struct wlan_dp_psoc_context *dp_ctx)
{
	return PLD_BUS_WIDTH_NONE;
}

static inline
void dp_set_current_throughput_level(struct wlan_objmgr_psoc *psoc,
				     enum pld_bus_width_type next_vote_level)
{
}

static inline
void wlan_dp_display_tx_rx_histogram(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void wlan_dp_clear_tx_rx_histogram(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bandwidth_init(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bandwidth_deinit(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bw_compute_timer_start(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bw_compute_timer_try_start(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bw_compute_timer_stop(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bw_compute_timer_try_stop(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void dp_bus_bw_compute_prev_txrx_stats(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void dp_bus_bw_compute_reset_prev_txrx_stats(struct wlan_objmgr_vdev *vdev)
{
}

static inline uint32_t
dp_get_bus_bw_high_threshold(struct wlan_dp_psoc_context *dp_ctx)
{
	return 0;
}

#endif /* WLAN_FEATURE_DP_BUS_BANDWIDTH */
#endif /* WLAN_DP_BUS_BANDWIDTH_H */

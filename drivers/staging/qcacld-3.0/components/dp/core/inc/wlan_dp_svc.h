/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_DP_SVC_H_
#define _WLAN_DP_SVC_H_

#include "wlan_dp_main.h"
#include "wlan_dp_metadata.h"
#include <qdf_status.h>
#include "wlan_dp_priv.h"

#ifdef WLAN_SUPPORT_SERVICE_CLASS
/**
 * struct dp_svc_ctx - service class context
 * @svc_table: service class table
 * @valid_svc_count: valid svc entries
 * @dp_svc_lock: svc lock
 */
struct dp_svc_ctx {
	struct dp_svc_data __rcu *svc_table[DP_SVC_ARRAY_SIZE];
	uint32_t valid_svc_count;
	qdf_spinlock_t dp_svc_lock;
};

/**
 * dp_svc_add() - Add service class
 * @svc_data: pointer to service class
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS dp_svc_add(struct dp_svc_data *svc_data);

/**
 * dp_svc_remove() - Remove service class
 * @svc_id: service class id
 *
 * Returns: Zero on successful deletion of service class
 */
QDF_STATUS dp_svc_remove(uint8_t svc_id);

/**
 * dp_svc_dump() - Dump service class table
 *
 * Returns: void
 */
void dp_svc_dump(void);

/**
 * dp_svc_get() - Get service class details
 * @svc_id: service class id
 * @svc_table: pointer to service class table
 * @table_size: number of services in a svc_table
 *
 * Returns: returns number of entries in a table
 */
uint8_t dp_svc_get(uint8_t svc_id, struct dp_svc_data *svc_table,
		   uint16_t table_size);

/**
 * dp_svc_get_meta_data_by_id() - Get metadata by service id
 * @svc_id: service class id
 * @nbuf: pointer to network buffer
 * @metadata: pointer to metadata
 *
 * Returns: QDF_STATUS_SUCCESS upon successful find and metadata update
 */
QDF_STATUS dp_svc_get_meta_data_by_id(uint8_t svc_id, qdf_nbuf_t nbuf,
				      uint32_t *metadata);

/**
 * dp_get_svc_ctx() - Get svc context
 *
 * Returns: pointer to dp_svc_ctx
 */
static inline struct dp_svc_ctx *dp_get_svc_ctx(void)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_get_context();

	if (dp_ctx)
		return (struct dp_svc_ctx *)dp_ctx->svc_ctx;

	return NULL;
}

/**
 * dp_svc_get_buffer_latency_tolerance_by_id() - Get buffer latency tolarence
 * @svc_id: service class id
 * @buffer_latency_tolerance: buffer to get buffer latency tolerance value
 *
 * Returns: QDF_STATUS
 */
static inline QDF_STATUS
dp_svc_get_buffer_latency_tolerance_by_id(uint8_t svc_id,
					  uint32_t *buffer_latency_tolerance)
{
	struct dp_svc_ctx *svc_ctx = dp_get_svc_ctx();
	struct dp_svc_data *svc_data;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	qdf_rcu_read_lock_bh();
	if (svc_ctx && svc_id < DP_SVC_ARRAY_SIZE) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (svc_data) {
			*buffer_latency_tolerance =
			    svc_data->buffer_latency_tolerance;
			status = QDF_STATUS_SUCCESS;
		}
	}
	qdf_rcu_read_unlock_bh();

	return status;
}

/**
 * dp_svc_get_app_ind_default_dscp_by_id() - Get default dscp value of svc
 * @svc_id: service class id
 * @app_ind_default_dscp: buffer to get default dscp value
 *
 * Returns: QDF_STATUS
 */
static inline
QDF_STATUS dp_svc_get_app_ind_default_dscp_by_id(uint8_t svc_id,
						 uint8_t *app_ind_default_dscp)
{
	struct dp_svc_ctx *svc_ctx = dp_get_svc_ctx();
	struct dp_svc_data *svc_data;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	qdf_rcu_read_lock_bh();
	if (svc_ctx && svc_id < DP_SVC_ARRAY_SIZE) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (svc_data &&
		    svc_data->flags & DP_SVC_FLAGS_APP_IND_DEF_DSCP) {
			*app_ind_default_dscp =
			    svc_data->app_ind_default_dscp;
			status = QDF_STATUS_SUCCESS;
		}
	}
	qdf_rcu_read_unlock_bh();

	return status;
}

/**
 * dp_svc_get_app_ind_special_dscp_by_id() - Get special dscp value of svc
 * @svc_id: service class id
 * @app_ind_special_dscp: buffer to get special dscp value
 *
 * Returns: QDF_STATUS
 */
static inline
QDF_STATUS dp_svc_get_app_ind_special_dscp_by_id(uint8_t svc_id,
						 uint8_t *app_ind_special_dscp)
{
	struct dp_svc_ctx *svc_ctx = dp_get_svc_ctx();
	struct dp_svc_data *svc_data;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	qdf_rcu_read_lock_bh();
	if (svc_ctx && svc_id < DP_SVC_ARRAY_SIZE) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (svc_data &&
		    svc_data->flags & DP_SVC_FLAGS_APP_IND_SPL_DSCP) {
			*app_ind_special_dscp =
			    svc_data->app_ind_special_dscp;
			status = QDF_STATUS_SUCCESS;
		}
	}
	qdf_rcu_read_unlock_bh();

	return status;
}

/**
 * dp_svc_inc_policy_ref_cnt_by_id() - Increment reference on service class
 * @svc_id: service class id
 *
 * Returns: QDF_STATUS
 */
static inline QDF_STATUS dp_svc_inc_policy_ref_cnt_by_id(uint8_t svc_id)
{
	struct dp_svc_ctx *svc_ctx = dp_get_svc_ctx();
	struct dp_svc_data *svc_data;
	qdf_spinlock_t *lock;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (svc_ctx && svc_id < DP_SVC_ARRAY_SIZE) {
		lock = &svc_ctx->dp_svc_lock;
		qdf_spin_lock_bh(lock);
		svc_data =
		    qdf_rcu_dereference_protected(svc_ctx->svc_table[svc_id],
						  qdf_lockdep_is_held(lock));
		if (svc_data) {
			svc_data->policy_ref_count++;
			status = QDF_STATUS_SUCCESS;
		}
		qdf_spin_unlock_bh(&svc_ctx->dp_svc_lock);
	}
	return status;
}

/**
 * dp_svc_dec_policy_ref_cnt_by_id() - Decrement reference on service class
 * @svc_id: service class id
 *
 * Returns: QDF_STATUS
 */
static inline QDF_STATUS dp_svc_dec_policy_ref_cnt_by_id(uint8_t svc_id)
{
	struct dp_svc_ctx *svc_ctx = dp_get_svc_ctx();
	struct dp_svc_data *svc_data;
	qdf_spinlock_t *lock;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (svc_ctx && svc_id < DP_SVC_ARRAY_SIZE) {
		lock = &svc_ctx->dp_svc_lock;
		qdf_spin_lock_bh(lock);
		svc_data =
		    qdf_rcu_dereference_protected(svc_ctx->svc_table[svc_id],
						  qdf_lockdep_is_held(lock));
		if (svc_data) {
			svc_data->policy_ref_count--;
			status = QDF_STATUS_SUCCESS;
		}
		qdf_spin_unlock_bh(&svc_ctx->dp_svc_lock);
	}
	return status;
}

/*
 * wlan_dp_svc_init() - Service class initialize
 * @dp_ctx: dp_ctx
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_svc_init(struct wlan_dp_psoc_context *dp_ctx);

/*
 * wlan_dp_svc_deinit() -Service class deinitialize
 * @dp_ctx: dp_ctx
 *
 * Return: None
 */
void wlan_dp_svc_deinit(struct wlan_dp_psoc_context *dp_ctx);
#else
static inline
QDF_STATUS wlan_dp_svc_init(struct wlan_dp_psoc_context *dp_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
void wlan_dp_svc_deinit(struct wlan_dp_psoc_context *dp_ctx)
{
}
#endif

#endif

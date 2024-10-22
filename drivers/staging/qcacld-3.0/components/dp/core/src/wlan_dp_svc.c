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

#include <dp_types.h>
#include <dp_internal.h>
#include "wlan_dp_svc.h"
#include <qdf_status.h>

static bool
dp_svc_is_matched(struct dp_svc_data *svc, struct dp_svc_data *tsvc)
{
	if ((svc->flags & DP_SVC_FLAGS_BUFFER_LATENCY_TOLERANCE) &&
	    (svc->buffer_latency_tolerance != tsvc->buffer_latency_tolerance))
		return false;
	if ((svc->flags & DP_SVC_FLAGS_APP_IND_DEF_DSCP) &&
	    (svc->app_ind_default_dscp != tsvc->app_ind_default_dscp))
		return false;
	if ((svc->flags & DP_SVC_FLAGS_APP_IND_SPL_DSCP) &&
	    (svc->app_ind_special_dscp != tsvc->app_ind_special_dscp))
		return false;

	return true;
}

static bool
dp_svc_is_duplicate(struct dp_svc_ctx *svc_ctx, struct dp_svc_data *data)
{
	uint8_t svc_id;
	struct dp_svc_data *svc_data;
	bool ret = false;

	qdf_rcu_read_lock_bh();
	for (svc_id = 0; svc_id < DP_SVC_ARRAY_SIZE; svc_id++) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (!svc_data)
			continue;

		if (data->svc_id == svc_id) {
			ret = true;
			break;
		}
		if ((svc_data->flags == data->flags) &&
		    dp_svc_is_matched(svc_data, data)) {
			ret = true;
			break;
		}
	}
	qdf_rcu_read_unlock_bh();

	return ret;
}

QDF_STATUS dp_svc_add(struct dp_svc_data *data)
{
	struct dp_svc_ctx *svc_ctx = NULL;
	struct dp_svc_data *svc_data = NULL, *temp;
	qdf_spinlock_t *lock;

	if (!data)
		return QDF_STATUS_E_INVAL;

	dp_info("service class add: svc_id:%u "
		"buffer_latency_tolerance:%u "
		"app_ind_default_dscp:%u app_ind_special_dscp:%u",
		data->svc_id, data->buffer_latency_tolerance,
		data->app_ind_default_dscp, data->app_ind_special_dscp);

	if (data->svc_id >= DP_MAX_SVC) {
		dp_err("invalid svc_id");
		return QDF_STATUS_E_INVAL;
	}

	svc_ctx = dp_get_svc_ctx();
	if (!svc_ctx) {
		dp_err("svc_ctx is not initialised");
		return QDF_STATUS_E_INVAL;
	}

	if (dp_svc_is_duplicate(svc_ctx, data)) {
		dp_err("duplicate svc_id/entry is not allowed");
		return QDF_STATUS_E_FAILURE;
	}

	svc_data = qdf_mem_malloc(sizeof(struct dp_svc_data));
	if (!svc_data) {
		dp_err("memory allocation failure");
		return QDF_STATUS_E_FAILURE;
	}

	svc_data->buffer_latency_tolerance = data->buffer_latency_tolerance;
	svc_data->app_ind_default_dscp = data->app_ind_default_dscp;
	svc_data->app_ind_special_dscp = data->app_ind_special_dscp;
	svc_data->flags = data->flags;
	svc_data->policy_ref_count = 0;

	lock = &svc_ctx->dp_svc_lock;
	qdf_spin_lock_bh(lock);
	if (svc_ctx->valid_svc_count == DP_MAX_SVC) {
		qdf_spin_unlock_bh(lock);
		qdf_mem_free(svc_data);
		dp_err("maximum service_class limit:%u reached", DP_MAX_SVC);
		return QDF_STATUS_E_FAILURE;
	}

	temp = qdf_rcu_dereference_protected(svc_ctx->svc_table[data->svc_id],
					     qdf_lockdep_is_held(lock));
	if (!temp) {
		svc_data->svc_id = data->svc_id;
		qdf_rcu_assign_pointer(svc_ctx->svc_table[svc_data->svc_id],
				       svc_data);
	} else {
		qdf_spin_unlock_bh(lock);
		qdf_mem_free(svc_data);
		dp_err("svc_id:%u is already in use", data->svc_id);
		return QDF_STATUS_E_FAILURE;
	}
	svc_ctx->valid_svc_count++;
	qdf_spin_unlock_bh(lock);

	dp_info("new svc added: svc_id:%u", svc_data->svc_id);

	return QDF_STATUS_SUCCESS;
}

static void dp_svc_delete_all(struct dp_svc_ctx *svc_ctx)
{
	struct dp_svc_data *svc_data;
	qdf_spinlock_t *lock;
	qdf_list_t tmp_list;
	qdf_list_node_t *node;
	uint8_t i;

	if (!svc_ctx) {
		dp_err("svc_ctx is not initialised");
		return;
	}

	qdf_list_create(&tmp_list, 0);

	qdf_spin_lock_bh(&svc_ctx->dp_svc_lock);
	lock = &svc_ctx->dp_svc_lock;

	for (i = 0; i < DP_SVC_ARRAY_SIZE; i++) {
		svc_data =
		    qdf_rcu_dereference_protected(svc_ctx->svc_table[i],
						  qdf_lockdep_is_held(lock));
		if (svc_data) {
			qdf_rcu_assign_pointer(svc_ctx->svc_table[i], NULL);
			svc_ctx->valid_svc_count--;
			qdf_list_insert_back(&tmp_list, &svc_data->node);
		}
	}
	qdf_spin_unlock_bh(&svc_ctx->dp_svc_lock);

	synchronize_rcu();
	while (!qdf_list_empty(&tmp_list)) {
		qdf_list_remove_front(&tmp_list, &node);
		svc_data = container_of(node, struct dp_svc_data, node);
		qdf_mem_free(svc_data);
	}
	qdf_list_destroy(&tmp_list);
}

QDF_STATUS dp_svc_remove(uint8_t svc_id)
{
	struct dp_svc_ctx *svc_ctx;
	int ret = QDF_STATUS_SUCCESS;
	struct dp_svc_data *svc_data;
	qdf_spinlock_t *lock;

	if (svc_id >= DP_SVC_ARRAY_SIZE) {
		dp_err("invalid svc_id:%u for delete", svc_id);
		return QDF_STATUS_E_INVAL;
	}

	svc_ctx = dp_get_svc_ctx();
	if (!svc_ctx) {
		dp_err("svc_ctx is not initialised");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&svc_ctx->dp_svc_lock);
	lock = &svc_ctx->dp_svc_lock;

	svc_data = qdf_rcu_dereference_protected(svc_ctx->svc_table[svc_id],
						 qdf_lockdep_is_held(lock));
	if (!svc_data) {
		dp_info("no service class with svc_id:%u", svc_id);
		ret = QDF_STATUS_E_INVAL;
		goto fail;
	}

	if (svc_data->policy_ref_count) {
		dp_err("svc_id:%u is in use with ref:%u", svc_id,
		       svc_data->policy_ref_count);
		ret = QDF_STATUS_E_INVAL;
		goto fail;
	}

	qdf_rcu_assign_pointer(svc_ctx->svc_table[svc_id], NULL);
	svc_ctx->valid_svc_count--;
	qdf_spin_unlock_bh(&svc_ctx->dp_svc_lock);
	dp_info("service class deleted for svc_id:%d", svc_id);
	synchronize_rcu();
	qdf_mem_free(svc_data);

	return ret;
fail:
	qdf_spin_unlock_bh(&svc_ctx->dp_svc_lock);
	return ret;
}

void dp_svc_dump(void)
{
	uint8_t svc_id = DP_SVC_INVALID_ID;
	struct dp_svc_ctx *svc_ctx;
	struct dp_svc_data *svc_data;

	svc_ctx = dp_get_svc_ctx();
	if (!svc_ctx) {
		dp_err("svc_ctx is not initialised");
		return;
	}

	qdf_rcu_read_lock_bh();
	for (svc_id = 0; svc_id < DP_SVC_ARRAY_SIZE; svc_id++) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (svc_data)
			dp_info("svc_id:%d buffer_latency_tolerance:%d "
				"app_ind_default_dscp:%d "
				"app_ind_special_dscp:%d "
				"policy_ref_count:%d",
				svc_data->svc_id,
				svc_data->buffer_latency_tolerance,
				svc_data->app_ind_default_dscp,
				svc_data->app_ind_special_dscp,
				svc_data->policy_ref_count);
	}
	qdf_rcu_read_unlock_bh();
}

static inline void
dp_svc_copy(struct dp_svc_data *dst_svc, struct dp_svc_data *src_svc)
{
	dst_svc->svc_id = src_svc->svc_id;
	dst_svc->buffer_latency_tolerance =
		 src_svc->buffer_latency_tolerance;
	dst_svc->app_ind_default_dscp =
		 src_svc->app_ind_default_dscp;
	dst_svc->app_ind_special_dscp =
		 src_svc->app_ind_special_dscp;
	dst_svc->flags = src_svc->flags;
}

uint8_t dp_svc_get(uint8_t svc_id, struct dp_svc_data *svc_table,
		   uint16_t table_size)
{
	uint16_t count = 0;
	struct dp_svc_data *svc_data;
	struct dp_svc_ctx *svc_ctx;

	svc_ctx = dp_get_svc_ctx();
	if (!svc_ctx) {
		dp_err("svc_ctx is not initialised");
		return count;
	}

	qdf_rcu_read_lock_bh();
	if (svc_id != DP_SVC_INVALID_ID) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (svc_data && (count + 1 < table_size)) {
			dp_svc_copy(&svc_table[count], svc_data);
			count++;
		}
	} else {
		for (svc_id = 0; svc_id < DP_SVC_ARRAY_SIZE; svc_id++) {
			svc_data =
				qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
			if (svc_data && (count + 1 < table_size)) {
				dp_svc_copy(&svc_table[count], svc_data);
				count++;
			}
		}
	}
	qdf_rcu_read_unlock_bh();

	return count;
}

QDF_STATUS
dp_svc_get_meta_data_by_id(uint8_t svc_id, qdf_nbuf_t nbuf, uint32_t *metadata)
{
	struct dp_svc_ctx *svc_ctx = dp_get_svc_ctx();
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct dp_svc_data *svc_data;

	if (!svc_ctx) {
		dp_err("svc_ctx is not initialised");
		return status;
	}

	qdf_rcu_read_lock_bh();
	if (svc_id < DP_SVC_ARRAY_SIZE) {
		svc_data = qdf_rcu_dereference(svc_ctx->svc_table[svc_id]);
		if (svc_data) {
			/* currently only LAPB metadata supported */
			*metadata = PREPARE_METADATA(LAPB_VALID_TAG, svc_id);
			status = QDF_STATUS_SUCCESS;
		}
	}
	qdf_rcu_read_unlock_bh();

	return status;
}

QDF_STATUS wlan_dp_svc_init(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_svc_ctx *svc_ctx = NULL;
	uint32_t i;

	if (!dp_ctx) {
		dp_err("DP context not found");
		return QDF_STATUS_E_FAILURE;
	}

	svc_ctx = qdf_mem_malloc(sizeof(struct dp_svc_ctx));
	if (!svc_ctx)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&svc_ctx->dp_svc_lock);

	qdf_spin_lock_bh(&svc_ctx->dp_svc_lock);
	for (i = 0; i < DP_SVC_ARRAY_SIZE; i++)
		qdf_rcu_assign_pointer(svc_ctx->svc_table[i], NULL);
	qdf_spin_unlock_bh(&svc_ctx->dp_svc_lock);
	dp_ctx->svc_ctx = svc_ctx;

	dp_info("service_class init successful");

	return QDF_STATUS_SUCCESS;
}

void wlan_dp_svc_deinit(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_svc_ctx *svc_ctx;

	if (!dp_ctx) {
		dp_err("DP context not found");
		return;
	}

	svc_ctx = dp_ctx->svc_ctx;
	dp_svc_delete_all(svc_ctx);
	qdf_spinlock_destroy(&svc_ctx->dp_svc_lock);
	qdf_mem_free(svc_ctx);
	dp_ctx->svc_ctx = NULL;

	dp_info("service_class deinit successful");
}

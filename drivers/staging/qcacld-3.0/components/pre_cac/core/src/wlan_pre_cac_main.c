/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: Implement various notification handlers which are accessed
 * internally in pre_cac component only.
 */

#include "wlan_pre_cac_main.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_reg_services_api.h"
#include "wlan_mlme_api.h"

struct pre_cac_ops *glbl_pre_cac_ops;

void pre_cac_stop(struct wlan_objmgr_psoc *psoc)
{
	struct pre_cac_psoc_priv *psoc_priv = pre_cac_psoc_get_priv(psoc);

	if (!psoc_priv)
		return;
	pre_cac_debug("flush pre_cac_work");
	if (psoc_priv->pre_cac_work.fn)
		qdf_flush_work(&psoc_priv->pre_cac_work);
}

void pre_cac_set_freq(struct wlan_objmgr_vdev *vdev,
		      qdf_freq_t freq)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return;

	vdev_priv->pre_cac_freq = freq;
}

qdf_freq_t pre_cac_get_freq(struct wlan_objmgr_vdev *vdev)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return 0;

	return vdev_priv->pre_cac_freq;
}

void pre_cac_set_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev,
				     qdf_freq_t freq)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return;

	vdev_priv->freq_before_pre_cac = freq;
}

qdf_freq_t pre_cac_get_freq_before_pre_cac(struct wlan_objmgr_vdev *vdev)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return 0;

	return vdev_priv->freq_before_pre_cac;
}

void pre_cac_adapter_set(struct wlan_objmgr_vdev *vdev,
			 bool status)
{
	struct pre_cac_vdev_priv *vdev_priv;

	if (!vdev) {
		pre_cac_debug("vdev is NULL");
		return;
	}

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return;

	vdev_priv->is_pre_cac_adapter = status;
}

bool pre_cac_adapter_is_active(struct wlan_objmgr_vdev *vdev)
{
	struct pre_cac_vdev_priv *vdev_priv;

	if (!vdev) {
		pre_cac_debug("vdev is NULL");
		return false;
	}

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return false;

	return vdev_priv->is_pre_cac_adapter;
}

void pre_cac_complete_set(struct wlan_objmgr_vdev *vdev,
			  bool status)
{
	struct pre_cac_vdev_priv *vdev_priv;

	if (!vdev) {
		pre_cac_debug("vdev is NULL");
		return;
	}

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return;

	vdev_priv->pre_cac_complete = status;
}

bool pre_cac_complete_get(struct wlan_objmgr_vdev *vdev)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return false;

	return vdev_priv->pre_cac_complete;
}

static void pre_cac_complete(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id,
			     QDF_STATUS status)
{
	if (glbl_pre_cac_ops &&
	    glbl_pre_cac_ops->pre_cac_complete_cb)
		glbl_pre_cac_ops->pre_cac_complete_cb(psoc, vdev_id, status);
}

static void pre_cac_handle_success(void *data)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)data;
	struct pre_cac_psoc_priv *psoc_priv;

	psoc_priv = pre_cac_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pre_cac_err("Invalid psoc priv");
		return;
	}
	pre_cac_debug("vdev id %d", psoc_priv->pre_cac_vdev_id);
	pre_cac_complete(psoc, psoc_priv->pre_cac_vdev_id, QDF_STATUS_SUCCESS);
}

static void pre_cac_conditional_csa_ind(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, bool status)
{
	if (glbl_pre_cac_ops &&
	    glbl_pre_cac_ops->pre_cac_conditional_csa_ind_cb)
		glbl_pre_cac_ops->pre_cac_conditional_csa_ind_cb(psoc,
							vdev_id, status);
}

static void pre_cac_handle_failure(void *data)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)data;
	struct pre_cac_psoc_priv *psoc_priv;

	psoc_priv = pre_cac_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pre_cac_err("Invalid psoc priv");
		return;
	}
	pre_cac_debug("vdev id %d", psoc_priv->pre_cac_vdev_id);
	pre_cac_complete(psoc, psoc_priv->pre_cac_vdev_id,
			 QDF_STATUS_E_FAILURE);
}

void pre_cac_clean_up(struct wlan_objmgr_psoc *psoc)
{
	struct pre_cac_psoc_priv *psoc_priv = pre_cac_psoc_get_priv(psoc);
	uint8_t vdev_id;

	if (!psoc_priv) {
		pre_cac_err("invalid psoc");
		return;
	}

	if (!pre_cac_is_active(psoc))
		return;

	if (pre_cac_is_active(psoc) && psoc_priv->pre_cac_work.fn) {
		pre_cac_debug("pre_cac_work already shceduled");
		return;
	}
	pre_cac_get_vdev_id(psoc, &vdev_id);
	pre_cac_debug("schedue pre_cac_work vdev %d", vdev_id);
	psoc_priv->pre_cac_vdev_id = vdev_id;
	qdf_create_work(0, &psoc_priv->pre_cac_work,
			pre_cac_handle_failure,
			psoc);
	qdf_sched_work(0, &psoc_priv->pre_cac_work);
}

void pre_cac_handle_radar_ind(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct pre_cac_psoc_priv *psoc_priv = pre_cac_psoc_get_priv(psoc);

	pre_cac_conditional_csa_ind(psoc, wlan_vdev_get_id(vdev), false);

	pre_cac_debug("schedue pre_cac_work vdev %d", wlan_vdev_get_id(vdev));
	psoc_priv->pre_cac_vdev_id = wlan_vdev_get_id(vdev);
	qdf_create_work(0, &psoc_priv->pre_cac_work,
			pre_cac_handle_failure,
			psoc);
	qdf_sched_work(0, &psoc_priv->pre_cac_work);
}

void pre_cac_handle_cac_end(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct pre_cac_psoc_priv *psoc_priv = pre_cac_psoc_get_priv(psoc);

	pre_cac_conditional_csa_ind(psoc, wlan_vdev_get_id(vdev), true);

	pre_cac_debug("schedue pre_cac_work vdev %d", wlan_vdev_get_id(vdev));
	psoc_priv->pre_cac_vdev_id = wlan_vdev_get_id(vdev);
	qdf_create_work(0, &psoc_priv->pre_cac_work,
			pre_cac_handle_success,
			psoc);
	qdf_sched_work(0, &psoc_priv->pre_cac_work);
}

static void pre_cac_get_vdev_id_handler(struct wlan_objmgr_psoc *psoc,
					void *obj, void *args)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct pre_cac_vdev_priv *vdev_priv;
	uint8_t *vdev_id = (uint8_t *)args;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return;

	if (vdev_priv->is_pre_cac_on)
		*vdev_id = vdev->vdev_objmgr.vdev_id;
}

void pre_cac_get_vdev_id(struct wlan_objmgr_psoc *psoc,
			 uint8_t *vdev_id)
{
	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     pre_cac_get_vdev_id_handler,
				     vdev_id, true, WLAN_PRE_CAC_ID);
}

int pre_cac_validate_and_get_freq(struct wlan_objmgr_pdev *pdev,
				  uint32_t chan_freq,
				  uint32_t *pre_cac_chan_freq)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	uint32_t len = CFG_VALID_CHANNEL_LIST_LEN;
	uint8_t pcl_weights[NUM_CHANNELS] = {0};
	uint32_t freq_list[NUM_CHANNELS] = {0};
	uint32_t weight_len = 0;
	QDF_STATUS status;
	uint32_t i;

	pre_cac_stop(psoc);

	if (pre_cac_is_active(psoc)) {
		pre_cac_err("pre cac is already in progress");
		return -EINVAL;
	}

	if (!chan_freq) {
		/* Channel is not obtained from PCL because PCL may not have
		 * the entire channel list. For example: if SAP is up on
		 * channel 6 and PCL is queried for the next SAP interface,
		 * if SCC is preferred, the PCL will contain only the channel
		 * 6. But, we are in need of a DFS channel. So, going with the
		 * first channel from the valid channel list.
		 */
		status = policy_mgr_get_valid_chans(psoc,
						    freq_list, &len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pre_cac_err("Failed to get channel list");
			return -EINVAL;
		}

		policy_mgr_update_with_safe_channel_list(psoc,
							 freq_list, &len,
							 pcl_weights,
							 weight_len);
		for (i = 0; i < len; i++) {
			if (wlan_reg_is_dfs_for_freq(pdev,
						     freq_list[i])) {
				*pre_cac_chan_freq = freq_list[i];
				break;
			}
		}

		if (*pre_cac_chan_freq == 0) {
			pre_cac_err("unable to find outdoor channel");
			return -EINVAL;
		}
	} else {
		/* Only when driver selects a channel, check is done for
		 * unnsafe and NOL channels. When user provides a fixed channel
		 * the user is expected to take care of this.
		 */
		if (!wlan_mlme_is_channel_valid(psoc, chan_freq) ||
		    !wlan_reg_is_dfs_for_freq(pdev, chan_freq)) {
			pre_cac_err("Invalid channel for pre cac:%d",
				    chan_freq);
			return -EINVAL;
		}
		*pre_cac_chan_freq = chan_freq;
	}

	pre_cac_debug("selected pre cac channel:%d", *pre_cac_chan_freq);
	return 0;
}

QDF_STATUS pre_cac_set_status(struct wlan_objmgr_vdev *vdev, bool status)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return QDF_STATUS_E_INVAL;

	vdev_priv->is_pre_cac_on = status;

	return QDF_STATUS_SUCCESS;
}

static void pre_cac_is_active_vdev_handler(struct wlan_objmgr_psoc *psoc,
					   void *obj, void *args)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct pre_cac_vdev_priv *vdev_priv;
	bool *is_pre_cac_on = (bool *)args;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv)
		return;

	*is_pre_cac_on = vdev_priv->is_pre_cac_on;
}

bool pre_cac_is_active(struct wlan_objmgr_psoc *psoc)
{
	bool is_pre_cac_on = false;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     pre_cac_is_active_vdev_handler,
				     &is_pre_cac_on, true, WLAN_PRE_CAC_ID);
	return is_pre_cac_on;
}

void pre_cac_clear_work(struct wlan_objmgr_psoc *psoc)
{
	struct pre_cac_psoc_priv *psoc_priv = pre_cac_psoc_get_priv(psoc);

	if (!psoc_priv)
		return;

	psoc_priv->pre_cac_work.fn = NULL;
	psoc_priv->pre_cac_work.arg = NULL;

	return;
}

struct pre_cac_vdev_priv *
pre_cac_vdev_get_priv_fl(struct wlan_objmgr_vdev *vdev,
			 const char *func, uint32_t line)
{
	struct pre_cac_vdev_priv *vdev_priv;

	vdev_priv = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							WLAN_UMAC_COMP_PRE_CAC);
	if (!vdev_priv) {
		pre_cac_nofl_err("%s:%u: vdev id: %d, vdev_priv is NULL",
				 func, line, wlan_vdev_get_id(vdev));
	}

	return vdev_priv;
}

struct pre_cac_psoc_priv *
pre_cac_psoc_get_priv_fl(struct wlan_objmgr_psoc *psoc,
			 const char *func, uint32_t line)
{
	struct pre_cac_psoc_priv *psoc_priv;

	psoc_priv = wlan_objmgr_psoc_get_comp_private_obj(psoc,
					WLAN_UMAC_COMP_PRE_CAC);
	if (!psoc_priv)
		pre_cac_nofl_err("%s:%u: psoc_priv is NULL", func, line);

	return psoc_priv;
}

void pre_cac_set_osif_cb(struct pre_cac_ops *osif_pre_cac_ops)
{
	glbl_pre_cac_ops = osif_pre_cac_ops;
}

QDF_STATUS
pre_cac_vdev_create_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pre_cac_vdev_priv *vdev_priv;
	QDF_STATUS status;

	vdev_priv = qdf_mem_malloc(sizeof(*vdev_priv));
	if (!vdev_priv) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	status = wlan_objmgr_vdev_component_obj_attach(
				vdev, WLAN_UMAC_COMP_PRE_CAC,
				(void *)vdev_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to attach priv with vdev");
		goto free_vdev_priv;
	}

	goto exit;

free_vdev_priv:
	qdf_mem_free(vdev_priv);
	status = QDF_STATUS_E_INVAL;
exit:
	return status;
}

QDF_STATUS
pre_cac_vdev_destroy_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pre_cac_vdev_priv *vdev_priv = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	vdev_priv = pre_cac_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pre_cac_err("vdev priv is NULL");
		goto exit;
	}

	status = wlan_objmgr_vdev_component_obj_detach(
					vdev, WLAN_UMAC_COMP_PRE_CAC,
					(void *)vdev_priv);
	if (QDF_IS_STATUS_ERROR(status))
		pre_cac_err("Failed to detach priv with vdev");

	qdf_mem_free(vdev_priv);
	vdev_priv = NULL;

exit:
	return status;
}

QDF_STATUS
pre_cac_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pre_cac_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = qdf_mem_malloc(sizeof(*psoc_priv));
	if (!psoc_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
				WLAN_UMAC_COMP_PRE_CAC,
				psoc_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to attach psoc component obj");
		goto free_psoc_priv;
	}

	return status;

free_psoc_priv:
	qdf_mem_free(psoc_priv);
	return status;
}

QDF_STATUS
pre_cac_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pre_cac_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = pre_cac_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pre_cac_err("psoc priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_PRE_CAC,
					psoc_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to detach psoc component obj");
		return status;
	}

	qdf_mem_free(psoc_priv);
	return status;
}

QDF_STATUS pre_cac_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_psoc_create_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to register psoc create handler");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_psoc_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to register psoc delete handler");
		goto fail_destroy_psoc;
	}

	status = wlan_objmgr_register_vdev_create_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_vdev_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to register vdev create handler");
		goto fail_create_vdev;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_vdev_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pre_cac_err("Failed to register vdev destroy handler");
		goto fail_destroy_vdev;
	}
	return status;

fail_destroy_vdev:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_PRE_CAC,
		pre_cac_vdev_create_notification, NULL);

fail_create_vdev:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_PRE_CAC,
		pre_cac_psoc_destroy_notification, NULL);

fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_PRE_CAC,
		pre_cac_psoc_create_notification, NULL);

	return status;
}

void pre_cac_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_vdev_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pre_cac_err("Failed to unregister vdev destroy handler");

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_vdev_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pre_cac_err("Failed to unregister vdev create handler");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_psoc_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pre_cac_err("Failed to unregister psoc destroy handler");

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_PRE_CAC,
				pre_cac_psoc_create_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pre_cac_err("Failed to unregister psoc create handler");
}

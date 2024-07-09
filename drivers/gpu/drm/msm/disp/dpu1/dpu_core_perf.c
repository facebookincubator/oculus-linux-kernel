/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/sort.h>
#include <linux/clk.h>
#include <linux/bitmap.h>

#include "dpu_kms.h"
#include "dpu_trace.h"
#include "dpu_crtc.h"
#include "dpu_core_perf.h"

#define DPU_PERF_MODE_STRING_SIZE	128

/**
 * enum dpu_perf_mode - performance tuning mode
 * @DPU_PERF_MODE_NORMAL: performance controlled by user mode client
 * @DPU_PERF_MODE_MINIMUM: performance bounded by minimum setting
 * @DPU_PERF_MODE_FIXED: performance bounded by fixed setting
 */
enum dpu_perf_mode {
	DPU_PERF_MODE_NORMAL,
	DPU_PERF_MODE_MINIMUM,
	DPU_PERF_MODE_FIXED,
	DPU_PERF_MODE_MAX
};

static struct dpu_kms *_dpu_crtc_get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv;

	if (!crtc->dev || !crtc->dev->dev_private) {
		DPU_ERROR("invalid device\n");
		return NULL;
	}

	priv = crtc->dev->dev_private;
	if (!priv || !priv->kms) {
		DPU_ERROR("invalid kms\n");
		return NULL;
	}

	return to_dpu_kms(priv->kms);
}

static bool _dpu_core_perf_crtc_is_power_on(struct drm_crtc *crtc)
{
	return dpu_crtc_is_enabled(crtc);
}

static bool _dpu_core_video_mode_intf_connected(struct drm_crtc *crtc)
{
	struct drm_crtc *tmp_crtc;
	bool intf_connected = false;

	if (!crtc)
		goto end;

	drm_for_each_crtc(tmp_crtc, crtc->dev) {
		if ((dpu_crtc_get_intf_mode(tmp_crtc) == INTF_MODE_VIDEO) &&
				_dpu_core_perf_crtc_is_power_on(tmp_crtc)) {
			DPU_DEBUG("video interface connected crtc:%d\n",
				tmp_crtc->base.id);
			intf_connected = true;
			goto end;
		}
	}

end:
	return intf_connected;
}

static void _dpu_core_perf_calc_crtc(struct dpu_kms *kms,
		struct drm_crtc *crtc,
		struct drm_crtc_state *state,
		struct dpu_core_perf_params *perf)
{
	struct dpu_crtc_state *dpu_cstate;
	int i;

	if (!kms || !kms->catalog || !crtc || !state || !perf) {
		DPU_ERROR("invalid parameters\n");
		return;
	}

	dpu_cstate = to_dpu_crtc_state(state);
	memset(perf, 0, sizeof(struct dpu_core_perf_params));

	if (!dpu_cstate->bw_control) {
		for (i = 0; i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
			perf->bw_ctl[i] = kms->catalog->perf.max_bw_high *
					1000ULL;
			perf->max_per_pipe_ib[i] = perf->bw_ctl[i];
		}
		perf->core_clk_rate = kms->perf.max_core_clk_rate;
	} else if (kms->perf.perf_tune.mode == DPU_PERF_MODE_MINIMUM) {
		for (i = 0; i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
			perf->bw_ctl[i] = 0;
			perf->max_per_pipe_ib[i] = 0;
		}
		perf->core_clk_rate = 0;
	} else if (kms->perf.perf_tune.mode == DPU_PERF_MODE_FIXED) {
		for (i = 0; i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
			perf->bw_ctl[i] = kms->perf.fix_core_ab_vote;
			perf->max_per_pipe_ib[i] = kms->perf.fix_core_ib_vote;
		}
		perf->core_clk_rate = kms->perf.fix_core_clk_rate;
	}

	DPU_DEBUG(
		"crtc=%d clk_rate=%llu core_ib=%llu core_ab=%llu llcc_ib=%llu llcc_ab=%llu mem_ib=%llu mem_ab=%llu\n",
			crtc->base.id, perf->core_clk_rate,
			perf->max_per_pipe_ib[DPU_POWER_HANDLE_DBUS_ID_MNOC],
			perf->bw_ctl[DPU_POWER_HANDLE_DBUS_ID_MNOC],
			perf->max_per_pipe_ib[DPU_POWER_HANDLE_DBUS_ID_LLCC],
			perf->bw_ctl[DPU_POWER_HANDLE_DBUS_ID_LLCC],
			perf->max_per_pipe_ib[DPU_POWER_HANDLE_DBUS_ID_EBI],
			perf->bw_ctl[DPU_POWER_HANDLE_DBUS_ID_EBI]);
}

int dpu_core_perf_crtc_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	u32 bw, threshold;
	u64 bw_sum_of_intfs = 0;
	enum dpu_crtc_client_type curr_client_type;
	bool is_video_mode;
	struct dpu_crtc_state *dpu_cstate;
	struct drm_crtc *tmp_crtc;
	struct dpu_kms *kms;
	int i;

	if (!crtc || !state) {
		DPU_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	kms = _dpu_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		DPU_ERROR("invalid parameters\n");
		return 0;
	}

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (dpu_crtc_get_client_type(crtc) == NRT_CLIENT)
		return 0;

	dpu_cstate = to_dpu_crtc_state(state);

	/* obtain new values */
	_dpu_core_perf_calc_crtc(kms, crtc, state, &dpu_cstate->new_perf);

	for (i = DPU_POWER_HANDLE_DBUS_ID_MNOC;
			i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
		bw_sum_of_intfs = dpu_cstate->new_perf.bw_ctl[i];
		curr_client_type = dpu_crtc_get_client_type(crtc);

		drm_for_each_crtc(tmp_crtc, crtc->dev) {
			if (_dpu_core_perf_crtc_is_power_on(tmp_crtc) &&
			    (dpu_crtc_get_client_type(tmp_crtc) ==
					    curr_client_type) &&
			    (tmp_crtc != crtc)) {
				struct dpu_crtc_state *tmp_cstate =
					to_dpu_crtc_state(tmp_crtc->state);

				DPU_DEBUG("crtc:%d bw:%llu ctrl:%d\n",
					tmp_crtc->base.id,
					tmp_cstate->new_perf.bw_ctl[i],
					tmp_cstate->bw_control);
				/*
				 * For bw check only use the bw if the
				 * atomic property has been already set
				 */
				if (tmp_cstate->bw_control)
					bw_sum_of_intfs +=
						tmp_cstate->new_perf.bw_ctl[i];
			}
		}

		/* convert bandwidth to kb */
		bw = DIV_ROUND_UP_ULL(bw_sum_of_intfs, 1000);
		DPU_DEBUG("calculated bandwidth=%uk\n", bw);

		is_video_mode = dpu_crtc_get_intf_mode(crtc) == INTF_MODE_VIDEO;
		threshold = (is_video_mode ||
			_dpu_core_video_mode_intf_connected(crtc)) ?
			kms->catalog->perf.max_bw_low :
			kms->catalog->perf.max_bw_high;

		DPU_DEBUG("final threshold bw limit = %d\n", threshold);

		if (!dpu_cstate->bw_control) {
			DPU_DEBUG("bypass bandwidth check\n");
		} else if (!threshold) {
			DPU_ERROR("no bandwidth limits specified\n");
			return -E2BIG;
		} else if (bw > threshold) {
			DPU_ERROR("exceeds bandwidth: %ukb > %ukb\n", bw,
					threshold);
			return -E2BIG;
		}
	}

	return 0;
}

static int _dpu_core_perf_crtc_update_bus(struct dpu_kms *kms,
		struct drm_crtc *crtc, u32 bus_id)
{
	struct dpu_core_perf_params perf = { { 0 } };
	enum dpu_crtc_client_type curr_client_type
					= dpu_crtc_get_client_type(crtc);
	struct drm_crtc *tmp_crtc;
	struct dpu_crtc_state *dpu_cstate;
	int ret = 0;

	drm_for_each_crtc(tmp_crtc, crtc->dev) {
		if (_dpu_core_perf_crtc_is_power_on(tmp_crtc) &&
			curr_client_type ==
				dpu_crtc_get_client_type(tmp_crtc)) {
			dpu_cstate = to_dpu_crtc_state(tmp_crtc->state);

			perf.max_per_pipe_ib[bus_id] =
				max(perf.max_per_pipe_ib[bus_id],
				dpu_cstate->new_perf.max_per_pipe_ib[bus_id]);

			DPU_DEBUG("crtc=%d bus_id=%d bw=%llu\n",
				tmp_crtc->base.id, bus_id,
				dpu_cstate->new_perf.bw_ctl[bus_id]);
		}
	}
	return ret;
}

/**
 * @dpu_core_perf_crtc_release_bw() - request zero bandwidth
 * @crtc - pointer to a crtc
 *
 * Function checks a state variable for the crtc, if all pending commit
 * requests are done, meaning no more bandwidth is needed, release
 * bandwidth request.
 */
void dpu_core_perf_crtc_release_bw(struct drm_crtc *crtc)
{
	struct drm_crtc *tmp_crtc;
	struct dpu_crtc *dpu_crtc;
	struct dpu_crtc_state *dpu_cstate;
	struct dpu_kms *kms;
	int i;

	if (!crtc) {
		DPU_ERROR("invalid crtc\n");
		return;
	}

	kms = _dpu_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		DPU_ERROR("invalid kms\n");
		return;
	}

	dpu_crtc = to_dpu_crtc(crtc);
	dpu_cstate = to_dpu_crtc_state(crtc->state);

	/* only do this for command mode rt client */
	if (dpu_crtc_get_intf_mode(crtc) != INTF_MODE_CMD)
		return;

	/*
	 * If video interface present, cmd panel bandwidth cannot be
	 * released.
	 */
	if (dpu_crtc_get_intf_mode(crtc) == INTF_MODE_CMD)
		drm_for_each_crtc(tmp_crtc, crtc->dev) {
			if (_dpu_core_perf_crtc_is_power_on(tmp_crtc) &&
				dpu_crtc_get_intf_mode(tmp_crtc) ==
						INTF_MODE_VIDEO)
				return;
		}

	/* Release the bandwidth */
	if (kms->perf.enable_bw_release) {
		trace_dpu_cmd_release_bw(crtc->base.id);
		DPU_DEBUG("Release BW crtc=%d\n", crtc->base.id);
		for (i = 0; i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
			dpu_crtc->cur_perf.bw_ctl[i] = 0;
			_dpu_core_perf_crtc_update_bus(kms, crtc, i);
		}
	}
}

static int _dpu_core_perf_set_core_clk_rate(struct dpu_kms *kms, u64 rate)
{
	struct dss_clk *core_clk = kms->perf.core_clk;

	if (core_clk->max_rate && (rate > core_clk->max_rate))
		rate = core_clk->max_rate;

	core_clk->rate = rate;
	return msm_dss_clk_set_rate(core_clk, 1);
}

static u64 _dpu_core_perf_get_core_clk_rate(struct dpu_kms *kms)
{
	u64 clk_rate = kms->perf.perf_tune.min_core_clk;
	struct drm_crtc *crtc;
	struct dpu_crtc_state *dpu_cstate;

	drm_for_each_crtc(crtc, kms->dev) {
		if (_dpu_core_perf_crtc_is_power_on(crtc)) {
			dpu_cstate = to_dpu_crtc_state(crtc->state);
			clk_rate = max(dpu_cstate->new_perf.core_clk_rate,
							clk_rate);
			clk_rate = clk_round_rate(kms->perf.core_clk->clk,
					clk_rate);
		}
	}

	if (kms->perf.perf_tune.mode == DPU_PERF_MODE_FIXED)
		clk_rate = kms->perf.fix_core_clk_rate;

	DPU_DEBUG("clk:%llu\n", clk_rate);

	return clk_rate;
}

int dpu_core_perf_crtc_update(struct drm_crtc *crtc,
		int params_changed, bool stop_req)
{
	struct dpu_core_perf_params *new, *old;
	int update_bus = 0, update_clk = 0;
	u64 clk_rate = 0;
	struct dpu_crtc *dpu_crtc;
	struct dpu_crtc_state *dpu_cstate;
	int i;
	struct msm_drm_private *priv;
	struct dpu_kms *kms;
	int ret;

	if (!crtc) {
		DPU_ERROR("invalid crtc\n");
		return -EINVAL;
	}

	kms = _dpu_crtc_get_kms(crtc);
	if (!kms || !kms->catalog) {
		DPU_ERROR("invalid kms\n");
		return -EINVAL;
	}
	priv = kms->dev->dev_private;

	dpu_crtc = to_dpu_crtc(crtc);
	dpu_cstate = to_dpu_crtc_state(crtc->state);

	DPU_DEBUG("crtc:%d stop_req:%d core_clk:%llu\n",
			crtc->base.id, stop_req, kms->perf.core_clk_rate);

	old = &dpu_crtc->cur_perf;
	new = &dpu_cstate->new_perf;

	if (_dpu_core_perf_crtc_is_power_on(crtc) && !stop_req) {
		for (i = 0; i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
			/*
			 * cases for bus bandwidth update.
			 * 1. new bandwidth vote - "ab or ib vote" is higher
			 *    than current vote for update request.
			 * 2. new bandwidth vote - "ab or ib vote" is lower
			 *    than current vote at end of commit or stop.
			 */
			if ((params_changed && ((new->bw_ctl[i] >
						old->bw_ctl[i]) ||
				  (new->max_per_pipe_ib[i] >
						old->max_per_pipe_ib[i]))) ||
			    (!params_changed && ((new->bw_ctl[i] <
						old->bw_ctl[i]) ||
				  (new->max_per_pipe_ib[i] <
						old->max_per_pipe_ib[i])))) {
				DPU_DEBUG(
					"crtc=%d p=%d new_bw=%llu,old_bw=%llu\n",
					crtc->base.id, params_changed,
					new->bw_ctl[i], old->bw_ctl[i]);
				old->bw_ctl[i] = new->bw_ctl[i];
				old->max_per_pipe_ib[i] =
						new->max_per_pipe_ib[i];
				update_bus |= BIT(i);
			}
		}

		if ((params_changed &&
				(new->core_clk_rate > old->core_clk_rate)) ||
				(!params_changed &&
				(new->core_clk_rate < old->core_clk_rate))) {
			old->core_clk_rate = new->core_clk_rate;
			update_clk = 1;
		}
	} else {
		DPU_DEBUG("crtc=%d disable\n", crtc->base.id);
		memset(old, 0, sizeof(*old));
		memset(new, 0, sizeof(*new));
		update_bus = ~0;
		update_clk = 1;
	}
	trace_dpu_perf_crtc_update(crtc->base.id,
				new->bw_ctl[DPU_POWER_HANDLE_DBUS_ID_MNOC],
				new->bw_ctl[DPU_POWER_HANDLE_DBUS_ID_LLCC],
				new->bw_ctl[DPU_POWER_HANDLE_DBUS_ID_EBI],
				new->core_clk_rate, stop_req,
				update_bus, update_clk);

	for (i = 0; i < DPU_POWER_HANDLE_DBUS_ID_MAX; i++) {
		if (update_bus & BIT(i)) {
			ret = _dpu_core_perf_crtc_update_bus(kms, crtc, i);
			if (ret) {
				DPU_ERROR("crtc-%d: failed to update bw vote for bus-%d\n",
					  crtc->base.id, i);
				return ret;
			}
		}
	}

	/*
	 * Update the clock after bandwidth vote to ensure
	 * bandwidth is available before clock rate is increased.
	 */
	if (update_clk) {
		clk_rate = _dpu_core_perf_get_core_clk_rate(kms);

		trace_dpu_core_perf_update_clk(kms->dev, stop_req, clk_rate);

		ret = _dpu_core_perf_set_core_clk_rate(kms, clk_rate);
		if (ret) {
			DPU_ERROR("failed to set %s clock rate %llu\n",
					kms->perf.core_clk->clk_name, clk_rate);
			return ret;
		}

		kms->perf.core_clk_rate = clk_rate;
		DPU_DEBUG("update clk rate = %lld HZ\n", clk_rate);
	}
	return 0;
}

#ifdef CONFIG_DEBUG_FS

static ssize_t _dpu_core_perf_mode_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct dpu_core_perf *perf = file->private_data;
	struct dpu_perf_cfg *cfg = &perf->catalog->perf;
	u32 perf_mode = 0;
	char buf[10];

	if (!perf)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (kstrtouint(buf, 0, &perf_mode))
		return -EFAULT;

	if (perf_mode >= DPU_PERF_MODE_MAX)
		return -EFAULT;

	if (perf_mode == DPU_PERF_MODE_FIXED) {
		DRM_INFO("fix performance mode\n");
	} else if (perf_mode == DPU_PERF_MODE_MINIMUM) {
		/* run the driver with max clk and BW vote */
		perf->perf_tune.min_core_clk = perf->max_core_clk_rate;
		perf->perf_tune.min_bus_vote =
				(u64) cfg->max_bw_high * 1000;
		DRM_INFO("minimum performance mode\n");
	} else if (perf_mode == DPU_PERF_MODE_NORMAL) {
		/* reset the perf tune params to 0 */
		perf->perf_tune.min_core_clk = 0;
		perf->perf_tune.min_bus_vote = 0;
		DRM_INFO("normal performance mode\n");
	}
	perf->perf_tune.mode = perf_mode;

	return count;
}

static ssize_t _dpu_core_perf_mode_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct dpu_core_perf *perf = file->private_data;
	int len = 0;
	char buf[DPU_PERF_MODE_STRING_SIZE] = {'\0'};

	if (!perf)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf),
			"mode %d min_mdp_clk %llu min_bus_vote %llu\n",
			perf->perf_tune.mode,
			perf->perf_tune.min_core_clk,
			perf->perf_tune.min_bus_vote);
	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;   /* increase offset */

	return len;
}

static const struct file_operations dpu_core_perf_mode_fops = {
	.open = simple_open,
	.read = _dpu_core_perf_mode_read,
	.write = _dpu_core_perf_mode_write,
};

static void dpu_core_perf_debugfs_destroy(struct dpu_core_perf *perf)
{
	debugfs_remove_recursive(perf->debugfs_root);
	perf->debugfs_root = NULL;
}

int dpu_core_perf_debugfs_init(struct dpu_core_perf *perf,
		struct dentry *parent)
{
	struct dpu_mdss_cfg *catalog = perf->catalog;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;

	priv = perf->dev->dev_private;
	if (!priv || !priv->kms) {
		DPU_ERROR("invalid KMS reference\n");
		return -EINVAL;
	}

	dpu_kms = to_dpu_kms(priv->kms);

	perf->debugfs_root = debugfs_create_dir("core_perf", parent);
	if (!perf->debugfs_root) {
		DPU_ERROR("failed to create core perf debugfs\n");
		return -EINVAL;
	}

	debugfs_create_u64("max_core_clk_rate", 0600, perf->debugfs_root,
			&perf->max_core_clk_rate);
	debugfs_create_u64("core_clk_rate", 0600, perf->debugfs_root,
			&perf->core_clk_rate);
	debugfs_create_u32("enable_bw_release", 0600, perf->debugfs_root,
			(u32 *)&perf->enable_bw_release);
	debugfs_create_u32("threshold_low", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.max_bw_low);
	debugfs_create_u32("threshold_high", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.max_bw_high);
	debugfs_create_u32("min_core_ib", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.min_core_ib);
	debugfs_create_u32("min_llcc_ib", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.min_llcc_ib);
	debugfs_create_u32("min_dram_ib", 0600, perf->debugfs_root,
			(u32 *)&catalog->perf.min_dram_ib);
	debugfs_create_file("perf_mode", 0600, perf->debugfs_root,
			(u32 *)perf, &dpu_core_perf_mode_fops);
	debugfs_create_u64("fix_core_clk_rate", 0600, perf->debugfs_root,
			&perf->fix_core_clk_rate);
	debugfs_create_u64("fix_core_ib_vote", 0600, perf->debugfs_root,
			&perf->fix_core_ib_vote);
	debugfs_create_u64("fix_core_ab_vote", 0600, perf->debugfs_root,
			&perf->fix_core_ab_vote);

	return 0;
}
#else
static void dpu_core_perf_debugfs_destroy(struct dpu_core_perf *perf)
{
}

int dpu_core_perf_debugfs_init(struct dpu_core_perf *perf,
		struct dentry *parent)
{
	return 0;
}
#endif

void dpu_core_perf_destroy(struct dpu_core_perf *perf)
{
	if (!perf) {
		DPU_ERROR("invalid parameters\n");
		return;
	}

	dpu_core_perf_debugfs_destroy(perf);
	perf->max_core_clk_rate = 0;
	perf->core_clk = NULL;
	perf->phandle = NULL;
	perf->catalog = NULL;
	perf->dev = NULL;
}

int dpu_core_perf_init(struct dpu_core_perf *perf,
		struct drm_device *dev,
		struct dpu_mdss_cfg *catalog,
		struct dpu_power_handle *phandle,
		struct dss_clk *core_clk)
{
	perf->dev = dev;
	perf->catalog = catalog;
	perf->phandle = phandle;
	perf->core_clk = core_clk;

	perf->max_core_clk_rate = core_clk->max_rate;
	if (!perf->max_core_clk_rate) {
		DPU_DEBUG("optional max core clk rate, use default\n");
		perf->max_core_clk_rate = DPU_PERF_DEFAULT_MAX_CORE_CLK_RATE;
	}

	return 0;
}

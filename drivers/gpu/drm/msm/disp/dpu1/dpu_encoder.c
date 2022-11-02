/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "msm_drv.h"
#include "dpu_kms.h"
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "dpu_hwio.h"
#include "dpu_hw_catalog.h"
#include "dpu_hw_intf.h"
#include "dpu_hw_ctl.h"
#include "dpu_formats.h"
#include "dpu_encoder_phys.h"
#include "dpu_crtc.h"
#include "dpu_trace.h"
#include "dpu_core_irq.h"

#define DPU_DEBUG_ENC(e, fmt, ...) DPU_DEBUG("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

#define DPU_ERROR_ENC(e, fmt, ...) DPU_ERROR("enc%d " fmt,\
		(e) ? (e)->base.base.id : -1, ##__VA_ARGS__)

#define DPU_DEBUG_PHYS(p, fmt, ...) DPU_DEBUG("enc%d intf%d pp%d " fmt,\
		(p) ? (p)->parent->base.id : -1, \
		(p) ? (p)->intf_idx - INTF_0 : -1, \
		(p) ? ((p)->hw_pp ? (p)->hw_pp->idx - PINGPONG_0 : -1) : -1, \
		##__VA_ARGS__)

#define DPU_ERROR_PHYS(p, fmt, ...) DPU_ERROR("enc%d intf%d pp%d " fmt,\
		(p) ? (p)->parent->base.id : -1, \
		(p) ? (p)->intf_idx - INTF_0 : -1, \
		(p) ? ((p)->hw_pp ? (p)->hw_pp->idx - PINGPONG_0 : -1) : -1, \
		##__VA_ARGS__)

/*
 * Two to anticipate panels that can do cmd/vid dynamic switching
 * plan is to create all possible physical encoder types, and switch between
 * them at runtime
 */
#define NUM_PHYS_ENCODER_TYPES 2

#define MAX_PHYS_ENCODERS_PER_VIRTUAL \
	(MAX_H_TILES_PER_DISPLAY * NUM_PHYS_ENCODER_TYPES)

#define MAX_CHANNELS_PER_ENC 2

#define MISR_BUFF_SIZE			256

#define IDLE_SHORT_TIMEOUT	1

#define MAX_VDISPLAY_SPLIT 1080

/**
 * enum dpu_enc_rc_events - events for resource control state machine
 * @DPU_ENC_RC_EVENT_KICKOFF:
 *	This event happens at NORMAL priority.
 *	Event that signals the start of the transfer. When this event is
 *	received, enable MDP/DSI core clocks. Regardless of the previous
 *	state, the resource should be in ON state at the end of this event.
 * @DPU_ENC_RC_EVENT_FRAME_DONE:
 *	This event happens at INTERRUPT level.
 *	Event signals the end of the data transfer after the PP FRAME_DONE
 *	event. At the end of this event, a delayed work is scheduled to go to
 *	IDLE_PC state after IDLE_TIMEOUT time.
 * @DPU_ENC_RC_EVENT_PRE_STOP:
 *	This event happens at NORMAL priority.
 *	This event, when received during the ON state, leave the RC STATE
 *	in the PRE_OFF state. It should be followed by the STOP event as
 *	part of encoder disable.
 *	If received during IDLE or OFF states, it will do nothing.
 * @DPU_ENC_RC_EVENT_STOP:
 *	This event happens at NORMAL priority.
 *	When this event is received, disable all the MDP/DSI core clocks, and
 *	disable IRQs. It should be called from the PRE_OFF or IDLE states.
 *	IDLE is expected when IDLE_PC has run, and PRE_OFF did nothing.
 *	PRE_OFF is expected when PRE_STOP was executed during the ON state.
 *	Resource state should be in OFF at the end of the event.
 * @DPU_ENC_RC_EVENT_ENTER_IDLE:
 *	This event happens at NORMAL priority from a work item.
 *	Event signals that there were no frame updates for IDLE_TIMEOUT time.
 *	This would disable MDP/DSI core clocks and change the resource state
 *	to IDLE.
 */
enum dpu_enc_rc_events {
	DPU_ENC_RC_EVENT_KICKOFF = 1,
	DPU_ENC_RC_EVENT_FRAME_DONE,
	DPU_ENC_RC_EVENT_PRE_STOP,
	DPU_ENC_RC_EVENT_STOP,
	DPU_ENC_RC_EVENT_ENTER_IDLE
};

/*
 * enum dpu_enc_rc_states - states that the resource control maintains
 * @DPU_ENC_RC_STATE_OFF: Resource is in OFF state
 * @DPU_ENC_RC_STATE_PRE_OFF: Resource is transitioning to OFF state
 * @DPU_ENC_RC_STATE_ON: Resource is in ON state
 * @DPU_ENC_RC_STATE_MODESET: Resource is in modeset state
 * @DPU_ENC_RC_STATE_IDLE: Resource is in IDLE state
 */
enum dpu_enc_rc_states {
	DPU_ENC_RC_STATE_OFF,
	DPU_ENC_RC_STATE_PRE_OFF,
	DPU_ENC_RC_STATE_ON,
	DPU_ENC_RC_STATE_IDLE
};

/**
 * struct dpu_encoder_virt - virtual encoder. Container of one or more physical
 *	encoders. Virtual encoder manages one "logical" display. Physical
 *	encoders manage one intf block, tied to a specific panel/sub-panel.
 *	Virtual encoder defers as much as possible to the physical encoders.
 *	Virtual encoder registers itself with the DRM Framework as the encoder.
 * @base:		drm_encoder base class for registration with DRM
 * @enc_spin_lock:	Virtual-Encoder-Wide Spin Lock for IRQ purposes
 * @bus_scaling_client:	Client handle to the bus scaling interface
 * @num_phys_encs:	Actual number of physical encoders contained.
 * @phys_encs:		Container of physical encoders managed.
 * @cur_master:		Pointer to the current master in this mode. Optimization
 *			Only valid after enable. Cleared as disable.
 * @hw_pp		Handle to the pingpong blocks used for the display. No.
 *			pingpong blocks can be different than num_phys_encs.
 * @intfs_swapped	Whether or not the phys_enc interfaces have been swapped
 *			for partial update right-only cases, such as pingpong
 *			split where virtual pingpong does not generate IRQs
 * @crtc_vblank_cb:	Callback into the upper layer / CRTC for
 *			notification of the VBLANK
 * @crtc_vblank_cb_data:	Data from upper layer for VBLANK notification
 * @crtc_kickoff_cb:		Callback into CRTC that will flush & start
 *				all CTL paths
 * @crtc_kickoff_cb_data:	Opaque user data given to crtc_kickoff_cb
 * @debugfs_root:		Debug file system root file node
 * @enc_lock:			Lock around physical encoder create/destroy and
				access.
 * @frame_busy_mask:		Bitmask tracking which phys_enc we are still
 *				busy processing current command.
 *				Bit0 = phys_encs[0] etc.
 * @crtc_frame_event_cb:	callback handler for frame event
 * @crtc_frame_event_cb_data:	callback handler private data
 * @frame_done_timeout:		frame done timeout in Hz
 * @frame_done_timer:		watchdog timer for frame done event
 * @vsync_event_timer:		vsync timer
 * @disp_info:			local copy of msm_display_info struct
 * @misr_enable:		misr enable/disable status
 * @misr_frame_count:		misr frame count before start capturing the data
 * @idle_pc_supported:		indicate if idle power collaps is supported
 * @rc_lock:			resource control mutex lock to protect
 *				virt encoder over various state changes
 * @rc_state:			resource controller state
 * @delayed_off_work:		delayed worker to schedule disabling of
 *				clks and resources after IDLE_TIMEOUT time.
 * @vsync_event_work:		worker to handle vsync event for autorefresh
 * @topology:                   topology of the display
 * @mode_set_complete:          flag to indicate modeset completion
 * @idle_timeout:		idle timeout duration in milliseconds
 */
struct dpu_encoder_virt {
	struct drm_encoder base;
	spinlock_t enc_spinlock;
	uint32_t bus_scaling_client;

	uint32_t display_num_of_h_tiles;

	unsigned int num_phys_encs;
	struct dpu_encoder_phys *phys_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct dpu_encoder_phys *cur_master;
	struct dpu_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];

	bool intfs_swapped;

	void (*crtc_vblank_cb)(void *);
	void *crtc_vblank_cb_data;

	struct dentry *debugfs_root;
	struct mutex enc_lock;
	DECLARE_BITMAP(frame_busy_mask, MAX_PHYS_ENCODERS_PER_VIRTUAL);
	void (*crtc_frame_event_cb)(void *, u32 event);
	void *crtc_frame_event_cb_data;

	atomic_t frame_done_timeout;
	struct timer_list frame_done_timer;
	struct timer_list vsync_event_timer;

	struct msm_display_info disp_info;
	bool misr_enable;
	u32 misr_frame_count;

	bool idle_pc_supported;
	struct mutex rc_lock;
	enum dpu_enc_rc_states rc_state;
	struct kthread_delayed_work delayed_off_work;
	struct kthread_work vsync_event_work;
	struct msm_display_topology topology;
	bool mode_set_complete;

	u32 idle_timeout;
};

#define to_dpu_encoder_virt(x) container_of(x, struct dpu_encoder_virt, base)
static inline int _dpu_encoder_power_enable(struct dpu_encoder_virt *dpu_enc,
								bool enable)
{
	struct drm_encoder *drm_enc;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;

	if (!dpu_enc) {
		DPU_ERROR("invalid dpu enc\n");
		return -EINVAL;
	}

	drm_enc = &dpu_enc->base;
	if (!drm_enc->dev || !drm_enc->dev->dev_private) {
		DPU_ERROR("drm device invalid\n");
		return -EINVAL;
	}

	priv = drm_enc->dev->dev_private;
	if (!priv->kms) {
		DPU_ERROR("invalid kms\n");
		return -EINVAL;
	}

	dpu_kms = to_dpu_kms(priv->kms);

	if (enable)
		pm_runtime_get_sync(&dpu_kms->pdev->dev);
	else
		pm_runtime_put_sync(&dpu_kms->pdev->dev);

	return 0;
}

void dpu_encoder_helper_report_irq_timeout(struct dpu_encoder_phys *phys_enc,
		enum dpu_intr_idx intr_idx)
{
	DRM_ERROR("irq timeout id=%u, intf=%d, pp=%d, intr=%d\n",
		  DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
		  phys_enc->hw_pp->idx - PINGPONG_0, intr_idx);

	if (phys_enc->parent_ops->handle_frame_done)
		phys_enc->parent_ops->handle_frame_done(
				phys_enc->parent, phys_enc,
				DPU_ENCODER_FRAME_EVENT_ERROR);
}

static int dpu_encoder_helper_wait_event_timeout(int32_t drm_id,
		int32_t hw_id, struct dpu_encoder_wait_info *info);

int dpu_encoder_helper_wait_for_irq(struct dpu_encoder_phys *phys_enc,
		enum dpu_intr_idx intr_idx,
		struct dpu_encoder_wait_info *wait_info)
{
	struct dpu_encoder_irq *irq;
	u32 irq_status;
	int ret;

	if (!phys_enc || !wait_info || intr_idx >= INTR_IDX_MAX) {
		DPU_ERROR("invalid params\n");
		return -EINVAL;
	}
	irq = &phys_enc->irq[intr_idx];

	/* note: do master / slave checking outside */

	/* return EWOULDBLOCK since we know the wait isn't necessary */
	if (phys_enc->enable_state == DPU_ENC_DISABLED) {
		DRM_ERROR("encoder is disabled id=%u, intr=%d, hw=%d, irq=%d",
			  DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			  irq->irq_idx);
		return -EWOULDBLOCK;
	}

	if (irq->irq_idx < 0) {
		DRM_DEBUG_KMS("skip irq wait id=%u, intr=%d, hw=%d, irq=%s",
			      DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			      irq->name);
		return 0;
	}

	DRM_DEBUG_KMS("id=%u, intr=%d, hw=%d, irq=%d, pp=%d, pending_cnt=%d",
		      DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
		      irq->irq_idx, phys_enc->hw_pp->idx - PINGPONG_0,
		      atomic_read(wait_info->atomic_cnt));

	ret = dpu_encoder_helper_wait_event_timeout(
			DRMID(phys_enc->parent),
			irq->hw_idx,
			wait_info);

	if (ret <= 0) {
		irq_status = dpu_core_irq_read(phys_enc->dpu_kms,
				irq->irq_idx, true);
		if (irq_status) {
			unsigned long flags;

			DRM_DEBUG_KMS("irq not triggered id=%u, intr=%d, "
				      "hw=%d, irq=%d, pp=%d, atomic_cnt=%d",
				      DRMID(phys_enc->parent), intr_idx,
				      irq->hw_idx, irq->irq_idx,
				      phys_enc->hw_pp->idx - PINGPONG_0,
				      atomic_read(wait_info->atomic_cnt));
			local_irq_save(flags);
			irq->cb.func(phys_enc, irq->irq_idx);
			local_irq_restore(flags);
			ret = 0;
		} else {
			ret = -ETIMEDOUT;
			DRM_DEBUG_KMS("irq timeout id=%u, intr=%d, "
				      "hw=%d, irq=%d, pp=%d, atomic_cnt=%d",
				      DRMID(phys_enc->parent), intr_idx,
				      irq->hw_idx, irq->irq_idx,
				      phys_enc->hw_pp->idx - PINGPONG_0,
				      atomic_read(wait_info->atomic_cnt));
		}
	} else {
		ret = 0;
		trace_dpu_enc_irq_wait_success(DRMID(phys_enc->parent),
			intr_idx, irq->hw_idx, irq->irq_idx,
			phys_enc->hw_pp->idx - PINGPONG_0,
			atomic_read(wait_info->atomic_cnt));
	}

	return ret;
}

int dpu_encoder_helper_register_irq(struct dpu_encoder_phys *phys_enc,
		enum dpu_intr_idx intr_idx)
{
	struct dpu_encoder_irq *irq;
	int ret = 0;

	if (!phys_enc || intr_idx >= INTR_IDX_MAX) {
		DPU_ERROR("invalid params\n");
		return -EINVAL;
	}
	irq = &phys_enc->irq[intr_idx];

	if (irq->irq_idx >= 0) {
		DPU_DEBUG_PHYS(phys_enc,
				"skipping already registered irq %s type %d\n",
				irq->name, irq->intr_type);
		return 0;
	}

	irq->irq_idx = dpu_core_irq_idx_lookup(phys_enc->dpu_kms,
			irq->intr_type, irq->hw_idx);
	if (irq->irq_idx < 0) {
		DPU_ERROR_PHYS(phys_enc,
			"failed to lookup IRQ index for %s type:%d\n",
			irq->name, irq->intr_type);
		return -EINVAL;
	}

	ret = dpu_core_irq_register_callback(phys_enc->dpu_kms, irq->irq_idx,
			&irq->cb);
	if (ret) {
		DPU_ERROR_PHYS(phys_enc,
			"failed to register IRQ callback for %s\n",
			irq->name);
		irq->irq_idx = -EINVAL;
		return ret;
	}

	ret = dpu_core_irq_enable(phys_enc->dpu_kms, &irq->irq_idx, 1);
	if (ret) {
		DRM_ERROR("enable failed id=%u, intr=%d, hw=%d, irq=%d",
			  DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			  irq->irq_idx);
		dpu_core_irq_unregister_callback(phys_enc->dpu_kms,
				irq->irq_idx, &irq->cb);
		irq->irq_idx = -EINVAL;
		return ret;
	}

	trace_dpu_enc_irq_register_success(DRMID(phys_enc->parent), intr_idx,
				irq->hw_idx, irq->irq_idx);

	return ret;
}

int dpu_encoder_helper_unregister_irq(struct dpu_encoder_phys *phys_enc,
		enum dpu_intr_idx intr_idx)
{
	struct dpu_encoder_irq *irq;
	int ret;

	if (!phys_enc) {
		DPU_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	irq = &phys_enc->irq[intr_idx];

	/* silently skip irqs that weren't registered */
	if (irq->irq_idx < 0) {
		DRM_ERROR("duplicate unregister id=%u, intr=%d, hw=%d, irq=%d",
			  DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			  irq->irq_idx);
		return 0;
	}

	ret = dpu_core_irq_disable(phys_enc->dpu_kms, &irq->irq_idx, 1);
	if (ret) {
		DRM_ERROR("disable failed id=%u, intr=%d, hw=%d, irq=%d ret=%d",
			  DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			  irq->irq_idx, ret);
	}

	ret = dpu_core_irq_unregister_callback(phys_enc->dpu_kms, irq->irq_idx,
			&irq->cb);
	if (ret) {
		DRM_ERROR("unreg cb fail id=%u, intr=%d, hw=%d, irq=%d ret=%d",
			  DRMID(phys_enc->parent), intr_idx, irq->hw_idx,
			  irq->irq_idx, ret);
	}

	trace_dpu_enc_irq_unregister_success(DRMID(phys_enc->parent), intr_idx,
					     irq->hw_idx, irq->irq_idx);

	irq->irq_idx = -EINVAL;

	return 0;
}

void dpu_encoder_get_hw_resources(struct drm_encoder *drm_enc,
		struct dpu_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	int i = 0;

	if (!hw_res || !drm_enc || !conn_state) {
		DPU_ERROR("invalid argument(s), drm_enc %d, res %d, state %d\n",
				drm_enc != 0, hw_res != 0, conn_state != 0);
		return;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	DPU_DEBUG_ENC(dpu_enc, "\n");

	/* Query resources used by phys encs, expected to be without overlap */
	memset(hw_res, 0, sizeof(*hw_res));
	hw_res->display_num_of_h_tiles = dpu_enc->display_num_of_h_tiles;

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.get_hw_resources)
			phys->ops.get_hw_resources(phys, hw_res, conn_state);
	}
}

static void dpu_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	int i = 0;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	DPU_DEBUG_ENC(dpu_enc, "\n");

	mutex_lock(&dpu_enc->enc_lock);

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.destroy) {
			phys->ops.destroy(phys);
			--dpu_enc->num_phys_encs;
			dpu_enc->phys_encs[i] = NULL;
		}
	}

	if (dpu_enc->num_phys_encs)
		DPU_ERROR_ENC(dpu_enc, "expected 0 num_phys_encs not %d\n",
				dpu_enc->num_phys_encs);
	dpu_enc->num_phys_encs = 0;
	mutex_unlock(&dpu_enc->enc_lock);

	drm_encoder_cleanup(drm_enc);
	mutex_destroy(&dpu_enc->enc_lock);
}

void dpu_encoder_helper_split_config(
		struct dpu_encoder_phys *phys_enc,
		enum dpu_intf interface)
{
	struct dpu_encoder_virt *dpu_enc;
	struct split_pipe_cfg cfg = { 0 };
	struct dpu_hw_mdp *hw_mdptop;
	struct msm_display_info *disp_info;

	if (!phys_enc || !phys_enc->hw_mdptop || !phys_enc->parent) {
		DPU_ERROR("invalid arg(s), encoder %d\n", phys_enc != 0);
		return;
	}

	dpu_enc = to_dpu_encoder_virt(phys_enc->parent);
	hw_mdptop = phys_enc->hw_mdptop;
	disp_info = &dpu_enc->disp_info;

	if (disp_info->intf_type != DRM_MODE_CONNECTOR_DSI)
		return;

	/**
	 * disable split modes since encoder will be operating in as the only
	 * encoder, either for the entire use case in the case of, for example,
	 * single DSI, or for this frame in the case of left/right only partial
	 * update.
	 */
	if (phys_enc->split_role == ENC_ROLE_SOLO) {
		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
		return;
	}

	cfg.en = true;
	cfg.mode = phys_enc->intf_mode;
	cfg.intf = interface;

	if (cfg.en && phys_enc->ops.needs_single_flush &&
			phys_enc->ops.needs_single_flush(phys_enc))
		cfg.split_flush_en = true;

	if (phys_enc->split_role == ENC_ROLE_MASTER) {
		DPU_DEBUG_ENC(dpu_enc, "enable %d\n", cfg.en);

		if (hw_mdptop->ops.setup_split_pipe)
			hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
	}
}

static void _dpu_encoder_adjust_mode(struct drm_connector *connector,
		struct drm_display_mode *adj_mode)
{
	struct drm_display_mode *cur_mode;

	if (!connector || !adj_mode)
		return;

	list_for_each_entry(cur_mode, &connector->modes, head) {
		if (cur_mode->vdisplay == adj_mode->vdisplay &&
			cur_mode->hdisplay == adj_mode->hdisplay &&
			cur_mode->vrefresh == adj_mode->vrefresh) {
			adj_mode->private = cur_mode->private;
			adj_mode->private_flags |= cur_mode->private_flags;
		}
	}
}

static struct msm_display_topology dpu_encoder_get_topology(
			struct dpu_encoder_virt *dpu_enc,
			struct dpu_kms *dpu_kms,
			struct drm_display_mode *mode)
{
	struct msm_display_topology topology;
	int i, intf_count = 0;

	for (i = 0; i < MAX_PHYS_ENCODERS_PER_VIRTUAL; i++)
		if (dpu_enc->phys_encs[i])
			intf_count++;

	/* User split topology for width > 1080 */
	topology.num_lm = (mode->vdisplay > MAX_VDISPLAY_SPLIT) ? 2 : 1;
	topology.num_enc = 0;
	topology.num_intf = intf_count;

	return topology;
}
static int dpu_encoder_virt_atomic_check(
		struct drm_encoder *drm_enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct dpu_encoder_virt *dpu_enc;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;
	const struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	struct msm_display_topology topology;
	int i = 0;
	int ret = 0;

	if (!drm_enc || !crtc_state || !conn_state) {
		DPU_ERROR("invalid arg(s), drm_enc %d, crtc/conn state %d/%d\n",
				drm_enc != 0, crtc_state != 0, conn_state != 0);
		return -EINVAL;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	DPU_DEBUG_ENC(dpu_enc, "\n");

	priv = drm_enc->dev->dev_private;
	dpu_kms = to_dpu_kms(priv->kms);
	mode = &crtc_state->mode;
	adj_mode = &crtc_state->adjusted_mode;
	trace_dpu_enc_atomic_check(DRMID(drm_enc));

	/*
	 * display drivers may populate private fields of the drm display mode
	 * structure while registering possible modes of a connector with DRM.
	 * These private fields are not populated back while DRM invokes
	 * the mode_set callbacks. This module retrieves and populates the
	 * private fields of the given mode.
	 */
	_dpu_encoder_adjust_mode(conn_state->connector, adj_mode);

	/* perform atomic check on the first physical encoder (master) */
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.atomic_check)
			ret = phys->ops.atomic_check(phys, crtc_state,
					conn_state);
		else if (phys && phys->ops.mode_fixup)
			if (!phys->ops.mode_fixup(phys, mode, adj_mode))
				ret = -EINVAL;

		if (ret) {
			DPU_ERROR_ENC(dpu_enc,
					"mode unsupported, phys idx %d\n", i);
			break;
		}
	}

	topology = dpu_encoder_get_topology(dpu_enc, dpu_kms, adj_mode);

	/* Reserve dynamic resources now. Indicating AtomicTest phase */
	if (!ret) {
		/*
		 * Avoid reserving resources when mode set is pending. Topology
		 * info may not be available to complete reservation.
		 */
		if (drm_atomic_crtc_needs_modeset(crtc_state)
				&& dpu_enc->mode_set_complete) {
			ret = dpu_rm_reserve(&dpu_kms->rm, drm_enc, crtc_state,
				conn_state, topology, true);
			dpu_enc->mode_set_complete = false;
		}
	}

	if (!ret)
		drm_mode_set_crtcinfo(adj_mode, 0);

	trace_dpu_enc_atomic_check_flags(DRMID(drm_enc), adj_mode->flags,
			adj_mode->private_flags);

	return ret;
}

static void _dpu_encoder_update_vsync_source(struct dpu_encoder_virt *dpu_enc,
			struct msm_display_info *disp_info)
{
	struct dpu_vsync_source_cfg vsync_cfg = { 0 };
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;
	struct dpu_hw_mdp *hw_mdptop;
	struct drm_encoder *drm_enc;
	int i;

	if (!dpu_enc || !disp_info) {
		DPU_ERROR("invalid param dpu_enc:%d or disp_info:%d\n",
					dpu_enc != NULL, disp_info != NULL);
		return;
	} else if (dpu_enc->num_phys_encs > ARRAY_SIZE(dpu_enc->hw_pp)) {
		DPU_ERROR("invalid num phys enc %d/%d\n",
				dpu_enc->num_phys_encs,
				(int) ARRAY_SIZE(dpu_enc->hw_pp));
		return;
	}

	drm_enc = &dpu_enc->base;
	/* this pointers are checked in virt_enable_helper */
	priv = drm_enc->dev->dev_private;

	dpu_kms = to_dpu_kms(priv->kms);
	if (!dpu_kms) {
		DPU_ERROR("invalid dpu_kms\n");
		return;
	}

	hw_mdptop = dpu_kms->hw_mdp;
	if (!hw_mdptop) {
		DPU_ERROR("invalid mdptop\n");
		return;
	}

	if (hw_mdptop->ops.setup_vsync_source &&
			disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE) {
		for (i = 0; i < dpu_enc->num_phys_encs; i++)
			vsync_cfg.ppnumber[i] = dpu_enc->hw_pp[i]->idx;

		vsync_cfg.pp_count = dpu_enc->num_phys_encs;
		if (disp_info->is_te_using_watchdog_timer)
			vsync_cfg.vsync_source = DPU_VSYNC_SOURCE_WD_TIMER_0;
		else
			vsync_cfg.vsync_source = DPU_VSYNC0_SOURCE_GPIO;

		hw_mdptop->ops.setup_vsync_source(hw_mdptop, &vsync_cfg);
	}
}

static void _dpu_encoder_irq_control(struct drm_encoder *drm_enc, bool enable)
{
	struct dpu_encoder_virt *dpu_enc;
	int i;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);

	DPU_DEBUG_ENC(dpu_enc, "enable:%d\n", enable);
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.irq_control)
			phys->ops.irq_control(phys, enable);
	}

}

static void _dpu_encoder_resource_control_helper(struct drm_encoder *drm_enc,
		bool enable)
{
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;
	struct dpu_encoder_virt *dpu_enc;

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	dpu_kms = to_dpu_kms(priv->kms);

	trace_dpu_enc_rc_helper(DRMID(drm_enc), enable);

	if (!dpu_enc->cur_master) {
		DPU_ERROR("encoder master not set\n");
		return;
	}

	if (enable) {
		/* enable DPU core clks */
		pm_runtime_get_sync(&dpu_kms->pdev->dev);

		/* enable all the irq */
		_dpu_encoder_irq_control(drm_enc, true);

	} else {
		/* disable all the irq */
		_dpu_encoder_irq_control(drm_enc, false);

		/* disable DPU core clks */
		pm_runtime_put_sync(&dpu_kms->pdev->dev);
	}

}

static int dpu_encoder_resource_control(struct drm_encoder *drm_enc,
		u32 sw_event)
{
	struct dpu_encoder_virt *dpu_enc;
	struct msm_drm_private *priv;
	struct msm_drm_thread *disp_thread;
	bool is_vid_mode = false;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private ||
			!drm_enc->crtc) {
		DPU_ERROR("invalid parameters\n");
		return -EINVAL;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	is_vid_mode = dpu_enc->disp_info.capabilities &
						MSM_DISPLAY_CAP_VID_MODE;

	if (drm_enc->crtc->index >= ARRAY_SIZE(priv->disp_thread)) {
		DPU_ERROR("invalid crtc index\n");
		return -EINVAL;
	}
	disp_thread = &priv->disp_thread[drm_enc->crtc->index];

	/*
	 * when idle_pc is not supported, process only KICKOFF, STOP and MODESET
	 * events and return early for other events (ie wb display).
	 */
	if (!dpu_enc->idle_pc_supported &&
			(sw_event != DPU_ENC_RC_EVENT_KICKOFF &&
			sw_event != DPU_ENC_RC_EVENT_STOP &&
			sw_event != DPU_ENC_RC_EVENT_PRE_STOP))
		return 0;

	trace_dpu_enc_rc(DRMID(drm_enc), sw_event, dpu_enc->idle_pc_supported,
			 dpu_enc->rc_state, "begin");

	switch (sw_event) {
	case DPU_ENC_RC_EVENT_KICKOFF:
		/* cancel delayed off work, if any */
		if (kthread_cancel_delayed_work_sync(
				&dpu_enc->delayed_off_work))
			DPU_DEBUG_ENC(dpu_enc, "sw_event:%d, work cancelled\n",
					sw_event);

		mutex_lock(&dpu_enc->rc_lock);

		/* return if the resource control is already in ON state */
		if (dpu_enc->rc_state == DPU_ENC_RC_STATE_ON) {
			DRM_DEBUG_KMS("id;%u, sw_event:%d, rc in ON state\n",
				      DRMID(drm_enc), sw_event);
			mutex_unlock(&dpu_enc->rc_lock);
			return 0;
		} else if (dpu_enc->rc_state != DPU_ENC_RC_STATE_OFF &&
				dpu_enc->rc_state != DPU_ENC_RC_STATE_IDLE) {
			DRM_DEBUG_KMS("id;%u, sw_event:%d, rc in state %d\n",
				      DRMID(drm_enc), sw_event,
				      dpu_enc->rc_state);
			mutex_unlock(&dpu_enc->rc_lock);
			return -EINVAL;
		}

		if (is_vid_mode && dpu_enc->rc_state == DPU_ENC_RC_STATE_IDLE)
			_dpu_encoder_irq_control(drm_enc, true);
		else
			_dpu_encoder_resource_control_helper(drm_enc, true);

		dpu_enc->rc_state = DPU_ENC_RC_STATE_ON;

		trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
				 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
				 "kickoff");

		mutex_unlock(&dpu_enc->rc_lock);
		break;

	case DPU_ENC_RC_EVENT_FRAME_DONE:
		/*
		 * mutex lock is not used as this event happens at interrupt
		 * context. And locking is not required as, the other events
		 * like KICKOFF and STOP does a wait-for-idle before executing
		 * the resource_control
		 */
		if (dpu_enc->rc_state != DPU_ENC_RC_STATE_ON) {
			DRM_DEBUG_KMS("id:%d, sw_event:%d,rc:%d-unexpected\n",
				      DRMID(drm_enc), sw_event,
				      dpu_enc->rc_state);
			return -EINVAL;
		}

		/*
		 * schedule off work item only when there are no
		 * frames pending
		 */
		if (dpu_crtc_frame_pending(drm_enc->crtc) > 1) {
			DRM_DEBUG_KMS("id:%d skip schedule work\n",
				      DRMID(drm_enc));
			return 0;
		}

		kthread_queue_delayed_work(
			&disp_thread->worker,
			&dpu_enc->delayed_off_work,
			msecs_to_jiffies(dpu_enc->idle_timeout));

		trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
				 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
				 "frame done");
		break;

	case DPU_ENC_RC_EVENT_PRE_STOP:
		/* cancel delayed off work, if any */
		if (kthread_cancel_delayed_work_sync(
				&dpu_enc->delayed_off_work))
			DPU_DEBUG_ENC(dpu_enc, "sw_event:%d, work cancelled\n",
					sw_event);

		mutex_lock(&dpu_enc->rc_lock);

		if (is_vid_mode &&
			  dpu_enc->rc_state == DPU_ENC_RC_STATE_IDLE) {
			_dpu_encoder_irq_control(drm_enc, true);
		}
		/* skip if is already OFF or IDLE, resources are off already */
		else if (dpu_enc->rc_state == DPU_ENC_RC_STATE_OFF ||
				dpu_enc->rc_state == DPU_ENC_RC_STATE_IDLE) {
			DRM_DEBUG_KMS("id:%u, sw_event:%d, rc in %d state\n",
				      DRMID(drm_enc), sw_event,
				      dpu_enc->rc_state);
			mutex_unlock(&dpu_enc->rc_lock);
			return 0;
		}

		dpu_enc->rc_state = DPU_ENC_RC_STATE_PRE_OFF;

		trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
				 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
				 "pre stop");

		mutex_unlock(&dpu_enc->rc_lock);
		break;

	case DPU_ENC_RC_EVENT_STOP:
		mutex_lock(&dpu_enc->rc_lock);

		/* return if the resource control is already in OFF state */
		if (dpu_enc->rc_state == DPU_ENC_RC_STATE_OFF) {
			DRM_DEBUG_KMS("id: %u, sw_event:%d, rc in OFF state\n",
				      DRMID(drm_enc), sw_event);
			mutex_unlock(&dpu_enc->rc_lock);
			return 0;
		} else if (dpu_enc->rc_state == DPU_ENC_RC_STATE_ON) {
			DRM_ERROR("id: %u, sw_event:%d, rc in state %d\n",
				  DRMID(drm_enc), sw_event, dpu_enc->rc_state);
			mutex_unlock(&dpu_enc->rc_lock);
			return -EINVAL;
		}

		/**
		 * expect to arrive here only if in either idle state or pre-off
		 * and in IDLE state the resources are already disabled
		 */
		if (dpu_enc->rc_state == DPU_ENC_RC_STATE_PRE_OFF)
			_dpu_encoder_resource_control_helper(drm_enc, false);

		dpu_enc->rc_state = DPU_ENC_RC_STATE_OFF;

		trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
				 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
				 "stop");

		mutex_unlock(&dpu_enc->rc_lock);
		break;

	case DPU_ENC_RC_EVENT_ENTER_IDLE:
		mutex_lock(&dpu_enc->rc_lock);

		if (dpu_enc->rc_state != DPU_ENC_RC_STATE_ON) {
			DRM_ERROR("id: %u, sw_event:%d, rc:%d !ON state\n",
				  DRMID(drm_enc), sw_event, dpu_enc->rc_state);
			mutex_unlock(&dpu_enc->rc_lock);
			return 0;
		}

		/*
		 * if we are in ON but a frame was just kicked off,
		 * ignore the IDLE event, it's probably a stale timer event
		 */
		if (dpu_enc->frame_busy_mask[0]) {
			DRM_ERROR("id:%u, sw_event:%d, rc:%d frame pending\n",
				  DRMID(drm_enc), sw_event, dpu_enc->rc_state);
			mutex_unlock(&dpu_enc->rc_lock);
			return 0;
		}

		if (is_vid_mode)
			_dpu_encoder_irq_control(drm_enc, false);
		else
			_dpu_encoder_resource_control_helper(drm_enc, false);

		dpu_enc->rc_state = DPU_ENC_RC_STATE_IDLE;

		trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
				 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
				 "idle");

		mutex_unlock(&dpu_enc->rc_lock);
		break;

	default:
		DRM_ERROR("id:%u, unexpected sw_event: %d\n", DRMID(drm_enc),
			  sw_event);
		trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
				 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
				 "error");
		break;
	}

	trace_dpu_enc_rc(DRMID(drm_enc), sw_event,
			 dpu_enc->idle_pc_supported, dpu_enc->rc_state,
			 "end");
	return 0;
}

static void dpu_encoder_virt_mode_set(struct drm_encoder *drm_enc,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adj_mode)
{
	struct dpu_encoder_virt *dpu_enc;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;
	struct list_head *connector_list;
	struct drm_connector *conn = NULL, *conn_iter;
	struct dpu_rm_hw_iter pp_iter;
	struct msm_display_topology topology;
	enum dpu_rm_topology_name topology_name;
	int i = 0, ret;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	DPU_DEBUG_ENC(dpu_enc, "\n");

	priv = drm_enc->dev->dev_private;
	dpu_kms = to_dpu_kms(priv->kms);
	connector_list = &dpu_kms->dev->mode_config.connector_list;

	trace_dpu_enc_mode_set(DRMID(drm_enc));

	list_for_each_entry(conn_iter, connector_list, head)
		if (conn_iter->encoder == drm_enc)
			conn = conn_iter;

	if (!conn) {
		DPU_ERROR_ENC(dpu_enc, "failed to find attached connector\n");
		return;
	} else if (!conn->state) {
		DPU_ERROR_ENC(dpu_enc, "invalid connector state\n");
		return;
	}

	topology = dpu_encoder_get_topology(dpu_enc, dpu_kms, adj_mode);

	/* Reserve dynamic resources now. Indicating non-AtomicTest phase */
	ret = dpu_rm_reserve(&dpu_kms->rm, drm_enc, drm_enc->crtc->state,
			conn->state, topology, false);
	if (ret) {
		DPU_ERROR_ENC(dpu_enc,
				"failed to reserve hw resources, %d\n", ret);
		return;
	}

	dpu_rm_init_hw_iter(&pp_iter, drm_enc->base.id, DPU_HW_BLK_PINGPONG);
	for (i = 0; i < MAX_CHANNELS_PER_ENC; i++) {
		dpu_enc->hw_pp[i] = NULL;
		if (!dpu_rm_get_hw(&dpu_kms->rm, &pp_iter))
			break;
		dpu_enc->hw_pp[i] = (struct dpu_hw_pingpong *) pp_iter.hw;
	}

	topology_name = dpu_rm_get_topology_name(topology);
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys) {
			if (!dpu_enc->hw_pp[i]) {
				DPU_ERROR_ENC(dpu_enc,
				    "invalid pingpong block for the encoder\n");
				return;
			}
			phys->hw_pp = dpu_enc->hw_pp[i];
			phys->connector = conn->state->connector;
			phys->topology_name = topology_name;
			if (phys->ops.mode_set)
				phys->ops.mode_set(phys, mode, adj_mode);
		}
	}

	dpu_enc->mode_set_complete = true;
}

static void _dpu_encoder_virt_enable_helper(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		DPU_ERROR("invalid parameters\n");
		return;
	}

	priv = drm_enc->dev->dev_private;
	dpu_kms = to_dpu_kms(priv->kms);
	if (!dpu_kms) {
		DPU_ERROR("invalid dpu_kms\n");
		return;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	if (!dpu_enc || !dpu_enc->cur_master) {
		DPU_ERROR("invalid dpu encoder/master\n");
		return;
	}

	if (dpu_enc->disp_info.intf_type == DRM_MODE_CONNECTOR_DisplayPort &&
	    dpu_enc->cur_master->hw_mdptop &&
	    dpu_enc->cur_master->hw_mdptop->ops.intf_audio_select)
		dpu_enc->cur_master->hw_mdptop->ops.intf_audio_select(
					dpu_enc->cur_master->hw_mdptop);

	if (dpu_enc->cur_master->hw_mdptop &&
			dpu_enc->cur_master->hw_mdptop->ops.reset_ubwc)
		dpu_enc->cur_master->hw_mdptop->ops.reset_ubwc(
				dpu_enc->cur_master->hw_mdptop,
				dpu_kms->catalog);

	_dpu_encoder_update_vsync_source(dpu_enc, &dpu_enc->disp_info);
}

void dpu_encoder_virt_restore(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	int i;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && (phys != dpu_enc->cur_master) && phys->ops.restore)
			phys->ops.restore(phys);
	}

	if (dpu_enc->cur_master && dpu_enc->cur_master->ops.restore)
		dpu_enc->cur_master->ops.restore(dpu_enc->cur_master);

	_dpu_encoder_virt_enable_helper(drm_enc);
}

static void dpu_encoder_virt_enable(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	int i, ret = 0;
	struct drm_display_mode *cur_mode = NULL;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);
	cur_mode = &dpu_enc->base.crtc->state->adjusted_mode;

	trace_dpu_enc_enable(DRMID(drm_enc), cur_mode->hdisplay,
			     cur_mode->vdisplay);

	dpu_enc->cur_master = NULL;
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.is_master && phys->ops.is_master(phys)) {
			DPU_DEBUG_ENC(dpu_enc, "master is now idx %d\n", i);
			dpu_enc->cur_master = phys;
			break;
		}
	}

	if (!dpu_enc->cur_master) {
		DPU_ERROR("virt encoder has no master! num_phys %d\n", i);
		return;
	}

	ret = dpu_encoder_resource_control(drm_enc, DPU_ENC_RC_EVENT_KICKOFF);
	if (ret) {
		DPU_ERROR_ENC(dpu_enc, "dpu resource control failed: %d\n",
				ret);
		return;
	}

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (!phys)
			continue;

		if (phys != dpu_enc->cur_master) {
			if (phys->ops.enable)
				phys->ops.enable(phys);
		}

		if (dpu_enc->misr_enable && (dpu_enc->disp_info.capabilities &
		     MSM_DISPLAY_CAP_VID_MODE) && phys->ops.setup_misr)
			phys->ops.setup_misr(phys, true,
						dpu_enc->misr_frame_count);
	}

	if (dpu_enc->cur_master->ops.enable)
		dpu_enc->cur_master->ops.enable(dpu_enc->cur_master);

	_dpu_encoder_virt_enable_helper(drm_enc);
}

static void dpu_encoder_virt_disable(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;
	struct drm_display_mode *mode;
	int i = 0;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	} else if (!drm_enc->dev) {
		DPU_ERROR("invalid dev\n");
		return;
	} else if (!drm_enc->dev->dev_private) {
		DPU_ERROR("invalid dev_private\n");
		return;
	}

	mode = &drm_enc->crtc->state->adjusted_mode;

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	DPU_DEBUG_ENC(dpu_enc, "\n");

	priv = drm_enc->dev->dev_private;
	dpu_kms = to_dpu_kms(priv->kms);

	trace_dpu_enc_disable(DRMID(drm_enc));

	/* wait for idle */
	dpu_encoder_wait_for_event(drm_enc, MSM_ENC_TX_COMPLETE);

	dpu_encoder_resource_control(drm_enc, DPU_ENC_RC_EVENT_PRE_STOP);

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.disable)
			phys->ops.disable(phys);
	}

	/* after phys waits for frame-done, should be no more frames pending */
	if (atomic_xchg(&dpu_enc->frame_done_timeout, 0)) {
		DPU_ERROR("enc%d timeout pending\n", drm_enc->base.id);
		del_timer_sync(&dpu_enc->frame_done_timer);
	}

	dpu_encoder_resource_control(drm_enc, DPU_ENC_RC_EVENT_STOP);

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		if (dpu_enc->phys_encs[i])
			dpu_enc->phys_encs[i]->connector = NULL;
	}

	dpu_enc->cur_master = NULL;

	DPU_DEBUG_ENC(dpu_enc, "encoder disabled\n");

	dpu_rm_release(&dpu_kms->rm, drm_enc);
}

static enum dpu_intf dpu_encoder_get_intf(struct dpu_mdss_cfg *catalog,
		enum dpu_intf_type type, u32 controller_id)
{
	int i = 0;

	for (i = 0; i < catalog->intf_count; i++) {
		if (catalog->intf[i].type == type
		    && catalog->intf[i].controller_id == controller_id) {
			return catalog->intf[i].id;
		}
	}

	return INTF_MAX;
}

static void dpu_encoder_vblank_callback(struct drm_encoder *drm_enc,
		struct dpu_encoder_phys *phy_enc)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	unsigned long lock_flags;

	if (!drm_enc || !phy_enc)
		return;

	DPU_ATRACE_BEGIN("encoder_vblank_callback");
	dpu_enc = to_dpu_encoder_virt(drm_enc);

	spin_lock_irqsave(&dpu_enc->enc_spinlock, lock_flags);
	if (dpu_enc->crtc_vblank_cb)
		dpu_enc->crtc_vblank_cb(dpu_enc->crtc_vblank_cb_data);
	spin_unlock_irqrestore(&dpu_enc->enc_spinlock, lock_flags);

	atomic_inc(&phy_enc->vsync_cnt);
	DPU_ATRACE_END("encoder_vblank_callback");
}

static void dpu_encoder_underrun_callback(struct drm_encoder *drm_enc,
		struct dpu_encoder_phys *phy_enc)
{
	if (!phy_enc)
		return;

	DPU_ATRACE_BEGIN("encoder_underrun_callback");
	atomic_inc(&phy_enc->underrun_cnt);
	trace_dpu_enc_underrun_cb(DRMID(drm_enc),
				  atomic_read(&phy_enc->underrun_cnt));
	DPU_ATRACE_END("encoder_underrun_callback");
}

void dpu_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*vbl_cb)(void *), void *vbl_data)
{
	struct dpu_encoder_virt *dpu_enc = to_dpu_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;
	int i;

	enable = vbl_cb ? true : false;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	trace_dpu_enc_vblank_cb(DRMID(drm_enc), enable);

	spin_lock_irqsave(&dpu_enc->enc_spinlock, lock_flags);
	dpu_enc->crtc_vblank_cb = vbl_cb;
	dpu_enc->crtc_vblank_cb_data = vbl_data;
	spin_unlock_irqrestore(&dpu_enc->enc_spinlock, lock_flags);

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys && phys->ops.control_vblank_irq)
			phys->ops.control_vblank_irq(phys, enable);
	}
}

void dpu_encoder_register_frame_event_callback(struct drm_encoder *drm_enc,
		void (*frame_event_cb)(void *, u32 event),
		void *frame_event_cb_data)
{
	struct dpu_encoder_virt *dpu_enc = to_dpu_encoder_virt(drm_enc);
	unsigned long lock_flags;
	bool enable;

	enable = frame_event_cb ? true : false;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	trace_dpu_enc_frame_event_cb(DRMID(drm_enc), enable);

	spin_lock_irqsave(&dpu_enc->enc_spinlock, lock_flags);
	dpu_enc->crtc_frame_event_cb = frame_event_cb;
	dpu_enc->crtc_frame_event_cb_data = frame_event_cb_data;
	spin_unlock_irqrestore(&dpu_enc->enc_spinlock, lock_flags);
}

static void dpu_encoder_frame_done_callback(
		struct drm_encoder *drm_enc,
		struct dpu_encoder_phys *ready_phys, u32 event)
{
	struct dpu_encoder_virt *dpu_enc = to_dpu_encoder_virt(drm_enc);
	unsigned int i;

	if (event & (DPU_ENCODER_FRAME_EVENT_DONE
			| DPU_ENCODER_FRAME_EVENT_ERROR
			| DPU_ENCODER_FRAME_EVENT_PANEL_DEAD)) {

		if (!dpu_enc->frame_busy_mask[0]) {
			/**
			 * suppress frame_done without waiter,
			 * likely autorefresh
			 */
			trace_dpu_enc_frame_done_cb_not_busy(DRMID(drm_enc),
					event, ready_phys->intf_idx);
			return;
		}

		/* One of the physical encoders has become idle */
		for (i = 0; i < dpu_enc->num_phys_encs; i++) {
			if (dpu_enc->phys_encs[i] == ready_phys) {
				clear_bit(i, dpu_enc->frame_busy_mask);
				trace_dpu_enc_frame_done_cb(DRMID(drm_enc), i,
						dpu_enc->frame_busy_mask[0]);
			}
		}

		if (!dpu_enc->frame_busy_mask[0]) {
			atomic_set(&dpu_enc->frame_done_timeout, 0);
			del_timer(&dpu_enc->frame_done_timer);

			dpu_encoder_resource_control(drm_enc,
					DPU_ENC_RC_EVENT_FRAME_DONE);

			if (dpu_enc->crtc_frame_event_cb)
				dpu_enc->crtc_frame_event_cb(
					dpu_enc->crtc_frame_event_cb_data,
					event);
		}
	} else {
		if (dpu_enc->crtc_frame_event_cb)
			dpu_enc->crtc_frame_event_cb(
				dpu_enc->crtc_frame_event_cb_data, event);
	}
}

static void dpu_encoder_off_work(struct kthread_work *work)
{
	struct dpu_encoder_virt *dpu_enc = container_of(work,
			struct dpu_encoder_virt, delayed_off_work.work);

	if (!dpu_enc) {
		DPU_ERROR("invalid dpu encoder\n");
		return;
	}

	dpu_encoder_resource_control(&dpu_enc->base,
						DPU_ENC_RC_EVENT_ENTER_IDLE);

	dpu_encoder_frame_done_callback(&dpu_enc->base, NULL,
				DPU_ENCODER_FRAME_EVENT_IDLE);
}

/**
 * _dpu_encoder_trigger_flush - trigger flush for a physical encoder
 * drm_enc: Pointer to drm encoder structure
 * phys: Pointer to physical encoder structure
 * extra_flush_bits: Additional bit mask to include in flush trigger
 */
static inline void _dpu_encoder_trigger_flush(struct drm_encoder *drm_enc,
		struct dpu_encoder_phys *phys, uint32_t extra_flush_bits)
{
	struct dpu_hw_ctl *ctl;
	int pending_kickoff_cnt;
	u32 ret = UINT_MAX;

	if (!drm_enc || !phys) {
		DPU_ERROR("invalid argument(s), drm_enc %d, phys_enc %d\n",
				drm_enc != 0, phys != 0);
		return;
	}

	if (!phys->hw_pp) {
		DPU_ERROR("invalid pingpong hw\n");
		return;
	}

	ctl = phys->hw_ctl;
	if (!ctl || !ctl->ops.trigger_flush) {
		DPU_ERROR("missing trigger cb\n");
		return;
	}

	pending_kickoff_cnt = dpu_encoder_phys_inc_pending(phys);

	if (extra_flush_bits && ctl->ops.update_pending_flush)
		ctl->ops.update_pending_flush(ctl, extra_flush_bits);

	ctl->ops.trigger_flush(ctl);

	if (ctl->ops.get_pending_flush)
		ret = ctl->ops.get_pending_flush(ctl);

	trace_dpu_enc_trigger_flush(DRMID(drm_enc), phys->intf_idx,
				    pending_kickoff_cnt, ctl->idx, ret);
}

/**
 * _dpu_encoder_trigger_start - trigger start for a physical encoder
 * phys: Pointer to physical encoder structure
 */
static inline void _dpu_encoder_trigger_start(struct dpu_encoder_phys *phys)
{
	if (!phys) {
		DPU_ERROR("invalid argument(s)\n");
		return;
	}

	if (!phys->hw_pp) {
		DPU_ERROR("invalid pingpong hw\n");
		return;
	}

	if (phys->ops.trigger_start && phys->enable_state != DPU_ENC_DISABLED)
		phys->ops.trigger_start(phys);
}

void dpu_encoder_helper_trigger_start(struct dpu_encoder_phys *phys_enc)
{
	struct dpu_hw_ctl *ctl;

	if (!phys_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}

	ctl = phys_enc->hw_ctl;
	if (ctl && ctl->ops.trigger_start) {
		ctl->ops.trigger_start(ctl);
		trace_dpu_enc_trigger_start(DRMID(phys_enc->parent), ctl->idx);
	}
}

static int dpu_encoder_helper_wait_event_timeout(
		int32_t drm_id,
		int32_t hw_id,
		struct dpu_encoder_wait_info *info)
{
	int rc = 0;
	s64 expected_time = ktime_to_ms(ktime_get()) + info->timeout_ms;
	s64 jiffies = msecs_to_jiffies(info->timeout_ms);
	s64 time;

	do {
		rc = wait_event_timeout(*(info->wq),
				atomic_read(info->atomic_cnt) == 0, jiffies);
		time = ktime_to_ms(ktime_get());

		trace_dpu_enc_wait_event_timeout(drm_id, hw_id, rc, time,
						 expected_time,
						 atomic_read(info->atomic_cnt));
	/* If we timed out, counter is valid and time is less, wait again */
	} while (atomic_read(info->atomic_cnt) && (rc == 0) &&
			(time < expected_time));

	return rc;
}

void dpu_encoder_helper_hw_reset(struct dpu_encoder_phys *phys_enc)
{
	struct dpu_encoder_virt *dpu_enc;
	struct dpu_hw_ctl *ctl;
	int rc;

	if (!phys_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	dpu_enc = to_dpu_encoder_virt(phys_enc->parent);
	ctl = phys_enc->hw_ctl;

	if (!ctl || !ctl->ops.reset)
		return;

	DRM_DEBUG_KMS("id:%u ctl %d reset\n", DRMID(phys_enc->parent),
		      ctl->idx);

	rc = ctl->ops.reset(ctl);
	if (rc) {
		DPU_ERROR_ENC(dpu_enc, "ctl %d reset failure\n",  ctl->idx);
		dpu_dbg_dump(false, __func__, true, true);
	}

	phys_enc->enable_state = DPU_ENC_ENABLED;
}

/**
 * _dpu_encoder_kickoff_phys - handle physical encoder kickoff
 *	Iterate through the physical encoders and perform consolidated flush
 *	and/or control start triggering as needed. This is done in the virtual
 *	encoder rather than the individual physical ones in order to handle
 *	use cases that require visibility into multiple physical encoders at
 *	a time.
 * dpu_enc: Pointer to virtual encoder structure
 */
static void _dpu_encoder_kickoff_phys(struct dpu_encoder_virt *dpu_enc)
{
	struct dpu_hw_ctl *ctl;
	uint32_t i, pending_flush;
	unsigned long lock_flags;

	if (!dpu_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}

	pending_flush = 0x0;

	/* update pending counts and trigger kickoff ctl flush atomically */
	spin_lock_irqsave(&dpu_enc->enc_spinlock, lock_flags);

	/* don't perform flush/start operations for slave encoders */
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (!phys || phys->enable_state == DPU_ENC_DISABLED)
			continue;

		ctl = phys->hw_ctl;
		if (!ctl)
			continue;

		if (phys->split_role != ENC_ROLE_SLAVE)
			set_bit(i, dpu_enc->frame_busy_mask);
		if (!phys->ops.needs_single_flush ||
				!phys->ops.needs_single_flush(phys))
			_dpu_encoder_trigger_flush(&dpu_enc->base, phys, 0x0);
		else if (ctl->ops.get_pending_flush)
			pending_flush |= ctl->ops.get_pending_flush(ctl);
	}

	/* for split flush, combine pending flush masks and send to master */
	if (pending_flush && dpu_enc->cur_master) {
		_dpu_encoder_trigger_flush(
				&dpu_enc->base,
				dpu_enc->cur_master,
				pending_flush);
	}

	_dpu_encoder_trigger_start(dpu_enc->cur_master);

	spin_unlock_irqrestore(&dpu_enc->enc_spinlock, lock_flags);
}

void dpu_encoder_trigger_kickoff_pending(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc;
	struct dpu_encoder_phys *phys;
	unsigned int i;
	struct dpu_hw_ctl *ctl;
	struct msm_display_info *disp_info;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);
	disp_info = &dpu_enc->disp_info;

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		phys = dpu_enc->phys_encs[i];

		if (phys && phys->hw_ctl) {
			ctl = phys->hw_ctl;
			if (ctl->ops.clear_pending_flush)
				ctl->ops.clear_pending_flush(ctl);

			/* update only for command mode primary ctl */
			if ((phys == dpu_enc->cur_master) &&
			   (disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE)
			    && ctl->ops.trigger_pending)
				ctl->ops.trigger_pending(ctl);
		}
	}
}

static u32 _dpu_encoder_calculate_linetime(struct dpu_encoder_virt *dpu_enc,
		struct drm_display_mode *mode)
{
	u64 pclk_rate;
	u32 pclk_period;
	u32 line_time;

	/*
	 * For linetime calculation, only operate on master encoder.
	 */
	if (!dpu_enc->cur_master)
		return 0;

	if (!dpu_enc->cur_master->ops.get_line_count) {
		DPU_ERROR("get_line_count function not defined\n");
		return 0;
	}

	pclk_rate = mode->clock; /* pixel clock in kHz */
	if (pclk_rate == 0) {
		DPU_ERROR("pclk is 0, cannot calculate line time\n");
		return 0;
	}

	pclk_period = DIV_ROUND_UP_ULL(1000000000ull, pclk_rate);
	if (pclk_period == 0) {
		DPU_ERROR("pclk period is 0\n");
		return 0;
	}

	/*
	 * Line time calculation based on Pixel clock and HTOTAL.
	 * Final unit is in ns.
	 */
	line_time = (pclk_period * mode->htotal) / 1000;
	if (line_time == 0) {
		DPU_ERROR("line time calculation is 0\n");
		return 0;
	}

	DPU_DEBUG_ENC(dpu_enc,
			"clk_rate=%lldkHz, clk_period=%d, linetime=%dns\n",
			pclk_rate, pclk_period, line_time);

	return line_time;
}

static int _dpu_encoder_wakeup_time(struct drm_encoder *drm_enc,
		ktime_t *wakeup_time)
{
	struct drm_display_mode *mode;
	struct dpu_encoder_virt *dpu_enc;
	u32 cur_line;
	u32 line_time;
	u32 vtotal, time_to_vsync;
	ktime_t cur_time;

	dpu_enc = to_dpu_encoder_virt(drm_enc);

	if (!drm_enc->crtc || !drm_enc->crtc->state) {
		DPU_ERROR("crtc/crtc state object is NULL\n");
		return -EINVAL;
	}
	mode = &drm_enc->crtc->state->adjusted_mode;

	line_time = _dpu_encoder_calculate_linetime(dpu_enc, mode);
	if (!line_time)
		return -EINVAL;

	cur_line = dpu_enc->cur_master->ops.get_line_count(dpu_enc->cur_master);

	vtotal = mode->vtotal;
	if (cur_line >= vtotal)
		time_to_vsync = line_time * vtotal;
	else
		time_to_vsync = line_time * (vtotal - cur_line);

	if (time_to_vsync == 0) {
		DPU_ERROR("time to vsync should not be zero, vtotal=%d\n",
				vtotal);
		return -EINVAL;
	}

	cur_time = ktime_get();
	*wakeup_time = ktime_add_ns(cur_time, time_to_vsync);

	DPU_DEBUG_ENC(dpu_enc,
			"cur_line=%u vtotal=%u time_to_vsync=%u, cur_time=%lld, wakeup_time=%lld\n",
			cur_line, vtotal, time_to_vsync,
			ktime_to_ms(cur_time),
			ktime_to_ms(*wakeup_time));
	return 0;
}

static void dpu_encoder_vsync_event_handler(struct timer_list *t)
{
	struct dpu_encoder_virt *dpu_enc = from_timer(dpu_enc, t,
			vsync_event_timer);
	struct drm_encoder *drm_enc = &dpu_enc->base;
	struct msm_drm_private *priv;
	struct msm_drm_thread *event_thread;

	if (!drm_enc->dev || !drm_enc->dev->dev_private ||
			!drm_enc->crtc) {
		DPU_ERROR("invalid parameters\n");
		return;
	}

	priv = drm_enc->dev->dev_private;

	if (drm_enc->crtc->index >= ARRAY_SIZE(priv->event_thread)) {
		DPU_ERROR("invalid crtc index\n");
		return;
	}
	event_thread = &priv->event_thread[drm_enc->crtc->index];
	if (!event_thread) {
		DPU_ERROR("event_thread not found for crtc:%d\n",
				drm_enc->crtc->index);
		return;
	}

	del_timer(&dpu_enc->vsync_event_timer);
}

static void dpu_encoder_vsync_event_work_handler(struct kthread_work *work)
{
	struct dpu_encoder_virt *dpu_enc = container_of(work,
			struct dpu_encoder_virt, vsync_event_work);
	ktime_t wakeup_time;

	if (!dpu_enc) {
		DPU_ERROR("invalid dpu encoder\n");
		return;
	}

	if (_dpu_encoder_wakeup_time(&dpu_enc->base, &wakeup_time))
		return;

	trace_dpu_enc_vsync_event_work(DRMID(&dpu_enc->base), wakeup_time);
	mod_timer(&dpu_enc->vsync_event_timer,
			nsecs_to_jiffies(ktime_to_ns(wakeup_time)));
}

void dpu_encoder_prepare_for_kickoff(struct drm_encoder *drm_enc,
		struct dpu_encoder_kickoff_params *params)
{
	struct dpu_encoder_virt *dpu_enc;
	struct dpu_encoder_phys *phys;
	bool needs_hw_reset = false;
	unsigned int i;

	if (!drm_enc || !params) {
		DPU_ERROR("invalid args\n");
		return;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);

	trace_dpu_enc_prepare_kickoff(DRMID(drm_enc));

	/* prepare for next kickoff, may include waiting on previous kickoff */
	DPU_ATRACE_BEGIN("enc_prepare_for_kickoff");
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		phys = dpu_enc->phys_encs[i];
		if (phys) {
			if (phys->ops.prepare_for_kickoff)
				phys->ops.prepare_for_kickoff(phys, params);
			if (phys->enable_state == DPU_ENC_ERR_NEEDS_HW_RESET)
				needs_hw_reset = true;
		}
	}
	DPU_ATRACE_END("enc_prepare_for_kickoff");

	dpu_encoder_resource_control(drm_enc, DPU_ENC_RC_EVENT_KICKOFF);

	/* if any phys needs reset, reset all phys, in-order */
	if (needs_hw_reset) {
		trace_dpu_enc_prepare_kickoff_reset(DRMID(drm_enc));
		for (i = 0; i < dpu_enc->num_phys_encs; i++) {
			phys = dpu_enc->phys_encs[i];
			if (phys && phys->ops.hw_reset)
				phys->ops.hw_reset(phys);
		}
	}
}

void dpu_encoder_kickoff(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc;
	struct dpu_encoder_phys *phys;
	ktime_t wakeup_time;
	unsigned int i;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	DPU_ATRACE_BEGIN("encoder_kickoff");
	dpu_enc = to_dpu_encoder_virt(drm_enc);

	trace_dpu_enc_kickoff(DRMID(drm_enc));

	atomic_set(&dpu_enc->frame_done_timeout,
			DPU_FRAME_DONE_TIMEOUT * 1000 /
			drm_enc->crtc->state->adjusted_mode.vrefresh);
	mod_timer(&dpu_enc->frame_done_timer, jiffies +
		((atomic_read(&dpu_enc->frame_done_timeout) * HZ) / 1000));

	/* All phys encs are ready to go, trigger the kickoff */
	_dpu_encoder_kickoff_phys(dpu_enc);

	/* allow phys encs to handle any post-kickoff business */
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		phys = dpu_enc->phys_encs[i];
		if (phys && phys->ops.handle_post_kickoff)
			phys->ops.handle_post_kickoff(phys);
	}

	if (dpu_enc->disp_info.intf_type == DRM_MODE_CONNECTOR_DSI &&
			!_dpu_encoder_wakeup_time(drm_enc, &wakeup_time)) {
		trace_dpu_enc_early_kickoff(DRMID(drm_enc),
					    ktime_to_ms(wakeup_time));
		mod_timer(&dpu_enc->vsync_event_timer,
				nsecs_to_jiffies(ktime_to_ns(wakeup_time)));
	}

	DPU_ATRACE_END("encoder_kickoff");
}

void dpu_encoder_prepare_commit(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc;
	struct dpu_encoder_phys *phys;
	int i;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		phys = dpu_enc->phys_encs[i];
		if (phys && phys->ops.prepare_commit)
			phys->ops.prepare_commit(phys);
	}
}

#ifdef CONFIG_DEBUG_FS
static int _dpu_encoder_status_show(struct seq_file *s, void *data)
{
	struct dpu_encoder_virt *dpu_enc;
	int i;

	if (!s || !s->private)
		return -EINVAL;

	dpu_enc = s->private;

	mutex_lock(&dpu_enc->enc_lock);
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (!phys)
			continue;

		seq_printf(s, "intf:%d    vsync:%8d     underrun:%8d    ",
				phys->intf_idx - INTF_0,
				atomic_read(&phys->vsync_cnt),
				atomic_read(&phys->underrun_cnt));

		switch (phys->intf_mode) {
		case INTF_MODE_VIDEO:
			seq_puts(s, "mode: video\n");
			break;
		case INTF_MODE_CMD:
			seq_puts(s, "mode: command\n");
			break;
		default:
			seq_puts(s, "mode: ???\n");
			break;
		}
	}
	mutex_unlock(&dpu_enc->enc_lock);

	return 0;
}

static int _dpu_encoder_debugfs_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, _dpu_encoder_status_show, inode->i_private);
}

static ssize_t _dpu_encoder_misr_setup(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct dpu_encoder_virt *dpu_enc;
	int i = 0, rc;
	char buf[MISR_BUFF_SIZE + 1];
	size_t buff_copy;
	u32 frame_count, enable;

	if (!file || !file->private_data)
		return -EINVAL;

	dpu_enc = file->private_data;

	buff_copy = min_t(size_t, count, MISR_BUFF_SIZE);
	if (copy_from_user(buf, user_buf, buff_copy))
		return -EINVAL;

	buf[buff_copy] = 0; /* end of string */

	if (sscanf(buf, "%u %u", &enable, &frame_count) != 2)
		return -EINVAL;

	rc = _dpu_encoder_power_enable(dpu_enc, true);
	if (rc)
		return rc;

	mutex_lock(&dpu_enc->enc_lock);
	dpu_enc->misr_enable = enable;
	dpu_enc->misr_frame_count = frame_count;
	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (!phys || !phys->ops.setup_misr)
			continue;

		phys->ops.setup_misr(phys, enable, frame_count);
	}
	mutex_unlock(&dpu_enc->enc_lock);
	_dpu_encoder_power_enable(dpu_enc, false);

	return count;
}

static ssize_t _dpu_encoder_misr_read(struct file *file,
		char __user *user_buff, size_t count, loff_t *ppos)
{
	struct dpu_encoder_virt *dpu_enc;
	int i = 0, len = 0;
	char buf[MISR_BUFF_SIZE + 1] = {'\0'};
	int rc;

	if (*ppos)
		return 0;

	if (!file || !file->private_data)
		return -EINVAL;

	dpu_enc = file->private_data;

	rc = _dpu_encoder_power_enable(dpu_enc, true);
	if (rc)
		return rc;

	mutex_lock(&dpu_enc->enc_lock);
	if (!dpu_enc->misr_enable) {
		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"disabled\n");
		goto buff_check;
	} else if (dpu_enc->disp_info.capabilities &
						~MSM_DISPLAY_CAP_VID_MODE) {
		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"unsupported\n");
		goto buff_check;
	}

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (!phys || !phys->ops.collect_misr)
			continue;

		len += snprintf(buf + len, MISR_BUFF_SIZE - len,
			"Intf idx:%d\n", phys->intf_idx - INTF_0);
		len += snprintf(buf + len, MISR_BUFF_SIZE - len, "0x%x\n",
					phys->ops.collect_misr(phys));
	}

buff_check:
	if (count <= len) {
		len = 0;
		goto end;
	}

	if (copy_to_user(user_buff, buf, len)) {
		len = -EFAULT;
		goto end;
	}

	*ppos += len;   /* increase offset */

end:
	mutex_unlock(&dpu_enc->enc_lock);
	_dpu_encoder_power_enable(dpu_enc, false);
	return len;
}

static int _dpu_encoder_init_debugfs(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc;
	struct msm_drm_private *priv;
	struct dpu_kms *dpu_kms;
	int i;

	static const struct file_operations debugfs_status_fops = {
		.open =		_dpu_encoder_debugfs_status_open,
		.read =		seq_read,
		.llseek =	seq_lseek,
		.release =	single_release,
	};

	static const struct file_operations debugfs_misr_fops = {
		.open = simple_open,
		.read = _dpu_encoder_misr_read,
		.write = _dpu_encoder_misr_setup,
	};

	char name[DPU_NAME_SIZE];

	if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
		DPU_ERROR("invalid encoder or kms\n");
		return -EINVAL;
	}

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	priv = drm_enc->dev->dev_private;
	dpu_kms = to_dpu_kms(priv->kms);

	snprintf(name, DPU_NAME_SIZE, "encoder%u", drm_enc->base.id);

	/* create overall sub-directory for the encoder */
	dpu_enc->debugfs_root = debugfs_create_dir(name,
			drm_enc->dev->primary->debugfs_root);
	if (!dpu_enc->debugfs_root)
		return -ENOMEM;

	/* don't error check these */
	debugfs_create_file("status", 0600,
		dpu_enc->debugfs_root, dpu_enc, &debugfs_status_fops);

	debugfs_create_file("misr_data", 0600,
		dpu_enc->debugfs_root, dpu_enc, &debugfs_misr_fops);

	for (i = 0; i < dpu_enc->num_phys_encs; i++)
		if (dpu_enc->phys_encs[i] &&
				dpu_enc->phys_encs[i]->ops.late_register)
			dpu_enc->phys_encs[i]->ops.late_register(
					dpu_enc->phys_encs[i],
					dpu_enc->debugfs_root);

	return 0;
}

static void _dpu_encoder_destroy_debugfs(struct drm_encoder *drm_enc)
{
	struct dpu_encoder_virt *dpu_enc;

	if (!drm_enc)
		return;

	dpu_enc = to_dpu_encoder_virt(drm_enc);
	debugfs_remove_recursive(dpu_enc->debugfs_root);
}
#else
static int _dpu_encoder_init_debugfs(struct drm_encoder *drm_enc)
{
	return 0;
}

static void _dpu_encoder_destroy_debugfs(struct drm_encoder *drm_enc)
{
}
#endif

static int dpu_encoder_late_register(struct drm_encoder *encoder)
{
	return _dpu_encoder_init_debugfs(encoder);
}

static void dpu_encoder_early_unregister(struct drm_encoder *encoder)
{
	_dpu_encoder_destroy_debugfs(encoder);
}

static int dpu_encoder_virt_add_phys_encs(
		u32 display_caps,
		struct dpu_encoder_virt *dpu_enc,
		struct dpu_enc_phys_init_params *params)
{
	struct dpu_encoder_phys *enc = NULL;

	DPU_DEBUG_ENC(dpu_enc, "\n");

	/*
	 * We may create up to NUM_PHYS_ENCODER_TYPES physical encoder types
	 * in this function, check up-front.
	 */
	if (dpu_enc->num_phys_encs + NUM_PHYS_ENCODER_TYPES >=
			ARRAY_SIZE(dpu_enc->phys_encs)) {
		DPU_ERROR_ENC(dpu_enc, "too many physical encoders %d\n",
			  dpu_enc->num_phys_encs);
		return -EINVAL;
	}

	if (display_caps & MSM_DISPLAY_CAP_VID_MODE) {
		enc = dpu_encoder_phys_vid_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			DPU_ERROR_ENC(dpu_enc, "failed to init vid enc: %ld\n",
				PTR_ERR(enc));
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		dpu_enc->phys_encs[dpu_enc->num_phys_encs] = enc;
		++dpu_enc->num_phys_encs;
	}

	if (display_caps & MSM_DISPLAY_CAP_CMD_MODE) {
		enc = dpu_encoder_phys_cmd_init(params);

		if (IS_ERR_OR_NULL(enc)) {
			DPU_ERROR_ENC(dpu_enc, "failed to init cmd enc: %ld\n",
				PTR_ERR(enc));
			return enc == 0 ? -EINVAL : PTR_ERR(enc);
		}

		dpu_enc->phys_encs[dpu_enc->num_phys_encs] = enc;
		++dpu_enc->num_phys_encs;
	}

	return 0;
}

static const struct dpu_encoder_virt_ops dpu_encoder_parent_ops = {
	.handle_vblank_virt = dpu_encoder_vblank_callback,
	.handle_underrun_virt = dpu_encoder_underrun_callback,
	.handle_frame_done = dpu_encoder_frame_done_callback,
};

static int dpu_encoder_setup_display(struct dpu_encoder_virt *dpu_enc,
				 struct dpu_kms *dpu_kms,
				 struct msm_display_info *disp_info,
				 int *drm_enc_mode)
{
	int ret = 0;
	int i = 0;
	enum dpu_intf_type intf_type;
	struct dpu_enc_phys_init_params phys_params;

	if (!dpu_enc || !dpu_kms) {
		DPU_ERROR("invalid arg(s), enc %d kms %d\n",
				dpu_enc != 0, dpu_kms != 0);
		return -EINVAL;
	}

	memset(&phys_params, 0, sizeof(phys_params));
	phys_params.dpu_kms = dpu_kms;
	phys_params.parent = &dpu_enc->base;
	phys_params.parent_ops = &dpu_encoder_parent_ops;
	phys_params.enc_spinlock = &dpu_enc->enc_spinlock;

	DPU_DEBUG("\n");

	if (disp_info->intf_type == DRM_MODE_CONNECTOR_DSI) {
		*drm_enc_mode = DRM_MODE_ENCODER_DSI;
		intf_type = INTF_DSI;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_HDMIA) {
		*drm_enc_mode = DRM_MODE_ENCODER_TMDS;
		intf_type = INTF_HDMI;
	} else if (disp_info->intf_type == DRM_MODE_CONNECTOR_DisplayPort) {
		*drm_enc_mode = DRM_MODE_ENCODER_TMDS;
		intf_type = INTF_DP;
	} else {
		DPU_ERROR_ENC(dpu_enc, "unsupported display interface type\n");
		return -EINVAL;
	}

	WARN_ON(disp_info->num_of_h_tiles < 1);

	dpu_enc->display_num_of_h_tiles = disp_info->num_of_h_tiles;

	DPU_DEBUG("dsi_info->num_of_h_tiles %d\n", disp_info->num_of_h_tiles);

	if ((disp_info->capabilities & MSM_DISPLAY_CAP_CMD_MODE) ||
	    (disp_info->capabilities & MSM_DISPLAY_CAP_VID_MODE))
		dpu_enc->idle_pc_supported =
				dpu_kms->catalog->caps->has_idle_pc;

	mutex_lock(&dpu_enc->enc_lock);
	for (i = 0; i < disp_info->num_of_h_tiles && !ret; i++) {
		/*
		 * Left-most tile is at index 0, content is controller id
		 * h_tile_instance_ids[2] = {0, 1}; DSI0 = left, DSI1 = right
		 * h_tile_instance_ids[2] = {1, 0}; DSI1 = left, DSI0 = right
		 */
		u32 controller_id = disp_info->h_tile_instance[i];

		if (disp_info->num_of_h_tiles > 1) {
			if (i == 0)
				phys_params.split_role = ENC_ROLE_MASTER;
			else
				phys_params.split_role = ENC_ROLE_SLAVE;
		} else {
			phys_params.split_role = ENC_ROLE_SOLO;
		}

		DPU_DEBUG("h_tile_instance %d = %d, split_role %d\n",
				i, controller_id, phys_params.split_role);

		phys_params.intf_idx = dpu_encoder_get_intf(dpu_kms->catalog,
													intf_type,
													controller_id);
		if (phys_params.intf_idx == INTF_MAX) {
			DPU_ERROR_ENC(dpu_enc, "could not get intf: type %d, id %d\n",
						  intf_type, controller_id);
			ret = -EINVAL;
		}

		if (!ret) {
			ret = dpu_encoder_virt_add_phys_encs(disp_info->capabilities,
												 dpu_enc,
												 &phys_params);
			if (ret)
				DPU_ERROR_ENC(dpu_enc, "failed to add phys encs\n");
		}
	}

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys) {
			atomic_set(&phys->vsync_cnt, 0);
			atomic_set(&phys->underrun_cnt, 0);
		}
	}
	mutex_unlock(&dpu_enc->enc_lock);

	return ret;
}

static void dpu_encoder_frame_done_timeout(struct timer_list *t)
{
	struct dpu_encoder_virt *dpu_enc = from_timer(dpu_enc, t,
			frame_done_timer);
	struct drm_encoder *drm_enc = &dpu_enc->base;
	struct msm_drm_private *priv;
	u32 event;

	if (!drm_enc->dev || !drm_enc->dev->dev_private) {
		DPU_ERROR("invalid parameters\n");
		return;
	}
	priv = drm_enc->dev->dev_private;

	if (!dpu_enc->frame_busy_mask[0] || !dpu_enc->crtc_frame_event_cb) {
		DRM_DEBUG_KMS("id:%u invalid timeout frame_busy_mask=%lu\n",
			      DRMID(drm_enc), dpu_enc->frame_busy_mask[0]);
		return;
	} else if (!atomic_xchg(&dpu_enc->frame_done_timeout, 0)) {
		DRM_DEBUG_KMS("id:%u invalid timeout\n", DRMID(drm_enc));
		return;
	}

	DPU_ERROR_ENC(dpu_enc, "frame done timeout\n");

	event = DPU_ENCODER_FRAME_EVENT_ERROR;
	trace_dpu_enc_frame_done_timeout(DRMID(drm_enc), event);
	dpu_enc->crtc_frame_event_cb(dpu_enc->crtc_frame_event_cb_data, event);
}

static const struct drm_encoder_helper_funcs dpu_encoder_helper_funcs = {
	.mode_set = dpu_encoder_virt_mode_set,
	.disable = dpu_encoder_virt_disable,
	.enable = dpu_kms_encoder_enable,
	.atomic_check = dpu_encoder_virt_atomic_check,

	/* This is called by dpu_kms_encoder_enable */
	.commit = dpu_encoder_virt_enable,
};

static const struct drm_encoder_funcs dpu_encoder_funcs = {
		.destroy = dpu_encoder_destroy,
		.late_register = dpu_encoder_late_register,
		.early_unregister = dpu_encoder_early_unregister,
};

int dpu_encoder_setup(struct drm_device *dev, struct drm_encoder *enc,
		struct msm_display_info *disp_info)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct dpu_kms *dpu_kms = to_dpu_kms(priv->kms);
	struct drm_encoder *drm_enc = NULL;
	struct dpu_encoder_virt *dpu_enc = NULL;
	int drm_enc_mode = DRM_MODE_ENCODER_NONE;
	int ret = 0;

	dpu_enc = to_dpu_encoder_virt(enc);

	mutex_init(&dpu_enc->enc_lock);
	ret = dpu_encoder_setup_display(dpu_enc, dpu_kms, disp_info,
			&drm_enc_mode);
	if (ret)
		goto fail;

	dpu_enc->cur_master = NULL;
	spin_lock_init(&dpu_enc->enc_spinlock);

	atomic_set(&dpu_enc->frame_done_timeout, 0);
	timer_setup(&dpu_enc->frame_done_timer,
			dpu_encoder_frame_done_timeout, 0);

	if (disp_info->intf_type == DRM_MODE_CONNECTOR_DSI)
		timer_setup(&dpu_enc->vsync_event_timer,
				dpu_encoder_vsync_event_handler,
				0);


	mutex_init(&dpu_enc->rc_lock);
	kthread_init_delayed_work(&dpu_enc->delayed_off_work,
			dpu_encoder_off_work);
	dpu_enc->idle_timeout = IDLE_TIMEOUT;

	kthread_init_work(&dpu_enc->vsync_event_work,
			dpu_encoder_vsync_event_work_handler);

	memcpy(&dpu_enc->disp_info, disp_info, sizeof(*disp_info));

	DPU_DEBUG_ENC(dpu_enc, "created\n");

	return ret;

fail:
	DPU_ERROR("failed to create encoder\n");
	if (drm_enc)
		dpu_encoder_destroy(drm_enc);

	return ret;


}

struct drm_encoder *dpu_encoder_init(struct drm_device *dev,
		int drm_enc_mode)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	int rc = 0;

	dpu_enc = devm_kzalloc(dev->dev, sizeof(*dpu_enc), GFP_KERNEL);
	if (!dpu_enc)
		return ERR_PTR(ENOMEM);

	rc = drm_encoder_init(dev, &dpu_enc->base, &dpu_encoder_funcs,
			drm_enc_mode, NULL);
	if (rc) {
		devm_kfree(dev->dev, dpu_enc);
		return ERR_PTR(rc);
	}

	drm_encoder_helper_add(&dpu_enc->base, &dpu_encoder_helper_funcs);

	return &dpu_enc->base;
}

int dpu_encoder_wait_for_event(struct drm_encoder *drm_enc,
	enum msm_event_wait event)
{
	int (*fn_wait)(struct dpu_encoder_phys *phys_enc) = NULL;
	struct dpu_encoder_virt *dpu_enc = NULL;
	int i, ret = 0;

	if (!drm_enc) {
		DPU_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	dpu_enc = to_dpu_encoder_virt(drm_enc);
	DPU_DEBUG_ENC(dpu_enc, "\n");

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];
		if (!phys)
			continue;

		switch (event) {
		case MSM_ENC_COMMIT_DONE:
			fn_wait = phys->ops.wait_for_commit_done;
			break;
		case MSM_ENC_TX_COMPLETE:
			fn_wait = phys->ops.wait_for_tx_complete;
			break;
		case MSM_ENC_VBLANK:
			fn_wait = phys->ops.wait_for_vblank;
			break;
		default:
			DPU_ERROR_ENC(dpu_enc, "unknown wait event %d\n",
					event);
			return -EINVAL;
		};

		if (fn_wait) {
			DPU_ATRACE_BEGIN("wait_for_completion_event");
			ret = fn_wait(phys);
			DPU_ATRACE_END("wait_for_completion_event");
			if (ret)
				return ret;
		}
	}

	return ret;
}

enum dpu_intf_mode dpu_encoder_get_intf_mode(struct drm_encoder *encoder)
{
	struct dpu_encoder_virt *dpu_enc = NULL;
	int i;

	if (!encoder) {
		DPU_ERROR("invalid encoder\n");
		return INTF_MODE_NONE;
	}
	dpu_enc = to_dpu_encoder_virt(encoder);

	if (dpu_enc->cur_master)
		return dpu_enc->cur_master->intf_mode;

	for (i = 0; i < dpu_enc->num_phys_encs; i++) {
		struct dpu_encoder_phys *phys = dpu_enc->phys_encs[i];

		if (phys)
			return phys->intf_mode;
	}

	return INTF_MODE_NONE;
}

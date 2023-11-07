/*
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare suspend / resume related API's
 */

#ifndef _WLAN_PMO_SUSPEND_RESUME_H_
#define _WLAN_PMO_SUSPEND_RESUME_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_wow.h"

/**
 * pmo_core_configure_dynamic_wake_events(): configure dynamic wake events
 * @psoc: objmgr psoc handle
 *
 * Some wake events need to be enabled dynamically. Control those here.
 *
 * Return: none
 */
void pmo_core_configure_dynamic_wake_events(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_get_wow_bus_suspend(): API to get wow bus is suspended or not
 * @psoc: objmgr psoc handle
 *
 * Return: True if bus suspende else false
 */
static inline bool pmo_core_get_wow_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	bool value = false;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		value = psoc_ctx->wow.is_wow_bus_suspended;
	}

	return value;
}

/**
 * pmo_core_psoc_user_space_suspend_req() -  Core handle user space suspend req
 * @psoc: objmgr psoc handle
 * @type: type of suspend
 *
 * Pmo core Handles user space suspend request for psoc
 *
 * Return: QDF status
 */
QDF_STATUS pmo_core_psoc_user_space_suspend_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type);

/**
 * pmo_core_psoc_user_space_resume_req() - Core handle user space resume req
 * @psoc: objmgr psoc handle
 * @type: type of suspend from resume required
 *
 * Pmo core Handles user space resume request for psoc
 *
 * Return: QDF status
 */
QDF_STATUS pmo_core_psoc_user_space_resume_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type);

/**
 * pmo_core_psoc_bus_suspend_req(): handles bus suspend for psoc
 * @psoc: objmgr psoc
 * @type: is this suspend part of runtime suspend or system suspend?
 * @wow_params: collection of wow enable override parameters
 *
 * Bails if a scan is in progress.
 * Calls the appropriate handlers based on configuration and event.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_core_psoc_bus_suspend_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type,
		struct pmo_wow_enable_params *wow_params);

#ifdef FEATURE_RUNTIME_PM
/**
 * pmo_core_psoc_bus_runtime_suspend(): handles bus runtime suspend
 * @psoc: objmgr psoc
 * @pld_cb: callback to do link auto suspend
 *
 * Suspend the wlan bus without apps suspend.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_core_psoc_bus_runtime_suspend(struct wlan_objmgr_psoc *psoc,
					     pmo_pld_auto_suspend_cb pld_cb);

/**
 * pmo_core_psoc_bus_runtime_resume(): handles bus runtime resume
 * @psoc: objmgr psoc
 * @pld_cb: callback to do link auto resume
 *
 * Resume the wlan bus from runtime suspend.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_core_psoc_bus_runtime_resume(struct wlan_objmgr_psoc *psoc,
					     pmo_pld_auto_resume_cb pld_cb);
#endif

/**
 * pmo_core_psoc_suspend_target() -Send suspend target command
 * @psoc: objmgr psoc handle
 * @disable_target_intr: disable target interrupt
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_core_psoc_suspend_target(struct wlan_objmgr_psoc *psoc,
		int disable_target_intr);

/**
 * pmo_core_psoc_bus_resume_req() - handle bus resume request for psoc
 * @psoc: objmgr psoc handle
 * @type: is this suspend part of runtime suspend or system suspend?
 *
 * Return:QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_core_psoc_bus_resume_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type);

/**
 * pmo_core_vdev_set_restore_dtim() - vdev dtim restore setting value
 * @vdev: objmgr vdev handle
 * @value: dtim restore policy value
 *
 * Return: None
 */
static inline
void pmo_core_vdev_set_restore_dtim(struct wlan_objmgr_vdev *vdev,
				    bool value)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->restore_dtim_setting = value;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
}

/**
 * pmo_core_vdev_get_restore_dtim() - Get vdev restore dtim setting
 * @vdev: objmgr vdev handle
 *
 * Return: dtim restore policy
 */
static inline
bool pmo_core_vdev_get_restore_dtim(struct wlan_objmgr_vdev *vdev)
{
	bool value;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	value = vdev_ctx->restore_dtim_setting;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return value;
}

#ifdef FEATURE_WLAN_DYNAMIC_ARP_NS_OFFLOAD
/**
 * pmo_core_dynamic_arp_ns_offload_enable() - Enable vdev arp/ns offload
 * @vdev: objmgr vdev handle
 *
 * Return: QDF_STATUS_E_ALREADY if arp/ns offload already enable
 */
static inline QDF_STATUS
pmo_core_dynamic_arp_ns_offload_enable(struct wlan_objmgr_vdev *vdev)
{
	bool value;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	value = vdev_ctx->dyn_arp_ns_offload_disable;
	if (!value)
		status = QDF_STATUS_E_ALREADY;
	else
		vdev_ctx->dyn_arp_ns_offload_disable = false;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return status;
}

/**
 * pmo_core_dynamic_arp_ns_offload_disable() - Disable vdev arp/ns offload
 * @vdev: objmgr vdev handle
 *
 * Return: QDF_STATUS_E_ALREADY if arp/ns offload already disable
 */
static inline QDF_STATUS
pmo_core_dynamic_arp_ns_offload_disable(struct wlan_objmgr_vdev *vdev)
{
	bool value;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	value = vdev_ctx->dyn_arp_ns_offload_disable;
	if (value)
		status = QDF_STATUS_E_ALREADY;
	else
		vdev_ctx->dyn_arp_ns_offload_disable = true;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return status;
}

/**
 * pmo_core_get_dynamic_arp_ns_offload_disable() - Get arp/ns offload state
 * @vdev: objmgr vdev handle
 *
 * Return: true if vdev arp/ns offload is disable
 */
static inline bool
pmo_core_get_dynamic_arp_ns_offload_disable(struct wlan_objmgr_vdev *vdev)
{
	bool value;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	value = vdev_ctx->dyn_arp_ns_offload_disable;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return value;
}

/**
 * pmo_core_dynamic_arp_ns_offload_runtime_prevent() - Prevent runtime suspend
 * @vdev: objmgr vdev handle
 *
 * API to prevent runtime suspend happen when arp/ns offload is disable
 *
 * Return: None
 */
static inline void
pmo_core_dynamic_arp_ns_offload_runtime_prevent(struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_runtime_pm_prevent_suspend(&vdev_ctx->dyn_arp_ns_offload_rt_lock);
}

/**
 * pmo_core_dynamic_arp_ns_offload_runtime_allow() - Allow runtime suspend
 * @vdev: objmgr vdev handle
 *
 * API to allow runtime suspend happen when arp/ns offload is enable
 *
 * Return: None
 */
static inline void
pmo_core_dynamic_arp_ns_offload_runtime_allow(struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_runtime_pm_allow_suspend(&vdev_ctx->dyn_arp_ns_offload_rt_lock);
}
#endif

/**
 * pmo_core_psoc_update_power_save_mode() - update power save mode
 * @psoc: objmgr psoc handle
 * @value:describe vdev power save mode
 *
 * Return: None
 */
static inline void
pmo_core_psoc_update_power_save_mode(struct wlan_objmgr_psoc *psoc,
				     uint8_t value)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->psoc_cfg.power_save_mode = value;
	}
}

/**
 * pmo_core_psoc_get_power_save_mode() - Get psoc power save mode
 * @psoc: objmgr psoc handle
 *
 * Return: vdev psoc power save mode value
 */
static inline uint8_t
pmo_core_psoc_get_power_save_mode(struct wlan_objmgr_psoc *psoc)
{
	uint8_t value = 0;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		value = psoc_ctx->psoc_cfg.power_save_mode;
	}

	return value;
}

/**
 * pmo_core_vdev_get_pause_bitmap() - Get vdev pause bitmap
 * @psoc_ctx: psoc priv ctx
 * @vdev_id: vdev id
 *
 * Return: vdev pause bitmap
 */
static inline
uint16_t pmo_core_vdev_get_pause_bitmap(struct pmo_psoc_priv_obj *psoc_ctx,
		uint8_t vdev_id)
{
	uint16_t value = 0;
	pmo_get_pause_bitmap handler;

	qdf_spin_lock_bh(&psoc_ctx->lock);
	handler = psoc_ctx->get_pause_bitmap;
	qdf_spin_unlock_bh(&psoc_ctx->lock);

	if (handler)
		value = handler(vdev_id);

	return value;
}

/**
 * pmo_is_vdev_in_ap_mode() - check that vdev is in ap mode or not
 * @vdev: objmgr vdev handle
 *
 * Helper function to know whether given vdev is in AP mode or not.
 *
 * Return: True/False
 */
static inline
bool pmo_is_vdev_in_ap_mode(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE mode;

	mode = pmo_get_vdev_opmode(vdev);

	return (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE) == 1 ? 1 : 0;
}

/**
 * pmo_core_psoc_handle_initial_wake_up() - handle initial wake up
 * @cb_ctx: callback context
 *
 * Return: None
 */
void pmo_core_psoc_handle_initial_wake_up(void *cb_ctx);

/**
 * pmo_core_psoc_is_target_wake_up_received() - check for initial wake up
 * @psoc: objmgr psoc handle
 *
 * Check if target initial wake up is received and fail PM suspend gracefully
 *
 * Return: -EAGAIN if initial wake up is received else 0
 */
int pmo_core_psoc_is_target_wake_up_received(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_psoc_clear_target_wake_up() - clear initial wake up
 * @psoc: objmgr psoc handle
 *
 * Clear target initial wake up reason
 *
 * Return: 0 for success and negative error code for failure
 */
int pmo_core_psoc_clear_target_wake_up(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_psoc_target_suspend_acknowledge() - update target susspend status
 * @context: HTC_INIT_INFO->context
 * @wow_nack: true when wow is rejected
 * @reason_code : WoW status reason code
 *
 * Return: none
 */
void pmo_core_psoc_target_suspend_acknowledge(void *context, bool wow_nack,
					      uint16_t reason_code);

/**
 * pmo_core_psoc_wakeup_host_event_received() - received host wake up event
 * @psoc: objmgr psoc handle
 *
 * Return: None
 */
void pmo_core_psoc_wakeup_host_event_received(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_config_listen_interval() - function to dynamically configure
 * listen interval
 * @vdev: objmgr vdev
 * @listen_interval: new listen interval passed by user
 *
 * This function allows user to configure listen interval dynamically
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_config_listen_interval(struct wlan_objmgr_vdev *vdev,
					   uint32_t listen_interval);

/**
 * pmo_core_config_modulated_dtim() - function to configure modulated dtim
 * @vdev: objmgr vdev handle
 * @mod_dtim: New modulated dtim value passed by user
 *
 * This function configures the modulated dtim in firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_config_modulated_dtim(struct wlan_objmgr_vdev *vdev,
					  uint32_t mod_dtim);

/**
 * pmo_core_txrx_suspend() - suspends TXRX
 * @psoc: objmgr psoc handle
 *
 * This function disables the EXT grp irqs and drains the TX/RX pipes;
 * this essentially suspends the TXRX activity
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_txrx_suspend(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_txrx_resume() - resumes TXRX
 * @psoc: objmgr psoc handle
 *
 * This function enables the EXT grp irqs, which inturn resumes
 * the TXRX activity
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_txrx_resume(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_config_forced_dtim() - function to configure forced dtim
 * @vdev: objmgr vdev handle
 * @dynamic_dtim: dynamic dtim value passed by user
 *
 * This function configures the forced modulated dtim in firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_config_forced_dtim(struct wlan_objmgr_vdev *vdev,
				       uint32_t dynamic_dtim);

#ifdef SYSTEM_PM_CHECK
/**
 * pmo_core_system_resume() - function to handle system resume notification
 * @psoc: objmgr psoc handle
 *
 * Return: None
 */
void pmo_core_system_resume(struct wlan_objmgr_psoc *psoc);
#else
static inline void pmo_core_system_resume(struct wlan_objmgr_psoc *psoc)
{}
#endif
#ifdef WLAN_FEATURE_IGMP_OFFLOAD
/**
 * pmo_core_enable_igmp_offload() - function to offload igmp
 * @vdev: objmgr vdev handle
 * @pmo_igmp_req: igmp req
 *
 * This function to offload igmp to fw
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pmo_core_enable_igmp_offload(struct wlan_objmgr_vdev *vdev,
			     struct pmo_igmp_offload_req *pmo_igmp_req);
#else
static inline QDF_STATUS
pmo_core_enable_igmp_offload(struct wlan_objmgr_vdev *vdev,
			     struct pmo_igmp_offload_req *pmo_igmp_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * pmo_core_vdev_get_moddtim_user_enabled() - Get vdev if mod dtim set
 * by user
 * @vdev: objmgr vdev handle
 *
 * Return: mod dtim set by user or not
 */
static inline
bool pmo_core_vdev_get_moddtim_user_enabled(struct wlan_objmgr_vdev *vdev)
{
	bool value;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	value = vdev_ctx->dyn_modulated_dtim_enabled;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return value;
}

/**
 * pmo_core_vdev_set_moddtim_user_enabled() - vdev moddtim user enable setting
 * @vdev: objmgr vdev handle
 * @value: vdev moddtim user enable or not
 *
 * Return: None
 */
static inline
void pmo_core_vdev_set_moddtim_user_enabled(struct wlan_objmgr_vdev *vdev,
					    bool value)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->dyn_modulated_dtim_enabled = value;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
}

/**
 * pmo_core_vdev_get_moddtim_user_active() - Get vdev if moddtim user is
 * sent to fw
 * @vdev: objmgr vdev handle
 *
 * Return: moddtim user is sent to fw or not
 */
static inline
bool pmo_core_vdev_get_moddtim_user_active(struct wlan_objmgr_vdev *vdev)
{
	bool retval;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	retval = vdev_ctx->is_dyn_modulated_dtim_activated;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return retval;
}

/**
 * pmo_core_vdev_set_moddtim_user_active() - vdev moddtim user active setting
 * @vdev: objmgr vdev handle
 * @value: vdev moddtim user active or not
 *
 * Return: None
 */
static inline
void pmo_core_vdev_set_moddtim_user_active(struct wlan_objmgr_vdev *vdev,
					   bool value)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->is_dyn_modulated_dtim_activated = value;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
}

/**
 * pmo_core_vdev_get_moddtim_user() - Get vdev moddtim set by user
 * @vdev: objmgr vdev handle
 *
 * Return: moddtim value set by user
 */
static inline
uint32_t pmo_core_vdev_get_moddtim_user(struct wlan_objmgr_vdev *vdev)
{
	uint32_t value;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	value = vdev_ctx->dyn_modulated_dtim;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return value;
}

/**
 * pmo_core_vdev_set_moddtim_user() - vdev moddtim user value setting
 * @vdev: objmgr vdev handle
 * @value: vdev moddtim value set by user
 *
 * Return: None
 */
static inline
void pmo_core_vdev_set_moddtim_user(struct wlan_objmgr_vdev *vdev,
				    uint32_t value)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->dyn_modulated_dtim = value;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
}
#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_SUSPEND_RESUME_H_ */

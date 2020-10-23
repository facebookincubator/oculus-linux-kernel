/*
 * Copyright (c) 2014-2017 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
#if !defined(__CDS_API_H)
#define __CDS_API_H

/**
 * DOC:  cds_api.h
 *
 * Connectivity driver services public API
 *
 */

#include <qdf_types.h>
#include <qdf_status.h>
#include <qdf_mem.h>
#include <qdf_debugfs.h>
#include <qdf_list.h>
#include <qdf_trace.h>
#include <qdf_event.h>
#include <qdf_lock.h>
#include <cds_reg_service.h>
#include <cds_mq.h>
#include <cds_packet.h>
#include <cds_sched.h>
#include <qdf_threads.h>
#include <qdf_mc_timer.h>
#include <cds_pack_align.h>

/* Amount of time to wait for WMA to perform an asynchronous activity.
 * This value should be larger than the timeout used by WMI to wait for
 * a response from target
 */
#define CDS_WMA_TIMEOUT  (15000)

/**
 * enum cds_driver_state - Driver state
 * @CDS_DRIVER_STATE_UNINITIALIZED: Driver is in uninitialized state.
 * CDS_DRIVER_STATE_LOADED: Driver is loaded and functional.
 * CDS_DRIVER_STATE_LOADING: Driver probe is in progress.
 * CDS_DRIVER_STATE_UNLOADING: Driver remove is in progress.
 * CDS_DRIVER_STATE_RECOVERING: Recovery in progress.
 * CDS_DRIVER_STATE_MODULE_STOPPING: Module stop in progress.
 */
enum cds_driver_state {
	CDS_DRIVER_STATE_UNINITIALIZED		= 0,
	CDS_DRIVER_STATE_LOADED			= BIT(0),
	CDS_DRIVER_STATE_LOADING		= BIT(1),
	CDS_DRIVER_STATE_UNLOADING		= BIT(2),
	CDS_DRIVER_STATE_RECOVERING		= BIT(3),
	CDS_DRIVER_STATE_FW_READY		= BIT(4),
	CDS_DRIVER_STATE_MODULE_STOPPING	= BIT(5),
};

#define __CDS_IS_DRIVER_STATE(_state, _mask) (((_state) & (_mask)) == (_mask))

/**
 * enum cds_fw_state - Firmware state
 * @CDS_FW_STATE_UNINITIALIZED: Firmware is in uninitialized state.
 * CDS_FW_STATE_DOWN: Firmware is down.
 */
enum cds_fw_state {
	CDS_FW_STATE_UNINITIALIZED = 0,
	CDS_FW_STATE_DOWN,
};

#define __CDS_IS_FW_STATE(_state, _mask) (((_state) & (_mask)) == (_mask))

/**
 * struct cds_sme_cbacks - list of sme functions registered with
 * CDS
 * @sme_get_valid_channels: gets the valid channel list for
 *  				   current reg domain
 * @sme_get_nss_for_vdev: gets the nss allowed for the vdev type
 */
struct cds_sme_cbacks {
	QDF_STATUS (*sme_get_valid_channels)(void*, uint16_t,
		uint8_t *, uint32_t *);
	void (*sme_get_nss_for_vdev)(void*, enum tQDF_ADAPTER_MODE,
		uint8_t *, uint8_t *);
};

/**
 * struct cds_dp_cbacks - list of datapath functions registered with CDS
 * @ol_txrx_update_mac_id_cb: updates mac_id for vdev
 * @hdd_en_lro_in_cc_cb: enables LRO if concurrency is not active
 * @hdd_disble_lro_in_cc_cb: disables LRO due to concurrency
 * @hdd_set_rx_mode_rps_cb: enable/disable RPS in SAP mode
 */
struct cds_dp_cbacks {
	void (*ol_txrx_update_mac_id_cb)(uint8_t , uint8_t);
	void (*hdd_en_lro_in_cc_cb)(struct hdd_context_s *);
	void (*hdd_disble_lro_in_cc_cb)(struct hdd_context_s *);
	void (*hdd_set_rx_mode_rps_cb)(struct hdd_context_s *, void *, bool);
};

void cds_set_driver_state(enum cds_driver_state);
void cds_clear_driver_state(enum cds_driver_state);
enum cds_driver_state cds_get_driver_state(void);

/**
 * cds_set_fw_state() - Set current firmware state
 * @state:	Firmware state to be set to.
 *
 * This API sets firmware state to state. This API only sets the state and
 * doesn't clear states, please make sure to use cds_clear_firmware_state
 * to clear any state if required.
 *
 * Return: None
 */
void cds_set_fw_state(enum cds_fw_state);

/**
 * cds_clear_fw_state() - Clear current fw state
 * @state:	Driver state to be cleared.
 *
 * This API clears fw state. This API only clears the state, please make
 * sure to use cds_set_fw_state to set any new states.
 *
 * Return: None
 */
void cds_clear_fw_state(enum cds_fw_state);

/**
 * cds_get_fw_state() - Get current firmware state
 *
 * This API returns current firmware state stored in global context.
 *
 * Return: Firmware state enum
 */
enum cds_fw_state cds_get_fw_state(void);


/**
 * cds_is_driver_loading() - Is driver load in progress
 *
 * Return: true if driver is loading and false otherwise.
 */
static inline bool cds_is_driver_loading(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_LOADING);
}

/**
 * cds_is_driver_unloading() - Is driver unload in progress
 *
 * Return: true if driver is unloading and false otherwise.
 */
static inline bool cds_is_driver_unloading(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_UNLOADING);
}

/**
 * cds_is_driver_recovering() - Is recovery in progress
 *
 * Return: true if recovery in progress  and false otherwise.
 */
static inline bool cds_is_driver_recovering(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_RECOVERING);
}

/**
 * cds_is_load_or_unload_in_progress() - Is driver load OR unload in progress
 *
 * Return: true if driver is loading OR unloading and false otherwise.
 */
static inline bool cds_is_load_or_unload_in_progress(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_LOADING) ||
		__CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_UNLOADING);
}

/**
 * cds_is_fw_down() - Is FW down or not
 *
 * Return: true if FW is down and false otherwise.
 */
static inline bool cds_is_fw_down(void)
{
	enum cds_fw_state state = cds_get_fw_state();

	return __CDS_IS_FW_STATE(state, BIT(CDS_FW_STATE_DOWN));
}

/**
 * cds_is_target_ready() - Is target is in ready state
 *
 * Return: true if target is in ready state and false otherwise.
 */
static inline bool cds_is_target_ready(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_FW_READY);
}

/**
 * cds_is_module_stop_in_progress() - Is module stopping
 *
 * Return: true if module stop is in progress.
 */
static inline bool cds_is_module_stop_in_progress(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_MODULE_STOPPING);
}

/**
 * cds_is_module_state_transitioning() - Is module state transitioning
 *
 * Return: true if module stop is in progress.
 */
static inline int cds_is_module_state_transitioning(void)
{
	if (cds_is_load_or_unload_in_progress() || cds_is_driver_recovering() ||
		cds_is_module_stop_in_progress()) {
		pr_info("%s: Load/Unload %d or recovery %d or module_stop %d is in progress",
			__func__, cds_is_load_or_unload_in_progress(),
				cds_is_driver_recovering(),
				cds_is_module_stop_in_progress());
		return true;
	} else {
		return false;
	}
}

/**
 * cds_set_recovery_in_progress() - Set recovery in progress
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_recovery_in_progress(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_RECOVERING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_RECOVERING);
}

/**
 * cds_set_target_ready() - Set target ready state
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_target_ready(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_FW_READY);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_FW_READY);
}

/**
 * cds_set_load_in_progress() - Set load in progress
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_load_in_progress(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_LOADING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_LOADING);
}

/**
 * cds_set_driver_loaded() - Set load completed
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_driver_loaded(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_LOADED);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_LOADED);
}

/**
 * cds_set_unload_in_progress() - Set unload in progress
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_unload_in_progress(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_UNLOADING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_UNLOADING);
}

/**
 * cds_set_module_stop_in_progress() - Setting module stop in progress
 *
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_module_stop_in_progress(bool value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_MODULE_STOPPING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_MODULE_STOPPING);
}

/**
 * cds_is_driver_loaded() - Is driver loaded
 *
 * Return: true if driver is loaded or false otherwise.
 */
static inline bool cds_is_driver_loaded(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_LOADED);
}

v_CONTEXT_t cds_init(void);
void cds_deinit(void);

QDF_STATUS cds_pre_enable(v_CONTEXT_t cds_context);

QDF_STATUS cds_open(void);

QDF_STATUS cds_enable(v_CONTEXT_t cds_context);

QDF_STATUS cds_disable(v_CONTEXT_t cds_context);

QDF_STATUS cds_post_disable(void);

QDF_STATUS cds_close(v_CONTEXT_t cds_context);

void cds_core_return_msg(void *pVContext, p_cds_msg_wrapper pMsgWrapper);

void *cds_get_context(QDF_MODULE_ID moduleId);

v_CONTEXT_t cds_get_global_context(void);

QDF_STATUS cds_alloc_context(void *p_cds_context, QDF_MODULE_ID moduleID,
			     void **ppModuleContext, uint32_t size);

QDF_STATUS cds_free_context(void *p_cds_context, QDF_MODULE_ID moduleID,
			    void *pModuleContext);

QDF_STATUS cds_set_context(QDF_MODULE_ID module_id, void *context);

QDF_STATUS cds_get_vdev_types(enum tQDF_ADAPTER_MODE mode, uint32_t *type,
			      uint32_t *subType);

void cds_flush_work(void *work);
void cds_flush_delayed_work(void *dwork);

bool cds_is_packet_log_enabled(void);

uint64_t cds_get_monotonic_boottime(void);

/**
 * cds_get_recovery_reason() - get self recovery reason
 * @reason: cds hang reason
 *
 * Return: None
 */
void cds_get_recovery_reason(enum cds_hang_reason *reason);

/**
 * cds_reset_recovery_reason() - reset the reason to unspecified
 *
 * Return: None
 */
void cds_reset_recovery_reason(void);

/**
 * cds_trigger_recovery() - trigger self recovery
 * @reason: recovery reason
 *
 * Return: none
 */
void cds_trigger_recovery(enum cds_hang_reason reason);

void cds_set_wakelock_logging(bool value);
bool cds_is_wakelock_enabled(void);
void cds_set_ring_log_level(uint32_t ring_id, uint32_t log_level);
enum wifi_driver_log_level cds_get_ring_log_level(uint32_t ring_id);
void cds_set_multicast_logging(uint8_t value);
uint8_t cds_is_multicast_logging(void);
QDF_STATUS cds_set_log_completion(uint32_t is_fatal,
		uint32_t type,
		uint32_t sub_type,
		bool recovery_needed);
void cds_get_and_reset_log_completion(uint32_t *is_fatal,
		uint32_t *type,
		uint32_t *sub_type,
		bool *recovery_needed);
bool cds_is_log_report_in_progress(void);
bool cds_is_fatal_event_enabled(void);
uint32_t cds_get_log_indicator(void);
void cds_set_fatal_event(bool value);
void cds_wlan_flush_host_logs_for_fatal(void);

void cds_init_log_completion(void);
QDF_STATUS cds_flush_logs(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code,
		bool dump_mac_trace,
		bool recovery_needed);
void cds_logging_set_fw_flush_complete(void);
void cds_svc_fw_shutdown_ind(struct device *dev);
#ifdef FEATURE_WLAN_DIAG_SUPPORT
void cds_tdls_tx_rx_mgmt_event(uint8_t event_id, uint8_t tx_rx,
			uint8_t type, uint8_t sub_type, uint8_t *peer_mac);
#else
static inline
void cds_tdls_tx_rx_mgmt_event(uint8_t event_id, uint8_t tx_rx,
			uint8_t type, uint8_t sub_type, uint8_t *peer_mac)

{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

int cds_get_radio_index(void);
QDF_STATUS cds_set_radio_index(int radio_index);
void cds_init_ini_config(struct cds_config_info *cds_cfg);
void cds_deinit_ini_config(void);
struct cds_config_info *cds_get_ini_config(void);

bool cds_is_5_mhz_enabled(void);
bool cds_is_10_mhz_enabled(void);
bool cds_is_sub_20_mhz_enabled(void);
bool cds_is_self_recovery_enabled(void);
void cds_pkt_stats_to_logger_thread(void *pl_hdr, void *pkt_dump, void *data);
QDF_STATUS cds_register_dp_cb(struct cds_dp_cbacks *dp_cbs);
QDF_STATUS cds_deregister_dp_cb(void);

uint32_t cds_get_arp_stats_gw_ip(void);
void cds_incr_arp_stats_tx_tgt_delivered(void);
void cds_incr_arp_stats_tx_tgt_acked(void);
#endif /* if !defined __CDS_API_H */

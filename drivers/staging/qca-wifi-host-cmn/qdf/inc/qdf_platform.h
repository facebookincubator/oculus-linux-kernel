/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: qdf_platform.h
 * This file defines platform API abstractions.
 */

#ifndef _QDF_PLATFORM_H
#define _QDF_PLATFORM_H

#include "qdf_types.h"

/**
 * typedef qdf_self_recovery_callback() - callback for self recovery
 * @psoc: pointer to the posc object
 * @reason: the reason for the recovery request
 * @func: the caller's function name
 * @line: the line number of the callsite
 *
 * Return: none
 */
typedef void (*qdf_self_recovery_callback)(void *psoc,
					   enum qdf_hang_reason reason,
					   const char *func,
					   const uint32_t line);

/**
 * typedef qdf_is_fw_down_callback() - callback to query if fw is down
 *
 * Return: true if fw is down and false if fw is not down
 */
typedef bool (*qdf_is_fw_down_callback)(void);

/**
 * qdf_register_fw_down_callback() - API to register fw down callback
 * @is_fw_down: callback to query if fw is down or not
 *
 * Return: none
 */
void qdf_register_fw_down_callback(qdf_is_fw_down_callback is_fw_down);

/**
 * qdf_is_fw_down() - API to check if fw is down or not
 *
 * Return: true: if fw is down
 *	   false: if fw is not down
 */
bool qdf_is_fw_down(void);

/**
 * typedef qdf_wmi_recv_qmi_cb() - callback to receive WMI over QMI
 * @cb_ctx: WMI event recv callback context(wmi_handle)
 * @buf: WMI buffer
 * @len: WMI buffer len
 *
 * Return: 0 if success otherwise -EINVAL
 */
typedef int (*qdf_wmi_recv_qmi_cb)(void *cb_ctx, void *buf, int len);

/**
 * typedef qdf_qmi_ind_cb() - callback to receive QMI Indication
 * @cb_ctx: QMI indication callback context
 * @type: Indication type
 * @event: Indication Event
 * @event_len: QMI indication event buffer len
 *
 * Return: 0 if success otherwise -EINVAL
 */
typedef int (*qdf_qmi_ind_cb)(void *cb_ctx, uint16_t type,
			      void *event, int event_len);

/**
 * typedef qdf_wmi_send_over_qmi_callback() - callback to send WMI over QMI
 * @buf: WMI buffer
 * @len: WMI buffer len
 * @cb_ctx: WMI event recv callback context(wmi_handle)
 * @wmi_rx_cb: WMI event receive call back
 *
 * Return: QDF_STATUS_SUCCESS if success otherwise QDF error code
 */
typedef QDF_STATUS (*qdf_wmi_send_over_qmi_callback)(void *buf, uint32_t len,
						     void *cb_ctx,
						     qdf_wmi_recv_qmi_cb
						     wmi_rx_cb);

/**
 * typedef qdf_send_ind_over_qmi_callback() - callback to receive QMI Indication
 * @cb_ctx: QMI Indication recv callback context
 * @qmi_ind_cb: QMI Indication receive callback
 *
 * Return: QDF_STATUS_SUCCESS if success otherwise QDF error code
 */
typedef QDF_STATUS (*qdf_send_ind_over_qmi_callback)(void *cb_ctx,
						     qdf_qmi_ind_cb qmi_ind_cb);

/**
 * qdf_register_wmi_send_recv_qmi_callback() - Register WMI over QMI callback
 * @wmi_send_recv_qmi_cb: callback to send recv WMI data over QMI
 *
 * Return: none
 */
void qdf_register_wmi_send_recv_qmi_callback(qdf_wmi_send_over_qmi_callback
					     wmi_send_recv_qmi_cb);

/**
 * qdf_register_qmi_indication_callback() - Register QMI Indication callback
 * @qmi_ind_cb: callback to receive QMI Indications
 *
 * Return: none
 */
void qdf_register_qmi_indication_callback(qdf_send_ind_over_qmi_callback qmi_ind_cb);

/**
 * qdf_wmi_send_recv_qmi() - API to send receive WMI data over QMI
 * @buf: WMI buffer
 * @len: WMI buffer len
 * @cb_ctx: WMI event recv callback context(wmi_handle)
 * @wmi_rx_cb: WMI event receive call back
 *
 * Return: QDF STATUS of operation
 */
QDF_STATUS qdf_wmi_send_recv_qmi(void *buf, uint32_t len, void *cb_ctx,
				 qdf_wmi_recv_qmi_cb wmi_rx_cb);

/**
 * qdf_reg_qmi_indication() - API to receive QMI Indication data
 * @cb_ctx: QMI Indication recv callback context
 * @qmi_ind_cb: QMI Indication event receive callback
 *
 * Return: QDF STATUS of operation
 */
QDF_STATUS qdf_reg_qmi_indication(void *cb_ctx, qdf_qmi_ind_cb qmi_ind_cb);

/**
 * typedef qdf_is_driver_unloading_callback() - callback to get driver
 *                                              unloading in progress or not
 *
 * Return: true if driver is unloading else false
 */
typedef bool (*qdf_is_driver_unloading_callback)(void);

/**
 * qdf_register_is_driver_unloading_callback() - driver unloading callback
 * @callback:  driver unloading callback
 *
 * Return: None
 */
void qdf_register_is_driver_unloading_callback(
				qdf_is_driver_unloading_callback callback);

/**
 * typedef qdf_is_driver_state_module_stop_callback() - callback to get driver
 *                                                 state is module stop or not
 *
 * Return: true if driver state is module stop else false
 */
typedef bool (*qdf_is_driver_state_module_stop_callback)(void);

/**
 * qdf_register_is_driver_state_module_stop_callback() - driver state is
 * module stop or not
 * @callback:  driver state module stop callback
 *
 * Return: None
 */
void qdf_register_is_driver_state_module_stop_callback(
			qdf_is_driver_state_module_stop_callback callback);

/**
 * qdf_register_self_recovery_callback() - register self recovery callback
 * @callback:  self recovery callback
 *
 * Return: None
 */
void qdf_register_self_recovery_callback(qdf_self_recovery_callback callback);

/**
 * qdf_trigger_self_recovery () - trigger self recovery
 * @psoc: the psoc at which the recovery is being triggered
 * @reason: the reason for the recovery request
 *
 * Call API only in case of fatal error,
 * if self_recovery_cb callback is registered, injcets fw crash and recovers
 * else raises QDF_BUG()
 *
 * Return: None
 */
#define qdf_trigger_self_recovery(psoc, reason) \
	__qdf_trigger_self_recovery(psoc, reason, __func__, __LINE__)
void __qdf_trigger_self_recovery(void *psoc, enum qdf_hang_reason reason,
				 const char *func, const uint32_t line);

/**
 * typedef qdf_is_recovering_callback() - callback to get driver recovering in
 * progress or not
 *
 * Return: true if driver is doing recovering else false
 */
typedef bool (*qdf_is_recovering_callback)(void);

/**
 * qdf_register_recovering_state_query_callback() - register recover status
 * query callback
 * @is_recovering: true if driver is recovering
 *
 * Return: none
 */
void qdf_register_recovering_state_query_callback(
	qdf_is_recovering_callback is_recovering);

/**
 * qdf_is_driver_unloading() - get driver unloading in progress status
 * or not
 *
 * Return: true if driver is unloading else false
 */
bool qdf_is_driver_unloading(void);

/**
 * qdf_is_driver_state_module_stop() - get driver state is module stop or not
 *
 * Return: true if driver state is module stop else false
 */
bool qdf_is_driver_state_module_stop(void);

/**
 * qdf_is_recovering() - get driver recovering in progress status
 * or not
 *
 * Return: true if driver is doing recovering else false
 */
bool qdf_is_recovering(void);

/*
 * struct qdf_op_sync - opaque operation synchronization context handle
 */
struct qdf_op_sync;

typedef int (*qdf_op_protect_cb)(void **out_sync, const char *func);
typedef void (*qdf_op_unprotect_cb)(void *sync, const char *func);

/**
 * qdf_op_protect() - attempt to protect a driver operation
 * @out_sync: output parameter for the synchronization context, populated on
 *	success
 *
 * Return: Errno
 */
#define qdf_op_protect(out_sync) __qdf_op_protect(out_sync, __func__)

qdf_must_check int
__qdf_op_protect(struct qdf_op_sync **out_sync, const char *func);

/**
 * qdf_op_unprotect() - release driver operation protection
 * @sync: synchronization context returned from qdf_op_protect()
 *
 * Return: None
 */
#define qdf_op_unprotect(sync) __qdf_op_unprotect(sync, __func__)

void __qdf_op_unprotect(struct qdf_op_sync *sync, const char *func);

/**
 * qdf_op_callbacks_register() - register driver operation protection callbacks
 * @on_protect: callback on protect
 * @on_unprotect: callback on unprotect
 *
 * Return: None
 */
void qdf_op_callbacks_register(qdf_op_protect_cb on_protect,
			       qdf_op_unprotect_cb on_unprotect);

/**
 * typedef qdf_is_drv_connected_callback() - callback to query if drv
 *                                           is connected
 *
 * Return: true if drv is connected else false
 */
typedef bool (*qdf_is_drv_connected_callback)(void);

/**
 * qdf_is_drv_connected() - API to check if drv is connected or not
 *
 * DRV is dynamic request voting using which fw can do page fault and
 * bring in page back without apps wake up
 *
 * Return: true: if drv is connected
 *	   false: if drv is not connected
 */
bool qdf_is_drv_connected(void);

/**
 * qdf_register_drv_connected_callback() - API to register drv connected cb
 * @is_drv_connected: callback to query if drv is connected or not
 *
 * Return: none
 */
void qdf_register_drv_connected_callback(qdf_is_drv_connected_callback
					 is_drv_connected);

/**
 * qdf_check_state_before_panic() - API to check if FW is down
 * or driver is in recovery before calling assert
 * @func: Caller function pointer used for debug info
 * @line: Caller function line number
 *
 * Return: none
 */
void qdf_check_state_before_panic(const char *func, const uint32_t line);

/**
 *typedef qdf_is_drv_supported_callback() - callback to query if drv is supported
 *
 * Return: true if drv is supported else false
 */
typedef bool (*qdf_is_drv_supported_callback)(void);

/**
 * qdf_is_drv_supported() - API to check if drv is supported or not
 *
 * DRV is dynamic request voting using which fw can do page fault and
 * bring in page back without apps wake up
 *
 * Return: true: if drv is supported
 *	   false: if drv is not supported
 */
bool qdf_is_drv_supported(void);

/**
 * qdf_register_drv_supported_callback() - API to register drv supported cb
 * @is_drv_supported: callback to query if drv is supported or not
 *
 * Return: none
 */
void qdf_register_drv_supported_callback(qdf_is_drv_supported_callback
					 is_drv_supported);

/**
 * typedef qdf_recovery_reason_update_callback() - recovery reason update callback
 * @reason: recovery reason
 */
typedef void (*qdf_recovery_reason_update_callback)(enum qdf_hang_reason
						    reason);

/**
 * qdf_register_recovery_reason_update() - Register callback to update recovery
 *                                         reason
 * @callback: callback to update recovery reason
 *
 * Return: none
 */
void qdf_register_recovery_reason_update(qdf_recovery_reason_update_callback
					 callback);

/**
 * qdf_recovery_reason_update() - update recovery reason
 * @reason: recovery reason
 *
 * Return: none
 */
void qdf_recovery_reason_update(enum qdf_hang_reason reason);

/**
 * typedef qdf_bus_reg_dump() - callback for getting bus specific register dump
 * @dev: Bus specific device
 * @buf: Hang event buffer in which the data will be populated
 * @len: length of data to be populated in the hang event buffer
 *
 * Return: none
 */
typedef void (*qdf_bus_reg_dump)(struct device *dev, uint8_t *buf,
				 uint32_t len);

/**
 * qdf_register_get_bus_reg_dump() - Register callback to update bus register
 *                                   dump
 * @callback: callback to update bus register dump
 *
 * Return: none
 */
void qdf_register_get_bus_reg_dump(qdf_bus_reg_dump callback);

/**
 * qdf_get_bus_reg_dump() - Get the register dump for the bus
 * @dev: device
 * @buf: buffer for hang data
 * @len: len of hang data
 *
 * Return: none
 */
void qdf_get_bus_reg_dump(struct device *dev, uint8_t *buf, uint32_t len);

#ifdef WLAN_SUPPORT_DPDK
/**
 * qdf_uio_register_device() - register dev to UIO dev
 * @parent: parent device to be registered with UIO dev
 * @info: UIO device capabilities
 *
 * Return: zero on success or a negative error code
 */
int qdf_uio_register_device(struct device *parent, qdf_uio_info_t *info);

/**
 * qdf_uio_unregister_device - unregister a UIO device
 * @info: UIO device capabilities
 *
 * Return: none
 */
void qdf_uio_unregister_device(qdf_uio_info_t *info);
#endif
#endif /*_QDF_PLATFORM_H*/

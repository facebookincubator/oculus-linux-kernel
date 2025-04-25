/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
  * DOC: wlan_dp_main.h
  *
  *
  */
#ifndef __WLAN_DP_MAIN_H__
#define __WLAN_DP_MAIN_H__

#include "wlan_dp_public_struct.h"
#include "wlan_dp_priv.h"
#include "wlan_dp_objmgr.h"
#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
#include "wlan_fpm_table.h"
#include "wlan_dp_fim.h"
#endif

#define NUM_RX_QUEUES 5

#define dp_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_DP, "enter")
#define dp_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_DP, "exit")

/**
 * dp_allocate_ctx() - Allocate DP context
 *
 */
QDF_STATUS dp_allocate_ctx(void);

/**
 * dp_free_ctx() - Free DP context
 *
 */
void dp_free_ctx(void);

/**
 * dp_get_front_intf_no_lock() - Get the first interface from the intf list
 * This API does not use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @dp_ctx: pointer to the DP context
 * @out_intf: double pointer to pass the next interface
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_get_front_intf_no_lock(struct wlan_dp_psoc_context *dp_ctx,
			  struct wlan_dp_intf **out_intf);

/**
 * dp_get_next_intf_no_lock() - Get the next intf from the intf list
 * This API does not use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @dp_ctx: pointer to the DP context
 * @cur_intf: pointer to the current intf
 * @out_intf: double pointer to pass the next intf
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_get_next_intf_no_lock(struct wlan_dp_psoc_context *dp_ctx,
			 struct wlan_dp_intf *cur_intf,
			 struct wlan_dp_intf **out_intf);

/**
 * __dp_take_ref_and_fetch_front_intf_safe - Helper macro to lock, fetch
 * front and next intf, take ref and unlock.
 * @dp_ctx: the global DP context
 * @dp_intf: an dp_intf pointer to use as a cursor
 * @dp_intf_next: dp_intf pointer to next intf
 *
 */
#define __dp_take_ref_and_fetch_front_intf_safe(dp_ctx, dp_intf, \
						dp_intf_next) \
	qdf_spin_lock_bh(&dp_ctx->intf_list_lock), \
	dp_get_front_intf_no_lock(dp_ctx, &dp_intf), \
	dp_get_next_intf_no_lock(dp_ctx, dp_intf, &dp_intf_next), \
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock)

/**
 * __dp_take_ref_and_fetch_next_intf_safe - Helper macro to lock, fetch next
 * interface, take ref and unlock.
 * @dp_ctx: the global DP context
 * @dp_intf: dp_intf pointer to use as a cursor
 * @dp_intf_next: dp_intf pointer to next interface
 *
 */
#define __dp_take_ref_and_fetch_next_intf_safe(dp_ctx, dp_intf, \
					       dp_intf_next) \
	qdf_spin_lock_bh(&dp_ctx->intf_list_lock), \
	dp_intf = dp_intf_next, \
	dp_get_next_intf_no_lock(dp_ctx, dp_intf, &dp_intf_next), \
	qdf_spin_unlock_bh(&dp_ctx->intf_list_lock)

/**
 * __dp_is_intf_valid - Helper macro to return true/false for valid interface.
 * @_dp_intf: an dp_intf pointer to use as a cursor
 */
#define __dp_is_intf_valid(_dp_intf) !!(_dp_intf)

/**
 * dp_for_each_intf_held_safe - Interface iterator called
 *                                      in a delete safe manner
 * @dp_ctx: the global DP context
 * @dp_intf: an dp_intf pointer to use as a cursor
 * @dp_intf_next: dp_intf pointer to the next interface
 *
 */
#define dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) \
	for (__dp_take_ref_and_fetch_front_intf_safe(dp_ctx, dp_intf, \
						     dp_intf_next); \
	     __dp_is_intf_valid(dp_intf); \
	     __dp_take_ref_and_fetch_next_intf_safe(dp_ctx, dp_intf, \
						    dp_intf_next))

/**
 * dp_get_intf_by_macaddr() - Api to Get interface from MAC address
 * @dp_ctx: DP context
 * @addr: MAC address
 *
 * Return: Pointer to DP interface.
 */
struct wlan_dp_intf*
dp_get_intf_by_macaddr(struct wlan_dp_psoc_context *dp_ctx,
		       struct qdf_mac_addr *addr);

/**
 * dp_get_intf_by_netdev() - Api to Get interface from netdev
 * @dp_ctx: DP context
 * @dev: Pointer to network device
 *
 * Return: Pointer to DP interface.
 */
struct wlan_dp_intf*
dp_get_intf_by_netdev(struct wlan_dp_psoc_context *dp_ctx, qdf_netdev_t dev);

/**
 * dp_get_front_link_no_lock() - Get the first link from the dp links list
 * This API does not use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @dp_intf: DP interface handle
 * @out_link: double pointer to pass the next link
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_get_front_link_no_lock(struct wlan_dp_intf *dp_intf,
			  struct wlan_dp_link **out_link);

/**
 * dp_get_next_link_no_lock() - Get the next link from the link list
 * This API does not use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @dp_intf: DP interface handle
 * @cur_link: pointer to the currentlink
 * @out_link: double pointer to pass the nextlink
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_get_next_link_no_lock(struct wlan_dp_intf *dp_intf,
			 struct wlan_dp_link *cur_link,
			 struct wlan_dp_link **out_link);

/**
 * __dp_take_ref_and_fetch_front_link_safe - Helper macro to lock, fetch
 * front and next link, take ref and unlock.
 * @dp_intf: DP interface handle
 * @dp_link: an dp_link pointer to use as a cursor
 * @dp_link_next: dp_link pointer to nextlink
 */
#define __dp_take_ref_and_fetch_front_link_safe(dp_intf, dp_link, \
						dp_link_next) \
	qdf_spin_lock_bh(&(dp_intf)->dp_link_list_lock), \
	dp_get_front_link_no_lock(dp_intf, &(dp_link)), \
	dp_get_next_link_no_lock(dp_intf, dp_link, &(dp_link_next)), \
	qdf_spin_unlock_bh(&(dp_intf)->dp_link_list_lock)

/**
 * __dp_take_ref_and_fetch_next_link_safe - Helper macro to lock, fetch next
 * interface, take ref and unlock.
 * @dp_intf: DP interface handle
 * @dp_link: dp_link pointer to use as a cursor
 * @dp_link_next: dp_link pointer to next link
 */
#define __dp_take_ref_and_fetch_next_link_safe(dp_intf, dp_link, \
					       dp_link_next) \
	qdf_spin_lock_bh(&(dp_intf)->dp_link_list_lock), \
	dp_link = dp_link_next, \
	dp_get_next_link_no_lock(dp_intf, dp_link, &(dp_link_next)), \
	qdf_spin_unlock_bh(&(dp_intf)->dp_link_list_lock)

/**
 * __dp_is_link_valid - Helper macro to return true/false for valid interface.
 * @_dp_link: an dp_link pointer to use as a cursor
 */
#define __dp_is_link_valid(_dp_link) !!(_dp_link)

/**
 * dp_for_each_link_held_safe - Interface iterator called
 *                                      in a delete safe manner
 * @dp_intf: DP interface handle
 * @dp_link: an dp_link pointer to use as a cursor
 * @dp_link_next: dp_link pointer to the next interface
 *
 */
#define dp_for_each_link_held_safe(dp_intf, dp_link, dp_link_next) \
	for (__dp_take_ref_and_fetch_front_link_safe(dp_intf, dp_link, \
						     dp_link_next); \
	     __dp_is_link_valid(dp_link); \
	     __dp_take_ref_and_fetch_next_link_safe(dp_intf, dp_link, \
						    dp_link_next))

/* MAX iteration count to wait for dp packet process to complete */
#define DP_TASK_MAX_WAIT_CNT  100
/* Milli seconds to wait when packet is getting processed */
#define DP_TASK_WAIT_TIME 200

#define DP_TX_FN_CLR (1 << 0)
#define DP_TX_SAP_STOP (1 << 1)
#define DP_TX_DFS_CAC_BLOCK (1 << 2)
#define WLAN_DP_SUSPEND (1 << 3)

/**
 * dp_wait_complete_tasks: Wait for DP tasks to complete
 * @dp_ctx: DP context pointer
 *
 * This function waits for dp tasks like TX to be completed
 *
 * Return: None
 */
void dp_wait_complete_tasks(struct wlan_dp_psoc_context *dp_ctx);

#define NUM_RX_QUEUES 5

#define dp_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_DP, "enter")
#define dp_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_DP, "exit")

/**
 * __wlan_dp_runtime_suspend() - Runtime suspend DP handler
 * @soc: CDP SoC handle
 * @pdev_id: DP PDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __wlan_dp_runtime_suspend(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * __wlan_dp_runtime_resume() - Runtime suspend DP handler
 * @soc: CDP SoC handle
 * @pdev_id: DP PDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __wlan_dp_runtime_resume(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * __wlan_dp_bus_suspend() - BUS suspend DP handler
 * @soc: CDP SoC handle
 * @pdev_id: DP PDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __wlan_dp_bus_suspend(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * __wlan_dp_bus_resume() - BUS resume DP handler
 * @soc: CDP SoC handle
 * @pdev_id: DP PDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __wlan_dp_bus_resume(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * wlan_dp_txrx_soc_attach() - Datapath soc attach
 * @params: SoC attach params
 * @is_wifi3_0_target: [OUT] Pointer to update if the target is wifi3.0
 *
 * Return: SoC handle
 */
void *wlan_dp_txrx_soc_attach(struct dp_txrx_soc_attach_params *params,
			      bool *is_wifi3_0_target);

/**
 * wlan_dp_txrx_soc_detach() - Datapath SoC detach
 * @soc: DP SoC handle
 *
 * Return: None
 */
void wlan_dp_txrx_soc_detach(ol_txrx_soc_handle soc);

/**
 * wlan_dp_txrx_attach_target() - DP target attach
 * @soc: DP SoC handle
 * @pdev_id: DP pdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_txrx_attach_target(ol_txrx_soc_handle soc, uint8_t pdev_id);

/**
 * wlan_dp_txrx_pdev_attach() - DP pdev attach
 * @soc: DP SoC handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_txrx_pdev_attach(ol_txrx_soc_handle soc);

/**
 * wlan_dp_txrx_pdev_detach() - DP pdev detach
 * @soc: DP SoC handle
 * @pdev_id: DP pdev id
 * @force: indicates if force detach is to be done or not
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_txrx_pdev_detach(ol_txrx_soc_handle soc, uint8_t pdev_id,
				    int force);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_link_switch_notification() - DP notifier for MLO link switch
 * @vdev: Objmgr vdev handle
 * @lswitch_req: Link switch request params
 * @notify_reason: Reason of notification
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_link_switch_notification(struct wlan_objmgr_vdev *vdev,
			    struct wlan_mlo_link_switch_req *lswitch_req,
			    enum wlan_mlo_link_switch_notify_reason notify_reason);
#endif

/**
 * dp_peer_obj_create_notification(): dp peer create handler
 * @peer: peer which is going to created by objmgr
 * @arg: argument for vdev create handler
 *
 * Register this api with objmgr to detect peer is created
 *
 * Return: QDF_STATUS status in case of success else return error
 */
QDF_STATUS
dp_peer_obj_create_notification(struct wlan_objmgr_peer *peer, void *arg);

/**
 * dp_peer_obj_destroy_notification(): dp peer delete handler
 * @peer: peer which is going to delete by objmgr
 * @arg: argument for vdev delete handler
 *
 * Register this api with objmgr to detect peer is deleted
 *
 * Return: QDF_STATUS status in case of success else return error
 */
QDF_STATUS
dp_peer_obj_destroy_notification(struct wlan_objmgr_peer *peer, void *arg);

/**
 * dp_vdev_obj_destroy_notification() - Free per DP vdev object
 * @vdev: vdev context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when vdev is being
 * deleted and delete DP vdev context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
dp_vdev_obj_destroy_notification(struct wlan_objmgr_vdev *vdev, void *arg);

/**
 * dp_vdev_obj_create_notification() - Allocate per DP vdev object
 * @vdev: vdev context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when vdev is being
 * created and creates DP vdev context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
dp_vdev_obj_create_notification(struct wlan_objmgr_vdev *vdev, void *arg);

/**
 * dp_pdev_obj_create_notification() - Allocate per DP pdev object
 * @pdev: pdev context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when pdev is being
 * created and creates DP pdev context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
dp_pdev_obj_create_notification(struct wlan_objmgr_pdev *pdev, void *arg);

/**
 * dp_pdev_obj_destroy_notification() - Free per DP pdev object
 * @pdev: pdev context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when pdev is being
 * deleted and delete DP pdev context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
dp_pdev_obj_destroy_notification(struct wlan_objmgr_pdev *pdev, void *arg);

/**
 * dp_psoc_obj_create_notification() - Function to allocate per DP
 * psoc private object
 * @psoc: psoc context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when psoc is being
 * created and creates DP soc context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
dp_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * dp_psoc_obj_destroy_notification() - Free psoc private object
 * @psoc: psoc context
 * @arg: Pointer to arguments
 *
 * This function gets called from object manager when psoc is being
 * deleted and delete DP soc context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
dp_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * dp_attach_ctx() - Api to attach dp ctx
 * @dp_ctx : DP Context
 *
 * Helper function to attach dp ctx
 *
 * Return: None.
 */
void dp_attach_ctx(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_detach_ctx() - to detach dp context
 *
 * Helper function to detach dp context
 *
 * Return: None.
 */
void dp_detach_ctx(void);

/**
 * dp_get_context() - to get dp context
 *
 * Helper function to get dp context
 *
 * Return: dp context.
 */
struct wlan_dp_psoc_context *dp_get_context(void);

/**
 * dp_add_latency_critical_client() - Add latency critical client
 * @vdev: pointer to vdev object (Should not be NULL)
 * @phymode: the phymode of the connected adapter
 *
 * This function checks if the present connection is latency critical
 * and adds to the latency critical clients count and informs the
 * datapath about this connection being latency critical.
 *
 * Returns: None
 */
static inline void
dp_add_latency_critical_client(struct wlan_objmgr_vdev *vdev,
			       enum qca_wlan_802_11_mode phymode)
{
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;

	if (!dp_link) {
		dp_err("No dp_link for objmgr vdev %pK", vdev);
		return;
	}

	dp_intf = dp_link->dp_intf;
	if (!dp_intf) {
		dp_err("Invalid dp_intf for dp_link %pK (" QDF_MAC_ADDR_FMT ")",
		       dp_link, QDF_MAC_ADDR_REF(dp_link->mac_addr.bytes));
		return;
	}

	switch (phymode) {
	case QCA_WLAN_802_11_MODE_11A:
	case QCA_WLAN_802_11_MODE_11G:
		qdf_atomic_inc(&dp_intf->dp_ctx->num_latency_critical_clients);

		dp_debug("Adding latency critical connection for vdev %d",
			 dp_link->link_id);
		cdp_vdev_inform_ll_conn(cds_get_context(QDF_MODULE_ID_SOC),
					dp_link->link_id,
					CDP_VDEV_LL_CONN_ADD);
		break;
	default:
		break;
	}
}

/**
 * dp_del_latency_critical_client() - Add tlatency critical client
 * @vdev: pointer to vdev object (Should not be NULL)
 * @phymode: the phymode of the connected adapter
 *
 * This function checks if the present connection was latency critical
 * and removes from the latency critical clients count and informs the
 * datapath about the removed connection being latency critical.
 *
 * Returns: None
 */
static inline void
dp_del_latency_critical_client(struct wlan_objmgr_vdev *vdev,
			       enum qca_wlan_802_11_mode phymode)
{
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;

	if (!dp_link) {
		dp_err("No dp_link for objmgr vdev %pK", vdev);
		return;
	}

	dp_intf = dp_link->dp_intf;
	if (!dp_intf) {
		dp_err("Invalid dp_intf for dp_link %pK (" QDF_MAC_ADDR_FMT ")",
		       dp_link, QDF_MAC_ADDR_REF(dp_link->mac_addr.bytes));
		return;
	}

	switch (phymode) {
	case QCA_WLAN_802_11_MODE_11A:
	case QCA_WLAN_802_11_MODE_11G:
		qdf_atomic_dec(&dp_intf->dp_ctx->num_latency_critical_clients);

		dp_info("Removing latency critical connection for vdev %d",
			dp_link->link_id);
		cdp_vdev_inform_ll_conn(cds_get_context(QDF_MODULE_ID_SOC),
					dp_link->link_id,
					CDP_VDEV_LL_CONN_DEL);
		break;
	default:
		break;
	}
}

/**
 * is_dp_intf_valid() - to check DP interface valid
 * @dp_intf: DP interface pointer
 *
 * API to check whether DP interface is valid
 *
 * Return: non zero value on interface valid
 */
int is_dp_intf_valid(struct wlan_dp_intf *dp_intf);

/**
 * is_dp_link_valid() - check if DP link is valid
 * @dp_link: DP link handle
 *
 * API to check whether DP link is valid
 *
 * Return: true if dp_link is valid, else false.
 */
bool is_dp_link_valid(struct wlan_dp_link *dp_link);

/**
 * dp_send_rps_ind() - send rps indication to daemon
 * @dp_intf: DP interface
 *
 * If RPS feature enabled by INI, send RPS enable indication to daemon
 * Indication contents is the name of interface to find correct sysfs node
 * Should send all available interfaces
 *
 * Return: none
 */
void dp_send_rps_ind(struct wlan_dp_intf *dp_intf);

/**
 * dp_try_send_rps_ind() - try to send rps indication to daemon.
 * @vdev: vdev handle
 *
 * If RPS flag is set in DP context then send rsp indication.
 *
 * Return: none
 */
void dp_try_send_rps_ind(struct wlan_objmgr_vdev *vdev);

/**
 * dp_send_rps_disable_ind() - send rps disable indication to daemon
 * @dp_intf: DP interface
 *
 * Return: none
 */
void dp_send_rps_disable_ind(struct wlan_dp_intf *dp_intf);

#ifdef QCA_CONFIG_RPS
/**
 * dp_set_rps() - Enable/disable RPS for mode specified
 * @vdev_id: vdev id which RPS needs to be enabled
 * @enable: Set true to enable RPS in SAP mode
 *
 * Callback function registered with ipa
 *
 * Return: none
 */
void dp_set_rps(uint8_t vdev_id, bool enable);
#else
static inline void dp_set_rps(uint8_t vdev_id, bool enable)
{
}
#endif

/**
 * dp_set_rx_mode_rps() - Enable/disable RPS in SAP mode
 * @enable: Set true to enable RPS in SAP mode
 *
 * Callback function registered with core datapath
 *
 * Return: none
 */
void dp_set_rx_mode_rps(bool enable);

/**
 * dp_set_rps_cpu_mask - set RPS CPU mask for interfaces
 * @dp_ctx: pointer to struct dp_context
 *
 * Return: none
 */
void dp_set_rps_cpu_mask(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_try_set_rps_cpu_mask() - try to set RPS CPU mask
 * @psoc: psoc handle
 *
 * If RPS flag is set in DP context then set RPS CPU mask.
 *
 * Return: none
 */
void dp_try_set_rps_cpu_mask(struct wlan_objmgr_psoc *psoc);

/**
 * dp_clear_rps_cpu_mask - clear RPS CPU mask for interfaces
 * @dp_ctx: pointer to struct dp_context
 *
 * Return: none
 */
void dp_clear_rps_cpu_mask(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_mic_init_work() - init mic error work
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
void dp_mic_init_work(struct wlan_dp_intf *dp_intf);

/**
 * dp_mic_deinit_work() - deinitialize mic error work
 * @dp_intf: Pointer to dp interface
 *
 * Return: None
 */
void dp_mic_deinit_work(struct wlan_dp_intf *dp_intf);

/**
 * dp_rx_mic_error_ind() - MIC error indication handler
 * @psoc: opaque handle for UMAC psoc object
 * @pdev_id: physical device instance id
 * @mic_failure_info: mic failure information
 *
 * This function indicates the Mic failure to the supplicant
 *
 * Return: None
 */
void
dp_rx_mic_error_ind(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
		    struct cdp_rx_mic_err_info *mic_failure_info);
/**
 * dp_intf_get_tx_ops: get TX ops from the DP interface
 * @psoc: pointer to psoc object
 *
 * Return: pointer to TX op callback
 */
static inline
struct wlan_dp_psoc_sb_ops *dp_intf_get_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	if (!psoc) {
		dp_err("psoc is null");
		return NULL;
	}

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("psoc private object is null");
		return NULL;
	}

	return &dp_ctx->sb_ops;
}

/**
 * dp_intf_get_rx_ops: get RX ops from the DP interface
 * @psoc: pointer to psoc object
 *
 * Return: pointer to RX op callback
 */
static inline
struct wlan_dp_psoc_nb_ops *dp_intf_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	if (!psoc) {
		dp_err("psoc is null");
		return NULL;
	}

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("psoc private object is null");
		return NULL;
	}

	return &dp_ctx->nb_ops;
}

/**
 * dp_get_arp_request_ctx: get ARP req context from the DP context
 * @psoc: pointer to psoc object
 *
 * Return: pointer to ARP request ctx.
 */
static inline
void *dp_get_arp_request_ctx(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("psoc private object is null");
		return NULL;
	}
	return dp_ctx->sb_ops.arp_request_ctx;
}

/**
 * dp_get_arp_stats_event_handler() - callback api to update the
 * stats received from FW
 * @psoc : psoc handle
 * @rsp: pointer to data received from FW.
 *
 * This is called when wlan driver received response event for
 * get arp stats to firmware.
 *
 * Return: None
 */
QDF_STATUS dp_get_arp_stats_event_handler(struct wlan_objmgr_psoc *psoc,
					  struct dp_rsp_stats *rsp);

/**
 * dp_trace_init() - Initialize DP trace
 * @psoc: psoc handle
 *
 * Return: None
 */

void dp_trace_init(struct wlan_objmgr_psoc *psoc);

/**
 * dp_set_dump_dp_trace() - set DP trace dump level
 * @cmd_type : command type
 * @count: count
 *
 * Return: None
 */
void dp_set_dump_dp_trace(uint16_t cmd_type, uint16_t count);

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#define DP_BUS_BW_CFG(bus_bw_cfg)	bus_bw_cfg
#define DP_BUS_BW_GET_RX_LVL(dp_ctx)	(dp_ctx)->cur_rx_level
static inline bool
dp_is_low_tput_gro_enable(struct wlan_dp_psoc_context *dp_ctx)
{
	return (qdf_atomic_read(&dp_ctx->low_tput_gro_enable)) ? true : false;
}
#else
#define DP_BUS_BW_CFG(bus_bw_cfg)	0
#define DP_BUS_BW_GET_RX_LVL(dp_ctx)	0
static inline bool
dp_is_low_tput_gro_enable(struct wlan_dp_psoc_context *dp_ctx)
{
	return false;
}
#endif

#define DP_DATA_STALL_ENABLE      BIT(0)
#define DP_HOST_STA_TX_TIMEOUT    BIT(16)
#define DP_HOST_SAP_TX_TIMEOUT    BIT(17)
#define DP_HOST_NUD_FAILURE       BIT(18)
#define DP_TIMEOUT_WLM_MODE       BIT(31)
#define FW_DATA_STALL_EVT_MASK     0x8000FFFF

/**
 * dp_is_data_stall_event_enabled() - Check if data stall detection is enabled
 * @evt: Data stall event to be checked
 *
 * Return: True if the data stall event is enabled
 */
bool dp_is_data_stall_event_enabled(uint32_t evt);

/*
 * dp_get_net_dev_stats(): Get netdev stats
 * @dp_intf: DP interface handle
 * @stats: To hold netdev stats
 *
 * Return: None
 */
static inline void
dp_get_net_dev_stats(struct wlan_dp_intf *dp_intf, qdf_net_dev_stats *stats)
{
	qdf_mem_copy(stats, &dp_intf->stats, sizeof(dp_intf->stats));
}

/*
 * dp_clear_net_dev_stats(): Clear netdev stats
 * @dp_intf: DP interface handle
 *
 * Return: None
 */
static inline
void dp_clear_net_dev_stats(struct wlan_dp_intf *dp_intf)
{
	qdf_mem_set(&dp_intf->stats, sizeof(dp_intf->stats), 0);
}

#ifdef FEATURE_DIRECT_LINK
/**
 * dp_direct_link_init() - Initializes Direct Link datapath
 * @dp_ctx: DP private context
 *
 * Return: QDF status
 */
QDF_STATUS dp_direct_link_init(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_direct_link_deinit() - De-initializes Direct Link datapath
 * @dp_ctx: DP private context
 * @is_ssr: true if SSR is in progress else false
 *
 * Return: None
 */
void dp_direct_link_deinit(struct wlan_dp_psoc_context *dp_ctx, bool is_ssr);

/**
 * dp_config_direct_link: Set direct link config of vdev
 * @dp_intf: DP interface handle
 * @config_direct_link: Flag to enable direct link path
 * @enable_low_latency: Flag to enable low link latency
 *
 * Return: QDF Status
 */
QDF_STATUS dp_config_direct_link(struct wlan_dp_intf *dp_intf,
				 bool config_direct_link,
				 bool enable_low_latency);
#else
static inline
QDF_STATUS dp_direct_link_init(struct wlan_dp_psoc_context *dp_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_direct_link_deinit(struct wlan_dp_psoc_context *dp_ctx, bool is_ssr)
{
}

static inline
QDF_STATUS dp_config_direct_link(struct wlan_dp_intf *dp_intf,
				 bool config_direct_link,
				 bool enable_low_latency)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#ifdef WLAN_FEATURE_11BE
/**
 * __wlan_dp_update_peer_map_unmap_version() - update peer map unmap version
 * @version: Peer map unmap version pointer to be updated
 *
 * Return: None
 */
static inline void
__wlan_dp_update_peer_map_unmap_version(uint8_t *version)
{
	/* 0x32 -> host supports HTT peer map v3 format and peer unmap v2 format. */
	*version = 0x32;
}
#else
static inline void
__wlan_dp_update_peer_map_unmap_version(uint8_t *version)
{
}
#endif

#ifdef WLAN_DP_PROFILE_SUPPORT
/**
 * wlan_dp_get_profile_info() - Get DP memory profile info
 *
 * Return: None
 */
struct wlan_dp_memory_profile_info *wlan_dp_get_profile_info(void);

/**
 * wlan_dp_select_profile_cfg() - Select DP profile configuration
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_dp_select_profile_cfg(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_dp_soc_cfg_sync_profile() - Sync DP soc cfg items with profile
 * @cdp_soc: cdp soc context
 *
 * Return: None
 */
void wlan_dp_soc_cfg_sync_profile(struct cdp_soc_t *cdp_soc);

/**
 * wlan_dp_pdev_cfg_sync_profile() - Sync DP pdev cfg items with profile
 * @cdp_soc: cdp soc context
 * @pdev_id: pdev id
 *
 * Return: QDF_STATUS
 */
void wlan_dp_pdev_cfg_sync_profile(struct cdp_soc_t *cdp_soc, uint8_t pdev_id);
#else

static inline
QDF_STATUS wlan_dp_select_profile_cfg(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * wlan_dp_link_cdp_vdev_delete_notification() - CDP vdev delete notification
 * @context: osif_vdev handle
 *
 * Return: None
 */
void wlan_dp_link_cdp_vdev_delete_notification(void *context);

/* DP CFG APIs - START */

#ifdef WLAN_SUPPORT_RX_FISA
/**
 * wlan_dp_cfg_is_rx_fisa_enabled() - Get Rx FISA enabled flag
 * @dp_cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
static inline
bool wlan_dp_cfg_is_rx_fisa_enabled(struct wlan_dp_psoc_cfg *dp_cfg)
{
	return dp_cfg->is_rx_fisa_enabled;
}

/**
 * wlan_dp_cfg_is_rx_fisa_lru_del_enabled() - Get Rx FISA LRU del enabled flag
 * @dp_cfg: soc configuration context
 *
 * Return: true if enabled, false otherwise.
 */
static inline
bool wlan_dp_cfg_is_rx_fisa_lru_del_enabled(struct wlan_dp_psoc_cfg *dp_cfg)
{
	return dp_cfg->is_rx_fisa_lru_del_enabled;
}
#else
static inline
bool wlan_dp_cfg_is_rx_fisa_enabled(struct wlan_dp_psoc_cfg *dp_cfg)
{
	return false;
}

static inline
bool wlan_dp_cfg_is_rx_fisa_lru_del_enabled(struct wlan_dp_psoc_cfg *dp_cfg)
{
	return false;
}
#endif


/* DP CFG APIs - END */

#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION

/**
 * dp_flow_priortization_init() - Initialize FPM and FIM modules
 * @dp_intf: DP interface handle
 *
 * Return: void
 */
static inline void dp_flow_priortization_init(struct wlan_dp_intf *dp_intf)
{
	dp_fpm_init(dp_intf);
	dp_fim_init(dp_intf);
}

/**
 * dp_flow_priortization_deinit() - Deinitialize FPM and FIM modules
 * @dp_intf: DP interface handle
 *
 * Return: void
 */
static inline void dp_flow_priortization_deinit(struct wlan_dp_intf *dp_intf)
{
	dp_fim_deinit(dp_intf);
	dp_fpm_deinit(dp_intf);
}
#else
static inline void dp_flow_priortization_init(struct wlan_dp_intf *dp_intf)
{
}

static inline void dp_flow_priortization_deinit(struct wlan_dp_intf *dp_intf)
{
}
#endif

/**
 * __wlan_dp_update_def_link() - update DP interface default link
 * @psoc: psoc handle
 * @intf_mac: interface MAC address
 * @vdev: objmgr vdev handle to set the def_link in dp_intf
 *
 */
void __wlan_dp_update_def_link(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *intf_mac,
			       struct wlan_objmgr_vdev *vdev);
#endif
